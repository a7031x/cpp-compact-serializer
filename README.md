<h2>Introduction</h2>

Have you ever wanted to save a struct or a class as file or stream? Assume that you are going to transfer an object from the client to the server, you need to pack up the object as network stream, and unpack the stream on the other side. In .Net or Java, this can be done by enumerating the fields or even using serializable attributes. In C++, there isno way to enumerate the fields automatically. The simplest way is hand codingthe procedure of packing and unpacking, the code may look like this,


    struct SomeClass
    {
        int field1;
        string field2;
        map<string, string> field3;
    };

    SomeClass obj;
    stringstream ss;
    ss << obj.field1 << obj.field2;
    for (auto& kv : obj.field3)
        ss << kv.first << kv.second;

 

This snippet doesn’t look awful just because the fields are simple. Imagine that there are embedded containers, or customized fields, the serialization procedure needs to recursively encode all the elements of inside containers and fields of sub-classes. And the way how they are encoded should be exactly symmetric to how they are decoded. Any inadvertence could cause data corruption.

What we expect is something like this,

    struct SomeClass
    {
        int field1;
        string field2;
        map<string, string> field3;
    };

    SomeClass obj;
    compact_archive<stringstream> ar;
    ar << obj;  //serialize
    ar >> obj;  //deserialize

Although we cannot make it this way in C++.The compiler doesn’t know the fields of the class. What we need is just alittle tweak,

    struct SomeClass
    {
        int field1;
        string field2;
        map<string, string> field3;
        DECLARE_SERIALIZE(field1, field2, field3)
    };

The macro DECLARE_SERIALIZE tells the compiler what are the fields. With this extra information, it’s enough for the compiler to realize the scenario.

<h2>Type deduction</h2>

First things first, before demystifying what the compact_archive and the macro are, let’s quickly recap how the C++ deduction system works.

Although C++ cannot enumerate the fields of a type, it can obtain enough information from the type. In the remaining of this article, a type can be a class, a struct, or a primitive, or even the instance of a type, you can easily tell what it really is according to the context. Let’s look at an example,

    vector<map<int, string>> v;

The C++ compiler deduces what is the type of v in compile time, strip off the type layer by layer, and finally knows it’sa vector of map with an int type key and a string type value. The compiler serialize all of the embedded type recursively, decomposing the fields down to a primitive or a basic STL object which cannot be decomposed any more.
How the compiler determines the type andthe action?
There are several ways,

<li>•Operators, such as std::is_pod, std::is_convertible, std::is_same.</li>
<li>•Template specification.</li>
<li>•Compiler specific syntax, such as __if_exists.</li>

Personally I don’t like the third way, and in most of cases they can be substituted with the first and second ways. But sometimes it’s simpler.

Let’s take a close look at the serializer,

    template<typename StreamType, typename Type>
    struct serialize_all
    {
        typedef void result_type;
        void operator()(StreamType& s, Type& a)
        {
            conditional<is_pod<Type>::value,
                serialize_pod<StreamType, Type>,
                conditional<is_container<remove_const<Type>::type>::value,
                  serialize_container<remove_const<Type>::type>,
                  conditional<is_pair<remove_const<Type>::type>::value,
                    serialize_pair<StreamType, remove_const<Type>::type>,
                    conditional<is_tuple<remove_const<Type>::type>::value,
                           serialize_tuple<StreamType, Type>,
                           serialize_complex<StreamType, Type>
                       >::type
                   >::type
                >::type
            >::type()(s, a);
        }
    };


A struct with an operator() method is always called a functor. It works like a function and sometimes holds states inits fields. The serialize_all functor dispatches types into specific handlers. The conditional template estimate thefirst template parameter, if it is true, then choose the first functor, otherwise, choose the second functor, where the second functor is another conditional. The layered conditionals perform like a “switch case” at compile time. Asub-functor may need to strip off the outer container and call the serialize_all to serialize the inner type.

<h2>User defined type</h2>

Compiler cannot determine a user defined type unless it inherit a common base class. But it’s awkward to do so, we won’tlike to have all the user defined types inherited the base class. So in this solution I left the user-defined case to the default functor. When theserialize_all functor finally determines it’s a user defined type, it calls theserialize method of the type,
 

    struct SomeClass
    {
        int field1;
        string field2;
        map<string, string> field3;

        template<typename ArType> void serialize(ArType& ar)
        {
            deserialize_parameters(ar, field1, field2, field3);
        }
 
        template<typename ArType> void serialize(ArType& ar)const {
            serialize_parameters(ar, field1, field2, field3);
        }
    };

 

serialize_parameters and deserialize_parametersare variadic template functions which serialize or deserialize the incoming parameters.Now replace the serialize with the macro,

    DECLARE_SERIALIZE(…)

For the type that are defined by the third party,we may not be able to modify the type. In this case, we can inherit the typeand create an outer type which declares the macro. However, only work for public or protected members.

<h2>Optimization</h2>

Imagine that we have a vector<char>to represent a binary block, the element number could be very large. It’s not acceptable to serialize the elements one by one, instead, we serialize the whole buffer at once. To expand the idea for vector of any POD type, in the serialize_container functor, it takes a further step to determine that if the element type is POD, then decides whether to serialize the element one by one or just copy the element array to the stream. If you don’t know what is POD, you can take it as a type that can be copied with memory copy. However, not all types that can be memory copied are POD, let’s spare it for now.

To apply the idea a little further, for all random accessible container with POD type, we could do the same thing. So firstly we need to determine if the container has random access iterator (sofar they are vector and string). This optimization could significantly improve the CPU performance when transfer binary data across network.

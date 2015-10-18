///////////////////////////////////////////////////////////////////////////////////////
//compact_archive
//supported: scalar types, containers (string and wstring are special types of container), std::pair, std::tuple and custom types.
//			std::vector and array of POD (plain of data) is optimized.
//unsupported: type check or integrity check. DO NOT try to deserialize to a different type. please keep (de)serialization in order. 
//warning: DO NOT pass pointer type parameter to the compact_archive operators.
//author: Hou S.D.
//date: 12-01-2012
///////////////////////////////////////////////////////////////////////////////////////
//example:
/*
struct CustomType
{
	string text;
	int data;

	CustomType(string text)
	{
		this->text = text;
		this->data = rand();
	}

	// the method below need to be explicitly defined.
	// Otherwise the compiler doesn't know how to serialize the custom type.
	template<typename Archive>
	void serialize(Archive& ar)
	{
	ar & text & data;	//list the members you want to serialize and deserialize.
	//any member of custom type needs to define the method in its type too.
	//IMPORTANT: if the struct type is POD type (e.g. RECT, POINT, SIZE), the serialize method is not required.
	//for what's POD type, please refer to MSDN.
	}
};

main()
{
	using namespace boost::assign;	//use the assign library to show the test example.

	int i1[5] = {1, 2, 3, 4, 5};
	int i2[5] = {0};			//make sure that i2 is the same length as i1

	std::vector<string> vs1 = list_of("abc")("def")("ghi");
	std::vector<string> vs2;			//output value of container type do not need to by initialized

	std::map<string, CustomType> mct1 = std::map_list_of("a", CustomType("c-a")) ("b", CustomType("c-b")), mct2;

	auto ar = make_compact_archive();	//if a stream is already exists, call the make_compact_archive(stream) instead.

	ar << i1 << vs1 << mct1;
	ar >> i2 >> vs2 >> mct2;
	//l2, vs2, mct2 should be equals to i1, vs1, mct1 relatively.
}
*/

#pragma once
#include <memory>
#include <ostream>
#include <sstream>
#include <vector>
#include <array>
#include <list>
#include <tuple>
#include <map>
#include <set>
#include <algorithm>
#include <functional>
#include <stdint.h>
#include <custom/stream_operation.hpp>


#define	DECLARE_SERIALIZE(...)	template<typename ArType> void serialize(ArType& ar) { deserialize_parameters(ar, __VA_ARGS__);}\
								template<typename ArType> void serialize(ArType& ar)const { serialize_parameters(ar, __VA_ARGS__);}

template<typename Type>
struct compact_serializer
{
private:
	compact_serializer();
};

template<typename Stream>
class compact_archive
{
public:
	compact_archive() : m_default_stream(make_shared<std::stringstream>()), m_stream(*m_default_stream) {}
	compact_archive(Stream& s) : m_stream(s) {}
	template<typename Type>
	//auto serialization or deserialization.
	//if Stream is derived type of istd::ostream (which could be std::istream or std::ostream), a compiling error will be exerted.
	compact_archive& operator & (Type& a)
	{
		serialize_all<Stream, Type>()(m_stream, a);
		return *this;
	}
	//explicitly serialization
	template<typename Type>
	compact_archive& operator << (const Type& a)
	{
		serialize_all<std::ostream, const Type>()(m_stream, a);
		return *this;
	}
	//explicitly deserialization
	template<typename Type>
	compact_archive& operator >> (Type& a)
	{
		serialize_all<std::istream, Type>()(m_stream, a);
		return *this;
	}
	//it is not recommended to use the methods below, which is designed for compatible with unreasonable defined types.
	template<typename Type>
	void serialize(Type* objects, int number)
	{
		serialize((std::ostream&)m_stream, objects, number);
	}
	template<typename Type>
	void deserialize(Type* objects, int number)
	{
		serialize((std::istream&)m_stream, objects, number);
	}
private:
	std::shared_ptr<std::stringstream> m_default_stream;
	Stream& m_stream;

private:
	static void serialize_buffer(std::ostream& s, const void* buffer, std::streamsize size) { s.write(reinterpret_cast<const char*>(buffer), size); }
	static void serialize_buffer(std::istream& s, void* buffer, std::streamsize size) { s.read(reinterpret_cast<char*>(buffer), size); }
	template<typename Type> static Type& reference(Type& a) { return a; }
	template<typename Type> static Type& reference(Type* a) { return *a; }

	template<typename StreamType, typename Type>
	static void serialize(StreamType& s, Type* p, int n)//serialize array
	{
		if (is_pod<Type>::value)
			serialize_buffer(s, p, sizeof(Type) * n);
		else
			std::for_each(&p[0], &p[n], std::bind(serialize_all<StreamType, Type>(), std::ref(s), std::placeholders::_1));

	}
	template<typename StreamType, typename IteratorType>
	static void serialize(StreamType& s, IteratorType itr, size_t n)//serialize iterators
	{
		for (size_t k = 0; k < n; ++k)
		{
			serialize(s, &*itr, 1);
			++itr;
		}
	}

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

	template<typename StreamType, typename Type>
	struct serialize_pod
	{
		void operator()(StreamType& s, Type& a)
		{
			serialize_buffer(s, &a, sizeof(a));
		}
	};

	struct _serialize_buffer_container
	{
		template<typename Type>
		void operator()(std::istream& s, Type& a, size_t size)
		{
			a.assign(size, Type::value_type());
			serialize(s, a.begin(), size);
		}
	};
	struct _serialize_pushback_container
	{
		template<typename Type>
		void operator()(std::istream& s, Type& a, size_t size)
		{
			typedef remove_const<Type::iterator::value_type>::type elem_type;
			while (size--)
			{
				elem_type elem;
				serialize_all<std::istream, elem_type>()(s, elem);
				a.push_back(elem);
			}
		}
	};
	struct _serialize_insert_container
	{
		template<typename Type>
		void operator()(std::istream& s, Type& a, size_t size)
		{
			while(size--)
			{
				typedef remove_const<Type::iterator::value_type>::type elem_type;
				elem_type elem;
				serialize_all<std::istream, elem_type>()(s, elem);
				a.insert(elem);
			}
		}
	};
	template<typename Type>
	struct serialize_container
	{
		void operator()(std::ostream& s, const Type& a)
		{
			int32_t size = (int32_t)a.size();
			serialize(s, &size, 1);
			serialize(s, a.begin(), size);
		}
		void operator()(std::istream& s, Type& a)
		{
			int32_t size;
			serialize(s, &size, 1);
			std::conditional<is_buffer_container<Type>::value, _serialize_buffer_container,
				std::conditional<has_pushback_operator<Type>::value, _serialize_pushback_container,
				_serialize_insert_container>::type>::type()(s, a, size);

		}
	};
	template<typename Type>
	struct serialize_container<std::vector<Type>>	//optimization for std::std::vector
	{
		void operator()(std::ostream& s, const std::vector<Type>& a)
		{
			int32_t size = (int32_t)a.size();
			serialize(s, &size, 1);
			if (size)
				serialize(s, &a[0], size);
		}
		void operator()(std::istream& s, std::vector<Type>& a)
		{
			int32_t size;
			serialize(s, &size, 1);
			a.assign(size, Type());
			if (0 < size)
				serialize(s, &a[0], size);
		}
	};

	template<typename StreamType, typename Type>
	struct serialize_complex
	{
		void operator()(StreamType& s, Type& a)
		{
			a.serialize(compact_archive<StreamType>(s));
		}
	};
	template<typename StreamType, typename Type>
	struct serialize_pair
	{
		void operator()(std::ostream& s, const Type& a)
		{
			serialize_all<std::ostream, const Type::first_type>()(s, a.first);
			serialize_all<std::ostream, const Type::second_type>()(s, a.second);
		}
		void operator()(std::istream& s, Type& a)
		{
			typedef remove_const<Type::first_type>::type first_type;
			serialize_all<std::istream, first_type>()(s, const_cast<first_type&>(a.first));
			serialize_all<std::istream, Type::second_type>()(s, a.second);
		}
	};
	template<typename StreamType, typename Type>
	struct serialize_tuple
	{
		void operator()(StreamType& s, Type& a)
		{
			serialize_element<0>(s, a);
		}
		template<int Index>
		void serialize_element(StreamType& s, Type& a)
		{
			serialize_all<StreamType, std::remove_reference<decltype(get<Index>(a))>::type>()(s, get<Index>(a));
			serialize_element<Index + 1>(s, a);
		}
		template<>
		void serialize_element<std::tuple_size<Type>::value>(StreamType& s, Type& a) {}
	};
	template<typename Type> struct is_pair { enum { value = false }; };
	template<typename T1, typename T2> struct is_pair<std::pair<T1, T2>> { enum { value = true }; };

	template<typename Type> struct is_vector { enum { value = false }; };
	template<typename Type, typename... Params> struct is_vector<std::vector<Type, Params...>> { enum { value = true }; };
	template<typename Type> struct is_array { enum { value = false }; };
	template<typename Type, int Size> struct is_array<std::array<Type, Size>> { enum { value = true }; };
	template<typename Type> struct is_string { enum { value = false }; };
	template<typename Type, typename... Params> struct is_string<std::basic_string<Type, Params...>> { enum { value = true }; };
	template<typename Type> struct is_set { enum { value = false }; };
	template<typename Type, typename... Params> struct is_set<std::set<Type, Params...>> { enum { value = true }; };
	template<typename Type> struct is_list { enum { value = false }; };
	template<typename Type, typename... Params> struct is_list<std::list<Type, Params...>> { enum { value = true }; };
	template<typename Type> struct is_map { enum { value = false }; };
	template<typename K, typename V, typename... Params> struct is_map<std::map<K, V, Params...>> { enum { value = true }; };
	template<typename Type> struct is_tuple { enum { value = false }; };
	template<typename... Params> struct is_tuple<std::tuple<Params...>> { enum { value = true }; };

	template<typename Type> struct is_container
	{
		enum
		{
			value =
			is_vector<Type>::value ||
			is_list<Type>::value ||
			is_array<Type>::value ||
			is_string<Type>::value ||
			is_set<Type>::value ||
			is_map<Type>::value
		};
	};

	template<typename Type> struct is_buffer_container
	{
		enum
		{
			value = is_vector<Type>::value || is_string<Type>::value
		};
	};

	template<typename Type> struct has_pushback_operator
	{
		enum
		{
			value = is_vector<Type>::value || is_list<Type>::value || is_string<Type>::value
		};
	};
};

template<typename Stream>
inline compact_archive<Stream> make_compact_archive(Stream& s)
{
	return compact_archive<Stream>(s);
}
inline compact_archive<std::stringstream> make_compact_archive()
{
	return compact_archive<std::stringstream>();
}

template<typename Type>
inline std::vector<char> serialize_chunk(const Type& type)
{
	std::stringstream ss;
	auto ar = make_compact_archive(ss);
	ar << type;
	std::vector<char> vec;
	custom::stream_to_std::vector(ss, vec);
	return vec;
}

template<typename Type>
inline Type deserialize_chunk(const std::vector<char>& chunk)
{
	std::stringstream ss;
	custom::std::vector_to_stream(chunk, ss);
	auto ar = make_compact_archive(ss);
	Type type;
	ar >> type;
	return type;
}

template<typename ArType, typename Head, typename... Params>
inline void serialize_parameters(ArType& ar, const Head& head, const Params&... params)
{
	ar << head;
	serialize_parameters(ar, params...);
}

template<typename ArType>
inline void serialize_parameters(ArType& ar)
{
}

template<typename ArType, typename Head, typename... Params>
inline void deserialize_parameters(ArType& ar, Head& head, Params&... params)
{
	ar >> head;
	deserialize_parameters(ar, params...);
}

template<typename ArType>
inline void deserialize_parameters(ArType& ar)
{
}


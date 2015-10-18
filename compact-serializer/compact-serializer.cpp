// compact-serializer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "compact_archive.hpp"
#include <map>
#include <assert.h>
#include <array>

using namespace std;

class CustomType
{
	string text;
	int data;

public:
	CustomType() {}
	CustomType(const string& text, int data) { this->text = text; this->data = data; }
	bool operator == (const CustomType& other)const { return this->text == other.text && this->data == other.data; }

	DECLARE_SERIALIZE(text, data)
};

int main()
{

	array<int, 5> i1 = { 1, 2, 3, 4, 5 };
	array<int, 5> i2 = { 0 };			//make sure that i2 is the same length as i1

	vector<string> vs1 = { "text1", "text2", "text3" };
	vector<string> vs2;			//output value of container type do not need to by initialized

	map<string, CustomType> mct1 = { {"a", CustomType("c-a", 1)}, {"b", CustomType("c-b", 2)} };
	map<string, CustomType> mct2;

	auto ar = make_compact_archive();	//if a stream is already exists, call the make_compact_archive(stream) instead.

	ar << i1 << vs1 << mct1;
	ar >> i2 >> vs2 >> mct2;

	assert(i1 == i2);
	assert(vs1 == vs2);
	assert(mct1 == mct2);
    return 0;
}


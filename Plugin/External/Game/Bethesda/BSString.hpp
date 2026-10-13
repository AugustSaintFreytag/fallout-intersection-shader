#pragma once

#include "BSMemObject.hpp"

template <typename T>
class BSStringT : public BSMemObject {
public:
	BSStringT() {};
	BSStringT(const T* apText) { Set(apText); }
	BSStringT(const BSStringT& aSrc) { Set(aSrc.c_str()); }
	~BSStringT();

	T* pString = nullptr;
	uint16_t	sLen = 0;
	uint16_t	sMaxLen = 0;
};

typedef BSStringT<char>		BSString;
typedef BSStringT<wchar_t>	BSWideString;
#pragma once
#include <cstdint>
#include <type_traits>

#if defined(_MSC_VER)
#	include <intrin.h>
#else
inline bool _bittest(const long* base, long bit) { return (*base & (1L << bit)) != 0; }
inline bool _bittestandset(long* base, long bit) { bool b = _bittest(base, bit); *base |= (1L << bit); return b; }
inline bool _bittestandreset(long* base, long bit) { bool b = _bittest(base, bit); *base &= ~(1L << bit); return b; }
#endif

template <typename T>
struct ScalarWrapper { 
private:
	T tField; 
};

template <typename TYPE, typename STRUCT>
class BitfieldBase :
	public std::conditional_t<std::is_scalar_v<STRUCT>, ScalarWrapper<STRUCT>, STRUCT>
{
public:
	TYPE& GetField() {
		return *reinterpret_cast<TYPE*>(this);
	}

	const TYPE& GetField() const {
		return *reinterpret_cast<const TYPE*>(this);
	}

	BitfieldBase() { GetField() = 0; }

	void Clear() { GetField() = 0; }
	void RawSet(TYPE data) { GetField() = data; }
	void Set(TYPE data) { GetField() |= data; }
	void Clear(TYPE data) { GetField() &= ~data; }
	void Unset(TYPE data) { Clear(data); }
	void Mask(TYPE data) { GetField() &= data; }
	void Toggle(TYPE data) { GetField() ^= data; }
	void SetBit(TYPE data, bool state) { state ? Set(data) : Clear(data); }

	void SetField(TYPE data, TYPE mask, TYPE pos) {
		GetField() = (GetField() & ~mask) | (data << pos);
	}

	TYPE GetField(TYPE mask, TYPE pos) const {
		return (GetField() & mask) >> pos;
	}

	bool Test(uint32_t bit) const {
		if constexpr (sizeof(TYPE) == 4)
			return _bittest(reinterpret_cast<const long*>(this), bit);
		else
			return (GetField() & (TYPE(1) << bit)) != 0;
	}

	bool TestSet(uint32_t bit) {
		if constexpr (sizeof(TYPE) == 4)
			return _bittestandset(reinterpret_cast<long*>(this), bit);
		else {
			TYPE mask = TYPE(1) << bit;
			bool bVal = (GetField() & mask) != 0;
			GetField() |= mask;
			return bVal;
		}
	}

	bool TestClear(uint32_t bit) {
		if constexpr (sizeof(TYPE) == 4)
			return _bittestandreset(reinterpret_cast<long*>(this), bit);
		else {
			TYPE mask = TYPE(1) << bit;
			bool bVal = (GetField() & mask) != 0;
			GetField() &= ~mask;
			return bVal;
		}
	}

	TYPE Get() const { return GetField(); }
	TYPE GetBit(TYPE data) const { return GetField() & data; }
	TYPE Extract(uint32_t bit) const { return (GetField() >> bit) & 1; }
	TYPE ExtractField(uint32_t shift, uint32_t length) const {
		return (GetField() >> shift) & (TYPE(~0) >> (sizeof(TYPE) * 8 - length));
	}

	bool IsSet(TYPE data) const { return GetBit(data) != 0; }
	bool IsUnSet(TYPE data) const { return GetBit(data) == 0; }
	bool IsClear(TYPE data) const { return IsUnSet(data); }

	operator TYPE&() {
		return GetField();
	}

	operator const TYPE&() const {
		return GetField();
	}

	void operator=(TYPE data) {
		GetField() = data;
	}

	void operator|=(TYPE data) {
		Set(data);
	}

	void operator&=(TYPE data) {
		Mask(data);
	}

	void operator^=(TYPE data) {
		Toggle(data);
	}
};

template <typename STRUCT>
using Bitfield = BitfieldBase<
	std::conditional_t<
	sizeof(STRUCT) == 1, uint8_t,
	std::conditional_t<
	sizeof(STRUCT) == 2, uint16_t,
	std::conditional_t<sizeof(STRUCT) == 4, uint32_t, void>
	>
	>,
	STRUCT
>;

using Bitfield8		= Bitfield<uint8_t>;
using Bitfield16	= Bitfield<uint16_t>;
using Bitfield32	= Bitfield<uint32_t>;

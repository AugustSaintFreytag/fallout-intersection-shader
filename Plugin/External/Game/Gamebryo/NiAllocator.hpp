#pragma once

enum NiMemEventType {
	NI_UNKNOWN					= 0,
	NI_OPER_NEW					= 1,
	NI_OPER_NEW_ARRAY			= 2,
	NI_OPER_DELETE				= 3,
	NI_OPER_DELETE_ARRAY		= 4,
	NI_MALLOC					= 5,
	NI_REALLOC					= 6,
	NI_ALIGNEDMALLOC			= 7,
	NI_ALIGNEDREALLOC			= 8,
	NI_FREE						= 9,
	NI_ALIGNEDFREE				= 10,
	NI_EXTERNAL_MALLOC			= 11,
	NI_EXTERNAL_REALLOC			= 12,
	NI_EXTERNAL_ALIGNEDMALLOC	= 13,
	NI_EXTERNAL_ALGINEDREALLOC	= 14,
	NI_EXTERNAL_FREE			= 15,
	NI_EXTERNAL_ALIGNEDFREE		= 16,
};


class NiAllocator {
public:
	virtual void	Destroy(bool);
	virtual void*	Allocate(size_t& stSizeInBytes, size_t& stAlignment, NiMemEventType eEventType, bool bProvideAccurateSizeOnDeallocate, const char* pcFile, int iLine, const char* pcFunction);
	virtual void	Deallocate(void* pvMemory, NiMemEventType eEventType, size_t stSizeInBytes);
	virtual void*	Reallocate(void* pvMemory, size_t& stSizeInBytes, size_t& stAlignment, NiMemEventType eEventType, bool bProvideAccurateSizeOnDeallocate, size_t stSizeCurrent, const char* pcFile, int iLine, const char* pcFunction);
	virtual void*	AllocateExternal(size_t& stSizeInBytes, size_t& stAlignment, NiMemEventType eEventType, bool bProvideAccurateSizeOnDeallocate, const char* pcFile, int iLine, const char* pcFunction);
	virtual void	DeallocateExternal(void* pvMemory, NiMemEventType eEventType, size_t stSizeInBytes);
	virtual void*	ReallocateExternal(void* pvMemory, size_t* stSizeInBytes, size_t* stAlignment, NiMemEventType eEventType, bool bProvideAccurateSizeOnDeallocate, size_t stSizeCurrent, const char* pcFile, int iLine, const char* pcFunction);
	virtual void	Initialize();
	virtual void	Shutdown();
	virtual bool	VerifyAddress(const void* pvMemory);
};

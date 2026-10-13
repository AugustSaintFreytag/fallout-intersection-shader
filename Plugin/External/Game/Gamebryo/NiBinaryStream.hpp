#pragma once

class NiBinaryStream;
typedef uint32_t(__cdecl* NIBINARYSTREAM_READFN)(NiBinaryStream* apThis, void* apvBuffer, uint32_t auiBytes, uint32_t* apuiComponentSizes, uint32_t auiNumComponents);
typedef uint32_t(__cdecl* NIBINARYSTREAM_WRITEFN)(NiBinaryStream* apThis, const void* apvBuffer, uint32_t auiBytes, uint32_t* apuiComponentSizes, uint32_t auiNumComponents);

class NiBinaryStream {
public:
	NiBinaryStream();
	virtual ~NiBinaryStream() {};

	virtual bool		FileIsGood() = 0;
	virtual void		Seek(int32_t aiNumBytes) = 0;
	virtual uint32_t	GetPosition() const;
	virtual void		SetEndianSwap(bool abDoSwap) = 0;

	uint32_t				m_uiAbsoluteCurrentPos;
	NIBINARYSTREAM_READFN	m_pfnRead;
	NIBINARYSTREAM_WRITEFN	m_pfnWrite;
};

ASSERT_SIZE(NiBinaryStream, 0x10);

#pragma once

#include "NiObject.hpp"
#include "Ni2DBuffer.hpp"
#include "NiDepthStencilBuffer.hpp"

class NiDX9Renderer;
class NiDX9TextureData;

NiSmartPointer(NiRenderTargetGroup);

class NiRenderTargetGroup : public NiObject {
public:
	NiRenderTargetGroup();
	virtual ~NiRenderTargetGroup();

	class RendererData : public NiMemObject {
	public:
		RendererData() {};
		virtual ~RendererData() {};
	};

	virtual uint32_t					GetWidth(uint32_t uiIndex) const;
	virtual uint32_t					GetHeight(uint32_t uiIndex) const;
	virtual uint32_t					GetDepthStencilWidth() const;
	virtual uint32_t					GetDepthStencilHeight() const;
	virtual const NiPixelFormat*		GetPixelFormat(uint32_t uiIndex) const;
	virtual const NiPixelFormat*		GetDepthStencilPixelFormat() const;
	virtual uint32_t					GetBufferCount() const;
	virtual bool						AttachBuffer(Ni2DBuffer* pkBuffer, uint32_t uiIndex);
	virtual bool						AttachDepthStencilBuffer(NiDepthStencilBuffer* pkDepthBuffer);
	virtual Ni2DBuffer*					GetBuffer(uint32_t uiIndex) const;
	virtual NiDepthStencilBuffer*		GetDepthStencilBuffer() const;
	virtual bool						GetByte08();
	virtual void						SetByte08();
	virtual const RendererData*			GetRendererData() const;
	virtual void						SetRendererData(RendererData* pkRendererData);
	virtual Ni2DBuffer::RendererData*	GetBufferRendererData(uint32_t uiIndex) const;
	virtual Ni2DBuffer::RendererData*	GetDepthStencilBufferRendererData() const;

	bool						Unk08;
	uint8_t						Unk09;
	uint16_t					usUsageCounter; // GECK only
	Ni2DBufferPtr				m_aspBuffers[4];
	uint32_t					m_uiNumBuffers;
	NiDepthStencilBufferPtr		m_spDepthStencilBuffer;
	RendererData*				m_pkRendererData;

	NIRTTI_ADDRESS(0x12708EC);

	static NiRenderTargetGroup* Create(uint32_t uiNumBuffers, NiDX9Renderer* pkRenderer);

	NiDX9TextureData* GetDX9RendererData() {
		return reinterpret_cast<NiDX9TextureData*>(m_pkRendererData);
	};
};

ASSERT_SIZE(NiRenderTargetGroup, 0x28);
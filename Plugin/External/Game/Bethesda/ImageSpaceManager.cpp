#include "ImageSpaceManager.hpp"

ImageSpaceManager* ImageSpaceManager::GetSingleton() {
#ifdef GAME
	return *reinterpret_cast<ImageSpaceManager**>(0x11F91AC);
#else
	return *reinterpret_cast<ImageSpaceManager**>(0xF23BFC);
#endif
}

// Source texture

// GAME - 0xB8C980
// GECK - 0x9140B0
void ImageSpaceManager::RenderEffect(ImageSpaceEffect* apEffect, NiDX9Renderer* apRenderer, NiTexture* apSourceTarget, BSRenderedTexture* apDestTarget, ImageSpaceEffectParam* apParam, bool abEndFrame) {
#if GAME
	ThisCall(0xB8C980, this, apEffect, apRenderer, apSourceTarget, apDestTarget, apParam, abEndFrame);
#else
	ThisCall(0x9140B0, this, apEffect, apRenderer, apSourceTarget, apDestTarget, apParam, abEndFrame);
#endif
}

// GAME - 0xB975F0
// GECK - 0x91ECE0
void ImageSpaceManager::RenderEffect(ImageSpaceManager::EffectID aeID, NiDX9Renderer* apRenderer, NiTexture* apSourceTarget, BSRenderedTexture* apDestTarget, ImageSpaceEffectParam* apParam, bool abEndFrame) {
#if GAME
	ThisCall(0xB975F0, this, aeID, apRenderer, apSourceTarget, apDestTarget, apParam, abEndFrame);
#else
	ThisCall(0x91ECE0, this, aeID, apRenderer, apSourceTarget, apDestTarget, apParam, abEndFrame);
#endif
}


// Rendered texture

// GAME - 0xB8C830
// GECK - 0x913F60
void ImageSpaceManager::RenderEffect(ImageSpaceEffect* apEffect, NiDX9Renderer* apRenderer, BSRenderedTexture* apSourceTarget, BSRenderedTexture* apDestTarget, ImageSpaceEffectParam* apParam, bool abEndFrame) {
#if GAME
	ThisCall(0xB8C830, this, apEffect, apRenderer, apSourceTarget, apDestTarget, apParam, abEndFrame);
#else
	ThisCall(0x913F60, this, apEffect, apRenderer, apSourceTarget, apDestTarget, apParam, abEndFrame);
#endif
}

// GAME - 0xB97550
// GECK - 0x91EC40
void ImageSpaceManager::RenderEffect(ImageSpaceManager::EffectID aeID, NiDX9Renderer* apRenderer, BSRenderedTexture* apSourceTarget, BSRenderedTexture* apDestTarget, ImageSpaceEffectParam* apParam, bool abEndFrame) {
#if GAME
	ThisCall(0xB97550, this, aeID, apRenderer, apSourceTarget, apDestTarget, apParam, abEndFrame);
#else
	ThisCall(0x91EC40, this, aeID, apRenderer, apSourceTarget, apDestTarget, apParam, abEndFrame);
#endif
}


// GAME - 0xB8C730
// GECK - 0x913E60
void ImageSpaceManager::RenderEffect(ImageSpaceManager::EffectID aeID, NiDX9Renderer* pkRenderer, BSRenderedTexture* apDestTarget, ImageSpaceEffectParam* apParam, bool abEndFrame) {
#if GAME
	ThisCall(0xB8C730, this, aeID, pkRenderer, apDestTarget, apParam, abEndFrame);
#else
	ThisCall(0x913E60, this, aeID, pkRenderer, apDestTarget, apParam, abEndFrame);
#endif
}

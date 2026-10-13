#include "NiDX9TextureData.hpp"

bool NiDX9TextureData::SaveTexture(const char* path, D3DXIMAGE_FILEFORMAT aeFormat) {
	return D3DXSaveTextureToFile(path, aeFormat, m_pkD3DTexture, nullptr);
}

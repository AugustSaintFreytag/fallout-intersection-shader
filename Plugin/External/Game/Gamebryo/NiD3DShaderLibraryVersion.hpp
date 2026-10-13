#pragma once

class NiD3DShaderLibraryVersion {
public:
	uint32_t m_uiSystemPixelShaderVersion;
	uint32_t m_uiMinPixelShaderVersion;
	uint32_t m_uiRequestedPixelShaderVersion;
	uint32_t m_uiSystemVertexShaderVersion;
	uint32_t m_uiMinVertexShaderVersion;
	uint32_t m_uiRequestedVertexShaderVersion;
	uint32_t m_uiSystemUserVersion;
	uint32_t m_uiMinUserVersion;
	uint32_t m_uiRequestedUserVersion;
	uint32_t m_uiPlatform;
};

ASSERT_SIZE(NiD3DShaderLibraryVersion, 0x28);
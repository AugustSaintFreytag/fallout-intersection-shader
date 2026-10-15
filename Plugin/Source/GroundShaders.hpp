#pragma once

namespace GroundShaders {

	bool Load();

	IDirect3DPixelShader9* GetOverlayShader(uint32_t auiPassType);

	IDirect3DPixelShader9* GetFieldShader();

}

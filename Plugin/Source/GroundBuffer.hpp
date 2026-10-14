#pragma once

class BSRenderedTexture;
class NiCamera;

// What the terrain overlay knows about the ground under each visible surface.
//
// The overlay used to extend the plane of the terrain fragment behind a static to the static itself. Where the terrain
// bends, at crests and cliff edges, that plane has nothing to do with the ground under the static. Seams appeared
// along the hidden crest line, and got worse with distance as the terrain behind a static lies further away.
//
// Instead, two steps run right before the overlay:
// 1. Height field: the recorded landscape passes are drawn again by an orthographic camera looking straight down,
//    into a depth texture around the camera. Terrain does not move, so it is only redrawn in full when the camera
//    moves away from its centre. In between, only landscape blocks not in it yet are drawn.
// 2. Ground buffer: a full-screen pass reconstructs every visible surface point from the scene depth and looks up
//    the terrain under it. It writes linear depth, height above the terrain and the terrain normal per pixel.
//
// The overlay reads the ground buffer from s14, where it read the scene depth before, so the terrain shaders
// need no further sampler. See Shaders/Ground/GroundBuffer.hlsl.
namespace GroundBuffer {
	// Call once the D3D device exists (NVSE DeferredInit).
	void Initialize();

	// Updates the height field from the landscape passes recorded into apTarget and renders the ground buffer.
	// Runs while apTarget is bound. Returns the ground buffer, or null if it is unavailable.
	IDirect3DTexture9* Render(IDirect3DDevice9* apDevice, const NiCamera* apCamera, const BSRenderedTexture* apTarget, IDirect3DBaseTexture9* apDepthTexture);
}

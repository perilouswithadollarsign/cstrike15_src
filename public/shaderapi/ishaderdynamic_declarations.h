//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
#ifndef SHADERAPI_ISHADERDYNAMIC_DECLARATIONS_H
#define SHADERAPI_ISHADERDYNAMIC_DECLARATIONS_H
//-----------------------------------------------------------------------------
// the 3D shader API interface
// This interface is all that shaders see.
//-----------------------------------------------------------------------------
enum StandardTextureId_t
{
	// Lightmaps
	TEXTURE_LIGHTMAP = 0,
	TEXTURE_LIGHTMAP_FULLBRIGHT,
	TEXTURE_LIGHTMAP_BUMPED,
	TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT,

	// Flat colors
	TEXTURE_WHITE,
	TEXTURE_BLACK,
	TEXTURE_BLACK_ALPHA_ZERO,
	TEXTURE_GREY,
	TEXTURE_GREY_ALPHA_ZERO,

	// Normalmaps
	TEXTURE_NORMALMAP_FLAT,
	TEXTURE_SSBUMP_FLAT,

	// Normalization
	TEXTURE_NORMALIZATION_CUBEMAP,
	TEXTURE_NORMALIZATION_CUBEMAP_SIGNED,

	// Frame-buffer textures
	TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0,
	TEXTURE_FRAME_BUFFER_FULL_TEXTURE_1,

	// Color correction
	TEXTURE_COLOR_CORRECTION_VOLUME_0,
	TEXTURE_COLOR_CORRECTION_VOLUME_1,
	TEXTURE_COLOR_CORRECTION_VOLUME_2,
	TEXTURE_COLOR_CORRECTION_VOLUME_3,

	// An alias to the Back Frame Buffer
	TEXTURE_FRAME_BUFFER_ALIAS,

	// Noise for shadow mapping algorithm
	TEXTURE_SHADOW_NOISE_2D,

	// A texture in which morph data gets accumulated (vs30, fast vertex textures required)
	TEXTURE_MORPH_ACCUMULATOR,

	// A texture which contains morph weights
	TEXTURE_MORPH_WEIGHTS,

	// A snapshot of the frame buffer's depth. Currently only valid on the 360
	TEXTURE_FRAME_BUFFER_FULL_DEPTH,

	// A snapshot of the frame buffer's depth. Currently only valid on the 360
	TEXTURE_IDENTITY_LIGHTWARP,

	// The current local env_cubemap
	TEXTURE_LOCAL_ENV_CUBEMAP,

	// Texture containing subdivision surface patch data
	TEXTURE_SUBDIVISION_PATCHES,

	// Screen-space texture which contains random 3D reflection vectors used in SSAO algorithm
	TEXTURE_SSAO_NOISE_2D,

	TEXTURE_PAINT,

	TEXTURE_MAX_STD_TEXTURES = TEXTURE_SSAO_NOISE_2D + 1
};

//-----------------------------------------------------------------------------
// State from ShaderAPI used to select proper vertex and pixel shader combos
//-----------------------------------------------------------------------------
struct LightState_t
{
	int  m_nNumLights;
	bool m_bAmbientLight;
	bool m_bStaticLight;
	inline int HasDynamicLight() { return (m_bAmbientLight || (m_nNumLights > 0)) ? 1 : 0; }
};



#endif

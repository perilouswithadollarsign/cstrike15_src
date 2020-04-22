//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef ISHADERSHADOW_H
#define ISHADERSHADOW_H

#ifdef _WIN32
#pragma once
#endif

#include "shaderapi/shareddefs.h"
#include <materialsystem/imaterial.h>


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class CMeshBuilder;
class IMaterialVar;
struct LightDesc_t; 


//-----------------------------------------------------------------------------
// important enumerations
//-----------------------------------------------------------------------------
enum ShaderDepthFunc_t 
{ 
	SHADER_DEPTHFUNC_NEVER,
	SHADER_DEPTHFUNC_NEARER,
	SHADER_DEPTHFUNC_EQUAL,
	SHADER_DEPTHFUNC_NEAREROREQUAL,
	SHADER_DEPTHFUNC_FARTHER,
	SHADER_DEPTHFUNC_NOTEQUAL,
	SHADER_DEPTHFUNC_FARTHEROREQUAL,
	SHADER_DEPTHFUNC_ALWAYS
};

enum ShaderBlendFactor_t
{
	SHADER_BLEND_ZERO,
	SHADER_BLEND_ONE,
	SHADER_BLEND_DST_COLOR,
	SHADER_BLEND_ONE_MINUS_DST_COLOR,
	SHADER_BLEND_SRC_ALPHA,
	SHADER_BLEND_ONE_MINUS_SRC_ALPHA,
	SHADER_BLEND_DST_ALPHA,
	SHADER_BLEND_ONE_MINUS_DST_ALPHA,
	SHADER_BLEND_SRC_ALPHA_SATURATE,
	SHADER_BLEND_SRC_COLOR,
	SHADER_BLEND_ONE_MINUS_SRC_COLOR
};

enum ShaderBlendOp_t
{
	SHADER_BLEND_OP_ADD,
	SHADER_BLEND_OP_SUBTRACT,
	SHADER_BLEND_OP_REVSUBTRACT,
	SHADER_BLEND_OP_MIN,
	SHADER_BLEND_OP_MAX
};

enum ShaderAlphaFunc_t
{
	SHADER_ALPHAFUNC_NEVER,
	SHADER_ALPHAFUNC_LESS,
	SHADER_ALPHAFUNC_EQUAL,
	SHADER_ALPHAFUNC_LEQUAL,
	SHADER_ALPHAFUNC_GREATER,
	SHADER_ALPHAFUNC_NOTEQUAL,
	SHADER_ALPHAFUNC_GEQUAL,
	SHADER_ALPHAFUNC_ALWAYS
};

enum ShaderTexChannel_t
{
	SHADER_TEXCHANNEL_COLOR = 0,
	SHADER_TEXCHANNEL_ALPHA
};

enum ShaderPolyModeFace_t
{
	SHADER_POLYMODEFACE_FRONT,
	SHADER_POLYMODEFACE_BACK,
	SHADER_POLYMODEFACE_FRONT_AND_BACK,
};

enum ShaderPolyMode_t
{
	SHADER_POLYMODE_POINT,
	SHADER_POLYMODE_LINE,
	SHADER_POLYMODE_FILL
};

enum ShaderFogMode_t
{
	SHADER_FOGMODE_DISABLED = 0,
	SHADER_FOGMODE_OO_OVERBRIGHT,
	SHADER_FOGMODE_BLACK,
	SHADER_FOGMODE_GREY,
	SHADER_FOGMODE_FOGCOLOR,
	SHADER_FOGMODE_WHITE,
	SHADER_FOGMODE_NUMFOGMODES
};



// m_ZBias has only two bits in ShadowState_t, so be careful extending this enum
enum PolygonOffsetMode_t
{
	SHADER_POLYOFFSET_DISABLE		= 0x0,
	SHADER_POLYOFFSET_DECAL			= 0x1,
	SHADER_POLYOFFSET_SHADOW_BIAS	= 0x2,
	SHADER_POLYOFFSET_RESERVED		= 0x3	// Reserved for future use
};


//-----------------------------------------------------------------------------
// The Shader interface versions
//-----------------------------------------------------------------------------
#define SHADERSHADOW_INTERFACE_VERSION	"ShaderShadow010"


//-----------------------------------------------------------------------------
// the shader API interface (methods called from shaders)
//-----------------------------------------------------------------------------
abstract_class IShaderShadow
{
public:
	// Sets the default *shadow* state
	virtual void SetDefaultState() = 0;

	// Methods related to depth buffering
	virtual void DepthFunc( ShaderDepthFunc_t depthFunc ) = 0;
	virtual void EnableDepthWrites( bool bEnable ) = 0;
	virtual void EnableDepthTest( bool bEnable ) = 0;
	virtual void EnablePolyOffset( PolygonOffsetMode_t nOffsetMode ) = 0;

	// Suppresses/activates color writing 
	virtual void EnableColorWrites( bool bEnable ) = 0;
	virtual void EnableAlphaWrites( bool bEnable ) = 0;

	// Methods related to alpha blending
	virtual void EnableBlending( bool bEnable ) = 0;
	virtual void EnableBlendingForceOpaque( bool bEnable ) = 0; // enables alpha blending on a batch but does not force it to render with translucents
	virtual void BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor ) = 0;
	virtual void EnableBlendingSeparateAlpha( bool bEnable ) = 0;
	virtual void BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor ) = 0;
	// More below...

	// Alpha testing
	virtual void EnableAlphaTest( bool bEnable ) = 0;
	virtual void AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ ) = 0;

	// Wireframe/filled polygons
	virtual void PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode ) = 0;

	// Back face culling
	virtual void EnableCulling( bool bEnable ) = 0;

	// Indicates the vertex format for use with a vertex shader
	// The flags to pass in here come from the VertexFormatFlags_t enum
	// If pTexCoordDimensions is *not* specified, we assume all coordinates
	// are 2-dimensional
	virtual void VertexShaderVertexFormat( unsigned int nFlags, 
			int nTexCoordCount, int* pTexCoordDimensions, int nUserDataSize ) = 0;

	// Pixel and vertex shader methods
	virtual void SetVertexShader( const char* pFileName, int nStaticVshIndex ) = 0;
	virtual	void SetPixelShader( const char* pFileName, int nStaticPshIndex = 0 ) = 0;

	// Convert from linear to gamma color space on writes to frame buffer.
	virtual void EnableSRGBWrite( bool bEnable ) = 0;

	// Convert from gamma to linear on texture fetch.
	virtual void EnableSRGBRead( Sampler_t sampler, bool bEnable ) = 0;

	// Per texture unit stuff
	virtual void EnableTexture( Sampler_t sampler, bool bEnable ) = 0;

	virtual void FogMode( ShaderFogMode_t fogMode, bool bVertexFog ) = 0;

	virtual void DisableFogGammaCorrection( bool bDisable ) = 0; //some blending modes won't work properly with corrected fog

	// Alpha to coverage
	virtual void EnableAlphaToCoverage( bool bEnable ) = 0;

	// Shadow map filtering
	//virtual void SetShadowDepthFiltering( Sampler_t stage ) = 0;

	// Per vertex texture unit stuff
	virtual void EnableVertexTexture( VertexTextureSampler_t sampler, bool bEnable ) = 0;
	
	// More alpha blending state
	virtual void BlendOp( ShaderBlendOp_t blendOp ) = 0;
	virtual void BlendOpSeparateAlpha( ShaderBlendOp_t blendOp ) = 0;

	virtual float GetLightMapScaleFactor( void ) const = 0;
};
// end class IShaderShadow



#endif // ISHADERSHADOW_H

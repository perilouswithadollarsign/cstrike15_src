//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERSHADOWDX8_H
#define SHADERSHADOWDX8_H

#ifdef _WIN32
#pragma once
#endif

#include "togl/rendermechanism.h"
#include "locald3dtypes.h"
#include "shaderapi/ishadershadow.h"

class IShaderAPIDX8;


//-----------------------------------------------------------------------------
// Important enumerations
//-----------------------------------------------------------------------------
enum
{
	MAX_SAMPLERS = 16,
	MAX_VERTEX_SAMPLERS = 4,
};


//-----------------------------------------------------------------------------
// A structure maintaining the shadowed board state
//-----------------------------------------------------------------------------
struct SamplerShadowState_t
{
	bool	m_TextureEnable : 1;
	bool	m_SRGBReadEnable : 1;
	bool	m_Fetch4Enable : 1;
#if ( defined ( POSIX ) )
	bool	m_ShadowFilterEnable : 1;
#endif
};

struct DepthTestState_t
{
	// assumes D3DCMPFUNC and D3DZBUFFER TYPE fit in a byte
	uint8 m_ZFunc;
	uint8 m_ZEnable;
	uint8 m_ColorWriteEnable;
	unsigned char	m_ZWriteEnable:1;
	unsigned char m_ZBias:2;

	typedef uint32 UIntAlias;
	FORCEINLINE void Check( void ) const { COMPILE_TIME_ASSERT( sizeof( *this ) == sizeof( UIntAlias ) ); }

};

struct AlphaTestAndMiscState_t
{
	uint8 m_AlphaFunc;
	uint8 m_AlphaRef;
	// Fill mode
	uint8 m_FillMode;
	unsigned char			m_AlphaTestEnable:1;
	unsigned char			m_EnableAlphaToCoverage:1;
	// Cull State?
	unsigned char			m_CullEnable:1;

	typedef uint32 UIntAlias;
	FORCEINLINE void Check( void ) const { COMPILE_TIME_ASSERT( sizeof( *this ) == sizeof( UIntAlias ) ); }
};

struct FogAndMiscState_t
{
	uint8			m_FogMode;
	unsigned char			m_bVertexFogEnable:1;
	unsigned char			m_bDisableFogGammaCorrection:1;
	// Auto-convert from linear to gamma upon writing to the frame buffer?
	unsigned char			m_SRGBWriteEnable:1;

	typedef uint16 UIntAlias;

	FORCEINLINE ShaderFogMode_t FogMode( void ) const { return ( ShaderFogMode_t ) m_FogMode; }
	FORCEINLINE void Check( void ) const { COMPILE_TIME_ASSERT( sizeof( *this ) == sizeof( UIntAlias ) ); }
};


struct AlphaBlendState_t
{
	// Alpha state
	uint8 m_SrcBlend;
	uint8 m_DestBlend;
	uint8 m_BlendOp;

	// Separate alpha blend state
	uint8 m_SrcBlendAlpha;
	uint8 m_DestBlendAlpha;
	uint8 m_BlendOpAlpha;

	unsigned char			m_AlphaBlendEnable:1;
	unsigned char			m_AlphaBlendEnabledForceOpaque:1;	// alpha blending enabled for this batch, but return false for IsTranslucent() so object can be rendered with opaques
	// Seperate Alpha Blend?
	unsigned char			m_SeparateAlphaBlendEnable:1;

	uint8 m_nPad00;

	typedef uint64 UIntAlias;
	FORCEINLINE void Check( void ) const { COMPILE_TIME_ASSERT( sizeof( *this ) == sizeof( UIntAlias ) ); }
};

struct ShadowState_t
{
	// Depth buffering state
	union
	{
		DepthTestState_t m_DepthTestState;
		DepthTestState_t::UIntAlias m_nDepthTestStateAsInt;
	};
	
	union 
	{
		AlphaTestAndMiscState_t m_AlphaTestAndMiscState;
		AlphaTestAndMiscState_t::UIntAlias m_nAlphaTestAndMiscStateAsInt;
	};

	union
	{
		FogAndMiscState_t m_FogAndMiscState;
		FogAndMiscState_t::UIntAlias m_nFogAndMiscStateAsInt;
	};

	union
	{
		AlphaBlendState_t m_AlphaBlendState;
		AlphaBlendState_t::UIntAlias m_nAlphaBlendStateAsInt;
	};


	// Sampler state. encoded as ints so we can do mask operations for fast change detection
	uint32 m_nFetch4Enable;

#if ( defined ( PLATFORM_OPENGL ) )
	uint32	m_nShadowFilterEnable;
#endif
};


//-----------------------------------------------------------------------------
// These are part of the "shadow" since they describe the shading algorithm
// but aren't actually captured in the state transition table 
// because it would produce too many transitions
//-----------------------------------------------------------------------------
struct ShadowShaderState_t
{
	// The vertex + pixel shader group to use...
	VertexShader_t	m_VertexShader;
	PixelShader_t	m_PixelShader;

	// The static vertex + pixel shader indices
	int				m_nStaticVshIndex;
	int				m_nStaticPshIndex;

	// Vertex data used by this snapshot
	// Note that the vertex format actually used will be the
	// aggregate of the vertex formats used by all snapshots in a material
	VertexFormat_t	m_VertexUsage;
};


//-----------------------------------------------------------------------------
// The shader setup API
//-----------------------------------------------------------------------------
abstract_class IShaderShadowDX8 : public IShaderShadow
{
public:
	// Initializes it
	virtual void Init() = 0;

	// Gets at the shadow state
	virtual ShadowState_t const& GetShadowState() = 0;
	virtual ShadowShaderState_t const& GetShadowShaderState() = 0;

	// This must be called right before taking a snapshot
	virtual void ComputeAggregateShadowState( ) = 0;

	// Class factory methods
	static IShaderShadowDX8* Create( IShaderAPIDX8* pShaderAPIDX8 );
	static void Destroy( IShaderShadowDX8* pShaderShadow );
};

extern IShaderShadowDX8 *g_pShaderShadowDx8;

#endif // SHADERSHADOWDX8_H

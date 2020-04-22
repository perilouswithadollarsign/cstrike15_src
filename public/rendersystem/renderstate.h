//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: API-independent render state declarations
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef RENDERSTATE_H
#define RENDERSTATE_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "tier1/generichash.h"
#include "basetypes.h"
#include "mathlib/mathlib.h"

//-----------------------------------------------------------------------------
// Handle to render state objects
//-----------------------------------------------------------------------------
DECLARE_POINTER_HANDLE( RsRasterizerStateHandle_t );
#define RENDER_RASTERIZER_STATE_HANDLE_INVALID	( (RsRasterizerStateHandle_t)0 )
DECLARE_POINTER_HANDLE( RsDepthStencilStateHandle_t );
#define RENDER_DEPTH_STENCIL_STATE_HANDLE_INVALID	( (RsDepthStencilStateHandle_t)0 )
DECLARE_POINTER_HANDLE( RsBlendStateHandle_t );
#define RENDER_BLEND_STATE_HANDLE_INVALID	( (RsBlendStateHandle_t)0 )


//-----------------------------------------------------------------------------
// Packs a float4 color to ARGB 8-bit int color compatible with D3D9
//-----------------------------------------------------------------------------
FORCEINLINE uint32 PackFloat4ColorToUInt32( float flR, float flG, float flB, float flA )
{
	uint32 nR, nG, nB, nA;
	nR = uint32( flR * 255.0f );
	nG = uint32( flG * 255.0f );
	nB = uint32( flB * 255.0f );
	nA = uint32( flA * 255.0f );
	return ( ( nA & 0xFF ) << 24 ) | ( ( nR & 0xFF ) << 16 ) | ( ( nG & 0xFF ) << 8 ) | ( ( nB & 0xFF ) );
}

//-----------------------------------------------------------------------------
// Packs a float4 color to ARGB 8-bit int color compatible with D3D9, clamping the floats to [0, 1]
//-----------------------------------------------------------------------------
FORCEINLINE uint32 ClampAndPackFloat4ColorToUInt32( float flR, float flG, float flB, float flA )
{
	uint32 nR, nG, nB, nA;
	nR = uint32( clamp( flR, 0.0f, 1.0f ) * 255.0f );
	nG = uint32( clamp( flG, 0.0f, 1.0f ) * 255.0f );
	nB = uint32( clamp( flB, 0.0f, 1.0f ) * 255.0f );
	nA = uint32( clamp( flA, 0.0f, 1.0f ) * 255.0f );
	return ( ( nA & 0xFF ) << 24 ) | ( ( nR & 0xFF ) << 16 ) | ( ( nG & 0xFF ) << 8 ) | ( ( nB & 0xFF ) );
}

//-----------------------------------------------------------------------------
// Unpacks an ARGB 8-bit int color to 4 floats
//-----------------------------------------------------------------------------
FORCEINLINE void UnpackUint32ColorToFloat4( uint32 nPackedColor, float* pflR, float* pflG, float* pflB, float* pflA )
{
	uint32 nR, nG, nB, nA;

	nR = ( nPackedColor >> 16 ) & 0xFF;
	nG = ( nPackedColor >> 8 ) & 0xFF;
	nB = ( nPackedColor ) & 0xFF;
	nA = ( nPackedColor >> 24 ) & 0xFF;

	*pflR = nR * 255.0f;
	*pflG = nG * 255.0f;
	*pflB = nB * 255.0f;
	*pflA = nA * 255.0f;
}

enum RsCullMode_t
{
	RS_CULL_NONE = 0,
	RS_CULL_BACK = 1,
	RS_CULL_FRONT = 2
};

enum RsFillMode_t
{
	RS_FILL_SOLID = 0,
	RS_FILL_WIREFRAME = 1
};

enum RsComparison_t
{
	RS_CMP_NEVER = 0,
	RS_CMP_LESS = 1,
	RS_CMP_EQUAL = 2,
	RS_CMP_LESS_EQUAL = 3,
	RS_CMP_GREATER = 4,
	RS_CMP_NOT_EQUAL = 5,
	RS_CMP_GREATER_EQUAL = 6,
	RS_CMP_ALWAYS = 7
};

enum RsStencilOp_t
{
	RS_STENCIL_OP_KEEP = 0,
	RS_STENCIL_OP_ZERO = 1,
	RS_STENCIL_OP_REPLACE = 2,
	RS_STENCIL_OP_INCR_SAT = 3,
	RS_STENCIL_OP_DECR_SAT = 4,
	RS_STENCIL_OP_INVERT = 5,
	RS_STENCIL_OP_INCR = 6,
	RS_STENCIL_OP_DECR = 7
};

enum RsHiStencilComparison360_t
{
	RS_HI_STENCIL_CMP_EQUAL = 0,
	RS_HI_STENCIL_CMP_NOT_EQUAL = 1
};

enum RsHiZMode360_t
{
	RS_HI_Z_AUTOMATIC = 0,
	RS_HI_Z_DISABLE = 1,
	RS_HI_Z_ENABLE = 2
};

enum RsBlendOp_t
{
	RS_BLEND_OP_ADD = 0,
	RS_BLEND_OP_SUBTRACT = 1,
	RS_BLEND_OP_REV_SUBTRACT = 2,
	RS_BLEND_OP_MIN = 3,
	RS_BLEND_OP_MAX = 4
};

enum RsBlendMode_t
{
	RS_BLEND_MODE_ZERO = 0,
	RS_BLEND_MODE_ONE = 1,
	RS_BLEND_MODE_SRC_COLOR = 2,
	RS_BLEND_MODE_INV_SRC_COLOR = 3,
	RS_BLEND_MODE_SRC_ALPHA = 4,
	RS_BLEND_MODE_INV_SRC_ALPHA = 5,
	RS_BLEND_MODE_DEST_ALPHA = 6,
	RS_BLEND_MODE_INV_DEST_ALPHA = 7,
	RS_BLEND_MODE_DEST_COLOR = 8,
	RS_BLEND_MODE_INV_DEST_COLOR = 9,
	RS_BLEND_MODE_SRC_ALPHA_SAT = 10,
	RS_BLEND_MODE_BLEND_FACTOR = 11,
	RS_BLEND_MODE_INV_BLEND_FACTOR = 12
};

enum RsColorWriteEnableBits_t
{
	RS_COLOR_WRITE_ENABLE_R = 0x1,
	RS_COLOR_WRITE_ENABLE_G = 0x2,
	RS_COLOR_WRITE_ENABLE_B = 0x4,
	RS_COLOR_WRITE_ENABLE_A = 0x8,
	RS_COLOR_WRITE_ENABLE_ALL = RS_COLOR_WRITE_ENABLE_R | RS_COLOR_WRITE_ENABLE_G | RS_COLOR_WRITE_ENABLE_B | RS_COLOR_WRITE_ENABLE_A
};

enum
{
	RS_MAX_RENDER_TARGETS = 8
};

//-----------------------------------------------------------------------------
struct RsRasterizerStateDesc_t
{
	RsFillMode_t m_nFillMode;
	RsCullMode_t m_nCullMode;
	bool m_bDepthClipEnable;
	bool m_bMultisampleEnable;
    int32 m_nDepthBias;	// TODO: make this a float?
    float32 m_flDepthBiasClamp;
    float32 m_flSlopeScaledDepthBias;

	/* Not exposing these DX11 states
    BOOL FrontCounterClockwise;
    BOOL ScissorEnable;
    BOOL AntialiasedLineEnable;

	This needs to be passed in explicitly when setting the blend state op:
	uint32 multisamplemask;
	*/


	FORCEINLINE uint32 HashValue() const
	{
		// TODO: Optimize this
		return HashItem( *this );
	}

	FORCEINLINE bool operator==( RsRasterizerStateDesc_t const &state ) const
	{
		return memcmp( this, &state, sizeof( RsRasterizerStateDesc_t ) ) == 0;
	}
};

//-----------------------------------------------------------------------------
struct RsDepthStencilStateDesc_t
{
	bool m_bDepthTestEnable;
	bool m_bDepthWriteEnable;
	RsComparison_t m_depthFunc;

	RsHiZMode360_t m_hiZEnable360;
	RsHiZMode360_t m_hiZWriteEnable360;

	bool m_bStencilEnable;
	uint8 m_nStencilReadMask;
	uint8 m_nStencilWriteMask;

	RsStencilOp_t m_frontStencilFailOp;
	RsStencilOp_t m_frontStencilDepthFailOp;
	RsStencilOp_t m_frontStencilPassOp;
	RsComparison_t m_frontStencilFunc;

	RsStencilOp_t m_backStencilFailOp;
	RsStencilOp_t m_backStencilDepthFailOp;
	RsStencilOp_t m_backStencilPassOp;
	RsComparison_t m_backStencilFunc;

	bool m_bHiStencilEnable360;
	bool m_bHiStencilWriteEnable360;
	RsHiStencilComparison360_t m_hiStencilFunc360;
	uint8 m_nHiStencilRef360;

	// Stencil ref not part of this, it's set explicitly when binding DS state block
	// TODO: Figure out if I should pull the 360 HiStencil ref out too.

	FORCEINLINE uint32 HashValue() const
	{
		// TODO: Optimize this
		return HashItem( *this );
	}

	FORCEINLINE bool operator==( RsDepthStencilStateDesc_t const &state ) const
	{
		return memcmp( this, &state, sizeof( RsDepthStencilStateDesc_t ) ) == 0;
	}
};

//-----------------------------------------------------------------------------
struct RsBlendStateDesc_t
{
	bool m_bAlphaToCoverageEnable;
	bool m_bIndependentBlendEnable;
	bool m_bHighPrecisionBlendEnable360;

	bool m_bBlendEnable[RS_MAX_RENDER_TARGETS];
	RsBlendMode_t m_srcBlend[RS_MAX_RENDER_TARGETS];
	RsBlendMode_t m_destBlend[RS_MAX_RENDER_TARGETS];
	RsBlendOp_t m_blendOp[RS_MAX_RENDER_TARGETS];
	RsBlendMode_t m_srcBlendAlpha[RS_MAX_RENDER_TARGETS];
	RsBlendMode_t m_destBlendAlpha[RS_MAX_RENDER_TARGETS];
	RsBlendOp_t m_blendOpAlpha[RS_MAX_RENDER_TARGETS];
	uint8 m_nRenderTargetWriteMask[RS_MAX_RENDER_TARGETS];

	FORCEINLINE uint32 HashValue() const
	{
		// TODO: Optimize this
		return HashItem( *this );
	}

	FORCEINLINE bool operator==( RsBlendStateDesc_t const &state ) const
	{
		return memcmp( this, &state, sizeof( RsBlendStateDesc_t ) ) == 0;
	}
};

enum RsFilter_t
{	
	RS_FILTER_MIN_MAG_MIP_POINT	= 0,
	RS_FILTER_MIN_MAG_POINT_MIP_LINEAR	= 0x1,
	RS_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT	= 0x4,
	RS_FILTER_MIN_POINT_MAG_MIP_LINEAR	= 0x5,
	RS_FILTER_MIN_LINEAR_MAG_MIP_POINT	= 0x10,
	RS_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR	= 0x11,
	RS_FILTER_MIN_MAG_LINEAR_MIP_POINT	= 0x14,
	RS_FILTER_MIN_MAG_MIP_LINEAR	= 0x15,
	RS_FILTER_ANISOTROPIC	= 0x55,
	RS_FILTER_COMPARISON_MIN_MAG_MIP_POINT	= 0x80,
	RS_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR	= 0x81,
	RS_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT	= 0x84,
	RS_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR	= 0x85,
	RS_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT	= 0x90,
	RS_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR	= 0x91,
	RS_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT	= 0x94,
	RS_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR	= 0x95,
	RS_FILTER_COMPARISON_ANISOTROPIC	= 0xd5
};

enum RsFilterType_t
{
	RS_FILTER_TYPE_POINT = 0,
	RS_FILTER_TYPE_LINEAR = 1
};

FORCEINLINE RsFilter_t RsEncodeBasicTextureFilter( RsFilterType_t minFilter, RsFilterType_t magFilter, RsFilterType_t mipFilter, bool bComparison )
{
	return static_cast< RsFilter_t >( ( bComparison ? 0x80 : 0 ) | ( ( minFilter & 0x3 ) << 4 ) | ( ( magFilter & 0x3 ) << 2 ) | ( mipFilter & 0x3 ) );
}

FORCEINLINE RsFilter_t RsEncodeAnisoTextureFilter( bool bComparison )
{
	return RsEncodeBasicTextureFilter( RS_FILTER_TYPE_LINEAR, RS_FILTER_TYPE_LINEAR, RS_FILTER_TYPE_LINEAR, bComparison );
}

FORCEINLINE RsFilterType_t RsGetTextureMinFilterType( RsFilter_t filter )
{
	return static_cast< RsFilterType_t >( 0x3 & ( filter >> 4 ) );
}

FORCEINLINE RsFilterType_t RsGetTextureMagFilterType( RsFilter_t filter )
{
	return static_cast< RsFilterType_t >( 0x3 & ( filter >> 2 ) );
}

FORCEINLINE RsFilterType_t RsGetTextureMipFilterType( RsFilter_t filter )
{
	return static_cast< RsFilterType_t >( 0x3 & filter );
}

FORCEINLINE bool RsIsComparisonTextureFilter( RsFilter_t filter )
{
	return ( filter & 0x80 ) != 0;
}

FORCEINLINE bool RsIsAnisoTextureFilter( RsFilter_t filter )
{
	return static_cast< RsFilter_t >( filter & ~0x80 ) == RS_FILTER_ANISOTROPIC;
}

enum RsTextureAddressMode_t
{
	RS_TEXTURE_ADDRESS_WRAP = 0,
	RS_TEXTURE_ADDRESS_MIRROR = 1,
	RS_TEXTURE_ADDRESS_CLAMP = 2,
	RS_TEXTURE_ADDRESS_BORDER = 3,
	RS_TEXTURE_ADDRESS_MIRROR_ONCE = 4
};


//-----------------------------------------------------------------------------
class CSamplerStateDesc
{
public:
	explicit CSamplerStateDesc(	RsFilter_t filter = RS_FILTER_MIN_MAG_MIP_LINEAR,
								RsTextureAddressMode_t addressU = RS_TEXTURE_ADDRESS_WRAP,
								RsTextureAddressMode_t addressV = RS_TEXTURE_ADDRESS_WRAP,
								RsTextureAddressMode_t addressW = RS_TEXTURE_ADDRESS_WRAP,
								float32 flMipLodBias = 0.0f,
								uint32 nMaxAniso = 16,
								RsComparison_t comparisonFunc = RS_CMP_LESS,
								uint32 nMinLod = 0,
								uint32 nMaxLod = 16,
								bool bSrgbFetch = false,
								bool bFetch4 = false )
	{
		SetFilterMode( filter );
		SetTextureAddressModeU( addressU );
		SetTextureAddressModeV( addressU );
		SetTextureAddressModeW( addressU );
		SetMipLodBias( flMipLodBias );
		SetMaxAnisotropy( nMaxAniso );
		SetComparisonFunc( comparisonFunc );
		SetMinMaxLod( nMinLod, nMaxLod );
		SetSrgbFetchEnabled( bSrgbFetch );
		SetFetch4Enabled( bFetch4 );
		float32 flZeros[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		SetBorderColor( flZeros );
		m_nPad = 0;
	}

	FORCEINLINE RsFilter_t GetFilterMode() const { return static_cast< RsFilter_t >( m_nFilterMode ); }
	FORCEINLINE void SetFilterMode( RsFilter_t filter ) { m_nFilterMode = filter; }

	FORCEINLINE RsTextureAddressMode_t GetTextureAddressModeU() const { return static_cast< RsTextureAddressMode_t >( m_nAddressU ); }
	FORCEINLINE RsTextureAddressMode_t GetTextureAddressModeV() const { return static_cast< RsTextureAddressMode_t >( m_nAddressV ); }
	FORCEINLINE RsTextureAddressMode_t GetTextureAddressModeW() const { return static_cast< RsTextureAddressMode_t >( m_nAddressW ); }

	FORCEINLINE void SetTextureAddressModeU( RsTextureAddressMode_t addressMode ) { m_nAddressU = addressMode; }
	FORCEINLINE void SetTextureAddressModeV( RsTextureAddressMode_t addressMode ) { m_nAddressV = addressMode; }
	FORCEINLINE void SetTextureAddressModeW( RsTextureAddressMode_t addressMode ) { m_nAddressW = addressMode; }
	
	FORCEINLINE float32 GetMipLodBias() const
	{
		return float32( m_nMipLodBias ) / 16.0f * ( m_nMipLodBiasSign ? -1.0f : 1.0f );
	}

	FORCEINLINE void SetMipLodBias( float32 flBias )
	{
		m_nMipLodBias = int( fabsf( flBias ) * 16.0f );
		m_nMipLodBiasSign = ( flBias >= 0.0f ) ? 0 : 1;
	}

	FORCEINLINE uint32 GetMaxAnisotropy() const { return 1 << m_nAnisoExp; }
	FORCEINLINE void SetMaxAnisotropy( uint32 nMaxAniso )
	{
		uint32 nAnisoExp = uint32( FastLog2( MAX( nMaxAniso, 1 ) ) );
		m_nAnisoExp = MIN( nAnisoExp, 7 );
	}

	FORCEINLINE RsComparison_t GetComparisonFunc() const { return static_cast< RsComparison_t >( m_nComparisonFunc ); }
	FORCEINLINE void SetComparisonFunc( RsComparison_t compFunc ) { m_nComparisonFunc = compFunc; }

	FORCEINLINE void SetBorderColor( const float32 *pBorderColor )
	{
		m_nBorderColor8Bit = ClampAndPackFloat4ColorToUInt32( pBorderColor[0], pBorderColor[1], pBorderColor[2], pBorderColor[3] );
	}

	FORCEINLINE void GetBorderColor( float32 *pBorderColorOut ) const
	{
		UnpackUint32ColorToFloat4( m_nBorderColor8Bit, pBorderColorOut, pBorderColorOut + 1, pBorderColorOut + 2, pBorderColorOut + 3 );
	}

	FORCEINLINE uint32 GetBorderColor32Bit() const
	{
		return m_nBorderColor8Bit;
	}

	FORCEINLINE void GetMinMaxLod( uint32 *pMinLodOut, uint32 *pMaxLodOut ) const
	{
		*pMinLodOut = m_nMinLod;
		*pMaxLodOut = m_nMaxLod;
	}

	FORCEINLINE uint32 GetMinLod() const
	{
		return m_nMinLod;
	}

	FORCEINLINE uint32 GetMaxLod() const
	{
		return m_nMaxLod;
	}

	FORCEINLINE void SetMinMaxLod( uint32 nMinLod, uint32 nMaxLod )
	{
		m_nMinLod = MIN( 15, nMinLod );
		m_nMaxLod = MIN( 15, nMaxLod );
	}

	FORCEINLINE void SetMinLod( uint32 nMinLod )
	{
		m_nMinLod = MIN( 15, nMinLod );
	}

	FORCEINLINE void SetMaxLod( uint32 nMaxLod )
	{
		m_nMaxLod = MIN( 15, nMaxLod );
	}

	bool GetFetch4Enabled() const { return m_nFetch4Enable ? true : false; }
	void SetFetch4Enabled( bool bEnable ) { m_nFetch4Enable = bEnable; }

	bool GetSrgbFetchEnabled() const { return m_nSrgbFetchEnable ? true : false; }
	void SetSrgbFetchEnabled( bool bEnable ) { m_nSrgbFetchEnable = bEnable; }

	FORCEINLINE uint32 HashValue( void ) const
	{
		COMPILE_TIME_ASSERT( sizeof( CSamplerStateDesc ) == 12 );
		return Hash12( this );
	}

	FORCEINLINE bool operator==( CSamplerStateDesc const &state ) const
	{
		return memcmp( this, &state, sizeof( CSamplerStateDesc ) ) == 0;
	}

private:

	// 32 bits
	uint32 m_nFilterMode : 8;
	uint32 m_nMipLodBias : 8;	// 4.4 fixed point
	uint32 m_nMipLodBiasSign : 1;
	uint32 m_nAddressU : 3;
	uint32 m_nAddressV : 3;
	uint32 m_nAddressW : 3;
	uint32 m_nAnisoExp : 3;
	uint32 m_nComparisonFunc : 3;

	// 32 bits
	uint32 m_nBorderColor8Bit;

	// 32 bits (18 bits available for extension
	uint32 m_nMinLod : 6;	// TODO: Either make them fixed-point, or kick them out of the state block alltogether
	uint32 m_nMaxLod : 6;
	uint32 m_nFetch4Enable : 1;
	uint32 m_nSrgbFetchEnable : 1;
	uint32 m_nPad : 18;
};


//-----------------------------------------------------------------------------
// Enums for builtin state objects
//-----------------------------------------------------------------------------
enum RenderCullMode_t
{
	RENDER_CULLMODE_CULL_BACKFACING = 0,		// this culls polygons with clockwise winding
	RENDER_CULLMODE_CULL_FRONTFACING = 1,		// this culls polygons with counterclockwise winding
	RENDER_CULLMODE_CULL_NONE = 2,				// no culling
};

enum RenderZBufferMode_t
{
	RENDER_ZBUFFER_NONE = 0,
	RENDER_ZBUFFER_ZTEST_AND_WRITE,
	RENDER_ZBUFFER_ZTEST_NO_WRITE,
	RENDER_ZBUFFER_ZTEST_GREATER_NO_WRITE,
	RENDER_ZBUFFER_ZTEST_EQUAL_NO_WRITE,

	// Stencil modes
	RENDER_ZBUFFER_NONE_STENCIL_TEST_NOTEQUAL,
	RENDER_ZBUFFER_ZTEST_AND_WRITE_STENCIL_SET1,
	RENDER_ZBUFFER_ZTEST_NO_WRITE_STENCIL_TEST_NOTEQUAL_SET0,
	RENDER_ZBUFFER_ZTEST_GREATER_NO_WRITE_STENCIL_TEST_NOTEQUAL_SET0,

	RENDER_ZBUFFER_NUM_BUILTIN_MODES
};

enum RenderBlendMode_t
{
	RENDER_BLEND_NONE = 0,
	RENDER_BLEND_NOPIXELWRITE = 1,
	RENDER_BLEND_RGBAPIXELWRITE,
	RENDER_BLEND_ALPHABLENDING,
	RENDER_BLEND_ADDITIVE_ON_ALPHA,

	RENDER_NUM_BUILTIN_BLENDSTATES
};

#endif // RENDERSTATE_H
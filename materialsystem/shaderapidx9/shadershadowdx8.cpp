//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#define DISABLE_PROTECTED_THINGS
#include "togl/rendermechanism.h"
#include "shadershadowdx8.h"
#include "locald3dtypes.h"
#include "utlvector.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapidx8_global.h"
#include "shaderapidx8.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialsystem.h"
#include "imeshdx8.h"
#include "materialsystem/materialsystem_config.h"
#include "vertexshaderdx8.h"

// NOTE: This must be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// The DX8 implementation of the shader setup interface
//-----------------------------------------------------------------------------
class CShaderShadowDX8 : public IShaderShadowDX8
{
public:
	// constructor, destructor
	CShaderShadowDX8( );
	virtual ~CShaderShadowDX8();

	// Initialize render state
	void Init( );

	// Sets the default state
	void SetDefaultState();

	// Methods related to depth buffering
	void DepthFunc( ShaderDepthFunc_t depthFunc );
	void EnableDepthWrites( bool bEnable );
	void EnableDepthTest( bool bEnable );
	void EnablePolyOffset( PolygonOffsetMode_t nOffsetMode );

	// Suppresses/activates color writing 
	void EnableColorWrites( bool bEnable );
	void EnableAlphaWrites( bool bEnable );

	// Methods related to alpha blending
	void EnableBlending( bool bEnable );
	void EnableBlendingForceOpaque( bool bEnable );
	void BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor );
	void BlendOp( ShaderBlendOp_t blendOp );
	void EnableBlendingSeparateAlpha( bool bEnable );
	void BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor );
	void BlendOpSeparateAlpha( ShaderBlendOp_t blendOp );

	// Alpha testing
	void EnableAlphaTest( bool bEnable );
	void AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ );

	// Wireframe/filled polygons
	void PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode );

	// Back face culling
	void EnableCulling( bool bEnable );
	
	// Convert from linear to gamma color space on writes to frame buffer.
	void EnableSRGBWrite( bool bEnable );

	// Convert from gamma to linear on texture fetch.
	void EnableSRGBRead( Sampler_t stage, bool bEnable );

	// Set up appropriate shadow filtering state (such as Fetch4 on ATI)
	//void SetShadowDepthFiltering( Sampler_t stage );

	// Computes the vertex format
	virtual void VertexShaderVertexFormat( unsigned int nFlags, 
		int nTexCoordCount, int* pTexCoordDimensions, int nUserDataSize );

	// Pixel and vertex shader methods
	virtual void SetVertexShader( const char* pFileName, int nStaticVshIndex );
	virtual	void SetPixelShader( const char* pFileName, int nStaticPshIndex );

	// Per texture unit stuff
	void EnableTexture( Sampler_t stage, bool bEnable );
	void EnableVertexTexture( VertexTextureSampler_t sampler, bool bEnable );

	// Last call to be make before snapshotting
	void ComputeAggregateShadowState( );

	// Gets at the shadow state
	const ShadowState_t & GetShadowState();
	const ShadowShaderState_t & GetShadowShaderState();

	void FogMode( ShaderFogMode_t fogMode, bool bVertexFog );
	void DisableFogGammaCorrection( bool bDisable );

	// Alpha to coverage
	void EnableAlphaToCoverage( bool bEnable );

	virtual float GetLightMapScaleFactor( void ) const;

private:
	struct SamplerState_t
	{
		bool m_TextureEnable : 1;
	};

	// Computes the blend factor
	D3DBLEND BlendFuncValue( ShaderBlendFactor_t factor ) const;

	// Computes the blend op
	D3DBLENDOP BlendOpValue( ShaderBlendOp_t blendOp ) const;

	// Configures our texture indices
	void ConfigureTextureCoordinates( unsigned int flags );

	// returns true if we're using texture coordinates at a given stage
	bool IsUsingTextureCoordinates( Sampler_t stage ) const;

	// State needed to create the snapshots
	IMaterialSystemHardwareConfig* m_pHardwareConfig;
	
	// Alpha blending...
	D3DBLEND	m_SrcBlend;
	D3DBLEND	m_DestBlend;
	D3DBLENDOP	m_BlendOp;

	// Separate alpha blending...
	D3DBLEND	m_SrcBlendAlpha;
	D3DBLEND	m_DestBlendAlpha;
	D3DBLENDOP	m_BlendOpAlpha;

	// Alpha testing
	D3DCMPFUNC	m_AlphaFunc;
	int			m_AlphaRef;

	// The current shadow state
	ShadowState_t m_ShadowState;
	ShadowShaderState_t m_ShadowShaderState;

	// State info stores with each sampler stage
	SamplerState_t m_SamplerState[MAX_SAMPLERS];
	SamplerState_t m_VertexSamplerState[MAX_VERTEX_SAMPLERS];
};


//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
static CShaderShadowDX8 g_ShaderShadow;
IShaderShadowDX8 *g_pShaderShadowDx8 = &g_ShaderShadow;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderShadowDX8, IShaderShadow, 
								  SHADERSHADOW_INTERFACE_VERSION, g_ShaderShadow )

//-----------------------------------------------------------------------------
// Global instance
//-----------------------------------------------------------------------------
IShaderShadowDX8* ShaderShadow()
{
	return &g_ShaderShadow;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderShadowDX8::CShaderShadowDX8( ) : m_pHardwareConfig(0)
{	
	memset( &m_ShadowState, 0, sizeof(m_ShadowState) );
}

CShaderShadowDX8::~CShaderShadowDX8()
{
}


//-----------------------------------------------------------------------------
// Initialize render state
//-----------------------------------------------------------------------------
void CShaderShadowDX8::Init( )
{
	m_pHardwareConfig = g_pHardwareConfig;
	
	// Clear out the shadow state
	memset( &m_ShadowState, 0, sizeof(m_ShadowState) );

	m_ShadowState.m_FogAndMiscState.m_bDisableFogGammaCorrection = false;

	// Pixel + vertex shaders
	m_ShadowShaderState.m_VertexShader = INVALID_SHADER;
	m_ShadowShaderState.m_PixelShader = INVALID_SHADER;
	m_ShadowShaderState.m_nStaticPshIndex = 0;
	m_ShadowShaderState.m_nStaticVshIndex = 0;
	m_ShadowShaderState.m_VertexUsage = 0;

	m_ShadowState.m_nFetch4Enable = 0;
#if ( defined ( DX_TO_GL_ABSTRACTION ) )
	m_ShadowState.m_nShadowFilterEnable = 0;
#endif

	for (int i = 0; i < MAX_SAMPLERS; ++i)
	{
		// A *real* measure if the texture stage is being used.
		// we sometimes have to set the shadow state to not mirror this.
		m_SamplerState[i].m_TextureEnable = false;
	}
}


//-----------------------------------------------------------------------------
// Sets the default state
//-----------------------------------------------------------------------------
void CShaderShadowDX8::SetDefaultState()
{
	DepthFunc( SHADER_DEPTHFUNC_NEAREROREQUAL );
	EnableDepthWrites( true );
	EnableDepthTest( true );
	EnableColorWrites( true );
	EnableAlphaWrites( false );
	EnableAlphaTest( false );
	EnableBlending( false );
	BlendFunc( SHADER_BLEND_ZERO, SHADER_BLEND_ZERO );
	BlendOp( SHADER_BLEND_OP_ADD );
	EnableBlendingSeparateAlpha( false );
	BlendFuncSeparateAlpha( SHADER_BLEND_ZERO, SHADER_BLEND_ZERO );
	BlendOpSeparateAlpha( SHADER_BLEND_OP_ADD );
	AlphaFunc( SHADER_ALPHAFUNC_GEQUAL, 0.7f );
	PolyMode( SHADER_POLYMODEFACE_FRONT_AND_BACK, SHADER_POLYMODE_FILL );
	EnableCulling( true );
	EnableAlphaToCoverage( false );
	EnablePolyOffset( SHADER_POLYOFFSET_DISABLE );
	EnableSRGBWrite( false );
	SetVertexShader( NULL, 0 );
	SetPixelShader( NULL, 0 );
	FogMode( SHADER_FOGMODE_DISABLED, false );
	DisableFogGammaCorrection( false );
	m_ShadowShaderState.m_VertexUsage = 0;

	int i;
	int nSamplerCount = HardwareConfig()->GetSamplerCount();
	for( i = 0; i < nSamplerCount; i++ )
	{
		EnableTexture( (Sampler_t)i, false );
		EnableSRGBRead( (Sampler_t)i, false );
	}
}


//-----------------------------------------------------------------------------
// Gets at the shadow state
//-----------------------------------------------------------------------------
const ShadowState_t &CShaderShadowDX8::GetShadowState()
{
	return m_ShadowState;
}

const ShadowShaderState_t &CShaderShadowDX8::GetShadowShaderState()
{
	return m_ShadowShaderState;
}


//-----------------------------------------------------------------------------
// Depth functions...
//-----------------------------------------------------------------------------
void CShaderShadowDX8::DepthFunc( ShaderDepthFunc_t depthFunc )
{
	D3DCMPFUNC zFunc;

	switch( depthFunc )
	{
	case SHADER_DEPTHFUNC_NEVER:
		zFunc = D3DCMP_NEVER;
		break;
	case SHADER_DEPTHFUNC_NEARER:
		zFunc = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? D3DCMP_GREATER : D3DCMP_LESS;
		break;
	case SHADER_DEPTHFUNC_EQUAL:
		zFunc = D3DCMP_EQUAL;
		break;
	case SHADER_DEPTHFUNC_NEAREROREQUAL:
		zFunc = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? D3DCMP_GREATEREQUAL : D3DCMP_LESSEQUAL;
		break;
	case SHADER_DEPTHFUNC_FARTHER:
		zFunc = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? D3DCMP_LESS : D3DCMP_GREATER;
		break;
	case SHADER_DEPTHFUNC_NOTEQUAL:
		zFunc = D3DCMP_NOTEQUAL;
		break;
	case SHADER_DEPTHFUNC_FARTHEROREQUAL:
		zFunc = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? D3DCMP_LESSEQUAL : D3DCMP_GREATEREQUAL;
		break;
	case SHADER_DEPTHFUNC_ALWAYS:
		zFunc = D3DCMP_ALWAYS;
		break;
	default:
		zFunc = D3DCMP_ALWAYS;
		Warning( "DepthFunc: invalid param\n" );
		break;
	}

	m_ShadowState.m_DepthTestState.m_ZFunc = zFunc;
}

void CShaderShadowDX8::EnableDepthWrites( bool bEnable )
{
	m_ShadowState.m_DepthTestState.m_ZWriteEnable = bEnable;
}

void CShaderShadowDX8::EnableDepthTest( bool bEnable )
{
	m_ShadowState.m_DepthTestState.m_ZEnable = bEnable ? D3DZB_TRUE : D3DZB_FALSE;
}

void CShaderShadowDX8::EnablePolyOffset( PolygonOffsetMode_t nOffsetMode )
{
	m_ShadowState.m_DepthTestState.m_ZBias = nOffsetMode;
}

//-----------------------------------------------------------------------------
// Color write state
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableColorWrites( bool bEnable )
{
	if (bEnable)
	{
		m_ShadowState.m_DepthTestState.m_ColorWriteEnable |= D3DCOLORWRITEENABLE_BLUE |
							D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED;
	}
	else
	{
		m_ShadowState.m_DepthTestState.m_ColorWriteEnable &= ~( D3DCOLORWRITEENABLE_BLUE |
							D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED );
	}
}

void CShaderShadowDX8::EnableAlphaWrites( bool bEnable )
{
	if (bEnable)
	{
		m_ShadowState.m_DepthTestState.m_ColorWriteEnable |= D3DCOLORWRITEENABLE_ALPHA;
	}
	else
	{
		m_ShadowState.m_DepthTestState.m_ColorWriteEnable &= ~D3DCOLORWRITEENABLE_ALPHA;
	}
}


//-----------------------------------------------------------------------------
// Alpha blending states
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableBlending( bool bEnable )
{
	m_ShadowState.m_AlphaBlendState.m_AlphaBlendEnable = bEnable;
	m_ShadowState.m_AlphaBlendState.m_AlphaBlendEnabledForceOpaque = false;
}

void CShaderShadowDX8::EnableBlendingForceOpaque( bool bEnable )
{
	m_ShadowState.m_AlphaBlendState.m_AlphaBlendEnable = bEnable;
	m_ShadowState.m_AlphaBlendState.m_AlphaBlendEnabledForceOpaque = true;
}

// Separate alpha blending
void CShaderShadowDX8::EnableBlendingSeparateAlpha( bool bEnable )
{
	m_ShadowState.m_AlphaBlendState.m_SeparateAlphaBlendEnable = bEnable;
}

void CShaderShadowDX8::EnableAlphaTest( bool bEnable )
{
	m_ShadowState.m_AlphaTestAndMiscState.m_AlphaTestEnable = bEnable;
}

void CShaderShadowDX8::AlphaFunc( ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */ )
{
	D3DCMPFUNC d3dCmpFunc;

	switch( alphaFunc )
	{
	case SHADER_ALPHAFUNC_NEVER:
		d3dCmpFunc = D3DCMP_NEVER;
		break;
	case SHADER_ALPHAFUNC_LESS:
		d3dCmpFunc = D3DCMP_LESS;
		break;
	case SHADER_ALPHAFUNC_EQUAL:
		d3dCmpFunc = D3DCMP_EQUAL;
		break;
	case SHADER_ALPHAFUNC_LEQUAL:
		d3dCmpFunc = D3DCMP_LESSEQUAL;
		break;
	case SHADER_ALPHAFUNC_GREATER:
		d3dCmpFunc = D3DCMP_GREATER;
		break;
	case SHADER_ALPHAFUNC_NOTEQUAL:
		d3dCmpFunc = D3DCMP_NOTEQUAL;
		break;
	case SHADER_ALPHAFUNC_GEQUAL:
		d3dCmpFunc = D3DCMP_GREATEREQUAL;
		break;
	case SHADER_ALPHAFUNC_ALWAYS:
		d3dCmpFunc = D3DCMP_ALWAYS;
		break;
	default:
		Warning( "AlphaFunc: invalid param\n" );
		return;
	}

	m_AlphaFunc = d3dCmpFunc;
	m_AlphaRef = (int)(alphaRef * 255);
}

D3DBLEND CShaderShadowDX8::BlendFuncValue( ShaderBlendFactor_t factor ) const
{
	switch( factor )
	{
	case SHADER_BLEND_ZERO:
		return D3DBLEND_ZERO;

	case SHADER_BLEND_ONE:
		return D3DBLEND_ONE;

	case SHADER_BLEND_DST_COLOR:
		return D3DBLEND_DESTCOLOR;

	case SHADER_BLEND_ONE_MINUS_DST_COLOR:
		return D3DBLEND_INVDESTCOLOR;

	case SHADER_BLEND_SRC_ALPHA:
		return D3DBLEND_SRCALPHA;

	case SHADER_BLEND_ONE_MINUS_SRC_ALPHA:
		return D3DBLEND_INVSRCALPHA;

	case SHADER_BLEND_DST_ALPHA:
		return D3DBLEND_DESTALPHA;

	case SHADER_BLEND_ONE_MINUS_DST_ALPHA:
		return D3DBLEND_INVDESTALPHA;

	case SHADER_BLEND_SRC_ALPHA_SATURATE:
		return D3DBLEND_SRCALPHASAT;

	case SHADER_BLEND_SRC_COLOR:
		return D3DBLEND_SRCCOLOR;

	case SHADER_BLEND_ONE_MINUS_SRC_COLOR:
		return D3DBLEND_INVSRCCOLOR;
	}

	Warning( "BlendFunc: invalid factor\n" );
	return D3DBLEND_ONE;
}

D3DBLENDOP CShaderShadowDX8::BlendOpValue( ShaderBlendOp_t blendOp ) const
{
	switch( blendOp )
	{
	case SHADER_BLEND_OP_ADD:
		return D3DBLENDOP_ADD;

	case SHADER_BLEND_OP_SUBTRACT:
		return D3DBLENDOP_SUBTRACT;

	case SHADER_BLEND_OP_REVSUBTRACT:
		return D3DBLENDOP_REVSUBTRACT;

	case SHADER_BLEND_OP_MIN:
		return D3DBLENDOP_MIN;

	case SHADER_BLEND_OP_MAX:
		return D3DBLENDOP_MAX;
	}

	Warning( "BlendOp: invalid op\n" );
	return D3DBLENDOP_ADD;
}

void CShaderShadowDX8::BlendFunc( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
	D3DBLEND d3dSrcFactor = BlendFuncValue( srcFactor );
	D3DBLEND d3dDstFactor = BlendFuncValue( dstFactor );
	m_SrcBlend = d3dSrcFactor;
	m_DestBlend = d3dDstFactor;
}

// Separate alpha blending
void CShaderShadowDX8::BlendFuncSeparateAlpha( ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor )
{
	D3DBLEND d3dSrcFactor = BlendFuncValue( srcFactor );
	D3DBLEND d3dDstFactor = BlendFuncValue( dstFactor );
	m_SrcBlendAlpha = d3dSrcFactor;
	m_DestBlendAlpha = d3dDstFactor;
}

void CShaderShadowDX8::BlendOp( ShaderBlendOp_t blendOp )
{
	m_BlendOp = BlendOpValue( blendOp );
}

void CShaderShadowDX8::BlendOpSeparateAlpha( ShaderBlendOp_t blendOp )
{
	m_BlendOpAlpha = BlendOpValue( blendOp );
}

//-----------------------------------------------------------------------------
// Polygon fill mode states
//-----------------------------------------------------------------------------
void CShaderShadowDX8::PolyMode( ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode )
{
	// DX8 can't handle different modes on front and back faces
// FIXME:	Assert( face == SHADER_POLYMODEFACE_FRONT_AND_BACK );
	if (face == SHADER_POLYMODEFACE_BACK)
		return;
	
	D3DFILLMODE fillMode;
	switch( polyMode )
	{
	case SHADER_POLYMODE_POINT:
		fillMode = D3DFILL_POINT;
		break;
	case SHADER_POLYMODE_LINE:
		fillMode =  D3DFILL_WIREFRAME;
		break;
	case SHADER_POLYMODE_FILL:
		fillMode =  D3DFILL_SOLID;
		break;
	default:
		Warning( "PolyMode: invalid poly mode\n" );
		return;
	}

	m_ShadowState.m_AlphaTestAndMiscState.m_FillMode = fillMode;
}


//-----------------------------------------------------------------------------
// Backface cull states
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableCulling( bool bEnable )
{
	m_ShadowState.m_AlphaTestAndMiscState.m_CullEnable = bEnable;
}


//-----------------------------------------------------------------------------
// Alpha to coverage
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableAlphaToCoverage( bool bEnable )
{
	m_ShadowState.m_AlphaTestAndMiscState.m_EnableAlphaToCoverage = bEnable;
}

//-----------------------------------------------------------------------------
// Enables auto-conversion from linear to gamma space on write to framebuffer.
//-----------------------------------------------------------------------------
void CShaderShadowDX8::EnableSRGBWrite( bool bEnable )
{
	if ( m_pHardwareConfig->SupportsSRGB() )
	{
		m_ShadowState.m_FogAndMiscState.m_SRGBWriteEnable = bEnable;
	}
	else
	{
		m_ShadowState.m_FogAndMiscState.m_SRGBWriteEnable = false;
	}
}

void CShaderShadowDX8::EnableTexture( Sampler_t sampler, bool bEnable )
{
	if ( sampler < m_pHardwareConfig->GetSamplerCount() )
	{
		m_SamplerState[sampler].m_TextureEnable = bEnable;
	}
	else
	{
		Warning( "Attempting to bind a texture to an invalid sampler (%d)!\n", sampler );
	}
}

void CShaderShadowDX8::EnableVertexTexture( VertexTextureSampler_t vtSampler, bool bEnable )
{
	if ( vtSampler < m_pHardwareConfig->GetVertexSamplerCount() )
	{
		m_VertexSamplerState[vtSampler].m_TextureEnable = bEnable;
	}
	else
	{
		Warning( "Attempting to bind a texture to an invalid vertex sampler (%d)!\n", vtSampler );
	}
}


void CShaderShadowDX8::EnableSRGBRead( Sampler_t sampler, bool bEnable )
{
}

#if 0
void CShaderShadowDX8::SetShadowDepthFiltering( Sampler_t stage )
{
	int nMask = ( 1 << stage );
	if ( stage < m_pHardwareConfig->GetSamplerCount() )
	{
#if ( defined ( POSIX ) )
//		m_ShadowState.m_ShadowFilterEnable |= nMask;
#else
		if ( !m_pHardwareConfig->SupportsFetch4() )
		{
			m_ShadowState.m_nFetch4Enable &= ~nMask;
		}
		else
		{
			m_ShadowState.m_nFetch4Enable |= nMask;
		}
#endif
	}
	else
	{
		Warning( "Attempting set shadow filtering state on an invalid sampler (%d)!\n", stage );
	}
}
#endif

//-----------------------------------------------------------------------------
// Compute the vertex format from vertex descriptor flags
//-----------------------------------------------------------------------------
void CShaderShadowDX8::VertexShaderVertexFormat( unsigned int nFlags, 
	int nTexCoordCount, int* pTexCoordDimensions, int nUserDataSize )
{
	// Code that creates a Mesh should specify whether it contains bone weights+indices, *not* the shader.
	Assert( ( nFlags & VERTEX_BONE_INDEX ) == 0 );
	nFlags &= ~VERTEX_BONE_INDEX;

	// This indicates we're using a vertex shader
	m_ShadowShaderState.m_VertexUsage = MeshMgr()->ComputeVertexFormat( nFlags, nTexCoordCount, 
		pTexCoordDimensions, 0, nUserDataSize );

	// Avoid an error if vertex stream 0 is too narrow
	if ( CVertexBufferBase::VertexFormatSize( m_ShadowShaderState.m_VertexUsage ) <= 16 )
	{
		// FIXME: this is only necessary because we
		//          (a) put the flex normal/position stream in ALL vertex decls
		//          (b) bind stream 0's VB to stream 2 if there is no actual flex data
		//        ...it would be far more sensible to not add stream 2 to all vertex decls.
		static bool bComplained = false;
		if( !bComplained )
		{
			Warning( "ERROR: shader asking for a too-narrow vertex format - you will see errors if running with debug D3D DLLs!\n\tPadding the vertex format with extra texcoords\n\tWill not warn again.\n" );
			bComplained = true;
		}
		// All vertex formats should contain position...
		Assert( nFlags & VERTEX_POSITION );
		nFlags |= VERTEX_POSITION;
		// This error should occur only if we have zero texcoords, or if we have a single, 1-D texcoord
		Assert( ( nTexCoordCount == 0 ) ||
			    ( ( nTexCoordCount == 1 ) && pTexCoordDimensions && ( pTexCoordDimensions[0] == 1 ) ) );
		nTexCoordCount = 1;
		m_ShadowShaderState.m_VertexUsage = MeshMgr()->ComputeVertexFormat( nFlags, nTexCoordCount, NULL, 0, nUserDataSize );
	}
}


//-----------------------------------------------------------------------------
// Pixel and vertex shader methods
//-----------------------------------------------------------------------------
void CShaderShadowDX8::SetVertexShader( const char* pFileName, int nStaticVshIndex )
{
	char	debugLabel[500] = "";
#ifdef DX_TO_GL_ABSTRACTION
	Q_snprintf( debugLabel, sizeof(debugLabel), "vs-file %s vs-index %d", pFileName, nStaticVshIndex ); 
#endif

	m_ShadowShaderState.m_VertexShader = ShaderManager()->CreateVertexShader( pFileName, nStaticVshIndex, debugLabel );
	m_ShadowShaderState.m_nStaticVshIndex = nStaticVshIndex;
}

void CShaderShadowDX8::SetPixelShader( const char* pFileName, int nStaticPshIndex )
{
	char	debugLabel[500] = "";
#ifdef DX_TO_GL_ABSTRACTION
	Q_snprintf( debugLabel, sizeof(debugLabel), "ps-file %s ps-index %d", pFileName, nStaticPshIndex ); 
#endif

	m_ShadowShaderState.m_PixelShader = ShaderManager()->CreatePixelShader( pFileName, nStaticPshIndex, debugLabel );
	m_ShadowShaderState.m_nStaticPshIndex = nStaticPshIndex;
}


//-----------------------------------------------------------------------------
// Returns the lightmap scale factor
//-----------------------------------------------------------------------------
float CShaderShadowDX8::GetLightMapScaleFactor( void ) const
{
	return g_pHardwareConfig->GetLightMapScaleFactor();
}


//-----------------------------------------------------------------------------
// Fog
//-----------------------------------------------------------------------------
void CShaderShadowDX8::FogMode( ShaderFogMode_t fogMode, bool bVertexFog )
{
	Assert( fogMode >= 0 && fogMode < SHADER_FOGMODE_NUMFOGMODES );
	m_ShadowState.m_FogAndMiscState.m_FogMode = fogMode;
	m_ShadowState.m_FogAndMiscState.m_bVertexFogEnable = bVertexFog;
}

void CShaderShadowDX8::DisableFogGammaCorrection( bool bDisable )
{
	m_ShadowState.m_FogAndMiscState.m_bDisableFogGammaCorrection = bDisable;
}


//-----------------------------------------------------------------------------
// NOTE: See Version 5 of this file for NVidia 8-stage shader stuff
//-----------------------------------------------------------------------------
inline bool CShaderShadowDX8::IsUsingTextureCoordinates( Sampler_t sampler ) const
{
	return m_SamplerState[sampler].m_TextureEnable;
}


//-----------------------------------------------------------------------------
// Computes shadow state based on bunches of other parameters
//-----------------------------------------------------------------------------
void CShaderShadowDX8::ComputeAggregateShadowState( )
{
	// Initialize the texture stage usage; this may get changed later
	int nEnableMask = 0;
	for (int i = 0; i < m_pHardwareConfig->GetSamplerCount(); ++i)
	{
		if ( IsUsingTextureCoordinates( (Sampler_t)i ) )
		{
			nEnableMask |= ( 1 << i );
		}
	}
	// Always use the same alpha src + dest if it's disabled
	// NOTE: This is essential for stateblocks to work
	if ( m_ShadowState.m_AlphaBlendState.m_AlphaBlendEnable )
	{
		m_ShadowState.m_AlphaBlendState.m_SrcBlend = m_SrcBlend;
		m_ShadowState.m_AlphaBlendState.m_DestBlend = m_DestBlend;
		m_ShadowState.m_AlphaBlendState.m_BlendOp = m_BlendOp;
	}
	else
	{
		m_ShadowState.m_AlphaBlendState.m_SrcBlend = D3DBLEND_ONE;
		m_ShadowState.m_AlphaBlendState.m_DestBlend = D3DBLEND_ZERO;
		m_ShadowState.m_AlphaBlendState.m_BlendOp = D3DBLENDOP_ADD;
	}

	// GR
	if (m_ShadowState.m_AlphaBlendState.m_SeparateAlphaBlendEnable)
	{
		m_ShadowState.m_AlphaBlendState.m_SrcBlendAlpha = m_SrcBlendAlpha;
		m_ShadowState.m_AlphaBlendState.m_DestBlendAlpha = m_DestBlendAlpha;
		m_ShadowState.m_AlphaBlendState.m_BlendOpAlpha = m_BlendOpAlpha;
	}
	else
	{
		m_ShadowState.m_AlphaBlendState.m_SrcBlendAlpha = D3DBLEND_ONE;
		m_ShadowState.m_AlphaBlendState.m_DestBlendAlpha = D3DBLEND_ZERO;
		m_ShadowState.m_AlphaBlendState.m_BlendOpAlpha = D3DBLENDOP_ADD;
	}

	// Use the same func if it's disabled
	if (m_ShadowState.m_AlphaTestAndMiscState.m_AlphaTestEnable)
	{
		// If alpha test is enabled, just use the values set
		m_ShadowState.m_AlphaTestAndMiscState.m_AlphaFunc = m_AlphaFunc;
		m_ShadowState.m_AlphaTestAndMiscState.m_AlphaRef = m_AlphaRef;
	}
	else
	{
		// A default value
		m_ShadowState.m_AlphaTestAndMiscState.m_AlphaFunc = D3DCMP_GREATEREQUAL;
		m_ShadowState.m_AlphaTestAndMiscState.m_AlphaRef = 0;

		// If not alpha testing and doing a standard alpha blend, force on alpha testing
		if ( m_ShadowState.m_AlphaBlendState.m_AlphaBlendEnable )
		{
			if ( ( m_ShadowState.m_AlphaBlendState.m_SrcBlend == D3DBLEND_SRCALPHA ) && ( m_ShadowState.m_AlphaBlendState.m_DestBlend == D3DBLEND_INVSRCALPHA ) )
			{
				m_ShadowState.m_AlphaTestAndMiscState.m_AlphaFunc = D3DCMP_GREATEREQUAL;
				m_ShadowState.m_AlphaTestAndMiscState.m_AlphaRef = 1;
			}
		}
	}

	// Alpha to coverage
	if ( m_ShadowState.m_AlphaTestAndMiscState.m_EnableAlphaToCoverage )
	{
		// Only allow this to be enabled if blending is disabled and testing is enabled
		if ( ( m_ShadowState.m_AlphaBlendState.m_AlphaBlendEnable == true ) || ( m_ShadowState.m_AlphaTestAndMiscState.m_AlphaTestEnable == false ) )
		{
			m_ShadowState.m_AlphaTestAndMiscState.m_EnableAlphaToCoverage = false;
		}
	}
}

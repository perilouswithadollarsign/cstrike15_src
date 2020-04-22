//======Copyright (c) Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//
// The dx8 implementation of the shader API
//==================================================================//

/*
DX9 todo:
-make the transforms in the older shaders match the transforms in lightmappedgeneric
-fix polyoffset for hardware that doesn't support D3DRS_SLOPESCALEDEPTHBIAS and D3DRS_DEPTHBIAS
	- code is there, I think matrix offset just needs tweaking
-fix forcehardwaresync - implement texture locking for hardware that doesn't support async query
-get the format for GetAdapterModeCount and EnumAdapterModes from somewhere (shaderapidx8.cpp, GetModeCount, GetModeInfo)
-record frame sync objects (allocframesyncobjects, free framesync objects, ForceHardwareSync)
-Need to fix ENVMAPMASKSCALE, BUMPOFFSET in lightmappedgeneric*.cpp and vertexlitgeneric*.cpp
fix this:
		// FIXME: This also depends on the vertex format and whether or not we are static lit in dx9
	#ifndef SHADERAPIDX9
		if (m_DynamicState.m_VertexShader != shader) // garymcthack
	#endif // !SHADERAPIDX9
unrelated to dx9:
mat_fullbright 1 doesn't work properly on alpha materials in testroom_standards
*/

#include "shaderapidx8.h"
#include "shaderapidx8_global.h"
#include "shadershadowdx8.h"
#include "locald3dtypes.h"												   
#include "utlvector.h"
#include "IHardwareConfigInternal.h"
#include "utlstack.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi/commandbuffer.h"
#include "shaderapi/gpumemorystats.h"
#include "shaderapidx8_global.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "imaterialinternal.h"
#include "imeshdx8.h"
#include "materialsystem/imorph.h"
#include "colorformatdx8.h"
#include "texturedx8.h"
#include "textureheap.h"
#if !defined ( _PS3 )
#include <malloc.h>
#endif // !_PS3
#include "interface.h"
#include "utlrbtree.h"
#include "utlsymbol.h"
#include "tier1/strtools.h"
#include "recording.h"
#ifndef _X360
#include <crtmemdebug.h>
#endif
#include "vertexshaderdx8.h"
#include "filesystem.h"
#include "mathlib/mathlib.h"
#include "materialsystem/materialsystem_config.h"
#include "worldsize.h"
#include "TransitionTable.h"
#include "tier0/perfstats.h"
#include "tier0/vprof.h"
#include "tier1/tier1.h"
#include "tier1/utlbuffer.h"
#include "vertexdecl.h"
#include "tier0/icommandline.h"
#include "materialsystem/ishadersystem.h"
#include "tier1/convar.h"
#include "tier1/keyvalues.h"
#include "vstdlib/vstrtools.h"
#include "color.h"
#ifdef RECORDING
#include "materialsystem/IShader.h"
#endif
#include "../stdshaders/common_hlsl_cpp_consts.h" // hack hack hack!
#include "keyvalues.h"
#include "bitmap/imageformat.h"
#include "materialsystem/idebugtextureinfo.h"
#include "tier1/utllinkedlist.h"
#include "vtf/vtf.h"
#include "datacache/idatacache.h"
#include "renderparm.h"
#include "tier2/tier2.h"
#include "materialsystem/deformations.h"
#include "bitmap/tgawriter.h"

#include "itextureinternal.h"
#include "texturemanager.h"
#include "bitmap/imageformat.h"
#include "togl/rendermechanism.h"

#if defined( _X360 )
#include "xbox/xbox_console.h"
#include "xbox/xbox_vxconsole.h"
#include "xbox/xbox_win32stubs.h"
#include "xbox/xbox_launch.h"
#endif
#include "tier0/tslist.h"
#ifndef _X360
#include "wmi.h"
#endif
#include "filesystem/IQueuedLoader.h"
#include "shaderdevicedx8.h"
#include "utldict.h"

#ifdef _PS3
#include "ps3gcm\gcmdrawstate.h"
#include "ps3gcm\gcmtexture.h"
#endif

// Define this if you want to use a stubbed d3d.
//#define STUBD3D

#ifdef STUBD3D
#include "stubd3ddevice.h"
#endif

#include "winutils.h"

#ifdef _WIN32
#include "nvapi.h"
#include "NvApiDriverSettings.h"
#endif

#include "winutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _WIN32
#pragma warning (disable:4189)
#endif

#if ( ( defined( _WIN32 ) || defined( LINUX ) ) && ( !defined( DYNAMIC_SHADER_COMPILE ) ) )

ConVar mat_vtxlit_new_path( "mat_vtxlit_new_path", "1", FCVAR_DEVELOPMENTONLY );
ConVar mat_unlit_new_path( "mat_unlit_new_path", "1", FCVAR_DEVELOPMENTONLY );
ConVar mat_lmap_new_path( "mat_lmap_new_path", "1", FCVAR_DEVELOPMENTONLY );
ConVar mat_depthwrite_new_path( "mat_depthwrite_new_path", "1", FCVAR_DEVELOPMENTONLY );


CON_COMMAND( toggleVtxLitPath, "toggleVtxLitPath" )
{
	mat_vtxlit_new_path.SetValue( !mat_vtxlit_new_path.GetBool() );
	Msg( "mat_vtxlit_new_path = %s\n", mat_vtxlit_new_path.GetBool()? "TRUE":"FALSE" );
}

CON_COMMAND( toggleUnlitPath, "toggleUnlitPath" )
{
	mat_unlit_new_path.SetValue( !mat_unlit_new_path.GetBool() );
	Msg( "mat_unlit_new_path = %s\n", mat_unlit_new_path.GetBool()? "TRUE":"FALSE" );
}

CON_COMMAND( toggleLmapPath, "toggleLmapPath" )
{
	mat_lmap_new_path.SetValue( !mat_lmap_new_path.GetBool() );
	Msg( "toggleLmapPath = %s\n", mat_lmap_new_path.GetBool()? "TRUE":"FALSE" );
}

CON_COMMAND( toggleShadowPath, "Toggles CSM generation method" )
{
	mat_depthwrite_new_path.SetValue( !mat_depthwrite_new_path.GetBool() );
	Msg( "mat_depthwrite_new_path = %s\n", mat_depthwrite_new_path.GetBool()? "TRUE":"FALSE" );
}

#else

ConVar mat_vtxlit_new_path( "mat_vtxlit_new_path", "0", FCVAR_DEVELOPMENTONLY );
ConVar mat_unlit_new_path( "mat_unlit_new_path", "0", FCVAR_DEVELOPMENTONLY );
ConVar mat_lmap_new_path( "mat_lmap_new_path", "0", FCVAR_DEVELOPMENTONLY );
ConVar mat_depthwrite_new_path( "mat_depthwrite_new_path", "0", FCVAR_DEVELOPMENTONLY );

#endif

// On OSX, this does 
#ifdef _OSX
ConVar r_frameratesmoothing( "r_frameratesmoothing", "1", FCVAR_ARCHIVE, "Enable frame rate smoothing, which significantly reduces stutters but at the expense of overall frame rate." );
#else
// This convar only does something on OSX, but is referenced from Windows and Linux. Just leave a dummy here for them to find.
ConVar r_frameratesmoothing( "r_frameratesmoothing", "0", FCVAR_NONE, "", true, 0, true, 0 );
#endif


// If you need to debug refcounting issues, enable this. As of this writing, only Z side is 
// instrumented.
// #define SPEW_REFCOUNTS 1

#ifdef SPEW_REFCOUNTS
	#define SPEW_REFCOUNT_( f, l, funcname, pObj, op, expected ) \
		do { \
			if ( pObj == NULL ) { \
				Msg( "%s(%d): %s: %s == NULL\n", f, l, funcname, #pObj ); \
				break; \
			} \
			pObj->AddRef(); \
			ULONG rc = pObj->Release(); \
			Assert( rc op expected ); \
			Msg( "%s(%d): %s: ref( %s [0x%08p] ) -> %d\n", f, l, funcname, #pObj, pObj, rc ); \
		} while ( 0 )

	#define SPEW_REFCOUNT( pObj ) SPEW_REFCOUNT_( __FILE__, __LINE__, __FUNCTION__, pObj, >=, 0 ) // Unsigned, so everything is >= 0.
	#define SPEW_REFCOUNT_EXPECTED( pObj, op, expected ) SPEW_REFCOUNT_( __FILE__, __LINE__, __FUNCTION__, pObj, op, expected )
#else
	#define SPEW_REFCOUNT_( f, l, funcname, pObj, op, expected ) \
		do { \
			if ( pObj == NULL ) { \
				break; \
			} \
			pObj->AddRef(); \
			ULONG rc = pObj->Release(); \
			Assert( rc op expected ); \
		} while ( 0 )

	#define SPEW_REFCOUNT( pObj ) /* nothing if we're not spewing */
	#define SPEW_REFCOUNT_EXPECTED( pObj, op, expected ) /* nothing if we're not spewing */
#endif

ConVar mat_texture_limit( "mat_texture_limit", "-1", FCVAR_NEVER_AS_STRING, 
	"If this value is not -1, the material system will limit the amount of texture memory it uses in a frame."
	" Useful for identifying performance cliffs. The value is in kilobytes." );

// This feature is useful because world static mesh size (in vertex count) is computed while the dynamic VBs are shrunk to save memory on map load/unload.  Setting this 
// var to 1 will not shrink the VBs, which will increase the number of vertices allowed in a single batch.
ConVar mat_do_not_shrink_dynamic_vb( "mat_do_not_shrink_dynamic_vb", "0", 0, "Do not shrink the size of dynamic vertex buffers during map load/unload to save memory." );

ConVar mat_frame_sync_enable( "mat_frame_sync_enable", "1", FCVAR_CHEAT );
ConVar mat_frame_sync_force_texture( "mat_frame_sync_force_texture", "0", FCVAR_CHEAT, "Force frame syncing to lock a managed texture." );

#if defined( _X360 )
ConVar mat_x360_vblank_miss_threshold( "mat_x360_vblank_miss_threshold", "8" );
#endif

static ConVar r_pixelfog( "r_pixelfog", "1" );

ConVar r_force_first_dynamic_light_to_directional_for_csm( "r_force_first_dynamic_light_to_directional_for_csm", "1", FCVAR_CHEAT|FCVAR_DEVELOPMENTONLY, "" );

#if defined( _X360 )

ConVar mat_texturecachesize( "mat_texturecachesize", "176" );
ConVar mat_force_flush_texturecache( "mat_force_flush_texturecache", "0" );
ConVar mat_hi_z_enable( "mat_hi_z_enable", "1", FCVAR_CHEAT, "Toggle Hi-Z on the Xbox 360" );
ConVar mat_hi_stencil_enable( "mat_hi_stencil_enable", "1", FCVAR_CHEAT, "Toggle Hi-Stencil on the Xbox 360" );

#if defined( CSTRIKE15 )
static ConVar r_shader_srgbread( "r_shader_srgbread", "1", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#else
static ConVar r_shader_srgbread( "r_shader_srgbread", "0", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#endif

#endif

extern ConVar mat_debugalttab;

#define ALLOW_SMP_ACCESS 0

#if ALLOW_SMP_ACCESS
static ConVar mat_use_smp( "mat_use_smp", "0" );
#endif

// [mhansen] Enable PIX in debug and profile builds (Xbox 360 only)
// PC:    By default, PIX profiling is explicitly disallowed using the D3DPERF_SetOptions(1) API on PC
// X360:  PIX_INSTRUMENTATION will only generate PIX events in RELEASE builds on 360
// Uncomment to use PIX instrumentation:
#if defined( _X360) && (defined( PROFILE_BUILD ) || defined( _DEBUG ))
#define	PIX_INSTRUMENTATION
#endif


// Convars for driving PIX (not all hooked up yet...JasonM)
static ConVar r_pix_start( "r_pix_start", "0" );
static ConVar r_pix_recordframes( "r_pix_recordframes", "0" );

#if defined( _X360 )
#define JUNE_2009_XDK_ISSUES
#endif

//-----------------------------------------------------------------------------
// Some important enumerations
//-----------------------------------------------------------------------------
enum
{
	MAX_VERTEX_TEXTURE_COUNT = 4,
};


//-----------------------------------------------------------------------------
// These board states change with high frequency; are not shadowed
//-----------------------------------------------------------------------------
struct SamplerState_t
{
	ShaderAPITextureHandle_t	m_BoundTexture;
	D3DTEXTUREADDRESS			m_UTexWrap;
	D3DTEXTUREADDRESS			m_VTexWrap;
	D3DTEXTUREADDRESS			m_WTexWrap;
	D3DTEXTUREFILTERTYPE		m_MagFilter;
	D3DTEXTUREFILTERTYPE		m_MinFilter;
	D3DTEXTUREFILTERTYPE		m_MipFilter;
	TextureBindFlags_t			m_nTextureBindFlags;
	int							m_nAnisotropicLevel;
	uint					    m_bShadowFilterEnable;
	bool						m_TextureEnable;
};


//-----------------------------------------------------------------------------
// State related to vertex textures
//-----------------------------------------------------------------------------
struct VertexTextureState_t
{
	ShaderAPITextureHandle_t	m_BoundVertexTexture;
	D3DTEXTUREADDRESS			m_UTexWrap;
	D3DTEXTUREADDRESS			m_VTexWrap;
	D3DTEXTUREFILTERTYPE		m_MagFilter;
	D3DTEXTUREFILTERTYPE		m_MinFilter;
	D3DTEXTUREFILTERTYPE		m_MipFilter;
};


enum TransformType_t
{
	TRANSFORM_IS_IDENTITY = 0,
	TRANSFORM_IS_CAMERA_TO_WORLD,
	TRANSFORM_IS_GENERAL,
};

enum TransformDirtyBits_t
{
	STATE_CHANGED_VERTEX_SHADER = 0x1,
	STATE_CHANGED = 0x1
};

struct UberlightRenderState_t
{
	Vector4D m_vSmoothEdge0;
	Vector4D m_vSmoothEdge1;
	Vector4D m_vSmoothOneOverW;
	Vector4D m_vShearRound;
	Vector4D m_vaAbB;
	VMatrix m_WorldToLight;
};

struct DepthBiasState_t
{
	float m_flOOSlopeScaleDepthBias;
	float m_flOODepthBias;
};


enum
{
#if !defined( _X360 )
	MAX_NUM_RENDERSTATES = ( D3DRS_BLENDOPALPHA+1 ),
#else
	MAX_NUM_RENDERSTATES = D3DRS_MAX,                 
#endif
//	MORPH_TARGET_FACTOR_COUNT = VERTEX_SHADER_MORPH_TARGET_FACTOR_COUNT * 4,
};

struct DynamicState_t
{
	// Viewport state
	D3DVIEWPORT9	m_Viewport;

	// Transform state
	D3DXMATRIX		m_Transform[NUM_MATRIX_MODES];
	unsigned char	m_TransformType[NUM_MATRIX_MODES];
	unsigned char	m_TransformChanged[NUM_MATRIX_MODES];

	InstanceInfo_t				m_InstanceInfo;
	CompiledLightingState_t		m_CompiledLightingState;
	MaterialLightingState_t		m_LightingState;

	// Used for GetDx9LightState
	mutable LightState_t		m_ShaderLightState;
	mutable bool				m_bLightStateComputed;

	// Used to deal with env cubemaps
	int							m_nLocalEnvCubemapSamplers;

	// Used to deal with lightmaps
	int							m_nLightmapSamplers;
	
	int							m_nPaintmapSamplers;
	
	// Shade mode
	D3DSHADEMODE	m_ShadeMode;

	// Clear color
	D3DCOLOR		m_ClearColor;

	// Fog
	D3DCOLOR		m_FogColor;
	Vector4D	m_vecPixelFogColor;
	Vector4D	m_vecPixelFogColorLinear;
	bool			m_bFogGammaCorrectionDisabled;
	bool			m_FogEnable;
	D3DFOGMODE		m_FogMode;
	float			m_FogStart;
	float			m_FogEnd;
	float			m_FogZ;
	float			m_FogMaxDensity;

	float			m_HeightClipZ;
	MaterialHeightClipMode_t m_HeightClipMode;	
	
	// user clip planes
	int				m_UserClipPlaneEnabled;
	int				m_UserClipPlaneChanged;
	D3DXPLANE		m_UserClipPlaneWorld[MAXUSERCLIPPLANES];
	D3DXPLANE		m_UserClipPlaneProj[MAXUSERCLIPPLANES];
	bool			m_UserClipLastUpdatedUsingFixedFunction;

	bool			m_FastClipEnabled;
	bool			m_bFastClipPlaneChanged;
	D3DXPLANE		m_FastClipPlane;

	// Used when overriding the user clip plane
	bool			m_bUserClipTransformOverride;
	D3DXMATRIX		m_UserClipTransform;

	// Cull mode
	D3DCULL			m_DesiredCullMode;
	D3DCULL			m_CullMode;
	bool			m_bCullEnabled;
	
	// Skinning
	int				m_NumBones;

	// Pixel and vertex shader constants...
	Vector4D*		m_pVectorVertexShaderConstant;
	BOOL*			m_pBooleanVertexShaderConstant;
	IntVector4D*	m_pIntegerVertexShaderConstant;
	Vector4D*		m_pVectorPixelShaderConstant;
	BOOL*			m_pBooleanPixelShaderConstant;
	IntVector4D*	m_pIntegerPixelShaderConstant;

	// Texture stage state
	SamplerState_t m_SamplerState[MAX_SAMPLERS];

	// Vertex texture stage state
	VertexTextureState_t m_VertexTextureState[MAX_VERTEX_TEXTURE_COUNT];

	DWORD m_RenderState[MAX_NUM_RENDERSTATES];

	RECT  m_ScissorRect;

	IDirect3DVertexDeclaration9 *m_pVertexDecl;
	VertexFormat_t m_DeclVertexFormat;
	bool m_bDeclHasColorMesh;
	bool m_bDeclUsingFlex;
	bool m_bDeclUsingMorph;
	bool m_bDeclUsingPreTessPatch;

	bool	m_bSRGBWritesEnabled;
	bool	m_bHWMorphingEnabled;

#if defined( _X360 )
	int		m_iVertexShaderGPRAllocation; //only need to track vertex shader
	bool	m_bBuffer2Frames;
#endif

	TessellationMode_t	m_TessellationMode;

	DynamicState_t() {}

private:
	DynamicState_t( DynamicState_t const& );

	DISALLOW_OPERATOR_EQUAL( DynamicState_t );
};

//-----------------------------------------------------------------------------
// Method to queue up dirty dynamic state change calls
//-----------------------------------------------------------------------------
typedef void (*StateCommitFunc_t)( D3DDeviceWrapper *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce );
static void CommitSetViewports( D3DDeviceWrapper *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce );

// NOTE: It's slightly memory inefficient, and definitely not typesafe,
// to put all commit funcs into the same table (vs, per-draw, per-pass),
// but it makes the code a heck of a lot simpler and smaller.
enum CommitFunc_t
{
	COMMIT_FUNC_CommitVertexTextures = 0,
	COMMIT_FUNC_CommitFlexWeights,
	COMMIT_FUNC_CommitSetScissorRect,
	COMMIT_FUNC_CommitSetViewports,

#if defined( _X360 )
	COMMIT_FUNC_CommitShaderGPRs,
#endif

	COMMIT_FUNC_COUNT,
	COMMIT_FUNC_BYTE_COUNT = ( COMMIT_FUNC_COUNT + 0x7 ) >> 3,
};

enum CommitFuncType_t
{
	COMMIT_PER_DRAW = 0,
	COMMIT_PER_PASS,

	COMMIT_FUNC_TYPE_COUNT,
};


#define ADD_COMMIT_FUNC( _func, _func_name )	\
	if ( !IsCommitFuncInUse( _func, COMMIT_FUNC_ ## _func_name ) )	\
	{																		\
		AddCommitFunc( _func, _func_name );						\
		MarkCommitFuncInUse( _func, COMMIT_FUNC_ ## _func_name );	\
	}

#define ADD_RENDERSTATE_FUNC( _func, _func_name, _state, _val )	\
	if ( m_bResettingRenderState || (m_DesiredState._state != _val) )	\
	{																		\
		m_DesiredState._state = _val;									\
		ADD_COMMIT_FUNC( _func, _func_name )						\
	}

#define ADD_VERTEX_TEXTURE_FUNC( _func, _func_name, _stage, _state, _val )	\
	Assert( _stage < MAX_VERTEX_TEXTURE_COUNT );							\
	if ( m_bResettingRenderState || (m_DesiredState.m_VertexTextureState[_stage]._state != _val) )	\
	{																		\
		m_DesiredState.m_VertexTextureState[_stage]._state = _val;		\
		ADD_COMMIT_FUNC( _func, _func_name )						\
	}

#if defined( _X360 )
static void CommitShaderGPRs( D3DDeviceWrapper *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce );
#endif

//-----------------------------------------------------------------------------
// Check render state support at compile time instead of runtime
//-----------------------------------------------------------------------------
#define SetSupportedRenderState( _state, _val )										\
	{																				\
		if( _state != D3DRS_NOTSUPPORTED )											\
		{																			\
			SetRenderState( _state, _val );											\
		}																			\
	}

#define SetSupportedRenderStateForce( _state, _val )								\
	{																				\
		if( _state != D3DRS_NOTSUPPORTED )											\
		{																			\
			SetRenderStateForce( _state, _val );									\
		}																			\
	}

//-----------------------------------------------------------------------------
// Allocated textures
//-----------------------------------------------------------------------------
struct Texture_t
{
	Texture_t()
	{
		m_Flags = 0;
		m_Count = 1;
		m_CountIndex = 0;
		m_nTimesBoundMax = 0;
		m_nTimesBoundThisFrame = 0;
		m_pTexture = NULL;
		m_ppTexture = NULL;
		m_ImageFormat = IMAGE_FORMAT_RGBA8888;
		m_pTextureGroupCounterGlobal = NULL;
		m_pTextureGroupCounterFrame = NULL;
	}

	uint8					m_UTexWrap;
	uint8					m_VTexWrap;
	uint8					m_WTexWrap;

	uint8					m_MagFilter;
	uint8					m_MinFilter;
	uint8					m_MipFilter;

	uint8					m_NumLevels;
	uint8					m_SwitchNeeded;	// Do we need to advance the current copy?
	uint8					m_NumCopies;	// copies are used to optimize procedural textures
	uint8					m_CurrentCopy;	// the current copy we're using...

private:
	union
	{
		IDirect3DBaseTexture	*m_pTexture;				// used when there's one copy
		IDirect3DBaseTexture	**m_ppTexture;				// used when there are more than one copies
		IDirect3DSurface		*m_pDepthStencilSurface;	// used when there's one depth stencil surface
		IDirect3DSurface		*m_pRenderTargetSurface[2];	// 360: regular+sRGB views onto an EDRAM surface
	};

	ImageFormat m_ImageFormat;

public:
	int						m_CreationFlags;

	CUtlSymbol				m_DebugName;
	CUtlSymbol				m_TextureGroupName;
	int						*m_pTextureGroupCounterGlobal;	// Global counter for this texture's group.
	int						*m_pTextureGroupCounterFrame;	// Per-frame global counter for this texture's group.

	// stats stuff
	int						m_SizeBytes;
	int						m_SizeTexels;
	int						m_LastBoundFrame;
	int						m_nTimesBoundMax;
	int						m_nTimesBoundThisFrame;

	enum Flags_t
	{
		IS_ALLOCATED             = 0x0001,
		IS_DEPTH_STENCIL         = 0x0002,
		IS_DEPTH_STENCIL_TEXTURE = 0x0004,	// depth stencil texture, not surface
		IS_RENDERABLE            = ( IS_DEPTH_STENCIL | IS_ALLOCATED ),
		IS_FINALIZED             = 0x0010,	// CONSOLE: completed async hi-res load
		IS_ERROR_TEXTURE         = 0x0020,	// CONSOLE:	failed during load
		CAN_CONVERT_FORMAT       = 0x0040,	// 360:		allow format conversion
		IS_LINEAR                = 0x0080,	// 360:		unswizzled linear format
		IS_RENDER_TARGET         = 0x0100,	// CONSOLE:	marks a render target texture source
		IS_RENDER_TARGET_SURFACE = 0x0200,	// 360:		marks a render target surface target
		IS_VERTEX_TEXTURE		 = 0x0800,
		IS_PWL_CORRECTED         = 0x1000,	// 360:		data is pwl corrected
		IS_CACHEABLE             = 0x2000,	// 360:		texture is subject to cache discard
	};

	short					m_Width;
	short					m_Height;
	short					m_Depth;
	unsigned short			m_Flags;

	typedef IDirect3DBaseTexture	*IDirect3DBaseTexturePtr;
	typedef IDirect3DBaseTexture	**IDirect3DBaseTexturePtrPtr;
	typedef IDirect3DSurface		*IDirect3DSurfacePtr;

	IDirect3DBaseTexturePtr GetTexture( void )
	{
		Assert( m_NumCopies == 1 );
		Assert( !( m_Flags & IS_DEPTH_STENCIL ) );
		return m_pTexture;
	}
	IDirect3DBaseTexturePtr GetTexture( int copy )
	{
		Assert( m_NumCopies > 1 );
		Assert( !( m_Flags & IS_DEPTH_STENCIL ) );
		return m_ppTexture[copy];
	}
	IDirect3DBaseTexturePtrPtr &GetTextureArray( void )
	{
		Assert( m_NumCopies > 1 );
		Assert( !( m_Flags & IS_DEPTH_STENCIL ) );
		return m_ppTexture;
	}

	IDirect3DSurfacePtr &GetDepthStencilSurface( void )
	{
		Assert( m_NumCopies == 1 );
		Assert( (m_Flags & IS_DEPTH_STENCIL) );
		return m_pDepthStencilSurface;
	}

	IDirect3DSurfacePtr &GetRenderTargetSurface( bool bSRGB )
	{
		Assert( IsX360() );
		Assert( m_NumCopies == 1 );
		Assert( m_Flags & IS_RENDER_TARGET_SURFACE );
		return m_pRenderTargetSurface[bSRGB?1:0];
	}

	void SetTexture( IDirect3DBaseTexturePtr pPtr )
	{
		m_pTexture = pPtr;
	}
	void SetTexture( int copy, IDirect3DBaseTexturePtr pPtr )
	{
		m_ppTexture[copy] = pPtr;
	}

	int GetMemUsage() const
	{
		return m_SizeBytes;
	}

	int GetWidth() const
	{
		return ( int )m_Width;
	}

	int GetHeight() const
	{
		return ( int )m_Height;
	}

	int GetDepth() const
	{
		return ( int )m_Depth;
	}

	void SetImageFormat( ImageFormat format )
	{
		m_ImageFormat = format;
	}
	ImageFormat GetImageFormat() const
	{
		return m_ImageFormat;
	}

public:
	short m_Count;
	short m_CountIndex;

	short GetCount() const
	{
		return m_Count;
	}
};

#define MAX_DEFORMATION_PARAMETERS 16
#define DEFORMATION_STACK_DEPTH 10

struct Deformation_t
{
	int m_nDeformationType;
	int m_nNumParameters;
	float m_flDeformationParameters[MAX_DEFORMATION_PARAMETERS];
};

// These represent the three different methods that we apply fog in the shader.
enum FogMethod_t
{
	FOGMETHOD_FIXEDFUNCTIONVERTEXFOG = 0,
	FOGMETHOD_VERTEXFOGBLENDEDINPIXELSHADER,
	FOGMETHOD_PIXELSHADERFOG,
	FOGMETHOD_NUMVALUES
};

//-----------------------------------------------------------------------------
// The DX8 implementation of the shader API
//-----------------------------------------------------------------------------
class CShaderAPIDx8 : public CShaderDeviceDx8, public IShaderAPIDX8, public IDebugTextureInfo
{
	typedef CShaderDeviceDx8 BaseClass;

public:
	// constructor, destructor
	CShaderAPIDx8( );
	virtual ~CShaderAPIDx8();

	// Methods of IShaderAPI
public:
	virtual void SetViewports( int nCount, const ShaderViewport_t* pViewports, bool setImmediately = false );
	virtual int  GetViewports( ShaderViewport_t* pViewports, int nMax ) const;
	virtual void ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight );
	virtual void ClearColor3ub( unsigned char r, unsigned char g, unsigned char b );
	virtual void ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a );
	virtual void BindVertexShader( VertexShaderHandle_t hVertexShader );
	virtual void BindGeometryShader( GeometryShaderHandle_t hGeometryShader );
	virtual void BindPixelShader( PixelShaderHandle_t hPixelShader );
	virtual void SetRasterState( const ShaderRasterState_t& state );
	virtual void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights );
	virtual void OnPresent( void );
#ifdef _GAMECONSOLE
	// Backdoor used by the queued context to directly use write-combined memory
	virtual IMesh *GetExternalMesh( const ExternalMeshInfo_t& info );
	virtual void SetExternalMeshData( IMesh *pMesh, const ExternalMeshData_t &data );
	virtual IIndexBuffer *GetExternalIndexBuffer( int nIndexCount, uint16 *pIndexData );
	virtual void FlushGPUCache( void *pBaseAddr, size_t nSizeInBytes );
#endif

	// Methods of IShaderDynamicAPI
public:
	virtual void GetBackBufferDimensions( int &nWidth, int &nHeight ) const
	{
		// Chain to the device
		BaseClass::GetBackBufferDimensions( nWidth, nHeight );
	}

	virtual const AspectRatioInfo_t &GetAspectRatioInfo( void ) const
	{
		// Chain to the device
		return BaseClass::GetAspectRatioInfo();
	}

	// Get the dimensions of the current render target
	virtual void GetCurrentRenderTargetDimensions( int& nWidth, int& nHeight ) const
	{
		ITexture *pTexture = ShaderAPI()->GetRenderTargetEx( 0 );
		if ( pTexture == NULL )
		{
			ShaderAPI()->GetBackBufferDimensions( nWidth, nHeight );
		}
		else
		{
			nWidth  = pTexture->GetActualWidth();
			nHeight = pTexture->GetActualHeight();
		}
	}

	// Get the current viewport
	virtual void GetCurrentViewport( int& nX, int& nY, int& nWidth, int& nHeight ) const
	{
		ShaderViewport_t viewport;
		ShaderAPI()->GetViewports( &viewport, 1 );
		nX = viewport.m_nTopLeftX;
		nY = viewport.m_nTopLeftY;
		nWidth = viewport.m_nWidth;
		nHeight = viewport.m_nHeight;
	}

	virtual void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords );
	virtual void SetScreenSizeForVPOS( int pshReg = 32 );
	virtual void SetVSNearAndFarZ( int vshReg );
	virtual float GetFarZ();

	virtual void EnableSinglePassFlashlightMode( bool bEnable );
	virtual bool SinglePassFlashlightModeEnabled( void );

public:
	// Methods of CShaderAPIBase
	virtual bool OnDeviceInit();
	virtual void OnDeviceShutdown();
	virtual void ReleaseShaderObjects( bool bReleaseManagedResources = true );
	virtual void RestoreShaderObjects();
	virtual void BeginPIXEvent( unsigned long color, const char *szName );
	virtual void EndPIXEvent();
	virtual void AdvancePIXFrame();
	virtual void NotifyShaderConstantsChangedInRenderPass();
	virtual void GenerateNonInstanceRenderState( MeshInstanceData_t *pInstance, CompiledLightingState_t** pCompiledState, InstanceInfo_t **pInstanceInfo );

public:
	// Methods of IShaderAPIDX8
	virtual void QueueResetRenderState();

	//
	// Abandon all hope ye who pass below this line which hasn't been ported.
	//

	// Sets the mode...
	bool SetMode( void* VD3DHWND, int nAdapter, const ShaderDeviceInfo_t &info );

	// Change the video mode after it's already been set.
	void ChangeVideoMode( const ShaderDeviceInfo_t &info );

	// Sets the default render state
	void SetDefaultState();

	// Methods to ask about particular state snapshots
	virtual bool IsTranslucent( StateSnapshot_t id ) const;
	virtual bool IsAlphaTested( StateSnapshot_t id ) const;
	virtual bool UsesVertexAndPixelShaders( StateSnapshot_t id ) const;
	virtual int CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 );

	// Computes the vertex format for a particular set of snapshot ids
	VertexFormat_t ComputeVertexFormat( int num, StateSnapshot_t* pIds ) const;
	VertexFormat_t ComputeVertexUsage( int num, StateSnapshot_t* pIds ) const;

	// Uses a state snapshot
	void UseSnapshot( StateSnapshot_t snapshot );

	// Set the number of bone weights
	virtual void SetNumBoneWeights( int numBones );
	virtual void EnableHWMorphing( bool bEnable );

	// Sets the vertex and pixel shaders
	virtual void SetVertexShaderIndex( int vshIndex = -1 );
	virtual void SetPixelShaderIndex( int pshIndex = 0 );

	// Matrix state
	void MatrixMode( MaterialMatrixMode_t matrixMode );
	void PushMatrix();
	void PopMatrix();
	void LoadMatrix( float *m );
	void LoadBoneMatrix( int boneIndex, const float *m );
	void MultMatrix( float *m );
	void MultMatrixLocal( float *m );
	void GetMatrix( MaterialMatrixMode_t matrixMode, float *dst );
	void LoadIdentity( void );
	void LoadCameraToWorld( void );
	void Ortho( double left, double top, double right, double bottom, double zNear, double zFar );
	void PerspectiveX( double fovx, double aspect, double zNear, double zFar );
	void PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right );
	void PickMatrix( int x, int y, int width, int height );
	void Rotate( float angle, float x, float y, float z );
	void Translate( float x, float y, float z );
	void Scale( float x, float y, float z );
	void ScaleXY( float x, float y );

	// Binds a particular material to render with
	void Bind( IMaterial* pMaterial );
	IMaterialInternal* GetBoundMaterial();

	// Level of anisotropic filtering
	virtual void SetAnisotropicLevel( int nAnisotropyLevel );

	virtual void	SyncToken( const char *pToken );

	void CullMode( MaterialCullMode_t cullMode );
	void FlipCullMode( void );

	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
	void ForceDepthFuncEquals( bool bEnable );

	// Turns off Z buffering
	void OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable = true );

	void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable );
	void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable );

	void SetHeightClipZ( float z );
	void SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode );

	void SetClipPlane( int index, const float *pPlane );
	void EnableClipPlane( int index, bool bEnable );
	
	void SetFastClipPlane(const float *pPlane);
	void EnableFastClip(bool bEnable);
	
	// The shade mode
	void ShadeMode( ShaderShadeMode_t mode );

	// Gets the dynamic mesh
	IMesh* GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered,
		IMesh* pVertexOverride, IMesh* pIndexOverride );
	IMesh* GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, 
		bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride );
	IMesh *GetFlexMesh();

	// Returns the number of vertices we can render using the dynamic mesh
	virtual void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices );
	virtual int GetMaxVerticesToRender( IMaterial *pMaterial );
	virtual int GetMaxIndicesToRender( );

	// Draws the mesh
	void DrawMesh( CMeshBase* mesh, int nCount, const MeshInstanceData_t *pInstances, VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, InstanceInfo_t *pInfo );
	void DrawMesh2( CMeshBase* mesh, int nCount, const MeshInstanceData_t *pInstances, VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, InstanceInfo_t *pInfo );
	void DrawShadowMesh( CMeshBase* mesh, int nCount, const MeshInstanceData_t *pInstances, VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, InstanceInfo_t *pInfo );
	void DrawMeshInternal( CMeshBase* mesh, int nCount, const MeshInstanceData_t *pInstances, VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, InstanceInfo_t *pInfo );

	// Draws
	void BeginPass( StateSnapshot_t snapshot  );
	void RenderPass( const unsigned char *pInstanceCommandBuffer, int nPass, int nPassCount );

	// We use smaller dynamic VBs during level transitions, to free up memory
	virtual int  GetCurrentDynamicVBSize( void );
	virtual void DestroyVertexBuffers( bool bExitingLevel = false );

	void SetVertexDecl( VertexFormat_t vertexFormat, bool bHasColorMesh, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch, VertexStreamSpec_t *pStreamSpec );

	void SetTessellationMode( TessellationMode_t mode );

	// Sets the constant register for vertex and pixel shaders
	FORCEINLINE void SetVertexShaderConstantInternal( int var, float const* pVec, int numVecs = 1, bool bForce = false );

	void SetVertexShaderConstant( int var, float const* pVec, int numVecs = 1, bool bForce = false );
	void SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false );
	void SetIntegerVertexShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false );
	
	void SetPixelShaderConstant( int var, float const* pVec, int numVecs = 1, bool bForce = false );
#ifdef _PS3
	FORCEINLINE void SetPixelShaderConstantInternal( int var, float const* pValues, int nNumConsts = 1, bool bForce = false )
	{
		Dx9Device()->SetPixelShaderConstantF( var, pValues, nNumConsts );
	}
#else
	FORCEINLINE void SetPixelShaderConstantInternal( int var, float const* pValues, int nNumConsts = 1, bool bForce = false );
#endif

	void SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false );
	void SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false );

	void InvalidateDelayedShaderConstants( void );

	// Returns the nearest supported format
	ImageFormat GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired = true ) const;
	ImageFormat GetNearestRenderTargetFormat( ImageFormat format ) const;
	virtual bool DoRenderTargetsNeedSeparateDepthBuffer() const;

	// stuff that shouldn't be used from within a shader 
    void ModifyTexture( ShaderAPITextureHandle_t textureHandle );
	void BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle );
	void BindVertexTexture( VertexTextureSampler_t nStage, ShaderAPITextureHandle_t textureHandle );
	void DeleteTexture( ShaderAPITextureHandle_t textureHandle );

	void WriteTextureToFile( ShaderAPITextureHandle_t hTexture, const char *szFileName );

	bool IsTexture( ShaderAPITextureHandle_t textureHandle );
	bool IsTextureResident( ShaderAPITextureHandle_t textureHandle );
	FORCEINLINE bool TextureIsAllocated( ShaderAPITextureHandle_t hTexture )
	{
		return m_Textures.IsValidIndex( hTexture ) && ( GetTexture( hTexture ).m_Flags & Texture_t::IS_ALLOCATED );
	}
	FORCEINLINE void AssertValidTextureHandle( ShaderAPITextureHandle_t textureHandle )
	{
#ifdef _DEBUG
		Assert( TextureIsAllocated( textureHandle ) );
#endif
	}

	// Lets the shader know about the full-screen texture so it can 
	virtual void SetFullScreenTextureHandle( ShaderAPITextureHandle_t h );

	virtual void SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture );

	// Set the render target to a texID.
	// Set to SHADER_RENDERTARGET_BACKBUFFER if you want to use the regular framebuffer.
	void SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER, 
		ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER );
	// Set the render target to a texID.
	// Set to SHADER_RENDERTARGET_BACKBUFFER if you want to use the regular framebuffer.
	void SetRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER, 
		ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER );

	// These are bound to the texture, not the texture environment
	void TexMinFilter( ShaderTexFilterMode_t texFilterMode );
	void TexMagFilter( ShaderTexFilterMode_t texFilterMode );
	void TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode );
	void TexSetPriority( int priority );

	ShaderAPITextureHandle_t CreateTextureHandle( void );
	void CreateTextureHandles( ShaderAPITextureHandle_t *handles, int count, bool bReuseHandles );

	ShaderAPITextureHandle_t CreateTexture( 
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int creationFlags, 
		const char *pDebugName,
		const char *pTextureGroupName );

	// Create a multi-frame texture (equivalent to calling "CreateTexture" multiple times, but more efficient)
	void CreateTextures( 
		ShaderAPITextureHandle_t *pHandles,
		int count,
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int flags, 
		const char *pDebugName,
		const char *pTextureGroupName );

	ShaderAPITextureHandle_t CreateDepthTexture( 
		ImageFormat renderTargetFormat, 
		int width, 
		int height, 
		const char *pDebugName,
		bool bTexture,
		bool bAliasDepthSurfaceOverColorX360 = false );

	void TexImage2D( 
		int level, 
		int cubeFaceID, 
		ImageFormat dstFormat, 
		int zOffset, 
		int width, 
		int height, 
		ImageFormat srcFormat, 
		bool bSrcIsTiled,
		void *imageData );

	void TexSubImage2D( 
		int level, 
		int cubeFaceID, 
		int xOffset, 
		int yOffset, 
		int zOffset, 
		int width, 
		int height,
		ImageFormat srcFormat, 
		int srcStride, 
		bool bSrcIsTiled,
		void *imageData );

	bool TexLock( int level, int cubeFaceID, int xOffset, int yOffset, int width, int height, CPixelWriter& writer );
	void TexUnlock( );

	// Copy sysmem surface to default pool surface asynchronously
	void UpdateTexture( int xOffset, int yOffset, int w, int h, ShaderAPITextureHandle_t hDstTexture, ShaderAPITextureHandle_t hSrcTexture );

	void *LockTex( ShaderAPITextureHandle_t hTexture );
	void UnlockTex( ShaderAPITextureHandle_t hTexture );

	void UpdateDepthBiasState();

	// stuff that isn't to be used from within a shader
	// what's the best way to hide this? subclassing?
	virtual void ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth );
	virtual void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth );
	virtual void PerformFullScreenStencilOperation( void );
	void ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = NULL );
	void ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = NULL, CThreadEvent *pPixelsReadEvent = NULL );
	void ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent = NULL );
	virtual void ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride );

	// Gets the current buffered state... (debug only)
	void GetBufferedState( BufferedState_t& state );

	// Make sure we finish drawing everything that has been requested 
	void FlushHardware();
	
	// Use this to begin and end the frame
	void BeginFrame();
	void EndFrame();

	// Used to clear the transition table when we know it's become invalid.
	void ClearSnapshots();

	// returns the D3D interfaces....
#if !defined( _GAMECONSOLE )
	FORCEINLINE D3DDeviceWrapper *Dx9Device() const
	{ 
		return (D3DDeviceWrapper*)&(m_DeviceWrapper);
	}
#endif

	// Backward compat
	virtual int GetActualSamplerCount() const;
	virtual int StencilBufferBits() const;
	virtual bool IsAAEnabled() const;	// Is antialiasing being used?
	virtual bool OnAdapterSet( );
	bool m_bAdapterSet;

	void UpdateFastClipUserClipPlane( void );
	bool ReadPixelsFromFrontBuffer() const;

	// returns the current time in seconds....
	double CurrentTime() const;

	// Get the current camera position in world space.
	void GetWorldSpaceCameraPosition( float* pPos ) const;
	void GetWorldSpaceCameraDirection( float* pDir ) const;

	// Fog methods
	void EnableFixedFunctionFog( bool bFogEnable );
	void FogStart( float fStart );
	void FogEnd( float fEnd );
	void FogMaxDensity( float flMaxDensity );
	void SetFogZ( float fogZ );
	void GetFogDistances( float *fStart, float *fEnd, float *fFogZ );

	void SceneFogMode( MaterialFogMode_t fogMode );
	MaterialFogMode_t GetSceneFogMode( );
	int GetPixelFogCombo( );//0 is either range fog, or no fog simulated with rigged range fog values. 1 is height fog
	void SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void GetSceneFogColor( unsigned char *rgb );
	void GetSceneFogColor( unsigned char *r, unsigned char *g, unsigned char *b );


	/// update the array of precalculated color values when state changes.
	void RegenerateFogConstants( void );

	Vector CalculateFogColorConstant( D3DCOLOR *pPackedColorOut, FogMethod_t fogMethod, ShaderFogMode_t fogMode, 
									  bool bDisableFogGammaCorrection, bool bSRGBWritesEnabled );

	
	// Selection mode methods
	int  SelectionMode( bool selectionMode );
	void SelectionBuffer( unsigned int* pBuffer, int size );
	void ClearSelectionNames( );
	void LoadSelectionName( int name );
	void PushSelectionName( int name );
	void PopSelectionName();
	bool IsInSelectionMode() const;
	void RegisterSelectionHit( float minz, float maxz );
	void WriteHitRecord();

	// Binds a standard texture
	virtual void BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id );
	virtual void BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id );
	virtual void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id );

	virtual float GetSubDHeight();

	virtual bool IsStereoActiveThisFrame() const;
	bool m_bIsStereoActiveThisFrame;
	
	// Gets the lightmap dimensions
	virtual void GetLightmapDimensions( int *w, int *h );

	// Use this to get the mesh builder that allows us to modify vertex data
	CMeshBuilder* GetVertexModifyBuilder();

	virtual bool InFlashlightMode() const;
	virtual bool IsRenderingPaint() const;
	virtual bool InEditorMode() const;

	// Helper to get at the texture state stage
	SamplerState_t& SamplerState( int nSampler ) { return m_DynamicState.m_SamplerState[nSampler]; }
	const SamplerState_t& SamplerState( int nSampler ) const { return m_DynamicState.m_SamplerState[nSampler]; }
	TextureBindFlags_t LastSetTextureBindFlags( int nSampler ) const { return SamplerState( nSampler ).m_nTextureBindFlags; }
	void SetLastSetTextureBindFlags( int nSampler, TextureBindFlags_t nBindFlags ) { SamplerState( nSampler ).m_nTextureBindFlags = nBindFlags; }

	virtual void SetLightingState( const MaterialLightingState_t& state );
	void SetLights( int nCount, const LightDesc_t *pDesc );
	void SetLightingOrigin( Vector vLightingOrigin );
	void DisableAllLocalLights();
	void SetAmbientLightCube( Vector4D colors[6] );
	float GetAmbientLightCubeLuminance( MaterialLightingState_t *pLightingState );

	void SetVertexShaderStateAmbientLightCube( int nVSReg, CompiledLightingState_t *pLightingState );
	void SetPixelShaderStateAmbientLightCube( int pshReg, CompiledLightingState_t *pLightingState );

	// Methods related to compiling lighting state for use in shaderes
	void CompileAmbientCube( CompiledLightingState_t *pCompiledState, int nLightCount, const MaterialLightingState_t *pLightingState );
	void CompileVertexShaderLocalLights( CompiledLightingState_t *pCompiledState, int nLightCount, const MaterialLightingState_t *pLightingState, bool bStaticLight );
	void CompilePixelShaderLocalLights( CompiledLightingState_t *pCompiledState, int nLightCount, const MaterialLightingState_t *pLightingState, bool bStaticLight );

	void CopyRenderTargetToTexture( ShaderAPITextureHandle_t textureHandle );
	void CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t textureHandle, int nRenderTargetID, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL );
	void CopyTextureToRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL );

	// Returns the cull mode (for fill rate computation)
	D3DCULL GetCullMode() const;
	void SetCullModeState( bool bEnable, D3DCULL nDesiredCullMode );
	void ApplyCullEnable( bool bEnable );
	void FlipCulling( bool bFlipCulling );

	// Alpha to coverage
	void ApplyAlphaToCoverage( bool bEnable );

	// Applies Z Bias
	void ApplyZBias( const DepthTestState_t &shaderState );

	// Applies texture enable
	void ApplyTextureEnable( const ShadowState_t& state, int stage );

	void ApplyFogMode( ShaderFogMode_t fogMode, bool bVertexFog, bool bSRGBWritesEnabled, bool bDisableFogGammaCorrection );

	void UpdatePixelFogColorConstant( bool bMultiplyByToneMapScale = true );

	void EnabledSRGBWrite( bool bEnabled );
	void SetSRGBWrite( bool bState );

	// Gamma<->Linear conversions according to the video hardware we're running on
	float GammaToLinear_HardwareSpecific( float fGamma ) const;
	float LinearToGamma_HardwareSpecific( float fLinear ) const;

	// Applies alpha blending
	void ApplyAlphaBlend( bool bEnable, D3DBLEND srcBlend, D3DBLEND destBlend );

	// Sets texture stage stage + render stage state
	void SetSamplerState( int stage, D3DSAMPLERSTATETYPE state, DWORD val );
	void SetRenderStateForce( D3DRENDERSTATETYPE state, DWORD val );
	void SetRenderState( D3DRENDERSTATETYPE state, DWORD val );

	// Scissor Rect
	void SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor );
	// Can we download textures?
	virtual bool CanDownloadTextures() const;

	void ForceHardwareSync_WithManagedTexture();
	void ForceHardwareSync( void );
	void UpdateFrameSyncQuery( int queryIndex, bool bIssue );

	void EvictManagedResources();

	// Get stats on GPU memory usage
	virtual void GetGPUMemoryStats( GPUMemoryStats &stats );

	virtual void EvictManagedResourcesInternal();

	// Gets at a particular transform
	inline D3DXMATRIX& GetTransform( int i )
	{
		return *m_pMatrixStack[i]->GetTop();
	}

	int GetCurrentNumBones( void ) const;
	bool IsHWMorphingEnabled( ) const;
	TessellationMode_t GetTessellationMode( ) const;
	void GetDX9LightState( LightState_t *state ) const;	// Used for DX9 only

	MaterialFogMode_t GetCurrentFogType( void ) const; // deprecated. don't use.

	void RecordString( const char *pStr );

	virtual bool IsRenderingMesh() const { return m_nRenderInstanceCount != 0; }

	int GetCurrentFrameCounter( void ) const
	{
		return m_CurrentFrame;
	}
 
	// Workaround hack for visualization of selection mode
	virtual void SetupSelectionModeVisualizationState();

	// Allocate and delete query objects.
	virtual ShaderAPIOcclusionQuery_t CreateOcclusionQueryObject( void );
	virtual void DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t h );

	// Bracket drawing with begin and end so that we can get counts next frame.
	virtual void BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t h );
	virtual void EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t h );

	// Get the number of pixels rendered between begin and end on an earlier frame.
	// Calling this in the same frame is a huge perf hit!
	virtual int OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t h, bool bFlush );

	void SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture );
	void SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture );
	const FlashlightState_t &GetFlashlightState( VMatrix &worldToTexture ) const;
	const FlashlightState_t &GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **pFlashlightDepthTexture ) const;
	virtual void GetFlashlightShaderInfo( bool *pShadowsEnabled, bool *pUberLight ) const;
	virtual float GetFlashlightAmbientOcclusion( ) const;
			
	virtual bool IsCascadedShadowMapping() const;
	virtual void SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas );
	virtual const CascadedShadowMappingState_t &GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas, bool bLightMapScale = false ) const;
				
	// Gets at the shadow state for a particular state snapshot
	virtual bool IsDepthWriteEnabled( StateSnapshot_t id ) const;

// IDebugTextureInfo implementation.

	virtual bool IsDebugTextureListFresh( int numFramesAllowed = 1 );
	virtual void EnableDebugTextureList( bool bEnable );
	virtual bool SetDebugTextureRendering( bool bEnable );
	virtual void EnableGetAllTextures( bool bEnable );
	virtual KeyValues* LockDebugTextureList( void );
	virtual void UnlockDebugTextureList( void );
	virtual int GetTextureMemoryUsed( TextureMemoryType eTextureMemory );

	virtual void ClearVertexAndPixelShaderRefCounts();
	virtual void PurgeUnusedVertexAndPixelShaders();

	// Called when the dx support level has changed
	virtual void DXSupportLevelChanged( int nDXLevel );

	// User clip plane override
	virtual void EnableUserClipTransformOverride( bool bEnable );
	virtual void UserClipTransform( const VMatrix &worldToProjection );

	bool UsingSoftwareVertexProcessing() const;

	// Mark all user clip planes as being dirty
	void MarkAllUserClipPlanesDirty();

	// Converts a D3DXMatrix to a VMatrix and back
	void D3DXMatrixToVMatrix( const D3DXMATRIX &in, VMatrix &out );
	void VMatrixToD3DXMatrix( const VMatrix &in, D3DXMATRIX &out );

	ITexture *GetRenderTargetEx( int nRenderTargetID ) const;

	virtual void SetToneMappingScaleLinear( const Vector &scale );
	virtual const Vector &GetToneMappingScaleLinear( void ) const;

	void SetFloatRenderingParameter(int parm_number, float value);					// Rendering Parameter Setters
	void SetIntRenderingParameter(int parm_number, int value);						//
	void SetTextureRenderingParameter(int parm_number, ITexture *pTexture);			//
	void SetVectorRenderingParameter(int parm_number, Vector const &value);			//

	float GetFloatRenderingParameter(int parm_number) const;						// Rendering Parameter Getters
	int GetIntRenderingParameter(int parm_number) const;							//
	ITexture *GetTextureRenderingParameter(int parm_number) const;					//
	Vector GetVectorRenderingParameter(int parm_number) const;						//

	// For dealing with device lost in cases where Present isn't called all the time (Hammer)
	virtual void HandleDeviceLost();

	virtual void EnableLinearColorSpaceFrameBuffer( bool bEnable );

	void SetStencilState( const ShaderStencilState_t &state );												// Stencil methods
	void SetStencilStateInternal( const ShaderStencilState_t &state );
	void GetCurrentStencilState( ShaderStencilState_t *pState );
	void ClearStencilBufferRectangle(int xmin, int ymin, int xmax, int ymax,int value);

#if defined ( _GAMECONSOLE )
	bool PostQueuedTexture( const void *pData, int nSize, ShaderAPITextureHandle_t *pHandles, int nHandles, int nWidth, int nHeight, int nDepth, int nMips, int *pRefCount );
#endif

#if defined( _X360 )
	HXUIFONT OpenTrueTypeFont( const char *pFontname, int tall, int style );
	void CloseTrueTypeFont( HXUIFONT hFont );
	bool GetTrueTypeFontMetrics( HXUIFONT hFont, wchar_t wchFirst, wchar_t wchLast, XUIFontMetrics *pFontMetrics, XUICharMetrics *pCharMetrics );
	// Render a sequence of characters and extract the data into a buffer
	// For each character, provide the width+height of the font texture subrect,
	// an offset to apply when rendering the glyph, and an offset into a buffer to receive the RGBA data
	bool GetTrueTypeGlyphs( HXUIFONT hFont, int numChars, wchar_t *pWch, int *pOffsetX, int *pOffsetY, int *pWidth, int *pHeight, unsigned char *pRGBA, int *pRGBAOffset );
	ShaderAPITextureHandle_t CreateRenderTargetSurface( int width, int height, ImageFormat format, RTMultiSampleCount360_t multiSampleCount, const char *pDebugName, const char *pTextureGroupName );
	void PersistDisplay();
	void *GetD3DDevice();

	void PushVertexShaderGPRAllocation( int iVertexShaderCount = 64 );
	void PopVertexShaderGPRAllocation( void );

	void EnableVSync_360( bool bEnable );

	virtual void SetCacheableTextureParams( ShaderAPITextureHandle_t *pHandles, int count, const char *pFilename, int mipSkipCount );
	virtual void FlushHiStencil();
#endif

#if defined( _GAMECONSOLE )
	virtual void BeginConsoleZPass2( int nNumDynamicIndicesNeeded );
	virtual void EndConsoleZPass();
	virtual unsigned int GetConsoleZPassCounter() const { return m_nZPassCounter; }

	virtual void EnablePredication( bool bZPass, bool bRenderPass );
	virtual void DisablePredication();
#endif

#if defined( _PS3 )
	virtual void FlushTextureCache();
#endif
	virtual void AntiAliasingHint( int nHint );

	virtual bool OwnGPUResources( bool bEnable );

// ------------ New Vertex/Index Buffer interface ----------------------------
	void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 );
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes );
	void Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount );

	// Draw the mesh with the currently bound vertex and index buffers.
	void DrawWithVertexAndIndexBuffers( void );
// ------------ End ----------------------------

	// deformations
	virtual void PushDeformation( const DeformationBase_t *pDeformation );
	virtual void PopDeformation( );
	virtual int GetNumActiveDeformations( ) const ;


	// for shaders to set vertex shader constants. returns a packed state which can be used to set the dynamic combo
	virtual int GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations,
												 float *pConstantValuesOut,
												 int nBufferSize,
												 int nMaximumDeformations,
												 int *pNumDefsOut ) const ;

	inline Texture_t &GetTexture( ShaderAPITextureHandle_t hTexture )
	{
		return m_Textures[hTexture];
	}

	// Gets the texture 
	IDirect3DBaseTexture* GetD3DTexture( ShaderAPITextureHandle_t hTexture );
	virtual void* GetD3DTexturePtr( ShaderAPITextureHandle_t hTexture );
	virtual bool IsStandardTextureHandleValid( StandardTextureId_t textureId );

#ifdef _PS3
	virtual void GetPs3Texture(void* tex, ShaderAPITextureHandle_t hTexture );
	virtual void GetPs3Texture(void* tex, StandardTextureId_t nTextureId );
#endif

	virtual bool ShouldWriteDepthToDestAlpha( void ) const;

	virtual void AcquireThreadOwnership();
	virtual void ReleaseThreadOwnership();

	virtual void UpdateGameTime( float flTime ) { m_flCurrGameTime = flTime; }

	virtual bool IsStereoSupported() const;
	virtual void UpdateStereoTexture( ShaderAPITextureHandle_t texHandle, bool *pStereoActiveThisFrame );
#ifndef _PS3
private:
#endif
	enum
	{
		SMALL_BACK_BUFFER_SURFACE_WIDTH = 256,
		SMALL_BACK_BUFFER_SURFACE_HEIGHT = 256,
	};

	bool m_bEnableDebugTextureList;
	bool m_bDebugGetAllTextures;
	bool m_bDebugTexturesRendering;
	KeyValues *m_pDebugTextureList;
	CThreadFastMutex m_DebugTextureListLock;

	int m_nTextureMemoryUsedLastFrame, m_nTextureMemoryUsedTotal;
	int m_nTextureMemoryUsedPicMip1, m_nTextureMemoryUsedPicMip2;
	int m_nDebugDataExportFrame;

	FlashlightState_t m_FlashlightState;
	VMatrix m_FlashlightWorldToTexture;
	ITexture *m_pFlashlightDepthTexture;
	UberlightRenderState_t m_UberlightRenderState;
	float m_pFlashlightAtten[4];
	float m_pFlashlightPos[4];
	float m_pFlashlightColor[4];
	float m_pFlashlightTweaks[4];
	DepthBiasState_t m_ZBias;
	DepthBiasState_t m_ZBiasDecal;

	CascadedShadowMappingState_t m_CascadedShadowMappingState;
	CascadedShadowMappingState_t m_CascadedShadowMappingState_LightMapScaled;
	ITexture *m_pCascadedShadowMappingDepthTexture;

	CShaderAPIDx8( CShaderAPIDx8 const& );

	enum
	{
		INVALID_TRANSITION_OP = 0xFFFF
	};

	// State transition table for the device is as follows:

	// Other app init causes transition from OK to OtherAppInit, during transition we must release resources
	// !Other app init causes transition from OtherAppInit to OK, during transition we must restore resources
	// Minimized or device lost or device not reset causes transition from OK to LOST_DEVICE, during transition we must release resources
	// Minimized or device lost or device not reset causes transition from OtherAppInit to LOST_DEVICE

	// !minimized AND !device lost causes transition from LOST_DEVICE to NEEDS_RESET
	// minimized or device lost causes transition from NEEDS_RESET to LOST_DEVICE

	// Successful TryDeviceReset and !Other app init causes transition from NEEDS_RESET to OK, during transition we must restore resources
	// Successful TryDeviceReset and Other app init causes transition from NEEDS_RESET to OtherAppInit

	void ExportTextureList();
	void AddBufferToTextureList( const char *pName, D3DSURFACE_DESC &desc );
	int D3DFormatToBitsPerPixel( D3DFORMAT fmt ) const;

	void SetupTextureGroup( ShaderAPITextureHandle_t hTexture, const char *pTextureGroupName );

	// Creates the matrix stack
	void CreateMatrixStacks();

	// Initializes the render state
	void InitRenderState( );

	// Resets all dx renderstates to dx default so that our shadows are correct.
	void ResetDXRenderState( );
	
	// Resets the render state
	void ResetRenderState( bool bFullReset = true );

	// Setup standard vertex shader constants (that don't change)
	void SetStandardVertexShaderConstants( float fOverbright );
	
	// Initializes vertex and pixel shaders
	void InitVertexAndPixelShaders();

	// Discards the vertex and index buffers
	void DiscardVertexBuffers();

	// Computes the fill rate
	void ComputeFillRate();

	// Takes a snapshot
	virtual StateSnapshot_t TakeSnapshot( );

	// Converts the clear color to be appropriate for HDR
	D3DCOLOR GetActualClearColor( D3DCOLOR clearColor );

	// We lost the device
	void OnDeviceLost();

	// Gets the matrix stack from the matrix mode
	int GetMatrixStack( MaterialMatrixMode_t mode ) const;

	// Flushes the matrix state, returns false if we don't need to
	// do any more work
	bool MatrixIsChanging( TransformType_t transform = TRANSFORM_IS_GENERAL );

	// Updates the matrix transform state
	void UpdateMatrixTransform( TransformType_t transform = TRANSFORM_IS_GENERAL );

	// Sets the vertex shader modelView state..
	// NOTE: GetProjectionMatrix should only be called from the Commit functions!
	const D3DXMATRIX &GetProjectionMatrix( void );
	void SetVertexShaderViewProj();
	void SetVertexShaderModelViewProjAndModelView();

	void SetPixelShaderFogParams( int reg );
	void SetPixelShaderFogParams( int reg, ShaderFogMode_t fogMode );

	void SetVertexShaderCameraPos()
	{
		float vertexShaderCameraPos[4];
		vertexShaderCameraPos[0] = m_WorldSpaceCameraPosition[0];
		vertexShaderCameraPos[1] = m_WorldSpaceCameraPosition[1];
		vertexShaderCameraPos[2] = m_WorldSpaceCameraPosition[2];
		vertexShaderCameraPos[3] = m_DynamicState.m_FogZ;  //waterheight in water fog mode

		// eyepos.x eyepos.y eyepos.z cWaterZ
		SetVertexShaderConstantInternal( VERTEX_SHADER_CAMERA_POS, vertexShaderCameraPos );
	}
	
	// This is where vertex shader constants are set for both hardware vertex fog and pixel shader-blended vertex fog.
	FORCEINLINE void UpdateVertexShaderFogParams( ShaderFogMode_t fogMode = SHADER_FOGMODE_NUMFOGMODES, bool bVertexFog = false )
	{
		float fogParams[4];

		if( fogMode == SHADER_FOGMODE_NUMFOGMODES ) //not passing in an explicit fog mode
		{
			if ( m_TransitionTable.CurrentShadowState() == NULL )
			{
				fogMode = SHADER_FOGMODE_DISABLED;
				bVertexFog = false;
			}
			else
			{
				fogMode = ( ShaderFogMode_t ) m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_FogMode;
				bVertexFog = m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_bVertexFogEnable;
			}
		}

		if( (m_SceneFogMode != MATERIAL_FOG_NONE) && (fogMode != SHADER_FOGMODE_DISABLED) )
		{
			//prepare the values for Fixed Function fog, and we'll modify them to handle pixel fog if necessary
			float ooFogRange = 1.0f;

			float fStart = m_VertexShaderFogParams[0];
			float fEnd = m_VertexShaderFogParams[1];

			// Check for divide by zero
			if ( fEnd != fStart )
			{
				ooFogRange = 1.0f / ( fEnd - fStart );
			}

			// Fixed-function-blended per-vertex fog requires some inverted params since a fog factor of 0 means fully fogged and 1 means no fog.
			// We could implement shader fog the same way, but would require an extra subtract in the vertex and/or pixel shader, which we want to avoid.
			if ( HardwareConfig()->GetDXSupportLevel() <= 90 )
			{
				fogParams[0] = ooFogRange * fEnd;
				fogParams[1] = 0.0f;
				fogParams[2] = 1.0f - clamp( m_flFogMaxDensity, 0.0f, 1.0f ); // Max fog density
				fogParams[3] = ooFogRange;
			}
			else
			{
				fogParams[0] = 1.0f - ( ooFogRange * fEnd );
				fogParams[1] = 0.0f;
				fogParams[2] = clamp( m_flFogMaxDensity, 0.0f, 1.0f ); // Max fog density
				fogParams[3] = ooFogRange;
			}
		}
		else
		{
			// Fixed-function-blended per-vertex fog requires some inverted params since a fog factor of 0 means fully fogged and 1 means no fog.
			// We could implement shader fog the same way, but would require an extra subtract in the vertex and/or pixel shader, which we want to avoid.
			if ( HardwareConfig()->GetDXSupportLevel() <= 90 )
			{
				//emulate no-fog by rigging the result to always be 0. 
				fogParams[0] = 1.0f;
				fogParams[1] = -FLT_MAX;
				fogParams[2] = 1.0f; //Max fog density
				fogParams[3] = 0.0f; //0 out distance factor
			}
			else
			{
				//emulate no-fog by rigging the result to always be 0. 
				fogParams[0] = 0.0f;
				fogParams[1] = -FLT_MAX;
				fogParams[2] = 0.0f; //Max fog density
				fogParams[3] = 0.0f; //0 out distance factor
			}
		}

		// cFogEndOverFogRange, cFogOne, unused, cOOFogRange
		SetVertexShaderConstantInternal( VERTEX_SHADER_FOG_PARAMS, fogParams, 1 );
		SetVertexShaderCameraPos();
	}

	// Compute stats info for a texture
	void ComputeStatsInfo( ShaderAPITextureHandle_t hTexture, bool bIsCubeMap, bool isVolumeTexture );

	// For procedural textures
	void AdvanceCurrentCopy( ShaderAPITextureHandle_t hTexture );

	// Deletes a D3D texture
	void DeleteD3DTexture( ShaderAPITextureHandle_t hTexture );

	// Unbinds a texture
	void UnbindTexture( ShaderAPITextureHandle_t hTexture );

	// Releases all D3D textures
	void ReleaseAllTextures();

	// Deletes all textures
	void DeleteAllTextures();

	// Gets the currently modified texture handle
	ShaderAPITextureHandle_t GetModifyTextureHandle() const;

	// Gets the bind id
	ShaderAPITextureHandle_t GetBoundTextureBindId( Sampler_t sampler ) const;
	
	// If mat_texture_limit is enabled, then this tells us if binding the specified texture would
	// take us over the limit.
	bool WouldBeOverTextureLimit( ShaderAPITextureHandle_t hTexture );

	// Sets the texture state
	void SetTextureState( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t hTexture, bool force = false );

	FORCEINLINE void TouchTexture( Sampler_t sampler, IDirect3DBaseTexture *pD3DTexture );

	// Lookup standard texture handle
	ShaderAPITextureHandle_t GetStandardTextureHandle(StandardTextureId_t id);

	// Grab/release the internal render targets such as the back buffer and the save game thumbnail
	void AcquireInternalRenderTargets();
	void ReleaseInternalRenderTargets();

	// create/release linear->gamma table texture lookups. Only used by hardware supporting pixel shader 2b and up
	void AcquireLinearToGammaTableTextures();
	void ReleaseLinearToGammaTableTextures();

	// Gets the texture being modified
	IDirect3DBaseTexture* GetModifyTexture();
	void SetModifyTexture( IDirect3DBaseTexture *pTex );

	// returns true if we're using texture coordinates at a given stage
	bool IsUsingTextureCoordinates( int stage, int flags ) const;

	// Debugging spew
	void SpewBoardState();

	// Compute and save the world space camera position and direction
	void CacheWorldSpaceCamera();

	// Compute and save the projection atrix with polyoffset built in if we need it.
	void CachePolyOffsetProjectionMatrix();

	// Vertex shader helper functions
	int  FindVertexShader( VertexFormat_t fmt, const char* pFileName ) const;
	int  FindPixelShader( const char* pFileName ) const;

	// Returns copies of the front and back buffers
	IDirect3DSurface* GetFrontBufferImage( ImageFormat& format );
	IDirect3DSurface* GetBackBufferImage( Rect_t *pSrcRect, Rect_t *pDstRect, ImageFormat& format );
	IDirect3DSurface* GetBackBufferImageHDR( Rect_t *pSrcRect, Rect_t *pDstRect, ImageFormat& format );

	// Copy bits from a host-memory surface
	void CopyBitsFromHostSurface( IDirect3DSurface* pSurfaceBits, 
		const Rect_t &dstRect, unsigned char *pData, ImageFormat srcFormat, ImageFormat dstFormat, int nDstStride );

	void SetTextureFilterMode( Sampler_t sampler, TextureFilterMode_t nMode );
	
	void ExecuteCommandBuffer( uint8 *pCmdBuffer );
#ifdef _PS3
	void ExecuteCommandBufferPPU(uint8 *pCmdBuffer );
#endif

	void ExecuteInstanceCommandBuffer( const unsigned char *pCmdBuf, int nInstanceIndex, bool bForceStateSet );
	void SetStandardTextureHandle( StandardTextureId_t nId, ShaderAPITextureHandle_t );
	void RecomputeAggregateLightingState( void );
	virtual void DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance );

	// Methods related to queuing functions to be called per-(pMesh->Draw call) or per-pass
	void ClearAllCommitFuncs( CommitFuncType_t func );
	bool IsCommitFuncInUse( CommitFuncType_t func, int nFunc ) const;
	void MarkCommitFuncInUse( CommitFuncType_t func, int nFunc );
	void AddCommitFunc( CommitFuncType_t func, StateCommitFunc_t f );
	void CallCommitFuncs( CommitFuncType_t func, bool bForce = false );

	// Commits transforms and lighting
	void CommitStateChanges();

	// Commits transforms that have to be dealt with on a per pass basis (ie. projection matrix for polyoffset)
	void CommitPerPassStateChanges( StateSnapshot_t id );

	// Need to handle fog mode on a per-pass basis
	void CommitPerPassFogMode( bool bUsingVertexAndPixelShaders );

	void CommitPerPassXboxFixups();

	// Commits user clip planes
	void CommitUserClipPlanes( );

	// Gets the user clip transform (world->view)
	D3DXMATRIX & GetUserClipTransform( );

	// transform commit
	bool VertexShaderTransformChanged( int i );

	void UpdateVertexShaderMatrix( int iMatrix );
	void CommitVertexShaderTransforms();
	void CommitPerPassVertexShaderTransforms();

	// Recomputes the fast-clip plane matrices based on the current fast-clip plane
	void CommitFastClipPlane( );

	// Computes a matrix which includes the poly offset given an initial projection matrix
	void ComputePolyOffsetMatrix( const D3DXMATRIX& matProjection, D3DXMATRIX &matProjectionOffset );

	bool SetSkinningMatrices( const MeshInstanceData_t &instance );
	bool IsRenderingInstances() const;
	
	// lighting commit
	void CommitVertexShaderLighting( CompiledLightingState_t *pLightingState );
	void CommitPixelShaderLighting( int pshReg, CompiledLightingState_t *pLightingState );

	// Gets the surface associated with a texture (refcount of surface is increased)
	IDirect3DSurface* GetTextureSurface( ShaderAPITextureHandle_t textureHandle );
	IDirect3DSurface* GetDepthTextureSurface( ShaderAPITextureHandle_t textureHandle );

	//
	// Methods related to hardware config
	//
	void SetDefaultConfigValuesForDxLevel( int dxLevelFromCaps, ShaderDeviceInfo_t &info, unsigned int nFlagsUsed );

	// Determines hardware capabilities
	bool DetermineHardwareCaps( );

	// Alpha To Coverage entrypoints and states - much of this involves vendor-dependent paths and states...
	bool CheckVendorDependentAlphaToCoverage();
	void EnableAlphaToCoverage();
	void DisableAlphaToCoverage();

	// Vendor-dependent shadow mapping detection
	void CheckVendorDependentShadowMappingSupport( bool &bSupportsShadowDepthTextures, bool &bSupportsFetch4 );

	// Override caps based on a requested dx level
	void OverrideCaps( int nForcedDXLevel );

	// Reports support for a given MSAA mode
	bool SupportsMSAAMode( int nMSAAMode );

	// Reports support for a given CSAA mode
	bool SupportsCSAAMode( int nNumSamples, int nQualityLevel );

	// Gamma correction of fog color, or not...
	D3DCOLOR ComputeGammaCorrectedFogColor( unsigned char r, unsigned char g, unsigned char b, bool bSRGBWritesEnabled );

	bool RestorePersistedDisplay( bool bUseFrontBuffer );

	void ClearStdTextureHandles( void );

	// debug logging
	void PrintfVA( char *fmt, va_list vargs );
	void Printf( char *fmt, ... );	
	float Knob( char *knobname, float *setvalue = NULL );

	void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics );

	virtual float GetLightMapScaleFactor() const;

	virtual ShaderAPITextureHandle_t FindTexture( const char *pDebugName );
	virtual void GetTextureDimensions( ShaderAPITextureHandle_t hTexture, int &nWidth, int &nHeight, int &nDepth );

#ifndef _GAMECONSOLE
	D3DDeviceWrapper	m_DeviceWrapper;
#endif

	// "normal" back buffer and depth buffer.  Need to keep this around so that we
	// know what to set the render target to when we are done rendering to a texture.
	CUtlVector<IDirect3DSurface*> m_pBackBufferSurfaces;
	IDirect3DSurface	*m_pBackBufferSurfaceSRGB;
	IDirect3DSurface	*m_pZBufferSurface;

	// Optimization for screenshots
	IDirect3DSurface	*m_pSmallBackBufferFP16TempSurface;

	ShaderAPITextureHandle_t m_hFullScreenTexture;

	ShaderAPITextureHandle_t m_hLinearToGammaTableTexture;
	ShaderAPITextureHandle_t m_hLinearToGammaTableIdentityTexture;

	//
	// State needed at the time of rendering (after snapshots have been applied)
	//

	// Interface for the D3DXMatrixStack
	ID3DXMatrixStack*	m_pMatrixStack[NUM_MATRIX_MODES];
	matrix3x4_t			m_boneMatrix[NUM_MODEL_TRANSFORMS];
	int					m_maxBoneLoaded;

	// Current matrix mode
	D3DTRANSFORMSTATETYPE m_MatrixMode;
	int m_CurrStack;

	// The current camera position in world space.
	Vector m_WorldSpaceCameraPosition;
	Vector m_WorldSpaceCameraDirection;

	// The current projection matrix with polyoffset baked into it.
	D3DXMATRIX		m_CachedPolyOffsetProjectionMatrix;
	D3DXMATRIX		m_CachedFastClipProjectionMatrix;
	D3DXMATRIX		m_CachedFastClipPolyOffsetProjectionMatrix;

	// The texture stage state that changes frequently
	DynamicState_t	m_DynamicState;

	// The *desired* dynamic state. Most dynamic state is committed into actual hardware state
	// at either per-pass or per-material time.	This can also be used to force the hardware
	// to match the desired state after returning from alt-tab.
	DynamicState_t	m_DesiredState;

	// A list of state commit functions to run as per-draw call commit time
	unsigned char m_pCommitFlags[COMMIT_FUNC_TYPE_COUNT][ COMMIT_FUNC_BYTE_COUNT ];
	CUtlVector< StateCommitFunc_t >	m_CommitFuncs[COMMIT_FUNC_TYPE_COUNT];

	// Render data
	CMeshBase *m_pRenderMesh;
	int m_nRenderInstanceCount;
	const MeshInstanceData_t *m_pRenderInstances;
	CompiledLightingState_t *m_pRenderCompiledState;
	InstanceInfo_t *m_pRenderInstanceInfo;
	ShaderStencilState_t m_RenderInitialStencilState;
	bool m_bRenderHasSetStencil;

	int m_nDynamicVBSize;
	IMaterialInternal *m_pMaterial;

	// Shadow depth bias states
	float m_fShadowSlopeScaleDepthBias;
	float m_fShadowDepthBias;

	bool		m_bReadPixelsEnabled : 1;
	bool		m_bFlipCulling : 1;
	bool		m_bSinglePassFlashlightMode : 1;
	bool		m_UsingTextureRenderTarget : 1;

	int			m_ViewportMaxWidth;
	int			m_ViewportMaxHeight;

	ShaderAPITextureHandle_t m_hCachedRenderTarget;
	bool		m_bUsingSRGBRenderTarget;

	// The current frame
	int m_CurrentFrame;

	// The texture we're currently modifying
	// Using thread local variables so that ModifyTexture(), TexLock()/TexUnlock(), ... can concurently be used
	// from different thread (eg scaleform and lightmap)
	static CTHREADLOCALINTEGER( ShaderAPITextureHandle_t ) m_ModifyTextureHandle;
	static CTHREADLOCALINT m_ModifyTextureLockedLevel;
	static CTHREADLOCALINT m_ModifyTextureLockedFace;

	// Stores all textures
	CUtlFixedLinkedList< Texture_t >	m_Textures;
#ifdef _PS3
	CUtlVector< ShaderAPITextureHandle_t > m_ArtificialTextureHandles;
	struct DepthBufferCacheEntry_t
	{
		CPs3gcmTexture *m_pReal;
		CPs3gcmTexture *m_pCache;
	};
	CUtlVector< DepthBufferCacheEntry_t > m_arrPs3DepthBufferCache;
#endif

	float			m_VertexShaderFogParams[2];
	float			m_flFogMaxDensity;

	// Shadow state transition table
	CTransitionTable m_TransitionTable;
	StateSnapshot_t m_nCurrentSnapshot;

	// Depth test override...
	bool	m_bOverrideMaterialIgnoreZ;
	bool 	m_bIgnoreZValue;

	// Are we in the middle of resetting the render state?
	bool	m_bResettingRenderState;

	// Can we buffer 2 frames ahead?
	bool	m_bBuffer2FramesAhead;

	// Selection name stack
	CUtlStack< int >	m_SelectionNames;
	bool				m_InSelectionMode;
	unsigned int*		m_pSelectionBufferEnd;
	unsigned int*		m_pSelectionBuffer;
	unsigned int*		m_pCurrSelectionRecord;
	float				m_SelectionMinZ;
	float				m_SelectionMaxZ;
	int					m_NumHits;

	// fog
	unsigned char m_SceneFogColor[3];
	MaterialFogMode_t m_SceneFogMode;

	// Tone Mapping state ( w is gamma scale )
	Vector4D m_ToneMappingScale;

	Deformation_t m_DeformationStack[DEFORMATION_STACK_DEPTH];

	Deformation_t *m_pDeformationStackPtr;

	void WriteShaderConstantsToGPU();

	// rendering parameter storage
	int IntRenderingParameters[MAX_INT_RENDER_PARMS];
	ITexture *TextureRenderingParameters[MAX_TEXTURE_RENDER_PARMS];
	float FloatRenderingParameters[MAX_FLOAT_RENDER_PARMS];
	Vector VectorRenderingParameters[MAX_VECTOR_RENDER_PARMS];

	ShaderAPITextureHandle_t m_StdTextureHandles[TEXTURE_MAX_STD_TEXTURES];
	
	// PIX instrumentation utilities...enable these with PIX_INSTRUMENTATION
	void StartPIXInstrumentation();
	void EndPIXInstrumentation();
	void SetPIXMarker( unsigned long color, const char *szName );
	void IncrementPIXError();
	bool PIXError();
	int m_nPIXErrorCount;
	int m_nPixFrame;
	bool m_bPixCapturing;

	void ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const
	{
		return MeshMgr()->ComputeVertexDescription( pBuffer, vertexFormat, desc );
	}

	int VertexFormatSize( VertexFormat_t vertexFormat ) const
	{
		return MeshMgr()->VertexFormatSize( vertexFormat );
	}

	// Bracket custom shadow rendering code that doesn't use transition tables or regular cshaders
	bool m_bToolsMode;
	bool m_bVtxLitMesh;
	bool m_bUnlitMesh;
	bool m_bLmapMesh;
	bool m_bGeneratingCSMs;
	BOOL m_bCSMsValidThisFrame;
	void BeginGeneratingCSMs();
	void EndGeneratingCSMs();

	// Per-cascade settings
	void PerpareForCascadeDraw( int cascade, float fShadowSlopeScaleDepthBias, float fShadowDepthBias );

	void SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias );

	void EnableBuffer2FramesAhead( bool bEnable );
	
	void GetActualProjectionMatrix( float *pMatrix );

	void SetDepthFeatheringShaderConstants( int iConstant, float fDepthBlendScale );

	void SetDisallowAccess( bool b )
	{
		g_bShaderAccessDisallowed = b;
	}

	void EnableShaderShaderMutex( bool b )
	{
		Assert( g_ShaderMutex.GetOwnerId() == 0 );
		g_bUseShaderMutex = b;
	}

	void ShaderLock()
	{
		g_ShaderMutex.Lock();
	}

	void ShaderUnlock()
	{
		g_ShaderMutex.Unlock();
	}

	//The idea behind a delayed constant is this.
	// Some shaders set constants based on rendering states, and some rendering states aren't updated until after a shader's already called Draw().
	//  So, for some functions that are state based, we save the constant we set and if the state changes between when it's set in the shader setup code 
	//   and when the shader is drawn, we update that constant.
	struct DelayedConstants_t
	{
		int iPixelShaderFogParams;

		void Invalidate( void )
		{
			iPixelShaderFogParams = -1;
		}
		DelayedConstants_t( void ) { this->Invalidate(); }
	};
	DelayedConstants_t m_DelayedShaderConstants;

	bool SetRenderTargetInternalXbox( ShaderAPITextureHandle_t hTexture, bool bForce = false );

	FogMethod_t ComputeFogMethod( ShaderFogMode_t shaderFogMode, MaterialFogMode_t sceneFogMode, bool bVertexFog );

#if defined( _X360 )
	CUtlStack<int> m_VertexShaderGPRAllocationStack;
#endif

	int	m_MaxVectorVertexShaderConstant;	
	int	m_MaxBooleanVertexShaderConstant;
	int	m_MaxIntegerVertexShaderConstant;
	int	m_MaxVectorPixelShaderConstant;	
	int	m_MaxBooleanPixelShaderConstant;
	int	m_MaxIntegerPixelShaderConstant;

	bool m_bGPUOwned;
	bool m_bResetRenderStateNeeded;
#if defined( _GAMECONSOLE )
	bool m_bInZPass;
	unsigned int m_nZPassCounter;
	StateSnapshot_t m_zPassSnapshot;
#endif

	float32 m_flCurrGameTime;

#if defined( _X360 )
	struct XboxFontMap_t
	{
		XboxFontMap_t()
		{
			m_pPhysicalMemory = NULL;
			m_nMemorySize = 0;
			m_nFontFileSize = 0;
		}

		void	*m_pPhysicalMemory;
		int		m_nMemorySize;
		int		m_nFontFileSize;
	};

	CUtlDict< XboxFontMap_t > m_XboxFontMemoryDict;
#endif

#if defined( _WIN32 )
	IDirect3DSurface *m_pNVAPI_registeredDepthStencilSurface;
	IDirect3DTexture *m_pNVAPI_registeredDepthTexture;
#endif
};


//-----------------------------------------------------------------------------
// Class Factory
//-----------------------------------------------------------------------------
static CShaderAPIDx8 g_ShaderAPIDX8;
IShaderAPIDX8 *g_pShaderAPIDX8 = &g_ShaderAPIDX8;
CShaderDeviceDx8* g_pShaderDeviceDx8 = &g_ShaderAPIDX8;

// FIXME: Remove IShaderAPI + IShaderDevice; they change after SetMode
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIDx8, IShaderAPI, 
				SHADERAPI_INTERFACE_VERSION, g_ShaderAPIDX8 )

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIDx8, IShaderDevice, 
				SHADER_DEVICE_INTERFACE_VERSION, g_ShaderAPIDX8 )

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIDx8, IDebugTextureInfo, 
				DEBUG_TEXTURE_INFO_VERSION, g_ShaderAPIDX8 )

//-----------------------------------------------------------------------------
// CShaderAPIDx8 static members
//-----------------------------------------------------------------------------
CTHREADLOCALINTEGER( ShaderAPITextureHandle_t ) CShaderAPIDx8::m_ModifyTextureHandle(INVALID_SHADERAPI_TEXTURE_HANDLE);
CTHREADLOCALINT CShaderAPIDx8::m_ModifyTextureLockedLevel(-1);
CTHREADLOCALINT CShaderAPIDx8::m_ModifyTextureLockedFace;

//-----------------------------------------------------------------------------
// Accessors for major interfaces
//-----------------------------------------------------------------------------
#if !defined( _GAMECONSOLE )
D3DDeviceWrapper *Dx9Device()
{
	return g_ShaderAPIDX8.Dx9Device();
}
#endif

// Pix wants a max of 32 characters
// We'll give it the right-most substrings separated by slashes
static char s_pPIXMaterialName[32];
void PIXifyName( char *pDest, const char *pSrc )
{
	char *pSrcWalk = (char *)pSrc;

	// Walk forward looking for the end, find the last \ or /
	char *pLastSlash = pSrcWalk;
	while ( *pSrcWalk )
	{
		char c = *pSrcWalk;
		if ( c == '\\' || c == '/' )
		{
			pLastSlash = pSrcWalk + 1;
		}
		++pSrcWalk;
	}

	size_t nBytes = (intp)pSrcWalk - (intp)pLastSlash + 1;
	if ( nBytes > 32 )
	{
		pLastSlash = pSrcWalk + 1 - 32;
		nBytes = 32;
	}
	else if ( pLastSlash != pSrc )
	{
		pSrcWalk = pLastSlash - 2;
		size_t nTestBytes = nBytes + 2;
		while( nTestBytes <= 32 )
		{
			char c = *pSrcWalk;
			if ( c == '\\' || c == '/' )
			{
				pLastSlash = pSrcWalk + 1;
				nBytes = nTestBytes - 1;
			}

			if ( pSrcWalk == pSrc )
			{
				pLastSlash = (char*)pSrc;
				nBytes = nTestBytes;
				break;
			}

			--pSrcWalk;
			++nTestBytes;
		}
	}

	memcpy( pDest, pLastSlash, nBytes );
}

#ifdef _WIN32
void PrintError( NvAPI_Status status, uint32 unStage, uint32 unProp, bool bPlus = false )
{
	NvAPI_ShortString szDesc = { 0 };
	NvAPI_GetErrorMessage( status, szDesc );
#ifdef DEBUG
	Msg( " NVAPI error: %s\n", szDesc );
#endif
	Error( "Failed to initialize NVidia driver!\nDriver error at 0x%08X%s%08X: %s\n\nPlease visit NVidia website to get the most recent version of the graphics drivers and restore your Counter-Strike: Global Offensive driver profile and global driver profile to NVidia defaults.",
		unStage, ( bPlus ? "+" : "-" ), unProp,
		szDesc );
}

#ifdef DEBUG
void DumpProfileSettings( NvDRSSessionHandle hSession, NvDRSProfileHandle hProfile, int numOfSettings )
{
	char szTemp[ 2048 ];
	size_t nChars = 0;

	if ( numOfSettings > 0 )
	{
		NVDRS_SETTING *setArray = new NVDRS_SETTING[ numOfSettings ];
		V_memset( setArray, 0, sizeof( NVDRS_SETTING ) * numOfSettings );
		NvU32 numSetRead = numOfSettings;
		setArray[ 0 ].version = NVDRS_SETTING_VER;
		NvAPI_Status status = NvAPI_DRS_EnumSettings( hSession, hProfile, 0, &numSetRead, setArray );
		if (status != NVAPI_OK)
		{
			PrintError( status, 0x454e4d53, numOfSettings );
			return;
		}
		for( unsigned int i = 0; i < numSetRead; i++ )
		{
			if ( setArray[ i ].settingLocation != NVDRS_CURRENT_PROFILE_LOCATION )
			{
				continue;
			}
			wcstombs_s( &nChars, szTemp, 2048, (wchar_t*)(setArray[ i ].settingName), 2048 );
			Msg( "Setting Name: %s\n", szTemp );
			Msg( "Setting ID: %X\n", setArray[ i ].settingId );
			Msg( "Setting Type: %X\n", setArray[ i ].settingType );
			Msg( "Predefined? : %d\n", setArray[ i ].isCurrentPredefined );
			switch ( setArray[ i ].settingType )
			{
				case NVDRS_DWORD_TYPE:
					Msg( "Setting Value: %X\n\n", setArray[ i ].u32CurrentValue );
					break;
				case NVDRS_BINARY_TYPE:
					{
						Msg( "Setting Binary (length=%d) :", setArray[ i ].binaryCurrentValue.valueLength );
						for( unsigned int len = 0; len < setArray[ i ].binaryCurrentValue.valueLength; len++ )
						{
							Msg(" %02x", setArray[ i ].binaryCurrentValue.valueData[ len ] );
						}
						Msg( "\n\n" );
					}
					break;
				case NVDRS_WSTRING_TYPE:
					{
						wcstombs_s( &nChars, szTemp, 2048, ( wchar_t* )( setArray[ i ].wszCurrentValue ), 2048 );
						Msg( "Setting Value: %s\n\n", szTemp );
					}
					break;
			}
		}
	}
	Msg("\n");
}
#endif

void ForceProfileSettings( NvDRSSessionHandle hSession, NvDRSProfileHandle hProfile, int numOfSettings )
{
	bool bNeedsSaving = false;
	uint32 unFirstFixedProp = 0;
	uint32 unFirstFixedValue = 0;
	if ( numOfSettings > 0 )
	{
		NVDRS_SETTING *setArray = new NVDRS_SETTING[ numOfSettings ];
		V_memset( setArray, 0, sizeof( NVDRS_SETTING ) * numOfSettings );
		NvU32 numSetRead = numOfSettings;
		setArray[ 0 ].version = NVDRS_SETTING_VER;
		NvAPI_Status status = NvAPI_DRS_EnumSettings( hSession, hProfile, 0, &numSetRead, setArray );
		if ( status != NVAPI_OK )
		{
			// PrintError( status, 0x454e4d53, numOfSettings );
			return;
		}
		for ( unsigned int i = 0; i < numSetRead; i++ )
		{
			if ( setArray[ i ].settingLocation != NVDRS_CURRENT_PROFILE_LOCATION )
			{
				continue;
			}
			bool bNeedsReset = false;
			switch ( setArray[ i ].settingId )
			{
			case 0x002C7F45: // Ambient Occlusion Compatibility
				switch ( setArray[ i ].u32CurrentValue )
				{
				case 0: // disabled
				case 0x2C: // CS:GO
					break;
				default:
					bNeedsReset = true;
					break;
				}
				break;
			case 0x00664339: // NVIDIA Predefined Ambient Occlusion Usage
				switch ( setArray[ i ].u32CurrentValue )
				{
				case 1: // Enabled
					break;
				default:
					bNeedsReset = true;
					break;
				}
				break;
			case 0x00667329: // Ambient Occlusion Setting
				switch ( setArray[ i ].u32CurrentValue )
				{
				case 0: // Off
					break;
				default:
					bNeedsReset = true;
					break;
				}
				break;
			case 0x00738E8F: // texture filtering LOD bias (DX)
			case 0x20403F79: // texture filtering LOD bias (GL)
				switch ( setArray[ i ].u32CurrentValue )
				{
				case 0: // Off
					break;
				default:
					bNeedsReset = true;
					break;
				}
				break;
			}
			if ( bNeedsReset )
			{
#ifdef _DEBUG
				Msg( "NVIDIA Setting Override Required -- ID: %X Old Value: %X\n", setArray[ i ].settingId, setArray[ i ].u32CurrentValue );
#endif
				if ( !bNeedsSaving )
				{
					unFirstFixedProp = setArray[ i ].settingId;
					unFirstFixedValue = setArray[ i ].u32CurrentValue;
				}
				status = NvAPI_DRS_RestoreProfileDefaultSetting( hSession, hProfile, setArray[ i ].settingId );
				if ( status != NVAPI_OK )
				{
					PrintError( status, setArray[ i ].settingId, setArray[ i ].u32CurrentValue );
				}
				bNeedsSaving = true;
			}
		}
	}
	
	if ( bNeedsSaving )
	{
		NvAPI_Status status;
		status = NvAPI_DRS_SaveSettings( hSession );
		if ( ( status != NVAPI_OK )
			&& ( status != NVAPI_FILE_NOT_FOUND )
			&& ( status != NVAPI_ERROR ) )
		{
			PrintError( status, unFirstFixedProp, unFirstFixedValue, true );
		}
	}
}

bool CheckAndFixProfileSettings( NvDRSSessionHandle hSession, NvDRSProfileHandle hProfile, bool &bCSGOProfileFound )
{
	NvAPI_Status status;
	NVDRS_PROFILE profileInformation = { 0 };
	profileInformation.version = NVDRS_PROFILE_VER;
	status = NvAPI_DRS_GetProfileInfo( hSession, hProfile, &profileInformation );
	if ( status != NVAPI_OK )
	{
		// PrintError( status, 0x50524f46, status );
		return false;
	}

	char szTemp[ 2048 ];
	size_t nChars = 0;

	if ( profileInformation.numOfApps > 0 )
	{
#if 1	// use the basic V1 info to work with oldest possible drivers
		NVDRS_APPLICATION_V1 *appArray = new NVDRS_APPLICATION_V1[ profileInformation.numOfApps ];
		appArray[ 0 ].version = NVDRS_APPLICATION_VER_V1;
#else	// most recent version of app info
		NVDRS_APPLICATION *appArray = new NVDRS_APPLICATION[ profileInformation.numOfApps ];
		appArray[ 0 ].version = NVDRS_APPLICATION_VER;
#endif
		NvU32 numAppsRead = profileInformation.numOfApps;
		status = NvAPI_DRS_EnumApplications( hSession, hProfile, 0, &numAppsRead, reinterpret_cast< NVDRS_APPLICATION * >( appArray ) );
		if ( status != NVAPI_OK )
		{
			// PrintError( status, 0x454e4d41, numAppsRead );
			delete[] appArray;
			return false;
		}
		for( unsigned int i = 0; i < numAppsRead; i++ )
		{
			wcstombs_s( &nChars, szTemp, 2048, ( wchar_t* )( appArray[ i ].appName ), 2048 );
			if ( V_stristr( szTemp, "csgo.exe" ) != NULL )
			{
				bCSGOProfileFound = true;
#ifdef DEBUG
				wcstombs_s( &nChars, szTemp, 2048, ( wchar_t* )( profileInformation.profileName ), 2048 );
				Msg( "Profile Name: %s\n", szTemp );
				Msg( "Number of Applications associated with the Profile: %d\n", profileInformation.numOfApps );
				Msg( "Number of Settings associated with the Profile: %d\n", profileInformation.numOfSettings );
				Msg( "Is Predefined: %d\n\n", profileInformation.isPredefined );
				Msg( "Executable: %s\n", szTemp);
				wcstombs_s( &nChars, szTemp, 2048, ( wchar_t* )( appArray[ i ].userFriendlyName ), 2048 );
				Msg( "User Friendly Name: %s\n", szTemp );
				Msg( "Is Predefined: %d\n\n", appArray[ i ].isPredefined );
				DumpProfileSettings( hSession, hProfile, profileInformation.numOfSettings );
#endif
				ForceProfileSettings( hSession, hProfile, profileInformation.numOfSettings );
			}
		}
		delete[] appArray;
	}

	return true;
}

void ScanAndFixNvDriverProfiles()
{
	NvAPI_Status status;
	status = NvAPI_Initialize();
	if ( status != NVAPI_OK )
	{
		// will get here for any non-Nv drivers or really really old Nv drivers that don't support NvAPI
		return;
	}

	NvDRSSessionHandle hSession = 0;
	status = NvAPI_DRS_CreateSession( &hSession );
	if ( status == NVAPI_OK )
	{
		status = NvAPI_DRS_LoadSettings( hSession );
		if ( status == NVAPI_OK )
		{
			NvDRSProfileHandle hProfile = 0;
			bool bCSGOProfileFound = false;
			unsigned int index = 0;
			while ( ( status = NvAPI_DRS_EnumProfiles( hSession, index, &hProfile ) ) == NVAPI_OK )
			{
				CheckAndFixProfileSettings( hSession, hProfile, bCSGOProfileFound );
				index++;
			}

			// force global settings
			status = NvAPI_DRS_GetBaseProfile( hSession, &hProfile );
			if ( status == NVAPI_OK )
			{
				NVDRS_PROFILE profileInformation = { 0 };
				profileInformation.version = NVDRS_PROFILE_VER;
				status = NvAPI_DRS_GetProfileInfo( hSession, hProfile, &profileInformation );
				if ( status == NVAPI_OK )
				{
					ForceProfileSettings( hSession, hProfile, profileInformation.numOfSettings );
				}
			}
		}
	}

	NvAPI_DRS_DestroySession( hSession );
	hSession = 0; 
}
#endif

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderAPIDx8::CShaderAPIDx8() :
	m_Textures( 32 ),
	m_CurrStack( -1 ),
	m_pRenderMesh( 0 ),
	m_nDynamicVBSize( DYNAMIC_VERTEX_BUFFER_MEMORY ),
	m_pMaterial( NULL ),
	m_CurrentFrame( 0 ),
	m_InSelectionMode( false ),
	m_SelectionMinZ( FLT_MAX ),
	m_SelectionMaxZ( FLT_MIN ),
	m_pSelectionBuffer( 0 ),
	m_pSelectionBufferEnd( 0 ),
	m_bResetRenderStateNeeded( false ),
	m_nPixFrame(0),
	m_bPixCapturing(false),
	m_nPIXErrorCount(0),
	m_pBackBufferSurfaces(),
	m_pBackBufferSurfaceSRGB( 0 ),
	m_pZBufferSurface( 0 ),
	m_bResettingRenderState( false ),
	m_bReadPixelsEnabled( false ),
	m_bSinglePassFlashlightMode( false ),
	m_ToneMappingScale( 1.0f, 1.0f, 1.0f, 1.0f ),
	m_hFullScreenTexture( INVALID_SHADERAPI_TEXTURE_HANDLE ),
	m_hLinearToGammaTableTexture( INVALID_SHADERAPI_TEXTURE_HANDLE ),
	m_hLinearToGammaTableIdentityTexture( INVALID_SHADERAPI_TEXTURE_HANDLE ),
	m_fShadowSlopeScaleDepthBias( 16.0f ),
	m_fShadowDepthBias( 0.00008f ),
	m_hCachedRenderTarget( INVALID_SHADERAPI_TEXTURE_HANDLE ),
	m_bUsingSRGBRenderTarget( false ),
	m_bFlipCulling( false ),
	m_flCurrGameTime( 0.0f ),
#if defined( _X360 )
	m_XboxFontMemoryDict( k_eDictCompareTypeCaseInsensitive ),
#endif
	m_bIsStereoActiveThisFrame( false )
{
	// FIXME: Remove! Backward compat
	m_bAdapterSet = false;
	m_bBuffer2FramesAhead = false;
	m_bReadPixelsEnabled = true;

	memset( m_pMatrixStack, 0, sizeof(ID3DXMatrixStack*) * NUM_MATRIX_MODES );
	memset( &m_DynamicState, 0, sizeof(m_DynamicState) );
	//m_DynamicState.m_HeightClipMode = MATERIAL_HEIGHTCLIPMODE_DISABLE;
	m_nWindowHeight = m_nWindowWidth = 0;
	m_maxBoneLoaded = 0;

	m_bEnableDebugTextureList = 0;
	m_bDebugTexturesRendering = 0;
	m_pDebugTextureList = NULL;
	m_nTextureMemoryUsedLastFrame = 0;
	m_nTextureMemoryUsedTotal = 0;
	m_nTextureMemoryUsedPicMip1 = 0;
	m_nTextureMemoryUsedPicMip2 = 0;
	m_nDebugDataExportFrame = 0;

	m_SceneFogColor[0] = 0;
	m_SceneFogColor[1] = 0;
	m_SceneFogColor[2] = 0;
	m_SceneFogMode = MATERIAL_FOG_NONE;

	// We haven't yet detected whether we support CreateQuery or not yet.
	memset(IntRenderingParameters,0,sizeof(IntRenderingParameters));
	memset(FloatRenderingParameters,0,sizeof(FloatRenderingParameters));
	memset(VectorRenderingParameters,0,sizeof(VectorRenderingParameters));

	m_pDeformationStackPtr = m_DeformationStack + DEFORMATION_STACK_DEPTH;

	m_bGPUOwned = false;
	m_MaxVectorVertexShaderConstant = 0;
	m_MaxBooleanVertexShaderConstant = 0;
	m_MaxIntegerVertexShaderConstant = 0;
	m_MaxVectorPixelShaderConstant = 0;
	m_MaxBooleanPixelShaderConstant = 0;
	m_MaxIntegerPixelShaderConstant = 0;

	// init to cover "real" backbuffer and HDR backbuffer
	m_pBackBufferSurfaces.SetCount( 2 );
	m_pBackBufferSurfaces[0] = NULL;
	m_pBackBufferSurfaces[1] = NULL;

	ClearStdTextureHandles();

	V_memset( &m_CascadedShadowMappingState, 0, sizeof( m_CascadedShadowMappingState ) );
	V_memset( &m_CascadedShadowMappingState_LightMapScaled, 0, sizeof( m_CascadedShadowMappingState_LightMapScaled ) );
	m_pCascadedShadowMappingDepthTexture = NULL;

#ifdef _GAMECONSOLE
	m_bInZPass = false;
	m_nZPassCounter = 0;
	m_zPassSnapshot = -1;
#endif

#if defined( _PS3 ) || defined( _OSX )
	g_pShaderAPI = this;
	g_pShaderDevice = this;
#endif

#ifdef _PS3
	//
	// Set artificial texture handle (BACKBUFFER)
	//
	{
		ShaderAPITextureHandle_t hTexture = m_Textures.AddToTail();
		m_ArtificialTextureHandles.AddToTail( hTexture );
		Texture_t *pTexture = &m_Textures[hTexture];
		void Ps3gcmInitializeArtificialTexture( Texture_t *pTexture );
		Ps3gcmInitializeArtificialTexture( pTexture );

		pTexture->m_DebugName = "^PS3^BACKBUFFER";
		pTexture->SetImageFormat( IMAGE_FORMAT_ARGB8888 );
	}
	//
	// Set artificial texture handle (DEPTHBUFFER)
	//
	{
		ShaderAPITextureHandle_t hTexture = m_Textures.AddToTail();
		m_ArtificialTextureHandles.AddToTail( hTexture );
		Texture_t *pTexture = &m_Textures[hTexture];
		void Ps3gcmInitializeArtificialTexture( Texture_t *pTexture );
		Ps3gcmInitializeArtificialTexture( pTexture );

		pTexture->m_DebugName = "^PS3^DEPTHBUFFER";
		pTexture->SetImageFormat( IMAGE_FORMAT_D24S8 );
	}
	//
	// End of artificial texture handles
	//
#endif

	m_bGeneratingCSMs = false;
    m_bVtxLitMesh = false;
	m_bLmapMesh = false;
	m_bUnlitMesh = false;

#ifdef WIN32
	ScanAndFixNvDriverProfiles();
	m_pNVAPI_registeredDepthStencilSurface = NULL;
	m_pNVAPI_registeredDepthTexture = NULL;
#endif
}

#ifdef _PS3
void Ps3gcmInitializeArtificialTexture( Texture_t *pTexture )
{
	pTexture->m_DebugName = "^PS3^";

	pTexture->m_Flags = Texture_t::IS_ALLOCATED;
	pTexture->m_Depth = 1;
	pTexture->m_Count = 1;
	pTexture->m_CountIndex = 0;

	pTexture->m_CreationFlags = 0;
	pTexture->m_Flags |= 0;

	// Set the initial texture state
	pTexture->m_NumCopies = 1;
	pTexture->m_CurrentCopy = 0;

	// -- patched after surfaces are created --
	pTexture->m_Width = 1;
	pTexture->m_Height = 1;
	pTexture->SetTexture( NULL );
	// --

	pTexture->SetImageFormat( IMAGE_FORMAT_ARGB8888 );
	pTexture->m_UTexWrap = D3DTADDRESS_CLAMP;
	pTexture->m_VTexWrap = D3DTADDRESS_CLAMP;
	pTexture->m_WTexWrap = D3DTADDRESS_CLAMP;

	pTexture->m_MinFilter = pTexture->m_MagFilter = D3DTEXF_LINEAR;

	pTexture->m_NumLevels = 1;
	pTexture->m_MipFilter = D3DTEXF_NONE;

	pTexture->m_SwitchNeeded = false;

	pTexture->m_SizeBytes = 0;
	pTexture->m_SizeTexels = 0;
	pTexture->m_LastBoundFrame = -1;
}

ShaderAPITextureHandle_t Ps3gcmGetArtificialTextureHandle( int iHandle )
{
	return ( ( CShaderAPIDx8 * ) g_pShaderAPIDX8 )->m_ArtificialTextureHandles[ iHandle ];
}

void Ps3gcmInitializeArtificialTexture( int iHandle, IDirect3DBaseTexture *pPtr )
{
	Texture_t *pTexture = &( ( CShaderAPIDx8 * ) g_pShaderAPIDX8 )->GetTexture( Ps3gcmGetArtificialTextureHandle( iHandle ) );
	pTexture->SetTexture( pPtr );

	D3DSURFACE_DESC ddd;
	pPtr->GetLevelDesc( 0, &ddd );
	pTexture->m_Width = ddd.Width;
	pTexture->m_Height = ddd.Height;
}
#endif

CShaderAPIDx8::~CShaderAPIDx8()
{
	if ( m_DynamicState.m_pVectorVertexShaderConstant )
	{
		delete[] m_DynamicState.m_pVectorVertexShaderConstant;
		m_DynamicState.m_pVectorVertexShaderConstant = NULL;
	}

	if ( m_DynamicState.m_pBooleanVertexShaderConstant )
	{
		delete[] m_DynamicState.m_pBooleanVertexShaderConstant;
		m_DynamicState.m_pBooleanVertexShaderConstant = NULL;
	}

	if ( m_DynamicState.m_pIntegerVertexShaderConstant )
	{
		delete[] m_DynamicState.m_pIntegerVertexShaderConstant;
		m_DynamicState.m_pIntegerVertexShaderConstant = NULL;
	}

	if ( m_DynamicState.m_pVectorPixelShaderConstant )
	{
		delete[] m_DynamicState.m_pVectorPixelShaderConstant;
		m_DynamicState.m_pVectorPixelShaderConstant = NULL;
	}

	if ( m_DynamicState.m_pBooleanPixelShaderConstant )
	{
		delete[] m_DynamicState.m_pBooleanPixelShaderConstant;
		m_DynamicState.m_pBooleanPixelShaderConstant = NULL;
	}

	if ( m_DynamicState.m_pIntegerPixelShaderConstant )
	{
		delete[] m_DynamicState.m_pIntegerPixelShaderConstant;
		m_DynamicState.m_pIntegerPixelShaderConstant = NULL;
	}

	if ( m_pDebugTextureList )
	{
		m_DebugTextureListLock.Lock();
		m_pDebugTextureList->deleteThis();
		m_pDebugTextureList = NULL;
		m_DebugTextureListLock.Unlock();
	}
}


void CShaderAPIDx8::ClearStdTextureHandles( void )
{
	for(int i = 0 ; i < ARRAYSIZE( m_StdTextureHandles ) ; i++ )
		m_StdTextureHandles[i] = INVALID_SHADERAPI_TEXTURE_HANDLE;
}


//-----------------------------------------------------------------------------
// FIXME: Remove! Backward compat.
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::OnAdapterSet()
{
	if ( !DetermineHardwareCaps( ) )
		return false;

	// FIXME: Check g_pHardwareConfig->ActualCaps() for a preferred DX level
	OverrideCaps( 0 );

	m_bAdapterSet = true;
	return true;
}


//-----------------------------------------------------------------------------
// Can we download textures?
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::CanDownloadTextures() const
{
	if ( IsDeactivated() )
		return false;

	return IsActive();
}


//-----------------------------------------------------------------------------
// Grab the render targets
//-----------------------------------------------------------------------------
void CShaderAPIDx8::AcquireInternalRenderTargets()
{
	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CShaderAPIDx8::AcquireInternalRenderTargets\n" );
	}

	Assert( m_pBackBufferSurfaces.Count() > BACK_BUFFER_INDEX_DEFAULT );
	if ( m_pBackBufferSurfaces[BACK_BUFFER_INDEX_DEFAULT] == NULL )
	{
		Dx9Device()->GetRenderTarget( 0, &m_pBackBufferSurfaces[BACK_BUFFER_INDEX_DEFAULT] );
		Assert( m_pBackBufferSurfaces[BACK_BUFFER_INDEX_DEFAULT] );
	}

	Assert( m_pBackBufferSurfaces.Count() > BACK_BUFFER_INDEX_HDR );
	if( ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT ) &&
		( m_pBackBufferSurfaces[BACK_BUFFER_INDEX_HDR] == NULL ) )
	{
		// create a float16 HDR rendertarget
		int nWidth, nHeight;
		GetBackBufferDimensions( nWidth, nHeight );

		HRESULT hRes = Dx9Device()->CreateRenderTarget(
			nWidth, 
			nHeight,
			D3DFMT_A16B16G16R16F, 
			m_PresentParameters.MultiSampleType,		// TODO: Check if the HW supports this
			m_PresentParameters.MultiSampleQuality, 
			false,	// Lockable
			&m_pBackBufferSurfaces[BACK_BUFFER_INDEX_HDR], 
			NULL );
		if ( ( hRes != D3D_OK ) || m_pBackBufferSurfaces[BACK_BUFFER_INDEX_HDR]  == NULL )
			hRes = Dx9Device()->CreateRenderTarget(
				nWidth, 
				nHeight,
				D3DFMT_A16B16G16R16F, 
				D3DMULTISAMPLE_NONE,
				0,
				false,	// Lockable
				&m_pBackBufferSurfaces[BACK_BUFFER_INDEX_HDR], 
				NULL );
		Assert( ( hRes == D3D_OK ) && m_pBackBufferSurfaces[BACK_BUFFER_INDEX_HDR] );
	}

#if defined( _X360 )
	if ( !m_pBackBufferSurfaceSRGB )
	{
		// create a SRGB back buffer clone
		int backWidth, backHeight;
		ShaderAPI()->GetBackBufferDimensions( backWidth, backHeight );
		D3DFORMAT backBufferFormat = ImageLoader::ImageFormatToD3DFormat( g_pShaderDevice->GetBackBufferFormat() );
#if defined( CSTRIKE15 )
		// [mariod] - implicit srgb render targets (regular 8888, writes happen in shaders) as opposed to PWL
		m_pBackBufferSurfaceSRGB = g_TextureHeap.AllocRenderTargetSurface( backWidth, backHeight, backBufferFormat, RT_MULTISAMPLE_MATCH_BACKBUFFER, 0 );
#else
		m_pBackBufferSurfaceSRGB = g_TextureHeap.AllocRenderTargetSurface( backWidth, backHeight, (D3DFORMAT)MAKESRGBFMT( backBufferFormat ), RT_MULTISAMPLE_MATCH_BACKBUFFER, 0 );
#endif
	}
#endif

	if ( !m_pZBufferSurface )
	{
		Dx9Device()->GetDepthStencilSurface( &m_pZBufferSurface );
		Assert( m_pZBufferSurface );
		SPEW_REFCOUNT_EXPECTED( m_pZBufferSurface, ==, 2 );
	}
}


//-----------------------------------------------------------------------------
// Release the render targets
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReleaseInternalRenderTargets( )
{
	if( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CShaderAPIDx8::ReleaseInternalRenderTargets\n" );
	}

	// Note: This function does not release renderable textures created elsewhere
	//       Those should be released separately via the texure manager
	FOR_EACH_VEC( m_pBackBufferSurfaces, i )
	{
		if ( m_pBackBufferSurfaces[i] != NULL )
		{
			IDirect3DSurface9* backBufferSurface = m_pBackBufferSurfaces[i];
			m_pBackBufferSurfaces[i] = NULL;

			// Default back buffer may have a reference held by DirectX.
			int nRemainingReferences = backBufferSurface->Release();
			Assert(nRemainingReferences == 0 || i == BACK_BUFFER_INDEX_DEFAULT && nRemainingReferences <= 1);
		}
	}

	if ( m_pZBufferSurface )
	{
#if defined(_WIN32) && !defined( DX_TO_GL_ABSTRACTION )
		if ( m_pNVAPI_registeredDepthStencilSurface != NULL )
		{
			// Unregister old one if there is any
			SPEW_REFCOUNT( m_pNVAPI_registeredDepthStencilSurface );
			NvAPI_D3D9_UnregisterResource( m_pNVAPI_registeredDepthStencilSurface );
			SPEW_REFCOUNT( m_pNVAPI_registeredDepthStencilSurface );
			m_pNVAPI_registeredDepthStencilSurface = NULL;
		}

		if ( m_pNVAPI_registeredDepthTexture != NULL )
		{
			// Unregister old one if there is any
			SPEW_REFCOUNT( m_pNVAPI_registeredDepthTexture );
			NvAPI_D3D9_UnregisterResource( m_pNVAPI_registeredDepthTexture );
			SPEW_REFCOUNT( m_pNVAPI_registeredDepthTexture );
			m_pNVAPI_registeredDepthTexture = NULL;
		}
#endif
		SPEW_REFCOUNT_EXPECTED( m_pZBufferSurface, <=, 2 );
		m_pZBufferSurface->Release();
		m_pZBufferSurface = NULL;
	}
}

//-----------------------------------------------------------------------------
// During init, places the persisted texture back into the back buffer.
// The initial 360 fixup present will then not flash. This routine has to be
// self contained, no other shader api systems are viable during init.
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::RestorePersistedDisplay( bool bUseFrontBuffer )
{
#if defined( _X360 )
	if ( !( XboxLaunch()->GetLaunchFlags() & LF_INTERNALLAUNCH ) )
	{
		// there is no persisted screen
		return false;
	}

	OwnGPUResources( false );

	// HLSL source for the persist shader:
	const char *strVertexShaderProgram = 
		" float4x4 matWVP : register(c0);"  
		" struct VS_IN"  
		" {" 
		" float4 ObjPos : POSITION;"
		" float2 TexCoord : TEXCOORD;"
		" };" 
		" struct VS_OUT" 
		" {" 
		" float4 ProjPos : POSITION;" 
		" float2 TexCoord : TEXCOORD;"
		" };"  
		" VS_OUT main( VS_IN In )"  
		" {"  
		" VS_OUT Out; "  
		" Out.ProjPos = mul( matWVP, In.ObjPos );"
		" Out.TexCoord = In.TexCoord;"
		" return Out;"  
		" }";

	const char *strPixelShaderProgram = 
		" struct PS_IN"
		" {"
		" float2 TexCoord : TEXCOORD;"
		" };"
		" sampler detail : register( s0 );"
		" float4 main( PS_IN In ) : COLOR"  
		" {"  
		" return tex2D( detail, In.TexCoord );"
		" }"; 
 
	// Hard-coded compiled shader data, so we don't have to bloat our DLLs
	// with all the shader compilation stuff from the D3D libs (over 2 MB!)
#if defined( _X360 ) && ( _XDK_VER != 20764 )
	// Make sure this hard-coded shader data gets updated with each XDK rev
#if !defined( JUNE_2009_XDK_ISSUES )
#error "Define X360_LINK_WITH_SHADER_COMPILE temporarily (to verify shader data and spew out new data if necessary)"
#endif
#endif

	// As of L4D ship, 80 DWORDS
	DWORD vertexShaderData[] = {
		0x102a1101, 0x000000bc, 0x00000084, 0x00000000, 0x00000024, 0x00000000, 0x00000084, 0x00000000, 
		0x00000000, 0x0000005c, 0x0000001c, 0x0000004f, 0xfffe0300, 0x00000001, 0x0000001c, 0x00000000, 
		0x00000048, 0x00000030, 0x00020000, 0x00040000, 0x00000038, 0x00000000, 0x6d617457, 0x565000ab, 
		0x00030003, 0x00040004, 0x00010000, 0x00000000, 0x76735f33, 0x5f300032, 0x2e302e32, 0x30373634, 
		0x2e3000ab, 0x00000000, 0x00000084, 0x00010002, 0x00000000, 0x00000000, 0x00000821, 0x00000001, 
		0x00000002, 0x00000001, 0x00000290, 0x00100003, 0x00305004, 0x00003050, 0x00001009, 0x30052003, 
		0x00001200, 0xc2000000, 0x00004005, 0x00001200, 0xc4000000, 0x00001009, 0x00002200, 0x00000000, 
		0x05f82000, 0x00000688, 0x00000000, 0x05f80000, 0x00000fc8, 0x00000000, 0xc80f0001, 0x001b8800, 
		0xa1020300, 0xc80f0001, 0x00c68800, 0xab020201, 0xc80f0001, 0x00b13494, 0xab020101, 0xc80f803e, 
		0x006c0034, 0xab020001, 0xc8038000, 0x00b0b000, 0xe2000000, 0x00000000, 0x00000000, 0x00000000, 
	};


	// As of L4D ship, 57 DWORDS
	DWORD pixelShaderData1[] = {     
		0x102a1100, 0x000000a8, 0x0000003c, 0x00000000, 0x00000024, 0x00000000, 0x00000084, 0x00000000, 
		0x00000000, 0x0000005c, 0x0000001c, 0x0000004f, 0xffff0300, 0x00000001, 0x0000001c, 0x00000000, 
		0x00000048, 0x00000030, 0x00030000, 0x00010000, 0x00000038, 0x00000000, 0x64657461, 0x696c00ab, 
		0x0004000c, 0x00010001, 0x00010000, 0x00000000, 0x70735f33, 0x5f300032, 0x2e302e32, 0x30373634, 
		0x2e3000ab, 0x00000000, 0x0000003c, 0x10000000, 0x00000004, 0x00000000, 0x00000821, 0x00010001, 
		0x00000001, 0x00003050, 0x00011002, 0x00001200, 0xc4000000, 0x00001003, 0x00002200, 0x00000000, 
		0x10080001, 0x1f1ff688, 0x00004000, 0xc80f8000, 0x00000000, 0xe2000000, 0x00000000, 0x00000000, 
		0x00000000, 
	};

	D3DVERTEXELEMENT9 VertexElements[3] =
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};


	IDirect3DTexture *pTexture;
	if ( bUseFrontBuffer )
	{
		Dx9Device()->GetFrontBuffer( &pTexture );
	}
	else
	{
		// 360 holds a persistent image across restarts
		Dx9Device()->GetPersistedTexture( &pTexture );
	}

	// Build the shaders to do this rendering (fixed-function pipeline FTW :o/ )
	IDirect3DVertexShader9 *pVertexShader;
	if ( !BuildStaticShader( true, (void **)&pVertexShader, "vertex shader",
							 strVertexShaderProgram, vertexShaderData, sizeof( vertexShaderData ) ) )
		return false;

	IDirect3DPixelShader9 *pPixelShader;
	if ( !BuildStaticShader( false, (void **)&pPixelShader, "pixel shader",
							 strPixelShaderProgram, pixelShaderData1, sizeof( pixelShaderData1 ) ) )
		return false;

	int w, h;
	GetBackBufferDimensions( w, h );

	// Create a vertex declaration from the element descriptions.
	IDirect3DVertexDeclaration9 *pVertexDecl;
	Dx9Device()->CreateVertexDeclaration( VertexElements, &pVertexDecl );
	XMMATRIX matWVP = XMMatrixOrthographicOffCenterLH( 0, (FLOAT)w, (FLOAT)h, 0, 0, 1 );

	ConVarRef mat_monitorgamma( "mat_monitorgamma" );
	ConVarRef mat_monitorgamma_tv_range_min( "mat_monitorgamma_tv_range_min" );
	ConVarRef mat_monitorgamma_tv_range_max( "mat_monitorgamma_tv_range_max" );
	ConVarRef mat_monitorgamma_tv_exp( "mat_monitorgamma_tv_exp" );
	ConVarRef mat_monitorgamma_tv_enabled( "mat_monitorgamma_tv_enabled" );
	g_pShaderDeviceDx8->SetHardwareGammaRamp( mat_monitorgamma.GetFloat(), mat_monitorgamma_tv_range_min.GetFloat(), mat_monitorgamma_tv_range_max.GetFloat(),
		mat_monitorgamma_tv_exp.GetFloat(), mat_monitorgamma_tv_enabled.GetBool() );

	// Structure to hold vertex data.
	struct COLORVERTEX
	{
		FLOAT       Position[3];
		float       TexCoord[2];
	};
	COLORVERTEX Vertices[4];

	Vertices[0].Position[0] = 0;
	Vertices[0].Position[1] = 0;
	Vertices[0].Position[2] = 0;
	Vertices[0].TexCoord[0] = 0;
	Vertices[0].TexCoord[1] = 0;

	Vertices[1].Position[0] = w-1;
	Vertices[1].Position[1] = 0;
	Vertices[1].Position[2] = 0;
	Vertices[1].TexCoord[0] = 1;
	Vertices[1].TexCoord[1] = 0;

	Vertices[2].Position[0] = w-1;
	Vertices[2].Position[1] = h-1;
	Vertices[2].Position[2] = 0;
	Vertices[2].TexCoord[0] = 1;
	Vertices[2].TexCoord[1] = 1;

	Vertices[3].Position[0] = 0;
	Vertices[3].Position[1] = h-1;
	Vertices[3].Position[2] = 0;
	Vertices[3].TexCoord[0] = 0;
	Vertices[3].TexCoord[1] = 1;

	Dx9Device()->SetTexture( 0, pTexture );
	Dx9Device()->SetVertexShader( pVertexShader );
	Dx9Device()->SetPixelShader( pPixelShader );
	Dx9Device()->SetVertexShaderConstantF( 0, (FLOAT*)&matWVP, 4 );
	Dx9Device()->SetVertexDeclaration( pVertexDecl );
	Dx9Device()->DrawPrimitiveUP( D3DPT_QUADLIST, 1, Vertices, sizeof( COLORVERTEX ) );

	Dx9Device()->SetVertexShader( NULL );
	Dx9Device()->SetPixelShader( NULL );
	Dx9Device()->SetTexture( 0, NULL );
	Dx9Device()->SetVertexDeclaration( NULL );

	pVertexShader->Release();
	pPixelShader->Release();
	pVertexDecl->Release();
	pTexture->Release();

	OwnGPUResources( true );

	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Initialize, shutdown the Device....
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::OnDeviceInit() 
{
	AcquireInternalRenderTargets();
	
	g_pHardwareConfig->CapsForEdit().m_TextureMemorySize = g_pShaderDeviceMgrDx8->GetVidMemBytes( m_nAdapter );

	CreateMatrixStacks();

	// Hide the cursor
	RECORD_COMMAND( DX8_SHOW_CURSOR, 1 );
	RECORD_INT( false );

#if !defined( _X360 )
	Dx9Device()->ShowCursor( false );
#endif

	// Initialize the shader manager
	ShaderManager()->Init();

	// Initialize the shader shadow
	ShaderShadow()->Init();

	// Initialize the mesh manager
	MeshMgr()->Init();

	m_bToolsMode = IsPlatformWindows() && ( CommandLine()->CheckParm( "-tools" ) != NULL );

	// Use fat vertices when running in tools
	MeshMgr()->UseFatVertices( m_bToolsMode );

	// Initialize the transition table.
	m_TransitionTable.Init();

	// Initialize the render state
	InitRenderState();

	// Clear the z and color buffers
	ClearBuffers( true, true, true, -1, -1 );

	AllocFrameSyncObjects();
	AllocNonInteractiveRefreshObjects();

	RECORD_COMMAND( DX8_BEGIN_SCENE, 0 );

	// Apply mandatory initialization HW fixups, GPU state will be left as expected
	if ( IsX360() )
	{
		// place the possible persisted display into the back buffer, ready for present()
		RestorePersistedDisplay( false );

		// 360 MUST perform an initial swap to stabilize the state
		// this ensures any states (e.g. gamma) are respected
		// without this, the 360 resets to internal default state on the first swap
		OwnGPUResources( false );
		Dx9Device()->Present( 0, 0, 0, 0 );

		// present corrupts the GPU state and back buffer (according to docs)
		// re-clear the back buffer in order to re-establish the expected contents
		ResetRenderState( false );
		ClearBuffers( true, true, true, -1, -1 );

		// place the front buffer image in the back buffer, later systems will detect and grab
		// other systems will detect and grab
		RestorePersistedDisplay( true );

#ifdef PLATFORM_X360
		if ( m_PresentParameters.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE )
		{
			// Ensure our custom present immediate threshold is set.
			EnableVSync_360( true );
		}
#endif
	}

	Dx9Device()->BeginScene();

	return true;
}

void CShaderAPIDx8::OnDeviceShutdown() 
{
	if ( IsX360() || !IsActive() )
		return;

	// Deallocate all textures
	DeleteAllTextures();

	// Release render targets
	ReleaseInternalRenderTargets();

	// Free objects that are used for frame syncing.
	FreeFrameSyncObjects();
	FreeNonInteractiveRefreshObjects();

	for (int i = 0; i < NUM_MATRIX_MODES; ++i)
	{
		if (m_pMatrixStack[i])
		{
			int ref = m_pMatrixStack[i]->Release();
			Assert( ref == 0 );
		}
	}

	// Shutdown the transition table.
	m_TransitionTable.Shutdown();

	MeshMgr()->Shutdown();

	ShaderManager()->Shutdown();

	ReleaseAllVertexDecl( );
}


//-----------------------------------------------------------------------------
// Sets the mode...
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::SetMode( void* VD3DHWND, int nAdapter, const ShaderDeviceInfo_t &info )
{
	//
	// FIXME: Note that this entire function is backward compat and will go soon
	//

	bool bRestoreNeeded = false;

	if ( IsActive() )
	{
		ReleaseResources();
		OnDeviceShutdown();
		ShutdownDevice();
		bRestoreNeeded = true;
	}

	LOCK_SHADERAPI();
	Assert( D3D() );
	Assert( nAdapter < g_pShaderDeviceMgr->GetAdapterCount() );

	const HardwareCaps_t& actualCaps = g_pShaderDeviceMgr->GetHardwareCaps( nAdapter );

	ShaderDeviceInfo_t actualInfo = info;
	int nDXLevel = actualInfo.m_nDXLevel ? actualInfo.m_nDXLevel : actualCaps.m_nDXSupportLevel;
	if ( nDXLevel > actualCaps.m_nMaxDXSupportLevel )
	{
		nDXLevel = actualCaps.m_nMaxDXSupportLevel;
	}
	actualInfo.m_nDXLevel = g_pShaderDeviceMgr->GetClosestActualDXLevel( nDXLevel );

	if ( !g_pShaderDeviceMgrDx8->ValidateMode( nAdapter, actualInfo ) )
		return false;

	g_pShaderAPI = this;
	g_pShaderDevice = this;
	g_pShaderShadow = ShaderShadow();
	bool bOk = InitDevice( VD3DHWND, nAdapter, actualInfo );
	if ( !bOk )
		return false;

	if ( !OnDeviceInit() )
		return false;

	if ( bRestoreNeeded && IsPC() )
	{
		ReacquireResources();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Creates the matrix stack
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CreateMatrixStacks()
{
	MEM_ALLOC_D3D_CREDIT();

	for (int i = 0; i < NUM_MATRIX_MODES; ++i)
	{
		HRESULT hr = D3DXCreateMatrixStack( 0, &m_pMatrixStack[i] );
		Assert( hr == D3D_OK );
	}
}


// Use this when recording *.rec files that need to be run on more than one kind of 
// hardware, etc.
//#define DX8_COMPATABILITY_MODE


//-----------------------------------------------------------------------------
// Vendor-dependent code to turn on alpha to coverage
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableAlphaToCoverage( void )
{
	if( !g_pHardwareConfig->ActualCaps().m_bSupportsAlphaToCoverage || !IsAAEnabled() )
		return;

	if ( IsPC() && ( m_PresentParameters.MultiSampleType < D3DMULTISAMPLE_4_SAMPLES ) )
		return;

	D3DRENDERSTATETYPE renderState = (D3DRENDERSTATETYPE)g_pHardwareConfig->Caps().m_AlphaToCoverageState;
	SetRenderState( renderState, g_pHardwareConfig->Caps().m_AlphaToCoverageEnableValue );	// Vendor dependent state
}

//-----------------------------------------------------------------------------
// Vendor-dependent code to turn off alpha to coverage
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DisableAlphaToCoverage()
{
	if( !g_pHardwareConfig->ActualCaps().m_bSupportsAlphaToCoverage || !IsAAEnabled() )
		return;

	if ( IsPC() && ( m_PresentParameters.MultiSampleType < D3DMULTISAMPLE_4_SAMPLES ) )
		return;

	D3DRENDERSTATETYPE renderState = (D3DRENDERSTATETYPE)g_pHardwareConfig->Caps().m_AlphaToCoverageState;
	SetRenderState( renderState, g_pHardwareConfig->Caps().m_AlphaToCoverageDisableValue );	// Vendor dependent state
}

//-----------------------------------------------------------------------------
// Determine capabilities
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::DetermineHardwareCaps( )
{
	HardwareCaps_t& actualCaps = g_pHardwareConfig->ActualCapsForEdit();
	if ( !g_pShaderDeviceMgrDx8->ComputeCapsFromD3D( &actualCaps, m_DisplayAdapter ) )
		return false;

	// See if the file tells us otherwise
	g_pShaderDeviceMgrDx8->ReadDXSupportLevels( actualCaps );

	// Read dxsupport.cfg which has config overrides for particular cards.
	g_pShaderDeviceMgrDx8->ReadHardwareCaps( actualCaps, actualCaps.m_nMaxDXSupportLevel );

	// What's in "-shader" overrides dxsupport.cfg
	const char *pShaderParam = CommandLine()->ParmValue( "-shader" );
	if ( pShaderParam )
	{
		Q_strncpy( actualCaps.m_pShaderDLL, pShaderParam, sizeof( actualCaps.m_pShaderDLL ) );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Override caps based on a particular dx level
//-----------------------------------------------------------------------------
void CShaderAPIDx8::OverrideCaps( int nForcedDXLevel )
{
	// Just use the actual caps if we can't use what was requested or if the default is requested
	if ( nForcedDXLevel <= 0 ) 
	{
		nForcedDXLevel = g_pHardwareConfig->ActualCaps().m_nDXSupportLevel;
	}
	nForcedDXLevel = g_pShaderDeviceMgr->GetClosestActualDXLevel( nForcedDXLevel );

	g_pHardwareConfig->SetupHardwareCaps( nForcedDXLevel, g_pHardwareConfig->ActualCaps() );
}


//-----------------------------------------------------------------------------
// Called when the dx support level has changed
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DXSupportLevelChanged( int nDXLevel )
{
	LOCK_SHADERAPI();
	if ( IsPC() )
	{
		OverrideCaps( nDXLevel );
	}
	else
	{
		Assert( 0 );
	}
}


//-----------------------------------------------------------------------------
// FIXME: Remove! Backward compat only
//-----------------------------------------------------------------------------
int CShaderAPIDx8::GetActualSamplerCount() const
{
	return g_pHardwareConfig->GetActualSamplerCount();
}

int CShaderAPIDx8::StencilBufferBits() const
{
	return m_bUsingStencil ? m_iStencilBufferBits : 0;
}

bool CShaderAPIDx8::IsAAEnabled() const
{
	bool bAntialiasing = ( m_PresentParameters.MultiSampleType != D3DMULTISAMPLE_NONE );
	return bAntialiasing;
}


//-----------------------------------------------------------------------------
// Methods related to queuing functions to be called per-(pMesh->Draw call) or per-pass
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsCommitFuncInUse( CommitFuncType_t func, int nFunc ) const
{
	Assert( nFunc < COMMIT_FUNC_COUNT );
	return ( m_pCommitFlags[func][ nFunc >> 3 ] & ( 1 << ( nFunc & 0x7 ) ) ) != 0;
}

void CShaderAPIDx8::MarkCommitFuncInUse( CommitFuncType_t func, int nFunc )
{
	m_pCommitFlags[func][ nFunc >> 3 ] |= 1 << ( nFunc & 0x7 );
}

void CShaderAPIDx8::AddCommitFunc( CommitFuncType_t func, StateCommitFunc_t f )
{
	m_CommitFuncs[func].AddToTail( f );
}


//-----------------------------------------------------------------------------
// Clears all commit functions
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ClearAllCommitFuncs( CommitFuncType_t func )
{
	memset( m_pCommitFlags[func], 0, COMMIT_FUNC_BYTE_COUNT );
	m_CommitFuncs[func].RemoveAll();
}


//-----------------------------------------------------------------------------
// Calls all commit functions in a particular list
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CallCommitFuncs( CommitFuncType_t func, bool bForce )
{
	// Don't bother committing anything if we're deactivated
	if ( IsDeactivated() )
		return;

	CUtlVector< StateCommitFunc_t > &funcList =	m_CommitFuncs[func];
	int nCount = funcList.Count();
	if ( nCount == 0 )
		return;

	for ( int i = 0; i < nCount; ++i )
	{
		funcList[i]( Dx9Device(), m_DesiredState, m_DynamicState, bForce );
	}
	ClearAllCommitFuncs( func );
}


//-----------------------------------------------------------------------------
// Sets the sampler state
//-----------------------------------------------------------------------------
static inline void SetSamplerState( D3DDeviceWrapper *pDevice, int stage, D3DSAMPLERSTATETYPE state, DWORD val )
{
	RECORD_SAMPLER_STATE( stage, state, val ); 

#if defined( _X360 )
	if ( state == D3DSAMP_NOTSUPPORTED )
		return;
#endif

	pDevice->SetSamplerState( stage, state, val );
}

inline void CShaderAPIDx8::SetSamplerState( int stage, D3DSAMPLERSTATETYPE state, DWORD val )
{
#ifndef DX_TO_GL_ABSTRACTION
	if ( IsDeactivated() )
		return;
#endif

	::SetSamplerState( Dx9Device(), stage, state, val );
}

//-----------------------------------------------------------------------------
// Sets the texture stage state
//-----------------------------------------------------------------------------
inline void CShaderAPIDx8::SetRenderState( D3DRENDERSTATETYPE state, DWORD val )
{
#if ( !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION ) )
	{
		if ( IsDeactivated() )
			return;
	}
#	else
	{
		Assert( state != D3DRS_NOTSUPPORTED ); //Use SetSupportedRenderState() macro to avoid this at compile time
		//if ( state == D3DRS_NOTSUPPORTED )
		//	return;
	}
#	endif

	Assert( state >= 0 && ( int )state < MAX_NUM_RENDERSTATES );
	if ( m_DynamicState.m_RenderState[state] != val )
	{
#ifdef DX_TO_GL_ABSTRACTION
		Dx9Device()->SetRenderStateInline( state, val );
#else
		Dx9Device()->SetRenderState( state, val );
#endif
		m_DynamicState.m_RenderState[state] = val;
	}
}

#ifdef DX_TO_GL_ABSTRACTION
// Purposely always writing the new state (even if it's not changed), in case SetRenderStateConstInline() compiles away to nothing (it sometimes does)
#define SetRenderStateConstMacro(t, state, val ) do { Assert( state >= 0 && ( int )state < MAX_NUM_RENDERSTATES ); if ( t->m_DynamicState.m_RenderState[state] != (DWORD)val ) { Dx9Device()->SetRenderStateConstInline( state, val ); } t->m_DynamicState.m_RenderState[state] = val; } while(0)
#else
#define SetRenderStateConstMacro(t, state, val ) t->SetRenderState( state, val );
#endif

inline void CShaderAPIDx8::SetScreenSizeForVPOS( int pshReg /* = 32 */)
{
	int nWidth, nHeight;
	ITexture *pTexture = ShaderAPI()->GetRenderTargetEx( 0 );
	if ( pTexture == NULL )
	{
		ShaderAPI()->GetBackBufferDimensions( nWidth, nHeight );
	}
	else
	{
		nWidth  = pTexture->GetActualWidth();
		nHeight = pTexture->GetActualHeight();
	}

	// Set constant to enable translation of VPOS to render target coordinates in ps_3_0
	float vScreenSize[4] = { 1.0f/(float)nWidth, 1.0f/(float)nHeight, 0.5f/(float)nWidth, 0.5f/(float)nHeight };

	SetPixelShaderConstantInternal( pshReg, vScreenSize, 1, true );
}


inline void CShaderAPIDx8::SetVSNearAndFarZ( int vshReg )
{
	VMatrix m;
	GetMatrix( MATERIAL_PROJECTION, m.m[0] );

	// m[2][2] =  F/(N-F)   (flip sign if RH)
	// m[3][2] = NF/(N-F)

	float vNearFar[4];

	float N =     m[3][2] / m[2][2];
	float F = (m[3][2]*N) / (N + m[3][2]);

	vNearFar[0] = N;
	vNearFar[1] = F;


	SetVertexShaderConstant( vshReg, vNearFar, 1 );
}

inline float CShaderAPIDx8::GetFarZ()
{
	VMatrix m;
	GetMatrix( MATERIAL_PROJECTION, m.m[0] );	// See above for the algebra here
	float N =     m[3][2] / m[2][2];
	return (m[3][2]*N) / (N + m[3][2]);
}


void CShaderAPIDx8::EnableSinglePassFlashlightMode( bool bEnable )
{
	m_bSinglePassFlashlightMode = bEnable;
}

bool CShaderAPIDx8::SinglePassFlashlightModeEnabled( void )
{
	return IsX360() || IsPS3() || m_bSinglePassFlashlightMode; //360/PS3 only supports single pass flashlights
}


//-----------------------------------------------------------------------------
// Commits viewports
//-----------------------------------------------------------------------------
static void CommitSetScissorRect( D3DDeviceWrapper *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce )
{
	// Set the enable/disable renderstate

	bool bEnableChanged = desiredState.m_RenderState[D3DRS_SCISSORTESTENABLE] != currentState.m_RenderState[D3DRS_SCISSORTESTENABLE];
	if ( bEnableChanged )
	{
		Dx9Device()->SetRenderState( D3DRS_SCISSORTESTENABLE, desiredState.m_RenderState[D3DRS_SCISSORTESTENABLE] );
		currentState.m_RenderState[D3DRS_SCISSORTESTENABLE] = desiredState.m_RenderState[D3DRS_SCISSORTESTENABLE];
	}

	// Only bother with the rect if we're enabling
	if ( desiredState.m_RenderState[D3DRS_SCISSORTESTENABLE] )
	{
		int nWidth, nHeight;
		ITexture *pTexture = ShaderAPI()->GetRenderTargetEx( 0 );
		if ( pTexture == NULL )
		{
			ShaderAPI()->GetBackBufferDimensions( nWidth, nHeight );
		}
		else
		{
			nWidth  = pTexture->GetActualWidth();
			nHeight = pTexture->GetActualHeight();
		}

		Assert( (desiredState.m_ScissorRect.left <= nWidth) && (desiredState.m_ScissorRect.bottom <= nHeight) &&
			    ( desiredState.m_ScissorRect.top >= 0 ) && (desiredState.m_ScissorRect.left >= 0) );

		Dx9Device()->SetScissorRect( &desiredState.m_ScissorRect );
		currentState.m_ScissorRect = desiredState.m_ScissorRect;
	}
}

// Routine for setting scissor rect
// If pScissorRect is NULL, disable scissoring by setting the render state
// If pScissorRect is non-NULL, set the RECT state in Direct3D AND set the renderstate
inline void CShaderAPIDx8::SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor )
{
	Assert( (nLeft <= nRight) && (nTop <= nBottom) ); //360 craps itself if this isn't true
	if ( !g_pHardwareConfig->Caps().m_bScissorSupported )
		return;

	DWORD dwEnableScissor = bEnableScissor ? TRUE : FALSE;
	RECT newScissorRect;
	newScissorRect.left = nLeft;
	newScissorRect.top = nTop;
	newScissorRect.right = nRight;
	newScissorRect.bottom = nBottom;

	// If we're turning it on, check the validity of the rect
	if ( bEnableScissor )
	{
		int nWidth, nHeight;
		ITexture *pTexture = GetRenderTargetEx( 0 );
		if ( pTexture == NULL )
		{
			GetBackBufferDimensions( nWidth, nHeight );
		}
		else
		{
			nWidth  = pTexture->GetActualWidth();
			nHeight = pTexture->GetActualHeight();
		}

		Assert( (nRight <= nWidth) && (nBottom <= nHeight) && ( nTop >= 0 ) && (nLeft >= 0) );

		newScissorRect.left   = clamp( newScissorRect.left,   0, nWidth );
		newScissorRect.top    = clamp( newScissorRect.top,    0, nHeight );
		newScissorRect.right  = clamp( newScissorRect.right,  0, nWidth );
		newScissorRect.bottom = clamp( newScissorRect.bottom, 0, nHeight );
	}

	if ( !m_bResettingRenderState )
	{
		bool bEnableChanged = m_DesiredState.m_RenderState[D3DRS_SCISSORTESTENABLE] != dwEnableScissor;
		bool bRectChanged = memcmp( &newScissorRect, &m_DesiredState.m_ScissorRect, sizeof(RECT) ) != 0;

		if ( !bEnableChanged && !bRectChanged )
			return;
	}

	m_DesiredState.m_RenderState[D3DRS_SCISSORTESTENABLE] = dwEnableScissor;
	memcpy( &m_DesiredState.m_ScissorRect, &newScissorRect, sizeof(RECT) );

	ADD_COMMIT_FUNC( COMMIT_PER_DRAW, CommitSetScissorRect );
}

inline void CShaderAPIDx8::SetRenderStateForce( D3DRENDERSTATETYPE state, DWORD val )
{
#if ( !defined( _GAMECONSOLE ) && !defined( DX_TO_GL_ABSTRACTION ) )

	{
		if ( IsDeactivated() )   // e.g. if the window minimized, don't set any render states
			return;
	}
#	else
	{
		Assert( state != D3DRS_NOTSUPPORTED ); //Use SetSupportedRenderStateForce() macro to avoid this at compile time
		//if ( state == D3DRS_NOTSUPPORTED )
		//	return;
	}
#	endif

	Dx9Device()->SetRenderState( state, val );
	m_DynamicState.m_RenderState[state] = val;
}


//-----------------------------------------------------------------------------
// Set the values for pixel shader constants that don't change.
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetStandardVertexShaderConstants( float fOverbright )
{
	LOCK_SHADERAPI();

	// Set a couple standard constants....
	Vector4D standardVertexShaderConstant( 0.0f, 1.0f, 2.0f, 0.5f );
	SetVertexShaderConstantInternal( VERTEX_SHADER_MATH_CONSTANTS0, standardVertexShaderConstant.Base(), 1 );

	// [ gamma, overbright, 1/3, 1/overbright ]
	standardVertexShaderConstant.Init( 1.0f/2.2f, fOverbright, 1.0f / 3.0f, 1.0f / fOverbright );
	SetVertexShaderConstantInternal( VERTEX_SHADER_MATH_CONSTANTS1, standardVertexShaderConstant.Base(), 1 );

#if 0
	int nModelIndex = VERTEX_SHADER_MODEL;

	// These point to the lighting and the transforms
	standardVertexShaderConstant.Init( 
		VERTEX_SHADER_LIGHTS,
		VERTEX_SHADER_LIGHTS + 5, 
        // Use COLOR instead of UBYTE4 since Geforce3 does not support it
        // vConst.w should be 3, but due to about hack, mul by 255 and add epsilon
		// 360 supports UBYTE4, so no fixup required
		(IsPC() || !IsX360()) ? 765.01f : 3.0f,
		 nModelIndex );	// DX8 has different constant packing
#endif

	SetVertexShaderConstantInternal( VERTEX_SHADER_LIGHT_INDEX, standardVertexShaderConstant.Base(), 1 );

	/*
	if ( g_pHardwareConfig->Caps().m_SupportsVertexShaders_3_0 )
	{
		Vector4D factors[4];
		factors[0].Init( 1, 0, 0, 0 );
		factors[1].Init( 0, 1, 0, 0 );
		factors[2].Init( 0, 0, 1, 0 );
		factors[3].Init( 0, 0, 0, 1 );
		SetVertexShaderConstantInternal( VERTEX_SHADER_DOT_PRODUCT_FACTORS, factors[0].Base(), 4 );
	}
*/

	float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	SetVertexShaderConstantInternal( VERTEX_SHADER_FLEXSCALE, c, 1 );
}

//-----------------------------------------------------------------------------
// Initialize vertex and pixel shaders
//-----------------------------------------------------------------------------
void CShaderAPIDx8::InitVertexAndPixelShaders()
{
	bool bPreviousState;
	if ( IsX360() )
	{
		// to init/update all constants now, must release ownership
		bPreviousState = OwnGPUResources( false );
	}

	// Allocate space for the pixel and vertex shader constants...
	// pixel shaders
	{
#ifdef _PS3
		m_DynamicState.m_pVectorPixelShaderConstant = NULL;
		m_DesiredState.m_pVectorPixelShaderConstant = NULL;
#else
		if (m_DynamicState.m_pVectorPixelShaderConstant)
		{
			delete[] m_DynamicState.m_pVectorPixelShaderConstant;
		}
		m_DynamicState.m_pVectorPixelShaderConstant = new Vector4D[g_pHardwareConfig->Caps().m_NumPixelShaderConstants];

		if (m_DesiredState.m_pVectorPixelShaderConstant)
		{
			delete[] m_DesiredState.m_pVectorPixelShaderConstant;
		}
		m_DesiredState.m_pVectorPixelShaderConstant = new Vector4D[g_pHardwareConfig->Caps().m_NumPixelShaderConstants];
#endif

		if (m_DynamicState.m_pBooleanPixelShaderConstant)
		{
			delete[] m_DynamicState.m_pBooleanPixelShaderConstant;
		}
		m_DynamicState.m_pBooleanPixelShaderConstant = new BOOL[g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants];

		if (m_DesiredState.m_pBooleanPixelShaderConstant)
		{
			delete[] m_DesiredState.m_pBooleanPixelShaderConstant;
		}
		m_DesiredState.m_pBooleanPixelShaderConstant = new BOOL[g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants];
	
		if (m_DynamicState.m_pIntegerPixelShaderConstant)
		{
			delete[] m_DynamicState.m_pIntegerPixelShaderConstant;
		}
		m_DynamicState.m_pIntegerPixelShaderConstant = new IntVector4D[g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants];

		if (m_DesiredState.m_pIntegerPixelShaderConstant)
		{
			delete[] m_DesiredState.m_pIntegerPixelShaderConstant;
		}
		m_DesiredState.m_pIntegerPixelShaderConstant = new IntVector4D[g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants];

		// force reset vector pixel constants
		int i;
#ifdef _PS3
// 		if ( int numResetBytes = g_pHardwareConfig->Caps().m_NumPixelShaderConstants * sizeof( float ) * 4 )
// 		{
// 			float *pResetPixelShaderConstants = ( float * ) stackalloc( numResetBytes );
// 			memset( pResetPixelShaderConstants, 0, numResetBytes );
// 			SetPixelShaderConstantInternal( 0, pResetPixelShaderConstants, g_pHardwareConfig->Caps().m_NumPixelShaderConstants, true );
// 		}

		gpGcmDrawState->m_dirtyStatesMask |= CGcmDrawState::kDirtyZeroAllPSConsts;

#else
		for ( i = 0; i < g_pHardwareConfig->Caps().m_NumPixelShaderConstants; ++i )
		{
			m_DesiredState.m_pVectorPixelShaderConstant[i].Init();
		}
		SetPixelShaderConstantInternal( 0, m_DesiredState.m_pVectorPixelShaderConstant[0].Base(), g_pHardwareConfig->Caps().m_NumPixelShaderConstants, true );
#endif

		// force reset boolean pixel constants
		int nNumBooleanPixelShaderConstants = g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants;
		if ( nNumBooleanPixelShaderConstants )
		{
			for ( i = 0; i < nNumBooleanPixelShaderConstants; ++i )
			{
				m_DesiredState.m_pBooleanPixelShaderConstant[i] = 0;
			}
			SetBooleanPixelShaderConstant( 0, m_DesiredState.m_pBooleanPixelShaderConstant, nNumBooleanPixelShaderConstants, true );
		}

		// force reset integer pixel constants
		int nNumIntegerPixelShaderConstants = g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants;
		if ( nNumIntegerPixelShaderConstants )
		{
			for ( i = 0; i < nNumIntegerPixelShaderConstants; ++i )
			{
				m_DesiredState.m_pIntegerPixelShaderConstant[i].Init();
			}
			SetIntegerPixelShaderConstant( 0, m_DesiredState.m_pIntegerPixelShaderConstant[0].Base(), nNumIntegerPixelShaderConstants, true );
		}
	}
	
	// vertex shaders
	{
		if (m_DynamicState.m_pVectorVertexShaderConstant)
		{
			delete[] m_DynamicState.m_pVectorVertexShaderConstant;
		}
		m_DynamicState.m_pVectorVertexShaderConstant = new Vector4D[g_pHardwareConfig->Caps().m_NumVertexShaderConstants];

		if (m_DesiredState.m_pVectorVertexShaderConstant)
		{
			delete[] m_DesiredState.m_pVectorVertexShaderConstant;
		}
		m_DesiredState.m_pVectorVertexShaderConstant = new Vector4D[g_pHardwareConfig->Caps().m_NumVertexShaderConstants];

		if (m_DynamicState.m_pBooleanVertexShaderConstant)
		{
			delete[] m_DynamicState.m_pBooleanVertexShaderConstant;
		}
		m_DynamicState.m_pBooleanVertexShaderConstant = new BOOL[g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants];

		if (m_DesiredState.m_pBooleanVertexShaderConstant)
		{
			delete[] m_DesiredState.m_pBooleanVertexShaderConstant;
		}
		m_DesiredState.m_pBooleanVertexShaderConstant = new BOOL[g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants];

		if (m_DynamicState.m_pIntegerVertexShaderConstant)
		{
			delete[] m_DynamicState.m_pIntegerVertexShaderConstant;
		}
		m_DynamicState.m_pIntegerVertexShaderConstant = new IntVector4D[g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants];

		if (m_DesiredState.m_pIntegerVertexShaderConstant)
		{
			delete[] m_DesiredState.m_pIntegerVertexShaderConstant;
		}
		m_DesiredState.m_pIntegerVertexShaderConstant = new IntVector4D[g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants];

		// force reset vector vertex constants

#ifdef _PS3
		int i;
		memset(m_DesiredState.m_pVectorVertexShaderConstant[0].Base(), 0,
			  g_pHardwareConfig->Caps().m_NumVertexShaderConstants * sizeof(vec_float4));
		
		gpGcmDrawState->m_dirtyStatesMask |= CGcmDrawState::kDirtyZeroAllVSConsts;
#else

		int i;
		for ( i = 0; i < g_pHardwareConfig->Caps().m_NumVertexShaderConstants; ++i )
		{
			m_DesiredState.m_pVectorVertexShaderConstant[i].Init();
		}
		SetVertexShaderConstantInternal( 0, m_DesiredState.m_pVectorVertexShaderConstant[0].Base(), g_pHardwareConfig->Caps().m_NumVertexShaderConstants, true );
#endif
		// force reset boolean vertex constants
		for ( i = 0; i < g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants; ++i )
		{
			m_DesiredState.m_pBooleanVertexShaderConstant[i] = 0;
		}
		SetBooleanVertexShaderConstant( 0, m_DesiredState.m_pBooleanVertexShaderConstant, g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants, true );

		if ( !IsPS3() )
		{
			// force reset integer vertex constants
			for ( i = 0; i < g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants; ++i )
			{
				m_DesiredState.m_pIntegerVertexShaderConstant[i].Init();
			}
			SetIntegerVertexShaderConstant( 0, m_DesiredState.m_pIntegerVertexShaderConstant[0].Base(), g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants, true );
		}
	}

	if ( IsX360() )
	{
		// to init/update all constants, must disable ownership
		bool bPreviousState = OwnGPUResources( false );
		WriteShaderConstantsToGPU();
		OwnGPUResources( bPreviousState );
	}

	SetStandardVertexShaderConstants( OVERBRIGHT );

	// Set up the vertex and pixel shader stuff
	ShaderManager()->ResetShaderState();
}


//-----------------------------------------------------------------------------
// Initialize render state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::InitRenderState()
{ 
	// Set the default shadow state
	g_pShaderShadowDx8->SetDefaultState();

	// Grab a snapshot of this state; we'll be using it to set the board
	// state to something well defined.
	m_TransitionTable.TakeDefaultStateSnapshot();

#ifdef _GAMECONSOLE
	m_zPassSnapshot = m_TransitionTable.TakeSnapshot();
#endif

	if ( !IsDeactivated() )
	{
		ResetRenderState();
	}
}


//-----------------------------------------------------------------------------
// Commits vertex textures
//-----------------------------------------------------------------------------
static void CommitVertexTextures( D3DDeviceWrapper *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce )
{
	int nCount = g_pMaterialSystemHardwareConfig->GetVertexSamplerCount();
	for ( int i = 0; i < nCount; ++i )
	{
		VertexTextureState_t &currentVTState = currentState.m_VertexTextureState[i];
		ShaderAPITextureHandle_t textureHandle = desiredState.m_VertexTextureState[i].m_BoundVertexTexture;

		Texture_t *pTexture = ( textureHandle != INVALID_SHADERAPI_TEXTURE_HANDLE ) ? &g_ShaderAPIDX8.GetTexture( textureHandle ) : NULL;
//		if ( pTexture && ( pTexture->m_Flags & Texture_t::IS_VERTEX_TEXTURE ) == 0 )
//		{
//			Warning( "Attempting to bind a vertex texture (%s) which was not created as a vertex texture!\n", pTexture->m_DebugName.String() );
//		}

		if ( bForce || ( currentVTState.m_BoundVertexTexture != textureHandle ) )
		{
			currentVTState.m_BoundVertexTexture = textureHandle;

// 			RECORD_COMMAND( DX8_SET_VERTEXTEXTURE, 3 );
// 			RECORD_INT( D3DVERTEXTEXTURESAMPLER0 + nStage );
// 			RECORD_INT( pTexture ? pTexture->GetUniqueID() : 0xFFFF );
// 			RECORD_INT( 0 );

			IDirect3DBaseTexture *pD3DTexture = ( textureHandle >= 0 ) ? g_ShaderAPIDX8.GetD3DTexture( textureHandle ) : NULL;

			pDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0 + i, pD3DTexture );
		}

		if ( !pTexture )
			continue;

		D3DTEXTUREADDRESS nNewUTexWrap = (D3DTEXTUREADDRESS)pTexture->m_UTexWrap;
		if ( bForce || ( currentVTState.m_UTexWrap != nNewUTexWrap ) )
		{
			currentVTState.m_UTexWrap = nNewUTexWrap;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSU, currentVTState.m_UTexWrap );
		}

		D3DTEXTUREADDRESS nNewVTexWrap = (D3DTEXTUREADDRESS)pTexture->m_VTexWrap;
		if ( bForce || ( currentVTState.m_VTexWrap != nNewVTexWrap ) )
		{
			currentVTState.m_VTexWrap = nNewVTexWrap;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSV, currentVTState.m_VTexWrap );
		}

		/*
		D3DTEXTUREADDRESS nNewWTexWrap = pTexture->GetWTexWrap();
		if ( bForce || ( currentVTState.m_WTexWrap != nNewWTexWrap ) )
		{
			currentVTState.m_WTexWrap = nNewWTexWrap;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSW, currentVTState.m_WTexWrap );
		}
		*/

		D3DTEXTUREFILTERTYPE nNewMinFilter = (D3DTEXTUREFILTERTYPE)pTexture->m_MinFilter;
		if ( bForce || ( currentVTState.m_MinFilter != nNewMinFilter ) )
		{
			currentVTState.m_MinFilter = nNewMinFilter;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MINFILTER, currentVTState.m_MinFilter );
		}

		D3DTEXTUREFILTERTYPE nNewMagFilter = (D3DTEXTUREFILTERTYPE)pTexture->m_MagFilter;
		if ( bForce || ( currentVTState.m_MagFilter != nNewMagFilter ) )
		{
			currentVTState.m_MagFilter = nNewMagFilter;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MAGFILTER, currentVTState.m_MagFilter );
		}

		D3DTEXTUREFILTERTYPE nNewMipFilter = (D3DTEXTUREFILTERTYPE)pTexture->m_MipFilter;
		if ( bForce || ( currentVTState.m_MipFilter != nNewMipFilter ) )
		{
			currentVTState.m_MipFilter = nNewMipFilter;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MIPFILTER, currentVTState.m_MipFilter );
		}
	}
}


//-----------------------------------------------------------------------------
// Returns true if the state snapshot is translucent
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsTranslucent( StateSnapshot_t id ) const
{
	LOCK_SHADERAPI();
	const ShadowState_t& snapshot = m_TransitionTable.GetSnapshot(id);
	return snapshot.m_AlphaBlendState.m_AlphaBlendEnable && !snapshot.m_AlphaBlendState.m_AlphaBlendEnabledForceOpaque;
}


//-----------------------------------------------------------------------------
// Returns true if the state snapshot is alpha tested
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsAlphaTested( StateSnapshot_t id ) const
{
	LOCK_SHADERAPI();
	return m_TransitionTable.GetSnapshot(id).m_AlphaTestAndMiscState.m_AlphaTestEnable;
}


//-----------------------------------------------------------------------------
// Gets at the shadow state for a particular state snapshot
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsDepthWriteEnabled( StateSnapshot_t id ) const
{
	LOCK_SHADERAPI();
	return m_TransitionTable.GetSnapshot(id).m_DepthTestState.m_ZWriteEnable;
}



//-----------------------------------------------------------------------------
// Returns true if the state snapshot uses shaders
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::UsesVertexAndPixelShaders( StateSnapshot_t id ) const
{
	LOCK_SHADERAPI();
	return m_TransitionTable.GetSnapshotShader(id).m_VertexShader != INVALID_SHADER;
}


//-----------------------------------------------------------------------------
// Takes a snapshot
//-----------------------------------------------------------------------------
StateSnapshot_t CShaderAPIDx8::TakeSnapshot( )
{
	LOCK_SHADERAPI();

	return m_TransitionTable.TakeSnapshot();
}

void CShaderAPIDx8::ResetDXRenderState( void )
{
	float zero = 0.0f;
	float one = 1.0f;
	DWORD dZero = *((DWORD*)(&zero));
	DWORD dOne = *((DWORD*)(&one));
	
	// Set default values for all dx render states.
	// NOTE: this does not include states encapsulated by the transition table,
	//       which are reset in CTransitionTable::UseDefaultState
    SetSupportedRenderStateForce( D3DRS_FILLMODE, D3DFILL_SOLID );
    SetSupportedRenderStateForce( D3DRS_SHADEMODE, D3DSHADE_GOURAUD );
    SetSupportedRenderStateForce( D3DRS_LASTPIXEL, TRUE );
    SetSupportedRenderStateForce( D3DRS_CULLMODE, D3DCULL_CCW );
    SetSupportedRenderStateForce( D3DRS_DITHERENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_FOGENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_SPECULARENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_FOGCOLOR, 0 );
    SetSupportedRenderStateForce( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
    SetSupportedRenderStateForce( D3DRS_FOGSTART, dZero );
    SetSupportedRenderStateForce( D3DRS_FOGEND, dOne );
    SetSupportedRenderStateForce( D3DRS_FOGDENSITY, dZero );
    SetSupportedRenderStateForce( D3DRS_RANGEFOGENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_STENCILENABLE, FALSE);
    SetSupportedRenderStateForce( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
    SetSupportedRenderStateForce( D3DRS_STENCILREF, 0 );
    SetSupportedRenderStateForce( D3DRS_STENCILMASK, 0xFFFFFFFF );
    SetSupportedRenderStateForce( D3DRS_STENCILWRITEMASK, 0xFFFFFFFF );
    SetSupportedRenderStateForce( D3DRS_TEXTUREFACTOR, 0xFFFFFFFF );
    SetSupportedRenderStateForce( D3DRS_WRAP0, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP1, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP2, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP3, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP4, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP5, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP6, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP7, 0 );
    SetSupportedRenderStateForce( D3DRS_CLIPPING, TRUE );
    SetSupportedRenderStateForce( D3DRS_LIGHTING, TRUE );
    SetSupportedRenderStateForce( D3DRS_AMBIENT, 0 );
    SetSupportedRenderStateForce( D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
    SetSupportedRenderStateForce( D3DRS_COLORVERTEX, TRUE );
    SetSupportedRenderStateForce( D3DRS_LOCALVIEWER, TRUE );
    SetSupportedRenderStateForce( D3DRS_NORMALIZENORMALS, FALSE );
    SetSupportedRenderStateForce( D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2 );
    SetSupportedRenderStateForce( D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL );
    SetSupportedRenderStateForce( D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL );
    SetSupportedRenderStateForce( D3DRS_VERTEXBLEND, D3DVBF_DISABLE );
    SetSupportedRenderStateForce( D3DRS_CLIPPLANEENABLE, 0 );
	
	// -disable_d3d9_hacks is for debugging. For example, the "CENT" driver hack thing causes the flashlight pass to appear much brighter on NVidia drivers.
	if ( IsPC() && !IsOpenGL() && !CommandLine()->CheckParm( "-disable_d3d9_hacks" ) )
	{	
		if ( g_pHardwareConfig->Caps().m_bNeedsATICentroidHack )
		{
			SetSupportedRenderStateForce( D3DRS_POINTSIZE, MAKEFOURCC( 'C', 'E', 'N', 'T' ) );
		}
		else if ( g_pHardwareConfig->Caps().m_VendorID == VENDORID_NVIDIA )
		{
			// NVIDIA has a bug in their driver, so we can't call this on DX10 hardware right now
			if ( !g_pHardwareConfig->UsesSRGBCorrectBlending() )
			{
				// This helps the driver to know to turn on fast z reject for NVidia dx80 hardware
				SetSupportedRenderStateForce( D3DRS_POINTSIZE, MAKEFOURCC( 'H', 'L', '2', 'A' ) );
			}
		}
		if( g_pHardwareConfig->Caps().m_bDisableShaderOptimizations )
		{
			SetSupportedRenderStateForce( D3DRS_ADAPTIVETESS_Y, MAKEFOURCC( 'C', 'O', 'P', 'M' ) );
		}
	}

	SetSupportedRenderStateForce( D3DRS_POINTSIZE, dOne );
    SetSupportedRenderStateForce( D3DRS_POINTSIZE_MIN, dOne );
    SetSupportedRenderStateForce( D3DRS_POINTSPRITEENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_POINTSCALEENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_POINTSCALE_A, dOne );
    SetSupportedRenderStateForce( D3DRS_POINTSCALE_B, dZero );
    SetSupportedRenderStateForce( D3DRS_POINTSCALE_C, dZero );
    SetSupportedRenderStateForce( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
    SetSupportedRenderStateForce( D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF );
    SetSupportedRenderStateForce( D3DRS_PATCHEDGESTYLE, D3DPATCHEDGE_DISCRETE );
    SetSupportedRenderStateForce( D3DRS_DEBUGMONITORTOKEN, D3DDMT_ENABLE );
	float sixtyFour = 64.0f;
    SetSupportedRenderStateForce( D3DRS_POINTSIZE_MAX, *((DWORD*)(&sixtyFour)));
    SetSupportedRenderStateForce( D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_TWEENFACTOR, dZero );
    SetSupportedRenderStateForce( D3DRS_POSITIONDEGREE, D3DDEGREE_CUBIC );
    SetSupportedRenderStateForce( D3DRS_NORMALDEGREE, D3DDEGREE_LINEAR );
    SetSupportedRenderStateForce( D3DRS_SCISSORTESTENABLE, FALSE);
    SetSupportedRenderStateForce( D3DRS_SLOPESCALEDEPTHBIAS, dZero );
    SetSupportedRenderStateForce( D3DRS_ANTIALIASEDLINEENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_MINTESSELLATIONLEVEL, dOne );
    SetSupportedRenderStateForce( D3DRS_MAXTESSELLATIONLEVEL, dOne );
    SetSupportedRenderStateForce( D3DRS_ADAPTIVETESS_X, dZero );
    SetSupportedRenderStateForce( D3DRS_ADAPTIVETESS_Y, dZero );
    SetSupportedRenderStateForce( D3DRS_ADAPTIVETESS_Z, dOne );
    SetSupportedRenderStateForce( D3DRS_ADAPTIVETESS_W, dZero );
    SetSupportedRenderStateForce( D3DRS_ENABLEADAPTIVETESSELLATION, FALSE );
    SetSupportedRenderStateForce( D3DRS_TWOSIDEDSTENCILMODE, FALSE );
    SetSupportedRenderStateForce( D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS );
    SetSupportedRenderStateForce( D3DRS_COLORWRITEENABLE1, 0x0000000f );
    SetSupportedRenderStateForce( D3DRS_COLORWRITEENABLE2, 0x0000000f );
    SetSupportedRenderStateForce( D3DRS_COLORWRITEENABLE3, 0x0000000f );
    SetSupportedRenderStateForce( D3DRS_BLENDFACTOR, 0xffffffff );
    SetSupportedRenderStateForce( D3DRS_SRGBWRITEENABLE, 0);
    SetSupportedRenderStateForce( D3DRS_DEPTHBIAS, dZero );
    SetSupportedRenderStateForce( D3DRS_WRAP8, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP9, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP10, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP11, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP12, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP13, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP14, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP15, 0 );
    SetSupportedRenderStateForce( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    SetSupportedRenderStateForce( D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD );

#if defined( _X360 )
	SetSupportedRenderStateForce( D3DRS_HIZENABLE, mat_hi_z_enable.GetBool() ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
	SetSupportedRenderStateForce( D3DRS_HIZWRITEENABLE, D3DHIZ_AUTOMATIC );

	SetSupportedRenderStateForce( D3DRS_HISTENCILENABLE, FALSE );
	SetSupportedRenderStateForce( D3DRS_HISTENCILWRITEENABLE, FALSE );
	SetSupportedRenderStateForce( D3DRS_HISTENCILFUNC, D3DHSCMP_EQUAL );
	SetSupportedRenderStateForce( D3DRS_HISTENCILREF, 0 );
#endif
}

//-----------------------------------------------------------------------------
// Own GPU Resources.  Return previous state. GPU Ownership allows the code
// to defer all constant setting and then just blast the peak watermark directly into
// the system's internal command buffer. The actual savings may be very negligible
// if not worse due to our constant layout, which causes a wider range to be
// copied.
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::OwnGPUResources( bool bEnable )
{
#if defined( _X360 )
	// no longer supporting, causes an unmeasurable benefit
	// code complexity and instability dictate it is better left off
	// modelfastpath and better batching has probably lessened the dense amount of constant setting
	// leaving code in case perf testing again shows constant setting to be an issue
	if ( !IsGPUOwnSupported() )
	{
		return false;
	}

	if ( m_bGPUOwned == bEnable )
	{
		return m_bGPUOwned;
	}

	if ( !bEnable )
	{
		Dx9Device()->GpuDisownAll();
	}
	else
	{
		// owned GPU constants can be set very fast, and must be in blocks of 4
		// owned constants are set via a peak watermark, otherwise the legacy method is used
		// there are 256, but the game only uses 217 (snapped to 220), leaving just enough room for shader literals
		COMPILE_TIME_ASSERT( VERTEX_SHADER_MODEL + 3*NUM_MODEL_TRANSFORMS == 217 );
		Dx9Device()->GpuOwnVertexShaderConstantF( 0, AlignValue( VERTEX_SHADER_MODEL + 3*NUM_MODEL_TRANSFORMS, 4 ) );
		// there are 256, but the game only utilizes 32, leaving lots of room for shader literals
		Dx9Device()->GpuOwnPixelShaderConstantF( 0, 32 );
	}

	bool bPrevious = m_bGPUOwned;
	m_bGPUOwned = bEnable;
	
	return bPrevious;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Reset render state (to its initial value)
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ResetRenderState( bool bFullReset )
{ 
	if ( IsDeactivated() )
		return;

	LOCK_SHADERAPI();
	RECORD_DEBUG_STRING( "BEGIN ResetRenderState" );

	// force xbox to do any reset work immediately
	OwnGPUResources( false );
	
	ClearAllCommitFuncs( COMMIT_PER_DRAW );
	ClearAllCommitFuncs( COMMIT_PER_PASS );

	m_bResettingRenderState = true;

	ResetDXRenderState();

	// We're not currently rendering anything
	m_nCurrentSnapshot = -1;

	D3DXMatrixIdentity( &m_CachedPolyOffsetProjectionMatrix );
	D3DXMatrixIdentity( &m_CachedFastClipProjectionMatrix );
	D3DXMatrixIdentity( &m_CachedFastClipPolyOffsetProjectionMatrix );
	m_UsingTextureRenderTarget = false;

	m_SceneFogColor[0] = 0;
	m_SceneFogColor[1] = 0;
	m_SceneFogColor[2] = 0;
	m_SceneFogMode = MATERIAL_FOG_NONE;

	// This is state that isn't part of the snapshot per-se, because we
	// don't need it when it's actually time to render. This just helps us
	// to construct the shadow state.
	m_DynamicState.m_ClearColor = D3DCOLOR_XRGB(0,0,0);

	UpdateDepthBiasState();

	if ( bFullReset )
	{
		InitVertexAndPixelShaders();
	}
	else
	{
		// just need to dirty the dynamic state, desired state gets copied into below
#ifndef _PS3
		Q_memset( m_DynamicState.m_pVectorPixelShaderConstant, 0, g_pHardwareConfig->Caps().m_NumPixelShaderConstants * sizeof( Vector4D ) );
#endif
		Q_memset( m_DynamicState.m_pBooleanPixelShaderConstant, 0, g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants * sizeof( BOOL ) );
		Q_memset( m_DynamicState.m_pIntegerPixelShaderConstant, 0, g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants * sizeof( IntVector4D ) );

		Q_memset( m_DynamicState.m_pVectorVertexShaderConstant, 0, g_pHardwareConfig->Caps().m_NumVertexShaderConstants * sizeof( Vector4D ) );
#ifdef _PS3
		Q_memset( m_DesiredState.m_pVectorVertexShaderConstant, 0, g_pHardwareConfig->Caps().m_NumVertexShaderConstants * sizeof( Vector4D ) );
		gpGcmDrawState->m_dirtyStatesMask |= CGcmDrawState::kDirtyZeroAllVSConsts;
#endif
		Q_memset( m_DynamicState.m_pBooleanVertexShaderConstant, 0, g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants * sizeof( BOOL ) );
		Q_memset( m_DynamicState.m_pIntegerVertexShaderConstant, 0, g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants * sizeof( IntVector4D ) );

		SetStandardVertexShaderConstants( OVERBRIGHT );
	}

	m_DynamicState.m_nLocalEnvCubemapSamplers = 0;
	m_DynamicState.m_nLightmapSamplers = 0;
	m_DynamicState.m_nPaintmapSamplers = 0;
	
	// Set the default compressed depth range written to dest alpha. Only need to compress it for 8bit alpha to get a useful gradient.
	SetFloatRenderingParameter( FLOAT_RENDERPARM_DEST_ALPHA_DEPTH_SCALE, ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )  ? 8192.0f : 192.0f );

	// Fog
	m_VertexShaderFogParams[0] = m_VertexShaderFogParams[1] = 0.0f;
	m_WorldSpaceCameraPosition.Init( 0, 0, 0 );
	m_WorldSpaceCameraDirection.Init( 0, 0, -1 );
	m_DynamicState.m_FogColor = 0xFFFFFFFF;
	m_DynamicState.m_vecPixelFogColor.Init();
	m_DynamicState.m_vecPixelFogColorLinear.Init();
	m_DynamicState.m_bFogGammaCorrectionDisabled = false;
	m_DynamicState.m_FogEnable = false;
	m_DynamicState.m_FogMode = D3DFOG_NONE;
	m_DynamicState.m_FogStart = -1;
	m_DynamicState.m_FogEnd = -1;
	m_DynamicState.m_FogMaxDensity = -1.0f;
	m_DynamicState.m_FogZ = 0.0f;

	SetSupportedRenderState( D3DRS_FOGCOLOR, m_DynamicState.m_FogColor );
	SetSupportedRenderState( D3DRS_FOGENABLE, m_DynamicState.m_FogEnable );
	SetSupportedRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
	SetSupportedRenderState( D3DRS_FOGVERTEXMODE, D3DFOG_NONE );
	SetSupportedRenderState( D3DRS_RANGEFOGENABLE, false );

	FogStart( 0 );
	FogEnd( 0 );
	FogMaxDensity( 1.0f );

	m_DynamicState.m_bSRGBWritesEnabled = false;

	// Set the cull mode
	m_DynamicState.m_bCullEnabled = true;
	m_DynamicState.m_CullMode = D3DCULL_CCW;
	m_DynamicState.m_DesiredCullMode = D3DCULL_CCW;
	SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

	// No shade mode yet
	m_DynamicState.m_ShadeMode = (D3DSHADEMODE)-1;
	ShadeMode( SHADER_SMOOTH );

	m_DynamicState.m_bHWMorphingEnabled = false;

	// Skinning...
	m_DynamicState.m_NumBones = 0;

	// It's safe and not a perf problem to always enable this renderstate, even for single-sample rendertargets
	SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, true );
	
	if ( g_pHardwareConfig->ActualCaps().m_bSupportsAlphaToCoverage )
	{
		D3DRENDERSTATETYPE renderState = (D3DRENDERSTATETYPE)g_pHardwareConfig->ActualCaps().m_AlphaToCoverageState;
		SetRenderState( renderState, g_pHardwareConfig->ActualCaps().m_AlphaToCoverageDisableValue );	// Vendor dependent state
	}
	// Anisotropic filtering is disabled by default
	if ( bFullReset )
	{
		SetAnisotropicLevel( 1 );
	}

	for ( int i = 0; i < g_pHardwareConfig->ActualCaps().m_NumSamplers; ++i )
	{
		SamplerState(i).m_BoundTexture = INVALID_SHADERAPI_TEXTURE_HANDLE;						  
		SamplerState(i).m_UTexWrap = D3DTADDRESS_WRAP;
		SamplerState(i).m_VTexWrap = D3DTADDRESS_WRAP;
		SamplerState(i).m_WTexWrap = D3DTADDRESS_WRAP;
		SamplerState(i).m_MagFilter = D3DTEXF_POINT;
		SamplerState(i).m_MinFilter = D3DTEXF_POINT;
		SamplerState(i).m_MipFilter = D3DTEXF_NONE;
		SamplerState(i).m_TextureEnable = false;
		SamplerState(i).m_nTextureBindFlags = TEXTURE_BINDFLAGS_NONE;
		SamplerState(i).m_bShadowFilterEnable = 0;

		// Just some initial state...
		Dx9Device()->SetTexture( i, 0 );

		SetSamplerState( i, D3DSAMP_ADDRESSU, SamplerState(i).m_UTexWrap );
		SetSamplerState( i, D3DSAMP_ADDRESSV, SamplerState(i).m_VTexWrap ); 
		SetSamplerState( i, D3DSAMP_ADDRESSW, SamplerState(i).m_WTexWrap ); 
		SetSamplerState( i, D3DSAMP_MINFILTER, SamplerState(i).m_MinFilter );
		SetSamplerState( i, D3DSAMP_MAGFILTER, SamplerState(i).m_MagFilter );
		SetSamplerState( i, D3DSAMP_MIPFILTER, SamplerState(i).m_MipFilter );
#if defined ( POSIX )
		SetSamplerState( i, D3DSAMP_SHADOWFILTER, SamplerState(i).m_bShadowFilterEnable );
#endif

		SetSamplerState( i, D3DSAMP_BORDERCOLOR, RGB( 0,0,0 ) );
	}

	// FIXME!!!!! : This barfs with the debug runtime on 6800.
	for( int i = 0; i < g_pHardwareConfig->ActualCaps().m_NumVertexSamplers; i++ )
	{
		m_DynamicState.m_VertexTextureState[i].m_BoundVertexTexture = INVALID_SHADERAPI_TEXTURE_HANDLE;
		Dx9Device()->SetTexture( D3DVERTEXTEXTURESAMPLER0 + i, NULL );

		m_DynamicState.m_VertexTextureState[i].m_UTexWrap = D3DTADDRESS_CLAMP;
		m_DynamicState.m_VertexTextureState[i].m_VTexWrap = D3DTADDRESS_CLAMP;
//		m_DynamicState.m_VertexTextureState[i].m_WTexWrap = D3DTADDRESS_CLAMP;
		m_DynamicState.m_VertexTextureState[i].m_MinFilter = D3DTEXF_POINT;
		m_DynamicState.m_VertexTextureState[i].m_MagFilter = D3DTEXF_POINT;
		m_DynamicState.m_VertexTextureState[i].m_MipFilter = D3DTEXF_POINT;
		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSU, m_DynamicState.m_VertexTextureState[i].m_UTexWrap );
		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSV, m_DynamicState.m_VertexTextureState[i].m_VTexWrap );
//		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSW, m_DynamicState.m_VertexTextureState[i].m_WTexWrap );
		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MINFILTER, m_DynamicState.m_VertexTextureState[i].m_MinFilter );
		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MAGFILTER, m_DynamicState.m_VertexTextureState[i].m_MagFilter );
		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MIPFILTER, m_DynamicState.m_VertexTextureState[i].m_MipFilter );
	}

	memset( &m_DynamicState.m_InstanceInfo, 0, sizeof(InstanceInfo_t) );
	memset( &m_DynamicState.m_CompiledLightingState, 0, sizeof(CompiledLightingState_t) );
	memset( &m_DynamicState.m_LightingState, 0, sizeof( MaterialLightingState_t ) );

	for ( int i = 0; i < NUM_MATRIX_MODES; ++i)
	{
		// By setting this to *not* be identity, we force an update...
		m_DynamicState.m_TransformType[i] = TRANSFORM_IS_GENERAL;
		m_DynamicState.m_TransformChanged[i] = STATE_CHANGED;
	}

	// set the board state to match the default state
	m_TransitionTable.UseDefaultState();

	// Set the default render state
	SetDefaultState();

	// Constant for all time
	SetSupportedRenderState( D3DRS_CLIPPING, TRUE );
	SetSupportedRenderState( D3DRS_LOCALVIEWER, TRUE );
	SetSupportedRenderState( D3DRS_POINTSCALEENABLE, FALSE );

#if 0
	float fBias = -1.0f;
	SetTextureStageState( 0, D3DTSS_MIPMAPLODBIAS, *( ( LPDWORD ) (&fBias) ) );
	SetTextureStageState( 1, D3DTSS_MIPMAPLODBIAS, *( ( LPDWORD ) (&fBias) ) );
	SetTextureStageState( 2, D3DTSS_MIPMAPLODBIAS, *( ( LPDWORD ) (&fBias) ) );
	SetTextureStageState( 3, D3DTSS_MIPMAPLODBIAS, *( ( LPDWORD ) (&fBias) ) );
#endif

	if ( bFullReset )
	{
		// Set the modelview matrix to identity too
		for ( int i = 0; i < NUM_MODEL_TRANSFORMS; ++i )
		{
			SetIdentityMatrix( m_boneMatrix[i] );
		}
		MatrixMode( MATERIAL_VIEW );
		LoadIdentity();
		MatrixMode( MATERIAL_PROJECTION );
		LoadIdentity();
	}

#ifdef _X360
	m_DynamicState.m_bBuffer2Frames = m_bBuffer2FramesAhead;
	SetRenderState( D3DRS_BUFFER2FRAMES, m_DynamicState.m_bBuffer2Frames );
#endif

	m_DynamicState.m_Viewport.X = m_DynamicState.m_Viewport.Y = 
		m_DynamicState.m_Viewport.Width = m_DynamicState.m_Viewport.Height = (DWORD)-1;
	m_DynamicState.m_Viewport.MinZ = m_DynamicState.m_Viewport.MaxZ = 0.0;

	// Be sure scissoring is off
	m_DynamicState.m_RenderState[D3DRS_SCISSORTESTENABLE] = FALSE;
	SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
	m_DynamicState.m_ScissorRect.left   = -1;
	m_DynamicState.m_ScissorRect.top    = -1;
	m_DynamicState.m_ScissorRect.right  = -1;
	m_DynamicState.m_ScissorRect.bottom = -1;

	//SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
	EnableFastClip( false );
	float fFakePlane[4];
	unsigned int iFakePlaneVal = 0xFFFFFFFF;
	fFakePlane[0] = fFakePlane[1] = fFakePlane[2] = fFakePlane[3] = *((float *)&iFakePlaneVal);
	SetFastClipPlane( fFakePlane ); //doing this to better wire up plane change detection

	float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// Make sure that our state is dirty.
	m_DynamicState.m_UserClipPlaneEnabled = 0;
	m_DynamicState.m_UserClipPlaneChanged = 0;
	m_DynamicState.m_UserClipLastUpdatedUsingFixedFunction = false;
	for ( int i = 0; i < g_pHardwareConfig->MaxUserClipPlanes(); i++ )
	{
		// Make sure that our state is dirty.
		m_DynamicState.m_UserClipPlaneWorld[i][0] = -1.0f;
		m_DynamicState.m_UserClipPlaneProj[i][0] = -9999.0f;
		m_DynamicState.m_UserClipPlaneEnabled |= ( 1 << i );
		SetClipPlane( i, zero );
		EnableClipPlane( i, false );
		Assert( m_DynamicState.m_UserClipPlaneEnabled == 0 );
	}
	Assert( m_DynamicState.m_UserClipPlaneChanged == ((1 << g_pHardwareConfig->MaxUserClipPlanes()) - 1) );

	m_DynamicState.m_FastClipEnabled = false;
	m_DynamicState.m_bFastClipPlaneChanged = true;

	// User clip override
	m_DynamicState.m_bUserClipTransformOverride = false;
	D3DXMatrixIdentity( &m_DynamicState.m_UserClipTransform );

	// Viewport defaults to the window size
	RECT windowRect;
#if defined(_WIN32) && !defined( DX_TO_GL_ABSTRACTION )
	GetClientRect( (HWND)m_hWnd, &windowRect );
#else
	toglGetClientRect( (VD3DHWND)m_hWnd, &windowRect );
#endif

	ShaderViewport_t viewport;
	viewport.Init( windowRect.left, windowRect.top, 
		windowRect.right - windowRect.left,	windowRect.bottom - windowRect.top );
	SetViewports( 1, &viewport, true );

	// No render mesh
	m_pRenderMesh = 0;
	m_nRenderInstanceCount = 0;
	m_pRenderInstances = NULL;
	m_pRenderCompiledState = NULL;
	m_pRenderInstanceInfo = NULL;
	m_bRenderHasSetStencil = false;

	// Reset cached vertex decl
	m_DynamicState.m_pVertexDecl = NULL;
	m_DynamicState.m_DeclVertexFormat = 0;
	m_DynamicState.m_bDeclHasColorMesh = false;
	m_DynamicState.m_bDeclUsingFlex = false;
	m_DynamicState.m_bDeclUsingMorph = false;
	m_DynamicState.m_bDeclUsingPreTessPatch = false;

	// Reset the render target to be the normal backbuffer
	if ( IsX360() )
	{
		m_hCachedRenderTarget = INVALID_SHADERAPI_TEXTURE_HANDLE;
		m_bUsingSRGBRenderTarget = false;
	}
	AcquireInternalRenderTargets();
	SetRenderTarget();

	if ( !bFullReset )
	{
		// Full reset has already allocated and inited the default values and sent them (above).
		// Normal reset sends the desired values.
		if ( g_pHardwareConfig->Caps().m_NumVertexShaderConstants != 0 )
		{
#ifndef _PS3
			// 217 on X360 to play nice with fast blatting code
			SetVertexShaderConstantInternal( 0, m_DesiredState.m_pVectorVertexShaderConstant[0].Base(), IsX360() ? 217 : g_pHardwareConfig->Caps().m_NumVertexShaderConstants, true );
#endif
		}
		
		if ( g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants != 0 )
		{
			SetIntegerVertexShaderConstant( 0, (int *)m_DesiredState.m_pIntegerVertexShaderConstant, g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants, true );
		}
		
		if ( g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants != 0 )
		{
			SetBooleanVertexShaderConstant( 0, m_DesiredState.m_pBooleanVertexShaderConstant, g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants, true );
		}

		if ( g_pHardwareConfig->Caps().m_NumPixelShaderConstants != 0 )
		{
#ifndef _PS3
			SetPixelShaderConstantInternal( 0, m_DesiredState.m_pVectorPixelShaderConstant[0].Base(), g_pHardwareConfig->Caps().m_NumPixelShaderConstants, true );
#endif
		}

		if ( g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants != 0 )
		{
			SetIntegerPixelShaderConstant( 0, (int *)m_DesiredState.m_pIntegerPixelShaderConstant, g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants, true );
		}

		if ( g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants != 0 )
		{
			SetBooleanPixelShaderConstant( 0, m_DesiredState.m_pBooleanPixelShaderConstant, g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants, true );
		}
	}

	// apply any queued commits that have 'desired' state
	CallCommitFuncs( COMMIT_PER_DRAW, true );
	CallCommitFuncs( COMMIT_PER_PASS, true );

	//
	// All state setting now completed
	//

	// save vertex/pixel shader constant pointers
	Vector4D *pVectorPixelShaderConstants = m_DesiredState.m_pVectorPixelShaderConstant;
	int *pBooleanPixelShaderConstants = m_DesiredState.m_pBooleanPixelShaderConstant;
	IntVector4D *pIntegerPixelShaderConstants = m_DesiredState.m_pIntegerPixelShaderConstant;
	Vector4D *pVectorVertexShaderConstants = m_DesiredState.m_pVectorVertexShaderConstant;
	int *pBooleanVertexShaderConstants = m_DesiredState.m_pBooleanVertexShaderConstant;
	IntVector4D *pIntegerVertexShaderConstants = m_DesiredState.m_pIntegerVertexShaderConstant;

	// DesiredState = DynamicState
	memcpy( &m_DesiredState, &m_DynamicState, sizeof( DynamicState_t ) );

	// restore vertex/pixel shader constant pointers
	m_DesiredState.m_pVectorPixelShaderConstant = pVectorPixelShaderConstants;
	m_DesiredState.m_pBooleanPixelShaderConstant = pBooleanPixelShaderConstants;
	m_DesiredState.m_pIntegerPixelShaderConstant = pIntegerPixelShaderConstants;
	m_DesiredState.m_pVectorVertexShaderConstant = pVectorVertexShaderConstants;
	m_DesiredState.m_pBooleanVertexShaderConstant = pBooleanVertexShaderConstants;
	m_DesiredState.m_pIntegerVertexShaderConstant = pIntegerVertexShaderConstants;

	m_bResettingRenderState = false;

	OwnGPUResources( true );

	RECORD_DEBUG_STRING( "END ResetRenderState" );
}


//-----------------------------------------------------------------------------
// Sets the default render state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetDefaultState()
{
	LOCK_SHADERAPI();

	CShaderAPIDx8::MatrixMode( MATERIAL_MODEL );

	CShaderAPIDx8::ShadeMode( SHADER_SMOOTH );
	CShaderAPIDx8::SetVertexShaderIndex( );
	CShaderAPIDx8::SetPixelShaderIndex( );

	MeshMgr()->MarkUnusedVertexFields( 0, 0, NULL );
}



//-----------------------------------------------------------------------------
//
// Methods related to vertex format
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Sets the vertex
//-----------------------------------------------------------------------------
inline void CShaderAPIDx8::SetVertexDecl( VertexFormat_t vertexFormat, bool bHasColorMesh, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch, VertexStreamSpec_t *pStreamSpec )
{
	VPROF("CShaderAPIDx8::SetVertexDecl");
	if ( pStreamSpec || vertexFormat != m_DynamicState.m_DeclVertexFormat || 
		bHasColorMesh != m_DynamicState.m_bDeclHasColorMesh || 
		bUsingFlex != m_DynamicState.m_bDeclUsingFlex || 
		bUsingMorph != m_DynamicState.m_bDeclUsingMorph ||
		bUsingPreTessPatch != m_DynamicState.m_bDeclUsingPreTessPatch )
	{
		IDirect3DVertexDeclaration9 *pDecl = FindOrCreateVertexDecl( vertexFormat, bHasColorMesh, bUsingFlex, bUsingMorph, bUsingPreTessPatch, pStreamSpec );
		Assert( pDecl );

		if ( ( pDecl != m_DynamicState.m_pVertexDecl ) && pDecl )
		{
			Dx9Device()->SetVertexDeclaration( pDecl );
			m_DynamicState.m_pVertexDecl = pDecl;
		}

		m_DynamicState.m_DeclVertexFormat = vertexFormat;
		m_DynamicState.m_bDeclHasColorMesh = bHasColorMesh;
		m_DynamicState.m_bDeclUsingFlex = bUsingFlex;
		m_DynamicState.m_bDeclUsingMorph = bUsingMorph;
		m_DynamicState.m_bDeclUsingPreTessPatch = bUsingPreTessPatch;
	}
}


void CShaderAPIDx8::SetTessellationMode( TessellationMode_t mode )
{
	m_DynamicState.m_TessellationMode = mode;
}


//-----------------------------------------------------------------------------
//
// Methods related to vertex buffers
//
//-----------------------------------------------------------------------------

IMesh *CShaderAPIDx8::GetFlexMesh()
{
	LOCK_SHADERAPI();
	return MeshMgr()->GetFlexMesh();
}

//-----------------------------------------------------------------------------
// Gets the dynamic mesh
//-----------------------------------------------------------------------------
IMesh* CShaderAPIDx8::GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered,
								IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	Assert( (pMaterial == NULL) || ((IMaterialInternal *)pMaterial)->IsRealTimeVersion() );

	LOCK_SHADERAPI();
	return MeshMgr()->GetDynamicMesh( pMaterial, 0, nHWSkinBoneCount, buffered, pVertexOverride, pIndexOverride );
}

IMesh* CShaderAPIDx8::GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, 
	bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	Assert( (pMaterial == NULL) || ((IMaterialInternal *)pMaterial)->IsRealTimeVersion() );

	LOCK_SHADERAPI();
	return MeshMgr()->GetDynamicMesh( pMaterial, vertexFormat, nHWSkinBoneCount, bBuffered, pVertexOverride, pIndexOverride );
}

//-----------------------------------------------------------------------------
// Returns the number of vertices we can render using the dynamic mesh
//-----------------------------------------------------------------------------
void CShaderAPIDx8::GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
{
	LOCK_SHADERAPI();
	MeshMgr()->GetMaxToRender( pMesh, bMaxUntilFlush, pMaxVerts, pMaxIndices );
}

int CShaderAPIDx8::GetMaxVerticesToRender( IMaterial *pMaterial )
{
	pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion(); //always work with the realtime version internally

	LOCK_SHADERAPI();
	return MeshMgr()->GetMaxVerticesToRender( pMaterial );
}

int CShaderAPIDx8::GetMaxIndicesToRender( )
{
	LOCK_SHADERAPI();
	return MeshMgr()->GetMaxIndicesToRender( );
}

void CShaderAPIDx8::MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords )
{
	LOCK_SHADERAPI();
	MeshMgr()->MarkUnusedVertexFields( nFlags, nTexCoordCount, pUnusedTexCoords ); 
}


#ifdef _GAMECONSOLE


//-----------------------------------------------------------------------------
// Backdoor used by the queued context to directly use write-combined memory
//-----------------------------------------------------------------------------
IMesh *CShaderAPIDx8::GetExternalMesh( const ExternalMeshInfo_t& info )
{
	LOCK_SHADERAPI();
	return MeshMgr()->GetExternalMesh( info ); 
}

void CShaderAPIDx8::SetExternalMeshData( IMesh *pMesh, const ExternalMeshData_t &data )
{
	LOCK_SHADERAPI();
	return MeshMgr()->SetExternalMeshData( pMesh, data ); 
}

IIndexBuffer *CShaderAPIDx8::GetExternalIndexBuffer( int nIndexCount, uint16 *pIndexData )
{
	LOCK_SHADERAPI();
	return MeshMgr()->GetExternalIndexBuffer( nIndexCount, pIndexData ); 
}

void CShaderAPIDx8::FlushGPUCache( void *pBaseAddr, size_t nSizeInBytes )
{
#ifdef _X360
	if ( nSizeInBytes > 0 )
	{
		Dx9Device()->InvalidateGpuCache( pBaseAddr, nSizeInBytes, 0 );
	}
#elif _PS3
	Dx9Device()->FlushVertexCache(); // @TODO@
#endif
}

#endif

const char *_gShaderName;

//-----------------------------------------------------------------------------
// Draws the mesh
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DrawMesh( CMeshBase *pMesh, int nCount, const MeshInstanceData_t *pInstances,
							  VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, 
							  InstanceInfo_t *pInfo )
{
	Assert( nCount > 0 );

	VPROF("CShaderAPIDx8::DrawMesh");
	if ( ShaderUtil()->GetConfig().m_bSuppressRendering )
		return;

	if ( m_pMaterial )
	{
		if ( IShader *pShader = m_pMaterial->GetShader() )
		{
			_gShaderName = pShader->GetName();

			if ( !strcmp( _gShaderName, "VertexLitGeneric" ) )
			{
				m_bVtxLitMesh = true;
			}
			else if ( !strcmp( _gShaderName, "LightmappedGeneric" ) )
			{
				m_bLmapMesh = true;
			}
			else if ( !strcmp( _gShaderName, "UnlitGeneric" ) )
			{
				m_bUnlitMesh = true;
			}
		}
	}

	if (m_bGeneratingCSMs)
	{
		if (!(m_bVtxLitMesh || m_bLmapMesh || m_bUnlitMesh))
		{
			DrawShadowMesh(pMesh, nCount, pInstances, nCompressionType, pCompiledState, pInfo);
		}
		goto xit;
	}
	else if (m_bVtxLitMesh || m_bLmapMesh || m_bUnlitMesh)
	{
		DrawMesh2(pMesh, nCount, pInstances, nCompressionType, pCompiledState, pInfo);
		goto xit;
	}

	// If we are here then it's neither vtx lit, lmap or shadows
	DrawMeshInternal( pMesh, nCount, pInstances, nCompressionType, pCompiledState, pInfo );

	// Stop profiling
xit:

	m_bVtxLitMesh = false;
	m_bLmapMesh = false;
	m_bUnlitMesh = false;
}

void CShaderAPIDx8::DrawMeshInternal( CMeshBase *pMesh, int nCount, const MeshInstanceData_t *pInstances,
									  VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, 
									  InstanceInfo_t *pInfo )
{
#if ( defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD ) || ( defined( PLATFORM_POSIX ) && ( GLMDEBUG > 0 ) ) )
	BeginPIXEvent( PIX_VALVE_ORANGE, s_pPIXMaterialName );
#endif

	m_pRenderMesh = pMesh;
	m_nRenderInstanceCount = nCount;
	m_pRenderInstances = pInstances;
	m_pRenderCompiledState = pCompiledState;
	m_pRenderInstanceInfo = pInfo;
	m_bRenderHasSetStencil = false;
	m_DynamicState.m_bLightStateComputed = false;
	m_DynamicState.m_nLocalEnvCubemapSamplers = 0;
	m_DynamicState.m_nLightmapSamplers = 0;
	m_DynamicState.m_nPaintmapSamplers = 0;
	GetCurrentStencilState( &m_RenderInitialStencilState );
	bool bUsingPreTessPatches = false;
	if ( m_pRenderMesh )
	{
		VertexFormat_t vertexFormat = m_pRenderMesh->GetVertexFormat();
		bUsingPreTessPatches = ( m_pRenderMesh->GetTessellationType() > 0 ) && ( GetTessellationMode() == TESSELLATION_MODE_ACC_PATCHES_EXTRA || GetTessellationMode() == TESSELLATION_MODE_ACC_PATCHES_REG );
		SetVertexDecl( vertexFormat, ( m_pMaterial->GetVertexFormat() & VERTEX_COLOR_STREAM_1 ) != 0, m_pRenderMesh->HasFlexMesh(),
			m_pMaterial->IsUsingVertexID(), bUsingPreTessPatches, m_pRenderMesh->GetVertexStreamSpec() );
	}

	bool bIsAlphaModulating = pInstances[0].m_DiffuseModulation[3] != 1.0f;
#ifdef _DEBUG
	// NOTE: At the moment, if one instance is modulating, they all are.
	// However, this is relatively easy to fix; I just don't have time.
	// Each shader render pass needs to store 2 alphablending states: one for when we are alpha
	// modulating and one when were are not. All shaders need to be rewritten
	// to not have either static or dynamic combos as a result of modulating/not modulating
	// Then this will work.
	for ( int i = 1; i < nCount; ++i )
	{
		bool bTestIsAlphaModulating = pInstances[i].m_DiffuseModulation[3] != 1.0f;
		Assert( bTestIsAlphaModulating == bIsAlphaModulating );
	}
#endif

	CommitStateChanges();
	Assert( m_pMaterial );
	m_pMaterial->DrawMesh( nCompressionType, bIsAlphaModulating, bUsingPreTessPatches );

	if ( m_bRenderHasSetStencil )
	{
		SetStencilStateInternal( m_RenderInitialStencilState );
	}

	m_pRenderMesh = NULL;
	m_nRenderInstanceCount = 0;
	m_pRenderInstances = NULL;
	m_pRenderCompiledState = NULL;
	m_pRenderInstanceInfo = NULL;
	m_bRenderHasSetStencil = false;
	m_DynamicState.m_bLightStateComputed = false;

#if ( defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD ) || ( defined( PLATFORM_POSIX ) && ( GLMDEBUG > 0 ) ) )
	EndPIXEvent();
#endif
}

void CShaderAPIDx8::DrawMesh2( CMeshBase* pMesh, int nCount, const MeshInstanceData_t *pInstances, 
							  VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, InstanceInfo_t *pInfo )
{
    int bFixedLightingMode;
    bool inFlashlightMode;
    uint32 mtlChangeTimestamp;
    bool bIsAlphaModulating;
    int modulationFlags;
    CBasePerMaterialContextData **pContextDataPtr;
    int dynVSIdx, dynPSIdx;
    IShader *pShader;
    IMaterialVar** params;
    bool mtlChanged;

	static ConVarRef r_staticlight_mode( "r_staticlight_mode" );

	if ( (m_bVtxLitMesh && !mat_vtxlit_new_path.GetBool()) ||
		 (m_bLmapMesh && !mat_lmap_new_path.GetBool()) ||
		 (m_bUnlitMesh && !mat_unlit_new_path.GetBool()) )		 
	{
		goto useOldPath;
	}

	// old path only if using avg of 3 staticlight color streams, since we fall back to sm2b
	if ( r_staticlight_mode.GetInt() == 1 )
	{
		goto useOldPath;
	}

	// Work out if this mesh can indeed use the new path
	if ( m_bToolsMode )
	{
		goto useOldPath;
	}

	// Get the context data ptr
	bIsAlphaModulating = pInstances[0].m_DiffuseModulation[3] != 1.0f;
	modulationFlags = bIsAlphaModulating? SHADER_USING_ALPHA_MODULATION : 0;	

	pContextDataPtr = m_pMaterial->GetContextData( modulationFlags );
	if ( !pContextDataPtr || !(*pContextDataPtr) )
	{
		goto useOldPath;
	}

	mtlChangeTimestamp = m_pMaterial->GetChangeTimestamp();	
	mtlChanged = (*pContextDataPtr)->m_nVarChangeID != mtlChangeTimestamp;
	if ( mtlChanged )
	{
		goto useOldPath;
	}

	inFlashlightMode = InFlashlightMode();
	if ( inFlashlightMode )
	{
		goto useOldPath;
	}

	bFixedLightingMode = GetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING );
	if ( bFixedLightingMode != ENABLE_FIXED_LIGHTING_NONE )
	{
		goto useOldPath;
	}

	// Possibly new path

	m_pRenderMesh = pMesh;
	m_nRenderInstanceCount = nCount;
	m_pRenderInstances = pInstances;
	m_pRenderCompiledState = pCompiledState;
	m_pRenderInstanceInfo = pInfo;
	m_bRenderHasSetStencil = false;
	m_DynamicState.m_bLightStateComputed = false;
	m_DynamicState.m_nLocalEnvCubemapSamplers = 0;
	m_DynamicState.m_nLightmapSamplers = 0;
	m_DynamicState.m_nPaintmapSamplers = 0;

	// Streams already set by the time we get here
		
	pShader = m_pMaterial->GetShader();
	params = m_pMaterial->GetVars();
	pShader->SetPPParams( params );
	pShader->SetModulationFlags( modulationFlags );
	pShader->ExecuteFastPath( &dynVSIdx, &dynPSIdx, params, this, nCompressionType, pContextDataPtr, m_bCSMsValidThisFrame );

	if ( dynVSIdx != -1 )
	{
		unsigned char* pInstanceCommandBuffer = NULL;
		StateSnapshot_t snapshotId = m_pMaterial->GetSnapshotId( modulationFlags, 0 );

		CommitSetViewports( Dx9Device(), m_DesiredState, m_DynamicState, false );
		ClearAllCommitFuncs( COMMIT_PER_DRAW );

		CommitFastClipPlane();

		CommitVertexShaderTransforms();

		if (m_DynamicState.m_UserClipPlaneEnabled)
		{
			CommitUserClipPlanes();
		}

		m_TransitionTable.UseSnapshot( snapshotId, false );
			
		pInstanceCommandBuffer = m_pMaterial->GetInstanceCommandBuffer( modulationFlags );
				
		ShadowShaderState_t *pShadowShaderState = m_TransitionTable.GetShaderShadowState( snapshotId );

		PixelShader_t ps = pShadowShaderState->m_PixelShader;
		HardwareShader_t hps = ShaderManager()->GetPixelShader( ps, dynPSIdx );
		ShaderManager()->SetPixelShaderState_Internal( hps, 0 );
		
		VertexShader_t vs = pShadowShaderState->m_VertexShader;
		HardwareShader_t hvs = ShaderManager()->GetVertexShader( vs, dynVSIdx );
		ShaderManager()->SetVertexShaderState_Internal( hvs, 0 );
		
		if ( pMesh )
		{
			// Set vertex decl
			VertexFormat_t vertexFormat = pMesh->GetVertexFormat();	
			SetVertexDecl( vertexFormat, ( m_pMaterial->GetVertexFormat() & VERTEX_COLOR_STREAM_1 ) != 0, pMesh->HasFlexMesh(),
				m_pMaterial->IsUsingVertexID(), false, pMesh->GetVertexStreamSpec() );

			pMesh->DrawPrims( pInstanceCommandBuffer );
		}
		else
		{
			
			MeshMgr()->DrawInstancedPrims( pInstanceCommandBuffer );
		}
		pShader->SetPPParams(NULL);
	}
	else
	{
		// Could not take new path
		pShader->SetPPParams(NULL);
		goto useOldPath;
	}

	goto xit;

useOldPath:	
	DrawMeshInternal( pMesh, nCount, pInstances, nCompressionType, pCompiledState, pInfo );
	goto xit;

xit:

	m_pRenderMesh = NULL;
	m_nRenderInstanceCount = 0;
	m_pRenderInstances = NULL;
	m_pRenderCompiledState = NULL;
	m_pRenderInstanceInfo = NULL;
	m_bRenderHasSetStencil = false;
	m_DynamicState.m_bLightStateComputed = false;

	return;
}

void CShaderAPIDx8::DrawShadowMesh( CMeshBase *pMesh, int nCount, const MeshInstanceData_t *pInstances,
								   VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, 
								   InstanceInfo_t *pInfo )
{	
	if ( !mat_depthwrite_new_path.GetBool() )
	{	
		DrawMeshInternal( pMesh, nCount, pInstances, nCompressionType, pCompiledState, pInfo );
		return;
	}

	// New DrawMesh
	
	// Work out shaders to set
	int dynVSIdx, dynPSIdx;
	IShader *pShader = m_pMaterial->GetShader();
	IMaterialVar** params = m_pMaterial->GetVars();
	pShader->SetPPParams( params );
	pShader->ExecuteFastPath( &dynVSIdx, &dynPSIdx, params, this, nCompressionType, NULL, false );

	if ( dynVSIdx != -1 )
	{
		StateSnapshot_t snapshotId = m_pMaterial->GetSnapshotId( 0, 0 );
		ShadowShaderState_t *pShadowShaderState = m_TransitionTable.GetShaderShadowState( snapshotId );

		if (m_DynamicState.m_UserClipPlaneEnabled)
        {
             CommitUserClipPlanes();
        }

		PixelShader_t ps = pShadowShaderState->m_PixelShader;
		HardwareShader_t hps;
		if ( ps != (PixelShader_t)-1 )
		{
			hps = ShaderManager()->GetPixelShader( ps, dynPSIdx );
		}
		else
		{
			hps = 0;
		}
		ShaderManager()->SetPixelShaderState_Internal( hps, 0 );
		
		VertexShader_t vs = pShadowShaderState->m_VertexShader;
		HardwareShader_t hvs = ShaderManager()->GetVertexShader( vs, dynVSIdx );
		ShaderManager()->SetVertexShaderState_Internal( hvs, 0 );
		
		if ( pMesh )
		{
			// Set skinning matrices if required
			int numBones = GetCurrentNumBones();
			if ( numBones > 0 )
			{
				SetSkinningMatrices( pInstances[0] );
			}

			// Work out vertex format. This involves an RB Tree lookup.
			VertexFormat_t vertexFormat = pMesh->GetVertexFormat();
			SetVertexDecl( vertexFormat, ( m_pMaterial->GetVertexFormat() & VERTEX_COLOR_STREAM_1 ) != 0, pMesh->HasFlexMesh(),
				false, false, pMesh->GetVertexStreamSpec() );

			// Draw
			pMesh->DrawPrims( NULL );
		}
		else
		{
			MeshMgr()->DrawInstancedPrims( NULL );
		}
	}
	else
	{
		//DevMsg("DrawShadowMesh: Bad dyn vs/ps index, cannot draw\n");	
		
		// gurjeets - Found bug on de_cbble where a couple of meshes get drawn into the CSM using Sprite_DX9, which actually
		// turns Alpha Blending and Colour Writes ON for the CSM render target. This is wrong.
		// The original DrawMesh deals with this by resetting the correct states when drawing subsequent (legit) meshes into the CSM.
		// DrawShadowMesh doesn't cope because it relies on correct states being set at the start of CSM rendering

		// No idea why de-cbble is trying to include some sprites in the CSM pass. They don't make it into the CSM anyway
		
		// I expect all geometry that is legit for CSM to be drawn using one of the shaders that have a FastPath version. Testing
		// a bunch of maps seems to indicate this is reasonable expectation. If we find some objects not casting shadows where
		// they should be I'll revisit this. 
	}
	pShader->SetPPParams( NULL );
}


void CShaderAPIDx8::DrawWithVertexAndIndexBuffers( void )
{
	VPROF("CShaderAPIDx8::DrawWithVertexAndIndexBuffers");
	if ( ShaderUtil()->GetConfig().m_bSuppressRendering )
		return;

#if ( defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD ) || ( defined( PLATFORM_POSIX ) && ( GLMDEBUG > 0 ) ) )
	BeginPIXEvent( PIX_VALVE_ORANGE, s_pPIXMaterialName );
#endif

//	m_pRenderMesh = pMesh;
	// FIXME: need to make this deal with multiple streams, etc.
	VertexFormat_t vertexFormat = MeshMgr()->GetCurrentVertexFormat();
	SetVertexDecl( vertexFormat, false /*( vertexFormat & VERTEX_COLOR_STEAM_1 ) != 0*/,
		false /*m_pRenderMesh->HasFlexMesh()*/, false /*m_pRenderMesh->IsUsingMorphData()*/, 
		false /*using pre tessellated patches*/,
		NULL /*m_pRenderMesh->GetVertexStreamSpec()*/ );
	CommitStateChanges();
	if ( m_pMaterial )
	{		
		// FIXME: Get alpha modulation state from instance information
		m_pMaterial->DrawMesh( CompressionType( vertexFormat ), m_pMaterial->GetAlphaModulation() != 1.0f, false );
	}
	else
	{
		MeshMgr()->RenderPassWithVertexAndIndexBuffers( NULL );
	}
//	m_pRenderMesh = NULL;

#if ( defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD ) || ( defined( PLATFORM_POSIX ) && ( GLMDEBUG > 0 ) ) )
	EndPIXEvent();
#endif
}

//-----------------------------------------------------------------------------
// Discards the vertex buffers
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DiscardVertexBuffers()
{
	MeshMgr()->DiscardVertexBuffers();
}

void CShaderAPIDx8::ForceHardwareSync_WithManagedTexture()
{
	if ( IsGameConsole() || IsOSX() || !m_pFrameSyncTexture )
		return;

#ifndef _PS3
	// Set the default state for everything so we don't get more than we ask for here!
	SetDefaultState();

	D3DLOCKED_RECT rect;
	HRESULT hr = m_pFrameSyncTexture->LockRect( 0, &rect, NULL, 0 );
	if ( SUCCEEDED( hr ) )
	{
		// modify..
		unsigned long *pData = (unsigned long*)rect.pBits;
		(*pData)++; 

		m_pFrameSyncTexture->UnlockRect( 0 );

		// Now draw something with this texture.
		DWORD iStage = 0;
		IDirect3DBaseTexture9 *pOldTexture;
		hr = Dx9Device()->GetTexture( iStage, &pOldTexture );
		if ( SUCCEEDED( hr ) )
		{
			Dx9Device()->SetTexture( iStage, m_pFrameSyncTexture );
			// Remember the old FVF.
			DWORD oldFVF;
			hr = Dx9Device()->GetFVF( &oldFVF );
			if ( SUCCEEDED( hr ) )
			{
				// Set the new FVF.
				Dx9Device()->SetFVF( D3DFVF_XYZ );
				// Now, draw the simplest primitive D3D has ever seen.
				unsigned short indices[3] = { 0, 1, 2 };
				Vector verts[3] = {vec3_origin, vec3_origin, vec3_origin};

				Dx9Device()->DrawIndexedPrimitiveUP(
					D3DPT_TRIANGLELIST,
					0,				// Min vertex index
					3,				// Num vertices used
					1,				// # primitives
					indices,		// indices
					D3DFMT_INDEX16,	// index format
					verts,			// Vertices
					sizeof( Vector )// Vertex stride
					);

				Dx9Device()->SetFVF( oldFVF );
			}
			Dx9Device()->SetTexture( iStage, pOldTexture );
		}
	}
	// If this assert fails, then we failed somewhere above.
	AssertOnce( SUCCEEDED( hr ) );
#endif // !_PS3
}

void CShaderAPIDx8::UpdateFrameSyncQuery( int queryIndex, bool bIssue )
{	
	if ( IsOSX() )
		return;
	
	Assert(queryIndex < NUM_FRAME_SYNC_QUERIES);
	// wait if already issued
	if ( m_bQueryIssued[queryIndex] )
	{
		double flStartTime = Plat_FloatTime();
		BOOL dummyData = 0;
		HRESULT hr = S_OK;
		// NOTE: This fix helps out motherboards that are a little freaky.
		// On such boards, sometimes the driver has to reset itself (an event which takes several seconds)
		// and when that happens, the frame sync query object gets lost
		for (;;)
		{
			hr = m_pFrameSyncQueryObject[queryIndex]->GetData( &dummyData, sizeof( dummyData ), D3DGETDATA_FLUSH );
			if ( hr != S_FALSE )
				break;
			double flCurrTime = Plat_FloatTime();
			// don't wait more than 200ms (5fps) for these
			if ( flCurrTime - flStartTime > 0.200f )
				break;
			// Avoid burning a full core while waiting for the query. Spinning can actually harm performance
			// because there might be driver threads that are trying to do work that end up starved, and the
			// power drawn by the CPU may take away from the power available to the integrated graphics chip.
			// A sleep of one millisecond should never be long enough to affect performance.
			ThreadSleep( 1 );
		}

		m_bQueryIssued[queryIndex] = false;
		Assert(hr == S_OK || hr == D3DERR_DEVICELOST);

		if ( hr == D3DERR_DEVICELOST )
		{
			MarkDeviceLost( );
			return;
		}
	}
	if ( bIssue )
	{
		m_pFrameSyncQueryObject[queryIndex]->Issue( D3DISSUE_END );
		m_bQueryIssued[queryIndex] = true;
	}
}


void CShaderAPIDx8::ForceHardwareSync( void )
{
	LOCK_SHADERAPI();
	VPROF( "CShaderAPIDx8::ForceHardwareSync" );
	PERF_STATS_BLOCK( "CShaderAPIDx8::ForceHardwareSync", PERF_STATS_SLOT_FORCE_HARDWARE_SYNC );
	TM_ZONE_PLOT( TELEMETRY_LEVEL1, "ForceHardwareSync", TELEMETRY_ZONE_PLOT_SLOT_4);
	
#ifdef DX_TO_GL_ABSTRACTION
	if ( true )
#else
	if ( !mat_frame_sync_enable.GetInt() )
#endif
		return;

	RECORD_COMMAND( DX8_HARDWARE_SYNC, 0 );

#if !defined( _X360 )
	// How do you query dx9 for how many frames behind the hardware is or, alternatively, how do you tell the hardware to never be more than N frames behind?
	// 1) The old QueryPendingFrameCount design was removed.  It was
	// a simple transaction with the driver through the 
	// GetDriverState, trivial for the drivers to lie.  We came up 
	// with a much better scheme for tracking pending frames where 
	// the driver can not lie without a possible performance loss:  
	// use the asynchronous query system with D3DQUERYTYPE_EVENT and 
	// data size 0.  When GetData returns S_OK for the query, you 
	// know that frame has finished.
	if ( mat_frame_sync_force_texture.GetBool() )
	{
		ForceHardwareSync_WithManagedTexture();
	}
	else if ( m_pFrameSyncQueryObject[0] )
	{
		// FIXME: Could install a callback into the materialsystem to do something while waiting for
		// the frame to finish (update sound, etc.)
		
		m_currentSyncQuery ++;
		if ( m_currentSyncQuery >= ARRAYSIZE(m_pFrameSyncQueryObject) )
		{
			m_currentSyncQuery = 0;
		}
		double fStart = Plat_FloatTime();
		int waitIndex = ((m_currentSyncQuery + NUM_FRAME_SYNC_QUERIES) - (NUM_FRAME_SYNC_FRAMES_LATENCY+1)) % NUM_FRAME_SYNC_QUERIES;
		UpdateFrameSyncQuery( waitIndex, false );
		UpdateFrameSyncQuery( m_currentSyncQuery, true );
	}
#else
	DWORD hFence = Dx9Device()->InsertFence();
	Dx9Device()->BlockOnFence( hFence );
#endif
}


//-----------------------------------------------------------------------------
// Needs render state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::QueueResetRenderState()
{
	m_bResetRenderStateNeeded = true;
}


//-----------------------------------------------------------------------------
// Use this to begin and end the frame
//-----------------------------------------------------------------------------
void CShaderAPIDx8::BeginFrame()
{
	LOCK_SHADERAPI();

	if ( m_bResetRenderStateNeeded )
	{
		ResetRenderState( false );
		m_bResetRenderStateNeeded = false;
	}

#if ALLOW_SMP_ACCESS
	Dx9Device()->SetASyncMode( mat_use_smp.GetBool() );
#endif

	++m_CurrentFrame;
	m_nTextureMemoryUsedLastFrame = 0;
	m_bCSMsValidThisFrame = false;
}

void CShaderAPIDx8::EndFrame()
{
	LOCK_SHADERAPI();

#if !defined( _X360 )
	MEMCHECK;
#endif

#if SHADERAPI_BUFFER_D3DCALLS
	Dx9Device()->ExecuteAllWork();
#endif

	ExportTextureList();
}

int CShaderAPIDx8::D3DFormatToBitsPerPixel( D3DFORMAT fmt ) const
{
	switch( fmt )
	{
	case D3DFMT_UNKNOWN:
		return 0;

#if !( defined( _X360 ) || defined( DX_TO_GL_ABSTRACTION ) )
	case D3DFMT_R3G3B2:
	case D3DFMT_P8:
	case D3DFMT_A4L4:
#endif
	case D3DFMT_A8:
	case D3DFMT_L8:
		return 8;

#if !( defined( _X360 ) || defined( DX_TO_GL_ABSTRACTION ) )
	case D3DFMT_A8R3G3B2:
	case D3DFMT_A8P8:
	case D3DFMT_D16_LOCKABLE:
	case D3DFMT_D15S1:
#endif
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4:
			
	case D3DFMT_D16:
	case D3DFMT_A8L8:
	case D3DFMT_V8U8:
#ifndef DX_TO_GL_ABSTRACTION
	case D3DFMT_X4R4G4B4:
	case D3DFMT_L6V5U5:
	case D3DFMT_L16:
	case D3DFMT_R16F:
#endif
		return 16;

#if !defined( _X360 )
	case D3DFMT_R8G8B8:
		return 24;
#endif

#if !( defined( _X360 ) || defined( DX_TO_GL_ABSTRACTION ) )
	case D3DFMT_D24X4S4:
	case D3DFMT_D32F_LOCKABLE:
#endif
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_X8L8V8U8:
	case D3DFMT_Q8W8V8U8:

	case D3DFMT_D24S8:
	case D3DFMT_D24X8:
#ifndef DX_TO_GL_ABSTRACTION			
	case D3DFMT_A2B10G10R10:
	case D3DFMT_A8B8G8R8:
	case D3DFMT_X8B8G8R8:
	case D3DFMT_G16R16:
	case D3DFMT_A2R10G10B10:
	case D3DFMT_V16U16:
	case D3DFMT_A2W10V10U10:
	case D3DFMT_R8G8_B8G8:
	case D3DFMT_G8R8_G8B8:
	case D3DFMT_D32:
	case D3DFMT_D24FS8:
	case D3DFMT_G16R16F:
	case D3DFMT_R32F:
#endif
		return 32;

	case D3DFMT_A16B16G16R16:
	case D3DFMT_A16B16G16R16F:
#ifndef DX_TO_GL_ABSTRACTION			
	case D3DFMT_Q16W16V16U16:
	case D3DFMT_G32R32F:
#endif
		return 64;

	case D3DFMT_A32B32G32R32F:
		return 128;

#if !( defined( _X360 ) || defined( DX_TO_GL_ABSTRACTION ) )
	case D3DFMT_MULTI2_ARGB8:
	case D3DFMT_CxV8U8:
	case D3DFMT_DXT2:
	case D3DFMT_DXT4:
#endif
#ifndef DX_TO_GL_ABSTRACTION	
	case D3DFMT_UYVY:
	case D3DFMT_YUY2:
#endif
	case D3DFMT_DXT1:
	case D3DFMT_DXT3:
	case D3DFMT_DXT5:
		Assert( !"Implement me" );
		return 0;

	default:
		Assert( !"Unknown D3DFORMAT" );
		return 0;
	}
	return 0;
}

void CShaderAPIDx8::AddBufferToTextureList( const char *pName, D3DSURFACE_DESC &desc )
{
//	ImageFormat imageFormat;
//	imageFormat = ImageLoader::D3DFormatToImageFormat( desc.Format );
//	if( imageFormat < 0 )
//	{
//		Assert( 0 );
//		return;
//	}
	int nBpp = D3DFormatToBitsPerPixel( desc.Format );
	KeyValues *pSubKey = m_pDebugTextureList->CreateNewKey();
	pSubKey->SetString( "Name", pName );
	pSubKey->SetString( "TexGroup", TEXTURE_GROUP_RENDER_TARGET );
	pSubKey->SetInt( "Size", 
//		ImageLoader::SizeInBytes( imageFormat ) * desc.Width * desc.Height );
		nBpp * desc.Width * desc.Height * MAX( 1, desc.MultiSampleType ) / 8 );
	char pTmpBuf[64];
	sprintf( pTmpBuf, "%d bit buffer", nBpp );
	pSubKey->SetString( "Format", pTmpBuf );//ImageLoader::GetName( imageFormat ) );
	pSubKey->SetInt( "Width", desc.Width );
	pSubKey->SetInt( "Height", desc.Height );
	
	pSubKey->SetInt( "BindsMax", 1 );
	pSubKey->SetInt( "BindsFrame", 1 );
}

void CShaderAPIDx8::ExportTextureList()
{
	if ( !m_bEnableDebugTextureList )
		return;

	if ( !m_pBackBufferSurfaces[BACK_BUFFER_INDEX_DEFAULT] || !m_pZBufferSurface )
		// Device vanished...
		return;

	m_DebugTextureListLock.Lock();

	m_nDebugDataExportFrame = m_CurrentFrame;

	if ( IsPC() || !IsX360() )
	{
		if ( m_pDebugTextureList )
			m_pDebugTextureList->deleteThis();

		m_pDebugTextureList = new KeyValues( "TextureList" );

		m_nTextureMemoryUsedTotal = 0;
		m_nTextureMemoryUsedPicMip1 = 0;
		m_nTextureMemoryUsedPicMip2 = 0;
		for ( ShaderAPITextureHandle_t hTexture = m_Textures.Head() ; hTexture != m_Textures.InvalidIndex(); hTexture = m_Textures.Next( hTexture ) )
		{
			Texture_t &tex = m_Textures[hTexture];
		
			if ( !( tex.m_Flags & Texture_t::IS_ALLOCATED ) )
				continue;

			// Compute total texture memory usage
			m_nTextureMemoryUsedTotal += tex.GetMemUsage();

			// Compute picmip memory usage
			{
				int numBytes = tex.GetMemUsage();

				if ( tex.m_NumLevels > 1 )
				{
					if ( tex.GetWidth() > 4 || tex.GetHeight() > 4 || tex.GetDepth() > 4 )
					{
						int topmipsize = ImageLoader::GetMemRequired( tex.GetWidth(), tex.GetHeight(), tex.GetDepth(), tex.GetImageFormat(), false );
						numBytes -= topmipsize;

						m_nTextureMemoryUsedPicMip1 += numBytes;

						if ( tex.GetWidth() > 8 || tex.GetHeight() > 8 || tex.GetDepth() > 8 )
						{
							int othermipsizeRatio = ( ( tex.GetWidth() > 8 ) ? 2 : 1 ) * ( ( tex.GetHeight() > 8 ) ? 2 : 1 ) * ( ( tex.GetDepth() > 8 ) ? 2 : 1 );
							int othermipsize = topmipsize / othermipsizeRatio;
							numBytes -= othermipsize;
						}

						m_nTextureMemoryUsedPicMip1 += numBytes;
					}
					else
					{
						m_nTextureMemoryUsedPicMip1 += numBytes;
						m_nTextureMemoryUsedPicMip2 += numBytes;
					}
				}
				else
				{
					m_nTextureMemoryUsedPicMip1 += numBytes;
					m_nTextureMemoryUsedPicMip2 += numBytes;
				}
			}

			if ( !m_bDebugGetAllTextures &&
				  tex.m_LastBoundFrame != m_CurrentFrame )
				continue;

			if ( tex.m_LastBoundFrame != m_CurrentFrame )
				tex.m_nTimesBoundThisFrame = 0;

			KeyValues *pSubKey = m_pDebugTextureList->CreateNewKey();
			pSubKey->SetString( "Name", tex.m_DebugName.String() );
			pSubKey->SetString( "TexGroup", tex.m_TextureGroupName.String() );
			pSubKey->SetInt( "Size", tex.GetMemUsage() );
			if ( tex.GetCount() > 1 )
				pSubKey->SetInt( "Count", tex.GetCount() );
			pSubKey->SetString( "Format", ImageLoader::GetName( tex.GetImageFormat() ) );
			pSubKey->SetInt( "Width", tex.GetWidth() );
			pSubKey->SetInt( "Height", tex.GetHeight() );

			pSubKey->SetInt( "BindsMax", tex.m_nTimesBoundMax );
			pSubKey->SetInt( "BindsFrame", tex.m_nTimesBoundThisFrame );
		}

		D3DSURFACE_DESC desc;
		m_pBackBufferSurfaces[BACK_BUFFER_INDEX_DEFAULT]->GetDesc( &desc );
		AddBufferToTextureList( "BACKBUFFER", desc );
		desc.MultiSampleType = D3DMULTISAMPLE_NONE;	// front-buffer isn't multisampled
		AddBufferToTextureList( "FRONTBUFFER", desc );
	//	ImageFormat imageFormat = ImageLoader::D3DFormatToImageFormat( desc.Format );
	//	if( imageFormat >= 0 )
		{
			VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_frame_" TEXTURE_GROUP_RENDER_TARGET, 
				COUNTER_GROUP_TEXTURE_PER_FRAME, 
	//			ImageLoader::SizeInBytes( imageFormat ) * desc.Width * desc.Height );
				4 * desc.Width * desc.Height * ( 1 + MAX( 1, desc.MultiSampleType ) ) ); // hack (front-buffer (single-sampled) + back-buffer (multi-sampled) )
		}

		m_pZBufferSurface->GetDesc( &desc );
		AddBufferToTextureList( "DEPTHBUFFER", desc );
	//	imageFormat = ImageLoader::D3DFormatToImageFormat( desc.Format );
	//	if( imageFormat >= 0 )
		{
			VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_frame_" TEXTURE_GROUP_RENDER_TARGET, 
				COUNTER_GROUP_TEXTURE_PER_FRAME, 
	//			ImageLoader::SizeInBytes( imageFormat ) * desc.Width * desc.Height );
				4 * desc.Width * desc.Height * MAX( 1, desc.MultiSampleType ) ); // hack
		}

		if( m_pBackBufferSurfaces[BACK_BUFFER_INDEX_HDR] != NULL )
		{
			m_pBackBufferSurfaces[BACK_BUFFER_INDEX_HDR]->GetDesc( &desc );
			AddBufferToTextureList( "HDR_BACKBUFFER", desc );
			{
				VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_frame_" TEXTURE_GROUP_RENDER_TARGET, 
					COUNTER_GROUP_TEXTURE_PER_FRAME, 
					8 * desc.Width * desc.Height * MAX( 1, desc.MultiSampleType ) ); // hack
			}
		}
	}

#if defined( _X360 )
	// toggle to do one shot transmission
	m_bEnableDebugTextureList = false;

	int numTextures = m_Textures.Count() + 3;
	xTextureList_t* pXTextureList = (xTextureList_t *)_alloca( numTextures * sizeof( xTextureList_t ) );
	memset( pXTextureList, 0, numTextures * sizeof( xTextureList_t ) );

	numTextures = 0;
	for ( ShaderAPITextureHandle_t hTexture = m_Textures.Head() ; hTexture != m_Textures.InvalidIndex(); hTexture = m_Textures.Next( hTexture ) )
	{
		Texture_t &tex = m_Textures[hTexture];
	
		if ( !m_bDebugGetAllTextures && tex.m_LastBoundFrame != m_CurrentFrame )
		{
			continue;
		}
		if ( !( tex.m_Flags & Texture_t::IS_ALLOCATED ) )
		{
			continue;
		}

		IDirect3DBaseTexture *pD3DTexture = CShaderAPIDx8::GetD3DTexture( hTexture );

		int refCount;
		if ( tex.m_Flags & Texture_t::IS_DEPTH_STENCIL )
		{
			// interface forces us to ignore these
			refCount =  -1;
		}
		else
		{
			refCount = GetD3DTextureRefCount( pD3DTexture );
		}

		pXTextureList[numTextures].pName = tex.m_DebugName.String();
		pXTextureList[numTextures].size = tex.m_SizeBytes * tex.m_NumCopies;
		pXTextureList[numTextures].pGroupName = tex.m_TextureGroupName.String();
		pXTextureList[numTextures].pFormatName = D3DFormatName( ImageLoader::ImageFormatToD3DFormat( tex.GetImageFormat() ) );
		pXTextureList[numTextures].width = tex.GetWidth();
		pXTextureList[numTextures].height = tex.GetHeight();
		pXTextureList[numTextures].depth = tex.GetDepth();
		pXTextureList[numTextures].numLevels = tex.m_NumLevels;
		pXTextureList[numTextures].binds = tex.m_nTimesBoundThisFrame;
		pXTextureList[numTextures].refCount = refCount;
		pXTextureList[numTextures].edram = ( tex.m_Flags & Texture_t::IS_RENDER_TARGET_SURFACE ) != 0;
		pXTextureList[numTextures].procedural = tex.m_NumCopies > 1;
		pXTextureList[numTextures].final = ( tex.m_Flags & Texture_t::IS_FINALIZED ) != 0;
		pXTextureList[numTextures].failed = ( tex.m_Flags & Texture_t::IS_ERROR_TEXTURE ) != 0;
		pXTextureList[numTextures].pwl = ( tex.m_Flags & Texture_t::IS_PWL_CORRECTED ) != 0;

		int reduced = 0;
		if ( tex.m_CreationFlags & TEXTURE_CREATE_EXCLUDED )
		{
			reduced = 1;
		}
		else if ( tex.m_CreationFlags & TEXTURE_CREATE_REDUCED )
		{
			reduced = 2;
		}
		pXTextureList[numTextures].reduced = reduced;

		cacheableState_e cacheableState = CS_STATIC;
		int cacheableSize = 0;
		if ( tex.m_Flags & Texture_t::IS_CACHEABLE )
		{
			if ( g_TextureHeap.IsTextureResident( pD3DTexture )	)
			{
				cacheableState = CS_VALID;
			}
			else
			{
				cacheableState = CS_EVICTED;
			}
			cacheableSize = g_TextureHeap.GetCacheableSize( pD3DTexture );
		}

		pXTextureList[numTextures].cacheableState = cacheableState;
		pXTextureList[numTextures].cacheableSize = cacheableSize;

		numTextures++;
	}

	// build special entries for implicit surfaces/textures
	D3DSURFACE_DESC desc;
	m_pBackBufferSurfaces[BACK_BUFFER_INDEX_DEFAULT]->GetDesc( &desc );
	int size = ImageLoader::GetMemRequired( 
		desc.Width,
		desc.Height,
		0,
		ImageLoader::D3DFormatToImageFormat( desc.Format ),
		false );
	pXTextureList[numTextures].pName = "_rt_BackBuffer";
	pXTextureList[numTextures].size = size;
	pXTextureList[numTextures].pGroupName = TEXTURE_GROUP_RENDER_TARGET_SURFACE;
	pXTextureList[numTextures].pFormatName = D3DFormatName( desc.Format );
	pXTextureList[numTextures].width = desc.Width;
	pXTextureList[numTextures].height = desc.Height;
	pXTextureList[numTextures].depth = 1;
	pXTextureList[numTextures].binds = 1;
	pXTextureList[numTextures].refCount = 1;
	pXTextureList[numTextures].sRGB = IS_D3DFORMAT_SRGB( desc.Format );
	pXTextureList[numTextures].edram = true;
	numTextures++;

	m_pZBufferSurface->GetDesc( &desc );
	pXTextureList[numTextures].pName = "_rt_DepthBuffer";
	pXTextureList[numTextures].size = size;
	pXTextureList[numTextures].pGroupName = TEXTURE_GROUP_RENDER_TARGET_SURFACE;
	pXTextureList[numTextures].pFormatName = D3DFormatName( desc.Format );
	pXTextureList[numTextures].width = desc.Width;
	pXTextureList[numTextures].height = desc.Height;
	pXTextureList[numTextures].depth = 1;
	pXTextureList[numTextures].binds = 1;
	pXTextureList[numTextures].refCount = 1;
	pXTextureList[numTextures].sRGB = IS_D3DFORMAT_SRGB( desc.Format );
	pXTextureList[numTextures].edram = true;
	numTextures++;

	// front buffer resides in DDR
	pXTextureList[numTextures].pName = "_rt_FrontBuffer";
	pXTextureList[numTextures].size = size;
	pXTextureList[numTextures].pGroupName = TEXTURE_GROUP_RENDER_TARGET;
	pXTextureList[numTextures].pFormatName = D3DFormatName( desc.Format );
	pXTextureList[numTextures].width = desc.Width;
	pXTextureList[numTextures].height = desc.Height;
	pXTextureList[numTextures].depth = 1;
	pXTextureList[numTextures].binds = 1;
	pXTextureList[numTextures].refCount = 1;
	pXTextureList[numTextures].sRGB = IS_D3DFORMAT_SRGB( desc.Format );
	numTextures++;

	int totalMemory = 0;
	int cacheableMemory = 0;
	for ( int i = 0; i < numTextures; i++ )
	{
		if ( pXTextureList[i].edram )
		{
			// skip edram based items
			continue;
		}
		totalMemory += pXTextureList[i].size;

		if ( pXTextureList[i].cacheableSize )
		{
			// don't accumulate the cacheable component
			cacheableMemory += pXTextureList[i].cacheableSize;
		}
	}

	Msg( "Total D3D Texture Memory: %.2f MB\n", (float)totalMemory/( 1024.0f * 1024.0f ) );
	Msg( "Static D3D Texture Memory: %.2f MB\n", (float)(totalMemory - cacheableMemory)/( 1024.0f * 1024.0f ) );
	Msg( "Dynamic D3D Texture Heap Memory: %.2f MB\n", (float)g_TextureHeap.GetCacheableHeapSize()/( 1024.0f * 1024.0f ) );

	// transmit to console
	XBX_rTextureList( numTextures, pXTextureList );
#endif

	m_DebugTextureListLock.Unlock();
}


//-----------------------------------------------------------------------------
// Releases/reloads resources when other apps want some memory
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReleaseShaderObjects( bool bReleaseManagedResources /*= true*/ )
{
	ReleaseInternalRenderTargets();
	EvictManagedResourcesInternal();	

	// FIXME: Move into shaderdevice when textures move over.

#ifdef _DEBUG
	// Helps to find the unreleased textures.
	if ( TextureCount() > 0 && bReleaseManagedResources )
	{
		ShaderAPITextureHandle_t hTexture;
		for ( hTexture = m_Textures.Head(); hTexture != m_Textures.InvalidIndex(); hTexture = m_Textures.Next( hTexture ) )
		{
			if ( GetTexture( hTexture ).m_NumCopies == 1 )
			{
				if ( GetTexture( hTexture ).GetTexture() )
				{
					Warning( "Didn't correctly clean up texture 0x%8.8x (%s)\n", hTexture, GetTexture( hTexture ).m_DebugName.String() ); 
				}
			}
			else
			{
				for ( int k = GetTexture( hTexture ).m_NumCopies; --k >= 0; )
				{
					if ( GetTexture( hTexture ).GetTexture( k ) != 0 )
					{
						Warning( "Didn't correctly clean up texture 0x%8.8x (%s)\n", hTexture, GetTexture( hTexture ).m_DebugName.String() ); 
						break;
					}
				}
			}
		}
	}
#endif

	Assert( !bReleaseManagedResources || TextureCount() == 0 );
}

void CShaderAPIDx8::RestoreShaderObjects()
{
	AcquireInternalRenderTargets();
	SetRenderTarget();
}

#if defined( PIX_INSTRUMENTATION )
ConVar pix_break_on_event( "pix_break_on_event", "" );
#endif

//--------------------------------------------------------------------
// PIX instrumentation routines Windows only for now.
// Turn these on with PIX_INSTRUMENTATION in shaderdevicedx8.h
//--------------------------------------------------------------------
void CShaderAPIDx8::BeginPIXEvent( unsigned long color, const char* szName )
{
#if ( defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD ) )
	LOCK_SHADERAPI();

	const char *p = pix_break_on_event.GetString();
	if ( p && V_strlen( p ) )
	{
		if ( V_stristr( szName, p ) != NULL )
		{
			DebuggerBreak();
		}
	}

	#if defined ( DX_TO_GL_ABSTRACTION )
		GLMBeginPIXEvent( szName );

		#if defined( _WIN32 )
			// AMD PerfStudio integration: Call into D3D9.DLL's D3DPERF_BeginEvent() (this gets intercepted by PerfStudio even in GL mode).
			if ( g_pShaderDeviceMgrDx8->m_pBeginEvent )
			{
				wchar_t wszName[128];
				mbstowcs( wszName, szName, 128 );

				g_pShaderDeviceMgrDx8->m_pBeginEvent( 0x2F2F2F2F, wszName );
			}
		#endif
	#elif defined(_X360 )
		char szPIXEventName[32];
		PIXifyName( szPIXEventName, szName );
		PIXBeginNamedEvent( color, szPIXEventName );
	#else // PC
 		if ( PIXError() )
 			return;

		wchar_t wszName[128];
		mbstowcs( wszName, szName, 128 );

		// Fire the PIX event, trapping for errors...
		if ( D3DPERF_BeginEvent( color, wszName ) < 0 )
		{
 			Warning( "PIX error Beginning %s event\n", szName );
 			IncrementPIXError();
		}
	#endif
#endif // #if defined( PIX_INSTRUMENTATION )
}

void CShaderAPIDx8::EndPIXEvent( void )
{
#if ( defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD ) )
	LOCK_SHADERAPI();

	#ifdef _X360
		PIXEndNamedEvent();
	#elif defined( _PS3 )

	#elif defined ( DX_TO_GL_ABSTRACTION )
		GLMEndPIXEvent();

		#if defined( _WIN32 )
			// AMD PerfStudio integration: Call into D3D9.DLL's D3DPERF_EndEvent() (this gets intercepted by PerfStudio even in GL mode).
			if ( g_pShaderDeviceMgrDx8->m_pEndEvent )
			{
				g_pShaderDeviceMgrDx8->m_pEndEvent();
			}
		#endif
	#else // PC
 		if ( PIXError() )
 			return;
	
		int nPIXReturnCode = D3DPERF_EndEvent();

		#if !defined( NVPERFHUD )
			// Fire the PIX event, trapping for errors...
			if ( nPIXReturnCode < 0 )
			{
 				Warning("PIX error ending event\n");
 				IncrementPIXError();
			}
		#endif
	#endif
#endif // #if defined( PIX_INSTRUMENTATION )
}

void CShaderAPIDx8::AdvancePIXFrame()
{
#if defined( PIX_INSTRUMENTATION )
	// Ping PIX when this bool goes from false to true
	if ( r_pix_start.GetBool() && (!m_bPixCapturing) )
	{
		StartPIXInstrumentation();
		m_bPixCapturing = true;
	}

	// If we want to record frames...
	if ( r_pix_recordframes.GetInt() )
	{
		if ( m_nPixFrame == 0 )									// First frame to record
		{
			StartPIXInstrumentation();
			m_nPixFrame++;
		}
		else if( m_nPixFrame == r_pix_recordframes.GetInt() )	// Last frame to record
		{
			EndPIXInstrumentation();
			r_pix_recordframes.SetValue(0);
			m_nPixFrame = 0;
		}
		else
		{
			m_nPixFrame++;										// Recording frames...
		}
	}
#endif
}

// No begin-end for this...use this to put discrete markers in the PIX stream
void CShaderAPIDx8::SetPIXMarker( unsigned long color, const char* szName )
{
#if !defined( POSIX )
#if defined( PIX_INSTRUMENTATION )
	LOCK_SHADERAPI();

	#if defined( DX_TO_GL_ABSTRACTION )
		if ( g_pShaderDeviceMgrDx8->m_pSetMarker )
		{
			wchar_t wszName[128];
			mbstowcs(wszName, szName, 128 );
			g_pShaderDeviceMgrDx8->m_pSetMarker( 0x2F2F2F2F, wszName );
		}
	#elif defined( _X360 )
		#ifndef _DEBUG
			char szPIXMarkerName[32];
			PIXifyName( szPIXMarkerName, szName );
			PIXSetMarker( color, szPIXMarkerName );
		#endif
	#else // PC
		if ( PIXError() )
			return;
		wchar_t wszName[128];
		mbstowcs(wszName, szName, 128 );
		D3DPERF_SetMarker( color, wszName );
	#endif

#endif  // PIX_INSTRUMENTATION
#endif // not POSIX
}

void CShaderAPIDx8::StartPIXInstrumentation()
{
#if defined( PIX_INSTRUMENTATION )
	SetPIXMarker( PIX_VALVE_ORANGE, "Valve_PIX_Capture_Start" );
#endif
}

void CShaderAPIDx8::EndPIXInstrumentation()
{
#if defined( PIX_INSTRUMENTATION )
	SetPIXMarker( PIX_VALVE_ORANGE, "Valve_PIX_Capture_End" );
#endif
}

void CShaderAPIDx8::IncrementPIXError()
{
#if defined( PIX_INSTRUMENTATION ) && !defined( NVPERFHUD )
	m_nPIXErrorCount++;
	if ( m_nPIXErrorCount >= MAX_PIX_ERRORS )
	{
		Warning( "Source engine built with PIX instrumentation, but PIX doesn't seem to have been used to instantiate the game, which is necessary on PC.\n" );
	}
#endif
}

// Have we already hit several PIX errors?
bool CShaderAPIDx8::PIXError()
{
#if defined( PIX_INSTRUMENTATION ) && !defined( NVPERFHUD )
	return m_nPIXErrorCount >= MAX_PIX_ERRORS;
#else
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Check for device lost
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ChangeVideoMode( const ShaderDeviceInfo_t &info )
{
	if ( IsGameConsole() )
		return;

	LOCK_SHADERAPI();

	m_PendingVideoModeChangeConfig = info;
	m_bPendingVideoModeChange = true;

	if ( info.m_DisplayMode.m_nWidth != 0 && info.m_DisplayMode.m_nHeight != 0 )
	{
		m_nWindowWidth = info.m_DisplayMode.m_nWidth;
		m_nWindowHeight = info.m_DisplayMode.m_nHeight;
	}
}


//-----------------------------------------------------------------------------
// Compute fill rate
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ComputeFillRate()
{
	if ( IsGameConsole() )
	{
		// not valid
		return;
	}

	static unsigned char* pBuf = 0;

	int width, height;
	GetWindowSize( width, height );
	// Snapshot; look at total # pixels drawn...
	if ( !pBuf )
	{
		int memSize = ShaderUtil()->GetMemRequired(
									width, 
									height, 
									1, 
									IMAGE_FORMAT_RGB888, 
									false ) + 4;

		pBuf = (unsigned char*)malloc( memSize );
	}

	ReadPixels( 
		0, 
		0, 
		width, 
		height, 
		pBuf, 
		IMAGE_FORMAT_RGB888 );

	int mask = 0xFF;
	int count = 0;
	unsigned char* pRead = pBuf;
	for (int i = 0; i < height; ++i)
	{
		for (int j = 0; j < width; ++j)
		{
			int val = *(int*)pRead;
			count += (val & mask);
			pRead += 3;
		}
	}
}

//-----------------------------------------------------------------------------
// Use this to get the mesh builder that allows us to modify vertex data
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::InFlashlightMode() const
{
	return ShaderUtil()->InFlashlightMode();
}

bool CShaderAPIDx8::IsCascadedShadowMapping() const
{
	return ShaderUtil()->IsCascadedShadowMapping();
}

void CShaderAPIDx8::SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas )
{
	LOCK_SHADERAPI();

	m_CascadedShadowMappingState = state;
	m_CascadedShadowMappingState_LightMapScaled = state;

	// save some PS instructions by pre-multiplying by (1/lightmapScale)
	m_CascadedShadowMappingState_LightMapScaled.m_vLightColor.x *= m_CascadedShadowMappingState_LightMapScaled.m_vLightColor.w;
	m_CascadedShadowMappingState_LightMapScaled.m_vLightColor.y *= m_CascadedShadowMappingState_LightMapScaled.m_vLightColor.w;
	m_CascadedShadowMappingState_LightMapScaled.m_vLightColor.z *= m_CascadedShadowMappingState_LightMapScaled.m_vLightColor.w;

	m_pCascadedShadowMappingDepthTexture = pDepthTextureAtlas;
}

const CascadedShadowMappingState_t &CShaderAPIDx8::GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas, bool bLightMapScale ) const
{
	if ( pDepthTextureAtlas )
		*pDepthTextureAtlas = m_pCascadedShadowMappingDepthTexture;

	if ( bLightMapScale )
	{
		return m_CascadedShadowMappingState_LightMapScaled;
	}
	else
	{
		return m_CascadedShadowMappingState;
	}
}

bool CShaderAPIDx8::IsRenderingPaint() const
{
	return ShaderUtil()->IsRenderingPaint();
}

bool CShaderAPIDx8::InEditorMode() const
{
	return ShaderUtil()->InEditorMode();
}


//-----------------------------------------------------------------------------
// returns the current time in seconds...
//-----------------------------------------------------------------------------
double CShaderAPIDx8::CurrentTime() const
{
	return m_flCurrGameTime;
}


//-----------------------------------------------------------------------------
// Updates the depth bias
//-----------------------------------------------------------------------------
static void OnDepthBiasChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	g_ShaderAPIDX8.UpdateDepthBiasState();
}

#ifdef OSX
static ConVar mat_slopescaledepthbias_decal( "mat_slopescaledepthbias_decal", "-4", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "", OnDepthBiasChanged );
static ConVar mat_depthbias_decal( "mat_depthbias_decal", "-0.25", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "", OnDepthBiasChanged );
static ConVar mat_slopescaledepthbias_normal( "mat_slopescaledepthbias_normal", "0.0f", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "", OnDepthBiasChanged );
static ConVar mat_depthbias_normal( "mat_depthbias_normal", "0.0f", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "", OnDepthBiasChanged );
#else
static ConVar mat_slopescaledepthbias_decal( "mat_slopescaledepthbias_decal", "-2", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "", OnDepthBiasChanged );
static ConVar mat_depthbias_decal( "mat_depthbias_decal", "-0.0000038", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "", OnDepthBiasChanged );
static ConVar mat_slopescaledepthbias_normal( "mat_slopescaledepthbias_normal", "0.0f", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "", OnDepthBiasChanged );
static ConVar mat_depthbias_normal( "mat_depthbias_normal", "0.0f", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "", OnDepthBiasChanged );
#endif

void CShaderAPIDx8::UpdateDepthBiasState()
{
	m_ZBias.m_flOOSlopeScaleDepthBias = mat_slopescaledepthbias_normal.GetFloat();
	m_ZBias.m_flOODepthBias = mat_depthbias_normal.GetFloat();

	m_ZBiasDecal.m_flOOSlopeScaleDepthBias = mat_slopescaledepthbias_decal.GetFloat();
	m_ZBiasDecal.m_flOODepthBias = mat_depthbias_decal.GetFloat();
}

//-----------------------------------------------------------------------------
// Methods called by the transition table that use dynamic state...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ApplyZBias( const DepthTestState_t& shaderState )
{
	// PORTAL 2 hack: Disable slope scale z bias for decals if we have a poly offset matrix set. This avoids decals and overlays rendering
	// on top of other stuff through portal views on the PS3.
	float a = m_DynamicState.m_FastClipEnabled ? 0.0f : m_ZBiasDecal.m_flOOSlopeScaleDepthBias;
    float b = m_ZBias.m_flOOSlopeScaleDepthBias;
    float c = m_ZBiasDecal.m_flOODepthBias;
    float d = m_ZBias.m_flOODepthBias;

	// bias = (s * D3DRS_SLOPESCALEDEPTHBIAS) + D3DRS_DEPTHBIAS, where s is the maximum depth slope of the triangle being rendered
    if ( g_pHardwareConfig->Caps().m_ZBiasAndSlopeScaledDepthBiasSupported )
    {
		float fSlopeScaleDepthBias, fDepthBias;
		if ( shaderState.m_ZBias == SHADER_POLYOFFSET_DECAL )
		{
			fSlopeScaleDepthBias = a;
			fDepthBias = c;
		}
		else if ( shaderState.m_ZBias == SHADER_POLYOFFSET_SHADOW_BIAS )
		{
			fSlopeScaleDepthBias = m_fShadowSlopeScaleDepthBias;
			fDepthBias = m_fShadowDepthBias;
			m_TransitionTable.SetShadowDepthBiasValuesDirty( false );
		}
		else // assume SHADER_POLYOFFSET_DISABLE
		{
			fSlopeScaleDepthBias = b;
			fDepthBias = d;
		}

		if( ReverseDepthOnX360() )
		{
			fSlopeScaleDepthBias = -fSlopeScaleDepthBias;
			fDepthBias = -fDepthBias;
		}

		SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, *((DWORD*) (&fSlopeScaleDepthBias)) );
		SetRenderState( D3DRS_DEPTHBIAS, *((DWORD*) (&fDepthBias)) );
	} 
	else
	{
		MarkAllUserClipPlanesDirty();
		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] |= STATE_CHANGED;
	}
}


//-----------------------------------------------------------------------------
// Used to clear the transition table when we know it's become invalid.
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ClearSnapshots()
{
	LOCK_SHADERAPI();
	m_TransitionTable.Reset();
	InitRenderState();
}


static void KillTranslation( D3DXMATRIX& mat )
{
	mat[3] = 0.0f;
	mat[7] = 0.0f;
	mat[11] = 0.0f;
	mat[12] = 0.0f;
	mat[13] = 0.0f;
	mat[14] = 0.0f;
	mat[15] = 1.0f;
}

static void PrintMatrix( const char *name, const D3DXMATRIX& mat )
{
	int row, col;
	char buf[128];

	Plat_DebugString( name );
	Plat_DebugString( "\n" );
	for( row = 0; row < 4; row++ )
	{
		Plat_DebugString( "    " );
		for( col = 0; col < 4; col++ )
		{
			sprintf( buf, "%f ", ( float )mat( row, col ) );
			Plat_DebugString( buf );
		}
		Plat_DebugString( "\n" );
	}
	Plat_DebugString( "\n" );
}


//-----------------------------------------------------------------------------
// Gets the vertex format for a particular snapshot id
//-----------------------------------------------------------------------------
VertexFormat_t CShaderAPIDx8::ComputeVertexUsage( int num, StateSnapshot_t* pIds ) const
{
	LOCK_SHADERAPI();
	if (num == 0)
		return 0;

	// We don't have to all sorts of crazy stuff if there's only one snapshot
	if ( num == 1 )
	{
		const ShadowShaderState_t& state = m_TransitionTable.GetSnapshotShader( pIds[0] );
		return state.m_VertexUsage;
	}

	Assert( pIds );

	// Aggregating vertex formats is a little tricky;
	// For example, what do we do when two passes want user data? 
	// Can we assume they are the same? For now, I'm going to
	// just print a warning in debug.

	VertexCompressionType_t compression = VERTEX_COMPRESSION_INVALID;
	int userDataSize = 0;
	int numBones = 0;
	int texCoordSize[VERTEX_MAX_TEXTURE_COORDINATES] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	int flags = 0;

	for (int i = num; --i >= 0; )
	{
		const ShadowShaderState_t& state = m_TransitionTable.GetSnapshotShader( pIds[i] );
		VertexFormat_t fmt = state.m_VertexUsage;
		flags |= VertexFlags(fmt);

		VertexCompressionType_t newCompression = CompressionType( fmt );
		if ( ( compression != newCompression ) && ( compression != VERTEX_COMPRESSION_INVALID ) )
		{
			Warning("Encountered a material with two passes that specify different vertex compression types!\n");
			compression = VERTEX_COMPRESSION_NONE; // Be safe, disable compression
		}

		int newNumBones = NumBoneWeights(fmt);
		if ((numBones != newNumBones) && (newNumBones != 0))
		{
			if (numBones != 0)
			{
				Warning("Encountered a material with two passes that use different numbers of bones!\n");
			}
			numBones = newNumBones;
		}

		int newUserSize = UserDataSize(fmt);
		if ((userDataSize != newUserSize) && (newUserSize != 0))
		{
			if (userDataSize != 0)
			{
				Warning("Encountered a material with two passes that use different user data sizes!\n");
			}
			userDataSize = newUserSize;
		}

		for ( int j = 0; j < VERTEX_MAX_TEXTURE_COORDINATES; ++j )
		{
			int newSize = TexCoordSize( j, fmt );
			if ( ( texCoordSize[j] != newSize ) && ( newSize != 0 ) )
			{
				if ( texCoordSize[j] != 0 ) 
				{
					Warning("Encountered a material with two passes that use different texture coord sizes!\n");
				}
				if ( texCoordSize[j] < newSize )
				{
					texCoordSize[j] = newSize;
				}
			}
		}
	}

	return MeshMgr()->ComputeVertexFormat( flags, VERTEX_MAX_TEXTURE_COORDINATES, 
		texCoordSize, numBones, userDataSize );
}

VertexFormat_t CShaderAPIDx8::ComputeVertexFormat( int num, StateSnapshot_t* pIds ) const
{
	LOCK_SHADERAPI();
	VertexFormat_t fmt = ComputeVertexUsage( num, pIds );
	return fmt;
}


//-----------------------------------------------------------------------------
// Commits a range of vertex shader constants
//-----------------------------------------------------------------------------
static void CommitVertexShaderConstantRange( D3DDeviceWrapper *pDevice, const DynamicState_t &desiredState,
	DynamicState_t &currentState, bool bForce, int nFirstConstant, int nCount )
{
	if ( IsX360() )
	{
		// invalid code path for 360, not coded for 360 GPU constant awareness
		Assert( 0 );
		return;
	}

	int nFirstCommit = nFirstConstant;
	int nCommitCount = 0;

	for ( int i = 0; i < nCount; ++i )
	{
		int nVar = nFirstConstant + i; 

		bool bDifferentValue = bForce || ( desiredState.m_pVectorVertexShaderConstant[nVar] != currentState.m_pVectorVertexShaderConstant[nVar] );
		if ( !bDifferentValue )
		{
			if ( nCommitCount != 0 )
			{
				// flush the prior range
				pDevice->SetVertexShaderConstantF( nFirstCommit, desiredState.m_pVectorVertexShaderConstant[nFirstCommit].Base(), nCommitCount );

				memcpy( &currentState.m_pVectorVertexShaderConstant[nFirstCommit], 
					&desiredState.m_pVectorVertexShaderConstant[nFirstCommit], nCommitCount * 4 * sizeof(float) );
			}

			// start of new range
			nFirstCommit = nVar + 1;
			nCommitCount = 0;
		}
		else
		{
			++nCommitCount;
		}
	}

	if ( nCommitCount != 0 )
	{
		// flush range
		pDevice->SetVertexShaderConstantF( nFirstCommit, desiredState.m_pVectorVertexShaderConstant[nFirstCommit].Base(), nCommitCount );

		memcpy( &currentState.m_pVectorVertexShaderConstant[nFirstCommit], 
			&desiredState.m_pVectorVertexShaderConstant[nFirstCommit], nCommitCount * 4 * sizeof(float) );
	}
}


//-----------------------------------------------------------------------------
// Gets the current buffered state... (debug only)
//-----------------------------------------------------------------------------
void CShaderAPIDx8::GetBufferedState( BufferedState_t& state )
{
	memcpy( &state.m_Transform[0], &GetTransform(MATERIAL_MODEL), sizeof(D3DXMATRIX) ); 
	memcpy( &state.m_Transform[1], &GetTransform(MATERIAL_VIEW), sizeof(D3DXMATRIX) ); 
	memcpy( &state.m_Transform[2], &GetTransform(MATERIAL_PROJECTION), sizeof(D3DXMATRIX) ); 
	memcpy( &state.m_Viewport, &m_DynamicState.m_Viewport, sizeof(state.m_Viewport) );
	state.m_PixelShader = ShaderManager()->GetCurrentPixelShader();
	state.m_VertexShader = ShaderManager()->GetCurrentVertexShader();
	for (int i = 0; i < g_pHardwareConfig->GetSamplerCount(); ++i)
	{
		state.m_BoundTexture[i] = m_DynamicState.m_SamplerState[i].m_BoundTexture;
	}
}


//-----------------------------------------------------------------------------
// The shade mode
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ShadeMode( ShaderShadeMode_t mode )
{
	LOCK_SHADERAPI();
	D3DSHADEMODE shadeMode = (mode == SHADER_FLAT) ? D3DSHADE_FLAT : D3DSHADE_GOURAUD;
	if (m_DynamicState.m_ShadeMode != shadeMode)
	{
		m_DynamicState.m_ShadeMode = shadeMode;
		SetSupportedRenderState( D3DRS_SHADEMODE, shadeMode );
	}
}


//-----------------------------------------------------------------------------
// Buffering 2 frames ahead
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableBuffer2FramesAhead( bool bEnable )
{
#ifdef _X360
	m_bBuffer2FramesAhead = bEnable;
	if ( bEnable != m_DynamicState.m_bBuffer2Frames )
	{
		SetRenderState( D3DRS_BUFFER2FRAMES, bEnable );
		m_DynamicState.m_bBuffer2Frames = bEnable;
	}
#endif
}

void CShaderAPIDx8::GetActualProjectionMatrix( float *pMatrix )
{
	memcpy( pMatrix, &GetProjectionMatrix(), sizeof( D3DMATRIX ) );
}

//-----------------------------------------------------------------------------
// 
// note this duplicates some constants for VS too (helps optimize away some PS ops for shaders that take advantage of it - currently only spritecard)
// PS: sets c13, c14, iConstant
// VS: sets c12, c13
//
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetDepthFeatheringShaderConstants( int iConstant, float fDepthBlendScale )
{
	float fConstantValues[4];

	fConstantValues[0] = 50.0f / fDepthBlendScale;
	// The old depth feathering shader code worked in proj. space, which wasn't a sane place to compute depth feathering on PS3 through portals 
	// (because we use an oblique projection on PS3 to simulate user clip planes). The new methods computes depth feathering in viewspace, so 
	// we need to convert fDepthBlendScale to something which looks reasonable vs. the computing the depth blend in proj. space.
	const float flDepthFeatherFudgeFactor = 1.5f;		
	fConstantValues[1] = flDepthFeatherFudgeFactor * fDepthBlendScale;
	fConstantValues[2] = flDepthFeatherFudgeFactor / fDepthBlendScale;
	fConstantValues[3] = 0.0f;
				
	VMatrix projToViewMatrix;
	MatrixInverseGeneral( *reinterpret_cast< const VMatrix * >( &GetProjectionMatrix() ), projToViewMatrix );
	projToViewMatrix = projToViewMatrix.Transpose();

	// Send down rows 2 (Z) and 3 (W), because that's all the shader needs to recover worldspace Z.
	SetPixelShaderConstantInternal( DEPTH_FEATHER_PROJ_TO_VIEW_Z, &projToViewMatrix.m[2][0], 2 );
	// same for VS 
	SetVertexShaderConstantInternal( VERTEX_SHADER_SHADER_SPECIFIC_CONST_12, &projToViewMatrix.m[2][0], 2 );

	SetPixelShaderConstantInternal( iConstant, fConstantValues );
}

void CShaderAPIDx8::FlipCulling( bool bFlipCulling )
{
	if ( bFlipCulling != m_bFlipCulling )
	{
		m_bFlipCulling = bFlipCulling;
		D3DCULL nCullMode = m_DynamicState.m_CullMode;
		if ( m_bFlipCulling )
		{
			if ( nCullMode != D3DCULL_NONE )
			{
				nCullMode = ( nCullMode == D3DCULL_CW ) ? D3DCULL_CCW : D3DCULL_CW;
			}
		}

		SetRenderState( D3DRS_CULLMODE, nCullMode );
	}
}


//-----------------------------------------------------------------------------
// Cull mode..
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetCullModeState( bool bEnable, D3DCULL nDesiredCullMode )
{ 
	nDesiredCullMode = bEnable ? nDesiredCullMode : D3DCULL_NONE;
	if ( nDesiredCullMode != m_DynamicState.m_CullMode )
	{
		D3DCULL nCullMode = nDesiredCullMode;
		if ( m_bFlipCulling )
		{
			if ( nCullMode != D3DCULL_NONE )
			{
				nCullMode = ( nCullMode == D3DCULL_CW ) ? D3DCULL_CCW : D3DCULL_CW;
			}
		}

		SetRenderState( D3DRS_CULLMODE, nCullMode );
		m_DynamicState.m_CullMode = nDesiredCullMode;
	}
}

void CShaderAPIDx8::ApplyCullEnable( bool bEnable )
{
	m_DynamicState.m_bCullEnabled = bEnable;
/*
	int nCullOverride = GetIntRenderingParameter( INT_RENDERPARM_CULL_OVERRIDE );
	if ( nCullOverride == RENDER_PARM_CULL_MODE_OVERRIDE_CW )
	{
		m_DynamicState.m_DesiredCullMode = D3DCULL_CW;
		m_DynamicState.m_bCullEnabled = true;
	}
	else if ( nCullOverride == RENDER_PARM_CULL_MODE_OVERRIDE_CCW )
	{
		m_DynamicState.m_DesiredCullMode = D3DCULL_CCW;
		m_DynamicState.m_bCullEnabled = true;
	}
*/
	SetCullModeState( m_DynamicState.m_bCullEnabled, m_DynamicState.m_DesiredCullMode );
}

void CShaderAPIDx8::CullMode( MaterialCullMode_t nCullMode )
{
	LOCK_SHADERAPI();
	D3DCULL nNewCullMode;
	switch( nCullMode )
	{
		case MATERIAL_CULLMODE_CCW:
			nNewCullMode = D3DCULL_CCW;	// Culls back-facing polygons (default)
			break;

		case MATERIAL_CULLMODE_CW:
			nNewCullMode = D3DCULL_CW;	// Culls front-facing polygons
			break;
		
		case MATERIAL_CULLMODE_NONE:
			nNewCullMode = D3DCULL_NONE;	// Culls nothing
			break;

		default:
			Warning( "CullMode: invalid cullMode\n" );
			return;
	}

	if ( m_DynamicState.m_DesiredCullMode != nNewCullMode )
	{
		m_DynamicState.m_DesiredCullMode = nNewCullMode;
		SetCullModeState( m_DynamicState.m_bCullEnabled, m_DynamicState.m_DesiredCullMode );
	}
}

void CShaderAPIDx8::FlipCullMode( void )
{
	LOCK_SHADERAPI();
	switch( m_DynamicState.m_DesiredCullMode )
	{
	case D3DCULL_CCW:
		m_DynamicState.m_DesiredCullMode = D3DCULL_CW;
		break;

	case D3DCULL_CW:
		m_DynamicState.m_DesiredCullMode = D3DCULL_CCW;
		break;
	
	case D3DCULL_NONE:
		m_DynamicState.m_DesiredCullMode = D3DCULL_NONE;
		break;
		
	default:
		Warning( "FlipCullMode: invalid cullMode\n" );
		return;
	}
	
	SetCullModeState( m_DynamicState.m_bCullEnabled, m_DynamicState.m_DesiredCullMode );	
}

static ConVar mat_alphacoverage( "mat_alphacoverage", IsX360() ? "0" : "1", FCVAR_DEVELOPMENTONLY );
void CShaderAPIDx8::ApplyAlphaToCoverage( bool bEnable )
{
	if ( mat_alphacoverage.GetBool() )
	{
		if ( bEnable )
			EnableAlphaToCoverage();
		else
			DisableAlphaToCoverage();
	}
	else
	{
		DisableAlphaToCoverage();
	}
}

//-----------------------------------------------------------------------------
// Returns the current cull mode of the current material (for selection mode only)
//-----------------------------------------------------------------------------
D3DCULL CShaderAPIDx8::GetCullMode() const
{
	Assert( m_pMaterial );

	if ( m_pMaterial->GetMaterialVarFlag( MATERIAL_VAR_NOCULL ) )
		return D3DCULL_NONE;

	return m_DynamicState.m_DesiredCullMode;
}

void CShaderAPIDx8::SetRasterState( const ShaderRasterState_t& state )
{
	// FIXME: Implement!
}


void CShaderAPIDx8::ForceDepthFuncEquals( bool bEnable )
{
	LOCK_SHADERAPI();
	if ( !g_pShaderDeviceDx8->IsDeactivated() )
	{
		m_TransitionTable.ForceDepthFuncEquals( bEnable );
	}
}

void CShaderAPIDx8::OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable )
{
	LOCK_SHADERAPI();
	if ( !g_pShaderDeviceDx8->IsDeactivated() )
	{
		m_TransitionTable.OverrideDepthEnable( bEnable, bDepthWriteEnable, bDepthTestEnable );
	}
}

void CShaderAPIDx8::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{
	LOCK_SHADERAPI();
	if ( !g_pShaderDeviceDx8->IsDeactivated() )
	{
		m_TransitionTable.OverrideAlphaWriteEnable( bOverrideEnable, bAlphaWriteEnable );
	}
}

void CShaderAPIDx8::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{
	LOCK_SHADERAPI();
	if ( !g_pShaderDeviceDx8->IsDeactivated() )
	{
		m_TransitionTable.OverrideColorWriteEnable( bOverrideEnable, bColorWriteEnable );
	}
}

void CShaderAPIDx8::UpdateFastClipUserClipPlane( void )
{
	float plane[4];
	switch( m_DynamicState.m_HeightClipMode )
	{
	case MATERIAL_HEIGHTCLIPMODE_DISABLE:
		EnableFastClip( false );
		break;
	case MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT:
		plane[0] = 0.0f;
		plane[1] = 0.0f;
		plane[2] = 1.0f;
		plane[3] = m_DynamicState.m_HeightClipZ;
		EnableFastClip( true );
		SetFastClipPlane(plane);
		break;
	case MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT:
		plane[0] = 0.0f;
		plane[1] = 0.0f;
		plane[2] = -1.0f;
		plane[3] = -m_DynamicState.m_HeightClipZ;
		EnableFastClip( true );
		SetFastClipPlane(plane);
		break;
	}
}

void CShaderAPIDx8::SetHeightClipZ( float z )
{
	LOCK_SHADERAPI();
	if( z != m_DynamicState.m_HeightClipZ )
	{
		m_DynamicState.m_HeightClipZ = z;
		UpdateVertexShaderFogParams();
		UpdateFastClipUserClipPlane();
		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] |= STATE_CHANGED;
	}
}

void CShaderAPIDx8::SetHeightClipMode( MaterialHeightClipMode_t heightClipMode )
{
	LOCK_SHADERAPI();
	if( heightClipMode != m_DynamicState.m_HeightClipMode )
	{
		m_DynamicState.m_HeightClipMode = heightClipMode;
		UpdateVertexShaderFogParams();
		UpdateFastClipUserClipPlane();
		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] |= STATE_CHANGED;
	}
}

void CShaderAPIDx8::SetClipPlane( int index, const float *pPlane )
{
	LOCK_SHADERAPI();
	Assert( index < g_pHardwareConfig->MaxUserClipPlanes() && index >= 0 );

	// NOTE: The plane here is specified in *world space*
	// NOTE: This is done because they assume Ax+By+Cz+Dw = 0 (where w = 1 in real space)
	// while we use Ax+By+Cz=D
	D3DXPLANE plane;
	plane.a = pPlane[0];
	plane.b = pPlane[1];
	plane.c = pPlane[2];
	plane.d = -pPlane[3];

	if ( plane != m_DynamicState.m_UserClipPlaneWorld[index] )
	{
		m_DynamicState.m_UserClipPlaneChanged |= ( 1 << index );
		m_DynamicState.m_UserClipPlaneWorld[index] = plane;
	}
}


//-----------------------------------------------------------------------------
// Converts a D3DXMatrix to a VMatrix and back
//-----------------------------------------------------------------------------
void CShaderAPIDx8::D3DXMatrixToVMatrix( const D3DXMATRIX &in, VMatrix &out )
{
	MatrixTranspose( *(const VMatrix*)&in, out );
}

void CShaderAPIDx8::VMatrixToD3DXMatrix( const VMatrix &in, D3DXMATRIX &out )
{
	MatrixTranspose( in, *(VMatrix*)&out );
}

	
//-----------------------------------------------------------------------------
// Mark all user clip planes as being dirty
//-----------------------------------------------------------------------------
void CShaderAPIDx8::MarkAllUserClipPlanesDirty()
{
	m_DynamicState.m_UserClipPlaneChanged |= ( 1 << g_pHardwareConfig->MaxUserClipPlanes() ) - 1;
	m_DynamicState.m_bFastClipPlaneChanged = true;
}


//-----------------------------------------------------------------------------
// User clip plane override
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableUserClipTransformOverride( bool bEnable )
{
	LOCK_SHADERAPI();
	if ( m_DynamicState.m_bUserClipTransformOverride != bEnable )
	{
		m_DynamicState.m_bUserClipTransformOverride = bEnable;
		MarkAllUserClipPlanesDirty();
	}
}


//-----------------------------------------------------------------------------
// Specify user clip transform
//-----------------------------------------------------------------------------
void CShaderAPIDx8::UserClipTransform( const VMatrix &worldToProjection )
{
	LOCK_SHADERAPI();
	D3DXMATRIX dxWorldToProjection;
	VMatrixToD3DXMatrix( worldToProjection, dxWorldToProjection );

	if ( m_DynamicState.m_UserClipTransform != dxWorldToProjection )
	{
		m_DynamicState.m_UserClipTransform = dxWorldToProjection;
		if ( m_DynamicState.m_bUserClipTransformOverride )
		{
			MarkAllUserClipPlanesDirty();
		}
	}
}


//-----------------------------------------------------------------------------
// Enables a user clip plane
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableClipPlane( int index, bool bEnable )
{
	LOCK_SHADERAPI();
	Assert( index < g_pHardwareConfig->MaxUserClipPlanes() && index >= 0 );
	if( ( m_DynamicState.m_UserClipPlaneEnabled & ( 1 << index ) ? true : false ) != bEnable )
	{
		if ( bEnable )
		{
			m_DynamicState.m_UserClipPlaneEnabled |= ( 1 << index );
		}
		else
		{
			m_DynamicState.m_UserClipPlaneEnabled &= ~( 1 << index );
		}
		SetRenderState( D3DRS_CLIPPLANEENABLE, m_DynamicState.m_UserClipPlaneEnabled );
	}
}

// Alternate oblique projectio matrix code adapted from http://www.terathon.com/code/oblique.html
// Fixes for D3D Z range adapted from this thread: http://www.gamedev.net/community/forums/topic.asp?topic_id=398719
static inline float sgn(float a)
{
	if (a > 0.0F) return (1.0F);
	if (a < 0.0F) return (-1.0F);
	return (0.0F);
}

// Clip plane must be in view space
void ApplyClipPlaneToProjectionMatrix( D3DXMATRIX &projMatrix, const Vector4D& clipPlane )
{
	Vector4D q;

	// Invalid projection matrix. (Sometimes we get in here with an identity transform as the projection matrix)
	if ( projMatrix._43 == 0.0f )
	{
		return;
	}

	// Calculate the clip-space corner point opposite the clipping plane
	// as (sgn(clipPlane.x), sgn(clipPlane.y), 1, 1) and
	// transform it into camera space by multiplying it
	// by the inverse of the projection matrix

	q.x = ( sgn( clipPlane.x ) + projMatrix._31 ) / projMatrix._11;
	q.y = ( sgn( clipPlane.y ) + projMatrix._32 ) / projMatrix._22;
	q.z = -1.0f;
	q.w = ( 1.0f + projMatrix._33 ) / projMatrix._43;

	// Calculate the scaled plane vector
	Vector4D c = clipPlane * ( 1.0f / DotProduct4D( clipPlane, q ) );

	// Replace the third row of the projection matrix
	projMatrix._13 = c.x;
	projMatrix._23 = c.y;
	projMatrix._33 = c.z;
	projMatrix._43 = c.w;
}

ConVar mat_alternatefastclipalgorithm( "mat_alternatefastclipalgorithm", "1" );

//-----------------------------------------------------------------------------
// Recomputes the fast-clip plane matrices based on the current fast-clip plane
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitFastClipPlane( )
{
	// Don't bother recomputing if unchanged or disabled
	if ( !m_DynamicState.m_bFastClipPlaneChanged || !m_DynamicState.m_FastClipEnabled )
		return;

	m_DynamicState.m_bFastClipPlaneChanged = false;

	D3DXMatrixIdentity( &m_CachedFastClipProjectionMatrix );

	// Compute worldToProjection - need inv. transpose for transforming plane.
	D3DXMATRIX viewToProjInvTrans, viewToProjInv, viewToProj = GetTransform(MATERIAL_PROJECTION);
	viewToProj._43 *= 0.5f;		// pull in zNear because the shear in effect 
								// moves it out: clipping artifacts when looking down at water 
								// could occur if this multiply is not done

	D3DXMATRIX worldToViewInvTrans, worldToViewInv, worldToView = GetUserClipTransform();

	D3DXMatrixInverse( &worldToViewInv, NULL, &worldToView );

	// PS3's Cg likes things in row-major rather than column-major, so let's just save ourselves the work of fixing every shader and call it even?
#ifdef _PS3
	worldToViewInvTrans = worldToViewInv;
#else // _PS3
	D3DXMatrixTranspose( &worldToViewInvTrans, &worldToViewInv ); 	
#endif // !_PS3

	D3DXMatrixInverse( &viewToProjInv, NULL, &viewToProj );
#ifdef _PS3
	viewToProjInvTrans = viewToProjInv;
#else // _PS3
	D3DXMatrixTranspose( &viewToProjInvTrans, &viewToProjInv ); 
#endif // !_PS3

	if ( !mat_alternatefastclipalgorithm.GetBool() )
	{
		// OLD code path
		D3DXPLANE plane;
		D3DXPlaneNormalize( &plane, &m_DynamicState.m_FastClipPlane );
		D3DXVECTOR4 clipPlane( plane.a, plane.b, plane.c, plane.d );

		// transform clip plane into view space
		D3DXVec4Transform( &clipPlane, &clipPlane, &worldToViewInvTrans );

		// transform clip plane into projection space
		D3DXVec4Transform( &clipPlane, &clipPlane, &viewToProjInvTrans );

#define ALLOW_FOR_FASTCLIPDUMPS 0

#if (ALLOW_FOR_FASTCLIPDUMPS == 1)
		static ConVar shader_dumpfastclipprojectioncoords( "shader_dumpfastclipprojectioncoords", "0", 0, "dump fast clip projected matrix" );
		if( shader_dumpfastclipprojectioncoords.GetBool() )
			DevMsg( "Fast clip plane projected coordinates: %f %f %f %f", clipPlane.x, clipPlane.y, clipPlane.z, clipPlane.w );
#endif

		if( (clipPlane.z * clipPlane.w) <= -0.4f ) // a plane with (z*w) > -0.4 at this point is behind the camera and will cause graphical glitches. Toss it. (0.4 found through experimentation)
		{
#if (ALLOW_FOR_FASTCLIPDUMPS == 1)
			if( shader_dumpfastclipprojectioncoords.GetBool() )
				DevMsg( "    %f %f %f %f\n", clipPlane.x, clipPlane.y, clipPlane.z, clipPlane.w );
#endif

			D3DXVec4Normalize( &clipPlane, &clipPlane );

			//if ((fabs(clipPlane.z) > 0.01) && (fabs(clipPlane.w) > 0.01f))  
			{
				// put projection space clip plane in Z column
				m_CachedFastClipProjectionMatrix._13 = clipPlane.x;
				m_CachedFastClipProjectionMatrix._23 = clipPlane.y;
				m_CachedFastClipProjectionMatrix._33 = clipPlane.z;
				m_CachedFastClipProjectionMatrix._43 = clipPlane.w;
			}
		}
#if (ALLOW_FOR_FASTCLIPDUMPS == 1)
		else
		{
			if( shader_dumpfastclipprojectioncoords.GetBool() )
				DevMsg( "\n" ); //finish off the line above
		}
#endif

		m_CachedFastClipProjectionMatrix = viewToProj * m_CachedFastClipProjectionMatrix;
	}
	else
	{
		// NEW code path

		D3DXPLANE plane;
		D3DXPlaneNormalize( &plane, &m_DynamicState.m_FastClipPlane );
		D3DXVECTOR4 clipPlane( plane.a, plane.b, plane.c, plane.d );

		// transform clip plane into view space
		D3DXVec4Transform( &clipPlane, &clipPlane, &worldToViewInvTrans );

		m_CachedFastClipProjectionMatrix = GetTransform( MATERIAL_PROJECTION );

		// Fuck with proj matrix
		ApplyClipPlaneToProjectionMatrix( m_CachedFastClipProjectionMatrix, Vector4D( clipPlane.x, clipPlane.y, clipPlane.z, clipPlane.w ) );
	}

	// Update the cached polyoffset matrix (with clip) too:
	ComputePolyOffsetMatrix( m_CachedFastClipProjectionMatrix, m_CachedFastClipPolyOffsetProjectionMatrix );

	// PORTAL 2 hack: Disable slope scale z bias for decals if we have a poly offset matrix set. This avoids decals and overlays rendering
	// on top of other stuff through portal views on the PS3.
	if ( m_TransitionTable.CurrentShadowState() )
	{
		ApplyZBias( m_TransitionTable.CurrentShadowState()->m_DepthTestState );
	}
}

//-----------------------------------------------------------------------------
// Sets the fast-clip plane; but doesn't update the matrices
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetFastClipPlane( const float *pPlane )
{	
	LOCK_SHADERAPI();
	D3DXPLANE plane;
	plane.a = pPlane[0];
	plane.b = pPlane[1];
	plane.c = pPlane[2];
	plane.d = -pPlane[3];
	if ( plane != m_DynamicState.m_FastClipPlane )
	{
		UpdateVertexShaderFogParams();

		m_DynamicState.m_FastClipPlane = plane;

		// Mark a dirty bit so when it comes time to commit view + projection transforms,
		// we also update the fast clip matrices
		m_DynamicState.m_bFastClipPlaneChanged = true;

		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] |= STATE_CHANGED;
	}
}


//-----------------------------------------------------------------------------
// Enables/disables fast-clip mode
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableFastClip( bool bEnable )
{
	LOCK_SHADERAPI();
	if( m_DynamicState.m_FastClipEnabled != bEnable )
	{
		UpdateVertexShaderFogParams();

		m_DynamicState.m_FastClipEnabled = bEnable;

		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] |= STATE_CHANGED;
	}
}

/*
// -----------------------------------------------------------------------------
// SetInvariantClipVolume - This routine takes six planes as input and sets the
// appropriate Direct3D user clip plane state
// What we mean by "invariant clipping" here is that certain devices implement
// user clip planes at the raster level, which means that multi-pass rendering
// where one pass is unclipped (such as base geometry) and another pass *IS*
// clipped (such as flashlight geometry), there is no z-fighting since the
// clipping is implemented at the raster level in an "invariant" way
// -----------------------------------------------------------------------------
void CShaderAPIDx8::SetInvariantClipVolume( Frustum_t *pFrustumPlanes )
{
	// Only do this on modern nVidia hardware, which does invariant clipping
	if ( m_VendorID == VENDORID_NVIDIA )
	{
		if ( pFrustumPlanes )
		{
//			if ()
//			{
//
//			}

			for (int i=0; i<6; i++)
			{
				const cplane_t *pPlane = pFrustumPlanes->GetPlane(i);

				SetClipPlane( i, (float *) &pPlane->normal );
				EnableClipPlane( i, true );

//	FRUSTUM_RIGHT		= 0,
//	FRUSTUM_LEFT		= 1,
//	FRUSTUM_TOP			= 2,
//	FRUSTUM_BOTTOM		= 3,
//	FRUSTUM_NEARZ		= 4,
//	FRUSTUM_FARZ		= 5,

			}
		}
		else // NULL disables the invariant clip volume...
		{
			for (int i=0; i<6; i++)
			{
				EnableClipPlane( i, false );
			}
		}
	}
}
*/


//-----------------------------------------------------------------------------
// Vertex blending
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetNumBoneWeights( int numBones )
{
	LOCK_SHADERAPI();
	if (m_DynamicState.m_NumBones != numBones)
	{
		m_DynamicState.m_NumBones = numBones;
	}
}

void CShaderAPIDx8::EnableHWMorphing( bool bEnable )
{
	LOCK_SHADERAPI();
	
	if ( bEnable )
	{
		Assert( 0 );
		Warning( "CShaderAPIDx8::EnableHWMorphing: HW morphing is getting enabled! This is not a supported code path - bad things are going to happen!\n" );
	}

	if ( m_DynamicState.m_bHWMorphingEnabled != bEnable )
	{
		m_DynamicState.m_bHWMorphingEnabled = bEnable;
	}
}

void CShaderAPIDx8::EnabledSRGBWrite( bool bEnabled )
{
	m_DynamicState.m_bSRGBWritesEnabled = bEnabled;

	if ( g_pHardwareConfig->GetDXSupportLevel() >= 92 )
	{
		UpdatePixelFogColorConstant();

		//if ( bEnabled && g_pHardwareConfig->NeedsShaderSRGBConversion() )
		//	BindTexture( SHADER_SAMPLER15, m_hLinearToGammaTableTexture );
		//else
		//	BindTexture( SHADER_SAMPLER15, m_hLinearToGammaTableIdentityTexture );
	}
}

void CShaderAPIDx8::SetSRGBWrite( bool bState )
{
	SetSupportedRenderState( D3DRS_SRGBWRITEENABLE, ( bState ) ? 1 : 0 );
	m_DynamicState.m_bSRGBWritesEnabled = bState;
}


//-----------------------------------------------------------------------------
// Fog methods...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::UpdatePixelFogColorConstant( bool bMultiplyByToneMapScale )
{
	Assert( HardwareConfig()->GetDXSupportLevel() >= 92 );
	float flDestAlphaDepthRange = GetFloatRenderingParameter( FLOAT_RENDERPARM_DEST_ALPHA_DEPTH_SCALE );
	float flInvDestAlphaDepthRange = flDestAlphaDepthRange > 0.0f ? 1.0f / flDestAlphaDepthRange : 0.0f;
	Vector4D vecFogColor( 0.0f, 0.0f, 0.0f, flInvDestAlphaDepthRange );

	switch( GetSceneFogMode() )
	{
		case MATERIAL_FOG_NONE:
			// Don't worry about fog color since we aren't going to use it
			// Even so, we do need to set LINEAR_FOG_COLOR since w component controls soft particles
			break; 

		case MATERIAL_FOG_LINEAR:
			{
				//setup the fog for mixing linear fog in the pixel shader so that it emulates ff range fog
				if( m_DynamicState.m_bSRGBWritesEnabled ) 
				{
					vecFogColor.AsVector3D() = m_DynamicState.m_vecPixelFogColorLinear.AsVector3D();
				}
				else
				{
					vecFogColor.AsVector3D() = m_DynamicState.m_vecPixelFogColor.AsVector3D();
				}

			}
			break;

		case MATERIAL_FOG_LINEAR_BELOW_FOG_Z:
			{
				//water fog has been around a while and has never tonemap scaled, and has always been in linear space
				if( g_pHardwareConfig->NeedsShaderSRGBConversion() )
				{
					// srgb in ps2b uses the 2.2 curve
					for( int i = 0; i < 3; ++i )
					{
						vecFogColor[i] = pow( m_DynamicState.m_vecPixelFogColor[i], 2.2f );
					}
				}
				else
				{
					vecFogColor.AsVector3D() = m_DynamicState.m_vecPixelFogColorLinear.AsVector3D(); //this is how water fog color has always been setup in the past
				}
			}
			break;

			NO_DEFAULT;
	};	

	if( bMultiplyByToneMapScale && ( (!m_DynamicState.m_bFogGammaCorrectionDisabled) && (g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER ) ) )
	{
		vecFogColor.AsVector3D() *= m_ToneMappingScale.x;
	}

	SetPixelShaderConstantInternal( LINEAR_FOG_COLOR, vecFogColor.Base() );
}

FogMethod_t CShaderAPIDx8::ComputeFogMethod( ShaderFogMode_t shaderFogMode, MaterialFogMode_t sceneFogMode, bool bVertexFog )
{
	if ( sceneFogMode == MATERIAL_FOG_LINEAR_BELOW_FOG_Z )
	{
		return FOGMETHOD_PIXELSHADERFOG;
	}

	if ( HardwareConfig()->GetDXSupportLevel() <= 90 )
	{
		// ps20 always uses fixed-function fog if there is no water.
		return FOGMETHOD_FIXEDFUNCTIONVERTEXFOG;
	}

	// use the vmt's $vertexfog param to decide what to do.
	if ( bVertexFog )
	{
		return FOGMETHOD_VERTEXFOGBLENDEDINPIXELSHADER;
	}
	else
	{
		return FOGMETHOD_PIXELSHADERFOG;
	}
}




Vector CShaderAPIDx8::CalculateFogColorConstant( D3DCOLOR *pPackedColorOut, FogMethod_t fogMethod, ShaderFogMode_t fogMode, 
												 bool bDisableFogGammaCorrection, bool bSRGBWritesEnabled )
{
	if ( fogMode == SHADER_FOGMODE_DISABLED )
	{
		return Vector( 0, 0, 0 );
	}
	
	bool bShouldGammaCorrect = true;			// By default, we'll gamma correct.

	uint8 r = 0, g = 0, b = 0;								// Black fog

	switch( fogMode )
	{
		case SHADER_FOGMODE_BLACK:				// Additive decals
			bShouldGammaCorrect = false;
			break;

		case SHADER_FOGMODE_OO_OVERBRIGHT:
		case SHADER_FOGMODE_GREY:				// Mod2x decals
			r = g = b = 128;
			break;

		case SHADER_FOGMODE_WHITE:				// Multiplicative decals
			r = g = b = 255;
			bShouldGammaCorrect = false;
			break;

		case SHADER_FOGMODE_FOGCOLOR:
			GetSceneFogColor( &r, &g, &b );		// Scene fog color
			break;
		NO_DEFAULT
	}

	bShouldGammaCorrect &= !bDisableFogGammaCorrection;



	const float fColorScale = 1.0f / 255.0f;

	Vector vecRet;
	vecRet.x = (float)(r) * fColorScale;
	vecRet.y = (float)(g) * fColorScale;
	vecRet.z = (float)(b) * fColorScale;
	if ( pPackedColorOut )
	{
		D3DCOLOR color;
		if ( bShouldGammaCorrect )
		{
			color = ComputeGammaCorrectedFogColor( r, g, b, bSRGBWritesEnabled );
		}
		else
		{
			color = D3DCOLOR_ARGB( 255, r, g, b );
		}
		
		( *pPackedColorOut ) = color;
	}

	return vecRet;

}

struct FogConstants_t
{
	Vector m_vecFogColorConstant[FOGMETHOD_NUMVALUES][SHADER_FOGMODE_NUMFOGMODES][2][2]; // [fogMode][bDisableFogGammaCorrection][bSRGBWritesEnabled]
	Vector m_vecFogColorConstantLinear[FOGMETHOD_NUMVALUES][SHADER_FOGMODE_NUMFOGMODES][2][2]; // [fogMode][bDisableFogGammaCorrection][bSRGBWritesEnabled]
	D3DCOLOR m_nPackedFogColorConstants[FOGMETHOD_NUMVALUES][SHADER_FOGMODE_NUMFOGMODES][2][2];
};

static FogConstants_t s_CurFogConstants;
static bool s_bFirstTimeGeneratingConstants = true;

void CShaderAPIDx8::RegenerateFogConstants( void )
{
	for( int nMethod = 0; nMethod < FOGMETHOD_NUMVALUES; nMethod++ )
	{
		for( int nMode = 0; nMode < SHADER_FOGMODE_NUMFOGMODES; nMode++ )
		{
			if ( ( ! s_bFirstTimeGeneratingConstants ) && ( nMode != SHADER_FOGMODE_FOGCOLOR ) )
				continue;									// state changes don't effect any mode execpt FOGCOLOR
			for( int nDisabledFogGammaCorrection = 0; nDisabledFogGammaCorrection < 2; nDisabledFogGammaCorrection++ )
			{
				for( int nSRGBWritesEnabled = 0; nSRGBWritesEnabled < 2 ; nSRGBWritesEnabled++ )
				{
					s_CurFogConstants.m_vecFogColorConstant[nMethod][nMode][nDisabledFogGammaCorrection][nSRGBWritesEnabled] = CalculateFogColorConstant(
						&( s_CurFogConstants.m_nPackedFogColorConstants[nMethod][nMode][nDisabledFogGammaCorrection][nSRGBWritesEnabled] ),
						( FogMethod_t ) nMethod, ( ShaderFogMode_t ) nMode, ( nDisabledFogGammaCorrection != 0 ), ( nSRGBWritesEnabled != 0 ) );

					// generate linear versions
					for( int i = 0; i < 3; i++ )
					{
						float *pComponent = &( s_CurFogConstants.m_vecFogColorConstantLinear[nMethod][nMode][nDisabledFogGammaCorrection][nSRGBWritesEnabled][i] );
						float *pSrcComponent = &( s_CurFogConstants.m_vecFogColorConstant[nMethod][nMode][nDisabledFogGammaCorrection][nSRGBWritesEnabled][i] );
						*( pComponent ) = GammaToLinear_HardwareSpecific( *pSrcComponent );
					}
				}
			}
		}
	}
	s_bFirstTimeGeneratingConstants = false;				// next time around, don't regenerate the whole table
}



void CShaderAPIDx8::ApplyFogMode( ShaderFogMode_t fogMode, bool bVertexFog, bool bSRGBWritesEnabled, bool bDisableFogGammaCorrection )
{
	FogMethod_t fogMethod = ComputeFogMethod( fogMode, m_SceneFogMode, bVertexFog );

	if ( fogMethod != FOGMETHOD_PIXELSHADERFOG )
	{
		UpdateVertexShaderFogParams( fogMode, bVertexFog );
	}

	if ( fogMode == SHADER_FOGMODE_DISABLED )
	{
		// Turn off fixed-function fog if it's currently enabled.
		EnableFixedFunctionFog( false );

		if ( m_DelayedShaderConstants.iPixelShaderFogParams != -1 )
		{
			SetPixelShaderFogParams( m_DelayedShaderConstants.iPixelShaderFogParams, fogMode );
		}
		return;
	}
	
	D3DCOLOR fixedFunctionFogPackedColor = s_CurFogConstants.m_nPackedFogColorConstants[fogMethod][fogMode][bDisableFogGammaCorrection][bSRGBWritesEnabled ];
	Vector vecFogColor = s_CurFogConstants.m_vecFogColorConstant[fogMethod][fogMode][bDisableFogGammaCorrection][bSRGBWritesEnabled];
	Vector vecFogColorLinear = s_CurFogConstants.m_vecFogColorConstantLinear[fogMethod][fogMode][bDisableFogGammaCorrection][bSRGBWritesEnabled];

	// Enable fixed-function fog if we are using it.
	EnableFixedFunctionFog( ( fogMethod == FOGMETHOD_FIXEDFUNCTIONVERTEXFOG ) && ( m_SceneFogMode != MATERIAL_FOG_NONE ) );

	if( m_DelayedShaderConstants.iPixelShaderFogParams != -1 )
	{
		// We set these for all methods since the pixel shader is always effectively doing fog, even when fog is disabled.
		SetPixelShaderFogParams( m_DelayedShaderConstants.iPixelShaderFogParams, fogMode );
	}

	//?m_DynamicState.m_bFogGammaCorrectionDisabled = !bShouldGammaCorrect;

	m_DynamicState.m_vecPixelFogColor.AsVector3D() = vecFogColor;
	m_DynamicState.m_vecPixelFogColorLinear.AsVector3D() = vecFogColorLinear;

	if ( fogMethod != FOGMETHOD_FIXEDFUNCTIONVERTEXFOG )
	{
		// All fog methods other than FOGMETHOD_FIXEDFUNCTIONVERTEXFOG use fog color int he pixel shader.
		UpdatePixelFogColorConstant( fogMode != SHADER_FOGMODE_GREY );
	}

	if ( ( fogMethod == FOGMETHOD_FIXEDFUNCTIONVERTEXFOG ) && ( fixedFunctionFogPackedColor != m_DynamicState.m_FogColor ) )
	{
		// Set the hardware fog color for the one fog method that uses it, FOGMETHOD_FIXEDFUNCTIONVERTEXFOG.
		m_DynamicState.m_FogColor = fixedFunctionFogPackedColor;
		SetSupportedRenderState( D3DRS_FOGCOLOR, m_DynamicState.m_FogColor );
	}
}

void CShaderAPIDx8::SceneFogMode( MaterialFogMode_t fogMode )
{
	LOCK_SHADERAPI();

	if( m_SceneFogMode != fogMode )
	{
		m_SceneFogMode = fogMode;

		if ( m_TransitionTable.CurrentShadowState() )
		{
			// Get the shadow state in sync since it depends on SceneFogMode.
			ApplyFogMode( m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.FogMode(), m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_bVertexFogEnable, m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_SRGBWriteEnable, m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_bDisableFogGammaCorrection );
		}
	}
}

MaterialFogMode_t CShaderAPIDx8::GetSceneFogMode()
{
	return m_SceneFogMode;
}

int CShaderAPIDx8::GetPixelFogCombo( void )
{
	Assert( 0 ); // deprecated
	return 0;
}
//-----------------------------------------------------------------------------
// Fog methods...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableFixedFunctionFog( bool bFogEnable )
{
	if ( IsGameConsole() )
	{
		// FF fog not applicable on 360 / PS3
		return;
	}
	// Set fog enable if it's different than before.
	if ( bFogEnable != m_DynamicState.m_FogEnable )
	{
		SetSupportedRenderState( D3DRS_FOGENABLE, bFogEnable );
		m_DynamicState.m_FogEnable = bFogEnable;
	}
}


void CShaderAPIDx8::FogStart( float fStart )
{
	LOCK_SHADERAPI();
	if (fStart != m_DynamicState.m_FogStart)
	{
		SetSupportedRenderState(D3DRS_FOGSTART, *((DWORD*)(&fStart)));
		m_VertexShaderFogParams[0] = fStart;
		UpdateVertexShaderFogParams();
		m_DynamicState.m_FogStart = fStart;
	}
}

void CShaderAPIDx8::FogEnd( float fEnd )
{
	LOCK_SHADERAPI();
	if (fEnd != m_DynamicState.m_FogEnd)
	{
		SetSupportedRenderState(D3DRS_FOGEND, *((DWORD*)(&fEnd)));
		m_VertexShaderFogParams[1] = fEnd;
		UpdateVertexShaderFogParams();
		m_DynamicState.m_FogEnd = fEnd;
	}
}

void CShaderAPIDx8::SetFogZ( float fogZ )
{
	LOCK_SHADERAPI();
	if (fogZ != m_DynamicState.m_FogZ)
	{
		m_DynamicState.m_FogZ = fogZ;
		UpdateVertexShaderFogParams();
	}
}


void CShaderAPIDx8::FogMaxDensity( float flMaxDensity )
{
	LOCK_SHADERAPI();
	if (flMaxDensity != m_DynamicState.m_FogMaxDensity)
	{
//		SetRenderState(D3DRS_FOGDENSITY, *((DWORD*)(&flMaxDensity)));  // ??? do I need to to this ???
		m_flFogMaxDensity = flMaxDensity;
		UpdateVertexShaderFogParams();
		m_DynamicState.m_FogMaxDensity = flMaxDensity;
	}
}

void CShaderAPIDx8::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
	LOCK_SHADERAPI();
	if( fStart )
		*fStart = m_DynamicState.m_FogStart;

	if( fEnd )
		*fEnd = m_DynamicState.m_FogEnd;

	if( fFogZ )
		*fFogZ = m_DynamicState.m_FogZ;
}

void CShaderAPIDx8::SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	LOCK_SHADERAPI();
	if( m_SceneFogColor[0] != r || m_SceneFogColor[1] != g || m_SceneFogColor[2] != b )
	{
		m_SceneFogColor[0] = r;
		m_SceneFogColor[1] = g;
		m_SceneFogColor[2] = b;
		RegenerateFogConstants();

		if ( m_TransitionTable.CurrentShadowState() )
		{
			ApplyFogMode( m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.FogMode(), m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_bVertexFogEnable, m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_SRGBWriteEnable, m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_bDisableFogGammaCorrection );
		}
	}
}

void CShaderAPIDx8::GetSceneFogColor( unsigned char *rgb )
{
	LOCK_SHADERAPI();
	rgb[0] = m_SceneFogColor[0];
	rgb[1] = m_SceneFogColor[1];
	rgb[2] = m_SceneFogColor[2];
}

void CShaderAPIDx8::GetSceneFogColor( unsigned char *r, unsigned char *g, unsigned char *b )
{
	*r = m_SceneFogColor[0];
	*g = m_SceneFogColor[1];
	*b = m_SceneFogColor[2];
}


//-----------------------------------------------------------------------------
// Gamma correction of fog color, or not...
//-----------------------------------------------------------------------------
D3DCOLOR CShaderAPIDx8::ComputeGammaCorrectedFogColor( unsigned char r, unsigned char g, unsigned char b, bool bSRGBWritesEnabled )
{
#ifdef _DEBUG
	if( g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT && !bSRGBWritesEnabled )
	{
//		Assert( 0 );
	}
#endif
	bool bLinearSpace =   g_pHardwareConfig->Caps().m_bFogColorAlwaysLinearSpace ||
						( bSRGBWritesEnabled && ( g_pHardwareConfig->Caps().m_bFogColorSpecifiedInLinearSpace || g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT ) );

	bool bScaleFogByToneMappingScale = g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER;

	float fr = ( r / 255.0f );
	float fg = ( g / 255.0f );
	float fb = ( b / 255.0f );
	if ( bLinearSpace )
	{
		fr = GammaToLinear( fr ); 
		fg = GammaToLinear( fg ); 
		fb = GammaToLinear( fb ); 
		if ( bScaleFogByToneMappingScale )
		{
			fr *= m_ToneMappingScale.x;		//
			fg *= m_ToneMappingScale.x;		// Linear
			fb *= m_ToneMappingScale.x;		//
		}
	}
	else if ( bScaleFogByToneMappingScale )
	{
		fr *= m_ToneMappingScale.w;			//
		fg *= m_ToneMappingScale.w;			// Gamma
		fb *= m_ToneMappingScale.w;			//
	}

	fr = MIN( fr, 1.0f );
	fg = MIN( fg, 1.0f );
	fb = MIN( fb, 1.0f );
	r = (int)( fr * 255 );
	g = (int)( fg * 255 );
	b = (int)( fb * 255 );
	return D3DCOLOR_ARGB( 255, r, g, b ); 
}


//-----------------------------------------------------------------------------
// Some methods chaining vertex + pixel shaders through to the shader manager
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetVertexShaderIndex( int vshIndex )
{
	ShaderManager()->SetVertexShaderIndex( vshIndex );
}

void CShaderAPIDx8::SetPixelShaderIndex( int pshIndex )
{
	ShaderManager()->SetPixelShaderIndex( pshIndex );
}

void CShaderAPIDx8::SyncToken( const char *pToken )
{
	LOCK_SHADERAPI();
	RECORD_COMMAND( DX8_SYNC_TOKEN, 1 );
	RECORD_STRING( pToken );
}


//-----------------------------------------------------------------------------
// Deals with vertex buffers
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DestroyVertexBuffers( bool bExitingLevel )
{
	LOCK_SHADERAPI();
	MeshMgr()->DestroyVertexBuffers( );

	// After a map is shut down, we switch to using smaller dynamic VBs
	// (VGUI shouldn't need much), so that we have more memory free during map loading
	m_nDynamicVBSize = ( bExitingLevel && !mat_do_not_shrink_dynamic_vb.GetBool() ) ? DYNAMIC_VERTEX_BUFFER_MEMORY_SMALL : DYNAMIC_VERTEX_BUFFER_MEMORY;
}

int CShaderAPIDx8::GetCurrentDynamicVBSize( void )
{
	return m_nDynamicVBSize;
}


#ifdef _PS3

FORCEINLINE void CShaderAPIDx8::SetVertexShaderConstantInternal( int var, float const* pVec, int numVecs, bool bForce )
{
	Dx9Device()->SetVertexShaderConstantF( var, pVec, numVecs );
}

#else


FORCEINLINE void CShaderAPIDx8::SetVertexShaderConstantInternal( int var, float const* pVec, int numVecs, bool bForce )
{
	Assert( numVecs > 0 );
	Assert( pVec );

	if ( IsPC() || IsPS3() )
	{
		Assert( var + numVecs <= g_pHardwareConfig->NumVertexShaderConstants() );

		if ( !bForce && memcmp( pVec, &m_DynamicState.m_pVectorVertexShaderConstant[var], numVecs * 4 * sizeof( float ) ) == 0 )
			return;

		Dx9Device()->SetVertexShaderConstantF( var, pVec, numVecs );
		memcpy( &m_DynamicState.m_pVectorVertexShaderConstant[var], pVec, numVecs * 4 * sizeof(float) );
	}
	else
	{
		Assert( var + numVecs <= g_pHardwareConfig->NumVertexShaderConstants() );
	}

	if ( IsX360() )
	{
		if ( !IsGPUOwnSupported() || !m_bGPUOwned )
		{
			Dx9Device()->SetVertexShaderConstantF( var, pVec, numVecs );
			memcpy( &m_DynamicState.m_pVectorVertexShaderConstant[var], pVec, numVecs * 4 * sizeof(float) );
		}
		else if ( var + numVecs > m_MaxVectorVertexShaderConstant )
		{
			m_MaxVectorVertexShaderConstant = var + numVecs;
		}
	}

	memcpy( &m_DesiredState.m_pVectorVertexShaderConstant[var], pVec, numVecs * 4 * sizeof(float) );		
}


#endif

//-----------------------------------------------------------------------------
// Sets the constant register for vertex and pixel shaders
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetVertexShaderConstant( int var, float const* pVec, int numVecs, bool bForce )
{
	SetVertexShaderConstantInternal( var, pVec, numVecs, bForce );
}

void CShaderAPIDx8::GenerateNonInstanceRenderState( MeshInstanceData_t *pInstance, CompiledLightingState_t** ppCompiledState, InstanceInfo_t **ppCompiledInfo )
{
	memset( pInstance, 0, sizeof(MeshInstanceData_t) );
	pInstance->m_pLightingState = &m_DynamicState.m_LightingState;
	if ( m_maxBoneLoaded && m_DynamicState.m_NumBones )
	{
		pInstance->m_nBoneCount = m_maxBoneLoaded + 1;
		pInstance->m_pPoseToWorld = m_boneMatrix;
		m_maxBoneLoaded = 0;
	}
	else
	{
		// casting from 4x3 matrices to a 4x4 D3DXMATRIX, need 4 floats of overflow
		float results[12+4];
		D3DXMatrixTranspose( (D3DXMATRIX *)&results[0], &GetTransform( MATERIAL_MODEL ) );
		memcpy( m_boneMatrix[0].Base(), results, 12 * sizeof(float) );
		pInstance->m_nBoneCount = 1;
		pInstance->m_pPoseToWorld = m_boneMatrix;
	}
	*ppCompiledState = &m_DynamicState.m_CompiledLightingState;
	*ppCompiledInfo = &m_DynamicState.m_InstanceInfo;
}

void CShaderAPIDx8::NotifyShaderConstantsChangedInRenderPass()
{
#if defined( _X360 )
	// send updated shader constants to gpu
	WriteShaderConstantsToGPU();
#endif
}

//-----------------------------------------------------------------------------
// Sets the boolean registers for vertex shader control flow
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetBooleanVertexShaderConstant( int var, int const* pVec, int numBools, bool bForce )
{
	Assert( pVec );
	Assert( var + numBools <= g_pHardwareConfig->NumBooleanVertexShaderConstants() );

	if ( IsPC() && !bForce && memcmp( pVec, &m_DesiredState.m_pBooleanVertexShaderConstant[var], numBools * sizeof( BOOL ) ) == 0 )
	{
		return;
	}

	if ( IsPC() || IsPS3() )
	{
		Dx9Device()->SetVertexShaderConstantB( var, pVec, numBools );
		memcpy( &m_DynamicState.m_pBooleanVertexShaderConstant[var], pVec, numBools * sizeof(BOOL) );
	}

	if ( IsX360() )
	{
		if ( !IsGPUOwnSupported() || !m_bGPUOwned )
		{
			Dx9Device()->SetVertexShaderConstantB( var, pVec, numBools );
			if ( IsGPUOwnSupported() )
			{
				memcpy( &m_DynamicState.m_pBooleanVertexShaderConstant[var], pVec, numBools * sizeof(BOOL) );
			}
		}
		else if ( var + numBools > m_MaxBooleanVertexShaderConstant )
		{
			m_MaxBooleanVertexShaderConstant = var + numBools;
			Assert( m_MaxBooleanVertexShaderConstant <= 16 );
		}
	}

	memcpy( &m_DesiredState.m_pBooleanVertexShaderConstant[var], pVec, numBools * sizeof(BOOL) );
}


//-----------------------------------------------------------------------------
// Sets the integer registers for vertex shader control flow
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetIntegerVertexShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce )
{
	Assert( pVec );
	Assert( var + numIntVecs <= g_pHardwareConfig->NumIntegerVertexShaderConstants() );

	if ( IsPC() && !bForce && memcmp( pVec, &m_DesiredState.m_pIntegerVertexShaderConstant[var], numIntVecs * sizeof( IntVector4D ) ) == 0 )
	{
		return;
	}

	if ( IsPC() || IsPS3() )
	{
		Dx9Device()->SetVertexShaderConstantI( var, pVec, numIntVecs );
		memcpy( &m_DynamicState.m_pIntegerVertexShaderConstant[var], pVec, numIntVecs * sizeof(IntVector4D) );
	}

	if ( IsX360() )
	{
		if ( !IsGPUOwnSupported() || !m_bGPUOwned )
		{
			Dx9Device()->SetVertexShaderConstantI( var, pVec, numIntVecs );
			if ( IsGPUOwnSupported() )
			{
				memcpy( &m_DynamicState.m_pIntegerVertexShaderConstant[var], pVec, numIntVecs * sizeof(IntVector4D) );
			}
		}
		else if ( var + numIntVecs > m_MaxIntegerVertexShaderConstant )
		{
			m_MaxIntegerVertexShaderConstant = var + numIntVecs;
			Assert( m_MaxIntegerVertexShaderConstant <= 16 );
		}
	}

	memcpy( &m_DesiredState.m_pIntegerVertexShaderConstant[var], pVec, numIntVecs * sizeof(IntVector4D) );
}

#ifndef _PS3
FORCEINLINE void CShaderAPIDx8::SetPixelShaderConstantInternal( int nStartConst, float const* pValues, int nNumConsts, bool bForce )
{
	Assert( nStartConst + nNumConsts <= g_pHardwareConfig->NumPixelShaderConstants() );

	if ( IsPC() || IsPS3() )
	{
		if ( !bForce )
		{
			DWORD* pSrc = (DWORD*)pValues;
			DWORD* pDst = (DWORD*)&m_DesiredState.m_pVectorPixelShaderConstant[nStartConst];
			while( nNumConsts && ( pSrc[0] == pDst[0] ) && ( pSrc[1] == pDst[1] ) && ( pSrc[2] == pDst[2] ) && ( pSrc[3] == pDst[3] ) )
			{
				pSrc += 4;
				pDst += 4;
				nNumConsts--;
				nStartConst++;
			}
			if ( !nNumConsts )
				return;
			pValues = reinterpret_cast< float const * >( pSrc );
		}
					
		Dx9Device()->SetPixelShaderConstantF( nStartConst, pValues, nNumConsts );
		memcpy( &m_DynamicState.m_pVectorPixelShaderConstant[nStartConst], pValues, nNumConsts * 4 * sizeof(float) );
	}

	if ( IsX360() )
	{
		if ( !IsGPUOwnSupported() || !m_bGPUOwned )
		{
			Dx9Device()->SetPixelShaderConstantF( nStartConst, pValues, nNumConsts );
			if ( IsGPUOwnSupported() )
			{
				memcpy( &m_DynamicState.m_pVectorPixelShaderConstant[nStartConst], pValues, nNumConsts * 4 * sizeof(float) );
			}
		}
		else if ( nStartConst + nNumConsts > m_MaxVectorPixelShaderConstant )
		{
			m_MaxVectorPixelShaderConstant = nStartConst + nNumConsts;
			Assert( m_MaxVectorPixelShaderConstant <= 32 );
			if ( m_MaxVectorPixelShaderConstant > 32 )
			{
				// NOTE!  There really are 224 pixel shader constants on the 360, but we do an optimization that only blasts the first 32 always.
				Error( "Don't use more then the first 32 pixel shader constants on the 360!" );
			}
		}
	}

	memcpy( &m_DesiredState.m_pVectorPixelShaderConstant[nStartConst], pValues, nNumConsts * 4 * sizeof(float) );
}
#endif

void CShaderAPIDx8::SetPixelShaderConstant( int var, float const* pVec, int numVecs, bool bForce )
{
	SetPixelShaderConstantInternal( var, pVec, numVecs, bForce );
}

template<class T> FORCEINLINE T GetData( uint8 const *pData )
{
	return * ( reinterpret_cast< T const *>( pData ) );
}



void CShaderAPIDx8::SetStandardTextureHandle( StandardTextureId_t nId, ShaderAPITextureHandle_t nHandle )
{
	Assert( nId < ARRAYSIZE( m_StdTextureHandles ) );
	m_StdTextureHandles[nId] = nHandle;
	
	if ( nId == TEXTURE_LOCAL_ENV_CUBEMAP )
	{
		m_DynamicState.m_nLocalEnvCubemapSamplers = 0;

		// NOTE: We could theoretically recompute localenvcubemapsamplers,
		// but this will happen outside of rendering shaders, so it won't matter
	}
	if ( nId == TEXTURE_LIGHTMAP )
	{
		m_DynamicState.m_nLightmapSamplers = 0;
	}
	if ( nId == TEXTURE_PAINT )
	{
		m_DynamicState.m_nPaintmapSamplers = 0;
	}
}

void CShaderAPIDx8::DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstances )
{
	MeshMgr()->DrawInstances( nInstanceCount, pInstances );
}


#ifdef _PS3


void CShaderAPIDx8::ExecuteCommandBuffer( uint8 *pCmdBuf )
{
	gpGcmDrawState->SetWorldSpaceCameraPosition((float*)&m_WorldSpaceCameraPosition);
	gpGcmDrawState->ExecuteCommandBuffer(pCmdBuf);
}

void CShaderAPIDx8::ExecuteCommandBufferPPU( uint8 *pCmdBuf )
{

	uint8 *pReturnStack[20];
	uint8 **pSP = &pReturnStack[ARRAYSIZE(pReturnStack)];
	uint8 *pLastCmd;
	for(;;)
	{
		uint8 *pCmd=pCmdBuf;
		int nCmd = GetData<int>( pCmdBuf );
		switch( nCmd )
		{

		case CBCMD_LENGTH:
			{
				pCmdBuf += sizeof(int) *2 ;
				break;
			}

		case CBCMD_PS3TEX:
			{
				pCmdBuf += sizeof(int) + (CBCMD_MAX_PS3TEX*sizeof(int));
				break;
			}

		case CBCMD_END:
			{
				if ( pSP == &pReturnStack[ARRAYSIZE(pReturnStack)] )
					return;
				else
				{
					// pop pc
					pCmdBuf = *( pSP ++ );
					break;
				}
			}

		case CBCMD_JUMP:
			pCmdBuf = GetData<uint8 *>(  pCmdBuf + sizeof( int ) );
			break;

		case CBCMD_JSR:
			{
				Assert( pSP > &(pReturnStack[0] ) );
				// 				*(--pSP ) = pCmdBuf + sizeof( int ) + sizeof( uint8 *);
				// 				pCmdBuf = GetData<uint8 *>(  pCmdBuf + sizeof( int ) );
				ExecuteCommandBuffer( GetData<uint8 *>(  pCmdBuf + sizeof( int ) ) );
				pCmdBuf = pCmdBuf + sizeof( int ) + sizeof( uint8 *);
				break;
			}

		case CBCMD_SET_VERTEX_SHADER_FLASHLIGHT_STATE:
			{
				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				SetVertexShaderConstantInternal( nStartConst, m_FlashlightWorldToTexture.Base(), 4, false );
				pCmdBuf += 2 * sizeof( int );
				break;
			}


		case CBCMD_SET_PIXEL_SHADER_FLASHLIGHT_STATE:
			{
				int nLightSampler		= GetData<int>( pCmdBuf + sizeof( int ) );
				int nDepthSampler		= GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				int nShadowNoiseSampler = GetData<int>( pCmdBuf + 3 * sizeof( int ) );
				int nColorConst			= GetData<int>( pCmdBuf + 4 * sizeof( int ) );
				int nAttenConst			= GetData<int>( pCmdBuf + 5 * sizeof( int ) );
				int nOriginConst		= GetData<int>( pCmdBuf + 6 * sizeof( int ) );
				int nDepthTweakConst	= GetData<int>( pCmdBuf + 7 * sizeof( int ) );
				int nScreenScaleConst	= GetData<int>( pCmdBuf + 8 * sizeof( int ) );
				int nWorldToTextureConstant = GetData<int>( pCmdBuf + 9 * sizeof( int ) );
				bool bFlashlightNoLambert = GetData<int>( pCmdBuf + 10 * sizeof( int ) ) != 0;
				bool bSinglePassFlashlight = GetData<int>( pCmdBuf + 11 * sizeof( int ) ) != 0;
				pCmdBuf += 12 * sizeof( int );

				ShaderAPITextureHandle_t hTexture = g_pShaderUtil->GetShaderAPITextureBindHandle( m_FlashlightState.m_pSpotlightTexture, m_FlashlightState.m_nSpotlightTextureFrame, 0 );
				BindTexture( (Sampler_t)nLightSampler, TEXTURE_BINDFLAGS_SRGBREAD, hTexture ); // !!!BUG!!!srgb or not?

				SetPixelShaderConstantInternal( nAttenConst, m_pFlashlightAtten, 1, false );
				SetPixelShaderConstantInternal( nOriginConst, m_pFlashlightPos, 1, false );

				m_pFlashlightColor[3] = bFlashlightNoLambert ? 2.0f : 0.0f; // This will be added to N.L before saturate to force a 1.0 N.L term

				// DX10 hardware and single pass flashlight require a hack scalar since the flashlight is added in linear space
				float flashlightColor[4] = { m_pFlashlightColor[0], m_pFlashlightColor[1], m_pFlashlightColor[2], m_pFlashlightColor[3] };
				if ( ( g_pHardwareConfig->UsesSRGBCorrectBlending() ) || ( bSinglePassFlashlight ) )
				{
					// Magic number that works well on the 360 and NVIDIA 8800
					flashlightColor[0] *= 2.5f;
					flashlightColor[1] *= 2.5f;
					flashlightColor[2] *= 2.5f;
				}

				SetPixelShaderConstantInternal( nColorConst, flashlightColor, 1, false );

				if ( nWorldToTextureConstant >= 0 )
				{
					SetPixelShaderConstantInternal( nWorldToTextureConstant, m_FlashlightWorldToTexture.Base(), 4, false );
				}

				BindStandardTexture( (Sampler_t)nShadowNoiseSampler, TEXTURE_BINDFLAGS_NONE, TEXTURE_SHADOW_NOISE_2D );
				if( m_pFlashlightDepthTexture && m_FlashlightState.m_bEnableShadows && ShaderUtil()->GetConfig().ShadowDepthTexture() )
				{
					ShaderAPITextureHandle_t hDepthTexture = g_pShaderUtil->GetShaderAPITextureBindHandle( m_pFlashlightDepthTexture, 0, 0 );
					BindTexture( (Sampler_t)nDepthSampler, TEXTURE_BINDFLAGS_SHADOWDEPTH, hDepthTexture );

					SetPixelShaderConstantInternal( nDepthTweakConst, m_pFlashlightTweaks, 1, false );

					// Dimensions of screen, used for screen-space noise map sampling
					float vScreenScale[4] = {1280.0f / 32.0f, 720.0f / 32.0f, 0, 0};
					int nWidth, nHeight;
					BaseClass::GetBackBufferDimensions( nWidth, nHeight );

					int nTexWidth, nTexHeight;
					GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SHADOW_NOISE_2D );

					vScreenScale[0] = (float) nWidth  / nTexWidth;
					vScreenScale[1] = (float) nHeight / nTexHeight;
					vScreenScale[2] = 1.0f / m_FlashlightState.m_flShadowMapResolution;
					vScreenScale[3] = 2.0f / m_FlashlightState.m_flShadowMapResolution;
					SetPixelShaderConstantInternal( nScreenScaleConst, vScreenScale, 1, false );
				}
				else
				{
					BindStandardTexture( (Sampler_t)nDepthSampler, TEXTURE_BINDFLAGS_NONE, TEXTURE_WHITE );
				}

				if ( IsX360() )
				{
					SetBooleanPixelShaderConstant( 0, &m_FlashlightState.m_nShadowQuality, 1 );
				}

				break;
			}

		case CBCMD_SET_PIXEL_SHADER_UBERLIGHT_STATE:
			{
				int iEdge0Const			= GetData<int>( pCmdBuf + sizeof( int ) );
				int iEdge1Const			= GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				int iEdgeOOWConst		= GetData<int>( pCmdBuf + 3 * sizeof( int ) );
				int iShearRoundConst	= GetData<int>( pCmdBuf + 4 * sizeof( int ) );
				int iAABBConst			= GetData<int>( pCmdBuf + 5 * sizeof( int ) );
				int iWorldToLightConst	= GetData<int>( pCmdBuf + 6 * sizeof( int ) );
				pCmdBuf += 7 * sizeof( int );

				SetPixelShaderConstantInternal( iEdge0Const, m_UberlightRenderState.m_vSmoothEdge0.Base(), 1, false );
				SetPixelShaderConstantInternal( iEdge1Const, m_UberlightRenderState.m_vSmoothEdge1.Base(), 1, false );
				SetPixelShaderConstantInternal( iEdgeOOWConst, m_UberlightRenderState.m_vSmoothOneOverW.Base(), 1, false );
				SetPixelShaderConstantInternal( iShearRoundConst, m_UberlightRenderState.m_vShearRound.Base(), 1, false );
				SetPixelShaderConstantInternal( iAABBConst, m_UberlightRenderState.m_vaAbB.Base(), 1, false );
				SetPixelShaderConstantInternal( iWorldToLightConst, m_UberlightRenderState.m_WorldToLight.Base(), 4, false );
				break;
			}

#ifndef NDEBUG
		default:
			Assert(0);
			break;
#endif
		}
		pLastCmd = pCmd;
	}
}

#else

void CShaderAPIDx8::ExecuteCommandBuffer( uint8 *pCmdBuf )
{

	uint8 *pReturnStack[20];
	uint8 **pSP = &pReturnStack[ARRAYSIZE(pReturnStack)];
	uint8 *pLastCmd;
	for(;;)
	{
		uint8 *pCmd=pCmdBuf;
		int nCmd = GetData<int>( pCmdBuf );
		switch( nCmd )
		{
		case CBCMD_END:
			{
				if ( pSP == &pReturnStack[ARRAYSIZE(pReturnStack)] )
					return;
				else
				{
					// pop pc
					pCmdBuf = *( pSP ++ );
					break;
				}
			}

		case CBCMD_JUMP:
			pCmdBuf = GetData<uint8 *>(  pCmdBuf + sizeof( int ) );
			break;

		case CBCMD_JSR:
			{
				Assert( pSP > &(pReturnStack[0] ) );
				// 				*(--pSP ) = pCmdBuf + sizeof( int ) + sizeof( uint8 *);
				// 				pCmdBuf = GetData<uint8 *>(  pCmdBuf + sizeof( int ) );
				ExecuteCommandBuffer( GetData<uint8 *>(  pCmdBuf + sizeof( int ) ) );
				pCmdBuf = pCmdBuf + sizeof( int ) + sizeof( uint8 *);
				break;
			}

		case CBCMD_SET_PIXEL_SHADER_FLOAT_CONST:
			{
				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				int nNumConsts = GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				float const *pValues = reinterpret_cast< float const *> ( pCmdBuf + 3 * sizeof( int ) );
				pCmdBuf += nNumConsts * 4 * sizeof( float ) + 3 * sizeof( int );
				SetPixelShaderConstantInternal( nStartConst, pValues, nNumConsts, false );
				break;
			}

		case CBCMD_SETPIXELSHADERFOGPARAMS:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				SetPixelShaderFogParams( nReg );			// !! speed fixme
				break;
			}

		case CBCMD_STORE_EYE_POS_IN_PSCONST:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				float flWValue = GetData<float>( pCmdBuf + 2 * sizeof( int ) );
				pCmdBuf += 2 * sizeof( int ) + sizeof( float );

				Vector4D vecValue( m_WorldSpaceCameraPosition.x, m_WorldSpaceCameraPosition.y, m_WorldSpaceCameraPosition.z, flWValue );
				SetPixelShaderConstantInternal( nReg, vecValue.Base(), 1, false );
				break;
			}

		case CBCMD_SET_DEPTH_FEATHERING_CONST:
			{
				int nConst = GetData<int>( pCmdBuf + sizeof( int ) );
				float fDepthBlendScale = GetData<float>( pCmdBuf + 2 * sizeof( int ) );
				pCmdBuf += 2 * sizeof( int ) + sizeof( float );
				SetDepthFeatheringShaderConstants( nConst, fDepthBlendScale );
				break;
			}

		case CBCMD_SET_VERTEX_SHADER_FLOAT_CONST:
			{
				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				int nNumConsts = GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				float const *pValues = reinterpret_cast< float const *> ( pCmdBuf + 3 * sizeof( int ) );
				pCmdBuf += nNumConsts * 4 * sizeof( float ) + 3 * sizeof( int );
				SetVertexShaderConstantInternal( nStartConst, pValues, nNumConsts, false );
				break;
			}

		case CBCMD_BIND_STANDARD_TEXTURE:
			{
				int nSampler = GetData<int>( pCmdBuf + sizeof( int ) );
				int nBindFlags = ( nSampler & TEXTURE_BINDFLAGS_VALID_MASK );
				nSampler &= ( MAX_SAMPLERS - 1 );
				int nTextureID = GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				pCmdBuf += 3 * sizeof( int );
				BindStandardTexture( (Sampler_t)nSampler, ( TextureBindFlags_t ) nBindFlags, (StandardTextureId_t )nTextureID );
				break;
			}

		case CBCMD_BIND_SHADERAPI_TEXTURE_HANDLE:
			{
				int nSampler = GetData<int>( pCmdBuf + sizeof( int ) );
				int nBindFlags = ( nSampler & TEXTURE_BINDFLAGS_VALID_MASK );
				nSampler &= ( MAX_SAMPLERS - 1 );
				ShaderAPITextureHandle_t hTexture = GetData<ShaderAPITextureHandle_t>( pCmdBuf + 2 * sizeof( int ) );
				Assert( hTexture != INVALID_SHADERAPI_TEXTURE_HANDLE );
				pCmdBuf += 2 * sizeof( int ) + sizeof( ShaderAPITextureHandle_t );
				BindTexture( (Sampler_t) nSampler, ( TextureBindFlags_t ) nBindFlags, hTexture );
				break;
			}

		case CBCMD_SET_PSHINDEX:
			{
				int nIdx = GetData<int>( pCmdBuf + sizeof( int ) );
				ShaderManager()->SetPixelShaderIndex( nIdx );
				pCmdBuf += 2 * sizeof( int );
				break;
			}

		case CBCMD_SET_VSHINDEX:
			{
				int nIdx = GetData<int>( pCmdBuf + sizeof( int ) );
				ShaderManager()->SetVertexShaderIndex( nIdx );
				pCmdBuf += 2 * sizeof( int );
				break;
			}

		case CBCMD_SET_VERTEX_SHADER_FLASHLIGHT_STATE:
			{
				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				SetVertexShaderConstantInternal( nStartConst, m_FlashlightWorldToTexture.Base(), 4, false );
				pCmdBuf += 2 * sizeof( int );
				break;
			}

		case CBCMD_SET_VERTEX_SHADER_NEARZFARZ_STATE:
			{
				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );

				VMatrix m;
				GetMatrix( MATERIAL_PROJECTION, m.m[0] );

				// m[2][2] =  F/(N-F)   (flip sign if RH)
				// m[3][2] = NF/(N-F)

				float vNearFar[4];

				float N =     m[3][2] / m[2][2];
				float F = (m[3][2]*N) / (N + m[3][2]);

				vNearFar[0] = N;
				vNearFar[1] = F;

				SetVertexShaderConstantInternal( nStartConst, vNearFar, 1, false );
				pCmdBuf += 2 * sizeof( int );
				break;
			}

		case CBCMD_SET_PIXEL_SHADER_FLASHLIGHT_STATE:
			{
				int nLightSampler		= GetData<int>( pCmdBuf + sizeof( int ) );
				int nDepthSampler		= GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				int nShadowNoiseSampler = GetData<int>( pCmdBuf + 3 * sizeof( int ) );
				int nColorConst			= GetData<int>( pCmdBuf + 4 * sizeof( int ) );
				int nAttenConst			= GetData<int>( pCmdBuf + 5 * sizeof( int ) );
				int nOriginConst		= GetData<int>( pCmdBuf + 6 * sizeof( int ) );
				int nDepthTweakConst	= GetData<int>( pCmdBuf + 7 * sizeof( int ) );
				int nScreenScaleConst	= GetData<int>( pCmdBuf + 8 * sizeof( int ) );
				int nWorldToTextureConstant = GetData<int>( pCmdBuf + 9 * sizeof( int ) );
				bool bFlashlightNoLambert = GetData<int>( pCmdBuf + 10 * sizeof( int ) ) != 0;
				bool bSinglePassFlashlight = GetData<int>( pCmdBuf + 11 * sizeof( int ) ) != 0;
				pCmdBuf += 12 * sizeof( int );

				ShaderAPITextureHandle_t hTexture = g_pShaderUtil->GetShaderAPITextureBindHandle( m_FlashlightState.m_pSpotlightTexture, m_FlashlightState.m_nSpotlightTextureFrame, 0 );
				BindTexture( (Sampler_t)nLightSampler, TEXTURE_BINDFLAGS_SRGBREAD, hTexture ); // !!!BUG!!!srgb or not?

				SetPixelShaderConstantInternal( nAttenConst, m_pFlashlightAtten, 1, false );
				SetPixelShaderConstantInternal( nOriginConst, m_pFlashlightPos, 1, false );

				m_pFlashlightColor[3] = bFlashlightNoLambert ? 2.0f : 0.0f; // This will be added to N.L before saturate to force a 1.0 N.L term

				// DX10 hardware and single pass flashlight require a hack scalar since the flashlight is added in linear space
				float flashlightColor[4] = { m_pFlashlightColor[0], m_pFlashlightColor[1], m_pFlashlightColor[2], m_pFlashlightColor[3] };
				if ( ( g_pHardwareConfig->UsesSRGBCorrectBlending() ) || ( bSinglePassFlashlight ) )
				{
					// Magic number that works well on the 360 and NVIDIA 8800
					flashlightColor[0] *= 2.5f;
					flashlightColor[1] *= 2.5f;
					flashlightColor[2] *= 2.5f;
				}

				SetPixelShaderConstantInternal( nColorConst, flashlightColor, 1, false );

				if ( nWorldToTextureConstant >= 0 )
				{
					SetPixelShaderConstantInternal( nWorldToTextureConstant, m_FlashlightWorldToTexture.Base(), 4, false );
				}

				BindStandardTexture( (Sampler_t)nShadowNoiseSampler, TEXTURE_BINDFLAGS_NONE, TEXTURE_SHADOW_NOISE_2D );
				if( m_pFlashlightDepthTexture && m_FlashlightState.m_bEnableShadows && ShaderUtil()->GetConfig().ShadowDepthTexture() )
				{
					ShaderAPITextureHandle_t hDepthTexture = g_pShaderUtil->GetShaderAPITextureBindHandle( m_pFlashlightDepthTexture, 0, 0 );
					BindTexture( (Sampler_t)nDepthSampler, TEXTURE_BINDFLAGS_SHADOWDEPTH, hDepthTexture );

					SetPixelShaderConstantInternal( nDepthTweakConst, m_pFlashlightTweaks, 1, false );

					// Dimensions of screen, used for screen-space noise map sampling
					float vScreenScale[4] = {1280.0f / 32.0f, 720.0f / 32.0f, 0, 0};
					int nWidth, nHeight;
					BaseClass::GetBackBufferDimensions( nWidth, nHeight );

					int nTexWidth, nTexHeight;
					GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SHADOW_NOISE_2D );

					vScreenScale[0] = (float) nWidth  / nTexWidth;
					vScreenScale[1] = (float) nHeight / nTexHeight;
					vScreenScale[2] = 1.0f / m_FlashlightState.m_flShadowMapResolution;
					vScreenScale[3] = 2.0f / m_FlashlightState.m_flShadowMapResolution;
					SetPixelShaderConstantInternal( nScreenScaleConst, vScreenScale, 1, false );
				}
				else
				{
					BindStandardTexture( (Sampler_t)nDepthSampler, TEXTURE_BINDFLAGS_NONE, TEXTURE_WHITE );
				}

				if ( IsX360() )
				{
					SetBooleanPixelShaderConstant( 0, &m_FlashlightState.m_nShadowQuality, 1 );
				}

				break;
			}

		case CBCMD_SET_PIXEL_SHADER_UBERLIGHT_STATE:
			{
				int iEdge0Const			= GetData<int>( pCmdBuf + sizeof( int ) );
				int iEdge1Const			= GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				int iEdgeOOWConst		= GetData<int>( pCmdBuf + 3 * sizeof( int ) );
				int iShearRoundConst	= GetData<int>( pCmdBuf + 4 * sizeof( int ) );
				int iAABBConst			= GetData<int>( pCmdBuf + 5 * sizeof( int ) );
				int iWorldToLightConst	= GetData<int>( pCmdBuf + 6 * sizeof( int ) );
				pCmdBuf += 7 * sizeof( int );

				SetPixelShaderConstantInternal( iEdge0Const, m_UberlightRenderState.m_vSmoothEdge0.Base(), 1, false );
				SetPixelShaderConstantInternal( iEdge1Const, m_UberlightRenderState.m_vSmoothEdge1.Base(), 1, false );
				SetPixelShaderConstantInternal( iEdgeOOWConst, m_UberlightRenderState.m_vSmoothOneOverW.Base(), 1, false );
				SetPixelShaderConstantInternal( iShearRoundConst, m_UberlightRenderState.m_vShearRound.Base(), 1, false );
				SetPixelShaderConstantInternal( iAABBConst, m_UberlightRenderState.m_vaAbB.Base(), 1, false );
				SetPixelShaderConstantInternal( iWorldToLightConst, m_UberlightRenderState.m_WorldToLight.Base(), 4, false );
				break;
			}

#ifndef NDEBUG
		default:
			Assert(0);
			break;
#endif
		}
		pLastCmd = pCmd;
	}
}

#endif



//ConVar mat_use_old_gamma( "mat_use_old_gamma", "1" );
#define USE_OLD_GAMMA ( false )

//-----------------------------------------------------------------------------
// Executes a command buffer containing per-instance data
//-----------------------------------------------------------------------------

#ifndef _PS3

void CShaderAPIDx8::ExecuteInstanceCommandBuffer( const unsigned char *pCmdBuf, int nInstanceIndex, bool bForceStateSet )
{
	if ( !pCmdBuf )
		return;

	//SNPROF( "CShaderAPIDx8::ExecuteInstanceCommandBuffer" );

	const MeshInstanceData_t &instance = m_pRenderInstances[nInstanceIndex];
	CompiledLightingState_t *pCompiledState = &m_pRenderCompiledState[nInstanceIndex];
	InstanceInfo_t *pInfo = &m_pRenderInstanceInfo[nInstanceIndex];

	// First, deal with all env_cubemaps
	if ( m_DynamicState.m_nLocalEnvCubemapSamplers )
	{
		ShaderAPITextureHandle_t hEnvCubemap;
		if ( !instance.m_pEnvCubemap )
		{
			// Should only hit this codepath during fast path brush model rendering
			hEnvCubemap = m_StdTextureHandles[TEXTURE_LOCAL_ENV_CUBEMAP];
		}
		else
		{
			// FIXME: Shitty!  But I don't see another way to convert the ITexture 
			// into a ShaderAPITextureHandle_t, and we can't expose ShaderAPITextureHandle_t
			// into MeshRenderData_t I don't think..?
			hEnvCubemap = ShaderUtil()->GetShaderAPITextureBindHandle( (ITexture*)(instance.m_pEnvCubemap), 0, 0 );
		}

		int i = SHADER_SAMPLER0;
		int nSamplerMask = m_DynamicState.m_nLocalEnvCubemapSamplers;
		for ( i = 0; nSamplerMask; ++i, nSamplerMask >>= 1 )
		{
			if ( nSamplerMask & 0x1 )
			{
				// NOTE: Don't use BindTexture here, as it will not actually do the binding
				SetTextureState( (Sampler_t)i, LastSetTextureBindFlags( i ), hEnvCubemap, true );
			}
		}
	}

	// Next, deal with the lightmap
	if ( instance.m_nLightmapPageId != MATERIAL_SYSTEM_LIGHTMAP_PAGE_INVALID )
	{
		if ( m_DynamicState.m_nLightmapSamplers )
		{
			// FIXME: Shitty!  But I don't see another way to convert the ITexture
			// into a ShaderAPITextureHandle_t, and we can't expose ShaderAPITextureHandle_t
			// into MeshRenderData_t I don't think..?
			ShaderAPITextureHandle_t hLightmap = ShaderUtil()->GetLightmapTexture( instance.m_nLightmapPageId );

			int i = SHADER_SAMPLER0;
			int nSamplerMask = m_DynamicState.m_nLightmapSamplers;
			for ( i = 0; nSamplerMask; ++i, nSamplerMask >>= 1 )
			{
				if ( nSamplerMask & 0x1 )
				{
					// NOTE: Don't use BindTexture here, as it will not actually do the binding
					SetTextureState( (Sampler_t)i, LastSetTextureBindFlags( i ), hLightmap, true );
				}
			}
		}

		if ( m_DynamicState.m_nPaintmapSamplers )
		{
			// FIXME: Shitty!  But I don't see another way to convert the ITexture
			// into a ShaderAPITextureHandle_t, and we can't expose ShaderAPITextureHandle_t
			// into MeshRenderData_t I don't think..?
			ShaderAPITextureHandle_t hPaintmap = ShaderUtil()->GetPaintmapTexture( instance.m_nLightmapPageId );

			int i = SHADER_SAMPLER0;
			int nSamplerMask = m_DynamicState.m_nPaintmapSamplers;
			for ( i = 0; nSamplerMask; ++i, nSamplerMask >>= 1 )
			{
				if ( nSamplerMask & 0x1 )
				{
					// NOTE: Don't use BindTexture here, as it will not actually do the binding
					SetTextureState( (Sampler_t)i, LastSetTextureBindFlags( i ), hPaintmap, true );
				}
			}
		}
	}

	// Next, deal with stencil state
	if ( instance.m_pStencilState )
	{
		SetStencilStateInternal( *instance.m_pStencilState );
		m_bRenderHasSetStencil = true;
	}
	else if ( m_bRenderHasSetStencil )
	{
		// If we've set the stencil state at any point, but this
		// instance has a NULL stencil state, reset to initial state
		SetStencilStateInternal( m_RenderInitialStencilState );
		m_bRenderHasSetStencil = false;
	}

	MaterialSystem_Config_t &config = ShaderUtil()->GetConfig();

	const unsigned char *pReturnStack[20];
	const unsigned char **pSP = &pReturnStack[ARRAYSIZE(pReturnStack)];
	static const unsigned char kInitialCmdString[] = "No Prev Cmd";
	const unsigned char *pLastCmd = kInitialCmdString;
	bool bConstantsChanged = false;

#ifdef _DEBUG
	uint32 nEncounteredCmd = 0;
#endif

	const unsigned char *pOrigCmd = pCmdBuf;
	for(;;)
	{
		const unsigned char *pCmd=pCmdBuf;
		int nCmd = GetData<int>( pCmdBuf );

#ifdef _DEBUG
		if ( ( nCmd > CBICMD_JSR ) && ( nEncounteredCmd & ( 1 << nCmd ) ) )
		{
			Warning( "Perf warning: Multiple identical commands (%d) in the per-instance command buffer!\n", nCmd );
		}
		nEncounteredCmd |= 1 << nCmd;
#endif
		switch( nCmd )
		{
		case CBICMD_END:
			{
				if ( pSP == &pReturnStack[ARRAYSIZE(pReturnStack)] )
				{
					if ( bConstantsChanged )
					{
						NotifyShaderConstantsChangedInRenderPass();
					}
					return;
				}

				// pop pc
				pCmdBuf = *( pSP ++ );
			}
			break;

		case CBICMD_JUMP:
			pCmdBuf = GetData<const unsigned char *>( pCmdBuf + sizeof( int ) );
			break;

		case CBICMD_JSR:
			{
				Assert( pSP > &( pReturnStack[0] ) );
				// 				*(--pSP ) = pCmdBuf + sizeof( int ) + sizeof( const unsigned char *);
				// 				pCmdBuf = GetData<const unsigned char *>(  pCmdBuf + sizeof( int ) );
				ExecuteInstanceCommandBuffer( GetData<const unsigned char *>( pCmdBuf + sizeof( int ) ), 
					nInstanceIndex, bForceStateSet );
				pCmdBuf = pCmdBuf + sizeof( int ) + sizeof( const unsigned char *);
			}
			break;

		case CBICMD_SETSKINNINGMATRICES:
			{
				pCmdBuf += sizeof( int );
				if ( bForceStateSet || !pInfo->m_bSetSkinConstants )
				{
					if ( SetSkinningMatrices( instance ) )
					{
						bConstantsChanged = true;
					}
					pInfo->m_bSetSkinConstants = true;
				}
			}
			break;

		case CBICMD_SETVERTEXSHADERLOCALLIGHTING:
			{
				pCmdBuf += sizeof( int );
				if ( instance.m_pLightingState )
				{
					if ( !pInfo->m_bVertexShaderLocalLightsCompiled )
					{
						CompileVertexShaderLocalLights( pCompiledState, instance.m_pLightingState->m_nLocalLightCount, instance.m_pLightingState, m_DynamicState.m_ShaderLightState.m_bStaticLight && (!m_DynamicState.m_ShaderLightState.m_bStaticLightIndirectOnly) );
						pInfo->m_bVertexShaderLocalLightsCompiled = true;
					}

					CommitVertexShaderLighting( pCompiledState );
					bConstantsChanged = true;
				}
			}
			break;

		case CBICMD_SETPIXELSHADERLOCALLIGHTING:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				if ( instance.m_pLightingState )
				{
					if ( !pInfo->m_bPixelShaderLocalLightsCompiled )
					{
						CompilePixelShaderLocalLights( pCompiledState, instance.m_pLightingState->m_nLocalLightCount, instance.m_pLightingState, m_DynamicState.m_ShaderLightState.m_bStaticLight && (!m_DynamicState.m_ShaderLightState.m_bStaticLightIndirectOnly) );
						pInfo->m_bPixelShaderLocalLightsCompiled = true;
					}

					CommitPixelShaderLighting( nReg, pCompiledState );
					bConstantsChanged = true;
				}
			}
			break;

		case CBICMD_SETPIXELSHADERAMBIENTLIGHTCUBE:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				if ( instance.m_pLightingState )
				{
					if ( !pInfo->m_bAmbientCubeCompiled )
					{
						CompileAmbientCube( pCompiledState, instance.m_pLightingState->m_nLocalLightCount, instance.m_pLightingState );
						pInfo->m_bAmbientCubeCompiled = true;
					}

					SetPixelShaderStateAmbientLightCube( nReg, pCompiledState );
					bConstantsChanged = true;
				}
			}
			break;

		case CBICMD_SETVERTEXSHADERAMBIENTLIGHTCUBE:
			{
				pCmdBuf += sizeof( int );
				if ( instance.m_pLightingState )
				{
					if ( !pInfo->m_bAmbientCubeCompiled )
					{
						CompileAmbientCube( pCompiledState, instance.m_pLightingState->m_nLocalLightCount, instance.m_pLightingState );
						pInfo->m_bAmbientCubeCompiled = true;
					}

					SetVertexShaderStateAmbientLightCube( VERTEX_SHADER_AMBIENT_LIGHT, pCompiledState );
					bConstantsChanged = true;
				}
			}
			break;

		case CBICMD_SETPIXELSHADERAMBIENTLIGHTCUBELUMINANCE:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				float flLuminance = GetAmbientLightCubeLuminance( instance.m_pLightingState );
				flLuminance = clamp( flLuminance, 0.0f, 1.0f );
				Vector4D psReg( flLuminance, flLuminance, flLuminance, flLuminance );
				SetPixelShaderConstantInternal( nReg, psReg.Base(), 1, false );
			}
			break;

		case CBICMD_SETPIXELSHADERGLINTDAMPING:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				float fGlintDamping = GetAmbientLightCubeLuminance( instance.m_pLightingState );

				// Get luminance of ambient cube and saturate it
				fGlintDamping = clamp( fGlintDamping, 0.0f, 1.0f );
				const float fDimGlint = 0.01f;

				// Remap so that glint damping smooth steps to zero for low luminances
				if ( fGlintDamping > fDimGlint )
					fGlintDamping = 1.0f;
				else
					fGlintDamping *= SimpleSplineRemapVal( fGlintDamping, 0.0f, fDimGlint, 0.0f, 1.0f );

				Vector4D psReg( fGlintDamping, fGlintDamping, fGlintDamping, fGlintDamping );
				SetPixelShaderConstantInternal( nReg, psReg.Base(), 1, false );
			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARCOLORSPACE:
			{
				// Skip the command.
				pCmdBuf += sizeof( int );
	
				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register
	
				if ( config.nFullbright != 2 )
				{
					if ( USE_OLD_GAMMA )
					{
						// Read the material-level gamma color scale ($color2) 
						Vector vSrcColor2 = GetData<Vector>( pCmdBuf );

						// Get the per-model-instance diffuse modulation
						const Vector4D &srcColor = instance.m_DiffuseModulation;
						
						Vector4D color;
						
						color[0] = srcColor[0] * vSrcColor2[0];
						color[1] = srcColor[1] * vSrcColor2[1];
						color[2] = srcColor[2] * vSrcColor2[2];
						color[3] = srcColor[3];
						
						color[0] = color[0] > 1.0f ? color[0] : GammaToLinear( color[0] );
						color[1] = color[1] > 1.0f ? color[1] : GammaToLinear( color[1] );
						color[2] = color[2] > 1.0f ? color[2] : GammaToLinear( color[2] );
						
						SetPixelShaderConstantInternal( nReg, color.Base() );
					}
					else
					{
						// Read the material-level gamma color scale ($color2) 
						fltx4 fl4DiffuseModulation = LoadUnalignedSIMD( &instance.m_DiffuseModulation );
						fltx4 fl4Color = GammaToLinearExtendedSIMD( MulSIMD( LoadUnalignedSIMD( pCmdBuf ),fl4DiffuseModulation ) );

						// restore alpha
						fl4Color = SetWSIMD( fl4Color, fl4DiffuseModulation );

						SetPixelShaderConstantInternal( nReg, (float const * ) &fl4Color );
					}

				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetPixelShaderConstantInternal( nReg, white.Base() );
				}
				pCmdBuf += sizeof( Vector4D ); // skip vSrcColor2 Vector
			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE:
			{
				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				// Read the material-level gamma color scale ($color2) 
				Vector vSrcColor2 = GetData<Vector>( pCmdBuf );
				pCmdBuf += sizeof( Vector ); // skip vSrcColor2 Vector

				if ( config.nFullbright != 2 )
				{
					// Get the per-model-instance diffuse modulation
					const Vector4D &srcColor = instance.m_DiffuseModulation;

					Vector4D color;

					color[0] = srcColor[0] * vSrcColor2[0];
					color[1] = srcColor[1] * vSrcColor2[1];
					color[2] = srcColor[2] * vSrcColor2[2];
					color[3] = srcColor[3];

					SetPixelShaderConstantInternal( nReg, color.Base() );
				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetPixelShaderConstantInternal( nReg, white.Base() );
				}

			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_IDENTITY:
			{
				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				Vector4D color( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
				SetPixelShaderConstantInternal( nReg, color.Base() );
			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARCOLORSPACE_LINEARSCALE:
			{
				// Skip the command.
				pCmdBuf += sizeof( int );
	
				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register
	
	
				if ( config.nFullbright != 2 )
				{
					// Read the material-level gamma color scale ($color2) 
					if ( USE_OLD_GAMMA )
					{
						Vector4D vSrcColor2 = GetData<Vector4D>( pCmdBuf );
						pCmdBuf += sizeof( Vector4D ); // skip vSrcColor2 Vector
						
						// Read the linear scale value
						float scale = GetData<float>( pCmdBuf );
						pCmdBuf += sizeof( float ); // skip the linear scale value

						// Get the per-model-instance diffuse modulation
						const Vector4D &srcColor = instance.m_DiffuseModulation;
						Vector4D color;
						
						color[0] = srcColor[0] * vSrcColor2[0];
						color[1] = srcColor[1] * vSrcColor2[1];
						color[2] = srcColor[2] * vSrcColor2[2];
						color[3] = srcColor[3];
						
						color[0] = ( color[0] > 1.0f ? color[0] : GammaToLinear( color[0] ) ) * scale;
						color[1] = ( color[1] > 1.0f ? color[1] : GammaToLinear( color[1] ) ) * scale;
						color[2] = ( color[2] > 1.0f ? color[2] : GammaToLinear( color[2] ) ) * scale;
						
						SetPixelShaderConstantInternal( nReg, color.Base() );
					}
					else
					{
						fltx4 fl4SrcColor2 = LoadUnaligned3SIMD( pCmdBuf ); 
						pCmdBuf += sizeof( Vector4D );

						float scale = GetData<float>( pCmdBuf );
						pCmdBuf += sizeof( float ); // skip the linear scale value

						fltx4 fl4Scale = ReplicateX4( scale );
						fltx4 fl4SrcColor = LoadUnalignedSIMD( &instance.m_DiffuseModulation );

						fl4SrcColor2 = MulSIMD( GammaToLinearExtendedSIMD( MulSIMD( fl4SrcColor2, fl4SrcColor ) ), fl4Scale );
						fl4SrcColor2 = SetWSIMD( fl4SrcColor2, fl4SrcColor ); // copy back the original unmodified alpha

						SetPixelShaderConstantInternal( nReg, ( float const * ) &fl4SrcColor2 );
					}
				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetPixelShaderConstantInternal( nReg, white.Base() );
					pCmdBuf += sizeof( float ) + sizeof( Vector4D );
				}

			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARSCALE:
			{
				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				// Read the material-level gamma color scale ($color2) 
				Vector vSrcColor2 = GetData<Vector>( pCmdBuf );
				pCmdBuf += sizeof( Vector ); // skip vSrcColor2 Vector

				// skip the pad
				pCmdBuf += sizeof( float );

				// Read the linear scale value
				float scale = GetData<float>( pCmdBuf );
				pCmdBuf += sizeof( float ); // skip the linear scale value

				if (config.nFullbright != 2 )
				{
					// Get the per-model-instance diffuse modulation
					const Vector4D &srcColor = instance.m_DiffuseModulation;
					Vector4D color;

					color[0] = srcColor[0] * vSrcColor2[0] * scale;
					color[1] = srcColor[1] * vSrcColor2[1] * scale;
					color[2] = srcColor[2] * vSrcColor2[2] * scale;
					color[3] = srcColor[3];

					SetPixelShaderConstantInternal( nReg, color.Base() );
				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetPixelShaderConstantInternal( nReg, white.Base() );
				}
			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARSCALE_SCALEINW:
			{
				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				// Read the material-level gamma color scale ($color2) 
				Vector vSrcColor2 = GetData<Vector>( pCmdBuf );
				pCmdBuf += sizeof( Vector ); // skip vSrcColor2 Vector

				// Read the linear scale value
				float scale = GetData<float>( pCmdBuf );
				pCmdBuf += sizeof( float ); // skip the linear scale value

				if ( config.nFullbright != 2 )
				{
					// Get the per-model-instance diffuse modulation
					const Vector4D &srcColor = instance.m_DiffuseModulation;
					Vector4D color;

					color[0] = srcColor[0] * vSrcColor2[0] * scale;
					color[1] = srcColor[1] * vSrcColor2[1] * scale;
					color[2] = srcColor[2] * vSrcColor2[2] * scale;
					color[3] = scale;

					SetPixelShaderConstantInternal( nReg, color.Base() );
				}
				else
				{
					Vector4D vecScale( scale, scale, scale, scale );
					SetPixelShaderConstantInternal( nReg, vecScale.Base() );
				}
			}
			break;

		case CBICMD_SETMODULATIONVERTEXSHADERDYNAMICSTATE:
			{
				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				// Read the material-level gamma color scale ($color2) 
				Vector vSrcColor2 = GetData<Vector>( pCmdBuf );
				pCmdBuf += sizeof( Vector ); // skip vSrcColor2 Vector

				if ( config.nFullbright != 2 )
				{
					// Get the per-model-instance diffuse modulation
					const Vector4D &srcColor = instance.m_DiffuseModulation;

					Vector4D color;

					color[0] = srcColor[0] * vSrcColor2[0];
					color[1] = srcColor[1] * vSrcColor2[1];
					color[2] = srcColor[2] * vSrcColor2[2];
					color[3] = srcColor[3];

					SetVertexShaderConstantInternal( nReg, color.Base() );
				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetVertexShaderConstantInternal( nReg, white.Base() );
				}
			}
			break;

		case CBICMD_SETMODULATIONVERTEXSHADERDYNAMICSTATE_LINEARSCALE:
			{
				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				// Read the material-level gamma color scale ($color2) 
				Vector vSrcColor2 = GetData<Vector>( pCmdBuf );
				pCmdBuf += sizeof( Vector ); // skip vSrcColor2 Vector

				float flScale = GetData<float>( pCmdBuf );
				pCmdBuf += sizeof( float );

				if ( config.nFullbright != 2 )
				{
					// Get the per-model-instance diffuse modulation
					const Vector4D &srcColor = instance.m_DiffuseModulation;

					Vector4D color;

					color[0] = srcColor[0] * vSrcColor2[0] * flScale;
					color[1] = srcColor[1] * vSrcColor2[1] * flScale;
					color[2] = srcColor[2] * vSrcColor2[2] * flScale;
					color[3] = srcColor[3];

					SetVertexShaderConstantInternal( nReg, color.Base() );
				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetVertexShaderConstantInternal( nReg, white.Base() );
				}
			}
			break;

#ifndef NDEBUG
		default:
			Warning( " unknown instance command %d last = %d\n", nCmd, GetData<int>( pLastCmd ) );
			DebuggerBreak();
			break;
#endif
		}
		pLastCmd = pCmd;
	}
}

#endif


//-----------------------------------------------------------------------------
// Sets the boolean registers for pixel shader control flow
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetBooleanPixelShaderConstant( int var, int const* pVec, int numBools, bool bForce )
{
	Assert( pVec );
	Assert( var + numBools <= g_pHardwareConfig->NumBooleanPixelShaderConstants() );

	if ( IsPC() && !bForce && memcmp( pVec, &m_DesiredState.m_pBooleanPixelShaderConstant[var], numBools * sizeof( BOOL ) ) == 0 )
	{
		return;
	}

	if ( IsPC() || IsPS3() )
	{
		Dx9Device()->SetPixelShaderConstantB( var, pVec, numBools );
		memcpy( &m_DynamicState.m_pBooleanPixelShaderConstant[var], pVec, numBools * sizeof(BOOL) );
	}

	if ( IsX360() )
	{
		if ( !IsGPUOwnSupported() || !m_bGPUOwned )
		{
			Dx9Device()->SetPixelShaderConstantB( var, pVec, numBools );
			memcpy( &m_DynamicState.m_pBooleanPixelShaderConstant[var], pVec, numBools * sizeof(BOOL) );
		}
		else if ( var + numBools > m_MaxBooleanPixelShaderConstant )
		{
			m_MaxBooleanPixelShaderConstant = var + numBools;
			Assert( m_MaxBooleanPixelShaderConstant <= 16 );
		}
	}

	memcpy( &m_DesiredState.m_pBooleanPixelShaderConstant[var], pVec, numBools * sizeof(BOOL) );
}


//-----------------------------------------------------------------------------
// Sets the integer registers for pixel shader control flow
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce )
{
	Assert( pVec );
	Assert( var + numIntVecs <= g_pHardwareConfig->NumIntegerPixelShaderConstants() );

	if ( IsPC() && !bForce && memcmp( pVec, &m_DesiredState.m_pIntegerPixelShaderConstant[var], numIntVecs * sizeof( IntVector4D ) ) == 0 )
	{
		return;
	}

	if ( IsPC() || IsPS3() )
	{
		Dx9Device()->SetPixelShaderConstantI( var, pVec, numIntVecs );
		memcpy( &m_DynamicState.m_pIntegerPixelShaderConstant[var], pVec, numIntVecs * sizeof(IntVector4D) );
	}

	if ( IsX360() )
	{
		if ( !IsGPUOwnSupported() || !m_bGPUOwned )
		{
			Dx9Device()->SetPixelShaderConstantI( var, pVec, numIntVecs );
			memcpy( &m_DynamicState.m_pIntegerPixelShaderConstant[var], pVec, numIntVecs * sizeof(IntVector4D) );
		}
		else if ( var + numIntVecs > m_MaxIntegerPixelShaderConstant )
		{
			m_MaxIntegerPixelShaderConstant = var + numIntVecs;
			Assert( m_MaxBooleanPixelShaderConstant <= 16 );
		}
	}

	memcpy( &m_DesiredState.m_pIntegerPixelShaderConstant[var], pVec, numIntVecs * sizeof(IntVector4D) );
}


void CShaderAPIDx8::InvalidateDelayedShaderConstants( void )
{
	m_DelayedShaderConstants.Invalidate();
}

//-----------------------------------------------------------------------------
//
// Methods dealing with texture stage state
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Gets the texture associated with a texture state...
//-----------------------------------------------------------------------------

inline IDirect3DBaseTexture* CShaderAPIDx8::GetD3DTexture( ShaderAPITextureHandle_t hTexture )
{
	if ( hTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
	{
		return NULL;
	}

	AssertValidTextureHandle( hTexture );

	Texture_t& tex = GetTexture( hTexture );
	if ( tex.m_NumCopies == 1 )
	{
		return tex.GetTexture();
	}
	else
	{
		return tex.GetTexture( tex.m_CurrentCopy );
	}
}

bool CShaderAPIDx8::IsStandardTextureHandleValid( StandardTextureId_t textureId )
{
	ShaderAPITextureHandle_t hTex = GetStandardTextureHandle( textureId );
	if ( hTex == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return false;

	if ( !GetD3DTexture( hTex ) )
		return false;

	return true;
}

inline void* CShaderAPIDx8::GetD3DTexturePtr( ShaderAPITextureHandle_t hTexture )
{
	return (void *)GetD3DTexture( hTexture );	
}

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline ShaderAPITextureHandle_t CShaderAPIDx8::GetModifyTextureHandle() const
{
	return m_ModifyTextureHandle;
}

inline IDirect3DBaseTexture* CShaderAPIDx8::GetModifyTexture()
{
	return CShaderAPIDx8::GetD3DTexture( m_ModifyTextureHandle );
}

void CShaderAPIDx8::SetModifyTexture( IDirect3DBaseTexture* pTex )
{
	if ( m_ModifyTextureHandle == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;
	
	Texture_t& tex = GetTexture( m_ModifyTextureHandle );
	if ( tex.m_NumCopies == 1 )
	{
		tex.SetTexture( pTex );
	}
	else
	{
		tex.SetTexture( tex.m_CurrentCopy, pTex );
	}
}

inline ShaderAPITextureHandle_t CShaderAPIDx8::GetBoundTextureBindId( Sampler_t sampler ) const
{
	return SamplerState( sampler ).m_BoundTexture;
}

inline bool CShaderAPIDx8::WouldBeOverTextureLimit( ShaderAPITextureHandle_t hTexture )
{
	if ( IsPC() )
	{
		if ( mat_texture_limit.GetInt() < 0 )
			return false;

		Texture_t &tex = GetTexture( hTexture );
		if ( tex.m_LastBoundFrame == m_CurrentFrame )
			return false;

		return m_nTextureMemoryUsedLastFrame + tex.GetMemUsage() > (mat_texture_limit.GetInt() * 1024);
	}
	return false;
}


//-----------------------------------------------------------------------------
// Moves to head of LRU. Allocates and queues for loading if evicted. Returns
// true if texture is resident, false otherwise.
//-----------------------------------------------------------------------------
FORCEINLINE void CShaderAPIDx8::TouchTexture( Sampler_t sampler, IDirect3DBaseTexture *pD3DTexture )
{
#ifdef _X360
	bool bValid = true;
	if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		CXboxTexture *pXboxTexture = (CXboxTexture *)pD3DTexture;
		if ( pXboxTexture->m_nFrameCount != m_CurrentFrame )
		{
			pXboxTexture->m_nFrameCount = m_CurrentFrame;
			bValid = g_TextureHeap.TouchTexture( pXboxTexture );
		}
		else
			bValid = ( pXboxTexture->m_BaseValid != 0 );
	}
	if ( bValid )
	{
		// base is valid for rendering
		Dx9Device()->SetSamplerState( sampler, D3DSAMP_MAXMIPLEVEL, 0 );
	}
	else
	{
		// base is invalid for rendering
		Dx9Device()->SetSamplerState( sampler, D3DSAMP_MAXMIPLEVEL, 1 );
	}
#endif
}

#ifndef _PS3

#define SETSAMPLESTATEANDMIRROR( sampler, samplerState, state_type, mirror_field, value )	\
	if ( samplerState.mirror_field != value )												\
	{																						\
		samplerState.mirror_field = value;													\
		SetSamplerState( sampler, state_type, samplerState.mirror_field );					\
	}

#define SETSAMPLEADRESSSTATEANDMIRROR( sampler, samplerState, state_type, mirror_field, value )	\
	if ( samplerState.mirror_field != D3DTEXTUREADDRESS(value) )												\
{																						\
	samplerState.mirror_field = D3DTEXTUREADDRESS(value);													\
	SetSamplerState( sampler, state_type, samplerState.mirror_field );					\
}

#else

// No Mirroring on SPU, the redundant set sampler states are on SPU

#define SETSAMPLESTATEANDMIRROR( sampler, samplerState, state_type, mirror_field, value )	\
	SetSamplerState( sampler, state_type, value );			

#define SETSAMPLEADRESSSTATEANDMIRROR( sampler, samplerState, state_type, mirror_field, value )	\
	SetSamplerState( sampler, state_type, D3DTEXTUREADDRESS(value) );

#endif 

#ifdef _X360
const DWORD g_MapLinearToSrgbGpuFormat[] = 
{
	GPUTEXTUREFORMAT_1_REVERSE,
	GPUTEXTUREFORMAT_1,
	GPUTEXTUREFORMAT_8,
	GPUTEXTUREFORMAT_1_5_5_5,
	GPUTEXTUREFORMAT_5_6_5,
	GPUTEXTUREFORMAT_6_5_5,
	GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16,
	GPUTEXTUREFORMAT_2_10_10_10_AS_16_16_16_16,
	GPUTEXTUREFORMAT_8_A,
	GPUTEXTUREFORMAT_8_B,
	GPUTEXTUREFORMAT_8_8,
	GPUTEXTUREFORMAT_Cr_Y1_Cb_Y0_REP,     
	GPUTEXTUREFORMAT_Y1_Cr_Y0_Cb_REP,      
	GPUTEXTUREFORMAT_16_16_EDRAM,          
	GPUTEXTUREFORMAT_8_8_8_8_A,
	GPUTEXTUREFORMAT_4_4_4_4,
	GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16,
	GPUTEXTUREFORMAT_11_11_10_AS_16_16_16_16,
	GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16,
	GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16,  
	GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16,
	GPUTEXTUREFORMAT_16_16_16_16_EDRAM,
	GPUTEXTUREFORMAT_24_8,
	GPUTEXTUREFORMAT_24_8_FLOAT,
	GPUTEXTUREFORMAT_16,
	GPUTEXTUREFORMAT_16_16,
	GPUTEXTUREFORMAT_16_16_16_16,
	GPUTEXTUREFORMAT_16_EXPAND,
	GPUTEXTUREFORMAT_16_16_EXPAND,
	GPUTEXTUREFORMAT_16_16_16_16_EXPAND,
	GPUTEXTUREFORMAT_16_FLOAT,
	GPUTEXTUREFORMAT_16_16_FLOAT,
	GPUTEXTUREFORMAT_16_16_16_16_FLOAT,
	GPUTEXTUREFORMAT_32,
	GPUTEXTUREFORMAT_32_32,
	GPUTEXTUREFORMAT_32_32_32_32,
	GPUTEXTUREFORMAT_32_FLOAT,
	GPUTEXTUREFORMAT_32_32_FLOAT,
	GPUTEXTUREFORMAT_32_32_32_32_FLOAT,
	GPUTEXTUREFORMAT_32_AS_8,
	GPUTEXTUREFORMAT_32_AS_8_8,
	GPUTEXTUREFORMAT_16_MPEG,
	GPUTEXTUREFORMAT_16_16_MPEG,
	GPUTEXTUREFORMAT_8_INTERLACED,
	GPUTEXTUREFORMAT_32_AS_8_INTERLACED,
	GPUTEXTUREFORMAT_32_AS_8_8_INTERLACED,
	GPUTEXTUREFORMAT_16_INTERLACED,
	GPUTEXTUREFORMAT_16_MPEG_INTERLACED,
	GPUTEXTUREFORMAT_16_16_MPEG_INTERLACED,
	GPUTEXTUREFORMAT_DXN,
	GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16,
	GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16,
	GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16,
	GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16,
	GPUTEXTUREFORMAT_2_10_10_10_AS_16_16_16_16,
	GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16,
	GPUTEXTUREFORMAT_11_11_10_AS_16_16_16_16,
	GPUTEXTUREFORMAT_32_32_32_FLOAT,
	GPUTEXTUREFORMAT_DXT3A,
	GPUTEXTUREFORMAT_DXT5A,
	GPUTEXTUREFORMAT_CTX1,
	GPUTEXTUREFORMAT_DXT3A_AS_1_1_1_1,
	GPUTEXTUREFORMAT_8_8_8_8_GAMMA_EDRAM,
	GPUTEXTUREFORMAT_2_10_10_10_FLOAT_EDRAM,
};

DWORD GetAs16SRGBFormatGPU( D3DFORMAT fmtBase )
{
	return g_MapLinearToSrgbGpuFormat[ (fmtBase & D3DFORMAT_TEXTUREFORMAT_MASK) >> D3DFORMAT_TEXTUREFORMAT_SHIFT ];
}

void ConvertTextureToAs16SRGBFormat( D3DTexture *pTexture )
{
	// First thing, mark the texture SignX, SignY and SignZ as sRGB.
	pTexture->Format.SignX = GPUSIGN_GAMMA;
	pTexture->Format.SignY = GPUSIGN_GAMMA;
	pTexture->Format.SignZ = GPUSIGN_GAMMA;

	// Get the texture format...
	XGTEXTURE_DESC desc;
	XGGetTextureDesc( pTexture, 0, &desc );

	// ...and convert it to a "good" format (AS_16_16_16_16).
	pTexture->Format.DataFormat = GetAs16SRGBFormatGPU( desc.Format );
}

ConVar r_use16bit_srgb_sampling( "r_use16bit_srgb_sampling", "1", FCVAR_CHEAT );
#endif


//--------------------------------------------------------------------------------------------------
// PS3 Texture Setting
//--------------------------------------------------------------------------------------------------

ShaderAPITextureHandle_t CShaderAPIDx8::GetStandardTextureHandle(StandardTextureId_t id)
{
	ShaderAPITextureHandle_t hTexture = INVALID_SHADERAPI_TEXTURE_HANDLE;

	if ( m_StdTextureHandles[id] != INVALID_SHADERAPI_TEXTURE_HANDLE )
	{
		hTexture = m_StdTextureHandles[id];
	}
	else
	{
		hTexture = ShaderUtil()->GetStandardTexture( id );
	}

	return hTexture;
}

#ifdef _PS3


void CShaderAPIDx8::GetPs3Texture(void* pPs3tex, ShaderAPITextureHandle_t hTexture )
{
	CPs3BindTexture_t& ps3tex = *(CPs3BindTexture_t*)pPs3tex;

	if (hTexture != INVALID_SHADERAPI_TEXTURE_HANDLE)
	{
		IDirect3DBaseTexture *pTexture = CShaderAPIDx8::GetD3DTexture( hTexture );
		Texture_t &tex = GetTexture( hTexture );

		ps3tex.m_UWrap	   = tex.m_UTexWrap;
		ps3tex.m_VWrap	   = tex.m_VTexWrap;
		ps3tex.m_WWrap	   = tex.m_WTexWrap;
		ps3tex.m_minFilter = tex.m_MinFilter;
		ps3tex.m_magFilter = tex.m_MagFilter;
		ps3tex.m_mipFilter = tex.m_MipFilter;

		ps3tex.m_nLayout = (uint32)pTexture->m_tex->m_layout;
		ps3tex.m_pLmBlock = &pTexture->m_tex->m_lmBlock;
	}
	else
	{
		ps3tex.m_nLayout = 0;
	}

}

void CShaderAPIDx8::GetPs3Texture(void* pPs3tex, StandardTextureId_t nTextureId  )
{
	CPs3BindTexture_t& ps3tex = *(CPs3BindTexture_t*)pPs3tex;

	ShaderAPITextureHandle_t hTexture = GetStandardTextureHandle(nTextureId);

	if (hTexture != INVALID_SHADERAPI_TEXTURE_HANDLE)
	{
		IDirect3DBaseTexture *pTexture = CShaderAPIDx8::GetD3DTexture( hTexture );
		Texture_t &tex = GetTexture( hTexture );

		ps3tex.m_UWrap	   = tex.m_UTexWrap;
		ps3tex.m_VWrap	   = tex.m_VTexWrap;
		ps3tex.m_WWrap	   = tex.m_WTexWrap;
		ps3tex.m_minFilter = tex.m_MinFilter;
		ps3tex.m_magFilter = tex.m_MagFilter;
		ps3tex.m_mipFilter = tex.m_MipFilter;

		ps3tex.m_nLayout = (uint32)pTexture->m_tex->m_layout;
		ps3tex.m_pLmBlock = &pTexture->m_tex->m_lmBlock;
	}
	else
	{
		ps3tex.m_nLayout = 0;
	}

}

void CShaderAPIDx8::SetTextureState( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t hTexture, bool force )
{
	// Get the dynamic texture info
	SamplerState_t &samplerState = SamplerState( sampler );

// 	// Set the texture state, but only if it changes
// 	if ( ( samplerState.m_BoundTexture == hTexture ) && ( LastSetTextureBindFlags( sampler ) == nBindFlags ))
// 		return;
// 
	// Disabling texturing
	if ( hTexture == INVALID_SHADERAPI_TEXTURE_HANDLE)
	{
		Dx9Device()->SetTexture( sampler, 0 );
		return;
	}

	samplerState.m_BoundTexture = hTexture;

	IDirect3DBaseTexture *pTexture = CShaderAPIDx8::GetD3DTexture( hTexture );

	SetSamplerState( sampler, D3DSAMP_SRGBTEXTURE, ( nBindFlags & TEXTURE_BINDFLAGS_SRGBREAD ) != 0 );

	samplerState.m_nTextureBindFlags = nBindFlags;

	Dx9Device()->SetTexture( sampler, pTexture );

	Texture_t &tex = GetTexture( hTexture );
			
	SETSAMPLEADRESSSTATEANDMIRROR( sampler, samplerState, D3DSAMP_ADDRESSU, m_UTexWrap, tex.m_UTexWrap );
	SETSAMPLEADRESSSTATEANDMIRROR( sampler, samplerState, D3DSAMP_ADDRESSV, m_VTexWrap, tex.m_VTexWrap );
	SETSAMPLEADRESSSTATEANDMIRROR( sampler, samplerState, D3DSAMP_ADDRESSW, m_WTexWrap, tex.m_WTexWrap );
	

	const uint nNewShadowFilterState = ( nBindFlags & TEXTURE_BINDFLAGS_SHADOWDEPTH ) ? 1 : 0;
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_SHADOWFILTER, m_bShadowFilterEnable, nNewShadowFilterState );
		
	D3DTEXTUREFILTERTYPE minFilter = D3DTEXTUREFILTERTYPE(tex.m_MinFilter);
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MINFILTER, m_MinFilter, minFilter );

	D3DTEXTUREFILTERTYPE magFilter = D3DTEXTUREFILTERTYPE(tex.m_MagFilter);
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MAGFILTER, m_MagFilter, magFilter );
	
	D3DTEXTUREFILTERTYPE mipFilter = D3DTEXTUREFILTERTYPE(tex.m_MipFilter);
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MIPFILTER, m_MipFilter, mipFilter );

}

void CShaderAPIDx8::SetTextureFilterMode( Sampler_t sampler, TextureFilterMode_t nMode )
{
	SamplerState_t &samplerState = SamplerState( sampler );
	D3DTEXTUREFILTERTYPE minFilter = samplerState.m_MinFilter;
	D3DTEXTUREFILTERTYPE magFilter = samplerState.m_MagFilter;
	D3DTEXTUREFILTERTYPE mipFilter = samplerState.m_MipFilter;


	switch( nMode )
	{
	case TFILTER_MODE_POINTSAMPLED:
		minFilter = D3DTEXF_POINT;
		magFilter = D3DTEXF_POINT;
		mipFilter = D3DTEXF_POINT;
		break;
	}
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MINFILTER, m_MinFilter, minFilter );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MAGFILTER, m_MagFilter, magFilter );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MIPFILTER, m_MipFilter, mipFilter );

}


void CShaderAPIDx8::BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle )
{
	//SNPROF("CShaderAPIDx8::BindTexture <><><><>");

	LOCK_SHADERAPI();

	SetTextureState( sampler, nBindFlags, textureHandle );
}

void CShaderAPIDx8::BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id )
{
	ShaderAPITextureHandle_t hTexture = GetStandardTextureHandle(id);
	
	BindTexture( sampler, nBindFlags, hTexture );

	if ((id == TEXTURE_LIGHTMAP_BUMPED) ||(id == TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT))
	{
		BindTexture( (Sampler_t)((int)sampler+1), nBindFlags, hTexture );
		BindTexture( (Sampler_t)((int)sampler+2), nBindFlags, hTexture );
	}

	Assert( LastSetTextureBindFlags( sampler ) == nBindFlags );
}


void CShaderAPIDx8::ExecuteInstanceCommandBuffer( const unsigned char *pCmdBuf, int nInstanceIndex, bool bForceStateSet )
{
	if ( !pCmdBuf )
		return;

	//SNPROF( "CShaderAPIDx8::ExecuteInstanceCommandBuffer" );

	const MeshInstanceData_t &instance = m_pRenderInstances[nInstanceIndex];
	CompiledLightingState_t *pCompiledState = &m_pRenderCompiledState[nInstanceIndex];
	InstanceInfo_t *pInfo = &m_pRenderInstanceInfo[nInstanceIndex];

	{
		//
		// Texture replacements for instances
		//

		if (IsRenderingInstances())
		{
			CPs3BindTexture_t			tex;
			ShaderAPITextureHandle_t	hTexture;

			// Env maps

			if (instance.m_pEnvCubemap)
			{
				hTexture = ShaderUtil()->GetShaderAPITextureBindHandle( (ITexture*)(instance.m_pEnvCubemap), 0, 0 );
				GetPs3Texture( (void*)&tex,hTexture );
				gpGcmDrawState->TextureReplace(TEXTURE_LOCAL_ENV_CUBEMAP, tex);
			}

			// Lightmaps/paint

			if (instance.m_nLightmapPageId != MATERIAL_SYSTEM_LIGHTMAP_PAGE_INVALID)
			{
				hTexture = ShaderUtil()->GetLightmapTexture( instance.m_nLightmapPageId );
				GetPs3Texture( (void*)&tex,hTexture );
				gpGcmDrawState->TextureReplace(TEXTURE_LIGHTMAP, tex);

				hTexture = ShaderUtil()->GetPaintmapTexture( instance.m_nLightmapPageId );
				GetPs3Texture( (void*)&tex,hTexture );
				gpGcmDrawState->TextureReplace(TEXTURE_PAINT, tex);
			}
		}
	}

	// Next, deal with stencil state
	if ( instance.m_pStencilState )
	{
		SetStencilStateInternal( *instance.m_pStencilState );
		m_bRenderHasSetStencil = true;
	}
	else if ( m_bRenderHasSetStencil )
	{
		// If we've set the stencil state at any point, but this
		// instance has a NULL stencil state, reset to initial state
		SetStencilStateInternal( m_RenderInitialStencilState );
		m_bRenderHasSetStencil = false;
	}

	MaterialSystem_Config_t &config = ShaderUtil()->GetConfig();

	const unsigned char *pReturnStack[20];
	const unsigned char **pSP = &pReturnStack[ARRAYSIZE(pReturnStack)];
	const unsigned char *pLastCmd;
	bool bConstantsChanged = false;

#ifdef _DEBUG
	uint32 nEncounteredCmd = 0;
#endif

	const unsigned char *pOrigCmd = pCmdBuf;



	for(;;)
	{

		const unsigned char *pCmd=pCmdBuf;
		int nCmd = GetData<int>( pCmdBuf );

#ifdef _DEBUG
		if ( ( nCmd > CBICMD_JSR ) && ( nEncounteredCmd & ( 1 << nCmd ) ) )
		{
			Warning( "Perf warning: Multiple identical commands (%d) in the per-instance command buffer!\n", nCmd );
		}
		nEncounteredCmd |= 1 << nCmd;
#endif
		switch( nCmd )
		{
		case CBICMD_END:
			{
				if ( pSP == &pReturnStack[ARRAYSIZE(pReturnStack)] )
				{
					if ( bConstantsChanged )
					{
						NotifyShaderConstantsChangedInRenderPass();
					}
					return;
				}

				// pop pc
				pCmdBuf = *( pSP ++ );
			}
			break;

		case CBICMD_JUMP:
			pCmdBuf = GetData<const unsigned char *>( pCmdBuf + sizeof( int ) );
			break;

		case CBICMD_JSR:
			{
				Assert( pSP > &( pReturnStack[0] ) );
				// 				*(--pSP ) = pCmdBuf + sizeof( int ) + sizeof( const unsigned char *);
				// 				pCmdBuf = GetData<const unsigned char *>(  pCmdBuf + sizeof( int ) );
				ExecuteInstanceCommandBuffer( GetData<const unsigned char *>( pCmdBuf + sizeof( int ) ), 
					nInstanceIndex, bForceStateSet );
				pCmdBuf = pCmdBuf + sizeof( int ) + sizeof( const unsigned char *);
			}
			break;

		case CBICMD_SETSKINNINGMATRICES:
			{

				pCmdBuf += sizeof( int );
				if ( bForceStateSet || !pInfo->m_bSetSkinConstants )
				{
					if ( SetSkinningMatrices( instance ) )
					{
						bConstantsChanged = true;
					}
					pInfo->m_bSetSkinConstants = true;
				}
			}
			break;

		case CBICMD_SETVERTEXSHADERLOCALLIGHTING:
			{

				pCmdBuf += sizeof( int );
				if ( instance.m_pLightingState )
				{
					if ( !pInfo->m_bVertexShaderLocalLightsCompiled )
					{
						CompileVertexShaderLocalLights( pCompiledState, instance.m_pLightingState->m_nLocalLightCount, instance.m_pLightingState );
						pInfo->m_bVertexShaderLocalLightsCompiled = true;
					}

					CommitVertexShaderLighting( pCompiledState );
					bConstantsChanged = true;
				}
			}
			break;

		case CBICMD_SETPIXELSHADERLOCALLIGHTING:
			{


				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				if ( instance.m_pLightingState )
				{
					if ( !pInfo->m_bPixelShaderLocalLightsCompiled )
					{
						CompilePixelShaderLocalLights( pCompiledState, instance.m_pLightingState->m_nLocalLightCount, instance.m_pLightingState );
						pInfo->m_bPixelShaderLocalLightsCompiled = true;
					}

					CommitPixelShaderLighting( nReg, pCompiledState );
					bConstantsChanged = true;
				}
			}
			break;

		case CBICMD_SETPIXELSHADERAMBIENTLIGHTCUBE:
			{

				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				if ( instance.m_pLightingState )
				{
					if ( !pInfo->m_bAmbientCubeCompiled )
					{
						CompileAmbientCube( pCompiledState, instance.m_pLightingState->m_nLocalLightCount, instance.m_pLightingState );
						pInfo->m_bAmbientCubeCompiled = true;
					}

					SetPixelShaderStateAmbientLightCube( nReg, pCompiledState );
					bConstantsChanged = true;
				}
			}
			break;

		case CBICMD_SETVERTEXSHADERAMBIENTLIGHTCUBE:
			{

				pCmdBuf += sizeof( int );
				if ( instance.m_pLightingState )
				{
					if ( !pInfo->m_bAmbientCubeCompiled )
					{
						CompileAmbientCube( pCompiledState, instance.m_pLightingState->m_nLocalLightCount, instance.m_pLightingState );
						pInfo->m_bAmbientCubeCompiled = true;
					}

					SetVertexShaderStateAmbientLightCube( VERTEX_SHADER_AMBIENT_LIGHT, pCompiledState );
					bConstantsChanged = true;
				}
			}
			break;

		case CBICMD_SETPIXELSHADERAMBIENTLIGHTCUBELUMINANCE:
			{


				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				float flLuminance = GetAmbientLightCubeLuminance( instance.m_pLightingState );
				flLuminance = clamp( flLuminance, 0.0f, 1.0f );
				Vector4D psReg( flLuminance, flLuminance, flLuminance, flLuminance );
				SetPixelShaderConstantInternal( nReg, psReg.Base(), 1, false );
			}
			break;

		case CBICMD_SETPIXELSHADERGLINTDAMPING:
			{

				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				float fGlintDamping = GetAmbientLightCubeLuminance( instance.m_pLightingState );

				// Get luminance of ambient cube and saturate it
				fGlintDamping = clamp( fGlintDamping, 0.0f, 1.0f );
				const float fDimGlint = 0.01f;

				// Remap so that glint damping smooth steps to zero for low luminances
				if ( fGlintDamping > fDimGlint )
					fGlintDamping = 1.0f;
				else
					fGlintDamping *= SimpleSplineRemapVal( fGlintDamping, 0.0f, fDimGlint, 0.0f, 1.0f );

				Vector4D psReg( fGlintDamping, fGlintDamping, fGlintDamping, fGlintDamping );
				SetPixelShaderConstantInternal( nReg, psReg.Base(), 1, false );
			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARCOLORSPACE:
			{

				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				if ( config.nFullbright != 2 )
				{
					if ( USE_OLD_GAMMA )
					{
						// Read the material-level gamma color scale ($color2) 
						Vector vSrcColor2 = GetData<Vector>( pCmdBuf );

						// Get the per-model-instance diffuse modulation
						const Vector4D &srcColor = instance.m_DiffuseModulation;

						Vector4D color;

						color[0] = srcColor[0] * vSrcColor2[0];
						color[1] = srcColor[1] * vSrcColor2[1];
						color[2] = srcColor[2] * vSrcColor2[2];
						color[3] = srcColor[3];

						color[0] = color[0] > 1.0f ? color[0] : GammaToLinear( color[0] );
						color[1] = color[1] > 1.0f ? color[1] : GammaToLinear( color[1] );
						color[2] = color[2] > 1.0f ? color[2] : GammaToLinear( color[2] );

						SetPixelShaderConstantInternal( nReg, color.Base() );
					}
					else
					{
						// Read the material-level gamma color scale ($color2) 
						fltx4 fl4DiffuseModulation = LoadUnalignedSIMD( &instance.m_DiffuseModulation );
						fltx4 fl4Color = GammaToLinearExtendedSIMD( MulSIMD( LoadUnalignedSIMD( pCmdBuf ),fl4DiffuseModulation ) );

						// restore alpha
						fl4Color = SetWSIMD( fl4Color, fl4DiffuseModulation );

						SetPixelShaderConstantInternal( nReg, (float const * ) &fl4Color );
					}

				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetPixelShaderConstantInternal( nReg, white.Base() );
				}
				pCmdBuf += sizeof( Vector4D ); // skip vSrcColor2 Vector
			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE:
			{


				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				// Read the material-level gamma color scale ($color2) 
				Vector vSrcColor2 = GetData<Vector>( pCmdBuf );
				pCmdBuf += sizeof( Vector ); // skip vSrcColor2 Vector

				if ( config.nFullbright != 2 )
				{
					// Get the per-model-instance diffuse modulation
					const Vector4D &srcColor = instance.m_DiffuseModulation;

					Vector4D color;

					color[0] = srcColor[0] * vSrcColor2[0];
					color[1] = srcColor[1] * vSrcColor2[1];
					color[2] = srcColor[2] * vSrcColor2[2];
					color[3] = srcColor[3];

					SetPixelShaderConstantInternal( nReg, color.Base() );
				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetPixelShaderConstantInternal( nReg, white.Base() );
				}

			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_IDENTITY:
			{


				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				Vector4D color( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
				SetPixelShaderConstantInternal( nReg, color.Base() );
			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARCOLORSPACE_LINEARSCALE:
			{

				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register


				if ( config.nFullbright != 2 )
				{
					// Read the material-level gamma color scale ($color2) 
					if ( USE_OLD_GAMMA )
					{
						Vector4D vSrcColor2 = GetData<Vector4D>( pCmdBuf );
						pCmdBuf += sizeof( Vector4D ); // skip vSrcColor2 Vector

						// Read the linear scale value
						float scale = GetData<float>( pCmdBuf );
						pCmdBuf += sizeof( float ); // skip the linear scale value

						// Get the per-model-instance diffuse modulation
						const Vector4D &srcColor = instance.m_DiffuseModulation;
						Vector4D color;

						color[0] = srcColor[0] * vSrcColor2[0];
						color[1] = srcColor[1] * vSrcColor2[1];
						color[2] = srcColor[2] * vSrcColor2[2];
						color[3] = srcColor[3];

						color[0] = ( color[0] > 1.0f ? color[0] : GammaToLinear( color[0] ) ) * scale;
						color[1] = ( color[1] > 1.0f ? color[1] : GammaToLinear( color[1] ) ) * scale;
						color[2] = ( color[2] > 1.0f ? color[2] : GammaToLinear( color[2] ) ) * scale;

						SetPixelShaderConstantInternal( nReg, color.Base() );
					}
					else
					{
						fltx4 fl4SrcColor2 = LoadUnaligned3SIMD( pCmdBuf ); 
						pCmdBuf += sizeof( Vector4D );

						float scale = GetData<float>( pCmdBuf );
						pCmdBuf += sizeof( float ); // skip the linear scale value

						fltx4 fl4Scale = ReplicateX4( scale );
						fltx4 fl4SrcColor = LoadUnalignedSIMD( &instance.m_DiffuseModulation );

						fl4SrcColor2 = MulSIMD( GammaToLinearExtendedSIMD( MulSIMD( fl4SrcColor2, fl4SrcColor ) ), fl4Scale );
						fl4SrcColor2 = SetWSIMD( fl4SrcColor2, fl4SrcColor ); // copy back the original unmodified alpha

						SetPixelShaderConstantInternal( nReg, ( float const * ) &fl4SrcColor2 );
					}
				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetPixelShaderConstantInternal( nReg, white.Base() );
					pCmdBuf += sizeof( float ) + sizeof( Vector4D );
				}

			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARSCALE:
			{

				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				// Read the material-level gamma color scale ($color2) 
				Vector vSrcColor2 = GetData<Vector>( pCmdBuf );
				pCmdBuf += sizeof( Vector ); // skip vSrcColor2 Vector

				// skip the pad
				pCmdBuf += sizeof( float );

				// Read the linear scale value
				float scale = GetData<float>( pCmdBuf );
				pCmdBuf += sizeof( float ); // skip the linear scale value

				if (config.nFullbright != 2 )
				{
					// Get the per-model-instance diffuse modulation
					const Vector4D &srcColor = instance.m_DiffuseModulation;
					Vector4D color;

					color[0] = srcColor[0] * vSrcColor2[0] * scale;
					color[1] = srcColor[1] * vSrcColor2[1] * scale;
					color[2] = srcColor[2] * vSrcColor2[2] * scale;
					color[3] = srcColor[3];

					SetPixelShaderConstantInternal( nReg, color.Base() );
				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetPixelShaderConstantInternal( nReg, white.Base() );
				}
			}
			break;

		case CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARSCALE_SCALEINW:
			{

				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				// Read the material-level gamma color scale ($color2) 
				Vector vSrcColor2 = GetData<Vector>( pCmdBuf );
				pCmdBuf += sizeof( Vector ); // skip vSrcColor2 Vector

				// Read the linear scale value
				float scale = GetData<float>( pCmdBuf );
				pCmdBuf += sizeof( float ); // skip the linear scale value

				if ( config.nFullbright != 2 )
				{
					// Get the per-model-instance diffuse modulation
					const Vector4D &srcColor = instance.m_DiffuseModulation;
					Vector4D color;

					color[0] = srcColor[0] * vSrcColor2[0] * scale;
					color[1] = srcColor[1] * vSrcColor2[1] * scale;
					color[2] = srcColor[2] * vSrcColor2[2] * scale;
					color[3] = scale;

					SetPixelShaderConstantInternal( nReg, color.Base() );
				}
				else
				{
					Vector4D vecScale( scale, scale, scale, scale );
					SetPixelShaderConstantInternal( nReg, vecScale.Base() );
				}
			}
			break;

		case CBICMD_SETMODULATIONVERTEXSHADERDYNAMICSTATE:
			{

				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				// Read the material-level gamma color scale ($color2) 
				Vector vSrcColor2 = GetData<Vector>( pCmdBuf );
				pCmdBuf += sizeof( Vector ); // skip vSrcColor2 Vector

				if ( config.nFullbright != 2 )
				{
					// Get the per-model-instance diffuse modulation
					const Vector4D &srcColor = instance.m_DiffuseModulation;

					Vector4D color;

					color[0] = srcColor[0] * vSrcColor2[0];
					color[1] = srcColor[1] * vSrcColor2[1];
					color[2] = srcColor[2] * vSrcColor2[2];
					color[3] = srcColor[3];

					SetVertexShaderConstantInternal( nReg, color.Base() );
				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetVertexShaderConstantInternal( nReg, white.Base() );
				}
			}
			break;

		case CBICMD_SETMODULATIONVERTEXSHADERDYNAMICSTATE_LINEARSCALE:
			{

				// Skip the command.
				pCmdBuf += sizeof( int );

				// Read the register number that we want to write the colormodulation value into.
				int nReg = GetData<int>( pCmdBuf ); 
				pCmdBuf += sizeof( int ); // skip register

				// Read the material-level gamma color scale ($color2) 
				Vector vSrcColor2 = GetData<Vector>( pCmdBuf );
				pCmdBuf += sizeof( Vector ); // skip vSrcColor2 Vector

				float flScale = GetData<float>( pCmdBuf );
				pCmdBuf += sizeof( float );

				if ( config.nFullbright != 2 )
				{
					// Get the per-model-instance diffuse modulation
					const Vector4D &srcColor = instance.m_DiffuseModulation;

					Vector4D color;

					color[0] = srcColor[0] * vSrcColor2[0] * flScale;
					color[1] = srcColor[1] * vSrcColor2[1] * flScale;
					color[2] = srcColor[2] * vSrcColor2[2] * flScale;
					color[3] = srcColor[3];

					SetVertexShaderConstantInternal( nReg, color.Base() );
				}
				else
				{
					Vector4D white( 1.0f, 1.0f, 1.0f, instance.m_DiffuseModulation[3] );
					SetVertexShaderConstantInternal( nReg, white.Base() );
				}
			}
			break;

#ifndef NDEBUG
		default:
			Warning( " unknown instance command %d last = %d\n", nCmd, GetData<int>( pLastCmd ) );
			DebuggerBreak();
			break;
#endif
		}
		pLastCmd = pCmd;
	}
}

#endif //_PS3

//-----------------------------------------------------------------------------
// Sets state on the board related to the texture state
//-----------------------------------------------------------------------------

#ifndef _PS3

void CShaderAPIDx8::SetTextureState( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t hTexture, bool force )
{
	// Get the dynamic texture info
	SamplerState_t &samplerState = SamplerState( sampler );

	// Set the texture state, but only if it changes
	if ( ( samplerState.m_BoundTexture == hTexture ) && ( LastSetTextureBindFlags( sampler ) == nBindFlags ) && !force )
		return;

	// Disabling texturing
	if ( hTexture == INVALID_SHADERAPI_TEXTURE_HANDLE || WouldBeOverTextureLimit( hTexture ) )
	{
		Dx9Device()->SetTexture( sampler, 0 );
		return;
	}

	samplerState.m_BoundTexture = hTexture;

	RECORD_COMMAND( DX8_SET_TEXTURE, 3 );
	RECORD_INT( sampler );
	RECORD_INT( hTexture );
	RECORD_INT( GetTexture( hTexture).m_CurrentCopy );

	IDirect3DBaseTexture *pTexture = CShaderAPIDx8::GetD3DTexture( hTexture );
	
#if defined( _X360 )
	GPUTEXTURE_FETCH_CONSTANT linearFormatBackup = pTexture->Format;

	if ( nBindFlags & TEXTURE_BINDFLAGS_SRGBREAD )
	{
#if defined( CSTRIKE15 )

		// [mariod] - no PWL textures
		// TODO - ensure all textures coming through here are set appropriately for shader srgb reads (not all shaders currently set to do this)
#else
		// Use X360 HW path
		if ( r_use16bit_srgb_sampling.GetBool() )
		{
			// Convert the GPU format to an EXPAND or AS_16 higher precision variant if one is available, and enable GAMMA.
			ConvertTextureToAs16SRGBFormat( reinterpret_cast< D3DTexture * >( pTexture ) );
		}
		else 
		{
			// convert to srgb format for the bind. This effectively emulates the old srgb read sampler state
			pTexture->Format.SignX = pTexture->Format.SignY = pTexture->Format.SignZ = 3; 
		}
#endif
	}

#else
	SetSamplerState( sampler, D3DSAMP_SRGBTEXTURE, ( nBindFlags & TEXTURE_BINDFLAGS_SRGBREAD ) != 0 );
#endif

	samplerState.m_nTextureBindFlags = nBindFlags;

	TouchTexture( sampler, pTexture );
	Dx9Device()->SetTexture( sampler, pTexture );

#if defined( _X360 )
	// put the format back in linear space
	pTexture->Format = linearFormatBackup;
#endif

	Texture_t &tex = GetTexture( hTexture );
	if ( tex.m_LastBoundFrame != m_CurrentFrame )
	{
		tex.m_LastBoundFrame = m_CurrentFrame;
		tex.m_nTimesBoundThisFrame = 0;

		if ( tex.m_pTextureGroupCounterFrame )
		{
			// Update the per-frame texture group counter.
			*tex.m_pTextureGroupCounterFrame += tex.GetMemUsage();
		}

		// Track memory usage.
		m_nTextureMemoryUsedLastFrame += tex.GetMemUsage();
	}

	if ( !m_bDebugTexturesRendering )
		++tex.m_nTimesBoundThisFrame;

	tex.m_nTimesBoundMax = MAX( tex.m_nTimesBoundMax, tex.m_nTimesBoundThisFrame );
	
	MaterialSystem_Config_t &config = ShaderUtil()->GetConfig();
	bool noFilter = config.bFilterTextures == 0;
	bool noMipFilter = config.bMipMapTextures == 0;

	// Set SHADOWFILTER or ATI Fetch4
#if defined ( DX_TO_GL_ABSTRACTION ) && !defined( _PS3 )
	const uint nNewShadowFilterState = ( nBindFlags & TEXTURE_BINDFLAGS_SHADOWDEPTH ) ? 1 : 0;
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_SHADOWFILTER, m_bShadowFilterEnable, nNewShadowFilterState );
#elif !defined( PLATFORM_X360 )
	if ( g_pHardwareConfig->SupportsFetch4() )
	{
		const uint nNewFetch4State = ( nBindFlags & TEXTURE_BINDFLAGS_SHADOWDEPTH ) ? ATI_FETCH4_ENABLE : ATI_FETCH4_DISABLE;
		SETSAMPLESTATEANDMIRROR( sampler, samplerState, ATISAMP_FETCH4, m_bShadowFilterEnable, nNewFetch4State );

		if ( nBindFlags & TEXTURE_BINDFLAGS_SHADOWDEPTH )
		{
			noFilter = true;
			noMipFilter = true;
		}
	}
#endif

	if ( nBindFlags & TEXTURE_BINDFLAGS_NOMIP )
	{
		noMipFilter = true;
	}

	D3DTEXTUREFILTERTYPE minFilter = noFilter ? D3DTEXF_NONE : D3DTEXTUREFILTERTYPE(tex.m_MinFilter);
	D3DTEXTUREFILTERTYPE magFilter = noFilter ? D3DTEXF_NONE : D3DTEXTUREFILTERTYPE(tex.m_MagFilter);
	D3DTEXTUREFILTERTYPE mipFilter = D3DTEXTUREFILTERTYPE(tex.m_MipFilter);
	if ( noMipFilter )
	{
		mipFilter = D3DTEXF_NONE;
	} 
	else if ( noFilter )
	{
		mipFilter = D3DTEXF_POINT;
	}

	D3DTEXTUREADDRESS uTexWrap = D3DTEXTUREADDRESS(tex.m_UTexWrap);
	D3DTEXTUREADDRESS vTexWrap = D3DTEXTUREADDRESS(tex.m_VTexWrap);
	D3DTEXTUREADDRESS wTexWrap = D3DTEXTUREADDRESS(tex.m_WTexWrap);
		
#if DX_TO_GL_ABSTRACTION
	if ( ( samplerState.m_MinFilter != minFilter ) || ( samplerState.m_MagFilter != magFilter ) || ( samplerState.m_MipFilter != mipFilter ) ||
		 ( samplerState.m_UTexWrap != uTexWrap ) || ( samplerState.m_VTexWrap != vTexWrap ) || ( samplerState.m_WTexWrap != wTexWrap ) )
	{
		samplerState.m_UTexWrap = uTexWrap;
		samplerState.m_VTexWrap = vTexWrap;
		samplerState.m_WTexWrap = wTexWrap;
		samplerState.m_MinFilter = minFilter;
		samplerState.m_MagFilter = magFilter;
		samplerState.m_MipFilter = mipFilter;
		Dx9Device()->SetSamplerStates( sampler, uTexWrap, vTexWrap, wTexWrap, minFilter, magFilter, mipFilter );
	}
#else
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_ADDRESSU, m_UTexWrap, uTexWrap );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_ADDRESSV, m_VTexWrap, vTexWrap );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_ADDRESSW, m_WTexWrap, wTexWrap );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MINFILTER, m_MinFilter, minFilter );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MAGFILTER, m_MagFilter, magFilter );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MIPFILTER, m_MipFilter, mipFilter );
#endif
}

void CShaderAPIDx8::SetTextureFilterMode( Sampler_t sampler, TextureFilterMode_t nMode )
{
	SamplerState_t &samplerState = SamplerState( sampler );
	D3DTEXTUREFILTERTYPE minFilter = samplerState.m_MinFilter;
	D3DTEXTUREFILTERTYPE magFilter = samplerState.m_MagFilter;
	D3DTEXTUREFILTERTYPE mipFilter = samplerState.m_MipFilter;
	

	switch( nMode )
	{
		case TFILTER_MODE_POINTSAMPLED:
			minFilter = D3DTEXF_POINT;
			magFilter = D3DTEXF_POINT;
			mipFilter = D3DTEXF_POINT;
			break;
	}
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MINFILTER, m_MinFilter, minFilter );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MAGFILTER, m_MagFilter, magFilter );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MIPFILTER, m_MipFilter, mipFilter );
	
}

void CShaderAPIDx8::BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle )
{
	LOCK_SHADERAPI();

	// A little trickery to deal with instance-based rendering.
	// BindTexture calls are expected to come from shaders. We need to sniff the
	// state sets here to see if any bind are set to the current local env cubemap.
	// If so, we need to keep track of that so that when we change per-instance
	// state, we know that we need to change these textures. Each time we start
	// rendering a new pass, we reset the bitfield indicating which samplers to modify.
	if ( IsRenderingInstances() )
	{
		int nMask = ( 1 << sampler );
		m_DynamicState.m_nLocalEnvCubemapSamplers &= ~nMask;
		m_DynamicState.m_nLightmapSamplers &= ~nMask;
		m_DynamicState.m_nPaintmapSamplers &= ~nMask;

		bool bIsLocalEnvCubemap = ( textureHandle == m_StdTextureHandles[TEXTURE_LOCAL_ENV_CUBEMAP] );
		if ( bIsLocalEnvCubemap )
		{
			m_DynamicState.m_nLocalEnvCubemapSamplers |= nMask;
			SetLastSetTextureBindFlags( sampler, nBindFlags );
			return;
		}

		bool bIsLightmap = ( textureHandle == m_StdTextureHandles[TEXTURE_LIGHTMAP] );
		if ( bIsLightmap )
		{
			m_DynamicState.m_nLightmapSamplers |= nMask;
			SetLastSetTextureBindFlags( sampler, nBindFlags );
			return;
		}

		bool bIsPaintmap = ( textureHandle == m_StdTextureHandles[TEXTURE_PAINT] );
		if ( bIsPaintmap )
		{
			m_DynamicState.m_nPaintmapSamplers |= nMask;
			SetLastSetTextureBindFlags( sampler, nBindFlags );
			return;
		}
	}

	SetTextureState( sampler, nBindFlags, textureHandle );
}

#endif

void CShaderAPIDx8::BindVertexTexture( VertexTextureSampler_t nStage, ShaderAPITextureHandle_t textureHandle )
{
	Assert( g_pMaterialSystemHardwareConfig->GetVertexSamplerCount() != 0 );
	LOCK_SHADERAPI();
	ADD_VERTEX_TEXTURE_FUNC( COMMIT_PER_PASS, CommitVertexTextures, nStage, m_BoundVertexTexture, textureHandle );
}


//-----------------------------------------------------------------------------
// Texture allocation/deallocation
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Computes stats info for a texture
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ComputeStatsInfo( ShaderAPITextureHandle_t hTexture, bool bIsCubeMap, bool isVolumeTexture )
{
	Texture_t &textureData = GetTexture( hTexture );

	textureData.m_SizeBytes = 0;
	textureData.m_SizeTexels = 0;
	textureData.m_LastBoundFrame = -1;
	if ( IsX360() )
	{
		textureData.m_nTimesBoundThisFrame = 0;
	}

	IDirect3DBaseTexture* pD3DTex = CShaderAPIDx8::GetD3DTexture( hTexture );

	if ( IsPC() || !IsX360() )
	{
		if ( bIsCubeMap )
		{
			IDirect3DCubeTexture* pTex = static_cast<IDirect3DCubeTexture*>(pD3DTex);
			if ( !pTex )
			{
				Assert( 0 );
				return;
			}

			int numLevels = pTex->GetLevelCount();
			for (int i = 0; i < numLevels; ++i)
			{
				D3DSURFACE_DESC desc;
				HRESULT hr = pTex->GetLevelDesc( i, &desc );
				Assert( !FAILED(hr) );
				textureData.m_SizeBytes += 6 * ImageLoader::GetMemRequired( desc.Width, desc.Height, 1, textureData.GetImageFormat(), false );
				textureData.m_SizeTexels += 6 * desc.Width * desc.Height;
			}
		}
		else if ( isVolumeTexture )
		{
			IDirect3DVolumeTexture9* pTex = static_cast<IDirect3DVolumeTexture9*>(pD3DTex);
			if ( !pTex )
			{
				Assert( 0 );
				return;
			}
			int numLevels = pTex->GetLevelCount();
			for (int i = 0; i < numLevels; ++i)
			{
				D3DVOLUME_DESC desc;
				HRESULT hr = pTex->GetLevelDesc( i, &desc );
				Assert( !FAILED( hr ) );
				textureData.m_SizeBytes += ImageLoader::GetMemRequired( desc.Width, desc.Height, desc.Depth, textureData.GetImageFormat(), false );
				textureData.m_SizeTexels += desc.Width * desc.Height;
			}
		}
		else
		{
			IDirect3DTexture* pTex = static_cast<IDirect3DTexture*>(pD3DTex);
			if ( !pTex )
			{
				Assert( 0 );
				return;
			}

			int numLevels = pTex->GetLevelCount();
			for (int i = 0; i < numLevels; ++i)
			{
				D3DSURFACE_DESC desc;
				HRESULT hr = pTex->GetLevelDesc( i, &desc );
				Assert( !FAILED( hr ) );
				textureData.m_SizeBytes += ImageLoader::GetMemRequired( desc.Width, desc.Height, 1, textureData.GetImageFormat(), false );
				textureData.m_SizeTexels += desc.Width * desc.Height;
			}
		}
	}

#if defined( _X360 )
	// 360 uses gpu storage size (which accounts for page alignment bloat), not format size
	textureData.m_SizeBytes = g_TextureHeap.GetSize( pD3DTex );
#endif
}

static D3DFORMAT ComputeFormat( IDirect3DBaseTexture* pTexture, bool bIsCubeMap )
{
	Assert( pTexture );
	D3DSURFACE_DESC desc;
	if ( bIsCubeMap )
	{
		IDirect3DCubeTexture* pTex = static_cast<IDirect3DCubeTexture*>(pTexture);
		HRESULT hr = pTex->GetLevelDesc( 0, &desc );
		Assert( !FAILED(hr) );
	}
	else
	{
		IDirect3DTexture* pTex = static_cast<IDirect3DTexture*>(pTexture);
		HRESULT hr = pTex->GetLevelDesc( 0, &desc );
		Assert( !FAILED(hr) );
	}
	return desc.Format;
}

ShaderAPITextureHandle_t CShaderAPIDx8::CreateDepthTexture( 
	ImageFormat renderTargetFormat, 
	int width, 
	int height, 
	const char *pDebugName,
	bool bTexture, 
	bool bAliasDepthSurfaceOverColorX360 )
{
	bAliasDepthSurfaceOverColorX360;

	LOCK_SHADERAPI();

	ShaderAPITextureHandle_t i = CreateTextureHandle();
	Texture_t *pTexture = &GetTexture( i );

	pTexture->m_Flags = Texture_t::IS_ALLOCATED;
	if( bTexture )
	{
		pTexture->m_Flags |= Texture_t::IS_DEPTH_STENCIL_TEXTURE;
	}
	else
	{
		pTexture->m_Flags |= Texture_t::IS_DEPTH_STENCIL;
	}

	pTexture->m_DebugName = pDebugName;
	pTexture->m_Width = width;
	pTexture->m_Height = height;
	pTexture->m_Depth = 1;			// fake
	pTexture->m_Count = 1;			// created single texture
	pTexture->m_CountIndex = 0;		// created single texture
	pTexture->m_CreationFlags = 0;	// fake
	pTexture->m_NumCopies = 1;
	pTexture->m_CurrentCopy = 0;

	ImageFormat renderFormat = ImageLoader::D3DFormatToImageFormat( FindNearestSupportedFormat( renderTargetFormat, false, true, false ) );
#if defined( _X360 )
	D3DFORMAT nDepthFormat = ReverseDepthOnX360() ? D3DFMT_D24FS8 : D3DFMT_D24S8;
#else
	D3DFORMAT nDepthFormat = m_bUsingStencil ? D3DFMT_D24S8 : D3DFMT_D24X8;
#endif
	D3DFORMAT format = FindNearestSupportedDepthFormat( m_nAdapter, m_AdapterFormat, renderFormat, nDepthFormat );
	D3DMULTISAMPLE_TYPE multisampleType = D3DMULTISAMPLE_NONE;

	pTexture->m_NumLevels = 1;
	pTexture->m_SizeTexels = width * height;
	pTexture->m_SizeBytes = ImageLoader::GetMemRequired( width, height, 1, renderFormat, false );

	RECORD_COMMAND( DX8_CREATE_DEPTH_TEXTURE, 5 );
	RECORD_INT( i );
	RECORD_INT( width );
	RECORD_INT( height );
	RECORD_INT( format );
	RECORD_INT( multisampleType );

	HRESULT hr;
	if ( !bTexture )
	{
#if defined( _X360 )
		int backWidth, backHeight;
		ShaderAPI()->GetBackBufferDimensions( backWidth, backHeight );
		D3DFORMAT backBufferFormat = ImageLoader::ImageFormatToD3DFormat( g_pShaderDevice->GetBackBufferFormat() );
		// immediately follows back buffer in EDRAM
		D3DSURFACE_PARAMETERS surfParameters;
		V_memset( &surfParameters, 0, sizeof( surfParameters ) );

		// FIXME: The multiply by two below seems suspect. I believe it's to account for size of the color buffer AND the size of the 
		// depth buffer, but it assumes that both buffers always have the same size. This is probably the case now, but might not be in the future.
		surfParameters.Base = 2*XGSurfaceSize( backWidth, backHeight, backBufferFormat, g_TextureHeap.GetBackBufferMultiSampleType() );

		if ( !bAliasDepthSurfaceOverColorX360 )
		{
			// If we don't do this, the color RT and depth RT will be co-located in EDRAM
			surfParameters.Base += XGSurfaceSize( width, height, ImageLoader::ImageFormatToD3DFormat( renderFormat ), D3DMULTISAMPLE_NONE );
		}

		surfParameters.ColorExpBias = 0;
		surfParameters.HierarchicalZBase = 0;
		surfParameters.HiZFunc = D3DHIZFUNC_DEFAULT;
		if ( XGHierarchicalZSize( backWidth, backHeight, D3DMULTISAMPLE_NONE ) + XGHierarchicalZSize( width, height, D3DMULTISAMPLE_NONE ) > GPU_HIERARCHICAL_Z_TILES )
		{
			// overflow, disable HiZ
			surfParameters.HierarchicalZBase = 0xFFFFFFFF;
		}

		hr = Dx9Device()->CreateDepthStencilSurface(
			width, height, format, multisampleType, 0, TRUE, &pTexture->GetDepthStencilSurface(), &surfParameters );
#else
		hr = Dx9Device()->CreateDepthStencilSurface(
			width, height, format, multisampleType, 0, TRUE, &pTexture->GetDepthStencilSurface(), NULL );
#endif
	}
	else
	{
		IDirect3DTexture9 *pTex;
		hr = Dx9Device()->CreateTexture( width, height, 1, D3DUSAGE_DEPTHSTENCIL, format, D3DPOOL_DEFAULT, &pTex, NULL );
		pTexture->SetTexture( pTex );
	}

    if ( FAILED( hr ) )
	{
		switch( hr )
		{
		case D3DERR_INVALIDCALL:
			Warning( "ShaderAPIDX8::CreateDepthStencilSurface: D3DERR_INVALIDCALL\n" );
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			Warning( "ShaderAPIDX8::CreateDepthStencilSurface: D3DERR_OUTOFVIDEOMEMORY\n" );
			break;
		default:
			break;
		}
		Assert( 0 );
	}

	return i;
}

// FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Could keep a free-list for this instead of linearly searching.  We
// don't create textures all the time, so this is probably fine for now.
// FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
ShaderAPITextureHandle_t CShaderAPIDx8::CreateTextureHandle( void )
{
	ShaderAPITextureHandle_t handle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	CreateTextureHandles( &handle, 1, false );
	return handle;
}

void CShaderAPIDx8::CreateTextureHandles( ShaderAPITextureHandle_t *pHandles, int nCount, bool bReuseHandles )
{
	if ( nCount <= 0 )
		return;

	MEM_ALLOC_CREDIT();

	ShaderAPITextureHandle_t hTexture;

	if ( bReuseHandles )
	{
		int nValid = 0;
		for ( int i = 0; i < nCount; i++ )
		{
			// prior handles were freed and caller wants them back
			hTexture = pHandles[i];
			if ( m_Textures.IsValidIndex( hTexture) && !( m_Textures[hTexture].m_Flags & Texture_t::IS_ALLOCATED ) )
			{
				nValid++;
			}
		}

		if ( nValid == nCount )
		{
			// all prior handles are available and can be re-used
			return;
		}
	}

	int idxCreating = 0;
	for ( hTexture = m_Textures.Head(); hTexture != m_Textures.InvalidIndex(); hTexture = m_Textures.Next( hTexture ) )
	{
		if ( !( m_Textures[hTexture].m_Flags & Texture_t::IS_ALLOCATED ) )
		{
			pHandles[ idxCreating++ ] = hTexture;
			if ( idxCreating >= nCount )
				return;
		}
	}

	while ( idxCreating < nCount )
	{
		pHandles[ idxCreating++ ] = m_Textures.AddToTail();
	}
}

//-----------------------------------------------------------------------------
// Creates a lovely texture
//-----------------------------------------------------------------------------

ShaderAPITextureHandle_t CShaderAPIDx8::CreateTexture(
	int			width, 
	int			height, 
	int			depth,
	ImageFormat dstImageFormat, 
	int			numMipLevels, 
	int			numCopies, 
	int			creationFlags,
	const char *pDebugName,
	const char *pTextureGroupName )
{
	ShaderAPITextureHandle_t handle = 0;
	CreateTextures( &handle, 1, width, height, depth, dstImageFormat, numMipLevels, numCopies, creationFlags, pDebugName, pTextureGroupName );
	return handle;
}

void CShaderAPIDx8::CreateTextures(
	ShaderAPITextureHandle_t *pHandles,
	int			count,
	int			width, 
	int			height, 
	int			depth,
	ImageFormat dstImageFormat, 
	int			numMipLevels, 
	int			numCopies, 
	int			creationFlags,
	const char *pDebugName,
	const char *pTextureGroupName )
{
	LOCK_SHADERAPI();

	if ( depth == 0 )
	{
		depth = 1;
	}

	bool bIsCubeMap = ( creationFlags & TEXTURE_CREATE_CUBEMAP ) != 0;
	bool bIsRenderTarget = ( creationFlags & TEXTURE_CREATE_RENDERTARGET ) != 0;
	bool bIsManaged = ( creationFlags & TEXTURE_CREATE_MANAGED ) != 0;
	bool bIsDepthBuffer = ( creationFlags & TEXTURE_CREATE_DEPTHBUFFER ) != 0;
	bool bIsDynamic = ( creationFlags & TEXTURE_CREATE_DYNAMIC ) != 0;
	bool isSRGB = ( creationFlags & TEXTURE_CREATE_SRGB ) != 0;			// for Posix/GL only... not used here ?
	bool bReuseHandles = ( creationFlags & TEXTURE_CREATE_REUSEHANDLES ) != 0;

	// Can't be both managed + dynamic. Dynamic is an optimization, but 
	// if it's not managed, then we gotta do special client-specific stuff
	// So, managed wins out!
	if ( bIsManaged )
	{
		bIsDynamic = false;
	}

	// Create a set of texture handles
	CreateTextureHandles( pHandles, count, bReuseHandles );
	Texture_t **arrTxp = ( Texture_t ** ) stackalloc( count * sizeof( Texture_t * ) );

	unsigned short usSetFlags = 0;
	usSetFlags |= ( creationFlags & TEXTURE_CREATE_VERTEXTEXTURE)  ? Texture_t::IS_VERTEX_TEXTURE : 0;
#if defined( _GAMECONSOLE )
	usSetFlags |= ( creationFlags & TEXTURE_CREATE_RENDERTARGET ) ? Texture_t::IS_RENDER_TARGET : 0;
	usSetFlags |= ( creationFlags & TEXTURE_CREATE_CANCONVERTFORMAT ) ? Texture_t::CAN_CONVERT_FORMAT : 0;
	usSetFlags |= ( creationFlags & TEXTURE_CREATE_PWLCORRECTED ) ? Texture_t::IS_PWL_CORRECTED : 0;
	usSetFlags |= ( creationFlags & TEXTURE_CREATE_ERROR ) ? Texture_t::IS_ERROR_TEXTURE : 0;
#endif

	for ( int idxFrame = 0; idxFrame < count; ++ idxFrame )
	{
		arrTxp[ idxFrame ] = &GetTexture( pHandles[ idxFrame ] );
		Texture_t *pTexture = arrTxp[ idxFrame ];
		pTexture->m_Flags = Texture_t::IS_ALLOCATED;
		pTexture->m_DebugName = pDebugName;
		pTexture->m_Width = width;
		pTexture->m_Height = height;
		pTexture->m_Depth = depth;
		pTexture->m_Count = count;
		pTexture->m_CountIndex = idxFrame;

		pTexture->m_CreationFlags = creationFlags;
		pTexture->m_Flags |= usSetFlags;

		RECORD_COMMAND( DX8_CREATE_TEXTURE, 12 );
		RECORD_INT( textureHandle );
		RECORD_INT( width );
		RECORD_INT( height );
		RECORD_INT( depth );		// depth for volume textures
		RECORD_INT( FindNearestSupportedFormat(dstImageFormat) );
		RECORD_INT( numMipLevels );
		RECORD_INT( bIsCubeMap );
		RECORD_INT( numCopies <= 1 ? 1 : numCopies );
		RECORD_INT( bIsRenderTarget ? 1 : 0 );
		RECORD_INT( bIsManaged );
		RECORD_INT( bIsDepthBuffer ? 1 : 0 );
		RECORD_INT( bIsDynamic ? 1 : 0 );

		IDirect3DBaseTexture* pD3DTex;

		// Set the initial texture state
		if ( numCopies <= 1 )
		{
			pTexture->m_NumCopies = 1;
			pD3DTex = CreateD3DTexture( width, height, depth, dstImageFormat, numMipLevels, creationFlags, (char*)pDebugName );
			pTexture->SetTexture( pD3DTex );
		}
		else
		{
			pTexture->m_NumCopies = numCopies;
			{
// X360TEMP
//				MEM_ALLOC_CREDIT();
				pTexture->GetTextureArray() = new IDirect3DBaseTexture* [numCopies];
			}
			for (int k = 0; k < numCopies; ++k)
			{
				pD3DTex = CreateD3DTexture( width, height, depth, dstImageFormat, numMipLevels, creationFlags, (char*)pDebugName );
				pTexture->SetTexture( k, pD3DTex );
			}
		}
		pTexture->m_CurrentCopy = 0;

		pD3DTex = CShaderAPIDx8::GetD3DTexture( pHandles[ idxFrame ] );

#if defined( _X360 )
		if ( pD3DTex )
		{
			D3DSURFACE_DESC desc;
			HRESULT	hr;
			if ( creationFlags & TEXTURE_CREATE_CUBEMAP )
			{
				hr = ((IDirect3DCubeTexture *)pD3DTex)->GetLevelDesc( 0, &desc );
			}
			else
			{
				hr = ((IDirect3DTexture *)pD3DTex)->GetLevelDesc( 0, &desc );
			}
			Assert( !FAILED( hr ) );

			// for proper info get the actual format because the input format may have been redirected
			dstImageFormat = ImageLoader::D3DFormatToImageFormat( desc.Format );
			Assert( dstImageFormat != IMAGE_FORMAT_UNKNOWN );

			// track linear or tiled
			if ( !XGIsTiledFormat( desc.Format ) )
			{
				pTexture->m_Flags |= Texture_t::IS_LINEAR;
			}

			if ( creationFlags & TEXTURE_CREATE_CACHEABLE ) 
			{
				// actual caching ability was resolved by the texture heap
				if ( g_TextureHeap.IsTextureCacheManaged( pD3DTex ) )
				{
					pTexture->m_Flags |= Texture_t::IS_CACHEABLE;
				}
			}
		}
#endif

		pTexture->SetImageFormat( dstImageFormat );
		pTexture->m_UTexWrap = D3DTADDRESS_CLAMP;
		pTexture->m_VTexWrap = D3DTADDRESS_CLAMP;
		pTexture->m_WTexWrap = D3DTADDRESS_CLAMP;

		if ( bIsRenderTarget )
		{
#if !defined( _X360 ) && !defined( _PS3 )
			if ( ( g_pHardwareConfig->Caps().m_VendorID == VENDORID_ATI ) &&  
				 ( ( dstImageFormat == IMAGE_FORMAT_D16_SHADOW ) || ( dstImageFormat == IMAGE_FORMAT_D24X8_SHADOW ) ) )
			{
				pTexture->m_MinFilter = pTexture->m_MagFilter = D3DTEXF_POINT;
			}
			else
#endif
			{
				pTexture->m_MinFilter = pTexture->m_MagFilter = D3DTEXF_LINEAR;
			}

			pTexture->m_NumLevels = 1;
			pTexture->m_MipFilter = D3DTEXF_NONE;
		}
		else
		{
			pTexture->m_NumLevels = pD3DTex ? pD3DTex->GetLevelCount() : 1;
			pTexture->m_MipFilter = (pTexture->m_NumLevels != 1) ? D3DTEXF_LINEAR : D3DTEXF_NONE;
			pTexture->m_MinFilter = pTexture->m_MagFilter = D3DTEXF_LINEAR;
		}
		pTexture->m_SwitchNeeded = false;
		
		ComputeStatsInfo( pHandles[idxFrame], bIsCubeMap, (depth > 1) );
		SetupTextureGroup( pHandles[idxFrame], pTextureGroupName );
	}
}

void CShaderAPIDx8::SetupTextureGroup( ShaderAPITextureHandle_t hTexture, const char *pTextureGroupName )
{
	Texture_t *pTexture = &GetTexture( hTexture );

	Assert( !pTexture->m_pTextureGroupCounterGlobal );
	
	// Setup the texture group stuff.
	if ( pTextureGroupName && pTextureGroupName[0] != 0 )
	{
		pTexture->m_TextureGroupName = pTextureGroupName;
	}
	else
	{
		pTexture->m_TextureGroupName = TEXTURE_GROUP_UNACCOUNTED;
	}

	// 360 cannot vprof due to multicore loading until vprof is reentrant and these counters are real.
#if defined( VPROF_ENABLED ) && !defined( _X360 )
	char counterName[256];
	Q_snprintf( counterName, sizeof( counterName ), "TexGroup_global_%s", pTexture->m_TextureGroupName.String() );
	pTexture->m_pTextureGroupCounterGlobal = g_VProfCurrentProfile.FindOrCreateCounter( counterName, COUNTER_GROUP_TEXTURE_GLOBAL );

	Q_snprintf( counterName, sizeof( counterName ), "TexGroup_frame_%s", pTexture->m_TextureGroupName.String() );
	pTexture->m_pTextureGroupCounterFrame = g_VProfCurrentProfile.FindOrCreateCounter( counterName, COUNTER_GROUP_TEXTURE_PER_FRAME );
#else
	pTexture->m_pTextureGroupCounterGlobal = NULL;
	pTexture->m_pTextureGroupCounterFrame  = NULL;
#endif

	if ( pTexture->m_pTextureGroupCounterGlobal )
	{
		*pTexture->m_pTextureGroupCounterGlobal += pTexture->GetMemUsage();
	}
}

//-----------------------------------------------------------------------------
// Deletes a texture...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DeleteD3DTexture( ShaderAPITextureHandle_t hTexture )
{
	int numDeallocated = 0;
	Texture_t &texture = GetTexture( hTexture );

	if ( texture.m_Flags & Texture_t::IS_DEPTH_STENCIL )
	{
		// garymcthack - need to make sure that playback knows how to deal with these.
		RECORD_COMMAND( DX8_DESTROY_DEPTH_TEXTURE, 1 );
		RECORD_INT( hTexture );

		if ( texture.GetDepthStencilSurface() )
		{
			int nRetVal = texture.GetDepthStencilSurface()->Release();
			Assert( nRetVal == 0 );
			texture.GetDepthStencilSurface() = 0;
			numDeallocated = 1;
		}
		else
		{
			// FIXME: we hit this on shutdown of HLMV on some machines
			Assert( 0 );
		}
	}
	else if ( texture.m_NumCopies == 1 )
	{
		if ( texture.GetTexture() )
		{
			RECORD_COMMAND( DX8_DESTROY_TEXTURE, 1 );
			RECORD_INT( hTexture );

			DestroyD3DTexture( texture.GetTexture() );
			texture.SetTexture( 0 );
			numDeallocated = 1;
		}
	}
	else
	{
		if ( texture.GetTextureArray() )
		{
			RECORD_COMMAND( DX8_DESTROY_TEXTURE, 1 );
			RECORD_INT( hTexture );

			// Multiple copy texture
			for (int j = 0; j < texture.m_NumCopies; ++j)
			{
				if (texture.GetTexture( j ))
				{
					DestroyD3DTexture( texture.GetTexture( j ) );
					texture.SetTexture( j, 0 );
					++numDeallocated;
				}
			}

			delete [] texture.GetTextureArray();
			texture.GetTextureArray() = 0;
		}
	}

	texture.m_NumCopies = 0;

	// Remove this texture from its global texture group counter.
	if ( texture.m_pTextureGroupCounterGlobal )
	{
		*texture.m_pTextureGroupCounterGlobal -= texture.GetMemUsage();
		// <sergiy> this is an old assert that Mike Dussault added in 2002. It affects computation of free memory. Iestyn said it's better to keep the assert in to find out what's wrong with free memory computation.
		Assert( *texture.m_pTextureGroupCounterGlobal >= 0 ); 
		texture.m_pTextureGroupCounterGlobal = NULL;
	}

	// remove this texture from std textures
	for( int i=0 ; i < ARRAYSIZE( m_StdTextureHandles ) ; i++ )
	{
		if ( m_StdTextureHandles[i] == hTexture )
		{
			m_StdTextureHandles[i] = INVALID_SHADERAPI_TEXTURE_HANDLE;
		}
	}

}


//-----------------------------------------------------------------------------
// Unbinds a texture from all texture stages
//-----------------------------------------------------------------------------
void CShaderAPIDx8::UnbindTexture( ShaderAPITextureHandle_t hTexture )
{
	// Make sure no texture units are currently bound to it...
	for ( int unit = 0; unit < g_pHardwareConfig->GetSamplerCount(); ++unit )
	{
		if ( hTexture == SamplerState( unit ).m_BoundTexture )
		{
			// Gotta set this here because INVALID_SHADERAPI_TEXTURE_HANDLE means don't actually
			// set bound texture (it's used for disabling texturemapping)
			SamplerState( unit ).m_BoundTexture = INVALID_SHADERAPI_TEXTURE_HANDLE;
			SetTextureState( (Sampler_t)unit, TEXTURE_BINDFLAGS_NONE, INVALID_SHADERAPI_TEXTURE_HANDLE );
		}
	}

	int nVertexSamplerCount = g_pHardwareConfig->GetVertexSamplerCount();
	for ( int nSampler = 0; nSampler < nVertexSamplerCount; ++nSampler )
	{
		if ( hTexture == m_DynamicState.m_VertexTextureState[ nSampler ].m_BoundVertexTexture )
		{
			// Gotta set this here because INVALID_SHADERAPI_TEXTURE_HANDLE means don't actually
			// set bound texture (it's used for disabling texture mapping)
			BindVertexTexture( (VertexTextureSampler_t)nSampler, INVALID_SHADERAPI_TEXTURE_HANDLE );
		}
	}
}


//-----------------------------------------------------------------------------
// Deletes a texture...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DeleteTexture( ShaderAPITextureHandle_t textureHandle )
{
	LOCK_SHADERAPI();
	AssertValidTextureHandle( textureHandle );

	if ( !TextureIsAllocated( textureHandle ) )
	{
		// already deallocated
		return;
	}
	
	// Unbind it!
	UnbindTexture( textureHandle );

	// Delete it baby
	DeleteD3DTexture( textureHandle );

	// Now remove the texture from the list
	// Mark as deallocated so that it can be reused.
	GetTexture( textureHandle ).m_Flags = 0;
}


void CShaderAPIDx8::WriteTextureToFile( ShaderAPITextureHandle_t hTexture, const char *szFileName )
{
	Texture_t *pTexInt = &GetTexture( hTexture );
	//Assert( pTexInt->IsCubeMap() == false );
	//Assert( pTexInt->IsVolumeTexture() == false );
	IDirect3DTexture *pD3DTexture = (IDirect3DTexture *)pTexInt->GetTexture();

	// Get the level of the texture we want to read from
	IDirect3DSurface* pTextureLevel;
	HRESULT hr = hr = pD3DTexture ->GetSurfaceLevel( 0, &pTextureLevel );
	if ( FAILED( hr ) )
		return;

	D3DSURFACE_DESC surfaceDesc;
	pD3DTexture->GetLevelDesc( 0, &surfaceDesc );

	D3DLOCKED_RECT	lockedRect;

	
	//if( pTexInt->m_Flags & Texture_t::IS_RENDER_TARGET )
#if !defined( _GAMECONSOLE ) //TODO: X360+PS3 versions
	{
		//render targets can't be locked, luckily we can copy the surface to system memory and lock that.
		IDirect3DSurface *pSystemSurface;

		Assert( !IsX360() );

		hr = Dx9Device()->CreateOffscreenPlainSurface( surfaceDesc.Width, surfaceDesc.Height, surfaceDesc.Format, D3DPOOL_SYSTEMMEM, &pSystemSurface, NULL );
		Assert( SUCCEEDED( hr ) );

		pSystemSurface->GetDesc( &surfaceDesc );

		hr = Dx9Device()->GetRenderTargetData( pTextureLevel, pSystemSurface );
		Assert( SUCCEEDED( hr ) );

		//pretend this is the texture level we originally grabbed with GetSurfaceLevel() and continue on
		pTextureLevel->Release();
		pTextureLevel = pSystemSurface;
	}
#endif

	// lock the region
	if ( FAILED( pTextureLevel->LockRect( &lockedRect, NULL, D3DLOCK_READONLY ) ) )
	{
		Assert( 0 );
		pTextureLevel->Release();
		return;
	}

	TGAWriter::WriteTGAFile( szFileName, surfaceDesc.Width, surfaceDesc.Height, pTexInt->GetImageFormat(), (const uint8 *)lockedRect.pBits, lockedRect.Pitch );

	if ( FAILED( pTextureLevel->UnlockRect() ) ) 
	{
		Assert( 0 );
		pTextureLevel->Release();
		return;
	}

	pTextureLevel->Release();
}


//-----------------------------------------------------------------------------
// Releases all textures
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReleaseAllTextures()
{
	ClearStdTextureHandles();
	ShaderAPITextureHandle_t hTexture;
	for ( hTexture = m_Textures.Head(); hTexture != m_Textures.InvalidIndex(); hTexture = m_Textures.Next( hTexture ) )
	{
		if ( TextureIsAllocated( hTexture ) )
		{
			// Delete it baby
			DeleteD3DTexture( hTexture );
		}
	}

	// Make sure all texture units are pointing to nothing
	for (int unit = 0; unit < g_pHardwareConfig->GetSamplerCount(); ++unit )
	{
		SamplerState( unit ).m_BoundTexture = INVALID_SHADERAPI_TEXTURE_HANDLE;
		SetTextureState( (Sampler_t)unit, TEXTURE_BINDFLAGS_NONE, INVALID_SHADERAPI_TEXTURE_HANDLE );
	}
}

void CShaderAPIDx8::DeleteAllTextures()
{
	ReleaseAllTextures();
	m_Textures.Purge();
}

bool CShaderAPIDx8::IsTexture( ShaderAPITextureHandle_t textureHandle )
{
	LOCK_SHADERAPI();

	if ( !TextureIsAllocated( textureHandle ) )
	{
		return false;
	}

#if !defined( _X360 )
	if ( GetTexture( textureHandle ).m_Flags & Texture_t::IS_DEPTH_STENCIL )
	{
		return GetTexture( textureHandle ).GetDepthStencilSurface() != 0;
	}
	else if ( ( GetTexture( textureHandle ).m_NumCopies == 1 && GetTexture( textureHandle ).GetTexture() != 0 ) ||
				( GetTexture( textureHandle ).m_NumCopies > 1 && GetTexture( textureHandle ).GetTexture( 0 ) != 0 ) )
	{
		return true;
	}
	else
	{
		return false;
	}
#else
	// query is about texture handle validity, not presence
	// texture handle is allocated, texture may or may not be present
	return true;
#endif
}

//-----------------------------------------------------------------------------
// Gets the surface associated with a texture (refcount of surface is increased)
//-----------------------------------------------------------------------------
IDirect3DSurface* CShaderAPIDx8::GetTextureSurface( ShaderAPITextureHandle_t textureHandle )
{
	MEM_ALLOC_D3D_CREDIT();

	IDirect3DSurface* pSurface;

	// We'll be modifying this sucka
	AssertValidTextureHandle( textureHandle );

	//APSFIXME
	//tmauer
	//right now the 360 is getting invalid texture hadles here
	// <sergiy> 5/20/2015: There's significant number of crashes in Windows NT - Crash Reports for CShaderAPIDx8::ReadPixelsAsync(int, int, int, int, unsigned char*, ImageFormat, ITexture*, CThreadEvent*)
	//                     due to this handle being invalid. It may have to do with losing D3D device. Robustifying this code to check if the crashes will end.
	if ( !m_Textures.IsValidIndex( textureHandle ) )
	{
		return NULL;
	}

	Texture_t &tex = GetTexture( textureHandle );
	if ( !( tex.m_Flags & Texture_t::IS_ALLOCATED ) )
	{
		return NULL;
	}

	if ( IsX360() && ( tex.m_Flags & Texture_t::IS_RENDER_TARGET_SURFACE ) )
	{
		pSurface = tex.GetRenderTargetSurface( false );
		pSurface->AddRef();
		return pSurface;
	}

	IDirect3DBaseTexture* pD3DTex = CShaderAPIDx8::GetD3DTexture( textureHandle );
	IDirect3DTexture* pTex = static_cast<IDirect3DTexture*>( pD3DTex );
	Assert( pTex );
	if ( !pTex )
	{
		return NULL;
	}
	
	HRESULT hr = pTex->GetSurfaceLevel( 0, &pSurface );
	Assert( hr == D3D_OK );

	return pSurface;
}

//-----------------------------------------------------------------------------
// Gets the surface associated with a texture (refcount of surface is increased)
//-----------------------------------------------------------------------------
IDirect3DSurface* CShaderAPIDx8::GetDepthTextureSurface( ShaderAPITextureHandle_t textureHandle )
{
	AssertValidTextureHandle( textureHandle );
	if ( !TextureIsAllocated( textureHandle ) )
	{
		return NULL;
	}
	return GetTexture( textureHandle ).GetDepthStencilSurface();
}

//-----------------------------------------------------------------------------
// Changes the render target
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle )
{
	LOCK_SHADERAPI();
	if ( IsDeactivated( ) )
	{
		return;
	}

#if defined( PIX_INSTRUMENTATION )
	{
		const char *pRT = "Backbuffer";
		const char *pDS = "DefaultDepthStencil";
		
		if ( colorTextureHandle == SHADER_RENDERTARGET_NONE )
		{
			pRT = "None";
		}
		else if ( colorTextureHandle != SHADER_RENDERTARGET_BACKBUFFER )
		{
			Texture_t &tex = GetTexture( colorTextureHandle );
			pRT = tex.m_DebugName.String();
		}
		
		if ( depthTextureHandle == SHADER_RENDERTARGET_NONE )
		{
			pDS = "None";
		}
		else if ( depthTextureHandle != SHADER_RENDERTARGET_DEPTHBUFFER )
		{
			Texture_t &tex = GetTexture( depthTextureHandle );
			pDS = tex.m_DebugName.String();
		}

		char buf[256];
		sprintf( buf, "SRT:%s %s", pRT ? pRT : "?", pDS ? pDS : "?" );
		BeginPIXEvent( 0xFFFFFFFF, buf );
		EndPIXEvent();
	}
#endif

#if !defined( _X360 )
	RECORD_COMMAND( DX8_TEST_COOPERATIVE_LEVEL, 0 );
	HRESULT hr = Dx9Device()->TestCooperativeLevel();
	if ( hr != D3D_OK )
	{
		MarkDeviceLost();
		return;
	}
#endif

	IDirect3DSurface* pColorSurface = NULL;
	IDirect3DSurface* pZSurface = NULL;

	RECORD_COMMAND( DX8_SET_RENDER_TARGET, 3 );
	RECORD_INT( nRenderTargetID );
	RECORD_INT( colorTextureHandle );
	RECORD_INT( depthTextureHandle );

	int backBufferIdx = GetIntRenderingParameter( INT_RENDERPARM_BACK_BUFFER_INDEX );

	// The 0th render target defines which depth buffer we are using, so 
	// don't bother if we are another render target
	if ( nRenderTargetID > 0 )
	{
		depthTextureHandle = SHADER_RENDERTARGET_NONE;
	}

	// NOTE!!!!  If this code changes, also change Dx8SetRenderTarget in playback.cpp
	bool usingTextureTarget = false;
	if ( colorTextureHandle == SHADER_RENDERTARGET_BACKBUFFER )
	{
		pColorSurface = m_pBackBufferSurfaces[ backBufferIdx ];

		// This is just to make the code a little simpler...
		// (simplifies the release logic)
		pColorSurface->AddRef();
	}
	else
	{
		// get the texture (Refcount increases)
		UnbindTexture( colorTextureHandle );
		pColorSurface = GetTextureSurface( colorTextureHandle );
		if ( !pColorSurface )
		{
			return;
		}

		usingTextureTarget = true;
	}

	bool bGetSurfaceLevelZSurface = false;
	if ( depthTextureHandle == SHADER_RENDERTARGET_DEPTHBUFFER )
	{
		// using the default depth buffer
		pZSurface = m_pZBufferSurface;

		// simplify the prologue logic
		pZSurface->AddRef();
		SPEW_REFCOUNT( pZSurface );
	}
	else if ( depthTextureHandle == SHADER_RENDERTARGET_NONE )
	{
		// GR - disable depth buffer
		pZSurface = NULL;
	}
	else
	{
		UnbindTexture( depthTextureHandle );

		Texture_t &tex = GetTexture( depthTextureHandle );

		//Cannot use a depth/stencil surface derived from a texture. 
		//Asserting helps get the whole call stack instead of letting the 360 report an error with a partial stack
		Assert( !( IsX360() && (tex.m_Flags & Texture_t::IS_DEPTH_STENCIL_TEXTURE) ) );

		if ( tex.m_Flags & Texture_t::IS_DEPTH_STENCIL )
		{
			pZSurface = GetDepthTextureSurface( depthTextureHandle );
			if ( pZSurface )
			{	
				pZSurface->AddRef();
				SPEW_REFCOUNT( pZSurface );
			}
		}
		else
		{	
			IDirect3DTexture9 *pD3DTexture = (IDirect3DTexture9*)tex.GetTexture();
			if(pD3DTexture)
			{
				HRESULT hr = pD3DTexture->GetSurfaceLevel( 0, &pZSurface );
				SPEW_REFCOUNT( pZSurface );

				bGetSurfaceLevelZSurface = true;
			}
			else
			{
				pZSurface = NULL;
				Warning("Unexpected NULL texture in CShaderAPIDx8::SetRenderTargetEx()\n");
			}
		}

		if ( !pZSurface )
		{
			// Refcount of color surface was increased above
			pColorSurface->Release();
			return;
		}
		usingTextureTarget = true;
	}

#ifdef _DEBUG
	if ( pZSurface )
	{
		D3DSURFACE_DESC zSurfaceDesc, colorSurfaceDesc;
		pZSurface->GetDesc( &zSurfaceDesc );
		pColorSurface->GetDesc( &colorSurfaceDesc );

		Assert( colorSurfaceDesc.Width <= zSurfaceDesc.Width );
		Assert( colorSurfaceDesc.Height <= zSurfaceDesc.Height );
	}
#endif
	
	// NOTE: The documentation says that SetRenderTarget increases the refcount
	// but it doesn't appear to in practice. If this somehow changes (perhaps
	// in a device-specific manner, we're in trouble).
	bool bSetValidRenderTarget = false;
	if ( IsPC() || !IsX360() )
	{
		if ( pColorSurface == m_pBackBufferSurfaces[backBufferIdx] && nRenderTargetID > 0 )
		{
			// SetRenderTargetEx is overloaded so that if you pass NULL in for anything that
			// isn't the zeroth render target, you effectively disable that MRT index.
			// (Passing in NULL for the zeroth render target means that you want to use the backbuffer
			// as the render target.)
			// hack hack hack!!!!!  If the render target id > 0 and the user passed in NULL, disable the render target
			Dx9Device()->SetRenderTarget( nRenderTargetID, NULL );
		}
		else
		{
			Dx9Device()->SetRenderTarget( nRenderTargetID, pColorSurface );
			
			bSetValidRenderTarget = true;
		}
	}
	else
	{
		Assert( nRenderTargetID == 0 );
		SetRenderTargetInternalXbox( colorTextureHandle );
		
		bSetValidRenderTarget = true;
	}

	// The 0th render target defines which depth buffer we are using, so 
	// don't bother if we are another render target
	if ( nRenderTargetID == 0 )
	{
		SPEW_REFCOUNT( pZSurface );
		Dx9Device()->SetDepthStencilSurface( pZSurface );
		SPEW_REFCOUNT( pZSurface );
	}

	// Only record if we're rendering to a texture or not (and its dimensions) when we actually set a valid render target (or depth-stencil surface).
	// If nRenderTargetID==0, we're always setting at least the depth-stencil surface.
	// Otherwise, only execute this logic when we are actually setting a valid surface.
	if ( ( !nRenderTargetID ) || ( bSetValidRenderTarget ) )
	{
		m_UsingTextureRenderTarget = usingTextureTarget;

		if ( m_UsingTextureRenderTarget )
		{
			D3DSURFACE_DESC  desc;
			HRESULT hr;
			if ( !pZSurface )
			{
				hr = pColorSurface->GetDesc( &desc );
			}
			else
			{
				hr = pZSurface->GetDesc( &desc );
			}
			Assert( !FAILED(hr) );
			m_ViewportMaxWidth = desc.Width;
			m_ViewportMaxHeight = desc.Height;
		}
	}

	int ref;
	if ( pZSurface )
	{
		SPEW_REFCOUNT( pZSurface );
		ref = pZSurface->Release();
		SPEW_REFCOUNT( pZSurface );
		Assert( bGetSurfaceLevelZSurface || ref != 0 );
	}

	ref = pColorSurface->Release();
//	Assert( ref != 0 );

	// Changing the render target sets a default viewport. 
	// Force an update but preserve the current desired state.
	m_DynamicState.m_Viewport.X = 0;
	m_DynamicState.m_Viewport.Y = 0;
	m_DynamicState.m_Viewport.Width = (DWORD)-1;
	m_DynamicState.m_Viewport.Height = (DWORD)-1;
	ADD_COMMIT_FUNC( COMMIT_PER_DRAW, CommitSetViewports );
}


//-----------------------------------------------------------------------------
// Changes the render target
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle )
{
	LOCK_SHADERAPI();
	SetRenderTargetEx( 0, colorTextureHandle, depthTextureHandle );
}

//-----------------------------------------------------------------------------
// Returns the nearest supported format
//-----------------------------------------------------------------------------
ImageFormat CShaderAPIDx8::GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired /* = true */ ) const
{
	return ImageLoader::D3DFormatToImageFormat( FindNearestSupportedFormat( fmt, false, false, bFilteringRequired ) );
}


ImageFormat CShaderAPIDx8::GetNearestRenderTargetFormat( ImageFormat fmt ) const
{
	return ImageLoader::D3DFormatToImageFormat( FindNearestSupportedFormat( fmt, false, true, false ) );
}


bool CShaderAPIDx8::DoRenderTargetsNeedSeparateDepthBuffer() const
{
	LOCK_SHADERAPI();
	return m_PresentParameters.MultiSampleType != D3DMULTISAMPLE_NONE;
}


//-----------------------------------------------------------------------------
// Indicates we're modifying a texture
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ModifyTexture( ShaderAPITextureHandle_t textureHandle )
{
	LOCK_SHADERAPI();
	// Can't do this if we're locked!
	Assert( m_ModifyTextureLockedLevel < 0 );

	AssertValidTextureHandle( textureHandle );
	m_ModifyTextureHandle = textureHandle;
	
	// If we're got a multi-copy texture, we need to up the current copy count
	Texture_t& tex = GetTexture( textureHandle );
	if (tex.m_NumCopies > 1)
	{
		// Each time we modify a texture, we'll want to switch texture
		// as soon as a TexImage2D call is made...
		tex.m_SwitchNeeded = true;
	}
}


//-----------------------------------------------------------------------------
// Advances the current copy of a texture...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::AdvanceCurrentCopy( ShaderAPITextureHandle_t hTexture )
{
	// May need to switch textures....
	Texture_t& tex = GetTexture( hTexture );
	if (tex.m_NumCopies > 1)
	{
		if (++tex.m_CurrentCopy >= tex.m_NumCopies)
			tex.m_CurrentCopy = 0;

		// When the current copy changes, we need to make sure this texture
		// isn't bound to any stages any more; thereby guaranteeing the new
		// copy will be re-bound.
		UnbindTexture( hTexture );
	}
}


//-----------------------------------------------------------------------------
// Locks, unlocks the current texture
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
								int width, int height, CPixelWriter& writer )
{
	LOCK_SHADERAPI();

	Assert( m_ModifyTextureLockedLevel < 0 );

	ShaderAPITextureHandle_t hTexture = GetModifyTextureHandle();
	if ( !m_Textures.IsValidIndex( hTexture ) )
		return false;

	// This test here just makes sure we don't try to download mipmap levels
	// if we weren't able to create them in the first place
	Texture_t& tex = GetTexture( hTexture );
	if ( level >= tex.m_NumLevels  )
		return false;

	// May need to switch textures....
	if ( tex.m_SwitchNeeded )
	{
		AdvanceCurrentCopy( hTexture );
		tex.m_SwitchNeeded = false;
	}

	IDirect3DBaseTexture *pTexture = GetModifyTexture();
#if defined( _X360 )
	// 360 can't lock a bound texture
	if ( pTexture && pTexture->IsSet( Dx9Device() ) )
	{
		UnbindTexture( hTexture );
	}
#endif

	bool bOK = LockTexture( hTexture, tex.m_CurrentCopy, pTexture,
		level, (D3DCUBEMAP_FACES)cubeFaceID, xOffset, yOffset, width, height, false, writer );
	if ( bOK )
	{
		m_ModifyTextureLockedLevel = level;
		m_ModifyTextureLockedFace = cubeFaceID;
	}
	return bOK;
}

void CShaderAPIDx8::TexUnlock( )
{
	LOCK_SHADERAPI();
	if ( m_ModifyTextureLockedLevel >= 0 )
	{
		Texture_t& tex = GetTexture( GetModifyTextureHandle() );
		UnlockTexture( GetModifyTextureHandle(), tex.m_CurrentCopy, GetModifyTexture(),
			m_ModifyTextureLockedLevel, (D3DCUBEMAP_FACES)(int)m_ModifyTextureLockedFace );

		m_ModifyTextureLockedLevel = -1;
	}
}

//-----------------------------------------------------------------------------
// Texture image upload
//-----------------------------------------------------------------------------
void CShaderAPIDx8::TexImage2D( 
	int			level,
	int			cubeFaceID, 
	ImageFormat	dstFormat, 
	int			z, 
	int			width, 
	int			height, 
	ImageFormat srcFormat, 
	bool		bSrcIsTiled,
	void		*pSrcData )
{
	LOCK_SHADERAPI();

	Assert( pSrcData );
	AssertValidTextureHandle( GetModifyTextureHandle() );

	if ( !m_Textures.IsValidIndex( GetModifyTextureHandle() ) )
		return;

	Assert( (width <= g_pHardwareConfig->Caps().m_MaxTextureWidth) && (height <= g_pHardwareConfig->Caps().m_MaxTextureHeight) );

	// This test here just makes sure we don't try to download mipmap levels
	// if we weren't able to create them in the first place
	Texture_t& tex = GetTexture( GetModifyTextureHandle() );
	if ( level >= tex.m_NumLevels )
		return;

	// May need to switch textures....
	if (tex.m_SwitchNeeded)
	{
		AdvanceCurrentCopy( GetModifyTextureHandle() );
		tex.m_SwitchNeeded = false;
	}

	TextureLoadInfo_t info;
	info.m_TextureHandle = GetModifyTextureHandle();
	info.m_pTexture = GetModifyTexture();
	info.m_nLevel = level;
	info.m_nCopy = tex.m_CurrentCopy;
	info.m_CubeFaceID = (D3DCUBEMAP_FACES)cubeFaceID;
	info.m_nWidth = width;
	info.m_nHeight = height;
	info.m_nZOffset = z;
	info.m_SrcFormat = srcFormat;
	info.m_pSrcData = (unsigned char *)pSrcData;
#if defined( _X360 )
	info.m_bSrcIsTiled = bSrcIsTiled;
	info.m_bCanConvertFormat = ( tex.m_Flags & Texture_t::CAN_CONVERT_FORMAT ) != 0;
#endif
	LoadTexture( info );
	SetModifyTexture( info.m_pTexture );
}

//-----------------------------------------------------------------------------
// Upload to a sub-piece of a texture
//-----------------------------------------------------------------------------
void CShaderAPIDx8::TexSubImage2D( 
	int			level, 
	int			cubeFaceID,
	int			xOffset,
	int			yOffset, 
	int			zOffset,
	int			width,
	int			height,
	ImageFormat srcFormat,
	int			srcStride,
	bool		bSrcIsTiled,
	void		*pSrcData )
{
	LOCK_SHADERAPI();

	Assert( pSrcData );
	AssertValidTextureHandle( GetModifyTextureHandle() );

	if ( !m_Textures.IsValidIndex( GetModifyTextureHandle() ) )
		return;

	// NOTE: This can only be done with procedural textures if this method is
	// being used to download the entire texture, cause last frame's partial update
	// may be in a completely different texture! Sadly, I don't have all of the
	// information I need, but I can at least check a couple things....
#ifdef _DEBUG
	if ( GetTexture( GetModifyTextureHandle() ).m_NumCopies > 1 )
	{
		Assert( (xOffset == 0) && (yOffset == 0) );
	}
#endif

	// This test here just makes sure we don't try to download mipmap levels
	// if we weren't able to create them in the first place
	Texture_t& tex = GetTexture( GetModifyTextureHandle() );
	if ( level >= tex.m_NumLevels )
	{
		return;
	}

	// May need to switch textures....
	if ( tex.m_SwitchNeeded )
	{
		AdvanceCurrentCopy( GetModifyTextureHandle() );
		tex.m_SwitchNeeded = false;
	}

	TextureLoadInfo_t info;
	info.m_TextureHandle = GetModifyTextureHandle();
	info.m_pTexture = GetModifyTexture();
	info.m_nLevel = level;
	info.m_nCopy = tex.m_CurrentCopy;
	info.m_CubeFaceID = (D3DCUBEMAP_FACES)cubeFaceID;
	info.m_nWidth = width;
	info.m_nHeight = height;
	info.m_nZOffset = zOffset;
	info.m_SrcFormat = srcFormat;
	info.m_pSrcData = (unsigned char *)pSrcData;
#if defined( _X360 )
	info.m_bSrcIsTiled = bSrcIsTiled;
	info.m_bCanConvertFormat = ( tex.m_Flags & Texture_t::CAN_CONVERT_FORMAT ) != 0;
#endif
	LoadSubTexture( info, xOffset, yOffset, srcStride );
}


//-----------------------------------------------------------------------------
// Upload to a default pool texture from a sysmem texture
//-----------------------------------------------------------------------------
void CShaderAPIDx8::UpdateTexture( int xOffset, int yOffset, int w, int h, ShaderAPITextureHandle_t hDstTexture, ShaderAPITextureHandle_t hSrcTexture )
{
	Assert( IsPC() );

	LOCK_SHADERAPI();

	AssertValidTextureHandle( hDstTexture );
	if ( !m_Textures.IsValidIndex( hDstTexture ) )
		return;

	AssertValidTextureHandle( hSrcTexture );
	if ( !m_Textures.IsValidIndex( hSrcTexture ) )
		return;

	IDirect3DTexture *pDstTexture = (IDirect3DTexture *) g_ShaderAPIDX8.GetTexture( hDstTexture ).GetTexture();
	IDirect3DSurface9 *pDstSurface;
	HRESULT hr = pDstTexture->GetSurfaceLevel( 0, &pDstSurface );
	Assert( !FAILED( hr ) );

	IDirect3DTexture *pSrcTexture = (IDirect3DTexture *) g_ShaderAPIDX8.GetTexture( hSrcTexture ).GetTexture();
	IDirect3DSurface9 *pSrcSurface;
	hr = pSrcTexture->GetSurfaceLevel( 0, &pSrcSurface );
	Assert( !FAILED( hr ) );

	RECT srcRect = { xOffset, yOffset, w, h };
	POINT dstPoint = { 0, 0 };
#if ( defined( _X360 ) || defined( DX_TO_GL_ABSTRACTION ) )
	AssertMsg( false, "Not supported on Xbox 360 or Posix." );
#else
	hr = Dx9Device()->UpdateSurface( pSrcSurface, &srcRect, pDstSurface, &dstPoint );
	Assert( !FAILED( hr ) );
#endif

	pDstSurface->Release();	// The GetSurfaceLevel calls incremented ref count
	pSrcSurface->Release();
}

void *CShaderAPIDx8::LockTex( ShaderAPITextureHandle_t hTexture )
{
	IDirect3DTexture *pTex = (IDirect3DTexture *) g_ShaderAPIDX8.GetTexture( hTexture ).GetTexture();
	IDirect3DSurface9 *pSurf;
	HRESULT hr = pTex->GetSurfaceLevel( 0, &pSurf );
	Assert( !FAILED( hr ) );

	D3DLOCKED_RECT lockedRect;
	hr = pSurf->LockRect( &lockedRect, NULL, D3DLOCK_NO_DIRTY_UPDATE );
	Assert( !FAILED( hr ) );

	pSurf->Release();	// The GetSurfaceLevel call incremented ref count

	return lockedRect.pBits;
}

void CShaderAPIDx8::UnlockTex( ShaderAPITextureHandle_t hTexture )
{
	IDirect3DTexture *pTex = (IDirect3DTexture *) g_ShaderAPIDX8.GetTexture( hTexture ).GetTexture();
	IDirect3DSurface9 *pSurf;
	HRESULT hr = pTex->GetSurfaceLevel( 0, &pSurf );
	Assert( !FAILED( hr ) );

	hr = pSurf->UnlockRect();
	Assert( !FAILED( hr ) );

	pSurf->Release();	// The GetSurfaceLevel call incremented ref count
}




//-----------------------------------------------------------------------------
// Is the texture resident?
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsTextureResident( ShaderAPITextureHandle_t textureHandle )
{
	return true;
}


//-----------------------------------------------------------------------------
// Level of anisotropic filtering
//-----------------------------------------------------------------------------
ConVar mat_aniso_disable( "mat_aniso_disable", "0", FCVAR_CHEAT, "NOTE: You must change mat_forceaniso after changing this convar for this to take effect" );
void CShaderAPIDx8::SetAnisotropicLevel( int nAnisotropyLevel )
{
	LOCK_SHADERAPI();

	// NOTE: This must be called before the rest of the code in this function so
	//       anisotropic can be set per-texture to force it on! This will also avoid
	//       a possible infinite loop that existed before.
	g_pShaderUtil->NoteAnisotropicLevel( nAnisotropyLevel );

	// Never set this to 1. In the case we want it set to 1, we will use this to override
	//   aniso per-texture, so set it to something reasonable.
	if ( mat_aniso_disable.GetInt() == 1 )
	{
		// Don't allow any aniso at all
		nAnisotropyLevel = 1;
	}
	else
	{
		int nAnisotropyLevelOverride = 1;
		if ( IsGameConsole() )
		{
			nAnisotropyLevelOverride = 4;
		}
		else
		{
			// Set it to 1/2 the max but at least 2
			nAnisotropyLevelOverride = MAX( 2, g_pHardwareConfig->Caps().m_nMaxAnisotropy / 2 );
		}

		// Use the larger value
		nAnisotropyLevel = MAX( nAnisotropyLevel, nAnisotropyLevelOverride );
	}

	// Make sure the value is in range
	nAnisotropyLevel = MIN( nAnisotropyLevel, g_pHardwareConfig->Caps().m_nMaxAnisotropy );

	// Set the D3D max anisotropy state for all samplers
	for ( int i = 0; i < g_pHardwareConfig->Caps().m_NumSamplers; ++i)
	{
		SamplerState(i).m_nAnisotropicLevel = nAnisotropyLevel;
		SetSamplerState( i, D3DSAMP_MAXANISOTROPY, SamplerState(i).m_nAnisotropicLevel );
	}
}


//-----------------------------------------------------------------------------
// Sets the priority
//-----------------------------------------------------------------------------
void CShaderAPIDx8::TexSetPriority( int priority )
{
#if !defined( _X360 )
	LOCK_SHADERAPI();

	// A hint to the cacher...
	ShaderAPITextureHandle_t hModifyTexture = GetModifyTextureHandle();
	if ( hModifyTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;

	Texture_t& tex = GetTexture( hModifyTexture );
	if ( tex.m_NumCopies > 1 )
	{
		for (int i = 0; i < tex.m_NumCopies; ++i)
			tex.GetTexture( i )->SetPriority( priority );
	}
	else if ( tex.GetTexture() ) // null check
	{
		tex.GetTexture()->SetPriority( priority );
	}
#endif
}


//-----------------------------------------------------------------------------
// Texturemapping state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode )
{
	LOCK_SHADERAPI();

	ShaderAPITextureHandle_t hModifyTexture = GetModifyTextureHandle();
	if ( hModifyTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;

	D3DTEXTUREADDRESS address;
	switch( wrapMode )
	{
	case SHADER_TEXWRAPMODE_CLAMP:
		address = D3DTADDRESS_CLAMP;
		break;
	case SHADER_TEXWRAPMODE_REPEAT:
		address = D3DTADDRESS_WRAP;
		break; 
	case SHADER_TEXWRAPMODE_BORDER:
		address = D3DTADDRESS_BORDER;
		break; 
	default:
		address = D3DTADDRESS_CLAMP;
		Warning( "CShaderAPIDx8::TexWrap: unknown wrapMode\n" );
		break;
	}

	switch( coord )
	{
	case SHADER_TEXCOORD_S:
		GetTexture( hModifyTexture ).m_UTexWrap = address;
		break;
	case SHADER_TEXCOORD_T:
		GetTexture( hModifyTexture ).m_VTexWrap = address;
		break;
	case SHADER_TEXCOORD_U:
		GetTexture( hModifyTexture ).m_WTexWrap = address;
		break;
	default:
		Warning( "CShaderAPIDx8::TexWrap: unknown coord\n" );
		break;
	}
}

void CShaderAPIDx8::TexMinFilter( ShaderTexFilterMode_t texFilterMode )
{
	LOCK_SHADERAPI();

	ShaderAPITextureHandle_t hModifyTexture = GetModifyTextureHandle();
	if ( hModifyTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;

	switch( texFilterMode )
	{
	case SHADER_TEXFILTERMODE_NEAREST:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_POINT;
		GetTexture( hModifyTexture ).m_MipFilter = D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_LINEAR:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_LINEAR;
		GetTexture( hModifyTexture ).m_MipFilter = D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_NEAREST_MIPMAP_NEAREST:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_POINT;
		GetTexture( hModifyTexture ).m_MipFilter = 
		GetTexture( hModifyTexture ).m_NumLevels != 1 ? D3DTEXF_POINT : D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_LINEAR_MIPMAP_NEAREST:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_LINEAR;
		GetTexture( hModifyTexture ).m_MipFilter = 
		GetTexture( hModifyTexture ).m_NumLevels != 1 ? D3DTEXF_POINT : D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_NEAREST_MIPMAP_LINEAR:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_POINT;
		GetTexture( hModifyTexture ).m_MipFilter = 
		GetTexture( hModifyTexture ).m_NumLevels != 1 ? D3DTEXF_LINEAR : D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_LINEAR_MIPMAP_LINEAR:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_LINEAR;
		GetTexture( hModifyTexture ).m_MipFilter = 
		GetTexture( hModifyTexture ).m_NumLevels != 1 ? D3DTEXF_LINEAR : D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_ANISOTROPIC:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_ANISOTROPIC;
		GetTexture( hModifyTexture ).m_MipFilter = 
		GetTexture( hModifyTexture ).m_NumLevels != 1 ? D3DTEXF_LINEAR : D3DTEXF_NONE;
		break;
	default:
		Warning( "CShaderAPIDx8::TexMinFilter: Unknown texFilterMode\n" );
		break;
	}
}

void CShaderAPIDx8::TexMagFilter( ShaderTexFilterMode_t texFilterMode )
{
	LOCK_SHADERAPI();

	ShaderAPITextureHandle_t hModifyTexture = GetModifyTextureHandle();
	if ( hModifyTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;

	switch( texFilterMode )
	{
	case SHADER_TEXFILTERMODE_NEAREST:
		GetTexture( hModifyTexture ).m_MagFilter = D3DTEXF_POINT;
		break;
	case SHADER_TEXFILTERMODE_LINEAR:
		GetTexture( hModifyTexture ).m_MagFilter = D3DTEXF_LINEAR;
		break;
	case SHADER_TEXFILTERMODE_NEAREST_MIPMAP_NEAREST:
		Warning( "CShaderAPIDx8::TexMagFilter: SHADER_TEXFILTERMODE_NEAREST_MIPMAP_NEAREST is invalid\n" );
		break;
	case SHADER_TEXFILTERMODE_LINEAR_MIPMAP_NEAREST:
		Warning( "CShaderAPIDx8::TexMagFilter: SHADER_TEXFILTERMODE_LINEAR_MIPMAP_NEAREST is invalid\n" );
		break;
	case SHADER_TEXFILTERMODE_NEAREST_MIPMAP_LINEAR:
		Warning( "CShaderAPIDx8::TexMagFilter: SHADER_TEXFILTERMODE_NEAREST_MIPMAP_LINEAR is invalid\n" );
		break;
	case SHADER_TEXFILTERMODE_LINEAR_MIPMAP_LINEAR:
		Warning( "CShaderAPIDx8::TexMagFilter: SHADER_TEXFILTERMODE_LINEAR_MIPMAP_LINEAR is invalid\n" );
		break;
	case SHADER_TEXFILTERMODE_ANISOTROPIC:
		GetTexture( hModifyTexture ).m_MagFilter = g_pHardwareConfig->Caps().m_bSupportsMagAnisotropicFiltering ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR;
		break;
	default:
		Warning( "CShaderAPIDx8::TexMAGFilter: Unknown texFilterMode\n" );
		break;
	}
}


//-----------------------------------------------------------------------------
// Gets the matrix stack from the matrix mode
//-----------------------------------------------------------------------------

int CShaderAPIDx8::GetMatrixStack( MaterialMatrixMode_t mode ) const
{
	Assert( mode >= 0 && mode < NUM_MATRIX_MODES );
	return mode;
}


//-----------------------------------------------------------------------------
// lighting related methods
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetLightingState( const MaterialLightingState_t& state )
{
	bool bAmbientChanged = Q_memcmp( m_DynamicState.m_LightingState.m_vecAmbientCube, state.m_vecAmbientCube, sizeof(state.m_vecAmbientCube) ) != 0;
	bool bLightsChanged = ( state.m_nLocalLightCount != 0 ) || ( m_DynamicState.m_LightingState.m_nLocalLightCount != 0 );
	if ( !bAmbientChanged && !bLightsChanged )
		return;

	m_DynamicState.m_LightingState = state;
	if ( bAmbientChanged )
	{
		m_DynamicState.m_InstanceInfo.m_bAmbientCubeCompiled = false;
	}
	if ( bLightsChanged )
	{
		m_DynamicState.m_InstanceInfo.m_bPixelShaderLocalLightsCompiled = false;
		m_DynamicState.m_InstanceInfo.m_bVertexShaderLocalLightsCompiled = false;
		m_DynamicState.m_InstanceInfo.m_bSetLightVertexShaderConstants = false;

		//#ifdef _DEBUG
		for ( int i = 0; i < m_DynamicState.m_LightingState.m_nLocalLightCount; ++i )
		{
			// NOTE: This means we haven't set the light up correctly
			if ( ( m_DynamicState.m_LightingState.m_pLocalLightDesc[i].m_Flags & LIGHTTYPE_OPTIMIZATIONFLAGS_DERIVED_VALUES_CALCED ) == 0 )
			{
				Warning( "Client code forgot to call LightDesc_t::RecalculateDerivedValues\n" );
				m_DynamicState.m_LightingState.m_pLocalLightDesc[i].RecalculateDerivedValues();
			}

			Assert( m_DynamicState.m_LightingState.m_pLocalLightDesc[i].m_Type != MATERIAL_LIGHT_DISABLE );
		}
		//#endif
	}
}


void CShaderAPIDx8::SetLightingOrigin( Vector vLightingOrigin )
{
	m_DynamicState.m_LightingState.m_vecLightingOrigin = vLightingOrigin;
}

//#define NO_LOCAL_LIGHTS
void CShaderAPIDx8::SetLights( int nCount, const LightDesc_t* pDesc )
{
	LOCK_SHADERAPI();
#ifdef NO_LOCAL_LIGHTS
	nCount = 0;
#endif

	int nMaxLight = MIN( g_pHardwareConfig->Caps().m_MaxNumLights, MATERIAL_MAX_LIGHT_COUNT );
	nCount = MIN( nMaxLight, nCount );

	m_DynamicState.m_LightingState.m_nLocalLightCount = nCount;
	if ( nCount > 0 )
	{
		memcpy( m_DynamicState.m_LightingState.m_pLocalLightDesc, pDesc, nCount * sizeof(LightDesc_t) );

#ifdef _DEBUG
		for ( int i = 0; i < nCount; ++i )
		{
			Assert( m_DynamicState.m_LightingState.m_pLocalLightDesc[i].m_Type != MATERIAL_LIGHT_DISABLE );
		}
#endif
	}
	m_DynamicState.m_InstanceInfo.m_bPixelShaderLocalLightsCompiled = false;
	m_DynamicState.m_InstanceInfo.m_bVertexShaderLocalLightsCompiled = false;
	m_DynamicState.m_InstanceInfo.m_bSetLightVertexShaderConstants = false;
}

void CShaderAPIDx8::DisableAllLocalLights()
{
	LOCK_SHADERAPI();
	if ( m_DynamicState.m_LightingState.m_nLocalLightCount != 0 )
	{
		m_DynamicState.m_LightingState.m_nLocalLightCount = 0;
		m_DynamicState.m_InstanceInfo.m_bPixelShaderLocalLightsCompiled = false;
		m_DynamicState.m_InstanceInfo.m_bVertexShaderLocalLightsCompiled = false;
		m_DynamicState.m_InstanceInfo.m_bSetLightVertexShaderConstants = false;
	}
}

//-----------------------------------------------------------------------------
// Ambient cube 
//-----------------------------------------------------------------------------

//#define NO_AMBIENT_CUBE 1
void CShaderAPIDx8::SetAmbientLightCube( Vector4D cube[6] )
{
	LOCK_SHADERAPI();
/*
	int i;
	for( i = 0; i < 6; i++ )
	{
		ColorClamp( cube[i].AsVector3D() );
//		if( i == 0 )
//		{
//			Warning( "%d: %f %f %f\n", i, cube[i][0], cube[i][1], cube[i][2] );
//		}
	}
*/
	if ( memcmp(&m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[0][0], cube, 6 * sizeof(Vector4D)) )
	{
		memcpy( &m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[0][0], cube, 6 * sizeof(Vector4D) );

#ifdef NO_AMBIENT_CUBE
		memset( &m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[0][0], 0, 6 * sizeof(Vector4D) );
#endif

//#define DEBUG_AMBIENT_CUBE

#ifdef DEBUG_AMBIENT_CUBE
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[0][0] = 1.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[0][1] = 0.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[0][2] = 0.0f;

		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[1][0] = 0.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[1][1] = 1.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[1][2] = 0.0f;

		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[2][0] = 0.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[2][1] = 0.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[2][2] = 1.0f;

		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[3][0] = 1.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[3][1] = 0.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[3][2] = 1.0f;

		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[4][0] = 1.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[4][1] = 1.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[4][2] = 0.0f;

		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[5][0] = 0.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[5][1] = 1.0f;
		m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[5][2] = 1.0f;
#endif

		// Copy into other state
		for ( int i = 0; i < 6; ++i )
		{
			m_DynamicState.m_LightingState.m_vecAmbientCube[i] = m_DynamicState.m_CompiledLightingState.m_AmbientLightCube[i].AsVector3D();
		}
		m_DynamicState.m_InstanceInfo.m_bAmbientCubeCompiled = true;
	}
}

void CShaderAPIDx8::SetVertexShaderStateAmbientLightCube( int nRegister, CompiledLightingState_t *pLightingState )
{
	float *pCubeBase = pLightingState->m_AmbientLightCube[0].Base();
	SetVertexShaderConstantInternal( nRegister, pCubeBase, 6 );
}


void CShaderAPIDx8::SetPixelShaderStateAmbientLightCube( int nRegister, CompiledLightingState_t *pLightingState )
{
	float *pCubeBase = pLightingState->m_AmbientLightCube[0].Base();
	SetPixelShaderConstantInternal( nRegister, pCubeBase, 6 );
}

float CShaderAPIDx8::GetAmbientLightCubeLuminance( MaterialLightingState_t *pLightingState )
{
	if ( !pLightingState )
		return 0.0f;

	Vector vLuminance( 0.3f, 0.59f, 0.11f );
	float fLuminance = 0.0f;

	for (int i=0; i<6; i++)
	{
		fLuminance += vLuminance.Dot( pLightingState->m_vecAmbientCube[i] );
	}

	return fLuminance / 6.0f;
}

static inline RECT* RectToRECT( Rect_t *pSrcRect, RECT &dstRect )
{
	if ( !pSrcRect )
		return NULL;

	dstRect.left = pSrcRect->x;
	dstRect.top = pSrcRect->y;
	dstRect.right = pSrcRect->x + pSrcRect->width;
	dstRect.bottom = pSrcRect->y + pSrcRect->height;
	return &dstRect;
}

// special code for RESZ resolve trick on ATI & Intel
#define RESZ_CODE 0x7fa05000

struct sRESZDummyVB
{
	float pos[ 3 ];
};
static sRESZDummyVB gRESZDummyVB[ 1 ] = { 0.0f, 0.0f, 0.0f };

void CShaderAPIDx8::CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t textureHandle, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	LOCK_SHADERAPI();
	VPROF_BUDGET( "CShaderAPIDx8::CopyRenderTargetToTexture", "Refraction overhead" );

	if ( !TextureIsAllocated( textureHandle ) )
		return;

#if defined( PIX_INSTRUMENTATION )
	{
		const char *pRT = ( nRenderTargetID < 0 ) ? "DS" : "RT";
		
		if ( textureHandle == SHADER_RENDERTARGET_NONE )
		{
			pRT = "None";
		}
		else if ( textureHandle != SHADER_RENDERTARGET_BACKBUFFER )
		{
			Texture_t &tex = GetTexture( textureHandle );
			pRT = tex.m_DebugName.String();
		}
				
		char buf[256];
		sprintf( buf, "CopyRTToTexture:%s", pRT ? pRT : "?" );
		BeginPIXEvent( 0xFFFFFFFF, buf );
		EndPIXEvent();
	}
#endif

	// Don't flush here!!  If you have to flush here, then there is a driver bug.
	// FlushHardware( );

	AssertValidTextureHandle( textureHandle );
	Texture_t *pTexture = &GetTexture( textureHandle );
	Assert( pTexture );
	IDirect3DTexture *pD3DTexture = (IDirect3DTexture *)pTexture->GetTexture();
	Assert( pD3DTexture );

	if ( !pD3DTexture )
	{
		Warning("Unexpected NULL texture in CShaderAPIDx8::CopyRenderTargetToTextureEx()\n");
		return;
	}

#if defined( _PS3 )
	IDirect3DSurface *pDstSurf;
	RECT srcRect, dstRect;
	HRESULT hr = pD3DTexture->GetSurfaceLevel( 0, &pDstSurf );
	Assert( !FAILED( hr ) );
	if ( FAILED( hr ) )
	{
		return;
	}

	// On PS3, we use some openGL functionality to blit one of the currently-bound ender targets to the given texture.
	// Like Xbox360, this cannot actually stretch the render target; we simply ignore the destination rect width/height.
	hr = Dx9Device()->StretchRect( ( IDirect3DSurface9 * )nRenderTargetID, RectToRECT( pSrcRect, srcRect ),
		pDstSurf, RectToRECT( pDstRect, dstRect ), D3DTEXF_LINEAR );
	pDstSurf->Release();
	Assert( !FAILED( hr ) );

#elif defined( _WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
	static ConVarRef mat_resolveFullFrameDepth( "mat_resolveFullFrameDepth" );

	if ( ( nRenderTargetID == -1 ) && g_pHardwareConfig->SupportsResolveDepth() && g_pHardwareConfig->HasFullResolutionDepthTexture() )
	{
		// z buffer resolve tricks

		/*
		not supporting this path yet
		benefit is no depth resolve required (if MSAA off)
		downside is risk of using/modifying depth buffer while rendering (now or in the future), and in managing depth surfaces - resolve at this stage is a much cleaner/safer way of using depth
		if ( g_pHardwareConfig->ActualCaps().m_bSupportsINTZ && ( config.m_nAASamples <= 1 ) )
		{
		// Supports INTZ and MSAA must be OFF
		// Can 'use' the depth stencil surface as is, without need to resolve
		// nothing to do here other than to check that the current DepthStencilSurface and DepthTexture are bound with our INTZ texture
		}
		else
		*/
		if ( g_pHardwareConfig->ActualCaps().m_bSupportsRESZ )
		{
			// Supports RESZ (ATI and Intel only)
			// Use a dummy draw call to set sampler 0 to our destination depth texture 
			// and set the pointsize renderstate to RESZ_CODE in order to perform the resolve
			// Works with or without MSAA

			Dx9Device()->SetVertexShader( NULL );
			Dx9Device()->SetPixelShader( NULL );
			Dx9Device()->SetFVF( D3DFVF_XYZ );

			// Bind depth stencil texture to texture sampler 0
			Dx9Device()->SetTexture( 0, pD3DTexture );

			// Perform a dummy draw call to ensure texture sampler 0 is set before the resolve is triggered
			// Vertex declaration and shaders may need to be adjusted to ensure no debug error message is produced
			SetRenderStateForce( D3DRS_ZENABLE, FALSE );
			SetRenderStateForce( D3DRS_ZWRITEENABLE, FALSE );
			SetRenderStateForce( D3DRS_COLORWRITEENABLE, 0 );

			Dx9Device()->DrawPrimitiveUP_RESZ( &gRESZDummyVB );

			SetRenderStateForce( D3DRS_ZWRITEENABLE, TRUE );
			SetRenderStateForce( D3DRS_ZENABLE, TRUE );
			SetRenderStateForce( D3DRS_COLORWRITEENABLE, 0x0F );

			// Trigger the depth buffer resolve; after this call texture sampler 0
			// will contain the contents of the resolve operation
			SetRenderStateForce( D3DRS_POINTSIZE, RESZ_CODE );

			// This hack to fix resz hack, has been found by Maksym Bezus
			// Without this line resz will be resolved only for first frame
			SetRenderStateForce( D3DRS_POINTSIZE, 0 );

			// reset vertex decl, fvf
			VertexFormat_t vertexFormat = MeshMgr()->GetCurrentVertexFormat();
			m_DynamicState.m_pVertexDecl = NULL;
			SetVertexDecl( vertexFormat, false, false, false, false, NULL );

			// reset VS/PS
			ShaderManager()->ResetShaderState();

			// reset bound texture
			SamplerState( 0 ).m_BoundTexture = INVALID_SHADERAPI_TEXTURE_HANDLE;
			Dx9Device()->SetTexture( 0, 0 );
		}
		else if ( g_pHardwareConfig->ActualCaps().m_VendorID == VENDORID_NVIDIA )
		{
			// Use NvAPI_D3D9_StretchRectEx to perform the resolve
			// Works with or without MSAA
			TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "NVIDIA Setup Resolve" );

			Assert( m_pZBufferSurface );

			if ( m_pNVAPI_registeredDepthStencilSurface != m_pZBufferSurface )
			{
				SPEW_REFCOUNT( m_pZBufferSurface );
				NvAPI_D3D9_RegisterResource( m_pZBufferSurface );
				SPEW_REFCOUNT( m_pZBufferSurface );

				if ( m_pNVAPI_registeredDepthStencilSurface != NULL )
				{
					// Unregister old one if there is any
					SPEW_REFCOUNT( m_pNVAPI_registeredDepthStencilSurface );
					NvAPI_D3D9_UnregisterResource( m_pNVAPI_registeredDepthStencilSurface );
					SPEW_REFCOUNT( m_pNVAPI_registeredDepthStencilSurface );
				}
				m_pNVAPI_registeredDepthStencilSurface = m_pZBufferSurface;
			}

			if ( m_pNVAPI_registeredDepthTexture != pD3DTexture )
			{
				SPEW_REFCOUNT( pD3DTexture );
				NvAPI_D3D9_RegisterResource( pD3DTexture );
				SPEW_REFCOUNT( pD3DTexture );

				if ( m_pNVAPI_registeredDepthTexture != NULL )
				{
					// Unregister old one if there is any
					SPEW_REFCOUNT( m_pNVAPI_registeredDepthStencilSurface );
					NvAPI_D3D9_UnregisterResource( m_pNVAPI_registeredDepthTexture );
					SPEW_REFCOUNT( m_pNVAPI_registeredDepthStencilSurface );
				}
				m_pNVAPI_registeredDepthTexture = pD3DTexture;
			}

			// Resolve
			SPEW_REFCOUNT( m_pZBufferSurface );
			Dx9Device()->StretchRectEx_NvAPI( m_pZBufferSurface, NULL, pD3DTexture, NULL, D3DTEXF_POINT );
			SPEW_REFCOUNT( m_pZBufferSurface );
		}
		else
		{
			// Fallback, extra depth only pass required
			// See SSAO_DepthPass()
		}
		return;
	}
	else
	{
		if ( nRenderTargetID == -1 )
		{
			// copy depth request
			// nothing to do here, extra depth pass required
			return;
		}

		IDirect3DSurface* pRenderTargetSurface;
		HRESULT hr = Dx9Device()->GetRenderTarget( nRenderTargetID, &pRenderTargetSurface );
		if ( FAILED( hr ) )
		{
			Assert( 0 );
			return;
		}

		IDirect3DSurface *pDstSurf;
		hr = pD3DTexture->GetSurfaceLevel( 0, &pDstSurf );
		Assert( !FAILED( hr ) );
		if ( FAILED( hr ) )
		{
			pRenderTargetSurface->Release();
			return;
		}

		bool tryblit = true;
		if ( tryblit )
		{
			RECORD_COMMAND( DX8_COPY_FRAMEBUFFER_TO_TEXTURE, 1 );
			RECORD_INT( textureHandle );

			RECT srcRect, dstRect;
			hr = Dx9Device()->StretchRect( pRenderTargetSurface, RectToRECT( pSrcRect, srcRect ),
										   pDstSurf, RectToRECT( pDstRect, dstRect ), D3DTEXF_LINEAR );
			Assert( !FAILED( hr ) );
		}

		pDstSurf->Release();
		pRenderTargetSurface->Release();
	}
#elif !defined( _X360 )
	static ConVarRef mat_resolveFullFrameDepth( "mat_resolveFullFrameDepth" );

	if ( ( nRenderTargetID == -1 ) && g_pHardwareConfig->SupportsResolveDepth() && g_pHardwareConfig->HasFullResolutionDepthTexture() )
	{
		Assert( m_pZBufferSurface );

		IDirect3DSurface* pDstSurf;
		HRESULT hr = pD3DTexture->GetSurfaceLevel( 0, &pDstSurf );
		Assert( !FAILED( hr ) );
		if ( FAILED( hr ) )
		{
			Assert( 0 );
			return;
		}

		bool tryblit = true;
		if ( tryblit )
		{
			RECORD_COMMAND( DX8_COPY_FRAMEBUFFER_TO_TEXTURE, 1 );
			RECORD_INT( textureHandle );

			RECT srcRect, dstRect;
			hr = Dx9Device()->StretchRect( m_pZBufferSurface, RectToRECT( pSrcRect, srcRect ),
										   pDstSurf, RectToRECT( pDstRect, dstRect ), D3DTEXF_POINT );
			Assert( !FAILED( hr ) );
		}

		pDstSurf->Release();
	}
	else
	{
		if ( nRenderTargetID == -1 )
		{
			// copy depth request
			// nothing to do here, extra depth pass required
			return;
		}

		IDirect3DSurface* pRenderTargetSurface;
		HRESULT hr = Dx9Device()->GetRenderTarget( nRenderTargetID, &pRenderTargetSurface );
		if ( FAILED( hr ) )
		{
			Assert( 0 );
			return;
		}

		IDirect3DSurface *pDstSurf;
		hr = pD3DTexture->GetSurfaceLevel( 0, &pDstSurf );
		Assert( !FAILED( hr ) );
		if ( FAILED( hr ) )
		{
			pRenderTargetSurface->Release();
			return;
		}

		bool tryblit = true;
		if ( tryblit )
		{
			RECORD_COMMAND( DX8_COPY_FRAMEBUFFER_TO_TEXTURE, 1 );
			RECORD_INT( textureHandle );

			RECT srcRect, dstRect;
			hr = Dx9Device()->StretchRect( pRenderTargetSurface, RectToRECT( pSrcRect, srcRect ),
										   pDstSurf, RectToRECT( pDstRect, dstRect ), D3DTEXF_LINEAR );
			Assert( !FAILED( hr ) );
		}

		pDstSurf->Release();
		pRenderTargetSurface->Release();
	}
#else
	DWORD flags = 0;
	switch( nRenderTargetID )
	{
	case -1:
		flags = D3DRESOLVE_DEPTHSTENCIL | D3DRESOLVE_FRAGMENT0;
		break;
	case 0:
		flags = D3DRESOLVE_RENDERTARGET0;
		break;
	case 1:
	case 2:
	case 3:
		// not supporting MRT
		Assert( 0 );
		return;
	NO_DEFAULT
	};

	// not prepared to handle mip mapping yet
	Assert( pD3DTexture->GetLevelCount() == 1 ); 

	D3DPOINT dstPoint = { 0 };
	if ( pDstRect )
	{
		dstPoint.x = pDstRect->x;
		dstPoint.y = pDstRect->y;
	}

	int destWidth, destHeight;
	if( pDstRect )
	{
		destWidth = pDstRect->width;
		destHeight = pDstRect->height;

		Assert( (destWidth <= pTexture->GetWidth()) && (destHeight <= pTexture->GetHeight()) );
	}
	else
	{
		destWidth = pTexture->GetWidth();
		destHeight = pTexture->GetHeight();
	}	

	RECT srcRect;
	RECT *pResolveRect = NULL;
	int srcWidth, srcHeight;
	if ( pSrcRect )
	{
		RectToRECT( pSrcRect, srcRect );
		pResolveRect = &srcRect;

		// resolve has no stretching ability, and we can only compensate when doing a resolve to a whole texture larger than the source
		Assert( !pDstRect || ( pSrcRect->width <= pDstRect->width && pSrcRect->height <= pDstRect->height ) );

		srcWidth = pSrcRect->width;
		srcHeight = pSrcRect->height;
	}
	else
	{
		srcRect.left = srcRect.top = 0;
		srcRect.right = m_DynamicState.m_Viewport.Width;
		srcRect.bottom = m_DynamicState.m_Viewport.Height;
		if ( (srcRect.right < 0) || (srcRect.bottom < 0) )
		{
			if ( m_UsingTextureRenderTarget )
			{
				srcRect.right = m_ViewportMaxWidth;
				srcRect.bottom = m_ViewportMaxHeight;
			}
			else
			{
				int w,h;
				GetBackBufferDimensions( w, h );
				srcRect.right = w;
				srcRect.bottom = h;
			}
		}
		srcWidth = srcRect.right;
		srcHeight = srcRect.bottom;
	}

	// Save off width and height so we can restore it after the resolve
	int nD3DTextureFormatSizeTwoDWidth = pD3DTexture->Format.Size.TwoD.Width;
	int nD3DTextureFormatSizeTwoDHeight = pD3DTexture->Format.Size.TwoD.Height;
	if ( ( srcWidth != destWidth ) || ( srcHeight != destHeight ) )
	{
		//Not a 1:1 resolve, we should only have gotten this far if we can downsize the target texture to compensate
		Assert( (destWidth > srcWidth) && (destHeight > srcHeight) && (dstPoint.x == 0) && (dstPoint.y == 0) );

		//What we're doing is telling D3D that this texture is smaller than it is so the resolve is 1:1.
		//We leave the texture in this state until it resolves from something bigger.
		//All outside code still thinks this texture is it's original size. And it still owns enough memory to go back to it's original size.
		pD3DTexture->Format.Size.TwoD.Width = srcWidth - 1;
		pD3DTexture->Format.Size.TwoD.Height = srcHeight - 1; //no idea why they store it as size-1, but they do
		pResolveRect = NULL;
	}

	// if we convert to srgb format, we need the original format for reverting. We only need the first DWORD of GPUTEXTURE_FETCH_CONSTANT.
	DWORD linearFormatBackup = pD3DTexture->Format.dword[0]; 
	if ( !( flags & D3DRESOLVE_DEPTHSTENCIL ) && ( m_DynamicState.m_bSRGBWritesEnabled ) )
	{
		// we need a matched resolve regarding sRGB to get values transfered as-is
		// when the surface is sRGB, use the corresponding sRGB texture
		pD3DTexture->Format.SignX = 
			pD3DTexture->Format.SignY = 
			pD3DTexture->Format.SignZ = 3;
	}
	
	if ( IsDebug() )
	{
		// From XDK Resolve() docs...
		// Each coordinate of the rectangle must be a multiple of 8 (use GPU_RESOLVE_ALIGNMENT).
		// The alignment requirement is relaxed for the lower right coordinate under one conditionif,
		// in conjunction with pDestPoint, it results in a destination rectangle whose lower right corner matches
		// the lower right corner of the destination.
		if ( pResolveRect )
		{
			bool bUnaligned = false;

			if ( ( pResolveRect->left % GPU_RESOLVE_ALIGNMENT ) ||
				( pResolveRect->right % GPU_RESOLVE_ALIGNMENT ) ||
				( pResolveRect->top % GPU_RESOLVE_ALIGNMENT ) ||
				( pResolveRect->bottom % GPU_RESOLVE_ALIGNMENT ) )
			{
				bUnaligned = true;
			}
			if ( ( abs( pResolveRect->left - dstPoint.x ) % 32 ) ||
				 ( abs( pResolveRect->top - dstPoint.y ) % 32 ) )
			{
				bUnaligned = true;
			}

			if ( bUnaligned )
			{
				// allowed if resolve is a 1:1
				if ( ( pResolveRect->left == 0 ) &&
					( pResolveRect->top == 0 ) &&
					( dstPoint.x == 0 ) &&
					( dstPoint.y == 0 ) &&
					( (unsigned int)pResolveRect->right - 1 == pD3DTexture->Format.Size.TwoD.Width ) &&
					( (unsigned int)pResolveRect->bottom - 1 == pD3DTexture->Format.Size.TwoD.Height ) )
				{
					bUnaligned = false;
				}
			}
			
			Assert( bUnaligned == false );
		}
	}

#if defined( DBGFLAG_ASSERT )
	if( pResolveRect )
	{
		D3DVIEWPORT9 viewPort;
		Dx9Device()->GetViewport( &viewPort );
		Assert( ( int ) viewPort.Width >= (pResolveRect->right - pResolveRect->left) ); //easier to catch here than with the cryptic error from d3d
	}
#endif

	HRESULT hr = Dx9Device()->Resolve( flags, (D3DRECT*)pResolveRect, pD3DTexture, &dstPoint, 0, 0, NULL, 0, 0,	NULL );
	Assert( !FAILED( hr ) );

	pD3DTexture->Format.dword[0] = linearFormatBackup;

	// Restore D3D texture to full size in case it was downsized above
	pD3DTexture->Format.Size.TwoD.Width = nD3DTextureFormatSizeTwoDWidth;
	pD3DTexture->Format.Size.TwoD.Height = nD3DTextureFormatSizeTwoDHeight;
#endif
}

void CShaderAPIDx8::CopyRenderTargetToTexture( ShaderAPITextureHandle_t textureHandle )
{
	LOCK_SHADERAPI();
	CopyRenderTargetToTextureEx( textureHandle, 0 );
}

void CShaderAPIDx8::CopyTextureToRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	LOCK_SHADERAPI();
	VPROF( "CShaderAPIDx8::CopyRenderTargetToTexture" );

	if ( !TextureIsAllocated( textureHandle ) )
		return;

	// Don't flush here!!  If you have to flush here, then there is a driver bug.
	// FlushHardware( );

	AssertValidTextureHandle( textureHandle );
	Texture_t *pTexture = &GetTexture( textureHandle );
	Assert( pTexture );
	IDirect3DTexture *pD3DTexture = (IDirect3DTexture *)pTexture->GetTexture();
	Assert( pD3DTexture );

#if !defined( _X360 )
	IDirect3DSurface* pRenderTargetSurface;
	HRESULT hr = Dx9Device()->GetRenderTarget( nRenderTargetID, &pRenderTargetSurface );
	if ( FAILED( hr ) )
	{
		Assert( 0 );
		return;
	}

	IDirect3DSurface *pDstSurf;
	hr = pD3DTexture->GetSurfaceLevel( 0, &pDstSurf );
	Assert( !FAILED( hr ) );
	if ( FAILED( hr ) )
	{
		pRenderTargetSurface->Release();
		return;
	}

	bool tryblit = true;
	if ( tryblit )
	{
		RECORD_COMMAND( DX8_COPY_FRAMEBUFFER_TO_TEXTURE, 1 );
		RECORD_INT( textureHandle );

		RECT srcRect, dstRect;
		hr = Dx9Device()->StretchRect( pDstSurf, RectToRECT( pSrcRect, srcRect ),
			pRenderTargetSurface, RectToRECT( pDstRect, dstRect ), D3DTEXF_LINEAR );
		Assert( !FAILED( hr ) );
	}

	pDstSurf->Release();
	pRenderTargetSurface->Release();
#else
	Assert( 0 );
#endif
}

static const char *TextureArgToString( int arg )
{
	static char buf[128];
	switch( arg & D3DTA_SELECTMASK )
	{
	case D3DTA_DIFFUSE:
		strcpy( buf, "D3DTA_DIFFUSE" );
		break;
	case D3DTA_CURRENT:
		strcpy( buf, "D3DTA_CURRENT" );
		break;
	case D3DTA_TEXTURE:
		strcpy( buf, "D3DTA_TEXTURE" );
		break;
	case D3DTA_TFACTOR:
		strcpy( buf, "D3DTA_TFACTOR" );
		break;
	case D3DTA_SPECULAR:
		strcpy( buf, "D3DTA_SPECULAR" );
		break;
	case D3DTA_TEMP:
		strcpy( buf, "D3DTA_TEMP" );
		break;
	default:
		strcpy( buf, "<ERROR>" );
		break;
	}

	if( arg & D3DTA_COMPLEMENT )
	{
		strcat( buf, "|D3DTA_COMPLEMENT" );
	}
	if( arg & D3DTA_ALPHAREPLICATE )
	{
		strcat( buf, "|D3DTA_ALPHAREPLICATE" );
	}
	return buf;
}

static const char *TextureOpToString( D3DTEXTUREOP op )
{
	switch( op )
	{
	case D3DTOP_DISABLE:
		return "D3DTOP_DISABLE";
    case D3DTOP_SELECTARG1:
		return "D3DTOP_SELECTARG1";
    case D3DTOP_SELECTARG2:
		return "D3DTOP_SELECTARG2";
    case D3DTOP_MODULATE:
		return "D3DTOP_MODULATE";
    case D3DTOP_MODULATE2X:
		return "D3DTOP_MODULATE2X";
    case D3DTOP_MODULATE4X:
		return "D3DTOP_MODULATE4X";
    case D3DTOP_ADD:
		return "D3DTOP_ADD";
    case D3DTOP_ADDSIGNED:
		return "D3DTOP_ADDSIGNED";
    case D3DTOP_ADDSIGNED2X:
		return "D3DTOP_ADDSIGNED2X";
    case D3DTOP_SUBTRACT:
		return "D3DTOP_SUBTRACT";
    case D3DTOP_ADDSMOOTH:
		return "D3DTOP_ADDSMOOTH";
    case D3DTOP_BLENDDIFFUSEALPHA:
		return "D3DTOP_BLENDDIFFUSEALPHA";
    case D3DTOP_BLENDTEXTUREALPHA:
		return "D3DTOP_BLENDTEXTUREALPHA";
    case D3DTOP_BLENDFACTORALPHA:
		return "D3DTOP_BLENDFACTORALPHA";
    case D3DTOP_BLENDTEXTUREALPHAPM:
		return "D3DTOP_BLENDTEXTUREALPHAPM";
    case D3DTOP_BLENDCURRENTALPHA:
		return "D3DTOP_BLENDCURRENTALPHA";
    case D3DTOP_PREMODULATE:
		return "D3DTOP_PREMODULATE";
    case D3DTOP_MODULATEALPHA_ADDCOLOR:
		return "D3DTOP_MODULATEALPHA_ADDCOLOR";
    case D3DTOP_MODULATECOLOR_ADDALPHA:
		return "D3DTOP_MODULATECOLOR_ADDALPHA";
    case D3DTOP_MODULATEINVALPHA_ADDCOLOR:
		return "D3DTOP_MODULATEINVALPHA_ADDCOLOR";
    case D3DTOP_MODULATEINVCOLOR_ADDALPHA:
		return "D3DTOP_MODULATEINVCOLOR_ADDALPHA";
    case D3DTOP_BUMPENVMAP:
		return "D3DTOP_BUMPENVMAP";
    case D3DTOP_BUMPENVMAPLUMINANCE:
		return "D3DTOP_BUMPENVMAPLUMINANCE";
    case D3DTOP_DOTPRODUCT3:
		return "D3DTOP_DOTPRODUCT3";
    case D3DTOP_MULTIPLYADD:
		return "D3DTOP_MULTIPLYADD";
    case D3DTOP_LERP:
		return "D3DTOP_LERP";
	default:
		return "<ERROR>";
	}
}

static const char *BlendModeToString( int blendMode )
{
	switch( blendMode )
	{
	case D3DBLEND_ZERO:
		return "D3DBLEND_ZERO";
    case D3DBLEND_ONE:
		return "D3DBLEND_ONE";
    case D3DBLEND_SRCCOLOR:
		return "D3DBLEND_SRCCOLOR";
    case D3DBLEND_INVSRCCOLOR:
		return "D3DBLEND_INVSRCCOLOR";
    case D3DBLEND_SRCALPHA:
		return "D3DBLEND_SRCALPHA";
    case D3DBLEND_INVSRCALPHA:
		return "D3DBLEND_INVSRCALPHA";
    case D3DBLEND_DESTALPHA:
		return "D3DBLEND_DESTALPHA";
    case D3DBLEND_INVDESTALPHA:
		return "D3DBLEND_INVDESTALPHA";
    case D3DBLEND_DESTCOLOR:
		return "D3DBLEND_DESTCOLOR";
    case D3DBLEND_INVDESTCOLOR:
		return "D3DBLEND_INVDESTCOLOR";
    case D3DBLEND_SRCALPHASAT:
		return "D3DBLEND_SRCALPHASAT";
#if !defined( _X360 )
	case D3DBLEND_BOTHSRCALPHA:
		return "D3DBLEND_BOTHSRCALPHA";
    case D3DBLEND_BOTHINVSRCALPHA:
		return "D3DBLEND_BOTHINVSRCALPHA";
#endif
	default:
		return "<ERROR>";
	}
}

//-----------------------------------------------------------------------------
// Spew Board State
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SpewBoardState()
{
	// FIXME: This has regressed
	return;
#ifdef DEBUG_BOARD_STATE
/*
	{
		static ID3DXFont* pFont = 0;
		if (!pFont)
		{
			HFONT hFont = CreateFont( 0, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
				ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
				DEFAULT_PITCH | FF_MODERN, 0 );
			Assert( hFont != 0 );

			HRESULT hr = D3DXCreateFont( Dx9Device(), hFont, &pFont );
		}

		static char buf[1024];
		static RECT r = { 0, 0, 640, 480 };

		if (m_DynamicState.m_VertexBlend == 0)
			return;
		
#if 1
		D3DXMATRIX* m = &GetTransform(MATERIAL_MODEL);
		D3DXMATRIX* m2 = &GetTransform(MATERIAL_MODEL + 1);
		sprintf(buf,"FVF %x\n"
			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n"
			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n\n",
			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n",
			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n",
			ShaderManager()->GetCurrentVertexShader(),
			m->m[0][0],	m->m[0][1],	m->m[0][2],	m->m[0][3],	
			m->m[1][0],	m->m[1][1],	m->m[1][2],	m->m[1][3],	
			m->m[2][0],	m->m[2][1],	m->m[2][2],	m->m[2][3],	
			m->m[3][0], m->m[3][1], m->m[3][2], m->m[3][3],
			m2->m[0][0], m2->m[0][1], m2->m[0][2], m2->m[0][3],	
			m2->m[1][0], m2->m[1][1], m2->m[1][2], m2->m[1][3],	
			m2->m[2][0], m2->m[2][1], m2->m[2][2], m2->m[2][3],	
			m2->m[3][0], m2->m[3][1], m2->m[3][2], m2->m[3][3]
			 );
#else
		Vector4D *pVec2 = &m_DynamicState.m_pVectorVertexShaderConstant[VERTEX_SHADER_MODELVIEWPROJ];
		Vector4D *pVec3 = &m_DynamicState.m_pVectorVertexShaderConstant[VERTEX_SHADER_VIEWPROJ];
		Vector4D *pVec4 = &m_DynamicState.m_pVectorVertexShaderConstant[VERTEX_SHADER_MODEL];

		sprintf(buf,"\n"
			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n"
			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n\n"

			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n"
			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n\n"

			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n"
			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n\n"

			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n"
			"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n",

			pVec1[0][0], pVec1[0][1], pVec1[0][2], pVec1[0][3],	
			pVec1[1][0], pVec1[1][1], pVec1[1][2], pVec1[1][3],	
			pVec1[2][0], pVec1[2][1], pVec1[2][2], pVec1[2][3],	
			pVec1[3][0], pVec1[3][1], pVec1[3][2], pVec1[3][3],

			pVec2[0][0], pVec2[0][1], pVec2[0][2], pVec2[0][3],	
			pVec2[1][0], pVec2[1][1], pVec2[1][2], pVec2[1][3],	
			pVec2[2][0], pVec2[2][1], pVec2[2][2], pVec2[2][3],	
			pVec2[3][0], pVec2[3][1], pVec2[3][2], pVec2[3][3],

			pVec3[0][0], pVec3[0][1], pVec3[0][2], pVec3[0][3],	
			pVec3[1][0], pVec3[1][1], pVec3[1][2], pVec3[1][3],	
			pVec3[2][0], pVec3[2][1], pVec3[2][2], pVec3[2][3],	
			pVec3[3][0], pVec3[3][1], pVec3[3][2], pVec3[3][3],

			pVec4[0][0], pVec4[0][1], pVec4[0][2], pVec4[0][3],	
			pVec4[1][0], pVec4[1][1], pVec4[1][2], pVec4[1][3],	
			pVec4[2][0], pVec4[2][1], pVec4[2][2], pVec4[2][3],	
			0, 0, 0, 1
			);
#endif
		pFont->Begin();
		pFont->DrawText( buf, -1, &r, DT_LEFT | DT_TOP,
			D3DCOLOR_RGBA( 255, 255, 255, 255 ) );
		pFont->End();

		return;
	}

#if 0
	Vector4D *pVec2 = &m_DynamicState.m_pVectorVertexShaderConstant[VERTEX_SHADER_MODELVIEWPROJ];
	Vector4D *pVec3 = &m_DynamicState.m_pVectorVertexShaderConstant[VERTEX_SHADER_VIEWPROJ];
	Vector4D *pVec4 = &m_DynamicState.m_pVectorVertexShaderConstant[VERTEX_SHADER_MODEL];

	static char buf2[1024];
	sprintf(buf2,"\n"
		"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n"
		"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n\n"

		"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n"
		"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n\n"

		"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n"
		"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n\n"

		"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n"
		"[%6.3f %6.3f %6.3f %6.3f]\n[%6.3f %6.3f %6.3f %6.3f]\n",

		pVec1[0][0], pVec1[0][1], pVec1[0][2], pVec1[0][3],	
		pVec1[1][0], pVec1[1][1], pVec1[1][2], pVec1[1][3],	
		pVec1[2][0], pVec1[2][1], pVec1[2][2], pVec1[2][3],	
		pVec1[3][0], pVec1[3][1], pVec1[3][2], pVec1[3][3],

		pVec2[0][0], pVec2[0][1], pVec2[0][2], pVec2[0][3],	
		pVec2[1][0], pVec2[1][1], pVec2[1][2], pVec2[1][3],	
		pVec2[2][0], pVec2[2][1], pVec2[2][2], pVec2[2][3],	
		pVec2[3][0], pVec2[3][1], pVec2[3][2], pVec2[3][3],

		pVec3[0][0], pVec3[0][1], pVec3[0][2], pVec3[0][3],	
		pVec3[1][0], pVec3[1][1], pVec3[1][2], pVec3[1][3],	
		pVec3[2][0], pVec3[2][1], pVec3[2][2], pVec3[2][3],	
		pVec3[3][0], pVec3[3][1], pVec3[3][2], pVec3[3][3],

		pVec4[0][0], pVec4[0][1], pVec4[0][2], pVec4[0][3],	
		pVec4[1][0], pVec4[1][1], pVec4[1][2], pVec4[1][3],	
		pVec4[2][0], pVec4[2][1], pVec4[2][2], pVec4[2][3],	
		0, 0, 0, 1.0f
		);
	Plat_DebugString(buf2);
	return;
#endif
*/

	char buf[256];
	sprintf(buf, "\nSnapshot id %d : \n", m_TransitionTable.CurrentSnapshot() );
	Plat_DebugString(buf);

	ShadowState_t &boardState = m_TransitionTable.BoardState();
	ShadowShaderState_t &boardShaderState = m_TransitionTable.BoardShaderState();

	sprintf(buf,"Depth States: ZFunc %d, ZWrite %d, ZEnable %d, ZBias %d\n",
		boardState.m_DepthTestState.m_ZFunc, boardState.m_DepthTestState.m_ZWriteEnable, 
		boardState.m_DepthTestState.m_ZEnable, boardState.m_DepthTestState.m_ZBias );
	Plat_DebugString(buf);
	sprintf(buf,"Cull Enable %d Cull Mode %d Color Write %d Fill %d Const Color sRGBWriteEnable %d\n",
		boardState.m_AlphaTestAndMiscState.m_CullEnable, m_DynamicState.m_CullMode, boardState.m_DepthTestState.m_ColorWriteEnable, 
		boardState.m_AlphaTestAndMiscState.m_FillMode, boardState.m_FogAndMiscState.m_SRGBWriteEnable );
	Plat_DebugString(buf);
	AlphaBlendState_t const &boardAlpha = boardState.m_AlphaBlendState;

	sprintf(buf,"Blend States: Blend Enable %d Test Enable %d Func %d SrcBlend %d (%s) DstBlend %d (%s)\n",
		boardAlpha.m_AlphaBlendEnable, boardState.m_AlphaTestAndMiscState.m_AlphaTestEnable, 
		boardState.m_AlphaTestAndMiscState.m_AlphaFunc, boardAlpha.m_SrcBlend, BlendModeToString( boardAlpha.m_SrcBlend ),
		boardAlpha.m_DestBlend, BlendModeToString( boardAlpha.m_DestBlend ) );
	Plat_DebugString(buf);
	int len = sprintf(buf,"Alpha Ref %d, LightsEnabled %d\n",
		boardState.m_AlphaTestAndMiscState.m_AlphaRef, m_DynamicState.m_LightingState.m_nLocalLightCount );
	Plat_DebugString(buf);
	
	sprintf(buf,"Pass Vertex Usage: %llx Pixel Shader %p Vertex Shader %p\n",
		boardShaderState.m_VertexUsage, ShaderManager()->GetCurrentPixelShader(), 
		ShaderManager()->GetCurrentVertexShader() );
	Plat_DebugString(buf);

	// REGRESSED!!!!
	/*
	D3DXMATRIX* m = &GetTransform(MATERIAL_MODEL);
	sprintf(buf,"WorldMat [%4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f]\n",
		m->m[0][0],	m->m[0][1],	m->m[0][2],	m->m[0][3],	
		m->m[1][0],	m->m[1][1],	m->m[1][2],	m->m[1][3],	
		m->m[2][0],	m->m[2][1],	m->m[2][2],	m->m[2][3],	
		m->m[3][0], m->m[3][1], m->m[3][2], m->m[3][3] );
	Plat_DebugString(buf);

	m = &GetTransform(MATERIAL_MODEL + 1);
	sprintf(buf,"WorldMat2 [%4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f]\n",
		m->m[0][0],	m->m[0][1],	m->m[0][2],	m->m[0][3],	
		m->m[1][0],	m->m[1][1],	m->m[1][2],	m->m[1][3],	
		m->m[2][0],	m->m[2][1],	m->m[2][2],	m->m[2][3],	
		m->m[3][0], m->m[3][1], m->m[3][2], m->m[3][3] );
	Plat_DebugString(buf);

	m = &GetTransform(MATERIAL_VIEW);
	sprintf(buf,"ViewMat [%4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f]\n",
		m->m[0][0],	m->m[0][1],	m->m[0][2],	
		m->m[1][0],	m->m[1][1],	m->m[1][2],
		m->m[2][0],	m->m[2][1],	m->m[2][2],
		m->m[3][0], m->m[3][1], m->m[3][2] );
	Plat_DebugString(buf);

	m = &GetTransform(MATERIAL_PROJECTION);
	sprintf(buf,"ProjMat [%4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f]\n",
		m->m[0][0],	m->m[0][1],	m->m[0][2],	
		m->m[1][0],	m->m[1][1],	m->m[1][2],
		m->m[2][0],	m->m[2][1],	m->m[2][2],
		m->m[3][0], m->m[3][1], m->m[3][2] );
	Plat_DebugString(buf);

	for (i = 0; i < GetTextureStageCount(); ++i)
	{
		m = &GetTransform(MATERIAL_TEXTURE0 + i);
		sprintf(buf,"TexMat%d [%4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f]\n",
			i, m->m[0][0],	m->m[0][1],	m->m[0][2],	
			m->m[1][0],	m->m[1][1],	m->m[1][2],
			m->m[2][0],	m->m[2][1],	m->m[2][2],
			m->m[3][0], m->m[3][1], m->m[3][2] );
		Plat_DebugString(buf);
	}
	*/

	sprintf(buf,"Viewport (%d %d) [%d %d] %4.3f %4.3f\n",
		m_DynamicState.m_Viewport.X, m_DynamicState.m_Viewport.Y, 
		m_DynamicState.m_Viewport.Width, m_DynamicState.m_Viewport.Height,
		m_DynamicState.m_Viewport.MinZ, m_DynamicState.m_Viewport.MaxZ);
	Plat_DebugString(buf);

	for ( int i = 0; i < MAX_SAMPLERS; ++i )
	{
		sprintf(buf,"	Texture Enabled: %d Bound Texture: %d UWrap: %d VWrap: %d\n",
			SamplerState(i).m_TextureEnable, GetBoundTextureBindId( (Sampler_t)i ),
			SamplerState(i).m_UTexWrap, SamplerState(i).m_VTexWrap );
		Plat_DebugString(buf);
		sprintf(buf,"	Mag Filter: %d Min Filter: %d Mip Filter: %d\n",
			SamplerState(i).m_MagFilter, SamplerState(i).m_MinFilter,
			SamplerState(i).m_MipFilter );
		Plat_DebugString(buf);
	}
#else
	Plat_DebugString("::SpewBoardState() Not Implemented Yet");
#endif
}

//-----------------------------------------------------------------------------
// Begin a render pass
//-----------------------------------------------------------------------------
void CShaderAPIDx8::BeginPass( StateSnapshot_t snapshot )
{
	LOCK_SHADERAPI();
	VPROF("CShaderAPIDx8::BeginPass");
	if (IsDeactivated())
		return;

	m_nCurrentSnapshot = snapshot;
//	Assert( m_pRenderMesh );
	// FIXME: This only does anything with temp meshes, so don't bother yet for the new code.
	if( m_pRenderMesh )
	{
		m_pRenderMesh->BeginPass( );
	}
}


//-----------------------------------------------------------------------------
// Render da polygon!
//-----------------------------------------------------------------------------
void CShaderAPIDx8::RenderPass( const unsigned char *pInstanceCommandBuffer, int nPass, int nPassCount )
{
	if ( IsDeactivated() )
		return;

	Assert( m_nCurrentSnapshot != -1 );
//	Assert( m_pRenderMesh );  MESHFIXME
	
	m_TransitionTable.UseSnapshot( m_nCurrentSnapshot );
	CommitPerPassStateChanges( m_nCurrentSnapshot );

	// Make sure that we bound a texture for every stage that is enabled
	// NOTE: not enabled/finished yet... see comment in CShaderAPIDx8::ApplyTextureEnable
//	int nSampler;
//	for ( nSampler = 0; nSampler < g_pHardwareConfig->GetSamplerCount(); nSampler++ )
//	{
//		if ( SamplerState( nSampler ).m_TextureEnable )
//		{
//		}
//	}
	
#ifdef DEBUG_BOARD_STATE
	// Spew out render state...
	if ( m_pMaterial->PerformDebugTrace() )
	{
		SpewBoardState();
	}
#endif

#ifdef TEST_CACHE_LOCKS
	g_pDataCache->Flush();
#endif

//	Assert( m_pRenderMesh );  MESHFIXME
	if ( m_pRenderMesh )
	{
		m_pRenderMesh->RenderPass( pInstanceCommandBuffer );
	}
	else
	{
		MeshMgr()->RenderPassWithVertexAndIndexBuffers( pInstanceCommandBuffer );
	}
	m_nCurrentSnapshot = -1;
}


//-----------------------------------------------------------------------------
// Matrix mode
//-----------------------------------------------------------------------------
void CShaderAPIDx8::MatrixMode( MaterialMatrixMode_t matrixMode )
{
	// NOTE!!!!!!
	// The only time that m_MatrixMode is used is for texture matrices.  Do not use
	// it for anything else unless you change this code!
	m_MatrixMode = (D3DTRANSFORMSTATETYPE)-1;
	m_CurrStack = GetMatrixStack( matrixMode );
}
				 	    
// The current camera position in world space.
void CShaderAPIDx8::GetWorldSpaceCameraPosition( float* pPos ) const
{
	memcpy( pPos, m_WorldSpaceCameraPosition.Base(), sizeof( float[3] ) );
}

// The current camera direction in world space.
void CShaderAPIDx8::GetWorldSpaceCameraDirection( float* pPos ) const
{
	memcpy( pPos, m_WorldSpaceCameraDirection.Base(), sizeof( float[3] ) );
}

void CShaderAPIDx8::CacheWorldSpaceCamera()
{
	D3DXMATRIX& view = GetTransform( MATERIAL_VIEW );
	m_WorldSpaceCameraPosition[0] = 
		-( view( 3, 0 ) * view( 0, 0 ) + 
		view( 3, 1 ) * view( 0, 1 ) + 
		view( 3, 2 ) * view( 0, 2 ) );
	m_WorldSpaceCameraPosition[1] = 
		-( view( 3, 0 ) * view( 1, 0 ) + 
		view( 3, 1 ) * view( 1, 1 ) + 
		view( 3, 2 ) * view( 1, 2 ) );
	m_WorldSpaceCameraPosition[2] = 
		-( view( 3, 0 ) * view( 2, 0 ) + 
		view( 3, 1 ) * view( 2, 1 ) + 
		view( 3, 2 ) * view( 2, 2 ) );

	// Protect against zero, as some pixel shaders will divide by this in CalcWaterFogAlpha() in common_ps_fxc.h
	if ( fabs( m_WorldSpaceCameraPosition[2] ) <= 0.00001f )
	{
		m_WorldSpaceCameraPosition[2] = 0.01f;
	}

	m_WorldSpaceCameraDirection.Init( -view(0,2), -view(1,2), -view(2, 2) );
}


//-----------------------------------------------------------------------------
// Computes a matrix which includes the poly offset given an initial projection matrix
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ComputePolyOffsetMatrix( const D3DXMATRIX& matProjection, D3DXMATRIX &matProjectionOffset )
{
	// We never need to do this on hardware that can handle zbias
	if ( g_pHardwareConfig->Caps().m_ZBiasAndSlopeScaledDepthBiasSupported )
		return;

	float offsetVal = 
		-1.0f * (m_DesiredState.m_Viewport.MaxZ - m_DesiredState.m_Viewport.MinZ) /
		16384.0f;

	D3DXMATRIX offset;
	D3DXMatrixTranslation( &offset, 0.0f, 0.0f, offsetVal );
	D3DXMatrixMultiply( &matProjectionOffset, &matProjection, &offset );
}


//-----------------------------------------------------------------------------
// Caches off the poly-offset projection matrix
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CachePolyOffsetProjectionMatrix()
{
	ComputePolyOffsetMatrix( GetTransform(MATERIAL_PROJECTION), m_CachedPolyOffsetProjectionMatrix );
}


//-----------------------------------------------------------------------------
// Performs a flush on the matrix state	if necessary
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::MatrixIsChanging( TransformType_t type )
{
	if ( IsDeactivated() )	
		return false;

	// early out if the transform is already one of our standard types
	if ((type != TRANSFORM_IS_GENERAL) && (type == m_DynamicState.m_TransformType[m_CurrStack]))
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Sets the actual matrix state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::UpdateMatrixTransform( TransformType_t type )
{
	m_DynamicState.m_TransformType[m_CurrStack] = type;
	m_DynamicState.m_TransformChanged[m_CurrStack] = STATE_CHANGED;

#ifdef _DEBUG
	// Store off the board state
	D3DXMATRIX *pSrc = &GetTransform(m_CurrStack);
	D3DXMATRIX *pDst = &m_DynamicState.m_Transform[m_CurrStack];
//	Assert( *pSrc != *pDst );
	memcpy( pDst, pSrc, sizeof(D3DXMATRIX) );
#endif

	if ( m_CurrStack == MATERIAL_MODEL )
	{
		m_DynamicState.m_InstanceInfo.m_bSetSkinConstants = false;
	}

	if ( m_CurrStack == MATERIAL_VIEW )
	{
		CacheWorldSpaceCamera();
	}

	if ( !IsX360() && m_CurrStack == MATERIAL_PROJECTION )
	{
		CachePolyOffsetProjectionMatrix();
	}

	// Any time the view or projection matrix changes, the user clip planes need recomputing....
	// Assuming we're not overriding the user clip transform
	if ( ( m_CurrStack == MATERIAL_PROJECTION ) ||
		 ( ( m_CurrStack == MATERIAL_VIEW ) && ( !m_DynamicState.m_bUserClipTransformOverride ) ) )
	{
		MarkAllUserClipPlanesDirty();
	}
}


//--------------------------------------------------------------------------------
// deformations
//--------------------------------------------------------------------------------
void CShaderAPIDx8::PushDeformation( DeformationBase_t const *pDef )
{
	Assert( m_pDeformationStackPtr > m_DeformationStack );
	--m_pDeformationStackPtr;
	m_pDeformationStackPtr->m_nDeformationType = pDef->m_eType;

	switch( pDef->m_eType )
	{
		case DEFORMATION_CLAMP_TO_BOX_IN_WORLDSPACE:
		{
			BoxDeformation_t const *pBox = reinterpret_cast< const BoxDeformation_t *>( pDef );
			m_pDeformationStackPtr->m_nNumParameters = 16;
			memcpy( m_pDeformationStackPtr->m_flDeformationParameters, &( pBox->m_SourceMins.x ), 16 * sizeof( float ) );
			break;
		}
		break;
		
		default:
			Assert( 0 );
	}
}


void CShaderAPIDx8::PopDeformation( )
{
	Assert( m_pDeformationStackPtr != m_DeformationStack + DEFORMATION_STACK_DEPTH );
	++m_pDeformationStackPtr;
}

int CShaderAPIDx8::GetNumActiveDeformations( void ) const
{
	return ( m_DeformationStack + DEFORMATION_STACK_DEPTH ) - m_pDeformationStackPtr;
}

bool CShaderAPIDx8::IsStereoSupported() const
{
#if defined( _GAMECONSOLE )
	return false;
#else
	LOCK_SHADERAPI();
	return Dx9Device()->IsStereoSupported();
#endif
}

void CShaderAPIDx8::UpdateStereoTexture( ShaderAPITextureHandle_t texHandle, bool *pStereoActiveThisFrame )
{
#if defined( _GAMECONSOLE )
	return;
#else
	LOCK_SHADERAPI();

	if ( ( texHandle == INVALID_SHADERAPI_TEXTURE_HANDLE ) || !m_Textures.IsValidIndex( texHandle ) )
	{
		if ( pStereoActiveThisFrame )
		{
			*pStereoActiveThisFrame = false;
		}
		m_bIsStereoActiveThisFrame = false;
		return;
	}

	IDirect3DBaseTexture *baseTex = GetTexture( texHandle ).GetTexture();
	if ( ( baseTex == NULL ) || ( baseTex->GetType() != D3DRTYPE_TEXTURE ) )
	{
		if ( pStereoActiveThisFrame )
		{
			*pStereoActiveThisFrame = false;
		}
		m_bIsStereoActiveThisFrame = false;
		return;
	}

	Dx9Device()->UpdateStereoTexture( ( IDirect3DTexture9 * )baseTex, m_bQueuedDeviceLost, pStereoActiveThisFrame );

	if ( pStereoActiveThisFrame )
	{
		m_bIsStereoActiveThisFrame = *pStereoActiveThisFrame;
	}
#endif
}

// for shaders to set vertex shader constants. returns a packed state which can be used to set the dynamic combo
int CShaderAPIDx8::GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations,
													 float *pConstantValuesOut,
													 int nBufferSize,
													 int nMaximumDeformations,
													 int *pDefCombosOut ) const
{
	int nCombosFound = 0;
	memset( pDefCombosOut, 0, sizeof( pDefCombosOut[0] ) * nMaximumDeformations );
	size_t nRemainingBufferSize = nBufferSize;

	for( const Deformation_t *i = m_DeformationStack + DEFORMATION_STACK_DEPTH -1; i >= m_pDeformationStackPtr; i-- )
	{
		int nFloatsOut = 4 * ( ( i->m_nNumParameters + 3 )>> 2 );
		if ( 
			( ( 1 << i->m_nDeformationType )  & nMaskOfUnderstoodDeformations ) &&
			( nRemainingBufferSize >= ( nFloatsOut * sizeof( float ) ) ) )
		{
			memcpy( pConstantValuesOut, i->m_flDeformationParameters, nFloatsOut * sizeof( float ) );
			pConstantValuesOut += nFloatsOut;
			nRemainingBufferSize -= nFloatsOut * sizeof( float );
			( *pDefCombosOut++ ) = i->m_nDeformationType;
			nCombosFound++;
		}
	}
	return nCombosFound;
}




//-----------------------------------------------------------------------------
// Matrix stack operations
//-----------------------------------------------------------------------------

void CShaderAPIDx8::PushMatrix()
{
	// NOTE: No matrix transform update needed here.
	m_pMatrixStack[m_CurrStack]->Push();
}

void CShaderAPIDx8::PopMatrix()
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->Pop();
		UpdateMatrixTransform();
	}
	else
	{
		// Have to pop even while deactivated, otherwise the stack will overflow and we'll crash
		m_pMatrixStack[m_CurrStack]->Pop();
	}
}

void CShaderAPIDx8::LoadIdentity( )
{
	if (MatrixIsChanging(TRANSFORM_IS_IDENTITY))
	{
		m_pMatrixStack[m_CurrStack]->LoadIdentity( );
		UpdateMatrixTransform( TRANSFORM_IS_IDENTITY );
	}
}

void CShaderAPIDx8::LoadCameraToWorld( )
{
	if (MatrixIsChanging(TRANSFORM_IS_CAMERA_TO_WORLD))
	{
		// could just use the transpose instead, if we know there's no scale
		float det;
		D3DXMATRIX inv;
		D3DXMatrixInverse( &inv, &det, &GetTransform(MATERIAL_VIEW) );

		// Kill translation
		inv.m[3][0] = inv.m[3][1] = inv.m[3][2] = 0.0f;

		m_pMatrixStack[m_CurrStack]->LoadMatrix( &inv );
		UpdateMatrixTransform( TRANSFORM_IS_CAMERA_TO_WORLD );
	}
}

void CShaderAPIDx8::LoadMatrix( float *m )
{
	// Check for identity...
	if ( (fabs(m[0] - 1.0f) < 1e-3) && (fabs(m[5] - 1.0f) < 1e-3) && (fabs(m[10] - 1.0f) < 1e-3) && (fabs(m[15] - 1.0f) < 1e-3) &&
		 (fabs(m[1]) < 1e-3)  && (fabs(m[2]) < 1e-3)  && (fabs(m[3]) < 1e-3) &&
		 (fabs(m[4]) < 1e-3)  && (fabs(m[6]) < 1e-3)  && (fabs(m[7]) < 1e-3) &&
		 (fabs(m[8]) < 1e-3)  && (fabs(m[9]) < 1e-3)  && (fabs(m[11]) < 1e-3) &&
		 (fabs(m[12]) < 1e-3) && (fabs(m[13]) < 1e-3) && (fabs(m[14]) < 1e-3) )
	{
		LoadIdentity();
		return;
	}

	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->LoadMatrix( (D3DXMATRIX*)m );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::LoadBoneMatrix( int boneIndex, const float *m )
{
	if ( IsDeactivated() )	
		return;

	memcpy( m_boneMatrix[boneIndex].Base(), m, sizeof(float)*12 );
	if ( boneIndex > m_maxBoneLoaded )
	{
		m_maxBoneLoaded = boneIndex;
	}
	if ( boneIndex == 0 )
	{
		MatrixMode( MATERIAL_MODEL );
		VMatrix transposeMatrix;
		transposeMatrix.Init( *(matrix3x4_t *)m );
		MatrixTranspose( transposeMatrix, transposeMatrix );
		LoadMatrix( (float*)transposeMatrix.m );
	}
	m_DynamicState.m_InstanceInfo.m_bSetSkinConstants = false;
}

//-----------------------------------------------------------------------------
// Commits morph target factors
//-----------------------------------------------------------------------------
static void CommitFlexWeights( D3DDeviceWrapper *pDevice, const DynamicState_t &desiredState, 
									 DynamicState_t &currentState, bool bForce )
{
	if ( IsX360() )
	{
		// not supporting for 360
		return;
	}

	CommitVertexShaderConstantRange( pDevice, desiredState, currentState, bForce,
		VERTEX_SHADER_FLEX_WEIGHTS, VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT );
}

void CShaderAPIDx8::SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights )
{
	if ( IsX360() )
	{
		// not supported for 360
		return;
	}

	LOCK_SHADERAPI();
	if ( g_pHardwareConfig->Caps().m_NumVertexShaderConstants < VERTEX_SHADER_FLEX_WEIGHTS + VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT )
		return;

	if ( nFirstWeight + nCount > VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT )	
	{
		Warning( "Attempted to set too many flex weights! Max is %d\n", VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT );
		nCount = VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT - nFirstWeight;
	}

	if ( nCount <= 0 )
		return;

	float *pDest = m_DesiredState.m_pVectorVertexShaderConstant[ VERTEX_SHADER_FLEX_WEIGHTS + nFirstWeight ].Base();
	memcpy( pDest, pWeights, nCount * sizeof(MorphWeight_t) );

	ADD_COMMIT_FUNC( COMMIT_PER_DRAW, CommitFlexWeights );
}

void CShaderAPIDx8::MultMatrix( float *m )
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->MultMatrix( (D3DXMATRIX*)m );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::MultMatrixLocal( float *m )
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->MultMatrixLocal( (D3DXMATRIX*)m );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::Rotate( float angle, float x, float y, float z )
{
	if (MatrixIsChanging())
	{
		D3DXVECTOR3 axis( x, y, z );
		m_pMatrixStack[m_CurrStack]->RotateAxisLocal( &axis, M_PI * angle / 180.0f );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::Translate( float x, float y, float z )
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->TranslateLocal( x, y, z );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::Scale( float x, float y, float z )
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->ScaleLocal( x, y, z );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::ScaleXY( float x, float y )
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->ScaleLocal( x, y, 1.0f );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
	if (MatrixIsChanging())
	{
		D3DXMATRIX matrix;

		// FIXME: This is being used incorrectly! Should read:
		// D3DXMatrixOrthoOffCenterRH( &matrix, left, right, bottom, top, zNear, zFar );
		// Which is certainly why we need these extra -1 scales in y. Bleah

		// NOTE: The camera can be imagined as the following diagram:
		//		/z
		//	   /
		//	  /____ x	Z is going into the screen
		//	  |
		//	  |
		//	  |y
		//
		// (0,0,z) represents the upper-left corner of the screen.
		// Our projection transform needs to transform from this space to a LH coordinate
		// system that looks thusly:
		// 
		//	y|  /z
		//	 | /
		//	 |/____ x	Z is going into the screen
		//
		// Where x,y lies between -1 and 1, and z lies from 0 to 1
		// This is because the viewport transformation from projection space to pixels
		// introduces a -1 scale in the y coordinates
//		D3DXMatrixOrthoOffCenterLH( &matrix, left, right, bottom, top, zNear, zFar );

		D3DXMatrixOrthoOffCenterRH( &matrix, left, right, top, bottom, zNear, zFar );
		m_pMatrixStack[m_CurrStack]->MultMatrixLocal(&matrix);
		Assert( m_CurrStack == MATERIAL_PROJECTION );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::PerspectiveX( double fovx, double aspect, double zNear, double zFar )
{
	if (MatrixIsChanging())
	{
		float width = 2 * zNear * tan( fovx * M_PI / 360.0 );
		float height = width / aspect;
		Assert( m_CurrStack == MATERIAL_PROJECTION );
		D3DXMATRIX rh;
		D3DXMatrixPerspectiveRH( &rh, width, height, zNear, zFar );
		m_pMatrixStack[m_CurrStack]->MultMatrixLocal(&rh);
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right )
{
	if (MatrixIsChanging())
	{
		float width = 2 * zNear * tan( fovx * M_PI / 360.0 );
		float height = width / aspect;

		// bottom, top, left, right are 0..1 so convert to -1..1
		float flFrontPlaneLeft   = -(width/2.0f)  * (1.0f - left)   + left   * (width/2.0f);
		float flFrontPlaneRight  = -(width/2.0f)  * (1.0f - right)  + right  * (width/2.0f);
		float flFrontPlaneBottom = -(height/2.0f) * (1.0f - bottom) + bottom * (height/2.0f);
		float flFrontPlaneTop    = -(height/2.0f) * (1.0f - top)    + top    * (height/2.0f);

		Assert( m_CurrStack == MATERIAL_PROJECTION );
		D3DXMATRIX rh;
		D3DXMatrixPerspectiveOffCenterRH( &rh, flFrontPlaneLeft, flFrontPlaneRight, flFrontPlaneBottom, flFrontPlaneTop, zNear, zFar );
		m_pMatrixStack[m_CurrStack]->MultMatrixLocal(&rh);
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::PickMatrix( int x, int y, int width, int height )
{
	if (MatrixIsChanging())
	{
		Assert( m_CurrStack == MATERIAL_PROJECTION );

		// This is going to create a matrix to append to the standard projection.
		// Projection space goes from -1 to 1 in x and y. This matrix we append
		// will transform the pick region to -1 to 1 in projection space
		ShaderViewport_t viewport;
		GetViewports( &viewport, 1 );
		
		int vx = viewport.m_nTopLeftX;
		int vy = viewport.m_nTopLeftX;
		int vwidth = viewport.m_nWidth;
		int vheight = viewport.m_nHeight;

		// Compute the location of the pick region in projection space...
		float px = 2.0 * (float)(x - vx) / (float)vwidth - 1;
		float py = 2.0 * (float)(y - vy)/ (float)vheight - 1;
		float pw = 2.0 * (float)width / (float)vwidth;
		float ph = 2.0 * (float)height / (float)vheight;

		// we need to translate (px, py) to the origin
		// and scale so (pw,ph) -> (2, 2)
		D3DXMATRIX matrix;
		D3DXMatrixIdentity( &matrix );
		matrix.m[0][0] = 2.0 / pw;
		matrix.m[1][1] = 2.0 / ph;
		matrix.m[3][0] = -2.0 * px / pw;
		matrix.m[3][1] = -2.0 * py / ph;

		m_pMatrixStack[m_CurrStack]->MultMatrixLocal(&matrix);
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::GetMatrix( MaterialMatrixMode_t matrixMode, float *dst )
{
	void *pSrc = &GetTransform( matrixMode );
	memcpy( (void *)dst, pSrc, sizeof(D3DXMATRIX) );
}


//-----------------------------------------------------------------------------
// Did a transform change?
//-----------------------------------------------------------------------------
inline bool CShaderAPIDx8::VertexShaderTransformChanged( int i )
{
	return (m_DynamicState.m_TransformChanged[i] & STATE_CHANGED_VERTEX_SHADER) != 0;
}

const D3DXMATRIX &CShaderAPIDx8::GetProjectionMatrix( void )
{
	bool bUsingZBiasProjectionMatrix = 
		!g_pHardwareConfig->Caps().m_ZBiasAndSlopeScaledDepthBiasSupported &&
		( m_TransitionTable.CurrentSnapshot() != -1 ) &&
		m_TransitionTable.CurrentShadowState() &&
		m_TransitionTable.CurrentShadowState()->m_DepthTestState.m_ZBias;

	if ( !m_DynamicState.m_FastClipEnabled )
	{
		if ( bUsingZBiasProjectionMatrix )
			return m_CachedPolyOffsetProjectionMatrix;

		return GetTransform( MATERIAL_PROJECTION );
	}
	
	if ( bUsingZBiasProjectionMatrix )
		return m_CachedFastClipPolyOffsetProjectionMatrix;

	return m_CachedFastClipProjectionMatrix;
}


//-----------------------------------------------------------------------------
// Workaround hack for visualization of selection mode
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetupSelectionModeVisualizationState()
{
	Dx9Device()->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

	D3DXMATRIX ident;
	D3DXMatrixIdentity( &ident );
	SetVertexShaderConstantInternal( VERTEX_SHADER_VIEWPROJ, ident, 4 );
	SetVertexShaderConstantInternal( VERTEX_SHADER_MODELVIEWPROJ, ident, 4 );
	SetVertexShaderConstantInternal( VERTEX_SHADER_MODEL, ident, 3 * NUM_MODEL_TRANSFORMS );
}


//-----------------------------------------------------------------------------
// Set view transforms
//-----------------------------------------------------------------------------

static void printmat4x4( char *label, float *m00 )
{
	// print label..
	// fetch 4 from row, print as a row
	// fetch 4 from column, print as a row
	
#ifdef DX_TO_GL_ABSTRACTION
	float	row[4];
	float	col[4];
	
	GLMPRINTF(("-M-    -- %s --", label ));
	for( int n=0; n<4; n++ )
	{
		// extract row and column floats
		for( int i=0; i<4;i++)
		{
			row[i] = m00[(n*4)+i];			
			col[i] = m00[(i*4)+n];
		}
		GLMPRINTF((		"-M-    [ %7.4f %7.4f %7.4f %7.4f ] T=> [ %7.4f %7.4f %7.4f %7.4f ]",
						row[0],row[1],row[2],row[3],
						col[0],col[1],col[2],col[3]						
						));
	}
	GLMPRINTF(("-M-"));
#endif
}

void CShaderAPIDx8::SetVertexShaderViewProj()
{
	D3DXMATRIX	transpose, matView, matProj;
	
	matView		= GetTransform(MATERIAL_VIEW);
	matProj		= GetProjectionMatrix();
	transpose	= matView * matProj;

// PS3's Cg likes things in row-major rather than column-major, so let's just save ourselves the work of fixing every shader and call it even?
#ifndef _PS3
	D3DXMatrixTranspose( &transpose, &transpose );
#endif // _PS3
		
	SetVertexShaderConstantInternal( VERTEX_SHADER_VIEWPROJ, transpose, 4 );
}

void CShaderAPIDx8::SetVertexShaderModelViewProjAndModelView( void )
{
	D3DXMATRIX modelView, transpose, matView, matProj, matModel;
	
	matModel	= GetTransform(MATERIAL_MODEL);
	matView		= GetTransform(MATERIAL_VIEW);
	matProj		= GetProjectionMatrix();
	
	D3DXMatrixMultiply( &modelView, &matModel, &matView );
	D3DXMatrixMultiply( &transpose, &modelView, &matProj );

// PS3's Cg likes things in row-major rather than column-major, so let's just save ourselves the work of fixing every shader and call it even?
#ifndef _PS3
	D3DXMatrixTranspose( &transpose, &transpose );
#endif // !_PS3
	SetVertexShaderConstantInternal( VERTEX_SHADER_MODELVIEWPROJ, transpose, 4 );

	// If we're doing FastClip, the above modelviewproj matrix won't work well for
	// vertex shaders which compute projPos.z, hence we'll compute a more useful
	// modelviewproj and put the third row of it in another constant
	//D3DXMatrixMultiply( &transpose, &modelView, &GetTransform( MATERIAL_PROJECTION ) ); // Get the non-FastClip projection matrix
	//D3DXMatrixTranspose( &transpose, &transpose );
}

void CShaderAPIDx8::UpdateVertexShaderMatrix( int iMatrix )
{
	//GLM_FUNC;
	if ( iMatrix == 0 )
	{
		int matrix = MATERIAL_MODEL;
		if (VertexShaderTransformChanged(matrix))
		{
			int vertexShaderConstant = VERTEX_SHADER_MODEL + iMatrix * 3;

			// Put the transform into the vertex shader constants...
			D3DXMATRIX transpose;
			D3DXMatrixTranspose( &transpose, &GetTransform(matrix) );
			SetVertexShaderConstantInternal( vertexShaderConstant, transpose, 3 );

			// clear the change flag
			m_DynamicState.m_TransformChanged[matrix] &= ~STATE_CHANGED_VERTEX_SHADER;
		}
	}
	else
	{
		SetVertexShaderConstantInternal( VERTEX_SHADER_MODEL + iMatrix, m_boneMatrix[iMatrix].Base(), 3 );
	}
}


	//GLM_FUNC;
//-----------------------------------------------------------------------------
// Commits vertex shader transforms that can change on a per pass basis
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitPerPassVertexShaderTransforms()
{
	//GLMPRINTF(( ">-M- CommitPerPassVertexShaderTransforms" ));
	bool projChanged = VertexShaderTransformChanged( MATERIAL_PROJECTION );
	if ( projChanged )
	{
		//GLMPRINTF(( "-M- projChanged=true in CommitPerPassVertexShaderTransforms" ));
		SetVertexShaderViewProj();
		SetVertexShaderModelViewProjAndModelView();

		// Clear change flags
		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] &= ~STATE_CHANGED_VERTEX_SHADER;
	}
	else
	{		
		//GLMPRINTF(( "-M- projChanged=false in CommitPerPassVertexShaderTransforms" ));
	}

	//GLMPRINTF(( "<-M- CommitPerPassVertexShaderTransforms" ));
}

//-----------------------------------------------------------------------------
// Commits vertex shader transforms
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitVertexShaderTransforms()
{
	//GLMPRINTF(( ">-M- CommitVertexShaderTransforms" ));

	bool viewChanged = VertexShaderTransformChanged(MATERIAL_VIEW);
	bool projChanged = VertexShaderTransformChanged(MATERIAL_PROJECTION);
	bool modelChanged = VertexShaderTransformChanged(MATERIAL_MODEL) && (m_DynamicState.m_NumBones < 1);

	//GLMPRINTF(( "-M-     viewChanged=%s  projChanged=%s  modelChanged = %s in CommitVertexShaderTransforms", viewChanged?"Y":"N",projChanged?"Y":"N",modelChanged?"Y":"N" ));
	if (viewChanged)
	{
		//GLMPRINTF(( "-M-       viewChanged --> UpdateVertexShaderFogParams" ));
		UpdateVertexShaderFogParams();
	}

	if( viewChanged || projChanged )
	{
		// NOTE: We have to deal with fast-clip *before* 
		//GLMPRINTF(( "-M-       viewChanged||projChanged --> SetVertexShaderViewProj" ));
		SetVertexShaderViewProj();
	}

	if( viewChanged || modelChanged || projChanged )
	{
		//GLMPRINTF(( "-M-       viewChanged||projChanged||modelChanged --> SetVertexShaderModelViewProjAndModelView" ));
		SetVertexShaderModelViewProjAndModelView();
	}

	if( modelChanged && m_DynamicState.m_NumBones < 1 )
	{
		UpdateVertexShaderMatrix( 0 );
	}

	// Clear change flags
	m_DynamicState.m_TransformChanged[MATERIAL_MODEL]		&= ~STATE_CHANGED_VERTEX_SHADER;
	m_DynamicState.m_TransformChanged[MATERIAL_VIEW]		&= ~STATE_CHANGED_VERTEX_SHADER;
	m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION]	&= ~STATE_CHANGED_VERTEX_SHADER;

	//GLMPRINTF(( "<-M- CommitVertexShaderTransforms" ));
}


bool CShaderAPIDx8::IsRenderingInstances() const
{
	// Kind of hackery, but it works
	return ( m_pRenderCompiledState != &m_DynamicState.m_CompiledLightingState ); 
}

static Vector4D s_IdentityPoseToWorld[3] = 
{
	Vector4D( 1, 0, 0, 0 ),
	Vector4D( 0, 1, 0, 0 ),
	Vector4D( 0, 0, 1, 0 )
};

bool CShaderAPIDx8::SetSkinningMatrices( const MeshInstanceData_t &instance )
{
	if ( ( m_DynamicState.m_NumBones == 0 ) && !IsRenderingInstances() )
	{
#if defined( DX_TO_GL_ABSTRACTION )
		Dx9Device()->SetMaxUsedVertexShaderConstantsHint( VERTEX_SHADER_BONE_TRANSFORM( 0 ) + 3 );
#endif
		return false;
	}
	
	// We're changing the model matrix here, we must be force the next draw call to set it
	m_DynamicState.m_TransformChanged[MATERIAL_MODEL] = STATE_CHANGED;

	uint nMaxVertexConstantIndex = 0;

	if ( instance.m_pBoneRemap )
	{
		for ( int k = 0; k < instance.m_nBoneCount; ++k )
		{
			const int nDestBone = instance.m_pBoneRemap[k].m_nActualBoneIndex;
			const int nSrcBone = instance.m_pBoneRemap[k].m_nSrcBoneIndex;
			uint nIndex = VERTEX_SHADER_BONE_TRANSFORM( nDestBone );
			SetVertexShaderConstantInternal( nIndex, instance.m_pPoseToWorld[ nSrcBone ].Base(), 3, true );
			nMaxVertexConstantIndex = MAX( nMaxVertexConstantIndex, nIndex + 3 );
		}
	}
	else
	{
		// 0 bones can come in; static prop case
		if ( instance.m_pPoseToWorld )
		{
			int nBoneCount = MAX( 1, instance.m_nBoneCount );
			uint nIndex = VERTEX_SHADER_BONE_TRANSFORM( 0 );
			SetVertexShaderConstantInternal( nIndex, instance.m_pPoseToWorld[ 0 ].Base(), 3 * nBoneCount, true );
			nMaxVertexConstantIndex = MAX( nMaxVertexConstantIndex, nIndex + 3 * nBoneCount );
		}
		else
		{
			uint nIndex = VERTEX_SHADER_BONE_TRANSFORM( 0 );
			SetVertexShaderConstantInternal( nIndex, s_IdentityPoseToWorld[ 0 ].Base(), 3, true );
			nMaxVertexConstantIndex = MAX( nMaxVertexConstantIndex, nIndex + 3 );
		}
	}
#if defined( DX_TO_GL_ABSTRACTION )
	Dx9Device()->SetMaxUsedVertexShaderConstantsHint( nMaxVertexConstantIndex );
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Compiles the ambient cube lighting state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CompileAmbientCube( CompiledLightingState_t *pCompiledState, int nLightCount, const MaterialLightingState_t *pLightingState )
{
	Vector4D *vecCube = pCompiledState->m_AmbientLightCube;
	const Vector *pAmbient = pLightingState->m_vecAmbientCube;
	vecCube[0].Init( pAmbient[0].x, pAmbient[0].y, pAmbient[0].z, 1.0f );
	vecCube[1].Init( pAmbient[1].x, pAmbient[1].y, pAmbient[1].z, 1.0f );
	vecCube[2].Init( pAmbient[2].x, pAmbient[2].y, pAmbient[2].z, 1.0f );
	vecCube[3].Init( pAmbient[3].x, pAmbient[3].y, pAmbient[3].z, 1.0f );
	vecCube[4].Init( pAmbient[4].x, pAmbient[4].y, pAmbient[4].z, 1.0f );
	vecCube[5].Init( pAmbient[5].x, pAmbient[5].y, pAmbient[5].z, 1.0f );
}

//-----------------------------------------------------------------------------
// Fixes material light state so the first local light is a directional light, 
// so the CSM shadow occlusion term can always be applied to the first light.
//
// Modified to be skipped for statically lit meshes (occlusion term implicitly baked into the vertex colors)
// This also allows statically lit meshes to have one extra dynamic light.
//-----------------------------------------------------------------------------
static const MaterialLightingState_t *CanonicalizeMaterialLightingStateForCSM( const MaterialLightingState_t *pOrigLightingState, MaterialLightingState_t &newLightingState, bool bStaticLight )
{
	if ( !r_force_first_dynamic_light_to_directional_for_csm.GetBool() || bStaticLight )
		return pOrigLightingState;

	// Nothing to do if there are no local lights (the shader will be adding in no light at all, so the occlusion term can't affect anything.)
	if ( ( !pOrigLightingState->m_nLocalLightCount ) || ( !g_pHardwareConfig->SupportsCascadedShadowMapping() ) )
		return pOrigLightingState;

	// Also nothing to do if there's >= 1 light, and the first light is a directional light.
	if ( ( pOrigLightingState->m_nLocalLightCount >= 1 ) && ( pOrigLightingState->m_pLocalLightDesc[0].m_Type == MATERIAL_LIGHT_DIRECTIONAL ) )
		return pOrigLightingState;
	
	// Attempt to find a directional light in the list.
	int nDirectionalLightIndex = -1;
	for ( int i = 0; i < pOrigLightingState->m_nLocalLightCount; ++i)
	{
		if ( pOrigLightingState->m_pLocalLightDesc[i].m_Type == MATERIAL_LIGHT_DIRECTIONAL )
		{
			nDirectionalLightIndex = i;
			break;
		}
	}
	
	// Create the new material lighting state.
	V_memcpy( &newLightingState, pOrigLightingState, sizeof( MaterialLightingState_t ) - MATERIAL_MAX_LIGHT_COUNT * sizeof( LightDesc_t ) );
	V_memset( &newLightingState.m_pLocalLightDesc, 0, sizeof( newLightingState.m_pLocalLightDesc ) );
		
	if ( nDirectionalLightIndex == -1 )
	{
		// Couldn't find a directional light, so make an all-black dummy one in the first slot.
		newLightingState.m_pLocalLightDesc[0].InitDirectional( Vector( 0, 0, -1 ), Vector( 0, 0, 0 ) );
	}
	else
	{
		// Force the first slot to contain the directional light.
		newLightingState.m_pLocalLightDesc[0] = pOrigLightingState->m_pLocalLightDesc[nDirectionalLightIndex];
	}

	// Now copy as many local lights as possible into the new material lighting state.
	int nSrcLightIndex = 0;
	int nDestLightIndex = 1;
	while ( ( nSrcLightIndex < pOrigLightingState->m_nLocalLightCount ) && ( nDestLightIndex < MATERIAL_MAX_LIGHT_COUNT ) )
	{
		// Don't copy the light if its the directional light found and copied earlier.
		if ( nSrcLightIndex != nDirectionalLightIndex )
		{
			newLightingState.m_pLocalLightDesc[nDestLightIndex] = pOrigLightingState->m_pLocalLightDesc[nSrcLightIndex];
			nDestLightIndex++;
		}
		
		nSrcLightIndex++;
	}

	// Set the # of local lights in the new material lighting state.
	newLightingState.m_nLocalLightCount = nDestLightIndex;

	return &newLightingState;
}

//-----------------------------------------------------------------------------
// Generates the vertex shader constants for lights
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CompileVertexShaderLocalLights( CompiledLightingState_t *pCompiledState, int nLightCount, const MaterialLightingState_t *pOrigLightingState, bool bStaticLight )
{
	// For vertex shaders, we don't need to bother with the max instance lightcount
	// because we use static conditionals (NOTE: test this!! might be faster to
	// have lightcounts all be the same)
	
	MaterialLightingState_t canonicalizedLightingState;
	const MaterialLightingState_t *pLightingState = CanonicalizeMaterialLightingStateForCSM( pOrigLightingState, canonicalizedLightingState, bStaticLight );
	
	// We can just use the data for this specific instance
	nLightCount = MIN( pLightingState->m_nLocalLightCount, g_pHardwareConfig->MaxNumLights() );
	pCompiledState->m_nLocalLightCount = nLightCount;

	// Set the lighting state
	for ( int i = 0; i < nLightCount; ++i )
	{
		Vector4D *pDest = &pCompiledState->m_VertexShaderLocalLights[ i * 5 ];
		const LightDesc_t& light = pLightingState->m_pLocalLightDesc[ i ];
		Assert( light.m_Type != MATERIAL_LIGHT_DISABLE );

		// The first one is the light color (and light type code on DX9)
		float w = ( light.m_Type == MATERIAL_LIGHT_DIRECTIONAL ) ? 1.0f : 0.0f;
		pDest[0].Init( light.m_Color.x, light.m_Color.y, light.m_Color.z, w);

		// The next constant holds the light direction (and light type code on DX9)
		w = ( light.m_Type == MATERIAL_LIGHT_SPOT ) ? 1.0f : 0.0f;
		if ( light.m_Type == MATERIAL_LIGHT_POINT )
		{
			pDest[1].Init( 0, 0, 0, w );
		}
		else
		{
			pDest[1].Init( light.m_Direction.x, light.m_Direction.y, light.m_Direction.z, w );
		}

		// The next constant holds the light position
		pDest[2].Init( light.m_Position.x, light.m_Position.y, light.m_Position.z, 1.0f );

		// The next constant holds exponent, stopdot, stopdot2, 1 / (stopdot - stopdot2)
		if ( light.m_Type == MATERIAL_LIGHT_SPOT )
		{
			float oodot = light.OneOverThetaDotMinusPhiDot();
			pDest[3].Init( light.m_Falloff, light.m_ThetaDot, light.m_PhiDot, oodot );
		}
		else
		{
			pDest[3].Init( 0, 1, 1, 1 );
		}

		// The last constant holds attenuation0, attenuation1, attenuation2
		pDest[4].Init( light.m_Attenuation0, light.m_Attenuation1, light.m_Attenuation2, 0.0f );
	}

	// Vertex Shader loop counter for number of lights (Only the .x component is used by our shaders) 
	// .x is the iteration count, .y is the initial value and .z is the increment step 
	int *pLoopControl = pCompiledState->m_VertexShaderLocalLightLoopControl;
	pLoopControl[0] = nLightCount; pLoopControl[1] = 0; pLoopControl[2] = 1; pLoopControl[3] = 0;

	// Enable lights using vertex shader static flow control
	int *pEnable = pCompiledState->m_VertexShaderLocalLightEnable;
	for ( int i = 0; i < VERTEX_SHADER_LIGHT_ENABLE_BOOL_CONST_COUNT; ++i )
	{
		pEnable[i] = ( i < nLightCount ) ? 1 : 0;
	}
}

//-----------------------------------------------------------------------------
// Generates the pixel shader constants for lights
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CompilePixelShaderLocalLights( CompiledLightingState_t *pCompiledState, int nLightCount, const MaterialLightingState_t *pOrigLightingState, bool bStaticLight )
{
#ifndef NDEBUG
	const char *materialName = m_pMaterial->GetName();
#endif

	// Total pixel shader lighting state for four lights
	for ( int i = 0; i < 6; i++ )
	{
		pCompiledState->m_PixelShaderLocalLights[i].Init();
	}

	if ( !pOrigLightingState )
		return;

	MaterialLightingState_t canonicalizedLightingState;
	const MaterialLightingState_t *pLightingState = CanonicalizeMaterialLightingStateForCSM( pOrigLightingState, canonicalizedLightingState, bStaticLight );
	
	nLightCount = pLightingState->m_nLocalLightCount;

	// Offset to create a point light from directional
	const float fFarAway = 10000.0f;

	// NOTE: Use maxlights so we can render instances with different #s of lights
	// without needing to change shader
	const int nNumLights = pLightingState->m_nLocalLightCount;
	int nIterCount = MIN( nLightCount, 3 );
	for ( int i = 0; i < nIterCount; ++i )
	{
		int nIndex = 2 * i;
		const LightDesc_t *light = &pLightingState->m_pLocalLightDesc[i];
		Assert( light->m_Type != MATERIAL_LIGHT_DISABLE );
		pCompiledState->m_PixelShaderLocalLights[nIndex].Init( light->m_Color.x, light->m_Color.y, light->m_Color.z, 0.0f );

		if ( light->m_Type == MATERIAL_LIGHT_DIRECTIONAL )
		{
			VectorMA( pLightingState->m_vecLightingOrigin, -fFarAway, light->m_Direction, pCompiledState->m_PixelShaderLocalLights[nIndex + 1].AsVector3D() ); 
		}
		else
		{
			VectorCopy( light->m_Position, pCompiledState->m_PixelShaderLocalLights[nIndex + 1].AsVector3D() );
		}
	}

	if ( nNumLights > 3 ) // At least four lights (our current max)
	{
		const LightDesc_t *light = &pLightingState->m_pLocalLightDesc[3];
		// Spread 4th light's constants across w components
		pCompiledState->m_PixelShaderLocalLights[0].w = light->m_Color.x;
		pCompiledState->m_PixelShaderLocalLights[1].w = light->m_Color.y;
		pCompiledState->m_PixelShaderLocalLights[2].w = light->m_Color.z;

		if ( light->m_Type == MATERIAL_LIGHT_DIRECTIONAL )
		{
			Vector vPos;
			VectorMA( pLightingState->m_vecLightingOrigin, -fFarAway, light->m_Direction, vPos ); 
			pCompiledState->m_PixelShaderLocalLights[3].w = vPos.x;
			pCompiledState->m_PixelShaderLocalLights[4].w = vPos.y;
			pCompiledState->m_PixelShaderLocalLights[5].w = vPos.z;
		}
		else
		{
			pCompiledState->m_PixelShaderLocalLights[3].w = light->m_Position.x;
			pCompiledState->m_PixelShaderLocalLights[4].w = light->m_Position.y;
			pCompiledState->m_PixelShaderLocalLights[5].w = light->m_Position.z;
		}
	}
}



//-----------------------------------------------------------------------------
// Vertex Shader lighting
//-----------------------------------------------------------------------------
static float s_pTwoEmptyLights[40] = { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,1,1,1, 1,1,1,1, 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,1,1,1, 1,1,1,1 };

void CShaderAPIDx8::CommitVertexShaderLighting( CompiledLightingState_t *pLightingState )
{
	// Set the lighting state
	if ( pLightingState->m_nLocalLightCount > 0 )
	{
		SetVertexShaderConstantInternal( VERTEX_SHADER_LIGHTS, pLightingState->m_VertexShaderLocalLights[0].Base(), 5 * MIN( pLightingState->m_nLocalLightCount, g_pHardwareConfig->MaxNumLights() ) );
	}

	// Zero out subsequent lights if we don't support static control flow
	if ( ( pLightingState->m_nLocalLightCount < g_pHardwareConfig->MaxNumLights() ) && !g_pHardwareConfig->SupportsStaticControlFlow() )
	{
		int nLightsToSet = g_pHardwareConfig->MaxNumLights() - pLightingState->m_nLocalLightCount;

		// The following logic breaks if max lights is more than two
		Assert( g_pHardwareConfig->MaxNumLights() == 2 );

		SetVertexShaderConstantInternal( VERTEX_SHADER_LIGHTS + 5 * (g_pHardwareConfig->MaxNumLights() - nLightsToSet), s_pTwoEmptyLights, 5 * nLightsToSet );
	}
	
	// On PS3, we don't have integer constants, so the shader code relies on the boolean flags instead
	if ( !IsPS3() )
	{
		SetIntegerVertexShaderConstant( 0, pLightingState->m_VertexShaderLocalLightLoopControl, 1 );
	}

	SetBooleanVertexShaderConstant( VERTEX_SHADER_LIGHT_ENABLE_BOOL_CONST, pLightingState->m_VertexShaderLocalLightEnable, VERTEX_SHADER_LIGHT_ENABLE_BOOL_CONST_COUNT );
}

void CShaderAPIDx8::CommitPixelShaderLighting( int pshReg, CompiledLightingState_t *pLightingState )
{
	if( g_pHardwareConfig->MaxNumLights() == 2 )
	{
		SetPixelShaderConstantInternal( pshReg, pLightingState->m_PixelShaderLocalLights[0].Base(), 4, false );
	}
	else
	{
		SetPixelShaderConstantInternal( pshReg, pLightingState->m_PixelShaderLocalLights[0].Base(), 6, false );
	}
}


//-----------------------------------------------------------------------------
// Commits user clip planes
//-----------------------------------------------------------------------------
D3DXMATRIX& CShaderAPIDx8::GetUserClipTransform( )
{
	if ( !m_DynamicState.m_bUserClipTransformOverride )
		return GetTransform(MATERIAL_VIEW);

	return m_DynamicState.m_UserClipTransform;
}


//-----------------------------------------------------------------------------
// Commits user clip planes
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitUserClipPlanes( )
{
	// We need to transform the clip planes, specified in world space,
	// to be in projection space.. To transform the plane, we must transform
	// the intercept and then transform the normal.

	D3DXMATRIX worldToProjectionInvTrans;
#ifndef _DEBUG
	if( m_DynamicState.m_UserClipPlaneChanged & m_DynamicState.m_UserClipPlaneEnabled & ((1 << g_pHardwareConfig->MaxUserClipPlanes()) - 1) )
#endif
	{
		worldToProjectionInvTrans = GetUserClipTransform( ) * GetTransform( MATERIAL_PROJECTION );
		D3DXMatrixInverse(&worldToProjectionInvTrans, NULL, &worldToProjectionInvTrans);
		// PS3's Cg likes things in row-major rather than column-major, so let's just save ourselves the work of fixing every shader and call it even?
#ifndef _PS3
		D3DXMatrixTranspose(&worldToProjectionInvTrans, &worldToProjectionInvTrans);
#endif // !_PS3
	}

	for (int i = 0; i < g_pHardwareConfig->MaxUserClipPlanes(); ++i)
	{
		// Don't bother with the plane if it's not enabled
		if ( (m_DynamicState.m_UserClipPlaneEnabled & (1 << i)) == 0 )
			continue;

		// Don't bother if it didn't change...
		if ( (m_DynamicState.m_UserClipPlaneChanged & (1 << i)) == 0 )
		{
#ifdef _DEBUG
			//verify that the plane has not actually changed
			D3DXPLANE clipPlaneProj;
			D3DXPlaneTransform( &clipPlaneProj, &m_DynamicState.m_UserClipPlaneWorld[i], &worldToProjectionInvTrans );
			Assert ( clipPlaneProj == m_DynamicState.m_UserClipPlaneProj[i] );
#endif
			continue;
		}

		m_DynamicState.m_UserClipPlaneChanged &= ~(1 << i);		

		D3DXPLANE clipPlaneProj;
		D3DXPlaneTransform( &clipPlaneProj, &m_DynamicState.m_UserClipPlaneWorld[i], &worldToProjectionInvTrans );

		if ( clipPlaneProj != m_DynamicState.m_UserClipPlaneProj[i] )
		{
			Dx9Device()->SetClipPlane( i, (float*)clipPlaneProj );
			m_DynamicState.m_UserClipPlaneProj[i] = clipPlaneProj;
		}
	}
}


//-----------------------------------------------------------------------------
// Need to handle fog mode on a per-pass basis
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitPerPassFogMode( bool bUsingVertexAndPixelShaders )
{
	if ( IsGameConsole() )
	{
		// FF fog not applicable on 360 / PS3
		return;
	}

	D3DFOGMODE dxFogMode = D3DFOG_NONE;
	if ( m_DynamicState.m_FogEnable )
	{
		dxFogMode = bUsingVertexAndPixelShaders ? D3DFOG_NONE : D3DFOG_LINEAR;
	}

	// Set fog mode if it's different than before.
	if( m_DynamicState.m_FogMode != dxFogMode )
	{
		SetSupportedRenderState( D3DRS_FOGVERTEXMODE, dxFogMode );
		m_DynamicState.m_FogMode = dxFogMode;
	}
}

//-----------------------------------------------------------------------------
// Handle Xbox GPU/DX API fixups necessary before actual draw.
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitPerPassXboxFixups()
{
#if defined( _X360 )
	// send updated shader constants to gpu
	WriteShaderConstantsToGPU();

	// sRGB write state may have changed after RT set, have to re-set correct RT
	SetRenderTargetInternalXbox( m_hCachedRenderTarget );
#endif
}

//-----------------------------------------------------------------------------
// These states can change between each pass
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitPerPassStateChanges( StateSnapshot_t id )
{
	CommitPerPassVertexShaderTransforms();
	CommitPerPassFogMode( true );
	CommitPerPassXboxFixups();
	CallCommitFuncs( COMMIT_PER_PASS, false );
}


//-----------------------------------------------------------------------------
// Commits transforms and lighting
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitStateChanges()
{
	VPROF("CShaderAPIDx8::CommitStateChanges");
	CommitFastClipPlane();

	CommitVertexShaderTransforms();

	if ( m_DynamicState.m_UserClipPlaneEnabled )
	{
		CommitUserClipPlanes( );
	}

	CallCommitFuncs( COMMIT_PER_DRAW );
}

//-----------------------------------------------------------------------------
// Commits viewports
//-----------------------------------------------------------------------------
static void CommitSetViewports( D3DDeviceWrapper *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce )
{
	bool bChanged = bForce || memcmp( &desiredState.m_Viewport, &currentState.m_Viewport, sizeof(D3DVIEWPORT9) );

	// The width + height can be zero at startup sometimes.
	if ( bChanged && ( desiredState.m_Viewport.Width != 0 ) && ( desiredState.m_Viewport.Height != 0 ) )
	{
		if ( ReverseDepthOnX360() ) //reverse depth on 360 for better perf through hierarchical z
		{
			D3DVIEWPORT9 reverseDepthViewport;
			reverseDepthViewport = desiredState.m_Viewport;
			reverseDepthViewport.MinZ = 1.0f - desiredState.m_Viewport.MinZ;
			reverseDepthViewport.MaxZ = 1.0f - desiredState.m_Viewport.MaxZ;
			Dx9Device()->SetViewport( &reverseDepthViewport );
		}
		else
		{
			Dx9Device()->SetViewport( &desiredState.m_Viewport );
		}
		memcpy( &currentState.m_Viewport, &desiredState.m_Viewport, sizeof( D3DVIEWPORT9 ) );
	}
}


void CShaderAPIDx8::SetViewports( int nCount, const ShaderViewport_t* pViewports, bool setImmediately )
{
	Assert( nCount == 1 && pViewports[0].m_nVersion == SHADER_VIEWPORT_VERSION );
	if ( nCount != 1 )
		return;

	LOCK_SHADERAPI();

	D3DVIEWPORT9 viewport;
	viewport.X = pViewports[0].m_nTopLeftX;
	viewport.Y = pViewports[0].m_nTopLeftY;
	viewport.Width = pViewports[0].m_nWidth;
	viewport.Height = pViewports[0].m_nHeight;
	viewport.MinZ = pViewports[0].m_flMinZ; 
	viewport.MaxZ = pViewports[0].m_flMaxZ;

	// Clamp the viewport to the current render target...
	if ( !m_UsingTextureRenderTarget )
	{
		// Clamp to both the back buffer and the window, if it is resizing
		int nMaxWidth = 0, nMaxHeight = 0;
		GetBackBufferDimensions( nMaxWidth, nMaxHeight );
		if ( IsPC() && m_IsResizing )
		{
			RECT viewRect;
#if defined(_WIN32) && !defined( DX_TO_GL_ABSTRACTION )
			GetClientRect( ( HWND )m_ViewHWnd, &viewRect );
#else
			toglGetClientRect( (VD3DHWND)m_ViewHWnd, &viewRect );
#endif
			m_nWindowWidth = viewRect.right - viewRect.left;
			m_nWindowHeight = viewRect.bottom - viewRect.top;
			nMaxWidth = MIN( m_nWindowWidth, nMaxWidth );
			nMaxHeight = MIN( m_nWindowHeight, nMaxHeight );
		}

		// Dimensions can freak out on app exit, so at least make sure the viewport is positive
		if ( (viewport.Width > (unsigned int)nMaxWidth ) && (nMaxWidth > 0) )
		{
			viewport.Width = nMaxWidth;
		}

		// Dimensions can freak out on app exit, so at least make sure the viewport is positive
		if ( ( viewport.Height > (unsigned int)nMaxHeight ) && (nMaxHeight > 0) )
		{
			viewport.Height = nMaxHeight;
		}
	}
	else
	{
		if ( viewport.Width > (unsigned int)m_ViewportMaxWidth )
		{
			viewport.Width = m_ViewportMaxWidth;
		}
		if ( viewport.Height > (unsigned int)m_ViewportMaxHeight )
		{
			viewport.Height = m_ViewportMaxHeight;
		}
	}

	// FIXME: Once we extract buffered primitives out, we can directly fill in desired state
	// and avoid the memcmp and copy
	if ( memcmp( &m_DesiredState.m_Viewport, &viewport, sizeof(D3DVIEWPORT9) ) )
	{
		memcpy( &m_DesiredState.m_Viewport, &viewport, sizeof(D3DVIEWPORT9) );
	}

	if ( setImmediately )
	{
		 CommitSetViewports( Dx9Device(), m_DesiredState, m_DynamicState, false);
	}
	else
	{
		ADD_COMMIT_FUNC( COMMIT_PER_DRAW, CommitSetViewports );
	}
}


//-----------------------------------------------------------------------------
// Gets the current viewport size
//-----------------------------------------------------------------------------
int CShaderAPIDx8::GetViewports( ShaderViewport_t* pViewports, int nMax ) const
{
	if ( !pViewports || nMax == 0 )
		return 1;

	LOCK_SHADERAPI();

	pViewports[0].m_nTopLeftX = m_DesiredState.m_Viewport.X;
	pViewports[0].m_nTopLeftY = m_DesiredState.m_Viewport.Y;
	pViewports[0].m_nWidth = m_DesiredState.m_Viewport.Width;
	pViewports[0].m_nHeight = m_DesiredState.m_Viewport.Height;
	pViewports[0].m_flMinZ = m_DesiredState.m_Viewport.MinZ;
	pViewports[0].m_flMaxZ = m_DesiredState.m_Viewport.MaxZ;
	return 1;
}


//-----------------------------------------------------------------------------
// Flush the hardware
//-----------------------------------------------------------------------------
void CShaderAPIDx8::FlushHardware( )
{
	LOCK_SHADERAPI();

	Dx9Device()->EndScene();

	DiscardVertexBuffers();

	Dx9Device()->BeginScene();
	
	ForceHardwareSync();
}


	
//-----------------------------------------------------------------------------
// Deal with device lost (alt-tab)
//-----------------------------------------------------------------------------
void CShaderAPIDx8::HandleDeviceLost()
{
	if ( IsGameConsole() )
	{
		return;
	}

	LOCK_SHADERAPI();

	if ( !IsActive() )
		return;

	if ( !IsDeactivated() )
	{
		Dx9Device()->EndScene();
	}

	CheckDeviceLost( m_bOtherAppInitializing );

	if ( !IsDeactivated() )
	{
		Dx9Device()->BeginScene();
	}
}

//-----------------------------------------------------------------------------
// Buffer clear	color
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ClearColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	LOCK_SHADERAPI();
	float a = 255;//(r * 0.30f + g * 0.59f + b * 0.11f) / MAX_HDR_OVERBRIGHT;

	// GR - need to force alpha to black for HDR
	m_DynamicState.m_ClearColor = D3DCOLOR_ARGB((unsigned char)a,r,g,b);
}

void CShaderAPIDx8::ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	LOCK_SHADERAPI();
	m_DynamicState.m_ClearColor = D3DCOLOR_ARGB(a,r,g,b);
}

// Converts the clear color to be appropriate for HDR
D3DCOLOR CShaderAPIDx8::GetActualClearColor( D3DCOLOR clearColor )
{
	bool bConvert = !IsX360() && m_TransitionTable.CurrentState().m_bLinearColorSpaceFrameBufferEnable;

#if defined( _X360 )
	// The PC disables SRGBWrite when clearing so that the clear color won't get gamma converted
	// The 360 cannot disable that state, and thus compensates for the sRGB conversion
	// the desired result is the clear color written to the RT as-is
	if ( clearColor & D3DCOLOR_ARGB( 0, 255, 255, 255 ) )
	{
		IDirect3DSurface *pRTSurface = NULL;
		Dx9Device()->GetRenderTarget( 0, &pRTSurface );
		if ( pRTSurface )
		{
			D3DSURFACE_DESC desc;
			HRESULT hr = pRTSurface->GetDesc( &desc );
			if ( !FAILED( hr ) && IS_D3DFORMAT_SRGB( desc.Format ) )
			{
				bConvert = true;
			}
			pRTSurface->Release();
		}
	}
#endif

	if ( bConvert )
	{
		// HDRFIXME: need to make sure this works this way.
		// HDRFIXME: Is there a helper function that'll do this easier?
		// convert clearColor from gamma to linear since our frame buffer is linear.
		Vector vecGammaColor;
		vecGammaColor.x = ( 1.0f / 255.0f ) * ( ( clearColor >> 16 ) & 0xff );
		vecGammaColor.y = ( 1.0f / 255.0f ) * ( ( clearColor >> 8 ) & 0xff );
		vecGammaColor.z = ( 1.0f / 255.0f ) * ( clearColor & 0xff );
		Vector vecLinearColor;
		vecLinearColor.x = GammaToLinear( vecGammaColor.x );
		vecLinearColor.y = GammaToLinear( vecGammaColor.y );
		vecLinearColor.z = GammaToLinear( vecGammaColor.z );
		clearColor &= D3DCOLOR_RGBA( 0, 0, 0, 255 );
		clearColor |= D3DCOLOR_COLORVALUE( vecLinearColor.x, vecLinearColor.y, vecLinearColor.z, 0.0f );
	}

	return clearColor;
}


//-----------------------------------------------------------------------------
// Clear buffers while obeying stencil
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth )
{
	//copy the clear color bool into the clear alpha bool
	ClearBuffersObeyStencilEx( bClearColor, bClearColor, bClearDepth );
}

void CShaderAPIDx8::ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
{
	LOCK_SHADERAPI();

	if ( !bClearColor && !bClearAlpha && !bClearDepth )
		return;

	// Before clearing can happen, user clip planes must be disabled
	SetRenderState( D3DRS_CLIPPLANEENABLE, 0 );

	D3DCOLOR clearColor = GetActualClearColor( m_DynamicState.m_ClearColor );

	unsigned char r, g, b, a;
	b = clearColor& 0xFF;
	g = ( clearColor >> 8 ) & 0xFF;
	r = ( clearColor >> 16 ) & 0xFF;
	a = ( clearColor >> 24 ) & 0xFF;

	ShaderUtil()->DrawClearBufferQuad( r, g, b, a, bClearColor, bClearAlpha, bClearDepth );

	// Reset user clip plane state
	SetRenderState( D3DRS_CLIPPLANEENABLE, m_DynamicState.m_UserClipPlaneEnabled );
}

//-------------------------------------------------------------------------
//Perform stencil operations to every pixel on the screen
//-------------------------------------------------------------------------
void CShaderAPIDx8::PerformFullScreenStencilOperation( void )
{
	LOCK_SHADERAPI();

	// We'll be drawing a large quad in altered worldspace, user clip planes must be disabled
	SetRenderState( D3DRS_CLIPPLANEENABLE, 0 );

	ShaderUtil()->DrawClearBufferQuad( 0, 0, 0, 0, false, false, false );

	// Reset user clip plane state
	SetRenderState( D3DRS_CLIPPLANEENABLE, m_DynamicState.m_UserClipPlaneEnabled );
}


//-----------------------------------------------------------------------------
// Buffer clear
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight )
{
	LOCK_SHADERAPI();
	if ( ShaderUtil()->GetConfig().m_bSuppressRendering )
		return;

	if ( IsDeactivated() )
		return;

	// State changed... need to flush the dynamic buffer
	CallCommitFuncs( COMMIT_PER_DRAW, true );

	float depth = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? 0.0f : 1.0f;
	DWORD mask = 0;

	if ( bClearColor )
	{
		mask |= D3DCLEAR_TARGET;
	}

	if ( bClearDepth )
	{
		mask |= D3DCLEAR_ZBUFFER;
	}

	/*
	// HACK HACK HACK
	if ( bClearDepth )
	{
		bClearStencil = true;
	}
	*/


	ShaderStencilState_t stencilState;
	if ( bClearStencil && m_bUsingStencil )
	{
		mask |= D3DCLEAR_STENCIL;

#if defined( _X360 )
		// Clear hi stencil to 1 (for deferred shadows)
		// FIXME: Add possibility to set hi stencil clear value
		GetCurrentStencilState( &stencilState );
		SetSupportedRenderStateForce( D3DRS_HISTENCILENABLE, FALSE );
		SetSupportedRenderStateForce( D3DRS_HISTENCILWRITEENABLE, TRUE );
		SetSupportedRenderStateForce( D3DRS_HISTENCILFUNC, D3DHSCMP_NOTEQUAL ); // toggle func to EQUAL to clear to 0
		SetSupportedRenderStateForce( D3DRS_HISTENCILREF, 0 );
#endif
	}


	// Only clear the current view... right!??!
	D3DRECT clear;
	clear.x1 = m_DesiredState.m_Viewport.X;
	clear.y1 = m_DesiredState.m_Viewport.Y;
	clear.x2 = clear.x1 + m_DesiredState.m_Viewport.Width;
	clear.y2 = clear.y1 + m_DesiredState.m_Viewport.Height;

	// SRGBWrite is disabled when clearing so that the clear color won't get gamma converted
	bool bSRGBWriteEnable = false;
	if ( !IsX360() && bClearColor && m_TransitionTable.CurrentShadowState() )
	{
		bSRGBWriteEnable = m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_SRGBWriteEnable;
	}
	
#if !defined( _X360 )
	if ( bSRGBWriteEnable )
	{
#ifndef DX_TO_GL_ABSTRACTION
		Dx9Device()->SetRenderState( D3DRS_SRGBWRITEENABLE, 0 );
#endif
	}
#endif
	
	D3DCOLOR clearColor = GetActualClearColor( m_DynamicState.m_ClearColor );

	if ( mask != 0 )
	{
		bool bRenderTargetMatchesViewport = 
			( renderTargetWidth == -1 && renderTargetHeight == -1 ) ||
			( m_DesiredState.m_Viewport.Width == -1 && m_DesiredState.m_Viewport.Height == -1 ) ||
			( renderTargetWidth ==  ( int )m_DesiredState.m_Viewport.Width &&
			  renderTargetHeight == ( int )m_DesiredState.m_Viewport.Height );

		if ( bRenderTargetMatchesViewport )
		{
			RECORD_COMMAND( DX8_CLEAR, 6 );
			RECORD_INT( 0 );
			RECORD_STRUCT( &clear, sizeof(clear) );
			RECORD_INT( mask );
			RECORD_INT( clearColor );
			RECORD_FLOAT( depth );
			RECORD_INT( 0 );

			Dx9Device()->Clear( 0, NULL, mask, clearColor, depth, 0L );	
		}
		else
		{
			RECORD_COMMAND( DX8_CLEAR, 6 );
			RECORD_INT( 0 );
			RECORD_STRUCT( &clear, sizeof(clear) );
			RECORD_INT( mask );
			RECORD_INT( clearColor );
			RECORD_FLOAT( depth );
			RECORD_INT( 0 );

			Dx9Device()->Clear( 1, &clear, mask, clearColor, depth, 0L );	
		}
	}

#if defined( _X360 )
	if ( mask & D3DCLEAR_STENCIL )
	{
		// reset hi-stencil state
		SetStencilStateInternal( stencilState );
	}
#endif

	// Restore state
	if ( bSRGBWriteEnable )
	{
		// sRGBWriteEnable shouldn't be true if we have no shadow state. . . Assert just in case.
		Assert( m_TransitionTable.CurrentShadowState() );
		m_TransitionTable.ApplySRGBWriteEnable( *m_TransitionTable.CurrentShadowState() );
	}
}

//-----------------------------------------------------------------------------
// Bind
//-----------------------------------------------------------------------------
void CShaderAPIDx8::BindVertexShader( VertexShaderHandle_t hVertexShader )
{
	ShaderManager()->BindVertexShader( hVertexShader );
}

void CShaderAPIDx8::BindGeometryShader( GeometryShaderHandle_t hGeometryShader )
{
	Assert( hGeometryShader == GEOMETRY_SHADER_HANDLE_INVALID );
}

void CShaderAPIDx8::BindPixelShader( PixelShaderHandle_t hPixelShader )
{
	ShaderManager()->BindPixelShader( hPixelShader );
}


//-----------------------------------------------------------------------------
// Returns a copy of the front buffer
//-----------------------------------------------------------------------------
IDirect3DSurface* CShaderAPIDx8::GetFrontBufferImage( ImageFormat& format )
{
#if !defined( _X360 )
	int w, h;
	GetBackBufferDimensions( w, h );

	HRESULT hr;
	IDirect3DSurface *pFullScreenSurfaceBits = 0;
	hr = Dx9Device()->CreateOffscreenPlainSurface( w, h, 
		D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &pFullScreenSurfaceBits, NULL );
	if (FAILED(hr))
		return 0;

	hr = Dx9Device()->GetFrontBufferData( 0, pFullScreenSurfaceBits );
	if (FAILED(hr))
		return 0;

	int windowWidth, windowHeight;
	GetWindowSize( windowWidth, windowHeight );
	
	IDirect3DSurface *pSurfaceBits = 0;
	hr = Dx9Device()->CreateOffscreenPlainSurface( windowWidth, windowHeight,
		D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &pSurfaceBits, NULL );
	Assert( hr == D3D_OK );
	
	POINT pnt;
	pnt.x = pnt.y = 0;
#ifdef _WIN32
	BOOL result = ClientToScreen( ( HWND )m_hWnd, &pnt );
#else
	BOOL result = ClientToScreen( (VD3DHWND)m_hWnd, &pnt );
#endif
	Assert( result );

	RECT srcRect;
	srcRect.left = pnt.x;
	srcRect.top = pnt.y;
	srcRect.right = pnt.x + windowWidth;
	srcRect.bottom = pnt.y + windowHeight;

	POINT dstPnt;
	dstPnt.x = dstPnt.y = 0;
	
	D3DLOCKED_RECT lockedSrcRect;
	hr = pFullScreenSurfaceBits->LockRect( &lockedSrcRect, &srcRect, D3DLOCK_READONLY );
	Assert( hr == D3D_OK );
	
	D3DLOCKED_RECT lockedDstRect;
	hr = pSurfaceBits->LockRect( &lockedDstRect, NULL, 0 );
	Assert( hr == D3D_OK );

	int i;
	for( i = 0; i < windowHeight; i++ )
	{
		memcpy( ( unsigned char * )lockedDstRect.pBits + ( i * lockedDstRect.Pitch ), 
			    ( unsigned char * )lockedSrcRect.pBits + ( i * lockedSrcRect.Pitch ),
				windowWidth * 4 ); // hack . .  what if this is a different format?
	}
	hr = pSurfaceBits->UnlockRect();
	Assert( hr == D3D_OK );
	hr = pFullScreenSurfaceBits->UnlockRect();
	Assert( hr == D3D_OK );

	pFullScreenSurfaceBits->Release();

	format = ImageLoader::D3DFormatToImageFormat( D3DFMT_A8R8G8B8 );
	return pSurfaceBits;
#else
	Assert( 0 );
	return NULL;
#endif
}


//-----------------------------------------------------------------------------
// Lets the shader know about the full-screen texture so it can 
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetFullScreenTextureHandle( ShaderAPITextureHandle_t h )
{
	LOCK_SHADERAPI();
	m_hFullScreenTexture = h;
}


//-----------------------------------------------------------------------------
// Lets the shader know about the full-screen texture so it can 
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture )
{
	LOCK_SHADERAPI();

	m_hLinearToGammaTableTexture = hSRGBWriteEnabledTexture;
	m_hLinearToGammaTableIdentityTexture = hIdentityTexture;
}

//-----------------------------------------------------------------------------
// Returns a copy of the back buffer
//-----------------------------------------------------------------------------
IDirect3DSurface* CShaderAPIDx8::GetBackBufferImageHDR( Rect_t *pSrcRect, Rect_t *pDstRect, ImageFormat& format )
{
#if !defined( _X360 )
	HRESULT hr;
	IDirect3DSurface *pSurfaceBits = 0;
	IDirect3DSurface *pTmpSurface = NULL;

	// Get the back buffer
	IDirect3DSurface* pBackBuffer;
	hr = Dx9Device()->GetRenderTarget( 0, &pBackBuffer );
	if (FAILED(hr))
		return 0;

	// Find about its size and format
	D3DSURFACE_DESC desc;
	D3DTEXTUREFILTERTYPE filter;
	
	hr = pBackBuffer->GetDesc( &desc );
	if (FAILED(hr))
		goto CleanUp;

	filter = ((pDstRect->width != pSrcRect->width) || (pDstRect->height != pSrcRect->height)) ? D3DTEXF_LINEAR : D3DTEXF_NONE;

	if ( ( pDstRect->x + pDstRect->width <= SMALL_BACK_BUFFER_SURFACE_WIDTH ) && 
		 ( pDstRect->y + pDstRect->height <= SMALL_BACK_BUFFER_SURFACE_HEIGHT ) )
	{
		if (!m_pSmallBackBufferFP16TempSurface)
		{
			hr = Dx9Device()->CreateRenderTarget( 
				SMALL_BACK_BUFFER_SURFACE_WIDTH, SMALL_BACK_BUFFER_SURFACE_HEIGHT, 
				desc.Format, D3DMULTISAMPLE_NONE, 0, TRUE, &m_pSmallBackBufferFP16TempSurface,
				NULL );
		}
		pTmpSurface = m_pSmallBackBufferFP16TempSurface;
		pTmpSurface->AddRef();

		desc.Width = SMALL_BACK_BUFFER_SURFACE_WIDTH;
		desc.Height = SMALL_BACK_BUFFER_SURFACE_HEIGHT;

		RECT srcRect, destRect;
		RectToRECT( pSrcRect, srcRect );
		RectToRECT( pDstRect, destRect );
		hr = Dx9Device()->StretchRect( pBackBuffer, &srcRect, pTmpSurface, &destRect, filter );
		if ( FAILED(hr) )
			goto CleanUp;
	}
	else
	{
		// Normally we would only have to create a separate render target here and StretchBlt to it first
		// if AA was enabled, but certain machines/drivers get reboots if we do GetRenderTargetData 
		// straight off the backbuffer.
		hr = Dx9Device()->CreateRenderTarget( desc.Width, desc.Height, desc.Format,
			D3DMULTISAMPLE_NONE, 0, TRUE, &pTmpSurface, NULL );
		if ( FAILED(hr) )
			goto CleanUp;

		hr = Dx9Device()->StretchRect( pBackBuffer, NULL, pTmpSurface, NULL, filter );
		if ( FAILED(hr) )
			goto CleanUp;
	}

	// Create a buffer the same size and format
	hr = Dx9Device()->CreateOffscreenPlainSurface( desc.Width, desc.Height, 
		desc.Format, D3DPOOL_SYSTEMMEM, &pSurfaceBits, NULL );
	if (FAILED(hr))
		goto CleanUp;

	// Blit from the back buffer to our scratch buffer
	hr = Dx9Device()->GetRenderTargetData( pTmpSurface ? pTmpSurface : pBackBuffer, pSurfaceBits );
	if (FAILED(hr))
		goto CleanUp2;

	format = ImageLoader::D3DFormatToImageFormat(desc.Format);
	if ( pTmpSurface )
	{
		pTmpSurface->Release();
	}
	pBackBuffer->Release();
	return pSurfaceBits;

CleanUp2:
	pSurfaceBits->Release();

CleanUp:
	if ( pTmpSurface )
	{
		pTmpSurface->Release();
	}

	pBackBuffer->Release();
#else
	Assert( 0 );
#endif
	return 0;
}


//-----------------------------------------------------------------------------
// Returns a copy of the back buffer
//-----------------------------------------------------------------------------
IDirect3DSurface* CShaderAPIDx8::GetBackBufferImage( Rect_t *pSrcRect, Rect_t *pDstRect, ImageFormat& format )
{
#if defined( _PS3 )
	IDirect3DSurface *pSurf = m_pBackBufferSurfaces[BACK_BUFFER_INDEX_DEFAULT];
	if ( ( pSurf ) && 
		( pSrcRect->width == pDstRect->width ) && ( pSrcRect->height == pDstRect->height ) && 
		( pSrcRect->x == 0 ) && ( pSrcRect->y == 0 ) && 
		( pDstRect->x == 0 ) && ( pDstRect->y == 0 ) )
	{
		D3DSURFACE_DESC desc;
		HRESULT hr = pSurf->GetDesc( &desc );
		if ( !FAILED( hr ) )
		{
			format = ImageLoader::D3DFormatToImageFormat( desc.Format );

			// The actual surface data on PS3 is big endian, and the caller expects little endian data, so flip the component order. (Yes this is a big hack.)
			// This is the simplest/least intrusive solution to get savegame screenshots working on ps3 I could think of.
			if ( format == IMAGE_FORMAT_BGRA8888 )
			{
				format = IMAGE_FORMAT_ARGB8888;
			}
			else if ( format == IMAGE_FORMAT_ARGB8888 )
			{
				format = IMAGE_FORMAT_BGRA8888;
			}
			else if ( format == IMAGE_FORMAT_RGBA8888 )
			{
				format = IMAGE_FORMAT_ABGR8888;
			}
			else if ( format == IMAGE_FORMAT_ABGR8888 )
			{
				format = IMAGE_FORMAT_RGBA8888;
			}
			else
			{
				AssertOnce( "Unsupported backbuffer format in GetBackBufferImage\n" );
			}

			pSurf->AddRef();

			// For safety, wait until the GPU backend finishes writing to the backbuffer.
			IDirect3DQuery9 *pQuery = NULL;
			Dx9Device()->CreateQuery( D3DQUERYTYPE_EVENT, &pQuery );
			if ( pQuery )
			{
				pQuery->Issue( D3DISSUE_END );
				
				BOOL bQueryResult;
				pQuery->GetData( &bQueryResult, sizeof( bQueryResult ), D3DGETDATA_FLUSH );
				
				pQuery->Release();

				__sync();
			}

			return pSurf;
		}
	}
	return NULL;
#elif !defined( _X360 )
	if ( !m_pBackBufferSurfaces[BACK_BUFFER_INDEX_DEFAULT] || ( m_hFullScreenTexture == INVALID_SHADERAPI_TEXTURE_HANDLE ) )
		return NULL;

	HRESULT hr;
	D3DSURFACE_DESC desc;

	// Get the current render target
	IDirect3DSurface* pRenderTarget;
	hr = Dx9Device()->GetRenderTarget( 0, &pRenderTarget );
	if (FAILED(hr))
		return 0;

	// Find about its size and format
	hr = pRenderTarget->GetDesc( &desc );

	if ( desc.Format == D3DFMT_A16B16G16R16F || desc.Format == D3DFMT_A32B32G32R32F )
		return GetBackBufferImageHDR( pSrcRect, pDstRect, format );

	IDirect3DSurface *pSurfaceBits = NULL;
	IDirect3DSurface *pTmpSurface = NULL;
	int nRenderTargetRefCount;
	REFERENCE( nRenderTargetRefCount );

	if ( (desc.MultiSampleType == D3DMULTISAMPLE_NONE) && (pRenderTarget != m_pBackBufferSurfaces[BACK_BUFFER_INDEX_DEFAULT]) && 
		 (pSrcRect->width == pDstRect->width) && (pSrcRect->height == pDstRect->height) )
	{
		// Don't bother to blit through the full-screen texture if we don't
		// have to stretch, we're not coming from the backbuffer, and we don't have to do AA resolve
		pTmpSurface = pRenderTarget;
		pTmpSurface->AddRef();
	}
	else
	{
		Texture_t *pTex = &GetTexture( m_hFullScreenTexture );
		IDirect3DTexture* pFullScreenTexture = (IDirect3DTexture*)pTex->GetTexture();

		D3DTEXTUREFILTERTYPE filter = ((pDstRect->width != pSrcRect->width) || (pDstRect->height != pSrcRect->height)) ? D3DTEXF_LINEAR : D3DTEXF_NONE;

		hr = pFullScreenTexture->GetSurfaceLevel( 0, &pTmpSurface ); 
		if ( FAILED(hr) )
			goto CleanUp;

		if ( pTmpSurface == pRenderTarget )
		{
			Warning( "Can't blit from full-sized offscreen buffer!\n" );
			goto CleanUp;
		}

		RECT srcRect, destRect;
		srcRect.left = pSrcRect->x; srcRect.right = pSrcRect->x + pSrcRect->width;
		srcRect.top = pSrcRect->y; srcRect.bottom = pSrcRect->y + pSrcRect->height;
		srcRect.left = clamp( srcRect.left, 0, (int)desc.Width );
		srcRect.right = clamp( srcRect.right, 0, (int)desc.Width );
		srcRect.top = clamp( srcRect.top, 0, (int)desc.Height );
		srcRect.bottom = clamp( srcRect.bottom, 0, (int)desc.Height );

		destRect.left = pDstRect->x ; destRect.right = pDstRect->x + pDstRect->width;
		destRect.top = pDstRect->y; destRect.bottom = pDstRect->y + pDstRect->height;
		destRect.left = clamp( destRect.left, 0, (int)desc.Width );
		destRect.right = clamp( destRect.right, 0, (int)desc.Width );
		destRect.top = clamp( destRect.top, 0, (int)desc.Height );
		destRect.bottom = clamp( destRect.bottom, 0, (int)desc.Height );

		hr = Dx9Device()->StretchRect( pRenderTarget, &srcRect, pTmpSurface, &destRect, filter );
		if ( FAILED(hr) )
		{
			AssertOnce( "Error resizing pixels!\n" );
			goto CleanUp;
		}
	}

	D3DSURFACE_DESC tmpDesc;
	hr = pTmpSurface->GetDesc( &tmpDesc );
	Assert( !FAILED(hr) );

	// Create a buffer the same size and format
	hr = Dx9Device()->CreateOffscreenPlainSurface( tmpDesc.Width, tmpDesc.Height, 
		desc.Format, D3DPOOL_SYSTEMMEM, &pSurfaceBits, NULL );
	if ( FAILED(hr) )
	{
		AssertOnce( "Error creating offscreen surface!\n" );
		goto CleanUp;
	}

	// Blit from the back buffer to our scratch buffer
	hr = Dx9Device()->GetRenderTargetData( pTmpSurface, pSurfaceBits );
	if ( FAILED(hr) )
	{
		AssertOnce( "Error copying bits into the offscreen surface!\n" );
		goto CleanUp;
	}

	format = ImageLoader::D3DFormatToImageFormat( desc.Format );

	pTmpSurface->Release();
#ifdef _DEBUG
	nRenderTargetRefCount = 
#endif
		pRenderTarget->Release();
	Assert( nRenderTargetRefCount == 1 );
	return pSurfaceBits;

CleanUp:
	if ( pSurfaceBits )
	{
		pSurfaceBits->Release();
	}

	if ( pTmpSurface )
	{
		pTmpSurface->Release();
	}
#else
	Assert( 0 );
#endif

	return 0;
}


//-----------------------------------------------------------------------------
// Copy bits from a host-memory surface
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CopyBitsFromHostSurface( IDirect3DSurface* pSurfaceBits, 
	const Rect_t &dstRect, unsigned char *pData, ImageFormat srcFormat, ImageFormat dstFormat, int nDstStride )
{
	// Copy out the bits...
	RECT rect;
	rect.left   = dstRect.x; 
	rect.right  = dstRect.x + dstRect.width; 
	rect.top    = dstRect.y; 
	rect.bottom = dstRect.y + dstRect.height;

	D3DLOCKED_RECT lockedRect;
	HRESULT hr;
	int flags = D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK;
	hr = pSurfaceBits->LockRect( &lockedRect, &rect, flags );
	if ( !FAILED( hr ) )
	{
		unsigned char *pImage = (unsigned char *)lockedRect.pBits;
		ShaderUtil()->ConvertImageFormat( (unsigned char *)pImage, srcFormat,
			pData, dstFormat, dstRect.width, dstRect.height, lockedRect.Pitch, nDstStride );

		hr = pSurfaceBits->UnlockRect( );
	}
}


//-----------------------------------------------------------------------------
// Reads from the current read buffer  + stretches
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pData, ImageFormat dstFormat, int nDstStride )
{
	LOCK_SHADERAPI();
	Assert( pDstRect );
	
	if ( IsPC() || !IsX360() )
	{
		Rect_t srcRect;
		if ( !pSrcRect )
		{
			srcRect.x = srcRect.y = 0;
			srcRect.width = m_nWindowWidth;
			srcRect.height = m_nWindowHeight;
			pSrcRect = &srcRect;
		}

		ImageFormat format;
		IDirect3DSurface* pSurfaceBits = GetBackBufferImage( pSrcRect, pDstRect, format );
		if ( pSurfaceBits )
		{
			CopyBitsFromHostSurface( pSurfaceBits, *pDstRect, pData, format, dstFormat, nDstStride );
		
			// Release the temporary surface
			pSurfaceBits->Release();
		}
	}
	else
	{
#if defined( _X360 )
		// 360 requires material system to handle due to RT complexities
		ShaderUtil()->ReadBackBuffer( pSrcRect, pDstRect, pData, dstFormat, nDstStride );
#endif
	}
}


//-----------------------------------------------------------------------------
// Reads from the current read buffer
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReadPixels( int x, int y, int width, int height, unsigned char *pData, ImageFormat dstFormat, ITexture *pRenderTargetTexture )
{
	Rect_t rect;
	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;

	if ( IsPC() || !IsX360() )
	{
		ImageFormat format;
		IDirect3DSurface* pSurfaceBits = NULL;

		if ( pRenderTargetTexture != NULL )
		{
			format = pRenderTargetTexture->GetImageFormat();
			ShaderAPITextureHandle_t hRenderTargetTexture = ShaderUtil()->GetShaderAPITextureBindHandle( pRenderTargetTexture, 0, 0 );

			//render targets can't be locked, luckily we can copy the surface to system memory and lock that.
			IDirect3DSurface *pSystemSurface;
			IDirect3DSurface* pRTSurface = GetTextureSurface( hRenderTargetTexture );

			D3DSURFACE_DESC surfaceDesc;
			pRTSurface->GetDesc( &surfaceDesc );

			Assert( !IsX360() );

			HRESULT hr = Dx9Device()->CreateOffscreenPlainSurface( surfaceDesc.Width, surfaceDesc.Height, surfaceDesc.Format, D3DPOOL_SYSTEMMEM, &pSystemSurface, NULL );
			Assert( SUCCEEDED( hr ) );

			if ( pSystemSurface != NULL )
			{
				pSystemSurface->GetDesc( &surfaceDesc );

				hr = Dx9Device()->GetRenderTargetData( pRTSurface, pSystemSurface );
				Assert( SUCCEEDED( hr ) );
			}

			//pretend this is the texture level we originally grabbed with GetSurfaceLevel() and continue on
			pRTSurface->Release();
			pSurfaceBits = pSystemSurface;
		}
		else
		{
			pSurfaceBits = GetBackBufferImage( &rect, &rect, format );
		}
		if (pSurfaceBits)
		{
			CopyBitsFromHostSurface( pSurfaceBits, rect, pData, format, dstFormat, 0 );
		
			// Release the temporary surface
			pSurfaceBits->Release();
		}
	}
	else
	{
#if defined( _X360 )
		// 360 requires material system to handle due to RT complexities
		ShaderUtil()->ReadBackBuffer( &rect, &rect, pData, dstFormat, 0 );
#endif
	}
}

static IDirect3DSurface *s_pSystemSurface = NULL;
static ImageFormat s_format;

// this is not fully async, it syncronizes the queue
// it does get queued and run from the render thread (if enabled)
void CShaderAPIDx8::ReadPixelsAsync( int x, int y, int width, int height, unsigned char *pData, ImageFormat dstFormat, ITexture *pRenderTargetTexture, CThreadEvent *pPixelsReadEvent )
{
	TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "%s", __FUNCTION__ );

	Rect_t rect;
	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;

	if ( IsPC() || !IsX360() )
	{
		if ( pRenderTargetTexture != NULL )
		{
			s_format = pRenderTargetTexture->GetImageFormat();
			ShaderAPITextureHandle_t hRenderTargetTexture = ShaderUtil()->GetShaderAPITextureBindHandle( pRenderTargetTexture, 0, 0 );

			//render targets can't be locked, luckily we can copy the surface to system memory and lock that.
			if ( IDirect3DSurface* pRTSurface = GetTextureSurface( hRenderTargetTexture ) )
			{
				D3DSURFACE_DESC surfaceDesc;
				pRTSurface->GetDesc( &surfaceDesc );

				Assert( !IsX360() );

				HRESULT hr = Dx9Device()->CreateOffscreenPlainSurface( surfaceDesc.Width, surfaceDesc.Height, surfaceDesc.Format, D3DPOOL_SYSTEMMEM, &s_pSystemSurface, NULL );
				Assert( SUCCEEDED( hr ) );

				if ( s_pSystemSurface != NULL )
				{
					s_pSystemSurface->GetDesc( &surfaceDesc );

					hr = Dx9Device()->GetRenderTargetData( pRTSurface, s_pSystemSurface );
					Assert( SUCCEEDED( hr ) );
				}

				//pretend this is the texture level we originally grabbed with GetSurfaceLevel() and continue on
				pRTSurface->Release();
			}
		}
		else
		{
			s_pSystemSurface = GetBackBufferImage( &rect, &rect, s_format );
		}
	}
	else
	{
#if defined( _X360 )
		Assert( 0 ); // not supported
#endif
	}

	// if the caller gave us a thread event, then signal the event
	if ( pPixelsReadEvent )
	{
		pPixelsReadEvent->Set();
	}
}

void CShaderAPIDx8::ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *pData, ImageFormat dstFormat, CThreadEvent *pGetResultEvent )
{
	TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "%s", __FUNCTION__ );

	Rect_t rect;
	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;

	if (s_pSystemSurface)
	{
		CopyBitsFromHostSurface( s_pSystemSurface, rect, pData, s_format, dstFormat, 0 );
		
		// Release the temporary surface
		s_pSystemSurface->Release();
	}

	// if the caller gave us a thread event, then signal the event
	if ( pGetResultEvent )
	{
		pGetResultEvent->Set();
	}
}

//-----------------------------------------------------------------------------
// Binds a particular material to render with
//-----------------------------------------------------------------------------
void CShaderAPIDx8::Bind( IMaterial* pMaterial )
{
	LOCK_SHADERAPI();
	IMaterialInternal* pMatInt = static_cast<IMaterialInternal*>( pMaterial );

	bool bMaterialChanged;
	if ( m_pMaterial && pMatInt && m_pMaterial->InMaterialPage() && pMatInt->InMaterialPage() )
	{
		bMaterialChanged = ( m_pMaterial->GetMaterialPage() != pMatInt->GetMaterialPage() );
	}
	else
	{
		bMaterialChanged = ( m_pMaterial != pMatInt ) || ( m_pMaterial && m_pMaterial->InMaterialPage() ) || ( pMatInt && pMatInt->InMaterialPage() );
	}

	if ( bMaterialChanged )
	{
#ifdef RECORDING
		RECORD_DEBUG_STRING( ( char * )pMaterial->GetName() );
		IShader *pShader = pMatInt->GetShader();
		if( pShader && pShader->GetName() )
		{
			RECORD_DEBUG_STRING( pShader->GetName() );
		}
		else
		{
			RECORD_DEBUG_STRING( "<NULL SHADER>" );
		}
#endif
		m_pMaterial = pMatInt;

#if ( defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD ) || ( defined( PLATFORM_POSIX ) && GLMDEBUG ) )
		PIXifyName( s_pPIXMaterialName, m_pMaterial->GetName() );
#endif

#ifdef _GAMECONSOLE
		if ( m_bInZPass )
		{
			if ( pMatInt->IsAlphaTested() || pMatInt->IsTranslucent() )
			{
				// we need to predicate render calls with this material to only occur during main rendering and not the z pass
				Dx9Device()->SetPredication( D3DPRED_ALL_RENDER );
			}
			else
			{
				Dx9Device()->SetPredication( 0 );
			}
		}
#endif
	}
}

// Get the currently bound material
IMaterialInternal* CShaderAPIDx8::GetBoundMaterial()
{
	return m_pMaterial;
}

//-----------------------------------------------------------------------------
// Binds a standard texture
//-----------------------------------------------------------------------------

#ifndef _PS3

void CShaderAPIDx8::BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id )
{
	if ( IsRenderingInstances() )
	{
		// when rendering instances, if they are binding one of the per-instance replacable std
		// textures, we don't want to bind it - we just want to remember what sampler it was bound
		// to so we can plug in the correct texture at the time we draw the instances.

		int nMask = ( 1 << sampler );
		m_DynamicState.m_nLocalEnvCubemapSamplers &= ~nMask;
		m_DynamicState.m_nLightmapSamplers &= ~nMask;
		m_DynamicState.m_nPaintmapSamplers &= ~nMask;

		if ( id == TEXTURE_LOCAL_ENV_CUBEMAP )
		{
			m_DynamicState.m_nLocalEnvCubemapSamplers |= nMask;
			SetLastSetTextureBindFlags( sampler, nBindFlags );
			return;
		}

		if ( id == TEXTURE_LIGHTMAP )
		{
			m_DynamicState.m_nLightmapSamplers |= nMask;
			SetLastSetTextureBindFlags( sampler, nBindFlags );
			return;
		}

		if ( id == TEXTURE_PAINT )
		{
			m_DynamicState.m_nPaintmapSamplers |= nMask;
			SetLastSetTextureBindFlags( sampler, nBindFlags );
			return;
		}
	}

	if ( m_StdTextureHandles[id] != INVALID_SHADERAPI_TEXTURE_HANDLE )
	{
		BindTexture( sampler, nBindFlags, m_StdTextureHandles[id] );
	}
	else
	{
		ShaderUtil()->BindStandardTexture( sampler, nBindFlags, id );
	}
	Assert( LastSetTextureBindFlags( sampler ) == nBindFlags );
}

#endif

void CShaderAPIDx8::BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id )
{
	ShaderUtil()->BindStandardVertexTexture( sampler, id );
}

void CShaderAPIDx8::GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id )
{
	ShaderUtil()->GetStandardTextureDimensions( pWidth, pHeight, id );
}

float CShaderAPIDx8::GetSubDHeight()
{
	return ShaderUtil()->GetSubDHeight();
}

bool CShaderAPIDx8::IsStereoActiveThisFrame() const
{
	return m_bIsStereoActiveThisFrame;
}

//-----------------------------------------------------------------------------
// Gets the lightmap dimensions
//-----------------------------------------------------------------------------
void CShaderAPIDx8::GetLightmapDimensions( int *w, int *h )
{
	ShaderUtil()->GetLightmapDimensions( w, h );
}


//-----------------------------------------------------------------------------
// Selection mode methods
//-----------------------------------------------------------------------------
int CShaderAPIDx8::SelectionMode( bool selectionMode )
{
	LOCK_SHADERAPI();
	int numHits = m_NumHits;
	if (m_InSelectionMode)
	{
		WriteHitRecord();
	}
	m_InSelectionMode = selectionMode;
	m_pCurrSelectionRecord = m_pSelectionBuffer;
	m_NumHits = 0;
	return numHits;
}

bool CShaderAPIDx8::IsInSelectionMode() const
{
	return m_InSelectionMode;
}

void CShaderAPIDx8::SelectionBuffer( unsigned int* pBuffer, int size )
{
	LOCK_SHADERAPI();
	Assert( !m_InSelectionMode );
	Assert( pBuffer && size );
	m_pSelectionBufferEnd = pBuffer + size;
	m_pSelectionBuffer = pBuffer;
	m_pCurrSelectionRecord = pBuffer;
}

void CShaderAPIDx8::ClearSelectionNames( )
{
	LOCK_SHADERAPI();
	if (m_InSelectionMode)
	{
		WriteHitRecord();
	}
	m_SelectionNames.Clear();
}

void CShaderAPIDx8::LoadSelectionName( int name )
{
	LOCK_SHADERAPI();
	if (m_InSelectionMode)
	{
		WriteHitRecord();
		Assert( m_SelectionNames.Count() > 0 );
		m_SelectionNames.Top() = name;
	}
}

void CShaderAPIDx8::PushSelectionName( int name )
{
	LOCK_SHADERAPI();
	if (m_InSelectionMode)
	{
		WriteHitRecord();
		m_SelectionNames.Push(name);
	}
}

void CShaderAPIDx8::PopSelectionName()
{
	LOCK_SHADERAPI();
	if (m_InSelectionMode)
	{
		WriteHitRecord();
		m_SelectionNames.Pop();
	}
}

void CShaderAPIDx8::WriteHitRecord( )
{
	if (m_SelectionNames.Count() && (m_SelectionMinZ != FLT_MAX))
	{
		Assert( m_pCurrSelectionRecord + m_SelectionNames.Count() + 3 <	m_pSelectionBufferEnd );
		*m_pCurrSelectionRecord++ = m_SelectionNames.Count();
		// NOTE: because of rounding, "(uint32)(float)UINT32_MAX" yields zero(!), hence the use of doubles.
		// [ ALSO: As of Nov 2011, VS2010 exhibits a debug build code-gen bug if we cast the result to int32 instead of uint32 ]
	    *m_pCurrSelectionRecord++ = (uint32)( 0.5 + m_SelectionMinZ*(double)((uint32)~0) );
	    *m_pCurrSelectionRecord++ = (uint32)( 0.5 + m_SelectionMaxZ*(double)((uint32)~0) );
		for (int i = 0; i < m_SelectionNames.Count(); ++i)
		{
			*m_pCurrSelectionRecord++ = m_SelectionNames[i];
		}

		++m_NumHits;
	}

	m_SelectionMinZ = FLT_MAX;
	m_SelectionMaxZ = FLT_MIN;
}

// We hit something in selection mode
void CShaderAPIDx8::RegisterSelectionHit( float minz, float maxz )
{
	if (minz < 0)
		minz = 0;
	if (maxz > 1)
		maxz = 1;
	if (m_SelectionMinZ > minz)
		m_SelectionMinZ = minz;
	if (m_SelectionMaxZ < maxz)
		m_SelectionMaxZ = maxz;
}

int CShaderAPIDx8::GetCurrentNumBones( void ) const
{
	return m_DynamicState.m_NumBones;
}

bool CShaderAPIDx8::IsHWMorphingEnabled( ) const
{
	return m_DynamicState.m_bHWMorphingEnabled;
}

TessellationMode_t CShaderAPIDx8::GetTessellationMode( ) const
{
	return m_DynamicState.m_TessellationMode;
}

static Vector s_EmptyCube[6];
void CShaderAPIDx8::RecomputeAggregateLightingState( void )
{
	m_DynamicState.m_ShaderLightState.m_bAmbientLight = false;
	m_DynamicState.m_ShaderLightState.m_nNumLights = 0;
	m_DynamicState.m_ShaderLightState.m_bStaticLight = false;
	m_DynamicState.m_ShaderLightState.m_bStaticLightIndirectOnly = false;

	for ( int i = 0; i < m_nRenderInstanceCount; ++i )
	{
		const MaterialLightingState_t *pLightingState = m_pRenderInstances[i].m_pLightingState;
		if ( !pLightingState )
			continue;

		if ( !m_DynamicState.m_ShaderLightState.m_bAmbientLight )
		{
			m_DynamicState.m_ShaderLightState.m_bAmbientLight = ( memcmp( s_EmptyCube, pLightingState->m_vecAmbientCube, 18 * sizeof(float) ) != 0 );
		}

		if ( !m_DynamicState.m_ShaderLightState.m_bStaticLight )
		{
			m_DynamicState.m_ShaderLightState.m_bStaticLight = ( m_pRenderInstances[i].m_pColorBuffer != NULL );
		}

		if ( !m_DynamicState.m_ShaderLightState.m_bStaticLightIndirectOnly )
		{
			m_DynamicState.m_ShaderLightState.m_bStaticLightIndirectOnly = m_pRenderInstances[i].m_bColorBufferHasIndirectLightingOnly;
		}

#ifdef _DEBUG
		if ( g_pHardwareConfig->GetDXSupportLevel() >= 92 )
		{
			Assert( pLightingState->m_nLocalLightCount <= MATERIAL_MAX_LIGHT_COUNT );		// 2b hardware gets four lights
		}
		else
		{
			Assert( pLightingState->m_nLocalLightCount <= (MATERIAL_MAX_LIGHT_COUNT-2) );	// 2.0 hardware gets two less
		}
#endif

		int nNumLights = pLightingState->m_nLocalLightCount;

		// Only force the first dynamic light to directional for csm for non statically lit meshes (or those that only have indirect lighting baked, like phong static props)
		if ( nNumLights &&
			 ( ( !m_DynamicState.m_ShaderLightState.m_bStaticLight ) || m_DynamicState.m_ShaderLightState.m_bStaticLightIndirectOnly ) &&
			 r_force_first_dynamic_light_to_directional_for_csm.GetBool() &&
			 g_pHardwareConfig->SupportsCascadedShadowMapping() )
		{
			int j;
			for ( j = 0; j < nNumLights; ++j )
			{
				 if ( pLightingState->m_pLocalLightDesc[j].m_Type == MATERIAL_LIGHT_DIRECTIONAL )
					break;
			}
			if ( j == nNumLights )
			{
				// Bump up the # of actual lights to account for the hidden all-black directional light.
				// This solution sucks, because if numlights already equals MATERIAL_MAX_LIGHT_COUNT we will be deleting the last local light.
				// A better way would be to add a scalar in the compiled light state which suppresses the CSM shadows.
				nNumLights = MIN( nNumLights + 1, MATERIAL_MAX_LIGHT_COUNT );
			}
		}

		m_DynamicState.m_ShaderLightState.m_nNumLights = MAX( m_DynamicState.m_ShaderLightState.m_nNumLights, nNumLights );
		// Cap it to the maximum number of lights supported by the hardware
		m_DynamicState.m_ShaderLightState.m_nNumLights = MIN( m_DynamicState.m_ShaderLightState.m_nNumLights, g_pHardwareConfig->MaxNumLights() );

	}
}

void CShaderAPIDx8::GetDX9LightState( LightState_t *state ) const
{
	if ( !m_DynamicState.m_bLightStateComputed )
	{
		const_cast<CShaderAPIDx8*>(this)->RecomputeAggregateLightingState();
		m_DynamicState.m_bLightStateComputed = true;
	}

	memcpy( state, &m_DynamicState.m_ShaderLightState, sizeof(LightState_t) );
}

MaterialFogMode_t CShaderAPIDx8::GetCurrentFogType( void ) const
{
	Assert( 0 ); // deprecated. don't use
	return MATERIAL_FOG_NONE;
}

void CShaderAPIDx8::RecordString( const char *pStr )
{
	RECORD_STRING( pStr );
}

void CShaderAPIDx8::EvictManagedResourcesInternal()
{
	if ( IsGameConsole() )
		return;

	if ( !ThreadOwnsDevice() || !ThreadInMainThread() )
	{
		ShaderUtil()->OnThreadEvent( SHADER_THREAD_EVICT_RESOURCES );
		return;
	}
	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CShaderAPIDx8::EvictManagedResourcesInternal\n" );
	}

#if !defined( _X360 )
	if ( Dx9Device() )
	{
		Dx9Device()->EvictManagedResources();
	}
#endif
}

void CShaderAPIDx8::EvictManagedResources( void )
{
	if ( IsGameConsole() )
	{
		return;
	}

	LOCK_SHADERAPI();
	Assert(ThreadOwnsDevice());
	// Tell other material system applications to release resources
	SendIPCMessage( EVICT_MESSAGE );
	EvictManagedResourcesInternal();
}

void CShaderAPIDx8::GetGPUMemoryStats( GPUMemoryStats &stats )
{
#ifdef _PS3
	Dx9Device()->GetGPUMemoryStats( stats );
#else
	// TODO: 360/PC/mac (if possible/useful)
	memset( &stats, 0, sizeof( stats ) );
#endif
}

bool CShaderAPIDx8::IsDebugTextureListFresh( int numFramesAllowed /* = 1 */ )
{
	return ( m_nDebugDataExportFrame <= m_CurrentFrame ) && ( m_nDebugDataExportFrame >= m_CurrentFrame - numFramesAllowed );
}

bool CShaderAPIDx8::SetDebugTextureRendering( bool bEnable )
{
	bool bVal = m_bDebugTexturesRendering;
	m_bDebugTexturesRendering = bEnable;
	return bVal;
}

void CShaderAPIDx8::EnableDebugTextureList( bool bEnable )
{
	m_bEnableDebugTextureList = bEnable;
}

void CShaderAPIDx8::EnableGetAllTextures( bool bEnable )
{
	m_bDebugGetAllTextures = bEnable;
}

KeyValues* CShaderAPIDx8::LockDebugTextureList( void )
{
	m_DebugTextureListLock.Lock();
	return m_pDebugTextureList;
}

void CShaderAPIDx8::UnlockDebugTextureList( void )
{
	m_DebugTextureListLock.Unlock();
}

int CShaderAPIDx8::GetTextureMemoryUsed( TextureMemoryType eTextureMemory )
{
	switch ( eTextureMemory )
	{
	case MEMORY_BOUND_LAST_FRAME:
		return m_nTextureMemoryUsedLastFrame;
	case MEMORY_TOTAL_LOADED:
		return m_nTextureMemoryUsedTotal;
	case MEMORY_ESTIMATE_PICMIP_1:
		return m_nTextureMemoryUsedPicMip1;
	case MEMORY_ESTIMATE_PICMIP_2:
		return m_nTextureMemoryUsedPicMip2;
	default:
		return 0;
	}
}

// Allocate and delete query objects.
ShaderAPIOcclusionQuery_t CShaderAPIDx8::CreateOcclusionQueryObject( void )
{
	// don't allow this on <80 because it falls back to wireframe in that case
	if( m_DeviceSupportsCreateQuery == 0 )
		return INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;

	// While we're deactivated, m_OcclusionQueryObjects just holds NULL pointers.
	// Create a dummy one here and let ReacquireResources create the actual D3D object.
	if ( IsDeactivated() )
		return INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;

	IDirect3DQuery9 *pQuery = NULL;
	HRESULT hr = Dx9Device()->CreateQuery( D3DQUERYTYPE_OCCLUSION, &pQuery );
	return ( hr == D3D_OK ) ? (ShaderAPIOcclusionQuery_t)pQuery : INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;
}

void CShaderAPIDx8::DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t handle )
{
	IDirect3DQuery9 *pQuery = (IDirect3DQuery9 *)handle;

	int nRetVal = pQuery->Release();
	Assert( nRetVal == 0 );
}

// Bracket drawing with begin and end so that we can get counts next frame.
void CShaderAPIDx8::BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t handle )
{
	IDirect3DQuery9 *pQuery = (IDirect3DQuery9 *)handle;

	HRESULT hResult = pQuery->Issue( D3DISSUE_BEGIN );
	Assert( hResult == D3D_OK );
}

void CShaderAPIDx8::EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t handle )
{
	IDirect3DQuery9 *pQuery = (IDirect3DQuery9 *)handle;

	HRESULT hResult = pQuery->Issue( D3DISSUE_END );
	Assert( hResult == D3D_OK );
}

// Get the number of pixels rendered between begin and end on an earlier frame.
// Calling this in the same frame is a huge perf hit!
int CShaderAPIDx8::OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t handle, bool bFlush )
{
	LOCK_SHADERAPI();
	IDirect3DQuery9 *pQuery = (IDirect3DQuery9 *)handle;

	DWORD nPixels;
	HRESULT hResult = pQuery->GetData( &nPixels, sizeof( nPixels ), bFlush ? D3DGETDATA_FLUSH : 0 );

	// This means that the query will not finish and resulted in an error, game should use
	// the previous query's results and not reissue the query
	if ( hResult == D3DERR_DEVICELOST )
		return OCCLUSION_QUERY_RESULT_ERROR;

	// This means the query isn't finished yet, game will have to use the previous query's
	// results and not reissue the query; wait for query to finish later.
	if ( hResult == S_FALSE )
		return OCCLUSION_QUERY_RESULT_PENDING;

	// NOTE: This appears to work around a driver bug for ATI on Vista
	if ( nPixels & 0x80000000 )
	{
		nPixels = 0;
	}
	return nPixels;
}

void CShaderAPIDx8::SetPixelShaderFogParams( int reg, ShaderFogMode_t fogMode )
{
	m_DelayedShaderConstants.iPixelShaderFogParams = reg; //save it off in case the ShaderFogMode_t disables fog. We only find out later.
	float fogParams[4];

	MaterialFogMode_t pixelFogMode = GetSceneFogMode();

	if( (pixelFogMode != MATERIAL_FOG_NONE) && ( fogMode != SHADER_FOGMODE_DISABLED ) )
	{
		float ooFogRange = 1.0f;

		float fStart = m_VertexShaderFogParams[0];
		float fEnd = m_VertexShaderFogParams[1];

		// Check for divide by zero
		if ( fEnd != fStart )
		{
			ooFogRange = 1.0f / ( fEnd - fStart );
		}

		// Fixed-function-blended per-vertex fog requires some inverted params since a fog factor of 0 means fully fogged and 1 means no fog.
		// We could implement shader fog the same way, but would require an extra subtract in the vertex and/or pixel shader, which we want to avoid.
		fogParams[0] = 1.0f - ooFogRange * fEnd; // -start / ( fogEnd - fogStart )
		fogParams[1] = m_DynamicState.m_FogZ; // water height
		fogParams[2] = clamp( m_flFogMaxDensity, 0.0f, 1.0f ); // Max fog density
		fogParams[3] = ooFogRange; // 1 / ( fogEnd - fogStart );
	}
	else
	{
		// Fixed-function-blended per-vertex fog requires some inverted params since a fog factor of 0 means fully fogged and 1 means no fog.
		// We could implement shader fog the same way, but would require an extra subtract in the vertex and/or pixel shader, which we want to avoid.
		//emulating MATERIAL_FOG_NONE by setting the parameters so that CalcRangeFog() always returns 0. Gets rid of a dynamic combo across the ps2x set.
		fogParams[0] = 0.0f;
		fogParams[1] = -FLT_MAX;
		fogParams[2] = 0.0f;
		fogParams[3] = 0.0f;
	}

	// cFogEndOverFogRange, cFogOne, unused, cOOFogRange
	SetPixelShaderConstantInternal( reg, fogParams, 1 );
}

void CShaderAPIDx8::SetPixelShaderFogParams( int reg )
{
	ShadowState_t const *pState = m_TransitionTable.CurrentShadowState();
	if ( pState )
	{
		SetPixelShaderFogParams( reg, pState->m_FogAndMiscState.FogMode() );
	}
	else
	{
		// Have to do this so that m_DelayedShaderConstants.iPixelShaderFogParams gets updated so that we 
		// get this set properly when the currnent shadow state is set.  If we don't do this, the 
		// first draw call of every frame will not get a fog constant set.
		SetPixelShaderFogParams( reg, SHADER_FOGMODE_DISABLED );
	}
}

void CShaderAPIDx8::SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture )
{
	LOCK_SHADERAPI();
	SetFlashlightStateEx( state, worldToTexture, NULL );
}

FORCEINLINE float ShadowAttenFromState( const FlashlightState_t &state )
{
	// DX10 requires some hackery due to sRGB/blend ordering change from DX9, which makes the shadows too light
	if ( g_pHardwareConfig->UsesSRGBCorrectBlending() )
		return state.m_flShadowAtten * 0.1f; // magic number

	return state.m_flShadowAtten;
}

FORCEINLINE float ShadowFilterFromState( const FlashlightState_t &state )
{
	// We developed shadow maps at 1024, so we expect the penumbra size to have been tuned relative to that
	return state.m_flShadowFilterSize / 1024.0f;
}

FORCEINLINE void HashShadow2DJitter( const float fJitterSeed, float *fU, float* fV )
{
	const int nTexRes = 128;
	int nSeed = fmod (fJitterSeed, 1.0f) * nTexRes * nTexRes;

	int nRow = nSeed / nTexRes;
	int nCol = nSeed % nTexRes;

	// Div and mod to get an individual texel in the fTexRes x fTexRes grid
	*fU = nRow / (float) nTexRes;	// Row
	*fV = nCol / (float) nTexRes;	// Column
}

void SetupUberlightFromState( UberlightRenderState_t *pUberlight, const FlashlightState_t &state )
{
	// Bail if we can't do ps30 or we don't even want an uberlight
	if ( !( g_pHardwareConfig->GetDXSupportLevel() < 95 ) || !state.m_bUberlight )
		return;

	const UberlightState_t &u = state.m_uberlightState;

	// Trap values to prevent dividing by zero
	float flNearEdge = u.m_fNearEdge < 0.0001f ? 0.001f : u.m_fNearEdge;
	float flFarEdge = u.m_fFarEdge < 0.0001f ? 0.001f : u.m_fFarEdge;
	float flRoundness = u.m_fRoundness < 0.0001f ? 0.001f : u.m_fRoundness;

	// Set uberlight shader parameters as function of user controls from UberlightState_t
	pUberlight->m_vSmoothEdge0.Init(	0.0f,			u.m_fCutOn - u.m_fNearEdge,	u.m_fCutOff,				0.0f );
	pUberlight->m_vSmoothEdge1.Init(	0.0f,			u.m_fCutOn,					u.m_fCutOff + u.m_fFarEdge,	0.0f );
	pUberlight->m_vSmoothOneOverW.Init(	0.0f,			1.0f / flNearEdge,			1.0f / flFarEdge,			0.0f );
	pUberlight->m_vShearRound.Init(		u.m_fShearx,	u.m_fSheary,				2.0f / flRoundness,		   -u.m_fRoundness / 2.0f );
	pUberlight->m_vaAbB.Init(			u.m_fWidth,		u.m_fWidth + u.m_fWedge,	u.m_fHeight,				u.m_fHeight + u.m_fHedge );

	QAngle angles;
	QuaternionAngles( state.m_quatOrientation, angles );

	// World to Light's View matrix
	VMatrix viewMatrixInverse;
	viewMatrixInverse.SetupMatrixOrgAngles( state.m_vecLightOrigin, angles );
	MatrixInverseGeneral( viewMatrixInverse, pUberlight->m_WorldToLight );
}

ConVar r_flashlightbrightness( "r_flashlightbrightness", "0.25", FCVAR_CHEAT );

void CShaderAPIDx8::SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture )
{
	LOCK_SHADERAPI();
	// fixme: do a test here.
	m_FlashlightState = state;
	m_FlashlightWorldToTexture = worldToTexture;
	m_pFlashlightDepthTexture = pFlashlightDepthTexture;

	if ( g_pHardwareConfig->GetDXSupportLevel() < 92 )
	{
		m_FlashlightState.m_bEnableShadows = false;
		m_pFlashlightDepthTexture = NULL;
	}

	// FIXME: This is shader specific code, only in here because of the command-buffer
	// stuff required to make the 360 be fast. We need this to be in the shader DLLs,
	// callable from shaderapidx8 in a fast way somehow.

	// Cache off pixel shader + vertex shader values
	m_pFlashlightAtten[0] = m_FlashlightState.m_fConstantAtten;		// Set the flashlight attenuation factors
	m_pFlashlightAtten[1] = m_FlashlightState.m_fLinearAtten;
	m_pFlashlightAtten[2] = m_FlashlightState.m_fQuadraticAtten;
	m_pFlashlightAtten[3] = m_FlashlightState.m_FarZAtten;

	m_pFlashlightPos[0] = m_FlashlightState.m_vecLightOrigin[0];		// Set the flashlight origin
	m_pFlashlightPos[1] = m_FlashlightState.m_vecLightOrigin[1];
	m_pFlashlightPos[2] = m_FlashlightState.m_vecLightOrigin[2];
	m_pFlashlightPos[3] = m_FlashlightState.m_FarZ;

	float flFlashlightScale = r_flashlightbrightness.GetFloat();

	if ( IsPC() && !g_pHardwareConfig->GetHDREnabled() )
	{
		// Non-HDR path requires 2.0 flashlight
		flFlashlightScale = 2.0f;
	}

	flFlashlightScale *= m_FlashlightState.m_fBrightnessScale;

	// Generate pixel shader constant
	float const *pFlashlightColor = m_FlashlightState.m_Color;
	m_pFlashlightColor[0] = flFlashlightScale * pFlashlightColor[0];
	m_pFlashlightColor[1] = flFlashlightScale * pFlashlightColor[1];
	m_pFlashlightColor[2] = flFlashlightScale * pFlashlightColor[2];
	m_pFlashlightColor[3] = pFlashlightColor[3];	// not used, will be whacked by ExecuteCommandBuffer

	// Red flashlight for testing
	//m_pFlashlightColor[0] = 0.5f; m_pFlashlightColor[1] = 0.0f; m_pFlashlightColor[2] = 0.0f;

	m_pFlashlightTweaks[0] = ShadowFilterFromState( m_FlashlightState );
	m_pFlashlightTweaks[1] = ShadowAttenFromState( m_FlashlightState );
	HashShadow2DJitter( m_FlashlightState.m_flShadowJitterSeed, &m_pFlashlightTweaks[2], &m_pFlashlightTweaks[3] );

	if ( !IsX360() )
	{
		SetupUberlightFromState( &m_UberlightRenderState, m_FlashlightState );
	}
}

const FlashlightState_t &CShaderAPIDx8::GetFlashlightState( VMatrix &worldToTexture ) const
{
	worldToTexture = m_FlashlightWorldToTexture;
	return m_FlashlightState;
}

const FlashlightState_t &CShaderAPIDx8::GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **ppFlashlightDepthTexture ) const
{
	worldToTexture = m_FlashlightWorldToTexture;
	*ppFlashlightDepthTexture = m_pFlashlightDepthTexture;
	return m_FlashlightState;
}

void CShaderAPIDx8::GetFlashlightShaderInfo( bool *pShadowsEnabled, bool *pUberLight ) const
{
	// Adding NULL ptr check on m_pFlashlightDepthTexture here so we slam the FLASHLIGHTSHADOWS dynamic combo off if a flashlight depth texture isn't actually bound (otherwise we set a TEXTURE_WHITE texture, 
	// which isn't depth which results in the pixel shader sampling a regular color texture as depth)
	*pShadowsEnabled = m_FlashlightState.m_bEnableShadows && ( m_pFlashlightDepthTexture != NULL );
	*pUberLight = m_FlashlightState.m_bUberlight;
}

float CShaderAPIDx8::GetFlashlightAmbientOcclusion( ) const
{
	return m_FlashlightState.m_flAmbientOcclusion;
}

bool CShaderAPIDx8::SupportsMSAAMode( int nMSAAMode )
{
	if ( IsX360() )
	{
		return false;
	}

	if ( m_PresentParameters.BackBufferFormat == D3DFMT_UNKNOWN )
	{
		// Autodetection code running during application startup:
		return ( D3D_OK == D3D()->CheckDeviceMultiSampleType( m_DisplayAdapter, DX8_DEVTYPE,
			D3DFMT_A8R8G8B8,
			false,
			ComputeMultisampleType( nMSAAMode ), NULL ) );
	}

	return ( D3D_OK == D3D()->CheckDeviceMultiSampleType( m_DisplayAdapter, m_DeviceType, 
														   m_PresentParameters.BackBufferFormat,
														   m_PresentParameters.Windowed,
														   ComputeMultisampleType( nMSAAMode ), NULL ) );
}

bool CShaderAPIDx8::SupportsCSAAMode( int nNumSamples, int nQualityLevel )
{
#ifdef DX_TO_GL_ABSTRACTION
	return false;
#endif

	// Only nVidia does this kind of AA
	if ( g_pHardwareConfig->Caps().m_VendorID != VENDORID_NVIDIA )
		return false;

	DWORD dwQualityLevels = 0;
	HRESULT hr = D3D()->CheckDeviceMultiSampleType( m_DisplayAdapter, m_DeviceType, 
													 m_PresentParameters.BackBufferFormat,
													 m_PresentParameters.Windowed,
													 ComputeMultisampleType( nNumSamples ), &dwQualityLevels );

	return ( ( D3D_OK == hr ) && ( (int) dwQualityLevels >= nQualityLevel ) );
}

void CShaderAPIDx8::BeginGeneratingCSMs()
{
	m_bGeneratingCSMs = true;

	if ( mat_depthwrite_new_path.GetBool() )
	{
		// Set bunch of render states for shadow drawing
		SetRenderStateForce( D3DRS_ZWRITEENABLE, TRUE );
		SetRenderStateForce( D3DRS_ZENABLE, D3DZB_TRUE );
		SetRenderStateForce( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
		SetRenderStateForce( D3DRS_DEPTHBIAS, 0.0f );
		SetRenderStateForce( D3DRS_COLORWRITEENABLE, 0 );
		SetRenderStateForce( D3DRS_ALPHATESTENABLE, FALSE );
		SetRenderStateForce( D3DRS_FILLMODE, D3DFILL_SOLID );

		// Disable culling 
		ApplyCullEnable( false );
	}
}

void CShaderAPIDx8::EndGeneratingCSMs()
{
	if ( mat_depthwrite_new_path.GetBool() )
	{
		SetRenderStateForce( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_BLUE |
			D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_ALPHA );
		SetRenderStateForce( D3DRS_DEPTHBIAS, 0.0f );

		float fShadowSlopeScaleDepthBias = 0.0f;
		SetRenderStateForce( D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD*)&fShadowSlopeScaleDepthBias );

		// Re-enable culling 
		ApplyCullEnable( true );
		
	}

	m_bGeneratingCSMs = false;
	m_bCSMsValidThisFrame = true;
}

void CShaderAPIDx8::PerpareForCascadeDraw( int cascade, float fShadowSlopeScaleDepthBias, float fShadowDepthBias )
{
	if ( mat_depthwrite_new_path.GetBool() )
	{
		SetRenderStateForce( D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD*)&fShadowSlopeScaleDepthBias );
		SetRenderState( D3DRS_DEPTHBIAS, *((DWORD*) (&fShadowDepthBias)) );
	}
}


void CShaderAPIDx8::SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias )
{
	m_fShadowSlopeScaleDepthBias = fShadowSlopeScaleDepthBias;
	m_fShadowDepthBias = fShadowDepthBias;
	m_TransitionTable.SetShadowDepthBiasValuesDirty( true );
	if ( m_TransitionTable.CurrentShadowState() && m_TransitionTable.CurrentShadowState()->m_DepthTestState.m_ZBias == SHADER_POLYOFFSET_SHADOW_BIAS )
	{
		// Need to set the render state here right away, because changing bias values between different draws
		// using the same depthwrite shader won't apply the snapshot again.
		ApplyZBias( m_TransitionTable.CurrentShadowState()->m_DepthTestState );
	}
}

void CShaderAPIDx8::ClearVertexAndPixelShaderRefCounts()
{
	LOCK_SHADERAPI();
	ShaderManager()->ClearVertexAndPixelShaderRefCounts();
}

void CShaderAPIDx8::PurgeUnusedVertexAndPixelShaders()
{
	LOCK_SHADERAPI();
	ShaderManager()->PurgeUnusedVertexAndPixelShaders();
}

bool CShaderAPIDx8::UsingSoftwareVertexProcessing() const
{
	return g_pHardwareConfig->Caps().m_bSoftwareVertexProcessing;
}

ITexture *CShaderAPIDx8::GetRenderTargetEx( int nRenderTargetID ) const
{
	return ShaderUtil()->GetRenderTargetEx( nRenderTargetID );
}

void CShaderAPIDx8::SetToneMappingScaleLinear( const Vector &scale )
{
	Vector scale_to_use = scale;
	m_ToneMappingScale.AsVector3D() = scale_to_use;
	
	bool mode_uses_srgb=false;

	switch( HardwareConfig()->GetHDRType() )
	{
		case HDR_TYPE_NONE:
			m_ToneMappingScale.x = 1.0;										// output scale
			m_ToneMappingScale.z = 1.0;										// reflection map scale
			break;

		case HDR_TYPE_FLOAT:
			m_ToneMappingScale.x = scale_to_use.x;							// output scale
			m_ToneMappingScale.z = 1.0;										// reflection map scale
			break;

		case HDR_TYPE_INTEGER:
			mode_uses_srgb = true;
			m_ToneMappingScale.x = scale_to_use.x;							// output scale
			m_ToneMappingScale.z = 16.0;									// reflection map scale
			break;
	}

#ifdef _PS3
	// We're using floating point cubemaps but not the full HDR_TYPE_FLOAT codepath
	m_ToneMappingScale.z = 1.0f;
#endif // _PS3

	m_ToneMappingScale.y = g_pHardwareConfig->GetLightMapScaleFactor();	// light map scale

	// w component gets gamma scale
	m_ToneMappingScale.w = LinearToGammaFullRange( m_ToneMappingScale.x );
	// For people trying to search the shader cpp code trying to figure out where these constants get set:
	// TONE_MAPPING_SCALE_PSH_CONSTANT == PSREG_LIGHT_SCALE (same register)
	// This is the same constant as cLightScale which corresponds to LINEAR_LIGHT_SCALE, LIGHT_MAP_SCALE, ENV_MAP_SCALE, and GAMMA_LIGHT_SCALE
	SetPixelShaderConstantInternal( TONE_MAPPING_SCALE_PSH_CONSTANT, m_ToneMappingScale.Base() );
	RegenerateFogConstants();

	// We have to change the fog color since we tone map directly in the shaders in integer HDR mode.
	if ( HardwareConfig()->GetHDRType() == HDR_TYPE_INTEGER && m_TransitionTable.CurrentShadowState() )
	{
		// Get the shadow state in sync since it depends on SetToneMappingScaleLinear.
		ApplyFogMode( m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.FogMode(), m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_bVertexFogEnable, mode_uses_srgb, m_TransitionTable.CurrentShadowState()->m_FogAndMiscState.m_bDisableFogGammaCorrection );
	}
}

const Vector & CShaderAPIDx8::GetToneMappingScaleLinear( void ) const
{
	return m_ToneMappingScale.AsVector3D();
}

void CShaderAPIDx8::EnableLinearColorSpaceFrameBuffer( bool bEnable )
{
	LOCK_SHADERAPI();
	m_TransitionTable.EnableLinearColorSpaceFrameBuffer( bEnable );
}

void CShaderAPIDx8::SetFloatRenderingParameter( int parm_number, float value )
{
	LOCK_SHADERAPI();
	if ( parm_number < ARRAYSIZE( FloatRenderingParameters ))
		FloatRenderingParameters[parm_number] = value;
}

void CShaderAPIDx8::SetIntRenderingParameter( int parm_number, int value )
{
	LOCK_SHADERAPI();
	if ( parm_number < ARRAYSIZE( IntRenderingParameters ))
		IntRenderingParameters[parm_number] = value;
}

void CShaderAPIDx8::SetTextureRenderingParameter( int parm_number, ITexture *pTexture )
{
	LOCK_SHADERAPI();
	if ( parm_number < ARRAYSIZE( TextureRenderingParameters ))
		TextureRenderingParameters[parm_number] = pTexture;
}


void CShaderAPIDx8::SetVectorRenderingParameter( int parm_number, Vector const & value )
{
	LOCK_SHADERAPI();
	if ( parm_number < ARRAYSIZE( VectorRenderingParameters ))
		VectorRenderingParameters[parm_number] = value;
}

float CShaderAPIDx8::GetFloatRenderingParameter( int parm_number ) const
{
	LOCK_SHADERAPI();
	if ( parm_number < ARRAYSIZE( FloatRenderingParameters ))
		return FloatRenderingParameters[parm_number];
	else
		return 0.0;
}

int CShaderAPIDx8::GetIntRenderingParameter( int parm_number ) const
{
	LOCK_SHADERAPI();
	if ( parm_number < ARRAYSIZE( IntRenderingParameters ))
		return IntRenderingParameters[parm_number];
	else
		return 0;
}

ITexture *CShaderAPIDx8::GetTextureRenderingParameter( int parm_number ) const
{
	LOCK_SHADERAPI();
	if ( parm_number < ARRAYSIZE( TextureRenderingParameters ))
		return TextureRenderingParameters[parm_number];
	else
		return 0;
}

Vector CShaderAPIDx8::GetVectorRenderingParameter( int parm_number ) const
{
	LOCK_SHADERAPI();
	if ( parm_number < ARRAYSIZE( VectorRenderingParameters ))
		return VectorRenderingParameters[parm_number];
	else
		return Vector( 0, 0, 0 );
}

#if defined( _GAMECONSOLE )
void CShaderAPIDx8::BeginConsoleZPass2( int nNumSpilloverIndicesNeeded )
{
	bool bEnableZPass = true;

	if ( MeshMgr()->GetDynamicIndexBufferIndicesLeft() < nNumSpilloverIndicesNeeded )
	{
		bEnableZPass = false;

		// TODO: If nNumDynamicIndicesNeeded <= GetDynamicIBAllocationCount() we could potentially flush the dynamic IB completely
		// and start filling it from the beginning, enabling the z pass
	}

	if ( bEnableZPass )
	{
		Assert( m_bInZPass == false );

		// reset renderstate to make sure the Zfunc is set to a state that's compatible with the Z pass
		m_TransitionTable.UseSnapshot( m_zPassSnapshot );

		Dx9Device()->BeginZPass( 0 );

		#if defined( _X360 )
		// set up predicated vertexshader GPR allocations so that we max out VS threads in the Z pass and
		// use the currently requested alloction for the main render pass
		Dx9Device()->SetPredication( D3DPRED_ALL_Z );
		Dx9Device()->SetShaderGPRAllocation( D3DSETALLOCATION_PREDICATED, 96, 32 );
		Dx9Device()->SetPredication( 0 );

		// this function knows about predication inside of a zpass block
		CommitShaderGPRs( Dx9Device(), m_DesiredState, m_DynamicState, true );
		#else
		Dx9Device()->SetPredication( 0 ); // just disable predication
		#endif

		m_bInZPass = true;
		m_nZPassCounter++;
	}
	else
	{
#ifndef _PS3
		Warning( "Cannot satisfy Console Z pass request due to large dynamic index count (indices left %d < spilloever %d). Tell Thorsten.\n", MeshMgr()->GetDynamicIndexBufferIndicesLeft(), nNumSpilloverIndicesNeeded );
#endif
	}
}

void CShaderAPIDx8::EndConsoleZPass()
{
	if ( m_bInZPass )
	{
		m_bInZPass = false;

		// reset all command predication
		Dx9Device()->SetPredication( 0 );

		HRESULT retVal = Dx9Device()->EndZPass();
		if ( retVal != S_OK )
		{
			Warning( "EndConsoleZPass() failed! Tell Thorsten.\n" );
		}

		#if defined( _X360 )
		// Reset shader GPRs un-predicated
		CommitShaderGPRs( Dx9Device(), m_DesiredState, m_DynamicState, true );
		#endif
	}
}

void CShaderAPIDx8::EnablePredication( bool bZPass, bool bRenderPass )
{
	DWORD mask = 0;
	mask |= bZPass ? D3DPRED_ALL_Z : 0;
	mask |= bRenderPass ? D3DPRED_ALL_RENDER : 0;
	Dx9Device()->SetPredication( mask );
}

void CShaderAPIDx8::DisablePredication()
{
	Dx9Device()->SetPredication( 0 );
}

#endif

// stencil entry points
void CShaderAPIDx8::GetCurrentStencilState( ShaderStencilState_t *pState )
{
	pState->m_bEnable = ( m_DynamicState.m_RenderState[D3DRS_STENCILENABLE] == ( (DWORD) TRUE ) );
	pState->m_FailOp = (ShaderStencilOp_t)( m_DynamicState.m_RenderState[D3DRS_STENCILFAIL] );
	pState->m_ZFailOp = (ShaderStencilOp_t)( m_DynamicState.m_RenderState[D3DRS_STENCILZFAIL] );
	pState->m_PassOp = (ShaderStencilOp_t)( m_DynamicState.m_RenderState[D3DRS_STENCILPASS] );
	pState->m_CompareFunc = (ShaderStencilFunc_t)( m_DynamicState.m_RenderState[D3DRS_STENCILFUNC] );
	pState->m_nReferenceValue = m_DynamicState.m_RenderState[D3DRS_STENCILREF];
	pState->m_nTestMask = m_DynamicState.m_RenderState[D3DRS_STENCILMASK];
	pState->m_nWriteMask = m_DynamicState.m_RenderState[D3DRS_STENCILWRITEMASK];
#if defined( _X360 )
	pState->m_bHiStencilEnable = ( m_DynamicState.m_RenderState[D3DRS_HISTENCILENABLE] == TRUE );
	pState->m_bHiStencilWriteEnable = ( m_DynamicState.m_RenderState[D3DRS_HISTENCILWRITEENABLE] == TRUE );
	pState->m_HiStencilCompareFunc = (ShaderHiStencilFunc_t)( m_DynamicState.m_RenderState[D3DRS_HISTENCILFUNC] );
	pState->m_nHiStencilReferenceValue = m_DynamicState.m_RenderState[D3DRS_HISTENCILREF];
#endif
}

//#define WORKAROUND_DEBUG_SHADERAPI_ERRORS

void CShaderAPIDx8::SetStencilStateInternal( const ShaderStencilState_t &state )
{
#ifdef WORKAROUND_DEBUG_SHADERAPI_ERRORS
	if ( m_bInZPass )
	{
		SetRenderState( D3DRS_STENCILENABLE, FALSE );
	}
	else
	{
		SetRenderState( D3DRS_STENCILENABLE, state.m_bEnable ? TRUE:FALSE );
	}
#else
	SetRenderState( D3DRS_STENCILENABLE, state.m_bEnable ? TRUE:FALSE );
#endif
#if defined( _X360 )
	SetRenderState( D3DRS_HISTENCILENABLE, mat_hi_stencil_enable.GetBool() ? state.m_bHiStencilEnable : FALSE );
	SetRenderState( D3DRS_HISTENCILWRITEENABLE, state.m_bHiStencilWriteEnable );
	SetRenderState( D3DRS_HISTENCILFUNC, state.m_HiStencilCompareFunc );
	SetRenderState( D3DRS_HISTENCILREF, state.m_nHiStencilReferenceValue );
#endif

	if ( !state.m_bEnable )
		return;

	SetRenderState( D3DRS_STENCILFAIL, state.m_FailOp );
	SetRenderState( D3DRS_STENCILZFAIL, state.m_ZFailOp );
	SetRenderState( D3DRS_STENCILPASS, state.m_PassOp );
	SetRenderState( D3DRS_STENCILFUNC, state.m_CompareFunc );
	SetRenderState( D3DRS_STENCILREF, state.m_nReferenceValue );
	SetRenderState( D3DRS_STENCILMASK, state.m_nTestMask );
	SetRenderState( D3DRS_STENCILWRITEMASK, state.m_nWriteMask );
}

void CShaderAPIDx8::SetStencilState( const ShaderStencilState_t &state )
{
	LOCK_SHADERAPI();
#ifdef _DEBUG
	if ( state.m_bEnable && state.m_CompareFunc == SHADER_STENCILFUNC_ALWAYS )
	{
		AssertMsg( state.m_FailOp == SHADER_STENCILOP_KEEP, "Always set failop to keep when comparefunc is always for hi-z perf reasons" );
	}
#endif
	SetStencilStateInternal( state );
}

void CShaderAPIDx8::ClearStencilBufferRectangle(
	int xmin, int ymin, int xmax, int ymax,int value)
{
	LOCK_SHADERAPI();
	D3DRECT clear;
	clear.x1 = xmin;
	clear.y1 = ymin;
	clear.x2 = xmax;
	clear.y2 = ymax;

	Dx9Device()->Clear( 
		1, &clear, D3DCLEAR_STENCIL, 0, 0, value );	
}

#if defined( _X360 )
// Flush Hi-Stencil changes to Hierarchical Z/Stencil tile memory. Introduces about 2000 cycle stall on GPU.
// There's an asynchronous flush, but it's less robust, so we'll take the stall for now.
void CShaderAPIDx8::FlushHiStencil()
{
	LOCK_SHADERAPI();
	Dx9Device()->FlushHiZStencil( D3DFHZS_SYNCHRONOUS );
}
#endif



void CShaderAPIDx8::AntiAliasingHint( int nHint )
{
#if defined( _PS3 )
	Dx9Device()->AntiAliasingHint( nHint );
#endif
}



#if defined( _PS3 )

void CShaderAPIDx8::FlushTextureCache()
{
	Dx9Device()->FlushTextureCache();
}

#endif // _PS3

int CShaderAPIDx8::CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 )
{
	LOCK_SHADERAPI();
	const ShadowState_t &shadow0 = m_TransitionTable.GetSnapshot(snapshot0);
	const ShadowState_t &shadow1 = m_TransitionTable.GetSnapshot(snapshot1);
	const ShadowShaderState_t &shader0 = m_TransitionTable.GetSnapshotShader(snapshot0);
	const ShadowShaderState_t &shader1 = m_TransitionTable.GetSnapshotShader(snapshot1);

	int dVertex = shader0.m_VertexShader - shader1.m_VertexShader;
	if ( dVertex )
		return dVertex;
	int dVCombo = shader0.m_nStaticVshIndex - shader1.m_nStaticVshIndex;
	if ( dVCombo)
		return dVCombo;

	int dPixel = shader0.m_PixelShader - shader1.m_PixelShader;
	if ( dPixel )
		return dPixel;
	int dPCombo = shader0.m_nStaticPshIndex - shader1.m_nStaticPshIndex;
	if ( dPCombo)
		return dPCombo;

	return snapshot0 - snapshot1;
}

//-----------------------------------------------------------------------------
// X360 TTF support requires XUI state manipulation of d3d.
// Font support lives inside the shaderapi in order to maintain privacy of d3d.
//-----------------------------------------------------------------------------
#if defined( _X360 )
HXUIFONT CShaderAPIDx8::OpenTrueTypeFont( const char *pFontname, int tall, int style )
{
	LOCK_SHADERAPI();

	struct FontTable_t
	{
		const char	*pFontName;
		const char	*pPath;
		bool		m_bRestrictiveLoadIntoMemory;
		bool		m_bAlwaysLoadIntoMemory;
	};

	// explicit mapping required, dvd searching too expensive
	static FontTable_t s_pFontToFilename[] = 
	{
#include "fontremaptable.h"
	};

	MEM_ALLOC_CREDIT_( "CShaderAPIDx8::OpenTrueTypeFont ( -> Xui*)" );

	// remap typeface to diskname
	const char *pDiskname = NULL;
	bool bUsingInMemoryFont = false;
	for ( int i = 0; i < ARRAYSIZE( s_pFontToFilename ); i++ )
	{
		if ( !V_stricmp( pFontname, s_pFontToFilename[i].pFontName ) )
		{
			pDiskname = s_pFontToFilename[i].pPath;
			// force all the names rto be normalized, want Courier to match to courier to COURIER
			pFontname = s_pFontToFilename[i].pFontName;

			bUsingInMemoryFont = s_pFontToFilename[i].m_bAlwaysLoadIntoMemory;
			if ( XBX_IsRestrictiveLanguage() && s_pFontToFilename[i].m_bRestrictiveLoadIntoMemory )
			{
				bUsingInMemoryFont = true;
			}
			
			break;
		}
	}
	if ( !pDiskname )
	{
		// not found
		DevMsg( "True Type Font: '%s' unknown.\n", pFontname );
		return NULL;
	}

	// font will be registered using the !!!Table's!!! typeface name
	wchar_t	wchFontname[MAX_PATH];
	Q_UTF8ToUnicode( pFontname, wchFontname, sizeof( wchFontname ) );

	// find font in registered typefaces
	TypefaceDescriptor *pDescriptors = NULL;
	DWORD numTypeFaces = 0;
	HRESULT hr = XuiEnumerateTypefaces( &pDescriptors, &numTypeFaces );
	if ( FAILED( hr ) )
	{
		return NULL;
	}

	bool bRegistered = false;
	for ( DWORD i=0; i<numTypeFaces; i++ )
	{
		if ( !V_wcscmp( pDescriptors[i].szTypeface, wchFontname ) )
		{
			bRegistered = true;
			break;
		}
	}

	// release enumeration query
	XuiDestroyTypefaceList( pDescriptors, numTypeFaces );

	if ( !bRegistered )
	{
		char filename[MAX_PATH];

		if ( bUsingInMemoryFont )
		{
			void *pMemory = NULL;
			int nAlignedSize = 0;
			int nFileSize = 0;

			int iMapIndex = m_XboxFontMemoryDict.Find( pFontname );
			if ( iMapIndex == m_XboxFontMemoryDict.InvalidIndex() )
			{
				V_snprintf( filename, sizeof( filename ), "d:/%s", pDiskname );
				V_FixSlashes( filename, '/' );
				nFileSize = g_pFullFileSystem->Size( filename, NULL );
				if ( nFileSize <= 0 )
				{
					return NULL;
				}

				// Size must be aligned to 32k for optimal load
				nAlignedSize = AlignValue( nFileSize, 32 * 1024 );	
				pMemory = XPhysicalAlloc( nAlignedSize, MAXULONG_PTR, 0, PAGE_READWRITE );

				CUtlBuffer buf;
				buf.SetExternalBuffer( pMemory, nAlignedSize, 0 );
				if ( !g_pFullFileSystem->ReadFile( filename, NULL, buf ) )
				{
					XPhysicalFree( pMemory );
					return NULL;
				}

				iMapIndex = m_XboxFontMemoryDict.Insert( pFontname );
				m_XboxFontMemoryDict[iMapIndex].m_pPhysicalMemory = pMemory;
				m_XboxFontMemoryDict[iMapIndex].m_nMemorySize = nAlignedSize;
				m_XboxFontMemoryDict[iMapIndex].m_nFontFileSize = nFileSize;
			}
			else
			{
				pMemory = m_XboxFontMemoryDict[iMapIndex].m_pPhysicalMemory;
				nFileSize = m_XboxFontMemoryDict[iMapIndex].m_nFontFileSize;
			}

			V_snprintf( filename, sizeof( filename ), "memory://%X,%X", (intp)pMemory, nFileSize );
		}
		else
		{
			V_snprintf( filename, sizeof( filename ), "file://d:/%s", pDiskname );
		}

		V_FixSlashes( filename, '/' );
		wchar_t	wchFilename[MAX_PATH];
		V_UTF8ToUnicode( filename, wchFilename, sizeof( wchFilename ) );

		TypefaceDescriptor desc;
		desc.fBaselineAdjust = 0;
		desc.szFallbackTypeface = NULL;
		desc.szLocator = wchFilename;
		desc.szReserved1 = 0;
		desc.szTypeface = wchFontname;
		hr = XuiRegisterTypeface( &desc, FALSE );
		if ( FAILED( hr ) )
		{
			return NULL;
		}
	}

	// empirically derived factor to achieve desired cell height
	float pointSize = tall * 0.59f;
	HXUIFONT hFont = NULL;
	hr = XuiCreateFont( wchFontname, pointSize, style, 0, &hFont );
	if ( FAILED( hr ) )
	{
		return NULL;
	}

	return hFont;
}
#endif

//-----------------------------------------------------------------------------
// Release TTF
//-----------------------------------------------------------------------------
#if defined( _X360 )
void CShaderAPIDx8::CloseTrueTypeFont( HXUIFONT hFont )
{
	if ( !hFont )
		return;
	LOCK_SHADERAPI();

	XuiReleaseFont( hFont );
}
#endif

//-----------------------------------------------------------------------------
// Get the TTF Metrics
//-----------------------------------------------------------------------------
#if defined( _X360 )
bool CShaderAPIDx8::GetTrueTypeFontMetrics( HXUIFONT hFont, wchar_t wchFirst, wchar_t wchLast, XUIFontMetrics *pFontMetrics, XUICharMetrics *pCharMetrics )
{
	if ( !hFont )
		return false;

	LOCK_SHADERAPI();

	int numChars = wchLast - wchFirst + 1;

	V_memset( pCharMetrics, 0, numChars * sizeof( XUICharMetrics ) );

	HRESULT hr = XuiGetFontMetrics( hFont, pFontMetrics );
	if ( !FAILED( hr ) )
	{
		// X360 issue: max character width may be too small.
		// Run through each character and fixup
		for ( int i = 0; i < numChars; i++ )
		{
			hr = XuiGetCharMetrics( hFont, wchFirst + i, pCharMetrics + i );
			if ( !FAILED( hr ) )
			{
				float maxWidth = pCharMetrics[i].fMaxX;
				if ( pCharMetrics[i].fMinX < 0 )
				{
					maxWidth = pCharMetrics[i].fMaxX - pCharMetrics[i].fMinX;
				}
				if ( maxWidth > pFontMetrics->fMaxWidth )
				{
					pFontMetrics->fMaxWidth = maxWidth;
				}
				if ( pCharMetrics[i].fAdvance > pFontMetrics->fMaxWidth )
				{
					pFontMetrics->fMaxWidth = pCharMetrics[i].fAdvance;
				}
			}
		}

		// Fonts are getting cut off, MaxHeight seems to be misreported smaller in some cases when fMaxDescent <= 0.
		// Additionally, XuiGetFontHeight() returns the same value as pFontMetrics->fLineHeight, not fMaxHeight, so we can't use that!
		// So, use MAX( fMaxHeight, ( fMaxAscent - fMaxDescent ) ) when fMaxDescent <= 0
		float maxHeight = 0;
		if ( pFontMetrics->fMaxDescent <= 0 )
		{
			// descent is negative for below baseline
			maxHeight = pFontMetrics->fMaxAscent - pFontMetrics->fMaxDescent;
		}
		if ( maxHeight > pFontMetrics->fMaxHeight )
		{
			pFontMetrics->fMaxHeight = maxHeight;
		}
	}

	return ( !FAILED( hr ) );
}
#endif

//-----------------------------------------------------------------------------
// Gets the glyph bits in rgba order. This function PURPOSELY hijacks D3D
// because XUI is involved. It is called at a very specific place in the VGUI
// render frame where its deleterious affects are going to be harmless.
//-----------------------------------------------------------------------------
#if defined( _X360 )
bool CShaderAPIDx8::GetTrueTypeGlyphs( HXUIFONT hFont, int numChars, wchar_t *pWch, int *pOffsetX, int *pOffsetY, int *pWidth, int *pHeight, unsigned char *pRGBA, int *pRGBAOffset )
{
	if ( !hFont )
		return false;

	// Ensure this doesn't talk to D3D at the same time as the loading bar
	AUTO_LOCK_FM( m_nonInteractiveModeMutex );

	LOCK_SHADERAPI();
	bool bSuccess = false;
	IDirect3DSurface *pRTSurface = NULL;
	IDirect3DSurface *pSavedSurface = NULL;
	IDirect3DSurface *pSavedDepthSurface = NULL;
	IDirect3DTexture *pTexture = NULL;
	D3DVIEWPORT9 savedViewport;
    D3DXMATRIX matView;
    D3DXMATRIX matXForm;
	D3DLOCKED_RECT lockedRect;

	// must release resources for xui
	bool bPreviousOwnState = OwnGPUResources( false );

	// have to reset to default state to rasterize glyph correctly
	// state will get re-established during next mesh draw
	ResetRenderState( false );

	Dx9Device()->SetRenderState( D3DRS_ZENABLE, FALSE );

	Dx9Device()->GetRenderTarget( 0, &pSavedSurface );
	Dx9Device()->GetDepthStencilSurface( &pSavedDepthSurface );
	Dx9Device()->GetViewport( &savedViewport );

	// Figure out the size of surface/texture we need to allocate
	int rtWidth = 0;
	int rtHeight = 0;
	for ( int i = 0; i < numChars; i++ )
	{
		rtWidth += pWidth[i];
		rtHeight = MAX( rtHeight, pHeight[i] );
	}

	// per resolve() restrictions, need to be 32 aligned
	// The 64 is critical to fixing their internal clip logic. Apprarently if the glyph is really larger
	// than our metrics, it won't render. This happens with foreign characters where we think its 28
	// wide, it's actually 34 (by XuiMeasureText numbers), we snap to 32, glyph gets culled.
	// The downside to this is we have bad metrics and we may be clipping bottom and right.
	rtWidth = AlignValue( rtWidth, 64 );
	rtHeight = AlignValue( rtHeight, 64 );

	// create a render target to capture the glyph render
	pRTSurface = g_TextureHeap.AllocRenderTargetSurface( rtWidth, rtHeight, D3DFMT_A8R8G8B8 );
	if ( !pRTSurface )
		goto cleanUp;

	Dx9Device()->SetRenderTarget( 0, pRTSurface );
	// Disable depth here otherwise you get a colour/depth multisample mismatch error (in 480p)
	Dx9Device()->SetDepthStencilSurface( NULL );
	Dx9Device()->Clear( 0, NULL, D3DCLEAR_TARGET, 0x00000000, ( ReverseDepthOnX360() ? 0.0 : 1.0f ), 0 );

	// create texture to get glyph render from EDRAM
	HRESULT hr = Dx9Device()->CreateTexture( rtWidth, rtHeight, 1, 0, D3DFMT_A8R8G8B8, 0, &pTexture, NULL );
	if ( FAILED( hr ) )
		goto cleanUp;

	XuiRenderBegin( m_hDC, 0x00000000 );

	D3DXMatrixIdentity( &matView );
	XuiRenderSetViewTransform( m_hDC, &matView );
	XuiRenderSetTransform( m_hDC, &matView );

	// rasterize the glyph
	XuiSelectFont( m_hDC, hFont );
	XuiSetColorFactor( m_hDC, 0xFFFFFFFF );

	// Draw the characters, stepping across the texture
	int xCursor = 0;
	for ( int i = 0; i < numChars; i++)
	{
		// FIXME: the drawRect params don't make much sense (should use "(xCursor+pWidth[i]), pHeight[i]", but then some characters disappear!)
		XUIRect drawRect = XUIRect( xCursor + pOffsetX[i], pOffsetY[i], rtWidth, rtHeight );
		wchar_t	text[2] = { pWch[i], 0 };
		XuiDrawText( m_hDC, text, XUI_FONT_STYLE_NORMAL|XUI_FONT_STYLE_SINGLE_LINE|XUI_FONT_STYLE_NO_WORDWRAP, 0, &drawRect ); 
		xCursor += pWidth[i];
	}

	XuiRenderEnd( m_hDC );

	// transfer from edram to system
	hr = Dx9Device()->Resolve( 0, NULL, pTexture, NULL, 0, 0, NULL, 0, 0, NULL );
	if ( FAILED( hr ) )
		goto cleanUp;

	hr = pTexture->LockRect( 0, &lockedRect, NULL, 0 );
	if ( FAILED( hr ) )
		goto cleanUp;

	// transfer to linear format, one character at a time
	xCursor = 0;
	for ( int i = 0;i < numChars; i++ )
	{
		int destPitch = pWidth[i]*4;
		unsigned char *pLinear = pRGBA + pRGBAOffset[i];
		RECT copyRect = { xCursor, 0, xCursor + pWidth[i], pHeight[i] };
		xCursor += pWidth[i];
		XGUntileSurface( pLinear, destPitch, NULL, lockedRect.pBits, rtWidth, rtHeight, &copyRect, 4 );

		// convert argb to rgba
		float r, g, b, a;
		for ( int y = 0; y < pHeight[i]; y++ )
		{
			unsigned char *pSrc = (unsigned char*)pLinear + y*destPitch;
			for ( int x = 0; x < pWidth[i]; x++ )
			{
				// undo pre-multiplied alpha since glyph bits will be sourced as a rgba texture
				if ( !pSrc[0] )
					a = 1;
				else
					a = (float)pSrc[0] * 1.0f/255.0f;
				
				r = ((float)pSrc[1] * 1.0f/255.0f)/a * 255.0f;
				if ( r > 255 )
					r = 255;
			
				g = ((float)pSrc[2] * 1.0f/255.0f)/a * 255.0f;
				if ( g > 255 )
					g = 255;

				b = ((float)pSrc[3] * 1.0f/255.0f)/a * 255.0f;
				if ( b > 255 )
					b = 255;

				pSrc[3] = pSrc[0];
				pSrc[2] = b;
				pSrc[1] = g;
				pSrc[0] = r;

				pSrc += 4;
			}
		}
	}

	pTexture->UnlockRect( 0 );

	bSuccess = true;

cleanUp:
	if ( pRTSurface )
	{
		Dx9Device()->SetRenderTarget( 0, pSavedSurface );
		Dx9Device()->SetDepthStencilSurface( pSavedDepthSurface );
		Dx9Device()->SetViewport( &savedViewport );
		pRTSurface->Release();
	}

	if ( pTexture )
		pTexture->Release();

	if ( pSavedSurface )
		pSavedSurface->Release();

	// XUI changed renderstates behind our back, so we need to reset to defaults again to get matching states
	ResetRenderState( false );

	OwnGPUResources( bPreviousOwnState );

	return bSuccess;
}
#endif

//-----------------------------------------------------------------------------
// Create a 360 Render Target Surface
//-----------------------------------------------------------------------------
#if defined( _X360 )
ShaderAPITextureHandle_t CShaderAPIDx8::CreateRenderTargetSurface( int width, int height, ImageFormat format, RTMultiSampleCount360_t multiSampleCount, const char *pDebugName, const char *pTextureGroupName )
{
	LOCK_SHADERAPI();
	ShaderAPITextureHandle_t textureHandle = CreateTextureHandle();
	Texture_t *pTexture = &GetTexture( textureHandle );

	pTexture->m_Flags = (Texture_t::IS_ALLOCATED | Texture_t::IS_RENDER_TARGET_SURFACE);
	pTexture->m_CreationFlags = 0;

	pTexture->m_DebugName = pDebugName;
	pTexture->m_Width = width;
	pTexture->m_Height = height;
	pTexture->m_Depth = 1;
	pTexture->m_NumCopies = 1;
	pTexture->m_CurrentCopy = 0;

	D3DFORMAT actualFormat = FindNearestSupportedFormat( format, false, true, false );
	ImageFormat dstImageFormat  = ImageLoader::D3DFormatToImageFormat( actualFormat );

	pTexture->GetRenderTargetSurface( false ) = g_TextureHeap.AllocRenderTargetSurface( width, height, actualFormat, multiSampleCount );

#if defined( CSTRIKE15 )
	// [mariod] - implicit srgb render targets (regular 8888, writes happen in shaders) as opposed to PWL
	pTexture->GetRenderTargetSurface( true ) = g_TextureHeap.AllocRenderTargetSurface( width, height, actualFormat, multiSampleCount );
#else
	pTexture->GetRenderTargetSurface( true ) = g_TextureHeap.AllocRenderTargetSurface( width, height, (D3DFORMAT)MAKESRGBFMT( actualFormat ), multiSampleCount );
#endif

	pTexture->SetImageFormat( dstImageFormat );

	pTexture->m_UTexWrap = D3DTADDRESS_CLAMP;
	pTexture->m_VTexWrap = D3DTADDRESS_CLAMP;
	pTexture->m_WTexWrap = D3DTADDRESS_CLAMP;
	pTexture->m_MagFilter = D3DTEXF_LINEAR;

	pTexture->m_NumLevels = 1;
	pTexture->m_MipFilter = D3DTEXF_NONE;
	pTexture->m_MinFilter = D3DTEXF_LINEAR;

	pTexture->m_SwitchNeeded = false;
	
	ComputeStatsInfo( textureHandle, false, false );
	SetupTextureGroup( textureHandle, pTextureGroupName );

	return textureHandle;
}

#endif

//-----------------------------------------------------------------------------
// Shader constants are batched and written to gpu once prior to draw.
//-----------------------------------------------------------------------------
void CShaderAPIDx8::WriteShaderConstantsToGPU()
{
#if defined( _X360 )
	if ( !IsGPUOwnSupported() || !m_bGPUOwned )
	{
		return;
	}

	// vector vertex constants can just blast their set range
	if ( m_MaxVectorVertexShaderConstant )
	{
		if ( m_bGPUOwned )
		{
			// faster path, write directly into GPU command buffer, bypassing shadow state
			// can only set what is actually owned
			Assert( m_MaxVectorVertexShaderConstant <= VERTEX_SHADER_MODEL + 3*NUM_MODEL_TRANSFORMS );
			int numVectors = AlignValue( m_MaxVectorVertexShaderConstant, 4 );
			BYTE* pCommandBufferData;
			Dx9Device()->GpuBeginVertexShaderConstantF4( 0, (D3DVECTOR4**)&pCommandBufferData, numVectors );
			memcpy( pCommandBufferData, m_DesiredState.m_pVectorVertexShaderConstant[0].Base(), numVectors * (sizeof( float ) * 4) );
			Dx9Device()->GpuEndVertexShaderConstantF4();
		}
		else
		{
			Dx9Device()->SetVertexShaderConstantF( 0, m_DesiredState.m_pVectorVertexShaderConstant[0].Base(), m_MaxVectorVertexShaderConstant );
		}

		memcpy( m_DynamicState.m_pVectorVertexShaderConstant[0].Base(), m_DesiredState.m_pVectorVertexShaderConstant[0].Base(), m_MaxVectorVertexShaderConstant * 4 * sizeof(float) );
		m_MaxVectorVertexShaderConstant = 0;
	}

	if ( m_MaxVectorPixelShaderConstant )
	{
		if ( m_bGPUOwned )
		{
			// faster path, write directly into GPU command buffer, bypassing shadow state
			// can only set what is actually owned
			Assert( m_MaxVectorPixelShaderConstant <= 32 );
			int numVectors = AlignValue( m_MaxVectorPixelShaderConstant, 4 );
			BYTE* pCommandBufferData;
			Dx9Device()->GpuBeginPixelShaderConstantF4( 0, (D3DVECTOR4**)&pCommandBufferData, numVectors );
			memcpy( pCommandBufferData, m_DesiredState.m_pVectorPixelShaderConstant[0].Base(), numVectors * (sizeof( float ) * 4) );
			Dx9Device()->GpuEndPixelShaderConstantF4();
		}
		else
		{
			Dx9Device()->SetPixelShaderConstantF( 0, m_DesiredState.m_pVectorPixelShaderConstant[0].Base(), m_MaxVectorPixelShaderConstant );
		}

		memcpy( m_DynamicState.m_pVectorPixelShaderConstant[0].Base(), m_DesiredState.m_pVectorPixelShaderConstant[0].Base(), m_MaxVectorPixelShaderConstant * 4 * sizeof(float) );
		m_MaxVectorPixelShaderConstant = 0;
	}

	// boolean and integer constants can just blast their set range
	// these are currently extremely small in number, if this changes they may benefit from a fast path pattern
	if ( m_MaxBooleanVertexShaderConstant )
	{
		Dx9Device()->SetVertexShaderConstantB( 0, m_DesiredState.m_pBooleanVertexShaderConstant, m_MaxBooleanVertexShaderConstant );
		memcpy( m_DynamicState.m_pBooleanVertexShaderConstant, m_DesiredState.m_pBooleanVertexShaderConstant, m_MaxBooleanVertexShaderConstant * sizeof(BOOL) );
		m_MaxBooleanVertexShaderConstant = 0;
	}
	if ( m_MaxIntegerVertexShaderConstant )
	{
		Dx9Device()->SetVertexShaderConstantI( 0, (int *)m_DesiredState.m_pIntegerVertexShaderConstant, m_MaxIntegerVertexShaderConstant );
		memcpy( m_DynamicState.m_pIntegerVertexShaderConstant[0].Base(), m_DesiredState.m_pIntegerVertexShaderConstant[0].Base(), m_MaxIntegerVertexShaderConstant * sizeof(IntVector4D) );
		m_MaxIntegerVertexShaderConstant = 0;
	}

	if ( m_MaxBooleanPixelShaderConstant )
	{
		Dx9Device()->SetPixelShaderConstantB( 0, m_DesiredState.m_pBooleanPixelShaderConstant, m_MaxBooleanPixelShaderConstant );
		memcpy( m_DynamicState.m_pBooleanPixelShaderConstant, m_DesiredState.m_pBooleanPixelShaderConstant, m_MaxBooleanPixelShaderConstant * sizeof(BOOL) );
		m_MaxBooleanPixelShaderConstant = 0;
	}

	// integer pixel constants are not used, so not supporting
#if 0
	if ( m_MaxIntegerPixelShaderConstant )
	{
		Dx9Device()->SetPixelShaderConstantI( 0, (int *)m_DesiredState.m_pIntegerPixelShaderConstant, m_MaxIntegerPixelShaderConstant );
		memcpy( m_DynamicState.m_pIntegerPixelShaderConstant[0].Base(), m_DesiredState.m_pIntegerPixelShaderConstant[0].Base(), m_MaxIntegerPixelShaderConstant * sizeof(IntVector4D) );
		m_MaxIntegerPixelShaderConstant = 0;
	}
#endif
#endif
}

//-----------------------------------------------------------------------------
// The application is about to perform a hard reboot, but wants to hide the screen flash
// by persisting the front buffer across a reboot boundary. The persisted frame buffer
// can be detected and restored.
//-----------------------------------------------------------------------------
#if defined( _X360 )
void CShaderAPIDx8::PersistDisplay()
{
	if ( m_PresentParameters.FrontBufferFormat != D3DFMT_LE_X8R8G8B8 )
	{
		// The format must be what PersistDisplay() expects, otherwise D3DRIP.
		// If this hits due to sRGB bit set that confuses PersistDisplay(),
		// the fix may be to slam the presentation parameters to the expected format,
		// do a ResetDevice(), and then PersistDisplay().
		Assert( 0 );
		return;
	}

	IDirect3DTexture *pTexture;
	HRESULT hr = Dx9Device()->GetFrontBuffer( &pTexture );
	if ( !FAILED( hr ) )
	{
		OwnGPUResources( false );
		Dx9Device()->PersistDisplay( pTexture, NULL );
		pTexture->Release();
	}
}
#endif

#if defined( _GAMECONSOLE )
bool CShaderAPIDx8::PostQueuedTexture( const void *pData, int nDataSize, ShaderAPITextureHandle_t *pHandles, int numHandles, int nWidth, int nHeight, int nDepth, int numMips, int *pRefCount )
{
	CUtlBuffer vtfBuffer;	
	IVTFTexture *pVTFTexture = NULL;
	int iTopMip = 0;
	bool bOK = false;
	
	if ( !pData || !nDataSize )
	{
		// invalid
		goto cleanUp;
	}

	// get a unique vtf and mount texture
	// vtf can expect non-volatile buffer data to be stable through vtf lifetime
	// this prevents redundant copious amounts of image memory transfers
	pVTFTexture = CreateVTFTexture();
	vtfBuffer.SetExternalBuffer( (void *)pData, nDataSize, nDataSize );	
	if ( !pVTFTexture->UnserializeFromBuffer( vtfBuffer, false, false, false, 0 ) )
	{
		goto cleanUp;
	}

	// provided vtf buffer is all mips, determine top mip due to possible picmip
	int mipWidth, mipHeight, mipDepth;
	do
	{
		pVTFTexture->ComputeMipLevelDimensions( iTopMip, &mipWidth, &mipHeight, &mipDepth );
		if ( nWidth == mipWidth && nHeight == mipHeight && nDepth == mipDepth )
		{
			break;
		}
		iTopMip++;
	} 
	while ( mipWidth != 1 || mipHeight != 1 || mipDepth != 1 );
	
	// create and blit
	for ( int iFrame = 0; iFrame < numHandles; iFrame++ )
	{
		ShaderAPITextureHandle_t hTexture = pHandles[iFrame];
		Texture_t *pTexture = &GetTexture( hTexture );

		int nFaceCount = ( pTexture->m_CreationFlags & TEXTURE_CREATE_CUBEMAP ) ? CUBEMAP_FACE_COUNT : 1;

		IDirect3DBaseTexture *pD3DTexture;
		if ( pTexture->m_CreationFlags & TEXTURE_CREATE_NOD3DMEMORY )
		{
			// We created the texture, but deferred allocating storage for the bits until now
			pD3DTexture = pTexture->GetTexture();
#if defined( _X360 )
			if ( !g_TextureHeap.FixupAllocD3DMemory( pD3DTexture ) )
			{
				goto cleanUp;
			}
#elif defined( _PS3 )
			if ( !Dx9Device()->AllocateTextureStorage( pD3DTexture ) )
			{
				goto cleanUp;
			}
#endif // _X360/_PS3
		}
		else
		{
			pD3DTexture = pTexture->GetTexture();
		}

		// blit the hi-res texture bits into d3d memory
		for ( int iFace = 0; iFace < nFaceCount; ++iFace )
		{
			for ( int iMip = 0; iMip < numMips; ++iMip )
			{
				pVTFTexture->ComputeMipLevelDimensions( iTopMip + iMip, &mipWidth, &mipHeight, &mipDepth );
				unsigned char *pSourceBits = pVTFTexture->ImageData( iFrame, iFace, iTopMip + iMip, 0, 0, 0 );

				TextureLoadInfo_t info;
				info.m_TextureHandle = hTexture;
				info.m_pTexture = pD3DTexture;
				info.m_nLevel = iMip;
				info.m_nCopy = 0;
				info.m_CubeFaceID = (D3DCUBEMAP_FACES)iFace;
				info.m_nWidth = mipWidth;
				info.m_nHeight = mipHeight;
				info.m_nZOffset = 0;
				info.m_SrcFormat = pVTFTexture->Format();
				info.m_pSrcData = pSourceBits;
#if defined( _X360 )
				info.m_bSrcIsTiled = pVTFTexture->IsPreTiled();
				info.m_bCanConvertFormat = ( pTexture->m_Flags & Texture_t::CAN_CONVERT_FORMAT ) != 0;
#endif // _X360
				LoadTexture( info );
			}
		}

		pTexture->m_Flags |= Texture_t::IS_FINALIZED;
		(*pRefCount)--;
	}

	// success
	bOK = true;

cleanUp:
	if ( pVTFTexture )
	{
		DestroyVTFTexture( pVTFTexture );
	}

	if ( !bOK )
	{
		// undo artificial lock
		(*pRefCount) -= numHandles;
	}

	return bOK;
}	
#endif

#if defined( _X360 )
void CShaderAPIDx8::SetCacheableTextureParams( ShaderAPITextureHandle_t *pHandles, int count, const char *pFilename, int mipSkipCount )
{
	for ( int i = 0; i < count; i++ )
	{
		Texture_t *pTexture = &GetTexture( pHandles[i] );
		if ( !( pTexture->m_Flags & Texture_t::IS_CACHEABLE ) )
		{
			// ignore
			continue;
		}

		g_TextureHeap.SetCacheableTextureParams( pTexture->GetTexture(), pFilename, mipSkipCount );
	}
}
#endif

#if defined( _X360 )
void *CShaderAPIDx8::GetD3DDevice()
{
	return Dx9Device();
}
#endif

#if defined( _X360 )
static void r_enable_gpr_allocations_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( ((ConVar *)var)->GetBool() == false )
	{
		//reset back the default 64/64 allocation before we stop updating
		if( Dx9Device() != NULL )
		{
			Dx9Device()->SetShaderGPRAllocation( 0, 0, 0 );
		}
	}
}
ConVar r_enable_gpr_allocations( "r_enable_gpr_allocations", "1", 0, "Enable usage of IDirect3DDevice9::SetShaderGPRAllocation()", r_enable_gpr_allocations_callback );

static void CommitShaderGPRs( D3DDeviceWrapper *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce )
{
	if ( ( desiredState.m_iVertexShaderGPRAllocation != currentState.m_iVertexShaderGPRAllocation ) || bForce )
	{
		if ( pDevice->GetDeviceState() & D3DDEVICESTATE_ZPASS_BRACKET )
		{
			// if a z pass is active, we need to predicate the allocation to only happen in the main render pass
			pDevice->SetPredication( D3DPRED_ALL_RENDER );
			pDevice->SetShaderGPRAllocation( D3DSETALLOCATION_PREDICATED, desiredState.m_iVertexShaderGPRAllocation, 128 - desiredState.m_iVertexShaderGPRAllocation );
			pDevice->SetPredication( 0 );
		}
		else
		{
			pDevice->SetShaderGPRAllocation( 0, desiredState.m_iVertexShaderGPRAllocation, 128 - desiredState.m_iVertexShaderGPRAllocation );
		}
		currentState.m_iVertexShaderGPRAllocation = desiredState.m_iVertexShaderGPRAllocation;
	}
}

void CShaderAPIDx8::PushVertexShaderGPRAllocation( int iVertexShaderCount )
{
	Assert( (iVertexShaderCount >= 16) && (iVertexShaderCount <= 112) );
	m_VertexShaderGPRAllocationStack.Push( iVertexShaderCount );

	if ( r_enable_gpr_allocations.GetBool() )
	{
		if ( m_DesiredState.m_iVertexShaderGPRAllocation != iVertexShaderCount )
		{
			m_DesiredState.m_iVertexShaderGPRAllocation = iVertexShaderCount;
			ADD_COMMIT_FUNC( COMMIT_PER_DRAW, CommitShaderGPRs );
		}
	}
}

void CShaderAPIDx8::PopVertexShaderGPRAllocation( void )
{
	m_VertexShaderGPRAllocationStack.Pop();

	if ( r_enable_gpr_allocations.GetBool() )
	{
		int iVertexShaderCount;
		if ( m_VertexShaderGPRAllocationStack.Count() )
			iVertexShaderCount = m_VertexShaderGPRAllocationStack.Top();
		else
			iVertexShaderCount = 64;

		if ( m_DesiredState.m_iVertexShaderGPRAllocation != iVertexShaderCount )
		{
			m_DesiredState.m_iVertexShaderGPRAllocation = iVertexShaderCount;
			ADD_COMMIT_FUNC( COMMIT_PER_DRAW, CommitShaderGPRs );
		}
	}
}

void CShaderAPIDx8::EnableVSync_360( bool bEnable )
{
	if ( bEnable )
	{
		Dx9Device()->SetRenderState( D3DRS_PRESENTIMMEDIATETHRESHOLD, mat_x360_vblank_miss_threshold.GetInt() ); //only swap on vertical blanks
	}
	else
	{
		Dx9Device()->SetRenderState( D3DRS_PRESENTIMMEDIATETHRESHOLD, 100 ); //allow a swap at any point in the DAC scan
	}
}
#endif

// ------------ New Vertex/Index Buffer interface ----------------------------

void CShaderAPIDx8::BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions )
{
	LOCK_SHADERAPI();
	MeshMgr()->BindVertexBuffer( streamID, pVertexBuffer, nOffsetInBytes, nFirstVertex, nVertexCount, fmt, nRepetitions );
}

void CShaderAPIDx8::BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
{
	LOCK_SHADERAPI();
	MeshMgr()->BindIndexBuffer( pIndexBuffer, nOffsetInBytes );
}

void CShaderAPIDx8::Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount )
{
	LOCK_SHADERAPI();
	MeshMgr()->Draw( primitiveType, nFirstIndex, nIndexCount );
}

// ------------ End ----------------------------

float CShaderAPIDx8::GammaToLinear_HardwareSpecific( float fGamma ) const
{
	return SrgbGammaToLinear( fGamma );
}

float CShaderAPIDx8::LinearToGamma_HardwareSpecific( float fLinear ) const
{
	return SrgbLinearToGamma( fLinear );
}


bool CShaderAPIDx8::ShouldWriteDepthToDestAlpha( void ) const
{
	return IsPC() && g_pHardwareConfig->GetDXSupportLevel() >= 92 &&
			(m_SceneFogMode != MATERIAL_FOG_LINEAR_BELOW_FOG_Z) && 
			(GetIntRenderingParameter(INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA) != 0);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CShaderAPIDx8::AcquireThreadOwnership()
{
	SetCurrentThreadAsOwner();

#if defined( _X360 ) || defined( DX_TO_GL_ABSTRACTION )
	Dx9Device()->AcquireThreadOwnership();
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReleaseThreadOwnership()
{
	RemoveThreadOwner();

#if defined( _X360 ) || defined( DX_TO_GL_ABSTRACTION )
	Dx9Device()->ReleaseThreadOwnership();
#endif
}


//-----------------------------------------------------------------------------
// Actual low level setting of the color RT. All Xbox RT funnels here
// to track the actual RT state.  Returns true if the RT gets set, otherwise false.
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::SetRenderTargetInternalXbox( ShaderAPITextureHandle_t hRenderTargetTexture, bool bForce )
{
	// valid for 360 only
	if ( IsPC() )
	{
		Assert( 0 );
		return false;
	}

	if ( hRenderTargetTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
	{
		// could be a reset, force to back buffer
		hRenderTargetTexture = SHADER_RENDERTARGET_BACKBUFFER;
	}

	if ( m_hCachedRenderTarget == INVALID_SHADERAPI_TEXTURE_HANDLE )
	{
		// let the set go through to establish the initial state
		bForce = true;
	}

	if ( !bForce && ( hRenderTargetTexture == m_hCachedRenderTarget && m_DynamicState.m_bSRGBWritesEnabled == m_bUsingSRGBRenderTarget ) )
	{
		// current RT matches expected state, leave state intact
		return false;
	}

	// track the updated state
	m_bUsingSRGBRenderTarget = m_DynamicState.m_bSRGBWritesEnabled;
	m_hCachedRenderTarget = hRenderTargetTexture;

#if defined( _X360 )
	IDirect3DSurface *pSurface;
	if ( m_hCachedRenderTarget == SHADER_RENDERTARGET_BACKBUFFER )
	{
		if ( !m_bUsingSRGBRenderTarget )
		{
			pSurface = m_pBackBufferSurfaces[BACK_BUFFER_INDEX_DEFAULT];
		}
		else
		{
			pSurface = m_pBackBufferSurfaceSRGB;
		}
	}
	else
	{
		AssertValidTextureHandle( m_hCachedRenderTarget );
		Texture_t *pTexture = &GetTexture( m_hCachedRenderTarget );
	    pSurface = pTexture->GetRenderTargetSurface( m_bUsingSRGBRenderTarget );
	}

	// the 360 does a wierd reset of some states on a SetRenderTarget()
	// the viewport is a clobbered state, it may not be changed by later callers, so it MUST be put back as expected
	// the other clobbered states are waiting to be discovered ... sigh
	D3DVIEWPORT9 viewport;
	Dx9Device()->GetViewport( &viewport );
	Dx9Device()->SetRenderTarget( 0, pSurface );
	Dx9Device()->SetViewport( &viewport );
#endif

	return true;
}


void CShaderAPIDx8::OnPresent( void )
{
	CallCommitFuncs( COMMIT_PER_DRAW );
}


void CShaderAPIDx8::AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics )
{
	ShaderManager()->AddShaderComboInformation( pSemantics );
}

//-----------------------------------------------------------------------------
// debug logging
//-----------------------------------------------------------------------------
void CShaderAPIDx8::PrintfVA( char *fmt, va_list vargs )
{
#ifdef DX_TO_GL_ABSTRACTION
	#if GLMDEBUG
		GLMPrintfVA( fmt, vargs );
	#endif
#else
	AssertOnce( !"Impl me" );
#endif
}

void CShaderAPIDx8::Printf( char *fmt, ... )
{
#ifdef DX_TO_GL_ABSTRACTION
	#if GLMDEBUG
		va_list	vargs;

		va_start(vargs, fmt);

		GLMPrintfVA( fmt, vargs );

		va_end( vargs );
	#endif
#else
	AssertOnce( !"Impl me" );
#endif
}

float CShaderAPIDx8::Knob( char *knobname, float *setvalue )
{
#ifdef DX_TO_GL_ABSTRACTION
	#if GLMDEBUG
		return GLMKnob( knobname, setvalue );
	#else
		return 0.0f;
	#endif
#else
	return 0.0f;
#endif
}

float CShaderAPIDx8::GetLightMapScaleFactor() const
{
	return HardwareConfig()->GetLightMapScaleFactor();
}

ShaderAPITextureHandle_t CShaderAPIDx8::FindTexture( const char *pDebugName )
{
	for ( ShaderAPITextureHandle_t hTexture = m_Textures.Head() ; hTexture != m_Textures.InvalidIndex(); hTexture = m_Textures.Next( hTexture ) )
	{
		Texture_t &tex = m_Textures[hTexture];
		if ( !V_stricmp( tex.m_DebugName.String(), pDebugName ) )
		{
			return hTexture;
		}
	}

	// not found
	return INVALID_SHADERAPI_TEXTURE_HANDLE;
}

void CShaderAPIDx8::GetTextureDimensions( ShaderAPITextureHandle_t hTexture, int &nWidth, int &nHeight, int &nDepth )
{
	nWidth = 0;
	nHeight = 0;
	nDepth = 1;

	if ( hTexture == INVALID_SHADERAPI_TEXTURE_HANDLE || !TextureIsAllocated( hTexture ) )
		return;

	Texture_t *pTexture = &GetTexture( hTexture );
	nWidth = pTexture->m_Width;
	nHeight = pTexture->m_Height;
	nDepth = pTexture->m_Depth;
}

#if defined( _X360 )

extern ConVar r_blocking_spew_threshold;
void D3DBlockingSpewCallback( DWORD Flags, D3DBLOCKTYPE BlockType, float ClockTime, DWORD ThreadTime )
{
	if ( ClockTime >= r_blocking_spew_threshold.GetFloat() )
	{
		const char *pBlockType = "";
		switch( BlockType )
		{		
		case D3DBLOCKTYPE_NONE:
			pBlockType = "D3DBLOCKTYPE_NONE";
			break;
		case D3DBLOCKTYPE_PRIMARY_OVERRUN:
			pBlockType = "D3DBLOCKTYPE_PRIMARY_OVERRUN";
			break;
		case D3DBLOCKTYPE_SECONDARY_OVERRUN:
			pBlockType = "D3DBLOCKTYPE_SECONDARY_OVERRUN";
			break;
		case D3DBLOCKTYPE_SWAP_THROTTLE:
			pBlockType = "D3DBLOCKTYPE_SWAP_THROTTLE";
			break;
		case D3DBLOCKTYPE_BLOCK_UNTIL_IDLE:
			pBlockType = "D3DBLOCKTYPE_BLOCK_UNTIL_IDLE";
			break;
		case D3DBLOCKTYPE_BLOCK_UNTIL_NOT_BUSY:
			pBlockType = "D3DBLOCKTYPE_BLOCK_UNTIL_NOT_BUSY";
			break;
		case D3DBLOCKTYPE_BLOCK_ON_FENCE:
			pBlockType = "D3DBLOCKTYPE_BLOCK_ON_FENCE";
			break;
		case D3DBLOCKTYPE_VERTEX_SHADER_RELEASE:
			pBlockType = "D3DBLOCKTYPE_VERTEX_SHADER_RELEASE";
			break;
		case D3DBLOCKTYPE_PIXEL_SHADER_RELEASE:
			pBlockType = "D3DBLOCKTYPE_PIXEL_SHADER_RELEASE";
			break;
		case D3DBLOCKTYPE_VERTEX_BUFFER_RELEASE:
			pBlockType = "D3DBLOCKTYPE_VERTEX_BUFFER_RELEASE";
			break;
		case D3DBLOCKTYPE_VERTEX_BUFFER_LOCK:
			pBlockType = "D3DBLOCKTYPE_VERTEX_BUFFER_LOCK";
			break;
		case D3DBLOCKTYPE_INDEX_BUFFER_RELEASE:
			pBlockType = "D3DBLOCKTYPE_INDEX_BUFFER_RELEASE";
			break;
		case D3DBLOCKTYPE_INDEX_BUFFER_LOCK:
			pBlockType = "D3DBLOCKTYPE_INDEX_BUFFER_LOCK";
			break;
		case D3DBLOCKTYPE_TEXTURE_RELEASE:
			pBlockType = "D3DBLOCKTYPE_TEXTURE_RELEASE";
			break;
		case D3DBLOCKTYPE_TEXTURE_LOCK:
			pBlockType = "D3DBLOCKTYPE_TEXTURE_LOCK";
			break;
		case D3DBLOCKTYPE_COMMAND_BUFFER_RELEASE:
			pBlockType = "D3DBLOCKTYPE_COMMAND_BUFFER_RELEASE";
			break;
		case D3DBLOCKTYPE_COMMAND_BUFFER_LOCK:
			pBlockType = "D3DBLOCKTYPE_COMMAND_BUFFER_LOCK";
			break;
		case D3DBLOCKTYPE_CONSTANT_BUFFER_RELEASE:
			pBlockType = "D3DBLOCKTYPE_CONSTANT_BUFFER_RELEASE";
			break;
		case D3DBLOCKTYPE_CONSTANT_BUFFER_LOCK:
			pBlockType = "D3DBLOCKTYPE_CONSTANT_BUFFER_LOCK";
			break;

		NO_DEFAULT;
		};

		Warning( "D3D Block: %s for %.2f ms\n", pBlockType, ClockTime );
	}
}

static void r_blocking_spew_threshold_callback( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( Dx9Device() != NULL )
	{
		if ( ((ConVar *)var)->GetFloat() >= 0.0f )
		{
			Dx9Device()->SetBlockCallback( 0, D3DBlockingSpewCallback );
		}
		else
		{
			Dx9Device()->SetBlockCallback( 0, NULL );
		}
	}
}
ConVar r_blocking_spew_threshold( "r_blocking_spew_threshold", "-1", 0, "Enable spew of Direct3D Blocks. Specify the minimum blocking time in milliseconds before spewing a warning.", r_blocking_spew_threshold_callback );
#endif




#ifdef _PS3
#include "shaderapidx8_ps3nonvirt.inl"
#endif

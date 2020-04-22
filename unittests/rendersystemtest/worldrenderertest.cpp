//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Render system unit test
//=============================================================================

#include "rendersystemtest.h"
#include "inputsystem/iinputsystem.h"
#include "materialsystem2/imaterialsystem2.h"
#include "worldrenderer/iworldrenderermgr.h"
#include "meshsystem/imeshsystem.h"
#include "filesystem.h"
#include "vstdlib/jobthread.h"
#include "particles/particles.h"
#include "dynamicdrawhelper.h"
#include "tier0/vprof.h"
#include "keyvalues.h"
#include "scenesystem/iscenesystem.h"
#include "icommandline.h"
#include "inputsystem/iinputstacksystem.h"

struct usercmd_t
{
	float m_flForwardMove;
	float m_flRightMove;
	float m_flYawMove;
	float m_flPitchMove;
	uint32 m_nKeys;
};

struct BoxVertex_t
{
	Vector m_vPos;
};

#define FKEY_FORWARD		0x00000001
#define FKEY_BACK			0x00000002
#define FKEY_LEFT			0x00000004
#define FKEY_RIGHT			0x00000008
#define FKEY_TURNLEFT		0x00000010
#define FKEY_TURNRIGHT		0x00000020
#define FKEY_TURNUP			0x00000040
#define FKEY_TURNDOWN		0x00000080
#define FKEY_SHOOTDECAL		0x00000100
#define FKEY_SHOOTPSYSTEM	0x00000200
#define FKEY_SHOOTSSYSTEM	0x00000400
#define FKEY_LIGHTELV		0x00000800
#define FKEY_LIGHTANG_POS	0x00001000
#define FKEY_LIGHTANG_NEG	0x00002000
#define FKEY_FAST			0x00004000
#define FKEY_ENABLE_VIS		0x00008000
#define FKEY_DISABLE_VIS	0x00010000
#define FKEY_DROP_FRUSTUM	0x00020000
#define FKEY_TOGGLE_AERIAL	0x00040000
#define FKEY_TOGGLE_BOUNCE	0x00080000
#define FKEY_TOGGLE_DIRECT	0x00100000
#define FKEY_TOGGLE_LIGHTING_ONLY	0x00200000
#define FKEY_SSAO			0x00400000
#define FKEY_BLUR_LIGHTING	0x00800000
#define FKEY_BLUR_SHADOWS	0x01000000
#define FKEY_FLASHLIGHT		0x02000000
#define FKEY_SHOOTDECAL2	0x04000000
#define FKEY_SHOOTWALLMONSTER 0x08000000

#define NUM_SHADOW_SPLITS 3
#define NUM_MULTI_SHADOWS 20
#define NUM_HIGH_RES_SHADOWS 4
#define SQRT_NUM_HIGH_RES_SHADOWS 2
#define NUM_LOW_RES_SHADOWS 16
#define SQRT_NUM_LOW_RES_SHADOWS 4
#define MAX_LIGHT_DISTANCE 2000.0f

#define MAX_MESH_SYSTEM_MODELS 5
char *g_pRenderableList[ MAX_MESH_SYSTEM_MODELS ] = 
{
	"models/w_minigun_reference.dmx",
	"models/pm_heavy_reference.dmx",
	"models/w_bonesaw_reference.dmx",
	"models/w_guitar_reference.dmx",
	"models/w_harpahorn.dmx",
};
int g_nCurrentModel = 0;

struct DecalVertex_t
{
	Vector m_vPos;
	Vector m_vDecalOrigin;
	Vector m_vDecalRight;
	Vector4D m_vDecalUpAndInfluence;
};

struct SphereVertex_t
{
	Vector m_vPos;
};

struct QuadVertex_t
{
	Vector m_vPos;
	Vector m_vEyeRay;
};

struct ViewRelatedConstants_t
{
	VMatrix m_mViewProjection;
	Vector4D m_vEyePt;
	Vector4D m_vEyeDir;
	Vector4D m_flFarPlane;
};

struct LightingConstants_t
{
	Vector4D m_vLightDir;
	Vector4D m_vLightColor;
	VMatrix m_mShadowMatrices[NUM_SHADOW_SPLITS];
};

struct DeferredLightingConstants_t
{
	Vector4D m_vLightDir;
	Vector4D m_vLightColor;
	Vector4D m_vInvScreenExtents;
	Vector4D m_vEyePt;
	VMatrix  m_mInvViewProj;
	VMatrix  m_mShadowMatrix[NUM_SHADOW_SPLITS];
};

struct AerialPerspectiveConstants_t
{
	Vector4D m_vLightDir;
	Vector4D m_vLightColor;
	Vector4D m_vInvScreenExtents;
	Vector4D m_vBm;
	Vector4D m_flG;
	Vector4D m_vNearAndFar;
	Vector4D m_vEyePt;
	
	VMatrix m_mInvViewProj;
};

struct DecalScreenData_t
{
	Vector4D m_vInvScreenExtents;
	VMatrix  m_mInvViewProj;
	Vector4D m_vEyePt;
	Vector4D m_vEyeDir;
};

struct BlurParams_t
{
	Vector4D m_flBlurFalloff;
	Vector4D m_flSharpness;
	Vector4D m_flFarPlane;
};

#define NUM_SSAO_SAMPLES 16
struct SampleSphereData_t
{
	VMatrix  m_mViewProj;
	Vector4D m_vRandSampleScale;
	Vector4D m_vSampleRadiusNBias;
	Vector4D m_vSphereSamples[ NUM_SSAO_SAMPLES ];
};

struct LightBufferData_t
{
	Vector4D m_vInvScreenExtents;
};

struct SkyVSCB_t
{
	VMatrix m_mViewProj;
	Vector4D m_vSkyScale;
};

struct SkyPSCB_t
{
	Vector4D m_vLightDir;
	Vector4D m_vEyePt;
	Vector4D m_vLightColor;
	Vector4D m_vBm;
	Vector4D m_flG;
};

struct PerParticleData_t
{
	Vector4D m_vPosRadius;
	Vector4D m_vColor;
};

struct SimulateWorkUnitWorldRender_t
{
	CParticleCollection *m_pParticles;
	int m_nNumSystems;
	float m_flTimeStep;
};

const int g_nNothingRenderedStencilRef = 0;
const int g_nCanSeeSkyStencilRef = 1;
const int g_nCannotSeeSkyStencilRef = 2;

// Globalizing these so we can render nodes in parallel
class CWorldRendererTest;
CWorldRendererTest *g_pWorldRendererTest = NULL;

enum RenderViewFlags_t
{
	VIEW_PLAYER_CAMERA		= 0x00000001,
	VIEW_LIGHT_SHADOW		= 0x00000002,
	VIEW_REFLECTION			= 0x00000004,
	VIEW_MONITOR			= 0x00000008,
	VIEW_POST_PROCESS		= 0x00000010,
	VIEW_LIGHTING			= 0x00000020,
	VIEW_DECALS				= 0x00000040
};

enum RenderTargetType_t
{
	DS_SHADOW_DEPTH0 = 0x0,
	DS_SHADOW_DEPTH1,
	DS_SHADOW_DEPTH2,
	DS_SHADOW_DEPTH_MULTIPLE,
	RT_SHADOW_MULTIPLE_0,
	RT_SHADOW_MULTIPLE_1,
	RT_SHADOW_BLUR_TEMP,

	DS_DEPTH_HIGHRES,
	RT_VIEW_DEPTH_HIGHRES,
	RT_VIEW_NORMAL_HIGHRES,
	RT_VIEW_SPEC_HIGHRES,
	RT_LIGHTING_HIGHRES, // Alpha contains occlusion
	RT_SPECULAR_HIGHRES,

	DS_DEPTH_LOWRES,
	RT_VIEW_DEPTH_LOWRES,
	RT_VIEW_NORMAL_LOWRES,
	RT_LIGHTING_LOWRES,

	RT_LIGHTING_TEMP_LOWRES,

	MAX_RENDER_TARGET_TYPES
};

char *g_pRenderTargetName[] = 
{
	{"DS_SHADOW_DEPTH0"},
	{"DS_SHADOW_DEPTH1"},
	{"DS_SHADOW_DEPTH2"},
	{"DS_SHADOW_DEPTH_MULTIPLE"},
	{"RT_SHADOW_MULTIPLE_1"},
	{"RT_SHADOW_MULTIPLE_2"},
	{"RT_SHADOW_BLUR_TEMP"},

	{"DS_DEPTH_HIGHRES"},
	{"RT_VIEW_DEPTH_HIGHRES"},
	{"RT_VIEW_NORMAL_HIGHRES"},
	{"RT_VIEW_SPEC_HIGHRES"},
	{"RT_LIGHTING_HIGHRES"},
	{"RT_SPECULAR_HIGHRES"},

	{"DS_DEPTH_LOWRES"},
	{"RT_VIEW_DEPTH_LOWRES"},
	{"RT_VIEW_NORMAL_LOWRES"},
	{"RT_LIGHTING_LOWRES"},

	{"RT_LIGHTING_TEMP_LOWRES"},

	{"MAX_RENDER_TARGET_TYPES"}
};

enum ViewBindingType_t
{
	RTB_SHADOW_DEPTH0 = 0x0,
	RTB_SHADOW_DEPTH1,
	RTB_SHADOW_DEPTH2,

	RTB_SHADOW_DEPTH_MULTIPLE_0,
	RTB_SHADOW_DEPTH_MULTIPLE_1,
	RTB_SHADOW_DEPTH_MULTIPLE_2,
	RTB_SHADOW_DEPTH_MULTIPLE_3,

	RTB_SHADOW_DEPTH_MULTIPLE_4,
	RTB_SHADOW_DEPTH_MULTIPLE_5,
	RTB_SHADOW_DEPTH_MULTIPLE_6,
	RTB_SHADOW_DEPTH_MULTIPLE_7,

	RTB_SHADOW_DEPTH_MULTIPLE_8,
	RTB_SHADOW_DEPTH_MULTIPLE_9,
	RTB_SHADOW_DEPTH_MULTIPLE_10,
	RTB_SHADOW_DEPTH_MULTIPLE_11,

	RTB_SHADOW_DEPTH_MULTIPLE_12,
	RTB_SHADOW_DEPTH_MULTIPLE_13,
	RTB_SHADOW_DEPTH_MULTIPLE_14,
	RTB_SHADOW_DEPTH_MULTIPLE_15,

	RTB_SHADOW_DEPTH_MULTIPLE_16,
	RTB_SHADOW_DEPTH_MULTIPLE_17,
	RTB_SHADOW_DEPTH_MULTIPLE_18,
	RTB_SHADOW_DEPTH_MULTIPLE_19,

	RTB_SHADOW_DEPTH_MULTIPLE_20,
	RTB_SHADOW_DEPTH_MULTIPLE_21,
	RTB_SHADOW_DEPTH_MULTIPLE_22,
	RTB_SHADOW_DEPTH_MULTIPLE_23,

	RTB_SHADOW_DEPTH_MULTIPLE_24,
	RTB_SHADOW_DEPTH_MULTIPLE_25,
	RTB_SHADOW_DEPTH_MULTIPLE_26,
	RTB_SHADOW_DEPTH_MULTIPLE_27,

	RTB_SHADOW_DEPTH_MULTIPLE_28,
	RTB_SHADOW_DEPTH_MULTIPLE_29,
	RTB_SHADOW_DEPTH_MULTIPLE_30,
	RTB_SHADOW_DEPTH_MULTIPLE_31,

	RTB_DEFERRED_DEPTH_NORM_SPEC,
	RTB_DEFERRED_SCALE_DOWN,
	RTB_DEFERRED_NORM_SPEC,
	RTB_CLEAR_SPEC_HIGHRES,
	RTB_LIGHTING_HIGHRES,
	RTB_LIGHTING_LOWRES,
	RTB_SSAO_LOWRES,
	RTB_BILAT_UPSAMPLE_0,
	RTB_BILAT_UPSAMPLE_1,
	RTB_LIGHT_BLUR_0,
	RTB_LIGHT_BLUR_1,
	RTB_SHADOW_BLUR_0,
	RTB_SHADOW_BLUR_1,
	RTB_SHADOW_BLUR_2,
	RTB_SAMPLE_LIGHTING,
	RTB_AERIAL_PERSPECTIVE,
	RTB_SKY,

	RTB_FORWARD,

	MAX_VIEW_BINDING_TYPES
};

ViewRelatedConstants_t CreateViewConstantsForFrustum( CWorldRenderFrustum* pFrustum )
{
	const Vector vEyePt = pFrustum->GetCameraPosition();
	Vector vEyeDir = pFrustum->Forward();
	float flFarPlane = pFrustum->GetFarPlane();
	vEyeDir /= flFarPlane;

	ViewRelatedConstants_t viewConstants;
	viewConstants.m_mViewProjection = pFrustum->GetViewProj();
	viewConstants.m_vEyePt.Init( vEyePt.x, vEyePt.y, vEyePt.z, 1 );
	viewConstants.m_flFarPlane.Init( flFarPlane, flFarPlane, flFarPlane, 1 );
	viewConstants.m_vEyeDir.Init( vEyeDir.x, vEyeDir.y, vEyeDir.z, 1 );
	return viewConstants;
}

#define MAX_SIMUL_VIEWS 5

class CRenderView;

class CStaticMeshRenderable
{
public:
	CStaticMeshRenderable() {}
	~CStaticMeshRenderable() {}

	inline bool operator==( const CStaticMeshRenderable& src ) const { return src.m_pDrawCall == m_pDrawCall; }

	IBVHNode							*m_pNode;
	CBVHDrawCall						*m_pDrawCall;
};

class CDynamicRenderable
{
public:
	CDynamicRenderable() {}
	~CDynamicRenderable() {}

	inline bool operator==( const CDynamicRenderable &src ) const { return src.m_hRenderable == m_hRenderable; }

	HRenderableStrong m_hRenderable;
	VMatrix m_mWorld;
	Vector m_vOrigin;
};

class CPointLightRenderable
{
public:
	CPointLightRenderable() {}
	~CPointLightRenderable() {}

	inline bool operator==( const CPointLightRenderable& src ) const { return src.m_pLight == m_pLight; }

	PointLightData_t *m_pLight;
	Vector m_vOrigin;
};

class CSpotLightRenderable
{
public:
	CSpotLightRenderable() {}
	~CSpotLightRenderable() {}

	inline bool operator==( const CSpotLightRenderable& src ) const { return src.m_pLight == m_pLight; }

	SpotLightData_t *m_pLight;
	Vector m_vOrigin;
	float m_flSquaredDistToEye;
	bool m_bInterior;
	bool m_bValid;
};

class CHemiLightRenderable
{
public:
	CHemiLightRenderable() {}
	~CHemiLightRenderable() {}

	inline bool operator==( const CHemiLightRenderable& src ) const { return src.m_pLight == m_pLight; }

	HemiLightData_t *m_pLight;
	Vector m_vOrigin;
};

struct RenderJob_t
{
	CWorldRendererTest *m_pTestApp;
	CRenderView *m_pRenderView;
	int m_nStartStatic;
	int m_nCountStatic;

	int m_nStartDynamic;
	int m_nCountDynamic;

	int m_nIndex;
};

class CRenderView
{
public:
	CRenderView() {}
	~CRenderView() { g_pRenderDevice->DestroyRenderTargetBinding( m_hRenderTargetBinding ); }

	void Init( IRenderDevice* pRenderDevice,
			   CWorldRenderFrustum *pFrustum,
			   RenderTargetBinding_t hRenderTargetBinding, 
			   RenderViewport_t &viewport, 
			   RenderViewFlags_t nFlags, 
			   int nPass,
			   int nViewType )
	{
		g_pRenderDevice = pRenderDevice;
		m_pFrustum = pFrustum;
		m_hRenderTargetBinding = hRenderTargetBinding;
		m_viewport = viewport;
		m_nFlags = nFlags;
		m_nPass = nPass;
		m_nViewType = nViewType;
		m_bValid = true;
	}

	RenderTargetBinding_t GetRenderTargetBinding() { return m_hRenderTargetBinding; }
	RenderViewFlags_t GetViewFlags() { return m_nFlags; }
	int GetRenderPass() { return m_nPass; }
	int GetViewType() { return m_nViewType; }
	CWorldRenderFrustum *GetFrustum() { return m_pFrustum; }
	IRenderDevice *GetRenderDevice() { return g_pRenderDevice; }
	bool IsValid() { return m_bValid; }
	void SetValid( bool bValid ) { m_bValid = bValid; }

	void UpdateFrustum( CWorldRenderFrustum *pFrustum )
	{
		m_pFrustum = pFrustum;
	}

	void Clear( IRenderContext *pRenderContext, Vector4D &vClearColor, int nFlags )
	{
		Assert( pRenderContext );

		BeginRender( pRenderContext );

		pRenderContext->Clear( vClearColor, nFlags );

		// don't call endrender because it clears the rendercontext and the renderable list
		pRenderContext->Submit();
	}

	void BeginRender( IRenderContext *pRenderContext )
	{
		pRenderContext->BindRenderTargets( m_hRenderTargetBinding );
		pRenderContext->SetViewports( 1, &m_viewport );
	}

	void EndRender( IRenderContext *pRenderContext, bool bClearList = true )
	{
		if ( pRenderContext )
		{
			pRenderContext->Submit();
		}

		if ( bClearList )
		{
			if ( m_renderableList.Count() > 0 )
			{
				m_renderableList.SetCountNonDestructively( 0 );
			}
			if ( m_dynList.Count() > 0 )
			{
				m_dynList.SetCountNonDestructively( 0 );
			}
		}
	}

	void AddStaticMeshRenderable( CStaticMeshRenderable *pRenderable )
	{
		m_renderableList.AddToTail( pRenderable );
	}

	void AddDynamicRenderable( CDynamicRenderable *pRenderable )
	{
		m_dynList.AddToTail( pRenderable );
	}

	bool HasRenderables() { return ( ( m_renderableList.Count() != 0 ) || ( m_dynList.Count() != 0 ) ); }

	CUtlVector< CStaticMeshRenderable* > &GetRenderableList() { return m_renderableList; }
	CUtlVector< CDynamicRenderable* > &GetDynamicList() { return m_dynList; }
private:
	RenderTargetBinding_t				m_hRenderTargetBinding;
	RenderViewport_t					m_viewport;
	RenderViewFlags_t					m_nFlags;
	int									m_nPass;
	int									m_nViewType;
	bool								m_bValid;

	IRenderDevice						*g_pRenderDevice;
	CWorldRenderFrustum					*m_pFrustum;
	CUtlVector< CStaticMeshRenderable* > m_renderableList;
	CUtlVector< CDynamicRenderable* >	m_dynList;
};

float RPercent()
{
	return float( rand() - (RAND_MAX/2) ) / (float)(RAND_MAX/2);
}

float RPercentABS()
{
	return float( rand() ) / (float)(RAND_MAX);
}

//-----------------------------------------------------------------------------
// Tests the world renderer
//-----------------------------------------------------------------------------
class CWorldRendererTest : public CRenderSystemBenchmarkTest
{
	typedef CRenderSystemBenchmarkTest BaseClass;

public:
	CWorldRendererTest( int nVariant );
	virtual	~CWorldRendererTest( void );
	virtual void RenderFrame( const RenderViewport_t &viewport, PlatWindow_t hWnd );
	virtual void RenderFrame_Internal( const RenderViewport_t &viewport );
	virtual void SimulateFrame( float flTimeStep );
	virtual void GetDescription( char *pNameBufferOut, size_t nBufLen );
	virtual bool ProcessUserInput( const InputEvent_t &ie );
	virtual bool CanBeRun();

	void RenderViewStatic( CRenderView *pRenderView, int nStart, int nCount, int nIndex );
	void RenderViewDynamic( CRenderView *pRenderView, int nStart, int nCount, int nIndex );
	void SetStateForRenderViewType( IRenderContext *pRenderContext, int nRenderView );
	ConstantBufferHandle_t GetViewRelated() { return m_hViewRelated; }
	ConstantBufferHandle_t GetLightBufferRelated() { return m_hLightBufferData; }
	IResourceDictionary *GetResourceDictionary() { return m_pResourceDictionary; }
	RsDepthStencilStateHandle_t GetSetStencilDS() { return m_hSetStencilDS; }
	LightBufferData_t *GetLightBufferData() { return &m_lightBufferData; }

private:
	void SetupMultiShadowMatrices( int nViewportStart, int nSqrtNumShadows, float flBias );
	void SetupShadowMatrices();
	void CullRenderablesAgainstView( CRenderView &renderView, CUtlVector<CStaticMeshRenderable> &renderableList, CUtlVector<CDynamicRenderable> &dynList );
	void RenderView( CRenderView &renderView );
	void UpdateShadowData();
	void UpdateFlashlightData();
	void UpdateViewModelData();
	void UpdateDroppedRenderables( float flElapsedTime );
	void GenerateRenderables( Vector &vCameraPos );
	void GeneratePointLightRenderables( IBVHNode *pNode, Vector &vOrigin, CFrustum *pFrustum, Vector &vEye );
	void GenerateSpotLightRenderables( IBVHNode *pNode, Vector &vOrigin, CFrustum *pFrustum, Vector &vEye );
	void GenerateHemiLightRenderables( IBVHNode *pNode, Vector &vOrigin, CFrustum *pFrustum, Vector &vEye );

	void BilateralUpsample();
	void RenderSSAO();
	void ScaleDepthAndNormalBuffers();
	void RenderAerialPerspective();
	void RenderSky();

	void RenderShadowDepthBuffers( IRenderContext *pRenderContext );
	void RenderSunShadowFrustums( IRenderContext *pRenderContext );
	void RenderBouncedLight( IRenderContext *pRenderContext );
	void RenderDirectLight( IRenderContext *pRenderContext );
	void BlurLightBuffer( );
	void BlurShadowBuffers( );

	void RenderDecals( IRenderContext *pRenderContext, int nVariation );
	void RenderDynamic( IRenderContext *pRenderContext, CDynamicRenderable *pRenderable, int nVariation, bool bSunDepth, bool bSetStencil );
	void RenderParticleLights( IRenderContext *pRenderContext, CParticleCollection *pParticles, int nVariation, bool bSunDepth, bool bSetStencil );
	void RenderScenePointLights( IRenderContext *pRenderContext, CUtlVector<CPointLightRenderable> &lightVector, bool bInterior );
	void RenderSceneHemiLights( IRenderContext *pRenderContext, CUtlVector<CHemiLightRenderable> &lightVector, bool bInterior );
	void RenderSceneSpotLights( IRenderContext *pRenderContext, CUtlVector<CSpotLightRenderable> &lightVector, bool bInterior );
	void RenderShadowedSpotLights( IRenderContext *pRenderContext, CUtlVector<CSpotLightRenderable> &lightVector );
	void SimulateParticles( float flTimeStep );
	void CalculateEyeRays( float flFarPlane );
	void InitDeferredData( const RenderViewport_t &viewport );
	void InitParticleData();
	void InitRenderableData();
	void UpdateFrustum( int nWidth, int nHeight );
	void AddDecal( IRenderContext *pRenderContext, Vector &vHit, Vector &vDecalDir, float flDecalSizeU, float flDecalSizeV, float flDecalShift, float flDecalInfluenceLength, bool bPaint );
	void AddWallMonster( IRenderContext *pRenderContext, Vector &vHit, Vector &vDecalDir, float flDecalSizeU, float flDecalSizeV, float flDecalShift, float flDecalInfluenceLength );
	void DropRenderable();
	void CreateSphere( int nTessellation );
	void CreateHemi( int nTessellationRes );
	void CreateCone( int nTessellation );
	void RenderScreenQuad( IRenderContext *pRenderContext, Vector *pEyeRays, const char *pDebugName );
	RenderShaderHandle_t GetVertexShader( char *pShaderName, IRenderDevice *pDevice, const char *pShaderVersion, const char *pDefineText = NULL );
	RenderShaderHandle_t GetPixelShader( char *pShaderName, IRenderDevice *pDevice, const char *pShaderVersion, const char *pDefineText = NULL );
	HRenderTexture CreateRenderTarget( RenderTargetType_t type, const RenderViewport_t &viewport );
	RenderTargetBinding_t CreateRenderTargetBinding( ViewBindingType_t type, const RenderViewport_t &viewport );
	void InitRenderTargetsAndViews( const RenderViewport_t &viewport );

	CWorldRenderFrustum *m_pFrustum;
	CWorldRenderFrustum *m_pDropFrustum;
	Vector m_vCullPos;
	usercmd_t m_cmd;

	HRenderTextureStrong m_pRenderTargets[ MAX_RENDER_TARGET_TYPES ];
	RenderTargetBinding_t m_pViewBindings[ MAX_VIEW_BINDING_TYPES ];
	CRenderView m_pRenderViews[ MAX_VIEW_BINDING_TYPES ];
	int m_nRenderThreads;
	RenderJob_t *m_pRenderJobs;

	// Resources
	ConstantBufferHandle_t m_hViewRelated;
	ConstantBufferHandle_t m_hSingleMatrixRelated;
	ConstantBufferHandle_t m_hLightingRelated;
	ConstantBufferHandle_t m_hDeferredLightingRelated;
	ConstantBufferHandle_t m_hLightBufferData;
	ConstantBufferHandle_t m_hDecalScreenData;
	ConstantBufferHandle_t m_hBlurParams;
	ConstantBufferHandle_t m_hSphereSampleData;

	CWorldRenderFrustum *m_pSplitFrustums[ NUM_SHADOW_SPLITS ];
	VMatrix m_pShadowMatrices[ NUM_SHADOW_SPLITS ];
	CWorldRenderFrustum *m_pMultiShadowFrustums[ NUM_MULTI_SHADOWS ];
	VMatrix m_pMultiShadowMatrices[ NUM_MULTI_SHADOWS ];

	LightingConstants_t m_forwardLightingData;
	DeferredLightingConstants_t m_deferredLightingData;
	BlurParams_t m_blurParams;
	LightBufferData_t m_lightBufferData;
	AerialPerspectiveConstants_t m_aerialPerspectiveData;
	SampleSphereData_t m_sphereSampleData;
	float m_flSampleRadius;

	// Render list and renderable lists
	CUtlVector<IBVHNode*> m_renderList;
	CUtlVector<CStaticMeshRenderable> m_renderableList;
	CUtlVector<CDynamicRenderable> m_dynRenderableList;
	CUtlVector<CPointLightRenderable> m_exteriorPointLights;
	CUtlVector<CPointLightRenderable> m_interiorPointLights;
	CUtlVector<CSpotLightRenderable> m_exteriorSpotLights;
	CUtlVector<CSpotLightRenderable> m_interiorSpotLights;
	CUtlVector<CHemiLightRenderable> m_exteriorHemiLights;
	CUtlVector<CHemiLightRenderable> m_interiorHemiLights;
	CUtlVector<CSpotLightRenderable> m_shadowedSpotLights;
	SpotLightData_t m_flashlight;
	float m_flMaxLightEncompassingFarPlane;

	bool m_bEnableVis;
	bool m_bDropFrustum;
	bool m_bSSAO;
	bool m_bBlurLighting;
	bool m_bBlurShadows;
	bool m_bDrawAerialPerspective;
	bool m_bDrawBouncedLight;
	bool m_bDrawDirectLight;
	bool m_bLightingOnly;
	bool m_bFlashlight;
	bool m_bViewLowTexture;

	// Deferred resources
	bool m_bMultiThreaded;
	bool m_bDeferred;
	bool m_bDeferredInit;
	bool m_bRenderTargetsInit;
	
	RenderTargetType_t m_nViewBuffer;

	float m_flLowResBufferScale;

	RenderShaderHandle_t m_hCopyTexture[4];
	RenderShaderHandle_t m_hBilateralBlur[4];
	RenderShaderHandle_t m_hShadowBlur[4];

	RenderInputLayout_t	m_hQuadLayout;
	Vector m_vEyeRays[4];
	Vector m_vEyeLocalRays[4];

	int m_nRandTextureSize;
	HRenderTextureStrong m_hFlashlightCookie[2];
	HRenderTextureStrong m_hRandomTexture;
	RenderShaderHandle_t m_hSSAOPS;
	RenderShaderHandle_t m_hAerialPerspectivePS[2];
	ConstantBufferHandle_t m_hAerialPerspectivePSCB;
	RsBlendStateHandle_t m_hExtinctionBS;
	RsBlendStateHandle_t m_hInscatteringBS;
	RsBlendStateHandle_t m_hAdditiveBS;
	RsBlendStateHandle_t m_hWriteAlphaBS;
	RsBlendStateHandle_t m_hWriteAllBS;
	RsDepthStencilStateHandle_t m_hSetStencilDS;
	RsDepthStencilStateHandle_t m_hZWriteNoTestDS;

	RenderShaderHandle_t m_hShadowFrustumVS[3];
	RenderShaderHandle_t m_hShadowFrustumPS[3];

	int m_nSphereIndices;
	int m_nHemiIndices;
	int m_nConeIndices;
	HRenderBufferStrong m_hSphereVB;
	HRenderBufferStrong m_hSphereIB;
	HRenderBufferStrong m_hHemiVB;
	HRenderBufferStrong m_hHemiIB;
	HRenderBufferStrong m_hConeVB;
	HRenderBufferStrong m_hConeIB;

	RenderShaderHandle_t m_hSceneLightVS[3];
	RenderShaderHandle_t m_hSceneLightPS[6];
	RenderInputLayout_t m_hSceneLightLayout[3];

	// Sky
	RenderInputLayout_t m_hSphereLayout;
	RenderShaderHandle_t m_hSkyVS;
	RenderShaderHandle_t m_hSkyPS;
	ConstantBufferHandle_t m_hSkyVSCB;
	ConstantBufferHandle_t m_hSkyPSCB;
	SkyPSCB_t m_skyData;
	Vector4D m_vRenderLightColor;
	bool m_bRenderSky;
	RsDepthStencilStateHandle_t m_hZTestAndStencilTestDS;
	RsDepthStencilStateHandle_t m_hStencilTestDS;

	// Decals
	RenderInputLayout_t m_hDecalLayout;
	HRenderBufferStrong m_hDecalVB;
	HRenderBufferStrong m_hDecalVB2;
	HRenderBufferStrong m_hDecalVB3;
	HRenderBufferStrong m_hDecalIB;
	RenderShaderHandle_t m_hDecalVS[2];
	RenderShaderHandle_t m_hDecalPS[2];
	HRenderTextureStrong m_hDecalAlbedo;
	HRenderTextureStrong m_hDecalAlbedo2;
	HRenderTextureStrong m_hDecalNormal;
	HRenderTextureStrong m_hDecalNormal2;
	HRenderTextureStrong m_hDecalNormal3[3];
	DecalScreenData_t m_decalScreenData;
	DecalScreenData_t m_lowResScreenData;
	DecalScreenData_t m_shadowScreenData;
	DecalVertex_t *m_pDecalBackingStore;
	DecalVertex_t *m_pDecalBackingStore2;
	int	m_nMaxDecals;
	bool m_bFullBuffer;
	bool m_bFullBuffer2;
	int m_nGlobalDecalPos;
	int m_nGlobalDecalPos2;
	int m_nGlobalDecalPos3;
	Vector m_vPreviousMonsterNormal;
	int m_nWallMonsterTexture;
	float m_flWallMonsterTimer;
	int m_nNumTriangles;

	// Particles
	CUtlVector<CParticleCollection*> m_lightSystemList;
	CUtlVector<CParticleCollection*> m_sphereSystemList;
	CUtlVector<SimulateWorkUnitWorldRender_t> m_particleWorkUnits;
	RenderInputLayout_t	m_hPointLightLayout;
	RenderShaderHandle_t m_hPointLightVS[5];
	RenderShaderHandle_t m_hPointLightPS[5];

	// Renderable test
	RenderShaderHandle_t m_hSimpleMeshVS[4];
	RenderShaderHandle_t m_hSimpleMeshPS[4];

	Vector m_vLightDir;
	Vector2D m_vLightAngleElevation;
	float m_flLightAngSpeed;
	float m_flLightElvSpeed;
	int m_nShadowSplits;
	RenderViewport_t m_shadowViewport;
	RenderViewport_t m_multiShadowViewport[ NUM_MULTI_SHADOWS ];
	VMatrix m_splitScaleBiasMatrices[ NUM_SHADOW_SPLITS ];
	VMatrix m_multiShadowScaleBiasMatrices[ NUM_MULTI_SHADOWS ];
	int m_nCurrentFrameNumber;
	int m_nShadowSize;
	int m_nLastFrustumWidth;
	int m_nLastFrustumHeight;
	int m_nWindowCenterX;
	int m_nWindowCenterY;
	int m_nCursorDeltaX;
	int m_nCursorDeltaY;
	int m_nCursorDeltaResetX;
	int m_nCursorDeltaResetY;
	int m_nReportLevel;
	float m_flDecalCountdown;
	float m_flParticleCountdown;

	float m_flNearPlane;
	float m_flFarPlane;

	// WorldRenderer
	IWorldRendererMgr *m_pWorldRendererMgr;
	IWorldRenderer *m_pWorldRenderer;
	IResourceDictionary *m_pResourceDictionary;
	bool m_bWorldLoaded;
};

DECLARE_TEST( CWorldRendererTest );

static void PerformStaticMeshWorkUnit( RenderJob_t & job )
{
	job.m_pTestApp->RenderViewStatic( job.m_pRenderView, job.m_nStartStatic, job.m_nCountStatic, job.m_nIndex );
	job.m_pTestApp->RenderViewDynamic( job.m_pRenderView, job.m_nStartDynamic, job.m_nCountDynamic, job.m_nIndex );
}

static int g_pBoxIndices[36] = { 2,1,0,3,2,0, 6,5,1,2,6,1, 7,4,5,6,7,5, 3,0,4,7,3,4, 6,2,3,7,6,3, 1,5,4,0,1,4 };

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CWorldRendererTest::CWorldRendererTest( int nVariant ) : BaseClass( nVariant )
{
	if ( !g_pWorldRendererMgr )
		return;

	g_pWorldRendererTest = this;

	// Disable HW instancing on GL for now
	if ( g_nPlatform == RST_PLATFORM_GL )
		g_pWorldRendererMgr->SetHWInstancingEnabled( false );

	CRenderContextPtr pRenderContext( g_pRenderDevice );

	m_flDecalCountdown = 0.0f;
	m_flParticleCountdown = 0.0f;

	if ( IsPlatformX360() )
	{
		m_nShadowSize = 512;
	}
	else
	{
		m_nShadowSize = 1024;
	}

	m_shadowViewport.Init( 0, 0, m_nShadowSize, m_nShadowSize, 0, 1 );

	SetupShadowMatrices();

	m_nViewBuffer = RT_LIGHTING_HIGHRES;

	m_nRenderThreads = g_pThreadPool->NumThreads();
	m_pRenderJobs = new RenderJob_t[ m_nRenderThreads ];

	m_nCurrentFrameNumber = 0;
	m_hViewRelated = g_pRenderDevice->CreateConstantBuffer( sizeof( ViewRelatedConstants_t ) );
	m_hSingleMatrixRelated = g_pRenderDevice->CreateConstantBuffer( sizeof( VMatrix ) );
	m_hLightingRelated = g_pRenderDevice->CreateConstantBuffer( sizeof( LightingConstants_t ) );
	m_hSkyVSCB = g_pRenderDevice->CreateConstantBuffer( sizeof( SkyVSCB_t ) );
	m_hSkyPSCB = g_pRenderDevice->CreateConstantBuffer( sizeof( SkyPSCB_t ) );

	//m_flLowResBufferScale = 0.5f;
	m_flLowResBufferScale = 0.5f;
	m_bEnableVis = true;
	m_bDropFrustum = false;
	m_bSSAO = true;
	m_bBlurLighting = false;
	m_bBlurShadows = true;
	m_bDrawAerialPerspective = true;
	m_bDrawBouncedLight = true;
	m_bDrawDirectLight = true;
	m_bLightingOnly = false;
	m_bFlashlight = false;
	m_bViewLowTexture = false;

	// Deferred
	m_bDeferred = true;
	m_bDeferredInit = false;
	m_bRenderTargetsInit = false;
	m_bMultiThreaded = true;

	// No deferred on GL yet
	if ( g_nPlatform == RST_PLATFORM_GL || g_nPlatform == RST_PLATFORM_X360 )
		m_bDeferred = false;

	m_bWorldLoaded = true;
	// Create a world renderer
	m_pWorldRenderer = g_pWorldRendererMgr->CreateWorldRenderer();

	// Load the world
	const char *pWorldName = CommandLine()->ParmValue( "-map", "maps/ep2_outland_10" );
	if ( !m_pWorldRenderer->Unserialize( pWorldName ) )
	{
		Error( "Cannot load world!\n" );
		m_bWorldLoaded = false;
		return;	   
	}

	m_pResourceDictionary = m_pWorldRenderer->GetResourceDictionary();

	// Initialization
	static const uint64 sMegabyte = 1024 * 1024;
	uint64 nMaxGPUMemory = 768 * sMegabyte;
	uint64 nMaxSysMemory = 512 * sMegabyte;

	if ( IsPlatformX360() )
	{
		// Set this low until texture eviction works on the 360
		nMaxGPUMemory = 256 * sMegabyte;
		nMaxSysMemory = 32 * sMegabyte;
	}
	m_pWorldRenderer->Initialize( g_pRenderDevice, nMaxGPUMemory, nMaxSysMemory );

	// Alloc split frustums
	for ( int s=0; s<NUM_SHADOW_SPLITS; ++s )
	{
		m_pSplitFrustums[s] = (CWorldRenderFrustum*)MemAlloc_AllocAligned( sizeof( CWorldRenderFrustum ), 16 );
	}

	// Alloc multi-shadow frustums
	for ( int s=0; s<NUM_MULTI_SHADOWS; ++s )
	{
		m_pMultiShadowFrustums[s] = (CWorldRenderFrustum*)MemAlloc_AllocAligned( sizeof( CWorldRenderFrustum ), 16 );
	}

	// Build up a frustum
	m_pFrustum = (CWorldRenderFrustum*)MemAlloc_AllocAligned( sizeof( CWorldRenderFrustum ), 16 );
	m_pDropFrustum = (CWorldRenderFrustum*)MemAlloc_AllocAligned( sizeof( CWorldRenderFrustum ), 16 );

	m_nLastFrustumWidth = 1024;
	m_nLastFrustumHeight = 768;
	float flFOVY = 90.0f;
	float flAspect = m_nLastFrustumWidth / (float)m_nLastFrustumHeight;

	// Find the player start
	Vector vEyePos( 0, 0, 0 );
	QAngle vAngles( -64, 90, 0 );

	CUtlVector< KeyValues* > playerstarts;
	m_pWorldRenderer->GetEntities( "info_player_start", playerstarts );
	if ( playerstarts.Count() )
	{
		const char *pEye = playerstarts[0]->GetString( "origin" );
		const char *pAngles = playerstarts[0]->GetString( "angles" );

		sscanf( pEye, "%f %f %f", &vEyePos.x, &vEyePos.y, &vEyePos.z );
		sscanf( pAngles, "%f %f %f", &vAngles.x, &vAngles.y, &vAngles.z );
		vEyePos.z += 64;
	}

	// eyepos, eyeangles, znear, zfar, fov, aspect ratio
	m_flNearPlane = 10.0f;
	m_flFarPlane = 18000.0f;
	m_pFrustum->InitCamera( vEyePos, vAngles, m_flNearPlane, m_flFarPlane, CalcFovX(flFOVY, flAspect), flAspect );
	m_pFrustum->UpdateFrustumFromCamera();

	// parallel spit shadow maps
	m_vLightAngleElevation.x = 120.0f;
	m_vLightAngleElevation.y = 70.0f;
	m_vLightDir = Vector(0,0,1);
	Q_memset( &m_skyData, 0, sizeof( SkyPSCB_t ) );
	m_vRenderLightColor = Vector4D(1,1,1,1);
	m_flLightAngSpeed = 20.0f;
	m_flLightElvSpeed = 20.0f;

	m_nShadowSplits = NUM_SHADOW_SPLITS;

	const char *pVertexShaderVersion = g_pRenderDevice->GetShaderVersionString( RENDER_VERTEX_SHADER );
  	const char *pPixelShaderVersion = g_pRenderDevice->GetShaderVersionString( RENDER_PIXEL_SHADER );

	// sphere and cone primitives
	CreateSphere( 10 );
	CreateHemi( 10 );
	CreateCone( 10 );

	if ( m_bDeferred )
	{
		// Decals
		m_nMaxDecals = 2000;
		m_bFullBuffer = false;
		m_nGlobalDecalPos = 0;
		m_bFullBuffer2 = false;
		m_nGlobalDecalPos2 = 0;
		m_nGlobalDecalPos3 = 0;
		m_vPreviousMonsterNormal.Init( 0, 0, 1 );
		m_nWallMonsterTexture = 0;
		m_flWallMonsterTimer = 0.0f;

		static RenderInputLayoutField_t decalLayout[] =
		{
			DEFINE_PER_VERTEX_FIELD( 0, "position", 0, DecalVertex_t, m_vPos )
			DEFINE_PER_VERTEX_FIELD( 0, "texcoord",	0, DecalVertex_t, m_vDecalOrigin )
			DEFINE_PER_VERTEX_FIELD( 0, "texcoord",	1, DecalVertex_t, m_vDecalRight )
			DEFINE_PER_VERTEX_FIELD( 0, "texcoord", 2, DecalVertex_t, m_vDecalUpAndInfluence )
		};

		m_hDecalLayout = g_pRenderDevice->CreateInputLayout( "decallayout", ARRAYSIZE( decalLayout ), decalLayout );

		BufferDesc_t decalDesc;
		decalDesc.m_nElementCount = m_nMaxDecals * 8;
		decalDesc.m_nElementSizeInBytes = sizeof( DecalVertex_t );
		decalDesc.m_pBudgetGroupName = "decals";
		decalDesc.m_pDebugName = "decalVB";
		m_hDecalVB = g_pRenderDevice->CreateVertexBuffer( RENDER_BUFFER_TYPE_SEMISTATIC, decalDesc );
		decalDesc.m_pDebugName = "decalVB2";
		m_hDecalVB2 = g_pRenderDevice->CreateVertexBuffer( RENDER_BUFFER_TYPE_SEMISTATIC, decalDesc );
		decalDesc.m_nElementCount = 8;
		decalDesc.m_pDebugName = "decalVB3";
		m_hDecalVB3 = g_pRenderDevice->CreateVertexBuffer( RENDER_BUFFER_TYPE_SEMISTATIC, decalDesc );
		
		decalDesc.m_nElementCount = m_nMaxDecals * 36;
		decalDesc.m_nElementSizeInBytes = sizeof( uint16 );
		decalDesc.m_pDebugName = "decalIB";
		m_hDecalIB = g_pRenderDevice->CreateIndexBuffer( RENDER_BUFFER_TYPE_STATIC, decalDesc );

		LockDesc_t lockDesc;
		pRenderContext->LockIndexBuffer( m_hDecalIB, m_nMaxDecals * 36 * sizeof( uint16 ), &lockDesc );
		uint16 *pIndices = (uint16*)lockDesc.m_pMemory;
		
		int nOffset = 0;
		for ( int d=0; d<m_nMaxDecals; ++d )
		{
			for ( int i=0; i<36; ++i )
			{
				pIndices[i] = g_pBoxIndices[i] + nOffset;
			}
			nOffset += 8;
			pIndices += 36;
		}
		pRenderContext->UnlockIndexBuffer( m_hDecalIB, m_nMaxDecals * 36 * sizeof( uint16 ), &lockDesc );

		m_hDecalVS[0] = GetVertexShader( "maps/decalsvs", g_pRenderDevice, pVertexShaderVersion, "#define SHADER_VARIATION 0\n" );
		m_hDecalPS[0] = GetPixelShader( "maps/decalsps", g_pRenderDevice, pPixelShaderVersion, "#define SHADER_VARIATION 0\n" );
		m_hDecalVS[1] = GetVertexShader( "maps/decalsvs", g_pRenderDevice, pVertexShaderVersion, "#define SHADER_VARIATION 1\n" );
		m_hDecalPS[1] = GetPixelShader( "maps/decalsps", g_pRenderDevice, pPixelShaderVersion, "#define SHADER_VARIATION 1\n" );

		m_hDecalScreenData = g_pRenderDevice->CreateConstantBuffer( sizeof( DecalScreenData_t ) );

		m_hDecalAlbedo = RENDER_TEXTURE_HANDLE_INVALID;//g_pRenderDevice->FindOrCreateFileTexture( "maps/decaltest_albedo.vtf" );
		//m_hDecalNormal = g_pRenderDevice->FindOrCreateFileTexture( "maps/decaltest_normal.vtf" );
		m_hDecalNormal = g_pRenderDevice->FindOrCreateFileTexture( "materials/plastercrack.vtf" );
		m_hDecalAlbedo2 = g_pRenderDevice->FindOrCreateFileTexture( "materials/paintdecal.vtf" );
		m_hDecalNormal2 = g_pRenderDevice->FindOrCreateFileTexture( "materials/paintdecalnorm.vtf" );
		m_hDecalNormal3[0] = g_pRenderDevice->FindOrCreateFileTexture( "materials/gearhead1.vtf" );
		m_hDecalNormal3[1] = g_pRenderDevice->FindOrCreateFileTexture( "materials/gearhead2.vtf" );
		m_hDecalNormal3[2] = g_pRenderDevice->FindOrCreateFileTexture( "materials/gearhead3.vtf" );

		m_pDecalBackingStore = new DecalVertex_t[ m_nMaxDecals * 8 ];
		m_pDecalBackingStore2 = new DecalVertex_t[ m_nMaxDecals * 8 ];
	}

	m_bRenderSky = true;
	if ( m_bRenderSky )
	{
		// Load shaders
		m_hSkyVS = GetVertexShader( "maps/skyvs", g_pRenderDevice, pVertexShaderVersion );
		m_hSkyPS = GetPixelShader( "maps/skyps", g_pRenderDevice, pPixelShaderVersion );
	}

	if ( m_bDeferred )
	{
		// particle system
		InitParticleData();
	}

	InitRenderableData();

	// Input
	Q_memset( &m_cmd, 0, sizeof( usercmd_t ) );
	m_nWindowCenterX = -1;
	m_nWindowCenterY = -1;
	m_nCursorDeltaX = 0;
	m_nCursorDeltaY = 0;
	m_nCursorDeltaResetX = 0;
	m_nCursorDeltaResetY = 0;
	m_nReportLevel = 0;

	g_pInputStackSystem->SetCursorVisible( GetInputContext(), false );
}


CWorldRendererTest::~CWorldRendererTest( void )
{
	if ( !g_pWorldRendererMgr )
		return;

	g_pInputStackSystem->SetCursorVisible( GetInputContext(), true );

	g_pRenderDevice->DestroyConstantBuffer( m_hViewRelated );
	g_pRenderDevice->DestroyConstantBuffer( m_hLightingRelated );

	m_pWorldRenderer->DestroyResources( g_pRenderDevice );
	g_pWorldRendererMgr->DestroyWorldRenderer( m_pWorldRenderer );

	m_dynRenderableList.RemoveAll();

	for ( int s=0; s<NUM_SHADOW_SPLITS; ++s )
	{
		MemAlloc_FreeAligned( m_pSplitFrustums[s] );
	}
	for ( int s=0; s<NUM_MULTI_SHADOWS; ++s )
	{
		MemAlloc_FreeAligned( m_pMultiShadowFrustums[s] );
	}

	MemAlloc_FreeAligned( m_pFrustum );
}


class CUniformSampler
{
private:
	Vector*			m_pvDirections;
	int				m_NumSamples;
	int				m_NumVariations;

public:
	CUniformSampler() :
	  m_pvDirections( NULL ),
	  m_NumSamples( 0 ),
	  m_NumVariations( 0 )
	  {
	  }
	~CUniformSampler()
	{
		if ( m_pvDirections ) { delete []m_pvDirections; };
	}

	int GetNumVariations() { return m_NumVariations; }
	int GetNumSamples() { return m_NumSamples; }
	bool InitSamples( int SqrtNumSamples, int NumVariations );
	Vector GetSampleDirection( int Sample, int Variation );
};


bool CUniformSampler::InitSamples( int SqrtNumSamples, int NumVariations )
{
	m_NumSamples = SqrtNumSamples * SqrtNumSamples;
	m_NumVariations = NumVariations;

	m_pvDirections = new Vector[ m_NumSamples * m_NumVariations ];
	if ( !m_pvDirections )
		return false;
#if 1
	int i=0; // array index
	float oneoverN = 1.0f/(float)SqrtNumSamples;

	for ( int n=0; n<m_NumVariations; n++ )
	{
		// fill an N*N*2 array with uniformly distributed
		// samples across the sphere using jittered stratification
		for ( int a=0; a<SqrtNumSamples; a++ ) 
		{
			for ( int b=0; b<SqrtNumSamples; b++ ) 
			{
				// generate unbiased distribution of spherical coords
				float x = ( a + fabs( RPercent() ) ) * oneoverN; // do not reuse results
				float y = ( b + fabs( RPercent() ) ) * oneoverN; // each sample must be random
				float theta = 2.0f * acosf(sqrtf(1.0f - x));
				float phi = 2.0f * M_PI * y;

				// convert spherical coords to unit vector
				Vector vec(sinf(theta)*cosf(phi), sinf(theta)*sinf(phi), cosf(theta));
				m_pvDirections[i] = vec;

				++i;
			}
		}
	}
#else
	int i=0;
	for ( int n=0; n<m_NumVariations; n++ )
	{
		// fill an N*N*2 array with uniformly distributed
		// samples across the sphere using jittered stratification
		for ( int a=0; a<SqrtNumSamples * SqrtNumSamples; a++ ) 
		{	
			m_pvDirections[i].x = RPercent();
			m_pvDirections[i].y = RPercent();
			m_pvDirections[i].z = RPercent();
			m_pvDirections[i].NormalizeInPlace();
			i++;
		}
	}
#endif
	return true;
}

Vector CUniformSampler::GetSampleDirection( int Sample, int Variation )
{
	int Start = m_NumSamples * Variation;
	return m_pvDirections[ Start + Sample ];
}

void CWorldRendererTest::InitDeferredData( const RenderViewport_t &viewport )
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );

	static RenderInputLayoutField_t quadLayout[] =
	{
		DEFINE_PER_VERTEX_FIELD( 0, "position", 0, QuadVertex_t, m_vPos )
		DEFINE_PER_VERTEX_FIELD( 0, "texcoord", 0, QuadVertex_t, m_vEyeRay )
	};
	m_hQuadLayout = g_pRenderDevice->CreateInputLayout( "quadlayout", ARRAYSIZE( quadLayout ), quadLayout );

	// Constants
	m_hDeferredLightingRelated = g_pRenderDevice->CreateConstantBuffer( sizeof( DeferredLightingConstants_t ) );
	m_hLightBufferData = g_pRenderDevice->CreateConstantBuffer( sizeof( LightBufferData_t ) );

	const char *pVertexShaderVersion = g_pRenderDevice->GetShaderVersionString( RENDER_VERTEX_SHADER );
	const char *pPixelShaderVersion = g_pRenderDevice->GetShaderVersionString( RENDER_PIXEL_SHADER );

	// Shaders for low-res buffers
	//float flBlurFalloff = 0.0005f;
	float flBlurFalloff = 0.005f;
	float flSharpness = 1000.0f;
	m_blurParams.m_flBlurFalloff.Init( flBlurFalloff, flBlurFalloff, flBlurFalloff, flBlurFalloff );
	m_blurParams.m_flSharpness.Init( flSharpness, flSharpness, flSharpness, flSharpness );
	m_blurParams.m_flFarPlane.Init( 1, 1, 1, 1 );
	m_hBlurParams = g_pRenderDevice->CreateConstantBuffer( sizeof( BlurParams_t ) );
	m_hCopyTexture[0] = GetPixelShader( "maps/copytextureps", g_pRenderDevice, pPixelShaderVersion, "#define SINGLE_TARGET 0\n#define COPY_TARGET_W 0\n#define COMBINE_LIGHTING 0\n" );
	m_hCopyTexture[1] = GetPixelShader( "maps/copytextureps", g_pRenderDevice, pPixelShaderVersion, "#define SINGLE_TARGET 1\n#define COPY_TARGET_W 0\n#define COMBINE_LIGHTING 0\n" );
	m_hCopyTexture[2] = GetPixelShader( "maps/copytextureps", g_pRenderDevice, pPixelShaderVersion, "#define SINGLE_TARGET 1\n#define COPY_TARGET_W 1\n#define COMBINE_LIGHTING 0\n" );
	m_hCopyTexture[3] = GetPixelShader( "maps/copytextureps", g_pRenderDevice, pPixelShaderVersion, "#define SINGLE_TARGET 1\n#define COPY_TARGET_W 0\n#define COMBINE_LIGHTING 1\n" );
	m_hBilateralBlur[0] = GetPixelShader( "maps/bilateralblur", g_pRenderDevice, pPixelShaderVersion, "#define BILAT_BLUR 1\n#define HORZ_BLUR 1\n#define BLUR_RADIUS 8\n"  );
	m_hBilateralBlur[1] = GetPixelShader( "maps/bilateralblur", g_pRenderDevice, pPixelShaderVersion, "#define BILAT_BLUR 1\n#define HORZ_BLUR 0\n#define BLUR_RADIUS 8\n"  );
	m_hBilateralBlur[2] = GetPixelShader( "maps/bilateralblur", g_pRenderDevice, pPixelShaderVersion, "#define BILAT_BLUR 1\n#define HORZ_BLUR_FULL 1\n#define BLUR_RADIUS 6\n"  );
	m_hBilateralBlur[3] = GetPixelShader( "maps/bilateralblur", g_pRenderDevice, pPixelShaderVersion, "#define BILAT_BLUR 1\n#define HORZ_BLUR 0\n#define BLUR_RADIUS 6\n"  );

	m_hShadowBlur[0] = GetPixelShader( "maps/bilateralblur", g_pRenderDevice, pPixelShaderVersion, "#define BILAT_BLUR 0\n#define HORZ_BLUR_FULL 1\n#define BLUR_RADIUS 3\n"  );
	m_hShadowBlur[1] = GetPixelShader( "maps/bilateralblur", g_pRenderDevice, pPixelShaderVersion, "#define BILAT_BLUR 0\n#define HORZ_BLUR 0\n#define BLUR_RADIUS 3\n"  );
	m_hShadowBlur[2] = GetPixelShader( "maps/bilateralblur", g_pRenderDevice, pPixelShaderVersion, "#define BILAT_BLUR 0\n#define HORZ_BLUR_FULL 1\n#define BLUR_RADIUS 2\n"  );
	m_hShadowBlur[3] = GetPixelShader( "maps/bilateralblur", g_pRenderDevice, pPixelShaderVersion, "#define BILAT_BLUR 0\n#define HORZ_BLUR 0\n#define BLUR_RADIUS 2\n"  );

	// Shaders for deferred ligthing
	m_hShadowFrustumVS[0] = GetVertexShader( "maps/shadowfrustumvs", g_pRenderDevice, pVertexShaderVersion, "#define DISCARD_OUTSIDE 1\n#define ALL_FRUSTUMS 0\n"  );
	m_hShadowFrustumPS[0] = GetPixelShader( "maps/shadowfrustumps", g_pRenderDevice, pPixelShaderVersion, "#define DISCARD_OUTSIDE 1\n#define ALL_FRUSTUMS 0\n"  );
	m_hShadowFrustumVS[1] = GetVertexShader( "maps/shadowfrustumvs", g_pRenderDevice, pVertexShaderVersion, "#define ALL_FRUSTUMS 0\n" );
	m_hShadowFrustumPS[1] = GetPixelShader( "maps/shadowfrustumps", g_pRenderDevice, pPixelShaderVersion, "#define ALL_FRUSTUMS 0\n" );
	m_hShadowFrustumVS[2] = GetVertexShader( "maps/shadowfrustumvs", g_pRenderDevice, pVertexShaderVersion, "#define ALL_FRUSTUMS 1\n"  );
	m_hShadowFrustumPS[2] = GetPixelShader( "maps/shadowfrustumps", g_pRenderDevice, pPixelShaderVersion, "#define ALL_FRUSTUMS 1\n"  );

	// Scene light shaders
	m_hSceneLightVS[0] = GetVertexShader( "maps/scenelightvs", g_pRenderDevice, pVertexShaderVersion, "#define LIGHT_TYPE 0\n"  );
	m_hSceneLightVS[1] = GetVertexShader( "maps/scenelightvs", g_pRenderDevice, pVertexShaderVersion, "#define LIGHT_TYPE 1\n"  );
	m_hSceneLightVS[2] = GetVertexShader( "maps/scenelightvs", g_pRenderDevice, pVertexShaderVersion, "#define LIGHT_TYPE 2\n"  );

	m_hSceneLightPS[0] = GetPixelShader( "maps/scenelightps", g_pRenderDevice, pPixelShaderVersion,   "#define LIGHT_TYPE 0\n#define LIGHT_SHADOWED 0\n"  );
	m_hSceneLightPS[1] = GetPixelShader( "maps/scenelightps", g_pRenderDevice, pPixelShaderVersion,   "#define LIGHT_TYPE 1\n#define LIGHT_SHADOWED 0\n"  );
	m_hSceneLightPS[2] = GetPixelShader( "maps/scenelightps", g_pRenderDevice, pPixelShaderVersion,   "#define LIGHT_TYPE 2\n#define LIGHT_SHADOWED 0\n"  );
	m_hSceneLightPS[3] = GetPixelShader( "maps/scenelightps", g_pRenderDevice, pPixelShaderVersion,   "#define LIGHT_TYPE 0\n#define LIGHT_SHADOWED 1\n"  );
	m_hSceneLightPS[4] = GetPixelShader( "maps/scenelightps", g_pRenderDevice, pPixelShaderVersion,   "#define LIGHT_TYPE 1\n#define LIGHT_SHADOWED 1\n"  );
	m_hSceneLightPS[5] = GetPixelShader( "maps/scenelightps", g_pRenderDevice, pPixelShaderVersion,   "#define LIGHT_TYPE 2\n#define LIGHT_SHADOWED 1\n"  );

	static RenderInputLayoutField_t pointLightLayout[] =
	{
		DEFINE_PER_VERTEX_FIELD( 0, "position", 0, SphereVertex_t, m_vPos )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 0, PointLightData_t, m_vOrigin )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 1, PointLightData_t, m_vColorNRadius )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 2, PointLightData_t, m_vAttenuation )
	};
	static RenderInputLayoutField_t hemiLightLayout[] =
	{
		DEFINE_PER_VERTEX_FIELD( 0, "position", 0, SphereVertex_t, m_vPos )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 0, HemiLightData_t, m_vTransform0 )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 1, HemiLightData_t, m_vTransform1 )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 2, HemiLightData_t, m_vTransform2 )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 3, HemiLightData_t, m_vColorNRadius )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 4, HemiLightData_t, m_vAttenuation )
	};
	static RenderInputLayoutField_t spotLightLayout[] =
	{
		DEFINE_PER_VERTEX_FIELD( 0, "position", 0, SphereVertex_t, m_vPos )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 0, SpotLightData_t, m_vTransform0 )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 1, SpotLightData_t, m_vTransform1 )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 2, SpotLightData_t, m_vTransform2 )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 3, SpotLightData_t, m_vColorNRadius )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 4, SpotLightData_t, m_vAttenuationNCosSpot )
	};

	m_hSceneLightLayout[0] = g_pRenderDevice->CreateInputLayout( "scenelight_point", ARRAYSIZE( pointLightLayout ), pointLightLayout );
	m_hSceneLightLayout[1] = g_pRenderDevice->CreateInputLayout( "scenelight_hemi", ARRAYSIZE( hemiLightLayout ), hemiLightLayout );
	m_hSceneLightLayout[2] = g_pRenderDevice->CreateInputLayout( "scenelight_spot", ARRAYSIZE( spotLightLayout ), spotLightLayout );

	m_hFlashlightCookie[0] = g_pRenderDevice->FindOrCreateFileTexture( "materials/flashlight4.vtf" );
	m_hFlashlightCookie[1] = g_pRenderDevice->FindOrCreateFileTexture( "materials/flashlight16.vtf" );

	// SSAO
	m_hSSAOPS = GetPixelShader( "maps/ssao", g_pRenderDevice, pPixelShaderVersion );

	CUniformSampler ssaoSampler;
	ssaoSampler.InitSamples( (int)sqrtf( (float)NUM_SSAO_SAMPLES ), 1 );

	m_hSphereSampleData = g_pRenderDevice->CreateConstantBuffer( sizeof( SampleSphereData_t ) );
	m_flSampleRadius = 16.0f;
	m_sphereSampleData.m_vRandSampleScale.Init( 1, 1, 1, 1 );
	m_sphereSampleData.m_vSampleRadiusNBias.Init( m_flSampleRadius, m_flSampleRadius, m_flSampleRadius, m_flSampleRadius );
	for ( int s=0; s<NUM_SSAO_SAMPLES; ++s )
	{
		Vector vSample = ssaoSampler.GetSampleDirection( s, 0 );
		//vSample *= 0.2f + 0.8f * RPercentABS();
		vSample *= RPercentABS();
		m_sphereSampleData.m_vSphereSamples[s].Init( vSample.x, vSample.y, vSample.z, 1 );
	}
	pRenderContext->SetConstantBufferData( m_hSphereSampleData, &m_sphereSampleData, sizeof( SampleSphereData_t ) );

#if 1
	// random texture
	m_nRandTextureSize = 4;
	CUniformSampler randTexSampler;
	randTexSampler.InitSamples( m_nRandTextureSize, 1 );

	TextureHeader_t randSpec;
	memset( &randSpec, 0, sizeof(TextureHeader_t) );
	randSpec.m_nWidth = m_nRandTextureSize;
	randSpec.m_nHeight = m_nRandTextureSize;
	randSpec.m_nNumMipLevels = 1;
	randSpec.m_nDepth = 1;
	randSpec.m_nFlags = TSPEC_UNFILTERABLE_OK;
	randSpec.m_nImageFormat = IMAGE_FORMAT_BGRA8888;
	if ( g_nPlatform == RST_PLATFORM_DX11 )
	{
		randSpec.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
	}
	m_hRandomTexture = g_pRenderDevice->FindOrCreateTexture( "worldrenderertest", "randomtexture", &randSpec );
	
	int nTotalRands = randSpec.m_nWidth * randSpec.m_nHeight;
	uint8 *pRandBits = new uint8[ nTotalRands * 4 ];
	for ( int r=0; r<nTotalRands; ++r )
	{
		Vector vRandVector = randTexSampler.GetSampleDirection( r, 0 );

		pRandBits[ r * 4     ] = (uint8)( ( vRandVector.z * 128.0f ) + 127 );
		pRandBits[ r * 4 + 1 ] = (uint8)( ( vRandVector.y * 128.0f ) + 127 );
		pRandBits[ r * 4 + 2 ] = (uint8)( ( vRandVector.x * 128.0f ) + 127 );
		pRandBits[ r * 4 + 3 ] = 0;
	}

	pRenderContext->SetTextureData( m_hRandomTexture, &randSpec, pRandBits, nTotalRands * 4, false );
	delete []pRandBits;
#else
	// random texture
	m_nRandTextureSize = 32;
	m_hRandomTexture = g_pRenderDevice->FindOrCreateFileTexture( "materials/SSAOReflectionVectors.vtf" );
#endif

	// Aerial perspective
	m_hAerialPerspectivePS[0] = GetPixelShader( "maps/aerialperspectiveps", g_pRenderDevice, pPixelShaderVersion, "#define SHADER_VARIATION 0\n" );
	m_hAerialPerspectivePS[1] = GetPixelShader( "maps/aerialperspectiveps", g_pRenderDevice, pPixelShaderVersion, "#define SHADER_VARIATION 1\n" );
	m_hAerialPerspectivePSCB = g_pRenderDevice->CreateConstantBuffer( sizeof( AerialPerspectiveConstants_t ) );

	// states for aerial perspective
	RsBlendStateDesc_t bd;
	memset( &bd, 0, sizeof( bd ) );
	bd.m_bAlphaToCoverageEnable = false;
	bd.m_bIndependentBlendEnable = false;
	for ( int i=0; i<4; ++i )
	{
		bd.m_bBlendEnable[i] = true;
		bd.m_srcBlend[i] = RS_BLEND_MODE_ZERO;
		bd.m_destBlend[i] = RS_BLEND_MODE_SRC_COLOR;
		bd.m_blendOp[i] = RS_BLEND_OP_ADD;
		bd.m_nRenderTargetWriteMask[i] = RS_COLOR_WRITE_ENABLE_R | RS_COLOR_WRITE_ENABLE_G | RS_COLOR_WRITE_ENABLE_B;
	}
	m_hExtinctionBS = pRenderContext->FindOrCreateBlendState( &bd );
	for ( int i=0; i<4; ++i )
	{
		bd.m_srcBlend[i] = RS_BLEND_MODE_ONE;
		bd.m_destBlend[i] = RS_BLEND_MODE_ONE;
	}
	m_hInscatteringBS = pRenderContext->FindOrCreateBlendState( &bd );

	m_hAdditiveBS = pRenderContext->FindOrCreateBlendState( &bd );

	for ( int i=0; i<4; ++i )
	{
		bd.m_bBlendEnable[i] = false;
		bd.m_srcBlend[i] = RS_BLEND_MODE_ZERO;
		bd.m_destBlend[i] = RS_BLEND_MODE_SRC_COLOR;
		bd.m_blendOp[i] = RS_BLEND_OP_ADD;
		bd.m_nRenderTargetWriteMask[i] = RS_COLOR_WRITE_ENABLE_A;
	}
	m_hWriteAlphaBS = pRenderContext->FindOrCreateBlendState( &bd );

	for ( int i=0; i<4; ++i )
	{
		bd.m_bBlendEnable[i] = false;
		bd.m_srcBlend[i] = RS_BLEND_MODE_ZERO;
		bd.m_destBlend[i] = RS_BLEND_MODE_SRC_COLOR;
		bd.m_blendOp[i] = RS_BLEND_OP_ADD;
		bd.m_nRenderTargetWriteMask[i] = RS_COLOR_WRITE_ENABLE_ALL;
	}
	m_hWriteAllBS = pRenderContext->FindOrCreateBlendState( &bd );

	RsDepthStencilStateDesc_t dsDesc;
	dsDesc.m_bDepthTestEnable = true;
	dsDesc.m_bDepthWriteEnable = false;
	dsDesc.m_depthFunc = RS_CMP_LESS_EQUAL;
	dsDesc.m_hiZEnable360 = RS_HI_Z_AUTOMATIC;
	dsDesc.m_hiZWriteEnable360 = RS_HI_Z_AUTOMATIC;

	dsDesc.m_bStencilEnable = true;
	dsDesc.m_nStencilReadMask = 0xFF;
	dsDesc.m_nStencilWriteMask = 0xFF;

	dsDesc.m_frontStencilFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_frontStencilDepthFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_frontStencilPassOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_frontStencilFunc = RS_CMP_EQUAL;

	dsDesc.m_backStencilFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_backStencilDepthFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_backStencilPassOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_backStencilFunc = RS_CMP_EQUAL;

	dsDesc.m_bHiStencilEnable360 = false;
	dsDesc.m_bHiStencilWriteEnable360 = false;
	dsDesc.m_hiStencilFunc360 = RS_HI_STENCIL_CMP_EQUAL;
	dsDesc.m_nHiStencilRef360 = 0;

	m_hZTestAndStencilTestDS = pRenderContext->FindOrCreateDepthStencilState( &dsDesc );

	dsDesc.m_bDepthTestEnable = false;
	dsDesc.m_bDepthWriteEnable = false;
	m_hStencilTestDS = pRenderContext->FindOrCreateDepthStencilState( &dsDesc );

	dsDesc.m_bDepthTestEnable = true;
	dsDesc.m_bDepthWriteEnable = true;

	dsDesc.m_frontStencilFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_frontStencilDepthFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_frontStencilPassOp = RS_STENCIL_OP_REPLACE;
	dsDesc.m_frontStencilFunc = RS_CMP_ALWAYS;

	dsDesc.m_backStencilFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_backStencilDepthFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_backStencilPassOp = RS_STENCIL_OP_REPLACE;
	dsDesc.m_backStencilFunc = RS_CMP_ALWAYS;

	m_hSetStencilDS = pRenderContext->FindOrCreateDepthStencilState( &dsDesc );

	//
	dsDesc.m_bDepthTestEnable = true;
	dsDesc.m_bDepthWriteEnable = true;
	dsDesc.m_depthFunc = RS_CMP_ALWAYS;
	dsDesc.m_hiZEnable360 = RS_HI_Z_AUTOMATIC;
	dsDesc.m_hiZWriteEnable360 = RS_HI_Z_AUTOMATIC;

	dsDesc.m_bStencilEnable = false;
	dsDesc.m_nStencilReadMask = 0xFF;
	dsDesc.m_nStencilWriteMask = 0xFF;

	dsDesc.m_frontStencilFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_frontStencilDepthFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_frontStencilPassOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_frontStencilFunc = RS_CMP_EQUAL;

	dsDesc.m_backStencilFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_backStencilDepthFailOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_backStencilPassOp = RS_STENCIL_OP_KEEP;
	dsDesc.m_backStencilFunc = RS_CMP_EQUAL;

	dsDesc.m_bHiStencilEnable360 = false;
	dsDesc.m_bHiStencilWriteEnable360 = false;

	m_hZWriteNoTestDS = pRenderContext->FindOrCreateDepthStencilState( &dsDesc );

	m_bDeferredInit = true;
}

void CWorldRendererTest::InitParticleData()
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );

	struct ParticleVertex_t
	{
		Vector	m_vPos;
	};

	static RenderInputLayoutField_t pointLightLayout[] =
	{
		DEFINE_PER_VERTEX_FIELD( 0, "position", 0, ParticleVertex_t, m_vPos )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 0, PerParticleData_t, m_vPosRadius )
		DEFINE_PER_INSTANCE_FIELD( 1, 1, "texcoord", 1, PerParticleData_t, m_vColor )
	};
	m_hPointLightLayout = g_pRenderDevice->CreateInputLayout( "pointlight", ARRAYSIZE( pointLightLayout ), pointLightLayout );

	const char *pVertexShaderVersion = g_pRenderDevice->GetShaderVersionString( RENDER_VERTEX_SHADER);
	const char *pPixelShaderVersion = g_pRenderDevice->GetShaderVersionString( RENDER_PIXEL_SHADER );
	for ( int i=0; i<5; ++i )
	{
		char pBuffer[100];
		Q_snprintf( pBuffer, 100, "#define SHADER_VARIATION %d\n", i );
		m_hPointLightVS[i] = GetVertexShader( "maps/pointlightvs", g_pRenderDevice, pVertexShaderVersion, pBuffer );
		m_hPointLightPS[i] = GetPixelShader( "maps/pointlightps", g_pRenderDevice, pPixelShaderVersion, pBuffer );
	}
}

void CWorldRendererTest::InitRenderableData()
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );

	// Load the simple shaders for dynamic renderables
	const char *pVertexShaderVersion = g_pRenderDevice->GetShaderVersionString( RENDER_VERTEX_SHADER);
	const char *pPixelShaderVersion = g_pRenderDevice->GetShaderVersionString( RENDER_PIXEL_SHADER );
	for ( int i=0; i<4; ++i )
	{
		char pBuffer[100];
		Q_snprintf( pBuffer, 100, "#define SHADER_VARIATION %d\n#define g_flPhongExp 10\n#define g_flPhongBoost 1.5\n", i );
		m_hSimpleMeshVS[i] = GetVertexShader( "maps/meshsimplevs", g_pRenderDevice, pVertexShaderVersion, pBuffer );
		m_hSimpleMeshPS[i] = GetPixelShader( "maps/meshsimpleps", g_pRenderDevice, pPixelShaderVersion, pBuffer );
	}

	// Load the minigun
	HRenderable hRenderable = g_pMeshSystem->FindOrCreateFileRenderable( g_pRenderableList[ 0 ] );
	int index = m_dynRenderableList.AddToTail();

	m_dynRenderableList[ index ].m_hRenderable = hRenderable;
	m_dynRenderableList[ index ].m_vOrigin.Init( 0, 0, 0 );
	m_dynRenderableList[ index ].m_mWorld.Identity();
}

void CWorldRendererTest::UpdateFrustum( int nWidth, int nHeight )
{
	// Build up a frustum
	if ( nWidth == m_nLastFrustumWidth && nHeight == m_nLastFrustumHeight )
		return;

	m_nLastFrustumWidth = nWidth;
	m_nLastFrustumHeight = nHeight;

	float flAspect = nWidth / (float)nHeight;
	m_pFrustum->SetAspect( flAspect );
	m_pFrustum->SetFOV( CalcFovX(90.0f, flAspect) );
	m_pFrustum->UpdateFrustumFromCamera();
}

void CWorldRendererTest::RenderScreenQuad( IRenderContext *pRenderContext, Vector *pEyeRays, const char *pDebugName )
{
	CDynamicVertexData<QuadVertex_t> vb( pRenderContext, 4, pDebugName, "screenquad" );
	vb.Lock( );
	vb->m_vPos = Vector( -1, -1, 0.5f );
	vb->m_vEyeRay = pEyeRays[0];
	vb.AdvanceVertex();

	vb->m_vPos = Vector( 1, -1, 0.5f );
	vb->m_vEyeRay = pEyeRays[1];
	vb.AdvanceVertex();

	vb->m_vPos = Vector( -1, 1, 0.5f );
	vb->m_vEyeRay = pEyeRays[2];
	vb.AdvanceVertex();

	vb->m_vPos = Vector( 1, 1, 0.5f );
	vb->m_vEyeRay = pEyeRays[3];
	vb.AdvanceVertex();
	
	vb.Unlock();
	vb.Bind( 0, 0 );

	pRenderContext->Draw( RENDER_PRIM_TRIANGLE_STRIP, 0, 4 );
}

void CWorldRendererTest::RenderFrame( const RenderViewport_t &viewport, PlatWindow_t hWnd )
{
	if ( !m_bRenderTargetsInit )
	{
		InitRenderTargetsAndViews( viewport );
	}

	if ( !m_bDeferredInit )
	{
		InitDeferredData( viewport );
	}

	if ( m_nReportLevel == 1 )
	{
		g_VProfCurrentProfile.Reset();
		g_VProfCurrentProfile.ResetPeaks();
		g_VProfCurrentProfile.Start();
		m_nReportLevel++;
		Msg("VPROF Trace Begin\n");
	}

	RenderFrame_Internal( viewport );

	if ( m_nReportLevel == 3 )
	{
		g_VProfCurrentProfile.Stop();
		g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, NULL );
		Msg("VPROF Trace Complete\n");
		m_nReportLevel = 0;
	}
	else if ( m_nReportLevel )
	{
		g_VProfCurrentProfile.MarkFrame();
	}
}

void CWorldRendererTest::CullRenderablesAgainstView( CRenderView &renderView, CUtlVector<CStaticMeshRenderable> &renderableList, CUtlVector<CDynamicRenderable> &dynList )
{
	if ( !renderView.IsValid() )
		return;

	RenderViewFlags_t nFlags = renderView.GetViewFlags();
	if ( !( nFlags & ( VIEW_PLAYER_CAMERA | VIEW_LIGHT_SHADOW | VIEW_REFLECTION ) ) )
		return;

	if ( nFlags & ( VIEW_POST_PROCESS | VIEW_LIGHTING | VIEW_DECALS ) )
		return;

	CWorldRenderFrustum *pFrustum = renderView.GetFrustum();
	if( !pFrustum )
		return;

	if ( m_bDropFrustum && pFrustum == m_pFrustum )
		pFrustum = m_pDropFrustum;

	// Static
	int nRenderables = renderableList.Count();
	for ( int r=0; r<nRenderables; ++r )
	{
		CStaticMeshRenderable &renderable = renderableList[ r ];
		
		IBVHNode *pNode = renderable.m_pNode;
		Vector vOriginShift = pNode->GetOrigin().m_vLocal;
		CBVHDrawCall *pDraw = renderable.m_pDrawCall;

		if ( pDraw->m_Flags & DRAW_CULL )
		{
			if ( !pFrustum->BoundingVolumeIntersectsFrustum( pDraw->m_Bounds, vOriginShift ) )
			{
				continue;
			}
		}

		// Add a renderable
		renderView.AddStaticMeshRenderable( &renderable );
	}

	// Dynamic
	nRenderables = dynList.Count();
	for ( int r=0; r<nRenderables; ++r )
	{
		CDynamicRenderable &renderable = dynList[ r ];
		
		if ( !g_pMeshSystem->RenderableIntersectsFrustum( renderable.m_hRenderable, pFrustum, -renderable.m_vOrigin ) )
			continue;

		// Add a renderable
		renderView.AddDynamicRenderable( &renderable );
	}
}

void CWorldRendererTest::SetupMultiShadowMatrices( int nViewportStart, int nSqrtNumShadows, float flBias )
{
	int nGutterSize = 4;
	if ( nViewportStart > 0 )
		nGutterSize = 2;

	float flScale = ( 0.5f / nSqrtNumShadows ) - ( nGutterSize / (float)m_nShadowSize );
	int nMultiShadowSize = m_nShadowSize / nSqrtNumShadows;
	int nTop = 0;
	int nViewport = nViewportStart;

	float frange   = 1.0f;
	if ( IsPlatformX360() )
	{
		frange   = -1.0f;
		flBias    = flBias * frange;
	}

	for ( int t=0; t<nSqrtNumShadows; ++t )
	{
		int nLeft = 0;
		for( int s=0; s<nSqrtNumShadows; ++s )
		{
			int nGutteredLeft = nLeft + nGutterSize;
			int nGutteredTop = nTop + nGutterSize;
			int nGutteredMultiShadowedSize = nMultiShadowSize - nGutterSize * 2;
			m_multiShadowViewport[ nViewport ].Init( nGutteredLeft, nGutteredTop, nGutteredMultiShadowedSize, nGutteredMultiShadowedSize, 0, 1.0f );

			float fBiasVal = flBias;
			if ( IsPlatformX360() )
			{
				fBiasVal = 1.0f - flBias;
			}

			float flShiftX = flScale + ( nGutteredLeft + 0.5f ) / m_nShadowSize;
			float flShiftY = flScale + ( nGutteredTop + 0.5f ) / m_nShadowSize;

			VMatrix texScaleBiasMat( flScale,	0.0f,		0.0f,		0.0f,
									 0.0f,		-flScale,	0.0f,		0.0f,
									 0.0f,		0.0f,		frange,		0.0f,
									 flShiftX,	flShiftY,	fBiasVal,	1.0f );

			m_multiShadowScaleBiasMatrices[ nViewport ] = texScaleBiasMat;

			nViewport++;
			nLeft += nMultiShadowSize;
		}
		nTop += nMultiShadowSize;
	}
}

void CWorldRendererTest::SetupShadowMatrices()
{
	// Multiple shadow viewports
	float fOffsetX = 0.5f + (0.5f / (float)m_nShadowSize);
	float fOffsetY = 0.5f + (0.5f / (float)m_nShadowSize);
	float frange   = 1.0f;
	float fBias    = -0.001f * frange;
	if ( IsPlatformX360() )
	{
		frange   = -1.0f;
		fBias    = 0.005f * frange;
	}

	for ( int s=0; s<NUM_SHADOW_SPLITS; ++s )
	{		
		//set special texture matrix for shadow mapping
		float fBiasVal = fBias;
		if ( IsPlatformX360() )
		{
			fBiasVal = 1.0f - fBias;
		}

		VMatrix texScaleBiasMat( 0.5f,     0.0f,     0.0f,      0.0f,
								 0.0f,    -0.5f,     0.0f,      0.0f,
								 0.0f,     0.0f,     frange,	0.0f,
								 fOffsetX, fOffsetY, fBiasVal,	1.0f );
		m_splitScaleBiasMatrices[ s ] = texScaleBiasMat;

		fBias *= 1.5f;
	}

	SetupMultiShadowMatrices( 0, SQRT_NUM_HIGH_RES_SHADOWS, -0.001f );
	if ( SQRT_NUM_LOW_RES_SHADOWS )
	{
		SetupMultiShadowMatrices( NUM_HIGH_RES_SHADOWS, SQRT_NUM_LOW_RES_SHADOWS, -0.003f );
	}
}

int SortSpotLights( const CSpotLightRenderable *pOne, const CSpotLightRenderable *pTwo )
{
	if ( pOne->m_flSquaredDistToEye < pTwo->m_flSquaredDistToEye )
		return -1;
	else if ( pOne->m_flSquaredDistToEye > pTwo->m_flSquaredDistToEye )
		return 1;
	return 0;
}

void CWorldRendererTest::GenerateRenderables( Vector &vCameraPos )
{
	// Figure out where we are and if we're interior or not
	int nLeafNode = m_pWorldRenderer->GetLeafNodeForPoint( vCameraPos );
	bool bCanSeeSky = true;
	if ( nLeafNode != -1 )
	{
		IBVHNode *pNode = m_pWorldRenderer->GetNode( nLeafNode );
		int nFlags = pNode->GetFlags();
		bCanSeeSky = ( nFlags & NODE_CAN_SEE_SKY ) != 0;
	}

	// Clear renderables
	m_renderList.SetCountNonDestructively( 0 );
	m_renderableList.SetCountNonDestructively( 0 );
	m_interiorPointLights.SetCountNonDestructively( 0 );
	m_exteriorPointLights.SetCountNonDestructively( 0 );
	m_interiorSpotLights.SetCountNonDestructively( 0 );
	m_exteriorSpotLights.SetCountNonDestructively( 0 );
	m_interiorHemiLights.SetCountNonDestructively( 0 );
	m_exteriorHemiLights.SetCountNonDestructively( 0 );
	m_shadowedSpotLights.SetCountNonDestructively( 0 );

	// Clear far plane
	m_flMaxLightEncompassingFarPlane = 0.0f;

	// Build the render list for this view
	BVHNodeFlags_t nSkipFlags = (BVHNodeFlags_t)0x0;
	if ( bCanSeeSky == false )
		nSkipFlags = NODE_CAN_SEE_SKY;

	float flLODScale =  1.0f;
	if ( IsPlatformX360() )
	{
		flLODScale = 0.5f;
	}
	float flElapsedTime = 0.0f;
	m_pWorldRenderer->BuildRenderList( &m_renderList, nSkipFlags, vCameraPos, flLODScale, m_pFrustum->GetFarPlane(), flElapsedTime, m_nCurrentFrameNumber );

	// Distance sort render traversals
	m_pWorldRenderer->SortRenderList( &m_renderList, vCameraPos );

	//Create node requests and dispatch them to the async filesystem
	// This can cause eviction of resources.
	m_pWorldRenderer->CreateAndDispatchLoadRequests( g_pRenderDevice, m_pFrustum->GetCameraPosition() );

	// Create renderables from each draw call encountered
	int nNodes = m_renderList.Count();
	for ( int n=0; n<nNodes; ++n )
	{
		IBVHNode *pNode = m_renderList[ n ];
		Vector vOriginShift = pNode->GetOrigin().m_vLocal;

		// Gross cull of the bounds before turning them into renderables
		AABB_t bounds = pNode->GetBounds();
		if ( !m_pFrustum->BoundingVolumeIntersectsFrustum( bounds, vOriginShift ) )
			continue;

		// Draw calls
		int nDraws = pNode->GetNumDrawCalls();
		for ( int d=0; d<nDraws; ++d )
		{
			CBVHDrawCall &draw = pNode->GetDrawCall( d );

			CStaticMeshRenderable renderable;
			renderable.m_pNode = pNode;
			renderable.m_pDrawCall = &draw;
			m_renderableList.AddToTail( renderable );	
		}

		// Point Lights
		GeneratePointLightRenderables( pNode, vOriginShift, m_pFrustum, vCameraPos );

		// Spot lights
		GenerateSpotLightRenderables( pNode, vOriginShift, m_pFrustum, vCameraPos );

		// Hemi lights
		GenerateHemiLightRenderables( pNode, vOriginShift, m_pFrustum, vCameraPos );
	}

	if ( m_bFlashlight )
	{
		CSpotLightRenderable renderable;
		renderable.m_vOrigin.Init( 0, 0, 0 );
		renderable.m_pLight = &m_flashlight;
		renderable.m_bValid = true;
		renderable.m_flSquaredDistToEye = 0.0f;

		// We're outside the light
		renderable.m_bInterior = false;
		m_shadowedSpotLights.AddToTail( renderable );
	}

	// We've been storing squared distance, but we want linear distance
	m_flMaxLightEncompassingFarPlane = sqrtf( m_flMaxLightEncompassingFarPlane );

	// Sort spot lights and set the NUM_MULTI_SHADOWS closest as shadow casting
	m_shadowedSpotLights.Sort( SortSpotLights );
	
	/*
	// Artificially limit shadows past a certain point
	int nShadows = 0;
	for ( int s=0; s<m_shadowedSpotLights.Count(); ++s )
	{
		if ( m_shadowedSpotLights[ s ].m_flSquaredDistToEye > MAX_LIGHT_DISTANCE * MAX_LIGHT_DISTANCE )
		{
			break;
		}

		nShadows++;
	}*/
	int nShadows = m_shadowedSpotLights.Count();

	nShadows = MIN( nShadows, NUM_MULTI_SHADOWS );
	m_shadowedSpotLights.SetCountNonDestructively( nShadows );

	for ( int s=0; s<nShadows; ++s )
	{
		int index = m_exteriorSpotLights.Find( m_shadowedSpotLights[s] );
		if ( index != -1 )
		{
			m_exteriorSpotLights.FastRemove( index );
		}

		index = m_interiorSpotLights.Find( m_shadowedSpotLights[s] );
		if ( index != -1 )
		{
			m_interiorSpotLights.FastRemove( index );
		}
	}
}

void CWorldRendererTest::GeneratePointLightRenderables( IBVHNode *pNode, Vector &vOrigin, CFrustum *pFrustum, Vector &vEye )
{
	float flNearPlane = pFrustum->GetNearPlane();
	int nLights = pNode->GetNumPointLights();
	PointLightData_t *pPointLights = pNode->GetPointLights();

	for ( int i=0; i<nLights; ++i )
	{
		float flRadius = pPointLights[i].m_vColorNRadius.w;

		AABB_t bounds;
		Vector vLightOrigin = pPointLights[i].m_vOrigin;
		bounds.m_vMinBounds = vLightOrigin - Vector( flRadius, flRadius, flRadius );
		bounds.m_vMaxBounds = vLightOrigin + Vector( flRadius, flRadius, flRadius );

		if ( pFrustum->BoundingVolumeIntersectsFrustum( bounds, vOrigin ) )
		{
			CPointLightRenderable renderable;
			renderable.m_vOrigin = vOrigin;
			renderable.m_pLight = &pPointLights[i];

			Vector vNewOrigin = vLightOrigin - vOrigin;
			Vector vDelta = vEye - vNewOrigin;
			float flSquaredDist = DotProduct( vDelta, vDelta );
			float flTargetRad = flRadius * 1.2f + flNearPlane;
			if ( DotProduct( vDelta, vDelta ) < flTargetRad * flTargetRad )
			{
				// We're inside the light
				m_interiorPointLights.AddToTail( renderable );
			}
			else
			{
				// We're outside the light
				m_exteriorPointLights.AddToTail( renderable );
			}

			m_flMaxLightEncompassingFarPlane = MAX( m_flMaxLightEncompassingFarPlane, flSquaredDist + flTargetRad * flTargetRad );
		}
	}
}

void CWorldRendererTest::GenerateSpotLightRenderables( IBVHNode *pNode, Vector &vOrigin, CFrustum *pFrustum, Vector &vEye )
{
	float flNearPlane = pFrustum->GetNearPlane();
	Vector vForward = pFrustum->Forward();
	Vector vFarEye = vEye + flNearPlane * vForward * 1.2f;
	int nLights = pNode->GetNumSpotLights();
	SpotLightData_t *pSpotLights = pNode->GetSpotLights();

	for ( int i=0; i<nLights; ++i )
	{
		float flRadius = pSpotLights[i].m_vColorNRadius.w;
		float flTargetRad = flRadius * 1.2f + 500.0f + flNearPlane;
		Vector vLightOrigin = Vector( pSpotLights[i].m_vTransform0.w, pSpotLights[i].m_vTransform1.w, pSpotLights[i].m_vTransform2.w );

		AABB_t bounds;
		bounds.m_vMinBounds = vLightOrigin - Vector( flTargetRad, flTargetRad, flTargetRad );
		bounds.m_vMaxBounds = vLightOrigin + Vector( flTargetRad, flTargetRad, flTargetRad );

		if ( pFrustum->BoundingVolumeIntersectsFrustum( bounds, vOrigin ) )
		{
			CSpotLightRenderable renderable;
			renderable.m_vOrigin = vOrigin;
			renderable.m_pLight = &pSpotLights[i];
			renderable.m_bValid = true;

			Vector vNewOrigin = vLightOrigin - vOrigin;
			Vector vDelta = vEye - vNewOrigin;
			Vector vLightDir = Vector( pSpotLights[i].m_vTransform0.z, pSpotLights[i].m_vTransform1.z, pSpotLights[i].m_vTransform2.z );

			float flSquaredDist = DotProduct( vDelta, vDelta );
			renderable.m_flSquaredDistToEye = flSquaredDist;

			//if ( flRadius > 3000.0f )
			//	renderable.m_flSquaredDistToEye = FLT_MAX;

			Vector vToLightFar = vFarEye - vNewOrigin;
			Vector vToLightNear = vEye - vNewOrigin;
			vToLightFar.NormalizeInPlace();
			vToLightNear.NormalizeInPlace();
			float flDotFar = DotProduct( vToLightFar, vLightDir );
			float flDotNear = DotProduct( vToLightNear, vLightDir );
			float flCosAngle = pSpotLights[i].m_vAttenuationNCosSpot.w * 0.85f;

			bool bInCone = ( flDotFar > flCosAngle || flDotNear > flCosAngle );
			if ( flSquaredDist < flTargetRad * flTargetRad && bInCone )
			{
				// We're inside the light
				renderable.m_bInterior = true;
				m_interiorSpotLights.AddToTail( renderable );
			}
			else
			{
				// We're outside the light
				renderable.m_bInterior = false;
				m_exteriorSpotLights.AddToTail( renderable );
			}

			m_shadowedSpotLights.AddToTail( renderable );

			m_flMaxLightEncompassingFarPlane = MAX( m_flMaxLightEncompassingFarPlane, flSquaredDist + flTargetRad * flTargetRad + 4000 * 4000 );
		}
	}
}

void CWorldRendererTest::GenerateHemiLightRenderables( IBVHNode *pNode, Vector &vOrigin, CFrustum *pFrustum, Vector &vEye )
{
	float flNearPlane = pFrustum->GetNearPlane();
	Vector vForward = pFrustum->Forward();
	Vector vFarEye = vEye + flNearPlane * vForward * 1.1f;
	int nLights = pNode->GetNumHemiLights();
	HemiLightData_t *pHemiLights = pNode->GetHemiLights();

	for ( int i=0; i<nLights; ++i )
	{
		float flRadius = pHemiLights[i].m_vColorNRadius.w;
		Vector vLightOrigin = Vector( pHemiLights[i].m_vTransform0.w, pHemiLights[i].m_vTransform1.w, pHemiLights[i].m_vTransform2.w );
		AABB_t bounds;
		bounds.m_vMinBounds = vLightOrigin - Vector( flRadius, flRadius, flRadius );
		bounds.m_vMaxBounds = vLightOrigin + Vector( flRadius, flRadius, flRadius );

		if ( pFrustum->BoundingVolumeIntersectsFrustum( bounds, vOrigin ) )
		{
			CHemiLightRenderable renderable;
			renderable.m_vOrigin = vOrigin;
			renderable.m_pLight = &pHemiLights[i];

			Vector vNewOrigin = vLightOrigin - vOrigin;
			Vector vDelta = vEye - vNewOrigin;
			float flSquaredDist = DotProduct( vDelta, vDelta );
			Vector vLightDir = Vector( pHemiLights[i].m_vTransform0.z, pHemiLights[i].m_vTransform1.z, pHemiLights[i].m_vTransform2.z );
			float flTargetRad = flRadius * 1.2f + flNearPlane;

			Vector vToLightFar = vFarEye - vNewOrigin;
			Vector vToLightNear = vEye - vNewOrigin;
			vToLightFar.NormalizeInPlace();
			vToLightNear.NormalizeInPlace();
			float flDotFar = DotProduct( vToLightFar, vLightDir );
			float flDotNear = DotProduct( vToLightNear, vLightDir );
			float flCosAngle = 0.0f;

			bool bInHemi = ( flDotFar > flCosAngle || flDotNear > flCosAngle );
			if ( DotProduct( vDelta, vDelta ) < flTargetRad * flTargetRad && bInHemi )
			{
				// We're inside the light
				m_interiorHemiLights.AddToTail( renderable );
			}
			else
			{
				// We're outside the light
				m_exteriorHemiLights.AddToTail( renderable );
			}

			m_flMaxLightEncompassingFarPlane = MAX( m_flMaxLightEncompassingFarPlane, flSquaredDist + flTargetRad * flTargetRad );
		}
	}
}

void CWorldRendererTest::RenderViewStatic( CRenderView *pRenderView, int nStart, int nCount, int nIndex )
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );
	pRenderView->BeginRender( pRenderContext );

	SetStateForRenderViewType( pRenderContext, pRenderView->GetViewType() );

	int nRenderPass = pRenderView->GetRenderPass();
	CWorldRenderFrustum *pFrustum = pRenderView->GetFrustum();
	CUtlVector< CStaticMeshRenderable* > &renderableList = pRenderView->GetRenderableList();

	// Set commonly used constants
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( pFrustum );

	if ( nRenderPass == VARIATION_SAMPLE_LIGHTING )
	{
		pRenderContext->SetConstantBufferData( m_hLightBufferData, (void*)&m_lightBufferData, sizeof( LightBufferData_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hLightBufferData, 0, 0 );
	}

	if ( m_bViewLowTexture && nRenderPass == VARIATION_SAMPLE_LIGHTING )
	{
		nRenderPass = VARIATION_SAMPLE_LIGHTING_LOW_TEXTURE;
	}

	// Render the nodes
	Vector vLastOrigin = Vector(0,0,0);
	bool bFirst = true;
	bool bSunDepth = false;
	if ( nRenderPass == VARIATION_DEPTH && pRenderView->GetViewType() < RTB_SHADOW_DEPTH_MULTIPLE_0 )
		bSunDepth = true;
	bool bSetStencil = false;
	if ( nRenderPass == VARIATION_NORM_DEPTH_SPEC )
		bSetStencil = true;

	for ( int r=0; r<nCount; ++r )
	{
		CStaticMeshRenderable *pRenderable = renderableList[ nStart + r ];

		IBVHNode *pNode = pRenderable->m_pNode;
		AABB_t bounds = pNode->GetBounds();
		Vector vOrigin = pNode->GetOrigin().m_vLocal;
		int nFlags = pNode->GetFlags();
		bool bCanSeeSky = ( nFlags & NODE_CAN_SEE_SKY ) != 0;

		// Don't render depth for sun shadow maps if we can't see the sky
		if ( bSunDepth && !bCanSeeSky )
			continue;

		if ( ( vLastOrigin != vOrigin ) || bFirst )
		{
			VMatrix mViewRelated = viewConstants.m_mViewProjection;

			VMatrix mTrans;
			mTrans.Identity();
			mTrans.SetTranslation( -vOrigin );
			mViewRelated = mTrans.Transpose() * mViewRelated;

			ViewRelatedConstants_t newViewConstants = viewConstants;
			newViewConstants.m_mViewProjection = mViewRelated;
			newViewConstants.m_vEyePt.x += vOrigin.x;
			newViewConstants.m_vEyePt.y += vOrigin.y;
			newViewConstants.m_vEyePt.z += vOrigin.z;

			pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&newViewConstants, sizeof( ViewRelatedConstants_t ) );
			pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );
			
			vLastOrigin = vOrigin;
			bFirst = false;
		}

		if ( bSetStencil )
		{
			if ( bCanSeeSky )
			{
				pRenderContext->SetDepthStencilState( m_hSetStencilDS, g_nCanSeeSkyStencilRef );
			}
			else
			{
				pRenderContext->SetDepthStencilState( m_hSetStencilDS, g_nCannotSeeSkyStencilRef );
			}
		}

		pNode->Draw( pRenderContext, pRenderable->m_pDrawCall, m_pResourceDictionary, (ShaderComboVariation_t)nRenderPass, m_hSingleMatrixRelated );
	}

	pRenderView->EndRender( pRenderContext, false );
}


void CWorldRendererTest::RenderViewDynamic( CRenderView *pRenderView, int nStart, int nCount, int nIndex )
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );
	pRenderView->BeginRender( pRenderContext );

	// Get view constants
	CWorldRenderFrustum *pFrustum = pRenderView->GetFrustum();
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( pFrustum );

	// Init material data
	RenderMaterialData_t &materialData = pRenderContext->GetMaterialData();
	materialData.Init();
	materialData.flTime = Plat_FloatTime();
	materialData.mVP = viewConstants.m_mViewProjection;
	materialData.mWVP = viewConstants.m_mViewProjection;
	materialData.vCameraPosition = viewConstants.m_vEyePt.AsVector3D();
	materialData.vCameraForwardVec = viewConstants.m_vEyeDir.AsVector3D().Normalized();
	materialData.flFarPlane = viewConstants.m_flFarPlane.x;
	materialData.vViewportSize[0] = 1.0f / m_lightBufferData.m_vInvScreenExtents.x;
	materialData.vViewportSize[1] = 1.0f / m_lightBufferData.m_vInvScreenExtents.y;

	materialData.customTextures[0] = m_pRenderTargets[ RT_LIGHTING_HIGHRES ];
	materialData.customTextures[1] = m_pRenderTargets[ RT_SPECULAR_HIGHRES ];

	int nRenderPass = pRenderView->GetRenderPass();
	if ( nRenderPass == VARIATION_DEPTH )
	{
		materialData.nMode = MODE_DEPTH;
	}
	else if ( nRenderPass == VARIATION_NORM_DEPTH_SPEC )
	{
		materialData.nMode = MODE_NORMAL_DEPTH_SPEC;
	}
	else if ( nRenderPass == VARIATION_SAMPLE_LIGHTING )
	{
		materialData.nMode = MODE_DEFERRED_GATHER;
	}
	else
	{
		materialData.nMode = MODE_FORWARD;
	}

	// Sun and stencil bools
	bool bSunDepth = false;
	if ( ( nRenderPass == VARIATION_DEPTH ) && ( pRenderView->GetViewType() < RTB_SHADOW_DEPTH_MULTIPLE_0 ) )
	{
		bSunDepth = true;
	}

	bool bSetStencil = false;
	if ( nRenderPass == VARIATION_NORM_DEPTH_SPEC )
	{
		bSetStencil = true;
	}

	// Render the nodes
	CUtlVector< CDynamicRenderable * > &renderableList = pRenderView->GetDynamicList();
	for ( int r = 0; r < nCount; r++ )
	{
		CDynamicRenderable *pRenderable = renderableList[ nStart + r ];

		// Copy world matrix into material data
		memcpy( materialData.mWorldArray[0], &( pRenderable->m_mWorld.m[0] ), sizeof( pRenderable->m_mWorld ) );

		RenderDynamic( pRenderContext, pRenderable, ( ShaderComboVariation_t )nRenderPass, bSunDepth, bSetStencil );
	}

	pRenderView->EndRender( pRenderContext, false );
}

void CWorldRendererTest::RenderView( CRenderView &renderView )
{
	if ( m_bMultiThreaded )
	{
		CUtlVector< CStaticMeshRenderable* > &renderableList = renderView.GetRenderableList();
		CUtlVector< CDynamicRenderable* > &dynList = renderView.GetDynamicList();
		int nRenderablesStatic = renderableList.Count();
		int nRenderablesDynamic = dynList.Count();
		int nCountStatic = nRenderablesStatic / m_nRenderThreads;
		int nCountDynamic = nRenderablesDynamic / m_nRenderThreads;
		int nTotalRenderablesStatic = 0;
		int nTotalRenderablesDynamic = 0;
		for ( int t=0; t<m_nRenderThreads; ++t )
		{
			m_pRenderJobs[ t ].m_pRenderView = &renderView;
			m_pRenderJobs[ t ].m_nStartStatic = nTotalRenderablesStatic;
			m_pRenderJobs[ t ].m_nCountStatic = nCountStatic;
			m_pRenderJobs[ t ].m_nStartDynamic = nTotalRenderablesDynamic;
			m_pRenderJobs[ t ].m_nCountDynamic = nCountDynamic;

			m_pRenderJobs[ t ].m_pTestApp = this;
			m_pRenderJobs[ t ].m_nIndex = t;
			nTotalRenderablesStatic += nCountStatic;
			nTotalRenderablesDynamic += nCountDynamic;
		}
		m_pRenderJobs[ m_nRenderThreads - 1 ].m_nCountStatic = nRenderablesStatic - m_pRenderJobs[ m_nRenderThreads - 1 ].m_nStartStatic;
		m_pRenderJobs[ m_nRenderThreads - 1 ].m_nCountDynamic = nRenderablesDynamic - m_pRenderJobs[ m_nRenderThreads - 1 ].m_nStartDynamic;

		ParallelProcess( m_pRenderJobs, m_nRenderThreads, PerformStaticMeshWorkUnit );
	}
	else
	{
		RenderViewStatic( &renderView, 0, renderView.GetRenderableList().Count(), 0 );
		RenderViewDynamic( &renderView, 0, renderView.GetDynamicList().Count(), 0 );
	}
}

void CWorldRendererTest::UpdateShadowData()
{
	for ( int s=0; s<m_nShadowSplits; ++s )
	{
		VMatrix mLightRelated;
		mLightRelated = m_pSplitFrustums[s]->GetViewProj();
		m_pShadowMatrices[s] = mLightRelated * m_splitScaleBiasMatrices[ s ];
		m_forwardLightingData.m_mShadowMatrices[s] = m_pShadowMatrices[s];
	}

	for ( int s=0; s<NUM_MULTI_SHADOWS; ++s )
	{
		m_pRenderViews[ RTB_SHADOW_DEPTH_MULTIPLE_0 + s ].SetValid( false );
	}

	for ( int s=0; s<m_shadowedSpotLights.Count(); ++s )
	{
		CSpotLightRenderable &renderable = m_shadowedSpotLights[s];
		
		Vector vOrigin = Vector( renderable.m_pLight->m_vTransform0.w, renderable.m_pLight->m_vTransform1.w, renderable.m_pLight->m_vTransform2.w );
		vOrigin -= renderable.m_vOrigin;
		Vector vDirection = Vector( renderable.m_pLight->m_vTransform0.z, renderable.m_pLight->m_vTransform1.z, renderable.m_pLight->m_vTransform2.z );
		Vector vUp = Vector( renderable.m_pLight->m_vTransform0.y, renderable.m_pLight->m_vTransform1.y, renderable.m_pLight->m_vTransform2.y );

		vDirection.NormalizeInPlace();
		vUp.NormalizeInPlace();
		QAngle vAngles;
		VectorAngles( vDirection, vUp, vAngles );

		float flRadius = renderable.m_pLight->m_vColorNRadius.w;
		float flFOV = RAD2DEG( acosf( renderable.m_pLight->m_vAttenuationNCosSpot.w ) * 2 );

		m_pMultiShadowFrustums[ s ]->InitCamera( vOrigin, vAngles, 20.0f, flRadius + 500.0f, flFOV, 1.0f );
		m_pMultiShadowFrustums[ s ]->UpdateFrustumFromCamera();

		VMatrix mLightRelated = m_pMultiShadowFrustums[ s ]->GetViewProj();
		m_pMultiShadowMatrices[ s ] = mLightRelated * m_multiShadowScaleBiasMatrices[ s ];

		m_pRenderViews[ RTB_SHADOW_DEPTH_MULTIPLE_0 + s ].SetValid( true );
	}
}

void CWorldRendererTest::UpdateFlashlightData()
{
	float flFlashlightLength = 1000.0f;
	float flSpotAngle = DEG2RAD( 45.0f );
	Vector vColor( 1,1,0.8f );
	vColor *= 100.0f;
	float flMax = 255.0f;

	Vector vAttenuation;
	vAttenuation.x = 0;
	vAttenuation.y = flMax / flFlashlightLength;
	vAttenuation.z = 0;
	float flTan45 = tanf( M_PI / 4.0f );
	float flScale = tanf( flSpotAngle ) / flTan45;

	Vector vOrigin = m_pFrustum->GetCameraPosition();
	Vector vDirection = m_pFrustum->Forward();
	Vector vLeft = m_pFrustum->Left();
	Vector vUp = m_pFrustum->Up();

	vOrigin -= vUp * 15.0f;
	vOrigin += vLeft * 25;

	Vector vRight = -vLeft;
	vRight *= flScale * flFlashlightLength;
	vUp *= flScale * flFlashlightLength;
	vDirection *= flFlashlightLength;

	m_flashlight.m_vTransform0.Init( vRight.x, vUp.x, vDirection.x, vOrigin.x );
	m_flashlight.m_vTransform1.Init( vRight.y, vUp.y, vDirection.y, vOrigin.y );
	m_flashlight.m_vTransform2.Init( vRight.z, vUp.z, vDirection.z, vOrigin.z );
	m_flashlight.m_vColorNRadius.Init( vColor.x, vColor.y, vColor.z, flFlashlightLength );
	m_flashlight.m_vAttenuationNCosSpot.Init( vAttenuation.x, vAttenuation.y, vAttenuation.z, cosf( flSpotAngle ) );
}

void CWorldRendererTest::UpdateViewModelData()
{
	if ( m_dynRenderableList.Count() < 1 )
		return;

	Vector vOrigin = m_pFrustum->GetCameraPosition();
	Vector vDirection = m_pFrustum->Forward();
	Vector vLeft = m_pFrustum->Left();
	Vector vUp = m_pFrustum->Up();

	vOrigin -= vUp * 15.0f;
	vOrigin -= vLeft * 25;
	vOrigin += vDirection * 30;

	VMatrix mWorld;
	mWorld.Identity();
	mWorld.SetForward( vDirection * 0.6f );
	mWorld.SetLeft( vLeft );
	mWorld.SetUp( vUp );

	VMatrix mRot;
	Vector vAxis( 0, 0, 1 );
	MatrixBuildRotationAboutAxis( mRot, vAxis, 90.0f );
	vAxis.Init( 1, 0, 0 );
	MatrixRotate( mRot, vAxis, 90.0f );

	m_dynRenderableList[ 0 ].m_vOrigin = vOrigin;
	m_dynRenderableList[ 0 ].m_mWorld = mWorld * mRot;
	m_dynRenderableList[ 0 ].m_mWorld.SetTranslation( vOrigin );
}

void CWorldRendererTest::UpdateDroppedRenderables( float flElapsedTime )
{
	int nRenderables = m_dynRenderableList.Count();
	for ( int i=1; i<nRenderables; ++i )
	{
		Vector vAxis( 0, 1, 0 );
		MatrixRotate( m_dynRenderableList[ i ].m_mWorld, vAxis, flElapsedTime * 60.0f );
		m_dynRenderableList[ i ].m_mWorld.SetTranslation( m_dynRenderableList[ i ].m_vOrigin );
	}
}

void CWorldRendererTest::RenderShadowDepthBuffers( IRenderContext *pRenderContext )
{
	Vector4D vShadowClear(1,1,1,1);

	VPROF_SCOPE_BEGIN("RenderSplitShadows");
	for ( int s=0; s<m_nShadowSplits; ++s )
	{
		m_pRenderViews[ RTB_SHADOW_DEPTH0 + s ].Clear( pRenderContext, vShadowClear, RENDER_CLEAR_FLAGS_CLEAR_DEPTH );

		if ( m_pRenderViews[ RTB_SHADOW_DEPTH0 + s ].HasRenderables() )
		{
			RenderView( m_pRenderViews[ RTB_SHADOW_DEPTH0 + s ] );

			// Render spheres
			m_pRenderViews[ RTB_SHADOW_DEPTH0 + s ].BeginRender( pRenderContext );
			
			ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pSplitFrustums[s] );
			pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
			pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

			pRenderContext->SetBlendMode( RENDER_BLEND_NOPIXELWRITE );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_AND_WRITE );
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );

			int nParticleSystems = m_sphereSystemList.Count();
			for ( int p=0; p<nParticleSystems; ++p )
			{
				RenderParticleLights( pRenderContext, m_sphereSystemList[p], VARIATION_DEPTH, true, false );
			}

			// Submit
			m_pRenderViews[ RTB_SHADOW_DEPTH0 + s ].EndRender( pRenderContext );
		}
	}
	VPROF_SCOPE_END();
	
	VPROF_SCOPE_BEGIN("RenderMultiShadows");
	for ( int s=0; s<m_shadowedSpotLights.Count(); ++s )
	{
		if ( s == 0 )
		{
			m_pRenderViews[ RTB_SHADOW_DEPTH_MULTIPLE_0 + s ].BeginRender( pRenderContext );

			pRenderContext->SetViewports( 1, &m_shadowViewport );
			pRenderContext->Clear( vShadowClear, RENDER_CLEAR_FLAGS_CLEAR_DEPTH );

			m_pRenderViews[ RTB_SHADOW_DEPTH_MULTIPLE_0 + s ].EndRender( pRenderContext, false );
		}

		if ( s == NUM_HIGH_RES_SHADOWS )
		{
			m_pRenderViews[ RTB_SHADOW_DEPTH_MULTIPLE_0 + s ].BeginRender( pRenderContext );

			pRenderContext->SetViewports( 1, &m_shadowViewport );
			pRenderContext->Clear( vShadowClear, RENDER_CLEAR_FLAGS_CLEAR_DEPTH );

			m_pRenderViews[ RTB_SHADOW_DEPTH_MULTIPLE_0 + s ].EndRender( pRenderContext, false );
		}
		
		if ( m_pRenderViews[ RTB_SHADOW_DEPTH_MULTIPLE_0 + s ].HasRenderables() )
		{
			RenderView( m_pRenderViews[ RTB_SHADOW_DEPTH_MULTIPLE_0 + s ] );
		}

		// Render spheres
		m_pRenderViews[ RTB_SHADOW_DEPTH_MULTIPLE_0 + s ].BeginRender( pRenderContext );

		ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pMultiShadowFrustums[ s ] );
		pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
		pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

		int nParticleSystems = m_sphereSystemList.Count();
		for ( int p=0; p<nParticleSystems; ++p )
		{
			RenderParticleLights( pRenderContext, m_sphereSystemList[p], VARIATION_DEPTH, false, false );
		}

		// Submit
		m_pRenderViews[ RTB_SHADOW_DEPTH_MULTIPLE_0 + s ].EndRender( pRenderContext );
	}
	VPROF_SCOPE_END();
}

void CWorldRendererTest::RenderSunShadowFrustums( IRenderContext *pRenderContext )
{
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );

	pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
	pRenderContext->SetBlendState( m_hAdditiveBS );
	pRenderContext->SetDepthStencilState( m_hStencilTestDS, g_nCanSeeSkyStencilRef );

	pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_NORMAL_HIGHRES ] );
	pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT );

	if ( IsPlatformX360() )
	{
		pRenderContext->BindTexture( 1, m_pRenderTargets[ DS_DEPTH_HIGHRES ] );
	}
	else
	{
		pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_VIEW_DEPTH_HIGHRES ] );
	}
	pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_POINT );

	pRenderContext->BindTexture( 2, m_pRenderTargets[ RT_VIEW_SPEC_HIGHRES ] );
	pRenderContext->SetSamplerStatePS( 2, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

	pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
	pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

	pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
	pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hShadowFrustumPS[2] );

	for ( int s=0; s<NUM_SHADOW_SPLITS; ++s )
	{
		m_deferredLightingData.m_mShadowMatrix[s] = m_pShadowMatrices[s];
		pRenderContext->BindTexture( 4 + s, m_pRenderTargets[ DS_SHADOW_DEPTH0 + s ] );

		if ( g_nPlatform == RST_PLATFORM_DX11 )
		{
			pRenderContext->SetSamplerStatePS( 4 + s, RS_FILTER_MIN_MAG_MIP_POINT );
		}
		else
		{
			pRenderContext->SetSamplerStatePS( 4 + s, RS_FILTER_MIN_MAG_MIP_LINEAR );
		}
	}

	pRenderContext->SetConstantBufferData( m_hDeferredLightingRelated, (void*)&m_deferredLightingData, sizeof( DeferredLightingConstants_t ) );
	pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDeferredLightingRelated, 0, 0 );

	RenderScreenQuad( pRenderContext, m_vEyeRays, "sunshadows" );
}

void CWorldRendererTest::RenderBouncedLight( IRenderContext *pRenderContext )
{
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );

	pRenderContext->SetBlendState( m_hAdditiveBS );
	pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_NO_WRITE );
	pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );

	pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_NORMAL_LOWRES ] );
	pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
	pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_VIEW_DEPTH_LOWRES ] );
	pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

	pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
	pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

	pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_lowResScreenData, sizeof( DecalScreenData_t ) );
	pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

	RenderSceneHemiLights( pRenderContext, m_exteriorHemiLights, false );
	RenderSceneHemiLights( pRenderContext, m_interiorHemiLights, true );
}

void CWorldRendererTest::RenderDirectLight( IRenderContext *pRenderContext )
{
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );
	pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
	pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

	if ( IsPlatformX360() )
	{
		pRenderContext->BindTexture( 1, m_pRenderTargets[ DS_DEPTH_HIGHRES ] );
	}
	else
	{
		pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_VIEW_DEPTH_HIGHRES ] );
	}
	pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_POINT );

	pRenderContext->BindTexture( 2, m_pRenderTargets[ RT_VIEW_SPEC_HIGHRES ] );
	pRenderContext->SetSamplerStatePS( 2, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

	// Additive light phase
	pRenderContext->SetBlendState( m_hAdditiveBS );
	pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE_STENCIL_TEST_NOTEQUAL );
	pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );

	int nParticleSystems = m_lightSystemList.Count();
	for ( int p=0; p<nParticleSystems; ++p )
	{
		RenderParticleLights( pRenderContext, m_lightSystemList[p], VARIATION_DEFAULT, false, false );
	}

	// Render scene lights
	pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_decalScreenData, sizeof( DecalScreenData_t ) );
	pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

	RenderScenePointLights( pRenderContext, m_exteriorPointLights, false );
	RenderScenePointLights( pRenderContext, m_interiorPointLights, true );
	RenderSceneSpotLights( pRenderContext, m_exteriorSpotLights, false );
	RenderSceneSpotLights( pRenderContext, m_interiorSpotLights, true );

	RenderShadowedSpotLights( pRenderContext, m_shadowedSpotLights );
}

void CWorldRendererTest::BlurLightBuffer( )
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );
	
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );

	// Bilat blur ( Horizontal )
	{
		m_pRenderViews[ RTB_LIGHT_BLUR_0 ].BeginRender( pRenderContext );

		pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
		//pRenderContext->SetBlendState( m_hWriteAllBS );
		pRenderContext->SetBlendMode( RENDER_BLEND_NONE );

		pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_DEPTH_HIGHRES ] );
		pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
		pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_LIGHTING_HIGHRES ] );
		pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

		pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
		pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_decalScreenData, sizeof( DecalScreenData_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hBlurParams, (void*)&m_blurParams, sizeof( BlurParams_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hBlurParams, 1, 7 );

		pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
		pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hBilateralBlur[2] );

		RenderScreenQuad( pRenderContext, m_vEyeRays, "bilatblurhorz" );

		// Submit
		m_pRenderViews[ RTB_LIGHT_BLUR_0 ].EndRender( pRenderContext );
	}

	// Bilat upsampling ( Vertical )
	{
		m_pRenderViews[ RTB_LIGHT_BLUR_1 ].BeginRender( pRenderContext );

		pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
		//pRenderContext->SetBlendState( m_hWriteAllBS );
		pRenderContext->SetBlendMode( RENDER_BLEND_NONE );

		pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_DEPTH_HIGHRES ] );
		pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
		pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_VIEW_SPEC_HIGHRES ] );
		pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

		pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
		pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_decalScreenData, sizeof( DecalScreenData_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hBlurParams, (void*)&m_blurParams, sizeof( BlurParams_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hBlurParams, 1, 7 );

		pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
		pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hBilateralBlur[3] );

		RenderScreenQuad( pRenderContext, m_vEyeRays, "bilatblurvert" );

		// Submit
		m_pRenderViews[ RTB_LIGHT_BLUR_1 ].EndRender( pRenderContext );
	}
}

void CWorldRendererTest::BlurShadowBuffers( )
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );
	
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );

	BlurParams_t highResBlurParams = m_blurParams;
	BlurParams_t lowResBlurParams = m_blurParams;

	// High res shadow blur
	{
		// blur ( Horizontal )
		{
			m_pRenderViews[ RTB_SHADOW_BLUR_0 ].BeginRender( pRenderContext );

			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );

			pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_SHADOW_MULTIPLE_0 ] );
			pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

			pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
			pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

			pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_shadowScreenData, sizeof( DecalScreenData_t ) );
			pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

			pRenderContext->SetConstantBufferData( m_hBlurParams, (void*)&highResBlurParams, sizeof( BlurParams_t ) );
			pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hBlurParams, 1, 7 );

			pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
			pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hShadowBlur[0] );

			RenderScreenQuad( pRenderContext, m_vEyeRays, "blurshadowshorz" );

			// Submit
			m_pRenderViews[ RTB_SHADOW_BLUR_0 ].EndRender( pRenderContext );
		}

		// blur ( Vertical )
		{
			m_pRenderViews[ RTB_SHADOW_BLUR_1 ].BeginRender( pRenderContext );

			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );

			pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_SHADOW_BLUR_TEMP ] );
			pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

			pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
			pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

			pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_shadowScreenData, sizeof( DecalScreenData_t ) );
			pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

			pRenderContext->SetConstantBufferData( m_hBlurParams, (void*)&highResBlurParams, sizeof( BlurParams_t ) );
			pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hBlurParams, 1, 7 );

			pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
			pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hShadowBlur[1] );

			RenderScreenQuad( pRenderContext, m_vEyeRays, "blurshadowvert" );

			// Submit
			m_pRenderViews[ RTB_SHADOW_BLUR_1 ].EndRender( pRenderContext );
		}
	}

	// Low res shadow blur
	{
		// blur ( Horizontal )
		{
			m_pRenderViews[ RTB_SHADOW_BLUR_0 ].BeginRender( pRenderContext );

			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );

			pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_SHADOW_MULTIPLE_1 ] );
			pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

			pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
			pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

			pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_shadowScreenData, sizeof( DecalScreenData_t ) );
			pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

			pRenderContext->SetConstantBufferData( m_hBlurParams, (void*)&lowResBlurParams, sizeof( BlurParams_t ) );
			pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hBlurParams, 1, 7 );

			pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
			pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hShadowBlur[2] );

			RenderScreenQuad( pRenderContext, m_vEyeRays, "blurshadowhorz2" );

			// Submit
			m_pRenderViews[ RTB_SHADOW_BLUR_0 ].EndRender( pRenderContext );
		}

		// blur ( Vertical )
		{
			m_pRenderViews[ RTB_SHADOW_BLUR_2 ].BeginRender( pRenderContext );

			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );

			pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_SHADOW_BLUR_TEMP ] );
			pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

			pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
			pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

			pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_shadowScreenData, sizeof( DecalScreenData_t ) );
			pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

			pRenderContext->SetConstantBufferData( m_hBlurParams, (void*)&lowResBlurParams, sizeof( BlurParams_t ) );
			pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hBlurParams, 1, 7 );

			pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
			pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hShadowBlur[3] );

			RenderScreenQuad( pRenderContext, m_vEyeRays, "blurshadowvert2" );

			// Submit
			m_pRenderViews[ RTB_SHADOW_BLUR_2 ].EndRender( pRenderContext );
		}
	}
}

void CWorldRendererTest::RenderDecals( IRenderContext *pRenderContext, int nVariation )
{
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );

	// Paint Decals
	if ( m_bFullBuffer2 || m_nGlobalDecalPos2 )
	{
		pRenderContext->SetBlendMode( RENDER_BLEND_ALPHABLENDING );

		pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
		pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_decalScreenData, sizeof( DecalScreenData_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

		if ( nVariation == 0 )
		{
			pRenderContext->BindTexture( 1, m_hDecalNormal2 );
			pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
		}
		else
		{
			pRenderContext->BindTexture( 1, m_hDecalAlbedo2 );
			pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
		}

		pRenderContext->BindVertexBuffer( 1, 0, 0, 0 );

		pRenderContext->BindVertexBuffer( 0, m_hDecalVB2, 0, sizeof( DecalVertex_t ) );
		pRenderContext->BindIndexBuffer( m_hDecalIB, 0 );
		pRenderContext->BindVertexShader( m_hDecalVS[ nVariation ], m_hDecalLayout );
		pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hDecalPS[ nVariation ] );

		int nIndices = 0;
		if ( m_bFullBuffer2 )
		{
			nIndices = m_nMaxDecals * 36;
		}
		else
		{
			nIndices = m_nGlobalDecalPos2 * 36;
		}

		pRenderContext->DrawIndexed( RENDER_PRIM_TRIANGLES, 0, nIndices );
	}

	// Bullet Decals
	if ( m_bFullBuffer || m_nGlobalDecalPos )
	{
		bool bDraw = true;
		pRenderContext->SetBlendMode( RENDER_BLEND_ALPHABLENDING );

		pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
		pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_decalScreenData, sizeof( DecalScreenData_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

		if ( nVariation == 0 )
		{
			pRenderContext->BindTexture( 1, m_hDecalNormal );
			pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
		}
		else
		{
			if ( m_hDecalAlbedo == RENDER_TEXTURE_HANDLE_INVALID )
			{
				bDraw = false;
			}
			else
			{
				pRenderContext->BindTexture( 1, m_hDecalAlbedo );
				pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
			}
		}

		if ( bDraw )
		{
			pRenderContext->BindVertexBuffer( 0, m_hDecalVB, 0, sizeof( DecalVertex_t ) );
			pRenderContext->BindIndexBuffer( m_hDecalIB, 0 );
			pRenderContext->BindVertexShader( m_hDecalVS[ nVariation ], m_hDecalLayout );
			pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hDecalPS[ nVariation ] );

			int nIndices = 0;
			if ( m_bFullBuffer )
			{
				nIndices = m_nMaxDecals * 36;
			}
			else
			{
				nIndices = m_nGlobalDecalPos * 36;
			}

			pRenderContext->DrawIndexed( RENDER_PRIM_TRIANGLES, 0, nIndices );
		}
	}

	// Wall monster decals
	if ( m_nGlobalDecalPos3 )
	{
		pRenderContext->SetBlendMode( RENDER_BLEND_ALPHABLENDING );

		pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
		pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_decalScreenData, sizeof( DecalScreenData_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

		if ( nVariation == 0 )
		{
			pRenderContext->BindTexture( 1, m_hDecalNormal3[ m_nWallMonsterTexture ] );
			pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
		
			pRenderContext->BindVertexBuffer( 0, m_hDecalVB3, 0, sizeof( DecalVertex_t ) );
			pRenderContext->BindIndexBuffer( m_hDecalIB, 0 );
			pRenderContext->BindVertexShader( m_hDecalVS[ nVariation ], m_hDecalLayout );
			pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hDecalPS[ nVariation ] );

			int nIndices = 36;
			pRenderContext->DrawIndexed( RENDER_PRIM_TRIANGLES, 0, nIndices );
		}
	}
}

void CWorldRendererTest::RenderDynamic( IRenderContext *pRenderContext, CDynamicRenderable *pRenderable, int nVariation, bool bSunDepth, bool bSetStencil )
{
	if ( bSunDepth )
	{
		bool bCanSeeSky = true;
		int nLeafNode = m_pWorldRenderer->GetLeafNodeForPoint( pRenderable->m_vOrigin );
		if ( nLeafNode > -1 )
		{
			IBVHNode *pNode = m_pWorldRenderer->GetNode( nLeafNode );
			if ( pNode )
			{
				int nFlags = pNode->GetFlags();
				bCanSeeSky = ( nFlags & NODE_CAN_SEE_SKY ) ? true : false;
			}
		}
	
		if ( !bCanSeeSky )
		{
			return;
		}
	}

	g_pMeshSystem->DrawRenderable( pRenderContext, pRenderable->m_hRenderable, m_hSimpleMeshVS[ nVariation ], m_hSimpleMeshPS[ nVariation ] );
}

void CWorldRendererTest::RenderParticleLights( IRenderContext *pRenderContext, CParticleCollection *pInputSystem, int nVariation, bool bSunDepth, bool bSetStencil )
{
	VPROF("RenderParticleLights");

	Vector vCenter = pInputSystem->GetControlPointAtCurrentTime( 0 );
	int nLeafNode = m_pWorldRenderer->GetLeafNodeForPoint( vCenter );
	IBVHNode *pNode = m_pWorldRenderer->GetNode( nLeafNode );
	int nFlags = pNode->GetFlags();
	bool bCanSeeSky = ( nFlags & NODE_CAN_SEE_SKY ) != 0;

	if ( bSunDepth && !bCanSeeSky )
		return;

	if ( bSetStencil )
	{	
		if ( bCanSeeSky )
		{
			pRenderContext->SetDepthStencilState( m_hSetStencilDS, g_nCanSeeSkyStencilRef );
		}
		else
		{
			pRenderContext->SetDepthStencilState( m_hSetStencilDS, g_nCannotSeeSkyStencilRef );
		}
	}

	pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_decalScreenData, sizeof( DecalScreenData_t ) );
	pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

	pRenderContext->BindVertexShader( m_hPointLightVS[nVariation], m_hPointLightLayout );
	pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hPointLightPS[nVariation] );

	pRenderContext->BindVertexBuffer( 0, m_hSphereVB, 0, sizeof( SphereVertex_t ) );
	pRenderContext->BindIndexBuffer( m_hSphereIB, 0 );

	CDynamicVertexData<PerParticleData_t> vb( pRenderContext, pInputSystem->m_nActiveParticles, "particles", "particles" );
	vb.Lock( );

	for ( int p=0; p<pInputSystem->m_nActiveParticles; ++p )
	{
		float const *pPos = pInputSystem->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, p );
		float const *pRadius = pInputSystem->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, p );
		float const *pColor = pInputSystem->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, p );

		switch( nVariation )
		{
		case VARIATION_DEFAULT:
			vb->m_vPosRadius.Init( pPos[0], pPos[4], pPos[8], pRadius[0] );
			break;
		case 4:
			vb->m_vPosRadius.Init( pPos[0], pPos[4], pPos[8], pRadius[0] * 0.05f );
			break;
		default:
			vb->m_vPosRadius.Init( pPos[0], pPos[4], pPos[8], pRadius[0] * 0.10f );
			break;
		}

		vb->m_vColor.Init( pColor[0], pColor[4], pColor[8], 1 );
		//vb->m_vColor.Init( 0.9, 0.4, 1.0, 1 );
		vb->m_vColor.AsVector3D().NormalizeInPlace();
		vb.AdvanceVertex();
	}

	vb.Unlock( );
	vb.Bind( 1, 0 );

	pRenderContext->DrawIndexedInstanced( RENDER_PRIM_TRIANGLES, 0, m_nNumTriangles * 3, pInputSystem->m_nActiveParticles );
}

void CWorldRendererTest::RenderScenePointLights( IRenderContext *pRenderContext, CUtlVector<CPointLightRenderable> &lightVector, bool bInterior )
{
	if ( bInterior )
	{
		pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_FRONTFACING );
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_GREATER_NO_WRITE );
	}
	else
	{
		pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_NO_WRITE );
	}

	const int nMaxLightsPerDraw = 512;
	CDynamicVertexData<PointLightData_t> *pVB = NULL;

	int nLights = lightVector.Count();
	int nLightsDrawn = 0;
	for ( int i=0; i<nLights; ++i )
	{
		if ( !pVB )
		{
			pVB = new CDynamicVertexData<PointLightData_t>( pRenderContext, nMaxLightsPerDraw, "scenelights_point", "scenelights_point" );
			pVB->Lock();
		}

		Vector vLightOrigin = lightVector[i].m_pLight->m_vOrigin;
		Vector vNewOrigin = vLightOrigin - lightVector[i].m_vOrigin;
		(*pVB)->m_vOrigin = vNewOrigin;
		(*pVB)->m_vColorNRadius = lightVector[i].m_pLight->m_vColorNRadius;
		(*pVB)->m_vAttenuation = lightVector[i].m_pLight->m_vAttenuation;
		pVB->AdvanceVertex();

		nLightsDrawn ++;
		if ( nLightsDrawn >= nMaxLightsPerDraw )
		{
			nLightsDrawn = 0;
			pVB->Unlock();

			pRenderContext->BindVertexBuffer( 0, m_hSphereVB, 0, sizeof( SphereVertex_t ) );
			pRenderContext->BindIndexBuffer( m_hSphereIB, 0 );
			pVB->Bind( 1, 0 );
			
			pRenderContext->BindVertexShader( m_hSceneLightVS[0], m_hSceneLightLayout[0] );
			pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hSceneLightPS[0] );

			pRenderContext->DrawIndexedInstanced( RENDER_PRIM_TRIANGLES, 0, m_nSphereIndices, nMaxLightsPerDraw );
			
			delete pVB;
			pVB = NULL;
		}
	}

	if ( pVB )
	{
		Assert( nLightsDrawn > 0 );
		pVB->Unlock();

		pRenderContext->BindVertexBuffer( 0, m_hSphereVB, 0, sizeof( SphereVertex_t ) );
		pRenderContext->BindIndexBuffer( m_hSphereIB, 0 );
		pVB->Bind( 1, 0 );
		
		pRenderContext->BindVertexShader( m_hSceneLightVS[0], m_hSceneLightLayout[0] );
		pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hSceneLightPS[0] );

		pRenderContext->DrawIndexedInstanced( RENDER_PRIM_TRIANGLES, 0, m_nSphereIndices, nLightsDrawn );

		delete pVB;
	}
}

void CWorldRendererTest::RenderSceneHemiLights( IRenderContext *pRenderContext, CUtlVector<CHemiLightRenderable> &lightVector, bool bInterior )
{
	if ( bInterior )
	{
		pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_FRONTFACING );
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_GREATER_NO_WRITE );
	}
	else
	{
		pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_NO_WRITE );
	}

	const int nMaxLightsPerDraw = 512;
	CDynamicVertexData<HemiLightData_t> *pVB = NULL;

	int nLights = lightVector.Count();
	int nLightsDrawn = 0;
	for ( int i=0; i<nLights; ++i )
	{
		if ( !pVB )
		{
			pVB = new CDynamicVertexData<HemiLightData_t>( pRenderContext, nMaxLightsPerDraw, "scenelights_hemi", "scenelights_hemi" );
			pVB->Lock();
		}
		Vector vOrigin = lightVector[i].m_vOrigin;

		(*pVB)->m_vTransform0 = lightVector[i].m_pLight->m_vTransform0;
		(*pVB)->m_vTransform1 = lightVector[i].m_pLight->m_vTransform1;
		(*pVB)->m_vTransform2 = lightVector[i].m_pLight->m_vTransform2;
		(*pVB)->m_vTransform0.w -= vOrigin.x;
		(*pVB)->m_vTransform1.w -= vOrigin.y;
		(*pVB)->m_vTransform2.w -= vOrigin.z;
		(*pVB)->m_vColorNRadius = lightVector[i].m_pLight->m_vColorNRadius;
		(*pVB)->m_vAttenuation = lightVector[i].m_pLight->m_vAttenuation;

		pVB->AdvanceVertex();

		nLightsDrawn ++;
		if ( nLightsDrawn >= nMaxLightsPerDraw )
		{
			nLightsDrawn = 0;
			pVB->Unlock();

			pRenderContext->BindVertexBuffer( 0, m_hHemiVB, 0, sizeof( SphereVertex_t ) );
			pRenderContext->BindIndexBuffer( m_hHemiIB, 0 );
			pVB->Bind( 1, 0 );
			
			pRenderContext->BindVertexShader( m_hSceneLightVS[1], m_hSceneLightLayout[1] );
			pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hSceneLightPS[1] );

			pRenderContext->DrawIndexedInstanced( RENDER_PRIM_TRIANGLES, 0, m_nHemiIndices, nMaxLightsPerDraw );
			
			delete pVB;
			pVB = NULL;
		}
	}

	if ( pVB )
	{
		pVB->Unlock();
		if ( nLightsDrawn > 0 )
		{
			pRenderContext->BindVertexBuffer( 0, m_hHemiVB, 0, sizeof( SphereVertex_t ) );
			pRenderContext->BindIndexBuffer( m_hHemiIB, 0 );
			pVB->Bind( 1, 0 );
			
			pRenderContext->BindVertexShader( m_hSceneLightVS[1], m_hSceneLightLayout[1] );
			pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hSceneLightPS[1] );

			pRenderContext->DrawIndexedInstanced( RENDER_PRIM_TRIANGLES, 0, m_nHemiIndices, nLightsDrawn );
		}

		delete pVB;
	}
}

void CWorldRendererTest::RenderSceneSpotLights( IRenderContext *pRenderContext, CUtlVector<CSpotLightRenderable> &lightVector, bool bInterior )
{
	if ( bInterior )
	{
		pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_FRONTFACING );
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_GREATER_NO_WRITE );
	}
	else
	{
		pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_NO_WRITE );
	}

	const int nMaxLightsPerDraw = 512;
	CDynamicVertexData<SpotLightData_t> *pVB = NULL;

	int nLights = lightVector.Count();
	int nLightsDrawn = 0;
	for ( int i=0; i<nLights; ++i )
	{
		if ( !lightVector[i].m_bValid )
			continue;

		if ( !pVB )
		{
			pVB = new CDynamicVertexData<SpotLightData_t>( pRenderContext, nMaxLightsPerDraw, "scenelights_spot", "scenelights_spot" );
			pVB->Lock();
		}

		Vector vOrigin = lightVector[i].m_vOrigin;

		(*pVB)->m_vTransform0 = lightVector[i].m_pLight->m_vTransform0;
		(*pVB)->m_vTransform1 = lightVector[i].m_pLight->m_vTransform1;
		(*pVB)->m_vTransform2 = lightVector[i].m_pLight->m_vTransform2;
		(*pVB)->m_vTransform0.w -= vOrigin.x;
		(*pVB)->m_vTransform1.w -= vOrigin.y;
		(*pVB)->m_vTransform2.w -= vOrigin.z;
		(*pVB)->m_vColorNRadius = lightVector[i].m_pLight->m_vColorNRadius;
		(*pVB)->m_vAttenuationNCosSpot = lightVector[i].m_pLight->m_vAttenuationNCosSpot;

		pVB->AdvanceVertex();

		nLightsDrawn ++;
		if ( nLightsDrawn >= nMaxLightsPerDraw )
		{
			nLightsDrawn = 0;
			pVB->Unlock();

			pRenderContext->BindVertexBuffer( 0, m_hConeVB, 0, sizeof( SphereVertex_t ) );
			pRenderContext->BindIndexBuffer( m_hConeIB, 0 );
			pVB->Bind( 1, 0 );
			
			pRenderContext->BindVertexShader( m_hSceneLightVS[2], m_hSceneLightLayout[2] );
			pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hSceneLightPS[2] );

			pRenderContext->DrawIndexedInstanced( RENDER_PRIM_TRIANGLES, 0, m_nHemiIndices, nMaxLightsPerDraw );
			
			delete pVB;
			pVB = NULL;
		}
	}

	if ( pVB )
	{
		pVB->Unlock();

		Assert( nLightsDrawn > 0 );
		pRenderContext->BindVertexBuffer( 0, m_hConeVB, 0, sizeof( SphereVertex_t ) );
		pRenderContext->BindIndexBuffer( m_hConeIB, 0 );
		pVB->Bind( 1, 0 );
		
		pRenderContext->BindVertexShader( m_hSceneLightVS[2], m_hSceneLightLayout[2] );
		pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hSceneLightPS[2] );

		pRenderContext->DrawIndexedInstanced( RENDER_PRIM_TRIANGLES, 0, m_nConeIndices, nLightsDrawn );

		delete pVB;
	}
}


void CWorldRendererTest::RenderShadowedSpotLights( IRenderContext *pRenderContext, CUtlVector<CSpotLightRenderable> &lightVector )
{
	int nLights = lightVector.Count();
	if ( nLights == 0 )
		return;

	const int nMaxLightsPerDraw = NUM_MULTI_SHADOWS;
	CDynamicVertexData<SpotLightData_t> *pVB = new CDynamicVertexData<SpotLightData_t>( pRenderContext, nMaxLightsPerDraw, "scenelights_shadowspot", "scenelights_shadowspot" );
	pVB->Lock();

	Assert( nLights <= NUM_MULTI_SHADOWS );
	for ( int i=0; i<nLights; ++i )
	{
		Vector vOrigin = lightVector[i].m_vOrigin;

		(*pVB)->m_vTransform0 = lightVector[i].m_pLight->m_vTransform0;
		(*pVB)->m_vTransform1 = lightVector[i].m_pLight->m_vTransform1;
		(*pVB)->m_vTransform2 = lightVector[i].m_pLight->m_vTransform2;
		(*pVB)->m_vTransform0.w -= vOrigin.x;
		(*pVB)->m_vTransform1.w -= vOrigin.y;
		(*pVB)->m_vTransform2.w -= vOrigin.z;
		(*pVB)->m_vColorNRadius = lightVector[i].m_pLight->m_vColorNRadius;
		(*pVB)->m_vAttenuationNCosSpot = lightVector[i].m_pLight->m_vAttenuationNCosSpot;

		pVB->AdvanceVertex();
	}

	pVB->Unlock();

	// Bind
	pRenderContext->BindVertexShader( m_hSceneLightVS[2], m_hSceneLightLayout[2] );
	pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hSceneLightPS[5] );

	pRenderContext->BindVertexBuffer( 0, m_hConeVB, 0, sizeof( SphereVertex_t ) );
	pRenderContext->BindIndexBuffer( m_hConeIB, 0 );

	for ( int i=0; i<nLights; ++i )
	{
		if ( i < NUM_HIGH_RES_SHADOWS )
		{
			pRenderContext->BindTexture( 4, m_pRenderTargets[ RT_SHADOW_MULTIPLE_0 ] );
			pRenderContext->SetSamplerStatePS( 4, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

			pRenderContext->BindTexture( 3, m_hFlashlightCookie[0] );
			pRenderContext->SetSamplerStatePS( 3, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
		}
		else
		{
			pRenderContext->BindTexture( 4, m_pRenderTargets[ RT_SHADOW_MULTIPLE_1 ] );
			pRenderContext->SetSamplerStatePS( 4, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

			pRenderContext->BindTexture( 3, m_hFlashlightCookie[1] );
			pRenderContext->SetSamplerStatePS( 3, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
		}

		pRenderContext->SetConstantBufferData( m_hSingleMatrixRelated, &m_pMultiShadowMatrices[ i ], sizeof( VMatrix ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hSingleMatrixRelated, 1, 7 );

		if ( lightVector[i].m_bInterior )
		{
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_FRONTFACING );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_GREATER_NO_WRITE );
		}
		else
		{
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_NO_WRITE );
		}

		pVB->Bind( 1, i * sizeof( SpotLightData_t ) );

		pRenderContext->DrawIndexedInstanced( RENDER_PRIM_TRIANGLES, 0, m_nConeIndices, 1 );
	}

	delete pVB;
}

void CWorldRendererTest::CalculateEyeRays( float flFarPlane )
{
	Vector vForward = m_pFrustum->Forward();
	Vector vUp = m_pFrustum->Up();
	Vector vLeft = m_pFrustum->Left();

	float flFovX = m_pFrustum->GetFOV();
	float flFovY = CalcFovY( flFovX, m_pFrustum->GetAspect() );
	flFovX *= 0.5f;
	flFovY *= 0.5f;
	Vector vFowardShift = flFarPlane * vForward;
	Vector vUpShift = flFarPlane * tanf( DEG2RAD( flFovY ) ) * vUp;
	Vector vRightShift = flFarPlane * tanf( DEG2RAD( flFovX ) ) * -vLeft;
	m_vEyeRays[0] = vFowardShift + -vRightShift + -vUpShift;
	m_vEyeRays[1] = vFowardShift + vRightShift + -vUpShift;
	m_vEyeRays[2] = vFowardShift + -vRightShift + vUpShift;
	m_vEyeRays[3] = vFowardShift + vRightShift + vUpShift;

	vFowardShift = flFarPlane * Vector( 0, 0, 1 );
	vUpShift = flFarPlane * tanf( DEG2RAD( flFovY ) ) * Vector( 0, 1, 0 );
	vRightShift = flFarPlane * tanf( DEG2RAD( flFovX ) ) * Vector( 1, 0, 0 );
	m_vEyeLocalRays[0] = vFowardShift + -vRightShift + -vUpShift;
	m_vEyeLocalRays[1] = vFowardShift + vRightShift + -vUpShift;
	m_vEyeLocalRays[2] = vFowardShift + -vRightShift + vUpShift;
	m_vEyeLocalRays[3] = vFowardShift + vRightShift + vUpShift;
}

void CWorldRendererTest::BilateralUpsample( )
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );
	
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );

	// Bilat upsampling ( Horizontal )
	{
		m_pRenderViews[ RTB_BILAT_UPSAMPLE_0 ].BeginRender( pRenderContext );

		pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
		pRenderContext->SetBlendState( m_hWriteAllBS );

		if ( IsPlatformX360() )
			pRenderContext->BindTexture( 0, m_pRenderTargets[ DS_DEPTH_LOWRES ] );
		else
			pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_DEPTH_LOWRES ] );
		pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
		pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_LIGHTING_LOWRES ] );
		pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

		pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
		pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_lowResScreenData, sizeof( DecalScreenData_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hBlurParams, (void*)&m_blurParams, sizeof( BlurParams_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hBlurParams, 1, 7 );

		pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
		pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hBilateralBlur[0] );

		RenderScreenQuad( pRenderContext, m_vEyeRays, "bilatupsamplehorz" );

		// Submit
		m_pRenderViews[ RTB_BILAT_UPSAMPLE_0 ].EndRender( pRenderContext );
	}

	// Bilat upsampling ( Vertical )
	{
		m_pRenderViews[ RTB_BILAT_UPSAMPLE_1 ].BeginRender( pRenderContext );

		pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
		pRenderContext->SetBlendState( m_hWriteAllBS );

		if ( IsPlatformX360() )
			pRenderContext->BindTexture( 0, m_pRenderTargets[ DS_DEPTH_HIGHRES ] );
		else
			pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_DEPTH_HIGHRES ] );

		pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
		pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_LIGHTING_TEMP_LOWRES ] );
		pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

		pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
		pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_decalScreenData, sizeof( DecalScreenData_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

		pRenderContext->SetConstantBufferData( m_hBlurParams, (void*)&m_blurParams, sizeof( BlurParams_t ) );
		pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hBlurParams, 1, 7 );

		pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
		pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hBilateralBlur[1] );

		RenderScreenQuad( pRenderContext, m_vEyeRays, "bilatupsamplevert" );

		// Submit
		m_pRenderViews[ RTB_BILAT_UPSAMPLE_1 ].EndRender( pRenderContext );
	}
}

void CWorldRendererTest::RenderSSAO()
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );

	m_pRenderViews[ RTB_SSAO_LOWRES ].BeginRender( pRenderContext );

	pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
	pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
	pRenderContext->SetBlendState( m_hWriteAlphaBS );

#if 0
	pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_DEPTH_LOWRES ] );
	pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
	pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_VIEW_NORMAL_LOWRES ] );
	pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_LINEAR, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
	pRenderContext->BindTexture( 2, m_hRandomTexture );
	pRenderContext->SetSamplerStatePS( 2, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_WRAP, RS_TEXTURE_ADDRESS_WRAP, RS_TEXTURE_ADDRESS_WRAP );
#else
	pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_DEPTH_LOWRES ] );
	pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
	pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_VIEW_NORMAL_LOWRES ] );
	pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
	pRenderContext->BindTexture( 2, m_hRandomTexture );
	pRenderContext->SetSamplerStatePS( 2, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_WRAP, RS_TEXTURE_ADDRESS_WRAP, RS_TEXTURE_ADDRESS_WRAP );
#endif

	pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
	pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

	pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_lowResScreenData, sizeof( DecalScreenData_t ) );
	pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

	pRenderContext->SetConstantBufferData( m_hSphereSampleData, &m_sphereSampleData, sizeof( SampleSphereData_t ) );
	pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hSphereSampleData, 1, 7 );
	
	pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
	pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hSSAOPS );
	RenderScreenQuad( pRenderContext, m_vEyeRays/*m_vEyeLocalRays*/, "ssao" );

	// Submit
	m_pRenderViews[ RTB_SSAO_LOWRES ].EndRender( pRenderContext );
}

void CWorldRendererTest::ScaleDepthAndNormalBuffers()
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );

	m_pRenderViews[ RTB_DEFERRED_SCALE_DOWN ].BeginRender( pRenderContext );

	pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
	pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
	pRenderContext->SetDepthStencilState( m_hZWriteNoTestDS );

	if ( IsPlatformX360() )
	{
		pRenderContext->BindTexture( 0, m_pRenderTargets[ DS_DEPTH_HIGHRES ] );
	}
	else
	{
		pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_DEPTH_HIGHRES ] );
	}
	pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT/*RENDER_TEXFILTERMODE_LINEAR*/, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

	pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_VIEW_NORMAL_HIGHRES ] );
	pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_POINT/*RENDER_TEXFILTERMODE_LINEAR*/, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

	pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
	pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

	DecalScreenData_t lowResCopyData = m_lowResScreenData;
	lowResCopyData.m_mInvViewProj = viewConstants.m_mViewProjection;
	pRenderContext->SetConstantBufferData( m_hDecalScreenData, &lowResCopyData, sizeof( DecalScreenData_t ) );
	pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

	pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );
	pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hCopyTexture[0] );

	RenderScreenQuad( pRenderContext, m_vEyeRays, "scaledepthnorm" );

	// Submit
	m_pRenderViews[ RTB_DEFERRED_SCALE_DOWN ].EndRender( pRenderContext );
}

void CWorldRendererTest::RenderAerialPerspective()
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );
	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );

	m_pRenderViews[ RTB_AERIAL_PERSPECTIVE ].BeginRender( pRenderContext );

	pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE_STENCIL_TEST_NOTEQUAL );
	pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );

	if ( IsPlatformX360() )
	{
		pRenderContext->BindTexture( 0, m_pRenderTargets[ DS_DEPTH_HIGHRES ] );
	}
	else
	{
		pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_DEPTH_HIGHRES ] );
	}
	pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT );

	pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
	pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

	pRenderContext->SetConstantBufferData( m_hAerialPerspectivePSCB, (void*)&m_aerialPerspectiveData, sizeof( AerialPerspectiveConstants_t ) );
	pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hAerialPerspectivePSCB, 0, 0 );

	pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );

	// Extinction
	pRenderContext->SetBlendState( m_hExtinctionBS );
	pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hAerialPerspectivePS[0] );
	RenderScreenQuad( pRenderContext, m_vEyeRays, "exinction" );

	// In-scattering
	pRenderContext->SetDepthStencilState( m_hStencilTestDS, g_nCanSeeSkyStencilRef );
	pRenderContext->SetBlendState( m_hInscatteringBS );
	pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hAerialPerspectivePS[1] );
	RenderScreenQuad( pRenderContext, m_vEyeRays, "inscattering" );

	// Submit
	m_pRenderViews[ RTB_AERIAL_PERSPECTIVE ].EndRender( pRenderContext );
}

void CWorldRendererTest::RenderSky()
{
	VPROF("RenderSky");

	CRenderContextPtr pRenderContext( g_pRenderDevice );

	// Update the camera
	m_pFrustum->SetNearFarPlanes( m_flNearPlane, m_flFarPlane );
	m_pFrustum->UpdateFrustumFromCamera();

	if ( m_bDeferred )
	{
		m_pRenderViews[ RTB_SKY ].BeginRender( pRenderContext );
	}
	else
	{
		m_pRenderViews[ RTB_FORWARD ].BeginRender( pRenderContext );
	}

	// Sky
	pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
	pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_FRONTFACING );

	if ( m_bDeferred )
	{
		pRenderContext->SetDepthStencilState( m_hZTestAndStencilTestDS, g_nNothingRenderedStencilRef );
	}
	else
	{
		pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_NO_WRITE );
	}
	SkyVSCB_t skyVSCB;

	VMatrix mTrans;
	mTrans.Identity();
	mTrans.SetTranslation( m_pFrustum->GetCameraPosition() );
	skyVSCB.m_mViewProj = mTrans.Transpose() * m_pFrustum->GetViewProj();
	skyVSCB.m_vSkyScale = Vector4D( 17000.0f, 17000.0f, 3000.0f, 0.0f );
	m_skyData.m_vEyePt = Vector4D( 0,0,0,0 );
	m_skyData.m_vLightDir = Vector4D( m_vLightDir.x, m_vLightDir.y, m_vLightDir.z, 1 );

	pRenderContext->SetConstantBufferData( m_hSkyVSCB, (void*)&skyVSCB, sizeof( SkyVSCB_t ) );
	pRenderContext->SetConstantBufferData( m_hSkyPSCB, (void*)&m_skyData, sizeof( SkyPSCB_t ) );
	pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hSkyVSCB, 0, 0 );
	pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hSkyPSCB, 0, 0 );
	pRenderContext->BindVertexShader( m_hSkyVS, m_hSphereLayout );
	pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hSkyPS );
	pRenderContext->BindVertexBuffer( 0, m_hSphereVB, 0, sizeof( SphereVertex_t ) );
	pRenderContext->BindIndexBuffer( m_hSphereIB, 0 );
	pRenderContext->DrawIndexed( RENDER_PRIM_TRIANGLES, 0, m_nNumTriangles * 3 );

	// Submit
	if ( m_bDeferred )
	{
		m_pRenderViews[ RTB_SKY ].EndRender( pRenderContext );
	}
	else
	{
		m_pRenderViews[ RTB_FORWARD ].EndRender( pRenderContext );
	}
}

void CWorldRendererTest::RenderFrame_Internal( const RenderViewport_t &viewport )
{
	VPROF("RenderFrame_Internal");
	if ( !m_bWorldLoaded )
		return;

	m_nWindowCenterX = viewport.m_nTopLeftX + (viewport.m_nWidth / 2);
	m_nWindowCenterY = viewport.m_nTopLeftY + (viewport.m_nHeight / 2);

	RenderViewport_t lowResViewport;
	lowResViewport = viewport;
	lowResViewport.m_nWidth = (int)( lowResViewport.m_nWidth * m_flLowResBufferScale );
	lowResViewport.m_nHeight = (int)( lowResViewport.m_nHeight * m_flLowResBufferScale );

	UpdateFrustum( viewport.m_nWidth, viewport.m_nHeight );

	CRenderContextPtr pRenderContext( g_pRenderDevice );
	
	VMatrix mWorld;
	mWorld.Identity();

	Vector vCameraPos = m_pFrustum->GetCameraPosition();

	float flFarPlane = m_pWorldRenderer->GetMaxVisibleDistance( vCameraPos );
	float flShiftedFar = m_flFarPlane;
	if ( m_bEnableVis )
		flShiftedFar = MIN( flFarPlane, m_flFarPlane );

	CWorldRenderFrustum *pCullFrustum = m_pFrustum;
	if ( m_bDropFrustum )
	{
		pCullFrustum = m_pDropFrustum;
		vCameraPos = m_vCullPos;
		flShiftedFar = m_flFarPlane;
	}

	// Update the camera
	m_pFrustum->SetNearFarPlanes( m_flNearPlane, flShiftedFar );
	m_pFrustum->UpdateFrustumFromCamera();
	VMatrix mViewProj = m_pFrustum->GetViewProj();

	// Shadow split frustums
	float flLightDistance = 5000.0f;
	float flLightFarPlane = 8000.0f;
	float flMaxShadowDistance = 5000.0f;
	m_pFrustum->CalculateLightFrusta( m_pSplitFrustums, m_nShadowSplits, m_vLightDir, flLightDistance, flLightFarPlane, flMaxShadowDistance );

	// Pull flashlight data from the camera
	UpdateFlashlightData();

	// Pull viewmodel data from the camera
	UpdateViewModelData();

	// Build renderables list
	GenerateRenderables( vCameraPos );

	// Update Shadow Data (do this before culling!!!)
	UpdateShadowData();

	// Culling
	if ( m_bDeferred )
	{
		for ( int v=0; v<RTB_FORWARD; ++v )
		{
			CullRenderablesAgainstView( m_pRenderViews[ v ], m_renderableList, m_dynRenderableList );
		}
	}
	else
	{
		for ( int v=RTB_SHADOW_DEPTH0; v<RTB_SHADOW_DEPTH_MULTIPLE_0; ++v )
		{
			CullRenderablesAgainstView( m_pRenderViews[ v ], m_renderableList, m_dynRenderableList );
		}

		CullRenderablesAgainstView( m_pRenderViews[ RTB_FORWARD ], m_renderableList, m_dynRenderableList );
	}

	// Update the camera to make sure we shift the far plane out enough to encompass any lights
	flShiftedFar = MAX( flShiftedFar, m_flMaxLightEncompassingFarPlane );
	m_pFrustum->SetNearFarPlanes( m_flNearPlane, flShiftedFar );
	m_pFrustum->UpdateFrustumFromCamera();
	mViewProj = m_pFrustum->GetViewProj();

	ViewRelatedConstants_t viewConstants = CreateViewConstantsForFrustum( m_pFrustum );
	m_forwardLightingData.m_vLightColor = m_vRenderLightColor;
	m_forwardLightingData.m_vLightDir = Vector4D( m_vLightDir.x, m_vLightDir.y, m_vLightDir.z, 1 );
	m_lightBufferData.m_vInvScreenExtents = Vector4D( 1.0f / viewport.m_nWidth, 1.0f / viewport.m_nHeight, 0, 0 );

	// Calculate eye rays
	CalculateEyeRays( flShiftedFar );

	//
	// Start of rendering
	//

	// Shadow depth buffers
	RenderShadowDepthBuffers( pRenderContext );

	if ( m_bDeferred )
	{
		m_deferredLightingData.m_vLightColor = m_vRenderLightColor;
		m_deferredLightingData.m_vLightDir = Vector4D( m_vLightDir.x, m_vLightDir.y, m_vLightDir.z, 1 );
		m_deferredLightingData.m_vInvScreenExtents = Vector4D( 1.0f / viewport.m_nWidth, 1.0f / viewport.m_nHeight, 0, 0 );
		m_deferredLightingData.m_vEyePt = Vector4D( vCameraPos.x, vCameraPos.y, vCameraPos.z, 1 );

		m_aerialPerspectiveData.m_vLightDir = m_deferredLightingData.m_vLightDir;
		m_aerialPerspectiveData.m_vLightColor = m_skyData.m_vLightColor;
		m_aerialPerspectiveData.m_vInvScreenExtents = m_deferredLightingData.m_vInvScreenExtents;
		m_aerialPerspectiveData.m_vBm = m_skyData.m_vBm;
		m_aerialPerspectiveData.m_flG = Vector4D( 0.7f, 0,0,0 );
		m_aerialPerspectiveData.m_vNearAndFar = Vector4D( m_flNearPlane, flShiftedFar, 0, 0 );
		m_aerialPerspectiveData.m_vEyePt = Vector4D( vCameraPos.x, vCameraPos.y, vCameraPos.z, 1 );

		m_sphereSampleData.m_mViewProj = viewConstants.m_mViewProjection;
		m_sphereSampleData.m_vRandSampleScale.Init( 1 / (float)m_nRandTextureSize, 1 / (float)m_nRandTextureSize, 
													0.5f + ( 0.5f / lowResViewport.m_nWidth ),
													0.5f + ( 0.5f / lowResViewport.m_nHeight ) );
		float flSSAOBias = 0.005f;
		float flSSAOStrenth = 5.0f;
		m_sphereSampleData.m_vSampleRadiusNBias.Init( m_flSampleRadius, flShiftedFar / ( m_flSampleRadius * flSSAOStrenth ), flShiftedFar, flSSAOBias * flShiftedFar );		
		m_blurParams.m_flFarPlane.Init( flShiftedFar, flShiftedFar, flShiftedFar, flShiftedFar );
		
		VMatrix mPosLookupViewProj = mViewProj;
		if ( IsPlatformX360() )
		{
			VMatrix mDepthFlip( 1.0f,    0.0f,     0.0f,      0.0f,
								0.0f,    1.0f,     0.0f,      0.0f,
								0.0f,    0.0f,     -1.0f,	  0.0f,
								0.0f,    0.0f,     1.0f,	  1.0f );
			mPosLookupViewProj = mPosLookupViewProj * mDepthFlip;
		}
		if ( !MatrixInverseGeneral( mPosLookupViewProj, m_deferredLightingData.m_mInvViewProj ) )
			return;

		m_decalScreenData.m_vInvScreenExtents = m_deferredLightingData.m_vInvScreenExtents;
		m_decalScreenData.m_mInvViewProj = m_deferredLightingData.m_mInvViewProj;
		m_decalScreenData.m_vEyePt = m_aerialPerspectiveData.m_vEyePt;
		m_decalScreenData.m_vEyeDir = viewConstants.m_vEyeDir;

		m_lowResScreenData.m_vInvScreenExtents = Vector4D( 1.0f / lowResViewport.m_nWidth, 1.0f / lowResViewport.m_nHeight, 0, 0 );
		m_lowResScreenData.m_mInvViewProj = m_deferredLightingData.m_mInvViewProj;
		m_lowResScreenData.m_vEyePt = m_aerialPerspectiveData.m_vEyePt;
		m_lowResScreenData.m_vEyeDir = viewConstants.m_vEyeDir;

		m_shadowScreenData.m_vInvScreenExtents = Vector4D( 1.0f / m_shadowViewport.m_nWidth, 1.0f / m_shadowViewport.m_nHeight, 0, 0 );
		m_shadowScreenData.m_mInvViewProj = m_deferredLightingData.m_mInvViewProj;
		m_shadowScreenData.m_vEyePt = m_aerialPerspectiveData.m_vEyePt;
		m_shadowScreenData.m_vEyeDir = viewConstants.m_vEyeDir;

		m_aerialPerspectiveData.m_mInvViewProj = m_deferredLightingData.m_mInvViewProj;;
		
		// Blur shadow buffers
		if ( m_bBlurShadows )
		{
			BlurShadowBuffers();
		}

		// Render depth and normal pass
		{
			// SetRenderTarget must be the first call in any command list
			Vector4D vNormalClear(1,1,1,1);
			m_pRenderViews[ RTB_DEFERRED_DEPTH_NORM_SPEC ].Clear( pRenderContext, vNormalClear, RENDER_CLEAR_FLAGS_CLEAR_DEPTH | RENDER_CLEAR_FLAGS_CLEAR_STENCIL );
			
			RenderView( m_pRenderViews[ RTB_DEFERRED_DEPTH_NORM_SPEC ] );

			// Render spheres
			m_pRenderViews[ RTB_DEFERRED_DEPTH_NORM_SPEC ].BeginRender( pRenderContext );

			VMatrix mViewRelated;
			mViewRelated = m_pFrustum->GetViewProj();
			pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
			pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_AND_WRITE_STENCIL_SET1 );
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );

			int nParticleSystems = m_sphereSystemList.Count();
			for ( int p=0; p<nParticleSystems; ++p )
			{
				RenderParticleLights( pRenderContext, m_sphereSystemList[p], VARIATION_NORM_DEPTH_SPEC, false, true );
			}

			// Submit
			m_pRenderViews[ RTB_DEFERRED_DEPTH_NORM_SPEC ].EndRender( pRenderContext );
		}

		// Clear low -res lighting
		{
			Vector4D vLightingClear(0,0,0,1);
			m_pRenderViews[ RTB_LIGHTING_LOWRES ].Clear( pRenderContext, vLightingClear, 0 );
		}

		// Decals
		{
			m_pRenderViews[ RTB_DEFERRED_NORM_SPEC ].BeginRender( pRenderContext );

			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_NO_WRITE );

			// Render decals into normal map... using depth
			if ( IsPlatformX360() )
			{
				pRenderContext->BindTexture( 0, m_pRenderTargets[ DS_DEPTH_HIGHRES ] );
			}
			else
			{
				pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_DEPTH_HIGHRES ] );
			}
			pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT );

			RenderDecals( pRenderContext, 0 );

			// Submit
			m_pRenderViews[ RTB_DEFERRED_NORM_SPEC ].EndRender( pRenderContext );
		}

		// Scale depth and normal buffers
		ScaleDepthAndNormalBuffers();

		// SSAO only requires depth (and normal later)
		if ( m_bSSAO )
		{
			RenderSSAO();
		}
	
		// Low res lighting requires depth and normal
		{
			m_pRenderViews[ RTB_LIGHTING_LOWRES ].BeginRender( pRenderContext );

			if ( m_bDrawBouncedLight )
			{
				RenderBouncedLight( pRenderContext );
			}

			// Submit
			m_pRenderViews[ RTB_LIGHTING_LOWRES ].EndRender( pRenderContext );
		}

		// Upsample
		BilateralUpsample();
		
		// Clear specular buffer
		{
			Vector4D vSpecClear(0,0,0,0);
			m_pRenderViews[ RTB_CLEAR_SPEC_HIGHRES ].Clear( pRenderContext, vSpecClear, 0 );
		}

		// Do lighting
		{
			m_pRenderViews[ RTB_LIGHTING_HIGHRES ].BeginRender( pRenderContext );

			RenderSunShadowFrustums( pRenderContext );

			if ( m_bDrawDirectLight )
			{
				RenderDirectLight( pRenderContext );
			}

			// Submit
			m_pRenderViews[ RTB_LIGHTING_HIGHRES ].EndRender( pRenderContext );
		}

		// Blur the light buffer
		if ( m_bBlurLighting )
		{
			BlurLightBuffer();
		}

		if ( m_bLightingOnly )
		{
			m_pRenderViews[ RTB_SAMPLE_LIGHTING ].BeginRender( pRenderContext );

			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );

			pRenderContext->BindTexture( 0, m_pRenderTargets[ m_nViewBuffer ] );
			pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );

			if ( m_nViewBuffer == RT_LIGHTING_HIGHRES )
			{
				pRenderContext->BindTexture( 1, m_pRenderTargets[ RT_SPECULAR_HIGHRES ] );
				pRenderContext->SetSamplerStatePS( 1, RS_FILTER_MIN_MAG_MIP_POINT, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP, RS_TEXTURE_ADDRESS_CLAMP );
			}

			pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
			pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

			pRenderContext->SetConstantBufferData( m_hDecalScreenData, &m_decalScreenData, sizeof( DecalScreenData_t ) );
			pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hDecalScreenData, 0, 0 );

			pRenderContext->BindVertexShader( m_hShadowFrustumVS[2], m_hQuadLayout );

			if ( m_nViewBuffer == RT_LIGHTING_LOWRES )
			{
				pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hCopyTexture[2] );
			}
			else if ( m_nViewBuffer == RT_LIGHTING_HIGHRES )
			{
				pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hCopyTexture[3] );
			}
			else
			{
				pRenderContext->BindShader( RENDER_PIXEL_SHADER, m_hCopyTexture[1] );
			}

			RenderScreenQuad( pRenderContext, m_vEyeRays, "lightingonly" );

			// Submit
			m_pRenderViews[ RTB_SAMPLE_LIGHTING ].EndRender( pRenderContext );
		}	
		// Render forward pass while sampling lighting
		else
		{
			Vector4D vForwardClear(0,0,0,0);
			m_pRenderViews[ RTB_SAMPLE_LIGHTING ].Clear( pRenderContext, vForwardClear, 0 );
			
			RenderView( m_pRenderViews[ RTB_SAMPLE_LIGHTING ] );

			// Render spheres
			m_pRenderViews[ RTB_SAMPLE_LIGHTING ].BeginRender( pRenderContext );

			pRenderContext->SetConstantBufferData( m_hViewRelated, (void*)&viewConstants, sizeof( ViewRelatedConstants_t ) );
			pRenderContext->BindConstantBuffer( RENDER_VERTEX_SHADER, m_hViewRelated, 0, 0 );

			int nParticleSystems = m_sphereSystemList.Count();
			
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_EQUAL_NO_WRITE );
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
			
			pRenderContext->BindTexture( 4, m_pRenderTargets[ RT_LIGHTING_HIGHRES ] );
			pRenderContext->SetSamplerStatePS( 4, RS_FILTER_MIN_MAG_MIP_POINT );

			pRenderContext->BindTexture( 5, m_pRenderTargets[ RT_SPECULAR_HIGHRES ] );
			pRenderContext->SetSamplerStatePS( 5, RS_FILTER_MIN_MAG_MIP_POINT );

			for ( int p=0; p<nParticleSystems; ++p )
			{
				RenderParticleLights( pRenderContext, m_sphereSystemList[p], VARIATION_SAMPLE_LIGHTING, false, false );
			}

			// Decals
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_NO_WRITE );
			if ( IsPlatformX360() )
			{
				pRenderContext->BindTexture( 0, m_pRenderTargets[ DS_DEPTH_HIGHRES ] );
			}
			else
			{
				pRenderContext->BindTexture( 0, m_pRenderTargets[ RT_VIEW_DEPTH_HIGHRES ] );
			}
			pRenderContext->SetSamplerStatePS( 0, RS_FILTER_MIN_MAG_MIP_POINT );
			pRenderContext->BindTexture( 2, m_pRenderTargets[ RT_LIGHTING_HIGHRES ] );
			pRenderContext->SetSamplerStatePS( 2, RS_FILTER_MIN_MAG_MIP_POINT );
			pRenderContext->BindTexture( 3, m_pRenderTargets[ RT_SPECULAR_HIGHRES ] );
			pRenderContext->SetSamplerStatePS( 3, RS_FILTER_MIN_MAG_MIP_POINT );
			RenderDecals( pRenderContext, 1 );

			// Render pointlights
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_AND_WRITE );

			nParticleSystems = m_lightSystemList.Count();
			for ( int p=0; p<nParticleSystems; ++p )
			{
				RenderParticleLights( pRenderContext, m_lightSystemList[p], 4, false, false );
			}

			// Submit
			m_pRenderViews[ RTB_SAMPLE_LIGHTING ].EndRender( pRenderContext );
		}
		

		// Render aerial perspective
		if ( m_bDrawAerialPerspective )
		{
			RenderAerialPerspective();
		}
	}
	else
	{
		VPROF_SCOPE_BEGIN("RenderWorldNodes");

		Vector4D c(0,1,0,0);
		m_pRenderViews[ RTB_FORWARD ].Clear( pRenderContext, c, RENDER_CLEAR_FLAGS_CLEAR_DEPTH );

		RenderView( m_pRenderViews[ RTB_FORWARD ] );

		// Submit
		m_pRenderViews[ RTB_FORWARD ].EndRender( pRenderContext );

		VPROF_SCOPE_END();
	}

	if ( m_bRenderSky )
	{
		RenderSky();
	}

	// Unbind the render targets
	for ( int s=0; s<NUM_SHADOW_SPLITS; ++s )
	{
		pRenderContext->BindTexture( 4 + s, RENDER_TEXTURE_HANDLE_INVALID );
	}
	pRenderContext->Submit();

	for ( int v=0; v<MAX_VIEW_BINDING_TYPES; ++v )
	{
		m_pRenderViews[ v ].EndRender( NULL );
	}

	// Update resources phase (arbitrarily only allow 10 resources to be updated)
	m_pWorldRenderer->UpdateResources( g_pRenderDevice, pRenderContext, 10 );

	m_nGlobalDecalPos3 = 0;
	m_nCurrentFrameNumber++;
}


void CWorldRendererTest::SimulateFrame( float flTimeStep )
{
	UpdateDroppedRenderables( flTimeStep );

	const float forwardSpeed = 500.0f;
	const float sideSpeed = 500.0f;
	const float yawSpeed = 90.0f;
	const float pitchSpeed = 45.0f;

#if defined( _X360 )
	const float flYawSensitivity = -0.04f;
	const float flPitchSensitivity = 0.04f;
#else
	const float flYawSensitivity = -0.2f;
	const float flPitchSensitivity = 0.2f;
#endif

	float flForward = forwardSpeed;
	float flSide = sideSpeed;
	if ( m_cmd.m_nKeys & FKEY_FAST )
	{
		flForward *= 5;
		flSide *= 5;
	}

	if ( m_cmd.m_nKeys & FKEY_ENABLE_VIS )
	{
		m_bEnableVis = true;
		Msg( "Vis enabled\n" );
	}
	else if( m_cmd.m_nKeys & FKEY_DISABLE_VIS )
	{
		m_bEnableVis = false;
		Msg( "Vis disabled\n" );
	}

	if ( m_cmd.m_nKeys & FKEY_DROP_FRUSTUM )
	{
		Q_memcpy( m_pDropFrustum, m_pFrustum, sizeof( CWorldRenderFrustum ) );
		m_bDropFrustum = !m_bDropFrustum;
		m_vCullPos = m_pFrustum->GetCameraPosition();
		if ( m_bDropFrustum )
			Msg( "Drop frustum\n" );
		else
			Msg( "no drop\n" );
		ThreadSleep( 200 );
	}

	if ( m_cmd.m_nKeys & FKEY_TOGGLE_AERIAL )
	{
		m_bDrawAerialPerspective = !m_bDrawAerialPerspective;
		if ( m_bDrawAerialPerspective )
			Msg( "Aerial Perspective: On\n" );
		else
			Msg( "Aerial Perspective: Off\n" );
		ThreadSleep( 200 );
	}

	if ( m_cmd.m_nKeys & FKEY_TOGGLE_BOUNCE )
	{
		m_bDrawBouncedLight = !m_bDrawBouncedLight;
		if ( m_bDrawBouncedLight )
			Msg( "Bounced light: On\n" );
		else
			Msg( "Bounced light: Off\n" );
		ThreadSleep( 200 );
	}

	if ( m_cmd.m_nKeys & FKEY_TOGGLE_DIRECT )
	{
		m_bDrawDirectLight = !m_bDrawDirectLight;
		if ( m_bDrawDirectLight )
			Msg( "Direct light: On\n" );
		else
			Msg( "Direct light: Off\n" );
		ThreadSleep( 200 );
	}

	if ( m_cmd.m_nKeys & FKEY_TOGGLE_LIGHTING_ONLY )
	{
		m_bLightingOnly = !m_bLightingOnly;
		if ( m_bLightingOnly )
			Msg( "Lighting Only: On\n" );
		else
			Msg( "Lighting Only: Off\n" );
		ThreadSleep( 200 );
	}

	if ( m_cmd.m_nKeys & FKEY_SSAO )
	{
		m_bSSAO = !m_bSSAO;
		if ( m_bSSAO )
			Msg( "SSAO: On\n" );
		else
			Msg( "SSAO: Off\n" );
		ThreadSleep( 200 );
	}

	if ( m_cmd.m_nKeys & FKEY_BLUR_LIGHTING )
	{
		m_bBlurLighting = !m_bBlurLighting;
		if ( m_bBlurLighting )
			Msg( "Blur Lighting: On\n" );
		else
			Msg( "Blur Lighting: Off\n" );
		ThreadSleep( 200 );
	}

	if ( m_cmd.m_nKeys & FKEY_BLUR_SHADOWS )
	{
		m_bBlurShadows = !m_bBlurShadows;
		if ( m_bBlurShadows )
			Msg( "Blur Shadows: On\n" );
		else
			Msg( "Blur Shadows: Off\n" );
		ThreadSleep( 200 );
	}

	m_cmd.m_flForwardMove = (m_cmd.m_nKeys & (FKEY_FORWARD|FKEY_BACK)) ? ( (m_cmd.m_nKeys & FKEY_FORWARD) ? flForward : -flForward) : 0;
	m_cmd.m_flRightMove = (m_cmd.m_nKeys & (FKEY_LEFT|FKEY_RIGHT)) ? ( (m_cmd.m_nKeys & FKEY_RIGHT) ? flSide : -flSide) : 0;
	float flYawMove = (m_cmd.m_nKeys & (FKEY_TURNLEFT|FKEY_TURNRIGHT)) ? ( (m_cmd.m_nKeys & FKEY_TURNLEFT) ? yawSpeed : -yawSpeed) : 0;
	float flPitchMove = (m_cmd.m_nKeys & (FKEY_TURNUP|FKEY_TURNDOWN)) ? ( (m_cmd.m_nKeys & FKEY_TURNDOWN) ? pitchSpeed : -pitchSpeed) : 0;
	
	if ( m_nCursorDeltaX || m_nCursorDeltaY )
	{
		m_cmd.m_flYawMove = m_nCursorDeltaX * flYawSensitivity;
		m_cmd.m_flPitchMove = m_nCursorDeltaY * flPitchSensitivity;
		m_nCursorDeltaX = m_nCursorDeltaResetX;
		m_nCursorDeltaY = m_nCursorDeltaResetY;
		if ( !IsPlatformX360() )
		{
			g_pInputStackSystem->SetCursorPosition( GetInputContext(), m_nWindowCenterX, m_nWindowCenterY );
		}
	}
	m_cmd.m_flPitchMove += flPitchMove * flTimeStep;
	m_cmd.m_flYawMove += flYawMove * flTimeStep;

	Vector vMoveAmount = ((m_pFrustum->Forward() * m_cmd.m_flForwardMove ) - ( m_pFrustum->Left() * m_cmd.m_flRightMove )) * flTimeStep;
	Vector eyePos = m_pFrustum->GetCameraPosition() + vMoveAmount;
	m_pFrustum->SetCameraPosition( eyePos );
	QAngle angles = m_pFrustum->GetCameraAngles();
	angles.y += m_cmd.m_flYawMove;
	angles.x += m_cmd.m_flPitchMove;
	angles.x = clamp( angles.x, -89.0f, 89.0f );
	m_cmd.m_flYawMove = 0;
	m_cmd.m_flPitchMove = 0;

	if ( angles.y > 180.0f )
	{
		angles.y -= 360.0f;
	}
	if ( angles.y < -180.0f )
	{
		angles.y += 360.0f;
	}
	m_pFrustum->SetCameraAngles( angles );

	// Update light
	VMatrix mLight,mLightAngle, mLightElevation;
	mLightAngle.Identity();
	mLightElevation.Identity();
	mLight.Identity();
	MatrixRotate( mLightElevation, Vector(1,0,0), m_vLightAngleElevation.y );
	MatrixRotate( mLightAngle, Vector(0,0,1), m_vLightAngleElevation.x );
	mLight = mLightElevation * mLightAngle;
	Vector vLightStart(1,0,0);
	matrix3x4_t mLight3x4 = mLight.As3x4();
	VectorRotate( vLightStart, mLight3x4, m_vLightDir );
	m_vLightDir.NormalizeInPlace();

	float flLerp = sinf( DEG2RAD( m_vLightAngleElevation.x ) );
	Vector4D vColorRed( 1.0, 0.7, 0.6, 1 );
	Vector4D vColorYellow( 1.0, 0.9, 0.7, 1 );
	m_skyData.m_vLightColor = Lerp( flLerp, vColorRed, vColorYellow );

	Vector4D vRenderColorRed( 1.0, 0.6, 0.4, 1 );
	Vector4D vRenderColorYellow( 1.0, 0.9, 0.7, 1 );
	m_vRenderLightColor = Lerp( flLerp, vRenderColorRed, vRenderColorYellow );

	Vector4D vBmRed( 1.0e-4, 0.4e-4, 0.3e-4, 1 );
	Vector4D vBmYellow( 1.0e-4, 0.7e-4, 0.3e-4, 1 );
	m_skyData.m_vBm = Lerp( flLerp, vBmRed, vBmYellow );

	float flGRed = 0.90f;
	float flGYellow = 0.99f;
	m_skyData.m_flG.x = Lerp( flLerp, flGRed, flGYellow );

	if ( m_cmd.m_nKeys & FKEY_LIGHTANG_POS )
	{
		m_vLightAngleElevation.x += m_flLightAngSpeed * flTimeStep;
	}
	else if ( m_cmd.m_nKeys & FKEY_LIGHTANG_NEG )
	{
		m_vLightAngleElevation.x -= m_flLightAngSpeed * flTimeStep;
	}

	if ( m_cmd.m_nKeys & FKEY_LIGHTELV )
	{
		m_vLightAngleElevation.y += m_flLightElvSpeed * flTimeStep;
		if ( m_vLightAngleElevation.y > 88.0f || m_vLightAngleElevation.y < 30.0f )
		{
			m_flLightElvSpeed = -m_flLightElvSpeed;
		}
	}

	// Shooting decals
	if ( m_cmd.m_nKeys & FKEY_SHOOTDECAL )
	{
		if ( m_flDecalCountdown <= 0 )
		{
			Vector vEye = m_pFrustum->GetCameraPosition();
			Vector vDir = m_pFrustum->Forward();
			Vector vSurfaceNormal;
			float flDistance = m_pWorldRenderer->CastRay( &vSurfaceNormal, vEye, vDir );
			
			if ( flDistance > 0 )
			{
				float flSize = RPercentABS() * 40.0f + 70.0f;
				CRenderContextPtr pRenderContext( g_pRenderDevice );
				Vector vPos = vEye + vDir * flDistance;
				AddDecal( pRenderContext, vPos, vSurfaceNormal, 
						  flSize, flSize, //size x,y
						  6.0f, 12.0f, false ); // intersection, length
			}
			m_flDecalCountdown = 0.1f;
		}
		m_flDecalCountdown -= flTimeStep;
	}

	// Shooting blood decals
	if ( m_cmd.m_nKeys & FKEY_SHOOTDECAL2 )
	{
		if ( m_flDecalCountdown <= 0 )
		{
			Vector vEye = m_pFrustum->GetCameraPosition();
			Vector vDir = m_pFrustum->Forward();
			Vector vSurfaceNormal;
			float flDistance = m_pWorldRenderer->CastRay( &vSurfaceNormal, vEye, vDir );
			
			if ( flDistance > 0 )
			{
				float flSize = RPercentABS() * 50.0f + 80.0f;
				CRenderContextPtr pRenderContext( g_pRenderDevice );
				Vector vPos = vEye + vDir * flDistance;
				AddDecal( pRenderContext, vPos, vSurfaceNormal, 
						  flSize, flSize, //size x,y
						  20.0f, 40.0f, true ); // intersection, length
			}
			m_flDecalCountdown = 0.05f;
		}
		m_flDecalCountdown -= flTimeStep;
	}

	// Shooting the wall-monster decal
	if ( m_cmd.m_nKeys & FKEY_SHOOTWALLMONSTER )
	{
		Vector vEye = m_pFrustum->GetCameraPosition();
		Vector vDir = m_pFrustum->Forward();
		Vector vSurfaceNormal;
		float flDistance = m_pWorldRenderer->CastRay( &vSurfaceNormal, vEye, vDir );
		
		if ( flDistance > 0 )
		{
			float flSize = 100.0f;
			CRenderContextPtr pRenderContext( g_pRenderDevice );
			Vector vPos = vEye + vDir * flDistance;

			Vector vMonsterNormal = m_vPreviousMonsterNormal + vSurfaceNormal * 4;
			vMonsterNormal.NormalizeInPlace();
			AddWallMonster( pRenderContext, vPos, vMonsterNormal, 
					  flSize, flSize, //size x,y
					  50.0f, 100.0f ); // intersection, length

			m_vPreviousMonsterNormal = vMonsterNormal;

			if ( m_flWallMonsterTimer < 0 )
			{
				m_nWallMonsterTexture = ( m_nWallMonsterTexture + 1 ) % 3;
				m_flWallMonsterTimer = 0.05f;
			}
			else
			{
				m_flWallMonsterTimer -= MAX( 0.01f, flTimeStep );
			}
		}
	}

	if ( !( m_cmd.m_nKeys & FKEY_SHOOTDECAL || m_cmd.m_nKeys & FKEY_SHOOTDECAL2 ) )
	{
		m_flDecalCountdown = 0.0f;
	}

	// shoot a particle system
	if ( m_cmd.m_nKeys & FKEY_SHOOTPSYSTEM || m_cmd.m_nKeys & FKEY_SHOOTSSYSTEM )
	{
		if ( m_flParticleCountdown <= 0 )
		{
			bool bSphereSystem = ( m_cmd.m_nKeys & FKEY_SHOOTSSYSTEM ) != 0;
			Vector vEye = m_pFrustum->GetCameraPosition();
			Vector vDir = m_pFrustum->Forward();
			Vector vSurfaceNormal;

			float flDistance = m_pWorldRenderer->CastRay( &vSurfaceNormal, vEye, vDir );
				
			if ( flDistance > 0 )
			{
				Msg( "psystem distance = %f\n", flDistance );

				CParticleCollection *pParticleSystem = g_pSceneSystem->CreateParticleCollection( "wonderinglights" );
				if ( !pParticleSystem )
					Error( "No wondering lights particle system!\n" );

				Vector vPos = vEye + vDir * flDistance;

				vPos += vSurfaceNormal * 100;
				pParticleSystem->SetControlPoint( 0, vPos );
				pParticleSystem->SetControlPointOrientation( 0, Vector(1,0,0), Vector(0,1,0), Vector(0,0,1) );

				if ( bSphereSystem )
					m_sphereSystemList.AddToTail( pParticleSystem );
				else
					m_lightSystemList.AddToTail( pParticleSystem );

				SimulateWorkUnitWorldRender_t workUnit;
				workUnit.m_nNumSystems = 1;
				workUnit.m_pParticles = pParticleSystem;
				m_particleWorkUnits.AddToTail( workUnit );
			}
			m_flParticleCountdown = 0.5f;
		}
		m_flParticleCountdown -= flTimeStep;
	}
	else
	{
		m_flParticleCountdown = 0.0f;
	}


	// Sim particles
	SimulateParticles( flTimeStep );
}

void CWorldRendererTest::AddDecal( IRenderContext *pRenderContext, Vector &vHit, Vector &vDecalDir, float flDecalSizeU, float flDecalSizeV, float flDecalShift, float flDecalInfluenceLength, bool bPaint )
{
	if ( g_nPlatform == RST_PLATFORM_GL )
		return;

	Vector vUp(RPercent(),RPercent(),RPercent());
	vUp.NormalizeInPlace();

	Vector vNegUp = -vUp;
	if ( DotProduct( vDecalDir, vUp ) > 0.95f || DotProduct( vDecalDir, vNegUp ) > 0.95f  )
		vUp = Vector(0,1,0);

	Vector vRight;
	vRight = CrossProduct( vUp, vDecalDir );
	vUp = CrossProduct( vDecalDir, vRight );
	vRight.NormalizeInPlace();
	vUp.NormalizeInPlace();
	vRight *= flDecalSizeU;
	vUp *= flDecalSizeV;
	vDecalDir.NormalizeInPlace();

	Vector vStartCorner = vHit;
	vStartCorner -= vRight * 0.5f;
	vStartCorner -= vUp * 0.5f;
	vStartCorner -= vDecalDir * flDecalShift;
	vDecalDir *= flDecalInfluenceLength;

	// Create the 8 corners of a box surrounding our decal
	Vector vCorners[8];
	vCorners[0] = vStartCorner;
	vCorners[1] = vStartCorner + vRight;
	vCorners[2] = vStartCorner + vRight + vUp;
	vCorners[3] = vStartCorner + vUp;

	vCorners[4] = vStartCorner + vDecalDir;
	vCorners[5] = vCorners[4] + vRight;
	vCorners[6] = vCorners[4] + vRight + vUp;
	vCorners[7] = vCorners[4] + vUp;

	float fRMul = 1.0f / ( flDecalSizeU * flDecalSizeU );
	float fUMul = 1.0f / ( flDecalSizeV * flDecalSizeV );
	vRight = fRMul * vRight;
	vUp = fUMul * vUp;

	if ( bPaint )
	{
		DecalVertex_t *pVertices = &m_pDecalBackingStore2[ m_nGlobalDecalPos2 * 8 ];
		for ( int i=0; i<8; ++i )
		{
			pVertices[i].m_vPos = vCorners[i];
			pVertices[i].m_vDecalRight = vRight;
			pVertices[i].m_vDecalUpAndInfluence = Vector4D( vUp.x, vUp.y, vUp.z, flDecalInfluenceLength );
			pVertices[i].m_vDecalOrigin = vStartCorner;
		}

		LockDesc_t lockDesc;
		pRenderContext->LockVertexBuffer( m_hDecalVB2, m_nMaxDecals * 8 * sizeof( DecalVertex_t ), &lockDesc );
		Q_memcpy( lockDesc.m_pMemory, m_pDecalBackingStore2, m_nMaxDecals * 8 * sizeof( DecalVertex_t ) );
		pRenderContext->UnlockVertexBuffer( m_hDecalVB2, m_nMaxDecals * 8 * sizeof( DecalVertex_t ), &lockDesc );

		// Go to the next decal
		m_nGlobalDecalPos2 ++;
		if ( m_nGlobalDecalPos2 >= m_nMaxDecals )
		{
			m_bFullBuffer2 = true;
			m_nGlobalDecalPos2 = 0;
		}
	}
	else
	{
		DecalVertex_t *pVertices = &m_pDecalBackingStore[ m_nGlobalDecalPos * 8 ];
		for ( int i=0; i<8; ++i )
		{
			pVertices[i].m_vPos = vCorners[i];
			pVertices[i].m_vDecalRight = vRight;
			pVertices[i].m_vDecalUpAndInfluence = Vector4D( vUp.x, vUp.y, vUp.z, flDecalInfluenceLength );
			pVertices[i].m_vDecalOrigin = vStartCorner;
		}

		LockDesc_t lockDesc;
		pRenderContext->LockVertexBuffer( m_hDecalVB, m_nMaxDecals * 8 * sizeof( DecalVertex_t ), &lockDesc );
		Q_memcpy( lockDesc.m_pMemory, m_pDecalBackingStore, m_nMaxDecals * 8 * sizeof( DecalVertex_t ) );
		pRenderContext->UnlockVertexBuffer( m_hDecalVB, m_nMaxDecals * 8 * sizeof( DecalVertex_t ), &lockDesc );

		// Go to the next decal
		m_nGlobalDecalPos ++;
		if ( m_nGlobalDecalPos >= m_nMaxDecals )
		{
			m_bFullBuffer = true;
			m_nGlobalDecalPos = 0;
		}
	}
}

void CWorldRendererTest::AddWallMonster( IRenderContext *pRenderContext, Vector &vHit, Vector &vDecalDir, float flDecalSizeU, float flDecalSizeV, float flDecalShift, float flDecalInfluenceLength )
{
	if ( g_nPlatform == RST_PLATFORM_GL )
		return;

	Vector vUp(0,0,1);
	Vector vNegUp = -vUp;
	if ( DotProduct( vDecalDir, vUp ) > 0.95f || DotProduct( vDecalDir, vNegUp ) > 0.95f  )
		vUp = m_pFrustum->Up();

	Vector vRight;
	vRight = CrossProduct( vUp, vDecalDir );
	vUp = CrossProduct( vDecalDir, vRight );
	vRight.NormalizeInPlace();
	vUp.NormalizeInPlace();
	vRight *= flDecalSizeU;
	vUp *= flDecalSizeV;
	vDecalDir.NormalizeInPlace();

	Vector vStartCorner = vHit;
	vStartCorner -= vRight * 0.5f;
	vStartCorner -= vUp * 0.5f;
	vStartCorner -= vDecalDir * flDecalShift;
	vDecalDir *= flDecalInfluenceLength;

	// Create the 8 corners of a box surrounding our decal
	Vector vCorners[8];
	vCorners[0] = vStartCorner;
	vCorners[1] = vStartCorner + vRight;
	vCorners[2] = vStartCorner + vRight + vUp;
	vCorners[3] = vStartCorner + vUp;

	vCorners[4] = vStartCorner + vDecalDir;
	vCorners[5] = vCorners[4] + vRight;
	vCorners[6] = vCorners[4] + vRight + vUp;
	vCorners[7] = vCorners[4] + vUp;

	float fRMul = 1.0f / ( flDecalSizeU * flDecalSizeU );
	float fUMul = 1.0f / ( flDecalSizeV * flDecalSizeV );
	vRight = fRMul * vRight;
	vUp = fUMul * vUp;

	DecalVertex_t pVertices[8];
	for ( int i=0; i<8; ++i )
	{
		pVertices[i].m_vPos = vCorners[i];
		pVertices[i].m_vDecalRight = vRight;
		pVertices[i].m_vDecalUpAndInfluence = Vector4D( vUp.x, vUp.y, vUp.z, flDecalInfluenceLength );
		pVertices[i].m_vDecalOrigin = vStartCorner;
	}

	LockDesc_t lockDesc;
	pRenderContext->LockVertexBuffer( m_hDecalVB3, 8 * sizeof( DecalVertex_t ), &lockDesc );
	Q_memcpy( lockDesc.m_pMemory, pVertices, 8 * sizeof( DecalVertex_t ) );
	pRenderContext->UnlockVertexBuffer( m_hDecalVB3, 8 * sizeof( DecalVertex_t ), &lockDesc );

	// Go to the next decal
	m_nGlobalDecalPos3 = 1;
}

void CWorldRendererTest::DropRenderable()
{
	Vector vOrigin = m_pFrustum->GetCameraPosition();
	vOrigin.z -= 32.0f;

	int index = m_dynRenderableList.AddToTail();

	VMatrix mRot;
	Vector vAxis( 0, 0, 1 );
	MatrixBuildRotationAboutAxis( mRot, vAxis, 90.0f );
	vAxis.Init( 1, 0, 0 );
	MatrixRotate( mRot, vAxis, 90.0f );

	m_dynRenderableList[ index ].m_vOrigin = vOrigin;
	m_dynRenderableList[ index ].m_mWorld = mRot;
	m_dynRenderableList[ index ].m_hRenderable = g_pMeshSystem->FindOrCreateFileRenderable( g_pRenderableList[ g_nCurrentModel ] );
	g_nCurrentModel ++;
	if ( g_nCurrentModel >= MAX_MESH_SYSTEM_MODELS )
		g_nCurrentModel = 0;
}

Vector Evaluate( float flU, float flV, Vector vRadius )
{
	flU *= 2.0 * M_PI;
	flV *= 2.0 * M_PI;
	return Vector( 
		vRadius.x * cos( flU ) * sin( flV ),
		vRadius.y * sin( flU ) * sin( flV ),
		vRadius.z * cos ( flV ) );
}

void CWorldRendererTest::CreateSphere( int nTessellationRes )
{
	Vector vRadius(1,1,1);
	CRenderContextPtr pRenderContext( g_pRenderDevice );

	// create the static index buffer
	BufferDesc_t indexDesc;
	indexDesc.m_nElementSizeInBytes = sizeof(uint16);
	indexDesc.m_nElementCount = 2 * 3 * ( nTessellationRes * nTessellationRes );
	indexDesc.m_pDebugName = "sphereib";
	indexDesc.m_pBudgetGroupName = "shapes";
	m_hSphereIB = g_pRenderDevice->CreateIndexBuffer( RENDER_BUFFER_TYPE_STATIC, indexDesc );

	// fill in the index buffer
	CIndexData<uint16> ib( pRenderContext, m_hSphereIB );
	ib.Lock( indexDesc.m_nElementCount );
	m_nNumTriangles = 0;
	// generate the fixed tesselation
	for( int u = 0; u < nTessellationRes; u += 1 )
	{
		int nu = ( u + 1 ) % nTessellationRes;
		for( int v = 0; v < nTessellationRes; v++ )
		{
			int nv = ( v + 1 ) % nTessellationRes;
			ib.Index2( u * nTessellationRes + v, nu * nTessellationRes + v );
			ib.Index2( u * nTessellationRes + nv, nu * nTessellationRes + v);
			ib.Index2( nu * nTessellationRes + nv, u * nTessellationRes + nv );
			m_nNumTriangles += 2;
		}
	}
	ib.Unlock( );

	m_nSphereIndices = indexDesc.m_nElementCount;

	// VB
	int nVertices = ( nTessellationRes ) * ( nTessellationRes );

	BufferDesc_t vertexDesc;
	vertexDesc.m_nElementSizeInBytes = sizeof(SphereVertex_t);
	vertexDesc.m_nElementCount = nVertices;
	vertexDesc.m_pDebugName = "spherevb";
	vertexDesc.m_pBudgetGroupName = "shapes";
	m_hSphereVB = g_pRenderDevice->CreateVertexBuffer( RENDER_BUFFER_TYPE_STATIC, vertexDesc );

	CVertexData<SphereVertex_t> vb( pRenderContext, m_hSphereVB );
	vb.Lock( nVertices );

	for( int u = 0 ; u < nTessellationRes; u++ )
	{
		float flU = u * ( 1.0 / nTessellationRes );
		for( int v = 0; v < nTessellationRes; v++ )
		{
			float flV = v * ( 1.0 / nTessellationRes );
			Vector pos1 = Evaluate( flU + 0.5 / nTessellationRes, flV, vRadius );
			vb->m_vPos = pos1;
			vb.AdvanceVertex();
		}
	}
	vb.Unlock( );

	static RenderInputLayoutField_t sphereLayout[] =
	{
		DEFINE_PER_VERTEX_FIELD( 0, "position", 0, SphereVertex_t, m_vPos )
	};

	m_hSphereLayout = g_pRenderDevice->CreateInputLayout( "spherelayout", ARRAYSIZE( sphereLayout ), sphereLayout );

	pRenderContext->Submit();
}


void CWorldRendererTest::CreateHemi( int nTessellationRes )
{
	Vector vRadius(1,1,1);
	CRenderContextPtr pRenderContext( g_pRenderDevice );

	// create the static index buffer
	BufferDesc_t indexDesc;
	indexDesc.m_nElementSizeInBytes = sizeof(uint16);
	indexDesc.m_nElementCount = 2 * 3 * ( nTessellationRes * nTessellationRes );
	indexDesc.m_pDebugName = "hemiIB";
	indexDesc.m_pBudgetGroupName = "shapes";
	m_hHemiIB = g_pRenderDevice->CreateIndexBuffer( RENDER_BUFFER_TYPE_STATIC, indexDesc );

	// fill in the index buffer
	CIndexData<uint16> ib( pRenderContext, m_hHemiIB );
	ib.Lock( indexDesc.m_nElementCount );
	// generate the fixed tesselation
	for( int u = 0; u < nTessellationRes; u += 1 )
	{
		int nu = ( u + 1 ) % nTessellationRes;
		for( int v = 0; v < nTessellationRes; v++ )
		{
			int nv = ( v + 1 ) % nTessellationRes;
			ib.Index2( u * nTessellationRes + v, nu * nTessellationRes + v );
			ib.Index2( u * nTessellationRes + nv, nu * nTessellationRes + v);
			ib.Index2( nu * nTessellationRes + nv, u * nTessellationRes + nv );

		}
	}
	ib.Unlock( );

	m_nHemiIndices = indexDesc.m_nElementCount;

	// VB
	int nVertices = ( nTessellationRes ) * ( nTessellationRes );

	BufferDesc_t vertexDesc;
	vertexDesc.m_nElementSizeInBytes = sizeof(SphereVertex_t);
	vertexDesc.m_nElementCount = nVertices;
	vertexDesc.m_pDebugName = "hemiVB";
	vertexDesc.m_pBudgetGroupName = "shapes";
	m_hHemiVB = g_pRenderDevice->CreateVertexBuffer( RENDER_BUFFER_TYPE_STATIC, vertexDesc );

	CVertexData<SphereVertex_t> vb( pRenderContext, m_hHemiVB );
	vb.Lock( nVertices );

	for( int u = 0 ; u < nTessellationRes; u++ )
	{
		float flU = u * ( 1.0 / nTessellationRes );
		for( int v = 0; v < nTessellationRes; v++ )
		{
			float flV = v * ( 1.0 / nTessellationRes );
			Vector pos1 = Evaluate( flU + 0.5 / nTessellationRes, flV, vRadius );
			if ( pos1.z < 0 )
				pos1.z = 0;
			vb->m_vPos = pos1;
			vb.AdvanceVertex();
		}
	}
	vb.Unlock( );

	pRenderContext->Submit();
}

void CWorldRendererTest::CreateCone( int nTessellationRes )
{
	CRenderContextPtr pRenderContext( g_pRenderDevice );

	Vector vTop(0,0,0);

	int nVertices = nTessellationRes + 1;
	int nIndices = ( nTessellationRes + nTessellationRes - 2 ) * 3;

	// create the static index buffer
	BufferDesc_t indexDesc;
	indexDesc.m_nElementSizeInBytes = sizeof(uint16);
	indexDesc.m_nElementCount = nIndices;
	indexDesc.m_pDebugName = "coneib";
	indexDesc.m_pBudgetGroupName = "shapes";
	m_hConeIB = g_pRenderDevice->CreateIndexBuffer( RENDER_BUFFER_TYPE_STATIC, indexDesc );

	// create the static vertex buffer
	BufferDesc_t vertexDesc;
	vertexDesc.m_nElementSizeInBytes = sizeof(SphereVertex_t);
	vertexDesc.m_nElementCount = nVertices;
	vertexDesc.m_pDebugName = "conevb";
	vertexDesc.m_pBudgetGroupName = "shapes";
	m_hConeVB = g_pRenderDevice->CreateVertexBuffer( RENDER_BUFFER_TYPE_STATIC, vertexDesc );

	m_nConeIndices = nIndices;

	CVertexData<SphereVertex_t> vb( pRenderContext, m_hConeVB );
	vb.Lock( nVertices );

	vb->m_vPos = vTop;
	vb.AdvanceVertex();

	float flTesRes = (float)nTessellationRes;
	for ( int i=0; i<nTessellationRes; ++i )
	{
		float flU = ( i / flTesRes ) * (2.0 * M_PI);

		Vector vPos = Vector( 1.1f * cos( flU ), 1.1f * sin( flU ), 1 );

		vb->m_vPos = vPos;
		vb.AdvanceVertex();
	}

	vb.Unlock();

	CIndexData<uint16> ib( pRenderContext, m_hConeIB );
	ib.Lock( indexDesc.m_nElementCount );

	// Cone
	for ( int i=1; i<nTessellationRes+1; ++i )
	{
		ib.Index( 0 );
		ib.Index( i );
		ib.Index( ( i % nTessellationRes ) + 1 );
	}

	// Cap
	for ( int i=2; i<nTessellationRes; ++i )
	{
		ib.Index( ( i + 1 ) );
		ib.Index( i );
		ib.Index( 1 );
	}

	ib.Unlock();
}

bool LoadFile( char *pFileName, CUtlBuffer &buffer, const char *pShaderVersion )
{
	Assert( pFileName && pShaderVersion );

	char pLocalFileName[260];
	Q_snprintf( pLocalFileName, 260, "%s.%s", pFileName, pShaderVersion );

	if ( !g_pFullFileSystem->ReadFile( pLocalFileName, NULL, buffer ) )
	{
		Error( "Couldn't load %s!\n", pFileName );
		return false;
	}
	return true;
}

RenderShaderHandle_t CWorldRendererTest::GetVertexShader( char *pShaderName, IRenderDevice *pDevice, const char *pShaderVersion, const char *pDefineText )
{
	CUtlBuffer buffer;

	char pLevelText[100];
	if ( 0 == Q_strncmp( pShaderVersion, "vs_3_0", 6 ) ||
		 0 == Q_strncmp( pShaderVersion, "ps_3_0", 6 ) )
		Q_snprintf( pLevelText, 100, "#define SHADER_LEVEL 3\n" );
	else
		Q_snprintf( pLevelText, 100, "#define SHADER_LEVEL 4\n" );
	buffer.Put( pLevelText, strlen( pLevelText ) );

	if ( pDefineText )
		buffer.Put( pDefineText, strlen( pDefineText ) );

	if ( !LoadFile( pShaderName, buffer, "vs_3_0" ) )
	{
		return RENDER_SHADER_HANDLE_INVALID;
	}

	char *pText = (char*)buffer.Base();
	int nBytes = buffer.TellPut();
	if ( nBytes < 1 )
		return RENDER_SHADER_HANDLE_INVALID;

	// Create the shader
	return pDevice->CreateShader( RENDER_VERTEX_SHADER, pText, nBytes, pShaderVersion );
}

RenderShaderHandle_t CWorldRendererTest::GetPixelShader( char *pShaderName, IRenderDevice *pDevice, const char *pShaderVersion, const char *pDefineText )
{
	CUtlBuffer buffer;

	char pLevelText[100];
	if ( 0 == Q_strncmp( pShaderVersion, "ps_3_0", 6 ) )
		Q_snprintf( pLevelText, 100, "#define SHADER_LEVEL 3\n" );
	else
		Q_snprintf( pLevelText, 100, "#define SHADER_LEVEL 4\n" );
	buffer.Put( pLevelText, strlen( pLevelText ) );

	if ( pDefineText )
		buffer.Put( pDefineText, strlen( pDefineText ) );

	if ( !LoadFile( pShaderName, buffer, "ps_3_0" ) )
	{
		return RENDER_SHADER_HANDLE_INVALID;
	}

	char *pText = (char*)buffer.Base();
	int nBytes = buffer.TellPut();
	if ( nBytes < 1 )
		return RENDER_SHADER_HANDLE_INVALID;

	// Create the shader
	return pDevice->CreateShader( RENDER_PIXEL_SHADER, pText, nBytes, pShaderVersion );
}


intp g_nMagicNumber = 1234567;
HRenderTexture CWorldRendererTest::CreateRenderTarget( RenderTargetType_t type, const RenderViewport_t &viewport )
{
	HRenderTexture hRetHandle = RENDER_TEXTURE_HANDLE_INVALID;
	TextureHeader_t spec;
	
	spec.m_nNumMipLevels = 1;
	spec.m_nDepth = 1;
	spec.m_Reflectivity.Init( 1, 1, 1 );

	int nLowResWidth = (int)( viewport.m_nWidth * m_flLowResBufferScale );
	int nLowResHeight = (int)( viewport.m_nHeight * m_flLowResBufferScale );

	ImageFormat ssaoFormat = IMAGE_FORMAT_A8;
	if ( g_nPlatform == RST_PLATFORM_DX11 )
		ssaoFormat = IMAGE_FORMAT_RGBA8888;

	switch ( type )
	{
	case DS_SHADOW_DEPTH0:
	case DS_SHADOW_DEPTH1:
	case DS_SHADOW_DEPTH2:
		{
			spec.m_nWidth = m_nShadowSize;
			spec.m_nHeight = m_nShadowSize;
			spec.m_nFlags = TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_D24X8/*IMAGE_FORMAT_D16*/;
		}
		break;

	case DS_SHADOW_DEPTH_MULTIPLE:
		{
			spec.m_nWidth = m_nShadowSize;
			spec.m_nHeight = m_nShadowSize;
			spec.m_nFlags = TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_D24X8/*IMAGE_FORMAT_D16*/;
		}
		break;

	case RT_SHADOW_MULTIPLE_0:
	case RT_SHADOW_MULTIPLE_1:
	case RT_SHADOW_BLUR_TEMP:
		{
			spec.m_nWidth = m_nShadowSize;
			spec.m_nHeight = m_nShadowSize;
			spec.m_nFlags = TSPEC_RENDER_TARGET;
			spec.m_nImageFormat = IMAGE_FORMAT_RG1616F;
		}
		break;

	case DS_DEPTH_HIGHRES:
		{
			spec.m_nWidth = viewport.m_nWidth;
			spec.m_nHeight = viewport.m_nHeight;
			spec.m_nFlags = TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_D24S8;
		}
		break;
	case RT_VIEW_DEPTH_HIGHRES:
		{
			spec.m_nWidth = viewport.m_nWidth;
			spec.m_nHeight = viewport.m_nHeight;
			spec.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_R32F;
		}
		break;
	case RT_VIEW_NORMAL_HIGHRES:
		{
			spec.m_nWidth = viewport.m_nWidth;
			spec.m_nHeight = viewport.m_nHeight;
			spec.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_RGBA1010102;
		}
		break;
	case RT_VIEW_SPEC_HIGHRES:
		{
			spec.m_nWidth = viewport.m_nWidth;
			spec.m_nHeight = viewport.m_nHeight;
			spec.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_BGRA8888;
			if ( g_nPlatform == RST_PLATFORM_DX11 )
			{
				spec.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
			}
		}
		break;
	case RT_LIGHTING_HIGHRES:
		{
			spec.m_nWidth = viewport.m_nWidth;
			spec.m_nHeight = viewport.m_nHeight;
			spec.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_RGBA16161616F;
			/*
			if ( g_nPlatform == RST_PLATFORM_DX11 )
			{
				spec.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
			}
			*/
		}
		break;
	case RT_SPECULAR_HIGHRES:
		{
			spec.m_nWidth = viewport.m_nWidth;
			spec.m_nHeight = viewport.m_nHeight;
			spec.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_BGRA8888;
			if ( g_nPlatform == RST_PLATFORM_DX11 )
			{
				spec.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
			}
		}
		break;
	case DS_DEPTH_LOWRES:
		{
			spec.m_nWidth = nLowResWidth;
			spec.m_nHeight = nLowResHeight;
			spec.m_nFlags = 0;
			spec.m_nImageFormat = IMAGE_FORMAT_D24S8;
		}
		break;
	case RT_VIEW_DEPTH_LOWRES:
		{
			spec.m_nWidth = nLowResWidth;
			spec.m_nHeight = nLowResHeight;
			spec.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_R32F;
		}
		break;
	case RT_VIEW_NORMAL_LOWRES:
		{
			spec.m_nWidth = nLowResWidth;
			spec.m_nHeight = nLowResHeight;
			spec.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_RGBA1010102;
		}
		break;
	case RT_LIGHTING_LOWRES:
		{
			spec.m_nWidth = nLowResWidth;
			spec.m_nHeight = nLowResHeight;
			spec.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_BGRA8888;
			if ( g_nPlatform == RST_PLATFORM_DX11 )
			{
				spec.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
			}
		}
		break;
	case RT_LIGHTING_TEMP_LOWRES:
		{
			spec.m_nWidth = nLowResWidth;
			spec.m_nHeight = nLowResHeight;
			spec.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_UNFILTERABLE_OK | TSPEC_RENDER_TARGET_SAMPLEABLE;
			spec.m_nImageFormat = IMAGE_FORMAT_BGRA8888;
			if ( g_nPlatform == RST_PLATFORM_DX11 )
			{
				spec.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
			}
		}
		break;

	default:
		return RENDER_TEXTURE_HANDLE_INVALID;
	}

	char pResourceName[16];
	Q_snprintf( pResourceName, sizeof(pResourceName), "%d", g_nMagicNumber );
	++g_nMagicNumber;
	hRetHandle = g_pRenderDevice->FindOrCreateTexture( "worldrenderer", pResourceName, &spec );
	return hRetHandle;
}

RenderTargetBinding_t CWorldRendererTest::CreateRenderTargetBinding( ViewBindingType_t type, const RenderViewport_t &viewport )
{
	RenderTargetBinding_t hRetHandle = RENDER_TARGET_BINDING_INVALID;
	HRenderTexture pRenderTargets[ 4 ] = { RENDER_TEXTURE_HANDLE_INVALID, RENDER_TEXTURE_HANDLE_INVALID, RENDER_TEXTURE_HANDLE_INVALID, RENDER_TEXTURE_HANDLE_INVALID };
	HRenderTexture hDepthStencil;
	int nTargets = 0;
	int nFlags = 0;

	RenderViewport_t lowResViewport = viewport;
	lowResViewport.m_nWidth = (int)( viewport.m_nWidth * m_flLowResBufferScale );
	lowResViewport.m_nHeight = (int)( viewport.m_nHeight * m_flLowResBufferScale );

	CWorldRenderFrustum *pFrustum = NULL;
	RenderViewport_t targetViewport;
	int nViewFlags = VIEW_PLAYER_CAMERA;
	int nViewPass = 0;

	switch ( type )
	{
	case RTB_SHADOW_DEPTH0:
	case RTB_SHADOW_DEPTH1:
	case RTB_SHADOW_DEPTH2:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_SHADOW_MULTIPLE_0 ];
			hDepthStencil = m_pRenderTargets[ type - DS_SHADOW_DEPTH0 ];
			nFlags = DISCARD_CONTENTS;

			pFrustum = m_pSplitFrustums[ type - DS_SHADOW_DEPTH0 ];
			targetViewport = m_shadowViewport;
			nViewFlags = VIEW_LIGHT_SHADOW;
			nViewPass = VARIATION_DEPTH;
		}
		break;
	case RTB_SHADOW_DEPTH_MULTIPLE_0:
	case RTB_SHADOW_DEPTH_MULTIPLE_1:
	case RTB_SHADOW_DEPTH_MULTIPLE_2:
	case RTB_SHADOW_DEPTH_MULTIPLE_3:
	case RTB_SHADOW_DEPTH_MULTIPLE_4:
	case RTB_SHADOW_DEPTH_MULTIPLE_5:
	case RTB_SHADOW_DEPTH_MULTIPLE_6:
	case RTB_SHADOW_DEPTH_MULTIPLE_7:
	case RTB_SHADOW_DEPTH_MULTIPLE_8:
	case RTB_SHADOW_DEPTH_MULTIPLE_9:
	case RTB_SHADOW_DEPTH_MULTIPLE_10:
	case RTB_SHADOW_DEPTH_MULTIPLE_11:
	case RTB_SHADOW_DEPTH_MULTIPLE_12:
	case RTB_SHADOW_DEPTH_MULTIPLE_13:
	case RTB_SHADOW_DEPTH_MULTIPLE_14:
	case RTB_SHADOW_DEPTH_MULTIPLE_15:
	case RTB_SHADOW_DEPTH_MULTIPLE_16:
	case RTB_SHADOW_DEPTH_MULTIPLE_17:
	case RTB_SHADOW_DEPTH_MULTIPLE_18:
	case RTB_SHADOW_DEPTH_MULTIPLE_19:
	case RTB_SHADOW_DEPTH_MULTIPLE_20:
	case RTB_SHADOW_DEPTH_MULTIPLE_21:
	case RTB_SHADOW_DEPTH_MULTIPLE_22:
	case RTB_SHADOW_DEPTH_MULTIPLE_23:
	case RTB_SHADOW_DEPTH_MULTIPLE_24:
	case RTB_SHADOW_DEPTH_MULTIPLE_25:
	case RTB_SHADOW_DEPTH_MULTIPLE_26:
	case RTB_SHADOW_DEPTH_MULTIPLE_27:
	case RTB_SHADOW_DEPTH_MULTIPLE_28:
	case RTB_SHADOW_DEPTH_MULTIPLE_29:
	case RTB_SHADOW_DEPTH_MULTIPLE_30:
	case RTB_SHADOW_DEPTH_MULTIPLE_31:
		{
			int nFrustum = type - RTB_SHADOW_DEPTH_MULTIPLE_0;

			nTargets = 1;
			if ( nFrustum < NUM_HIGH_RES_SHADOWS )
				pRenderTargets[0] = m_pRenderTargets[ RT_SHADOW_MULTIPLE_0 ];
			else
				pRenderTargets[0] = m_pRenderTargets[ RT_SHADOW_MULTIPLE_1 ];

			hDepthStencil = m_pRenderTargets[ DS_SHADOW_DEPTH_MULTIPLE ];
			nFlags = PRESERVE_DEPTHSTENCIL;//DISCARD_CONTENTS;

			if ( nFrustum < NUM_MULTI_SHADOWS )
			{
				pFrustum = m_pMultiShadowFrustums[ nFrustum ];
				targetViewport = m_multiShadowViewport[ nFrustum ];
			}
			else
			{
				pFrustum = NULL;
				targetViewport = viewport;
			}
			nViewFlags = VIEW_LIGHT_SHADOW;
			nViewPass = VARIATION_DEPTH;
		}
		break;

	case RTB_DEFERRED_DEPTH_NORM_SPEC:
		{
			nTargets = 3;
			pRenderTargets[0] = m_pRenderTargets[ RT_VIEW_NORMAL_HIGHRES ];
			pRenderTargets[1] = m_pRenderTargets[ RT_VIEW_DEPTH_HIGHRES ];
			pRenderTargets[2] = m_pRenderTargets[ RT_VIEW_SPEC_HIGHRES ];
			hDepthStencil = m_pRenderTargets[ DS_DEPTH_HIGHRES ];
			nFlags = DISCARD_CONTENTS;

			pFrustum = m_pFrustum;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA;
			nViewPass = VARIATION_NORM_DEPTH_SPEC;
		}
		break;
	case RTB_DEFERRED_SCALE_DOWN:
		{
			nTargets = 2;
			pRenderTargets[0] = m_pRenderTargets[ RT_VIEW_DEPTH_LOWRES ];
			pRenderTargets[1] = m_pRenderTargets[ RT_VIEW_NORMAL_LOWRES ];
			hDepthStencil = m_pRenderTargets[ DS_DEPTH_LOWRES ];
			nFlags = PRESERVE_DEPTHSTENCIL;

			pFrustum = m_pFrustum;
			targetViewport = lowResViewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_DEFERRED_NORM_SPEC:
		{
			nTargets = 2;
			pRenderTargets[0] = m_pRenderTargets[ RT_VIEW_NORMAL_HIGHRES ];
			pRenderTargets[1] = m_pRenderTargets[ RT_VIEW_SPEC_HIGHRES ];
			hDepthStencil = m_pRenderTargets[ DS_DEPTH_HIGHRES ];
			nFlags = PRESERVE_CONTENTS;

			pFrustum = m_pFrustum;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_DECALS;
			nViewPass = 0;
		}
		break;
	case RTB_CLEAR_SPEC_HIGHRES:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_SPECULAR_HIGHRES ];
			hDepthStencil = RENDER_TEXTURE_HANDLE_INVALID;
			nFlags = 0;

			pFrustum = m_pFrustum;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_LIGHTING;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_LIGHTING_HIGHRES:
		{
			nTargets = 2;
			pRenderTargets[0] = m_pRenderTargets[ RT_LIGHTING_HIGHRES ];
			pRenderTargets[1] = m_pRenderTargets[ RT_SPECULAR_HIGHRES ];
			hDepthStencil = m_pRenderTargets[ DS_DEPTH_HIGHRES ];
			nFlags = PRESERVE_DEPTHSTENCIL;

			pFrustum = m_pFrustum;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_LIGHTING;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_LIGHTING_LOWRES:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_LIGHTING_LOWRES ];
			hDepthStencil = m_pRenderTargets[ DS_DEPTH_LOWRES ];
			nFlags = PRESERVE_DEPTHSTENCIL;

			pFrustum = m_pFrustum;
			targetViewport = lowResViewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_LIGHTING;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_SSAO_LOWRES:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_LIGHTING_LOWRES ];
			hDepthStencil = RENDER_TEXTURE_HANDLE_INVALID;
			nFlags = 0;

			pFrustum = NULL;
			targetViewport = lowResViewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_BILAT_UPSAMPLE_0:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_LIGHTING_TEMP_LOWRES ];
			hDepthStencil = RENDER_TEXTURE_HANDLE_INVALID;
			nFlags = 0;

			pFrustum = NULL;
			targetViewport = lowResViewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_BILAT_UPSAMPLE_1:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_LIGHTING_HIGHRES ];
			hDepthStencil = RENDER_TEXTURE_HANDLE_INVALID;
			nFlags = 0;

			pFrustum = NULL;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_LIGHT_BLUR_0:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_VIEW_SPEC_HIGHRES ];
			hDepthStencil = RENDER_TEXTURE_HANDLE_INVALID;
			nFlags = 0;

			pFrustum = NULL;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_LIGHT_BLUR_1:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_LIGHTING_HIGHRES ];
			hDepthStencil = RENDER_TEXTURE_HANDLE_INVALID;
			nFlags = 0;

			pFrustum = NULL;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_SHADOW_BLUR_0:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_SHADOW_BLUR_TEMP ];
			hDepthStencil = RENDER_TEXTURE_HANDLE_INVALID;
			nFlags = 0;

			pFrustum = NULL;
			targetViewport = m_shadowViewport;
			nViewFlags = VIEW_LIGHT_SHADOW | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_SHADOW_BLUR_1:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_SHADOW_MULTIPLE_0 ];
			hDepthStencil = RENDER_TEXTURE_HANDLE_INVALID;
			nFlags = 0;

			pFrustum = NULL;
			targetViewport = m_shadowViewport;
			nViewFlags = VIEW_LIGHT_SHADOW | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_SHADOW_BLUR_2:
		{
			nTargets = 1;
			pRenderTargets[0] = m_pRenderTargets[ RT_SHADOW_MULTIPLE_1 ];
			hDepthStencil = RENDER_TEXTURE_HANDLE_INVALID;
			nFlags = 0;

			pFrustum = NULL;
			targetViewport = m_shadowViewport;
			nViewFlags = VIEW_LIGHT_SHADOW | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_SAMPLE_LIGHTING:
		{
			nTargets = 1;
			pRenderTargets[0] = RENDER_TEXTURE_DEFAULT_RENDER_TARGET;
			hDepthStencil = m_pRenderTargets[ DS_DEPTH_HIGHRES ];
			nFlags = PRESERVE_DEPTHSTENCIL;

			pFrustum = m_pFrustum;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA;
			nViewPass = VARIATION_SAMPLE_LIGHTING;
		}
		break;
	case RTB_AERIAL_PERSPECTIVE:
		{
			nTargets = 1;
			pRenderTargets[0] = RENDER_TEXTURE_DEFAULT_RENDER_TARGET;
			hDepthStencil = m_pRenderTargets[ DS_DEPTH_HIGHRES ];
			nFlags = PRESERVE_DEPTHSTENCIL;

			pFrustum = NULL;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_SKY:
		{
			nTargets = 1;
			pRenderTargets[0] = RENDER_TEXTURE_DEFAULT_RENDER_TARGET;
			hDepthStencil = m_pRenderTargets[ DS_DEPTH_HIGHRES ];
			nFlags = PRESERVE_DEPTHSTENCIL;

			pFrustum = m_pFrustum;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA | VIEW_POST_PROCESS;
			nViewPass = VARIATION_DEFAULT;
		}
		break;
	case RTB_FORWARD:
		{
			nTargets = 1;
			pRenderTargets[0] = RENDER_TEXTURE_DEFAULT_RENDER_TARGET;
			hDepthStencil = RENDER_TEXTURE_DEFAULT_RENDER_TARGET;
			nFlags = PRESERVE_DEPTHSTENCIL;

			pFrustum = m_pFrustum;
			targetViewport = viewport;
			nViewFlags = VIEW_PLAYER_CAMERA;
			nViewPass = VARIATION_DEFAULT;
		}
		break;

	default:
		return RENDER_TARGET_BINDING_INVALID;
	}

	hRetHandle = g_pRenderDevice->CreateRenderTargetBinding( nFlags, nTargets, pRenderTargets, hDepthStencil );
	m_pRenderViews[ type ].Init( g_pRenderDevice, pFrustum, hRetHandle, targetViewport, (RenderViewFlags_t)nViewFlags, nViewPass, type );

	return hRetHandle;
}

void CWorldRendererTest::SetStateForRenderViewType( IRenderContext *pRenderContext, int nRenderView )
{
	switch ( nRenderView )
	{
	case RTB_SHADOW_DEPTH0:
	case RTB_SHADOW_DEPTH1:
	case RTB_SHADOW_DEPTH2:
		{
			pRenderContext->SetBlendMode( RENDER_BLEND_NOPIXELWRITE );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_AND_WRITE );
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_NONE );
		}
		break;
	case RTB_SHADOW_DEPTH_MULTIPLE_0:
	case RTB_SHADOW_DEPTH_MULTIPLE_1:
	case RTB_SHADOW_DEPTH_MULTIPLE_2:
	case RTB_SHADOW_DEPTH_MULTIPLE_3:
	case RTB_SHADOW_DEPTH_MULTIPLE_4:
	case RTB_SHADOW_DEPTH_MULTIPLE_5:
	case RTB_SHADOW_DEPTH_MULTIPLE_6:
	case RTB_SHADOW_DEPTH_MULTIPLE_7:
	case RTB_SHADOW_DEPTH_MULTIPLE_8:
	case RTB_SHADOW_DEPTH_MULTIPLE_9:
	case RTB_SHADOW_DEPTH_MULTIPLE_10:
	case RTB_SHADOW_DEPTH_MULTIPLE_11:
	case RTB_SHADOW_DEPTH_MULTIPLE_12:
	case RTB_SHADOW_DEPTH_MULTIPLE_13:
	case RTB_SHADOW_DEPTH_MULTIPLE_14:
	case RTB_SHADOW_DEPTH_MULTIPLE_15:
	case RTB_SHADOW_DEPTH_MULTIPLE_16:
	case RTB_SHADOW_DEPTH_MULTIPLE_17:
	case RTB_SHADOW_DEPTH_MULTIPLE_18:
	case RTB_SHADOW_DEPTH_MULTIPLE_19:
	case RTB_SHADOW_DEPTH_MULTIPLE_20:
	case RTB_SHADOW_DEPTH_MULTIPLE_21:
	case RTB_SHADOW_DEPTH_MULTIPLE_22:
	case RTB_SHADOW_DEPTH_MULTIPLE_23:
	case RTB_SHADOW_DEPTH_MULTIPLE_24:
	case RTB_SHADOW_DEPTH_MULTIPLE_25:
	case RTB_SHADOW_DEPTH_MULTIPLE_26:
	case RTB_SHADOW_DEPTH_MULTIPLE_27:
	case RTB_SHADOW_DEPTH_MULTIPLE_28:
	case RTB_SHADOW_DEPTH_MULTIPLE_29:
	case RTB_SHADOW_DEPTH_MULTIPLE_30:
	case RTB_SHADOW_DEPTH_MULTIPLE_31:
		{
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_AND_WRITE );
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
		}
		break;

	case RTB_DEFERRED_DEPTH_NORM_SPEC:
		{
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_AND_WRITE_STENCIL_SET1 );
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
		}
		break;
	case RTB_DEFERRED_SCALE_DOWN:
		{
		}
		break;
	case RTB_DEFERRED_NORM_SPEC:
		{
		}
		break;
	case RTB_LIGHTING_HIGHRES:
		{
		}
		break;
	case RTB_LIGHTING_LOWRES:
		{
		}
		break;
	case RTB_SSAO_LOWRES:
		{
		}
		break;
	case RTB_BILAT_UPSAMPLE_0:
		{
		}
		break;
	case RTB_BILAT_UPSAMPLE_1:
		{
		}
		break;
	case RTB_LIGHT_BLUR_0:
		{
		}
		break;
	case RTB_LIGHT_BLUR_1:
		{
		}
		break;
	case RTB_SAMPLE_LIGHTING:
		{
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_EQUAL_NO_WRITE );
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );
			
			pRenderContext->BindTexture( 4, m_pRenderTargets[ RT_LIGHTING_HIGHRES ] );
			pRenderContext->SetSamplerStatePS( 4, RS_FILTER_MIN_MAG_MIP_POINT );
			pRenderContext->BindTexture( 5, m_pRenderTargets[ RT_SPECULAR_HIGHRES ] );
			pRenderContext->SetSamplerStatePS( 5, RS_FILTER_MIN_MAG_MIP_POINT );
			
		}
		break;
	case RTB_AERIAL_PERSPECTIVE:
		{
		}
		break;
	case RTB_SKY:
		{
		}
		break;
	case RTB_FORWARD:
		{
			// viewport and state
			pRenderContext->SetBlendMode( RENDER_BLEND_NONE );
			pRenderContext->SetZBufferMode( RENDER_ZBUFFER_ZTEST_AND_WRITE );
			pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_BACKFACING );

			// Bind
			for ( int s=0; s<NUM_SHADOW_SPLITS; ++s )
			{
				m_forwardLightingData.m_mShadowMatrices[s] = m_pShadowMatrices[s];
				pRenderContext->BindTexture( 4 + s, m_pRenderTargets[ DS_SHADOW_DEPTH0 + s ] );
				pRenderContext->SetSamplerStatePS( 4 + s, RS_FILTER_MIN_MAG_MIP_POINT );
			}

			pRenderContext->SetConstantBufferData( m_hLightingRelated, (void*)&m_forwardLightingData, sizeof( LightingConstants_t ) );
			pRenderContext->BindConstantBuffer( RENDER_PIXEL_SHADER, m_hLightingRelated, 0, 0 );
		}
		break;
	}
}

void CWorldRendererTest::InitRenderTargetsAndViews( const RenderViewport_t &viewport )
{
	for ( int i=0; i<MAX_RENDER_TARGET_TYPES; ++i )
	{
		m_pRenderTargets[i] = CreateRenderTarget( (RenderTargetType_t)i, viewport );
	}

	for ( int i=0; i<MAX_VIEW_BINDING_TYPES; ++i )
	{
		m_pViewBindings[i] = CreateRenderTargetBinding( (ViewBindingType_t)i, viewport );
	}

	m_bRenderTargetsInit = true;
}

void CWorldRendererTest::GetDescription( char *pNameBufferOut, size_t nBufLen )
{
	if ( m_nVariant )
	{
		Q_snprintf( pNameBufferOut, nBufLen, "Render the world (deferred) (Press Space to Continue)" );
	}
	else
	{
		Q_snprintf( pNameBufferOut, nBufLen, "Render the world (forward) (Press Space to Continue)" );
	}
}

//-----------------------------------------------------------------------------
// Can this test be run?
//-----------------------------------------------------------------------------
bool CWorldRendererTest::CanBeRun()
{ 
	return ( g_pWorldRendererMgr != NULL ) && m_bWorldLoaded; 
}

//-----------------------------------------------------------------------------
// check for a keypress
//-----------------------------------------------------------------------------
bool CWorldRendererTest::ProcessUserInput( const InputEvent_t &ie )
{
	if ( ie.m_nType == IE_AnalogValueChanged )
	{
		switch( ie.m_nData )
		{
		case MOUSE_XY:
			if ( m_nWindowCenterX > 0 && m_nWindowCenterY > 0 )
			{
				m_nCursorDeltaX += ie.m_nData2 - m_nWindowCenterX;
				m_nCursorDeltaY += ie.m_nData3 - m_nWindowCenterY;
			}
			break;

		case JOYSTICK_AXIS( 0, JOY_AXIS_U ):
			m_nCursorDeltaResetX = ie.m_nData2 / 300;
			m_nCursorDeltaX += m_nCursorDeltaResetX;
			break;

		case JOYSTICK_AXIS( 0, JOY_AXIS_R ):
			m_nCursorDeltaResetY = -ie.m_nData2 / 300;
			m_nCursorDeltaY += m_nCursorDeltaResetY;
			break;

		case JOYSTICK_AXIS( 0, JOY_AXIS_X ):
			if ( abs( ie.m_nData2 ) < 2 )
			{
				m_cmd.m_nKeys &= ~FKEY_RIGHT;
				m_cmd.m_nKeys &= ~FKEY_LEFT;
			}
			else if ( ie.m_nData2 > 0 )
			{
				m_cmd.m_nKeys |= FKEY_RIGHT;
				m_cmd.m_nKeys &= ~FKEY_LEFT;
			}
			else
			{
				m_cmd.m_nKeys &= ~FKEY_RIGHT;
				m_cmd.m_nKeys |= FKEY_LEFT;
			}
			break;

		case JOYSTICK_AXIS( 0, JOY_AXIS_Y ):
			if ( abs( ie.m_nData2 ) < 2 )
			{
				m_cmd.m_nKeys &= ~FKEY_FORWARD;
				m_cmd.m_nKeys &= ~FKEY_BACK;
			}
			else if ( ie.m_nData2 > 0 )
			{
				m_cmd.m_nKeys |= FKEY_FORWARD;
				m_cmd.m_nKeys &= ~FKEY_BACK;
			}
			else
			{
				m_cmd.m_nKeys &= ~FKEY_FORWARD;
				m_cmd.m_nKeys |= FKEY_BACK;
			}
			break;
		}
	}
	else if ( ie.m_nType == IE_ButtonPressed || ie.m_nType == IE_ButtonReleased )
	{
		bool bDown = (ie.m_nType == IE_ButtonPressed) ? true : false;
		ButtonCode_t code = (ButtonCode_t)ie.m_nData;
		uint nFlag = 0;
		switch( code )
		{
		case KEY_W: nFlag = FKEY_FORWARD; break;
		case KEY_S: nFlag = FKEY_BACK; break;
		case KEY_A: nFlag = FKEY_LEFT; break;
		case KEY_D: nFlag = FKEY_RIGHT; break;
		case KEY_UP: nFlag = FKEY_TURNUP; break;
		case KEY_DOWN: nFlag = FKEY_TURNDOWN; break;
		case KEY_LEFT: nFlag = FKEY_TURNLEFT; break;
		case KEY_RIGHT: nFlag = FKEY_TURNRIGHT; break;
		case KEY_XBUTTON_X: case MOUSE_LEFT: nFlag = FKEY_SHOOTDECAL; break;
		case KEY_XBUTTON_B: case KEY_N: nFlag = FKEY_SHOOTPSYSTEM; break;
		case KEY_XBUTTON_Y: case KEY_M: nFlag = FKEY_SHOOTSSYSTEM; break;
		case KEY_O: nFlag = FKEY_LIGHTELV; break;
		case KEY_XBUTTON_UP: case KEY_K: nFlag = FKEY_LIGHTANG_POS; break;
		case KEY_XBUTTON_DOWN: case KEY_L: nFlag = FKEY_LIGHTANG_NEG; break;
		case KEY_XBUTTON_LEFT_SHOULDER: case KEY_LSHIFT: nFlag = FKEY_FAST; break;
		case KEY_HOME: nFlag = FKEY_ENABLE_VIS; break;
		case KEY_END: nFlag = FKEY_DISABLE_VIS; break;
		case KEY_RSHIFT: nFlag = FKEY_DROP_FRUSTUM; break;
		case KEY_P: nFlag = FKEY_TOGGLE_AERIAL; break;
		case KEY_V: nFlag = FKEY_TOGGLE_BOUNCE; break;
		case KEY_C: nFlag = FKEY_TOGGLE_DIRECT; break;
		case KEY_B: nFlag = FKEY_TOGGLE_LIGHTING_ONLY; break;
		case KEY_PAD_ENTER: nFlag = FKEY_SSAO; break;
		case KEY_PAD_8: nFlag = FKEY_BLUR_LIGHTING; break;
		case KEY_PAD_9: nFlag = FKEY_BLUR_SHADOWS; break;
		case MOUSE_RIGHT: nFlag = FKEY_SHOOTDECAL2; break;
		case MOUSE_MIDDLE: nFlag = FKEY_SHOOTWALLMONSTER; break;
		case KEY_X:
			{
				if ( !bDown )
				{
					m_bViewLowTexture = !m_bViewLowTexture;
					if ( m_bViewLowTexture )
						Msg( "Low Texture Mode: On\n" );
					else
						Msg( "Low Texture Mode: Off\n" );
				}
				break;
			}
		case KEY_F: 
			{
				if ( !bDown )
				{
					m_bFlashlight = !m_bFlashlight;
					if ( m_bFlashlight )
						Msg( "Flashlight: On\n" );
					else
						Msg( "Flashlight: Off\n" );
				}
				break;
			}
		case KEY_G:
			{
				if ( !bDown )
				{
					Msg( "Dropped Renderable\n" );
					DropRenderable();
				}
				break;
			}
		case KEY_0:
			{
				// Reload shaders
				if ( ie.m_nType == IE_ButtonPressed )
				{
					g_pMaterialSystem2->DynamicShaderCompile_ReloadAllShaders();
					return false;
				}
				break;
			}
		case KEY_PAD_PLUS: 
			{
				m_flSampleRadius += 1.0f; 
				Msg( "SSAO Sample Radius: %f\n", m_flSampleRadius );
				break;
			}
		case KEY_PAD_MINUS: 
			{
				m_flSampleRadius -= 1.0f; 
				m_flSampleRadius = MAX( 1, m_flSampleRadius ); 
				Msg( "SSAO Sample Radius: %f\n", m_flSampleRadius );
				break;
			}
		case KEY_PAD_DIVIDE:
			{
				if ( !bDown )
				{
					m_nViewBuffer = (RenderTargetType_t)( m_nViewBuffer + 1 );
					if ( m_nViewBuffer >= MAX_RENDER_TARGET_TYPES )
					{
						m_nViewBuffer = (RenderTargetType_t)0;
					}

					Msg( "%s\n", g_pRenderTargetName[ m_nViewBuffer ] );
				}
				break;
			}
		case KEY_PAD_MULTIPLY:
			{
				if ( !bDown )
				{
					m_nViewBuffer = (RenderTargetType_t)( m_nViewBuffer - 1 );
					if ( m_nViewBuffer < 0 )
					{
						m_nViewBuffer = (RenderTargetType_t)( MAX_RENDER_TARGET_TYPES - 1 );
					}

					Msg( "%s\n", g_pRenderTargetName[ m_nViewBuffer ] );
				}
				break;
			}
		case KEY_TAB: 
			{
				m_nReportLevel = bDown ? 1 : 3;
				break;
			}
		case KEY_Z:
			{
				if ( !bDown )
				{
					Vector vCameraOrigin( 100, 24975, 250 );
					QAngle vCameraAngles( 9.6, -27, 0 );

					m_pFrustum->SetCameraPosition( vCameraOrigin );
					m_pFrustum->SetCameraAngles( vCameraAngles );

					m_pWorldRenderer->ClearOutstandingLoadRequests();
				}
			}
			break;
		}
		if ( nFlag )
		{
			if ( bDown )
			{
				m_cmd.m_nKeys |= nFlag;
			}
			else
			{
				m_cmd.m_nKeys &= ~nFlag;
			}
			return false;
		}
	}

	return BaseClass::ProcessUserInput( ie );
}

static void PerformWorkUnit( SimulateWorkUnitWorldRender_t & job )
{
	for( int i =0; i < job.m_nNumSystems; i++ )
	{
		job.m_pParticles->Simulate( job.m_flTimeStep );
	}
}

void CWorldRendererTest::SimulateParticles( float flTimeStep )
{
	int nSystems = m_particleWorkUnits.Count();
	for( int i = 0; i < nSystems; i++ )
	{
		m_particleWorkUnits[i].m_flTimeStep = flTimeStep;
	}

	ParallelProcess( m_particleWorkUnits.Base(), nSystems, PerformWorkUnit );
}

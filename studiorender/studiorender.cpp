//===== Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include <stdlib.h>
#include <algorithm>

#include "studiorender.h"
#include "studiorendercontext.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "optimize.h"
#include "mathlib/vmatrix.h"
#include "tier0/vprof.h"
#include "tier1/strtools.h"
#include "tier1/keyvalues.h"
#include "tier0/memalloc.h"
#include "convar.h"
#include "materialsystem/itexture.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
CStudioRender g_StudioRender;
CStudioRender *g_pStudioRenderImp = &g_StudioRender;


//-----------------------------------------------------------------------------
// Activate to get stats
//-----------------------------------------------------------------------------
//#define REPORT_FLEX_STATS 1

#ifdef REPORT_FLEX_STATS
static int s_nModelsDrawn = 0;
static int s_nActiveFlexCount = 0;
static ConVar r_flexstats( "r_flexstats", "0", FCVAR_CHEAT );
#endif

// Multiplicative factor on LOD switch points. See GetLODForMetric() in studio.h
static ConVar r_lod_switch_scale( "r_lod_switch_scale", "1", FCVAR_HIDDEN );

#ifndef _CERT
static ConVar mat_rendered_faces_count( "mat_rendered_faces_count", "0", FCVAR_CHEAT, "Set to N to count how many faces each model draws each frame and spew the top N offenders from the last 150 frames (use 'mat_rendered_faces_spew' to spew all models rendered in the current frame)" );
static ConVar mat_print_top_model_vert_counts( "mat_print_top_model_vert_counts", "0", 0, "Constantly print to screen the top N models as measured by total faces rendered this frame");

bool ModelFaceCountHashCompareFunc( studiohwdata_t *const &a, studiohwdata_t *const &b ) { return a == b; }
uint32 ModelFaceCountHashKeyFunc( studiohwdata_t *const &a ) { return HashIntConventional( (int32)(intp)a ); }
#endif // !_CERT

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CStudioRender::CStudioRender()
#ifndef _CERT
 :	m_ModelFaceCountHash( 1024, 0, 0, ModelFaceCountHashCompareFunc, ModelFaceCountHashKeyFunc )
#endif
{
	m_pRC = NULL;
	m_pBoneToWorld = NULL;
	m_pFlexWeights = NULL;
	m_pFlexDelayedWeights = NULL;
	m_pStudioHdr = NULL;
	m_pStudioMeshes = NULL;
	m_pSubModel = NULL;
	m_pStudioHWData = NULL;
	m_pGlintTexture = NULL;
	m_GlintWidth = 0;
	m_GlintHeight = 0;
	m_pCurrentFlashlight = 0;

	// Cache-align our important matrices
	g_pMemAlloc->PushAllocDbgInfo( __FILE__, __LINE__ );

	m_PoseToWorld = (matrix3x4_t*)MemAlloc_AllocAligned( MAXSTUDIOBONES * sizeof(matrix3x4_t), 32 );
	m_PoseToDecal = (matrix3x4_t*)MemAlloc_AllocAligned( MAXSTUDIOBONES * sizeof(matrix3x4_t), 32 );

	g_pMemAlloc->PopAllocDbgInfo();
	m_nDecalId = 1;
}

CStudioRender::~CStudioRender()
{
	MemAlloc_FreeAligned(m_PoseToWorld);
	MemAlloc_FreeAligned(m_PoseToDecal);
}

void CStudioRender::InitDebugMaterials( void )
{

	// Four Wireframe Materials: ( ZBuffer, DisplacementMapped )
	m_pMaterialWireframe[0][0] = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/debugmrmwireframe", TEXTURE_GROUP_OTHER, true );
	m_pMaterialWireframe[0][0]->IncrementReferenceCount();

	m_pMaterialWireframe[1][0] = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/debugmrmwireframezbuffer", TEXTURE_GROUP_OTHER, true );
	m_pMaterialWireframe[1][0]->IncrementReferenceCount();

	m_pMaterialWireframe[0][1] = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/debugmrmwireframedisplaced", TEXTURE_GROUP_OTHER, true );
	m_pMaterialWireframe[0][1]->IncrementReferenceCount();

	m_pMaterialWireframe[1][1] = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/debugmrmwireframezbufferdisplaced", TEXTURE_GROUP_OTHER, true );
	m_pMaterialWireframe[1][1]->IncrementReferenceCount();

	m_pMaterialMRMNormals = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/debugmrmnormals", TEXTURE_GROUP_OTHER, true );
	m_pMaterialMRMNormals->IncrementReferenceCount();

	m_pMaterialTangentFrame = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/debugvertexcolor", TEXTURE_GROUP_OTHER, true );
	m_pMaterialTangentFrame->IncrementReferenceCount();

	m_pMaterialTranslucentModelHulls = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/debugtranslucentmodelhulls", TEXTURE_GROUP_OTHER, true );
	m_pMaterialTranslucentModelHulls->IncrementReferenceCount();

	m_pMaterialSolidModelHulls = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/debugsolidmodelhulls", TEXTURE_GROUP_OTHER, true );
	m_pMaterialSolidModelHulls->IncrementReferenceCount();

	m_pMaterialAdditiveVertexColorVertexAlpha = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/additivevertexcolorvertexalpha", TEXTURE_GROUP_OTHER, true );
	m_pMaterialAdditiveVertexColorVertexAlpha->IncrementReferenceCount();

	m_pMaterialModelBones = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/debugmodelbones", TEXTURE_GROUP_OTHER, true );
	m_pMaterialModelBones->IncrementReferenceCount();

	m_pMaterialModelEnvCubemap = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/env_cubemap_model", TEXTURE_GROUP_OTHER, true );
	m_pMaterialModelEnvCubemap->IncrementReferenceCount();
	
	m_pMaterialWorldWireframe = g_pMaterialSystem->FindMaterial( "//platform/materials/debug/debugworldwireframe", TEXTURE_GROUP_OTHER, true );
	m_pMaterialWorldWireframe->IncrementReferenceCount();

	KeyValues *pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$treesway", 0 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	m_pDepthWrite[0][0][0] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite000", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	m_pDepthWrite[0][0][0]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$treesway", 0 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	m_pDepthWrite[0][1][0] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite010", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	m_pDepthWrite[0][1][0]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$treesway", 0 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	m_pDepthWrite[1][0][0] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite100", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	m_pDepthWrite[1][0][0]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$treesway", 0 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	m_pDepthWrite[1][1][0] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite110", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	m_pDepthWrite[1][1][0]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$treesway", 1 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	m_pDepthWrite[0][0][1] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite001", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	m_pDepthWrite[0][0][1]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$treesway", 1 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	m_pDepthWrite[0][1][1] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite011", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	m_pDepthWrite[0][1][1]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$treesway", 1 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	m_pDepthWrite[1][0][1] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite101", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	m_pDepthWrite[1][0][1]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$treesway", 1 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	m_pDepthWrite[1][1][1] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite111", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	m_pDepthWrite[1][1][1]->IncrementReferenceCount();

	// Full frame depth as color (r32f)

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$treesway", 0 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	m_pSSAODepthWrite[ 0 ][ 0 ][ 0 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite000", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pSSAODepthWrite[ 0 ][ 0 ][ 0 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$treesway", 0 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	m_pSSAODepthWrite[ 0 ][ 1 ][ 0 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite010", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pSSAODepthWrite[ 0 ][ 1 ][ 0 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$treesway", 0 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	m_pSSAODepthWrite[ 1 ][ 0 ][ 0 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite100", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pSSAODepthWrite[ 1 ][ 0 ][ 0 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$treesway", 0 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	m_pSSAODepthWrite[ 1 ][ 1 ][ 0 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite110", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pSSAODepthWrite[ 1 ][ 1 ][ 0 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$treesway", 1 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	m_pSSAODepthWrite[ 0 ][ 0 ][ 1 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite001", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pSSAODepthWrite[ 0 ][ 0 ][ 1 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$treesway", 1 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	m_pSSAODepthWrite[ 0 ][ 1 ][ 1 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite011", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pSSAODepthWrite[ 0 ][ 1 ][ 1 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$treesway", 1 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	m_pSSAODepthWrite[ 1 ][ 0 ][ 1 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite101", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pSSAODepthWrite[ 1 ][ 0 ][ 1 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$treesway", 1 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	m_pSSAODepthWrite[ 1 ][ 1 ][ 1 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite111", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pSSAODepthWrite[ 1 ][ 1 ][ 1 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "EyeGlint" );
	m_pGlintBuildMaterial = g_pMaterialSystem->CreateMaterial( "___glintbuildmaterial", pVMTKeyValues );

	pVMTKeyValues = new KeyValues( "unlitgeneric" );
	pVMTKeyValues->SetInt( "$color", 0 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$writez", 0 );
	m_pMaterialSolidBackfacePrepass = g_pMaterialSystem->FindProceduralMaterial( "__utilBackfacePrepass", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_pMaterialSolidBackfacePrepass->IncrementReferenceCount();

}

void CStudioRender::ShutdownDebugMaterials( void )
{
	for ( int i=0; i<2; i++ )
	{
		for ( int j=0; j<2; j++ )
		{
			if ( m_pMaterialWireframe[i][j] )
			{
				m_pMaterialWireframe[i][j]->DecrementReferenceCount();
				m_pMaterialWireframe[i][j] = NULL;
			}
		}
	}

	if ( m_pMaterialMRMNormals )
	{
		m_pMaterialMRMNormals->DecrementReferenceCount();
		m_pMaterialMRMNormals = NULL;
	}

	if ( m_pMaterialTangentFrame )
	{
		m_pMaterialTangentFrame->DecrementReferenceCount();
		m_pMaterialTangentFrame = NULL;
	}

	if ( m_pMaterialTranslucentModelHulls )
	{
		m_pMaterialTranslucentModelHulls->DecrementReferenceCount();
		m_pMaterialTranslucentModelHulls = NULL;
	}
	
	if ( m_pMaterialSolidModelHulls )
	{
		m_pMaterialSolidModelHulls->DecrementReferenceCount();
		m_pMaterialSolidModelHulls = NULL;
	}
	
	if ( m_pMaterialAdditiveVertexColorVertexAlpha )
	{
		m_pMaterialAdditiveVertexColorVertexAlpha->DecrementReferenceCount();
		m_pMaterialAdditiveVertexColorVertexAlpha = NULL;
	}
	
	if ( m_pMaterialModelBones )
	{
		m_pMaterialModelBones->DecrementReferenceCount();
		m_pMaterialModelBones = NULL;
	}
	
	if ( m_pMaterialModelEnvCubemap )
	{
		m_pMaterialModelEnvCubemap->DecrementReferenceCount();
		m_pMaterialModelEnvCubemap = NULL;
	}

	if ( m_pMaterialWorldWireframe )
	{
		m_pMaterialWorldWireframe->DecrementReferenceCount();
		m_pMaterialWorldWireframe = NULL;
	}

	// DepthWrite materials
	for ( int32 i = 0; i < 8; i++ )
	{
		if ( m_pDepthWrite[ ( i & 0x4 ) >> 2 ][ ( i & 0x2 ) >> 1 ][ i & 0x1 ] )
		{
			m_pDepthWrite[ ( i & 0x4 ) >> 2 ][ ( i & 0x2 ) >> 1 ][ i & 0x1 ]->DecrementReferenceCount();
		}

		if ( m_pSSAODepthWrite[ ( i & 0x4 ) >> 2 ][ ( i & 0x2 ) >> 1 ][ i & 0x1 ] )
		{
			m_pSSAODepthWrite[ ( i & 0x4 ) >> 2 ][ ( i & 0x2 ) >> 1 ][ i & 0x1 ]->DecrementReferenceCount();
		}

	}

	if ( m_pGlintBuildMaterial )
	{
		m_pGlintBuildMaterial->DecrementReferenceCount();
		m_pGlintBuildMaterial = NULL;
	}

	if ( m_pMaterialSolidBackfacePrepass )
	{
		m_pMaterialSolidBackfacePrepass->DecrementReferenceCount();
		m_pMaterialSolidBackfacePrepass = NULL;
	}
}

static void ReleaseMaterialSystemObjects( int nChangeFlags )
{
//	g_StudioRender.UncacheGlint();
}

static void RestoreMaterialSystemObjects( int nChangeFlags )
{
//	g_StudioRender.PrecacheGlint();
}



//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CStudioRender::Init()
{
	if ( g_pMaterialSystem && g_pMaterialSystemHardwareConfig )
	{
		g_pMaterialSystem->AddReleaseFunc( ReleaseMaterialSystemObjects );
		g_pMaterialSystem->AddRestoreFunc( RestoreMaterialSystemObjects );

		InitDebugMaterials();

		return INIT_OK;
	}

	return INIT_FAILED;
}

void CStudioRender::Shutdown( void )
{
	UncacheGlint();
	ShutdownDebugMaterials();

	if ( g_pMaterialSystem )
	{
		g_pMaterialSystem->RemoveReleaseFunc( ReleaseMaterialSystemObjects );
		g_pMaterialSystem->RemoveRestoreFunc( RestoreMaterialSystemObjects );
	}
}


//-----------------------------------------------------------------------------
// Begin/End frame methods
//-----------------------------------------------------------------------------
void CStudioRender::BeginFrame( void )
{
#ifndef _CERT
	// Clear the model face count hash table for the coming frame
	if ( mat_rendered_faces_count.GetBool() || mat_print_top_model_vert_counts.GetBool() )
		m_ModelFaceCountHash.RemoveAll();
#endif // !_CERT

	PrecacheGlint();
}

void CStudioRender::EndFrame( void )
{
#ifndef _CERT
	UpdateModelFaceCounts();
#endif // !_CERT

	CleanupDecals();
}


//-----------------------------------------------------------------------------
// Sets the lighting render state
//-----------------------------------------------------------------------------
void CStudioRender::SetLightingRenderState()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// FIXME: What happens when we use the fixed function pipeline but vertex shaders 
	// are active? For the time being this only works because everything that does
	// vertex lighting does, in fact, have a vertex shader which is used to render it.
	pRenderContext->SetAmbientLightCube( m_pRC->m_LightBoxColors );

	if ( m_pRC->m_Config.bSoftwareLighting )
	{
		pRenderContext->DisableAllLocalLights();
	}
	else
	{
		pRenderContext->SetLights( m_pRC->m_NumLocalLights, m_pRC->m_LocalLights );
	}
}


//-----------------------------------------------------------------------------
// Shadow state (affects the models as they are rendered)
//-----------------------------------------------------------------------------
void CStudioRender::AddShadow( IMaterial* pMaterial, void* pProxyData, FlashlightState_t *pFlashlightState, VMatrix *pWorldToTexture, ITexture *pFlashlightDepthTexture )
{
	int i = m_ShadowState.AddToTail();
	ShadowState_t& state = m_ShadowState[i];
	state.m_pMaterial = pMaterial;
	state.m_pProxyData = pProxyData;
	state.m_pFlashlightState = pFlashlightState;
	state.m_pWorldToTexture = pWorldToTexture;
	state.m_pFlashlightDepthTexture = pFlashlightDepthTexture;
}

void CStudioRender::ClearAllShadows()
{
	m_ShadowState.RemoveAll();
}

void CStudioRender::GetFlexStats( )
{
#ifdef REPORT_FLEX_STATS
	static bool s_bLastFlexStats = false;
	bool bDoStats = r_flexstats.GetInt() != 0;
	if ( bDoStats )
	{
		if ( !s_bLastFlexStats )
		{
			s_nModelsDrawn = 0;
			s_nActiveFlexCount = 0;
		}

		// Count number of active weights
		int nActiveFlexCount = 0;
		for ( int i = 0; i < MAXSTUDIOFLEXDESC; ++i )
		{
			if ( fabs( m_FlexWeights[i] ) >= 0.001f || fabs( m_FlexDelayedWeights[i] ) >= 0.001f )
			{
				++nActiveFlexCount;
			}
		}

		++s_nModelsDrawn;
		s_nActiveFlexCount += nActiveFlexCount;
	}
	else
	{
		if ( s_bLastFlexStats )
		{
			if ( s_nModelsDrawn )
			{
				Msg( "Average number of flexes/model: %d\n", s_nActiveFlexCount / s_nModelsDrawn );
			}
			else
			{
				Msg( "No models rendered to take stats of\n" );
			}

			s_nModelsDrawn = 0;
			s_nActiveFlexCount = 0;
		}
	}

	s_bLastFlexStats = bDoStats;
#endif
}

ConVar cl_skipslowpath( "cl_skipslowpath", "0", FCVAR_CHEAT | FCVAR_MATERIAL_SYSTEM_THREAD, "Set to 1 to skip any models that don't go through the model fast path" );

//-----------------------------------------------------------------------------
// Main model rendering entry point
//-----------------------------------------------------------------------------
void CStudioRender::DrawModel( const DrawModelInfo_t& info, const StudioRenderContext_t &rc, 
	matrix3x4_t *pBoneToWorld, const FlexWeights_t &flex, int flags )
{
	if ( cl_skipslowpath.GetBool () )
		return;
	VPROF( "CStudioRender::DrawModel");

	if ( ( flags & STUDIORENDER_MODEL_IS_CACHEABLE ) && !g_pMDLCache->IsDataLoaded( VoidPtrToMDLHandle( info.m_pStudioHdr->VirtualModel() ), MDLCACHE_STUDIOHWDATA ) )
	{
		// cacheable models may have had their hw data evicted while they were queued for rendering
		return;
	}

	m_pRC = const_cast< StudioRenderContext_t* >( &rc );
	m_pFlexWeights = flex.m_pFlexWeights;
	m_pFlexDelayedWeights = flex.m_pFlexDelayedWeights;
	m_pBoneToWorld = pBoneToWorld;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Disable flex if we're told to...
	bool flexConfig = m_pRC->m_Config.bFlex;
	if (flags & STUDIORENDER_DRAW_NO_FLEXES)
	{
		m_pRC->m_Config.bFlex = false;
	}

	// Enable wireframe if we're told to...
	bool bWireframe = m_pRC->m_Config.bWireframe;
	if ( flags & STUDIORENDER_DRAW_WIREFRAME )
	{
		m_pRC->m_Config.bWireframe = true;
	}

	int boneMask = BONE_USED_BY_VERTEX_AT_LOD( info.m_Lod );

	// Preserve the matrices if we're skinning
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	m_VertexCache.StartModel();

	m_pStudioHdr = info.m_pStudioHdr;
	m_pStudioMeshes = info.m_pHardwareData->m_pLODs[info.m_Lod].m_pMeshData;
	m_pStudioHWData = info.m_pHardwareData;

#if PIX_ENABLE
	char szPIXEventName[128];
	sprintf( szPIXEventName, "%s*", m_pStudioHdr->name );	// PIX
	PIXEVENT( pRenderContext, szPIXEventName );
#endif

	// Bone to world must be set before calling drawmodel; it uses that here
	ComputePoseToWorld( m_PoseToWorld, m_pStudioHdr, boneMask, m_pRC->m_ViewOrigin, pBoneToWorld );

	bool bOldFlashlightState = false;
	if ( pRenderContext->IsCullingEnabledForSinglePassFlashlight() && IsGameConsole() )
	{
		bOldFlashlightState = pRenderContext->GetFlashlightMode();
		pRenderContext->SetFlashlightMode( m_ShadowState.Count() > 0 );
	}

	if ( ( flags & STUDIORENDER_NO_PRIMARY_DRAW ) == 0 )	// if this flag is set, then we are drawing multiple shadows in separate calls ( probably for capture )
	{	
		R_StudioRenderModel( pRenderContext, info.m_Skin, info.m_Body, info.m_HitboxSet, info.m_pClientEntity,
			info.m_pHardwareData->m_pLODs[info.m_Lod].ppMaterials, 
			info.m_pHardwareData->m_pLODs[info.m_Lod].pMaterialFlags, flags, boneMask, info.m_Lod, info.m_pColorMeshes);
	}

	if ( pRenderContext->IsCullingEnabledForSinglePassFlashlight() && IsGameConsole() )
	{
		pRenderContext->SetFlashlightMode( bOldFlashlightState );
	}

	// Draw all the decals on this model
	// If the model is not in memory, this code may not function correctly
	// This code assumes the model has been rendered!
	// So skip if the model hasn't been rendered
	// Also, skip if we're rendering to the shadow depth map
	if ( ( m_pStudioMeshes != 0 ) && !( flags &  ( STUDIORENDER_SHADOWDEPTHTEXTURE | STUDIORENDER_SSAODEPTHTEXTURE ) ) )
	{
		// Draw shadows
		if ( !( flags & STUDIORENDER_DRAW_NO_SHADOWS ) )
		{
			DrawShadows( info, flags, boneMask );
		}

		if ( ( ( flags & STUDIORENDER_DRAW_GROUP_MASK ) != STUDIORENDER_DRAW_TRANSLUCENT_ONLY ) && !( flags & STUDIORENDER_SKIP_DECALS ) )
		{
			DrawDecal( info, info.m_Lod, info.m_Body );
		}

		if( (flags & STUDIORENDER_DRAW_GROUP_MASK) != STUDIORENDER_DRAW_TRANSLUCENT_ONLY &&
			!( flags & STUDIORENDER_DRAW_NO_SHADOWS ) && !( flags & STUDIORENDER_SKIP_DECALS ) )
		{
			DrawFlashlightDecals( info, info.m_Lod );
		}
	}

	// Restore the matrices if we're skinning
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	// Restore the configs
	m_pRC->m_Config.bFlex = flexConfig;
	m_pRC->m_Config.bWireframe = bWireframe;

#ifdef REPORT_FLEX_STATS
	GetFlexStats();
#endif

	pRenderContext->SetNumBoneWeights( 0 );
	m_pRC = NULL;
	m_pBoneToWorld = NULL;
	m_pFlexWeights = NULL;
	m_pFlexDelayedWeights = NULL;
	m_pStudioHdr = NULL;
	m_pStudioMeshes = NULL;
	m_pStudioHWData = NULL;
}

void CStudioRender::DrawModelStaticProp( const DrawModelInfo_t& info, 
	const StudioRenderContext_t &rc, const matrix3x4_t& rootToWorld, int flags )
{
	if ( cl_skipslowpath.GetBool () )
		return;
	VPROF( "CStudioRender::DrawModelStaticProp");

	m_pRC = const_cast<StudioRenderContext_t*>( &rc );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	memcpy( &m_StaticPropRootToWorld, &rootToWorld, sizeof(matrix3x4_t) );
	memcpy( &m_PoseToWorld[0], &rootToWorld, sizeof(matrix3x4_t) );
	m_pBoneToWorld = &m_StaticPropRootToWorld;

	bool flexConfig = m_pRC->m_Config.bFlex;
	m_pRC->m_Config.bFlex = false;
	bool bWireframe = m_pRC->m_Config.bWireframe;
	if ( flags & STUDIORENDER_DRAW_WIREFRAME )
	{
		m_pRC->m_Config.bWireframe = true;
	}

	int lod = info.m_Lod;
	m_pStudioHdr = info.m_pStudioHdr;
	m_pStudioMeshes = info.m_pHardwareData->m_pLODs[lod].m_pMeshData;
	m_pStudioHWData = info.m_pHardwareData;

	R_StudioRenderModel( pRenderContext, info.m_Skin, info.m_Body, info.m_HitboxSet, info.m_pClientEntity,
		info.m_pHardwareData->m_pLODs[lod].ppMaterials, 
		info.m_pHardwareData->m_pLODs[lod].pMaterialFlags, flags, BONE_USED_BY_ANYTHING, lod, info.m_pColorMeshes);

	// If we're not shadow depth mapping
	if ( ( flags & ( STUDIORENDER_SHADOWDEPTHTEXTURE | STUDIORENDER_SSAODEPTHTEXTURE ) ) == 0 )
	{
		// Draw shadows
		if ( !( flags & STUDIORENDER_DRAW_NO_SHADOWS ) )
		{
			DrawShadows( info, flags, BONE_USED_BY_ANYTHING );
		}

		// FIXME: Should this occur in a separate call?
		// Draw all the decals on this model
		if ( ( ( flags & STUDIORENDER_DRAW_GROUP_MASK ) != STUDIORENDER_DRAW_TRANSLUCENT_ONLY ) && !( flags & STUDIORENDER_SKIP_DECALS ) )
		{
			DrawDecal( info, lod, info.m_Body );
		}

		if( (flags & STUDIORENDER_DRAW_GROUP_MASK) != STUDIORENDER_DRAW_TRANSLUCENT_ONLY &&
			!( flags & STUDIORENDER_DRAW_NO_SHADOWS ) && !( flags & STUDIORENDER_SKIP_DECALS ) )
		{
			DrawFlashlightDecals( info, lod );
		}
	}

	// Restore the configs
	m_pRC->m_Config.bFlex = flexConfig;
	m_pRC->m_Config.bWireframe = bWireframe;

	pRenderContext->SetNumBoneWeights( 0 );
	m_pBoneToWorld = NULL;
	m_pRC = NULL;
	m_pStudioHdr = NULL;
	m_pStudioMeshes = NULL;
	m_pStudioHWData = NULL;
}


//-----------------------------------------------------------------------------
// Used to render instances
//-----------------------------------------------------------------------------
struct BaseMeshRenderData_t
{
	studiomeshgroup_t *m_pGroup;
	mstudiomesh_t *m_pMesh;
	IMaterial *m_pMaterial;
};

struct ShadowMeshRenderData_t : public BaseMeshRenderData_t
{
	StudioShadowArrayInstanceData_t *m_pInstance;
	IMaterial *m_pSrcMaterial;
	VertexCompressionType_t m_nCompressionType;
	int m_nMeshBoneCount;
	bool m_bIsAlphaTested;
	bool m_bUsesTreeSway;
};

struct MeshRenderData_t : public BaseMeshRenderData_t
{
	StudioArrayInstanceData_t *m_pInstance;
};

struct MeshRenderData2_t : public BaseMeshRenderData_t
{
	StudioArrayInstanceData_t *m_pInstance;

	int16 m_nCompressionType; // VertexCompressionType_t smooshed to 16 bits
	int16 m_nMeshBoneCount;
};


//-----------------------------------------------------------------------------
// Counts the number of meshes to draw
//-----------------------------------------------------------------------------
int CStudioRender::CountMeshesToDraw( const StudioModelArrayInfo_t &drawInfo, int nCount, StudioArrayInstanceData_t *pInstanceData, int nInstanceStride, int nTimesRendered )
{
	VPROF( "CStudioRender::CountMeshesToDraw" );

#ifndef _CERT
	bool bCountRenderedFaces = mat_rendered_faces_count.GetBool() || mat_print_top_model_vert_counts.GetBool();
#endif // !_CERT

	StudioArrayInstanceData_t *pCurInstance = pInstanceData;
	int nTotalMeshCount = 0;
	for ( int i = 0; i < nCount; ++i, pCurInstance = (StudioArrayInstanceData_t*)( (char*)pCurInstance + nInstanceStride ) )
	{
		// This subarray has the same skin + body + lod
		int nLod = pCurInstance->m_nLOD;

		// get the studio mesh data for this lod
		studiomeshdata_t *pMeshDataBase = drawInfo.m_pHardwareData->m_pLODs[nLod].m_pMeshData;

#ifndef _CERT
		// Each model counts how many rendered faces it accounts for each frame:
		if ( bCountRenderedFaces )
			drawInfo.m_pHardwareData->UpdateFacesRenderedCount( drawInfo.m_pStudioHdr, m_ModelFaceCountHash, nLod, nTimesRendered );
#endif // !_CERT


		int nBody = pCurInstance->m_nBody;
		for ( int body = 0; body < drawInfo.m_pStudioHdr->numbodyparts; ++body ) 
		{
			mstudiobodyparts_t  *pbodypart = drawInfo.m_pStudioHdr->pBodypart( body );

			int index = nBody / pbodypart->base;
			index = index % pbodypart->nummodels;
			mstudiomodel_t *pSubmodel = pbodypart->pModel( index );

			for ( int meshIndex = 0; meshIndex < pSubmodel->nummeshes; ++meshIndex )
			{
				mstudiomesh_t *pMesh = pSubmodel->pMesh(meshIndex);
				studiomeshdata_t *pMeshData = &pMeshDataBase[pMesh->meshid];
				nTotalMeshCount += pMeshData->m_NumGroup;
			}
		}
	}
	return nTotalMeshCount;
}


//-----------------------------------------------------------------------------
// Counts the number of meshes to draw
//-----------------------------------------------------------------------------
int CStudioRender::CountMeshesToDraw( const StudioModelArrayInfo2_t &drawInfo, int nCount, StudioArrayData_t *pArrayData, int nInstanceStride, int nTimesRendered )
{
	VPROF( "CStudioRender::CountMeshesToDraw" );

#ifndef _CERT
	bool bCountRenderedFaces = mat_rendered_faces_count.GetBool() || mat_print_top_model_vert_counts.GetBool();
#endif // !_CERT

	int nTotalMeshCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		StudioArrayData_t &arrayData = pArrayData[i];
		StudioArrayInstanceData_t *pCurInstance = (StudioArrayInstanceData_t*)( arrayData.m_pInstanceData );
		for ( int j = 0; j < arrayData.m_nCount; ++j, pCurInstance = (StudioArrayInstanceData_t*)( (char*)pCurInstance + nInstanceStride ) )
		{
			// This subarray has the same skin + body + lod
			int nLod = pCurInstance->m_nLOD;

			// get the studio mesh data for this lod
			studiomeshdata_t *pMeshDataBase = arrayData.m_pHardwareData->m_pLODs[nLod].m_pMeshData;

#ifndef _CERT
			// Each model counts how many rendered faces it accounts for each frame:
			if ( bCountRenderedFaces )
				arrayData.m_pHardwareData->UpdateFacesRenderedCount( arrayData.m_pStudioHdr, m_ModelFaceCountHash, nLod, nTimesRendered );
#endif // !_CERT

			int nBody = pCurInstance->m_nBody;
			for ( int body = 0; body < arrayData.m_pStudioHdr->numbodyparts; ++body ) 
			{
				mstudiobodyparts_t  *pbodypart = arrayData.m_pStudioHdr->pBodypart( body );

				int index = nBody / pbodypart->base;
				index = index % pbodypart->nummodels;
				mstudiomodel_t *pSubmodel = pbodypart->pModel( index );

				for ( int meshIndex = 0; meshIndex < pSubmodel->nummeshes; ++meshIndex )
				{
					mstudiomesh_t *pMesh = pSubmodel->pMesh(meshIndex);
					studiomeshdata_t *pMeshData = &pMeshDataBase[pMesh->meshid];
					nTotalMeshCount += pMeshData->m_NumGroup;
				}
			}
		}
	}
	return nTotalMeshCount;
}


//-----------------------------------------------------------------------------
// Sort models function
//-----------------------------------------------------------------------------
inline bool CStudioRender::SortLessFunc( const MeshRenderData_t &left, const MeshRenderData_t &right )
{
	if ( left.m_pMaterial != right.m_pMaterial )
		return left.m_pMaterial > right.m_pMaterial;
	if ( left.m_pGroup->m_pMesh != right.m_pGroup->m_pMesh )
		return left.m_pGroup->m_pMesh > right.m_pGroup->m_pMesh;
	if ( left.m_pInstance->m_pEnvCubemapTexture != right.m_pInstance->m_pEnvCubemapTexture )
		return left.m_pInstance->m_pEnvCubemapTexture > right.m_pInstance->m_pEnvCubemapTexture;
	bool bLeftHasLighting = ( left.m_pInstance->m_pLightingState != NULL );
	bool bRightHasLighting = ( right.m_pInstance->m_pLightingState != NULL );
	if ( bLeftHasLighting != bRightHasLighting )
		return bLeftHasLighting;
	if ( !bLeftHasLighting )
		return false;
	return left.m_pInstance->m_pLightingState->m_nLocalLightCount > right.m_pInstance->m_pLightingState->m_nLocalLightCount;
}


//-----------------------------------------------------------------------------
// Sort models function
//-----------------------------------------------------------------------------
inline bool CStudioRender::SortLessFunc2( const MeshRenderData2_t &left, const MeshRenderData2_t &right )
{
	if ( left.m_pMaterial != right.m_pMaterial )
		return left.m_pMaterial > right.m_pMaterial;
	if ( left.m_nCompressionType != right.m_nCompressionType )
		return left.m_nCompressionType > right.m_nCompressionType;
	if ( left.m_nMeshBoneCount != right.m_nMeshBoneCount )
		return left.m_nMeshBoneCount > right.m_nMeshBoneCount;
	bool bLeftHasColorMesh = ( left.m_pInstance->m_pColorMeshInfo != NULL );
	bool bRightHasColorMesh = ( right.m_pInstance->m_pColorMeshInfo != NULL );
	if ( bLeftHasColorMesh != bRightHasColorMesh )
		return bLeftHasColorMesh < bRightHasColorMesh;
	if ( left.m_pInstance->m_pEnvCubemapTexture != right.m_pInstance->m_pEnvCubemapTexture )
		return left.m_pInstance->m_pEnvCubemapTexture > right.m_pInstance->m_pEnvCubemapTexture;
	if ( left.m_pGroup->m_pMesh != right.m_pGroup->m_pMesh )
		return left.m_pGroup->m_pMesh > right.m_pGroup->m_pMesh;
	bool bLeftHasLighting = ( left.m_pInstance->m_pLightingState != NULL );
	bool bRightHasLighting = ( right.m_pInstance->m_pLightingState != NULL );
	if ( bLeftHasLighting != bRightHasLighting )
		return bLeftHasLighting;
	if ( !bLeftHasLighting )
		return false;
	return left.m_pInstance->m_pLightingState->m_nLocalLightCount > right.m_pInstance->m_pLightingState->m_nLocalLightCount;
}


//-----------------------------------------------------------------------------
// Builds the list of things to render in what order
//-----------------------------------------------------------------------------
int CStudioRender::BuildSortedRenderList( MeshRenderData_t *pRenderData, int *pTotalStripCount, const StudioModelArrayInfo_t &drawInfo, 
	int nCount, StudioArrayInstanceData_t *pInstanceData, int nInstanceStride, int nFlags )
{
	SNPROF( "CStudioRender::BuildSortedRenderList" );

	studiohdr_t *pStudioHdr = drawInfo.m_pStudioHdr;
	short *pSkinRefBase	= pStudioHdr->pSkinref( 0 );

	bool bSkipTranslucent = ( nFlags & STUDIORENDER_DRAW_OPAQUE_ONLY ) != 0;
	bool bSkipOpaque = ( nFlags & STUDIORENDER_DRAW_TRANSLUCENT_ONLY ) != 0;
	bool bSelectiveOverride = ( m_pRC->m_nForcedMaterialType == OVERRIDE_SELECTIVE );

	*pTotalStripCount = 0;
	StudioArrayInstanceData_t *pCurInstance = pInstanceData;
	int nRenderDataCount = 0;
	for ( int i = 0; i < nCount; ++i, pCurInstance = (StudioArrayInstanceData_t*)( (char*)pCurInstance + nInstanceStride ) )
	{
		// This subarray has the same skin + body + lod
		int nLod = pCurInstance->m_nLOD;

		// get the studio mesh data for this lod
		studiomeshdata_t *pMeshDataBase = drawInfo.m_pHardwareData->m_pLODs[nLod].m_pMeshData;
		IMaterial **ppMaterials = drawInfo.m_pHardwareData->m_pLODs[nLod].ppMaterials;

		short *pSkinRef = pSkinRefBase;
		int skin = pCurInstance->m_nSkin;
		if ( skin > 0 && skin < pStudioHdr->numskinfamilies )
		{
			pSkinRef += ( skin * pStudioHdr->numskinref );
		}

		int nBody = pCurInstance->m_nBody;
		for ( int body = 0; body < pStudioHdr->numbodyparts; ++body ) 
		{
			mstudiobodyparts_t  *pbodypart = pStudioHdr->pBodypart( body );

			int index = nBody / pbodypart->base;
			index = index % pbodypart->nummodels;
			mstudiomodel_t *pSubmodel = pbodypart->pModel( index );

			for ( int meshIndex = 0; meshIndex < pSubmodel->nummeshes; ++meshIndex )
			{
				mstudiomesh_t *pMesh = pSubmodel->pMesh(meshIndex);
				studiomeshdata_t *pMeshData = &pMeshDataBase[ pMesh->meshid ];
				IMaterial *pMaterial = ppMaterials[ pSkinRef[ pMesh->material ] ];
				bool bIsTranslucent = pMaterial->IsTranslucentUnderModulation( pCurInstance->m_DiffuseModulation.w );
				if ( bSkipTranslucent && bIsTranslucent )
					continue;
				else if ( bSkipOpaque && !bIsTranslucent )
					continue;

				Assert( pMeshData );
				// Assert( pMeshData->m_NumGroup ); // can't Assert on m_NumGroup since it can be zero on lods.

				for ( int g = 0; g < pMeshData->m_NumGroup; ++g )
				{
					studiomeshgroup_t* pGroup = &pMeshData->m_pMeshGroup[g];

					MeshRenderData_t &data = pRenderData[nRenderDataCount++];

					int nOverrideIndex = GetForcedMaterialOverrideIndex( pSkinRef[ pMesh->material ] );
					if ( bSelectiveOverride && nOverrideIndex != -1 )
					{
						data.m_pMaterial = m_pRC->m_pForcedMaterial[ nOverrideIndex ];
					}
					else
					{
						data.m_pMaterial = pMaterial;
					}
					data.m_pGroup = pGroup;
					data.m_pInstance = pCurInstance;
					data.m_pMesh = pMesh;

					*pTotalStripCount += pGroup->m_NumStrips;
				}
			}
		}
	}

	std::make_heap( pRenderData, pRenderData + nRenderDataCount, SortLessFunc ); 
	std::sort_heap( pRenderData, pRenderData + nRenderDataCount, SortLessFunc );
	return nRenderDataCount;
}


//-----------------------------------------------------------------------------
// Builds the list of things to render in what order
//-----------------------------------------------------------------------------
int CStudioRender::BuildSortedRenderList( MeshRenderData2_t *pRenderData, int *pTotalStripCount, const StudioModelArrayInfo2_t &drawInfo, 
	int nCount, StudioArrayData_t *pArrayData, int nInstanceStride, int nFlags )
{
	SNPROF( "CStudioRender::BuildSortedRenderList" );

	bool bSkipTranslucent = ( nFlags & STUDIORENDER_DRAW_OPAQUE_ONLY ) != 0;
	bool bSkipOpaque = ( nFlags & STUDIORENDER_DRAW_TRANSLUCENT_ONLY ) != 0;
	bool bSelectiveOverride = ( m_pRC->m_nForcedMaterialType == OVERRIDE_SELECTIVE );

	*pTotalStripCount = 0;
	int nRenderDataCount = 0;
	for ( int a = 0; a < nCount; ++a )
	{
		StudioArrayData_t &arrayData = pArrayData[a];
		studiohdr_t *pStudioHdr = arrayData.m_pStudioHdr;
		short *pSkinRefBase	= pStudioHdr->pSkinref( 0 );
		StudioArrayInstanceData_t *pCurInstance = (StudioArrayInstanceData_t*)( arrayData.m_pInstanceData );
		for ( int i = 0; i < arrayData.m_nCount; ++i, pCurInstance = (StudioArrayInstanceData_t*)( (char*)pCurInstance + nInstanceStride ) )
		{
			// This subarray has the same skin + body + lod
			int nLod = pCurInstance->m_nLOD;

			// get the studio mesh data for this lod
			studiomeshdata_t *pMeshDataBase = arrayData.m_pHardwareData->m_pLODs[nLod].m_pMeshData;
			IMaterial **ppMaterials = arrayData.m_pHardwareData->m_pLODs[nLod].ppMaterials;

			short *pSkinRef = pSkinRefBase;
			int skin = pCurInstance->m_nSkin;
			if ( skin > 0 && skin < pStudioHdr->numskinfamilies )
			{
				pSkinRef += ( skin * pStudioHdr->numskinref );
			}

			int nBody = pCurInstance->m_nBody;
			for ( int body = 0; body < pStudioHdr->numbodyparts; ++body ) 
			{
				mstudiobodyparts_t  *pbodypart = pStudioHdr->pBodypart( body );

				int index = nBody / pbodypart->base;
				index = index % pbodypart->nummodels;
				mstudiomodel_t *pSubmodel = pbodypart->pModel( index );

				for ( int meshIndex = 0; meshIndex < pSubmodel->nummeshes; ++meshIndex )
				{
					mstudiomesh_t *pMesh = pSubmodel->pMesh(meshIndex);
					studiomeshdata_t *pMeshData = &pMeshDataBase[ pMesh->meshid ];
					IMaterial *pMaterial = ppMaterials[ pSkinRef[ pMesh->material ] ];
					bool bIsTranslucent = pMaterial->IsTranslucentUnderModulation( pCurInstance->m_DiffuseModulation.w );
					if ( bSkipTranslucent && bIsTranslucent )
						continue;
					else if ( bSkipOpaque && !bIsTranslucent )
						continue;

					Assert( pMeshData );
					// Assert( pMeshData->m_NumGroup ); // can't Assert on m_NumGroup since it can be zero on lods.

					for ( int g = 0; g < pMeshData->m_NumGroup; ++g )
					{
						studiomeshgroup_t* pGroup = &pMeshData->m_pMeshGroup[g];

						MeshRenderData2_t &data = pRenderData[nRenderDataCount++];

						int nOverrideIndex = GetForcedMaterialOverrideIndex( pSkinRef[ pMesh->material ] );
						if ( bSelectiveOverride && nOverrideIndex != -1 )
						{
							data.m_pMaterial = m_pRC->m_pForcedMaterial[ nOverrideIndex ];
						}
						else
						{
							data.m_pMaterial = pMaterial;
						}
						data.m_pGroup = pGroup;
						data.m_pInstance = pCurInstance;
						data.m_pMesh = pMesh;
						data.m_nCompressionType = CompressionType( pGroup->m_pMesh->GetVertexFormat() );
						data.m_nMeshBoneCount = NumBoneWeights( pGroup->m_pMesh->GetVertexFormat() );

						data.m_pInstance->m_bColorMeshHasIndirectLightingOnly = ( pStudioHdr->flags & STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY ) ? true : false;

						*pTotalStripCount += pGroup->m_NumStrips;
					}
				}
			}
		}
	}

	std::make_heap( pRenderData, pRenderData + nRenderDataCount, SortLessFunc2 ); 
	std::sort_heap( pRenderData, pRenderData + nRenderDataCount, SortLessFunc2 );
	return nRenderDataCount;
}


//-----------------------------------------------------------------------------
// BuildForcedMaterialRenderList
//-----------------------------------------------------------------------------
void CStudioRender::BuildForcedMaterialRenderList( MeshRenderData_t *pRenderData, 
	int *pTotalStripCount, const StudioModelArrayInfo_t &drawInfo, const StudioRenderContext_t &rc, 
	int nCount, StudioArrayInstanceData_t *pInstanceData, int nInstanceStride )
{
	VPROF( "CStudioRender::BuildForcedMaterialRenderList" );

	studiohdr_t *pStudioHdr = drawInfo.m_pStudioHdr;

	*pTotalStripCount = 0;

	StudioArrayInstanceData_t *pCurInstance = pInstanceData;
	int nRenderDataCount = 0;
	for ( int i = 0; i < nCount; ++i, pCurInstance = (StudioArrayInstanceData_t*)( (char*)pCurInstance + nInstanceStride ) )
	{
		// This subarray has the same skin + body + lod
		int nLod = pCurInstance->m_nLOD;

		// get the studio mesh data for this lod
		studiomeshdata_t *pMeshDataBase = drawInfo.m_pHardwareData->m_pLODs[nLod].m_pMeshData;

		int nBody = pCurInstance->m_nBody;
		for ( int body = 0; body < pStudioHdr->numbodyparts; ++body ) 
		{
			mstudiobodyparts_t  *pbodypart = pStudioHdr->pBodypart( body );

			int index = nBody / pbodypart->base;
			index = index % pbodypart->nummodels;
			mstudiomodel_t *pSubmodel = pbodypart->pModel( index );

			for ( int meshIndex = 0; meshIndex < pSubmodel->nummeshes; ++meshIndex )
			{
				mstudiomesh_t *pMesh = pSubmodel->pMesh(meshIndex);
				studiomeshdata_t *pMeshData = &pMeshDataBase[pMesh->meshid];
				Assert( pMeshData && pMeshData->m_NumGroup );

				for ( int g = 0; g < pMeshData->m_NumGroup; ++g )
				{
					studiomeshgroup_t* pGroup = &pMeshData->m_pMeshGroup[g];

					MeshRenderData_t &data = pRenderData[nRenderDataCount++];
					data.m_pMaterial = rc.m_pForcedMaterial[ 0 ];
					data.m_pGroup = pGroup;
					data.m_pInstance = pCurInstance;
					data.m_pMesh = pMesh;

					*pTotalStripCount += pGroup->m_NumStrips;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// BuildForcedMaterialRenderList
//-----------------------------------------------------------------------------
void CStudioRender::BuildForcedMaterialRenderList( MeshRenderData2_t *pRenderData, 
	int *pTotalStripCount, const StudioModelArrayInfo2_t &drawInfo, const StudioRenderContext_t &rc, 
	int nCount, StudioArrayData_t *pArrayData, int nInstanceStride )
{
	VPROF( "CStudioRender::BuildForcedMaterialRenderList" );

	*pTotalStripCount = 0;

	int nRenderDataCount = 0;
	for ( int a = 0; a < nCount; ++a )
	{
		StudioArrayData_t &arrayData = pArrayData[a];
		studiohdr_t *pStudioHdr = arrayData.m_pStudioHdr;
		StudioArrayInstanceData_t *pCurInstance = (StudioArrayInstanceData_t*)( arrayData.m_pInstanceData );
		for ( int i = 0; i < arrayData.m_nCount; ++i, pCurInstance = (StudioArrayInstanceData_t*)( (char*)pCurInstance + nInstanceStride ) )
		{
			// This subarray has the same skin + body + lod
			int nLod = pCurInstance->m_nLOD;

			// get the studio mesh data for this lod
			studiomeshdata_t *pMeshDataBase = arrayData.m_pHardwareData->m_pLODs[nLod].m_pMeshData;

			int nBody = pCurInstance->m_nBody;
			for ( int body = 0; body < pStudioHdr->numbodyparts; ++body ) 
			{
				mstudiobodyparts_t  *pbodypart = pStudioHdr->pBodypart( body );

				int index = nBody / pbodypart->base;
				index = index % pbodypart->nummodels;
				mstudiomodel_t *pSubmodel = pbodypart->pModel( index );

				for ( int meshIndex = 0; meshIndex < pSubmodel->nummeshes; ++meshIndex )
				{
					mstudiomesh_t *pMesh = pSubmodel->pMesh(meshIndex);
					studiomeshdata_t *pMeshData = &pMeshDataBase[pMesh->meshid];
					Assert( pMeshData && pMeshData->m_NumGroup );

					for ( int g = 0; g < pMeshData->m_NumGroup; ++g )
					{
						studiomeshgroup_t* pGroup = &pMeshData->m_pMeshGroup[g];

						MeshRenderData2_t &data = pRenderData[nRenderDataCount++];
						data.m_pMaterial = rc.m_pForcedMaterial[ 0 ];
						data.m_pGroup = pGroup;
						data.m_pInstance = pCurInstance;
						data.m_pMesh = pMesh;
						data.m_nCompressionType = CompressionType( pGroup->m_pMesh->GetVertexFormat() );
						data.m_nMeshBoneCount = NumBoneWeights( pGroup->m_pMesh->GetVertexFormat() );

						*pTotalStripCount += pGroup->m_NumStrips;
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Restores meshes, if necessary
//-----------------------------------------------------------------------------
void CStudioRender::RestoreMeshes( int nCount, BaseMeshRenderData_t *pRenderData, int nStride )
{
#ifdef	IS_WINDOWS_PC

	// FIXME: Can we build a list of unique studiomeshdata_ts?
	for ( int i = 0; i < nCount; ++i, pRenderData = (BaseMeshRenderData_t*)( (unsigned char*)pRenderData + nStride ) )
	{
		BaseMeshRenderData_t &data = *pRenderData;

		studiomeshgroup_t *pGroup = data.m_pGroup;

		// Older models are merely flexed while new ones are also delta flexed
		Assert( !( pGroup->m_Flags & MESHGROUP_IS_DELTA_FLEXED ) );

		// Needed when we switch back and forth between hardware + software lighting
		if ( !pGroup->m_MeshNeedsRestore )
			continue;

		IMesh *pMesh = pGroup->m_pMesh;
		VertexCompressionType_t compressionType = CompressionType( pMesh->GetVertexFormat() );
		switch ( compressionType )
		{
		case VERTEX_COMPRESSION_ON:
			R_StudioRestoreMesh<VERTEX_COMPRESSION_ON>( data.m_pMesh, pGroup );
			break;
		case VERTEX_COMPRESSION_NONE:
		default:
			R_StudioRestoreMesh<VERTEX_COMPRESSION_NONE>( data.m_pMesh, pGroup );
			break;
		}
		pGroup->m_MeshNeedsRestore = false;
	}
#endif
}

//-----------------------------------------------------------------------------
// Allocate temporary arrays either on the stack, or from the heap. 
// Prevents using all the stack when *lots* of objects are rendered to CSM's.
//-----------------------------------------------------------------------------
#if defined( CSTRIKE15 ) // 7ls && !defined( _GAMECONSOLE )
	#define STUDIORENDER_TEMP_DATA_MALLOC( typeName, p, n ) const int nTempDataSize##p = (n); void *pvFree##p = NULL; typeName *p = (typeName *) ( ( nTempDataSize##p < 64*1024 ) ? stackalloc( nTempDataSize##p ) : ( pvFree##p = malloc( nTempDataSize##p ) ) );
	#define STUDIORENDER_TEMP_DATA_FREE( p ) free( pvFree##p )
#else
	#define STUDIORENDER_TEMP_DATA_MALLOC( typeName, p, n ) typeName *p = (typeName *) stackalloc(n);
	#define STUDIORENDER_TEMP_DATA_FREE( p )
#endif

//-----------------------------------------------------------------------------
// Draws meshes
//-----------------------------------------------------------------------------
void CStudioRender::DrawMeshRenderData( IMatRenderContext *pRenderContext, 
	const StudioModelArrayInfo_t &drawInfo, int nCount, MeshRenderData_t *pRenderData, int nTotalStripCount, int nFlashlightMask )
{
	SNPROF( "CStudioRender::DrawMeshRenderData" );

	int nInstanceCount = 0;
	STUDIORENDER_TEMP_DATA_MALLOC( MeshInstanceData_t, pInstance, nTotalStripCount * sizeof(MeshInstanceData_t) );
	IMaterial *pLastMaterial = NULL;
	IMesh *pLastMesh = NULL;
#ifdef _GAMECONSOLE
	bool bLastUsingFlashlight = false;
	bool bSavedFlashlightEnable = pRenderContext->GetFlashlightMode();
#endif // _GAMECONSOLE
	int nMaxBoneCount = 0;
	int nMaxLightCount = 0;
	bool bIsSkinned = drawInfo.m_pStudioHdr->numbones > 1;

#if PIX_ENABLE
	char szPIXEventName[128];
	sprintf( szPIXEventName, "%s*", drawInfo.m_pStudioHdr->name );	// PIX
	PIXEVENT( pRenderContext, szPIXEventName );
#endif
	for ( int i = 0; i < nCount; ++i )
	{
		MeshRenderData_t &data = pRenderData[i];
		StudioArrayInstanceData_t *pCurrInstance = data.m_pInstance;

		// Skip models not affected by this flashlight
		if ( nFlashlightMask && ( ( nFlashlightMask & pCurrInstance->m_nFlashlightUsage ) == 0 ) )
			continue;

		if ( ( pLastMaterial != data.m_pMaterial ) || ( pLastMesh != data.m_pGroup->m_pMesh ) 
#ifdef _GAMECONSOLE
			|| ( bLastUsingFlashlight != ( pCurrInstance->m_nFlashlightUsage != 0 ) ) 
#endif // _GAMECONSOLE
			)
		{
			if ( nInstanceCount > 0 )
			{
#ifdef _GAMECONSOLE
				if ( pRenderContext->IsCullingEnabledForSinglePassFlashlight() )
				{
					pRenderContext->SetFlashlightMode( bLastUsingFlashlight );
				}				
#endif // _GAMECONSOLE
				pRenderContext->SetNumBoneWeights( bIsSkinned ? nMaxBoneCount : 0 );
				pRenderContext->Bind( pLastMaterial, NULL );
				pRenderContext->DrawInstances( nInstanceCount, pInstance );
			}
			nInstanceCount = 0;
			nMaxBoneCount = 0;
			nMaxLightCount = 0;
			pLastMesh = data.m_pGroup->m_pMesh;
			pLastMaterial = data.m_pMaterial;
#ifdef _GAMECONSOLE
			bLastUsingFlashlight = pCurrInstance->m_nFlashlightUsage != 0;
#endif // _GAMECONSOLE
		}

		studiomeshgroup_t* pGroup = data.m_pGroup;
		for ( int j = 0; j < pGroup->m_NumStrips; ++j )
		{
			OptimizedModel::StripHeader_t* pStrip = &pGroup->m_pStripData[j];

			Assert( nInstanceCount < nTotalStripCount );

			MeshInstanceData_t &instance = pInstance[nInstanceCount++];
			instance.m_pEnvCubemap = pCurrInstance->m_pEnvCubemapTexture;
			instance.m_pPoseToWorld = pCurrInstance->m_pPoseToWorld;
			instance.m_pLightingState = pCurrInstance->m_pLightingState;
			if ( pCurrInstance->m_pColorMeshInfo )
			{
				const ColorMeshInfo_t &colorMesh = pCurrInstance->m_pColorMeshInfo[ pGroup->m_ColorMeshID ];
				instance.m_pColorBuffer = colorMesh.m_pMesh;
				instance.m_nColorVertexOffsetInBytes = colorMesh.m_nVertOffsetInBytes;
			}
			else
			{
				instance.m_pColorBuffer = NULL;
				instance.m_nColorVertexOffsetInBytes = 0;
			}
			instance.m_nBoneCount = pStrip->numBoneStateChanges;
			instance.m_pBoneRemap = ( instance.m_nBoneCount > 0 ) ? (MeshBoneRemap_t*)( pStrip->pBoneStateChange(0) ) : NULL;
			instance.m_nIndexOffset = pStrip->indexOffset;
			instance.m_nIndexCount = pStrip->numIndices;
			instance.m_nPrimType = MATERIAL_TRIANGLES;
			instance.m_pVertexBuffer = data.m_pGroup->m_pMesh;
			instance.m_pIndexBuffer = data.m_pGroup->m_pMesh;
			instance.m_nVertexOffsetInBytes = 0;
			instance.m_pStencilState = pCurrInstance->m_pStencilState;
			instance.m_DiffuseModulation = pCurrInstance->m_DiffuseModulation;
			instance.m_nLightmapPageId = MATERIAL_SYSTEM_LIGHTMAP_PAGE_INVALID;

			instance.m_bColorBufferHasIndirectLightingOnly = ( drawInfo.m_pStudioHdr->flags & STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY ) ? true : false;

			nMaxBoneCount = MAX( nMaxBoneCount, pStrip->numBones );
		}
	}

	if ( nInstanceCount > 0 )
	{
#ifdef _GAMECONSOLE
		if ( pRenderContext->IsCullingEnabledForSinglePassFlashlight() )
		{
			pRenderContext->SetFlashlightMode( bLastUsingFlashlight );
		}
#endif // _GAMECONSOLE
		pRenderContext->SetNumBoneWeights( bIsSkinned ? nMaxBoneCount : 0 );
		pRenderContext->Bind( pLastMaterial, NULL );
		pRenderContext->DrawInstances( nInstanceCount, pInstance );
	}

#ifdef _GAMECONSOLE
	pRenderContext->SetFlashlightMode( bSavedFlashlightEnable );
#endif // _GAMECONSOLE
	pRenderContext->SetNumBoneWeights( 0 );

	STUDIORENDER_TEMP_DATA_FREE( pInstance );
}


//-----------------------------------------------------------------------------
// Draws meshes
//-----------------------------------------------------------------------------
void CStudioRender::DrawMeshRenderData( IMatRenderContext *pRenderContext, 
	const StudioModelArrayInfo2_t &drawInfo, int nCount, MeshRenderData2_t *pRenderData, int nTotalStripCount, int nFlashlightMask )
{
	SNPROF( "CStudioRender::DrawMeshRenderData" );

	int nInstanceCount = 0;
	int nMaxBatchSize = IsGameConsole() ? CONSOLE_MAX_MODEL_FAST_PATH_BATCH_SIZE : nTotalStripCount;
	STUDIORENDER_TEMP_DATA_MALLOC( MeshInstanceData_t, pInstance, nMaxBatchSize * sizeof(MeshInstanceData_t) );
	IMaterial *pLastMaterial = NULL;
	int nMaxBoneCount = 0;
	int nMaxLightCount = 0;
	int nLastMeshBoneCount = 0;
	VertexCompressionType_t nLastCompressionType = VERTEX_COMPRESSION_INVALID;
	bool bLastMeshUsedColorMesh = false;
#ifdef _GAMECONSOLE
	bool bLastUsingFlashlight = false;
	bool bSavedFlashlightEnable = pRenderContext->GetFlashlightMode();
#endif // _GAMECONSOLE
	int nStartingStripIndex = 0; // used to interrupt batching within a group if the number of strips exceeds the max batch size
	
	for ( int i = 0; i < nCount; ++i )
	{
		MeshRenderData2_t &data = pRenderData[i];
		StudioArrayInstanceData_t *pCurrInstance = data.m_pInstance;

		// Skip models not affected by this flashlight
		if ( nFlashlightMask && ( ( nFlashlightMask & pCurrInstance->m_nFlashlightUsage ) == 0 ) )
			continue;

		bool bUsingColorMesh = ( pCurrInstance->m_pColorMeshInfo != NULL );
		if ( ( pLastMaterial != data.m_pMaterial ) ||					// shadow material is different
			( nLastCompressionType != ( int )data.m_nCompressionType ) ||		// compression type is different
			( nLastMeshBoneCount != data.m_nMeshBoneCount ) ||			// # of bones in the mesh data is different
			( bLastMeshUsedColorMesh != bUsingColorMesh ) || 			// Lighting type is different
			( IsGameConsole() && ( nInstanceCount >= nMaxBatchSize ) )	// max # of batches to render at once due to stack limitations on console
#ifdef _GAMECONSOLE
			|| ( bLastUsingFlashlight != ( pCurrInstance->m_nFlashlightUsage != 0 ) )
#endif // _GAMECONSOLE
			)
		{
			if ( nInstanceCount > 0 )
			{
#ifdef _GAMECONSOLE
				if ( pRenderContext->IsCullingEnabledForSinglePassFlashlight() )
				{
					pRenderContext->SetFlashlightMode( bLastUsingFlashlight );
				}
#endif // _GAMECONSOLE
				pRenderContext->SetNumBoneWeights( nLastMeshBoneCount > 0 ? nMaxBoneCount : 0 );
				pRenderContext->Bind( pLastMaterial, NULL );
				pRenderContext->DrawInstances( nInstanceCount, pInstance );
			}
			nInstanceCount = 0;
			nMaxBoneCount = 0;
			nMaxLightCount = 0;
			nLastCompressionType = ( VertexCompressionType_t )( int )data.m_nCompressionType;
			pLastMaterial = data.m_pMaterial;
			nLastMeshBoneCount = data.m_nMeshBoneCount;
			bLastMeshUsedColorMesh = bUsingColorMesh;
#ifdef _GAMECONSOLE
			bLastUsingFlashlight = pCurrInstance->m_nFlashlightUsage != 0;
#endif // _GAMECONSOLE
		}

		studiomeshgroup_t* pGroup = data.m_pGroup;
		int j;
		for ( j = nStartingStripIndex; j < pGroup->m_NumStrips; ++j )
		{
			OptimizedModel::StripHeader_t* pStrip = &pGroup->m_pStripData[j];

			MeshInstanceData_t &instance = pInstance[nInstanceCount++];
			instance.m_pEnvCubemap = pCurrInstance->m_pEnvCubemapTexture;
			instance.m_pPoseToWorld = pCurrInstance->m_pPoseToWorld;
			instance.m_pLightingState = pCurrInstance->m_pLightingState;
			if ( pCurrInstance->m_pColorMeshInfo )
			{
				const ColorMeshInfo_t &colorMesh = pCurrInstance->m_pColorMeshInfo[ pGroup->m_ColorMeshID ];
				instance.m_pColorBuffer = colorMesh.m_pMesh;
				instance.m_nColorVertexOffsetInBytes = colorMesh.m_nVertOffsetInBytes;
			}
			else
			{
				instance.m_pColorBuffer = NULL;
				instance.m_nColorVertexOffsetInBytes = 0;
			}
			instance.m_nBoneCount = pStrip->numBoneStateChanges;
			instance.m_pBoneRemap = ( instance.m_nBoneCount > 0 ) ? (MeshBoneRemap_t*)( pStrip->pBoneStateChange(0) ) : NULL;
			instance.m_nIndexOffset = pStrip->indexOffset;
			instance.m_nIndexCount = pStrip->numIndices;
			instance.m_nPrimType = MATERIAL_TRIANGLES;
			instance.m_pVertexBuffer = data.m_pGroup->m_pMesh;
			instance.m_pIndexBuffer = data.m_pGroup->m_pMesh;
			instance.m_nVertexOffsetInBytes = 0;
			instance.m_pStencilState = pCurrInstance->m_pStencilState;
			instance.m_DiffuseModulation = pCurrInstance->m_DiffuseModulation;
			instance.m_nLightmapPageId = MATERIAL_SYSTEM_LIGHTMAP_PAGE_INVALID;

			instance.m_bColorBufferHasIndirectLightingOnly = pCurrInstance->m_bColorMeshHasIndirectLightingOnly;

			nMaxBoneCount = MAX( nMaxBoneCount, pStrip->numBones );

			if ( IsGameConsole() && nInstanceCount >= nMaxBatchSize )
			{
				break;
			}
		}

		if ( IsGameConsole() && j < pGroup->m_NumStrips )
		{
			// We're going to have to process this pRenderData[] entry again,
			// but start iterating from a higher strip index next time.
			nStartingStripIndex = j + 1;
			-- i;
		}
		else
		{
			nStartingStripIndex = 0;
		}
	}

	if ( nInstanceCount > 0 )
	{
#ifdef _GAMECONSOLE
		if ( pRenderContext->IsCullingEnabledForSinglePassFlashlight() )
		{
			pRenderContext->SetFlashlightMode( bLastUsingFlashlight );
		}
#endif // _GAMECONSOLE
		pRenderContext->SetNumBoneWeights( nLastMeshBoneCount > 0 ? nMaxBoneCount : 0 );
		pRenderContext->Bind( pLastMaterial, NULL );
		pRenderContext->DrawInstances( nInstanceCount, pInstance );
	}

#ifdef _GAMECONSOLE
	pRenderContext->SetFlashlightMode( bSavedFlashlightEnable );
#endif // _GAMECONSOLE
	pRenderContext->SetNumBoneWeights( 0 );

	STUDIORENDER_TEMP_DATA_FREE( pInstance );
}


//-----------------------------------------------------------------------------
// Draws meshes illuminated by the flashlight
//-----------------------------------------------------------------------------
extern ConVar r_flashlightscissor;
void CStudioRender::DrawModelArrayFlashlight( IMatRenderContext *pRenderContext, const StudioModelArrayInfo_t &drawInfo, int nCount, MeshRenderData_t *pRenderData, int nTotalStripCount )
{
	int nFlashlightCount = drawInfo.m_nFlashlightCount;
	if ( !nFlashlightCount )
		return;

	pRenderContext->SetFlashlightMode( true );
	bool bDoScissor = r_flashlightscissor.GetBool() && ( pRenderContext->GetRenderTarget() == NULL );

	int i;
	for ( i = 0; i < nFlashlightCount; ++i )
	{
		FlashlightInstance_t &flashlight = drawInfo.m_pFlashlights[i];
		const FlashlightState_t& state = flashlight.m_FlashlightState;
		if ( bDoScissor )
		{
			if ( state.DoScissor() )
			{
				pRenderContext->PushScissorRect( state.GetLeft(), state.GetTop(), state.GetRight(), state.GetBottom() );
			}
		}

		if ( !flashlight.m_pDebugMaterial )
		{
			pRenderContext->SetFlashlightStateEx( state, flashlight.m_WorldToTexture, flashlight.m_pFlashlightDepthTexture );
			DrawMeshRenderData( pRenderContext, drawInfo, nCount, pRenderData, nTotalStripCount, ( 1 << i ) );
		}
		else
		{
			// Debugging mode where we render wireframe on models hit by the flashlight
			// FIXME: Could make this faster, but why bother? It's a debugging mode.
			int nSizeInBytes = nCount * sizeof(MeshRenderData_t);
			STUDIORENDER_TEMP_DATA_MALLOC( MeshRenderData_t, pTempData, nSizeInBytes );
			memcpy( pTempData, pRenderData, nSizeInBytes );
			for ( int j = 0; j < nCount; ++j )
			{
				pTempData[j].m_pMaterial = flashlight.m_pDebugMaterial;
			}
			pRenderContext->SetFlashlightMode( false );
			DrawMeshRenderData( pRenderContext, drawInfo, nCount, pTempData, nTotalStripCount, ( 1 << i ) );
			pRenderContext->SetFlashlightMode( true );

			STUDIORENDER_TEMP_DATA_FREE( pTempData );
		}
		
		if ( bDoScissor && state.DoScissor() )
		{
			pRenderContext->PopScissorRect();
		}
	}

	pRenderContext->SetFlashlightMode( false );
}


//-----------------------------------------------------------------------------
// Draws meshes illuminated by the flashlight
//-----------------------------------------------------------------------------
void CStudioRender::DrawModelArrayFlashlight( IMatRenderContext *pRenderContext, const StudioModelArrayInfo2_t &drawInfo, int nCount, MeshRenderData2_t *pRenderData, int nTotalStripCount )
{
	int nFlashlightCount = drawInfo.m_nFlashlightCount;
	if ( !nFlashlightCount )
		return;

	pRenderContext->SetFlashlightMode( true );
	bool bDoScissor = r_flashlightscissor.GetBool() && ( pRenderContext->GetRenderTarget() == NULL );

	int i;
	for ( i = 0; i < nFlashlightCount; ++i )
	{
		FlashlightInstance_t &flashlight = drawInfo.m_pFlashlights[i];
		const FlashlightState_t& state = flashlight.m_FlashlightState;
		if ( bDoScissor )
		{
			if ( state.DoScissor() )
			{
				pRenderContext->PushScissorRect( state.GetLeft(), state.GetTop(), state.GetRight(), state.GetBottom() );
			}
		}

		if ( !flashlight.m_pDebugMaterial )
		{
			pRenderContext->SetFlashlightStateEx( state, flashlight.m_WorldToTexture, flashlight.m_pFlashlightDepthTexture );
			DrawMeshRenderData( pRenderContext, drawInfo, nCount, pRenderData, nTotalStripCount, ( 1 << i ) );
		}
		else
		{
			// Debugging mode where we render wireframe on models hit by the flashlight
			// FIXME: Could make this faster, but why bother? It's a debugging mode.
			int nSizeInBytes = nCount * sizeof(MeshRenderData2_t);
			STUDIORENDER_TEMP_DATA_MALLOC( MeshRenderData2_t, pTempData, nSizeInBytes );
			memcpy( pTempData, pRenderData, nSizeInBytes );
			for ( int j = 0; j < nCount; ++j )
			{
				pTempData[j].m_pMaterial = flashlight.m_pDebugMaterial;
			}
			pRenderContext->SetFlashlightMode( false );
			DrawMeshRenderData( pRenderContext, drawInfo, nCount, pTempData, nTotalStripCount, ( 1 << i ) );
			pRenderContext->SetFlashlightMode( true );

			STUDIORENDER_TEMP_DATA_FREE( pTempData );
		}

		if ( bDoScissor && state.DoScissor() )
		{
			pRenderContext->PopScissorRect();
		}
	}

	pRenderContext->SetFlashlightMode( false );
}


//-----------------------------------------------------------------------------
// Draws decals illuminated by the flashlight
//-----------------------------------------------------------------------------
void CStudioRender::DrawModelArrayFlashlightDecals( IMatRenderContext *pRenderContext, studiohdr_t *pStudioHdr, int nFlashlightCount, FlashlightInstance_t *pFlashlights, int nCount, DecalRenderData_t *pRenderData )
{
	if ( !nFlashlightCount )
		return;

	pRenderContext->SetFlashlightMode( true );
	bool bDoScissor = r_flashlightscissor.GetBool() && ( pRenderContext->GetRenderTarget() == NULL );

	int i;
	for ( i = 0; i < nFlashlightCount; ++i )
	{
		FlashlightInstance_t &flashlight = pFlashlights[i];
		const FlashlightState_t& state = flashlight.m_FlashlightState;
		if ( bDoScissor )
		{
			if ( state.DoScissor() )
			{
				pRenderContext->PushScissorRect( state.GetLeft(), state.GetTop(), state.GetRight(), state.GetBottom() );
			}
		}

		if ( !flashlight.m_pDebugMaterial )
		{
			pRenderContext->SetFlashlightStateEx( state, flashlight.m_WorldToTexture, flashlight.m_pFlashlightDepthTexture );
			DrawModelArrayDecals( pRenderContext, pStudioHdr, nCount, pRenderData, ( 1 << i ) );
		}
		else
		{
			// Debugging mode where we render wireframe on models hit by the flashlight
			// FIXME: Could make this faster, but why bother? It's a debugging mode.
			int nSizeInBytes = nCount * sizeof(DecalRenderData_t);
			STUDIORENDER_TEMP_DATA_MALLOC( DecalRenderData_t, pTempData, nSizeInBytes );
			memcpy( pTempData, pRenderData, nSizeInBytes );
			for ( int j = 0; j < nCount; ++j )
			{
				pTempData[j].m_pRenderMaterial = flashlight.m_pDebugMaterial;
			}
			pRenderContext->SetFlashlightMode( false );
			DrawModelArrayDecals( pRenderContext, pStudioHdr, nCount, pTempData, ( 1 << i ) );
			pRenderContext->SetFlashlightMode( true );

			STUDIORENDER_TEMP_DATA_FREE( pTempData );
		}

		if ( bDoScissor && state.DoScissor() )
		{
			pRenderContext->PopScissorRect();
		}
	}

	pRenderContext->SetFlashlightMode( false );
}

int CStudioRender::CountDecalMeshesToDraw( int nCount, StudioArrayInstanceData_t *pInstanceData, int nInstanceStride )
{
	VPROF( "CStudioRender::CountDecalMeshesToDraw" );

	int nDecalMeshCount = 0;
	StudioArrayInstanceData_t *pCurInstance = pInstanceData;
	for ( int i = 0; i < nCount; ++i, pCurInstance = (StudioArrayInstanceData_t*)( (char*)pCurInstance + nInstanceStride ) )
	{
		StudioDecalHandle_t handle = pCurInstance->m_Decals;
		if ( handle == STUDIORENDER_DECAL_INVALID )
			continue;

		const DecalModelList_t& list = m_DecalList[(intp)handle];
		unsigned short mat = list.m_pLod[pCurInstance->m_nLOD].m_FirstMaterial;
		for ( ; mat != m_DecalMaterial.InvalidIndex(); mat = m_DecalMaterial.Next(mat))
		{
			++nDecalMeshCount;
		}
	}
	return nDecalMeshCount;
}



//-----------------------------------------------------------------------------
// Sort decals function
//-----------------------------------------------------------------------------
inline bool CStudioRender::SortDecalsLessFunc( const DecalRenderData_t &left, const DecalRenderData_t &right )
{
	if ( left.m_pDecalMaterial->m_pvProxyUserData != right.m_pDecalMaterial->m_pvProxyUserData )
	{
		int nUniqueID_Left = int( reinterpret_cast< uintp >( left.m_pDecalMaterial->m_pvProxyUserData ) ) & 0xFFFFFF;
		int nUniqueID_Right = int( reinterpret_cast< uintp >( right.m_pDecalMaterial->m_pvProxyUserData ) ) & 0xFFFFFF;
		if ( nUniqueID_Left != nUniqueID_Right )
		{
			if ( nUniqueID_Left && nUniqueID_Right )
				return nUniqueID_Left < nUniqueID_Right;
			else
				return nUniqueID_Left > nUniqueID_Right;	// if left is player decal, and right is non-proxied then true means to render player decal earlier
		}
	}
	if ( left.m_pDecalMaterial->m_pMaterial != right.m_pDecalMaterial->m_pMaterial )
		return left.m_pDecalMaterial->m_pMaterial > right.m_pDecalMaterial->m_pMaterial;
	if ( left.m_pInstance->m_pEnvCubemapTexture != right.m_pInstance->m_pEnvCubemapTexture )
		return left.m_pInstance->m_pEnvCubemapTexture > right.m_pInstance->m_pEnvCubemapTexture;
	bool bLeftHasLighting = ( left.m_pInstance->m_pLightingState != NULL );
	bool bRightHasLighting = ( right.m_pInstance->m_pLightingState != NULL );
	if ( bLeftHasLighting != bRightHasLighting )
		return bLeftHasLighting;
	if ( !bLeftHasLighting )
		return false;
	return left.m_pInstance->m_pLightingState->m_nLocalLightCount > right.m_pInstance->m_pLightingState->m_nLocalLightCount;
}


//-----------------------------------------------------------------------------
// Sorts decals to render
//-----------------------------------------------------------------------------
int CStudioRender::BuildSortedDecalRenderList( DecalRenderData_t *pDecalRenderData, int nCount, StudioArrayInstanceData_t *pInstanceData, int nInstanceStride )
{
	VPROF( "CStudioRender::BuildSortedDecalRenderList" );

	int nDecalMeshCount = 0;
	StudioArrayInstanceData_t *pCurInstance = pInstanceData;
	for ( int i = 0; i < nCount; ++i, pCurInstance = (StudioArrayInstanceData_t*)( (char*)pCurInstance + nInstanceStride ) )
	{
		StudioDecalHandle_t handle = pCurInstance->m_Decals;
		if ( handle == STUDIORENDER_DECAL_INVALID )
			continue;

		const DecalModelList_t& list = m_DecalList[(intp)handle];
		unsigned short mat = list.m_pLod[pCurInstance->m_nLOD].m_FirstMaterial;
		for ( ; mat != m_DecalMaterial.InvalidIndex(); mat = m_DecalMaterial.Next(mat))
		{
			DecalMaterial_t *pDecalMaterial = &m_DecalMaterial[mat];

			// It's possible for the index count to become zero due to decal retirement
			int indexCount = pDecalMaterial->m_Indices.Count();
			if ( indexCount == 0 )
				continue;

			DecalRenderData_t &decalRenderData = pDecalRenderData[nDecalMeshCount++];
			decalRenderData.m_pDecalMaterial = pDecalMaterial;
			decalRenderData.m_pInstance = pCurInstance;
			decalRenderData.m_pRenderMaterial = pDecalMaterial->m_pMaterial;
			decalRenderData.m_bIsVertexLit = pDecalMaterial->m_pMaterial->IsVertexLit();
		}
	}

	// Debug mode
	if ( m_pRC->m_Config.bWireframeDecals )
	{
		for ( int i = 0; i < nDecalMeshCount; ++i )
		{
			pDecalRenderData[i].m_pRenderMaterial = m_pMaterialWireframe[0][0]; // TODO: support displacement mapping
		}
	}

	std::make_heap( pDecalRenderData, pDecalRenderData + nDecalMeshCount, SortDecalsLessFunc ); 
	std::sort_heap( pDecalRenderData, pDecalRenderData + nDecalMeshCount, SortDecalsLessFunc );
	return nDecalMeshCount;
}


//-----------------------------------------------------------------------------
// Main entry point for drawing an array of instances
//-----------------------------------------------------------------------------
void CStudioRender::DrawModelArray( const StudioModelArrayInfo_t &drawInfo, const StudioRenderContext_t &rc, 
	int nCount, StudioArrayInstanceData_t *pInstanceData, int nInstanceStride, int nFlags )
{
	m_pRC = const_cast< StudioRenderContext_t* >( &rc );
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Preserve the matrices if we're skinning
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	// Count number of meshes per instance
	bool bDoShadows = ( ( nFlags & STUDIORENDER_DRAW_NO_SHADOWS ) == 0 );
	bool bSinglePassFlashlight = IsGameConsole();
	int nTimesRendered  = 1 + ( bSinglePassFlashlight ? 0 : ( bDoShadows ? drawInfo.m_nFlashlightCount : 1 ) );
	int nTotalMeshCount = CountMeshesToDraw( drawInfo, nCount, pInstanceData, nInstanceStride, nTimesRendered );

	// Build list of meshes to render
	int nTotalStripCount;
	STUDIORENDER_TEMP_DATA_MALLOC( MeshRenderData_t, pRenderData, nTotalMeshCount * sizeof(MeshRenderData_t) );
	if ( !rc.m_pForcedMaterial[ 0 ] || rc.m_nForcedMaterialType == OVERRIDE_SELECTIVE )
	{
		nTotalMeshCount = BuildSortedRenderList( pRenderData, &nTotalStripCount, drawInfo, nCount, pInstanceData, nInstanceStride, nFlags );
	}
	else
	{
		BuildForcedMaterialRenderList( pRenderData, &nTotalStripCount, drawInfo, rc, nCount, pInstanceData, nInstanceStride );
	}

	// Restore meshes, if necessary
	RestoreMeshes( nTotalMeshCount, pRenderData, sizeof(MeshRenderData_t) );

	// Draw, baby, draw!
	DrawMeshRenderData( pRenderContext, drawInfo, nTotalMeshCount, pRenderData, nTotalStripCount, 0 );

	// Draw all flashlights
	if ( bDoShadows && !bSinglePassFlashlight )
	{
		DrawModelArrayFlashlight( pRenderContext, drawInfo, nTotalMeshCount, pRenderData, nTotalStripCount );
	}

	// Count number of decals meshes per instance
	int nTotalDecalCount = CountDecalMeshesToDraw( nCount, pInstanceData, nInstanceStride );
	STUDIORENDER_TEMP_DATA_MALLOC( DecalRenderData_t, pDecalData, nTotalDecalCount * sizeof(DecalRenderData_t) );

	// Build list of decals to render
	nTotalDecalCount = BuildSortedDecalRenderList( pDecalData, nCount, pInstanceData, nInstanceStride );

	// Draw all decals
	DrawModelArrayDecals( pRenderContext, drawInfo.m_pStudioHdr, nTotalDecalCount, pDecalData, 0 );

	// Illuminate decals by the flashlight
	if ( bDoShadows && !bSinglePassFlashlight )
	{
		DrawModelArrayFlashlightDecals( pRenderContext, drawInfo.m_pStudioHdr, drawInfo.m_nFlashlightCount, drawInfo.m_pFlashlights, nTotalDecalCount, pDecalData );
	}

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
	pRenderContext->SetNumBoneWeights( 0 );

	STUDIORENDER_TEMP_DATA_FREE( pRenderData );
	STUDIORENDER_TEMP_DATA_FREE( pDecalData );
}


//-----------------------------------------------------------------------------
// More optimal version?
//-----------------------------------------------------------------------------
void CStudioRender::DrawModelArray2( const StudioModelArrayInfo2_t &drawInfo, const StudioRenderContext_t &rc, 
	int nCount, StudioArrayData_t *pArrayData, int nInstanceStride, int nFlags )
{
	m_pRC = const_cast< StudioRenderContext_t* >( &rc );
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Preserve the matrices if we're skinning
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	// Count number of meshes per instance
	bool bDoShadows = ( ( nFlags & STUDIORENDER_DRAW_NO_SHADOWS ) == 0 );
	bool bSinglePassFlashlight = IsGameConsole();
	int nTimesRendered  = 1 + ( bSinglePassFlashlight ? 0 : ( bDoShadows ? drawInfo.m_nFlashlightCount : 1 ) );
	int nTotalMeshCount = CountMeshesToDraw( drawInfo, nCount, pArrayData, nInstanceStride, nTimesRendered );

	// Build list of meshes to render
	int nTotalStripCount;
	STUDIORENDER_TEMP_DATA_MALLOC( MeshRenderData2_t, pRenderData, nTotalMeshCount * sizeof(MeshRenderData2_t) );
	if ( !rc.m_pForcedMaterial[ 0 ] || rc.m_nForcedMaterialType == OVERRIDE_SELECTIVE )
	{
		nTotalMeshCount = BuildSortedRenderList( pRenderData, &nTotalStripCount, drawInfo, nCount, pArrayData, nInstanceStride, nFlags );
	}
	else
	{
		BuildForcedMaterialRenderList( pRenderData, &nTotalStripCount, drawInfo, rc, nCount, pArrayData, nInstanceStride );
	}

	// Restore meshes, if necessary
	RestoreMeshes( nTotalMeshCount, pRenderData, sizeof(MeshRenderData2_t) );

	// Draw, baby, draw!
	DrawMeshRenderData( pRenderContext, drawInfo, nTotalMeshCount, pRenderData, nTotalStripCount, 0 );

	// Draw all flashlights
	if ( bDoShadows && !bSinglePassFlashlight )
	{
		DrawModelArrayFlashlight( pRenderContext, drawInfo, nTotalMeshCount, pRenderData, nTotalStripCount );
	}

	// Count number of decals meshes per instance
	for ( int i = 0; i < nCount; ++i )
	{
		StudioArrayData_t &arrayData = pArrayData[i];
		StudioArrayInstanceData_t *pGroupInstances = ( StudioArrayInstanceData_t * )( arrayData.m_pInstanceData );
		int nTotalDecalCount = CountDecalMeshesToDraw( arrayData.m_nCount, pGroupInstances, nInstanceStride );
		STUDIORENDER_TEMP_DATA_MALLOC( DecalRenderData_t, pDecalData, nTotalDecalCount * sizeof(DecalRenderData_t) );

		// Build list of decals to render
		nTotalDecalCount = BuildSortedDecalRenderList( pDecalData, arrayData.m_nCount, pGroupInstances, nInstanceStride );

		// Draw all decals
		DrawModelArrayDecals( pRenderContext, arrayData.m_pStudioHdr, nTotalDecalCount, pDecalData, 0 );

		// Illuminate decals by the flashlight
		if ( bDoShadows && !bSinglePassFlashlight )
		{
			DrawModelArrayFlashlightDecals( pRenderContext, arrayData.m_pStudioHdr, drawInfo.m_nFlashlightCount, drawInfo.m_pFlashlights, nTotalDecalCount, pDecalData );
		}
		
		STUDIORENDER_TEMP_DATA_FREE( pDecalData );
	}

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
	pRenderContext->SetNumBoneWeights( 0 );

	STUDIORENDER_TEMP_DATA_FREE( pRenderData );
}


//-----------------------------------------------------------------------------
// Counts the number of meshes to draw ( shadow rendering )
//-----------------------------------------------------------------------------
int CStudioRender::CountMeshesToDraw( int nCount, StudioArrayData_t *pShadowData, int nInstanceStride )
{
	VPROF( "CStudioRender::CountMeshesToDraw (shadow)" );

#ifndef _CERT
	bool bCountRenderedFaces = mat_rendered_faces_count.GetBool() || mat_print_top_model_vert_counts.GetBool();
#endif // !_CERT

	int nTotalMeshCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		StudioArrayData_t &shadow = pShadowData[i];
		StudioShadowArrayInstanceData_t *pCurInstance = ( StudioShadowArrayInstanceData_t* )shadow.m_pInstanceData;
		for ( int j = 0; j < shadow.m_nCount; ++j, pCurInstance = ( StudioShadowArrayInstanceData_t* )( (uint8*)pCurInstance + nInstanceStride ) )
		{
			// This subarray has the same skin + body + lod
			int nLod = pCurInstance->m_nLOD;

			// get the studio mesh data for this lod
			studiomeshdata_t *pMeshDataBase = shadow.m_pHardwareData->m_pLODs[nLod].m_pMeshData;

#ifndef _CERT
			// Each model counts how many rendered faces it accounts for each frame:
			if ( bCountRenderedFaces )
				shadow.m_pHardwareData->UpdateFacesRenderedCount( shadow.m_pStudioHdr, m_ModelFaceCountHash, nLod, 1 );
#endif // !_CERT

			int nBody = pCurInstance->m_nBody;
			for ( int body = 0; body < shadow.m_pStudioHdr->numbodyparts; ++body ) 
			{
				mstudiobodyparts_t  *pbodypart = shadow.m_pStudioHdr->pBodypart( body );

				int index = nBody / pbodypart->base;
				index = index % pbodypart->nummodels;
				mstudiomodel_t *pSubmodel = pbodypart->pModel( index );

				for ( int meshIndex = 0; meshIndex < pSubmodel->nummeshes; ++meshIndex )
				{
					mstudiomesh_t *pMesh = pSubmodel->pMesh(meshIndex);
					studiomeshdata_t *pMeshData = &pMeshDataBase[pMesh->meshid];
					nTotalMeshCount += pMeshData->m_NumGroup;
				}
			}
		}
	}
	return nTotalMeshCount;
}


//-----------------------------------------------------------------------------
// Sets up a depth write material  based on an initial material
//-----------------------------------------------------------------------------
void CStudioRender::GetDepthWriteMaterial( IMaterial** ppDepthMaterial, bool *pIsAlphaTested, bool *pUsesTreeSway, IMaterial *pSrcMaterial, bool bIsTranslucentUnderModulation, bool bIsSSAODepthWrite )
{
	static unsigned int originalTextureVarCache = 0;
	static unsigned int nOriginalTreeSwayVarCache = 0;

	IMaterialVar *pOriginalTextureVar = pSrcMaterial->FindVarFast( "$basetexture", &originalTextureVarCache );
	IMaterialVar *pOriginalTreeSwayVar = pSrcMaterial->FindVarFast( "$treesway", &nOriginalTreeSwayVarCache );

	// Select proper override material
	int nAlphaTest = (int) ( ( pSrcMaterial->IsAlphaTested() || bIsTranslucentUnderModulation ) && pOriginalTextureVar->IsTexture() ); // alpha tested base texture
	int nNoCull = (int) pSrcMaterial->IsTwoSided();
	int nTreeSway = (int) ( pOriginalTreeSwayVar && pOriginalTreeSwayVar->GetIntValue() );

	if ( !bIsSSAODepthWrite )
	{
		*ppDepthMaterial = m_pDepthWrite[ nAlphaTest ][ nNoCull ][ nTreeSway ];
	}
	else
	{
		*ppDepthMaterial = m_pSSAODepthWrite[ nAlphaTest ][ nNoCull ][ nTreeSway ];
	}
	*pIsAlphaTested = ( nAlphaTest != 0 );
	*pUsesTreeSway = ( nTreeSway != 0 );
}


//-----------------------------------------------------------------------------
// Sets up a depth write material  based on an initial material
//-----------------------------------------------------------------------------
void CStudioRender::SetupAlphaTestedDepthWrite( IMaterial* pDepthMaterial, IMaterial *pSrcMaterial )
{
	static unsigned int originalTextureVarCache = 0;
	IMaterialVar *pOriginalTextureVar = pSrcMaterial->FindVarFast( "$basetexture", &originalTextureVarCache );
	static unsigned int originalTextureFrameVarCache = 0;
	IMaterialVar *pOriginalTextureFrameVar = pSrcMaterial->FindVarFast( "$frame", &originalTextureFrameVarCache );
	static unsigned int originalAlphaRefCache = 0;
	IMaterialVar *pOriginalAlphaRefVar = pSrcMaterial->FindVarFast( "$AlphaTestReference", &originalAlphaRefCache );

	static unsigned int textureVarCache = 0;
	IMaterialVar *pTextureVar = pDepthMaterial->FindVarFast( "$basetexture", &textureVarCache );
	static unsigned int textureFrameVarCache = 0;
	IMaterialVar *pTextureFrameVar = pDepthMaterial->FindVarFast( "$frame", &textureFrameVarCache );
	static unsigned int alphaRefCache = 0;
	IMaterialVar *pAlphaRefVar = pDepthMaterial->FindVarFast( "$AlphaTestReference", &alphaRefCache );

	if ( pOriginalTextureVar->IsTexture() ) // If $basetexture is defined
	{
		if( pTextureVar && pOriginalTextureVar )
		{
			pTextureVar->SetTextureValue( pOriginalTextureVar->GetTextureValue() );
		}

		if( pTextureFrameVar && pOriginalTextureFrameVar )
		{
			pTextureFrameVar->SetIntValue( pOriginalTextureFrameVar->GetIntValue() );
		}

		if( pAlphaRefVar && pOriginalAlphaRefVar )
		{
			pAlphaRefVar->SetFloatValue( pOriginalAlphaRefVar->GetFloatValue() );
		}
	}
}


//-----------------------------------------------------------------------------
// Sets up a depth write material  based on an initial material
//-----------------------------------------------------------------------------
void CStudioRender::SetupTreeSwayDepthWrite( IMaterial* pDepthMaterial, IMaterial *pSrcMaterial )
{
	static const char* paramNames[14] =
	{
		"$treeswayheight",
		"$treeswaystartheight",
		"$treeswayradius",
		"$treeswaystartradius",
		"$treeswayspeed",
		"$treeswayspeedhighwindmultiplier",
		"$treeswaystrength",
		"$treeswayscrumblespeed",
		"$treeswayscrumblestrength",
		"$treeswayscrumblefrequency",
		"$treeswayfalloffexp",
		"$treeswayscrumblefalloffexp",
		"$treeswayspeedlerpstart",
		"$treeswayspeedlerpend",
	};
	static unsigned int originalVarCache[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	static unsigned int varCache[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	IMaterialVar *pOriginalVar = NULL;
	IMaterialVar *pVar = NULL;

	for ( int32 i = 0; i < ARRAYSIZE( paramNames ); i++ )
	{
		pOriginalVar = pSrcMaterial->FindVarFast( paramNames[i], originalVarCache + i );
		pVar = pDepthMaterial->FindVarFast( paramNames[i], varCache + i );
		if( pOriginalVar && pVar )
		{
			pVar->SetFloatValue( pOriginalVar->GetFloatValue() );
		}
	}
}


//-----------------------------------------------------------------------------
// Sort models function
//-----------------------------------------------------------------------------
inline bool CStudioRender::ShadowSortLessFunc( const ShadowMeshRenderData_t &left, const ShadowMeshRenderData_t &right )
{
	if ( left.m_pMaterial != right.m_pMaterial )
		return left.m_pMaterial > right.m_pMaterial;
	if ( left.m_bIsAlphaTested && right.m_bIsAlphaTested )
	{
		if ( left.m_pSrcMaterial != right.m_pSrcMaterial )
			return left.m_pSrcMaterial > right.m_pSrcMaterial;
	}
	if ( left.m_bUsesTreeSway && right.m_bUsesTreeSway )
	{
		if ( left.m_pSrcMaterial != right.m_pSrcMaterial )
			return left.m_pSrcMaterial > right.m_pSrcMaterial;
	}
	if ( left.m_nCompressionType != right.m_nCompressionType )
		return left.m_nCompressionType > right.m_nCompressionType;
	if ( left.m_nMeshBoneCount != right.m_nMeshBoneCount )
		return left.m_nMeshBoneCount > right.m_nMeshBoneCount;
	return left.m_pGroup->m_pMesh > right.m_pGroup->m_pMesh;
}


//-----------------------------------------------------------------------------
// BuildShadowRenderList
//-----------------------------------------------------------------------------
int CStudioRender::BuildShadowRenderList( ShadowMeshRenderData_t *pRenderData, int *pTotalStripCount, 
	int nCount, StudioArrayData_t *pShadowData, int nInstanceStride, int flags )
{
	VPROF( "CStudioRender::BuildShadowRenderList" );

	*pTotalStripCount = 0;
	int nRenderDataCount = 0;
	for ( int i = 0; i < nCount; ++i )
	{
		StudioArrayData_t &shadow = pShadowData[i];
		studiohdr_t *pStudioHdr = shadow.m_pStudioHdr;
		short *pSkinRefBase	= pStudioHdr->pSkinref( 0 );
		StudioShadowArrayInstanceData_t *pCurInstance = ( StudioShadowArrayInstanceData_t* )shadow.m_pInstanceData;
		for ( int j = 0; j < shadow.m_nCount; ++j, pCurInstance = ( StudioShadowArrayInstanceData_t* )( (uint8*)pCurInstance + nInstanceStride ) )
		{
			// This subarray has the same skin + body + lod
			int nLod = pCurInstance->m_nLOD;

			// get the studio mesh data for this lod
			studiomeshdata_t *pMeshDataBase = shadow.m_pHardwareData->m_pLODs[nLod].m_pMeshData;
			IMaterial **ppMaterials = shadow.m_pHardwareData->m_pLODs[nLod].ppMaterials;

			short *pSkinRef = pSkinRefBase;
			int skin = pCurInstance->m_nSkin;
			if ( skin > 0 && skin < pStudioHdr->numskinfamilies )
			{
				pSkinRef += ( skin * pStudioHdr->numskinref );
			}

			int nBody = pCurInstance->m_nBody;
			for ( int body = 0; body < pStudioHdr->numbodyparts; ++body ) 
			{
				mstudiobodyparts_t  *pbodypart = pStudioHdr->pBodypart( body );

				int index = nBody / pbodypart->base;
				index = index % pbodypart->nummodels;
				mstudiomodel_t *pSubmodel = pbodypart->pModel( index );

				for ( int meshIndex = 0; meshIndex < pSubmodel->nummeshes; ++meshIndex )
				{
					mstudiomesh_t *pMesh = pSubmodel->pMesh(meshIndex);
					studiomeshdata_t *pMeshData = &pMeshDataBase[pMesh->meshid];
					Assert( pMeshData );
					//Assert( pMeshData->m_NumGroup ); // Can't assume that this is non-zero for lods with removed meshes.

					IMaterial *pMaterial = ppMaterials[ pSkinRef[ pMesh->material ] ];

					// Used on cstrike15 to efficiently render translucent renderables into csm shadow buffers
					if ( ( flags & STUDIORENDER_SHADOWDEPTHTEXTURE_INCLUDE_TRANSLUCENT_MATERIALS ) == 0 )
					{
						// Bail if the material is still considered translucent after setting the AlphaModulate to 1.0
						if ( pMaterial->IsTranslucentUnderModulation() )
							continue;
					}

					IMaterial *pDepthMaterial;
					bool bIsAlphaTested = false;
					bool bUsesTreeSway = false;
					GetDepthWriteMaterial( &pDepthMaterial, &bIsAlphaTested, &bUsesTreeSway, pMaterial, pMaterial->IsTranslucentUnderModulation(), ( flags & STUDIORENDER_SSAODEPTHTEXTURE ) ? true : false );

					for ( int g = 0; g < pMeshData->m_NumGroup; ++g )
					{
						studiomeshgroup_t* pGroup = &pMeshData->m_pMeshGroup[g];
						VertexCompressionType_t nCompressionType = CompressionType( pGroup->m_pMesh->GetVertexFormat() );

						ShadowMeshRenderData_t &data = pRenderData[nRenderDataCount++];
						data.m_pGroup = pGroup;
						data.m_pInstance = pCurInstance;
						data.m_pMesh = pMesh;
						data.m_pMaterial = pDepthMaterial;
						data.m_pSrcMaterial = pMaterial;
						data.m_nCompressionType = nCompressionType;
						data.m_nMeshBoneCount = NumBoneWeights( pGroup->m_pMesh->GetVertexFormat() );
						data.m_bIsAlphaTested = bIsAlphaTested;
						data.m_bUsesTreeSway = bUsesTreeSway;

						*pTotalStripCount += pGroup->m_NumStrips;
					}
				}
			}
		}
	}

	std::make_heap( pRenderData, pRenderData + nRenderDataCount, ShadowSortLessFunc ); 
	std::sort_heap( pRenderData, pRenderData + nRenderDataCount, ShadowSortLessFunc );
	return nRenderDataCount;
}


//-----------------------------------------------------------------------------
// Draws shadow meshes
//-----------------------------------------------------------------------------
void CStudioRender::DrawShadowMeshRenderData( IMatRenderContext *pRenderContext, 
	int nCount, ShadowMeshRenderData_t *pRenderData, int nTotalStripCount )
{
	VPROF( "CStudioRender::DrawShadowMeshRenderData" );

	int nInstanceCount = 0;
	STUDIORENDER_TEMP_DATA_MALLOC( MeshInstanceData_t, pInstance, nTotalStripCount * sizeof(MeshInstanceData_t) );
	int nMaxBoneCount = 0;
	int nLastMeshBoneCount = 0;
	IMaterial *pLastMaterial = NULL;
	IMaterial *pLastSrcMaterial = NULL;
	bool bIsAlphaTested = false;
	bool bUsesTreeSway = false;
	VertexCompressionType_t nLastCompressionType = VERTEX_COMPRESSION_INVALID;
	for ( int i = 0; i < nCount; ++i )
	{
		ShadowMeshRenderData_t &data = pRenderData[i];
		StudioShadowArrayInstanceData_t *pCurrInstance = data.m_pInstance;

		if ( ( pLastMaterial != data.m_pMaterial ) ||					// shadow material is different
			 ( bIsAlphaTested && ( pLastSrcMaterial != data.m_pSrcMaterial ) ) || // alpha channel is different
			 ( bUsesTreeSway && ( pLastSrcMaterial != data.m_pSrcMaterial ) ) || // tree sway params are different
			 ( nLastCompressionType != data.m_nCompressionType ) ||		// compression type is different
			 ( nLastMeshBoneCount != data.m_nMeshBoneCount ) )			// # of bones in the mesh data is different
		{
			if ( nInstanceCount > 0 )
			{
				if ( bIsAlphaTested )
				{
					SetupAlphaTestedDepthWrite( pLastMaterial, pLastSrcMaterial );
				}
				if ( bUsesTreeSway )
				{
					SetupTreeSwayDepthWrite( pLastMaterial, pLastSrcMaterial );
				}

				pRenderContext->SetNumBoneWeights( nLastMeshBoneCount > 0 ? nMaxBoneCount : 0 );
				pRenderContext->Bind( pLastMaterial, NULL );
				pRenderContext->DrawInstances( nInstanceCount, pInstance );
			}
			nInstanceCount = 0;
			nMaxBoneCount = 0;
			nLastCompressionType = data.m_nCompressionType;
			pLastMaterial = data.m_pMaterial;
			pLastSrcMaterial = data.m_pSrcMaterial;
			nLastMeshBoneCount = data.m_nMeshBoneCount;
			bIsAlphaTested = data.m_bIsAlphaTested;
			bUsesTreeSway = data.m_bUsesTreeSway;
		}

		studiomeshgroup_t* pGroup = data.m_pGroup;
		for ( int j = 0; j < pGroup->m_NumStrips; ++j )
		{
			OptimizedModel::StripHeader_t* pStrip = &pGroup->m_pStripData[j];

			Assert( nInstanceCount < nTotalStripCount );

			MeshInstanceData_t &instance = pInstance[nInstanceCount++];
			instance.m_pEnvCubemap = NULL;
			instance.m_pPoseToWorld = pCurrInstance->m_pPoseToWorld;
			instance.m_pLightingState = NULL;
			instance.m_nBoneCount = pStrip->numBoneStateChanges;
			instance.m_pBoneRemap = ( instance.m_nBoneCount > 0 ) ? (MeshBoneRemap_t*)( pStrip->pBoneStateChange(0) ) : NULL;
			instance.m_nIndexOffset = pStrip->indexOffset;
			instance.m_nIndexCount = pStrip->numIndices;
			instance.m_nPrimType = MATERIAL_TRIANGLES;
			instance.m_pColorBuffer = NULL;
			instance.m_nColorVertexOffsetInBytes = 0;
			instance.m_pStencilState = NULL;
			instance.m_pVertexBuffer = pGroup->m_pMesh;
			instance.m_pIndexBuffer = pGroup->m_pMesh;
			instance.m_nVertexOffsetInBytes = 0;
			instance.m_DiffuseModulation.Init( 1.0f, 1.0f, 1.0f, 1.0f );
			instance.m_nLightmapPageId = MATERIAL_SYSTEM_LIGHTMAP_PAGE_INVALID;
			instance.m_bColorBufferHasIndirectLightingOnly = false;
			nMaxBoneCount = MAX( nMaxBoneCount, pStrip->numBones );
		}
	}

	if ( nInstanceCount > 0 )
	{
		if ( bIsAlphaTested )
		{
			SetupAlphaTestedDepthWrite( pLastMaterial, pLastSrcMaterial );
		}
		if ( bUsesTreeSway )
		{
			SetupTreeSwayDepthWrite( pLastMaterial, pLastSrcMaterial );
		}
		pRenderContext->SetNumBoneWeights( nLastMeshBoneCount > 0 ? nMaxBoneCount : 0 );
		pRenderContext->Bind( pLastMaterial, NULL );
		pRenderContext->DrawInstances( nInstanceCount, pInstance );
	}

	pRenderContext->SetNumBoneWeights( 0 );

	STUDIORENDER_TEMP_DATA_FREE( pInstance );
}


//-----------------------------------------------------------------------------
// Draws all models to the shadow depth buffer
//-----------------------------------------------------------------------------
void CStudioRender::DrawModelShadowArray( const StudioRenderContext_t &rc, int nCount, StudioArrayData_t *pShadowData, int nInstanceStride, int flags )
{
	m_pRC = const_cast< StudioRenderContext_t* >( &rc );
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Preserve the matrices if we're skinning
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	// Count number of meshes to draw
	int nTotalMeshCount = CountMeshesToDraw( nCount, pShadowData, nInstanceStride );

	// Build list of meshes to render
	int nTotalStripCount;

	STUDIORENDER_TEMP_DATA_MALLOC( ShadowMeshRenderData_t, pRenderData, nTotalMeshCount * sizeof(ShadowMeshRenderData_t) );

	nTotalMeshCount = BuildShadowRenderList( pRenderData, &nTotalStripCount, nCount, pShadowData, nInstanceStride, flags );

	// Restore meshes, if necessary
	RestoreMeshes( nTotalMeshCount, pRenderData, sizeof(ShadowMeshRenderData_t) );

	// Draw, baby, draw!
	DrawShadowMeshRenderData( pRenderContext, nTotalMeshCount, pRenderData, nTotalStripCount );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
	pRenderContext->SetNumBoneWeights( 0 );

	STUDIORENDER_TEMP_DATA_FREE( pRenderData );
}


void CStudioRender::ComputeDiffuseModulation( Vector4D *pDiffuseModulation )
{
	if ( ( m_pRC->m_nForcedMaterialType != OVERRIDE_DEPTH_WRITE ) && ( m_pRC->m_nForcedMaterialType != OVERRIDE_SSAO_DEPTH_WRITE ) )
	{
		pDiffuseModulation->Init( m_pRC->m_ColorMod[0], m_pRC->m_ColorMod[1], m_pRC->m_ColorMod[2], m_pRC->m_AlphaMod ); 
	}
	else
	{
		pDiffuseModulation->Init( 1.0f, 1.0f, 1.0f, 1.0f );
	}
}



// this is a fast path that does not support debug modes or transparency (or skeletons obviously)
// pass in an array of instances and draw them

void CStudioRender::DrawModelArrayStaticProp( const DrawModelInfo_t& info, 
											 const StudioRenderContext_t &rc, int nInstanceCount, const MeshInstanceData_t *pInstanceData, ColorMeshInfo_t **pColorMeshes )
{
	VPROF( "CStudioRender::DrawModelArrayStaticProp");

	m_pRC = const_cast<StudioRenderContext_t*>( &rc );
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetNumBoneWeights( 0 );
	pRenderContext->MatrixMode( MATERIAL_MODEL );

	// this is needed by R_SetupSkinAndLighting
	bool flexConfig = m_pRC->m_Config.bFlex;
	m_pRC->m_Config.bFlex = false;
	bool bWireframe = m_pRC->m_Config.bWireframe;
	m_pRC->m_Config.bWireframe = false;

	m_bSkippedMeshes = false;
	m_bDrawTranslucentSubModels = false;

	int lod = info.m_Lod;
	m_pStudioHdr = info.m_pStudioHdr;
	m_pStudioMeshes = info.m_pHardwareData->m_pLODs[lod].m_pMeshData;
	m_pStudioHWData = info.m_pHardwareData;

#if PIX_ENABLE
	char szPIXEventName[128];
	sprintf( szPIXEventName, "%s*", m_pStudioHdr->name );	// PIX
	PIXEVENT( pRenderContext, szPIXEventName );
#endif
	// Build list of submodels
	Assert(m_pStudioHdr->numbodyparts==1);
	mstudiobodyparts_t *pbodypart = m_pStudioHdr->pBodypart( 0 );
	m_pSubModel = pbodypart->pModel( 0 );

	// get skinref array
	int skin = info.m_Skin;
	int *pMaterialFlags = info.m_pHardwareData->m_pLODs[lod].pMaterialFlags;
	short *pskinref	= m_pStudioHdr->pSkinref( 0 );
	if ( skin > 0 && skin < m_pStudioHdr->numskinfamilies )
	{
		pskinref += ( skin * m_pStudioHdr->numskinref );
	}

#ifndef _CERT
	int nFacesPerModel = 0;
#endif // !_CERT

	// draw each mesh
	for ( int i = 0; i < m_pSubModel->nummeshes; ++i)
	{
		mstudiomesh_t *pmesh = m_pSubModel->pMesh(i);
		studiomeshdata_t *pMeshData = &m_pStudioMeshes[pmesh->meshid];
		Assert( pMeshData );

		if ( !pMeshData->m_NumGroup )
			continue;

		if ( !pMaterialFlags )
			continue;

		int materialFlags = pMaterialFlags[pskinref[pmesh->material]];
		StudioModelLighting_t lighting = LIGHTING_HARDWARE;
		IMaterial* pMaterial = R_StudioSetupSkinAndLighting( pRenderContext, pskinref[ pmesh->material ], info.m_pHardwareData->m_pLODs[lod].ppMaterials, materialFlags, info.m_pClientEntity, info.m_pColorMeshes, lighting );
		if ( !pMaterial )
			continue;

		// if this fails you've got a static prop with an eyeball!
		Assert( pmesh->materialtype != 1 );
		for ( int j = 0; j < pMeshData->m_NumGroup; ++j )
		{
			studiomeshgroup_t* pGroup = &pMeshData->m_pMeshGroup[j];
			// Needed when we switch back and forth between hardware + software lighting
#ifdef IS_WINDOWS_PC
			if ( IsPC() && pGroup->m_MeshNeedsRestore )
			{
				VertexCompressionType_t compressionType = CompressionType( pGroup->m_pMesh->GetVertexFormat() );
				switch ( compressionType )
				{
				case VERTEX_COMPRESSION_ON:
					R_StudioRestoreMesh<VERTEX_COMPRESSION_ON>( pmesh, pGroup );
				case VERTEX_COMPRESSION_NONE:
				default:
					R_StudioRestoreMesh<VERTEX_COMPRESSION_NONE>( pmesh, pGroup );
					break;
				}
				pGroup->m_MeshNeedsRestore = false;
			}
#endif
			IMesh *pMesh = pGroup->m_pMesh;
			for ( int k = 0; k < nInstanceCount; k++ )
			{
				const MeshInstanceData_t &instance = pInstanceData[k];
				if ( instance.m_pEnvCubemap )
				{
					pRenderContext->BindLocalCubemap( const_cast<ITexture *>( instance.m_pEnvCubemap ) );
				}
				if ( pColorMeshes[k] )
				{
					pMesh->SetColorMesh( pColorMeshes[k][pGroup->m_ColorMeshID].m_pMesh, pColorMeshes[k][pGroup->m_ColorMeshID].m_nVertOffsetInBytes );
				}
				else
				{
					pMesh->SetColorMesh( NULL, 0 );
				}
				pRenderContext->LoadMatrix( *instance.m_pPoseToWorld );

				for ( int strip = 0; strip < pGroup->m_NumStrips; ++strip )
				{
					OptimizedModel::StripHeader_t* pStrip = &pGroup->m_pStripData[strip];
					pMesh->SetPrimitiveType( GetPrimitiveTypeForStripHeaderFlags( pStrip->flags ) );
					pMesh->DrawModulated( instance.m_DiffuseModulation, pStrip->indexOffset, pStrip->numIndices );

#ifndef _CERT
					// Count # faces per instance for the first instance only
					if ( k == 0 )
					{
						// This code does not work with SubD but we're not really using that anyways
						Assert( GetPrimitiveTypeForStripHeaderFlags( pStrip->flags ) == MATERIAL_TRIANGLES );
						nFacesPerModel += pStrip->numIndices / 3;
					}
#endif // !_CERT
				}
			}
			pMesh->SetColorMesh( NULL, 0 );
		}
	}

#ifndef _CERT
	if ( mat_rendered_faces_count.GetBool() || mat_print_top_model_vert_counts.GetBool() )
	{
		// Each model counts how many rendered faces it accounts for each frame:
		m_pStudioHWData->UpdateFacesRenderedCount( m_pStudioHdr, m_ModelFaceCountHash, lod, nInstanceCount, nFacesPerModel );
	}
#endif // !_CERT


	// Restore the configs
	m_pRC->m_Config.bFlex = flexConfig;
	m_pRC->m_Config.bWireframe = bWireframe;

	m_pRC = NULL;
	m_pStudioHdr = NULL;
	m_pStudioMeshes = NULL;
	m_pStudioHWData = NULL;
}

#ifndef _CERT


bool FacesRenderedInfoCompareFunc( IStudioRender::FacesRenderedInfo_t const &a, IStudioRender::FacesRenderedInfo_t const &b ) { return a.pStudioHdr == b.pStudioHdr; }
uint32 FacesRenderedInfoKeyFunc( IStudioRender::FacesRenderedInfo_t const &a ) { return HashIntConventional( (int32)(intp)a.pStudioHdr ); }

int FacesRenderedInfoSort( const void *a, const void *b )
{
	IStudioRender::FacesRenderedInfo_t *A = (IStudioRender::FacesRenderedInfo_t *)a, *B = (IStudioRender::FacesRenderedInfo_t *)b;
	return ( A->nFaceCount > B->nFaceCount ) ? -1 : +1;
}

void UpdateAndSpewFacesRenderedHistory( CUtlVector< IStudioRender::FacesRenderedInfo_t > &newItems, int nTotal, int nSpewFromCurrentFrame, int nSpewFromHistory, bool bClearHistory )
{
	static const int NUM_ITEMS_TO_TRACK  = 20;
	static const int NUM_FRAMES_TO_TRACK = 20;
	static IStudioRender::FacesRenderedInfo_t history[ NUM_FRAMES_TO_TRACK ][ NUM_ITEMS_TO_TRACK ];
	static int nItems = 0, nOldestItem = 0;
	IStudioRender::FacesRenderedInfo_t emptyItem = { NULL, 0 };

	if ( bClearHistory )
	{
		nItems = nOldestItem = 0;
		return;
	}
	if ( !nTotal )
		return;

	// Record the top 'NUM_ITEMS_TO_TRACK' models for the last rendered frame:
	int nCurItem = ( nOldestItem + nItems ) % NUM_FRAMES_TO_TRACK;
	if ( nItems == NUM_FRAMES_TO_TRACK )
		nOldestItem = ( nOldestItem + 1 ) % NUM_FRAMES_TO_TRACK;
	nItems = MIN( ( nItems + 1 ), NUM_FRAMES_TO_TRACK );
	for ( int i = 0; i < NUM_ITEMS_TO_TRACK; i++ )
	{
		history[ nCurItem ][ i ] = ( i < newItems.Count() ) ? newItems[ i ] : emptyItem;
	}

	nSpewFromCurrentFrame = MIN( nSpewFromCurrentFrame, newItems.Count() );
	if ( nSpewFromCurrentFrame )
	{
		// Spew the top N offenders from this frame to the console
		Msg( "Faces rendered this frame, by model:\n" );
		for ( int i = 0; i < nSpewFromCurrentFrame; i++ )
		{
			Msg( "%-7d (%-3d times, %-7d avg) %s\n", newItems[ i ].nFaceCount, newItems[ i ].nRenderCount, ( int )( ( float )newItems[ i ].nFaceCount / ( float )newItems[ i ].nRenderCount ), newItems[ i ].pStudioHdr->name );
		}
	}
	else
	{
		static float lastSpewTime = 0.0f;
		if ( Plat_FloatTime() < ( lastSpewTime + 0.25f ) )
			return;
		lastSpewTime = Plat_FloatTime();

		// Spew the N most expensive models over the last 'NUM_FRAMES_TO_TRACK' frames:
		CUtlHash< IStudioRender::FacesRenderedInfo_t > topItemHash( 64, 0, 0, FacesRenderedInfoCompareFunc, FacesRenderedInfoKeyFunc );
		for ( int i = 0; i < nItems; i++ )
		{
			IStudioRender::FacesRenderedInfo_t *items = history[ ( nOldestItem + i ) % NUM_FRAMES_TO_TRACK ];
			for ( int j = 0; j < NUM_ITEMS_TO_TRACK; j++ )
			{
				if ( !items[ j ].nFaceCount )
					continue;
				UtlHashHandle_t h = topItemHash.Find( items[ j ] );
				if ( h != topItemHash.InvalidHandle() )
				{
					IStudioRender::FacesRenderedInfo_t &topItem = topItemHash[ h ];
					topItem.nFaceCount = MAX( items[ j ].nFaceCount, topItem.nFaceCount );
					continue;
				}
				topItemHash.Insert( items[ j ] );
			}
		}
		CUtlVector< IStudioRender::FacesRenderedInfo_t > topItems;
		for ( UtlHashHandle_t h = topItemHash.GetFirstHandle(); h != topItemHash.InvalidHandle(); h = topItemHash.GetNextHandle( h ) )
		{
			topItems.AddToTail( topItemHash[ h ] );
		}
		qsort( topItems.Base(), topItems.Count(), sizeof( topItems[0] ), FacesRenderedInfoSort );
		for ( int j = 0; j < MIN( nSpewFromHistory, NUM_ITEMS_TO_TRACK ); j++ )
		{
			if ( j < topItems.Count() )
				ConMsg( "%-7d (%-3d times, %-7d avg) %s\n", topItems[ j ].nFaceCount, topItems[ j ].nRenderCount, ( int )( ( float )topItems[ j ].nFaceCount / ( float )topItems[ j ].nRenderCount ), topItems[ j ].pStudioHdr->name );
		}
		ConMsg( "%-7d total model faces rendered this frame (mat_rendered_faces_count)\n", nTotal );
	}
}

int BuildFacesRenderedInfoListForMostRecentFrame( CUtlVector< IStudioRender::FacesRenderedInfo_t > &items, CUtlHash< studiohwdata_t * > &hash )
{
	int nTotal = 0;
	for ( UtlHashHandle_t h = hash.GetFirstHandle(); h != hash.InvalidHandle(); h = hash.GetNextHandle( h ) )
	{
		studiohwdata_t *pHwData = hash[ h ];
		if ( pHwData->m_pStudioHdr )
		{
			IStudioRender::FacesRenderedInfo_t item = { pHwData->m_pStudioHdr, pHwData->m_NumFacesRenderedThisFrame, pHwData->m_NumTimesRenderedThisFrame };
			items.AddToTail( item );
			nTotal += pHwData->m_NumFacesRenderedThisFrame;
		}
	}

	// Sort models by face count (biggest first)
	qsort( items.Base(), items.Count(), sizeof( items[0] ), FacesRenderedInfoSort );

	return nTotal;
}

void CStudioRender::UpdateModelFaceCounts( int nSpewFromCurrentFrame, bool bClearHistory )
{
	CUtlVector< IStudioRender::FacesRenderedInfo_t > items;
	if ( bClearHistory )
	{
		UpdateAndSpewFacesRenderedHistory( items, 0, 0, 0, bClearHistory );
	}
	else if ( mat_rendered_faces_count.GetBool() )
	{
		int nTotal = BuildFacesRenderedInfoListForMostRecentFrame( items, m_ModelFaceCountHash );
		UpdateAndSpewFacesRenderedHistory( items, nTotal, nSpewFromCurrentFrame, mat_rendered_faces_count.GetInt(), false );
		mat_rendered_faces_count.SetValue( 0 ); // set back to 0 so we don't spew anymore
	}
}

int CStudioRender::GetForcedMaterialOverrideIndex( int nMaterialIndex )
{
	if ( m_pRC )
	{
		for ( int i = 0; i < m_pRC->m_nForcedMaterialIndexCount; i++ )
		{
			if ( m_pRC->m_nForcedMaterialIndex[ i ] == nMaterialIndex )
			{
				return i;
			}
		}
	}

	return -1;
}


void CStudioRender::GatherRenderedFaceInfo( IStudioRender::FaceInfoCallbackFunc_t pFunc )
{
	int nTopN = mat_print_top_model_vert_counts.GetInt();

	if ( nTopN )
	{
		CUtlVector< IStudioRender::FacesRenderedInfo_t > items;
		int nTotal = BuildFacesRenderedInfoListForMostRecentFrame( items, m_ModelFaceCountHash );

		if ( items.Count() > 0 )
		{
			nTopN = MIN( nTopN, items.Count());
			pFunc( nTopN, items.Base(), nTotal );
		}
	}
}

CON_COMMAND( mat_rendered_faces_spew, "'mat_rendered_faces_spew <n>' Spew the number of faces rendered for the top N models used this frame (mat_rendered_faces_count must be set to use this)" )
{
	int nNumToSpew = ( args.ArgC() > 1 ) ? Q_atoi( args[ 1 ] ) : INT_MAX;
	if ( !mat_rendered_faces_count.GetBool() )
	{
		Msg( "ERROR: mat_rendered_faces_count must be set in order to use mat_rendered_faces_spew\n" ); 
		return;
	}
	g_StudioRender.UpdateModelFaceCounts( nNumToSpew );
}

#endif // !_CERT

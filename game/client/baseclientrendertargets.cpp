//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation for CBaseClientRenderTargets class.
//			Provides Init functions for common render textures used by the engine.
//			Mod makers can inherit from this class, and call the Create functions for
//			only the render textures the want for their mod.
//=============================================================================//

#include "cbase.h"
#include "baseclientrendertargets.h"						// header	
#include "materialsystem/imaterialsystemhardwareconfig.h"	// Hardware config checks
#include "materialsystem/itexture.h"						// Hardware config checks
#include "tier0/icommandline.h"
#ifdef GAMEUI_UISYSTEM2_ENABLED
#include "gameui.h"
#endif
#include "c_env_cascade_light.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar cl_disable_water_render_targets( "cl_disable_water_render_targets", "0" );

ITexture* CBaseClientRenderTargets::CreateWaterReflectionTexture( IMaterialSystem* pMaterialSystem, int iSize )
{
	iSize = CommandLine()->ParmValue( "-reflectionTextureSize", iSize );
#if defined( PORTAL )
	return pMaterialSystem->CreateNamedMultiRenderTargetTexture(
		"_rt_WaterReflection",
		iSize, iSize, RT_SIZE_NO_CHANGE,
		pMaterialSystem->GetBackBufferFormat(), 

		// Force separate depth buffer for recursive water reflection views, since using the default depth buffer
		// will clear depth in the upper left hand <512,512> corner of the screen causing translucent renderables to
		// render on top of closer geometry.
		MATERIAL_RT_DEPTH_SEPARATE,
		
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
#else
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_WaterReflection",
		iSize, iSize, RT_SIZE_PICMIP,
		pMaterialSystem->GetBackBufferFormat(), 
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
#endif
}

ITexture* CBaseClientRenderTargets::CreateWaterRefractionTexture( IMaterialSystem* pMaterialSystem, int iSize )
{
	iSize = CommandLine()->ParmValue( "-reflectionTextureSize", iSize );
#if defined( PORTAL )
	return pMaterialSystem->CreateNamedMultiRenderTargetTexture(
		"_rt_WaterRefraction",
		iSize, iSize, RT_SIZE_NO_CHANGE,
		// This is different than reflection because it has to have alpha for fog factor.
		IMAGE_FORMAT_RGBA8888, 
		
		// Force separate depth buffer for recursive water reflection views, since using the default depth buffer
		// will clear depth in the upper left hand <512,512> corner of the screen causing translucent renderables to
		// render on top of closer geometry.
		//
		// EDIT: on consoles it doesn't matter for Portal 2 because we never use refraction anyways...we always opt for cheaper translucent water.  Save memory instead.
		// @TODO: get rid of this buffer entirely on consoles
		IsGameConsole() ? MATERIAL_RT_DEPTH_SHARED : MATERIAL_RT_DEPTH_SEPARATE,

		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
#else
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_WaterRefraction",
		iSize, iSize, RT_SIZE_PICMIP,
		// This is different than reflection because it has to have alpha for fog factor.
		IMAGE_FORMAT_RGBA8888, 
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
#endif
}

ITexture* CBaseClientRenderTargets::CreateCameraTexture( IMaterialSystem* pMaterialSystem, int iSize )
{
	iSize = CommandLine()->ParmValue( "-monitorTextureSize", iSize );
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_Camera",
		iSize, iSize, RT_SIZE_DEFAULT,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, 
		0,
		CREATERENDERTARGETFLAGS_HDR );
}


//-----------------------------------------------------------------------------
// Purpose: Called by the engine in material system init and shutdown.
//			Clients should override this in their inherited version, but the base
//			is to init all standard render targets for use.
// Input  : pMaterialSystem - the engine's material system (our singleton is not yet inited at the time this is called)
//			pHardwareConfig - the user hardware config, useful for conditional render target setup
//-----------------------------------------------------------------------------
void CBaseClientRenderTargets::SetupClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig, int iWaterTextureSize, int iCameraTextureSize )
{
	IMaterialSystem *pSave = materials;

	// Make sure our config is loaded before we try to init rendertargets
	ConfigureCurrentSystemLevel();

	// Water effects
	materials = pMaterialSystem;							// in case not initted yet for mat system util
	g_pMaterialSystem = pMaterialSystem;
	g_pMaterialSystemHardwareConfig = pHardwareConfig;
	if ( iWaterTextureSize && !cl_disable_water_render_targets.GetBool() )
	{
		m_WaterReflectionTexture.Init( CreateWaterReflectionTexture( pMaterialSystem, iWaterTextureSize ) );
		m_WaterRefractionTexture.Init( CreateWaterRefractionTexture( pMaterialSystem, iWaterTextureSize ) );
	}

	// mdonofrio - Don't need monitors or flashlight shadows for CS15. 
	// Still need to initrendertargets for shadowmgr (blob/rtt shadows) - it will skip depth textures if convars set appropriately??

#if !( defined( CSTRIKE15 ) && defined( _PS3 ) ) 
	// Monitors
	if ( iCameraTextureSize )
		m_CameraTexture.Init( CreateCameraTexture( pMaterialSystem, iCameraTextureSize ) );
#endif

	ITexture *pGlintTexture = pMaterialSystem->CreateNamedRenderTargetTextureEx2( 
		"_rt_eyeglint", 32, 32, RT_SIZE_NO_CHANGE, IMAGE_FORMAT_BGRA8888, MATERIAL_RT_DEPTH_NONE );
	pGlintTexture->IncrementReferenceCount();

	g_CascadeLightManager.InitRenderTargets();

	g_pClientShadowMgr->InitRenderTargets();
	
#ifdef GAMEUI_UISYSTEM2_ENABLED
	g_pGameUIGameSystem->InitRenderTargets();
#endif

	m_RenderToRTHelperTexture.Init( pMaterialSystem->CreateNamedRenderTargetTextureEx2( "render_to_rt_helper", 512, 512, RT_SIZE_DEFAULT, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_SHARED, TEXTUREFLAGS_SINGLECOPY ) );

	materials = pSave;
}

void CBaseClientRenderTargets::InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig )
{
	SetupClientRenderTargets( pMaterialSystem, pHardwareConfig );
}

//-----------------------------------------------------------------------------
// Purpose: Shut down each CTextureReference we created in InitClientRenderTargets.
//			Called by the engine in material system shutdown.
// Input  :  - 
//-----------------------------------------------------------------------------
void CBaseClientRenderTargets::ShutdownClientRenderTargets()
{
	// Water effects
	m_WaterReflectionTexture.Shutdown();
	m_WaterRefractionTexture.Shutdown();

	// Monitors
	m_CameraTexture.Shutdown();

	g_pClientShadowMgr->ShutdownRenderTargets();

	g_CascadeLightManager.ShutdownRenderTargets();

	m_RenderToRTHelperTexture.Shutdown();

}

static CBaseClientRenderTargets g_BaseClientRenderTargets;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CBaseClientRenderTargets, IClientRenderTargets, 
	CLIENTRENDERTARGETS_INTERFACE_VERSION, g_BaseClientRenderTargets );

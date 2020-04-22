//===== Copyright (c) Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//
	
#include "pch_materialsystem.h"

#ifndef _PS3
#define MATSYS_INTERNAL
#endif

#include "cmaterialsystem.h"

#include "colorspace.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "IHardwareConfigInternal.h"
#include "shadersystem.h"
#include "texturemanager.h"
#include "shaderlib/ShaderDLL.h"
#include "tier1/callqueue.h"
#include "tier1/smartptr.h"
#include "vstdlib/jobthread.h"
#include "cmatnullrendercontext.h"
#include "datacache/iresourceaccesscontrol.h"
#include "filesystem/IQueuedLoader.h"
#include "filesystem/IXboxInstaller.h"
#include "cdll_int.h"
#include "vjobs_interface.h"
#include "ps3/ps3_sn.h"
#include "shaderapidx9/imeshdx8.h"
#include "tier0/perfstats.h"

#if defined( _X360 )
#include "xbox/xbox_console.h"
#include "xbox/xbox_win32stubs.h"
#elif defined(_PS3)
#include "ps3/ps3_helpers.h"
#include "ps3/ps3_console.h"
#endif

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

#ifdef PLATFORM_POSIX
#define _finite finite
#endif

// this is hooked into the engines convar
ConVar mat_debugalttab( "mat_debugalttab", "0", FCVAR_CHEAT );
ConVar gpu_level( "gpu_level", "3", 0, "GPU Level - Default: High" );
ConVar mat_force_vertexfog( "mat_force_vertexfog", "0", FCVAR_DEVELOPMENTONLY );
static ConVar mat_forcemanagedtextureintohardware( "mat_forcemanagedtextureintohardware", "1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar mat_supportflashlight( "mat_supportflashlight", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "0 - do not support flashlight (don't load flashlight shader combos), 1 - flashlight is supported" );
// Default this to zero for the press playtest!
static ConVar mat_forcehardwaresync( "mat_forcehardwaresync", /* IsPC() ? "1" : */ "0" );

// Make sure this convar gets created before videocfg.lib is initialized, so it can be driven by dxsupport.cfg
static ConVar mat_tonemapping_occlusion_use_stencil( "mat_tonemapping_occlusion_use_stencil", "0", FCVAR_DEVELOPMENTONLY );
#if defined( DX_TO_GL_ABSTRACTION ) && !defined( _PS3 )
    static ConVar mat_dxlevel( "mat_dxlevel", "100", FCVAR_DEVELOPMENTONLY, "", true, 90, true, 100, NULL );
    //static ConVar mat_dxlevel( "mat_dxlevel", "95", FCVAR_DEVELOPMENTONLY, "", true, 95, true, 95, NULL );
    //static ConVar mat_dxlevel( "mat_dxlevel", "92", FCVAR_DEVELOPMENTONLY, "", true, 92, true, 92, NULL );
#else
static ConVar mat_dxlevel( "mat_dxlevel", "0", FCVAR_DEVELOPMENTONLY );
#endif

ConVar mat_queue_mode( "mat_queue_mode", "-1", FCVAR_RELEASE, "The queue/thread mode the material system should use: -1=default, 0=synchronous single thread, 1=queued single thread, 2=queued multithreaded" );

ConVar mat_queue_report( "mat_queue_report", "0", FCVAR_ARCHIVE, "Report thread stalls.  Positive number will filter by stalls >= time in ms.  -1 reports all locks." );
ConVar mat_queue_mode_force_allow( "mat_queue_mode_force_allow", IsPS3() ? "1" : "0", FCVAR_DEVELOPMENTONLY, "Whether QMS can be enabled on single threaded CPU" );
ConVar mat_queue_priority("mat_queue_priority", "1", FCVAR_RELEASE);

// FIXME: Would like to remove these, but what the hey.
#if defined( DX_TO_GL_ABSTRACTION )
static ConVar cpu_level( "cpu_level", "3", 0, "CPU Level - Default: High" );
static ConVar gpu_mem_level( "gpu_mem_level", "3", 0, "Memory Level - Default: High" );
#else
static ConVar cpu_level( "cpu_level", "2", 0, "CPU Level - Default: High" );
static ConVar mem_level( "mem_level", "2", 0, "Memory Level - Default: High" );
static ConVar gpu_mem_level( "gpu_mem_level", "2", 0, "Memory Level - Default: High" );
#endif

static ConVar mat_picmip( "mat_picmip", "0", FCVAR_NONE, "", true, -10, true, 4 );

ConVar csm_quality_level( "csm_quality_level", "0", 0, "Cascaded shadow map quality level, [0,3], 0=VERY_LOW, 3=HIGHEST" );

// Moving this here (instead of in viewpostprocess.cpp) so videocfg.cpp can modify its value early during init based off whatever setting is in video.txt
#if defined( CSTRIKE15 )
ConVar mat_software_aa_strength( "mat_software_aa_strength", "-1.0", 0, "Software AA - perform a software anti-aliasing post-process (an alternative/supplement to MSAA). This value sets the strength of the effect: (0.0 - off), (1.0 - full)" );
#else
ConVar mat_software_aa_strength( "mat_software_aa_strength", IsPS3()? "0" : "-1.0", 0, "Software AA - perform a software anti-aliasing post-process (an alternative/supplement to MSAA). This value sets the strength of the effect: (0.0 - off), (1.0 - full)" );
#endif

ConVar mat_async_tex_maxtime_ms( "mat_async_tex_maxtime_ms", "0.5", FCVAR_DEVELOPMENTONLY, "Cutoff time (in ms) spent in ServiceAsyncTextureLoads" );

// Material system console channel
BEGIN_DEFINE_LOGGING_CHANNEL( LOG_MaterialSystemConsole, "MaterialSystemConsole", LCF_CONSOLE_ONLY );
ADD_LOGGING_CHANNEL_TAG( "Console" );
END_DEFINE_LOGGING_CHANNEL();

IMaterialInternal *g_pErrorMaterial = NULL;

#if defined( INCLUDE_SCALEFORM )
extern IScaleformUI* g_pScaleformUI;
#endif

CreateInterfaceFn g_fnMatSystemConnectCreateInterface = NULL;  

#ifdef _PS3
#define m_pRenderContext Ps3TlsMaterialSystemRenderContext
#elif defined(_X360)
IMatRenderContextInternal *CMaterialSystem::m_pRenderContexts[2];
#define m_pRenderContext CMaterialSystem::m_pRenderContexts[(int)ThreadInMainThread()]
#else
CTHREADLOCALPTR(IMatRenderContextInternal) CMaterialSystem::m_pRenderContext;
#endif

#if defined( _X360 )
static const unsigned int g_GamerpicSize = 64;
static const ImageFormat g_GamerpicFormat = IMAGE_FORMAT_LINEAR_BGRA8888; // note that this format is intentionally BGRA instead of ARBB
#endif // _X360

//#define PERF_TESTING 1

#ifdef DX_TO_GL_ABSTRACTION
// Uncomment if you want the material queued system to run on its own thread pool
// Otherwise it will use the global thread pool
#define MAT_QUEUED_OWN_THREADPOOL
#endif

//-----------------------------------------------------------------------------
// Implementational structures
//-----------------------------------------------------------------------------

#define MATERIAL_MAX_TREE_DEPTH 256


//-----------------------------------------------------------------------------
// Singleton instance exposed to the engine
//-----------------------------------------------------------------------------

CMaterialSystem g_MaterialSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMaterialSystem, IMaterialSystem, 
						MATERIAL_SYSTEM_INTERFACE_VERSION, g_MaterialSystem );

// Expose this to the external shader DLLs
MaterialSystem_Config_t g_config;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( MaterialSystem_Config_t, MaterialSystem_Config_t, MATERIALSYSTEM_CONFIG_VERSION, g_config );

//-----------------------------------------------------------------------------

CThreadFastMutex g_MatSysMutex;

//-----------------------------------------------------------------------------
// Purpose: additional materialsystem information, internal use only
//-----------------------------------------------------------------------------
#ifndef _GAMECONSOLE
struct MaterialSystem_Config_Internal_t
{
	int gpu_level;
};
MaterialSystem_Config_Internal_t g_config_internal;
#endif

//-----------------------------------------------------------------------------
// Necessary to allow the shader DLLs to get ahold of IMaterialSystemHardwareConfig
//-----------------------------------------------------------------------------
IHardwareConfigInternal* g_pHWConfig = 0;
static void *GetHardwareConfig()
{
	if ( g_pHWConfig )
		return (IMaterialSystemHardwareConfig*)g_pHWConfig;

	// can't call QueryShaderAPI here because it calls a factory function
	// and we end up in an infinite recursion
	return NULL;
}
EXPOSE_INTERFACE_FN( GetHardwareConfig, IMaterialSystemHardwareConfig, MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION );


//-----------------------------------------------------------------------------
// Necessary to allow the shader DLLs to get ahold of ICvar
//-----------------------------------------------------------------------------
static void *GetICVar()
{
	return g_pCVar;
}
EXPOSE_INTERFACE_FN( GetICVar, ICVar, CVAR_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Accessor to get at the material system
//-----------------------------------------------------------------------------
IMaterialSystemInternal *g_pInternalMaterialSystem = &g_MaterialSystem;
#ifndef _PS3
IShaderUtil *g_pShaderUtil = &g_MaterialSystem;
#endif
IVJobs * g_pVJobs = NULL;

#if defined( USE_SDL ) || defined( OSX )

#include "appframework/ilaunchermgr.h"
ILauncherMgr *g_pLauncherMgr = NULL;	// set in CMaterialSystem::Connect

#endif

//-----------------------------------------------------------------------------
// Factory used to get at internal interfaces (used by shaderapi + shader dlls)
//-----------------------------------------------------------------------------
void *ShaderFactory( const char *pName, int *pReturnCode )
{
	if (pReturnCode)
	{
		*pReturnCode = IFACE_OK;
	}
	
	if ( !Q_stricmp( pName, FILESYSTEM_INTERFACE_VERSION ))
		return g_pFullFileSystem;

	if ( !Q_stricmp( pName, QUEUEDLOADER_INTERFACE_VERSION ))
		return g_pQueuedLoader;

	if ( !Q_stricmp( pName, VJOBS_INTERFACE_VERSION ) )
		return g_pVJobs;

#if defined( _X360 )
	if ( !Q_stricmp( pName, XBOXINSTALLER_INTERFACE_VERSION ))
		return g_pXboxInstaller;
#endif

	if ( !Q_stricmp( pName, SHADER_UTIL_INTERFACE_VERSION ))
		return g_pShaderUtil;

#if defined( USE_SDL )
    if ( !Q_stricmp( pName, "SDLMgrInterface001" /*SDLMGR_INTERFACE_VERSION*/ ))
		return g_pLauncherMgr;
#endif

#if PLATFORM_OSX
	if ( !Q_stricmp( pName, "CocoaMgrInterface006" /*COCOAMGR_INTERFACE_VERSION*/ ))
		return g_pLauncherMgr;
#endif

	void * pInterface = g_MaterialSystem.QueryInterface( pName );
	if ( pInterface )
		return pInterface;

	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return NULL;	
}

//-----------------------------------------------------------------------------
// Resource preloading for materials.
//-----------------------------------------------------------------------------
class CResourcePreloadMaterial : public CResourcePreload
{
	virtual bool CreateResource( const char *pName )
	{
		IMaterial *pMaterial = g_MaterialSystem.FindMaterial( pName, TEXTURE_GROUP_WORLD, false );
		IMaterialInternal *pMatInternal = static_cast< IMaterialInternal * >( pMaterial );
		if ( pMatInternal )
		{
			// always work with the realtime material internally
			pMatInternal = pMatInternal->GetRealTimeVersion();

			// tag these for later identification (prevents an unwanted purge)
			pMatInternal->MarkAsPreloaded( true );
			if ( !pMatInternal->IsErrorMaterial() )
			{
				// force material's textures to create now
				pMatInternal->Precache();
				return true;
			}
			else
			{
				if ( IsPosix() )
				{
					printf("\n ##### CResourcePreloadMaterial::CreateResource can't find material %s\n", pName);
				}
			}
		}

		return false;
	}

	//-----------------------------------------------------------------------------
	// Called before queued loader i/o jobs are actually performed. Must free up memory
	// to ensure i/o requests have enough memory to succeed.  The materials that were
	// touched by the CreateResource() are inhibited from purging (as is their textures,
	// by virtue of ref counts), all others are candidates.  The preloaded materials
	// are by definition zero ref'd until owned by the normal loading process. Any material
	// that stays zero ref'd is a candidate for the post load purge.
	//-----------------------------------------------------------------------------
	virtual void PurgeUnreferencedResources()
	{
		bool bSpew = ( g_pQueuedLoader->GetSpewDetail() & LOADER_DETAIL_PURGES ) != 0;

		bool bDidUncacheMaterial = false;
		MaterialHandle_t hNext;
		for ( MaterialHandle_t hMaterial = g_MaterialSystem.FirstMaterial(); hMaterial != g_MaterialSystem.InvalidMaterial(); hMaterial = hNext )
		{
			hNext = g_MaterialSystem.NextMaterial( hMaterial );

			IMaterialInternal *pMatInternal = g_MaterialSystem.GetMaterialInternal( hMaterial );
			Assert( pMatInternal->GetReferenceCount() >= 0 );

			// preloaded materials are safe from this pre-purge
			if ( !pMatInternal->IsPreloaded() )
			{
				// undo any possible artifical ref count
				pMatInternal->ArtificialRelease();
				if ( pMatInternal->GetReferenceCount() <= 0 )
				{
					if ( bSpew )
					{
						Msg( "CResourcePreloadMaterial: Purging: %s (%d)\n", pMatInternal->GetName(), pMatInternal->GetReferenceCount() );
					}
					bDidUncacheMaterial = true;
					pMatInternal->Uncache();
					pMatInternal->DeleteIfUnreferenced();
				}
			}
			else
			{
				// clear the bit
				pMatInternal->MarkAsPreloaded( false );
			}
		}

		// purged materials unreference their textures
		// purge any zero ref'd textures
		TextureManager()->RemoveUnusedTextures();

		// fixup any excluded textures, may cause some new batch requests
		MaterialSystem()->UpdateExcludedTextures();
	}

	virtual void PurgeAll()
	{
		bool bSpew = ( g_pQueuedLoader->GetSpewDetail() & LOADER_DETAIL_PURGES ) != 0;

		bool bDidUncacheMaterial = false;
		MaterialHandle_t hNext;
		for ( MaterialHandle_t hMaterial = g_MaterialSystem.FirstMaterial(); hMaterial != g_MaterialSystem.InvalidMaterial(); hMaterial = hNext )
		{
			hNext = g_MaterialSystem.NextMaterial( hMaterial );

			IMaterialInternal *pMatInternal = g_MaterialSystem.GetMaterialInternal( hMaterial );
			Assert( pMatInternal->GetReferenceCount() >= 0 );

			pMatInternal->MarkAsPreloaded( false );
			// undo any possible artifical ref count
			pMatInternal->ArtificialRelease();
			if ( pMatInternal->GetReferenceCount() <= 0 )
			{
				if ( bSpew )
				{
					Msg( "CResourcePreloadMaterial: Purging: %s (%d)\n", pMatInternal->GetName(), pMatInternal->GetReferenceCount() );
				}
				bDidUncacheMaterial = true;
				pMatInternal->Uncache();
				pMatInternal->DeleteIfUnreferenced();
			}
		}

		// purged materials unreference their textures
		// purge any zero ref'd textures
		TextureManager()->RemoveUnusedTextures();
	}

	void OnEndMapLoading( bool bAbort )
	{
		CMaterialDict *pDict = g_MaterialSystem.GetMaterialDict();
		for (MaterialHandle_t i = pDict->FirstMaterial(); i != pDict->InvalidMaterial(); i = pDict->NextMaterial(i) )
		{
			pDict->GetMaterialInternal(i)->CompactMaterialVars();
		}

		CompactMaterialVarHeap();
	}

#if defined( _PS3 )
	virtual bool RequiresRendererLock()
	{
		return true;
	}
#endif // _PS3
};

static CResourcePreloadMaterial s_ResourcePreloadMaterial;

//-----------------------------------------------------------------------------
// Resource preloading for cubemaps.
//-----------------------------------------------------------------------------
class CResourcePreloadCubemap : public CResourcePreload
{
	virtual bool CreateResource( const char *pName )
	{
		ITexture *pTexture = g_MaterialSystem.FindTexture( pName, TEXTURE_GROUP_CUBE_MAP, true );
		ITextureInternal *pTexInternal = static_cast< ITextureInternal * >( pTexture );
		if ( pTexInternal )
		{
			// There can be cubemaps that are unbound by materials. To prevent an unwanted purge,
			// mark and increase the ref count. Otherwise the pre-purge discards these zero
			// ref'd textures, and then the normal loading process hitches on the miss.
			// The zombie cubemaps DO get discarded after the normal loading process completes
			// if no material references them.
			pTexInternal->MarkAsPreloaded( true );
			pTexInternal->IncrementReferenceCount();
			if ( !IsErrorTexture( pTexInternal ) )
			{
				return true;
			}
		}
		return false;
	}

	//-----------------------------------------------------------------------------
	// All valid cubemaps should have been owned by their materials.  Undo the preloaded
	// cubemap locks. Any zero ref'd cubemaps will be purged by the normal loading path conclusion.
	//-----------------------------------------------------------------------------
	virtual void OnEndMapLoading( bool bAbort )
	{
		int iIndex = -1;
		for ( ;; )
		{
			ITextureInternal *pTexInternal;
			iIndex = TextureManager()->FindNext( iIndex, &pTexInternal );
			if ( iIndex == -1 || !pTexInternal )
			{
				// end of list
				break;
			}

			if ( pTexInternal->IsPreloaded() )
			{
				// undo the artificial increase
				pTexInternal->MarkAsPreloaded( false );
				pTexInternal->DecrementReferenceCount();
			}
		}	
	}

#if defined( _PS3 )
	virtual bool RequiresRendererLock()
	{
		return true;
	}
#endif // _PS3
};
static CResourcePreloadCubemap s_ResourcePreloadCubemap;

//-----------------------------------------------------------------------------
// Creates the debugging materials
//-----------------------------------------------------------------------------
void CMaterialSystem::CreateDebugMaterials()
{
	if ( !m_pDrawFlatMaterial )
	{
		KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
		pVMTKeyValues->SetInt( "$model", 1 );
		pVMTKeyValues->SetFloat( "$decalscale", 0.05f );
		pVMTKeyValues->SetString( "$basetexture", "error" );	// This is the "error texture"
		pVMTKeyValues->SetInt( "$gammacolorread", 1 );
		pVMTKeyValues->SetInt( "$linearwrite", 1 );
		g_pErrorMaterial = static_cast<IMaterialInternal*>(CreateMaterial( "___error.vmt", pVMTKeyValues ))->GetRealTimeVersion();

		pVMTKeyValues = new KeyValues( "UnlitGeneric" );
		pVMTKeyValues->SetInt( "$flat", 1 );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		pVMTKeyValues->SetInt( "$gammacolorread", 1 );
		pVMTKeyValues->SetInt( "$linearwrite", 1 );
		m_pDrawFlatMaterial = static_cast<IMaterialInternal*>(CreateMaterial( "___flat.vmt", pVMTKeyValues ))->GetRealTimeVersion();

		pVMTKeyValues = new KeyValues( "BufferClearObeyStencil" );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		m_pBufferClearObeyStencil[BUFFER_CLEAR_NONE] = static_cast<IMaterialInternal*>(CreateMaterial( "___buffer_clear_obey_stencil0.vmt", pVMTKeyValues ))->GetRealTimeVersion();

		pVMTKeyValues = new KeyValues( "BufferClearObeyStencil" );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		pVMTKeyValues->SetInt( "$clearcolor", 1 );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		m_pBufferClearObeyStencil[BUFFER_CLEAR_COLOR] = static_cast<IMaterialInternal*>(CreateMaterial( "___buffer_clear_obey_stencil1.vmt", pVMTKeyValues ))->GetRealTimeVersion();

		pVMTKeyValues = new KeyValues( "BufferClearObeyStencil" );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		pVMTKeyValues->SetInt( "$clearalpha", 1 );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		m_pBufferClearObeyStencil[BUFFER_CLEAR_ALPHA] = static_cast<IMaterialInternal*>(CreateMaterial( "___buffer_clear_obey_stencil2.vmt", pVMTKeyValues ))->GetRealTimeVersion();

		pVMTKeyValues = new KeyValues( "BufferClearObeyStencil" );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		pVMTKeyValues->SetInt( "$clearcolor", 1 );
		pVMTKeyValues->SetInt( "$clearalpha", 1 );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		m_pBufferClearObeyStencil[BUFFER_CLEAR_COLOR_AND_ALPHA] = static_cast<IMaterialInternal*>(CreateMaterial( "___buffer_clear_obey_stencil3.vmt", pVMTKeyValues ))->GetRealTimeVersion();

		pVMTKeyValues = new KeyValues( "BufferClearObeyStencil" );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		pVMTKeyValues->SetInt( "$cleardepth", 1 );
		m_pBufferClearObeyStencil[BUFFER_CLEAR_DEPTH] = static_cast<IMaterialInternal*>(CreateMaterial( "___buffer_clear_obey_stencil4.vmt", pVMTKeyValues ))->GetRealTimeVersion();

		pVMTKeyValues = new KeyValues( "BufferClearObeyStencil" );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		pVMTKeyValues->SetInt( "$cleardepth", 1 );
		pVMTKeyValues->SetInt( "$clearcolor", 1 );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		m_pBufferClearObeyStencil[BUFFER_CLEAR_COLOR_AND_DEPTH] = static_cast<IMaterialInternal*>(CreateMaterial( "___buffer_clear_obey_stencil5.vmt", pVMTKeyValues ))->GetRealTimeVersion();

		pVMTKeyValues = new KeyValues( "BufferClearObeyStencil" );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		pVMTKeyValues->SetInt( "$cleardepth", 1 );
		pVMTKeyValues->SetInt( "$clearalpha", 1 );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		m_pBufferClearObeyStencil[BUFFER_CLEAR_ALPHA_AND_DEPTH] = static_cast<IMaterialInternal*>(CreateMaterial( "___buffer_clear_obey_stencil6.vmt", pVMTKeyValues ))->GetRealTimeVersion();

		pVMTKeyValues = new KeyValues( "BufferClearObeyStencil" );
		pVMTKeyValues->SetInt( "$nocull", 1 );
		pVMTKeyValues->SetInt( "$cleardepth", 1 );
		pVMTKeyValues->SetInt( "$clearcolor", 1 );
		pVMTKeyValues->SetInt( "$clearalpha", 1 );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		m_pBufferClearObeyStencil[BUFFER_CLEAR_COLOR_AND_ALPHA_AND_DEPTH] = static_cast<IMaterialInternal*>(CreateMaterial( "___buffer_clear_obey_stencil7.vmt", pVMTKeyValues ))->GetRealTimeVersion();

		if ( IsPS3() )
		{
			pVMTKeyValues = new KeyValues( "BufferClearObeyStencil" );
			pVMTKeyValues->SetInt( "$nocull", 1 );
			pVMTKeyValues->SetInt( "$cleardepth", 1 );
			pVMTKeyValues->SetInt( "$vertexcolor", 1 );
			pVMTKeyValues->SetInt( "$reloadzcull", 1 );
			m_pReloadZcullMaterial = static_cast<IMaterialInternal*>(CreateMaterial( "__reload_zcull.vmt", pVMTKeyValues ))->GetRealTimeVersion();
		}
		
		if ( IsX360() )
		{
			pVMTKeyValues = new KeyValues( "RenderTargetBlit_X360" );
			m_pRenderTargetBlitMaterial = static_cast<IMaterialInternal*>(CreateMaterial( "___renderTargetBlit.vmt", pVMTKeyValues ))->GetRealTimeVersion();
		}

		// PORTAL2 - hack to make sure BINK shaders are always precached to avoid hitches at runtime
		if ( IsGameConsole() )
		{
			pVMTKeyValues = new KeyValues( "Bik" );

			pVMTKeyValues->SetInt( "$nofog", 1 );
			pVMTKeyValues->SetInt( "$spriteorientation", 3 );
			pVMTKeyValues->SetInt( "$translucent", 1 );
			pVMTKeyValues->SetInt( "$vertexcolor", 1 );
			pVMTKeyValues->SetInt( "$vertexalpha", 1 );
			pVMTKeyValues->SetInt( "$nolod", 1 );
			pVMTKeyValues->SetInt( "$nomip", 1 );
			pVMTKeyValues->SetInt( "$nobasetexture", 1 );

			m_pBIKPreloadMaterial = static_cast<IMaterialInternal*>(CreateMaterial( "___binkprecache.vmt", pVMTKeyValues ))->GetRealTimeVersion();
		}

		ShaderSystem()->CreateDebugMaterials();
	}
}


//-----------------------------------------------------------------------------
// Deletes the debugging materials
//-----------------------------------------------------------------------------
void CMaterialSystem::CleanUpDebugMaterials()
{
	if ( m_pDrawFlatMaterial )
	{
		m_pDrawFlatMaterial->DecrementReferenceCount();

		RemoveMaterial( m_pDrawFlatMaterial );

		m_pDrawFlatMaterial = NULL;

		for ( int i = BUFFER_CLEAR_NONE; i < BUFFER_CLEAR_TYPE_COUNT; ++i )
		{
			m_pBufferClearObeyStencil[i]->DecrementReferenceCount();
			RemoveMaterial( m_pBufferClearObeyStencil[i] );
			m_pBufferClearObeyStencil[i] = NULL;
		}

		if ( IsPS3() )
		{
			m_pReloadZcullMaterial->DecrementReferenceCount();
			RemoveMaterial( m_pReloadZcullMaterial );
			m_pReloadZcullMaterial = NULL;
		}

		if ( IsX360() )
		{
			m_pRenderTargetBlitMaterial->DecrementReferenceCount();
			RemoveMaterial( m_pRenderTargetBlitMaterial );
			m_pRenderTargetBlitMaterial = NULL;
		}

		// PORTAL2 - clean up preloaded bink shader
		if ( IsGameConsole() )
		{
			m_pBIKPreloadMaterial->DecrementReferenceCount();
			RemoveMaterial( m_pBIKPreloadMaterial );
			m_pBIKPreloadMaterial = NULL;
		}

		ShaderSystem()->CleanUpDebugMaterials();
	}
}

void CMaterialSystem::CleanUpErrorMaterial()
{
	// Destruction of g_pErrorMaterial is deferred until after CMaterialDict::Shutdown.
	// The global g_pErrorMaterial is set to NULL so that IMaterialInternal::DestroyMaterial will delete it.
	IMaterialInternal *pErrorMaterial = g_pErrorMaterial;
	g_pErrorMaterial = NULL;
	pErrorMaterial->DecrementReferenceCount();
	IMaterialInternal::DestroyMaterial( pErrorMaterial );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMaterialSystem::CMaterialSystem()
{
	m_nRenderThreadID = 0xFFFFFFFF;
	m_ShaderHInst = 0;
	m_pMaterialProxyFactory = NULL;
	m_pClientMaterialSystemInterface = NULL;
	m_nAdapter = 0;
	m_nAdapterFlags = 0;
	m_bRequestedEditorMaterials = false;
	m_bRequestedGBuffers = false;
	m_StandardTexturesAllocated = false;
	m_bRestoreManangedResources = true;
	m_bInFrame = false;
	m_bThreadHasOwnership = false;

#ifdef DX_TO_GL_ABSTRACTION
	m_ThreadOwnershipID = 0;
#endif

	m_pShaderDLL = NULL;
	m_FullbrightLightmapTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_FullbrightBumpedLightmapTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_BlackTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_BlackAlphaZeroTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_FlatNormalTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_FlatSSBumpTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_GreyTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_GreyAlphaZeroTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_WhiteTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_LinearToGammaTableTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_LinearToGammaIdentityTableTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
	m_MaxDepthTextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;

	m_bInStubMode = false;
	m_pForcedTextureLoadPathID = NULL;
	m_bDisableRenderTargetAllocationForever = false;
	m_nAllocatingRenderTargets = false;
	m_pRenderContext = &m_HardwareRenderContext;
	m_iCurQueuedContext = 0;
	m_bGeneratedConfig = false;
	m_pMatQueueThreadPool = NULL;

	m_pActiveAsyncTextureLoad = NULL;

#ifndef _PS3
	m_pActiveAsyncJob = NULL;
#else
	m_bQMSJobSubmitted = false;
#endif
	m_IdealThreadMode = m_ThreadMode = MATERIAL_SINGLE_THREADED;
	m_nServiceThread = 0;

	m_nConfigurationFlags = 0;
	m_bForcedSingleThreaded = true;
	m_bAllowQueuedRendering = false;

	m_bStereoBoolsInitialized = false;
	m_bIsStereoSupported = false;
	m_bIsStereoActiveThisFrame = false;

	m_bLevelLoadingComplete = false;

	m_pSubString = NULL;
	m_bDeferredMaterialReload = false;
}

CMaterialSystem::~CMaterialSystem()
{
	if (m_pShaderDLL)
	{
		delete[] m_pShaderDLL;
	}

	if ( m_pSubString )
	{
		free( m_pSubString );
		m_pSubString = NULL;
	}
}


//-----------------------------------------------------------------------------
// Creates/destroys the shader implementation for the selected API
//-----------------------------------------------------------------------------
CreateInterfaceFn CMaterialSystem::CreateShaderAPI( char const* pShaderDLL )
{
	if ( !pShaderDLL )
		return 0;

	// Clean up the old shader
	DestroyShaderAPI();

	// Load the new shader
	m_ShaderHInst = Sys_LoadModule( pShaderDLL );

	// Error loading the shader
	if ( !m_ShaderHInst )
		return 0;

	// Get our class factory methods...
	return Sys_GetFactory( m_ShaderHInst );
}

void CMaterialSystem::DestroyShaderAPI()
{
	if (m_ShaderHInst)
	{
		// NOTE: By unloading the library, this will destroy m_pShaderAPI
		Sys_UnloadModule( m_ShaderHInst );
		g_pShaderAPI = 0;
		g_pHWConfig = 0;
		g_pShaderShadow = 0;
		m_ShaderHInst = 0;
	}
}


//-----------------------------------------------------------------------------
// Sets which shader we should be using. Has to be done before connect!
//-----------------------------------------------------------------------------
void CMaterialSystem::SetShaderAPI( char const *pShaderAPIDLL )
{
#if defined( _PS3 ) || defined( _OSX )
	return;
#endif

	if ( m_ShaderAPIFactory )
	{
		Warning( "Cannot set the shader API twice!\n" );
		return;
	}

	if ( !pShaderAPIDLL )
	{
		pShaderAPIDLL = "shaderapidx9";
	}

	// m_pShaderDLL is needed to spew driver info
	Assert( pShaderAPIDLL );
	int len = Q_strlen( pShaderAPIDLL ) + 1;
	m_pShaderDLL = new char[len];
	memcpy( m_pShaderDLL, pShaderAPIDLL, len );

	m_ShaderAPIFactory = CreateShaderAPI( pShaderAPIDLL );
	if ( !m_ShaderAPIFactory )
	{
		DestroyShaderAPI();
	}
}
	

//-----------------------------------------------------------------------------
// Connect/disconnect
//-----------------------------------------------------------------------------
bool CMaterialSystem::Connect( CreateInterfaceFn factory )
{
	if ( !factory )
		return false;

	if ( !BaseClass::Connect( factory ) )
		return false;

	if ( !g_pFullFileSystem )
	{
		Warning( "The material system requires the filesystem to run!\n" );
		return false;
	}
	
	g_pVJobs = ( IVJobs* )factory( VJOBS_INTERFACE_VERSION, NULL );

	// Get at the interfaces exported by the shader DLL

#ifndef _OSX
	g_pShaderDeviceMgr = (IShaderDeviceMgr*)m_ShaderAPIFactory( SHADER_DEVICE_MGR_INTERFACE_VERSION, 0 );
	if ( !g_pShaderDeviceMgr )
		return false;
	g_pHWConfig = (IHardwareConfigInternal*)m_ShaderAPIFactory( MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, 0 );
	if ( !g_pHWConfig )
		return false;
#endif

#ifndef DEDICATED

#if defined( USE_SDL )
#if !defined( LINUX )
	g_pHWConfig = g_pHardwareConfig;
#endif

	g_pLauncherMgr = (ILauncherMgr *)factory( "SDLMgrInterface001", NULL );
	if ( !g_pLauncherMgr )
		return false;

#elif defined( _PS3 )
	g_pHWConfig = g_pHardwareConfig;
#elif defined( _OSX )
	g_pHWConfig = g_pHardwareConfig;

	// write a link to the Cocoa manager into the config record so the shader subsystem can get to it.
	// alas we can't include icocoamgr.h due to a header conflict in the SDK, so the interface name here is hardwired for now
	// /System/Library/Frameworks/CoreServices.framework/Frameworks/CarbonCore.framework/Headers/Threads.h:520:
	// error: declaration of C function ‘OSErr CreateThreadPool(ThreadStyle, SInt16, Size)’
#define  COCOAMGR_INTERFACE_VERSION "CocoaMgrInterface006"

	g_pLauncherMgr = (ILauncherMgr *)factory( COCOAMGR_INTERFACE_VERSION, NULL );		
	if ( !g_pLauncherMgr )
		return false;

#elif defined(_WIN32)

#else

	#error

#endif

#endif // !DEDICATED

#ifndef _OSX
	// FIXME: ShaderAPI, ShaderDevice, and ShaderShadow should only come in after setting mode
	g_pShaderAPI = (IShaderAPI*)m_ShaderAPIFactory( SHADERAPI_INTERFACE_VERSION, 0 );
	if ( !g_pShaderAPI )
		return false;
	g_pShaderDevice = (IShaderDevice*)m_ShaderAPIFactory( SHADER_DEVICE_INTERFACE_VERSION, 0 );
	if ( !g_pShaderDevice )
		return false;
	g_pShaderShadow = (IShaderShadow*)m_ShaderAPIFactory( SHADERSHADOW_INTERFACE_VERSION, 0 );
	if ( !g_pShaderShadow )
		return false;
#endif

	// Remember the factory for connect
	g_fnMatSystemConnectCreateInterface = factory;

#if defined( INCLUDE_SCALEFORM )
	g_pScaleformUI = ( IScaleformUI* ) factory( SCALEFORMUI_INTERFACE_VERSION, 0 );
#endif

	return g_pShaderDeviceMgr->Connect( ShaderFactory );	
}

void CMaterialSystem::Disconnect()
{
	// Forget the factory for connect
	g_fnMatSystemConnectCreateInterface = NULL;

	if ( g_pShaderDeviceMgr )
	{
		g_pShaderDeviceMgr->Disconnect();
		g_pShaderDeviceMgr = NULL;

		// Unload the DLL
		DestroyShaderAPI();
	}
#if !defined( _PS3 ) && !defined( _OSX )
	g_pShaderAPI = NULL;
	g_pHWConfig = NULL;
	g_pShaderShadow = NULL;
	g_pShaderDevice = NULL;
#endif

#if defined( INCLUDE_SCALEFORM )
	g_pScaleformUI = NULL;
#endif

	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Used to enable editor materials. Must be called before Init.
//-----------------------------------------------------------------------------
void CMaterialSystem::EnableEditorMaterials()
{
	m_bRequestedEditorMaterials = true;
}

void CMaterialSystem::EnableGBuffers()
{
	m_bRequestedGBuffers = true;
}


//-----------------------------------------------------------------------------
// Method to get at interfaces supported by the SHADDERAPI
//-----------------------------------------------------------------------------
void *CMaterialSystem::QueryShaderAPI( const char *pInterfaceName )
{
	// Returns various interfaces supported by the shader API dll
	void *pInterface = NULL;
	if (m_ShaderAPIFactory)
	{
		pInterface = m_ShaderAPIFactory( pInterfaceName, NULL );
	}
	return pInterface;
}


//-----------------------------------------------------------------------------
// Method to get at different interfaces supported by the material system
//-----------------------------------------------------------------------------
void *CMaterialSystem::QueryInterface( const char *pInterfaceName )
{
	// Returns various interfaces supported by the shader API dll
	void *pInterface = QueryShaderAPI( pInterfaceName );
	if ( pInterface )
		return pInterface;

	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}


//-----------------------------------------------------------------------------
// Must be called before Init(), if you're going to call it at all...
//-----------------------------------------------------------------------------
void CMaterialSystem::SetAdapter( int nAdapter, int nAdapterFlags )
{
	m_nAdapter = nAdapter;
	m_nAdapterFlags = nAdapterFlags;
	g_pShaderDeviceMgr->GetCurrentModeInfo( &m_nAdapterInfo, m_nAdapter );
}

	
//-----------------------------------------------------------------------------
// Initializes the color correction terms
//-----------------------------------------------------------------------------
void CMaterialSystem::InitColorCorrection( )
{
	if ( ColorCorrectionSystem() )
	{
		ColorCorrectionSystem()->Init();
	}
}

#ifndef _PS3 // make some empty stubs so we can use IsPS3() instead of ifdefs
static inline void PS3InitFontLibrary( unsigned fontFileCacheSizeInBytes, unsigned maxNumFonts ) {};
static inline void PS3DumpFontLibrary(){}
#define kPS3_DEFAULT_MAX_USER_FONTS 0
#endif

//-----------------------------------------------------------------------------
// Initialization + shutdown of the material system
//-----------------------------------------------------------------------------
void AllocateScratchRSXMemory();

InitReturnVal_t CMaterialSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	// NOTE! : Overbright is 1.0 so that Hammer will work properly with the white bumped and unbumped lightmaps.
	MathLib_Init( 2.2f, 2.2f, 0.0f, OVERBRIGHT );

	g_pShaderDeviceMgr->SetAdapter( m_nAdapter, m_nAdapterFlags );
	if ( g_pShaderDeviceMgr->Init( ) != INIT_OK )
	{
		DestroyShaderAPI();
		return INIT_FAILED;
	}

	mat_forcemanagedtextureintohardware.SetValue( HardwareConfig()->PreferTexturesInHWMemory() );
	mat_forcehardwaresync.SetValue( HardwareConfig()->PreferHardwareSync() );
	mat_dxlevel.SetValue( HardwareConfig()->GetDXSupportLevel() );

	// Texture manager...
	TextureManager()->Init( m_nAdapterFlags );

	// Shader system!
	ShaderSystem()->Init();

#if defined( WIN32 ) && !defined( _X360 )
	// HACKHACK: <sigh> This horrible hack is possibly the only way to reliably detect an old
	// version of hammer initializing the material system. We need to know this so that we set
	// up the editor materials properly. If we don't do this, we never allocate the white lightmap,
	// for example. We can remove this when we update the SDK!!
	char szExeName[_MAX_PATH];
	if ( ::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), szExeName, sizeof( szExeName ) ) )
	{
		char szRight[20];
		Q_StrRight( szExeName, 11, szRight, sizeof( szRight ) );
		if ( ( !Q_stricmp( szRight, "\\hammer.exe" ) ) || ( !Q_stricmp( szRight, "\\vmview.exe" ) ) )
		{
			m_bRequestedEditorMaterials = true;
			m_bRequestedGBuffers = true;
		}
	}

	// HACKHACK: This will go away when we get rid of tools mode in the first place.
	if ( CommandLine()->FindParm( "-foundrymode" ) != 0 )
	{
		m_bRequestedEditorMaterials = true;
		m_bRequestedGBuffers = true;
	}
	if ( CommandLine()->FindParm( "-tools" ) != 0 )
	{
		m_bRequestedGBuffers = true;
	}
	if ( CommandLine()->FindParm( "-buildcubemaps" ) || CommandLine()->FindParm( "-buildmodelforworld" ) )
	{
		mat_queue_mode.SetValue( 0 );
	}

#endif // WIN32

	m_nConfigurationFlags = 0;
	if ( m_bRequestedEditorMaterials )
	{
		m_nConfigurationFlags |= MATCONFIG_FLAGS_SUPPORT_EDITOR;
	}
	if ( m_bRequestedGBuffers )
	{
		m_nConfigurationFlags |= MATCONFIG_FLAGS_SUPPORT_GBUFFER;
	}

	InitColorCorrection();

	// Set up debug materials...
	CreateDebugMaterials();	

	if ( IsGameConsole() )
	{
		g_pQueuedLoader->InstallLoader( RESOURCEPRELOAD_MATERIAL, &s_ResourcePreloadMaterial );
		g_pQueuedLoader->InstallLoader( RESOURCEPRELOAD_CUBEMAP, &s_ResourcePreloadCubemap );
	}

	if ( IsPS3() )
	{
		InitializePS3Fonts();
		// load the font library and keep it resident forever.
		// for an alternative means, where you just load and unload
		// the library as needed, look at PS3FontLibraryRAII -- 
		// that's disabled at the moment because we evidently render
		// characters ad hoc each frame always.
		PS3InitFontLibrary( 256 * 1024, kPS3_DEFAULT_MAX_USER_FONTS ); // try a 256kb file cache
	}

	// Set up a default material system config
//	GenerateConfigFromConfigKeyValues( &g_config, false );
//	UpdateConfig( false );

	// JAY: Added this command line parameter to force creating <32x32 mips
	// to test for reported performance regressions on some systems
	if ( CommandLine()->FindParm("-forceallmips") )
	{
		extern bool g_bForceTextureAllMips;
		g_bForceTextureAllMips = true;
	}

	BeginRenderTargetAllocation();
	m_CompositeTextureGenerator.Init();
	EndRenderTargetAllocation();
	m_CustomMaterialManager.Init();

	return m_HardwareRenderContext.Init( this );
}

//-----------------------------------------------------------------------------
// For backwards compatability
//-----------------------------------------------------------------------------
static CreateInterfaceFn s_TempCVarFactory;
static CreateInterfaceFn s_TempFileSystemFactory;

void* TempCreateInterface( const char *pName, int *pReturnCode )
{
	void *pRetVal = NULL;

	if ( s_TempCVarFactory )
	{
		pRetVal = s_TempCVarFactory( pName, pReturnCode );
		if (pRetVal)
			return pRetVal;
	}

	pRetVal = s_TempFileSystemFactory( pName, pReturnCode );
	if (pRetVal)
		return pRetVal;

	return NULL;
}


//-----------------------------------------------------------------------------
// Initializes and shuts down the shader API
//-----------------------------------------------------------------------------
CreateInterfaceFn CMaterialSystem::Init( char const* pShaderAPIDLL,
										IMaterialProxyFactory *pMaterialProxyFactory,
										CreateInterfaceFn fileSystemFactory,
										CreateInterfaceFn cvarFactory )
{
	SetShaderAPI( pShaderAPIDLL );

	s_TempCVarFactory = cvarFactory;
	s_TempFileSystemFactory = fileSystemFactory;
	if ( !Connect( TempCreateInterface ) )
		return 0;

	if (Init() != INIT_OK)
		return NULL;

	// save the proxy factory
	m_pMaterialProxyFactory = pMaterialProxyFactory;

	return m_ShaderAPIFactory;
}

void CMaterialSystem::Shutdown( )
{
	DestroyMatQueueThreadPool();

	m_CustomMaterialManager.Shutdown();
	m_CompositeTextureGenerator.Shutdown();

	m_HardwareRenderContext.Shutdown();

	// Clean up standard textures
	ReleaseStandardTextures();

	// Clean up the debug materials
	CleanUpDebugMaterials();

	g_pMorphMgr->FreeMaterials();
	g_pOcclusionQueryMgr->FreeOcclusionQueryObjects();

	GetLightmaps()->Shutdown();
	m_MaterialDict.Shutdown();

	CleanUpErrorMaterial();

	// Shader system!
	ShaderSystem()->Shutdown();

	// Texture manager...
	TextureManager()->Shutdown();

	if (g_pShaderDeviceMgr)
	{
		g_pShaderDeviceMgr->Shutdown();
	}
#if defined( _PS3 )
	// this would have been called in g_pShaderDeviceMgr->Shutdown(), but since g_pShaderDeviceMgr is sometimes NULL, we'll clean up mesh manager once more, just in case
	MeshMgr()->Shutdown();
#endif

	if ( IsPS3() ) 
		PS3DumpFontLibrary();

	BaseClass::Shutdown();
}

void CMaterialSystem::ModInit()
{
	// Set up a default material system config
	GenerateConfigFromConfigKeyValues( &g_config, false );
	UpdateConfig( false );

	// Shader system!
	ShaderSystem()->ModInit();
}

void CMaterialSystem::ModShutdown()
{
	// Shader system!
	ShaderSystem()->ModShutdown();

	// HACK - this is here to unhook ourselves from the client interface, since we're not actually notified when it happens
	m_pMaterialProxyFactory = NULL;
	m_pClientMaterialSystemInterface = NULL;
}

//-----------------------------------------------------------------------------
// Returns the current adapter in use
//-----------------------------------------------------------------------------
IMaterialSystemHardwareConfig *CMaterialSystem::GetHardwareConfig( const char *pVersion, int *returnCode )
{
	return ( IMaterialSystemHardwareConfig * )m_ShaderAPIFactory( pVersion, returnCode );
}


//-----------------------------------------------------------------------------
// Returns the current adapter in use
//-----------------------------------------------------------------------------
int CMaterialSystem::GetCurrentAdapter() const
{
	return g_pShaderDevice->GetCurrentAdapter();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMaterialSystem::SetThreadMode( MaterialThreadMode_t nextThreadMode, int nServiceThread ) 
{
	m_IdealThreadMode = nextThreadMode;
	m_nServiceThread = nServiceThread;
}

MaterialThreadMode_t CMaterialSystem::GetThreadMode()
{ 
	return m_ThreadMode; 
}


bool CMaterialSystem::IsRenderThreadSafe( )
{
#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
    return true;
#else
	return	( m_ThreadMode != MATERIAL_QUEUED_THREADED && ThreadInMainThread() ) ||
		( m_ThreadMode == MATERIAL_QUEUED_THREADED && m_nRenderThreadID == ThreadGetCurrentId() );
#endif
}

void CMaterialSystem::OnDebugEvent( const char * pEvent )
{
	g_pShaderDevice->OnDebugEvent( pEvent );
}

bool CMaterialSystem::AllowThreading( bool bAllow, int nServiceThread )
{
	if ( CommandLine()->ParmValue( "-threads", 2 ) < 2 ) // if -threads specified on command line to restrict all the pools then obey and not turn on QMS
		bAllow = false;

	bool bOldAllow = m_bAllowQueuedRendering;

	if ( GetCPUInformation().m_nPhysicalProcessors >= 2 || mat_queue_mode_force_allow.GetBool() )
	{
		m_bAllowQueuedRendering = bAllow;
		bool bQueued = m_IdealThreadMode != MATERIAL_SINGLE_THREADED;
		if ( bAllow && !bQueued )
		{
			// go into queued mode
			DevMsg( "Queued Material System: ENABLED!\n" );
			OnDebugEvent( "Allow Threading");
			SetThreadMode( MATERIAL_QUEUED_THREADED, nServiceThread );
		}
		else if ( !bAllow && bQueued )
		{
			// disabling queued mode just needs to stop the queuing of drawing
			// but still allow other threaded access to the Material System
			// flush the queue
			DevMsg( "Queued Material System: DISABLED!\n" );
			ForceSingleThreaded();
			MaterialLock_t hMaterialLock = Lock();
			SetThreadMode( MATERIAL_SINGLE_THREADED, -1 );
			Unlock( hMaterialLock );
			OnDebugEvent( "Disallow Threading" );
		}
	}
	else
	{
		m_bAllowQueuedRendering = false;
	}

	return bOldAllow;
}

void CMaterialSystem::ExecuteQueued() 
{
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMatRenderContext *CMaterialSystem::GetRenderContext()
{ 
	IMatRenderContext *pResult = m_pRenderContext;
	if ( !pResult )
	{
		pResult = &m_HardwareRenderContext;
		m_pRenderContext = &m_HardwareRenderContext;
	}
	return RetAddRef( pResult ); 
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMatRenderContext *CMaterialSystem::CreateRenderContext( MaterialContextType_t type )
{
	switch ( type )
	{
	case MATERIAL_HARDWARE_CONTEXT:		
		return NULL;

	case MATERIAL_QUEUED_CONTEXT:		
		{
			CMatQueuedRenderContext *pQueuedContext = new CMatQueuedRenderContext;
			pQueuedContext->Init( this, &m_HardwareRenderContext );
			pQueuedContext->BeginQueue( &m_HardwareRenderContext );
			return pQueuedContext;

		}

	case MATERIAL_NULL_CONTEXT:
		{
			CMatRenderContextBase *pNullContext = CreateNullRenderContext();
			pNullContext->Init();
			pNullContext->InitializeFrom( &m_HardwareRenderContext );
			return pNullContext;
		}
	}
	Assert(0);
	return NULL;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMatRenderContext *CMaterialSystem::SetRenderContext( IMatRenderContext *pNewContext )
{
	IMatRenderContext *pOldContext = m_pRenderContext;
	if ( pNewContext )
	{
		pNewContext->AddRef();
		m_pRenderContext = ( assert_cast<IMatRenderContextInternal *>(pNewContext) );
	}
	else
	{
		m_pRenderContext = NULL;
	}
	return pOldContext;
}


//-----------------------------------------------------------------------------
// Get/Set Material proxy factory
//-----------------------------------------------------------------------------
IMaterialProxyFactory* CMaterialSystem::GetMaterialProxyFactory()
{ 
	return m_pMaterialProxyFactory; 
}

void CMaterialSystem::SetMaterialProxyFactory( IMaterialProxyFactory* pFactory )
{
	// Changing the factory requires an uncaching of all materials
	// since the factory may contain different proxies
	UncacheAllMaterials();

	m_pMaterialProxyFactory = pFactory; 
}


IClientMaterialSystem *CMaterialSystem::GetClientMaterialSystemInterface()
{
	if ( m_pClientMaterialSystemInterface )
		return m_pClientMaterialSystemInterface;

	if ( !m_pMaterialProxyFactory )
		return NULL;

	CreateInterfaceFn pClientFactory = m_pMaterialProxyFactory->GetFactory();
	if ( !pClientFactory )
		return NULL;

	m_pClientMaterialSystemInterface = (IClientMaterialSystem *)pClientFactory( VCLIENTMATERIALSYSTEM_INTERFACE_VERSION, NULL );
	return m_pClientMaterialSystemInterface;
}

void CMaterialSystem::RefreshFrontBufferNonInteractive()
{

#ifdef _PS3
	extern bool IsItSafeToRefreshFrontBufferNonInteractivePs3();
	if ( !IsItSafeToRefreshFrontBufferNonInteractivePs3() )
#else
	if ( !ThreadInMainThread() )
#endif
		return;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->RefreshFrontBufferNonInteractive();
}

#if GCM_ALLOW_TIMESTAMPS || X360_ALLOW_TIMESTAMPS
static CThreadFastMutex s_mtFrameTimestamps;
static double s_flMstThreadBeginTimestamp = 0.0f;
static double s_flTotalFrameBeginTimestamp = 0.0f;
static ApplicationPerformanceCountersInfo_t s_apci;

void OnFrameTimestampAvailableMain( float ms )
{
	AUTO_LOCK( s_mtFrameTimestamps );
	s_apci.msMain = ms;
}
void OnFrameTimestampAvailableMST( float ms )
{
	AUTO_LOCK( s_mtFrameTimestamps );
	if ( !ms )
	{
		s_flMstThreadBeginTimestamp = Plat_FloatTime();
	}
	else
	{
		double flMstThreadFrameTimeSec = Plat_FloatTime() - s_flMstThreadBeginTimestamp;
		s_apci.msMST = flMstThreadFrameTimeSec*1000.0f;
	}
}

void OnFrameTimestampAvailableRsx( float ms )
{
	AUTO_LOCK( s_mtFrameTimestamps );
	s_apci.msGPU = ms;
}

void OnFrameTimestampAvailableTotal( float ms )
{
	AUTO_LOCK( s_mtFrameTimestamps );
	s_apci.msTotal = ms;
}

#ifdef _X360
static LARGE_INTEGER s_gpuStartTime;
static LARGE_INTEGER s_gpuTime;

void OnGpuStartFrame(uint32 context)
{
	QueryPerformanceCounter( &s_gpuStartTime );
}

void OnGpuEndFrame(uint32 context)
{
	LARGE_INTEGER current_time;
	QueryPerformanceCounter( &current_time );
	s_gpuTime.QuadPart = current_time.QuadPart - s_gpuStartTime.QuadPart;
}

#endif

#endif

uint32 CMaterialSystem::GetFrameTimestamps( ApplicationPerformanceCountersInfo_t &apci, ApplicationInstantCountersInfo_t & aici )
{
#if defined (_X360) &&  X360_ALLOW_TIMESTAMPS
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	s_apci.msGPU = 1000.0f * ((double)s_gpuTime.QuadPart / (double)(freq.QuadPart));
#endif


#if GCM_ALLOW_TIMESTAMPS || X360_ALLOW_TIMESTAMPS
	AUTO_LOCK( s_mtFrameTimestamps );
	V_memcpy( &apci, &s_apci, sizeof( s_apci ) );
	return 1000;
#else
	return 0;
#endif
}

bool CMaterialSystem::CanDownloadTextures() const
{
	return g_pShaderAPI->CanDownloadTextures();
}

int CMaterialSystem::GetConfigurationFlags( void ) const
{
	return m_nConfigurationFlags;
}

//-----------------------------------------------------------------------------
// Can we use editor materials?
//-----------------------------------------------------------------------------
bool CMaterialSystem::CanUseEditorMaterials( void ) const
{
	return GetConfigurationFlags() & MATCONFIG_FLAGS_SUPPORT_EDITOR;
}

//-----------------------------------------------------------------------------
// Methods related to mode setting...
//-----------------------------------------------------------------------------

// Gets the number of adapters...
int	 CMaterialSystem::GetDisplayAdapterCount() const
{
	return g_pShaderDeviceMgr->GetAdapterCount( );
}

// Returns info about each adapter
void CMaterialSystem::GetDisplayAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const
{
	g_pShaderDeviceMgr->GetAdapterInfo( adapter, info );
}

// Returns the number of modes
int	 CMaterialSystem::GetModeCount( int adapter ) const
{
	return g_pShaderDeviceMgr->GetModeCount( adapter );
}


//-----------------------------------------------------------------------------
// Compatability function, should go away eventually
//-----------------------------------------------------------------------------
static void ConvertModeStruct( ShaderDeviceInfo_t *pMode, const MaterialSystem_Config_t &config ) 
{
	pMode->m_DisplayMode.m_nWidth = config.m_VideoMode.m_Width;					
	pMode->m_DisplayMode.m_nHeight = config.m_VideoMode.m_Height;
	pMode->m_DisplayMode.m_Format = config.m_VideoMode.m_Format;			
	pMode->m_DisplayMode.m_nRefreshRateNumerator = config.m_VideoMode.m_RefreshRate;	
	pMode->m_DisplayMode.m_nRefreshRateDenominator = config.m_VideoMode.m_RefreshRate ? 1 : 0;	
	pMode->m_nBackBufferCount = ( config.m_bWantTripleBuffered && config.WaitForVSync() && !config.Windowed() ) ? 2 : 1; // Only used in ShaderAPIDX10
	pMode->m_nAASamples = config.m_nAASamples;
	pMode->m_nAAQuality = config.m_nAAQuality;
	pMode->m_nDXLevel = config.dxSupportLevel;					
	pMode->m_nWindowedSizeLimitWidth = (int)config.m_WindowedSizeLimitWidth;	
	pMode->m_nWindowedSizeLimitHeight = (int)config.m_WindowedSizeLimitHeight;

	pMode->m_bWindowed = config.Windowed();
	pMode->m_bResizing = config.Resizing();			
	pMode->m_bUseStencil = config.Stencil();
	pMode->m_bLimitWindowedSize = config.LimitWindowedSize();	
	pMode->m_bWaitForVSync = config.WaitForVSync();	
	pMode->m_bScaleToOutputResolution = config.ScaleToOutputResolution();
	pMode->m_bUsingMultipleWindows = config.UsingMultipleWindows();
}

static void ConvertModeStruct( MaterialVideoMode_t *pMode, const ShaderDisplayMode_t &info ) 
{
	pMode->m_Width = info.m_nWidth;					
	pMode->m_Height = info.m_nHeight;
	pMode->m_Format = info.m_Format;			
	pMode->m_RefreshRate = info.m_nRefreshRateNumerator / info.m_nRefreshRateDenominator;
}


//-----------------------------------------------------------------------------
// Returns mode information..
//-----------------------------------------------------------------------------
void CMaterialSystem::GetModeInfo( int nAdapter, int nMode, MaterialVideoMode_t& info ) const
{
	ShaderDisplayMode_t shaderInfo;
	g_pShaderDeviceMgr->GetModeInfo( &shaderInfo, nAdapter, nMode );
	ConvertModeStruct( &info, shaderInfo );
}


//-----------------------------------------------------------------------------
// Returns the mode info for the current display device
//-----------------------------------------------------------------------------
void CMaterialSystem::GetDisplayMode( MaterialVideoMode_t& info ) const
{
	ShaderDisplayMode_t shaderInfo;
	g_pShaderDeviceMgr->GetCurrentModeInfo( &shaderInfo, m_nAdapter );
	ConvertModeStruct( &info, shaderInfo );
}


void CMaterialSystem::ForceSingleThreaded()
{
	if ( !ThreadInMainThread() )
	{
		Error("Can't force single thread from within thread!\n");
	}
	if ( GetThreadMode() != MATERIAL_SINGLE_THREADED )
	{
#ifndef _PS3

		if ( m_pActiveAsyncJob && !m_pActiveAsyncJob->IsFinished() )
		{
			m_pActiveAsyncJob->WaitForFinish();
		}
		SafeRelease( m_pActiveAsyncJob ); 
#else
		if (m_bQMSJobSubmitted)
		{
			g_pGcmSharedData->WaitForQMS();
			m_bQMSJobSubmitted = 0;
		}
#endif

#ifdef MAT_QUEUED_OWN_THREADPOOL
		ThreadRelease();
#else
		g_pShaderAPI->AcquireThreadOwnership();
#endif
		g_pShaderAPI->EnableShaderShaderMutex( false );
		m_pRenderContext =  &m_HardwareRenderContext;
		for ( int i = 0; i < ARRAYSIZE( m_QueuedRenderContexts ); i++ )
		{
			Assert( m_QueuedRenderContexts[i].IsInitialized() );

			m_QueuedRenderContexts[i].EndQueue(true);

			m_QueuedRenderContexts[i].Shutdown();
		}
		if( mat_debugalttab.GetBool() )
		{
			Warning("Forcing queued mode off!\n");
		}

		// NOTE: Must happen after EndQueue or proxies get bound again, which is bad.
		m_ThreadMode = MATERIAL_SINGLE_THREADED;

		m_bForcedSingleThreaded = true;
	}
}
//-----------------------------------------------------------------------------
// Sets the mode...
//-----------------------------------------------------------------------------
bool CMaterialSystem::SetMode( void* hwnd, const MaterialSystem_Config_t &config )
{
	Assert( m_bGeneratedConfig );

	ForceSingleThreaded();

	ShaderDeviceInfo_t info;
	ConvertModeStruct( &info, config );

	bool bPreviouslyUsingGraphics = g_pShaderDevice->IsUsingGraphics();
	bool bOk = g_pShaderAPI->SetMode( hwnd, m_nAdapter, info );
	if ( !bOk )
		return false;

	AllocateScratchRSXMemory();

	TextureManager()->FreeStandardRenderTargets();
	TextureManager()->AllocateStandardRenderTargets();

	// FIXME: There's gotta be a better way of doing this?
	// After the first mode set, make sure to download any textures created
	// before the first mode set. After the first mode set, all textures
	// will be reloaded via the reaquireresources call. Same goes for procedural materials
	if ( !bPreviouslyUsingGraphics )
	{
		if ( IsPC() )
		{
			TextureManager()->RestoreRenderTargets();
			TextureManager()->RestoreNonRenderTargetTextures();
			if ( MaterialSystem()->CanUseEditorMaterials() )
			{
				// We are in Hammer.  Allocate these here since we aren't going to allocate
				// lightmaps.
				// HACK!
				// NOTE! : Overbright is 1.0 so that Hammer will work properly with the white bumped and unbumped lightmaps.
				MathLib_Init( 2.2f, 2.2f, 0.0f, OVERBRIGHT );
				AllocateStandardTextures();
			}
		}

		if ( IsX360() || IsPS3() )
		{
			// shaderapi was not viable at init time, it is now
			TextureManager()->ReloadTextures();
			AllocateStandardTextures();
		}
	}

	g_pShaderDevice->SetHardwareGammaRamp( config.m_fMonitorGamma, config.m_fGammaTVRangeMin, config.m_fGammaTVRangeMax, 
		config.m_fGammaTVExponent, config.m_bGammaTVEnabled );

	// Copy over that state which isn't stored currently in convars
	g_config.m_VideoMode = config.m_VideoMode;
	g_config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, config.Windowed() );
	g_config.SetFlag( MATSYS_VIDCFG_FLAGS_STENCIL, config.Stencil() );
	WriteConfigIntoConVars( config );

	extern void SetupDirtyDiskReportFunc(); 
	SetupDirtyDiskReportFunc();

	return true;
}

// Creates/ destroys a child window
bool CMaterialSystem::AddView( void* hwnd )
{
	return g_pShaderDevice->AddView( hwnd );
}

void CMaterialSystem::RemoveView( void* hwnd )
{
	g_pShaderDevice->RemoveView( hwnd );
}

// Activates a view
void CMaterialSystem::SetView( void* hwnd )
{
	g_pShaderDevice->SetView( hwnd );
}


//-----------------------------------------------------------------------------
// Installs a function to be called when we need to release vertex buffers
//-----------------------------------------------------------------------------
void CMaterialSystem::AddReleaseFunc( MaterialBufferReleaseFunc_t func )
{
	// Shouldn't have two copies in our list
	Assert( m_ReleaseFunc.Find( func ) == -1 );
	m_ReleaseFunc.AddToTail( func );
}

void CMaterialSystem::RemoveReleaseFunc( MaterialBufferReleaseFunc_t func )
{
	int i = m_ReleaseFunc.Find( func );
	if( i != -1 )
		m_ReleaseFunc.Remove(i);
}


//-----------------------------------------------------------------------------
// Installs a function to be called when we need to restore vertex buffers
//-----------------------------------------------------------------------------
void CMaterialSystem::AddRestoreFunc( MaterialBufferRestoreFunc_t func )
{
	// Shouldn't have two copies in our list
	Assert( m_RestoreFunc.Find( func ) == -1 );
	m_RestoreFunc.AddToTail( func );
}

void CMaterialSystem::RemoveRestoreFunc( MaterialBufferRestoreFunc_t func )
{
	int i = m_RestoreFunc.Find( func );
	Assert( i != -1 );
	m_RestoreFunc.Remove(i);
}


//-----------------------------------------------------------------------------
// Installs a function to be called when we need to release vertex buffers
//-----------------------------------------------------------------------------
void CMaterialSystem::AddEndFrameCleanupFunc( EndFrameCleanupFunc_t func )
{
	// Shouldn't have two copies in our list
	Assert( m_EndFrameCleanupFunc.Find( func ) == -1 );
	m_EndFrameCleanupFunc.AddToTail( func );
}

void CMaterialSystem::RemoveEndFrameCleanupFunc( EndFrameCleanupFunc_t func )
{
	int i = m_EndFrameCleanupFunc.Find( func );
	if( i != -1 )
	{
		m_EndFrameCleanupFunc.Remove(i);
	}
}

// Gets called when the level is shuts down, will call the registered callback
void CMaterialSystem::OnLevelShutdown()
{
	// changing contexts away from gameplay
	m_bLevelLoadingComplete = false;

	int nSize = m_OnLevelShutdownFuncs.Count();
	for (int i = 0 ; i < nSize ; ++i)
	{
		COnLevelShutdownFunc & instance = m_OnLevelShutdownFuncs[i];
		instance.m_Func(instance.m_pUserData);
	}
}

// Installs a function to be called when the level is shuts down
bool CMaterialSystem::AddOnLevelShutdownFunc( OnLevelShutdownFunc_t func, void * pUserData )
{
	COnLevelShutdownFunc instance(func, pUserData);
	int i = m_OnLevelShutdownFuncs.Find( instance );
	if (i != -1)
	{
		return false;
	}
	m_OnLevelShutdownFuncs.AddToTail(instance);
	return true;
}

bool CMaterialSystem::RemoveOnLevelShutdownFunc( OnLevelShutdownFunc_t func, void * pUserData )
{
	COnLevelShutdownFunc instance(func, pUserData);
	int i = m_OnLevelShutdownFuncs.Find( instance );
	if (i == -1)
	{
		return false;
	}
	m_OnLevelShutdownFuncs.Remove(i);
	return true;
}

//-----------------------------------------------------------------------------
// // Installs a function to be called when we need to perform operation before next rendering context is started
//-----------------------------------------------------------------------------
void CMaterialSystem::AddEndFramePriorToNextContextFunc( EndFramePriorToNextContextFunc_t func )
{
	// Shouldn't have two copies in our list
	Assert( m_EndFramePriorToNextContextFunc.Find( func ) == m_EndFramePriorToNextContextFunc.InvalidIndex() );
	m_EndFramePriorToNextContextFunc.AddToTail( func );
}

void CMaterialSystem::RemoveEndFramePriorToNextContextFunc( EndFramePriorToNextContextFunc_t func )
{
	int i = m_EndFramePriorToNextContextFunc.Find( func );
	if( i != m_EndFramePriorToNextContextFunc.InvalidIndex() )
	{
		m_EndFramePriorToNextContextFunc.Remove(i);
	}
}

ICustomMaterialManager *CMaterialSystem::GetCustomMaterialManager()
{
	return &m_CustomMaterialManager;
}

ICompositeTextureGenerator *CMaterialSystem::GetCompositeTextureGenerator()
{
	return &m_CompositeTextureGenerator;
}

void CMaterialSystem::OnLevelLoadingComplete()
{
	// called from engine when level has loaded and gameplay is expected to commence
	m_bLevelLoadingComplete = true;
}

//-----------------------------------------------------------------------------
// Called by the shader API when it's just about to lose video memory
//-----------------------------------------------------------------------------
void CMaterialSystem::ReleaseShaderObjects( int nChangeFlags )
{	
	if( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CMaterialSystem::ReleaseShaderObjects\n" );
	}

	Flush( false );

	m_HardwareRenderContext.OnReleaseShaderObjects();

	if ( ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED ) == 0 ) 
	{
		g_pOcclusionQueryMgr->FreeOcclusionQueryObjects();

		// If we're in an alt+tab event don't release managed resources
		bool bReleaseManaged = ( nChangeFlags & MATERIAL_RESTORE_RELEASE_MANAGED_RESOURCES ) ? true : false;
		m_bRestoreManangedResources = bReleaseManaged;

		TextureManager()->ReleaseTextures( bReleaseManaged );
		ReleaseStandardTextures();
		if ( bReleaseManaged )
		{
			GetLightmaps()->ReleaseLightmapPages();
		}
	}
	for (int i = 0; i < m_ReleaseFunc.Count(); ++i)
	{
		m_ReleaseFunc[i]( nChangeFlags );
	}
}

void CMaterialSystem::RestoreShaderObjects( CreateInterfaceFn shaderFactory, int nChangeFlags )
{
#if !defined( _PS3 ) && !defined( _OSX )
	if ( shaderFactory )
	{
		g_pShaderAPI = (IShaderAPI*)shaderFactory( SHADERAPI_INTERFACE_VERSION, NULL );
		g_pShaderDevice = (IShaderDevice*)shaderFactory( SHADER_DEVICE_INTERFACE_VERSION, NULL );
		g_pShaderShadow = (IShaderShadow*)shaderFactory( SHADERSHADOW_INTERFACE_VERSION, NULL );
	}
#endif

	for( MaterialHandle_t i = m_MaterialDict.FirstMaterial(); i != m_MaterialDict.InvalidMaterial(); i = m_MaterialDict.NextMaterial( i ) )
	{
		IMaterialInternal *pMat = m_MaterialDict.GetMaterialInternal( i );
		if ( pMat )
		{
			pMat->ReportVarChanged( NULL );
		}
	}

	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CMaterialSystem::RestoreShaderObjects\n" );
	}

	// Shader API sets this to the max value the card supports when it resets
	// the state, so restore this value.
	g_pShaderAPI->SetAnisotropicLevel( GetCurrentConfigForVideoCard().m_nForceAnisotropicLevel );

	if ( ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED ) == 0 ) 
	{
		// NOTE: render targets must be restored first, then vb/ibs, then managed textures
		// FIXME: Gotta restore lightmap pages + standard textures before restore funcs are called
		// because they use them both.
		TextureManager()->RestoreRenderTargets();
		AllocateStandardTextures();
		if ( m_bRestoreManangedResources )
		{
			GetLightmaps()->RestoreLightmapPages();
		}
		g_pOcclusionQueryMgr->AllocOcclusionQueryObjects();
	}

	for (int i = 0; i < m_RestoreFunc.Count(); ++i)
	{
		m_RestoreFunc[i]( nChangeFlags );
	}

	if ( ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED ) == 0 && m_bRestoreManangedResources ) 
	{
		TextureManager()->RestoreNonRenderTargetTextures();

		// Reset the restore to true in the odd case that we get a RestoreShaderObjects without a ReleaseShaderObjects
		m_bRestoreManangedResources = true;
	}

}


//-----------------------------------------------------------------------------
// Use this to spew information about the 3D layer 
//-----------------------------------------------------------------------------
void CMaterialSystem::SpewDriverInfo() const
{
	Warning( "ShaderAPI: %s\n", m_pShaderDLL );
	g_pShaderDevice->SpewDriverInfo();
}


//-----------------------------------------------------------------------------
// Color converting blitter
//-----------------------------------------------------------------------------

bool CMaterialSystem::ConvertImageFormat( unsigned char *src, enum ImageFormat srcImageFormat,
						unsigned char *dst, enum ImageFormat dstImageFormat, 
						int width, int height, int srcStride, int dstStride )
{
	return ImageLoader::ConvertImageFormat( src, srcImageFormat, 
		dst, dstImageFormat, width, height, srcStride, dstStride );
}

//-----------------------------------------------------------------------------
// Figures out the amount of memory needed by a bitmap
//-----------------------------------------------------------------------------
int CMaterialSystem::GetMemRequired( int width, int height, int depth, ImageFormat format, bool mipmap )
{
	return ImageLoader::GetMemRequired( width, height, depth, format, mipmap );
}


ShaderAPITextureHandle_t CMaterialSystem::GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrameVar, int nTextureChannel )
{
	return ShaderSystem()->GetShaderAPITextureBindHandle( pTexture, nFrameVar, nTextureChannel );
}


//-----------------------------------------------------------------------------
// Method to allow clients access to the MaterialSystem_Config
//-----------------------------------------------------------------------------
MaterialSystem_Config_t& CMaterialSystem::GetConfig()
{
	Assert( m_bGeneratedConfig );
	return g_config;
}


//-----------------------------------------------------------------------------
// Gets image format info
//-----------------------------------------------------------------------------
ImageFormatInfo_t const& CMaterialSystem::ImageFormatInfo( ImageFormat fmt) const
{
	return ImageLoader::ImageFormatInfo(fmt);
}


//-----------------------------------------------------------------------------
// Reads keyvalues for information
//-----------------------------------------------------------------------------
static void ReadInt( KeyValues *pGroup, const char *pName, int nDefaultVal, int nUndefinedVal, int *pDest )
{
	*pDest = pGroup->GetInt( pName, nDefaultVal );
//	Warning( "\t%s = %d\n", pName, *pDest );
	Assert( *pDest != nUndefinedVal );
}

static void ReadFlt( KeyValues *pGroup, const char *pName, float flDefaultVal, float flUndefinedVal, float *pDest )
{
	*pDest = pGroup->GetFloat( pName, flDefaultVal );
//	Warning( "\t%s = %f\n", pName, *pDest );
	Assert( *pDest != flUndefinedVal );
}

static void LoadFlags( KeyValues *pGroup, const char *pName, bool bDefaultValue, unsigned int nFlag, unsigned int *pFlags )
{
	int nValue = pGroup->GetInt( pName, bDefaultValue ? 1 : 0 );
//	Warning( "\t%s = %s\n", pName, nValue ? "true" : "false" );
	if ( nValue )
	{
		*pFlags |= nFlag;
	}
}


//-----------------------------------------------------------------------------
// This is called when the config changes
//-----------------------------------------------------------------------------
void CMaterialSystem::GenerateConfigFromConfigKeyValues( MaterialSystem_Config_t *pConfig, bool bOverwriteCommandLineValues )
{
	KeyValues *pKeyValues = new KeyValues( "config" );
	if ( !pKeyValues )
		return;

	if ( IsPC() && !GetRecommendedVideoConfig( pKeyValues ) ) 
	{
		pKeyValues->deleteThis();
		return;
	}

	float flAspectRatio;
	pConfig->m_Flags = 0;
	ReadInt( pKeyValues, "setting.defaultres", 640, -1, &pConfig->m_VideoMode.m_Width );
	ReadInt( pKeyValues, "setting.defaultresheight", 480, -1, &pConfig->m_VideoMode.m_Height );
	ReadFlt( pKeyValues, "setting.aspectratio", ( 4.0f / 3.0f ), -1, &flAspectRatio );
	
	int nUnsupported = 0;
	ReadInt( pKeyValues, "setting.unsupported", 0, -1, &nUnsupported );
	pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_UNSUPPORTED, nUnsupported != 0 );

#ifdef LINUX

	uint width = 0;
	uint height = 0;
	uint refreshHz = 0; // Not used

	// query backbuffer size (window size whether FS or windowed)
	if( g_pLauncherMgr )
	{
		g_pLauncherMgr->GetNativeDisplayInfo( -1, width, height, refreshHz );
	}

	pConfig->m_VideoMode.m_Width = width;
	pConfig->m_VideoMode.m_Height = height;

#elif defined( _X360 )
	pConfig->m_VideoMode.m_Width = GetSystemMetrics( SM_CXSCREEN );
	pConfig->m_VideoMode.m_Height = GetSystemMetrics( SM_CYSCREEN );
	// We can afford better aniso in standard def
	if ( pConfig->m_VideoMode.m_Width == 640 )
	{
		static ConVarRef mat_forceaniso( "mat_forceaniso" );
		mat_forceaniso.SetValue( 8 );
	}
#elif defined( _PS3 )
	// Shader API does the dirty work of querying cellVideo libraries for screen res, so just piggy back on it
	ShaderDisplayMode_t info;
	g_pShaderDeviceMgr->GetModeInfo( &info, 0, 0 );
	pConfig->m_VideoMode.m_Width = info.m_nWidth;
	pConfig->m_VideoMode.m_Height = info.m_nHeight;
	// We can afford better aniso in standard def
	if ( pConfig->m_VideoMode.m_Width == 640 )
	{
		static ConVarRef mat_forceaniso( "mat_forceaniso" );
		mat_forceaniso.SetValue( 8 );
	}
#endif

	// Destroy the keys.
	pKeyValues->deleteThis();

	m_bGeneratedConfig = true;
}


//-----------------------------------------------------------------------------
// If mat_proxy goes to 0, we need to reload all materials, because their shader
// params might have been messed with.
//-----------------------------------------------------------------------------
static void MatProxyCallback( IConVar *pConVar, const char *old, float flOldValue )
{
	ConVarRef var( pConVar );
	int oldVal = (int)flOldValue;
	if ( var.GetInt() == 0 && oldVal != 0 )
	{
		g_MaterialSystem.ReloadMaterials();
	}
}


//-----------------------------------------------------------------------------
// Convars that control the config record
//-----------------------------------------------------------------------------
ConVar mat_vsync( "mat_vsync", IsGameConsole() ? "1" : "0", FCVAR_NONE, "Force sync to vertical retrace", true, 0.0, true, 1.0 );

// Texture-related
static ConVar mat_forceaniso( "mat_forceaniso", "1" ); // 0 = Bilinear, 1 = Trilinear, 2+ = Aniso
static ConVar mat_filterlightmaps(	"mat_filterlightmaps", "1" );
static ConVar mat_filtertextures(	"mat_filtertextures", "1" );
static ConVar mat_mipmaptextures(	"mat_mipmaptextures", "1" );

static void mat_showmiplevels_Callback_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	// turn off texture filtering if we are showing mip levels.
	mat_filtertextures.SetValue( ( ( ConVar * )var )->GetInt() == 0 );
}
// Debugging textures
static ConVar mat_showmiplevels(	"mat_showmiplevels", "0", FCVAR_CHEAT, "color-code miplevels 2: normalmaps, 1: everything else", mat_showmiplevels_Callback_f );

static ConVar mat_specular(			"mat_specular", "1", 0, "Enable/Disable specularity for perf testing.  Will cause a material reload upon change." );
static ConVar mat_bumpmap(			"mat_bumpmap", "1" );
static ConVar mat_detail_tex(		"mat_detail_tex", "1" );
static ConVar mat_phong(			"mat_phong", "1" );
static ConVar mat_parallaxmap(		"mat_parallaxmap", "1" );
static ConVar mat_reducefillrate(	"mat_reducefillrate", "0" );

// moved up: static ConVar mat_picmip(			"mat_picmip", "0", FCVAR_NONE, "", true, -10, true, 4 );

static ConVar mat_monitorgamma(		"mat_monitorgamma", "2.2", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "monitor gamma (typically 2.2 for CRT and 1.7 for LCD)", true, 1.6f, true, 2.6f  );
static ConVar mat_monitorgamma_tv_range_min( "mat_monitorgamma_tv_range_min", "16" );
static ConVar mat_monitorgamma_tv_range_max( "mat_monitorgamma_tv_range_max", "235" );
// TV's generally have a 2.5 gamma, so we need to convert our 2.2 frame buffer into a 2.5 frame buffer for display on a TV
static ConVar mat_monitorgamma_tv_exp( "mat_monitorgamma_tv_exp", "2.5", 0, "", true, 1.0f, true, 4.0f );

static ConVar mat_monitorgamma_tv_enabled( "mat_monitorgamma_tv_enabled", IsGameConsole() ? "1" : "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "" );

static ConVar mat_triplebuffered(   "mat_triplebuffered", "0", 0, "This means we want triple buffering if we are fullscreen and vsync'd" );
static ConVar mat_antialias(		"mat_antialias", "0" );
static ConVar mat_aaquality(		"mat_aaquality", "0" );
static ConVar mat_diffuse(			"mat_diffuse", "1" );
static ConVar mat_showlowresimage(	"mat_showlowresimage", "0", FCVAR_CHEAT );
static ConVar mat_fullbright(		"mat_fullbright","0", FCVAR_CHEAT );
static ConVar mat_normalmaps(		"mat_normalmaps", "0", FCVAR_CHEAT );
static ConVar mat_measurefillrate(	"mat_measurefillrate", "0", FCVAR_CHEAT );
static ConVar mat_fillrate(			"mat_fillrate", "0", FCVAR_CHEAT );
static ConVar mat_reversedepth(		"mat_reversedepth", "0", FCVAR_CHEAT );
#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
static ConVar mat_bufferprimitives( "mat_bufferprimitives", "0" );	// I'm not seeing any benefit speed wise for buffered primitives on GLM/POSIX (checked via TF2 timedemo) - default to zero
#else
static ConVar mat_bufferprimitives( "mat_bufferprimitives", "1" );
#endif
static ConVar mat_drawflat(			"mat_drawflat","0", FCVAR_CHEAT );
static ConVar mat_softwarelighting( "mat_softwarelighting", "0" );
static ConVar mat_proxy(			"mat_proxy", "0", FCVAR_CHEAT, "", MatProxyCallback );
static ConVar mat_norendering(		"mat_norendering", "0", FCVAR_CHEAT );
static ConVar mat_compressedtextures(  "mat_compressedtextures", "1" );
static ConVar mat_fastspecular(		"mat_fastspecular", "1", 0, "Enable/Disable specularity for visual testing.  Will not reload materials and will not affect perf." );
static ConVar mat_fastnobump(		"mat_fastnobump", "0", FCVAR_CHEAT ); // Binds 1-texel normal map for quick internal testing
static ConVar mat_drawgray(			"mat_drawgray","0", FCVAR_CHEAT );

// These are not controlled by the material system, but are limited by settings in the material system
static ConVar r_shadowrendertotexture(		"r_shadowrendertotexture", "0" );
#if ( defined( CSTRIKE15 ) && defined( _PS3) )
static ConVar r_flashlightdepthtexture(		"r_flashlightdepthtexture", "1" );
#else
static ConVar r_flashlightdepthtexture(		"r_flashlightdepthtexture", "1" );
#endif
// On non-gameconsoles mat_motion_blur_enabled now comes from video.txt/videodefaults.txt
static ConVar mat_motion_blur_enabled( "mat_motion_blur_enabled", IsGameConsole() ? "1" : "0" );

static ConVar mat_paint_enabled( "mat_paint_enabled", "0" );


uint32 g_nDebugVarsSignature = 0;


//-----------------------------------------------------------------------------
// This is called when the config changes
//-----------------------------------------------------------------------------
void CMaterialSystem::ReadConfigFromConVars( MaterialSystem_Config_t *pConfig )
{
	if ( !g_pCVar )
		return;

	// video panel config items
	pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, !mat_vsync.GetBool() );
	pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR, !mat_specular.GetBool() );
	pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_DISABLE_BUMPMAP, !mat_bumpmap.GetBool() );
	pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_DISABLE_DETAIL, !mat_detail_tex.GetBool() );
	pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_DISABLE_PHONG, !mat_phong.GetBool() );
	pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING, mat_parallaxmap.GetBool() );
	pConfig->m_nForceAnisotropicLevel = MAX( mat_forceaniso.GetInt(), 1 );

	// special handling for mat_dxlevel since it must be clamped to allowable values
	int nDxLevel = clamp( mat_dxlevel.GetInt(), g_pMaterialSystemHardwareConfig->GetMinDXSupportLevel(), g_pMaterialSystemHardwareConfig->GetMaxDXSupportLevel() );
	pConfig->dxSupportLevel = nDxLevel;
	mat_dxlevel.SetValue( nDxLevel );

	pConfig->skipMipLevels = mat_picmip.GetInt();

	pConfig->m_fMonitorGamma = mat_monitorgamma.GetFloat();
	pConfig->m_fGammaTVRangeMin = mat_monitorgamma_tv_range_min.GetFloat();
	pConfig->m_fGammaTVRangeMax = mat_monitorgamma_tv_range_max.GetFloat();
	pConfig->m_fGammaTVExponent = mat_monitorgamma_tv_exp.GetFloat();
	pConfig->m_bGammaTVEnabled = mat_monitorgamma_tv_enabled.GetBool();

	pConfig->m_bWantTripleBuffered = mat_triplebuffered.GetBool();
	pConfig->m_nAASamples = mat_antialias.GetInt();
	pConfig->m_nAAQuality = mat_aaquality.GetInt();
	pConfig->bShowDiffuse = mat_diffuse.GetBool();	
//	pConfig->bAllowCheats = false; // hack
	pConfig->bShowNormalMap = mat_normalmaps.GetBool();
	pConfig->bShowLowResImage = mat_showlowresimage.GetBool();
	pConfig->bMeasureFillRate = mat_measurefillrate.GetBool();
	pConfig->bVisualizeFillRate = mat_fillrate.GetBool();
	pConfig->bFilterLightmaps = mat_filterlightmaps.GetBool();
	pConfig->bFilterTextures = mat_filtertextures.GetBool();
	pConfig->bMipMapTextures = mat_mipmaptextures.GetBool();
	pConfig->nShowMipLevels = mat_showmiplevels.GetInt();
	pConfig->bReverseDepth = mat_reversedepth.GetBool();
	pConfig->bBufferPrimitives = mat_bufferprimitives.GetBool();
	pConfig->bDrawFlat = mat_drawflat.GetBool();
	pConfig->bSoftwareLighting = mat_softwarelighting.GetBool();
	pConfig->proxiesTestMode = mat_proxy.GetInt();
	pConfig->m_bSuppressRendering = mat_norendering.GetBool();
	pConfig->bCompressedTextures = mat_compressedtextures.GetBool();
	pConfig->bShowSpecular = mat_fastspecular.GetBool();
	pConfig->nFullbright = mat_fullbright.GetInt();
	pConfig->m_bFastNoBump = mat_fastnobump.GetBool();
	pConfig->m_bMotionBlur = mat_motion_blur_enabled.GetBool();
	pConfig->m_bSupportFlashlight = mat_supportflashlight.GetBool();
	pConfig->m_bShadowDepthTexture = r_flashlightdepthtexture.GetBool();
	pConfig->bDrawGray = mat_drawgray.GetBool();
	
	// PAINT
	pConfig->m_bPaintInGame = mat_paint_enabled.GetBool();
	pConfig->m_bPaintInMap = GetPaintmaps()->IsEnabled();

	ConVarRef csm_quality_level( "csm_quality_level" );
	pConfig->m_nCSMQuality = (CSMQualityMode_t)clamp( csm_quality_level.GetInt(), CSMQUALITY_VERY_LOW, (int)CSMQUALITY_TOTAL_MODES - 1 );

	pConfig->SetFlag( MATSYS_VIDCFG_FLAGS_ENABLE_HDR, HardwareConfig() && HardwareConfig()->GetHDREnabled() );
}


//-----------------------------------------------------------------------------
// This is called when the config changes
//-----------------------------------------------------------------------------
void CMaterialSystem::WriteConfigIntoConVars( const MaterialSystem_Config_t &config )
{
	if ( !g_pCVar )
		return;

	mat_vsync.SetValue( config.WaitForVSync() );
	mat_specular.SetValue( config.UseSpecular() );
	mat_bumpmap.SetValue( config.UseBumpmapping() );
	mat_detail_tex.SetValue( config.UseDetailTexturing() );
	mat_parallaxmap.SetValue( config.UseParallaxMapping() );
	mat_forceaniso.SetValue( config.m_nForceAnisotropicLevel );
	mat_dxlevel.SetValue( config.dxSupportLevel );
	mat_picmip.SetValue( config.skipMipLevels );
	mat_phong.SetValue( config.UsePhong() );

	mat_monitorgamma.SetValue( config.m_fMonitorGamma );
	mat_monitorgamma_tv_range_min.SetValue( config.m_fGammaTVRangeMin );
	mat_monitorgamma_tv_range_max.SetValue( config.m_fGammaTVRangeMax );
	mat_monitorgamma_tv_exp.SetValue( config.m_fGammaTVExponent );
	mat_monitorgamma_tv_enabled.SetValue( config.m_bGammaTVEnabled );

	mat_triplebuffered.SetValue( config.m_bWantTripleBuffered ? 1 : 0 );
	mat_antialias.SetValue( config.m_nAASamples );
	mat_aaquality.SetValue( config.m_nAAQuality );
	mat_diffuse.SetValue( config.bShowDiffuse ? 1 : 0 );	
//	config.bAllowCheats = false; // hack
	mat_normalmaps.SetValue( config.bShowNormalMap ? 1 : 0 );
	mat_showlowresimage.SetValue( config.bShowLowResImage ? 1 : 0 );
	mat_measurefillrate.SetValue( config.bMeasureFillRate ? 1 : 0 );
	mat_fillrate.SetValue( config.bVisualizeFillRate ? 1 : 0 );
	mat_filterlightmaps.SetValue( config.bFilterLightmaps ? 1 : 0 );
	mat_filtertextures.SetValue( config.bFilterTextures ? 1 : 0 );
	mat_mipmaptextures.SetValue( config.bMipMapTextures ? 1 : 0 );
	mat_showmiplevels.SetValue( config.nShowMipLevels );
	mat_reversedepth.SetValue( config.bReverseDepth ? 1 : 0 );
	mat_bufferprimitives.SetValue( config.bBufferPrimitives ? 1 : 0 );
	mat_drawflat.SetValue( config.bDrawFlat ? 1 : 0 );
	mat_softwarelighting.SetValue( config.bSoftwareLighting ? 1 : 0 );
	mat_proxy.SetValue( config.proxiesTestMode );
	mat_norendering.SetValue( config.m_bSuppressRendering ? 1 : 0 );
	mat_compressedtextures.SetValue( config.bCompressedTextures ? 1 : 0 );
	mat_fastspecular.SetValue( config.bShowSpecular ? 1 : 0 );
	mat_fullbright.SetValue( config.nFullbright );
	mat_fastnobump.SetValue( config.m_bFastNoBump ? 1 : 0 );
	bool hdre = config.HDREnabled();
	HardwareConfig()->SetHDREnabled( hdre );
	r_flashlightdepthtexture.SetValue( config.m_bShadowDepthTexture ? 1 : 0 );
	mat_motion_blur_enabled.SetValue( config.m_bMotionBlur ? 1 : 0 );
	mat_supportflashlight.SetValue( config.m_bSupportFlashlight ? 1 : 0 );
	mat_drawgray.SetValue( config.bDrawGray ? 1 : 0 );

	ConVarRef csm_quality_level( "csm_quality_level" );
	csm_quality_level.SetValue( clamp<int>( config.m_nCSMQuality, CSMQUALITY_VERY_LOW, CSMQUALITY_TOTAL_MODES - 1 ) );
}


//-----------------------------------------------------------------------------
// This is called constantly to catch for config changes
//-----------------------------------------------------------------------------
bool CMaterialSystem::OverrideConfig( const MaterialSystem_Config_t &_config, bool forceUpdate )
{
	Assert( m_bGeneratedConfig );
	// internal config settings

#ifndef _GAMECONSOLE
	MaterialSystem_Config_Internal_t config_internal;
	config_internal.gpu_level = gpu_level.GetInt();
#endif

	if ( 
		!memcmp( &_config, &g_config, sizeof(_config) ) 
#ifndef _GAMECONSOLE
		&& !memcmp( &config_internal, &g_config_internal, sizeof(config_internal) ) 
#endif
		)
		return false;

	MaterialLock_t hLock = Lock();
	MaterialSystem_Config_t config = _config;

	// It's illegal to create a window larger than the screen resolution
	if ( config.Windowed() )
	{
		config.m_VideoMode.m_Width = clamp( config.m_VideoMode.m_Width, 0, m_nAdapterInfo.m_nWidth );
		config.m_VideoMode.m_Height = clamp( config.m_VideoMode.m_Height, 0, m_nAdapterInfo.m_nHeight );
	}

	bool bRedownloadLightmaps = false;
	bool bRedownloadTextures = false;
	bool recomputeSnapshots = false;
	bool bReloadMaterials = false;
	bool bResetAnisotropy = false;
	bool bSetStandardVertexShaderConstants = false;
	bool bMonitorGammaChanged = false;
	bool bVideoModeChange = false;
	bool bResetTextureFilter = false;
	bool bForceAltTab = false;

	if ( !g_pShaderDevice->IsUsingGraphics() )
	{
		g_config = config;

#ifndef _GAMECONSOLE
		g_config_internal = config_internal;
#endif

		// Shouldn't call this more than once.
		ColorSpace::SetGamma( 2.2f, 2.2f, OVERBRIGHT, g_config.bAllowCheats, false );
		Unlock( hLock );
		return bRedownloadLightmaps;
	}

	// set the default state since we might be changing the number of
	// texture units, etc. (i.e. we don't want to leave unit 2 in overbright mode
	// if it isn't going to be reset upon each SetDefaultState because there is
	// effectively only one texture unit.)
	g_pShaderAPI->SetDefaultState();
	
	// toggle dx emulation level
	if ( config.dxSupportLevel != g_config.dxSupportLevel )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: Setting bReloadMaterials because new dxlevel = %d and old dxlevel = %d\n",
				( int )config.dxSupportLevel, g_config.dxSupportLevel );
		}

		// Necessary for DXSupportLevelChanged to work 
		g_config.dxSupportLevel = config.dxSupportLevel;

		// This will reset config to match whatever the dxlevel wants
		g_pShaderAPI->DXSupportLevelChanged( config.dxSupportLevel );
		bReloadMaterials = true;
	}

	if ( config.m_nCSMQuality != g_config.m_nCSMQuality )
	{
		//forceUpdate = true;
		bReloadMaterials = true;
		recomputeSnapshots = true;
	}

	if ( config.HDREnabled() != g_config.HDREnabled() )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: Setting forceUpdate, bReloadMaterials, and bForceAltTab because new hdr level = %d and old hdr level = %d\n",
				( int )config.HDREnabled(), g_config.HDREnabled() );
		}

		forceUpdate = true;
		bReloadMaterials = true;
		bForceAltTab = true;
	}

	if ( config.ShadowDepthTexture() != g_config.ShadowDepthTexture() )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: Setting forceUpdate, bReloadMaterials and recomputeSnapshots (ShadowDepthTexture changed: %d -> %d)\n",
				g_config.ShadowDepthTexture() ? 1 : 0, config.ShadowDepthTexture() ? 1 : 0 );
		}

		if ( !IsGameConsole() )
		{
			// On the 360, we don't actually destroy or recreate any render targets when r_shadowdepthtexture changes,
			// so we don't have to do any of this stuff
			forceUpdate = true;
			bReloadMaterials = true;
			recomputeSnapshots = true;
		}
	}

	if ( forceUpdate )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: forceUpdate is true, therefore setting recomputeSnapshots, bRedownloadLightmaps, bRedownloadTextures, bResetAnisotropy, and bSetStandardVertexShaderConstants\n" );
		}
		GetLightmaps()->EnableLightmapFiltering( config.bFilterLightmaps );
		recomputeSnapshots = true;
		bRedownloadLightmaps = true;
		bRedownloadTextures = true;
		bResetAnisotropy = true;
		bSetStandardVertexShaderConstants = true;
	}

	// toggle bump mapping
	if ( config.UseBumpmapping() != g_config.UseBumpmapping() )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: forceUpdate is true, therefore setting recomputeSnapshots, bRedownloadLightmaps, bRedownloadTextures, bResetAnisotropy, and bSetStandardVertexShaderConstants\n" );
		}
		recomputeSnapshots = true;
		bReloadMaterials = true;
		bResetAnisotropy = true;
	}

	// toggle detail texturing
	if ( config.UseDetailTexturing() != g_config.UseDetailTexturing() )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: forceUpdate is true, therefore setting recomputeSnapshots, bRedownloadLightmaps, bRedownloadTextures, bResetAnisotropy, and bSetStandardVertexShaderConstants\n" );
		}
		recomputeSnapshots = true;
		bReloadMaterials = true;
		bResetAnisotropy = true;
	}

	// toggle specularity
	if ( config.UseSpecular() != g_config.UseSpecular() )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new usespecular=%d, old usespecular=%d, setting recomputeSnapshots, bReloadMaterials, and bResetAnisotropy\n", 
				( int )config.UseSpecular(), ( int )g_config.UseSpecular() );
		}
		recomputeSnapshots = true;
		bReloadMaterials = true;
		bResetAnisotropy = true;
	}
	
	// toggle parallax mapping
	if ( config.UseParallaxMapping() != g_config.UseParallaxMapping() )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new UseParallaxMapping=%d, old UseParallaxMapping=%d, setting bReloadMaterials\n",
				( int )config.UseParallaxMapping(), ( int )g_config.UseParallaxMapping() );
		}
		bReloadMaterials = true;
	}

	// toggle phong
	if ( config.UsePhong() != g_config.UsePhong() )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new UsePhong=%d, old UsePhong=%d, setting bReloadMaterials\n",
				( int )config.UsePhong(), ( int )g_config.UsePhong() );
		}
		bReloadMaterials = true;
	}

	// toggle reverse depth
	if ( config.bReverseDepth != g_config.bReverseDepth )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new ReduceFillrate=%d, old ReduceFillrate=%d, setting bReloadMaterials\n",
				( int )config.bReverseDepth, ( int )g_config.bReverseDepth );
		}
		recomputeSnapshots = true;
		bResetAnisotropy = true;
	}

	// toggle no transparency
	if ( config.bNoTransparency != g_config.bNoTransparency )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new bNoTransparency=%d, old bNoTransparency=%d, setting recomputeSnapshots and bResetAnisotropy\n",
				( int )config.bNoTransparency, ( int )g_config.bNoTransparency );
		}
		recomputeSnapshots = true;
		bResetAnisotropy = true;
	}

	// toggle lightmap filtering
	if ( config.bFilterLightmaps != g_config.bFilterLightmaps )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new bFilterLightmaps=%d, old bFilterLightmaps=%d, setting EnableLightmapFiltering\n",
				( int )config.bFilterLightmaps, ( int )g_config.bFilterLightmaps );
		}
		GetLightmaps()->EnableLightmapFiltering( config.bFilterLightmaps );
	}
	
	// toggle software lighting
	if ( config.bSoftwareLighting != g_config.bSoftwareLighting )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new bSoftwareLighting=%d, old bSoftwareLighting=%d, setting bReloadMaterials\n",
				( int )config.bFilterLightmaps, ( int )g_config.bFilterLightmaps );
		}
		bReloadMaterials = true;
	}

#ifndef _GAMECONSOLE
	if ( config_internal.gpu_level != g_config_internal.gpu_level )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new gpu_level=%d, old gpu_level=%d, setting bReloadMaterials\n",
				( int )config_internal.gpu_level, ( int )g_config_internal.gpu_level );
		}
		bReloadMaterials = true;
	}

#endif

	// generic things that cause us to redownload lightmaps
	if ( config.bAllowCheats != g_config.bAllowCheats )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new bAllowCheats=%d, old bAllowCheats=%d, setting bRedownloadLightmaps\n",
				( int )config.bAllowCheats, ( int )g_config.bAllowCheats );
		}
		bRedownloadLightmaps = true;
	}

	// generic things that cause us to redownload textures
	if ( config.bAllowCheats != g_config.bAllowCheats ||
		config.skipMipLevels != g_config.skipMipLevels  ||
		config.nShowMipLevels != g_config.nShowMipLevels  ||
		( config.bCompressedTextures != g_config.bCompressedTextures ) ||
		config.bShowLowResImage != g_config.bShowLowResImage ||
		config.bDrawGray != g_config.bDrawGray
		)
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: setting bRedownloadTextures, recomputeSnapshots, and bResetAnisotropy\n" );
		}
		bRedownloadTextures = true;
		recomputeSnapshots = true;
		bResetAnisotropy = true;
	}		

	if ( config.m_nForceAnisotropicLevel != g_config.m_nForceAnisotropicLevel )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new m_nForceAnisotropicLevel: %d, old m_nForceAnisotropicLevel: %d, setting bResetAnisotropy and bResetTextureFilter\n",
				( int )config.m_nForceAnisotropicLevel, ( int )g_config.m_nForceAnisotropicLevel );
		}
		bResetAnisotropy = true;
		bResetTextureFilter = true;
	}

	if ( config.m_fMonitorGamma != g_config.m_fMonitorGamma || config.m_fGammaTVRangeMin != g_config.m_fGammaTVRangeMin ||
		config.m_fGammaTVRangeMax != g_config.m_fGammaTVRangeMax ||	config.m_fGammaTVExponent != g_config.m_fGammaTVExponent ||
		config.m_bGammaTVEnabled != g_config.m_bGammaTVEnabled )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: new monitorgamma: %f, old monitorgamma: %f, setting bMonitorGammaChanged\n",
				config.m_fMonitorGamma, g_config.m_fMonitorGamma );
		}
		bMonitorGammaChanged = true;
	}

	if ( config.m_VideoMode.m_Width != g_config.m_VideoMode.m_Width ||
		config.m_VideoMode.m_Height != g_config.m_VideoMode.m_Height ||
		config.m_VideoMode.m_RefreshRate != g_config.m_VideoMode.m_RefreshRate ||
		config.m_nAASamples != g_config.m_nAASamples ||
		config.m_nAAQuality != g_config.m_nAAQuality ||
		config.Windowed() != g_config.Windowed() ||
		config.NoWindowBorder() != g_config.NoWindowBorder() ||
		config.Stencil() != g_config.Stencil() )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: video mode changed for one of various reasons\n" );
		}
		bVideoModeChange = true;
	}

	// Toggle triple buffering
	if ( config.m_bWantTripleBuffered != g_config.m_bWantTripleBuffered )
	{
		// Only force a video mode change if we are fullscreen and vsync'd
		if ( ( IsGameConsole() || !config.Windowed() ) && ( config.WaitForVSync() ) )
		{
			if ( mat_debugalttab.GetBool() )
			{
				Warning( "mat_debugalttab: video mode changed because triple buffering changed\n" );
			}
			bVideoModeChange = true;
		}
	}

	// toggle wait for vsync
	if ( (IsGameConsole() || !config.Windowed()) && (config.WaitForVSync() != g_config.WaitForVSync()) )
	{
#		if ( !defined( _X360 ) )
		{
			if ( mat_debugalttab.GetBool() )
			{
				Warning( "mat_debugalttab: video mode changed due to toggle of wait for vsync\n" );
			}
			bVideoModeChange = true;
		}
#		else
		{
			g_pShaderAPI->EnableVSync_360( config.WaitForVSync() );
		}
#		endif
	}


	g_config = config;
#ifndef _GAMECONSOLE
	g_config_internal = config_internal;
#endif

	if ( bRedownloadTextures || bRedownloadLightmaps )
	{
		// Get rid of this?
		ColorSpace::SetGamma( 2.2f, 2.2f, OVERBRIGHT, g_config.bAllowCheats, false );
	}

	// 360 does not support various configuration changes and cannot reload materials
	if ( !IsGameConsole() )
	{
		if ( bResetAnisotropy || recomputeSnapshots || bRedownloadLightmaps ||
			bRedownloadTextures || bResetAnisotropy || bVideoModeChange ||
			bSetStandardVertexShaderConstants || bResetTextureFilter )
		{
			Unlock( hLock );
			ForceSingleThreaded();
			hLock = Lock();
		}
	}
	if ( bReloadMaterials && !IsGameConsole() )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: ReloadMaterials\n" );
		}
		ReloadMaterials();
	}

	// 360 does not support various configuration changes and cannot reload textures
	// 360 has no reason to reload textures, it's unnecessary and massively expensive
	// 360 does not use this path as an init affect to get its textures into memory
	if ( bRedownloadTextures && !IsGameConsole() )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: redownloading textures\n" );
		}

		if ( CanDownloadTextures() )
		{
			TextureManager()->RestoreRenderTargets();
			TextureManager()->RestoreNonRenderTargetTextures();
		}
	}
	else if ( bResetTextureFilter )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: ResetTextureFilteringState\n" );
		}
		TextureManager()->ResetTextureFilteringState();
	}

	// Recompute all state snapshots
	if ( recomputeSnapshots )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: RecomputeAllStateSnapshots\n" );
		}
		RecomputeAllStateSnapshots();
	}

	if ( bResetAnisotropy )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: SetAnisotropicLevel\n" );
		}
		g_pShaderAPI->SetAnisotropicLevel( config.m_nForceAnisotropicLevel );
	}

	if ( bSetStandardVertexShaderConstants )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: SetStandardVertexShaderConstants\n" );
		}
		g_pShaderAPI->SetStandardVertexShaderConstants( OVERBRIGHT );
	}

	if ( bMonitorGammaChanged )
	{
		if( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: SetHardwareGammaRamp\n" );
		}
		g_pShaderDevice->SetHardwareGammaRamp( config.m_fMonitorGamma, config.m_fGammaTVRangeMin, config.m_fGammaTVRangeMax, 
			config.m_fGammaTVExponent, config.m_bGammaTVEnabled );
	}

	if ( bVideoModeChange )
	{
		if ( mat_debugalttab.GetBool() )
		{
			Warning( "mat_debugalttab: ChangeVideoMode\n" );
		}
		ShaderDeviceInfo_t info;
		ConvertModeStruct( &info, config );
		g_pShaderAPI->ChangeVideoMode( info );
	}

	if ( bForceAltTab )
	{
		// Simulate an Alt-Tab
//		g_pShaderAPI->ReleaseResources();
//		g_pShaderAPI->ReacquireResources();
	}

	Unlock( hLock );
	if ( bVideoModeChange )
	{
		ForceSingleThreaded();
	}
	return bRedownloadLightmaps;
}


//-----------------------------------------------------------------------------
// This is called when the config changes
//-----------------------------------------------------------------------------
bool CMaterialSystem::UpdateConfig( bool forceUpdate )
{
	int nUpdateFlags = 0;
	if ( g_pCVar && g_pCVar->HasQueuedMaterialThreadConVarSets() )
	{
		ForceSingleThreaded();
		nUpdateFlags = g_pCVar->ProcessQueuedMaterialThreadConVarSets();
	}

	MaterialSystem_Config_t config = g_config;
#ifndef DEDICATED
	ReadConfigFromConVars( &config );
#endif
	return OverrideConfig( config, forceUpdate );
}



void CMaterialSystem::ReleaseResources()
{
	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CMaterialSystem::ReleaseResources\n" );
	}
	g_pShaderDevice->ReleaseResources();
#if defined( FEATURE_SUBD_SUPPORT )
	g_pSubDMgr->ReleaseResources();
#endif
}

void CMaterialSystem::ReacquireResources()
{
	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CMaterialSystem::ReacquireResources\n" );
	}
	g_pShaderDevice->ReacquireResources();
#if defined( FEATURE_SUBD_SUPPORT )
	g_pSubDMgr->ReacquireResources();
#endif
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMaterialSystem::OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices )
{
	if ( IsInStubMode() )
	{
		return false;
	}

	return GetRenderContextInternal()->OnDrawMesh( pMesh, firstIndex, numIndices );
}

bool CMaterialSystem::OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists )
{
	if ( IsInStubMode() )
		return false;

	return GetRenderContextInternal()->OnDrawMesh( pMesh, pLists, nLists );
}

bool CMaterialSystem::OnDrawMeshModulated( IMesh *pMesh, const Vector4D &diffuseModulation, int firstIndex, int numIndices )
{
	if ( IsInStubMode() )
		return false;

	return GetRenderContextInternal()->OnDrawMeshModulated( pMesh, diffuseModulation, firstIndex, numIndices );
}

void CMaterialSystem::OnThreadEvent( uint32 threadEvent )
{
	m_threadEvents.AddToTail( threadEvent );
}


//-----------------------------------------------------------------------------
// Creates a procedural texture
//-----------------------------------------------------------------------------
ITexture *CMaterialSystem::CreateProceduralTexture( 
	const char			*pTextureName, 
	const char			*pTextureGroupName, 
	int					w, 
	int					h, 
	ImageFormat			fmt, 
	int					nFlags )
{
	ITextureInternal* pTex = TextureManager()->CreateProceduralTexture( pTextureName, pTextureGroupName, w, h, 1, fmt, nFlags );
	return pTex;
}
#if defined( _X360 )

//-----------------------------------------------------------------------------
// Create a texture for displaying gamerpics.
// This function allocates the texture in the correct gamerpic format, but it does not fill in the gamerpic data.
//-----------------------------------------------------------------------------
ITexture *CMaterialSystem::CreateGamerpicTexture(
	const char			*pTextureName,
	const char			*pTextureGroupName,
	int					nFlags )
{
	return CreateProceduralTexture( pTextureName, pTextureGroupName, g_GamerpicSize, g_GamerpicSize, g_GamerpicFormat, nFlags );
}

//-----------------------------------------------------------------------------
// Update the given texture with the player gamerpic for the local player at the given index.
// Note: this texture must be the correct size and format. Use CreateGamerpicTexture.
//-----------------------------------------------------------------------------
bool CMaterialSystem::UpdateLocalGamerpicTexture(
	ITexture			*pTexture, 
	DWORD				userIndex )
{
	Assert( pTexture != NULL );
	Assert( pTexture->GetActualWidth() == g_GamerpicSize && pTexture->GetActualHeight() == g_GamerpicSize );
	Assert( pTexture->GetImageFormat() == g_GamerpicFormat );
	Assert( userIndex >= 0 && userIndex < 4 );

	// lock
	CPixelWriter writer;
	g_pShaderAPI->ModifyTexture( ((ITextureInternal*)pTexture)->GetTextureHandle( 0 ) );
	g_pShaderAPI->TexLock( 0, 0, 0, 0, g_GamerpicSize, g_GamerpicSize, writer );

	// Write the gamerpic to the texture.
	BYTE *pBuf = (BYTE*)writer.GetPixelMemory();
	DWORD retVal = XUserReadGamerPicture( userIndex, FALSE, pBuf, g_GamerpicSize * writer.GetPixelSize(), g_GamerpicSize * writer.GetPixelSize(), NULL );

	// unlock
	g_pShaderAPI->TexUnlock();

	return (retVal == ERROR_SUCCESS);
}

//-----------------------------------------------------------------------------
// Update the given texture with a remote player's gamerpic.
// Note: this texture must be the correct size and format. Use CreateGamerpicTexture.
//-----------------------------------------------------------------------------
bool CMaterialSystem::UpdateRemoteGamerpicTexture(
	ITexture			*pTexture,
	XUID				xuid )
{
	Assert( pTexture != NULL );
	Assert( pTexture->GetActualWidth() == g_GamerpicSize && pTexture->GetActualHeight() == g_GamerpicSize );
	Assert( pTexture->GetImageFormat() == g_GamerpicFormat );

	//
	// Read the remote player's profile.
	//

	const DWORD xuidCount = 1;
	XUID xuids[xuidCount];
	xuids[0] = xuid;

	const DWORD settingIdCount = 1;
	DWORD settingIds[settingIdCount];
	settingIds[0] = XPROFILE_GAMERCARD_PICTURE_KEY;

	// Get the size of the results.
	DWORD resultsSize = 0;
	DWORD retVal = XUserReadProfileSettingsByXuid( 0, XBX_GetPrimaryUserId(), xuidCount, xuids, settingIdCount, settingIds, &resultsSize, 0, 0 );
	if ( retVal != ERROR_INSUFFICIENT_BUFFER )
	{
		return false;
	}

	Assert( resultsSize > 0 );

	// Get the profile with the correct results size.
	CArrayAutoPtr<unsigned char> spResultsBuffer( new unsigned char[resultsSize] );
	XUSER_READ_PROFILE_SETTING_RESULT *pResults = (XUSER_READ_PROFILE_SETTING_RESULT*)spResultsBuffer.Get();
	retVal = XUserReadProfileSettingsByXuid( 0, 0, xuidCount, xuids, settingIdCount, settingIds, &resultsSize, pResults, 0 );
	if ( retVal != ERROR_SUCCESS || pResults->dwSettingsLen == 0 )
	{
		return false;
	}

	// lock
	CPixelWriter writer;
	g_pShaderAPI->ModifyTexture( ((ITextureInternal*)pTexture)->GetTextureHandle( 0 ) );
	g_pShaderAPI->TexLock( 0, 0, 0, 0, g_GamerpicSize, g_GamerpicSize, writer );

	// Write the gamerpic to the texture.
	BYTE *pBuf = (BYTE*)writer.GetPixelMemory();
	retVal = XUserReadGamerPictureByKey( &(pResults->pSettings[0].data), FALSE, pBuf, g_GamerpicSize * writer.GetPixelSize(), g_GamerpicSize * writer.GetPixelSize(), NULL );

	// unlock
	g_pShaderAPI->TexUnlock();

	return (retVal == ERROR_SUCCESS);
}

#endif // _X360

	
//-----------------------------------------------------------------------------
// Create new materials	(currently only used by the editor!)
//-----------------------------------------------------------------------------
IMaterial *CMaterialSystem::CreateMaterial( const char *pMaterialName, KeyValues *pVMTKeyValues )
{
	// For not, just create a material with no default settings
	IMaterialInternal* pMaterial = IMaterialInternal::CreateMaterial( pMaterialName, TEXTURE_GROUP_OTHER, pVMTKeyValues );
	pMaterial->IncrementReferenceCount();
	AddMaterialToMaterialList( pMaterial );
	return pMaterial->GetQueueFriendlyVersion();
}


//-----------------------------------------------------------------------------
// Finds or creates a procedural material
//-----------------------------------------------------------------------------
IMaterial *CMaterialSystem::FindProceduralMaterial( const char *pMaterialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues )
{
	// We need lower-case symbols for this to work
	int nLen = Q_strlen( pMaterialName ) + 1;
	char *pTemp = (char*)stackalloc( nLen );
	Q_strncpy( pTemp, pMaterialName, nLen );
	Q_strlower( pTemp );
	Q_FixSlashes( pTemp, '/' );

	// 'true' causes the search to find procedural materials
	IMaterialInternal *pMaterial = m_MaterialDict.FindMaterial( pTemp, true );
	if ( pMaterial )
	{
		if ( pVMTKeyValues != NULL )
		{
			pVMTKeyValues->deleteThis();
		}
	}
	else
	{
		if ( pVMTKeyValues != NULL )
		{
			pMaterial = IMaterialInternal::CreateMaterial( pMaterialName, pTextureGroupName, pVMTKeyValues );
			AddMaterialToMaterialList( static_cast<IMaterialInternal*>( pMaterial ) );
		}
		else
		{
			pMaterial = g_pErrorMaterial;
		}
	}

	return pMaterial->GetQueueFriendlyVersion();
}


//-----------------------------------------------------------------------------
// Search by name
//-----------------------------------------------------------------------------
IMaterial* CMaterialSystem::FindMaterial( char const *pMaterialName, const char *pTextureGroupName, bool bComplain, const char *pComplainPrefix )
{
	if ( g_pResourceAccessControl )
	{
		if ( !g_pResourceAccessControl->IsAccessAllowed( RESOURCE_MATERIAL, pMaterialName ) )
			return g_pErrorMaterial->GetRealTimeVersion();
	}

	if ( !pMaterialName )
	{
		return g_pErrorMaterial->GetQueueFriendlyVersion();
	}

	// We need lower-case symbols for this to work
	int nLen = Q_strlen( pMaterialName ) + 1;
	char *pFixedNameTemp = (char*)stackalloc( nLen );
	char *pTemp = (char*)stackalloc( nLen );
	Q_strncpy( pFixedNameTemp, pMaterialName, nLen );
	Q_strlower( pFixedNameTemp );
#ifdef PLATFORM_POSIX
	// strip extensions needs correct slashing for the OS, so fix it up early for Posix
	Q_FixSlashes( pFixedNameTemp, '/' );
#endif
	Q_StripExtension( pFixedNameTemp, pTemp, nLen );
#ifndef PLATFORM_POSIX
	Q_FixSlashes( pTemp, '/' );
#endif
	
	Assert( nLen >= Q_strlen( pTemp ) + 1 );

	IMaterialInternal *pExistingMaterial = m_MaterialDict.FindMaterial( pTemp, false );	// 'false' causes the search to find only file-created materials

	if ( pExistingMaterial )
		return pExistingMaterial->GetQueueFriendlyVersion();

#if defined( DEVELOPMENT_ONLY ) || defined( ALLOW_TEXT_MODE )
	static bool s_bTextMode = CommandLine()->HasParm( "-textmode" );
	if ( s_bTextMode )
		return g_pErrorMaterial->GetQueueFriendlyVersion();
#endif

//	if ( !m_MaterialDict.IsMissing(pTemp) )
	{
		// It hasn't been seen yet, so let's check to see if it's in the filesystem.
		nLen = Q_strlen( "materials/" ) + Q_strlen( pTemp ) + Q_strlen( ".vmt" ) + 1;
		char *vmtName = (char *)stackalloc( nLen );

		// Check to see if this is a UNC-specified material name
		bool bIsUNC = pTemp[0] == '/' && pTemp[1] == '/' && pTemp[2] != '/';
		if ( !bIsUNC )
		{
			Q_strncpy( vmtName, "materials/", nLen );
			Q_strncat( vmtName, pTemp, nLen, COPY_ALL_CHARACTERS );
		}
		else
		{
			Q_strncpy( vmtName, pTemp, nLen );
		}

		//Q_strncat( vmtName, ".vmt", nLen, COPY_ALL_CHARACTERS );
		Assert( nLen >= (int)Q_strlen( vmtName ) + 1 );

		CUtlVector<FileNameHandle_t> includes;
		KeyValues *pKeyValues = new KeyValues("vmt");
		KeyValues *pPatchKeyValues = new KeyValues( "vmt_patches" );
		if ( !LoadVMTFile( *pKeyValues, *pPatchKeyValues, vmtName, true, &includes ) )
		{
			pKeyValues->deleteThis();
			pKeyValues = NULL;
			pPatchKeyValues->deleteThis();
			pPatchKeyValues = NULL;
		}
		else
		{
			char *matNameWithExtension;
			nLen = Q_strlen( pTemp ) + Q_strlen( ".vmt" ) + 1;
			matNameWithExtension = (char *)stackalloc( nLen );
			Q_strncpy( matNameWithExtension, pTemp, nLen );
			Q_strncat( matNameWithExtension, ".vmt", nLen, COPY_ALL_CHARACTERS );

			IMaterialInternal *pMat = NULL;
			if ( !Q_stricmp( pKeyValues->GetName(), "subrect" ) )
			{
				pMat = m_MaterialDict.AddMaterialSubRect( matNameWithExtension, pTextureGroupName, pKeyValues, pPatchKeyValues );
			}
			else
			{
				pMat = m_MaterialDict.AddMaterial( matNameWithExtension, pTextureGroupName );
				if ( g_pShaderDevice->IsUsingGraphics() )
				{
					if ( !bIsUNC )
					{
						m_pForcedTextureLoadPathID = "GAME";
					}
					pMat->PrecacheVars( pKeyValues, pPatchKeyValues, &includes );
					m_pForcedTextureLoadPathID = NULL;
				}
			}
			pKeyValues->deleteThis();
			pPatchKeyValues->deleteThis();
			return pMat->GetQueueFriendlyVersion();
		}

		if ( bComplain )
		{
			Assert( pTemp );

			// convert to lowercase
			nLen = Q_strlen(pTemp) + 1 ;
			char *name = (char*)stackalloc( nLen );
			Q_strncpy( name, pTemp, nLen );
			Q_strlower( name );

			if ( m_MaterialDict.NoteMissing( name ) )
			{
				if ( pComplainPrefix )
				{
					DevWarning( "%s", pComplainPrefix );
				}
				DevWarning( "material \"%s\" not found.\n", name );
			}
		}
	}

	return g_pErrorMaterial->GetQueueFriendlyVersion();
}

bool CMaterialSystem::LoadKeyValuesFromVMTFile( KeyValues &vmtKeyValues, const char *pMaterialName, bool bUsesUNCFilename )
{
	CUtlVector<FileNameHandle_t> includes;
	KeyValues *pPatchKeyValues = new KeyValues( "vmt_patches" );

	bool bResult = LoadVMTFile( vmtKeyValues, *pPatchKeyValues, pMaterialName, bUsesUNCFilename, &includes );

	// we don't need these, they were applied to vmtKeyValues
	pPatchKeyValues->deleteThis();
	pPatchKeyValues = NULL;

	return bResult;
}

static char const *TextureAliases[] = 
{
	// this table is only here for backwards compatibility where a render target change was made,
	// and we wish to redirect an existing old client.dll for hl2 to reference this texture. It's
	// not meant as a general texture aliasing system.
	"_rt_FullFrameFB1", "_rt_FullScreen"
};

ITexture *CMaterialSystem::FindTexture( char const *pTextureName, const char *pTextureGroupName, bool bComplain /* = false */, int nAdditionalCreationFlags /* = 0 */ )
{
	ITextureInternal *pTexture = TextureManager()->FindOrLoadTexture( pTextureName, pTextureGroupName, nAdditionalCreationFlags );
	Assert( pTexture );
	if ( pTexture->IsError() && !CommandLine()->HasParm( "-textmode" ) )
	{
		if ( IsPC() )
		{
			for ( int i=0; i<NELEMS( TextureAliases ); i+=2 )
			{
				if ( !Q_stricmp( pTextureName, TextureAliases[i] ) )
				{
					return FindTexture( TextureAliases[i+1], pTextureGroupName, bComplain, nAdditionalCreationFlags );
				}
			}
		}
		if ( bComplain )
		{
			DevWarning( "Texture '%s' not found.\n", pTextureName );
		}
	}

	return pTexture;
}

bool CMaterialSystem::IsTextureLoaded( char const* pTextureName ) const
{
	return TextureManager()->IsTextureLoaded( pTextureName );
}

bool CMaterialSystem::GetTextureInformation( char const *szTextureName, MaterialTextureInfo_t &info ) const
{
	return TextureManager()->GetTextureInformation( szTextureName, info );
}

void CMaterialSystem::AddTextureAlias( const char *pAlias, const char *pRealName )
{
	TextureManager()->AddTextureAlias( pAlias, pRealName );
}

void CMaterialSystem::RemoveTextureAlias( const char *pAlias )
{
	TextureManager()->RemoveTextureAlias( pAlias );
}

void CMaterialSystem::SetExcludedTextures( const char *pScriptName, bool bUsingWeaponModelCache )
{
	TextureManager()->SetExcludedTextures( pScriptName, bUsingWeaponModelCache );
}

void CMaterialSystem::UpdateExcludedTextures( void )
{
	TextureManager()->UpdateExcludedTextures();

	// Have to re-setup the representative textures since they may have been removed out from under us by the queued loader.
	for ( MaterialHandle_t i = FirstMaterial(); i != InvalidMaterial(); i = NextMaterial( i ) )
	{
		GetMaterialInternal( i )->FindRepresentativeTexture();
		GetMaterialInternal( i )->PrecacheMappingDimensions();
	}
}

void CMaterialSystem::ClearForceExcludes( void )
{
	TextureManager()->ClearForceExcludes();
}

//-----------------------------------------------------------------------------
// Recomputes state snapshots for all materials
//-----------------------------------------------------------------------------
void CMaterialSystem::RecomputeAllStateSnapshots()
{
	g_pShaderAPI->ClearSnapshots();
	for (MaterialHandle_t i = FirstMaterial(); i != InvalidMaterial(); i = NextMaterial(i) )
	{
		GetMaterialInternal(i)->RecomputeStateSnapshots();
	}
	g_pShaderAPI->ResetRenderState();
}


//-----------------------------------------------------------------------------
// Uncache all materials
//-----------------------------------------------------------------------------
void CMaterialSystem::UncacheAllMaterials()
{
	MaterialLock_t hLock = Lock();
	Flush( true );

	for ( MaterialHandle_t i = FirstMaterial(); i != InvalidMaterial(); i = NextMaterial( i ) )
	{
		Assert( GetMaterialInternal(i)->GetReferenceCount() >= 0 );
		GetMaterialInternal(i)->Uncache();
	}
	TextureManager()->RemoveUnusedTextures();
	Unlock( hLock );
}


//-----------------------------------------------------------------------------
// Uncache unused materials
//-----------------------------------------------------------------------------
void CMaterialSystem::UncacheUnusedMaterials( bool bRecomputeStateSnapshots )
{
	MaterialLock_t hLock = Lock();
	Flush( true );

	// We need two loops to make sure we don't reset the snapshots if nothing got removed,
	// otherwise the snapshot recomputation is expensive and avoided at load time
	bool bDidUncacheMaterial = false;
	for ( MaterialHandle_t i = FirstMaterial(); i != InvalidMaterial(); i = NextMaterial(i) )
	{
		IMaterialInternal *pMatInternal = GetMaterialInternal( i );
		Assert( pMatInternal->GetReferenceCount() >= 0 );
		if ( pMatInternal->GetReferenceCount() <= 0 )
		{
			bDidUncacheMaterial = true;
			pMatInternal->Uncache();
		}
	}

	if ( IsX360() && bRecomputeStateSnapshots )
	{
		// Always recompute snapshots because the queued loading process skips it during pre-purge,
		// allowing it to happen just once, here.
		bDidUncacheMaterial = true;
	}

	if ( bDidUncacheMaterial && bRecomputeStateSnapshots )
	{
		// Clear the state snapshots since we are going to rebuild all of them.
		g_pShaderAPI->ClearSnapshots();
		g_pShaderAPI->ClearVertexAndPixelShaderRefCounts();

		for ( MaterialHandle_t i = FirstMaterial(); i != InvalidMaterial(); i = NextMaterial(i) )
		{
			IMaterialInternal *pMatInternal = GetMaterialInternal(i);
			if ( pMatInternal->GetReferenceCount() > 0 )
			{
				// Recompute the state snapshots for the materials that we are keeping
				// since we blew all of them away above.
				pMatInternal->RecomputeStateSnapshots();
			}
		}
		g_pShaderAPI->PurgeUnusedVertexAndPixelShaders();
	}

	if ( bRecomputeStateSnapshots )
	{
		// kick out all per material context datas
		for( MaterialHandle_t i = m_MaterialDict.FirstMaterial(); i != m_MaterialDict.InvalidMaterial(); i = m_MaterialDict.NextMaterial( i ) )
		{
			GetMaterialInternal(i)->ClearContextData();
		}
	}

	TextureManager()->RemoveUnusedTextures();
	Unlock( hLock );
}


//-----------------------------------------------------------------------------
// Release temporary HW memory...
//-----------------------------------------------------------------------------
void CMaterialSystem::ResetTempHWMemory( bool bExitingLevel )
{
	if( !IsPS3() )
	{
		// Doing this on map transitions is not beneficial on PS3 (in fact it may fragment our RSX allocator)
		g_pShaderAPI->DestroyVertexBuffers( bExitingLevel );
	}
	TextureManager()->ReleaseTempRenderTargetBits();
}

//-----------------------------------------------------------------------------
// Get GPU memory usage stats
//-----------------------------------------------------------------------------
void CMaterialSystem::GetGPUMemoryStats( GPUMemoryStats &stats )
{
	g_pShaderAPI->GetGPUMemoryStats( stats );
}

bool CMaterialSystem::IsLevelLoadingComplete() const
{
	return m_bLevelLoadingComplete;
}

void CMaterialSystem::OnAsyncTextureDataComplete( AsyncTextureContext_t *pContext, void *pData, int nNumReadBytes, AsyncTextureLoadError_t loadError )
{
	// queue the async loaded texture data, cannot deal with update the texture data until end-of-frame on the main thread
	AsyncTextureLoad_t textureLoad;
	textureLoad.m_pContext = pContext;
	textureLoad.m_pData = pData;
	textureLoad.m_nNumReadBytes = nNumReadBytes;
	textureLoad.m_LoadError = loadError;
	m_QueuedAsyncTextureLoads.PushItem( textureLoad ); 
}

void CMaterialSystem::ServiceAsyncTextureLoads()
{
	if ( !m_QueuedAsyncTextureLoads.Count() && !m_pActiveAsyncTextureLoad )
	{
		// nothing to do
		return;
	}

	// We don't necessarily process all the elements in the queue in order to avoid
	// large spikes on the main thread
	// Spreading the cost of creating the async textures across multiple frames

	double flStartTime = Plat_FloatTime();
	float flRemainingMaxTimeMs = mat_async_tex_maxtime_ms.GetFloat();


	// Resume FinishAsyncDownload() that was interrupted on the previous frame
	// Need to do it first (before processing any other async textures from the queue)
	// otherwise the VTF texture created on the previous frame will be invalid !
	if ( m_pActiveAsyncTextureLoad )
	{
		m_pActiveAsyncTextureLoad->m_pContext->m_pTexture->FinishAsyncDownload(
			m_pActiveAsyncTextureLoad->m_pContext,
			m_pActiveAsyncTextureLoad->m_pData,
			m_pActiveAsyncTextureLoad->m_nNumReadBytes,
			!m_bLevelLoadingComplete || (m_pActiveAsyncTextureLoad->m_LoadError != ASYNCTEXTURE_LOADERROR_NONE),
			flRemainingMaxTimeMs );

		m_pActiveAsyncTextureLoad = NULL;

		// Limit the amount of time spent creating D3D resources
		float flElapsedMs = (Plat_FloatTime() - flStartTime) * 1000.0f;
		flRemainingMaxTimeMs = mat_async_tex_maxtime_ms.GetFloat() - flElapsedMs;
		if (flRemainingMaxTimeMs < 0.0f)
		{
			return;
		}
	}

	// async texture loads are only valid AFTER the level has completed loading
	// abort data incorrectly delivered during loading or on any data error
	while ( m_QueuedAsyncTextureLoads.PopItem( &m_AsyncTextureLoad ) )
	{		
		bool bDownloadCompleted = m_AsyncTextureLoad.m_pContext->m_pTexture->FinishAsyncDownload( 
			m_AsyncTextureLoad.m_pContext,
			m_AsyncTextureLoad.m_pData, 
			m_AsyncTextureLoad.m_nNumReadBytes, 
			!m_bLevelLoadingComplete || ( m_AsyncTextureLoad.m_LoadError != ASYNCTEXTURE_LOADERROR_NONE ),
			flRemainingMaxTimeMs );

		if ( !bDownloadCompleted )
		{
			// FinishAsyncDownload has been interrupted and need to resume at the next frame
			m_pActiveAsyncTextureLoad = &m_AsyncTextureLoad;
			break;
		}
		else
		{
			m_pActiveAsyncTextureLoad = NULL;
		}

		// Limit the amount of time spent creating D3D resources
		float flElapsedMs = ( Plat_FloatTime() - flStartTime ) * 1000.0f;
		flRemainingMaxTimeMs = mat_async_tex_maxtime_ms.GetFloat() - flElapsedMs;
		if ( flRemainingMaxTimeMs < 0.0f )
		{
			break;
		}
	}
}

//#define SPEW_SERVICE_END_FRAME_TIME
void CMaterialSystem::ServiceEndFramePriorToNextContext()
{
#if !defined( _CERT ) && defined( SPEW_SERVICE_END_FRAME_TIME )
	double flStartTime = Plat_FloatTime();
#endif

	// All these callers are highly specialized handlers aware of
	// this precise calling state. These handlers all need to perform some
	// operation that must be on the main thread while no rendering
	// is concurrent.
	bool bDataProcessed = false;

	volatile int nLastCallIndex;
	volatile EndFramePriorToNextContextFunc_t pLastCallSite;

	for ( int i = 0; i < m_EndFramePriorToNextContextFunc.Count(); ++i )
	{
		nLastCallIndex = i;
		pLastCallSite = m_EndFramePriorToNextContextFunc[ i ];

		bDataProcessed |= m_EndFramePriorToNextContextFunc[ i ]();
	}

	if ( !bDataProcessed )
	{
		// Because this is at the main thread frame boundary, the async texture loading can simply be delayed until the next frame,
		// to lighten the amount of a burst of main thread work. The async texture loading is tolerant of getting delayed because
		// it's only driving a resident texture to a higher/lower resolution. This is more of a specific assist because the above caller
		// is known to be the modelloader handling of weapon meshes eviction/restore.
		ServiceAsyncTextureLoads();
	}

#if !defined( _CERT ) && defined( SPEW_SERVICE_END_FRAME_TIME )
	float flElapsed = ( Plat_FloatTime() - flStartTime ) * 1000.0f;
	if ( flElapsed > 1.0f )
	{
		DevWarning( "ServiceEndFramePriorToNextContext: %.2f ms\n", flElapsed );
	}
#endif
	
	bool bDeviceReady = g_pShaderAPI->CanDownloadTextures();

	if ( m_bDeferredMaterialReload && bDeviceReady)
	{
		m_bDeferredMaterialReload = false;
		char *pReloadSubString = m_pSubString;
		m_pSubString = NULL;
		ReloadMaterials( pReloadSubString );
		if ( pReloadSubString )
		{
			free( pReloadSubString );
		}
	}


}

//-----------------------------------------------------------------------------
// Cache used materials
//-----------------------------------------------------------------------------
void CMaterialSystem::CacheUsedMaterials( )
{
	IMatRenderContextInternal *pRenderContext = GetRenderContextInternal();
	pRenderContext->EvictManagedResources();

	for (MaterialHandle_t i = FirstMaterial(); i != InvalidMaterial(); i = NextMaterial(i) )
	{
		IMaterialInternal* pMat = GetMaterialInternal(i);
		Assert( pMat->GetReferenceCount() >= 0 );


		if( pMat->GetReferenceCount() > 0 )
		{
			pMat->Precache();
		}
	}
	if( mat_forcemanagedtextureintohardware.GetBool() )
	{
		TextureManager()->ForceAllTexturesIntoHardware();
	}
}

//-----------------------------------------------------------------------------
// Reloads textures + materials
//-----------------------------------------------------------------------------
void CMaterialSystem::ReloadTextures( void )
{
	ForceSingleThreaded();
	// 360 should not have gotten here
	Assert( !IsX360() );

	TextureManager()->RestoreRenderTargets();
	TextureManager()->RestoreNonRenderTargetTextures();
}

void CMaterialSystem::ReloadMaterials( const char *pSubString )
{
	bool bDeviceReady = g_pShaderAPI->CanDownloadTextures();

	if ( !bDeviceReady )
	{
		if ( m_bDeferredMaterialReload && !m_pSubString )
			return;	// ignore request, all materials already pending a reload (otherwise malicious user can request only a subset of materials to reload)

		if ( m_pSubString )
		{
			free( m_pSubString );
			m_pSubString = NULL;
		}
		if ( pSubString )
		{
			m_pSubString = strdup( pSubString );
		}
		m_bDeferredMaterialReload = true;
		return;
	}

	ForceSingleThreaded();
	bool bVertexFormatChanged = false;
	if( pSubString == NULL )
	{
		bVertexFormatChanged = true;
		UncacheAllMaterials();
		CacheUsedMaterials();
	}
	else
	{
		Flush( false );
	
		char const chMultiDelim = '*';
		CUtlVector< char > arrSearchSubString;
		CUtlVector< char const * > arrSearchItems;

		if ( strchr( pSubString, chMultiDelim ) )
		{
			arrSearchSubString.SetCount( strlen( pSubString ) + 1 );
			strcpy( arrSearchSubString.Base(), pSubString );
			for ( char * pch = arrSearchSubString.Base(); pch; )
			{
				char *pchEnd = strchr( pch, chMultiDelim );
				pchEnd ? *( pchEnd ++ ) = 0 : 0;
				arrSearchItems.AddToTail( pch );
				pch = pchEnd;
			}
		}

		for (MaterialHandle_t i = FirstMaterial(); i != InvalidMaterial(); i = NextMaterial(i) )
		{
			if( GetMaterialInternal(i)->GetReferenceCount() <= 0 )
				continue;

			char const *szMatName = GetMaterialInternal(i)->GetName();
			
			if ( arrSearchItems.Count() > 1 )
			{
				bool bMatched = false;
				
				for ( int k = 0; !bMatched && ( k < arrSearchItems.Count() ); ++ k )
					if( Q_stristr( szMatName, arrSearchItems[k] ) )
						bMatched = true;
				
				if ( !bMatched )
					continue;
			}
			else
			{
				if( !Q_stristr( szMatName, pSubString ) )
					continue;
			}

			if ( !GetMaterialInternal(i)->IsPrecached() )
			{
				if ( GetMaterialInternal(i)->IsPrecachedVars() )
				{
					GetMaterialInternal(i)->Uncache( );
				}
			}
			else
			{
				VertexFormat_t oldVertexFormat = GetMaterialInternal(i)->GetVertexFormat();
				GetMaterialInternal(i)->Uncache();
				GetMaterialInternal(i)->Precache();
				GetMaterialInternal(i)->ReloadTextures();
				if( GetMaterialInternal(i)->GetVertexFormat() != oldVertexFormat )
				{
					bVertexFormatChanged = true;
				}
			}
		}
	}

	if( bVertexFormatChanged )
	{
		// Reloading materials could cause a vertex format change, so
		// we need to release and restore
		// NOTE: Not calling ReleaseShaderObjects/RestoreShaderObjects
		// because we don't want to free anything other than vbs
		// FIXME: Should I add a flags to the release func? Probably.
		ReleaseShaderObjects( MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED );
		RestoreShaderObjects( NULL, MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED );
	}
}

#define NOMINAL_LIGHTMAP 1.0f 
//-----------------------------------------------------------------------------
// Allocates the standard textures used by the material system
//-----------------------------------------------------------------------------
void CMaterialSystem::AllocateStandardTextures()
{
	if ( m_StandardTexturesAllocated )
		return;

	m_StandardTexturesAllocated = true;

	// This texture is where subdivision patch list goes
#if defined( FEATURE_SUBD_SUPPORT )
	if ( g_pSubDMgr->ShouldAllocateTextures() )
	{
		g_pSubDMgr->AllocateTextures();
	}
#endif

	float nominal_lightmap_value = NOMINAL_LIGHTMAP;
	if ( HardwareConfig()->GetHDRType() == HDR_TYPE_INTEGER )
	{
		nominal_lightmap_value = NOMINAL_LIGHTMAP/16.0;
	}

	unsigned char texel[4];
	texel[3] = 255;

	// using a pixel writer to hide platform endian issues
	// the PC passes through, the 360 has endian swapped formats
	// NOTE: the IMAGE_FORMAT_BGRA8888 is used when expecting a IMAGE_FORMAT_BGRX8888 to ensure the alpha component gets converted/swapped
	CPixelWriter pixelWriter;
	unsigned char outTexel[4];

	int tcFlags = TEXTURE_CREATE_MANAGED;
	int tcFlagsSRGB = TEXTURE_CREATE_MANAGED | (IsPS3()?0:TEXTURE_CREATE_SRGB);
	if ( IsX360() )
	{
		// during init time, ok to allow any pixel conversion operations
		tcFlags |= TEXTURE_CREATE_CANCONVERTFORMAT;
		tcFlagsSRGB |= TEXTURE_CREATE_CANCONVERTFORMAT;
	}

	// allocate a black single texel texture
	m_BlackTextureHandle = g_pShaderAPI->CreateTexture( 1, 1, 1, IMAGE_FORMAT_BGRX8888, 1, 1, tcFlags, "[BLACK_TEXID]", TEXTURE_GROUP_OTHER );
	g_pShaderAPI->ModifyTexture( m_BlackTextureHandle );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
	texel[0] = texel[1] = texel[2] = 0;
	pixelWriter.SetPixelMemory( IMAGE_FORMAT_BGRA8888, outTexel, 0 );
	pixelWriter.WritePixelNoAdvance( texel[0], texel[1], texel[2], texel[3] );
	g_pShaderAPI->TexImage2D( 0, 0, IMAGE_FORMAT_BGRX8888, 0, 1, 1, IMAGE_FORMAT_BGRX8888, false, outTexel );
	g_pShaderAPI->SetStandardTextureHandle( TEXTURE_BLACK, m_BlackTextureHandle );

	// allocate a fully white single texel texture
	m_WhiteTextureHandle = g_pShaderAPI->CreateTexture( 1, 1, 1, IMAGE_FORMAT_BGRX8888, 1, 1, tcFlags, "[WHITE_TEXID]", TEXTURE_GROUP_OTHER );
	g_pShaderAPI->ModifyTexture( m_WhiteTextureHandle );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
	texel[0] = texel[1] = texel[2] = 255;
	pixelWriter.SetPixelMemory( IMAGE_FORMAT_BGRA8888, outTexel, 0 );
	pixelWriter.WritePixelNoAdvance( texel[0], texel[1], texel[2], texel[3] );
	g_pShaderAPI->TexImage2D( 0, 0, IMAGE_FORMAT_BGRX8888, 0, 1, 1, IMAGE_FORMAT_BGRX8888, false, outTexel );
	g_pShaderAPI->SetStandardTextureHandle( TEXTURE_WHITE, m_WhiteTextureHandle );

	// allocate a grey single texel texture with an alpha of zero (for mat_fullbright 2)
	m_GreyTextureHandle = g_pShaderAPI->CreateTexture( 1, 1, 1, IMAGE_FORMAT_BGRX8888, 1, 1, tcFlagsSRGB, "[GREY_TEXID]", TEXTURE_GROUP_OTHER );
	g_pShaderAPI->ModifyTexture( m_GreyTextureHandle );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
	texel[0] = texel[1] = texel[2] = 128;
	texel[3] = 255; // needs to be 255 so that mat_fullbright 2 stuff isn't translucent.
	pixelWriter.SetPixelMemory( IMAGE_FORMAT_BGRA8888, outTexel, 0 );
	pixelWriter.WritePixelNoAdvance( texel[0], texel[1], texel[2], texel[3] );
	g_pShaderAPI->TexImage2D( 0, 0, IMAGE_FORMAT_BGRX8888, 0, 1, 1, IMAGE_FORMAT_BGRX8888, false, outTexel );
	g_pShaderAPI->SetStandardTextureHandle( TEXTURE_GREY, m_GreyTextureHandle );

	// allocate a grey single texel texture with an alpha of zero (for mat_fullbright 2)
	m_GreyAlphaZeroTextureHandle = g_pShaderAPI->CreateTexture( 1, 1, 1, IMAGE_FORMAT_RGBA8888, 1, 1, tcFlagsSRGB, "[GREYALPHAZERO_TEXID]", TEXTURE_GROUP_OTHER );
	g_pShaderAPI->ModifyTexture( m_GreyAlphaZeroTextureHandle );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
	texel[0] = texel[1] = texel[2] = 128;
	texel[3] = 0; // needs to be 0 so that self-illum doens't affect mat_fullbright 2
	pixelWriter.SetPixelMemory( IMAGE_FORMAT_BGRA8888, outTexel, 0 );
	pixelWriter.WritePixelNoAdvance( texel[0], texel[1], texel[2], texel[3] );
	g_pShaderAPI->TexImage2D( 0, 0, IMAGE_FORMAT_BGRX8888, 0, 1, 1, IMAGE_FORMAT_RGBA8888, false, outTexel );
	texel[3] = 255; // set back to default value so we don't affect the rest of this code.
	g_pShaderAPI->SetStandardTextureHandle( TEXTURE_GREY_ALPHA_ZERO, m_GreyAlphaZeroTextureHandle );

	// allocate a black single texel texture with an alpha of zero ( for paintmap not allocated or not enabled )
	m_BlackAlphaZeroTextureHandle = g_pShaderAPI->CreateTexture( 1, 1, 1, IMAGE_FORMAT_RGBA8888, 1, 1, tcFlags, "[BLACKALPHAZERO_TEXID]", TEXTURE_GROUP_OTHER );
	g_pShaderAPI->ModifyTexture( m_BlackAlphaZeroTextureHandle );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
	texel[0] = texel[1] = texel[2] = texel[3] = 0;
	pixelWriter.SetPixelMemory( IMAGE_FORMAT_BGRA8888, outTexel, 0 );
	pixelWriter.WritePixelNoAdvance( texel[0], texel[1], texel[2], texel[3] );
	g_pShaderAPI->TexImage2D( 0, 0, IMAGE_FORMAT_BGRX8888, 0, 1, 1, IMAGE_FORMAT_RGBA8888, false, outTexel );
	texel[3] = 255; // set back to default value so we don't affect the rest of this code.
	g_pShaderAPI->SetStandardTextureHandle( TEXTURE_GREY_ALPHA_ZERO, m_BlackAlphaZeroTextureHandle );

	// allocate a white, single texel texture for the fullbright lightmap
	// note: make sure and redo this when changing gamma, etc.
	// don't mipmap lightmaps
	// 360 expects RGBE encoded lightmaps, PC does not
	ImageFormat targetFormat  = IsX360() ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_BGRX8888;
	m_FullbrightLightmapTextureHandle = g_pShaderAPI->CreateTexture( 1, 1, 1, targetFormat, 1, 1, tcFlagsSRGB, "[FULLBRIGHT_LIGHTMAP_TEXID]", TEXTURE_GROUP_LIGHTMAP );
	g_pShaderAPI->ModifyTexture( m_FullbrightLightmapTextureHandle );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
	pixelWriter.SetPixelMemory( IMAGE_FORMAT_BGRA8888, outTexel, 0 );
	if ( HardwareConfig()->GetHDREnabled() )
	{
		// We use 4.12 fixed point lightmaps on all platforms currently.
		// Since we are using an eight-bit texture, here, the closest representation to one is:
		// ~( 1.0/16.0 * 255 )
		texel[0] = texel[1] = texel[2] = clamp( 256.0f / g_pShaderAPI->GetLightMapScaleFactor(), 0, 255 );
	}
	else
	{
		float tmpVect[3] = { nominal_lightmap_value, nominal_lightmap_value, nominal_lightmap_value };
		ColorSpace::LinearToLightmap( texel, tmpVect );
	}
	pixelWriter.WritePixelNoAdvance( texel[0], texel[1], texel[2], texel[3] );
	g_pShaderAPI->TexImage2D( 0, 0, targetFormat, 0, 1, 1, targetFormat, false, outTexel );

	// allocate a single texel flat normal texture
	m_FlatNormalTextureHandle = g_pShaderAPI->CreateTexture( 1, 1, 1, IMAGE_FORMAT_BGRX8888, 1, 1, tcFlags, "[FLAT_NORMAL_TEXTURE]", TEXTURE_GROUP_OTHER );
	g_pShaderAPI->ModifyTexture( m_FlatNormalTextureHandle );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
	texel[0] = 127; // R
	texel[1] = 127; // G
	texel[2] = 255; // B
	// using pixel writer to hide platform endian issues
	pixelWriter.SetPixelMemory( IMAGE_FORMAT_BGRA8888, outTexel, 0 );
	pixelWriter.WritePixelNoAdvance( texel[0], texel[1], texel[2], texel[3] );
	g_pShaderAPI->TexImage2D( 0, 0, IMAGE_FORMAT_BGRX8888, 0, 1, 1, IMAGE_FORMAT_BGRX8888, false, outTexel );
	g_pShaderAPI->SetStandardTextureHandle( TEXTURE_NORMALMAP_FLAT, m_FlatNormalTextureHandle );

	// allocate a single texel flat normal ssbump texture
	m_FlatSSBumpTextureHandle = g_pShaderAPI->CreateTexture( 1, 1, 1, IMAGE_FORMAT_BGRX8888, 1, 1, tcFlags, "[FLAT_SSBUMP_TEXTURE]", TEXTURE_GROUP_OTHER );
	g_pShaderAPI->ModifyTexture( m_FlatSSBumpTextureHandle );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
	texel[0] = 85; // 1/3*255
	texel[1] = 85; // 1/3*255
	texel[2] = 85; // 1/3*255
	// using pixel writer to hide platform endian issues
	pixelWriter.SetPixelMemory( IMAGE_FORMAT_BGRA8888, outTexel, 0 );
	pixelWriter.WritePixelNoAdvance( texel[0], texel[1], texel[2], texel[3] );
	g_pShaderAPI->TexImage2D( 0, 0, IMAGE_FORMAT_BGRX8888, 0, 1, 1, IMAGE_FORMAT_BGRX8888, false, outTexel );
	g_pShaderAPI->SetStandardTextureHandle( TEXTURE_SSBUMP_FLAT, m_FlatSSBumpTextureHandle );

	// allocate a single texel fullbright 1 lightmap for use with bump textures
	targetFormat = IsX360() ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_BGRX8888;
	m_FullbrightBumpedLightmapTextureHandle = g_pShaderAPI->CreateTexture( 1, 1, 1, targetFormat, 1, 1, tcFlags, "[FULLBRIGHT_BUMPED_LIGHTMAP_TEXID]", TEXTURE_GROUP_LIGHTMAP );
	g_pShaderAPI->ModifyTexture( m_FullbrightBumpedLightmapTextureHandle );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
	// using pixel writer to hide platform endian issues
	if ( HardwareConfig()->GetHDREnabled() )
	{
		// We use 4.12 fixed point lightmaps on all platforms currently.
		// Since we are using an eight-bit texture, here, the closest representation to one is:
		// ~( 1.0/16.0 * 255 )
		texel[0] = texel[1] = texel[2] = clamp( 256.0f / g_pShaderAPI->GetLightMapScaleFactor(), 0, 255 );
	}
	else
	{
		float linearColor[3] = { nominal_lightmap_value, nominal_lightmap_value, nominal_lightmap_value };
		unsigned char dummy[3];
		ColorSpace::LinearToBumpedLightmap( linearColor, linearColor, linearColor, linearColor,
			dummy, texel, dummy, dummy );	
	}
	pixelWriter.SetPixelMemory( IMAGE_FORMAT_BGRA8888, outTexel, 0 );
	pixelWriter.WritePixelNoAdvance( texel[0], texel[1], texel[2], texel[3] );
	g_pShaderAPI->TexImage2D( 0, 0, targetFormat, 0, 1, 1, targetFormat, false, outTexel );
	g_pShaderAPI->SetStandardTextureHandle( TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT, m_FullbrightBumpedLightmapTextureHandle );

#if defined( GAMMA_TEX1D_LOOKUP )	
	{
		int iGammaLookupFlags = tcFlags;
		ImageFormat gammalookupfmt;
		gammalookupfmt = IMAGE_FORMAT_I8;

		// generate the linear->gamma conversion table texture.
		{
			const int LINEAR_TO_GAMMA_TABLE_WIDTH = 512;
			m_LinearToGammaTableTextureHandle = g_pShaderAPI->CreateTexture( LINEAR_TO_GAMMA_TABLE_WIDTH, 1, 1, gammalookupfmt, 1, 1, iGammaLookupFlags, "[LINEAR_TO_GAMMA_LOOKUP_SRGBON_TEXID]", TEXTURE_GROUP_PIXEL_SHADERS );
			g_pShaderAPI->ModifyTexture( m_LinearToGammaTableTextureHandle );
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
			g_pShaderAPI->TexWrap( SHADER_TEXCOORD_S, SHADER_TEXWRAPMODE_CLAMP );
			g_pShaderAPI->TexWrap( SHADER_TEXCOORD_T, SHADER_TEXWRAPMODE_CLAMP );
			g_pShaderAPI->TexWrap( SHADER_TEXCOORD_U, SHADER_TEXWRAPMODE_CLAMP );

			float pixelData[LINEAR_TO_GAMMA_TABLE_WIDTH]; //sometimes used as float, sometimes as uint8, sizeof(float) > sizeof(uint8)
			for( int i = 0; i != LINEAR_TO_GAMMA_TABLE_WIDTH; ++i )
			{
				float fLookupResult = ((float)i) / ((float)(LINEAR_TO_GAMMA_TABLE_WIDTH - 1));
				fLookupResult = g_pShaderAPI->LinearToGamma_HardwareSpecific( fLookupResult );
				
				//do an extra srgb conversion because we'll be converting back on texture read
				fLookupResult = g_pShaderAPI->LinearToGamma_HardwareSpecific( fLookupResult ); //that's right, linear->gamma->gamma2x so that that gamma->linear srgb read still ends up in gamma
				
				int iColor = RoundFloatToInt( fLookupResult * 255.0f );
				if( iColor > 255 )
					iColor = 255;

				((uint8 *)pixelData)[i] = (uint8)iColor;
			}

			g_pShaderAPI->TexImage2D( 0, 0, gammalookupfmt, 0, LINEAR_TO_GAMMA_TABLE_WIDTH, 1, gammalookupfmt, false, (void *)pixelData );
		}

		// generate the identity conversion table texture.
		{
			const int LINEAR_TO_GAMMA_IDENTITY_TABLE_WIDTH = 256;
			m_LinearToGammaIdentityTableTextureHandle = g_pShaderAPI->CreateTexture( LINEAR_TO_GAMMA_IDENTITY_TABLE_WIDTH, 1, 1, gammalookupfmt, 1, 1, tcFlags, "[LINEAR_TO_GAMMA_LOOKUP_SRGBOFF_TEXID]", TEXTURE_GROUP_PIXEL_SHADERS );
			g_pShaderAPI->ModifyTexture( m_LinearToGammaIdentityTableTextureHandle );
			g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
			g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
			g_pShaderAPI->TexWrap( SHADER_TEXCOORD_S, SHADER_TEXWRAPMODE_CLAMP );
			g_pShaderAPI->TexWrap( SHADER_TEXCOORD_T, SHADER_TEXWRAPMODE_CLAMP );
			g_pShaderAPI->TexWrap( SHADER_TEXCOORD_U, SHADER_TEXWRAPMODE_CLAMP );

			float pixelData[LINEAR_TO_GAMMA_IDENTITY_TABLE_WIDTH]; //sometimes used as float, sometimes as uint8, sizeof(float) > sizeof(uint8)
			for( int i = 0; i != LINEAR_TO_GAMMA_IDENTITY_TABLE_WIDTH; ++i )
			{
				float fLookupResult = ((float)i) / ((float)(LINEAR_TO_GAMMA_IDENTITY_TABLE_WIDTH - 1));
				
				//do an extra srgb conversion because we'll be converting back on texture read
				fLookupResult = g_pShaderAPI->LinearToGamma_HardwareSpecific( fLookupResult );

				int iColor = RoundFloatToInt( fLookupResult * 255.0f );
				if ( iColor > 255 )
					iColor = 255;

				((uint8 *)pixelData)[i] = (uint8)iColor;
			}

			g_pShaderAPI->TexImage2D( 0, 0, gammalookupfmt, 0, LINEAR_TO_GAMMA_IDENTITY_TABLE_WIDTH, 1, gammalookupfmt, false, (void *)pixelData );
		}
	}

	//only the shaderapi can handle switching between textures correctly, so pass off the textures to it.
	g_pShaderAPI->SetLinearToGammaConversionTextures( m_LinearToGammaTableTextureHandle, m_LinearToGammaIdentityTableTextureHandle );
#endif

	// create the maximum depth texture
	m_MaxDepthTextureHandle = g_pShaderAPI->CreateTexture( 1, 1, 1, IMAGE_FORMAT_RGBA8888, 1, 1, tcFlags, "[MAXDEPTH_TEXID]", TEXTURE_GROUP_OTHER );
	g_pShaderAPI->ModifyTexture( m_MaxDepthTextureHandle );
	g_pShaderAPI->TexMinFilter( SHADER_TEXFILTERMODE_LINEAR );
	g_pShaderAPI->TexMagFilter( SHADER_TEXFILTERMODE_LINEAR );
	// 360 gets depth out of the red channel (which doubles as depth in D24S8) and may be 0/1 depending on REVERSE_DEPTH_ON_X360
	// PC gets depth out of the alpha channel
	texel[0] = texel[1] = texel[2] = ReverseDepthOnX360() ? 0 : 255;
	texel[3] = 255;
	pixelWriter.SetPixelMemory( IMAGE_FORMAT_RGBA8888, outTexel, 0 );
	pixelWriter.WritePixelNoAdvance( texel[0], texel[1], texel[2], texel[3] );
	g_pShaderAPI->TexImage2D( 0, 0, IMAGE_FORMAT_RGBA8888, 0, 1, 1, IMAGE_FORMAT_RGBA8888, false, outTexel );
}

void CMaterialSystem::ReleaseStandardTextures()
{
	if ( m_StandardTexturesAllocated )
	{
		g_pShaderAPI->DeleteTexture( m_WhiteTextureHandle );
		g_pShaderAPI->DeleteTexture( m_BlackTextureHandle );
		g_pShaderAPI->DeleteTexture( m_BlackAlphaZeroTextureHandle );
		g_pShaderAPI->DeleteTexture( m_GreyTextureHandle );
		g_pShaderAPI->DeleteTexture( m_GreyAlphaZeroTextureHandle );

#if defined( FEATURE_SUBD_SUPPORT )
		g_pSubDMgr->FreeTextures();
#endif

		g_pShaderAPI->DeleteTexture( m_FullbrightLightmapTextureHandle );
		g_pShaderAPI->DeleteTexture( m_FlatNormalTextureHandle );
		g_pShaderAPI->DeleteTexture( m_FlatSSBumpTextureHandle );
		g_pShaderAPI->DeleteTexture( m_FullbrightBumpedLightmapTextureHandle );

#if defined( GAMMA_TEX1D_LOOKUP )
		g_pShaderAPI->DeleteTexture( m_LinearToGammaTableTextureHandle );
		g_pShaderAPI->DeleteTexture( m_LinearToGammaIdentityTableTextureHandle );
		g_pShaderAPI->SetLinearToGammaConversionTextures( INVALID_SHADERAPI_TEXTURE_HANDLE, INVALID_SHADERAPI_TEXTURE_HANDLE );
#endif

		g_pShaderAPI->DeleteTexture( m_MaxDepthTextureHandle );

		m_StandardTexturesAllocated = false;
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMaterialSystem::BeginFrame( float frameTime )
{
	// Safety measure (calls should only come from the main thread, also check correct pairing)
	if ( !ThreadInMainThread() || IsInFrame() )
		return;

	// check debug vars. we will use these to setup g_nDebugVarsSignature so that materials will
	// rebuild their draw lists when debug modes changed.
	g_nDebugVarsSignature = ( 
		(mat_specular.GetInt() != 0 ) + ( mat_normalmaps.GetInt() << 1 ) +
		( mat_fullbright.GetInt() << 2 ) + ( mat_fastnobump.GetInt() << 4 ) + ( mat_fastspecular.GetInt() << 5 ) ) << 24;
	
	Assert( m_bGeneratedConfig );

	VPROF_BUDGET( "CMaterialSystem::BeginFrame", VPROF_BUDGETGROUP_SWAP_BUFFERS );

	IMatRenderContextInternal *pRenderContext = GetRenderContextInternal();
	if ( mat_forcehardwaresync.GetBool() && (IsPC() || m_ThreadMode != MATERIAL_QUEUED_THREADED) )
	{
		pRenderContext->ForceHardwareSync();
	}

	pRenderContext->MarkRenderDataUnused( true );
	pRenderContext->BeginFrame();
	pRenderContext->SetFrameTime( frameTime );
	pRenderContext->SetToneMappingScaleLinear( Vector( 1,1,1) );

	//g_pMDLCache->UpdateCombiner();

	Assert( !m_bInFrame );
	m_bInFrame = true;
}

bool CMaterialSystem::IsInFrame( ) const
{
	return m_bInFrame;
}


void CMaterialSystem::ThreadExecuteQueuedContext( CMatQueuedRenderContext *pContext )
{
	#ifdef _PS3
		#if GCM_ALLOW_NULL_FLIPS
		extern void Ps3NullFlipsStartSceneTime();
		Ps3NullFlipsStartSceneTime();
		#endif
	// This function takes up a lot of global thread pool time in GPU-bound levels, and it's convenient
	// to have it as a bar in snTuner profiler. Making it PS3-only to avoid polluting X360 bars
	VPROF_BUDGET( "ThreadExecuteQueuedContext", "All Threaded Rendering" );
	#endif

	#if GCM_ALLOW_TIMESTAMPS || X360_ALLOW_TIMESTAMPS
	OnFrameTimestampAvailableMST( 0.0f ); // signals frame start
	#endif

	m_nRenderThreadID = ThreadGetCurrentId(); 
	IMatRenderContextInternal* pSavedRenderContext = m_pRenderContext;
	m_pRenderContext =  &m_HardwareRenderContext;
	pContext->EndQueue( true );

	for (int i = 0; i < m_EndFrameCleanupFunc.Count(); ++i)
	{
		m_EndFrameCleanupFunc[ i ]();
	}

	m_pRenderContext =  pSavedRenderContext;
	m_nRenderThreadID = 0xFFFFFFFF; 
}

IThreadPool *CMaterialSystem::CreateMatQueueThreadPool()
{
	if( IsX360() )
	{
		return g_pThreadPool;
	}
	else if( !m_pMatQueueThreadPool )
	{
		ThreadPoolStartParams_t startParams;

		startParams.nThreads = 1;
		startParams.nStackSize = 256*1024;
		startParams.fDistribute = TRS_TRUE;

		// The rendering thread has the GL context and the main thread is coming in and
		//  "helping" finish jobs - that breaks OpenGL, which requires TLS. This flag states 
		//  that only the threadpool threads should execute these jobs.
		startParams.bExecOnThreadPoolThreadsOnly = true;

		m_pMatQueueThreadPool = CreateNewThreadPool();
		m_pMatQueueThreadPool->Start( startParams, "MatQueue" );
	}

	return m_pMatQueueThreadPool;
}

void CMaterialSystem::DestroyMatQueueThreadPool()
{	
	if( m_pMatQueueThreadPool )
	{
		m_pMatQueueThreadPool->Stop();
		delete m_pMatQueueThreadPool;
		m_pMatQueueThreadPool = NULL;
	}
}

#if GCM_ALLOW_TIMESTAMPS || X360_ALLOW_TIMESTAMPS
static double s_flMainThreadBeginTimestampSec = 0.0f;
#endif

//
// On OSX, Forced Single Threaded needs to last for more frames than windows that the window resizing/switch to 
// and from fullscreen don't force GL calls at the same time as the render thread.
//

#if defined ( OSX )

static void CheckOsxForcedNextThreadMode( MaterialThreadMode_t* pNextThreadMode, bool* pbForcedSingleThreaded )
{
	const int nOsxFramesAtSingleThreaded = 2;
	static int nOsxFrames = nOsxFramesAtSingleThreaded;
	if ( *pbForcedSingleThreaded )
	{
		*pNextThreadMode = MATERIAL_SINGLE_THREADED;

		if ( nOsxFrames == 0 )
		{
			*pbForcedSingleThreaded = false;
			nOsxFrames = nOsxFramesAtSingleThreaded;
		}
		else
		{
			nOsxFrames--;
		}
	}
	else
	{
		nOsxFrames = nOsxFramesAtSingleThreaded;
	}
}

#endif 

void CMaterialSystem::EndFrame( void )
{

	SNPROF("CMaterialSystem::EndFrame");

	// Safety measure (calls should only come from the main thread, also check correct pairing)
	if ( !ThreadInMainThread() || !IsInFrame() )
		return;

	Assert( m_bGeneratedConfig );
	VPROF_BUDGET( "CMaterialSystem::EndFrame", VPROF_BUDGETGROUP_SWAP_BUFFERS );

	GetRenderContextInternal()->EndFrame();
		
	//-------------------------------------------------------------

	UpdateConfig( false );

	int iConVarThreadMode = mat_queue_mode.GetInt();
	MaterialThreadMode_t nextThreadMode = ( iConVarThreadMode >= 0 ) ? (MaterialThreadMode_t)iConVarThreadMode : m_IdealThreadMode;
	// note: This is a hack because there is no explicit query for the device being deactivated due to device lost.
	// however, that is all the current implementation of CanDownloadTextures actually does.
	bool bDeviceReady = g_pShaderAPI->CanDownloadTextures();
	if ( !bDeviceReady || !m_bAllowQueuedRendering )
	{
		nextThreadMode = MATERIAL_SINGLE_THREADED;
	}

#if !defined ( OSX )
	if ( m_bForcedSingleThreaded )
	{
		nextThreadMode = MATERIAL_SINGLE_THREADED;
		m_bForcedSingleThreaded = false;
	}
#else
	CheckOsxForcedNextThreadMode(&nextThreadMode, &m_bForcedSingleThreaded );
#endif

#if GCM_ALLOW_TIMESTAMPS || X360_ALLOW_TIMESTAMPS
	{
		// This is called on the main thread and here the main thread is finished
		double flMainThreadEndTimestampSec = Plat_FloatTime();
		double flMainThreadTimeInSeconds = flMainThreadEndTimestampSec - s_flMainThreadBeginTimestampSec;
		OnFrameTimestampAvailableMain( flMainThreadTimeInSeconds*1000.0f );
	}
#endif

	switch ( m_ThreadMode )
	{
	case MATERIAL_SINGLE_THREADED:
#if GCM_ALLOW_TIMESTAMPS || X360_ALLOW_TIMESTAMPS
		{
			double flCurrentTime = Plat_FloatTime();
			OnFrameTimestampAvailableTotal( (flCurrentTime-s_flTotalFrameBeginTimestamp)*1000.0f );
			s_flTotalFrameBeginTimestamp = flCurrentTime;
			OnFrameTimestampAvailableMST( 0.0f ); // signals frame start
		}
#endif
		ServiceEndFramePriorToNextContext();
		g_pfnSwapBufferMarker();
		break;

#ifndef _PS3

	case MATERIAL_QUEUED_THREADED:
		{
			VPROF_BUDGET( "Mat_ThreadedEndframe", "Mat_ThreadedEndframe" );
			{
				TM_ZONE_PLOT( TELEMETRY_LEVEL1, "Endframe_Wait", TELEMETRY_ZONE_PLOT_SLOT_3 );
				PERF_STATS_BLOCK( "Endframe_Wait", PERF_STATS_SLOT_END_FRAME );

			while ( m_pActiveAsyncJob && !m_pActiveAsyncJob->IsFinished() )
			{
				// [jason]
				// Potential fix for a deadlock on threaded rendering: we were occasionally seeing deadlocks on the main thread, 
				// waiting for work to be completed on the rendering job threads.  Every time we break into this with the debugger,
				// there are no job threads that are stalled - everything is waiting for new work to be sent down.  I suspect that 
				// in the gap between the main thread determining there is still outstanding work and the main thread locking, we 
				// occasionally get a context switch to the job thread that completes its work.  When we switch back to the main
				// thread, we begin to wait on a thread that will never signal (because it's already done working) so we deadlock.  
				// The fix is to break this infinite wait into a busy loop of 1/2 second waits - either the worker thread will signal 
				// and we'll resume processing, or at worst we'll wait 1/2 second and then double-check there is actually work running 
				// on the job queue and find there is nothing left to wait for.

					// [Michael Dorgan]
					// Alas, the thread system ignores the timeout value passed in right now and always does a Sleep(0)
					// Still, a Sleep(0) yield does seem to fix this, so removed the 500 param.

					m_pActiveAsyncJob->WaitForFinish();

					if ( !m_pActiveAsyncJob->IsFinished() )
						DevMsg( "CMaterialSystem::EndFrame - waiting on additional threaded work for MatQueuedThreaded.\n" );

					if ( !IsPC() && mat_forcehardwaresync.GetBool() )
					{
						g_pShaderAPI->ForceHardwareSync();
					}
				}
			}
			SafeRelease( m_pActiveAsyncJob );


#if GCM_ALLOW_TIMESTAMPS || X360_ALLOW_TIMESTAMPS
			{
				double flCurrentTime = Plat_FloatTime();
				OnFrameTimestampAvailableTotal( (flCurrentTime-s_flTotalFrameBeginTimestamp)*1000.0f );
				s_flTotalFrameBeginTimestamp = flCurrentTime;
			}
#endif
			ServiceEndFramePriorToNextContext();
			g_pfnSwapBufferMarker();

			CMatQueuedRenderContext *pPrevContext = &m_QueuedRenderContexts[m_iCurQueuedContext];

#ifdef MAT_QUEUED_OWN_THREADPOOL
			// Needs to be done after calling ServiceEndFramePriorToNextContext to ensure the render thread
			// will have ownership, as ServiceEndFramePriorToNextContext could locking the material system
			// => RenderThread would then lose ownership
			if ( !m_bThreadHasOwnership )
			{
				ThreadAcquire();
			}
#endif

#ifndef MAT_QUEUED_OWN_THREADPOOL
			m_QueuedRenderContexts[m_iCurQueuedContext].GetCallQueueInternal()->QueueCall( g_pShaderAPI, &IShaderAPI::ReleaseThreadOwnership );
#endif
			m_iCurQueuedContext = ( ( m_iCurQueuedContext + 1 ) % ARRAYSIZE( m_QueuedRenderContexts) );
			m_QueuedRenderContexts[m_iCurQueuedContext].BeginQueue( pPrevContext );
#ifndef MAT_QUEUED_OWN_THREADPOOL
			m_QueuedRenderContexts[m_iCurQueuedContext].GetCallQueueInternal()->QueueCall( g_pShaderAPI, &IShaderAPI::AcquireThreadOwnership );
#endif
			m_pRenderContext =  &m_QueuedRenderContexts[m_iCurQueuedContext];

			m_pActiveAsyncJob = new CFunctorJob( CreateFunctor( this, &CMaterialSystem::ThreadExecuteQueuedContext, pPrevContext ) );
			if ( !IsPC() )
			{
				if ( m_nServiceThread >= 0 )
				{
					m_pActiveAsyncJob->SetServiceThread( m_nServiceThread );
				}
			}
			if ( mat_queue_priority.GetBool() )
			{
				m_pActiveAsyncJob->SetFlags( m_pActiveAsyncJob->GetFlags() | JF_QUEUE );
			}
#ifdef MAT_QUEUED_OWN_THREADPOOL
			IThreadPool *pThreadPool = CreateMatQueueThreadPool();
#else
			IThreadPool *pThreadPool = g_pThreadPool;
#endif
			pThreadPool->AddJob( m_pActiveAsyncJob );
			break;
		}


#else


	case MATERIAL_QUEUED_THREADED:
		{
			// Wait for previous submitted QMS run
			if (m_bQMSJobSubmitted)
			{
				g_pGcmSharedData->WaitForQMS();
				m_bQMSJobSubmitted = 0;

				if ( !IsPC() && mat_forcehardwaresync.GetBool() )
				{
					g_pShaderAPI->ForceHardwareSync();
				}
			}

#if GCM_ALLOW_TIMESTAMPS || X360_ALLOW_TIMESTAMPS
			{
				double flCurrentTime = Plat_FloatTime();
				OnFrameTimestampAvailableTotal( (flCurrentTime-s_flTotalFrameBeginTimestamp)*1000.0f );
				s_flTotalFrameBeginTimestamp = flCurrentTime;
			}
#endif
			ServiceEndFramePriorToNextContext();
			g_pfnSwapBufferMarker();

			// Switch Render Contexts

			CMatQueuedRenderContext *pPrevContext = &m_QueuedRenderContexts[m_iCurQueuedContext];

			m_QueuedRenderContexts[m_iCurQueuedContext].GetCallQueueInternal()->QueueCall( g_pShaderAPI, &IShaderAPI::ReleaseThreadOwnership );
			m_iCurQueuedContext = ( ( m_iCurQueuedContext + 1 ) % ARRAYSIZE( m_QueuedRenderContexts) );
			m_QueuedRenderContexts[m_iCurQueuedContext].BeginQueue( pPrevContext );
			m_QueuedRenderContexts[m_iCurQueuedContext].GetCallQueueInternal()->QueueCall( g_pShaderAPI, &IShaderAPI::AcquireThreadOwnership );
			m_pRenderContext =  &m_QueuedRenderContexts[m_iCurQueuedContext];
		
			// Run QMS on prevContext

			g_pGcmSharedData->RunQMS(&RunQMS, (void*)this, (void*)pPrevContext);
			m_bQMSJobSubmitted = 1;

			break;
		}

#endif

	case MATERIAL_QUEUED_SINGLE_THREADED:
		{
			VPROF_BUDGET( "Mat_ThreadedEndframe", "Mat_QueuedEndframe" );

			g_pShaderAPI->SetDisallowAccess( false );
			m_pRenderContext = &m_HardwareRenderContext;
#if GCM_ALLOW_TIMESTAMPS || X360_ALLOW_TIMESTAMPS
			OnFrameTimestampAvailableMST( 0.0f ); // signals frame start
#endif
			m_QueuedRenderContexts[m_iCurQueuedContext].CallQueued();

			// Set up for next frame, we don't cycle through m_iCurQueuedContext though
			m_QueuedRenderContexts[m_iCurQueuedContext].CycleDynamicBuffers();
			m_pRenderContext =  &m_QueuedRenderContexts[m_iCurQueuedContext];
			g_pShaderAPI->SetDisallowAccess( true );
#if GCM_ALLOW_TIMESTAMPS || X360_ALLOW_TIMESTAMPS
			double flCurrentTime = Plat_FloatTime();
			OnFrameTimestampAvailableTotal( (flCurrentTime-s_flTotalFrameBeginTimestamp)*1000.0f );
			s_flTotalFrameBeginTimestamp = flCurrentTime;
#endif
			ServiceEndFramePriorToNextContext();
			g_pfnSwapBufferMarker();
			break;
		}
	}

	// Tick perfstats. We measure the main thread time from the point just after we finish waiting
	// for the render thread.
	g_PerfStats.Tick();

#if GCM_ALLOW_TIMESTAMPS  || X360_ALLOW_TIMESTAMPS
	s_flMainThreadBeginTimestampSec = Plat_FloatTime();
#endif

	bool bRelease = false;
	if ( !bDeviceReady )
	{
		if ( nextThreadMode != MATERIAL_SINGLE_THREADED )
		{
			Assert( nextThreadMode == MATERIAL_SINGLE_THREADED );
			bRelease = true;
			nextThreadMode = MATERIAL_SINGLE_THREADED;
			if( mat_debugalttab.GetBool() )
			{
				Warning("Handling alt-tab in queued mode!\n");
			}
		}
	}

	if ( m_threadEvents.Count()	)
	{
		nextThreadMode = MATERIAL_SINGLE_THREADED;
	}

	if ( m_ThreadMode != nextThreadMode )
	{
		// Shut down the current mode & set new mode
		switch ( m_ThreadMode )
		{
		case MATERIAL_SINGLE_THREADED:
			break;

		case MATERIAL_QUEUED_THREADED:
			{
#ifndef _PS3
				if ( m_pActiveAsyncJob )
				{
					m_pActiveAsyncJob->WaitForFinish();
					SafeRelease( m_pActiveAsyncJob );
				}
#else
				if (m_bQMSJobSubmitted)
				{
					g_pGcmSharedData->WaitForQMS();
					m_bQMSJobSubmitted = 0;
				}
#endif
				// We have a queued context set here, need hardware to flush the queue if the job isn't active
				m_pRenderContext = &m_HardwareRenderContext;

				m_QueuedRenderContexts[m_iCurQueuedContext].EndQueue( true );
#ifdef MAT_QUEUED_OWN_THREADPOOL
				ThreadRelease();
#else
				g_pShaderAPI->AcquireThreadOwnership();
#endif
			}
			break;

		case MATERIAL_QUEUED_SINGLE_THREADED:
			{
				g_pShaderAPI->SetDisallowAccess( false );
				// We have a queued context set here, need hardware to flush the queue if the job isn't active
				m_pRenderContext = &m_HardwareRenderContext;
				m_QueuedRenderContexts[m_iCurQueuedContext].EndQueue( true );
				break;
			}
		}

		m_ThreadMode = nextThreadMode;
#ifndef DX_TO_GL_ABSTRACTION
		Assert( g_MatSysMutex.GetOwnerId() == 0 );
#endif

		g_pShaderAPI->EnableShaderShaderMutex( m_ThreadMode != MATERIAL_SINGLE_THREADED ); // use mutex even for queued to allow "disalow access" to function properly
		g_pShaderAPI->EnableBuffer2FramesAhead( true );

		switch ( m_ThreadMode )
		{
		case MATERIAL_SINGLE_THREADED:
			m_pRenderContext =  &m_HardwareRenderContext;
			for ( int i = 0; i < ARRAYSIZE( m_QueuedRenderContexts ); i++ )
			{
				Assert( m_QueuedRenderContexts[i].IsInitialized() );
				m_QueuedRenderContexts[i].Shutdown();
			}
			g_pScaleformUI->SetSingleThreadedMode(true);
			break;

		case MATERIAL_QUEUED_SINGLE_THREADED:
		case MATERIAL_QUEUED_THREADED:
			for ( int i = 0; i < ARRAYSIZE( m_QueuedRenderContexts ); i++ )
			{
				if ( !m_QueuedRenderContexts[i].IsInitialized() )
				{
					m_QueuedRenderContexts[i].Init( this, &m_HardwareRenderContext );
				}
			}
			m_iCurQueuedContext = 0;
			m_QueuedRenderContexts[m_iCurQueuedContext].BeginQueue( &m_HardwareRenderContext );
			m_pRenderContext = &m_QueuedRenderContexts[m_iCurQueuedContext];
			if ( m_ThreadMode == MATERIAL_QUEUED_SINGLE_THREADED )
			{
				g_pShaderAPI->SetDisallowAccess( true );
				g_pScaleformUI->SetSingleThreadedMode(true);
			}
			else
			{
#ifdef MAT_QUEUED_OWN_THREADPOOL
				ThreadAcquire();
#else
				g_pShaderAPI->ReleaseThreadOwnership();
				m_QueuedRenderContexts[m_iCurQueuedContext].GetCallQueueInternal()->QueueCall( g_pShaderAPI, &IShaderAPI::AcquireThreadOwnership );
#endif
				g_pScaleformUI->SetSingleThreadedMode(false);
			}
			break;
		}
	}

	if ( m_ThreadMode == MATERIAL_SINGLE_THREADED )
	{
		for ( int i = 0; i < m_threadEvents.Count(); i++ )
		{
			g_pShaderDevice->HandleThreadEvent(m_threadEvents[i]);
		}
		m_threadEvents.RemoveAll();
	}

	if ( m_ThreadMode == MATERIAL_SINGLE_THREADED || m_ThreadMode == MATERIAL_QUEUED_SINGLE_THREADED )
	{
		for (int i = 0; i < m_EndFrameCleanupFunc.Count(); ++i)
		{
			m_EndFrameCleanupFunc[ i ]();
		}
	}

	Assert( m_bInFrame );
	m_bInFrame = false;
}

void CMaterialSystem::SetInStubMode( bool bInStubMode )
{
	m_bInStubMode = bInStubMode;
}

bool CMaterialSystem::IsInStubMode()
{
	return m_bInStubMode;
}

void CMaterialSystem::Flush( bool flushHardware )
{
	GetRenderContextInternal()->Flush( flushHardware );
}

uint32 CMaterialSystem::GetCurrentFrameCount()
{
	return g_FrameNum;
}


//-----------------------------------------------------------------------------
// Flushes managed textures from the texture cacher
//-----------------------------------------------------------------------------
void CMaterialSystem::EvictManagedResources()
{
	g_pShaderAPI->EvictManagedResources();
}

int __cdecl MaterialNameCompareFunc( const void *elem1, const void *elem2 )
{
	IMaterialInternal *pMaterialA = g_MaterialSystem.GetMaterialInternal( *(MaterialHandle_t *)elem1 );
	IMaterialInternal *pMaterialB = g_MaterialSystem.GetMaterialInternal( *(MaterialHandle_t *)elem2 );

	// case insensitive to group similar named materials
	return stricmp( pMaterialA->GetName(), pMaterialB->GetName() );
}

void CMaterialSystem::DebugPrintUsedMaterials( const char *pSearchSubString, bool bVerbose )
{
	MaterialHandle_t	h;
	int					i;
	int					nNumCached;
	int					nRefCount;
	int					nSortedMaterials;
	int					nNumErrors;
	
	// build a mapping to sort the material names
	MaterialHandle_t *pSorted = (MaterialHandle_t*)stackalloc( GetNumMaterials() * sizeof(MaterialHandle_t) );
	nSortedMaterials = 0;
	for (h = FirstMaterial(); h != InvalidMaterial(); h = NextMaterial(h) )
	{
		pSorted[nSortedMaterials++] = h;
	}
	qsort( pSorted, nSortedMaterials, sizeof(MaterialHandle_t), MaterialNameCompareFunc );

	nNumCached = 0;
	nNumErrors = 0;
	for (i = 0; i < nSortedMaterials; i++)
	{
		// iterate using sort mapping
		IMaterialInternal *pMaterial = GetMaterialInternal(pSorted[i]);

		nRefCount = pMaterial->GetReferenceCount();
		
		if ( nRefCount < 0 )
		{
			nNumErrors++;
			continue;
		}
		
		if (!nRefCount)
		{
			if (pMaterial->IsPrecached() || pMaterial->IsPrecachedVars())
			{
				nNumErrors++;
			}
			continue;
		}

		// nonzero reference count
		// tally the valid ones
		nNumCached++;

		if( pSearchSubString )
		{
			if( !Q_stristr( pMaterial->GetName(), pSearchSubString ) &&
				(!pMaterial->GetShader() || !Q_stristr( pMaterial->GetShader()->GetName(), pSearchSubString )) )
			{
				continue;
			}
		}

		DevMsg( "%s (shader: %s) refCount: %d.\n", pMaterial->GetName(), 
			pMaterial->GetShader() ? pMaterial->GetShader()->GetName() : "unknown\n", nRefCount );

		if( !bVerbose )
			continue;

		if( !pMaterial->IsPrecached() )
			continue;

		if( !pMaterial->GetShader() )
			continue;

		for( int j = 0; j < pMaterial->GetShader()->GetParamCount(); j++ )
		{
			IMaterialVar *var;
			var = pMaterial->GetShaderParams()[j];
			
			if( !var )
				continue;

			switch( var->GetType() )
			{
			case MATERIAL_VAR_TYPE_TEXTURE:
				{
					ITextureInternal *texture = static_cast<ITextureInternal *>( var->GetTextureValue() );
					if( !texture )
					{
						DevWarning( "Programming error: CMaterialSystem::DebugPrintUsedMaterialsCallback: NULL texture\n" );
						continue;
					}
					
					if( IsTextureInternalEnvCubemap( texture ) )
					{
						DevMsg( "    \"%s\" \"env_cubemap\"\n", var->GetName() );
					}
					else
					{
						DevMsg( "    \"%s\" \"%s\"\n", 
							var->GetName(),
							texture->GetName() );
						DevMsg( "        %dx%d refCount: %d numframes: %d\n", texture->GetActualWidth(), texture->GetActualHeight(), 
							texture->GetReferenceCount(), texture->GetNumAnimationFrames() );
					}
				}
				break;
			case MATERIAL_VAR_TYPE_UNDEFINED:
				break;
			default:
				DevMsg( "    \"%s\" \"%s\"\n", var->GetName(), var->GetStringValue() );
				break;
			}
		}
	}

	// list the critical errors after, otherwise the console log scrolls them away
	if (nNumErrors)
	{
		for (i = 0; i < nSortedMaterials; i++)
		{
			// iterate using sort mapping
			IMaterialInternal *pMaterial = GetMaterialInternal(pSorted[i]);

			nRefCount = pMaterial->GetReferenceCount();

			if ( nRefCount < 0 )
			{
				// reference counts should not be negative
				DevWarning( "DebugPrintUsedMaterials: refCount (%d) < 0 for material: \"%s\"\n",
					nRefCount, pMaterial->GetName() );
			}
			else if (!nRefCount)
			{
				// ensure that it stayed uncached after the post loading uncache
				// this is effectively a coding bug thats needs to be fixed
				// a material is being precached without incrementing its reference
				if (pMaterial->IsPrecached() || pMaterial->IsPrecachedVars())
				{
					DevWarning( "DebugPrintUsedMaterials: material: \"%s\" didn't unache\n",
						pMaterial->GetName() );
				}
			}
		}
		DevWarning( "%d Errors\n", nNumErrors );
	}

	if (!pSearchSubString)
	{
		DevMsg( "%d Cached, %d Total Materials\n", nNumCached, GetNumMaterials() );
	}
}

void CMaterialSystem::DebugPrintUsedTextures( void )
{
	TextureManager()->DebugPrintUsedTextures();
}

#if defined( _X360 ) || defined( _PS3 )
void CMaterialSystem::ListUsedMaterials( void )
{	
	int numMaterials = GetNumMaterials();
	xMaterialList_t* pMaterialList = (xMaterialList_t *)stackalloc( numMaterials * sizeof( xMaterialList_t ) );

	numMaterials = 0;
	for ( MaterialHandle_t hMaterial = FirstMaterial(); hMaterial != InvalidMaterial(); hMaterial = NextMaterial( hMaterial ) )
	{
		IMaterialInternal *pMaterial = GetMaterialInternal( hMaterial );
		pMaterialList[numMaterials].pName = pMaterial->GetName();
		pMaterialList[numMaterials].pShaderName = pMaterial->GetShader() ? pMaterial->GetShader()->GetName() : "???";
		pMaterialList[numMaterials].refCount = pMaterial->GetReferenceCount();
		numMaterials++;
	}

	XBX_rMaterialList( numMaterials, pMaterialList );
}
#endif

void CMaterialSystem::ToggleSuppressMaterial( char const* pMaterialName )
{
	/*
	// This version suppresses all but the material
	IMaterial *pMaterial = GetFirstMaterial();
	while (pMaterial)
	{
		if (stricmp(pMaterial->GetName(), pMaterialName))
		{
			IMaterialInternal* pMatInt = static_cast<IMaterialInternal*>(pMaterial);
			pMatInt->ToggleSuppression();
		}
		pMaterial = GetNextMaterial();
	}
	*/

	// Note: if we use this function a lot, we'll want to do something else, like have them
	// pass in a texture group or reuse whatever texture group the material already had.
	// As it is, this is rarely used, so if it's not in TEXTURE_GROUP_OTHER, it'll go in 
	// TEXTURE_GROUP_SHARED.
	IMaterial* pMaterial = FindMaterial( pMaterialName, TEXTURE_GROUP_OTHER, true, NULL );
	if ( !IsErrorMaterial( pMaterial ) )
	{
		IMaterialInternal* pMatInt = static_cast<IMaterialInternal*>(pMaterial);
		pMatInt = pMatInt->GetRealTimeVersion(); //always work with the realtime material internally
		pMatInt->ToggleSuppression();
	}
}

void CMaterialSystem::ToggleDebugMaterial( char const* pMaterialName )
{
	// Note: if we use this function a lot, we'll want to do something else, like have them
	// pass in a texture group or reuse whatever texture group the material already had.
	// As it is, this is rarely used, so if it's not in TEXTURE_GROUP_OTHER, it'll go in 
	// TEXTURE_GROUP_SHARED.
	IMaterial* pMaterial = FindMaterial( pMaterialName, TEXTURE_GROUP_OTHER, false, NULL );
	if ( !IsErrorMaterial( pMaterial ) )
	{
		IMaterialInternal* pMatInt = static_cast<IMaterialInternal*>(pMaterial);
		pMatInt = pMatInt->GetRealTimeVersion(); //always work with the realtime material internally
		pMatInt->ToggleDebugTrace();
	}
	else
	{
		Warning("Unknown material %s\n", pMaterialName );
	}
}


//-----------------------------------------------------------------------------
// Used to iterate over all shaders for editing purposes
//-----------------------------------------------------------------------------
int CMaterialSystem::ShaderCount() const
{
	return ShaderSystem()->ShaderCount();
}

int CMaterialSystem::GetShaders( int nFirstShader, int nMaxCount, IShader **ppShaderList ) const
{
	return ShaderSystem()->GetShaders( nFirstShader, nMaxCount, ppShaderList );
}


//-----------------------------------------------------------------------------
// FIXME: Is there a better way of doing this?
// Returns shader flag names for editors to be able to edit them
//-----------------------------------------------------------------------------
int CMaterialSystem::ShaderFlagCount() const
{
	return ShaderSystem()->ShaderStateCount( );
}

const char *CMaterialSystem::ShaderFlagName( int nIndex ) const
{
	return ShaderSystem()->ShaderStateString( nIndex );
}


//-----------------------------------------------------------------------------
// Returns the currently active shader fallback for a particular shader
//-----------------------------------------------------------------------------
void CMaterialSystem::GetShaderFallback( const char *pShaderName, char *pFallbackShader, int nFallbackLength )
{
	// FIXME: This is pretty much a hack. We need a better way for the
	// editor to get ahold of shader fallbacks
	int nCount = ShaderCount();
	IShader** ppShaderList = (IShader**)stackalloc( nCount * sizeof(IShader) );
	GetShaders( 0, nCount, ppShaderList );

	do
	{
		int i;
		for ( i = 0; i < nCount; ++i )
		{
			if ( !Q_stricmp( pShaderName, ppShaderList[i]->GetName() ) )
				break;
		}

		// Didn't find a match!
		if ( i == nCount )
		{
			Q_strncpy( pFallbackShader, "wireframe", nFallbackLength );
			return;
		}

		// Found a match
		// FIXME: Theoretically, getting fallbacks should require a param list
		// In practice, it looks rare or maybe even neved done
		const char *pFallback = ppShaderList[i]->GetFallbackShader( NULL );
		if ( !pFallback )
		{
			Q_strncpy( pFallbackShader, pShaderName, nFallbackLength );
			return;
		}
		else
		{
			pShaderName = pFallback;
		}
	} while (true);
}

//-----------------------------------------------------------------------------
// Triggers OpenGL shader preloading at game startup
//-----------------------------------------------------------------------------
#if defined( DX_TO_GL_ABSTRACTION ) && !defined( _GAMECONSOLE )
void	CMaterialSystem::DoStartupShaderPreloading( void )
{
	GetRenderContextInternal()->DoStartupShaderPreloading();
}
#endif

	
void CMaterialSystem::SwapBuffers( void )
{
	VPROF_BUDGET( "CMaterialSystem::SwapBuffers", VPROF_BUDGETGROUP_SWAP_BUFFERS );
	GetRenderContextInternal()->SwapBuffers();
	g_FrameNum++;
}

bool CMaterialSystem::InEditorMode() const
{
	Assert( m_bGeneratedConfig );
	return g_config.bEditMode && CanUseEditorMaterials();
}

void CMaterialSystem::NoteAnisotropicLevel( int currentLevel )
{
	Assert( m_bGeneratedConfig );
	g_config.m_nForceAnisotropicLevel = currentLevel;
}

// Get the current config for this video card (as last set by control panel or the default if not)
const MaterialSystem_Config_t &CMaterialSystem::GetCurrentConfigForVideoCard() const
{
	Assert( m_bGeneratedConfig );
	return g_config;
}

// Does the device support the given MSAA level?
bool CMaterialSystem::SupportsMSAAMode( int nNumSamples )
{
	return g_pShaderAPI->SupportsMSAAMode( nNumSamples );
}

void CMaterialSystem::ReloadFilesInList( IFileList *pFilesToReload )
{
	if ( IsPC() )
	{
		// We have to flush the materials in 2 steps because they have recursive dependencies. The problem case
		// is if you have two materials, A and B, that depend on C. You tell A to reload and it also reloads C. Then
		// the filesystem thinks C doesn't need to be reloaded anymore. So when you get to B, it decides not to reload 
		// either since C doesn't need to be reloaded. To fix this, we ask all materials if they want to reload in
		// one stage, then in the next stage we actually reload the appropriate ones.
		MaterialHandle_t hNext;
		for ( MaterialHandle_t h=m_MaterialDict.FirstMaterial(); h != m_MaterialDict.InvalidMaterial(); h=hNext )
		{
			hNext = m_MaterialDict.NextMaterial( h );
			IMaterialInternal *pMat = m_MaterialDict.GetMaterialInternal( h );

			pMat->DecideShouldReloadFromWhitelist( pFilesToReload );
		}

		// Now reload the materials that wanted to be reloaded.
		for ( MaterialHandle_t h=m_MaterialDict.FirstMaterial(); h != m_MaterialDict.InvalidMaterial(); h=hNext )
		{
			hNext = m_MaterialDict.NextMaterial( h );
			IMaterialInternal *pMat = m_MaterialDict.GetMaterialInternal( h );

			pMat->ReloadFromWhitelistIfMarked();
		}

		// Flush out necessary textures.
		TextureManager()->ReloadFilesInList( pFilesToReload );
	}
}

// Does the device support the given CSAA level?
bool CMaterialSystem::SupportsCSAAMode( int nNumSamples, int nQualityLevel )
{
	return g_pShaderAPI->SupportsCSAAMode( nNumSamples, nQualityLevel );
}

void CMaterialSystem::SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias ) 
{
	g_pShaderAPI->SetShadowDepthBiasFactors( fShadowSlopeScaleDepthBias, fShadowDepthBias );
}

void CMaterialSystem::FlipCulling( bool bFlipCulling ) 
{
	g_pShaderAPI->FlipCulling( bFlipCulling );
}

bool CMaterialSystem::SupportsHDRMode( HDRType_t nHDRMode )
{
	return HardwareConfig()->SupportsHDRMode( nHDRMode );
}

bool CMaterialSystem::UsesSRGBCorrectBlending( void ) const
{
	return HardwareConfig()->UsesSRGBCorrectBlending();
}

// Get video card identitier
const MaterialSystemHardwareIdentifier_t &CMaterialSystem::GetVideoCardIdentifier( void ) const
{
	static MaterialSystemHardwareIdentifier_t foo;
	Assert( 0 );
	return foo;
}

void CMaterialSystem::AddModeChangeCallBack( ModeChangeCallbackFunc_t func )
{
	g_pShaderDeviceMgr->AddModeChangeCallback( func );
}

void CMaterialSystem::RemoveModeChangeCallBack( ModeChangeCallbackFunc_t func )
{
	g_pShaderDeviceMgr->RemoveModeChangeCallback( func );
}

//-----------------------------------------------------------------------------
// Gets configuration information associated with the display card.
// It will return a list of key values to set.
//-----------------------------------------------------------------------------
bool CMaterialSystem::GetRecommendedVideoConfig( KeyValues *pKeyValues )
{
	MaterialLock_t hLock = Lock();
	bool bResult = g_pShaderDeviceMgr->GetRecommendedVideoConfig( m_nAdapter, pKeyValues );
	Unlock( hLock );
	return bResult;
}

//-----------------------------------------------------------------------------
// Gets configuration information associated with a particular DX level.
// It will return a list of key values to set.
//-----------------------------------------------------------------------------
bool CMaterialSystem::GetRecommendedConfigurationInfo( int nDXLevel, KeyValues *pKeyValues )
{
	MaterialLock_t hLock = Lock();
	bool bResult = g_pShaderDeviceMgr->GetRecommendedConfigurationInfo( m_nAdapter, nDXLevel, pKeyValues );
	Unlock( hLock );
	return bResult;
}

//-----------------------------------------------------------------------------
// For dealing with device lost in cases where SwapBuffers isn't called all the time (Hammer)
//-----------------------------------------------------------------------------
void CMaterialSystem::HandleDeviceLost()
{
	if ( IsGameConsole() )
		return;

	g_pShaderAPI->HandleDeviceLost();
}
	
bool CMaterialSystem::UsingFastClipping( void )
{
	return (HardwareConfig()->UseFastClipping() || (HardwareConfig()->MaxUserClipPlanes() < 1));
};

int CMaterialSystem::StencilBufferBits( void )
{
	return HardwareConfig()->StencilBufferBits();
}

ITexture* CMaterialSystem::CreateRenderTargetTexture( 
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
	ImageFormat format, 
	MaterialRenderTargetDepth_t depth )
{
	return CreateNamedRenderTargetTextureEx( NULL, w, h, sizeMode, format, depth, TEXTUREFLAGS_CLAMPS|TEXTUREFLAGS_CLAMPT, 0 );
}

ITexture* CMaterialSystem::CreateNamedRenderTargetTexture( 
	const char *pRTName, 
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
	ImageFormat format, 
	MaterialRenderTargetDepth_t depth,
	bool bClampTexCoords, 
	bool bAutoMipMap )
{
	unsigned int textureFlags = 0;
	if ( bClampTexCoords )
	{
		textureFlags |= TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT;
	}

	unsigned int renderTargetFlags = 0;
	if ( bAutoMipMap )
	{
		renderTargetFlags |= CREATERENDERTARGETFLAGS_AUTOMIPMAP;
	}

	return CreateNamedRenderTargetTextureEx( pRTName, w, h, sizeMode, format, depth, textureFlags, renderTargetFlags );
}

inline RenderTargetType_t DepthTypeToRenderTargetType( MaterialRenderTargetDepth_t depth )
{
	// GR - determine RT type based on depth buffer requirements
	switch ( depth )
	{
	case MATERIAL_RT_DEPTH_SEPARATE:
		// using own depth buffer
		return RENDER_TARGET_WITH_DEPTH;

	case MATERIAL_RT_DEPTH_NONE:
		// no depth buffer
		return RENDER_TARGET_NO_DEPTH;

	case MATERIAL_RT_DEPTH_ONLY:
		// only depth buffer
		return RENDER_TARGET_ONLY_DEPTH;

	case MATERIAL_RT_DEPTH_SHARED:
	default:
		// using shared depth buffer
		return RENDER_TARGET;
	}
}

ITexture* CMaterialSystem::CreateNamedRenderTargetTextureEx( 
	const char *pRTName, 
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
	ImageFormat format, 
	MaterialRenderTargetDepth_t depth,
	unsigned int textureFlags, 
	unsigned int renderTargetFlags )
{
	if ( !m_nAllocatingRenderTargets )
	{
		Warning( "Tried to create render target outside of CMaterialSystem::BeginRenderTargetAllocation/EndRenderTargetAllocation block\n" );
		return NULL;
	}

	RenderTargetType_t rtType = DepthTypeToRenderTargetType( depth );

	
	ITextureInternal* pTex = TextureManager()->CreateRenderTargetTexture( pRTName, w, h, sizeMode, format, rtType, textureFlags, renderTargetFlags, false );
	pTex->IncrementReferenceCount();

#if defined( _X360 )
	if ( !( renderTargetFlags & CREATERENDERTARGETFLAGS_NOEDRAM ) )
	{
		// create the EDRAM surface that is bound to the RT Texture
		pTex->CreateRenderTargetSurface( 0, 0, IMAGE_FORMAT_UNKNOWN, true );
	}
#endif

	return pTex;
}

//-----------------------------------------------------------------------------------------------------
// New version which must be called inside BeginRenderTargetAllocation-EndRenderTargetAllocation block
//-----------------------------------------------------------------------------------------------------
ITexture *CMaterialSystem::CreateNamedRenderTargetTextureEx2(
	const char *pRTName, 
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
	ImageFormat format, 
	MaterialRenderTargetDepth_t depth,
	unsigned int textureFlags, 
	unsigned int renderTargetFlags )
{
	// Only proceed if we are between BeginRenderTargetAllocation and EndRenderTargetAllocation
	if ( !m_nAllocatingRenderTargets )
	{
		Warning( "Tried to create render target outside of CMaterialSystem::BeginRenderTargetAllocation/EndRenderTargetAllocation block\n" );
		return NULL;
	}

	ITexture* pTexture = CreateNamedRenderTargetTextureEx( pRTName, w, h, sizeMode, format, depth, textureFlags, renderTargetFlags );

	pTexture->DecrementReferenceCount(); // Follow the same convention as CTextureManager::LoadTexture (return refcount of 0).
	return pTexture;
}



ITexture *CMaterialSystem::CreateNamedMultiRenderTargetTexture(
	const char *pRTName, 
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
	ImageFormat format, 
	MaterialRenderTargetDepth_t depth,
	unsigned int textureFlags, 
	unsigned int renderTargetFlags )
{
	// Only proceed if we are between BeginRenderTargetAllocation and EndRenderTargetAllocation
	if ( !m_nAllocatingRenderTargets )
	{
		Warning( "Tried to create render target outside of CMaterialSystem::BeginRenderTargetAllocation/EndRenderTargetAllocation block\n" );
		return NULL;
	}

	RenderTargetType_t rtType = DepthTypeToRenderTargetType( depth );

	ITextureInternal* pTex =TextureManager()->CreateRenderTargetTexture( pRTName, w, h, sizeMode, format, rtType, textureFlags, renderTargetFlags, true );	

#if defined( _X360 )
	if ( !( renderTargetFlags & CREATERENDERTARGETFLAGS_NOEDRAM ) )
	{
		// create the EDRAM surface that is bound to the RT Texture
		pTex->CreateRenderTargetSurface( 0, 0, IMAGE_FORMAT_UNKNOWN, true );
	}
#endif


	return pTex; //ref count is 0
}



void CMaterialSystem::BeginRenderTargetAllocation( void )
{
	if ( m_bDisableRenderTargetAllocationForever )
	{
		Warning( "Tried BeginRenderTargetAllocation after game startup. If I let you do this, all users would suffer.\n" );
		return;
	}
	m_nAllocatingRenderTargets++;
}

void CMaterialSystem::EndRenderTargetAllocation( void )
{
	if ( m_bDisableRenderTargetAllocationForever )
		return;
	m_nAllocatingRenderTargets--;

	if ( ! m_nAllocatingRenderTargets )
	{
		if ( IsPC() && CanDownloadTextures() )
		{
			// Simulate an Alt-Tab...will cause RTs to be allocated first
			g_pShaderDevice->ReleaseResources();
			g_pShaderDevice->ReacquireResources();
		}
		TextureManager()->CacheExternalStandardRenderTargets();
	}
}

void CMaterialSystem::FinishRenderTargetAllocation( void )
{
	// disable all future render target allocation to prevent re-load bugs from creeping in.
	if (
		( CommandLine()->CheckParm( "-tools" ) == NULL ) && 
		( ! m_bRequestedEditorMaterials ) )
	{
		m_bDisableRenderTargetAllocationForever = true;
	}
}
	
void CMaterialSystem::ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly( void )
{
	m_bDisableRenderTargetAllocationForever = false;
}


//-----------------------------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------------------------
void CMaterialSystem::UpdateLightmap( int lightmapPageID, int lightmapSize[2],
										int offsetIntoLightmapPage[2], 
										float *pFloatImage, float *pFloatImageBump1,
										float *pFloatImageBump2, float *pFloatImageBump3 )
{
	CMatCallQueue *pCallQueue = GetRenderCallQueue();
	if ( !pCallQueue )
	{
		m_Lightmaps.UpdateLightmap( lightmapPageID, lightmapSize, offsetIntoLightmapPage, pFloatImage, pFloatImageBump1, pFloatImageBump2, pFloatImageBump3 );
	}
	else
	{
		ExecuteOnce( DebuggerBreakIfDebugging() );
	}
}


void CMaterialSystem::UpdatePaintmap( int paintmap, BYTE* pPaintData, int numRects, Rect_t* pRects )
{
	CMatCallQueue *pCallQueue = GetRenderCallQueue();
	if ( !pCallQueue )
	{
		m_Paintmaps.UpdatePaintmap( paintmap, pPaintData, numRects, pRects );
	}
	else
	{
		ExecuteOnce( DebuggerBreakIfDebugging() );
	}
}


//-----------------------------------------------------------------------------------------------------
// NVIDIA stereo
//-----------------------------------------------------------------------------------------------------
void CMaterialSystem::NVStereoUpdate()
{
	g_pShaderAPI->UpdateStereoTexture( TextureManager()->StereoParamTexture()->GetTextureHandle( 0 ), &m_bIsStereoActiveThisFrame );
}

bool CMaterialSystem::IsStereoSupported()
{
	if ( !m_bStereoBoolsInitialized )
	{
		m_bIsStereoSupported = g_pShaderAPI->IsStereoSupported();
		m_bStereoBoolsInitialized = true;
	}

	return m_bIsStereoSupported;
}

bool CMaterialSystem::IsStereoActiveThisFrame() const
{
	// NOTE: This is updated in NVStereoUpdate above
	return m_bIsStereoActiveThisFrame;
}


//-----------------------------------------------------------------------------------------------------
// 360 TTF Font Support
//-----------------------------------------------------------------------------------------------------
#if defined( _X360 )
HXUIFONT CMaterialSystem::OpenTrueTypeFont( const char *pFontname, int tall, int style )
{
	MaterialLock_t hLock = Lock();
	HXUIFONT result = g_pShaderAPI->OpenTrueTypeFont( pFontname, tall, style );
	Unlock( hLock );
	return result;
}
void CMaterialSystem::CloseTrueTypeFont( HXUIFONT hFont )
{
	MaterialLock_t hLock = Lock();
	g_pShaderAPI->CloseTrueTypeFont( hFont );
	Unlock( hLock );
}
bool CMaterialSystem::GetTrueTypeFontMetrics( HXUIFONT hFont, wchar_t wchFirst, wchar_t wchLast, XUIFontMetrics *pFontMetrics, XUICharMetrics *pCharMetrics )
{
	MaterialLock_t hLock = Lock();
	bool result = g_pShaderAPI->GetTrueTypeFontMetrics( hFont, wchFirst, wchLast, pFontMetrics, pCharMetrics );
	Unlock( hLock );
	return result;
}
bool CMaterialSystem::GetTrueTypeGlyphs( HXUIFONT hFont, int numChars, wchar_t *pWch, int *pOffsetX, int *pOffsetY, int *pWidth, int *pHeight, unsigned char *pRGBA, int *pRGBAOffset )
{
	MaterialLock_t hLock = Lock();
	bool result = g_pShaderAPI->GetTrueTypeGlyphs( hFont, numChars, pWch, pOffsetX, pOffsetY, pWidth, pHeight, pRGBA, pRGBAOffset );
	Unlock( hLock );
	return result;
}
#endif

//-----------------------------------------------------------------------------------------------------
// 360 Back Buffer access. Due to hardware, RT data must be blitted from EDRAM
// and converted.
//-----------------------------------------------------------------------------------------------------
#if defined( _X360 )
void CMaterialSystem::ReadBackBuffer( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pDstData, ImageFormat dstFormat, int dstStride ) 
{
	Assert( pSrcRect && pDstRect && pDstData );

	int fbWidth, fbHeight;
	g_pShaderAPI->GetBackBufferDimensions( fbWidth, fbHeight ); 

	if ( pDstRect->width > fbWidth || pDstRect->height > fbHeight )
	{
		Assert( 0 );
		return;
	}

	// intermediate results will be placed at (0,0)
	Rect_t	rect;
	rect.x = 0;
	rect.y = 0;
	rect.width = pDstRect->width;
	rect.height = pDstRect->height;

	ITexture *pTempRT;
	bool bStretch = ( pSrcRect->width != pDstRect->width || pSrcRect->height != pDstRect->height );
	if ( !bStretch )
	{
		// hijack an unused RT (no surface required) for 1:1 resolve work, fastest path
		pTempRT = FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
	}
	else
	{
		// hijack an unused RT (with surface abilities) for stretch work, slower path
		pTempRT = FindTexture( "_rt_WaterReflection", TEXTURE_GROUP_RENDER_TARGET );
	}

	Assert( !pTempRT->IsError() && pDstRect->width <= pTempRT->GetActualWidth() && pDstRect->height <= pTempRT->GetActualHeight() );
	GetRenderContextInternal()->CopyRenderTargetToTextureEx( pTempRT, 0, pSrcRect, &rect );

	// access the RT bits
	CPixelWriter writer;
	g_pShaderAPI->ModifyTexture( ((ITextureInternal*)pTempRT)->GetTextureHandle( 0 ) );
	if ( !g_pShaderAPI->TexLock( 0, 0, 0, 0, pTempRT->GetActualWidth(), pTempRT->GetActualHeight(), writer ) )
		return;

	// this will be adequate for non-block formats
	int srcStride = pTempRT->GetActualWidth() * ImageLoader::SizeInBytes( pTempRT->GetImageFormat() );

	// untile intermediate RT in place to achieve linear access
	XGUntileTextureLevel(
		pTempRT->GetActualWidth(),
		pTempRT->GetActualHeight(),
		0,
		XGGetGpuFormat( ImageLoader::ImageFormatToD3DFormat( pTempRT->GetImageFormat() ) ),
		0,
		(char*)writer.GetPixelMemory(),
		srcStride,
		NULL,
		writer.GetPixelMemory(),
		NULL );

	// swap back to x86 order as expected by image conversion
	ImageLoader::ByteSwapImageData( (unsigned char*)writer.GetPixelMemory(), srcStride*pTempRT->GetActualHeight(), pTempRT->GetImageFormat() );

	// convert to callers format
	Assert( dstFormat == IMAGE_FORMAT_RGB888 );
	ImageLoader::ConvertImageFormat( (unsigned char*)writer.GetPixelMemory(), pTempRT->GetImageFormat(), pDstData, dstFormat, pDstRect->width, pDstRect->height, srcStride, dstStride );

	g_pShaderAPI->TexUnlock();
}
#endif

#if defined( _X360 )
void CMaterialSystem::PersistDisplay() 
{
	g_pShaderAPI->PersistDisplay();
}
#endif

#if defined( _X360 )
void *CMaterialSystem::GetD3DDevice() 
{
	return g_pShaderAPI->GetD3DDevice();
}
#endif

#if defined( _X360 )
bool CMaterialSystem::OwnGPUResources( bool bEnable ) 
{
	return g_pShaderAPI->OwnGPUResources( bEnable );
}
#endif

//-----------------------------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------------------------
class CThreadRelease : public CJob
{
	virtual JobStatus_t DoExecute()
	{
		g_pShaderAPI->ReleaseThreadOwnership();

		return JOB_OK;
	}
};

void CMaterialSystem::ThreadRelease( )
{
	if ( !m_bThreadHasOwnership )
	{
		return;
	}

	double flStartTime, flEndThreadRelease, flEndTime;

	if ( mat_queue_report.GetInt() )
	{
		flStartTime = Plat_FloatTime();
	}

	CJob		*pActiveAsyncJob = new CThreadRelease();
	CreateMatQueueThreadPool()->AddJob( pActiveAsyncJob );
	pActiveAsyncJob->WaitForFinish();

	SafeRelease( pActiveAsyncJob );

	if ( mat_queue_report.GetInt() )
	{
		flEndThreadRelease = Plat_FloatTime();
	}

	g_pShaderAPI->AcquireThreadOwnership();

	m_bThreadHasOwnership = false;

	if ( mat_queue_report.GetInt() )
	{
		flEndTime = Plat_FloatTime();
		double flResult = ( flEndTime - flStartTime ) * 1000.0;

		if ( mat_queue_report.GetInt() == -1 || flResult > mat_queue_report.GetFloat() )
		{
			Color red(  200,  20,  20, 255 );
			ConColorMsg( red, "CMaterialSystem::ThreadRelease: %0.2fms = Release:%0.2fms + Acquire:%0.2fms\n", flResult, ( flEndThreadRelease - flStartTime ) * 1000.0, ( flEndTime - flEndThreadRelease ) * 1000.0 );
		}
	}
}

class CThreadAcquire : public CJob
{
	virtual JobStatus_t DoExecute()
	{
		g_pShaderAPI->AcquireThreadOwnership();

		return JOB_OK;
	}
};

void CMaterialSystem::ThreadAcquire( )
{
	double flStartTime, flEndTime;

	if ( mat_queue_report.GetInt() )
	{
		flStartTime = Plat_FloatTime();
	}

	g_pShaderAPI->ReleaseThreadOwnership();

	CJob		*pActiveAsyncJob = new CThreadAcquire();
	CreateMatQueueThreadPool()->AddJob( pActiveAsyncJob );
	//	while we could wait for this job to finish, there's no reason too
	//	pActiveAsyncJob->WaitForFinish();

	SafeRelease( pActiveAsyncJob );

	m_bThreadHasOwnership = true;

	if ( mat_queue_report.GetInt() )
	{
		flEndTime = Plat_FloatTime();
		double flResult = ( flEndTime - flStartTime ) * 1000.0;

		if ( mat_queue_report.GetInt() == -1 || flResult > mat_queue_report.GetFloat() )
		{
			Color red(  200,  20,  20, 255 );
			ConColorMsg( red, "CMaterialSystem::ThreadAcquire: %0.2fms\n", flResult );
		}
	}
}


//-----------------------------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------------------------
MaterialLock_t CMaterialSystem::Lock()
{
	IMatRenderContextInternal *pCurContext = GetRenderContextInternal();

#ifdef _X360
	// use this to help catch synchronization blocks in PIX timing captures
	PIXEVENT( pCurContext, "CMaterialSystem::Lock" );
#endif

#ifndef _PS3

	if ( pCurContext != &m_HardwareRenderContext && m_pActiveAsyncJob )
	{
		m_pActiveAsyncJob->WaitForFinishAndRelease();
		m_pActiveAsyncJob = NULL;
	}
#else
	if ( pCurContext != &m_HardwareRenderContext && m_bQMSJobSubmitted )
	{
		g_pGcmSharedData->WaitForQMS();
		m_bQMSJobSubmitted = 0;
	}
#endif

	g_MatSysMutex.Lock();

	MaterialLock_t hMaterialLock = (MaterialLock_t)pCurContext;
	m_pRenderContext = &m_HardwareRenderContext;

	if ( m_ThreadMode != MATERIAL_SINGLE_THREADED )
	{
		g_pShaderAPI->SetDisallowAccess( false );
		if ( pCurContext->GetCallQueueInternal() )
		{
#ifdef MAT_QUEUED_OWN_THREADPOOL
			ThreadRelease();
#else
			g_pShaderAPI->AcquireThreadOwnership();
#endif
		}
	}

	g_pShaderAPI->ShaderLock();

	return hMaterialLock;
}

//-----------------------------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------------------------
void CMaterialSystem::Unlock( MaterialLock_t hMaterialLock )
{
	IMatRenderContextInternal *pRenderContext = (IMatRenderContextInternal *)hMaterialLock;
	m_pRenderContext =  pRenderContext ;
	g_pShaderAPI->ShaderUnlock();

	if ( m_ThreadMode == MATERIAL_QUEUED_SINGLE_THREADED )
	{
		g_pShaderAPI->SetDisallowAccess( true );
	}
	else if ( m_ThreadMode == MATERIAL_QUEUED_THREADED )
	{
		if ( pRenderContext->GetCallQueueInternal() )
		{
#ifndef MAT_QUEUED_OWN_THREADPOOL
			g_pShaderAPI->ReleaseThreadOwnership();
#endif
		}
	}
	g_MatSysMutex.Unlock();
}

//-----------------------------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------------------------
CMatCallQueue *CMaterialSystem::GetRenderCallQueue()
{
	IMatRenderContextInternal *pRenderContext = m_pRenderContext;
	return pRenderContext ? pRenderContext->GetCallQueueInternal() : NULL;
}

void CMaterialSystem::UnbindMaterial( IMaterial *pMaterial )
{
	Assert( (pMaterial == NULL) || ((IMaterialInternal *)pMaterial)->IsRealTimeVersion() );
	if ( m_HardwareRenderContext.GetCurrentMaterial() == pMaterial )
	{
		m_HardwareRenderContext.Bind( g_pErrorMaterial, NULL );
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMaterialSystem::CompactMemory()
{
	for ( int i = 0; i < ARRAYSIZE(m_QueuedRenderContexts); i++)
	{
		m_QueuedRenderContexts[i].CompactMemory();
	}
}

//-----------------------------------------------------------------------------
// Material + texture related commands
//-----------------------------------------------------------------------------
void CMaterialSystem::DebugPrintUsedMaterials( const CCommand &args )
{
	if( args.ArgC() == 1 )
	{
		DebugPrintUsedMaterials( NULL, false );
	}
	else
	{
		DebugPrintUsedMaterials( args[ 1 ], false );
	}
}

void CMaterialSystem::DebugPrintUsedMaterialsVerbose( const CCommand &args )
{
	if( args.ArgC() == 1 )
	{
		DebugPrintUsedMaterials( NULL, true );
	}
	else
	{
		DebugPrintUsedMaterials( args[ 1 ], true );
	}
}

void CMaterialSystem::DebugPrintAspectRatioInfo( const CCommand &args )
{
	int width, height;
	GetBackBufferDimensions( width, height );

	const AspectRatioInfo_t &aspectRatioInfo = GetAspectRatioInfo();

	DevMsg( "========================\n" );
	DevMsg( "m_bIsWidescreen: %s\n", aspectRatioInfo.m_bIsWidescreen ? "true" : "false" );
	DevMsg( "m_bIsHidef: %s\n", aspectRatioInfo.m_bIsHidef ? "true" : "false" );
	DevMsg( "m_flFrameBufferAspectRatio: %f\n", aspectRatioInfo.m_flFrameBufferAspectRatio );
	DevMsg( "m_flPhysicalAspectRatio: %f\n", aspectRatioInfo.m_flPhysicalAspectRatio );
	DevMsg( "m_flFrameBuffertoPhysicalScalar: %f\n", aspectRatioInfo.m_flFrameBuffertoPhysicalScalar );
	DevMsg( "fb width: %d fb height: %d\n", width, height );
	DevMsg( "========================\n" );
}

void CMaterialSystem::DebugPrintUsedTextures( const CCommand &args )
{
	DebugPrintUsedTextures();
}

#if defined( _X360 ) || defined( _PS3 )
void CMaterialSystem::ListUsedMaterials( const CCommand &args )
{
	ListUsedMaterials();
}
#endif // !_X360

void CMaterialSystem::ReloadAllMaterials( const CCommand &args )
{
	ReloadMaterials( NULL );
}

void CMaterialSystem::ReloadMaterials( const CCommand &args )
{
	if( args.ArgC() != 2 )
	{
		Log_Warning( LOG_MaterialSystemConsole, "Usage: mat_reloadmaterial material_name_substring\n"
					"   or  mat_reloadmaterial substring1*substring2*...*substringN\n" );
		return;
	}
	ReloadMaterials( args[ 1 ] );
}

void CMaterialSystem::ReloadTextures( const CCommand &args )
{
	ReloadTextures();
}

#ifdef _PS3
//void CMaterialSystem::GetVRAMScreenShotInfo( char **pointerToRawImageData, uint32 *uWidth, uint32 *uHeight, uint32 *uPitch, VRAMScreenShotInfoColor_t *colour )
void CMaterialSystem::TransmitScreenshotToVX()
{
	extern char *GetScreenShotInfoForVX( IDirect3DDevice9 *pDevice, uint32 *uWidth, uint32 *uHeight, uint32 *uPitch, uint32 *colour );
	
	uint32 uWidth, uHeight, uPitch, uColor;
	char *pFrameBuffer = GetScreenShotInfoForVX( Dx9Device(), &uWidth, &uHeight, &uPitch, &uColor );
	if ( pFrameBuffer )
	{
		g_pValvePS3Console->TransmitScreenshot( pFrameBuffer, uWidth, uHeight, uPitch, uColor );
	}
	
	//return GetVRAMScreenShotInfoGCM( pointerToRawImageData, Dx9Device(), uWidth, uHeight, uPitch, colour);
}

void CMaterialSystem::CompactRsxLocalMemory( char const *szReason )
{
	extern void Ps3gcmLocalMemoryAllocator_CompactWithReason( char const *szReason );
	Ps3gcmLocalMemoryAllocator_CompactWithReason( szReason );
}

void CMaterialSystem::SetFlipPresentFrequency( int nNumVBlanks )
{
	extern void Ps3gcmFlip_SetFlipPresentFrequency( int nNumVBlanks );
	// 7ltodo Ps3gcmFlip_SetFlipPresentFrequency( nNumVBlanks );
}
#endif


void CMaterialSystem::SpinPresent( uint nFrames )
{
	for( uint i = 0; i < nFrames; ++i )
	{
		// BeginScene(); // do we need this?
		g_pShaderAPI->ClearColor3ub( 0, 0, 0 );
		g_pShaderAPI->ClearBuffers( true, true, true, -1, -1 );
		g_pShaderDevice->Present();
		// EndScene(); // do we need this?
	}
}


CON_COMMAND( mat_hdr_enabled, "Report if HDR is enabled for debugging" )
{
	if( HardwareConfig() && HardwareConfig()->GetHDREnabled() )
	{
		Log_Warning( LOG_MaterialSystemConsole, "HDR Enabled\n" );
	}
	else
	{
		Log_Warning( LOG_MaterialSystemConsole, "HDR Disabled\n" );
	}
}

#ifdef _PS3
#include "shaderutil_ps3nonvirt.inl"
#endif

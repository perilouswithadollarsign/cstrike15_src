//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: loads and unloads main matsystem dll and interface
//
//===========================================================================//

#include "render_pch.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/ivballoctracker.h"
#include "materialsystem/imesh.h"
#include "tier0/dbg.h"
#include "sys_dll.h"
#include "host.h"
#include "cmodel_engine.h"
#include "modelloader.h"
#include "staticpropmgr.h"
#include "gl_model_private.h"
#include "view.h"
#include "gl_matsysiface.h"
#include "gl_cvars.h"
#include "gl_lightmap.h"
#include "lightcache.h"
#include "vstdlib/random.h"
#include "tier0/icommandline.h"
#include "draw.h"
#include "decal_private.h"
#include "l_studio.h"
#include "keyvalues.h"
#include "materialsystem/imaterial.h"
#include "gl_shader.h"
#include "ivideomode.h"
#include "cdll_engine_int.h"
#include "utldict.h"
#include "filesystem.h"
#include "host_saverestore.h"
#include "server.h"
#include "game/client/iclientrendertargets.h"
#include "tier2/tier2.h"
#include "videocfg/videocfg.h"
#include "LoadScreenUpdate.h"
#include "cl_main.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Controls.h>
#include "GameEventManager.h"
#if defined( _X360 )
#include "xbox/xbox_launch.h"
#endif

extern IFileSystem *g_pFileSystem;
#ifndef DEDICATED
#include "iregistry.h"
#endif

#include "igame.h"

#include "toolframework/itoolframework.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Start the frame count at one so stuff gets updated the first frame
int	r_framecount = 1;               // used for dlight + lightstyle push checking
int	d_lightstylevalue[256];
int	d_lightstylenumframes[256];

const MaterialSystem_Config_t *g_pMaterialSystemConfig;

static CSysModule	*g_MaterialsDLL = NULL;
bool g_LostVideoMemory = false;

IMaterial*	g_materialEmpty;	// purple checkerboard for missing textures

void ReleaseMaterialSystemObjects( int nChangeFlags );
void RestoreMaterialSystemObjects( int nChangeFlags );

static ConVar mat_shadowstate( "mat_shadowstate", "1" );
static ConVar mat_maxframelatency( "mat_maxframelatency", "1" );
ConVar mat_debugalttab( "mat_debugalttab", "0", FCVAR_CHEAT );

// Static pointers to renderable textures
static CTextureReference g_PowerOfTwoFBTexture;
static CTextureReference g_WaterReflectionTexture;
static CTextureReference g_WaterRefractionTexture;
static CTextureReference g_CameraTexture;
static CTextureReference g_BuildCubemaps16BitTexture;
static CTextureReference g_QuarterSizedFBTexture0;
static CTextureReference g_QuarterSizedFBTexture1;
static CTextureReference g_QuarterSizedFBTexture2;
static CTextureReference g_QuarterSizedFBTexture3;
static CTextureReference g_TeenyFBTexture0;
static CTextureReference g_TeenyFBTexture1;
static CTextureReference g_TeenyFBTexture2;
#ifdef _PS3
static CTextureReference g_FullFrameRawBufferAliasPS3_BackBuffer;
static CTextureReference g_FullFrameRawBufferAliasPS3_DepthBuffer;
#endif
static CTextureReference g_FullFrameFBTexture0;
static CTextureReference g_FullFrameFBTexture1;
static CTextureReference g_FullFrameFBTexture2;
static CTextureReference g_FullFrameDepth;

#if defined( _X360 )
	static CTextureReference g_RtGlowTexture360;
#endif

// each sort ID's mesh for the depth fill render
CUtlVector<IMesh *> g_DepthMeshForSortID;
// Each surface's first vertex index in the depth fill VB
CUtlVector<uint16> g_DepthFillVBFirstVertexForSurface;

void WorldStaticMeshCreate( void );
void WorldStaticMeshDestroy( void );
int GetScreenAspectMode( int width, int height );

ConVar	r_norefresh( "r_norefresh","0");
ConVar	r_decals( "r_decals", "2048" );

ConVar	r_lightmap( "r_lightmap", "-1", FCVAR_CHEAT | FCVAR_MATERIAL_SYSTEM_THREAD );
ConVar	r_lightstyle( "r_lightstyle","-1", FCVAR_CHEAT | FCVAR_MATERIAL_SYSTEM_THREAD );
ConVar	r_dynamic( "r_dynamic","1", FCVAR_RELEASE );

ConVar  mat_norendering( "mat_norendering", "0", FCVAR_CHEAT );
ConVar	mat_wireframe(  "mat_wireframe", "0", FCVAR_CHEAT );
ConVar	mat_luxels(  "mat_luxels", "0", FCVAR_CHEAT );
ConVar	mat_normals(  "mat_normals", "0", FCVAR_CHEAT );
ConVar	mat_bumpbasis(  "mat_bumpbasis", "0", FCVAR_CHEAT );
ConVar	mat_envmapsize(  "mat_envmapsize", "128" );
ConVar  mat_envmaptgasize( "mat_envmaptgasize", "32.0" );
ConVar  mat_levelflush( "mat_levelflush", "1" );
ConVar  mat_fastspecular( "mat_fastspecular", "1", 0, "Enable/Disable specularity for visual testing.  Will not reload materials and will not affect perf." );
ConVar  mat_fullbright( "mat_fullbright","0", FCVAR_CHEAT );

static ConVar mat_monitorgamma( "mat_monitorgamma", "2.2", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "monitor gamma (typically 2.2 for CRT and 1.7 for LCD)", true, 1.6f, true, 2.6f  );
static ConVar mat_monitorgamma_tv_range_min( "mat_monitorgamma_tv_range_min", "16" );
static ConVar mat_monitorgamma_tv_range_max( "mat_monitorgamma_tv_range_max", "235" );
// TV's generally have a 2.5 gamma, so we need to convert our 2.2 frame buffer into a 2.5 frame buffer for display on a TV
static ConVar mat_monitorgamma_tv_exp( "mat_monitorgamma_tv_exp", "2.5", 0, "", true, 1.0f, true, 4.0f );

static ConVar mat_monitorgamma_tv_enabled( "mat_monitorgamma_tv_enabled", IsGameConsole() ? "1" : "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "" );
				  
ConVar r_drawbrushmodels( "r_drawbrushmodels", "1", FCVAR_CHEAT, "Render brush models. 0=Off, 1=Normal, 2=Wireframe" );

ConVar r_shadowrendertotexture( "r_shadowrendertotexture", "0" );

#if ( defined( CSTRIKE15 ) && defined( _PS3 ) )
ConVar r_flashlightdepthtexture( "r_flashlightdepthtexture", "1" );
#else
ConVar r_flashlightdepthtexture( "r_flashlightdepthtexture", "1" );
#endif

// On non-gameconsoles mat_motion_blur_enabled now comes from video.txt/videodefaults.txt
ConVar mat_motion_blur_enabled( "mat_motion_blur_enabled", IsGameConsole() ? "1" : "0" );

// Note: this is only here so we can ship an update without changing materialsystem.dll.
// Once we ship materialsystem.dll again, we can get rid of this and make the only one exist in materialsystem.dll.
ConVar mat_depthbias_normal( "mat_depthbias_normal", "0.0f", FCVAR_CHEAT );

static ITexture *CreateFullFrameDepthTexture( void );

static void mat_resolveFullFrameDepth_changed( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConVar *cVar = (ConVar*)(var);
	int newVal = cVar->GetInt();
	int oldVal = atoi( pOldValue );

	static int oldNonZeroVal = -1;

	if ( newVal == oldVal )
		return;

	g_pMaterialSystem->ReloadMaterials( "particle" );
}
static ConVar mat_resolveFullFrameDepth( "mat_resolveFullFrameDepth", "0", FCVAR_CHEAT, "Enable depth resolve to a texture. 0=disable, 1=enable via resolve tricks if supported in hw, otherwise disable, 2=force extra depth only pass", mat_resolveFullFrameDepth_changed );


static void NukeModeSwitchSaveGames( void )
{
	if( g_pFileSystem->FileExists( "SAVE\\modeswitchsave.sav" ) )
	{
		g_pFileSystem->RemoveFile( "SAVE\\modeswitchsave.sav" );
	}
	if( g_pFileSystem->FileExists( "SAVE\\modeswitchsave.tga" ) )
	{
		g_pFileSystem->RemoveFile( "SAVE\\modeswitchsave.tga" );
	}
}


void mat_hdr_level_Callback( IConVar *var, const char *pOldString, float flOldValue )
{
	if ( IsX360() )
	{
		// can't support, expected to be static
		return;
	}

	// CSGO doesn't support any values other than 2.
	mat_hdr_level.SetValue( clamp( mat_hdr_level.GetInt(), 2, 2 ) );

	// Can do any reloading that is necessary here upon change.
	// FIXME: should check if there is actually going to be a change here (ie. are we able to run in HDR
	// given the current map and hardware.
#ifndef DEDICATED
	if ( g_pMaterialSystemHardwareConfig->GetHardwareHDRType() != HDR_TYPE_NONE &&
         saverestore->IsValidSave() &&
		 modelloader->LastLoadedMapHasHDRLighting() &&
		 sv.GetMaxClients() == 1 &&
		 !sv.IsLevelMainMenuBackground()
		 )
	{
		NukeModeSwitchSaveGames();
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "save modeswitchsave;wait;load modeswitchsave\n" );
	}
#endif
}

// Convar range change to [2,2] since CS:GO does not support any other setting.
ConVar mat_hdr_level( "mat_hdr_level", "2", FCVAR_DEVELOPMENTONLY, 
					  "Set to 0 for no HDR, 1 for LDR+bloom on HDR maps, and 2 for full HDR on HDR maps.",
					  mat_hdr_level_Callback );

MaterialSystem_SortInfo_t *materialSortInfoArray = 0;
static bool s_bConfigLightingChanged = false;

extern unsigned long GetRam();


//-----------------------------------------------------------------------------
// return true if lightmaps need to be redownloaded
//-----------------------------------------------------------------------------
bool MaterialConfigLightingChanged()
{
	return s_bConfigLightingChanged;
}

void ClearMaterialConfigLightingChanged()
{
	s_bConfigLightingChanged = false;
}

//-----------------------------------------------------------------------------
// Reads convars from the registry
//-----------------------------------------------------------------------------
static void ReadMaterialSystemConfigFromRegistry( MaterialSystem_Config_t &config )
{
#if defined(DEDICATED) || defined(_GAMECONSOLE)
	return;
#else
	// Create and Init the video config block.
	KeyValues *pConfigKeys = new KeyValues( "VideoConfig" );
	if ( !pConfigKeys )
		return;

	if ( !ReadCurrentVideoConfig( pConfigKeys ) )
	{
		pConfigKeys->deleteThis();
		return;
	}


	// Does gamma need to have config set here too!

	// Get the window sizes and (whether or not we are windowed).
	KeyValues *pFindKey = pConfigKeys->FindKey( "setting.defaultres" );
	if ( pFindKey )
	{
		config.m_VideoMode.m_Width = pFindKey->GetInt();
	}

	pFindKey = pConfigKeys->FindKey( "setting.defaultresheight" );
	if ( pFindKey )
	{
		config.m_VideoMode.m_Height = pFindKey->GetInt();
	}

	pFindKey = pConfigKeys->FindKey( "setting.fullscreen" );
	if ( pFindKey )
	{
		bool bFullscreen = pFindKey->GetBool();
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, !bFullscreen );
	}

	pFindKey = pConfigKeys->FindKey( "setting.nowindowborder" );
	if ( pFindKey )
	{
		bool bNoWindowBorder = pFindKey->GetBool();
		config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WINDOW_BORDER, bNoWindowBorder );
	}

	UpdateVideoConfigConVars( pConfigKeys );

	// Destroy the keys.
	pConfigKeys->deleteThis();
#endif
}

//=============================================================================
// FIXME! This was copied from VUI\perfwizardpanel.cpp
struct RatioToAspectMode_t
{
	int anamorphic;
	float aspectRatio;
};
RatioToAspectMode_t g_RatioToAspectModes[] =
{
	{	0,		4.0f / 3.0f },
	{	1,		16.0f / 9.0f },
	{	2,		16.0f / 10.0f },
	{	2,		1.0f },
};

//--------------------------------------------------------------------------------------------------------------
int GetScreenAspectMode( int width, int height )
{
	float aspectRatio = (float)width / (float)height;

	// just find the closest ratio
	float closestAspectRatioDist = 99999.0f;
	int closestAnamorphic = 0;
	for (int i = 0; i < ARRAYSIZE(g_RatioToAspectModes); i++)
	{
		float dist = fabs( g_RatioToAspectModes[i].aspectRatio - aspectRatio );
		if (dist < closestAspectRatioDist)
		{
			closestAspectRatioDist = dist;
			closestAnamorphic = g_RatioToAspectModes[i].anamorphic;
		}
	}

	return closestAnamorphic;
}

//-----------------------------------------------------------------------------
// Writes convars into the registry
//-----------------------------------------------------------------------------
static void WriteMaterialSystemConfigToRegistry( const MaterialSystem_Config_t &config )
{
#if defined(DEDICATED) || defined(_GAMECONSOLE)
	return;
#else
	ConVarRef defaultres_restart( "defaultres_restart" );
	ConVarRef defaultresheight_restart( "defaultresheight_restart" );

	int nWidth = defaultres_restart.GetInt() != -1 ? defaultres_restart.GetInt() : config.m_VideoMode.m_Width;
	int nHeight = defaultresheight_restart.GetInt() != -1 ? defaultresheight_restart.GetInt() : config.m_VideoMode.m_Height;

	int nAspectRatioMode = GetScreenAspectMode( nWidth, nHeight );
	UpdateCurrentVideoConfig( config.m_VideoMode.m_Width, config.m_VideoMode.m_Height, nAspectRatioMode, !config.Windowed(), config.NoWindowBorder(), true );
#endif
}

//-----------------------------------------------------------------------------
// Override config with command line params
//-----------------------------------------------------------------------------
static void OverrideMaterialSystemConfigFromCommandLine( MaterialSystem_Config_t &config )
{
	if ( IsX360() )
	{
		// these overrides cannot be supported
		// the console configuration is explicit
		return;
	}

	// Check for windowed mode command line override
	if ( CommandLine()->FindParm( "-sw" ) || 
		CommandLine()->FindParm( "-startwindowed" ) ||
		CommandLine()->FindParm( "-windowed" ) ||
		CommandLine()->FindParm( "-window" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	}
	// Check for fullscreen override
	else if ( CommandLine()->FindParm( "-full" ) ||	CommandLine()->FindParm( "-fullscreen" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, false );
	}

	// Get width and height
	if ( CommandLine()->FindParm( "-width" ) || CommandLine()->FindParm( "-w" ) )
	{
		config.m_VideoMode.m_Width = CommandLine()->ParmValue( "-width", config.m_VideoMode.m_Width );
		config.m_VideoMode.m_Width = CommandLine()->ParmValue( "-w", config.m_VideoMode.m_Width );
		if( !( CommandLine()->FindParm( "-height" ) || CommandLine()->FindParm( "-h" ) ) )
		{
			config.m_VideoMode.m_Height = ( config.m_VideoMode.m_Width * 3 ) / 4;
		}
	}
	if ( CommandLine()->FindParm( "-height" ) || CommandLine()->FindParm( "-h" ) )
	{
		config.m_VideoMode.m_Height = CommandLine()->ParmValue( "-height", config.m_VideoMode.m_Height );
		config.m_VideoMode.m_Height = CommandLine()->ParmValue( "-h", config.m_VideoMode.m_Height );
	}

	if ( CommandLine()->FindParm( "-resizing" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, CommandLine()->CheckParm( "-resizing" ) ? true : false );
	}

	if ( CommandLine()->FindParm( "-mat_vsync" ) )
	{
		int vsync = CommandLine()->ParmValue( "-mat_vsync", 0 );
		config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, vsync == 0 );
	}

	if ( CommandLine()->FindParm( "-mat_antialias" ) )
	{
		config.m_nAASamples = CommandLine()->ParmValue( "-mat_antialias", config.m_nAASamples );
		
		ConVarRef mat_antialias( "mat_antialias" );
		mat_antialias.SetValue( config.m_nAASamples );
	}

	if ( CommandLine()->FindParm( "-mat_aaquality" ) )
	{
		config.m_nAAQuality = CommandLine()->ParmValue( "-mat_aaquality", config.m_nAAQuality );
		
		ConVarRef mat_aaquality( "mat_aaquality" );
		mat_aaquality.SetValue( config.m_nAAQuality );
	}

	if ( CommandLine()->FindParm( "-csm_quality_level" ) )
	{
		int nCSMQuality = CommandLine()->ParmValue( "-csm_quality_level", CSMQUALITY_VERY_LOW );
		config.m_nCSMQuality = (CSMQualityMode_t)clamp( nCSMQuality, CSMQUALITY_VERY_LOW, CSMQUALITY_TOTAL_MODES - 1 );
		
		// Just slam the convar because CMaterialSystem::ReadConfigFromConVars() just overrides the config anyway. ARGH.
		ConVarRef csm_quality_level( "csm_quality_level" );
		csm_quality_level.SetValue( config.m_nCSMQuality );
	}

	if ( CommandLine()->FindParm( "-mat_resolveFullFrameDepth" ) )
	{
		int resolveFullFrameDepth = CommandLine()->ParmValue( "-mat_resolveFullFrameDepth", 1 );

		mat_resolveFullFrameDepth.SetValue( resolveFullFrameDepth );
	}

	// safe mode
	if ( CommandLine()->FindParm( "-safe" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
		config.m_VideoMode.m_Width = 640;
		config.m_VideoMode.m_Height = 480;
		config.m_VideoMode.m_RefreshRate = 0;
		config.m_nAASamples = 0;
		config.m_nAAQuality = 0;
		config.m_bWantTripleBuffered = false;
		config.m_nCSMQuality = CSMQUALITY_VERY_LOW;
	}
}


//-----------------------------------------------------------------------------
// Updates the material system config
//-----------------------------------------------------------------------------
void OverrideMaterialSystemConfig( MaterialSystem_Config_t &config )
{
	// enable/disable flashlight support based on mod (user can also set this explicitly)
	// FIXME: this is only here because dxsupport_override.cfg is currently broken
	ConVarRef mat_supportflashlight( "mat_supportflashlight" );
	if ( mat_supportflashlight.GetInt() == -1 )
	{
		const char * gameName = COM_GetModDirectory();
		if ( !V_stricmp( gameName, "portal" ) ||
			 !V_stricmp( gameName, "tf" ) )
		{
			mat_supportflashlight.SetValue( false );
		}
		else
		{
			mat_supportflashlight.SetValue( true );
		}
	}
	config.m_bSupportFlashlight = mat_supportflashlight.GetBool();

	// apply the settings in the material system
	bool bLightmapsNeedReloading = materials->OverrideConfig( config, false );
	if ( bLightmapsNeedReloading )
	{
		s_bConfigLightingChanged = true;
	}
}	


// auto config version to store in the registry so we can force reconfigs if needed
#define AUTOCONFIG_VERSION 1 

//-----------------------------------------------------------------------------
// Purpose: Initializes configuration
//-----------------------------------------------------------------------------
void InitMaterialSystemConfig( bool bInEditMode )
{
	// get the default config for the current card as a starting point.
	g_pMaterialSystemConfig = &materials->GetCurrentConfigForVideoCard();
	if ( !g_pMaterialSystemConfig )
	{
		Sys_Error( "Could not get the material system config record!" );
	}

	if ( bInEditMode )
		return;

	MaterialSystem_Config_t config = *g_pMaterialSystemConfig;

#if !defined(DEDICATED)

	// Capture autoconfig.
	if ( CommandLine()->FindParm( "-autoconfig" ) )
	{
#ifdef _GAMECONSOLE
		AssertMsg( false, "VideoCFG not supported on Xbox 360." );
#else
		ResetVideoConfigToDefaults();
#endif
	}

	ReadMaterialSystemConfigFromRegistry( config );

#endif

	OverrideMaterialSystemConfigFromCommandLine( config );
	OverrideMaterialSystemConfig( config );
	
	// now, set default hdr state
	bool bEnableHDR = ( mat_hdr_level.GetInt() >= 2 );
	g_pMaterialSystemHardwareConfig->SetHDREnabled( bEnableHDR );

	UpdateMaterialSystemConfig();
}


//-----------------------------------------------------------------------------
// Updates the material system config
//-----------------------------------------------------------------------------
void UpdateMaterialSystemConfig( void )
{
	// INFESTED_DLL - Alien Swarm doesn't want fullbright turned on when there are no lights (since it uses dynamic lights and skips vrad)
	static char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );
	if ( host_state.worldbrush && !host_state.worldbrush->lightdata && Q_stricmp( gamedir, "infested" ) )
	{
		mat_fullbright.SetValue( 1 );
	}
	
	// apply the settings in the material system
	bool bLightmapsNeedReloading = materials->UpdateConfig( false );
	if ( bLightmapsNeedReloading )
	{
		s_bConfigLightingChanged = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets all the relevant keyvalue data to be uploaded as part of the benchmark data
// Input  : *dataToUpload - keyvalue set that will be uploaded
//-----------------------------------------------------------------------------
void GetMaterialSystemConfigForBenchmarkUpload(KeyValues *dataToUpload)
{
#if !defined(DEDICATED)
	// hardware info
	MaterialAdapterInfo_t driverInfo;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), driverInfo );

	dataToUpload->SetInt( "vendorID", driverInfo.m_VendorID );
	dataToUpload->SetInt( "deviceID", driverInfo.m_DeviceID );
	dataToUpload->SetInt( "ram", GetRam() );

	const CPUInformation& pi = GetCPUInformation();
	double fFrequency = pi.m_Speed / 1000000.0;
	dataToUpload->SetInt( "cpu_speed", (int)fFrequency );
	dataToUpload->SetString( "cpu", pi.m_szProcessorID );

	// material system settings
	dataToUpload->SetInt( "width", g_pMaterialSystemConfig->m_VideoMode.m_Width );
	dataToUpload->SetInt( "height", g_pMaterialSystemConfig->m_VideoMode.m_Height );
	dataToUpload->SetInt( "AASamples", g_pMaterialSystemConfig->m_nAASamples );
	dataToUpload->SetInt( "AAQuality", g_pMaterialSystemConfig->m_nAAQuality );
	dataToUpload->SetBool( "TripleBuffered", g_pMaterialSystemConfig->m_bWantTripleBuffered );
	dataToUpload->SetInt( "AnisoLevel", g_pMaterialSystemConfig->m_nForceAnisotropicLevel );
	dataToUpload->SetInt( "SkipMipLevels", g_pMaterialSystemConfig->skipMipLevels );
	dataToUpload->SetInt( "DXLevel", g_pMaterialSystemConfig->dxSupportLevel );
	dataToUpload->SetBool( "ShadowDepthTexture", g_pMaterialSystemConfig->ShadowDepthTexture() );
	dataToUpload->SetBool( "MotionBlur", g_pMaterialSystemConfig->MotionBlur() );
	dataToUpload->SetBool( "Windowed", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_WINDOWED) ? true : false );
	dataToUpload->SetBool( "NoWaitForVSync", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC) ? true : false );
	dataToUpload->SetBool( "DisableSpecular", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR) ? true : false );
	dataToUpload->SetBool( "DisableBumpmapping", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_DISABLE_BUMPMAP) ? true : false );
	dataToUpload->SetBool( "EnableParallaxMapping", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING) ? true : false );
	dataToUpload->SetBool( "ZPrefill", (g_pMaterialSystemConfig->m_Flags & MATSYS_VIDCFG_FLAGS_USE_Z_PREFILL) ? 1 : 0 );
	dataToUpload->SetBool( "RenderToTextureShadows", r_shadowrendertotexture.GetBool() );
	dataToUpload->SetBool( "FlashlightDepthTexture", r_flashlightdepthtexture.GetBool() );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Dumps the specified config info to the console
//-----------------------------------------------------------------------------
void PrintMaterialSystemConfig( const MaterialSystem_Config_t &config )
{
	Warning( "width: %d\n", config.m_VideoMode.m_Width );
	Warning( "height: %d\n", config.m_VideoMode.m_Height );
	Warning( "m_nForceAnisotropicLevel: %d\n", config.m_nForceAnisotropicLevel );
	Warning( "aasamples: %d\n", config.m_nAASamples );
	Warning( "aaquality: %d\n", config.m_nAAQuality );
	Warning( "tripleBuffered: %s\n", config.m_bWantTripleBuffered ? "true" : "false" );

	Warning( "skipMipLevels: %d\n", config.skipMipLevels );
	Warning( "dxSupportLevel: %d\n", config.dxSupportLevel );
	Warning( "monitorGamma: %f\n", config.m_fMonitorGamma );
	Warning( "MATSYS_VIDCFG_FLAGS_UNSUPPORTED: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_UNSUPPORTED ) ? "true" : "false" );
	Warning( "MATSYS_VIDCFG_FLAGS_WINDOWED: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_WINDOWED ) ? "true" : "false" );
	Warning( "MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR ) ? "true" : "false" );
	Warning( "MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING ) ? "true" : "false" );
	Warning( "MATSYS_VIDCFG_FLAGS_USE_Z_PREFILL: %s\n", ( config.m_Flags & MATSYS_VIDCFG_FLAGS_USE_Z_PREFILL ) ? "true" : "false" );
	Warning( "r_shadowrendertotexture: %s\n", r_shadowrendertotexture.GetBool() ? "true" : "false" );
	Warning( "motionblur: %s\n", config.m_bMotionBlur ? "true" : "false" );
	Warning( "shadowdepthtexture: %s\n", config.m_bShadowDepthTexture ? "true" : "false" );
	Warning( "CSM Quality Level: %u\n", config.m_nCSMQuality );
}

CON_COMMAND( mat_configcurrent, "show the current video control panel config for the material system" )
{
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
	PrintMaterialSystemConfig( config );
}

#if !defined(DEDICATED) && !defined( _X360 )
CON_COMMAND( mat_setvideomode, "sets the width, height, windowed state of the material system" )
{
	if ( args.ArgC() < 4 )
		return;

	int nWidth = Q_atoi( args[1] );
	int nHeight = Q_atoi( args[2] );
	bool bWindowed = Q_atoi( args[3] ) > 0 ? true : false;
	bool bNoBorder = videomode->NoWindowBorder();
	
	if ( args.ArgC() >= 5 )
	{
		bNoBorder = Q_atoi( args[4] ) > 0 ? true : false;
	}

	videomode->SetMode( nWidth, nHeight, bWindowed, bNoBorder );
}
#endif

CON_COMMAND( mat_savechanges, "saves current video configuration to the registry" )
{
	// if the user has got to the point where they can adjust and apply video changes, then we can clear safe mode
	CommandLine()->RemoveParm( "-safe" );

	// write out config
	UpdateMaterialSystemConfig();
	WriteMaterialSystemConfigToRegistry( *g_pMaterialSystemConfig );
}

#if !defined( _GAMECONSOLE ) && !defined( DEDICATED )
CON_COMMAND( mat_updateconvars, "updates the video config convars" )
{
	UpdateVideoConfigConVars();
}
#endif

// Players have been using mat_debug to cheat in CS.
bool g_bHasIssuedMatSuppressOrDebug;

//-----------------------------------------------------------------------------
// A console command to debug materials
//-----------------------------------------------------------------------------
CON_COMMAND_F( mat_debug, "Activates debugging spew for a specific material.", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg ("usage: mat_debug [ <material name> ]\n");
		return;
	}

	materials->ToggleDebugMaterial( args[1] );
	
	g_bHasIssuedMatSuppressOrDebug = true;
}

// Players have been using mat_suppress to cheat
//-----------------------------------------------------------------------------
// A console command to suppress materials
//-----------------------------------------------------------------------------
CON_COMMAND_F( mat_suppress, "Suppress a material from drawing", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg ("usage: mat_suppress [ <material name> ]\n");
		return;
	}

	materials->ToggleSuppressMaterial( args[1] );
	
	g_bHasIssuedMatSuppressOrDebug = true;
}

static ITexture *CreatePowerOfTwoFBTexture( void )
{
	if ( IsX360() )
		return NULL;

	return materials->CreateNamedRenderTargetTextureEx2( 
		"_rt_PowerOfTwoFB",
		1024, 1024, RT_SIZE_DEFAULT,
		// Has dest alpha for vort warp effect
		IMAGE_FORMAT_RGBA8888,
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
}

static ITexture *CreateWaterReflectionTexture( void )
{
	int iSize = CommandLine()->ParmValue( "-reflectionTextureSize", 1024 );
	return materials->CreateNamedRenderTargetTextureEx2(
		"_rt_WaterReflection",
		iSize, iSize, RT_SIZE_PICMIP,
		materials->GetBackBufferFormat(), 
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
}

static ITexture *CreateWaterRefractionTexture( void )
{
	int iSize = CommandLine()->ParmValue( "-reflectionTextureSize", 1024 );
	return materials->CreateNamedRenderTargetTextureEx2(
		"_rt_WaterRefraction",
		iSize, iSize, RT_SIZE_PICMIP,
		// This is different than reflection because it has to have alpha for fog factor.
		IMAGE_FORMAT_RGBA8888, 
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR );
}

static ITexture *CreateCameraTexture( void )
{
	int iSize = CommandLine()->ParmValue( "-monitorTextureSize", 256 );
	return materials->CreateNamedRenderTargetTextureEx2(
		"_rt_Camera",
		iSize, iSize, RT_SIZE_DEFAULT,
		materials->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, 
		0,
		CREATERENDERTARGETFLAGS_HDR );
}

static ITexture *CreateBuildCubemaps16BitTexture( void )
{
	return materials->CreateNamedRenderTargetTextureEx2(
		"_rt_BuildCubemaps16bit",
		0, 0, 
		RT_SIZE_FULL_FRAME_BUFFER,
		IMAGE_FORMAT_RGBA16161616, 
		MATERIAL_RT_DEPTH_SHARED );
}

static ITexture *CreateQuarterSizedFBTexture( int n, unsigned int iRenderTargetFlags )
{
	char nbuf[20];
	sprintf(nbuf,"_rt_SmallFB%d",n);

	ImageFormat fmt=materials->GetBackBufferFormat();
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
		fmt = IMAGE_FORMAT_RGBA16161616F;

	return materials->CreateNamedRenderTargetTextureEx2(
		nbuf, 0, 0, RT_SIZE_HDR,
		fmt, MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT, 
		iRenderTargetFlags );
}

static ITexture *CreateTeenyFBTexture( int n )
{
	char nbuf[20];
	sprintf(nbuf,"_rt_TeenyFB%d",n);

	ImageFormat fmt = materials->GetBackBufferFormat();
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
		fmt = IMAGE_FORMAT_RGBA16161616F;

	return materials->CreateNamedRenderTargetTextureEx2(
		nbuf, 32, 32, RT_SIZE_DEFAULT,
		fmt, MATERIAL_RT_DEPTH_SHARED );
}

static ITexture *CreateFullFrameFBTexture( int textureIndex, int iExtraFlags = 0 )
{
	char textureName[256];

	if ( textureIndex > 0 )
	{
		sprintf( textureName, "_rt_FullFrameFB%d", textureIndex );
	}
	else
	{
		Q_strcpy( textureName, "_rt_FullFrameFB" );
	}

	int rtFlags = iExtraFlags | CREATERENDERTARGETFLAGS_HDR;
	if ( IsX360() )
	{
		// just make the system memory texture only
		rtFlags |= CREATERENDERTARGETFLAGS_NOEDRAM;
	}
	return materials->CreateNamedRenderTargetTextureEx2(
		textureName,
		1, 1, RT_SIZE_FULL_FRAME_BUFFER, materials->GetBackBufferFormat(), 
		MATERIAL_RT_DEPTH_SHARED, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		rtFlags );
}

static ITexture *CreateFullFrameDepthTexture( void )
{
	if ( IsGameConsole() )
	{
		return materials->CreateNamedRenderTargetTextureEx2( "_rt_FullFrameDepth", 1, 1, 
			RT_SIZE_FULL_FRAME_BUFFER, g_pMaterialSystemHardwareConfig->GetShadowDepthTextureFormat(), MATERIAL_RT_DEPTH_NONE,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
			CREATERENDERTARGETFLAGS_NOEDRAM );

	}
	else
	{
		if ( g_pMaterialSystemHardwareConfig->SupportsResolveDepth() )
		{
			if ( IsPlatformOpenGL() )
			{
				return materials->CreateNamedRenderTargetTextureEx2( "_rt_FullFrameDepth", 1, 1,
																	 RT_SIZE_FULL_FRAME_BUFFER, IMAGE_FORMAT_D24S8, MATERIAL_RT_DEPTH_ONLY,
																	 TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
																	 CREATERENDERTARGETFLAGS_NOEDRAM );
			}
			else
			{
				return materials->CreateNamedRenderTargetTextureEx2( "_rt_FullFrameDepth", 1, 1,
																	 RT_SIZE_FULL_FRAME_BUFFER, IMAGE_FORMAT_INTZ, MATERIAL_RT_DEPTH_NONE,
																	 TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
																	 CREATERENDERTARGETFLAGS_NOEDRAM );
			}
		}

		else
		{
 			return materials->CreateNamedRenderTargetTextureEx2( "_rt_FullFrameDepth", 1, 1,
 																 RT_SIZE_FULL_FRAME_BUFFER, IMAGE_FORMAT_R32F, MATERIAL_RT_DEPTH_SHARED, 
 																 TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
 																 CREATERENDERTARGETFLAGS_NOEDRAM );
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Create render targets which mods rely upon to render correctly
//-----------------------------------------------------------------------------
void InitWellKnownRenderTargets( void )
{
#if !defined( DEDICATED )

	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: InitWellKnownRenderTargets\n" );
	}

	// Begin block in which all render targets should be allocated
	materials->BeginRenderTargetAllocation();

	// Create the render targets upon which mods may rely

	if ( IsPC() )
	{
		// Create for all mods as vgui2 uses it for 3D painting
		g_PowerOfTwoFBTexture.Init( CreatePowerOfTwoFBTexture() );
	}

	// Create these for all mods because the engine references them
	if ( IsPC() && g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
	{
		// Used for building HDR Cubemaps
		g_BuildCubemaps16BitTexture.Init( CreateBuildCubemaps16BitTexture() );
	}

	// Used in Bloom effects
	g_QuarterSizedFBTexture0.Init( CreateQuarterSizedFBTexture( 0, 0 ) );

	/*
	// Commenting out this texture aliasing because it breaks the paint screenspace effect in Portal 2.
	if( IsX360() )
	materials->AddTextureAlias( "_rt_SmallFB1", "_rt_SmallFB0" ); //an alias is good enough on the 360 since we don't have a texture lock problem during post processing
	else
	g_QuarterSizedFBTexture1.Init( CreateQuarterSizedFBTexture( 1, 0 ) );			
	*/
	g_QuarterSizedFBTexture1.Init( CreateQuarterSizedFBTexture( 1, 0 ) );
#if ! ( defined( LEFT4DEAD ) || defined( CSTRIKE15 ) )
	g_QuarterSizedFBTexture2.Init( CreateQuarterSizedFBTexture( 2, 0 ) );
	g_QuarterSizedFBTexture3.Init( CreateQuarterSizedFBTexture( 3, 0 ) );			
#endif


#if defined( _X360 )
	g_RtGlowTexture360.InitRenderTargetTexture( 8, 8, RT_SIZE_NO_CHANGE, IMAGE_FORMAT_ARGB8888, MATERIAL_RT_DEPTH_NONE, false, "_rt_Glows360" );

	// NOTE: The 360 requires render targets generated with 1xMSAA to be 80x16 aligned in EDRAM
	//       Using 1120x624 since this seems to be the largest surface we can fit in EDRAM next to the back buffer
	g_RtGlowTexture360.InitRenderTargetSurface( 1120, 624, IMAGE_FORMAT_ARGB8888, false );
#endif

	if ( IsPC() )
	{
		g_TeenyFBTexture0.Init( CreateTeenyFBTexture( 0 ) );
		g_TeenyFBTexture1.Init( CreateTeenyFBTexture( 1 ) );
		g_TeenyFBTexture2.Init( CreateTeenyFBTexture( 2 ) );
	}

#ifdef _PS3
	g_FullFrameRawBufferAliasPS3_BackBuffer.Init(
		materials->CreateNamedRenderTargetTextureEx2(
		"^PS3^BACKBUFFER",
		1, 1, RT_SIZE_FULL_FRAME_BUFFER,
		materials->GetBackBufferFormat(), 
		MATERIAL_RT_DEPTH_SHARED,
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		CREATERENDERTARGETFLAGS_HDR ) );
	g_FullFrameRawBufferAliasPS3_DepthBuffer.Init(
		materials->CreateNamedRenderTargetTextureEx2(
		"^PS3^DEPTHBUFFER",
		1, 1, RT_SIZE_FULL_FRAME_BUFFER,
		g_pMaterialSystemHardwareConfig->GetShadowDepthTextureFormat(),
		MATERIAL_RT_DEPTH_NONE,
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
		CREATERENDERTARGETFLAGS_NOEDRAM ) );
#endif

	g_FullFrameFBTexture0.Init( CreateFullFrameFBTexture( 0 ) );

	// Since the tools may not draw the world, we don't want depth buffer effects
	if ( toolframework->InToolMode() )
	{
		mat_resolveFullFrameDepth.SetValue( 0 );
	}

#if defined( LEFT4DEAD )
	if ( IsPC() )	
	{
		g_FullFrameFBTexture1.Init( CreateFullFrameFBTexture( 1 ) );	// save some memory on the 360
	}
#else

	g_FullFrameFBTexture1.Init( CreateFullFrameFBTexture( 1, CREATERENDERTARGETFLAGS_TEMP ) );

#endif

#ifndef _PS3
	g_FullFrameDepth.Init( CreateFullFrameDepthTexture() );
#endif

	// Allow the client to init their own mod-specific render targets
	if ( g_pClientRenderTargets )
	{
		g_pClientRenderTargets->InitClientRenderTargets( materials, g_pMaterialSystemHardwareConfig );
	}
	else
	{
		// If this mod doesn't define the interface, fallback to initializing the standard render textures 
		// NOTE: these should match up with the 'Get' functions in cl_dll/rendertexture.h/cpp
		g_WaterReflectionTexture.Init( CreateWaterReflectionTexture() );
		g_WaterRefractionTexture.Init( CreateWaterRefractionTexture() );
		g_CameraTexture.Init( CreateCameraTexture() );
	}

	// End block in which all render targets should be allocated (kicking off an Alt-Tab type behavior)
	materials->EndRenderTargetAllocation();

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetNonInteractiveTempFullscreenBuffer( g_FullFrameFBTexture0, MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Shut down the render targets which mods rely upon to render correctly
//-----------------------------------------------------------------------------
void ShutdownWellKnownRenderTargets( void )
{
#if !defined( DEDICATED )
	if ( IsX360() )
	{
		// cannot allowing RT's to reconstruct, causes other fatal problems
		// many other 360 systems have been coded with this expected constraint
		Assert( 0 );
		return;
	}

	if ( IsPC() && mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: ShutdownWellKnownRenderTargets\n" );
	}

	g_PowerOfTwoFBTexture.Shutdown();
	g_BuildCubemaps16BitTexture.Shutdown();
		
	g_QuarterSizedFBTexture0.Shutdown();
	
	if( IsX360() )
		materials->RemoveTextureAlias( "_rt_SmallFB1" );
	else
		g_QuarterSizedFBTexture1.Shutdown();

	g_QuarterSizedFBTexture2.Shutdown();
	g_QuarterSizedFBTexture3.Shutdown();

	#if defined( _X360 )
		g_RtGlowTexture360.Shutdown();
	#endif

	g_TeenyFBTexture0.Shutdown();
	g_TeenyFBTexture1.Shutdown();
	g_TeenyFBTexture2.Shutdown();
	g_FullFrameFBTexture0.Shutdown();
	g_FullFrameFBTexture1.Shutdown();
	if ( IsX360() )
	{
		g_FullFrameFBTexture2.Shutdown();
	}
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetNonInteractiveTempFullscreenBuffer( NULL, MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD );

	g_FullFrameDepth.Shutdown();

	// Shutdown client render targets
	if ( g_pClientRenderTargets )
	{
		g_pClientRenderTargets->ShutdownClientRenderTargets();
	}
	else
	{
		g_WaterReflectionTexture.Shutdown();
		g_WaterRefractionTexture.Shutdown();
		g_CameraTexture.Shutdown();
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Make the debug system materials
//-----------------------------------------------------------------------------
static void InitDebugMaterials( void )
{
	if ( IsPC() && mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: InitDebugMaterials\n" );
	}

	g_materialEmpty = GL_LoadMaterial( "debug/debugempty", TEXTURE_GROUP_OTHER );
#ifndef DEDICATED
	g_materialWireframe = GL_LoadMaterial( "debug/debugwireframe", TEXTURE_GROUP_OTHER );
	g_materialTranslucentSingleColor = GL_LoadMaterial( "debug/debugtranslucentsinglecolor", TEXTURE_GROUP_OTHER );
	g_materialTranslucentVertexColor = GL_LoadMaterial( "debug/debugtranslucentvertexcolor", TEXTURE_GROUP_OTHER );
	g_materialWorldWireframe = GL_LoadMaterial( "debug/debugworldwireframe", TEXTURE_GROUP_OTHER );
	g_materialWorldWireframeZBuffer = GL_LoadMaterial( "debug/debugworldwireframezbuffer", TEXTURE_GROUP_OTHER );
	g_materialWorldWireframeGreen = GL_LoadMaterial( "debug/debugworldwireframegreen", TEXTURE_GROUP_OTHER );

	g_materialBrushWireframe = GL_LoadMaterial( "debug/debugbrushwireframe", TEXTURE_GROUP_OTHER );
	g_materialDecalWireframe = GL_LoadMaterial( "debug/debugdecalwireframe", TEXTURE_GROUP_OTHER );
	g_materialDebugLightmap = GL_LoadMaterial( "debug/debuglightmap", TEXTURE_GROUP_OTHER );
	g_materialDebugLightmapZBuffer = GL_LoadMaterial( "debug/debuglightmapzbuffer", TEXTURE_GROUP_OTHER );
	g_materialDebugLuxels = GL_LoadMaterial( "debug/debugluxels", TEXTURE_GROUP_OTHER );

	g_materialLeafVisWireframe = GL_LoadMaterial( "debug/debugleafviswireframe", TEXTURE_GROUP_OTHER );
	g_pMaterialWireframeVertexColor = GL_LoadMaterial( "debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER );
	g_pMaterialWireframeVertexColorIgnoreZ = GL_LoadMaterial( "debug/debugwireframevertexcolorignorez", TEXTURE_GROUP_OTHER );
	g_pMaterialLightSprite = GL_LoadMaterial( "engine/lightsprite", TEXTURE_GROUP_OTHER );
	g_pMaterialShadowBuild = GL_LoadMaterial( "engine/shadowbuild", TEXTURE_GROUP_OTHER);
	g_pMaterialMRMWireframe = GL_LoadMaterial( "debug/debugmrmwireframe", TEXTURE_GROUP_OTHER );
	g_pMaterialDebugFlat = GL_LoadMaterial( "debug/debugdrawflattriangles", TEXTURE_GROUP_OTHER );

	g_pMaterialAmbientCube = GL_LoadMaterial( "debug/debugambientcube", TEXTURE_GROUP_OTHER );

	g_pMaterialWriteZ = GL_LoadMaterial( "engine/writez", TEXTURE_GROUP_OTHER );

	// Materials for writing to shadow depth buffer
	KeyValues *pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$color_depth", 0 );
	g_pMaterialDepthWrite[0][0] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite00", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	g_pMaterialDepthWrite[0][0]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	g_pMaterialDepthWrite[0][1] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite01", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	g_pMaterialDepthWrite[0][1]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	g_pMaterialDepthWrite[1][0] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite10", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	g_pMaterialDepthWrite[1][0]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt("$color_depth", 0);
	g_pMaterialDepthWrite[1][1] = g_pMaterialSystem->FindProceduralMaterial("__DepthWrite11", TEXTURE_GROUP_OTHER, pVMTKeyValues);
	g_pMaterialDepthWrite[1][1]->IncrementReferenceCount();


	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	g_pMaterialSSAODepthWrite[ 0 ][ 0 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite00", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	g_pMaterialSSAODepthWrite[ 0 ][ 0 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 0 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	g_pMaterialSSAODepthWrite[ 0 ][ 1 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite01", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	g_pMaterialSSAODepthWrite[ 0 ][ 1 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 0 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	g_pMaterialSSAODepthWrite[ 1 ][ 0 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite10", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	g_pMaterialSSAODepthWrite[ 1 ][ 0 ]->IncrementReferenceCount();

	pVMTKeyValues = new KeyValues( "DepthWrite" );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$alphatest", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$color_depth", 1 );
	g_pMaterialSSAODepthWrite[ 1 ][ 1 ] = g_pMaterialSystem->FindProceduralMaterial( "__ColorDepthWrite11", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	g_pMaterialSSAODepthWrite[ 1 ][ 1 ]->IncrementReferenceCount();



#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void ShutdownDebugMaterials( void )
{
	if ( IsPC() && mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: ShutdownDebugMaterials\n" );
	}

	GL_UnloadMaterial( g_materialEmpty );
#ifndef DEDICATED
	GL_UnloadMaterial( g_pMaterialLightSprite );
	GL_UnloadMaterial( g_pMaterialWireframeVertexColor );
	GL_UnloadMaterial( g_pMaterialWireframeVertexColorIgnoreZ );
	GL_UnloadMaterial( g_materialLeafVisWireframe );

	GL_UnloadMaterial( g_materialDebugLuxels );
	GL_UnloadMaterial( g_materialDebugLightmapZBuffer );
	GL_UnloadMaterial( g_materialDebugLightmap );
	GL_UnloadMaterial( g_materialDecalWireframe );
	GL_UnloadMaterial( g_materialBrushWireframe );

	GL_UnloadMaterial( g_materialWorldWireframeGreen );
	GL_UnloadMaterial( g_materialWorldWireframeZBuffer );
	GL_UnloadMaterial( g_materialWorldWireframe );
	GL_UnloadMaterial( g_materialTranslucentSingleColor );
	GL_UnloadMaterial( g_materialTranslucentVertexColor );
	GL_UnloadMaterial( g_materialWireframe );
	GL_UnloadMaterial( g_pMaterialShadowBuild );
	GL_UnloadMaterial( g_pMaterialMRMWireframe );
	GL_UnloadMaterial( g_pMaterialWriteZ );

	GL_UnloadMaterial( g_pMaterialAmbientCube );
	GL_UnloadMaterial( g_pMaterialDebugFlat );

	// Materials for writing to shadow depth buffer
	for (int i = 0; i<2; i++)
	{
		for (int j = 0; j<2; j++)
		{
			if( g_pMaterialDepthWrite[i][j] )
			{
				g_pMaterialDepthWrite[i][j]->DecrementReferenceCount();
			}
			g_pMaterialDepthWrite[i][j] = NULL;

			if ( g_pMaterialSSAODepthWrite[ i ][ j ] )
			{
				g_pMaterialSSAODepthWrite[ i ][ j ]->DecrementReferenceCount();
			}
			g_pMaterialSSAODepthWrite[ i ][ j ] = NULL;
		}


	}
#endif
}


//-----------------------------------------------------------------------------
// Used to deal with making sure Present is called often enough 
//-----------------------------------------------------------------------------
void InitStartupScreen()
{
	if ( !IsGameConsole() )
		return;

	int screenWidth, screenHeight;
	materials->GetBackBufferDimensions( screenWidth, screenHeight );

	// NOTE: Brutal hackery, this code is duplicated in gameui.dll, and a bunch of other places
	// but I have to do this prior to gameui being loaded.
	char filename[MAX_PATH];
	CL_GetStartupImage( filename, sizeof( filename ) );

	ITexture *pTexture = materials->FindTexture( filename, TEXTURE_GROUP_OTHER );
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetNonInteractiveTempFullscreenBuffer( pTexture, MATERIAL_NON_INTERACTIVE_MODE_STARTUP );

#if defined( CSTRIKE15 )
	pTexture = materials->FindTexture( "console/spinner", TEXTURE_GROUP_OTHER );
	pRenderContext->SetNonInteractivePacifierTexture( pTexture, 0.5f, 0.5f, 0.2f );

	const int logoWidth = 896;
	const int logoHeight = 64;

	float x, y, w, h;
	w = vgui::scheme()->GetProportionalScaledValue( logoWidth ) / 2;
	h = vgui::scheme()->GetProportionalScaledValue( logoHeight ) / 2;

	x = (screenWidth / 2 - w / 2 ) / (float)screenWidth;
	y = (screenHeight / 2 - h / 2) / (float)screenHeight;
	w = (w / (float)screenWidth);
	h = (h / (float)screenHeight);

	pTexture = materials->FindTexture( "console/logo", TEXTURE_GROUP_OTHER );
	pRenderContext->SetNonInteractiveLogoTexture( pTexture, x, y, w, h );
#else
	// this is what the loading progress calcs
	float x, y, w, h, size;
	size = vgui::scheme()->GetProportionalScaledValue( 85 );
	x = screenWidth - vgui::scheme()->GetProportionalScaledValue( 85 );
	y = vgui::scheme()->GetProportionalScaledValue( 70 );

	// back solve to send in to achieve the above placement
	x = x /(float)screenWidth;
	y = y /(float)screenHeight;
	size = size/(float)screenHeight;

	pTexture = materials->FindTexture( "vgui/spinner", TEXTURE_GROUP_OTHER );
	pRenderContext->SetNonInteractivePacifierTexture( pTexture, x, y, size );

	y = vgui::scheme()->GetProportionalScaledValue( 390 );
	w = vgui::scheme()->GetProportionalScaledValue( 240 );
	h = vgui::scheme()->GetProportionalScaledValue( 60 );

	// center align at bottom
	x = (screenWidth/2 - w/2)/(float)screenWidth;
	y = y/(float)screenHeight;
	w = w/(float)screenWidth;
	h = h/(float)screenHeight;

	pTexture = materials->FindTexture( "vgui/portal2logo", TEXTURE_GROUP_OTHER );
	pRenderContext->SetNonInteractiveLogoTexture( pTexture, x, y, w, h );
#endif // CSTRIKE15
}

//-----------------------------------------------------------------------------
// A console command to spew out driver information
//-----------------------------------------------------------------------------
CON_COMMAND( mat_info, "Shows material system info" )
{
	materials->SpewDriverInfo();
}

extern ConVar r_fastzreject;

void InitMaterialSystem( void )
{
	materials->AddReleaseFunc( ReleaseMaterialSystemObjects );
	materials->AddRestoreFunc( RestoreMaterialSystemObjects );

	r_fastzreject.SetValue( g_pMaterialSystemHardwareConfig->PreferZPrepass() );

	UpdateMaterialSystemConfig();

	InitWellKnownRenderTargets();

	InitDebugMaterials();
}

void ShutdownMaterialSystem( void )
{
	ShutdownDebugMaterials();

	ShutdownWellKnownRenderTargets();
}

//-----------------------------------------------------------------------------
// Methods to restore, release material system objects
//-----------------------------------------------------------------------------
void ReleaseMaterialSystemObjects( int nChangeFlags )
{
	if ( IsGameConsole() )
	{
		// 360 has not implemented release/restore
		Warning( "ReleaseMaterialSystemObjects(): not implemented for 360\n" );
		Assert( 0 );
		return;
	}

#ifndef DEDICATED
	DispInfo_ReleaseMaterialSystemObjects( host_state.worldmodel );

	modelrender->ReleaseAllStaticPropColorData();
#endif

#ifndef DEDICATED
	WorldStaticMeshDestroy();
#endif
	g_LostVideoMemory = true;
}

void RestoreMaterialSystemObjects( int nChangeFlags )
{
	if ( IsGameConsole() )
	{
		// 360 has not implemented release/restore
		Warning( "RestoreMaterialSystemObjects(): not implemented for 360\n" );
		Assert( 0 );
		return;
	}

	bool bThreadingAllowed = Host_AllowQueuedMaterialSystem( false );
	g_LostVideoMemory = false;

	if ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED )
	{
		// ensure decals have no stale references to invalid lods
		modelrender->RemoveAllDecalsFromAllModels( false );
	}

	if (host_state.worldmodel && materialSortInfoArray)
	{
		if ( (nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED) || materials->GetNumSortIDs() == 0 )
		{
#ifndef DEDICATED
			// Reload lightmaps, world meshes, etc. because we may have switched from bumped to unbumped
			R_LoadWorldGeometry( true );
#endif
		}
		else
		{
			modelloader->Map_LoadDisplacements( host_state.worldmodel, true );
#ifndef DEDICATED
			WorldStaticMeshCreate();
			// Gotta recreate the lightmaps
			R_RedownloadAllLightmaps();
#endif
		}

#ifndef DEDICATED
		// Need to re-figure out the env_cubemaps, so blow away the lightcache.
		R_StudioInitLightingCache();
		modelrender->RestoreAllStaticPropColorData();

		// Make sure we update visibility flags for props (especially coming back from an Alt-Tab)
		StaticPropMgr()->RestoreStaticProps();
#endif
	}

#ifndef DEDICATED
	// Rebuild the overlay vertex buffer.
	OverlayMgr()->ReSortMaterials();
#endif

	Host_AllowQueuedMaterialSystem( bThreadingAllowed );
}

bool TangentSpaceSurfaceSetup( SurfaceHandle_t surfID, Vector &tVect )
{
	Vector sVect;
	VectorCopy( MSurf_TexInfo( surfID )->textureVecsTexelsPerWorldUnits[0].AsVector3D(), sVect );
	VectorCopy( MSurf_TexInfo( surfID )->textureVecsTexelsPerWorldUnits[1].AsVector3D(), tVect );
	VectorNormalize( sVect );
	VectorNormalize( tVect );
	Vector tmpVect;
	CrossProduct( sVect, tVect, tmpVect );
	// Make sure that the tangent space works if textures are mapped "backwards".
	if( DotProduct( MSurf_Plane( surfID ).normal, tmpVect ) > 0.0f )
	{
		return true;
	}
	return false;
}

void TangentSpaceComputeBasis( Vector& tangentS, Vector& tangentT, const Vector& normal, const Vector& tVect, bool negateTangent )
{
	// tangent x binormal = normal
	// tangent = sVect
	// binormal = tVect
	CrossProduct( normal, tVect, tangentS );
	VectorNormalize( tangentS );
	CrossProduct( tangentS, normal, tangentT );
	VectorNormalize( tangentT );

	if ( negateTangent )
	{
		VectorScale( tangentS, -1.0f, tangentS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MaterialSystem_DestroySortinfo( void )
{
	if ( materialSortInfoArray )
	{
#ifndef DEDICATED
		WorldStaticMeshDestroy();
#endif
		delete[] materialSortInfoArray;
		materialSortInfoArray = NULL;
	}
}


#ifndef DEDICATED

//-----------------------------------------------------------------------------
// Purpose: Build a vertex buffer for this face
// Input  : *pWorld - world model base
//			*surf - surf to add to the mesh
//			overbright - overbright factor (for colors)
//			&builder - mesh that holds the vertex buffer
//-----------------------------------------------------------------------------
#ifdef NEWMESH
void BuildMSurfaceVertexArrays( worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, CVertexBufferBuilder &builder )
{
	SurfaceCtx_t ctx;
	SurfSetupSurfaceContext( ctx, surfID );

	byte flatColor[4] = { 255, 255, 255, 255 };

	Vector tVect;
	bool negate = false;
	if ( MSurf_Flags( surfID ) & SURFDRAW_TANGENTSPACE )
	{
		negate = TangentSpaceSurfaceSetup( surfID, tVect );
	}

	for ( int i = 0; i < MSurf_VertCount( surfID ); i++ )
	{
		int vertIndex = pBrushData->vertindices[MSurf_FirstVertIndex( surfID ) + i];

		// world-space vertex
		Vector& vec = pBrushData->vertexes[vertIndex].position;

		// output to mesh
		builder.Position3fv( vec.Base() );

		Vector2D uv;
		SurfComputeTextureCoordinate( surfID, vec, uv.Base() );
		builder.TexCoord2fv( 0, uv.Base() );

		// garymct: normalized (within space of surface) lightmap texture coordinates
		SurfComputeLightmapCoordinate( ctx, surfID, vec, uv );
		builder.TexCoord2fv( 1, uv.Base() );

		if ( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
		{
			// bump maps appear left to right in lightmap page memory, calculate 
			// the offset for the width of a single map. The pixel shader will use 
			// this to compute the actual texture coordinates
			builder.TexCoord2f( 2, ctx.m_BumpSTexCoordOffset, 0.0f );
		}
		else
		{
			// PORTAL 2 FIX - paint shader assumes it can use 3 lightmapped coordinates in all cases, so set the offset to something reasonable
			builder.TexCoord2f( 2, 0.0f, 0.0f );
		}

		Vector& normal = pBrushData->vertnormals[ pBrushData->vertnormalindices[MSurf_FirstVertNormal( surfID ) + i] ];
		builder.Normal3fv( normal.Base() );

		if ( MSurf_Flags( surfID ) & SURFDRAW_TANGENTSPACE )
		{
			Vector tangentS, tangentT;
			TangentSpaceComputeBasis( tangentS, tangentT, normal, tVect, negate );
			builder.TangentS3fv( tangentS.Base() );
			builder.TangentT3fv( tangentT.Base() );
		}

		// The amount to blend between basetexture and basetexture2 used to sit in lightmap
		// alpha, so we didn't care about the vertex color or vertex alpha. But now if they're
		// using it, we have to make sure the vertex has the color and alpha specified correctly
		// or it will look weird.
		if ( !SurfaceHasDispInfo( surfID ) && 
			 (MSurf_TexInfo( surfID )->texinfoFlags & TEXINFO_USING_BASETEXTURE2) )
		{
			static bool bWarned = false;
			if ( !bWarned )
			{
				const char *pMaterialName = MSurf_TexInfo( surfID )->material->GetName();
				bWarned = true;
				Warning( "Warning: WorldTwoTextureBlend found on a non-displacement surface (material: %s). This wastes perf for no benefit.\n", pMaterialName );
			}

			builder.Color4ub( 255, 255, 255, 0 );
		}
		else
		{
			builder.Color3ubv( flatColor );
		}
		
		builder.AdvanceVertex();
	}
}
#else
//-----------------------------------------------------------------------------
// Purpose: Build a vertex buffer for this face
// Input  : *pWorld - world model base
//			*surf - surf to add to the mesh
//			&builder - mesh that holds the vertex buffer
//-----------------------------------------------------------------------------
static byte flatColor[4] = { 255, 255, 255, 255 };
static byte flatColorNoAlpha[4] = { 255, 255, 255, 0 };

void BuildMSurfaceVertexArrays( worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, CMeshBuilder &builder )
{
	SurfaceCtx_t ctx;
	SurfSetupSurfaceContext( ctx, surfID );

	Vector tVect;
	bool negate = false;
	if ( MSurf_Flags( surfID ) & SURFDRAW_TANGENTSPACE )
	{
		negate = TangentSpaceSurfaceSetup( surfID, tVect );
	}

	int vertCount = MSurf_VertCount(surfID);
	for ( int i = 0; i < vertCount; i++ )
	{
		int vertIndex = pBrushData->vertindices[MSurf_FirstVertIndex( surfID ) + i];

		// world-space vertex
		Vector& vec = pBrushData->vertexes[vertIndex].position;

		// output to mesh
		builder.Position3fv( vec.Base() );

		Vector2D uv;
		SurfComputeTextureCoordinate( surfID, vec, uv.Base() );
		builder.TexCoord2fv( 0, uv.Base() );

		// garymct: normalized (within space of surface) lightmap texture coordinates
		SurfComputeLightmapCoordinate( ctx, surfID, vec, uv );
		builder.TexCoord2fv( 1, uv.Base() );

		if ( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
		{
			// bump maps appear left to right in lightmap page memory, calculate 
			// the offset for the width of a single map. The pixel shader will use 
			// this to compute the actual texture coordinates

			if ( uv.x + ctx.m_BumpSTexCoordOffset*3 > 1.00001f )
			{
				Assert(0);

				SurfComputeLightmapCoordinate( ctx, surfID, vec, uv );
			}
			builder.TexCoord2f( 2, ctx.m_BumpSTexCoordOffset, 0.0f );
		}
		else
		{
			// PORTAL 2 FIX - paint shader assumes it can use 3 lightmapped coordinates in all cases, so set the offset to something reasonable
			builder.TexCoord2f( 2, 0.0f, 0.0f );
		}

		Vector& normal = pBrushData->vertnormals[ pBrushData->vertnormalindices[MSurf_FirstVertNormal( surfID ) + i] ];
		builder.Normal3fv( normal.Base() );

		if ( MSurf_Flags( surfID ) & SURFDRAW_TANGENTSPACE )
		{
			Vector tangentS, tangentT;
			TangentSpaceComputeBasis( tangentS, tangentT, normal, tVect, negate );
			builder.TangentS3fv( tangentS.Base() );
			builder.TangentT3fv( tangentT.Base() );
		}

		// The amount to blend between basetexture and basetexture2 used to sit in lightmap
		// alpha, so we didn't care about the vertex color or vertex alpha. But now if they're
		// using it, we have to make sure the vertex has the color and alpha specified correctly
		// or it will look weird.
		if ( !SurfaceHasDispInfo( surfID ) && (MSurf_TexInfo( surfID )->texinfoFlags & TEXINFO_USING_BASETEXTURE2) )
		{
			builder.Color4ubv( flatColorNoAlpha );
		}
		else
		{
			builder.Color4ubv( flatColor );
		}
		
		builder.AdvanceVertex();
	}
}
#endif // NEWMESH

static int VertexCountForSurfaceList( const CMSurfaceSortList &list, const surfacesortgroup_t &group )
{
	int vertexCount = 0;
	MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(list, group, surfID)
		vertexCount += MSurf_VertCount(surfID);
	MSL_FOREACH_SURFACE_IN_GROUP_END();
	return vertexCount;
}

//-----------------------------------------------------------------------------
// Builds a static mesh from a list of all surfaces with the same material
//-----------------------------------------------------------------------------

struct meshlist_t
{
#ifdef NEWMESH
	IVertexBuffer *pVertexBuffer;
#else
	IMesh *pMesh;
#endif
	IMaterial *pMaterial;
	int vertCount;
	VertexFormat_t vertexFormat;
};

static CUtlVector<meshlist_t> g_Meshes;

ConVar mat_max_worldmesh_vertices("mat_max_worldmesh_vertices", "65536");

static VertexFormat_t GetUncompressedFormat( const IMaterial * pMaterial )
{
	// FIXME: IMaterial::GetVertexFormat() should do this stripping (add a separate 'SupportsCompression' accessor)
	return ( pMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED );
}

int FindOrAddMesh( IMaterial *pMaterial, int vertexCount )
{
	VertexFormat_t format = GetUncompressedFormat( pMaterial );

	CMatRenderContextPtr pRenderContext( materials );

	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int worldLimit = mat_max_worldmesh_vertices.GetInt();
	worldLimit = MAX(worldLimit,1024);
	if ( nMaxVertices > worldLimit )
	{
		nMaxVertices = mat_max_worldmesh_vertices.GetInt();
	}

	for ( int i = 0; i < g_Meshes.Count(); i++ )
	{
		if ( g_Meshes[i].vertexFormat != format )
			continue;

		if ( g_Meshes[i].vertCount + vertexCount > nMaxVertices )
			continue;

		g_Meshes[i].vertCount += vertexCount;
		return i;
	}

	int index = g_Meshes.AddToTail();
	g_Meshes[index].vertCount = vertexCount;
	g_Meshes[index].vertexFormat = format;
	g_Meshes[index].pMaterial = pMaterial;

	return index;
}

void SetTexInfoBaseTexture2Flags()
{
	for ( int i=0; i < host_state.worldbrush->numtexinfo; i++ )
	{
		host_state.worldbrush->texinfo[i].texinfoFlags &= ~TEXINFO_USING_BASETEXTURE2;
	}
	
	for ( int i=0; i < host_state.worldbrush->numtexinfo; i++ )
	{
		mtexinfo_t *pTexInfo = &host_state.worldbrush->texinfo[i];
		IMaterial *pMaterial = pTexInfo->material;
		if ( !pMaterial )
			continue;

		IMaterialVar **pParms = pMaterial->GetShaderParams();
		int nParms = pMaterial->ShaderParamCount();
		for ( int i=0; i < nParms; i++ )
		{
			if ( !pParms[i]->IsDefined() )
				continue;

			if ( Q_stricmp( pParms[i]->GetName(), "$basetexture2" ) == 0 )
			{
				pTexInfo->texinfoFlags |= TEXINFO_USING_BASETEXTURE2;
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Determines vertex formats for all the world geometry
//-----------------------------------------------------------------------------
VertexFormat_t ComputeWorldStaticMeshVertexFormat( const IMaterial * pMaterial )
{
	VertexFormat_t vertexFormat = GetUncompressedFormat( pMaterial );

	// FIXME: set VERTEX_FORMAT_COMPRESSED if there are no artifacts and if it saves enough memory (use 'mem_dumpvballocs')
	// vertexFormat |= VERTEX_FORMAT_COMPRESSED;
	// FIXME: check for and strip unused vertex elements (TANGENT_S/T?)

	return vertexFormat;
}

//-----------------------------------------------------------------------------
// Builds static meshes for all the world geometry
//-----------------------------------------------------------------------------
void WorldStaticMeshCreate( void )
{
	r_framecount = 1;
	WorldStaticMeshDestroy();
	g_Meshes.RemoveAll();

	SetTexInfoBaseTexture2Flags();

	int nSortIDs = materials->GetNumSortIDs();
	if ( nSortIDs == 0 )
	{
		// this is probably a bug in alt-tab.  It's calling this as a restore function
		// but the lightmaps haven't been allocated yet
		Assert(0);
		return;
	}

	// Setup sortbins for flashlight rendering
	// FIXME!!!!  Could have less bins since we don't care about the lightmap
	// for projective light rendering purposes.
	// Not entirely true since we need the correct lightmap page for WorldVertexTransition materials.
	g_pShadowMgr->SetNumWorldMaterialBuckets( nSortIDs );

	Assert( !g_WorldStaticMeshes.Count() );
	g_WorldStaticMeshes.SetCount( nSortIDs );
	memset( g_WorldStaticMeshes.Base(), 0, sizeof(g_WorldStaticMeshes[0]) * g_WorldStaticMeshes.Count() );

	CMSurfaceSortList matSortArray;
	matSortArray.Init( nSortIDs, 512 );
	CUtlVector<int> sortIndex;
	sortIndex.SetCount( g_WorldStaticMeshes.Count() );
	CUtlVector<int> depthMeshIndexList;
	depthMeshIndexList.SetCount( g_WorldStaticMeshes.Count() );
	g_DepthMeshForSortID.SetCount( g_WorldStaticMeshes.Count() );
	extern bool g_bReplayLoadedTools;
	bool bTools = CommandLine()->CheckParm( "-tools" ) != NULL || g_bReplayLoadedTools;

	int i;
	// sort the surfaces into the sort arrays
	for( int surfaceIndex = 0; surfaceIndex < host_state.worldbrush->numsurfaces; surfaceIndex++ )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( surfaceIndex );
		// set these flags here as they are determined by material data
		MSurf_Flags( surfID ) &= ~(SURFDRAW_TANGENTSPACE);

		// do we need to compute tangent space here?
		if ( bTools || ( MSurf_TexInfo( surfID )->material->GetVertexFormat() & VERTEX_TANGENT_SPACE ) )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_TANGENTSPACE;
		}
		
		// don't create vertex buffers for nodraw faces, water faces, or faces with dynamic data
//		if ( (MSurf_Flags( surfID ) & (SURFDRAW_NODRAW|SURFDRAW_WATERSURFACE|SURFDRAW_DYNAMIC)) 
//			|| SurfaceHasDispInfo( surfID ) )
		if( SurfaceHasDispInfo( surfID ) )
		{
			MSurf_VertBufferIndex( surfID ) = 0xFFFF;
			continue;
		}

		// attach to head of list
		matSortArray.AddSurfaceToTail( surfID, 0, MSurf_MaterialSortID( surfID ) );
	}

	// iterate the arrays and create buffers
	for ( i = 0; i < g_WorldStaticMeshes.Count(); i++ )
	{
		const surfacesortgroup_t &group = matSortArray.GetGroupForSortID(0,i);
		int vertexCount = VertexCountForSurfaceList( matSortArray, group );

		SurfaceHandle_t surfID = matSortArray.GetSurfaceAtHead( group );
		g_WorldStaticMeshes[i] = NULL;
		sortIndex[i] = surfID ? FindOrAddMesh( MSurf_TexInfo( surfID )->material, vertexCount ) : -1;
		depthMeshIndexList[i] = surfID ? FindOrAddMesh( g_pMaterialDepthWrite[0][1], vertexCount ) : -1;
	}

	CMatRenderContextPtr pRenderContext( materials );

	PIXEVENT( pRenderContext, "WorldStaticMeshCreate()" );
#ifdef NEWMESH
	for ( i = 0; i < g_Meshes.Count(); i++ )
	{
		Assert( g_Meshes[i].vertCount > 0 );
		Assert( g_Meshes[i].pMaterial );
		g_Meshes[i].pVertexBuffer = pRenderContext->CreateStaticVertexBuffer( GetUncompressedFormat( g_Meshes[i].pMaterial ), g_Meshes[i].vertCount, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_WORLD );
		int vertBufferIndex = 0;
		// NOTE: Index count is zero because this will be a static vertex buffer!!!
		CVertexBufferBuilder vertexBufferBuilder;
		vertexBufferBuilder.Begin( g_Meshes[i].pVertexBuffer, g_Meshes[i].vertCount );
		for ( int j = 0; j < g_WorldStaticMeshes.Count(); j++ )
		{
			int meshId = sortIndex[j];
			if ( meshId == i )
			{
				g_WorldStaticMeshes[j] = g_Meshes[i].pVertexBuffer;
				const surfacesortgroup_t &group = matSortArray.GetGroupForSortID(0,j);
				MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(matSortArray, group, surfID);
					MSurf_VertBufferIndex( surfID ) = vertBufferIndex;
					BuildMSurfaceVertexArrays( host_state.worldbrush, surfID, vertexBufferBuilder );

					vertBufferIndex += MSurf_VertCount( surfID );

				MSL_FOREACH_SURFACE_IN_GROUP_END();
			}
		}
		vertexBufferBuilder.End();
		Assert(vertBufferIndex == g_Meshes[i].vertCount);
	}
#else
	g_DepthFillVBFirstVertexForSurface.SetCount( host_state.worldbrush->numsurfaces );
	for ( i = 0; i < g_Meshes.Count(); i++ )
	{
		Assert( g_Meshes[i].vertCount > 0 );
		if ( g_VBAllocTracker )
			g_VBAllocTracker->TrackMeshAllocations( "WorldStaticMeshCreate" );
		VertexFormat_t vertexFormat = ComputeWorldStaticMeshVertexFormat( g_Meshes[i].pMaterial );
		g_Meshes[i].pMesh = pRenderContext->CreateStaticMesh( vertexFormat, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_WORLD, g_Meshes[i].pMaterial );
		int vertBufferIndex = 0;
		// NOTE: Index count is zero because this will be a static vertex buffer!!!
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( g_Meshes[i].pMesh, MATERIAL_TRIANGLES, g_Meshes[i].vertCount, 0 );

		for ( int j = 0; j < g_WorldStaticMeshes.Count(); j++ )
		{
			int meshId = sortIndex[j];
			if ( meshId == i )
			{
				g_WorldStaticMeshes[j] = g_Meshes[i].pMesh;
				const surfacesortgroup_t &group = matSortArray.GetGroupForSortID(0,j);
				MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(matSortArray, group, surfID);

					MSurf_VertBufferIndex( surfID ) = vertBufferIndex;
					BuildMSurfaceVertexArrays( host_state.worldbrush, surfID, meshBuilder );

					vertBufferIndex += MSurf_VertCount( surfID );

				MSL_FOREACH_SURFACE_IN_GROUP_END();
			}
			if ( depthMeshIndexList[j] == i )
			{
				g_DepthMeshForSortID[j] = g_Meshes[i].pMesh;
				const surfacesortgroup_t &group = matSortArray.GetGroupForSortID(0,j);
				MSL_FOREACH_SURFACE_IN_GROUP_BEGIN(matSortArray, group, surfID);

					g_DepthFillVBFirstVertexForSurface[ MSurf_Index(surfID) ] = vertBufferIndex;
					BuildMSurfaceVertexArrays( host_state.worldbrush, surfID, meshBuilder );

					vertBufferIndex += MSurf_VertCount( surfID );

				MSL_FOREACH_SURFACE_IN_GROUP_END();
			}
		}
		meshBuilder.End();
		Assert(vertBufferIndex == g_Meshes[i].vertCount);
		if ( g_VBAllocTracker )
			g_VBAllocTracker->TrackMeshAllocations( NULL );
	}
#endif
	//Msg("Total %d meshes, %d before\n", g_Meshes.Count(), g_WorldStaticMeshes.Count() );
}

void WorldStaticMeshDestroy( void )
{
	CMatRenderContextPtr pRenderContext( materials );

	// Blat out the static meshes associated with each material
	for ( int i = 0; i < g_Meshes.Count(); i++ )
	{
#ifdef NEWMESH
		pRenderContext->DestroyVertexBuffer( g_Meshes[i].pVertexBuffer );
#else
		pRenderContext->DestroyStaticMesh( g_Meshes[i].pMesh );
#endif
	}
	g_WorldStaticMeshes.Purge();
	g_Meshes.RemoveAll();
	g_DepthMeshForSortID.RemoveAll();
}


//-----------------------------------------------------------------------------
// Compute texture and lightmap coordinates
//-----------------------------------------------------------------------------

void SurfComputeTextureCoordinate( SurfaceHandle_t surfID, Vector const& vec, float * RESTRICT pUV )
{
	mtexinfo_t* pTexInfo = MSurf_TexInfo( surfID );

	// base texture coordinate
	float u = DotProduct (vec, pTexInfo->textureVecsTexelsPerWorldUnits[0].AsVector3D()) + 
		pTexInfo->textureVecsTexelsPerWorldUnits[0][3];
	u /= pTexInfo->material->GetMappingWidth();

	float v = DotProduct (vec, pTexInfo->textureVecsTexelsPerWorldUnits[1].AsVector3D()) + 
		pTexInfo->textureVecsTexelsPerWorldUnits[1][3];
	v /= pTexInfo->material->GetMappingHeight();

	pUV[0] = u;
	pUV[1] = v;
}

#if _DEBUG
void CheckTexCoord( float coord )
{
	Assert(coord <= 1.0f );
}
#endif

void SurfComputeLightmapCoordinate( SurfaceCtx_t const& ctx, SurfaceHandle_t surfID, 
										 Vector const& vec, Vector2D& uv )
{
	if ( (MSurf_Flags( surfID ) & SURFDRAW_NOLIGHT) )
	{
		uv.x = uv.y = 0.5f;
	}
	else if ( MSurf_LightmapExtents( surfID )[0] == 0 )
	{
		uv = (0.5f * ctx.m_Scale + ctx.m_Offset);
	}
	else
	{
		mtexinfo_t* pTexInfo = MSurf_TexInfo( surfID );

		uv.x = DotProduct (vec, pTexInfo->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
			pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][3];
		uv.x -= MSurf_LightmapMins( surfID )[0];
		uv.x += 0.5f;

		uv.y = DotProduct (vec, pTexInfo->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
			pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][3];
		uv.y -= MSurf_LightmapMins( surfID )[1];
		uv.y += 0.5f;

		uv *= ctx.m_Scale;
		uv += ctx.m_Offset;

		assert( uv.IsValid() );
	}
#if _DEBUG
	// This was here for check against displacements and they actually get calculated later correctly.
//	CheckTexCoord( uv.x );
//	CheckTexCoord( uv.y );
#endif
	uv.x = clamp(uv.x, 0.0f, 1.0f);
	uv.y = clamp(uv.y, 0.0f, 1.0f);
}


//-----------------------------------------------------------------------------
// Compute a context necessary for creating vertex data
//-----------------------------------------------------------------------------

void SurfSetupSurfaceContext( SurfaceCtx_t& ctx, SurfaceHandle_t surfID )
{
	materials->GetLightmapPageSize( 
		SortInfoToLightmapPage( MSurf_MaterialSortID( surfID ) ), 
		&ctx.m_LightmapPageSize[0], &ctx.m_LightmapPageSize[1] );
	ctx.m_LightmapSize[0] = ( MSurf_LightmapExtents( surfID )[0] ) + 1;
	ctx.m_LightmapSize[1] = ( MSurf_LightmapExtents( surfID )[1] ) + 1;

	ctx.m_Scale.x = 1.0f / ( float )ctx.m_LightmapPageSize[0];
	ctx.m_Scale.y = 1.0f / ( float )ctx.m_LightmapPageSize[1];

	ctx.m_Offset.x = ( float )MSurf_OffsetIntoLightmapPage( surfID )[0] * ctx.m_Scale.x;
	ctx.m_Offset.y = ( float )MSurf_OffsetIntoLightmapPage( surfID )[1] * ctx.m_Scale.y;

	if ( ctx.m_LightmapPageSize[0] != 0.0f )
	{
		ctx.m_BumpSTexCoordOffset = ( float )ctx.m_LightmapSize[0] / ( float )ctx.m_LightmapPageSize[0];
	}
	else
	{
		ctx.m_BumpSTexCoordOffset = 0.0f;
	}
}

#endif // DEDICATED

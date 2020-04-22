//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#if defined( INCLUDE_SCALEFORM )
#include "basepanel.h"
#include "options_video_scaleform.h"
#include "filesystem.h"
#include "vgui/ILocalize.h"
#include "inputsystem/iinputsystem.h"
#include "IGameUIFuncs.h"
#include "c_playerresource.h"
#include <vstdlib/vstrtools.h>
#include "matchmaking/imatchframework.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"
#include "iachievementmgr.h"
#include "gameui_interface.h"
#include "gameui_util.h"
#include "vgui_int.h"
#include "materialsystem/materialsystem_config.h"
#include "vgui/ISurface.h"

#ifndef _GAMECONSOLE
#include "steam/steam_api.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar sys_refldetail			( "sys_refldetail", "0", FCVAR_ARCHIVE, "Convar used exclusively by the options screen to set water reflection levels. Changing this convar manually will have no effect." );
ConVar sys_antialiasing			( "sys_antialiasing", "0", FCVAR_ARCHIVE, "Convar used exclusively by the options screen to set anti aliasing levels. Changing this convar manually will have no effect." );
ConVar sys_vsync				( "sys_vsync", "0", FCVAR_NONE, "Used to set video property at device reset." );
ConVar sys_aspectratio			( "sys_aspectratio", "-1", FCVAR_ARCHIVE, "Convar used exclusively by the options screen to set aspect ratio. Changing this convar manually will have no effect." );
ConVar sys_resolution			( "sys_resolution", "-1", FCVAR_NONE, "Convar used exclusively by the options screen to set resolution. Changing this convar manually will have no effect." );

ConVar message_laptop			( "message_power", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar message_antialias		( "message_antialias", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar message_filtering		( "message_filtering", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar message_vsync			( "message_vsync", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar message_queued_mode		( "message_queued_mode", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar message_shader_detail	( "message_shader_detail", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar message_cpu_detail		( "message_cpu_detail", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar message_model_detail		( "message_model_detail", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar message_paged_pool_detail( "message_paged_pool_detail", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar mat_software_aa_strength_detail( "mat_software_aa_strength_detail", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar csm_quality_level_detail( "csm_quality_level_detail", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );
ConVar mat_motion_blur_enabled_detail( "mat_motion_blur_enabled_detail", "0", FCVAR_NONE, "Tracks whether a user has received a notification message during this instance of the game." );

// Some convars in video options UI support an "Auto" selector which will use the automatically detected value for CPU/GPU
// this callback takes the value selected by the user and applies the "restart" value for the same convar
ConVar videooptions_optionsui_callback_disabled( "videooptions_optionsui_callback_disabled", "0", FCVAR_NONE, "Used to set video property from options UI." );
static void VideoOptionsUiConvarChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	// Sometimes we need to revert the convar value, so don't enter this function again
	if ( videooptions_optionsui_callback_disabled.GetBool() )
		return;

	// Get the new value
	const char * szName = var->GetName();
	int nLen = V_strlen( szName );
	static const char * const szSuffix = "_optionsui";
	static const int kSuffixLen = V_strlen( szSuffix );
	
	// Get the matching restart cvar
	if ( nLen <= kSuffixLen )
		return;
	if ( V_stricmp( szName + nLen - kSuffixLen, szSuffix ) )
		return;
	SplitScreenConVarRef cvrestart( CFmtStr( "%.*s_restart", nLen - kSuffixLen, szName ), true );
	if ( !cvrestart.IsValid() )
		return;

	SplitScreenConVarRef cv( var );
	if ( cv.GetInt( 0 ) == 9999999 )
	{
		// "Auto" mode enabled
		bool bFoundDefaultValue = false;
		KeyValues *kvDefaults = new KeyValues( "defaults" );
		if ( kvDefaults->LoadFromFile( filesystem, "cfg/videodefaults.txt", "USRLOCAL" ) )
		{
			if ( char const *szValue = kvDefaults->GetString( CFmtStr( "setting.%.*s", nLen - kSuffixLen, szName ), NULL ) )
			{
				DevMsg( "Auto-detected '%s' = '%s'!\n", szName, szValue );
				cvrestart.SetValue( 0, szValue );
				bFoundDefaultValue = true;
			}
		}
		kvDefaults->deleteThis();
		
		if ( !bFoundDefaultValue )
		{
			Warning( "Failed to access video defaults data, reverting change of '%s' to '%s'!\n", szName, pOldValue );
			videooptions_optionsui_callback_disabled.SetValue( 1 );
			var->SetValue( pOldValue );
			videooptions_optionsui_callback_disabled.SetValue( 0 );
		}
	}
	else
	{
		// Real user setting selected
		cvrestart.SetValue( 0, cv.GetString( 0 ) );
	}
}

// Note that engine.dll, UpdateVideoConfigConVars() can also change these convars (it explictly concats the "_restart" string to the end of each convar name)
ConVar fullscreen_restart		( "fullscreen_restart", "-1", FCVAR_NONE, "Used to set video property at device reset." );
ConVar mat_queue_mode_restart	( "mat_queue_mode_restart", "-1", FCVAR_NONE, "Used to set video property at device reset." );
ConVar mem_level_restart		( "mem_level_restart", "-1", FCVAR_NONE, "Used to set video property at device reset." );
ConVar defaultres_restart		( "defaultres_restart", "-1", FCVAR_NONE, "Used to set video property at device reset." );
ConVar defaultresheight_restart	( "defaultresheight_restart", "-1", FCVAR_NONE, "Used to set video property at device reset." );
ConVar mat_motion_blur_enabled_restart( "mat_motion_blur_enabled_restart", "0", FCVAR_NONE, "Used to set video property at device reset." );

#define VIDEO_OPTIONS_UI_CVAR( cvarname, defaultvalue ) \
ConVar cvarname##_restart( #cvarname "_restart", defaultvalue, FCVAR_NONE, "Used to set video property at device reset." ); \
ConVar cvarname##_optionsui( #cvarname "_optionsui", "9999999", FCVAR_NONE, "Used to set video property from options UI.", VideoOptionsUiConvarChanged );

VIDEO_OPTIONS_UI_CVAR( csm_quality_level, "0" );
VIDEO_OPTIONS_UI_CVAR( gpu_mem_level, "-1" );
VIDEO_OPTIONS_UI_CVAR( cpu_level, "-1" );
VIDEO_OPTIONS_UI_CVAR( gpu_level, "-1" );
VIDEO_OPTIONS_UI_CVAR( mat_forceaniso, "-1" );
VIDEO_OPTIONS_UI_CVAR( mat_antialias, "0" );
VIDEO_OPTIONS_UI_CVAR( mat_aaquality, "0" );

#undef VIDEO_OPTIONS_UI_CVAR


static vmode_t s_pWindowedModes[] = 
{
	// NOTE: These must be sorted by ascending width, then ascending height
	{ 640, 480, 32, 60 },
	{ 852, 480, 32, 60 },
	{ 1280, 720, 32, 60 },
	{ 1920, 1080, 32, 60 },
};

struct RatioToAspectMode_t
{
	int anamorphic;
	float aspectRatio;
};

static RatioToAspectMode_t s_RatioToAspectModes[] =
{
	{	0,		4.0f / 3.0f },
	{	1,		16.0f / 9.0f },
	{	2,		16.0f / 10.0f },
	{	2,		1.0f },
};


COptionsVideoScaleform::COptionsVideoScaleform() :
	m_pResolutionWidget( NULL ),
	m_nSelectedAspect( -1 )
{
	memset( m_rgOptionsBySlot, 0, sizeof( m_rgOptionsBySlot ) );
	memset( m_rgTextBySlot, 0, sizeof( m_rgTextBySlot ) );
	
	m_iSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
}



COptionsVideoScaleform::~COptionsVideoScaleform()
{
	m_vecAAModes.Purge();
	m_vecAspectModes.Purge();
}

bool COptionsVideoScaleform::HandleUpdateChoice( OptionChoice_t * pOptionChoice, int nCurrentChoice )
{
	if ( pOptionChoice && 
		 nCurrentChoice >= 0 &&
		 nCurrentChoice < pOptionChoice->m_Choices.Count() )
	{
		int iConVarSlot = pOptionChoice->m_bSystemValue ? 0 : m_iSplitScreenSlot;

		// fuck these annoying warnings
// 		if ( ShowWarning( pOptionChoice->m_szConVar ) )
// 		{
// 			// show a warning before allowing this option to be changed
// 			return false;
// 		}

		pOptionChoice->m_nChoiceIndex = nCurrentChoice;

		SplitScreenConVarRef varOption( pOptionChoice->m_szConVar );
		varOption.SetValue( iConVarSlot, pOptionChoice->m_Choices[nCurrentChoice].m_szValue );

		if ( !V_strcmp( pOptionChoice->m_szConVar, "sys_refldetail" ) )
		{
			SetReflection( V_atoi( pOptionChoice->m_Choices[nCurrentChoice].m_szValue ) );
			m_bResetRequired = true;
		}
		else if ( !V_strcmp( pOptionChoice->m_szConVar, "sys_antialiasing" ) )
		{
			SetAAMode( V_atoi( pOptionChoice->m_Choices[nCurrentChoice].m_szValue ) );
			m_bResetRequired = true;
		}
		else if ( !V_strcmp( pOptionChoice->m_szConVar, "sys_aspectratio" ) )
		{
			SelectAspectRatio( V_atoi( pOptionChoice->m_Choices[nCurrentChoice].m_szValue ) );
			SelectResolution();
			m_bResetRequired = true;
		}
		else if ( !V_strcmp( pOptionChoice->m_szConVar, "sys_resolution" ) )
		{
			SelectResolution();
			m_bResetRequired = true;
		}
		else if ( !V_strcmp( pOptionChoice->m_szConVar, "sys_vsync" ) ||
		          !V_strcmp( pOptionChoice->m_szConVar, "fullscreen_restart" ) || 
				  !V_strcmp( pOptionChoice->m_szConVar, "mat_software_aa_strength" ) || 
				  !V_strcmp( pOptionChoice->m_szConVar, "csm_quality_level_restart" ) ||
				  !V_strcmp( pOptionChoice->m_szConVar, "mat_motion_blur_enabled_restart" ) ||
				  !V_strcmp( pOptionChoice->m_szConVar, "cpu_level_restart" ) ||
				  !V_strcmp( pOptionChoice->m_szConVar, "gpu_mem_level_restart" ) ||
				  V_strstr( pOptionChoice->m_szConVar, "_optionsui" ) )
		{
			m_bResetRequired = true;
			ApplyChangesToSystemConVar( pOptionChoice->m_szConVar,  V_atoi( pOptionChoice->m_Choices[nCurrentChoice].m_szValue ) );
		}

		if ( m_DialogType == DIALOG_TYPE_VIDEO_ADVANCED || m_bResetRequired )
		{
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowApplyButton", 0, NULL );
			}
		}

		return true;
	}

	return false;
}


void COptionsVideoScaleform::SetChoiceWithConVar( OptionChoice_t * pOption, bool bForceDefaultValue )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	SplitScreenConVarRef varOption( pOption->m_szConVar );
	int iConVarSlot = pOption->m_bSystemValue ? 0 : m_iSplitScreenSlot;

	int nResult = FindChoiceFromString( pOption, varOption.GetString( iConVarSlot ) );

	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	if ( !V_strcmp( pOption->m_szConVar, "sys_antialiasing" ) )
	{
		if ( !bForceDefaultValue )
		{
			SplitScreenConVarRef mat_antialias( "mat_antialias_optionsui" );
			SplitScreenConVarRef mat_aaquality( "mat_aaquality_optionsui" );
			nResult = FindAAMode( mat_antialias.GetInt( 0 ), mat_aaquality.GetInt( 0 ) );
		}
	}
	else if ( !V_strcmp( pOption->m_szConVar, "sys_vsync" ) )
	{
		if ( !bForceDefaultValue )
		{
			SplitScreenConVarRef mat_vsync( "mat_vsync" );
			SplitScreenConVarRef mat_triplebuffered( "mat_triplebuffered" );
			nResult = FindVSync( mat_vsync.GetBool( 0 ), mat_triplebuffered.GetBool( 0 ) );
		}
	}
	else if ( !V_strcmp( pOption->m_szConVar, "sys_aspectratio" ) )
	{
		if ( !bForceDefaultValue )
		{
			nResult = FindCurrentAspectRatio();
		}

		pOption->m_nChoiceIndex = nResult;

		SelectAspectRatio( nResult );
	}
	else if ( !V_strcmp( pOption->m_szConVar, "sys_resolution" ) )
	{
		ResolutionModes_t current = FindCurrentResolution();

		char szResolutionName[ 256 ];

		GetResolutionName( current.m_nWidth, current.m_nHeight, szResolutionName, sizeof( szResolutionName ) );
		
		wchar_t wszTemp[ SF_OPTIONS_MAX ];
		g_pVGuiLocalize->ConvertANSIToUnicode( szResolutionName, wszTemp, sizeof( wszTemp ) );

		for ( int nChoice = 0; nChoice < pOption->m_Choices.Count(); ++nChoice )
		{
			if ( V_wcscmp( pOption->m_Choices[ nChoice ].m_wszLabel, wszTemp ) == 0 )
			{
				nResult = nChoice;
				break;
			}
		}

		if ( nResult == -1 )
		{
			nResult =  pOption->m_Choices.Count() - 1;
		}

		pOption->m_nChoiceIndex = nResult;
	}
	else if (  !V_strcmp( pOption->m_szConVar, "fullscreen_restart" ) )
	{
		nResult = fullscreen_restart.GetInt() != -1 ? fullscreen_restart.GetInt() : !config.Windowed();

#ifdef POSIX
		if ( FullScreenWindowMode() )
#else
		if ( nResult == 0 && FullScreenWindowMode() )
#endif
		{
			nResult = 2;
		}

	}
	else if (  !V_strcmp( pOption->m_szConVar, "mat_software_aa_strength" ) )
	{
		if ( !bForceDefaultValue )
		{
			ConVarRef mat_software_aa_strength( "mat_software_aa_strength" );
			nResult = mat_software_aa_strength.GetInt() > 0;
		}
	}
	else if (  !V_strcmp( pOption->m_szConVar, "csm_quality_level_restart" ) )
	{
		if ( !bForceDefaultValue )
		{
			nResult = clamp<int>( csm_quality_level_restart.GetInt(), CSMQUALITY_VERY_LOW, CSMQUALITY_TOTAL_MODES - 1 );
		}
	}
	else if (  !V_strcmp( pOption->m_szConVar, "mat_motion_blur_enabled_restart" ) )
	{
		if ( !bForceDefaultValue )
		{
			ConVarRef mat_motion_blur_enabled( "mat_motion_blur_enabled" );
			nResult = mat_motion_blur_enabled.GetBool();
		}
	}
	else
	{
		if ( bForceDefaultValue )
		{
			char szConVarName[256];	
			if ( !SplitRestartConvar( pOption->m_szConVar, szConVarName, sizeof( szConVarName ) ) )
			{
				nResult = FindChoiceFromString( pOption, varOption.GetDefault() );
				varOption.SetValue( iConVarSlot, varOption.GetDefault() );
			}
		}
	}
	

	if ( nResult == -1 )
	{
		// Unexpected ConVar value, try matching with the default
		Warning( "ConVar did not match any of the options found in data file: %s\n", pOption->m_szConVar );

		nResult = FindChoiceFromString( pOption, varOption.GetDefault() );

		if ( nResult == -1 )
		{
			// Completely unexpected ConVar value. Display whatever choice is at the zero index so that
			// the client does not draw undefined characters
			Assert( false );
			Warning( "ConVar default not match any of the options found in data file: %s\n", pOption->m_szConVar );

			nResult = 0;						
		}
	}

	pOption->m_nChoiceIndex = nResult;
}

void COptionsVideoScaleform::ResetToDefaults( void )
{
#if !defined( _GAMECONSOLE )
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	m_bOptionsChanged = true;
	m_bResetRequired = true;

	KeyValues *pConfigKeys = new KeyValues( "VideoConfig" );
	if ( !pConfigKeys )
		return;

	if ( !ReadCurrentVideoConfig( pConfigKeys, true ) )
	{
		pConfigKeys->deleteThis();
		return;
	}

	/*if ( m_DialogType == DIALOG_TYPE_VIDEO )
	{
		defaultresheight_restart.SetValue( pConfigKeys->GetInt( "setting.defaultresheight", defaultresheight_restart.GetInt() ) );
		defaultres_restart.SetValue( pConfigKeys->GetInt( "setting.defaultres", defaultres_restart.GetInt() ) );
		fullscreen_restart.SetValue( pConfigKeys->GetBool( "setting.fullscreen", fullscreen_restart.GetBool() ) );
		sys_aspectratio.SetValue( GetScreenAspectRatio( defaultres_restart.GetInt(), defaultresheight_restart.GetInt() ) );
	}
	else */if ( m_DialogType == DIALOG_TYPE_VIDEO || m_DialogType == DIALOG_TYPE_VIDEO_ADVANCED )
	{
		videooptions_optionsui_callback_disabled.SetValue( 1 );

		defaultresheight_restart.SetValue( pConfigKeys->GetInt( "setting.defaultresheight", defaultresheight_restart.GetInt() ) );
		defaultres_restart.SetValue( pConfigKeys->GetInt( "setting.defaultres", defaultres_restart.GetInt() ) );
		fullscreen_restart.SetValue( pConfigKeys->GetBool( "setting.fullscreen", fullscreen_restart.GetBool() ) );
		sys_aspectratio.SetValue( GetScreenAspectRatio( defaultres_restart.GetInt(), defaultresheight_restart.GetInt() ) );

		gpu_mem_level_restart.SetValue( clamp( pConfigKeys->GetInt( "setting.gpu_mem_level", GPU_MEM_LEVEL_LOW ), GPU_MEM_LEVEL_LOW, GPU_MEM_LEVEL_HIGH ) );
		gpu_mem_level_optionsui.SetValue( 9999999 );
		mem_level_restart.SetValue( clamp( pConfigKeys->GetInt( "setting.mem_level", MEM_LEVEL_LOW ), MEM_LEVEL_LOW, MEM_LEVEL_HIGH ) );
		gpu_level_restart.SetValue( clamp( pConfigKeys->GetInt( "setting.gpu_level", GPU_LEVEL_LOW ), GPU_LEVEL_LOW, GPU_LEVEL_VERYHIGH ) );
		gpu_level_optionsui.SetValue( 9999999 );
		cpu_level_restart.SetValue( clamp( pConfigKeys->GetInt( "setting.cpu_level", CPU_LEVEL_LOW ), CPU_LEVEL_LOW, CPU_LEVEL_HIGH ) );
		cpu_level_optionsui.SetValue( 9999999 );

		mat_forceaniso_restart.SetValue( pConfigKeys->GetInt( "setting.mat_forceaniso", 1 ) );
		mat_forceaniso_optionsui.SetValue( 9999999 );
		mat_queue_mode_restart.SetValue( pConfigKeys->GetInt( "setting.mat_queue_mode", -1 ) );

		sys_antialiasing.SetValue( 9999999 );
		mat_antialias_restart.SetValue( pConfigKeys->GetInt( "setting.mat_antialias", 0 ) );
		mat_aaquality_restart.SetValue( pConfigKeys->GetInt( "setting.mat_aaquality", 0 ) );
		mat_antialias_optionsui.SetValue( 9999999 );
		mat_aaquality_optionsui.SetValue( 9999999 );
		sys_vsync.SetValue( FindVSync( pConfigKeys->GetBool( "setting.mat_vsync", true ), pConfigKeys->GetBool( "setting.mat_triplebuffered", false ) ) );

		ConVarRef mat_software_aa_strength( "mat_software_aa_strength" );
		mat_software_aa_strength.SetValue( pConfigKeys->GetBool( "setting.mat_software_aa_strength", false ) );

		csm_quality_level_restart.SetValue( pConfigKeys->GetInt( "setting.csm_quality_level", CSMQUALITY_VERY_LOW ) );
		csm_quality_level_optionsui.SetValue( 9999999 );

		mat_motion_blur_enabled_restart.SetValue( pConfigKeys->GetInt( "setting.mat_motion_blur_enabled", 0 ) != 0 );

		videooptions_optionsui_callback_disabled.SetValue( 0 );
	}

	pConfigKeys->deleteThis();

	RefreshValues( true );

	DisableConditionalWidgets();


	WITH_SLOT_LOCKED
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowApplyButton", 0, NULL );
	}

	return;
#endif // !defined( _GAMECONSOLE )
}


bool COptionsVideoScaleform::OnMessageBoxEvent( MessageBoxFlags_t buttonPressed )
{
	if ( buttonPressed & MESSAGEBOX_FLAG_OK  )
	{
		switch( m_NoticeType )
		{
		case NOTICE_TYPE_RESET_TO_DEFAULT:
			ResetToDefaults();
			break;

		case NOTICE_TYPE_INFO:
			break;

		case NOTICE_TYPE_DISCARD_CHANGES:
			// reset the convars and continue with exit
			engine->ExecuteClientCmd( "mat_updateconvars" );
			defaultresheight_restart.SetValue( -1 );
			defaultres_restart.SetValue( -1 );
			fullscreen_restart.SetValue( -1 );
			Hide();
			break;

		default:
			AssertMsg( false, "Invalid message box notice type" );
		}

		m_NoticeType = NOTICE_TYPE_NONE;
	}

	return true;
}



void COptionsVideoScaleform::BuildAAModes( CUtlVector<OptionChoiceData_t> &choices )
{
	AAMode_t mode;
	
	// none
	mode.m_nNumSamples = 0;
	mode.m_nQualityLevel = 0;

	int nIndex = m_vecAAModes.AddToTail( mode );

	OptionChoiceData_t choiceElement;

	wchar_t const *wszDefaultMSAA = NULL;

	V_wcsncpy( choiceElement.m_wszLabel, wszDefaultMSAA = g_pVGuiLocalize->Find( "#SFUI_Settings_None" ), sizeof( choiceElement.m_wszLabel ) );
	V_snprintf( choiceElement.m_szValue, sizeof( choiceElement.m_szValue ), "%i", nIndex );
	choices.AddToTail( choiceElement );

	// 2x
	if ( materials->SupportsMSAAMode(2) )
	{
		mode.m_nNumSamples = 2;
		mode.m_nQualityLevel = 0;
		nIndex = m_vecAAModes.AddToTail( mode );

		V_wcsncpy( choiceElement.m_wszLabel, wszDefaultMSAA = g_pVGuiLocalize->Find( "#SFUI_Settings_2X_MSAA" ), sizeof( choiceElement.m_wszLabel ) );
		V_snprintf( choiceElement.m_szValue, sizeof( choiceElement.m_szValue ), "%i", nIndex );
		choices.AddToTail( choiceElement );
	}

	// 4x
	if ( materials->SupportsMSAAMode(4) )
	{
		mode.m_nNumSamples = 4;
		mode.m_nQualityLevel = 0;
		nIndex = m_vecAAModes.AddToTail( mode );

		V_wcsncpy( choiceElement.m_wszLabel, wszDefaultMSAA = g_pVGuiLocalize->Find( "#SFUI_Settings_4X_MSAA" ), sizeof( choiceElement.m_wszLabel ) );
		V_snprintf( choiceElement.m_szValue, sizeof( choiceElement.m_szValue ), "%i", nIndex );
		choices.AddToTail( choiceElement );
	}

	// 8x
	if ( materials->SupportsMSAAMode(8) )
	{
		mode.m_nNumSamples = 8;
		mode.m_nQualityLevel = 0;
		nIndex = m_vecAAModes.AddToTail( mode );

		V_wcsncpy( choiceElement.m_wszLabel, wszDefaultMSAA = g_pVGuiLocalize->Find( "#SFUI_Settings_8X_MSAA" ), sizeof( choiceElement.m_wszLabel ) );
		V_snprintf( choiceElement.m_szValue, sizeof( choiceElement.m_szValue ), "%i", nIndex );
		choices.AddToTail( choiceElement );
	}
	

	// 8x CSAA
	if ( materials->SupportsCSAAMode(4, 2) )
	{
		mode.m_nNumSamples = 4;
		mode.m_nQualityLevel = 2;
		nIndex = m_vecAAModes.AddToTail( mode );

		V_wcsncpy( choiceElement.m_wszLabel, g_pVGuiLocalize->Find( "#SFUI_Settings_8X_CSAA" ), sizeof( choiceElement.m_wszLabel ) );
		V_snprintf( choiceElement.m_szValue, sizeof( choiceElement.m_szValue ), "%i", nIndex );
		choices.AddToTail( choiceElement );
	}

	// 16x CSAA
	if ( materials->SupportsCSAAMode(4, 4) )
	{
		mode.m_nNumSamples = 4;
		mode.m_nQualityLevel = 4;
		nIndex = m_vecAAModes.AddToTail( mode );

		V_wcsncpy( choiceElement.m_wszLabel, g_pVGuiLocalize->Find( "#SFUI_Settings_16X_CSAA" ), sizeof( choiceElement.m_wszLabel ) );
		V_snprintf( choiceElement.m_szValue, sizeof( choiceElement.m_szValue ), "%i", nIndex );
		choices.AddToTail( choiceElement );
	}

	// 16xQ CSAA
	if ( materials->SupportsCSAAMode(8, 2) )
	{
		mode.m_nNumSamples = 8;
		mode.m_nQualityLevel = 2;
		nIndex = m_vecAAModes.AddToTail( mode );

		V_wcsncpy( choiceElement.m_wszLabel, g_pVGuiLocalize->Find( "#SFUI_Settings_16XQ_CSAA" ), sizeof( choiceElement.m_wszLabel ) );
		V_snprintf( choiceElement.m_szValue, sizeof( choiceElement.m_szValue ), "%i", nIndex );
		choices.AddToTail( choiceElement );
	}

	//
	// Add the autodetect setting here:
	//
	V_wcsncpy( choiceElement.m_wszLabel, g_pVGuiLocalize->Find( "#SFUI_Settings_Choice_Autodetect" ), sizeof( choiceElement.m_wszLabel ) );
	V_wcsncat( choiceElement.m_wszLabel, L" : <font color='#707070'>", sizeof( choiceElement.m_wszLabel ) );
	V_wcsncat( choiceElement.m_wszLabel, wszDefaultMSAA, sizeof( choiceElement.m_wszLabel ) );
	V_wcsncat( choiceElement.m_wszLabel, L"</font>", sizeof( choiceElement.m_wszLabel ) );
	V_snprintf( choiceElement.m_szValue, sizeof( choiceElement.m_szValue ), "%i", 9999999 );
	choices.AddToTail( choiceElement );
	mode.m_nNumSamples = 9999999;
	mode.m_nQualityLevel = 9999999;
	nIndex = m_vecAAModes.AddToTail( mode );
}


int COptionsVideoScaleform::FindAAMode( int nAASamples, int nAAQuality )
{
	AAMode_t target;
	target.m_nNumSamples = nAASamples;
	target.m_nQualityLevel = nAAQuality;
	
	return m_vecAAModes.Find( target );
}

void COptionsVideoScaleform::SetAAMode( int nIndex )
{
	if ( m_vecAAModes.Count() > 0 )
	{
		videooptions_optionsui_callback_disabled.SetValue( 1 );

		if ( nIndex >= m_vecAAModes.Count() - 1 )
		{
			// Selecting "Autodetect" mode:
			KeyValues *kvDefaults = new KeyValues( "defaults" );
			kvDefaults->LoadFromFile( filesystem, "cfg/videodefaults.txt", "USRLOCAL" );
			char const *szValueAA = kvDefaults->GetString( "setting.mat_antialias", "0" );
			char const *szValueAAQ = kvDefaults->GetString( "setting.mat_aaquality", "0" );
			kvDefaults->deleteThis();

			ApplyChangesToSystemConVar( "mat_antialias_restart", V_atoi( szValueAA ) );
			ApplyChangesToSystemConVar( "mat_aaquality_restart", V_atoi( szValueAAQ ) );
			ApplyChangesToSystemConVar( "mat_antialias_optionsui", 9999999 );
			ApplyChangesToSystemConVar( "mat_aaquality_optionsui", 9999999 );
		}
		else
		{
			ApplyChangesToSystemConVar( "mat_antialias_restart", m_vecAAModes[ nIndex ].m_nNumSamples );
			ApplyChangesToSystemConVar( "mat_aaquality_restart", m_vecAAModes[ nIndex ].m_nQualityLevel );
			ApplyChangesToSystemConVar( "mat_antialias_optionsui", m_vecAAModes[ nIndex ].m_nNumSamples );
			ApplyChangesToSystemConVar( "mat_aaquality_optionsui", m_vecAAModes[ nIndex ].m_nQualityLevel );
		}

		videooptions_optionsui_callback_disabled.SetValue( 0 );

		m_bResetRequired = true;
	}
	else
	{
		AssertMsg( false, "SetAAMode called without first calling BuildAAMode" );
		Warning( "Error: SetAAMode called without first calling BuildAAMode!" );
	}
}


int COptionsVideoScaleform::FindReflection( void )
{
	int nResult = -1;
#ifndef _GAMECONSOLE
	SplitScreenConVarRef r_waterforceexpensive( "r_waterforceexpensive" );
#endif
	SplitScreenConVarRef r_waterforcereflectentities( "r_waterforcereflectentities" );

#ifndef _GAMECONSOLE
	if ( r_waterforceexpensive.GetBool( 0 ) )
#endif
	{
		if ( r_waterforcereflectentities.GetBool( 0 ) )
		{
			nResult = 2;
		}
		else
		{
			nResult = 1;
		}
	}
#ifndef _GAMECONSOLE
	else
	{
		nResult = 0;
	}
#endif

	return nResult;
}

void COptionsVideoScaleform::SetReflection( int nIndex )
{
	switch( nIndex )
	{
	case 0:
#ifndef _GAMECONSOLE
		ApplyChangesToSystemConVar( "r_waterforceexpensive", false );
#endif
		ApplyChangesToSystemConVar( "r_waterforcereflectentities", false );
		break;
	case 1:
#ifndef _GAMECONSOLE
		ApplyChangesToSystemConVar( "r_waterforceexpensive", true );
#endif
		ApplyChangesToSystemConVar( "r_waterforcereflectentities", false );
		break;
	case 2:
#ifndef _GAMECONSOLE
		ApplyChangesToSystemConVar( "r_waterforceexpensive", true );
#endif
		ApplyChangesToSystemConVar( "r_waterforcereflectentities", true );
		break;
	default:
		AssertMsg( false, "sys_refldetail value out of range" );
		break;
	};
}

int COptionsVideoScaleform::FindVSync( bool bVsync, bool bTrippleBuffered )
{
	int nResult = 0;

	if ( bVsync )
	{
		nResult = 1;
	}

	if ( bTrippleBuffered )
	{
		nResult = 2;
	}

	return nResult;
}

void COptionsVideoScaleform::SetVSync( int nIndex )
{
	switch( nIndex )
	{
	case 0:
		ApplyChangesToSystemConVar( "mat_vsync", false );
		ApplyChangesToSystemConVar( "mat_triplebuffered", false );
		break;
	case 1:
		ApplyChangesToSystemConVar( "mat_vsync", true );
		ApplyChangesToSystemConVar( "mat_triplebuffered", false );
		break;
	case 2:
		ApplyChangesToSystemConVar( "mat_vsync", true );
		ApplyChangesToSystemConVar( "mat_triplebuffered", true );
		break;
	default:
		AssertMsg( false, "sys_vsync value out of range" );
		break;
	};
}


int COptionsVideoScaleform::GetScreenAspectRatio( int width, int height )
{
	float aspectRatio = (float)width / (float)height;

	// just find the closest ratio
	float closestAspectRatioDist = 99999.0f;
	int closestAnamorphic = 0;
	for (int i = 0; i < ARRAYSIZE(s_RatioToAspectModes); i++)
	{
		float dist = fabs( s_RatioToAspectModes[i].aspectRatio - aspectRatio );
		if (dist < closestAspectRatioDist)
		{
			closestAspectRatioDist = dist;
			closestAnamorphic = s_RatioToAspectModes[i].anamorphic;
		}
	}

	return closestAnamorphic;
}


void COptionsVideoScaleform::BuildResolutionModes( CUtlVector<OptionChoiceData_t> &choices )
{
	if ( m_vecAspectModes.Count() == 0 )
	{
		GenerateCompatibleResolutions();
	}

	if ( m_nSelectedAspect == -1 )
	{
		m_nSelectedAspect = FindCurrentAspectRatio();
	}

	OptionChoiceData_t choiceElement;

	for ( int i = 0; i < m_vecAspectModes[m_nSelectedAspect].m_vecResolutionModes.Count(); i++ )
	{
		GetResolutionName( m_vecAspectModes[m_nSelectedAspect].m_vecResolutionModes[i].m_nWidth,
			m_vecAspectModes[m_nSelectedAspect].m_vecResolutionModes[i].m_nHeight,
			choiceElement.m_szValue,
			sizeof( choiceElement.m_szValue) );

		g_pVGuiLocalize->ConvertANSIToUnicode( choiceElement.m_szValue, choiceElement.m_wszLabel, sizeof( choiceElement.m_wszLabel ) );

		choices.AddToTail( choiceElement );
	}

	// [will] - It's possible that there is no valid resolution for a given aspect ratio. In that case, display the "None" label.
	if( choices.Count() == 0 )
	{
		V_wcsncpy( choiceElement.m_wszLabel, g_pVGuiLocalize->Find( "#SFUI_Settings_None" ), sizeof( choiceElement.m_wszLabel ) );
		V_strncpy( choiceElement.m_szValue, "#SFUI_Settings_None", sizeof( choiceElement.m_szValue ) );

		choices.AddToTail( choiceElement );
	}
}



void COptionsVideoScaleform::GetResolutionName( int nWidth, int nHeight, char *pOutBuffer, int nOutBufferLength )
{
	V_snprintf( pOutBuffer, nOutBufferLength, "%i x %i", nWidth, nHeight );
}


void COptionsVideoScaleform::GenerateCompatibleResolutions( )
{
	if ( m_vecAspectModes.Count() != 0 )
	{
		return;
	}

	m_vecAspectModes.AddMultipleToTail( 3 );

	// get full video mode list
	vmode_t *plist = NULL;
	int nCount = 0;
	gameuifuncs->GetVideoModes( &plist, &nCount );

	int nDesktopWidth, nDesktopHeight;
	gameuifuncs->GetDesktopResolution( nDesktopWidth, nDesktopHeight );

	if ( m_nSelectedAspect == -1 )
	{
		m_nSelectedAspect = FindCurrentAspectRatio();
	}

	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	SplitScreenConVarRef fullscreen_restart( "fullscreen_restart" );
	bool bWindowed = fullscreen_restart.GetInt( 0 ) != -1 ? !fullscreen_restart.GetInt( 0 ) : config.Windowed();

	CUtlVector< vmode_t > windowedModes;

	if ( bWindowed )
	{
		GenerateWindowedModes( windowedModes, nCount, plist );
		nCount = windowedModes.Count();
		plist = windowedModes.Base();
	}

	for ( int i = 0; i < nCount; i++, plist++ )
	{
		// don't show modes bigger than the desktop for windowed mode
		if ( bWindowed && ( plist->width > nDesktopWidth || plist->height > nDesktopHeight ) )
			continue;

		ResolutionModes_t mode;
		mode.m_nHeight = plist->height;
		mode.m_nWidth = plist->width;

		int nAspectMode = GetScreenAspectRatio( plist->width, plist->height );

		if ( m_vecAspectModes[nAspectMode].m_vecResolutionModes.Find( mode ) != -1 )
			continue;

		m_vecAspectModes[nAspectMode].m_vecResolutionModes.AddToTail( mode );
	}

}

int COptionsVideoScaleform::FindCurrentAspectRatio()
{
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	return GetScreenAspectRatio( config.m_VideoMode.m_Width, config.m_VideoMode.m_Height );
}

const COptionsVideoScaleform::ResolutionModes_t COptionsVideoScaleform::FindCurrentResolution()
{
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	ResolutionModes_t mode;
	SplitScreenConVarRef defaultres_restart( "defaultres_restart" );
	SplitScreenConVarRef defaultresheight_restart( "defaultresheight_restart" );

	mode.m_nWidth= defaultres_restart.GetInt( 0 ) != -1 ? defaultres_restart.GetInt( 0 ) : config.m_VideoMode.m_Width;
	mode.m_nHeight = defaultresheight_restart.GetInt( 0 ) != -1 ? defaultresheight_restart.GetInt( 0 ) : config.m_VideoMode.m_Height;

	return mode;
}

void COptionsVideoScaleform::SelectAspectRatio( int nSelection )
{
	m_nSelectedAspect = nSelection;

	if ( m_vecAspectModes.Count() == 0 )
	{
		GenerateCompatibleResolutions();
	}

	if ( m_pResolutionWidget )
	{
		m_pResolutionWidget->m_Choices.Purge();
		BuildResolutionModes( m_pResolutionWidget->m_Choices );
		SetChoiceWithConVar( m_pResolutionWidget );

		UpdateWidget( m_pResolutionWidget->m_nWidgetSlotID, m_pResolutionWidget );
	}

}

void COptionsVideoScaleform::SelectResolution()
{
	// [will] - Make sure the choice is valid (to prevent crash when there are no resolutions available at that aspect ratio).
	if ( m_pResolutionWidget->m_nChoiceIndex >= 0 && m_pResolutionWidget->m_nChoiceIndex < m_vecAspectModes[m_nSelectedAspect].m_vecResolutionModes.Count() )
	{
		ResolutionModes_t mode;
		mode.m_nWidth = m_vecAspectModes[m_nSelectedAspect].m_vecResolutionModes[m_pResolutionWidget->m_nChoiceIndex].m_nWidth;
		mode.m_nHeight = m_vecAspectModes[m_nSelectedAspect].m_vecResolutionModes[m_pResolutionWidget->m_nChoiceIndex].m_nHeight;

		m_bResetRequired = true;
		ApplyChangesToSystemConVar( "sys_resolution", m_pResolutionWidget->m_nChoiceIndex );
		ApplyChangesToSystemConVar( "defaultres_restart", mode.m_nWidth );
		ApplyChangesToSystemConVar( "defaultresheight_restart", mode.m_nHeight );
	}
}




void COptionsVideoScaleform::GenerateWindowedModes( CUtlVector< vmode_t > &windowedModes, int nCount, vmode_t *pFullscreenModes )
{
	int nFSMode = 0;
	for ( int i = 0; i < ARRAYSIZE( s_pWindowedModes ); ++i )
	{
		while ( true )
		{
			if ( nFSMode >= nCount )
				break;

			if ( pFullscreenModes[nFSMode].width > s_pWindowedModes[i].width )
				break;

			if ( pFullscreenModes[nFSMode].width == s_pWindowedModes[i].width )
			{
				if ( pFullscreenModes[nFSMode].height > s_pWindowedModes[i].height )
					break;

				if ( pFullscreenModes[nFSMode].height == s_pWindowedModes[i].height )
				{
					// Don't add the matching fullscreen mode
					++nFSMode;
					break;
				}
			}

			windowedModes.AddToTail( pFullscreenModes[nFSMode] );
			++nFSMode;
		}

		windowedModes.AddToTail( s_pWindowedModes[i] );
	}

	for ( ; nFSMode < nCount; ++nFSMode )
	{
		windowedModes.AddToTail( pFullscreenModes[nFSMode] );
	}
}

bool COptionsVideoScaleform::ShowWarning( const char * szConVar )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	AssertMsg( m_NoticeType == NOTICE_TYPE_NONE, "Notice being displayed while previous notice active" );
	if ( m_NoticeType != NOTICE_TYPE_NONE ) 
		return false; // cannot show another notice

	const char * szTitle = NULL;
	const char * szBody = NULL;

	if ( !V_strcmp( szConVar, "mat_powersavingsmode" ) )
	{
		ConVarRef convar( "message_power" );
		
		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_Laptop_Power";
			szBody = "#SFUI_Settings_PowerSavings_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "sys_antialiasing" ) )
	{
		ConVarRef convar( "message_antialias" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_Antialiasing_Mode";
			szBody = "#SFUI_Settings_Antialiasing_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "mat_forceaniso_restart" ) )
	{
		ConVarRef convar( "message_filtering" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_Filtering_Mode";
			szBody = "#SFUI_Settings_Filtering_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "sys_vsync" ) )
	{
		ConVarRef convar( "message_vsync" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_Vertical_Sync";
			szBody = "#SFUI_Settings_WaitForVSync_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "mat_queue_mode_restart" ) )
	{
		ConVarRef convar( "message_queued_mode" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_Multicore";
			szBody = "#SFUI_Settings_QueuedMode_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "gpu_level_restart" ) )
	{
		ConVarRef convar( "message_shader_detail" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_Shader_Detail";
			szBody = "#SFUI_Settings_ShaderDetail_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "cpu_level_restart" ) )
	{
		ConVarRef convar( "message_cpu_detail" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_Effect_Detail";
			szBody = "#SFUI_Settings_CPUDetail_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "gpu_mem_level_restart" ) )
	{
		ConVarRef convar( "message_model_detail" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_Model_Texture_Detail";
			szBody = "#SFUI_Settings_ModelDetail_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "mem_level_restart" ) )
	{
		ConVarRef convar( "message_paged_pool_detail" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_Paged_Pool";
			szBody = "#SFUI_Settings_Paged_Pool_Mem_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "csm_quality_level_restart" ) )
	{
		ConVarRef convar( "csm_quality_level_detail" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_CSM";
			szBody = "#SFUI_Settings_CSM_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "mat_software_aa_strength" ) )
	{
		ConVarRef convar( "mat_software_aa_strength_detail" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_FXAA";
			szBody = "#SFUI_Settings_FXAA_Info";
			convar.SetValue( true, 0 );
		}
	}
	else if ( !V_strcmp( szConVar, "mat_motion_blur_enabled_restart" ) )
	{
		ConVarRef convar( "mat_motion_blur_enabled_detail" );

		if ( convar.GetBool( 0 ) == false )
		{
			m_NoticeType = NOTICE_TYPE_INFO;
			szTitle = "#SFUI_Settings_MotionBlur";
			szBody = "#SFUI_Settings_MotionBlur_Info";
			convar.SetValue( true, 0 );
		}
	}

	if ( m_NoticeType != NOTICE_TYPE_NONE )  
	{
		if ( ( m_NoticeType == NOTICE_TYPE_INFO ) && szTitle && szBody )
		{
			( ( CCStrike15BasePanel* )BasePanel() )->OnOpenMessageBox( szTitle, szBody, "#SFUI_Settings_Changes_Notice_Nav", ( MESSAGEBOX_FLAG_OK ), this, &m_pConfirmDialog );
			return true;
		}
		else
		{
			Assert( false );
		}
	}

	return false;
}

void COptionsVideoScaleform::PreSaveChanges()
{
	bool bRefreshInventoryIcons = false;

	if ( m_bResetRequired )
	{
		const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

		/*if ( m_DialogType == DIALOG_TYPE_VIDEO_ADVANCED )
		{
			// iterate all the advanced video options and commit any convars flagged with restart
			for ( int iOption = 0; iOption < m_vecOptions.Count(); ++iOption )
			{
				Option_t * pOption = m_vecOptions[iOption];

				char szConvVar[256];

				if ( SplitRestartConvar( pOption->m_szConVar, szConvVar, sizeof( pOption->m_szConVar ) ) )
				{
					SplitScreenConVarRef cvRestart( pOption->m_szConVar );
					SplitScreenConVarRef cvExtracted( szConvVar );

					if ( pOption->m_bRefreshInventoryIconsWhenIncreased && cvRestart.GetInt( 0 ) > cvExtracted.GetInt( 0 ) )
					{
						bRefreshInventoryIcons = true;
					}

					cvExtracted.SetValue( 0, cvRestart.GetInt( 0 ) );
				}

			}

			SplitScreenConVarRef mat_antialias( "mat_antialias" );
			SplitScreenConVarRef mat_aaquality( "mat_aaquality" );

			SplitScreenConVarRef mat_antialias_restart( "mat_antialias_restart" );
			SplitScreenConVarRef mat_aaquality_restart( "mat_aaquality_restart" );

			mat_antialias.SetValue( 0, mat_antialias_restart.GetInt( 0 ) );
			mat_aaquality.SetValue( 0, mat_aaquality_restart.GetInt( 0 ) );
			
			ConVarRef csm_quality_level( "csm_quality_level" );
			csm_quality_level.SetValue( clamp<int>( csm_quality_level_restart.GetInt(), CSMQUALITY_VERY_LOW, CSMQUALITY_TOTAL_MODES - 1 ) );

			SetVSync( sys_vsync.GetInt() );

			ConVarRef mat_motion_blur_enabled( "mat_motion_blur_enabled" );
			mat_motion_blur_enabled.SetValue( mat_motion_blur_enabled_restart.GetInt() );
		}
		else */if (  m_DialogType == DIALOG_TYPE_VIDEO  )
		{
			int nWidth = defaultres_restart.GetInt() != -1 ? defaultres_restart.GetInt() : config.m_VideoMode.m_Width;
			int nHeight = defaultresheight_restart.GetInt() != -1 ? defaultresheight_restart.GetInt() : config.m_VideoMode.m_Height;
			bool bWindowed = fullscreen_restart.GetInt() != -1 ? !fullscreen_restart.GetInt() : config.Windowed();
			bool bNoBorder = false;

#ifdef POSIX
			if ( fullscreen_restart.GetInt() == 2 || ( fullscreen_restart.GetInt() == -1 && config.NoWindowBorder() && !bWindowed ) )
			{
				// full screen windowed mode
				bNoBorder = true;
				bWindowed = false;
			}
#else
			if ( fullscreen_restart.GetInt() == 2 )
			{
				// full screen windowed mode
				bNoBorder = true;
				bWindowed = true;
				gameuifuncs->GetDesktopResolution( nWidth, nHeight );
			}
#endif

			DisableConditionalWidgets();

			char szCmd[ 256 ];
			V_snprintf( szCmd, sizeof( szCmd ), "mat_setvideomode %i %i %i %i\n", nWidth, nHeight, bWindowed, bNoBorder );

			engine->ClientCmd_Unrestricted( szCmd );

			// iterate all the advanced video options and commit any convars flagged with restart
			for ( int iOption = 0; iOption < m_vecOptions.Count(); ++iOption )
			{
				Option_t * pOption = m_vecOptions[iOption];

				char szConvVar[256];

				if ( SplitRestartConvar( pOption->m_szConVar, szConvVar, sizeof( pOption->m_szConVar ) ) )
				{
					SplitScreenConVarRef cvRestart( CFmtStr( "%s_restart", szConvVar ) );
					SplitScreenConVarRef cvExtracted( szConvVar );

					if ( pOption->m_bRefreshInventoryIconsWhenIncreased && cvRestart.GetInt( 0 ) > cvExtracted.GetInt( 0 ) )
					{
						bRefreshInventoryIcons = true;
					}

					cvExtracted.SetValue( 0, cvRestart.GetInt( 0 ) );
				}

			}

			SplitScreenConVarRef mat_antialias( "mat_antialias" );
			SplitScreenConVarRef mat_aaquality( "mat_aaquality" );

			SplitScreenConVarRef mat_antialias_restart( "mat_antialias_restart" );
			SplitScreenConVarRef mat_aaquality_restart( "mat_aaquality_restart" );

			mat_antialias.SetValue( 0, mat_antialias_restart.GetInt( 0 ) );
			mat_aaquality.SetValue( 0, mat_aaquality_restart.GetInt( 0 ) );

			ConVarRef csm_quality_level( "csm_quality_level" );
			csm_quality_level.SetValue( clamp<int>( csm_quality_level_restart.GetInt(), CSMQUALITY_VERY_LOW, CSMQUALITY_TOTAL_MODES - 1 ) );

			SetVSync( sys_vsync.GetInt() );

			ConVarRef mat_motion_blur_enabled( "mat_motion_blur_enabled" );
			mat_motion_blur_enabled.SetValue( mat_motion_blur_enabled_restart.GetInt() );
		}

		engine->ClientCmd_Unrestricted( "mat_savechanges\n" );

		if ( bRefreshInventoryIcons )
		{
			engine->ClientCmd_Unrestricted( "econ_clear_inventory_images\n" );
		}

		m_bResetRequired = false;

		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HideApplyButton", 0, NULL );
		}
	}
};

void COptionsVideoScaleform::HandleDisableConditionalWidgets( Option_t * pOption, int & nWidgetIDOut, bool & bDisableOut )
{
	if ( pOption->m_szConVar && ( V_strcmp( pOption->m_szConVar, "mat_monitorgamma" ) == 0 ||
								  V_strcmp( pOption->m_szConVar, "mat_monitorgamma_tv_enabled" ) == 0 ) )
	{
		nWidgetIDOut = pOption->m_nWidgetSlotID;

		const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
		
		bDisableOut = fullscreen_restart.GetInt() != -1 ? ( fullscreen_restart.GetInt() != 1 ) : config.Windowed();
	}
	else if ( pOption->m_szConVar && ( V_strcmp( pOption->m_szConVar, "sys_resolution" ) == 0 ||
									   V_strcmp( pOption->m_szConVar, "sys_aspectratio" ) == 0 ) )
	{
		nWidgetIDOut = pOption->m_nWidgetSlotID;

		bDisableOut = fullscreen_restart.GetInt() != -1 ? ( fullscreen_restart.GetInt() == 2 ) : FullScreenWindowMode();
	}
}



bool COptionsVideoScaleform::InitUniqueWidget( const char * szWidgetID, OptionChoice_t * pOptionChoice  )
{
	bool bFound = false;
	bool bWidescreen = IsWidescreen();

	if ( !bWidescreen && !V_strcmp( szWidgetID, "SplitScreenMode" ) )
	{
		int nChoice = pOptionChoice->m_Choices.AddToTail();
		OptionChoiceData_t * pNewOptionChoice = &( pOptionChoice->m_Choices[ nChoice ]);

		V_wcsncpy( pNewOptionChoice->m_wszLabel, g_pVGuiLocalize->Find( "#SFUI_Settings_SplitMode_Horz" ), sizeof( pNewOptionChoice->m_wszLabel ) );
		V_strncpy( pNewOptionChoice->m_szValue, "0", sizeof( pNewOptionChoice->m_szValue ) );

		bFound = true;
	}
	else if ( !V_strcmp( szWidgetID, "Antialiasing Mode" ) )
	{
		BuildAAModes( pOptionChoice->m_Choices );
		bFound = true;
	}
	else if ( !V_strcmp( szWidgetID, "Resolution" ) )
	{
		m_pResolutionWidget = pOptionChoice;
		BuildResolutionModes( pOptionChoice->m_Choices );
		bFound = true;
	}

	return bFound;
}

bool COptionsVideoScaleform::FullScreenWindowMode( void )
{
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	bool bWindowed = config.Windowed();
	bool bBorderless = config.NoWindowBorder();

	ResolutionModes_t current = FindCurrentResolution();

	int desktopWidth, desktopHeight;
	gameuifuncs->GetDesktopResolution( desktopWidth, desktopHeight );

	bool bFullResolution = ( desktopHeight == current.m_nHeight && desktopWidth == current.m_nWidth );

#ifdef POSIX
	return !bWindowed && bBorderless;
#else
	return ( bWindowed && bBorderless && bFullResolution );
#endif
}






#endif // INCLUDE_SCALEFORM

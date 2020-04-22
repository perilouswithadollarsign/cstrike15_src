//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "scaleformuiintegration.h"
#include "tier1/keyvalues.h"
#include "vgui/ILocalize.h"
#include "matchmaking/imatchframework.h"
#include "shaderapi/ishaderapi.h"
#if !defined( NO_STEAM )
#include "steam/steam_api.h"
#endif


#include "Render/ImageFiles/PNG_ImageFile.h"
#include "Render/ImageFiles/DDS_ImageFile.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

using namespace Scaleform::GFx;

#if !defined( NO_STEAM )
static CSteamAPIContext g_SteamAPIContext;
CSteamAPIContext *steamapicontext = &g_SteamAPIContext;
#endif

/********************************************
 * This is the singleton that will be exposed to all other DLLs
 * through the valve dll system
 */

ScaleformUIImpl ScaleformUIImpl::m_Instance;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( ScaleformUIImpl, IScaleformUI, SCALEFORMUI_INTERFACE_VERSION, ScaleformUIImpl::m_Instance )

static const char* defSafeZone = "1.0";
static const char* defHudScaling = "0.85";

ConVar safezonex( "safezonex", defSafeZone, FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "The percentage of the screen width that is considered safe from overscan", true, 0.2f, true, 1.0f );
ConVar safezoney( "safezoney", defSafeZone, FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "The percentage of the screen height that is considered safe from overscan", true, 0.85f, true, 1.0f );
ConVar hud_scaling( "hud_scaling", defHudScaling, FCVAR_ARCHIVE, "Scales hud elements", true, .5f, true, 0.95f );

void safezonechanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConVarRef varOption( "safezonex" );

	float fNewSafe = varOption.GetFloat();
	fNewSafe = clamp( fNewSafe, SFINST.GetEnginePtr()->GetSafeZoneXMin(), varOption.GetMax() );

	if ( fNewSafe != varOption.GetFloat() )
		varOption.SetValue( fNewSafe );

	SFINST.UpdateSafeZone();
}

// Defaults to a typically unused value, so that a fresh user profile detects the load and initializes the UI tint properly
ConVar sf_ui_tint( "sf_ui_tint", "8", FCVAR_ARCHIVE, "The current tint applied to the Scaleform UI" );

void ui_tint_changed( IConVar *var, const char *pOldValue, float flOldValue )
{
	SFINST.UpdateTint();
}

ScaleformUIImpl::ScaleformUIImpl()
{
	ClearMembers();
}

void ScaleformUIImpl::ClearMembers( void )
{
#if defined( USE_SDL ) || defined( OSX )
	m_pLauncherMgr = NULL;
#endif

	m_pSystem = NULL;
	m_pLoader = NULL;
	m_pAllocator = NULL;
	m_pShaderDeviceMgr = NULL;
	m_pDeviceCallbacks = NULL;
	m_pGameUIFuncs = NULL;
	m_bTrySWFFirst = false;
	m_iScreenWidth = 0;
	m_iScreenHeight = 0;
	m_iLastMouseX = 0;
	m_iLastMouseY = 0;
	m_pDefaultAvatarImage = NULL;
	m_pDefaultAvatarTexture = NULL;
	m_pDefaultInventoryImage = NULL;
	m_pDefaultInventoryTexture = NULL;
#ifdef USE_DEFAULT_INVENTORY_ICON_BACKGROUNDS
	m_defaultInventoryIcons.Init( 64 );
#endif
	m_pDefaultChromeHTMLImage = NULL;
	m_pEngine = NULL;
	m_pGameEventManager = NULL;
	m_pShaderAPI = NULL;

	m_bPumpScaleformStats = false;
	m_bForcePS3 = false;
	m_bDenyAllInputToGame = false;

	m_pRenderHAL.Clear();
	m_pRenderer2D.Clear();

	m_pThreadCommandQueue = NULL;
	
	m_bIMEEnabled = false;
	m_iIMEFocusSlot = SF_FULL_SCREEN_SLOT;

	m_pDevice = NULL;
#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
	m_pD3D9Stateblock = NULL;
#endif

	m_bSingleThreaded = true;
	m_bClearMeshCacheQueued = false;
	m_fTime = 0.0f;
}

bool ScaleformUIImpl::Connect( CreateInterfaceFn factory )
{
	if ( !factory )
	{
		return false;
	}

	if ( !BaseClass::Connect( factory ) )
	{
		return false;
	}

	int result;

#if defined( USE_SDL )
	m_pLauncherMgr = (ILauncherMgr *)factory( SDLMGR_INTERFACE_VERSION, NULL);
#elif defined( OSX )
	m_pLauncherMgr = (ILauncherMgr *)factory( COCOAMGR_INTERFACE_VERSION, NULL);
#endif

	m_pShaderDeviceMgr = ( IShaderDeviceMgr* ) factory( SHADER_DEVICE_MGR_INTERFACE_VERSION, &result );
	m_pGameUIFuncs = ( IGameUIFuncs* ) factory( VENGINE_GAMEUIFUNCS_VERSION, &result );
	m_pEngine = ( IVEngineClient* )factory( VENGINE_CLIENT_INTERFACE_VERSION, NULL );
	m_pGameEventManager = ( IGameEventManager2* )factory ( INTERFACEVERSION_GAMEEVENTSMANAGER2, &result );
	m_pShaderAPI = ( IShaderAPI * )factory( SHADERAPI_INTERFACE_VERSION, &result );

	if ( !m_pShaderDeviceMgr || !m_pGameUIFuncs || !m_pEngine || !m_pGameEventManager || !m_pShaderAPI )
	{
		Warning( "ScaleformUI missing expected interface\n" );
		return false;
	}

	// Initialize the console variables.
	ConVar_Register();

	RefreshKeyBindings();

	return m_pShaderDeviceMgr != NULL;
}

void ScaleformUIImpl::Disconnect( void )
{
	ShutdownRendererImpl();
	m_pShaderDeviceMgr = NULL;
	m_pGameUIFuncs = NULL;
	m_pEngine = NULL;
	m_pGameEventManager = NULL;
	m_pShaderAPI = NULL;
	BaseClass::Disconnect();
}

void ScaleformUIImpl::InitFonts( void )
{
	MEM_ALLOC_CREDIT();

	// get the file that defines the font mapping

	KeyValues* pkeyValueData = new KeyValues( "english" );
	KeyValues::AutoDelete autodelete( pkeyValueData );

	// Load the config data
	if ( !pkeyValueData )
	{
		return;
	}

	pkeyValueData->LoadFromFile( g_pFullFileSystem, "resource/flash/fontmapping.cfg", "game" );
	 
	// PC uses one font library containing all languages.
	const char *fontLib = pkeyValueData->GetString( "fontlib", "resource/flash/fontlib.swf" );

	SF::Ptr<FontLib> pFontLib = *new FontLib;
	m_pLoader->SetFontLib( pFontLib );

	SF::Ptr<MovieDef> pFontMovie = *m_pLoader->CreateMovie( fontLib );
	pFontLib->AddFontsFrom( pFontMovie, true );

	pFontMovie = *m_pLoader->CreateMovie( "resource/flash/fontlib_extra.swf" );
	pFontLib->AddFontsFrom( pFontMovie, true );

	SF::Ptr<FontMap> pFontMap = *new FontMap();

	// now connect each of the fonts with its exported name

	for ( KeyValues* piter = pkeyValueData->GetFirstTrueSubKey(); piter; piter = piter->GetNextTrueSubKey() )
	{
		const char* exportedName = piter->GetName();
		const char* fontName = piter->GetString( "font", "Arial" );
		const char* fontStyle = piter->GetString( "style", "Normal" );
		FontMap::MapFontFlags fontFlags = FontMap::MFF_Original;

		if ( !V_stricmp( fontStyle, "bold" ) )
		{
			fontFlags = FontMap::MFF_Bold;

		}
		else if ( !V_stricmp( fontStyle, "italic" ) )
		{
			fontFlags = FontMap::MFF_Bold;
		}
		else if ( !V_stricmp( fontStyle, "bolditalic" ) || !V_stricmp( fontStyle, "italicbold" ) )
		{
			fontFlags = FontMap::MFF_BoldItalic;
		}

		pFontMap->MapFont( exportedName, fontName, fontFlags );
	}

	m_pLoader->SetFontMap( pFontMap );
}

InitReturnVal_t ScaleformUIImpl::Init( void )
{
	MEM_ALLOC_CREDIT();

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

	Assert( m_pAllocator == NULL );
	m_pAllocator = new CScaleformSysAlloc();

	Assert( m_pSystem == NULL );
	m_pSystem = new System( m_pAllocator );

	Assert( m_pLoader == NULL );
	m_pLoader = new Loader();

	m_pLoader->SetLog( SF::Ptr<Log> ( *new ScaleformUILogging() ) );
	m_pLoader->SetFileOpener( SF::Ptr<ScaleformFileOpener> ( *new ScaleformFileOpener() ) );
	m_pLoader->SetTextClipboard( SF::Ptr<ScaleformClipboard> ( *new ScaleformClipboard() ) );
	m_pLoader->SetTranslator( SF::Ptr<ScaleformTranslatorAdapter> ( *new ScaleformTranslatorAdapter() ) );
	m_pLoader->SetAS2Support( SF::Ptr<AS2Support>( *new AS2Support() ) );
	
	SF::Log::SetGlobalLog( m_pLoader->GetLog() );

	// setup image handlers
	SF::Ptr<SF::GFx::ImageFileHandlerRegistry> pimgReg = *new SF::GFx::ImageFileHandlerRegistry();
#ifdef SF_ENABLE_LIBJPEG
	pimgReg->AddHandler( &SF::Render::JPEG::FileReader::Instance );
#endif 
#ifdef SF_ENABLE_LIBPNG
	pimgReg->AddHandler( &SF::Render::PNG::FileReader::Instance );
#endif
	pimgReg->AddHandler( &SF::Render::TGA::FileReader::Instance );
	pimgReg->AddHandler( &SF::Render::DDS::FileReader::Instance );
	m_pLoader->SetImageFileHandlerRegistry( pimgReg );

#if defined( PLATFORM_WINDOWS_PC )

	m_bTrySWFFirst = true;

#elif defined( _CERT )

	if ( CommandLine()->FindParm( "-tryswf" ) )
	{
		m_bTrySWFFirst = true;
	}
	else
	{
		m_bTrySWFFirst = false;
	}

#else

	if ( CommandLine()->FindParm( "-ignoreswf" ) )
	{
		m_bTrySWFFirst = false;
	}
	else
	{
		m_bTrySWFFirst = true;
	}

#endif

#if !defined( _CERT )
	if ( CommandLine()->FindParm( "-sfstats" ) )
		m_bPumpScaleformStats = true;
#endif


#if !defined( NO_STEAM )
	SteamAPI_InitSafe();
	g_SteamAPIContext.Init();
#endif // NO_STEAM

	InitValueImpl();
	InitTranslationImpl();
	InitRendererImpl();
	InitMovieImpl();
	InitHighLevelImpl();
	InitMovieSlotImpl();
	
	safezonex.InstallChangeCallback( safezonechanged, false );
	safezoney.InstallChangeCallback( safezonechanged, false );
	hud_scaling.InstallChangeCallback( safezonechanged, false );

	sf_ui_tint.InstallChangeCallback( ui_tint_changed, false );

	return nRetVal;
}

void ScaleformUIImpl::Shutdown( void )
{

// On Ps3, we crash on exit here.
// Should revisit to ensure it's not related to the IB/VB mesh cacheing optimisation
// But for pre-cert this hack will get us much better coverage testing at this point

#ifdef _PS3
        return; 
#endif

	if ( m_pSystem )
	{
		safezonex.RemoveChangeCallback( safezonechanged );
		safezoney.RemoveChangeCallback( safezonechanged );
		hud_scaling.RemoveChangeCallback( safezonechanged );

		sf_ui_tint.RemoveChangeCallback( ui_tint_changed );

		ShutdownTranslationImpl();

		ShutdownMovieSlotImpl();

		ShutdownHighLevelImpl();

		ShutdownMovieImpl();

		ShutdownRendererImpl();

		ShutdownValueImpl();

		m_pTranslatorAdapter = NULL;
		delete m_pLoader;
		delete m_pSystem;
		delete m_pAllocator;

		ClearMembers();

		ConVar_Unregister();

#if !defined( NO_STEAM )
		g_SteamAPIContext.Clear(); // Steam API context shutdown
#endif
	}

	BaseClass::Shutdown();
}

void* ScaleformUIImpl::QueryInterface( const char *pInterfaceName )
{
	if ( !Q_strncmp( pInterfaceName, SCALEFORMUI_INTERFACE_VERSION, Q_strlen( SCALEFORMUI_INTERFACE_VERSION ) + 1 ) )
	{
		return ( IScaleformUI* ) &ScaleformUIImpl::m_Instance;
	}

	return BaseClass::QueryInterface( pInterfaceName );
}

const AppSystemInfo_t* ScaleformUIImpl::GetDependencies( void )
{
	return BaseClass::GetDependencies();

}

void ScaleformUIImpl::LogPrintf( const char *format, ... )
{
#if !defined( _CERT )
	va_list al;
	va_start( al, format );

	SF::Ptr<Log> pLog = m_pLoader->GetLog();

	if ( pLog )
	{
		pLog->LogMessageVarg( SF::Log_Message, format, al );
	}

	pLog = NULL;
#endif
}

void SF_VerboseToggle( void );
void SF_VerboseOn( void );
void SF_VerboseOff( void );
static ConCommand dev_scaleform_verbose_toggle("dev_scaleform_verbose_toggle", SF_VerboseToggle, "Enable/disable Scaleform verbose mode.", FCVAR_DONTRECORD | FCVAR_DEVELOPMENTONLY );
static ConCommand dev_scaleform_verbose_on("dev_scaleform_verbose_on", SF_VerboseOn, "Enable Scaleform verbose mode.", FCVAR_DONTRECORD | FCVAR_DEVELOPMENTONLY );
static ConCommand dev_scaleform_verbose_off("dev_scaleform_verbose_off", SF_VerboseOff, "Disable Scaleform verbose mode.", FCVAR_DONTRECORD | FCVAR_DEVELOPMENTONLY );

bool ScaleformUIImpl::GetVerbose( void )
{
	if ( m_pLoader )
	{
		SF::Ptr<ActionControl> pActionControl = m_pLoader->GetActionControl();
		if ( pActionControl )
		{
			return ( ( pActionControl->GetActionFlags() & ActionControl::Action_Verbose ) != 0 );
		}
	}
	return false;
}

void ScaleformUIImpl::SetVerbose( bool bVerbose )
{
	if ( m_pLoader )
	{
		SF::Ptr<ActionControl> pActionControl = m_pLoader->GetActionControl();
		if ( !pActionControl )
		{
			pActionControl = *new ActionControl();
			m_pLoader->SetActionControl(pActionControl);
		}
		pActionControl->SetVerboseAction( bVerbose );
		pActionControl->SetLogAllFilenames( true );
	}
}

void SF_VerboseToggle( void )
{
	bool bWasVerbose = SFINST.GetVerbose();
	SFINST.SetVerbose( !bWasVerbose );
}

void SF_VerboseOn( void )
{
	SFINST.SetVerbose( true );
}

void SF_VerboseOff( void )
{
	SFINST.SetVerbose( false );
}

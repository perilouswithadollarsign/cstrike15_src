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
// Material editor
//=============================================================================

#include "vstdlib/cvar.h"
#include "appframework/vguimatsysapp.h"
#include "soundsystem/isoundsystem.h"
#include "nowindows.h"
#include "FileSystem.h"
#include "vgui/IVGui.h"
#include "vgui/ISystem.h"
//#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "vstdlib/iprocessutils.h"
#include "matsys_controls/matsyscontrols.h"
#include "datacache/idatacache.h"
#include "vgui_controls/frame.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "materialsystem/materialsystemutil.h"
#include "vgui/keycode.h"
#include "tier0/vprof.h"  
#include "inputsystem/iinputsystem.h"
#include "game_controls/igameuisystemmgr.h"
#include "console_logging.h"
#include "vscript/ivscript.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "tier1/interface.h"
#include "materialsystem/IMaterialProxy.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"

LINK_GAME_CONTROLS_LIB();


using namespace vgui;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
const MaterialSystem_Config_t *g_pMaterialSystemConfig;



//-----------------------------------------------------------------------------
// A material proxy that resets the base texture to use the dynamic texture
//-----------------------------------------------------------------------------
class CPerftestGameControlsProxy : public IMaterialProxy
{
public:
	CPerftestGameControlsProxy();
	virtual ~CPerftestGameControlsProxy(){};
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pProxyData );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar* m_BaseTextureVar;
};

CPerftestGameControlsProxy::CPerftestGameControlsProxy(): m_BaseTextureVar( NULL )
{
}

bool CPerftestGameControlsProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool bFoundVar;
	m_BaseTextureVar = pMaterial->FindVar( "$basetexture", &bFoundVar, false );
	return bFoundVar;
}

void CPerftestGameControlsProxy::OnBind( void *pProxyData )
{
	const char *pBaseTextureName = ( const char * )pProxyData;
	ITexture *pTexture = g_pMaterialSystem->FindTexture( pBaseTextureName, TEXTURE_GROUP_OTHER, true );
	m_BaseTextureVar->SetTextureValue( pTexture );
}

IMaterial *CPerftestGameControlsProxy::GetMaterial()
{
	return m_BaseTextureVar->GetOwningMaterial();
}

static CPerftestGameControlsProxy s_CPerftestGameControlsProxy;



//-----------------------------------------------------------------------------
// Factory to create dynamic material. ( Return in this case. )
//-----------------------------------------------------------------------------
class CPerftestMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	IMaterialProxy *CreateProxy( const char *proxyName );
	void DeleteProxy( IMaterialProxy *pProxy );
	CreateInterfaceFn GetFactory();
};
static CPerftestMaterialProxyFactory s_DynamicMaterialProxyFactory;

IMaterialProxy *CPerftestMaterialProxyFactory::CreateProxy( const char *proxyName )
{
	if ( Q_strcmp( proxyName, "GameControlsProxy" ) == NULL )
	{	
		return static_cast< IMaterialProxy * >( &s_CPerftestGameControlsProxy );
	}
	return NULL;
}

void CPerftestMaterialProxyFactory::DeleteProxy( IMaterialProxy *pProxy )
{
}

CreateInterfaceFn CPerftestMaterialProxyFactory::GetFactory()
{
	return Sys_GetFactoryThis();
}








//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CVguiPerfTestApp : public CVguiMatSysApp
{
	typedef CVguiMatSysApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual bool AppUsesReadPixels() { return true; }	

private:
	bool SetupSearchPaths();

	virtual const char *GetAppName() { return "VguiPerfTest"; }

	bool RunPerfTest( CMatRenderContextPtr &pRenderContext, const char *pMenuName );
	void CVguiPerfTestApp::PlayMenuSound( const char *pSoundFileName );

};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CVguiPerfTestApp );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CVguiPerfTestApp::Create()
{
	if ( !BaseClass::Create() )
		return false;

	AppSystemInfo_t appSystems[] = 
	{
		{ "vstdlib.dll",			PROCESS_UTILS_INTERFACE_VERSION },
		{ "vstdlib.dll",			EVENTSYSTEM_INTERFACE_VERSION }, // input event support
//		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
//		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },   // needed for sound system.
//		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "vscript.dll",			VSCRIPT_INTERFACE_VERSION },	 // scripting support
		{ "soundsystem.dll",		SOUNDSYSTEM_INTERFACE_VERSION }, // sound support

		{ "", "" }	// Required to terminate the list
	};

	AddSystem( g_pGameUISystemMgr, GAMEUISYSTEMMGR_INTERFACE_VERSION );
	
	return AddSystems( appSystems );
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CVguiPerfTestApp::PreInit( )
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	if ( !BaseClass::PreInit() )
		return false;

	// initialize interfaces
	CreateInterfaceFn appFactory = GetFactory(); 
	if (!vgui::VGui_InitMatSysInterfacesList( "VguiPerfTest", &appFactory, 1 ))
		return false;

	if ( !g_pFullFileSystem || !g_pMaterialSystem || !g_pVGui || !g_pVGuiSurface || !g_pMatSystemSurface || !g_pEventSystem )
	{
		Warning( "Perf Test App is missing a required interface!\n" );
		return false;
	}

	// Add paths...
	if ( !SetupSearchPaths() )
		return false;


	materials->SetMaterialProxyFactory( &s_DynamicMaterialProxyFactory );

	return true;
}

//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CVguiPerfTestApp::SetupSearchPaths()
{
	if ( !BaseClass::SetupSearchPaths( NULL, false, true ) )
		return false;

	return true;
}



// Replace first underscore (if any) with \0 and return
// This handles special mods like tf_movies, l4d_movies, tf_comics
// As a result, such mods will use the gpu_level settings etc from the base mod
void StripModSuffix2( char *pModName )
{
	int i = 0;
	while ( pModName[i] != '\0' )	// Walk to the end of the string
	{
		if ( pModName[i] == '_')	// If we hit an underscore
		{
			pModName[i] = '\0';		// Terminate the string here and bail out
			return;
		}
		i++;
	}
}

//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CVguiPerfTestApp::Main()
{
	CConsoleLoggingListener consoleLoggingListener;
	LoggingSystem_PushLoggingState();
	LoggingSystem_RegisterLoggingListener( &consoleLoggingListener );
	LoggingSystem_SetChannelSpewLevelByTag( "Console", LS_MESSAGE );
	LoggingSystem_SetChannelSpewLevelByTag( "Developer", LS_ERROR );
	LoggingSystem_SetChannelSpewLevelByTag( "DeveloperVerbose", LS_ERROR );

	CSimpleLoggingListener simpleLoggingListner( false );
	LoggingSystem_RegisterLoggingListener( &simpleLoggingListner );

	g_pMaterialSystem->ModInit();
	if (!SetVideoMode())
		return 0;

	g_pDataCache->SetSize( 64 * 1024 * 1024 );

	g_pMaterialSystemConfig = &g_pMaterialSystem->GetCurrentConfigForVideoCard();

	// configuration settings
	vgui::system()->SetUserConfigFile( "VguiPerfTest.vdf", "EXECUTABLE_PATH");

	// If you load a vgui panel you must load up a scheme.

	// load scheme
	if (!vgui::scheme()->LoadSchemeFromFile( "resource/BoxRocket.res", "VguiPerfTest" ))
	{
		Assert( 0 );
	}

	// load the boxrocket localization file
	g_pVGuiLocalize->AddFile( "resource/boxrocket_%language%.txt" );

	// load the mod localization file
	// Construct the mod name so we can use the mod-specific encrypted config files
	char pModPath[MAX_PATH];
	V_snprintf( pModPath, sizeof(pModPath), "" );
	g_pFullFileSystem->GetSearchPath( "MOD", false, pModPath, sizeof( pModPath ) );

	char pModName[32];
	V_StripTrailingSlash( pModPath );
	V_FileBase( pModPath, pModName, sizeof( pModName ) );
	StripModSuffix2( pModName );

	// Language file is a file of the form "resource/nimbus_%language%.txt"
	CUtlString pModLanguageFile;
	pModLanguageFile += "resource/";
	pModLanguageFile += pModName;
	pModLanguageFile += "_%language%.txt";

	g_pVGuiLocalize->AddFile( pModLanguageFile.Get() );



	// start vgui
	g_pVGui->Start();

	// add our main window
	//vgui::PHandle hMainPanel;

	// load the base localization file
	g_pVGuiLocalize->AddFile( "Resource/valve_%language%.txt" );
	g_pFullFileSystem->AddSearchPath( "platform", "PLATFORM" );
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt");


	// run app frame loop
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	const char *pGuiFileName = CommandLine()->ParmValue( "-gui" );

	bool bReload = true;
	while ( bReload )
	{
		bReload = RunPerfTest( pRenderContext, pGuiFileName );
	}

	g_pMaterialSystem->ModShutdown();

	LoggingSystem_PopLoggingState();
	return 1;
}

//-----------------------------------------------------------------------------
// returns true if ui should reload
//-----------------------------------------------------------------------------
bool CVguiPerfTestApp::RunPerfTest( CMatRenderContextPtr &pRenderContext, const char *pMenuName )
{
	g_pGameUISystemMgr->Shutdown();
	g_pGameUISystemMgr->Init();
	g_pMaterialSystem->BeginRenderTargetAllocation();
	g_pGameUISystemMgr->InitRenderTargets();
	g_pMaterialSystem->EndRenderTargetAllocation();

	if ( !pMenuName )
	{
		pMenuName = "menu_1";
	}
	
	g_pGameUISystemMgr->LoadGameUIScreen( KeyValues::AutoDeleteInline(
		new KeyValues( pMenuName ) ) );
	

	g_VProfCurrentProfile.Reset();
	g_VProfCurrentProfile.ResetPeaks();
	g_VProfCurrentProfile.Start();

	float m_flTotalTime = 0;
	float flStartTime = Plat_FloatTime();
	{
		KeyValues *kvEvent = new KeyValues( "StartPlaying" );
		kvEvent->SetFloat( "time", 0 );
		g_pGameUISystemMgr->SendEventToAllScreens( kvEvent );
	}
	int frameCount = 0;
	bool bReload = false;
	bool bAbleToAdvance = true;

	vgui::VPANEL root = g_pVGuiSurface->GetEmbeddedPanel();
	g_pVGuiSurface->Invalidate( root );


	while ( g_pVGui->IsRunning() )
	{
		AppPumpMessages();
		
		
		g_pSoundSystem->Update( Plat_FloatTime() );

		g_pMaterialSystem->BeginFrame( 0 );

		pRenderContext->ClearColor4ub( 0, 0, 0, 255 ); 
		//pRenderContext->ClearColor4ub( 76, 88, 68, 255 ); 
		pRenderContext->ClearBuffers( true, true );

		// Handle all input to game ui.
		g_pGameUISystemMgr->RunFrame();

		// Must call run frame for close button to work.
		g_pVGui->RunFrame(); // need to load a default scheme for this to work.
		//g_pVGuiSurface->PaintTraverseEx( root, true );

		Rect_t viewport;
		pRenderContext->GetViewport( viewport.x, viewport.y, viewport.width, viewport.height );

		float flCurrentRenderTime = Plat_FloatTime() - flStartTime;
		g_pGameUISystemMgr->Render( viewport, DmeTime_t( flCurrentRenderTime ) );
		
		g_pMaterialSystem->EndFrame();
		g_pMaterialSystem->SwapBuffers(); 

		g_VProfCurrentProfile.MarkFrame();

		frameCount++;
		//if ( frameCount >= 5000 )
		//	break;

		if ( g_pInputSystem->IsButtonDown( KEY_ESCAPE ) )
		{
			 bReload = true;
			 break;
		}
		else if ( g_pInputSystem->IsButtonDown( KEY_PAD_PLUS ) )
		{
			m_flTotalTime = 0;
			{
				KeyValues *kvEvent = new KeyValues( "StartPlaying" );
				kvEvent->SetFloat( "time", m_flTotalTime );
				g_pGameUISystemMgr->SendEventToAllScreens( kvEvent );
			}
		}
		else if ( g_pInputSystem->IsButtonDown( KEY_PAD_MINUS ) )
		{		
			KeyValues *kvEvent = new KeyValues( "StopPlaying" );
			g_pGameUISystemMgr->SendEventToAllScreens( kvEvent );
		}
		else if ( g_pInputSystem->IsButtonDown( KEY_PAD_DIVIDE ) )
		{
			KeyValues *kvEvent = new KeyValues( "ShowCursorCoords" );
			g_pGameUISystemMgr->SendEventToAllScreens( kvEvent );
		}
		else if ( g_pInputSystem->IsButtonDown( KEY_PAD_9 ) )
		{
			KeyValues *kvEvent = new KeyValues( "ShowGraphicName" );
			g_pGameUISystemMgr->SendEventToAllScreens( kvEvent );
		}
		else if ( g_pInputSystem->IsButtonDown( KEY_PAD_MULTIPLY ) )
		{
			if ( bAbleToAdvance )
			{
				KeyValues *kvEvent = new KeyValues( "AdvanceState" );
				g_pGameUISystemMgr->SendEventToAllScreens( kvEvent );
			}
			bAbleToAdvance = false;
		}
		else if ( !g_pInputSystem->IsButtonDown( KEY_PAD_MULTIPLY ) )
		{
			bAbleToAdvance = true;
		}
		
	}

	g_VProfCurrentProfile.Stop();
	g_VProfCurrentProfile.OutputReport( VPRT_FULL & ~VPRT_HIERARCHY, NULL );

	return bReload;
	
}



//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CVguiPerfTestApp::PlayMenuSound( const char *pSoundFileName )
{
	if ( pSoundFileName == NULL || pSoundFileName[ 0 ] == '\0' )
		return;

	if ( !Q_stristr( pSoundFileName, ".wav" ) )
		return;

	char filename[ 256 ];
	sprintf( filename, "sound/%s", pSoundFileName );
	CAudioSource *pAudioSource = g_pSoundSystem->FindOrAddSound( filename );
	if ( pAudioSource == NULL )
		return;

	g_pSoundSystem->PlaySound( pAudioSource, 1.0f, NULL );
}




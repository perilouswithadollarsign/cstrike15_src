//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//=============================================================================
#ifdef _WIN32

#include "appframework/materialsystem2app.h"
#include "FileSystem.h"
#include "materialsystem2/IMaterialSystem2.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "filesystem_init.h"
#include "inputsystem/iinputsystem.h"
#include "tier2/tier2.h"
#include "rendersystem/irenderdevice.h"
#include "rendersystem/irenderhardwareconfig.h"
#include "vstdlib/jobthread.h"
//#include "videocfg/videocfg.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMaterialSystem2App::CMaterialSystem2App()
{
	m_RenderFactory = NULL;
	m_hSwapChain = SWAP_CHAIN_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Creates render system
//-----------------------------------------------------------------------------
bool CMaterialSystem2App::AddRenderSystem()
{
	bool bIsVistaOrHigher = IsPlatformWindowsPC() && ( Plat_GetOSVersion() >= PLAT_OS_VERSION_VISTA );
	const char *pShaderDLL = !IsPlatformX360() ? CommandLine()->ParmValue( "-rendersystemdll" ) : NULL;
	if ( !pShaderDLL )
	{
		if ( IsPlatformWindowsPC() )
		{
			pShaderDLL = "rendersystemdx11.dll";
		}
		else if ( IsPlatformX360() )
		{
			pShaderDLL = "rendersystemdx9_360.dll";
		}
		else
		{
			pShaderDLL = "rendersystemgl.dll";
		}
	}

	// Disallow dx11 on XP machines
	if ( !bIsVistaOrHigher && !Q_stricmp( pShaderDLL, "rendersystemdx11.dll" ) )
	{
		pShaderDLL = "rendersystemdx9.dll";
	}

	AppModule_t module = LoadModule( pShaderDLL );
	if ( module == APP_MODULE_INVALID )
	{
		if ( IsPlatformWindowsPC() )
		{
			pShaderDLL = "rendersystemdx9.dll";
			module = LoadModule( pShaderDLL );
		}
		if ( module == APP_MODULE_INVALID )
		{
			pShaderDLL = "rendersystemempty.dll";
			module = LoadModule( pShaderDLL );
			if ( module == APP_MODULE_INVALID )
				return false;
		}
	}
	AddSystem( module, RENDER_DEVICE_MGR_INTERFACE_VERSION );

	if ( IsPlatformX360() )
	{
		m_nRenderSystem = RENDER_SYSTEM_X360;
	}
	else if ( V_stristr( pShaderDLL, "rendersystemgl" ) != NULL )
	{
		m_nRenderSystem = RENDER_SYSTEM_GL;
	}
	else if ( V_stristr( pShaderDLL, "rendersystemdx11" ) != NULL )
	{
		m_nRenderSystem = RENDER_SYSTEM_DX11;
	}
	else
	{
		m_nRenderSystem = RENDER_SYSTEM_DX9;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CMaterialSystem2App::Create()
{
	if ( !AddRenderSystem() )
		return false;

	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "materialsystem2.dll",	MATERIAL_SYSTEM2_INTERFACE_VERSION },

		// Required to terminate the list
		{ "", "" }
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	const char *pNumThreadsString = CommandLine()->ParmValue( "-threads" );
	if ( pNumThreadsString )
	{
		m_nThreadCount = atoi( pNumThreadsString );
	}
	else
	{
		const CPUInformation &cpuInfo = GetCPUInformation();
		m_nThreadCount = cpuInfo.m_nLogicalProcessors - 1;		// one core for main thread
	}

	if ( m_nThreadCount > 0 )
	{
		ThreadPoolStartParams_t sparms( false, m_nThreadCount );
		g_pThreadPool->Start( sparms );
	}

	return true;
}

void CMaterialSystem2App::Destroy()
{
}


//-----------------------------------------------------------------------------
// Pump messages
//-----------------------------------------------------------------------------
void CMaterialSystem2App::AppPumpMessages()
{
	g_pInputSystem->PollInputState();
}


//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CMaterialSystem2App::SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool )
{
	if ( !BaseClass::SetupSearchPaths( pStartingDir, bOnlyUseStartingDir, bIsTool ) )
		return false;

	g_pFullFileSystem->AddSearchPath( GetGameInfoPath(), "SKIN", PATH_ADD_TO_HEAD );
	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CMaterialSystem2App::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pMaterialSystem2 || !g_pRenderDeviceMgr || !g_pInputSystem )
	{
		Warning( "CMaterialSystem2App::PreInit: Unable to connect to necessary interface!\n" );
		return false;
	}

	// Needed to set up the device prior to Init() of other systems
	g_pRenderDeviceMgr->InstallRenderDeviceSetup( this );

	// Add paths...
	// NOTE: Not sure if I should have this here or not. For now, my test
	// is rendersystem test, which wants to do it itself.
//	if ( !SetupSearchPaths( NULL, false, true ) )
//		return false;

	return true; 
}


//-----------------------------------------------------------------------------
// Replace first underscore (if any) with \0 and return
// This handles special mods like tf_movies, l4d_movies, tf_comics
// As a result, such mods will use the gpu_level settings etc from the base mod
//-----------------------------------------------------------------------------
static void StripModSuffix( char *pModName )
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
// Configures the application for the specific mod we're running
//-----------------------------------------------------------------------------
void CMaterialSystem2App::ApplyModSettings( )
{
	/* PORTFIXME
	char pModPath[MAX_PATH];
	V_snprintf( pModPath, sizeof(pModPath), "" );
	g_pFullFileSystem->GetSearchPath( "MOD", false, pModPath, sizeof( pModPath ) );

	// Construct the mod name so we can use the mod-specific encrypted config files
	char pModName[32];
	V_StripTrailingSlash( pModPath );
	V_FileBase( pModPath, pModName, sizeof( pModName ) );
	StripModSuffix( pModName );

	// Just use the highest levels in non-game apps
	UpdateSystemLevel( CPU_LEVEL_HIGH, GPU_LEVEL_VERYHIGH, MEM_LEVEL_HIGH, GPU_MEM_LEVEL_HIGH, false, pModName );
	*/
}


//-----------------------------------------------------------------------------
// Create our device + window
//-----------------------------------------------------------------------------
bool CMaterialSystem2App::CreateRenderDevice()
{
	// Create a device for this adapter
	int nAdapterCount = g_pRenderDeviceMgr->GetAdapterCount();
	int nAdapter = CommandLine()->ParmValue( "-adapter", 0 );
	if ( nAdapter >= nAdapterCount )
	{
		Warning( "Specified too high an adapter number on the commandline (%d/%d)!\n", nAdapter, nAdapterCount );
		return false;
	}

	int nFlags = 0;
	bool bResizing = !IsConsoleApp() && ( CommandLine()->CheckParm( "-resizing" ) != NULL );	
	if ( bResizing )
	{
		nFlags |= RENDER_CREATE_DEVICE_RESIZE_WINDOWS;
	}

	m_RenderFactory = g_pRenderDeviceMgr->CreateDevice( nAdapter, nFlags );
	if ( !m_RenderFactory )
	{
		Warning( "Unable to set mode!\n" );
		return false;
	}

	// Let other systems see the render device
	AddNonAppSystemFactory( m_RenderFactory );
	ReconnectSystems( RENDER_DEVICE_INTERFACE_VERSION );
	ReconnectSystems( RENDER_HARDWARECONFIG_INTERFACE_VERSION );

	g_pRenderDevice = (IRenderDevice*)m_RenderFactory( RENDER_DEVICE_INTERFACE_VERSION, NULL );
	g_pRenderHardwareConfig = (IRenderHardwareConfig*)m_RenderFactory( RENDER_HARDWARECONFIG_INTERFACE_VERSION, NULL );

	// Fixup the platform level
	if ( m_nRenderSystem == RENDER_SYSTEM_DX11 )
	{
		if ( g_pRenderHardwareConfig->GetDXSupportLevel() < 100 )
		{
			m_nRenderSystem = RENDER_SYSTEM_DX9;
		}
	}

	if ( !IsConsoleApp() )
		return CreateMainWindow( bResizing );
	return CreateMainConsoleWindow();
}


//-----------------------------------------------------------------------------
// Create our window
//-----------------------------------------------------------------------------
bool CMaterialSystem2App::PostInit( )
{
	if ( !BaseClass::PostInit() )
		return false;

	// Set up mod settings 
	ApplyModSettings();
	return true;
}


void CMaterialSystem2App::PreShutdown()
{
	if ( g_pInputSystem )
	{
		g_pInputSystem->DetachFromWindow( );
	}

	if ( g_pRenderDevice )
	{
		g_pRenderDevice->DestroySwapChain( m_hSwapChain );
		m_hSwapChain = SWAP_CHAIN_HANDLE_INVALID;
	}

	BaseClass::PreShutdown();
}


//-----------------------------------------------------------------------------
// Creates the main 3d window
//-----------------------------------------------------------------------------
bool CMaterialSystem2App::CreateMainWindow( bool bResizing )
{
	// NOTE: This could be placed into a separate function
	// Create a main 3d-capable window
	int nWidth = 1280;
	int nHeight = IsPlatformX360() ? 720 : 960;
	bool bFullscreen = ( CommandLine()->CheckParm( "-fullscreen" ) != NULL );

	const char *pArg;
	if ( CommandLine()->CheckParm( "-width", &pArg ) )
	{
		nWidth = atoi( pArg );
	}
	if ( CommandLine()->CheckParm( "-height", &pArg ) )
	{
		nHeight = atoi( pArg );
	}

	m_hSwapChain = Create3DWindow( GetAppName(), nWidth, nHeight, bResizing, bFullscreen, true );
	return ( m_hSwapChain != SWAP_CHAIN_HANDLE_INVALID );
}

bool CMaterialSystem2App::CreateMainConsoleWindow()
{
	RenderDeviceInfo_t mode;
	mode.m_DisplayMode.m_nWidth = 512;
	mode.m_DisplayMode.m_nHeight = 512;
	mode.m_DisplayMode.m_Format = IMAGE_FORMAT_RGBA8888;
	mode.m_DisplayMode.m_nRefreshRateNumerator = 60;
	mode.m_DisplayMode.m_nRefreshRateDenominator = 1;
	mode.m_bFullscreen = false;
	mode.m_nBackBufferCount = 1;
	mode.m_bWaitForVSync = false;

#ifdef PLATFORM_WINDOWS_PC
	m_hSwapChain = g_pRenderDevice->CreateSwapChain( Plat_GetShellWindow(), mode );
	return ( m_hSwapChain != SWAP_CHAIN_HANDLE_INVALID );
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Creates a 3d-capable window
//-----------------------------------------------------------------------------
SwapChainHandle_t CMaterialSystem2App::Create3DWindow( const char *pTitle, int nWidth, int nHeight, bool bResizing, bool bFullscreen, bool bAcceptsInput )
{
	PlatWindow_t hWnd = (PlatWindow_t)CreateAppWindow( GetAppInstance(), pTitle, !bFullscreen, nWidth, nHeight, bResizing );
	if ( !hWnd )
		return SWAP_CHAIN_HANDLE_INVALID;

	// By default, everything will just use this one swap chain.
	RenderDeviceInfo_t mode;
	mode.m_DisplayMode.m_nWidth = nWidth;
	mode.m_DisplayMode.m_nHeight = nHeight;
	mode.m_DisplayMode.m_Format = IMAGE_FORMAT_RGBA8888;
	mode.m_DisplayMode.m_nRefreshRateNumerator = 60;
	mode.m_DisplayMode.m_nRefreshRateDenominator = 1;
	mode.m_bFullscreen = bFullscreen;
	mode.m_nBackBufferCount = bFullscreen ? 2 : 1;
	mode.m_bWaitForVSync = ( CommandLine()->CheckParm( "-vsync" ) != NULL );
	SwapChainHandle_t hSwapChain = g_pRenderDevice->CreateSwapChain( hWnd, mode );

	if ( bAcceptsInput )
	{
		g_pInputSystem->AttachToWindow( (void*)hWnd );
	}

	return hSwapChain;
}


//-----------------------------------------------------------------------------
// Returns the window associated with a swap chain
//-----------------------------------------------------------------------------
PlatWindow_t CMaterialSystem2App::GetAppWindow()
{
	if ( !m_hSwapChain || !g_pRenderDevice )
		return PLAT_WINDOW_INVALID;
	return g_pRenderDevice->GetSwapChainWindow( m_hSwapChain );
}



#endif // _WIN32


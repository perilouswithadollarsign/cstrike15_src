//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//=============================================================================
#ifdef _WIN32

#include "appframework/matsysapp.h"
#include "FileSystem.h"
#include "materialsystem/IMaterialSystem.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "filesystem_init.h"
#include "inputsystem/iinputsystem.h"
#include "tier2/tier2.h"
#include "videocfg/videocfg.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMatSysApp::CMatSysApp()
{
}


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CMatSysApp::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },

		// Required to terminate the list
		{ "", "" }
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	IMaterialSystem *pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );

	if ( !pMaterialSystem )
	{
		Warning( "CMatSysApp::Create: Unable to connect to necessary interface!\n" );
		return false;
	}

	pMaterialSystem->SetShaderAPI( "shaderapidx9.dll" );
	return true;
}

void CMatSysApp::Destroy()
{
}


//-----------------------------------------------------------------------------
// Pump messages
//-----------------------------------------------------------------------------
void CMatSysApp::AppPumpMessages()
{
	g_pInputSystem->PollInputState();
}


//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CMatSysApp::SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool )
{
	if ( !BaseClass::SetupSearchPaths( pStartingDir, bOnlyUseStartingDir, bIsTool ) )
		return false;

	g_pFullFileSystem->AddSearchPath( GetGameInfoPath(), "SKIN", PATH_ADD_TO_HEAD );
	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CMatSysApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pMaterialSystem || !g_pInputSystem )
	{
		Warning( "CMatSysApp::PreInit: Unable to connect to necessary interface!\n" );
		return false;
	}

	// Add paths...
	if ( !SetupSearchPaths( NULL, false, true ) )
		return false;

	const char *pArg;
	int iWidth = 1024;
	int iHeight = 768;
	bool bWindowed = (CommandLine()->CheckParm( "-fullscreen" ) == NULL);
	if (CommandLine()->CheckParm( "-width", &pArg ))
	{
		iWidth = atoi( pArg );
	}
	if (CommandLine()->CheckParm( "-height", &pArg ))
	{
		iHeight = atoi( pArg );
	}

	m_nWidth = iWidth;
	m_nHeight = iHeight;
	m_HWnd = CreateAppWindow( GetAppInstance(), GetAppName(), bWindowed, iWidth, iHeight, false );
	if ( !m_HWnd )
		return false;

	g_pInputSystem->AttachToWindow( m_HWnd );

	// NOTE: If we specifically wanted to use a particular shader DLL, we set it here...
	//m_pMaterialSystem->SetShaderAPI( "shaderapidx8" );

	// Get the adapter from the command line....
	const char *pAdapterString;
	int adapter = 0;
	if (CommandLine()->CheckParm( "-adapter", &pAdapterString ))
	{
		adapter = atoi( pAdapterString );
	}

	int adapterFlags = 0;
	if ( CommandLine()->CheckParm( "-ref" ) )
	{
		adapterFlags |= MATERIAL_INIT_REFERENCE_RASTERIZER;
	}
	if ( AppUsesReadPixels() )
	{
		adapterFlags |= MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE;
	}

	g_pMaterialSystem->SetAdapter( adapter, adapterFlags );

	return true; 
}

void CMatSysApp::PostShutdown()
{
	if ( g_pInputSystem )
	{
		g_pInputSystem->DetachFromWindow( );
	}

	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Gets the window size
//-----------------------------------------------------------------------------
int CMatSysApp::GetWindowWidth() const
{
	return m_nWidth;
}

int CMatSysApp::GetWindowHeight() const
{
	return m_nHeight;
}


//-----------------------------------------------------------------------------
// Returns the window
//-----------------------------------------------------------------------------
void* CMatSysApp::GetAppWindow()
{
	return m_HWnd;
}

// Replace first underscore (if any) with \0 and return
// This handles special mods like tf_movies, l4d_movies, tf_comics
// As a result, such mods will use the gpu_level settings etc from the base mod
void StripModSuffix( char *pModName )
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
// Sets the video mode
//-----------------------------------------------------------------------------
bool CMatSysApp::SetVideoMode( )
{
	MaterialSystem_Config_t config;
	if ( CommandLine()->CheckParm( "-fullscreen" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, false );
	}
	else
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	}

	if ( CommandLine()->CheckParm( "-resizing" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, true );
	}

	if ( CommandLine()->CheckParm( "-mat_vsync" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, false );
	}

	config.m_nAASamples = CommandLine()->ParmValue( "-mat_antialias", 1 );
	config.m_nAAQuality = CommandLine()->ParmValue( "-mat_aaquality", 0 );
	
	if ( CommandLine()->FindParm( "-csm_quality_level" ) )
	{
		int nCSMQuality = CommandLine()->ParmValue( "-csm_quality_level", CSMQUALITY_VERY_LOW );
		config.m_nCSMQuality = (CSMQualityMode_t)clamp( nCSMQuality, CSMQUALITY_VERY_LOW, CSMQUALITY_TOTAL_MODES - 1 );
	}
	
	config.m_VideoMode.m_Width = config.m_VideoMode.m_Height = 0;
	config.m_VideoMode.m_Format = IMAGE_FORMAT_BGRX8888;
	config.m_VideoMode.m_RefreshRate = 0;
	config.SetFlag(	MATSYS_VIDCFG_FLAGS_STENCIL, true );

	bool modeSet = g_pMaterialSystem->SetMode( m_HWnd, config );
	if (!modeSet)
	{
		Error( "Unable to set mode\n" );
		return false;
	}

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

	g_pMaterialSystem->OverrideConfig( config, false );
	return true;
}

#endif // _WIN32


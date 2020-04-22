//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: An application framework 
//
//=============================================================================//

#include "appframework/AppFramework.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "interface.h"
#include "filesystem.h"
#include "appframework/IAppSystemGroup.h"
#include "filesystem_init.h"
#include "vstdlib/cvar.h"
#include "tier2/tier2.h"

#ifdef _X360
#include "xbox/xbox_win32stubs.h"
#include "xbox/xbox_console.h"
#include "xbox/xbox_launch.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Globals...
//-----------------------------------------------------------------------------
void* s_HInstance;

static CSimpleWindowsLoggingListener s_SimpleWindowsLoggingListener;
static CSimpleLoggingListener s_SimpleLoggingListener;
ILoggingListener *g_pDefaultLoggingListener = &s_SimpleLoggingListener;


//-----------------------------------------------------------------------------
// HACK: Since I don't want to refit vgui yet...
//-----------------------------------------------------------------------------
void *GetAppInstance()
{
	return s_HInstance;
}


//-----------------------------------------------------------------------------
// Sets the application instance, should only be used if you're not calling AppMain.
//-----------------------------------------------------------------------------
void SetAppInstance( void* hInstance )
{
	s_HInstance = hInstance;
}

//-----------------------------------------------------------------------------
// Version of AppMain used by windows applications
//-----------------------------------------------------------------------------
int AppMain( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	g_pDefaultLoggingListener = &s_SimpleWindowsLoggingListener;
	s_HInstance = hInstance;

#ifdef WIN32
	// Prepend the module filename since most apps expect arg 0 to be that.
	char szModuleFilename[MAX_PATH];
	Plat_GetModuleFilename( szModuleFilename, sizeof( szModuleFilename ) );
	int nAllocLen = strlen( lpCmdLine ) + strlen( szModuleFilename ) + 4;
	char *pNewCmdLine = new char[nAllocLen];	// 2 for quotes, 1 for a space, and 1 for a null-terminator.
	_snprintf( pNewCmdLine, nAllocLen, "\"%s\" %s", szModuleFilename, lpCmdLine );

	// Setup ICommandLine.
	CommandLine()->CreateCmdLine( pNewCmdLine );
	delete [] pNewCmdLine;
#else
	CommandLine()->CreateCmdLine( lpCmdLine );	
#endif
	
	return pAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Version of AppMain used by console applications
//-----------------------------------------------------------------------------
int AppMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	g_pDefaultLoggingListener = &s_SimpleLoggingListener;
	s_HInstance = NULL;
	CommandLine()->CreateCmdLine( argc, argv );

	return pAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Used to startup/shutdown the application
//-----------------------------------------------------------------------------
int AppStartup( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	g_pDefaultLoggingListener = &s_SimpleWindowsLoggingListener;
	s_HInstance = hInstance;
	CommandLine()->CreateCmdLine( lpCmdLine );

	return pAppSystemGroup->Startup();
}

int AppStartup( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	g_pDefaultLoggingListener = &s_SimpleLoggingListener;
	s_HInstance = NULL;
	CommandLine()->CreateCmdLine( argc, argv );

	return pAppSystemGroup->Startup();
}

void AppShutdown( CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );
	pAppSystemGroup->Shutdown();
}


//-----------------------------------------------------------------------------
//
// Default implementation of an application meant to be run using Steam
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSteamApplication::CSteamApplication( CSteamAppSystemGroup *pAppSystemGroup )
{
	m_pChildAppSystemGroup = pAppSystemGroup;
	m_pFileSystem = NULL;
	m_bSteam = false;
}

//-----------------------------------------------------------------------------
// Create necessary interfaces
//-----------------------------------------------------------------------------
bool CSteamApplication::Create()
{
	FileSystem_SetErrorMode( FS_ERRORMODE_AUTO );

	char pFileSystemDLL[MAX_PATH];
	if ( !GetFileSystemDLLName( pFileSystemDLL, MAX_PATH, m_bSteam ) )
		return false;
	
	FileSystem_SetupSteamInstallPath();

	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, CVAR_INTERFACE_VERSION );

	AppModule_t fileSystemModule = LoadModule( pFileSystemDLL );
	m_pFileSystem = (IFileSystem*)AddSystem( fileSystemModule, FILESYSTEM_INTERFACE_VERSION );
	if ( !m_pFileSystem )
	{
		if( !IsPS3() )
			Error( "Unable to load %s", pFileSystemDLL );
		return false;
	}

	return true;
}

bool CSteamApplication::GetFileSystemDLLName( char *pOut, int nMaxBytes, bool &bIsSteam )
{
	return FileSystem_GetFileSystemDLLName( pOut, nMaxBytes, bIsSteam ) == FS_OK;
}

//-----------------------------------------------------------------------------
// The file system pointer is invalid at this point
//-----------------------------------------------------------------------------
void CSteamApplication::Destroy()
{
	m_pFileSystem = NULL;
}

//-----------------------------------------------------------------------------
// Pre-init, shutdown
//-----------------------------------------------------------------------------
bool CSteamApplication::PreInit()
{
	return true;
}

void CSteamApplication::PostShutdown()
{
}

//-----------------------------------------------------------------------------
// Run steam main loop
//-----------------------------------------------------------------------------
int CSteamApplication::Main()
{
	// Now that Steam is loaded, we can load up main libraries through steam
	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return 0;

	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Use this version in cases where you can't control the main loop and
// expect to be ticked
//-----------------------------------------------------------------------------
int CSteamApplication::Startup()
{
	int nRetVal = BaseClass::Startup();
	if ( GetCurrentStage() != RUNNING )
		return nRetVal;

	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return 0;

	// Now that Steam is loaded, we can load up main libraries through steam
	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Startup();
}

void CSteamApplication::Shutdown()
{
	m_pChildAppSystemGroup->Shutdown();
	BaseClass::Shutdown();
}


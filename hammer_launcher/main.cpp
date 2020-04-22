//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Launcher for hammer, which is sitting in its own DLL
//
//===========================================================================//

#include <windows.h>
#include <eh.h>
#include "appframework/AppFramework.h"
#include "ihammer.h"
#include "tier0/dbg.h"
#include "vstdlib/cvar.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "filesystem_init.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "vgui/ivgui.h"
#include "vgui/isurface.h"
#include "inputsystem/iinputsystem.h"
#include "tier0/icommandline.h"
#include "SteamWriteMinidump.h"
#include "p4lib/ip4.h"

//-----------------------------------------------------------------------------
// Global systems
//-----------------------------------------------------------------------------
IHammer *g_pHammer;
IFileSystem *g_pFileSystem;

extern "C" void WriteMiniDumpUsingExceptionInfo
( 
 unsigned int			uStructuredExceptionCode, 
struct _EXCEPTION_POINTERS * pExceptionInfo
	)
{
	// TODO: dynamically set the minidump comment from contextual info about the crash (i.e current VPROF node)?
	SteamWriteMiniDumpUsingExceptionInfoWithBuildId( uStructuredExceptionCode, pExceptionInfo, 0 );
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CHammerApp : public CAppSystemGroup
{
public:
	// Methods of IApplication
	virtual bool Create( );
	virtual bool PreInit( );
	virtual int Main( );
	virtual void PostShutdown();
	virtual void Destroy();

private:
	int	MainLoop();
};


//-----------------------------------------------------------------------------
// Define the application object
//-----------------------------------------------------------------------------
CHammerApp	g_ApplicationObject;
DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( g_ApplicationObject );

static CSimpleWindowsLoggingListener s_SimpleWindowsLoggingListener;

//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CHammerApp::Create()
{
	LoggingSystem_PushLoggingState();
	LoggingSystem_RegisterLoggingListener( &s_SimpleWindowsLoggingListener );

	// Save some memory so engine/hammer isn't so painful
	CommandLine()->AppendParm( "-disallowhwmorph", NULL );

	IAppSystem *pSystem;

	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	pSystem = AddSystem( cvarModule, CVAR_INTERFACE_VERSION );
	if ( !pSystem )
		return false;
	
	bool bSteam;
	char pFileSystemDLL[MAX_PATH];
	if ( FileSystem_GetFileSystemDLLName( pFileSystemDLL, MAX_PATH, bSteam ) != FS_OK )
		return false;

	FileSystem_SetupSteamInstallPath();

	AppModule_t fileSystemModule = LoadModule( pFileSystemDLL );
	g_pFileSystem = (IFileSystem*)AddSystem( fileSystemModule, FILESYSTEM_INTERFACE_VERSION );

	AppSystemInfo_t appSystems[] = 
	{
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "datacache.dll",			STUDIO_DATA_CACHE_INTERFACE_VERSION },
		{ "vguimatsurface.dll",		VGUI_SURFACE_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "p4lib.dll",				P4_INTERFACE_VERSION },
		{ "hammer_dll.dll",			INTERFACEVERSION_HAMMER },
		{ "", "" }	// Required to terminate the list
	};

	AppSystemInfo_t appSystemsNoP4[] = 
	{
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "datacache.dll",			STUDIO_DATA_CACHE_INTERFACE_VERSION },
		{ "vguimatsurface.dll",		VGUI_SURFACE_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "hammer_dll.dll",			INTERFACEVERSION_HAMMER },
		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( CommandLine()->FindParm( "-nop4" ) ? appSystemsNoP4 : appSystems ) ) 
		return false;

	// Connect to interfaces loaded in AddSystems that we need locally
	g_pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	g_pHammer = (IHammer*)FindSystem( INTERFACEVERSION_HAMMER );
	g_pDataCache = (IDataCache*)FindSystem( DATACACHE_INTERFACE_VERSION );
	g_pInputSystem = (IInputSystem*)FindSystem( INPUTSYSTEM_INTERFACE_VERSION );
	p4 = ( IP4 * )FindSystem( P4_INTERFACE_VERSION );

	// This has to be done before connection.
	g_pMaterialSystem->SetShaderAPI( "shaderapidx9.dll" );

	return true;
}

void CHammerApp::Destroy()
{
	LoggingSystem_PopLoggingState();

	g_pFileSystem = NULL;
	g_pMaterialSystem = NULL;
	g_pDataCache = NULL;
	g_pHammer = NULL;
	g_pInputSystem = NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CHammerApp::PreInit( )
{
	if ( !g_pHammer->InitSessionGameConfig( GetVProjectCmdLineValue() ) )
		return false;

	bool bDone = false;
	do
	{
		CFSSteamSetupInfo steamInfo;
		steamInfo.m_pDirectoryName = g_pHammer->GetDefaultModFullPath();
		steamInfo.m_bOnlyUseDirectoryName = true;
		steamInfo.m_bToolsMode = true;
		steamInfo.m_bSetSteamDLLPath = true;
		steamInfo.m_bSteam = g_pFileSystem->IsSteam();
		if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		{
			MessageBox( NULL, "Failed to setup steam environment.", "Error", MB_OK );
			return false;
		}

		CFSMountContentInfo fsInfo;
		fsInfo.m_pFileSystem = g_pFileSystem;
		fsInfo.m_bToolsMode = true;
		fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;
		if ( !fsInfo.m_pDirectoryName )
		{
			Error( "FileSystem_LoadFileSystemModule: no -defaultgamedir or -game specified." );
		}

		if ( FileSystem_MountContent( fsInfo ) == FS_OK )
		{
			bDone = true;
		}
		else
		{
			char str[512];
			Q_snprintf( str, sizeof( str ), "%s", FileSystem_GetLastErrorString() );
			MessageBox( NULL, str, "Warning", MB_OK );

			if ( g_pHammer->RequestNewConfig() == REQUEST_QUIT )
				return false;
		}

		FileSystem_AddSearchPath_Platform( fsInfo.m_pFileSystem, steamInfo.m_GameInfoPath );

	} while (!bDone);

	// Required to run through the editor
	g_pMaterialSystem->EnableEditorMaterials();

	// needed for VGUI model rendering
	g_pMaterialSystem->SetAdapter( 0, MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE );

	return true; 
}

void CHammerApp::PostShutdown()
{
}


//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CHammerApp::Main( )
{
	return g_pHammer->MainLoop();
}






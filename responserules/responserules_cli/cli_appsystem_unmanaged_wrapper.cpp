/// The unmanaged side of wrapping the app system for CLR

#include "stdafx.h"
#include "cli_appsystem_unmanaged_wrapper.h"
#include "cli_appsystem_adapter.h"


// temporarily make unicode go away as we deal with Valve types
#ifdef _UNICODE
#define PUT_UNICODE_BACK 
#undef _UNICODE
#endif

#include "filesystem_helpers.h"
#include "utils\common\filesystem_tools.h"


#ifdef PUT_UNICODE_BACK
#define _UNICODE 
#undef PUT_UNICODE_BACK
#endif

inline void CCLIAppSystemAdapter::Wipe( bool bPerformDelete )
{	
	if ( bPerformDelete ) 
	{
		delete m_pLocalRandomStream;
	}
	m_pFilesystem	= NULL;
	m_pLocalRandomStream	= NULL;
	// m_pCommandline	= NULL;
}

CCLIAppSystemAdapter::CCLIAppSystemAdapter() 
{
	Wipe( false );
}


CCLIAppSystemAdapter::~CCLIAppSystemAdapter()
{
	Wipe( true );
	g_pFullFileSystem = NULL;
}

bool CCLIAppSystemAdapter::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "filesystem_stdio.dll",	FILESYSTEM_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};
	return AddSystems( appSystems );
}

bool CCLIAppSystemAdapter::PreInit( )
{
	CreateInterfaceFn factory = GetFactory();
#if TIER2_USE_INIT_DEFAULT_FILESYSTEM
#else
	m_pFilesystem = (IFileSystem*)factory(FILESYSTEM_INTERFACE_VERSION,NULL );
#endif

	// m_pCommandline = CommandLine();
	m_pLocalRandomStream = new CUniformRandomStream();
	// GetCommandLine()->AppendParm("rreditor.exe","");
	// GetCommandLine()->AppendParm("-noasync","1");

#if TIER2_USE_INIT_DEFAULT_FILESYSTEM
	return true;
#else
	return ( m_pFilesystem != NULL );
#endif
		
}


void CCLIAppSystemAdapter::SetupFileSystem( ) 
{
	g_pFullFileSystem = m_pFilesystem;

	g_pFullFileSystem->RemoveAllSearchPaths();
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD );
	g_pFullFileSystem->AddSearchPath( "", "DEFAULT_WRITE_PATH", PATH_ADD_TO_HEAD );

#if TIER2_USE_INIT_DEFAULT_FILESYSTEM
	InitDefaultFileSystem();
#endif
	FileSystem_Init( "./", 0, FS_INIT_COMPATIBILITY_MODE, true );
	// m_pFilesystem->AddSearchPath( "./", "GAME", PATH_ADD_TO_HEAD );
}

void CCLIAppSystemAdapter::AddFileSystemRoot( const char *pPath ) 
{
	g_pFullFileSystem->AddSearchPath( pPath, "LOCAL", PATH_ADD_TO_HEAD );
	g_pFullFileSystem->AddSearchPath( pPath, "GAME", PATH_ADD_TO_HEAD );
}


IFileSystem * CCLIAppSystemAdapter::GetFilesytem() 
{ 
#if TIER2_USE_INIT_DEFAULT_FILESYSTEM
	return g_pFullFileSystem;
#else
	return  m_pFilesystem;
#endif
}

// -----------------------------------------------------------
// | unmanaged-code implementations for the AppSystemWrapper |
// -----------------------------------------------------------

CCLIAppSystemAdapter * AppSystemWrapper_Unmanaged::sm_pAppSystemSingleton = NULL;
int AppSystemWrapper_Unmanaged::sm_nSingletonReferences = 0;


AppSystemWrapper_Unmanaged::AppSystemWrapper_Unmanaged( const char *pCommandLine )
{
	if ( sm_pAppSystemSingleton != NULL )
	{
		Assert( sm_nSingletonReferences > 0 );
		sm_nSingletonReferences++;
	}
	else
	{
		Assert( sm_nSingletonReferences == 0 );
		sm_pAppSystemSingleton = new CCLIAppSystemAdapter();
		sm_nSingletonReferences = 1;
		InitializeAppSystem( sm_pAppSystemSingleton, pCommandLine );
	}
}

AppSystemWrapper_Unmanaged::~AppSystemWrapper_Unmanaged()
{
	if ( sm_nSingletonReferences > 1 )
	{
		sm_nSingletonReferences--;
	}
	else if ( sm_nSingletonReferences == 1 )
	{
		TerminateAppSystem( sm_pAppSystemSingleton );
		delete sm_pAppSystemSingleton;
		sm_pAppSystemSingleton = NULL;
		sm_nSingletonReferences = 0;
	}
	else
	{
		Assert( sm_pAppSystemSingleton == NULL && sm_nSingletonReferences == 0 ) ;
	}
}


void AppSystemWrapper_Unmanaged::InitializeAppSystem( CCLIAppSystemAdapter * pAppSys, const char *pCommandLine ) 
{
	pAppSys->GetCommandLine()->CreateCmdLine( pCommandLine );
	pAppSys->Startup();
	pAppSys->SetupFileSystem();
}

void AppSystemWrapper_Unmanaged::TerminateAppSystem( CCLIAppSystemAdapter * pAppSys )
{
	pAppSys->Shutdown();
}


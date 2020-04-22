//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "tier0/platform.h"

#include "appframework/ilaunchermgr.h"
#if defined( PLATFORM_PS3)
#include "ps3/ps3_helpers.h"
#endif

#include "tier0/platwindow.h"
#include "appframework/IAppSystemGroup.h"
#include "appframework/iappsystem.h"
#include "interface.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include <algorithm>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
extern ILoggingListener *g_pDefaultLoggingListener;


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CAppSystemGroup::CAppSystemGroup( CAppSystemGroup *pAppSystemParent ) : m_SystemDict(false, 0, 16)
{
	m_pParentAppSystem = pAppSystemParent;
}


//-----------------------------------------------------------------------------
// Actually loads a DLL
//-----------------------------------------------------------------------------
CSysModule *CAppSystemGroup::LoadModuleDLL( const char *pDLLName )
{
	return Sys_LoadModule( pDLLName );
}


//-----------------------------------------------------------------------------
// Methods to load + unload DLLs
//-----------------------------------------------------------------------------
AppModule_t CAppSystemGroup::LoadModule( const char *pDLLName )
{
	// Remove the extension when creating the name.
	int nLen = Q_strlen( pDLLName ) + 1;
	char *pModuleName = (char*)stackalloc( nLen );
	Q_StripExtension( pDLLName, pModuleName, nLen );

	// See if we already loaded it...
	for ( int i = m_Modules.Count(); --i >= 0; ) 
	{
		if ( m_Modules[i].m_pModuleName )
		{
			if ( !Q_stricmp( pModuleName, m_Modules[i].m_pModuleName ) )
				return i;
		}
	}

	CSysModule *pSysModule = LoadModuleDLL( pDLLName );
	if (!pSysModule)
	{
#ifdef _X360
		Warning("AppFramework : Unable to load module %s! (err #%d)\n", pDLLName, GetLastError() );
#else
		Warning("AppFramework : Unable to load module %s!\n", pDLLName );
#endif
		return APP_MODULE_INVALID;
	}

	int nIndex = m_Modules.AddToTail();
	m_Modules[nIndex].m_pModule = pSysModule;
	m_Modules[nIndex].m_Factory = 0;
	m_Modules[nIndex].m_pModuleName = (char*)malloc( nLen );
	Q_strncpy( m_Modules[nIndex].m_pModuleName, pModuleName, nLen );

	return nIndex;
}


int CAppSystemGroup::ReloadModule( const char * pDLLName )
{
	// Remove the extension when creating the name.
	int nLen = Q_strlen( pDLLName ) + 1;
	char *pModuleName = (char*)stackalloc( nLen );
	Q_StripExtension( pDLLName, pModuleName, nLen );

	// See if we already loaded it...
	for ( int i = m_Modules.Count(); --i >= 0; ) 
	{
		Module_t &module = m_Modules[i];
		if ( module.m_pModuleName && !Q_stricmp( pModuleName, module.m_pModuleName ) )
		{
			// found the module, reload
			Msg("Unloading module %s, dll %s\n", pModuleName, pDLLName );
			Sys_UnloadModule( m_Modules[i].m_pModule );
			Msg("Module %s unloaded, reloading\n", pModuleName );
			CSysModule *pSysModule = NULL;
			CreateInterfaceFn fnFactory = NULL;
			while( !pSysModule )	   
			{
				pSysModule = LoadModuleDLL( pDLLName );
				if( !pSysModule )
				{
					Warning("Cannot load, retrying in 5 seconds..\n");
					ThreadSleep( 5000 );
				}
				fnFactory = Sys_GetFactory( pSysModule ) ;
				if( !fnFactory )
				{
					Error( "Could not get factory from %s\n", pModuleName );
				}
				( *fnFactory )( "Reload Interface", NULL ); // let the CreateInterface function work and do after-reload stuff
			}
			
			Msg( "Reload complete, module %p->%p, factory %llx->%llx\n", module.m_pModule, pSysModule, (uint64)(uintp)module.m_Factory, (uint64)(uintp)fnFactory );
			module.m_pModule = pSysModule;
			if( module.m_Factory )
			{ // don't reload factory pointer unless it was initialized to non-NULL
				module.m_Factory = fnFactory;
			}
			
			return 0; // no error
		}
	}
	
	Warning( "No such module: '%s' in appsystem @%p. Dumping available modules:\n", pModuleName, this );
	for ( int i = 0; i < m_Modules.Count(); ++i ) 
	{
		Module_t &module = m_Modules[i];
		#ifdef _PS3
		Msg( "%25s %llx %p %6d %6d bytes\n", module.m_pModuleName, (uint64)module.m_Factory, module.m_pModule, ( ( PS3_PrxLoadParametersBase_t *)module.m_pModule )->sysPrxId, ( ( PS3_PrxLoadParametersBase_t *)module.m_pModule )->cbSize );
		#else
		Msg("%25s %p %p\n", module.m_pModuleName, (void*)module.m_Factory, module.m_pModule );
		#endif
	}
	
	return m_pParentAppSystem ? m_pParentAppSystem->ReloadModule( pDLLName ) : -1;
}


AppModule_t CAppSystemGroup::LoadModule( CreateInterfaceFn factory )
{
	if (!factory)
	{
		Warning("AppFramework : Unable to load module %p!\n", factory );
		return APP_MODULE_INVALID;
	}

	// See if we already loaded it...
	for ( int i = m_Modules.Count(); --i >= 0; ) 
	{
		if ( m_Modules[i].m_Factory )
		{
			if ( m_Modules[i].m_Factory == factory )
				return i;
		}
	}

	int nIndex = m_Modules.AddToTail();
	m_Modules[nIndex].m_pModule = NULL;
	m_Modules[nIndex].m_Factory = factory;
	m_Modules[nIndex].m_pModuleName = NULL; 
	return nIndex;
}

void CAppSystemGroup::UnloadAllModules()
{
	// NOTE: Iterate in reverse order so they are unloaded in opposite order
	// from loading
	for (int i = m_Modules.Count(); --i >= 0; )
	{
		if ( m_Modules[i].m_pModule )
		{
			Sys_UnloadModule( m_Modules[i].m_pModule );
		}
		if ( m_Modules[i].m_pModuleName )
		{
			free( m_Modules[i].m_pModuleName );
		}
	}
	m_Modules.RemoveAll();
}


//-----------------------------------------------------------------------------
// Methods to add/remove various global singleton systems 
//-----------------------------------------------------------------------------
IAppSystem *CAppSystemGroup::AddSystem( AppModule_t module, const char *pInterfaceName )
{
	if (module == APP_MODULE_INVALID)
		return NULL;

	int nFoundIndex = m_SystemDict.Find( pInterfaceName );
	if ( nFoundIndex != m_SystemDict.InvalidIndex() )
	{
		Warning("AppFramework : Attempted to add two systems with the same interface name %s!\n", pInterfaceName );
		return m_Systems[ m_SystemDict[nFoundIndex] ];
	}

	Assert( (module >= 0) && (module < m_Modules.Count()) );
	CreateInterfaceFn pFactory = m_Modules[module].m_pModule ? Sys_GetFactory( m_Modules[module].m_pModule ) : m_Modules[module].m_Factory;

	int retval;
	void *pSystem = pFactory( pInterfaceName, &retval );
	if ((retval != IFACE_OK) || (!pSystem))
	{
		Warning("AppFramework : Unable to create system %s!\n", pInterfaceName );
		return NULL;
	}

	IAppSystem *pAppSystem = static_cast<IAppSystem*>(pSystem);
	
	int sysIndex = m_Systems.AddToTail( pAppSystem );

	// Inserting into the dict will help us do named lookup later
	MEM_ALLOC_CREDIT();
	m_SystemDict.Insert( pInterfaceName, sysIndex );
	return pAppSystem;
}

static const char *g_StageLookup[] = 
{
	"CREATION",
	"LOADING DEPENDENCIES",
	"CONNECTION",
	"PREINITIALIZATION",
	"INITIALIZATION",
	"POSTINITIALIZATION",
	"RUNNING",
	"PRESHUTDOWN",
	"SHUTDOWN",
	"POSTSHUTDOWN",
	"DISCONNECTION",
	"DESTRUCTION",
};

void CAppSystemGroup::ReportStartupFailure( int nErrorStage, int nSysIndex )
{
	COMPILE_TIME_ASSERT( APPSYSTEM_GROUP_STAGE_COUNT == ARRAYSIZE( g_StageLookup ) );

	const char *pszStageDesc = "Unknown";
	if ( nErrorStage >= 0 && nErrorStage < ( int )ARRAYSIZE( g_StageLookup ) )
	{
		pszStageDesc = g_StageLookup[ nErrorStage ];
	}

	const char *pszSystemName = "(Unknown)";
	for ( int i = m_SystemDict.First(); i != m_SystemDict.InvalidIndex(); i = m_SystemDict.Next( i ) )
	{
		if ( m_SystemDict[ i ] != nSysIndex )
			continue;

		pszSystemName = m_SystemDict.GetElementName( i );
		break;
	}
		 
	// Walk the dictionary
	Warning( "System (%s) failed during stage %s\n", pszSystemName, pszStageDesc );
}

void CAppSystemGroup::AddSystem( IAppSystem *pAppSystem, const char *pInterfaceName )
{
	if ( !pAppSystem )
		return;

	int sysIndex = m_Systems.AddToTail( pAppSystem );

	// Inserting into the dict will help us do named lookup later
	MEM_ALLOC_CREDIT();
	m_SystemDict.Insert( pInterfaceName, sysIndex );
}

void CAppSystemGroup::RemoveAllSystems()
{
	// NOTE: There's no deallcation here since we don't really know
	// how the allocation has happened. We could add a deallocation method
	// to the code in interface.h; although when the modules are unloaded
	// the deallocation will happen anyways
	m_Systems.RemoveAll();
	m_SystemDict.RemoveAll();
}


//-----------------------------------------------------------------------------
// Simpler method of doing the LoadModule/AddSystem thing.
//-----------------------------------------------------------------------------
bool CAppSystemGroup::AddSystems( AppSystemInfo_t *pSystemList )
{
	while ( pSystemList->m_pModuleName[0] )
	{
		AppModule_t module = LoadModule( pSystemList->m_pModuleName );
		IAppSystem *pSystem = AddSystem( module, pSystemList->m_pInterfaceName );
		if ( !pSystem )
		{
			Warning( "Unable to load interface %s from %s, requested from EXE.\n", pSystemList->m_pInterfaceName, pSystemList->m_pModuleName );
			return false;
		}
		++pSystemList;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Methods to find various global singleton systems 
//-----------------------------------------------------------------------------
void *CAppSystemGroup::FindSystem( const char *pSystemName )
{
	unsigned short i = m_SystemDict.Find( pSystemName );
	if (i != m_SystemDict.InvalidIndex())
		return m_Systems[m_SystemDict[i]];

	// If it's not an interface we know about, it could be an older
	// version of an interface, or maybe something implemented by
	// one of the instantiated interfaces...

	// QUESTION: What order should we iterate this in?
	// It controls who wins if multiple ones implement the same interface
 	for ( i = 0; i < m_Systems.Count(); ++i )
	{
		void *pInterface = m_Systems[i]->QueryInterface( pSystemName );
		if (pInterface)
			return pInterface;
	}

	int nExternalCount = m_NonAppSystemFactories.Count();
	for ( i = 0; i < nExternalCount; ++i )
	{
		void *pInterface = m_NonAppSystemFactories[i]( pSystemName, NULL );
		if (pInterface)
			return pInterface;
	}

	if ( m_pParentAppSystem )
	{
		void* pInterface = m_pParentAppSystem->FindSystem( pSystemName );
		if ( pInterface )
			return pInterface;
	}

	// No dice..
	return NULL;
}


//-----------------------------------------------------------------------------
// Adds a factory to the system so other stuff can query it. Triggers a connect systems
//-----------------------------------------------------------------------------
void CAppSystemGroup::AddNonAppSystemFactory( CreateInterfaceFn fn )
{
	m_NonAppSystemFactories.AddToTail( fn );
}


//-----------------------------------------------------------------------------
// Removes a factory, triggers a disconnect call if it succeeds
//-----------------------------------------------------------------------------
void CAppSystemGroup::RemoveNonAppSystemFactory( CreateInterfaceFn fn )
{
	m_NonAppSystemFactories.FindAndRemove( fn );
}


//-----------------------------------------------------------------------------
// Causes the systems to reconnect to an interface
//-----------------------------------------------------------------------------
void CAppSystemGroup::ReconnectSystems( const char *pInterfaceName )
{
	// Let the libraries regrab the specified interface
	for (int i = 0; i < m_Systems.Count(); ++i )
	{
		IAppSystem *pSystem = m_Systems[i];
		pSystem->Reconnect( GetFactory(), pInterfaceName );
	}
}



//-----------------------------------------------------------------------------
// Gets at the parent appsystem group
//-----------------------------------------------------------------------------
CAppSystemGroup *CAppSystemGroup::GetParent()
{
	return m_pParentAppSystem;
}

	
//-----------------------------------------------------------------------------
// Deals with sorting dependencies and finding circular dependencies
//-----------------------------------------------------------------------------
void CAppSystemGroup::ComputeDependencies( LibraryDependencies_t &depend )
{
	bool bDone = false;
	while ( !bDone )
	{
		bDone = true;

		// If i depends on j, then i depends on what j depends on
		// Add secondary dependencies to i. We stop when no dependencies are added
		int nCount = depend.GetNumStrings();
		for ( int i = 0; i < nCount; ++i )
		{
			int nDependentCount = depend[i].GetNumStrings();
			for ( int j = 0; j < nDependentCount; ++j )
			{
				int nIndex = depend.Find( depend[i].String( j ) );
				if ( nIndex == UTL_INVAL_SYMBOL )
					continue;

				int nSecondaryDepCount = depend[nIndex].GetNumStrings();
				for ( int k = 0; k < nSecondaryDepCount; ++k )
				{
					// Don't bother if we already contain the secondary dependency
					const char *pSecondaryDependency = depend[nIndex].String( k );
					if ( depend[i].Find( pSecondaryDependency ) != UTL_INVAL_SYMBOL )
						continue;

					// Check for circular dependency
					if ( !Q_stricmp( pSecondaryDependency, depend.String( i ) ) )
					{
						Warning( "Encountered a circular dependency with library %s!\n", pSecondaryDependency );
						continue;
					}

					bDone = false;
					depend[i].AddString( pSecondaryDependency );
					nDependentCount = depend[i].GetNumStrings();
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Sorts dependencies
//-----------------------------------------------------------------------------
CAppSystemGroup::LibraryDependencies_t *CAppSystemGroup::sm_pSortDependencies;
bool CAppSystemGroup::SortLessFunc( const int &left, const int &right )
{
	const char *pLeftInterface = sm_pSortDependencies->String( left );
	const char *pRightInterface = sm_pSortDependencies->String( right );
	bool bRightDependsOnLeft = ( (*sm_pSortDependencies)[pRightInterface].Find( pLeftInterface ) != UTL_INVAL_SYMBOL );
	return ( bRightDependsOnLeft );
}

void CAppSystemGroup::SortDependentLibraries( LibraryDependencies_t &depend )
{
	int nCount = depend.GetNumStrings();

	int *pIndices = (int*)stackalloc( depend.GetNumStrings() * sizeof(int) );
	for ( int i = 0; i < nCount; ++i )
	{
		pIndices[i] = i;
	}

	// Sort by dependency. Can't use fancy stl algorithms here because the sort func isn't strongly transitive.
	// Using lame bubble sort instead. We could speed this up using a proper depth-first graph walk, but it's not worth the effort.
	sm_pSortDependencies = &depend;
	bool bChanged = true;
	while ( bChanged )
	{
		bChanged = false;
		for ( int i = 1; i < nCount; i++ )
		{
			for ( int j = 0; j < i; j++ )
			{
				if ( SortLessFunc( pIndices[i], pIndices[j] ) )
				{
					int nTmp = pIndices[i];
					pIndices[i] = pIndices[j];
					pIndices[j] = nTmp;
					bChanged = true;
				}
			}
		}
	}
	sm_pSortDependencies = NULL;


	// This logic will make it so it respects the specified initialization order
	// in the face of no dependencies telling the system otherwise. 
	// Doing this just for safety to reduce the amount of changed code
	bool bDone = false;
	while ( !bDone )
	{
		bDone = true;
		for ( int i = 1; i < nCount; ++i )
		{
			int nLeft = pIndices[i-1];
			int nRight = pIndices[i];
			if ( nRight > nLeft )
				continue;

			const char *pLeftInterface = depend.String( nLeft );
			const char *pRightInterface = depend.String( nRight );
			bool bRightDependsOnLeft = ( depend[pRightInterface].Find( pLeftInterface ) != UTL_INVAL_SYMBOL );
			if ( bRightDependsOnLeft )
				continue;
			Assert ( UTL_INVAL_SYMBOL == depend[pRightInterface].Find( pLeftInterface ) );
			V_swap( pIndices[i], pIndices[i-1] );
			bDone = false;
		}
	}

	// Reorder appsystem list + dictionary indexing
	Assert( m_Systems.Count() == nCount );
	int nTempSize = nCount * sizeof(IAppSystem*);
	IAppSystem **pTemp = (IAppSystem**)stackalloc( nTempSize );
	memcpy( pTemp, m_Systems.Base(), nTempSize );
	for ( int i = 0; i < nCount; ++i )
	{
		m_Systems[i] = pTemp[ pIndices[i] ];
	}
								    
	// Remap system indices
	for ( uint16 i = m_SystemDict.First(); i != m_SystemDict.InvalidIndex(); i = m_SystemDict.Next( i ) )
	{
		int j = 0;
		for ( ; j < nCount; ++j )
		{
			if ( pIndices[j] == m_SystemDict[i] )
			{
				m_SystemDict[i] = j;
				break;
			}
		}
		Assert( j != nCount );
	}

	( void )stackfree( pTemp );
	( void )stackfree( pIndices );
}


//-----------------------------------------------------------------------------
// Finds appsystem names
//-----------------------------------------------------------------------------
const char *CAppSystemGroup::FindSystemName( int nIndex )
{
	for ( uint16 i = m_SystemDict.First(); i != m_SystemDict.InvalidIndex(); i = m_SystemDict.Next( i ) )
	{
		if ( m_SystemDict[i] == nIndex )
			return m_SystemDict.GetElementName( i );
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Method to load all dependent systems
//-----------------------------------------------------------------------------
bool CAppSystemGroup::LoadDependentSystems()
{
	LibraryDependencies_t dependencies;

	// First, load dependencies.
	for ( int i = 0; i < m_Systems.Count(); ++i )
	{
		IAppSystem *pSystem = m_Systems[i];
		const char *pInterfaceName = FindSystemName( i );
		dependencies.AddString( pInterfaceName );

		const AppSystemInfo_t *pDependencies = pSystem->GetDependencies();
		if ( !pDependencies )
			continue;

		for ( ; pDependencies->m_pInterfaceName && pDependencies->m_pInterfaceName[0]; ++pDependencies )
		{
			dependencies[ pInterfaceName ].AddString( pDependencies->m_pInterfaceName );

			CreateInterfaceFn factory = GetFactory();
			if ( factory( pDependencies->m_pInterfaceName, NULL ) ) 
				continue;

			AppModule_t module = LoadModule( pDependencies->m_pModuleName );
			IAppSystem *pSystem = AddSystem( module, pDependencies->m_pInterfaceName );
			if ( !pSystem )
			{
				Warning( "Unable to load interface %s from %s (Dependency of %s)\n", pDependencies->m_pInterfaceName, pDependencies->m_pModuleName, pInterfaceName );
				return false;
			}
		}
	}

	ComputeDependencies( dependencies );
	SortDependentLibraries( dependencies );
	return true;
}


//-----------------------------------------------------------------------------
// Method to connect/disconnect all systems
//-----------------------------------------------------------------------------
bool CAppSystemGroup::ConnectSystems()
{
	// Let the libraries grab any other interfaces they may need
	for (int i = 0; i < m_Systems.Count(); ++i )
	{
		IAppSystem *pSystem = m_Systems[i];
		if ( !pSystem->Connect( GetFactory() ) )
		{
			ReportStartupFailure( CONNECTION, i );
			return false;
		}
	}

	return true;
}

void CAppSystemGroup::DisconnectSystems()
{
	// Disconnect in reverse order of connection
	for (int i = m_Systems.Count(); --i >= 0; )
	{
		m_Systems[i]->Disconnect();
	}
}


//-----------------------------------------------------------------------------
// Method to initialize/shutdown all systems
//-----------------------------------------------------------------------------
InitReturnVal_t CAppSystemGroup::InitSystems()
{
	for (int nSystemsInitialized = 0; nSystemsInitialized < m_Systems.Count(); ++nSystemsInitialized )
	{
		InitReturnVal_t nRetVal = m_Systems[nSystemsInitialized]->Init();
		if ( nRetVal != INIT_OK )
		{
			for( int nSystemsRewind = nSystemsInitialized; nSystemsRewind-->0; )
			{
				m_Systems[nSystemsRewind]->Shutdown();
			}
		
			ReportStartupFailure( INITIALIZATION, nSystemsInitialized );
			return nRetVal;
		}
	}
	return INIT_OK;
}

void CAppSystemGroup::ShutdownSystems()
{
	// Shutdown in reverse order of initialization
	for (int i = m_Systems.Count(); --i >= 0; )
	{
		m_Systems[i]->Shutdown();
	}
}


//-----------------------------------------------------------------------------
// Window management
//-----------------------------------------------------------------------------
void* CAppSystemGroup::CreateAppWindow( void *hInstance, const char *pTitle, bool bWindowed, int w, int h, bool bResizing )
{
#if defined( PLATFORM_WINDOWS ) || defined( PLATFORM_OSX )
	int nFlags = 0;
	if ( !bWindowed )
	{
		nFlags |= WINDOW_CREATE_FULLSCREEN;
	}
	if ( bResizing )
	{
		nFlags |= WINDOW_CREATE_RESIZING;
	}

	PlatWindow_t hWnd = Plat_CreateWindow( hInstance, pTitle, w, h, nFlags );
	if ( hWnd == PLAT_WINDOW_INVALID )
		return NULL;

	int CenterX, CenterY;
	Plat_GetDesktopResolution( &CenterX, &CenterY );
	CenterX = ( CenterX - w ) / 2;
	CenterY = ( CenterY - h ) / 2;
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;

	// In VCR modes, keep it in the upper left so mouse coordinates are always relative to the window.
	Plat_SetWindowPos( hWnd, CenterX, CenterY );

	return hWnd;
#elif defined( PLATFORM_OSX )
	extern ICocoaMgr *g_pCocoaMgr;
	g_pCocoaMgr->CreateGameWindow( pTitle, bWindowed, w, h );
	return (void*)Sys_GetFactoryThis();	// Other stuff will query for ICocoaBridge out of this.
#elif defined( PLATFORM_LINUX )
#ifndef DEDICATED

// PBTODO
// 	extern IGLXMgr *g_pGLXMgr;
// 	g_pGLXMgr->CreateWindow( pTitle, bWindowed, w, h );
	return (void*)Sys_GetFactoryThis();	// Other stuff will query for ICocoaBridge out of this.
#endif
#endif
	return NULL;
}

void CAppSystemGroup::SetAppWindowTitle( void* hWnd, const char *pTitle )
{
	Plat_SetWindowTitle( (PlatWindow_t)hWnd, pTitle );
}


//-----------------------------------------------------------------------------
// Returns the stage at which the app system group ran into an error
//-----------------------------------------------------------------------------
CAppSystemGroup::AppSystemGroupStage_t CAppSystemGroup::GetCurrentStage() const
{
	return m_nCurrentStage;
}


//-----------------------------------------------------------------------------
// Gets at a factory that works just like FindSystem
//-----------------------------------------------------------------------------
// This function is used to make this system appear to the outside world to
// function exactly like the currently existing factory system
CAppSystemGroup *s_pCurrentAppSystem;
void *AppSystemCreateInterfaceFn(const char *pName, int *pReturnCode)
{
	void *pInterface = s_pCurrentAppSystem->FindSystem( pName );
	if ( pReturnCode )
	{
		*pReturnCode = pInterface ? IFACE_OK : IFACE_FAILED;
	}
	return pInterface;
}


//-----------------------------------------------------------------------------
// Gets at a class factory for the topmost appsystem group in an appsystem stack
//-----------------------------------------------------------------------------
CreateInterfaceFn CAppSystemGroup::GetFactory()
{
	return AppSystemCreateInterfaceFn;
}

	
//-----------------------------------------------------------------------------
// Main application loop
//-----------------------------------------------------------------------------
int CAppSystemGroup::Run()
{	
	// The factory now uses this app system group
	s_pCurrentAppSystem	= this;
	
	// Load, connect, init
	int nRetVal = OnStartup();

	// NOTE: In case of OnStartup Failure
	// On PS/3, not unloading the PRXes in order will cause crashes on quit, which is a TRC failure
	// We probably should, but don't have to do this on all platforms, since it's not required to clean-up crash-free.

 	if ( m_nCurrentStage == RUNNING )
	{
		// Main loop implemented by the application
		// FIXME: HACK workaround to avoid vgui porting
		nRetVal = Main();
	}

	// Shutdown, disconnect, unload
	OnShutdown();

	// The factory now uses the parent's app system group
	s_pCurrentAppSystem	= GetParent();

	return nRetVal;
}


//-----------------------------------------------------------------------------
// Virtual methods for override
//-----------------------------------------------------------------------------
int CAppSystemGroup::Startup()
{
	return OnStartup();
}

void CAppSystemGroup::Shutdown()
{
	return OnShutdown();
}


//-----------------------------------------------------------------------------
// Use this version in cases where you can't control the main loop and
// expect to be ticked
//-----------------------------------------------------------------------------
int CAppSystemGroup::OnStartup()
{
	// The factory now uses this app system group
	s_pCurrentAppSystem	= this;

	// Call an installed application creation function
	m_nCurrentStage = CREATION;
	if ( !Create() )
		return -1;

	// Load dependent libraries
	m_nCurrentStage = DEPENDENCIES;
	if ( !LoadDependentSystems() )
		return -1;

	// Let all systems know about each other
	m_nCurrentStage = CONNECTION;
	if ( !ConnectSystems() )
		return -1;

	// Allow the application to do some work before init
	m_nCurrentStage = PREINITIALIZATION;
	if ( !PreInit() )
		return -1;

	// Call Init on all App Systems
	m_nCurrentStage = INITIALIZATION;
	int nRetVal = InitSystems();
	if ( nRetVal != INIT_OK )
		return -1;

	m_nCurrentStage = POSTINITIALIZATION;
	if ( !PostInit() )
		return -1;

	m_nCurrentStage = RUNNING;
	return nRetVal;
}

void CAppSystemGroup::OnShutdown()
{
	// The factory now uses this app system group
	s_pCurrentAppSystem	= this;

	switch( m_nCurrentStage )
	{
	case RUNNING:
	case POSTINITIALIZATION:
		break;

	case PREINITIALIZATION:
	case INITIALIZATION:
		goto disconnect;
	
	case CREATION:
	case DEPENDENCIES:
	case CONNECTION:
		goto destroy;

	default:
		break;
	}

	// Allow the application to do some work before shutdown
	m_nCurrentStage = PRESHUTDOWN;
	PreShutdown();

	// Cal Shutdown on all App Systems
	m_nCurrentStage = SHUTDOWN;
	ShutdownSystems();

	// Allow the application to do some work after shutdown
	m_nCurrentStage = POSTSHUTDOWN;
	PostShutdown();

disconnect:
	// Systems should disconnect from each other
	m_nCurrentStage = DISCONNECTION;
	DisconnectSystems();

destroy:
	// Unload all DLLs loaded in the AppCreate block
	m_nCurrentStage = DESTRUCTION;
	RemoveAllSystems();

	// Have to do this because the logging listeners & response policies may live in modules which are being unloaded
	// @TODO: this seems like a bad legacy practice... app systems should unload their spew handlers gracefully.
	LoggingSystem_ResetCurrentLoggingState();
	Assert( g_pDefaultLoggingListener != NULL );
	LoggingSystem_RegisterLoggingListener( g_pDefaultLoggingListener );

	UnloadAllModules();

	// Call an installed application destroy function
	Destroy();
}


	
//-----------------------------------------------------------------------------
//
// This class represents a group of app systems that are loaded through steam
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSteamAppSystemGroup::CSteamAppSystemGroup( IFileSystem *pFileSystem, CAppSystemGroup *pAppSystemParent )
{
	m_pFileSystem = pFileSystem;
	m_pGameInfoPath[0] = 0;
}


//-----------------------------------------------------------------------------
// Used by CSteamApplication to set up necessary pointers if we can't do it in the constructor
//-----------------------------------------------------------------------------
void CSteamAppSystemGroup::Setup( IFileSystem *pFileSystem, CAppSystemGroup *pParentAppSystem )
{
	m_pFileSystem = pFileSystem;
	m_pParentAppSystem = pParentAppSystem;
}


//-----------------------------------------------------------------------------
// Loads the module from Steam
//-----------------------------------------------------------------------------
CSysModule *CSteamAppSystemGroup::LoadModuleDLL( const char *pDLLName )
{
	return m_pFileSystem->LoadModule( pDLLName );
}


//-----------------------------------------------------------------------------
// Returns the game info path
//-----------------------------------------------------------------------------
const char *CSteamAppSystemGroup::GetGameInfoPath()	const
{
	return m_pGameInfoPath;
}


//-----------------------------------------------------------------------------
// Sets up the search paths
//-----------------------------------------------------------------------------
bool CSteamAppSystemGroup::SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool )
{
	CFSSteamSetupInfo steamInfo;
	steamInfo.m_pDirectoryName = pStartingDir;
	steamInfo.m_bOnlyUseDirectoryName = bOnlyUseStartingDir;
	steamInfo.m_bToolsMode = bIsTool;
	steamInfo.m_bSetSteamDLLPath = true;
	steamInfo.m_bSteam = m_pFileSystem->IsSteam();
	if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		return false;

	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = m_pFileSystem;
	fsInfo.m_bToolsMode = bIsTool;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

	if ( FileSystem_MountContent( fsInfo ) != FS_OK )
		return false;

	// Finally, load the search paths for the "GAME" path.
	CFSSearchPathsInit searchPathsInit;
	searchPathsInit.m_pDirectoryName = steamInfo.m_GameInfoPath;
	searchPathsInit.m_pFileSystem = fsInfo.m_pFileSystem;
	if ( FileSystem_LoadSearchPaths( searchPathsInit ) != FS_OK )
		return false;

	FileSystem_AddSearchPath_Platform( fsInfo.m_pFileSystem, steamInfo.m_GameInfoPath );
	Q_strncpy( m_pGameInfoPath, steamInfo.m_GameInfoPath, sizeof(m_pGameInfoPath) );
	return true;
}

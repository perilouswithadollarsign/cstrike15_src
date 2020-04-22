//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Engine system for loading and managing tools
//
//=============================================================================

#include "quakedef.h"
#include "tier0/icommandline.h"
#include "toolframework/itooldictionary.h"
#include "toolframework/itoolsystem.h"
#include "toolframework/itoolframework.h"
#include "toolframework/iclientenginetools.h"
#include "toolframework/iserverenginetools.h"
#include "tier1/keyvalues.h"
#include "tier1/utlvector.h"
#include "tier1/tier1.h"
#include "filesystem_engine.h"
#include "toolframework/itoolframework.h"
#include "IHammer.h"
#include "baseclientstate.h"
#include "sys.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class IToolSystem;
extern CreateInterfaceFn g_AppSystemFactory;
extern IHammer *g_pHammer;
typedef bool (*FnQuitHandler)( void *pvUserData );
void EngineTool_InstallQuitHandler( void *pvUserData, FnQuitHandler func );

struct ToolModule_t
{
	ToolModule_t() : m_pModule( NULL ), m_pDictionary( NULL ) {}

	ToolModule_t &operator =( const ToolModule_t &other )
	{
		if ( this == &other )
			return *this;

		m_sDllName = other.m_sDllName;
		m_sFirstTool = other.m_sFirstTool;
		m_pModule = other.m_pModule;
		m_pDictionary = other.m_pDictionary;
		m_Systems.CopyArray( other.m_Systems.Base(), other.m_Systems.Count() );
		return *this;
	}

	CUtlString			m_sDllName;
	CUtlString			m_sFirstTool;
	CSysModule			*m_pModule;
	IToolDictionary		*m_pDictionary;

	CUtlVector< IToolSystem * > m_Systems;
};

//-----------------------------------------------------------------------------
// Purpose: -tools loads framework
//-----------------------------------------------------------------------------
class CToolFrameworkInternal : public CBaseAppSystem< IToolFrameworkInternal >
{
public:
	// Here's where the app systems get to learn about each other 
	virtual bool	Connect( CreateInterfaceFn factory );
	virtual void	Disconnect();

	// Here's where systems can access other interfaces implemented by this object
	// Returns NULL if it doesn't implement the requested interface
	virtual void	*QueryInterface( const char *pInterfaceName );

	// Init, shutdown
	virtual InitReturnVal_t Init();
	virtual void	Shutdown();

	virtual bool	CanQuit();

public:
	// Level init, shutdown
	virtual void	ClientLevelInitPreEntityAllTools();
	// entities are created / spawned / precached here
	virtual void	ClientLevelInitPostEntityAllTools();

	virtual void	ClientLevelShutdownPreEntityAllTools();
	// Entities are deleted / released here...
	virtual void	ClientLevelShutdownPostEntityAllTools();

	virtual void	ClientPreRenderAllTools();
	virtual void	ClientPostRenderAllTools();

	virtual bool	IsThirdPersonCamera();

	virtual bool	IsToolRecording();

	// Level init, shutdown
	virtual void	ServerLevelInitPreEntityAllTools();
	// entities are created / spawned / precached here
	virtual void	ServerLevelInitPostEntityAllTools();

	virtual void	ServerLevelShutdownPreEntityAllTools();
	// Entities are deleted / released here...
	virtual void	ServerLevelShutdownPostEntityAllTools();
	// end of level shutdown

	// Called each frame before entities think
	virtual void	ServerFrameUpdatePreEntityThinkAllTools();
	// called after entities think
	virtual void	ServerFrameUpdatePostEntityThinkAllTools();
	virtual void	ServerPreClientUpdateAllTools();
	const char*		GetEntityData( const char *pActualEntityData );
	virtual void	ServerPreSetupVisibilityAllTools();

	virtual bool	PostInit();

	virtual bool	ServerInit( CreateInterfaceFn serverFactory ); 
	virtual bool	ClientInit( CreateInterfaceFn clientFactory ); 

	virtual void	ServerShutdown();
	virtual void	ClientShutdown();

	virtual void	Think( bool finalTick );

	virtual void	PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg );

	virtual void	AdjustEngineViewport( int& x, int& y, int& width, int& height );
	virtual bool	SetupEngineView( Vector &origin, QAngle &angles, float &fov );
	virtual bool	SetupAudioState( AudioState_t &audioState );

	virtual int		GetToolCount();
	virtual const char* GetToolName( int index );
	virtual void	SwitchToTool( int index );
	virtual IToolSystem* SwitchToTool( const char* pToolName );

	virtual bool	IsTopmostTool( const IToolSystem *sys );
	virtual const IToolSystem *GetToolSystem( int index ) const;
	virtual IToolSystem *GetTopmostTool();

	virtual bool	LoadToolModule( char const *pToolModule, bool bSwitchToFirst );

	virtual void	PostMessage( KeyValues *msg );

	virtual bool	GetSoundSpatialization( int iUserData, int guid, SpatializationInfo_t& info );

	virtual void	HostRunFrameBegin();
	virtual void	HostRunFrameEnd();

	virtual void	RenderFrameBegin();
	virtual void	RenderFrameEnd();

	virtual void	VGui_PreRenderAllTools( int paintMode );
	virtual void	VGui_PostRenderAllTools( int paintMode );

	virtual void	VGui_PreSimulateAllTools();
	virtual void	VGui_PostSimulateAllTools();

	// Are we using tools?
	virtual bool	InToolMode();

	// Should the game be allowed to render the world?
	virtual bool	ShouldGameRenderView();

	// Should sounds from the game be played
	virtual bool	ShouldGamePlaySounds();

	virtual IMaterialProxy *LookupProxy( const char *proxyName );

	virtual bool	LoadFilmmaker();
	virtual void	UnloadFilmmaker();

	ToolModule_t	*Find( char const *pModuleName );
								 
	void			UnloadTools( char const *pModule, bool bCheckCanQuit );
	void			GetModules( CUtlVector< CUtlString > &list );

private:
	void			LoadToolsFromEngineToolsManifest();
	void			LoadToolsFromCommandLine( CUtlVector< CUtlString > &list );

	ToolModule_t	*LoadToolsFromLibrary( const char *dllname );

	void			InvokeMethod( ToolSystemFunc_t f );
	void			InvokeMethodInt( ToolSystemFunc_Int_t f, int arg );

	void			ShutdownTools();

	// Purpose: Shuts down all modules
	void			ShutdownModules();

	// Purpose: Shuts down all tool dictionaries
	void			ShutdownToolDictionaries();

	CUtlVector< IToolSystem * >		m_ToolSystems;
	CUtlVector< ToolModule_t > m_Modules;

	int m_nActiveToolIndex;
	bool m_bInToolMode;

	CreateInterfaceFn		m_ClientFactory;
	CreateInterfaceFn		m_ServerFactory;
};

static CToolFrameworkInternal g_ToolFrameworkInternal;
IToolFrameworkInternal *toolframework = &g_ToolFrameworkInternal;


//-----------------------------------------------------------------------------
// Purpose: Used to invoke a method of all added Game systems in order
// Input  : f - function to execute
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::InvokeMethod( ToolSystemFunc_t f )
{
	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *sys = m_ToolSystems[i];
		(sys->*f)();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used to invoke a method of all added Game systems in order
// Input  : f - function to execute
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::InvokeMethodInt( ToolSystemFunc_Int_t f, int arg )
{
	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *sys = m_ToolSystems[i];
		(sys->*f)( arg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Here's where the app systems get to learn about each other 
// Input  : factory - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::Connect( CreateInterfaceFn factory )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::Disconnect()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pvUserData - 
// Output : static bool
//-----------------------------------------------------------------------------
static bool CToolFrameworkInternal_QuitHandler( void *pvUserData )
{
	CToolFrameworkInternal *tfm = reinterpret_cast< CToolFrameworkInternal * >( pvUserData );
	if ( tfm )
	{
		return tfm->CanQuit();
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Init, shutdown
// Input  :  - 
// Output : InitReturnVal_t
//-----------------------------------------------------------------------------
InitReturnVal_t CToolFrameworkInternal::Init()
{
	m_bInToolMode = false;
	m_nActiveToolIndex = -1;
	m_ClientFactory = m_ServerFactory = NULL;

// Disabled in REL for now
#if 1
#ifndef DEDICATED
	EngineTool_InstallQuitHandler( this, CToolFrameworkInternal_QuitHandler );

	// FIXME: Eventually this should be -edit
	if ( CommandLine()->FindParm( "-tools" ) )
	{
		CUtlVector< CUtlString > vecToolList;
		int toolParamIndex = CommandLine()->FindParm( "-tools" );
		if ( toolParamIndex != 0 )
		{
			// See if additional tools were specified
			for ( int i = toolParamIndex + 1; i < CommandLine()->ParmCount(); ++i )
			{
				char const *pToolName = CommandLine()->GetParm( i );
				if ( !pToolName || !*pToolName || *pToolName == '-' || *pToolName == '+' )
					break;

				vecToolList.AddToTail( CUtlString( pToolName ) );
			}
		}

		if ( vecToolList.Count() > 0 )
		{
			LoadToolsFromCommandLine( vecToolList );
		}
		else
		{
		 	LoadToolsFromEngineToolsManifest();
		}
	}
#endif
#endif
	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Called at end of Host_Init
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::PostInit()
{
	bool bRetVal = true;

	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *system = m_ToolSystems[ i ];

		// FIXME: Should this really get access to a list if factories
		bool success = system->Init( );
		if ( !success )
		{
			bRetVal = false;
		}
	}

	// Activate first tool if we didn't encounter an error
	if ( bRetVal )
	{
		SwitchToTool( 0 );
	}

	return bRetVal;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::Shutdown()
{
	// Shut down all tools
	ShutdownTools();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : finalTick - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::Think( bool finalTick )
{
	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		system->Think( finalTick );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : serverFactory - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::ServerInit( CreateInterfaceFn serverFactory )
{
	m_ServerFactory = serverFactory;

	bool retval = true;

	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		// FIXME: Should this really get access to a list if factories
		bool success = system->ServerInit( serverFactory );
		if ( !success )
		{
			retval = false;
		}
	}
	return retval;
}
 
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : clientFactory - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::ClientInit( CreateInterfaceFn clientFactory )
{
	m_ClientFactory = clientFactory;

	bool retval = true;

	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		// FIXME: Should this really get access to a list if factories
		bool success = system->ClientInit( clientFactory );
		if ( !success )
		{
			retval = false;
		}
	}
	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerShutdown()
{
	// Reverse order
	int toolCount = m_ToolSystems.Count();
	for ( int i = toolCount - 1; i >= 0; --i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		system->ServerShutdown();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientShutdown()
{
	// Reverse order
	int toolCount = m_ToolSystems.Count();
	for ( int i = toolCount - 1; i >= 0; --i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		system->ClientShutdown();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::CanQuit()
{
	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		bool canquit = system->CanQuit( "OnQuit" );
		if ( !canquit )
		{
			return false;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Shuts down all modules
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ShutdownModules()
{
	// Shutdown dictionaries
	int i;
	for ( i = m_Modules.Count(); --i >= 0; )
	{
		Assert( !m_Modules[i].m_pDictionary );
		Sys_UnloadModule( m_Modules[i].m_pModule );
		m_Modules[i].m_pModule = NULL;
	}

	m_Modules.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Shuts down all tool dictionaries
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ShutdownToolDictionaries()
{
	// Shutdown dictionaries
	int i;
	for ( i = m_Modules.Count(); --i >= 0; )
	{
		m_Modules[i].m_pDictionary->Shutdown();
	}

	for ( i = m_Modules.Count(); --i >= 0; )
	{
		m_Modules[i].m_pDictionary->Disconnect();
		m_Modules[i].m_pDictionary = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Shuts down all tools
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ShutdownTools()
{
	// Deactivate tool
	SwitchToTool( -1 );

	// Reverse order
	int i;
	int toolCount = m_ToolSystems.Count();
	for ( i = toolCount - 1; i >= 0; --i )
	{
		IToolSystem *system = m_ToolSystems[ i ];
		system->Shutdown();
	}

	m_ToolSystems.RemoveAll();

	ShutdownToolDictionaries();
	ShutdownModules();
}

//-----------------------------------------------------------------------------
// Purpose: Adds tool from specified library
// Input  : *dllname - 
//-----------------------------------------------------------------------------
ToolModule_t *CToolFrameworkInternal::LoadToolsFromLibrary( const char *dllname )
{
	CSysModule *module = Sys_LoadModule( dllname );
	if ( !module )
	{
		Warning( "CToolFrameworkInternal::LoadToolsFromLibrary:  Unable to load '%s'\n", dllname );
		return NULL;
	}

	CreateInterfaceFn factory = Sys_GetFactory( module );
	if ( !factory )
	{
		Sys_UnloadModule( module );
		Warning( "CToolFrameworkInternal::LoadToolsFromLibrary:  Dll '%s' has no factory\n", dllname );
		return NULL;
	}

	IToolDictionary *dictionary = ( IToolDictionary * )factory( VTOOLDICTIONARY_INTERFACE_VERSION, NULL );
	if ( !dictionary )
	{
		Sys_UnloadModule( module );
		Warning( "CToolFrameworkInternal::LoadToolsFromLibrary:  Dll '%s' doesn't support '%s'\n", dllname, VTOOLDICTIONARY_INTERFACE_VERSION );
		return NULL;
	}

	if ( !dictionary->Connect( g_AppSystemFactory ) )
	{
		Sys_UnloadModule( module );
		Warning( "CToolFrameworkInternal::LoadToolsFromLibrary:  Dll '%s' connection phase failed.\n", dllname );
		return NULL;
	}

	if ( dictionary->Init( ) != INIT_OK )
	{
		Sys_UnloadModule( module );
		Warning( "CToolFrameworkInternal::LoadToolsFromLibrary:  Dll '%s' initialization phase failed.\n", dllname );
		return NULL;
	}

	dictionary->CreateTools();

	int idx = m_Modules.AddToTail();

	ToolModule_t *tm = &m_Modules[ idx ];
	tm->m_pDictionary = dictionary;
	tm->m_pModule = module;
	tm->m_sDllName = dllname;

	bool first = true;
	int toolCount = dictionary->GetToolCount();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *tool = dictionary->GetTool( i );
		if ( tool )
		{
			Msg( "Loaded tool '%s'\n", tool->GetToolName() );
			if ( first )
			{
				first = false;
				tm->m_sFirstTool = tool->GetToolName();
			}
			// Add to global dictionary
			m_ToolSystems.AddToTail( tool );
			// Add to local dicitionary
			tm->m_Systems.AddToTail( tool );
		}
	}

	// If this is Hammer, get a pointer to the Hammer interface.
	g_pHammer = (IHammer*)factory( INTERFACEVERSION_HAMMER, NULL );

	return tm;
}


//-----------------------------------------------------------------------------
// Load filmmaker (used by Replay system)
//-----------------------------------------------------------------------------
bool g_bReplayLoadedTools = false;		// This used where significant CommandLine()->CheckParm("-tools") logic is used

bool CToolFrameworkInternal::LoadFilmmaker()
{
	if ( V_stricmp( COM_GetModDirectory(), "tf" ) )
		return false;

	extern bool g_bReplayLoadedTools;
#ifndef DEDICATED
	extern CreateInterfaceFn g_ClientFactory;
#endif
	extern CreateInterfaceFn g_ServerFactory;

	LoadToolsFromLibrary( "tools/ifm.dll" );
#ifndef DEDICATED
	ClientInit( g_ClientFactory );
#endif
	ServerInit( g_ServerFactory );
	PostInit();

	g_bReplayLoadedTools = true;

	return true;
}

//-----------------------------------------------------------------------------
// Unload filmmaker (used by Replay system)
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::UnloadFilmmaker()
{
	if ( V_stricmp( COM_GetModDirectory(), "tf" ) )
		return;

	ServerShutdown();
	ClientShutdown();
	ShutdownTools();

	g_bReplayLoadedTools = false;
}

//-----------------------------------------------------------------------------
// Are we using tools?
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::InToolMode()
{
	extern bool g_bReplayLoadedTools;
	return m_bInToolMode || g_bReplayLoadedTools;
}


//-----------------------------------------------------------------------------
// Should the game be allowed to render the world?
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::ShouldGameRenderView()
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->ShouldGameRenderView( );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Should sounds from the game be played
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::ShouldGamePlaySounds()
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->ShouldGamePlaySounds();
	}
	return true;
}


IMaterialProxy *CToolFrameworkInternal::LookupProxy( const char *proxyName )
{
	int toolCount = GetToolCount();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *tool = m_ToolSystems[ i ];
		Assert( tool );

		IMaterialProxy *matProxy = tool->LookupProxy( proxyName );
		if ( matProxy )
		{
			return matProxy;
		}
	}
	return NULL;
}


void CToolFrameworkInternal::LoadToolsFromCommandLine( CUtlVector< CUtlString > &list )
{
	m_bInToolMode = true;

	// CHECK both bin/tools and gamedir/bin/tools
	for ( int i = 0; i < list.Count(); ++i )
	{
		LoadToolsFromLibrary( CFmtStr( "tools/%s.dll", list[ i ].String() ) );
	}
}
	
//-----------------------------------------------------------------------------
// Purpose: FIXME:  Should scan a KeyValues file
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::LoadToolsFromEngineToolsManifest()
{
	m_bInToolMode = true;

	// Load rootdir/bin/enginetools.txt
	KeyValues *kv = new KeyValues( "enginetools" );
	Assert( kv );
	// Load enginetools.txt if it available and fallback to sdkenginetools.txt 
	char szToolConfigFile[25];

	if ( g_pFileSystem->FileExists( "enginetools.txt", "EXECUTABLE_PATH" ) )
	{
		V_strncpy( szToolConfigFile, "enginetools.txt", sizeof( szToolConfigFile ) );
	}
	else
	{
		V_strncpy( szToolConfigFile, "sdkenginetools.txt", sizeof( szToolConfigFile ) );
	}

	if ( kv && kv->LoadFromFile( g_pFileSystem, szToolConfigFile, "EXECUTABLE_PATH" ) )
	{
		for ( KeyValues *tool = kv->GetFirstSubKey();
				tool != NULL;
				tool = tool->GetNextKey() )
		{
			if ( !Q_stricmp( tool->GetName(),  "library" ) )
			{
				// CHECK both bin/tools and gamedir/bin/tools
				LoadToolsFromLibrary( tool->GetString() );
			}
		}

		kv->deleteThis();
	}
}

ToolModule_t *CToolFrameworkInternal::Find( char const *pModuleName )
{
	for ( int i = 0; i < m_Modules.Count(); ++i )
	{
		ToolModule_t *tm = &m_Modules[ i ];
		if ( !Q_stricmp( tm->m_sDllName.String(), pModuleName ) )
			return tm;
	}

	return NULL;
}
 
//-----------------------------------------------------------------------------
// Purpose: Simple helper class for doing autocompletion of all files in a specific directory by extension
//-----------------------------------------------------------------------------
class CToolAutoCompleteFileList
{
public:
	CToolAutoCompleteFileList( const char *cmdname, const char *subdir, const char *extension )
	{
		m_pszCommandName	= cmdname;
		m_pszSubDir			= subdir;
		m_pszExtension		= extension;
	}

	int AutoCompletionFunc( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );

private:
	const char	*m_pszCommandName;
	const char	*m_pszSubDir;
	const char	*m_pszExtension;
};

//-----------------------------------------------------------------------------
// Purpose: Fills in a list of commands based on specified subdirectory and extension into the format:
//  commandname subdir/filename.ext
//  commandname subdir/filename2.ext
// Returns number of files in list for autocompletion
//-----------------------------------------------------------------------------
int CToolAutoCompleteFileList::AutoCompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = m_pszCommandName;

	char *substring = (char *)partial;
	if ( Q_strstr( partial, cmdname ) )
	{
		substring = (char *)partial + strlen( cmdname ) + 1;
	}

	// Search the directory structure.
	char searchpath[MAX_QPATH];
	if ( m_pszSubDir && m_pszSubDir[0] && Q_strcasecmp( m_pszSubDir, "NULL" ) )
	{
		Q_snprintf(searchpath,sizeof(searchpath),"%s/*.%s", m_pszSubDir, m_pszExtension );
	}
	else
	{
		Q_snprintf(searchpath,sizeof(searchpath),"*.%s", m_pszExtension );
	}

	CUtlSymbolTable entries( 0, 0, true );
	CUtlVector< CUtlSymbol > symbols;

	char const *findfn = Sys_FindFirstEx( searchpath, "EXECUTABLE_PATH", NULL, 0 );
	while ( findfn )
	{
		char sz[ MAX_QPATH ] = { 0 };
		Q_StripExtension( findfn, sz, sizeof( sz ) );

		bool add = false;
		// Insert into lookup
		if ( substring[0] )
		{
			if ( !Q_strncasecmp( findfn, substring, strlen( substring ) ) )
			{
				add = true;
			}
		}
		else
		{
			add = true;
		}

		if ( add )
		{
			CUtlSymbol sym = entries.AddString( findfn );

			int idx = symbols.Find( sym );
			if ( idx == symbols.InvalidIndex() )
			{
				symbols.AddToTail( sym );
			}
		}

		findfn = Sys_FindNext( NULL, 0 );

		// Too many
		if ( symbols.Count() >= COMMAND_COMPLETION_MAXITEMS )
			break;
	}

	Sys_FindClose();

	for ( int i = 0; i < symbols.Count(); i++ )
	{
		char const *filename = entries.String( symbols[ i ] );

		Q_snprintf( commands[ i ], sizeof( commands[ i ] ), "%s %s", cmdname, filename );
		// Remove .dem
		commands[ i ][ strlen( commands[ i ] ) - 4 ] = 0;
	}

	return symbols.Count();
}

static int g_ToolLoad_CompletionFunc( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	static CToolAutoCompleteFileList ToolLoad_Complete( "toolload", "tools", "dll" );
	return ToolLoad_Complete.AutoCompletionFunc( partial, commands );
}

// This now works in most cases, however it will not work once the sfm has run a python script since when python imports modules, it doesn't properly unload them,
// So they stick around in memory and crash the second time you load the ifm.dll (or maybe on the second unload).  This is a pretty serious flaw in python25.dll
//-----------------------------------------------------------------------------
// Purpose: Simple helper class for doing autocompletion of all files in a specific directory by extension
//-----------------------------------------------------------------------------
class CToolUnloadAutoCompleteFileList
{
public:
	CToolUnloadAutoCompleteFileList( const char *cmdname )
	{
		m_pszCommandName	= cmdname;
	}

	int AutoCompletionFunc( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
	{
		char const *cmdname = m_pszCommandName;

		char *substring = (char *)partial;
		if ( Q_strstr( partial, cmdname ) )
		{
			substring = (char *)partial + strlen( cmdname ) + 1;
		}

		CUtlVector< CUtlString > modules;
		int c = 0;
		g_ToolFrameworkInternal.GetModules( modules );
		for ( int i = 0; i < modules.Count(); i++ )
		{
			char const *pFileName = modules[ i ].String();
			char filename[ MAX_PATH ];
			Q_FileBase( pFileName, filename, sizeof( filename ) );

			bool add = false;
			// Insert into lookup
			if ( substring[0] )
			{
				if ( !Q_strncasecmp( filename, substring, strlen( substring ) ) )
				{
					add = true;
				}
			}
			else
			{
				add = true;
			}

			if ( add )
			{
				Q_snprintf( commands[ c ], sizeof( commands[ c ] ), "%s %s", cmdname, filename );
				++c;
			}
		}

		return c;
	}

private:
	const char	*m_pszCommandName;
};

static int g_ToolUnload_CompletionFunc( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	static CToolUnloadAutoCompleteFileList ToolUnload_Complete( "toolunload" );
	return ToolUnload_Complete.AutoCompletionFunc( partial, commands );
}

void Tool_Unload_f( const CCommand &args )
{
	if ( !toolframework->InToolMode() )
		return;

	if ( args.ArgC() < 2 )
	{
		ConMsg ("toolunload <toolname> [-nosave]: unloads a tool\n");
		return;
	}

	bool bPromptForSave = true;
	if ( args.ArgC() > 2 )
	{
		if ( Q_stricmp( "-nosave", args.Arg( 2 ) ) == 0 )
		{
			bPromptForSave = false;
		}
	}

	char fn[ MAX_PATH ];
	Q_FileBase( args.Arg( 1 ), fn, sizeof( fn ) );

	CFmtStr module( "tools/%s.dll", fn ); 
	g_ToolFrameworkInternal.UnloadTools( module, bPromptForSave );
}

static ConCommand ToolUnload( "toolunload", Tool_Unload_f, "Unload a tool.", 0, g_ToolUnload_CompletionFunc ); 


// If module not already loaded, loads it and optionally switches to first tool in module
bool CToolFrameworkInternal::LoadToolModule( char const *pToolModule, bool bSwitchToFirst )
{
	CFmtStr module( "tools/%s.dll", pToolModule );

	ToolModule_t *tm = Find( module );
	if ( tm )
	{
		// Already loaded
		return true;
	}

	tm = LoadToolsFromLibrary( module );
	if ( !tm )
	{
		ConMsg( "failed to load tools from %s\n", pToolModule );
		return false;
	}

	for ( int i = 0; i < tm->m_Systems.Count(); ++i )
	{
		IToolSystem *sys = tm->m_Systems[ i ];
		if ( sys )
		{
			sys->ClientInit( m_ClientFactory );
			sys->ServerInit( m_ServerFactory ); 

			sys->Init();
		}
	}

	// Since this is a late init, do some more work
	if ( bSwitchToFirst )
	{
		SwitchToTool( tm->m_sFirstTool.String() );
	}

	return true;
}

void CToolFrameworkInternal::GetModules( CUtlVector< CUtlString > &list )
{
	for ( int i = 0; i < m_Modules.Count(); ++i )
	{
		CUtlString str;
		str = m_Modules[ i ].m_sDllName;
		list.AddToTail( str );
	}
}

void CToolFrameworkInternal::UnloadTools( char const *pModule, bool bCheckCanQuit )
{
	ToolModule_t *tm = g_ToolFrameworkInternal.Find( pModule );
	if ( !tm )
	{
		ConMsg( "module %s not loaded\n", pModule );
		return;
	}

	if ( bCheckCanQuit )
	{		
		for ( int i = tm->m_Systems.Count() - 1; i >= 0; --i )
		{
			IToolSystem *sys = tm->m_Systems[ i ];
			if ( !sys->CanQuit( "OnUnload" ) )
			{
				Msg( "Can't unload %s, %s not ready to exit\n",
					pModule, sys->GetToolName() );
				return;
			}
		}
	}

	for ( int i = tm->m_Systems.Count() - 1; i >= 0; --i )
	{
		IToolSystem *sys = tm->m_Systems[ i ];

		int idx =  m_ToolSystems.Find( sys );
		if ( idx == m_nActiveToolIndex )
		{
			sys->OnToolDeactivate();
			m_nActiveToolIndex = -1;
		}

		sys->Shutdown();
		sys->ServerShutdown();
		sys->ClientShutdown();

		m_ToolSystems.Remove( idx );
	}

	tm->m_pDictionary->Shutdown();
	tm->m_pDictionary->Disconnect();

	// This (should) discards and material proxies being held onto by any materials in the tool
	materials->UncacheUnusedMaterials( false );

	Sys_UnloadModule( tm->m_pModule );

	for ( int i = 0; i < m_Modules.Count(); ++i )
	{
		if ( m_Modules[ i ].m_pModule == tm->m_pModule )
		{
			m_Modules.Remove( i );
			break;
		}
	}

	// Switch to another tool, or set the active index to -1 if none left!!!
	SwitchToTool( 0 );
}

void Tool_Load_f( const CCommand &args )
{
	if ( !toolframework->InToolMode() )
		return;

	if ( args.ArgC() != 2 )
	{
		ConMsg ("toolload <toolname>: loads a tool\n");
		return;
	}

	char fn[ MAX_PATH ];
	Q_FileBase( args.Arg( 1 ), fn, sizeof( fn ) );

	CFmtStr module( "tools/%s.dll", fn ); 
	if ( g_ToolFrameworkInternal.Find( module )  )
	{
		ConMsg( "module %s already loaded\n", module.Access() );
		return;
	}

	g_ToolFrameworkInternal.LoadToolModule( fn, true );
}

static ConCommand ToolLoad( "toolload", Tool_Load_f, "Load a tool.", 0, g_ToolLoad_CompletionFunc ); 

//-----------------------------------------------------------------------------
// Purpose: Level init, shutdown
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientLevelInitPreEntityAllTools()
{
	InvokeMethod( &IToolSystem::ClientLevelInitPreEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientLevelInitPostEntityAllTools()
{
	InvokeMethod( &IToolSystem::ClientLevelInitPostEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientLevelShutdownPreEntityAllTools()
{
	InvokeMethod( &IToolSystem::ClientLevelShutdownPreEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Entities are deleted / released here...
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientLevelShutdownPostEntityAllTools()
{
	InvokeMethod( &IToolSystem::ClientLevelShutdownPostEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientPreRenderAllTools()
{
	InvokeMethod( &IToolSystem::ClientPreRender );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::IsThirdPersonCamera()
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->IsThirdPersonCamera( );
	}
	return false;
}

// is the current tool recording?
bool CToolFrameworkInternal::IsToolRecording()
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->IsToolRecording( );
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ClientPostRenderAllTools()
{
	InvokeMethod( &IToolSystem::ClientPostRender );
}

//-----------------------------------------------------------------------------
// Purpose: Level init, shutdown
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerLevelInitPreEntityAllTools()
{
	InvokeMethod( &IToolSystem::ServerLevelInitPreEntity );
}

//-----------------------------------------------------------------------------
// Purpose: entities are created / spawned / precached here
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerLevelInitPostEntityAllTools()
{
	InvokeMethod( &IToolSystem::ServerLevelInitPostEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerLevelShutdownPreEntityAllTools()
{
	InvokeMethod( &IToolSystem::ServerLevelShutdownPreEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Entities are deleted / released here...
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerLevelShutdownPostEntityAllTools()
{
	InvokeMethod( &IToolSystem::ServerLevelShutdownPostEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Called each frame before entities think
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerFrameUpdatePreEntityThinkAllTools()
{
	InvokeMethod( &IToolSystem::ServerFrameUpdatePreEntityThink );
}

//-----------------------------------------------------------------------------
// Purpose: Called after entities think
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerFrameUpdatePostEntityThinkAllTools()
{
	InvokeMethod( &IToolSystem::ServerFrameUpdatePostEntityThink );
}

//-----------------------------------------------------------------------------
// Purpose: Called before client networking occurs on the server
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::ServerPreClientUpdateAllTools()
{
	InvokeMethod( &IToolSystem::ServerPreClientUpdate );
}


//-----------------------------------------------------------------------------
// The server uses this to call into the tools to get the actual
// entities to spawn on startup
//-----------------------------------------------------------------------------
const char* CToolFrameworkInternal::GetEntityData( const char *pActualEntityData )
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->GetEntityData( pActualEntityData );
	}
	return pActualEntityData;
}

void* CToolFrameworkInternal::QueryInterface( const char *pInterfaceName )
{
	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *tool = m_ToolSystems[ i ];
		Assert( tool );
		void *pRet = tool->QueryInterface( pInterfaceName );
		if ( pRet )
			return pRet;
	}
	return NULL;
}

void CToolFrameworkInternal::ServerPreSetupVisibilityAllTools()
{
	InvokeMethod( &IToolSystem::ServerPreSetupVisibility );
}


//-----------------------------------------------------------------------------
// Purpose: Post a message to tools
// Input  : hEntity - 
//			*msg - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg )
{
	// FIXME: Only message topmost tool?

	int toolCount = m_ToolSystems.Count();
	for ( int i = 0; i < toolCount; ++i )
	{
		IToolSystem *tool = m_ToolSystems[ i ];
		Assert( tool );
		tool->PostToolMessage( hEntity, msg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Only active tool gets to adjust viewport
// Input  : x - 
//			y - 
//			width - 
//			height - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::AdjustEngineViewport( int& x, int& y, int& width, int& height )
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		tool->AdjustEngineViewport( x, y, width, height );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Only active tool gets to set the camera/view
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::SetupEngineView( Vector &origin, QAngle &angles, float &fov )
{
	if ( m_nActiveToolIndex < 0 )
		return false;

	IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
	Assert( tool );
	return tool->SetupEngineView( origin, angles, fov );
}

//-----------------------------------------------------------------------------
// Purpose: Only active tool gets to set the microphone
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::SetupAudioState( AudioState_t &audioState )
{
	if ( m_nActiveToolIndex < 0 )
		return false;

	IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
	Assert( tool );
	return tool->SetupAudioState( audioState );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::VGui_PreRenderAllTools( int paintMode )
{
	InvokeMethodInt( &IToolSystem::VGui_PreRender, paintMode );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::VGui_PostRenderAllTools( int paintMode )
{
	InvokeMethodInt( &IToolSystem::VGui_PostRender, paintMode );
}

void CToolFrameworkInternal::VGui_PreSimulateAllTools()
{
	InvokeMethod( &IToolSystem::VGui_PreSimulate );
}

void CToolFrameworkInternal::VGui_PostSimulateAllTools()
{
	InvokeMethod( &IToolSystem::VGui_PostSimulate );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : int
//-----------------------------------------------------------------------------
int CToolFrameworkInternal::GetToolCount()
{
	return m_ToolSystems.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CToolFrameworkInternal::GetToolName( int index )
{
	if ( index < 0 || index >= m_ToolSystems.Count() )
	{
		return "";
	}
	IToolSystem *sys = m_ToolSystems[ index ];
	if ( sys )
	{
		return sys->GetToolName();
	}
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::SwitchToTool( int index )
{
	if ( ( m_ToolSystems.Count() < 1 ) || ( index >= m_ToolSystems.Count() ) )
	{
		m_nActiveToolIndex = -1;
		return;
	}

	if ( index != m_nActiveToolIndex )
	{
		if ( m_nActiveToolIndex >= 0 )
		{
			IToolSystem *pOldTool = m_ToolSystems[ m_nActiveToolIndex ];
			pOldTool->OnToolDeactivate();
		}

		m_nActiveToolIndex = index;
		if ( m_nActiveToolIndex >= 0 )
		{
			IToolSystem *pNewTool = m_ToolSystems[ m_nActiveToolIndex ];
			pNewTool->OnToolActivate();
		}
	}
}


//-----------------------------------------------------------------------------
// Switches to a named tool
//-----------------------------------------------------------------------------
IToolSystem *CToolFrameworkInternal::SwitchToTool( const char* pToolName )
{
	int nCount = GetToolCount();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pToolName, GetToolName(i) ) )
		{
			SwitchToTool( i );
			return m_ToolSystems[i];
		}
	}

	return NULL;
}

	
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *sys - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolFrameworkInternal::IsTopmostTool( const IToolSystem *sys )
{
	if ( m_ToolSystems.Count() <= 0 || ( m_nActiveToolIndex < 0 ) )
		return false;

	return ( m_ToolSystems[ m_nActiveToolIndex ] == sys );
}

IToolSystem *CToolFrameworkInternal::GetTopmostTool()
{
	return m_nActiveToolIndex >= 0 ? m_ToolSystems[ m_nActiveToolIndex ] : NULL;
}

//-----------------------------------------------------------------------------
// returns a tool system by index
//-----------------------------------------------------------------------------
const IToolSystem *CToolFrameworkInternal::GetToolSystem( int index ) const
{
	if ( ( index < 0 ) || ( index >= m_ToolSystems.Count() ) )
		return NULL;

	return m_ToolSystems[index];
}


void CToolFrameworkInternal::PostMessage( KeyValues *msg )
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		tool->PostToolMessage( 0, msg );
	}
}

bool CToolFrameworkInternal::GetSoundSpatialization( int iUserData, int guid, SpatializationInfo_t& info )
{
	if ( m_nActiveToolIndex >= 0 )
	{
		IToolSystem *tool = m_ToolSystems[ m_nActiveToolIndex ];
		Assert( tool );
		return tool->GetSoundSpatialization( iUserData, guid, info );
	}
	return true;
}

void CToolFrameworkInternal::HostRunFrameBegin()
{
	InvokeMethod( &IToolSystem::HostRunFrameBegin );
}

void CToolFrameworkInternal::HostRunFrameEnd()
{
	InvokeMethod( &IToolSystem::HostRunFrameEnd );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::RenderFrameBegin()
{
#ifndef DEDICATED
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
#endif
	InvokeMethod( &IToolSystem::RenderFrameBegin );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CToolFrameworkInternal::RenderFrameEnd()
{
#ifndef DEDICATED
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
#endif
	InvokeMethod( &IToolSystem::RenderFrameEnd );
}

// Exposed because it's an IAppSystem
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CToolFrameworkInternal, IToolFrameworkInternal, VTOOLFRAMEWORK_INTERFACE_VERSION, g_ToolFrameworkInternal );

//-----------------------------------------------------------------------------
// Purpose: exposed from engine to client .dll
//-----------------------------------------------------------------------------
class CClientEngineTools : public IClientEngineTools
{
public:
	virtual void LevelInitPreEntityAllTools();
	virtual void LevelInitPostEntityAllTools();
	virtual void LevelShutdownPreEntityAllTools();
	virtual void LevelShutdownPostEntityAllTools();
	virtual void PreRenderAllTools();
	virtual void PostRenderAllTools();
	virtual void PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg );
	virtual void AdjustEngineViewport( int& x, int& y, int& width, int& height );
	virtual bool SetupEngineView( Vector &origin, QAngle &angles, float &fov );
	virtual bool SetupAudioState( AudioState_t &audioState );
	virtual void VGui_PreRenderAllTools( int paintMode );
	virtual void VGui_PostRenderAllTools( int paintMode );
	virtual bool IsThirdPersonCamera( );
	virtual bool InToolMode();
};

EXPOSE_SINGLE_INTERFACE( CClientEngineTools, IClientEngineTools, VCLIENTENGINETOOLS_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::LevelInitPreEntityAllTools()
{
	g_ToolFrameworkInternal.ClientLevelInitPreEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::LevelInitPostEntityAllTools()
{
	g_ToolFrameworkInternal.ClientLevelInitPostEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::LevelShutdownPreEntityAllTools()
{
	g_ToolFrameworkInternal.ClientLevelShutdownPreEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::LevelShutdownPostEntityAllTools()
{
	g_ToolFrameworkInternal.ClientLevelShutdownPostEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::PreRenderAllTools()
{
	g_ToolFrameworkInternal.ClientPreRenderAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CClientEngineTools::PostRenderAllTools()
{
	g_ToolFrameworkInternal.ClientPostRenderAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hEntity - 
//			*msg - 
//-----------------------------------------------------------------------------
void CClientEngineTools::PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg )
{
	g_ToolFrameworkInternal.PostToolMessage( hEntity, msg );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
//			width - 
//			height - 
//-----------------------------------------------------------------------------
void CClientEngineTools::AdjustEngineViewport( int& x, int& y, int& width, int& height )
{
	g_ToolFrameworkInternal.AdjustEngineViewport( x, y, width, height );
}

bool CClientEngineTools::SetupEngineView( Vector &origin, QAngle &angles, float &fov )
{
	return g_ToolFrameworkInternal.SetupEngineView( origin, angles, fov );
}

bool CClientEngineTools::SetupAudioState( AudioState_t &audioState )
{
	return g_ToolFrameworkInternal.SetupAudioState( audioState );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientEngineTools::VGui_PreRenderAllTools( int paintMode )
{
	g_ToolFrameworkInternal.VGui_PreRenderAllTools( paintMode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientEngineTools::VGui_PostRenderAllTools( int paintMode )
{
	g_ToolFrameworkInternal.VGui_PostRenderAllTools( paintMode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CClientEngineTools::IsThirdPersonCamera( )
{
	return g_ToolFrameworkInternal.IsThirdPersonCamera( );
}

bool CClientEngineTools::InToolMode()
{
	return g_ToolFrameworkInternal.InToolMode();
}

//-----------------------------------------------------------------------------
// Purpose: Exposed to server.dll
//-----------------------------------------------------------------------------
class CServerEngineTools : public IServerEngineTools
{
public:
	// Inherited from IServerEngineTools
	virtual void LevelInitPreEntityAllTools();
	virtual void LevelInitPostEntityAllTools();
	virtual void LevelShutdownPreEntityAllTools();
	virtual void LevelShutdownPostEntityAllTools();
	virtual void FrameUpdatePreEntityThinkAllTools();
	virtual void FrameUpdatePostEntityThinkAllTools();
	virtual void PreClientUpdateAllTools();
	virtual void PreSetupVisibilityAllTools();
	virtual const char* GetEntityData( const char *pActualEntityData );
	virtual void* QueryInterface( const char *pInterfaceName );
	virtual bool InToolMode();
};

EXPOSE_SINGLE_INTERFACE( CServerEngineTools, IServerEngineTools, VSERVERENGINETOOLS_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::LevelInitPreEntityAllTools()
{
	g_ToolFrameworkInternal.ServerLevelInitPreEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::LevelInitPostEntityAllTools()
{
	g_ToolFrameworkInternal.ServerLevelInitPostEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::LevelShutdownPreEntityAllTools()
{
	g_ToolFrameworkInternal.ServerLevelShutdownPreEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::LevelShutdownPostEntityAllTools()
{
	g_ToolFrameworkInternal.ServerLevelShutdownPostEntityAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::FrameUpdatePreEntityThinkAllTools()
{
	g_ToolFrameworkInternal.ServerFrameUpdatePreEntityThinkAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::FrameUpdatePostEntityThinkAllTools()
{
	g_ToolFrameworkInternal.ServerFrameUpdatePostEntityThinkAllTools();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CServerEngineTools::PreClientUpdateAllTools()
{
	g_ToolFrameworkInternal.ServerPreClientUpdateAllTools();
}


//-----------------------------------------------------------------------------
// The server uses this to call into the tools to get the actual
// entities to spawn on startup
//-----------------------------------------------------------------------------
const char* CServerEngineTools::GetEntityData( const char *pActualEntityData )
{
	return g_ToolFrameworkInternal.GetEntityData( pActualEntityData );
}

void* CServerEngineTools::QueryInterface( const char *pInterfaceName )
{
	return g_ToolFrameworkInternal.QueryInterface( pInterfaceName );
}

void CServerEngineTools::PreSetupVisibilityAllTools()
{
	return g_ToolFrameworkInternal.ServerPreSetupVisibilityAllTools();
}

bool CServerEngineTools::InToolMode()
{
	return g_ToolFrameworkInternal.InToolMode();
}


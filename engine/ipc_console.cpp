//====== Copyright c 1996-2008, Valve Corporation, All rights reserved. =======//
//
// Purpose: Exposing command console for scripting
//
//

#include <windows.h>
#include <stdio.h>

#include "valve_ipc_win32.h"

#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "mathlib/mathlib.h"
#include "tier1/convar.h"
#include "UtlStringMap.h"

#include "con_nprint.h"
#include "cdll_int.h"
#include "globalvars_base.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern IVEngineClient *engineClient;
extern CGlobalVarsBase *gpGlobals;

//////////////////////////////////////////////////////////////////////////
//
// Definition of CValveIpcServerUtl
//

class CValveIpcServerUtl : public CValveIpcServer
{
public:
	explicit CValveIpcServerUtl( char const *szServerName ) : CValveIpcServer( szServerName ) {}
	virtual BOOL ExecuteCommand( char *bufCommand, DWORD numCommandBytes, char *bufResult, DWORD &numResultBytes )
	{
		CUtlBuffer cmd( bufCommand, numCommandBytes, CUtlBuffer::READ_ONLY );
		CUtlBuffer res( bufResult, VALVE_IPC_CS_BUFFER, int( 0 ) );
		if ( !ExecuteCommand( cmd, res ) )
			return FALSE;
		numResultBytes = res.TellPut();
		return TRUE;
	}
	virtual BOOL ExecuteCommand( CUtlBuffer &cmd, CUtlBuffer &res ) = 0;
};



//////////////////////////////////////////////////////////////////////////
//
// Class encapsulating a single ipc console
//

class CIpcConsoleImpl : public CValveIpcServerUtl
{
public:
	explicit CIpcConsoleImpl( char const *szName );
	virtual BOOL ExecuteCommand( CUtlBuffer &cmd, CUtlBuffer &res );
};

CIpcConsoleImpl::CIpcConsoleImpl( char const *szName ) :
	CValveIpcServerUtl( szName )
{
}

BOOL CIpcConsoleImpl::ExecuteCommand( CUtlBuffer &cmd, CUtlBuffer &res )
{
	extern ConVar sv_cheats;
	if ( !sv_cheats.GetBool() )
	{
		res.PutString( "ERROR: Engine IPC communication requires sv_cheats 1!\n" );
		res.PutInt( 0 );
		return TRUE;
	}

	// NOTE: ExecuteCommand is called on a separate thread!
	engineClient->ClientCmd( ( char const * ) cmd.Base() );
	res.PutInt( 0 );
	return TRUE;
}



//////////////////////////////////////////////////////////////////////////
//
// Class encapsulating management of ipc consoles
//

class CIpcConsoleMgr
{
public:
	bool Enable( char const *szConsoleName );
	bool Disable( char const *szConsoleName );
	void DisableAll();
	void Show( char const *szConsoleName );
	void ShowAll();

protected:
	CIpcConsoleImpl *FindConsole( char const *szConsoleName );
	CIpcConsoleImpl *CreateConsole( char const *szConsoleName );
	bool RemoveConsole( CIpcConsoleImpl *pConsole );

protected:
	CUtlStringMap< CIpcConsoleImpl * > m_mapConsoles;
};

static CIpcConsoleMgr &IpcConsoleGetMgr()
{
	static CIpcConsoleMgr s_mgr;
	return s_mgr;
}

static char const *IpcConsoleGetDefaultName()
{
	return "ENGINE_IPC_SERVER";
}

bool CIpcConsoleMgr::Enable( char const *szConsoleName )
{
	CIpcConsoleImpl *pConsole = FindConsole( szConsoleName );
	if ( !pConsole )
		pConsole = CreateConsole( szConsoleName );

	if ( pConsole->EnsureRegisteredAndRunning() )
	{
		Msg( "IPC: enabled console \"%s\"\n", szConsoleName );
		return true;
	}

	RemoveConsole( pConsole );
	Error( "IPC: Failed to enable console \"%s\"\n", szConsoleName );
	return false;
}

bool CIpcConsoleMgr::Disable( const char *szConsoleName )
{
	CIpcConsoleImpl *pConsole = FindConsole( szConsoleName );
	if ( !pConsole )
	{
		Msg( "IPC: disabling unknown console \"%s\"\n", szConsoleName );
		return false;
	}

	pConsole->EnsureStoppedAndUnregistered();
	RemoveConsole( pConsole );
	Msg( "IPC: disabled console \"%s\"\n", szConsoleName );
	return true;
}

void CIpcConsoleMgr::DisableAll()
{
	int numActiveConsoles = 0;
	for ( int idx = 0; idx < m_mapConsoles.GetNumStrings(); ++ idx )
	{
		char const *szConsoleName = m_mapConsoles.String( idx );
		CIpcConsoleImpl *&pConsole = m_mapConsoles[ szConsoleName ];
		if ( !pConsole )
			continue;

		pConsole->EnsureStoppedAndUnregistered();
		delete pConsole;
		pConsole = NULL;
		
		Msg( "IPC: disabled console \"%s\"\n", szConsoleName );
		++ numActiveConsoles;
	}
	Msg( "IPC: disabled %d console(s).\n", numActiveConsoles );
}

void CIpcConsoleMgr::Show( const char *szConsoleName )
{
	CIpcConsoleImpl *pConsole = FindConsole( szConsoleName );
	if ( pConsole )
	{
		Msg( "IPC: Active console \"%s\"\n", szConsoleName );
	}
	else
	{
		Msg( "IPC: Unknown console \"%s\"\n", szConsoleName );
	}
}

void CIpcConsoleMgr::ShowAll()
{
	int numActiveConsoles = 0;
	for ( int idx = 0; idx < m_mapConsoles.GetNumStrings(); ++ idx )
	{
		char const *szConsoleName = m_mapConsoles.String( idx );
		CIpcConsoleImpl *pConsole = m_mapConsoles[ szConsoleName ];
		if ( !pConsole )
			continue;
		
		Msg( "IPC: Active console \"%s\"\n", szConsoleName );
		++ numActiveConsoles;
	}
	Msg( "IPC: %d active console(s).\n", numActiveConsoles );
}

CIpcConsoleImpl * CIpcConsoleMgr::FindConsole( char const *szConsoleName )
{
	if ( m_mapConsoles.Find( szConsoleName ) == m_mapConsoles.InvalidIndex() )
		return NULL;

	CIpcConsoleImpl *&pConsole = m_mapConsoles[ szConsoleName ];
	return pConsole;
}

CIpcConsoleImpl * CIpcConsoleMgr::CreateConsole( char const *szConsoleName )
{
	CIpcConsoleImpl *&pConsole = m_mapConsoles[ szConsoleName ];
	pConsole = new CIpcConsoleImpl( szConsoleName );
	return pConsole;
}

bool CIpcConsoleMgr::RemoveConsole( CIpcConsoleImpl *pConsoleRemove )
{
	if ( !pConsoleRemove )
		return false;

	for ( int idx = 0; idx < m_mapConsoles.GetNumStrings(); ++ idx )
	{
		char const *szConsoleName = m_mapConsoles.String( idx );
		CIpcConsoleImpl *&pConsole = m_mapConsoles[ szConsoleName ];
		if ( pConsole != pConsoleRemove )
			continue;

		delete pConsole;
		pConsole = NULL;
		return true;
	}

	return false;
}



//////////////////////////////////////////////////////////////////////////
//
//  Console commands to enable, disable and show ipc console names
//

CON_COMMAND_F( ipc_console_enable, "Enable IPC console", FCVAR_CHEAT | FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_DONTRECORD )
{
	char const *szConsoleName = IpcConsoleGetDefaultName();
	if ( args.ArgC() <= 1 )
	{
		IpcConsoleGetMgr().Enable( szConsoleName );
		return;
	}
	else for ( int k = 1; k < args.ArgC(); ++ k )
	{
		szConsoleName = args.Arg( k );
		if ( IpcConsoleGetMgr().Enable( szConsoleName ) )
			return;
	}
}

CON_COMMAND_F( ipc_console_disable, "Disable IPC console(s)", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_DONTRECORD )
{
	char const *szConsoleName = IpcConsoleGetDefaultName();
	if ( args.ArgC() <= 1 )
	{
		IpcConsoleGetMgr().Disable( szConsoleName );
		return;
	}
	else for ( int k = 1; k < args.ArgC(); ++ k )
	{
		szConsoleName = args.Arg( k );
		IpcConsoleGetMgr().Disable( szConsoleName );
	}
}

CON_COMMAND_F( ipc_console_disable_all, "Disable all IPC consoles", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_DONTRECORD )
{
	IpcConsoleGetMgr().DisableAll();
}

CON_COMMAND_F( ipc_console_show, "Show status of IPC consoles", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_DONTRECORD )
{
	if ( args.ArgC() <= 1 )
	{
		IpcConsoleGetMgr().ShowAll();
		return;
	}
	else for ( int k = 1; k < args.ArgC(); ++ k )
	{
		char const *szConsoleName = args.Arg( k );
		IpcConsoleGetMgr().Show( szConsoleName );
	}
}


//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: steam state machine that handles authenticating steam users
//
//=============================================================================//
#ifdef _WIN32
#if !defined( _X360 )
#include "winlite.h"
#include <winsock2.h> // INADDR_ANY defn
#endif
#elif POSIX
#include <netinet/in.h>
#endif

#include "baseclient.h"
#include "utlvector.h"
#include "netadr.h"
#include "cl_steamauth.h"
#include "interface.h"
#include "filesystem_engine.h"
#include "tier0/icommandline.h"
#include "tier0/vprof.h"
#include "host.h"
#include "cmd.h"
#include "common.h"
#include "inputsystem/iinputsystem.h"
#include "materialsystem/imaterialsystem.h"
#ifndef DEDICATED
#include "vgui_baseui_interface.h"
#include "server.h"
#include "matchmaking/imatchframework.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


#pragma warning( disable: 4355 ) // disables ' 'this' : used in base member initializer list'

extern ConVar cl_hideserverip;

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CSteam3Client s_Steam3Client;
CSteam3Client  &Steam3Client()
{
	return s_Steam3Client;
}


static void Callback_SteamAPIWarningMessageHook( int n, const char *sz )
{
	if ( n == 0 )
	{
		Msg( "[STEAM] %s\n", sz );
	}
	else
	{
		Warning( "[STEAM] %s\n", sz );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSteam3Client::CSteam3Client() 
#if !defined(NO_STEAM)
:
			m_CallbackClientGameServerDeny( this, &CSteam3Client::OnClientGameServerDeny ),
			m_CallbackGameServerChangeRequested( this, &CSteam3Client::OnGameServerChangeRequested ),
			m_CallbackGameOverlayActivated( this, &CSteam3Client::OnGameOverlayActivated ),
			m_CallbackPersonaStateChanged( this, &CSteam3Client::OnPersonaUpdated ),
			m_CallbackLowBattery( this, &CSteam3Client::OnLowBattery ),
			m_CallbackSteamSocketStatus( this, &CSteam3Client::OnSteamSocketStatus )
#endif
{
	m_bActive = false;
	m_bGSSecure = false;
	m_bGameOverlayActive = false;
	m_bInitialized = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSteam3Client::~CSteam3Client()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Unload the steam3 engine
//-----------------------------------------------------------------------------
void CSteam3Client::Shutdown()
{	
	if ( !m_bActive )
		return;

	m_bActive = false;	
#if !defined( NO_STEAM )
	if( m_bInitialized )
	{
		SteamAPI_Shutdown();
		m_bInitialized = false;
	}
	Clear(); // Steam API context shutdown
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Initialize the steam3 connection
//-----------------------------------------------------------------------------
void CSteam3Client::Activate()
{
	if ( m_bActive )
		return;

	m_bActive = true;
	m_bGSSecure = false;

#if !defined( NO_STEAM )

	#ifndef _PS3
	SteamAPI_InitSafe(); // ignore failure, that will fall out later when they don't get a valid logon cookie
	#else
	extern SteamPS3Params_t g_EngineSteamPS3Params;
	SteamAPI_Init( &g_EngineSteamPS3Params );
	#endif

	m_bInitialized = Init(); // Steam API context init

	if ( m_bInitialized )
	{
		SteamClient()->SetWarningMessageHook( Callback_SteamAPIWarningMessageHook );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Get the steam3 logon cookie to use
//-----------------------------------------------------------------------------
void CSteam3Client::GetAuthSessionTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket, uint64 unGSSteamID,  bool bSecure )
{
	m_bGSSecure = bSecure;

#if !defined( NO_STEAM )
	if ( !SteamUser() )
		return;

	Assert( m_hAuthTicket == k_HAuthTicketInvalid );

	CSteamID steamID = SteamUser()->GetSteamID();
	// prepend the steamID
	uint64 ulSteamID = steamID.ConvertToUint64();

	// use CUtlBuffer to stick 64 bit steamID into the ticket
	CUtlBuffer buffer( pTicket, cbMaxTicket, 0 );
	buffer.PutInt64( LittleQWord( ulSteamID ) );
	Assert( buffer.TellPut() == sizeof( uint64 ));
	pTicket = (uint8 *)buffer.PeekPut();
	m_hAuthTicket = SteamUser()->GetAuthSessionTicket( pTicket, cbMaxTicket-sizeof(uint64), pcbTicket );
	// include the size of the steamID
	(*pcbTicket) = (*pcbTicket) + sizeof(uint64);

#else
	return;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Tell steam that we are leaving a server
//-----------------------------------------------------------------------------
void CSteam3Client::CancelAuthTicket()
{
	m_bGSSecure = false;
	if ( !SteamUser() )
		return;

#if !defined( NO_STEAM )
	if ( m_hAuthTicket != k_HAuthTicketInvalid )
		SteamUser()->CancelAuthTicket( m_hAuthTicket );
	m_hAuthTicket = k_HAuthTicketInvalid;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Process any callbacks we may have
//-----------------------------------------------------------------------------
void CSteam3Client::RunFrame()
{
#if !defined( NO_STEAM )
	VPROF_BUDGET( "CSteam3Client::RunFrame", VPROF_BUDGETGROUP_STEAM );
	SteamAPI_RunCallbacks();
#endif
}


#if !defined(NO_STEAM)
//-----------------------------------------------------------------------------
// Purpose: Disconnect the user from their current server
//-----------------------------------------------------------------------------
void CSteam3Client::OnClientGameServerDeny( ClientGameServerDeny_t *pClientGameServerDeny )
{
	if ( pClientGameServerDeny->m_uAppID == GetSteamAppID() )
	{
		const char *pszReason = "Unknown";
		switch ( pClientGameServerDeny->m_uReason )
		{
			case ( k_EDenyInvalidVersion ) : pszReason = "Invalid version"; break;
			case ( k_EDenyGeneric ) : pszReason = "Kicked"; break;
			case ( k_EDenyNotLoggedOn ) : pszReason = "Not logged on"; break;
			case ( k_EDenyNoLicense ) : pszReason = "No license"; break;
			case ( k_EDenyCheater ) : pszReason = "VAC banned "; break;
			case ( k_EDenyLoggedInElseWhere ) : pszReason = "Dropped from server"; break;
			case ( k_EDenyUnknownText ) : pszReason = "Unknown"; break;
			case ( k_EDenyIncompatibleAnticheat ) : pszReason = "Incompatible Anti Cheat"; break;
			case ( k_EDenyMemoryCorruption ) : pszReason = "Memory corruption"; break;
			case ( k_EDenyIncompatibleSoftware ) : pszReason = "Incompatible software"; break;
			case ( k_EDenySteamConnectionLost ) : pszReason = "Steam connection lost"; break;
			case ( k_EDenySteamConnectionError ) : pszReason = "Steam connection error"; break;
			case ( k_EDenySteamResponseTimedOut ) : pszReason = "Response timed out"; break;
			
			// k_EDenySteamValidationStalled is just an informative event, we will receive
			// a real validation result later, shouldn't kick client for validation stall!
			case ( k_EDenySteamValidationStalled ) : DevMsg( "Validation stalled\n" ); return;
		}

		Warning( "Disconnect: %s\n", pszReason );

		Host_Disconnect( true );
	}
	
}


extern ConVar	password;
//-----------------------------------------------------------------------------
// Purpose: Disconnect the user from their current server
//-----------------------------------------------------------------------------
void CSteam3Client::OnGameServerChangeRequested( GameServerChangeRequested_t *pGameServerChangeRequested )
{
#ifdef PORTAL2
	if ( GetSteamUniverse() != k_EUniverseBeta && GetSteamUniverse() != k_EUniverseInternal )
	{
		// Portal 2 doesn't support joining pure servers from Steam Overlay
		return;
	}
#endif
#ifndef DEDICATED
	if ( g_pMatchFramework )
	{
		g_pMatchFramework->CloseSession();	// Make sure we disconnect from our old server
	}
#endif
	password.SetValue( pGameServerChangeRequested->m_rgchPassword );
	Msg( "Connecting to %s\n", cl_hideserverip.GetInt()>0 ? "<hidden>" : pGameServerChangeRequested->m_rgchServer );
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), va( "connect %s\n", pGameServerChangeRequested->m_rgchServer ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Client::OnGameOverlayActivated( GameOverlayActivated_t *pGameOverlayActivated )
{
	g_pInputSystem->ResetInputState();
	m_bGameOverlayActive = !!pGameOverlayActivated->m_bActive;
	if ( m_bGameOverlayActive )
	{
#ifndef DEDICATED
		// Don't activate it if it's already active (a sub window may be active)
		// Multiplayer doesn't want the UI to appear, since it can't pause anyway
		if ( !EngineVGui()->IsGameUIVisible() && sv.IsActive() && sv.IsSinglePlayerGame() )
		{
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), "gameui_activate" );
		}
#endif
	}
}

extern void UpdateNameFromSteamID( IConVar *pConVar, CSteamID *pSteamID );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Client::OnPersonaUpdated( PersonaStateChange_t *pPersonaStateChanged )
{
	if ( !SteamUtils() || !SteamFriends() || !SteamUser() || !pPersonaStateChanged )
		return;

	// Check that something changed about local user
	CSteamID steamID = SteamUser()->GetSteamID();
	if ( steamID.ConvertToUint64() == pPersonaStateChanged->m_ulSteamID )
	{
		if ( pPersonaStateChanged->m_nChangeFlags & k_EPersonaChangeName )
		{
			IConVar *pConVar = g_pCVar->FindVar( "name" );
			UpdateNameFromSteamID( pConVar, &steamID );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Client::OnLowBattery( LowBatteryPower_t *pLowBat )
{
	// on the 9min, 5 min and 1 min warnings tell the engine to fire off a save
	switch(  pLowBat->m_nMinutesBatteryLeft )
	{
	case 9: 
	case 5: 
	case 1: 
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "save LowBattery_AutoSave" );
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Client::OnSteamSocketStatus( SocketStatusCallback_t *pSocketStatus )
{
}

#endif

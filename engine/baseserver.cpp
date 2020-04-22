//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// baseserver.cpp: implementation of the CBaseServer class.
//
//////////////////////////////////////////////////////////////////////



#if defined(_WIN32) && !defined(_X360)
#include "winlite.h"		// FILETIME
#elif defined(OSX) || defined(CYGWIN)
#include <time.h>                  
#include <sys/time.h>                  
#include <sys/resource.h>                  
#include <netinet/in.h>
#elif defined(LINUX)
#include <time.h>                  
#include <sys/sysinfo.h>          
#include <asm/param.h> // for HZ
#include <netinet/in.h>
#elif defined(_X360)
#elif defined(_PS3)
#else
#error "Includes for CPU usage calcs here"
#endif

#include "filesystem_engine.h"
#include "baseserver.h"
#include "hltvserver.h"
#include "sysexternal.h"
#include "quakedef.h"
#include "host.h"
#include "netmessages.h"
#include "master.h"
#include "sys.h"
#include "framesnapshot.h"
#include "sv_packedentities.h"
#include "dt_send_eng.h"
#include "dt_recv_eng.h"
#include "networkstringtable.h"
#include "sys_dll.h"
#include "host_cmd.h"
#include "sv_steamauth.h"
#include "SteamUserIDValidation.h"

#include <proto_oob.h>
#include <vstdlib/random.h>
#include <irecipientfilter.h>
#include <keyvalues.h>
#include <tier0/vprof.h>
#include <cdll_int.h>
#include <eiface.h>
#include <client_class.h>
#include "tier0/icommandline.h"
#include "sv_steamauth.h"
#include "sv_ipratelimit.h"
#include "cl_steamauth.h"
#include "fmtstr.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif
#include "mathlib/IceKey.H"
#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"
#include "fmtstr.h"
#include "sv_plugin.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CThreadFastMutex g_svInstanceBaselineMutex;

// BUGBUG: JAY: Leaving this here for some of the matchmaking code.  I don't want to delete the code or enable it
// in other games yet. (came over in the merge and will be rationalized later)
#define IsLeft4Dead() false

// Give new data to Steam's master server updater every N seconds.
// This is NOT how often packets are sent to master servers, only how often the
// game server talks to Steam's master server updater (which is on the game server's
// machine, not the Steam servers).
#define MASTER_SERVER_UPDATE_INTERVAL		2.0

// Steam has a matching one in matchmakingtypes.h
#define MAX_TAG_STRING_LENGTH		128

bool g_bSteamMasterHeartbeatsEnabled = false;

int SortServerTags( char* const *p1, char* const *p2 )
{
	return ( Q_strcmp( *p1, *p2 ) > 0 );
}

struct NoReentry_t
{
	NoReentry_t( int *pn ) : m_pn( pn ) { ++ *m_pn; }
	~NoReentry_t() { -- *m_pn; }
	int *m_pn;
};

static void ServerTagsCleanUp( void )
{
	static int s_nNoReentry = 0;
	if ( s_nNoReentry )
		return;
	NoReentry_t noReentry( &s_nNoReentry );

	CUtlVector<char*> TagList;
	ConVarRef sv_tags( "sv_tags" );
	if ( sv_tags.IsValid() )
	{
		int i;
		char tmptags[MAX_TAG_STRING_LENGTH];
		tmptags[0] = '\0';

		V_SplitString( sv_tags.GetString(), ",", TagList );

		// make a pass on the tags to eliminate preceding whitespace and empty tags
		for ( i = 0; i < TagList.Count(); i++ )
		{
			if ( i > 0 )
			{
				Q_strncat( tmptags, ",", MAX_TAG_STRING_LENGTH );
			}

			char *pChar = TagList[i];
			while ( *pChar && *pChar == ' ' )
			{
				pChar++;
			}

			// make sure we don't have an empty string (all spaces or ,,)
			if ( *pChar )
			{
				Q_strncat( tmptags, pChar, MAX_TAG_STRING_LENGTH );
			}
		}

		// reset our lists and sort the tags
		TagList.PurgeAndDeleteElements();
		V_SplitString( tmptags, ",", TagList );
		TagList.Sort( SortServerTags );
		tmptags[0] = '\0';

		// create our new, sorted list of tags
		for ( i = 0; i < TagList.Count(); i++ )
		{
			if ( i > 0 )
			{
				Q_strncat( tmptags, ",", MAX_TAG_STRING_LENGTH );
			}

			Q_strncat( tmptags, TagList[i], MAX_TAG_STRING_LENGTH );
		}

		// set our convar and purge our list
		if ( Q_strcmp( tmptags, sv_tags.GetString() ) )
		{
			sv_tags.SetValue( tmptags );
		}
		TagList.PurgeAndDeleteElements();
	}
}

static void SvTagsChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	sv.UpdateGameData();
	if ( sv.IsActive() )
	{
		Cbuf_AddText( CBUF_SERVER, "heartbeat\n" );
	}
	ServerTagsCleanUp();
}

static void SvGameDataChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	// TODO: sv.UpdateGameData();
	if ( sv.IsActive() )
	{
		Cbuf_AddText( CBUF_SERVER, "heartbeat\n" );
	}
}

extern ConVar	sv_search_key;
extern ConVar	sv_lan;
extern ConVar	cl_hideserverip;

ConVar			sv_region( "sv_region","-1", FCVAR_NONE | FCVAR_RELEASE, "The region of the world to report this server in." );
static ConVar	sv_instancebaselines( "sv_instancebaselines", "1", FCVAR_DEVELOPMENTONLY, "Enable instanced baselines. Saves network overhead." );
static ConVar	sv_stats( "sv_stats", "1", 0, "Collect CPU usage stats" );
static ConVar	sv_enableoldqueries( "sv_enableoldqueries", "0", 0, "Enable support for old style (HL1) server queries" );

static ConVar	sv_reservation_tickrate_adjustment( "sv_reservation_tickrate_adjustment", "0", FCVAR_RELEASE, "Adjust server tickrate upon reservation" );

static void SvPasswordChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef cvref( pConVar );
	bool bOldPassword = ( pOldValue && pOldValue[0] && Q_stricmp( pOldValue, "none" ) );
	char const *pNewValue = cvref.GetString();
	bool bNewPassword = ( pNewValue && pNewValue[0] && Q_stricmp( pNewValue, "none" ) );

	if ( ( sv.GetNumHumanPlayers() > 0 ) || sv.IsReserved() )
	{
		if ( !bOldPassword && bNewPassword )
		{
			Msg( "Cannot require sv_password when server is already reserved or clients connected!\n" );
			cvref.SetValue( "" );
		}
	}

	sv.OnPasswordChanged();
}

static ConVar	sv_password( "sv_password", "", FCVAR_NOTIFY | FCVAR_PROTECTED | FCVAR_DONTRECORD | FCVAR_RELEASE, "Server password for entry into multiplayer games", SvPasswordChangeCallback );
ConVar			sv_tags( "sv_tags", "", FCVAR_NOTIFY | FCVAR_RELEASE, "Server tags. Used to provide extra information to clients when they're browsing for servers. Separate tags with a comma.", SvTagsChangeCallback );
ConVar			sv_visiblemaxplayers( "sv_visiblemaxplayers", "-1",  FCVAR_RELEASE, "Overrides the max players reported to prospective clients" );
ConVar			sv_alternateticks( "sv_alternateticks", ( IsX360() ) ? "1" : "0", FCVAR_RELEASE, "If set, server only simulates entities on even numbered ticks.\n" );
ConVar			sv_allow_wait_command( "sv_allow_wait_command", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "Allow or disallow the wait command on clients connected to this server." );
#if !defined( CSTRIKE15 )
// We are switching CStrike to always have lobbies associated with servers for community matchmaking
ConVar			sv_allow_lobby_connect_only( "sv_allow_lobby_connect_only", "1",  FCVAR_RELEASE, "If set, players may only join this server from matchmaking lobby, may not connect directly." );
#endif
static ConVar   sv_reservation_timeout( "sv_reservation_timeout", "45", FCVAR_RELEASE, "Time in seconds before lobby reservation expires.", true, 5.0f, true, 180.0f );
static ConVar   sv_reservation_grace( "sv_reservation_grace", "5", 0, "Time in seconds given for a lobby reservation.", true, 3.0f, true, 30.0f );

ConVar			sv_steamgroup( "sv_steamgroup", "", FCVAR_NOTIFY | FCVAR_RELEASE, "The ID of the steam group that this server belongs to. You can find your group's ID on the admin profile page in the steam community.", SvGameDataChangeCallback );
ConVar			sv_steamgroup_exclusive( "sv_steamgroup_exclusive", "0", FCVAR_RELEASE, "If set, only members of Steam group will be able to join the server when it's empty, public people will be able to join the server only if it has players." );

static void SvMmQueueReservationChanged( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	if ( serverGameDLL )
		serverGameDLL->UpdateGCInformation();
}
ConVar			sv_mmqueue_reservation( "sv_mmqueue_reservation", "", FCVAR_DEVELOPMENTONLY | FCVAR_DONTRECORD, "Server queue reservation", SvMmQueueReservationChanged );
ConVar			sv_mmqueue_reservation_timeout( "sv_mmqueue_reservation_timeout", "21", FCVAR_DEVELOPMENTONLY, "Time in seconds before mmqueue reservation expires.", true, 5.0f, true, 180.0f );
ConVar			sv_mmqueue_reservation_extended_timeout( "sv_mmqueue_reservation_extended_timeout", "21", FCVAR_DEVELOPMENTONLY, "Extended time in seconds before mmqueue reservation expires.", true, 5.0f, true, 180.0f );

extern CNetworkStringTableContainer *networkStringTableContainerServer;
extern ConVar sv_stressbots;

int g_CurGameServerID = 1;

static void SetMasterServerKeyValue( ISteamGameServer *pGameServer, IConVar *pConVar )
{
	ConVarRef var( pConVar );

	// For protected cvars, don't send the string
	if ( var.IsFlagSet( FCVAR_PROTECTED ) )
	{
		// If it has a value string and the string is not "none"
		if ( ( strlen( var.GetString() ) > 0 ) &&
				stricmp( var.GetString(), "none" ) )
		{
			pGameServer->SetKeyValue( var.GetName(), "1" );
		}
		else
		{
			pGameServer->SetKeyValue( var.GetName(), "0" );
		}
	}
	else
	{
		pGameServer->SetKeyValue( var.GetName(), var.GetString() );
	}

	if ( Steam3Server().BIsActive() )
	{
		sv.RecalculateTags();
	}
}

static KeyValues *g_pKVrulesConvars = NULL;

static void ServerNotifyVarChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	if ( !pConVar->IsFlagSet( FCVAR_NOTIFY ) )
		return;

	if ( !g_pKVrulesConvars->GetBool( pConVar->GetName() ) )
		return;
	
	ISteamGameServer *pGameServer = Steam3Server().SteamGameServer();
	if ( !pGameServer )
	{
		// This will force it to send all the rules whenever the master server updater is there.
		sv.SetMasterServerRulesDirty();
		return;
	}

	SetMasterServerKeyValue( pGameServer, pConVar );
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBaseServer::CBaseServer() : 
	m_ServerQueryChallenges( 0, 1024 ),  // start with 1K of entries, and alloc in 1K chunks
	m_BaselineHandles( DefLessFunc( int ) ),
	m_flFlagForSteamIDReuseAfterShutdownTime( 0 )
{
	// Just get a unique ID to talk to the steam master server updater.
	m_bRestartOnLevelChange = false;
	
	m_StringTables = NULL;
	m_pInstanceBaselineTable = NULL;
	m_pLightStyleTable = NULL;
	m_pUserInfoTable = NULL;
	m_pServerStartupTable = NULL;
	m_pDownloadableFileTable = NULL;

	m_fLastCPUCheckTime = 0;
	m_fStartTime = 0;
	m_fCPUPercent = 0;
	m_Socket = NS_SERVER;
	m_nTickCount = 0;
	
	m_szMapname[0] = 0;
	m_szBaseMapname[0] = 0;
	m_szMapGroupName[0] = 0;
	m_szSkyname[0] = 0;
	m_Password[0] = 0;
	worldmapCRC = 0;
	clientDllCRC = 0;
	stringTableCRC = 0;

	serverclasses = serverclassbits = 0;
	m_nMaxclients = m_nSpawnCount = 0;
	m_flTickInterval = 0.03;
	m_flTimescale = 1.0f;
	m_nUserid = 0;
	m_bIsDedicated = false;
	m_bIsDedicatedForXbox = false;
	m_bIsDedicatedForPS3 = false;
	m_fCPUPercent = 0;
	m_fLastCPUCheckTime = 0;
	
	m_bMasterServerRulesDirty = true;
	m_flLastMasterServerUpdateTime = 0;

	m_nReservationCookie = 0;
	m_pnReservationCookieSession = NULL;
	m_flReservationExpiryTime = -1.0f;
	m_flTimeLastClientLeft = -1.0f;
	m_numGameSlots = 0;

	m_flTimeReservationGraceStarted = -1.0f;

	m_GameDataVersion = 0;
	m_nMatchId = 0;
}

CBaseServer::~CBaseServer()
{
	ClearBaselineHandles();
}

/*
================
SV_CheckChallenge

Make sure connecting client is not spoofing
================
*/
bool CBaseServer::CheckChallengeNr( const ns_address &adr, int nChallengeValue )
{
	// See if the challenge is valid
	// Don't care if it is a local address.
	if ( adr.IsLoopback() )
		return true;

	// X360TBD: network
	if ( IsX360() || IsDedicatedForXbox() )
		return true;

	for (int i=0 ; i<m_ServerQueryChallenges.Count() ; i++)
	{
		if ( adr.CompareAdr(m_ServerQueryChallenges[i].adr, true) ) // base adr only
		{
			if (nChallengeValue != m_ServerQueryChallenges[i].challenge)
			{
				return false;
			}

			if ( net_time > ( m_ServerQueryChallenges[i].time+ CHALLENGE_LIFETIME) ) // allow challenge values to last for 1 hour
			{
				m_ServerQueryChallenges.FastRemove(i);
				ConMsg( "Old challenge from %s.\n", ns_address_render( adr ).String() );
				return false;
			}

			return true;
		}

		// clean up any old entries
		if ( net_time > ( m_ServerQueryChallenges[i].time+ CHALLENGE_LIFETIME) ) 
		{
			m_ServerQueryChallenges.FastRemove(i);
			i--; // backup one as we just shifted the whole vector back by the deleted element
		}
	}
	
	if ( nChallengeValue != -1 )
	{
		ConDMsg( "No challenge from %s.\n", ns_address_render( adr ).String() ); // this is a common message
	}
	return false;
}


bool CBaseServer::CanAcceptChallengesFrom( const ns_address &adrFrom ) const
{
	// check timeout
	if ( m_flTimeReservationGraceStarted < 0 )
		return true;
	if ( ( net_time - m_flTimeReservationGraceStarted ) > sv_reservation_grace.GetFloat() )
		return true;

	// otherwise can only accept from a single address
	return adrFrom.CompareAdr( m_adrReservationGraceStarted );
}


const char *CBaseServer::GetPassword() const
{
	const char *password = sv_password.GetString();

	// if password is empty or "none", return NULL
	if ( !password[0] || !Q_stricmp(password, "none" ) )
	{
		return NULL;
	}

	return password;
}


void CBaseServer::SetPassword(const char *password)
{
	if ( password != NULL )
	{
		Q_strncpy( m_Password, password, sizeof(m_Password) );
	}
	else
	{
		m_Password[0] = 0; // clear password
	}
}

int CBaseServer::GetNextUserID()
{
	// Note: we'll usually exit on the first pass of this loop..
	for ( int i=0; i < m_Clients.Count()+1; i++ )
	{
		int nTestID = (m_nUserid + i + 1) % SHRT_MAX;

		// Make sure no client has this user ID.		
		int iClient;
		for ( iClient=0; iClient < m_Clients.Count(); iClient++ )
		{
			if ( m_Clients[iClient]->GetUserID() == nTestID )
				break;
		}

		// Ok, no client has this ID, so return it.		
		if ( iClient == m_Clients.Count() )
			return nTestID;
	}
	
	Assert( !"GetNextUserID: can't find a unique ID." );
	return m_nUserid + 1;
}

bool CBaseServer::IsSinglePlayerGame() const
{
#if !defined( DEDICATED )
	if ( sv.IsDedicated() )
	{
		return false;
	}

	if ( m_nMaxclients <= 1 )
		return true;

	#if !defined( PORTAL2 ) && !defined( CSTRIKE15 )
	//
	// Portal 2 offline splitscreen games should NOT be paused
	// transition movies during loading rely on player think functions
	// See bugbait 81253: https://bugbait.valvesoftware.com/show_bug.cgi?id=81253
	//
	if ( IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		if ( KeyValues *pSettings = pIMatchSession->GetSessionSettings() )
		{
			char const *szNetwork = pSettings->GetString( "system/network", "" );
			if ( szNetwork && !Q_stricmp( szNetwork, "offline" ) )
				return true;
		}
	}
	#endif
#endif

	return false;
}

/*
================
SV_ConnectClient

Initializes a CSVClient for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
IClient *CBaseServer::ConnectClient ( const ns_address &adr, int protocol, int challenge, int authProtocol, 
							    const char *name, const char *password, const char *hashedCDkey, int cdKeyLen,
								CUtlVector< CCLCMsg_SplitPlayerConnect_t * > & splitScreenClients, bool isClientLowViolence, CrossPlayPlatform_t clientPlatform,
								const byte *pbEncryptionKey, int nEncryptionKeyIndex )
{

	ns_address_render sAdr( adr );

#ifdef IHV_DEMO
	if ( !adr.IsLoopback() )
	{
		Warning( "This demo version only works as a listen server. Ignoring connection request from %s\n", adr.ToString() );
		return NULL;
	}
	else
	{
		DevMsg( "IHV Demo Version - Connected to loopback client.\n");
	}
#endif

	COM_TimestampedLog( "CBaseServer::ConnectClient" );

	if ( !IsActive() )
	{
		DevMsg( "Server not active, ignoring %s\n", sAdr.String() );
		return NULL;
	}

	if ( !name || !password || !hashedCDkey )
	{
		DevMsg( "Bad auth data from %s\n", sAdr.String() );
		return NULL;
	}

	// Make sure protocols match up
	if ( !CheckProtocol( adr, protocol ) )
	{
		DevMsg( "Protocol error from %s\n", sAdr.String() );
		return NULL;
	}


	if ( !CheckChallengeNr( adr, challenge ) )
	{
		RejectConnection( adr, "Bad challenge.\n");
		return NULL;
	}

	bool bIsLocalConnection = adr.IsLocalhost() || adr.IsLoopback();

#ifndef NO_STEAM
	if ( IsExclusiveToLobbyConnections() && !IsReserved() && !bIsLocalConnection )
	{
		RejectConnection( adr, "Server only accepting connections from game lobby %s %d.\n", sAdr.String(), challenge );
		return NULL;
	}
#endif

	// Listen server level background map is always a single 
	//  player map, don't allow shenanigans or mayhem.
	// Also, if the user started a "single player" campaign, don't let anyone else join
	if ( !IsDedicated() &&
		!bIsLocalConnection )
	{
		if ( sv.IsLevelMainMenuBackground() )
		{
			RejectConnection( adr, "#Valve_Reject_Background_Map" );
			return NULL;
		}
		if ( IsSinglePlayerGame() )
		{
			RejectConnection( adr, "#Valve_Reject_Single_Player" );
			return NULL;
		}
		if ( ShouldHideServer() )
		{
			// Right now, hidden means commentary, "solo" or background map (l4d), the former of which are covered above.
			RejectConnection( adr, "#Valve_Reject_Hidden_Game" );
			return NULL;
		}
	}

	// SourceTV checks password & restrictions later once we know
	// if its a normal spectator client or a relay proxy
	if ( !IsHLTV() && !IsReplay() )
	{
#ifndef NO_STEAM
		// LAN servers restrict to class b IP addresses
		if ( !CheckIPRestrictions( adr, authProtocol ) )
		{
			RejectConnection( adr, "#Valve_Reject_LAN_Game");
			return NULL;
		}
#endif
									    
		if ( !CheckPassword( adr, password, name ) )
		{
			// failed
			ConMsg ( "%s:  password failed.\n", sAdr.String() );
			// Special rejection handler.
			RejectConnection( adr, "#Valve_Reject_Bad_Password" );
			return NULL;
		}
	}

	if ( m_numGameSlots )
	{
		int numSlotsRequested = splitScreenClients.Count();

		if ( GetNumClients() - GetNumFakeClients() + numSlotsRequested > m_numGameSlots )
		{
			bool bClientReconnecting = false;
			for ( int slot = 0 ; slot < m_Clients.Count() ; slot++ )
			{
				CBaseClient *client = m_Clients[slot];
				if ( client->IsFakeClient() )
					continue;
				if ( client->IsConnected() && adr.CompareAdr ( client->m_NetChannel->GetRemoteAddress() ) )
				{
					ConMsg ( "%s:reconnect\n", sAdr.String() );
					bClientReconnecting = true;
					break;
				}
			}

			if ( ( numSlotsRequested == 1 ) && bClientReconnecting )
			{
				// Allow client to proceed with the connection normally and take over their self
			}
			else
			{
				RejectConnection( adr, "#Valve_Reject_Server_Full" );
				return NULL;	// cannot accept, exceeding game mode slot count
			}
		}
	}

	// Let a GOTV server redirect the client immediately without further
	// handshake and netchannel work if the redirect is known to be required
	// at this point.
	ns_address netAdrRedirect;
	if ( GetRedirectAddressForConnectClient( adr, splitScreenClients, &netAdrRedirect ) )
	{
		if ( netAdrRedirect.IsValid() )
			RejectConnection( adr, "ConnectRedirectAddress:%s\n", ns_address_render( netAdrRedirect ).String() );
		return NULL;
	}

	COM_TimestampedLog( "CBaseServer::ConnectClient:  GetFreeClient" );

	CBaseClient	*client = GetFreeClient( adr );

	if ( !client )
	{
		RejectConnection( adr, "#Valve_Reject_Server_Full" );
		return NULL;	// no free slot found
	}

	int nNextUserID = GetNextUserID();
	if ( !CheckChallengeType( client, nNextUserID, adr, authProtocol, hashedCDkey, cdKeyLen ) ) // we use the client pointer to track steam requests
	{
		return NULL;
	}

#ifndef _HLTVTEST
#ifndef _REPLAYTEST
	if ( !FinishCertificateCheck( adr, authProtocol, hashedCDkey	) )
	{
		return NULL;
	}
#endif
#endif

	// Make sure client user info carries correct cookie
	bool bValidatedUserInfo = false;
	if ( GetReservationCookie() != 0u )
	{
		if ( splitScreenClients.Count() )
		{
			const CMsg_CVars& convars = splitScreenClients[0]->convars();
			for ( int i = 0; i< convars.cvars_size(); ++i )
			{
				const char *cvname = NetMsgGetCVarUsingDictionary( convars.cvars(i) );
				const char *value = convars.cvars(i).value().c_str();
				
				if ( stricmp( cvname, "cl_session" ) )
					continue;

				uint64 uid;
				if ( sscanf( value, "$%llx", &uid ) != 1 )
				{
					Warning( "failed to parse session id %s\n", value );
				}
				else
				{
					if ( uid == GetReservationCookie() )
						bValidatedUserInfo = true;
					else
						Warning( "mismatching cookie from %s, client %llx, server %llx!\n",
							sAdr.String(), uid, GetReservationCookie() );
				}
			}
		}
	}
	else
	{
		bValidatedUserInfo = true;
	}
	// If we failed to validate user info - reject
	if ( !bValidatedUserInfo )
	{
		RejectConnection( adr, "Invalid user info.\n" );
		return NULL;
	}

	// Final validation chance by server.dll
	if ( char const *szGameServerError = serverGameDLL->ClientConnectionValidatePreNetChan( ( this == &sv ), sAdr.String(), authProtocol, client->m_SteamID.ConvertToUint64() ) )
	{
		RejectConnection( adr, "%s", szGameServerError );
		return NULL;
	}

	COM_TimestampedLog( "CBaseServer::ConnectClient:  NET_CreateNetChannel" );

	// Fix the empty name from FCVAR_USERINFO data
	// for ( int k = 0; !name[ 0 ] && ( k < splitScreenPlayers.Count() ); ++k )
	char const *pchClientConnectionName = name;
	if ( !pchClientConnectionName[ 0 ] && splitScreenClients.Count() )
	{
		for ( int iCvar = 0; iCvar < splitScreenClients[ 0 ]->convars().cvars().size(); ++iCvar )
		{
			CMsg_CVars::CVar const &rCvarInfo = splitScreenClients[ 0 ]->convars().cvars( iCvar );
			if ( !V_strcmp( "name", NetMsgGetCVarUsingDictionary( rCvarInfo ) ) )
			{
				pchClientConnectionName = rCvarInfo.value().c_str();
				break;
			}
		}
	}

	// Use an override for connection name
	pchClientConnectionName = serverGameClients->ClientNameHandler( client->m_SteamID.ConvertToUint64(), pchClientConnectionName );

	// create network channel
	// Encryption keys for the client must have been received previously
	INetChannel * netchan = NET_CreateNetChannel( m_Socket, &adr, pchClientConnectionName, client, pbEncryptionKey, false );

	if ( !netchan )
	{
		RejectConnection( adr, "Failed to create net channel.\n");
		return NULL;
	}

	// setup netchannl settings
	netchan->SetChallengeNr( challenge );
	
	COM_TimestampedLog( "CBaseServer::ConnectClient:  client->Connect" );

	// make sure client is reset and clear
	client->Connect( pchClientConnectionName, nNextUserID, netchan,
		false,	// real client
		clientPlatform,
		(splitScreenClients.Count() > 0) ? &splitScreenClients[0]->convars() : NULL ); // userinfo if supplied

	client->m_bLowViolence = isClientLowViolence;

	m_nUserid = nNextUserID;

	// Will get reset from userinfo, but this value comes from sv_updaterate ( the default )
	client->m_fSnapshotInterval = 1.0f/20.0f;
	client->m_fNextMessageTime = net_time + client->m_fSnapshotInterval;
	// Force a full delta update on first packet.
	client->m_nDeltaTick = -1;
	client->m_nSignonTick = 0;
	client->m_nStringTableAckTick = 0;
	client->m_pLastSnapshot = NULL;
	
	// Tell client connection worked, now use netchannels
	NET_OutOfBandPrintf ( m_Socket, adr, "%c.%08X.0000.0000.0000.0000.", S2C_CONNECTION, nEncryptionKeyIndex );

	// Set up client structure.
	if ( authProtocol == PROTOCOL_HASHEDCDKEY )
	{
		// use hased CD key as player GUID
		Q_strncpy ( client->m_GUID, hashedCDkey, SIGNED_GUID_LEN );
		client->m_GUID[SIGNED_GUID_LEN] = '\0';
	}
	else if ( authProtocol == PROTOCOL_STEAM )
	{
		// StartSteamValidation() above initialized the clients networkid
	}

	//now process the split screen clients who came in with this client
	for( int playerIndex = 1; playerIndex < splitScreenClients.Count(); ++ playerIndex )
	{
		ConMsg( "Processing Split Screen connection packet.\n" );

		client->CLCMsg_SplitPlayerConnect( *splitScreenClients[ playerIndex ] );
	}

	if ( netchan && !netchan->IsLoopback() )
	{
		ConMsg("Client \"%s\" connected (%s).\n", client->GetClientName(), cl_hideserverip.GetInt()>0 ? "<ip hidden>" : netchan->GetAddress() );
	}

	// Once someone is successfully on the server, it'll hibernate when client count goes to zero
	m_flReservationExpiryTime = 0.0f;
	m_flTimeLastClientLeft = -1.0f;

	return client;
}

/*
================
RequireValidChallenge

Return true if this server query must provide a valid challenge number
================
*/
bool CBaseServer::RequireValidChallenge( const ns_address &adr )
{
	if ( sv_enableoldqueries.GetBool() == true )
	{
		return false; // don't enforce challenge numbers
	}

	return true;
}

/*
================
ValidChallenge

Return true if this challenge number is correct for this host (for server queries)
================
*/
bool CBaseServer::ValidChallenge( const ns_address & adr, int challengeNr )
{
	if ( !IsActive() )            // Must be running a server.
		return false ;

	if ( !IsMultiplayer() )   // ignore in single player
		return false ;

	if ( RequireValidChallenge( adr) )
	{
		if ( !CheckChallengeNr( adr, challengeNr ) )
		{
			ReplyServerChallenge( adr );
			return false;
		}
	}

	return true; 
}

bool CBaseServer::ValidInfoChallenge( const ns_address & adr, const char *nugget )
{
	if ( !IsActive() )            // Must be running a server.
		return false ;

	if ( !IsMultiplayer() )   // ignore in single player
		return false ;

	if ( RequireValidChallenge( adr) )
	{
		if ( Q_stricmp( nugget, A2S_KEY_STRING ) ) // if the string isn't equal then fail out
		{
			return false;
		}
	}

	return true; 
}


bool CBaseServer::ProcessConnectionlessPacket(netpacket_t * packet)
{
	// NOTE: msg is copy-constructed from packet->message, so reading
	// from "msg" will not advance read-pointer in "packet->message"!!!
	// ... and vice-versa, hence passing "packet" to a nested function
	// will make that function receive the original unprocessed message.
	// [ this differs from client-side connectionless packet processing ]

	if ( !CheckConnectionLessRateLimits( packet->from ) )
	{
		return false;
	}

	bf_read msg = packet->message;	// handy shortcut 

	char c = msg.ReadChar();

	if ( !IsActive() || !Host_ShouldRun() )
	{
		// Server should not be processing most of the traffic in this
		// state except client LAN searches

		if ( IsDedicated() )
			return true;

		if ( c != 0 )
			return true;
	}

	switch ( c )
	{
		case A2A_PING :		NET_OutOfBandPrintf (packet->source, packet->from, "%c00000000000000", A2A_ACK );
							break;
	
		case A2A_PRINT :	// Don't spew to server console, this was being used to spam the server console since we publish server public IP addresses...
							break;

		case A2A_ACK :		ConMsg ("A2A_ACK from %s\n", ns_address_render( packet->from ).String() );
							break;


		case A2S_GETCHALLENGE :  
#if !defined(NO_STEAM)
			// Drop packet if we don't yet have our Steam ID
			// because we're still logging on
			if ( !Steam3Server().BHasLogonResult() )
				break;
#endif
			ReplyChallenge( packet->from, msg );
			break;
		
		case A2S_SERVERQUERY_GETCHALLENGE: ReplyServerChallenge( packet->from );
							break;

#if ENGINE_CONNECT_VIA_MMS
		case C2S_VALIDATE_SESSION: {
										int protocol = msg.ReadLong();
										uint64 uiXuid = msg.ReadLongLong();
										uint64 uiSessionId = msg.ReadLongLong();
										Msg( "C2S_VALIDATE_SESSION from %llx, session %llx, protocol %d.\n", uiXuid, uiSessionId, protocol );

										if ( GetHostVersion() == protocol )
										{
											extern void HostValidateSessionImpl();
											HostValidateSessionImpl();
										}
								   }
								   break;
#endif

		case C2S_CONNECT :	{	char cdkey[STEAM_KEYSIZE];
								char name[256] = {};
								char password[256] = {};

								int protocol = msg.ReadLong();
								int authProtocol = msg.ReadLong();
								int challengeNr = msg.ReadLong();

								msg.ReadString( name, sizeof(name) );
								msg.ReadString( password, sizeof(password) );

								//decode split screen player info from the message
								int numPlayers = msg.ReadByte();
								if ( numPlayers > host_state.max_splitscreen_players || numPlayers < 0 )
								{	// Make sure we reject connection early, otherwise it will trigger Disconnect
									// freeing client inside ConnectClient and cause memory access violation
									DevMsg( "Rejecting connection request from %s, players = %u.\n", ns_address_render( packet->from ).String(), numPlayers );
									RejectConnection( packet->from, "No more split screen slots!" );
									break;
								}

								struct AutoCleanupVectorPlayers_t : public CUtlVector< CCLCMsg_SplitPlayerConnect_t * >
									{ ~AutoCleanupVectorPlayers_t() { PurgeAndDeleteElements(); } }
								splitScreenPlayers;
								if( numPlayers > 0 )
								{
									for( int playerCount = 0; playerCount < numPlayers; ++ playerCount ) // get CLC_SplitPlayerConnect msg for all players even if only one is connecting
									{
										msg.ReadVarInt32(); //the packet type.
										CCLCMsg_SplitPlayerConnect_t *pSplitPlayerConnect = new CCLCMsg_SplitPlayerConnect_t;
										splitScreenPlayers.AddToTail( pSplitPlayerConnect );
										if ( !pSplitPlayerConnect->ReadFromBuffer( msg ) )
										{
											numPlayers = -1; // Trigger an error
										}
										else if ( pSplitPlayerConnect->convars().cvars_size() )
										{	// Make sure convars are expanded using dictionary
											for ( int iCV = 0; iCV < pSplitPlayerConnect->convars().cvars_size(); ++ iCV )
											{
												CMsg_CVars::CVar *convar = pSplitPlayerConnect->mutable_convars()->mutable_cvars( iCV );
												NetMsgExpandCVarUsingDictionary( convar );
											}
										}
									}
								}

								if ( numPlayers < 0 )
								{	// Catch malformed user info
									DevMsg( "Rejecting connection request from %s, malformed userinfo.\n", ns_address_render( packet->from ).String() );
									RejectConnection( packet->from, "No more split screen slots!" );
									break;
								}

								bool isClientLowViolence = msg.ReadOneBit() != 0;

								uint64 nReservationCookie = msg.ReadLongLong();

								CrossPlayPlatform_t clientPlatform = (CrossPlayPlatform_t)msg.ReadByte();
								if( (clientPlatform == CROSSPLAYPLATFORM_UNKNOWN) || (clientPlatform > CROSSPLAYPLATFORM_LAST) )
								{
									DevMsg( "Rejecting connection request from %s, client's cross-play platform is unrecognized.\n", ns_address_render( packet->from ).String() );
									RejectConnection( packet->from, "Invalid cross-play platform id\n" );
									break;
								}

								bool bIsLocalConnection = packet->from.IsLocalhost() || packet->from.IsLoopback();

								// On a listen server, local player always gets on regardless of cookie
								if ( IsDedicated() || !bIsLocalConnection )
								{
									if ( IsExclusiveToLobbyConnections() && 
										m_nReservationCookie != nReservationCookie )
									{
										DevMsg( "Rejecting connection request from %s, client's reservation cookie %llx does not match servers's cookie %llx.\n",
											ns_address_render( packet->from ).String(), nReservationCookie, m_nReservationCookie );
										RejectConnection( packet->from, "#Valve_Reject_Connect_From_Lobby" );
										break;
									}

									// if this server is currently reserved, only allow connection if the client provides the cookie the server was reserved with
									if ( IsReserved() && m_nReservationCookie != nReservationCookie )
									{
										DevMsg( "Rejecting connection request from %s (reservation cookie 0x%llx), server is reserved with reservation cookie 0x%llx "
											"for %.1f more seconds\n", ns_address_render( packet->from ).String(), nReservationCookie, m_nReservationCookie, 
											( m_flReservationExpiryTime - net_time ) );
										RejectConnection( packet->from, "#Valve_Reject_Reserved_For_Lobby" );
										break;
									}

									// Check if we are running with a special flag and not advertise to Steam
									if ( !V_strcmp( g_pLaunchOptions->GetFirstSubKey()->GetNextKey()->GetNextKey()->GetString(), "server_is_unavailable" ) )
									{
										DevMsg( "Rejecting connection request from %s (reservation cookie 0x%llx), server is reserved with reservation cookie 0x%llx "
											"for %.1f more seconds, running with tag server_is_unavailable\n", ns_address_render( packet->from ).String(), nReservationCookie, m_nReservationCookie, 
											( m_flReservationExpiryTime - net_time ) );
										RejectConnection( packet->from, "#Valve_Reject_Workshop_Loading" );
										break;
									}
								}

								//
								// Get client certificate
								//
								const byte *pbNetEncryptPrivateKey = NULL;
								int cbNetEncryptPrivateKey = 0;
								bool bNetEncryptPrivateKey = NET_CryptGetNetworkCertificate( k_ENetworkCertificate_PrivateKey, &pbNetEncryptPrivateKey, &cbNetEncryptPrivateKey );
								
								// Read whether the client would like to use encryption key?
								byte *pbClientPlainKey = NULL;
								int nEncryptionKeyIndex = msg.ReadLong();

								// Peek into client-supplied speculative accountid
								static bool s_bExternalCryptKeys = ( CommandLine()->CheckParm( "-externalnetworkcryptkey" ) != NULL );
								AccountID_t unAccountIDfor3rdParties = 0;
								if ( s_bExternalCryptKeys && splitScreenPlayers.Count() )
								{
									const CMsg_CVars& convars = splitScreenPlayers[ 0 ]->convars();
									for ( int i = 0; i < convars.cvars_size(); ++i )
									{
										const char *cvname = NetMsgGetCVarUsingDictionary( convars.cvars( i ) );
										const char *value = convars.cvars( i ).value().c_str();

										if ( stricmp( cvname, "accountid" ) )
											continue;

										if ( unAccountIDfor3rdParties )
										{
											unAccountIDfor3rdParties = 0;
											break;
										}

										unAccountIDfor3rdParties = Q_atoi( value );
									}
								}

								// Community-servers feature to manage encryption keys in their secure clients
								if ( s_bExternalCryptKeys )
								{
									bool bClientWantsToUseCryptKey = ( nEncryptionKeyIndex != 0 );
									bNetEncryptPrivateKey = g_pServerPluginHandler->BNetworkCryptKeyCheckRequired( packet->from.GetIP(), packet->from.GetPort(),
										unAccountIDfor3rdParties, bClientWantsToUseCryptKey );
								}

								if ( nEncryptionKeyIndex )
								{
									int numEncryptedBytes = msg.ReadLong();
									if ( ( numEncryptedBytes > 0 ) && ( numEncryptedBytes <= 256 ) )
									{
										byte *pbEncryptedKey = ( byte * ) stackalloc( numEncryptedBytes );
										msg.ReadBytes( pbEncryptedKey, numEncryptedBytes );

										// Community-servers feature to manage encryption keys in their secure clients
										if ( s_bExternalCryptKeys )
										{
											// Client can send whatever gibberish they want, plugins manage the encryption keys
											byte chPlainKey[ 1024 ] = {};
											if ( g_pServerPluginHandler->BNetworkCryptKeyValidate( packet->from.GetIP(), packet->from.GetPort(), unAccountIDfor3rdParties,
												nEncryptionKeyIndex, numEncryptedBytes, pbEncryptedKey, chPlainKey ) )
											{
												pbClientPlainKey = ( byte * ) stackalloc( NET_CRYPT_KEY_LENGTH );
												Q_memcpy( pbClientPlainKey, chPlainKey, NET_CRYPT_KEY_LENGTH );
											}
										}
										else
										{
											//
											// Server will send the certificate and its signature down to the client
											//
											byte chPlainKey[ 1024 ] = {};
											if ( bNetEncryptPrivateKey && NET_CryptVerifyClientSessionKey( serverGameDLL->IsValveDS(),
												pbNetEncryptPrivateKey, cbNetEncryptPrivateKey,
												pbEncryptedKey, numEncryptedBytes,
												chPlainKey, Q_ARRAYSIZE( chPlainKey ) ) )
											{
												pbClientPlainKey = ( byte * ) stackalloc( NET_CRYPT_KEY_LENGTH );
												Q_memcpy( pbClientPlainKey, chPlainKey, NET_CRYPT_KEY_LENGTH );
											}
										}
									}
								}

								if ( ( this == &sv ) && bNetEncryptPrivateKey && !pbClientPlainKey )
								{
									DevMsg( "Rejecting connection request from %s, no certificate.\n", ns_address_render( packet->from ).String() );
									RejectConnection( packet->from, "Invalid certificate\n" );
									break;
								}

								if ( authProtocol == PROTOCOL_STEAM )
								{
									int keyLen = msg.ReadShort();
									if ( keyLen < 0 || keyLen > sizeof(cdkey) )
									{
										RejectConnection( packet->from, "Invalid Steam key length\n" );
										break;
									}
									msg.ReadBytes( cdkey, keyLen );

									ConnectClient( packet->from, protocol, 
										challengeNr, authProtocol, name, password, cdkey, keyLen, splitScreenPlayers, isClientLowViolence, clientPlatform,
										pbClientPlainKey, nEncryptionKeyIndex );	// cd key is actually a raw encrypted key	
								}
								else
								{
									msg.ReadString( cdkey, sizeof(cdkey) );
									ConnectClient( packet->from, protocol, 
										challengeNr, authProtocol, name, password, cdkey, strlen(cdkey), splitScreenPlayers, isClientLowViolence, clientPlatform,
										pbClientPlainKey, nEncryptionKeyIndex );
								}

							}
							break;
		
		case A2A_CUSTOM	:	// TODO fire game event with string and adr so 3rd party tool can get it
			break;

#if ENGINE_CONNECT_VIA_MMS
		case A2S_RESERVE :	ReplyReservationRequest( packet->from, msg );
							break;
#endif

		case A2S_RESERVE_CHECK : ReplyReservationCheckRequest( packet->from, msg );
								 break;

		case A2S_PING: 
			{
				if ( msg.ReadLong() == GetHostVersion() )
				{
					uint32 token = msg.ReadLong();
			
					char	buffer[64];
					bf_write msg(buffer,sizeof(buffer));

					msg.WriteLong( CONNECTIONLESS_HEADER );
					msg.WriteByte( S2A_PING_RESPONSE );
					msg.WriteLong( GetHostVersion() );
					msg.WriteLong( token );

					NET_SendPacket( NULL, m_Socket, packet->from, msg.GetData(), msg.GetNumBytesWritten() );
				}

				break;
			}

		case 0 :
			{
				// Feed into matchmaking
				KeyValues *notify = new KeyValues( "OnNetLanConnectionlessPacket" );
				notify->SetPtr( "rawpkt", packet );
				g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( notify );
			}
			break;

		case A2S_INFO:
			// Handle A2S_INFO ourselves, we are concealing SteamID of the game server and report players differently
			if ( g_bSteamMasterHeartbeatsEnabled )
			{
				extern ConVar host_info_show;
				switch ( host_info_show.GetInt() )
				{
				case 2:
					goto steam3server_oob_packet_handler;

				case 1:
					{
						  // Validate challenge
						  char nugget[ 64 ];
						  if ( !msg.ReadString( nugget, sizeof( nugget ) ) )
							  break;
						  if ( !ValidInfoChallenge( packet->from, nugget ) )
							  break;

						  // Respond
						  extern AppId_t g_unSteamAppID;
						  AppId_t appIdResponse = g_unSteamAppID ? g_unSteamAppID : 730;
						  CUtlBuffer buf;
						  buf.EnsureCapacity( MAX_ROUTABLE_PAYLOAD );

						  buf.PutUnsignedInt( LittleDWord( CONNECTIONLESS_HEADER ) );
						  buf.PutUnsignedChar( S2A_INFO_SRC );
						  buf.PutUnsignedChar( 17 ); // Hardcoded protocol version number
						  extern ConVar host_name_store;
						  buf.PutString( host_name_store.GetBool() ? GetName() : "Counter-Strike: Global Offensive" );
						  buf.PutString( GetMapName() );
						  buf.PutString( "csgo" );
						  buf.PutString( "Counter-Strike: Global Offensive" );

						  // The next field is a 16-bit version of the AppID.  If our AppID < 65536,
						  // then let's go ahead and put in in there, to maximize compatibility
						  // with old clients who might be only using this field but not the new one.
						  // However, if our AppID won't fit, there's no way we can be compatible,
						  // anyway, so just put in a zero, which is better than a bogus AppID.
						  uint16 usAppIdShort = ( uint16 ) appIdResponse;
						  buf.PutShort( LittleWord( usAppIdShort ) );

						  // player info
						  buf.PutUnsignedChar( GetNumPlayers() );
						  buf.PutUnsignedChar( GetMaxHumanPlayers() );
						  buf.PutUnsignedChar( MAX( 0, GetNumFakeClients() - GetNumProxies() ) );

						  // NOTE: This key's meaning is changed in the new version. Since we send gameport and specport,
						  // it knows whether we're running SourceTV or not. Then it only needs to know if we're a dedicated or listen server.
						  if ( IsDedicated() )
							  buf.PutUnsignedChar( 'd' );	// d = dedicated server
						  else
							  buf.PutUnsignedChar( 'l' );	// l = listen server

#if defined(_WIN32)
						  buf.PutUnsignedChar( 'w' );
#elif defined(OSX)
						  buf.PutUnsignedChar( 'm' );
#else // LINUX?
						  buf.PutUnsignedChar( 'l' );
#endif

						  // Password?
						  buf.PutUnsignedChar( GetPassword() ? 1 : 0 );
						  buf.PutUnsignedChar( Steam3Server().BSecure() ? 1 : 0 );
						  buf.PutString( GetHostVersionString() );

						  //
						  // NEW DATA.
						  //

						  // Write a byte with some flags that describe what is to follow.
						  byte nNewFlags = 0;
						  nNewFlags |= S2A_EXTRA_DATA_HAS_GAME_PORT;

						  // 				if ( GetSteamID().IsValid() )
						  // 					nNewFlags |= S2A_EXTRA_DATA_HAS_STEAMID;

						  extern bool CanShowHostTvStatus();
						  CHLTVServer *hltv = NULL;
						  for ( CActiveHltvServerIterator it; it; it.Next() )
						  {
							  if ( CanShowHostTvStatus() )
							  {
								  hltv = it;
								  // pass spectator data (port and name)
								  nNewFlags |= S2A_EXTRA_DATA_HAS_SPECTATOR_DATA;
							  }
							  break;
						  }

						  if ( m_GameType.String()[ 0 ] != '\0' )
							  nNewFlags |= S2A_EXTRA_DATA_HAS_GAMETAG_DATA;

						  nNewFlags |= S2A_EXTRA_DATA_GAMEID;

						  buf.PutUnsignedChar( nNewFlags );

						  // Write the rest of the data.
						  if ( nNewFlags & S2A_EXTRA_DATA_HAS_GAME_PORT )
						  {
							  buf.PutShort( LittleWord( GetUDPPort() ) );
						  }

						  if ( nNewFlags & S2A_EXTRA_DATA_HAS_SPECTATOR_DATA )
						  {
							  buf.PutShort( LittleWord( hltv->GetUDPPort() ) );
							  buf.PutString( host_name_store.GetBool() ? GetName() : "Counter-Strike: Global Offensive" );
						  }

						  if ( nNewFlags & S2A_EXTRA_DATA_HAS_GAMETAG_DATA )
						  {
							  buf.PutString( m_GameType.String() );
						  }

						  if ( nNewFlags & S2A_EXTRA_DATA_GAMEID )
						  {
							  // !FIXME! Is there a reason we aren't using the other half
							  // of this field?  Shouldn't we put the game mod ID in there, too?
							  // We have the game dir.
							  buf.PutInt64( LittleQWord( CGameID( appIdResponse ).ToUint64() ) );
						  }

						  NET_SendPacket( NULL, m_Socket, packet->from, ( unsigned char * ) buf.Base(), buf.TellPut() );
					}
					break;
				}
			}
			break;

		case A2S_PLAYER:
			// Handle A2S_PLAYER ourselves, we are concealing SteamID of the game server and report players differently
			if ( g_bSteamMasterHeartbeatsEnabled )
			{
				extern ConVar host_players_show;
				switch ( host_players_show.GetInt() )
				{
				case 2:
					goto steam3server_oob_packet_handler;
				case 1:
					{
						CUtlBuffer buf;
						buf.EnsureCapacity( MAX_ROUTABLE_PAYLOAD );

						buf.PutUnsignedInt( LittleDWord( CONNECTIONLESS_HEADER ) );
						buf.PutUnsignedChar( S2A_PLAYER );
						buf.PutUnsignedChar( 1 );
						buf.PutUnsignedChar( 0 );
						buf.PutString( "Max Players" );
						buf.PutInt( GetMaxHumanPlayers() );

						if ( m_fStartTime == 0 ) // record when we started
						{
							m_fStartTime = Sys_FloatTime();
						}
						float flTime = Sys_FloatTime() - m_fStartTime;
						float flLittleTime;
						LittleFloat( &flLittleTime, &flTime );
						buf.PutFloat( flLittleTime );

						NET_SendPacket( NULL, m_Socket, packet->from, ( unsigned char * ) buf.Base(), buf.TellPut() );
					}
					break;
				}
			}
			break;

		case A2S_RULES:
			if ( g_bSteamMasterHeartbeatsEnabled )
			{
				extern ConVar host_rules_show;
				if ( host_rules_show.GetBool() )
					goto steam3server_oob_packet_handler;
			}
			break;

		default:
		{
			static bool s_bEnableLegacyPackets = !!CommandLine()->FindParm( "-enablelegacypackets" );
			if ( !s_bEnableLegacyPackets )
				break;

			steam3server_oob_packet_handler:

			// We don't understand it, let the master server updater at it.
			if ( packet->from.IsType< netadr_t >() && Steam3Server().SteamGameServer() && Steam3Server().IsMasterServerUpdaterSharingGameSocket() )
			{
				const netadr_t &adr = packet->from.AsType< netadr_t>();
				Steam3Server().SteamGameServer()->HandleIncomingPacket( 
					packet->message.GetBasePointer(), 
					packet->message.TotalBytesAvailable(),
					adr.GetIPHostByteOrder(),
					adr.GetPort()
					);
			
				// This is where it will usually want to respond to something immediately by sending some
				// packets, so check for that immediately.
				ForwardPacketsFromMasterServerUpdater();
			}
		}
		break;
	}

	return true;
}

int CBaseServer::GetNumFakeClients() const
{
	int count = 0; 
	for ( int i = 0; i < m_Clients.Count(); i++ )
	{
		if ( m_Clients[i]->IsConnected() &&
			 m_Clients[i]->IsFakeClient() )
		{
			count++;
		}
	}
	return count;
}

/*
==================
void SV_CountPlayers

Counts number of connections.  Clients includes regular connections
==================
*/
int CBaseServer::GetNumClients( void ) const
{
	int count	= 0;

	for (int i=0 ; i < m_Clients.Count() ; i++ )
	{
		if ( m_Clients[ i ]->IsConnected() )
		{
			count++;
		}
	}

	return count;
}

/*
==================
void SV_CountPlayers

Counts number of HLTV and Replay connections.  Clients includes regular connections
==================
*/
int CBaseServer::GetNumProxies( void ) const
{
	int count	= 0;

	for (int i=0 ; i < m_Clients.Count() ; i++ )
	{
		if ( m_Clients[ i ]->IsConnected() && (m_Clients[ i ]->IsHLTV()
#if defined( REPLAY_ENABLED )
			|| m_Clients[ i ]->IsReplay()
#endif
			) )
		{
			count++;
		}
	}

	return count;
}

int CBaseServer::GetNumPlayers()
{
	int count = 0;
	if ( !GetUserInfoTable())
	{
		return 0;
	}

	const int maxPlayers = GetUserInfoTable()->GetNumStrings();

	for ( int i=0; i < maxPlayers; i++ )
	{
		const player_info_t *pi = (const player_info_t *) m_pUserInfoTable->GetStringUserData( i, NULL );
		// WARNING: using raw bytes access to player info instead of GetPlayerInfo to save server CPU when
		// responding to server info queries. Structure not byteswapped, must use GetPlayerInfo.

		if ( !pi )
			continue;

		if ( pi->fakeplayer )
			continue;	// don't count bots

		count++;
	}

	return count;
}

bool CBaseServer::GetPlayerInfo( int nClientIndex, player_info_t *pinfo )
{
	if ( !pinfo || !GetUserInfoTable() )
		return false;

	if ( nClientIndex < 0 || nClientIndex >= GetUserInfoTable()->GetNumStrings() )
	{
		Q_memset( pinfo, 0, sizeof( player_info_t ) );
		return false;
	}

	player_info_t *pi = (player_info_t*) GetUserInfoTable()->GetStringUserData( nClientIndex, NULL );

	if ( !pi )
	{
		Q_memset( pinfo, 0, sizeof( player_info_t ) );
		return false;	
	}

	Q_memcpy( pinfo, pi, sizeof( player_info_t ) );

	// Fixup from network order (big endian)
	CByteswap byteswap;
	byteswap.SetTargetBigEndian( true );
	byteswap.SwapFieldsToTargetEndian( pinfo );

	return true;
}


void CBaseServer::UserInfoChanged( int nClientIndex )
{
	if ( !m_pUserInfoTable )
		return;

	player_info_t pi;

	bool oldlock = networkStringTableContainerServer->Lock( false );
	if ( m_Clients[ nClientIndex ]->FillUserInfo( pi ) )
	{
		// Fixup to network order (big endian)
		CByteswap byteswap;
		byteswap.SetTargetBigEndian( true );
		byteswap.SwapFieldsToTargetEndian( &pi );

		// update user info settings
		m_pUserInfoTable->SetStringUserData( nClientIndex, sizeof(pi), &pi );
	}
	else
	{
		// delete user data settings
		m_pUserInfoTable->SetStringUserData( nClientIndex, 0, NULL );
	}
	networkStringTableContainerServer->Lock( oldlock );
	
}

void CBaseServer::FillServerInfo(CSVCMsg_ServerInfo &serverinfo)
{
	char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	serverinfo.set_protocol( GetHostVersion() );
	serverinfo.set_server_count( GetSpawnCount() );
	serverinfo.set_map_crc( worldmapCRC );
	serverinfo.set_client_crc( clientDllCRC );
	serverinfo.set_string_table_crc( CRC32_ConvertToUnsignedLong( &stringTableCRC ) );
	serverinfo.set_max_clients( GetMaxClients() );
	serverinfo.set_max_classes( serverclasses );
	serverinfo.set_is_dedicated( IsDedicated() );
	
#ifdef _WIN32
	serverinfo.set_c_os( 'W' );
#else
	serverinfo.set_c_os( 'L' );
#endif

	// HACK to signal that the server is "new"
	serverinfo.set_c_os( tolower( serverinfo.c_os() ) );

	serverinfo.set_tick_interval( GetTickInterval() );
	serverinfo.set_game_dir( gamedir );
	serverinfo.set_map_name( GetMapName() );
	serverinfo.set_map_group_name( GetMapGroupName() );
	serverinfo.set_sky_name( m_szSkyname );
	extern ConVar host_name_store;
	serverinfo.set_host_name( host_name_store.GetBool() ? GetName() : "Counter-Strike: Global Offensive" );
	serverinfo.set_is_hltv( IsHLTV() );
	serverinfo.set_is_redirecting_to_proxy_relay( false );
	
	char szMapPath[MAX_PATH];
	V_ComposeFileName( "maps", GetMapName(), szMapPath, sizeof(szMapPath) );
	serverinfo.set_ugc_map_id( serverGameDLL->GetUGCMapFileID( szMapPath ) );

#if defined( REPLAY_ENABLED )
	serverinfo.set_is_replay( IsReplay() );
#endif

// Don't expose server public IP in the server info, the client is already connected, so there's no reason to store it either
// 	if( Steam3Server().SteamGameServer() == NULL )
// 		serverinfo.set_public_ip( NULL );
// 	else
// 		serverinfo.set_public_ip( Steam3Server().SteamGameServer()->GetPublicIP() );
}

/*
=================
SVC_GetChallenge

Returns a challenge number that can be used
in a subsequent client_connect command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.
=================
*/

void CBaseServer::ReplyChallenge( const ns_address &adr, bf_read &inmsg )
{
	if ( !CanAcceptChallengesFrom( adr ) )
		// Silently swallow the challenge request
		return;

	// Check if we are running with a special flag and not advertise to Steam
	if ( !V_strcmp( g_pLaunchOptions->GetFirstSubKey()->GetNextKey()->GetNextKey()->GetString(), "server_is_unavailable" ) )
	{
		DevMsg( "Server running with server_is_unavailable, ignoring challenge from %s\n", ns_address_render( adr ).String() );
		return;
	}

	char	ALIGN4 buffer[512] ALIGN4_POST;
	bf_write msg(buffer,sizeof(buffer));

	char context[ 256 ] = { 0 };
	inmsg.ReadString( context, sizeof( context ) );

	// get a free challenge number
	int challengeNr = GetChallengeNr( adr );
	int	authprotocol = GetChallengeType( adr );

	msg.WriteLong( CONNECTIONLESS_HEADER );
	
	msg.WriteByte( S2C_CHALLENGE );
	msg.WriteLong( challengeNr );
	msg.WriteLong( authprotocol );

	bool bWroteInfo = false;
	uint64 ullSteamIDGS = 0ull;

#if !defined( NO_STEAM )
	ullSteamIDGS = Steam3Server().GetGSSteamID().ConvertToUint64();
	if ( authprotocol == PROTOCOL_STEAM )
	{
		msg.WriteShort( 0 ); //  steam2 encryption key not there anymore
		msg.WriteLongLong( ullSteamIDGS );
		msg.WriteByte( Steam3Server().BSecure() );
		bWroteInfo = true;
	}
#endif

	if ( !bWroteInfo )
	{
		msg.WriteShort( 1 );
		msg.WriteLongLong( ullSteamIDGS );
		msg.WriteByte( 0 );
	}

	bool bReloadServerMap = false;

	if ( StringHasPrefix( context, "connect" ) )
	{
		bool bIsLocalConnection = adr.IsLoopback() || adr.IsLocalhost();
		// A local player never has to enter a PW
		bool bWillRequirePassword = ( GetPassword() != NULL ) && 
			!bIsLocalConnection;

		// Validate the challenge if the challenge was passed as part of connect
		// packet:
		unsigned long nChallenge = 0;
		char const *pszValidate = context + Q_strlen( "connect" );
		if ( pszValidate[0] == '0' && pszValidate[1] == 'x' )
		{
			char chValidateChallenge[ 9 ] = {0};
			Q_strncpy( chValidateChallenge, pszValidate + 2, sizeof( chValidateChallenge ) );
			nChallenge = strtoul( chValidateChallenge, NULL, 16 );
		}

		bool bAllowDC = !serverGameDLL->IsValveDS();	// Official DS direct connect disabled
		if ( bAllowDC )
			bAllowDC = serverGameDLL->ShouldAllowDirectConnect();	// let the ongoing game overrule direct connect

		if ( bAllowDC )
		{
			//
			// Game server must be logged on with a persistent GSLT unless LAN case
			//
			static const bool s_bAllowLanWhitelist = !CommandLine()->FindParm( "-ignorelanwhitelist" );
			bool bWhitelistedClient = bIsLocalConnection															// localhost/loopback
				|| ( s_bAllowLanWhitelist && (
					adr.IsReservedAdr()																				// LAN RFC 1918
				|| ( ( adr.GetAddressType() == NSAT_NETADR ) && ( adr.GetIP() == Steam3Server().GetPublicIP() ) )	// same public IP (pinhole NAT or same host in DMZ)
				) );	
			bool bPersistentGameServerAccount = !sv_lan.GetBool() && !Steam3Server().BLanOnly()
				&& Steam3Server().GetGSSteamID().IsValid() && Steam3Server().GetGSSteamID().BPersistentGameServerAccount();
			if ( IsDedicated() && !bWhitelistedClient && !bPersistentGameServerAccount )
				bAllowDC = false;	// just fail direct connect
		}

		if ( IsExclusiveToLobbyConnections() && !GetReservationCookie() )
		{
			// Server is in the state when it requires lobby connection, but
			// is unreserved.

			// For untrusted clients: we need to tell them to retry with
			// a valid challenge
			if ( nChallenge != ( unsigned long ) challengeNr )
			{
				msg.WriteString( "connect-retry" );
			}
			// For trusted clients: give them a grace period to establish
			// the required lobby and submit a reservation request
			else
			{
				if ( !bAllowDC )
				{
					if ( serverGameDLL->IsValveDS() )
					{
						// Do not allow direct-connect to Valve dedicated servers
						msg.WriteString( "connect-matchmaking-only" );
						DevMsg( "Cannot direct-connect to Valve CS:GO Server\n" );
					}
					else
					{
						// Do not allow direct-connect to this community server
						msg.WriteString( "connect-lan-only" );
						DevMsg( "Cannot direct-connect to community CS:GO Server, visit http://steamcommunity.com/dev/managegameservers\n" );

						static bool s_bPrintedInfo = false;
						if ( sv.IsDedicated() && !s_bPrintedInfo )
						{
							s_bPrintedInfo = true;

							Warning( "****************************************************\n" );
							Warning( "*  Client connections are restricted to LAN only.  *\n" );
							Warning( "*  Double-check your sv_setsteamaccount setting.   *\n" );
							Warning( "*  To create a game server account go to           *\n" );
							Warning( "*  http://steamcommunity.com/dev/managegameservers *\n" );
							Warning( "****************************************************\n" );
						}
					}
				}
				else
				{
					#if ENGINE_CONNECT_VIA_MMS
					msg.WriteString( "connect-granted" );
				
					m_flTimeReservationGraceStarted = net_time;
					m_adrReservationGraceStarted = adr;

					DevMsg( "Server requires lobby reservation and is unreserved: granting reservation grace period to %s\n", ns_address_render( adr ).String() );
					#else
					if ( m_pnReservationCookieSession && *m_pnReservationCookieSession )
					{
						// Reserve in place
						m_flReservationExpiryTime = net_time + sv_mmqueue_reservation_timeout.GetFloat();
						SetReservationCookie( *m_pnReservationCookieSession, "[R] Connect from %s", ns_address_render( adr ).String() );
						bReloadServerMap = true;
						
						// Reply with whatever context was requested
						msg.WriteString( context );
					}
					else
					{
						DevMsg( "Server is not connected to matchmaking backend, ignoring challenge from %s\n", ns_address_render( adr ).String() );
						return;
					}
					#endif
				}
			}
		}
		else
		{
			// Otherwise just reply with whatever context was requested
			msg.WriteString( context );
		}

		char const *pszLobbyType = "";
		if ( IsExclusiveToLobbyConnections() )
		{
			pszLobbyType = "public";
			if ( sv_steamgroup_exclusive.GetBool() || GetPassword() )
			{
				pszLobbyType = "friends";
			}
		}

		msg.WriteLong( GetHostVersion() );
		msg.WriteString( pszLobbyType );

		msg.WriteByte( bWillRequirePassword ? 1 : 0 );

		#if ENGINE_CONNECT_VIA_MMS
		if ( !bAllowDC )
		{
			msg.WriteLongLong( (uint64)-1 );
			msg.WriteByte( 0 );
		}
		else
		{
			if ( !IsHLTV() )
			{
				msg.WriteLongLong( sv.GetReservationCookie() );
			}
			else
			{
				msg.WriteLongLong( 0 );
			}
			static ConVar *dcFriendsReqd = cvar->FindVar( "sv_dc_friends_reqd" );
			
			if ( !IsHLTV() && dcFriendsReqd )
			{
				msg.WriteByte( dcFriendsReqd->GetInt() );
			}
			else
			{
				msg.WriteByte( 0 );
			}			
		}
		#else
		msg.WriteLongLong( ( uint64 ) -1 );
		msg.WriteByte( 0 );
		#endif

		msg.WriteByte( serverGameDLL->IsValveDS() ? 1 : 0 );
		
		//
		// Server will send the certificate and its signature down to the client
		//
		const byte *pbNetEncryptPublic = NULL;
		int cbNetEncryptPublic = 0;
		const byte *pbNetEncryptSignature = NULL;
		int cbNetEncryptSignature = 0;
		bool bServerRequiresEncryptedChannels = ( ( this == &sv ) &&
			NET_CryptGetNetworkCertificate( k_ENetworkCertificate_PublicKey, &pbNetEncryptPublic, &cbNetEncryptPublic ) &&
			NET_CryptGetNetworkCertificate( k_ENetworkCertificate_Signature, &pbNetEncryptSignature, &cbNetEncryptSignature ) );
		msg.WriteByte( bServerRequiresEncryptedChannels ? 1 : 0 );
		if ( bServerRequiresEncryptedChannels )
		{
			msg.WriteLong( cbNetEncryptPublic );
			msg.WriteBytes( pbNetEncryptPublic, cbNetEncryptPublic );
			msg.WriteLong( cbNetEncryptSignature );
			msg.WriteBytes( pbNetEncryptSignature, cbNetEncryptSignature );
		}
	}
	else
	{
		msg.WriteString( context );
	}

	NET_SendPacket( NULL, m_Socket, adr, msg.GetData(), msg.GetNumBytesWritten() );

	//
	// Force the map to be reloaded upon first connection
	//
	if ( bReloadServerMap && IsDedicated() )
	{
		Cbuf_AddText( CBUF_SERVER, CFmtStr( "nextlevel %s\n", GetMapName() ) ); // gamerules will clean it up when they construct for the next map
		Cbuf_AddText( CBUF_SERVER, CFmtStr( "map %s reserved\n", GetMapName() ) );
		Cbuf_Execute();
	}
}


/*
=================
ReplyServerChallenge

Returns a challenge number that can be used
in a subsequent server query commands.
We do this to prevent DDoS attacks via bandwidth
amplification.
=================
*/
void CBaseServer::ReplyServerChallenge(const ns_address &adr)
{
	char	buffer[16];
	bf_write msg(buffer,sizeof(buffer));

	// get a free challenge number
	int challengeNr = GetChallengeNr( adr );
	
	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( S2C_CHALLENGE );
	msg.WriteLong( challengeNr );
	NET_SendPacket( NULL, m_Socket, adr, msg.GetData(), msg.GetNumBytesWritten() );
}

//-----------------------------------------------------------------------------
// Purpose: encrypts an 8-byte sequence
//-----------------------------------------------------------------------------
static inline void Decrypt8ByteSequence( IceKey& cipher, const unsigned char *cipherText, unsigned char *plainText )
{
	cipher.decrypt( cipherText, plainText );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void DecryptBuffer( IceKey& cipher, unsigned char *bufData, uint bufferSize)
{
	unsigned char *cipherText = bufData;
	unsigned char *plainText = bufData;
	uint bytesEncrypted = 0;

	while (bytesEncrypted < bufferSize)
	{
		// encrypt 8 byte section
		Decrypt8ByteSequence( cipher, cipherText, plainText );
		bytesEncrypted += 8;
		cipherText += 8;
		plainText += 8;
	}
	}


//-----------------------------------------------------------------------------
// Purpose: Replies to a reservation request sent by the client
//-----------------------------------------------------------------------------
void CBaseServer::ReplyReservationRequest( const ns_address &adr, bf_read &msgIn )
{
	if ( msgIn.ReadLong() != GetHostVersion() )
		return;

	byte decrypted[ 1024 ];

	bool bAccepted = false;
	static char const * s_pchTournamentServer = CommandLine()->ParmValue( "-tournament", ( char const * ) NULL );
	if ( s_pchTournamentServer )
	{
		DevMsg( "Reservation request from %s rejected: server running in tournament mode for '%s'\n", 
			ns_address_render( adr ).String(), s_pchTournamentServer );
	}
	else if ( IsReserved() )
	{
		// Special case -- server is reserved, but takes too long
		// to switch the map, client is retrying meanwhile, we don't want
		// to blow them off with a failure code!
		if ( m_ReservationStatus.m_bActive && m_ReservationStatus.m_bSuccess && m_ReservationStatus.m_Remote.CompareAdr( adr ) )
		{
			DevMsg( "Ignoring follow-up reservation request from %s: server reservation transition in progress (for %d more seconds)\n", 
				ns_address_render( adr ).String(), (int) ( m_flReservationExpiryTime - net_time ) );
			SendReservationStatus( kEReservationStatusPending );
			return;
		}

		DevMsg( "Reservation request from %s rejected: server already reserved (for %d more seconds)\n", 
			ns_address_render( adr ).String(), (int) ( m_flReservationExpiryTime - net_time ) );
	}
	else if ( GetNumClients() - GetNumFakeClients() > 0 )
	{
		DevMsg( "Reservation request from %s rejected: server not empty\n", 
			ns_address_render( adr ).String() );
	}
	else if ( !V_strcmp( g_pLaunchOptions->GetFirstSubKey()->GetNextKey()->GetNextKey()->GetString(), "server_is_unavailable" ) )
	{
		DevMsg( "Reservation request from %s rejected: server is running with flag server_is_unavailable\n", 
			ns_address_render( adr ).String() );
	}
	else
	{
		bool bOkay = true;
		if ( !IsX360() && !IsDedicatedForXbox() )
		{
			int nChallengeNr = GetChallengeNr( adr );
			if ( !CanAcceptChallengesFrom( adr ) )
			{
				DevMsg( "Reservation request from address %s, but challenges exclusive for %s\n",
					ns_address_render( adr ).String(), m_adrReservationGraceStarted.ToString() );
				bOkay = false;
			}
			else if ( !nChallengeNr )
			{
				DevMsg( "Reservation request from unknown address %s\n", ns_address_render( adr ).String() );
				bOkay = false;
			}
			else
			{
				int payloadSize = msgIn.ReadLong();
				if ( payloadSize <= 0 || payloadSize > sizeof( decrypted ) || ( payloadSize % 8 ) )
				{
					DevMsg( "ReplyReservationRequest:  Reservation request with bogus payload size from %s [%d bytes]\n", ns_address_render( adr ).String(), payloadSize );
					bOkay = false;
				}
				else
				{
					IceKey cipher(1); /* medium encryption level */
					unsigned char ucEncryptionKey[8] = { 0 };
					*( int * )&ucEncryptionKey[ 0 ] = LittleDWord( nChallengeNr ^ 0x5ef8ce12 );
					*( int * )&ucEncryptionKey[ 4 ] = LittleDWord( nChallengeNr ^ 0xaa98e42c );

					cipher.set( ucEncryptionKey );

					msgIn.ReadBytes( decrypted, payloadSize );
					// Try and decrypt it
					DecryptBuffer( cipher, decrypted, payloadSize );

					// Rewind and use decrypted payload
					msgIn.StartReading( decrypted, payloadSize );

					unsigned int nMagic = msgIn.ReadLong();
					if ( nMagic == 0xfeedbeef )
					{
						bOkay = true;
					}
					else
					{
						Msg( "ReplyReservationRequest:  Reservation request with bogus payload data from %s [%d bytes]\n", ns_address_render( adr ).String(), payloadSize );
					}
				}
			}
		}

		if ( bOkay )
		{
			//
			// Parse the incoming message
			//
			uint64	nReservationCookie;
			int		nSettingsSize;

			nReservationCookie = msgIn.ReadLongLong();
			nSettingsSize = msgIn.ReadLong();

			float flExpiryTime = sv_reservation_timeout.GetFloat();

			DevMsg( "Reservation request from %s accepted: server reserved with reservation cookie 0x%llx for %.1f seconds\n", 
				ns_address_render( adr ).String(), nReservationCookie, flExpiryTime );
			DevMsg( "            settings size = %d\n", nSettingsSize );

			//
			// Establish reservation time
			//
			bAccepted = true;

			// This time will be held until the first client connects, at which time the reservation will stay
			//  in effect until the last client leaves the server
			m_flReservationExpiryTime = net_time + flExpiryTime;

			SetReservationCookie( nReservationCookie, "ReplyReservationRequest" );

			//
			// Process game settings
			//
			m_numGameSlots = 0;
			if ( nSettingsSize > 0 )
			{
				KeyValues *pKV = new KeyValues("");
				KeyValues::AutoDelete autodelete_pKV( pKV );
				byte tmp[MAX_OOB_KEYVALUES];
				Assert( nSettingsSize <= sizeof( tmp ) );

				if ( nSettingsSize <= sizeof( tmp ) )
				{
					// convert from binary to keyvalues
					CUtlBuffer buf;
					msgIn.ReadBytes( tmp, nSettingsSize );
					buf.Put( tmp, nSettingsSize );
					pKV->ReadAsBinary( buf );

					if ( sv_reservation_tickrate_adjustment.GetInt() > 0 )
					{
						float fDesiredTickInterval = 1.0f / sv_reservation_tickrate_adjustment.GetInt();
						// quantize tick intervals to the nearest N / 512 fraction
						float fNumerator = floorf(fDesiredTickInterval * 512.0f + 0.5f);
						host_state.interval_per_tick = fNumerator / 512.0f;
						host_state.interval_per_tick = clamp( host_state.interval_per_tick, MINIMUM_TICK_INTERVAL, MAXIMUM_TICK_INTERVAL );
						DevMsg( "Reservation tickrate adjustment to %.0f ticks %.6f ms\n", floorf( 1.0f/host_state.interval_per_tick ), host_state.interval_per_tick );
						if ( host_state.interval_per_tick < MINIMUM_TICK_INTERVAL ||
							host_state.interval_per_tick > MAXIMUM_TICK_INTERVAL )
						{
							Sys_Error( "GetTickInterval returned bogus tick interval (%f)[%f to %f is valid range]", host_state.interval_per_tick,
								MINIMUM_TICK_INTERVAL, MAXIMUM_TICK_INTERVAL );
						}

						extern ConVar sv_maxupdaterate;
						extern ConVar sv_maxcmdrate;
						sv_maxupdaterate.SetValue(1.0f / host_state.interval_per_tick);
						sv_maxcmdrate.SetValue(1.0f / host_state.interval_per_tick);
					}

					// let the game know the settings
					serverGameDLL->ApplyGameSettings( pKV );
					// adjust the game slots
					m_numGameSlots = pKV->GetInt( "members/numSlots", 0 );
		#ifdef PORTAL2	// HACK: PORTAL2 uses maxclients instead of GAMERULES
					SetMaxClients( m_numGameSlots );
		#endif
				}
			}
		}
	}

	m_ReservationStatus.m_bActive = true;
	m_ReservationStatus.m_bSuccess = bAccepted;
	m_ReservationStatus.m_Remote = adr;

	// Send the status right away
	SendReservationStatus( bAccepted ? kEReservationStatusPending : kEReservationStatusRejected );
}

void CBaseServer::ReplyReservationCheckRequest( const ns_address &adr, bf_read &msgIn )
{
	if ( msgIn.ReadLong() != GetHostVersion() )
		return;

	uint32 token = msgIn.ReadLong();
	uint32 uiReservationStage = msgIn.ReadLong();
	
	uint64	nReservationCookie= msgIn.ReadLongLong();
	bool reservationMatch = ( nReservationCookie == GetReservationCookie() );
	
	uint64 uiClientSteamID = msgIn.ReadLongLong();
	uint8 uiAwaitingClients = 0x7F; // large positive int8 number
	if ( uiReservationStage && reservationMatch && uiClientSteamID && sv_mmqueue_reservation.GetString()[0] == 'Q' )
	{
		DevMsg( "Reservation from client %u: %u\n", uint32( uiClientSteamID ), uiReservationStage );
		// For queue reservation client ID must match
		bool bThisClientShouldJoin = false, bThisClientJustConfirmed = false;
		uint8 uiActualAwaitingClients = 0;
		for ( int k = 0; k < m_arrReservationPlayers.Count(); ++ k )
		{
			QueueMatchPlayer_t &qmp = m_arrReservationPlayers[k];
			if ( qmp.m_uiAccountID == CSteamID( uiClientSteamID ).GetAccountID() )
			{
				bThisClientShouldJoin = true;
				qmp.m_adr = adr;
				qmp.m_uiToken = token;
				if ( qmp.m_uiReservationStage < uiReservationStage )
					bThisClientJustConfirmed = true;
				qmp.m_uiReservationStage = uiReservationStage;
			}
			
			if ( qmp.m_uiReservationStage < uiReservationStage )
				uiActualAwaitingClients += 1;
		}

		// Reservation matches, report to this client how many more clients we are awaiting
		if ( bThisClientShouldJoin )
		{
			uiAwaitingClients = uiActualAwaitingClients;

			if ( bThisClientJustConfirmed && serverGameDLL )
			{
				// Report to the GC the new level of confirmations
				CUtlVector< uint32 > arrConfirmedAccounts;
				arrConfirmedAccounts.EnsureCapacity( m_arrReservationPlayers.Count() );
				uint32 uiMinReservationLevel = 0xFFFFu;
				for ( int jj = 0; jj < m_arrReservationPlayers.Count(); ++ jj )
				{
					if ( m_arrReservationPlayers[jj].m_uiReservationStage < uiMinReservationLevel )
						uiMinReservationLevel = m_arrReservationPlayers[jj].m_uiReservationStage;
				}
				for ( int jj = 0; jj < m_arrReservationPlayers.Count(); ++ jj )
				{
					if ( ( m_arrReservationPlayers[jj].m_uiReservationStage >= 2 ) ||
						( m_arrReservationPlayers[jj].m_uiReservationStage > uiMinReservationLevel ) )
						arrConfirmedAccounts.AddToTail( m_arrReservationPlayers[jj].m_uiAccountID );
				}
				DevMsg( "Match start status: %u/%u\n", uiMinReservationLevel, arrConfirmedAccounts.Count() );
				serverGameDLL->ReportGCQueuedMatchStart( uiMinReservationLevel, arrConfirmedAccounts.Base(), arrConfirmedAccounts.Count() );

				if ( !uiActualAwaitingClients )
				{
					// Extend the reservation a little bit more since the match is now confirmed to be starting
					m_flReservationExpiryTime = net_time + sv_mmqueue_reservation_extended_timeout.GetFloat();
					DevMsg( "Reservation time extended +%d sec\n", sv_mmqueue_reservation_extended_timeout.GetInt() );
				}
			}
		}
	}

	if ( uiReservationStage && reservationMatch && uiClientSteamID && sv_mmqueue_reservation.GetString()[ 0 ] == 'G' )
	{
		DevMsg( "Reservation from in-progress client %u: %u\n", uint32( uiClientSteamID ), uiReservationStage );

		// Reservation matches, report to this client how many more clients we are awaiting
		uiAwaitingClients = 0;

		// Extend the reservation a little bit more since the match is now confirmed to be starting
		m_flReservationExpiryTime = net_time + sv_mmqueue_reservation_extended_timeout.GetFloat();
		DevMsg( "Reservation time extended +%d sec\n", sv_mmqueue_reservation_extended_timeout.GetInt() );
	}

	//
	// Send response to this client
	//
	char	buffer[64];
	bf_write msg(buffer,sizeof(buffer));

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( S2A_RESERVE_CHECK_RESPONSE );
	msg.WriteLong( GetHostVersion() );
	msg.WriteLong( token );
	msg.WriteLong( uiReservationStage );
	msg.WriteByte( uiAwaitingClients );
	
	NET_SendPacket( NULL, m_Socket, adr, msg.GetData(), msg.GetNumBytesWritten() );

	//
	// Send response to all clients too when this packet is really important
	//
	if ( !uiAwaitingClients )
	{
		// Make sure we send this notification to all clients to force them to connect
		// to our server as soon as everybody confirmed
		for ( int jj = 0; jj < m_arrReservationPlayers.Count(); ++ jj )
		{
			char	buffer[64];
			bf_write msg(buffer,sizeof(buffer));

			msg.WriteLong( CONNECTIONLESS_HEADER );
			msg.WriteByte( S2A_RESERVE_CHECK_RESPONSE );
			msg.WriteLong( GetHostVersion() );
			msg.WriteLong( m_arrReservationPlayers[jj].m_uiToken );
			msg.WriteLong( uiReservationStage );
			msg.WriteByte( uiAwaitingClients );

			NET_SendPacket( NULL, m_Socket, m_arrReservationPlayers[jj].m_adr, msg.GetData(), msg.GetNumBytesWritten() );
		}
	}
}

void CBaseServer::ClearReservationStatus()
{
	m_ReservationStatus.m_bActive = false;
}

void CBaseServer::FlagForSteamIDReuseAfterShutdown()
{
	m_flFlagForSteamIDReuseAfterShutdownTime = Plat_FloatTime();
}

void CBaseServer::SendReservationStatus( EReservationStatus_t kEReservationStatus )
{
	if ( !m_ReservationStatus.m_bActive  )
	{
		return;
	}

	char	buffer[32];
	bf_write msg(buffer,sizeof(buffer));

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( S2A_RESERVE_RESPONSE );
	msg.WriteLong( GetHostVersion() );
	msg.WriteByte( kEReservationStatus );
	msg.WriteOneBit( ( serverGameDLL && serverGameDLL->IsValveDS() ) ? 1 : 0 ); // whether we are running ValveDS
	msg.WriteLong( m_numGameSlots ); // number of game slots actually set on the server

	DevMsg( "Reservation response to %s: %u, server reserved for %d more seconds\n", 
		ns_address_render( m_ReservationStatus.m_Remote ).String(), kEReservationStatus, (int) ( m_flReservationExpiryTime - net_time ) );

	NET_SendPacket( NULL, m_Socket, m_ReservationStatus.m_Remote, msg.GetData(), msg.GetNumBytesWritten() );

	if ( kEReservationStatus != kEReservationStatusPending )
		ClearReservationStatus();
}

const char *CBaseServer::GetName( void ) const
{
	return host_name.GetString();
}

bool UseCDKeyAuth()
{
#ifdef NO_STEAM
	return true;
#endif

	// if we are physically on a 360 (360 listen server) or we are on a PC dedicated server for 360 clients, don't require Steam auth
	if ( IsX360() || NET_IsDedicatedForXbox() )
		return true;

	// for single player games that don't use Steam features, don't require Steam auth
	if ( !serverGameDLL->ShouldPreferSteamAuth() && Host_IsSinglePlayerGame() )
		return true;

#ifndef DEDICATED
	// for listen servers, if the Steam interface is not available (Steam not running), OK to not require Steam auth
	if ( !sv.IsDedicated() && !Steam3Client().SteamUser() )
		return true;
#endif

	// require Steam auth
	return false;
}

int CBaseServer::GetChallengeType( const ns_address &adr)
{	
	if ( UseCDKeyAuth() )
	{
		return PROTOCOL_HASHEDCDKEY;
	}
	else
	{
		return PROTOCOL_STEAM;
	}
}

int CBaseServer::GetChallengeNr ( const ns_address &adr)
{
	int		oldest = 0;
	float	oldestTime = FLT_MAX;
		
	// see if we already have a challenge for this ip
	for (int i = 0 ; i < m_ServerQueryChallenges.Count() ; i++)
	{
		if ( adr.CompareAdr (m_ServerQueryChallenges[i].adr, true) )
		{
			// reuse challenge number, but update time
			m_ServerQueryChallenges[i].time = net_time;
			return m_ServerQueryChallenges[i].challenge;
		}
		
		if (m_ServerQueryChallenges[i].time < oldestTime)
		{
			// remember oldest challenge
			oldestTime = m_ServerQueryChallenges[i].time;
			oldest = i;
		}
	}

	if ( m_ServerQueryChallenges.Count() > MAX_CHALLENGES )
	{
		m_ServerQueryChallenges.FastRemove( oldest );	
	}

	int newEntry = m_ServerQueryChallenges.AddToTail();
	// note the 0x0FFF of the top 16 bits, so that -1 will never be sent as a challenge
	m_ServerQueryChallenges[newEntry].challenge = (RandomInt(0,0x0FFF) << 16) | RandomInt(0,0xFFFF);
	m_ServerQueryChallenges[newEntry].adr = adr;
	m_ServerQueryChallenges[newEntry].time = net_time;
	return m_ServerQueryChallenges[newEntry].challenge;
}

void CBaseServer::GetNetStats( float &avgIn, float &avgOut )
{
	avgIn = avgOut = 0.0f;

	for (int i = 0; i < m_Clients.Count(); i++ )
	{
		CBaseClient	*cl = m_Clients[ i ];

		// Fake clients get killed in here.
		if ( cl->IsFakeClient() )
			continue;
	
		if ( !cl->IsConnected() )
			continue;

		INetChannel *netchan = cl->GetNetChannel();

		avgIn += netchan->GetAvgData(FLOW_INCOMING);
		avgOut += netchan->GetAvgData(FLOW_OUTGOING);
	}
}

void CBaseServer::CalculateCPUUsage( void )
{
	if ( !sv_stats.GetBool() )
	{
		return;
	}

	if(m_fStartTime==0)
	// record when we started
	{
		m_fStartTime=Sys_FloatTime();
	}

	if( Sys_FloatTime () > m_fLastCPUCheckTime+1)
	// only do this every 1 second
	{
#if defined ( _WIN32 ) 
		static float lastAvg=0;
		static __int64 lastTotalTime=0,lastNow=0;

		HANDLE handle;
		FILETIME creationTime, exitTime, kernelTime, userTime, nowTime;
 		__int64 totalTime,now;
			
		handle = GetCurrentProcess ();

		// get CPU time
		GetProcessTimes (handle, &creationTime, &exitTime,
						&kernelTime, &userTime);
		GetSystemTimeAsFileTime(&nowTime);

		if(lastNow==0)
		{
			memcpy(&lastNow,&creationTime,sizeof(__int64));;
		}


		memcpy(&totalTime,&userTime,sizeof(__int64));;
		memcpy(&now,&kernelTime,sizeof(__int64));;
		totalTime+=now;

		memcpy(&now,&nowTime,sizeof(__int64));;


		m_fCPUPercent = (double)(totalTime-lastTotalTime)/(double)(now-lastNow);
		
		// now save this away for next time
		if(Sys_FloatTime () > lastAvg+5) 
		// only do it every 5 seconds, so we keep a moving average
		{
			memcpy(&lastNow,&nowTime,sizeof(__int64));
			memcpy(&lastTotalTime,&totalTime,sizeof(__int64));
			lastAvg=m_fLastCPUCheckTime;
		}
#elif defined ( _PS3 )
		// FAKE
		m_fCPUPercent = 0.1;
#elif defined ( LINUX )
		// FAKE
		m_fCPUPercent = 0.1;
#elif defined ( POSIX )
	/*
		// linux CPU % code here :)
		static int32 lastrunticks,lastcputicks;
		static float lastAvg=0;

		struct sysinfo infos; 
		int32 dummy;
		int length;
		char statFile[PATH_MAX];
		int32 now = time(NULL);
		int32 ctime,stime,start_time;
		FILE *pFile;
		int32 runticks,cputicks;

		snprintf(statFile,PATH_MAX,"/proc/%i/stat",getpid());
		
		// we can't use FS_Open() cause its outside our dir
		pFile = fopen(statFile, "r");
		if ( pFile == NULL )
        	{
			goto end;
        	}
	        sysinfo(&infos);

		fscanf(pFile,
			"%d %s %c %d %d %d %d %d %lu %lu \
			%lu %lu %lu %ld %ld %ld %ld %ld %ld %lu \
			%lu %ld %lu %lu %lu %lu %lu %lu %lu %lu \
			%lu %lu %lu %lu %lu %lu",
			&dummy,statFile,&dummy,&dummy,&dummy,&dummy,
			&dummy,&dummy,&dummy,&dummy, // end of first line
			&dummy,&dummy,&dummy,&ctime,&stime,
			&dummy,&dummy,&dummy,&dummy,&dummy, // end of second
			&start_time,&dummy,&dummy,&dummy,&dummy,
			&dummy,&dummy,&dummy,&dummy,&dummy, // end of third
			&dummy,&dummy,&dummy,&dummy,&dummy,&dummy);
		fclose(pFile);					

		runticks = infos.uptime*HZ -start_time; // time the process has been running
		cputicks = stime+ctime;
			
		if(lastcputicks==0)
		{
			lastcputicks=cputicks;
		}

		if(lastrunticks==0)
		{
			lastrunticks=runticks;
		}
		else
		{
			m_fCPUPercent = (float)(cputicks-lastcputicks)/(float)(runticks-lastrunticks);
		}	

		*/
		//ConMsg("%f %li %li %li %li\n",cpuPercent,
		//	cputicks,(cputicks-lastcputicks),
		//	(runticks-lastrunticks),runticks);

		static struct rusage s_lastUsage;
		static float lastAvg = 0;
		struct rusage currentUsage;

		if ( getrusage( RUSAGE_SELF, &currentUsage ) == 0 )
		{
			double flTimeDiff = (double)( currentUsage.ru_utime.tv_sec - s_lastUsage.ru_utime.tv_sec ) + (double)(( currentUsage.ru_utime.tv_usec - s_lastUsage.ru_utime.tv_usec )/1000000); 
			m_fCPUPercent = flTimeDiff/(m_fLastCPUCheckTime - lastAvg);

			// now save this away for next time
			if( m_fLastCPUCheckTime > lastAvg+5) 
			{
				s_lastUsage = currentUsage;
				lastAvg = m_fLastCPUCheckTime;
			}
		}
		
		// limit checking :)
		if( m_fCPUPercent > 0.999 )
			m_fCPUPercent = 0.999;
		if( m_fCPUPercent < 0 )
			m_fCPUPercent = 0;
		
end:

#else
#error
#endif
		m_fLastCPUCheckTime=Sys_FloatTime(); 
	}
}


//-----------------------------------------------------------------------------
// Purpose: Prepare for level transition, etc.
//-----------------------------------------------------------------------------
void CBaseServer::InactivateClients( void )
{
	for (int i = 0; i < m_Clients.Count(); i++ )
	{
		CBaseClient	*cl = m_Clients[ i ];

		// Fake clients get killed in here (but split screen users don't)
		if ( cl->IsFakeClient() && !cl->IsSplitScreenUser() && !cl->IsHLTV()
#if defined( REPLAY_ENABLED )
			&& !cl->IsReplay()
#endif
			)
		{
			// If we don't do this, it'll have a bunch of extra steam IDs for unauthenticated users.
			Steam3Server().NotifyClientDisconnect( cl );
			cl->Clear();
			continue;
		}
		else if ( !cl->IsConnected() )
		{
			continue;
		}

		cl->Inactivate();
	}
}

void CBaseServer::ReconnectClients( void )
{
	for (int i=0 ; i< m_Clients.Count() ; i++ )
	{
		CBaseClient *cl = m_Clients[i];
		
		if ( cl->IsConnected() )
		{
			cl->SetSignonState( SIGNONSTATE_CONNECTED );
			CNETMsg_SignonState_t signon( cl->GetSignonState(), -1 );
			cl->FillSignOnFullServerInfo( signon );
			cl->SendNetMsg( signon );
		}
	}
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client in sv_timeout.GetFloat()
seconds, drop the conneciton.

When a client is normally dropped, the CSVClient goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
void CBaseServer::CheckTimeouts (void)
{
	int i;
	
	for ( i = 0 ; i < m_Clients.Count() ; ++i )
	{
		IClient	*cl = m_Clients[ i ];
		
		if ( cl->IsFakeClient() || !cl->IsConnected() )
			continue;

		INetChannel *netchan = cl->GetNetChannel();
		if ( !netchan )
			continue;
	
		// Don't timeout in _DEBUG builds
#if !defined( _DEBUG )
		if ( netchan->IsTimedOut() )
		{
			if ( this == &sv )
			{
				if ( CGameClient *pGameClient = dynamic_cast< CGameClient * >( cl ) )
				{
					serverGameDLL->OnEngineClientNetworkEvent( pGameClient->edict, pGameClient->m_SteamID.ConvertToUint64(), INetChannelInfo::k_ENetworkEventType_TimedOut, NULL );
				}
			}
			cl->Disconnect( CFmtStr( "%s timed out", cl->GetClientName() ) );
		}
		else
#endif
		// Handle steam socket disconnection
		if ( netchan->IsRemoteDisconnected() )
		{
			cl->Disconnect( CFmtStr( "%s disconnected", cl->GetClientName() ) );
		}
		else if ( netchan->IsOverflowed() )
		{
			cl->Disconnect( CFmtStr( "%s overflowed reliable channel", cl->GetClientName() ) );
		}
	}
}

// ==================
// check if clients update thier user setting (convars) and call 
// ==================
void CBaseServer::UpdateUserSettings(void)
{
	for (int i=0 ; i< m_Clients.Count() ; i++ )
	{
		CBaseClient	*cl = m_Clients[ i ];

		if ( cl->m_bConVarsChanged )
		{
			cl->UpdateUserSettings();
		}
	}
}

// ==================
// check if clients need the serverinfo packet sent
// ==================
void CBaseServer::SendPendingServerInfo()
{
	for (int i=0 ; i< m_Clients.Count() ; i++ )
	{
		CBaseClient	*cl = m_Clients[ i ];

		if ( cl->m_bSendServerInfo )
		{
			cl->SendServerInfo();
		}
	}
}

void CBaseServer::OnSteamServerLogonSuccess( uint32 externalIP )
{
	for (int i=0 ; i< m_Clients.Count() ; i++ )
	{
		CBaseClient	*cl = m_Clients[ i ];
		cl->OnSteamServerLogonSuccess( externalIP );
	}
}

/*
================
SV_CheckProtocol

Make sure connecting client is using proper protocol
================
*/
bool CBaseServer::CheckProtocol( const ns_address &adr, int nProtocol )
{
	if ( nProtocol != GetHostVersion() )
	{
		// Client is newer than server
		if ( nProtocol > GetHostVersion() )
		{
			RejectConnection( adr, "This server is using an older protocol ( %i ) than your client ( %i ).\n",
				GetHostVersion(), nProtocol  );
		}
		else
		// Server is newer than client
		{
			RejectConnection( adr, "This server is using a newer protocol ( %i ) than your client ( %i ).\n",
				GetHostVersion(), nProtocol );
		}
		return false;
	}

	// Success
	return true;
}

/*
================
SV_CheckKeyInfo

Determine if client is outside appropriate address range
================
*/
bool CBaseServer::CheckChallengeType( CBaseClient * client, int nNewUserID, const ns_address &adr, int nAuthProtocol, const char *pchLogonCookie, int cbCookie )
{
	// Check protocol ID
	if ( ( nAuthProtocol <= 0 ) || ( nAuthProtocol > PROTOCOL_LASTVALID ) )
	{
		RejectConnection( adr, "Invalid connection.\n");
		return false;
	}

	if ( ( nAuthProtocol == PROTOCOL_HASHEDCDKEY ) && (Q_strlen( pchLogonCookie ) <= 0 ||  Q_strlen(pchLogonCookie) != 32 ) )
	{
		RejectConnection( adr, "Invalid authentication certificate length.\n" );
		return false;
	}

	if ( IsDedicatedForXbox() )
	{
		return true;
	}

	if ( nAuthProtocol == PROTOCOL_STEAM )
	{
		// Dev hack to allow 360/Steam PC cross platform play
// 		int ip0 = 207;
// 		int ip1 = 173;
// 		int ip2 = 179;
// 		int ip3Min = 230;
// 		int ip3Max = 245;
// 
// 		if ( adr.ip[0] == ip0 &&
// 			adr.ip[1] == ip1 &&
// 			adr.ip[2] == ip2 &&
// 			adr.ip[3] >= ip3Min &&
// 			adr.ip[3] <= ip3Max )
// 		{
// 			return true;
// 		}

		client->SetSteamID( CSteamID() ); // set an invalid SteamID

		// Convert raw certificate back into data
		if ( cbCookie <= 0 || cbCookie >= STEAM_KEYSIZE )
		{
			RejectConnection( adr, "STEAM certificate length error! %i/%i\n", cbCookie, STEAM_KEYSIZE );
			return false;
		}
		ns_address checkAdr = adr;
		if ( adr.IsLoopback() || adr.IsLocalhost() )
		{
			checkAdr.AsType<netadr_t>().SetIP( net_local_adr.GetIPHostByteOrder() );
		}

		if ( !Steam3Server().NotifyClientConnect( client, nNewUserID, checkAdr, pchLogonCookie, cbCookie ) 
			&& !Steam3Server().BLanOnly() ) // the userID isn't alloc'd yet so we need to fill it in manually
		{
			RejectConnection( adr, "STEAM validation rejected\n" );
			return false;
		}
	}
	else
	{
		if ( !Steam3Server().NotifyLocalClientConnect( client ) ) // the userID isn't alloc'd yet so we need to fill it in manually
		{
			RejectConnection( adr, "GSCreateLocalUser failed\n" );
			return false;
		}
	}

	return true;
}

bool CBaseServer::CheckIPRestrictions( const ns_address &adr, int nAuthProtocol )
{
	// Determine if client is outside appropriate address range
	if ( adr.IsLoopback() )
		return true;

	// X360TBD: network
	if ( IsX360() )
		return true;

	// allow other users if they're on the same ip range
	if ( Steam3Server().BLanOnly() )
	{
		if ( !adr.IsType<netadr_t>() )
			return false;

		// allow connection, if client is in the same subnet 
		if ( adr.AsType<netadr_t>().CompareClassBAdr( net_local_adr ) )
			return true;

		// allow connection, if client has a private IP
		if ( adr.AsType<netadr_t>().IsReservedAdr() )
			return true;
		
		// reject connection
		return false;
	}

	return true;
}

void CBaseServer::SetMasterServerRulesDirty()
{
	m_bMasterServerRulesDirty = true;
}

bool CBaseServer::CheckPassword( const ns_address &adr, const char *password, const char *name )
{
	// If a server is reserved, then we ignore PW checks
	// if ( GetReservationCookie() != 0ull )
	//	return true;

	const char *server_password = GetPassword();

	if ( !server_password )
		return true;	// no password set

	if ( adr.IsLocalhost() || adr.IsLoopback() )
	{
		return true; // local client can always connect
	}

	int iServerPassLen = Q_strlen(server_password);

	if ( iServerPassLen != Q_strlen(password) )
	{
		return false; // different length cannot be equal
	}

	if ( Q_strncmp( password, server_password, iServerPassLen ) == 0)
	{
		return true; // passwords are equal
	}

	return false; // all test failed
}

float CBaseServer::GetTime() const
{
	return m_nTickCount * m_flTickInterval;
}

float CBaseServer::GetFinalTickTime() const
{
	return (m_nTickCount + (host_frameticks - host_currentframetick)) * m_flTickInterval;
}

void CBaseServer::DisconnectClient(IClient *client, const char *reason )
{
	client->Disconnect( reason );
}

void CBaseServer::ClearBaselineHandles( void )
{
	FOR_EACH_MAP_FAST( m_BaselineHandles, i )
	{
		g_pSerializedEntities->ReleaseSerializedEntity( m_BaselineHandles[ i ] );
	}
	m_BaselineHandles.Purge();
}


void CBaseServer::Clear( void )
{
	if ( m_StringTables )
	{
		m_StringTables->RemoveAllTables();
		m_StringTables = NULL;
	}

	m_pInstanceBaselineTable = NULL;
	m_pLightStyleTable = NULL;
	m_pUserInfoTable = NULL;
	m_pServerStartupTable = NULL;

	ClearBaselineHandles();
	
	m_State = ss_dead;
	
	m_nTickCount = 0;
	
	Q_memset( m_szMapname, 0, sizeof( m_szMapname ) );
	Q_memset( m_szBaseMapname, 0, sizeof( m_szBaseMapname ) );
	Q_memset( m_szMapGroupName, 0, sizeof( m_szMapGroupName ) );
	Q_memset( m_szSkyname, 0, sizeof( m_szSkyname ) );

	clientDllCRC = 0;
	worldmapCRC = 0;
	stringTableCRC = 0;

	MEM_ALLOC_CREDIT();

	// Use a different limit on the signon buffer, so we can save some memory in SP (for xbox).
	if ( IsMultiplayer() || IsDedicated() )
	{
		m_SignonBuffer.EnsureCapacity( NET_MAX_PAYLOAD );
	}
	else
	{
		m_SignonBuffer.EnsureCapacity( 16384 );
	}
	
	m_Signon.StartWriting( m_SignonBuffer.Base(), m_SignonBuffer.Count() );
	m_Signon.SetDebugName( "m_Signon" );

	serverclasses = 0;
	serverclassbits = 0;
}

/*
================
SV_RejectConnection

Rejects connection request and sends back a message
================
*/
void CBaseServer::RejectConnection( const ns_address &adr, const char *fmt, ... )
{
	va_list		argptr;
	char	text[1024];

	va_start (argptr, fmt);
	Q_vsnprintf ( text, sizeof( text ), fmt, argptr);
	va_end (argptr);

	NET_OutOfBandPrintf( m_Socket, adr, "%c%s", S2C_CONNREJECT, text );

	Warning( "RejectConnection: %s - %s\n", ns_address_render( adr ).String(), text );
}

void CBaseServer::SetPaused( bool paused )
{
	// pause request can be rejected, unpause needs to fall through
	if ( m_State != ss_paused && !IsPausable() )
	{
		return;
	}

	if ( !IsActive() )
		return;

	if ( paused )
	{
		m_State = ss_paused;
	}
	else
	{
		m_State = ss_active;
	}

	CSVCMsg_SetPause_t setpause;
	setpause.set_paused( paused );
	BroadcastMessage( setpause );
}

void CBaseServer::SetTimescale( float flTimescale )
{
	m_flTimescale = flTimescale;
}

//-----------------------------------------------------------------------------
// Purpose: General initialization of the server
//-----------------------------------------------------------------------------
void CBaseServer::Init( bool bIsDedicated )
{
	m_nMaxclients = 0;
	m_nSpawnCount = 0;
	m_nUserid = 1;
	m_bIsDedicated = bIsDedicated;
	m_bIsDedicatedForXbox = bIsDedicated && ( CommandLine()->FindParm( "-xlsp" ) != 0 );
	m_bIsDedicatedForPS3 = bIsDedicated && ( CommandLine()->FindParm( "-ps3ds" ) != 0 );

	m_Socket = NS_SERVER;	
	
	m_Signon.SetDebugName( "m_Signon" );

	if ( !g_pKVrulesConvars )
	{
		g_pKVrulesConvars = new KeyValues( "NotifyRulesCvars" );
		if ( !g_pKVrulesConvars->LoadFromFile( g_pFullFileSystem, "gamerulescvars.txt", "MOD" ) )
		{
			Warning( "Failed to load gamerulescvars.txt, game rules cvars might not be reported to management tools.\n" );
		}
	}
	
	g_pCVar->InstallGlobalChangeCallback( ServerNotifyVarChangeCallback );
	SetMasterServerRulesDirty();
	
	Clear();
}

INetworkStringTable *CBaseServer::GetInstanceBaselineTable( void )
{
	if ( m_pInstanceBaselineTable == NULL )
	{
		m_pInstanceBaselineTable = m_StringTables->FindTable( INSTANCE_BASELINE_TABLENAME );
	}

	return m_pInstanceBaselineTable;
}

INetworkStringTable *CBaseServer::GetLightStyleTable( void )
{
	if ( m_pLightStyleTable == NULL )
	{
		m_pLightStyleTable= m_StringTables->FindTable( LIGHT_STYLES_TABLENAME );
	}

	return m_pLightStyleTable;
}

INetworkStringTable *CBaseServer::GetUserInfoTable( void )
{
	if ( m_pUserInfoTable == NULL )
	{
		if ( m_StringTables == NULL )
		{
			return NULL;
		}
		m_pUserInfoTable = m_StringTables->FindTable( USER_INFO_TABLENAME );
	}

	return m_pUserInfoTable;
}

bool CBaseServer::GetClassBaseline( ServerClass *pClass, SerializedEntityHandle_t *pHandle )
{
	if ( sv_instancebaselines.GetInt() )
	{
		ErrorIfNot( pClass->m_InstanceBaselineIndex != INVALID_STRING_INDEX,
			("SV_GetInstanceBaseline: missing instance baseline for class '%s'", pClass->m_pNetworkName)
			);

		AUTO_LOCK_FM( g_svInstanceBaselineMutex );
		int nSlot = m_BaselineHandles.Find( pClass->m_InstanceBaselineIndex ); 
		ErrorIfNot( nSlot != m_BaselineHandles.InvalidIndex(), ( "CBaseServer::GetClassBaseline: for %s failed\n", pClass->GetName() ) );

		*pHandle = m_BaselineHandles[ nSlot ];
		Assert( *pHandle != SERIALIZED_ENTITY_HANDLE_INVALID );
		return *pHandle != SERIALIZED_ENTITY_HANDLE_INVALID;
	}

	*pHandle = SERIALIZED_ENTITY_HANDLE_INVALID;
	return true;
}

bool CBaseServer::ShouldUpdateMasterServer()
{
	// If the game server itself is ever running, then it's the one who gets to update the master server.
	// (SourceTV will not update it in this case).
	return true;
}


void CBaseServer::CheckMasterServerRequestRestart()
{
	if ( !( CommandLine()->FindParm( "-fake_stale_server" ) ) )
	{
		if ( !Steam3Server().SteamGameServer() || !Steam3Server().SteamGameServer()->WasRestartRequested() )
		return;
	}

	// Connection was rejected by the HLMaster (out of date version)

	// hack, vgui console looks for this string; 
	Msg("%cMasterRequestRestart\n", 3);

#ifndef _WIN32
	if (CommandLine()->FindParm(AUTO_RESTART))
	{
		Msg("Your server is out of date and will be shutdown during hibernation or changelevel, whichever comes first.\n");
		Log_Msg( LOG_SERVER_LOG, "Your server is out of date and will be shutdown during hibernation or changelevel, whichever comes first.\n");

		SetRestartOnLevelChange( true );

		Cbuf_AddText(CBUF_SERVER, "sv_shutdown\n");
		Cbuf_Execute();
	}
#endif
#ifdef _WIN32
	if (g_pFileSystem->IsSteam())
#else
	else if ( 1 ) // under linux assume steam
#endif
	{
		Msg("Your server needs to be restarted in order to receive the latest update.\n");
		Log_Msg( LOG_SERVER_LOG, "Your server needs to be restarted in order to receive the latest update.\n");
	}
	else
	{
		Msg("Your server is out of date.  Please update and restart.\n");
	}

	if ( serverGameDLL && serverGameDLL->IsValveDS() && !CommandLine()->FindParm( "-noautoupdate" ) )
	{	// Valve DS always posts sv_shutdown when out of date
		Cbuf_AddText( CBUF_SERVER, "sv_shutdown\n" );
		Cbuf_Execute();
	}
}

void CBaseServer::UpdateMasterServer()
{
	if ( !ShouldUpdateMasterServer() )
		return;

	if ( !Steam3Server().SteamGameServer() )
		return;
	
	// Call this every frame
	ForwardPacketsFromMasterServerUpdater();
		
	// Only update every so often.
	double flCurTime = Plat_FloatTime();
	if ( flCurTime - m_flLastMasterServerUpdateTime < MASTER_SERVER_UPDATE_INTERVAL )
		return;

	m_flLastMasterServerUpdateTime = flCurTime;

	// If we are not in legacy mode then make sure server tags still get updated every so often. 
	// In legacy mode this is done by heartbeat code
	UpdateGameData();

	CheckMasterServerRequestRestart();
	
	if ( NET_IsDedicated() && sv_region.GetInt() == -1 )
    {
		sv_region.SetValue( 255 ); // HACK!HACK! undo me once we want to enforce regions

        //Log_Printf( "You must set sv_region in your server.cfg or use +sv_region on the command line\n" );
		//Con_Printf( "You must set sv_region in your server.cfg or use +sv_region on the command line\n" );
        //Cbuf_AddText( "quit\n" );
        //return;
    }

	static bool bUpdateMasterServers = !CommandLine()->FindParm( "-nomaster" );
	if ( bUpdateMasterServers )
	{
		bool bActive = IsActive() && IsMultiplayer() && g_bEnableMasterServerUpdater && !IsSinglePlayerGame();
		if ( ShouldHideFromMasterServer() )
			bActive = false;

		g_bSteamMasterHeartbeatsEnabled = bActive;
		Steam3Server().SteamGameServer()->EnableHeartbeats( bActive );

		if ( bActive )
		{
			UpdateMasterServerRules();
			UpdateMasterServerPlayers();
			Steam3Server().SendUpdatedServerDetails();
		}
	}
	
	if ( serverGameDLL && Steam3Server().GetGSSteamID().IsValid() )
		serverGameDLL->UpdateGCInformation();
}

void CBaseServer::UpdateMasterServerRules()
{
	// Only do this if the rules vars are dirty.
	if ( !m_bMasterServerRulesDirty )
		return;

	ISteamGameServer *pGameServer = Steam3Server().SteamGameServer();
	if ( !pGameServer )
		return;
		
	pGameServer->ClearAllKeyValues();
	
	// Need to respond with game directory, game name, and any server variables that have been set that
	// affect rules.  Also, probably need a hook into the .dll to respond with additional rule information.
	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();

		if ( !(var->IsFlagSet( FCVAR_NOTIFY ) ) )
			continue;

		ConVar *pConVar = dynamic_cast< ConVar* >( var );
		if ( !pConVar )
			continue;

		if ( !g_pKVrulesConvars->GetBool( pConVar->GetName() ) )
			continue;

		SetMasterServerKeyValue( pGameServer, pConVar );
	}

	if (  Steam3Server().SteamGameServer() )
	{
		RecalculateTags();
	}

	// Ok.. it's all updated, only send incremental updates now until we decide they're all dirty.
	m_bMasterServerRulesDirty = false;
}


void CBaseServer::ForwardPacketsFromMasterServerUpdater()
{
	ISteamGameServer *p = Steam3Server().SteamGameServer();
	if ( !p )
		return;
	
	while ( 1 )
	{
		uint32 netadrAddress;
		uint16 netadrPort;
		unsigned char packetData[16 * 1024];
 		int len = p->GetNextOutgoingPacket( packetData, sizeof( packetData ), &netadrAddress, &netadrPort );
		if ( len <= 0 )
			break;
		
		// Send this packet for them..
		netadr_t adr( netadrAddress, netadrPort );
		NET_SendPacket( NULL, m_Socket, adr, packetData, len );
	}
}


/*
=================
SV_ReadPackets

Read's packets from clients and executes messages as appropriate.
=================
*/

#define PARENT_PROCESS_UPDATE_INTERVAL ( 10.0 )				// every 10 seconds

void UpdateParentProcess( void )
{
#ifdef _LINUX
	static double s_flNextParentProcessUpdate = -1;
	float flNow = Sys_FloatTime();
	if ( flNow >= s_flNextParentProcessUpdate )
	{
		s_flNextParentProcessUpdate = flNow + PARENT_PROCESS_UPDATE_INTERVAL;
		char sbuf[2048];
		sprintf( sbuf, "status map=%s;players=%d", sv.GetMapName(), sv.GetNumClients() );
		SendStringToParentProcess( sbuf );
	}
#endif

}


void CBaseServer::RunFrame( void )
{
	VPROF_BUDGET( "CBaseServer::RunFrame", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	NET_ProcessSocket( m_Socket, this );	

	CheckTimeouts();	// drop clients that timeed out

	UpdateUserSettings(); // update client settings 
	
	SendPendingServerInfo(); // send outstanding signon packets after ALL user settings have been updated

	CalculateCPUUsage(); // update CPU usage

	UpdateMasterServer();

	if ( IsChildProcess() )
	{
		UpdateParentProcess();								// sned status to the parent process occasionally
	}

	if ( m_bMasterServerRulesDirty )
	{
		m_bMasterServerRulesDirty = false;
		RecalculateTags();
	}

	ProcessSplitScreenDisconnects();
}


void CBaseServer::ProcessVoice( void )
{
	//This call will improve voice transmission with a slowed host_timescale. But will also send data to the clients too frequently depending on network throttling.
	//It can cause prediction errors when not throttled. There is no throttling for listen server loopback connections. Thusly, listen server hosts have prediction errors.
	//SendClientMessages( false );
}


CBaseClient * CBaseServer::GetFreeClient( const ns_address &adr )
{
	CBaseClient *client = GetFreeClientInternal( adr );
	return client;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *adr - 
//			*pslot - 
//			**ppClient - 
// Output : int
//-----------------------------------------------------------------------------
CBaseClient * CBaseServer::GetFreeClientInternal( const ns_address &adr )
{
	CBaseClient *freeclient = NULL;
	
	for ( int slot = 0 ; slot < m_Clients.Count() ; slot++ )
	{
		CBaseClient *client = m_Clients[slot];

		if ( client->IsFakeClient() )
			continue;

		if ( client->IsConnected() )
		{
			if ( adr.CompareAdr ( client->m_NetChannel->GetRemoteAddress() ) )
			{
				ConMsg ( "%s:reconnect\n", ns_address_render( adr ).String() );

				for ( int k = host_state.max_splitscreen_players; k -- > 1; )
				{
					// Processing split screen users in reverse order

					CBaseClient *& pSsUser = client->m_SplitScreenUsers[ k ];
					if ( !pSsUser )
						continue;

					RemoveClientFromGame( pSsUser );

					// perform a silent netchannel shutdown, don't send disconnect msg
					pSsUser->m_NetChannel->Shutdown( NULL );
					pSsUser->m_NetChannel = NULL;

					pSsUser->Clear();

					// Mark the split screen slot as empty
					pSsUser = NULL;
				}

				RemoveClientFromGame( client );

				// perform a silent netchannel shutdown, don't send disconnect msg
				client->m_NetChannel->Shutdown( NULL );
				client->m_NetChannel = NULL;
		
				client->Clear();
				return client;
			}
		}
		else
		{
			// use first found free slot 
			if ( !freeclient )
			{
				freeclient = client; 
			}
		}
	}

	if ( !freeclient )
	{
		int count = m_Clients.Count();

		if ( count >= m_nMaxclients )
		{
			return NULL; // server full
		}

		// we have to create a new client slot
		freeclient = CreateNewClient( count );
		
		m_Clients.AddToTail( freeclient );
	}
	
	// Success
	return freeclient;
}

void CBaseServer::SendClientMessages ( bool bSendSnapshots )
{
	VPROF_BUDGET( "SendClientMessages", VPROF_BUDGETGROUP_OTHER_NETWORKING );
    SNPROF("SendClientMessages");
	
	for (int i=0; i< m_Clients.Count(); i++ )
	{
		CBaseClient* client = m_Clients[i];
		
		// Update Host client send state...
		if ( !client->ShouldSendMessages() )
			continue;

		// Connected, but inactive, just send reliable, sequenced info.
		if ( client->m_NetChannel )
		{
			client->m_NetChannel->Transmit();
			client->UpdateSendState();
		}
		else
		{
			Msg("Client has no netchannel.\n");
		}
	}
}

CBaseClient *CBaseServer::CreateFakeClient(const char *name)
{
	ns_address adr; // it's an empty address
	adr.Clear(); // sets NA_NULL

	CBaseClient *fakeclient = GetFreeClient( adr );
				
	if ( !fakeclient )
	{
		// server is full
		return NULL;		
	}

	INetChannel *netchan = NULL;
	if ( sv_stressbots.GetBool() )
	{
		netchan = NET_CreateNetChannel( m_Socket, &adr, ns_address_render( adr ).String(), fakeclient, NULL, true );
	}

	// a NULL netchannel signals a fakeclient
	m_nUserid = GetNextUserID();
	fakeclient->Connect( name, m_nUserid, netchan, true, CROSSPLAYPLATFORM_THISPLATFORM );

	// fake some cvar settings
	//fakeclient->SetUserCVar( "name", name ); // set already by Connect()
	fakeclient->SetUserCVar( "rate", CFmtStr( "%d", DEFAULT_RATE ) );
	fakeclient->SetUserCVar( "cl_updaterate", CFmtStr( "%d", (int)( 1.0f / host_state.interval_per_tick ) ) );
	fakeclient->SetUserCVar( "cl_cmdrate", CFmtStr( "%d", (int)( 1.0f / host_state.interval_per_tick ) ) );
	fakeclient->SetUserCVar( "cl_interp_ratio", "1.0" );
	fakeclient->SetUserCVar( "cl_interp", "0.1" );
	fakeclient->SetUserCVar( "cl_interpolate", "0" );
	fakeclient->SetUserCVar( "cl_predict", "1" );
	fakeclient->SetUserCVar( "cl_predictweapons", "1" );
	fakeclient->SetUserCVar( "cl_lagcompensation", "1" );
	fakeclient->SetUserCVar( "closecaption","0" );
	fakeclient->SetUserCVar( "english", "1" );
	fakeclient->SetUserCVar( "cl_clanid", "0" );
	// fakeclient->SetUserCVar( "cl_team", "blue" );
	fakeclient->SetUserCVar( "hud_classautokill", "1" );
	fakeclient->SetUserCVar( "tf_medigun_autoheal", "0" );
	fakeclient->SetUserCVar( "cl_autorezoom", "1" );
	fakeclient->SetUserCVar( "fov_desired", "75" );

	// create client in game.dll
	fakeclient->ActivatePlayer();

	fakeclient->m_nSignonTick = m_nTickCount;
			
	return fakeclient;
}

void CBaseServer::Shutdown( void )
{
	g_pCVar->RemoveGlobalChangeCallback( ServerNotifyVarChangeCallback );

	if ( !IsActive() )
		return;

	m_State = ss_dead;

	CUtlVector< CBaseClient * > vecDelete;

	// Let reliable messages go out
	for ( int retry = 0; retry < 3; ++ retry )
	{
		for ( int i = 0; i < m_Clients.Count(); ++ i )
		{
			CBaseClient *cl = m_Clients[i];
			if ( INetChannel *pChannel = cl->GetNetChannel() )
			{
				pChannel->Transmit();
			}
		}
		Sys_Sleep( 10 );
	}

	// Only drop clients if we have not cleared out entity data prior to this.
	for(  int i=m_Clients.Count()-1; i>=0; i-- )
	{
		CBaseClient * cl = m_Clients[ i ];
		if ( cl->IsConnected() )
		{
			// Hack, but this forces instant cleanup
			if ( cl->IsSplitScreenUser() )
			{
				cl->m_bSplitScreenUser = false;
			}

			cl->Disconnect( "Server shutting down" );
		}
		else
		{
			// free any memory do this out side here in case the reason the server is shutting down 
			//  is because the listen server client typed disconnect, in which case we won't call
			//  cl->DropClient, but the client might have some frame snapshot references left over, etc.
			cl->Clear();	
		}

		vecDelete.AddToTail( cl );

		m_Clients.Remove( i );
	}

	// Let drop messages go out
	Sys_Sleep( 100 );

#if !defined( _X360 ) && !defined( NO_STEAM )
	if ( !IsHLTV() )
	{
		if ( m_flFlagForSteamIDReuseAfterShutdownTime && ( Plat_FloatTime() - m_flFlagForSteamIDReuseAfterShutdownTime < 1.0 ) )
			;// Server was flagged for shutdown and SteamID reuse, don't LogOff
		else
			Steam3Server().DeactivateAndLogoff();

		// Reset the shutdown flag
		m_flFlagForSteamIDReuseAfterShutdownTime = 0;
	}
#endif

	// Let drop messages go out
	Sys_Sleep( 100 );

	for ( int i = 0; i < vecDelete.Count(); ++i )
	{
		delete vecDelete[ i ];
	}

	// clear everthing
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Sends text to all active clients
// Input  : *fmt -
//			... -
//-----------------------------------------------------------------------------
void CBaseServer::BroadcastPrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	Q_vsnprintf (string, sizeof( string ), fmt,argptr);
	va_end (argptr);

	CSVCMsg_Print_t print;
	print.set_text( string );
	BroadcastMessage( print );	
}

void CBaseServer::BroadcastMessage( INetMessage &msg, bool onlyActive, bool reliable )
{
	for ( int i = 0; i < m_Clients.Count(); i++ )
	{
		CBaseClient *cl = m_Clients[ i ];

		if ( (onlyActive && !cl->IsActive()) || !cl->IsSpawned() )
		{
			continue;
		}

		if ( !cl->SendNetMsg( msg, reliable ) )
		{
			if ( msg.IsReliable() || reliable )
			{
				DevMsg( "BroadcastMessage: Reliable broadcast message overflow for client %s", cl->GetClientName() );
			}
		}
	}
}

void CBaseServer::BroadcastMessage( INetMessage &msg, IRecipientFilter &filter )
{
	if ( filter.IsInitMessage() )
	{
		// This really only applies to the first player to connect, but that works in single player well enought
		if ( IsActive() )
		{
			ConDMsg( "SV_BroadcastMessage: Init message being created after signon buffer has been transmitted\n" );
		}
		
		if ( !msg.WriteToBuffer( m_Signon ) )
		{
			Sys_Error( "SV_BroadcastMessage: Init message would overflow signon buffer!\n" );
			return;
		}
	}
	else
	{
		msg.SetReliable( filter.IsReliable() );

		int num = filter.GetRecipientCount();
	
		for ( int i = 0; i < num; i++ )
		{
			int index = filter.GetRecipientIndex( i );

			if ( index < 1 || index > m_Clients.Count() )
			{
				Msg( "SV_BroadcastMessage:  Recipient Filter for message type %i (reliable: %s, init: %s) with bogus client index (%i) in list of %i clients\n", 
						msg.GetType(), 
						filter.IsReliable() ? "yes" : "no",
						filter.IsInitMessage() ? "yes" : "no",
						index, num );

				if ( msg.IsReliable() )
					Host_Error( "Reliable message (type %i) discarded.", msg.GetType() );

				continue;
			}

			CBaseClient *cl = m_Clients[ index - 1 ];

			if ( !cl->IsSpawned() )
			{
				continue;
			}

			if ( !cl->SendNetMsg( msg ) )
			{
				if ( msg.IsReliable() )
				{
					DevMsg( "BroadcastMessage: Reliable filter message overflow for client %s\n", cl->GetClientName() );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Writes events to the client's network buffer
// Input  : *cl -
//			*pack -
//			*msg -
//-----------------------------------------------------------------------------
static ConVar sv_debugtempentities( "sv_debugtempentities", "0", 0, "Show temp entity bandwidth usage." );

// 8 KB should be far more than is needed -- in fact less than 100 bytes seems to be sufficient
const int kTempEntityBufferSize = 8192;

void CBaseServer::WriteTempEntities( CBaseClient *client, CFrameSnapshot *pCurrentSnapshot, CFrameSnapshot *pLastSnapshot, 
	CSVCMsg_TempEntities_t &msg, int ev_max )
{
	msg.Clear();

	// allocate the temp buffer for the temp ents
	char ALIGN4 tempEntityData[kTempEntityBufferSize] ALIGN4_POST;
	bf_write buffer( &tempEntityData[0], ARRAYSIZE(tempEntityData) );

	bool bDebug = sv_debugtempentities.GetBool();

	// Container which calls ReleaseReference on all snapshots in list on exit of function scope
	CReferencedSnapshotList snapshotlist;
	// Builds list and calls AddReference on each item in list (uses a mutex to be thread safe)
	framesnapshotmanager->BuildSnapshotList( pCurrentSnapshot, pLastSnapshot, CFrameSnapshotManager::knDefaultSnapshotSet, snapshotlist );

	//keep count of the number of entities that we write out (since some can be omitted by the client)
	int32 nNumEntitiesWritten = 0;
	CEventInfo *pLastEvent = NULL;

	// Build list of events sorted by send table classID (makes the delta work better in cases with a lot of the same message type )
	for ( int nSnapShotIndex = 0; 
		 nSnapShotIndex < snapshotlist.m_vecSnapshots.Count(); 
		 ++nSnapShotIndex )
	{
		CFrameSnapshot *pSnapshot = snapshotlist.m_vecSnapshots[ nSnapShotIndex ];

		for( int i = 0; i < pSnapshot->m_nTempEntities; ++i )
		{
			CEventInfo *event = pSnapshot->m_pTempEntities[ i ];

			if ( client->IgnoreTempEntity( event ) )
				continue; // event is not seen by this player
			
			//we are writing this entity so update our count so the receiver knows how many to parse
			nNumEntitiesWritten++;	

			if ( event->fire_delay == 0.0f )
			{
				buffer.WriteOneBit( 0 );
			} 
			else
			{
				buffer.WriteOneBit( 1 );
				buffer.WriteSBitLong( event->fire_delay*100.0f, 8 );
			}

			if ( pLastEvent && 
				pLastEvent->classID == event->classID )
			{
				buffer.WriteOneBit( 0 ); // delta against last temp entity

				int startBit = bDebug ? buffer.GetNumBitsWritten() : 0;

				SendTable_WriteAllDeltaProps( event->pSendTable, pLastEvent->m_Packed, event->m_Packed, -1, &buffer );

				if ( bDebug )
				{
					int length = buffer.GetNumBitsWritten() - startBit;
					DevMsg("TE %s delta bits: %i\n", event->pSendTable->GetName(), length );
				}
			}
			else
			{
				 // full update, just compressed against zeros in MP

				buffer.WriteOneBit( 1 );

				int startBit = bDebug ? buffer.GetNumBitsWritten() : 0;

				buffer.WriteUBitLong( event->classID, GetClassBits() );

				// Will write all non-zero fields
				SendTable_WriteAllDeltaProps( event->pSendTable, SERIALIZED_ENTITY_HANDLE_INVALID, event->m_Packed, -1, &buffer );

				if ( bDebug )
				{
					int length = buffer.GetNumBitsWritten() - startBit;
					DevMsg("TE %s full bits: %i\n", event->pSendTable->GetName(), length );
				}
			}

			if ( IsMultiplayer() )
			{
				// in single player, don't used delta compression, lastEvent remains NULL
				pLastEvent = event;
			}
		}
	}

	//don't do any more work if we didn't write anything out
	if( nNumEntitiesWritten <= 0 )
		return;

	if ( buffer.IsOverflowed() )
	{
		Warning( "WriteTempOverflow! Discarding all ents!\n" );
		return;
	}

	// Copy the data to the message buffer. This copying is unfortunate but at least
	// it has a cost that is proportional to the message size. The alternative is to
	// initially set the buffer size larger and then resize it down, but the initial
	// setting of the buffer size will require zeroing all of its bytes which is
	// more expensive (as seen on CS:GO server profiles on Linux).
	const int nBytesWritten = Bits2Bytes( buffer.GetNumBitsWritten() );
	msg.set_entity_data( &tempEntityData[0], nBytesWritten );

	// set num entries
	msg.set_num_entries( nNumEntitiesWritten );
}

void CBaseServer::SetMaxClients( int number )
{
	m_nMaxclients = clamp( number, 1, ABSOLUTE_PLAYER_LIMIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseServer::RecalculateTags( void )
{
	if ( IsHLTV() || IsReplay() )
		return;

	// We're going to modify the sv_tags convar here, which will cause this to be called again. Prevent recursion.
	static bool bRecalculatingTags = false;
	if ( bRecalculatingTags )
		return;

	bRecalculatingTags = true;

	// Games without this interface will have no tagged cvars besides "increased_maxplayers"
	if ( serverGameTags )
	{
		KeyValues *pKV = new KeyValues( "GameTags" );

		serverGameTags->GetTaggedConVarList( pKV );

		KeyValues *p = pKV->GetFirstSubKey();
		while ( p )
		{
			ConVar *pConVar = g_pCVar->FindVar( p->GetString("convar") );
			if ( pConVar )
			{
				const char *pszDef = pConVar->GetDefault();
				const char *pszCur = pConVar->GetString();
				if ( Q_strcmp( pszDef, pszCur ) )
				{
					AddTag( p->GetString("tag") );
				}
				else
				{
					RemoveTag( p->GetString("tag") );
				}
			}

			p = p->GetNextKey();
		}

		pKV->deleteThis();
	}

	// Check maxplayers
	int minmaxplayers = 1;
	int maxmaxplayers = ABSOLUTE_PLAYER_LIMIT;
	int defaultmaxplayers = 1;
	serverGameClients->GetPlayerLimits( minmaxplayers, maxmaxplayers, defaultmaxplayers );

	int nHumans;
	int nMaxHumans;
	int nBots;

	GetMasterServerPlayerCounts( nHumans, nMaxHumans, nBots );

	if ( nMaxHumans > maxmaxplayers )
	{
		AddTag( "increased_maxplayers" );
	}
	else
	{
		RemoveTag( "increased_maxplayers" );
	}

	if ( g_bLowViolence )
	{
		AddTag( "low_violence" );
	}
	else
	{
		RemoveTag( "low_violence" );
	}

	// Group name
	const char *pszGroupName = sv_steamgroup.GetString();
	if ( !pszGroupName || !pszGroupName[0] )
	{
		RemoveTag( "*grp:", true );
	}
	else
	{
		char chGroupNameBuf[64] = {0};
		Q_snprintf( chGroupNameBuf, sizeof( chGroupNameBuf ) - 2, "%si", pszGroupName );
		AddTag( "*grp:", chGroupNameBuf );
	}

	bRecalculatingTags = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseServer::AddTag( const char *pszTag, const char *pszSubTagValue )
{
	CSplitString TagList( sv_tags.GetString(), "," );
	for ( int i = 0; i < TagList.Count(); i++ )
	{
		if ( pszSubTagValue )
		{
			// See if the subtag matches
			if ( StringHasPrefix( TagList[i], pszTag ) )
			{
				// Ok, see if the tag value matches
				if ( Q_stricmp(TagList[i]+strlen(pszTag),pszSubTagValue) == 0 )
					return;

				// They have a subtag specified, but it's wrong. Remove it.
				RemoveTag( pszTag, true );
			}
		}
		else
		{
			// Already in the tag list?
			if ( !Q_stricmp(TagList[i],pszTag) )
				return;
		}
	}

	// Append it
	char tmptags[MAX_TAG_STRING_LENGTH];
	tmptags[0] = '\0';
	Q_strncpy( tmptags, pszTag, MAX_TAG_STRING_LENGTH );
	if ( pszSubTagValue )
	{
		Q_strncat( tmptags, pszSubTagValue, MAX_TAG_STRING_LENGTH );
	}
	Q_strncat( tmptags, ",", MAX_TAG_STRING_LENGTH );
	Q_strncat( tmptags, sv_tags.GetString(), MAX_TAG_STRING_LENGTH );
	sv_tags.SetValue( tmptags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseServer::RemoveTag( const char *pszTag, bool bSubTag )
{
	const char *pszTags = sv_tags.GetString();
	if ( !pszTags || !pszTags[0] )
		return;

	char tmptags[MAX_TAG_STRING_LENGTH];
	tmptags[0] = '\0';

	CSplitString TagList ( sv_tags.GetString(), "," );
	bool bFoundIt = false;
	for ( int i = 0; i < TagList.Count(); i++ )
	{
		bool bMatched = false;
		if ( bSubTag )
		{
			bMatched = StringHasPrefix( TagList[i], pszTag );
		}
		else
		{
			bMatched = Q_stricmp(TagList[i],pszTag) == 0;
		}

		// Keep any tags other than the specified one
		if ( !bMatched )
		{
			Q_strncat( tmptags, TagList[i], MAX_TAG_STRING_LENGTH );
			Q_strncat( tmptags, ",", MAX_TAG_STRING_LENGTH );
		}
		else
		{
			bFoundIt = true;
		}
	}

	// Didn't find it in our list?
	if ( !bFoundIt )
		return;

	sv_tags.SetValue( tmptags );
}

CBaseClient *CBaseServer::CreateSplitClient( const CMsg_CVars& vecUserInfo, CBaseClient *pAttachedTo )
{
	// 0.0.0.0:0 signifies a split client. It'll plumb all the way down to winsock calls but it won't make them.
	ns_address adr;
	adr.Clear();
	CBaseClient *pSplitClient = GetFreeClient( adr );
	if ( !pSplitClient )
	{
		// server is full
		return NULL;		
	}

	INetChannel *netchan = NULL;
	// [ NET ENCRYPT ] Split clients don't configure an encryption key because the master channel will network
	netchan = NET_CreateNetChannel( m_Socket, &adr, ns_address_render( adr ).String(), pSplitClient, NULL, true );

	m_nUserid = GetNextUserID();

	// Name will be pulled from vecUserInfo!!!
	pSplitClient->Connect( "split", m_nUserid, netchan, true, pAttachedTo->m_ClientPlatform, &vecUserInfo );
	
	Assert( pSplitClient->m_bFakePlayer == true );
	pSplitClient->m_bSplitScreenUser = true;
	pSplitClient->m_pAttachedTo = pAttachedTo;
	
	pSplitClient->m_nSignonTick = m_nTickCount;

	pSplitClient->m_bSplitAllowFastDisconnect = true;

	if ( !pSplitClient->CheckConnect() )
	{
		pSplitClient->m_bSplitAllowFastDisconnect = false;
		return NULL;
	}

	pSplitClient->m_bSplitAllowFastDisconnect = false;

	return pSplitClient;
}

CBaseClient *CBaseServer::GetBaseUserForSplitClient( CBaseClient *pSplitUser )
{
	if ( pSplitUser->m_pAttachedTo )
		return pSplitUser->m_pAttachedTo;

	return pSplitUser;
}

void CBaseServer::QueueSplitScreenDisconnect( CBaseClient *pSplitHost, CBaseClient *pSplitUser )
{
	SplitDisconnect_t disc;
	disc.m_pUser = pSplitHost;
	disc.m_pSplit = pSplitUser;

	m_QueuedForDisconnect.AddToTail( disc );
}

void CBaseServer::ProcessSplitScreenDisconnects()
{
	// Destroy it
	for ( int i = 0; i < m_QueuedForDisconnect.Count(); ++i )
	{
		SplitDisconnect_t &disc = m_QueuedForDisconnect[ i ];
		CBaseClient *pClient = disc.m_pUser;
		for ( int j = 0; j < host_state.max_splitscreen_players; ++j )
		{
			if ( pClient->m_SplitScreenUsers[ j ] != disc.m_pSplit )
				continue;

			pClient->m_SplitScreenUsers[ j ] = NULL;
			disc.m_pSplit->m_bSplitAllowFastDisconnect = true;
			disc.m_pSplit->Disconnect( "leaving splitscreen" );
			disc.m_pSplit->m_bSplitScreenUser = false;
		}
	}
	m_QueuedForDisconnect.Purge();
}

// Exposing state of the server to the client code
ConVar sv_hosting_lobby( "sv_hosting_lobby", "0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );

void CBaseServer::UpdateReservedState()
{
	if ( ( m_flReservationExpiryTime && !IsReserved() ) ||
		 ( m_flReservationExpiryTime < net_time ) )
	{
		m_flReservationExpiryTime = 0.0f;	
		sv.UpdateHibernationState();
	}
	
	if ( m_flTimeLastClientLeft != -1.0f )
	{
		sv.UpdateHibernationState();
	}
}

uint64 CBaseServer::GetReservationCookie() const
{
	return m_nReservationCookie;
}

bool CBaseServer::ReserveServerForQueuedGame( char const *szReservationPayload )
{
	if ( !szReservationPayload )
		return false;
	switch ( szReservationPayload[0] )
	{
	case 'Q':	// Queued competitive, locked game from the start
	case 'G':	// Joinable in progress game
		break;
	
	case 'R':
		sscanf( szReservationPayload + 1, "%p", &m_pnReservationCookieSession );
		return true;

	default:
		return false;
	}

	extern bool sv_ShutDown_WasRequested();
	if ( sv_ShutDown_WasRequested() )
	{
		Warning( "Rejecting reservation because sv_shutdown was requested.\n" );
		return false; // don't accept new reservations if we are in sv_shutdown mode
	}

	uint64 uiReservationCookie = 0;
	uint64 uiMatchID = 0;
	int32 bReserve = 0;
	sscanf( szReservationPayload+1, "%llx,%llx,%d:", &uiReservationCookie, &uiMatchID, &bReserve );
	if ( !uiReservationCookie || !uiMatchID )
		return false;

	m_nMatchId = uiMatchID;

	if ( bReserve )
	{
		if ( !IsReserved() || ( GetReservationCookie() == uiReservationCookie ) )
		{
			m_flReservationExpiryTime = net_time + sv_mmqueue_reservation_timeout.GetFloat();
			sv_mmqueue_reservation.SetValue( szReservationPayload );
			SetReservationCookie( uiReservationCookie, "ReserveServerForQueuedGame: %s", szReservationPayload );
			return true;
		}
		else
			return false;
	}
	else
	{
		if ( IsReserved() && ( GetReservationCookie() == uiReservationCookie ) )
		{
			Unreserve();
			return true;
		}
		else
			return !IsReserved();
	}
}

void CBaseServer::SetReservationCookie( uint64 uiCookie, char const *pchReasonFormat, ... )
{
	if ( uiCookie != m_nReservationCookie )
	{
		char reason[ 256 ] = { 0 };
		va_list argptr;
		va_start( argptr, pchReasonFormat );
		Q_vsnprintf( reason, sizeof( reason ), pchReasonFormat, argptr );
		va_end( argptr );
		ConColorMsg( Color( 255, 0, 255, 255 ), "-> Reservation cookie %llx:  reason %s\n", uiCookie, reason );

		if ( !uiCookie )
		{
			sv_mmqueue_reservation.SetValue( "" );
			m_arrReservationPlayers.RemoveAll();
		}
		else if ( StringHasPrefix( sv_mmqueue_reservation.GetString(), CFmtStr( "Q%llx,", uiCookie ) ) )
		{
			// Set reservation players
			m_arrReservationPlayers.RemoveAll();
			for ( char const *pszPrev = sv_mmqueue_reservation.GetString(), *pszNext = pszPrev;
				( pszNext = strchr( pszPrev, '[' ) ) != NULL; pszPrev = pszNext + 1 )
			{
				uint32 uiAccountId = 0;
				sscanf( pszNext, "[%x]", &uiAccountId );
				if ( uiAccountId )
				{
					QueueMatchPlayer_t qmp;
					qmp.m_uiAccountID = uiAccountId;
					qmp.m_uiToken = 0;
					qmp.m_uiReservationStage = 0;
					m_arrReservationPlayers.AddToTail( qmp );
				}
			}

			// Set the number of expected humans
			m_numGameSlots = m_arrReservationPlayers.Count();

			// Tournament servers need additional slots for casters
			static char const * s_pchTournamentServer = CommandLine()->ParmValue( "-tournament", ( char const * ) NULL );
			if ( s_pchTournamentServer )
			{
				int numCasters = 0;
				for ( char const *pszPrev = sv_mmqueue_reservation.GetString(), *pszNext = pszPrev;
					( pszNext = strchr( pszPrev, '{' ) ) != NULL; pszPrev = pszNext + 1 )
				{
					uint32 uiAccountId = 0;
					sscanf( pszNext, "{%x}", &uiAccountId );
					if ( uiAccountId )
						++ numCasters;
				}

				// Allow launch parameter to limit number of caster slots
				static int s_nTournamentExtraCastersSlots = CommandLine()->ParmValue( "-tournament_extra_casters_slots", 2 );
				numCasters = MAX( 0, MIN( s_nTournamentExtraCastersSlots, numCasters ) );
				m_numGameSlots += numCasters;
			}
		}
		else if ( StringHasPrefix( sv_mmqueue_reservation.GetString(), CFmtStr( "G%llx,", uiCookie ) ) )
		{
			// Set reservation players
			m_arrReservationPlayers.RemoveAll();

			// Set the number of expected humans
			m_numGameSlots = 0;
		}
		else
		{
			// Set exposed reservation variable
			sv_mmqueue_reservation.SetValue( CFmtStr( "0x%llx", uiCookie ) );
		}
	}

	m_nReservationCookie = uiCookie;

	UpdateGameData();

	// Expose the current reservation state via a replicated convar to clients
	sv_hosting_lobby.SetValue( IsReserved() );
}

void CBaseServer::Unreserve()
{
	if ( IsReserved() )
	{
		Msg( "Server was reserved for %d more seconds.  Reservation cleared.\n", (int) ( m_flReservationExpiryTime - net_time ) );
		m_flReservationExpiryTime = 0.0f;	
		sv.UpdateHibernationState();
	}
	else
	{
		Msg( "Server is not currently reserved.\n" );
	}

	UpdateGameData();
}

char const *CBaseServer::GetGameType() const
{
	return m_GameType.String();
}

char const *CBaseServer::GetGameData() const
{
	return m_GameData.Base();
}

int CBaseServer::GetGameDataVersion() const
{
	return m_GameDataVersion;
}

void CBaseServer::ClearTagStrings()
{
	m_GameType = "";
}

static void BuildTokenList( char const *pchString, char chDelim, CUtlVector< CUtlString > &list )
{
	// Have to split up the string
	int len = Q_strlen( pchString );
	char *szCritBuf = (char *)stackalloc( len + 1 );
	Q_strncpy( szCritBuf, pchString, len + 1 );
	char *pszTok = strchr( szCritBuf, chDelim );
	char *pszPrevTok = szCritBuf;

	while ( pszTok && pszPrevTok )
	{
		// Save character
		char szTemp = *pszTok; 
		*pszTok = 0;
		if ( *pszPrevTok )
		{
			list.AddToTail( CUtlString( pszPrevTok ) );
		}
		// Restore and advance to next token after delim
		*pszTok = szTemp;
		pszPrevTok = pszTok + 1;
		pszTok = strchr( pszPrevTok, chDelim );
	}

	// Close off any trailing token w/o a delim at the end
	if ( pszPrevTok && *pszPrevTok )
	{
		list.AddToTail( CUtlString( pszPrevTok ) );
	}
}

void CBaseServer::AddTagString( CUtlString &dest, char const *pchString )
{
	if ( !pchString || !*pchString )
		return;

	char *chDelim = ",";

	if ( Q_strstr( pchString, chDelim ) )
	{
		CUtlVector< CUtlString > list;
		BuildTokenList( pchString, *chDelim, list );
		for ( int i = 0; i < list.Count(); ++i )
		{
			AddTagString( dest, list[ i ].String() );
		}
		return;
	}

	if ( !dest.IsEmpty() )
	{
		dest += chDelim;
	}
	dest += pchString;
}

void CBaseServer::UpdateGameType()
{
	ClearTagStrings();

	CUtlString tags;

	if ( serverGameDLL )
	{
		char szMatchMakingTags[ 1024 ] = { 0 };
		serverGameDLL->GetMatchmakingTags( szMatchMakingTags, sizeof( szMatchMakingTags ) );
		if ( szMatchMakingTags[ 0 ] )
		{
			AddTagString( m_GameType, szMatchMakingTags );
		}
	}

	bool bHaveAnyClients = GetNumHumanPlayers() > 0;
	if ( !bHaveAnyClients )
	{
		AddTagString( m_GameType, "empty" );
	}

	static ConVarRef var( "sv_tags" );
	if ( var.IsValid() && var.GetString()[ 0 ] )
	{
		AddTagString( m_GameType, var.GetString() );
	}

	// Is this server "secure"?
#if !defined( NO_STEAM ) && !defined( _GAMECONSOLE )
	{
		AddTagString( m_GameType, Steam3Server().BSecure() ? "secure" : "insecure" );
	}
#endif

// 	if ( IsDedicated() &&  serverGameDLL->IsValveDS() && !IsX360() && !IsDedicatedForXbox() &&
// 		 !bHaveAnyClients &&
// 		 !IsReserved() && !( GetNumClients() - GetNumFakeClients() ) &&
// 		 !sv_steamgroup_exclusive.GetBool() )
// 	{
// 		AddTagString( va( "*sv_search_key_%s%d", sv_search_key.GetString(), GetHostVersion() ) );
// 	}

	if ( Steam3Server().SteamGameServer() )
	{
		Steam3Server().SteamGameServer()->SetGameTags( m_GameType.String() );
	}
}

void CBaseServer::UpdateGameData()
{
	UpdateGameType();

	const int nPacketSize = MAX_ROUTABLE_PAYLOAD - 128;	// The packet has to fit into NET_SendPacket and there's some data sent before the tags too

	// Remember old data
	CUtlVector< char > oldGameData;
	oldGameData.EnsureCapacity( m_GameData.Count() );
	oldGameData.AddMultipleToTail( m_GameData.Count(), m_GameData.Base() );

	//
	// Generate new data
	m_GameData.RemoveAll();
	m_GameData.EnsureCapacity( nPacketSize );
	m_GameData.AddMultipleToTail( nPacketSize );
	memset( m_GameData.Base(), 0, m_GameData.Count() );

	// Add search key
	CUtlString utlKey;
	if ( serverGameDLL && IsDedicated() && !IsX360() && !IsDedicatedForXbox() &&
	 		( GetNumHumanPlayers() <= 0 ) &&
	 		!IsReserved() && !( GetNumClients() - GetNumFakeClients() ) &&
	 		!sv_steamgroup_exclusive.GetBool() && !GetPassword() )
	{
		AddTagString( utlKey, CFmtStr( "%skey:%s%d",
			serverGameDLL->IsValveDS() ? "v" : "c",
			sv_search_key.GetString(), GetHostVersion() ) );
	}

	if ( utlKey.Length() > m_GameData.Count() - 3 )
	{
		Warning( "GameData: sv_search_key too long, cannot advertise server!\n" );
		return;
	}

	// Group name
	CUtlString utlGroups;
	const char *pszGroupName = sv_steamgroup.GetString();
	{
		CUtlVector< CUtlString > list;
		char *chDelim = ",";
		BuildTokenList( pszGroupName, *chDelim, list );

		for ( int i = 0; i < list.Count(); ++i )
		{
			AddTagString( utlGroups, CFmtStr( "grp:%si", list[ i ].String() ) );
		}
	}

	if ( utlKey.Length() + utlGroups.Length() > m_GameData.Count() - 3 )
	{
		Warning( "GameData: Too many Steam groups set for sv_steamgroup, not advertising Steam groups affiliation.\n" );
		utlGroups.Purge();
	}

	if ( serverGameDLL )
	{
		serverGameDLL->GetMatchmakingGameData( m_GameData.Base(), m_GameData.Count() - 1 - utlKey.Length() - utlGroups.Length() - 2 );
	}

	int nLen = Q_strlen( m_GameData.Base() );
	char *pszWrite = m_GameData.Base() + nLen;
	int numBytes = m_GameData.Count() - nLen;

	Q_snprintf( pszWrite, numBytes - 1, "%s%s%s%s",
		nLen ? "," : "",
		utlGroups.Get(), utlGroups.Length() ? "," : "",
		utlKey.Get() );

	// Always update Steam
	if ( Steam3Server().SteamGameServer() )
	{
		Steam3Server().SteamGameServer()->SetGameData( m_GameData.Base() );
	}

	// Increment data version if changed
	if ( oldGameData.Count() != m_GameData.Count() ||
		memcmp( oldGameData.Base(), m_GameData.Base(), m_GameData.Count() ) )
	{
		++ m_GameDataVersion;
	}
}

bool CBaseServer::IsPlayingSoloAgainstBots() const
{
	if ( sv.IsActive() && sv.IsMultiplayer() )
	{
		int nNumHumanPlayers = sv.GetNumClients() - sv.GetNumFakeClients();
		if ( nNumHumanPlayers == 1 )
		{
			return true;
		}
	}
	return false;
}

bool CBaseServer::IsExclusiveToLobbyConnections() const
{
#ifndef NO_STEAM
	if ( !IsDedicated() )
		return false;

#if !defined( CSTRIKE15 )
	// We are switching CStrike to always have lobbies associated with servers for community matchmaking
	if ( !sv_allow_lobby_connect_only.GetBool() )
		return false;
#endif

	if ( sv_lan.GetBool() )
		return false;
	return true;
#else
	return false;
#endif
}

// CON_COMMAND( sv_unreserve, "Clears any lobby reservation for this server\n" )
// {
// 	sv.Unreserve();
// }

bool CBaseServer::ShouldHideServer() const
{
	if ( serverGameDLL && 
		 serverGameDLL->ShouldHideServer() )
	{
		return true;
	}

	return false;
}

// If a server is hidden from master server it won't show up on public internet (server browser, steam server list)
//  but it can still respond to LAN info, players, rules queries etc.
bool CBaseServer::ShouldHideFromMasterServer() const
{
	// UNDONE: MATCHMAKING: Left4Dead keeps passworded, listen and cheat servers off the master server.  TF2 does not.
	extern ConVar sv_cheats;
	if ( !IsDedicated() && (!IsHLTV()) )
	{
		return true;
	}

	return ShouldHideServer();
}

void CBaseServer::OnPasswordChanged()
{
	if ( IsActive() )
	{
		Cbuf_AddText( CBUF_SERVER, "heartbeat\n" );
	}
}

int CBaseServer::GetMaxClients() const
{
	return m_nMaxclients;
}

int CBaseServer::GetMaxHumanPlayers() const
{
	if ( serverGameClients )
	{
		int nMaxHuman = serverGameClients->GetMaxHumanPlayers();
		if ( nMaxHuman != -1 )
		{
			return nMaxHuman;
		}
	}

	return GetMaxClients();
}

int CBaseServer::GetNumHumanPlayers( void ) const
{
	int count	= 0;

	for (int i=0 ; i < m_Clients.Count() ; i++ )
	{
		if ( m_Clients[ i ]->IsHumanPlayer() )
		{
			count++;
		}
	}

	return count;
}

void CBaseServer::GetMasterServerPlayerCounts( int &nHumans, int &nMaxHumanSlots, int &nBots )
{
	// count active users
	nHumans = GetNumHumanPlayers();
	nMaxHumanSlots = GetMaxHumanPlayers();
	nBots = sv.GetNumFakeClients();

	if ( sv_visiblemaxplayers.GetInt() > 0 )
	{
		nMaxHumanSlots = sv_visiblemaxplayers.GetInt();
	}

	extern bool CanShowHostTvStatus();
	if ( !CanShowHostTvStatus() && ( nBots > 0 ) )
	{
		for ( CActiveHltvServerIterator hltv; hltv; hltv.Next() )
		{
			nBots--; // reduce the bot count by HLTV bot
		}
	}
}

void CBaseServer::ShowTags() const
{
	Msg( "Tags:\n" );
	Msg( "Public :  %s\n", GetGameType() );
	Msg( "Private:  %s\n", GetGameData() );
}

CON_COMMAND( sv_showtags, "Describe current gametags." )
{
	sv.ShowTags();
}


const ConVar &GetIndexedConVar( const ConVar &cv, int nIndex )
{
	if ( nIndex == 0 )
		return cv;
	const ConVar *pIndexedCV = g_pCVar->FindVar( va( "%s%d", cv.GetBaseName(), nIndex ) );
	if ( pIndexedCV )
		return *pIndexedCV;
	else
		return cv;
}
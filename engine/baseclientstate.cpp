//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:  baseclientstate.cpp: implementation of the CBaseClientState class.
// 
//===========================================================================//

#include "client_pch.h"
#include "baseclientstate.h"
#include "inetchannel.h"
#include "netmessages.h"
#include "proto_oob.h"
#include "dt_recv_eng.h"
#include "host_cmd.h"
#include "GameEventManager.h"
#include "cl_rcon.h"
#ifndef DEDICATED
#include "cl_pluginhelpers.h"
#include "vgui_askconnectpanel.h"
#include "cdll_engine_int.h"
#endif
#include "sv_steamauth.h"
#include "snd_audio_source.h"
#include "server.h"
#include "cl_steamauth.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#include "replayhistorymanager.h"
#endif
#include "filesystem/IQueuedLoader.h"
#include "serializedentity.h"
#include "checksum_engine.h"

#include "matchmaking/imatchframework.h"
#include "mathlib/IceKey.H"
#include "hltvserver.h"
#include "UtlStringMap.h"

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#endif

#include "vgui/ILocalize.h"
#include "eiface.h"
#include "cl_broadcast.h"

#include "csgo_limits.h"
#include "csgo_limits.inl"

#if defined( _PS3 )
		#include <sysutil/sysutil_userinfo.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_teammate_color_1( "cl_teammate_color_1", "240 243 32" );
ConVar cl_teammate_color_2( "cl_teammate_color_2", "150 34 223" );
ConVar cl_teammate_color_3( "cl_teammate_color_3", "0 165 90" );
ConVar cl_teammate_color_4( "cl_teammate_color_4", "92 168 255" );
ConVar cl_teammate_color_5( "cl_teammate_color_5", "255 155 37" );

#if defined( INCLUDE_SCALEFORM )
const char* g_szDefaultScaleformClientMovieName = "resource/flash/GameUIRootMovie.swf";
#endif

#ifdef ENABLE_RPT
void CL_NotifyRPTOfDisconnect( );
#endif // ENABLE_RPT

#if ( !defined( NO_STEAM ) && (!defined( DEDICATED ) ) )
void UpdateNameFromSteamID( IConVar *pConVar, CSteamID *pSteamID )
{
#if !defined( DEDICATED )   
	if ( !pConVar || !pSteamID || !Steam3Client().SteamFriends() )
		return;

#if defined( _PS3 )
	CSteamID sPsnId = Steam3Client().SteamUser()->GetConsoleSteamID();
	if ( sPsnId.IsValid() )
	{
		const char *pszName = Steam3Client().SteamFriends()->GetFriendPersonaName( sPsnId );
		pConVar->SetValue( pszName );
	}
#else

	Assert( pSteamID->GetAccountID() != 0 || CommandLine()->FindParm( "-ignoreSteamAsserts" ) );
	const char *pszName = Steam3Client().SteamFriends()->GetFriendPersonaName( *pSteamID );
	pConVar->SetValue( pszName );

#endif  // _PS3
#endif
}

void SetNameToSteamIDName( IConVar *pConVar )
{
#if !defined( DEDICATED )
	if ( Steam3Client().SteamUtils() && Steam3Client().SteamFriends() && Steam3Client().SteamUser() )
	{
		CSteamID steamID = Steam3Client().SteamUser()->GetSteamID();
		UpdateNameFromSteamID( pConVar, &steamID );
	}
#endif
}
#endif

void CL_NameCvarChanged( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	CSplitScreenAddedConVar *pCheck = dynamic_cast< CSplitScreenAddedConVar * >( pConVar );
	if ( pCheck )
		return;

#ifndef DEDICATED
#if !defined( NO_STEAM )
	static bool bPreventRent = false;
	if ( !bPreventRent )
	{
		bPreventRent = true;
		SetNameToSteamIDName( pConVar );
		bPreventRent = false;
	}
#endif
#endif

	ConVarRef var( pConVar );

	// store off the last known name, that isn't default, in the registry
	// this is a transition step so it can be used to display in friends
	if ( 0 != Q_stricmp( var.GetString(), var.GetDefault()  ) 
		&& 0 != Q_stricmp( var.GetString(), "player" ) )
	{
	    Sys_SetRegKeyValue( "Software\\Valve\\Steam", "LastGameNameUsed", (char *)var.GetString() );
	}
}



#ifndef DEDICATED
void askconnect_accept_f()
{
	char szHostName[256];
	if ( IsAskConnectPanelActive( szHostName, sizeof( szHostName ) ) )
	{
		char szCommand[512];
		V_snprintf( szCommand, sizeof( szCommand ), "connect %s", szHostName );
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), szCommand );
		HideAskConnectPanel();
	}
}
ConCommand askconnect_accept( "askconnect_accept", askconnect_accept_f, "Accept a redirect request by the server.", FCVAR_DONTRECORD );
#endif

#ifndef SWDS
extern IVEngineClient *engineClient;
// ---------------------------------------------------------------------------------------- //
static void SendClanTag( const char *pTag, const char *pName )
{
	KeyValues *kv = new KeyValues( "ClanTagChanged" );
	kv->SetString( "tag", pTag );
	kv->SetString( "name", pName );
	engineClient->ServerCmdKeyValues( kv );
}
#endif

// ---------------------------------------------------------------------------------------- //
void CL_ClanIdChanged( IConVar *pConVar, const char *pOldString, float flOldValue )
{
#ifndef SWDS
	// Get the clan ID we're trying to select
	ConVarRef var( pConVar );
	uint32 newId = var.GetInt();
	if ( newId == 0 )
	{
		// Default value, equates to no tag
		SendClanTag( "", "" );
		return;
	}

#if !defined( NO_STEAM )
	// Make sure this player is actually part of the desired clan
	ISteamFriends *pFriends = Steam3Client().SteamFriends();
	if ( pFriends )
	{
		int iGroupCount = pFriends->GetClanCount();
		for ( int k = 0; k < iGroupCount; ++ k )
		{
			CSteamID clanID = pFriends->GetClanByIndex( k );
			if ( clanID.GetAccountID() == newId )
			{
				CSteamID clanID( newId, Steam3Client().SteamUtils()->GetConnectedUniverse(), k_EAccountTypeClan );
				// valid clan, accept the change
				const char *szClanTag = pFriends->GetClanTag( clanID );
				char chLimitedTag[ MAX_CLAN_TAG_LENGTH ];
				CopyStringTruncatingMalformedUTF8Tail( chLimitedTag, szClanTag, MAX_CLAN_TAG_LENGTH );

				const char *szClanName = pFriends->GetClanName( clanID );
				SendClanTag( chLimitedTag, szClanName );
				return;
			}
		}
	}
#endif // NO_STEAM

	// Couldn't validate the ID, so clear to the default (no tag)
	var.SetValue( 0 );
#endif // !SWDS
}

ConVar	cl_resend	( "cl_resend", "2", FCVAR_RELEASE, "Delay in seconds before the client will resend the 'connect' attempt", true, CL_MIN_RESEND_TIME, true, CL_MAX_RESEND_TIME );
ConVar	cl_resend_timeout	( "cl_resend_timeout", "60", FCVAR_RELEASE, "Total time allowed for the client to resend the 'connect' attempt", true, CL_MIN_RESEND_TIME, true, 1000 * CL_MAX_RESEND_TIME );
ConVar	cl_name		( "name","unnamed", FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_SS | FCVAR_PRINTABLEONLY | FCVAR_SERVER_CAN_EXECUTE, "Current user name", CL_NameCvarChanged );
ConVar	password	( "password", "", FCVAR_ARCHIVE | FCVAR_SERVER_CANNOT_QUERY | FCVAR_DONTRECORD, "Current server access password" );
static ConVar cl_interpolate( "cl_interpolate", "1", FCVAR_RELEASE, "Enables or disables interpolation on listen servers or during demo playback" );
ConVar  cl_clanid( "cl_clanid", "0", FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_HIDDEN, "Current clan ID for name decoration", CL_ClanIdChanged );
ConVar  cl_color( "cl_color", "0", FCVAR_ARCHIVE | FCVAR_USERINFO, "Preferred teammate color", true, 0, true, 4 );
ConVar  cl_decryptdata_key( "cl_decryptdata_key", "", FCVAR_RELEASE, "Key to decrypt encrypted GOTV messages" );
ConVar  cl_decryptdata_key_pub( "cl_decryptdata_key_pub", "", FCVAR_RELEASE, "Key to decrypt public encrypted GOTV messages" );
ConVar	cl_hideserverip( "cl_hideserverip", "0", FCVAR_RELEASE, "If set to 1, server IPs will be hidden in the console (except when you type 'status')" );


#ifdef _X360
ConVar	cl_networkid_force ( "networkid_force", "", FCVAR_USERINFO | FCVAR_SS | FCVAR_PRINTABLEONLY | FCVAR_SERVER_CAN_EXECUTE | FCVAR_DEVELOPMENTONLY, "Forceful value for network id (e.g. XUID)" );
#endif

static ConVar cl_failremoteconnections( "cl_failremoteconnections", "0", FCVAR_DEVELOPMENTONLY, "Force connection attempts to time out" );

static uint32 GetPrivateIPDelayMsecs()
{
	// Lesser of 1/2 cl_resend interval or 1000 msecs
	float flSeconds = clamp( cl_resend.GetFloat() * 0.5f, 0.0f, 1.0f );

	return (uint32)( flSeconds * 1000.0f );
}

// ---------------------------------------------------------------------------------------- //
// C_ServerClassInfo implementation.
// ---------------------------------------------------------------------------------------- //

C_ServerClassInfo::C_ServerClassInfo()
{
	m_ClassName = NULL;
	m_DatatableName = NULL;
	m_InstanceBaselineIndex = INVALID_STRING_INDEX;
}

C_ServerClassInfo::~C_ServerClassInfo()
{
	delete [] m_ClassName;
	delete [] m_DatatableName;
}

// ---------------------------------------------------------------------------------------- //
// Server messaging helpers
// ---------------------------------------------------------------------------------------- //

CServerMsg::CServerMsg( CBaseClientState *pParent, IMatchAsyncOperationCallback *pCallback, 
	const ns_address& serverAdr, int socket, uint32 maxAttempts, double timeout ):
	m_pParent( pParent )
{
	m_eState = AOS_RUNNING;
	m_pCallback = pCallback;
	m_serverAdr = serverAdr;
	m_socket = socket;
	m_lastMsgSendTime = 0.0;
	m_timeOut = timeout;
	m_maxAttempts = maxAttempts;
	m_numAttempts = 0;
	m_result = 0;
}

void CServerMsg::Update()
{
	if ( m_eState != AOS_RUNNING )
	{
		return;
	}

	double dt = net_time - m_lastMsgSendTime;

	if ( dt < m_timeOut )
	{
		return;
	}

	if ( m_numAttempts >= m_maxAttempts )
	{
		// Failed to receive a reply
		m_eState = AOS_FAILED;
		m_pCallback->OnOperationFinished( this );
	}
	else
	{
		m_lastToken = RandomInt( INT_MIN, INT_MAX );
		SendMsg( m_serverAdr, m_socket, m_lastToken );
		m_lastMsgSendTime = net_time;
		m_numAttempts++;
	}
}

bool CServerMsg::IsValidResponse( const ns_address&  from, uint32 token )
{
	if ( GetState() != AOS_RUNNING )
	{
		// We are not expecting any responses
		return false;
	}

	if ( !from.CompareAdr(GetServerAddr()) )
	{
		// Not expecting a response from this address
		return false;
	}

	if ( token != GetLastToken() )
	{
		// This response is not for the last message sent
		return false;
	}

	return true;
}

void CServerMsg::ResponseReceived( uint64 result )
{
	if ( GetState() == AOS_RUNNING )
	{
		m_eState = AOS_SUCCEEDED;
		m_result = result;
		m_pCallback->OnOperationFinished( this );
	}
}

extern ConVar			sv_mmqueue_reservation_timeout;
extern ConVar			sv_mmqueue_reservation_extended_timeout;

CServerMsg_CheckReservation::CServerMsg_CheckReservation( CBaseClientState *pParent, IMatchAsyncOperationCallback *pCallback, 
	const ns_address &serverAdr, int socket, uint64 reservationCookie, uint32 uiReservationStage ) :
	CServerMsg( pParent, pCallback, serverAdr, socket, (uiReservationStage > 1) ? sv_mmqueue_reservation_extended_timeout.GetInt() : sv_mmqueue_reservation_timeout.GetInt(), 1.0 )	// Try as many times as server reservation seconds
{
	m_reservationCookie = reservationCookie;
	m_uiReservationStage = uiReservationStage;
}

void CServerMsg_CheckReservation::Release()
{
	if ( m_pParent )
		m_pParent->m_arrSvReservationCheck.FindAndFastRemove( this );
	delete this;
}

void CServerMsg_CheckReservation::SendMsg( const ns_address &serverAdr, int socket, uint32 token )
{

	// send the reservation message
	char	buffer[64];
	bf_write msg(buffer,sizeof(buffer));

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( A2S_RESERVE_CHECK );
	msg.WriteLong( GetHostVersion() );
	msg.WriteLong( token );
	msg.WriteLong( m_uiReservationStage );
	msg.WriteLongLong( m_reservationCookie );
#ifndef SWDS
	msg.WriteLongLong( Steam3Client().SteamUser()->GetSteamID().ConvertToUint64() );
#else
	msg.WriteLongLong( 0 );
#endif

	#ifndef DEDICATED
		if ( serverAdr.GetAddressType() == NSAT_PROXIED_GAMESERVER )
			NET_InitSteamDatagramProxiedGameserverConnection( serverAdr );
	#endif

	NET_SendPacket( NULL, socket, serverAdr, msg.GetData(), msg.GetNumBytesWritten() );
}

void CServerMsg_CheckReservation::ResponseReceived( const ns_address &from, bf_read &msg, int32 hostVersion, uint32 token )
{
	if ( hostVersion != GetHostVersion() )
		return;

	if ( !IsValidResponse( from, token ) )
		return;

	uint32 uiReservationStage = msg.ReadLong();
	int numPlayersAwaiting = msg.ReadByte();
	if ( numPlayersAwaiting == 0 )
	{
		DevMsg( "Server confirmed all players reservation%u\n", uiReservationStage );
		CServerMsg::ResponseReceived( numPlayersAwaiting );
	}
	else
	{
		DevMsg( "Server reservation%u is awaiting %d\n", uiReservationStage, numPlayersAwaiting );

		if ( 0x7F == numPlayersAwaiting )
		{
			// Failed to receive a reply
			m_eState = AOS_FAILED;
			m_pCallback->OnOperationFinished( this );
		}
		else
		{
			m_result = numPlayersAwaiting;
			m_pCallback->OnOperationFinished( this ); // we remain in AOS_RUNNING, just notify the callback
		}
	}
}

CServerMsg_Ping::CServerMsg_Ping( CBaseClientState *pParent, IMatchAsyncOperationCallback *pCallback, const ns_address &serverAdr, int socket ) :
	CServerMsg( pParent, pCallback, serverAdr, socket, 3, 5.0 )
{
	m_timeLastMsgSent = 0.0;
}

void CServerMsg_Ping::Release()
{
	if ( m_pParent )
		m_pParent->m_arrSvPing.FindAndFastRemove( this );
	delete this;
}

void CServerMsg_Ping::SendMsg( const ns_address &serverAdr, int socket, uint32 token )
{
	m_timeLastMsgSent = net_time;

	char	buffer[64];
	bf_write msg(buffer,sizeof(buffer));

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( A2S_PING );
	msg.WriteLong( GetHostVersion() );
	msg.WriteLong( token );

	#ifndef DEDICATED
		if ( serverAdr.GetAddressType() == NSAT_PROXIED_GAMESERVER )
			NET_InitSteamDatagramProxiedGameserverConnection( serverAdr );
	#endif

	DevMsg( "Pinging %s\n", ns_address_render( serverAdr ).String() );
	NET_SendPacket( NULL, socket, serverAdr, msg.GetData(), msg.GetNumBytesWritten() );
}

void CServerMsg_Ping::ResponseReceived( const ns_address& from, bf_read &msg, int32 hostVersion, uint32 token )
{
	if ( hostVersion != GetHostVersion() )
		return;

	if ( !IsValidResponse( from, token ) )
		return;

	double dt = net_time - m_timeLastMsgSent;
	uint64 result = (uint64)( dt * 1000 );
	CServerMsg::ResponseReceived( result );
}

// ---------------------------------------------------------------------------------------- //
// C_ServerClassInfo implementation.
// ---------------------------------------------------------------------------------------- //

CBaseClientState::CBaseClientState() :
	m_BaselineHandles( DefLessFunc( int ) )
{
	m_bSplitScreenUser = false;
	m_Socket = NS_CLIENT;
	m_pServerClasses = NULL;
	m_StringTableContainer = NULL;
	m_NetChannel = NULL;
	m_nSignonState = SIGNONSTATE_NONE;
	m_nChallengeNr = 0;
	m_flConnectTime = 0;
	m_nRetryNumber = 0;
	m_nRetryMax = CL_CONNECTION_RETRIES;
	m_nServerCount = 0;
	m_nCurrentSequence = 0;
	m_nDeltaTick = 0;
	m_bPaused = 0;
	m_nViewEntity = 0;
	m_nPlayerSlot = 0;
	m_nSplitScreenSlot = 0;
	m_nMaxClients = 0;
	m_nNumPlayersToConnect = 1;
	Q_memset( m_pEntityBaselines, 0, sizeof( m_pEntityBaselines ) );
	m_nServerClasses = 0;
	m_nServerClassBits = 0;
	m_ListenServerSteamID = 0ull;
	m_flNextCmdTime = -1.0f;
	Q_memset( m_szLevelName, 0, sizeof( m_szLevelName ) );
	Q_memset( m_szLevelNameShort, 0, sizeof( m_szLevelNameShort ) );
	Q_memset( m_szLastLevelNameShort, 0, sizeof( m_szLevelNameShort ) );
	m_iEncryptionKeySize = 0;
	Q_memset( m_szEncryptionKey, 0, sizeof( m_szEncryptionKey ) );

	m_bRestrictServerCommands = true;
	m_bRestrictClientCommands = true;
	m_bServerConnectionRedirect = false;
	m_bServerInfoProcessed = false;
	m_nServerProtocolVersion = 0;
	m_nServerInfoMsgProtocol = 0;

	m_pServerReservationOperation = NULL;
	m_pServerReservationCallback = NULL;
	m_flReservationMsgSendTime = 0;
	m_nReservationMsgRetryNumber = 0;
	m_bEnteredPassword = false;
	m_bWaitingForPassword = false;
#if ENGINE_CONNECT_VIA_MMS
	m_bWaitingForServerGameDetails = false;
#endif
	m_nServerReservationCookie = 0;
	m_pKVGameSettings = NULL;
	m_unUGCMapFileID = 0;
	m_ulGameServerSteamID = 0;
}

CBaseClientState::~CBaseClientState()
{
	if ( m_pKVGameSettings )
	{
		m_pKVGameSettings->deleteThis();
		m_pKVGameSettings = NULL;
	}

	FOR_EACH_MAP( m_BaselineHandles, i )
	{
		g_pSerializedEntities->ReleaseSerializedEntity( m_BaselineHandles[ i ] );
	}
	m_BaselineHandles.RemoveAll();
}

void CBaseClientState::Clear( void )
{
	m_nServerCount = -1;
	m_nDeltaTick = -1;
	
	m_ClockDriftMgr.Clear();

	m_nCurrentSequence = 0;
	m_nServerClasses = 0;
	m_nServerClassBits = 0;
	m_nPlayerSlot = 0;
	m_nSplitScreenSlot = 0;
	m_szLevelName[0] = 0;
	m_nMaxClients = 0;
	m_unUGCMapFileID = 0;
	// m_nNumPlayersToConnect = 1; <-- when clearing state we need to preserve num players to properly "reconnect" to the server

	// Need to cache off the name of the current map before clearing it so we can use when doing a memory flush.
	if ( m_szLevelNameShort[0] )
	{
		V_strncpy( m_szLastLevelNameShort, m_szLevelNameShort, sizeof( m_szLastLevelNameShort ) );
	}
	m_szLevelNameShort[ 0 ] = 0;

	if ( m_pServerClasses )
	{
		delete[] m_pServerClasses;
		m_pServerClasses = NULL;
	}

	if ( m_StringTableContainer  )
	{
#ifndef SHARED_NET_STRING_TABLES
		m_StringTableContainer->RemoveAllTables();
#endif
	
		m_StringTableContainer = NULL;
	}

	FreeEntityBaselines();

	if ( !m_bSplitScreenUser )
	{
		RecvTable_Term( false );
	}

	if ( m_NetChannel ) 
		m_NetChannel->Reset();
	
	m_bPaused = 0;
	m_nViewEntity = 0;
	m_nChallengeNr = 0;
	m_flConnectTime = 0.0f;

	m_bServerInfoProcessed = false;
	m_nServerProtocolVersion = 0;
	m_nServerInfoMsgProtocol = 0;

	// Free all avatar data
	m_mapPlayerAvatarData.PurgeAndDeleteElements();
}

void CBaseClientState::FileReceived( const char * fileName, unsigned int transferID, bool bIsReplayDemoFile )
{
	ConMsg( "CBaseClientState::FileReceived: %s.\n", fileName );
#if defined( REPLAY_ENABLED )
	if ( isReplayDemoFile )
	{
		CClientReplayHistoryEntryData *pEntry = static_cast< CClientReplayHistoryEntryData *>( g_pClientReplayHistoryManager->FindEntry( fileName ) );		Assert( pEntry );
		if ( pEntry )
		{
			pEntry->m_bTransferComplete = true;
			g_pClientReplayHistoryManager->FlushEntriesToDisk();
		}
	}
#endif
}

void CBaseClientState::FileDenied(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile )
{
	ConMsg( "CBaseClientState::FileDenied: %s.\n", fileName );
}

void CBaseClientState::FileRequested(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile )
{
	ConMsg( "File '%s' requested from %s.\n", fileName, m_NetChannel->GetAddress() );

	m_NetChannel->SendFile( fileName, transferID, bIsReplayDemoFile ); // CBaseClientState always sends file
}

void CBaseClientState::FileSent(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile )
{
	ConMsg( "File '%s' sent.\n", fileName );
}


void CBaseClientState::ConnectionStart(INetChannel *chan)
{
	m_NETMsgTick.Bind< CNETMsg_Tick_t >( chan, UtlMakeDelegate( this, &CBaseClientState::NETMsg_Tick ) );
	m_NETMsgStringCmd.Bind< CNETMsg_StringCmd_t >( chan, UtlMakeDelegate( this, &CBaseClientState::NETMsg_StringCmd ) );
	m_NETMsgSignonState.Bind< CNETMsg_SignonState_t >( chan, UtlMakeDelegate( this, &CBaseClientState::NETMsg_SignonState ) );
	m_NETMsgSetConVar.Bind< CNETMsg_SetConVar_t >( chan, UtlMakeDelegate( this, &CBaseClientState::NETMsg_SetConVar ) );
	m_NETMsgPlayerAvatarData.Bind< CNETMsg_PlayerAvatarData_t >( chan, UtlMakeDelegate( this, &CBaseClientState::NETMsg_PlayerAvatarData ) );
	
	m_SVCMsgServerInfo.Bind< CSVCMsg_ServerInfo_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_ServerInfo ) );
	m_SVCMsgClassInfo.Bind< CSVCMsg_ClassInfo_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_ClassInfo ) );
	m_SVCMsgSendTable.Bind< CSVCMsg_SendTable_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_SendTable ) );
	m_SVCMsgCmdKeyValues.Bind< CSVCMsg_CmdKeyValues_t>( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_CmdKeyValues ) );
	m_SVCMsg_EncryptedData.Bind< CSVCMsg_EncryptedData_t>( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_EncryptedData ) );
	m_SVCMsgPrint.Bind< CSVCMsg_Print_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_Print ) );
	m_SVCMsgSetPause.Bind< CSVCMsg_SetPause_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_SetPause ) );
	m_SVCMsgSetView.Bind< CSVCMsg_SetView_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_SetView ) );
	m_SVCMsgCreateStringTable.Bind< CSVCMsg_CreateStringTable_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_CreateStringTable ) );
	m_SVCMsgUpdateStringTable.Bind< CSVCMsg_UpdateStringTable_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_UpdateStringTable ) );
	m_SVCMsgVoiceInit.Bind< CSVCMsg_VoiceInit_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_VoiceInit ) );
	m_SVCMsgVoiceData.Bind< CSVCMsg_VoiceData_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_VoiceData ) );	
	m_SVCMsgFixAngle.Bind< CSVCMsg_FixAngle_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_FixAngle ) );
	m_SVCMsgPrefetch.Bind< CSVCMsg_Prefetch_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_Prefetch ) );
	m_SVCMsgCrosshairAngle.Bind< CSVCMsg_CrosshairAngle_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_CrosshairAngle ) );
	m_SVCMsgBSPDecal.Bind< CSVCMsg_BSPDecal_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_BSPDecal ) );
	m_SVCMsgSplitScreen.Bind< CSVCMsg_SplitScreen_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_SplitScreen ) );
	m_SVCMsgGetCvarValue.Bind< CSVCMsg_GetCvarValue_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_GetCvarValue ) );
	m_SVCMsgMenu.Bind< CSVCMsg_Menu_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_Menu ) );
	m_SVCMsgUserMessage.Bind< CSVCMsg_UserMessage_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_UserMessage ) );
	m_SVCMsgPaintmapData.Bind< CSVCMsg_PaintmapData_t >( chan, UtlMakeDelegate(this, &CBaseClientState::SVCMsg_PaintmapData ) );
	m_SVCMsgGameEvent.Bind< CSVCMsg_GameEvent_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_GameEvent ) );
	m_SVCMsgGameEventList.Bind< CSVCMsg_GameEventList_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_GameEventList ) );
	m_SVCMsgTempEntities.Bind< CSVCMsg_TempEntities_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_TempEntities ) );
	m_SVCMsgPacketEntities.Bind< CSVCMsg_PacketEntities_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_PacketEntities ) );
	m_SVCMsgSounds.Bind< CSVCMsg_Sounds_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_Sounds ) );
	m_SVCMsgEntityMsg.Bind< CSVCMsg_EntityMsg_t >( chan, UtlMakeDelegate( this, &CBaseClientState::SVCMsg_EntityMsg ) );	
}

void CBaseClientState::ConnectionStop( )
{
	m_NETMsgTick.Unbind();
	m_NETMsgStringCmd.Unbind();
	m_NETMsgSignonState.Unbind();
	m_NETMsgSetConVar.Unbind();
	m_NETMsgPlayerAvatarData.Unbind();

	m_SVCMsgServerInfo.Unbind();
	m_SVCMsgClassInfo.Unbind();
	m_SVCMsgSendTable.Unbind();
	m_SVCMsgCmdKeyValues.Unbind();
	m_SVCMsg_EncryptedData.Unbind();
	m_SVCMsgPrint.Unbind();
	m_SVCMsgSetPause.Unbind();
	m_SVCMsgSetView.Unbind();
	m_SVCMsgCreateStringTable.Unbind();
	m_SVCMsgUpdateStringTable.Unbind();
	m_SVCMsgVoiceInit.Unbind();
	m_SVCMsgVoiceData.Unbind();
	m_SVCMsgFixAngle.Unbind();
	m_SVCMsgPrefetch.Unbind();
	m_SVCMsgCrosshairAngle.Unbind();
	m_SVCMsgBSPDecal.Unbind();
	m_SVCMsgSplitScreen.Unbind();
	m_SVCMsgGetCvarValue.Unbind();
	m_SVCMsgMenu.Unbind();
	m_SVCMsgUserMessage.Unbind();
	m_SVCMsgPaintmapData.Unbind();
	m_SVCMsgGameEvent.Unbind();
	m_SVCMsgGameEventList.Unbind();
	m_SVCMsgTempEntities.Unbind();
	m_SVCMsgPacketEntities.Unbind();
	m_SVCMsgSounds.Unbind();
	m_SVCMsgEntityMsg.Unbind();
}
void CBaseClientState::ConnectionClosing( const char *reason )
{
	ConMsg( "Disconnect: %s.\n", reason?reason:"unknown reason" );
	Disconnect();
}

//-----------------------------------------------------------------------------
// Purpose: A svc_signonnum has been received, perform a client side setup
// Output : void CL_SignonReply
//-----------------------------------------------------------------------------
bool CBaseClientState::SetSignonState ( int state, int count, const CNETMsg_SignonState *msg )
{
	//	ConDMsg ("CL_SignonReply: %i\n", GetBaseLocalClient().signon);

	if ( state < SIGNONSTATE_NONE || state > SIGNONSTATE_CHANGELEVEL )
	{
		ConMsg ("Received signon %i when at %i\n", state, m_nSignonState );
		Assert( 0 );
		return false;
	}

	if ( (state > SIGNONSTATE_CONNECTED) &&	(state <= m_nSignonState) && !m_NetChannel->IsPlayback() )
	{
		ConMsg ("Received signon %i when at %i\n", state, m_nSignonState);
		Assert( 0 );
		return false;
	}

	if ( (count != m_nServerCount) && (count != -1) && (m_nServerCount != -1) && !m_NetChannel->IsPlayback() )
	{
		ConMsg ("Received wrong spawn count %i when at %i\n", count, m_nServerCount );
		Assert( 0 );
		return false;
	}

	if ( m_nSignonState < SIGNONSTATE_CONNECTED && state >= SIGNONSTATE_CONNECTED )
	{
		// Reset direct connect lobby once client is in game
		m_DirectConnectLobby = DirectConnectLobby_t();

		// Reset all client-generated keys that are too old
		if ( m_mapGeneratedEncryptionKeys.Count() > 300 )
		{
			int numPurge = 300 - m_mapGeneratedEncryptionKeys.Count();
			numPurge = MIN( numPurge, 3000 );
			while ( numPurge -- > 0 )
			{
				int32 idxOldestKey = m_mapGeneratedEncryptionKeys.FirstInorder();
				delete [] m_mapGeneratedEncryptionKeys.Element( idxOldestKey );
				m_mapGeneratedEncryptionKeys.RemoveAt( idxOldestKey );
			}
		}
	}

	m_nSignonState = state;
	return true;
}

//////////////////////////////////////////////////////////////////////////
//
// 3rd party plugins managed encryption keys map
//
static CUtlStringMap< CUtlBuffer * > g_mapServersToCertificates;
void RegisterServerCertificate( char const *szServerAddress, int numBytesPayload, void const *pvPayload )
{
	// Allocate new storage
	CUtlBuffer *pNew = new CUtlBuffer;
	pNew->EnsureCapacity( numBytesPayload );
	pNew->SeekPut( CUtlBuffer::SEEK_HEAD, numBytesPayload );
	V_memcpy( pNew->Base(), pvPayload, numBytesPayload );

	UtlSymId_t symid = g_mapServersToCertificates.Find( szServerAddress );
	if ( symid != UTL_INVAL_SYMBOL )
	{
		delete g_mapServersToCertificates[ symid ];
		g_mapServersToCertificates[ symid ] = pNew;
	}
	else
	{
		g_mapServersToCertificates.Insert( szServerAddress, pNew );
	}
}

//-----------------------------------------------------------------------------
// Purpose: called by CL_Connect and CL_CheckResend
// If we are in ca_connecting state and we have gotten a challenge
//   response before the timeout, send another "connect" request.
// Output : void CL_SendConnectPacket
//-----------------------------------------------------------------------------
void CBaseClientState::SendConnectPacket ( const ns_address &netAdrRemote, int challengeNr, int authProtocol, uint64 unGSSteamID, bool bGSSecure )
{
	COM_TimestampedLog( "SendConnectPacket" );

	if ( !netAdrRemote.IsLoopback() )
	{
		bool bFound = m_Remote.IsAddressInList( netAdrRemote );
		if ( !bFound )
		{
			Warning( "Sending connect packet to unexpected address %s\n", ns_address_render( netAdrRemote ).String() );
		}
	}

	const char *CDKey = "NOCDKEY";
	
	char		msg_buffer[MAX_ROUTABLE_PAYLOAD * 2];
	bf_write	msg( msg_buffer, sizeof(msg_buffer) );

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( C2S_CONNECT );
	msg.WriteLong( m_nServerProtocolVersion ? m_nServerProtocolVersion : GetHostVersion() ); // fake to the server as the client with matching version, validate later
	msg.WriteLong( authProtocol );
	msg.WriteLong( challengeNr );
	// msg.WriteString( GetClientName() );	// Name
	msg.WriteString( "" );	// Server can find the name in FCVAR_USERINFO block, save on connectionless packet size
	msg.WriteString( password.GetString() );		// password

	//Send player info for main player and split screen players
	msg.WriteByte( m_nNumPlayersToConnect );
	int numBytesPacketHeader = msg.GetNumBytesWritten();
	for( int playerCount = 0; playerCount < m_nNumPlayersToConnect; ++playerCount )
	{
		CCLCMsg_SplitPlayerConnect_t splitMsg;
		Host_BuildUserInfoUpdateMessage( playerCount, splitMsg.mutable_convars(), false );
		if ( CHLTVClientState *pHLTVClientState = dynamic_cast< CHLTVClientState * >( this ) )
		{
			pHLTVClientState->SetLocalInfoConvarsForUpstreamConnection( *splitMsg.mutable_convars(), true );
		}
#ifdef _DEBUG
		for ( int ii = 0; ii < splitMsg.convars().cvars_size(); ++ ii )
		{
			CMsg_CVars::CVar cvinfo( splitMsg.convars().cvars( ii ) );
			NetMsgExpandCVarUsingDictionary( &cvinfo );
			DevMsg( "[NET] connect user info: '%s' = '%s'\n", cvinfo.name().c_str(), cvinfo.value().c_str() );
		}
#endif
		splitMsg.WriteToBuffer( msg );
	}
	int numBytesFcvarUserInfo = msg.GetNumBytesWritten() - numBytesPacketHeader;

	// Track cookie and certificate
	int numBytesCookie = msg.GetNumBytesWritten();

	// add the low violence setting
	msg.WriteOneBit( g_bLowViolence );

	// add the server reservation cookie, if we have one
	msg.WriteLongLong( m_nServerReservationCookie );

	msg.WriteByte( (uint8)CROSSPLAYPLATFORM_THISPLATFORM );

	//
	// write the client encryption key to be used
	//
	DeferredConnection_t &dc = m_DeferredConnection;
	if ( dc.m_nEncryptionKey )
	{
		msg.WriteLong( dc.m_nEncryptionKey );
		byte *pbEncryptionKey = NULL;
		int32 idx = m_mapGeneratedEncryptionKeys.Find( dc.m_nEncryptionKey );
		if ( idx != m_mapGeneratedEncryptionKeys.InvalidIndex() )
		{
			pbEncryptionKey = m_mapGeneratedEncryptionKeys.Element( idx );
		}
		else
		{
			dc.m_nEncryptedSize = 0;
		}
		msg.WriteLong( dc.m_nEncryptedSize );
		if ( dc.m_nEncryptedSize )
			msg.WriteBytes( pbEncryptionKey + NET_CRYPT_KEY_LENGTH, dc.m_nEncryptedSize );
	}
	else
	{
		// Try to see if there's a client-plugin override for the encryption key for this server?
		bool bWriteZeroEncryptionKey = true;
		ns_address_render renderRemoteAsString( netAdrRemote );
		UtlSymId_t utlKey = g_mapServersToCertificates.Find( renderRemoteAsString.String() );
		if ( utlKey != UTL_INVAL_SYMBOL )
		{
			CUtlBuffer &buf = *g_mapServersToCertificates[ utlKey ];
			const int32 numMetadataBytes = sizeof( int32 ) + NET_CRYPT_KEY_LENGTH;
			if ( buf.TellPut() > numMetadataBytes )
			{
				bWriteZeroEncryptionKey = false;
				
				msg.WriteLong( *reinterpret_cast< int32 * >( buf.Base() ) );
				int32 numEncryptedSize = buf.TellPut() - numMetadataBytes;
				msg.WriteLong( numEncryptedSize );
				msg.WriteBytes( ( const char * )( buf.Base() ) + numMetadataBytes, numEncryptedSize );
			}
		}
		
		if ( bWriteZeroEncryptionKey )
		{
			msg.WriteLong( 0 );
		}
	}

	numBytesCookie = msg.GetNumBytesWritten() - numBytesCookie;
	int numBytesSteamAuth = msg.GetNumBytesWritten();
	switch ( authProtocol )
	{
		// Fall through, bogus protocol type, use CD key hash.
		case PROTOCOL_HASHEDCDKEY:	CDKey = GetCDKeyHash();
									msg.WriteString( CDKey );		// cdkey
									break;

		case PROTOCOL_STEAM:		if ( !PrepareSteamConnectResponse( unGSSteamID, bGSSecure, netAdrRemote, msg ) )
									{
										return;
									}
									break;

		default: 					Host_Error( "Unexepected authentication protocol %i!\n", authProtocol );
									return;
	}
	numBytesSteamAuth = msg.GetNumBytesWritten() - numBytesSteamAuth;

	// Mark time of this attempt for retransmit requests
	m_flConnectTime = net_time;

	// remember challengenr for TCP connection
	m_nChallengeNr = challengeNr;

	// Send protocol and challenge value
	if ( msg.GetNumBytesWritten() > 896 )
	{
		Warning( "[NET] Client connect packet too large for %s, total size %u bytes ( %u header, %u info, %u cookie, %u auth )\n", ns_address_render( netAdrRemote ).String(), msg.GetNumBytesWritten(),
			numBytesPacketHeader, numBytesFcvarUserInfo, numBytesCookie, numBytesSteamAuth );
		Assert( 0 );
	}
	else
	{
		DevMsg( "[NET] Sending client connect packet to %s, total size %u bytes ( %u header, %u info, %u cookie, %u auth )\n", ns_address_render( netAdrRemote ).String(), msg.GetNumBytesWritten(),
			numBytesPacketHeader, numBytesFcvarUserInfo, numBytesCookie, numBytesSteamAuth );
	}
	NET_SendPacket( NULL, m_Socket, netAdrRemote, msg.GetData(), msg.GetNumBytesWritten() );


	// Remember Steam ID, if any
	m_ulGameServerSteamID = unGSSteamID;
}


//-----------------------------------------------------------------------------
// Purpose: append steam specific data to a connection response
//-----------------------------------------------------------------------------
bool CBaseClientState::PrepareSteamConnectResponse( uint64 unGSSteamID, bool bGSSecure, const ns_address &adr, bf_write &msg )
{
	// X360TBD: Network - Steam Dedicated Server hack
	if ( IsX360() )
	{
		return true;
	}

#if !defined( NO_STEAM ) && !defined( DEDICATED )
	if ( !Steam3Client().SteamUser() )
	{
		COM_ExplainDisconnection( true, "The server requires that you be running Steam.\n" );
		Disconnect();
		return false;
	}
#endif
	
#ifndef DEDICATED
	// now append the steam3 cookie
	char steam3Cookie[ STEAM_KEYSIZE ];	
	uint32 steam3CookieLen;
	Steam3Client().GetAuthSessionTicket( steam3Cookie, sizeof(steam3Cookie), &steam3CookieLen, unGSSteamID, bGSSecure );

	msg.WriteShort( steam3CookieLen );
	if ( steam3CookieLen > 0 )
		msg.WriteBytes( steam3Cookie, steam3CookieLen );
#endif

	return true;
}


bool Remote_t::Resolve()
{
	if ( !m_adrRemote.SetFromString( m_szRetryAddress ) )
	{
		return false;
	}

	if ( m_adrRemote.IsType<netadr_t>() && m_adrRemote.AsType<netadr_t>().GetPort() == 0 )
	{
		m_adrRemote.AsType<netadr_t>().SetPort( PORT_SERVER );
	}

	return true;
}

bool CAddressList::IsRemoteInList( char const *pchAdrCheck ) const
{
	for ( int i = 0; i < m_List.Count(); ++i )
	{
		if ( !Q_stricmp( pchAdrCheck, Get( i ).m_szRetryAddress.String() ) )
			return true;
	}
	return false;
}

bool CAddressList::IsAddressInList( const ns_address &adr ) const
{
	for ( int i = 0; i < m_List.Count(); ++i )
	{
		if ( adr.CompareAdr( Get( i ).m_adrRemote ) )
			return true;
	}
	return false;
}

void CAddressList::RemoveAll()
{
	m_List.RemoveAll();
}

void CAddressList::Describe( CUtlString &str )
{
	for ( int i = 0; i < m_List.Count(); ++i )
	{
		str += va( "%s(%s) ", Get( i ).m_szAlias.String(), ns_address_render( Get( i ).m_adrRemote ).String() );
	}
}

int	CAddressList::Count() const
{
	return m_List.Count();
}

Remote_t &CAddressList::Get( int index )
{
	Assert( index >= 0 && index < m_List.Count() );
	return m_List[ index ];
}

const Remote_t &CAddressList::Get( int index ) const
{
	Assert( index >= 0 && index < m_List.Count() );
	return m_List[ index ];
}

void CAddressList::AddRemote( char const *pchAddress, char const *pchAlias )
{
	if ( IsRemoteInList( pchAddress ) )
		return;

	Remote_t remote;
	remote.m_szRetryAddress = pchAddress;
	remote.m_szAlias = pchAlias;
	remote.Resolve();

	m_List.AddToTail( remote );
}


void CBaseClientState::ConnectInternal( const char *pchPublicAddress, char const *pchPrivateAddress, int numPlayers, const char* szJoinType )
{
#ifndef DEDICATED
#if !defined( NO_STEAM )
	if ( !IsX360() )	// X360 matchmaking sets the forced user info values
	{
		// Get our name from steam. Needs to be done before connecting
		// because we won't have triggered a check by changing our name.
		IConVar *pVar = g_pCVar->FindVar( "name" );
		if ( pVar )
		{
			SetNameToSteamIDName( pVar );
		}
	}
#endif
#endif

	m_Remote.RemoveAll();
	m_Remote.AddRemote( pchPublicAddress, "public" );
	m_Remote.AddRemote( pchPrivateAddress, "private" );

	if ( ShouldUseDirectConnectAddress( m_Remote ) )
	{
		ConColorMsg( Color( 0, 255, 0, 255 ), "Adding direct connect address to connection %s\n",  ns_address_render( m_DirectConnectLobby.m_adrRemote ).String() );
		m_Remote.AddRemote( ns_address_render( m_DirectConnectLobby.m_adrRemote ).String(), "direct" );
	}

	//standard connect always connects one players.
	m_nNumPlayersToConnect = numPlayers;

	// For the check for resend timer to fire a connection / getchallenge request.
	SetSignonState( SIGNONSTATE_CHALLENGE, -1, NULL );

	// Force connection request to fire.
	m_flConnectTime = -FLT_MAX;  

	m_nRetryNumber = 0;

	// Retry for up to timeout seconds
	m_nRetryMax =  cl_resend_timeout.GetFloat() / cl_resend.GetFloat();	

	m_ulGameServerSteamID = 0;

#if !defined ( DEDICATED )
	if ( szJoinType && g_ClientDLL )
		g_ClientDLL->RecordUIEvent( szJoinType );
#endif
}

void CBaseClientState::Connect( const char *pchPublicAddress, char const *pchPrivateAddress, const char* szJoinType )
{
	ConnectInternal( pchPublicAddress, pchPrivateAddress, 1, szJoinType );
}

void CBaseClientState::ConnectSplitScreen( const char *pchPublicAddress, char const *pchPrivateAddress, int numPlayers, const char* szJoinType )
{
	ConnectInternal( pchPublicAddress, pchPrivateAddress, numPlayers, szJoinType );
}

INetworkStringTable *CBaseClientState::GetStringTable( const char * name ) const
{
	if ( !m_StringTableContainer )
	{
		Assert( m_StringTableContainer );
		return NULL;
	}

	return m_StringTableContainer->FindTable( name );
}

void CBaseClientState::ForceFullUpdate( char const *pchReason )
{
	if ( m_nDeltaTick == -1 )
		return;

	FreeEntityBaselines();
	m_nDeltaTick = -1;
	DevMsg( "Requesting full game update (%s)...\n", pchReason );
}

void CBaseClientState::FullConnect( const ns_address &adr, int nEncryptionKey )
{
	// Initiate the network channel

	byte *pbEncryptionKey = NULL;
	if ( nEncryptionKey )
	{
		int32 idxEncryptedKey = m_mapGeneratedEncryptionKeys.Find( nEncryptionKey );
		if ( idxEncryptedKey != m_mapGeneratedEncryptionKeys.InvalidIndex() )
		{
			pbEncryptionKey = m_mapGeneratedEncryptionKeys.Element( idxEncryptedKey );
		}
		else
		{
			ns_address_render renderRemoteAsString( adr );
			UtlSymId_t utlKey = g_mapServersToCertificates.Find( renderRemoteAsString.String() );
			if ( utlKey != UTL_INVAL_SYMBOL )
			{
				CUtlBuffer &buf = *g_mapServersToCertificates[ utlKey ];
				const int32 numMetadataBytes = sizeof( int32 ) + NET_CRYPT_KEY_LENGTH;
				if ( buf.Size() > numMetadataBytes )
				{
					pbEncryptionKey = ( ( byte * ) ( buf.Base() ) + sizeof( int32 ) );
				}
			}
		}
	}
	
	COM_TimestampedLog( "CBaseClientState::FullConnect" );

	m_NetChannel = NET_CreateNetChannel( m_Socket, &adr, "CLIENT", this, pbEncryptionKey, false );

	Assert( m_NetChannel );
	
	m_NetChannel->StartStreaming( m_nChallengeNr );	// open TCP stream

	// Bump connection time to now so we don't resend a connection
	// Request	
	m_flConnectTime = net_time; 

	// We'll request a full delta from the baseline
	m_nDeltaTick = -1;

	// We can send a cmd right away
	m_flNextCmdTime = net_time;

	// If we used a server reservation cookie to connect, clear it
	m_nServerReservationCookie = 0;

	// Mark client as connected
	SetSignonState( SIGNONSTATE_CONNECTED, -1, NULL );
#if !defined(DEDICATED)
	ns_address rconAdr = m_NetChannel->GetRemoteAddress();
	if ( rconAdr.IsType<netadr_t>() )
	{
		RCONClient().SetAddress( rconAdr.AsType<netadr_t>() );
	}
#endif
}

void CBaseClientState::ConnectionCrashed(const char *reason)
{
	ConMsg( "Connection lost: %s.\n", reason?reason:"unknown reason" );
	Disconnect();
}

void CBaseClientState::Disconnect( bool bShowMainMenu )
{
	m_DeferredConnection.m_bActive = false;
	m_bWaitingForPassword = false;
#if ENGINE_CONNECT_VIA_MMS
	m_bWaitingForServerGameDetails = false;
#endif
	m_bEnteredPassword = false;
	m_flConnectTime = -FLT_MAX;
	m_nRetryNumber = 0;
	m_ulGameServerSteamID = 0;

	if ( m_nSignonState == SIGNONSTATE_NONE )
		return;

#if !defined( DEDICATED ) && defined( ENABLE_RPT )
	CL_NotifyRPTOfDisconnect( );
#endif

	SetSignonState( SIGNONSTATE_NONE, -1, NULL );
	// Don't clear cookie here as this can get called as part of connection process if changing to new server, etc.
	// m_nServerReservationCookie = 0;		

	ns_address adr;
	if ( m_NetChannel )
	{
		adr = m_NetChannel->GetRemoteAddress();
	}
	else if ( m_Remote.Count() > 0 )
	{
		const char *pszAddr = m_Remote.Get( 0 ).m_szRetryAddress;
		if ( !adr.SetFromString( m_Remote.Get( 0 ).m_szRetryAddress ) )
		{
			Warning( "Unable to parse retry address '%s'\n", pszAddr );
		}
	}

#ifndef DEDICATED
	ns_address checkAdr = adr;
	if ( adr.IsLoopback() || adr.IsLocalhost() )
	{
		checkAdr.AsType<netadr_t>().SetIP( net_local_adr.GetIPHostByteOrder() );
	}

	if ( m_ListenServerSteamID != 0ull && m_Remote.Count() > 0 )
	{
		Assert( g_pSteamSocketMgr->GetSteamIDForRemote( m_Remote.Get( 0 ).m_adrRemote ) == m_ListenServerSteamID );
		NET_TerminateSteamConnection( m_Socket, m_ListenServerSteamID );
		m_ListenServerSteamID = 0ull;
	}
 	Steam3Client().CancelAuthTicket();
#endif

	if ( m_NetChannel )
	{
		m_NetChannel->Shutdown( "Disconnect" );
		m_NetChannel = NULL;
	}

#ifndef DEDICATED
	// Get rid of any whitelist in our filesystem and reload any files that the previous whitelist forced 
	// to come from Steam.
	// MD: This causes an annoying pause when you disconnect from a server, so just leave the last whitelist active
	// until you connect to a new server.
	//CL_HandlePureServerWhitelist( NULL );
#endif

#ifndef DEDICATED
	if ( m_bSplitScreenUser && 
		splitscreen->IsValidSplitScreenSlot( m_nSplitScreenSlot ) )
	{
		splitscreen->RemoveSplitScreenUser( m_nSplitScreenSlot, m_nPlayerSlot + 1 );
	}

#if defined( INCLUDE_SCALEFORM )
	if ( g_pScaleformUI )
	{
		g_pScaleformUI->ShutdownIME();
		g_pScaleformUI->SlotRelease( SF_SS_SLOT( m_nSplitScreenSlot ) );
	}
#endif

#endif // DEDICATED
}

void CBaseClientState::RunFrame (void)
{
	VPROF("CBaseClientState::RunFrame");
	if ( (m_nSignonState > SIGNONSTATE_NEW) && m_NetChannel && g_GameEventManager.HasClientListenersChanged() )
	{
		// assemble a list of all events we listening to and tell the server
		CCLCMsg_ListenEvents_t msg;
		g_GameEventManager.WriteListenEventList( &msg );
		m_NetChannel->SendNetMsg( msg );
	}

	if ( m_nSignonState == SIGNONSTATE_CHALLENGE )
	{
		CheckForResend();
	}

	CheckForReservationResend();

	for ( int i = 0; i < m_arrSvReservationCheck.Count(); ++ i )
	{
		CServerMsg_CheckReservation *pSv = m_arrSvReservationCheck[ i ];
		Assert( pSv );
		if ( pSv )
		{
			pSv->Update();
			// Calling "Update" will potentially call handlers' interface async operation
			// finished callback which can release "pSv" object and modify
			// contents of our clientstate m_arrSvPing array. Always must check
			// it again for being in array before attempting to dereference it.
			// Also, it is ok to skip update on some ping objects when array changes
			// since update is only responsible for re-sends of the pings
		}
	}
	for ( int i = 0; i < m_arrSvReservationCheck.Count(); ++ i )
	{
		CServerMsg_CheckReservation *pSv = m_arrSvReservationCheck[ i ];
		Assert( pSv );
		if ( pSv && pSv->IsFinished() )
		{
			// delete pSvPing; -- client will release
			m_arrSvReservationCheck.FastRemove( i -- );
		}
	}

	for ( int iSvPing = 0; iSvPing < m_arrSvPing.Count(); ++ iSvPing )
	{
		CServerMsg_Ping *pSvPing = m_arrSvPing[ iSvPing ];
		Assert( pSvPing );
		if ( pSvPing )
		{
			pSvPing->Update();
			// Calling "Update" will potentially call handlers' interface async operation
			// finished callback which can release "pSvPing" object and modify
			// contents of our clientstate m_arrSvPing array. Always must check
			// it again for being in array before attempting to dereference it.
			// Also, it is ok to skip update on some ping objects when array changes
			// since update is only responsible for re-sends of the pings
		}
	}
	for ( int iSvPing = 0; iSvPing < m_arrSvPing.Count(); ++ iSvPing )
	{
		CServerMsg_Ping *pSvPing = m_arrSvPing[ iSvPing ];
		Assert( pSvPing );
		if ( pSvPing && pSvPing->IsFinished() )
		{
			// delete pSvPing; -- client will release
			m_arrSvPing.FastRemove( iSvPing -- );
		}
	}
}

/*
=================
ResetConnectionRetries

Reset the resend state so that the next call to CheckForResend()
will try. Call this after a listen server connects to Steam.
=================
*/
void CBaseClientState::ResetConnectionRetries()
{
	m_flConnectTime = -FLT_MAX;
	m_nRetryNumber = 0;
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CBaseClientState::CheckForResend ( bool bForceResendNow /* = false */ )
{
	// resend if we haven't gotten a reply yet
	// We only resend during the connection process.
	if ( m_nSignonState != SIGNONSTATE_CHALLENGE )
		return;

	if ( m_bWaitingForPassword )
		return;

	// Wait at least the resend # of seconds.
	if ( !bForceResendNow && ( ( net_time - m_flConnectTime ) < cl_resend.GetFloat() ) )
		return;

	// No addresses in list!
	if ( m_Remote.Count() <= 0 )
	{
		Assert( 0 );
		return;
	}

	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		if ( !m_Remote.Get( i ).Resolve() )
		{
			ConMsg( "Bad server address %s(%s)\n", m_Remote.Get( i ).m_szAlias.String(), m_Remote.Get( i ).m_szRetryAddress.String() );
			Disconnect();
			return;
		}
	}

	// Only retry so many times before failure.
	if ( m_nRetryNumber >= GetConnectionRetryNumber() )
	{
		COM_ExplainDisconnection( true, "Connection failed after %i retries.\n", GetConnectionRetryNumber() );
		// Host_Disconnect();
		Disconnect();
		return;
	}
	
	// Mark time of this attempt.
	m_flConnectTime = net_time;	// for retransmit requests

	// Display appropriate message
	if ( !StringHasPrefix( m_Remote.Get( 0 ).m_szRetryAddress, "localhost" ) )
	{
		CUtlString desc;
		m_Remote.Describe( desc );
		ConMsg ("%s %s...\n", m_nRetryNumber == 0 ? "Connecting to" : "Retrying", desc.String() );
	}

#ifndef DEDICATED
#if ENGINE_CONNECT_VIA_MMS
	if ( m_bWaitingForServerGameDetails )
		// This is a special handler for when we need to fetch server game details
		// before we can connect to the server
	{
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			Remote_t &remote = m_Remote.Get( i );
			ResendGameDetailsRequest( remote.m_adrRemote );
		}

		++m_nRetryNumber;
		return;
	}
#endif
#endif

	char payload[ 128 ];
	Q_snprintf( payload, sizeof( payload ), "%cconnect0x%08X", A2S_GETCHALLENGE, m_DeferredConnection.m_nChallenge );

	// Request another challenge value.
	ISteamSocketMgr::ESteamCnxType cnxType = g_pSteamSocketMgr->GetCnxType();

	bool bShouldSendSteamRetry = 
		( m_ListenServerSteamID != 0ull ) && 
		( m_nRetryNumber > 0 ) &&
		!( m_nRetryNumber & 0x1 );

	if ( m_ListenServerSteamID != 0ull &&
		cnxType == ISteamSocketMgr::ESCT_ALWAYS ) 
	{
		bShouldSendSteamRetry = true;
	}

	if ( IsX360() )
		bShouldSendSteamRetry = false;
 	else if ( m_ListenServerSteamID ) // PORTAL2-specific: force Steam cnx for P2P (todo: need latest Steam P2P APIs)
 		bShouldSendSteamRetry = true;

	if ( !bShouldSendSteamRetry )
	{
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			const Remote_t &remote = m_Remote.Get( i );
			const char *pszProtocol = "[unknown protocol]";
			char szAddress[128];
			V_strcpy_safe( szAddress, ns_address_render( remote.m_adrRemote ).String() );
			switch ( remote.m_adrRemote.GetAddressType() )
			{
				case NSAT_PROXIED_CLIENT: // we're connecting to a server, not a client
				default:
					Assert( false );
					break;
				case NSAT_NETADR:
					pszProtocol = "UDP";
					if ( cl_hideserverip.GetInt()>0 )
						V_sprintf_safe( szAddress, "<hidden>" );
					break;
				case NSAT_P2P:
					pszProtocol = "SteamP2P";
					break;

				case NSAT_PROXIED_GAMESERVER:
					#ifdef DEDICATED
						Assert( false );
					#else

						// Make sure we have a ticket, and are setup to talk to this guy
						if ( !NET_InitSteamDatagramProxiedGameserverConnection( remote.m_adrRemote ) )
							continue;

						pszProtocol = "SteamDatagram";
					#endif
					break;
			}
			if ( developer.GetInt() != 0 )
			{
				ConColorMsg( Color( 0, 255, 0, 255 ), "%.3f:  Sending connect to %s address %s via %s\n", net_time, remote.m_szAlias.String(), szAddress, pszProtocol );
			}
			
			NET_OutOfBandDelayedPrintf( m_Socket, remote.m_adrRemote, GetPrivateIPDelayMsecs() * i, payload, Q_strlen( payload ) + 1 );
		}
	}
	else if ( m_ListenServerSteamID )
	{
		Msg( "%.3f:  Sending Steam connect to %s %llx\n", net_time, m_Remote.Get( 0 ).m_szRetryAddress.String(), m_ListenServerSteamID );
		m_Remote.Get( 0 ).m_adrRemote = NET_InitiateSteamConnection( m_Socket, m_ListenServerSteamID, payload, Q_strlen( payload ) + 1 );
	}
	else
	{
		Warning( "%.3f:  Steam connection to unknown SteamId (%s) failed!\n", net_time, m_Remote.Get( 0 ).m_szRetryAddress.String() );
	}

	++m_nRetryNumber;
}

void CBaseClientState::ResendGameDetailsRequest( const ns_address &adr )
{
#ifndef DEDICATED
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
		"Client::ResendGameDetailsRequest", "to", ns_address_render( adr ).String() ) );
#endif
}

static void Read_S2A_INFO_SRC( const ns_address &from, bf_read *msg )
{
	char str[ 1024 ];

	Msg( "Responder   : %s\n", ns_address_render( from ).String() );

	// read protocol version
	Msg( "Protocol    : %d\n", (int)msg->ReadByte() );

	msg->ReadString( str, sizeof( str ) );
	Msg( "Hostname    : %s\n", str );

	msg->ReadString( str, sizeof( str ) );
	Msg( "Map         : %s\n", str );

	msg->ReadString( str, sizeof( str ) );
	Msg( "Game        : %s\n", str );

	msg->ReadString( str, sizeof( str ) );
	Msg( "Description : %s\n", str );

	Msg( "AppID       : %u\n", (unsigned int)msg->ReadShort() );

	Msg( "Players      : %u\n", (unsigned int)msg->ReadByte() );
	Msg( "MaxPlayers   : %u\n", (unsigned int)msg->ReadByte() );
	Msg( "Bots         : %u\n", (unsigned int)msg->ReadByte() );

	char const *sType = "???";
	switch ( msg->ReadByte() )
	{
	default:
		break;
	case 'd':
		sType = "dedicated";
		break;
	case 'p':
		sType = "proxy";
		break;
	case 'l':
		sType = "listen";
		break;
	}
	Msg( "Server Type  : %s\n", sType );

	char const *osType = "???";
	switch ( msg->ReadByte() )
	{
	default:
		break;
	case 'l':
		osType = "Linux";
		break;
	case 'w':
		osType = "Windows";
		break;
	}

	Msg( "OS    Type   : %s\n", osType );

	Msg( "Password     : %s\n", msg->ReadByte() > 0 ? "yes" : "no" );
	Msg( "Secure       : %s\n", msg->ReadByte() > 0 ? "yes" : "no" );

	msg->ReadString( str, sizeof( str ) );
	Msg( "Version      : %s\n", str );

	if ( msg->GetNumBytesLeft() <= 0 )
		return;

	unsigned char infoByte = msg->ReadByte();
	if ( infoByte & S2A_EXTRA_DATA_HAS_GAME_PORT )
	{
		Msg( "Game Port    : %u\n", (unsigned short)msg->ReadShort() );
	}

	if ( infoByte & S2A_EXTRA_DATA_HAS_SPECTATOR_DATA )
	{
		Msg( "Spectator Port: %u\n", (unsigned short)msg->ReadShort() );
		msg->ReadString( str, sizeof( str ) );
		Msg( "SpectatorName : %s\n", str );
	}

	if ( infoByte & S2A_EXTRA_DATA_HAS_GAMETAG_DATA )
	{
		msg->ReadString( str, sizeof( str ) );
		Msg( "Public Tags   : %s\n", str );
	}
}

bool CBaseClientState::ProcessConnectionlessPacket( netpacket_t *packet )
{
	VPROF( "ProcessConnectionlessPacket" );

	Assert( packet );

	// NOTE: msg is a reference to packet->message, so reading
	// from "msg" will also advance read-pointer in "packet->message"!!!
	// ... and vice-versa, hence passing "packet" to a nested function
	// will make that function receive a modified message with advanced
	// read pointer.
	// [ this differs from server-side connectionless packet processing ]

	bf_read &msg = packet->message;	// handy shortcut 
	bf_read msgOriginal = packet->message;

	int c = msg.ReadByte();

	char string[MAX_ROUTABLE_PAYLOAD];

	// FIXME:  For some of these, we should confirm that the sender of 
	// the message is what we think the server is...

	switch ( c )
	{
		
	case S2C_CONNECTION:	
		if ( ( m_nSignonState == SIGNONSTATE_CHALLENGE ) &&
			( packet->from.CompareAdr(m_DeferredConnection.m_adrServerAddress, true ) ) &&
			( msg.ReadByte() == '.' ) )
		{
			char chEncryptionKeyIndex[9] = {};
			for ( int j = 0; j < 8; ++ j )
				chEncryptionKeyIndex[j] = msg.ReadByte();

			bool bConnectionExpectingEncryptionKey = ( m_DeferredConnection.m_nEncryptionKey != 0 ) ||
				( g_mapServersToCertificates.Find( ns_address_render( packet->from ).String() ) != UTL_INVAL_SYMBOL );

			int nEncryptionKeyIndex = 0;
			if ( ( 1 == sscanf( chEncryptionKeyIndex, "%08X", &nEncryptionKeyIndex ) ) &&
				( ( nEncryptionKeyIndex != 0 ) == bConnectionExpectingEncryptionKey ) )
			{
				// server accepted our connection request
				FullConnect( packet->from, nEncryptionKeyIndex );
			}
		}
		break;
		
	case S2C_CHALLENGE:		
		// Response from getchallenge we sent to the server we are connecting to

		if ( packet->from.IsLocalhost() || packet->from.IsLoopback() || !cl_failremoteconnections.GetBool() )
		{
			DeferredConnection_t &dc = m_DeferredConnection;

			dc.m_bActive = false;
			dc.m_adrServerAddress = packet->from;
			dc.m_nChallenge = msg.ReadLong();
			dc.m_nAuthprotocol = msg.ReadLong();
			dc.m_unGSSteamID = 0;
			dc.m_bGSSecure = false;
			if ( dc.m_nAuthprotocol == PROTOCOL_STEAM )
			{
				if ( msg.ReadShort() != 0 )
				{
					Msg("Invalid Steam key size.\n");
					Disconnect();
					return false;
				}
				dc.m_unGSSteamID = msg.ReadLongLong();
				dc.m_bGSSecure = msg.ReadByte() ? true : false;
			}
			else
			{
				msg.ReadShort();
				dc.m_unGSSteamID = msg.ReadLongLong();	// still read out game server SteamID for token validation
				msg.ReadByte();
			}

			if ( msg.IsOverflowed() )
			{
				Msg( "Invalid challenge packet.\n" );
				Disconnect();
				return false;
			}

			// The host can disable access to secure servers if you load unsigned code (mods, plugins, hacks)
			if ( dc.m_bGSSecure && !Host_IsSecureServerAllowed() )
			{
				m_netadrReserveServer.RemoveAll();
				m_nServerReservationCookie = 0;				
				m_pServerReservationCallback = NULL;
#if !defined(DEDICATED)
				g_pMatchFramework->CloseSession();
				g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnClientInsecureBlocked", "reason", "connect" ) );
#endif
				Disconnect();
				return false;
			}	

			char context[ 256 ] = { 0 };
			msg.ReadString( context, sizeof( context ) );

			if ( StringHasPrefix( context, "reserve" ) )
			{
				HandleReserveServerChallengeResponse( m_DeferredConnection.m_nChallenge );
			}
			else if ( StringHasPrefix( context, "connect" ) )
			{
				// Blow it off if we are not connected.
				if ( m_nSignonState != SIGNONSTATE_CHALLENGE )
				{
					return false;
				}

				dc.m_bActive = true;

				int nProto = msg.ReadLong();
				m_nServerProtocolVersion = nProto;
 				if ( nProto > GetHostVersion() ) // server is running newer version
				{
					Msg( "Server is running a newer version, client version %d, server version %d\n", GetHostVersion(), nProto );
					Disconnect();
					return false;
				}
				if ( nProto < GetHostVersion() ) // server is running older version
				{
					Msg( "Server is running an older version, client version %d, server version %d\n", GetHostVersion(), nProto );
					Disconnect();
					return false;
				}

				msg.ReadString( dc.m_chLobbyType, ARRAYSIZE( dc.m_chLobbyType ) - 1 );
				dc.m_bRequiresPassword = ( msg.ReadByte() != 0 );
				dc.m_unLobbyID = msg.ReadLongLong();
				dc.m_bDCFriendsReqd =  (msg.ReadByte() != 0);
				dc.m_bOfficialValveServer = ( msg.ReadByte() != 0 );

				// Generate an encryption key for this challenge
				bool bEncryptedChannel = ( msg.ReadByte() != 0 );
				if ( bEncryptedChannel )
				{
					byte chKeyPub[1024] = {};
					byte chKeySgn[1024] = {};
					int cbKeyPub = msg.ReadLong();
					msg.ReadBytes( chKeyPub, cbKeyPub );
					int cbKeySgn = msg.ReadLong();
					msg.ReadBytes( chKeySgn, cbKeySgn );
					if ( msg.IsOverflowed() )
					{
						Msg( "Invalid challenge packet.\n" );
						Disconnect();
						return false;
					}

					// Verify server certificate signature
					byte *pbAllocatedKey = NULL;
					int nAllocatedCryptoBlockSize = 0;
					if ( !NET_CryptVerifyServerCertificateAndAllocateSessionKey( dc.m_bOfficialValveServer, dc.m_adrServerAddress,
						chKeyPub, cbKeyPub, chKeySgn, cbKeySgn,
						&pbAllocatedKey, &nAllocatedCryptoBlockSize ) || !pbAllocatedKey || !nAllocatedCryptoBlockSize )
					{
						delete [] pbAllocatedKey;
						Msg( "Bad challenge signature.\n" );
						Disconnect();
						return false;
					}

					static int s_nGeneratedEncryptionKey = 0;
					++s_nGeneratedEncryptionKey;
					if ( !s_nGeneratedEncryptionKey )
						++ s_nGeneratedEncryptionKey;

					m_mapGeneratedEncryptionKeys.InsertOrReplace( s_nGeneratedEncryptionKey, pbAllocatedKey );
					dc.m_nEncryptionKey = s_nGeneratedEncryptionKey;
					dc.m_nEncryptedSize = nAllocatedCryptoBlockSize;
				}
				else
				{
					dc.m_nEncryptionKey = 0;
					dc.m_nEncryptedSize = 0;
				}
				
				Msg( "Server using '%s' lobbies, requiring pw %s, lobby id %llx\n",
					dc.m_chLobbyType[0] ? dc.m_chLobbyType : "<none>",
					dc.m_bRequiresPassword ? "yes" : "no",
					dc.m_unLobbyID );

				#if ENGINE_CONNECT_VIA_MMS
				// Check if server is denying dc. If it sent us a -1 then we have to use our own reservation id.
				// This will work if we joined the right lobby, otherwise it will fail
				if ( dc.m_unLobbyID == (uint64)(-1) )
				{
					// GSidhu - Detect the case when we are trying to direct connect - ensure the reservation
					// cookie we are holding is reset to 0 between session

					//if ( m_nServerReservationCookie == 0 )
					//{
					//	COM_ExplainDisconnection( true, "Connecting to a Competitive mode game on a Valve CS:GO server is not allowed, use matchmaking instead.\n" );
					//	Disconnect();
					//	break;
					//}
					// else
					{
						dc.m_unLobbyID = m_nServerReservationCookie;
					}
				}

				RememberIPAddressForLobby( dc.m_unLobbyID, dc.m_adrServerAddress );
				#endif

				if ( !dc.m_chLobbyType[0] && !dc.m_unLobbyID && dc.m_bRequiresPassword &&
					( !password.GetString()[ 0 ] ) )
				{
					// Stop resending challenges while PW dialog is up
					m_bWaitingForPassword = true;
					// Show PW UI with current string
#ifndef DEDICATED
					SCR_EndLoadingPlaque();
					EngineVGui()->ShowPasswordUI( password.GetString() );
#endif
				}
				else if ( dc.m_chLobbyType[0] && !sv.IsActive() && !dc.m_unLobbyID &&
					m_nServerReservationCookie )
				{
					// Server protocol violation - client has reserved this server, but server
					// replies that it requires lobbies and doesn't yet have a lobby ID
					Warning( "Server error - failed to handle reservation request.\n" );
					Disconnect();
					return false;
				}
				else if ( StringHasPrefix( context, "connect-retry" ) &&
						  dc.m_chLobbyType[0] && !sv.IsActive() && !dc.m_unLobbyID )
						  // Server tells us that we need to issue another "connect" with a valid
						  // challenge, then it will reserve itself for a brief period to
						  // let us create the required lobby
				{
					Msg( "Grace request retry for unreserved server...\n" );
					CheckForResend( true );	// force a resend with the correct challenge nr
				}
				else if ( StringHasPrefix( context, "connect-matchmaking-only" ) )
					// This response is sent by Valve CS:GO servers - we cannot 
					// direct-connect and need to go via matchmaking instead
				{
					COM_ExplainDisconnection( true, "You must use matchmaking to connect to this CS:GO server.\n" );
					Disconnect();
					break;
				}
				else if ( StringHasPrefix( context, "connect-lan-only" ) )
					// This response is sent by anonymous community servers - we cannot 
					// direct-connect unless we are on the same LAN network
				{
					COM_ExplainDisconnection( true, "You cannot connect to this CS:GO server because it is restricted to LAN connections only.\n" );
					Disconnect();
					break;
				}
				else if ( !StringHasPrefix( context, "connect-granted" ) &&
						  dc.m_chLobbyType[0] && !sv.IsActive() && !dc.m_unLobbyID )
						  // Server requires lobbies, but is unreserved at the moment, so
						  // we should keep waiting for "connect-granted" response before we
						  // proceed and create a lobby
				{
					Msg( "Server did not approve grace request, retrying...\n" );
					CheckForResend( true );	// force a resend with the correct challenge nr
				}
				else
				{
					if ( dc.m_chLobbyType[0] && !sv.IsActive() && !dc.m_unLobbyID )
					{
						Msg( "Server approved grace request...\n" );
					}

					HandleDeferredConnection();
				}

			}
		}
		break;

	case A2A_PRINT:			
		if ( msg.ReadString( string, sizeof(string) ) )
		{
			ConMsg ( "%s\n", string );
		}
		break;

	case S2C_CONNREJECT:	
		if ( m_nSignonState == SIGNONSTATE_CHALLENGE )
		{
			msg.ReadString( string, sizeof(string) );
			// Check if the connection is rejected with a redirect address
			if ( char const *szRedirectAddress = StringAfterPrefix( string, "ConnectRedirectAddress:" ) )
			{
				m_Remote.RemoveAll();
				m_Remote.AddRemote( szRedirectAddress, "public" );

				// For the check for resend timer to fire a connection / getchallenge request.
				SetSignonState( SIGNONSTATE_CHALLENGE, -1, NULL );

				// Force connection request to fire.
				m_flConnectTime = -FLT_MAX;

				m_nRetryNumber = 0;

				// Retry for up to timeout seconds
				m_nRetryMax = cl_resend_timeout.GetFloat() / cl_resend.GetFloat();

				break;
			}
			// Force failure dialog to come up now.
			COM_ExplainDisconnection( true, "%s", string );
			Disconnect();
			// Host_Disconnect();
		}
		break;

	case A2A_PING:			
		NET_OutOfBandPrintf( m_Socket, packet->from, "%c00000000000000", A2A_ACK );
		break;

	case A2A_ACK:			
		ConMsg ("A2A_ACK from %s\n", ns_address_render( packet->from ).String() );

#if defined( _GAMECONSOLE )
		// skip \r\n
		msg.ReadByte();
		msg.ReadByte();

		const void *pvData = msg.GetBasePointer() + ( msg.GetNumBitsRead() >> 3 );
		int numBytes = msg.GetNumBytesLeft();

		KeyValues *notify = new KeyValues( "A2A_ACK" );
		notify->SetPtr( "ptr", const_cast< void * >( pvData ) );
		notify->SetInt( "size", numBytes );

		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( notify );
#endif
		break;

	case A2A_CUSTOM:
		break;	// TODO fire local game event

	case S2A_RESERVE_RESPONSE: 
		if ( msg.ReadLong() == GetHostVersion() )
		{
			ReservationResponseReply_t reply;
			reply.m_adrFrom = packet->from;
			reply.m_uiResponse = msg.ReadByte();
			reply.m_bValveDS = msg.ReadOneBit() ? true : false;
			reply.m_numGameSlots = msg.ReadLong();
			HandleReservationResponse( reply );
		}
		break;

	case S2A_RESERVE_CHECK_RESPONSE:
		{
			int32 hostVersion = msg.ReadLong();
			uint32 token = msg.ReadLong();
			for ( int i = 0; i < m_arrSvReservationCheck.Count(); ++ i )
			{
				CServerMsg_CheckReservation *pSv = m_arrSvReservationCheck[ i ];
				if ( pSv && pSv->m_serverAdr.CompareAdr( packet->from ) )
				{
					pSv->ResponseReceived( packet->from, msg, hostVersion, token );
				}
			}
		}

		break;

	case S2A_PING_RESPONSE:
		{
			int32 hostVersion = msg.ReadLong();
			uint32 token = msg.ReadLong();
			for ( int iSvPing = 0; iSvPing < m_arrSvPing.Count(); ++ iSvPing )
			{
				CServerMsg_Ping *pSvPing = m_arrSvPing[ iSvPing ];
				if ( pSvPing && pSvPing->m_serverAdr.CompareAdr( packet->from ) )
				{
					pSvPing->ResponseReceived( packet->from, msg, hostVersion, token );
				}
			}
		}

		break;

#ifndef DEDICATED
	case 0:
		{
			// Feed into matchmaking
			packet->message = msgOriginal;
			KeyValues *notify = new KeyValues( "OnNetLanConnectionlessPacket" );
			notify->SetPtr( "rawpkt", packet );

			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( notify );
		}
		return true;
#endif

#if defined( _GAMECONSOLE )
	case M2A_SERVER_BATCH:
		{
			// skip \n
			msg.ReadByte();

			const void *pvData = msg.GetBasePointer() + ( msg.GetNumBitsRead() >> 3 );
			int numBytes = msg.GetNumBytesLeft();

			KeyValues *notify = new KeyValues( "M2A_SERVER_BATCH" );
			notify->SetPtr( "ptr", const_cast< void * >( pvData ) );
			notify->SetInt( "size", numBytes );

			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( notify );
		}
		break;

	case A2A_KV_CMD:
		{
			int nVersion = msg.ReadByte();
			if ( nVersion == A2A_KV_VERSION )
			{
				int nHeader = msg.ReadLong();
				int nReplyId = msg.ReadLong();
				int nChallenge = msg.ReadLong();
				int nExtra = msg.ReadLong();
				int numBytes = msg.ReadLong();

				KeyValues *kvData = NULL;
				if ( numBytes > 0 && numBytes <= MAX_ROUTABLE_PAYLOAD )
				{
					void *pvBytes = stackalloc( numBytes );
					if ( msg.ReadBytes( pvBytes, numBytes ) )
					{
						kvData = new KeyValues( "" );
						CUtlBuffer buf( pvBytes, numBytes, CUtlBuffer::READ_ONLY );
						buf.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
						if ( !kvData->ReadAsBinary( buf ) )
						{
							kvData->deleteThis();
							kvData = NULL;
						}
					}
				}

				KeyValues *notify = new KeyValues( "A2A_KV_CMD" );
				if ( kvData )
					notify->AddSubKey( kvData );

				notify->SetInt( "version", nVersion );
				notify->SetInt( "header", nHeader );
				notify->SetInt( "replyid", nReplyId );
				notify->SetInt( "challenge", nChallenge );
				notify->SetInt( "extra", nExtra );
				notify->SetInt( "size", numBytes );

				g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( notify );
			}
		}
		break;
#endif

	case S2A_INFO_SRC:
		{
			// Handle pingserver response
			Read_S2A_INFO_SRC( packet->from, &msg );
		}
		return true;
	// Unknown?
	default:			
		// Otherwise, don't do anything.
		ConDMsg( "Bad connectionless packet ( CL '%c') from %s.\n", c, ns_address_render( packet->from ).String() );
		return false;
	}

	return true;
}

void CBaseClientState::OnEvent( KeyValues *pEvent )
{
#if ENGINE_CONNECT_VIA_MMS
	char const *szEvent = pEvent->GetName();
	
	if ( !Q_stricmp( "OnNetLanConnectionlessPacket", szEvent ) )
	{
		if ( KeyValues *pGameDetailsServer = pEvent->FindKey( "GameDetailsServer" ) )
		{
			char const *szDetailsAdr = pGameDetailsServer->GetString( "ConnectServerDetailsRequest/server" );
			if ( !m_bWaitingForServerGameDetails ||
				 !szDetailsAdr || !*szDetailsAdr )
				return;

			int idxRemoteReconnect = -1;
			for ( int i = 0; i < m_Remote.Count(); ++i )
			{
				const netadr_t &adr = m_Remote.Get( i ).m_adrRemote;
				if ( !Q_stricmp( adr.ToString(), szDetailsAdr ) )
				{
					idxRemoteReconnect = i;
					break;
				}
			}

			if ( idxRemoteReconnect >= 0 )
			{
				// This is our direct connect probe response
				m_bWaitingForServerGameDetails = false;
				Msg( "Received game details information from %s...\n",
					m_Remote.Get( idxRemoteReconnect ).m_szRetryAddress.String() );
				Disconnect( false );	// disconnect the current attempt, will retry with reservation

				//
				// Prepare the settings
				//
				pGameDetailsServer->SetName( "settings" );

				if ( KeyValues *kvLanSearch = pGameDetailsServer->FindKey( "ConnectServerDetailsRequest" ) )
				{
					// LanSearch is not needed for the sessions
					pGameDetailsServer->RemoveSubKey( kvLanSearch );
					kvLanSearch->deleteThis();
				}

				// Add the bypass lobby flag
				KeyValues *optionsKey = pGameDetailsServer->FindKey( "options", true);
				optionsKey->SetInt( "bypasslobby", 1 );

				Disconnect( false );
				g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
					"OnEngineLevelLoadingSession", "reason", "CreateSession" ) );

				g_pMatchFramework->CreateSession( pGameDetailsServer );
			}

		}
	}
#endif
}

void CBaseClientState::SetConnectionPassword( char const *pchCurrentPW )
{
	if ( !pchCurrentPW || !*pchCurrentPW )
	{
		m_bWaitingForPassword = false;
		m_bEnteredPassword = false;

		Msg( "Connection to %s failed, server requires a password\n", ns_address_render( m_DeferredConnection.m_adrServerAddress ).String() );
		Disconnect();
		return;
	}

#ifndef DEDICATED
	SCR_BeginLoadingPlaque();
#endif

	m_bWaitingForPassword = false;
	m_bEnteredPassword = true;
	password.SetValue( pchCurrentPW );
	HandleDeferredConnection();
}

void CBaseClientState::RememberIPAddressForLobby( uint64 unLobbyID, const ns_address &adrRemote )
{
	ConColorMsg( Color( 0, 255, 0, 255 ), "RememberIPAddressForLobby: lobby %llx from address %s\n", unLobbyID, ( cl_hideserverip.GetInt()>0 && adrRemote.IsType<netadr_t>() ) ? "<ip hidden>" : ns_address_render( adrRemote ).String() );

	m_DirectConnectLobby.m_unLobbyID = unLobbyID;
	m_DirectConnectLobby.m_adrRemote = adrRemote;
	// Keep valid for 1 minute
	m_DirectConnectLobby.m_flEndTime = realtime + 60.0f;
}

void CBaseClientState::HandleDeferredConnection()
{
	DeferredConnection_t &dc = m_DeferredConnection;
	if ( !dc.m_bActive )
		return;
	dc.m_bActive = false;

#ifndef DEDICATED
	// If we started a listen server then we should connect no matter what
	if ( ( dc.m_chLobbyType[0] || dc.m_unLobbyID ) && !sv.IsActive() )
	{
#if ENGINE_CONNECT_VIA_MMS
		if ( dc.m_unLobbyID )
		{
			IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
			KeyValues *pSessionSysData = pMatchSession ? pMatchSession->GetSessionSystemData() : NULL;
			uint64 xuidSessionReservation = pSessionSysData ? pSessionSysData->GetUint64( "xuidReserve" ) : 0ull;
			if ( dc.m_bOfficialValveServer || ( xuidSessionReservation == dc.m_unLobbyID ) )	// Force the connection to official server, they aren't direct-connectable otherwise
			{
				SendConnectPacket ( dc.m_adrServerAddress, dc.m_nChallenge, dc.m_nAuthprotocol, dc.m_unGSSteamID, dc.m_bGSSecure );
			}
			else
			{
				KeyValues *pSettings = new KeyValues( "settings" );
				KeyValues::AutoDelete autodelete( pSettings );
				pSettings->SetString( "system/network", "LIVE" );
				pSettings->SetString( "options/action", "joinsession" );
				pSettings->SetUint64( "options/sessionid", dc.m_unLobbyID );
				pSettings->SetBool( "options/dcFriendsRed", dc.m_bDCFriendsReqd );
				if ( !dc.m_bOfficialValveServer )
					pSettings->SetInt( "game/hosted", 1 );
				pSettings->SetString( "options/server", dc.m_bOfficialValveServer ? "official" : "dedicated" );

				Disconnect( true );
				g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
					"OnEngineLevelLoadingSession", "reason", "MatchSession" ) );
				g_pMatchFramework->MatchSession( pSettings );
			}
		}
		else
		{
			// Server has no lobby but needs one, we need to enter through the UI.
			Msg( "Retrying connection to %s, server requires lobby reservation but is unreserved.\n", dc.m_adrServerAddress.ToString() );

			// Prevent resending the challenges to the server
			m_bWaitingForServerGameDetails = true;
			ResendGameDetailsRequest( dc.m_adrServerAddress );
		}
#else
		bool bCanSendConnectPacketRightNow = false;
		
		// Always allow to connect if we have a reservation in m_nServerReservationCookie
		if ( m_nServerReservationCookie )
			bCanSendConnectPacketRightNow = true;

		// Always allow to connect to listen server peer SteamID
		if ( m_ListenServerSteamID )
			bCanSendConnectPacketRightNow = true;

		// Always allow to connect from dedicated or ValveDS or GOTV relay
		if ( IsClientStateTv() || NET_IsDedicated() || sv.IsDedicated() || ( serverGameDLL && serverGameDLL->IsValveDS() ) )
			bCanSendConnectPacketRightNow = true;

		if ( !bCanSendConnectPacketRightNow && (
			!dc.m_unGSSteamID ||														// Connecting to GOTV relay
			( CSteamID( dc.m_unGSSteamID ).GetEAccountType() == k_EAccountTypeInvalid )	// Connecting to sv_lan 1 server
			) )
		{
			static const bool s_bAllowLanWhitelist = !CommandLine()->FindParm( "-ignorelanwhitelist" );
			bCanSendConnectPacketRightNow = // only allow for LAN server setup to bypass GC auth
				dc.m_adrServerAddress.IsLocalhost() || dc.m_adrServerAddress.IsLoopback()		// localhost/loopback
				|| ( s_bAllowLanWhitelist && dc.m_adrServerAddress.IsReservedAdr() )			// LAN RFC 1918
				;
		}
		
		// If we determined that client is good to go then just follow up with a real connect packet
		if ( bCanSendConnectPacketRightNow )
		{
			SendConnectPacket( dc.m_adrServerAddress, dc.m_nChallenge, dc.m_nAuthprotocol, dc.m_unGSSteamID, dc.m_bGSSecure );
			return;
		}

		// Check that if we are falling through we have a good game server Steam ID to ask GC about
		if ( !dc.m_unGSSteamID || !CSteamID( dc.m_unGSSteamID ).BGameServerAccount() )
		{
			Disconnect( true ); // cannot retry this attempt - the GS SteamID is not good
			COM_ExplainDisconnection( true, "You cannot connect to this CS:GO server because it is restricted to LAN connections only.\n" );
			return;
		}
		
		//
		// Otherwise we require that client obtained a GS cookie from GC
		// which allows GC to deny connections to blacklisted game servers
		//
		uint64 uiReservationCookie = 0ull;
		{
			KeyValues *kvCreateSession = new KeyValues( "OnEngineLevelLoadingSession" );
			kvCreateSession->SetString( "reason", "CreateSession" );
			kvCreateSession->SetString( "adr", ns_address_render( dc.m_adrServerAddress ).String() );
			kvCreateSession->SetUint64( "gsid", dc.m_unGSSteamID );
			kvCreateSession->SetPtr( "ptr", &uiReservationCookie );
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvCreateSession );
		}

		if ( !uiReservationCookie )
		{
			Disconnect( true );	// disconnect the current attempt, will retry with GC reservation
			{
				KeyValues *kvCreateSession = new KeyValues( "OnEngineLevelLoadingSession" );
				kvCreateSession->SetString( "reason", "CreateSession" );
				kvCreateSession->SetString( "adr", ns_address_render( dc.m_adrServerAddress ).String() );
				kvCreateSession->SetUint64( "gsid", dc.m_unGSSteamID );
				// NO PTR HERE, FORCE COOKIE: kvCreateSession->SetPtr( "ptr", &uiReservationCookie );
				g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvCreateSession );
			}
		}
		else
			SendConnectPacket( dc.m_adrServerAddress, dc.m_nChallenge, dc.m_nAuthprotocol, dc.m_unGSSteamID, dc.m_bGSSecure );
#endif
	}
	else
#endif
	{
		SendConnectPacket ( dc.m_adrServerAddress, dc.m_nChallenge, dc.m_nAuthprotocol, dc.m_unGSSteamID, dc.m_bGSSecure );
	}
}


bool CBaseClientState::NETMsg_Tick( const CNETMsg_Tick& msg )
{
	VPROF( "ProcessTick" );

	m_NetChannel->SetRemoteFramerate(
		CNETMsg_Tick_t::FrametimeToFloat( msg.host_computationtime() ),
		CNETMsg_Tick_t::FrametimeToFloat( msg.host_computationtime_std_deviation() ),
		CNETMsg_Tick_t::FrametimeToFloat( msg.host_framestarttime_std_deviation() ) );

	// Note: CClientState separates the client and server clock states and drifts
	// the client's clock to match the server's, but right here, we keep the two clocks in sync.
	SetClientTickCount( msg.tick() );
	SetServerTickCount( msg.tick() );

	if ( m_StringTableContainer )
	{
		m_StringTableContainer->SetTick( GetServerTickCount() );
	}
	return true;// (GetServerTickCount()>0);
}

void CBaseClientState::SendStringCmd(const char * command)
{
	if ( m_NetChannel) 
	{
		CNETMsg_StringCmd_t stringCmd( command );
		m_NetChannel->SendNetMsg( stringCmd );

		if ( strstr( command, "disconnect" ) )
		{
			// When client requests a disconnect this is last moment to
			// push any data into secure network, forcefully transmit netchannel
			m_NetChannel->Transmit();
		}
	}
}

bool CBaseClientState::NETMsg_StringCmd( const CNETMsg_StringCmd& msg )
{
	VPROF( "ProcessStringCmd" );

	return InternalProcessStringCmd( msg );
}


bool CBaseClientState::InternalProcessStringCmd( const CNETMsg_StringCmd& msg )
{
	const char *command = msg.command().c_str();

	// Don't restrict commands from the server in single player or if cl_restrict_stuffed_commands is 0.
	if ( !m_bRestrictServerCommands || sv.IsActive() )
	{
		Cbuf_AddText ( Cbuf_GetCurrentPlayer(), command, kCommandSrcCode ); // FIXME: Should this be kCommandSrcNetServer and we push the m_bRestrictServerCommands into the server check?
		return true;
	}

	CCommand args;
	args.Tokenize( command );

	if ( args.ArgC() <= 0 )
		return true;

	// Run the command, but make sure the command parser knows to only execute commands marked with FCVAR_SERVER_CAN_EXECUTE.
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), command, kCommandSrcNetServer );

	return true;
}


#ifndef DEDICATED
class CScaleformAvatarImageProviderImpl : public IScaleformAvatarImageProvider
{
public:
	// Scaleform low-level image needs rgba bits of the inventory image (if it's ready)
	virtual bool GetImageInfo( uint64 xuid, ImageInfo_t *pImageInfo ) OVERRIDE
	{
		CSteamID steamID( xuid );
		if ( !steamID.IsValid() || !steamID.BIndividualAccount() || !steamID.GetAccountID() )
			return false;

		if ( !GetBaseLocalClient().IsConnected() )
			return false;

		// Find the player with the given account ID
		CBaseClientState::PlayerAvatarDataMap_t const &data = GetBaseLocalClient().m_mapPlayerAvatarData;
		CBaseClientState::PlayerAvatarDataMap_t::IndexType_t const idxData = data.Find( steamID.GetAccountID() );
		if ( idxData == data.InvalidIndex() )
			return false;

		const CNETMsg_PlayerAvatarData& msg = *data.Element( idxData );
		pImageInfo->m_cbImageData = msg.rgb().size();
		pImageInfo->m_pvImageData = msg.rgb().data();
		return true;
	}
} g_CScaleformAvatarImageProviderImpl;
#endif

bool CBaseClientState::NETMsg_PlayerAvatarData( const CNETMsg_PlayerAvatarData& msg )
{
	PlayerAvatarDataMap_t::IndexType_t idxData = m_mapPlayerAvatarData.Find( msg.accountid() );
	if ( idxData != m_mapPlayerAvatarData.InvalidIndex() )
	{
		delete m_mapPlayerAvatarData.Element( idxData );
		m_mapPlayerAvatarData.RemoveAt( idxData );
	}

	CNETMsg_PlayerAvatarData_t *pClientDataCopy = new CNETMsg_PlayerAvatarData_t;
	pClientDataCopy->CopyFrom( msg );
	m_mapPlayerAvatarData.Insert( pClientDataCopy->accountid(), pClientDataCopy );

#ifndef DEDICATED
	if ( g_pScaleformUI )
		g_pScaleformUI->AvatarImageReload( uint64( pClientDataCopy->accountid() ), &g_CScaleformAvatarImageProviderImpl );
#endif

	return true;
}

CNETMsg_PlayerAvatarData_t * CBaseClientState::AllocOwnPlayerAvatarData() const
{
#ifndef DEDICATED
	// If the game server is not GOTV then upload our own avatar data
	extern ConVar sv_reliableavatardata;
	if ( ( this == &GetBaseLocalClient() )
		&& !GetBaseLocalClient().ishltv
		&& m_NetChannel && IsConnected()
		&& sv_reliableavatardata.GetBool() )
	{
		CSteamID steamID = Steam3Client().SteamUser()->GetSteamID();
		int iAvatar = Steam3Client().SteamFriends()->GetMediumFriendAvatar( steamID );
		if ( ( iAvatar != -1 ) && ( iAvatar != 0 ) )
		{
			uint32 wide = 0, tall = 0;
			if ( Steam3Client().SteamUtils()->GetImageSize( iAvatar, &wide, &tall ) && ( wide == 64 ) && ( tall == 64 ) )
			{
				CUtlVector< byte > memAvatarData;
				memAvatarData.SetCount( 64 * 64 * 4 );
				memset( memAvatarData.Base(), 0xFF, memAvatarData.Count() );

				Steam3Client().SteamUtils()->GetImageRGBA( iAvatar, memAvatarData.Base(), memAvatarData.Count() );
				// trim alpha for size
				for ( int y = 0; y < 64; ++y ) for ( int x = 0; x < 64; ++x )
				{
					V_memmove( memAvatarData.Base() + y * 64 * 3 + x * 3, memAvatarData.Base() + y * 64 * 4 + x * 4, 3 );
				}

				return new CNETMsg_PlayerAvatarData_t( steamID.GetAccountID(), memAvatarData.Base(), 64 * 64 * 3 );
			}
		}

		// If we got here then we failed to obtain our own medium avatar, ping Steam rack and hope for an avatar changed callback
		if ( !Steam3Client().SteamFriends()->RequestUserInformation( steamID, false ) )
		{
			static bool s_bReentrantGuard = false;
			if ( !s_bReentrantGuard )
			{	// Allow one retry here
				s_bReentrantGuard = true;
				return AllocOwnPlayerAvatarData();
				s_bReentrantGuard = false;
			}
		}
	}
#endif
	return NULL;
}

bool CBaseClientState::NETMsg_SetConVar( const CNETMsg_SetConVar& msg )
{
	VPROF( "ProcessSetConVar" );

	// Never process on local client, since the ConVar is directly linked here
	if ( m_NetChannel->IsLoopback() )
		return true;

	for ( int i=0; i<msg.convars().cvars_size(); i++ )
	{
		const char *name = NetMsgGetCVarUsingDictionary( msg.convars().cvars(i) );
		const char *value = msg.convars().cvars(i).value().c_str();

		// De-constify
		ConVarRef var( name );

		if ( !var.IsValid() )
		{
			ConMsg( "SetConVar: No such cvar ( %s set to %s), skipping\n",
				name, value );
			continue; 
		}

		// Make sure server is only setting replicated game ConVars
		if ( !var.IsFlagSet( FCVAR_REPLICATED ) )
		{
			ConMsg( "SetConVar: Can't set server cvar %s to %s, not marked as FCVAR_REPLICATED on client\n",
				name, value );
			continue;
		}

		// Set value directly ( don't call through cv->DirectSet!!! )
		if ( !sv.IsActive() )
		{
			var.SetValue( value );
			DevMsg( "SetConVar: %s = \"%s\"\n", name, value );
		}
	}

	return true;
}

bool CBaseClientState::NETMsg_SignonState( const CNETMsg_SignonState& msg )
{
	VPROF( "ProcessSignonState" );

	return SetSignonState( msg.signon_state(), msg.spawn_count(), &msg ) ;	
}

bool CBaseClientState::SVCMsg_Print( const CSVCMsg_Print& msg )
{
	VPROF( "SVCMsg_Print" );

	ConMsg( "%s", msg.text().c_str() );
	return true;
}

bool CBaseClientState::SVCMsg_Menu( const CSVCMsg_Menu& msg )
{
	VPROF( "SVCMsg_Menu" );

#if !defined(DEDICATED)
	PluginHelpers_Menu( msg );	
#endif
	return true;
}

bool CBaseClientState::SVCMsg_ServerInfo( const CSVCMsg_ServerInfo& msg )
{
	VPROF( "SVCMsg_ServerInfo" );

#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_PROCESSSERVERINFO);
#endif

	COM_TimestampedLog( " CBaseClient::SVCMsg_ServerInfo" );
	
	if ( msg.protocol() != GetHostVersion() )
	{
#if !defined( DEDICATED )
		if ( demoplayer && demoplayer->IsPlayingBack() )
		{
			ConMsg ( "WARNING: Server demo version %i, client version %i.\n", msg.protocol(), GetHostVersion() );
		}
		else
#endif
		{
			ConMsg ( "WARNING: Server version %i, client version %i.\n", msg.protocol(), GetHostVersion() );
		}
	}

	// Parse servercount (i.e., # of servers spawned since server .exe started)
	// So that we can detect new server startup during download, etc.
	m_nServerCount = msg.server_count();

	m_nMaxClients = msg.max_clients();

	m_nServerClasses = msg.max_classes();
	m_nServerClassBits = Q_log2( m_nServerClasses ) + 1;
	
	if ( m_nMaxClients < 1 || m_nMaxClients > ABSOLUTE_PLAYER_LIMIT )
	{
		ConMsg ("Bad maxclients (%u) from server.\n", m_nMaxClients);
		return false;
	}

	if ( m_nServerClasses < 1 || m_nServerClasses > MAX_SERVER_CLASSES )
	{
		ConMsg ("Bad maxclasses (%u) from server.\n", m_nServerClasses);
		return false;
	}

#ifndef DEDICATED
	if ( !sv.IsActive() && 
		 ( s_ClientBroadcastPlayer.IsPlayingBack() ||  // /*( demoplayer && demoplayer->IsPlayingBack() )*/
			!( m_NetChannel->IsLoopback() || m_NetChannel->IsNull() || m_NetChannel->GetRemoteAddress().IsLocalhost() ) 
		 )
	)
	{
		// reset server enforced cvars
		g_pCVar->RevertFlaggedConVars( FCVAR_REPLICATED );	

		extern void RevertAllModifiedLocalState();
		RevertAllModifiedLocalState();
	}
#endif

	// clear all baselines still around from last game
	FreeEntityBaselines();

	// force changed flag to being reset
	g_GameEventManager.HasClientListenersChanged( true );
	
#ifndef DEDICATED
	splitscreen->AddBaseUser( 0, msg.player_slot() + 1 );

#if defined( INCLUDE_SCALEFORM )
	if ( g_pScaleformUI )
	{
		extern IScaleformSlotInitController *g_pIScaleformSlotInitControllerEngineImpl;
		g_pScaleformUI->InitSlot( SF_SS_SLOT( 0 ), g_szDefaultScaleformClientMovieName, g_pIScaleformSlotInitControllerEngineImpl );
	}
#endif

#endif
	m_nPlayerSlot = msg.player_slot();
	m_nViewEntity = msg.player_slot() + 1; 
	
	if ( msg.tick_interval() < MINIMUM_TICK_INTERVAL ||
		 msg.tick_interval() > MAXIMUM_TICK_INTERVAL )
	{
		ConMsg ("Interval_per_tick %f out of range [%f to %f]\n",
			msg.tick_interval(), MINIMUM_TICK_INTERVAL, MAXIMUM_TICK_INTERVAL );
		return false;
	}
	
	if ( !COM_CheckGameDirectory( msg.game_dir().c_str() ) )
	{
		return false;
	}

	Q_snprintf( m_szLevelName, sizeof( m_szLevelName ), "maps/%s%s.bsp", msg.map_name().c_str(), GetPlatformExt() );
	Q_FixSlashes( m_szLevelName );
	Q_strncpy( m_szLevelNameShort, msg.map_name().c_str(), sizeof( m_szLevelNameShort ) );
	Q_strncpy( m_szMapGroupName, msg.map_group_name().c_str(), sizeof( m_szMapGroupName ) );
	m_unUGCMapFileID = msg.ugc_map_id();

#if !defined(DEDICATED)
	EngineVGui()->SetProgressLevelName( m_szLevelNameShort );
	audiosourcecache->LevelInit( m_szLevelNameShort );
#endif

	ConVarRef skyname( "sv_skyname" );
	if ( skyname.IsValid() )
	{
		skyname.SetValue( msg.sky_name().c_str() );
	}

	m_nDeltaTick = -1;	// no valid snapshot for this game yet
	
	// fire a client side event about server data
	IGameEvent *pEvent = g_GameEventManager.CreateEvent( "server_spawn" );
	if ( pEvent )
	{
		pEvent->SetString( "hostname", msg.host_name().c_str() );

		ns_address adr = m_NetChannel->GetRemoteAddress();
		if ( adr.IsType<netadr_t>() )
		{
			pEvent->SetString( "address", CUtlNetAdrRender( adr.AsType<netadr_t>(), true ).String() );
			pEvent->SetInt(    "port", adr.GetPort() );
		}
		else
		{
			pEvent->SetString( "address", ns_address_render( adr ).String() );
		}

		pEvent->SetString( "game", msg.game_dir().c_str() );
		pEvent->SetString( "mapname", msg.map_name().c_str() );
		pEvent->SetInt(    "maxplayers", msg.max_clients() );
		pEvent->SetInt(	   "password", 0 );				// TODO
		pEvent->SetString( "os", Q_strupr( va("%c", msg.c_os() ) ) );
		pEvent->SetBool(    "dedicated", msg.is_dedicated() );
		pEvent->SetBool(    "official", msg.is_official_valve_server() );
		if ( m_ulGameServerSteamID != 0 )
		{
			pEvent->SetString( "steamid", CSteamID( m_ulGameServerSteamID ).Render() );
		}


		g_GameEventManager.FireEventClientSide( pEvent );
	}

	// Verify that the client doesn't play on the server with mismatching version
	if ( m_nServerProtocolVersion && ( GetHostVersion() > m_nServerProtocolVersion ) )
	{
		if ( !msg.is_hltv() && !msg.is_redirecting_to_proxy_relay() )
		{
			// Newer client attempts to play on an older server which is not GOTV, bail here
			Warning( "Failed to connect to a gameserver, client version %d, server version %d\n", GetHostVersion(), m_nServerProtocolVersion );
			Disconnect();
			return false;
		}
	}

	// must be set BEFORE loading bsp which indicates above dependent code has completed
	m_bServerInfoProcessed = true;
	m_nServerInfoMsgProtocol = msg.protocol();

	if ( !sv.IsActive() && ( !msg.is_hltv() || !msg.is_redirecting_to_proxy_relay() ) )
	{
		// For a non-local connection, the bsp needs to be loaded first, BEFORE any server string table
		// callbacks occur.  These occur after this function and before the SIGNONSTATE_NEW,
		// causing unexpected out of order issues. The server material string table of precached
		// materials has bsp dependencies (i.e. due to cubemap patching) so bsp load must be first.
		// This also lets (fixes) the queued loader batch fast load the resources instead of the callbacks
		// loading them via the slower synchronous method.
		if ( IsGameConsole() && g_pQueuedLoader->IsMapLoading() )
		{
			Msg( "New CSVCMsg_ServerInfo message - loading map %s. Forcing current map load to end.\n", msg.map_name().c_str() );
			g_pQueuedLoader->EndMapLoading( true );
		}

		SV_CheckForFlushMemory( m_szLastLevelNameShort, m_szLevelNameShort );
		SV_FlushMemoryIfMarked();

		// A map is about to be loaded into memory
		HostState_Pre_LoadMapIntoMemory();

		// CSGO custom map detection
		bool bClientHasMap = true;

		bool bIsRelay = false;
		for ( CHltvServerIterator hltv; hltv; hltv.Next() )
		{
			if ( hltv->IsTVRelay() )
			{
				bIsRelay = true;
				break;
			}
		}
		if ( !bIsRelay ) // not a single one of the hltv servers is relay
		{
			char bspModelName[ MAX_PATH ];
			Q_snprintf( bspModelName, sizeof( bspModelName ), "maps/%s.bsp", msg.map_name().c_str() );

#if !defined( _GAMECONSOLE ) && !defined( DEDICATED )
			bool bCrcClientMapValid = false;
			CRC32_t crcClientMap;
			CRC32_Init( &crcClientMap );

			// Compute CRC of client map on disk
			{
				FileHandle_t mapfile = g_pFileSystem->OpenEx( bspModelName, "rb", IsGameConsole() ? FSOPEN_NEVERINPACK : 0, "GAME" );
				if ( mapfile != FILESYSTEM_INVALID_HANDLE )
				{
					g_pFileSystem->Close( mapfile );

					bCrcClientMapValid = CRC_MapFile( &crcClientMap, bspModelName );
				}
			}

			if ( demoplayer && demoplayer->IsPlayingBack() )
			{
				// We are playing a demo
				// Can we use the map on disk directly?
				if ( bCrcClientMapValid && msg.map_crc() && ( crcClientMap == msg.map_crc() ) )
				{
					// Seems like everything looks good on disk and we can proceed with map on disk
				}
				else
				{
					// We are trying to playback a demo, but CRC of the client map doesn't match
					// the CRC of the server map.
					// There are some known official maps that we can redirect into proper version
					// on Steam Workshop.
					char chMapName[ MAX_PATH ] = { 0 };
					V_sprintf_safe( chMapName, "%s", msg.map_name().c_str() );
					// Check if it's Valve official Workshop symbolic link
					if ( char const *szPastWorkshop = StringAfterPrefix( chMapName, "workshop/" ) )
					{
						//uint64 uiWkshpId = Q_atoui64( szPastWorkshop );
						bool bValveOfficialMap = false;
								/** Removed for partner depot **/
						if ( bValveOfficialMap )
						{
							if ( char const *szMapNameTrail = strchr( szPastWorkshop, '/' ) )
								Q_memmove( chMapName, szMapNameTrail + 1, chMapName + Q_ARRAYSIZE( chMapName ) - szMapNameTrail - 1 );
						}
					}
					// Now that workshop symlink has been resolved, verify whether
					// it is one of Valve official maps and redirect to Workshop based on known CRC
					uint64 uiKnownVersionWkshpId = 0;
					uint32 uiRepackedWkshpCrc = 0;
							/** Removed for partner depot **/
					extern ConVar debug_map_crc;
					if ( debug_map_crc.GetBool() && !uiKnownVersionWkshpId )
					{	// Force a debug error when debugging map CRC's
						Warning( "debug_map_crc: Map version mismatch for %s CRC=%u, no fallback version specified in csgo_official_map_versions!\n",
							chMapName, msg.map_crc() );
					}

					if ( uiKnownVersionWkshpId )
					{	// We have a fallback version specified for the CRC mismatch
						Msg( "Map version fallback for %s CRC=%u: %llu (CRC=%u)\n", chMapName, msg.map_crc(), uiKnownVersionWkshpId, uiRepackedWkshpCrc );
						Q_snprintf( m_szLevelName, sizeof( m_szLevelName ), "maps/workshop/%llu/%s.bsp", uiKnownVersionWkshpId, chMapName );
						Q_FixSlashes( m_szLevelName );
						Q_snprintf( m_szLevelNameShort, sizeof( m_szLevelNameShort ), "workshop/%llu/%s", uiKnownVersionWkshpId, chMapName );

						Q_snprintf( bspModelName, sizeof( bspModelName ), "maps/workshop/%llu/%s.bsp", uiKnownVersionWkshpId, chMapName );
						m_unUGCMapFileID = uiKnownVersionWkshpId;

						// Dirty method: patch in a different map crc for derived class processing to pick up repacked crc value
						const_cast< CSVCMsg_ServerInfo & >( msg ).set_map_crc( uiRepackedWkshpCrc );

						// Check if we already have the workshop file for the fallback version downloaded
						FileHandle_t mapfile = g_pFileSystem->OpenEx( bspModelName, "rb", IsGameConsole() ? FSOPEN_NEVERINPACK : 0, "GAME" );
						if ( mapfile != FILESYSTEM_INVALID_HANDLE )
						{
							g_pFileSystem->Close( mapfile );
							bCrcClientMapValid = true; // prevent CRC from resetting bClientHasMap further down in the code, compat versions never change so chances are the map is good right off the bat
						}
						else
							bClientHasMap = false;
					}
				}
			}
			else
			{
				// We are not playing a demo
				if ( bCrcClientMapValid && msg.map_crc() && ( crcClientMap != msg.map_crc() ) && ( m_unUGCMapFileID != 0 ) )
					bClientHasMap = false; // If the crc doesn't match servers delay map load until after downloading new version from the workshop
			}

			if ( !bCrcClientMapValid )
				bClientHasMap = false;
#endif

		if ( bClientHasMap )
			modelloader->GetModelForName( bspModelName, IModelLoader::FMODELLOADER_CLIENT );
	} // not a relay

	// If we connect to a dedicated server, we need to load up the dictionary file
	CRC32_t crc = CRC32_ConvertFromUnsignedLong( msg.string_table_crc() );
	if ( !g_pStringTableDictionary->OnLevelLoadStart( bClientHasMap ? m_szLevelNameShort : NULL, &crc ) )
	{
		// Allow us to continue with a mismatch string table
		// this can occur with slighty different versisons
		Warning( "***String table CRC mismatch, may need to rebuild bsp if model oddities occur!\n" );
	}

	// Client needs an opportunity to write all profile information
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
		"OnProfilesWriteOpportunity", "reason", "checkpoint"
		) );
	}

	COM_TimestampedLog( "CBaseClient::ProcessServerInfo(done)" );

	return true;
}

bool CBaseClientState::SVCMsg_SendTable( const CSVCMsg_SendTable& msg )
{
	VPROF( "SVCMsg_SendTable" );

	if ( !RecvTable_RecvClassInfos( msg ) )
	{
		Host_EndGame(true, "ProcessSendTable: RecvTable_RecvClassInfos failed.\n" );
		return false;
	}

	return true;
}

bool CBaseClientState::SVCMsg_ClassInfo( const CSVCMsg_ClassInfo& msg )
{
	VPROF( "SVCMsg_ClassInfo" );

	COM_TimestampedLog( " CBaseClient::SVCMsg_ClassInfo" );

	if ( msg.create_on_client() )
	{
		ConMsg ( "Can't create class tables.\n");
		Assert( 0 );
		return false;
	}

	if( m_pServerClasses )
	{
		delete [] m_pServerClasses;
	}

	m_nServerClasses = msg.classes_size();
	m_pServerClasses = new C_ServerClassInfo[ m_nServerClasses ];

	if ( !m_pServerClasses )
	{
		Host_EndGame(true, "SVCMsg_ClassInfo: can't allocate %d C_ServerClassInfos.\n", m_nServerClasses);
		return false;
	}

	// copy class names and class IDs from message to CClientState
	for (int i=0; i<m_nServerClasses; i++)
	{
		const CSVCMsg_ClassInfo::class_t& svclass = msg.classes( i );

		if( svclass.class_id() >= m_nServerClasses )
		{
			Host_EndGame(true, "SVCMsg_ClassInfo: invalid class index (%d).\n", svclass.class_id());
			return false;
		}

		C_ServerClassInfo * svclassinfo = &m_pServerClasses[svclass.class_id()];

		int len = Q_strlen(svclass.class_name().c_str()) + 1;
		svclassinfo->m_ClassName = new char[ len ];
		Q_strncpy( svclassinfo->m_ClassName, svclass.class_name().c_str(), len );
		len = Q_strlen(svclass.data_table_name().c_str()) + 1;
		svclassinfo->m_DatatableName = new char[ len ];
		Q_strncpy( svclassinfo->m_DatatableName,svclass.data_table_name().c_str(), len );
	}

	COM_TimestampedLog( " CBaseClient::SVCMsg_ClassInfo(done)" );

	return LinkClasses();	// link server and client classes
}

bool CBaseClientState::SVCMsg_SetPause( const CSVCMsg_SetPause& msg )
{
	VPROF( "SVCMsg_SetPause" );

	m_bPaused = msg.paused();
	return true;
}


bool CBaseClientState::SVCMsg_CreateStringTable( const CSVCMsg_CreateStringTable &msg )
{
	VPROF( "SVCMsg_CreateStringTable" );

#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_PROCESSSTRINGTABLE);
#endif

	COM_TimestampedLog( " CBaseClient::ProcessCreateStringTable(%s)", msg.name().c_str() );
	m_StringTableContainer->AllowCreation( true );

#ifndef SHARED_NET_STRING_TABLES
	CNetworkStringTable *table = (CNetworkStringTable*)
		m_StringTableContainer->CreateStringTable( msg.name().c_str(), msg.max_entries(), msg.user_data_size(), msg.user_data_size_bits(), msg.flags() );

	Assert ( table );

	table->SetTick( GetServerTickCount() ); // set creation tick

	HookClientStringTable( msg.name().c_str() );
	
	bf_read data( &msg.string_data()[0], msg.string_data().size() );
	table->ParseUpdate( data, msg.num_entries() );

#endif

	m_StringTableContainer->AllowCreation( false );

	COM_TimestampedLog( " CBaseClient::ProcessCreateStringTable(%s)-done", msg.name().c_str() );
	return true;
}

bool CBaseClientState::SVCMsg_UpdateStringTable( const CSVCMsg_UpdateStringTable &msg )
{
	VPROF( "ProcessUpdateStringTable" );

#ifndef SHARED_NET_STRING_TABLES

	//m_StringTableContainer is NULL on level transitions, Seems to be caused by a UpdateStringTable packet comming in before the ServerInfo packet
	//  I'm not sure this is safe, but at least we won't crash. The realy odd thing is this can happen on the server as well.//tmauer
	if(m_StringTableContainer != NULL)
	{
		CNetworkStringTable *table = (CNetworkStringTable*)
			m_StringTableContainer->GetTable( msg.table_id() );

		bf_read data( &msg.string_data()[0], msg.string_data().size() );
		table->ParseUpdate( data, msg.num_changed_entries() );
	}
	else
	{
		Warning("m_StringTableContainer is NULL in CBaseClientState::ProcessUpdateStringTable\n");
	}

#endif
	return true;
}

bool CBaseClientState::SVCMsg_SetView( const CSVCMsg_SetView& msg )
{
#if !defined( LINUX )
	// dkorus: may need this for online split screen
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
#endif

	m_nViewEntity = msg.entity_index();
	return true;
}

bool CBaseClientState::SVCMsg_PacketEntities( const CSVCMsg_PacketEntities &msg )
{
	VPROF( "ProcessPacketEntities" );

	// First update is the final signon stage where we actually receive an entity (i.e., the world at least)

	if ( m_nSignonState < SIGNONSTATE_SPAWN )
	{
		ConMsg("Received packet entities while connecting!\n");
		return false;
	}

	if ( m_nSignonState == SIGNONSTATE_SPAWN )
	{
		if ( !msg.is_delta() )
		{
			// We are done with signon sequence.
			SetSignonState( SIGNONSTATE_FULL, m_nServerCount, NULL );
		}
		else
		{
			ConMsg("Received delta packet entities while spawing!\n");
			return false;
		}
	}

	// overwrite a -1 delta_tick only if packet was uncompressed
	if ( (m_nDeltaTick >= 0) || !msg.is_delta() )
	{
		// we received this snapshot successfully, now this is our delta reference
		m_nDeltaTick = GetServerTickCount();
	}

	return true;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pHead - 
//			*pClassName - 
// Output : static ClientClass*
//-----------------------------------------------------------------------------
ClientClass* CBaseClientState::FindClientClass(const char *pClassName)
{
	for(ClientClass *pCur=ClientDLL_GetAllClasses(); pCur; pCur=pCur->m_pNext)
	{
		if( Q_stricmp(pCur->m_pNetworkName, pClassName) == 0)
			return pCur;
	}

	return NULL;
}


bool CBaseClientState::LinkClasses()
{
	// Verify that we have received info about all classes.
	for ( int i=0; i < m_nServerClasses; i++ )
	{
		if ( !m_pServerClasses[i].m_DatatableName )
		{
			Host_EndGame(true, "CL_ParseClassInfo_EndClasses: class %d not initialized.\n", i);
			return false;
		}
	}

	// Match the server classes to the client classes.
	for ( int i=0; i < m_nServerClasses; i++ )
	{
		C_ServerClassInfo *pServerClass = &m_pServerClasses[i];

		// (this can be null in which case we just use default behavior).
		pServerClass->m_pClientClass = FindClientClass(pServerClass->m_ClassName);

		if ( pServerClass->m_pClientClass )
		{
			// If the class names match, then their datatables must match too.
			// It's ok if the client is missing a class that the server has. In that case,
			// if the server actually tries to use it, the client will bomb out.
			const char *pServerName = pServerClass->m_DatatableName;
			const char *pClientName = pServerClass->m_pClientClass->m_pRecvTable->GetName();

			if ( Q_stricmp( pServerName, pClientName ) != 0 )
			{
				Host_EndGame( true, "CL_ParseClassInfo_EndClasses: server and client classes for '%s' use different datatables (server: %s, client: %s)",
					pServerClass->m_ClassName, pServerName, pClientName );
				
				return false;
			}

			// copy class ID
			pServerClass->m_pClientClass->m_ClassID = i;
		}
		else
		{
			Msg( "Client missing DT class %s\n", pServerClass->m_ClassName );
		}
	}

	return true;
}

PackedEntity *CBaseClientState::GetEntityBaseline(int iBaseline, int nEntityIndex)
{
	Assert( (iBaseline == 0) || (iBaseline == 1) );
	return m_pEntityBaselines[iBaseline][nEntityIndex];
}

void CBaseClientState::FreeEntityBaselines()
{
	for ( int i=0; i<2; i++ )
	{
		for ( int j=0; j<MAX_EDICTS; j++ )
		if ( m_pEntityBaselines[i][j] )
		{
			delete m_pEntityBaselines[i][j];
			m_pEntityBaselines[i][j] = NULL;
		}
	}
}

void CBaseClientState::SetEntityBaseline(int iBaseline, ClientClass *pClientClass, int index, SerializedEntityHandle_t handle)
{
	Assert( index >= 0 && index < MAX_EDICTS );
	Assert( pClientClass );
	Assert( (iBaseline == 0) || (iBaseline == 1) );

	PackedEntity *entitybl = m_pEntityBaselines[iBaseline][index];

	if ( !entitybl )
	{
		entitybl = m_pEntityBaselines[iBaseline][index] = new PackedEntity();
	}

	entitybl->m_pClientClass = pClientClass;
	entitybl->m_nEntityIndex = index;
	entitybl->m_pServerClass = NULL;

	// Copy out the data we just decoded.
	entitybl->SetPackedData( handle );
}

void CBaseClientState::CopyEntityBaseline( int iFrom, int iTo )
{
	Assert ( iFrom != iTo );
	

	for ( int i=0; i<MAX_EDICTS; i++ )
	{
		PackedEntity *blfrom = m_pEntityBaselines[iFrom][i];
		PackedEntity *blto = m_pEntityBaselines[iTo][i];

		if( !blfrom )
		{
			// make sure blto doesn't exists
			if ( blto )
			{
				// ups, we already had this entity but our ack got lost
				// we have to remove it again to stay in sync
				delete m_pEntityBaselines[iTo][i];
				m_pEntityBaselines[iTo][i] = NULL;
			}
			continue;
		}

		if ( !blto )
		{
			// create new to baseline if none existed before
			blto = m_pEntityBaselines[iTo][i] = new PackedEntity();
			blto->m_pClientClass = NULL;
			blto->m_pServerClass = NULL;
			blto->m_ReferenceCount = 0;
		}

		Assert( blfrom->m_nEntityIndex == i );

		blto->m_nEntityIndex	= blfrom->m_nEntityIndex; 
		blto->m_pClientClass	= blfrom->m_pClientClass;
		blto->m_pServerClass	= blfrom->m_pServerClass;
		blto->CopyPackedData( blfrom->GetPackedData() );
	}
}

ClientClass *CBaseClientState::GetClientClass( int index )
{
	Assert( index < m_nServerClasses );
	return m_pServerClasses[index].m_pClientClass;
}


void CBaseClientState::UpdateInstanceBaseline( int nStringNumber )
{
	int nSlot = m_BaselineHandles.Find( nStringNumber );
	if ( nSlot != m_BaselineHandles.InvalidIndex() )
	{
		// Release old
		g_pSerializedEntities->ReleaseSerializedEntity( m_BaselineHandles[ nSlot ] );
		m_BaselineHandles[ nSlot ] = SERIALIZED_ENTITY_HANDLE_INVALID;
	}
	else
	{
		m_BaselineHandles.Insert( nStringNumber, SERIALIZED_ENTITY_HANDLE_INVALID );
	}
}

bool CBaseClientState::GetClassBaseline( int iClass, SerializedEntityHandle_t *pHandle )
{
	ErrorIfNot( 
		iClass >= 0 && iClass < m_nServerClasses, 
		("GetDynamicBaseline: invalid class index '%d'", iClass) );

	// We lazily update these because if you connect to a server that's already got some dynamic baselines,
	// you'll get the baselines BEFORE you get the class descriptions.
	C_ServerClassInfo *pInfo = &m_pServerClasses[iClass];

	INetworkStringTable *pBaselineTable = GetStringTable( INSTANCE_BASELINE_TABLENAME );

	ErrorIfNot( pBaselineTable != NULL,	("GetDynamicBaseline: NULL baseline table" ) );

	if ( pInfo->m_InstanceBaselineIndex == INVALID_STRING_INDEX )
	{
		// The key is the class index string.
		char str[64];
		Q_snprintf( str, sizeof( str ), "%d", iClass );

		pInfo->m_InstanceBaselineIndex = pBaselineTable->FindStringIndex( str );

		ErrorIfNot( 
			pInfo->m_InstanceBaselineIndex != INVALID_STRING_INDEX,
			("GetDynamicBaseline: FindStringIndex(%s-%s) failed.", str, pInfo->m_ClassName );
			);
	}

	int slot = m_BaselineHandles.Find( pInfo->m_InstanceBaselineIndex );
	Assert( slot != m_BaselineHandles.InvalidIndex() );

	*pHandle = m_BaselineHandles[ slot ];
	if ( *pHandle == SERIALIZED_ENTITY_HANDLE_INVALID )
	{
		SerializedEntityHandle_t handle = g_pSerializedEntities->AllocateSerializedEntity( __FILE__, __LINE__ );

		int nLength = 0;
		const void *pData = pBaselineTable->GetStringUserData( pInfo->m_InstanceBaselineIndex, &nLength );

		bf_read readbuf( "UpdateInstanceBaseline", pData, nLength );

		RecvTable_ReadFieldList( pInfo->m_pClientClass->m_pRecvTable, readbuf, handle, -1, false );

		*pHandle = m_BaselineHandles[ slot ] = handle;
	}

	return *pHandle != SERIALIZED_ENTITY_HANDLE_INVALID;
}

bool CBaseClientState::SVCMsg_GameEventList( const CSVCMsg_GameEventList& msg )
{
	VPROF( "SVCMsg_GameEventList" );

	return g_GameEventManager.ParseEventList( msg );
}

bool CBaseClientState::SVCMsg_GetCvarValue( const CSVCMsg_GetCvarValue& msg )
{
	VPROF( "SVCMsg_GetCvarValue" );

	// Prepare the response.
	CCLCMsg_RespondCvarValue_t returnMsg;

	returnMsg.set_cookie( msg.cookie() );
	returnMsg.set_name( msg.cvar_name().c_str() );
	returnMsg.set_value( "" );
	returnMsg.set_status_code( eQueryCvarValueStatus_CvarNotFound );

	char tempValue[256];

	// Does any ConCommand exist with this name?
	const ConVar *pVar = g_pCVar->FindVar( msg.cvar_name().c_str() );
	if ( pVar )
	{
		if ( pVar->IsFlagSet( FCVAR_SERVER_CANNOT_QUERY ) )
		{
			// The server isn't allowed to query this.
			returnMsg.set_status_code( eQueryCvarValueStatus_CvarProtected );
		}
		else
		{
			returnMsg.set_status_code( eQueryCvarValueStatus_ValueIntact );

			if ( pVar->IsFlagSet( FCVAR_NEVER_AS_STRING ) )
			{
				// The cvar won't store a string, so we have to come up with a string for it ourselves.
				if ( fabs( pVar->GetFloat() - pVar->GetInt() ) < 0.001f )
				{
					Q_snprintf( tempValue, sizeof( tempValue ), "%d", pVar->GetInt() );
				}
				else
				{
					Q_snprintf( tempValue, sizeof( tempValue ), "%f", pVar->GetFloat() );
				}
				returnMsg.set_value( tempValue );
			}
			else
			{
				// The easy case..
				returnMsg.set_value( pVar->GetString() );
			}
		}				
	}
	else
	{
		if ( g_pCVar->FindCommand( msg.cvar_name().c_str() ) )
			returnMsg.set_status_code( eQueryCvarValueStatus_NotACvar ); // It's a command, not a cvar.
		else
			returnMsg.set_status_code( eQueryCvarValueStatus_CvarNotFound );
	}

	// Send back.
	m_NetChannel->SendNetMsg( returnMsg );
	return true;
}

bool CBaseClientState::SVCMsg_SplitScreen( const CSVCMsg_SplitScreen& msg )
{
#ifndef DEDICATED
	switch ( msg.type() )
	{
	default:
		Assert( 0 );
		break;
	case MSG_SPLITSCREEN_ADDUSER:
		{
			splitscreen->AddSplitScreenUser( msg.slot(), msg.player_index() );
#if defined( INCLUDE_SCALEFORM )
			extern IScaleformSlotInitController *g_pIScaleformSlotInitControllerEngineImpl;
			g_pScaleformUI->InitSlot( SF_SS_SLOT( msg.slot() ), g_szDefaultScaleformClientMovieName, g_pIScaleformSlotInitControllerEngineImpl );
#endif
		}
		break;
	case MSG_SPLITSCREEN_REMOVEUSER:
		{
			splitscreen->RemoveSplitScreenUser( msg.slot(), msg.player_index() );

#if defined( INCLUDE_SCALEFORM )
			g_pScaleformUI->SlotRelease( SF_SS_SLOT( msg.slot() ) );
#endif
		}
		break;
	}
#endif

	return true;
}

bool CBaseClientState::ChangeSplitscreenUser( int nSplitScreenUserSlot )
{
#ifndef DEDICATED
	Assert( splitscreen->IsValidSplitScreenSlot( nSplitScreenUserSlot ) );
	if ( !splitscreen->IsValidSplitScreenSlot( nSplitScreenUserSlot ) )
		return true;

	// Msg( "Networking changing slot to %d\n", msg->m_nSlot );
	splitscreen->SetActiveSplitScreenPlayerSlot( nSplitScreenUserSlot );
#endif
	return true;
}

bool CBaseClientState::SVCMsg_CmdKeyValues( const CSVCMsg_CmdKeyValues& msg )
{
#ifndef DEDICATED
	KeyValues *pMsgKeyValues = CmdKeyValuesHelper::SVCMsg_GetKeyValues( msg );
	KeyValues::AutoDelete autodelete_pMsgKeyValues( pMsgKeyValues );
	char const *szName = pMsgKeyValues->GetName();
	if ( !V_strcmp( szName, "dsp_player" ) )
	{
		extern void dsp_player_set( int val );
		dsp_player_set( pMsgKeyValues->GetInt() );
		return true;
	}

	KeyValues *pEvent = new KeyValues( "Client::CmdKeyValues" );
	pEvent->AddSubKey( autodelete_pMsgKeyValues.Detach() );
	pEvent->SetInt( "slot", m_nSplitScreenSlot );
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pEvent );

#endif
	return true;
}

bool CBaseClientState::SVCMsg_EncryptedData( const CSVCMsg_EncryptedData& msg )
{
#ifndef DEDICATED
	// Decrypt the message and process embedded data
	char const *szKey = "";
	switch ( msg.key_type() )
	{
	case kEncryptedMessageKeyType_Private:
		szKey = cl_decryptdata_key.GetString();
		break;
	case kEncryptedMessageKeyType_Public:
		szKey = cl_decryptdata_key_pub.GetString();
		break;
	}
	return CmdEncryptedDataMessageCodec::SVCMsg_EncryptedData_Process( msg, m_NetChannel, szKey );
#else
	return true;
#endif
}

int CBaseClientState::GetViewEntity()
{
	return m_nViewEntity;
}


bool CBaseClientState::ShouldUseDirectConnectAddress( const CAddressList &list ) const
{
	// Expired?
	if ( realtime > m_DirectConnectLobby.m_flEndTime )
		return false;
	// No server IP?
	if ( m_DirectConnectLobby.m_adrRemote.IsType<netadr_t>() && !m_DirectConnectLobby.m_adrRemote.AsType<netadr_t>().GetIPHostByteOrder() )
		return false;
	// Either joining unreserved server or same lobby ID
	if ( m_DirectConnectLobby.m_unLobbyID != 0ull && m_DirectConnectLobby.m_unLobbyID != m_nServerReservationCookie )
		return false;
	// Already in list
	if ( list.IsAddressInList( m_DirectConnectLobby.m_adrRemote ) )											
		return false;
	return true;
}

uint64 CBaseClientState::CAsyncOperation_ReserveServer::GetResult() {
	if ( m_eState != AOS_SUCCEEDED )
		return 0;
	static char buf[64];
	V_strcpy_safe( buf, ns_address_render( m_adr ).String() );
	return reinterpret_cast<uintp>( buf );
}

//-----------------------------------------------------------------------------
// Purpose: Sends a message to game server to reserve it for members of our
//			lobby for a short time to ensure everyone in lobby will be able to join.  
//			Server will send response accepting or denying reservation.  It will only
//			accept if it is empty and unreserved.
//-----------------------------------------------------------------------------
void CBaseClientState::ReserveServer( const ns_address &netAdrPublic, const ns_address &netAdrPrivate, uint64 nServerReservationCookie,
									  KeyValues *pKVGameSettings, IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation )
{
	NET_SetMultiplayer( true );

	// Should not already have a reservation in progress -- should only attempt one reservation at a time
	Assert( !m_pServerReservationCallback );
	if ( m_pServerReservationCallback )
	{
		ReservationResponseReply_t reply;
		reply.m_adrFrom = m_netadrReserveServer.Get( 0 ).m_adrRemote;
		HandleReservationResponse( reply );
	}

	if ( ppAsyncOperation )
	{
		m_pServerReservationOperation = new CAsyncOperation_ReserveServer( this );
		*ppAsyncOperation = m_pServerReservationOperation;
	}

	m_pServerReservationCallback = pCallback;
	m_nServerReservationCookie = nServerReservationCookie;
	m_pKVGameSettings = pKVGameSettings->MakeCopy();
	
	m_flReservationMsgSendTime = FLT_MIN;
	m_nReservationMsgRetryNumber = 0;
	m_bEnteredPassword = false;
	m_netadrReserveServer.RemoveAll();
	m_netadrReserveServer.AddRemote( ns_address_render( netAdrPublic ).String(), "public" );

	if ( !netAdrPrivate.IsNull() )	// if we have a valid private address specified
		m_netadrReserveServer.AddRemote( ns_address_render( netAdrPrivate ).String(), "private" );

	if ( ShouldUseDirectConnectAddress( m_netadrReserveServer ) )
	{
		ConColorMsg( Color( 0, 255, 0, 255 ), "Adding direct connect address to reservation %s\n", ns_address_render( m_DirectConnectLobby.m_adrRemote ).String() );
		m_netadrReserveServer.AddRemote( ns_address_render( m_DirectConnectLobby.m_adrRemote ).String(), "direct" );
	}

	// send the reservation message
	SendReserveServerMsg();
}

bool CBaseClientState::CheckServerReservation( const ns_address &netAdrPublic, uint64 nServerReservationCookie, uint32 uiReservationStage,
	IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation )
{
	Assert( ppAsyncOperation );
	if ( !ppAsyncOperation )
		return false;

	NET_SetMultiplayer( true );

	CServerMsg_CheckReservation *pSvReservationCheck = new CServerMsg_CheckReservation( this, pCallback, netAdrPublic, 
		m_Socket, nServerReservationCookie, uiReservationStage );
	*ppAsyncOperation = pSvReservationCheck;
	m_arrSvReservationCheck.AddToTail( pSvReservationCheck );

	return true;
}

bool CBaseClientState::ServerPing( const ns_address &netAdrPublic,
	IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation )
{
	Assert( ppAsyncOperation );
	if ( !ppAsyncOperation )
		return false;

	NET_SetMultiplayer( true );

	CServerMsg_Ping *pSvPing = new CServerMsg_Ping( this, pCallback, netAdrPublic, m_Socket );
	*ppAsyncOperation = pSvPing;
	m_arrSvPing.AddToTail( pSvPing );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles response from game server to reservation request
//-----------------------------------------------------------------------------
void CBaseClientState::HandleReservationResponse( const ReservationResponseReply_t &reply )
{
	// Is this the address we expect?  (It might not if this is a very delayed response
	// from an earlier game server we tried to reserve on and subsequently gave up on.)
	bool bAddressMatches = ( reply.m_adrFrom.IsLoopback() || m_netadrReserveServer.IsAddressInList( reply.m_adrFrom ) );
	if ( !bAddressMatches )
		return;

	IMatchAsyncOperationCallback *pCallback = m_pServerReservationCallback;
	if ( reply.m_uiResponse != 2 )
	{
		m_pServerReservationCallback = NULL;

		if ( m_pKVGameSettings )
		{
			m_pKVGameSettings->deleteThis();
			m_pKVGameSettings = NULL;
		}
	}

	if ( !pCallback )
		return;

	// If we are expecting a response (have a callback to call) and the address matches what 
	// we expect, call the callback then clear it.

	if ( m_pServerReservationOperation )
	{
		if ( reply.m_uiResponse == 2 )
		{
			// Reservation pending response, reset retry counter
			m_nReservationMsgRetryNumber = 0;
			DevMsg( "[MM] Server %s reservation pending response waiting...\n", ns_address_render( reply.m_adrFrom ).String() );
			return;
		}

		m_pServerReservationOperation->m_eState = reply.m_uiResponse ? AOS_SUCCEEDED : AOS_FAILED;
		m_pServerReservationOperation->m_adr = reply.m_adrFrom;
		m_pServerReservationOperation->m_numGameSlotsForReservation = reply.m_numGameSlots;
	}

	pCallback->OnOperationFinished( m_pServerReservationOperation );
}

//-----------------------------------------------------------------------------
// Purpose: Resend a server reservation request if necessary
//-----------------------------------------------------------------------------
void CBaseClientState::CheckForReservationResend()
{
	const float RESERVATION_RESEND_INTERVAL=3.0f;
	const float MAX_RESERVATION_RETRIES=2;

	if ( m_bWaitingForPassword )
		return;

	// do we have a reservation in progress?
	if ( !m_pServerReservationCallback )
		return;

	// is it time to resend?
	if ( ( net_time - m_flReservationMsgSendTime ) < RESERVATION_RESEND_INTERVAL ) 
		return;

	// fail if too many resends
	if ( m_nReservationMsgRetryNumber >= MAX_RESERVATION_RETRIES )
	{
		CUtlString desc;
		m_netadrReserveServer.Describe( desc );
		Msg( "[MM] Attempt to reserve server %s failed; timed out after %d attempts\n", desc.String(), m_nReservationMsgRetryNumber + 1 );
		
		ReservationResponseReply_t reply;
		reply.m_adrFrom = m_netadrReserveServer.Get( 0 ).m_adrRemote;
		HandleReservationResponse( reply );
		return;
	}

	m_nReservationMsgRetryNumber++;

	SendReserveServerMsg();
}

void CBaseClientState::SendReserveServerChallenge()
{
	// Send to master asking for a challenge #
	for ( int i = 0; i < m_netadrReserveServer.Count(); ++i )
	{
		Msg( "[MM] Sending reservation request to %s\n", ns_address_render( m_netadrReserveServer.Get( i ).m_adrRemote ).String() );
		NET_OutOfBandDelayedPrintf( m_Socket, m_netadrReserveServer.Get( i ).m_adrRemote, GetPrivateIPDelayMsecs() * i, "%creserve0000000", A2S_GETCHALLENGE );
	}

	// Mark time of this attempt.
	m_flReservationMsgSendTime = net_time;	// for retransmit requests
}

void CBaseClientState::HandleReserveServerChallengeResponse( int nChallengeNr )
{
	if ( !m_pServerReservationCallback )
		return;

	char	buffer[MAX_OOB_KEYVALUES+128];
	bf_write msg(buffer,sizeof(buffer));

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( A2S_RESERVE );
	msg.WriteLong( GetHostVersion() );

	BuildReserveServerPayload( msg, nChallengeNr );

	for ( int i = 0; i < m_netadrReserveServer.Count(); ++i )
	{
		NET_SendPacket( NULL, m_Socket, m_netadrReserveServer.Get( i ).m_adrRemote, msg.GetData(), msg.GetNumBytesWritten() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: encrypts an 8-byte sequence
//-----------------------------------------------------------------------------
inline void Encrypt8ByteSequence( IceKey& cipher, const unsigned char *plainText, unsigned char *cipherText)
{
	cipher.encrypt(plainText, cipherText);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void EncryptBuffer( IceKey& cipher, unsigned char *bufData, uint bufferSize)
{
	unsigned char *cipherText = bufData;
	unsigned char *plainText = bufData;
	uint bytesEncrypted = 0;

	while (bytesEncrypted < bufferSize)
	{
		// encrypt 8 byte section
		Encrypt8ByteSequence( cipher, plainText, cipherText);
		bytesEncrypted += 8;
		cipherText += 8;
		plainText += 8;
	}
}

void CBaseClientState::BuildReserveServerPayload( bf_write &msg, int nChallengeNr )
{
	char	buffer[MAX_OOB_KEYVALUES+128];
	bf_write payload(buffer,sizeof(buffer));

	if ( !IsX360() )
	{
		// Magic # to ensure icey decrypt worked
		payload.WriteLong( 0xfeedbeef );
	}

	// send the cookie that everyone in the joining party will provide to let them into the reserved server
	payload.WriteLongLong( m_nServerReservationCookie );

	int nSettingsLength = 0;
	CUtlBuffer buf;
	//this buffer needs to be endian compliant sot he X360 can talk correctly to the PC Dedicated server.
	if( buf.IsBigEndian() )
	{
		buf.SetBigEndian( false );
	}
	if ( m_pKVGameSettings )
	{
		// if we have KeyValues with game settings, convert to binary blob
		m_pKVGameSettings->WriteAsBinary( buf );
		nSettingsLength = buf.TellPut();
		// make sure it's not going to overflow one UDP packet
		Assert( nSettingsLength <= MAX_OOB_KEYVALUES );
		if ( nSettingsLength > MAX_OOB_KEYVALUES )
		{
			ReservationResponseReply_t reply;
			reply.m_adrFrom = m_netadrReserveServer.Get( 0 ).m_adrRemote;
			HandleReservationResponse( reply );
			return;
		}
	}

	// write # of bytes in game settings keyvalues
	payload.WriteLong( nSettingsLength );
	if ( nSettingsLength > 0 )
	{
		// write game setting keyvalues
		payload.WriteBytes( buf.Base(), nSettingsLength );
	}

	if ( !IsX360() )
	{
		// Pad it to multiple of 8 bytes
		while ( payload.GetNumBytesWritten() % 8 )
		{
			payload.WriteByte( 0 );
		}

		IceKey cipher(1); /* medium encryption level */
		unsigned char ucEncryptionKey[8] = { 0 };
		*( int * )&ucEncryptionKey[ 0 ] = LittleDWord( nChallengeNr ^ 0x5ef8ce12 );
		*( int * )&ucEncryptionKey[ 4 ] = LittleDWord( nChallengeNr ^ 0xaa98e42c );

		cipher.set( ucEncryptionKey );

		EncryptBuffer( cipher, (byte *)payload.GetBasePointer(), payload.GetNumBytesWritten() );
		msg.WriteLong( payload.GetNumBytesWritten() );
	}

	msg.WriteBytes( payload.GetBasePointer(), payload.GetNumBytesWritten() );
}

//-----------------------------------------------------------------------------
// Purpose: Sends a server reservation request
//-----------------------------------------------------------------------------
void CBaseClientState::SendReserveServerMsg()
{
	if ( !IsX360() )
	{
		// The PC uses a more complicated challenge response system to prevent DDoS style attacks
		SendReserveServerChallenge();
		return;
	}

	Assert( m_pServerReservationCallback );

	char	buffer[MAX_OOB_KEYVALUES+128];
	bf_write msg(buffer,sizeof(buffer));

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( A2S_RESERVE );
	msg.WriteLong( GetHostVersion() );

	BuildReserveServerPayload( msg, 0 );

	for ( int i = 0; i < m_netadrReserveServer.Count(); ++i )
	{
		NET_SendPacket( NULL, m_Socket, m_netadrReserveServer.Get( i ).m_adrRemote, msg.GetData(), msg.GetNumBytesWritten() );
	}

	// Mark time of this attempt.
	m_flReservationMsgSendTime = net_time;	// for retransmit requests
}

#ifndef DEDICATED
CSetActiveSplitScreenPlayerGuard::CSetActiveSplitScreenPlayerGuard( char const *pchContext, int nLine, int slot )
{
	m_pchContext = pchContext;
	m_nLine = nLine;
	m_nSaveSlot = splitscreen->SetActiveSplitScreenPlayerSlot( slot );
	m_bResolvable = splitscreen->SetLocalPlayerIsResolvable( pchContext, nLine, true );
}

CSetActiveSplitScreenPlayerGuard::~CSetActiveSplitScreenPlayerGuard()
{
	splitscreen->SetActiveSplitScreenPlayerSlot( m_nSaveSlot );
	splitscreen->SetLocalPlayerIsResolvable( m_pchContext, m_nLine, m_bResolvable );
}
#endif

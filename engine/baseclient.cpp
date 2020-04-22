//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:  baseclient.cpp: implementation of the CBaseClient class.
//
//===========================================================================//
 

#include "server_pch.h"
#include "baseclient.h"
#include "server.h"
#include "networkstringtable.h"
#include "framesnapshot.h"
#include "GameEventManager.h"
#include "LocalNetworkBackdoor.h"
#ifndef DEDICATED
#include "vgui_baseui_interface.h"
#endif
#include "sv_remoteaccess.h" // NotifyDedicatedServerUI()
#include "MapReslistGenerator.h"
#include "sv_steamauth.h"
#include "sv_main.h"
#include "host_state.h"
#include "net_chan.h"
#include "hltvserver.h"
#include "icliententity.h"

#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"
#include "tier0/etwprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IServerGameDLL	*serverGameDLL;

ConVar	sv_reliableavatardata( "sv_reliableavatardata", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "When enabled player avatars are exchanged via gameserver (0: off, 1: players, 2: server)" );
ConVar	sv_duplicate_playernames_ok( "sv_duplicate_playernames_ok", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "When enabled player names won't have the (#) in front of their names its the same as another player." );


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBaseClient::CBaseClient()
{
	// init all pointers
	m_NetChannel = NULL;
	m_ConVars = NULL;
	m_Server = NULL;
	m_pBaseline = NULL;
	m_bIsHLTV = false;
	m_pHltvSlaveServer = NULL;
#if defined( REPLAY_ENABLED )
	m_bIsReplay = false;
#endif
	m_bConVarsChanged = false;
	m_bSendServerInfo = false;
	m_bFullyAuthenticated = false;
	m_nSignonState = SIGNONSTATE_NONE;
	m_bSplitScreenUser = false;
	m_bSplitAllowFastDisconnect = false;
	m_bSplitPlayerDisconnecting = false;
	m_nSplitScreenPlayerSlot = 0;
	m_pAttachedTo = NULL;
	Q_memset( m_SplitScreenUsers, 0, sizeof( m_SplitScreenUsers ) );
	m_SplitScreenUsers[ 0 ] = this;
	m_ClientPlatform = CROSSPLAYPLATFORM_THISPLATFORM;

	m_nDebugID = EVENT_DEBUG_ID_INIT;
}

CBaseClient::~CBaseClient()
{
	// first remove the client as listener
	g_GameEventManager.RemoveListener( this );

	m_nDebugID = EVENT_DEBUG_ID_SHUTDOWN;
}


void CBaseClient::SetRate(int nRate, bool bForce )
{
	if ( m_NetChannel )
		m_NetChannel->SetDataRate( nRate );
}

int	CBaseClient::GetRate( void ) const
{
	if ( m_NetChannel )
	{
		return m_NetChannel->GetDataRate(); 
	}
	else
	{
		return 0;
	}
}

bool CBaseClient::FillUserInfo( player_info_s &userInfo )
{
	Q_memset( &userInfo, 0, sizeof(userInfo) );

	if ( !IsConnected() )
		return false; // inactive user, no more data available

	userInfo.version = CDLL_PLAYER_INFO_S_VERSION_CURRENT;
	Q_strncpy( userInfo.name, GetClientName(), MAX_PLAYER_NAME_LENGTH );
	Q_strncpy( userInfo.guid, GetNetworkIDString(), SIGNED_GUID_LEN + 1 );
	userInfo.friendsID = m_SteamID.GetAccountID();
	userInfo.xuid = GetClientXuid();

	Q_strncpy( userInfo.friendsName, m_FriendsName, sizeof(m_FriendsName) );
	userInfo.userID = GetUserID();
	userInfo.fakeplayer = ( IsFakeClient() && !IsSplitScreenUser() );
	userInfo.ishltv = IsHLTV();
#if defined( REPLAY_ENABLED )
	userInfo.isreplay = IsReplay();
#endif		
	for( int i=0; i< MAX_CUSTOM_FILES; i++ )
		userInfo.customFiles[i] = m_nCustomFiles[i].crc;

	userInfo.filesDownloaded = m_nFilesDownloaded;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Send text to client
// Input  : *fmt -
//			... -
//-----------------------------------------------------------------------------
void CBaseClient::ClientPrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	Q_vsnprintf (string, sizeof( string ), fmt,argptr);
	va_end (argptr);

	CSVCMsg_Print_t print;
	print.set_text( string );
	
	SendNetMsg( print, print.IsReliable(), false );
}

//-----------------------------------------------------------------------------
// Purpose: Send text to client
// Input  : *fmt -
//			... -
//-----------------------------------------------------------------------------
bool CBaseClient::SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice )
{
	if ( !m_NetChannel )
	{
		return true;
	}

	//
	// Send the actual message that was passed
	//
	int nStartBit = m_NetChannel->GetNumBitsWritten( msg.IsReliable() || bForceReliable );
	bool bret = m_NetChannel->SendNetMsg( msg, bForceReliable, bVoice );
	if ( IsTracing() )
	{
		int nBits = m_NetChannel->GetNumBitsWritten( msg.IsReliable() || bForceReliable ) - nStartBit;
		TraceNetworkMsg( nBits, "NetMessage %s", msg.ToString() );
	}
	return bret;
}

CHLTVServer	* CBaseClient::GetHltvServer()
{
	if ( m_Server->IsHLTV() )
		return static_cast< CHLTVServer* >( m_Server );
	else
	{
		Assert( !m_bIsHLTV || m_pHltvSlaveServer ); // if we have m_bIsHLTV mark, it means we connect to HLTV data source, either as a client listening to the HLTV port, or as a master client of HLTV Slave server. We can't be m_bIsHLTV and not be connected to any HLTV server from any end
		return NULL;
	}
}

CHLTVServer	* CBaseClient::GetAnyConnectedHltvServer()
{
	if ( m_pHltvSlaveServer )
	{
		Assert( !m_Server->IsHLTV() ); // we shouldn't cascade hltv->hltv within the same process
		return m_pHltvSlaveServer;
	}
	else
	{
		return GetHltvServer();
	}
}


char const *CBaseClient::GetUserSetting( char const *cvar ) const
{
	if ( !m_ConVars || !cvar || !cvar[0] )
	{
		return "";
	}

	const char * value = m_ConVars->GetString( cvar, "" );

	if ( value[0]==0 )
	{
		// For non FCVAR_SS fields, defer to the player 0 value
		if ( m_bSplitScreenUser )
		{
			return m_pAttachedTo->GetUserSetting( cvar );
		}

// 		// check if this var even existed
// 		if ( m_ConVars->GetDataType( cvar ) == KeyValues::TYPE_NONE )
// 		{ 
// 			DevMsg( "GetUserSetting: cvar '%s' unknown.\n", cvar );
// 		}
	}

	return value;
}

void CBaseClient::SetUserCVar( const char *cvar, const char *value)
{
	if ( !cvar || !value )
		return;

	m_ConVars->SetString( cvar, value );
}

void CBaseClient::SetUpdateRate( float fUpdateRate, bool bForce)
{
	fUpdateRate = clamp( fUpdateRate, 1, 128.0f );

	m_fSnapshotInterval = 1.0f / fUpdateRate;
}

float CBaseClient::GetUpdateRate(void) const
{
	if ( m_fSnapshotInterval > 0 )
		return 1.0f / m_fSnapshotInterval;
	else
		return 0.0f;
}

void CBaseClient::FreeBaselines()
{
	if ( m_pBaseline )
	{
		m_pBaseline->ReleaseReference();
		m_pBaseline = NULL;
	}

	m_nBaselineUpdateTick = -1;
	m_nBaselineUsed = 0;
	m_BaselinesSent.ClearAll();
}

void CBaseClient::SetSignonState( int nState )
{
	bool bOldIsConnected = IsConnected();
	m_nSignonState = nState;
	bool bNewIsConnected = IsConnected();
	if ( ( bOldIsConnected != bNewIsConnected ) && ( !IsFakeClient() || IsSplitScreenUser() ) )
	{	// SetSignonState can be called in the callstack of an existing client
		// and we don't want to trigger full hibernation and cause all clients
		// to disconnect as a result, and come back into a callstack with a bad "this" CBaseClient pointer.
		// So we just defer hibernation update till next SV_Think frame
		// ( Also note that disconnecting GOTV clients can trigger a hibernation update on the main game server )
		sv.UpdateHibernationStateDeferred();
	}
}

void CBaseClient::Clear()
{
	// Throw away any residual garbage in the channel.
	if ( m_NetChannel )
	{
		m_NetChannel->Shutdown("Disconnect by server.\n");
		m_NetChannel = NULL;
	}

	if ( m_ConVars )
	{
		m_ConVars->deleteThis();
		m_ConVars = NULL;
	}

	FreeBaselines();

	// This used to be a memset, but memset will screw up any embedded classes
	// and we want to preserve some things like index.
	SetSignonState( SIGNONSTATE_NONE );
	m_nDeltaTick = -1;
	m_nSignonTick = 0;
	m_nStringTableAckTick = 0;
	m_pLastSnapshot = NULL;
	m_nForceWaitForTick = -1;
	m_bFakePlayer = false;
	m_bLowViolence = false;
	m_bSplitScreenUser = false;
	m_bSplitAllowFastDisconnect = false;
	m_bSplitPlayerDisconnecting = false;
	m_nSplitScreenPlayerSlot = 0;
	m_pAttachedTo = NULL;
	m_bIsHLTV = false;
	//???TODO: do we need to disconnect slave hltv server?
	//m_pHltvSlaveServer = NULL;
#if defined( REPLAY_ENABLED )
	m_bIsReplay = false;
#endif
	m_fNextMessageTime = 0;
	m_fSnapshotInterval = 0;
	m_bReceivedPacket = false;
	m_UserID = 0;
	m_Name[0] = 0;
	strcpy(m_Name, "EMPTY");
	m_FriendsName[0] = 0;
	m_nSendtableCRC = 0;
	m_nBaselineUpdateTick = -1;
	m_nBaselineUsed = 0;
	m_nFilesDownloaded = 0;
	m_bConVarsChanged = false;
	m_bSendServerInfo = false;
	m_nLoadingProgress = 0;
	m_bFullyAuthenticated = false;
	m_ClientPlatform = CROSSPLAYPLATFORM_THISPLATFORM;

	m_msgAvatarData.Clear();

	Q_memset( m_nCustomFiles, 0, sizeof(m_nCustomFiles) );
}

bool CBaseClient::ProcessSignonStateMsg(int state, int spawncount)
{
	if ( IsSplitScreenUser() )
		return true;

	COM_TimestampedLog( "CBaseClient::ProcessSignonStateMsg: %s  :  %d", GetClientName(), GetSignonState() );

	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	switch( GetSignonState() )
	{
		case SIGNONSTATE_CONNECTED :	// client is connected, leave client in this state and let SendPendingSignonData do the rest
										m_bSendServerInfo = true; 
										break;

		case SIGNONSTATE_NEW		:	// client got server info, send prespawn datam_Client->SendServerInfo()
										if ( !SendSignonData() )
											return false;
										
										break;

		case SIGNONSTATE_PRESPAWN	:	SpawnPlayer();
										break;

		case SIGNONSTATE_SPAWN		:	{
											for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
											{
												if ( m_SplitScreenUsers[ i ] )
												{
													m_SplitScreenUsers[ i ]->ActivatePlayer();
												}
											}
										}
										break;

		case SIGNONSTATE_FULL		:	{
											for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
											{
												if ( m_SplitScreenUsers[ i ] )
												{
													m_SplitScreenUsers[ i ]->SendFullConnectEvent();
												}
											}

											// My net channel
											INetChannel *pMyNetChannel = GetNetChannel();

											// Force load known avatars for the users when they fully connect
											if ( ( GetServer() == &sv ) &&
												( sv_reliableavatardata.GetInt() == 2 ) &&
												!this->IsFakeClient() && !this->IsHLTV() &&
												m_SteamID.IsValid() )
											{
												//
												// Try to load the avatar data for this player
												//
												CUtlBuffer bufAvatarData;
												CUtlBuffer bufAvatarDataDefault;
												CUtlBuffer *pbufUseRgb = NULL;
												if ( !pbufUseRgb &&
													g_pFullFileSystem->ReadFile( CFmtStr( "avatars/%llu.rgb", m_SteamID.ConvertToUint64() ), "MOD", bufAvatarData ) &&
													( bufAvatarData.TellPut() == 64*64*3 ) )
													pbufUseRgb = &bufAvatarData;
												if ( !pbufUseRgb &&
													g_pFullFileSystem->ReadFile( "avatars/default.rgb", "MOD", bufAvatarDataDefault ) &&
													( bufAvatarDataDefault.TellPut() == 64 * 64 * 3 ) )
													pbufUseRgb = &bufAvatarDataDefault;

												if ( pbufUseRgb )
												{
													m_msgAvatarData.set_rgb( pbufUseRgb->Base(), pbufUseRgb->TellPut() );
													m_msgAvatarData.set_accountid( m_SteamID.GetAccountID() );

													OnPlayerAvatarDataChanged();

													// Since we are forcing this avatar inform the user about it
													if ( pMyNetChannel )
														pMyNetChannel->EnqueueVeryLargeAsyncTransfer( m_msgAvatarData );
												}
											}

											// Also broadcast to this user all avatars that we have from other players!
											if ( ( GetServer() == &sv ) && pMyNetChannel )
											{
												// Broadcast to this user avatars of all other users who already uploaded their avatars
												for ( int iClient = 0; iClient < sv.GetClientCount(); ++iClient )
												{
													CBaseClient *pClient = dynamic_cast< CBaseClient * >( sv.GetClient( iClient ) );
													if ( !pClient->IsConnected() )
														continue;

													// In debug build we can set to echo my own avatar back to client
													if ( pClient == this )
														continue;

													if ( pClient->m_msgAvatarData.rgb().size() != 64 * 64 * 3 )
														continue;

													pMyNetChannel->EnqueueVeryLargeAsyncTransfer( pClient->m_msgAvatarData );
												}
											}
										}
										break;	

		case SIGNONSTATE_CHANGELEVEL:	break;	

	}

	return true;
}

void CBaseClient::Reconnect( void )
{
	ConMsg("Forcing client reconnect (%i)\n", GetSignonState() );
	
	m_NetChannel->Clear();

	SetSignonState( SIGNONSTATE_CONNECTED );
	
	CNETMsg_SignonState_t signon( GetSignonState(), -1 );
	FillSignOnFullServerInfo( signon );
	m_NetChannel->SendNetMsg( signon );
}

void CBaseClient::Inactivate( void )
{
	FreeBaselines();

	m_nDeltaTick = -1;
	m_nSignonTick = 0;
	m_nStringTableAckTick = 0;
	m_pLastSnapshot = NULL;
	m_nForceWaitForTick = -1;

	SetSignonState( SIGNONSTATE_CHANGELEVEL );

	if ( m_NetChannel )
	{
		// don't do that for fakeclients
		m_NetChannel->Clear();
		
		if ( NET_IsMultiplayer() && !IsSplitScreenUser() )
		{
			CNETMsg_SignonState_t signon( GetSignonState(), m_Server->GetSpawnCount() );
			FillSignOnFullServerInfo( signon );
			SendNetMsg( signon );

			// force sending message now
			m_NetChannel->Transmit();	
		}
	}

	// don't receive event messages anymore
	g_GameEventManager.RemoveListener( this );
}

void CBaseClient::SetName(const char * name)
{
	if ( StringHasPrefix( name, m_Name ) )
		return; // didn't change

	int			i;
	int			dupc = 1;
	char		*p, *val;

	char	newname[MAX_PLAYER_NAME_LENGTH];

	// remove evil char '%'
	char *pFrom = (char *)name;
	char *pTo = m_Name;
	char *pLimit = &m_Name[sizeof(m_Name)-1];

	while ( *pFrom && pTo < pLimit )
	{
		// Don't copy '%' or '~' chars across
		// Don't copy '#' chars across if they would go into the first position in the name
		if ( *pFrom != '%' &&
			 *pFrom != '~' &&
			 ( *pFrom != '#' || pTo != &m_Name[0] ) )
		{
			*pTo++ = *pFrom;
		}
		else
		{
			*pTo++ = '?';
		}

		pFrom++;
	}
	*pTo = 0;

	if ( Q_strlen( m_Name ) <= 0 )
	{
		Q_snprintf( m_Name, sizeof(m_Name), "unnamed" );
	}

	val = m_Name;

	// Don't care about duplicate names on the xbox. It can only occur when a player
	// is reconnecting after crashing, and we don't want to ever show the (X) then.
	// We also don't care for tournaments to use (1) in names since names are baked into the GC schema
	// also don't care in coop because bots can have the same names
	static char const * s_pchTournamentServer = CommandLine()->ParmValue( "-tournament", ( char const * ) NULL );
	if ( !s_pchTournamentServer && !IsX360() && !NET_IsDedicatedForXbox() && !sv_duplicate_playernames_ok.GetBool() )
	{
		// Check to see if another user by the same name exists
		while ( true )
		{
			for ( i = 0; i < m_Server->GetClientCount(); i++ )
			{
				IClient *client = m_Server->GetClient( i );

				if( !client->IsConnected() || client == this )
					continue;
				
				if( !Q_stricmp( client->GetClientName(), val ) )
					break;
			}

			if (i >= m_Server->GetClientCount())
				break;

			p = val;

			if (val[0] == '(')
			{
				if (val[2] == ')')
				{
					p = val + 3;
				}
				else if (val[3] == ')')	//assumes max players is < 100
				{
					p = val + 4;
				}
			}

			Q_snprintf(newname, sizeof(newname), "(%d)%-.*s", dupc++, MAX_PLAYER_NAME_LENGTH - 4, p );
			Q_strncpy(m_Name, newname, sizeof(m_Name));
			
			val = m_Name;		
		}	
	}

	m_ConVars->SetString( "name", m_Name );

	m_Server->UserInfoChanged( m_nClientSlot );
}

void CBaseClient::ActivatePlayer()
{
	COM_TimestampedLog( "CBaseClient::ActivatePlayer" );

	// tell server to update the user info table (if not already done)
	m_Server->UserInfoChanged( m_nClientSlot );

	SetSignonState( SIGNONSTATE_FULL );
	MapReslistGenerator().OnPlayerSpawn();

	// update the UI
	NotifyDedicatedServerUI("UpdatePlayers");
}

void CBaseClient::SpawnPlayer( void )
{
	COM_TimestampedLog( "CBaseClient::SpawnPlayer" );

	if ( !IsFakeClient() )
	{
		// free old baseline snapshot
		FreeBaselines();
		
		// create baseline snapshot for real clients
		m_pBaseline = framesnapshotmanager->CreateEmptySnapshot( 
#ifdef DEBUG_SNAPSHOT_REFERENCES
			CFmtStr( "CBaseClient[%d,%s]::SpawnPlayer", m_nClientSlot, GetClientName() ).Access(),
#endif
			0, MAX_EDICTS );
	}

	// Set client clock to match server's
	CNETMsg_Tick_t tick( m_Server->GetTick(), host_frameendtime_computationduration, host_frametime_stddeviation, host_framestarttime_stddeviation );
	if ( GetHltvReplayDelay() )
	{
		tick.set_hltv_replay_flags( 1 );
	}
	SendNetMsg( tick, true );
	
	// Spawned into server, not fully active, though
	SetSignonState( SIGNONSTATE_SPAWN );
	CNETMsg_SignonState_t signonState( GetSignonState(), m_Server->GetSpawnCount() );
	FillSignOnFullServerInfo( signonState );
	SendNetMsg( signonState );
}

bool CBaseClient::SendSignonData( void )
{
	COM_TimestampedLog( " CBaseClient::SendSignonData" );
#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_SENDSIGNONDATA, false );
#endif

	if ( m_Server->m_Signon.IsOverflowed() )
	{
		Host_Error( "Signon buffer overflowed %i bytes!!!\n", m_Server->m_Signon.GetNumBytesWritten() );
		return false;
	}

	m_NetChannel->SendData( m_Server->m_Signon );

#ifndef DEDICATED
	S_PreventSound( false ); //it is now safe to use audio again.
#endif

	SetSignonState( SIGNONSTATE_PRESPAWN );
	CNETMsg_SignonState_t signonState( GetSignonState(), m_Server->GetSpawnCount() );
	FillSignOnFullServerInfo( signonState );
	
	return m_NetChannel->SendNetMsg( signonState );
}

void CBaseClient::Connect( const char *szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, CrossPlayPlatform_t clientPlatform, const CMsg_CVars *pVecCvars /*= NULL*/ )
{
	COM_TimestampedLog( "CBaseClient::Connect" );


#ifndef DEDICATED
	if ( !bFakePlayer )
	{
		EngineVGui()->UpdateProgressBar(PROGRESS_SIGNONCONNECT, false);
	}
#endif
	Clear();

	m_UserID = nUserID;

	m_ConVars = new KeyValues("userinfo");
	if ( pVecCvars )
	{
		ApplyConVars( *pVecCvars, true ); // set all initial user info cvars
		char const *pchName = GetUserSetting( "name" );
		pchName = serverGameClients->ClientNameHandler( m_SteamID.ConvertToUint64(), pchName );
		SetName( ( pchName && *pchName ) ? pchName : szName );
	}
	else
	{
		szName = serverGameClients->ClientNameHandler( m_SteamID.ConvertToUint64(), szName );
		SetName( szName );
	}

	m_bFakePlayer = bFakePlayer;
	if ( bFakePlayer )
	{
		Steam3Server().NotifyLocalClientConnect( this );
	}
	m_NetChannel = pNetChannel;

	if ( m_NetChannel && m_Server && m_Server->IsMultiplayer() )
	{
		m_NetChannel->SetCompressionMode( true );
	}

	m_ClientPlatform = clientPlatform;

	SetSignonState( SIGNONSTATE_CONNECTED );
}

//-----------------------------------------------------------------------------
// Purpose: Drops client from server, with explanation
// Input  : *cl -
//			crash -
//			*fmt -
//			... -
//-----------------------------------------------------------------------------
void CBaseClient::PerformDisconnection( const char *pReason )
{
#if !defined( DEDICATED ) && defined( ENABLE_RPT )
	SV_NotifyRPTOfDisconnect( m_nClientSlot );
#endif

	Steam3Server().NotifyClientDisconnect( this );

	SetSignonState( SIGNONSTATE_NONE );

	// Make sure the client is valid to be disconnected
	// Splitscreen parasites end up disconnecting twice
	// sometimes which may cause havoc because
	// m_Clients is accessed at the wrong index -- Vitaliy
	if ( m_nClientSlot < 0 ||
		m_nClientSlot >= m_Server->GetClientCount() ||
		m_Server->GetClient( m_nClientSlot ) != ( IClient * ) this )
	{
		return;
	}

	m_Server->UserInfoChanged( m_nClientSlot );

	// L4D: don't print when bots remove themselves
	if ( developer.GetInt() > 1 || !IsFakeClient() || IsSplitScreenUser() )
	{
		ConMsg("Dropped %s from server: %s\n", GetClientName(), pReason );
	}

	// remove the client as listener
	g_GameEventManager.RemoveListener( this );

	if ( m_pAttachedTo && m_pAttachedTo->GetNetChannel() )
	{
		m_pAttachedTo->GetNetChannel()->DetachSplitPlayer( m_nSplitScreenPlayerSlot );
		m_pAttachedTo = NULL;
	}

	// Send the remaining reliable buffer so the client finds out the server is shutting down.
	if ( m_NetChannel )
	{
		m_NetChannel->Shutdown( pReason );
		m_NetChannel = NULL;
	}

	Clear(); // clear state
}

void CBaseClient::Disconnect( const char *fmt )
{
	if ( GetSignonState() == SIGNONSTATE_NONE )
		return;	// no recursion

	// Make sure the client is valid to be disconnected
	// During server shutdown splitscreen parasites end up
	// disconnecting twice sometimes which may cause havoc because
	// m_Clients is accessed at the wrong index -- Vitaliy
	if ( m_nClientSlot < 0 ||
		 m_nClientSlot >= m_Server->GetClientCount() ||
		 m_Server->GetClient( m_nClientSlot ) != ( IClient * ) this )
	{
		return;
	}

	if ( IsSplitScreenUser() && !m_bSplitAllowFastDisconnect )
	{
		CNETMsg_StringCmd_t stringCmd( va( "ss_disconnect %d\n", m_nSplitScreenPlayerSlot ) );
		SendNetMsg( stringCmd, true );
		return;
	}

	// Need to have all splitscreen parasites go away too
	if ( !IsSplitScreenUser() )
	{
		for ( int j = host_state.max_splitscreen_players; j -- > 1; )
		{
			if ( !m_SplitScreenUsers[ j ] )
				continue;

			m_SplitScreenUsers[ j ]->PerformDisconnection( "leaving splitscreen" );
			m_SplitScreenUsers[ j ] = NULL;
		}
	}

	// Strip trailing return character
// 	while ( len > 0 )
// 	{
// 		if ( string[ len - 1 ] != '\n' )
// 		{
// 			break;
// 		}
// 
// 		string[ len - 1 ] = 0;
// 		--len;
// 	}

	PerformDisconnection( fmt );

	NotifyDedicatedServerUI("UpdatePlayers");

	Steam3Server().SendUpdatedServerDetails(); // Update the master server.
}


void CBaseClient::FireGameEvent( IGameEvent *event, bool bPassthrough )
{
	CSVCMsg_GameEvent_t eventMsg;

	// create bitstream from KeyValues
	if ( g_GameEventManager.SerializeEvent( event, &eventMsg ) )
	{
		if ( m_NetChannel )
		{
			if ( bPassthrough )
				eventMsg.set_passthrough( 1 );
			m_NetChannel->SendNetMsg( eventMsg, event->IsReliable() );

			// This is our last chance to deliver this message out since the
			// secure channels will be closed!
			if ( !Q_stricmp( event->GetName(), "server_pre_shutdown" ) )
				m_NetChannel->Transmit();
		}
	}
	else
	{
		DevMsg("GameEventManager: failed to serialize event '%s'.\n", event->GetName() );
	}
}


int CBaseClient::GetEventDebugID( void )
{
	return m_nDebugID;
}

bool CBaseClient::SendServerInfo( void )
{
	COM_TimestampedLog( " CBaseClient::SendServerInfo: %s  :  %d", GetClientName(), GetSignonState() );

	// supporting smaller stack
	net_scratchbuffer_t scratch;
	bf_write msg( "SV_SendServerinfo->msg", scratch.GetBuffer(), scratch.Size() );

	// Only send this message to developer console, or multiplayer clients.
	if ( developer.GetBool() || m_Server->IsMultiplayer() )
	{
		char devtext[ 2048 ];

		int nHumans;
		int nMaxHumans;
		int nBots;

		sv.GetMasterServerPlayerCounts( nHumans, nMaxHumans, nBots );

		Q_snprintf( devtext, sizeof( devtext ), 
			"\n%s\nMap: %s\nPlayers: %i (%i bots) / %i humans\nBuild: %d\nServer Number: %i\n\n",
			serverGameDLL->GetGameDescription(),
			m_Server->GetMapName(),
			nHumans, nBots, nMaxHumans,
			build_number(),
			m_Server->GetSpawnCount() );

		CSVCMsg_Print_t printMsg;
		printMsg.set_text( devtext );
		printMsg.WriteToBuffer( msg );
	}

	// write additional server payload
	if ( KeyValues *kvExtendedServerInfo = serverGameDLL->GetExtendedServerInfoForNewClient() )
	{
		// This field must be always set when sending the packet to client,
		// because kvExtendedServerInfo describes the game and is cached in server.dll,
		// but the clients can connect on SERVER port or on GOTV port and must
		// receive appropriate server info for the port that they are using
		kvExtendedServerInfo->SetInt( "gotv", m_Server->IsHLTV() );

		CSVCMsg_CmdKeyValues_t cmdExtendedServerInfo;
		CmdKeyValuesHelper::SVCMsg_SetKeyValues( cmdExtendedServerInfo, kvExtendedServerInfo );
		cmdExtendedServerInfo.WriteToBuffer( msg );
	}

	CSVCMsg_ServerInfo_t serverinfo;	// create serverinfo message

	serverinfo.set_player_slot( m_nClientSlot ); // own slot number

	m_Server->FillServerInfo( serverinfo ); // fill rest of info message

	serverinfo.WriteToBuffer( msg );

	if ( g_pMatchFramework && !sv.GetReservationCookie() )
	{
		KeyValues *kvUpdate = new KeyValues( "OnEngineListenServerStarted" );
		if ( Steam3Server().SteamGameServer() )
			kvUpdate->SetInt( "externalIP", Steam3Server().SteamGameServer()->GetPublicIP() );
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvUpdate );
	}

	// send first tick
	m_nSignonTick = m_Server->m_nTickCount;
	
	CNETMsg_Tick_t signonTick( m_nSignonTick, 0, 0, 0 );
	signonTick.WriteToBuffer( msg );

	// Write replicated ConVars to non-listen server clients only
	if ( !m_NetChannel->IsLoopback() )
	{
		CNETMsg_SetConVar_t convars;
		Host_BuildConVarUpdateMessage( convars.mutable_convars(), FCVAR_REPLICATED, true );
		if ( m_Server->IsHLTV() )
		{
			static_cast< CHLTVServer* >( m_Server )->FixupConvars( convars );
		}
		convars.WriteToBuffer( msg );
	}

	// write stringtable baselines
#ifndef SHARED_NET_STRING_TABLES
	m_Server->m_StringTables->WriteBaselines( m_Server->GetMapName(), msg );
#endif

	m_bSendServerInfo = false;

	// send signon state
	SetSignonState( SIGNONSTATE_NEW );
	CNETMsg_SignonState_t signonMsg( GetSignonState(), m_Server->GetSpawnCount() );
	FillSignOnFullServerInfo( signonMsg );
	signonMsg.WriteToBuffer( msg );

	// send server info as one data block
	DevMsg( "Sending server info signon packet for %s: %u / %u buffer %s\n",
		m_NetChannel->GetAddress(), msg.GetNumBytesWritten(), NET_MAX_PAYLOAD,
		( msg.IsOverflowed() ? " OVERFLOW" : "" ) );
	if ( msg.IsOverflowed() ||
		 !m_NetChannel->SendData( msg ) )
	{
		Disconnect("Server info data overflow");
		return false;
	}
		
	COM_TimestampedLog( " CBaseClient::SendServerInfo(finished)" );

	return true;
}

void CBaseClient::OnSteamServerLogonSuccess( uint32 externalIP )
{
	if ( g_pMatchFramework && !sv.GetReservationCookie() )
	{
		KeyValues *kvUpdate = new KeyValues( "OnEngineListenServerStarted" );
		kvUpdate->SetInt( "externalIP", externalIP );		
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvUpdate );
	}
}

CClientFrame *CBaseClient::GetDeltaFrame( int nTick )
{
	Assert( 0 ); // derive moe
	return NULL; // CBaseClient has no delta frames
}

void CBaseClient::WriteGameSounds(bf_write &buf, int nMaxSounds )
{
	// CBaseClient has no events
}

void CBaseClient::ConnectionStart(INetChannel *chan)
{
	m_NetMessages[ NETMSG_Tick ].Bind< CNETMsg_Tick_t >( chan, UtlMakeDelegate( this, &CBaseClient::NETMsg_Tick ) );
	m_NetMessages[ NETMSG_StringCmd ].Bind< CNETMsg_StringCmd_t >( chan, UtlMakeDelegate( this, &CBaseClient::NETMsg_StringCmd ) );
	m_NetMessages[ NETMSG_SignonState ].Bind< CNETMsg_SignonState_t >( chan, UtlMakeDelegate( this, &CBaseClient::NETMsg_SignonState ) );
	m_NetMessages[ NETMSG_SetConVar ].Bind< CNETMsg_SetConVar_t >( chan, UtlMakeDelegate( this, &CBaseClient::NETMsg_SetConVar ) );
	m_NetMessages[ NETMSG_PlayerAvatarData ].Bind< CNETMsg_PlayerAvatarData_t >( chan, UtlMakeDelegate( this, &CBaseClient::NETMsg_PlayerAvatarData ) );
	
	m_NetMessages[ NETMSG_ClientInfo ].Bind< CCLCMsg_ClientInfo_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_ClientInfo ) );
	m_NetMessages[ NETMSG_Move ].Bind< CCLCMsg_Move_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_Move ) );
	m_NetMessages[ NETMSG_VoiceData ].Bind< CCLCMsg_VoiceData_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_VoiceData ) );
	m_NetMessages[ NETMSG_BaselineAck ].Bind< CCLCMsg_BaselineAck_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_BaselineAck ) );
	m_NetMessages[ NETMSG_ListenEvents ].Bind< CCLCMsg_ListenEvents_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_ListenEvents ) );
	m_NetMessages[ NETMSG_RespondCvarValue ].Bind< CCLCMsg_RespondCvarValue_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_RespondCvarValue ) );
	m_NetMessages[ NETMSG_FileCRCCheck ].Bind< CCLCMsg_FileCRCCheck_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_FileCRCCheck ) );
	m_NetMessages[ NETMSG_SplitPlayerConnect ].Bind< CCLCMsg_SplitPlayerConnect_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_SplitPlayerConnect ) );
	m_NetMessages[ NETMSG_LoadingProgress ].Bind< CCLCMsg_LoadingProgress_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_LoadingProgress ) );	
	m_NetMessages[ NETMSG_CmdKeyValues ].Bind< CCLCMsg_CmdKeyValues_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_CmdKeyValues ) );	
	m_NetMessages[ NETMSG_HltvReplay ].Bind< CCLCMsg_HltvReplay_t >( chan, UtlMakeDelegate( this, &CBaseClient::CLCMsg_HltvReplay ) );
	
	m_NetMessages[ NETMSG_UserMessage ].Bind< CSVCMsg_UserMessage_t >( chan, UtlMakeDelegate( this, &CBaseClient::SVCMsg_UserMessage ) );
}

void CBaseClient::ConnectionStop( )
{
	m_NetMessages[ NETMSG_Tick ].Unbind();
	m_NetMessages[ NETMSG_StringCmd ].Unbind();
	m_NetMessages[ NETMSG_SignonState ].Unbind();
	m_NetMessages[ NETMSG_SetConVar ].Unbind();
	m_NetMessages[ NETMSG_PlayerAvatarData ].Unbind();

	m_NetMessages[ NETMSG_ClientInfo ].Unbind();
	m_NetMessages[ NETMSG_Move ].Unbind();
	m_NetMessages[ NETMSG_VoiceData ].Unbind();
	m_NetMessages[ NETMSG_BaselineAck ].Unbind();
	m_NetMessages[ NETMSG_ListenEvents ].Unbind();
	m_NetMessages[ NETMSG_RespondCvarValue ].Unbind();
	m_NetMessages[ NETMSG_FileCRCCheck ].Unbind();
	m_NetMessages[ NETMSG_SplitPlayerConnect ].Unbind();
	m_NetMessages[ NETMSG_LoadingProgress ].Unbind();
	m_NetMessages[ NETMSG_CmdKeyValues ].Unbind();
	m_NetMessages[ NETMSG_HltvReplay ].Unbind();

	m_NetMessages[ NETMSG_UserMessage ].Unbind();
}

bool CBaseClient::NETMsg_Tick( const CNETMsg_Tick& msg )
{
	// framerate stats is the same whether we're in replay or not
	m_NetChannel->SetRemoteFramerate(
		CNETMsg_Tick_t::FrametimeToFloat( msg.host_computationtime() ),
		CNETMsg_Tick_t::FrametimeToFloat( msg.host_computationtime_std_deviation() ),
		CNETMsg_Tick_t::FrametimeToFloat( msg.host_framestarttime_std_deviation() ) );
	int nTick = msg.tick();
	if ( nTick == -1 // tick == -1 is a call from client to send the full frame update, the client may be in bad state w.r.t. hltv replay
	  || !msg.hltv_replay_flags() == !GetHltvReplayDelay() ) // the ack should be from the frame from the same timeline as we're feeding the player. Real-time-line acks shouldn't mix up with Replay-time-line acks
	{
		return UpdateAcknowledgedFramecount( nTick );
	}
	else
	{
		// we're fine, the ack is probably from the frame before switching to/from replay
		return true;
	}
}

bool CBaseClient::NETMsg_StringCmd( const CNETMsg_StringCmd& msg )
{
	ExecuteStringCommand( msg.command().c_str() );
	return true;
}

bool CBaseClient::NETMsg_PlayerAvatarData( const CNETMsg_PlayerAvatarData& msg )
{
	if ( sv_reliableavatardata.GetInt() != 1 )
		return true;

	if ( ( GetServer() == &sv ) && ( msg.rgb().size() == 64*64*3 ) )
	{
		m_msgAvatarData.CopyFrom( msg );
		m_msgAvatarData.set_accountid( m_SteamID.GetAccountID() );

		OnPlayerAvatarDataChanged();
	}
	return true;
}

void CBaseClient::OnPlayerAvatarDataChanged()
{
	// Broadcast this user's avatar to all other users who already got other updates
	for ( int iClient = 0; iClient < sv.GetClientCount(); ++iClient )
	{
		CBaseClient *pClient = dynamic_cast< CBaseClient * >( sv.GetClient( iClient ) );
		if ( !pClient->IsActive() )
			continue;

		// In debug build we can set to echo my own avatar back to client
		if ( pClient == this )
			continue;

		// If this is a GOTV thunk then forward them the raw data
		if ( pClient->IsHLTV() )
		{
			if ( CHLTVServer *hltv = pClient->GetAnyConnectedHltvServer() )
			{
				hltv->NETMsg_PlayerAvatarData( m_msgAvatarData );
			}
			continue;
		}

		if ( INetChannel *pNetChannel = pClient->GetNetChannel() )
		{
			pNetChannel->EnqueueVeryLargeAsyncTransfer( m_msgAvatarData );
		}
	}
}

void CBaseClient::ApplyConVars( const CMsg_CVars& list, bool bCreateIfNotExisting )
{
	int convars_size = list.cvars_size();

	for ( int i = 0; i < convars_size; ++i )
	{
		const char *name = NetMsgGetCVarUsingDictionary( list.cvars(i) );
		const char *value = list.cvars(i).value().c_str();

		if ( !V_stricmp( name, "name" ) )
		{
			value = serverGameClients->ClientNameHandler( m_SteamID.ConvertToUint64(), value );
		}

		if ( !bCreateIfNotExisting && !m_ConVars->FindKey( name ) )
		{
			static double s_dblLastWarned = 0.0;
			double dblTimeNow = Plat_FloatTime();
		#ifndef _DEBUG	// warn all the time in debug build
			if ( dblTimeNow - s_dblLastWarned > 10 )
		#endif
			{
				s_dblLastWarned = dblTimeNow;
				Warning( "Client \"%s\" SteamID %s userinfo ignored: \"%s\" = \"%s\"\n",
					this->GetClientName(), CSteamID( this->GetClientXuid() ).Render(), name, value );
			}
			continue;
		}

		m_ConVars->SetString( name, value );
		m_bConVarsChanged = true;
	}
}

bool CBaseClient::NETMsg_SetConVar( const CNETMsg_SetConVar& msg )
{
	ApplyConVars( msg.convars(), false );	// followup cvars, must be set on connect
	return true;
}

bool CBaseClient::NETMsg_SignonState( const CNETMsg_SignonState& msg )
{
	if ( msg.signon_state() == SIGNONSTATE_CHANGELEVEL )
	{
		return true; // ignore this message
	}

	if ( msg.signon_state() > SIGNONSTATE_CONNECTED )
	{
		if ( msg.spawn_count() != (uint32)m_Server->GetSpawnCount() )
		{
			Reconnect();
			return true;
		}
	}

	// client must acknowledge our current state, otherwise start again
	if ( msg.signon_state() != (uint32)GetSignonState() )
	{
		Reconnect();
		return true;
	}

	return ProcessSignonStateMsg( msg.signon_state(), msg.spawn_count() );
}

bool CBaseClient::CLCMsg_ClientInfo( const CCLCMsg_ClientInfo& msg )
{
	m_nSendtableCRC = msg.send_table_crc();

	m_bIsHLTV = msg.is_hltv();

#if defined( REPLAY_ENABLED )
	m_bIsReplay = msg.is_replay();
#endif

	m_nFilesDownloaded = 0;
	Q_strncpy( m_FriendsName, msg.friends_name().c_str(), sizeof(m_FriendsName) );

	for ( int i=0; i<MAX_CUSTOM_FILES; i++ ) 
	{
		CRC32_t crc = ( i < msg.custom_files_size() ) ? msg.custom_files( i ) : 0;

		m_nCustomFiles[i].crc = crc;
		m_nCustomFiles[i].reqID = 0;
	}

	if ( msg.server_count() != ( uint32 )m_Server->GetSpawnCount() )
	{
		Reconnect();	// client still in old game, reconnect
	}

	return true;
}

bool CBaseClient::CLCMsg_LoadingProgress( const CCLCMsg_LoadingProgress& msg )
{
	m_nLoadingProgress = msg.progress();
	return true;
}

bool CBaseClient::CLCMsg_BaselineAck( const CCLCMsg_BaselineAck& msg )
{
	if ( msg.baseline_tick() != m_nBaselineUpdateTick )
	{
		// This occurs when there are multiple ack's queued up for processing from a client.
		return true;
	}

	if ( msg.baseline_nr() != m_nBaselineUsed )
	{
		DevMsg("CBaseClient::ProcessBaselineAck: wrong baseline nr received (%i)\n", msg.baseline_tick() );
		return true;
	}

	Assert( m_pBaseline );	

	// copy ents send as full updates this frame into baseline stuff
	CClientFrame *frame = GetDeltaFrame( m_nBaselineUpdateTick );
	if ( frame == NULL )
	{
		// Will get here if we have a lot of packet loss and finally receive a stale ack from 
		//  remote client.  Our "window" could be well beyond what it's acking, so just ignore the ack.
		DevMsg( "Dropping baseline ack %d\n", m_nBaselineUpdateTick );
		return true;
	}

	CFrameSnapshot *pSnapshot = frame->GetSnapshot();

	if ( pSnapshot == NULL )
	{
		// TODO if client lags for a couple of seconds the snapshot is lost
		// fix: don't remove snapshots that are labled a possible basline candidates
		// or: send full update
		DevMsg("CBaseClient::ProcessBaselineAck: invalid frame snapshot (%i)\n", m_nBaselineUpdateTick );
		return false;
	}

	int index = m_BaselinesSent.FindNextSetBit( 0 );

	while ( index >= 0 )
	{
		// get new entity
		PackedEntityHandle_t hNewEntity = pSnapshot->m_pEntities[index].m_pPackedData;
		if ( hNewEntity == INVALID_PACKED_ENTITY_HANDLE )
		{
			DevMsg("CBaseClient::ProcessBaselineAck: invalid packet handle (%i)\n", index );
			return false;
		}

		PackedEntityHandle_t hOldEntity = m_pBaseline->m_pEntities[index].m_pPackedData;

		if ( hOldEntity != INVALID_PACKED_ENTITY_HANDLE )
		{
			// remove reference before overwriting packed entity
			framesnapshotmanager->RemoveEntityReference( hOldEntity );
		}

		// increase reference
		framesnapshotmanager->AddEntityReference( hNewEntity );

		// copy entity handle, class & serial number to
		m_pBaseline->m_pEntities[index] = pSnapshot->m_pEntities[index];

		// go to next entity
		index = m_BaselinesSent.FindNextSetBit( index + 1 );
	}

	m_pBaseline->m_nTickCount = m_nBaselineUpdateTick;

	// flip used baseline flag
	m_nBaselineUsed = (m_nBaselineUsed==1)?0:1;

	m_nBaselineUpdateTick = -1; // ready to update baselines again

	return true;
}

bool CBaseClient::CLCMsg_ListenEvents( const CCLCMsg_ListenEvents& msg )
{
	// first remove the client as listener
	g_GameEventManager.RemoveListener( this );

	CBitVec<MAX_EVENT_NUMBER> EventArray;

	for( int i = 0; i < msg.event_mask_size(); i++ )
	{
		EventArray.SetDWord( i, msg.event_mask( i ) );
	}

	int index = EventArray.FindNextSetBit( 0 );
	while( index >= 0 )
	{
		CGameEventDescriptor *descriptor = g_GameEventManager.GetEventDescriptor( index );

		if ( descriptor )
		{
			g_GameEventManager.AddListener( this, descriptor, CGameEventManager::CLIENTSTUB );
		}
		else
		{
			DevMsg("ProcessListenEvents: game event %i not found.\n", index );
			return false;
		}

		index = EventArray.FindNextSetBit( index + 1 );
	}

	return true;
}



bool CBaseClient::IsTracing() const
{
	return m_Trace.m_nMinWarningBytes != 0;
}

void CBaseClient::StartTrace( bf_write &msg )
{
	if ( !IsTracing() )
		return;
	m_Trace.m_nStartBit = msg.GetNumBitsWritten();
	m_Trace.m_nCurBit = m_Trace.m_nStartBit;
}

#define SERVER_PACKETS_LOG	"netspike.txt"

void CBaseClient::EndTrace( bf_write &msg )
{
	if ( !IsTracing() )
		return;

	int bits = m_Trace.m_nCurBit - m_Trace.m_nStartBit;
	if ( bits < ( m_Trace.m_nMinWarningBytes << 3 ) )
	{
		m_Trace.m_Records.RemoveAll();
		return;
	}

	CNetChan *chan = static_cast< CNetChan * >( m_NetChannel );
	if ( chan )
	{
		int bufReliable = chan->GetBuffer( CNetChan::BUF_RELIABLE ).GetNumBitsWritten();
		 int bufUnreliable = chan->GetBuffer( CNetChan::BUF_UNRELIABLE ).GetNumBitsWritten();
		 int bufVoice = chan->GetBuffer( CNetChan::BUF_VOICE ).GetNumBitsWritten();

		 TraceNetworkMsg( bufReliable, "[Reliable payload]" );
		 TraceNetworkMsg( bufUnreliable, "[Unreliable payload]" );
		 TraceNetworkMsg( bufVoice, "[Voice payload]" );
	}

	CUtlBuffer logData( 0, 0, CUtlBuffer::TEXT_BUFFER );

	logData.Printf( "%f/%d Player [%s][%d][adr:%s] was sent a datagram %d bits (%8.3f bytes)\n",
		realtime, 
		host_tickcount,
		GetClientName(), 
		GetPlayerSlot(), 
		GetNetChannel()->GetAddress(),
		bits, (float)bits / 8.0f );

	const int WriteSize = 10*1024;
	for ( int i = 0 ; i < m_Trace.m_Records.Count() ; ++i )
	{
		Spike_t &sp = m_Trace.m_Records[ i ];
		logData.Printf( "%64.64s : %8d bits (%8.3f bytes)\n", sp.m_szDesc, sp.m_nBits, (float)sp.m_nBits / 8.0f );
		if ( logData.TellPut() > WriteSize && i != m_Trace.m_Records.Count()-1 )
		{
			COM_LogString( SERVER_PACKETS_LOG, (char *)logData.Base() );
			logData.Clear();
		}
	}

	COM_LogString( SERVER_PACKETS_LOG, (char *)logData.Base() );
	m_Trace.m_Records.RemoveAll();
}

void CBaseClient::TraceNetworkData( bf_write &msg, char const *fmt, ... )
{
	if ( !IsTracing() )
		return;
	char buf[ 64 ];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );

	Spike_t t;
	Q_strncpy( t.m_szDesc, buf, sizeof( t.m_szDesc ) );
	t.m_nBits = msg.GetNumBitsWritten() - m_Trace.m_nCurBit;
	m_Trace.m_Records.AddToTail( t );
	m_Trace.m_nCurBit = msg.GetNumBitsWritten();
}

void CBaseClient::TraceNetworkMsg( int nBits, char const *fmt, ... )
{
	if ( !IsTracing() )
		return;
	char buf[ 64 ];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );

	Spike_t t;
	Q_strncpy( t.m_szDesc, buf, sizeof( t.m_szDesc ) );
	t.m_nBits = nBits;
	m_Trace.m_Records.AddToTail( t );
}

void CBaseClient::SetTraceThreshold( int nThreshold )
{
	m_Trace.m_nMinWarningBytes = nThreshold;
}

static ConVar sv_multiplayer_maxtempentities( "sv_multiplayer_maxtempentities", "32" );
ConVar sv_multiplayer_maxsounds( "sv_multiplayer_sounds", "20" );

bool CBaseClient::SendSnapshot( CClientFrame *pFrame )
{
    SNPROF( "SendSnapshot" );

	// never send the same snapshot twice
	if ( m_pLastSnapshot == pFrame->GetSnapshot() )
	{
		m_NetChannel->Transmit();	
		return false;
	}

	// if we send a full snapshot (no delta-compression) before, wait until client
	// received and acknowledge that update. don't spam client with full updates
	if ( m_nForceWaitForTick > 0 )
	{
		// just continue transmitting reliable data
		m_NetChannel->Transmit();	
		return false;
	}

	VPROF_BUDGET( "SendSnapshot", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	net_scratchbuffer_t scratch;
	bf_write msg( "CBaseClient::SendSnapshot",
		scratch.GetBuffer(), scratch.Size() );

	TRACE_PACKET( ( "SendSnapshot(%d)\n", pFrame->tick_count ) );

	// now create client snapshot packet
	CClientFrame * deltaFrame = m_nDeltaTick < 0 ? NULL : GetDeltaFrame( m_nDeltaTick ); // NULL if delta_tick is not found
	if ( !deltaFrame )
	{
		// We need to send a full update and reset the instanced baselines
		char reason[ 128 ];
		Q_snprintf( reason, sizeof( reason ), "%s can't find frame from tick %d", GetClientName(), m_nDeltaTick );
		OnRequestFullUpdate( reason );
	}

	if ( IsTracing() )
	{
		StartTrace( msg );
	}

	// send tick time
	{
		CNETMsg_Tick_t tickmsg( pFrame->tick_count, host_frameendtime_computationduration, host_frametime_stddeviation, host_framestarttime_stddeviation );
		if ( !tickmsg.WriteToBuffer( msg ) )
		{
			Disconnect( "#GameUI_Disconnect_TickMessage" );
			return false;
		}
	}

	if ( IsTracing() )
	{
		TraceNetworkData( msg, "NET_Tick" );
	}

#ifndef SHARED_NET_STRING_TABLES
	// in LocalNetworkBackdoor mode we updated the stringtables already in SV_ComputeClientPacks()
	if ( !g_pLocalNetworkBackdoor )
	{
		// Update shared client/server string tables. Must be done before sending entities
		m_Server->m_StringTables->WriteUpdateMessage( this, GetMaxAckTickCount(), msg );
	}
#endif

	int nDeltaStartBit = 0;
	if ( IsTracing() )
	{
		nDeltaStartBit = msg.GetNumBitsWritten();
	}

	// send entity update, delta compressed if deltaFrame != NULL
	{
		m_Server->WriteDeltaEntities( this, pFrame, deltaFrame, m_packetmsg );
		if ( !m_packetmsg.WriteToBuffer( msg ) )
		{
			Disconnect( "#GameUI_Disconnect_DeltaEntMessage" );
			return false;
		}
	}

	if ( IsTracing() )
	{
		int nBits = msg.GetNumBitsWritten() - nDeltaStartBit;
		TraceNetworkMsg( nBits, "Total Delta" );
	}
			
	// send all unreliable temp entities between last and current frame
	// send max 64 events in multi player, 255 in SP
	int nMaxTempEnts = m_Server->IsMultiplayer() ? sv_multiplayer_maxtempentities.GetInt() : 255;
	m_Server->WriteTempEntities( this, pFrame->GetSnapshot(), m_pLastSnapshot.GetObject(), m_tempentsmsg, nMaxTempEnts );
	if ( m_tempentsmsg.num_entries() )
	{
		m_tempentsmsg.WriteToBuffer( msg );
	}

	if ( IsTracing() )
	{
		TraceNetworkData( msg, "Temp Entities" );
	}

	int nMaxSounds = m_Server->IsMultiplayer() ? sv_multiplayer_maxsounds.GetInt() : 255;
	WriteGameSounds( msg, nMaxSounds );
	
	if ( IsTracing() )
	{
		TraceNetworkMsg( 0, "Finished [delta %s]", deltaFrame ? "yes" : "no" );
		EndTrace( msg );
	}

	// write message to packet and check for overflow
	if ( msg.IsOverflowed() )
	{
		if ( !deltaFrame )
		{
			// if this is a reliable snapshot, drop the client
			Disconnect( "ERROR! Reliable snaphsot overflow." );
			return false;
		}
		else
		{
			// unreliable snapshots may be dropped
			ConMsg ("WARNING: msg overflowed for %s\n", m_Name);
			msg.Reset();
		}
	}

	// remember this snapshot
	m_pLastSnapshot = pFrame->GetSnapshot();

	// Don't send the datagram to fakeplayers unless sv_stressbots is on (which will make m_NetChannel non-null).
	if ( m_bFakePlayer && !m_NetChannel )
	{
		m_nDeltaTick = pFrame->tick_count;
		m_nStringTableAckTick = m_nDeltaTick;
		return true;
	}

	bool bSendOK;

	// is this is a full entity update (no delta) ?
	if ( !deltaFrame )
	{
		// transmit snapshot as reliable data chunk
		bSendOK = m_NetChannel->SendData( msg );
		bSendOK = bSendOK && m_NetChannel->Transmit();

		// remember this tickcount we send the reliable snapshot
		// so we can continue sending other updates if this has been acknowledged
		m_nForceWaitForTick = pFrame->tick_count;
	}
	else
	{
		// just send it as unreliable snapshot
		bSendOK = m_NetChannel->SendDatagram( &msg ) > 0;
	}
		
	if ( !bSendOK )
	{
		Disconnect( "ERROR! Couldn't send snapshot." );
		return false;
	}
	return true;
}

bool CBaseClient::ExecuteStringCommand( const char *pCommand )
{
	if ( !pCommand || !pCommand[0] )
		return false;

	if ( !Q_stricmp( pCommand, "demorestart" ) )
	{
		DemoRestart();
		// trick, dont return true, so serverGameClients gets this command too
		return false; 
	}

	return false;
}

void CBaseClient::DemoRestart()
{
	
}

bool CBaseClient::ShouldSendMessages( void )
{
	if ( !IsConnected() )
		return false;

	// if the reliable message overflowed, drop the client
	if ( m_NetChannel && m_NetChannel->IsOverflowed() )
	{
		m_NetChannel->Reset();
		Disconnect( CFmtStr( "%s overflowed reliable buffer\n", m_Name ) );
		return false;
	}

	// check, if it's time to send the next packet
	bool bSendMessage = m_fNextMessageTime <= net_time ;

	// don't throttle loopback connections
	if ( m_NetChannel && m_NetChannel->IsLoopback() )
	{
		bSendMessage = true;
	}

	if ( !bSendMessage && !IsActive() )
	{
		// if we are in signon modem instantly reply if
		// we got a answer and have reliable data waiting
		if ( m_bReceivedPacket && m_NetChannel && m_NetChannel->HasPendingReliableData() )
		{
			bSendMessage = true;
		}
	}

	if ( bSendMessage && m_NetChannel && !m_NetChannel->CanPacket() )
	{
		// we would like to send a message, but bandwidth isn't available yet
		// tell netchannel that we are choking a packet
		m_NetChannel->SetChoked();	
		// Record an ETW event to indicate that we are throttling.
		ETWThrottled();
		bSendMessage = false;
	}

	return bSendMessage;
}

void CBaseClient::UpdateSendState( void )
{
	// wait for next incoming packet
	m_bReceivedPacket = false;

	// in single player mode always send messages
	if ( !m_Server->IsMultiplayer() && !host_limitlocal.GetFloat() )
	{
		m_fNextMessageTime = net_time; // send ASAP and 
		m_bReceivedPacket = true;	// don't wait for incoming packets
	}
	else if ( IsActive() )	// multiplayer mode
	{
		// snapshot mode: send snapshots frequently
		float maxDelta = MIN( m_Server->GetTickInterval(), m_fSnapshotInterval );
		float delta = clamp( net_time - m_fNextMessageTime, 0.0f, maxDelta );
		m_fNextMessageTime = net_time + m_fSnapshotInterval - delta;
	}
	else // multiplayer signon mode
	{
		if ( m_NetChannel && m_NetChannel->HasPendingReliableData() && 
			m_NetChannel->GetTimeSinceLastReceived() < 1.0f )
		{
			// if we have pending reliable data send as fast as possible
			m_fNextMessageTime = net_time;
		}
		else
		{
			// signon mode: only respond on request or after 1 second
			m_fNextMessageTime = net_time + 1.0f;
		}
	}
}

void CBaseClient::UpdateUserSettings()
{
	// set user name
	SetName( m_ConVars->GetString( "name", "unnamed") );

	// set server to client network rate
	SetRate( m_ConVars->GetInt( "rate", DEFAULT_RATE ), false );

	// set server to client update rate
	SetUpdateRate( m_ConVars->GetFloat( "cl_updaterate", 64 ), false );

	SetMaxRoutablePayloadSize( m_ConVars->GetInt( "net_maxroutable", MAX_ROUTABLE_PAYLOAD ) );

	m_Server->UserInfoChanged( m_nClientSlot );

	m_bConVarsChanged = false;
}

void CBaseClient::OnRequestFullUpdate( char const *pchReason )
{
	// client requests a full update 
	m_pLastSnapshot = NULL;

	// free old baseline snapshot
	FreeBaselines();

	// and create new baseline snapshot
	m_pBaseline = framesnapshotmanager->CreateEmptySnapshot( 
#ifdef DEBUG_SNAPSHOT_REFERENCES
		CFmtStr( "CBaseClient[%d,%s]::OnRequestFullUpdate(%s)", m_nClientSlot, GetClientName(), pchReason ).Access(),
#endif
		0, MAX_EDICTS );

	DevMsg("Sending full update to Client %s (%s)\n", GetClientName(), pchReason );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cl - 
//-----------------------------------------------------------------------------
bool CBaseClient::UpdateAcknowledgedFramecount(int tick)
{
	if ( IsFakeClient() )
	{
		// fake clients are always fine
		m_nDeltaTick = tick; 
		m_nStringTableAckTick = tick;
		return true;
	}

	// are we waiting for full reliable update acknowledge
	if ( m_nForceWaitForTick > 0 )
	{
		if ( tick > m_nForceWaitForTick )
		{
			// we should never get here since full updates are transmitted as reliable data now
			ConDMsg( "Acknowledging reliable snapshot failed: ack %d while waiting for %d.\n", tick, m_nForceWaitForTick );
			return true;
		}
		else if ( tick == -1 )
		{
			if( !m_NetChannel->HasPendingReliableData() )
			{
				// that's strange: we sent the client a full update, and it was fully received ( no reliable data in waiting buffers )
				// but the client is requesting another full update.
				//
				// This can happen if they request full updates in succession really quickly (using cl_fullupdate or "record X;stop" quickly).
				// There was a bug here where if we just return out, the client will have nuked its entities and we'd send it
				// a supposedly uncompressed update but m_nDeltaTick was not -1, so it was delta'd and it'd miss lots of stuff.
				// Led to clients getting full spectator mode radar while their player was not a spectator.
				ConDMsg("Client forced immediate full update.\n");
				m_nForceWaitForTick = m_nDeltaTick = -1;
				OnRequestFullUpdate( "forced immediate full update" );
				return true;
			}
		}
		else if ( tick < m_nForceWaitForTick )
		{
			// keep on waiting, do nothing
			return true;
		}
		else // ( tick == m_nForceWaitForTick )
		{
			// great, the client acknowledge the tick we send the full update
			m_nForceWaitForTick = -1; 
			// continue sending snapshots...
		}	 
	}
	else
	{
		if ( m_nDeltaTick == -1 )
		{
			// we still want to send a full update, don't change delta_tick from -1
			return true;
		}

		if ( tick == -1 )
		{
			OnRequestFullUpdate( "client ack'd -1" );
		}
		else
		{
			if ( m_nDeltaTick > tick )
			{
				// client already acknowledged new tick and now switch back to older
				// thats not allowed since we always delete older frames
				Disconnect("Client delta ticks out of order.\n");
				return false;
			}
		}
	}

	// get acknowledged client frame
	m_nDeltaTick = tick; 

	if ( m_nDeltaTick > -1 )
	{
		m_nStringTableAckTick = m_nDeltaTick;
	}

	if ( (m_nBaselineUpdateTick > -1) && (m_nDeltaTick > m_nBaselineUpdateTick) )
	{
		// server sent a baseline update, but it wasn't acknowledged yet so it was probably lost. 
		m_nBaselineUpdateTick = -1;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: return a string version of the userid
//-----------------------------------------------------------------------------
const char *GetUserIDString( const USERID_t& id )
{
	static char idstr[ MAX_NETWORKID_LENGTH ];

	idstr[ 0 ] = 0;

	switch ( id.idtype )
	{
	case IDTYPE_STEAM:
		{
			TSteamGlobalUserID nullID;
			Q_memset( &nullID, 0, sizeof( TSteamGlobalUserID ) );

			if ( Steam3Server().BLanOnly() && !Q_memcmp( &id.uid.steamid, &nullID, sizeof( TSteamGlobalUserID ) ) ) 
			{
				strcpy( idstr, "STEAM_ID_LAN" );
			}
			else if ( !Q_memcmp( &id.uid.steamid, &nullID, sizeof( TSteamGlobalUserID ) ))
			{
				strcpy( idstr, "STEAM_ID_PENDING" );
			}
			else
			{			
				Q_snprintf( idstr, sizeof( idstr ) - 1, "STEAM_%u:%u:%u", (SteamInstanceID_t)id.uid.steamid.m_SteamInstanceID, 
													(unsigned int)((SteamLocalUserID_t)id.uid.steamid.m_SteamLocalUserID.Split.High32bits), 
													(unsigned int)((SteamLocalUserID_t)id.uid.steamid.m_SteamLocalUserID.Split.Low32bits ));			
				idstr[ sizeof( idstr ) - 1 ] = '\0';
			}
		}
		break;		
	case IDTYPE_HLTV:
		{
			strcpy( idstr, "HLTV" );
		}
		break;
	case IDTYPE_REPLAY:
		{
			strcpy( idstr, "REPLAY" );
		}
		break;
	default:
		{
			strcpy( idstr, "UNKNOWN" );
		}
		break;
	}

	return idstr;
}

//-----------------------------------------------------------------------------
// Purpose: return a string version of the userid
//-----------------------------------------------------------------------------
const char *CBaseClient::GetNetworkIDString() const
{
	if ( IsFakeClient() )
	{
		return "BOT";
	}

#if defined( _X360 )
	if ( m_ConVars )
#elif defined( SERVER_XLSP )
	if ( NET_IsDedicatedForXbox() && m_ConVars )
#else
	if ( 0 )
#endif
	{
		const char * value = m_ConVars->GetString( "networkid_force", "" );
		if ( value && *value )
			return value;
	}

	return ( GetUserIDString( GetNetworkID() ) );
}

uint64 CBaseClient::GetClientXuid() const
{
	// For 2nd SS player IsFakeClient() == true, so need to short-circuit it straight into forced network_id -- Vitaliy
	const char * value = NULL;

#if defined( _X360 )
	if ( m_ConVars )
#elif defined( SERVER_XLSP )
	if ( NET_IsDedicatedForXbox() && m_ConVars )
#else
	if ( 0 )
#endif
	{
		value = m_ConVars->GetString( "networkid_force", NULL );
	}

	if ( value && *value && strlen( value ) > 10 )
		return ( uint64( strtoul( value, NULL, 16 ) ) << 32 ) | uint64( strtoul( value + 9, NULL, 16 ) );

	if ( IsFakeClient() )
		return 0ull;
	else
		return m_SteamID.ConvertToUint64();
}

bool CBaseClient::IgnoreTempEntity( CEventInfo *event )
{
	int iPlayerIndex = GetPlayerSlot()+1;

	return !event->filter.IncludesPlayer( iPlayerIndex );
}

const USERID_t CBaseClient::GetNetworkID() const
{
	USERID_t userID;

	m_SteamID.ConvertToSteam2( &userID.uid.steamid );

	userID.idtype = IDTYPE_STEAM; 
	userID.uid.steamid.m_SteamInstanceID = 1;

	return userID;
}

void CBaseClient::SetSteamID( const CSteamID &steamID )
{
	m_SteamID = steamID;
}

void CBaseClient::SetMaxRoutablePayloadSize( int nMaxRoutablePayloadSize )
{
	if ( m_NetChannel )
	{
		m_NetChannel->SetMaxRoutablePayloadSize( nMaxRoutablePayloadSize );
	}
}

int CBaseClient::GetMaxAckTickCount() const
{
	int nMaxTick = m_nSignonTick;
	if ( m_nDeltaTick > nMaxTick )
	{
		nMaxTick = m_nDeltaTick;
	}
	if ( m_nStringTableAckTick > nMaxTick )
	{
		nMaxTick = m_nStringTableAckTick;
	}
	return nMaxTick;
}

int CBaseClient::GetAvailableSplitScreenSlot() const
{
	for ( int i = 1; i < host_state.max_splitscreen_players; ++i )
	{
		if ( m_SplitScreenUsers[ i ] )
			continue;
		return i;
	}

	return -1;
}

void CBaseClient::SendFullConnectEvent()
{
	IGameEvent *event = g_GameEventManager.CreateEvent( "player_connect_full" );
	if ( event )
	{
		event->SetInt( "userid", m_UserID );
		event->SetInt( "index", m_nClientSlot );
		g_GameEventManager.FireEvent( event );
	}
}

void CBaseClient::FillSignOnFullServerInfo( CNETMsg_SignonState_t &state )
{
	//
	//	FillSignOnFullServerInfo
	//		fills the signon state message with full information about
	//		server clients connected to it at the moment
	//

	if ( IsX360() || sv.IsDedicatedForXbox() )
	{
		//
		//	We only do this on X360 listen server and X360-dedicated server
		//
		state.set_num_server_players (sv.GetClientCount());
		for ( int j = 0 ; j < sv.GetClientCount() ; j++ )
		{
			IClient *client = sv.GetClient( j );
	
			char const *szNetworkId = client->GetNetworkIDString();
			
			state.add_players_networkids( szNetworkId );
		}		
	}

	const char *pMapname = HostState_GetNewLevel();
	state.set_map_name( pMapname ? pMapname : "" );
}

struct SessionClient_t
{
	uint64 xSession;
	int numPlayers;
	CCopyableUtlVector< IClient * > arrClients;

	bool operator == ( const SessionClient_t & x ) const
	{
		return xSession == x.xSession;
	}

	static int Less( const SessionClient_t *a, const SessionClient_t *b )
	{
		// Groups with invalid session id should absolutely
		// get dropped
		if ( a->xSession != b->xSession )
		{
			if ( !a->xSession )
				return 1;
			if ( !b->xSession )
				return -1;
		}

		// Keep more players if possible
		if ( a->numPlayers != b->numPlayers )
			return ( a->numPlayers > b->numPlayers ) ? -1 : 1;

		// Keep more clients if possible
		if ( a->arrClients.Count() != b->arrClients.Count() )
			return ( a->arrClients.Count() > b->arrClients.Count() ) ? -1 : 1;

		// Prefer to preserve the clients that have the original
		// reservation session (that's the lobby leader)
		if ( a->xSession != b->xSession )
		{
			if ( a->xSession == sv.GetReservationCookie() )
				return -1;
			if ( b->xSession == sv.GetReservationCookie() )
				return 1;
		}

		// Otherwise keep the client that came first
		return ( a->arrClients[0]->GetUserID() < b->arrClients[0]->GetUserID() ) ? -1 : 1;
	}
};

void HostValidateSessionImpl()
{
	if ( !sv.IsDedicatedForXbox() )
		return;

	Msg( "[SESSION] Validating Session Information...\n" );

	CUtlVector< SessionClient_t > arrSessions;

	//
	//	Collect all connected clients by their sessions
	//
	for ( int j = 0 ; j < sv.GetClientCount() ; j++ )
	{
		IClient *client = sv.GetClient( j );
		if( !client )
			continue;
		if ( !client->IsConnected() )
			continue;
		if ( client->IsFakeClient() )
			continue;
		if ( client->IsSplitScreenUser() )
			continue;

		// Now we have a client who is a real human client
		const char *szSession = client->GetUserSetting( "cl_session" );
		uint64 uid = 0;
		if ( sscanf( szSession, "$%llx", &uid ) != 1 )
		{
			Warning( "couldn't parse cl_session %s\n", szSession );
		}

		SessionClient_t sc;
		sc.xSession = uid;
		sc.numPlayers = 0;

		int idx = arrSessions.Find( sc );
		if ( idx == arrSessions.InvalidIndex() )
		{
			idx = arrSessions.AddToTail( sc );
		}
		arrSessions[idx].arrClients.AddToTail( client );
		arrSessions[idx].numPlayers += client->GetNumPlayers();
	}

	//
	//	Sort the sessions
	//
	if ( !arrSessions.Count() )
	{
		Msg( "[SESSION] No clients.\n" );
		return;
	}

	arrSessions.Sort( &SessionClient_t::Less );

	//
	//	Set the new reservation cookie and drop the rest
	//
	int iDropIndex = 0;
	if ( arrSessions[0].xSession )
	{
		Msg( "[SESSION] Updating reservation cookie: %llx, keeping %d players.\n",
			arrSessions[0].xSession, arrSessions[0].numPlayers );
		sv.SetReservationCookie( arrSessions[0].xSession, "HostValidateSession" );
		iDropIndex = 1;
	}

	for ( int k = iDropIndex; k < arrSessions.Count(); ++ k )
	{
		for ( int clIdx = 0; clIdx < arrSessions[k].arrClients.Count(); ++ clIdx )
		{
			arrSessions[k].arrClients[clIdx]->Disconnect( "Session migrated" );
		}
	}
}

bool CBaseClient::CheckConnect()
{
	return true;
}

bool CBaseClient::CLCMsg_SplitPlayerConnect( const CCLCMsg_SplitPlayerConnect& msg )
{
	int slot = GetAvailableSplitScreenSlot();
	if ( slot == -1 )
	{
		Warning( "no more split screen slots!\n" );
		Disconnect( "No more split screen slots!" );
		return true;
	}

	CBaseClient *pSplitClient = sv.CreateSplitClient( msg.convars(), this );
	if ( pSplitClient )
	{
		Assert( pSplitClient->m_bSplitScreenUser );
		pSplitClient->m_nSplitScreenPlayerSlot = slot;

		Assert( slot < ARRAYSIZE(m_SplitScreenUsers) );
		m_SplitScreenUsers[ slot ] = pSplitClient;
		
		CSVCMsg_SplitScreen_t splitscreenmsg;
		splitscreenmsg.set_player_index( pSplitClient->m_nEntityIndex );
		splitscreenmsg.set_slot( slot );
		splitscreenmsg.set_type( MSG_SPLITSCREEN_ADDUSER );

		m_NetChannel->AttachSplitPlayer( slot, pSplitClient->m_NetChannel );

		SendNetMsg( splitscreenmsg, true );

		if ( pSplitClient->m_pAttachedTo->IsActive() )
		{
			//only activate if the main player is in a state where we would want to, otherwise the client will take care of it later.
			pSplitClient->ActivatePlayer();
		}
	}

	return true;
}

bool CBaseClient::CLCMsg_CmdKeyValues( const CCLCMsg_CmdKeyValues& msg )
{
	return true;
}

void CBaseClient::SplitScreenDisconnect( const CCommand &args )
{
	// Fixme, this will work for 2 players, but not 4 right now
	int nSlot = 1;
	if ( args.ArgC() > 1 )
	{
		nSlot = Q_atoi( args.Arg( 1 ) );
	}

	if ( nSlot <= 0 )
		nSlot = 1;

	if ( m_SplitScreenUsers[ nSlot ] != NULL )
	{
		CBaseClient *pSplitClient = m_SplitScreenUsers[ nSlot ];
		DisconnectSplitScreenUser( pSplitClient );
	}
}

void CBaseClient::DisconnectSplitScreenUser( CBaseClient *pSplitClient )
{
	sv.QueueSplitScreenDisconnect( this, pSplitClient );
	CSVCMsg_SplitScreen_t msg;
	msg.set_player_index( pSplitClient->m_nEntityIndex );
	msg.set_slot( pSplitClient->m_nSplitScreenPlayerSlot );
	msg.set_type( MSG_SPLITSCREEN_REMOVEUSER );

	m_NetChannel->DetachSplitPlayer( pSplitClient->m_nSplitScreenPlayerSlot );

	SendNetMsg( msg, true );
}

bool CBaseClient::ChangeSplitscreenUser( int nSplitScreenUserSlot )
{
	if ( IsSplitScreenUser() )
	{
		return m_pAttachedTo->ChangeSplitscreenUser( nSplitScreenUserSlot );
	}

	int other = nSplitScreenUserSlot;
	
	bool success = false;
	if ( other == -1 )
	{
		// Revert to self
		success = m_NetChannel->SetActiveChannel( m_NetChannel );
	}
	else
	{
		if ( ( other >= 0 ) && ( other < ARRAYSIZE(m_SplitScreenUsers) ) && m_SplitScreenUsers[ other ] )
		{
			success = m_NetChannel->SetActiveChannel( m_SplitScreenUsers[ other ]->m_NetChannel );
		}
	}

	if ( !success )
	{
		if ( !NET_IsDedicated() )
		{
			Msg( "Unable to set SetActiveChannel to user in slot %d\n", nSplitScreenUserSlot );
		}
		Assert( 0 );
		return false;
	}
	return true;
}

bool CBaseClient::IsSplitScreenPartner( const CBaseClient *pOther ) const
{
	if ( !pOther )
		return false;

	if ( pOther->IsSplitScreenUser() && 
		pOther->m_pAttachedTo == this )
		return true;

	if ( IsSplitScreenUser() && 
		m_pAttachedTo == pOther )
		return true;

	return false;
}

int CBaseClient::GetNumPlayers()
{
	if ( IsSplitScreenUser() )
	{
		if ( m_pAttachedTo )
			return m_pAttachedTo->GetNumPlayers();
		else
			return 0;
	}
	else
	{
		int numPlayers = 0;
		for ( int k = 0; k < ARRAYSIZE( m_SplitScreenUsers ); ++ k )
		{
			if ( m_SplitScreenUsers[k] )
				++ numPlayers;
		}
		return numPlayers;
	}
}

// Is an actual human player or splitscreen player (not a bot and not a HLTV slot)
bool CBaseClient::IsHumanPlayer() const
{
	if ( !IsConnected() )
		return false;

	if ( IsHLTV() )
		return false;

	if ( IsFakeClient() && !IsSplitScreenUser() )
		return false;

	return true;
}

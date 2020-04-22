//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "mm_framework.h"

#ifndef _X360
#include "xbox/xboxstubs.h"
#endif

#include "smartptr.h"
#include "utlvector.h"

#include "x360_lobbyapi.h"
#include "leaderboards.h"

#include "igameevents.h"

#include "GameUI/IGameUI.h"
#include "filesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define STEAM_PACK_BITFIELDS

ConVar cl_names_debug( "cl_names_debug", "0", FCVAR_DEVELOPMENTONLY );
#define PLAYER_DEBUG_NAME "WWWWWWWWWWWWWWW"

#ifndef NO_STEAM
static float s_flSteamStatsRequestTime = 0;
static bool s_bSteamStatsRequestFailed = false;
bool g_bSteamStatsReceived = false;
#endif

static DWORD GetTitleSpecificDataId( int idx )
{
	static DWORD arrTSDI[3] = {
		XPROFILE_TITLE_SPECIFIC1,
		XPROFILE_TITLE_SPECIFIC2,
		XPROFILE_TITLE_SPECIFIC3
	};

	if ( idx >= 0 && idx < ARRAYSIZE( arrTSDI ) )
		return arrTSDI[idx];

	return DWORD(-1);
}

static int GetTitleSpecificDataIndex( DWORD TSDataId )
{
	switch( TSDataId )
	{
	case XPROFILE_TITLE_SPECIFIC1: return 0;
	case XPROFILE_TITLE_SPECIFIC2: return 1;
	case XPROFILE_TITLE_SPECIFIC3: return 2;
	default: return -1;
	}
}

static MM_XWriteOpportunity s_arrXWO[ XUSER_MAX_COUNT ]; // rely on static memory being zero'd

#ifdef _X360
CUtlVector< PlayerLocal::XPendingAsyncAward_t * > PlayerLocal::s_arrPendingAsyncAwards;
#endif

void SignalXWriteOpportunity( MM_XWriteOpportunity eXWO )
{
	if ( !eXWO )
	{
		Warning( "SignalXWriteOpportunity called with MMXWO_NONE!\n" );
		return;
	}
	else
	{
		Msg( "SignalXWriteOpportunity(%d)\n", eXWO );
	}

	// In case session has just started we bump every player's last write time
	// to the current time since player shouldn't write within the first 5 mins
	// from when the session started (TCR)
	if ( eXWO == MMXWO_SESSION_STARTED )
	{
		for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
		{
			IPlayerLocal *pLocalPlayer = g_pPlayerManager->GetLocalPlayer( k );
			if ( PlayerLocal *pPlayer = dynamic_cast< PlayerLocal * >( pLocalPlayer ) )
			{
				pPlayer->SetTitleDataWriteTime( Plat_FloatTime() );
			}
		}
		return;
	}

	for ( int k = 0; k < ARRAYSIZE( s_arrXWO ); ++ k )
	{
		// Only elevate write opportunity:
		// this way any other code can signal CHECKPOINT after SESSION_FINISHED
		// and the write will happen as SESSION_FINISHED
		if ( s_arrXWO[k] < eXWO )
			s_arrXWO[k] = eXWO;
	}
}

MM_XWriteOpportunity GetXWriteOpportunity( int iCtrlr )
{
	if ( iCtrlr >= 0 && iCtrlr < ARRAYSIZE( s_arrXWO ) )
	{
		MM_XWriteOpportunity result = s_arrXWO[ iCtrlr ];
		s_arrXWO[ iCtrlr ] = MMXWO_NONE; // reset
		return result;
	}
	else
		return MMXWO_NONE;
}

static CUtlVector< XUID > s_arrSessionSearchesQueue;
static int s_numSearchesOutstanding = 0;

ConVar mm_player_search_count( "mm_player_search_count", "5", FCVAR_DEVELOPMENTONLY );

void PumpSessionSearchQueue()
{
	while ( s_arrSessionSearchesQueue.Count() > 0 &&
		    s_numSearchesOutstanding < mm_player_search_count.GetInt() )
	{
		XUID xid = s_arrSessionSearchesQueue[0];
		s_arrSessionSearchesQueue.Remove( 0 );

		if ( PlayerFriend *player = g_pPlayerManager->FindPlayerFriend( xid ) )
		{
			player->StartSearchForSessionInfoImpl();
		}
	}
}


//
// PlayerFriend implementation
//

PlayerFriend::PlayerFriend( XUID xuid, FriendInfo_t const *pFriendInfo /* = NULL */ ) :
	m_uFriendMark( 0 ),
	m_bIsStale( false ),
	m_eSearchState( SEARCH_NONE ),
	m_pDetails( NULL ),
	m_pPublishedPresence( NULL )
{
	memset( m_wszRichPresence, 0, sizeof( m_wszRichPresence ) );
	memset( &m_xSessionID, 0, sizeof( m_xSessionID ) );
	memset( &m_GameSessionInfo, 0, sizeof( m_GameSessionInfo ) );
	m_uiTitleID = 0;
	m_uiGameServerIP = 0;

#ifdef _X360
	memset( &m_xsiSearchState, 0, sizeof( m_xsiSearchState ) );
	m_pQOS_xnaddr = NULL;
	m_pQOS_xnkid = NULL;
	m_pQOS_xnkey = NULL;
	m_XNQOS = NULL;
	memset( &m_SessionSearchOverlapped, 0, sizeof( m_SessionSearchOverlapped ) );
#endif

	m_xuid = xuid;
	m_eOnlineState = STATE_ONLINE;
	UpdateFriendInfo( pFriendInfo );
}

wchar_t const * PlayerFriend::GetRichPresence()
{
	return m_wszRichPresence;
}

KeyValues * PlayerFriend::GetGameDetails()
{
	return m_pDetails;
}

KeyValues * PlayerFriend::GetPublishedPresence()
{
	return m_pPublishedPresence;
}

bool PlayerFriend::IsJoinable()
{
	if ( m_pPublishedPresence && m_pPublishedPresence->GetString( "connect" )[0] )
		return true; // joining via connect string
	
	if ( !( const uint64 & ) m_xSessionID )
		return false;

	if ( m_pDetails->GetInt( "members/numSlots" ) <= m_pDetails->GetInt( "members/numPlayers" ) )
		return false;
	if ( *m_pDetails->GetString( "system/lock" ) )
		return false;
	if ( !Q_stricmp( "private", m_pDetails->GetString( "system/access" ) ) )
		return false;
	return true; // joining via lobby
}

uint64 PlayerFriend::GetTitleID()
{
	return m_uiTitleID;
}

uint32 PlayerFriend::GetGameServerIP()
{
	return m_uiGameServerIP;
}

void PlayerFriend::Join()
{
	// Requesting to join this player
	KeyValues *pSettings = KeyValues::FromString(
		"settings",
		" system { "
			" network LIVE "
		" } "
		" options { "
			" action joinsession "
		" } "
		);
	
	if ( m_eSearchState == SEARCH_NONE )
	{
		pSettings->SetString( "system/network", m_pDetails->GetString( "system/network", "LIVE" ) );
	}

	pSettings->SetUint64( "options/sessionid", ( const uint64 & ) m_xSessionID );
	pSettings->SetUint64( "options/friendxuid", m_xuid );

#ifdef _X360
	char chSessionInfoBuffer[ XSESSION_INFO_STRING_LENGTH ] = {0};
	MMX360_SessionInfoToString( m_GameSessionInfo, chSessionInfoBuffer );
	pSettings->SetString( "options/sessioninfo", chSessionInfoBuffer );
#endif
	
	KeyValues::AutoDelete autodelete( pSettings );

	g_pMatchFramework->MatchSession( pSettings );
}

void PlayerFriend::Update()
{
	if ( !m_xuid )
		return;

#ifdef _X360
	if( m_eSearchState == SEARCH_XNKID )
	{
		Live_Update_SearchXNKID();
	}

	if ( m_eSearchState == SEARCH_QOS )
	{
		Live_Update_Search_QOS();
	}
#endif

	if ( m_eSearchState == SEARCH_COMPLETED )
	{
		m_eSearchState = SEARCH_NONE;

		-- s_numSearchesOutstanding;
		PumpSessionSearchQueue();

		if( V_memcmp( &( m_GameSessionInfo.sessionID ), &m_xSessionID, sizeof( m_xSessionID ) ) != 0)
		{
			// Re-discover everything again since session ID changed
			StartSearchForSessionInfo();
		}

		// Signal that we are finished with a search
		KeyValues *kvEvent = new KeyValues( "OnMatchPlayerMgrUpdate", "update", "friend" );
		kvEvent->SetUint64( "xuid", GetXUID() );
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvEvent );
	}
}

#ifdef _X360

#ifdef _DEBUG
static ConVar mm_player_delay_xnkid( "mm_player_delay_xnkid", "0", FCVAR_DEVELOPMENTONLY );
static ConVar mm_player_delay_qos( "mm_player_delay_qos", "0", FCVAR_DEVELOPMENTONLY );

static bool ShouldDelayBasedOnTimeThrottling( float &flStaticTimekeeper, float flDelay )
{
	if ( flDelay <= 0.0f )
	{
		flStaticTimekeeper = 0.0f;
		return false;
	}
	else if ( flStaticTimekeeper <= 0.0f )
	{
		flStaticTimekeeper = Plat_FloatTime();
		return true;
	}
	else if ( flStaticTimekeeper + flDelay < Plat_FloatTime() )
	{
		flStaticTimekeeper = 0.0f;
		return false;
	}
	else
	{
		return true;
	}
}

static bool ShouldDelayPlayerXnkid()
{
	static float s_flTime = 0.0f;
	return ShouldDelayBasedOnTimeThrottling( s_flTime, mm_player_delay_xnkid.GetFloat() );
}

static bool ShouldDelayPlayerQos()
{
	static float s_flTime = 0.0f;
	return ShouldDelayBasedOnTimeThrottling( s_flTime, mm_player_delay_qos.GetFloat() );
}
#else

inline static bool ShouldDelayPlayerXnkid() { return false; }
inline static bool ShouldDelayPlayerQos() { return false; }

#endif

void PlayerFriend::Live_Update_SearchXNKID()
{
	if( !XHasOverlappedIoCompleted( & m_SessionSearchOverlapped ) )
		return;

	if ( ShouldDelayPlayerXnkid() )
		return;

	DWORD result = 0;
	if( XGetOverlappedResult( &m_SessionSearchOverlapped, &result, false ) == ERROR_SUCCESS )
	{
		//result should be 1
		if( GetXSearchResults()->dwSearchResults >= 1)
		{
			V_memcpy( &m_GameSessionInfo, &( GetXSearchResults()->pResults[0].info ), sizeof( m_GameSessionInfo ) );
		}
		else
		{
			memset( &m_xSessionID, 0, sizeof( m_xSessionID ) );
			memset( &m_GameSessionInfo, 0, sizeof( m_GameSessionInfo ) );
			if ( m_pDetails )
				m_pDetails->deleteThis();
			m_pDetails = NULL;
		}
	}
	else
	{
		memset( &m_xSessionID, 0, sizeof( m_xSessionID ) );
		memset( &m_GameSessionInfo, 0, sizeof( m_GameSessionInfo ) );
		if ( m_pDetails )
			m_pDetails->deleteThis();
		m_pDetails = NULL;
	}

	m_eSearchState = SEARCH_COMPLETED;

	if ( ( const uint64 & ) m_GameSessionInfo.sessionID )
	{
		// Issue the QOS query
		m_xsiSearchState = m_GameSessionInfo;
		m_pQOS_xnaddr = &m_xsiSearchState.hostAddress;
		m_pQOS_xnkid = &m_xsiSearchState.sessionID;
		m_pQOS_xnkey = &m_xsiSearchState.keyExchangeKey;
		int err = g_pMatchExtensions->GetIXOnline()->XNetQosLookup( 1,
			&m_pQOS_xnaddr, &m_pQOS_xnkid, &m_pQOS_xnkey,
			0, NULL, NULL, 2, 0, 0, NULL, &m_XNQOS );

		if ( err == ERROR_SUCCESS )
			m_eSearchState = SEARCH_QOS;
	}
}

void PlayerFriend::Live_Update_Search_QOS()
{
	if( m_XNQOS->cxnqosPending != 0 )
		return;

	if ( ShouldDelayPlayerQos() )
		return;

	if ( m_pDetails )
		m_pDetails->deleteThis();
	m_pDetails = NULL;

	XNQOSINFO *pQOS = &m_XNQOS->axnqosinfo[0];
	if( pQOS->bFlags & XNET_XNQOSINFO_COMPLETE &&
		pQOS->bFlags & XNET_XNQOSINFO_DATA_RECEIVED &&
		pQOS->cbData && pQOS->pbData )
	{
		MM_GameDetails_QOS_t gd = { pQOS->pbData, pQOS->cbData, pQOS->wRttMedInMsecs };
		m_pDetails = g_pMatchFramework->GetMatchNetworkMsgController()->UnpackGameDetailsFromQOS( &gd );
	}

	g_pMatchExtensions->GetIXOnline()->XNetQosRelease( m_XNQOS );
	m_XNQOS = NULL;

	if ( m_pDetails )
	{
		// Set AUX fields like sessioninfo
		if ( KeyValues *kvOptions = m_pDetails->FindKey( "options", true ) )
		{
			kvOptions->SetUint64( "sessionid", ( const uint64 & ) m_xSessionID );

			char chSessionInfoBuffer[ XSESSION_INFO_STRING_LENGTH ] = {0};
			MMX360_SessionInfoToString( m_GameSessionInfo, chSessionInfoBuffer );
			kvOptions->SetString( "sessioninfo", chSessionInfoBuffer );
		}

		// Set the "player" key
		if ( KeyValues *kvPlayer = m_pDetails->FindKey( "player", true ) )
		{
			kvPlayer->SetUint64( "xuid", GetXUID() );
			kvPlayer->SetUint64( "xuidOnline", GetXUID() );
			kvPlayer->SetString( "name", GetName() );
			kvPlayer->SetWString( "richpresence", GetRichPresence() );
		}
	}

	m_eSearchState = SEARCH_COMPLETED;
}

#elif !defined( NO_STEAM )

void PlayerFriend::Steam_OnLobbyDataUpdate( LobbyDataUpdate_t *pParam )
{
	// Only callbacks about lobby itself
	if ( pParam->m_ulSteamIDLobby != pParam->m_ulSteamIDMember )
		return;

	// Listening for only callbacks related to current player
	if ( pParam->m_ulSteamIDLobby != ( const uint64 & ) m_xSessionID )
		return;

	// Unregister the callback
	m_CallbackOnLobbyDataUpdate.Unregister();

	// Set session info
	memset( &m_GameSessionInfo, 0, sizeof( m_GameSessionInfo ) );
	m_GameSessionInfo.sessionID = m_xSessionID;

	// Describe the lobby
	if ( m_pDetails )
		m_pDetails->deleteThis();
	m_pDetails = NULL;

	m_pDetails = g_pMatchFramework->GetMatchNetworkMsgController()->UnpackGameDetailsFromSteamLobby( pParam->m_ulSteamIDLobby );

	if ( m_pDetails )
	{
		// Set AUX fields like session id
		if ( KeyValues *kvOptions = m_pDetails->FindKey( "options", true ) )
		{
			kvOptions->SetUint64( "sessionid", pParam->m_ulSteamIDLobby );
		}

		// Set the "player" key
		if ( KeyValues *kvPlayer = m_pDetails->FindKey( "player", true ) )
		{
			kvPlayer->SetUint64( "xuid", GetXUID() );
			kvPlayer->SetUint64( "xuidOnline", GetXUID() );
			kvPlayer->SetString( "name", GetName() );
			kvPlayer->SetWString( "richpresence", GetRichPresence() );
		}
	}

	m_eSearchState = SEARCH_COMPLETED;
}

#endif

void PlayerFriend::Destroy()
{
	AbortSearch();

	if ( m_pPublishedPresence )
		m_pPublishedPresence->deleteThis();
	m_pPublishedPresence = NULL;

	delete this;
}

void PlayerFriend::AbortSearch()
{
#ifdef _X360
#elif !defined( NO_STEAM )
	m_CallbackOnLobbyDataUpdate.Unregister();
#endif

	// Clean up the queue
	while ( s_arrSessionSearchesQueue.FindAndRemove( m_xuid ) )
		continue;

	bool bAbortedSearch = false;

	switch ( m_eSearchState )
	{
#ifdef _X360
	case SEARCH_XNKID:
		MMX360_CancelOverlapped( &m_SessionSearchOverlapped );
		bAbortedSearch = true;
		break;

	case SEARCH_QOS:
		// We should gracefully abort the QOS operation outstanding
		g_pMatchExtensions->GetIXOnline()->XNetQosRelease( m_XNQOS );
		m_XNQOS = NULL;
		bAbortedSearch = true;
		break;
#elif !defined( NO_STEAM )
	case SEARCH_WAIT_LOBBY_DATA:
		bAbortedSearch = true;
		break;
#endif

	case SEARCH_COMPLETED:
		bAbortedSearch = true;
		break;
	}

	if ( bAbortedSearch )
	{
		-- s_numSearchesOutstanding;
		PumpSessionSearchQueue();
	}

	m_eSearchState = SEARCH_NONE;

	if ( m_pDetails )
		m_pDetails->deleteThis();
	m_pDetails = NULL;
}

void PlayerFriend::SetFriendMark( unsigned maskSetting )
{
	m_uFriendMark = maskSetting;
}

unsigned PlayerFriend::GetFriendMark()
{
	return m_uFriendMark;
}

void PlayerFriend::SetIsStale( bool bStale )
{
	m_bIsStale = bStale;
}

bool PlayerFriend::GetIsStale()
{
	return m_bIsStale;
}

void PlayerFriend::UpdateFriendInfo( FriendInfo_t const *pFriendInfo )
{
	if ( !pFriendInfo )
		return;

	if ( pFriendInfo->m_szName )
		Q_strncpy( m_szName, pFriendInfo->m_szName, ARRAYSIZE( m_szName ) );

	if ( pFriendInfo->m_wszRichPresence )
		Q_wcsncpy( m_wszRichPresence, pFriendInfo->m_wszRichPresence, ARRAYSIZE( m_wszRichPresence ) );

	m_uiTitleID = pFriendInfo->m_uiTitleID;
	
	if ( pFriendInfo->m_uiGameServerIP != ~0 )
		m_uiGameServerIP = pFriendInfo->m_uiGameServerIP;
	
	if ( cl_names_debug.GetBool() )
	{
		Q_strncpy( m_szName, PLAYER_DEBUG_NAME, ARRAYSIZE( m_szName ) );
	}

	if ( pFriendInfo->m_pGameDetails )
	{
		AbortSearch();
		
		if ( m_pDetails )
			m_pDetails->deleteThis();
		m_pDetails = pFriendInfo->m_pGameDetails->MakeCopy();
		
#ifdef _X360
		char const *szSessionInfo = m_pDetails->GetString( "options/sessioninfo" );
		MMX360_SessionInfoFromString( m_GameSessionInfo, szSessionInfo );
		m_xSessionID = m_GameSessionInfo.sessionID;
#elif !defined( NO_STEAM )
		uint64 uiSessionId = m_pDetails->GetUint64( "options/sessionid" );
		m_xSessionID = ( XNKID & ) uiSessionId;
#endif

		// Signal that we are finished with a search
		KeyValues *kvEvent = new KeyValues( "OnMatchPlayerMgrUpdate", "update", "friend" );
		kvEvent->SetUint64( "xuid", GetXUID() );
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvEvent );
	}
	else if ( pFriendInfo->m_uiTitleID &&
		pFriendInfo->m_uiTitleID != g_pMatchFramework->GetMatchTitle()->GetTitleID() )
	{
		if ( m_pDetails )
			m_pDetails->deleteThis();

		m_pDetails = new KeyValues( "TitleSettings" );
		m_pDetails->SetUint64( "titleid", pFriendInfo->m_uiTitleID );
	}
	else
	{
		m_xSessionID = pFriendInfo->m_xSessionID;

		if( m_eSearchState == SEARCH_NONE )
		{
			StartSearchForSessionInfo();
		}
	}

#if !defined( NO_STEAM )
	// Update published presence for the friend too
	if ( m_pPublishedPresence )
	{
		m_pPublishedPresence->deleteThis();
		m_pPublishedPresence = NULL;
	}

	ISteamFriends *pf = steamapicontext->SteamFriends();
	if ( pf && ( g_pMatchFramework->GetMatchTitle()->GetTitleID() == m_uiTitleID ) )
	{
		pf->RequestFriendRichPresence( GetXUID() ); // refresh friend's rich presence
		int numRichPresenceKeys = pf->GetFriendRichPresenceKeyCount( GetXUID() );
		for ( int j = 0; j < numRichPresenceKeys; ++ j )
		{
			const char *pszKey = pf->GetFriendRichPresenceKeyByIndex( GetXUID(), j );
			if ( pszKey && *pszKey )
			{
				char const *pszValue = pf->GetFriendRichPresence( GetXUID(), pszKey );
				if ( pszValue && *pszValue )
				{
					if ( !m_pPublishedPresence )
						m_pPublishedPresence = new KeyValues( "RP" );
					
					CFmtStr fmtNewKey( "%s", pszKey );
					while ( char *szFixChar = strchr( fmtNewKey.Access(), ':' ) )
						*szFixChar = '/';
					m_pPublishedPresence->SetString( fmtNewKey, pszValue );
				}
			}
		}
	}
#endif
}

bool PlayerFriend::IsUpdatingInfo()
{
	return m_eSearchState != SEARCH_NONE;
}


void PlayerFriend::StartSearchForSessionInfo()
{
	if ( !m_xuid )
		return;

	if ( m_eSearchState != SEARCH_NONE )
		return;

	m_eSearchState = SEARCH_QUEUED;

	// Check if we are not already in the queue
	if ( s_arrSessionSearchesQueue.Find( m_xuid ) == s_arrSessionSearchesQueue.InvalidIndex() )
	{
		s_arrSessionSearchesQueue.AddToTail( m_xuid );
	}

	PumpSessionSearchQueue();
}

void PlayerFriend::StartSearchForSessionInfoImpl()
{
#ifdef _X360
	if ( !XBX_GetNumGameUsers() || XBX_GetPrimaryUserIsGuest() )
	{
		memset( &m_xSessionID, 0, sizeof( m_xSessionID ) );
		memset( &m_GameSessionInfo, 0, sizeof( m_GameSessionInfo ) );
		if ( m_pDetails )
			m_pDetails->deleteThis();
		m_pDetails = NULL;

		m_eSearchState = SEARCH_NONE;

		return;
	}
#endif

	if( m_eSearchState == SEARCH_NONE ||
		m_eSearchState == SEARCH_QUEUED )
	{
#ifdef _X360

		if( ( const uint64 & ) m_xSessionID )
		{
			int iCtrlr = XBX_GetPrimaryUserId();

			DWORD numBytesResult = 0;
			DWORD dwError = g_pMatchExtensions->GetIXOnline()->XSessionSearchByID( m_xSessionID, iCtrlr, &numBytesResult, NULL, NULL );
			if( dwError != ERROR_INSUFFICIENT_BUFFER )
			{
				memset( &m_xSessionID, 0, sizeof( m_xSessionID ) );
				memset( &m_GameSessionInfo, 0, sizeof( m_GameSessionInfo ) );
				if ( m_pDetails )
					m_pDetails->deleteThis();
				m_pDetails = NULL;

				m_eSearchState = SEARCH_NONE;
				return;
			}

			m_bufSessionSearchResults.EnsureCapacity( numBytesResult );
			ZeroMemory( GetXSearchResults(), numBytesResult );

			dwError = g_pMatchExtensions->GetIXOnline()->XSessionSearchByID( m_xSessionID, iCtrlr, &numBytesResult, GetXSearchResults(), &m_SessionSearchOverlapped );
			if( dwError != ERROR_IO_PENDING )
			{
				memset( &m_xSessionID, 0, sizeof( m_xSessionID ) );
				memset( &m_GameSessionInfo, 0, sizeof( m_GameSessionInfo ) );
				if ( m_pDetails )
					m_pDetails->deleteThis();
				m_pDetails = NULL;

				m_eSearchState = SEARCH_NONE;
				return;
			}

			m_eSearchState = SEARCH_XNKID;

#elif !defined( NO_STEAM )

		if ( steamapicontext->SteamMatchmaking() &&
			( const uint64 & ) m_xSessionID &&
			steamapicontext->SteamMatchmaking()->RequestLobbyData( ( const uint64 & ) m_xSessionID ) )
		{
			// Enable the callback
			m_CallbackOnLobbyDataUpdate.Register( this, &PlayerFriend::Steam_OnLobbyDataUpdate );

			m_eSearchState = SEARCH_WAIT_LOBBY_DATA;

#else

		if ( 0 )
		{

#endif
			
			++ s_numSearchesOutstanding;
		}
		else
		{
			memset( &m_xSessionID, 0, sizeof( m_xSessionID ) );
			memset( &m_GameSessionInfo, 0, sizeof( m_GameSessionInfo ) );
			if ( m_pDetails )
				m_pDetails->deleteThis();
			m_pDetails = NULL;

			m_eSearchState = SEARCH_NONE;
		}
	}
}



//
// PlayerLocal implementation
//

PlayerLocal::PlayerLocal( int iController ) :
	m_eLoadedTitleData( eXUserSigninState_NotSignedIn ),
	m_flLastSave( 0.0f ),
	m_uiPlayerFlags( 0 ),
	m_pLeaderboardData( new KeyValues( "Leaderboard" ) ),
	m_autodelete_pLeaderboardData( m_pLeaderboardData )
{
	Assert( iController >= 0 && iController < XUSER_MAX_COUNT );

	memset( &m_ProfileData, 0, sizeof( m_ProfileData ) );
	memset( m_bufTitleData, 0, sizeof( m_bufTitleData ) );
	memset( m_bSaveTitleData, 0, sizeof( m_bSaveTitleData ) );

	m_iController = iController;
	GetXWriteOpportunity( iController ); // reset

#ifdef _X360
	m_bIsTitleDataValid = false;
	for ( int i=0; i<TITLE_DATA_COUNT; ++i )
	{
		m_bIsTitleDataBlockValid[ i ] = false;
	}
	m_bIsFreshPlayerProfile = false;
	if ( !XBX_GetPrimaryUserIsGuest() )
	{
		XUserGetXUID( iController, &m_xuid );
	}

	DetectOnlineState();
#elif !defined( NO_STEAM )
	CSteamID steamIDPlayer;
	if ( steamapicontext->SteamUser() )
	{
		m_eOnlineState = steamapicontext->SteamUser()->BLoggedOn() ? IPlayer::STATE_ONLINE : IPlayer::STATE_OFFLINE;
		steamIDPlayer = steamapicontext->SteamUser()->GetSteamID();
		m_xuid = steamIDPlayer.IsValid() ? steamIDPlayer.ConvertToUint64() : 0;
	}
	else
	{
		m_xuid = 0;
	}
#else
	m_xuid = 1ull;
	m_eOnlineState = IPlayer::STATE_OFFLINE;
#endif

#ifdef _X360
	if( m_xuid )
	{
		XUserGetName( m_iController, m_szName, ARRAYSIZE( m_szName ) );
		LoadPlayerProfileData();
	}
	else if ( char const *szGuestName = g_pMatchFramework->GetMatchTitle()->GetGuestPlayerName( m_iController ) )
	{
		Q_strncpy( m_szName, szGuestName, ARRAYSIZE( m_szName ) );
	}
	else
	{
		m_szName[0] = 0;
	}
#elif defined ( _PS3 )
	ConVarRef cl_name( "name" );
	const char* pPlayerName = cl_name.GetString();
	Q_strncpy( m_szName, pPlayerName, ARRAYSIZE( m_szName ) );
#elif !defined( NO_STEAM )
	// Get user name from Steam
	if ( steamIDPlayer.IsValid() && steamapicontext->SteamUser() && steamapicontext->SteamFriends() )
	{
		const char *pszName = steamapicontext->SteamFriends()->GetFriendPersonaName( steamIDPlayer );
		if ( pszName )
		{
			Q_strncpy( m_szName, pszName, ARRAYSIZE( m_szName ) );
		}			
	}
	m_CallbackOnPersonaStateChange.Register( this, &PlayerLocal::Steam_OnPersonaStateChange );
	m_CallbackOnServersConnected.Register( this, &PlayerLocal::Steam_OnServersConnected );
	m_CallbackOnServersDisconnected.Register( this, &PlayerLocal::Steam_OnServersDisconnected );
	LoadPlayerProfileData();
#else
	if ( char const *szGuestName = g_pMatchFramework->GetMatchTitle()->GetGuestPlayerName( m_iController ) )
	{
		Q_strncpy( m_szName, szGuestName, ARRAYSIZE( m_szName ) );
	}
	else
	{
		m_szName[0] = 0;
	}
#endif

	if ( cl_names_debug.GetBool() )
	{
		Q_strncpy( m_szName, PLAYER_DEBUG_NAME, ARRAYSIZE( m_szName ) );
	}
}

PlayerLocal::~PlayerLocal()
{
#ifdef _X360
	for ( int k = 0; k < s_arrPendingAsyncAwards.Count(); ++ k )
	{
		// Detach pending achievement awards from currently destructed player
		if ( s_arrPendingAsyncAwards[k]->m_pLocalPlayer == this )
			s_arrPendingAsyncAwards[k]->m_pLocalPlayer = NULL;
	}
#endif
}

void PlayerLocal::LoadTitleData()
{
	if ( m_eLoadedTitleData == eXUserSigninState_SignedInToLive )
		// already processed
		return;

	if ( m_eLoadedTitleData >= GetAssumedSigninState() )
		// already processed
		return;

#ifdef _X360
	m_bIsTitleDataValid = false;
	for ( int i=0; i<TITLE_DATA_COUNT; ++i )
	{
		m_bIsTitleDataBlockValid[ i ] = false;
	}

	float flTimeStart;
	flTimeStart = Plat_FloatTime();
	Msg( "Player %d : LoadTitleData...\n", m_iController );

	//
	// Enumerate the state of all achievements
	//
	{
		DWORD numAchievements = 0;
		HANDLE hEnumerator = NULL;
		DWORD dwBytes;
		DWORD ret = XUserCreateAchievementEnumerator( 0, m_iController, INVALID_XUID, XACHIEVEMENT_DETAILS_TFC, 0, 80, &dwBytes, &hEnumerator );
		if ( ret == ERROR_SUCCESS )
		{
			CUtlVector< char > vBuffer;
			vBuffer.SetCount( dwBytes );
			ret = XEnumerate( hEnumerator, vBuffer.Base(), dwBytes, &numAchievements, NULL );
			CloseHandle( hEnumerator );
			hEnumerator = NULL;
			if ( ret == ERROR_SUCCESS )
			{
				XACHIEVEMENT_DETAILS const *pXboxAchievements = ( XACHIEVEMENT_DETAILS const * ) vBuffer.Base();
				for ( DWORD i = 0; i < numAchievements; ++i )
				{
					if ( AchievementEarned( pXboxAchievements[i].dwFlags ) )
					{
						m_arrAchievementsEarned.FindAndFastRemove( pXboxAchievements[i].dwId );
						m_arrAchievementsEarned.AddToTail( pXboxAchievements[i].dwId );
					}
				}
			}
		}
	}

	//
	// Load actual title data blocks
	//

	DWORD dwNumDataIds = TITLE_DATA_COUNT_X360;
	CArrayAutoPtr< DWORD > pdwTitleDataIds( new DWORD[ dwNumDataIds ] );
	for ( DWORD k = 0; k < dwNumDataIds; ++ k )
		pdwTitleDataIds[k] = GetTitleSpecificDataId( k );

	m_eLoadedTitleData = GetAssumedSigninState();

	DWORD resultsSize = 0;
	DWORD ret = ERROR_FILE_NOT_FOUND;

	if ( m_eLoadedTitleData == eXUserSigninState_SignedInLocally )
	{
		ret = g_pMatchExtensions->GetIXOnline()->XUserReadProfileSettings(
			g_pMatchFramework->GetMatchTitle()->GetTitleID(), m_iController,
			dwNumDataIds, pdwTitleDataIds.Get(),
			&resultsSize, NULL,
			NULL );
	}
	else if ( m_eLoadedTitleData == eXUserSigninState_SignedInToLive )
	{
		ret = g_pMatchExtensions->GetIXOnline()->XUserReadProfileSettingsByXuid(
			g_pMatchFramework->GetMatchTitle()->GetTitleID(), m_iController,
			1, &m_xuid,
			dwNumDataIds, pdwTitleDataIds.Get(),
			&resultsSize, NULL,
			NULL );
	}

	if ( ret != ERROR_INSUFFICIENT_BUFFER )
	{
		Warning( "Player %d : LoadTitleData failed to get size (err=0x%08X)!\n", m_iController, ret );
		// Failed
		OnProfileTitleDataLoaded( ret );
		return;
	}

	CArrayAutoPtr< char > spResultBuffer( new char[ resultsSize ] );
	XUSER_READ_PROFILE_SETTING_RESULT *pResult = (XUSER_READ_PROFILE_SETTING_RESULT *) spResultBuffer.Get();

	if ( m_eLoadedTitleData == eXUserSigninState_SignedInLocally )
	{
		ret = g_pMatchExtensions->GetIXOnline()->XUserReadProfileSettings(
			g_pMatchFramework->GetMatchTitle()->GetTitleID(), m_iController,
			dwNumDataIds, pdwTitleDataIds.Get(),
			&resultsSize, pResult,
			NULL );
	}
	else if ( m_eLoadedTitleData == eXUserSigninState_SignedInToLive )
	{
		ret = g_pMatchExtensions->GetIXOnline()->XUserReadProfileSettingsByXuid(
			g_pMatchFramework->GetMatchTitle()->GetTitleID(), m_iController,
			1, &m_xuid,
			dwNumDataIds, pdwTitleDataIds.Get(),
			&resultsSize, pResult,
			NULL );
	}

	if ( ret != ERROR_SUCCESS )
	{
		Warning( "Player %d : LoadTitleData failed to read data (err=0x%08X)!\n", m_iController, ret );
		// Failed
		OnProfileTitleDataLoaded( ret );
		return;
	}

	m_bIsTitleDataValid = true;
	m_bIsFreshPlayerProfile = true;
	for ( DWORD iSetting = 0; iSetting < pResult->dwSettingsLen; ++ iSetting )
	{
		XUSER_PROFILE_SETTING const &xps = pResult->pSettings[ iSetting ];

		if ( xps.data.type != XUSER_DATA_TYPE_BINARY )
		{
			m_bIsTitleDataValid = false;
			m_bIsFreshPlayerProfile = false;
			continue;
		}
		if ( xps.data.binary.cbData != XPROFILE_SETTING_MAX_SIZE )
		{
			if ( xps.data.binary.cbData != 0 )
			{
				m_bIsTitleDataValid = false;
			}
			continue;
		}

		m_bIsFreshPlayerProfile = false;
		m_bIsTitleDataBlockValid[ iSetting ] = true;

		int iDataIndex = GetTitleSpecificDataIndex( xps.dwSettingId );
		if ( iDataIndex >= 0 )
		{
			Msg( "Player %d : LoadTitleData succeeded with Data%d\n",
				m_iController, iDataIndex );
			V_memcpy( m_bufTitleData[ iDataIndex ], xps.data.binary.pbData, XPROFILE_SETTING_MAX_SIZE );
		}
	}

	// Clear the dirty flag after dirty
	V_memset( m_bSaveTitleData, 0, sizeof( m_bSaveTitleData[0] ) * TITLE_DATA_COUNT_X360 );

	// After we loaded some title data, see if we need to retrospectively award achievements
	EvaluateAwardsStateBasedOnStats();

	Msg( "Player %d : LoadTitleData finished (%.3f sec).\n",
		m_iController, Plat_FloatTime() - flTimeStart );
		 
	if ( m_bIsTitleDataValid )
		OnProfileTitleDataLoaded( 0 );
	else
		OnProfileTitleDataLoaded( 1 );

#elif !defined ( NO_STEAM )

	// Always request user stats from Steam
	if ( steamapicontext->SteamUserStats() )
	{
		m_eLoadedTitleData = GetAssumedSigninState();
		m_CallbackOnUserStatsReceived.Register( this, &PlayerLocal::Steam_OnUserStatsReceived );
		steamapicontext->SteamUserStats()->RequestCurrentStats();

		s_flSteamStatsRequestTime = Plat_FloatTime();
		if ( !s_bSteamStatsRequestFailed )
		{
			DevMsg( "Requesting Steam stats... (%2.2f)\n", Plat_FloatTime() );
		}
	}

#else

	m_eLoadedTitleData = GetAssumedSigninState();
	// Since we don't have Steam, we reset all configuration values and stats.
	IGameEvent *event = g_pMatchExtensions->GetIGameEventManager2()->CreateEvent( "reset_game_titledata" );
	if ( event )
	{
		event->SetInt( "controllerId", m_iController );
		g_pMatchExtensions->GetIGameEventManager2()->FireEventClientSide( event );
	}
	g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "ResetConfiguration", "iController", m_iController ) );


#endif
}

#if !defined( _X360 ) && !defined ( NO_STEAM )

ConVar mm_cfgoverride_file( "mm_cfgoverride_file", "", FCVAR_DEVELOPMENTONLY );
ConVar mm_cfgoverride_commit( "mm_cfgoverride_commit", "", FCVAR_DEVELOPMENTONLY );
ConVar mm_cfgdebug_mode( "mm_cfgdebug_mode", "0", FCVAR_DEVELOPMENTONLY );

static bool SetSteamStatWithPotentialOverride( char const *szField, int32 iValue )
{
	char const *szStatFieldSteamDB = szField;
	return steamapicontext->SteamUserStats() ? steamapicontext->SteamUserStats()->SetStat( szStatFieldSteamDB, iValue ) : false;
}

static bool SetSteamStatWithPotentialOverride( char const *szField, float fValue )
{
	char const *szStatFieldSteamDB = szField;
	return steamapicontext->SteamUserStats() ? steamapicontext->SteamUserStats()->SetStat( szStatFieldSteamDB, fValue ) : false;
}

template < typename T >
static inline bool ApplySteamStatPotentialOverride( char const *szField, T *pValue, bool bResult, T (KeyValues::*pfn)( char const *, T ) )
{
#ifdef _CERT
	return bResult;
#else
	if ( mm_cfgdebug_mode.GetInt() > 0 )
	{
		DevMsg( "[PlayerStats] '%s' = %d (0x%08X)\n", szField, (int32)*pValue, *(int32*)pValue );
	}

	char const *szFile = mm_cfgoverride_file.GetString();
	if ( !szFile || !*szFile )
		return bResult;
	
	KeyValues *kvOverride = new KeyValues( "cfgoverride.kv" );
	KeyValues::AutoDelete autodelete( kvOverride );
	if ( !kvOverride->LoadFromFile( g_pFullFileSystem, szFile ) )
		return bResult;

	if ( KeyValues *kvItemOverride = kvOverride->FindKey( "items" )->FindKey( szField ) )
	{
		*pValue = ( kvItemOverride->*pfn )( "", 0 );
		DevMsg( "[PlayerStats] '%s' overrides '%s' = '%s'\n", szFile, szField, kvItemOverride->GetString( "", "" ) );
		if ( mm_cfgoverride_commit.GetBool() )
			SetSteamStatWithPotentialOverride( szField, *pValue );
		return true;
	}
	
	// Match by wildcard
	for ( KeyValues *kvWildcard = kvOverride->FindKey( "wildcards" )->GetFirstValue(); kvWildcard; kvWildcard = kvWildcard->GetNextValue() )
	{
		char const *szWildcard = kvWildcard->GetName();
		int nLen = Q_strlen( szWildcard );
		if ( !nLen || ( szWildcard[nLen-1] != '*' ) )
			continue;
		if ( (nLen <= 1) || !Q_strnicmp( szWildcard, szField, nLen - 1 ) )
		{
			*pValue = ( kvWildcard->*pfn )( "", 0 );
			DevMsg( "[PlayerStats] '%s' overrides '%s' = '%s' [wildcard match '%s']\n", szFile, szField, kvWildcard->GetString( "", "" ), szWildcard );
			if ( mm_cfgoverride_commit.GetBool() )
				SetSteamStatWithPotentialOverride( szField, *pValue );
			return true;
		}
	}

	return bResult;
#endif
}

static bool GetSteamStatWithPotentialOverride( char const *szField, int32 *pValue )
{
	char const *szStatFieldSteamDB = szField;
	bool bResult = steamapicontext->SteamUserStats()->GetStat( szStatFieldSteamDB, pValue );
	return ApplySteamStatPotentialOverride<int32>( szField, pValue, bResult, &KeyValues::GetInt );
}

static bool GetSteamStatWithPotentialOverride( char const *szField, float *pValue )
{
	char const *szStatFieldSteamDB = szField;
	bool bResult = steamapicontext->SteamUserStats()->GetStat( szStatFieldSteamDB, pValue );
	return ApplySteamStatPotentialOverride<float>( szField, pValue, bResult, &KeyValues::GetFloat );
}

void PlayerLocal::Steam_OnUserStatsReceived( UserStatsReceived_t *pParam )
{
#ifndef NO_STEAM
	if ( !s_bSteamStatsRequestFailed || ( pParam->m_eResult == k_EResultOK ) )
	{
		DevMsg( "PlayerLocal::Steam_OnUserStatsReceived... (%2.2f sec since request)\n", s_flSteamStatsRequestTime ? ( Plat_FloatTime() - s_flSteamStatsRequestTime ) : 0.0f );
	}
	s_flSteamStatsRequestTime = 0;
#endif

	// If failed, we'll request one more time
	if ( pParam->m_eResult != k_EResultOK )
	{
		if ( !s_bSteamStatsRequestFailed )
		{
			DevWarning( "PlayerLocal::Steam_OnUserStatsReceived (failed with error %d)\n", pParam->m_eResult );
			s_bSteamStatsRequestFailed = true;
		}
		m_eLoadedTitleData = eXUserSigninState_NotSignedIn;
		return;
	}
	s_bSteamStatsRequestFailed = false;
	g_bSteamStatsReceived = true;

	//
	// Achievements state
	//
	for ( TitleAchievementsDescription_t const *pAchievement = g_pMatchFramework->GetMatchTitle()->DescribeTitleAchievements();
		pAchievement && pAchievement->m_szAchievementName; ++ pAchievement )
	{
		bool bAchieved;
		if ( steamapicontext->SteamUserStats()->GetAchievement( pAchievement->m_szAchievementName, &bAchieved ) && bAchieved )
		{
			m_arrAchievementsEarned.FindAndFastRemove( pAchievement->m_idAchievement );
			m_arrAchievementsEarned.AddToTail( pAchievement->m_idAchievement );
		}
	}

	//
	// Load all our stats data
	//
	TitleDataFieldsDescription_t const *pTitleDataTable = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
	for ( ; pTitleDataTable && pTitleDataTable->m_szFieldName; ++ pTitleDataTable )
	{
		switch( pTitleDataTable->m_eDataType )
		{
		case TitleDataFieldsDescription_t::DT_uint8:
		case TitleDataFieldsDescription_t::DT_uint16:
		case TitleDataFieldsDescription_t::DT_uint32:
			{
				uint32 i32field[3] = {0};

				if ( GetSteamStatWithPotentialOverride( pTitleDataTable->m_szFieldName, ( int32 * ) &i32field[0] ) )
				{
					*( uint16 * )( &i32field[1] ) = uint16( i32field[0] );
					*( uint8  * )( &i32field[2] ) = uint8 ( i32field[0] );
					
					memcpy( &m_bufTitleData[ pTitleDataTable->m_iTitleDataBlock ][ pTitleDataTable->m_numBytesOffset ],
						&i32field[ 2 - ( pTitleDataTable->m_eDataType / 16 ) ], pTitleDataTable->m_eDataType / 8 );
				}
			}
			break;
		
		case TitleDataFieldsDescription_t::DT_float:
			{
				float flField = 0.0f;
				if ( GetSteamStatWithPotentialOverride( pTitleDataTable->m_szFieldName, &flField ) )
				{
					memcpy( &m_bufTitleData[ pTitleDataTable->m_iTitleDataBlock ][ pTitleDataTable->m_numBytesOffset ],
						&flField, pTitleDataTable->m_eDataType / 8 );
				}
			}
			break;

		case TitleDataFieldsDescription_t::DT_uint64:
			{
				uint32 i32field[2] = { 0 };

				char chBuffer[ 256 ] = {0};

				for ( int k = 0; k < ARRAYSIZE( i32field ); ++ k )
				{
					Q_snprintf( chBuffer, ARRAYSIZE( chBuffer ), "%s.%d", pTitleDataTable->m_szFieldName, k );
					if ( !GetSteamStatWithPotentialOverride( chBuffer, ( int32 * ) &i32field[k] ) )
						i32field[k] = 0;
				}
				
				memcpy( &m_bufTitleData[ pTitleDataTable->m_iTitleDataBlock ][ pTitleDataTable->m_numBytesOffset ],
					&i32field[0], pTitleDataTable->m_eDataType / 8 );
			}
			break;

		case TitleDataFieldsDescription_t::DT_BITFIELD:
			{
			#ifdef STEAM_PACK_BITFIELDS
				char chStatField[64] = {0};
				uint32 uiOffsetInTermsOfUINT32 = pTitleDataTable->m_numBytesOffset/32;
				V_snprintf( chStatField, sizeof( chStatField ), "bitfield_%02u_%03X", pTitleDataTable->m_iTitleDataBlock + 1, uiOffsetInTermsOfUINT32*4 );
				int32 iCombinedBitValue = 0;
				if ( GetSteamStatWithPotentialOverride( chStatField, &iCombinedBitValue ) )
				{
					( reinterpret_cast< uint32 * >( &m_bufTitleData[pTitleDataTable->m_iTitleDataBlock][0] ) )[ uiOffsetInTermsOfUINT32 ] = iCombinedBitValue;
				}
			#else
				int i32field = 0;
				if ( GetSteamStatWithPotentialOverride( pTitleDataTable->m_szFieldName, &i32field ) )
				{
					char &rByte = m_bufTitleData[ pTitleDataTable->m_iTitleDataBlock ][ pTitleDataTable->m_numBytesOffset/8 ];
					char iMask = ( 1 << ( pTitleDataTable->m_numBytesOffset % 8 ) );
					if ( i32field )
						rByte |= iMask;
					else
						rByte &=~iMask;
				}
			#endif
			}
			break;
		}
	}

#if defined ( _PS3 )

	// We just loaded all our stats and settings from Steam.
	TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
	Assert( fields );
	TitleDataFieldsDescription_t const *versionField = TitleDataFieldsDescriptionFindByString( fields, TITLE_DATA_PREFIX "CFG.sys.version" );
	Assert( versionField );
	int versionNumber = TitleDataFieldsDescriptionGetValue<int32>( versionField, this );

	ConVarRef cl_configversion("cl_configversion");
	// Check the version number to see if this is a new save profile.
	// In that case, we need to reset everything to the defaults.
	if ( versionNumber != cl_configversion.GetInt() )
	{
		// This will wipe out all achievement and stats.  This is called in Host_ResetConfiguration for xbox, but we don't
		// want to call it for anything that uses steam unless we're okay with clearning all stats.
		IGameEvent *event = g_pMatchExtensions->GetIGameEventManager2()->CreateEvent( "reset_game_titledata" );
		if ( event )
		{
			event->SetInt( "controllerId", m_iController );
			g_pMatchExtensions->GetIGameEventManager2()->FireEventClientSide( event );
		}

		// ResetConfiguration will set all the settings to the defaults.
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "ResetConfiguration", "iController", m_iController ) );
	}

#endif


#if !defined ( _X360 )
	// send an event to anyone else who needs Steam user stat data
	IGameEvent *event =  g_pMatchExtensions->GetIGameEventManager2()->CreateEvent( "user_data_downloaded" );
	if ( event )
	{
#ifdef GAME_DLL
		g_pMatchExtensions->GetIGameEventManager2()->FireEvent( event );
#else
		// not sure this event is caught anywhere but we brought it over from orange box just in case
		g_pMatchExtensions->GetIGameEventManager2()->FireEventClientSide( event );
#endif
	}
#endif

	// After we loaded some title data, see if we need to retrospectively award achievements
	EvaluateAwardsStateBasedOnStats();

	// Flush stats if we are clearing for debugging
	if ( mm_cfgoverride_commit.GetBool() )
		steamapicontext->SteamUserStats()->StoreStats();

	//
	// Finished reading stats
	//
	DevMsg( "User%d stats retrieved.\n", m_iController );
	OnProfileTitleDataLoaded( 0 );

#ifdef _DEBUG

	static bool debugDumpStats = true;
	if ( debugDumpStats )
	{
		debugDumpStats = false;
		// Debug code.
		// Dump the stats once loaded so we can see what they are.
		g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "ms_player_dump_properties" );
	}

#endif

}

void PlayerLocal::Steam_OnPersonaStateChange( PersonaStateChange_t *pParam )
{
	if ( !steamapicontext ||
		!steamapicontext->SteamUtils() ||
		!steamapicontext->SteamFriends() ||
		!steamapicontext->SteamUser() ||
		!pParam ||
		!m_xuid )
		return;

	// Check that something changed about local user
	if ( m_xuid == pParam->m_ulSteamID )
	{
		if ( pParam->m_nChangeFlags & k_EPersonaChangeName )
		{
			CSteamID steamID;
			steamID.SetFromUint64( m_xuid );

			if ( char const *szName = steamapicontext->SteamFriends()->GetFriendPersonaName( steamID ) )
			{
				Q_strncpy( m_szName, szName, ARRAYSIZE( m_szName ) );
			}
			
			if ( cl_names_debug.GetBool() )
			{
				Q_strncpy( m_szName, PLAYER_DEBUG_NAME, ARRAYSIZE( m_szName ) );
			}
		}
	}
}

void PlayerLocal::UpdatePlayersSteamLogon()
{
	if ( !steamapicontext->SteamUser() )
		return;

	IPlayer::OnlineState_t eState = steamapicontext->SteamUser()->BLoggedOn() ? IPlayer::STATE_ONLINE : IPlayer::STATE_OFFLINE;

	m_eOnlineState = eState;

	// Update XUID on PS3:
	CSteamID cSteamId = steamapicontext->SteamUser()->GetSteamID();
	if ( !m_xuid && cSteamId.IsValid() )
	{
		m_xuid = steamapicontext->SteamUser()->GetSteamID().ConvertToUint64();
	}
}

void PlayerLocal::Steam_OnServersConnected( SteamServersConnected_t *pParam )
{
	DevMsg( "Steam_OnServersConnected\n" );
	
	UpdatePlayersSteamLogon();
}

void PlayerLocal::Steam_OnServersDisconnected( SteamServersDisconnected_t *pParam )
{
	DevWarning( "Steam_OnServersDisconnected\n" );

	UpdatePlayersSteamLogon();
}

#endif

void PlayerLocal::SetTitleDataWriteTime( float flTime )
{
	m_flLastSave = flTime;
}

#if defined ( _X360 )
bool PlayerLocal::IsTitleDataBlockValid( int blockId )
{
	if ( blockId < 0 || blockId >= TITLE_DATA_COUNT )
		return false;

	return m_bIsTitleDataBlockValid[ blockId ];
}

void PlayerLocal::ClearBufTitleData( void )
{
	memset( m_bufTitleData, 0, sizeof( m_bufTitleData ) );
}

#endif

// Test if we can still read from profile; used when storage device is removed and we
// want to verify the profile still has a storage connection
bool PlayerLocal::IsTitleDataStorageConnected( void )
{

#if defined( _X360 )

	// try to write out storage block 3 to see if there is a storage unit associated with this profile

	CUtlVector< XUSER_PROFILE_SETTING > pXPS;

	DWORD dwNumDataBufferBytes = XPROFILE_SETTING_MAX_SIZE;
	CArrayAutoPtr< char > spDataBuffer( new char[ dwNumDataBufferBytes ] );
	V_memset( spDataBuffer.Get(), 0, dwNumDataBufferBytes );
	int titleStorageBlock3Index = 2;

	XUSER_PROFILE_SETTING xps;
	V_memset( &xps, 0, sizeof( xps ) );
	xps.dwSettingId = GetTitleSpecificDataId( titleStorageBlock3Index );
	xps.data.type = XUSER_DATA_TYPE_BINARY;
	xps.data.binary.cbData = XPROFILE_SETTING_MAX_SIZE;
	xps.data.binary.pbData = (PBYTE) spDataBuffer.Get();

	V_memcpy( xps.data.binary.pbData, m_bufTitleData[ titleStorageBlock3Index ], XPROFILE_SETTING_MAX_SIZE );

	pXPS.AddToTail( xps );


	//
	// Issue the XWrite operation
	//
	DWORD ret;
	ret = g_pMatchExtensions->GetIXOnline()->XUserWriteProfileSettings( m_iController, pXPS.Count(), pXPS.Base(), NULL );

	if ( ret != ERROR_SUCCESS )
	{
		return false;
	}
#endif
	return true;

}

void PlayerLocal::WriteTitleData()
{
#if defined( _DEMO ) && defined( _X360 )
	// Demo versions are not allowed to write profile data
	return;
#endif

#ifdef _X360
	if ( !m_xuid )
		return;

	if ( GetAssumedSigninState() == eXUserSigninState_NotSignedIn )
		return;

#if defined( CSTRIKE15 )
	// Code to handle TCR 047 
	// Calling GetXWriteOpportunity clears the MM_XWriteOppurtinty to MMXWO_NONE
	// but we don't want that if we are trying to write within 3 seconds of the last
	// write.  Rather we want the write to succeed after the 3 seconds expire; so
	// we just bail until the 3 seconds is up and then allow the write to happen
	// We do not have to queue up writes since the data we store on 360 is 
	// live data and is always the most current version of the data
	const float cMinWriteDelay = 3.0f;
	if ( Plat_FloatTime() - m_flLastSave < cMinWriteDelay )
	{
		return;
	}
#endif

	// NOTE: Need to call this here, because this has side effects.
	// Getting the opportunity will clear the opportunity. This is used
	// to only allow writes to happen at times out of game where we
	// can be sure we don't hitch. If we don't do it here, then we can get into
	// a state where some previous opportunity to write was set (say,
	// leaving a cooperative game) with no stats to be saved. If that happens
	// and we don't reset the state, then the next time we enter a new level
	// it'll save at a bad time.
	MM_XWriteOpportunity eXWO = GetXWriteOpportunity( m_iController );
#endif

	//
	// Determine if XWrite is required first
	//

	DWORD numXWritesRequired = 0;

	for ( int iData = 0; iData < ( IsX360() ? TITLE_DATA_COUNT_X360 : TITLE_DATA_COUNT ); ++ iData )
	{
		if ( !m_bSaveTitleData[iData] )
			continue;

		++ numXWritesRequired;
	}

	if ( !numXWritesRequired )
		// early out if nothing to do here
		return;

#ifdef _X360
	if ( m_eLoadedTitleData < GetAssumedSigninState() )
	{
		// haven't loaded data for the state
		return;
	}

	if ( !IsTitleDataValid() )
		return;

	bool bCanXWrite = true;
#if !defined (CSTRIKE15 )
	//
	// Check if we can actually XWrite (TCR 136)
	//
	static const float s_flXWritesFreq = 5 * 60 + 1; // 5 minutes
	if ( Plat_FloatTime() - m_flLastSave < s_flXWritesFreq )
		 bCanXWrite = false;

	switch ( eXWO )
	{
	default:
	case MMXWO_NONE:
		bCanXWrite = false;
		break;
	case MMXWO_CHECKPOINT:
		break;
	case MMXWO_SETTINGS:
	case MMXWO_SESSION_FINISHED:
		bCanXWrite = true;
		break;
	}

#else
	// Cstrike only writes to user profile; writes are <500ms; earlier code ensures we only 
	// write to profile at most every 3 seconds so we are TCR compliant
	// Cstrike needs a waiver for every 5 minute writes since we save stats at end of round
	// so we can ignore 5 minute timer check;  
	// If we get to this code for any WriteOpportunity we are ok to write 
	if ( eXWO == MMXWO_NONE )
	{
		bCanXWrite = false;
	}
#endif

	if ( !bCanXWrite )
	{
		// have to wait longer
		return;
	}

	//
	// Prepare the XWrite batch
	//

	float flTimeStart;
	flTimeStart = Plat_FloatTime();
	Msg( "Player %d : WriteTitleData initiated...\n", m_iController );

	CUtlVector< XUSER_PROFILE_SETTING > pXPS;

	DWORD dwNumDataBufferBytes = TITLE_DATA_COUNT_X360 * XPROFILE_SETTING_MAX_SIZE;
	CArrayAutoPtr< char > spDataBuffer( new char[ dwNumDataBufferBytes ] );
	V_memset( spDataBuffer.Get(), 0, dwNumDataBufferBytes );

	for ( int iData = 0; iData < TITLE_DATA_COUNT_X360; ++ iData )
	{
		if ( !m_bSaveTitleData[iData] )
			continue;

		Msg( "Player %d : WriteTitleData preparing TitleData%d...\n", m_iController, iData + 1 );

		XUSER_PROFILE_SETTING xps;
		V_memset( &xps, 0, sizeof( xps ) );
		xps.dwSettingId = GetTitleSpecificDataId( iData );
		xps.data.type = XUSER_DATA_TYPE_BINARY;
		xps.data.binary.cbData = XPROFILE_SETTING_MAX_SIZE;
		xps.data.binary.pbData = (PBYTE) spDataBuffer.Get() + iData * XPROFILE_SETTING_MAX_SIZE;

		V_memcpy( xps.data.binary.pbData, m_bufTitleData[ iData ], XPROFILE_SETTING_MAX_SIZE );

		pXPS.AddToTail( xps );
	}

	// Clear dirty state
	V_memset( m_bSaveTitleData, 0, sizeof( m_bSaveTitleData[0] ) * TITLE_DATA_COUNT_X360 );

	//
	// Issue the XWrite operation
	//
	DWORD ret;
	m_flLastSave = Plat_FloatTime();
	ret = g_pMatchExtensions->GetIXOnline()->XUserWriteProfileSettings( m_iController, pXPS.Count(), pXPS.Base(), NULL );

	if ( ret != ERROR_SUCCESS )
	{
		Warning( "Player %d : WriteTitleData failed (%.3f sec), err=0x%08X\n",
			m_iController, Plat_FloatTime() - flTimeStart, ret );

		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnProfileDataWriteFailed", "iController", m_iController ) );
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnProfileUnavailable", "iController", m_iController ) );
	}
	else
	{
		Msg( "Player %d : WriteTitleData finished (%.3f sec).\n",
			m_iController, Plat_FloatTime() - flTimeStart );
	}

#elif !defined( NO_STEAM )

	//
	//	Steam stats have been written earlier
	//	Clear dirty state
	//
	V_memset( m_bSaveTitleData, 0, sizeof( m_bSaveTitleData ) );

	g_pPlayerManager->RequestStoreStats();
	GetXWriteOpportunity( m_iController ); // clear XWrite opportunity

	DevMsg( "User stats written.\n" );
#endif

	g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnProfileDataSaved", "iController", m_iController ) );
}

void PlayerLocal::Update()
{
#ifdef _X360
	// When we are playing as guest, no updates
	if ( !m_xuid )
		return;

	UpdatePendingAwardsState();
#endif

	// Load title data if not loaded yet
	LoadTitleData();

	// Save title data if time has come to do so
	WriteTitleData();

#ifndef NO_STEAM
	// Re-request only if we got our callback with an error code. 
	if ( s_flSteamStatsRequestTime && ( Plat_FloatTime() - s_flSteamStatsRequestTime > 10.0 ) && s_bSteamStatsRequestFailed )
	{
		DevWarning( "=========== Failed to retrieve Steam stats (%2.2f sec) =================\n", Plat_FloatTime() - s_flSteamStatsRequestTime );
		steamapicontext->SteamUserStats()->RequestCurrentStats();
		s_flSteamStatsRequestTime = Plat_FloatTime();
	}
#endif
}

void PlayerLocal::Destroy()
{
	m_iController = XUSER_INDEX_NONE;
	m_xuid = 0;
	m_eOnlineState = STATE_OFFLINE;

#ifdef _X360
#elif !defined( NO_STEAM )
	m_CallbackOnUserStatsReceived.Unregister();
	m_CallbackOnPersonaStateChange.Unregister();
#endif

	delete this;
}

void PlayerLocal::RecomputeXUID( char const *szNetwork )
{
	if ( !m_xuid )
		return;

#ifdef _X360
	DWORD dwFlagSignin = XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY;
	if ( !Q_stricmp( "LIVE", szNetwork ) )
		dwFlagSignin = XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY;

	XUSER_SIGNIN_INFO xsi;
	if ( ERROR_SUCCESS != XUserGetSigninInfo( m_iController, dwFlagSignin, &xsi ) ||
		!xsi.xuid )
	{
		if ( ERROR_SUCCESS != XUserGetXUID( m_iController, &xsi.xuid ) )
		{
			DevWarning( "Player::RecomputeXUID failed! Leaving ctrlr%d as %llx.\n", m_iController, m_xuid );
		}
		else
		{
			m_xuid = xsi.xuid;
		}
	}
	else
	{
		m_xuid = xsi.xuid;
	}
#endif
}

void PlayerLocal::DetectOnlineState()
{
#ifdef _X360
	OnlineState_t eOnlineState = IPlayer::STATE_OFFLINE;
	if ( !XBX_GetPrimaryUserIsGuest() )
	{
		if ( XUserGetSigninState( m_iController ) == eXUserSigninState_SignedInToLive )
		{
			eOnlineState = IPlayer::STATE_NO_MULTIPLAYER;
			BOOL bValue = false;
			if ( ERROR_SUCCESS == XUserCheckPrivilege( m_iController, XPRIVILEGE_MULTIPLAYER_SESSIONS, &bValue ) && bValue )
				eOnlineState = IPlayer::STATE_ONLINE;
		}
	}
	m_eOnlineState = eOnlineState;
#endif
}

const UserProfileData & PlayerLocal::GetPlayerProfileData()
{
	return m_ProfileData;
}

void PlayerLocal::LoadPlayerProfileData()
{
#ifdef _X360

	// These are the values we're interested in having returned (must match the indices above)
	const DWORD dwSettingIds[] =
	{
		XPROFILE_GAMERCARD_REP,
		XPROFILE_GAMER_DIFFICULTY,
		XPROFILE_GAMER_CONTROL_SENSITIVITY,
		XPROFILE_GAMER_YAXIS_INVERSION,
		XPROFILE_OPTION_CONTROLLER_VIBRATION,
		XPROFILE_GAMER_PREFERRED_COLOR_FIRST,
		XPROFILE_GAMER_PREFERRED_COLOR_SECOND,
		XPROFILE_GAMER_ACTION_AUTO_AIM,
		XPROFILE_GAMER_ACTION_AUTO_CENTER,
		XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL,
		XPROFILE_GAMERCARD_REGION,
		XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED ,
		XPROFILE_GAMERCARD_CRED,
		XPROFILE_GAMERCARD_ZONE,
		XPROFILE_GAMERCARD_TITLES_PLAYED,
		XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED,
		XPROFILE_GAMERCARD_TITLE_CRED_EARNED,

		// [jason] For debugging voice settings only
		XPROFILE_OPTION_VOICE_MUTED,
		XPROFILE_OPTION_VOICE_THRU_SPEAKERS,
		XPROFILE_OPTION_VOICE_VOLUME
	};

	enum { NUM_PROFILE_SETTINGS = ARRAYSIZE( dwSettingIds ) };

	// First, we call with a NULL pointer and zero size to retrieve the buffer size we'll get back
	DWORD dwResultSize = 0;	// Must be zero to get the correct size back
	XUSER_READ_PROFILE_SETTING_RESULT *pResults = NULL;
	DWORD dwError = g_pMatchExtensions->GetIXOnline()->XUserReadProfileSettings(	0,			// Family ID (current title)
		m_iController,
		NUM_PROFILE_SETTINGS,
		dwSettingIds,
		&dwResultSize,
		pResults,
		NULL );

	// We need this to inform us that it's given us a size back for the buffer
	if ( dwError != ERROR_INSUFFICIENT_BUFFER )
	{
		Warning( "Player %d : LoadPlayerProfileData failed to get size (err=0x%08X)!\n", m_iController, dwError );
		return;
	}

	// Now we allocate that buffer and supply it to the call
	BYTE *pData = (BYTE *) stackalloc( dwResultSize );
	ZeroMemory( pData, dwResultSize );

	pResults = (XUSER_READ_PROFILE_SETTING_RESULT *) pData;

	dwError = g_pMatchExtensions->GetIXOnline()->XUserReadProfileSettings(	0,			// Family ID (current title)
		m_iController,
		NUM_PROFILE_SETTINGS,
		dwSettingIds,
		&dwResultSize,
		pResults,
		NULL );	// Not overlapped, must be synchronous

	// We now have a raw buffer of results
	if ( dwError != ERROR_SUCCESS )
	{
		Warning( "Player %d : LoadTitleData failed to get data (err=0x%08X)!\n", m_iController, dwError );
		return;
	}

	for ( DWORD k = 0; k < pResults->dwSettingsLen; ++ k )
	{
		XUSER_PROFILE_SETTING const &xps = pResults->pSettings[k];
		switch ( xps.dwSettingId )
		{
		case XPROFILE_GAMERCARD_REP:
			m_ProfileData.reputation = xps.data.fData;
			break;
		case XPROFILE_GAMER_DIFFICULTY:
			m_ProfileData.difficulty = xps.data.nData;
			break;
		case XPROFILE_GAMER_CONTROL_SENSITIVITY:
			m_ProfileData.sensitivity = xps.data.nData;
			break;
		case XPROFILE_GAMER_YAXIS_INVERSION:
			m_ProfileData.yaxis = xps.data.nData;
			break;
		case XPROFILE_OPTION_CONTROLLER_VIBRATION:
			m_ProfileData.vibration = xps.data.nData;
			break;
		case XPROFILE_GAMER_PREFERRED_COLOR_FIRST:
			m_ProfileData.color1 = xps.data.nData;
			break;
		case XPROFILE_GAMER_PREFERRED_COLOR_SECOND:
			m_ProfileData.color2 = xps.data.nData;
			break;
		case XPROFILE_GAMER_ACTION_AUTO_AIM:
			m_ProfileData.action_autoaim = xps.data.nData;
			break;
		case XPROFILE_GAMER_ACTION_AUTO_CENTER:
			m_ProfileData.action_autocenter = xps.data.nData;
			break;
		case XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL:
			m_ProfileData.action_movementcontrol = xps.data.nData;
			break;
		case XPROFILE_GAMERCARD_REGION:
			m_ProfileData.region = xps.data.nData;
			break;
		case XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED:
			m_ProfileData.achearned = xps.data.nData;
			break;
		case XPROFILE_GAMERCARD_CRED:
			m_ProfileData.cred = xps.data.nData;
			break;
		case XPROFILE_GAMERCARD_ZONE:
			m_ProfileData.zone = xps.data.nData;
			break;
		case XPROFILE_GAMERCARD_TITLES_PLAYED:
			m_ProfileData.titlesplayed = xps.data.nData;
			break;
		case XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED:
			m_ProfileData.titleachearned = xps.data.nData;
			break;
		case XPROFILE_GAMERCARD_TITLE_CRED_EARNED:
			m_ProfileData.titlecred = xps.data.nData;
			break;

			// [jason] For debugging voice settings only:
		case XPROFILE_OPTION_VOICE_MUTED:
			DevMsg( "Player %d : XPROFILE_OPTION_VOICE_MUTED setting: %d\n", m_iController, xps.data.nData );
			break;
		case XPROFILE_OPTION_VOICE_THRU_SPEAKERS:
			DevMsg( "Player %d : XPROFILE_OPTION_VOICE_THRU_SPEAKERS setting: %d\n", m_iController, xps.data.nData );
			break;
		case XPROFILE_OPTION_VOICE_VOLUME:
			DevMsg( "Player %d : XPROFILE_OPTION_VOICE_VOLUME setting: %d\n", m_iController, xps.data.nData );
			break;
		}
	}
#endif
#ifdef _PS3
	m_ProfileData.vibration = 3; // vibration enabled on PS3
#endif

	DevMsg( "Player %d : LoadPlayerProfileData finished\n", m_iController );
}

MatchmakingData* PlayerLocal::GetPlayerMatchmakingData( void )
{
	return &m_MatchmakingData;
}

void PlayerLocal::UpdatePlayerMatchmakingData( int mmDataType )
{
#if defined ( _X360 )
	if ( mmDataType < 0 || mmDataType >= MMDATA_TYPE_COUNT )
	{
		DevMsg( "Invalid matchmaking data type passed to UpdatePlayerMatchmakingData ( %d )", mmDataType );
		return;
	}

	DevMsg( "Player::UpdatePlayerMatchmakingData( ctrlr%d; mmDataType%d )\n", GetPlayerIndex(), mmDataType );

	// Now obtain the avg calculator for the difficulty
	// Calculator operates on "avgValue" and "newValue"
	CExpressionCalculator calc( g_pMMF->GetMatchTitleGameSettingsMgr()->GetFormulaAverage( mmDataType ) );
	char const *szAvg = "avgValue";
	char const *szNew = "newValue";
	float flResult;

	//
	// Average up our data
	//

#define CALC_AVG( field ) \
	calc.SetVariable( szAvg, m_MatchmakingData.field[mmDataType][MMDATA_SCOPE_LIFETIME] ); \
	calc.SetVariable( szNew, m_MatchmakingData.field[mmDataType][MMDATA_SCOPE_ROUND] * MM_AVG_CONST ); \
	if ( calc.Evaluate( flResult ) ) \
		m_MatchmakingData.field[mmDataType][MMDATA_SCOPE_LIFETIME] = flResult;

	CALC_AVG( mContribution );
	CALC_AVG( mMVPs );
	CALC_AVG( mKills );
	CALC_AVG( mDeaths );
	CALC_AVG( mHeadShots );
	CALC_AVG( mDamage );
	CALC_AVG( mShotsFired );
	CALC_AVG( mShotsHit );
	CALC_AVG( mDominations );
	// Average rounds played makes no sense since the average is a per round average; increment both the average and running totals
	m_MatchmakingData.mRoundsPlayed[mmDataType][MMDATA_SCOPE_LIFETIME] += 1;
	m_MatchmakingData.mRoundsPlayed[mmDataType][MMDATA_SCOPE_ROUND] += 1;

#undef CALC_AVG
#endif // #if defined ( _X360 )
}

void PlayerLocal::ResetPlayerMatchmakingData( int mmDataScope )
{
#if defined ( _X360 )
	if ( mmDataScope < 0 || mmDataScope >= MMDATA_SCOPE_COUNT )
	{
		DevMsg( "Invalid matchmaking data scope passed to ResetPlayerMatchmakingData ( %d )", mmDataScope );
		return;
	}

	DevMsg( "Player::ResetPlayerMatchmakingData( ctrlr%d; mmDataScope%d )\n", GetPlayerIndex(), mmDataScope );

	ConVarRef score_default( "score_default" );

	for ( int i=0; i<MMDATA_TYPE_COUNT; ++i )
	{
		m_MatchmakingData.mContribution[i][mmDataScope] = score_default.GetInt();
		m_MatchmakingData.mMVPs[i][mmDataScope] = 0;
		m_MatchmakingData.mKills[i][mmDataScope] = 0;
		m_MatchmakingData.mDeaths[i][mmDataScope] = 0;
		m_MatchmakingData.mHeadShots[i][mmDataScope] = 0;
		m_MatchmakingData.mDamage[i][mmDataScope] = 0;
		m_MatchmakingData.mShotsFired[i][mmDataScope] = 0;
		m_MatchmakingData.mShotsHit[i][mmDataScope] = 0;
		m_MatchmakingData.mDominations[i][mmDataScope] = 0;
		m_MatchmakingData.mRoundsPlayed[i][mmDataScope] = 0;
	}
#endif // #if defined ( _X360 )
}

const void * PlayerLocal::GetPlayerTitleData( int iTitleDataIndex )
{
	if ( iTitleDataIndex >= 0 && iTitleDataIndex < TITLE_DATA_COUNT )
	{
		return m_bufTitleData[ iTitleDataIndex ];
	}
	else
	{
		return NULL;
	}
}

void PlayerLocal::UpdatePlayerTitleData( TitleDataFieldsDescription_t const *fdKey, const void *pvNewTitleData, int numNewBytes )
{
	if ( !fdKey || (fdKey->m_eDataType == fdKey->DT_0) || !pvNewTitleData || (numNewBytes <= 0) || !fdKey->m_szFieldName )
		return;
	Assert( TITLE_DATA_COUNT == TitleDataFieldsDescription_t::DB_TD_COUNT );
	if ( fdKey->m_iTitleDataBlock < 0 || fdKey->m_iTitleDataBlock >= TitleDataFieldsDescription_t::DB_TD_COUNT )
		return;

#if !defined( NO_STEAM ) && !defined( _CERT )
	if ( steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->GetConnectedUniverse() == k_EUniverseBeta )
#endif
	{
#if ( defined( _GAMECONSOLE ) && !defined( _CERT ) )
		// Validate that the caller is not forging the field description
		bool bKeyForged = true;
		for ( TitleDataFieldsDescription_t const *fdCheck = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
			fdCheck && fdCheck->m_szFieldName; ++ fdCheck )
		{
			if ( fdCheck == fdKey )
			{
				bKeyForged = false;
				break;
			}
		}
		if ( bKeyForged )
		{
			DevWarning( "PlayerLocal::UpdatePlayerTitleData( %s ) with invalid key!\n", fdKey->m_szFieldName );
			Assert( 0 );
			return;
		}
#endif
	}

	// Validate data size
	if ( fdKey->m_eDataType/8 != numNewBytes )
	{
		DevWarning( "PlayerLocal::UpdatePlayerTitleData( %s ) new data size %d != description size %d!\n", fdKey->m_szFieldName, numNewBytes, fdKey->m_eDataType/8 );
		Assert( 0 );
		return;
	}

	// Debug output for the write attempt
	switch ( fdKey->m_eDataType )
	{
	case TitleDataFieldsDescription_t::DT_uint8:
	case TitleDataFieldsDescription_t::DT_BITFIELD:
//		DevMsg( "PlayerLocal(%s)::UpdatePlayerTitleData: %s = %d (0x%02X)\n", GetName(), fdKey->m_szFieldName, *( uint8 const * ) pvNewTitleData, *( uint8 const * ) pvNewTitleData );
		break;
	case TitleDataFieldsDescription_t::DT_uint16:
//		DevMsg( "PlayerLocal(%s)::UpdatePlayerTitleData: %s = %d (0x%04X)\n", GetName(), fdKey->m_szFieldName, *( uint16 const * ) pvNewTitleData, *( uint16 const * ) pvNewTitleData );
		break;
	case TitleDataFieldsDescription_t::DT_uint32:
//		DevMsg( "PlayerLocal(%s)::UpdatePlayerTitleData: %s = %d (0x%08X)\n", GetName(), fdKey->m_szFieldName, *( uint32 const * ) pvNewTitleData, *( uint32 const * ) pvNewTitleData );
		break;
	case TitleDataFieldsDescription_t::DT_uint64:
//		DevMsg( "PlayerLocal(%s)::UpdatePlayerTitleData: %s = %llu (0x%llX)\n", GetName(), fdKey->m_szFieldName, *( uint64 const * ) pvNewTitleData, *( uint64 const * ) pvNewTitleData );
		break;
	case TitleDataFieldsDescription_t::DT_float:
//		DevMsg( "PlayerLocal(%s)::UpdatePlayerTitleData: %s = %f (%0.2f)\n", GetName(), fdKey->m_szFieldName, *( float const * ) pvNewTitleData, *( float const * ) pvNewTitleData );
		break;
	}

	// Check if the title allows the requested stat update
	KeyValues *kvTitleRequest = new KeyValues( "stat" );
	KeyValues::AutoDelete autodelete_kvTitleRequest( kvTitleRequest );
	kvTitleRequest->SetString( "name", fdKey->m_szFieldName );
	if ( !g_pMMF->GetMatchTitleGameSettingsMgr()->AllowClientProfileUpdate( kvTitleRequest ) )
		return;

	//
	// Copy off the old data and check that the new data is different
	// At the same time update internal buffers with new data
	//
	uint8 *pvData = ( uint8 * ) m_bufTitleData[ fdKey->m_iTitleDataBlock ];
	switch ( fdKey->m_eDataType )
	{
	case TitleDataFieldsDescription_t::DT_BITFIELD:
		{
			uint8 *pvOldData = ( uint8 * ) stackalloc( numNewBytes );
			uint8 bBitValue = !!*( ( uint8 const * ) pvNewTitleData );
			uint8 bBitMask = ( 1 << (fdKey->m_numBytesOffset%8) );
			memcpy( pvOldData, pvData + fdKey->m_numBytesOffset/8, numNewBytes );
			if ( !!( bBitMask & pvOldData[0] ) == bBitValue )
				return;
			if ( bBitValue )
				pvOldData[0] |= bBitMask;
			else
				pvOldData[0] &=~bBitMask;
			memcpy( pvData + fdKey->m_numBytesOffset/8, pvOldData, numNewBytes );
		}
		break;
	default:
		if ( !memcmp( pvData + fdKey->m_numBytesOffset, pvNewTitleData, numNewBytes ) )
			return;
		memcpy( pvData + fdKey->m_numBytesOffset, pvNewTitleData, numNewBytes );
		break;
	}

	// Check our "guest" status
#ifdef _GAMECONSOLE
	bool bRegisteredPlayer = false;
	for ( int k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetUserId( k ) == m_iController )
		{
			if ( XBX_GetUserIsGuest( k ) )
			{
				DevMsg( "pPlayerLocal(%s)->UpdatePlayerTitleData not saving for guests.\n", GetName() );
				return;
			}
			bRegisteredPlayer = true;
			break;
		}
	}
	if ( !bRegisteredPlayer )
	{
		DevMsg( "pPlayerLocal(%s)->UpdatePlayerTitleData not saving for not participating gamers.\n", GetName() );
		Assert( 0 ); // title code shouldn't be calling UpdateAwardsData for players not in active gameplay, title bug?
		return;
	}
#endif

	// Mark stats to be stored at next available opportunity
	m_bSaveTitleData[ fdKey->m_iTitleDataBlock ] = true;

#ifndef NO_STEAM
	// On Steam we can freely upload stats rights now
	//
	// Prepare the data
	//
	switch( fdKey->m_eDataType )
	{
	case TitleDataFieldsDescription_t::DT_uint8:
	case TitleDataFieldsDescription_t::DT_uint16:
	case TitleDataFieldsDescription_t::DT_uint32:
		{
			uint32 i32field[4] = {0};

			memcpy( &i32field[3],
				&m_bufTitleData[ fdKey->m_iTitleDataBlock ][ fdKey->m_numBytesOffset ],
				fdKey->m_eDataType / 8 );

			i32field[0] = *( uint32 * )( &i32field[3] );
			i32field[1] = *( uint16 * )( &i32field[3] );
			i32field[2] = *( uint8  * )( &i32field[3] );

			SetSteamStatWithPotentialOverride( fdKey->m_szFieldName, ( int32 ) i32field[ 2 - ( fdKey->m_eDataType / 16 ) ] );
		}
		break;

	case TitleDataFieldsDescription_t::DT_float:
		{
			float flField = 0.0f;

			memcpy( &flField,
				&m_bufTitleData[ fdKey->m_iTitleDataBlock ][ fdKey->m_numBytesOffset ],
				fdKey->m_eDataType / 8 );

			SetSteamStatWithPotentialOverride( fdKey->m_szFieldName, flField );
		}
		break;

	case TitleDataFieldsDescription_t::DT_uint64:
		{
			uint32 i32field[2] = { 0 };

			memcpy( &i32field[0],
				&m_bufTitleData[ fdKey->m_iTitleDataBlock ][ fdKey->m_numBytesOffset ],
				fdKey->m_eDataType / 8 );

			char chBuffer[ 256 ] = {0};

			for ( int k = 0; k < ARRAYSIZE( i32field ); ++ k )
			{
				Q_snprintf( chBuffer, ARRAYSIZE( chBuffer ), "%s.%d", fdKey->m_szFieldName, k );
				SetSteamStatWithPotentialOverride( chBuffer, ( int32 ) i32field[k] );
			}
		}
		break;

	case TitleDataFieldsDescription_t::DT_BITFIELD:
		{
		#ifdef STEAM_PACK_BITFIELDS
			char chStatField[64] = {0};
			uint32 uiOffsetInTermsOfUINT32 = fdKey->m_numBytesOffset/32;
			V_snprintf( chStatField, sizeof( chStatField ), "bitfield_%02u_%03X", fdKey->m_iTitleDataBlock + 1, uiOffsetInTermsOfUINT32*4 );
			int32 iCombinedBitValue = ( reinterpret_cast< uint32 * >( &m_bufTitleData[fdKey->m_iTitleDataBlock][0] ) )[ uiOffsetInTermsOfUINT32 ];
			SetSteamStatWithPotentialOverride( chStatField, iCombinedBitValue );
		#else
			int32 i32field = !!(
				m_bufTitleData[ fdKey->m_iTitleDataBlock ][ fdKey->m_numBytesOffset/8 ]
			& ( 1 << ( fdKey->m_numBytesOffset % 8 ) ) );
			SetSteamStatWithPotentialOverride( fdKey->m_szFieldName, i32field );
		#endif
		}
		break;
	}
#endif // NO_STEAM

	// If a component achievement was affected, evaluate its state
	int numFieldNameChars = Q_strlen( fdKey->m_szFieldName );
	if ( ( numFieldNameChars > 0 ) && ( fdKey->m_szFieldName[numFieldNameChars - 1] == ']' ) )
	{
		EvaluateAwardsStateBasedOnStats();
	}
}

void PlayerLocal::GetLeaderboardData( KeyValues *pLeaderboardInfo )
{
	// Iterate over all views specified
	for ( KeyValues *pView = pLeaderboardInfo->GetFirstTrueSubKey(); pView; pView = pView->GetNextTrueSubKey() )
	{
		char const *szViewName = pView->GetName();

		KeyValues *pCurrentData = m_pLeaderboardData->FindKey( szViewName );

		// If no such data yet or refresh was requested, then queue this request
		if ( pView->GetInt( ":refresh" ) || !pCurrentData )
		{
			if ( g_pLeaderboardRequestQueue )
				g_pLeaderboardRequestQueue->Request( pView );
		}

		// If we have data, then fill in the values
		pView->MergeFrom( pCurrentData, KeyValues::MERGE_KV_BORROW );
	}
}

void PlayerLocal::UpdateLeaderboardData( KeyValues *pLeaderboardInfo )
{
	DevMsg( "PlayerLocal::UpdateLeaderboardData for %s ...\n", GetName() );

#ifdef _X360
	IX360LeaderboardBatchWriter *pLbWriter = MMX360_CreateLeaderboardBatchWriter( m_xuid );
#endif

	// Iterate over all views specified
	for ( KeyValues *pView = pLeaderboardInfo->GetFirstTrueSubKey(); pView; pView = pView->GetNextTrueSubKey() )
	{
		char const *szViewName = pView->GetName();

		// Check if the title allows the requested leaderboard update
		KeyValues *kvTitleRequest = new KeyValues( "leaderboard" );
		KeyValues::AutoDelete autodelete_kvTitleRequest( kvTitleRequest );
		kvTitleRequest->SetString( "name", szViewName );
		if ( !g_pMMF->GetMatchTitleGameSettingsMgr()->AllowClientProfileUpdate( kvTitleRequest ) )
			continue;

		// Find leaderboard description
		KeyValues *pDescription = g_pMMF->GetMatchTitle()->DescribeTitleLeaderboard( szViewName );
		if ( !pDescription )
		{
			DevWarning( "   View %s failed to allocate description!\n", szViewName );
		}
		KeyValues::AutoDelete autodelete_pDescription( pDescription );
		KeyValues *pCurrentData = m_pLeaderboardData->FindKey( szViewName );
		if ( !pDescription->GetBool( ":nocache" ) && !pCurrentData )
		{
			pCurrentData = new KeyValues( szViewName );
			m_pLeaderboardData->AddSubKey( pCurrentData );
		}

		// On PC we just issue a write per each request, no batching
		if ( !IsX360() )
		{
			if ( pDescription )
			{
#if !defined( _X360 ) && !defined( NO_STEAM )
				Steam_WriteLeaderboardData( pDescription, pView );
#endif
			}
			continue;
		}

		// Check if the rating field passes rule requirement
		if ( KeyValues *pValDesc = pDescription->FindKey( ":rating" ) )
		{
			char const *szValue = pDescription->GetString( ":rating/name" );
			KeyValues *val = pView->FindKey( szValue );
			if ( !val || !*szValue )
			{
				DevWarning( "   View %s is rated, but no :rating field '%s' in update!\n", szViewName, szValue );
				continue;
			}

			char const *szRule = pValDesc->GetString( "rule" );
			IPropertyRule *pRuleObj = GetRuleByName( szRule );
			if ( !pRuleObj )
			{
				DevWarning( "   View %s is rated, but invalid rule '%s' specified!\n", szViewName, szRule );
				continue;
			}

			uint64 uiValue = val->GetUint64();
			uint64 uiCurrent = pCurrentData->GetUint64( szValue );
			if ( !pRuleObj->ApplyRuleUint64( uiCurrent, uiValue ) )
			{
				DevMsg( "   View %s is rated, rejected by rule '%s' ( old %lld, new %lld )!\n", szViewName, szRule, uiCurrent, val->GetUint64() );
				continue;
			}

			DevMsg( "   View %s is rated, rule '%s' passed ( old %lld, new %lld )!\n", szViewName, szRule, uiCurrent, uiValue );
			pCurrentData->SetUint64( ":rating", uiValue );
			// ... and proceed setting other contexts and properties
		}

		// Iterate over all values
		for ( KeyValues *val = pView->GetFirstValue(); val; val = val->GetNextValue() )
		{
			char const *szValue = val->GetName();

			if ( szValue[0] == ':' )
				continue;	// no update for system fields

			if ( KeyValues *pValDesc = pDescription->FindKey( szValue ) )
			{
				char const *szRule = pValDesc->GetString( "rule" );
				IPropertyRule *pRuleObj = GetRuleByName( szRule );
				if ( !pRuleObj )
				{
					DevWarning( "   View %s/%s has invalid rule '%s' specified!\n", szViewName, szValue, szRule );
					continue;
				}

				char const *szType = pValDesc->GetString( "type" );

				if ( !Q_stricmp( "uint64", szType ) )
				{
					uint64 uiValue = val->GetUint64();
					uint64 uiCurrent = pCurrentData->GetUint64( szValue );

					// Check if new value passes the rule
					if ( !pRuleObj->ApplyRuleUint64( uiCurrent, uiValue ) )
					{
						DevMsg( "   View %s/%s rejected by rule '%s' ( old %lld, new %lld )!\n",
							szViewName, szValue, szRule, uiCurrent, val->GetUint64() );
						continue;
					}

					DevMsg( "   View %s/%s updated by rule '%s' ( old %lld, new %lld )!\n",
						szViewName, szValue, szRule, uiCurrent, uiValue );
					pCurrentData->SetUint64( szValue, uiValue );
					
#ifdef _X360
					if ( pLbWriter )
					{
						XUSER_PROPERTY xProp = {0};
						xProp.dwPropertyId = pValDesc->GetInt( "prop" );
						xProp.value.type = XUSER_DATA_TYPE_INT64;
						xProp.value.i64Data = uiValue;

						pLbWriter->AddProperty( pDescription->GetInt( ":id" ), xProp );
					}
#endif
				}
				else if ( !Q_stricmp( "float", szType ) )
				{
					float flValue = val->GetFloat();
					float flCurrent = pCurrentData->GetFloat( szValue );

					// Check if new value passes the rule
					if ( !pRuleObj->ApplyRuleFloat( flCurrent, flValue ) )
					{
						DevMsg( "   View %s/%s rejected by rule '%s' ( old %.4f, new %.4f )!\n",
							szViewName, szValue, szRule, flCurrent, val->GetFloat() );
						continue;
					}

					DevMsg( "   View %s/%s updated by rule '%s' ( old %.4f, new %.4f )!\n",
						szViewName, szValue, szRule, flCurrent, flValue );
					pCurrentData->SetFloat( szValue, flValue );
					
#ifdef _X360
					if ( pLbWriter )
					{
						XUSER_PROPERTY xProp = {0};
						xProp.dwPropertyId = pValDesc->GetInt( "prop" );
						xProp.value.type = XUSER_DATA_TYPE_FLOAT;
						xProp.value.i64Data = flValue;

						pLbWriter->AddProperty( pDescription->GetInt( ":id" ), xProp );
					}
#endif
				}
				else
				{
					DevWarning( "   View %s/%s has invalid type '%s' specified!\n", szViewName, szValue, szType );
				}
			}
			else
			{
				DevWarning( "   View %s/%s is missing description!\n", szViewName, szValue );
			}
		}
	}

	//
	// Now submit all accumulated leaderboard writes
	//
#ifdef _X360
	if ( pLbWriter )
	{
		pLbWriter->WriteBatchAndDestroy();
		pLbWriter = NULL;
	}
	else
	{
		Warning( "PlayerLocal::UpdateLeaderboardData failed to save leaderboard data to writer!\n" );
		Assert( 0 );
	}
#endif

	DevMsg( "PlayerLocal::UpdateLeaderboardData for %s finished.\n", GetName() );
}

void PlayerLocal::OnLeaderboardRequestFinished( KeyValues *pLeaderboardData )
{
	m_pLeaderboardData->MergeFrom( pLeaderboardData, KeyValues::MERGE_KV_UPDATE );

	KeyValues *kvEvent = new KeyValues( "OnProfileLeaderboardData" );
	kvEvent->SetInt( "iController", m_iController );
	kvEvent->MergeFrom( pLeaderboardData, KeyValues::MERGE_KV_UPDATE );
	g_pMatchEventsSubscription->BroadcastEvent( kvEvent );
}

void PlayerLocal::GetAwardsData( KeyValues *pAwardsData )
{
	for ( KeyValues *kvValue = pAwardsData->GetFirstValue(); kvValue; kvValue = kvValue->GetNextValue() )
	{
		char const *szName = kvValue->GetName();

		// If title is requesting all achievement IDs
		if ( !Q_stricmp( "@achievements", szName ) )
		{
			for ( int k = 0; k < m_arrAchievementsEarned.Count(); ++ k )
			{
				KeyValues *kvAchValue = new KeyValues( "" );
				kvAchValue->SetInt( NULL, m_arrAchievementsEarned[k] );
				kvValue->AddSubKey( kvAchValue );
			}
			continue;
		}

		// If title is requesting all awards IDs
		if ( !Q_stricmp( "@awards", szName ) )
		{
			for ( int k = 0; k < m_arrAvatarAwardsEarned.Count(); ++ k )
			{
				KeyValues *kvAchValue = new KeyValues( "" );
				kvAchValue->SetInt( NULL, m_arrAvatarAwardsEarned[k] );
				kvValue->AddSubKey( kvAchValue );
			}
			continue;
		}

		// Try to find the achievement in the achievement map
		for ( TitleAchievementsDescription_t const *pAchievement = g_pMatchFramework->GetMatchTitle()->DescribeTitleAchievements();
			pAchievement && pAchievement->m_szAchievementName; ++ pAchievement )
		{
			if ( !Q_stricmp( szName, pAchievement->m_szAchievementName ) )
			{
				kvValue->SetInt( "", ( m_arrAchievementsEarned.Find( pAchievement->m_idAchievement ) != m_arrAchievementsEarned.InvalidIndex() ) ? 1 : 0 );
				szName = NULL;
				break;
			}
		}
		if ( !szName )
			continue;

		// Try to find the avatar award in the map
		for ( TitleAvatarAwardsDescription_t const *pAvAward = g_pMatchFramework->GetMatchTitle()->DescribeTitleAvatarAwards();
			pAvAward && pAvAward->m_szAvatarAwardName; ++ pAvAward )
		{
			if ( !Q_stricmp( szName, pAvAward->m_szAvatarAwardName ) )
			{
				kvValue->SetInt( "", ( m_arrAvatarAwardsEarned.Find( pAvAward->m_idAvatarAward ) != m_arrAvatarAwardsEarned.InvalidIndex() ) ? 1 : 0 );
				szName = NULL;
				break;
			}
		}
		if ( !szName )
			continue;

		DevWarning( "pPlayerLocal(%s)->GetAwardsData(%s) UNKNOWN NAME!\n", GetName(), szName );
	}
}

void PlayerLocal::UpdateAwardsData( KeyValues *pAwardsData )
{
	if ( !pAwardsData ) 
		return;

#ifdef _X360
	if ( !m_xuid )
		return;

	if ( GetAssumedSigninState() == eXUserSigninState_NotSignedIn )
		return;
#endif

	// Check our "guest" status
#ifdef _GAMECONSOLE
	bool bRegisteredPlayer = false;
	for ( int k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetUserId( k ) == m_iController )
		{
			if ( XBX_GetUserIsGuest( k ) )
			{
				DevMsg( "pPlayerLocal(%s)->UpdateAwardsData is unavailable for guests.\n", GetName() );
				return;
			}
			bRegisteredPlayer = true;
			break;
		}
	}
	if ( !bRegisteredPlayer )
	{
		DevMsg( "pPlayerLocal(%s)->UpdateAwardsData is unavailable for not participating gamers.\n", GetName() );
		Assert( 0 ); // title code shouldn't be calling UpdateAwardsData for players not in active gameplay, title bug?
		return;
	}
#endif

	for ( KeyValues *kvValue = pAwardsData->GetFirstValue(); kvValue; kvValue = kvValue->GetNextValue() )
	{
		char const *szName = kvValue->GetName();
		if ( !kvValue->GetInt( "" ) )
			continue;

		// Check if the title allows the requested award
		KeyValues *kvTitleRequest = new KeyValues( "award" );
		KeyValues::AutoDelete autodelete_kvTitleRequest( kvTitleRequest );
		kvTitleRequest->SetString( "name", szName );
		if ( !g_pMMF->GetMatchTitleGameSettingsMgr()->AllowClientProfileUpdate( kvTitleRequest ) )
			continue;

		// Try to find the achievement in the achievement map
		for ( TitleAchievementsDescription_t const *pAchievement = g_pMatchFramework->GetMatchTitle()->DescribeTitleAchievements();
			pAchievement && pAchievement->m_szAchievementName; ++ pAchievement )
		{
			if ( !Q_stricmp( szName, pAchievement->m_szAchievementName ) )
			{
				// Found the achievement to award
				if ( m_arrAchievementsEarned.Find( pAchievement->m_idAchievement ) == m_arrAchievementsEarned.InvalidIndex() )
				{
#ifdef _X360
					XPendingAsyncAward_t *pAsync = new XPendingAsyncAward_t;
					Q_memset( pAsync, 0, sizeof( XPendingAsyncAward_t ) );
					pAsync->m_flStartTimestamp = Plat_FloatTime();
					pAsync->m_pLocalPlayer = this;
					pAsync->m_eType = XPendingAsyncAward_t::TYPE_ACHIEVEMENT;
					pAsync->m_pAchievementDesc = pAchievement;
					pAsync->m_xAchievement.dwUserIndex = m_iController;
					pAsync->m_xAchievement.dwAchievementId = pAchievement->m_idAchievement;
					DWORD dwErrCode = XUserWriteAchievements( 1, &pAsync->m_xAchievement, &pAsync->m_xOverlapped );
					if ( dwErrCode == ERROR_IO_PENDING )
					{
						DevMsg( "pPlayerLocal(%s)->UpdateAwardsData(%s) initiated async award.\n", GetName(), pAchievement->m_szAchievementName );
						s_arrPendingAsyncAwards.AddToTail( pAsync );
						m_arrAchievementsEarned.AddToTail( pAchievement->m_idAchievement );
					}
					else if ( ( dwErrCode == 1 ) || ( dwErrCode == 0 ) )
					{
						delete pAsync;
						DevMsg( "pPlayerLocal(%s)->UpdateAwardsData(%s) already awarded by system.\n", GetName(), pAchievement->m_szAchievementName );
						m_arrAchievementsEarned.AddToTail( pAchievement->m_idAchievement );
					}
					else
					{
						delete pAsync;
						DevWarning( "pPlayerLocal(%s)->UpdateAwardsData(%s) failed to initiate async award.\n", GetName(), pAchievement->m_szAchievementName );
						g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
							"OnProfileUnavailable", "iController", m_iController ) );
					}
#elif !defined( NO_STEAM )
					bool bSteamResult = steamapicontext->SteamUserStats()->SetAchievement( pAchievement->m_szAchievementName );
					if ( bSteamResult )
					{
						m_bSaveTitleData[0] = true; // signal that stats must be stored
						m_arrAchievementsEarned.AddToTail( pAchievement->m_idAchievement );
						DevMsg( "pPlayerLocal(%s)->UpdateAwardsData(%s) set achievement and stored stats.\n", GetName(), pAchievement->m_szAchievementName );
						
						KeyValues *kvAwardedEvent = new KeyValues( "OnPlayerAward" );
						kvAwardedEvent->SetInt( "iController", m_iController );
						kvAwardedEvent->SetString( "award", pAchievement->m_szAchievementName );
						g_pMatchEventsSubscription->BroadcastEvent( kvAwardedEvent );
					}
					else
					{
						DevWarning( "pPlayerLocal(%s)->UpdateAwardsData(%s) failed to set in Steam.\n", GetName(), pAchievement->m_szAchievementName );
					}
#else
					DevMsg( "pPlayerLocal(%s)->UpdateAwardsData(%s) skipped.\n", GetName(), pAchievement->m_szAchievementName );
#endif
				}
				else
				{
					DevMsg( "pPlayerLocal(%s)->UpdateAwardsData(%s) already earned.\n", GetName(), pAchievement->m_szAchievementName );
				}
				szName = NULL;
				break;
			}
		}
		if ( !szName )
			continue;

		// Try to find the avatar award in the map
		for ( TitleAvatarAwardsDescription_t const *pAvAward = g_pMatchFramework->GetMatchTitle()->DescribeTitleAvatarAwards();
			pAvAward && pAvAward->m_szAvatarAwardName; ++ pAvAward )
		{
			if ( !Q_stricmp( szName, pAvAward->m_szAvatarAwardName ) )
			{
				// Found the avaward to award
				if ( m_arrAvatarAwardsEarned.Find( pAvAward->m_idAvatarAward ) == m_arrAvatarAwardsEarned.InvalidIndex() )
				{
#ifdef _X360
					XPendingAsyncAward_t *pAsync = new XPendingAsyncAward_t;
					Q_memset( pAsync, 0, sizeof( XPendingAsyncAward_t ) );
					pAsync->m_flStartTimestamp = Plat_FloatTime();
					pAsync->m_pLocalPlayer = this;
					pAsync->m_eType = XPendingAsyncAward_t::TYPE_AVATAR_AWARD;
					pAsync->m_pAvatarAwardDesc = pAvAward;
					pAsync->m_xAvatarAsset.dwUserIndex = m_iController;
					pAsync->m_xAvatarAsset.dwAwardId = pAvAward->m_idAvatarAward;
					DWORD dwErrCode = XUserAwardAvatarAssets( 1, &pAsync->m_xAvatarAsset, &pAsync->m_xOverlapped );
					if ( dwErrCode == ERROR_IO_PENDING )
					{
						DevMsg( "pPlayerLocal(%s)->UpdateAwardsData(%s) initiated async award.\n", GetName(), pAvAward->m_szAvatarAwardName );
						s_arrPendingAsyncAwards.AddToTail( pAsync );
						m_arrAvatarAwardsEarned.AddToTail( pAvAward->m_idAvatarAward );
					}
					else if ( ( dwErrCode == 1 ) || ( dwErrCode == 0 ) )
					{
						delete pAsync;
						DevMsg( "pPlayerLocal(%s)->UpdateAwardsData(%s) already awarded by system.\n", GetName(), pAvAward->m_szAvatarAwardName );
						m_arrAvatarAwardsEarned.AddToTail( pAvAward->m_idAvatarAward );
						if ( TitleDataFieldsDescription_t const *fdBitfield = TitleDataFieldsDescriptionFindByString( g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage(), pAvAward->m_szTitleDataBitfieldStatName ) )
						{
							TitleDataFieldsDescriptionSetBit( fdBitfield, this, true );
						}
					}
					else
					{
						delete pAsync;
						DevWarning( "pPlayerLocal(%s)->UpdateAwardsData(%s) failed to initiate async award.\n", GetName(), pAvAward->m_szAvatarAwardName );
						g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
							"OnProfileUnavailable", "iController", m_iController ) );
					}
#else
					DevMsg( "pPlayerLocal(%s)->UpdateAwardsData(%s) skipped.\n", GetName(), pAvAward->m_szAvatarAwardName );
#endif
				}
				else
				{
					DevMsg( "pPlayerLocal(%s)->UpdateAwardsData(%s) already earned.\n", GetName(), pAvAward->m_szAvatarAwardName );
				}
				szName = NULL;
				break;
			}
		}
		if ( !szName )
			continue;

		DevWarning( "pPlayerLocal(%s)->write_awards(%s) UNKNOWN NAME!\n", GetName(), szName );
	}
}

#ifdef _X360
void PlayerLocal::UpdatePendingAwardsState()
{
	bool bWarningFired = false;
	for ( int k = 0; k < s_arrPendingAsyncAwards.Count(); ++ k )
	{
		XPendingAsyncAward_t *pPendingAsyncAward = s_arrPendingAsyncAwards[k];
		if ( pPendingAsyncAward->m_pLocalPlayer && ( pPendingAsyncAward->m_pLocalPlayer != this ) )
			continue;
		if ( !XHasOverlappedIoCompleted( &pPendingAsyncAward->m_xOverlapped ) )
			continue;
		
		DWORD result = 0;
		DWORD dwXresult = XGetOverlappedResult( &pPendingAsyncAward->m_xOverlapped, &result, false );
		bool bSuccessfullyEarned = ( dwXresult == ERROR_SUCCESS );
		if ( this == pPendingAsyncAward->m_pLocalPlayer )
		{
			if ( !bSuccessfullyEarned )
			{
				switch ( pPendingAsyncAward->m_eType )
				{
				case XPendingAsyncAward_t::TYPE_ACHIEVEMENT:
					m_arrAchievementsEarned.FindAndFastRemove( pPendingAsyncAward->m_pAchievementDesc->m_idAchievement );
					break;
				case XPendingAsyncAward_t::TYPE_AVATAR_AWARD:
					m_arrAchievementsEarned.FindAndFastRemove( pPendingAsyncAward->m_pAvatarAwardDesc->m_idAvatarAward );
					break;
				}
				if ( !bWarningFired )
				{
					g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
						"OnProfileUnavailable", "iController", m_iController ) );
					bWarningFired = true;
				}
			}
			else
			{
				if ( pPendingAsyncAward->m_eType == XPendingAsyncAward_t::TYPE_AVATAR_AWARD )
				{
					if ( TitleDataFieldsDescription_t const *fdBitfield = TitleDataFieldsDescriptionFindByString( g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage(),
						pPendingAsyncAward->m_pAvatarAwardDesc->m_szTitleDataBitfieldStatName ) )
					{
						TitleDataFieldsDescriptionSetBit( fdBitfield, this, true );
					}
				}

				KeyValues *kvAwardedEvent = new KeyValues( "OnPlayerAward" );
				kvAwardedEvent->SetInt( "iController", m_iController );
				if ( pPendingAsyncAward->m_eType == XPendingAsyncAward_t::TYPE_AVATAR_AWARD )
					kvAwardedEvent->SetString( "award", pPendingAsyncAward->m_pAvatarAwardDesc->m_szAvatarAwardName );
				if ( pPendingAsyncAward->m_eType == XPendingAsyncAward_t::TYPE_ACHIEVEMENT )
					kvAwardedEvent->SetString( "award", pPendingAsyncAward->m_pAchievementDesc->m_szAchievementName );
				g_pMatchEventsSubscription->BroadcastEvent( kvAwardedEvent );
			}
		}

		// Remove the pending structure
		s_arrPendingAsyncAwards.Remove( k -- );
		delete pPendingAsyncAward;
	}
}
#endif

void PlayerLocal::EvaluateAwardsStateBasedOnStats()
{
	TitleDataFieldsDescription_t const *pTitleDataStorage = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
	
	KeyValues *kvAwards = NULL;
	KeyValues::AutoDelete autodelete_kvAwards( kvAwards );

	//
	// Evaluate the state of component achievements
	//
	for ( TitleAchievementsDescription_t const *pAchievement = g_pMatchFramework->GetMatchTitle()->DescribeTitleAchievements();
		pAchievement && pAchievement->m_szAchievementName; ++ pAchievement )
	{
		if ( pAchievement->m_numComponents <= 1 )
			continue;
		if ( m_arrAchievementsEarned.Find( pAchievement->m_idAchievement ) != m_arrAchievementsEarned.InvalidIndex() )
			continue;
		TitleDataFieldsDescription_t const *fdBitfield = TitleDataFieldsDescriptionFindByString( pTitleDataStorage,
			CFmtStr( "%s[1]", pAchievement->m_szAchievementName ) );
		int numComponentsSet = 0;
		for ( ; numComponentsSet < pAchievement->m_numComponents; ++ numComponentsSet, ++ fdBitfield )
		{
			if ( !fdBitfield || !fdBitfield->m_szFieldName || (fdBitfield->m_eDataType != fdBitfield->DT_BITFIELD) )
			{
				DevWarning( "EvaluateAwardsStateBasedOnStats for achievement [%s] error: invalid component configuration (comp#%d)!\n", pAchievement->m_szAchievementName, numComponentsSet + 1 );
				break;
			}
#ifdef _DEBUG
			// In debug make sure bitfields names match
			if ( Q_stricmp( fdBitfield->m_szFieldName, CFmtStr( "%s[%d]", pAchievement->m_szAchievementName, numComponentsSet + 1 ) ) )
			{
				Assert( 0 );
			}
#endif
			if ( !TitleDataFieldsDescriptionGetBit( fdBitfield, this ) )
				break;
		}
		if ( numComponentsSet == pAchievement->m_numComponents )
		{
			// Achievement should be earned based on components
			if ( !kvAwards )
			{
				kvAwards = new KeyValues( "write_award" );
				autodelete_kvAwards.Assign( kvAwards );
			}
			DevMsg( "PlayerLocal(%s)::EvaluateAwardsStateBasedOnStats is awarding %s\n", GetName(), pAchievement->m_szAchievementName );
			kvAwards->SetInt( pAchievement->m_szAchievementName, 1 );
		}
	}

	//
	// Evaluate the state of all avatar awards
	//
	for ( TitleAvatarAwardsDescription_t const *pAvatarAward = g_pMatchFramework->GetMatchTitle()->DescribeTitleAvatarAwards();
		pAvatarAward && pAvatarAward->m_szAvatarAwardName; ++ pAvatarAward )
	{
		if ( TitleDataFieldsDescription_t const *fdBitfield = TitleDataFieldsDescriptionFindByString( pTitleDataStorage, pAvatarAward->m_szTitleDataBitfieldStatName ) )
		{
			if ( TitleDataFieldsDescriptionGetBit( fdBitfield, this ) )
			{
				m_arrAvatarAwardsEarned.FindAndFastRemove( pAvatarAward->m_idAvatarAward );
				m_arrAvatarAwardsEarned.AddToTail( pAvatarAward->m_idAvatarAward );
			}
		}
	}

	//
	// Award all accumulated awards
	//
	UpdateAwardsData( kvAwards );
}

void PlayerLocal::LoadGuestsTitleData()
{
#ifdef _GAMECONSOLE
	for ( int k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		int iCtrlr = XBX_GetUserId( k );
		if ( iCtrlr == m_iController )
			continue;
		if ( !XBX_GetUserIsGuest( k ) )
			continue;

		DevMsg( "User%d stats inheriting from user%d.\n", iCtrlr, m_iController );
		PlayerLocal *pGuest = ( PlayerLocal * ) g_pPlayerManager->GetLocalPlayer( iCtrlr );
		Q_memcpy( pGuest->m_bufTitleData, m_bufTitleData, sizeof( m_bufTitleData ) );
	}
#endif
}


void PlayerLocal::OnProfileTitleDataLoaded( int iErrorCode )
{
	if ( iErrorCode )
	{
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnProfileDataLoadFailed", "iController", m_iController, "error", iErrorCode ) );
	}
	else
	{
		LoadGuestsTitleData();
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnProfileDataLoaded", "iController", m_iController ) );
	}

	// Invite awaiting our title data
	if ( m_uiPlayerFlags & PLAYER_INVITE_AWAITING_TITLEDATA )
	{
		m_uiPlayerFlags &=~PLAYER_INVITE_AWAITING_TITLEDATA;

		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnInvite", "action", "join" ) );
	}
}

XUSER_SIGNIN_STATE PlayerLocal::GetAssumedSigninState()
{
#ifdef _X360
	return ( GetOnlineState() != STATE_OFFLINE ) ? ( eXUserSigninState_SignedInToLive ) : ( eXUserSigninState_SignedInLocally );
#elif !defined( NO_STEAM )
	if ( steamapicontext->SteamUser() && steamapicontext->SteamUser()->BLoggedOn() )
		return eXUserSigninState_SignedInToLive;
	else
		return eXUserSigninState_SignedInLocally;
#else // No steam.


#if defined( _PS3 )

	return eXUserSigninState_SignedInLocally;

#else

	return eXUserSigninState_NotSignedIn;

#endif


#endif
}

void PlayerLocal::SetNeedsSave()
{
	for ( int ii=0; ii<TITLE_DATA_COUNT; ++ii )
	{
		m_bSaveTitleData[ii] = true;
	}
}

CON_COMMAND_F( ms_player_dump_properties, "Prints a dump the current players property data", FCVAR_CHEAT )
{
	Msg( "[DMM] ms_player_dump_properties...\n" );
	Msg( "        Num game users: %d\n", XBX_GetNumGameUsers() );
	for ( unsigned int iUserSlot = 0; iUserSlot < XBX_GetNumGameUsers(); ++ iUserSlot )
	{
		int iCtrlr = iUserSlot;
		bool bGuest = false;
#ifdef _GAMECONSOLE
		iCtrlr = XBX_GetUserId( iUserSlot );
		bGuest = !!XBX_GetUserIsGuest( iUserSlot );
#endif
		Msg( "Slot%d ctrlr%d: %s\n", iUserSlot, iCtrlr, bGuest ? "guest" : "profile" );
		IPlayerLocal *pPlayerLocal = g_pPlayerManager->GetLocalPlayer( iCtrlr );
		if ( !pPlayerLocal )
			continue;
		Msg( "  Name = %s\n", pPlayerLocal->GetName() );
		TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
		for ( ; fields && fields->m_szFieldName; ++ fields )
		{
			switch ( fields->m_eDataType )
			{
			case TitleDataFieldsDescription_t::DT_BITFIELD:
				Msg( "BITFIELD %s = %u\n", fields->m_szFieldName, TitleDataFieldsDescriptionGetBit( fields, pPlayerLocal ) ? 1 : 0 );
				break;
			case TitleDataFieldsDescription_t::DT_uint8:
				Msg( "UINT8    %s = %u\n", fields->m_szFieldName, TitleDataFieldsDescriptionGetValue<uint8>( fields, pPlayerLocal ) );
				break;
			case TitleDataFieldsDescription_t::DT_uint16:
				Msg( "UINT16   %s = %u\n", fields->m_szFieldName, TitleDataFieldsDescriptionGetValue<uint16>( fields, pPlayerLocal ) );
				break;
			case TitleDataFieldsDescription_t::DT_uint32:
				Msg( "UINT32   %s = %u\n", fields->m_szFieldName, TitleDataFieldsDescriptionGetValue<uint32>( fields, pPlayerLocal ) );
				break;
			case TitleDataFieldsDescription_t::DT_float:
				Msg( "FLOAT    %s = %.3f\n", fields->m_szFieldName, TitleDataFieldsDescriptionGetValue<float>( fields, pPlayerLocal ) );
				break;
			case TitleDataFieldsDescription_t::DT_uint64:
				Msg( "UINT64   %s = 0x%llX\n", fields->m_szFieldName, TitleDataFieldsDescriptionGetValue<uint64>( fields, pPlayerLocal ) );
				break;
			}
		}
	}
	Msg( "        ms_player_dump_properties finished.\n" );
}

#ifdef _DEBUG
CON_COMMAND_F( ms_player_award, "Awards the current player an award", FCVAR_CHEAT )
{
	int iCtrlr = args.FindArgInt( "ctrlr", XBX_GetPrimaryUserId() );
	IPlayerLocal *pPlayer = g_pPlayerManager->GetLocalPlayer( iCtrlr );
	if ( !pPlayer )
	{
		DevWarning( "ERROR: Controller %d is not registered!\n", iCtrlr );
		return;
	}
	
	KeyValues *kvAwards = new KeyValues( "write_awards", args.FindArg( "award" ), 1 );
	KeyValues::AutoDelete autodelete( kvAwards );
	(( PlayerLocal * )pPlayer)->UpdateAwardsData( kvAwards );
}
#endif

#if !defined(NO_STEAM) && !defined(_CERT)
CON_COMMAND_F( ms_player_unaward, "UnAwards the current player an award", FCVAR_DEVELOPMENTONLY )
{
	if ( !CommandLine()->FindParm( "+ms_player_unaward" ) )
	{
		Warning( "Error: You must pass +ms_player_unaward from command line!\n" );
		return;
	}

	if ( !args.FindArg( "unaward" ) )
	{
		Warning( "Syntax: +ms_player_unaward unaward ACHIEVEMENT|everything\n" );
		return;
	}

	if ( !V_stricmp( "everything", args.FindArg( "unaward" ) ) )
	{
		steamapicontext->SteamUserStats()->ResetAllStats( false );
		steamapicontext->SteamUserStats()->ResetAllStats( true );
		while ( steamapicontext->SteamRemoteStorage()->GetFileCount() > 0 )
		{
			int nUnused;
			steamapicontext->SteamRemoteStorage()->FileDelete( steamapicontext->SteamRemoteStorage()->GetFileNameAndSize( 0, &nUnused ) );
		}
		steamapicontext->SteamUserStats()->StoreStats();
		Msg( "Everything was reset!\n" );
		if ( !IsGameConsole() )
		{
			g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "exec config_default.cfg; exec joy_preset_1.cfg; host_writeconfig;" );
		}
		return;
	}

	if ( steamapicontext->SteamUserStats()->ClearAchievement( args.FindArg( "unaward" ) ) )
	{
		steamapicontext->SteamUserStats()->StoreStats();
		Msg( "%s unawarded!\n", args.FindArg( "unaward" ) );
	}
	else
	{
		Warning( "%s failed\n", args.FindArg( "unaward" ) );
	}
}
#endif



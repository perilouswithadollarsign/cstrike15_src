//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"
#include "mm_netmsgcontroller.h"

#include "matchsystem.h"

#include "mm_title_main.h"
#include "x360_lobbyapi.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Stress testing events listeners every frame
ConVar mm_events_listeners_validation( "mm_events_listeners_validation", "0", FCVAR_DEVELOPMENTONLY );


//
// Implementation of Steam invite listener
//
uint64 g_uiLastInviteFlags = 0ull;
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
class CMatchSteamInviteListener
{
public:
	void RunFrame();
	void Register();
	STEAM_CALLBACK_MANUAL( CMatchSteamInviteListener, Steam_OnGameLobbyJoinRequested, GameLobbyJoinRequested_t, m_CallbackOnGameLobbyJoinRequested );
#ifdef _PS3
	STEAM_CALLBACK_MANUAL( CMatchSteamInviteListener, Steam_OnPSNGameBootInviteResult, PSNGameBootInviteResult_t, m_CallbackOnPSNGameBootInviteResult );
#endif

protected:
	GameLobbyJoinRequested_t m_msgPending;
}
g_MatchSteamInviteListener;

void CMatchSteamInviteListener::Register()
{
	m_CallbackOnGameLobbyJoinRequested.Register( this, &CMatchSteamInviteListener::Steam_OnGameLobbyJoinRequested );
#ifdef _PS3
	m_CallbackOnPSNGameBootInviteResult.Register( this, &CMatchSteamInviteListener::Steam_OnPSNGameBootInviteResult );
#endif
}
#else
class CMatchSteamInviteListener
{
public:
	void RunFrame() {}
	void Register() {}
}
g_MatchSteamInviteListener;
#endif



//
// Implementation
//

CMatchFramework::CMatchFramework() :
	m_pMatchSession( NULL ),
	m_bJoinTeamSession( false ),
	m_pTeamSessionSettings( NULL )
{
}

CMatchFramework::~CMatchFramework()
{
	;
}

InitReturnVal_t CMatchFramework::Init()
{
	InitReturnVal_t ret = INIT_OK;

	ret = MM_Title_Init();
	if ( ret != INIT_OK )
		return ret;

	g_MatchSteamInviteListener.Register();

	return INIT_OK;
}

void CMatchFramework::Shutdown()
{
	// Shutdown event system
	g_pMatchEventsSubscription->Shutdown();

	// Shutdown the title
	MM_Title_Shutdown();

	// Cancel any pending server updates before shutdown
	g_pServerManager->EnableServersUpdate( false );

	// Cancel any pending datacenter queries
	g_pDatacenter->EnableUpdate( false );
}

void CMatchFramework::RunFrame()
{
	// Run frame listeners validation if requested
	if ( mm_events_listeners_validation.GetBool() )
	{
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "mm_events_listeners_validation" ) );
	}

#ifdef _X360
	if ( IXOnline *pIXOnline = g_pMatchExtensions->GetIXOnline() )
		pIXOnline->RunFrame();

	MMX360_UpdateDormantOperations();

	SysSession360_UpdatePending();
#endif

	RunFrame_Invite();
	g_MatchSteamInviteListener.RunFrame();

#ifdef _X360
	// Pump rate adjustments
	MatchSession_RateAdjustmentUpdate();
#endif

	// Let the network mgr run
	g_pConnectionlessLanMgr->Update();

	// Let the matchsystem run
	g_pMatchSystem->Update();

	// Let the match session run
	if ( m_pMatchSession )
		m_pMatchSession->Update();

	// Let the match title run frame
	g_pIMatchTitle->RunFrame();

	if ( m_bJoinTeamSession )
	{
		m_bJoinTeamSession = false;
		MatchSession( m_pTeamSessionSettings );
		m_pTeamSessionSettings->deleteThis();
		m_pTeamSessionSettings = NULL;
	}
}

IMatchExtensions * CMatchFramework::GetMatchExtensions()
{
	return g_pMatchExtensions;
}

IMatchEventsSubscription * CMatchFramework::GetEventsSubscription()
{
	return g_pMatchEventsSubscription;
}

IMatchTitle * CMatchFramework::GetMatchTitle()
{
	return g_pIMatchTitle;
}

IMatchTitleGameSettingsMgr * CMatchFramework::GetMatchTitleGameSettingsMgr()
{
	return g_pIMatchTitleGameSettingsMgr;
}

IMatchNetworkMsgController * CMatchFramework::GetMatchNetworkMsgController()
{
	return g_pMatchNetMsgControllerBase;
}

IMatchSystem * CMatchFramework::GetMatchSystem()
{
	return g_pMatchSystem;
}

void CMatchFramework::ApplySettings( KeyValues* keyValues )
{
	g_pMatchExtensions->GetIServerGameDLL()->ApplyGameSettings( keyValues );
}


#ifdef _X360
static XINVITE_INFO s_InviteInfo;
#else
static uint64 s_InviteInfo;
#endif
static bool s_bInviteSessionDelayedJoin;
static int s_nInviteConfirmed;

template < int datasize >
static bool IsZeroData( void const *pvData )
{
	static char s_zerodata[ datasize ];
	return !memcmp( s_zerodata, pvData, datasize );
}

static bool ValidateInviteController( int iController )
{
#ifdef _X360
	XUSER_SIGNIN_STATE eSignInState = XUserGetSigninState( iController );
	XUSER_SIGNIN_INFO xsi = {0};
	if ( ( eSignInState != eXUserSigninState_SignedInToLive ) ||
		( ERROR_SUCCESS != XUserGetSigninInfo( iController, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) ) ||
		! ( xsi.dwInfoFlags & XUSER_INFO_FLAG_LIVE_ENABLED ) )
	{
		DevWarning( "ValidateInviteController: ctrl%d check1 failed (state=%d, flags=0x%X, xuid=%llx)!\n",
			eSignInState, xsi.dwInfoFlags, xsi.xuid );
		
		if ( KeyValues *notify = new KeyValues(
			"OnInvite", "action", "error", "error", "NotOnline" ) )
		{
			notify->SetInt( "user", iController );
			g_pMatchEventsSubscription->BroadcastEvent( notify );
		}
		
		return false;
	}

	BOOL bMultiplayer = FALSE;
	if ( ( ERROR_SUCCESS != XUserCheckPrivilege( iController, XPRIVILEGE_MULTIPLAYER_SESSIONS, &bMultiplayer ) ) ||
		( !bMultiplayer ) )
	{
		DevWarning( "ValidateInviteController: ctrl%d check2 failed (state=%d, flags=0x%X, xuid=%llx) - on multiplayer priv!\n",
			eSignInState, xsi.dwInfoFlags, xsi.xuid );

		if ( KeyValues *notify = new KeyValues(
			"OnInvite", "action", "error", "error", "NoMultiplayer" ) )
		{
			notify->SetInt( "user", iController );
			g_pMatchEventsSubscription->BroadcastEvent( notify );
		}

		return false;
	}
#endif

	return true;
}

static bool ValidateInviteControllers()
{
	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		if ( !ValidateInviteController( XBX_GetUserId( k ) ) )
			return false;
	}
	return true;
}

static bool VerifyInviteEligibility()
{
#ifdef _X360
	// Make sure that the inviter is not signed in
	for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
	{
		XUID xuid;
		if ( ERROR_SUCCESS == XUserGetXUID( k, &xuid ) &&
			xuid == s_InviteInfo.xuidInviter )
		{
			g_pMatchEventsSubscription->BroadcastEvent( new KeyValues(
				"OnInvite", "action", "error", "error", "SameConsole" ) );
			return false;
		}
	}

	// Check if the user is currently inactive
	bool bExistingUser = false;
	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetInvitedUserId() == (DWORD) XBX_GetUserId( k ) &&
			!XBX_GetUserIsGuest( k ) )
		{
			bExistingUser = true;
			break;
		}
	}

	// Check if this is the existing user that the invite is for a different session
	// than the session they are currently in (e.g. they are in a lobby and do
	// "Join Party and Game" with another user who is in the same lobby)
	char chInviteSessionInfo[ XSESSION_INFO_STRING_LENGTH ] = {0};
	MMX360_SessionInfoToString( s_InviteInfo.hostInfo, chInviteSessionInfo );
	if ( IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		bool bJoinable = ( ( IMatchSessionInternal * ) pIMatchSession )->IsAnotherSessionJoinable( chInviteSessionInfo );
		if ( !bJoinable && bExistingUser )
		{
			Warning( "VerifyInviteEligibility: declined invite due to local session!\n" );
			return false;
		}
	}

	// New user is eligible since otherwise he shouldn't be able to accept an invite
	if ( !bExistingUser || ( XBX_GetNumGameUsers() < 2 ) ||
		( g_pMMF->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_INVITE_ONLY_SINGLE_USER ) )
	{
		if ( !ValidateInviteController( XBX_GetInvitedUserId() ) )
			return false;
		else
			return true;
	}
#endif

	// Check that every user is valid
	return ValidateInviteControllers();
}

static void JoinInviteSession()
{
	s_bInviteSessionDelayedJoin = false;

#ifdef _X360
	if ( 0ull == ( uint64 const & ) s_InviteInfo.hostInfo.sessionID )
#else
	if ( !s_InviteInfo )
#endif
		return;
	
	if ( g_pMatchExtensions->GetIVEngineClient()->IsDrawingLoadingImage() )
	{
		s_bInviteSessionDelayedJoin = true;
		return;
	}

	// Invites cannot be accepted from inside an event broadcast
	// internally used events must be top-level events since they
	// operate on signed in / active users, trigger playermanager,
	// account access and other events
	// Wait until next frame in such case
	if ( g_pMatchEventsSubscription && g_pMatchEventsSubscription->IsBroacasting() )
	{
		s_bInviteSessionDelayedJoin = true;
		return;
	}

#if !defined( NO_STEAM ) && !defined( _GAMECONSOLE ) && !defined( SWDS )
	extern bool g_bSteamStatsReceived;
	if ( !g_bSteamStatsReceived && ( g_uiLastInviteFlags & MM_INVITE_FLAG_PCBOOT ) )
	{
		s_bInviteSessionDelayedJoin = true;
		return;
	}
#endif

#ifdef _X360
	DevMsg( "JoinInviteSession: sessionid = %llx, xuid = %llx\n", ( uint64 const & ) s_InviteInfo.hostInfo.sessionID, s_InviteInfo.xuidInvitee );
#else
	DevMsg( "JoinInviteSession: sessionid = %llx\n", s_InviteInfo );
#endif

	//
	// Validate the user accepting the invite
	//
#ifdef _GAMECONSOLE
	if ( XBX_GetInvitedUserId() == INVALID_USER_ID )
	{
		DevWarning( "JoinInviteSession: no invited user!\n" );
		return;
	}
#endif
#ifdef _X360
	XUSER_SIGNIN_STATE eSignInState = XUserGetSigninState( XBX_GetInvitedUserId() );
	XUSER_SIGNIN_INFO xsi = {0};
	if ( ( eSignInState != eXUserSigninState_SignedInToLive ) ||
		( ERROR_SUCCESS != XUserGetSigninInfo( XBX_GetInvitedUserId(), XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) ) ||
		! ( xsi.dwInfoFlags & XUSER_INFO_FLAG_LIVE_ENABLED ) ||
		( xsi.dwInfoFlags & XUSER_INFO_FLAG_GUEST ) ||
		!IsEqualXUID( xsi.xuid, s_InviteInfo.xuidInvitee ) )
	{
		DevWarning( "JoinInviteSession: invited user signin information validation failed (state=%d, flags=0x%X, xuid=%llx)!\n",
			eSignInState, xsi.dwInfoFlags, xsi.xuid );
		return;
	}
	BOOL bMultiplayer = FALSE;
	if ( ( ERROR_SUCCESS != XUserCheckPrivilege( XBX_GetInvitedUserId(), XPRIVILEGE_MULTIPLAYER_SESSIONS, &bMultiplayer ) ) ||
		( !bMultiplayer ) )
	{
		DevWarning( "JoinInviteSession: no multiplayer priv!\n" );
		return;
	}
#endif

	//
	// Check if the currently-involved user is accepting the invite
	//
#ifdef _GAMECONSOLE
	bool bExistingUser = false;
	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetInvitedUserId() == (DWORD) XBX_GetUserId( k ) &&
			!XBX_GetUserIsGuest( k ) )
		{
			bExistingUser = true;
			break;
		}
	}
	if ( !bExistingUser ||
		( ( XBX_GetNumGameUsers() > 1 ) && ( g_pMMF->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_INVITE_ONLY_SINGLE_USER ) ) )
	{
		// Another controller is accepting the invite or guest status
		// has changed.
		// then we need to reset all our XBX core state:

		DevMsg( "JoinInviteSession: activating inactive controller%d\n", XBX_GetInvitedUserId() );

		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnProfilesWriteOpportunity", "reason", "deactivation" ) );

		XBX_ClearUserIdSlots();

		XBX_SetPrimaryUserId( XBX_GetInvitedUserId() );
		XBX_SetPrimaryUserIsGuest( 0 );

		XBX_SetUserId( 0, XBX_GetInvitedUserId() );
		XBX_SetUserIsGuest( 0, 0 );

		XBX_SetNumGameUsers( 1 );
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnProfilesChanged", "numProfiles", int(1) ) );

		IPlayerLocal *pPlayer = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() );
		if ( !pPlayer )
		{
			g_pMatchEventsSubscription->BroadcastEvent( new KeyValues(
				"OnInvite", "action", "error", "error", "" ) );
			return;
		}
		( ( PlayerLocal * ) pPlayer )->SetFlag_AwaitingTitleData();

		// Since we have activated a new profile, we need to wait until title data gets loaded
		DevMsg( "JoinInviteSession: activated inactive controller%d, waiting for title data...\n", XBX_GetInvitedUserId() );
		return;
	}
#endif

	// Validate storage device
	s_nInviteConfirmed = -1;
	if ( KeyValues *notify = new KeyValues( "OnInvite" ) )
	{
#ifdef _X360
		char chSessionInfo[ XSESSION_INFO_STRING_LENGTH ] = {0};
		MMX360_SessionInfoToString( s_InviteInfo.hostInfo, chSessionInfo );
		notify->SetInt( "user", XBX_GetInvitedUserId() );
		notify->SetString( "sessioninfo", chSessionInfo );
#else
		notify->SetUint64( "sessionid", s_InviteInfo );
#endif
		notify->SetString( "action", "storage" );
		notify->SetPtr( "confirmed", &s_nInviteConfirmed );

		g_pMatchEventsSubscription->BroadcastEvent( notify );

		// If handlers decided they need to confirm storage devices, etc.
		if ( s_nInviteConfirmed != -1 )
		{
			DevMsg( "JoinInviteSession: waiting for storage device selection...\n" );
			return;
		}
	}

	// Verify eligibility
	DevMsg( "JoinInviteSession: verifying eligibility...\n" );
	if ( !VerifyInviteEligibility() )
		return;
	DevMsg( "JoinInviteSession: connecting...\n" );

	//
	// Argument validation
	//
#ifdef _GAMECONSOLE
	Assert( XBX_GetInvitedUserId() >= 0 );
	Assert( XBX_GetInvitedUserId() < XUSER_MAX_COUNT );
	Assert( XBX_GetSlotByUserId( XBX_GetInvitedUserId() ) < ( int ) XBX_GetNumGameUsers() );
	Assert( XBX_GetNumGameUsers() < MAX_SPLITSCREEN_CLIENTS );
#endif

	// Requesting to join the stored off session
	KeyValues *pSettings = KeyValues::FromString(
		"settings",
		" system { "
			" network LIVE "
		" } "
		" options { "
			" action joinsession "
		" } "
		);
	
#ifdef _X360
	pSettings->SetUint64( "options/sessionid", ( const uint64 & ) s_InviteInfo.hostInfo.sessionID );

	if ( !IsZeroData< sizeof( s_InviteInfo.hostInfo.keyExchangeKey ) >( &s_InviteInfo.hostInfo.keyExchangeKey ) )
	{
		// Missing sessioninfo will cause the session info to be discovered during session
		// creation time
		char chSessionInfoBuffer[ XSESSION_INFO_STRING_LENGTH ] = {0};
		MMX360_SessionInfoToString( s_InviteInfo.hostInfo, chSessionInfoBuffer );
		pSettings->SetString( "options/sessioninfo", chSessionInfoBuffer );
	}
#else
	pSettings->SetUint64( "options/sessionid", s_InviteInfo );
#endif
	
	KeyValues::AutoDelete autodelete( pSettings );
	Q_memset( &s_InviteInfo, 0, sizeof( s_InviteInfo ) );

	g_pMatchFramework->MatchSession( pSettings );
}

static void OnInviteAccepted()
{
	// Verify eligibility
	DevMsg( "OnInviteAccepted: verifying eligibility...\n" );
	if ( !VerifyInviteEligibility() )
		return;
	DevMsg( "OnInviteAccepted: confirming...\n" );

	// Make sure the user confirms the invite
	s_nInviteConfirmed = -1;
	if ( KeyValues *notify = new KeyValues( "OnInvite" ) )
	{
#ifdef _X360
		char chSessionInfo[ XSESSION_INFO_STRING_LENGTH ] = {0};
		MMX360_SessionInfoToString( s_InviteInfo.hostInfo, chSessionInfo );
		notify->SetInt( "user", XBX_GetInvitedUserId() );
		notify->SetString( "sessioninfo", chSessionInfo );
#else
		notify->SetUint64( "sessionid", s_InviteInfo );
#endif
		notify->SetString( "action", "accepted" );
		notify->SetPtr( "confirmed", &s_nInviteConfirmed );

		g_pMatchEventsSubscription->BroadcastEvent( notify );

		// If handlers decided they need to confirm destructive actions or
		// select storage devices, etc.
		if ( s_nInviteConfirmed != -1 )
		{
			DevMsg( "OnInviteAccepted: waiting for confirmation...\n" );
			return;
		}
	}
	DevMsg( "OnInviteAccepted: accepting...\n" );

	// Otherwise, launch depending on our current MOD
	// if ( !Q_stricmp( GetCurrentMod(), "left4dead2" ) ) <-- for multi-game package
	{
		// Kick off our join
		JoinInviteSession();
	}
	// 	else	<-- for multi-game package supporting cross-game invites
	// 	{
	// 		// Save off our session ID for later retrieval
	// 		// NOTE: We may need to actually save off the inviter's XID and search for them later on if we took too long or the
	// 		//		 session they were a part of went away
	// 
	// 		XBX_SetInviteSessionId( inviteInfo.hostInfo.sessionID );
	// 
	// 		// Quit via the menu path "QuitNoConfirm"
	// 		EngineVGui()->SystemNotification( SYSTEMNOTIFY_INVITE_SHUTDOWN, NULL );
	// 	}
}

void CMatchFramework::RunFrame_Invite()
{
	if ( s_bInviteSessionDelayedJoin )
		JoinInviteSession();
}

void CMatchFramework::AcceptInvite( int iController )
{
#ifdef _X360
	s_bInviteSessionDelayedJoin = false;

	// Grab our invite info
	DWORD dwError = g_pMatchExtensions->GetIXOnline()->XInviteGetAcceptedInfo( iController, &s_InviteInfo );
	if ( dwError != ERROR_SUCCESS )
	{
		ZeroMemory( &s_InviteInfo, sizeof( s_InviteInfo ) );
		return;
	}

	// We only care if we're asked to join this title's session
	if ( s_InviteInfo.dwTitleID != GetMatchTitle()->GetTitleID() )
	{
		ZeroMemory( &s_InviteInfo, sizeof( s_InviteInfo ) );
		return;
	}

	// We just mark the invited user and let the matchmaking handle profile changes
	XBX_SetInvitedUserId( iController );

	// Invite accepted logic after globals have been setup
	OnInviteAccepted();
#endif
}

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
void CMatchSteamInviteListener::Steam_OnGameLobbyJoinRequested( GameLobbyJoinRequested_t *pJoinInvite )
{
#ifdef _PS3
	if ( pJoinInvite->m_steamIDFriend.ConvertToUint64() != ~0ull )
	{
		g_uiLastInviteFlags = ( pJoinInvite->m_steamIDFriend.BConsoleUserAccount() ? MM_INVITE_FLAG_CONSOLE : 0 );
	}
#endif

#if !defined( _GAMECONSOLE )
	g_uiLastInviteFlags = ( pJoinInvite->m_steamIDFriend.ConvertToUint64() == ~0ull ) ? MM_INVITE_FLAG_PCBOOT : 0;
#endif

	m_msgPending = GameLobbyJoinRequested_t();
	s_bInviteSessionDelayedJoin = false;
	s_InviteInfo = pJoinInvite->m_steamIDLobby.ConvertToUint64();
	if ( !s_InviteInfo )
		return;
	
	#ifdef _GAMECONSOLE
	// We just mark the invited user and let the matchmaking handle profile changes
	XBX_SetInvitedUserId( XBX_GetPrimaryUserId() );
	#endif

	// Whether we have to make invite go pending
	char chBuffer[2] = {};
	if ( g_pMatchExtensions->GetIVEngineClient()->IsDrawingLoadingImage() ||
		( g_pMatchEventsSubscription && g_pMatchEventsSubscription->IsBroacasting() ) ||
		( g_pMatchExtensions->GetIBaseClientDLL()->GetStatus( chBuffer, 2 ), ( chBuffer[0] != '+' ) ) )
	{
		m_msgPending = *pJoinInvite;
		return;
	}

	// Invite accepted logic after globals have been setup
	OnInviteAccepted();
}

#ifdef _PS3
void CMatchSteamInviteListener::Steam_OnPSNGameBootInviteResult( PSNGameBootInviteResult_t *pParam )
{
	if ( pParam->m_bGameBootInviteExists && pParam->m_steamIDLobby.IsValid() )
	{
		g_uiLastInviteFlags = MM_INVITE_FLAG_CONSOLE;
	}
}
#endif

void CMatchSteamInviteListener::RunFrame()
{
	if ( m_msgPending.m_steamIDLobby.IsValid() )
	{
		GameLobbyJoinRequested_t msgRequest = m_msgPending;
		Steam_OnGameLobbyJoinRequested( &msgRequest );
	}
}
#endif


IMatchSession *CMatchFramework::GetMatchSession()
{
	return m_pMatchSession;
}

void CMatchFramework::CreateSession( KeyValues *pSettings )
{
	DevMsg( "CreateSession: \n");
	KeyValuesDumpAsDevMsg( pSettings );

#ifndef SWDS
	if ( !pSettings )
		return;

	IMatchSessionInternal *pMatchSessionNew = NULL;

	//
	// Analyze the type of session requested to create
	//

	char const *szNetwork = pSettings->GetString( "system/network", "offline" );

#ifdef _X360
	if ( !Q_stricmp( "LIVE", szNetwork ) && !ValidateInviteControllers() )
		return;
#endif

	// Recompute XUIDs for the session type that we are creating
	g_pPlayerManager->RecomputePlayerXUIDs( szNetwork );

	//
	// Process create session request
	//
	if ( !Q_stricmp( "offline", szNetwork ) )
	{
		CMatchSessionOfflineCustom *pSession = new CMatchSessionOfflineCustom( pSettings );
		pMatchSessionNew = pSession;
	}
	else
	{
		CMatchSessionOnlineHost *pSession = new CMatchSessionOnlineHost( pSettings );
		pMatchSessionNew = pSession;
	}

	if ( pMatchSessionNew )
	{
		CloseSession();
		m_pMatchSession = pMatchSessionNew;
	}
#endif
}

void CMatchFramework::MatchSession( KeyValues *pSettings )
{
#ifndef SWDS
	if ( !pSettings )
		return;

	DevMsg( "MatchSession: \n");
	KeyValuesDumpAsDevMsg( pSettings );

	IMatchSessionInternal *pMatchSessionNew = NULL;

	//
	// Analyze what kind of client-side matchmaking
	// needs to happen.
	//

	char const *szNetwork = pSettings->GetString( "system/network", "LIVE" );
	char const *szAction = pSettings->GetString( "options/action", "" );

	// Recompute XUIDs for the session type that we are creating
	g_pPlayerManager->RecomputePlayerXUIDs( szNetwork );

	//
	// Process match session request
	//
	if ( !Q_stricmp( "joinsession", szAction ) )
	{
#ifdef _X360
		// For LIVE sessions we need to be eligible
		if ( !Q_stricmp( "LIVE", szNetwork ) && !ValidateInviteControllers() )
			return;
#endif

		// We have an explicit session to join
		CMatchSessionOnlineClient *pSession = new CMatchSessionOnlineClient( pSettings );
		pMatchSessionNew = pSession;
	}
	else if ( !Q_stricmp( "joininvitesession", szAction ) )
	{
#ifdef _X360
		ZeroMemory( &s_InviteInfo, sizeof( s_InviteInfo ) );
		XUSER_SIGNIN_INFO xsi;
		if ( ERROR_SUCCESS == XUserGetSigninInfo( XBX_GetInvitedUserId(), XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) )
			s_InviteInfo.xuidInvitee = xsi.xuid;

		uint64 uiSessionID = pSettings->GetUint64( "options/sessionid", 0ull );
		s_InviteInfo.hostInfo.sessionID = ( XNKID & ) uiSessionID;

		OnInviteAccepted();
#endif
	}
	else // "quickmatch" or "custommatch"
	{
#ifdef _X360
		// For LIVE sessions we need to be eligible
		if ( !Q_stricmp( "LIVE", szNetwork ) && !ValidateInviteControllers() )
			return;
#endif

		CMatchSessionOnlineSearch *pSession = new CMatchSessionOnlineSearch( pSettings );
		pMatchSessionNew = pSession;
	}

	if ( pMatchSessionNew )
	{
		CloseSession();
		m_pMatchSession = pMatchSessionNew;
	}
#endif
}


void CMatchFramework::CloseSession()
{
	// Destroy the session
	if ( m_pMatchSession )
	{
		IMatchSessionInternal *pMatchSession = m_pMatchSession;
		m_pMatchSession = NULL;
		pMatchSession->Destroy();

		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "closed" ) );
	}
}

bool CMatchFramework::IsOnlineGame( void )
{
	IMatchSession *pMatchSession = GetMatchSession();

	if ( pMatchSession ) 
	{
		KeyValues* kv = pMatchSession->GetSessionSettings();
		if ( kv )
		{
			char const *szMode = kv->GetString( "system/network", NULL );
			if ( szMode && !V_stricmp( "LIVE", szMode ) )
			{
				return true;
			}
		}
	}
	return false;
}

void CMatchFramework::UpdateTeamProperties( KeyValues *pTeamProperties )
{
	IMatchSession *pMatchSession = GetMatchSession();
	IMatchTitleGameSettingsMgr *pMatchTitleGameSettingsMgr = GetMatchTitleGameSettingsMgr();

	if ( pMatchSession && pMatchTitleGameSettingsMgr )
	{
		pMatchSession->UpdateTeamProperties( pTeamProperties );
		KeyValues *pCurrentSettings = pMatchSession->GetSessionSettings();
		pMatchTitleGameSettingsMgr->UpdateTeamProperties( pCurrentSettings, pTeamProperties );
	}
}

void CMatchFramework::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "mmF->CloseSession", szEvent ) )
	{
		CloseSession();
		return;
	}
	else if ( !Q_stricmp( "OnInvite", szEvent ) )
	{
		if ( !Q_stricmp( "join", pEvent->GetString( "action" ) ) )
		{
			s_bInviteSessionDelayedJoin = true;
		}
		else if ( !Q_stricmp( "deny", pEvent->GetString( "action" ) ) )
		{
			Q_memset( &s_InviteInfo, 0, sizeof( s_InviteInfo ) );
			s_bInviteSessionDelayedJoin = false;
		}
		return;
	}
	else if ( !Q_stricmp( "OnSteamOverlayCall::LobbyJoin", szEvent ) )
	{
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
		GameLobbyJoinRequested_t msg;
		msg.m_steamIDLobby.SetFromUint64( pEvent->GetUint64( "sessionid" ) );
		msg.m_steamIDFriend.SetFromUint64( ~0ull );
		g_MatchSteamInviteListener.Steam_OnGameLobbyJoinRequested( &msg );
#endif
		return;
	}
	else if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{	
		KeyValues *pUpdate = pEvent->FindKey( "update" );
		
		if ( pUpdate )
		{
			const char *pAction = pUpdate->GetString( "options/action", "" );
			if ( !Q_stricmp( "joinsession", pAction ) )
			{
				KeyValues *pTeamMembers = pUpdate->FindKey( "teamMembers" );

				if ( pTeamMembers )
				{
					// Received console team match settings from host
					// Find what team we are on

					int numPlayers = pTeamMembers->GetInt( "numPlayers" );
					int playerTeam = -1;

					int activeUer = XBX_GetPrimaryUserId();
					IPlayerLocal *player = g_pPlayerManager->GetLocalPlayer( activeUer );
					uint64 localPlayerId = player->GetXUID();

					for ( int i = 0; i < numPlayers; i++ )
					{
						KeyValues *pTeamPlayer = pTeamMembers->FindKey( CFmtStr( "player%d", i ) );
						uint64 playerId = pTeamPlayer->GetUint64( "xuid" );

						if ( playerId == localPlayerId )
						{
							int team = pTeamPlayer->GetInt( "team" );
							DevMsg( "Adding player %llu to team %d\n", playerId, team );
							playerTeam = team;
							break;
						}
					}
				
					m_pTeamSessionSettings = pUpdate->MakeCopy();
					m_pTeamSessionSettings->SetName( "settings ");

					// Delete the "teamMembers" key
					m_pTeamSessionSettings->RemoveSubKey( m_pTeamSessionSettings->FindKey( "teamMembers" ) );

					// Add "conteam" value
					m_pTeamSessionSettings->SetInt( "conteam", playerTeam );

					// Add the "sessionHostDataUnpacked" key
					KeyValues *pSessionHostDataSrc = pUpdate->FindKey( "sessionHostDataUnpacked" );
					if ( pSessionHostDataSrc )
					{
						KeyValues *pSessionHostDataDst = m_pTeamSessionSettings->CreateNewKey();
						pSessionHostDataDst->SetName( "sessionHostDataUnpacked" );
					
						pSessionHostDataSrc->CopySubkeys( pSessionHostDataDst );
					}

					m_bJoinTeamSession = true;
				}
			}
		}
	}

	//
	// Delegate to the managers
	//
	if ( g_pPlayerManager )
		g_pPlayerManager->OnEvent( pEvent );
	if ( g_pServerManager )
		g_pServerManager->OnEvent( pEvent );
	if ( g_pDatacenter )
		g_pDatacenter->OnEvent( pEvent );
	if ( g_pDlcManager )
		g_pDlcManager->OnEvent( pEvent );

	//
	// Delegate to the title
	//
	if ( g_pIMatchTitleEventsSink )
		g_pIMatchTitleEventsSink->OnEvent( pEvent );

	//
	// Delegate to the session
	//
	if ( m_pMatchSession )
		m_pMatchSession->OnEvent( pEvent );
}

void CMatchFramework::SetCurrentMatchSession( IMatchSessionInternal *pNewMatchSession )
{
	m_pMatchSession = pNewMatchSession;
}

uint64 CMatchFramework::GetLastInviteFlags()
{
	return g_uiLastInviteFlags;
}



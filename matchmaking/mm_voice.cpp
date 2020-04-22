//===== Copyright � 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_voice.h"

#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( _GAMECONSOLE ) && !defined( _CERT )
ConVar mm_voice_fulldebug( "mm_voice_fulldebug", "0", FCVAR_DEVELOPMENTONLY );
#define MMVOICEMSG(...) if ( mm_voice_fulldebug.GetInt() > 0 ) { Msg( "[MMVOICE] " __VA_ARGS__ ); }
#define MMVOICEMSG2(...) if ( mm_voice_fulldebug.GetInt() > 1 ) { Msg( "[MMVOICE] " __VA_ARGS__ ); }
#else
#define MMVOICEMSG(...) ((void)0)
#define MMVOICEMSG2(...) ((void)0)
#endif

#if !defined(NO_STEAM) && !defined( SWDS )
static inline bool FriendRelationshipMute( int iRelationship )
{
	switch ( iRelationship )
	{
	case k_EFriendRelationshipBlocked:
	case k_EFriendFlagIgnored:
	case k_EFriendFlagIgnoredFriend:
		return true;
	default:
		return false;
	}
}
#endif

//
// Construction/destruction
//

CMatchVoice::CMatchVoice()
{
	;
}

CMatchVoice::~CMatchVoice()
{
	;
}

static CMatchVoice g_MatchVoice;
CMatchVoice *g_pMatchVoice = &g_MatchVoice;

//
// Implementation
//

// Whether remote player talking can be visualized / audible
bool CMatchVoice::CanPlaybackTalker( XUID xuidTalker )
{
	if ( IsMachineMutingLocalTalkers( xuidTalker ) )
	{
		MMVOICEMSG2( "CanPlaybackTalker(0x%llX)=false(IsMachineMutingLocalTalkers)\n", xuidTalker );
		return false;
	}

	if ( IsMachineMuted( xuidTalker ) )
	{
		MMVOICEMSG2( "CanPlaybackTalker(0x%llX)=false(IsMachineMuted)\n", xuidTalker );
		return false;
	}

	return true;
}

// Whether we are explicitly muting a remote player
bool CMatchVoice::IsTalkerMuted( XUID xuidTalker )
{
#if defined( _PS3 ) && !defined( NO_STEAM )
	if ( steamapicontext->SteamFriends()->GetUserRestrictions() )
	{
		MMVOICEMSG( "IsTalkerMuted(0x%llX)=true(GetUserRestrictions)\n", xuidTalker );
		return true;
	}
#endif

#if !defined(NO_STEAM) && !defined( SWDS )
	if ( FriendRelationshipMute( steamapicontext->SteamFriends()->GetFriendRelationship( xuidTalker ) ) )
	{
		MMVOICEMSG( "IsTalkerMuted(0x%llX)=true(GetFriendRelationship=0x%X)\n", xuidTalker, steamapicontext->SteamFriends()->GetFriendRelationship( xuidTalker ) );
		return true;
	}

	if ( m_arrMutedTalkers.Find( xuidTalker ) != m_arrMutedTalkers.InvalidIndex() )
	{
		MMVOICEMSG( "IsTalkerMuted(0x%llX)=true(locallist)\n", xuidTalker );
		return true;
	}
#endif

#if defined( _GAMECONSOLE ) && !defined( _CERT )
	XUID xuidOriginal = xuidTalker; xuidOriginal;
#endif
	xuidTalker = RemapTalkerXuid( xuidTalker );

#if !defined(NO_STEAM) && !defined( SWDS )
	if ( FriendRelationshipMute( steamapicontext->SteamFriends()->GetFriendRelationship( xuidTalker ) ) )
	{
		MMVOICEMSG( "IsTalkerMuted(0x%llX/0x%llX)=true(GetFriendRelationship=0x%X)\n", xuidTalker, xuidOriginal, steamapicontext->SteamFriends()->GetFriendRelationship( xuidTalker ) );
		return true;
	}
#endif

#ifdef _X360
	if ( MMX360_GetUserCtrlrIndex( xuidTalker ) >= 0 )
		// local players are never considered muted locally
		return false;

	for ( DWORD dwCtrlr = 0; dwCtrlr < XUSER_MAX_COUNT; ++ dwCtrlr )
	{
		int iSlot = ( XBX_GetNumGameUsers() > 0 ) ? XBX_GetSlotByUserId( dwCtrlr ) : -1;

		if ( iSlot >= 0 && iSlot < ( int ) XBX_GetNumGameUsers() &&
			 XBX_GetUserIsGuest( iSlot ) )
			continue;

		BOOL mutedInGuide = false;
		if ( ERROR_SUCCESS == g_pMatchExtensions->GetIXOnline()->XUserMuteListQuery( dwCtrlr, xuidTalker, &mutedInGuide ) &&
			 mutedInGuide )
		{
			return true;
		}
	}
#endif

	if ( m_arrMutedTalkers.Find( xuidTalker ) != m_arrMutedTalkers.InvalidIndex() )
	{
		MMVOICEMSG( "IsTalkerMuted(0x%llX/0x%llX)=true(locallist)\n", xuidTalker, xuidOriginal );
		return true;
	}

	return false;
}

// Whether we are muting any player on the player's machine
bool CMatchVoice::IsMachineMuted( XUID xuidPlayer )
{
#ifdef _X360
	if ( MMX360_GetUserCtrlrIndex( xuidPlayer ) >= 0 )
		// local players are never considered muted locally
		return false;

	// Find the session and the talker within session members
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return IsTalkerMutedWithPrivileges( -1, xuidPlayer );

	KeyValues *pSettings = pMatchSession->GetSessionSettings();

	KeyValues *pMachine = NULL;
	KeyValues *pTalker = SessionMembersFindPlayer( pSettings, xuidPlayer, &pMachine );
	if ( !pTalker || !pMachine )
		return IsTalkerMutedWithPrivileges( -1, xuidPlayer );

	// Walk all users from that machine
	int numPlayers = pMachine->GetInt( "numPlayers" );
	for ( int k = 0; k < numPlayers; ++ k )
	{
		KeyValues *pOtherPlayer = pMachine->FindKey( CFmtStr( "player%d", k ) );
		if ( !pOtherPlayer )
			continue;

		char const *szOtherName = pOtherPlayer->GetString( "name" );
		if ( strchr( szOtherName, '(' ) )
			continue;

		XUID xuidOther = pOtherPlayer->GetUint64( "xuid" );
		if ( IsTalkerMutedWithPrivileges( -1, xuidOther ) )
			return true;
	}
	return false;
#else
	return IsTalkerMuted( xuidPlayer );
#endif
}

#ifdef _PS3
struct TalkerXuidRemap_t
{
	XUID xuidSteamId;
	XUID xuidPsnId;
};
#define TALKER_REMAP_CACHE_SIZE 4
static CUtlVector< TalkerXuidRemap_t > g_arrTalkerRemapCache( 0, TALKER_REMAP_CACHE_SIZE );
#endif
// X360: Remap XUID of a player to a valid LIVE-enabled XUID
// PS3: Remap SteamID of a player to a PSN ID
XUID CMatchVoice::RemapTalkerXuid( XUID xuidTalker )
{
	if ( !IsGameConsole() )
		return xuidTalker;

#ifdef _PS3
	for ( int k = 0; k < g_arrTalkerRemapCache.Count(); ++ k )
		if ( g_arrTalkerRemapCache[k].xuidSteamId == xuidTalker )
			return g_arrTalkerRemapCache[k].xuidPsnId;
#endif

	// Find the session and the talker within session members
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return xuidTalker;

	KeyValues *pSettings = pMatchSession->GetSessionSettings();

	KeyValues *pMachine = NULL;
	KeyValues *pTalker = SessionMembersFindPlayer( pSettings, xuidTalker, &pMachine );
	if ( !pTalker || !pMachine )
		return xuidTalker;

#ifdef _PS3
	XUID xuidPsnId = pMachine->GetUint64( "psnid" );
	if ( !xuidPsnId )
		return xuidTalker;
	if ( g_arrTalkerRemapCache.Count() >= TALKER_REMAP_CACHE_SIZE )
		g_arrTalkerRemapCache.SetCountNonDestructively( TALKER_REMAP_CACHE_SIZE - 1 );
	TalkerXuidRemap_t txr = { xuidTalker, xuidPsnId };
	g_arrTalkerRemapCache.AddToHead( txr );
	return xuidPsnId;
#endif

	// Check this user name if he is a guest
	char const *szTalkerName = pTalker->GetString( "name" );
	char const *pchr = strchr( szTalkerName, '(' );
	if ( !pchr )
		return xuidTalker;	// user is not a guest

	// Find another user from the same machine
	int numPlayers = pMachine->GetInt( "numPlayers" );
	for ( int k = 0; k < numPlayers; ++ k )
	{
		KeyValues *pOtherPlayer = pMachine->FindKey( CFmtStr( "player%d", k ) );
		if ( !pOtherPlayer )
			continue;

		char const *szOtherName = pOtherPlayer->GetString( "name" );
		if ( strchr( szOtherName, '(' ) )
			continue;

		XUID xuidOther = pOtherPlayer->GetUint64( "xuid" );
		if ( xuidOther )
			return xuidOther;
	}

	// No remapping
	return xuidTalker;
}

// Check player-player voice privileges for machine blocking purposes
bool CMatchVoice::IsTalkerMutedWithPrivileges( int dwCtrlr, XUID xuidTalker )
{
#ifdef _X360
	if ( -1 == dwCtrlr )	// all controllers should be considered
	{
		for ( dwCtrlr = 0; dwCtrlr < XUSER_MAX_COUNT; ++ dwCtrlr )
		{
			if ( IsTalkerMutedWithPrivileges( dwCtrlr, xuidTalker ) )
				return true;
		}
		return false;
	}

	// Analyze this particular local controller against the given talker
	int iSlot = ( XBX_GetNumGameUsers() > 0 ) ? XBX_GetSlotByUserId( dwCtrlr ) : -1;

	if ( iSlot >= 0 && iSlot < ( int ) XBX_GetNumGameUsers() &&
		 XBX_GetUserIsGuest( iSlot ) )
		 // Guest has no say
		 return false;

	XUSER_SIGNIN_INFO xsi;
	if ( ERROR_SUCCESS == XUserGetSigninInfo( dwCtrlr, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) )
	{
		if ( xsi.dwInfoFlags & XUSER_INFO_FLAG_GUEST )
			// LIVE guests have no say
			return false;
	}

	BOOL mutedInGuide = false;
	if ( ERROR_SUCCESS == g_pMatchExtensions->GetIXOnline()->XUserMuteListQuery( dwCtrlr, xuidTalker, &mutedInGuide ) &&
		 mutedInGuide )
	{
		return true;
	}

	// Check permissions to see if this player has friends-only or no communication set
	// Don't check permissions against other local players
	// Check for open privileges
	BOOL bHasPrivileges;
	DWORD dwResult = XUserCheckPrivilege( dwCtrlr, XPRIVILEGE_COMMUNICATIONS, &bHasPrivileges );
	if ( dwResult == ERROR_SUCCESS )
	{
		if ( !bHasPrivileges )
		{
			// Second call checks for friends-only
			XUserCheckPrivilege( dwCtrlr, XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY, &bHasPrivileges );

			if ( bHasPrivileges )
			{
				// Privileges are set to friends-only. See if the remote player is on our friends list.
				BOOL bIsFriend;
				dwResult = XUserAreUsersFriends( dwCtrlr, &xuidTalker, 1, &bIsFriend, NULL );
				if ( dwResult != ERROR_SUCCESS || !bIsFriend )
				{
					return true;
				}
			}
			else
			{
				// Privilege is nobody, mute them all
				return true;
			}
		}
	}
#endif

	if ( m_arrMutedTalkers.Find( xuidTalker ) != m_arrMutedTalkers.InvalidIndex() )
	{
		MMVOICEMSG( "IsTalkerMutedWithPrivileges(%d/0x%llX)=true(locallist)\n", dwCtrlr, xuidTalker );
		return true;
	}

	return false;
}

// Check if player machine is muting any of local players
bool CMatchVoice::IsMachineMutingLocalTalkers( XUID xuidPlayer )
{
	// Find the session and the talker within session members
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return false;

	KeyValues *pSettings = pMatchSession->GetSessionSettings();

	KeyValues *pMachine = NULL;
	SessionMembersFindPlayer( pSettings, xuidPlayer, &pMachine );
	if ( !pMachine )
		return false;

	// Find the local player record in the session
	XUID xuidLocalId = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() )->GetXUID();
	KeyValues *pLocalMachine = NULL;
	SessionMembersFindPlayer( pSettings, xuidLocalId, &pLocalMachine );
	if ( !pLocalMachine || pLocalMachine == pMachine )
		return false;
	int numLocalPlayers = pLocalMachine->GetInt( "numPlayers" );

	// Check the mutelist on the machine
	if ( KeyValues *pMutelist = pMachine->FindKey( "Mutelist" ) )
	{
		for ( KeyValues *val = pMutelist->GetFirstValue(); val; val = val->GetNextValue() )
		{
			XUID xuidMuted = val->GetUint64();
			if ( !xuidMuted )
				continue;

			for ( int iLocal = 0; iLocal < numLocalPlayers; ++ iLocal )
			{
				XUID xuidLocal = pLocalMachine->GetUint64( CFmtStr( "player%d/xuid", iLocal ) );
				if ( xuidMuted == xuidLocal )
				{
					MMVOICEMSG2( "IsMachineMutingLocalTalkers(0x%llX/0x%llX)=true(mutelist)\n", xuidPlayer, xuidLocal );
					return true;
				}
			}
		}
	}

	return false;
}

// Whether voice recording mode is currently active
bool CMatchVoice::IsVoiceRecording()
{
#if !defined(_X360) && !defined(NO_STEAM) && !defined( SWDS )

#ifdef _PS3
	EVoiceResult res = steamapicontext->SteamUser()->GetAvailableVoice( NULL, NULL, 11025 );
#else
	EVoiceResult res = steamapicontext->SteamUser()->GetAvailableVoice( NULL, NULL, 0 );
#endif

	switch ( res )
	{
	case k_EVoiceResultOK:
	case k_EVoiceResultNoData:
		return true;
	default:
		return false;
	}
#endif

	return false;
}

// Enable or disable voice recording
void CMatchVoice::SetVoiceRecording( bool bRecordingEnabled )
{
#if !defined(_X360) && !defined(NO_STEAM) && !defined( SWDS )
	if ( bRecordingEnabled )
		steamapicontext->SteamUser()->StartVoiceRecording();
	else
		steamapicontext->SteamUser()->StopVoiceRecording();
#endif
}

// Enable or disable voice mute for a given talker
void CMatchVoice::MuteTalker( XUID xuidTalker, bool bMute )
{
#if !defined(_X360) && !defined(NO_STEAM) && !defined( SWDS )
	if ( !xuidTalker )
	{
		if ( !bMute )
			m_arrMutedTalkers.Purge();
	}
	else
	{
		m_arrMutedTalkers.FindAndFastRemove( xuidTalker );
		if ( bMute )
		{
			m_arrMutedTalkers.AddToTail( xuidTalker );
		}
	}
	
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnSysMuteListChanged" ) );
#endif
}

CON_COMMAND( voice_reset_mutelist, "Reset all mute information for all players who were ever muted." )
{
	g_pMatchVoice->MuteTalker( 0ull, false );
	Msg( "Mute list cleared.\n" );
}

#if !defined( _X360 ) && !defined( NO_STEAM )
CON_COMMAND( voice_mute, "Mute a specific Steam user" )
{
	if ( args.ArgC() != 2 )
	{
		goto usage;
	}
	else
	{
		int iUserId = Q_atoi( args.Arg( 1 ) );
		player_info_t pi;
		if ( !g_pMatchExtensions->GetIVEngineClient()->GetPlayerInfo( iUserId, &pi ) || !pi.xuid )
		{
			Msg( "Player# is invalid or refers to a bot, please use \"voice_show_mute\" command.\n" );
			goto usage;
		}

		g_pMatchVoice->MuteTalker( pi.xuid, true );
		if ( !g_pMatchExtensions->GetIVEngineClient()->GetDemoPlaybackParameters() )
		{
			Msg( "%s is now muted.\n", pi.name );
		}
		return;
	}

usage:
	Msg( "Example usage: voice_mute player#   -   where player# is a number that you can find with \"voice_show_mute\" command.\n" );
}

CON_COMMAND( voice_unmute, "Unmute a specific Steam user, or `all` to unmute all connected players." )
{
	if ( args.ArgC() != 2 )
	{
		goto usage;
	}
	else
	{
		if ( !Q_stricmp( "all", args.Arg(1) ) )
		{
			XUID xuidLocal = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() )->GetXUID();
			int maxClients = g_pMatchExtensions->GetIVEngineClient()->GetMaxClients();
			for ( int i = 1; i <= maxClients; ++ i )
			{
				// Get the player info from the engine	
				player_info_t pi;
				if ( !g_pMatchExtensions->GetIVEngineClient()->GetPlayerInfo( i, &pi ) )
					continue;
				if ( !pi.xuid )
					continue;
				if ( pi.xuid == xuidLocal )
					continue;

				g_pMatchVoice->MuteTalker( pi.xuid, false );
			}
			Msg( "All connected players have been unmuted.\n" );
			return;
		}

		int iUserId = Q_atoi( args.Arg( 1 ) );
		player_info_t pi;
		if ( !g_pMatchExtensions->GetIVEngineClient()->GetPlayerInfo( iUserId, &pi ) || !pi.xuid )
		{
			Msg( "Player# is invalid or refers to a bot, please use \"voice_show_mute\" command.\n" );
			goto usage;
		}

		g_pMatchVoice->MuteTalker( pi.xuid, false );
		if ( !g_pMatchExtensions->GetIVEngineClient()->GetDemoPlaybackParameters() )
		{
			Msg( "%s is now unmuted.\n", pi.name );
		}
		return;
	}

usage:
	Msg( "Example usage: voice_unmute {player#|all}   -   where player# is a number that you can find with \"voice_show_mute\" command, or all to unmute all connected players.\n" );
}

CON_COMMAND( voice_show_mute, "Show whether current players are muted." )
{
	if ( g_pMatchExtensions->GetIVEngineClient()->GetDemoPlaybackParameters() )
		return;

	bool bPrinted = false;
	XUID xuidLocal = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() )->GetXUID();
	int maxClients = g_pMatchExtensions->GetIVEngineClient()->GetMaxClients();
	for ( int i = 1; i <= maxClients; ++ i )
	{
		// Get the player info from the engine	
		player_info_t pi;
		if ( !g_pMatchExtensions->GetIVEngineClient()->GetPlayerInfo( i, &pi ) )
			continue;
		if ( !pi.xuid )
			continue;
		if ( pi.xuid == xuidLocal )
			continue;

		if ( !bPrinted )
		{
			bPrinted = true;
			Msg( "Player#     Player Name\n" );
			Msg( "-------     ----------------\n" );
		}

		Msg( " % 2d %s %s\n", i, g_pMatchVoice->IsTalkerMuted( pi.xuid ) ? "(muted)" : "       ", pi.name );
	}
	
	if ( bPrinted )
	{
		Msg( "-------     ----------------\n" );
	}
	else
	{
		Msg( "No players currently connected who can be muted.\n" );
	}
}
#endif

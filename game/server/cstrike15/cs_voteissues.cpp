//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============
//
// Purpose:  CS-specific things to vote on
//
//=============================================================================

#include "cbase.h"
#include "cs_voteissues.h"
#include "cs_player.h"

#include "vote_controller.h"
#include "fmtstr.h"
#include "eiface.h"
#include "cs_gamerules.h"
#include "inetchannelinfo.h"
#include "cs_gamestats.h"
#include "gametypes.h"

//[tj]removing this to get voting to compile
//#include "cs_gcmessages.h"

#ifdef CLIENT_DLL
#include "gc_clientsystem.h"
#endif // CLIENT_DLL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar mp_maxrounds;
extern ConVar mp_winlimit;

//-----------------------------------------------------------------------------
// Purpose: Base CS Issue
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Restart Round Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_restart_game_allowed( "sv_vote_issue_restart_game_allowed", "0", FCVAR_RELEASE, "Can people hold votes to restart the game?" );
ConVar sv_arms_race_vote_to_restart_disallowed_after( "sv_arms_race_vote_to_restart_disallowed_after", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Arms Race gun level after which vote to restart is disallowed" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRestartGameIssue::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_restartgame 1;" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRestartGameIssue::IsEnabled( void )
{
	if ( sv_vote_issue_restart_game_allowed.GetBool() )
	{
		// Vote to restart is allowed
		if ( sv_arms_race_vote_to_restart_disallowed_after.GetInt() > 0 )
		{
			// Need to test if a player has surpassed the maximum weapon progression
			if ( sv_arms_race_vote_to_restart_disallowed_after.GetInt() > CSGameRules()->GetMaxGunGameProgressiveWeaponIndex() )
			{
				// No players have surpassed the maximum weapon progression, so enable vote to restart
				return true;
			}
			else
			{
				// A player has surpassed the maximum weapon progression, so disable vote to restart
				return false;
			}
		}

		// No maximum weapon progression value is defined, so enable vote to restart
		return true;
	}

	// Vote to restart is not enabled
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CRestartGameIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CRestartGameIssue::GetDisplayString( void )
{
	return "#SFUI_vote_restart_game";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CRestartGameIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_restart_game";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRestartGameIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_restart_game_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}


//-----------------------------------------------------------------------------
// Purpose: Kick Player Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_kick_allowed( "sv_vote_issue_kick_allowed", "1", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Can people hold votes to kick players from the server?" );
ConVar sv_vote_kick_ban_duration( "sv_vote_kick_ban_duration", "15", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "How long should a kick vote ban someone from the server? (in minutes)" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKickIssue::ExecuteCommand( void )
{
	CCSPlayer *subject = NULL;

	//[tj] Not applicable without TF-specific GC code
	//uint32 unReason = kVoteKickBanPlayerReason_Other;	
	uint32 unReason;

	ExtractDataFromDetails( m_szDetailsString, &subject, &unReason );

	if( subject )
	{
		// Check the cached value of player crashed state
		if ( ( sv_vote_kick_ban_duration.GetInt() > 0 ) && !m_bPlayerCrashed )
		{
			CSGameRules()->SendKickBanToGC( subject, k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_VotedOff );
			// don't roll the kick command into this, it will fail on a lan, where kickid will go through
			engine->ServerCommand( CFmtStr( "banid %d %d;", sv_vote_kick_ban_duration.GetInt(), subject->GetUserID() ) );
		}

		engine->ServerCommand( CFmtStr( "kickid_ex %d %d You have been voted off;", subject->GetUserID(), CSGameRules()->IsPlayingOffline() ? 0 : 1 ) );
	}
	else if ( !m_bPlayerCrashed && m_steamIDtoBan.IsValid() && ( sv_vote_kick_ban_duration.GetInt() > 0 ) )
	{
		// Player has already disconnected, but let's still ban him!
		CSGameRules()->SendKickBanToGCforAccountId( m_steamIDtoBan.GetAccountID(), k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_VotedOff );

		// Also enlist this user's SteamID in the banlist
		engine->ServerCommand( CFmtStr( "banid %d STEAM64BITID_%llu;", sv_vote_kick_ban_duration.GetInt(), m_steamIDtoBan.ConvertToUint64() ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CKickIssue::IsEnabled( void )
{
	return sv_vote_issue_kick_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CKickIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	CCSPlayer *pSubject = NULL;
	ExtractDataFromDetails( pszDetails, &pSubject );
	if ( !pSubject || pSubject->IsBot() )
	{
		nFailCode = VOTE_FAILED_PLAYERNOTFOUND;
		return false;
	}

	if ( pSubject->IsReplay() || pSubject->IsHLTV() )
	{
		nFailCode = VOTE_FAILED_PLAYERNOTFOUND;
		return false;
	}
	
	CBaseEntity *pVoteCaller = UTIL_EntityByIndex( iEntIndex );
	if ( pVoteCaller )
	{
		CCSPlayer *pPlayer = ToCSPlayer( pVoteCaller );
		bool bCanKickVote = false;

		if ( pSubject && pPlayer )
		{
			int voterTeam = pPlayer->GetTeamNumber();
			int nSubjectTeam = pSubject->GetTeamNumber();

			bCanKickVote = ( voterTeam == TEAM_TERRORIST || voterTeam ==  TEAM_CT ) && ( voterTeam == nSubjectTeam || nSubjectTeam == TEAM_SPECTATOR );			
		} 	
		
		if ( pSubject )
		{
			bool bDeny = false;
			if ( !engine->IsDedicatedServer() )
			{
				if ( pSubject->entindex() == 1 )
				{
					bDeny = true;
				}
			}

			// This should only be set if we've successfully authenticated via rcon
			if ( pSubject->IsAutoKickDisabled() )
			{
				bDeny = true;
			}

			if ( bDeny )
			{
				nFailCode = VOTE_FAILED_CANNOT_KICK_ADMIN;
				return false;
			}
		}

		if ( !bCanKickVote )
		{
			// Can't initiate a vote to kick a player on the other team
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKickIssue::OnVoteFailed( void )
{
	CBaseCSIssue::OnVoteFailed();

	CCSPlayer *subject = NULL;
	uint32 unReason;// = kVoteKickBanPlayerReason_Other;
	ExtractDataFromDetails( m_szDetailsString, &subject, &unReason );

	//[tj]removing this to get voting to compile
	//NotifyGC( subject, false, unReason );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
 void CKickIssue::OnVoteStarted( void )
 {
	 CCSPlayer *pSubject = NULL;
	 ExtractDataFromDetails(	m_szDetailsString, &pSubject );

	 // Auto vote 'No' for the person being kicked unless they are idle
	 // NOTE: Subtle. There's a problem with IsAwayFromKeyboard where if a player
	 // has idled and is taken over by a bot, IsAwayFromKeyboard will return false
	 // because the camera controller that takes over when idling will spoof 
	 // input messages, making it impossible to know if the player is moving his
	 // joystick or not. Being on TEAM_SPECTATOR, however, means you're idling,
	 // so we don't want to autovote NO if they are on team spectator
	 if ( m_pVoteController && pSubject && ( pSubject->GetTeamNumber( ) != TEAM_SPECTATOR ) && !pSubject->IsBot( ) )
	 {
		 m_pVoteController->TryCastVote( pSubject->entindex( ), "Option2" );
	 }

	 // Also when the vote starts, figure out if the player should not be banned
	 // if the player is crashed/hung. Need to perform the check here instead of
	 // inside Execute to prevent cheaters quitting before the vote finishes and
	 // not getting banned.
	 m_bPlayerCrashed = false;
	 if ( pSubject )
	 {
		 INetChannelInfo *pNetChanInfo = engine->GetPlayerNetInfo( pSubject->entindex() );
		 if ( !pNetChanInfo || pNetChanInfo->IsTimingOut() )
		 {
			 // don't ban the player
			 DevMsg( "Will not ban kicked player: net channel was idle for %.2f sec.\n", pNetChanInfo ? pNetChanInfo->GetTimeSinceLastReceived() : 0.0f );
			 m_bPlayerCrashed = true;
		 }

		 pSubject->GetSteamID( &m_steamIDtoBan );
	 }
 }

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CKickIssue::GetDisplayString( void )
{
	uint32 unReason = kVoteKickBanPlayerReason_Other;
	ExtractDataFromDetails( m_szDetailsString, NULL, &unReason );
	switch ( unReason )
	{
	case kVoteKickBanPlayerReason_Other:	return "#SFUI_vote_kick_player_other";
	case kVoteKickBanPlayerReason_Cheating:	return "#SFUI_vote_kick_player_cheating";
	case kVoteKickBanPlayerReason_Idle:		return "#SFUI_vote_kick_player_idle";
	case kVoteKickBanPlayerReason_Scamming:	return "#SFUI_vote_kick_player_scamming";
	}
	return "#SFUI_vote_kick_player_other";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CKickIssue::GetOtherTeamDisplayString( void )
{
	return "#SFUI_otherteam_vote_kick_player";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CKickIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_kick_player";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CKickIssue::GetDetailsString( void )
{
	int iUserID = 0;
	const char *pReason = strstr( m_szDetailsString, " " );
	if ( pReason != NULL )
	{
		pReason += 1;
		CUtlString userID;
		userID.SetDirect( m_szDetailsString, pReason - m_szDetailsString );
		iUserID = atoi( userID );
	}
	else
	{
		iUserID = atoi( m_szDetailsString );
	}	

	CBasePlayer *pPlayer = UTIL_PlayerByUserId( iUserID );
	if ( pPlayer )
	{
		return pPlayer->GetPlayerName();
	}
	else
	{
		return "unnamed";
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKickIssue::ExtractDataFromDetails( const char *pszDetails, CCSPlayer **pSubject, uint32 *pReason )
{
	int iUserID = 0;
	const char *pReasonString = strstr( pszDetails, " " );
	if ( pReasonString != NULL )
	{
		pReasonString += 1;
		CUtlString userID;
		userID.SetDirect( pszDetails, pReasonString - pszDetails );
		iUserID = atoi( userID );
		if ( pReason )
		{
			//[tj] Not applicable without TF-specific GC code
			//*pReason = GetKickBanPlayerReason( pReasonString );
		}
	}
	else
	{
		iUserID = atoi( pszDetails );
	}

	if ( iUserID >= 0 )
	{
		if( pSubject )
		{
			*pSubject = ToCSPlayer( UTIL_PlayerByUserId( iUserID ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKickIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_kick_allowed.GetBool() )
		return;

	char szBuffer[MAX_COMMAND_LENGTH];
	Q_snprintf( szBuffer, MAX_COMMAND_LENGTH, "callvote %s <userID>\n", GetTypeString() );
	ClientPrint( pForWhom, HUD_PRINTCONSOLE, szBuffer );
}



//-----------------------------------------------------------------------------
// Purpose: LoadBackup Player Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_loadbackup_allowed( "sv_vote_issue_loadbackup_allowed", "1", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Can people hold votes to load match from backup?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadBackupIssue::ExecuteCommand( void )
{
	CSGameRules()->LoadRoundDataInformation( m_szDetailsString );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CLoadBackupIssue::IsEnabled( void )
{
	return sv_vote_issue_loadbackup_allowed.GetBool( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CLoadBackupIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadBackupIssue::OnVoteFailed( void )
{
	CBaseCSIssue::OnVoteFailed( );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CLoadBackupIssue::GetDisplayString( void )
{
	return "#SFUI_vote_loadbackup";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CLoadBackupIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_loadbackup";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CLoadBackupIssue::GetDetailsString( void )
{

	// create human readable name
	if ( !FStrEq( m_szDetailsString, m_szPrevDetailsString ) )
	{
		V_strcpy_safe( m_szNiceName, m_szDetailsString );

		KeyValues *kvSaveFile = new KeyValues( "" );
		KeyValues::AutoDelete autodelete_kvSaveFile( kvSaveFile );
		autodelete_kvSaveFile->UsesEscapeSequences( true );

		if ( kvSaveFile->LoadFromFile( filesystem, m_szDetailsString ) )
		{
			V_sprintf_safe( m_szNiceName, 
				"Score %d:%d",
				kvSaveFile->GetInt( "FirstHalfScore/team1" ) + kvSaveFile->GetInt( "SecondHalfScore/team1" ) + kvSaveFile->GetInt( "OvertimeScore/team1" ),
				kvSaveFile->GetInt( "FirstHalfScore/team2" ) + kvSaveFile->GetInt( "SecondHalfScore/team2" ) + kvSaveFile->GetInt( "OvertimeScore/team2" ) );
		}

		V_strcpy_safe( m_szPrevDetailsString, m_szDetailsString );
	}

	return m_szNiceName;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadBackupIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_loadbackup_allowed.GetBool( ) )
		return;

	char szBuffer[ MAX_COMMAND_LENGTH ];
	Q_snprintf( szBuffer, MAX_COMMAND_LENGTH, "callvote %s <backup filename>\n", GetTypeString( ) );
	ClientPrint( pForWhom, HUD_PRINTCONSOLE, szBuffer );
}


//-----------------------------------------------------------------------------
// Purpose: Changelevel
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_changelevel_allowed( "sv_vote_issue_changelevel_allowed", "1", 0, "Can people hold votes to change levels?" );
ConVar sv_vote_to_changelevel_before_match_point( "sv_vote_to_changelevel_before_match_point", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Restricts vote to change level to rounds prior to match point (default 0, vote is never disallowed)" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChangeLevelIssue::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "changelevel %s;", m_szDetailsString ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CChangeLevelIssue::CanTeamCallVote( int iTeam ) const
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CChangeLevelIssue::IsEnabled( void )
{
	if ( sv_vote_issue_changelevel_allowed.GetBool() )
	{			 		
		return ( sv_vote_to_changelevel_before_match_point.GetInt() > 0 &&
			 CSGameRules() && 
			 ( CSGameRules()->IsMatchPoint() || CSGameRules()->IsIntermission() ) ) == false;
	}
	// Change Level vote disabled
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CChangeLevelIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( !Q_strcmp( pszDetails, "" ) )
	{
		nFailCode = VOTE_FAILED_MAP_NAME_REQUIRED;
		return false;
	}
	else
	{
		if ( !g_pGameTypes->IsWorkshopMapGroup( gpGlobals->mapGroupName.ToCStr() ) && !engine->IsMapValid( pszDetails ) )
		{
			nFailCode = VOTE_FAILED_MAP_NOT_FOUND;
			return false;
		}
	}
	
	bool mapIsInGroup = false;
	if ( gpGlobals )
	{	
		const char* groupName = gpGlobals->mapGroupName.ToCStr();
		const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( groupName );
		if ( mapsInGroup )
		{
			int numMaps = mapsInGroup->Count();

			for( int i = 0 ; i < numMaps ; ++i )
			{
				const char* internalMapName = ( *mapsInGroup )[i];
				if ( !V_strcmp( pszDetails, internalMapName ) )
				{
					mapIsInGroup = true;
					break;
				}
			}
		}
	}

	return mapIsInGroup;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CChangeLevelIssue::GetDisplayString( void )
{
	return "#SFUI_vote_changelevel";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CChangeLevelIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_changelevel";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CChangeLevelIssue::GetDetailsString( void )
{
	return m_szDetailsString;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChangeLevelIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_changelevel_allowed.GetBool() )
		return;

	char szBuffer[MAX_COMMAND_LENGTH];
	Q_snprintf( szBuffer, MAX_COMMAND_LENGTH, "callvote %s <mapname>\n", GetTypeString() );
	ClientPrint( pForWhom, HUD_PRINTCONSOLE, szBuffer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CChangeLevelIssue::IsYesNoVote( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Nextlevel
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_nextlevel_allowed( "sv_vote_issue_nextlevel_allowed", "1", 1, "Can people hold votes to set the next level?" );
ConVar sv_vote_issue_nextlevel_choicesmode( "sv_vote_issue_nextlevel_choicesmode", "1", 0, "Present players with a list of lowest playtime maps to choose from?" );
ConVar sv_vote_issue_nextlevel_allowextend( "sv_vote_issue_nextlevel_allowextend", "1", 0, "Allow players to extend the current map?" );
ConVar sv_vote_issue_nextlevel_prevent_change( "sv_vote_issue_nextlevel_prevent_change", "1", 1, "Not allowed to vote for a nextlevel if one has already been set." );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNextLevelIssue::ExecuteCommand( void )
{
	if ( Q_strcmp( m_szDetailsString, "Extend current Map" ) == 0 )
	{
		// Players want to extend the current map, so extend any existing limits
		if ( mp_timelimit.GetInt() > 0 )
		{
			engine->ServerCommand( CFmtStr( "mp_timelimit %d;", mp_timelimit.GetInt() + 20 ) );
		}
		
		if ( mp_maxrounds.GetInt() > 0 )
		{
			engine->ServerCommand( CFmtStr( "mp_maxrounds %d;", mp_maxrounds.GetInt() + 2 ) );
		}
		
		if ( mp_winlimit.GetInt() > 0 )
		{
			engine->ServerCommand( CFmtStr( "mp_winlimit %d;", mp_winlimit.GetInt() + 2 ) );
		}
	}
	else
	{
		engine->ServerCommand( CFmtStr( "nextlevel %s;", m_szDetailsString ) );

		if ( gpGlobals )
		{	
			const char* groupName = gpGlobals->mapGroupName.ToCStr();
			const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( groupName );
			if ( mapsInGroup )
			{
				int numMaps = mapsInGroup->Count();

				for( int i = 0 ; i < numMaps ; ++i )
				{
					const char* internalMapName = ( *mapsInGroup )[i];
					if ( !V_strcmp( m_szDetailsString, internalMapName ) )
					{
						CSGameRules()->SetNextMapInMapGroup(i);
						break;
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNextLevelIssue::CanTeamCallVote( int iTeam ) const
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNextLevelIssue::IsEnabled( void )
{
	return sv_vote_issue_nextlevel_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNextLevelIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	// CSGameRules created vote
	if ( sv_vote_issue_nextlevel_choicesmode.GetBool() && iEntIndex == 99 )
	{
		// Invokes a UI down stream
		if ( Q_strcmp( pszDetails, "" ) == 0 )
		{
			return true;
		}

		return false;
	}
	
	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( Q_strcmp( pszDetails, "" ) == 0 )
	{
		nFailCode = VOTE_FAILED_MAP_NAME_REQUIRED;
		return false;
	}
	else
	{
		if ( !g_pGameTypes->IsWorkshopMapGroup( gpGlobals->mapGroupName.ToCStr() ) && !engine->IsMapValid( pszDetails ) )
		{
			nFailCode = VOTE_FAILED_MAP_NOT_FOUND;
			return false;
		}
	}

	if ( sv_vote_issue_nextlevel_prevent_change.GetBool() )
	{
		if ( nextlevel.GetString() && *nextlevel.GetString() && 
			( g_pGameTypes->IsWorkshopMapGroup( gpGlobals->mapGroupName.ToCStr() ) || engine->IsMapValid( nextlevel.GetString() ) ) )
		{
			nFailCode = VOTE_FAILED_NEXTLEVEL_SET;
			return false;
		}
	}
	
	bool mapIsInGroup = false;
	if ( gpGlobals )
	{	
		const char* groupName = gpGlobals->mapGroupName.ToCStr();
		const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( groupName );
		if ( mapsInGroup )
		{
			int numMaps = mapsInGroup->Count();

			for( int i = 0 ; i < numMaps ; ++i )
			{
				const char* internalMapName = ( *mapsInGroup )[i];
				if ( !V_strcmp( pszDetails, internalMapName ) )
				{
					mapIsInGroup = true;
					break;
				}
			}
		}
	}

	return mapIsInGroup;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNextLevelIssue::GetDisplayString( void )
{
	// If we don't have a map passed in already...
	if ( Q_strcmp( m_szDetailsString, "" ) == 0 )
	{
		if ( sv_vote_issue_nextlevel_choicesmode.GetBool() )
		{
			return "#SFUI_vote_nextlevel_choices";
		}
	}

	return "#SFUI_vote_nextlevel";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNextLevelIssue::GetVotePassedString( void )
{
	if ( Q_strcmp( m_szDetailsString, STRING( gpGlobals->mapname ) ) == 0 )
	{
		return "#SFUI_vote_passed_nextlevel_extend";
	}
	return "#SFUI_vote_passed_nextlevel";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNextLevelIssue::GetDetailsString( void )
{
	return m_szDetailsString;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNextLevelIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_nextlevel_allowed.GetBool() )
		return;

	if ( !sv_vote_issue_nextlevel_choicesmode.GetBool() )
	{
		char szBuffer[MAX_COMMAND_LENGTH];
		Q_snprintf( szBuffer, MAX_COMMAND_LENGTH, "callvote %s <mapname>\n", GetTypeString() );
		ClientPrint( pForWhom, HUD_PRINTCONSOLE, szBuffer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNextLevelIssue::IsYesNoVote( void )
{
	// If we don't have a map name already, this will trigger a list of choices
	if ( Q_strcmp( m_szDetailsString, "" ) == 0 )
	{
		if ( sv_vote_issue_nextlevel_choicesmode.GetBool() )
			return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNextLevelIssue::GetNumberVoteOptions( void )
{
	// If we don't have a map name already, this will trigger a list of choices
	if ( Q_strcmp( m_szDetailsString, "" ) == 0 )
	{
		if ( sv_vote_issue_nextlevel_choicesmode.GetBool() )
			return MAX_VOTE_OPTIONS;
	}

	// Vote on a specific map - Yes, No
	return 2;
}

//-----------------------------------------------------------------------------
// Purpose: Scramble Teams Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_scramble_teams_allowed( "sv_vote_issue_scramble_teams_allowed", "1", 0, "Can people hold votes to scramble the teams?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CScrambleTeams::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_scrambleteams 2;" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CScrambleTeams::IsEnabled( void )
{
	return sv_vote_issue_scramble_teams_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CScrambleTeams::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( CSGameRules() && CSGameRules()->GetScrambleTeamsOnRestart() )
	{
		nFailCode = VOTE_FAILED_SCRAMBLE_IN_PROGRESS;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CScrambleTeams::GetDisplayString( void )
{
	return "#SFUI_vote_scramble_teams";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CScrambleTeams::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_scramble_teams";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CScrambleTeams::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_scramble_teams_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}


//-----------------------------------------------------------------------------
// Purpose: Pause Match
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_pause_match_allowed( "sv_vote_issue_pause_match_allowed", "1", 0, "Can people hold votes to pause/unpause the match?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPauseMatchIssue::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_pause_match;" ) );

	CBaseEntity *pVoteCaller = UTIL_EntityByIndex( m_pVoteController->GetCallingEntity( ) );
	if ( !pVoteCaller )
		return;

	CCSPlayer *pPlayer = ToCSPlayer( pVoteCaller );
	if ( !pPlayer )
		return;

	CFmtStr fmtEntName;
	if ( pPlayer )
		fmtEntName.AppendFormat( "#ENTNAME[%d]%s", pPlayer->entindex(), pPlayer->GetPlayerName() );
	UTIL_ClientPrintAll( HUD_PRINTTALK, "#SFUI_vote_passed_pause_match_chat", fmtEntName.Access() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPauseMatchIssue::IsEnabled( void )
{
	return CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && sv_vote_issue_pause_match_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPauseMatchIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( !CSGameRules() )
		return false;

	if ( CSGameRules()->IsMatchWaitingForResume() )
	{
		nFailCode = VOTE_FAILED_MATCH_PAUSED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CPauseMatchIssue::GetDisplayString( void )
{
	return "#SFUI_vote_pause_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CPauseMatchIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_pause_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPauseMatchIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !sv_vote_issue_pause_match_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}




//-----------------------------------------------------------------------------
// Purpose: UnPause Match
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUnpauseMatchIssue::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_unpause_match;" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUnpauseMatchIssue::IsEnabled( void )
{
	return CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && sv_vote_issue_pause_match_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUnpauseMatchIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if (!CSGameRules())
		return false;

	if ( !CSGameRules()->IsMatchWaitingForResume() )
	{

		nFailCode = VOTE_FAILED_MATCH_NOT_PAUSED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CUnpauseMatchIssue::GetDisplayString( void )
{
	return "#SFUI_vote_unpause_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CUnpauseMatchIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_unpause_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUnpauseMatchIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_pause_match_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}




//-----------------------------------------------------------------------------
// Purpose: TimeOut
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_timeout_allowed( "sv_vote_issue_timeout_allowed", "1", 0, "Can people hold votes to time out?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CStartTimeOutIssue::ExecuteCommand( void )
{
	CBaseEntity *pVoteHolder = UTIL_EntityByIndex( m_pVoteController->GetCallingEntity( ) );
	if ( !pVoteHolder )
		return;

	if ( pVoteHolder->GetTeamNumber() == TEAM_CT )
	{
		engine->ServerCommand( CFmtStr( "timeout_ct_start;" ) );
	}
	else if ( pVoteHolder->GetTeamNumber() == TEAM_TERRORIST )
	{
		engine->ServerCommand( CFmtStr( "timeout_terrorist_start;" ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CStartTimeOutIssue::IsEnabled( void )
{
	return CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && sv_vote_issue_timeout_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CStartTimeOutIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( CSGameRules() && CSGameRules()->IsTimeOutActive() )
	{
		nFailCode = VOTE_FAILED_TIMEOUT_ACTIVE;
		return false;
	}

	if ( CSGameRules() && CSGameRules()->IsMatchWaitingForResume() )
	{
		nFailCode = VOTE_FAILED_MATCH_PAUSED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CStartTimeOutIssue::CanTeamCallVote(int iTeam ) const
{
	if ( !CSGameRules() )
		return false;

	if ( CSGameRules()->IsTimeOutActive() )
		return false;

	if ( iTeam == TEAM_CT )
	{
		return ( CSGameRules()->GetCTTimeOuts() > 0 );
	}
	else if ( iTeam == TEAM_TERRORIST )
	{
		return ( CSGameRules()->GetTerroristTimeOuts() > 0 );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CStartTimeOutIssue::GetDisplayString( void )
{
	return "#SFUI_vote_start_timeout";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CStartTimeOutIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_timeout";
}

vote_create_failed_t CStartTimeOutIssue::MakeVoteFailErrorCodeForClients( vote_create_failed_t eDefaultFailCode )
{
	switch ( eDefaultFailCode )
	{
	case VOTE_FAILED_TEAM_CANT_CALL:
	{
		if ( CSGameRules( ) && CSGameRules( )->IsTimeOutActive( ) )
			return VOTE_FAILED_TIMEOUT_ACTIVE;
		else if ( CSGameRules() && CSGameRules()->IsMatchWaitingForResume() )
			return VOTE_FAILED_MATCH_PAUSED;
		else
			return VOTE_FAILED_TIMEOUT_EXHAUSTED;
	}

	default:
		return eDefaultFailCode;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CStartTimeOutIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_pause_match_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_matchready_allowed( "sv_vote_issue_matchready_allowed", "1", 0, "Can people hold votes to ready/unready the match?" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CReadyForMatchIssue::ExecuteCommand( void )
{
	CSGameRules()->SetWarmupPeriodStartTime( gpGlobals->curtime );

	engine->ServerCommand( CFmtStr( "mp_warmup_pausetimer 0;" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CReadyForMatchIssue::IsEnabled( void )
{
	return CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && sv_vote_issue_matchready_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CReadyForMatchIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( CSGameRules() && !CSGameRules()->IsWarmupPeriod() )
	{
		nFailCode = VOTE_FAILED_NOT_IN_WARMUP;
		return false;
	}

#if !defined ( _DEBUG )
	if ( UTIL_HumansInGame( true, true ) != 10 )
	{
		nFailCode = VOTE_FAILED_NOT_10_PLAYERS;
		return false;
	}
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CReadyForMatchIssue::GetDisplayString( void )
{
	return "#SFUI_vote_ready_for_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CReadyForMatchIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_ready_for_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CReadyForMatchIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_matchready_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}




//-----------------------------------------------------------------------------
// Purpose: Someone wants to abort the ready process
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNotReadyForMatchIssue::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_warmup_pausetimer 1;" ) );

	CBaseEntity *pVoteCaller = UTIL_EntityByIndex( m_pVoteController->GetCallingEntity( ) );
	if ( !pVoteCaller )
		return;

	CCSPlayer *pPlayer = ToCSPlayer( pVoteCaller );
	if ( !pPlayer )
		return;

	CFmtStr fmtEntName;
	if ( pPlayer )
		fmtEntName.AppendFormat( "#ENTNAME[%d]%s", pPlayer->entindex(), pPlayer->GetPlayerName() );
	UTIL_ClientPrintAll( HUD_PRINTTALK, "#SFUI_vote_passed_not_ready_for_match_chat", fmtEntName.Access() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNotReadyForMatchIssue::IsEnabled( void )
{
	return CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && sv_vote_issue_matchready_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNotReadyForMatchIssue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( CSGameRules() && !CSGameRules()->IsWarmupPeriod() )
	{
		nFailCode = VOTE_FAILED_NOT_IN_WARMUP;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNotReadyForMatchIssue::GetDisplayString( void )
{
	return "#SFUI_vote_not_ready_for_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNotReadyForMatchIssue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_not_ready_for_match";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNotReadyForMatchIssue::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_matchready_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}





//-----------------------------------------------------------------------------
// Purpose: Swap Teams Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_swap_teams_allowed( "sv_vote_issue_swap_teams_allowed", "1", 0, "Can people hold votes to swap the teams?" );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSwapTeams::ExecuteCommand( void )
{
	engine->ServerCommand( CFmtStr( "mp_swapteams;" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSwapTeams::IsEnabled( void )
{
	return sv_vote_issue_swap_teams_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSwapTeams::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	if ( CSGameRules() && CSGameRules()->GetSwapTeamsOnRestart() )
	{
		nFailCode = VOTE_FAILED_SWAP_IN_PROGRESS;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CSwapTeams::GetDisplayString( void )
{
	return "#SFUI_vote_swap_teams";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CSwapTeams::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_swap_teams";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSwapTeams::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_swap_teams_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}



//-----------------------------------------------------------------------------
// Purpose: Surrender Issue
//-----------------------------------------------------------------------------
ConVar sv_vote_issue_surrender_allowed( "sv_vote_issue_surrrender_allowed", "1", 0, "Can people hold votes to surrender?" );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSurrender::ExecuteCommand( void )
{
	CBaseEntity *pVoteHolder = UTIL_EntityByIndex( m_pVoteController->GetCallingEntity( ) );
	if ( !pVoteHolder )
		return;

	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
	{
		if ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE || CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME )
		{
			// if the vote succeeds at the round end, just cancel it because we don't handle this in the gamerules round logic
			CBasePlayer *pVoteCaller = dynamic_cast< CBasePlayer* >( pVoteHolder );
			if ( pVoteCaller )
				m_pVoteController->SendVoteFailedMessage( VOTE_FAILED_CANT_ROUND_END, pVoteCaller, 0 );
		}
		else if ( ( CSGameRules()->m_eQueuedMatchmakingRematchState == CSGameRules()->k_EQueuedMatchmakingRematchState_MatchInProgress ) &&
			( CSGameRules()->GetGamePhase() != GAMEPHASE_MATCH_ENDED ) )
		{
			if ( pVoteHolder->GetTeamNumber() == TEAM_TERRORIST )
			{
				CSGameRules()->GetMatch()->AddCTWins( 1 );
				CSGameRules()->m_eQueuedMatchmakingRematchState = CSGameRules()->k_EQueuedMatchmakingRematchState_VoteToRematch_T_Surrender;
				CSGameRules()->TerminateRound( 0, Terrorists_Surrender );
			}
			if ( pVoteHolder->GetTeamNumber() == TEAM_CT )
			{
				CSGameRules()->GetMatch()->AddTerroristWins( 1 );
				CSGameRules()->m_eQueuedMatchmakingRematchState = CSGameRules()->k_EQueuedMatchmakingRematchState_VoteToRematch_CT_Surrender;
				CSGameRules()->TerminateRound( 0, CTs_Surrender );
			}
		}
	}
	else
	{
		if ( pVoteHolder->GetTeamNumber() == TEAM_TERRORIST )
		{
			CSGameRules()->TerminateRound( 0, Terrorists_Surrender );
		}
		if ( pVoteHolder->GetTeamNumber() == TEAM_CT )
		{
			CSGameRules()->TerminateRound( 0, CTs_Surrender );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSurrender::IsEnabled( void )
{
	return sv_vote_issue_surrender_allowed.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSurrender::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
	{
		if ( CSGameRules()->IsPlayingCooperativeGametype() )
		{
			nFailCode = VOTE_FAILED_ISSUE_DISABLED;
			return false;
		}

		if ( CSGameRules()->m_eQueuedMatchmakingRematchState != CSGameRules()->k_EQueuedMatchmakingRematchState_MatchInProgress )
		{
			nFailCode = VOTE_FAILED_ISSUE_DISABLED;
			return false;
		}

		if ( CSGameRules()->GetGamePhase() == GAMEPHASE_MATCH_ENDED || CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME ||
			 CSGameRules()->IsLastRoundBeforeHalfTime() )
		{
			nFailCode = VOTE_FAILED_ISSUE_DISABLED;
			return false;
		}

		if ( CSGameRules()->GetRoundsPlayed() < 1 )
		{	// Cannot surrender unless at least one round was played
			nFailCode = VOTE_FAILED_WAITINGFORPLAYERS;
			return false;
		}
	}

	if ( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	return true;
}

bool CSurrender::CanTeamCallVote( int iTeam ) const
{
	if ( !CSGameRules() || !CSGameRules()->IsQueuedMatchmaking() )
		return false;

	bool bTeamsArePlayingSwitchedSides = CSGameRules()->AreTeamsPlayingSwitchedSides();
	FOR_EACH_MAP( CSGameRules()->m_mapQueuedMatchmakingPlayersData, idxQPD )
	{
		CCSGameRules::CQMMPlayerData_t const &qpd = * CSGameRules()->m_mapQueuedMatchmakingPlayersData.Element( idxQPD );
		if ( qpd.m_bAbandonAllowsSurrender )
		{
			bool bSecondTeam = ( qpd.m_iDraftIndex >= 5 );
			int iTeamOfThisQPlayer = ( bSecondTeam == bTeamsArePlayingSwitchedSides ) ? TEAM_CT : TEAM_TERRORIST;
			if ( iTeamOfThisQPlayer == iTeam )
				return true;
		}
	}

#if 0
	return true; // Allow in debug mode
#endif

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CSurrender::GetDisplayString( void )
{
	return "#SFUI_vote_surrender";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CSurrender::GetOtherTeamDisplayString( void )
{
	return "#SFUI_otherteam_vote_surrender";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CSurrender::GetVotePassedString( void )
{
	return ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() ) ?  "#SFUI_vote_passed_surrender_q" : "#SFUI_vote_passed_surrender";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSurrender::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !sv_vote_issue_surrender_allowed.GetBool() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQueuedMatchmakingRematch::ExecuteCommand( void )
{
	if ( CSGameRules() )
	{
		CSGameRules()->m_eQueuedMatchmakingRematchState = CSGameRules()->k_EQueuedMatchmakingRematchState_VoteToRematchSucceeded;
	}
}

void CQueuedMatchmakingRematch::OnVoteFailed()
{
	if ( CSGameRules() )
	{
		CSGameRules()->m_eQueuedMatchmakingRematchState = CSGameRules()->k_EQueuedMatchmakingRematchState_VoteToRematchFailed;
	}
}

void CQueuedMatchmakingRematch::OnVoteStarted()
{
	if ( CSGameRules() )
	{
		CSGameRules()->m_eQueuedMatchmakingRematchState = CSGameRules()->k_EQueuedMatchmakingRematchState_VoteToRematchInProgress;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CQueuedMatchmakingRematch::IsEnabled( void )
{
	return IsTimeForRematchVote();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CQueuedMatchmakingRematch::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsTimeForRematchVote() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CQueuedMatchmakingRematch::GetDisplayString( void )
{
	return "#SFUI_vote_rematch";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CQueuedMatchmakingRematch::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_rematch";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQueuedMatchmakingRematch::ListIssueDetails( CBasePlayer *pForWhom )
{
	if ( !IsTimeForRematchVote() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}

bool CQueuedMatchmakingRematch::IsTimeForRematchVote()
{
	return CSGameRules() && CSGameRules()->IsQueuedMatchmaking() &&
		( CSGameRules()->GetGamePhase() == GAMEPHASE_MATCH_ENDED ) &&
		( CSGameRules()->m_eQueuedMatchmakingRematchState == CSGameRules()->k_EQueuedMatchmakingRematchState_VoteStarting );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQueuedMatchmakingContinue::ExecuteCommand( void )
{
	// By design: successful vote to continue does not disrupt the game
}

void CQueuedMatchmakingContinue::OnVoteFailed()
{
	Msg( "Aborting match...\n" );
}

void CQueuedMatchmakingContinue::OnVoteStarted()
{
	CBaseCSIssue::OnVoteStarted();

	if ( CSGameRules() )
		CSGameRules()->m_bNeedToAskPlayersForContinueVote = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CQueuedMatchmakingContinue::IsEnabled( void )
{
	return ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CQueuedMatchmakingContinue::CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	if ( !CBaseCSIssue::CanCallVote( iEntIndex, pszTypeString, pszDetails, nFailCode, nTime ) )
		return false;

	if ( !IsEnabled() )
		return false;

	if ( iEntIndex != 99 ) // only game rules on the server can call the vote
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CQueuedMatchmakingContinue::GetDisplayString( void )
{
	return "#SFUI_vote_continue";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CQueuedMatchmakingContinue::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_continue";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQueuedMatchmakingContinue::ListIssueDetails( CBasePlayer *pForWhom )
{
	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}



#ifdef TF_MVM_MODE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMannVsMachineStartRound::ExecuteCommand( void )
{
	if ( CSGameRules() && CSGameRules()->InSetup() && CSGameRules()->GetActiveRoundTimer() )
	{
		// need to find the current timer and set the new countdown time to 5
		CTeamRoundTimer *pTimer = CSGameRules()->GetActiveRoundTimer();
		if ( pTimer && pTimer->ShowInHud() && ( pTimer->GetTimerState() == RT_STATE_SETUP ) && ( pTimer->GetTimeRemaining() > 6 ) )
		{
			variant_t sVariant;
			sVariant.SetInt( 6 );

			pTimer->AcceptInput( "SetTime", NULL, NULL, sVariant, 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMannVsMachineStartRound::IsEnabled( void )
{
	if ( CSGameRules() )
	{
		if ( !CSGameRules()->IsMannVsMachineMode() )
			return false;

		if ( !CSGameRules()->InSetup() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMannVsMachineStartRound::CanCallVote( int iEntIndex, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
//	if( !CBaseCSIssue::CanCallVote( iEntIndex, pszDetails, nFailCode, nTime ) )
//		return false;

	if( !IsEnabled() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CMannVsMachineStartRound::GetDisplayString( void )
{
	return "#SFUI_vote_td_start_round";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CMannVsMachineStartRound::GetVotePassedString( void )
{
	return "#SFUI_vote_passed_td_start_round";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMannVsMachineStartRound::ListIssueDetails( CBasePlayer *pForWhom )
{
	if( !IsEnabled() )
		return;

	ListStandardNoArgCommand( pForWhom, GetTypeString() );
}
#endif // TF_MVM_MODE


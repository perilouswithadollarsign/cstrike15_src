//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Base VoteController.  Handles holding and voting on issues.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "vote_controller.h"
#include "shareddefs.h"
#include "eiface.h"
#include "team.h"
#include "gameinterface.h"
#include "cs_gamerules.h"
#include "usermessages.h"

#ifdef TF_DLL
#include "tf/tf_gamerules.h"
#endif

#include "EventLog.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Datatable
IMPLEMENT_SERVERCLASS_ST( CVoteController, DT_VoteController )
	SendPropInt( SENDINFO( m_iActiveIssueIndex ) ),
	SendPropInt( SENDINFO( m_iOnlyTeamToVote ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_nVoteOptionCount ), SendPropInt( SENDINFO_ARRAY( m_nVoteOptionCount ), 8, SPROP_UNSIGNED ) ),
	SendPropInt( SENDINFO( m_nPotentialVotes ) ),
	SendPropBool( SENDINFO( m_bIsYesNoVote ) )
END_SEND_TABLE()

BEGIN_DATADESC( CVoteController )
	DEFINE_THINKFUNC( VoteControllerThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( vote_controller, CVoteController );

CVoteController *g_voteControllerGlobal = NULL;
CVoteController *g_voteControllerCT = NULL;
CVoteController *g_voteControllerT = NULL;

ConVar sv_vote_timer_duration("sv_vote_timer_duration", "15", FCVAR_RELEASE, "How long to allow voting on an issue");
ConVar sv_vote_command_delay("sv_vote_command_delay", "2", FCVAR_RELEASE, "How long after a vote passes until the action happens", false, 0, true, 4.5);
ConVar sv_allow_votes("sv_allow_votes", "1", FCVAR_RELEASE, "Allow voting?");
ConVar sv_vote_failure_timer("sv_vote_failure_timer", "300", FCVAR_RELEASE, "A vote that fails cannot be re-submitted for this long");
ConVar sv_vote_creation_timer("sv_vote_creation_timer", "120", FCVAR_RELEASE, "How often someone can individually call a vote.");
// default value of the sv_vote_quorum_ratio is 0.501 so on a 32 player server, you will still need 1 more than half, otherwise at 0.6, you would need 20 people to vote yes instead of 17
ConVar sv_vote_quorum_ratio( "sv_vote_quorum_ratio", "0.501", FCVAR_RELEASE, "The minimum ratio of players needed to vote on an issue to resolve it.", true, 0.01, true, 1.0 );
ConVar sv_vote_allow_spectators( "sv_vote_allow_spectators", "0", FCVAR_RELEASE, "Allow spectators to initiate votes?" );
ConVar sv_vote_count_spectator_votes( "sv_vote_count_spectator_votes", "0", FCVAR_RELEASE, "Allow spectators to vote on issues?" );
ConVar sv_vote_allow_in_warmup( "sv_vote_allow_in_warmup", "0", FCVAR_RELEASE, "Allow voting during warmup?" );
ConVar sv_vote_disallow_kick_on_match_point( "sv_vote_disallow_kick_on_match_point", "0", FCVAR_RELEASE, "Disallow vote kicking on the match point round." );




//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CommandListIssues( void )
{
	CBasePlayer *commandIssuer = UTIL_GetCommandClient() ;

	if ( !commandIssuer )
		return;

	// list team-specific issues
	if ( commandIssuer->GetTeamVoteController() )
	{
		commandIssuer->GetTeamVoteController()->ListIssues( commandIssuer );
	}

	// and always list global issues
	if ( g_voteControllerGlobal )
	{
		g_voteControllerGlobal->ListIssues( commandIssuer );
	}

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
ConCommand ListIssues("listissues", CommandListIssues, "List all the issues that can be voted on.", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );

//-----------------------------------------------------------------------------
// Purpose: This should eventually ask the player what team they are voting on
// to take into account different idle / spectator rules.
//-----------------------------------------------------------------------------

int GetVoterTeam( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return TEAM_UNASSIGNED;

	CBasePlayer *pPlayer = ToBasePlayer( pEntity );
	if ( !pPlayer )
		return TEAM_UNASSIGNED;

	int iTeam = pPlayer->GetAssociatedTeamNumber( );

	return iTeam;

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CON_COMMAND_F( callvote, "Start a vote on an issue.", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS )
{
	if ( !g_voteControllerGlobal || !g_voteControllerCT || !g_voteControllerT )
	{
		DevMsg( "Vote Controllers Not Found!\n" );
		return;
	}

	CBasePlayer *pVoteCaller = UTIL_GetCommandClient();
	int iEntindex = 99;
	if ( pVoteCaller )
		iEntindex = pVoteCaller->entindex();

	if ( !sv_vote_allow_spectators.GetBool() && pVoteCaller && pVoteCaller->IsSpectator() )
	{
		g_voteControllerGlobal->SendVoteFailedMessage( VOTE_FAILED_SPECTATOR, pVoteCaller );
		return;

	}

	// Parameters
	char szEmptyDetails[ MAX_VOTE_DETAILS_LENGTH ];
	szEmptyDetails[ 0 ] = '\0';
	const char *arg2 = args[ 1 ];
	const char *arg3 = args.ArgC() >= 3 ? args[ 2 ] : szEmptyDetails;


	CVoteController *pVoteController;
	if ( pVoteCaller )
	{
		pVoteController = pVoteCaller->GetTeamVoteController();
	}
	else
	{
		pVoteController = g_voteControllerGlobal;
	}

	// If we don't have any arguments, invoke VoteSetup UI
	if( args.ArgC() < 2 )
	{
		pVoteController->SetupVote( iEntindex );

		return;
	}

	if ( g_voteControllerGlobal->HasIssue( arg2 ) )
	{
		g_voteControllerGlobal->CreateVote( iEntindex, arg2, arg3 );
	}
	else if ( pVoteController->HasIssue( arg2 ) )
	{
		pVoteController->CreateVote( iEntindex, arg2, arg3 );
	}
	else
	{
		DevMsg( "Vote Issue Not Found!\n" );
		return;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CVoteController::~CVoteController()
{
	if ( g_voteControllerGlobal == this )	{	g_voteControllerGlobal = NULL; }
	else if ( g_voteControllerCT == this )	{	g_voteControllerCT = NULL; }
	else if ( g_voteControllerT == this ) 	{	g_voteControllerT = NULL; }


	for( int issueIndex = 0; issueIndex < m_potentialIssues.Count(); ++issueIndex )
	{
		delete m_potentialIssues[issueIndex];
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::ResetData( void )
{
	m_iActiveIssueIndex = INVALID_ISSUE;

	for ( int index = 0; index < m_nVoteOptionCount.Count(); index++ )
	{
		m_nVoteOptionCount.Set( index, 0 );
	}

	m_nPotentialVotes = 0;
	m_acceptingVotesTimer.Invalidate();
	m_executeCommandTimer.Invalidate();
	m_iEntityHoldingVote = -1;
	m_iOnlyTeamToVote = TEAM_INVALID;
	m_bIsYesNoVote = true;

	for( int voteIndex = 0; voteIndex < MAX_PLAYERS; ++voteIndex )
	{
		m_nVotesCast[voteIndex] = VOTE_UNCAST;
	}
	m_arrVotedUsers.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::Spawn( void )
{
	ResetData();

	BaseClass::Spawn();

	SetThink( &CVoteController::VoteControllerThink );
	SetNextThink( gpGlobals->curtime );

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CVoteController::UpdateTransmitState( void )
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoteController::CanTeamCastVote( int iTeam ) const
{
	if ( m_iOnlyTeamToVote == TEAM_INVALID )
		return true;

	return iTeam == m_iOnlyTeamToVote;
}

//-----------------------------------------------------------------------------
// Purpose: Handles menu-driven setup of Voting
//-----------------------------------------------------------------------------
bool CVoteController::SetupVote( int iEntIndex )
{
	CBasePlayer *pVoteCaller = UTIL_PlayerByIndex( iEntIndex );
	if( !pVoteCaller )
		return false;

	bool bAllowVotes = sv_allow_votes.GetBool();
	if ( bAllowVotes && m_potentialIssues.Count() )
	{
		CSingleUserRecipientFilter filter( pVoteCaller );
		filter.MakeReliable();
		
		CCSUsrMsg_VoteSetup msg;
		
		for( int iIndex = 0; iIndex < m_potentialIssues.Count(); ++iIndex )
		{
			CBaseIssue *pCurrentIssue = m_potentialIssues[iIndex];
			
			if ( pCurrentIssue )
			{
				if ( pCurrentIssue->IsEnabled() )
				{
					msg.add_potential_issues( pCurrentIssue->GetTypeString() );
				}
				else
				{
					char szDisabledIssueStr[MAX_COMMAND_LENGTH + 12];
					V_strcpy( szDisabledIssueStr, pCurrentIssue->GetTypeString() );
					V_strcat( szDisabledIssueStr, " (Disabled on Server)", sizeof(szDisabledIssueStr) );

					msg.add_potential_issues( szDisabledIssueStr );
				}
			}
		}

		SendUserMessage( filter, CS_UM_VoteSetup, msg );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles console-driven setup of Voting
//-----------------------------------------------------------------------------
bool CVoteController::CreateVote( int iEntIndex, const char *pszTypeString, const char *pszDetailString )
{
	// Terrible Hack:  Dedicated servers pass 99 as the EntIndex
	bool bDedicatedServer = ( iEntIndex == 99 ) ? true : false;

	CBasePlayer *pVoteCaller = UTIL_PlayerByIndex( iEntIndex );

	if ( !pVoteCaller && !bDedicatedServer )
		return false;

	if( !sv_allow_votes.GetBool() )
	{
		SendVoteFailedMessage( VOTE_FAILED_DISABLED, pVoteCaller );

		return false;
	}

	// Already running a vote?
	if ( IsAVoteInProgress() )
	{
		// send a message to the user to who tried to vote that their vote failed and why
		ClientPrint( pVoteCaller, HUD_PRINTCENTER, "#SFUI_vote_failed_vote_in_progress" );
		return false;
	}
	
	// Find the issue the user is asking for
	for( int issueIndex = 0; issueIndex < m_potentialIssues.Count(); ++issueIndex )
	{
		CBaseIssue *pCurrentIssue = m_potentialIssues[issueIndex];
		if ( !pCurrentIssue )
			return false;
		
		if( FStrEq( pszTypeString, pCurrentIssue->GetTypeString() ) )
		{
			vote_create_failed_t nErrorCode = VOTE_FAILED_GENERIC;
			int nTime = 0;

			if( pCurrentIssue->CanCallVote( iEntIndex, pszTypeString, pszDetailString, nErrorCode, nTime ) )
			{
				// Prevent spamming commands
#ifndef _DEBUG
				if ( pVoteCaller && !pCurrentIssue->ShouldIgnoreCreationTimer() )
				{
					int nTimeLeft = sv_vote_creation_timer.GetFloat() - pVoteCaller->GetLastHeldVoteTimer().GetElapsedTime();
					if( pVoteCaller->GetLastHeldVoteTimer().HasStarted() && nTimeLeft > 1 )
					{
						SendVoteFailedMessage( VOTE_FAILED_RATE_EXCEEDED, pVoteCaller, nTimeLeft );
						return false;
					}
				}
#endif

				// if this is not an instant voting issue ( i.e. tournament pause match )

				if (pCurrentIssue->GetVotesRequiredToPass() > 1 )
				{
					// can't call it if there's a global vote in progress.
					if ( g_voteControllerGlobal && g_voteControllerGlobal->IsAVoteInProgress() )
					{
						// send a message to the user to who tried to vote that their vote failed and why
						ClientPrint( pVoteCaller, HUD_PRINTCENTER, "#SFUI_vote_failed_vote_in_progress" );
						return false;
					}


					// can't call it if this is a global vote and the other team is mid-vote.

					CVoteController * pOtherTeamVoteController = g_voteControllerGlobal;

					if ( pVoteCaller )
					{
						switch ( pVoteCaller->GetAssociatedTeamNumber() )
						{
							case TEAM_CT:
								pOtherTeamVoteController = g_voteControllerT;
								break;

							case TEAM_TERRORIST:
								pOtherTeamVoteController = g_voteControllerCT;
								break;

							default:
								break;
						}
					}

					if ( ( this == g_voteControllerGlobal ) && ( pOtherTeamVoteController && pOtherTeamVoteController->IsAVoteInProgress() ) )
					{
						// send a message to the user to who tried to vote that their vote failed and why
						ClientPrint( pVoteCaller, HUD_PRINTCENTER, "#SFUI_vote_failed_vote_in_progress" );
						return false;
					}
				}

				// Establish a bunch of data on this particular issue
				m_iEntityHoldingVote = iEntIndex;
				pCurrentIssue->SetIssueDetails( pszDetailString );
				m_bIsYesNoVote = pCurrentIssue->IsYesNoVote();
				m_iActiveIssueIndex = issueIndex;
				
				m_iOnlyTeamToVote = TEAM_INVALID;
				if ( !bDedicatedServer && pCurrentIssue->IsAllyRestrictedVote() )
					m_iOnlyTeamToVote = GetVoterTeam( pVoteCaller );
				
				// Now get our choices
				m_VoteOptions.RemoveAll();
				pCurrentIssue->GetVoteOptions( m_VoteOptions );
				int nNumVoteOptions = m_VoteOptions.Count();
				if ( nNumVoteOptions >= 2 )
				{
					IGameEvent *event = gameeventmanager->CreateEvent( "vote_options" );
					if ( event )
					{
						event->SetInt( "count", nNumVoteOptions );
						for ( int iIndex = 0; iIndex < nNumVoteOptions; iIndex++ )
						{
							char szNumber[2];
							Q_snprintf( szNumber, sizeof( szNumber ), "%i", iIndex + 1 );

							char szOptionName[8] = "option";
							Q_strncat( szOptionName, szNumber, sizeof( szOptionName ), COPY_ALL_CHARACTERS );

							event->SetString( szOptionName, m_VoteOptions[iIndex] );
						}
						gameeventmanager->FireEvent( event );
					}
				}
				else
				{
					Assert( nNumVoteOptions >= 2 );
				}

				// Get the data out to the client
				for( int playerIndex = 1; playerIndex <= MAX_PLAYERS; ++playerIndex )
				{
					CBasePlayer *pPlayer = UTIL_PlayerByIndex( playerIndex );

					if ( pPlayer && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
					{
						CSingleUserRecipientFilter filter( pPlayer );					
						filter.MakeReliable();
						
						CCSUsrMsg_VoteStart msg;
						msg.set_team( m_iOnlyTeamToVote );			// move into the filter
						msg.set_ent_idx( m_iEntityHoldingVote );
						msg.set_vote_type( pCurrentIssue->GetVoteIssue() );
						msg.set_disp_str( pCurrentIssue->GetDisplayString() );
						msg.set_details_str( pCurrentIssue->GetDetailsString() );
						msg.set_other_team_str( pCurrentIssue->GetOtherTeamDisplayString() );
						msg.set_is_yes_no_vote( m_bIsYesNoVote );
						SendUserMessage( filter, CS_UM_VoteStart, msg );
					}
				}

				UTIL_LogPrintf( "Vote started \"%s %s\" from #%u %s\n",
					pCurrentIssue->GetTypeString(),
					pCurrentIssue->GetDetailsString(),
					m_iEntityHoldingVote,
					pVoteCaller ? GameLogSystem()->FormatPlayer( pVoteCaller ) : "n/a" );

				m_nPotentialVotes = pCurrentIssue->CountPotentialVoters();
				// FIX TO MAKE REMATCH ONLY
				float flVoteDuration = CSGameRules()->IsQueuedMatchmaking() ? 45.0f : sv_vote_timer_duration.GetFloat();
				m_acceptingVotesTimer.Start( flVoteDuration );
				pCurrentIssue->OnVoteStarted();

				// Force the vote holder to agree with a Yes/No vote
				if ( m_bIsYesNoVote && !bDedicatedServer )
				{
					TryCastVote( iEntIndex, "Option1" );
				}

				if ( !bDedicatedServer )
				{
					pVoteCaller->GetLastHeldVoteTimer().Start();
				}

				return true;
			}
			else
			{
				if ( !bDedicatedServer )
				{
					SendVoteFailedMessage( pCurrentIssue->MakeVoteFailErrorCodeForClients( nErrorCode ), pVoteCaller, nTime );
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Sent to everyone, unless we pass a player pointer
//-----------------------------------------------------------------------------
void CVoteController::SendVoteFailedMessage( vote_create_failed_t nReason, CBasePlayer *pVoteCaller, int nTime )
{
	// driller: need to merge all failure case stuff into a single path
	if ( pVoteCaller ) 
	{
		CSingleUserRecipientFilter user( pVoteCaller );
		user.MakeReliable();

		CCSUsrMsg_CallVoteFailed msg;
		msg.set_reason( nReason );
		msg.set_time( nTime );
		SendUserMessage( user, CS_UM_CallVoteFailed, msg );
	}
	else
	{
		UTIL_LogPrintf("Vote failed \"%s %s\" \n",
			m_potentialIssues[m_iActiveIssueIndex]->GetTypeString(),
			m_potentialIssues[m_iActiveIssueIndex]->GetDetailsString() );

		CBroadcastRecipientFilter filter;
		filter.MakeReliable();

		CCSUsrMsg_VoteFailed msg;
		msg.set_team( m_iOnlyTeamToVote );
		msg.set_reason( nReason );
		SendUserMessage( filter, CS_UM_VoteFailed, msg );
	}
}

//-----------------------------------------------------------------------------
// Purpose:  Player generated a vote command.  i.e. /vote option1
//-----------------------------------------------------------------------------
CVoteController::TryCastVoteResult CVoteController::TryCastVote( int iEntIndex, const char *pszVoteString )
{
	if( !sv_allow_votes.GetBool() )
		return CAST_FAIL_SERVER_DISABLE;

	if( iEntIndex > MAX_PLAYERS )
		return CAST_FAIL_SYSTEM_ERROR;

	if( m_iActiveIssueIndex == INVALID_ISSUE )
		return CAST_FAIL_NO_ACTIVE_ISSUE;

	if( m_executeCommandTimer.HasStarted() )
		return CAST_FAIL_VOTE_CLOSED;

	CBaseEntity *pVoter = UTIL_EntityByIndex( iEntIndex );
	if ( !IsValidVoter( ToBasePlayer( pVoter ) ) )
		return CAST_FAIL_TEAM_RESTRICTED;

	if( m_potentialIssues[m_iActiveIssueIndex] && m_potentialIssues[m_iActiveIssueIndex]->IsAllyRestrictedVote() )
	{
		CBaseEntity *pVoteHolder = UTIL_EntityByIndex( m_iEntityHoldingVote );

		if( ( pVoteHolder == NULL ) || ( pVoter == NULL ) || ( GetVoterTeam( pVoteHolder ) != GetVoterTeam( pVoter ) ) )
		{
			return CAST_FAIL_TEAM_RESTRICTED;
		}
	}

	// Look for a previous vote
	int nOldVote = VOTE_UNCAST;
	if ( iEntIndex < MAX_PLAYERS )
	{
		nOldVote = m_nVotesCast[iEntIndex];
	}
#ifndef DEBUG
	if( nOldVote != VOTE_UNCAST )
	{
		return CAST_FAIL_NO_CHANGES;
	}
#endif // !DEBUG

	// Which option are they voting for?
	int nCurrentVote = VOTE_UNCAST;
	if ( !StringHasPrefix( pszVoteString, "Option" ) )
		return CAST_FAIL_SYSTEM_ERROR;

	nCurrentVote = (CastVote)( atoi( pszVoteString + V_strlen( "Option" ) ) - 1 );

	if ( nCurrentVote < VOTE_OPTION1 || nCurrentVote > VOTE_OPTION5 )
		return CAST_FAIL_SYSTEM_ERROR;
	
	// They're changing their vote
#ifdef DEBUG
	if ( nOldVote != VOTE_UNCAST )
	{
		if( nOldVote == nCurrentVote )
		{
			return CAST_FAIL_DUPLICATE;
		}
		VoteChoice_Decrement( nOldVote );
	}
#endif // DEBUG

	// With a Yes/No vote, slam anything past "No" to No
	if ( m_potentialIssues[m_iActiveIssueIndex]->IsYesNoVote() )
	{
		if ( nCurrentVote > VOTE_OPTION2 )
			nCurrentVote = VOTE_OPTION2;
	}

#ifndef DEBUG
	if ( CBasePlayer *pBasePlayerVoter = ToBasePlayer( pVoter ) )
	{
		if ( uint64 uiSteamID = pBasePlayerVoter->GetSteamIDAsUInt64() )
		{
			if ( m_arrVotedUsers.Find( uiSteamID ) == m_arrVotedUsers.InvalidIndex() )
			{
				m_arrVotedUsers.AddToTail( uiSteamID );	// remember that this user already voted
			}
			else
			{
				return CAST_FAIL_NO_CHANGES;
			}
		}
	}
#endif

	// Register and track this vote
	VoteChoice_Increment( nCurrentVote );
	m_nVotesCast[iEntIndex] = nCurrentVote;

	// Tell the client-side UI
	IGameEvent *event = gameeventmanager->CreateEvent( "vote_cast" );
	if ( event )
	{
		event->SetInt( "vote_option", nCurrentVote );
		event->SetInt( "team", m_iOnlyTeamToVote );
		event->SetInt( "entityid", iEntIndex );
		gameeventmanager->FireEvent( event );
	}

	UTIL_LogPrintf( "Vote cast \"%s %s\" from #%u %s option%d\n",
		m_potentialIssues[m_iActiveIssueIndex]->GetTypeString(),
		m_potentialIssues[m_iActiveIssueIndex]->GetDetailsString(),
		iEntIndex,
		pVoter ? GameLogSystem()->FormatPlayer( pVoter ) : "n/a",
		nCurrentVote );

	CheckForEarlyVoteClose();

	return CAST_OK;
}

//-----------------------------------------------------------------------------
// Purpose:  Increments the vote count for a particular vote option 
//			 i.e. nVoteChoice = 0 might mean a Yes vote
//-----------------------------------------------------------------------------
void CVoteController::VoteChoice_Increment( int nVoteChoice )
{
	if ( nVoteChoice < VOTE_OPTION1 || nVoteChoice > VOTE_OPTION5 )
		return;

	int nValue = m_nVoteOptionCount.Get( nVoteChoice );
	m_nVoteOptionCount.Set( nVoteChoice, ++nValue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::VoteChoice_Decrement( int nVoteChoice )
{
	if ( nVoteChoice < VOTE_OPTION1 || nVoteChoice > VOTE_OPTION5 )
		return;

	int nValue = m_nVoteOptionCount.Get( nVoteChoice );
	m_nVoteOptionCount.Set( nVoteChoice, --nValue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::VoteControllerThink( void )
{
	if ( !m_potentialIssues.IsValidIndex( m_iActiveIssueIndex ) )
	{
		SetNextThink( gpGlobals->curtime + 0.5f );

		return;
	}

	CBaseIssue *pActiveIssue = m_potentialIssues[m_iActiveIssueIndex];

	int nWinningVoteOption = GetWinningVoteOption();
	if ( !pActiveIssue->IsYesNoVote() || nWinningVoteOption >= m_VoteOptions.Count() )
	{
		Assert( nWinningVoteOption >= 0 && nWinningVoteOption < m_VoteOptions.Count() );
		Msg( "Trying to resolve a vote that is not a YES/NO vote, YES/NO votes aren't currently supported! \n");
		return;
	}

	bool bVoteHasFinished = false;
	bool bVoteShouldBailOnNos = (pActiveIssue->GetVoteIssue() == VOTEISSUE_REMATCH);

	// Vote time is up - process the result
	if( m_acceptingVotesTimer.HasStarted() )
	{	
		int nNumVotesYES = m_nVoteOptionCount[VOTE_OPTION1];
		int nNumVotesNO = m_nVoteOptionCount[VOTE_OPTION2];

		//int nVoteTally = nNumVotesYES + nNumVotesNO;

		bool bVotePassed = false;
	
		int nVotesToSucceed = pActiveIssue->GetVotesRequiredToPass();

		// Have we exceeded the required ratio of Voted-vs-Abstained?
		if ( nNumVotesYES >= nVotesToSucceed )
		{
			// bail early, we succeeded
			bVotePassed = true;
			bVoteHasFinished = true;
		}
		else if ( bVoteShouldBailOnNos && (m_nPotentialVotes - nNumVotesNO) < nVotesToSucceed )
		{
			// failed because too many no votes
			SendVoteFailedMessage( pActiveIssue->MakeVoteFailErrorCodeForClients( VOTE_FAILED_YES_MUST_EXCEED_NO ) );
			bVotePassed = false;
			bVoteHasFinished = true;
		}
		else if ( m_acceptingVotesTimer.IsElapsed() )
		{
			// timed out, not enough people voted yes
			SendVoteFailedMessage( pActiveIssue->MakeVoteFailErrorCodeForClients( VOTE_FAILED_QUORUM_FAILURE ) );
			bVotePassed = false;
			bVoteHasFinished = true;
		}

		if ( bVoteHasFinished )
		{
			// for record-keeping
			if ( pActiveIssue->IsYesNoVote() )
			{
				pActiveIssue->SetYesNoVoteCount( m_nVoteOptionCount[VOTE_OPTION1],  m_nVoteOptionCount[VOTE_OPTION2], m_nPotentialVotes );
			}

			m_acceptingVotesTimer.Invalidate();

			if ( bVotePassed )
			{
				m_executeCommandTimer.Start( pActiveIssue->GetCommandDelay() );
				m_resetVoteTimer.Start( 5.0 );

				CBaseEntity *pVoteHolder = UTIL_EntityByIndex( m_iEntityHoldingVote );
				CBasePlayer *pVoteHolderPlayer = ( pVoteHolder && pVoteHolder->IsPlayer() ) ? (CBasePlayer *)( pVoteHolder ) : NULL;
				if( pVoteHolderPlayer )
				{
					pVoteHolderPlayer->GetLastHeldVoteTimer().Invalidate();	// You can go ahead and make a new vote since yours passed.
				}

				UTIL_LogPrintf("Vote succeeded \"%s %s\" from #%u %s\n",
					pActiveIssue->GetTypeString(),
					pActiveIssue->GetDetailsString(),
					m_iEntityHoldingVote,
					pVoteHolderPlayer ? GameLogSystem()->FormatPlayer( pVoteHolderPlayer ) : "n/a" );

				CBroadcastRecipientFilter filter;
				filter.MakeReliable();

				CCSUsrMsg_VotePass msg;
				msg.set_team( m_iOnlyTeamToVote );
				msg.set_vote_type( pActiveIssue->GetVoteIssue() );
				msg.set_disp_str( pActiveIssue->GetVotePassedString() );
				msg.set_details_str( pActiveIssue->GetDetailsString() );
				SendUserMessage( filter, CS_UM_VotePass, msg );
			}
			else
			{
				pActiveIssue->OnVoteFailed();
				m_resetVoteTimer.Start( 5.0 );
			}
		}
	}

	// Vote passed - execute the command
	if( m_executeCommandTimer.HasStarted() && m_executeCommandTimer.IsElapsed() )
	{

		m_executeCommandTimer.Invalidate();
		m_potentialIssues[m_iActiveIssueIndex]->ExecuteCommand();
	}

	if ( m_resetVoteTimer.HasStarted() && m_resetVoteTimer.IsElapsed() )
	{
		ResetData();
		m_resetVoteTimer.Invalidate();
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: End the vote early if everyone's voted
//-----------------------------------------------------------------------------
void CVoteController::CheckForEarlyVoteClose( void )
{
	int nVoteTally = 0;
	for ( int index = 0; index < MAX_VOTE_OPTIONS; index++ )
	{
		nVoteTally += m_nVoteOptionCount.Get( index );
	}

	if( nVoteTally >= m_nPotentialVotes )
	{
		m_acceptingVotesTimer.Start( 0 );	// Run the timer out right now
	}
}

#ifdef DEBUG  // Don't want to do this check for debug builds (so we can test with bots)
	ConVar sv_vote_ignore_bots( "sv_vote_ignore_bots", "0" );
    #define SV_VOTE_IGNORE_BOTS sv_vote_ignore_bots.GetBool()
#else
#define SV_VOTE_IGNORE_BOTS true
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVoteController::IsValidVoter( CBasePlayer *pWhom )
{
	if ( pWhom == NULL )
		return false;

	if ( !pWhom->IsConnected() )
		return false;

	if ( !sv_vote_allow_spectators.GetBool() || !sv_vote_count_spectator_votes.GetBool() )
	{
		if ( pWhom->GetTeamNumber() != TEAM_TERRORIST && pWhom->GetTeamNumber() != TEAM_CT )
			return false;
	}

	if ( SV_VOTE_IGNORE_BOTS )  // Don't want to do this check for debug builds (so we can test with bots)
	{
		if ( pWhom->IsBot() )
			return false;

		if ( pWhom->IsFakeClient() )
			return false;
	}

	if ( pWhom->IsHLTV() )
		return false;

	if ( pWhom->IsReplay() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::RegisterIssue( CBaseIssue *pszNewIssue )
{
	m_potentialIssues.AddToTail( pszNewIssue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVoteController::ListIssues( CBasePlayer *pForWhom )
{
	if( !sv_allow_votes.GetBool() )
		return;

	ClientPrint( pForWhom, HUD_PRINTCONSOLE, "---Vote commands---\n" );

	for( int issueIndex = 0; issueIndex < m_potentialIssues.Count(); ++issueIndex )
	{
		CBaseIssue *pCurrentIssue = m_potentialIssues[issueIndex];
		pCurrentIssue->ListIssueDetails( pForWhom );
	}
	ClientPrint( pForWhom, HUD_PRINTCONSOLE, "--- End Vote commands---\n" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CVoteController::GetWinningVoteOption( void )
{
	if ( m_potentialIssues[m_iActiveIssueIndex]->IsYesNoVote() )
	{
		return ( m_nVoteOptionCount[VOTE_OPTION1] > m_nVoteOptionCount[VOTE_OPTION2] ) ? VOTE_OPTION1 : VOTE_OPTION2;
	}
	else
	{
		CUtlVector <int> pVoteCounts;

		// Which option had the most votes?
		// driller:  Need to handle ties
		int nHighest = m_nVoteOptionCount[0];
		for ( int iIndex = 0; iIndex < m_nVoteOptionCount.Count(); iIndex ++ )
		{
			nHighest = ( ( nHighest < m_nVoteOptionCount[iIndex] ) ? m_nVoteOptionCount[iIndex] : nHighest );
			pVoteCounts.AddToTail( m_nVoteOptionCount[iIndex] );
		}
		
		m_nHighestCountIndex = -1;
		for ( int iIndex = 0; iIndex < m_nVoteOptionCount.Count(); iIndex++ )
		{
			if ( m_nVoteOptionCount[iIndex] == nHighest )
			{
				m_nHighestCountIndex = iIndex;
				// henryg: break on first match, not last. this avoids a crash
				// if we are all tied at zero and we pick something beyond the
				// last vote option. this code really ought to ignore attempts
				// to tally votes for options beyond the last valid one!
				break;
			}
		}

		return m_nHighestCountIndex;
	}

	return -1;
}

bool CVoteController::HasIssue( const char *pszIssue )
{
	for ( int issueIndex = 0; issueIndex < m_potentialIssues.Count( ); ++issueIndex )
	{
		CBaseIssue *pCurrentIssue = m_potentialIssues[ issueIndex ];
		if ( !pCurrentIssue )
			return false;

		if ( FStrEq( pszIssue, pCurrentIssue->GetTypeString( ) ) )
			return true;
	}

	return false;
}

void CVoteController::EndVoteImmediately( void )
{
	if ( !IsAVoteInProgress( ) )
		return;

	CBaseIssue *pActiveIssue = m_potentialIssues[ m_iActiveIssueIndex ];

	// for record-keeping
	if ( pActiveIssue->IsYesNoVote( ) )
	{
		pActiveIssue->SetYesNoVoteCount( m_nVoteOptionCount[ VOTE_OPTION1 ], m_nVoteOptionCount[ VOTE_OPTION2 ], m_nPotentialVotes );
	}

	SendVoteFailedMessage( pActiveIssue->MakeVoteFailErrorCodeForClients( VOTE_FAILED_QUORUM_FAILURE ) );
	pActiveIssue->OnVoteFailed( );
	m_resetVoteTimer.Start( 5.0 );

	m_acceptingVotesTimer.Invalidate( );
}

//-----------------------------------------------------------------------------
// Purpose: BaseIssue
//-----------------------------------------------------------------------------
CBaseIssue::CBaseIssue( const char *pszTypeString, CVoteController *pVoteController )
{
	V_strcpy_safe( m_szTypeString, pszTypeString );

	m_iNumYesVotes = 0;
	m_iNumNoVotes = 0;
	m_iNumPotentialVotes = 0;

	m_pVoteController = pVoteController;

	ASSERT( pVoteController );
	if ( pVoteController )
	{
		pVoteController->RegisterIssue( this );
		
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseIssue::~CBaseIssue()
{
	for ( int index = 0; index < m_FailedVotes.Count(); index++ )
	{
		FailedVote *pFailedVote = m_FailedVotes[index];
		delete pFailedVote;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CBaseIssue::GetTypeString( void )
{
	return m_szTypeString;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CBaseIssue::GetDetailsString( void )
{
	return m_szDetailsString;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseIssue::SetIssueDetails( const char *pszDetails )
{
	V_strcpy_safe( m_szDetailsString, pszDetails );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseIssue::IsAllyRestrictedVote( void )
{
	return false;
}

int CBaseIssue::GetVotesRequiredToPass( void )
{
	// TODO: to reduce risk of new bugs, this logic was preserved as-is from VoteControllerThink. But it can/should be cleaned up for legibility.
	int nPotentialVoters = CountPotentialVoters();
	
	// BUGBUG: disconnecting/reconnecting players during the vote can affect the final tally, so we will use the larger number here:
	if ( m_pVoteController && ( m_pVoteController->GetPotentialVotes( ) > nPotentialVoters ) )
		nPotentialVoters = m_pVoteController->GetPotentialVotes( );

	int nVotesToSucceed = 0;

	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() 
		 && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && !IsAllyRestrictedVote() )
	{	// in queued matchmaking must have 100% voting to succeed (unless kick vote)
		nVotesToSucceed = CSGameRules()->m_numQueuedMatchmakingAccounts;
	}
	// Unanimous votes require all attending humans (which might be less than 10 players)
	else if ( IsUnanimousVoteToPass() )
	{
		nVotesToSucceed = MAX( 1, nPotentialVoters );
	}
	else
	{
		float flnVotesToSucceed = ( CSGameRules() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() ) ? MAX( 1, nPotentialVoters - 1 ) : ( ( float )nPotentialVoters * sv_vote_quorum_ratio.GetFloat() );
		nVotesToSucceed = ceil( flnVotesToSucceed );
	}

	return nVotesToSucceed;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CBaseIssue::GetVotePassedString( void )
{
	return "Unknown vote passed.";
}

float CBaseIssue::GetFailedVoteLockOutTime( void )
{
	return sv_vote_failure_timer.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:  Store failures to prevent vote spam
//-----------------------------------------------------------------------------
void CBaseIssue::OnVoteFailed( void )
{
	// Check for an existing match
	for ( int index = 0; index < m_FailedVotes.Count(); index++ )
	{
		FailedVote *pFailedVote = m_FailedVotes[index];
		if ( Q_strncmp( pFailedVote->szFailedVoteParameter, GetDetailsString(), Q_ARRAYSIZE( pFailedVote->szFailedVoteParameter ) - 1 ) == 0 )
		{
			pFailedVote->flLockoutTime = gpGlobals->curtime + GetFailedVoteLockOutTime();

			return;
		}
	}

	// Need to create a new one
	FailedVote *pNewFailedVote = new FailedVote;
	int iIndex = m_FailedVotes.AddToTail( pNewFailedVote );
	V_strcpy_safe( m_FailedVotes[iIndex]->szFailedVoteCommand, GetTypeString() );
	V_strcpy_safe( m_FailedVotes[iIndex]->szFailedVoteParameter, GetDetailsString() );
	m_FailedVotes[iIndex]->flLockoutTime = gpGlobals->curtime + GetFailedVoteLockOutTime();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseIssue::CanTeamCallVote( int iTeam ) const
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseIssue::CanCallVote( int iEntIndex, const char *pszCommand, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime )
{
	// Automated server vote - don't bother testing against it
	if ( iEntIndex == 99 )
		return true;

	// Bogus player
	if( iEntIndex == -1 )
		return false;

#ifdef TF_DLL
	if ( TFGameRules() && TFGameRules()->IsInWaitingForPlayers() && !TFGameRules()->IsInTournamentMode() )
	{
		nFailCode = VOTE_FAILED_WAITINGFORPLAYERS;
		return false;
	}
#endif // TF_DLL

	if ( !sv_vote_allow_in_warmup.GetBool() && CSGameRules() && CSGameRules()->IsWarmupPeriod() && !IsEnabledDuringWarmup() )
	{
		nFailCode = VOTE_FAILED_WAITINGFORPLAYERS;
		return false;
	}

	CBaseEntity *pVoteCaller = UTIL_EntityByIndex( iEntIndex );
	if( pVoteCaller && !CanTeamCallVote( GetVoterTeam( pVoteCaller ) ) )
	{
		nFailCode = VOTE_FAILED_TEAM_CANT_CALL;
		return false;
	}

	if ( IsVoteCallExclusiveToSpectators( ) )
	{
		CBasePlayer *pVoteCallerPlayer = ToBasePlayer( pVoteCaller );

		if ( !pVoteCallerPlayer || !pVoteCallerPlayer->IsSpectator( ) )
		{
			nFailCode = VOTE_FAILED_TEAM_CANT_CALL;
			return false;
		}
	}

	// Only few votes are actually allowed in queue matchmaking mode
	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() && !IsEnabledInQueuedMatchmaking() )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	// Disable all voting after rematch vote is initiated at the end of the match
	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() && ( CSGameRules()->m_eQueuedMatchmakingRematchState >= CSGameRules()->k_EQueuedMatchmakingRematchState_VoteToRematchInProgress ) )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	// don't let kick votes happen on match point or last round
	if ( CSGameRules() && (CSGameRules()->IsQueuedMatchmaking() || sv_vote_disallow_kick_on_match_point.GetBool()) && 
		 CSGameRules()->IsLastRoundOfMatch() && CSGameRules()->IsMatchPoint() && FStrEq( VOTEISSUE_NAME_KICK, pszCommand ) )
	{
		nFailCode = VOTE_FAILED_ISSUE_DISABLED;
		return false;
	}

	// Did this fail recently?
	for( int iIndex = 0; iIndex < m_FailedVotes.Count(); iIndex++ )
	{
		FailedVote *pCurrentFailure = m_FailedVotes[iIndex];
		int nTimeRemaining = pCurrentFailure->flLockoutTime - gpGlobals->curtime;
		bool bFailed = false;

		if ( nTimeRemaining > 1 )
		{
			if ( Q_strlen( pCurrentFailure->szFailedVoteCommand ) > 0 && FStrEq( pCurrentFailure->szFailedVoteCommand, pszCommand ) )
			{
				// If this issue requires a parameter, see if we're voting for the same one again (i.e. changelevel ctf_2fort)
				if ( Q_strlen( pCurrentFailure->szFailedVoteParameter ) > 0 && FStrEq( pCurrentFailure->szFailedVoteParameter, pszDetails ) )
				{
					bFailed = true;
					if ( FStrEq( VOTEISSUE_NAME_CHANGELEVEL, pszCommand ) )
					{
						nFailCode = VOTE_FAILED_FAILED_RECENT_CHANGEMAP;	
					}
					else if ( FStrEq( VOTEISSUE_NAME_KICK, pszCommand ) )
					{
						nFailCode = VOTE_FAILED_FAILED_RECENT_KICK;	
					}
					else
					{
						nFailCode = VOTE_FAILED_FAILED_RECENTLY;	
					}
				}
				else
				{
					if ( FStrEq( VOTEISSUE_NAME_SWAPTEAMS, pszCommand ) )
					{
						nFailCode = VOTE_FAILED_FAILED_RECENT_SWAPTEAMS;	
						bFailed = true;
					}
					else if ( FStrEq( VOTEISSUE_NAME_SCRAMBLE, pszCommand ) )
					{
						nFailCode = VOTE_FAILED_FAILED_RECENT_SCRAMBLETEAMS;	
						bFailed = true;
					}
					else if ( FStrEq( VOTEISSUE_NAME_RESTARTGAME, pszCommand ) )
					{
						nFailCode = VOTE_FAILED_FAILED_RECENT_RESTART;	
						bFailed = true;
					}
				}
			}
			// Otherwise we have a parameter-less vote, so just check the lockout timer (i.e. restartgame)
			else
			{
				bFailed = true;
				nFailCode = VOTE_FAILED_FAILED_RECENTLY;	
			}
		}

		if ( bFailed )
		{	
			nTime = nTimeRemaining;
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CBaseIssue::CountPotentialVoters( void )
{
	int nTotalPlayers = 0;

	for( int playerIndex = 1; playerIndex <= MAX_PLAYERS; ++playerIndex )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( playerIndex );
		if ( m_pVoteController && m_pVoteController->IsValidVoter( pPlayer ) )
		{
			if ( m_pVoteController->CanTeamCastVote( GetVoterTeam( pPlayer ) ) )
			{
				nTotalPlayers++;
			}
		}
	}

	return nTotalPlayers;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CBaseIssue::GetNumberVoteOptions( void )
{
	return 2;  // The default issue is Yes/No (so 2), but it can be anywhere between 1 and MAX_VOTE_COUNT
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseIssue::IsYesNoVote( void )
{
	return true;  // Default
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseIssue::SetYesNoVoteCount( int iNumYesVotes, int iNumNoVotes, int iNumPotentialVotes )
{
	m_iNumYesVotes = iNumYesVotes;
	m_iNumNoVotes = iNumNoVotes;
	m_iNumPotentialVotes = iNumPotentialVotes;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseIssue::ListStandardNoArgCommand( CBasePlayer *forWhom, const char *issueString )
{
	char szBuffer[MAX_COMMAND_LENGTH];
	Q_snprintf( szBuffer, MAX_COMMAND_LENGTH, "callvote %s\n", issueString );
	ClientPrint( forWhom, HUD_PRINTCONSOLE, szBuffer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseIssue::GetVoteOptions( CUtlVector <const char*> &vecNames )
{
	// The default vote issue is a Yes/No vote
	vecNames.AddToHead( "Yes" );
	vecNames.AddToTail( "No" );

	return true;
}

float CBaseIssue::GetCommandDelay( void )
{
	return sv_vote_command_delay.GetFloat();
}

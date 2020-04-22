//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: CS's custom CPlayerResource
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#include "cs_player.h"
#include "cs_simple_hostage.h"
#include "cs_player_resource.h"
#include "econ_game_account_client.h"
#include "weapon_c4.h"
#include <coordsize.h>
#include "cs_bot_manager.h"
#include "cs_gamerules.h"
#include "cs_bot.h"
#include "cs_team.h"

extern bool	g_fGameOver;

// Datatable
IMPLEMENT_SERVERCLASS_ST(CCSPlayerResource, DT_CSPlayerResource)
	SendPropInt( SENDINFO( m_iPlayerC4 ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iPlayerVIP ), 8, SPROP_UNSIGNED ),
	SendPropArray3( SENDINFO_ARRAY3(m_bHostageAlive), SendPropInt( SENDINFO_ARRAY(m_bHostageAlive), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_isHostageFollowingSomeone), SendPropInt( SENDINFO_ARRAY(m_isHostageFollowingSomeone), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iHostageEntityIDs), SendPropInt( SENDINFO_ARRAY(m_iHostageEntityIDs), -1, SPROP_UNSIGNED ) ),
	SendPropVector( SENDINFO(m_bombsiteCenterA), -1, SPROP_COORD),
	SendPropVector( SENDINFO(m_bombsiteCenterB), -1, SPROP_COORD),
	SendPropArray3( SENDINFO_ARRAY3(m_hostageRescueX), SendPropInt( SENDINFO_ARRAY(m_hostageRescueX), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_hostageRescueY), SendPropInt( SENDINFO_ARRAY(m_hostageRescueY), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_hostageRescueZ), SendPropInt( SENDINFO_ARRAY(m_hostageRescueZ), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMVPs), SendPropInt( SENDINFO_ARRAY(m_iMVPs), COORD_INTEGER_BITS+1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iArmor), SendPropInt( SENDINFO_ARRAY(m_iArmor), COORD_INTEGER_BITS+1, SPROP_UNSIGNED ) ),	
	SendPropArray3( SENDINFO_ARRAY3(m_bHasDefuser), SendPropInt( SENDINFO_ARRAY(m_bHasDefuser), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_bHasHelmet), SendPropInt( SENDINFO_ARRAY(m_bHasHelmet), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iScore), SendPropInt( SENDINFO_ARRAY(m_iScore), 32) ),	
	SendPropArray3( SENDINFO_ARRAY3(m_iCompetitiveRanking), SendPropInt( SENDINFO_ARRAY(m_iCompetitiveRanking), 32) ),	
	SendPropArray3( SENDINFO_ARRAY3(m_iCompetitiveWins), SendPropInt( SENDINFO_ARRAY(m_iCompetitiveWins), 32) ),	
	SendPropArray3( SENDINFO_ARRAY3( m_iCompTeammateColor ), SendPropInt( SENDINFO_ARRAY( m_iCompTeammateColor ), 32 ) ),
	
#if CS_CONTROLLABLE_BOTS_ENABLED
	SendPropArray3( SENDINFO_ARRAY3(m_bControllingBot), SendPropInt( SENDINFO_ARRAY(m_bControllingBot), 1, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iControlledPlayer), SendPropInt( SENDINFO_ARRAY(m_iControlledPlayer), 8, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iControlledByPlayer), SendPropInt( SENDINFO_ARRAY(m_iControlledByPlayer), 8, SPROP_UNSIGNED ) ),
#endif
	SendPropArray3( SENDINFO_ARRAY3(m_iBotDifficulty), SendPropInt( SENDINFO_ARRAY(m_iBotDifficulty), 32) ),
	SendPropArray3( SENDINFO_ARRAY3(m_szClan), SendPropStringT( SENDINFO_ARRAY(m_szClan) ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iTotalCashSpent), SendPropInt( SENDINFO_ARRAY(m_iTotalCashSpent), 32) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iCashSpentThisRound), SendPropInt( SENDINFO_ARRAY(m_iCashSpentThisRound), 32) ),
	SendPropArray3( SENDINFO_ARRAY3(m_nEndMatchNextMapVotes), SendPropInt( SENDINFO_ARRAY(m_nEndMatchNextMapVotes), 32) ),
	SendPropBool( SENDINFO( m_bEndMatchNextMapAllVoted ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_nActiveCoinRank), SendPropInt( SENDINFO_ARRAY(m_nActiveCoinRank), 32) ),
	SendPropArray3( SENDINFO_ARRAY3(m_nMusicID), SendPropInt( SENDINFO_ARRAY(m_nMusicID), 32) ),
	// SendPropArray3( SENDINFO_ARRAY3(m_bIsAssassinationTarget), SendPropBool( SENDINFO_ARRAY(m_bIsAssassinationTarget), 32) ),

	SendPropArray3( SENDINFO_ARRAY3(m_nPersonaDataPublicLevel), SendPropInt( SENDINFO_ARRAY(m_nPersonaDataPublicLevel), 32) ),
	SendPropArray3( SENDINFO_ARRAY3(m_nPersonaDataPublicCommendsLeader), SendPropInt( SENDINFO_ARRAY(m_nPersonaDataPublicCommendsLeader), 32) ),
	SendPropArray3( SENDINFO_ARRAY3(m_nPersonaDataPublicCommendsTeacher), SendPropInt( SENDINFO_ARRAY(m_nPersonaDataPublicCommendsTeacher), 32) ),
	SendPropArray3( SENDINFO_ARRAY3(m_nPersonaDataPublicCommendsFriendly), SendPropInt( SENDINFO_ARRAY(m_nPersonaDataPublicCommendsFriendly), 32) ),
	
	
END_SEND_TABLE()

BEGIN_DATADESC( CCSPlayerResource )
	// DEFINE_ARRAY( m_iPing, FIELD_INTEGER, MAX_PLAYERS+1 ),
	// DEFINE_ARRAY( m_iPacketloss, FIELD_INTEGER, MAX_PLAYERS+1 ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( cs_player_manager, CCSPlayerResource );

CCSPlayerResource::CCSPlayerResource( void )
{
	m_bEndMatchNextMapAllVoted = false;
	m_bPreferencesAssigned_T = false;
	m_bPreferencesAssigned_CT = false;
	memset( m_nAttemptedToGetColor, false, sizeof( m_nAttemptedToGetColor ) );
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerResource::UpdatePlayerData( void )
{
	int i;

	m_iPlayerC4 = 0;
	m_iPlayerVIP = 0;

	int nTotalPlayingPlayers = 0;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		int botDifficulty = -1;
		CCSPlayer *pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
		
		bool bSetValidRanking = false;

		if ( pPlayer && pPlayer->IsConnected() )
		{
			if ( pPlayer->IsVIP() )
			{
				// we should only have one VIP
				Assert( m_iPlayerVIP == 0 );
				m_iPlayerVIP = i;
			}

			if ( pPlayer->HasC4() )
			{
				// we should only have one bomb
				m_iPlayerC4 = i;
			}

			m_iMVPs.Set( i, pPlayer->GetNumMVPs() );
			m_bHasDefuser.Set( i, pPlayer->HasDefuser() );
			m_bHasHelmet.Set( i, pPlayer->m_bHasHelmet );
			m_iArmor.Set( i, pPlayer->ArmorValue() );			
			m_iScore.Set( i, pPlayer->GetScore() );
			m_iTotalCashSpent.Set( i, pPlayer->GetTotalCashSpent() );
			m_iCashSpentThisRound.Set( i, pPlayer->GetCashSpentThisRound() );
			m_szClan.Set(i, AllocPooledString( pPlayer->GetClanTag() ) );

			m_nEndMatchNextMapVotes.Set( i, pPlayer->GetEndMatchNextMapVote() );

			m_nActiveCoinRank.Set( i, pPlayer->GetRank( MEDAL_CATEGORY_SEASON_COIN ) );

			m_nMusicID.Set( i, pPlayer->GetMusicID() );

			// UpdateAssassinationTargets();

			if ( CEconPersonaDataPublic const *pPublic = pPlayer->GetPersonaDataPublic() )
			{
				m_nPersonaDataPublicLevel.Set( i, pPublic->Obj().player_level() );
				m_nPersonaDataPublicCommendsLeader.Set( i, pPublic->Obj().commendation().cmd_leader() );
				m_nPersonaDataPublicCommendsTeacher.Set( i, pPublic->Obj().commendation().cmd_teaching() );
				m_nPersonaDataPublicCommendsFriendly.Set( i, pPublic->Obj().commendation().cmd_friendly() );
			}
			else
			{
				m_nPersonaDataPublicLevel.Set( i, -1 );
				m_nPersonaDataPublicCommendsLeader.Set( i, -1 );
				m_nPersonaDataPublicCommendsTeacher.Set( i, -1 );
				m_nPersonaDataPublicCommendsFriendly.Set( i, -1 );
			}

			if ( pPlayer->IsBot() )
			{
				CCSBot* pBot = dynamic_cast< CCSBot* >( pPlayer );

				if ( pBot )
				{
					// Retrieve and store the bot's difficulty level
					const BotProfile* pProfile = pBot->GetProfile();

					if ( pProfile )
					{
						botDifficulty = pProfile->GetMaxDifficulty();
					}

					m_iCompTeammateColor.Set( i, -2 );
					m_nAttemptedToGetColor[i] = true;
//					SetPlayerTeammateColor( i, false );
				
				}
			}
			else
			{
				if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
					nTotalPlayingPlayers++;
			
				SetPlayerTeammateColor( i, false );
			}

			if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
			{
				for ( int k = 0; k < CCSGameRules::sm_QueuedServerReservation.rankings().size(); ++ k )
				{
					if ( CCSGameRules::sm_QueuedServerReservation.rankings( k ).account_id() == pPlayer->GetHumanPlayerAccountID() )
					{
						m_iCompetitiveRanking.Set( i, CCSGameRules::sm_QueuedServerReservation.rankings( k ).rank_id() );
						m_iCompetitiveWins.Set( i, CCSGameRules::sm_QueuedServerReservation.rankings( k ).wins() );
						bSetValidRanking = true;
						break;
					}
				}
			}
		}
		else
		{
			m_iMVPs.Set( i, 0 );
			m_bHasDefuser.Set( i, false );
			m_bHasHelmet.Set( i, false );
			m_iArmor.Set( i, 0 );		
			m_szClan.Set( i, MAKE_STRING( "" ) );
			m_nEndMatchNextMapVotes.Set( i, -1 );
			m_nActiveCoinRank.Set( i, -1 );
			m_nMusicID.Set( i, -1 );
			m_bIsAssassinationTarget.Set( i, 0 );

			m_nPersonaDataPublicLevel.Set( i, -1 );
			m_nPersonaDataPublicCommendsLeader.Set( i, -1 );
			m_nPersonaDataPublicCommendsTeacher.Set( i, -1 );
			m_nPersonaDataPublicCommendsFriendly.Set( i, -1 );

			m_iCompTeammateColor.Set( i, -1 );
			m_nAttemptedToGetColor[i] = false;
		}

		if ( !bSetValidRanking )
		{
			m_iCompetitiveRanking.Set( i, 0 );
			m_iCompetitiveWins.Set( i, 0 );
		}
		m_iBotDifficulty.Set( i, botDifficulty );
	}

	if ( g_fGameOver && nTotalPlayingPlayers > 0 && CSGameRules() && CSGameRules()->IsEndMatchVotingForNextMap() )
	{
		if ( m_bEndMatchNextMapAllVoted == false )
		{
			// ignore whether all players voted and just return whether or not we ran out of time
			if ( CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_VoteTimeEnded ||
				CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_AllPlayersVoted ||
				CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_SelectingWinner ||
				CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_SettingNextLevel ||
				CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_VoteAllDone )
			{
				m_bEndMatchNextMapAllVoted = true;
			}
/*			m_bEndMatchNextMapAllVoted = (CSGameRules()->m_eEndMatchMapVoteState == CSGameRules()->k_EEndMatchMapVoteState_VoteTimeEnded);*/


// 			int nNumVotes = 0;
// 			int nHighestSingleVoteSlot = 0;
// 
// 			// this is done in three different places
// 			// TODO: make a function out of this
// 			int nVotes[MAX_ENDMATCH_VOTE_PANELS];
// 			for ( int i = 0; i < MAX_ENDMATCH_VOTE_PANELS; i++ )
// 				nVotes[i] = 0;
// 
// 			for ( int i = 1; i <= MAX_PLAYERS; i++ )
// 			{
// 				int nMapSlot = m_nEndMatchNextMapVotes[i];
// 				if ( nMapSlot != -1 && nMapSlot < MAX_ENDMATCH_VOTE_PANELS )
// 				{
// 					nVotes[nMapSlot] += 1;
// 					nNumVotes += 1;
// 				}
// 			}
// 			//////
// 
// 			for ( int i = 0; i < MAX_ENDMATCH_VOTE_PANELS + 1; i++ )
// 			{
// 				if ( nVotes[nHighestSingleVoteSlot] < nVotes[i] )
// 					nHighestSingleVoteSlot = i;
// 			}
// 
// 			if ( nTotalPlayingPlayers > 0 )
// 			{
// 				float flnVotesToSucceed = (float)nTotalPlayingPlayers * 0.501f;
// 				int nVotesToSucceed = ceil( flnVotesToSucceed );
// 
// 				if ( nNumVotes >= nTotalPlayingPlayers || nVotes[nHighestSingleVoteSlot] >= nVotesToSucceed || ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) ) )
// 					m_bEndMatchNextMapAllVoted = true;
// 			}
		}
	}
	else
	{
		// if we aren't at the end of the game or we don't have any players, no one has voted
		m_bEndMatchNextMapAllVoted = false;
	}

	int numHostages = g_Hostages.Count();

	for ( i = 0; i < MAX_HOSTAGES; i++ )
	{
		if ( i >= numHostages )
		{
//			engine->Con_NPrintf( i, "Dead" );
			m_bHostageAlive.Set( i, false );
			m_isHostageFollowingSomeone.Set( i, false );
			m_iHostageEntityIDs.Set( i, 0 );
			continue;
		}

		CHostage* pHostage = g_Hostages[i];

		m_bHostageAlive.Set( i, pHostage->IsRescuable() );

		if ( pHostage->IsValid() )
		{
			m_iHostageEntityIDs.Set( i, pHostage->entindex() );
			m_isHostageFollowingSomeone.Set( i, pHostage->IsFollowingSomeone() );
//			engine->Con_NPrintf( i, "ID:%d Pos:(%.0f,%.0f,%.0f)", pHostage->entindex(), pHostage->GetAbsOrigin().x, pHostage->GetAbsOrigin().y, pHostage->GetAbsOrigin().z );
		}
		else
		{
//			engine->Con_NPrintf( i, "Invalid" );
		}
	}

	if( !m_foundGoalPositions )
	{
		// We only need to update these once a map, but we need the client to know about them.
		CBaseEntity* ent = NULL;
		while ( ( ent = gEntList.FindEntityByClassname( ent, "func_bomb_target" ) ) != NULL )
		{
			const Vector &pos = ent->WorldSpaceCenter();
			CNavArea *area = TheNavMesh->GetNearestNavArea( pos, true, 10000.0f, false, false );
			const char *placeName = (area) ? TheNavMesh->PlaceToName( area->GetPlace() ) : NULL;
			if ( placeName == NULL )
			{
				// The bomb site has no area or place name, so just choose A then B
				if ( m_bombsiteCenterA.Get().IsZero() )
				{
					m_bombsiteCenterA = pos;
				}
				else
				{
					m_bombsiteCenterB = pos;
				}
			}
			else
			{
				// The bomb site has a place name, so choose accordingly
				if( FStrEq( placeName, "BombsiteA" ) || FStrEq( placeName, "Bombsite" ) )
				{
					m_bombsiteCenterA = pos;
				}
				else
				{
					m_bombsiteCenterB = pos;
				}
			}
			m_foundGoalPositions = true;
		}

		int hostageRescue = 0;
		while ( (( ent = gEntList.FindEntityByClassname( ent, "func_hostage_rescue" ) ) != NULL)  &&  (hostageRescue < MAX_HOSTAGE_RESCUES) )
		{
			const Vector &pos = ent->WorldSpaceCenter();
			m_hostageRescueX.Set( hostageRescue, (int) pos.x );	
			m_hostageRescueY.Set( hostageRescue, (int) pos.y );	
			m_hostageRescueZ.Set( hostageRescue, (int) pos.z );	

			hostageRescue++;
			m_foundGoalPositions = true;
		}
	}


#if CS_CONTROLLABLE_BOTS_ENABLED

	for ( int i=0; i < MAX_PLAYERS+1; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		bool bControllingBot = false;
		CCSPlayer *pControlledPlayer = NULL;
		CCSPlayer *pControlledByPlayer = NULL;

		if ( pPlayer && pPlayer->IsConnected() )
		{
			bControllingBot = pPlayer->IsControllingBot();
			pControlledPlayer = pPlayer->GetControlledBot();
			pControlledByPlayer = pPlayer->GetControlledByPlayer();
		}

		m_bControllingBot.Set( i, bControllingBot ? 1 : 0 );
		m_iControlledPlayer.Set( i, pControlledPlayer ? pControlledPlayer->entindex() : 0 );
		m_iControlledByPlayer.Set( i, pControlledByPlayer ? pControlledByPlayer->entindex() : 0 );

	}

#endif

	BaseClass::UpdatePlayerData();
}

void CCSPlayerResource::Spawn( void )
{
	m_iPlayerC4 = 0;
	m_iPlayerVIP = 0;
	m_bombsiteCenterA.Init();
	m_bombsiteCenterB.Init();
	m_foundGoalPositions = false;
	m_bPreferencesAssigned_CT = false;
	m_bPreferencesAssigned_T = false;
	memset( m_nAttemptedToGetColor, false, sizeof( m_nAttemptedToGetColor ) );

	for ( int i=0; i < MAX_HOSTAGES; i++ )
	{
		m_bHostageAlive.Set( i, 0 );
		m_isHostageFollowingSomeone.Set( i, 0 );
		m_iHostageEntityIDs.Set(i, 0);
	}

	for ( int i=0; i < MAX_HOSTAGE_RESCUES; i++ )
	{
		m_hostageRescueX.Set( i, 0 );
		m_hostageRescueY.Set( i, 0 );
		m_hostageRescueZ.Set( i, 0 );
	}

	for ( int i=0; i < MAX_PLAYERS+1; i++ )
	{
		m_iMVPs.Set( i, 0 );
		m_bHasDefuser.Set( i, false );
		m_bHasHelmet.Set( i, false );
		m_iArmor.Set( i, 0 );		
		m_iScore.Set( i, 0 );		
		m_iCompetitiveRanking.Set( i, 0 );
		m_iCompetitiveWins.Set( i, 0 );
		m_iCompTeammateColor.Set( i, -1 );
		m_iBotDifficulty.Set( i, -1 );
		m_szClan.Set( i, MAKE_STRING( "" ) );
		m_nActiveCoinRank.Set( i, -1 );
		m_nMusicID.Set( i, -1 );
		m_bIsAssassinationTarget.Set( i, 0 );

		m_nPersonaDataPublicLevel.Set( i, -1 );
		m_nPersonaDataPublicCommendsLeader.Set( i, -1 );
		m_nPersonaDataPublicCommendsTeacher.Set( i, -1 );
		m_nPersonaDataPublicCommendsFriendly.Set( i, -1 );
	}

	m_bEndMatchNextMapAllVoted = false;
	for ( int i=0; i < MAX_ENDMATCH_VOTE_PANELS+1; i++ )
	{
		m_nEndMatchNextMapVotes.Set( i, 0 );
	}

	BaseClass::Spawn();
}

const Vector CCSPlayerResource::GetBombsiteAPosition()
{
	return m_bombsiteCenterA;
}

const Vector CCSPlayerResource::GetBombsiteBPosition()
{
	return m_bombsiteCenterB;
}


const Vector CCSPlayerResource::GetHostageRescuePosition( int iIndex )
{
	if ( iIndex < 0 || iIndex >= MAX_HOSTAGE_RESCUES )
		return vec3_origin;

	Vector ret;

	ret.x = m_hostageRescueX[iIndex];
	ret.y = m_hostageRescueY[iIndex];
	ret.z = m_hostageRescueZ[iIndex];

	return ret;
}

//--------------------------------------------------------------------------------------------------------
int CCSPlayerResource::GetCompTeammateColor( int iIndex )
{
	CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( iIndex );
	if ( !pPlayer )
		return -1;

	if ( pPlayer->IsBot() )
		return -2;

	return m_iCompTeammateColor[iIndex];
}

void CCSPlayerResource::ResetPlayerTeammateColor( int index )
{
	CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( index );
	if ( !pPlayer )
		return;
		
	if ( CSGameRules() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && CSGameRules()->IsQueuedMatchmaking() )
		return;

	int nTeamNum = pPlayer->GetTeamNumber();
	if ( nTeamNum > TEAM_SPECTATOR )
	{
		SetPlayerTeammateColor( index, true );
		return;
	}

	m_iCompTeammateColor.Set( index, -1 );
}

void CCSPlayerResource::ForcePlayersPickColors()
{
	m_bPreferencesAssigned_CT = true;
	m_bPreferencesAssigned_T = true;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		m_nAttemptedToGetColor[i] = true;
}

void CCSPlayerResource::SetPlayerTeammateColor( int index, bool bReset )
{
	CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( index );
	if ( !pPlayer )
		return;

	m_nAttemptedToGetColor[index] = true;

	if ( !CSGameRules() || !CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
	{
		m_iCompTeammateColor.Set( index, -1 );
		return;
	}

 	if ( pPlayer->IsBot() )
 	{
 		m_iCompTeammateColor.Set( index, -2 );
 		return;
 	}

	int nTeamNum = pPlayer->GetTeamNumber();
	if ( nTeamNum > TEAM_SPECTATOR )
	{
		if ( CSGameRules() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
		{
			// check to see if we have a color already
			int idxThisPlayer = -1;

			// don't use the QMM code
			/*
			if ( CSGameRules()->IsQueuedMatchmaking() )
			{
				CCSPlayer *pThisPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( index );
				CSteamID steamID;
				pThisPlayer->GetSteamID( &steamID );
				int numTotalPlayers = 0;		
				static ConVarRef sv_mmqueue_reservation( "sv_mmqueue_reservation" );
				for ( char const *pszPrev = sv_mmqueue_reservation.GetString(), *pszNext = pszPrev;
					  ( pszNext = strchr( pszPrev, '[' ) ) != NULL; pszPrev = pszNext + 1 )
				{
					uint32 uiAccountId = 0;
					sscanf( pszNext, "[%x]", &uiAccountId );
					if ( uiAccountId && ( steamID.GetAccountID() == uiAccountId ) )
					{
						idxThisPlayer = numTotalPlayers;
					}
					++numTotalPlayers;
				}
			}
			*/

			// let all players have at least one crack at getting their prefered color before we start assigning loser colors
			if ( (nTeamNum == TEAM_TERRORIST && m_bPreferencesAssigned_T == false) ||
				 (nTeamNum == TEAM_CT && m_bPreferencesAssigned_CT == false) )
			{
				int nNumAttemptedToGetColor = 0;
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CCSPlayer *pOtherPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
					if ( pOtherPlayer && pOtherPlayer->GetTeamNumber() == pPlayer->GetTeamNumber() )
					{
						if ( m_nAttemptedToGetColor[i] == true )
							nNumAttemptedToGetColor++;

						if ( nNumAttemptedToGetColor >= 5 )
						{
							if ( nTeamNum == TEAM_TERRORIST )
								m_bPreferencesAssigned_T = true;
							else
								m_bPreferencesAssigned_CT = true;

							break;
						}
					}
				}
			}

			// Valve MM gives us the player index - does this work?
			if ( idxThisPlayer > -1 )
			{
				m_iCompTeammateColor.Set( index, ( idxThisPlayer % 5 ) );
			}
			else if ( m_iCompTeammateColor[index] == -1 || bReset )//otherwise we have to do it ourselves
			{
				int nPreferredColor = pPlayer->GetTeammatePreferredColor( );
				if ( nPreferredColor == -1 )
				{
					pPlayer->InitTeammatePreferredColor( );
					nPreferredColor = pPlayer->GetTeammatePreferredColor( );
				}	

				// we didn't initialize, so try again another time
				if ( nPreferredColor == -1 )
					return;

				int nAssignedColor = m_iCompTeammateColor[index] > -1 ? m_iCompTeammateColor[index] : nPreferredColor;
				bool bColorInUse = false;
				for ( int ii = 0; ii < 5; ii++ )
				{
					nAssignedColor = nAssignedColor % 5;

					bColorInUse = false;
					for ( int j = 1; j <= gpGlobals->maxClients; j++ )
					{
						CCSPlayer *pOtherPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( j );
						if ( pOtherPlayer && pOtherPlayer->GetTeamNumber( ) == pPlayer->GetTeamNumber( ) )
						{
							if ( nAssignedColor == m_iCompTeammateColor[j] && pOtherPlayer != pPlayer )
							{
								// All players should get a crack at getting their prefered color before a 
								// previously connected player crawls up the color scale and nabs it first
								if ( ( nTeamNum == TEAM_TERRORIST && m_bPreferencesAssigned_T == false) ||
									 ( nTeamNum == TEAM_CT && m_bPreferencesAssigned_CT == false ) )
									return;

								bColorInUse = true;
								nAssignedColor++;
								break;
							}
						}
					}

					if ( bColorInUse == false )
						break;
				}

				// somehow this failed
				AssertMsg( !bColorInUse, "Trying to assign a color to a teammate, but all colors are already in use!" );

				nAssignedColor = bColorInUse == false ? nAssignedColor : -1;
				m_iCompTeammateColor.Set( index, nAssignedColor );
			}
		}
		else
			m_iCompTeammateColor.Set( index, -1 );
	}
}

bool CCSPlayerResource::IsAssassinationTarget( int index ) const
{
	return m_bIsAssassinationTarget[ index ];
}


bool Helper_DoesPlayerHaveAssassinateQuestForTeam( const CCSPlayer *pPlayer, int iTeamNum )
{
	// If this player has an assassination quest targeting this team, prefer not to pick them as the target
	CEconQuestDefinition *pQuest = GetItemSchema()->GetQuestDefinition( pPlayer->Inventory()->GetActiveQuestID() );
	return ( pQuest && IsAssassinationQuest( pQuest ) && ( ( int ) pQuest->GetTargetTeam() == iTeamNum ) );
}


bool Helper_ValidateAssassinationTarget( const CCSPlayer *pCurrentAssassinationTarget, int iTeamNum )
{
	// Validate current assassination target, pick new one if needed
	if ( !pCurrentAssassinationTarget || !pCurrentAssassinationTarget->IsConnected() ||
		pCurrentAssassinationTarget->GetTeamNumber() != iTeamNum || pCurrentAssassinationTarget->IsDead() ||
		pCurrentAssassinationTarget->IsControllingBot() || Helper_DoesPlayerHaveAssassinateQuestForTeam( pCurrentAssassinationTarget, iTeamNum ) )
	{
		return false;
	}

	return true;
}

ConVar sv_assassination_target_ratio( "sv_assassination_target_ratio", "5" );
void CCSPlayerResource::UpdateAssassinationTargets( const CEconQuestDefinition * pQuest )
{
	CCSTeam *pTeam = GetGlobalCSTeam( pQuest->GetTargetTeam() );
	if ( !pTeam )
		return;

	// 1 out of X players is an assassination target, no less than 1 and more more than MAX_ASSASSINATION_TARGETS.
	CUtlVector<CCSPlayer*> vecCandiates;
	auto iTargetsNeeded = Min( Max( 1, pTeam->GetHumanMembers( &vecCandiates ) / Max( 1, sv_assassination_target_ratio.GetInt() ) ), 3 );

	CUtlVector< CCSPlayer* > vecNotIdealPlayers;
	FOR_EACH_VEC_BACK( vecCandiates, iter )
	{
		CCSPlayer* pCur = vecCandiates[ iter ];
		// Validate current assassination targets, remove from candidate list
		if ( pCur->IsAssassinationTarget() )
		{
			// Still valid, then reduce count of needed targets
			if ( Helper_ValidateAssassinationTarget( pCur, pQuest->GetTargetTeam() ) )
			{
				iTargetsNeeded--;
			}
			else
			{
				// Prefer not to pick recently invalidated players
				m_bIsAssassinationTarget.GetForModify( pCur->entindex() ) = false;
				vecNotIdealPlayers.AddToHead( pCur );
			}
			vecCandiates.Remove( iter );
		}
		else if ( Helper_DoesPlayerHaveAssassinateQuestForTeam( pCur, GetTeamNumber() ) )
		{
			// Prefer not to pick players with this quest 
			vecCandiates.Remove( iter );
			vecNotIdealPlayers.AddToTail( pCur );
		}
	}

	while ( iTargetsNeeded-- > 0 )
	{
		CUtlVector< CCSPlayer* > &vecBucket = vecCandiates.Count() > 0 ? vecCandiates : vecNotIdealPlayers;
		CCSPlayer *pTarget = vecBucket[ RandomInt( 0, vecBucket.Count() - 1 ) ];
		vecBucket.FindAndFastRemove( pTarget );
		m_bIsAssassinationTarget.GetForModify( pTarget->entindex() ) = true;
	}
}

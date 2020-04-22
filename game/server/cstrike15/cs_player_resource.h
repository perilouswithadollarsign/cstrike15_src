//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: CS's custom CPlayerResource
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_PLAYER_RESOURCE_H
#define CS_PLAYER_RESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "player_resource.h"

extern Vector g_vecDefuserPosition;
extern CBaseEntity* g_pDefuserEntity;

class CCSPlayerResource : public CPlayerResource
{
	DECLARE_CLASS( CCSPlayerResource, CPlayerResource );
	
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CCSPlayerResource();

	virtual void UpdatePlayerData( void );
	virtual void Spawn( void );

	const Vector	GetBombsiteAPosition();
	const Vector	GetBombsiteBPosition();

	const Vector	GetHostageRescuePosition( int index );

	bool			EndMatchNextMapAllVoted( void ) { return m_bEndMatchNextMapAllVoted; }

	int				GetCompTeammateColor( int iIndex );
	void			ResetPlayerTeammateColor( int index );
	void			ForcePlayersPickColors( void );
	void			SetPlayerTeammateColor( int index, bool bReset );

	bool			IsAssassinationTarget( int index ) const;
	void			UpdateAssassinationTargets( const CEconQuestDefinition * pQuest );

protected:

	CNetworkVar( int, m_iPlayerC4 );  // entity index of C4 carrier or 0
	CNetworkVar( int, m_iPlayerVIP ); // entity index of VIP player or 0
	CNetworkArray( bool, m_bHostageAlive, MAX_HOSTAGES );
	CNetworkArray( bool, m_isHostageFollowingSomeone, MAX_HOSTAGES );
	CNetworkArray( int, m_iHostageEntityIDs, MAX_HOSTAGES );
	CNetworkVector( m_bombsiteCenterA );// Location of bombsite A
	CNetworkVector( m_bombsiteCenterB );// Location of bombsite B
	CNetworkArray( int, m_hostageRescueX, MAX_HOSTAGE_RESCUES );// Locations of all hostage rescue spots
	CNetworkArray( int, m_hostageRescueY, MAX_HOSTAGE_RESCUES );
	CNetworkArray( int, m_hostageRescueZ, MAX_HOSTAGE_RESCUES );

	CNetworkArray( int, m_iMVPs, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_iArmor, MAX_PLAYERS + 1 );	
	CNetworkArray( bool, m_bHasDefuser, MAX_PLAYERS + 1);
	CNetworkArray( bool, m_bHasHelmet, MAX_PLAYERS + 1);
	CNetworkArray( int, m_iScore, MAX_PLAYERS + 1 );	
	CNetworkArray( int, m_iCompetitiveRanking, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_iCompetitiveWins, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_iCompTeammateColor, MAX_PLAYERS + 1 );

#if CS_CONTROLLABLE_BOTS_ENABLED
	CNetworkArray( int, m_bControllingBot, MAX_PLAYERS+1 );
	CNetworkArray( int, m_iControlledPlayer, MAX_PLAYERS+1 );
	CNetworkArray( int, m_iControlledByPlayer, MAX_PLAYERS+1 );
#endif
	CNetworkArray( int, m_iBotDifficulty, MAX_PLAYERS+1 );	// Difficulty level of a bot ( -1 if not applicable )
	CNetworkArray( string_t, m_szClan, MAX_PLAYERS+1 );
	CNetworkArray( int, m_iTotalCashSpent, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_iCashSpentThisRound, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_nEndMatchNextMapVotes, MAX_PLAYERS + 1 );
	CNetworkVar( bool, m_bEndMatchNextMapAllVoted );

	CNetworkArray( int, m_nActiveCoinRank, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_nMusicID, MAX_PLAYERS + 1 );
	CNetworkArray( bool, m_bIsAssassinationTarget, MAX_PLAYERS + 1 );
	
	CNetworkArray( int, m_nPersonaDataPublicLevel, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_nPersonaDataPublicCommendsLeader, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_nPersonaDataPublicCommendsTeacher, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_nPersonaDataPublicCommendsFriendly, MAX_PLAYERS + 1 );

	bool m_nAttemptedToGetColor[MAX_PLAYERS + 1];


private:
	bool m_foundGoalPositions;
	bool m_bPreferencesAssigned_CT;
	bool m_bPreferencesAssigned_T;
};

inline CCSPlayerResource* CSPlayerResource()
{
	return static_cast<CCSPlayerResource*>(g_pPlayerResource);
}

#endif // CS_PLAYER_RESOURCE_H

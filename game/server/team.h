//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Team management class. Contains all the details for a specific team
//
// $NoKeywords: $
//=============================================================================//

#ifndef TEAM_H
#define TEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"
#include "utlvector.h"
#include "cs_player.h"

class CBasePlayer;
class CTeamSpawnPoint;

class CTeam : public CBaseEntity
{
	DECLARE_CLASS( CTeam, CBaseEntity );
public:
	CTeam( void );
	virtual ~CTeam( void );

	DECLARE_SERVERCLASS();

	virtual void Precache( void ) { return; };

	virtual void Think( void );
	virtual int  UpdateTransmitState( void );

	//-----------------------------------------------------------------------------
	// Initialization
	//-----------------------------------------------------------------------------
	virtual void		Init( const char *pName, int iNumber );

	//-----------------------------------------------------------------------------
	// Data Handling
	//-----------------------------------------------------------------------------
	virtual int			GetTeamNumber( void ) const;
	virtual void		SetName( const char *pName );
	virtual const char *GetName( void );
	virtual void		UpdateClientData( CBasePlayer *pPlayer );
	virtual int		ShouldTransmitToPlayer( CBasePlayer* pRecipient, CBaseEntity* pEntity );
	virtual void		SetClanName( const char *pName );
	virtual const char *GetClanName( void );
	virtual void		SetClanID( uint32 iClanID );
	virtual uint32		GetClanID( void );
	virtual void		SetFlagImageString( const char *pName );
	virtual const char *GetFlagImageString( void );
	virtual void		SetLogoImageString( const char *pName );
	virtual const char *GetLogoImageString( void );
	virtual void		SetNumMapVictories( int numVictories );

	//-----------------------------------------------------------------------------
	// Spawnpoints
	//-----------------------------------------------------------------------------
	virtual void InitializeSpawnpoints( void );
	virtual void AddSpawnpoint( CTeamSpawnPoint *pSpawnpoint );
	virtual void RemoveSpawnpoint( CTeamSpawnPoint *pSpawnpoint );
	virtual CBaseEntity *SpawnPlayer( CBasePlayer *pPlayer );

	//-----------------------------------------------------------------------------
	// Players
	//-----------------------------------------------------------------------------
	virtual void InitializePlayers( void );
	virtual void AddPlayer( CBasePlayer *pPlayer );
	virtual void RemovePlayer( CBasePlayer *pPlayer );
	virtual int  GetNumPlayers( void );
	virtual CBasePlayer *GetPlayer( int iIndex );
	static int TeamGGSortFunction( CCSPlayer* const *entry1, CCSPlayer* const *entry2 );
	virtual void DetermineGGLeaderAndSort( void );
	virtual int GetGGLeader( int nTeam );

	//-----------------------------------------------------------------------------
	// Scoring
	//-----------------------------------------------------------------------------
	virtual void AddScore( int score )					{ m_scoreTotal += score; }
	virtual void AddScoreFirstHalf( int score )			{ m_scoreFirstHalf += score; }
	virtual void AddScoreSecondHalf( int score )		{ m_scoreSecondHalf += score; }
	virtual void AddScoreOvertime( int score )			{ m_scoreOvertime += score; }

	virtual void SetScore( int score )					{ m_scoreTotal = score; }
	virtual void SetScoreFirstHalf( int score )			{ m_scoreFirstHalf = score; }
	virtual void SetScoreSecondHalf( int score )		{ m_scoreSecondHalf = score; }
	virtual void SetScoreOvertime( int score )			{ m_scoreOvertime = score; }

	virtual int  GetScore( void )						{ return m_scoreTotal; }
	virtual int  GetScoreFirstHalf( void )				{ return m_scoreFirstHalf; }
	virtual int  GetScoreSecondHalf( void )				{ return m_scoreSecondHalf; }
	virtual int  GetScoreOvertime( void )				{ return m_scoreOvertime; }

	virtual void ResetScores( void );
	virtual void ResetTeamLeaders( void );

	void MarkSurrendered() { m_bSurrendered = 1; }

	void AwardAchievement( int iAchievement );

	virtual int GetAliveMembers( void );

#if defined ( CSTRIKE15 )
	virtual int GetBotMembers( CUtlVector< class CCSBot* > *pOutVecBots = NULL );
	virtual int GetHumanMembers( CUtlVector< class CCSPlayer* > *pOutVecPlayers = NULL );
#endif

	float m_flLastPlayerSortTime;
	static int m_nStaticGGLeader_CT;
	static int m_nStaticGGLeader_T;

	int m_nLastGGLeader_CT;
	int m_nLastGGLeader_T;

public:
	CUtlVector< CTeamSpawnPoint * > m_aSpawnPoints;
	CUtlVector< CBasePlayer * >		m_aPlayers;

	// Data
	CNetworkString( m_szTeamname, MAX_TEAM_NAME_LENGTH );
	CNetworkString( m_szClanTeamname, MAX_TEAM_NAME_LENGTH );
	CNetworkString( m_szTeamFlagImage, MAX_TEAM_FLAG_ICON_LENGTH );
	CNetworkString( m_szTeamLogoImage, MAX_TEAM_LOGO_ICON_LENGTH );
	CNetworkString( m_szTeamMatchStat, MAX_PATH );
	CNetworkVar( int, m_numMapVictories );
	CNetworkVar( uint32, m_iClanID );
	CNetworkVar( int, m_bSurrendered );
	CNetworkVar( int, m_scoreTotal );
	CNetworkVar( int, m_scoreFirstHalf );
	CNetworkVar( int, m_scoreSecondHalf );	
	CNetworkVar( int, m_scoreOvertime );
	int		m_iDeaths;

	CNetworkVar( int, m_nGGLeaderEntIndex_CT );
	CNetworkVar( int, m_nGGLeaderEntIndex_T );
	bool	m_bGGHasLeader_CT;
	bool	m_bGGHasLeader_T;

	// Spawnpoints
	int		m_iLastSpawn;		// Index of the last spawnpoint used

	CNetworkVar( int, m_iTeamNum );			// Which team is this?
};

extern CUtlVector< CTeam * > g_Teams;
extern CTeam *GetGlobalTeam( int iIndex );
extern int GetNumberOfTeams( void );
extern const char* GetTeamName( int iTeam );
#endif // TEAM_H

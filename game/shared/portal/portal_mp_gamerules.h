//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Portal multiplayer testing.
//
//=============================================================================//

#ifndef PORTAL_MP_GAMERULES_H
#define PORTAL_MP_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "gamerules.h"
//#include "hl2mp_gamerules.h"
//#include "multiplay_gamerules.h"
#include "teamplay_gamerules.h"

class CPortal_Player;

#ifdef CLIENT_DLL
#define CPortalMPGameRules C_PortalMPGameRules
#define CPortalMPGameRulesProxy C_PortalMPGameRulesProxy
#endif

// NOTE!!! YOU MUST UPDATE MACROS PropStringArrayArrayInnerList and PropStringArrayArrayOuterList IF YOU CHANGE THESE!!!
#define MAX_PORTAL2_COOP_LEVELS_PER_BRANCH	16  // max number of levels per branch
#define MAX_PORTAL2_COOP_BRANCHES			6   // max numbers or "branches" or paths in the coop campaign
#define MAX_PORTAL2_COOP_LEVEL_NAME_SIZE	64   // max numbers or "branches" or paths in the coop campaign
#define MAX_COOP_CREDITS_NAME_LENGTH		128

//for any code that needs updating, red was combine/terrorists, blue was rebels/counterterrorists
enum
{
	TEAM_RED = 2, 
	TEAM_BLUE,
};

enum Coop_CreditsState_t
{
	LIST_NAMES  = 0,
	SHOW_TEXT_BLOCK,
	LAST_CREDITSSTATE
};

enum Coop_Taunts
{
	TAUNT_HIGHFIVE  = 0,
	TAUNT_WAVE,
	TAUNT_RPS,
	TAUNT_LAUGH,
	TAUNT_ROBOTDANCE,
	TAUNT_CORETEASE,
	TAUNT_HUG,
	TAUNT_TRICKFIRE,
	MAX_PORTAL2_COOP_TAUNTS,
};

class CPortalMPGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CPortalMPGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();

	
#ifdef GAME_DLL
	DECLARE_DATADESC();
	void	InputAddRedTeamScore( inputdata_t &inputdata );
	void	InputAddBlueTeamScore( inputdata_t &inputdata );
#endif
};

class PortalMPViewVectors : public CViewVectors
{
public:
	PortalMPViewVectors( 
		Vector vView,
		Vector vHullMin,
		Vector vHullMax,
		Vector vDuckHullMin,
		Vector vDuckHullMax,
		Vector vDuckView,
		Vector vObsHullMin,
		Vector vObsHullMax,
		Vector vDeadViewHeight,
		Vector vCrouchTraceMin,
		Vector vCrouchTraceMax ) :
	CViewVectors( 
		vView,
		vHullMin,
		vHullMax,
		vDuckHullMin,
		vDuckHullMax,
		vDuckView,
		vObsHullMin,
		vObsHullMax,
		vDeadViewHeight )
	{
		m_vCrouchTraceMin = vCrouchTraceMin;
		m_vCrouchTraceMax = vCrouchTraceMax;
	}

	Vector m_vCrouchTraceMin;
	Vector m_vCrouchTraceMax;	
};

class CPortalMPGameRules : public CTeamplayRules
{
public:
	//DECLARE_CLASS( CPortalGameRules, CSingleplayRules );
	//DECLARE_CLASS( CPortalGameRules, CMultiplayRules );
	DECLARE_CLASS( CPortalMPGameRules, CTeamplayRules );

#ifdef CLIENT_DLL
	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.
#else
	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.
#endif

	CPortalMPGameRules( void );
	virtual ~CPortalMPGameRules( void );

	virtual void Precache( void );
	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );
	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void LevelInitPreEntity();

#if !defined ( CLIENT_DLL )
	virtual const char *GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer );
#endif

	virtual float FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon );
	virtual float FlWeaponTryRespawn( CBaseCombatWeapon *pWeapon );
	virtual Vector VecWeaponRespawnSpot( CBaseCombatWeapon *pWeapon );
	virtual int WeaponShouldRespawn( CBaseCombatWeapon *pWeapon );
	virtual void Think( void );
	virtual void CreateStandardEntities( void );
	virtual void ClientSettingsChanged( CBasePlayer *pPlayer );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual void GoToIntermission( void );
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual const char *GetGameDescription( void );
	// derive this function if you mod uses encrypted weapon info files
	virtual const unsigned char *GetEncryptionKey( void ) { return (unsigned char *)"x9Ke0BY7"; }
	virtual const CViewVectors* GetViewVectors() const;
	const PortalMPViewVectors* GetPortalMPViewVectors() const;
	void RunPlayerConditionThink( void );
	virtual void FrameUpdatePostEntityThink( void );

	virtual bool IsCoOp( void );
	bool Is2GunsCoOp( void );
	bool IsVS( void );

#ifdef CLIENT_DLL
	virtual bool IsChallengeMode();
#endif

	float GetMapRemainingTime();
	void CleanUpMap();
	void CheckRestartGame();
	void RestartGame();

#ifndef CLIENT_DLL
	virtual Vector VecItemRespawnSpot( CItem *pItem );
	virtual QAngle VecItemRespawnAngles( CItem *pItem );
	virtual float	FlItemRespawnTime( CItem *pItem );
	virtual bool	CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem );
	virtual bool FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual bool FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual void ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues );

	void	AddLevelDesignerPlacedObject( CBaseEntity *pEntity );
	void	RemoveLevelDesignerPlacedObject( CBaseEntity *pEntity );
	void	ManageObjectRelocation( void );

	virtual float	GetLaserTurretDamage( void );
	virtual float	GetLaserTurretMoveSpeed( void );
	virtual float	GetRocketTurretDamage( void );
	virtual float	FlPlayerFallDamage( CBasePlayer *pPlayer );

	virtual void InitDefaultAIRelationships();

	virtual void RegisterScriptFunctions();

	void	SetMapCompleteData( int nPlayer );
	bool	IsPlayerDataReceived( int nPlayer ) const { return m_bDataReceived[ nPlayer ]; }
	void	StartPlayerTransitionThinks( void );

	virtual void ClientDisconnected( edict_t *pClient );
#endif

	bool CheckGameOver( void );
	bool IsIntermission( void );

	void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );


	bool	IsTeamplay( void ) { return m_bTeamPlayEnabled;	}
	void	CheckAllPlayersReady( void );
	virtual bool ForceSplitScreenPlayersOnToSameTeam() { return false; }
	int		GetCoopSection( void ) { return m_nCoopSectionIndex; }
	int		GetCoopBranchLevel( int nBranch ) { return m_nCoopBranchIndex[nBranch]; }
	bool	IsAnyLevelComplete( void );
	bool	IsFullBranchComplete( int nBranch );
	bool	IsPlayerFullBranchComplete( int nPlayer, int nBranch );
	bool	IsLevelInBranchComplete( int nBranch, int nLevel );
	bool	IsPlayerLevelInBranchComplete( int nPlayer, int nBranch, int nLevel ) { return m_bLevelCompletions[ nPlayer ][ nBranch ][ nLevel ]; }
	const char *GetBranchLevelName( int nBranch = 0, int nLevel = 0 ) { return m_szLevelNames[nBranch][nLevel]; }
	int		GetBranchTotalLevelCount( int nBranch = 0 ) { return m_nLevelCount[nBranch]; }
	int		GetActiveBranches( void );
	int		GetSelectedDLCCourse( void );

	// CREDITS
	const char *GetCoopCreditsNameSingle() { return m_szCoopCreditsNameSingle; 	}
	const char *GetCoopCreditsJobTitle() { return m_szCoopCreditsJobTitle; 	}
	int		GetCoopCreditsNameIndex( void ) { return m_nCoopCreditsIndex; }
	int		GetCoopCreditsState( void ) { return m_nCoopCreditsState; }
	int		GetCoopCreditsScanState( void ) { return m_nCoopCreditsScanState; }
	bool	GetCoopCreditsFadeState( void ) { return m_bCoopFadeCreditsState; }

#ifndef CLIENT_DLL
	void	SaveMPStats( void );
	void	AddBranchLevel( int nBranch, const char *pchName );
	void	SetAllMapsComplete( bool bComplete = true, int nPlayer = -1 );
	void	SetBranchComplete( int nBranch, bool bComplete = true );
	void	SetMapComplete( const char *pchName, bool bComplete = true );
	void	SetMapCompleteSimple( int nPlayer, const char *pchName, bool bComplete );
	void	SendAllMapCompleteData( void );
	bool	SupressSpawnPortalgun( int nTeam );
	void	PlayerWinRPS( CBasePlayer* pWinnerPlayer );
	int		GetRPSOutcome( void ) { return m_nRPSOutcome; }
	void	ShuffleRPSOutcome( void ) { m_nRPSOutcome = RandomInt( 0, 4 ); }
	void	AddCreditsName( const char *pchName );
	void	SetGladosJustBlewUpBots( void ) { m_bGladosJustBlewUp = true; }
	int		GetLevelsCompletedThisBranch( void );
#endif

	void	SetMapComplete( int nPlayer, int nBranch, int nLevel, bool bComplete = true );
	bool	IsLobbyMap( void );
	bool	IsStartMap( void );
	bool	IsCreditsMap( void );
	bool	IsCommunityCoopHub( void );
	bool	IsCommunityCoop( void );
	
	void	PortalPlaced( void ) { m_nNumPortalsPlaced++; }
	int		GetNumPortalsPlaced( void ) { return m_nNumPortalsPlaced; }

#ifdef CLIENT_DLL
	void KeyValueBuilder( KeyValues *pKeyValues );
	void SaveMapCompleteData( void );
	void LoadMapCompleteData( void );

	bool IsClientCrossplayingPCvsPC() const { return m_bIsClientCrossplayingPCvsPC; }
#endif

private:

	CNetworkVar( bool, m_bTeamPlayEnabled );
	CNetworkVar( float, m_flGameStartTime );
	CUtlVector<EHANDLE> m_hRespawnableItemsAndWeapons;
	float m_tmNextPeriodicThink;
	float m_flRestartGameTime;
	bool m_bCompleteReset;
	bool m_bAwaitingReadyRestart;
	bool m_bGladosJustBlewUp;
	bool m_bHeardAllPlayersReady;
	bool m_bIsCoopInMapName;
	bool m_bIs2GunsInMapName;
	bool m_bIsVSInMapName;
	float m_fNextDLCSelectTime;
	CNetworkVar( int, m_nCoopSectionIndex );
	CNetworkArray( int, m_nCoopBranchIndex, MAX_PORTAL2_COOP_BRANCHES );
	CNetworkVar( int, m_nSelectedDLCCourse );
	CNetworkVar( int, m_nNumPortalsPlaced ); // Number of portals the players have placed so far this round.

	CNetworkVar( bool, m_bMapNamesLoaded );
	char m_szLevelNames[ MAX_PORTAL2_COOP_BRANCHES ][ MAX_PORTAL2_COOP_LEVELS_PER_BRANCH ][ MAX_PORTAL2_COOP_LEVEL_NAME_SIZE ];
	CNetworkArray( int, m_nLevelCount, MAX_PORTAL2_COOP_BRANCHES );

	CNetworkVar( bool, m_bCoopCreditsLoaded );
	CUtlVector< CUtlString > m_szCoopCreditsNames;
	CNetworkString( m_szCoopCreditsNameSingle, MAX_COOP_CREDITS_NAME_LENGTH );
	CNetworkString( m_szCoopCreditsJobTitle, MAX_COOP_CREDITS_NAME_LENGTH );
	CNetworkVar( int, m_nCoopCreditsIndex );
	CNetworkVar( int, m_nCoopCreditsState );
	CNetworkVar( int, m_nCoopCreditsScanState );
	CNetworkVar( bool, m_bCoopFadeCreditsState );

	bool m_bLevelCompletions[ 2 ][ MAX_PORTAL2_COOP_BRANCHES ][ MAX_PORTAL2_COOP_LEVELS_PER_BRANCH ];

#ifndef CLIENT_DLL
	bool m_bDataReceived[ 2 ];

	int m_nRPSWinCount[ 2 ];
	int m_nRPSOutcome;
#else
	bool m_bIsClientCrossplayingPCvsPC;
#endif
};


//-----------------------------------------------------------------------------
// Gets us at the Half-Life 2 game rules
//-----------------------------------------------------------------------------
inline CPortalMPGameRules* PortalMPGameRules()
{
	extern CPortalMPGameRules *g_pPortalMPGameRules;
	return g_pPortalMPGameRules;
}

bool IsLocalSplitScreen( void );

#ifdef CLIENT_DLL
	bool ClientIsCrossplayingWithConsole( void );
#endif


#endif // PORTAL_MP_GAMERULES_H

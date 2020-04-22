//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The CS game stats header
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_GAMESTATS_H
#define CS_GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "cs_blackmarket.h"
#include "GameStats.h"
#include "cs_gamestats_shared.h"
#include "GameEventListener.h"
#include "weapon_csbase.h"
#include "steamworks_gamestats_server.h"

// forward declares
class CBreakableProp;

const float cDisseminationTimeHigh = 0.25f;		// Time interval for high priority stats sent to the player
const float cDisseminationTimeLow = 2.5f;	// Time interval for medium priority stats sent to the player

#define		BULLET_SUB_GROUP_MASK	0xF0000000
#define		BULLET_ID_GROUP_MASK	0x000FFFFF
#define		BULLET_RECOIL_MASK		0x0FF00000
#define		RECOIL_BIT_SHIFT(val) (val<<20)
#define		SUB_BULLET_BIT_SHIFT(val) 	(val << 28 )

//Helper enum table and conversion function to simplify bomb data recording. Update whenever new bomb-related events become relevant
enum CSBombEventName
{
	BOMB_EVENT_NAME_NONE = 0,						// 0 is an unknown event

	BOMB_EVENT_NAME_FIRST,
	BOMB_EVENT_NAME_PLANTED = BOMB_EVENT_NAME_FIRST,
	BOMB_EVENT_NAME_DEFUSED,

	BOMB_EVENT_NAME_MAX = BOMB_EVENT_NAME_DEFUSED,	// number of BOMB_EVENT_NAMES
};
CSBombEventName BombEventNameFromString( const char* pEventName );

int GetCSLevelIndex( const char *pLevelName );

#if !defined( NO_STEAM )
uint32 GetPlayerID( CCSPlayer *pPlayer );
#endif

typedef struct
{
	char szGameName[8];
	byte iVersion;
	char szMapName[32];
	char ipAddr[4];
	short port;
	int serverid;
} gamestats_header_t;

typedef struct
{
	gamestats_header_t	header;
	short	iMinutesPlayed;

	short	iTerroristVictories[CS_NUM_LEVELS];
	short	iCounterTVictories[CS_NUM_LEVELS];
	short	iBlackMarketPurchases[WEAPON_MAX];

	short	iAutoBuyPurchases;
	short	iReBuyPurchases;
	short	iAutoBuyM4A1Purchases;
	short	iAutoBuyAK47Purchases;
	short	iAutoBuyFamasPurchases;
	short	iAutoBuyGalilPurchases;
	short	iAutoBuyGalilARPurchases;
	short	iAutoBuyVestHelmPurchases;
	short	iAutoBuyVestPurchases;

} cs_gamestats_t;



struct WeaponStats
{
	int shots;
	int hits;
	int kills;
	int damage;
};

static byte PackVelocityComponent( float f )
{
	// Gets velocity components and sticks fits them into a byte by scaling them
	// and clamping them
	f = f / 2.0f;
	if ( f > 127 ) f = 127;
	if ( f < -128 ) f = -128;
	return (byte) f;
}

static byte PackMovementComponent( bool bDucking, bool bInAir)
{
	return (((byte)bDucking) << 1) | (byte)bInAir;
}

static uint32 PackMovementStatInternal( Vector vVelocity, bool bDucking, bool bInAir )
{
	return ( (uint32)PackVelocityComponent( vVelocity.x ) << 24 ) |
		( (uint32)PackVelocityComponent( vVelocity.y ) << 16 ) |
		( (uint32)PackVelocityComponent( vVelocity.z ) << 8 ) |
		( (uint32)PackMovementComponent( bDucking, bInAir ) );
}

static uint32 PackPlayerMovementStat( CCSPlayer *pPlayer )
{
	Vector vAttackerVelocity;
	pPlayer->GetVelocity( &vAttackerVelocity, NULL );
	bool bInAir = FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) ? false : true;
	bool bDucking = FBitSet( pPlayer->GetFlags(), FL_DUCKING ) ? true : false;
	return PackMovementStatInternal( vAttackerVelocity, bDucking, bInAir );
}

// Pack and convert Target Aim Angle component into 32bits
// OGS
static uint8 PackAimAngleComponent( float f )
{
	// Pack component of QAngle into Byte. Angle is -180 -> 180, so there's no space for the full range. 

	// Make sure we're not getting weird values, rather have dumb data than broken game
	if ( f > 180 ) f = 180;
	if ( f < -180 ) f = -180;

	// The greatest granularity comes from packing angle into ( 2 * Pi )/255 units. 
	// Effectively that's just a shift and stretch of the range: ( ( f + 180 )/ 360 ) * 255;
	return (uint8) ( ( ( f + 180 ) / 360 ) * 255 );
}

static uint32 PackAimAngleStat( CCSPlayer *pPlayer )
{
	QAngle vPlayerAimAngle = pPlayer->GetFinalAimAngle();
	return ( (uint32)PackAimAngleComponent( (float)vPlayerAimAngle.z ) << 16 |
		     (uint32)PackAimAngleComponent( (float)vPlayerAimAngle.y ) << 8  |
			 (uint32)PackAimAngleComponent( (float)vPlayerAimAngle.x ) 
		   );	   
}


//=============================================================================
//
// OGS Gamestats
//
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
struct SWeaponShotData : public BaseStatData 
{
	SWeaponShotData( CCSPlayer *pPlayer, CWeaponCSBase* pWeapon, uint8 subBullet, uint8 round, uint8 iRecoilIndex )
	{
		Clear();

		if ( pWeapon )
		{			
			m_ui8WeaponID = (uint8)pWeapon->GetEconItemView()->GetItemIndex();
		}

		if ( pPlayer )
		{
			m_iUserID = GetPlayerID( pPlayer );
			m_uiBulletID = pPlayer->GetBulletGroup();
			m_vAttackerPos = pPlayer->GetAbsOrigin();
			m_uAttackerMovement = PackPlayerMovementStat( pPlayer );
		}

		m_uiSubBulletID = subBullet;
		m_RoundID = round;
		m_uiRecoilIndex = iRecoilIndex;
	}

	void Clear()
	{
		m_iUserID = 0;
		m_WeaponID = WEAPON_NONE;
		m_ui8WeaponID = 0;
		m_uiBulletID = 0;
		m_uiSubBulletID = 0;
		m_vAttackerPos.Init();		
		m_uAttackerMovement = 0;
		m_RoundID = 0;
		m_uiRecoilIndex = 0;
	}

	int			m_iUserID;
	CSWeaponID	m_WeaponID;
	uint8       m_ui8WeaponID;
	uint32		m_uiBulletID;
	uint8		m_uiSubBulletID;
	uint8		m_uiRecoilIndex;
	Vector		m_vAttackerPos;
	uint32		m_uAttackerMovement;
	uint8		m_RoundID;
};

struct SWeaponHitData : public BaseStatData 
{
	SWeaponHitData( CCSPlayer *pCSTarget, const CTakeDamageInfo &info, uint8 subBullet, uint8 round, uint8 iRecoilIndex );

	SWeaponHitData()
	{
		Clear();
	}

	// When any grenade explodes-- this is separate from any damage it may deal, which are also recorded as hits.
	// Can fail! check the return!
	bool InitAsGrenadeDetonation( class CBaseCSGrenadeProjectile *pGrenade, uint32 unBulletGroup );
	
	// When a bomb is planted or defused, we want to collect data from each alive player, reporting their locations
	bool InitAsBombEvent( CCSPlayer *pCSPlayer, class CPlantedC4 *pPlantedC4, uint32 unBulletGroup, uint8 nBombsite, CSBombEventName nBombEventName );
	
	void Clear()
	{
		m_uiBulletID = 0;
		m_uiSubBulletID = 0;
		m_vAttackerPos.Init();
		m_vTargetPos.Init();
		m_uiDamage = 0;
		m_HitRegion = 0;
		m_RoundID = 0;
		m_ui8WeaponID = 0;
		m_ui64TargertID = 0;
		m_ui64AttackerID = 0;
		m_ui8Health = 0;
		m_uAttackerMovement = 0;
		m_uiRecoilIndex = 0;
	}

	void CompactBulletID()
	{
		m_uiBulletID = (m_uiBulletID & BULLET_ID_GROUP_MASK) | (SUB_BULLET_BIT_SHIFT(m_uiSubBulletID) & BULLET_SUB_GROUP_MASK) | (RECOIL_BIT_SHIFT(MIN(m_uiRecoilIndex, 255)) & BULLET_RECOIL_MASK);
	}

	// Data that gets sent to OGS
	uint32	m_uiBulletID;
	uint8	m_uiSubBulletID;
	uint8	m_uiRecoilIndex;
	Vector	m_vAttackerPos;
	Vector	m_vTargetPos;
	uint16	m_uiDamage;
	uint8	m_HitRegion;
	uint8	m_RoundID;
	
	uint8	m_ui8Health;
	uint8	m_ui8WeaponID;
	uint32	m_ui64AttackerID;
	uint32	m_ui64TargertID;
	uint32  m_uAttackerMovement;

	BEGIN_STAT_TABLE( "CSGOWeaponHitData" )
		REGISTER_STAT_NAMED( m_ui8WeaponID, "WeaponID" )
		REGISTER_STAT_NAMED( m_uiBulletID, "BulletID" )
		REGISTER_STAT_NAMED( m_ui64AttackerID, "AttackerID" )
		REGISTER_STAT_NAMED( (int)m_vAttackerPos.x, "AttackerX" )
		REGISTER_STAT_NAMED( (int)m_vAttackerPos.y, "AttackerY" )
		REGISTER_STAT_NAMED( (int)m_vAttackerPos.z, "AttackerZ" )
		REGISTER_STAT_NAMED( (int)m_uAttackerMovement, "AttackerMovement" )
		REGISTER_STAT_NAMED( m_ui64TargertID, "TargetID" )
		REGISTER_STAT_NAMED( (int)m_vTargetPos.x, "TargetX" )
		REGISTER_STAT_NAMED( (int)m_vTargetPos.y, "TargetY" )
		REGISTER_STAT_NAMED( (int)m_vTargetPos.z, "TargetZ" )
		REGISTER_STAT_NAMED( m_ui8Health, "Health" )
		REGISTER_STAT_NAMED( m_uiDamage, "Damage" )
		REGISTER_STAT_NAMED( m_HitRegion, "HitRegion" )
		REGISTER_STAT_NAMED( m_RoundID, "Round" )
	END_STAT_TABLE()

};

struct SWeaponMissData : public BaseStatData 
{
	SWeaponMissData( SWeaponShotData *data ) 
	{
		Clear();

		if ( data )
		{
			m_ui64AttackerID = data->m_iUserID;
			m_ui8WeaponID = data->m_ui8WeaponID;//data->m_WeaponID;
			m_uiBulletID = data->m_uiBulletID;
			m_uiRecoilIndex = data->m_uiRecoilIndex;
			m_uiSubBulletID = data->m_uiSubBulletID;
			m_vAttackerPos = data->m_vAttackerPos;
			m_uAttackerMovement = data->m_uAttackerMovement;
			m_RoundID = data->m_RoundID;

			TimeSubmitted = data->TimeSubmitted;
		}
	}

	void Clear()
	{
		m_ui8WeaponID = 0;
		m_uiBulletID = 0;
		m_uiSubBulletID = 0;
		m_uiRecoilIndex = 0;
		m_ui64AttackerID = 0;
		m_vAttackerPos.Init();
		m_uAttackerMovement = 0;
		m_RoundID = 0;		
	}

	void CompactBulletID()
	{
		m_uiBulletID = (m_uiBulletID & BULLET_ID_GROUP_MASK) | (SUB_BULLET_BIT_SHIFT(m_uiSubBulletID) & BULLET_SUB_GROUP_MASK) | (RECOIL_BIT_SHIFT(MIN(m_uiRecoilIndex, 255)) & BULLET_RECOIL_MASK);
	}

	uint8	m_ui8WeaponID;
	uint32	m_uiBulletID;
	uint8	m_uiSubBulletID;
	uint8	m_uiRecoilIndex;
	uint32	m_ui64AttackerID;
	Vector	m_vAttackerPos;
	uint32	m_uAttackerMovement;
	uint8	m_RoundID;


	BEGIN_STAT_TABLE( "CSGOWeaponMissData" )
		REGISTER_STAT_NAMED( m_ui8WeaponID, "WeaponID" )
		REGISTER_STAT_NAMED( m_uiBulletID, "BulletID" )
		REGISTER_STAT_NAMED( m_ui64AttackerID, "AttackerID" )
		REGISTER_STAT_NAMED( (int)m_vAttackerPos.x, "AttackerX" )
		REGISTER_STAT_NAMED( (int)m_vAttackerPos.y, "AttackerY" )
		REGISTER_STAT_NAMED( (int)m_vAttackerPos.z, "AttackerZ" )
		REGISTER_STAT_NAMED( (int)m_uAttackerMovement, "AttackerMovement" )
		REGISTER_STAT_NAMED( m_RoundID, "Round" )
	END_STAT_TABLE()

};

struct SMarketPurchases : public BaseStatData 
{
	SMarketPurchases( uint64 ulPlayerID, int iPrice, const char *pName, int round ) : ItemCost(iPrice)
	{
		m_nPlayerID = ulPlayerID;
		
		if ( pName )
		{
			//Can we find a valid Item Definition?
			if ( GetItemSchema()->GetItemDefinitionByName( pName ) )
			{				
				ItemID = (uint)GetItemSchema()->GetItemDefinitionByName( pName )->GetDefinitionIndex();				
			}
			else
			{
				//We don't know about you, you're probably equipment (armor, defuse kit)
				//Use CSWeaponID instead (works for all equipment)
				ItemID = WeaponIdFromString( pName ); 	 //Returns WEAPON_NONE on failure to find string pName
			}			
		}
		else
		{
			   ItemID = WEAPON_NONE;
		}

		// Can't buy 'none'. Investigate why we can't get a weapon ID from the given string. 
		Assert( ItemID != WEAPON_NONE );

		m_iPurchaseCnt = 1;
		m_niRound = round;
	}
	uint32	m_nPlayerID;
	int		ItemCost;
	uint	ItemID;
	char	m_iPurchaseCnt;
	int		m_niRound;

	BEGIN_STAT_TABLE( "CSGOMarketPurchase" )
		REGISTER_STAT_NAMED( m_nPlayerID, "AccountID" )
		REGISTER_STAT( ItemCost )
		REGISTER_STAT_NAMED( m_iPurchaseCnt, "PurchaseCount" )
		REGISTER_STAT_NAMED( ItemID, "WeaponID" )
		REGISTER_STAT_NAMED( m_niRound, "Round" )
		END_STAT_TABLE()
};

typedef CUtlVector< SWeaponHitData* > CSGOWeaponHitData;
typedef CUtlVector< SWeaponMissData* > CSGOWeaponMissData;
typedef CUtlVector< SWeaponShotData* > CSGOWeaponShotsData;
typedef CUtlVector< SMarketPurchases* > CSGOMarketPurchaseData;

#endif // OGS Data

//=============================================================================
//
// CS Game Stats Class
//
class CCSGameStats : public CBaseGameStats, public CGameEventListener, public CAutoGameSystemPerFrame
#if !defined( _GAMECONSOLE )
, public IGameStatTracker
#endif
{
public:

	// Constructor/Destructor.
	CCSGameStats( void );
	~CCSGameStats( void );

	virtual void Clear( void );
	virtual bool Init();
	virtual void PreClientUpdate();
	
	// Overridden events
	virtual void Event_LevelInit( void );
	virtual void Event_LevelShutdown( float flElapsed );
	virtual void Event_ShotFired( CBasePlayer *pPlayer, CBaseCombatWeapon* pWeapon );
	virtual void Event_ShotHit( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilled_PreWeaponDrop( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerConnected( CBasePlayer *pPlayer );
	virtual void Event_PlayerDisconnected( CBasePlayer *pPlayer );
	virtual void Event_WindowShattered( CBasePlayer *pPlayer );
	virtual void Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info );

	// CSS specific events
    void Event_BombPlanted( CCSPlayer *pPlayer );
    void Event_BombDefused( CCSPlayer *pPlayer );
	void Event_BombExploded( CCSPlayer *pPlayer );
	void Event_MoneyEarned( CCSPlayer *pPlayer, int moneyEarned );
	void Event_MoneySpent( CCSPlayer* pPlayer, int moneySpent, const char *pItemName );
    void Event_HostageRescued( CCSPlayer *pPlayer );
    void Event_PlayerSprayedDecal( CCSPlayer*pPlayer );
	void Event_AllHostagesRescued();
	void Event_BreakProp( CCSPlayer *pPlayer, CBreakableProp *pProp );
	void Event_PlayerDonatedWeapon (CCSPlayer* pPlayer);
	void Event_PlayerDominatedOther( CCSPlayer* pAttacker, CCSPlayer* pVictim);
	void Event_PlayerRevenge( CCSPlayer* pAttacker );
	void Event_PlayerAvengedTeammate( CCSPlayer* pAttacker, CCSPlayer* pAvengedPlayer );
	void Event_MVPEarned( CCSPlayer* pPlayer );	
	void Event_KnifeUse( CCSPlayer* pPlayer, bool bStab, int iDamage );

	void RecordWeaponHit( SWeaponHitData* pHitData );
	
	// Steamworks Gamestats
#if !defined( _GAMECONSOLE )
	void UploadRoundStats( void );
	virtual void SubmitGameStats( KeyValues *pKV );
	virtual StatContainerList_t* GetStatContainerList( void );
	bool AnyOGSDataToSubmit( void );
#endif

	virtual void FireGameEvent( IGameEvent *event );

	void UpdatePlayerRoundStats(int winner);
	void DumpMatchWeaponMetrics();

	const PlayerStats_t&		FindPlayerStats( CBasePlayer *pPlayer ) const;
	void						ResetPlayerStats( CBasePlayer *pPlayer );
	void						ResetKillHistory( CBasePlayer *pPlayer );
	void						ResetRoundStats();
	void						ResetPlayerClassMatchStats();
	void						ClearOGSRoundStats();

	const StatsCollection_t&	GetTeamStats( int iTeamIndex ) const;
	void						ResetAllTeamStats();
	void						ResetAllStats();
	void						ResetWeaponStats();
 	void						IncrementTeamStat( int iTeamIndex, int iStatIndex, int iAmount );
	void                        CalcDominationAndRevenge( CCSPlayer *pAttacker, CCSPlayer *pVictim, int *piDeathFlags );
    void                        CalculateOverkill(CCSPlayer* pAttacker, CCSPlayer* pVictim);
	void						PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );

	void						IncrementStat( CCSPlayer* pPlayer, CSStatType_t statId, int iValue, bool bPlayerOnly = false, bool bIncludeBotController = false );

	void						SendStatsToPlayer( CCSPlayer * pPlayer, int iMinStatPriority );
	void						CreateNewGameStatsSession( void );

protected:	
	void						SetStat( CCSPlayer *pPlayer, CSStatType_t statId, int iValue );
	void						TrackKillStats( CCSPlayer *pAttacker, CCSPlayer *pVictim );

private:
	PlayerStats_t				m_aPlayerStats[MAX_PLAYERS+1];	// List of stats for each player for current life - reset after each death    
	StatsCollection_t			m_aTeamStats[TEAM_MAXCOUNT - FIRST_GAME_TEAM];

	float						m_fDisseminationTimerLow;		// how long since last medium priority stat update
	float						m_fDisseminationTimerHigh;		// how long since last high priority stat update

	int							m_numberOfRoundsForDirectAverages;
	int							m_numberOfTerroristEntriesForDirectAverages;
	int							m_numberOfCounterTerroristEntriesForDirectAverages;

	CUtlDict< CSStatType_t, short >	m_PropStatTable;

	WeaponStats					m_weaponStats[WEAPON_MAX][WeaponMode_MAX];

	// Steamworks Gamestats
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	CSGOWeaponHitData			m_WeaponHitData;
	CSGOWeaponMissData			m_WeaponMissData;
	CSGOWeaponShotsData			m_WeaponShotData;
	CSGOMarketPurchaseData		m_MarketPurchases;

	// A static list of all the stat containers, one for each data structure being tracked
	static StatContainerList_t * s_StatLists;
#endif	
};

extern CCSGameStats CCS_GameStats;

#endif // CS_GAMESTATS_H

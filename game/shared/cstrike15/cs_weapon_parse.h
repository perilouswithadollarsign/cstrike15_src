//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CS_WEAPON_PARSE_H
#define CS_WEAPON_PARSE_H
#ifdef _WIN32
#pragma once
#endif


#ifdef CLIENT_DLL
	#define CWeaponCSBase C_WeaponCSBase
#endif

#include "weapon_parse.h"
#include "econ_item_constants.h"


class CWeaponCSBase;

//--------------------------------------------------------------------------------------------------------
enum CSWeaponType
{
	WEAPONTYPE_KNIFE=0,	
	WEAPONTYPE_PISTOL,
	WEAPONTYPE_SUBMACHINEGUN,
	WEAPONTYPE_RIFLE,
	WEAPONTYPE_SHOTGUN,
	WEAPONTYPE_SNIPER_RIFLE,
	WEAPONTYPE_MACHINEGUN,
	WEAPONTYPE_C4,
	WEAPONTYPE_GRENADE,
	WEAPONTYPE_EQUIPMENT,
	WEAPONTYPE_STACKABLEITEM,
	WEAPONTYPE_UNKNOWN
};


/*
	NOTE!!!! Please try not to change the order. If you need to add something, please add it to the end.
	Anytime the order of the weaponID's change, we need to updated a bunch of tables in a couple DB's. Also,
	changing the order can invalidate saved queries and in general makes using the OGS stats difficult.
*/
//--------------------------------------------------------------------------------------------------------
enum CSWeaponID
{
	WEAPON_NONE = 0,

	WEAPON_FIRST,

	WEAPON_DEAGLE = WEAPON_FIRST,
	WEAPON_ELITE,
	WEAPON_FIVESEVEN,
	WEAPON_GLOCK,
	WEAPON_P228,
	WEAPON_USP,

	WEAPON_AK47,
	WEAPON_AUG,
	WEAPON_AWP,
	WEAPON_FAMAS,
	WEAPON_G3SG1,
	WEAPON_GALIL,
	WEAPON_GALILAR,
	WEAPON_M249,
	WEAPON_M3,
	WEAPON_M4A1,
	WEAPON_MAC10,
	WEAPON_MP5NAVY,
	WEAPON_P90,
	WEAPON_SCOUT,
	WEAPON_SG550,
	WEAPON_SG552,
	WEAPON_TMP,
	WEAPON_UMP45,
	WEAPON_XM1014,

	WEAPON_BIZON,
	WEAPON_MAG7,
	WEAPON_NEGEV,
	WEAPON_SAWEDOFF,
	WEAPON_TEC9,
	WEAPON_TASER,

	WEAPON_HKP2000,
	WEAPON_MP7,
	WEAPON_MP9,
	WEAPON_NOVA,
	WEAPON_P250,
	WEAPON_SCAR17,
	WEAPON_SCAR20,
	WEAPON_SG556,
	WEAPON_SSG08,
	WEAPON_LAST = WEAPON_SSG08,

	ITEM_FIRST,
	WEAPON_KNIFE_GG = ITEM_FIRST,
	WEAPON_KNIFE,

	WEAPON_FLASHBANG,
	WEAPON_HEGRENADE,
	WEAPON_SMOKEGRENADE,
	WEAPON_MOLOTOV,
	WEAPON_DECOY,
	WEAPON_INCGRENADE,
	WEAPON_TAGRENADE,
	WEAPON_C4,
	ITEM_MAX = WEAPON_C4,

	EQUIPMENT_FIRST,
	ITEM_KEVLAR = EQUIPMENT_FIRST,
	ITEM_ASSAULTSUIT,
	ITEM_HEAVYASSAULTSUIT,
	ITEM_NVG,
	ITEM_DEFUSER,
	ITEM_CUTTERS,
	EQUIPMENT_MAX,

	WEAPON_HEALTHSHOT,

	WEAPON_MAX = EQUIPMENT_MAX,		// number of weapons weapon index
};

#define MAX_EQUIPMENT (EQUIPMENT_MAX - EQUIPMENT_FIRST)

enum
{
	ITEM_PRICE_KEVLAR = 650,
	ITEM_PRICE_HELMET = 350,
	ITEM_PRICE_ASSAULTSUIT = ITEM_PRICE_KEVLAR + ITEM_PRICE_HELMET,
	ITEM_PRICE_HEAVYASSAULTSUIT = 1250,
	ITEM_PRICE_DEFUSEKIT = 400,
	ITEM_PRICE_NVG = 1250,
};


struct WeaponPaintableMaterial_t;


void PrepareEquipmentInfo( void );

class WeaponRecoilData
{
public:

	WeaponRecoilData();
	~WeaponRecoilData();

	void GetRecoilOffsets( CWeaponCSBase *pWeapon, int iMode, int iIndex, float& fAngle, float &fMagnitude );
	void GenerateRecoilPatternForItemDefinition( item_definition_index_t idx );

private:

	struct RecoilOffset
	{
		float	fAngle;
		float	fMagnitude;
	};

	struct RecoilData
	{
		item_definition_index_t		iItemDefIndex;
		RecoilOffset				recoilTable[2][64];
	};

	CUtlMap< item_definition_index_t, RecoilData* > m_mapRecoilTables;

	void GenerateRecoilTable( RecoilData *data );

};

//--------------------------------------------------------------------------------------------------------
class CCSWeaponInfo : public FileWeaponInfo_t
{
public:
	DECLARE_CLASS_GAMEROOT( CCSWeaponInfo, FileWeaponInfo_t );
	
	CCSWeaponInfo();
	
	virtual void Parse( ::KeyValues *pKeyValuesData, const char *szWeaponName );

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	virtual void RefreshDynamicParameters() { GenerateRecoilTable(); }

	const char *GetZoomInSound( void ) const { return m_szZoomINSound; }
	const char *GetZoomOutSound( void ) const { return m_szZoomOUTSound; }

	const Vector& GetSmokeColor() const { return m_vSmokeColor; }

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	void GetRecoilOffsets( int iMode, int iIndex, float& fAngle, float &fMagnitude ) const;

protected:

	CSWeaponType m_WeaponType;
	int m_iTeam;				// Which team can have this weapon. TEAM_UNASSIGNED if both can have it.

public:

	CSWeaponID	m_weaponId;




	int		GetWeaponPrice					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	bool	IsFullAuto						( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	bool	HasSilencer						( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetBullets						( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetCycleTime					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetHeatPerShot					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoveryTimeCrouch			( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoveryTimeStand			( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoveryTimeCrouchFinal		( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	float	GetRecoveryTimeStandFinal		( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetRecoveryTransitionStartBullet( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetRecoveryTransitionEndBullet	( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;

	int		GetRecoilSeed					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetFlinchVelocityModifierLarge	( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetFlinchVelocityModifierSmall	( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetTimeToIdleAfterFire			( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetIdleInterval					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRange						( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRangeModifier				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetDamage						( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetPenetration					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetCrosshairDeltaDistance		( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetCrosshairMinDistance			( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 

	float	GetInaccuracyAltSwitch			( void ) const { return m_fInaccuracyAltSwitch; };

	float	GetInaccuracyPitchShift			( void ) const { return m_fInaccuracyPitchShift;  }
	float	GetInaccuracyAltSoundThreshhold ( void ) const { return m_fInaccuracyAltSoundThreshold; }

	float	GetMaxSpeed						( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 

	float	GetSpread						( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyCrouch				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyStand				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 0.001f ) const;
	float   GetInaccuracyJumpInitial		( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyJump				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyLand				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyLadder				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyFire				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 0.001f ) const;
	float	GetInaccuracyMove				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 0.001f ) const;

	float	GetInaccuracyReload				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 0.001f ) const;

	float	GetRecoilAngle					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoilAngleVariance			( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoilMagnitude				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	float	GetRecoilMagnitudeVariance		( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetTracerFrequency				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 

	int		GetPrimaryClipSize				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetSecondaryClipSize			( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const; 
	int		GetDefaultPrimaryClipSize		( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetDefaultSecondaryClipSize		( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;

	int		GetKillAward					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	bool	HasBurstMode					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	bool	IsRevolver						( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	bool	HasAlternateFastSlowReload		( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	float	GetArmorRatio					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	bool	HasTraditionalScope				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	bool	CannotShootUnderwater			( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	bool	DoesUnzoomAfterShot				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	bool	DoesHideViewModelWhenZoomed		( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetBucketSlot					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetZoomLevels					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;

	int		GetZoomFOV1						( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetZoomFOV2						( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	float	GetZoomTime0					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	float	GetZoomTime1					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	float	GetZoomTime2					( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;

	int		GetPrimaryReserveAmmoMax				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;
	int		GetSecondaryReserveAmmoMax				( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const;


	CSWeaponType GetWeaponType						( const CEconItemView* pWepView = NULL ) const;
	const char* GetAddonLocation					( const CEconItemView* pWepView = NULL ) const;
	const char* GetEjectBrassEffectName				( const CEconItemView* pWepView = NULL ) const;
	const char* GetTracerEffectName					( const CEconItemView* pWepView = NULL ) const;
	const char* GetMuzzleFlashEffectName_1stPerson	( const CEconItemView* pWepView = NULL ) const;
	const char* GetMuzzleFlashEffectName_1stPersonAlt	( const CEconItemView* pWepView = NULL ) const;
	const char* GetMuzzleFlashEffectName_3rdPerson	( const CEconItemView* pWepView = NULL ) const;
	const char* GetMuzzleFlashEffectName_3rdPersonAlt	( const CEconItemView* pWepView = NULL ) const;
	const char* GetHeatEffectName					( const CEconItemView* pWepView = NULL ) const;
	const char* GetPlayerAnimationExtension			( const CEconItemView* pWepView = NULL ) const;
	void		SetUsedByTeam						( int nTeam )	{ m_iTeam = nTeam; }
	int			GetUsedByTeam						( const CEconItemView* pWepView = NULL ) const;

	bool		CanBeUsedWithShield() const { return m_bCanUseWithShield; }
	float		GetBotAudibleRange() const { return m_flBotAudibleRange; }
	const char* GetWrongTeamMsg() const { return m_WrongTeamMsg; }
	const char* GetAnimExtension() const { return m_szAnimExtension; }
	const char* GetShieldViewModel() const { return m_szShieldViewModel; }
	const char* GetSilencerModel() const { return m_szSilencerModel; }
	float		GetAddonScale() const { return m_flAddonScale; }
	float		GetThrowVelocity() const { return m_fThrowVelocity; }

	const char* GetAddonModel						( const CEconItemView* pWepView = NULL ) const;
	const CUtlVector< WeaponPaintableMaterial_t >* GetPaintData( const CEconItemView* pWepView = NULL ) const;

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	void GenerateRecoilTable();

	void SetWeaponPrice( int price )	{ m_iWeaponPrice = price; };
	void SetWeaponType( CSWeaponType type )	{ m_WeaponType = type; };

private:
	static bool m_bCSWeaponInfoLookupInitialized;

	bool	m_bFullAuto;		// is this a fully automatic weapon?
	float	m_flHeatPerShot;
	int		m_iWeaponPrice;
	float	m_flArmorRatio;
	float	m_flMaxSpeed[2];	// How fast the player can run while this is his primary weapon.

	int		m_iCrosshairMinDistance;
	int		m_iCrosshairDeltaDistance;

	// Parameters for FX_FireBullets:
	float	m_flPenetration;
	int		m_iDamage;
	float	m_flRange;
	float	m_flRangeModifier;
	int		m_iBullets;
	float	m_flCycleTime;
	float	m_flCycleTimeAlt;

   	char	m_szHeatEffectName[MAX_WEAPON_STRING];

	Vector	m_vSmokeColor;

   	char	m_szMuzzleFlashEffectName_1stPerson[MAX_WEAPON_STRING];
	char	m_szMuzzleFlashEffectName_3rdPerson[MAX_WEAPON_STRING];
	char	m_szEjectBrassEffectName[MAX_WEAPON_STRING];
	char	m_szTracerEffectName[MAX_WEAPON_STRING];
	int		m_iTracerFequency;

	// variables for new accuracy model
	float m_fSpread[2];
	float m_fInaccuracyCrouch[2];
	float m_fInaccuracyStand[2];
	float m_fInaccuracyJump[2];
	float m_fInaccuracyLand[2];
	float m_fInaccuracyLadder[2];
	float m_fInaccuracyImpulseFire[2];
	float m_fInaccuracyMove[2];
	float m_fRecoveryTimeStand;
	float m_fRecoveryTimeCrouch;
	float m_fRecoveryTimeStandFinal;
	float m_fRecoveryTimeCrouchFinal;
	float m_fInaccuracyReload;
	float m_fInaccuracyAltSwitch;
	float m_fInaccuracyPitchShift;
	float m_fInaccuracyAltSoundThreshold;
	float m_fRecoilAngle[2];
	float m_fRecoilAngleVariance[2];
	float m_fRecoilMagnitude[2];
	float m_fRecoilMagnitudeVariance[2];
	int   m_iRecoilSeed;

	float m_fFlinchVelocityModifierLarge;			// velocity modifier for anyone hit by this weapon
	float m_fFlinchVelocityModifierSmall;			// velocity modifier for anyone hit by this weapon

	// Delay until the next idle animation after shooting.
	float	m_flTimeToIdleAfterFire;
	float	m_flIdleInterval;

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	struct RecoilOffset
	{
		float	fAngle;
		float	fMagnitude;
	};
	RecoilOffset	m_recoilTable[2][64];


	int		m_iZoomLevels;
	int		m_iZoomFov[ 3 ];
	float	m_fZoomTime[ 3 ];
	bool	m_bHideViewModelZoomed;
	char	m_szZoomINSound[ MAX_WEAPON_STRING ];
	char	m_szZoomOUTSound[ MAX_WEAPON_STRING ];

	float m_flBotAudibleRange;	// How far away a bot can hear this weapon.

	bool  m_bCanUseWithShield;

	char m_WrongTeamMsg[ 32 ];	// Reference to a string describing the error if someone tries to buy
	// this weapon but they're on the wrong team to have it.
	// Zero-length if no specific message for this weapon.

	char m_szAnimExtension[ 16 ];
	char m_szShieldViewModel[ 64 ];

	char m_szAddonModel[ MAX_WEAPON_STRING ];		// If this is set, it is used as the addon model. Otherwise, szWorldModel is used.
	char m_szAddonLocation[ MAX_WEAPON_STRING ];	//If this is set, the weapon will look for an attachment location with this name. Otherwize the default is used based on weapon type.
	char m_szSilencerModel[ MAX_WEAPON_STRING ];	// Alternate model with silencer attached

	float m_flAddonScale;

	// grenade throw parameters
	float	m_fThrowVelocity;

	int		m_iKillAward;


};


//--------------------------------------------------------------------------------------------------------
// Utility conversion functions 
//--------------------------------------------------------------------------------------------------------
const char* WeaponClassAsString( CSWeaponType weaponType );
CSWeaponType WeaponClassFromString( const char * weaponType );
CSWeaponType WeaponClassFromWeaponID( CSWeaponID weaponID );
const char* WeaponIdAsString( CSWeaponID weaponID );
CSWeaponID WeaponIdFromString( const char *szWeaponName );
const char *WeaponIDToAlias( int id );
CSWeaponID AliasToWeaponID( const char *szAlias );
const CCSWeaponInfo* GetWeaponInfo( CSWeaponID weaponID );
bool IsGunWeapon( CSWeaponType weaponType );

#endif // CS_WEAPON_PARSE_H

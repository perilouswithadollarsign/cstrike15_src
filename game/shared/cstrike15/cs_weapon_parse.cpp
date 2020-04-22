//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <keyvalues.h>
#include "cs_weapon_parse.h"
#include "cs_shareddefs.h"
#include "weapon_csbase.h"
#include "weapon_csbasegun.h"
#include "icvar.h"
#include "cs_gamerules.h"
#include "ihasattributes.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------------
struct WeaponTypeInfo
{
	CSWeaponType type;
	const char * name;
};

//--------------------------------------------------------------------------------------------------------
WeaponTypeInfo s_weaponTypeInfo[] =
{
	{ WEAPONTYPE_KNIFE,			"Knife" },
	{ WEAPONTYPE_PISTOL,		"Pistol" },
	{ WEAPONTYPE_SUBMACHINEGUN, "Submachine Gun" },	// First match is printable
	{ WEAPONTYPE_SUBMACHINEGUN, "submachinegun" },
	{ WEAPONTYPE_SUBMACHINEGUN, "smg" },
	{ WEAPONTYPE_RIFLE,			"Rifle" },
	{ WEAPONTYPE_SHOTGUN,		"Shotgun" },
	{ WEAPONTYPE_SNIPER_RIFLE,	"SniperRifle" },
	{ WEAPONTYPE_MACHINEGUN,	"Machine Gun" },		// First match is printable
	{ WEAPONTYPE_MACHINEGUN,	"machinegun" },
	{ WEAPONTYPE_MACHINEGUN,	"mg" },
	{ WEAPONTYPE_C4,			"C4" },
	{ WEAPONTYPE_GRENADE,		"Grenade" },
	{ WEAPONTYPE_STACKABLEITEM,	"StackableItem" },
};


struct WeaponNameInfo
{
	CSWeaponID id;
	const char *name;
};

// note you have to add an item here if it is a new base item that derives from another base item (like the silenced m4)
WeaponNameInfo s_weaponNameInfo[] =
{
	{ WEAPON_DEAGLE,			"weapon_deagle" },
	{ WEAPON_DEAGLE,			"weapon_revolver" },
	{ WEAPON_ELITE,				"weapon_elite" },
	{ WEAPON_FIVESEVEN,			"weapon_fiveseven" },
	{ WEAPON_FIVESEVEN,			"weapon_cz75a" },
	{ WEAPON_GLOCK,				"weapon_glock" },
	{ WEAPON_P228,				"weapon_p228" },
	{ WEAPON_USP,				"weapon_usp" },

	{ WEAPON_AK47,				"weapon_ak47" },
	{ WEAPON_AUG,				"weapon_aug" },
	{ WEAPON_AWP,				"weapon_awp" },
	{ WEAPON_FAMAS,				"weapon_famas" },
	{ WEAPON_G3SG1,				"weapon_g3sg1" },
	{ WEAPON_GALIL,				"weapon_galil" },
	{ WEAPON_GALILAR,			"weapon_galilar" },
	{ WEAPON_M249,				"weapon_m249" },
	{ WEAPON_M3,				"weapon_m3" },
	{ WEAPON_M4A1,				"weapon_m4a1" },
	{ WEAPON_M4A1,				"weapon_m4a1_silencer" },
	{ WEAPON_MAC10,				"weapon_mac10" },
	{ WEAPON_MP5NAVY,			"weapon_mp5navy" },
	{ WEAPON_P90,				"weapon_p90" },
	{ WEAPON_SCOUT,				"weapon_scout" },
	{ WEAPON_SG550,				"weapon_sg550" },
	{ WEAPON_SG552,				"weapon_sg552" },
	{ WEAPON_TMP,				"weapon_tmp" },
	{ WEAPON_UMP45,				"weapon_ump45" },
	{ WEAPON_XM1014,			"weapon_xm1014" },

	{ WEAPON_BIZON,				"weapon_bizon" },
	{ WEAPON_MAG7,				"weapon_mag7" },
	{ WEAPON_NEGEV,				"weapon_negev" },
	{ WEAPON_SAWEDOFF,			"weapon_sawedoff" },
	{ WEAPON_TEC9,				"weapon_tec9" },
	{ WEAPON_TEC9,				"weapon_cz75a" },
	{ WEAPON_TASER,				"weapon_taser" },

	{ WEAPON_HKP2000,			"weapon_hkp2000" },
	{ WEAPON_MP7,				"weapon_mp7" },
	{ WEAPON_MP9,				"weapon_mp9" },
	{ WEAPON_NOVA,				"weapon_nova" },
	{ WEAPON_P250,				"weapon_p250" },
	{ WEAPON_SCAR17,			"weapon_scar17" },
	{ WEAPON_SCAR20,			"weapon_scar20" },
	{ WEAPON_SG556,				"weapon_sg556" },
	{ WEAPON_SSG08,				"weapon_ssg08" },

	{ WEAPON_KNIFE_GG,			"weapon_knifegg" },
	{ WEAPON_KNIFE,				"weapon_knife" },

	{ WEAPON_HEGRENADE,			"weapon_hegrenade" },
	{ WEAPON_SMOKEGRENADE,		"weapon_smokegrenade" },
	{ WEAPON_FLASHBANG,			"weapon_flashbang" },
	{ WEAPON_MOLOTOV,			"weapon_molotov" },
	{ WEAPON_INCGRENADE,		"weapon_incgrenade" },
	{ WEAPON_DECOY,				"weapon_decoy" },
	{ WEAPON_TAGRENADE,			"weapon_tagrenade" },

	{ WEAPON_C4,				"weapon_c4" },

	{ WEAPON_HEALTHSHOT,		"weapon_healthshot" },

	{ ITEM_KEVLAR,				"item_kevlar" },
	{ ITEM_ASSAULTSUIT,			"item_assaultsuit" },
	{ ITEM_HEAVYASSAULTSUIT,			"item_heavyassaultsuit" },	
	{ ITEM_NVG,					"item_nvg" },
	{ ITEM_DEFUSER,				"item_defuser" },
	{ ITEM_CUTTERS,				"item_cutters" },

	{ WEAPON_NONE,				"weapon_none" },
};



//--------------------------------------------------------------------------------------------------------------


struct EquipmentInfo
{
	const char*		szClassName;
	const char*		szPrintName;
	int				iTeam;
	int				iPrice;
};

EquipmentInfo g_EquipmentInfo[] =
{
	{ "item_assaultsuit",	"#SFUI_WPNHUD_ASSAULTSUIT",		TEAM_UNASSIGNED,	ITEM_PRICE_ASSAULTSUIT },
	{ "item_kevlar",		"#SFUI_WPNHUD_KEVLAR",			TEAM_UNASSIGNED,	ITEM_PRICE_KEVLAR },
	{ "item_heavyassaultsuit",	"#SFUI_WPNHUD_HEAVYASSAULTSUIT",	TEAM_UNASSIGNED,		ITEM_PRICE_HEAVYASSAULTSUIT },
	{ "item_defuser",		"#SFUI_WPNHUD_DEFUSER",			TEAM_CT,			ITEM_PRICE_DEFUSEKIT },
	{ "item_cutters",		"#SFUI_WPNHUD_CUTTERS",			TEAM_CT,			ITEM_PRICE_DEFUSEKIT },
};

void LoadEquipmentData()
{
	for ( int i = 0; i < ARRAYSIZE( g_EquipmentInfo ); ++i )
	{
		WEAPON_FILE_INFO_HANDLE handle = g_WeaponDatabase.FindOrCreateWeaponInfo(g_EquipmentInfo[i].szClassName);
		CCSWeaponInfo* pInfo = dynamic_cast<CCSWeaponInfo*>( g_WeaponDatabase.GetFileWeaponInfoFromHandle( handle ) );
		if ( pInfo )
		{
			pInfo->m_weaponId = WeaponIdFromString( pInfo->szClassName );
			Q_strncpy( pInfo->szClassName, g_EquipmentInfo[i].szClassName, MAX_WEAPON_STRING );
			Q_strncpy( pInfo->szPrintName, g_EquipmentInfo[i].szPrintName, MAX_WEAPON_STRING );
			pInfo->SetUsedByTeam( g_EquipmentInfo[i].iTeam );
			pInfo->SetWeaponPrice( g_EquipmentInfo[i].iPrice );
			pInfo->SetWeaponType( WEAPONTYPE_EQUIPMENT );
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
const CCSWeaponInfo* GetWeaponInfo( CSWeaponID weaponID )
{
	if ( weaponID == WEAPON_NONE )
		return NULL;

	const char *weaponName = WeaponIdAsString( weaponID );
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = g_WeaponDatabase.FindWeaponInfo( weaponName );
	if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
	{
		return NULL;
	}

	CCSWeaponInfo *pWeaponInfo = dynamic_cast< CCSWeaponInfo* >( g_WeaponDatabase.GetFileWeaponInfoFromHandle( hWpnInfo ) );

	return pWeaponInfo;
}

//--------------------------------------------------------------------------------------------------------
const char* WeaponClassAsString( CSWeaponType weaponType )
{
	for ( int i = 0; i < ARRAYSIZE( s_weaponTypeInfo ); ++i )
	{
		if ( s_weaponTypeInfo[i].type == weaponType )
		{
			return s_weaponTypeInfo[i].name;
		}
	}

	return NULL;
}


//--------------------------------------------------------------------------------------------------------
CSWeaponType WeaponClassFromString( const char* weaponType )
{
	for ( int i = 0; i < ARRAYSIZE( s_weaponTypeInfo ); ++i )
	{
		if ( !V_stricmp( s_weaponTypeInfo[i].name, weaponType ) )
		{
			return s_weaponTypeInfo[i].type;
		}
	}

	return WEAPONTYPE_UNKNOWN;
}


//--------------------------------------------------------------------------------------------------------
CSWeaponType WeaponClassFromWeaponID( CSWeaponID weaponID )
{
	const char *weaponStr = WeaponIdAsString( weaponID );
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot(weaponStr  );
	if ( hWpnInfo != GetInvalidWeaponInfoHandle() )
	{
		CCSWeaponInfo *pWeaponInfo = dynamic_cast< CCSWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
		if ( pWeaponInfo )
		{
			return pWeaponInfo->GetWeaponType();
		}
	}

	return WEAPONTYPE_UNKNOWN;
}


//--------------------------------------------------------------------------------------------------------
const char * WeaponIdAsString( CSWeaponID weaponID )
{
	for ( int i = 0; i < ARRAYSIZE( s_weaponNameInfo ); ++i )
	{
		if ( s_weaponNameInfo[i].id == weaponID )
			return s_weaponNameInfo[i].name;
	}

	return NULL;
}


//--------------------------------------------------------------------------------------------------------
CSWeaponID WeaponIdFromString( const char *szWeaponName )
{
	for ( int i = 0; i < ARRAYSIZE( s_weaponNameInfo ); ++i )
	{
		if ( V_stricmp( s_weaponNameInfo[i].name, szWeaponName ) == 0 )
			return s_weaponNameInfo[i].id;
	}

	return WEAPON_NONE;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a weapon ID, return its alias
//
const char *WeaponIDToAlias( int id )
{
	for ( int i = 0; i < ARRAYSIZE( s_weaponNameInfo ); ++i )
	{
		if ( s_weaponNameInfo[i].id == id )
			return ( strchr( s_weaponNameInfo[i].name, '_' ) + 1 );
	}

	return NULL;
}

//--------------------------------------------------------------------------------------------------------
//
// Given an alias, return the associated weapon ID
//
CSWeaponID AliasToWeaponID( const char *szAlias )
{
	if ( !szAlias )
		return WEAPON_NONE;

	for ( int i = 0; i < ARRAYSIZE( s_weaponNameInfo ); ++i )
	{
		Assert( strchr( s_weaponNameInfo[i].name, '_' ) );
		if ( Q_stricmp( ( strchr( s_weaponNameInfo[i].name, '_' ) + 1 ), szAlias ) == 0 )
			return s_weaponNameInfo[i].id;
	}

	return WEAPON_NONE;
}

bool IsGunWeapon( CSWeaponType weaponType )
{
	switch ( weaponType )
	{
	case WEAPONTYPE_PISTOL:
	case WEAPONTYPE_SUBMACHINEGUN:
	case WEAPONTYPE_RIFLE:
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_SNIPER_RIFLE:
	case WEAPONTYPE_MACHINEGUN:
		return true;

	default:
		return false;
	}
}

//--------------------------------------------------------------------------------------------------------
void ParseVector( KeyValues *keyValues, const char *keyName, Vector& vec )
{
	vec.x = vec.y = vec.z = 0.0f;

	if ( !keyValues || !keyName )
		return;

	const char *vecString = keyValues->GetString( keyName, "0 0 0" );
	if ( vecString && *vecString )
	{
		float x, y, z;
		if ( 3 == sscanf( vecString, "%f %f %f", &x, &y, &z ) )
		{
			vec.x = x;
			vec.y = y;
			vec.z = z;
		}
	}
}


struct EnumerationStringValue
{
	const char* szString;
	int iValue;
};

int ParseEnumeration( KeyValues* pKeyValuesData, const char* szKeyName, const EnumerationStringValue enumStringTable[], int iCount, int iDefaultValue )
{
	const char *pTeam = pKeyValuesData->GetString( "Team", NULL );
	if ( !pTeam )
		return iDefaultValue;

	for ( int i = 0; i < iCount; ++i )
	{
		if ( V_stricmp( pTeam, enumStringTable[i].szString ) == 0 )
			return enumStringTable[i].iValue;
	}
	
	Assert( false );
	return iDefaultValue;
}


FileWeaponInfo_t* CreateWeaponInfo()
{
	return new CCSWeaponInfo;
}


template <typename T>
void ZeroObject( T* p )
{
	memset( p, 0x0, sizeof(T) );
}

CCSWeaponInfo::CCSWeaponInfo()
{
	m_weaponId = WEAPON_NONE;
	m_flMaxSpeed[0] = m_flMaxSpeed[1] = 1; // This should always be set in the script.
	m_iZoomLevels = 0;
	ZeroObject(m_iZoomFov);
	ZeroObject(m_fZoomTime);
	m_bHideViewModelZoomed = false;
	m_szZoomINSound[0] = '\0';
	m_szZoomOUTSound[0] = '\0';
	m_WeaponType = WEAPONTYPE_UNKNOWN;
	m_bFullAuto = false;
	m_iTeam = TEAM_UNASSIGNED;
	m_flBotAudibleRange = 0.f;
	m_flArmorRatio = 0.f;
	m_iCrosshairMinDistance = 0;
	m_iCrosshairDeltaDistance = 0;
	m_bCanUseWithShield = false;
	m_WrongTeamMsg[0] = '\0';
	m_szAnimExtension[0] = '\0';
	m_szShieldViewModel[0] = '\0';
	m_szAddonModel[0] = '\0';
	m_szAddonLocation[0] = '\0';
	m_szSilencerModel[0] = '\0';

	m_flAddonScale = 1.f;

	m_fFlinchVelocityModifierLarge = 1.f;
	m_fFlinchVelocityModifierSmall = 1.f;

	m_flPenetration = 0;
	m_iDamage = 0;
	m_flRange = 0.f;
	m_flRangeModifier = 0.f;
	m_iBullets = 0;
	m_flCycleTime = 0.f;
	m_flCycleTimeAlt = 0.f;

	ZeroObject(m_fSpread);
	ZeroObject(m_fInaccuracyCrouch);
	ZeroObject(m_fInaccuracyStand);
	ZeroObject(m_fInaccuracyJump);
	ZeroObject(m_fInaccuracyLand);
	ZeroObject(m_fInaccuracyLadder);
	ZeroObject(m_fInaccuracyImpulseFire);
	ZeroObject(m_fInaccuracyMove);
	m_fRecoveryTimeStand = 0.f;
	m_fRecoveryTimeCrouch = 0.f;
	m_fInaccuracyReload = 0.f;
	m_fInaccuracyAltSwitch = 0.f;
	m_fInaccuracyPitchShift = 0.f;
	m_fInaccuracyAltSoundThreshold = 0.f;
	ZeroObject(m_fRecoilAngle);
	ZeroObject(m_fRecoilAngleVariance);
	ZeroObject(m_fRecoilMagnitude);
	ZeroObject(m_fRecoilMagnitudeVariance);
	m_iRecoilSeed = 0;

	m_flTimeToIdleAfterFire = 0.f;
	m_flIdleInterval = 0.f;

	m_fThrowVelocity = 0.0f;

	m_iKillAward = 0;
	m_iWeaponPrice = 0;

	m_flHeatPerShot = 0.f;
	m_szHeatEffectName[0] = '\0';
	m_vSmokeColor.Zero();

	m_szMuzzleFlashEffectName_1stPerson[0] = '\0';
	m_szMuzzleFlashEffectName_3rdPerson[0] = '\0';
	m_szEjectBrassEffectName[0] = '\0';
	m_szTracerEffectName[0] = '\0';
	m_iTracerFequency = 0;

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	ZeroObject(m_recoilTable);

	if ( !m_bCSWeaponInfoLookupInitialized )
	{
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_bFullAuto ),					FIELD_BOOLEAN,	"is full auto" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flHeatPerShot ),				FIELD_FLOAT,	"heat per shot" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flAddonScale ),					FIELD_FLOAT,	"addon scale" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iTracerFequency ),				FIELD_INTEGER,	"tracer frequency" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flMaxSpeed[0] ),				FIELD_FLOAT,	"max player speed" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flMaxSpeed[1] ),				FIELD_FLOAT,	"max player speed alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iWeaponPrice ),					FIELD_INTEGER,	"in game price" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flArmorRatio ),					FIELD_FLOAT,	"armor ratio" ) );

		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iKillAward ),					FIELD_INTEGER,	"kill award" ) );

		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iCrosshairMinDistance ),		FIELD_INTEGER,	"crosshair min distance" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iCrosshairDeltaDistance ),		FIELD_INTEGER,	"crosshair delta distance" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flPenetration ),				FIELD_FLOAT,	"penetration" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iDamage ),						FIELD_INTEGER,	"damage" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flRange ),						FIELD_FLOAT,	"range" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flRangeModifier ),				FIELD_FLOAT,	"range modifier" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iBullets ),						FIELD_INTEGER,	"bullets" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flCycleTime ),					FIELD_FLOAT,	"cycletime" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flCycleTimeAlt ),				FIELD_FLOAT,	"cycletime alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flTimeToIdleAfterFire ),		FIELD_FLOAT,	"time to idle" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_flIdleInterval ),				FIELD_FLOAT,	"idle interval" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fFlinchVelocityModifierLarge ),	FIELD_FLOAT,	"flinch velocity modifier large" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fFlinchVelocityModifierSmall ),	FIELD_FLOAT,	"flinch velocity modifier small" ) );

		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fZoomTime[ 0 ] ),				FIELD_FLOAT,	"zoom time 0" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fZoomTime[ 1 ] ),				FIELD_FLOAT,	"zoom time 1" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fZoomTime[ 2 ] ),				FIELD_FLOAT,	"zoom time 2" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iZoomFov[1] ),					FIELD_INTEGER,	"zoom fov 1" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iZoomFov[2] ),					FIELD_INTEGER,	"zoom fov 2" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_bHideViewModelZoomed ),			FIELD_BOOLEAN,	"hide view model zoomed" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iZoomLevels ),					FIELD_INTEGER,	"zoom levels" ) );

		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fSpread[0] ),					FIELD_FLOAT,	"spread" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyCrouch[0] ),			FIELD_FLOAT,	"inaccuracy crouch" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyStand[0] ),			FIELD_FLOAT,	"inaccuracy stand" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyJump[0] ),			FIELD_FLOAT,	"inaccuracy jump" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyLand[0] ),			FIELD_FLOAT,	"inaccuracy land" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyLadder[0] ),			FIELD_FLOAT,	"inaccuracy ladder" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyImpulseFire[0] ),	FIELD_FLOAT,	"inaccuracy fire" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyMove[0] ),			FIELD_FLOAT,	"inaccuracy move" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyReload ),			FIELD_FLOAT,	"inaccuracy reload" ) );

		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fSpread[1] ),					FIELD_FLOAT,	"spread alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyCrouch[1] ),			FIELD_FLOAT,	"inaccuracy crouch alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyStand[1] ),			FIELD_FLOAT,	"inaccuracy stand alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyJump[1] ),			FIELD_FLOAT,	"inaccuracy jump alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyLand[1] ),			FIELD_FLOAT,	"inaccuracy land alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyLadder[1] ),			FIELD_FLOAT,	"inaccuracy ladder alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyImpulseFire[1] ),	FIELD_FLOAT,	"inaccuracy fire alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fInaccuracyMove[1] ),			FIELD_FLOAT,	"inaccuracy move alt" ) );

		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoveryTimeCrouch ),			FIELD_FLOAT,	"recovery time crouch" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoveryTimeStand ),			FIELD_FLOAT,	"recovery time stand" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoveryTimeCrouchFinal ),		FIELD_FLOAT,	"recovery time crouch final" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoveryTimeStandFinal ),		FIELD_FLOAT,	"recovery time stand final" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_iRecoilSeed ),					FIELD_INTEGER,	"recoil seed" ) );

		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoilAngle[0] ),				FIELD_FLOAT,	"recoil angle" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoilAngleVariance[0] ),		FIELD_FLOAT,	"recoil angle variance" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoilMagnitude[0] ),			FIELD_FLOAT,	"recoil magnitude" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoilMagnitudeVariance[0] ),	FIELD_FLOAT,	"recoil magnitude variance" ) );

		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoilAngle[1] ),				FIELD_FLOAT,	"recoil angle alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoilAngleVariance[1] ),		FIELD_FLOAT,	"recoil angle variance alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoilMagnitude[1] ),			FIELD_FLOAT,	"recoil magnitude alt" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( CCSWeaponInfo, m_fRecoilMagnitudeVariance[1] ),	FIELD_FLOAT,	"recoil magnitude variance alt" ) );

		m_bCSWeaponInfoLookupInitialized = true;
	}

}

bool CCSWeaponInfo::m_bCSWeaponInfoLookupInitialized;


CSWeaponType CCSWeaponInfo::GetWeaponType( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pszString = pWepView->GetStaticData()->GetWeaponTypeString();
		
		if ( pszString )
		{
			return WeaponClassFromString( pszString );
		}
		else
		{
//			DevWarning( "Weapon %s is missing a weapontype in the item schema.\n", WeaponIdAsString( m_weaponId ) );
			return m_WeaponType;
		}
	}

	return m_WeaponType;
}

const char* CCSWeaponInfo::GetAddonLocation( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetAddonLocation();

		if ( pszString )
		{
			return pszString;
		}
	}

	return m_szAddonLocation;
}

const char* CCSWeaponInfo::GetEjectBrassEffectName( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetEjectBrassEffect();

		if ( pszString )
		{
			return pszString;
		}
	}

	return m_szEjectBrassEffectName;
}

const char* CCSWeaponInfo::GetTracerEffectName( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetTracerEffect();

		if ( pszString )
		{
			return pszString;
		}
	}

	return m_szTracerEffectName;
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_1stPerson( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetMuzzleFlashEffect1stPerson();

		if ( pszString )
		{
			return pszString;
		}
	}

	return m_szMuzzleFlashEffectName_1stPerson;
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_1stPersonAlt( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetMuzzleFlashEffect1stPersonAlt();

		if ( pszString )
		{
			return pszString;
		}
	}

	return m_szMuzzleFlashEffectName_1stPerson;
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_3rdPerson( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetMuzzleFlashEffect3rdPerson();

		if ( pszString )
		{
			return pszString;
		}
	}

	return m_szMuzzleFlashEffectName_3rdPerson;
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_3rdPersonAlt( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetMuzzleFlashEffect3rdPersonAlt();

		if ( pszString )
		{
			return pszString;
		}
	}

	return m_szMuzzleFlashEffectName_3rdPerson;
}

const char* CCSWeaponInfo::GetHeatEffectName( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetHeatEffect();

		if ( pszString )
		{
			return pszString;
		}
	}

	return m_szHeatEffectName;
}

const char* CCSWeaponInfo::GetPlayerAnimationExtension( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetPlayerAnimationExtension();

		if ( pszString )
		{
			return pszString;
		}
	}

	return m_szAnimExtension;
}

int CCSWeaponInfo::GetUsedByTeam( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		return pWepView->GetStaticData()->GetUsedByTeam();
	}

	return m_iTeam;
}

const char* CCSWeaponInfo::GetAddonModel( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchAddon = pWepView->GetStaticData()->GetEntityOverrideModel();
		return pchAddon ? pchAddon : m_szAddonModel;
	}
	else
	{
		return m_szAddonModel;
	}
}

const CUtlVector< WeaponPaintableMaterial_t >* CCSWeaponInfo::GetPaintData( const CEconItemView* pWepView ) const
{
	if ( !pWepView || !pWepView->IsValid() )
		return NULL;

	return pWepView->GetStaticData()->GetPaintData();
}

void CCSWeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	m_weaponId = WeaponIdFromString( szWeaponName );

	m_flMaxSpeed[0] = ( float )pKeyValuesData->GetInt( "MaxPlayerSpeed", 1 );
	m_flMaxSpeed[1] = ( float )pKeyValuesData->GetInt( "MaxPlayerSpeedAlt", m_flMaxSpeed[0] );

	m_iZoomLevels = pKeyValuesData->GetInt( "ZoomLevels", 0 );

	// sanity check
	Assert( m_iZoomLevels < ARRAYSIZE( m_fZoomTime ) && m_iZoomLevels < ARRAYSIZE( m_iZoomFov ) );
	m_iZoomLevels = MIN( m_iZoomLevels, ARRAYSIZE( m_iZoomFov ) );

	m_iZoomFov[0] = 0;	// not used -- always default FoV
	m_fZoomTime[0] = pKeyValuesData->GetFloat( "ZoomTime0", 0.0f );
	m_iZoomFov[1] = pKeyValuesData->GetInt( "ZoomFov1", 90 );
	m_fZoomTime[1] = pKeyValuesData->GetFloat( "ZoomTime1", 0.0f );
	m_iZoomFov[2] = pKeyValuesData->GetInt( "ZoomFov2", 90 );
	m_fZoomTime[2] = pKeyValuesData->GetFloat( "ZoomTime2", 0.0f );
	m_bHideViewModelZoomed = pKeyValuesData->GetBool( "HideViewModelZoomed", false );
	V_strncpy( m_szZoomINSound, pKeyValuesData->GetString( "ZoomINSound" ), sizeof( m_szZoomINSound ) );
	V_strncpy( m_szZoomOUTSound, pKeyValuesData->GetString( "ZoomOUTSound" ), sizeof( m_szZoomOUTSound ) );

	m_iKillAward = pKeyValuesData->GetInt( "KillAward", 300 );

	m_iWeaponPrice = pKeyValuesData->GetInt( "WeaponPrice", -1 );
	if ( m_iWeaponPrice == -1 )
	{
		// This weapon should have the price in its script.
		Assert( false );
	}

	m_flArmorRatio				= pKeyValuesData->GetFloat( "WeaponArmorRatio", 1 );
	m_iCrosshairMinDistance		= pKeyValuesData->GetInt( "CrosshairMinDistance", 4 );
	m_iCrosshairDeltaDistance	= pKeyValuesData->GetInt( "CrosshairDeltaDistance", 3 );
	m_bCanUseWithShield			= !!pKeyValuesData->GetInt( "CanEquipWithShield", false );
	m_flAddonScale				= pKeyValuesData->GetFloat( "AddonScale", 1.0f );
	m_fFlinchVelocityModifierLarge	= pKeyValuesData->GetFloat( "FlinchVelocityModifierLarge", 1.0f );
	m_fFlinchVelocityModifierSmall	= pKeyValuesData->GetFloat( "FlinchVelocityModifierSmall", 1.0f );

	m_flPenetration		= pKeyValuesData->GetFloat( "Penetration", 1.0 );
	m_iDamage			= pKeyValuesData->GetInt( "Damage", 42 ); // Douglas Adams 1952 - 2001
	m_flRange			= pKeyValuesData->GetFloat( "Range", 8192.0f );
	m_flRangeModifier	= pKeyValuesData->GetFloat( "RangeModifier", 0.98f );
	m_iBullets			= pKeyValuesData->GetInt( "Bullets", 1 );
	m_flCycleTime		= pKeyValuesData->GetFloat( "CycleTime", 0.15 );
	m_flCycleTimeAlt	= pKeyValuesData->GetFloat( "CycleTimeAlt", 0.3 );

	// new accuracy model parameters
	m_fSpread[0]				= pKeyValuesData->GetFloat( "Spread", 0.0f );
	m_fInaccuracyCrouch[0]		= pKeyValuesData->GetFloat( "InaccuracyCrouch", 0.0f );
	m_fInaccuracyStand[0]		= pKeyValuesData->GetFloat( "InaccuracyStand", 0.0f );
	m_fInaccuracyJump[0]		= pKeyValuesData->GetFloat( "InaccuracyJump", 0.0f );
	m_fInaccuracyLand[0]		= pKeyValuesData->GetFloat( "InaccuracyLand", 0.0f );
	m_fInaccuracyLadder[0]		= pKeyValuesData->GetFloat( "InaccuracyLadder", 0.0f );
	m_fInaccuracyImpulseFire[0]	= pKeyValuesData->GetFloat( "InaccuracyFire", 0.0f );
	m_fInaccuracyMove[0]		= pKeyValuesData->GetFloat( "InaccuracyMove", 0.0f );

	m_fSpread[1]				= pKeyValuesData->GetFloat( "SpreadAlt",			m_fSpread[0] );
	m_fInaccuracyCrouch[1]		= pKeyValuesData->GetFloat( "InaccuracyCrouchAlt",	m_fInaccuracyCrouch[0] );
	m_fInaccuracyStand[1]		= pKeyValuesData->GetFloat( "InaccuracyStandAlt",	m_fInaccuracyStand[0] );
	m_fInaccuracyJump[1]		= pKeyValuesData->GetFloat( "InaccuracyJumpAlt",	m_fInaccuracyJump[0] );
	m_fInaccuracyLand[1]		= pKeyValuesData->GetFloat( "InaccuracyLandAlt",	m_fInaccuracyLand[0] );
	m_fInaccuracyLadder[1]		= pKeyValuesData->GetFloat( "InaccuracyLadderAlt",	m_fInaccuracyLadder[0] );
	m_fInaccuracyImpulseFire[1]	= pKeyValuesData->GetFloat( "InaccuracyFireAlt",	m_fInaccuracyImpulseFire[0] );
	m_fInaccuracyMove[1]		= pKeyValuesData->GetFloat( "InaccuracyMoveAlt",	m_fInaccuracyMove[0] );

	m_fRecoilAngle[0]				= pKeyValuesData->GetFloat( "RecoilAngle", 0.0f );
	m_fRecoilAngleVariance[0]		= pKeyValuesData->GetFloat( "RecoilAngleVariance", 0.0f );
	m_fRecoilMagnitude[0]			= pKeyValuesData->GetFloat( "RecoilMagnitude", 0.0f );
	m_fRecoilMagnitudeVariance[0]	= pKeyValuesData->GetFloat( "RecoilMagnitudeVariance", 0.0f );
	m_fRecoilAngle[1]				= pKeyValuesData->GetFloat( "RecoilAngleAlt",				m_fRecoilAngle[0] );
	m_fRecoilAngleVariance[1]		= pKeyValuesData->GetFloat( "RecoilAngleVarianceAlt",		m_fRecoilAngleVariance[0] );
	m_fRecoilMagnitude[1]			= pKeyValuesData->GetFloat( "RecoilMagnitudeAlt",			m_fRecoilMagnitude[0] );
	m_fRecoilMagnitudeVariance[1]	= pKeyValuesData->GetFloat( "RecoilMagnitudeVarianceAlt",	m_fRecoilMagnitudeVariance[0] );
	m_iRecoilSeed					= pKeyValuesData->GetInt( "RecoilSeed", 0 );

	// FIXME[pmf]: temp code - remove when weapon scripts have seed values
	if ( m_iRecoilSeed == 0 && IsGunWeapon(GetWeaponType()) )
	{
		// create a temporary seed value based on a hash of the weapon name
		const char *weaponName = szClassName;

		CRC32_t crc;
		CRC32_Init( &crc );
		CRC32_ProcessBuffer( &crc, (void *)weaponName, Q_strlen( weaponName )  );
		CRC32_Final( &crc );

		m_iRecoilSeed = (int)crc & 0xFFFF;

#ifdef GAME_DLL
		Msg( "RECOIL: No seed found for weapon %s, generated placeholder seed %i\n", weaponName, m_iRecoilSeed );
#endif
	}

	m_fInaccuracyReload			= pKeyValuesData->GetFloat( "InaccuracyReload", 0.0f );
	m_fInaccuracyAltSwitch		= pKeyValuesData->GetFloat( "InaccuracyAltSwitch", 0.0f );

	m_fInaccuracyPitchShift		= pKeyValuesData->GetFloat( "InaccuracyPitchShift", 0.0f );
	m_fInaccuracyAltSoundThreshold = pKeyValuesData->GetFloat( "InaccuracyAltSoundThreshold", 0.0f );

	m_fRecoveryTimeCrouch		= pKeyValuesData->GetFloat( "RecoveryTimeCrouch", 1.0f );
	m_fRecoveryTimeStand		= pKeyValuesData->GetFloat( "RecoveryTimeStand", 1.0f );

	m_fRecoveryTimeCrouchFinal	= pKeyValuesData->GetFloat( "RecoveryTimeCrouchFinal", m_fRecoveryTimeCrouch );
	m_fRecoveryTimeStandFinal	= pKeyValuesData->GetFloat( "RecoveryTimeStandFinal", m_fRecoveryTimeStand );

	m_flTimeToIdleAfterFire	= pKeyValuesData->GetFloat( "TimeToIdle", 2 );
	m_flIdleInterval	= pKeyValuesData->GetFloat( "IdleInterval", 20 );

	// grenade parameters
	m_fThrowVelocity	= pKeyValuesData->GetFloat( "ThrowVelocity", 0.0f );

	m_flHeatPerShot = pKeyValuesData->GetFloat( "HeatPerShot", 0.25f );

	UTIL_StringToVector( m_vSmokeColor.Base(), pKeyValuesData->GetString( "SmokeColor", "1.0 1.0 1.0" ) );

	V_strncpy( m_szHeatEffectName, pKeyValuesData->GetString( "HeatEffect" ), sizeof( m_szHeatEffectName ) );

	// Figure out what team can have this weapon.
	EnumerationStringValue teamEnums[] =
	{
		{ "CT",	TEAM_CT },
		{ "TERRORIST",	TEAM_TERRORIST },
		{ "ANY", TEAM_UNASSIGNED },
	};
	m_iTeam = ParseEnumeration(pKeyValuesData, "Team", teamEnums, ARRAYSIZE(teamEnums), TEAM_UNASSIGNED );

	const char *pWrongTeamMsg = pKeyValuesData->GetString( "WrongTeamMsg", "" );
	V_strncpy( m_WrongTeamMsg, pWrongTeamMsg, sizeof( m_WrongTeamMsg ) );

	const char *pShieldViewModel = pKeyValuesData->GetString( "shieldviewmodel", "" );
	V_strncpy( m_szShieldViewModel, pShieldViewModel, sizeof( m_szShieldViewModel ) );
	
	const char *pAnimEx = pKeyValuesData->GetString( "PlayerAnimationExtension", "m4" );
	V_strncpy( m_szAnimExtension, pAnimEx, sizeof( m_szAnimExtension ) );

	// Default is 2000.
	m_flBotAudibleRange = pKeyValuesData->GetFloat( "BotAudibleRange", 2000.0f );
	
	const char *pTypeString = pKeyValuesData->GetString( "WeaponType", "" );
	m_WeaponType = WeaponClassFromString( pTypeString );

	m_bFullAuto = pKeyValuesData->GetBool( "FullAuto", false );

	// Read the addon model.
	V_strncpy( m_szAddonModel, pKeyValuesData->GetString( "AddonModel" ), sizeof( m_szAddonModel ) );

	// Read a special addon attachment location if not the default location
	V_strncpy( m_szAddonLocation, pKeyValuesData->GetString( "AddonLocation" ), sizeof( m_szAddonLocation ) );
	
	// Read the silencer model.
	V_strncpy( m_szSilencerModel, pKeyValuesData->GetString( "SilencerModel" ), sizeof( m_szSilencerModel ) );

#ifndef CLIENT_DLL
	// Enforce consistency for the weapon here, since that way we don't need to save off the model bounds
	// for all time.
	engine->ForceExactFile( UTIL_VarArgs( "scripts/%s.ctx", szWeaponName ) );

	// Model bounds are rounded to the nearest integer, then extended by 1
	engine->ForceModelBounds( szWorldModel, Vector( -15, -12, -18 ), Vector( 44, 16, 19 ) );
	if ( m_szAddonModel[0] )
	{
		engine->ForceModelBounds( m_szAddonModel, Vector( -5, -5, -6 ), Vector( 13, 5, 7 ) );
	}
	if ( m_szSilencerModel[0] )
	{
		engine->ForceModelBounds( m_szSilencerModel, Vector( -15, -12, -18 ), Vector( 44, 16, 19 ) );
	}
	
#endif // !CLIENT_DLL

    // particle muzzle flash effect to play when fired
	V_strncpy( m_szMuzzleFlashEffectName_1stPerson, pKeyValuesData->GetString( "MuzzleFlashEffect_1stPerson" ), sizeof( m_szMuzzleFlashEffectName_1stPerson ) );
	V_strncpy( m_szMuzzleFlashEffectName_3rdPerson, pKeyValuesData->GetString( "MuzzleFlashEffect_3rdPerson" ), sizeof( m_szMuzzleFlashEffectName_3rdPerson ) );

	// particle effect for the shell casing to eject when we fire bullets
	V_strncpy( m_szEjectBrassEffectName, pKeyValuesData->GetString( "EjectBrassEffect" ), sizeof( m_szEjectBrassEffectName ) );

	// gun tracer effect and frequency
	V_strncpy( m_szTracerEffectName, pKeyValuesData->GetString( "TracerEffect" ), sizeof( m_szTracerEffectName ) );
	m_iTracerFequency		= pKeyValuesData->GetInt( "TracerFrequency", 0 );

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
	// generate the recoil table after everything else has been loaded (since it depends on other script parameters)
	GenerateRecoilTable();
}

ConVar weapon_recoil_suppression_shots( "weapon_recoil_suppression_shots", "4", FCVAR_RELEASE | FCVAR_CHEAT |  FCVAR_REPLICATED, "Number of shots before weapon uses full recoil" );
ConVar weapon_recoil_suppression_factor( "weapon_recoil_suppression_factor", "0.75", FCVAR_RELEASE | FCVAR_CHEAT |  FCVAR_REPLICATED, "Initial recoil suppression factor (first suppressed shot will use this factor * standard recoil, lerping to 1 for later shots" );
ConVar weapon_recoil_variance("weapon_recoil_variance", "0.55", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Amount of variance per recoil impulse", true, 0.0f, true, 1.0f );

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
void CCSWeaponInfo::GenerateRecoilTable()
{
	const int iSuppressionShots = weapon_recoil_suppression_shots.GetInt();
	const float fBaseSuppressionFactor = weapon_recoil_suppression_factor.GetFloat();
	const float fRecoilVariance = weapon_recoil_variance.GetFloat();
	CUniformRandomStream recoilRandom;

	for ( int iMode = 0; iMode < 2; ++iMode )
	{
		recoilRandom.SetSeed( m_iRecoilSeed );

		float fAngle = 0.0f;
		float fMagnitude = 0.0f;

		for ( int j = 0; j < ARRAYSIZE( m_recoilTable[iMode] ); ++j )
		{
			float fAngleNew = m_fRecoilAngle[iMode] + recoilRandom.RandomFloat(-m_fRecoilAngleVariance[iMode], +m_fRecoilAngleVariance[iMode]);
			float fMagnitudeNew = m_fRecoilMagnitude[iMode] + recoilRandom.RandomFloat(-m_fRecoilMagnitudeVariance[iMode], +m_fRecoilMagnitudeVariance[iMode]);

			if ( m_bFullAuto && j > 0 )
			{
				fAngle = Lerp( fRecoilVariance, fAngle, fAngleNew );
				fMagnitude = Lerp( fRecoilVariance, fMagnitude, fMagnitudeNew );
			}
			else
			{
				fAngle = fAngleNew;
				fMagnitude = fMagnitudeNew;
			}

			if ( m_bFullAuto && j < iSuppressionShots )
			{
				float fSuppressionFactor = Lerp( (float)j / (float)iSuppressionShots, fBaseSuppressionFactor, 1.0f );
				fMagnitude *= fSuppressionFactor;
			}

			m_recoilTable[iMode][j].fAngle = fAngle;
			m_recoilTable[iMode][j].fMagnitude = fMagnitude;
		}
	}
}

// recoiltable in csweaponinfo is obsolete. remove this once confirmed that the new implementation generates the same result.
void CCSWeaponInfo::GetRecoilOffsets( int iMode, int iIndex, float& fAngle, float &fMagnitude ) const
{
	iIndex = iIndex % ARRAYSIZE( m_recoilTable[iMode] );
	fAngle = m_recoilTable[iMode][iIndex].fAngle;
	fMagnitude = m_recoilTable[iMode][iIndex].fMagnitude;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Get an attribute associated with the weapon. It might seem a bit odd that in order to get 
// a weapon's damage you need to ask its deprecated weaponinfo object to extract it from its own econitemview
// but we do this because In CS, there are a bunch of cases where there is no weapon instantiated yet and we need the data. All of this
// legacy code uses weaponinfo objects.

// 
// POSSIBLE RETURNS:
//	-2: total failure. We did not find the attribute in the econ item or in the weaponinfo instance.
//	-1: the attribute was found in the econitemview that was passed in.
//	>=0: the returned int is the index into g_WeaponInfoTable that contains the desired data.
//////////////////////////////////////////////////////////////////////////////////////////////////////

static bool GetAttribute_bool( const CCSWeaponInfo* pWeaponInfo, const char * iszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView, float flScale = 1.0f )
{
	uint32 unLocalValue = 0;
	int index = -2;

	if ( pWepView && pWepView->IsValid() )
	{
		if ( pWepView->FindAttribute( hAttrib, &unLocalValue ) )
		{
			return ( unLocalValue != 0 );
		}
	}

	// otherwise use legacy weapon script value
	index = pWeaponInfo->GetIndexofAttribute( AllocPooledString( iszAttrib ) );

	if ( index == -1 ) // we found the attribute in the item.
	{
		return false;
	}
	else if ( index > -1 )
	{
		Assert( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType == FIELD_BOOLEAN );
		if ( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType != FIELD_BOOLEAN )
			return false;

		const bool* pBool;
		pBool = reinterpret_cast< const bool* >( reinterpret_cast< const char* >( pWeaponInfo ) + pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_nWeaponParseDataOffset );

		return *pBool;
	}

	return false;
}

static int GetAttribute_int( const CCSWeaponInfo* pWeaponInfo, const char * iszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView, float flScale = 1.0f )
{
	uint32 unLocalValue = 0;
	int index = -2;

	if ( pWepView && pWepView->IsValid() )
	{
		if ( pWepView->FindAttribute( hAttrib, &unLocalValue ) )
		{
			return unLocalValue;
		}
	}

	// otherwise use legacy weapon script value
	index = pWeaponInfo->GetIndexofAttribute( AllocPooledString( iszAttrib ) );

	if ( index == -1 )
	{
		return 0;
	}
	else if ( index > -1 )
	{
		Assert( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType == FIELD_INTEGER );
		if ( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType != FIELD_INTEGER )
			return false;

		const int* pInt;
		pInt = reinterpret_cast< const int* >( reinterpret_cast< const char* >( pWeaponInfo ) + pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_nWeaponParseDataOffset );

		return *pInt;
	}

	return 0;
}

static float GetAttribute_float( const CCSWeaponInfo* pWeaponInfo, const char * iszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView, float flScale = 1.0f )
{
	float flLocalValue = 0.0f;
	int index = -2;

	if ( pWepView && pWepView->IsValid() )
	{
		if ( pWepView->FindAttribute( hAttrib, &flLocalValue ) )
		{
			return flScale * flLocalValue;
		}
	}

	// otherwise use legacy weapon script value
	index = pWeaponInfo->GetIndexofAttribute( AllocPooledString( iszAttrib ) );

	if ( index == -1 )
	{
		return flLocalValue;
	}
	else if ( index > -1 )
	{
		Assert( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType == FIELD_FLOAT );
		if ( pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_fieldType != FIELD_FLOAT )
			return false;

		const float* pFloat;
		pFloat = reinterpret_cast< const float* >( reinterpret_cast< const char* >( pWeaponInfo ) + pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_nWeaponParseDataOffset );

		return flScale * *pFloat;
	}

	return 0;
}

// template < class T >
// T GetAttributeFromSchemaOrScript( const CCSWeaponInfo* pWeaponInfo, const char * iszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView )
// {
// 	uint32 unLocalValue = 0;
// 	int index = -2;
// 
// 	if ( pWepView && pWepView->IsValid() )
// 	{
// 		if ( pWepView->FindAttribute( hAttrib, &unLocalValue ) )
// 		{
// 			return unLocalValue;
// 		}
// 	}
// 
// 	// otherwise use legacy weapon script value
// 	index = pWeaponInfo->GetIndexofAttribute( AllocPooledString( iszAttrib ) );
// 
// 	if ( index == -1 )
// 	{
// 		return false;
// 	}
// 	else if ( index > -1 )
// 	{
// 		const T* pValue;
// 		pValue = reinterpret_cast< const T* >( reinterpret_cast< const char* >( pWeaponInfo ) + pWeaponInfo->ms_vecWeaponInfoLookup[ index ]->m_nWeaponParseDataOffset );
// 
// 		return *pValue;
// 	}
// 
// 	return false;
// }

// Weapon attribute accessor is macroized and templatized so that we can use static attribute def handles 
// because generating them at every access is costly.
//
#define GET_WEAPON_ATTR_FUNC( type, functionname, attrname )										\
type CCSWeaponInfo::functionname( const CEconItemView* pWepView, int nAlt, float flScale ) const	\
{																									\
	const char* pszAttrib;																			\
	static CSchemaAttributeDefHandle hAttrib( attrname );											\
	static CSchemaAttributeDefHandle hAttribAlt( attrname " alt" );									\
	CSchemaAttributeDefHandle * pAttrib;															\
	if ( nAlt )																						\
	{																								\
		pszAttrib = attrname " alt";																\
		pAttrib = &hAttribAlt;																		\
	}																								\
	else																							\
	{																								\
		pszAttrib = attrname;																		\
		pAttrib = &hAttrib;																			\
	}																								\
	return GetAttribute_##type( this, pszAttrib, *pAttrib, pWepView, flScale );						\
}		


GET_WEAPON_ATTR_FUNC( int,		GetWeaponPrice,					"in game price" )
GET_WEAPON_ATTR_FUNC( bool,		IsFullAuto,						"is full auto" )
GET_WEAPON_ATTR_FUNC( bool,		HasSilencer,					"has silencer" )
GET_WEAPON_ATTR_FUNC( int,		GetBullets,						"bullets" )
GET_WEAPON_ATTR_FUNC( float,	GetCycleTime,					"cycletime" )
GET_WEAPON_ATTR_FUNC( float,	GetHeatPerShot,					"heat per shot" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoveryTimeCrouch,			"recovery time crouch" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoveryTimeStand,			"recovery time stand" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoveryTimeCrouchFinal,		"recovery time crouch final" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoveryTimeStandFinal,		"recovery time stand final" )
GET_WEAPON_ATTR_FUNC( int,		GetRecoveryTransitionStartBullet, "recovery transition start bullet" )
GET_WEAPON_ATTR_FUNC( int,		GetRecoveryTransitionEndBullet, "recovery transition end bullet" )

GET_WEAPON_ATTR_FUNC( int,		GetRecoilSeed,					"recoil seed" )
GET_WEAPON_ATTR_FUNC( float,	GetFlinchVelocityModifierLarge, "flinch velocity modifier large" )
GET_WEAPON_ATTR_FUNC( float,	GetFlinchVelocityModifierSmall, "flinch velocity modifier small" )
GET_WEAPON_ATTR_FUNC( float,	GetTimeToIdleAfterFire,			"time to idle" )
GET_WEAPON_ATTR_FUNC( float,	GetIdleInterval,				"idle interval" )
GET_WEAPON_ATTR_FUNC( float,	GetRange,						"range" )
GET_WEAPON_ATTR_FUNC( float,	GetRangeModifier,				"range modifier" )
GET_WEAPON_ATTR_FUNC( int,		GetDamage,						"damage" )
GET_WEAPON_ATTR_FUNC( float,	GetPenetration,					"penetration" )
GET_WEAPON_ATTR_FUNC( int,		GetCrosshairDeltaDistance,		"crosshair delta distance" )
GET_WEAPON_ATTR_FUNC( int,		GetCrosshairMinDistance,		"crosshair min distance" )

GET_WEAPON_ATTR_FUNC( float,	GetMaxSpeed,					"max player speed" )

GET_WEAPON_ATTR_FUNC( float,	GetSpread,						"spread" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyCrouch,			"inaccuracy crouch" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyStand,				"inaccuracy stand" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyJumpInitial,		"inaccuracy jump initial" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyJump,				"inaccuracy jump" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyLand,				"inaccuracy land" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyLadder,			"inaccuracy ladder" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyFire,				"inaccuracy fire" )
GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyMove,				"inaccuracy move" )

GET_WEAPON_ATTR_FUNC( float,	GetInaccuracyReload,			"inaccuracy reload" )

GET_WEAPON_ATTR_FUNC( float,	GetRecoilAngle,					"recoil angle" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoilAngleVariance,			"recoil angle variance" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoilMagnitude,				"recoil magnitude" )
GET_WEAPON_ATTR_FUNC( float,	GetRecoilMagnitudeVariance,		"recoil magnitude variance" )
GET_WEAPON_ATTR_FUNC( int,		GetTracerFrequency,				"tracer frequency" )



GET_WEAPON_ATTR_FUNC( int,		GetPrimaryClipSize,				"primary clip size" )
GET_WEAPON_ATTR_FUNC( int,		GetSecondaryClipSize,			"secondary clip size" )
GET_WEAPON_ATTR_FUNC( int,		GetDefaultPrimaryClipSize,		"primary default clip size" )
GET_WEAPON_ATTR_FUNC( int,		GetDefaultSecondaryClipSize,	"secondary default clip size" )

GET_WEAPON_ATTR_FUNC( int,		GetKillAward,					"kill award" )
GET_WEAPON_ATTR_FUNC( bool,		HasBurstMode,					"has burst mode" )
GET_WEAPON_ATTR_FUNC( bool,		IsRevolver,						"is revolver" )
GET_WEAPON_ATTR_FUNC( bool,		HasAlternateFastSlowReload,		"alternate fastslow reload" )
GET_WEAPON_ATTR_FUNC( float,	GetArmorRatio,					"armor ratio" )
GET_WEAPON_ATTR_FUNC( bool,		HasTraditionalScope,			"traditional scope" )
GET_WEAPON_ATTR_FUNC( bool,		CannotShootUnderwater,			"cannot shoot underwater" )
GET_WEAPON_ATTR_FUNC( bool,		DoesUnzoomAfterShot,			"unzoom after shot" )
GET_WEAPON_ATTR_FUNC( bool,		DoesHideViewModelWhenZoomed,	"hide view model zoomed" )
GET_WEAPON_ATTR_FUNC( int,		GetBucketSlot,					"bucket slot" )
GET_WEAPON_ATTR_FUNC( int,		GetZoomLevels,					"zoom levels" )
GET_WEAPON_ATTR_FUNC( int,		GetZoomFOV1,					"zoom fov 1" )
GET_WEAPON_ATTR_FUNC( int,		GetZoomFOV2,					"zoom fov 2" )
GET_WEAPON_ATTR_FUNC( float,	GetZoomTime0,					"zoom time 0" )
GET_WEAPON_ATTR_FUNC( float,	GetZoomTime1,					"zoom time 1" )
GET_WEAPON_ATTR_FUNC( float,	GetZoomTime2,					"zoom time 2" )

GET_WEAPON_ATTR_FUNC( int,		GetPrimaryReserveAmmoMax,		"primary reserve ammo max" )
GET_WEAPON_ATTR_FUNC( int,		GetSecondaryReserveAmmoMax,		"secondary reserve ammo max" )

WeaponRecoilData::WeaponRecoilData()
{
	m_mapRecoilTables.SetLessFunc( DefLessFunc( item_definition_index_t ) );
}

WeaponRecoilData::~WeaponRecoilData()
{
	m_mapRecoilTables.PurgeAndDeleteElements();
}

static inline float AttrValueAsFloat( attrib_value_t val )
{
	float flValue;
	Q_memcpy( &flValue, &val, sizeof( float ) );
	return flValue;
}

void WeaponRecoilData::GenerateRecoilTable( RecoilData *data )
{
	const int iSuppressionShots = weapon_recoil_suppression_shots.GetInt();
	const float fBaseSuppressionFactor = weapon_recoil_suppression_factor.GetFloat();
	const float fRecoilVariance = weapon_recoil_variance.GetFloat();
	CUniformRandomStream recoilRandom;

	if ( !data )
		return;

	const CEconItemDefinition *pEconItemDefinition = GetItemSchema()->GetItemDefinition( data->iItemDefIndex );
	Assert( pEconItemDefinition );

	// Walk the attributes to determine all things that we need
	int iSeed = 0;
	bool bHasAttrSeed = false;
	bool bFullAuto = false;
	bool bHasAttrFullAuto = false;
	float flRecoilAngle[2] = {};
	bool bHasAttrRecoilAngle[2] = {};
	float flRecoilAngleVariance[2] = {};
	bool bHasAttrRecoilAngleVariance[2] = {};
	float flRecoilMagnitude[2] = {};
	bool bHasAttrRecoilMagnitude[2] = {};
	float flRecoilMagnitudeVariance[2] = {};
	bool bHasAttrRecoilMagnitudeVariance[2] = {};
	CCSWeaponInfo const *pWeaponInfo = NULL;

	if ( ( data->iItemDefIndex >= WEAPON_FIRST ) && ( data->iItemDefIndex <= WEAPON_LAST ) )
	{
		pWeaponInfo = GetWeaponInfo( CSWeaponID( data->iItemDefIndex ) );
	}
	if ( !pWeaponInfo && pEconItemDefinition->GetItemClass() )
	{
		char const *szItemClass = pEconItemDefinition->GetItemClass();
		CSWeaponID wpnId = WeaponIdFromString( szItemClass );
		if ( wpnId != WEAPON_NONE )
		{
			pWeaponInfo = GetWeaponInfo( wpnId );
		}
	}
	if ( pWeaponInfo )
	{
		iSeed = pWeaponInfo->GetRecoilSeed();
		bFullAuto = pWeaponInfo->IsFullAuto();
		for ( int iMode = 0; iMode < 2; ++ iMode )
		{
			flRecoilAngle[iMode] = pWeaponInfo->GetRecoilAngle( NULL, iMode );
			flRecoilAngleVariance[iMode] = pWeaponInfo->GetRecoilAngleVariance( NULL, iMode );
			flRecoilMagnitude[iMode] = pWeaponInfo->GetRecoilMagnitude( NULL, iMode );
			flRecoilMagnitudeVariance[iMode] = pWeaponInfo->GetRecoilMagnitudeVariance( NULL, iMode );
		}
	}

	const CUtlVector< static_attrib_t > &arrAttributes = pEconItemDefinition->GetStaticAttributes();
	Assert( pWeaponInfo || arrAttributes.Count() );
	for ( int j = 0; j < arrAttributes.Count(); ++ j )
	{
		switch ( arrAttributes[j].iDefIndex )
		{
		case 59: // recoil seed
			Assert( !bHasAttrSeed );
			bHasAttrSeed = true;
			iSeed = arrAttributes[j].m_value.asFloat;
			break;
		case 60: // recoil angle
			Assert( !bHasAttrRecoilAngle[0] );
			bHasAttrRecoilAngle[0] = true;
			flRecoilAngle[0] = arrAttributes[j].m_value.asFloat;
			break;
		case 64: // recoil angle alt
			Assert( !bHasAttrRecoilAngle[1] );
			bHasAttrRecoilAngle[1] = true;
			flRecoilAngle[1] = arrAttributes[j].m_value.asFloat;
			break;
		case 61: // recoil angle variance
			Assert( !bHasAttrRecoilAngleVariance[0] );
			bHasAttrRecoilAngleVariance[0] = true;
			flRecoilAngleVariance[0] = arrAttributes[j].m_value.asFloat;
			break;
		case 65: // recoil angle variance alt
			Assert( !bHasAttrRecoilAngleVariance[1] );
			bHasAttrRecoilAngleVariance[1] = true;
			flRecoilAngleVariance[1] = arrAttributes[j].m_value.asFloat;
			break;
		case 62: // recoil magnitude
			Assert( !bHasAttrRecoilMagnitude[0] );
			bHasAttrRecoilMagnitude[0] = true;
			flRecoilMagnitude[0] = arrAttributes[j].m_value.asFloat;
			break;
		case 66: // recoil magnitude alt
			Assert( !bHasAttrRecoilMagnitude[1] );
			bHasAttrRecoilMagnitude[1] = true;
			flRecoilMagnitude[1] = arrAttributes[j].m_value.asFloat;
			break;
		case 63: // recoil magnitude variance
			Assert( !bHasAttrRecoilMagnitudeVariance[0] );
			bHasAttrRecoilMagnitudeVariance[0] = true;
			flRecoilMagnitudeVariance[0] = arrAttributes[j].m_value.asFloat;
			break;
		case 67: // recoil magnitude variance alt
			Assert( !bHasAttrRecoilMagnitudeVariance[1] );
			bHasAttrRecoilMagnitudeVariance[1] = true;
			flRecoilMagnitudeVariance[1] = arrAttributes[j].m_value.asFloat;
			break;
		case 22: // full auto
			Assert( !bHasAttrFullAuto );
			bHasAttrFullAuto = true;
			bFullAuto = ( arrAttributes[j].m_value.asUint32 != 0.0f );
			break;
		}
	}

	for ( int iMode = 0; iMode < 2; ++iMode )
	{
		Assert( pWeaponInfo || ( bHasAttrSeed && bHasAttrFullAuto &&
			bHasAttrRecoilAngle[iMode] && bHasAttrRecoilAngleVariance[iMode] &&
			bHasAttrRecoilMagnitude[iMode] && bHasAttrRecoilMagnitudeVariance[iMode] ) );

		recoilRandom.SetSeed( iSeed );

		float fAngle = 0.0f;
		float fMagnitude = 0.0f;

		for ( int j = 0; j < ARRAYSIZE( data->recoilTable[iMode] ); ++j )
		{
			float fAngleNew = flRecoilAngle[iMode] + recoilRandom.RandomFloat(- flRecoilAngleVariance[iMode], + flRecoilAngleVariance[iMode] );
			float fMagnitudeNew = flRecoilMagnitude[iMode] + recoilRandom.RandomFloat(- flRecoilMagnitudeVariance[iMode], + flRecoilMagnitudeVariance[iMode] );

			if ( bFullAuto && ( j > 0 ) )
			{
				fAngle = Lerp( fRecoilVariance, fAngle, fAngleNew );
				fMagnitude = Lerp( fRecoilVariance, fMagnitude, fMagnitudeNew );
			}
			else
			{
				fAngle = fAngleNew;
				fMagnitude = fMagnitudeNew;
			}

			if ( bFullAuto && ( j < iSuppressionShots ) )
			{
				float fSuppressionFactor = Lerp( (float)j / (float)iSuppressionShots, fBaseSuppressionFactor, 1.0f );
				fMagnitude *= fSuppressionFactor;
			}

			data->recoilTable[iMode][j].fAngle = fAngle;
			data->recoilTable[iMode][j].fMagnitude = fMagnitude;
		}
	}
}

void WeaponRecoilData::GetRecoilOffsets( CWeaponCSBase *pWeapon, int iMode, int iIndex, float& fAngle, float &fMagnitude )
{
	// Recoil offset tables are indexed by a weapon's definition index.
	// Look for the existing table, otherwise generate it.

	item_definition_index_t iDefIndex = pWeapon->GetEconItemView()->GetItemDefinition()->GetDefinitionIndex();

	RecoilData *wepData = NULL;
	CUtlMap< item_definition_index_t, RecoilData* >::IndexType_t iMapLocation = m_mapRecoilTables.Find( iDefIndex );
	if ( iMapLocation == m_mapRecoilTables.InvalidIndex() )
	{
		Assert( !"Generating recoil table too late" ); // failed to find recoil table, need to re-generate!
		wepData = new RecoilData;
		wepData->iItemDefIndex = iDefIndex;
		iMapLocation = m_mapRecoilTables.InsertOrReplace( iDefIndex, wepData );
		GenerateRecoilTable( wepData );
	}
	else
	{
		wepData = m_mapRecoilTables.Element( iMapLocation );
		Assert( wepData );
		Assert( wepData->iItemDefIndex == iDefIndex );
	}

	iIndex = iIndex % ARRAYSIZE( wepData->recoilTable[iMode] );
	fAngle = wepData->recoilTable[iMode][iIndex].fAngle;
	fMagnitude = wepData->recoilTable[iMode][iIndex].fMagnitude;
}

void WeaponRecoilData::GenerateRecoilPatternForItemDefinition( item_definition_index_t idx )
{
	CUtlMap< item_definition_index_t, RecoilData* >::IndexType_t iMapLocation = m_mapRecoilTables.Find( idx );
	if ( iMapLocation == m_mapRecoilTables.InvalidIndex() )
	{
		RecoilData *wepData = new RecoilData;
		wepData->iItemDefIndex = idx;
		iMapLocation = m_mapRecoilTables.InsertOrReplace( idx, wepData );
		GenerateRecoilTable( wepData );
	}
}

WeaponRecoilData g_WeaponRecoilData;

void GenerateWeaponRecoilPatternForItemDefinition( item_definition_index_t idx )
{
	g_WeaponRecoilData.GenerateRecoilPatternForItemDefinition( idx );
}




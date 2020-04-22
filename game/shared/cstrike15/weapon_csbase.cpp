//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Laser Rifle & Shield combo
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "weapon_csbase.h"
#include "ammodef.h"
#include "cs_gamerules.h"
#include "basegrenade_shared.h"
#include "weapon_basecsgrenade.h"
#include "platforminputdevice.h"
#include "inputsystem/iinputsystem.h"


// CS-PRO TEST CHANGE: instant movement inaccuracy, curve exponent x^0.25
#define MOVEMENT_ACCURACY_DECAYED	0
#define MOVEMENT_WALK_CURVE01_EXPONENT   0.85
#define MOVEMENT_CURVE01_EXPONENT   0.25

#define SILENCER_VISIBLE 0
#define SILENCER_HIDDEN 1

#ifdef SIXENSE
#include "sixense\in_sixense.h"
#include "view.h"
#endif

#if defined( CLIENT_DLL )

	#include "vgui/ISurface.h"
	#include "vgui_controls/Controls.h"
	#include "c_cs_player.h"
	#include "predicted_viewmodel.h"
	#include "hud_crosshair.h"
	#include "c_te_effect_dispatch.h"
	#include "c_te_legacytempents.h"
	#include "cs_hud_weaponselection.h"
	#include "prediction.h"
	#include "materialsystem/icompositetexturegenerator.h"
	#include "materialsystem/icustommaterialmanager.h"
	#include "cs_custom_material_swap.h"
	#include "cs_custom_weapon_visualsdata_processor.h"
	//#include "glow_outline_effect.h"
	#include "HUD/sfhudreticle.h"

	extern IVModelInfoClient* modelinfo;

	extern ConVar spec_show_xray;

#else

	#include "cs_player.h"
	#include "te_effect_dispatch.h"
	#include "keyvalues.h"
	#include "cs_ammodef.h"
	#include "cvisibilitymonitor.h"

	extern IVModelInfo* modelinfo;

#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#if defined( CSTRIKE15 )

extern ConVar crosshair;

extern WeaponRecoilData g_WeaponRecoilData;

#endif

extern ConVar cl_righthand;
extern ConVar mp_weapons_allow_map_placed;
extern ConVar mp_weapons_glow_on_ground;
extern ConVar sv_jump_impulse;

ConVar weapon_land_dip_amt( "weapon_land_dip_amt", "20.0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_REPLICATED, "The amount the gun should dip when the player lands after a jump." );

ConVar weapon_recoil_decay1( "weapon_recoil_decay1_exp", "3.5", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Decay factor exponent for weapon recoil" );
ConVar weapon_recoil_decay2_exp( "weapon_recoil_decay2_exp", "8", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Decay factor exponent for weapon recoil" );
ConVar weapon_recoil_decay2_lin( "weapon_recoil_decay2_lin", "18", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Decay factor (linear term) for weapon recoil" );
ConVar weapon_recoil_vel_decay( "weapon_recoil_vel_decay", "4.5", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Decay factor for weapon recoil velocity" );

ConVar weapon_recoil_decay_coefficient( "weapon_recoil_decay_coefficient", "2.0", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "" );

ConVar weapon_accuracy_nospread( "weapon_accuracy_nospread", "0", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Disable weapon inaccuracy spread" );
ConVar weapon_recoil_cooldown( "weapon_recoil_cooldown", "0.55", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "DEPRECATED. Recoil now decays using weapon_recoil_decay_coefficient");
ConVar weapon_recoil_scale( "weapon_recoil_scale", "2.0", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Overall scale factor for recoil. Used to reduce recoil on specific platforms");
ConVar weapon_recoil_scale_motion_controller( "weapon_recoil_scale_motion_controller", "1.0", FCVAR_RELEASE | FCVAR_CHEAT |FCVAR_REPLICATED, "Overall scale factor for recoil. Used to reduce recoil.  Only for motion controllers");
ConVar weapon_air_spread_scale( "weapon_air_spread_scale", "1.0", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Scale factor for jumping inaccuracy, set to 0 to make jumping accuracy equal to standing", true, 0.0f, false, 1.0f );

ConVar weapon_legacy_recoiltable( "weapon_legacy_recoiltable", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar weapon_reticle_knife_show( "weapon_reticle_knife_show", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "When enabled will show knife reticle on clients. Used for game modes requiring target id display when holding a knife." );

ConVar weapon_auto_cleanup_time( "weapon_auto_cleanup_time", "0", FCVAR_RELEASE, "If set to non-zero, weapons will delete themselves after the specified time (in seconds) if no players are near." );
ConVar weapon_max_before_cleanup( "weapon_max_before_cleanup", "0", FCVAR_RELEASE, "If set to non-zero, will remove the oldest dropped weapon to maintain the specified number of dropped weapons in the world." );


#if defined( CLIENT_DLL )
ConVar cl_weapon_debug_print_accuracy("cl_weapon_debug_print_accuracy", "0", FCVAR_RELEASE | FCVAR_CHEAT );
ConVar cl_weapon_debug_show_accuracy("cl_weapon_debug_show_accuracy", "0", FCVAR_RELEASE | FCVAR_CHEAT, "Draws a circle representing the effective range with every shot." );
ConVar cl_weapon_debug_show_accuracy_duration( "cl_weapon_debug_show_accuracy_duration", "10", FCVAR_RELEASE | FCVAR_CHEAT );
#endif

#define MP_WEAPON_ACCURACY_SEPARATE_WALK_FUNCTION true
	
/*
// TODO[pmf] make these work with recoil tables
ConVar weapon_recoil_seed( "weapon_recoil_seed", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_REPLICATED  );

#if defined( GAME_DLL )
CON_COMMAND_F( weapon_recoil_reseed, "Randomly generate a new recoil seed for the current weapon", FCVAR_DEVELOPMENTONLY )
{
	int iNewSeed = RandomInt(0, 0xFFFF);
	weapon_recoil_seed.SetValue(iNewSeed);

	Msg("Recoil seed set to %i\n", iNewSeed);
}
#endif
*/


void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay, int nLightIndex = LIGHT_INDEX_TE_DYNAMIC );

// ----------------------------------------------------------------------------- //
// Global functions.
// ----------------------------------------------------------------------------- //

struct WeaponAliasTranslationInfoStruct
{
	char* alias;
	char* translatedAlias;
};

static const WeaponAliasTranslationInfoStruct s_WeaponAliasTranslationInfo[] = 
{
	{ "cv47", "ak47" },
	{ "defender", "galil" },
	{ "krieg552", "sg552" },
	{ "magnum", "awp" },
	{ "d3au1", "g3sg1" },
	{ "clarion", "famas" },
	{ "bullpup", "aug" },
	{ "krieg550", "sg550" },
	{ "9x19mm", "glock" },
	{ "km45", "usp" },
	{ "228compact", "p228" },
	{ "nighthawk", "deagle" },
	{ "elites", "elite" },
	{ "fn57", "fiveseven" },
	{ "12gauge", "m3" },
	{ "autoshotgun", "xm1014" },
	{ "mp", "tmp" },
	{ "smg", "mp5navy" },
	{ "mp5", "mp5navy" },
	{ "c90", "p90" },
	{ "vest", "kevlar" },
	{ "vesthelm", "assaultsuit" },
	{ "nvgs", "nightvision" },

	{ "", "" } // this needs to be last
};


struct WeaponAliasInfo
{
	CSWeaponID id;
	const char* alias;
};

bool IsAmmoType( int iAmmoType, const char *pAmmoName )
{
	return GetAmmoDef()->Index( pAmmoName ) == iAmmoType;
}

//--------------------------------------------------------------------------------------------------------
//
// Given an alias, return the translated alias.
//
const char * GetTranslatedWeaponAlias( const char *szAlias )
{
	for ( int i = 0; i < ARRAYSIZE( s_WeaponAliasTranslationInfo ); ++i )
	{
		if ( Q_stricmp( s_WeaponAliasTranslationInfo[i].alias, szAlias ) == 0 )
		{
			return s_WeaponAliasTranslationInfo[i].translatedAlias;
		}
	}

	return szAlias;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a translated alias, return the alias.
//
const char * GetWeaponAliasFromTranslated( const char *translatedAlias )
{
	int i = 0;
	const WeaponAliasTranslationInfoStruct *info = &( s_WeaponAliasTranslationInfo[i] );

	while ( info->alias[0] != 0 )
	{
		if ( Q_stricmp( translatedAlias, info->translatedAlias ) == 0 )
		{
			return info->alias;
		}
		info = &( s_WeaponAliasTranslationInfo[++i] );
	}

	return translatedAlias;
}

//--------------------------------------------------------------------------------------------------------
//
// Return true if given weapon ID is a primary weapon
//
bool IsPrimaryWeapon( CSWeaponID id )
{
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( id );
	if ( pWeaponInfo )
	{
		return pWeaponInfo->iSlot == WEAPON_SLOT_RIFLE;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------
//
// Return true if given weapon ID is a secondary weapon
//
bool IsSecondaryWeapon( CSWeaponID id )
{
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( id );
	if ( pWeaponInfo )
		return pWeaponInfo->iSlot == WEAPON_SLOT_PISTOL;

	return false;
}

//--------------------------------------------------------------------------------------------------------
//
// Return true if given weapon ID is a grenade weapon
//
bool IsGrenadeWeapon( CSWeaponID id )
{
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( id );
	if ( pWeaponInfo )
	{
		return pWeaponInfo->iSlot == WEAPON_SLOT_GRENADES;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------
//
// Return true if given weapon ID is armor
//
bool IsArmor( CSWeaponID id )
{
	return id == ITEM_ASSAULTSUIT || id == ITEM_KEVLAR;
}

#ifdef CLIENT_DLL
int GetShellForAmmoType( const char *ammoname )
{
	if ( !Q_strcmp( BULLET_PLAYER_762MM, ammoname ) )
		return CS_SHELL_762NATO;

	if ( !Q_strcmp( BULLET_PLAYER_556MM, ammoname ) )
		return CS_SHELL_556;

	if ( !Q_strcmp( BULLET_PLAYER_338MAG, ammoname ) )
		return CS_SHELL_338MAG;

	if ( !Q_strcmp( BULLET_PLAYER_BUCKSHOT, ammoname ) )
		return CS_SHELL_12GAUGE;

	if ( !Q_strcmp( BULLET_PLAYER_57MM, ammoname ) )
		return CS_SHELL_57;

	// default 9 mm
	return CS_SHELL_9MM;
}
#endif


// ----------------------------------------------------------------------------- //
// CWeaponCSBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCSBase, DT_WeaponCSBase )

BEGIN_NETWORK_TABLE( CWeaponCSBase, DT_WeaponCSBase )
#if !defined( CLIENT_DLL )
SendPropInt( SENDINFO( m_weaponMode ), 1, SPROP_UNSIGNED ),
SendPropFloat( SENDINFO( m_fAccuracyPenalty ), 0, SPROP_CHANGES_OFTEN ),
SendPropFloat( SENDINFO( m_fLastShotTime ) ),
//SendPropInt( SENDINFO( m_iRecoilIndex ) ),	//DEPRECATED
SendPropFloat( SENDINFO( m_flRecoilIndex ) ),
// world weapon models have no aminations
SendPropExclude( "DT_AnimTimeMustBeFirst", "m_flAnimTime" ),
SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
SendPropEHandle( SENDINFO( m_hPrevOwner ) ),
SendPropBool( SENDINFO( m_bBurstMode ) ),
SendPropTime( SENDINFO( m_flPostponeFireReadyTime ) ),
SendPropBool( SENDINFO( m_bReloadVisuallyComplete ) ),
//	SendPropExclude( "DT_LocalActiveWeaponData", "m_flTimeWeaponIdle" ),
SendPropBool( SENDINFO( m_bSilencerOn ) ),
SendPropTime( SENDINFO( m_flDoneSwitchingSilencer ) ),
SendPropInt( SENDINFO( m_iOriginalTeamNumber ) ),
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
SendPropFloat( SENDINFO( m_fAccuracyFishtail ) ),
#endif
#ifdef IRONSIGHT
SendPropInt( SENDINFO( m_iIronSightMode ), 2, SPROP_UNSIGNED ),
#endif //IRONSIGHT
#else
RecvPropInt( RECVINFO( m_weaponMode ) ),
RecvPropFloat( RECVINFO( m_fAccuracyPenalty ) ),
RecvPropFloat( RECVINFO( m_fLastShotTime ) ),
RecvPropInt( RECVINFO( m_iRecoilIndex ) ),	// DEPRECATED. Kept for old demo compatibility.
RecvPropFloat( RECVINFO( m_flRecoilIndex ) ),
RecvPropEHandle( RECVINFO( m_hPrevOwner ) ),
RecvPropBool( RECVINFO( m_bBurstMode ) ),
RecvPropTime( RECVINFO( m_flPostponeFireReadyTime ) ),
RecvPropBool( RECVINFO( m_bReloadVisuallyComplete ) ),
RecvPropBool( RECVINFO( m_bSilencerOn ) ),
RecvPropTime( RECVINFO( m_flDoneSwitchingSilencer ) ),
RecvPropInt( RECVINFO( m_iOriginalTeamNumber ) ),
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
RecvPropFloat( RECVINFO( m_fAccuracyFishtail ) ),
#endif
#ifdef IRONSIGHT
RecvPropInt( RECVINFO( m_iIronSightMode ) ),
#endif //IRONSIGHT
#endif
END_NETWORK_TABLE()

#if defined( CLIENT_DLL )
BEGIN_PREDICTION_DATA( CWeaponCSBase )
	DEFINE_PRED_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flNextPrimaryAttack, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flNextSecondaryAttack, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_weaponMode, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_fAccuracyPenalty, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
	DEFINE_PRED_FIELD_TOL( m_fAccuracyFishtail, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
#endif
	DEFINE_PRED_FIELD( m_fLastShotTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iRecoilIndex, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flRecoilIndex, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bReloadVisuallyComplete, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#ifdef IRONSIGHT
	DEFINE_PRED_FIELD( m_iIronSightMode, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
#endif
	DEFINE_PRED_FIELD( m_flPostponeFireReadyTime, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()
#endif


LINK_ENTITY_TO_CLASS_ALIASED( weapon_cs_base, WeaponCSBase );


#ifdef GAME_DLL

#define REMOVEUNOWNEDWEAPON_THINK_CONTEXT			"WeaponCSBase_RemoveUnownedWeaponThink"
#define REMOVEUNOWNEDWEAPON_THINK_INTERVAL			0.2
#define REMOVEUNOWNEDWEAPON_THINK_REMOVE			3

	BEGIN_DATADESC( CWeaponCSBase )

		//DEFINE_FUNCTION( DefaultTouch ),
		DEFINE_THINKFUNC( FallThink ),
		DEFINE_THINKFUNC( RemoveUnownedWeaponThink ),

		DEFINE_KEYFIELD( m_bCanBePickedUp, FIELD_BOOLEAN, "CanBePickedUp" )

	END_DATADESC()

#endif

#if defined( CLIENT_DLL )
	ConVar cl_crosshairstyle( "cl_crosshairstyle", "2", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS, "0 = DEFAULT, 1 = DEFAULT STATIC, 2 = ACCURATE SPLIT (accurate recoil/spread feedback with a fixed inner part), 3 = ACCURATE DYNAMIC (accurate recoil/spread feedback), 4 = CLASSIC STATIC, 5 = OLD CS STYLE (fake recoil - inaccurate feedback)" );
	ConVar cl_crosshaircolor( "cl_crosshaircolor", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Set crosshair color as defined in game_options.consoles.txt" );
	//ConVar cl_dynamiccrosshair( "cl_dynamiccrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_scalecrosshair( "cl_scalecrosshair", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS, "Enable crosshair scaling (deprecated)" );
	ConVar cl_crosshairscale( "cl_crosshairscale", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS, "Crosshair scaling factor (deprecated)" );
	ConVar cl_crosshairalpha( "cl_crosshairalpha", "200", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshairusealpha( "cl_crosshairusealpha", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshairgap( "cl_crosshairgap", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshairgap_useweaponvalue( "cl_crosshairgap_useweaponvalue", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If set to 1, the gap will update dynamically based on which weapon is currently equipped" );
	ConVar cl_crosshairsize( "cl_crosshairsize", "5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshairthickness( "cl_crosshairthickness", "0.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshairdot( "cl_crosshairdot", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshaircolor_r( "cl_crosshaircolor_r", "50", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshaircolor_g( "cl_crosshaircolor_g", "250", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar cl_crosshaircolor_b( "cl_crosshaircolor_b", "50", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS );
	ConVar weapon_debug_spread_show( "weapon_debug_spread_show", "0", FCVAR_CLIENTDLL | FCVAR_SS | FCVAR_CHEAT, "Enables display of weapon accuracy; 1: show accuracy box, 3: show accuracy with dynamic crosshair" );
	ConVar weapon_debug_spread_gap( "weapon_debug_spread_gap", "0.67", FCVAR_CLIENTDLL  | FCVAR_SS | FCVAR_CHEAT );
	ConVar cl_crosshair_drawoutline( "cl_crosshair_drawoutline", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS, "Draws a black outline around the crosshair for better visibility" );
	ConVar cl_crosshair_outlinethickness( "cl_crosshair_outlinethickness", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE  | FCVAR_SS, "Set how thick you want your crosshair outline to draw (0.1-3)", true, 0.1, true, 3 );

	ConVar cl_crosshair_dynamic_splitdist("cl_crosshair_dynamic_splitdist", "7", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If using cl_crosshairstyle 2, this is the distance that the crosshair pips will split into 2. (default is 7)"/*, true, 10, true, 3*/);
	ConVar cl_crosshair_dynamic_splitalpha_innermod("cl_crosshair_dynamic_splitalpha_innermod", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If using cl_crosshairstyle 2, this is the alpha modification that will be used for the INNER crosshair pips once they've split. [0 - 1]", true, 0, true, 1);
	ConVar cl_crosshair_dynamic_splitalpha_outermod("cl_crosshair_dynamic_splitalpha_outermod", "0.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If using cl_crosshairstyle 2, this is the alpha modification that will be used for the OUTER crosshair pips once they've split. [0.3 - 1]", true, 0.3, true, 1);
	ConVar cl_crosshair_dynamic_maxdist_splitratio("cl_crosshair_dynamic_maxdist_splitratio", "0.35", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If using cl_crosshairstyle 2, this is the ratio used to determine how long the inner and outer xhair pips will be. [inner = cl_crosshairsize*(1-cl_crosshair_dynamic_maxdist_splitratio), outer = cl_crosshairsize*cl_crosshair_dynamic_maxdist_splitratio]  [0 - 1]", true, 0, true, 1);


void DrawCrosshairRect( int r, int g, int b, int a, int x0, int y0, int x1, int y1, bool bAdditive )
{
	if ( cl_crosshair_drawoutline.GetBool() )
	{
		float flThick = cl_crosshair_outlinethickness.GetFloat();
		vgui::surface()->DrawSetColor( 0, 0, 0, a );
		vgui::surface()->DrawFilledRect( x0-flThick, y0-flThick, x1+flThick, y1+flThick );
	}

	vgui::surface()->DrawSetColor( r, g, b, a );

	if ( bAdditive )
	{
		vgui::surface()->DrawTexturedRect( x0, y0, x1, y1 );
	}
	else
	{
		// Alpha-blended crosshair
		vgui::surface()->DrawFilledRect( x0, y0, x1, y1 );
	}
}

#endif

// must be included after the above macros
#ifndef CLIENT_DLL
	#include "cs_bot.h"
#endif

// ----------------------------------------------------------------------------- //
// CWeaponCSBase implementation. 
// ----------------------------------------------------------------------------- //
CWeaponCSBase::CWeaponCSBase()
#ifdef CLIENT_DLL
	: m_GlowObject( this, Vector( 1.0f, 1.0f, 1.0f ), 0.0f, false, false )
#endif
{
	m_nWeaponID = WEAPON_NONE;
	SetPredictionEligible( true );
	m_nextOwnerTouchTime = 0.0f;
	m_nextPrevOwnerTouchTime = 0.0f;
	m_hPrevOwner = NULL;
	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.

	m_bCanBePickedUp = true;

	m_fAccuracyPenalty = 0.0f;
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
	m_fAccuracyFishtail = 0.0f;
#endif
	m_fLastShotTime = 0.0f;
	m_weaponMode = Primary_Mode;
	m_fAccuracySmoothedForZoom = 0.0f;
	m_fScopeZoomEndTime = 0.0f;
	m_bBurstMode = false;

	ResetPostponeFireReadyTime();

	m_bWasOwnedByCT = false;
	m_bWasOwnedByTerrorist = false;

#ifdef CLIENT_DLL
	m_bOldFirstPersonSpectatedState = false;

	// This will make it so the weapons get lit with the same ambient cube that the player gets lit with.
	SetUseParentLightingOrigin( true );
	m_iCrosshairTextureID = 0;
	m_flGunAccuracyPosition = 0;

	m_bVisualsDataSet = false;

	m_flLastClientFireBulletTime = 0;
#else

	m_iDefaultExtraAmmo = 0;
	m_bFiredOutOfAmmoEvent = false;
	m_numRemoveUnownedWeaponThink = 0;

#endif

	m_bReloadVisuallyComplete = false;

	m_bSilencerOn = false;	
	m_flDoneSwitchingSilencer = 0.0f;

	m_iOriginalTeamNumber = 0;

	ResetGunHeat();
	
#ifdef IRONSIGHT
	m_iIronSightMode = IronSight_should_approach_unsighted;
	m_IronSightController = NULL;
	UpdateIronSightController();
#endif //IRONSIGHT
	

}

CWeaponCSBase::~CWeaponCSBase()
{

#ifdef IRONSIGHT
	delete m_IronSightController;
	m_IronSightController = NULL;
#endif //IRONSIGHT

#ifndef CLIENT_DLL
	if ( CSGameRules() )
		CSGameRules()->RemoveDroppedWeaponFromList( this );
#endif
}

void CWeaponCSBase::ResetGunHeat()
{

#ifdef CLIENT_DLL

	m_gunHeat = 0.0f;
	m_smokeAttachments = 0x0;
	m_lastSmokeTime = 0.0f;

#endif

}


#ifndef CLIENT_DLL
bool CWeaponCSBase::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( !BaseClass::KeyValue( szKeyName, szValue ) )
	{
		if ( FStrEq( szKeyName, "ammo" ) )
		{
			int bullets = atoi( szValue );
			if ( bullets < 0 )
				return false;

			m_iDefaultExtraAmmo = bullets;

			return true;
		}
	}

	return false;
}
#endif


bool CWeaponCSBase::IsPredicted() const
{ 
	return true;
}


bool CWeaponCSBase::IsPistol() const
{
	return GetWeaponType() == WEAPONTYPE_PISTOL;
}


bool CWeaponCSBase::PlayEmptySound()
{
	//MIKETODO: certain weapons should override this to make it empty:
	//	C4
	//	Flashbang
	//	HE Grenade
	//	Smoke grenade				

	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();

	if ( IsPistol() )
	{
		EmitSound( filter, entindex(), "Default.ClipEmpty_Pistol" );
	}
	else
	{
		EmitSound( filter, entindex(), "Default.ClipEmpty_Rifle" );
	}

	return 0;
}

const char *CWeaponCSBase::GetShootSound( int iIndex ) const
{
	CEconItemView *pItem = ( (CWeaponCSBase *)this )->GetEconItemView();
	if ( pItem && pItem->IsValid() )
	{
		const char *pszSound = pItem->GetStaticData()->GetWeaponReplacementSound( (WeaponSound_t)iIndex );
		if ( pszSound )
		{
			return pszSound;
		}
	}

	return BaseClass::GetShootSound(iIndex);
}

const char *CWeaponCSBase::GetPlayerAnimationExtension( void ) const
{
	CEconItemView *pItem = ( (CWeaponCSBase *)this )->GetEconItemView();
	if ( pItem && pItem->IsValid() )
	{
		return GetCSWpnData().GetPlayerAnimationExtension( pItem );
	}

	return GetCSWpnData().GetPlayerAnimationExtension();
}

const char *CWeaponCSBase::GetAddonModel( void ) const
{
	CEconItemView *pItem = ( (CWeaponCSBase *)this )->GetEconItemView();
	if ( pItem && pItem->IsValid() )
	{
		return GetCSWpnData().GetAddonModel( pItem );
	}

	return GetCSWpnData().GetAddonModel();
}

CCSPlayer* CWeaponCSBase::GetPlayerOwner() const
{
	return dynamic_cast< CCSPlayer* >( GetOwner() );
}

#ifdef CLIENT_DLL
// -----------------------------------------------------------------------------
// Purpose:
// -----------------------------------------------------------------------------
C_BaseEntity *CWeaponCSBase::GetWeaponForEffect()
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return NULL;

	bool bThird = false;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		if ( GetPlayerOwner()->GetPlayerRenderMode( nSlot ) == PLAYER_RENDER_THIRDPERSON )
		{
			bThird = true;
		}
	}

	if ( pLocalPlayer == GetPlayerOwner() && !bThird )
		return pLocalPlayer->GetViewModel();

	return this;
}

void CWeaponCSBase::UpdateOutlineGlow( void )
{
	Vector glowColor;
	bool bRender = false;
	if ( !GetPlayerOwner() )
	{
		bool bRenderForSpectator = CanSeeSpectatorOnlyTools() && spec_show_xray.GetInt();
		if ( bRenderForSpectator && (GetWeaponType() == WEAPONTYPE_C4) )
		{
			glowColor.x = (240.0f/255.0f);
			glowColor.y = (225.0f/255.0f);
			glowColor.z = (90.0f/255.0f);
			bRender = true;
		}
		else if ( (m_iClip1 > 0 || m_iClip2 > 0) && mp_weapons_glow_on_ground.GetBool() )
		{
			glowColor.x = (224.0f/255.0f);
			glowColor.y = (224.0f/255.0f);
			glowColor.z = (224.0f/255.0f);
			bRender = true;
		}
	}

	// Start glowing
	m_GlowObject.SetRenderFlags( bRender, false );
	m_GlowObject.SetColor( glowColor );
	m_GlowObject.SetAlpha( bRender ? 0.8f : 0.0f );
}
#endif

void CWeaponCSBase::AddToPriorOwnerList( CCSPlayer* pPlayer )
{
	if ( !IsAPriorOwner( pPlayer ) )
	{
		// Add player to prior owner list
		m_PriorOwners.AddToTail( pPlayer );
	}
}

void CWeaponCSBase::AddPriorOwner( CCSPlayer* pPlayer )
{
	if ( pPlayer )
	{
		if ( !IsAPriorOwner( pPlayer ) )
			AddToPriorOwnerList( pPlayer );

		// Assign the team that once owned this weapon
		if ( pPlayer->GetTeamNumber() == TEAM_CT )
		{
			m_bWasOwnedByCT = true;
		}
		else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
		{
			m_bWasOwnedByTerrorist = true;
		}
	}
}

bool CWeaponCSBase::WasOwnedByTeam( int teamNumber )
{
	// Verify if the incoming team matches the team of a previous owner
	switch ( teamNumber )
	{
	case TEAM_CT:
		return m_bWasOwnedByCT;

	case TEAM_TERRORIST:
		return m_bWasOwnedByTerrorist;
	}

	return false;
}


bool CWeaponCSBase::IsAPriorOwner( CCSPlayer* pPlayer ) const 
{
	return ( m_PriorOwners.Find( pPlayer ) != -1 );
}

void CWeaponCSBase::SecondaryAttack( void )
{
#ifndef CLIENT_DLL
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	if ( pPlayer->HasShield() == false )
		 BaseClass::SecondaryAttack();
	else
	{
		pPlayer->SetShieldDrawnState( !pPlayer->IsShieldDrawn() );

		if ( pPlayer->IsShieldDrawn() )
			 SendWeaponAnim( ACT_SHIELD_UP );
		else 
			 SendWeaponAnim( ACT_SHIELD_DOWN );

		m_flNextSecondaryAttack = gpGlobals->curtime + 0.4;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.4;
	}
#endif
}

bool CWeaponCSBase::SendWeaponAnim( int iActivity )
{
#ifdef CS_SHIELD_ENABLED
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( pPlayer && pPlayer->HasShield() )
	{
		CBaseViewModel *vm = pPlayer->GetViewModel( 1 );
	
		if ( vm == NULL )
			return false;
	
		vm->SetWeaponModel( SHIELD_VIEW_MODEL, this );

		int	idealSequence = vm->SelectWeightedSequence( ( Activity )iActivity );
		
		if ( idealSequence >= 0 )
		{
			vm->SendViewModelMatchingSequence( idealSequence );
		}
	} 
#endif

	return BaseClass::SendWeaponAnim( iActivity );
}

void CWeaponCSBase::SendViewModelAnim( int nSequence )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer || pPlayer->IsTaunting() || pPlayer->IsLookingAtWeapon() )
		return;

	CBaseViewModel *vm = pPlayer->GetViewModel( m_nViewModelIndex );
	if (vm)
	{
		bool bIsLookingAt = ( vm->GetSequence() != ACT_INVALID && V_stristr( vm->GetSequenceName(vm->GetSequence()), "lookat" ) );

		if ( vm->GetCycle() < 0.98f && bIsLookingAt && V_stristr( vm->GetSequenceName( nSequence ), "idle" ) )
		{
			// Don't switch from taunt to idle
			return;
		}

#ifdef CLIENT_DLL
		if ( !bIsLookingAt )
		{
			//Fade down stat trak glow if we're doing anything other than inspecting
			vm->SetStatTrakGlowMultiplier( 0.0f );
		}
#endif

	}	

	BaseClass::SendViewModelAnim( nSequence );
}

// Common code put here to support separate zoom from silencer/burst
void CWeaponCSBase::CallSecondaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	if ( m_iClip2 != -1 && !GetReserveAmmoCount( AMMO_POSITION_SECONDARY ) )
	{
		m_bFireOnEmpty = TRUE;
	}

	if ( pPlayer->HasShield() )
		CWeaponCSBase::SecondaryAttack();
	else
		SecondaryAttack();
}

void CWeaponCSBase::UpdateGunHeat( float heat, int iAttachmentIndex )
{
	
#ifdef CLIENT_DLL

	static const float SECONDS_FOR_COOL_DOWN = 2.0f;
	static const float MIN_TIME_BETWEEN_SMOKES = 4.0f;

	float currentShotTime = gpGlobals->curtime;
	float timeSinceLastShot = currentShotTime - m_fLastShotTime;
	
	// Drain off any heat from prior shots.
	m_gunHeat -= timeSinceLastShot * ( 1.0f/SECONDS_FOR_COOL_DOWN );
	if ( m_gunHeat <= 0.0f )
	{
		m_gunHeat = 0.0f;
	}

	// Add the new heat to the gun.
	m_gunHeat += heat;
	if ( m_gunHeat > 1.0f )
	{
		// Reset the heat so we have to build up to it again.
		m_gunHeat = 0.0f;
		m_smokeAttachments |= ( 0x1 << iAttachmentIndex );
	}
	
	// Logic for the gun smoke.
	if ( m_smokeAttachments != 0x0 )
	{
		// We don't want to hammer the smoke effect too much, so prevent smoke from spawning too soon after the last smoke.
		if ( currentShotTime - m_lastSmokeTime > MIN_TIME_BETWEEN_SMOKES )
		{
			const char *pszHeatEffect = GetCSWpnData().GetHeatEffectName();

			if ( pszHeatEffect && Q_strlen( pszHeatEffect ) > 0 )
			{
				static const int MAX_SMOKE_ATTACHMENT_INDEX = 16;
				for ( int i=0; i<MAX_SMOKE_ATTACHMENT_INDEX && m_smokeAttachments > 0x0; ++i )
				{
					int attachmentFlag = ( 0x1<<i );
					if ( ( attachmentFlag & m_smokeAttachments ) > 0x0 )
					{
						//Remove the attachment flag from the smoke attachments since we are firing it off.
						m_smokeAttachments = ( m_smokeAttachments & ( ~attachmentFlag ) );

						CCSPlayer *pPlayer = GetPlayerOwner();
						if ( pPlayer )
						{
							C_BaseViewModel *pViewModel = pPlayer->GetViewModel();
							if ( pViewModel )
							{
								// Dispatch this effect to the split screens that are rendering this first person view model.
								int nSlot = GetViewModelSplitScreenSlot( pViewModel );
								DispatchParticleEffect( pszHeatEffect, PATTACH_POINT_FOLLOW, pViewModel, i, false, nSlot );
								m_lastSmokeTime = currentShotTime;
							}
						}
					}
				}
			}
			//Reset the smoke attachments so that we can start doing a smoke effect for later shots.
			m_smokeAttachments = 0x0;
		}

	}

#endif

}



void CWeaponCSBase::ItemPostFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	UpdateAccuracyPenalty();

	UpdateShieldState();

	if ( ( m_bInReload ) && ( pPlayer->m_flNextAttack <= gpGlobals->curtime ))
	{
		// the AE_WPN_COMPLETE_RELOAD event should handle the stocking the clip, but in case it's missing, we can do it here as well
		int j = MIN( GetMaxClip1() - m_iClip1, GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );	
		// Add them to the clip
		m_iClip1 += j;
		GiveReserveAmmo( AMMO_POSITION_PRIMARY, -j, true );

		m_bInReload = false;
	}

	if ( (pPlayer->m_nButtons & IN_ATTACK ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) )
	{
		ItemPostFrame_ProcessPrimaryAttack( pPlayer );
	}
	else if ( ( pPlayer->m_nButtons & IN_ZOOM ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		if ( ItemPostFrame_ProcessZoomAction( pPlayer ) )
			pPlayer->m_nButtons &= ~IN_ZOOM;
	}
	else if ( (pPlayer->m_nButtons & IN_ATTACK2 ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ))
	{
		if ( ItemPostFrame_ProcessSecondaryAttack( pPlayer ) )
			pPlayer->m_nButtons &= ~IN_ATTACK2;
	}
	else if ( pPlayer->m_nButtons & IN_RELOAD && GetMaxClip1() != WEAPON_NOCLIP && !m_bInReload && m_flNextPrimaryAttack < gpGlobals->curtime ) 
	{
		ItemPostFrame_ProcessReloadAction( pPlayer );
	}
	else if ( !( pPlayer->m_nButtons & ( IN_ATTACK|IN_ATTACK2|IN_ZOOM ) ) )
	{
		ItemPostFrame_ProcessIdleNoAction( pPlayer );
	}
}


void CWeaponCSBase::ItemPostFrame_ProcessPrimaryAttack( CCSPlayer *pPlayer )
{
	if ( ( m_iClip1 == 0 ) || ( GetMaxClip1() == -1 && !GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) ) )
	{
		m_bFireOnEmpty = TRUE;
	}

	if ( CSGameRules()->IsFreezePeriod() )	// Can't shoot during the freeze period
		return;

	if ( pPlayer->m_bIsDefusing )
		return;

	if ( pPlayer->State_Get() != STATE_ACTIVE )
		return;

	if ( pPlayer->IsShieldDrawn() )
		return;

	// don't repeat fire if this is not a full auto weapon or it's clip is empty
	if ( pPlayer->m_iShotsFired > 0 && ( !IsFullAuto() || m_iClip1 == 0 ) )
		return;

	// ignore attack if we're waiting for the attack button to be released
	if ( pPlayer->m_bWaitForNoAttack )
		return;

	if ( !CanPrimaryAttack() )
		return;


	if ( IsRevolver() ) // holding primary, will fire when time is elapsed
	{

		// don't allow a rapid fire shot instantly in the middle of a haul back hold, let the hammer return first
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.25f;

		if ( GetActivity() != ACT_VM_HAULBACK )
		{
			ResetPostponeFireReadyTime();
			BaseClass::SendWeaponAnim( ACT_VM_HAULBACK );
			return;
		}

		m_weaponMode = Primary_Mode;

		if ( !IsPostponFireReadyTimeElapsed() )
			return;

		if ( m_bFireOnEmpty )
		{
			ResetPostponeFireReadyTime();
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		}

		// we're going to fire after this point

	}

	PrimaryAttack();
	m_fLastShotTime = gpGlobals->curtime;

	if ( IsRevolver() )
	{
		// we just fired.
		// there's a bit of a cool-off before you can alt-fire at normal alt-fire rate
		m_flNextSecondaryAttack = gpGlobals->curtime + ( GetCycleTime( Secondary_Mode ) * 1.7f );
	}
}

bool CWeaponCSBase::ItemPostFrame_ProcessZoomAction( CCSPlayer *pPlayer )
{
	if ( IsRevolver() )	// Revolver treats zoom as secondary fire
		return ItemPostFrame_ProcessSecondaryAttack( pPlayer );

	if ( HasZoom() || IsKindOf( WEAPONTYPE_KNIFE ) )
	{
		CallSecondaryAttack();
	}
	return true;
}

bool CWeaponCSBase::ItemPostFrame_ProcessSecondaryAttack( CCSPlayer *pPlayer )
{
	if ( IsRevolver() )
	{
		if ( CSGameRules()->IsFreezePeriod() )	// you can't fire the revolver in freezetime
			return false;

		if ( pPlayer->m_bIsDefusing )
			return false;

		if ( pPlayer->State_Get() != STATE_ACTIVE )
			return false;

		if ( ( m_iClip1 == 0 ) || ( GetMaxClip1() == -1 && !GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) ) )
		{
			m_bFireOnEmpty = TRUE;
		}

		m_weaponMode = Secondary_Mode;

		if ( !m_bFireOnEmpty )
		{
			ResetPostponeFireReadyTime();

			if ( GetActivity() == ACT_VM_HAULBACK )
			{
				BaseClass::SendWeaponAnim( ACT_VM_IDLE );
				return false;
			}
		}

		if ( pPlayer->m_iShotsFired > 0 ) // revolver secondary isn't full-auto even though primary is
			return false; // shots fired is zeroed when the buttons release

		if ( m_bFireOnEmpty )
		{
			if ( GetActivity() != ACT_VM_HAULBACK )
			{
				ResetPostponeFireReadyTime();
				BaseClass::SendWeaponAnim( ACT_VM_HAULBACK );
				return false;
			}

			if ( !IsPostponFireReadyTimeElapsed() )
				return false;
		}
	}

	CallSecondaryAttack();
	return true;
}

void CWeaponCSBase::ItemPostFrame_ProcessReloadAction( CCSPlayer *pPlayer )
{
	// reload when reload is pressed, or if no buttons are down and weapon is empty.

	//MIKETODO: add code for shields...
	//if ( !FBitSet( m_iWeaponState, WPNSTATE_SHIELD_DRAWN ) )

	ItemPostFrame_RevolverResetHaulback();

	if ( !pPlayer->IsShieldDrawn() )
	{
		if ( Reload() )
		{
#ifndef CLIENT_DLL
			// allow the bots to react to the reload
			pPlayer->SetHasReloaded();
			IGameEvent * event = gameeventmanager->CreateEvent( "weapon_reload" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}
#endif
		}
	}
}

void CWeaponCSBase::ItemPostFrame_ProcessIdleNoAction( CCSPlayer *pPlayer )
{
	ItemPostFrame_RevolverResetHaulback();

	// no fire buttons down
	m_bFireOnEmpty = FALSE;

	// we can fire after this
	pPlayer->m_bWaitForNoAttack = false;

	// set the shots fired to 0 after the player releases a button
	pPlayer->m_iShotsFired = 0;

#ifndef CLIENT_DLL
	if ( CSGameRules() && CSGameRules()->IsPlayingTraining() )
	{
		if ( !m_bFiredOutOfAmmoEvent && m_iClip1 == 0 && !m_bInReload && !pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "weapon_outofammo" );
			if ( event )
			{
				m_bFiredOutOfAmmoEvent = true;
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}
		}
	}
#endif

	if ( gpGlobals->curtime > m_flNextPrimaryAttack && m_iClip1 == 0 && IsUseable() && !( GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD ) && !m_bInReload )
	{
		// Reload if current clip is empty and weapon has waited as long as it has to after firing
		Reload();
#ifndef CLIENT_DLL
		pPlayer->SetHasReloaded();
#endif
		return;
	}

#ifdef IRONSIGHT
	UpdateIronSightController();
	if ( m_iIronSightMode == IronSight_viewmodel_is_deploying && GetActivity() != GetDeployActivity() )
	{
		m_iIronSightMode = IronSight_should_approach_unsighted;
	}
#endif

	WeaponIdle();
}


void CWeaponCSBase::ItemPostFrame_RevolverResetHaulback()
{
	if ( IsRevolver() ) // not holding any weapon buttons
	{
		m_weaponMode = Secondary_Mode;
		ResetPostponeFireReadyTime();
		if ( GetActivity() == ACT_VM_HAULBACK )
		{
			BaseClass::SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}


void CWeaponCSBase::ItemBusyFrame()
{
	UpdateAccuracyPenalty();

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer && !( pPlayer->m_nButtons & ( IN_ATTACK|IN_ATTACK2|IN_ZOOM ) ) )
	{
		// we can fire after this
		pPlayer->m_bWaitForNoAttack = false;
	}

	BaseClass::ItemBusyFrame();
}

float CWeaponCSBase::GetInaccuracy() const
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return 0.0f;

	if ( weapon_accuracy_nospread.GetBool() )
		return 0.0f;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	float fMaxSpeed = GetMaxSpeed();
	if ( fMaxSpeed == 0.0f )
		fMaxSpeed = GetCSWpnData().GetMaxSpeed( GetEconItemView(), 0 );

	float fAccuracy = m_fAccuracyPenalty;

	// CS-PRO TEST FEATURE
	// Adding movement penalty here results in an instaneous penalty that doesn't persist.
#if !MOVEMENT_ACCURACY_DECAYED
	float flVerticalSpeed = abs( pPlayer->GetAbsVelocity().z );

	float flMovementInaccuracyScale = RemapValClamped(pPlayer->GetAbsVelocity().Length2D(), 
		fMaxSpeed * CS_PLAYER_SPEED_DUCK_MODIFIER, 
		fMaxSpeed * 0.95f,							// max out at 95% of run speed to avoid jitter near max speed
		0.0f, 1.0f );

	if ( flMovementInaccuracyScale > 0.0f )
	{
		// power curve only applies at speeds greater than walk
		if (( MP_WEAPON_ACCURACY_SEPARATE_WALK_FUNCTION ) && ( pPlayer->m_bIsWalking ) )
		{
			//flMovementInaccuracyScale *= 1.0;	// reduce inaccuracy when walking or slower. This is commented out because at 1.0, it's a noop but preserved in case a different value is desired.
			//flMovementInaccuracyScale = powf( flMovementInaccuracyScale, float( MOVEMENT_WALK_CURVE01_EXPONENT ) );
		}
		else
		{
			flMovementInaccuracyScale = powf( flMovementInaccuracyScale, float( MOVEMENT_CURVE01_EXPONENT ));
		}


		fAccuracy += flMovementInaccuracyScale * weaponInfo.GetInaccuracyMove( GetEconItemView(), m_weaponMode );
	}

	// If we are in the air/on ladder, add inaccuracy based on vertical speed (maximum accuracy at apex of jump)
	if ( pPlayer->GetGroundEntity() == nullptr )
	{
		float flInaccuracyJumpInitial = weaponInfo.GetInaccuracyJumpInitial( GetEconItemView() ) * weapon_air_spread_scale.GetFloat();
		static const float kMaxFallingPenalty = 2.0f;	// Accuracy is never worse than 2x starting penalty

		// Use sqrt here to make the curve more "sudden" around the accurate point at the apex of the jump
		float fSqrtMaxJumpSpeed = sqrtf( sv_jump_impulse.GetFloat() );
		float fSqrtVerticalSpeed = sqrtf( flVerticalSpeed );

		float flAirSpeedInaccuracy = RemapVal( fSqrtVerticalSpeed,
			fSqrtMaxJumpSpeed * 0.25f,	// Anything less than 6.25% of maximum speed has no additional accuracy penalty for z-motion (6.25% = .25 * .25)
			fSqrtMaxJumpSpeed,			// Penalty at max jump speed
			0.0f,						// No movement-related penalty when close to stopped
			flInaccuracyJumpInitial );	// Movement-penalty at start of jump

		// Clamp to min/max values.  (Don't use RemapValClamped because it makes clamping to > kJumpMovePenalty hard)
		if ( flAirSpeedInaccuracy < 0 )
			flAirSpeedInaccuracy = 0;
		else if ( flAirSpeedInaccuracy > ( kMaxFallingPenalty * flInaccuracyJumpInitial ) )
			flAirSpeedInaccuracy = kMaxFallingPenalty * flInaccuracyJumpInitial;

		// Apply air velocity inaccuracy penalty
		// (There is an additional penalty for being in the air at all applied in UpdateAccuracyPenalty())
		fAccuracy += flAirSpeedInaccuracy;
	}
#endif // !MOVEMENT_ACCURACY_DECAYED

	if ( fAccuracy > 1.0f )
		fAccuracy = 1.0f;

	return fAccuracy;
}


int CWeaponCSBase::GetRecoilSeed( void ) const
{
	CEconItemView *pItem = ( (CWeaponCSBase *)this )->GetEconItemView();
	if ( pItem->IsValid() )
	{
		return GetCSWpnData().GetRecoilSeed( pItem );
	}

	return GetCSWpnData().GetRecoilSeed();
}




const CCSWeaponInfo &CWeaponCSBase::GetCSWpnData() const
{
	// If we don't have a weapon file the below code will do a bogus cast.
	// There is a ton of code that assumes this function can't fail so if somebody
	// is bored they can do a massive find/replace and allow this function to return null.
	// For now, just crash now instead of creating a time bomb for later.
	if ( GetWeaponFileInfoHandle() == GetInvalidWeaponInfoHandle() )
	{
		CFmtStr outputStr( "Weapon '%s' script file not found, but its data was accessed. This error is fatal.\n", GetName() ? GetName() : "<unknown>" );
		AssertFatalMsg( 0, outputStr.Access() );
	}

	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CCSWeaponInfo *pCSInfo = assert_cast< const CCSWeaponInfo* >( pWeaponInfo );
	return *pCSInfo;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CWeaponCSBase::GetViewModel( int /*viewmodelindex = 0 -- this is ignored in the base class here*/ ) const
{
	CCSPlayer *pOwner = GetPlayerOwner();

	if ( pOwner == NULL )
	{
		 return BaseClass::GetViewModel();
	}
	
	if ( pOwner->HasShield() && CanBeUsedWithShield() )
		 return GetShieldViewModel();
	else
		 return BaseClass::GetViewModel();
}

// bool CWeaponCSBase::AllowTaunts( void )
// {
// 	//return true; // hack: all weapons can taunt for now
// 	return CALL_ATTRIB_HOOK_BOOL( taunt_id );
// }

void CWeaponCSBase::Precache( void )
{
	BaseClass::Precache();

#ifdef CS_SHIELD_ENABLED
	if ( CanBeUsedWithShield() )
	{
		 PrecacheModel( GetShieldViewModel() );
	}
#endif

	if ( GetSilencerModel()[0] != 0 )
	{
		PrecacheModel( GetSilencerModel() );
	}

	PrecacheScriptSound( "Default.ClipEmpty_Pistol" );
	PrecacheScriptSound( "Default.ClipEmpty_Rifle" );
	if ( GetZoomInSound() && GetZoomInSound()[0] )
	{
		PrecacheScriptSound( GetZoomInSound() );
	}
	if ( GetZoomOutSound() && GetZoomOutSound()[0] )
	{
		PrecacheScriptSound( GetZoomOutSound() );
	}

	const char *pMuzzleFlashEffectName_1stPerson = GetCSWpnData().GetMuzzleFlashEffectName_1stPerson( GetEconItemView() );
	if ( pMuzzleFlashEffectName_1stPerson && pMuzzleFlashEffectName_1stPerson[0] )
	{
		PrecacheEffect( pMuzzleFlashEffectName_1stPerson );
	}

	const char *pMuzzleFlashEffectName_1stPersonAlt = GetCSWpnData().GetMuzzleFlashEffectName_1stPersonAlt( GetEconItemView() );
	if ( pMuzzleFlashEffectName_1stPersonAlt && pMuzzleFlashEffectName_1stPersonAlt[0] )
	{
		PrecacheEffect( pMuzzleFlashEffectName_1stPersonAlt );
	}

	const char *pMuzzleFlashEffectName_3rdPerson = GetCSWpnData().GetMuzzleFlashEffectName_3rdPerson( GetEconItemView() );
	if ( pMuzzleFlashEffectName_3rdPerson && pMuzzleFlashEffectName_3rdPerson[0] )
	{
		PrecacheEffect( pMuzzleFlashEffectName_3rdPerson );
	}

	const char *pMuzzleFlashEffectName_3rdPersonAlt = GetCSWpnData().GetMuzzleFlashEffectName_3rdPersonAlt( GetEconItemView() );
	if ( pMuzzleFlashEffectName_3rdPersonAlt && pMuzzleFlashEffectName_3rdPersonAlt[0] )
	{
		PrecacheEffect( pMuzzleFlashEffectName_3rdPersonAlt );
	}

	const char *pEjectBrassEffectName = GetCSWpnData().GetEjectBrassEffectName( GetEconItemView() );
	if ( pEjectBrassEffectName && pEjectBrassEffectName[0] )
	{
		PrecacheEffect( pEjectBrassEffectName );
	}

	const char *pHeatEffectName = GetCSWpnData().GetHeatEffectName( GetEconItemView() );
	if ( pHeatEffectName && pHeatEffectName[0] )
	{
		PrecacheEffect( pHeatEffectName );
	}

	PrecacheEffect( "gunshotsplash" );

}

Activity CWeaponCSBase::GetDeployActivity( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if (pOwner)
	{
		if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 && LookupActivity( "ACT_VM_EMPTY_DRAW" ) > 0 )
		{
			return ACT_VM_EMPTY_DRAW;
		}
	}
	return ACT_VM_DRAW;
}

bool CWeaponCSBase::DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt )
{
	// Msg( "deploy %s at %f\n", GetClassname(), gpGlobals->curtime );
	CCSPlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
	{
		return false;
	}

	pOwner->SetAnimationExtension( szAnimExt );

	SetViewModel();
	SendWeaponAnim( GetDeployActivity() );

	pOwner->SetNextAttack( gpGlobals->curtime + SequenceDuration() );
	m_flNextPrimaryAttack	= gpGlobals->curtime;
	m_flNextSecondaryAttack	= gpGlobals->curtime;

	SetWeaponVisible( true );
	pOwner->SetShieldDrawnState( false );

	if ( pOwner->HasShield() == true )
		 SetWeaponModelIndex( SHIELD_WORLD_MODEL );
	else
		 SetWeaponModelIndex( szWeaponModel );

//#ifndef CLIENT_DLL
//	SetWeaponModules();
//#endif

#ifdef IRONSIGHT
	m_iIronSightMode = IronSight_viewmodel_is_deploying;
#endif //IRONSIGHT

	return true;
}

void CWeaponCSBase::UpdateShieldState( void )
{
	//empty by default.
	CCSPlayer *pOwner = GetPlayerOwner();

	if ( pOwner == NULL )
		 return;

	//ADRIANTODO
	//Make the hitbox set switches here!!!
	if ( pOwner->HasShield() == false )
	{
		
		pOwner->SetShieldDrawnState( false );
		//pOwner->SetHitBoxSet( 0 );
		return;
	}
	else
	{
		//pOwner->SetHitBoxSet( 1 );
	}
}

void CWeaponCSBase::SetWeaponModelIndex( const char *pName )
{
 	 m_iWorldModelIndex = modelinfo->GetModelIndex( pName );
}

bool CWeaponCSBase::CanBeSelected( void )
{
	if ( !VisibleInWeaponSelection() )
		return false;

	return true;
}

bool CWeaponCSBase::CanDeploy( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( pPlayer->HasShield() && CanBeUsedWithShield() == false )
		 return false;

	return BaseClass::CanDeploy();
}

float CWeaponCSBase::CalculateNextAttackTime( float fCycleTime )
{
	float fCurAttack = m_flNextPrimaryAttack;
	float fDeltaAttack = gpGlobals->curtime - fCurAttack;
	if ( fDeltaAttack < 0 || fDeltaAttack > gpGlobals->interval_per_tick )
	{
		fCurAttack = gpGlobals->curtime;
	}
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = fCurAttack + fCycleTime;

	return fCurAttack;
}

bool CWeaponCSBase::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( pPlayer )
		pPlayer->SetFOV( pPlayer, 0, 0.0f ); // reset the default FOV.

	if ( pPlayer )
		pPlayer->SetShieldDrawnState( false );

	ResetGunHeat();

	return BaseClass::Holster( pSwitchingTo );
}

bool CWeaponCSBase::Deploy()
{

#ifdef IRONSIGHT
	if ( GetIronSightController() )
		GetIronSightController()->SetState( IronSight_viewmodel_is_deploying );
#endif //IRONSIGHT

#ifdef CLIENT_DLL
	m_iAlpha =  80;

	if ( cl_crosshairstyle.GetInt() == 4 || cl_crosshairstyle.GetInt() == 5 )
		m_flCrosshairDistance = 1;

	// Stop glowing
	//if ( m_nGlowIndex != -1 )
	//{
	//	g_GlowObjectManager.UnregisterGlowObject( m_nGlowIndex );
	//	m_nGlowIndex = -1;
	//}
#endif

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		pPlayer->m_iShotsFired = 0;
		pPlayer->m_bResumeZoom = false;
		pPlayer->m_bIsScoped = false;
		pPlayer->SetFOV( pPlayer, 0 );
		pPlayer->m_bWaitForNoAttack = true;
	}
	m_fAccuracyPenalty = 0.0f;
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
	m_fAccuracyFishtail = 0.0f;
#endif
//	m_iRecoilIndex = 0;	DEPRECATED
	m_flRecoilIndex = 0.0f;

	ResetGunHeat();

	return BaseClass::Deploy();
}

#ifndef CLIENT_DLL
bool CWeaponCSBase::IsRemoveable()
{
	if ( BaseClass::IsRemoveable() == true )
	{
		if ( m_nextPrevOwnerTouchTime > gpGlobals->curtime || m_nextOwnerTouchTime > gpGlobals->curtime )
		{
			return false;
		}
	}

	// currently held or never had an owner (level designer placed)
	if ( GetOwner() || !m_hPrevOwner.m_Value.IsValid() )
		return false;

	return BaseClass::IsRemoveable();
}

void CWeaponCSBase::RemoveUnownedWeaponThink()
{
	// Keep thinking in case we need to remove ourselves
	SetContextThink( &CWeaponCSBase::RemoveUnownedWeaponThink, gpGlobals->curtime + REMOVEUNOWNEDWEAPON_THINK_INTERVAL, REMOVEUNOWNEDWEAPON_THINK_CONTEXT );

	if ( GetOwner() )	// owned weapons don't get deleted, reset counter
	{
		if ( m_numRemoveUnownedWeaponThink )
		{
			m_numRemoveUnownedWeaponThink = 0;
		}
		return;
	}

	if ( weapon_auto_cleanup_time.GetFloat() > 0  )
	{
		if ( !GetGroundEntity() )
			return; // gun is falling, and might land near a player - don't remove until we're on the ground

		float flSpawnTime = m_nextOwnerTouchTime;
//		if ( m_nextOwnerTouchTime > flSpawnTime )
//			flSpawnTime = m_nextOwnerTouchTime;

		float flTime = weapon_auto_cleanup_time.GetFloat();

		// check if it's out of ammo, if so clean it up super quick
		if ( m_iClip1 == 0  )
			flTime = MIN( 5, flTime ) ;

		if ( flSpawnTime > 0 && (gpGlobals->curtime - flSpawnTime) < flTime )
			return;

		// check to see if any players are close
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer )
				continue;

			if ( (GetAbsOrigin() - pPlayer->GetAbsOrigin()).Length() < 1200 )
				return;
		}
	}
	else if ( m_numRemoveUnownedWeaponThink <= REMOVEUNOWNEDWEAPON_THINK_REMOVE )
	{
		++ m_numRemoveUnownedWeaponThink;
		return; // let the server think a few frames while the owner might acquire this weapon
	}

	// Schedule this weapon to be removed
	UTIL_Remove( this );
}
#endif

ConVar mp_weapon_prev_owner_touch_time( "mp_weapon_prev_owner_touch_time", "1.5", FCVAR_CHEAT | FCVAR_REPLICATED );
#if defined ( CLIENT_DLL )
CEG_NOINLINE void CWeaponCSBase::Drop( const Vector &vecVelocity )
#else
void CWeaponCSBase::Drop( const Vector &vecVelocity )
#endif
{
#ifdef CLIENT_DLL
	CEG_PROTECT_VIRTUAL_FUNCTION( CWeaponCSBase_Drop );

	BaseClass::Drop( vecVelocity );

	CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
	if ( pHudSelection )
	{
		pHudSelection->OnWeaponDrop( this );
	}

	UpdateOutlineGlow();

	return;
#else

	// Once somebody drops a gun, it's fair game for removal when/if
	// a game_weapon_manager does a cleanup on surplus weapons in the
	// world.
	SetRemoveable( true );

	StopAnimation();
	StopFollowingEntity( );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	// clear follow stuff, setup for collision
	SetGravity( 1.0 );
	m_iState = WEAPON_NOT_CARRIED;
	RemoveEffects( EF_NODRAW );
	SetGroundEntity( NULL );

	m_bInReload = false; // stop reloading 
	m_bReloadVisuallyComplete = false;

	SetThink( NULL );
	m_nextPrevOwnerTouchTime = gpGlobals->curtime + mp_weapon_prev_owner_touch_time.GetFloat();
	// [msmith] There is an issue where the model index does not get updated on the client if we let a player
	// pick up a weapon in the same frame that it is thrown down by a different player.
	// The m_nextOwnerTouchTime delay fixes that.
	m_nextOwnerTouchTime = gpGlobals->curtime + 0.1f;
	m_hPrevOwner = GetPlayerOwner();

	SetOwnerEntity( NULL );
	SetOwner( NULL );

	VerifyAndSetContextSensitiveWeaponModel();

	FallInit();

	if ( CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		// Don't allow non-c4 weapon pickups in gun game progressive mode
		if ( !IsA( WEAPON_C4 ) )
		{
			SetTouch( NULL );
		}
		else
		{
			SetTouch( &CWeaponCSBase::DefaultTouch );
		}
	}
	else
	{
		SetTouch( &CWeaponCSBase::DefaultTouch );
	}

	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj != NULL )
	{
		AngularImpulse	angImp( 100, 100, 100 );
		pObj->AddVelocity( &vecVelocity, &angImp );
	}
	else
	{
		SetAbsVelocity( vecVelocity );
	}

	SetNextThink( gpGlobals->curtime );

	// add it to the list of dropped weapons
	if ( CSGameRules() )
		CSGameRules()->AddDroppedWeaponToList( this );

#ifdef IRONSIGHT
	if ( GetIronSightController() )
		GetIronSightController()->SetState( IronSight_weapon_is_dropped );
#endif

#endif
}


// whats going on here is that if the player drops this weapon, they shouldn't take it back themselves
// for a little while.  But if they throw it at someone else, the other player should get it immediately.
void CWeaponCSBase::DefaultTouch( CBaseEntity *pOther )
{
	if ( m_bCanBePickedUp == false )
		return;

	if ( pOther->GetFlags() & FL_CONVEYOR )
	{
		pOther->StartTouch( this );
	}

	// Add a small delay in general so the networking has a chance to update the values on the client side before someone
	// picks up the weapon.
	if ( gpGlobals->curtime < m_nextOwnerTouchTime )
	{
		return;
	}

	if ( ( m_hPrevOwner != NULL ) && ( pOther == m_hPrevOwner ) && ( gpGlobals->curtime < m_nextPrevOwnerTouchTime ) )
	{
		return;
	}

	BaseClass::DefaultTouch( pOther );
}

#if defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// Purpose: Draw the weapon's crosshair
//-----------------------------------------------------------------------------
int CWeaponCSBase::GetReticleWeaponSpread( void )
{
	int result = 0;
	CCSPlayer* pPlayer = ( CCSPlayer* )C_BasePlayer::GetLocalPlayer();
	
	if ( pPlayer )
	{
		if ( GetWeaponType() == WEAPONTYPE_GRENADE ||  GetCSWeaponID() == WEAPON_C4 || GetCSWeaponID() == WEAPON_KNIFE || GetCSWeaponID() == WEAPON_KNIFE_GG )
		{
			return -1; //early out since we don't want a gap and don't want to change the sentinel value with the math later.
		}
		

		float fHalfFov = DEG2RAD( pPlayer->GetFOV() ) * 0.5f;
		float fSpreadDistance = ( GetInaccuracy() + GetSpread() ) * 320.0f / tanf( fHalfFov );
		result = RoundFloatToInt( YRES( fSpreadDistance ));
	}

	return result;

}

int CWeaponCSBase::GetReticleCrosshairGap( void )
{
	int result = 0;

	CCSPlayer* pPlayer = ( CCSPlayer* )C_BasePlayer::GetLocalPlayer();

	if ( pPlayer )
	{
		int crosshairGap;

		if ( !( pPlayer->GetFlags() & FL_ONGROUND ) )
			crosshairGap = 30;
		else if ( pPlayer->GetFlags() & FL_DUCKING )
			crosshairGap = 3;
		else if ( pPlayer->GetAbsVelocity().Length() > 100 )
			crosshairGap = 20;		
		else
			crosshairGap = 10;

		result = ScreenHeight() * crosshairGap / 768;
	}

	return result;
}

bool CWeaponCSBase::WantReticleShown( void )
{
	CCSPlayer* pPlayer = ( CCSPlayer* )C_BasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return false;

	if ( pPlayer->HasShield() && pPlayer->IsShieldDrawn() == true )
		return false;

	bool isAKnife = ( GetWeaponType() == WEAPONTYPE_KNIFE && !IsA( WEAPON_TASER ) );

#ifdef CLIENT_DLL
	bool isUsingMotionController = PlatformInputDevice::IsInputDeviceAPointer( g_pInputSystem->GetCurrentInputDevice() );
#else
	bool isUsingMotionController = PlatformInputDevice::IsInputDeviceAPointer( GetPlayerInputDevice() );
#endif

	// we hide the sniper Reticle UNLESS we're using a motion controller AND we're not zoomed
	bool hideSniperReticle = GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE && 
							 (pPlayer->m_bIsScoped || !isUsingMotionController);
							 

	// no crosshair for sniper rifles
	if ( hideSniperReticle ||
		 ( GetWeaponType() == WEAPONTYPE_C4 ) ||
		 ( isAKnife && !weapon_reticle_knife_show.GetBool() ) )
		return false;

	return true;
}

#ifdef CLIENT_DLL
ConVar cl_cam_driver_compensation_scale( "cl_cam_driver_compensation_scale", "0.75", FCVAR_RELEASE, "" );
#endif

void CWeaponCSBase::DrawCrosshair()
{
	if ( !crosshair.GetInt() )
		return;
	CHudCrosshair *pCrosshair = GET_HUDELEMENT( CHudCrosshair );

	// clear old hud crosshair
	if ( !pCrosshair )
		return;
		pCrosshair->SetCrosshair( 0, Color( 255, 255, 255, 255 ) );

	CCSPlayer* pPlayer = ( CCSPlayer* )C_BasePlayer::GetLocalPlayer();

	if ( !pPlayer || GetPlayerOwner() == NULL )
		return;

	Assert( (pPlayer == GetPlayerOwner()) || ( pPlayer->GetObserverMode()==OBS_MODE_IN_EYE) );
	if ( pPlayer->IsInVGuiInputMode() )
		return;

	if ( pPlayer->GetObserverInterpState() == C_CSPlayer::OBSERVER_INTERP_TRAVELING )
		return;

	// no crosshair for sniper rifles

	// localplayer must be owner if not in Spec mode



	int	r, g, b;
	switch ( cl_crosshaircolor.GetInt() )
	{
	case 0 :	r = 250;	g = 50;		b = 50;		break;
	case 1 :	r = 50;		g = 250;	b = 50;		break;
	case 2 :	r = 250;	g = 250;	b = 50;		break;
	case 3 :	r = 50;		g = 50;		b = 250;	break;
	case 4 :	r = 50;		g = 250;	b = 250;	break;
	case 5 :
		r = cl_crosshaircolor_r.GetInt();
		g = cl_crosshaircolor_g.GetInt();
		b = cl_crosshaircolor_b.GetInt();
		break;
	default :	r = 50;		g = 250;	b = 50;		break;
	}

	// if user is using nightvision, make the crosshair red.
	if ( pPlayer->m_bNightVisionOn )
	{
		r = 250;
		g = 50;
		b = 50;
	}

	int alpha = clamp( cl_crosshairalpha.GetInt(), 0, 255 );

	if ( !m_iCrosshairTextureID )
	{
		CHudTexture *pTexture = HudIcons().GetIcon( "whiteAdditive" );
		if ( pTexture )
		{
			m_iCrosshairTextureID = pTexture->textureId;
		}
	}

	bool bAdditive = !cl_crosshairusealpha.GetBool() && !pPlayer->m_bNightVisionOn;
	if ( bAdditive )
	{
		vgui::surface()->DrawSetTexture( m_iCrosshairTextureID );
		alpha = 200;
	}


#ifdef IRONSIGHT
	if ( GetIronSightController() && !weapon_debug_spread_show.GetBool() && GetIronSightController()->ShouldHideCrossHair() )
	{
		alpha = 0;
	}
#endif


	if ( pPlayer->HasShield() && pPlayer->IsShieldDrawn() == true )
		return;
	if ( GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE && !weapon_debug_spread_show.GetBool() )
		return;
	float fHalfFov = DEG2RAD( pPlayer->GetFOV() ) * 0.5f;
	float flInaccuracy = GetInaccuracy();
	float flSpread = GetSpread();
	
//	float flCrossDistanceScale = cl_crosshair_dynamic_distancescale.GetFloat();
	float fSpreadDistance = ((flInaccuracy + flSpread) * 320.0f / tanf(fHalfFov))/* * flCrossDistanceScale*/;
	float flCappedSpreadDistance = fSpreadDistance;
	float flMaxCrossDistance = cl_crosshair_dynamic_splitdist.GetFloat();
 	if (fSpreadDistance > flMaxCrossDistance)
 	{
// 		bHitMax = true;
// 		//flLineAlpha = alpha * MAX(0, 1 - ((fSpreadDistance - flMaxCrossDistance) / 20));
 		flCappedSpreadDistance = flMaxCrossDistance;
 	}

	int iSpreadDistance = cl_crosshairstyle.GetInt() < 4 ? RoundFloatToInt( YRES( fSpreadDistance )) : 2;
	int iCappedSpreadDistance = cl_crosshairstyle.GetInt() < 4 ? RoundFloatToInt(YRES(flCappedSpreadDistance)) : 2;

	float flAccuracyFishtail = GetAccuracyFishtail();
	
#ifdef CLIENT_DLL
	if (cl_weapon_debug_print_accuracy.GetInt() == 1)
	{
		float flVel = pPlayer->GetLocalVelocity().Length2D();
		if (flVel > 0)
			Msg("Inaccuracy =\t%f\tSpread =\t%f\tSpreadDistance =\t%f\tPlayer Velocity =\t%f\n", flInaccuracy, flSpread, fSpreadDistance, flVel);
	}
		
#endif

	int iDeltaDistance = GetCSWpnData().GetCrosshairDeltaDistance( GetEconItemView() ); // Amount by which the crosshair expands when shooting ( per frame )
	float fCrosshairDistanceGoal = cl_crosshairgap_useweaponvalue.GetBool() ? GetCSWpnData().GetCrosshairMinDistance( GetEconItemView() ) : 4; // The minimum distance the crosshair can achieve...

	//0 = default
	//1 = default static
	//2 = classic standard
	//3 = classic dynamic
	//4 = classic static
	//if ( cl_dynamiccrosshair.GetBool() )
	if ( cl_crosshairstyle.GetInt() != 4 && (cl_crosshairstyle.GetInt() == 2 || cl_crosshairstyle.GetInt() == 3) )
	{
		if ( !( pPlayer->GetFlags() & FL_ONGROUND ) )
			fCrosshairDistanceGoal *= 2.0f;
		else if ( pPlayer->GetFlags() & FL_DUCKING )
			fCrosshairDistanceGoal *= 0.5f;
		else if ( pPlayer->GetAbsVelocity().Length() > 100 )
			fCrosshairDistanceGoal *= 1.5f;
	}

	// [jpaquin] changed to only bump up the crosshair size if the player is still shooting or is spectating someone else
	if ( pPlayer->m_iShotsFired > m_iAmmoLastCheck && ( pPlayer->m_nButtons & ( IN_ATTACK|IN_ATTACK2 )) && m_iClip1 >= 0 )
	{
		if ( cl_crosshairstyle.GetInt() == 5 )
			m_flCrosshairDistance += (GetRecoilMagnitude( m_weaponMode ) / 3.5);
		else if ( cl_crosshairstyle.GetInt() != 4 )
			fCrosshairDistanceGoal += iDeltaDistance;
	}

	m_iAmmoLastCheck = pPlayer->m_iShotsFired;

	if ( m_flCrosshairDistance > fCrosshairDistanceGoal )
	{
		if ( cl_crosshairstyle.GetInt() == 5 )
		{
			//float flPer = ( gpGlobals->frametime / 0.025f );
			//m_flCrosshairDistance -= (0.11 + ( m_flCrosshairDistance * 0.01 )) * gpGlobals->frametime;
			m_flCrosshairDistance -= 42.0 * gpGlobals->frametime;
		}
		else
			m_flCrosshairDistance = Lerp( ( gpGlobals->frametime / 0.025f ), fCrosshairDistanceGoal, m_flCrosshairDistance );
	}

	// clamp max crosshair expansion
	m_flCrosshairDistance = clamp( m_flCrosshairDistance, fCrosshairDistanceGoal, 25.0f );

	int iCrosshairDistance;
	int iCappedCrosshairDistance = 0;
	int iBarSize;
	int iBarThickness;

	iCrosshairDistance = RoundFloatToInt((m_flCrosshairDistance * ScreenHeight() / 1200.0f) + cl_crosshairgap.GetFloat());
	iBarSize = RoundFloatToInt( YRES( cl_crosshairsize.GetFloat() ));
	iBarThickness = MAX( 1, RoundFloatToInt( YRES( cl_crosshairthickness.GetFloat() )) );

	//0 = default
	//1 = default static
	//2 = classic standard
	//3 = classic dynamic
	//4 = classic static
	//if ( weapon_debug_spread_show.GetInt() == 2 )
	if ( weapon_debug_spread_show.GetInt( ) == 2 || (iSpreadDistance > 0 && ( cl_crosshairstyle.GetInt( ) == 2 || cl_crosshairstyle.GetInt( ) == 3 ) ) )
	{
		iCrosshairDistance = iSpreadDistance + cl_crosshairgap.GetFloat();

		if ( cl_crosshairstyle.GetInt( ) == 2 )
			iCappedCrosshairDistance = iCappedSpreadDistance + cl_crosshairgap.GetFloat();
	}
	else if ( cl_crosshairstyle.GetInt( ) == 4 || ( iSpreadDistance == 0 && ( cl_crosshairstyle.GetInt( ) == 2 || cl_crosshairstyle.GetInt( ) == 3 ) ) )
	{
		iCrosshairDistance = fCrosshairDistanceGoal + cl_crosshairgap.GetFloat();
		iCappedCrosshairDistance = 4 + cl_crosshairgap.GetFloat();
	}

#ifdef SIXENSE
	int iCenterX;
	int iCenterY;

	if( g_pSixenseInput->IsEnabled() ) 
	{
		// Never autoaim a predicted weapon (for now)
		Vector	aimVector;
		AngleVectors( CurrentViewAngles() - g_pSixenseInput->GetViewAngleOffset(), &aimVector );

		// calculate where the bullet would go so we can draw the cross appropriately
		Vector vecStart = pPlayer->Weapon_ShootPosition();
		Vector vecEnd = pPlayer->Weapon_ShootPosition() + aimVector * MAX_TRACE_LENGTH;


		trace_t tr;
		UTIL_TraceLine( vecStart, vecEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

		Vector screen;
		screen.Init();
		ScreenTransform(tr.endpos, screen);

		iCenterX = ScreenWidth() / 2;
		iCenterY = ScreenHeight() / 2;

		iCenterX += 0.5 * screen[0] * ScreenWidth() + 0.5;
		iCenterY += 0.5 * screen[1] * ScreenHeight() + 0.5;
		iCenterY = ScreenHeight() - iCenterY;

	} 
	else 
	{
		iCenterX = ScreenWidth() / 2;
		iCenterY = ScreenHeight() / 2;
	}

#else
	int iCenterX = ScreenWidth() / 2;
	int iCenterY = ScreenHeight() / 2;
#endif

	float flAngleToScreenPixel = 0;

#ifdef CLIENT_DLL
	// subtract a ratio of cam driver motion from crosshair according to cl_cam_driver_compensation_scale
	if ( cl_cam_driver_compensation_scale.GetFloat() != 0 )
	{
		CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
		if ( pOwner )
		{
			CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
			if ( vm && vm->m_flCamDriverWeight > 0 )
			{
				QAngle angCamDriver = vm->m_flCamDriverWeight * vm->m_angCamDriverLastAng * clamp( cl_cam_driver_compensation_scale.GetFloat(), -10.0f, 10.0f );
				if ( angCamDriver.x != 0 || angCamDriver.y != 0  )
				{
					flAngleToScreenPixel = VIEWPUNCH_COMPENSATE_MAGIC_SCALAR * 2 * ( ScreenHeight() / ( 2.0f * tanf(DEG2RAD( pPlayer->GetFOV() ) / 2.0f) ) );
					iCenterY -= ( flAngleToScreenPixel * sinf( DEG2RAD( angCamDriver.x ) ) ) ;
					iCenterX += ( flAngleToScreenPixel * sinf( DEG2RAD( angCamDriver.y ) ) ) ;
				}
			}
		}
	}
#endif

/*
	// Optionally subtract out viewangle since it doesn't affect shooting.
	if ( cl_flinch_compensate_crosshair.GetBool() )
	{
		QAngle viewPunch = pPlayer->GetViewPunchAngle();

		if ( viewPunch.x != 0 || viewPunch.y != 0 )
		{
			if ( flAngleToScreenPixel == 0 )
				flAngleToScreenPixel = VIEWPUNCH_COMPENSATE_MAGIC_SCALAR * 2 * ( ScreenHeight() / ( 2.0f * tanf(DEG2RAD( pPlayer->GetFOV() ) / 2.0f) ) );

			iCenterY -= flAngleToScreenPixel * sinf( DEG2RAD( viewPunch.x ) );
			iCenterX += flAngleToScreenPixel * sinf( DEG2RAD( viewPunch.y ) );
		}
	}
*/

	int nFishTaleShift = ( flAccuracyFishtail * ( ScreenHeight() / 500.0f ) );

	//0 = default
	//1 = default static
	//2 = classic standard
	//3 = classic dynamic
	//4 = classic static
	if ( cl_crosshairstyle.GetInt() == 4 )
	{
		nFishTaleShift = 0;
	}

	float flAlphaSplitInner = cl_crosshair_dynamic_splitalpha_innermod.GetFloat();
	float flAlphaSplitOuter = cl_crosshair_dynamic_splitalpha_outermod.GetFloat();
	float flSplitRatio = cl_crosshair_dynamic_maxdist_splitratio.GetFloat();
	int iInnerCrossDist = iCrosshairDistance;
	float flLineAlphaInner = alpha;
	float flLineAlphaOuter = alpha;
	int iBarSizeInner = iBarSize;
	int iBarSizeOuter = iBarSize;

	// draw the crosshair that splits off from the main xhair
	if (cl_crosshairstyle.GetInt() == 2 && fSpreadDistance > flMaxCrossDistance)
	{
		iInnerCrossDist = iCappedCrosshairDistance;
		flLineAlphaInner = alpha * flAlphaSplitInner;
		flLineAlphaOuter = alpha * flAlphaSplitOuter;
		iBarSizeInner = ceil((float)iBarSize * (1.0f - flSplitRatio));
		iBarSizeOuter = floor((float)iBarSize * flSplitRatio);

		// draw horizontal crosshair lines
		int iInnerLeft = (iCenterX - iCrosshairDistance - iBarThickness / 2 + nFishTaleShift) - iBarSizeInner;
		int iInnerRight = iInnerLeft + 2 * (iCrosshairDistance + iBarSizeInner) + iBarThickness + nFishTaleShift;
		int iOuterLeft = iInnerLeft - iBarSizeOuter;
		int iOuterRight = iInnerRight + iBarSizeOuter;
		int y0 = iCenterY - iBarThickness / 2;
		int y1 = y0 + iBarThickness;
		DrawCrosshairRect(r, g, b, flLineAlphaOuter, iOuterLeft, y0, iInnerLeft, y1, bAdditive);
		DrawCrosshairRect(r, g, b, flLineAlphaOuter, iInnerRight, y0, iOuterRight, y1, bAdditive);

		// draw vertical crosshair lines
		int iInnerTop = (iCenterY - iCrosshairDistance - iBarThickness / 2) - iBarSizeInner;
		int iInnerBottom = iInnerTop + 2 * (iCrosshairDistance + iBarSizeInner) + iBarThickness;
		int iOuterTop = iInnerTop - iBarSizeOuter;
		int iOuterBottom = iInnerBottom + iBarSizeOuter;
		int x0 = iCenterX - iBarThickness / 2;
		int x1 = x0 + iBarThickness;
		DrawCrosshairRect(r, g, b, flLineAlphaOuter, x0, iOuterTop, x1, iInnerTop, bAdditive);
		DrawCrosshairRect(r, g, b, flLineAlphaOuter, x0, iInnerBottom, x1, iOuterBottom, bAdditive);
	}

	// draw horizontal crosshair lines
	int iInnerLeft = iCenterX - iInnerCrossDist - iBarThickness / 2 + nFishTaleShift;
	int iInnerRight = iInnerLeft + 2 * iInnerCrossDist + iBarThickness + nFishTaleShift;
	int iOuterLeft = iInnerLeft - iBarSizeInner;
	int iOuterRight = iInnerRight + iBarSizeInner;
	int y0 = iCenterY - iBarThickness / 2;
	int y1 = y0 + iBarThickness;
	DrawCrosshairRect(r, g, b, flLineAlphaInner, iOuterLeft, y0, iInnerLeft, y1, bAdditive);
	DrawCrosshairRect(r, g, b, flLineAlphaInner, iInnerRight, y0, iOuterRight, y1, bAdditive);

	// draw vertical crosshair lines
	int iInnerTop = iCenterY - iInnerCrossDist - iBarThickness / 2;
	int iInnerBottom = iInnerTop + 2 * iInnerCrossDist + iBarThickness;
	int iOuterTop = iInnerTop - iBarSizeInner;
	int iOuterBottom = iInnerBottom + iBarSizeInner;
	int x0 = iCenterX - iBarThickness / 2;
	int x1 = x0 + iBarThickness;
	DrawCrosshairRect(r, g, b, flLineAlphaInner, x0, iOuterTop, x1, iInnerTop, bAdditive);
	DrawCrosshairRect(r, g, b, flLineAlphaInner, x0, iInnerBottom, x1, iOuterBottom, bAdditive);

	// draw dot
	if ( cl_crosshairdot.GetBool() )
	{
		int x0 = iCenterX - iBarThickness / 2;
		int x1 = x0 + iBarThickness;
		int y0 = iCenterY - iBarThickness / 2;
		int y1 = y0 + iBarThickness;
		DrawCrosshairRect( r, g, b, alpha, x0, y0, x1, y1, bAdditive );
	}

	if ( weapon_debug_spread_show.GetInt() == 1 )
	{
		// colors
		r = 250;
		g = 250;
		b = 50;
		//vgui::surface()->DrawSetColor( r, g, b, alpha );

		int iBarThickness = MAX( 1, RoundFloatToInt( YRES( cl_crosshairthickness.GetFloat() )) );

		// draw vertical spread lines
		int iInnerLeft	= iCenterX - iSpreadDistance;
		int iInnerRight	= iCenterX + iSpreadDistance;
		int iOuterLeft	= iInnerLeft - iBarThickness;
		int iOuterRight	= iInnerRight + iBarThickness;
		int iInnerTop		= iCenterY - iSpreadDistance;
		int iInnerBottom	= iCenterY + iSpreadDistance;
		int iOuterTop		= iInnerTop - iBarThickness;
		int iOuterBottom	= iInnerBottom + iBarThickness;

		int iGap = RoundFloatToInt( weapon_debug_spread_gap.GetFloat() * iSpreadDistance );

		// draw horizontal lines
		DrawCrosshairRect( r, g, b, alpha, iOuterLeft, iOuterTop, iCenterX - iGap, iInnerTop, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iCenterX + iGap, iOuterTop, iOuterRight, iInnerTop, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iOuterLeft, iInnerBottom, iCenterX - iGap, iOuterBottom, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iCenterX + iGap, iInnerBottom, iOuterRight, iOuterBottom, bAdditive );

		// draw vertical lines
		DrawCrosshairRect( r, g, b, alpha, iOuterLeft, iOuterTop, iInnerLeft, iCenterY - iGap, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iOuterLeft, iCenterY + iGap, iInnerLeft, iOuterBottom, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iInnerRight, iOuterTop, iOuterRight, iCenterY - iGap, bAdditive );
		DrawCrosshairRect( r, g, b, alpha, iInnerRight, iCenterY + iGap, iOuterRight, iOuterBottom, bAdditive );
	}

}



void CWeaponCSBase::OnDataChanged( DataUpdateType_t type )
{
	if ( type == DATA_UPDATE_CREATED )
	{
		m_nWeaponID = WeaponIdFromString( GetClassname() );
	}

	bool bChangedCarryState = false;
	if ( ( m_iState != WEAPON_NOT_CARRIED && m_iOldState == WEAPON_NOT_CARRIED ) ||
	     ( m_iState == WEAPON_NOT_CARRIED && m_iOldState != WEAPON_NOT_CARRIED ) )
	{
		bChangedCarryState = true;
	}

	C_BaseCombatCharacter *pOwner = GetPreviousOwner();

	if ( GetWeaponType() == WEAPONTYPE_GRENADE )
	{
		pOwner = ( (CBaseGrenade *) this )->GetThrower();
	}

	if ( pOwner )
	{
		C_BasePlayer *pPlayer = ToBasePlayer( pOwner );
		C_CSPlayer *pObservedPlayer = GetHudPlayer();

		// check if weapon was dropped by local player or the player we are observing
		if ( pObservedPlayer == pPlayer )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( C_BasePlayer::GetSplitScreenSlotForPlayer( pPlayer ) );

			if ( m_iState == WEAPON_NOT_CARRIED && m_iOldState != WEAPON_NOT_CARRIED )
			{
				CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
				if ( pHudSelection )
				{
					pHudSelection->OnWeaponDrop( this );
				}
			}
		}
	}

	BaseClass::OnDataChanged( type );

#ifdef CLIENT_DLL
	bool bFirstPersonSpectatedState = IsFirstPersonSpectated();
	if ( ( bFirstPersonSpectatedState && !m_bOldFirstPersonSpectatedState ) ||
		 ( !bFirstPersonSpectatedState && m_bOldFirstPersonSpectatedState ) )
	{
		bChangedCarryState = true;
	}

	if ( type == DATA_UPDATE_CREATED )
	{
		// this will trigger the custom material to start making itself (if needed) the weapon will render with 
		// the original material for a few frames, then switch to the custom material when it's ready
		UpdateCustomMaterial();
		UpdateOutlineGlow();
	}
	else if ( bChangedCarryState )
	{
		CheckCustomMaterial();
		UpdateOutlineGlow();
	}
#endif

#ifdef IRONSIGHT
	UpdateIronSightController();
#endif //IRONSIGHT

	if ( GetPredictable() && !ShouldPredict() )
		ShutdownPredictable();
}


bool CWeaponCSBase::ShouldPredict()
{
	if ( C_BasePlayer::IsLocalPlayer( GetOwner() ) )
		return true;

	return BaseClass::ShouldPredict();
}

void CWeaponCSBase::ProcessMuzzleFlashEvent()
{
	// This is handled from the player's animstate, so it can match up to the beginning of the fire animation
}

int CWeaponCSBase::GetViewModelSplitScreenSlot( C_BaseViewModel *pViewModel )
{
	uint32 visBits = pViewModel->m_VisibilityBits.Get(0, 0xff);
	int nSlot = -1;

	if ( visBits < 3 )
	{
		// If visBits is 3 then it is both slots 1 and 2 meaning all users can see this viewmodel (as indicated via -1).
		// If visBits is 1 or 2 then the view model is only seen in that specific slot either slot (but zero based means slot 0 or 1).
		// If visBits is 0, then split screen is not active so we should not be drawing this view model at all.  Currently not handled here but should be handled elsewhere.
		nSlot = visBits-1;
	}
	return nSlot;
}

bool CWeaponCSBase::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
{
	int nSlot = GetViewModelSplitScreenSlot( pViewModel );

	if( event == 5001 )
	{
		C_CSPlayer *pPlayer = ToCSPlayer( GetOwner() );
		
		if ( !pPlayer )
			return true;

		if( pPlayer && pPlayer->GetFOV() != pPlayer->GetDefaultFOV() && pPlayer->m_bIsScoped && DoesHideViewModelWhenZoomed() )
			return true;

		// hide particle effects when we're interpolating between observer targets
		C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer->GetObserverTarget() == pPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pLocalPlayer->GetObserverInterpState() == C_BasePlayer::OBSERVER_INTERP_TRAVELING )
			return true;

		bool bLocalThirdPerson = ( (pPlayer == pLocalPlayer) && pPlayer->ShouldDraw() );

		Vector origin;
		int iAttachmentIndex = GetMuzzleAttachmentIndex_1stPerson( pViewModel );
		const char *pszEffect = GetMuzzleFlashEffectName_1stPerson();

		if ( pszEffect && Q_strlen( pszEffect ) > 0 && iAttachmentIndex >= 0 )
		{
			// The view model fixes up the split screen visibility of any effects spawned off of it.
			if ( !bLocalThirdPerson )
				DispatchParticleEffect( pszEffect, PATTACH_POINT_FOLLOW, pViewModel, iAttachmentIndex, false, nSlot );
			
			// we can't trust this position
			//pViewModel->GetAttachment( iAttachmentIndex, origin );
			
			//silencers produce no light at all - even smaller lights would illuminate smoke or cause unwanted visual effects
			if ( !(HasSilencer() && IsSilenced()) )
			{
				CPVSFilter filter( origin );
				origin = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset();
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( pPlayer );
				QAngle	vangles;
				Vector	vforward, vright, vup;
				engine->GetViewAngles( vangles );
				AngleVectors( vangles, &vforward, &vright, &vup );
				VectorMA( origin, cl_righthand.GetBool() ? 4 : -4, vright, origin );
				VectorMA( origin, 31, vforward, origin );
				origin[2] += 3.0f;

				TE_DynamicLight( filter, 0.0, &origin, 255, 186, 64, 5, 70, 0.05, 768 );
			}

			UpdateGunHeat( GetCSWpnData().GetHeatPerShot(), iAttachmentIndex  );
		}

		return true;
	}
	else if ( event == AE_CLIENT_EJECT_BRASS )
	{
		C_CSPlayer *pPlayer = ToCSPlayer( GetOwner() );
		if( pPlayer && pPlayer->GetFOV() != pPlayer->GetDefaultFOV() && pPlayer->m_bIsScoped && DoesHideViewModelWhenZoomed() )
			return true;

		Vector origin;
		const char *pszEffect = GetEjectBrassEffectName();
		int iAttachmentIndex = -1;

		// If options is non-zero in length, treat as an attachment name to use for this particle effect.
		if ( options && Q_strlen( options ) > 0 )
		{
			iAttachmentIndex = pViewModel->LookupAttachment( options );
		}
		else
		{
			iAttachmentIndex = GetEjectBrassAttachmentIndex_1stPerson( pViewModel );
		}

		if ( pszEffect && Q_strlen( pszEffect ) > 0 && iAttachmentIndex >= 0 )
		{

			C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer();
			bool bLocalThirdPerson = ( (pPlayer == pLocalPlayer) && pPlayer->ShouldDraw() );

			// The view model fixes up the split screen visibility of any effects spawned off of it.
			if ( !bLocalThirdPerson )
				DispatchParticleEffect( pszEffect, PATTACH_POINT_FOLLOW, pViewModel, iAttachmentIndex, false, nSlot );
		}

		return true;
	}
	else if ( event == AE_CL_SET_STATTRAK_GLOW )
	{
		pViewModel->SetStatTrakGlowMultiplier( atof( options ) );
		return true;
	}
	else if ( event == AE_WPN_NEXTCLIP_TO_POSEPARAM )
	{
		// sets the given pose param to a 0..1 value representing the clip amount after an impending reload
		CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
		if ( pOwner )
		{
			CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
			if ( vm )
			{
				int iNextClip = MIN( GetMaxClip1(), m_iClip1 + GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );
				vm->SetPoseParameter( options, 1.0f - (((float)iNextClip) / ((float)GetMaxClip1())) );
			}
		}
		return true;
	}
	else if ( event == AE_WPN_CLIP_TO_POSEPARAM )
	{
		// sets the given pose param to a 0..1 value representing the clip amount
		CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
		if ( pOwner )
		{
			CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
			if ( vm )
			{
				vm->SetPoseParameter( options, 1.0f - (((float)m_iClip1) / ((float)GetMaxClip1())) );
			}
		}
		return true;
	}
	else if ( event == AE_WPN_EMPTYSHOTS_TO_POSEPARAM )
	{
		// sets the given pose param to a 0..1 value representing the number of empty shots
		CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
		if ( pOwner )
		{
			CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
			if ( vm )
			{
				vm->SetPoseParameter( options, fmod( m_iNumEmptyAttacks, (float)GetMaxClip1() ) / (float)GetMaxClip1() );
			}
		}
		return true;
	}

	return BaseClass::OnFireEvent( pViewModel, origin, angles, event, options );
}

const char* CWeaponCSBase::GetMuzzleFlashEffectName_1stPerson( void )
{
	if ( HasSilencer() && IsSilenced() )
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_1stPersonAlt( GetEconItemView() );
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_1stPerson( GetEconItemView() );
	}
}

const char* CWeaponCSBase::GetHeatEffectName( void )
{
	return GetCSWpnData().GetHeatEffectName( GetEconItemView() );
}

const char* CWeaponCSBase::GetMuzzleFlashEffectName_3rdPerson( void )
{
	if ( HasSilencer() && IsSilenced() )
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_3rdPersonAlt( GetEconItemView() );
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_3rdPerson( GetEconItemView() );
	}
}

const char* CWeaponCSBase::GetEjectBrassEffectName( void )
{
	return GetCSWpnData().GetEjectBrassEffectName( GetEconItemView() );
}

int CWeaponCSBase::GetMuzzleAttachmentIndex_1stPerson( C_BaseViewModel *pViewModel )
{
	if ( pViewModel )
	{
		if ( HasSilencer() && IsSilenced() )
		{
			return pViewModel->LookupAttachment( "muzzle_flash2" );
		}
		return pViewModel->LookupAttachment( "1" );
	}
	return -1;
}

int CWeaponCSBase::GetMuzzleAttachmentIndex_3rdPerson( void )
{
	CBaseWeaponWorldModel *pWeaponWorldModel = GetWeaponWorldModel();
	if ( pWeaponWorldModel )
	{
		if ( HasSilencer() && IsSilenced() )
		{
			return pWeaponWorldModel->LookupAttachment( "muzzle_flash2" );
		}
		return pWeaponWorldModel->LookupAttachment( "muzzle_flash" );
	}
	return -1;
}

int CWeaponCSBase::GetEjectBrassAttachmentIndex_1stPerson( C_BaseViewModel *pViewModel )
{
	if ( pViewModel )
		return pViewModel->LookupAttachment( "2" );

	return -1;
}

int CWeaponCSBase::GetEjectBrassAttachmentIndex_3rdPerson( void )
{
	CBaseWeaponWorldModel *pWeaponWorldModel = GetWeaponWorldModel();
	
	if ( pWeaponWorldModel )
		return pWeaponWorldModel->LookupAttachment( "shell_eject" );

	return -1;
}

#else		

//-----------------------------------------------------------------------------
// Purpose: Get the accuracy derived from weapon and player, and return it
//-----------------------------------------------------------------------------
const Vector& CWeaponCSBase::GetBulletSpread()
{
	static Vector cone = VECTOR_CONE_8DEGREES;
	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: Match the anim speed to the weapon speed while crouching
//-----------------------------------------------------------------------------
float CWeaponCSBase::GetDefaultAnimSpeed()
{
	return 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the laser rifle effect
//-----------------------------------------------------------------------------
void CWeaponCSBase::BulletWasFired( const Vector &vecStart, const Vector &vecEnd )
{
}


bool CWeaponCSBase::ShouldRemoveOnRoundRestart()
{
	if ( GetPlayerOwner() )
		return false;
	else
		return true;
}

void CWeaponCSBase::OnRoundRestart( void )
{
	// Clear out the list of prior owners
//	m_PriorOwners.RemoveAll();
}

//=========================================================
// Materialize - make a CWeaponCSBase visible and tangible
//=========================================================
void CWeaponCSBase::Materialize()
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		// changing from invisible state to visible.
		RemoveEffects( EF_NODRAW );
		DoMuzzleFlash();
	}

	AddSolidFlags( FSOLID_TRIGGER );

	//SetTouch( &CWeaponCSBase::DefaultTouch );

	SetThink( NULL );

}

//=========================================================
// AttemptToMaterialize - the item is trying to rematerialize,
// should it do so now or wait longer?
//=========================================================
void CWeaponCSBase::AttemptToMaterialize()
{
	float time = g_pGameRules->FlWeaponTryRespawn( this );

	if ( time == 0 )
	{
		Materialize();
		return;
	}

	SetNextThink( gpGlobals->curtime + time );
}

//=========================================================
// CheckRespawn - a player is taking this weapon, should 
// it respawn?
//=========================================================
void CWeaponCSBase::CheckRespawn()
{
	//GOOSEMAN : Do not respawn weapons!
	return;
}


//=========================================================
// Respawn- this item is already in the world, but it is
// invisible and intangible. Make it visible and tangible.
//=========================================================
CBaseEntity* CWeaponCSBase::Respawn()
{
	// make a copy of this weapon that is invisible and inaccessible to players ( no touch function ). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	CBaseEntity *pNewWeapon = CBaseEntity::Create( GetClassname(), g_pGameRules->VecWeaponRespawnSpot( this ), GetAbsAngles(), GetOwner() );

	if ( pNewWeapon )
	{
		pNewWeapon->AddEffects( EF_NODRAW );// invisible for now
		pNewWeapon->SetTouch( NULL );// no touch
		pNewWeapon->SetThink( &CWeaponCSBase::AttemptToMaterialize );

		UTIL_DropToFloor( this, MASK_SOLID );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->SetNextThink( gpGlobals->curtime + g_pGameRules->FlWeaponRespawnTime( this ) );
	}
	else
	{
		Msg( "Respawn failed to create %s!\n", GetClassname() );
	}

	return pNewWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCSBase::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );

	if ( pPlayer )
	{
		m_OnPlayerUse.FireOutput( pActivator, pCaller );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponCSBase::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	int nEvent = pEvent->Event();

	if ( (pEvent->type & AE_TYPE_NEWEVENTSYSTEM) && (pEvent->type & AE_TYPE_SERVER) )
	{
		if ( nEvent == AE_WPN_COMPLETE_RELOAD )
		{
			m_bReloadVisuallyComplete = true;
			CCSPlayer *pPlayer = GetPlayerOwner();

			if ( pPlayer )
			{
				int j = MIN( GetMaxClip1() - m_iClip1, GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );	

				// Add them to the clip
				m_iClip1 += j;
				GiveReserveAmmo( AMMO_POSITION_PRIMARY, -j, true );

				m_flRecoilIndex = 0;
			}
		}
		else if ( nEvent == AE_BEGIN_TAUNT_LOOP )
		{
			CCSPlayer *pPlayer = GetPlayerOwner();

			if ( pPlayer && pPlayer->IsLookingAtWeapon() && pPlayer->IsHoldingLookAtWeapon() )
			{
				CBaseViewModel *pViewModel = pPlayer->GetViewModel();

				float flPrevCycle = pViewModel->GetCycle();
				float flNewCycle = V_atof( pEvent->options );
				pViewModel->SetCycle( flNewCycle );

				float flSequenceDuration = pViewModel->SequenceDuration( pViewModel->GetSequence() );
				pPlayer->ModifyTauntDuration( ( flNewCycle - flPrevCycle ) * flSequenceDuration );
			}

 			//if ( pPlayer && pPlayer->IsTaunting() && pPlayer->IsHoldingTaunt() )
 			//{
 			//	CBaseViewModel *pViewModel = pPlayer->GetViewModel();
 			//
 			//	float flPrevCycle = pViewModel->GetCycle();
 			//	float flNewCycle = V_atof( pEvent->options );
 			//	pViewModel->SetCycle( flNewCycle );
 			//
 			//	float flSequenceDuration = pViewModel->SequenceDuration( pViewModel->GetSequence() );
 			//	pPlayer->ModifyTauntDuration( ( flNewCycle - flPrevCycle ) * flSequenceDuration );
 			//}

			return;
		}
#ifndef CLIENT_DLL
		//update the bullet bodygroup on the client
		else if ( nEvent == AE_CL_BODYGROUP_SET_TO_CLIP )
		{
			CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
			if ( pOwner )
			{
				CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
				if ( vm )
				{
					int iNumBodygroupIndices = vm->GetNumBodyGroups();

					for ( int iGroup=1; iGroup<iNumBodygroupIndices; iGroup++ )
					{
						vm->SetBodygroup( iGroup, (m_iClip1 >= iGroup) ? 0 : 1 );
					}
				}
			}
		}
		else if ( nEvent == AE_CL_BODYGROUP_SET_TO_NEXTCLIP )
		{
			CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
			if ( pOwner )
			{
				CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
				if ( vm )
				{
					int iNextClip = MIN( GetMaxClip1(), m_iClip1 + GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );
					int iNumBodygroupIndices = vm->GetNumBodyGroups();
					for ( int iGroup=1; iGroup<iNumBodygroupIndices; iGroup++ )
					{
						vm->SetBodygroup( iGroup, (iNextClip >= iGroup) ? 0 : 1 );
					}
				}
			}
		}
		else if ( nEvent == AE_WPN_NEXTCLIP_TO_POSEPARAM )
		{
			// sets the given pose param to a 0..1 value representing the clip amount after an impending reload
			CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
			if ( pOwner )
			{
				CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
				if ( vm )
				{
					int iNextClip = MIN( GetMaxClip1(), m_iClip1 + GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );
					vm->SetPoseParameter( pEvent->options, 1.0f - (((float)iNextClip) / ((float)GetMaxClip1())) );
				}
			}
		}
		else if ( nEvent == AE_WPN_CLIP_TO_POSEPARAM )
		{
			// sets the given pose param to a 0..1 value representing the clip amount
			CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
			if ( pOwner )
			{
				CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
				if ( vm )
				{
					vm->SetPoseParameter( pEvent->options, 1.0f - (((float)m_iClip1) / ((float)GetMaxClip1())) );
				}
			}
		}
		else if ( nEvent == AE_WPN_EMPTYSHOTS_TO_POSEPARAM )
		{
			// sets the given pose param to a 0..1 value representing the number of empty shots
			CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() );
			if ( pOwner )
			{
				CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
				if ( vm )
				{
					vm->SetPoseParameter( pEvent->options, fmod( m_iNumEmptyAttacks, (float)GetMaxClip1() ) / (float)GetMaxClip1() );
				}
			}
		}
		else if ( nEvent == AE_CL_SHOW_SILENCER )
		{
			if ( CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() ) )
			{
				if ( CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex ) )
				{
					vm->SetBodygroup( vm->FindBodygroupByName( "silencer" ), SILENCER_VISIBLE );
				}
			}

			//world model
			CBaseWeaponWorldModel *pWorldModel = GetWeaponWorldModel();
			if ( pWorldModel )
			{
				pWorldModel->SetBodygroupPreset( "show_silencer" );
			}
			else
			{
				SetBodygroup( FindBodygroupByName( "silencer" ), SILENCER_VISIBLE );
			}
		}
		else if ( nEvent == AE_CL_HIDE_SILENCER )
		{
			if ( CBasePlayer *pOwner = ToBasePlayer( GetPlayerOwner() ) )
			{
				if ( CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex ) )
				{
					vm->SetBodygroup( vm->FindBodygroupByName( "silencer" ), SILENCER_HIDDEN );
				}
			}

			//world model
			CBaseWeaponWorldModel *pWorldModel = GetWeaponWorldModel();
			if ( pWorldModel )
			{
				pWorldModel->SetBodygroupPreset( "hide_silencer" );
			}
			else
			{
				SetBodygroup( FindBodygroupByName( "silencer" ), SILENCER_HIDDEN );
			}
		}
		else if ( nEvent == AE_CL_ATTACH_SILENCER_COMPLETE )
		{
			CBasePlayer *pPlayer = ToBasePlayer( GetPlayerOwner() );
			m_bSilencerOn = true;
			m_weaponMode = Secondary_Mode;
			IGameEvent * event = gameeventmanager->CreateEvent( "silencer_on" );
			if ( event && pPlayer )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}
		}
		else if ( nEvent == AE_CL_DETACH_SILENCER_COMPLETE )
		{
			CBasePlayer *pPlayer = ToBasePlayer( GetPlayerOwner() );
			m_bSilencerOn = false;
			m_weaponMode = Primary_Mode;
			IGameEvent * event = gameeventmanager->CreateEvent( "silencer_off" );
			if ( event && pPlayer )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}
		}
		else if ( nEvent == AE_WPN_CZ_DUMP_CURRENT_MAG )
		{
			CCSPlayer *pCSPlayer = GetPlayerOwner();
			if ( pCSPlayer && pCSPlayer->GetActiveCSWeapon() )
			{
				//m_iClip1 = 0;
				if ( CBaseViewModel *vm = pCSPlayer->GetViewModel( m_nViewModelIndex ) )
				{
					vm->SetBodygroup( vm->FindBodygroupByName( "front_mag" ), 1 );
					//world model
					CBaseWeaponWorldModel *pWorldModel = GetWeaponWorldModel();
					if ( pWorldModel )
					{
						pWorldModel->SetBodygroup( pWorldModel->FindBodygroupByName( "front_mag" ), 1 );
					}
					else
					{
						SetBodygroup( FindBodygroupByName( "front_mag" ), 1 );
					}
					
					//if the front mag is removed, all subsequent anims use the non-front mag reload
					m_iReloadActivityIndex = ACT_SECONDARY_VM_RELOAD;
				}
			}
		}
		else if ( nEvent == AE_WPN_CZ_UPDATE_BODYGROUP )
		{
			CCSPlayer *pCSPlayer = GetPlayerOwner();
			if ( pCSPlayer && pCSPlayer->GetActiveCSWeapon() )
			{
				int iGroupNum = ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0) ? 1 : 0;

				if ( CBaseViewModel *vm = pCSPlayer->GetViewModel( m_nViewModelIndex ) )
				{
					vm->SetBodygroup( vm->FindBodygroupByName( "front_mag" ), iGroupNum );
					//world model
					CBaseWeaponWorldModel *pWorldModel = GetWeaponWorldModel();
					if ( pWorldModel )
					{
						pWorldModel->SetBodygroup( pWorldModel->FindBodygroupByName( "front_mag" ), iGroupNum );
					}
					else
					{
						SetBodygroup( FindBodygroupByName( "front_mag" ), iGroupNum );
					}

					//if the front mag is removed, all subsequent anims use the non-front mag reload
					m_iReloadActivityIndex = ( iGroupNum == 0 ) ? ACT_VM_RELOAD : ACT_SECONDARY_VM_RELOAD;
						
				}
			}
		}
		else if ( nEvent == AE_WPN_PRIMARYATTACK )
		{

			// delay time
			const char *p = pEvent->options;
			char token[256];
			p = nexttoken(token, p, ' ');
			if ( token ) 
			{
				SetPostponeFireReadyTime( gpGlobals->curtime + atof( token ) );

				// play the prepare warning sound to other players
				CCSPlayer *pCSPlayer = GetPlayerOwner();
				if ( pCSPlayer )
				{
					Vector soundPosition = pCSPlayer->GetAbsOrigin() + Vector( 0, 0, 50 );
					CPASAttenuationFilter filter( soundPosition );
					filter.RemoveRecipient( pCSPlayer );

					// remove anyone that is first person spectating this player, since the viewmodel should play this sound for them on their client.
					int i;
					CBasePlayer *pPlayer;
					for( i=1;i<=gpGlobals->maxClients;i++ )
					{
						pPlayer = UTIL_PlayerByIndex( i );

						if ( !pPlayer )
							continue;

						if( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pPlayer->GetObserverTarget() == pCSPlayer )
						{
							filter.RemoveRecipient( pPlayer );
						}
					}

					EmitSound( filter, entindex(), "Weapon_Revolver.Prepare" );
				}

			}
			
			return;
		}
		else if ( nEvent == AE_CL_BODYGROUP_SET_VALUE )
		{
			CCSPlayer *pCSPlayer = GetPlayerOwner();
			if ( pCSPlayer && pCSPlayer->GetActiveCSWeapon() )
			{
				if ( CBaseViewModel *vm = pCSPlayer->GetViewModel( m_nViewModelIndex ) )
				{
					char szBodygroupName[256];
					int value = 0;

					char token[256];

					const char *p = pEvent->options;

					// Bodygroup Name
					p = nexttoken(token, p, ' ');
					if ( token ) 
					{
						Q_strncpy( szBodygroupName, token, sizeof(szBodygroupName) );
					}

					// Get the desired value
					p = nexttoken(token, p, ' ');
					if ( token ) 
					{
						value = atoi( token );
					}

					int index = vm->FindBodygroupByName( szBodygroupName );
					if ( index >= 0 )
					{
						vm->SetBodygroup( index, value );
					}
				}
			}
			return;
		}

#endif

	}

	BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
}

bool CWeaponCSBase::Reload()
{
	m_bFiredOutOfAmmoEvent = false;
	m_bReloadVisuallyComplete = false;

#ifdef IRONSIGHT
	m_iIronSightMode = IronSight_should_approach_unsighted;
#endif //IRONSIGHT

	return true;//BaseClass::Reload();
}

void CWeaponCSBase::Spawn()
{
	m_nWeaponID = WeaponIdFromString( GetClassname() );

	BaseClass::Spawn();

	// Override the bloat that our base class sets as it's a little bit bigger than we want.
	// If it's too big, you drop a weapon and its box is so big that you're still touching it
	// when it falls and you pick it up again right away.
	CollisionProp()->UseTriggerBounds( true, 30 );

	// Set this here to allow players to shoot dropped weapons
	SetCollisionGroup( COLLISION_GROUP_WEAPON );

	SetExtraAmmoCount( m_iDefaultExtraAmmo );	//Start with no additional ammo

	ResetGunHeat();

	m_nextOwnerTouchTime = 0.0f;
	m_nextPrevOwnerTouchTime = 0.0f;

	m_hPrevOwner = NULL;

	// [tj] initialize donor of this weapon
	m_donor = NULL;
	m_donated = false;

	m_bSilencerOn = HasSilencer();
	m_weaponMode = HasSilencer() ? Secondary_Mode : Primary_Mode;

	VisibilityMonitor_AddEntity( this, 400.0f, NULL, NULL );

#ifdef IRONSIGHT
	UpdateIronSightController();
#endif //IRONSIGHT

#ifndef CLIENT_DLL
	if (  mp_death_drop_gun.GetInt() == 0 && !IsA( WEAPON_C4 ) )
	{
		SetContextThink( &CWeaponCSBase::RemoveUnownedWeaponThink, gpGlobals->curtime + REMOVEUNOWNEDWEAPON_THINK_INTERVAL, REMOVEUNOWNEDWEAPON_THINK_CONTEXT );
	}
#endif

}

bool CWeaponCSBase::DefaultReload( int iClipSize1, int iClipSize2, int iActivity )
{
	if ( BaseClass::DefaultReload( iClipSize1, iClipSize2, iActivity ) )
	{
		SendActivityEvents();
		return true;
	}
	else
	{
		return false;
	}
}

void CWeaponCSBase::SendActivityEvents( int nActEvents )
{
	CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( GetOwner() );
	if ( !pPlayer )
		return;

	// Send a message to any clients that have this entity to play the reload.
	CPASFilter filter( pPlayer->GetAbsOrigin() );
	filter.RemoveRecipient( pPlayer );

	CCSUsrMsg_ReloadEffect msg;
	msg.set_entidx( pPlayer->entindex() );
	msg.set_actanim( nActEvents );

	const Vector& origin = pPlayer->GetAbsOrigin();
	msg.set_origin_x( origin.x );
	msg.set_origin_y( origin.y );
	msg.set_origin_z( origin.z );

	SendUserMessage( filter, CS_UM_ReloadEffect, msg );
	
	if ( nActEvents == ACT_VM_RELOAD || nActEvents == ACT_SECONDARY_VM_RELOAD )
	{	
		// Make the player play his reload animation.
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );
	}
}

#endif


bool CWeaponCSBase::DefaultPistolReload()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 )
		return true;

	if ( !DefaultReload( GetCSWpnData().iDefaultClip1, 0, ACT_VM_RELOAD ) )
		return false;

	pPlayer->m_iShotsFired = 0;

	return true;
}

bool CWeaponCSBase::IsUseable()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( Clip1() <= 0 )
	{
		if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 && GetMaxClip1() != -1 )			
		{
			// clip is empty ( or nonexistant ) and the player has no more ammo of this type. 
			return false;
		}
	}

	return true;
}


#if defined( CLIENT_DLL )

float	g_lateralBob = 0;
float	g_verticalBob = 0;

static ConVar	cl_bob_version( "cl_bob_version","0", FCVAR_CHEAT );
static ConVar	cl_bobcycle( "cl_bobcycle","0.98", FCVAR_ARCHIVE, "the frequency at which the viewmodel bobs.", true, 0.1, true, 2.0 );
//static ConVar	cl_bob( "cl_bob","0.002", FCVAR_ARCHIVE );
static ConVar	cl_bobup( "cl_bobup","0.5", FCVAR_CHEAT );

ConVar	cl_use_new_headbob( "cl_use_new_headbob","1", FCVAR_CHEAT );
static ConVar	cl_bobamt_vert( "cl_bobamt_vert","0.25", FCVAR_ARCHIVE, "The amount the viewmodel moves up and down when running", true, 0.1, true, 2 );
static ConVar	cl_bobamt_lat( "cl_bobamt_lat","0.4", FCVAR_ARCHIVE, "The amount the viewmodel moves side to side when running", true, 0.1, true, 2 );
static ConVar	cl_bob_lower_amt( "cl_bob_lower_amt","21", FCVAR_ARCHIVE, "The amount the viewmodel lowers when running", true, 5, true, 30 );

static ConVar	cl_viewmodel_shift_left_amt( "cl_viewmodel_shift_left_amt","1.5", FCVAR_ARCHIVE, "The amount the viewmodel shifts to the left when shooting accuracy increases.", true, 0.5, true, 2.0 );
static ConVar	cl_viewmodel_shift_right_amt( "cl_viewmodel_shift_right_amt","0.75", FCVAR_ARCHIVE, "The amount the viewmodel shifts to the right when shooting accuracy decreases.", true, 0.25, true, 2.0 );

//-----------------------------------------------------------------------------
// Purpose: Helper function to calculate head bob
//-----------------------------------------------------------------------------
float CalcViewModelBobHelper( CBasePlayer *player, BobState_t *pBobState, int nVMIndex )
{
	if ( cl_use_new_headbob.GetBool() == false )
		return 0;

	Assert( pBobState );
	if ( !pBobState )
		return 0;

	float	cycle;

	//NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it

	if ( ( !gpGlobals->frametime ) || ( player == NULL ) )
	{
		//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
		return 0.0f;// just use old value
	}

	//Find the speed of the player
	float speed = player->GetLocalVelocity().Length2D();

	// do the dip before speed gets clamped
	float flSpeedFactor;

	float flRunAddAmt = 0.0f;

	float flmaxSpeedDelta = MAX( 0, (gpGlobals->curtime - pBobState->m_flLastBobTime ) * 640.0f );//320

//	Msg( "speed raw = %f\n", speed );

	// don't allow too big speed changes
	speed = clamp( speed, pBobState->m_flLastSpeed-flmaxSpeedDelta, pBobState->m_flLastSpeed+flmaxSpeedDelta );
	speed = clamp( speed, -320.0f, 320.0f );

	pBobState->m_flLastSpeed = speed;


	C_BaseViewModel *pViewModel = player ? player->GetViewModel(nVMIndex) : NULL;
	bool bShouldIgnoreOffsetAndAccuracy = (pViewModel && pViewModel->m_bShouldIgnoreOffsetAndAccuracy);

	// when the player is moving forward, the gun lowers a bit -mtw
	C_CSPlayer *pPlayer = ToCSPlayer( player );
	if ( pPlayer && !pPlayer->IsZoomed() )
	{
		flSpeedFactor = speed * 0.006f; 
		clamp( flSpeedFactor, 0.0f, 0.5f );
		float flLowerAmt = cl_bob_lower_amt.GetFloat() * 0.2; 

		// HACK until we get 'ACT_VM_IDLE_LOWERED' anims
		//if ( pPlayer->m_Shared.IsSprinting() )
			//flLowerAmt *= 10.0f;     

		if ( bShouldIgnoreOffsetAndAccuracy )
			flLowerAmt *= 0.1;

		flRunAddAmt = ( flLowerAmt * flSpeedFactor );
	}

	//FIXME: This maximum speed value must come from the server.
	//		 MaxSpeed() is not sufficient for dealing with sprinting - jdw

	float bob_offset = RemapVal( speed, 0.0f, 320.0f, 0.0f, 1.0f );

	pBobState->m_flBobTime += ( gpGlobals->curtime - pBobState->m_flLastBobTime ) * bob_offset;
	pBobState->m_flLastBobTime = gpGlobals->curtime;

	//Msg( "speed = %f\n", speed );

	float flBobCycle = 0.5f;
	float flAccuracyDiff = 0;
	float flGunAccPos = 0;

	
	CWeaponCSBase *pWeapon = ( ( bShouldIgnoreOffsetAndAccuracy || !pPlayer ) ? NULL : pPlayer->GetActiveCSWeapon() );
	if ( pPlayer && pWeapon )
	{
		float flMaxSpeed = pWeapon->GetMaxSpeed();
		flBobCycle = (((1000.0f - flMaxSpeed) / 3.5f) * 0.001f) * cl_bobcycle.GetFloat();

		float flAccuracy = 0.0f;

		bool bIsElites = ( pWeapon->GetCSWeaponID() == WEAPON_ELITE ) ? true : false;

		// don't move the gun around based on accuracy when reloading
		if ( !pWeapon->m_bInReload && !bIsElites )
		{
			// move the gun left or right based on the players current accuracy
			float flCrouchAccuracy = pWeapon->GetInaccuracyCrouch();
			float flBaseAccuracy = pWeapon->GetInaccuracyStand();
			if ( pPlayer->m_Local.m_bDucking || pPlayer->m_Local.m_bDucked )
				flAccuracy = flCrouchAccuracy;
			else
				flAccuracy = pWeapon->GetInaccuracy();

			bool bIsSniper = ( pWeapon->GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE ) ? true : false;

			float flMultiplier = 1;
			if ( flAccuracy < flBaseAccuracy )
			{
				if ( !bIsSniper )
					flMultiplier = 18;
				else
					flMultiplier = 0.15;

				flMultiplier *= cl_viewmodel_shift_left_amt.GetFloat();
			}
			else
			{
				flAccuracy = MIN( flAccuracy, 0.082f );
				flMultiplier *= cl_viewmodel_shift_right_amt.GetFloat();
			}

			// clamp the crouch accuracy
			flAccuracyDiff = MAX( (flAccuracy - flBaseAccuracy) * flMultiplier, -0.1);
		}

/*
		if ( pWeapon->m_flGunAccuracyPosition > 0.0f )
			Msg( "m_flGunAccuracyPosition1 = %f, flAccuracy = %f\n", pWeapon->m_flGunAccuracyPosition, flAccuracy );

		pWeapon->m_flGunAccuracyPosition = Lerp( ( gpGlobals->frametime / 0.02f ), (flAccuracyDiff*20), pWeapon->m_flGunAccuracyPosition );

		if ( pWeapon->m_flGunAccuracyPosition > 0.0f )
			Msg( "m_flGunAccuracyPosition1 = %f, flAccuracy = %f\n", pWeapon->m_flGunAccuracyPosition, flAccuracy );
*/
		pWeapon->m_flGunAccuracyPosition = Approach( (flAccuracyDiff*80), pWeapon->m_flGunAccuracyPosition, abs( ( (flAccuracyDiff*80)-pWeapon->m_flGunAccuracyPosition )*gpGlobals->frametime ) * 4.0f );
		flGunAccPos = pWeapon->m_flGunAccuracyPosition;
	}
	else
	{
		flBobCycle = (((1000.0f - 150) / 3.5f) * 0.001f) * cl_bobcycle.GetFloat();
	}

	//Msg( "flBobCycle = %f\n", flBobCycle );
	//Calculate the vertical bob
	cycle = pBobState->m_flBobTime - (int)(pBobState->m_flBobTime/flBobCycle)*flBobCycle;
	cycle /= flBobCycle;

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = M_PI * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
	}
	float flBobMultiplier = 0.00625f;
	// if we're in the air, slow our bob down a bit
	if ( player->GetGroundEntity() == NULL )
		flBobMultiplier = 0.00125f;

	float flBobVert = bShouldIgnoreOffsetAndAccuracy ? 0.3 : cl_bobamt_vert.GetFloat();
	pBobState->m_flVerticalBob = speed*( flBobMultiplier * flBobVert );
	pBobState->m_flVerticalBob = ( pBobState->m_flVerticalBob*0.3 + pBobState->m_flVerticalBob*0.7*sin(cycle) );
	pBobState->m_flRawVerticalBob = pBobState->m_flVerticalBob;

	pBobState->m_flVerticalBob = clamp( ( pBobState->m_flVerticalBob - ( flRunAddAmt - (flGunAccPos*0.2) ) ), -7.0f, 4.0f );

	//Msg( "pBobState->m_flVerticalBob = %f, cycle = %f, pBobState->m_flBobTime = %f\n", pBobState->m_flVerticalBob, cycle, pBobState->m_flBobTime );

	//Calculate the lateral bob
	cycle = pBobState->m_flBobTime - (int)(pBobState->m_flBobTime/flBobCycle*2)*flBobCycle*2;
	cycle /= flBobCycle*2;

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = M_PI * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
	}

	float flBobLat = bShouldIgnoreOffsetAndAccuracy ? 0.5 : cl_bobamt_lat.GetFloat();
	if ( pPlayer && pWeapon )
	{
		pBobState->m_flLateralBob = speed*( flBobMultiplier * flBobLat );
		pBobState->m_flLateralBob = pBobState->m_flLateralBob*0.3 + pBobState->m_flLateralBob*0.7*sin(cycle);
		pBobState->m_flRawLateralBob = pBobState->m_flLateralBob;

		pBobState->m_flLateralBob = clamp( (pBobState->m_flLateralBob + flGunAccPos*0.25), -8.0f, 8.0f );
		//Msg( "flAccuracyDiff = %f\n", flAccuracyDiff );
	}

	//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Helper function to add head bob
//-----------------------------------------------------------------------------
void AddViewModelBobHelper( Vector &origin, QAngle &angles, BobState_t *pBobState )
{
	if ( cl_use_new_headbob.GetBool() == false )
		return;

	Assert( pBobState );
	if ( !pBobState )
		return;

	Vector	forward, right;
	AngleVectors( angles, &forward, &right, NULL );

	// Apply bob, but scaled down to 40%
	VectorMA( origin, pBobState->m_flVerticalBob * 0.4f, forward, origin );

	// Z bob a bit more
	origin[2] += pBobState->m_flVerticalBob * 0.1f;

	// bob the angles
	angles[ ROLL ]	+= pBobState->m_flVerticalBob * 0.5f;
	angles[ PITCH ]	-= pBobState->m_flVerticalBob * 0.4f;
	angles[ YAW ]	-= pBobState->m_flLateralBob  * 0.3f;

	VectorMA( origin, pBobState->m_flLateralBob * 0.2f, right, origin );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CWeaponCSBase::CalcViewmodelBob( void )
{
	if ( cl_use_new_headbob.GetBool() == true )
	{
		CBasePlayer *player = ToBasePlayer( GetOwner() );
		//Assert( player );
		BobState_t *pBobState = GetBobState();
		if ( pBobState )
		{
			return ::CalcViewModelBobHelper( player, pBobState );
		}
		else
			return 0;
	}

	static	float bobtime;
	static	float lastbobtime;
	static  float lastspeed;
	float	cycle;

	CBasePlayer *player = ToBasePlayer( GetOwner() );
	//Assert( player );

	//NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it

	if ( ( !gpGlobals->frametime ) || 
		( player == NULL ) ||
		( cl_bobcycle.GetFloat() <= 0.0f ) ||
		( cl_bobup.GetFloat() <= 0.0f ) ||
		( cl_bobup.GetFloat() >= 1.0f ) )
	{
		//NOTENOTE: We don't use this return value in our case ( need to restructure the calculation function setup! )
		return 0.0f;// just use old value
	}

	//Find the speed of the player
	float speed = player->GetLocalVelocity().Length2D();
	float flmaxSpeedDelta = MAX( 0, ( gpGlobals->curtime - lastbobtime ) * 320.0f );

	// don't allow too big speed changes
	speed = clamp( speed, lastspeed-flmaxSpeedDelta, lastspeed+flmaxSpeedDelta );
	speed = clamp( speed, -320, 320 );

	lastspeed = speed;

	//FIXME: This maximum speed value must come from the server.
	//		 MaxSpeed() is not sufficient for dealing with sprinting - jdw

	float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );

	bobtime += ( gpGlobals->curtime - lastbobtime ) * bob_offset;
	lastbobtime = gpGlobals->curtime;

	//Calculate the vertical bob
	cycle = bobtime - ( int )( bobtime/cl_bobcycle.GetFloat() )*cl_bobcycle.GetFloat();
	cycle /= cl_bobcycle.GetFloat();

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = M_PI * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = M_PI + M_PI*( cycle-cl_bobup.GetFloat() )/( 1.0 - cl_bobup.GetFloat() );
	}

	g_verticalBob = speed*0.005f;
	g_verticalBob = g_verticalBob*0.3 + g_verticalBob*0.7*sin( cycle );

	g_verticalBob = clamp( g_verticalBob, -7.0f, 4.0f );

	//Calculate the lateral bob
	cycle = bobtime - ( int )( bobtime/cl_bobcycle.GetFloat()*2 )*cl_bobcycle.GetFloat()*2;
	cycle /= cl_bobcycle.GetFloat()*2;

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = M_PI * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = M_PI + M_PI*( cycle-cl_bobup.GetFloat() )/( 1.0 - cl_bobup.GetFloat() );
	}

	g_lateralBob = speed*0.005f;
	g_lateralBob = g_lateralBob*0.3 + g_lateralBob*0.7*sin( cycle );
	g_lateralBob = clamp( g_lateralBob, -7.0f, 4.0f );

	//NOTENOTE: We don't use this return value in our case ( need to restructure the calculation function setup! )
	return 0.0f;

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CWeaponCSBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	if ( cl_use_new_headbob.GetBool() == true )
	{
		// call helper functions to do the calculation
		BobState_t *pBobState = GetBobState();
		if ( pBobState )
		{
			CalcViewmodelBob();
			::AddViewModelBobHelper( origin, angles, pBobState );
		}
		return;
	}

	Vector	forward, right;
	AngleVectors( angles, &forward, &right, NULL );

	CalcViewmodelBob();

	// Apply bob, but scaled down to 40%
	VectorMA( origin, g_verticalBob * 0.4f, forward, origin );

	// Z bob a bit more
	origin[2] += g_verticalBob * 0.1f;

	// bob the angles
	angles[ ROLL ]	+= g_verticalBob * 0.5f;
	angles[ PITCH ]	-= g_verticalBob * 0.4f;

	angles[ YAW ]	-= g_lateralBob  * 0.3f;

	//	VectorMA( origin, g_lateralBob * 0.2f, right, origin );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the head bob state for this weapon, which is stored
//			in the view model.  Note that this this function can return
//			NULL if the player is dead or the view model is otherwise not present.
//-----------------------------------------------------------------------------
BobState_t *CWeaponCSBase::GetBobState()
{
	// get the view model for this weapon
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return NULL;
	CBaseViewModel *baseViewModel = pOwner->GetViewModel( m_nViewModelIndex );
	if ( baseViewModel == NULL )
		return NULL;
	CPredictedViewModel *viewModel = dynamic_cast<CPredictedViewModel *>(baseViewModel);
	if ( viewModel == NULL )
		return NULL;

	//Assert( viewModel );

	// get the bob state out of the view model
	return &( viewModel->GetBobState() );
}

#else

void CWeaponCSBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{

}

float CWeaponCSBase::CalcViewmodelBob( void )
{
	return 0.0f;
}

#endif

#ifndef CLIENT_DLL
bool CWeaponCSBase::PhysicsSplash( const Vector &centerPoint, const Vector &normal, float rawSpeed, float scaledSpeed )
{
	if ( rawSpeed > 20 )
	{

		float size = 4.0f;
		if ( !IsPistol() )
			size += 2.0f;

		// adjust splash size based on speed
		size += RemapValClamped( rawSpeed, 0, 400, 0, 3 );

		CEffectData	data;
		data.m_vOrigin = centerPoint;
		data.m_vNormal = normal;
		data.m_flScale = random->RandomFloat( size, size + 1.0f );

		if ( GetWaterType() & CONTENTS_SLIME )
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}

		DispatchEffect( "gunshotsplash", data );

		return true;
	}

	return false;
}
#endif // !CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPicker - 
//-----------------------------------------------------------------------------
void CWeaponCSBase::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
#if !defined( CLIENT_DLL )
	RemoveEffects( EF_ITEM_BLINK );

	if( pNewOwner->IsPlayer() && pNewOwner->IsAlive() )
	{
		m_OnPlayerPickup.FireOutput( pNewOwner, this );

		m_bFiredOutOfAmmoEvent = false;

		// Robin: We don't want to delete weapons the player has picked up, so 
		// clear the name of the weapon. This prevents wildcards that are meant 
		// to find NPCs finding weapons dropped by the NPCs as well.
		SetName( NULL_STRING );

		CEconEntity *pEconEntity = dynamic_cast< CEconEntity* >( this );
		if ( pEconEntity && m_hPrevOwner == NULL )
		{
			CCSPlayer *pPlayer = ToCSPlayer( pNewOwner );
			// set the original owner XUID
			if ( pPlayer && pEconEntity->GetOriginalOwnerXuid() == 0ull && !pPlayer->IsBot() )
			{
				CSteamID steamID;
				pPlayer->GetSteamID( &steamID );

				uint64 verylong = steamID.ConvertToUint64();
				pEconEntity->SetOriginalOwnerXuid( (uint32)verylong, (uint32)(verylong >> 32) );
			}

			m_iOriginalTeamNumber = pPlayer ? pPlayer->GetTeamNumber() : 0;
		}

		if ( CSGameRules() )
			CSGameRules()->RemoveDroppedWeaponFromList( this );
	}

	// Someone picked me up, so make it so that I can't be removed.
	SetRemoveable( false );
#else
	UpdateOutlineGlow();
#endif

#ifdef IRONSIGHT
	UpdateIronSightController();
#endif //IRONSIGHT

}


void CWeaponCSBase::UpdateAccuracyPenalty( )
{
	CCSPlayer *pPlayer = GetPlayerOwner( );
	if ( !pPlayer )
		return;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData( );

	float fNewPenalty = 0.0f;

	// Adding movement penalty here results in a penalty that persists and requires decay to eliminate.
#if MOVEMENT_ACCURACY_DECAYED
	// movement penalty
	fNewPenalty += RemapValClamped( pPlayer->GetAbsVelocity().Length2D(), 
		weaponInfo.GetMaxSpeed(m_weaponMode, GetEconItemView()) * CS_PLAYER_SPEED_DUCK_MODIFIER, 
		weaponInfo.GetMaxSpeed(m_weaponMode, GetEconItemView()) * 0.95f,							// max out at 95% of run speed to avoid jitter near max speed
		0.0f, weaponInfo.GetInaccuracyMove( m_weaponMode, GetEconItemView( ) );
#endif


	// on ladder?
	if ( pPlayer->GetMoveType( ) == MOVETYPE_LADDER )
	{
		fNewPenalty += weaponInfo.GetInaccuracyLadder( GetEconItemView( ), m_weaponMode ) + weaponInfo.GetInaccuracyLadder( GetEconItemView( ), Primary_Mode );
	}
	// in the air?
	else if ( pPlayer->GetGroundEntity() == nullptr )
	// 	else if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
	{
		fNewPenalty += weaponInfo.GetInaccuracyStand( GetEconItemView(), m_weaponMode );
		fNewPenalty += weaponInfo.GetInaccuracyJump( GetEconItemView(), m_weaponMode ) * weapon_air_spread_scale.GetFloat();
	}
	else if ( FBitSet( pPlayer->GetFlags( ), FL_DUCKING ) )
	{
		fNewPenalty += weaponInfo.GetInaccuracyCrouch( GetEconItemView( ), m_weaponMode );
	}
	else
	{
		fNewPenalty += weaponInfo.GetInaccuracyStand( GetEconItemView( ), m_weaponMode );
	}

	if ( m_bInReload )
	{
		fNewPenalty += weaponInfo.GetInaccuracyReload( GetEconItemView( ) );
	}

	if ( fNewPenalty > m_fAccuracyPenalty )
	{
		m_fAccuracyPenalty = fNewPenalty;
	}
	else
	{
		float fDecayFactor = logf( 10.0f ) / GetRecoveryTime( );

		m_fAccuracyPenalty = Lerp( expf( TICK_INTERVAL * -fDecayFactor ), fNewPenalty, ( float ) m_fAccuracyPenalty );
	}


#define WEAPON_RECOIL_DECAY_THRESHOLD 1.10	
	// Decay the recoil index if a little more than cycle time has elapsed since the last shot. In other words,
	// don't decay if we're firing full-auto.
	if ( gpGlobals->curtime > m_fLastShotTime + ( GetCycleTime() * WEAPON_RECOIL_DECAY_THRESHOLD ) )
	{
		float fDecayFactor = logf( 10.0f ) * weapon_recoil_decay_coefficient.GetFloat( );

		m_flRecoilIndex = Lerp( expf( TICK_INTERVAL * -fDecayFactor ), 0.0f, ( float ) m_flRecoilIndex );
	}
}

float CWeaponCSBase::GetRecoveryTime( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner( );
	if ( !pPlayer )
		return -1.0f;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData( );

	if ( pPlayer->GetMoveType( ) == MOVETYPE_LADDER )
	{
		return weaponInfo.GetRecoveryTimeStand( GetEconItemView( ) );
	}
	else if ( !FBitSet( pPlayer->GetFlags( ), FL_ONGROUND ) )	// in air
	{
		// enforce a large recovery speed penalty (400%) for players in the air; this helps to provide
		// comparable in-air accuracy to the old weapon model
		
		return weaponInfo.GetRecoveryTimeCrouch( GetEconItemView( ) ) * 4.0f;
	}
	else if ( FBitSet( pPlayer->GetFlags( ), FL_DUCKING ) )
	{
		float flRecoveryTime = weaponInfo.GetRecoveryTimeCrouch( GetEconItemView( ) );
		float flRecoveryTimeFinal = weaponInfo.GetRecoveryTimeCrouchFinal( GetEconItemView( ) );

		if ( flRecoveryTimeFinal != -1.0f )	// uninitialized final recovery values are set to -1.0 from the weapon_base prefab in schema
		{
			int nRecoilIndex = m_flRecoilIndex;

			flRecoveryTime = RemapValClamped( nRecoilIndex, weaponInfo.GetRecoveryTransitionStartBullet( GetEconItemView( ) ), weaponInfo.GetRecoveryTransitionEndBullet( GetEconItemView( ) ), flRecoveryTime, flRecoveryTimeFinal );
		}

		return flRecoveryTime;
	}
	else
	{
		float flRecoveryTime = weaponInfo.GetRecoveryTimeStand( GetEconItemView( ) );
		float flRecoveryTimeFinal = weaponInfo.GetRecoveryTimeStandFinal( GetEconItemView( ) );

		if ( flRecoveryTimeFinal != -1.0f )	// uninitialized final recovery values are set to -1.0 from the weapon_base prefab in schema
		{
			int nRecoilIndex = m_flRecoilIndex;

			flRecoveryTime = RemapValClamped( nRecoilIndex, weaponInfo.GetRecoveryTransitionStartBullet( GetEconItemView( ) ), weaponInfo.GetRecoveryTransitionEndBullet( GetEconItemView( ) ), flRecoveryTime, flRecoveryTimeFinal );
		}

		return flRecoveryTime;
	}
}




void CWeaponCSBase::OnJump( float fImpulse )
{
}

void CWeaponCSBase::OnLand( float fVelocity )
{
	float fPenalty = GetCSWpnData().GetInaccuracyLand( GetEconItemView(), m_weaponMode ) * fVelocity;
	m_fAccuracyPenalty += fPenalty;
	fPenalty = clamp( fPenalty, -1.0f, 1.0f );

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// NOTE: do NOT call GetAimPunchAngle() here because it may be adjusted by some recoil scalar.
	// We just want to update the raw punch angle.
	QAngle angle = pPlayer->GetRawAimPunchAngle();
	float fVKick = RAD2DEG(asinf(fPenalty)) * 0.2f;
	float fHKick = SharedRandomFloat("LandPunchAngleYaw", -1.0f, +1.0f) * fVKick * 0.1f;

	angle.x += fVKick;	// pitch
	angle.y += fHKick;	// yaw

	pPlayer->SetAimPunchAngle( angle );
}


void CWeaponCSBase::Recoil( CSWeaponMode weaponMode )
{
	/** Removed for partner depot **/
}

#ifdef CLIENT_DLL

void WeaponSaveCustomTextures( void )
{
	C_BasePlayer *local = C_BasePlayer::GetLocalPlayer();
	if ( local )
	{
		CWeaponCSBase *pWeapon = ( CWeaponCSBase* )local->GetActiveWeapon();
		pWeapon->SaveCustomMaterialsTextures();
	}
}
ConCommand cl_saveweaponcustomtextures( "cl_saveweaponcustomtextures", WeaponSaveCustomTextures, "Save custom textures of current weapon." );

void CWeaponCSBase::SaveCustomMaterialsTextures( )
{
	/** Removed for partner depot **/
}

void CWeaponCSBase::UpdateCustomMaterial( void )
{
	/** Removed for partner depot **/
}

void CWeaponCSBase::CheckCustomMaterial( void )
{
	/** Removed for partner depot **/
}

#endif // CLIENT_DLL

#ifdef IRONSIGHT
CIronSightController *CWeaponCSBase::GetIronSightController( void )
{
	if ( m_IronSightController && m_IronSightController->IsInitializedAndAvailable() )
	{
		return m_IronSightController;
	}
	return NULL;
}

void CWeaponCSBase::UpdateIronSightController()
{
	if (!m_IronSightController)
		m_IronSightController = new CIronSightController();
	
	if (m_IronSightController)
		m_IronSightController->Init(this);
}
#endif

int CWeaponCSBase::GetZoomFOV( int nZoomLevel ) const
{
	switch( nZoomLevel )
	{
	case 0: return 0; // not used -- always default FoV
	case 1:	return GetCSWpnData().GetZoomFOV1();
	case 2:	return GetCSWpnData().GetZoomFOV2();
	default: Assert(0); return 0;
	}
}

float CWeaponCSBase::GetZoomTime( int nZoomLevel ) const
{
	switch ( nZoomLevel )
	{
	case 0:	return GetCSWpnData().GetZoomTime0();
	case 1:	return GetCSWpnData().GetZoomTime1();
	case 2:	return GetCSWpnData().GetZoomTime2();
	default: Assert( 0 ); return 0;
	}
}

CCSPlayer * CWeaponCSBase::GetOriginalOwner()
{
	if ( m_PriorOwners.Count() )
		return m_PriorOwners.Element( 0 );

	return NULL;
}

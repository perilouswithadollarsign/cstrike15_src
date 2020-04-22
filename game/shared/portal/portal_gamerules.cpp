//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The Half-Life 2 game rules, such as the relationship tables and ammo
//			damage cvars.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "portal_gamerules.h"
#include "ammodef.h"
#include "hl2_shareddefs.h"
#include "portal_shareddefs.h"
#include "weapon_portalgun_shared.h"

#ifndef CLIENT_DLL
#include "player_voice_listener.h"
#endif // CLIENT_DLL

#ifdef CLIENT_DLL
#ifndef NO_STEAM
	#include "steam/steam_api.h"
#endif //NO_STEAM
	#include "c_user_message_register.h"
#else
	#include "player.h"
	#include "game.h"
	#include "gamerules.h"
	#include "teamplay_gamerules.h"
	#include "portal_player.h"
	#include "globalstate.h"
	#include "ai_basenpc.h"
	#include "portal/weapon_physcannon.h"
	#include "props.h"		// For props flags used in making the portal weight box
	#include "datacache/imdlcache.h"	// For precaching box model
	#include "vscript_server.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
	extern ConVar locator_lerp_rest;
	extern ConVar locator_start_at_crosshair;
	extern ConVar locator_topdown_style;
	extern ConVar locator_background_style;
	extern ConVar locator_background_color;
	extern ConVar locator_background_thickness_x;
	extern ConVar locator_background_thickness_y;
	extern ConVar locator_target_offset_x;
	extern ConVar locator_target_offset_y;
	extern ConVar locator_background_shift_x;
	extern ConVar locator_background_shift_y;
	extern ConVar locator_background_border_color;
	extern ConVar locator_icon_min_size_non_ss;
	extern ConVar locator_icon_max_size_non_ss;

#endif // CLIENT_DLL

REGISTER_GAMERULES_CLASS( CPortalGameRules );

BEGIN_NETWORK_TABLE_NOBASE( CPortalGameRules, DT_PortalGameRules )
END_NETWORK_TABLE()


IMPLEMENT_NETWORKCLASS_ALIASED( PortalGameRulesProxy, DT_PortalGameRulesProxy )
LINK_ENTITY_TO_CLASS_ALIASED( portal_gamerules, PortalGameRulesProxy );


#ifdef CLIENT_DLL
	void RecvProxy_PortalGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CPortalGameRules *pRules = PortalGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CPortalGameRulesProxy, DT_PortalGameRulesProxy )
		RecvPropDataTable( "portal_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_PortalGameRules ), RecvProxy_PortalGameRules )
	END_RECV_TABLE()
#else
	void* SendProxy_PortalGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CPortalGameRules *pRules = PortalGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CPortalGameRulesProxy, DT_PortalGameRulesProxy )
		SendPropDataTable( "portal_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_PortalGameRules ), SendProxy_PortalGameRules )
	END_SEND_TABLE()
#endif


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
CPortalGameRules::CPortalGameRules()
{
#ifndef CLIENT_DLL
	g_pCVar->FindVar( "sv_maxreplay" )->SetValue( "1.5" );

	if ( !GlobalEntity_IsInTable( "player_regenerates_health" ) )
		GlobalEntity_Add( MAKE_STRING("player_regenerates_health"), gpGlobals->mapname, GLOBAL_ON );
	else
		GlobalEntity_SetState( MAKE_STRING("player_regenerates_health"), GLOBAL_ON );

	if ( gpGlobals->mapname.ToCStr() && StringHasPrefix( gpGlobals->mapname.ToCStr(), "mp_coop" ) )
	{
		static ConVarRef flashlightbrightness( "r_flashlightbrightness" );
		if ( flashlightbrightness.IsValid() )
		{
			// All MP maps use this brightness but don't set it explicitly... we need this here for MP maps in commentary mode
			flashlightbrightness.SetValue( 0.25f );
		}
	}

#else
	locator_lerp_rest.SetValue( 0.0f );
	locator_start_at_crosshair.SetValue( 0 );
	locator_topdown_style.SetValue( 0 );
	locator_background_style.SetValue( 0 );
	locator_background_color.SetValue( "0 0 0 128");
	locator_target_offset_x.SetValue( 0 );
	locator_target_offset_y.SetValue( 0 );
	locator_background_thickness_x.SetValue( 12 );
	locator_background_thickness_y.SetValue( 12 );
	locator_background_shift_x.SetValue( 0 );
	locator_background_shift_y.SetValue( 0 );
	locator_background_border_color.SetValue( "32 32 32 64" );
	locator_icon_min_size_non_ss.SetValue( 1.5f );
	locator_icon_max_size_non_ss.SetValue( 1.75f );
#endif
}


#ifndef CLIENT_DLL

// ------------------------------------------------------------------------------------
//  Parse commands coming down from the client
// ------------------------------------------------------------------------------------
bool CPortalGameRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
	const char *pcmd = args[0];
	if ( FStrEq( pcmd, "lobby_select_day" ) )
	{
		if ( args.ArgC() < 2 )
			return true;

		//int nDay = atoi( args[1] );
		// Msg("Selecting day %d\n", nDay );
		return true;
	}

	return BaseClass::ClientCommand( pEdict, args );
}

// ------------------------------------------------------------------------------------
// 
// ------------------------------------------------------------------------------------
const char *CPortalGameRules::GetGameDescription( void )
{
#ifdef PORTAL2
	return "Portal 2";
#else
	return "Portal";
#endif
}

// ------------------------------------------------------------------------------------
// 
// ------------------------------------------------------------------------------------
bool CPortalGameRules::AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	return BaseClass::AllowDamage( pVictim, info );
}

bool CPortalGameRules::IsSavingAllowed( void )
{
	if ( UTIL_GetLocalPlayerOrListenServerHost()->GetBonusChallenge() > 0 )
	{
		return false;
	}
	return true;
}

#else

// ------------------------------------------------------------------------------------
// 
// ------------------------------------------------------------------------------------
bool CPortalGameRules::IsBonusChallengeTimeBased( void )
{
	CBasePlayer* pPlayer = UTIL_PlayerByIndex( 1 );
	if ( !pPlayer )
		return true;

	int iBonusChallenge = pPlayer->GetBonusChallenge();
	if ( iBonusChallenge == PORTAL_CHALLENGE_TIME || iBonusChallenge == PORTAL_CHALLENGE_NONE )
		return true;

	return false;
}


bool CPortalGameRules::IsChallengeMode()
{
	CBasePlayer* pPlayer = UTIL_PlayerByIndex( 1 );
	if ( pPlayer )
		return pPlayer->GetBonusChallenge() != 0;

	return false;
}


#endif// !( CLIENT_DLL )

bool CPortalGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		V_swap(collisionGroup0,collisionGroup1);
	}

	// Cubes shouldn't collide with debris but should otherwise act like COLLISION_GROUP_NONE
	if( collisionGroup1 == COLLISION_GROUP_WEIGHTED_CUBE && collisionGroup0 == COLLISION_GROUP_DEBRIS )
		return false;

	if( collisionGroup0 == COLLISION_GROUP_WEIGHTED_CUBE )
		collisionGroup0 = COLLISION_GROUP_NONE;

	if( collisionGroup1 == COLLISION_GROUP_WEIGHTED_CUBE )
		collisionGroup1 = COLLISION_GROUP_NONE;

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}











// ------------------------------------------------------------------------------------ //
// Global functions.
// ------------------------------------------------------------------------------------ //

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			3.5
// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)


CAmmoDef *GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;
	
	if ( !bInitted )
	{
		bInitted = true;

		def.AddAmmoType("AR2",				DMG_BULLET,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_ar2",			"sk_npc_dmg_ar2",			"sk_max_ar2",			BULLET_IMPULSE(200, 1225), 0 );
		def.AddAmmoType("AlyxGun",			DMG_BULLET,					TRACER_LINE,			"sk_plr_dmg_alyxgun",		"sk_npc_dmg_alyxgun",		"sk_max_alyxgun",		BULLET_IMPULSE(200, 1225), 0 );
		def.AddAmmoType("Pistol",			DMG_BULLET,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_pistol",		"sk_npc_dmg_pistol",		"sk_max_pistol",		BULLET_IMPULSE(200, 1225), 0 );
		def.AddAmmoType("SMG1",				DMG_BULLET,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_smg1",			"sk_npc_dmg_smg1",			"sk_max_smg1",			BULLET_IMPULSE(200, 1225), 0 );
		def.AddAmmoType("357",				DMG_BULLET,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_357",			"sk_npc_dmg_357",			"sk_max_357",			BULLET_IMPULSE(800, 5000), 0 );
		def.AddAmmoType("XBowBolt",			DMG_BULLET,					TRACER_LINE,			"sk_plr_dmg_crossbow",		"sk_npc_dmg_crossbow",		"sk_max_crossbow",		BULLET_IMPULSE(800, 8000), 0 );

		def.AddAmmoType("Buckshot",			DMG_BULLET | DMG_BUCKSHOT,	TRACER_LINE,			"sk_plr_dmg_buckshot",		"sk_npc_dmg_buckshot",		"sk_max_buckshot",		BULLET_IMPULSE(400, 1200), 0 );
		def.AddAmmoType("RPG_Round",		DMG_BURN,					TRACER_NONE,			"sk_plr_dmg_rpg_round",		"sk_npc_dmg_rpg_round",		"sk_max_rpg_round",		0, 0 );
		def.AddAmmoType("SMG1_Grenade",		DMG_BURN,					TRACER_NONE,			"sk_plr_dmg_smg1_grenade",	"sk_npc_dmg_smg1_grenade",	"sk_max_smg1_grenade",	0, 0 );
		def.AddAmmoType("SniperRound",		DMG_BULLET | DMG_SNIPER,	TRACER_NONE,			"sk_plr_dmg_sniper_round",	"sk_npc_dmg_sniper_round",	"sk_max_sniper_round",	BULLET_IMPULSE(650, 6000), 0 );
		def.AddAmmoType("SniperPenetratedRound", DMG_BULLET | DMG_SNIPER, TRACER_NONE,			"sk_dmg_sniper_penetrate_plr", "sk_dmg_sniper_penetrate_npc", "sk_max_sniper_round", BULLET_IMPULSE(150, 6000), 0 );
		def.AddAmmoType("Grenade",			DMG_BURN,					TRACER_NONE,			"sk_plr_dmg_grenade",		"sk_npc_dmg_grenade",		"sk_max_grenade",		0, 0);
		def.AddAmmoType("Thumper",			DMG_SONIC,					TRACER_NONE,			10, 10, 2, 0, 0 );
		def.AddAmmoType("Gravity",			DMG_CLUB,					TRACER_NONE,			0,	0, 8, 0, 0 );
		def.AddAmmoType("Battery",			DMG_CLUB,					TRACER_NONE,			NULL, NULL, NULL, 0, 0 );
#ifndef PORTAL2
		def.AddAmmoType("GaussEnergy",		DMG_SHOCK,					TRACER_NONE,			"sk_jeep_gauss_damage",		"sk_jeep_gauss_damage", "sk_max_gauss_round", BULLET_IMPULSE(650, 8000), 0 ); // hit like a 10kg weight at 400 in/s
#endif
		def.AddAmmoType("CombineCannon",	DMG_BULLET,					TRACER_LINE,			"sk_npc_dmg_gunship_to_plr", "sk_npc_dmg_gunship", NULL, 1.5 * 750 * 12, 0 ); // hit like a 1.5kg weight at 750 ft/s
		def.AddAmmoType("AirboatGun",		DMG_AIRBOAT,				TRACER_LINE,			"sk_plr_dmg_airboat",		"sk_npc_dmg_airboat",		NULL,					BULLET_IMPULSE(10, 600), 0 );
		def.AddAmmoType("StriderMinigun",	DMG_BULLET,					TRACER_LINE,			5, 5, 15, 1.0 * 750 * 12, AMMO_FORCE_DROP_IF_CARRIED ); // hit like a 1.0kg weight at 750 ft/s
#ifndef PORTAL2
		def.AddAmmoType("HelicopterGun",	DMG_BULLET,					TRACER_LINE_AND_WHIZ,	"sk_npc_dmg_helicopter_to_plr", "sk_npc_dmg_helicopter",	"sk_max_smg1",	BULLET_IMPULSE(400, 1225), AMMO_FORCE_DROP_IF_CARRIED | AMMO_INTERPRET_PLRDAMAGE_AS_DAMAGE_TO_PLAYER );
#endif
		def.AddAmmoType("AR2AltFire",		DMG_DISSOLVE,				TRACER_NONE,			0, 0, "sk_max_ar2_altfire", 0, 0 );

		def.AddAmmoType("PortalTurretBullet", DMG_BULLET, TRACER_LINE_AND_WHIZ,	3, 3, 150, BULLET_IMPULSE(200, 1225), 0 );
	}

	return &def;
}


///////////////////////////////////////////////////////////////////////
// Portal singleplayer specific global vscript functions
///////////////////////////////////////////////////////////////////////
#if !defined ( CLIENT_DLL )
static bool ScriptIsMultiplayer( void )
{
	return false;//g_pGameRules->IsMultiplayer();
}

static bool TryDLC1InstalledOrCatch( void )
{
	return true;
}

extern float GetPlayerSilenceDuration( int nPlayer );
extern int GetOrangePlayerIndex( void );
extern int GetBluePlayerIndex( void );
extern int GetCoopSectionIndex( void );
extern int GetCoopBranchLevelIndex( int nBranch );
extern int GetHighestActiveBranch( void );
extern void AddBranchLevelName( int nBranch, const char *pchName );
extern void MarkMapComplete( const char *pchName );
extern bool IsLevelComplete( int nBranch, int nLevel );
extern bool IsPlayerLevelComplete( int nPlayer, int nBranch, int nLevel );
extern void AddCoopCreditsName( const char *pchName );

bool ScriptSteamShowURL( const char *pURL )
{
#if !defined(NO_STEAM)
	if ( steamapicontext && steamapicontext->SteamFriends() &&
		steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->IsOverlayEnabled() )
	{
		steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( pURL );
		return true;
	}
#endif

	return false;
}

void ScriptShowHudMessageAll( const char *pMsg, float flHoldTime )
{
	hudtextparms_t tTextParam = {0};
	tTextParam.x			= -1;
	tTextParam.y			= -1;
	tTextParam.effect		= 0;
	tTextParam.r1			= 255;
	tTextParam.g1			= 255;
	tTextParam.b1			= 255;
	tTextParam.a1			= 255;
	tTextParam.r2			= 255;
	tTextParam.g2			= 255;
	tTextParam.b2			= 255;
	tTextParam.a2			= 255;
	tTextParam.fadeinTime	= 0;
	tTextParam.fadeoutTime	= 0;
	tTextParam.holdTime		= flHoldTime;
	tTextParam.fxTime		= 0;
	tTextParam.channel		= 1;
	UTIL_HudMessageAll( tTextParam, pMsg );
}

void GivePlayerPortalgun( void )
{
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CPortal_Player *pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			pPlayer->GivePlayerPortalGun( false, true );
		}
	}
}

void UpgradePlayerPortalgun( void )
{
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CPortal_Player *pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			CWeaponPortalgun *pPortalGun = static_cast< CWeaponPortalgun* >( pPlayer->Weapon_OwnsThisType( "weapon_portalgun" ) );
			if ( pPortalGun )
			{
				pPortalGun->SetCanFirePortal1();
				pPortalGun->SetCanFirePortal2();
			}
			else
			{
				DevMsg( "Portalgun upgrade failed! Player not holding a portalgun.\n");
			}
		}
	}
}

void UpgradePlayerPotatogun( void )
{
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CPortal_Player *pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			CWeaponPortalgun *pPortalGun = static_cast< CWeaponPortalgun* >( pPlayer->Weapon_OwnsThisType( "weapon_portalgun" ) );
			if ( pPortalGun )
			{
				pPortalGun->SetCanFirePortal1();
				pPortalGun->SetCanFirePortal2();
				pPortalGun->SetPotatosOnPortalgun( true );
			}
			else
			{
				DevMsg( "Potatogun upgrade failed! Player not holding a portalgun.\n");
			}
		}
	}
}


HSCRIPT GetPlayer( void )
{
	return ToHScript( UTIL_GetLocalPlayer() );	
}

void CPortalGameRules::RegisterScriptFunctions( void )
{
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptIsMultiplayer, "IsMultiplayer", "Is this a multiplayer game?" );
	ScriptRegisterFunction( g_pScriptVM, GetPlayerSilenceDuration, "Time that the specified player has been silent on the mic." );
	ScriptRegisterFunction( g_pScriptVM, GetOrangePlayerIndex, "Player index of the orange player." );
	ScriptRegisterFunction( g_pScriptVM, GetBluePlayerIndex, "Player index of the blue player." );
	ScriptRegisterFunction( g_pScriptVM, GetCoopSectionIndex, "Section that the coop players have selected to load." );
	ScriptRegisterFunction( g_pScriptVM, GetCoopBranchLevelIndex, "Given the 'branch' argument, returns the current chosen level." );
	ScriptRegisterFunction( g_pScriptVM, GetHighestActiveBranch, "Returns which branches should be available in the hub." );
	ScriptRegisterFunction( g_pScriptVM, AddBranchLevelName, "Adds a level to the specified branche's list." );
	ScriptRegisterFunction( g_pScriptVM, MarkMapComplete, "Marks a maps a complete for both players." );
	ScriptRegisterFunction( g_pScriptVM, IsLevelComplete, "Returns true if the level in the specified branch is completed by either player." );
	ScriptRegisterFunction( g_pScriptVM, IsPlayerLevelComplete, "Returns true if the level in the specified branch is completed by a specific player." );
	ScriptRegisterFunction( g_pScriptVM, GetPlayer, "Returns the player (SP Only)." );
	ScriptRegisterFunction( g_pScriptVM, PrecacheMovie, "Precaches a named movie. Only valid to call within the entity's 'Precache' function called on mapspawn." );
	ScriptRegisterFunction( g_pScriptVM, AddCoopCreditsName, "Adds a name to the coop credit's list." );
	ScriptRegisterFunction( g_pScriptVM, ScriptSteamShowURL, "Bring up the steam overlay and shows the specified URL.  (Full address with protocol type is required, e.g. http://www.steamgames.com/)" );
	ScriptRegisterFunction( g_pScriptVM, ScriptShowHudMessageAll, "Show center print text message." );
	ScriptRegisterFunction( g_pScriptVM, GivePlayerPortalgun, "Give player the portalgun." );
	ScriptRegisterFunction( g_pScriptVM, UpgradePlayerPortalgun, "Give player the portalgun." );
	ScriptRegisterFunction( g_pScriptVM, UpgradePlayerPotatogun, "Give player the portalgun." );
	ScriptRegisterFunction( g_pScriptVM, TryDLC1InstalledOrCatch, "Tests if the DLC1 is installed for Try/Catch blocks." );
	g_pScriptVM->RegisterInstance( &PlayerVoiceListener(), "PlayerVoiceListener" );
}
#endif // !CLIENT_DLL

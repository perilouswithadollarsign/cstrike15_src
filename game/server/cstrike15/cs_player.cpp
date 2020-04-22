//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cs_player.h"
#include "cs_gamerules.h"
#include "trains.h"
#include "vcollide_parse.h"
#include "in_buttons.h"
#include "igamemovement.h"
#include "ai_hull.h"
#include "ndebugoverlay.h"
#include "weapon_csbase.h"
#include "decals.h"
#include "cs_ammodef.h"
#include "IEffects.h"
#include "cs_client.h"
#include "client.h"
#include "cs_shareddefs.h"
#include "shake.h"
#include "team.h"
#include "items.h"
#include "weapon_c4.h"
#include "weapon_parse.h"
#include "weapon_knife.h"
#include "movehelper_server.h"
#include "tier0/vprof.h"
#include "te_effect_dispatch.h"
#include "vphysics/player_controller.h"
#include "weapon_hegrenade.h"
#include "weapon_flashbang.h"
#include "weapon_smokegrenade.h"
#include "weapon_molotov.h"
#include "weapon_decoy.h"
#include "weapon_sensorgrenade.h"
//#include "weapon_carriable_item.h"
#include <keyvalues.h>
#include "engine/IEngineSound.h"
#include "bot.h"
#include "studio.h"
#include <coordsize.h>
#include "predicted_viewmodel.h"
#include "props_shared.h"
#include "tier0/icommandline.h"
#include "info_camera_link.h"
#include "hintmessage.h"
#include "obstacle_pushaway.h"
#include "movevars_shared.h"
#include "death_pose.h"
#include "basecsgrenade_projectile.h"
#include "hegrenade_projectile.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "CRagdollMagnet.h"
#include "datacache/imdlcache.h"
#include "npcevent.h"
#include "cs_gamestats.h"
#include "GameStats.h"
#include "cs_achievement_constants.h"
#include "cs_simple_hostage.h"
#include "cs_weapon_parse.h"
#include "sendprop_priorities.h"
#include "achievementmgr.h"
#include "fmtstr.h"
#include "gametypes/igametypes.h"
#include "weapon_c4.h"
#include "cs_shareddefs.h"
#include "inputsystem/iinputsystem.h"
#include "platforminputdevice.h"
#include "Effects/inferno.h"
#include "cs_entity_spotting.h"
#include "entityutil.h"
#include "vehicle_base.h"
#include "cs_player_resource.h"
#include "cs_entity_spotting.h"
#include "particle_parse.h"
#include "mapinfo.h"
#include "cstrike15_item_system.h"
//#include "particle_parse.h"
#include "../public/vstdlib/vstrtools.h"
#include "../public/vgui/ILocalize.h"
#include "../shared/cstrike15/flashbang_projectile.h"
#include "usermessages.h"
#include "teamplayroundbased_gamerules.h"
#include "animation.h"
#include "cs_team.h"
#include "econ_game_account_client.h"
#include "world.h"
#include "item_healthshot.h"
#include "hltvdirector.h"
#include "ihltv.h"
#include "netmessages.pb.h"
#include "econ_item_view_helpers.h"
#include "playerdecals_signature.h"

#if defined( CLIENT_DLL )

	#include "custom_material.h"
	#include "cs_custom_clothing_visualsdata_processor.h"

#endif

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	#include "econ_gcmessages.h"
	#include "econ_entity_creation.h"
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// HPE TEMP - REMOVE WHEN DONE TESTING
#define REPORT_PLAYER_DAMAGE 0

#pragma warning( disable : 4355 )

// Minimum interval between rate-limited commands that players can run.
#define CS_COMMAND_MAX_RATE 0.3

#define WEARABLE_VEST_IFF_KEVLAR 0	// If set to 1, character wears vest if and only if they have game kevlar.

const char *g_pszHeavyPhoenixModel = "models/player/custom_player/legacy/tm_phoenix_heavy.mdl";

const float CycleLatchInterval = 0.2f;

const int cGunGameSelectMaxAmmoAmount = 250;

#define SPOTTED_ENTITY_UPDATE_INTERVAL 0.5f
#define SPOTTED_ENTITY_COUNT_MESSAGE_MAX 40

#define CS_PUSHAWAY_THINK_CONTEXT	"CSPushawayThink"

extern ConVar mp_maxrounds;
ConVar cs_ShowStateTransitions( "cs_ShowStateTransitions", "-2", FCVAR_CHEAT, "cs_ShowStateTransitions <ent index or -1 for all>. Show player state transitions." );
ConVar sv_max_usercmd_future_ticks( "sv_max_usercmd_future_ticks", "8", 0, "Prevents clients from running usercmds too far in the future. Prevents speed hacks." );
ConVar mp_logmoney( "mp_logmoney", "0", FCVAR_RELEASE, "Enables money logging.  Values are: 0=off, 1=on", true, 0.0f, true, 1.0f );
extern ConVar cs_AssistDamageThreshold;
extern ConVar mp_spawnprotectiontime;
extern ConVar mp_td_spawndmgthreshold;
extern ConVar mp_td_dmgtowarn;
extern ConVar mp_td_dmgtokick;
extern ConVar sv_kick_ban_duration;
extern ConVar dev_reportmoneychanges;
extern ConVar mp_randomspawn;
extern ConVar mp_dm_bonus_percent;
extern ConVar mp_respawn_on_death_t;
extern ConVar mp_respawn_on_death_ct;
extern ConVar mp_ct_default_melee;
extern ConVar mp_ct_default_secondary;
extern ConVar mp_ct_default_primary;
extern ConVar mp_ct_default_grenades;
extern ConVar mp_t_default_melee;
extern ConVar mp_t_default_secondary;
extern ConVar mp_t_default_primary;
extern ConVar mp_t_default_grenades;
extern ConVar mp_damage_scale_ct_body;
extern ConVar mp_damage_scale_ct_head;
extern ConVar mp_damage_scale_t_body;
extern ConVar mp_damage_scale_t_head;
extern ConVar mp_randomspawn_los;
extern ConVar mp_randomspawn_dist;
extern ConVar mp_use_respawn_waves;
extern ConVar cash_player_respawn_amount;
extern ConVar cash_player_get_killed;
extern ConVar mp_death_drop_c4;
extern ConVar mp_buy_anywhere;
extern ConVar sv_staminamax;
extern ConVar sv_coaching_enabled;
extern ConVar mp_coop_force_join_ct;
extern ConVar mp_guardian_special_weapon_needed;
extern ConVar mp_guardian_player_dist_min;
extern ConVar mp_guardian_player_dist_max;
extern ConVar mp_guardian_target_site;
extern ConVar mp_player_healthbuffer_decay_rate;

// friendly fire damage scalers
extern ConVar ff_damage_reduction_grenade;
extern ConVar ff_damage_reduction_grenade_self;
extern ConVar ff_damage_reduction_bullets;
extern ConVar ff_damage_reduction_other;


ConVar phys_playerscale( "phys_playerscale", "10.0", FCVAR_REPLICATED, "This multiplies the bullet impact impuse on players for more dramatic results when players are shot." );
ConVar phys_headshotscale( "phys_headshotscale", "1.3", FCVAR_REPLICATED, "Modifier for the headshot impulse hits on players" );

ConVar sv_damage_print_enable( "sv_damage_print_enable", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "Turn this off to disable the player's damage feed in the console after getting killed.");

ConVar sv_spawn_afk_bomb_drop_time( "sv_spawn_afk_bomb_drop_time", "15", FCVAR_REPLICATED | FCVAR_RELEASE, "Players that have never moved since they spawned will drop the bomb after this amount of time." );
extern ConVar spec_replay_winddown_time;

ConVar mp_drop_knife_enable( "mp_drop_knife_enable", "0", FCVAR_RELEASE, "Allows players to drop knives." );

static ConVar tv_relayradio( "tv_relayradio", "0", FCVAR_RELEASE, "Relay team radio commands to TV: 0=off, 1=on" );

// [Jason] Allow us to turn down the frequency of the damage notification
ConVar CS_WarnFriendlyDamageInterval( "CS_WarnFriendlyDamageInterval", "3.0", FCVAR_CHEAT, "Defines how frequently the server notifies clients that a player damaged a friend" );


const char* g_pszLootModelName = "models/props/props_gameplay/money_bag.mdl";

ConVar sv_guardian_min_wave_for_heavy( "sv_guardian_min_wave_for_heavy", "0", FCVAR_RELEASE | FCVAR_GAMEDLL );
ConVar sv_guardian_max_wave_for_heavy( "sv_guardian_max_wave_for_heavy", "0", FCVAR_RELEASE | FCVAR_GAMEDLL );
ConVar sv_guardian_heavy_count( "sv_guardian_heavy_count", "0", FCVAR_RELEASE | FCVAR_GAMEDLL );
ConVar sv_guardian_heavy_all( "sv_guardian_heavy_all", "0", FCVAR_RELEASE | FCVAR_GAMEDLL );

// [Forrest] Allow MVP to be turned off for a server
// [Forrest] Allow freezecam to be turned off for a server
// [Forrest] Allow win panel to be turned off for a server
static void SvNoMVPChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.IsValid() && var.GetBool() )
	{
		// Clear the MVPs of all players when MVP is turned off.
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

			if ( pPlayer )
			{
				pPlayer->SetNumMVPs( 0 );
			}
		}
	}
}
ConVar sv_nomvp( "sv_nomvp", "0", 0, "Disable MVP awards.", SvNoMVPChangeCallback );
ConVar sv_disablefreezecam( "sv_disablefreezecam", "0", FCVAR_REPLICATED, "Turn on/off freezecam on server" );
ConVar sv_nowinpanel( "sv_nowinpanel", "0", 0, "Turn on/off win panel on server" );

ConVar sv_show_voip_indicator_for_enemies( "sv_show_voip_indicator_for_enemies", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Makes it so the voip icon is shown over enemies as well as allies when they are talking" );

ConVar bot_mimic( "bot_mimic", "0", FCVAR_CHEAT );
ConVar bot_freeze( "bot_freeze", "0", FCVAR_CHEAT );
ConVar bot_crouch( "bot_crouch", "0", FCVAR_CHEAT );
ConVar bot_mimic_yaw_offset( "bot_mimic_yaw_offset", "180", FCVAR_CHEAT );

ConVar bot_chatter_use_rr( "bot_chatter_use_rr", "1", FCVAR_DEVELOPMENTONLY, "0 = Use old bot chatter system, 1 = Use response rules" );
// [jpaquin] allow buy zones to refill carried ammo automatically
#define SECONDS_BETWEEN_AUTOAMMOBUY_CHECKS .5f
ConVar sv_autobuyammo( "sv_autobuyammo", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Enable automatic ammo purchase when inside buy zones during buy periods" );

ConVar gg_knife_kill_demotes( "gg_knife_kill_demotes", "1", FCVAR_REPLICATED, "0 = knife kill in gungame has no effect on player level, 1 = knife kill demotes player by one level" );

// [mlowrance] allow adjustment of velocitymodifier based on dmg_burn (molotov )
// ConVar sv_velocitymod_dmgtype_burn( "sv_velocitymod_dmgtype_burn", "0.7", FCVAR_REPLICATED, "Sets the players speed modifier when hit by Damagetype DMG_BURN" );

// [jhail] adjusting the UI tint based on gameplay/team mode
static const int g_CT_Tint			= 1;
static const int g_T_Tint			= 2;
static const int g_KnifeLevelTint	= 4;

extern ConVar mp_autokick;
extern ConVar sv_turbophysics;
extern ConVar spec_freeze_time;
extern ConVar spec_freeze_time_lock;
extern ConVar spec_freeze_traveltime;
extern ConVar spec_freeze_deathanim_time;
extern ConVar spec_allow_roaming;

ConVar cs_hostage_near_rescue_music_distance( "cs_hostage_near_rescue_music_distance", "2000", FCVAR_CHEAT );

// players don't need physics boxes in csgo, but maybe in mods?
ConVar cs_enable_player_physics_box( "cs_enable_player_physics_box", "0", FCVAR_RELEASE );

ConVar mp_deathcam_skippable( "mp_deathcam_skippable", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines whether a player can early-out of the deathcam." );

#define THROWGRENADE_COUNTER_BITS 3


EHANDLE g_pLastCTSpawn;
EHANDLE g_pLastTerroristSpawn;

void TE_RadioIcon( IRecipientFilter& filter, float delay, CBaseEntity *pPlayer );


// -------------------------------------------------------------------------------- //
// Classes
// -------------------------------------------------------------------------------- //

class CPhysicsPlayerCallback : public IPhysicsPlayerControllerEvent
{
public:
	int ShouldMoveTo( IPhysicsObject *pObject, const Vector &position )
	{
		CCSPlayer *pPlayer = (CCSPlayer * )pObject->GetGameData();
		if ( pPlayer )
		{
			if ( pPlayer->TouchedPhysics() )
			{
				return 0;
			}
		}
		return 1;
	}
};

static CPhysicsPlayerCallback playerCallback;


// -------------------------------------------------------------------------------- //
// Ragdoll entities.
// -------------------------------------------------------------------------------- //

class CCSRagdoll : public CBaseAnimatingOverlay
{
public:
	DECLARE_CLASS( CCSRagdoll, CBaseAnimatingOverlay );
	DECLARE_SERVERCLASS();

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	void Init( void )
	{
		CBasePlayer *pPlayer = assert_cast< CBasePlayer* >( m_hPlayer.Get() );

		SetSolid( SOLID_BBOX );
		SetMoveType( MOVETYPE_STEP );
		SetFriction( 1.0f );
		SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
		m_takedamage = DAMAGE_NO;
 		SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		SetAbsAngles( QAngle( 0, m_flAbsYaw, 0 ) );
		SetAbsOrigin( pPlayer->GetAbsOrigin() );
		SetAbsVelocity( pPlayer->GetAbsVelocity() );
		AddSolidFlags( FSOLID_NOT_SOLID );
		ChangeTeam( pPlayer->GetTeamNumber() );
		UseClientSideAnimation();
	}

public:
	// In case the client has the player entity, we transmit the player index.
	// In case the client doesn't have it, we transmit the player's model index, origin, and angles
	// so they can create a ragdoll in the right place.
	CNetworkHandle( CBaseEntity, m_hPlayer );	// networked entity handle 
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
	CNetworkVar(int, m_iDeathPose );
	CNetworkVar(int, m_iDeathFrame );
	CNetworkVar(float, m_flDeathYaw );
	CNetworkVar(float, m_flAbsYaw );
};

LINK_ENTITY_TO_CLASS( cs_ragdoll, CCSRagdoll );

IMPLEMENT_SERVERCLASS_ST_NOBASE( CCSRagdoll, DT_CSRagdoll )
	SendPropVector	(SENDINFO(m_vecOrigin ), -1,  SPROP_COORD|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),
	SendPropVector( SENDINFO(m_vecRagdollOrigin ), -1,  SPROP_COORD ),
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropModelIndex( SENDINFO( m_nModelIndex ) ),
	SendPropInt		( SENDINFO(m_nForceBone ), 8, 0 ),
	SendPropVector	( SENDINFO( m_vecForce ) ),
	SendPropVector( SENDINFO( m_vecRagdollVelocity ) ),
	SendPropInt( SENDINFO( m_iDeathPose ), ANIMATION_SEQUENCE_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDeathFrame ), 5 ),
	SendPropInt( SENDINFO(m_iTeamNum ), TEAMNUM_NUM_BITS, 0 ),
	SendPropInt( SENDINFO( m_bClientSideAnimation ), 1, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flDeathYaw ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flAbsYaw ), 0, SPROP_NOSCALE )
END_SEND_TABLE()


// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class CTEPlayerAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEPlayerAnimEvent, CBaseTempEntity );
	DECLARE_SERVERCLASS();

					CTEPlayerAnimEvent( const char *name ) : CBaseTempEntity( name )
					{
					}

	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropInt( SENDINFO( m_iEvent ), Q_log2( PLAYERANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nData ), 32 )
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent( "PlayerAnimEvent" );

void TE_PlayerAnimEvent( CBasePlayer *pPlayer, PlayerAnimEvent_t event, int nData )
{
	CPVSFilter filter( (const Vector& )pPlayer->EyePosition() );
	
	g_TEPlayerAnimEvent.m_hPlayer = pPlayer;
	g_TEPlayerAnimEvent.m_iEvent = event;
	g_TEPlayerAnimEvent.m_nData = nData;
	g_TEPlayerAnimEvent.Create( filter, 0 );
}

// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( player, CCSPlayer );
PRECACHE_REGISTER(player );



BEGIN_SEND_TABLE_NOBASE( CCSPlayer, DT_CSLocalPlayerExclusive )
	SendPropVectorXY(SENDINFO(m_vecOrigin),               -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY, SENDPROP_LOCALPLAYER_ORIGINXY_PRIORITY ),
	SendPropFloat   (SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ, SENDPROP_LOCALPLAYER_ORIGINZ_PRIORITY ),

	SendPropFloat( SENDINFO( m_flStamina ), 14, 0, 0, 100.0f  ),
	SendPropInt( SENDINFO( m_iDirection ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iShotsFired ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nNumFastDucks ), 8, SPROP_UNSIGNED ), // unused

	SendPropBool( SENDINFO( m_bDuckOverride ) ),
	
	SendPropFloat( SENDINFO( m_flVelocityModifier ), 8, 0, 0, 1 ),
	
	// [tj]Set up the send table for per-client domination data
	SendPropArray3( SENDINFO_ARRAY3( m_bPlayerDominated ), SendPropBool( SENDINFO_ARRAY( m_bPlayerDominated ) ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_bPlayerDominatingMe ), SendPropBool( SENDINFO_ARRAY( m_bPlayerDominatingMe ) ) ),

	SendPropArray3( SENDINFO_ARRAY3( m_iWeaponPurchasesThisRound ), SendPropInt( SENDINFO_ARRAY( m_iWeaponPurchasesThisRound ), 4, SPROP_UNSIGNED ) ),

	SendPropInt( SENDINFO( m_nQuestProgressReason ), QuestProgress::QuestReasonBits, SPROP_UNSIGNED ),
END_SEND_TABLE()


BEGIN_SEND_TABLE_NOBASE( CCSPlayer, DT_CSNonLocalPlayerExclusive )
	SendPropVectorXY(SENDINFO(m_vecOrigin),               -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY, SENDPROP_NONLOCALPLAYER_ORIGINXY_PRIORITY ),
	SendPropFloat   (SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_NOSCALE|SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ, SENDPROP_NONLOCALPLAYER_ORIGINZ_PRIORITY ),
END_SEND_TABLE()


IMPLEMENT_SERVERCLASS_ST( CCSPlayer, DT_CSPlayer )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nMuzzleFlashParity" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	//SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),
	SendPropExclude( "DT_BaseEntity", "m_cellbits" ),
	SendPropExclude( "DT_BaseEntity", "m_cellX" ),
	SendPropExclude( "DT_BaseEntity", "m_cellY" ),
	SendPropExclude( "DT_BaseEntity", "m_cellZ" ),
	
	// cs_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),
	
	// Data that only gets sent to the local player.
	SendPropDataTable( "cslocaldata", 0, &REFERENCE_SEND_TABLE(DT_CSLocalPlayerExclusive ), SendProxy_SendLocalDataTable ),
	SendPropDataTable( "csnonlocaldata", 0, &REFERENCE_SEND_TABLE(DT_CSNonLocalPlayerExclusive ), SendProxy_SendNonLocalDataTable ),
	

	SendPropAngle( SENDINFO_VECTORELEM( m_angEyeAngles, 0 ), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN ),
	SendPropAngle( SENDINFO_VECTORELEM( m_angEyeAngles, 1 ), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN ),
		
	SendPropInt( SENDINFO( m_iAddonBits ), NUM_ADDON_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iPrimaryAddon ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iSecondaryAddon ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iThrowGrenadeCounter ), THROWGRENADE_COUNTER_BITS, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bWaitForNoAttack ) ),
	SendPropBool( SENDINFO( m_bIsRespawningForDMBonus ) ),
	SendPropInt( SENDINFO( m_iPlayerState ), Q_log2( NUM_PLAYER_STATES )+1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iAccount ), 16, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iStartAccount ), 16, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_totalHitsOnServer ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bInBombZone ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bInBuyZone ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bInNoDefuseArea ), 1, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bKilledByTaser ) ),
	SendPropInt( SENDINFO( m_iMoveState ), 0, SPROP_CHANGES_OFTEN ),
	SendPropInt( SENDINFO( m_iClass ), Q_log2( CS_MAX_PLAYER_MODELS )+1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_ArmorValue ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bHasDefuser ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bNightVisionOn ), 1, SPROP_UNSIGNED ),	//send as int so we can use a RecvProxy on the client
	SendPropBool( SENDINFO( m_bHasNightVision ) ),
	SendPropBool( SENDINFO( m_bInHostageRescueZone ) ),
	SendPropBool( SENDINFO( m_bIsDefusing ) ),
	SendPropBool( SENDINFO( m_bIsGrabbingHostage ) ),
	SendPropBool( SENDINFO( m_bIsScoped ) ),
	SendPropBool( SENDINFO( m_bIsWalking ) ),
	SendPropBool( SENDINFO( m_bResumeZoom ) ),
	SendPropFloat( SENDINFO( m_fImmuneToGunGameDamageTime ) ),
	SendPropBool( SENDINFO( m_bGunGameImmunity ) ),
	SendPropBool( SENDINFO( m_bHasMovedSinceSpawn ) ),
	SendPropBool( SENDINFO( m_bMadeFinalGunGameProgressiveKill ) ),
	SendPropInt( SENDINFO( m_iGunGameProgressiveWeaponIndex ), 32, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
	SendPropInt( SENDINFO( m_iNumGunGameTRKillPoints ) ),
	SendPropInt( SENDINFO( m_iNumGunGameKillsWithCurrentWeapon ) ),
	SendPropInt( SENDINFO( m_iNumRoundKills ) ),
	SendPropFloat( SENDINFO( m_fMolotovUseTime ) ),
	SendPropFloat( SENDINFO( m_fMolotovDamageTime ) ),
	SendPropString( SENDINFO( m_szArmsModel ) ),
	SendPropEHandle( SENDINFO( m_hCarriedHostage ) ),
	SendPropEHandle( SENDINFO( m_hCarriedHostageProp ) ),
	SendPropBool( SENDINFO( m_bIsRescuing ) ),
	SendPropFloat( SENDINFO( m_flGroundAccelLinearFracLastTime ), 0, SPROP_CHANGES_OFTEN ),
	SendPropFloat( SENDINFO( m_flGuardianTooFarDistFrac ) ),
	SendPropFloat( SENDINFO( m_flDetectedByEnemySensorTime ) ),

	SendPropBool( SENDINFO( m_bCanMoveDuringFreezePeriod ) ),
	SendPropBool( SENDINFO( m_isCurrentGunGameLeader ) ),
	SendPropBool( SENDINFO( m_isCurrentGunGameTeamLeader ) ),

	SendPropArray3( SENDINFO_ARRAY3(m_rank), SendPropInt( SENDINFO_ARRAY(m_rank), 0, SPROP_UNSIGNED ) ),

	SendPropInt( SENDINFO( m_unMusicID ), 16, SPROP_UNSIGNED ),

#ifdef CS_SHIELD_ENABLED
	SendPropBool( SENDINFO( m_bHasShield ) ),
	SendPropBool( SENDINFO( m_bShieldDrawn ) ),
#endif

	SendPropBool( SENDINFO( m_bHasHelmet ) ), 
	SendPropBool( SENDINFO( m_bHasHeavyArmor ) ),
	SendPropFloat	(SENDINFO(m_flFlashDuration ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flFlashMaxAlpha ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_iProgressBarDuration ), 4, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flProgressBarStartTime ), 0, SPROP_NOSCALE ),
	SendPropEHandle( SENDINFO( m_hRagdoll ) ),
	SendPropInt( SENDINFO( m_cycleLatch ), 4, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),

	SendPropInt( SENDINFO( m_unCurrentEquipmentValue ), 16, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_unRoundStartEquipmentValue ), 16, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_unFreezetimeEndEquipmentValue ), 16, SPROP_UNSIGNED ),

#if CS_CONTROLLABLE_BOTS_ENABLED
	SendPropBool( SENDINFO( m_bIsControllingBot ) ),
	SendPropBool( SENDINFO( m_bHasControlledBotThisRound ) ),
	SendPropBool( SENDINFO( m_bCanControlObservedBot ) ),
	SendPropInt( SENDINFO( m_iControlledBotEntIndex ) ),
#endif
	

	// data used to show and hide hud via scripts in the training map
	SendPropBool( SENDINFO( m_bHud_MiniScoreHidden ) ),
	SendPropBool( SENDINFO( m_bHud_RadarHidden ) ),

	SendPropInt( SENDINFO( m_nLastKillerIndex ), 8, SPROP_UNSIGNED ),
	// when a player dies, we send to the client the number of unbroken  times in a row the player has been killed by their last killer
	SendPropInt( SENDINFO( m_nLastConcurrentKilled ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nDeathCamMusic ), 8, SPROP_UNSIGNED ),

	SendPropBool( SENDINFO( m_bIsLookingAtWeapon ) ),
	SendPropBool( SENDINFO( m_bIsHoldingLookAtWeapon ) ),
	SendPropInt( SENDINFO( m_iNumRoundKillsHeadshots ) ),

	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_Kills), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_Kills), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_Damage), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_Damage), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_EquipmentValue), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_EquipmentValue), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_MoneySaved), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_MoneySaved), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_KillReward), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_KillReward), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_LiveTime), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_LiveTime), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_Deaths), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_Deaths), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_Assists), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_Assists), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_HeadShotKills), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_HeadShotKills), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_Objective), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_Objective), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iMatchStats_CashEarned), SendPropInt( SENDINFO_ARRAY(m_iMatchStats_CashEarned), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iMatchStats_UtilityDamage ), SendPropInt( SENDINFO_ARRAY( m_iMatchStats_UtilityDamage ), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_iMatchStats_EnemiesFlashed ), SendPropInt( SENDINFO_ARRAY( m_iMatchStats_EnemiesFlashed ), 0, SPROP_UNSIGNED, 0, SENDPROP_MATCHSTATS_PRIORITY ) ),



#if defined( PLAYER_TAUNT_SHIPPING_FEATURE )
	SendPropBool( SENDINFO( m_bIsTaunting ) ),
	SendPropBool( SENDINFO( m_bIsThirdPersonTaunt ) ),
	SendPropBool( SENDINFO( m_bIsHoldingTaunt ) ),
	SendPropFloat( SENDINFO( m_flTauntYaw ), 0, SPROP_NOSCALE ),
#endif

#if defined( USE_PLAYER_ATTRIBUTE_MANAGER )
	SendPropDataTable( SENDINFO_DT( m_AttributeManager ), &REFERENCE_SEND_TABLE(DT_AttributeManager) ),
#endif

	SendPropFloat( SENDINFO( m_flLowerBodyYawTarget ), 8, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bStrafing ) ),

	SendPropFloat( SENDINFO( m_flThirdpersonRecoil ), 8, SPROP_NOSCALE ),

END_SEND_TABLE()


BEGIN_DATADESC( CCSPlayer )

	DEFINE_INPUTFUNC( FIELD_VOID, "OnRescueZoneTouch", RescueZoneTouch ),
	DEFINE_THINKFUNC( PushawayThink )

END_DATADESC()


// has to be included after above macros
#include "cs_bot.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



// -------------------------------------------------------------------------------- //

void cc_CreatePredictionError_f( const CCommand &args )
{
	float distance = 32;

	if ( args.ArgC() >= 2 )
	{
		distance = atof(args[1] );
	}

	CBaseEntity *pEnt = CBaseEntity::Instance( 1 );
	pEnt->SetAbsOrigin( pEnt->GetAbsOrigin() + Vector( distance, 0, 0 ) );
}

ConCommand cc_CreatePredictionError( "CreatePredictionError", cc_CreatePredictionError_f, "Create a prediction error", FCVAR_CHEAT );

// -------------------------------------------------------------------------------- //
// CCSPlayer implementation.
// -------------------------------------------------------------------------------- //
CCSPlayer::CCSPlayer()
{
	m_PlayerAnimState = CreatePlayerAnimState( this, this, LEGANIM_9WAY, true );
	m_PlayerAnimStateCSGO = CreateCSGOPlayerAnimstate( this );

	UseClientSideAnimation();
	m_numRoundsSurvived = m_maxNumRoundsSurvived = 0;

	m_bCanMoveDuringFreezePeriod = false;

	m_isCurrentGunGameLeader = false;
	m_isCurrentGunGameTeamLeader = false;

	m_iLastWeaponFireUsercmd = 0;
	m_iAddonBits = 0;
	m_bEscaped = false;
	m_iAccount = 0;
	m_iAccountMoneyEarnedForNextRound = 0;
	m_iStartAccount = 0;
	m_iTotalCashSpent = 0;
	m_iCashSpentThisRound = 0;

	m_nEndMatchNextMapVote = -1;

	m_bIsVIP = false;
	m_iClass = (int )CS_CLASS_NONE;
	m_angEyeAngles.Init();

	m_flThirdpersonRecoil = 0;

	SetViewOffset( VEC_VIEW );

	m_pCurStateInfo = NULL;	// no state yet
	m_iThrowGrenadeCounter = 0;
	m_bWaitForNoAttack = true;

	m_bIsSpawning = false;
	
	m_lifeState = LIFE_DEAD; // Start "dead".
	m_bWasInBombZoneTrigger = false;
	m_bInBombZoneTrigger = false;
	m_bInBombZone = false;
	m_bWasInBuyZone = false;
	m_bInBuyZone = false;
	m_bInNoDefuseArea = false;
	m_bWasInHostageRescueZone = false;
	m_bInHostageRescueZone = false;
	m_flDeathTime = 0.0f;
	m_fForceTeam = -1.0f;
	m_iHostagesKilled = 0;
	iRadioMenu = -1;
	m_bTeamChanged = false;
	m_iShotsFired = 0;
	m_bulletsFiredSinceLastSpawn = 0;
	m_iDirection = 0;
	m_receivesMoneyNextRound = true;
	m_bIsBeingGivenItem = false;
	m_isVIP = false;
	m_bIsRespawningForDMBonus = false;
	m_bHasUsedDMBonusRespawn = false;

	m_nQuestProgressReason = QuestProgress::QUEST_NONOFFICIAL_SERVER;

	m_unCurrentEquipmentValue = 0;
	m_unRoundStartEquipmentValue = 0;
	m_unFreezetimeEndEquipmentValue = 0;

	m_nLastKillerIndex = 0;
	m_nLastConcurrentKilled = 0;
	m_nDeathCamMusic = 0;
	m_bJustBecameSpectator = false;

	m_bJustKilledTeammate = false;
	m_bPunishedForTK = false;
	m_iTeamKills = 0;
	m_flLastAction = gpGlobals->curtime;
	m_iNextTimeCheck = 0;

	m_szNewName[0] = 0; 
	m_szClanTag[0] = 0;
	m_szClanName[0] = 0;
	
	m_iTeammatePreferredColor = -1;

	for ( int i=0; i<NAME_CHANGE_HISTORY_SIZE; i++ )
	{
		m_flNameChangeHistory[i] = -NAME_CHANGE_HISTORY_INTERVAL;
	}

	m_iIgnoreGlobalChat = 0;
	m_bIgnoreRadio = false;

	m_pHintMessageQueue = new CHintMessageQueue(this );
	m_iDisplayHistoryBits = 0;
	m_bShowHints = true;
	m_flNextMouseoverUpdate = gpGlobals->curtime;

	m_lastDamageHealth = 0;
	m_lastDamageArmor = 0;

	m_nTeamDamageGivenForMatch = 0;
	m_bTDGaveProtectionWarning = false;
	m_bTDGaveProtectionWarningThisRound = false;

	m_flLastTHWarningTime = 0.0f;

	m_applyDeafnessTime = 0.0f;

	m_cycleLatch = 0;
	m_cycleLatchTimer.Invalidate();

	m_iShouldHaveCash = 0;

	m_lastNavArea = NULL;

	// [menglish] Init achievement variables
	// [menglish] Init bullet collision variables
	m_NumEnemiesKilledThisRound = 0;
	m_NumEnemiesKilledThisSpawn = 0;
	m_maxNumEnemiesKillStreak = 0;
	m_NumEnemiesAtRoundStart = 0;

	m_NumChickensKilledThisSpawn = 0;

	m_bLastKillUsedUniqueWeapon = false;
	m_bLastKillUsedUniqueWeaponMatch = false;

	m_KillingSpreeStartTime = -1;
	m_firstKillBlindStartTime = -1;
	m_killsWhileBlind = 0;
	m_bombCarrierkills = 0;
	m_knifeKillBombPlacer = false;
	m_bSurvivedHeadshotDueToHelmet = false;
	m_pGooseChaseDistractingPlayer = NULL;
	m_gooseChaseStep = GC_NONE;
	m_defuseDefenseStep = DD_NONE;
	m_lastRoundResult = Invalid_Round_End_Reason;
	m_bMadeFootstepNoise = false;
	m_bombPickupTime = -1.0f;
	m_knifeKillsWhenOutOfAmmo = 0;
	m_attemptedBombPlace = false;
	m_bombPlacedTime = -1.0f;
	m_bombDroppedTime = -1.0f;
	m_killedTime = -1.0f;
	m_spawnedTime = -1.0f;
	m_longestLife = -1.0f;
	m_triggerPulled = false;
	m_triggerPulls = 0;
	m_bMadePurchseThisRound = false;
	m_roundsWonWithoutPurchase = 0;
	m_iDeathFlags = 0;
	m_lastWeaponBeforeC4AutoSwitch = NULL;
	m_lastFlashBangAttacker = NULL;
	m_iMVPs = 0;
	m_iEnemyKills = 0;
	m_iEnemyKillHeadshots = 0;
	m_iEnemy3Ks = 0;
	m_iEnemy4Ks = 0;
	m_iEnemy5Ks = 0;
	m_iEnemyKillsAgg = 0;
	m_numFirstKills = 0;
	m_numClutchKills = 0;
	m_numPistolKills = 0;
	m_numSniperKills = 0;
// 	m_iQuestEnemyKills = 0;
// 	m_iQuestEnemyKillHeadshots = 0;
	m_iRoundsWon = 0;
// 	m_iQuestDMBonusPoints = 0;
// 	m_iQuestChickenKills = 0;
	m_uiAccountId = 0;
	m_bKilledDefuser = false;
	m_bKilledRescuer = false;
	m_maxGrenadeKills = 0;
	m_grenadeDamageTakenThisRound = 0;
	m_firstShotKills = 0;
	m_hasReloaded = false;
	m_flNextAutoBuyAmmoTime = 0;
	m_flGotHostageTalkTimer = 0;
	m_flDefusingTalkTimer = 0;
	m_flC4PlantTalkTimer = 0;
	m_flFlinchStack = 1.0;

	m_iTotalCashSpent = 0;
	m_iCashSpentThisRound = 0;

	// setting this to the current time prevents late-joining players from getting prioritized for receiving the defuser/bomb
	m_fLastGivenDefuserTime = gpGlobals->curtime;
	m_fLastGivenBombTime = gpGlobals->curtime;

	m_wasNotKilledNaturally = false;

	m_iGunGameProgressiveWeaponIndex = 0;
	m_bRespawning = false;
	m_bMadeFinalGunGameProgressiveKill = false;
	m_LastDamageType = 0;
	m_fImmuneToGunGameDamageTime = 0.0f;
	m_fJustLeftImmunityTime = 0.0f;
	m_lowHealthGoalTime = 0.0f;
	m_bGunGameImmunity = false;
	m_bHasMovedSinceSpawn = false;
	m_iNumGunGameKillsWithCurrentWeapon = 0;
	m_iNumGunGameTRKillPoints = 0;
	m_iNumGunGameTRBombTotalPoints = 0;
	m_iNumRoundKills = 0;
	m_iNumRoundKillsHeadshots = 0;
	m_iNumRoundTKs = 0;
	m_bShouldProgressGunGameTRBombModeWeapon = false;
	m_switchTeamsOnNextRoundReset = false;	
	m_bGunGameTRModeHasHEGrenade = false;
	m_bGunGameTRModeHasFlashbang = false;
	m_bGunGameTRModeHasMolotov = false;
	m_bGunGameTRModeHasIncendiary = false;
	m_fMolotovUseTime = 0.0f;
	m_fMolotovDamageTime = 0.0f;
	m_flGuardianTooFarDistFrac = 0.0f;
	m_flNextGuardianTooFarHurtTime = 0.0f;
	m_flDetectedByEnemySensorTime = 0.0f;
#if CS_CONTROLLABLE_BOTS_ENABLED
	m_bIsControllingBot = false;
	m_bCanControlObservedBot = false;
	m_iControlledBotEntIndex = -1;
#endif
	m_botsControlled = 0;
	m_iFootsteps = 0;
	m_iMediumHealthKills = 0;

	m_bHud_MiniScoreHidden = false;
	m_bHud_RadarHidden = false;

	m_iMoveState = MOVESTATE_IDLE;

	m_iLastTeam = TEAM_UNASSIGNED;
	m_bHasSeenJoinGame = false;
	m_bInvalidSteamLogonDelayed = false;

	m_iPlayerState = -1;
	m_storedSpawnPosition = vec3_origin;
	m_storedSpawnAngle.Init( );

	m_flDominateEffectDelayTime = -1;
	m_hDominateEffectPlayer = NULL;

	m_nPreferredGrenadeDrop = 0;

	V_memset( (void*)m_rank.Base(), 0, sizeof( m_rank ) );
	m_bNeedToUpdateCoinFromInventory = true;

	SetMusicID( 0 );
	m_bNeedToUpdateMusicFromInventory = true;

	V_memset( m_unEquippedPlayerSprayIDs, 0, sizeof( m_unEquippedPlayerSprayIDs ) );
	m_bNeedToUpdatePlayerSprayFromInventory = true;

	m_pPersonaDataPublic = NULL;
	m_bNeedToUpdatePersonaDataPublicFromInventory = true;

	m_duckUntilOnGround = false;

	ClearScore();
	ClearContributionScore();

	SetSpotRules( CCSEntitySpotting::SPOT_RULE_ENEMY | CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_FRIEND );
}

void CCSPlayer::ClearScore( void )
{
	// give us some points to start a round, to keep us from going negative
	ConVarRef matchStartScore( "score_default" );
	m_iScore = matchStartScore.GetInt(); 

	ClearRoundContributionScore(); 
	ClearRoundProximityScore(); 
} 


CCSPlayer::~CCSPlayer()
{
	delete m_pHintMessageQueue;
	m_pHintMessageQueue = NULL;

	// delete the records of damage taken and given
	ResetDamageCounters();
	
	if ( m_PlayerAnimState )
		m_PlayerAnimState->Release();
	
	if ( m_PlayerAnimStateCSGO )
		m_PlayerAnimStateCSGO->Release();

	delete m_pPersonaDataPublic;
	m_pPersonaDataPublic = NULL;
}


CCSPlayer *CCSPlayer::CreatePlayer( const char *className, edict_t *ed )
{
	CCSPlayer::s_PlayerEdict = ed;
	return (CCSPlayer* )CreateEntityByName( className );
}


void CCSPlayer::Precache()
{
	Vector mins( -13, -13, -10 );
	Vector maxs( 13, 13, 75 );
	bool bPreload = true;

	PlayerModelInfo::GetPtr()->InitializeForCurrentMap();

	// The following code is allowed to load non-existant .phy files due to the nested nature of the loads.
	// Tell the cache system not to worry if file are not found.

	const PlayerViewmodelArmConfig *pViewmodelArmConfig = NULL;

	// Only precache the models that are referenced by the map.
	for ( int classID=PlayerModelInfo::GetPtr()->GetFirstClass(); classID<=PlayerModelInfo::GetPtr()->GetLastClass(); ++classID )
	{

		g_pMDLCache->DisableFileNotFoundWarnings();

		engine->PrecacheModel( PlayerModelInfo::GetPtr()->GetClassModelPath(classID ), bPreload );
		engine->ForceModelBounds( PlayerModelInfo::GetPtr()->GetClassModelPath(classID ), mins, maxs );

		g_pMDLCache->EnableFileNotFoundWarnings();

		// non-econ viewmodel arms and gloves get precached here
		pViewmodelArmConfig = GetPlayerViewmodelArmConfigForPlayerModel( PlayerModelInfo::GetPtr()->GetClassModelPath(classID ) );

		if ( pViewmodelArmConfig )
		{
			g_pMDLCache->DisableVCollideLoad();
			
			if ( pViewmodelArmConfig->szAssociatedGloveModel && pViewmodelArmConfig->szAssociatedGloveModel[0] != '\0' )
				PrecacheModel( pViewmodelArmConfig->szAssociatedGloveModel );

			if ( pViewmodelArmConfig->szAssociatedSleeveModel && pViewmodelArmConfig->szAssociatedSleeveModel[0] != '\0' )
				PrecacheModel( pViewmodelArmConfig->szAssociatedSleeveModel );

			if ( pViewmodelArmConfig->szAssociatedSleeveModelEconOverride && pViewmodelArmConfig->szAssociatedSleeveModelEconOverride[0] != '\0' )
				PrecacheModel( pViewmodelArmConfig->szAssociatedSleeveModelEconOverride );

			g_pMDLCache->EnableVCollideLoad();
		}

	}

#ifdef CS_SHIELD_ENABLED
	PrecacheModel( SHIELD_VIEW_MODEL );
#endif

	PrecacheScriptSound( "Player.DeathHeadShot" );
	PrecacheScriptSound( "Player.Death" );
	PrecacheScriptSound( "Player.DamageHelmet" );
	//PrecacheScriptSound( "Player.DamageHelmetOtherFar" );
	PrecacheScriptSound( "Player.DamageHeadShot" );
	PrecacheScriptSound( "Player.DamageHeadShotOtherFar" );
	PrecacheScriptSound( "Flesh.BulletImpact" );
	PrecacheScriptSound( "Player.DamageKevlar" );
	PrecacheScriptSound( "Player.PickupWeapon" );
	PrecacheScriptSound( "Player.PickupWeaponSilent" );
	PrecacheScriptSound( "Player.Dominate" );
	PrecacheScriptSound( "Default.Land" );
	PrecacheScriptSound( "HealthShot.Pickup" );

	PrecacheScriptSound( "Music.Kill_01" );
	PrecacheScriptSound( "Music.Kill_02" );
	PrecacheScriptSound( "Music.Kill_03" );
	PrecacheScriptSound( "Music.GG_DeathCam_01" );
	PrecacheScriptSound( "Music.GG_DeathCam_02" );
	PrecacheScriptSound( "Music.GG_DeathCam_03" );
	PrecacheScriptSound( "Music.Final_Round_Stinger" );
	PrecacheScriptSound( "Music.Match_Point_Stinger" );
	PrecacheScriptSound( "Music.GG_Nemesis" );
	PrecacheScriptSound( "Music.GG_Revenge" );
	PrecacheScriptSound( "Music.GG_Dominating" );
	PrecacheScriptSound( "Player.Respawn" );
	PrecacheScriptSound( "UI.DeathMatchBonusKill" );

	PrecacheScriptSound( "UI.ArmsRace.BecomeMatchLeader" );
	PrecacheScriptSound( "UI.ArmsRace.BecomeTeamLeader" );
	PrecacheScriptSound( "UI.ArmsRace.Demoted" );
	PrecacheScriptSound( "UI.ArmsRace.LevelUp" );
	PrecacheScriptSound( "UI.Guardian.TooFarWarning" );
	PrecacheScriptSound( "UI.DeathMatchBonusAlertEnd" );
	PrecacheScriptSound( "UI.DeathMatchBonusAlertStart" );
	PrecacheScriptSound( "UI.DeathNotice" );
	
	PrecacheScriptSound( "Hostage.Breath" );

	PrecacheScriptSound( "SprayCan.Shake" );
	PrecacheScriptSound( "SprayCan.Paint" );
	
	// CS Bot sounds
	PrecacheScriptSound( "Bot.StuckSound" );
	PrecacheScriptSound( "Bot.StuckStart" );
	PrecacheScriptSound( "Bot.FellOff" );

	UTIL_PrecacheOther( "item_kevlar" );
	UTIL_PrecacheOther( "item_assaultsuit" );
	UTIL_PrecacheOther( "item_heavyassaultsuit" );
	UTIL_PrecacheOther( "item_defuser" );

	PrecacheModel( "sprites/glow01.vmt" );

	PrecacheEffect( "csblood" );
	PrecacheEffect( "gunshotsplash" );

	PrecacheParticleSystem( "speech_voice" );
	PrecacheParticleSystem( "impact_helmet_headshot" );
	PrecacheParticleSystem( "ar_screenglow_leader_red" );

	PrecacheModel( "models/player/holiday/santahat.mdl" );
	PrecacheModel( "models/ghost/ghost.mdl" ); // halloween
	PrecacheModel( "models/player/holiday/facemasks/facemask_battlemask.mdl" );

	if ( CSGameRules()->IsPlayingCooperativeGametype() )
		PrecacheModel( g_pszHeavyPhoenixModel );

	PrecacheModel( "models/weapons/w_muzzlefireshape.mdl" );

	PrecacheModel( "models/tools/bullet_hit_marker.mdl" );

	// Not shipping loot quests this operation, don't precache the model
	PrecacheModel( "models/matlibrary/matlibrary_default.mdl" );
	//PrecacheModel( g_pszLootModelName );

	PrecacheModel( "models/player/contactshadow/contactshadow_leftfoot.mdl" );
	PrecacheModel( "models/player/contactshadow/contactshadow_rightfoot.mdl" );

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: Allow pre-frame adjustments on the player
//-----------------------------------------------------------------------------
ConVar sv_runcmds( "sv_runcmds", "1" );
void CCSPlayer::PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper )
{
	VPROF( "CCSPlayer::PlayerRunCommand" );

	if ( !sv_runcmds.GetInt() )
		return;

	// don't run commands in the future
	if ( !IsEngineThreaded() && 
		( ucmd->tick_count > (gpGlobals->tickcount + sv_max_usercmd_future_ticks.GetInt() ) ) )
	{
		DevMsg( "Client cmd out of sync (delta %i).\n", ucmd->tick_count - gpGlobals->tickcount );
		return;
	}

	// If they use a negative bot_mimic value, then don't process their usercmds, but have
	// bots process them instead (so they can stay still and have the bot move around ).
	CUserCmd tempCmd;
	if ( -bot_mimic.GetInt() == entindex() )
	{
		tempCmd = *ucmd;
		ucmd = &tempCmd;

		ucmd->forwardmove = ucmd->sidemove = ucmd->upmove = 0;
		ucmd->buttons = 0;
		ucmd->impulse = 0;
	}

	if ( IsBot() && bot_crouch.GetInt() )
	{
		ucmd->buttons |= IN_DUCK;
	}

	if ( ( IsTaunting() && !IsThirdPersonTaunt() ) || IsLookingAtWeapon() )
	{
		if ( ( ucmd->buttons & ( IN_ATTACK | IN_ATTACK2 | IN_RELOAD ) ) != 0 /*|| ucmd->forwardmove || ucmd->sidemove || ucmd->upmove*/ )
		{
			StopTaunting();
			StopLookingAtWeapon();

			if ( ( ucmd->buttons & IN_ATTACK2 ) != 0 && ( ucmd->buttons & ( IN_ATTACK | IN_RELOAD ) ) == 0 )
			{
				CWeaponCSBase *pWeapon = GetActiveCSWeapon();
				if ( pWeapon && pWeapon->HasZoom() )
				{
					// Force the animation back to idle since changing zoom has no specific animation
					CBaseViewModel *pViewModel = GetViewModel();
					if ( pViewModel )
					{
						int nSequence = pViewModel->LookupSequence( "idle" );

						if ( nSequence != ACTIVITY_NOT_AVAILABLE )
						{
							pViewModel->ForceCycle( 0 );
							pViewModel->ResetSequence( nSequence );
						}
					}
				}
			}
		}
	}

	if ( IsTaunting() && IsThirdPersonTaunt() )
	{
		// For some taunts, it is critical that the player not move once they start
		if ( m_bMustNotMoveDuringTaunt )
		{
			ucmd->forwardmove = 0;
			ucmd->upmove = 0;
			ucmd->sidemove = 0;
			ucmd->viewangles = pl.v_angle;
		}
	}

	BaseClass::PlayerRunCommand( ucmd, moveHelper );
}


bool CCSPlayer::RunMimicCommand( CUserCmd& cmd )
{
	if ( !IsBot() )
		return false;

	int iMimic = abs( bot_mimic.GetInt() );
	if ( iMimic > gpGlobals->maxClients )
		return false;

	CBasePlayer *pPlayer = UTIL_PlayerByIndex( iMimic );
	if ( !pPlayer )
		return false;

	if ( !pPlayer->GetLastUserCommand() )
		return false;

	cmd = *pPlayer->GetLastUserCommand();
	cmd.viewangles[YAW] += bot_mimic_yaw_offset.GetFloat();

	pl.fixangle = FIXANGLE_NONE;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Simulates a single frame of movement for a player
//-----------------------------------------------------------------------------
void CCSPlayer::RunPlayerMove( const QAngle& viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, float frametime )
{
	CUserCmd cmd;

	// Store off the globals.. they're gonna get whacked
	float flOldFrametime = gpGlobals->frametime;
	float flOldCurtime = gpGlobals->curtime;

	float flTimeBase = gpGlobals->curtime + gpGlobals->frametime - frametime;
	this->SetTimeBase( flTimeBase );

	CUserCmd lastUserCmd = *GetLastUserCommand();
	Q_memset( &cmd, 0, sizeof( cmd ) );

	if ( !RunMimicCommand( cmd ) )
	{
		cmd.forwardmove = forwardmove;
		cmd.sidemove = sidemove;
		cmd.upmove = upmove;
		cmd.buttons = buttons;
		cmd.impulse = impulse;

		VectorCopy( viewangles, cmd.viewangles );
		cmd.random_seed = random->RandomInt( 0, 0x7fffffff );
	}

	MoveHelperServer()->SetHost( this );
	PlayerRunCommand( &cmd, MoveHelperServer() );

	// save off the last good usercmd
	if ( -bot_mimic.GetInt() == entindex() )
	{
		CUserCmd lastCmd = *GetLastUserCommand();
		lastCmd.command_number = cmd.command_number;
		lastCmd.tick_count = cmd.tick_count;
		SetLastUserCommand( lastCmd );
	}
	else
	{
		SetLastUserCommand( cmd );
	}

	// Clear out any fixangle that has been set
	pl.fixangle = FIXANGLE_NONE;

	// Restore the globals..
	gpGlobals->frametime = flOldFrametime;
	gpGlobals->curtime = flOldCurtime;

	MoveHelperServer()->SetHost( NULL );
}


void CCSPlayer::InitialSpawn( void )
{
#if defined( USE_PLAYER_ATTRIBUTE_MANAGER )
	m_AttributeManager.InitializeAttributes( this );
	m_AttributeManager.SetPlayer( this );
	m_AttributeList.SetManager( &m_AttributeManager );
#endif

	BaseClass::InitialSpawn();

	// we're going to give the bots money here instead of FinishClientPutInServer()
	// because of the bots' timing for purchasing weapons/items.
	if ( IsBot() )
	{
		m_iAccount = CSGameRules()->GetStartMoney();
		m_iAccountMoneyEarnedForNextRound = 0;

		if ( CSGameRules()->ShouldRecordMatchStats() )
		{
			m_iMatchStats_CashEarned.GetForModify( CSGameRules()->GetTotalRoundsPlayed() ) = m_iAccount;
		}

		// Keep track in QMM data
		if ( m_uiAccountId && CSGameRules() )
		{
			if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
			{
				pQMM->m_cash = m_iAccount;

				if ( CSGameRules()->ShouldRecordMatchStats() )
				{
					pQMM->m_iMatchStats_CashEarned[ CSGameRules()->GetTotalRoundsPlayed() ] = m_iMatchStats_CashEarned.Get( CSGameRules()->GetTotalRoundsPlayed() );
				}
			}
		}
	}

	if ( !engine->IsDedicatedServer() && TheNavMesh->IsOutOfDate() && this == UTIL_GetListenServerHost() && !IsGameConsole() )
	{
		ClientPrint( this, HUD_PRINTCENTER, "The Navigation Mesh was built using a different version of this map." );
	}

	Assert( GetAbsVelocity().Length() == 0.0f );
	State_Enter( STATE_WELCOME );
	UpdateInventory( true );

	// [tj] We reset the stats at the beginning of the map (including domination tracking )
	CCS_GameStats.ResetPlayerStats(this );
	RemoveNemesisRelationships();

	// for late joiners, we want to give them a fighting chance in gun game so, give them the lowest level reached by a player already
	int nMinWep = 0;
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			int nCurWep = pPlayer->GetPlayerGunGameWeaponIndex();
			if ( nCurWep < nMinWep )
				nMinWep = nCurWep;
		}
	}

	if ( nMinWep > 0 )
	{
		// +1 because when they select a team, we reduce their level by 1
		m_iGunGameProgressiveWeaponIndex = nMinWep + 1;
	}
}

void CCSPlayer::SetModelFromClass( void )
{

	if ( GetTeamNumber() == TEAM_TERRORIST )
	{
		if (  !PlayerModelInfo::GetPtr()->IsTClass( m_iClass ) )
		{
			m_iClass = PlayerModelInfo::GetPtr()->GetNextClassForTeam( GetTeamNumber() );
		}
	}
	else if ( GetTeamNumber() == TEAM_CT )
	{
		if ( !PlayerModelInfo::GetPtr()->IsCTClass( m_iClass ) )
		{
			m_iClass = PlayerModelInfo::GetPtr()->GetNextClassForTeam( GetTeamNumber() );
		}
	}
	else
	{
		// Default model / class if we are not on a team.
		SetModel( PlayerModelInfo::GetPtr()->GetClassModelPath( PlayerModelInfo::GetPtr()->GetFirstTClass() ) );
		return;
	}
	
	SetModel( PlayerModelInfo::GetPtr()->GetClassModelPath( m_iClass ) );
}
void CCSPlayer::SetCSSpawnLocation( Vector position, QAngle angle )
{
	m_storedSpawnPosition = position;
	m_storedSpawnAngle = angle;
}

void CCSPlayer::Spawn()
{
	m_RateLimitLastCommandTimes.Purge();

	// Get rid of the progress bar...
	SetProgressBarTime( 0 );

	CreateViewModel();

	// Set their player model.

	bool bStartsWithHeavyArmorThisRound = false;
	// Sometimes spawn a heavy in guardian mode, don't pick model based on player class
	if ( GetTeamNumber() == TEAM_TERRORIST && CSGameRules()->IsPlayingCoopGuardian() && IsBot() &&
		(
			(
			CSGameRules()->GetCoopWaveNumber() >= sv_guardian_min_wave_for_heavy.GetInt() && 
			CSGameRules()->GetCoopWaveNumber() <= sv_guardian_max_wave_for_heavy.GetInt() &&
			CSGameRules()->m_nNumHeaviesToSpawn > 0 && !CSGameRules()->IsWarmupPeriod()
			)
			||
			sv_guardian_heavy_all.GetBool()
		)
		)
	{
		//
		// Also see cs_bot_manager.cpp that manages spawning sniper bots
		// Ensure that we have a sniper profile on the bot team for cooperative game
		//
		// Check my bot profile if I am a sniper?
		bool bThisCanBeHeavyBot = true;
		if ( CCSBot *pMyBot = dynamic_cast< CCSBot * >( this ) )
		{
			const BotProfile *pExistingBotProfile = pMyBot->GetProfile();
			if ( pExistingBotProfile && pExistingBotProfile->GetWeaponPreferenceCount() && WeaponClassFromWeaponID( pExistingBotProfile->GetWeaponPreference( 0 ) ) == WEAPONTYPE_SNIPER_RIFLE )
			{	// found a bot who will buy a sniper rifle guaranteed
				bThisCanBeHeavyBot = false;
			}
		}


		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CCSPlayer *player = static_cast< CCSPlayer* >( UTIL_PlayerByIndex( i ) );

			if ( player == NULL )
				continue;

			// skip players on other teams
			if ( player->GetTeamNumber() != GetTeamNumber() )
				continue;

			// if not a bot, fail the test
			if ( !player->IsBot() )
				continue ;

			if ( player->HasHeavyArmor() )
			{
				bThisCanBeHeavyBot = false;
				break;
			}
		}

		int minNumSniperBots = 1;
		extern ConVar mp_guardian_special_weapon_needed;
		char const *szWepShortName = mp_guardian_special_weapon_needed.GetString();
		if ( szWepShortName && *szWepShortName && V_strcmp( szWepShortName, "any" ) )
		{
			CSWeaponID csWeaponIdGuardianRequired = AliasToWeaponID( szWepShortName );
			if ( ( csWeaponIdGuardianRequired != WEAPON_NONE ) &&
				( WeaponClassFromWeaponID( csWeaponIdGuardianRequired ) == WEAPONTYPE_SNIPER_RIFLE ) )
			{
				++minNumSniperBots;
			}
		}
		//
		// End of code that is matching cs_bot_manager.cpp for enumerating guardian sniper bots
		//

		if ( sv_guardian_heavy_all.GetBool() )
		{
			bStartsWithHeavyArmorThisRound = true;
		}
		else if ( bThisCanBeHeavyBot )
		{
			// Each bot has an equal chance of being heavy distributed across all rounds where heavies are possible, except last round where we slam odds to 100% if any unspawned heavies remain.	
			int iNumWavesTillMax = sv_guardian_max_wave_for_heavy.GetInt() - CSGameRules()->GetCoopWaveNumber();
			float flHeavyChance = 1.0f;
			if ( iNumWavesTillMax >= CSGameRules()->m_nNumHeaviesToSpawn )
			{
				flHeavyChance = 1 / ( float ) ( iNumWavesTillMax - CSGameRules()->m_nNumHeaviesToSpawn + 2 );
			}
			if ( RandomFloat() < flHeavyChance )
			{
				CSGameRules()->m_nNumHeaviesToSpawn--;
				bStartsWithHeavyArmorThisRound = true;
			}
		}
	}
	
	if ( bStartsWithHeavyArmorThisRound )
	{
		SetModel( g_pszHeavyPhoenixModel );
		GiveNamedItem( "item_heavyassaultsuit" );
		m_iClass = 1; // Set some valid class, doesn't matter which.
	}
	else
	{
		SetModelFromClass();
	}

	if ( GetTeamNumber() == TEAM_TERRORIST && CSGameRules()->IsPlayingCoopGuardian() && IsBot() )
	{
		CCSBot *pMyBot = dynamic_cast< CCSBot * >( this );
		const BotProfile *pExistingBotProfile = pMyBot ? pMyBot->GetProfile() : NULL;
		if ( pExistingBotProfile )
		{
			// Mimic cs_bot_init.cpp that is naming bots for coop missions
			char const *szHeavy = bStartsWithHeavyArmorThisRound ? "Heavy Phoenix " : "Attacker ";
			char szName[ 128 ] = {};
			V_sprintf_safe( szName, "%s%s", szHeavy, pExistingBotProfile->GetName() );
			// Override the player name
			SetPlayerName( szName );
			// have to inform the engine that the bot name has been updated
			engine->SetFakeClientConVarValue( edict(), "name", szName );
		}
	}

	m_bCanMoveDuringFreezePeriod = false;

//	m_isCurrentGunGameLeader = false;
//	m_isCurrentGunGameTeamLeader = false;

	if ( GetParent() )
		SetParent( NULL );


	BaseClass::Spawn();

	// After base class spawn strips our last-round items and grants defaults, give us our heavy armor if we've been chosen this round
	if ( bStartsWithHeavyArmorThisRound )
	{
		m_bHasHeavyArmor = true;
		GiveNamedItem( "item_heavyassaultsuit" );
	}
	UpdateInventory( false );

	// [pfreese] Clear the last known nav area (used to be done by CBasePlayer )
	m_lastNavArea = NULL;

	AddFlag(FL_ONGROUND ); // set the player on the ground at the start of the round.

	// Override what CBasePlayer set for the view offset.
	SetViewOffset( VEC_VIEW );

	//
	// Our player movement speed is set once here. This will override the cl_xxxx
	// cvars unless they are set to be lower than this.
	//
	SetMaxSpeed( CS_PLAYER_SPEED_RUN );

	SetFOV( this, 0 );

	m_bIsDefusing = false;
	m_bIsGrabbingHostage = false;

	m_bIsWalking = false;

	// [jpaquin] variable to keep entities for checking to refil ammo every tick
	m_flNextAutoBuyAmmoTime = gpGlobals->curtime + RandomFloat( 0, SECONDS_BETWEEN_AUTOAMMOBUY_CHECKS );

	// [dwenger] Reset hostage-related variables
	m_bIsRescuing = false;
	m_bInjuredAHostage = false;
	m_iNumFollowers = 0;
	
	// [tj] Reset this flag if the player is not in observer mode (as happens when a player spawns late )
	if (m_iPlayerState != STATE_OBSERVER_MODE )
	{
		m_wasNotKilledNaturally = false;
	}
	m_bulletsFiredSinceLastSpawn = 0;
	m_iShotsFired = 0;
	m_iDirection = 0;
	m_bWaitForNoAttack = true;

	if ( m_pHintMessageQueue )
	{
		m_pHintMessageQueue->Reset();
	}
	m_iDisplayHistoryBits &= ~DHM_ROUND_CLEAR;

	// Special-case here. A bunch of things happen in CBasePlayer::Spawn(), and we really want the
	// player states to control these things, so give whatever player state we're in a chance
	// to reinitialize itself.
	State_Transition( m_iPlayerState );

	ClearFlashbangScreenFade();

	m_flVelocityModifier = 1.0f;
	m_flGroundAccelLinearFracLastTime = 0.0f;

	ResetStamina();

	m_fNextRadarUpdateTime = 0.0f;
	m_flLastMoneyUpdateTime = 0.0f;
	m_fMolotovDamageTime = 0.0f;
	m_iLastWeaponFireUsercmd = 0;

	m_iNumSpawns++;

	if ( !engine->IsDedicatedServer() && CSGameRules()->m_iTotalRoundsPlayed < 2 && TheNavMesh->IsOutOfDate() && this == UTIL_GetListenServerHost() && !IsGameConsole() )
	{
		ClientPrint( this, HUD_PRINTCENTER, "The Navigation Mesh was built using a different version of this map." );
	}

	m_bTeamChanged	= false;
	m_iOldTeam = TEAM_UNASSIGNED;

	m_bHasMovedSinceSpawn = false;

	m_iRadioMessages = 60;
	m_flRadioTime = gpGlobals->curtime;

	if ( m_hRagdoll )
	{
		UTIL_Remove( m_hRagdoll );
	}

	m_hRagdoll = NULL;
	
	// did we change our name while we were dead?
	if ( m_szNewName[0] != 0 )
	{
		ChangeName( m_szNewName );
		m_szNewName[0] = 0;
	}

	if ( m_bIsVIP )
	{
		HintMessage( "#Hint_you_are_the_vip", true, true );
	}

	m_bIsInAutoBuy = false;
	m_bIsInRebuy = false;
	m_bAutoReload = false;

	// reset the number of enemies killed this round only we're not respawning to get the bonus weapon.
	if ( !m_bIsRespawningForDMBonus )	
	{
		m_NumEnemiesKilledThisSpawn = 0;
		m_NumChickensKilledThisSpawn = 0;
	}

	SetContextThink( &CCSPlayer::PushawayThink, gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, CS_PUSHAWAY_THINK_CONTEXT );

	if ( GetActiveWeapon() && !IsObserver() )
	{
		GetActiveWeapon()->Deploy();
		m_flNextAttack = gpGlobals->curtime; // Allow reloads to finish, since we're playing the deploy anim instead.  This mimics goldsrc behavior, anyway.
	}

	m_flLastTHWarningTime = 0.0f;

	m_applyDeafnessTime = 0.0f;
	m_lowHealthGoalTime = 0.0f;

	m_cycleLatch = 0;
	if ( !m_bUseNewAnimstate )
		m_cycleLatchTimer.Start( RandomFloat( 0.0f, CycleLatchInterval ) );

	StockPlayerAmmo();

	// Calculate timeout for gun game immunity
	ConVarRef mp_respawn_immunitytime( "mp_respawn_immunitytime" );
	float flImmuneTime = mp_respawn_immunitytime.GetFloat();

	if ( flImmuneTime > 0 || CSGameRules()->IsWarmupPeriod() )
	{                
		//Make sure we can't move if we respawn in gun game after the rounds ends
		CCSMatch* pMatch = CSGameRules()->GetMatch();
		if ( pMatch && pMatch->GetPhase() == GAMEPHASE_MATCH_ENDED )
		{
			AddFlag( FL_FROZEN );
		}

		if ( CSGameRules()->IsPlayingGunGameDeathmatch() && !IsBot() )
		{
			// set immune time to super high and open the buy menu
			m_bInBuyZone = true;
		}
		else if ( CSGameRules()->IsWarmupPeriod() )
		{
			flImmuneTime = 3;
		}

		m_fImmuneToGunGameDamageTime = gpGlobals->curtime + flImmuneTime;
		m_bGunGameImmunity = true;
	}
	else
	{
		m_fImmuneToGunGameDamageTime = 0.0f;
		m_bGunGameImmunity = false;
	}

	m_knifeKillsWhenOutOfAmmo = 0;
	m_botsControlled = 0;
	m_iFootsteps = 0;
	m_iMediumHealthKills = 0;
	m_killedTime = -1.0f;
	m_spawnedTime = gpGlobals->curtime;
	m_bKilledByTaser = false;
	m_bHasBeenControlledByPlayerThisRound = false;
	m_bHasControlledBotThisRound = false;

	StopTaunting();
	m_bIsHoldingTaunt = false;
	StopLookingAtWeapon();
	m_bIsHoldingLookAtWeapon = false;

	m_bDuckOverride = false;

	// save off how much money we started with; OGS will want this later
	m_iStartAccount = m_iAccount;		
	// reset the money that is being held until next round
	m_iAccountMoneyEarnedForNextRound = 0;

	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CBaseCombatWeapon *pWeapon = GetWeapon( i );
		if ( pWeapon )
		{
			CWeaponCSBase *pCSwep = dynamic_cast< CWeaponCSBase * >( pWeapon );
			if ( pCSwep )
			{
				pCSwep->WeaponReset();
			}
		}
	}

	for ( int i = 0; i < m_iWeaponPurchasesThisRound.Count(); ++i )
	{
		m_iWeaponPurchasesThisRound.Set(i, 0);
	}

	if ( IsAbleToInstantRespawn() && !CSGameRules()->IsPlayingOffline() && IsBot() )
	{
		// Modify this bot's difficulty in Arms Race mode on respawn since arms race doesn't have a round end except at match end
		CSGameRules()->ModifyRealtimeBotDifficulty( this );
	}

	if ( IsAbleToInstantRespawn() )
	{
		if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{
			int nCurWepKills = GetNumGunGameKillsWithCurrentWeapon();
			m_iNumRoundKills = MIN( nCurWepKills, m_iNumRoundKills );
			m_iNumRoundKillsHeadshots = MIN( nCurWepKills, m_iNumRoundKillsHeadshots );
		}
		else
		{
			m_iNumRoundKills = 0;
			m_iNumRoundKillsHeadshots = 0;
		}
		m_iNumRoundTKs = 0;
	}

// 	if( IsBot() )
// 	{
// 		SetDeathCamMusicIndex( RandomInt(1, 8) );
// 	}

	//
	// Transfer all parameters that we know about
	//
	CSteamID const *pThisSteamId = engine->GetClientSteamID( this->edict() );
	if ( pThisSteamId && pThisSteamId->IsValid() && pThisSteamId->GetAccountID() && !IsBot() && CSGameRules() )
	{
		SetHumanPlayerAccountID( pThisSteamId->GetAccountID() );
	}

	// If we're constantly respawning then reset damage stats on spawn. Otherwise this'll happen on roundrespawn after damage is reported.
	if ( IsAbleToInstantRespawn() )
	{
		ResetDamageCounters();
		RemoveSelfFromOthersDamageCounters();
	
		if ( cash_player_respawn_amount.GetInt() > 0 )
		{
			AddAccountAward( PlayerCashAward::RESPAWN );
		}
	}

	// clear out and carried hostage stuff
	RemoveCarriedHostage();

	ResetKillStreak();

	// play a respawn sound if you're in deathmatch 
	//	TODO: turn this into a convar and put it inside the above IsAbleToInstantRespawn check
	if ( State_Get() == STATE_ACTIVE )
	{
		bool bPlaySound = false;

		int nTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
		if ( CSGameRules()->IsPlayingCoopMission() )
		{
			if ( GetTeamNumber() == TEAM_CT )
				bPlaySound = true;
		}
		else if ( (CSGameRules()->IsPlayingGunGameDeathmatch() && GetTeamNumber() >= TEAM_TERRORIST) ||
			 (CSGameRules()->IsPlayingCooperativeGametype() && GetTeamNumber() == nTeam) )
		{
			bPlaySound = true;
		}

		if ( bPlaySound )
			EmitSound( "Player.Respawn" );
	}

	if ( m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	{
		m_PlayerAnimStateCSGO->Reset();
		m_PlayerAnimStateCSGO->Update( EyeAngles()[YAW], EyeAngles()[PITCH], true );
		DoAnimationEvent( PLAYERANIMEVENT_DEPLOY ); // re-deploy default weapon when spawning
	}

	// if a player spawns with a smoke grenade in competitive, it counts as being "bought" so they can't end up with 2 per player that round
	if ( CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
	{
		if ( GetAmmoCount( GetAmmoDef()->Index( AMMO_TYPE_SMOKEGRENADE ) ) )
			m_iWeaponPurchasesThisRound.GetForModify(WEAPON_SMOKEGRENADE)++;
	}

	engine->ClientResetReplayRequestTime( entindex() - 1 );


	
	// coop bots with custom models
	if ( CSGameRules()->IsPlayingCoopMission() && IsBot() )
	{
		if ( GetTeamNumber() == TEAM_TERRORIST )
		{
			CCSBot* pBot = static_cast< CCSBot* >( this );
			if ( pBot )
			{
				SpawnPointCoopEnemy* pSpawn = pBot->GetLastCoopSpawnPoint();
				if ( pSpawn )
				{
					if ( V_stricmp( pSpawn->GetPlayerModelToUse(), "" ) != 0 )
					{
						SetModel( pSpawn->GetPlayerModelToUse() );
						//SetViewModelArms( viewModelArms );
						return;
					}
				}
			}
		}
	}

	if ( m_bInvalidSteamLogonDelayed )
	{
		m_bInvalidSteamLogonDelayed = false;
		Warning( "Invalid Steam Logon Delayed: Kicking client [U:1:%d] %s\n", GetHumanPlayerAccountID(), GetPlayerName() );
		engine->ServerCommand( UTIL_VarArgs( "kickid_ex %d %d No Steam logon\n", this->GetUserID(), /*bIsPlayingOffline*/ false ? 0 : 1 ) );
	}
}

void CCSPlayer::SetHumanPlayerAccountID( uint32 uiAccountId )
{
	m_uiAccountId = uiAccountId;
	if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
	{
		CCSGameRules::CQMMPlayerData_t &qmmPlayerData = *pQMM;
		this->m_iFrags = qmmPlayerData.m_numKills;
		this->m_iAssists = qmmPlayerData.m_numAssists;
		this->m_iDeaths = qmmPlayerData.m_numDeaths;
		this->m_iMVPs = qmmPlayerData.m_numMVPs;
		this->m_iContributionScore = qmmPlayerData.m_numScorePoints;
		this->m_iAccount = qmmPlayerData.m_cash;
		this->m_iTeamKills = qmmPlayerData.m_numTeamKills;
		this->m_nTeamDamageGivenForMatch = qmmPlayerData.m_numTeamDamagePoints;
		this->m_iHostagesKilled = qmmPlayerData.m_numHostageKills;

		this->m_iEnemyKills = qmmPlayerData.m_numEnemyKills;
		this->m_iEnemyKillHeadshots = qmmPlayerData.m_numEnemyKillHeadshots;
		this->m_iEnemy3Ks = qmmPlayerData.m_numEnemy3Ks;
		this->m_iEnemy4Ks = qmmPlayerData.m_numEnemy4Ks;
		this->m_iEnemy5Ks = qmmPlayerData.m_numEnemy5Ks;
		this->m_iEnemyKillsAgg = qmmPlayerData.m_numEnemyKillsAgg;
		this->m_numFirstKills = qmmPlayerData.m_numFirstKills;
		this->m_numClutchKills = qmmPlayerData.m_numClutchKills;
		this->m_numPistolKills = qmmPlayerData.m_numPistolKills;
		this->m_numSniperKills = qmmPlayerData.m_numSniperKills;

		this->m_iRoundsWon = qmmPlayerData.m_numRoundsWon;

		this->m_receivesMoneyNextRound = !qmmPlayerData.m_bReceiveNoMoneyNextRound;

		this->pl.frags = this->m_iFrags;
		this->pl.assists = this->m_iAssists;
		this->pl.deaths = this->m_iDeaths;
		this->pl.score = this->m_iContributionScore;

		for ( int i = 0; i < MAX_MATCH_STATS_ROUNDS; i ++ )
		{
			this->m_iMatchStats_Kills.GetForModify( i ) = qmmPlayerData.m_iMatchStats_Kills[ i ];
			this->m_iMatchStats_Damage.GetForModify( i ) = qmmPlayerData.m_iMatchStats_Damage[ i ];
			this->m_iMatchStats_MoneySaved.GetForModify( i ) = qmmPlayerData.m_iMatchStats_MoneySaved[ i ];
			this->m_iMatchStats_EquipmentValue.GetForModify( i ) = qmmPlayerData.m_iMatchStats_EquipmentValue[ i ];
			this->m_iMatchStats_KillReward.GetForModify( i ) = qmmPlayerData.m_iMatchStats_KillReward[ i ];
			this->m_iMatchStats_LiveTime.GetForModify( i ) = qmmPlayerData.m_iMatchStats_LiveTime[ i ];
			this->m_iMatchStats_Deaths.GetForModify( i ) = qmmPlayerData.m_iMatchStats_Deaths[ i ];
			this->m_iMatchStats_Assists.GetForModify( i ) = qmmPlayerData.m_iMatchStats_Assists[ i ];
			this->m_iMatchStats_HeadShotKills.GetForModify( i ) = qmmPlayerData.m_iMatchStats_HeadShotKills[ i ];
			this->m_iMatchStats_Objective.GetForModify( i ) = qmmPlayerData.m_iMatchStats_Objective[ i ];
			this->m_iMatchStats_CashEarned.GetForModify( i ) = qmmPlayerData.m_iMatchStats_CashEarned[ i ];
			this->m_iMatchStats_UtilityDamage.GetForModify( i ) = qmmPlayerData.m_iMatchStats_UtilityDamage[ i ];
			this->m_iMatchStats_EnemiesFlashed.GetForModify( i ) = qmmPlayerData.m_iMatchStats_EnemiesFlashed[ i ];
		}

		// Store last known player name
		Q_strncpy( qmmPlayerData.m_chPlayerName, this->GetPlayerName(), Q_ARRAYSIZE( qmmPlayerData.m_chPlayerName ) );
	}

	// In tournament mode force clan tags for the players according to the reservation
	if ( CCSGameRules::sm_QueuedServerReservation.has_tournament_event() )
	{
		for ( int32 iTeam = 0; iTeam < CCSGameRules::sm_QueuedServerReservation.tournament_teams().size(); ++ iTeam )
		{
			TournamentTeam const &ttTeam = CCSGameRules::sm_QueuedServerReservation.tournament_teams( iTeam );
			for ( int32 iTeamPlayer = 0; iTeamPlayer < ttTeam.players().size(); ++ iTeamPlayer )
			{
				TournamentPlayer const &ttPlayer = ttTeam.players( iTeamPlayer );
				if ( ttPlayer.account_id() && ( ttPlayer.account_id() == m_uiAccountId ) )
				{
					// Set the clan tag and full team name
					// SetClanTag( ttTeam.team_clantag().c_str() );
					SetClanTag( "" );
					SetClanName( ttTeam.team_name().c_str() );

					// Set the player name as well
					SetPlayerName( ttPlayer.player_nick().c_str() );

					// break out of all loops
					iTeam = CCSGameRules::sm_QueuedServerReservation.tournament_teams().size();
					iTeamPlayer = ttTeam.players().size();
				}
			}
		}
	}
}


void CCSPlayer::ShowViewPortPanel( const char * name, bool bShow, KeyValues *data )
{
	if ( CSGameRules()->IsLogoMap() )
		return;

	if ( CommandLine()->FindParm("-makedevshots" ) )
		return;

	BaseClass::ShowViewPortPanel( name, bShow, data );
}

void CCSPlayer::ClearFlashbangScreenFade( void )
{
	if( IsBlind() )
	{
		color32 clr = { 0, 0, 0, 0 };
		UTIL_ScreenFade( this, clr, 0.01, 0.0, FFADE_OUT | FFADE_PURGE );

		m_flFlashDuration = 0.0f;
		m_flFlashMaxAlpha = 255.0f;
	}	

	// clear blind time (after screen fades are canceled )
	m_blindUntilTime = 0.0f;
	m_blindStartTime = 0.0f;
}

void CCSPlayer::GiveDefaultItems()
{
	if ( State_Get() != STATE_ACTIVE )
		return;

	GiveDefaultWearables();

	if ( CSGameRules()->IsPlayingCooperativeGametype() )
	{
		if ( IsBot() && GetTeamNumber() == TEAM_TERRORIST && CSGameRules()->IsPlayingCoopMission() )
		{
			RemoveAllItems( true );

			CCSBot* pBot = static_cast< CCSBot* >( this );
			if ( pBot )
			{
				SpawnPointCoopEnemy *pEnemySpawnSpot = pBot->GetLastCoopSpawnPoint();
				if ( pEnemySpawnSpot )
				{
					int nArmor = pEnemySpawnSpot->GetArmorToSpawnWith();
					if ( nArmor == 1 )
						GiveNamedItem( "item_assaultsuit" );
					else if ( nArmor == 2 )
						GiveNamedItem( "item_heavyassaultsuit" );

					const char *szWepsToGive = pEnemySpawnSpot->GetWeaponsToGive();
					CUtlVector< char* > msgStrs;
					V_SplitString( szWepsToGive, ",", msgStrs );
					//run through and give all of the items specified
					FOR_EACH_VEC( msgStrs, nCurrMsg )
					{
						char* szWeapon = msgStrs[ nCurrMsg ];
						V_StripLeadingWhitespace( szWeapon );
						V_StripTrailingWhitespace( szWeapon );
						pBot->GiveWeapon( szWeapon );
					}
				}
			}

			// make sure they have a knife
			CBaseCombatWeapon *knife = Weapon_GetSlot( WEAPON_SLOT_KNIFE );	
			if ( !knife )
			{
				GiveNamedItem( "weapon_knife_t" );
			}

			return;
		}
		else
		{
			// clear the player's items in case the round was lost 
			// due to the terrorists planting or CTs reaching the hostage
			int nOtherTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_CT : TEAM_TERRORIST;
			if ( IsAlive() && !IsBot() && GetTeamNumber() != nOtherTeam )
				RemoveAllItems( false );

			if ( IsBot() && GetTeamNumber() == nOtherTeam )
				CSGameRules()->GiveGuardianBotGrenades( this );

			if ( CSGameRules()->IsPlayingCoopMission() )
			{
				for ( int i = 0; i < 3; i++ )
					GiveNamedItem( "weapon_healthshot" );
			}
		}
	}

	if( CSGameRules()->IsBombDefuseMap() && mp_defuser_allocation.GetInt() == DefuserAllocation::All && GetTeamNumber() == TEAM_CT )
	{
		GiveDefuser( false );
	}

	if ( CSGameRules()->IsArmorFree() )
	{
		// Armor must be purchased in competitive mode
		if ( CSGameRules()->IsPlayingCoopMission() )
			GiveNamedItem( "item_heavyassaultsuit" );
		else
			GiveNamedItem( "item_assaultsuit" );
	}

	const char *pchTeamKnifeName = GetTeamNumber() == TEAM_TERRORIST ? "weapon_knife_t" : "weapon_knife";

	// don't give default items if the player is in a training map or deathmatch- we control weapon giving in the map for training and in DM, the player could get a random weapon
	if ( CSGameRules()->IsPlayingTraining() || CSGameRules()->IsPlayingGunGameDeathmatch() )
	{
		if ( CSGameRules()->IsPlayingGunGameDeathmatch() )
		{
			CBaseCombatWeapon *knife = Weapon_GetSlot( WEAPON_SLOT_KNIFE );	
			// if the player doesn't have something in the melee slot, give them a knife
			if ( !knife )
				GiveNamedItem( pchTeamKnifeName );

			// if they don't have any pistol, give them the default pistol
			if ( !Weapon_GetSlot( WEAPON_SLOT_PISTOL ) )
			{
				const char *secondaryString = NULL;
				if ( GetTeamNumber() == TEAM_CT )
					secondaryString = mp_ct_default_secondary.GetString();
				else if ( GetTeamNumber() == TEAM_TERRORIST )
					secondaryString = mp_t_default_secondary.GetString();

				CSWeaponID weaponId = WeaponIdFromString( secondaryString );
				if ( weaponId )
				{
					const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponId );
					if ( pWeaponInfo && pWeaponInfo->GetWeaponType() == WEAPONTYPE_PISTOL )
					{
						GiveNamedItem( secondaryString );
						m_bUsingDefaultPistol = true;
					}
				}
			}
		}

		m_bPickedUpWeapon = false; // make sure this is set after getting default weapons
		return;
	}

	if ( CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		// Single Player Progressive Gun Game, so give the current weapon
		GiveCurrentProgressiveGunGameWeapon();

		// Give each player the knife as well if they don't have it already
		if ( !Weapon_GetSlot( WEAPON_SLOT_KNIFE ) )
		{
			int thisWeaponID = CSGameRules()->GetCurrentGunGameWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() );

			if ( thisWeaponID != WEAPON_KNIFE_GG )
			{
				GiveNamedItem( pchTeamKnifeName );
			}
		}

		// Award grenades in TR Bomb mode
		if ( CSGameRules()->IsPlayingGunGameTRBomb() )
		{
			bool bGiveMolotov = false;
			bool bGiveFlashbang = false;
			bool bGiveHEGrenade = false;
			bool bGiveIncendiary = false;

			int nBonusGrenade = CSGameRules()->GetGunGameTRBonusGrenade( this );

			if ( nBonusGrenade == WEAPON_MOLOTOV && !m_bGunGameTRModeHasMolotov )
			{
				// Award a molotov cocktail
				bGiveMolotov = true;
				m_bGunGameTRModeHasMolotov = true;			
			}
			else if ( nBonusGrenade == WEAPON_INCGRENADE && !m_bGunGameTRModeHasIncendiary )
			{
				// Award an incendiary grenade
				bGiveIncendiary = true;
				m_bGunGameTRModeHasIncendiary = true;
			}
			else if ( nBonusGrenade == WEAPON_FLASHBANG && !m_bGunGameTRModeHasFlashbang )
			{
				// Award a flash grenade
				bGiveFlashbang = true;
				m_bGunGameTRModeHasFlashbang = true;
			}
			else if ( nBonusGrenade == WEAPON_HEGRENADE && !m_bGunGameTRModeHasHEGrenade )
			{
				// Award an he grenade
				bGiveHEGrenade = true;
				m_bGunGameTRModeHasHEGrenade = true;
			}

			// Give grenades as necessary based on flags since we want unused grenades to persist between rounds
			if ( m_bGunGameTRModeHasMolotov && !HasWeaponOfType( WEAPON_MOLOTOV ) )
			{
				GiveWeaponFromID( WEAPON_MOLOTOV );
				m_bGunGameTRModeHasMolotov = true;
			}

			if ( m_bGunGameTRModeHasIncendiary && !HasWeaponOfType( WEAPON_INCGRENADE ) )
			{
				GiveWeaponFromID( WEAPON_INCGRENADE );
				m_bGunGameTRModeHasIncendiary = true;
			}

			if ( m_bGunGameTRModeHasFlashbang && !HasWeaponOfType( WEAPON_FLASHBANG ) )
			{
				GiveWeaponFromID( WEAPON_FLASHBANG );
				m_bGunGameTRModeHasFlashbang = true;
			}

			if ( m_bGunGameTRModeHasHEGrenade && !HasWeaponOfType( WEAPON_HEGRENADE ) )
			{
				GiveWeaponFromID( WEAPON_HEGRENADE );
				m_bGunGameTRModeHasHEGrenade = true;
			}

			if ( bGiveMolotov || bGiveFlashbang || bGiveHEGrenade || bGiveIncendiary)
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "gg_bonus_grenade_achieved" );

				if ( event )
				{
					event->SetInt( "userid", GetUserID() );
					gameeventmanager->FireEvent( event );
				}

				//HintMessage( "BONUS GREANDE!", true, true );
			}
		}

		return;
	}
	
	
	CBaseCombatWeapon *knife = Weapon_GetSlot( WEAPON_SLOT_KNIFE );	
	CBaseCombatWeapon *pistol = Weapon_GetSlot( WEAPON_SLOT_PISTOL );	
	CBaseCombatWeapon *rifle = Weapon_GetSlot( WEAPON_SLOT_RIFLE );	
	 
	//If the player has the knife, then they survived the previous round and need to display their current inventrory
	//The only question is whether they got rid of their pistol. 
	if ( knife )
	{	
		if ( pistol )
		{
			CSingleUserRecipientFilter filter(this );	
			filter.MakeReliable();			
			CCSUsrMsg_DisplayInventory msg;
			msg.set_display( true );
			msg.set_user_id( GetUserID() );
			SendUserMessage( filter, CS_UM_DisplayInventory, msg );
			return;
		}
		else
		{
			CSingleUserRecipientFilter filter(this );	
			filter.MakeReliable();			
			CCSUsrMsg_DisplayInventory msg;
			msg.set_display( false );
			msg.set_user_id( GetUserID() );
			SendUserMessage( filter, CS_UM_DisplayInventory, msg );
		}
	}


	m_bUsingDefaultPistol = true;

	const char *meleeString = NULL;
	if ( GetTeamNumber() == TEAM_CT )
		meleeString = mp_ct_default_melee.GetString();
	else if ( GetTeamNumber() == TEAM_TERRORIST )
		meleeString = mp_t_default_melee.GetString();

	if ( meleeString && *meleeString )
	{
		// remove everything in the melee slot
		while ( knife )
		{
			DestroyWeapon( knife );
			knife = Weapon_GetSlot( WEAPON_SLOT_KNIFE );	
		}

		// always give them a knife (mainly because we don't have animations to support no weapons)
		GiveNamedItem( pchTeamKnifeName );

		char token[256];
		meleeString = engine->ParseFile( meleeString, token, sizeof( token ) );
		while (meleeString != NULL )
		{
			// if it's not a knife, give it.  This is pretty much only going to be a taser, but we support anything
			if ( V_strcmp( token, "weapon_knife" ) )
			{
				CSWeaponID weaponId = WeaponIdFromString( token );
				if ( weaponId )
				{
					//TODO: NEED ECON ITEM
					const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponId );
					if ( pWeaponInfo && pWeaponInfo->GetWeaponType() == WEAPONTYPE_KNIFE )
					{
						GiveNamedItem( token );
					}
				}
			}
			meleeString = engine->ParseFile( meleeString, token, sizeof( token ) );
		}
	}

	if ( !pistol )
	{
		const char *secondaryString = NULL;
		if ( GetTeamNumber() == TEAM_CT )
			secondaryString = mp_ct_default_secondary.GetString();
		else if ( GetTeamNumber() == TEAM_TERRORIST )
			secondaryString = mp_t_default_secondary.GetString();
	
		CSWeaponID weaponId = WeaponIdFromString( secondaryString );
		if ( weaponId )
		{
			//TODO: NEED ECON ITEM
			const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponId );
			if ( pWeaponInfo && pWeaponInfo->GetWeaponType() == WEAPONTYPE_PISTOL )
				GiveNamedItem( secondaryString );
		}
	}

	if ( !rifle )
	{
		const char *primaryString = NULL;
		if ( GetTeamNumber() == TEAM_CT )
			primaryString = mp_ct_default_primary.GetString();
		else if ( GetTeamNumber() == TEAM_TERRORIST )
			primaryString = mp_t_default_primary.GetString();

		CSWeaponID weaponId = WeaponIdFromString( primaryString );
		if ( weaponId )
		{
			const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponId );
			if ( pWeaponInfo && pWeaponInfo->GetWeaponType() != WEAPONTYPE_KNIFE && pWeaponInfo->GetWeaponType() != WEAPONTYPE_PISTOL && pWeaponInfo->GetWeaponType() != WEAPONTYPE_C4 && pWeaponInfo->GetWeaponType() != WEAPONTYPE_GRENADE && pWeaponInfo->GetWeaponType() != WEAPONTYPE_EQUIPMENT )
				GiveNamedItem( primaryString );
		}
	}

	// give the player grenades if he needs them
	const char *grenadeString = NULL;
	if ( GetTeamNumber() == TEAM_CT )
		grenadeString = mp_ct_default_grenades.GetString();
	else if ( GetTeamNumber() == TEAM_TERRORIST )
		grenadeString = mp_t_default_grenades.GetString();

	if ( grenadeString && *grenadeString )
	{
		char token[256];
		grenadeString = engine->ParseFile( grenadeString, token, sizeof( token ) );
		while ( grenadeString != NULL )
		{
			CSWeaponID weaponId = WeaponIdFromString( token );
			if ( weaponId )
			{
				const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponId );
				if ( pWeaponInfo && pWeaponInfo->GetWeaponType() == WEAPONTYPE_GRENADE )
				{
					if ( !HasWeaponOfType( weaponId ) )
						GiveNamedItem( token );
				}
			}
			grenadeString = engine->ParseFile( grenadeString, token, sizeof( token ) );
		}
	}

	if ( Weapon_GetSlot( WEAPON_SLOT_PISTOL ) )
	{
		Weapon_GetSlot( WEAPON_SLOT_PISTOL )->GiveReserveAmmo( AMMO_POSITION_PRIMARY, 250 );
	}

	if ( Weapon_GetSlot( WEAPON_SLOT_RIFLE ) )
	{
		Weapon_GetSlot( WEAPON_SLOT_RIFLE )->GiveReserveAmmo( AMMO_POSITION_PRIMARY, 250 );
	}


	m_bPickedUpWeapon = false; // make sure this is set after getting default weapons

	if ( CSGameRules()->IsPlayingCoopMission() )
	{
		SelectItem( "weapon_healthshot" );
	}
}

bool IsWearableEnabled( loadout_positions_t iSlot )
{
	return iSlot == LOADOUT_POSITION_CLOTHING_HANDS;
}

void CCSPlayer::GiveDefaultWearables( void )
{
	// Must a living human player on a team
	if ( ( GetTeamNumber() != TEAM_CT && GetTeamNumber() != TEAM_TERRORIST ) )
		return;

	// Loop through our current wearables and ensure we're supposed to have them.
	ValidateWearables();

	if ( IsBot() || IsControllingBot() )
		return;
	
	// Now go through all our loadout slots and find any wearables that we should be wearing.

	for ( int i = LOADOUT_POSITION_FIRST_COSMETIC; i <= LOADOUT_POSITION_LAST_COSMETIC; i++ )
	{
		m_EquippedLoadoutItemIndices[i] = LOADOUT_SLOT_USE_BASE_ITEM;
	}

	for ( int i = LOADOUT_POSITION_FIRST_COSMETIC; i <= LOADOUT_POSITION_LAST_COSMETIC; i++ )
	{
		if ( !IsWearableEnabled( ( loadout_positions_t ) i ) )
			continue;

		GiveWearableFromSlot( ( loadout_positions_t ) i );
	}
}

void CCSPlayer::GiveWearableFromSlot( loadout_positions_t position )
{
	/** Removed for partner depot **/
}


void CCSPlayer::SetClanTag( const char *pTag )
{
	if ( pTag )
	{
		Q_strncpy( m_szClanTag, pTag, sizeof( m_szClanTag ) );
	}
}

void CCSPlayer::SetClanName( const char *pName )
{
	if ( pName )
	{
		Q_strncpy( m_szClanName, pName, sizeof( m_szClanName ) );
	}
}

void CCSPlayer::InitTeammatePreferredColor()
{
	const char *pColor = engine->GetClientConVarValue( entindex(), "cl_color" );
	int nColor = atoi( pColor ); // convar.cpp code parses strings wierdly and cannot enforce range on a value e.g. " 4343" with leading space
	// so we have to just assume that whatever string user supplied would parse as zero

	if ( nColor >= 0 && nColor <= 4 )
	{
		SetTeammatePreferredColor( nColor );
	}
	else
	{
		SetTeammatePreferredColor( 0 );
	}
}

void CCSPlayer::SetTeammatePreferredColor( int nColor )
{
	if ( nColor < 0 || nColor > 4 )
	{
		AssertMsg( nColor >= 0 && nColor <= 4, "SetTeammatePreferredColor called with an invalid color (outside the range of 0 and 4)" );
		return;
	}

	m_iTeammatePreferredColor = nColor;
}

void CCSPlayer::CreateRagdollEntity()
{
	// If we already have a ragdoll, don't make another one.
	CCSRagdoll *pRagdoll = dynamic_cast< CCSRagdoll* >( m_hRagdoll.Get() );
	
	if ( !pRagdoll )
	{
		// create a new one
		pRagdoll = dynamic_cast< CCSRagdoll* >( CreateEntityByName( "cs_ragdoll" ) );
	}

	if ( pRagdoll )
	{
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_vecForce = m_vecTotalBulletForce;
		pRagdoll->m_hPlayer = this;
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_nModelIndex = m_nModelIndex;
		pRagdoll->m_nForceBone = m_nForceBone;
		pRagdoll->m_iDeathPose = m_iDeathPose;
		pRagdoll->m_iDeathFrame = m_iDeathFrame;
		pRagdoll->m_flDeathYaw = m_flDeathYaw;
		pRagdoll->m_flAbsYaw = GetAbsAngles()[YAW];
		pRagdoll->Init();
	}

	// ragdolls will be removed on round restart automatically
	m_hRagdoll = pRagdoll;
}

int CCSPlayer::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if ( m_bGunGameImmunity )
	{
		// No damage if immune
		return 0;
	}

	// set damage type sustained
	m_bitsDamageType |= info.GetDamageType();

	if ( !CBaseCombatCharacter::OnTakeDamage_Alive( info ) )
		return 0;

	// don't apply damage forces in CS

	// fire global game event

	IGameEvent * event = gameeventmanager->CreateEvent( "player_hurt" );

	if ( event )
	{
		event->SetInt("userid", GetUserID() );
		event->SetInt("health", Max(0, m_iHealth.Get() ) );
		event->SetInt("armor", Max(0, ArmorValue() ) );

		event->SetInt( "dmg_health", m_lastDamageHealth );
		event->SetInt( "dmg_armor", m_lastDamageArmor );

		if ( info.GetDamageType() & DMG_BLAST )
		{
			event->SetInt( "hitgroup", HITGROUP_GENERIC );
		}
		else
		{
			event->SetInt( "hitgroup", m_LastHitGroup );
		}

		CBaseEntity * attacker = info.GetAttacker();
		const char *weaponName = "";

		// FIXME[pmf]: this could break with dynamic weapons
		if ( attacker->IsPlayer() )
		{
			CBasePlayer *player = ToBasePlayer( attacker );
			event->SetInt("attacker", player->GetUserID() ); // hurt by other player

			CBaseEntity *pInflictor = info.GetInflictor();
			if ( pInflictor )
			{
				if ( pInflictor == player )
				{
					// If the inflictor is the killer,  then it must be their current weapon doing the damage
					if ( player->GetActiveWeapon() )
					{
						weaponName = player->GetActiveWeapon()->GetClassname();
					}
				}
				else
				{
					weaponName = STRING( pInflictor->m_iClassname );  // it's just that easy
				}
			}
		}
		else
		{
			event->SetInt("attacker", 0 ); // hurt by "world"
		}

		if ( IsWeaponClassname( weaponName ) )
		{
			weaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
		}
		else if ( StringHasPrefixCaseSensitive( weaponName, "hegrenade" ) )	//"hegrenade_projectile"	
		{
			// [tj] Handle grenade-surviving achievement
			if ( IsOtherEnemy( info.GetAttacker()->entindex() ) )
			{
				m_grenadeDamageTakenThisRound += info.GetDamage();
			}

			weaponName = "hegrenade";
		}
		else if ( StringHasPrefixCaseSensitive( weaponName, "flashbang" ) )	//"flashbang_projectile"
		{
			weaponName = "flashbang";
		}
		else if ( StringHasPrefixCaseSensitive( weaponName, "smokegrenade" ) )	//"smokegrenade_projectile"
		{
			weaponName = "smokegrenade";
		}

		event->SetString( "weapon", weaponName );
		event->SetInt( "priority", 6 ); // player_hurt

		gameeventmanager->FireEvent( event );
	}
	
	return 1;
}

// Returns the % of the enemies this player killed in the round
int CCSPlayer::GetPercentageOfEnemyTeamKilled()
{
	if ( m_NumEnemiesAtRoundStart > 0 )
	{
		if ( m_NumEnemiesKilledThisRound > m_NumEnemiesAtRoundStart )
		{
			DevMsg( "Invalid percentage of enemies killed\n" );
		}
		return RoundFloatToInt( (float )m_NumEnemiesKilledThisRound / (float)m_NumEnemiesAtRoundStart * 100.0f );
	}

	return 0;
}

void CCSPlayer::HandleOutOfAmmoKnifeKills( CCSPlayer* pAttackerPlayer, CWeaponCSBase* pAttackerWeapon )
{
	if ( pAttackerWeapon && 
		pAttackerWeapon->IsA( WEAPON_KNIFE ) )
	{
		// if they were out of ammo in their primary and secondary AND had a primary or secondary, log as an out of ammo knife kill

		bool hasValidPrimaryOrSecondary = false; // can't really be out of ammo on anything if we don't have either a primary or a secondary
		bool allPrimaryAndSecondariesOutOfAmmo = true;



		if(	pAttackerPlayer->HasPrimaryWeapon() )
		{
			hasValidPrimaryOrSecondary = true;


			CBaseCombatWeapon *pWeapon = pAttackerPlayer->Weapon_GetSlot( WEAPON_SLOT_RIFLE );
			if( !pWeapon || !pAttackerPlayer->DidPlayerEmptyAmmoForWeapon( pWeapon ) )
			{
				allPrimaryAndSecondariesOutOfAmmo = false;
			}
		}
		if(	pAttackerPlayer->HasSecondaryWeapon() )
		{
			hasValidPrimaryOrSecondary = true;


			CBaseCombatWeapon *pWeapon = pAttackerPlayer->Weapon_GetSlot( WEAPON_SLOT_PISTOL );
			if( !pWeapon || !pAttackerPlayer->DidPlayerEmptyAmmoForWeapon( pWeapon ) )
			{
				allPrimaryAndSecondariesOutOfAmmo = false;
			}
		}

		if( hasValidPrimaryOrSecondary && allPrimaryAndSecondariesOutOfAmmo )
		{
			pAttackerPlayer->IncrKnifeKillsWhenOutOfAmmo();
		}

	}
}

class CPlayerTrophy : public CPhysicsProp
{
public:
	void OnTouchLoot( CBaseEntity *other )
	{
	/** Removed for partner depot **/
	}
};

void CCSPlayer::Event_Killed( const CTakeDamageInfo &info )
{
	SetKilledTime( gpGlobals->curtime );

	// [pfreese] Process on-death achievements
	ProcessPlayerDeathAchievements(ToCSPlayer(info.GetAttacker() ), this, info );

	SetArmorValue( 0 );

	m_bIsRespawningForDMBonus = false;

	// [tj] Added a parameter so we know if it was death that caused the drop
	// [menglish] Keep track of what the player has dropped for the freeze panel callouts
	CBaseEntity* pAttacker = info.GetAttacker();
	bool friendlyFire = pAttacker && IsOtherSameTeam( pAttacker->GetTeamNumber() ) && !IsOtherEnemy( pAttacker->entindex() );
	
	CCSPlayer* pAttackerPlayer = ToCSPlayer( info.GetAttacker() );
	if ( pAttackerPlayer )
	{
		CWeaponCSBase* pAttackerWeapon = dynamic_cast< CWeaponCSBase * >( info.GetWeapon() );	// this can be NULL if the kill is by HE/molly/impact/etc. (inflictor is non-NULL and points to grenade then)

		if ( CSGameRules()->IsPlayingGunGameProgressive() && gg_knife_kill_demotes.GetBool() )
		{
			if ( pAttackerWeapon && pAttackerWeapon->IsA( WEAPON_KNIFE ) )
			{
				if ( IsOtherEnemy( pAttackerPlayer->entindex() ) )	// Don't demote a team member
				{
					// Killed by a knife, so drop one weapon class
					SubtractProgressiveWeaponIndex();

					CRecipientFilter filter;
					filter.AddRecipient( this );
					filter.MakeReliable();
					CFmtStr fmtEntName( "#ENTNAME[%d]%s", pAttackerPlayer->entindex(), pAttackerPlayer->GetPlayerName() );
					UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#Cstrike_TitlesTXT_Hint_lost_a_level", fmtEntName.Access() );
					// todo: play this sound: orch_hit_csharp_short?
				}
			}
		}

		// killed by a taser?
		if ( pAttackerWeapon && pAttackerWeapon->IsA( WEAPON_TASER ) )
		{
			m_bKilledByTaser = true;
		}

		if ( pAttackerPlayer != this && cash_player_get_killed.GetInt() != 0 )
		{
			AddAccountAward( PlayerCashAward::RESPAWN );
		}

		HandleOutOfAmmoKnifeKills( pAttackerPlayer, pAttackerWeapon );

		// here we figure out if the attacker saved another person
		Vector forward;
		AngleVectors( EyeAngles(), &forward, NULL, NULL);
		CTeam *pAttackerTeam = GetGlobalTeam( pAttackerPlayer->GetTeamNumber() );
		if ( pAttackerTeam && !( m_LastDamageType & DMG_FALL ) && !m_wasNotKilledNaturally )
		{
			for ( int iPlayer = 0; iPlayer < pAttackerTeam->GetNumPlayers(); iPlayer++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( pAttackerTeam->GetPlayer( iPlayer ) );
				if ( !pPlayer || pAttackerPlayer == this || pPlayer == this || pPlayer == pAttackerPlayer )
					continue;

				if ( pAttackerPlayer->IsOtherEnemy( pPlayer->entindex() ) )
					continue;

				Assert( pPlayer->GetTeamNumber() == pAttackerTeam->GetTeamNumber() );

				if ( pPlayer->m_lifeState == LIFE_ALIVE )
				{
					Vector toAimSpot = pPlayer->EyePosition() - EyePosition();
					toAimSpot.NormalizeInPlace();
					float flKillerCone = DotProduct( toAimSpot, forward );
					// aiming tolerance depends on how close the target is - closer targets subtend larger angles
					float aimTolerance = 0.8f;
					if ( flKillerCone >= aimTolerance )
					{
						// the target was aiming at this player, now do a quick trace to them to see if they could actually shoot them
						trace_t result;
						UTIL_TraceLine( EyePosition(), pPlayer->EyePosition(), MASK_SOLID, this, COLLISION_GROUP_NONE, &result );
						if ( !result.m_pEnt || result.m_pEnt != pPlayer )
							continue;

						if ( GetActiveCSWeapon() )
						{
							// if they are holding a grenade or the c4, don't count it
							if ( GetActiveCSWeapon()->GetWeaponType() == WEAPONTYPE_GRENADE || GetActiveCSWeapon()->GetWeaponType() == WEAPONTYPE_C4 )
								continue;

							Vector vecLength = (result.startpos - result.endpos);

							// if they are holding a knife or taser, check knife range
							if ( GetActiveCSWeapon()->GetWeaponType() == WEAPONTYPE_KNIFE && ( vecLength.Length() > 80.0f ) )
								continue;
							if ( GetActiveCSWeapon()->GetCSWeaponID() == WEAPON_TASER && ( vecLength.Length() > 200.0f ) )
								continue;
						}
						// now make sure that the "saved" player wasn't looking directly at the guy who was killed
						Vector vecAttackerFwd;
						AngleVectors( pPlayer->EyeAngles(), &vecAttackerFwd, NULL, NULL);
						Vector toKilledSpot = EyePosition() - pPlayer->EyePosition();
						toKilledSpot.NormalizeInPlace();
						float flKilledCone = DotProduct( toKilledSpot, vecAttackerFwd );
						if ( flKilledCone < 0.65f )
						{
							// we got it!  send a message to the "saved"
							CSingleUserRecipientFilter usersaved( pPlayer );
							usersaved.MakeReliable();
							CFmtStr fmtEntName( "#ENTNAME[%d]%s", entindex(), GetPlayerName() );
							UTIL_ClientPrintFilter( usersaved, HUD_PRINTTALK, "#Chat_SavePlayer_Saved",
								CFmtStr( "#ENTNAME[%d]%s", pAttackerPlayer->entindex(), pAttackerPlayer->GetPlayerName() ),
								CFmtStr( "#ENTNAME[%d]%s", entindex(), GetPlayerName() ) );

							// now send a message to the "savior"
							CSingleUserRecipientFilter usersavior( pAttackerPlayer );
							usersavior.MakeReliable();
							UTIL_ClientPrintFilter( usersavior, HUD_PRINTTALK, "#Chat_SavePlayer_Savior",
								CFmtStr( "#ENTNAME[%d]%s", pPlayer->entindex(), pPlayer->GetPlayerName() ),
								CFmtStr( "#ENTNAME[%d]%s", entindex(), GetPlayerName() ) );

							// now send a message to the "savior"
							CTeamRecipientFilter teamfilter( TEAM_SPECTATOR, true );
							UTIL_ClientPrintFilter( teamfilter, HUD_PRINTTALK, "#Chat_SavePlayer_Spectator",
								CFmtStr( "#ENTNAME[%d]%s", pAttackerPlayer->entindex(), pAttackerPlayer->GetPlayerName() ),
								CFmtStr( "#ENTNAME[%d]%s", pPlayer->entindex(), pPlayer->GetPlayerName() ),
								CFmtStr( "#ENTNAME[%d]%s", entindex(), GetPlayerName() ) );
							break;
						}		
					}
				}
			}
		}
	}

	// if we died from killing ourself, check if we should lose a weapon in progressive
	DecrementProgressiveWeaponFromSuicide();

	//Only count the drop if it was not friendly fire
	DropWeapons(true, !friendlyFire );

	m_iNumFollowers = 0;
	
	// Just in case the progress bar is on screen, kill it.
	SetProgressBarTime( 0 );

	m_bIsDefusing = false;
	m_bIsGrabbingHostage = false;

	m_bHasNightVision = false;
	m_bNightVisionOn = false;

	// [dwenger] Added for fun-fact support
	m_bPickedUpDefuser = false;
	m_bDefusedWithPickedUpKit = false;
	m_bPickedUpWeapon = false;
	m_bAttemptedDefusal = false;
	m_nPreferredGrenadeDrop = 0;
	m_bIsSpawning = false;

	m_flDefusedBombWithThisTimeRemaining = 0;

	m_bHasHelmet = false;
	m_bHasHeavyArmor = false;

	m_flFlashDuration = 0.0f;
	if( FlashlightIsOn() )
		FlashlightTurnOff();
	
	// show killer in death cam mode
	if( IsValidObserverTarget( info.GetAttacker() ) )
	{
		SetObserverTarget( info.GetAttacker() );
	}
	else
	{
		ResetObserverMode();
	}

	//update damage info with our accumulated physics force
	CTakeDamageInfo subinfo = info;
	subinfo.SetDamageForce( m_vecTotalBulletForce );

	//Adrian: Select a death pose to extrapolate the ragdoll's velocity.
	SelectDeathPose( info );

	// See if there's a ragdoll magnet that should influence our force.
	CRagdollMagnet *pMagnet = CRagdollMagnet::FindBestMagnet( this );
	if( pMagnet )
	{
		m_vecTotalBulletForce += pMagnet->GetForceVector( this );
	}

	// Note: since we're dead, it won't draw us on the client, but we don't set EF_NODRAW
	// because we still want to transmit to the clients in our PVS.
	CreateRagdollEntity();

	State_Transition( STATE_DEATH_ANIM );	// Transition into the dying state.
	BaseClass::Event_Killed( subinfo );

	if ( IsAssassinationTarget() )
	{
		CPlayerTrophy *pLoot = (CPlayerTrophy*)CreateEntityByName( "prop_physics" );
		Vector vecDir = RandomVector( 0.0f, 25.0f );
		pLoot->SetAbsOrigin( GetAbsOrigin() + vecDir + Vector( 0.f, 0.f, 35.f ) );
		pLoot->KeyValue( "model", g_pszLootModelName );
		pLoot->SetAbsVelocity( vecDir );
		pLoot->SetLocalAngularVelocity( QAngle( RandomFloat( 50, 100 ), RandomFloat( 50, 100 ), RandomFloat( 50, 100 ) ) );
		pLoot->Spawn();
		pLoot->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE );
		pLoot->SetTouch( &CPlayerTrophy::OnTouchLoot );
	}

	// [pfreese] If this kill ended the round, award the MVP to someone on the
	// winning team.
	// TODO - move this code somewhere else more MVP related

	if ( CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		// Lose all awarded grenades on death
		m_bGunGameTRModeHasHEGrenade = false;
		m_bGunGameTRModeHasFlashbang = false;
		m_bGunGameTRModeHasMolotov = false;
		m_bGunGameTRModeHasIncendiary = false;
	}

	if ( CSGameRules()->IsPlayingGunGame() )
	{
		RecordRebuyStructLastRound();

		if ( pAttacker != this || !IsAbleToInstantRespawn() )
		{
			// Re-evaluate end-of-gun-game when a player is killed
			if ( CSGameRules()->CheckWinConditions() )
			{
				m_bMadeFinalGunGameProgressiveKill = false;
//				int nEntity2 = pAttackerPlayer ? pAttackerPlayer->entindex() : this->entindex();
//				CSGameRules()->StartSlomoDeathCam( this->entindex(), nEntity2 );
			}
		}
	}
	else
	{
		if ( CSGameRules()->CheckWinConditions() )
		{
//			int nEntity2 = pAttackerPlayer ? pAttackerPlayer->entindex() : this->entindex();
//			CSGameRules()->StartSlomoDeathCam( this->entindex(), nEntity2 );
		}
	}

	SendLastKillerDamageToClient( pAttackerPlayer );

	OutputDamageGiven();
	OutputDamageTaken();

	if ( m_bPunishedForTK )
	{
		m_bPunishedForTK = false;
		HintMessage( "#Hint_cannot_play_because_tk", true, true );
	}

	if ( CSGameRules()->ShouldRecordMatchStats() )
	{
		m_iMatchStats_LiveTime.GetForModify( CSGameRules()->GetRoundsPlayed() ) += CSGameRules()->GetRoundElapsedTime();

		// Keep track in QMM data
		if ( m_uiAccountId && CSGameRules() )
		{
			if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
			{
				pQMM->m_iMatchStats_LiveTime[ CSGameRules()->GetRoundsPlayed() ] = m_iMatchStats_LiveTime.Get( CSGameRules()->GetRoundsPlayed() );
			}
		}
	}


#if CS_CONTROLLABLE_BOTS_ENABLED
	if ( IsControllingBot() )	// Should this be here, or at the top?
	{
		ReleaseControlOfBot();
	}
#endif

	if ( pAttackerPlayer &&
		!CSGameRules()->IsWarmupPeriod() &&
		CSGameRules()->IsPlayingCooperativeGametype() )
	{
		int nTeamCheck = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
		if ( CSGameRules()->IsPlayingCoopMission() )
			nTeamCheck = TEAM_CT;

		if ( pAttacker->GetTeamNumber() == nTeamCheck )
		{
			CWeaponCSBase* pSpecialWeapon = dynamic_cast< CWeaponCSBase * >( info.GetWeapon() );	// this can be NULL if the kill is by HE/molly/impact/etc. (inflictor is non-NULL and points to grenade then)
			if ( CSGameRules()->CheckGotGuardianModeSpecialKill( pSpecialWeapon ) )
			{
				CSingleUserRecipientFilter filter( pAttackerPlayer );
				EmitSound( filter, pAttackerPlayer->entindex(), "GunGameWeapon.ImpendingLevelUp" );
			}
		}

		if ( GetTeamNumber() == nTeamCheck )
		{
			CBroadcastRecipientFilter filtermsg;
			filtermsg.RemoveAllRecipients();

			//play a sound here for all players on the player team to indicate that someone died
			for ( int j = 1; j <= MAX_PLAYERS; j++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( j ) );
				if ( pPlayer && pPlayer->GetTeamNumber() == nTeamCheck )
				{
					if ( pPlayer != this )
						filtermsg.AddRecipient( pPlayer );

					CSingleUserRecipientFilter filter( pPlayer );
					pPlayer->EmitSound( filter, pPlayer->entindex(), "UI.ArmsRace.Demoted" );
				}
			}

			UTIL_ClientPrintFilter( filtermsg, HUD_PRINTCENTER, "#SFUI_Notice_Guardian_PlayerHasDied", GetPlayerName() );
		}
	}
}

bool CCSPlayer::IsCloseToActiveBomb( void )
{
	float bombCheckDistSq = AchievementConsts::KillEnemyNearBomb_MaxDistance * AchievementConsts::KillEnemyNearBomb_MaxDistance;
	for ( int i=0; i < g_PlantedC4s.Count(); i++ )
	{
		CPlantedC4 *pC4 = g_PlantedC4s[i];

		if ( pC4 && pC4->IsBombActive() )
		{
			Vector bombPos = pC4->GetAbsOrigin();
			Vector distToBomb = this->GetAbsOrigin() - bombPos;
			if ( distToBomb.LengthSqr() < bombCheckDistSq )
			{
				return true;
			}
		}
	}		
	
	return false;
}

bool CCSPlayer::IsCloseToHostage( void )
{
	float hostageCheckDistSq = AchievementConsts::KillEnemyNearHostage_MaxDistance * AchievementConsts::KillEnemyNearHostage_MaxDistance;
	for ( int i=0; i < g_Hostages.Count(); i++ )
	{
		CHostage *pHostage = g_Hostages[i];
		if ( pHostage && pHostage->IsValid() )
		{
			Vector hostagePos = pHostage->GetAbsOrigin();
			Vector distToHostage = this->GetAbsOrigin() - hostagePos;
			if ( distToHostage.LengthSqr() < hostageCheckDistSq )
			{
				return true;
			}
		}
	}		

	return false;
}

bool CCSPlayer::IsObjectiveKill( CCSPlayer* pCSVictim )
{
	// check all cases where this kill is 'objective' Based
	
	// Killing someone close to a hostage
	if ( ( pCSVictim && pCSVictim->IsCloseToHostage() ) ||
		this->IsCloseToHostage() )
		return true;
	
	switch ( GetTeamNumber() )
	{
	case TEAM_TERRORIST:
		// Terrorist kills CT in a bomb plant zone after a bomb is planted
		if ( ( pCSVictim && pCSVictim->IsCloseToActiveBomb() ) 
			|| this->IsCloseToActiveBomb() )
			return true;

		// Terrorist kills hostage rescuer
		if ( pCSVictim && ( pCSVictim->GetNumFollowers() > 0 ) )
			return true;

		break;

	case TEAM_CT:
		// killing someone WHILE guiding hostages
		if ( this->GetNumFollowers() > 0 )
			return true;

		break;
	}
	
	return false;
}

// [menglish, tj] Update and check any one-off achievements based on the kill
// Notify that I've killed some entity. (called from Victim's Event_Killed ).
// (dkorus:  This function appears to be misnamed, it also called for self kills/suicides )
void CCSPlayer::Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::Event_KilledOther(pVictim, info );

	if ( !CSGameRules() )
		return;

	CCSPlayer *pCSVictim = ToCSPlayer( pVictim );
	CCSPlayer *pCSAttacker = ToCSPlayer( info.GetAttacker() );
	if ( pCSVictim == pCSAttacker )
	{
		// Bail if this was a suicide
		return;
	}

	if ( IsOtherSameTeam( pVictim->GetTeamNumber() ) && !IsOtherEnemy( pCSVictim )  && pVictim != pCSAttacker )
	{
		CSGameRules()->ScorePlayerTeamKill( pCSAttacker );
		UpdateTeamLeaderPlaySound( pCSAttacker->GetTeamNumber() );
	}
	else // on a different team from the attacker
	{
		
		// score kill
		if ( pCSAttacker && pCSVictim &&
			(pVictim->GetTeamNumber() == TEAM_CT || pVictim->GetTeamNumber() == TEAM_TERRORIST ) ) // this makes sure the victim is not a hostage before awarding a score
		{
			if ( pCSAttacker->IsObjectiveKill( pCSVictim ) )
				CSGameRules()->ScorePlayerObjectiveKill( pCSAttacker );
			else			
				CSGameRules()->ScorePlayerKill( pCSAttacker );
		}
		else if ( pCSAttacker && pVictim && StringHasPrefixCaseSensitive( pVictim->GetClassname(), "chicken" ) )
		{
			CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase * >( info.GetWeapon() );
			pCSAttacker->AddDeathmatchKillScore( 1, pWeapon ? pWeapon->GetCSWeaponID() : WEAPON_NONE,
				pWeapon ? pWeapon->GetDefaultLoadoutSlot() : LOADOUT_POSITION_INVALID );

			if ( IGameEvent *event = CSGameRules()->CreateWeaponKillGameEvent( "other_death", info ) )
			{
				event->SetInt("otherid", pVictim->entindex() );
				event->SetString("othertype", "chicken" );
				gameeventmanager->FireEvent( event );
			}

			m_NumChickensKilledThisSpawn++;
		}


		// Killed an enemy
		if ( CSGameRules()->IsPlayingGunGame() && pCSVictim && pCSAttacker )
		{
			bool bBonus = false;

			if ( !IsControllingBot() )
			{
				m_iNumGunGameKillsWithCurrentWeapon++;
			}

			IGameEvent * event = gameeventmanager->CreateEvent( "gg_killed_enemy" );

			if ( event )
			{
				event->SetInt("victimid", pCSVictim->GetPlayerInfo()->GetUserID() );
				event->SetInt("attackerid", pCSAttacker->GetPlayerInfo()->GetUserID() );
				if ( pCSVictim->GetDeathFlags() & CS_DEATH_DOMINATION )
				{
					event->SetInt( "dominated", 1 );
				}
				else if ( pCSVictim->GetDeathFlags() & CS_DEATH_REVENGE )
				{
					event->SetInt( "revenge", 1 );
				}
				
				if ( CSGameRules()->IsPlayingGunGameDeathmatch() && pCSAttacker->GetActiveCSWeapon() &&
					CSGameRules()->IsDMBonusActive() && ( CSGameRules()->GetDMBonusWeaponLoadoutSlot() == pCSAttacker->GetActiveCSWeapon()->GetDefaultLoadoutSlot() ) )
						bBonus = true;
				event->SetInt( "bonus", bBonus );

				gameeventmanager->FireEvent( event );

			}

			if ( CSGameRules()->IsPlayingGunGameTRBomb() )
			{
				// don't award if controlling a bot or in the warmup round
				if ( !IsControllingBot() && !CSGameRules()->IsWarmupPeriod() )
				{
					// Don't allow kill points for upgrades after a round has ended - (with some slack)
					// also don't upgrade if we hit the last round before half-time
					if ( CSGameRules()->GetRoundRestartTime() + 0.5f < gpGlobals->curtime && !CSGameRules()->IsLastRoundBeforeHalfTime() )
					{
						// Bump up the count of how many kills this player has in the current round
						m_iNumGunGameTRKillPoints++;
						m_iNumGunGameTRBombTotalPoints++;

						ConVarRef mp_ggtr_bomb_pts_for_upgrade(	"mp_ggtr_bomb_pts_for_upgrade" );
						if ( m_iNumGunGameTRKillPoints == mp_ggtr_bomb_pts_for_upgrade.GetInt() || CSGameRules()->GetGunGameTRBonusGrenade( this ) > 0 )
						{
							// Play client sound for impending weapon upgrade...
							SendGunGameWeaponUpgradeAlert();
						}
					}
				}

				// Test for end of round for gun game TR (ensure that the weapon that made the kill is the one that matches the last weapon in the progression )
				CWeaponCSBase* pWeapon = dynamic_cast<CWeaponCSBase *>( info.GetWeapon() );
				CEconItemView* pItem = pWeapon ? pWeapon->GetEconItemView() : NULL;
				int wID = WEAPON_NONE;
				if ( pItem != NULL )
				{
					wID = pItem->GetItemIndex();
				}
				else
				{
					const CCSWeaponInfo* pDamageWeaponInfo = GetWeaponInfoFromDamageInfo( info );
					wID = pDamageWeaponInfo ? pDamageWeaponInfo->m_weaponId : WEAPON_NONE;
				}
				int curwID = CSGameRules()->GetCurrentGunGameWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() );
				if ( wID == curwID && CSGameRules()->IsFinalGunGameProgressiveWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() ) )
				{
					// Determine if current # kills with this weapon >= # kills necessary to level up the weapon
					if ( m_iNumGunGameKillsWithCurrentWeapon >= CSGameRules()->GetGunGameNumKillsRequiredForWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() ) )
					{
						// Made the proper number of kills with the final weapon so record this fact
						m_bMadeFinalGunGameProgressiveKill = true;

					}
				}
			}

			bool bKilledLeader = false;
			//bool bUpgradedWeapon = false;

			if ( CSGameRules()->IsPlayingGunGameProgressive() )
			{
				if ( pCSVictim != pCSAttacker )
				{
					// Test for end of round
					if ( CSGameRules()->IsFinalGunGameProgressiveWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() ) )
					{
						// Determine if current # kills with this weapon == # kills necessary to level up the weapon
						if ( m_iNumGunGameKillsWithCurrentWeapon == CSGameRules()->GetGunGameNumKillsRequiredForWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() ) )
						{
							// Made the proper number of kills with the final weapon so record this fact
							m_bMadeFinalGunGameProgressiveKill = true;
						}
					}
					else
					{
						int nOtherTeam = pCSVictim->GetTeamNumber();
						bool bIsCurrentLeader = entindex() == GetGlobalTeam( GetTeamNumber() )->GetGGLeader( GetTeamNumber() );
						bKilledLeader = pCSVictim->entindex() == GetGlobalTeam( nOtherTeam )->GetGGLeader( nOtherTeam );
						// send e message that you killed the enemy leader
						if ( bKilledLeader && !bIsCurrentLeader )
							ClientPrint( this, HUD_PRINTCENTER, "#Player_Killed_Enemy_Leader" );

						bool bWithKnife = false;
						if ( pCSAttacker == this )
						{
							CWeaponCSBase* pAttackerWeapon = dynamic_cast< CWeaponCSBase * >( GetActiveWeapon() );
							if ( pAttackerWeapon && pAttackerWeapon->IsA( WEAPON_KNIFE ) )
								bWithKnife = true;
						}
						
						// Determine if current # kills with this weapon == # kills necessary to level up the weapon
						if ( bWithKnife || (!bIsCurrentLeader && bKilledLeader) || m_iNumGunGameKillsWithCurrentWeapon == CSGameRules()->GetGunGameNumKillsRequiredForWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() ) )
						{
							// Reset kill count with respect to new weapon
							m_iNumGunGameKillsWithCurrentWeapon = 0;

							m_iNumRoundKills = 0;
							m_iNumRoundKillsHeadshots = 0;
							m_iNumRoundTKs = 0;

							CSingleUserRecipientFilter filter( this );
							//bUpgradedWeapon = true;

							// emit the level up sound here because emily wants them
							// to overlap with the leader acquisition sound
							EmitSound( filter, entindex(), "UI.ArmsRace.LevelUp" );

							// Single Player Progressive Gun Game, so give the next weapon
							GiveNextProgressiveGunGameWeapon();
// 							if ( CSGameRules()->IsPlayingGunGameProgressive() )
// 							{
// 								int nRequiredKills = CSGameRules()->GetGunGameNumKillsRequiredForWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() );
// 								if ( m_iNumGunGameKillsWithCurrentWeapon >= nRequiredKills || m_iNumRoundKills >= nRequiredKills )
// 								{
// 									// if we hit the max for this weapon and
// 
// 								}
// 							}

							// Alert everyone that this player has the final ggp weapon
							if ( CSGameRules()->IsFinalGunGameProgressiveWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() ) )
							{
								IGameEvent * eventGGFinalWeap = gameeventmanager->CreateEvent( "gg_final_weapon_achieved" );

								// Change the UI to reflect gun-game last weapon level reached
								//ConVarRef sf_ui_tint( "sf_ui_tint" ); 
								//sf_ui_tint.SetValue( g_KnifeLevelTint );

								if ( eventGGFinalWeap )
								{
									//CCSPlayer *pCSAchiever = ( CCSPlayer* )( info.GetAttacker() );

									eventGGFinalWeap->SetInt("playerid", pCSAttacker ? pCSAttacker->GetPlayerInfo()->GetUserID() : -1 );
			
									gameeventmanager->FireEvent( eventGGFinalWeap );

									CRecipientFilter filterKnifeLevelNotification;
									for ( int i = 1; i <= gpGlobals->maxClients; i++ )
									{
										CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

										if ( pPlayer && ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR || pPlayer->GetTeamNumber() == TEAM_CT || pPlayer->GetTeamNumber() == TEAM_TERRORIST ) )
										{
											filterKnifeLevelNotification.AddRecipient( pPlayer );
										}
									}
									filterKnifeLevelNotification.MakeReliable();
									CFmtStr fmtEntName;
									if ( pCSAttacker )
										fmtEntName.AppendFormat( "#ENTNAME[%d]%s", pCSAttacker->entindex(), pCSAttacker->GetPlayerName() );
									UTIL_ClientPrintFilter( filterKnifeLevelNotification, HUD_PRINTTALK, "#SFUI_Notice_Knife_Level", fmtEntName.Access() );
								}
							}
						}
					}
				}
			}

			if ( this == pCSAttacker )
			{
				// force a sort right now
				GetGlobalTeam( GetTeamNumber() )->DetermineGGLeaderAndSort();

				bool bPlayedLeaderSound = UpdateTeamLeaderPlaySound( GetTeamNumber() );
				if ( !bPlayedLeaderSound )
				{
					int nConsecutiveKills = CCS_GameStats.FindPlayerStats( pCSVictim ).statsKills.iNumKilledByUnanswered[entindex()];

					CSingleUserRecipientFilter filter( this );
					// check if we got bonus points first and play that sound
					if ( 0/*bKilledLeader*/ )
					{
						EmitSound( filter, entindex(), "UI.DeathMatchBonusKill" );
					}
// 					else if ( bUpgradedWeapon )
// 					{
// 						EmitSound( filter, entindex(), "UI.ArmsRace.LevelUp" );
// 					}
					else if ( bBonus )
					{
						EmitSound( filter, entindex(), "UI.DeathMatchBonusKill" );
					}
					else if ( CSGameRules()->IsPlayingGunGameProgressive() == false )
					{
						if ( nConsecutiveKills < 2 )
						{
							//EmitSound( filter, entindex(), "Music.Kill_01" );
						}
						else if ( nConsecutiveKills < 3 )
						{
							//EmitSound( filter, entindex(), "Music.Kill_02" );
						}
						else if ( nConsecutiveKills < 4 )
						{
							//EmitSound( filter, entindex(), "Music.Kill_03" );
						}
					}
				}
			}
		}

		if ( pCSVictim && ( pCSVictim->GetDeathFlags() & CS_DEATH_DOMINATION || IsPlayerDominated( pCSVictim->entindex() ) ) )
		{
			m_hDominateEffectPlayer = pCSVictim;
			m_flDominateEffectDelayTime = gpGlobals->curtime + 1.0f;
		}
	}
}

void CCSPlayer::GiveCurrentProgressiveGunGameWeapon( void )
{
	int currentWeaponID = CSGameRules()->GetCurrentGunGameWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() );

	if ( currentWeaponID != -1 )
	{
		// Drop the current pistol
		CBaseCombatWeapon *pWeapon;

		pWeapon = Weapon_GetSlot( WEAPON_SLOT_PISTOL );
		DestroyWeapon( pWeapon );

		// Drop the current rifle
		pWeapon = Weapon_GetSlot( WEAPON_SLOT_RIFLE );
		DestroyWeapon( pWeapon );

		// Assign the weapon
		GiveWeaponFromID( currentWeaponID );
	}

	// Tell game rules to recalculate the current highest gun game index
	CSGameRules()->CalculateMaxGunGameProgressiveWeaponIndex();
}

void CCSPlayer::IncrementGunGameProgressiveWeapon( int nNumLevelsToIncrease )
{
	int newWeaponIndex = m_iGunGameProgressiveWeaponIndex + nNumLevelsToIncrease - 1;

	if ( newWeaponIndex >= CSGameRules()->GetNumProgressiveGunGameWeapons( GetTeamNumber() ) - 1 )
	{
		// Clamp weapon index to 2nd to last index
		newWeaponIndex = CSGameRules()->GetNumProgressiveGunGameWeapons( GetTeamNumber() ) - 2;
	}

	if ( newWeaponIndex >= m_iGunGameProgressiveWeaponIndex )
	{
		// Bump up the player's weapon level to the new weapon
		m_iGunGameProgressiveWeaponIndex = newWeaponIndex;

		GiveNextProgressiveGunGameWeapon();
	}
}

void CCSPlayer::GiveNextProgressiveGunGameWeapon( void )
{
	int nextWeaponID = CSGameRules()->GetNextGunGameWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() );

	if ( nextWeaponID != -1 )
	{
		// Assign the next weapon to use
		m_iGunGameProgressiveWeaponIndex++;

		// Drop the current pistol
		CBaseCombatWeapon *pWeapon;
		
		pWeapon = Weapon_GetSlot( WEAPON_SLOT_PISTOL );
		DestroyWeapon( pWeapon );

		// Drop the current rifle
		pWeapon = Weapon_GetSlot( WEAPON_SLOT_RIFLE );
		DestroyWeapon( pWeapon );

		if ( nextWeaponID == WEAPON_KNIFE || nextWeaponID == WEAPON_KNIFE_GG )
		{
			// Drop the knife so that when we re-give it it will be primary
			pWeapon = Weapon_GetSlot( WEAPON_SLOT_KNIFE );
			DestroyWeapon( pWeapon );
		}

		// Assign the new weapon
		GiveWeaponFromID( nextWeaponID );

		// Send a game event for leveling up
		if ( CSGameRules()->IsPlayingGunGameTRBomb() )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "ggtr_player_levelup" );
			if ( event )
			{
				const char* szName = WeaponIdAsString( static_cast<CSWeaponID>(nextWeaponID ) );

				event->SetInt( "userid", GetUserID() );
				event->SetInt( "weaponrank", m_iGunGameProgressiveWeaponIndex );
				event->SetString( "weaponname", szName );

				gameeventmanager->FireEvent( event );
			}
		}
		else if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "ggprogressive_player_levelup" );
			if ( event )
			{
				const char* szName = WeaponIdAsString( static_cast<CSWeaponID>(nextWeaponID ) );

				event->SetInt( "userid", GetUserID() );
				event->SetInt( "weaponrank", m_iGunGameProgressiveWeaponIndex );
				event->SetString( "weaponname", szName );

				gameeventmanager->FireEvent( event );
			}
		}

		UpdateLeader();

		// Tell game rules to recalculate the current highest gun game index
		CSGameRules()->CalculateMaxGunGameProgressiveWeaponIndex();
	}
}

void CCSPlayer::SubtractProgressiveWeaponIndex( void )
{
	int previousWeaponID = CSGameRules()->GetPreviousGunGameWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() );

	if ( previousWeaponID != -1 )
	{
		// Assign the previous weapon to use
		m_iGunGameProgressiveWeaponIndex--;
		// clear the number of kills with the current weapon
		m_iNumGunGameKillsWithCurrentWeapon = 0;

		// Destroy the gold knife if we are on that level so we get the regular knife when we spawn
		CBaseCombatWeapon *pWeapon = Weapon_GetSlot( WEAPON_SLOT_KNIFE );
		if ( pWeapon && pWeapon->GetWeaponID() == WEAPON_KNIFE_GG )
		{
			DestroyWeapon( pWeapon );
		}
	}
}

void CCSPlayer::GiveWeaponFromID ( int nWeaponID )
{
	const char *pchClassName = NULL;

	const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinition( nWeaponID );
	if ( pDef && pDef->GetDefinitionIndex() != 0 )
	{
		pchClassName = pDef->GetDefinitionName();
	}
	else
	{
		pchClassName = WeaponIdAsString( (CSWeaponID)nWeaponID );
	}

	if ( !pchClassName )
		return;

	GiveNamedItem( pchClassName );
}

void CCSPlayer::DeathSound( const CTakeDamageInfo &info )
{
	if( m_LastHitGroup == HITGROUP_HEAD )	
	{
		EmitSound( "Player.DeathHeadShot" );
	}
	else
	{
		EmitSound( "Player.Death" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayer::InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity )
{
	if ( !cs_enable_player_physics_box.GetBool() )
	{
		VPhysicsDestroyObject();
		return;
	}
	BaseClass::InitVCollision( vecAbsOrigin, vecAbsVelocity );

	if ( sv_turbophysics.GetBool() )
		return;
	
	// Setup the HL2 specific callback.
	GetPhysicsController()->SetEventHandler( &playerCallback );
}

void CCSPlayer::VPhysicsShadowUpdate( IPhysicsObject *pPhysics )
{
	if ( !CanMove() )
		return;

	BaseClass::VPhysicsShadowUpdate( pPhysics );
}

bool CCSPlayer::HasShield() const
{
#ifdef CS_SHIELD_ENABLED
	return m_bHasShield;
#else
	return false;
#endif
}


bool CCSPlayer::IsShieldDrawn() const
{
#ifdef CS_SHIELD_ENABLED
	return m_bShieldDrawn;
#else
	return false;
#endif
}


void CCSPlayer::SprayPaint( CCSUsrMsg_PlayerDecalDigitalSignature const &msg )
{
	if ( !m_uiAccountId ) return;
	if ( !CSGameRules() ) return;
	if ( !msg.has_data() ) return;
	if ( !msg.data().trace_id() ) return;

	if ( !msg.data().endpos().size() )
	{
		//
		// This is user's initial request for traces to be performed
		//

		if ( gpGlobals->curtime < m_flNextDecalTime ) return;
		PushAwayDecalPaintingTime( 0.2f );	// prevent trace spamming

		if ( int( msg.data().equipslot() ) < 0 ) return;
		if ( msg.data().equipslot() >= Q_ARRAYSIZE( m_unEquippedPlayerSprayIDs ) ) return;
		if ( !m_unEquippedPlayerSprayIDs[ msg.data().equipslot() ] ) return;

		// relax the server-side check, clients should just have a spray equipped and client message will
		// be authoritative about spraying
		// if ( m_unEquippedPlayerSprayIDs[ msg.data().equipslot() ] != msg.data().tx_defidx() ) return;
		if ( fabs( gpGlobals->curtime - msg.data().creationtime() ) > 10 ) return;

		//
		// Perform all the traces to validate spray application
		//
		trace_t	tr;
		Vector forward, right;
		if ( char const *szError = IsAbleToApplySpray( &tr, &forward, &right ) )
		{
			ClientPrint( this, HUD_PRINTCENTER, szError );
			return;
		}

		// line hit something, so paint a decal
		PushAwayDecalPaintingTime( PLAYERDECALS_COOLDOWN_SECONDS );

		//
		// Ask the user to create a digital signature for this decal before it can be applied in the world
		//
		CCSGameRules::ServerPlayerDecalData_t data;
		data.m_unAccountID = m_uiAccountId;
		data.m_nTraceID = msg.data().trace_id();
		data.m_vecOrigin = tr.endpos;
		data.m_vecRight = right;
		data.m_vecNormal = tr.plane.normal;
		data.m_nEquipSlot = msg.data().equipslot();
		data.m_nPlayer = msg.data().tx_defidx();
		data.m_nEntity = tr.m_pEnt->entindex();
		data.m_nHitbox = tr.hitbox;
		data.m_nTintID = msg.data().tint_id();
		data.m_flCreationTime = gpGlobals->curtime;
		data.m_rtGcTime = msg.data().rtime();

		// Always pretend the player was standing directly in front of the surface they hit.
		// Matches code in QcCreatePreviewDecal, update that if you touch this.
		data.m_vecStart = tr.endpos + tr.plane.normal;

		CSGameRules()->m_arrServerPlayerDecalData.AddToTail( data );

		CSingleUserRecipientFilter filter( this );
		CCSUsrMsg_PlayerDecalDigitalSignature askuser;
		data.CopyToMsg( askuser );
		SendUserMessage( filter, CS_UM_PlayerDecalDigitalSignature, askuser );

		CBroadcastRecipientFilter sprayCanFilter;
		EmitSound( sprayCanFilter, entindex(), "SprayCan.Shake" );	// start playing the spray sound when the server confirms the trace
		// it may take multiple seconds for the GC digital signature to come through from the client, so we'll
		// play another spray sound when we actually apply the spray decal image
	}
	else
	{
		// We should have this user's request in the vector
		CCSGameRules::ServerPlayerDecalData_t data;
		data.m_unAccountID = m_uiAccountId;
		data.InitFromMsg( msg );
		{
			int idx = CSGameRules()->m_arrServerPlayerDecalData.Find( data );
			if ( idx < 0 )
				return;
			CSGameRules()->m_arrServerPlayerDecalData.FastRemove( idx );
		}

		// Verify the signature of the user's spray paint
#ifdef _DEBUG
		{
			float flendpos[3] = { msg.data().endpos( 0 ), msg.data().endpos( 1 ), msg.data().endpos( 2 ) };
			float flstartpos[3] = { msg.data().startpos( 0 ), msg.data().startpos( 1 ), msg.data().startpos( 2 ) };
			float flright[3] = { msg.data().right( 0 ), msg.data().right( 1 ), msg.data().right( 2 ) };
			float flnormal[3] = { msg.data().normal( 0 ), msg.data().normal( 1 ), msg.data().normal( 2 ) };
			DevMsg( "Server signature #%u e(%08X,%08X,%08X) s(%08X,%08X,%08X) r(%08X,%08X,%08X) n(%08X,%08X,%08X)\n", data.m_nTraceID,
				*reinterpret_cast< uint32 * >( &flendpos[0] ), *reinterpret_cast< uint32 * >( &flendpos[1] ), *reinterpret_cast< uint32 * >( &flendpos[2] ),
				*reinterpret_cast< uint32 * >( &flstartpos[0] ), *reinterpret_cast< uint32 * >( &flstartpos[1] ), *reinterpret_cast< uint32 * >( &flstartpos[2] ),
				*reinterpret_cast< uint32 * >( &flright[0] ), *reinterpret_cast< uint32 * >( &flright[1] ), *reinterpret_cast< uint32 * >( &flright[2] ),
				*reinterpret_cast< uint32 * >( &flnormal[0] ), *reinterpret_cast< uint32 * >( &flnormal[1] ), *reinterpret_cast< uint32 * >( &flnormal[2] )
				);
		}
#endif
		if ( !BValidateClientPlayerDecalSignature( msg.data() ) )
			return;

		// Follow through and apply the decal
		CBroadcastRecipientFilter sprayCanFilter;
		EmitSound( sprayCanFilter, entindex(), "SprayCan.Paint" );
		extern void FE_PlayerDecal( CCSGameRules::ServerPlayerDecalData_t const &data, std::string const &signature );
		FE_PlayerDecal( data, msg.data().signature() );
	}

	// Expire any digital signature requests that are too old
	CUtlVector< CCSGameRules::ServerPlayerDecalData_t > &arrData = CSGameRules()->m_arrServerPlayerDecalData;
	FOR_EACH_VEC( arrData, i )
	{	// everything that is 5+ minutes old will never come back
		if ( arrData[ i ].m_flCreationTime + 5 * 60 < gpGlobals->curtime )
			arrData.FastRemove( i-- );
	}

	//
	// old-style via TempEnts system
	// UTIL_PlayerDecalTrace( &tr, right, ( ( entindex() & 0x7F ) << 24 ) | uint32( unEquippedPlayerSprayID ) );
	// must be removed!
}

void CCSGameRules::ServerPlayerDecalData_t::CopyToMsg( CCSUsrMsg_PlayerDecalDigitalSignature &msg ) const
{
	PlayerDecalDigitalSignature &ddata = *msg.mutable_data();
	ddata.set_accountid( m_unAccountID );
	ddata.set_trace_id( m_nTraceID );
	ddata.set_rtime( m_rtGcTime );
	for ( int i = 0; i < 3; ++ i ) ddata.add_endpos( m_vecOrigin[i] );
	for ( int i = 0; i < 3; ++ i ) ddata.add_startpos( m_vecStart[i] );
	for ( int i = 0; i < 3; ++ i ) ddata.add_right( m_vecRight[i] );
	for ( int i = 0; i < 3; ++ i ) ddata.add_normal( m_vecNormal[i] );
	ddata.set_equipslot( m_nEquipSlot );
	ddata.set_tx_defidx( m_nPlayer );
	ddata.set_entindex( m_nEntity );
	ddata.set_hitbox( m_nHitbox );
	ddata.set_tint_id( m_nTintID );
	ddata.set_creationtime( m_flCreationTime );
}

void CCSGameRules::ServerPlayerDecalData_t::InitFromMsg( CCSUsrMsg_PlayerDecalDigitalSignature const &msg )
{
	PlayerDecalDigitalSignature const &ddata = msg.data();
	m_unAccountID = ddata.accountid();
	m_nTraceID = ddata.trace_id();
	m_rtGcTime = ddata.rtime();
	if ( ddata.endpos_size() == 3 ) m_vecOrigin.Init( ddata.endpos( 0 ), ddata.endpos( 1 ), ddata.endpos( 2 ) );
	if ( ddata.startpos_size() == 3 ) m_vecStart.Init( ddata.startpos( 0 ), ddata.startpos( 1 ), ddata.startpos( 2 ) );
	if ( ddata.right_size() == 3 ) m_vecRight.Init( ddata.right( 0 ), ddata.right( 1 ), ddata.right( 2 ) );
	if ( ddata.normal_size() == 3 ) m_vecNormal.Init( ddata.normal( 0 ), ddata.normal( 1 ), ddata.normal( 2 ) );
	m_nEquipSlot = ddata.equipslot();
	m_nPlayer = ddata.tx_defidx();
	m_nEntity = ddata.entindex();
	m_nHitbox = ddata.hitbox();
	m_nTintID = ddata.tint_id();
	m_flCreationTime = ddata.creationtime();
}

void CCSPlayer::ImpulseCommands()
{
	int iImpulse = ( int ) m_nImpulse;
	switch ( iImpulse )
	{
#if !CSTRIKE_BETA_BUILD || defined ( _DEBUG )
	case 111:	// CS:GO has 4 sprays
	case 112:
	case 113:
	case 114:
		// If this player doesn't have a spraycan equipped then swallow the spray request (this probably doesn't work)
		if ( sv_cheats->GetBool() )
		{
			CCSUsrMsg_PlayerDecalDigitalSignature msg;
			msg.mutable_data()->set_equipslot( iImpulse - 111 );
			SprayPaint( msg );
		}
		break;
#endif
	}

	// Fall through to generic implementation (and it resets the impulse state)
	BaseClass::ImpulseCommands();
}

void CCSPlayer::CheatImpulseCommands( int iImpulse )
{
	switch( iImpulse )
	{
		case 101:
		{
			if( sv_cheats->GetBool() )
			{
				extern int gEvilImpulse101;
				gEvilImpulse101 = true;

				InitializeAccount( CSGameRules()->GetMaxMoney() );

				for ( int i = 0; i < MAX_WEAPONS; ++i )
				{
					CBaseCombatWeapon *pWeapon = GetWeapon( i );
					if ( pWeapon )
					{
						pWeapon->GiveReserveAmmo( AMMO_POSITION_PRIMARY, 999 );
						pWeapon->GiveReserveAmmo( AMMO_POSITION_SECONDARY, 999 );
					}
				}

				gEvilImpulse101 = false;
			}
		}
		break;

		default:
		{
			BaseClass::CheatImpulseCommands( iImpulse );
		}
	}
}

void CCSPlayer::SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize )
{
	BaseClass::SetupVisibility( pViewEntity, pvs, pvssize );

	int area = pViewEntity ? pViewEntity->NetworkProp()->AreaNum() : NetworkProp()->AreaNum();
	PointCameraSetupVisibility( this, area, pvs, pvssize );
}


bool CCSPlayer::ShouldCheckOcclusion( CBasePlayer *pOtherPlayer )
{
	if ( BaseClass::ShouldCheckOcclusion( pOtherPlayer ) )
		return true;

	int nMyTeam = GetTeamNumber(), nOtherTeam = pOtherPlayer->GetTeamNumber();
	if ( nMyTeam <= TEAM_SPECTATOR || nOtherTeam <= TEAM_SPECTATOR )
		return false; // no need to check occlusion for spectators (or for active player looking at spectators), unless we really want to reduce network traffic a little bit

	Assert( nMyTeam == TEAM_CT || nMyTeam == TEAM_TERRORIST );
	Assert( nOtherTeam == TEAM_CT || nOtherTeam == TEAM_TERRORIST );

	if ( nMyTeam == nOtherTeam )
		return false; // no need to check occlusion between teammates

	if ( !IsAlive() || !pOtherPlayer->IsAlive() )
		return false; // no need to check occlusion when one of the players is dead

	Assert (! static_cast< CCSPlayer* >( pOtherPlayer )->m_isCurrentGunGameTeamLeader ); // we should not even try to check occlusion for a gun game team leader;  CServerGameEnts::CheckTransmit() shouldn't even call us here

	return true;
}

bool CCSPlayer::IsValidObserverTarget(CBaseEntity * target )
{
	if ( target == NULL )
		return false;
		
	if ( !target->IsPlayer() )
	{
		// [jason] If the target is planted C4, we allow that to be observed as well
		CPlantedC4* pPlantedC4 = dynamic_cast< CPlantedC4* >(target );
		if ( pPlantedC4 )
			return true;

		// allow spectating grenades
		CBaseCSGrenadeProjectile* pGrenade = dynamic_cast< CBaseCSGrenadeProjectile* >( target );
		if ( pGrenade )
			return true;

		return false;
	}

	// fall through to the base checks
	return BaseClass::IsValidObserverTarget( target );
}

CBaseEntity* CCSPlayer::FindNextObserverTarget( bool bReverse )
{
	CBaseEntity* pTarget = BaseClass::FindNextObserverTarget( bReverse );

	// [jason] If we have no valid targets left (eg. last teammate dies in competitive mode )
	//	then try to place the camera near any planted bomb 
	if ( !pTarget )
	{		
		if ( g_PlantedC4s.Count() > 0 )
		{
			// Immediately change the observer target, so we can handle SetObserverMode appropriately
			SetObserverTarget( g_PlantedC4s[0] );

			// [mbooth] free roaming spectator is useful for testing
			if ( !spec_allow_roaming.GetBool() || GetObserverMode() != OBS_MODE_ROAMING )
			{
				// Allow the camera to pivot
				SetObserverMode( OBS_MODE_CHASE );
			}

			return g_PlantedC4s[0];
		}
	}

	return pTarget;
}

void CCSPlayer::UpdateAddonBits()
{
	SNPROF( "CCSPlayer::UpdateAddonBits" );

	int iNewBits = 0;
	
	//it's ok to show the active weapon as a holstered weapon if it's not yet visible (still deploying)
	CBaseCombatWeapon *pActiveWeapon = GetActiveWeapon();
	bool bActiveWeaponIsVisible = true;
	if ( pActiveWeapon && pActiveWeapon->GetWeaponWorldModel() )
	{
		bActiveWeaponIsVisible = !pActiveWeapon->GetWeaponWorldModel()->IsEffectActive( EF_NODRAW );
	}

	int nFlashbang = GetAmmoCount( GetAmmoDef()->Index( AMMO_TYPE_FLASHBANG ) );
	if ( dynamic_cast< CFlashbang* >( GetActiveWeapon() ) && bActiveWeaponIsVisible )
	{
		--nFlashbang;
	}
	
	if ( nFlashbang >= 1 )
		iNewBits |= ADDON_FLASHBANG_1;
	
	if ( nFlashbang >= 2 )
		iNewBits |= ADDON_FLASHBANG_2;

	if ( GetAmmoCount( GetAmmoDef()->Index( AMMO_TYPE_HEGRENADE ) ) &&
		( !dynamic_cast< CHEGrenade* >( GetActiveWeapon() ) || !bActiveWeaponIsVisible ) )
	{
		iNewBits |= ADDON_HE_GRENADE;
	}

	if ( GetAmmoCount( GetAmmoDef()->Index( AMMO_TYPE_SMOKEGRENADE ) ) && 
		( !dynamic_cast< CSmokeGrenade* >( GetActiveWeapon() ) || !bActiveWeaponIsVisible ) )
	{
		iNewBits |= ADDON_SMOKE_GRENADE;
	}

	// [mlowrance] Molotov purposely left out

	if ( GetAmmoCount( GetAmmoDef()->Index( AMMO_TYPE_DECOY ) ) && 
		( !dynamic_cast< CDecoyGrenade* >( GetActiveWeapon() ) || !bActiveWeaponIsVisible ) )
	{
		iNewBits |= ADDON_DECOY;
	}

	if ( GetAmmoCount( GetAmmoDef()->Index( AMMO_TYPE_TAGRENADE ) ) &&
		 ( !dynamic_cast< CSensorGrenade* >( GetActiveWeapon() ) || !bActiveWeaponIsVisible ) )
	{
		iNewBits |= ADDON_TAGRENADE;
	}

	if ( HasC4() && ( !dynamic_cast< CC4* >( GetActiveWeapon() ) || !bActiveWeaponIsVisible ) )
		iNewBits |= ADDON_C4;

	if ( HasDefuser() )
		iNewBits |= ADDON_DEFUSEKIT;

	CWeaponCSBase *weapon = dynamic_cast< CWeaponCSBase * >(Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );
	if ( weapon && ( weapon != GetActiveWeapon() || !bActiveWeaponIsVisible ) )
	{
		iNewBits |= ADDON_PRIMARY;
		m_iPrimaryAddon = weapon->GetCSWeaponID();
	}
	else
	{
		m_iPrimaryAddon = WEAPON_NONE;
	}

	weapon = dynamic_cast< CWeaponCSBase * >(Weapon_GetSlot( WEAPON_SLOT_PISTOL ) );
	if ( weapon && ( weapon != GetActiveWeapon() || !bActiveWeaponIsVisible ) )
	{
		iNewBits |= ADDON_PISTOL;
		if ( weapon->GetCSWeaponID() == WEAPON_ELITE )
		{
			iNewBits |= ADDON_PISTOL2;
		}
		m_iSecondaryAddon = weapon->GetCSWeaponID();
	}
	else if ( weapon && weapon->GetCSWeaponID() == WEAPON_ELITE )
	{
		// The active weapon is weapon_elite.  Set ADDON_PISTOL2 without ADDON_PISTOL, so we know
		// to display the empty holster.
		iNewBits |= ADDON_PISTOL2;
		m_iSecondaryAddon = weapon->GetCSWeaponID();
	}
	else
	{
		m_iSecondaryAddon = WEAPON_NONE;
	}

	CWeaponCSBase *knife = dynamic_cast< CWeaponCSBase * >(Weapon_GetSlot( WEAPON_SLOT_KNIFE ) );
	if ( knife && ( knife != GetActiveWeapon() || !bActiveWeaponIsVisible ) )
	{
		iNewBits |= ADDON_KNIFE;
	}

	m_iAddonBits = iNewBits;
}


void CCSPlayer::AppendSpottedEntityUpdateMessage( int entindex, bool bSpotted, 
	CCSUsrMsg_ProcessSpottedEntityUpdate::SpottedEntityUpdate *pMsg )
{
	CBaseEntity * pEntity = UTIL_EntityByIndex( entindex );

	if ( pEntity )
	{
		pMsg->set_entity_idx( entindex );

		// Entity may not yet exist on client so we need to pass the class ID
		pMsg->set_class_id( pEntity->GetServerClass()->m_ClassID );

		// generic entity data
		// write out position
		pMsg->set_origin_x( pEntity->GetAbsOrigin().x/4 );
		pMsg->set_origin_y( pEntity->GetAbsOrigin().y/4 );
		pMsg->set_origin_z( pEntity->GetAbsOrigin().z/4 );
		pMsg->set_angle_y(  AngleNormalize( pEntity->GetAbsAngles().y ) );
		
		// Clients are are unaware of the defuser class, so we need to flag defuse entities manually
		pMsg->set_defuser( FClassnameIs( pEntity, "item_defuser" ) || FClassnameIs( pEntity, "item_cutters" ) );

		// class specific data first
		CCSPlayer * pPlayer = ToCSPlayer( UTIL_PlayerByIndex( entindex ) );
		if ( pPlayer )
		{
			pMsg->set_player_has_defuser( pPlayer->HasDefuser() );	// has defuser
			pMsg->set_player_has_c4( pPlayer->HasC4() );			// has bomb
		}
	}
}


void CCSPlayer::ProcessSpottedEntityUpdate()
{
	if ( gpGlobals->curtime < m_fNextRadarUpdateTime )
		return;

	m_fNextRadarUpdateTime = gpGlobals->curtime + SPOTTED_ENTITY_UPDATE_INTERVAL + RandomFloat( 0.f, SPOTTED_ENTITY_UPDATE_INTERVAL );

	// Determine which entities are outside of PVS and test their spot state
	GatherNonPVSSpottedEntitiesFunctor NonPVSSpottable( this );
	ForEachEntity( NonPVSSpottable );

	const CBitVec<MAX_EDICTS> & entities = NonPVSSpottable.GetSpotted( );

	int nIndex = entities.FindNextSetBit( 0 );
	
	CSingleUserRecipientFilter user( this );
	CCSUsrMsg_ProcessSpottedEntityUpdate msg;

	msg.set_new_update( true ); // Start of a new update frame. Signals the client to reset its spotting data.

	int nMessageEntityCount = 0;	// The number of entities updated in this message
	
	while ( nIndex > -1 )
	{
		if ( this->entindex() != nIndex )
		{
			if ( nMessageEntityCount >= SPOTTED_ENTITY_COUNT_MESSAGE_MAX )
			{
				// We do not have enough space for this entity. Start a new message.			
				SendUserMessage( user, CS_UM_ProcessSpottedEntityUpdate, msg );
				msg.Clear();
				msg.set_new_update( false ); // Start of a partial update. Clients do not need to clear their spotting data.
				nMessageEntityCount = 0;
				continue;
			}

			AppendSpottedEntityUpdateMessage( nIndex, true, msg.add_entity_updates() );
			nMessageEntityCount++;
		}

		nIndex = entities.FindNextSetBit( nIndex + 1 );
	}

	SendUserMessage( user, CS_UM_ProcessSpottedEntityUpdate, msg );
}

void CCSPlayer::UpdateMouseoverHints()
{
	return;

	// these traces and checks existed to pop up hints telling the player what to do when they saw a friend or enemy
	// all of these hints are now handled by the game instructor and have time-outs, success limits, etc and this is no longer needed
	/*
	if ( IsBlind() || IsObserver() )
		return;

	// Exit out if hint has already been displayed or is not applicable
	if ( !CSGameRules()->IsHostageRescueMap() || m_iDisplayHistoryBits & DHF_HOSTAGE_SEEN_FAR )
	{
		return;
	}

	Vector forward, up;
	EyeVectors( &forward, NULL, &up );

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable )
	Vector searchStart = EyePosition();
	Vector searchEnd = searchStart + forward * 2048;

	int useableContents = MASK_NPCSOLID_BRUSHONLY | MASK_VISIBLE_AND_NPCS;

	UTIL_TraceLine( searchStart, searchEnd, useableContents, this, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction != 1.0f )
	{
		if (tr.DidHitNonWorldEntity() && tr.m_pEnt )
		{
			CBaseEntity *pObject = tr.m_pEnt;
			switch ( pObject->Classify() )
			{
	
			case CLASS_PLAYER:
				{
					const float grenadeBloat = 1.2f; // Be conservative in estimating what a player can distinguish
					if ( !TheBots->IsLineBlockedBySmoke( EyePosition(), pObject->EyePosition(), grenadeBloat ) )
					{
						if ( g_pGameRules->PlayerRelationship( this, pObject ) == GR_TEAMMATE )
						{
							if ( !(m_iDisplayHistoryBits & DHF_FRIEND_SEEN ) )
							{
								m_iDisplayHistoryBits |= DHF_FRIEND_SEEN;
								HintMessage( "#Hint_spotted_a_friend", true );
							}
						}
						else
						{
							if ( !(m_iDisplayHistoryBits & DHF_ENEMY_SEEN ) )
							{
								m_iDisplayHistoryBits |= DHF_ENEMY_SEEN;
								HintMessage( "#Hint_spotted_an_enemy", true );
							}
						}
					}
				}
				break;
			case CLASS_PLAYER_ALLY:
				switch ( GetTeamNumber() )
				{
				case TEAM_CT:
					if ( !(m_iDisplayHistoryBits & DHF_HOSTAGE_SEEN_FAR ) && tr.fraction > 0.1f )
					{
						m_iDisplayHistoryBits |= DHF_HOSTAGE_SEEN_FAR;
						HintMessage( "#Hint_rescue_the_hostages", true );
					}
					else if ( !(m_iDisplayHistoryBits & DHF_HOSTAGE_SEEN_NEAR ) && tr.fraction <= 0.1f )
					{
						m_iDisplayHistoryBits |= DHF_HOSTAGE_SEEN_FAR;
						m_iDisplayHistoryBits |= DHF_HOSTAGE_SEEN_NEAR;
					}
					break;
				case TEAM_TERRORIST:
					if ( !(m_iDisplayHistoryBits & DHF_HOSTAGE_SEEN_FAR ) )
					{
						m_iDisplayHistoryBits |= DHF_HOSTAGE_SEEN_FAR;
						HintMessage( "#Hint_prevent_hostage_rescue", true );
					}
					break;
				}
				break;
			}
		
		}
	}
	*/
}

void CCSPlayer::PostThink()
{
	BaseClass::PostThink();

	// if we're spawning, clear it
	if ( m_bIsSpawning )
		m_bIsSpawning = false;

	if ( IsTaunting() )
	{
		if ( gpGlobals->curtime >= m_flTauntEndTime )
		{
			StopTaunting();
		}
		else if ( IsThirdPersonTaunt() && ( GetGroundEntity() == NULL && GetWaterLevel() == WL_NotInWater ) )
		{
			StopTaunting();
		}
	}

	if ( IsLookingAtWeapon() )
	{
		if ( gpGlobals->curtime >= m_flLookWeaponEndTime )
		{
			StopLookingAtWeapon();
		}
	}


	// failsafe to show active world model if it fails to unhide by the time deploy is complete
	CWeaponCSBase *pCSWeapon = GetActiveCSWeapon();
	if ( pCSWeapon && pCSWeapon->GetActivity() != pCSWeapon->GetDeployActivity() )
	{
		pCSWeapon->ShowWeaponWorldModel( true );
	}


	UpdateAddonBits();

	ProcessSpottedEntityUpdate();

	//UpdateTeamMoney();

	if ( !(m_iDisplayHistoryBits & DHF_ROUND_STARTED ) && CanPlayerBuy(false ) )
	{
		m_iDisplayHistoryBits |= DHF_ROUND_STARTED;
	}
	if ( m_flNextMouseoverUpdate < gpGlobals->curtime )
	{
		m_flNextMouseoverUpdate = gpGlobals->curtime + 0.2f;
		if ( m_bShowHints )
		{
			UpdateMouseoverHints();
		}
	}
#if !defined( CSTRIKE15 )
	if ( GetActiveWeapon() && !(m_iDisplayHistoryBits & DHF_AMMO_EXHAUSTED ) )
	{
		// The "out of ammo" prompt shouldn't display in GunGame or training, because there are no buy-zones. <-- why doesn't this just check for buyzones in the map?  -mtw
		if ( !CSGameRules()->IsPlayingGunGame() && !CSGameRules()->IsPlayingTraining() )
		{
			CBaseCombatWeapon *pWeapon = GetActiveWeapon();
			if ( !pWeapon->HasAnyAmmo() && !(pWeapon->GetWpnData().iFlags & ITEM_FLAG_EXHAUSTIBLE ) )
			{
				m_iDisplayHistoryBits |= DHF_AMMO_EXHAUSTED;
				HintMessage( "#Hint_out_of_ammo", false );
			}
		}
	}
#endif

	if ( !m_bWasInBuyZone && IsInBuyZone() && IsAlive() )
	{
		// we entered a buyzone
		m_bWasInBuyZone = true;
		IGameEvent * event = gameeventmanager->CreateEvent( "enter_buyzone" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetBool( "canbuy", CanPlayerBuy( false ) );
			gameeventmanager->FireEvent( event );
		}
	}
	else if ( m_bWasInBuyZone && !IsInBuyZone() && IsAlive() )
	{
		// we exited a buy zone
		m_bWasInBuyZone = false;
		IGameEvent * event = gameeventmanager->CreateEvent( "exit_buyzone" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetBool( "canbuy", CanPlayerBuy( false ) );
			gameeventmanager->FireEvent( event );
		}
	}

	if ( !m_bWasInBombZoneTrigger && m_bInBombZoneTrigger )
	{
		// we entered a bomb zone
		m_bWasInBombZoneTrigger = true;
		IGameEvent * event = gameeventmanager->CreateEvent( "enter_bombzone" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetBool( "hasbomb", HasC4() );
			event->SetBool( "isplanted", CSGameRules()->m_bBombPlanted );
			gameeventmanager->FireEvent( event );
		}
	}
	else if ( m_bWasInBombZoneTrigger && !m_bInBombZoneTrigger )
	{
		// we exited a bomb zone
		m_bWasInBombZoneTrigger = false;
		IGameEvent * event = gameeventmanager->CreateEvent( "exit_bombzone" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetBool( "hasbomb", HasC4() );
			event->SetBool( "isplanted", CSGameRules()->m_bBombPlanted );
			gameeventmanager->FireEvent( event );
		}
	}

	if ( !m_bWasInHostageRescueZone && m_bInHostageRescueZone )
	{
		// we entered a hostage rescue zone
		m_bWasInHostageRescueZone = true;
		IGameEvent * event = gameeventmanager->CreateEvent( "enter_rescue_zone" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			gameeventmanager->FireEvent( event );
		}
	}
	else if ( m_bWasInHostageRescueZone && !m_bInHostageRescueZone )
	{
		// we exited a hostage rescue zone
		m_bWasInHostageRescueZone = false;
		IGameEvent * event = gameeventmanager->CreateEvent( "exit_rescue_zone" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			gameeventmanager->FireEvent( event );
		}
	}



#if 0
	if ( m_iNumFollowers > 0 && IsAlive() )
	{
		ShortestPathCost cost = ShortestPathCost();
		float dist = NavAreaTravelDistance( this->GetLastKnownArea(),
						TheNavMesh->GetNearestNavArea( TheCSBots()->GetZone(0)->m_center ),
						cost );

		if( dist < cs_hostage_near_rescue_music_distance.GetFloat() )
		{
			CBroadcastRecipientFilter filter;
			PlayMusicSelection( filter, "Music.HostageNearRescue" );
			//DevMsg("***DISTANCE TO RESCUE: %f: FOLLOWERS: %i\n", dist, m_iNumFollowers );
		}
	}
#endif
	
	// Store the eye angles pitch so the client can compute its animation state correctly.
	QAngle eyeAngles = EyeAngles();
	Vector &angEyeAngles = m_angEyeAngles.GetForModify();
	angEyeAngles.x = eyeAngles.x;
	angEyeAngles.y = eyeAngles.y;
	angEyeAngles.z = eyeAngles.z;

	m_flThirdpersonRecoil = GetAimPunchAngle()[PITCH];

	m_bUseNewAnimstate ? m_PlayerAnimStateCSGO->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] ) : m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	// check if we need to apply a deafness DSP effect.
	if ((m_applyDeafnessTime != 0.0f ) && (m_applyDeafnessTime <= gpGlobals->curtime ))
	{
		ApplyDeafnessEffect();
	}

	if ( IsPlayerUnderwater() && GetWaterLevel() < WL_Eyes )
	{
		StopSound( "Player.AmbientUnderWater" );
		SetPlayerUnderwater( false );
	}

	if( !m_bUseNewAnimstate && IsAlive() && m_cycleLatchTimer.IsElapsed() )
	{
		m_cycleLatchTimer.Start( CycleLatchInterval );

		// Cycle is a float from 0 to 1.  We don't need to transmit a whole float for that.  Compress it in to a small fixed point
		m_cycleLatch.GetForModify() = 16 * GetCycle();// 4 point fixed
	}

	// if player is not blind, set flash duration to default
	if ( m_flFlashDuration > 0.000001f && !IsBlind() )
	{
		m_flFlashDuration = 0.0f;
	}

	// inactive player drops the bomb after a certain duration (afk)
	if ( !m_bHasMovedSinceSpawn && CSGameRules()->GetRoundElapsedTime() > sv_spawn_afk_bomb_drop_time.GetFloat() 
		 && !CSGameRules()->IsPlayingCoopMission() )
	{
		// Drop the C4
		CBaseCombatWeapon *pC4 = Weapon_OwnsThisType( "weapon_c4" );
		if ( pC4 )
		{
			SetBombDroppedTime( gpGlobals->curtime );
			CSWeaponDrop( pC4, false, false );
			
			//odd that the AFK player 'says' they have dropped the bomb... but it's better than nothing
			Radio( "SpottedLooseBomb",   "#Cstrike_TitlesTXT_Game_afk_bomb_drop" );
		}
	}

	if ( m_bNeedToUpdateCoinFromInventory )
	{
		m_bNeedToUpdateCoinFromInventory = false;
		
		bool bHasCoin = false;
		if ( CEconItemView const *pItemViewCoin = Inventory()->GetItemInLoadout( 0, LOADOUT_POSITION_FLAIR0 ) )
		{
			if ( const GameItemDefinition_t *pDefItem = pItemViewCoin->GetItemDefinition() )
			{
				bHasCoin = true;
				SetRank( MEDAL_CATEGORY_SEASON_COIN, MedalRank_t( pDefItem->GetDefinitionIndex() ) );
			}
		}
		if ( !bHasCoin )
		{
			SetRank( MEDAL_CATEGORY_SEASON_COIN, MedalRank_t( 0 ) );
		}
	}

	if ( m_bNeedToUpdateMusicFromInventory )
	{
		m_bNeedToUpdateMusicFromInventory = false;
		
		bool bHasMusic = false;
		if ( CEconItemView const *pItemViewMusic = Inventory()->GetItemInLoadout( 0, LOADOUT_POSITION_MUSICKIT ) )
		{
			static const CEconItemAttributeDefinition *pAttr_MusicID = GetItemSchema()->GetAttributeDefinitionByName( "music id" );
			uint32 unMusicID;

			if ( pItemViewMusic && pItemViewMusic->IsValid() && pItemViewMusic->FindAttribute( pAttr_MusicID, &unMusicID ) )
			{
				SetMusicID( unMusicID );
				bHasMusic = true;
			}
		}
		if ( !bHasMusic )
		{
			SetMusicID( CSMUSIC_NOPACK );
		}
	}

	if ( m_bNeedToUpdatePlayerSprayFromInventory )
	{
		m_bNeedToUpdatePlayerSprayFromInventory = false;
		COMPILE_TIME_ASSERT( ARRAYSIZE( m_unEquippedPlayerSprayIDs ) == ( LOADOUT_POSITION_SPRAY0 /*LOADOUT_POSITION_SPRAY3*/ + 1 - LOADOUT_POSITION_SPRAY0 ) );
		for ( int iSprayLoadoutSlot = LOADOUT_POSITION_SPRAY0; iSprayLoadoutSlot <= LOADOUT_POSITION_SPRAY0 /*LOADOUT_POSITION_SPRAY3*/; ++ iSprayLoadoutSlot )
		{
			bool bHasEquippedPlayerSpray = false;
			if ( CEconItemView const *pItemViewEquipped = Inventory()->GetItemInLoadout( 0, iSprayLoadoutSlot ) )
			{
				if ( pItemViewEquipped && pItemViewEquipped->IsValid() )
				{
					if ( uint32 unStickerKitID = pItemViewEquipped->GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 ) )
					{
						m_unEquippedPlayerSprayIDs[ iSprayLoadoutSlot - LOADOUT_POSITION_SPRAY0 ] = unStickerKitID;
						bHasEquippedPlayerSpray = true;
					}
				}
			}
			if ( !bHasEquippedPlayerSpray )
			{
				m_unEquippedPlayerSprayIDs[ iSprayLoadoutSlot - LOADOUT_POSITION_SPRAY0 ] = 0;
			}
		}
	}

	if ( m_bNeedToUpdatePersonaDataPublicFromInventory )
	{
		m_bNeedToUpdatePersonaDataPublicFromInventory = false;
		if ( CCSPlayerInventory *pInv = Inventory() )
		{
			if ( GCSDK::CGCClientSharedObjectCache *pSOC = pInv->GetSOC() )
			{
				if ( GCSDK::CGCClientSharedObjectTypeCache *pCacheType = pSOC->FindTypeCache( CEconPersonaDataPublic::k_nTypeID ) )
				{
					if ( pCacheType->GetCount() == 1 )
					{
						CEconPersonaDataPublic *pCacheData = ( CEconPersonaDataPublic * ) pCacheType->GetObject( 0 );
						delete m_pPersonaDataPublic;
						if ( pCacheData )
						{
							m_pPersonaDataPublic = new CEconPersonaDataPublic;
							m_pPersonaDataPublic->Obj().CopyFrom( pCacheData->Obj() );
						}
					}
				}
			}
		}
	}

	if ( m_flFlinchStack < 1.0 )
	{
		m_flFlinchStack = Approach( 1.0, m_flFlinchStack, gpGlobals->frametime * 0.35 );//0.65
		//Msg( "m_flFlinchStack = %f\n", m_flFlinchStack );
	}

	m_flFlinchStack = clamp( m_flFlinchStack, 0.1, 1.0 );
	
	int nTeamCheck = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
	//int nOtherTeam = IsHostageRescueMap() ? TEAM_CT : TEAM_TERRORIST;

	if ( CSGameRules()->IsPlayingCoopGuardian() || CSGameRules()->IsPlayingCoopMission() )
	{

		float flDistance = 0;
		float flDistMin = (float)mp_guardian_player_dist_min.GetInt();
		float flDistMax = (float)mp_guardian_player_dist_max.GetInt();
		if ( CSGameRules()->IsPlayingCoopGuardian() && TheCSBots() && TheNavMesh
			 && GetTeamNumber() == nTeamCheck && IsAlive() )
		{
			
			if ( CSGameRules()->IsHostageRescueMap() )
			{
				Vector vecStartSpot = Vector( 0, 0, 0 );
				for ( int iHostage = 0; iHostage < g_Hostages.Count(); iHostage++ )
				{
					CHostage *pHostage = g_Hostages[iHostage];
					if ( pHostage )
					{
						vecStartSpot = ( vecStartSpot + pHostage->GetAbsOrigin() ) / ( iHostage + 1 );
					}
				}

				flDistance = ( GetAbsOrigin() - vecStartSpot ).Length();
			}
			else if ( TheCSBots()->GetTerroristTargetSite() >= 0 )
			{
				const CCSBotManager::Zone *zone = TheCSBots()->GetZone( mp_guardian_target_site.GetInt() );
				if ( zone && zone->m_area[0] )
					flDistance = ( GetAbsOrigin() - zone->m_area[0]->GetCenter() ).Length();
			}
		}

		bool bCanHurt = false;
		int nDamageToGive = 20;

		if ( CSGameRules()->IsPlayingCoopGuardian() )
			bCanHurt = true;

		if ( IsAlive() && flDistMin > 0 )
			m_flGuardianTooFarDistFrac = MAX( 0, ( flDistance - flDistMin ) / ( flDistMax - flDistMin ) );
		else
			m_flGuardianTooFarDistFrac = 0;

		if ( flDistance > flDistMax )
		{
			if ( bCanHurt && m_flNextGuardianTooFarHurtTime <= gpGlobals->curtime )
			{
				Vector vecDamagePos = GetAbsOrigin();

				CWorld *pWorld = GetWorldEntity();
				if ( !pWorld )
					return;

				CTakeDamageInfo info( pWorld, pWorld, nDamageToGive, DMG_GENERIC );
				info.SetDamagePosition( vecDamagePos );
				info.SetDamageForce( vec3_origin );
				TakeDamage( info );

				m_flNextGuardianTooFarHurtTime = gpGlobals->curtime + 0.75;
			}
		}
	}

	if ( m_flDetectedByEnemySensorTime > 0.0f && (gpGlobals->curtime - m_flDetectedByEnemySensorTime) > 5.0f )
	{
		m_flDetectedByEnemySensorTime = 0.0f;
	}
}

void CCSPlayer::PushawayThink()
{
	// Push physics props out of our way.
	PerformObstaclePushaway( this );
	SetNextThink( gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, CS_PUSHAWAY_THINK_CONTEXT );
}


void CCSPlayer::SetModel( const char *szModelName )
{
	m_bUseNewAnimstate = ( Q_stristr( szModelName, "custom_player" ) != 0 );

	if ( m_bUseNewAnimstate )
	{
		if ( m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
			m_PlayerAnimStateCSGO->Reset();

	}

	BaseClass::SetModel( szModelName );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we can switch to the given weapon.
// Input  : pWeapon - 
//-----------------------------------------------------------------------------
bool CCSPlayer::Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon->CanDeploy() )
		return false;
	
	if ( GetActiveWeapon() )
	{
		if ( !GetActiveWeapon()->CanHolster() )
			return false;
	}

	return true;
}

void CCSPlayer::OnSwitchWeapons( CBaseCombatWeapon* pBaseWeapon )
{
	if ( pBaseWeapon )
	{
		CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase* >( pBaseWeapon );

		if ( pWeapon )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "item_equip" );
			if( event )
			{
				const char *weaponName = pWeapon->GetClassname();
				if ( IsWeaponClassname( weaponName ) )
				{
					weaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
				}
				event->SetInt( "userid", GetUserID() );
				event->SetString( "item", weaponName );
				event->SetBool( "canzoom", pWeapon->HasZoom()  );
				event->SetBool( "hassilencer", pWeapon->HasSilencer() );
				event->SetBool( "issilenced", pWeapon->IsSilenced() );
				event->SetBool( "hastracers", (pWeapon->GetCSWpnData().GetTracerFrequency() > 0 ) ? true : false );
				int nType = pWeapon->GetWeaponType();
				event->SetInt( "weptype", (nType == WEAPONTYPE_UNKNOWN ) ? -1 : nType );
				/*	
				WEAPONTYPE_UNKNOWN = -1
				//
				WEAPONTYPE_KNIFE=0,	
				WEAPONTYPE_PISTOL,
				WEAPONTYPE_SUBMACHINEGUN,
				WEAPONTYPE_RIFLE,
				WEAPONTYPE_SHOTGUN,
				WEAPONTYPE_SNIPER_RIFLE,
				WEAPONTYPE_MACHINEGUN,
				WEAPONTYPE_C4,
				WEAPONTYPE_GRENADE,
				*/

				bool bIsPainted = false;
				CEconItemView *pItem = pWeapon->GetEconItemView();
				if (pItem)
				{
					const CPaintKit *pPaintKit = pItem->GetCustomPaintKit();
					if ( pPaintKit && (pPaintKit->nID > 0 ) )
					{
						bIsPainted = true;
					}
				}
				event->SetBool( "ispainted", bIsPainted );

				gameeventmanager->FireEvent( event );
			}

			CSWeaponType weaponType = pWeapon->GetWeaponType();
			CSWeaponID weaponID = static_cast<CSWeaponID>( pWeapon->GetCSWeaponID() );

			if ( weaponType == WEAPONTYPE_GRENADE )
			{	// When switching to grenade remember the preferred grenade
				m_nPreferredGrenadeDrop = weaponID;
			}

			if ( weaponType != WEAPONTYPE_KNIFE && weaponType != WEAPONTYPE_C4 && weaponType != WEAPONTYPE_GRENADE )
			{
				if ( m_WeaponTypesHeld.Find( weaponID ) == -1 )
				{
					// Add this weapon to the list of weapons used by the player
					m_WeaponTypesHeld.AddToTail( weaponID );
				}
			}

			MDLCACHE_CRITICAL_SECTION();
			// Add a deploy event to let the 3rd person animation system know to update to the current weapon and optionally play a deploy animation if it exists.
			if ( (gpGlobals->curtime - pBaseWeapon->m_flLastTimeInAir) < 0.1f )
			{
				// if the weapon was flying through the air VERY recently, assume we 'caught' it and play a catch anim
				DoAnimationEvent( PLAYERANIMEVENT_CATCH_WEAPON );
			}
			else
			{
				DoAnimationEvent( PLAYERANIMEVENT_DEPLOY );
			}
			
		}
	}
}

bool CCSPlayer::ShouldDoLargeFlinch( const CTakeDamageInfo& info, int nHitGroup )
{
	if ( FBitSet( GetFlags(), FL_DUCKING ) )
		return false;

	//CWeaponCSBase *pWeapon = dynamic_cast<CWeaponCSBase*>(info.GetWeapon());
	//if ( pWeapon && pWeapon->GetWeaponType() == WEAPONTYPE_SUBMACHINEGUN )
	//	return true;

	if ( nHitGroup == HITGROUP_LEFTLEG )
		return false;

	if ( nHitGroup == HITGROUP_RIGHTLEG )
		return false;

	return true;
}

bool CCSPlayer::IsArmored( int nHitGroup )
{
	bool bApplyArmor = false;

	if ( ArmorValue() > 0 )
	{
		switch ( nHitGroup )
		{
		case HITGROUP_GENERIC:
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			bApplyArmor = true;
			break;
		case HITGROUP_HEAD:
			if ( m_bHasHelmet )
			{
				bApplyArmor = true;
			}
			break;
		default:
			break;
		}
	}

	return bApplyArmor;
}

void CCSPlayer::Pain( CCSPlayer* pAttacker, bool bHasArmour, int nDmgTypeBits )
{
	if ( nDmgTypeBits & DMG_BURN )
	{
		if ( bHasArmour == false )
		{
			EmitSound( "Player.BurnDamage" );
		}
		else
		{
			EmitSound( "Player.BurnDamageKevlar" );
		}
		return;
	}

	if ( nDmgTypeBits & DMG_CLUB )
	{
		if ( bHasArmour == false )
		{
			EmitSound( "Flesh.BulletImpact" );
		}
		else
		{
			EmitSound( "Player.DamageKevlar" );
		}
		return;
	}

	switch (m_LastHitGroup )
	{
		case HITGROUP_HEAD: {
			//When hit in the head we play a sound for the player who made the headshot to give them
			//feedback. This plays even at a very long range. Other players receive another sound
			//that doesn't carry as far.
			CRecipientFilter filter;
			for (int i = 1; i <= gpGlobals->maxClients; ++i)
			{
				CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
				if (!pPlayer || pPlayer == pAttacker)
				{
					//exclude the player who made the shot.
					continue;
				}

				filter.AddRecipient(pPlayer);
			}

			EmitSound_t params;
			params.m_pSoundName = m_bHasHelmet ? "Player.DamageHelmet" : "Player.DamageHeadShot";
			params.m_flSoundTime = 0.0f;
			params.m_pflSoundDuration = nullptr;
			params.m_bWarnOnDirectWaveReference = true;

			EmitSound(filter, entindex(), params);

			if (pAttacker != nullptr)
			{
				//The player who made the shot gets this 'feedback' version of the sound.
				CRecipientFilter attacker_filter;
				attacker_filter.AddRecipient(pAttacker);

				EmitSound_t attacker_params;
				attacker_params.m_pSoundName = m_bHasHelmet ? "Player.DamageHelmetFeedback" : "Player.DamageHeadShotFeedback";
				attacker_params.m_flSoundTime = 0.0f;
				attacker_params.m_pflSoundDuration = nullptr;
				attacker_params.m_bWarnOnDirectWaveReference = true;

				EmitSound(attacker_filter, entindex(), attacker_params);
			}

			break;
		}
		default:
			if ( bHasArmour == false )
			{
				EmitSound( "Flesh.BulletImpact" );
			}
			else
			{
				EmitSound( "Player.DamageKevlar" );
			}
			break;
	}
}

ConVar mp_tagging_scale( "mp_tagging_scale", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Scalar for player tagging modifier when hit. Lower values for greater tagging." );


class CBombShieldTraceEnum : public IEntityEnumerator
{
public:
	CBombShieldTraceEnum( Ray_t *pRay ) : m_pRay(pRay), m_bHitBombBlocker(false)
	{
	}

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		Assert( pHandleEntity );

		trace_t tr;
		enginetrace->ClipRayToEntity( *m_pRay, MASK_ALL, pHandleEntity, &tr );

		if (( tr.fraction < 1.0f ) || (tr.startsolid) || (tr.allsolid))
		{
			if ( !V_strcmp( tr.surface.name, "TOOLS/TOOLSBLOCKBOMB" ) )
			{
				m_bHitBombBlocker = true;
				return false;
			}
		}

		return true;
	}

	bool HitBombBlocker( void ) { return m_bHitBombBlocker; }

private:
	Ray_t	*m_pRay;
	bool m_bHitBombBlocker;
};


static CUtlVector< CCSPlayer::ITakeDamageListener * > s_arrTakeDamageListeners;
CCSPlayer::ITakeDamageListener::ITakeDamageListener()
{
	s_arrTakeDamageListeners.AddToTail( this );
}
CCSPlayer::ITakeDamageListener::~ITakeDamageListener()
{
	Verify( s_arrTakeDamageListeners.FindAndRemove( this ) );
}

int CCSPlayer::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;
	FOR_EACH_VEC_BACK( s_arrTakeDamageListeners, idxTDL )
	{
		s_arrTakeDamageListeners[idxTDL]->OnTakeDamageListenerCallback( this, info );
	}

	if ( m_bGunGameImmunity )
	{
		// No damage if immune
		return 0;
	}

	CBaseEntity *pInflictor = info.GetInflictor();

	if ( !pInflictor )
		return 0;

	if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
		return 0;

	//if this is C4 bomb damage, make sure it didn't pass through any bomb blockers to reach this player.
	CPlantedC4 *pInflictorC4 = dynamic_cast< CPlantedC4 * >( pInflictor );
	if ( pInflictorC4 )
	{
		Ray_t ray;
		ray.Init( pInflictorC4->GetAbsOrigin(), GetAbsOrigin() );

		CBombShieldTraceEnum bombShieldTrace( &ray );
		enginetrace->EnumerateEntities( ray, true, &bombShieldTrace );

		if ( bombShieldTrace.HitBombBlocker() )
		{
			return 0;
		}
	}

	// Because explosions and fire damage don't do raytracing, but rather lookup entities in volume,
	// we need to set damage hitgroup to generic to make sure previous bullet damage hitgroup is not
	// carried over. Only bullets do raytracing so force it here!
	if ( ( info.GetDamageType() & DMG_BULLET ) == 0 )
		m_LastHitGroup = HITGROUP_GENERIC;

	float flArmorBonus = 0.5f;
	float flArmorRatio = 0.5f;
	float flDamage = info.GetDamage();

	bool bFriendlyFireEnabled = CSGameRules()->IsFriendlyFireOn();

	m_LastDamageType = info.GetDamageType();

	CSGameRules()->PlayerTookDamage(this, info );

	CCSPlayer *pAttacker = ToCSPlayer(info.GetAttacker() );

	// determine some useful info about the source of this damage
	bool bDamageIsFromTeammate = pAttacker && ( pAttacker != this ) && IsOtherSameTeam( pAttacker->GetTeamNumber() ) && !IsOtherEnemy( pAttacker );

	if ( (!bFriendlyFireEnabled && bDamageIsFromTeammate) || ( bDamageIsFromTeammate && CSGameRules()->IsFreezePeriod() ) )
	{
		// when FF is off and that damage is from a teammate (not yourself ) never do damage
		// this FF setting should be consistent and the behavior should match player expectations (no middle ground ) [mtw]
		return 0;
	}

	bool bDamageIsFromSelf = (pAttacker == this );
	bool bDamageIsFromGunfire = !!(info.GetDamageType() & DMG_BULLET ); //  check the damage type [mtw]
	bool bDamageIsFromGrenade = pInflictor && !!(info.GetDamageType() & DMG_BLAST ) && dynamic_cast< CHEGrenadeProjectile* >( pInflictor ) != NULL;
	bool bDamageIsFromFire = !!(info.GetDamageType() & DMG_BURN ); //  check the damage type [mtw]
	bool bDamageIsFromOpponent = pAttacker != NULL && IsOtherEnemy( pAttacker );

	// Check "Goose Chase" achievement
	if ( m_bIsDefusing && ( m_gooseChaseStep == GC_NONE ) && bDamageIsFromOpponent && pAttacker )
	{
		CTeam *pAttackerTeam = GetGlobalTeam( pAttacker->GetTeamNumber() );
		if ( pAttackerTeam )
		{
			// count enemies
			int livingEnemies = 0;

			for ( int iPlayer=0; iPlayer < pAttackerTeam->GetNumPlayers(); iPlayer++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( pAttackerTeam->GetPlayer( iPlayer ) );
				Assert( pPlayer );
				if ( !pPlayer )
					continue;

				Assert( pPlayer->GetTeamNumber() == pAttackerTeam->GetTeamNumber() );

				if ( pPlayer->m_lifeState == LIFE_ALIVE )
				{
					livingEnemies++;
				}
			}

			//Must be last enemy alive;
			if (livingEnemies == 1 )
			{
				m_gooseChaseStep = GC_SHOT_DURING_DEFUSE;
				m_pGooseChaseDistractingPlayer = pAttacker;
			}
		}	
	}

	// score penalty for damage to teammates, regardless of whether it is actually applied
	if ( bDamageIsFromTeammate || bDamageIsFromSelf )
	{
		CSGameRules()->ScoreFriendlyFire( pAttacker, flDamage );
	}

	// warn about team attacks
	// ignoring the FF check so both players are notified that they hit their teammate [mtw]
	// don't do this when a player is hurt by a molotov [mtw]
	if ( bDamageIsFromTeammate && !bDamageIsFromFire && pAttacker )
	{
		if ( !(pAttacker->m_iDisplayHistoryBits & DHF_FRIEND_INJURED ) )
		{
			ClientPrint( pAttacker, HUD_PRINTCENTER, "#Hint_try_not_to_injure_teammates" );
			
			pAttacker->m_iDisplayHistoryBits |= DHF_FRIEND_INJURED;
		}

		// [Jason] Change the constant time interval to be a convar instead (was 1.0f before )
		if ( (pAttacker->m_flLastAttackedTeammate + CS_WarnFriendlyDamageInterval.GetInt() ) < gpGlobals->curtime )
		{
			pAttacker->m_flLastAttackedTeammate = gpGlobals->curtime;

			// tell the rest of this player's team
			for ( int i=1; i<=gpGlobals->maxClients; ++i )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if ( pPlayer && IsOtherSameTeam( pPlayer->GetTeamNumber() ) && !IsOtherEnemy( pPlayer->entindex() ) )
				{
					ClientPrint( pPlayer, HUD_PRINTTALK, "#Cstrike_TitlesTXT_Game_teammate_attack", CFmtStr( "#ENTNAME[%d]%s", pAttacker->entindex(), pAttacker->GetPlayerName() ) );
				}
			}
		}
	}

	float fFriendlyFireDamageReductionRatio = 1.0f;

	// if a player damages them self with a grenade, scale by the convar
	if ( bDamageIsFromSelf && bDamageIsFromGrenade )
	{
		fFriendlyFireDamageReductionRatio = ff_damage_reduction_grenade_self.GetFloat();
	}
	else if ( bDamageIsFromTeammate )
	{
		// reduce all other FF damage per convar settings
		if ( bDamageIsFromGunfire )
		{
			fFriendlyFireDamageReductionRatio = ff_damage_reduction_bullets.GetFloat();
		}
		else if ( bDamageIsFromGrenade )
		{
			fFriendlyFireDamageReductionRatio = ff_damage_reduction_grenade.GetFloat();
		}
		else
		{
			fFriendlyFireDamageReductionRatio = ff_damage_reduction_other.GetFloat();
		}

		if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() )
			fFriendlyFireDamageReductionRatio = 0;

	}

	flDamage *= fFriendlyFireDamageReductionRatio;

	float fHeavyArmorDamageReductionRatio = 0.85f;
	if ( CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() && IsBot() )
		fHeavyArmorDamageReductionRatio = 0.35f;

	if ( HasHeavyArmor() )
		flDamage *= fHeavyArmorDamageReductionRatio;

	// TODO[pmf]: we should be able to replace all this below with pWeapon = info.GetWeapon()
	const CCSWeaponInfo* pFlinchInfoSource = NULL;
	CCSPlayer *pInflictorPlayer = ToCSPlayer( info.GetInflictor() );
	CWeaponCSBase *pInflictorWeapon = NULL;

	if ( pInflictorPlayer )
	{
		pInflictorWeapon = pInflictorPlayer->GetActiveCSWeapon();

		if ( pInflictorWeapon )
		{
			pFlinchInfoSource = &pInflictorWeapon->GetCSWpnData();
		}
	}

	CBaseCSGrenadeProjectile* pGrenade = dynamic_cast< CBaseCSGrenadeProjectile* >( pInflictor );
	if ( !pFlinchInfoSource	 )
	{	
		if ( pGrenade )
		{
			pFlinchInfoSource = pGrenade->m_pWeaponInfo;
		}
	}

	// special case for inferno (caused by molotov projectiles )
	if ( !pFlinchInfoSource	 )
	{
		if ( pInflictor->ClassMatches( "inferno" ) )
		{
			pFlinchInfoSource = GetWeaponInfo( WEAPON_MOLOTOV );
		}
	}

	if ( pAttacker )
	{
		// [paquin. forest] if  this is blast damage, and we haven't opted out with a cvar,
		// we need to get the armor ratio out of the inflictor

		if( info.GetDamageType() & DMG_BURN )
		{
			// (DDK ) Ideally we'd use the info's weapon information instead of damage type, but this field appears to be unused and not available when passing this thru
			pAttacker->AddBurnDamageDelt( entindex() );
		}

		if ( info.GetDamageType() & DMG_BLAST )
		{
			// [paquin] if we know this is a grenade, use it's armor ratio, otherwise
			// use the he grenade armor ratio

			const CCSWeaponInfo* pWeaponInfo;

			if ( pGrenade && pGrenade->m_pWeaponInfo )
			{
				pWeaponInfo = pGrenade->m_pWeaponInfo;
			}
			else
			{
				pWeaponInfo = GetWeaponInfo( WEAPON_HEGRENADE );
			}

			if ( pWeaponInfo )
			{
				flArmorRatio *= pWeaponInfo->GetArmorRatio( pInflictorWeapon ? pInflictorWeapon->GetEconItemView() : NULL );
			}
		}
		else
		{
			const CCSWeaponInfo* pWeaponInfo = GetWeaponInfoFromDamageInfo(info);
			if ( pWeaponInfo )
			{
				flArmorRatio *= pWeaponInfo->GetArmorRatio( pInflictorWeapon ? pInflictorWeapon->GetEconItemView() : NULL );

				if ( info.GetDamageType() & DMG_BULLET && bDamageIsFromOpponent )
				{
					CCS_GameStats.Event_ShotHit( pAttacker, info );	// [pmf] Should this be done AFTER damage reduction?
				}
			}
		}
	}

	float fDamageToHealth = flDamage;
	float fDamageToArmor = 0;
	float fHeavyArmorBonus = 1.0f;

	if ( HasHeavyArmor() )
	{
		flArmorRatio *= 0.5f;
		flArmorBonus = 0.33f;
		fHeavyArmorBonus = 0.33f;
	}

	// Deal with Armour
	bool bDamageTypeAppliesToArmor = ( info.GetDamageType() == DMG_GENERIC ) ||
		( info.GetDamageType() & (DMG_BULLET | DMG_BLAST | DMG_CLUB | DMG_SLASH) );
	if ( bDamageTypeAppliesToArmor && ArmorValue() && IsArmored( m_LastHitGroup ) )
	{
		fDamageToHealth = flDamage * flArmorRatio;
		fDamageToArmor = (flDamage - fDamageToHealth ) * (flArmorBonus * fHeavyArmorBonus);

		int armorValue = ArmorValue();

		// Does this use more armor than we have?
		if (fDamageToArmor > armorValue )
		{
			fDamageToHealth = flDamage - armorValue / flArmorBonus;
			fDamageToArmor = armorValue;
			armorValue = 0;
		}
		else
		{

			if ( fDamageToArmor < 0 )
					fDamageToArmor = 1;

			armorValue -= fDamageToArmor;
		}
		m_lastDamageArmor = (int )fDamageToArmor;
		SetArmorValue(armorValue );

		// [tj] Handle headshot-surviving achievement
		if ( ( m_LastHitGroup == HITGROUP_HEAD ) && bDamageIsFromGunfire )
		{
			if ( flDamage > GetHealth() && fDamageToHealth < GetHealth() )
			{
				m_bSurvivedHeadshotDueToHelmet = true;
			}
		}

		flDamage = fDamageToHealth;
			
		info.SetDamage( flDamage );

		if ( ArmorValue() <= 0.0 )
		{
			m_bHasHelmet = false;
			m_bHasHeavyArmor = false;
		}

		if( !(info.GetDamageType() & DMG_FALL ) && !(info.GetDamageType() & DMG_BURN ) && !(info.GetDamageType() & DMG_BLAST ) )
		{

			Pain( pAttacker, true /*has armor*/, info.GetDamageType() );
		}
	}
	else 
	{
		m_lastDamageArmor = 0;
		if( !(info.GetDamageType() & DMG_FALL ) )
			Pain( pAttacker, false /*no armor*/, info.GetDamageType() );
	}

	CEconItemView *pItem = NULL;

	if ( pInflictorWeapon != NULL || ( pGrenade && fDamageToHealth > 0 ) )
	{
		if ( !pGrenade )
			pItem = pInflictorWeapon->GetEconItemView();

		// The word "flinch" actually means "tagging" here which reduces your movement velocity
		int nKnifeSpeed = 250;
		float fFlinchModifier = 1.0;// ShouldDoLargeFlinch( info, m_LastHitGroup ) ? pFlinchInfoSource->GetFlinchVelocityModifierLarge( pInflictorWeapon->GetEconItemView() ) : pFlinchInfoSource->GetFlinchVelocityModifierSmall( pInflictorWeapon->GetEconItemView() );

		float flFlinchModLarge = pFlinchInfoSource->GetFlinchVelocityModifierLarge( pItem );
		float flFlinchModSmall = pFlinchInfoSource->GetFlinchVelocityModifierSmall( pItem );

		// if grenade, scale by the damage
		if ( pGrenade )
		{
			float flScale = 1.0f - ( fDamageToHealth / 40.0f );
			flFlinchModLarge += ( flScale * 1.05 );
			flFlinchModLarge = MIN( 1.5f, flFlinchModLarge );
		}

		// apply the minimum large flinch amount on the first hit and on subsequent hits, 
		// apply a portion of the small amount - less as we apply more
		m_flFlinchStack = MIN( m_flFlinchStack, MIN( flFlinchModLarge, flFlinchModLarge - ( 1.0 - m_flFlinchStack ) * flFlinchModSmall ) );

		// don't modify m_flFlinchStack, keep it raw because it will decay in Think
		fFlinchModifier = m_flFlinchStack;

		//Msg( "%s: m_flFlinchStack = %f\n", GetPlayerName(), m_flFlinchStack );

		// scale this by a global convar value
		fFlinchModifier *= mp_tagging_scale.GetFloat();

		float flHeavyArmorFlinchBonus = m_bHasHeavyArmor ? CS_PLAYER_HEAVYARMOR_FLINCH_MODIFIER : 1;
		fFlinchModifier *= flHeavyArmorFlinchBonus;
		//float flExosuitFlinchBonus = m_bHasExosuit ? CS_PLAYER_EXOSUIT_FLINCH_MODIFIER : 1;
		//fFlinchModifier *= flExosuitFlinchBonus;

		// get the player's current max speed based on their weapon 
		float flWepMaxSpeed = GetActiveCSWeapon() ? GetActiveCSWeapon()->GetMaxSpeed() : nKnifeSpeed;

		// this is the value we use to scale the above fFlinchModifier - 
		// knives receive less, AKs receive a bit more, etc
 		float flLocalWepScaler = MAX( 0.15, ( flWepMaxSpeed - 120 ) / ( nKnifeSpeed - 120 ) ) * 0.8f;
		flLocalWepScaler += 0.08;
		fFlinchModifier = ( fFlinchModifier * flLocalWepScaler );

		// the held weapon also determines what the tagging cap should be
		// since it's accumulative, we want to be able to cap it so we don't keep getting more
		// tagged the more someone shoots us
		float flRatio = (MIN( 1.0, ( ( flWepMaxSpeed - 80 ) / ( nKnifeSpeed - 80 ) ) ) * 1.2f) - 0.08f;
		//float flClampMin = MAX( 0.2, flRatio - ( 0.65 * ( 1 + ( 1.0 - flRatio ) ) ) );
		float flClampMin = MAX( 0.2, (flRatio / 4) );

		float flClampMax = ( flFlinchModLarge > 0.65 ) ? flFlinchModLarge : 0.65;
		// do the clamp
		fFlinchModifier = clamp( fFlinchModifier, flClampMin, flClampMax );

		// reduce stamina slightly
		m_flStamina = clamp( m_flStamina + ( 8 * ( 1.0 - fFlinchModifier ) ), 0.0f, sv_staminamax.GetFloat() );

		// lerp between no flinch (all damage reduced to 0) to specified flinch (full damage)
		SetFlinchVelocityModifier( Lerp( fFriendlyFireDamageReductionRatio, 1.0f, fFlinchModifier ) );

		//Msg( "%s: flClampMin = %f, m_flFlinchStack = %f, fFlinchModifier = %f\n", GetPlayerName(), flClampMin, m_flFlinchStack, fFlinchModifier );
	}
	
	// keep track of amount of damage last sustained
	m_lastDamageAmount = flDamage;

	// round damage to integer
	m_lastDamageHealth = (int )flDamage;
	info.SetDamage( m_lastDamageHealth );

#if REPORT_PLAYER_DAMAGE
	// damage output spew
	char dmgtype[64];
	CTakeDamageInfo::DebugGetDamageTypeString( info.GetDamageType(), dmgtype, sizeof(dmgtype ) );

	if ( info.GetDamageType() & DMG_HEADSHOT )
		Q_strncat(dmgtype, "HEADSHOT", sizeof(dmgtype ) );

	char outputString[256];
	Q_snprintf( outputString, sizeof(outputString ), "%f: Player %s incoming %f damage from %s, type %s; applied %d health and %d armor\n", 
		gpGlobals->curtime, GetPlayerName(),
		inputInfo.GetDamage(), info.GetInflictor()->GetDebugName(), dmgtype,
		m_lastDamageHealth, m_lastDamageArmor );

	Msg(outputString );
#endif

	if ( info.GetDamage() <= 0 )
		return 0;

	CSingleUserAndReplayRecipientFilter user( this );
	user.MakeReliable();

	CCSUsrMsg_Damage msg;
	
	msg.set_amount( (int )info.GetDamage() );
	msg.set_victim_entindex( entindex() );

	const Vector& inflictor = info.GetInflictor()->WorldSpaceCenter();
	msg.mutable_inflictor_world_pos()->set_x( inflictor.x );
	msg.mutable_inflictor_world_pos()->set_y( inflictor.y );
	msg.mutable_inflictor_world_pos()->set_z( inflictor.z );

	SendUserMessage( user, CS_UM_Damage, msg );

	// Do special explosion damage effect
	if ( info.GetDamageType() & DMG_BLAST )
	{
		OnDamagedByExplosion( info );
	}

	if ( info.GetDamageType() & DMG_BURN )
	{
		m_fMolotovDamageTime = gpGlobals->curtime;
	}

	if( m_lowHealthGoalTime == 0.0f && GetHealth() - info.GetDamage() <= AchievementConsts::StillAlive_MaxHealthLeft )
	{
		// we're really low on health...
		// set an achievement timer in case we can stay alive with this much health for a while
		m_lowHealthGoalTime = gpGlobals->curtime + 30.0f;
	}

	
	// [menglish] Achievement award for kill stealing i.e. killing an enemy who was very damaged from other players   <--- "LOL" -mtw
	// [Forrest] Moved this check before RecordDamageTaken so that the damage currently being dealt by this player
	//           won't disqualify them from getting the achievement.
	if(GetHealth() - info.GetDamage() <= 0 && GetHealth() <= AchievementConsts::KillLowDamage_MaxHealthLeft )
	{
		bool onlyDamage = true;
		if( pAttacker && IsOtherEnemy( pAttacker ) )
		{
			//Verify that the killer has not done damage to this player beforehand
			FOR_EACH_LL( m_DamageList, i )
			{
				if ( m_DamageList[i]->GetPlayerRecipientPtr() == this && m_DamageList[i]->GetPlayerDamagerPtr() == pAttacker )
				{
					onlyDamage = false;
					break;
				}
			}
			if ( onlyDamage )
			{
				pAttacker->AwardAchievement(CSKillLowDamage );
			}
		}
	}

	//
	// this is the actual damage applied to the player and not the raw damage that was output from the weapon
	int nHealthRemoved = (GetHealth() < info.GetDamage()) ? GetHealth() : info.GetDamage();

	if ( pAttacker )
	{
		// Record for the shooter
		pAttacker->RecordDamage( pAttacker, this, info.GetDamage(), nHealthRemoved );

		if ( CSGameRules()->ShouldRecordMatchStats() )
		{
			pAttacker->m_iMatchStats_Damage.GetForModify( CSGameRules()->GetRoundsPlayed() ) += nHealthRemoved;	// record in MatchStats'
			
			// Keep track in QMM data
			if ( pAttacker->m_uiAccountId && CSGameRules() )
			{
				if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( pAttacker->m_uiAccountId ) )
				{
					pQMM->m_iMatchStats_Damage[ CSGameRules()->GetRoundsPlayed() ] = pAttacker->m_iMatchStats_Damage.Get( CSGameRules()->GetRoundsPlayed() );
				}
			}

			// utility damage
			if ( bDamageIsFromGrenade || ( pInflictor->ClassMatches( "inferno" ) ) )
			{
				pAttacker->m_iMatchStats_UtilityDamage.GetForModify( CSGameRules( )->GetRoundsPlayed( ) ) += nHealthRemoved;	// record in MatchStats'

				// Keep track in QMM data
				if ( pAttacker->m_uiAccountId && CSGameRules( ) )
				{
					if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules( )->QueuedMatchmakingPlayersDataFind( pAttacker->m_uiAccountId ) )
					{
						pQMM->m_iMatchStats_UtilityDamage[ CSGameRules( )->GetRoundsPlayed( ) ] = pAttacker->m_iMatchStats_UtilityDamage.Get( CSGameRules( )->GetRoundsPlayed( ) );
					}
				}
			}


		}

		// And for the victim (don't double-record if it is the same person)
		if ( pAttacker != this )
		{
			RecordDamage( pAttacker, this, info.GetDamage(), nHealthRemoved );
		}

		// Track total number of health points removed
		if ( bDamageIsFromOpponent && ( nHealthRemoved > 0 ) &&
			!this->IsBot() && !CSGameRules()->IsWarmupPeriod() )
		{
			if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( GetHumanPlayerAccountID() ) )
			{
				pQMM->m_numHealthPointsRemovedTotal += nHealthRemoved;
			}
		}
		if ( bDamageIsFromOpponent && ( nHealthRemoved > 0 ) &&
			!pAttacker->IsBot() && !CSGameRules()->IsWarmupPeriod() )
		{
			if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( pAttacker->GetHumanPlayerAccountID() ) )
			{
				pQMM->m_numHealthPointsDealtTotal += nHealthRemoved;
			}
		}

		// if a player is dealing damage to a bot without a pistol, then they lose pistols only bonus
		if ( bDamageIsFromOpponent && ( nHealthRemoved > 0 ) &&
			IsBot() && CSGameRules()->IsPlayingCoopMission() )
		{
			if ( CWeaponCSBase *pWeaponDamage = dynamic_cast< CWeaponCSBase * >( info.GetWeapon() ) )
			switch ( pWeaponDamage->GetWeaponType() )
			{
			case WEAPONTYPE_SUBMACHINEGUN:
			case WEAPONTYPE_RIFLE:
			case WEAPONTYPE_SHOTGUN:
			case WEAPONTYPE_SNIPER_RIFLE:
			case WEAPONTYPE_MACHINEGUN:
				CSGameRules()->m_coopBonusPistolsOnly = false;
				break;
			}
		}

		if ( bDamageIsFromOpponent )
		{
			CSGameRules()->ScorePlayerDamage( pAttacker, info.GetDamage() );
		}
		else if ( bDamageIsFromTeammate )
		{
			// we need to check to see how much damage our attacker has done to teammates during this round and warm or kick as needed
			int nDamageGivenThisRound = 0;
			//CDamageRecord *pDamageList = pAttacker->GetDamageGivenList();
			FOR_EACH_LL( pAttacker->GetDamageList(), i )
			{
				if ( !pAttacker->GetDamageList()[i] )
					continue;

				if ( pAttacker->GetDamageList()[i]->GetPlayerDamagerPtr() != pAttacker )
					continue;

				CCSPlayer *pDamageGivenListPlayer = pAttacker->GetDamageList()[i]->GetPlayerRecipientPtr();
				if ( !pDamageGivenListPlayer )
					continue;

				int nGivenTeam = pDamageGivenListPlayer->GetTeamNumber();	
				if( ( pDamageGivenListPlayer != pAttacker ) && pAttacker->IsOtherSameTeam( nGivenTeam ) && !IsOtherEnemy( pDamageGivenListPlayer ) )
				{	
					nDamageGivenThisRound += pAttacker->GetDamageList()[i]->GetActualHealthRemoved();
				}		
			}

			bool bIsPlayingOffline = CSGameRules() && CSGameRules()->IsPlayingOffline();

			if ( mp_autokick.GetBool() && !bIsPlayingOffline )
			{
				if ( mp_spawnprotectiontime.GetInt() > 0 && CSGameRules() && CSGameRules()->GetRoundElapsedTime() < mp_spawnprotectiontime.GetInt() )
				{
					if ( nDamageGivenThisRound > mp_td_spawndmgthreshold.GetInt() )
					{
						// if we've already warned, but haven't warned this round, kick them or if we already have given a warning previously for doing too much in general
						if ( (pAttacker->m_bTDGaveProtectionWarning && !pAttacker->m_bTDGaveProtectionWarningThisRound) || (pAttacker->m_nTeamDamageGivenForMatch + nDamageGivenThisRound) > mp_td_dmgtowarn.GetInt() )
						{
							if ( sv_kick_ban_duration.GetInt() > 0 )
							{
								CSGameRules()->SendKickBanToGC( pAttacker, k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_THSpawn );
								// don't roll the kick command into this, it will fail on a lan, where kickid will go through
								engine->ServerCommand( CFmtStr( "banid %d %d;", sv_kick_ban_duration.GetInt(), pAttacker->GetUserID() ) );
							}

							engine->ServerCommand( UTIL_VarArgs( "kickid_ex %d %d For doing too much team damage\n", pAttacker->GetUserID(), bIsPlayingOffline ? 0 : 1 ) );
						}
						else if ( !pAttacker->m_bTDGaveProtectionWarningThisRound )
						{
							pAttacker->m_bTDGaveProtectionWarningThisRound = true;
							pAttacker->m_bTDGaveProtectionWarning = true;

							// give a warning
							ClientPrint( pAttacker, HUD_PRINTTALK, "#Cstrike_TitlesTXT_Hint_warning_team_damage_start" );
						}
					}
				}

				if ( (pAttacker->m_nTeamDamageGivenForMatch + nDamageGivenThisRound) > mp_td_dmgtowarn.GetInt() )
				{
					if ( (m_flLastTHWarningTime + 5.0f) < gpGlobals->curtime )
					{
						m_flLastTHWarningTime = gpGlobals->curtime;
						ClientPrint( pAttacker, HUD_PRINTTALK, "#Cstrike_TitlesTXT_Hint_warning_team_damage" );
					}			

					if ( (pAttacker->m_nTeamDamageGivenForMatch + nDamageGivenThisRound) > mp_td_dmgtokick.GetInt() )
					{
						if ( sv_kick_ban_duration.GetInt() > 0 )
						{
							CSGameRules()->SendKickBanToGC( pAttacker, k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_THLimit );
							// don't roll the kick command into this, it will fail on a lan, where kickid will go through
							engine->ServerCommand( CFmtStr( "banid %d %d;", sv_kick_ban_duration.GetInt(), pAttacker->GetUserID() ) );
						}

						engine->ServerCommand( UTIL_VarArgs( "kickid_ex %d %d For doing too much team damage\n", pAttacker->GetUserID(), bIsPlayingOffline ? 0 : 1 ) );
					}
				}

				//Msg( "nDamageGiven from %s is %d, m_nTeamDamageGivenForMatch is %d\n", pAttacker->GetPlayerName(), nDamageGivenThisRound, pAttacker->m_nTeamDamageGivenForMatch );
			}
		}
	}
	else
	{
		RecordDamage( NULL, this, info.GetDamage(), nHealthRemoved ); //damaged by a null player - likely the world
	}

	m_vecTotalBulletForce += info.GetDamageForce();

	gamestats->Event_PlayerDamage( this, info );

	return CBaseCombatCharacter::OnTakeDamage( info );
}


//MIKETODO: this probably should let the shield model catch the trace attacks.
bool CCSPlayer::IsHittingShield( const Vector &vecDirection, trace_t *ptr )
{
	if ( HasShield() == false )
		 return false;

	if ( IsShieldDrawn() == false )
		 return false;
	
	float		flDot;
	Vector		vForward;
	Vector2D	vec2LOS = vecDirection.AsVector2D();
	AngleVectors( GetLocalAngles(), &vForward );

	Vector2DNormalize( vForward.AsVector2D() );
	Vector2DNormalize( vec2LOS );
	
	flDot = DotProduct2D ( vec2LOS , vForward.AsVector2D() );

	if ( flDot < -0.87f )
		 return true;
	
	return false;
}

void CCSPlayer::AwardAchievement( int iAchievement, int iCount )
{
	if ( IsControllingBot() )
	{
		// Players controlling bots cannot earn achievements
		return;
	}

	BaseClass::AwardAchievement( iAchievement, iCount );
}

void CCSPlayer::ClearGunGameImmunity( void )
{
	// Fired a shot so no longer immune
	m_bGunGameImmunity = false;
	m_fImmuneToGunGameDamageTime = 0.0f;
}

ConVar mp_flinch_punch_scale( "mp_flinch_punch_scale", "3", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Scalar for first person view punch when getting hit." );

//ConVar aimpunch_fix( "aimpunch_fix", "0" );

void CCSPlayer::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	bool bShouldBleed = true;
	bool bShouldSpark = false;
	bool bHitShield = IsHittingShield( vecDir, ptr );


	CBasePlayer *pAttacker = (CBasePlayer* )ToBasePlayer( info.GetAttacker() );

	// show blood for firendly fire only if FF is on
	if ( pAttacker && IsOtherSameTeam( pAttacker->GetTeamNumber() ) && !IsOtherEnemy( pAttacker->entindex() ) )
		 bShouldBleed = CSGameRules()->IsFriendlyFireOn();
		
	if ( m_takedamage != DAMAGE_YES )
		return;

	m_LastHitGroup = ptr->hitgroup;

	m_nForceBone = ptr->physicsbone;	//Save this bone for ragdoll

	float flDamage = info.GetDamage();

	QAngle punchAngle;
	float flAng;
	
	bool hitByBullet = false;
	bool hitByGrenadeProjectile = false;
	bool bHeadShot = false;

	float flBodyDamageScale = (GetTeamNumber() == TEAM_CT) ? mp_damage_scale_ct_body.GetFloat() : mp_damage_scale_t_body.GetFloat();
	float flHeadDamageScale = (GetTeamNumber() == TEAM_CT) ? mp_damage_scale_ct_head.GetFloat() : mp_damage_scale_t_head.GetFloat();

	// heavy armor reduces headshot damage by have of what it is, so it does x2 instead of x4
	if ( HasHeavyArmor() )
		flHeadDamageScale = flHeadDamageScale * 0.5;

	if( m_bGunGameImmunity )
	{
		bShouldBleed = false;
	}
	else if( bHitShield )
	{
		flDamage = 0;
		bShouldBleed = false;
		bShouldSpark = true;
	}
	else if( info.GetDamageType() & DMG_SHOCK )
	{
		bShouldBleed = false;
	}
	else if( info.GetDamageType() & DMG_BLAST )
	{
		if ( ArmorValue() > 0 )
			 bShouldBleed = false;

			if ( bShouldBleed == true )
			{
				// punch view if we have no armor
				punchAngle = GetRawAimPunchAngle();
				punchAngle.x = mp_flinch_punch_scale.GetFloat() * flDamage * -0.1;

				if ( punchAngle.x < mp_flinch_punch_scale.GetFloat() * -4 )
					punchAngle.x = mp_flinch_punch_scale.GetFloat() * -4;

				SetAimPunchAngle( punchAngle );
			}
		}
		else
		{
			const CCSWeaponInfo* pWeaponInfo = GetWeaponInfoFromDamageInfo( info );
			CWeaponCSBase *pWeapon = dynamic_cast<CWeaponCSBase*>(info.GetWeapon());
			CEconItemView *pItem = NULL;

			if ( pWeapon )
				pItem = pWeapon->GetEconItemView();

			if ( pWeaponInfo )
			{
				hitByBullet = IsGunWeapon( pWeaponInfo->GetWeaponType( pItem ) );
				hitByGrenadeProjectile = ( ( pWeaponInfo->GetWeaponType( pItem ) == WEAPONTYPE_GRENADE ) && ( info.GetDamageType() & DMG_CLUB ) != 0 );
			}

			switch ( ptr->hitgroup )
			{
			case HITGROUP_GENERIC:
				break;

			case HITGROUP_HEAD:

				if ( m_bHasHelmet && !hitByGrenadeProjectile )
				{
					//				bShouldBleed = false;
					bShouldSpark = true;
				}

				flDamage *= 4;
				flDamage *= flHeadDamageScale;

				if ( !m_bHasHelmet )
				{
					punchAngle = GetRawAimPunchAngle();

					punchAngle.x += mp_flinch_punch_scale.GetFloat() * flDamage * -0.5;

					if ( punchAngle.x < mp_flinch_punch_scale.GetFloat() * -12 )
						punchAngle.x = mp_flinch_punch_scale.GetFloat() * -12;

					punchAngle.z = mp_flinch_punch_scale.GetFloat() * flDamage * random->RandomFloat(-1,1 );

					if ( punchAngle.z < mp_flinch_punch_scale.GetFloat() * -9 )
						punchAngle.z = mp_flinch_punch_scale.GetFloat() * -9;

					else if ( punchAngle.z > mp_flinch_punch_scale.GetFloat() * 9 )
						punchAngle.z = mp_flinch_punch_scale.GetFloat() * 9;

					SetAimPunchAngle( punchAngle );
				}

				bHeadShot = true;

				break;

			case HITGROUP_CHEST:

				flDamage *= 1.0;
				flDamage *= flBodyDamageScale;

				if ( ArmorValue() <= 0 )
					flAng = -0.1;
				else
					flAng = -0.005;


				punchAngle = GetRawAimPunchAngle();

				punchAngle.x += mp_flinch_punch_scale.GetFloat() * flDamage * flAng;

				if ( punchAngle.x < mp_flinch_punch_scale.GetFloat() * -4 )
					punchAngle.x = mp_flinch_punch_scale.GetFloat() * -4;

				SetAimPunchAngle( punchAngle );

				break;

			case HITGROUP_STOMACH:

				flDamage *= 1.25;
				flDamage *= flBodyDamageScale;

				if ( ArmorValue() <= 0 )
					flAng = -0.1;
				else
					flAng = -0.005;


				punchAngle = GetRawAimPunchAngle();

				punchAngle.x += mp_flinch_punch_scale.GetFloat() * flDamage * flAng;

				if ( punchAngle.x < mp_flinch_punch_scale.GetFloat() * -4 )
					punchAngle.x = mp_flinch_punch_scale.GetFloat() * -4;

				SetAimPunchAngle( punchAngle );


				break;

			case HITGROUP_LEFTARM:
			case HITGROUP_RIGHTARM:

				flDamage *= 1.0;
				flDamage *= flBodyDamageScale;
				// 
				// 			punchAngle = GetRawAimPunchAngle();
				// 			punchAngle.x = mp_flinch_punch_scale.GetFloat() * flDamage * -0.005;
				// 
				// 			if ( punchAngle.x < mp_flinch_punch_scale.GetFloat() * -2 )
				// 				punchAngle.x = mp_flinch_punch_scale.GetFloat() * -2;
				// 
				// 			SetAimPunchAngle( punchAngle );
				// 
				break;

			case HITGROUP_LEFTLEG:
			case HITGROUP_RIGHTLEG:

				flDamage *= 0.75;
				flDamage *= flBodyDamageScale;
				// 
				// 			punchAngle = GetRawAimPunchAngle();
				// 			punchAngle.x = mp_flinch_punch_scale.GetFloat() * flDamage * -0.005;
				// 
				// 			if ( punchAngle.x < mp_flinch_punch_scale.GetFloat() * -1 )
				// 				punchAngle.x = mp_flinch_punch_scale.GetFloat() * -1;
				// 
				// 			SetAimPunchAngle( punchAngle );
				// 
				break;

			default:
				break;
		}
	}


	// Since this code only runs on the server, make sure it shows the tempents it creates.
	CDisablePredictionFiltering disabler;
	
	if ( bShouldBleed )
	{
		// This does smaller splotches on the guy and splats blood on the world.
		TraceBleed( flDamage, vecDir, ptr, info.GetDamageType() );

		CEffectData	data;
		data.m_vOrigin = ptr->endpos;
		data.m_vNormal = vecDir * -1;
		data.m_nEntIndex = ptr->m_pEnt ?  ptr->m_pEnt->entindex() : 0;
		data.m_flMagnitude = flDamage;

		// reduce blood effect if target has armor
		if ( ArmorValue() > 0 )
			data.m_flMagnitude *= 0.5f;
	
		// reduce blood effect if target is hit in the helmet
		if ( ptr->hitgroup == HITGROUP_HEAD && bShouldSpark )
			data.m_flMagnitude = 1;

		DispatchEffect( "csblood", data );
	}
	if ( ( ptr->hitgroup == HITGROUP_HEAD/* || bHitShield*/ ) && bShouldSpark ) // they hit a helmet
	{
		// show metal spark effect
		//g_pEffects->Sparks( ptr->endpos, 1, 1, &ptr->plane.normal );

		QAngle angle;
		VectorAngles( ptr->plane.normal, angle );
		DispatchParticleEffect( "impact_helmet_headshot", ptr->endpos, angle );
	}
	
	if ( !bHitShield )
	{
		CTakeDamageInfo subInfo = info;

		subInfo.SetDamage( flDamage );

		float impulseMultiplier = 1.0f;

		if ( hitByBullet )
		{
			impulseMultiplier = phys_playerscale.GetFloat();
			if ( bHeadShot )
			{
				subInfo.AddDamageType( DMG_HEADSHOT );
				impulseMultiplier *= phys_headshotscale.GetFloat();
			}
		}

		if ( hitByGrenadeProjectile )
		{
			impulseMultiplier = 0.0f;
		}


		subInfo.SetDamageForce( info.GetDamageForce() * impulseMultiplier );

		AddMultiDamage( subInfo, this );
	}
}


void CCSPlayer::Reset( bool resetScore )
{
	if( resetScore )
	{
		RemoveNemesisRelationships();
		ResetFragCount();
		ResetAssistsCount();
		ResetDeathCount();
		ClearScore();
		ClearContributionScore();
		// when score is reset, make sure 
		m_numRoundsSurvived = m_maxNumRoundsSurvived = 0;
		m_longestLife = -1.0f;
		m_iTotalCashSpent = 0;
		m_iCashSpentThisRound = 0;
		m_nTeamDamageGivenForMatch = 0;
		m_bTDGaveProtectionWarning = false;
		m_bTDGaveProtectionWarningThisRound = false;
		m_nEndMatchNextMapVote = -1;

		m_iEnemyKills = 0;
		m_iEnemyKillHeadshots = 0;
		m_iEnemy3Ks = 0;
		m_iEnemy4Ks = 0;
		m_iEnemy5Ks = 0;
		m_iEnemyKillsAgg = 0;
		m_numFirstKills = 0;
		m_numClutchKills = 0;
		m_numPistolKills = 0;
		m_numSniperKills = 0;

		m_iRoundsWon = 0;

		m_bLastKillUsedUniqueWeaponMatch = 0;
		m_uniqueKillWeaponsMatch.RemoveAll();
	}

	ResetDamageCounters();
	ResetAccount();		// remove all player cash

	//remove any weapons they bought before the round started
	RemoveAllItems( true );

	//RemoveShield();

	if ( !resetScore )
		InitializeAccount();		// don't track initial amount as earned
	else
		InitializeAccount( CSGameRules()->GetStartMoney() );

	// setting this to the current time prevents late-joining players from getting prioritized for receiving the defuser/bomb
	m_fLastGivenDefuserTime = gpGlobals->curtime;
	m_fLastGivenBombTime = gpGlobals->curtime;


}

//-----------------------------------------------------------------------------
// Purpose: Displays a hint message to the player
// Input  : *pMessage - 
//			bDisplayIfDead - 
//			bOverrideClientSettings - 
//-----------------------------------------------------------------------------
void CCSPlayer::HintMessage( const char *pMessage, bool bDisplayIfDead, bool bOverrideClientSettings )
{
	if ( !bDisplayIfDead && !IsAlive() || !IsNetClient() || !m_pHintMessageQueue )
		return;

	if ( bOverrideClientSettings || m_bShowHints )
		m_pHintMessageQueue->AddMessage( pMessage );
}


void CCSPlayer::ResetAccount()
{
	m_iAccount = 0;
	m_iAccountMoneyEarnedForNextRound = 0;
}


void CCSPlayer::InitializeAccount( int amount )
{
	if ( ( amount == -1 ) && m_uiAccountId )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			amount = pQMM->m_cash;
		}
	}

	m_iAccount = ( amount == -1 ) ? CSGameRules()->GetStartMoney() : amount;
	m_iAccountMoneyEarnedForNextRound = 0;

	int MaxAmount = CSGameRules()->GetMaxMoney();
	Assert( m_iAccount >= 0 && m_iAccount <= MaxAmount );

	m_iAccount = clamp<int, int, int>( m_iAccount, 0, MaxAmount );

	if ( CSGameRules()->ShouldRecordMatchStats() )
	{
		m_iMatchStats_CashEarned.GetForModify( CSGameRules()->GetTotalRoundsPlayed() ) = m_iAccount;
	}

	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			pQMM->m_cash = m_iAccount;

			if ( CSGameRules()->ShouldRecordMatchStats() )
			{
				pQMM->m_iMatchStats_CashEarned[ CSGameRules()->GetTotalRoundsPlayed() ] = m_iMatchStats_CashEarned.Get( CSGameRules()->GetTotalRoundsPlayed() );
			}
		}
	}
}


bool CCSPlayer::AreAccountAwardsEnabled( PlayerCashAward::Type reason ) const
{
	// no awards in the warmup period
	if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() )
		return false;

	// cash awards for individual actions must be enabled for this game mode
	if ( !mp_playercashawards.GetBool() )
		return false;

	return true;
}

void CCSPlayer::AddAccountAward( PlayerCashAward::Type reason )
{
	Assert( reason != PlayerCashAward::NONE );
	AddAccountAward(reason, CSGameRules()->PlayerCashAwardValue ( reason ) );
}


void CCSPlayer::AddAccountAward( PlayerCashAward::Type reason, int amount, const CWeaponCSBase *pWeapon )
{
	if ( !AreAccountAwardsEnabled( reason ) )
		return;

	if ( amount == 0 )
		return;

	const char* awardReasonToken = NULL;	
	const char* sign_string = "+$";
	const char* szWeaponName = NULL;

	extern ConVar cash_player_killed_enemy_default;
	extern ConVar cash_player_killed_enemy_factor;

	int currentround = CSGameRules()->GetTotalRoundsPlayed();

	switch ( reason )
	{
	case PlayerCashAward::KILL_TEAMMATE:
		awardReasonToken = "#Player_Cash_Award_Kill_Teammate";
		sign_string = "-$";
		break;
	case PlayerCashAward::KILLED_ENEMY:

		awardReasonToken = "#Player_Cash_Award_Killed_Enemy_Generic";

		// if award amount is non-default, use the verbose message.
		if ( pWeapon && ( amount != cash_player_killed_enemy_default.GetInt() ))
		{
			szWeaponName = pWeapon->GetEconItemView()->GetItemDefinition()->GetItemBaseName();
			awardReasonToken = "#Player_Cash_Award_Killed_Enemy";
		}

		// scale amount by kill award factor convar.
		amount = RoundFloatToInt( amount * cash_player_killed_enemy_factor.GetFloat() );

		// Match stats

		if ( CSGameRules()->ShouldRecordMatchStats() )
		{
			m_iMatchStats_KillReward.GetForModify( currentround ) += amount; 

			// Keep track of Match stats in QMM data
			if ( m_uiAccountId && CSGameRules() )
			{
				if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
				{
					pQMM->m_iMatchStats_KillReward[ currentround ] = m_iMatchStats_EquipmentValue.Get( currentround );
				}
			}
		}

		break;
	case PlayerCashAward::BOMB_PLANTED:
		awardReasonToken = "#Player_Cash_Award_Bomb_Planted";
		break;
	case PlayerCashAward::BOMB_DEFUSED:
		awardReasonToken = "#Player_Cash_Award_Bomb_Defused";
		break;
	case PlayerCashAward::RESCUED_HOSTAGE:
		awardReasonToken = "#Player_Cash_Award_Rescued_Hostage";
		break;
	case PlayerCashAward::INTERACT_WITH_HOSTAGE:
		awardReasonToken = "#Player_Cash_Award_Interact_Hostage";
		break;
	case PlayerCashAward::RESPAWN:
		awardReasonToken = "#Player_Cash_Award_Respawn";
		break;
	case PlayerCashAward::GET_KILLED:
		awardReasonToken = "#Player_Cash_Award_Get_Killed";
		break;
	case PlayerCashAward::DAMAGE_HOSTAGE:
		awardReasonToken = "#Player_Cash_Award_Damage_Hostage";
		sign_string = "-$";
		break;
	case PlayerCashAward::KILL_HOSTAGE:
		awardReasonToken = "#Player_Cash_Award_Kill_Hostage";
		sign_string = "-$";
		break;
	default:
		break;
	}

	char strAmount[64];
	Q_snprintf( strAmount, sizeof( strAmount ), "%s%d", sign_string, abs( amount ));

	// don't message 0 or negative values in coop
	if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() && amount <= 0 )
	{
	}
	else
	{
		ClientPrint( this, HUD_PRINTTALK, awardReasonToken, strAmount, szWeaponName );
	}

	AddAccount( amount, true, false );

	if ( dev_reportmoneychanges.GetBool() )
	{
		CCSBot *pBot = ToCSBot( m_hControlledBot.Get() );
		if ( pBot )
			Msg( "%s earned %d (for %s)	(while being controlled by %s)		(total money: %d)\n", pBot->GetPlayerName(), amount, awardReasonToken, GetPlayerName(), GetAccountBalance() );
		else
			Msg( "%s earned %d (for %s)			(total money: %d)\n", GetPlayerName(), amount, awardReasonToken, GetAccountBalance() );
	}
}

void CCSPlayer::AddAccountFromTeam( int amount, bool bTrackChange, TeamCashAward::Type reason )
{
	// no awards in the warmup period
	if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() )
		return;

	AddAccount( amount, bTrackChange, false, NULL );

	if( IsControllingBot() )
	{
		// make sure we award team bonus to the actual player controlling the bot
		m_PreControlData.m_iAccount = clamp( m_PreControlData.m_iAccount + amount, 0, CSGameRules()->GetMaxMoney() );
		
		if ( CSGameRules() && CSGameRules()->IsPlayingClassic() &&
			!CSGameRules()->IsWarmupPeriod() && !CSGameRules()->IsFreezePeriod() &&
			( amount > 0 ) )
		{	// Any money earned during the round cannot be used until next round starts.
			m_PreControlData.m_iAccountMoneyEarnedForNextRound = clamp( ( int ) m_PreControlData.m_iAccountMoneyEarnedForNextRound + amount, 0, CSGameRules()->GetMaxMoney() );
		}

		if ( CSGameRules()->ShouldRecordMatchStats() )
		{
			m_iMatchStats_CashEarned.GetForModify( CSGameRules()->GetTotalRoundsPlayed() ) = m_PreControlData.m_iAccount;
		}

		// Keep track in QMM data
		if ( m_uiAccountId && CSGameRules() )
		{
			if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
			{
				pQMM->m_cash = m_PreControlData.m_iAccount;

				if ( CSGameRules()->ShouldRecordMatchStats() )
				{
					pQMM->m_iMatchStats_CashEarned[ CSGameRules()->GetTotalRoundsPlayed() ] = m_iMatchStats_CashEarned.Get( CSGameRules()->GetTotalRoundsPlayed() );
				}
			}
		}

		if ( dev_reportmoneychanges.GetBool() )
		{
			CCSBot *pBot = ToCSBot( m_hControlledBot.Get() );
			Msg( "%s earned %d (while controlling %s), Adding to Pre-Bot Control Data Account		(total cached money: %d)\n", GetPlayerName(), amount, pBot ? pBot->GetPlayerName() : "NULL", m_PreControlData.m_iAccount );
		}
	}
}

void CCSPlayer::AddAccount( int amount, bool bTrackChange, bool bItemBought, const char *pItemName )
{
	// we don't want to award a bot money that is being controlled by a player because the player is currently storing the bots money
	if ( IsBot() && HasControlledByPlayer() )
		return;

	int iAccountStarting = m_iAccount;

	m_iAccount += amount;

	if ( CSGameRules() && CSGameRules()->IsPlayingClassic() &&
		!CSGameRules()->IsWarmupPeriod() && !CSGameRules()->IsFreezePeriod() &&
		( amount > 0 ) )
	{	// Any money earned during the round cannot be used until next round starts.
		m_iAccountMoneyEarnedForNextRound = clamp( (int) m_iAccountMoneyEarnedForNextRound + amount, 0, CSGameRules()->GetMaxMoney() );
	}

	if ( dev_reportmoneychanges.GetBool() && amount < 0 && bItemBought )
	{
		CCSBot *pBot = ToCSBot( m_hControlledBot.Get() );
		if ( pBot )
			Msg( "%s spent %d on a %s (while being controlled by %s)		(total left: %d)\n", pBot->GetPlayerName(), amount, pItemName, GetPlayerName(), m_iAccount.Get() );
		else
			Msg( "%s spent %d on a %s		(total left: %d)\n", GetPlayerName(), amount, pItemName, m_iAccount.Get() );
	}

	if ( bTrackChange )
	{
		// note: if we lost money, but didn't make a purchase, we don't log it as money spent or earned, it's a penalty

		if ( amount > 0 )
		{
			CCS_GameStats.Event_MoneyEarned( this, amount );
		}
		else if ( amount < 0 && bItemBought )
		{
			CCS_GameStats.Event_MoneySpent( this, -amount, pItemName );
			m_iTotalCashSpent += -amount;
			m_iCashSpentThisRound += -amount;
		}
	}
		
	m_iAccount = clamp( (int)m_iAccount, 0, CSGameRules()->GetMaxMoney() );

	CSingleUserRecipientFilter user( this );

	CCSUsrMsg_AdjustMoney msg;
	msg.set_amount( amount );
	SendUserMessage( user, CS_UM_AdjustMoney, msg );

	if ( CSGameRules()->ShouldRecordMatchStats() )
	{
		m_iMatchStats_CashEarned.GetForModify( CSGameRules()->GetTotalRoundsPlayed() ) = m_iAccount;
	}

	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() && !IsControllingBot() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			pQMM->m_cash = m_iAccount;

			if ( CSGameRules()->ShouldRecordMatchStats() )
			{
				pQMM->m_iMatchStats_CashEarned[ CSGameRules()->GetTotalRoundsPlayed() ] = m_iMatchStats_CashEarned.Get( CSGameRules()->GetTotalRoundsPlayed() );
			}
		}
	}

	if ( mp_logmoney.GetBool() && amount && ( m_iAccount != iAccountStarting ) )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" money change %d%s%d = $%d%s%s%s%s%s\n",
			GetPlayerName(),
			entindex(),
			GetNetworkIDString(),
			GetTeam()->GetName(),
			iAccountStarting, ( amount > 0 ) ? "+" : "", amount, m_iAccount.Get(),
			bTrackChange ? " (tracked)" : "",
			bItemBought ? " (purchase" : "",
			( pItemName && *pItemName ) ? ": " : "",
			( pItemName && *pItemName ) ? pItemName : "",
			bItemBought ? ")" : ""
			);
	}
}

int CCSPlayer::AddDeathmatchKillScore( int nScore, CSWeaponID wepID, int iSlot, bool bIsAssist, const char* szVictim )
{
	if ( !CSGameRules() || !CSGameRules()->IsPlayingGunGameDeathmatch() )
		return 0;

	if ( nScore <= 0 )
		return 0;

	int nBonus = 0;

	if ( CSGameRules()->IsDMBonusActive() && CSGameRules()->GetDMBonusWeaponLoadoutSlot() == iSlot && !bIsAssist )
		nBonus = ( (float)( mp_dm_bonus_percent.GetInt() ) / 100.0f ) * nScore;

	const char* awardReasonToken = NULL;

	// handle econ weapons
	const char* szWeaponName = "unknown";
	CEconItemView *pItem = Inventory()->GetItemInLoadout( GetTeamNumber(), iSlot );
	if ( pItem )
	{
		szWeaponName = pItem->GetItemDefinition()->GetItemBaseName();
	}

	if ( bIsAssist )
	{
		if ( nScore == 1 )
			awardReasonToken = "#Player_Point_Award_Assist_Enemy";	
		else
			awardReasonToken = "#Player_Point_Award_Assist_Enemy_Plural";	

		szWeaponName = szVictim;
	}
	else
	{
		if ( nScore == 1 )
			awardReasonToken = "#Player_Point_Award_Killed_Enemy";	
		else
			awardReasonToken = "#Player_Point_Award_Killed_Enemy_Plural";	
	}

	char strnScore[64];
	if ( nBonus > 0 )
		Q_snprintf( strnScore, sizeof( strnScore ), "%d (+%d)", abs( nScore ), abs( nBonus ) );
	else
		Q_snprintf( strnScore, sizeof( strnScore ), "%d", abs( nScore ));


	ClientPrint( this, HUD_PRINTTALK, awardReasonToken, strnScore, szWeaponName );

	AddContributionScore( nScore + nBonus );

	// return if not the current leader
	bool bCurrentLeader = true;
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer && pPlayer != this && GetScore() <= pPlayer->GetScore() ) 
		{
			bCurrentLeader = false;
			break;
		}
	}

	if ( bCurrentLeader )
	{
		//if we made it this far, we are the current leader
		IGameEvent *event = gameeventmanager->CreateEvent( "gg_leader" );
		if ( event )
		{
			event->SetInt( "playerid", GetUserID() );
			gameeventmanager->FireEvent( event );
		}
	}

	return nScore + nBonus;
}

void CCSPlayer::MarkAsNotReceivingMoneyNextRound( bool bAllowMoneyNextRound /*=false*/ )
{
	m_receivesMoneyNextRound = bAllowMoneyNextRound;

	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			pQMM->m_bReceiveNoMoneyNextRound = !bAllowMoneyNextRound;
		}
	}
}

void CCSPlayer::ProcessSuicideAsKillReward()
{
	// Don't give any rewards during warmup period or freezetime aka buytime
	// If any human disconnects during buytime then a bot will replace their vacant spot on the team
	// so enemies will have fodder to kill and get a kill reward
	if ( CCSGameRules *pCSGameRules = CSGameRules() )
	{
		if ( pCSGameRules->IsWarmupPeriod() )
			return;
		if ( pCSGameRules->IsFreezePeriod() )
			return;
	}

	// Find alive players on the enemy team and give them maximum possible kill reward
	// tie break by an enemy player with the lowest amount of money
	int myteam = GetTeamNumber();
	int team = myteam;
	switch ( team )
	{
	case TEAM_TERRORIST:
		team = TEAM_CT;
		break;
	case TEAM_CT:
		team = TEAM_TERRORIST;
		break;
	default:
		return;
	}

	// Best bonus enemy
	CCSPlayer *pBestEnemy = NULL;
	int numBestBonusMoney = 0;
	
	// Look at alive players on the team
	for ( int nAttempt = 0; ( nAttempt < 3 ) && !pBestEnemy; ++ nAttempt )
	{
		for ( int playerNum = 1; playerNum <= gpGlobals->maxClients; ++playerNum )
		{
			CCSPlayer *player = ( CCSPlayer * ) UTIL_PlayerByIndex( playerNum );
			if ( !player )
				continue;
			if ( !player->IsAlive() )
				continue;
			if ( player->GetTeamNumber() != team )
				continue;
			if ( !player->AreAccountAwardsEnabled( PlayerCashAward::KILLED_ENEMY ) )
				continue;
			if ( ( nAttempt < 2 ) && player->IsControllingBot() )
				continue;
			if ( ( nAttempt < 1 ) && player->IsBot() )
				continue;

			// Let's see which guns this player has?
			extern ConVar cash_player_killed_enemy_default;
			int numBonusMoney = cash_player_killed_enemy_default.GetInt();
			int arrSlots[] = { WEAPON_SLOT_RIFLE, WEAPON_SLOT_PISTOL };
			for ( int k = 0; k < Q_ARRAYSIZE( arrSlots ); ++k )
			{
				CBaseCombatWeapon *pWpn = player->Weapon_GetSlot( arrSlots[ k ] );
				CWeaponCSBase *pWpnCsBase = dynamic_cast< CWeaponCSBase * >( pWpn );
				if ( !pWpnCsBase ) continue;

				int numWpnKillAward = pWpnCsBase->GetKillAward();
				if ( numWpnKillAward > numBonusMoney )
					numBonusMoney = numWpnKillAward;
			}

			// See if this is a better player to reward?
			if ( ( numBonusMoney > numBestBonusMoney ) ||
				( ( numBonusMoney == numBestBonusMoney ) && ( player->GetAccountBalance() < pBestEnemy->GetAccountBalance() ) ) )
			{
				pBestEnemy = player;
				numBestBonusMoney = numBonusMoney;
			}
		}
	}

	// Give the player kill reward
	extern ConVar cash_player_killed_enemy_factor;
	int numDollarsEarned = RoundFloatToInt( numBestBonusMoney * cash_player_killed_enemy_factor.GetFloat() );
	if ( pBestEnemy && ( numDollarsEarned > 0 ) )
	{
		pBestEnemy->AddAccountAward( PlayerCashAward::KILLED_ENEMY, numBestBonusMoney );

		CFmtStr fmtDollarsEarned( "%u", numDollarsEarned );
		ClientPrint( pBestEnemy, HUD_PRINTTALK, "#Player_Cash_Award_ExplainSuicide_YouGotCash", CFmtStr( "#ENTNAME[%d]%s", this->entindex(), this->GetPlayerName() ), fmtDollarsEarned.Get() );

		// Notify all players about what just happened?
		CRecipientFilter rfSuicidingTeam, rfGettingMoneyTeam;
		rfSuicidingTeam.MakeReliable(); rfGettingMoneyTeam.MakeReliable();
		for ( int playerNum = 1; playerNum <= gpGlobals->maxClients; ++playerNum )
		{
			CCSPlayer *player = ( CCSPlayer * ) UTIL_PlayerByIndex( playerNum );
			if ( !player || ( player == pBestEnemy ) )
				continue;
			if ( player->GetTeamNumber() == myteam )
			{
				rfSuicidingTeam.AddRecipient( player );
			}
			else if ( player->GetTeamNumber() == team )
			{
				rfGettingMoneyTeam.AddRecipient( player );
			}
		}

		UTIL_ClientPrintFilter( rfGettingMoneyTeam, HUD_PRINTTALK, "#Player_Cash_Award_ExplainSuicide_TeammateGotCash",
			CFmtStr( "#ENTNAME[%d]%s", this->entindex(), this->GetPlayerName() ), fmtDollarsEarned.Get(),
			CFmtStr( "#ENTNAME[%d]%s", pBestEnemy->entindex(), pBestEnemy->GetPlayerName() ) );
		UTIL_ClientPrintFilter( rfSuicidingTeam, HUD_PRINTTALK, "#Player_Cash_Award_ExplainSuicide_EnemyGotCash",
			CFmtStr( "#ENTNAME[%d]%s", this->entindex(), this->GetPlayerName() ) );

		// Notify spectators
		CTeamRecipientFilter teamfilter( TEAM_SPECTATOR, true );
		UTIL_ClientPrintFilter( teamfilter, HUD_PRINTTALK, "#Player_Cash_Award_ExplainSuicide_Spectators",
			CFmtStr( "#ENTNAME[%d]%s", this->entindex(), this->GetPlayerName() ), fmtDollarsEarned.Get(),
			CFmtStr( "#ENTNAME[%d]%s", pBestEnemy->entindex(), pBestEnemy->GetPlayerName() ) );
	}
}

bool CCSPlayer::DoesPlayerGetRoundStartMoney()
{
	return m_receivesMoneyNextRound;
}

CCSPlayer* CCSPlayer::Instance( int iEnt )
{
	return dynamic_cast< CCSPlayer* >( CBaseEntity::Instance( INDEXENT( iEnt ) ) );
}

bool CCSPlayer::ShouldPickupItemSilently( CBaseCombatCharacter *pNewOwner ) 
{ 
	CCSPlayer *pNewCSOwner = dynamic_cast< CCSPlayer* >( pNewOwner );
	if ( !pNewCSOwner || !CSGameRules() || CSGameRules()->IsFreezePeriod() /*|| pNewCSOwner->CanPlayerBuy( false )*/ )
		return false; // turns out that item touch calls happen in between FinishMove and the trigger touch so CanPlayerBuy always returns false in this case.....

	if ( pNewCSOwner->GetAbsVelocity().Length2D() < (CS_PLAYER_SPEED_RUN * CS_PLAYER_SPEED_WALK_MODIFIER) )
		return true;

	if ( CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() && IsBot() && (m_spawnedTime + 0.15) < gpGlobals->curtime)
		return true;

	return false; 
}

void CCSPlayer::DropC4()
{
}

bool CCSPlayer::HasDefuser()
{
	return m_bHasDefuser;
}

void CCSPlayer::RemoveDefuser()
{
	m_bHasDefuser = false;
}

void CCSPlayer::GiveDefuser( bool bPickedUp /* = false */ )
{
	if ( !m_bHasDefuser )
	{
		bool bIsSilentPickup = ShouldPickupItemSilently( this );
		IGameEvent * event = gameeventmanager->CreateEvent( "item_pickup" );
		if( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetString( "item", "defuser" );
			event->SetBool( "silent", bIsSilentPickup );
			gameeventmanager->FireEvent( event );
		}

		if ( !bIsSilentPickup )
			EmitSound( "Player.PickupWeapon" );
	}

	m_bHasDefuser = true;

	if ( !bPickedUp )
	{
		m_fLastGivenDefuserTime = gpGlobals->curtime;
	}

	// [dwenger] Added for fun-fact support
	m_bPickedUpDefuser = bPickedUp;

	RecalculateCurrentEquipmentValue();
}


// player blinded by a flashbang grenade
void CCSPlayer::Blind( float holdTime, float fadeTime, float startingAlpha )
{
	// Don't flash a spectator.
	color32 clr = {255, 255, 255, 255};

	clr.a = startingAlpha;

	// estimate when we can see again
	float oldBlindUntilTime = m_blindUntilTime;
	float oldBlindStartTime = m_blindStartTime;
	m_blindUntilTime = MAX( m_blindUntilTime, gpGlobals->curtime + holdTime + 0.5f * fadeTime );
	m_blindStartTime = gpGlobals->curtime;

	fadeTime /= 1.4f;

	if ( gpGlobals->curtime > oldBlindUntilTime )
	{
		// The previous flashbang is wearing off, or completely gone
		m_flFlashDuration = fadeTime;
		m_flFlashMaxAlpha = startingAlpha;
	}
	else
	{
		// The previous flashbang is still going strong - only extend the duration
		float remainingDuration = oldBlindStartTime + m_flFlashDuration - gpGlobals->curtime;

		float flNewDuration = Max( remainingDuration, fadeTime );

		// The flashbang client effect runs off a network var change callback... Make sure the bits for duration get
		// sent by changing it a tiny bit whenever these end up being equal.
		if ( m_flFlashDuration == flNewDuration )
			flNewDuration += 0.01f;

		m_flFlashDuration = flNewDuration;
		m_flFlashMaxAlpha = Max( m_flFlashMaxAlpha.Get(), startingAlpha );
	}


	if ( m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	{
		// Magic numbers to reduce the fade time to within 'perceptible' range.
		// Players can see well enough to shoot back somewhere around 50% white plus burn-in effect.
		// Varies by player and amount of panic ;)
		// So this makes raised arm goes down earlier, making it a better representation of actual blindness.
		float flAdjustedHold = holdTime * 0.45f;
		float flAdjustedEnd = fadeTime * 0.7f;

		//DevMsg( "Flashing. Time is: %f. Params: holdTime: %f, fadeTime: %f, alpha: %f\n", gpGlobals->curtime, holdTime, fadeTime, m_flFlashMaxAlpha );

		m_PlayerAnimStateCSGO->m_flFlashedAmountEaseOutStart = gpGlobals->curtime + flAdjustedHold;
		m_PlayerAnimStateCSGO->m_flFlashedAmountEaseOutEnd = gpGlobals->curtime + flAdjustedEnd;

		// This check moves the ease-out start and end to account for a non-255 starting alpha.
		// However it looks like starting alpha is ALWAYS 255, since no current code path seems to ever pass in less.
		if ( m_flFlashMaxAlpha < 255 )
		{
			float flScaleBack = 1.0f - (( flAdjustedEnd / 255.0f ) * m_flFlashMaxAlpha);
			m_PlayerAnimStateCSGO->m_flFlashedAmountEaseOutStart -= flScaleBack;
			m_PlayerAnimStateCSGO->m_flFlashedAmountEaseOutEnd -= flScaleBack;
		}

		// when fade out time is very soon, don't pull the arm up all the way. It looks silly and robotic.
		if ( flAdjustedEnd < 1.5f )
		{
			m_PlayerAnimStateCSGO->m_flFlashedAmountEaseOutStart -= 1.0f;
		}
	}
}

void CCSPlayer::Deafen( float flDistance )
{
	// Spectators don't get deafened
	if ( (GetObserverMode() == OBS_MODE_NONE )  ||  (GetObserverMode() == OBS_MODE_IN_EYE ) )
	{
		// dsp presets are defined in hl2/scripts/dsp_presets.txt

		int effect;

		if( flDistance < 100 )
		{
			effect = 134;
		}
		else if( flDistance < 500 )
		{
			effect = 135;
		}
		else if( flDistance < 1000 )
		{
			effect = 136;
		}
		else 
		{
			// too far for us to get an effect
			return;
		}

		CSingleUserRecipientFilter user( this );
		enginesound->SetPlayerDSP( user, effect, false );

		//TODO: bots can't hear sound for a while?
	}
}

void CCSPlayer::GiveShield( void )
{
#ifdef CS_SHIELD_ENABLED
	m_bHasShield = true;
	m_bShieldDrawn = false;

	if ( HasSecondaryWeapon() )
	{
		CBaseCombatWeapon *pWeapon = Weapon_GetSlot( WEAPON_SLOT_PISTOL );
		pWeapon->SetModel( pWeapon->GetViewModel() );
		pWeapon->Deploy();
	}

	CBaseViewModel *pVM = GetViewModel( 1 );

	if ( pVM )
	{
		ShowViewModel( true );
		pVM->RemoveEffects( EF_NODRAW );
		pVM->SetWeaponModel( SHIELD_VIEW_MODEL, GetActiveWeapon() );
		pVM->SendViewModelMatchingSequence( 1 );
	}
#endif
}

void CCSPlayer::RemoveShield( void )
{
#ifdef CS_SHIELD_ENABLED
	m_bHasShield = false;

	CBaseViewModel *pVM = GetViewModel( 1 );

	if ( pVM )
	{
		pVM->AddEffects( EF_NODRAW );
	}
#endif
}

void CCSPlayer::RemoveAllItems( bool removeSuit )
{

	//reset addon bits
	m_iAddonBits = 0;

	if( HasDefuser() )
	{
		RemoveDefuser();
	}

	if ( HasShield() )
	{
		RemoveShield();
	}

	m_bHasNightVision = false;
	m_bNightVisionOn = false;

	// [dwenger] Added for fun-fact support
	m_bPickedUpDefuser = false;
	m_bDefusedWithPickedUpKit = false;
	m_bPickedUpWeapon = false;
	m_bAttemptedDefusal = false;
	m_nPreferredGrenadeDrop = 0;

	if ( removeSuit )
	{
		m_bHasHelmet = false;
		m_bHasHeavyArmor = false;
		SetArmorValue( 0 );
	}

	BaseClass::RemoveAllItems( removeSuit );
}

void CCSPlayer::ValidateWearables( void )
{
	/** Removed for partner depot **/
}

void CCSPlayer::ObserverRoundRespawn()
{
	ClearFlashbangScreenFade();

	// did we change our name last round?
	if ( m_szNewName[0] != 0 )
	{
		// ... and force the name change now.  After this happens, the gamerules will get
		// a ClientSettingsChanged callback from the above ClientCommand, but the name
		// matches what we're setting here, so it will do nothing.
		ChangeName( m_szNewName );
		m_szNewName[0] = 0;
	}

	
	m_unRoundStartEquipmentValue = m_unCurrentEquipmentValue = 0;

	// Resets camera mode, specifically for the case where coaches are popped into 3rd to spectate a bomb
	// when it's the only valid target. Limited to coach because no one ever complained about other situations
	// where this happens.
	if ( IsCoach() )
	{
		// check mp_forcecamera settings
		if ( ( GetObserverMode() > OBS_MODE_FIXED ) )
		{
			switch ( mp_forcecamera.GetInt() )
			{
			case OBS_ALLOW_ALL	:	break;	// no restrictions
			case OBS_ALLOW_TEAM :	SetObserverMode( OBS_MODE_IN_EYE );	break;
			case OBS_ALLOW_NONE :	SetObserverMode( OBS_MODE_FIXED ); break;	// don't allow anything
			}
		}
	}
}

void CCSPlayer::RoundRespawn()
{
	if ( CSGameRules()->IsPlayingGunGame() )
	{
		bool resetScore = CSGameRules()->IsPlayingGunGameProgressive();
		Reset( resetScore );

		// Reinitialize some gun-game progressive variables

		m_bMadeFinalGunGameProgressiveKill = false;
		m_iNumGunGameKillsWithCurrentWeapon = 0;

		// Ensure the player has the proper gun-game progressive weapons
		if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{
			// Clear out weapons in progressive mode
			m_iGunGameProgressiveWeaponIndex = 0;

			// Reset the UI to default tint when we are no longer at last weapon level
			if ( !CSGameRules()->IsFinalGunGameProgressiveWeapon( m_iGunGameProgressiveWeaponIndex, GetTeamNumber() ) )
			{
				ConVarRef sf_ui_tint( "sf_ui_tint" ); 
				if ( sf_ui_tint.GetInt() == g_KnifeLevelTint )
				{
					if ( GetTeamNumber() == TEAM_TERRORIST )
						sf_ui_tint.SetValue( g_T_Tint );
					else 
						sf_ui_tint.SetValue( g_CT_Tint );
				}
			}
		}

		if ( !CSGameRules()->IsPlayingGunGameTRBomb() )
		{
			// Ensure player has the default items
			GiveDefaultItems();
		}

		if ( CSGameRules()->IsPlayingGunGameTRBomb() )
		{
			// Progress weapons over rounds for TR Bomb mode
			if ( m_bShouldProgressGunGameTRBombModeWeapon )
			{
				ResetTRBombModeWeaponProgressFlag();
				if ( CSGameRules()->GetRoundsPlayed() > 0 )
					IncrementGunGameProgressiveWeapon( 1 );
			}
		}
	}

	//MIKETODO: menus
	//if ( m_iMenu != Menu_ChooseAppearance )
	{
		// remove them from any vehicle they may be in
		if ( IsInAVehicle() )
		{
			LeaveVehicle();
		}

		if ( GetParent() )
			SetParent( NULL );

		// Put them back into the game.
		StopObserverMode();
		State_Transition( STATE_ACTIVE );

		respawn( this, false );

		m_nButtons = 0;
		SetNextThink( TICK_NEVER_THINK );
		ResetForceTeamThink();
	}

	if ( CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		// [hpe:jason] Reset the kill points after we award the upgrade, so the UI continues to show that 
		//		the next weapon level has been unlocked until we respawn for the next round.
		m_iNumGunGameTRKillPoints = 0;

	}

	m_iNumRoundKills = 0;
	m_iNumRoundKillsHeadshots = 0;
	m_iNumRoundTKs = 0;

	m_receivesMoneyNextRound = true; // reset this variable so they can receive their cash next round.
	m_iAccountMoneyEarnedForNextRound = 0;

	//If they didn't die, this will print out their damage info
	OutputDamageGiven();
	OutputDamageTaken();
	ResetDamageCounters();

	m_unRoundStartEquipmentValue = RecalculateCurrentEquipmentValue();

}

void CCSPlayer::CheckTKPunishment( void )
{
	// teamkill punishment.. 
	if ( (m_bJustKilledTeammate == true ) && mp_tkpunish.GetInt() )
	{
		m_bJustKilledTeammate = false;
		m_bPunishedForTK = true;
		CommitSuicide();
	}
}

bool CCSPlayer::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*= 0*/ )
{
	if ( IsTaunting() )
	{
		// Don't allow weapon switch while taunting
		if ( IsThirdPersonTaunt() )
			return false;

		// Stop taunting for view model look at
		StopTaunting();
	}

	if ( IsLookingAtWeapon() )
	{
		StopLookingAtWeapon();
	}

	bool bBaseClassSwitch = BaseClass::Weapon_Switch( pWeapon, viewmodelindex );

	// clear any bomb-plant force-duck when switching to any weapon
	if( bBaseClassSwitch )
		m_bDuckOverride = false;

	return bBaseClassSwitch;
}

class CCSPlayerResourcePlayer : public CCSPlayerResource { friend class CCSPlayer; };
CWeaponCSBase* CCSPlayer::GetActiveCSWeapon() const
{
	CWeaponCSBase *csWeapon = dynamic_cast< CWeaponCSBase* >( GetActiveWeapon( ) );
	/** Removed for partner depot **/
	return csWeapon; 
}

void CCSPlayer::LogTriggerPulls()
{
	if( !(m_nButtons & IN_ATTACK ) )
	{
		m_triggerPulled = false;
	}
	else if( !m_triggerPulled )
	{
		// we are pulling a trigger, and we weren't already pulling it.
		m_triggerPulled = true;
		m_triggerPulls++;
	}
}

void CCSPlayer::PreThink()
{
	BaseClass::PreThink();
	if ( m_bAutoReload )
	{
		m_bAutoReload = false;
		m_nButtons |= IN_RELOAD;
	}
	LogTriggerPulls();

	if ( m_afButtonLast != m_nButtons )
		m_flLastAction = gpGlobals->curtime;

	if ( g_fGameOver )
		return;

	// Quest update from matchmaking data
	// TODO: Valve official servers only?
	// TODO: Send quest id too?
	CCSGameRules::CQMMPlayerData_t* pQMM = NULL;
	if( m_uiAccountId && CSGameRules() )
	{
		pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId );
	}

	State_PreThink();

	if ( m_pHintMessageQueue )
		m_pHintMessageQueue->Update();

	//Reset bullet force accumulator, only lasts one frame
	m_vecTotalBulletForce = vec3_origin;

	if ( mp_autokick.GetBool() && !IsBot() && !IsHLTV() && !IsAutoKickDisabled() )
	{
		if ( this != UTIL_GetLocalPlayerOrListenServerHost() )
		{
			if ( gpGlobals->curtime - m_flLastAction > CSGameRules()->GetRoundLength() * 2 )
			{
				UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#Game_idle_kick", CFmtStr( "#ENTNAME[%d]%s", entindex(), GetPlayerName() ) );
				engine->ServerCommand( UTIL_VarArgs( "kickid_ex %d %d %s\n", GetUserID(), CSGameRules()->IsPlayingOffline() ? 0 : 1, "Player idle" ) );
				m_flLastAction = gpGlobals->curtime;
			}
		}
	}

	if ( m_flDominateEffectDelayTime > -1 && m_flDominateEffectDelayTime <= gpGlobals->curtime && m_hDominateEffectPlayer.Get() )
	{
		CCSPlayer *pOtherPlayer = dynamic_cast<CCSPlayer*>( m_hDominateEffectPlayer.Get() );
		if ( pOtherPlayer )
		{
			int victimEntIndex = pOtherPlayer->entindex();
			int killerEntIndex = entindex();

			if ( IsPlayerDominated( victimEntIndex ) )
			{
				//PlayerStats_t statsVictim = CCS_GameStats.FindPlayerStats( pOtherPlayer );	
				int iKills = CCS_GameStats.FindPlayerStats( pOtherPlayer ).statsKills.iNumKilledByUnanswered[killerEntIndex];	
				CFmtStr fmtPrintEntName( "#ENTNAME[%d]%s", pOtherPlayer->entindex(), pOtherPlayer->GetPlayerName() );
				if ( CS_KILLS_FOR_DOMINATION == iKills )
				{
					ClientPrint( this, HUD_PRINTTALK, "#Player_You_Are_Now_Dominating", fmtPrintEntName.Access() );
				}
				else
				{
					ClientPrint( this, HUD_PRINTTALK, "#Player_You_Are_Still_Dominating", fmtPrintEntName.Access() );
				}
				// Play gun game domination sound
				CRecipientFilter filter;
				filter.AddRecipient( this );
				EmitSound( filter, entindex(), "Music.GG_Dominating" );

				// have this player brag to his team about dominating someone
				Radio( "NiceShot"/*"OnARollBrag"*/ );
			}
		}		
		m_flDominateEffectDelayTime = -1;
		m_hDominateEffectPlayer = NULL;
	}

#ifndef _XBOX
	++ m_nTicksSinceLastPlaceUpdate;
	// No reason to update this every tick! Once per second is good enough
	if ( m_nTicksSinceLastPlaceUpdate > 30 )
	{	
		m_nTicksSinceLastPlaceUpdate = 0;
		// CS would like their players to continue to update their LastArea since it is displayed in the hud voice chat UI
		// But we won't do the population tracking while dead.
		// We need to check simple line of sight to make sure we don't grab nav areas from floors under us if we're slightly off
		// the nav mesh up above.
		const bool checkLOSToGround  = true;
		CNavArea *area = TheNavMesh->GetNavArea( WorldSpaceCenter(), 1000, checkLOSToGround );
		if ( area && area != m_lastNavArea )
		{
			m_lastNavArea = area;
			if ( area->GetPlace() != UNDEFINED_PLACE )
			{
				const char *placeName = TheNavMesh->PlaceToName( area->GetPlace() );
				if ( placeName && *placeName )
				{
					Q_strncpy( m_szLastPlaceName.GetForModify(), placeName, MAX_PLACE_NAME_LENGTH );
				}
			}
		}
	}
#endif
}

void CCSPlayer::MoveToNextIntroCamera()
{
	m_pIntroCamera = gEntList.FindEntityByClassname( m_pIntroCamera, "point_viewcontrol" );

	// if m_pIntroCamera is NULL we just were at end of list, start searching from start again
	if(!m_pIntroCamera )
		m_pIntroCamera = gEntList.FindEntityByClassname(m_pIntroCamera, "point_viewcontrol" );

	// find the target
	CBaseEntity *Target = NULL;
	
	if( m_pIntroCamera )
	{
		Target = gEntList.FindEntityByName( NULL, STRING(m_pIntroCamera->m_target ) );
	}

	// if we still couldn't find a camera, goto T spawn
	if(!m_pIntroCamera )
		m_pIntroCamera = gEntList.FindEntityByClassname(m_pIntroCamera, "info_player_terrorist" );

	SetViewOffset( vec3_origin );	// no view offset
	UTIL_SetSize( this, vec3_origin, vec3_origin ); // no bbox

	if( !Target ) //if there are no cameras(or the camera has no target, find a spawn point and black out the screen
	{
		if ( m_pIntroCamera.IsValid() )
			SetAbsOrigin( m_pIntroCamera->GetAbsOrigin() + VEC_VIEW );

		SetAbsAngles( QAngle( 0, 0, 0 ) );
		
		m_pIntroCamera = NULL;  // never update again
		return;
	}
	

	Vector vCamera = Target->GetAbsOrigin() - m_pIntroCamera->GetAbsOrigin();
	Vector vIntroCamera = m_pIntroCamera->GetAbsOrigin();
	
	VectorNormalize( vCamera );
		
	QAngle CamAngles;
	VectorAngles( vCamera, CamAngles );

	SetAbsOrigin( vIntroCamera );
	SetAbsAngles( CamAngles );
	SnapEyeAngles( CamAngles );
	m_fIntroCamTime = gpGlobals->curtime + 6;
}

class NotVIP
{
public:
	bool operator()( CBasePlayer *player )
	{
		CCSPlayer *csPlayer = static_cast< CCSPlayer * >(player );
		csPlayer->MakeVIP( false );

		return true;
	}
};

// Expose the VIP selection to plugins, since we don't have an official VIP mode.  This
// allows plugins to access the (limited ) VIP functionality already present (scoreboard
// identification and radar color ).
CON_COMMAND( cs_make_vip, "Marks a player as the VIP" )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() != 2 )
	{
		return;
	}

	CCSPlayer *player = static_cast< CCSPlayer * >(UTIL_PlayerByIndex( atoi( args[1] ) ) );
	if ( !player )
	{
		// Invalid value clears out VIP
		NotVIP notVIP;
		ForEachPlayer( notVIP );
		return;
	}

	player->MakeVIP( true );
}

void CCSPlayer::MakeVIP( bool isVIP )
{
	if ( isVIP )
	{
		NotVIP notVIP;
		ForEachPlayer( notVIP );
	}
	m_isVIP = isVIP;
}

bool CCSPlayer::IsVIP() const
{
	return m_isVIP;
}

void CCSPlayer::DropShield( void )
{
#ifdef CS_SHIELD_ENABLED
	//Drop an item_defuser
	Vector vForward, vRight;
	AngleVectors( GetAbsAngles(), &vForward, &vRight, NULL );
		
	RemoveShield();

	CBaseAnimating *pShield = (CBaseAnimating * )CBaseEntity::Create( "item_shield", WorldSpaceCenter(), GetLocalAngles() );	
	pShield->ApplyAbsVelocityImpulse( vForward * 200 + vRight * random->RandomFloat( -50, 50 ) );

	CBaseCombatWeapon *pActive = GetActiveWeapon();

	if ( pActive )
	{
		pActive->Deploy();
	}
#endif
}

void CCSPlayer::SetShieldDrawnState( bool bState )
{
#ifdef CS_SHIELD_ENABLED
	m_bShieldDrawn = bState;
#endif
}

bool CCSPlayer::CSWeaponDrop(CBaseCombatWeapon *pWeapon, bool bDropShield, bool bThrowForward)
{
	Vector vTossPos = WorldSpaceCenter();
	if (bThrowForward)
	{
		Vector vForward;
		AngleVectors(EyeAngles(), &vForward, NULL, NULL);
		vTossPos = vTossPos + vForward * 100;
	}
	return CSWeaponDrop( pWeapon, vTossPos, bDropShield );
}

bool CCSPlayer::CSWeaponDrop( CBaseCombatWeapon *pWeapon, Vector targetPos, bool bDropShield )
{
	bool bSuccess = false;

	CWeaponCSBase *pCSWeapon = dynamic_cast< CWeaponCSBase* >( pWeapon );

	if ( pWeapon )
		pWeapon->ShowWeaponWorldModel( false );

	if (  mp_death_drop_gun.GetInt() == 0 && pCSWeapon && !pCSWeapon->IsA( WEAPON_C4 ))
	{
		if ( pWeapon )
			UTIL_Remove( pWeapon );

		UpdateAddonBits();
		return true;
	}

	if ( HasShield() && bDropShield == true )
	{
		DropShield();
		return true;
	}

	if ( pWeapon )
	{

		Weapon_Drop( pWeapon, &targetPos, NULL );

		pWeapon->SetSolidFlags( FSOLID_NOT_STANDABLE | FSOLID_TRIGGER | FSOLID_USE_TRIGGER_BOUNDS );
		pWeapon->SetMoveCollide( MOVECOLLIDE_FLY_BOUNCE );

		if( pCSWeapon )
		{
			// Track which player was owner of this weapon ( needed to track kills with enemy weapon stats )
			pCSWeapon->AddPriorOwner( this );


			pCSWeapon->SetModel( pCSWeapon->GetWorldDroppedModel() );

			// set silencer bodygroup
			if ( pCSWeapon->HasSilencer() )
			{
				pCSWeapon->SetBodygroup( pCSWeapon->FindBodygroupByName( "silencer" ), pCSWeapon->IsSilenced() ? 0 : 1 );
			}

			//Find out the index of the ammo type
			int iAmmoIndex = pCSWeapon->GetPrimaryAmmoType();

			//If it has an ammo type, find out how much the player has
			if( iAmmoIndex != -1 )
			{
				// Check to make sure we don't have other weapons using this ammo type
				bool bAmmoTypeInUse = false;
				if ( IsAlive() && GetHealth() > 0 )
				{
					for ( int i=0; i<MAX_WEAPONS; ++i )
					{
						CBaseCombatWeapon *pOtherWeapon = GetWeapon( i );
						if ( pOtherWeapon && pOtherWeapon != pWeapon && pOtherWeapon->GetPrimaryAmmoType() == iAmmoIndex )
						{
							bAmmoTypeInUse = true;
							break;
						}
					}
				}

				if ( !bAmmoTypeInUse )
				{
					int iAmmoToDrop = GetAmmoCount( iAmmoIndex );
					
					// only add 1 ammo to dropped grenades
					if( pCSWeapon->GetWeaponType() == WEAPONTYPE_GRENADE )
						iAmmoToDrop = 0;

//					//Add this much to the dropped weapon
//					pCSWeapon->SetExtraAmmoCount( iAmmoToDrop );

					//Remove all ammo of this type from the player
					SetAmmoCount( 0, iAmmoIndex );
				}
			}

			//record this time as when this weapon was last dropped
			pCSWeapon->m_flDroppedAtTime = gpGlobals->curtime;
		}

		//=========================================
		// Teleport the weapon to the player's hand
		//=========================================
		int iBIndex = -1;
		int iWeaponBoneIndex = -1;

		MDLCACHE_CRITICAL_SECTION();


		if ( !m_bUseNewAnimstate )
		{

			// now we use the weapon_bone to drop the item from.  Previously we were incorrectly using the root position from the character
			iBIndex = LookupBone( "ValveBiped.weapon_bone" );
			iWeaponBoneIndex = pWeapon->LookupBone( "ValveBiped.weapon_bone" );

			// dkorus: If we hit this assert, the model changed and we no longer have a valid "ValveBiped.weapon_bone" to use for our weapon drop position
			//		   This code will have to change to match the new bone name
			AssertMsg( iBIndex != -1, "Missing weapon bone from player!  Make sure the bone exists and or that the string is updated." );

			if ( iBIndex == -1 || iWeaponBoneIndex == -1 )
			{
				iBIndex = LookupBone( "ValveBiped.Bip01_R_Hand" );
				iWeaponBoneIndex = 0; // use the root
			}

			if ( iBIndex != -1 )  
			{
				Vector origin;
				QAngle angles;
				matrix3x4_t transform;

				// Get the transform for the weapon bonetoworldspace in the NPC
				GetBoneTransform( iBIndex, transform );

				// find offset of root bone from origin in local space
				// Make sure we're detached from hierarchy before doing this!!!
				pWeapon->StopFollowingEntity();
				MatrixAngles( transform, angles, origin );

				pWeapon->SetAbsOrigin( Vector( 0, 0, 0 ) );
				pWeapon->SetAbsAngles( QAngle( 0, 0, 0 ) );
				pWeapon->InvalidateBoneCache();
				matrix3x4_t rootLocal;
				pWeapon->GetBoneTransform( iWeaponBoneIndex, rootLocal );

				// invert it
				matrix3x4_t rootInvLocal;
				MatrixInvert( rootLocal, rootInvLocal );

				matrix3x4_t weaponMatrix;
				ConcatTransforms( transform, rootInvLocal, weaponMatrix );
				MatrixAngles( weaponMatrix, angles, origin );

				// run a hull trace to prevent throwing guns through walls or world geometry
				trace_t trDropTrace;
				UTIL_TraceHull( EyePosition(), origin, Vector( -5, -5, -5 ), Vector( 5, 5, 5 ), MASK_SOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trDropTrace );
				if ( trDropTrace.fraction != 1.0 )
				{
					////uncomment to see debug visualization
					//debugoverlay->AddBoxOverlay( origin, Vector(-5,-5,-5), Vector(5,5,5), QAngle(0,0,0), 0,200,0,128, 4.0f );
					//debugoverlay->AddBoxOverlay( EyePosition(), Vector(-5,-5,-5), Vector(5,5,5), QAngle(0,0,0), 200,0,0,128, 4.0f );
					//debugoverlay->AddLineOverlay( EyePosition(), origin, 255,0,0, true, 4.0f );

					// move the weapon drop position to a valid point between the player's eyes (assumed valid) and their right hand (assumed invalid)
					origin -= (( origin - EyePosition() ) * trDropTrace.fraction);

					//debugoverlay->AddBoxOverlay( origin, Vector(-5,-5,-5), Vector(5,5,5), QAngle(0,0,0), 0,0,200,128, 4.0f );
				}

				pWeapon->Teleport( &origin, &angles, NULL );
			
				//Have to teleport the physics object as well
				IPhysicsObject *pWeaponPhys = pWeapon->VPhysicsGetObject();

				if( pWeaponPhys )
				{
					Vector vPos;
					QAngle vAngles;

					pWeaponPhys->GetPosition( &vPos, &vAngles );
					pWeaponPhys->SetPosition( vPos, vAngles, true );

					AngularImpulse	angImp(0,0,0 );
					Vector vecAdd = (GetAbsVelocity() * 0.5f) + Vector( 0, 0, 110 );
					pWeaponPhys->AddVelocity( &vecAdd, &angImp );
				}
			}

		}
		else
		{
			Assert( pWeapon->GetModel() );

			Vector vecWeaponThrowFromPos = EyePosition();
			QAngle angWeaponThrowFromAngle = EyeAngles();

			int nPlayerRightHandAttachment = LookupAttachment( "weapon_hand_R" );
			if ( nPlayerRightHandAttachment != -1 )
			{
				bool bAttachSuccess = GetAttachment( nPlayerRightHandAttachment, vecWeaponThrowFromPos );
				Assert( bAttachSuccess ); bAttachSuccess;
			}
			else
			{
				DevWarning( "Warning: Can't find player's right hand attachment! [weapon_hand_R]\n" );
			}

			pWeapon->StopFollowingEntity();

			// run a hull trace to prevent throwing guns through walls or world geometry
			// Note we do a conservative trace here that blocks against more stuff than is absolutely necessary
			// (we could figure out what kind of object is being thrown, and use a different trace based on that,
			// but it doesn't really matter since it will just move the drop point slightly back towards your head)
			trace_t trDropTrace;
			UTIL_TraceHull( EyePosition(), vecWeaponThrowFromPos, Vector( -5, -5, -5 ), Vector( 5, 5, 5 ), MASK_PLAYERSOLID|CONTENTS_GRENADECLIP, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trDropTrace );
			if ( trDropTrace.fraction != 1.0 )
			{

				//uncomment to see debug visualization
				//debugoverlay->AddBoxOverlay( vecWeaponThrowFromPos, Vector(-5,-5,-5), Vector(5,5,5), QAngle(0,0,0), 0,200,0,128, 4.0f );
				//debugoverlay->AddBoxOverlay( EyePosition(), Vector(-5,-5,-5), Vector(5,5,5), QAngle(0,0,0), 200,0,0,128, 4.0f );
				//debugoverlay->AddLineOverlay( EyePosition(), vecWeaponThrowFromPos, 255,0,0, true, 4.0f );

				// move the weapon drop position to a valid point between the player's eyes (assumed valid) and their right hand (assumed invalid)
				vecWeaponThrowFromPos -= (( vecWeaponThrowFromPos - EyePosition() ) * trDropTrace.fraction);
			}
			
			//debugoverlay->AddBoxOverlay( vecWeaponThrowFromPos, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 0,0,200,128, 4.0f );

			pWeapon->SetAbsOrigin( vecWeaponThrowFromPos );
			pWeapon->SetAbsAngles( angWeaponThrowFromAngle );
			
			if ( pWeapon->m_hWeaponWorldModel.Get() )
				pWeapon->Teleport( &vecWeaponThrowFromPos, &angWeaponThrowFromAngle, NULL );

			//Have to teleport the physics object as well
			IPhysicsObject *pWeaponPhys = pWeapon->VPhysicsGetObject();

			if( pWeaponPhys )
			{
				Vector vPos;
				QAngle vAngles;

				pWeaponPhys->GetPosition( &vPos, &vAngles );
				pWeaponPhys->SetPosition( vPos, vAngles, true );

				AngularImpulse	angImp(0,0,0 );
				Vector vecAdd = (GetAbsVelocity() * 0.5f) + Vector( 0, 0, 110 );
				pWeaponPhys->AddVelocity( &vecAdd, &angImp );
			}
		}
		
		bSuccess = true;
	}
	
	UpdateAddonBits();

	return bSuccess;
}


void CCSPlayer::TransferInventory( CCSPlayer* pTargetPlayer )
{
    // as part of transferring inventory, remove what WE have
	SetArmorValue( 0 );
	m_bHasHelmet = false;
	m_bHasHeavyArmor = false;
	m_bHasNightVision = false;
	m_bNightVisionOn = false;

	RemoveAllItems( true );
}

bool CCSPlayer::DropWeaponSlot( int nSlot, bool fromDeath )
{
	bool bSuccess = false;

	CWeaponCSBase *pWeapon = assert_cast< CWeaponCSBase* >( Weapon_GetSlot( nSlot ) );
	if ( pWeapon )
	{
		bSuccess = CSWeaponDrop( pWeapon, false );

		// UNDONE!
		// The idea here was to add progressive permanent wear to a weapon
		// This was a really inefficient way to deal with this in the first place
		// Also ends up creating a new weapon which caused the weapon to unequip
		/*if ( fromDeath && bSuccess )
		{
			CAttributeContainer *pAttributeContainer = pWeapon->GetAttributeContainer();
			if ( pAttributeContainer )
			{
				CEconItemView *pItem = pAttributeContainer->GetItem();
				if ( pItem )
				{
					if ( pItem->GetItemID() > 0 )
					{
						CSingleUserRecipientFilter filter( this );
						filter.MakeReliable();

						CCSUsrMsg_ItemDrop msg;
						msg.set_itemid( pItem->GetItemID() );
						msg.set_death( true );
						SendUserMessage( filter, CS_UM_ItemDrop, msg ); 
					}
				}
			}
		}*/
	}

	return bSuccess;
}


bool CCSPlayer::HasPrimaryWeapon( void )
{
	bool bSuccess = false;

	CBaseCombatWeapon *pWeapon = Weapon_GetSlot( WEAPON_SLOT_RIFLE );

	if ( pWeapon )
	{
		bSuccess = true;
	}

	return bSuccess;
}


bool CCSPlayer::HasSecondaryWeapon( void )
{
	bool bSuccess = false;

	CBaseCombatWeapon *pWeapon = Weapon_GetSlot( WEAPON_SLOT_PISTOL );
	if ( pWeapon )
	{
		bSuccess = true;
	}

	return bSuccess;
}

uint32 CCSPlayer::RecalculateCurrentEquipmentValue( void )
{
	m_unCurrentEquipmentValue = 0;
#if !defined( NO_STEAM_GAMECOORDINATOR )
	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CWeaponCSBase* pWeapon = dynamic_cast<CWeaponCSBase*>( GetWeapon( i ) );
		if ( pWeapon != NULL )
		{
			int nWeaponPrice = pWeapon->GetWeaponPrice();
			if ( !nWeaponPrice )
			{
				nWeaponPrice = GetWeaponPrice( pWeapon->GetCSWeaponID() );
			}

			if ( pWeapon->GetWeaponType() == WEAPONTYPE_GRENADE )
			{	// support for multiple grenades of same type
				int numAmmoItems = GetAmmoCount( pWeapon->GetPrimaryAmmoType() );
				if ( numAmmoItems > 1 )
				{
					nWeaponPrice *= numAmmoItems;
				}
			}

			m_unCurrentEquipmentValue += nWeaponPrice;
		}
	}

	if ( HasDefuser() )
	{
		m_unCurrentEquipmentValue += GetWeaponPrice ( ITEM_DEFUSER );
	}

	// HACK: possessing any armor is 'kevlar', posessing any armor and a helmet is 'assault suit'.
	// Similar checks like this are scattered through out the code... would be better to centralize this check.
	if ( ArmorValue() > 0 )
	{
		// Grr... Can't use GetWeaponPrice because it subtracts out what you already have-- which is what we're trying to count.
		if ( m_bHasHelmet.Get() )
		{
			m_unCurrentEquipmentValue += (uint16)ITEM_PRICE_ASSAULTSUIT;
		}
		else
		{
			m_unCurrentEquipmentValue += (uint16)ITEM_PRICE_KEVLAR;
		}
	}
#endif
	return m_unCurrentEquipmentValue;
}

void CCSPlayer::UpdateFreezetimeEndEquipmentValue( void )
{
	m_unFreezetimeEndEquipmentValue = m_unCurrentEquipmentValue;
#if !defined( NO_STEAM_GAMECOORDINATOR )
	// Match Stats
	if ( CSGameRules()->ShouldRecordMatchStats() )
	{
		int currentround = CSGameRules()->GetTotalRoundsPlayed();

		m_iMatchStats_EquipmentValue.GetForModify( currentround ) = m_unCurrentEquipmentValue;

		//	add post-freeze account balance to the match stats. This is approximately what the player is saving from this round.
		// we may lose items purchased post freezetime but we also avoid money earned from kills that shouldn't be considered 'saved'
		m_iMatchStats_MoneySaved.GetForModify( currentround ) = GetAccountBalance(); 

		// Keep track in QMM data
		if ( m_uiAccountId && CSGameRules() )
		{
			if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
			{
				pQMM->m_iMatchStats_EquipmentValue[ currentround ] = m_iMatchStats_EquipmentValue.Get( currentround );
				pQMM->m_iMatchStats_MoneySaved[ currentround ] = m_iMatchStats_MoneySaved.Get( currentround );
			}
		}
	}
#endif
}

/*void CCSPlayer::UpdateAppearanceIndex( void )
{
	static const char *pchRandomHeadName = "ctm_head_random";
	static const CCStrike15ItemDefinition *pRandomAppearanceDef = dynamic_cast< const CCStrike15ItemDefinition* >( GetItemSchema()->GetItemDefinitionByName( pchRandomHeadName ) );

	if ( IsControllingBot() && GetControlledBot() )
	{
		m_unAppearanceIndex = GetControlledBot()->m_unAppearanceIndex;
		return;
	}

	const char *pAppearance = pchRandomHeadName;

	const CCStrike15ItemDefinition *pDef = dynamic_cast< const CCStrike15ItemDefinition* >( GetItemSchema()->GetItemDefinitionByName( pAppearance ) );
	if ( !pDef || !AreSlotsConsideredIdentical( pDef->GetLoadoutSlot( GetTeamNumber() ), LOADOUT_POSITION_APPEARANCE ) )
	{
		pDef = pRandomAppearanceDef;
	}

	if ( pDef )
	{
		if ( pDef == pRandomAppearanceDef )
		{
			CUtlVector< const CCStrike15ItemDefinition* > vecAppearances;

			// Make a list of all appearances and randomly choose
			const CEconItemSchema::ItemDefinitionMap_t &mapItems = GetItemSchema()->GetItemDefinitionMap();
			FOR_EACH_MAP_FAST( mapItems, i )
			{
				const CCStrike15ItemDefinition *pItemDef = dynamic_cast< const CCStrike15ItemDefinition* >( mapItems[ i ] );
				if ( !pItemDef->IsHidden() && pItemDef != pRandomAppearanceDef && 
					 AreSlotsConsideredIdentical( pItemDef->GetLoadoutSlot( GetTeamNumber() ), LOADOUT_POSITION_APPEARANCE ) )
				{
					vecAppearances.AddToTail( pItemDef );
				}
			}

			if ( vecAppearances.Count() > 0 )
			{
				pDef = vecAppearances[ RandomInt( 0, vecAppearances.Count() - 1 ) ];
			}
		}

		m_unAppearanceIndex = pDef->GetDefinitionIndex();
	}
}*/

bool CCSPlayer::BAttemptToBuyCheckSufficientBalance( int nCostOfPurchaseToCheck, bool bClientPrint )
{
	if ( GetAccountBalance() - m_iAccountMoneyEarnedForNextRound < nCostOfPurchaseToCheck )
	{
		if ( !m_bIsInAutoBuy && !m_bIsInRebuy )
		{
			if ( ( m_iAccountMoneyEarnedForNextRound <= 0 ) || ( GetAccountBalance() < nCostOfPurchaseToCheck ) )	// simply not enough money
				ClientPrint( this, HUD_PRINTCENTER, "#Not_Enough_Money" );
			else // money has been earned that is only useful next round, inform the user about that case separately
				ClientPrint( this, HUD_PRINTCENTER, "#Not_Enough_Money_NextRound", CFmtStr( "%d", m_iAccountMoneyEarnedForNextRound ) );
		}
		return false;
	}
	else
		return true;
}

BuyResult_e CCSPlayer::AttemptToBuyVest( void )
{
	int iKevlarPrice = ITEM_PRICE_KEVLAR;

	if ( ArmorValue() >= 100 )
	{
		if( !m_bIsInAutoBuy && !m_bIsInRebuy )
			ClientPrint( this, HUD_PRINTCENTER, "#Already_Have_Kevlar" );
		return BUY_ALREADY_HAVE;
	}
	else if ( !BAttemptToBuyCheckSufficientBalance( iKevlarPrice ) )
	{
		return BUY_CANT_AFFORD;
	}
	else
	{
		if ( m_bHasHelmet ) 
		{
			if( !m_bIsInAutoBuy && !m_bIsInRebuy )
				ClientPrint( this, HUD_PRINTCENTER, "#Already_Have_Helmet_Bought_Kevlar" );
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "item_pickup" );
		if( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetString( "item", "vest" );
			event->SetBool( "silent", false );
			gameeventmanager->FireEvent( event );
		}

		EmitSound( "Player.PickupWeapon" );

		const char* szKevlarName = "item_kevlar";
		GiveNamedItem( szKevlarName );
		AddAccount( -iKevlarPrice, true, true, szKevlarName );
		return BUY_BOUGHT;
	}
}


//************************************
// Try to buy kevlar vest + helmet
//************************************
BuyResult_e CCSPlayer::AttemptToBuyAssaultSuit( void )
{
	int iPrice = GetWeaponPrice( ITEM_ASSAULTSUIT );

	// process the result
	if ( !BAttemptToBuyCheckSufficientBalance( iPrice ) )
	{			
		return BUY_CANT_AFFORD;
	}

	bool bHasFullArmor = ArmorValue() >= 100;

	// special messaging
	if( !m_bIsInAutoBuy && !m_bIsInRebuy )
	{	
		if ( bHasFullArmor && m_bHasHelmet )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Already_Have_Kevlar_Helmet" );
		}
		else if ( bHasFullArmor && !m_bHasHelmet )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Already_Have_Kevlar_Bought_Helmet" );
		}
		else if ( !bHasFullArmor && m_bHasHelmet )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Already_Have_Helmet_Bought_Kevlar" );
			iPrice = ITEM_PRICE_KEVLAR;
		}
	}
	if ( /*bHasFullArmor && */m_bHasHelmet )
	{
		return BUY_ALREADY_HAVE;
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "item_pickup" );
	if( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetString( "item", "vesthelm" );
		event->SetBool( "silent", false );
		gameeventmanager->FireEvent( event );
	}

	EmitSound( "Player.PickupWeapon" );

	const char* szAssaultSuitName = "item_assaultsuit";
	GiveNamedItem( szAssaultSuitName );
	AddAccount( -iPrice, true, true, szAssaultSuitName );
	return BUY_BOUGHT;
}

//************************************
// Try to buy kevlar vest + helmet
//************************************
BuyResult_e CCSPlayer::AttemptToBuyHeavyAssaultSuit( void )
{
	int iPrice = GetWeaponPrice( ITEM_HEAVYASSAULTSUIT );

	// process the result
	if ( !BAttemptToBuyCheckSufficientBalance( iPrice ) )
	{			
		return BUY_CANT_AFFORD;
	}

	bool bHasFullArmor = ArmorValue() >= 200;

	// special messaging
	if( !m_bIsInAutoBuy && !m_bIsInRebuy )
	{	
		if ( bHasFullArmor && m_bHasHelmet )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Already_Have_Kevlar_Helmet" );
		}
		else if ( bHasFullArmor && !m_bHasHelmet )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Already_Have_Kevlar_Bought_Helmet" );
		}
		else if ( !bHasFullArmor && m_bHasHelmet )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Already_Have_Helmet_Bought_Kevlar" );
			iPrice = ITEM_PRICE_KEVLAR;
		}
	}
// 	if ( /*bHasFullArmor && */m_bHasHelmet )
// 	{
// 		return BUY_ALREADY_HAVE;
// 	}

	IGameEvent * event = gameeventmanager->CreateEvent( "item_pickup" );
	if( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetString( "item", "heavyarmor" );
		event->SetBool( "silent", false );
		gameeventmanager->FireEvent( event );
	}

	EmitSound( "Player.PickupWeapon" );

	const char* szAssaultSuitName = "item_heavyassaultsuit";
	GiveNamedItem( szAssaultSuitName );
	AddAccount( -iPrice, true, true, szAssaultSuitName );
	return BUY_BOUGHT;
}

BuyResult_e CCSPlayer::AttemptToBuyDefuser( void )
{
	CCSGameRules *MPRules = CSGameRules();

	if( ( GetTeamNumber() == TEAM_CT ) && MPRules->IsBombDefuseMap() || MPRules->IsHostageRescueMap() )
	{
		if ( HasDefuser() )		// prevent this guy from buying more than 1 Defuse Kit
		{
			if( !m_bIsInAutoBuy && !m_bIsInRebuy )
				ClientPrint( this, HUD_PRINTCENTER, "#Already_Have_One" );
			return BUY_ALREADY_HAVE;
		}
		else if ( !BAttemptToBuyCheckSufficientBalance( ITEM_PRICE_DEFUSEKIT )  )
		{
			return BUY_CANT_AFFORD;
		}
		else
		{
			GiveDefuser(); 

//			CBroadcastRecipientFilter filter;
// 			EmitSound( filter, entindex(), "Player.PickupWeapon" );

			if ( CSGameRules()->IsHostageRescueMap() )
				AddAccount( -ITEM_PRICE_DEFUSEKIT, true, true, "item_cutters" );
			else
				AddAccount( -ITEM_PRICE_DEFUSEKIT, true, true, "item_defuser" );

			return BUY_BOUGHT;
		}
	}

	return BUY_NOT_ALLOWED;
}

BuyResult_e CCSPlayer::AttemptToBuyNightVision( void )
{
	int iNVGPrice = ITEM_PRICE_NVG;

	if ( m_bHasNightVision == TRUE )
	{
		if( !m_bIsInAutoBuy && !m_bIsInRebuy )
			ClientPrint( this, HUD_PRINTCENTER, "#Already_Have_One" );
		return BUY_ALREADY_HAVE;
	}
	else if ( !BAttemptToBuyCheckSufficientBalance( iNVGPrice ) )
	{
		return BUY_CANT_AFFORD;
	}
	else
	{
//			CBroadcastRecipientFilter filter;
// 		EmitSound( filter, entindex(), "Player.PickupWeapon" );

		m_bHasNightVision = true;

		AddAccount( -iNVGPrice, true, true, "weapon_nvg" );

		IGameEvent * event = gameeventmanager->CreateEvent( "item_pickup" );
		if( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetString( "item", "nvgs" );
			event->SetBool( "silent", false );
			gameeventmanager->FireEvent( event );
		}

		EmitSound( "Player.PickupWeapon" );

		if ( !(m_iDisplayHistoryBits & DHF_NIGHTVISION ) )
		{
			HintMessage( "#Hint_use_nightvision", false );
			m_iDisplayHistoryBits |= DHF_NIGHTVISION;
		}
		return BUY_BOUGHT;
	}
}

// Handles the special "buy" alias commands we're creating to accommodate the buy
// scripts players use (now that we've rearranged the buy menus and broken the scripts )
BuyResult_e CCSPlayer::HandleCommand_Buy( const char *item, int nPos, bool bAddToRebuy/* = true */  )
{
	bAddToRebuy = ( bAddToRebuy && !m_bIsInRebuy ); // Only addtorebuy if bAddToRebuy is default and we're not in rebuy.

	if ( StringIsEmpty( item ) )
	{
		CEconItemView *pItem = Inventory()->GetItemInLoadout( GetTeamNumber(), nPos );
		if ( pItem && pItem->IsValid() )
		{
			item = pItem->GetItemDefinition()->GetDefinitionName();
		}
	}

	BuyResult_e result = HandleCommand_Buy_Internal( item, nPos, bAddToRebuy );
	if (result == BUY_BOUGHT )
	{
		m_bMadePurchseThisRound = true;
		CCS_GameStats.IncrementStat(this, CSSTAT_ITEMS_PURCHASED, 1 );

		// strip "weapon_" from the weapon class name
		const char *item_name = item;
		if ( IsWeaponClassname( item_name ) )
		{
			item_name += WEAPON_CLASSNAME_PREFIX_LENGTH;
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "item_purchase" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetInt( "team", GetTeamNumber() );	
			event->SetString( "weapon", item_name );
			gameeventmanager->FireEvent( event );
		}

		if ( CSGameRules() && CSGameRules()->IsPlayingGunGameDeathmatch() )
			AddAccount( 9999, false, false );
	}
	return result;
}

BuyResult_e CCSPlayer::HandleCommand_Buy_Internal( const char * wpnName, int nPos, bool bAddToRebuy/* = true */ ) 
{
	char itemNameUsed[ 256 ];

	BuyResult_e result = CanPlayerBuy( false ) ? BUY_PLAYER_CANT_BUY : BUY_INVALID_ITEM; // set some defaults

	// translate the new weapon names to the old ones that are actually being used.
	wpnName = GetTranslatedWeaponAlias( wpnName );

	CEconItemView *pItem = Inventory()->GetItemInLoadoutFilteredByProhibition( GetTeamNumber(), nPos );

	CSWeaponID weaponId = WEAPON_NONE;
	const CCSWeaponInfo* pWeaponInfo = NULL;
	if ( pItem && pItem->IsValid() )
	{
		weaponId = WeaponIdFromString( pItem->GetStaticData()->GetItemClass() );
		if ( weaponId == WEAPON_NONE )
			return BUY_INVALID_ITEM;

		pWeaponInfo = GetWeaponInfo( weaponId );
	}
	else
	{
		// Since the loadout pos we got is invalid, make sure that this item isn't in the item schema before giving it by name. Fixes exploit of bypassing loadout with "buy m4a1 1"
		// and still allows buying of gear such as kevlar, assaultsuit, etc.
		char wpnClassName[MAX_WEAPON_STRING];
		wpnClassName[0] = '\0';
		V_sprintf_safe( wpnClassName, "weapon_%s", wpnName );
		V_strlower( wpnClassName );

		if ( !GetItemSchema()->GetItemDefinitionByName( wpnClassName ) || !( CSGameRules() && CSGameRules()->IsPlayingClassic() ) )
		{
			weaponId = AliasToWeaponID(wpnName );
			pWeaponInfo = GetWeaponInfo( weaponId );
		}
	}

	if ( pWeaponInfo == NULL )
	{
		if ( Q_stricmp( itemNameUsed, "primammo" ) == 0 )
		{
			result = AttemptToBuyAmmo( 0 );
		}
		else if ( Q_stricmp( itemNameUsed, "secammo" ) == 0 )
		{
			result = AttemptToBuyAmmo( 1 );
		}
	}
	else
	{
		if( !CanPlayerBuy( true ) )
		{
			return BUY_PLAYER_CANT_BUY;
		}

		AcquireResult::Type acquireResult = CanAcquire( weaponId, AcquireMethod::Buy, pItem );
		switch ( acquireResult )
		{
		case AcquireResult::Allowed:
			break;

		case AcquireResult::AlreadyOwned:
		case AcquireResult::ReachedGrenadeTotalLimit:
		case AcquireResult::ReachedGrenadeTypeLimit:
			if( !m_bIsInAutoBuy && !m_bIsInRebuy )
				ClientPrint( this, HUD_PRINTCENTER, "#Cannot_Carry_Anymore" );
			return BUY_ALREADY_HAVE;

		case AcquireResult::NotAllowedByTeam:
			if ( !m_bIsInAutoBuy && !m_bIsInRebuy && pWeaponInfo->GetWrongTeamMsg()[0] != 0 )
			{
				ClientPrint( this, HUD_PRINTCENTER, "#Alias_Not_Avail", pWeaponInfo->GetWrongTeamMsg() );
			}
			return BUY_NOT_ALLOWED;


		case AcquireResult::NotAllowedByProhibition:

			return BUY_NOT_ALLOWED;

		default:
			// other unhandled reason
			return BUY_NOT_ALLOWED;
		}

		BuyResult_e equipResult = BUY_INVALID_ITEM;

		if ( weaponId == ITEM_KEVLAR )
		{
			equipResult = AttemptToBuyVest();
		}
		else if ( weaponId == ITEM_ASSAULTSUIT  )
		{
			equipResult = AttemptToBuyAssaultSuit();
		}
		else if ( weaponId == ITEM_HEAVYASSAULTSUIT )
		{
			equipResult = AttemptToBuyHeavyAssaultSuit();
		}	
		else if ( weaponId == ITEM_DEFUSER || weaponId == ITEM_CUTTERS )
		{
			equipResult = AttemptToBuyDefuser();
		}
		else if ( weaponId == ITEM_NVG )
		{
			equipResult = AttemptToBuyNightVision();
		}

		if ( equipResult != BUY_INVALID_ITEM )
		{
			if ( equipResult == BUY_BOUGHT )
			{
				if ( bAddToRebuy )
				{
					itemid_t ullItemID = 0;
					if ( pItem )
					{
						pItem->GetItemID() != 0 ? pItem->GetItemID() : pItem->GetFauxItemIDFromDefinitionIndex();
					}
					else
					{
						CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pWeaponInfo->szClassName );
						if ( pDef )
						{
							ullItemID = CombinedItemIdMakeFromDefIndexAndPaint( pDef->GetDefinitionIndex(), 0 );
						}
					}

					AddToRebuy( weaponId, ullItemID );
				}
				m_iWeaponPurchasesThisRound.GetForModify(weaponId)++;
				COMPILE_TIME_ASSERT( WEAPON_MAX < MAX_WEAPONS );
			}
			return equipResult; // intentional early return here
		}

		bool bPurchase = false;

		// do they have enough money?
		if ( !BAttemptToBuyCheckSufficientBalance( pWeaponInfo->GetWeaponPrice( pItem ) ) )
		{
			return BUY_CANT_AFFORD;
		}
		else // essentially means: ( GetAccountBalance() >= pWeaponInfo->GetWeaponPrice( pItem ) )
		{
			if ( m_lifeState != LIFE_DEAD )
			{
				if ( pWeaponInfo->iSlot == WEAPON_SLOT_PISTOL )
				{
					DropWeaponSlot( WEAPON_SLOT_PISTOL );
				}
				else if ( pWeaponInfo->iSlot == WEAPON_SLOT_RIFLE )
				{
					DropWeaponSlot( WEAPON_SLOT_RIFLE );
				}
			}

			bPurchase = true;
		}

		if ( HasShield() )
		{
			if ( pWeaponInfo->CanBeUsedWithShield() == false )
			{
				return BUY_NOT_ALLOWED;
			}
		}

		if ( bPurchase )
		{
			result = BUY_BOUGHT;

			if ( pWeaponInfo->iSlot == WEAPON_SLOT_PISTOL )
				m_bUsingDefaultPistol = false;

// 			if ( IsGrenadeWeapon( weaponId ) )
// 			{
//			CBroadcastRecipientFilter filter;
// 				EmitSound( filter, entindex(), "Player.PickupWeapon" );
// 			}

			// when all is said and done, regardless of what weapon came down the pipe, give the player the one that
			// is appropriate for the loadout slot that the player is attempting to purchase for.

			int slot = -1;

			// get the slot from the pItem directly or from the pWeaponInfo if there is no pItem
			if ( !pItem || !pItem->IsValid() )
			{
				CEconItemDefinition * pItemDef = GetItemSchema()->GetItemDefinitionByName( pWeaponInfo->szClassName );

				if ( pItemDef )
					slot = ((CCStrike15ItemDefinition*)pItemDef)->GetLoadoutSlot( GetTeamNumber() ) ;
				else
					result = BUY_INVALID_ITEM;
			}
			else
			{
				slot = pItem->GetItemDefinition()->GetLoadoutSlot( GetTeamNumber() );
			}
 
 			CEconItemView *pResultItem = ( result == BUY_BOUGHT )
				? Inventory()->GetItemInLoadoutFilteredByProhibition( GetTeamNumber(), slot ) : NULL;
			if ( !pResultItem || !pResultItem->IsValid() ) 
			{
				result = BUY_INVALID_ITEM;
			}
			else
			{
				const char * szWeaponName = pResultItem->GetItemDefinition( )->GetDefinitionName( );

				bool bWeaponMismatch = false;
				const CEconItemDefinition * pOriginalItemDef = NULL;

				///////////////////////////////////////////////////////////////////////////////////////////////////////////
				// Use the cache of purchases to determine what item to give the player
				// The cache guarantees that the inventory that the player starts with is the one that's guaranteed to them
				// throughout the match
				//
				if ( CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && !CSGameRules()->IsWarmupPeriod() )
				{
					if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
					{
						int nTeamArrayIndex = GetTeamNumber() - 2;
						
						if ( ( nTeamArrayIndex != TEAM_TERRORIST_BASE0 ) && ( nTeamArrayIndex != TEAM_CT_BASE0 ) )
						{
							Assert( 0 );
						}
						else
						{
							CCSGameRules::CQMMPlayerData_t::LoadoutSlotToDefIndexMap_t::IndexType_t idx = pQMM->m_mapLoadoutSlotToItem[ nTeamArrayIndex ].Find( slot );
							if ( idx != CCSGameRules::CQMMPlayerData_t::LoadoutSlotToDefIndexMap_t::InvalidIndex() )
							{
								item_definition_index_t un16DefIndex = pQMM->m_mapLoadoutSlotToItem[ nTeamArrayIndex ].Element( idx );

								if ( pResultItem->GetItemDefinition()->GetDefinitionIndex() != un16DefIndex )
								{
									// cached item does not match attempted purchase. give the base item instead of the one asked for.
									pResultItem = NULL;
									pOriginalItemDef = GetItemSchema( )->GetItemDefinition( un16DefIndex );
									szWeaponName = pOriginalItemDef ? pOriginalItemDef->GetDefinitionName( ) : "";
									bWeaponMismatch = true;
								}
							}
							else if ( Inventory()->InventoryRetrievedFromSteamAtLeastOnce() ) // player hasn't bought a weapon from this slot yet. Cache it (but only if we got valid inventory from Steam)
							{
								pQMM->m_mapLoadoutSlotToItem[ nTeamArrayIndex ].Insert( slot, pResultItem->GetItemDefinition()->GetDefinitionIndex() );
							}
						}
					}
				}

				// if the item didn't match, first look up the price in the item definition

				if ( bWeaponMismatch )
				{
					int price = 0;

					if ( pOriginalItemDef )
					{
						KeyValues *pkvAttrib = pOriginalItemDef->GetRawDefinition( )->FindKey( "attributes" );
						if ( pkvAttrib )
						{
							price = V_atoi( pkvAttrib->GetString( "in game price" ) );
						}
					}

					// this item had no price override in schema attributes so use the legacy class price from weaponinfo
					if ( price == 0 )
						price = pWeaponInfo->GetWeaponPrice( );

					if ( !BAttemptToBuyCheckSufficientBalance( price ) )
					{
						result = BUY_CANT_AFFORD;
					}
					else // essentially means: (  GetAccountBalance( ) >= price )
					{
						AddAccount( -price, true, true, szWeaponName );
						GiveNamedItem( szWeaponName, 0 );
					}
				}
				else
				{
					AddAccount( -pWeaponInfo->GetWeaponPrice( pItem ), true, true, szWeaponName );
					GiveNamedItem( szWeaponName, 0, pResultItem );
				}
			}
		}
	}

	if ( result == BUY_BOUGHT )
	{
		if ( bAddToRebuy )
		{
			AddToRebuy( weaponId, nPos );
		}
		m_iWeaponPurchasesThisRound.GetForModify(weaponId)++;
	}

	return result;
}

void CCSPlayer::SetBuyMenuOpen( bool bOpen ) 
{ 
	m_bIsBuyMenuOpen = bOpen; 
	extern ConVar mp_buy_during_immunity;

	if ( CanBuyDuringImmunity() )
	{
		if ( bOpen )
			m_fImmuneToGunGameDamageTime += 10;
		else
			m_fImmuneToGunGameDamageTime -= 10;
	}
}

BuyResult_e CCSPlayer::BuyGunAmmo( CBaseCombatWeapon *pWeapon, bool bBlinkMoney )
{
	if ( !CanPlayerBuy( false ) )
	{
		return BUY_PLAYER_CANT_BUY;
	}

	// Ensure that the weapon uses ammo
	int nAmmo = pWeapon->GetPrimaryAmmoType();
	if ( nAmmo == -1 )
	{
		return BUY_ALREADY_HAVE;
	}

	// Can only buy if the player does not already have full ammo
	int maxcarry = pWeapon->GetReserveAmmoMax( AMMO_POSITION_PRIMARY );

	if ( pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) >= maxcarry )
	{
		return BUY_ALREADY_HAVE;
	}

	// Purchase the ammo if the player has enough money
	if ( GetAccountBalance() >= GetCSAmmoDef()->GetCost( nAmmo ) )
	{
		GiveAmmo( GetCSAmmoDef()->GetBuySize( nAmmo ), nAmmo, true );
		AddAccount( -GetCSAmmoDef()->GetCost( nAmmo ), true, true, GetCSAmmoDef()->GetAmmoOfIndex( nAmmo )->pName  );
		return BUY_BOUGHT;
	}

	if ( bBlinkMoney )
	{
		// Not enough money.. let the player know
		if( !m_bIsInAutoBuy && !m_bIsInRebuy )
					ClientPrint( this, HUD_PRINTCENTER, "#Not_Enough_Money" );
	}

	return BUY_CANT_AFFORD;
}


BuyResult_e CCSPlayer::BuyAmmo( int nSlot, bool bBlinkMoney )
{
	if ( !CanPlayerBuy( false ) )
	{
		return BUY_PLAYER_CANT_BUY;
	}

	if ( nSlot < 0 || nSlot > 1 )
	{
		return BUY_INVALID_ITEM;
	}

	// Buy one ammo clip for all weapons in the given slot
	//
	//  nSlot == 1 : Primary weapons
	//  nSlot == 2 : Secondary weapons
	
	CBaseCombatWeapon *pSlot = Weapon_GetSlot( nSlot );
	if ( !pSlot )
		return BUY_INVALID_ITEM;
	
	//MIKETODO: shield.
	//if ( player->HasShield() && player->m_rgpPlayerItems[2] )
	//	 pItem = player->m_rgpPlayerItems[2];

	return BuyGunAmmo( pSlot, bBlinkMoney );
}


BuyResult_e CCSPlayer::AttemptToBuyAmmo( int iAmmoType )
{
	Assert( iAmmoType == 0 || iAmmoType == 1 );

	BuyResult_e result = BuyAmmo( iAmmoType, true );

	if ( result == BUY_BOUGHT )
	{
		while ( BuyAmmo( iAmmoType, false ) == BUY_BOUGHT )
		{
			// empty loop - keep buying
		}

		return BUY_BOUGHT;
	}

	return result;
}

BuyResult_e CCSPlayer::AttemptToBuyAmmoSingle( int iAmmoType )
{
	return BuyAmmo( iAmmoType, true );
}

void CCSPlayer::InternalAutoBuyAmmo( int nSlot )
{
	CBaseCombatWeapon *pWeapon = Weapon_GetSlot( nSlot );
	if ( pWeapon )
	{
		int nAmmo = pWeapon->GetPrimaryAmmoType();
		if ( nAmmo != -1 )
		{
			int maxAmmo = pWeapon->GetReserveAmmoMax( AMMO_POSITION_PRIMARY );

			if ( pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) < maxAmmo )
			{
				bool playRearmSound = true;

				do
				{
					pWeapon->GiveReserveAmmo( AMMO_POSITION_PRIMARY, GetCSAmmoDef()->GetBuySize( nAmmo ), playRearmSound );
					playRearmSound = false;
				} 
				while( pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) < maxAmmo );
			}
		}
	}
}


void CCSPlayer::AutoBuyAmmo( bool bForce )
{ 
	if ( (sv_autobuyammo.GetBool() || bForce) && ( m_flNextAutoBuyAmmoTime < gpGlobals->curtime ) && (CanPlayerBuy( false ) || CSGameRules()->IsPlayingCoopMission()) )
	{
		// only allow this check to happen every 2 seconds.
		m_flNextAutoBuyAmmoTime = gpGlobals->curtime + SECONDS_BETWEEN_AUTOAMMOBUY_CHECKS - fmodf( gpGlobals->curtime - m_flNextAutoBuyAmmoTime, SECONDS_BETWEEN_AUTOAMMOBUY_CHECKS );

		InternalAutoBuyAmmo( WEAPON_SLOT_PISTOL );
		InternalAutoBuyAmmo( WEAPON_SLOT_RIFLE );
	}
}

void CCSPlayer::GuardianForceFillAmmo( void )
{
	if ( CSGameRules()->IsPlayingCoopGuardian() )
	{
		InternalAutoBuyAmmo( WEAPON_SLOT_PISTOL );
		InternalAutoBuyAmmo( WEAPON_SLOT_RIFLE );
	}
}

const char *RadioEventName[ RADIO_NUM_EVENTS+1 ] =
{
	"RADIO_INVALID",

	"EVENT_START_RADIO_1",

	"EVENT_RADIO_GO_GO_GO",
	"EVENT_RADIO_TEAM_FALL_BACK",
	"EVENT_RADIO_STICK_TOGETHER_TEAM",
	"EVENT_RADIO_HOLD_THIS_POSITION",
	"EVENT_RADIO_FOLLOW_ME",

	"EVENT_START_RADIO_2",

	"EVENT_RADIO_AFFIRMATIVE",
	"EVENT_RADIO_NEGATIVE",
	"EVENT_RADIO_CHEER",
	"EVENT_RADIO_COMPLIMENT",
	"EVENT_RADIO_THANKS",

	"EVENT_START_RADIO_3",

	"EVENT_RADIO_ENEMY_SPOTTED",
	"EVENT_RADIO_NEED_BACKUP",
	"EVENT_RADIO_YOU_TAKE_THE_POINT",
	"EVENT_RADIO_SECTOR_CLEAR",
	"EVENT_RADIO_IN_POSITION",

	// unused
	"EVENT_RADIO_COVER_ME",
	"EVENT_RADIO_REGROUP_TEAM",
	"EVENT_RADIO_TAKING_FIRE",
	"EVENT_RADIO_REPORT_IN_TEAM",
	"EVENT_RADIO_REPORTING_IN",
	"EVENT_RADIO_GET_OUT_OF_THERE",
	"EVENT_RADIO_ENEMY_DOWN",
	"EVENT_RADIO_STORM_THE_FRONT",

	"EVENT_RADIO_END",

	NULL		// must be NULL-terminated
};


/**
 * Convert name to RadioType
 */
RadioType NameToRadioEvent( const char *name )
{
	for( int i=0; RadioEventName[i]; ++i )
		if (!stricmp( RadioEventName[i], name ) )
			return static_cast<RadioType>( i );

	return RADIO_INVALID;
}


void CCSPlayer::HandleMenu_Radio1( int slot )
{
	if( m_iRadioMessages < 0 )
		return;

	if( m_flRadioTime > gpGlobals->curtime )
		return;

	m_iRadioMessages--;
	m_flRadioTime = gpGlobals->curtime + 1.5;

	switch ( slot )
	{
		case RADIO_COVER_ME :
			Radio( "Radio.CoverMe",   "#Cstrike_TitlesTXT_Cover_me" );
			break;

		case RADIO_YOU_TAKE_THE_POINT :
			Radio( "Radio.YouTakeThePoint", "#Cstrike_TitlesTXT_You_take_the_point" );
			break;

		case RADIO_HOLD_THIS_POSITION :
			Radio( "Radio.HoldPosition",  "#Cstrike_TitlesTXT_Hold_this_position" );
			break;

		case RADIO_REGROUP_TEAM :
			Radio( "Radio.Regroup",   "#Cstrike_TitlesTXT_Regroup_team" );
			break;

		case RADIO_FOLLOW_ME :
			Radio( "Radio.FollowMe",  "#Cstrike_TitlesTXT_Follow_me" );
			break;

		case RADIO_TAKING_FIRE :
			Radio( "Radio.TakingFire", "#Cstrike_TitlesTXT_Taking_fire" );
			break;
	}

	// tell bots about radio message
	IGameEvent * event = gameeventmanager->CreateEvent( "player_radio" );
	if ( event )
	{
		event->SetInt("userid", GetUserID() );
		event->SetInt("slot", slot );
		gameeventmanager->FireEvent( event );
	}
}

void CCSPlayer::HandleMenu_Radio2( int slot )
{
	if( m_iRadioMessages < 0 )
		return;

	if( m_flRadioTime > gpGlobals->curtime )
		return;

	m_iRadioMessages--;
	m_flRadioTime = gpGlobals->curtime + 1.5;

	switch ( slot )
	{
		case RADIO_GO_GO_GO :
			Radio( "Radio.GoGoGo",			"#Cstrike_TitlesTXT_Go_go_go" );
			break;

		case RADIO_TEAM_FALL_BACK :
			Radio( "Radio.TeamFallBack",	"#Cstrike_TitlesTXT_Team_fall_back" );
			break;

		case RADIO_STICK_TOGETHER_TEAM :
			Radio( "Radio.StickTogether",	"#Cstrike_TitlesTXT_Stick_together_team" );
			break;

		case RADIO_THANKS :
			Radio( "Radio.Thanks",   "#Cstrike_TitlesTXT_Thanks" );
			break;

		case RADIO_CHEER :
			Radio( "Radio.Cheer",		"#Cstrike_TitlesTXT_Cheer" );
			break;

		case RADIO_COMPLIMENT :
			Radio( "Radio.Compliment",		"#Cstrike_TitlesTXT_Compliment" );
			break;

		case RADIO_REPORT_IN_TEAM :
			Radio( "Radio.ReportInTeam",	"#Cstrike_TitlesTXT_Report_in_team" );
			break;
	}

	// tell bots about radio message
	IGameEvent * event = gameeventmanager->CreateEvent( "player_radio" );
	if ( event )
	{
		event->SetInt("userid", GetUserID() );
		event->SetInt("slot", slot );
		gameeventmanager->FireEvent( event );
	}
}

void CCSPlayer::HandleMenu_Radio3( int slot )
{
	if( m_iRadioMessages < 0 )
		return;

	if( m_flRadioTime > gpGlobals->curtime )
		return;

	m_iRadioMessages--;
	m_flRadioTime = gpGlobals->curtime + 1.5;

	switch ( slot )
	{
		case RADIO_AFFIRMATIVE :
			if ( random->RandomInt( 0,1 ) )
				Radio( "Radio.Affirmitive",	"#Cstrike_TitlesTXT_Affirmative" );
			else 
				Radio( "Radio.Roger",		"#Cstrike_TitlesTXT_Roger_that" ); 

			break;

		case RADIO_ENEMY_SPOTTED :
			Radio( "Radio.EnemySpotted",	"#Cstrike_TitlesTXT_Enemy_spotted" );
			break;

		case RADIO_NEED_BACKUP :
			Radio( "Radio.NeedBackup",		"#Cstrike_TitlesTXT_Need_backup" );
			break;

		case RADIO_SECTOR_CLEAR :
			Radio( "Radio.SectorClear",		"#Cstrike_TitlesTXT_Sector_clear" );
			break;

		case RADIO_IN_POSITION :
			Radio( "Radio.InPosition",		"#Cstrike_TitlesTXT_In_position" );
			break;

		case RADIO_REPORTING_IN :
			Radio( "Radio.ReportingIn",		"#Cstrike_TitlesTXT_Reporting_in" );
			break;

		case RADIO_GET_OUT_OF_THERE :
			Radio( "Radio.GetOutOfThere",	"#Cstrike_TitlesTXT_Get_out_of_there" );
			break;

		case RADIO_NEGATIVE :
			Radio( "Radio.Negative",		"#Cstrike_TitlesTXT_Negative" );
			break;

		case RADIO_ENEMY_DOWN :
			Radio( "Radio.EnemyDown",		"#Cstrike_TitlesTXT_Enemy_down" );
			break;
	}

	// tell bots about radio message
	IGameEvent * event = gameeventmanager->CreateEvent( "player_radio" );
	if ( event )
	{
		event->SetInt("userid", GetUserID() );
		event->SetInt("slot", slot );
		gameeventmanager->FireEvent( event );
	}
}

void UTIL_CSRadioMessage( IRecipientFilter& filter, int iClient, int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL )
{
	CCSUsrMsg_RadioText msg;

	msg.set_msg_dst( msg_dest );
	msg.set_client( iClient );
	msg.set_msg_name( msg_name );

	if ( param1 )
		msg.add_params( param1 );
	else
		msg.add_params( "" );

	if ( param2 )
		msg.add_params( param2 );
	else
		msg.add_params( "" );

	if ( param3 )
		msg.add_params( param3 );
	else
		msg.add_params( "" );

	if ( param4 )
		msg.add_params( param4 );
	else
		msg.add_params( "" );

	SendUserMessage( filter, CS_UM_RadioText, msg );		
}

void CCSPlayer::ConstructRadioFilter( CRecipientFilter& filter )
{
	filter.MakeReliable();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CCSPlayer *player = static_cast<CCSPlayer *>( UTIL_PlayerByIndex( i ) );
		if ( !player )
			continue;

		if ( player->IsHLTV() )
		{
			if ( tv_relayradio.GetBool() )
				filter.AddRecipient( player );
			else
				continue;
		}
		else
		{
			// Skip players ignoring the radio
			if ( player->m_bIgnoreRadio )
				continue;

			bool bTeamOnly = true;
#if defined ( CSTRIKE15 )
			if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() )
				bTeamOnly = false;
#endif
			if ( CSGameRules()->CanPlayerHearTalker( player, this, bTeamOnly ) )
				filter.AddRecipient( player );
		}
	}
}

void CCSPlayer::Radio( const char *pszRadioSound, const char *pszRadioText, bool bTriggeredAutomatically )
{
	if ( !IsAlive() )
		return;

	if ( IsObserver() )
		return;

	if ( CSGameRules() && CSGameRules()->IsPlayingTraining() )
		return;

	CRecipientFilter filter;
	ConstructRadioFilter( filter );

	// Don't show radio messages in chat for terrorists in coop missions
	bool bHideRadioChatText = ( CSGameRules()->IsPlayingCoopMission() && GetTeamNumber() == TEAM_TERRORIST );

	if ( pszRadioText && !bHideRadioChatText )
	{
		CFmtStr fmtPrintEntName( "#ENTNAME[%d]%s", entindex(), GetPlayerName() );
		const char *pszLocationText = CSGameRules()->GetChatLocation( true, this );
		if ( pszLocationText && *pszLocationText )
		{
			UTIL_CSRadioMessage( filter, entindex(), HUD_PRINTTALK, "#Game_radio_location", fmtPrintEntName.Access(), pszLocationText, pszRadioText, (bTriggeredAutomatically ? "auto" : "") );
		}
		else
		{
			UTIL_CSRadioMessage( filter, entindex(), HUD_PRINTTALK, "#Game_radio", fmtPrintEntName.Access(), pszRadioText, "", (bTriggeredAutomatically ? "auto" : "") );
		}
	}

	if ( bot_chatter_use_rr.GetBool() )
	{
		AIConcept_t concept( pszRadioSound );

		AI_CriteriaSet botCriteria;
		if ( IsBot() )
		{
			if ( CSGameRules()->IsPlayingCoopMission() )
			{
				botCriteria.AppendCriteria( "gamemode", "coop" );
			}
			if ( HasHeavyArmor() )
			{
				botCriteria.AppendCriteria( "isheavy", 1 );
			}
		}	

		Speak( concept, &botCriteria, NULL, 0, &filter );
	}
	else
	{
		CCSUsrMsg_SendAudio msg;
		msg.set_radio_sound( pszRadioSound );
		SendUserMessage ( filter, CS_UM_SendAudio, msg );
	}

	//icon over the head for teammates
	if ( !CSGameRules()->IsPlayingCoopMission() )
		TE_RadioIcon( filter, 0.0, this );
}

//-----------------------------------------------------------------------------
// Purpose: Outputs currently connected players to the console
//-----------------------------------------------------------------------------
void CCSPlayer::ListPlayers()
{
	char buf[64];
	for ( int i=1; i <= gpGlobals->maxClients; i++ )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			if ( pPlayer->IsBot() )
			{
				Q_snprintf( buf, sizeof(buf ), "B %d : %s", pPlayer->GetUserID(), pPlayer->GetPlayerName() );
			}
			else
			{
				Q_snprintf( buf, sizeof(buf ), "  %d : %s", pPlayer->GetUserID(), pPlayer->GetPlayerName() );
			}
			ClientPrint( this, HUD_PRINTCONSOLE, buf );
		}
	}
	ClientPrint( this, HUD_PRINTCONSOLE, "\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
void CCSPlayer::OnDamagedByExplosion( const CTakeDamageInfo &info )
{
	float lastDamage = info.GetDamage();

	//Adrian - This is hacky since we might have been damaged by something else
	//but since the round is ending, who cares.
	if ( CSGameRules()->m_bTargetBombed == true )
		 return;

	float distanceFromPlayer = 9999.0f;

	CBaseEntity *inflictor = info.GetInflictor();
	if ( inflictor )
	{
		Vector delta = GetAbsOrigin() - inflictor->GetAbsOrigin();
		distanceFromPlayer = delta.Length();
	}

	bool shock = lastDamage >= 30.0f;

	if ( !shock )
		return;

	m_applyDeafnessTime = gpGlobals->curtime + 0.3;
	m_currentDeafnessFilter = 0;
}

void CCSPlayer::ApplyDeafnessEffect()
{
	// what's happening here is that the low-pass filter and the oscillator frequency effects need
	// to fade in and out slowly.  So we have several filters that we switch between to achieve this
	// effect.  The first 3rd of the total effect will be the "fade in" of the effect.  Which means going
	// from filter to filter from the first to the last.  Then we keep on the "last" filter for another
	// third of the total effect time.  Then the last third of the time we go back from the last filter
	// to the first.  Clear as mud?

	// glossary:
	//  filter: an individual filter as defined in dsp_presets.txt
	//  section: one of the sections for the total effect, fade in, full, fade out are the possible sections
	//  effect: the total effect of combining all the sections, the whole of what the player hears from start to finish.

	const int firstGrenadeFilterIndex = 137;
	const int lastGrenadeFilterIndex = 139;
	const float grenadeEffectLengthInSecs = 4.5f; // time of the total effect
	const float fadeInSectionTime = 0.1f;
	const float fadeOutSectionTime = 1.5f;

	const float timeForEachFilterInFadeIn = fadeInSectionTime / (lastGrenadeFilterIndex - firstGrenadeFilterIndex );
	const float timeForEachFilterInFadeOut = fadeOutSectionTime / (lastGrenadeFilterIndex - firstGrenadeFilterIndex );

	float timeIntoEffect = gpGlobals->curtime - m_applyDeafnessTime;

	if (timeIntoEffect >= grenadeEffectLengthInSecs )
	{
		// the effect is done, so reset the deafness variables.
		m_applyDeafnessTime = 0.0f;
		m_currentDeafnessFilter = 0;
		return;
	}

	int section = 0;

	if (timeIntoEffect < fadeInSectionTime )
	{
		section = 0;
	}
	else if (timeIntoEffect < (grenadeEffectLengthInSecs - fadeOutSectionTime ) )
	{
		section = 1;
	}
	else
	{
		section = 2;
	}

	int filterToUse = 0;

	if (section == 0 )
	{
		// fade into the effect.
		int filterIndex = (int )(timeIntoEffect / timeForEachFilterInFadeIn );
		filterToUse = filterIndex += firstGrenadeFilterIndex;
	}
	else if (section == 1 )
	{
		// in full effect.
		filterToUse = lastGrenadeFilterIndex;
	}
	else if (section == 2 )
	{
		// fade out of the effect
		float timeIntoSection = timeIntoEffect - (grenadeEffectLengthInSecs - fadeOutSectionTime );
		int filterIndex = (int )(timeIntoSection / timeForEachFilterInFadeOut );
		filterToUse = lastGrenadeFilterIndex - filterIndex - 1;
	}

	if (filterToUse != m_currentDeafnessFilter )
	{
		m_currentDeafnessFilter = filterToUse;

		CSingleUserRecipientFilter user( this );
		enginesound->SetPlayerDSP( user, m_currentDeafnessFilter, false );
	}
}


void CCSPlayer::NoteWeaponFired()
{
	Assert( m_pCurrentCommand );
	if( m_pCurrentCommand )
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}

	// Remember the tickcount when the weapon was fired and lock viewangles here!
	if ( m_iLockViewanglesTickNumber != gpGlobals->tickcount )
	{
		m_iLockViewanglesTickNumber = gpGlobals->tickcount;
		m_qangLockViewangles = pl.v_angle;
	}
}


bool CCSPlayer::WantsLagCompensationOnEntity( const CBaseEntity *entity, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const
{
	// No need to lag compensate at all if we're not attacking in this command and
	// we haven't attacked recently.
	if ( !( pCmd->buttons & IN_ATTACK ) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5 ) )
	{
		if ( ( pCmd->buttons & (IN_ATTACK2 | IN_ZOOM) ) == 0 )
			return false;

		CWeaponCSBase *weapon = GetActiveCSWeapon();
		if ( !weapon )
			return false;

		if ( weapon->GetCSWeaponID() != WEAPON_KNIFE )
			return false;	// IN_ATTACK2 with WEAPON_KNIFE should do lag compensation
	}

	return BaseClass::WantsLagCompensationOnEntity( entity, pCmd, pEntityTransmitBits );
}

// Handles the special "radio" alias commands we're creating to accommodate the scripts players use
// ** Returns true if we've handled the command **
bool HandleRadioAliasCommands( CCSPlayer *pPlayer, const char *pszCommand )
{
	bool bRetVal = false;

	// don't execute them if we are not alive or are an observer
	if( !pPlayer->IsAlive() || pPlayer->IsObserver() )
		return false;

	// Radio1 commands
	if ( FStrEq( pszCommand, "coverme" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio1( RADIO_COVER_ME );
	}
	else if ( FStrEq( pszCommand, "takepoint" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio1( RADIO_YOU_TAKE_THE_POINT );
	}
	else if ( FStrEq( pszCommand, "holdpos" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio1( RADIO_HOLD_THIS_POSITION );	
	}
	else if ( FStrEq( pszCommand, "regroup" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio1( RADIO_REGROUP_TEAM );	
	}
	else if ( FStrEq( pszCommand, "followme" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio1( RADIO_FOLLOW_ME );	
	}
	else if ( FStrEq( pszCommand, "takingfire" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio1( RADIO_TAKING_FIRE );	
	}
	// Radio2 commands
	else if ( FStrEq( pszCommand, "go" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio2( RADIO_GO_GO_GO );
	}
	else if ( FStrEq( pszCommand, "fallback" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio2( RADIO_TEAM_FALL_BACK );
	}
	else if ( FStrEq( pszCommand, "sticktog" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio2( RADIO_STICK_TOGETHER_TEAM );
	}
	else if ( FStrEq( pszCommand, "cheer" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio2( RADIO_CHEER );
	}
	else if ( FStrEq( pszCommand, "thanks" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio2( RADIO_THANKS );
	}
	else if ( FStrEq( pszCommand, "compliment" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio2( RADIO_COMPLIMENT );
	}
	//else if ( FStrEq( pszCommand, "getinpos" ) )
	//{
	//	bRetVal = true;
	//	pPlayer->HandleMenu_Radio2( 4 );
	//}
	//else if ( FStrEq( pszCommand, "stormfront" ) )
	//{
	//	bRetVal = true;
	//	pPlayer->HandleMenu_Radio2( 5 );
	//}
	else if ( FStrEq( pszCommand, "report" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio2( RADIO_REPORT_IN_TEAM );
	}
	// Radio3 commands
	else if ( FStrEq( pszCommand, "roger" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio3( RADIO_AFFIRMATIVE );
	}
	else if ( FStrEq( pszCommand, "enemyspot" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio3( RADIO_ENEMY_SPOTTED );
	}
	else if ( FStrEq( pszCommand, "needbackup" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio3( RADIO_NEED_BACKUP );
	}
	else if ( FStrEq( pszCommand, "sectorclear" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio3( RADIO_SECTOR_CLEAR);
	}
	else if ( FStrEq( pszCommand, "inposition" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio3( RADIO_IN_POSITION );
	}
	else if ( FStrEq( pszCommand, "reportingin" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio3( RADIO_REPORTING_IN );
	}
	else if ( FStrEq( pszCommand, "getout" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio3( RADIO_GET_OUT_OF_THERE );
	}
	else if ( FStrEq( pszCommand, "negative" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio3( RADIO_NEGATIVE );
	}
	else if ( FStrEq( pszCommand, "enemydown" ) )
	{
		bRetVal = true;
		pPlayer->HandleMenu_Radio3( RADIO_ENEMY_DOWN );
	}

	return bRetVal;
}

bool CCSPlayer::ShouldRunRateLimitedCommand( const CCommand &args )
{
	const char *pcmd = args[0];

	int i = m_RateLimitLastCommandTimes.Find( pcmd );
	if ( i == m_RateLimitLastCommandTimes.InvalidIndex() )
	{
		m_RateLimitLastCommandTimes.Insert( pcmd, gpGlobals->curtime );
		return true;
	}
	else if ( (gpGlobals->curtime - m_RateLimitLastCommandTimes[i] ) < CS_COMMAND_MAX_RATE )
	{
		// Too fast.
		return false;
	}
	else
	{
		m_RateLimitLastCommandTimes[i] = gpGlobals->curtime;
		return true;
	}
}

bool CCSPlayer::ClientCommand( const CCommand &args )
{
	const char *pcmd = args[0];

	// Bots mimic our client commands.
/*
	if ( bot_mimic.GetInt() && !( GetFlags() & FL_FAKECLIENT ) )
	{
		for ( int i=1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer != this && ( pPlayer->GetFlags() & FL_FAKECLIENT ) )
			{
				pPlayer->ClientCommand( pcmd );
			}
		}
	}
*/
	static ConVarRef sv_mmqueue_reservation( "sv_mmqueue_reservation" );
	const bool cbIsMatchmaking = sv_mmqueue_reservation.GetString()[ 0 ] == 'Q';

#if defined ( DEBUG )

	if ( FStrEq( pcmd, "bot_cmd" ) )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( UTIL_PlayerByIndex( atoi( args[1] ) ) );
		if ( pPlayer && pPlayer != this && ( pPlayer->GetFlags() & FL_FAKECLIENT ) )
		{
			CCommand botArgs( args.ArgC() - 2, &args.ArgV()[2], kCommandSrcCode );
			pPlayer->ClientCommand( botArgs );
			pPlayer->RemoveEffects( EF_NODRAW );
		}
		return true;
	}

	if ( FStrEq( pcmd, "blind" ) )
	{
		if ( ShouldRunRateLimitedCommand( args ) )
		{
			if ( args.ArgC() == 3 )
			{
				Blind( atof( args[1] ), atof( args[2] ) );
			}
			else
			{
				ClientPrint( this, HUD_PRINTCONSOLE, "usage: blind holdtime fadetime\n" );
			}
		}
		return true;
	}

	if ( FStrEq( pcmd, "deafen" ) )
	{
		Deafen( 0.0f );
		return true;
	}

	if ( FStrEq( pcmd, "he_deafen" ) )
	{
		m_applyDeafnessTime = gpGlobals->curtime + 0.3;
		m_currentDeafnessFilter = 0;
		return true;
	}

	if ( FStrEq( pcmd, "hint_reset" ) )
	{
		m_iDisplayHistoryBits = 0;
		return true;
	}

	if ( FStrEq( pcmd, "punch" ) )
	{
		float flDamage = 100;

		QAngle punchAngle = GetViewPunchAngle();

		punchAngle.x = flDamage * random->RandomFloat ( -0.15, 0.15 );
		punchAngle.y = flDamage * random->RandomFloat ( -0.15, 0.15 );
		punchAngle.z = flDamage * random->RandomFloat ( -0.15, 0.15 );

		clamp( punchAngle.x, -4, punchAngle.x );
		clamp( punchAngle.y, -5, 5 );
		clamp( punchAngle.z, -5, 5 );

		// +y == down
		// +x == left
		// +z == roll clockwise
		if ( args.ArgC() == 4 )
		{
			punchAngle.x = atof(args[1] );
			punchAngle.y = atof(args[2] );
			punchAngle.z = atof(args[3] );
		}

		SetViewPunchAngle( punchAngle );

		return true;
	}
	
#endif //DEBUG
	
	if ( FStrEq( pcmd, "jointeam" ) ) 
	{
		// Players can spam this at the wrong time to get into states we don't want them in. Ignore those cases.
		if ( ( !IsBot() && !IsHLTV() ) && ( !m_bHasSeenJoinGame || cbIsMatchmaking ) )
			return true;

		if ( args.ArgC() < 2 )
		{
			Warning( "Player sent bad jointeam syntax\n" );
		}

		if ( ShouldRunRateLimitedCommand( args ) )
		{
			int iTeam = atoi( args[1] );
			bool bForceChange = false;

			if ( args.ArgC() > 2)
			{
				bForceChange = atoi( args[2] ) > 0;
			}

			if ( m_fForceTeam == -1.0f || gpGlobals->curtime < m_fForceTeam )
			{
				HandleCommand_JoinTeam( iTeam, !bForceChange );
			}
		}
		return true;
	}
	// [dwenger] - reset the team info (used only in support of the "back" option
	// from the class choice menu ).
	else if ( FStrEq( pcmd, "resetteam" ) )
	{
		// Players can spam this at the wrong time to get into states we don't want them in. Ignore those cases.
		if ( ( !IsBot() && !IsHLTV() ) && ( !m_bHasSeenJoinGame || cbIsMatchmaking ) )
			return true;

		m_bTeamChanged = false;
		m_iOldTeam = TEAM_UNASSIGNED;
		m_iClass = (int )CS_CLASS_NONE;

		return true;
	}
	else if ( FStrEq( pcmd, "spectate" ) )
	{
		// Players can spam this at the wrong time to get into states we don't want them in. Ignore those cases.
		if ( ( !IsBot() && !IsHLTV() ) && ( !m_bHasSeenJoinGame || cbIsMatchmaking ) )
			return true;

		if ( ShouldRunRateLimitedCommand( args ) )
		{
			// instantly join spectators
			HandleCommand_JoinTeam( TEAM_SPECTATOR );
		}

		return true;
	}

	else if ( FStrEq( pcmd, "coach" ) )
	{
		// Players can spam this at the wrong time to get into states we don't want them in. Ignore those cases.
		if ( ( !IsBot() && !IsHLTV() ) && ( !m_bHasSeenJoinGame || cbIsMatchmaking ) )
			return true;

		if ( sv_coaching_enabled.GetBool() && args.ArgC() == 2 )
		{
			if ( !CSGameRules()->IsFreezePeriod() && !CSGameRules()->IsWarmupPeriod() )
			{
				Msg( "You can only choose to coach a team during warmup or freeze time.\n" );

				return true;
			}

			CReliableBroadcastRecipientFilter filter;

			if ( FStrEq( args[1], "ct") || FStrEq( args[1], "t") )
			{
				if ( ShouldRunRateLimitedCommand( args ) )
				{
					// already coaching CTs
					if ( FStrEq( args[1], "ct") )
					{
						if ( GetCoachingTeam() == TEAM_CT )
						{
							return true;
						}

						HandleCommand_JoinTeam( TEAM_SPECTATOR, false, TEAM_CT );

						UTIL_SayText2Filter( filter, this, kEUtilSayTextMessageType_AllChat, "#CSGO_Coach_Join_CT", GetPlayerName() );
					}
					else if ( FStrEq( args[1], "t") )
					{
						// already coaching Ts
						if ( GetCoachingTeam() == TEAM_TERRORIST )
						{
							return true;
						}

						HandleCommand_JoinTeam( TEAM_SPECTATOR, false, TEAM_TERRORIST );

						UTIL_SayText2Filter( filter, this, kEUtilSayTextMessageType_AllChat, "#CSGO_Coach_Join_T", GetPlayerName() );
					}

					SetObserverMode( OBS_MODE_IN_EYE );
				}
			}
			else
			{
				Msg( "Type either 'coach ct' or 'coach t'.\n" );

				m_iCoachingTeam = 0;
			}
		}
		else if ( !sv_coaching_enabled.GetBool() )
		{
			Msg( "Coaching is disabled. The server needs to set sv_coaching_enabled 1.\n" );

			m_iCoachingTeam = 0;
		}
		else if ( args.ArgC() != 2 )
		{
			Msg( "Type either 'coach ct' or 'coach t'.\n" );

			m_iCoachingTeam = 0;

		}

		return true;	
	}

	else if ( FStrEq( pcmd, "joingame" ) )
	{
		// player just closed MOTD dialog
		if ( m_iPlayerState == STATE_WELCOME )
		{
			if ( !CSGameRules() )
				return false;

			SetContextThink( &CBasePlayer::PlayerForceTeamThink, gpGlobals->curtime + 0.5f, CS_FORCE_TEAM_THINK_CONTEXT );
			int nAutoJoinTeam = 0;
			if ( CSGameRules() && CSGameRules()->IsPlayingTraining() )
				nAutoJoinTeam = TEAM_CT;
			if ( !IsBot() && CSGameRules()->IsPlayingCoopMission() )
				nAutoJoinTeam = TEAM_CT;	

			if ( CSGameRules()->IsPlayingCoopGuardian() )
			{
				if ( CSGameRules()->IsHostageRescueMap() )
				{
					nAutoJoinTeam = TEAM_TERRORIST;
				}
				else
				{
					nAutoJoinTeam = TEAM_CT;
				}
			}
			if ( nAutoJoinTeam != 0 )
			{
				HandleCommand_JoinTeam( nAutoJoinTeam, false );
			}
			else
			{
				// When playing queued matchmaking we will just automatically join the correct team
				if ( cbIsMatchmaking )
					HandleCommand_JoinTeam( TEAM_CT, false ); // join team code will actually auto-adjust the team based on server queue reservation
				else
					State_Transition( STATE_PICKINGTEAM );
			}

			// Let other commands know they may now be legal.
			m_bHasSeenJoinGame = true;
		}
		
		return true;
	}
//  	else if ( FStrEq( pcmd, "joinclass" ) ) 
//  	{
//  		if ( ShouldRunRateLimitedCommand( args ) && ( !g_pGameRules->IgnorePlayerKillCommand() || !IsAlive() ) )
//  		{
//  			HandleCommand_JoinClass();
//  		}
//  		return true;
// 	}
	else if ( FStrEq( pcmd, "drop" ) )
	{
		if ( !AttemptToBuyDMBonusWeapon() ) // Overload 'drop' when in deathmatch.
			HandleDropWeapon();

		return true;
	}
	else if ( FStrEq( pcmd, "buy" ) )
	{
		BuyResult_e result = BUY_INVALID_ITEM;
		if ( args.ArgC() == 3 )
		{
			int nPos = atoi( args[2] );
			result = HandleCommand_Buy( args[1], nPos );
		}
		else if ( args.ArgC() == 2 )
		{
			AutoBuyInfoStruct * commandInfo = GetAutoBuyCommandInfo( args[1] );

			if ( commandInfo )
				result = HandleCommand_Buy( args[1], commandInfo->m_LoadoutPosition );
		}
		if ( ( result == BUY_INVALID_ITEM ) && ShouldRunRateLimitedCommand( args ) ) // rate limit spew
		{
			// Print out a message on the console
			int msg_dest = HUD_PRINTCONSOLE;

			ClientPrint( this, msg_dest, "usage: buy <item>\n" );

			ClientPrint( this, msg_dest, "  p228\n" );
			ClientPrint( this, msg_dest, "  glock\n" );
			ClientPrint( this, msg_dest, "  scout\n" );
			ClientPrint( this, msg_dest, "  xm1014\n" );
			ClientPrint( this, msg_dest, "  mac10\n" );
			ClientPrint( this, msg_dest, "  aug\n" );
			ClientPrint( this, msg_dest, "  elite\n" );
			ClientPrint( this, msg_dest, "  fiveseven\n" );
			ClientPrint( this, msg_dest, "  ump45\n" );
			ClientPrint( this, msg_dest, "  sg550\n" );
			ClientPrint( this, msg_dest, "  galil\n" );
			ClientPrint( this, msg_dest, "  galilar\n" );
			ClientPrint( this, msg_dest, "  famas\n" );
			ClientPrint( this, msg_dest, "  usp_silencer\n" );
			ClientPrint( this, msg_dest, "  awp\n" );
			ClientPrint( this, msg_dest, "  mp5navy\n" );
			ClientPrint( this, msg_dest, "  m249\n" );
			ClientPrint( this, msg_dest, "  nova\n" );
			ClientPrint( this, msg_dest, "  m4a1\n" );
			ClientPrint( this, msg_dest, "  m4a1_silencer\n" );
			ClientPrint( this, msg_dest, "  tmp\n" );
			ClientPrint( this, msg_dest, "  g3sg1\n" );
			ClientPrint( this, msg_dest, "  deagle\n" );
			ClientPrint( this, msg_dest, "  sg552\n" );
			ClientPrint( this, msg_dest, "  ak47\n" );
			ClientPrint( this, msg_dest, "  p90\n" );
			ClientPrint( this, msg_dest, "  bizon\n" );
			ClientPrint( this, msg_dest, "  mag7\n" );
			ClientPrint( this, msg_dest, "  negev\n" );
			ClientPrint( this, msg_dest, "  sawedoff\n" );
			ClientPrint( this, msg_dest, "  tec9\n" );
			ClientPrint( this, msg_dest, "  taser\n" );
			ClientPrint( this, msg_dest, "  hkp2000\n" );
			ClientPrint( this, msg_dest, "  mp7\n" );
			ClientPrint( this, msg_dest, "  mp9\n" );
			ClientPrint( this, msg_dest, "  nova\n" );
			ClientPrint( this, msg_dest, "  p250\n" );
			ClientPrint( this, msg_dest, "  scar17\n" );
			ClientPrint( this, msg_dest, "  scar20\n" );
			ClientPrint( this, msg_dest, "  sg556\n" );
			ClientPrint( this, msg_dest, "  ssg08\n" );

			ClientPrint( this, msg_dest, "  flashbang\n" );
			ClientPrint( this, msg_dest, "  smokegrenade\n" );
			ClientPrint( this, msg_dest, "  hegrenade\n" );
			ClientPrint( this, msg_dest, "  molotov\n" );
			ClientPrint( this, msg_dest, "  decoy\n" );

			ClientPrint( this, msg_dest, "  primammo\n" );
			ClientPrint( this, msg_dest, "  secammo\n" );
		}
				
		return true;
	}
	else if ( FStrEq( pcmd, "autobuy" ) )
	{
		// hijack autobuy for when money isnt relevant and we want random weapons instead, such as deathmatch.
		if ( CSGameRules()->IsPlayingGunGameDeathmatch() )
		{
			if ( IsBuyMenuOpen() )
			{
				BuyRandom();
			}
			else
			{
				engine->ClientCommand( edict(), "dm_togglerandomweapons" );
			}
		}
		else
		{
			AutoBuy( ( args.ArgC() > 1 ) ? args.Arg( 1 ) : "" );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "rebuy" ) )
	{
		Rebuy( (args.ArgC() > 1) ? args.Arg( 1 ) : "" );
		return true;
	}
	else if ( FStrEq( pcmd, "open_buymenu" ) )
	{
		SetBuyMenuOpen( true );			
		return true;
	}
	else if ( FStrEq( pcmd, "close_buymenu" ) )
	{
		SetBuyMenuOpen( false );			
		return true;
	}
// 	else if ( FStrEq( pcmd, "buyammo1" ) )
// 	{
// 		AttemptToBuyAmmoSingle(0 );			
// 		return true;
// 	}
// 	else if ( FStrEq( pcmd, "buyammo2" ) )
// 	{
// 		AttemptToBuyAmmoSingle(1 );			
// 		return true;
// 	}
	else if ( FStrEq( pcmd, "mute" ) )
	{		
		// This is only used in scaleform (by the scoreboard), 
		// but there is an error if the binding doesn't have a handler in code.
		// So we simply add a handler to to eat the command here.
		return true;
	}
	else if ( FStrEq( pcmd, "nightvision" ) )
	{
		if ( ShouldRunRateLimitedCommand( args ) )
		{
			if( m_bHasNightVision )
			{
				if( m_bNightVisionOn )
				{
					CBroadcastRecipientFilter filter;
					EmitSound( filter, entindex(), "Player.NightVisionOff" );
				}
				else
				{
					CBroadcastRecipientFilter filter;
					EmitSound( filter, entindex(), "Player.NightVisionOn" );
				}

				m_bNightVisionOn = !m_bNightVisionOn;
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "extendfreeze" ) )
	{
		if ( ShouldRunRateLimitedCommand( args ) && CSGameRules() && !CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
		{
			m_flDeathTime += 2.0f;
		}
		return true;
	}
	else if ( FStrEq( pcmd, "menuselect" ) )
	{
		return true;
	}
	else if ( HandleRadioAliasCommands( this, pcmd ) )
	{
		return true;
	}
	else if ( FStrEq( pcmd, "listplayers" ) )
	{
		if ( ShouldRunRateLimitedCommand( args ) )
			ListPlayers();
		return true;
	}

	else if ( FStrEq( pcmd, "ignorerad" ) )
	{
		if ( ShouldRunRateLimitedCommand( args ) )
		{
			m_bIgnoreRadio = !m_bIgnoreRadio;
			if ( m_bIgnoreRadio )
			{
				ClientPrint( this, HUD_PRINTTALK, "#Ignore_Radio" );
			}
			else
			{
				ClientPrint( this, HUD_PRINTTALK, "#Accept_Radio" );
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "become_vip" ) )
	{
		//MIKETODO: VIP mode
		/*
		if ( ( CSGameRules()->m_iMapHasVIPSafetyZone == 1 ) && ( m_iTeam == TEAM_CT ) )
		{
			mp->AddToVIPQueue( this );
		}
		*/
		return true;
	}
	else if ( FStrEq( pcmd, "spec_next" ) || FStrEq( pcmd, "spec_prev" ) || FStrEq( pcmd, "spec_grenade" ) || FStrEq( pcmd, "spec_scoreboard" ) )
	{
		if ( GetTeamNumber() == TEAM_SPECTATOR )
		{
			// Showing the scoreboard should ensure a spectating player doesn't go AFK
			m_flLastAction = gpGlobals->curtime;
		}
		return true; // we handled this command, don't spam console
	}
	else if ( FStrEq( pcmd, "endmatch_votenextmap" ) )
	{
		if ( GetTeamNumber() == TEAM_SPECTATOR )
			return false;

		int index = atoi( args[1] );

		CCSPlayerResource *pResource = dynamic_cast< CCSPlayerResource * >( g_pPlayerResource );
		if ( !pResource || pResource->EndMatchNextMapAllVoted() || ( m_nEndMatchNextMapVote > -1 ) ||
			( index < 0 ) || ( index >= MAX_ENDMATCH_VOTE_PANELS ) || !CSGameRules() ||
			!CSGameRules()->IsEndMatchVotingForNextMap() || ( CSGameRules()->m_nEndMatchMapGroupVoteOptions[index] == -1 ) )
		{
			CSingleUserRecipientFilter filter( this );
			EmitSound( filter, entindex(), "Vote.Failed" );
		}
		else
		{
			// TODO: make a sound or message when you try to vote twice?
			m_nEndMatchNextMapVote = index;
		}

		return true;
	}
	else if ( FStrEq( pcmd, "+taunt" ) )
	{
		m_bIsHoldingTaunt = true;

		if ( ShouldRunRateLimitedCommand( args ) )
		{
			Taunt();
		}

		return true;
	}
	else if ( FStrEq( pcmd, "-taunt" ) )
	{
		m_bIsHoldingTaunt = false;

		return true;
	}
	else if ( FStrEq( pcmd, "+lookatweapon" ) )
	{
		m_bIsHoldingLookAtWeapon = true;

		if ( ShouldRunRateLimitedCommand( args ) )
		{
			LookAtHeldWeapon();
		}

		return true;
	}
	else if ( FStrEq( pcmd, "-lookatweapon" ) )
	{
		m_bIsHoldingLookAtWeapon = false;

		return true;
	}

	return BaseClass::ClientCommand( args );
}

bool CCSPlayer::AllowTaunts( void )
{
	// Check to see if we are in water (above our waist).
	if ( GetWaterLevel() > WL_Waist )
		return false;

	// Check to see if we are on the ground.
	if ( GetGroundEntity() == NULL )
		return false;

	return true;
}

void CCSPlayer::Taunt( void )
{
	return;
	/*
	if ( IsTaunting() )
		return;

	// We always allow first person taunts, but 3rd person is more restricted
	m_bIsThirdPersonTaunt = AllowTaunts() && CSGameRules() && CSGameRules()->AllowTaunts();

	int nSequence = ACTIVITY_NOT_AVAILABLE;

	// Need a weapon to taunt
	CWeaponCSBase *pActiveWeapon = GetActiveCSWeapon();
	if ( !pActiveWeapon )
		return;

	// Can't taunt while zoomed
	if ( pActiveWeapon->IsZoomed() )
		return;

	//if ( m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	//{
	//	// just hardcode the team-specific taunts for now
	//	m_flTauntEndTime = m_PlayerAnimStateCSGO->AttemptTauntAnimation( 
	//		GetTeamNumber() == TEAM_TERRORIST ? "taunt_fistswing_pump" : "taunt_salute"
	//		);
	//
	//	m_bIsTaunting = ( m_flTauntEndTime > gpGlobals->curtime );
	//}
	//else
	{
		// Do cool 3rd person taunt if this weapon allows it
		CEconItemView *pItemView = pActiveWeapon->GetAttributeContainer()->GetItem();
		if ( !pItemView->IsValid() )
			return;

		const CEconTauntDefinition *pTauntDef = GetItemSchema()->GetTauntDefinition( pItemView->GetTauntID() );
		if ( !pTauntDef )
			return;

		const char *pchTauntSequenceName = "";
		pchTauntSequenceName = pTauntDef->GetSequenceName();
		if ( StringIsEmpty( pchTauntSequenceName ) )
			return;

		nSequence = LookupSequence( pchTauntSequenceName );
		if ( nSequence == ACTIVITY_NOT_AVAILABLE )
			return;

		m_bIsTaunting = true;
		m_flTauntEndTime = gpGlobals->curtime + SequenceDuration( nSequence );
	}

	
	//	m_flTauntYaw = BodyAngles().y + 160.0f;
	// m_bMustNotMoveDuringTaunt = false;
	*/
}

void CCSPlayer::LookAtHeldWeapon( void )
{
	if ( IsTaunting() || IsLookingAtWeapon() )
		return;

	int nSequence = ACTIVITY_NOT_AVAILABLE;

	// Need a weapon to taunt
	CWeaponCSBase *pActiveWeapon = GetActiveCSWeapon();
	if ( !pActiveWeapon )
		return;

	// Can't taunt while zoomed, reloading, or switching silencer
	if ( pActiveWeapon->IsZoomed() || pActiveWeapon->m_bInReload || pActiveWeapon->IsSwitchingSilencer() )
		return;

	// don't let me inspect a shotgun that's reloading
	if ( pActiveWeapon->GetWeaponType() == WEAPONTYPE_SHOTGUN && pActiveWeapon->GetShotgunReloadState() != 0 )
	{
		return;
	}

#ifdef IRONSIGHT
	if ( pActiveWeapon->m_iIronSightMode == IronSight_should_approach_sighted )
		return;
#endif

	CBaseViewModel *pViewModel = GetViewModel();
	if ( pViewModel )
	{
		nSequence = pViewModel->SelectWeightedSequence( ACT_VM_IDLE_LOWERED );

		if ( nSequence == ACT_INVALID )
			nSequence = pViewModel->LookupSequence( "lookat01" );

		// make sure the silencer bodygroup is correct
		if ( GetActiveCSWeapon() && GetActiveCSWeapon()->HasSilencer() )
		{
			pViewModel->SetBodygroup( pViewModel->FindBodygroupByName( "silencer" ), GetActiveCSWeapon()->IsSilenced() ? 0 : 1 );
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "inspect_weapon" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			gameeventmanager->FireEvent( event );
		}

		// UNDONE: Nothing happens if the weapon has no lookat
		/*if ( nSequence == ACTIVITY_NOT_AVAILABLE )
		{
			// Fallback to draw since not all the lookats have been created
			nSequence = pViewModel->LookupSequence( "draw" );
		}*/

		if ( nSequence != ACTIVITY_NOT_AVAILABLE )
		{
			m_flLookWeaponEndTime = gpGlobals->curtime + pViewModel->SequenceDuration( nSequence );
			m_bIsLookingAtWeapon = true;

			pViewModel->ForceCycle( 0 );
			pViewModel->ResetSequence( nSequence );
		}
	}

}

// called by the jointeam to command to let the client know the reason
// for the jointeam having failed
void CCSPlayer::SendJoinTeamFailedMessage( int reason, bool raiseTeamScreen )
{
	IGameEvent * event = gameeventmanager->CreateEvent( "jointeam_failed" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "reason", reason );
		gameeventmanager->FireEvent( event );
	}
	
	if ( raiseTeamScreen && IsHLTV() == false )
	{
		ShowViewPortPanel( PANEL_TEAM );
	}
	
}



// returns true if the selection has been handled and the player's menu 
// can be closed...false if the menu should be displayed again
// if bQueue is true then the team move will not occur until round restart
bool CCSPlayer::HandleCommand_JoinTeam( int team, bool bQueue, int iCoachTeam  )
{
	//coaching
	if ( ( m_iCoachingTeam != 0 ) && ( iCoachTeam == 0 ) )
	{
		CReliableBroadcastRecipientFilter filter;
		UTIL_SayText2Filter( filter, this, kEUtilSayTextMessageType_AllChat, "#CSGO_No_Longer_Coach", GetPlayerName() );
	}
	m_iCoachingTeam = iCoachTeam;

	// prevent suicide for deranking
	if ( g_pGameRules->IgnorePlayerKillCommand() && IsAlive() ) 
		return false;

	if ( CSGameRules()->IsPlayingCoopGuardian() )
	{
		int nTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
		if ( GetTeamNumber() == nTeam )
			return false;
	}

	if ( CSGameRules()->IsPlayingCooperativeGametype() )
	{
		int nTeam = TEAM_CT;
		if ( GetTeamNumber() == nTeam )
			return false;
	}

	if ( IsAbleToInstantRespawn() )
	{
		bQueue = false;
	}

	if ( !IsBot() )
	{
		if  (( team == TEAM_UNASSIGNED ) && ( m_iLastTeam == TEAM_SPECTATOR ) && (GetTeamNumber() != TEAM_SPECTATOR) )
		{
			// Default the player to the spectator team since the player was a spectator during the last match
			team = TEAM_SPECTATOR;
		}
	}

	CCSGameRules *mp = CSGameRules();

	//
	// When playing queued matchmaking we force team assignment based on GC configuration
	//
	static ConVarRef sv_mmqueue_reservation( "sv_mmqueue_reservation" );
	if ( !IsBot() && ( sv_mmqueue_reservation.GetString()[0] == 'Q' ) )
	{
		CSteamID const *pThisSteamId = engine->GetClientSteamID( this->edict() );
		int numTotalPlayers = 0;
		int idxThisPlayer = -1;
		for ( char const *pszPrev = sv_mmqueue_reservation.GetString(), *pszNext = pszPrev;
			( pszNext = strchr( pszPrev, '[' ) ) != NULL; pszPrev = pszNext + 1 )
		{
			uint32 uiAccountId = 0;
			sscanf( pszNext, "[%x]", &uiAccountId );
			if ( uiAccountId && pThisSteamId && ( pThisSteamId->GetAccountID() == uiAccountId ) )
			{
				idxThisPlayer = numTotalPlayers;
			}
			++ numTotalPlayers;
		}

		if ( idxThisPlayer == -1 )
		{
			if ( strstr( sv_mmqueue_reservation.GetString(), CFmtStr( "{%x}", pThisSteamId ? pThisSteamId->GetAccountID() : 0 ) ) )
				idxThisPlayer = -2;

			team = TEAM_SPECTATOR;
			DevMsg( "Forcing spectator team for %s player/%d (%x - %s).\n", ( ( idxThisPlayer == -2 ) ? "caster" : "UNKNOWN" ),
				numTotalPlayers, ( pThisSteamId ? pThisSteamId->GetAccountID() : 0 ),
				GetPlayerName() );
		}
		else
		{
			bool bThisPlayerIsFirstTeam = ( idxThisPlayer < ( numTotalPlayers / 2 ) );
			bool bAreTeamsSwitched = mp ? mp->AreTeamsPlayingSwitchedSides() : false;

			// Force team according to queue reservation
			team = ( bThisPlayerIsFirstTeam == !bAreTeamsSwitched ) ? TEAM_CT : TEAM_TERRORIST;

			// In cooperative mode use the map type
			if ( mp )
			{
				if ( mp->IsPlayingCoopGuardian() )
				{
					if ( mp->IsHostageRescueMap() )
						team = TEAM_TERRORIST;
					else
						team = TEAM_CT;
				}
				else if ( mp->IsPlayingCoopMission() )
				{
					team = TEAM_CT;
				}
			}

			DevMsg( "Team for queue player#%d/%d (%x - %s) on %s team for %s teams: %s\n",
				idxThisPlayer+1, numTotalPlayers, ( pThisSteamId ? pThisSteamId->GetAccountID() : 0 ),
				GetPlayerName(), bThisPlayerIsFirstTeam ? "first" : "second", bAreTeamsSwitched ? "switched" : "normal",
				( (team == TEAM_CT) ? "CT" : "T" ) );

			// Mark player as having connected once
			if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( pThisSteamId->GetAccountID() ) )
			{
				if ( !pQMM->m_bEverFullyConnected )
				{
					pQMM->m_bEverFullyConnected = true;
				}
			}
		}
	}
	else if ( mp && ( ( mp->GetGamePhase() == GAMEPHASE_WARMUP_ROUND ) || mp->IsWarmupPeriod() ) )
	{
		// Allow immediate team changes in warmup
		bQueue = false;
	}

	if ( !GetGlobalTeam( team ) )
	{
		DevWarning( "HandleCommand_JoinTeam( %d ) - invalid team index.\n", team );
		return false;
	}

	// If this player is a VIP, don't allow him to switch teams/appearances unless the following conditions are met :
	// a ) There is another TEAM_CT player who is in the queue to be a VIP
	// b ) This player is dead

	//MIKETODO: handle this when doing VIP mode
	/*
	if ( m_bIsVIP == true )
	{
		if ( !IsDead() )
		{	
			ClientPrint( this, HUD_PRINTCENTER, "#Cannot_Switch_From_VIP" );	
			MenuReset();
			return true;
		}
		else if ( mp->IsVIPQueueEmpty() == true )
		{
			ClientPrint( this, HUD_PRINTCENTER, "#Cannot_Switch_From_VIP" );
			MenuReset();
			return true;
		}			
	}
	
	//MIKETODO: VIP mode
	
	case 3:
		if ( ( mp->m_iMapHasVIPSafetyZone == 1 ) && ( m_iTeam == TEAM_CT ) )
		{
			mp->AddToVIPQueue( player );
			MenuReset();
			return true;
		}
		else
		{
			return false;
		}
		break;
	*/

	int currTeam = GetTeamNumber();

	// If we already died and changed teams once, deny
	if ( bQueue && ( ( GetPendingTeamNumber() != GetTeamNumber() ) || m_bJustBecameSpectator ) )
	{
		SendJoinTeamFailedMessage( TeamJoinFailedReason::CHANGED_TOO_OFTEN, true );
		return true;
	}

	// check if we're limited in our team selection
	if ( team == TEAM_UNASSIGNED && !IsBot() )
	{
		team = mp->GetHumanTeam(); // returns TEAM_UNASSIGNED if we're unrestricted
	}

	if ( team == TEAM_UNASSIGNED )
	{
		// Attempt to auto-select a team, may set team to T, CT or SPEC
		team = mp->SelectDefaultTeam( !IsBot() );

		if ( team == TEAM_UNASSIGNED )
		{
			// still team unassigned, try to kick a bot if possible

			// kick a bot to allow human to join
			if ( cv_bot_auto_vacate.GetBool() && !IsBot() )
			{
				team = ( random->RandomInt( 0, 1 ) == 0 ) ? TEAM_TERRORIST : TEAM_CT;
				if ( UTIL_KickBotFromTeam( team, bQueue ) == false )
				{
					// no bots on that team, try the other
					team = ( team == TEAM_CT ) ? TEAM_TERRORIST : TEAM_CT;
					if ( UTIL_KickBotFromTeam( team, bQueue ) == false )
					{
						// couldn't kick any bots, fail
						team = TEAM_UNASSIGNED;
					}
				}
			}
			
			if ( team == TEAM_UNASSIGNED )
			{
				SendJoinTeamFailedMessage( TeamJoinFailedReason::BOTH_TEAMS_FULL, true );
				return false;
			}
		}
	}

	// Do custom team validation when loading round backup data
	bool bLoadingRoundBackupData = ( CSGameRules() && CSGameRules()->m_bLoadingRoundBackupData );

	if ( !mp->WillTeamHaveRoomForPlayer( this, team ) )
	{
		// attempt to kick a bot to make room for this player
		bool madeRoom = false;
		if ( cv_bot_auto_vacate.GetBool() && !IsBot() && team != TEAM_SPECTATOR )
		{
			if ( UTIL_KickBotFromTeam( team, bQueue ) )
				madeRoom = true;
		}

		if ( !madeRoom )
		{
			// in training, we don't want to print messages or show the team screen or anything
			if ( CSGameRules() && CSGameRules()->IsPlayingTraining() )
				return false;

			if ( !bLoadingRoundBackupData )
			{	// Don't fail team joining when loading round backup data
				if ( team == TEAM_TERRORIST )
				{
					SendJoinTeamFailedMessage( TeamJoinFailedReason::TERRORISTS_FULL, true );
				}
				else if ( team == TEAM_CT )
				{
					SendJoinTeamFailedMessage( TeamJoinFailedReason::CTS_FULL, true );
				}
				else if ( team == TEAM_SPECTATOR )
				{
					SendJoinTeamFailedMessage( TeamJoinFailedReason::CANT_JOIN_SPECTATOR, true );
				}

				return false;
			}
		}
	}

	// check if humans are restricted to a single team ( Tour of Duty, etc )
	if ( !IsBot() && team != TEAM_SPECTATOR )
	{
		int humanTeam = mp->GetHumanTeam();
		if ( humanTeam != TEAM_UNASSIGNED && humanTeam != team )
		{
			if ( humanTeam == TEAM_TERRORIST )
			{
				SendJoinTeamFailedMessage( TeamJoinFailedReason::HUMANS_CAN_ONLY_JOIN_TS, true );
			}
			else if ( humanTeam == TEAM_CT )
			{
				SendJoinTeamFailedMessage( TeamJoinFailedReason::HUMANS_CAN_ONLY_JOIN_CTS, true );
			}

			return false;
		}
	}

	if ( team == TEAM_SPECTATOR )
	{
		// Prevent this if the cvar is set
		if ( !bLoadingRoundBackupData )
		{
			if ( (!mp_allowspectators.GetInt() && !IsHLTV()) || (CSGameRules() && CSGameRules()->IsPlayingTraining()) )
			{
				SendJoinTeamFailedMessage( TeamJoinFailedReason::CANT_JOIN_SPECTATOR, false );
				return false;
			}
		}
		
		if ( GetTeamNumber() != TEAM_UNASSIGNED && State_Get() == STATE_ACTIVE )
		{
			m_fNextSuicideTime = gpGlobals->curtime;	// allow the suicide to work

			CommitSuicide();

			// add 1 to frags to balance out the 1 subtracted for killing yourself
			IncrementFragCount( 1 );
		}

		ChangeTeam( TEAM_SPECTATOR );
		m_iClass = (int )CS_CLASS_NONE;

		// do we have fadetoblack on? (need to fade their screen back in )
		if ( mp_forcecamera.GetInt() == OBS_ALLOW_NONE && CSGameRules()->IsPlayingCooperativeGametype() )
		{
			color32_s clr = { 0,0,0,255 };
			UTIL_ScreenFade( this, clr, 0, 0, FFADE_IN | FFADE_PURGE );
		}

		m_iDeathPostEffect = 0;

		if ( bQueue )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "teamchange_pending" );
			if ( event )
			{
				event->SetInt( "userid", GetUserID() );
				event->SetInt( "toteam", GetPendingTeamNumber() );
				gameeventmanager->FireEvent( event );
			}
		}

		return true;
	}
	
	// If the code gets this far, the team is not TEAM_UNASSIGNED


	if (mp->TeamStacked( team, GetTeamNumber() ) )//players are allowed to change to their own team so they can just change their model
	{
		// attempt to kick a bot to make room for this player
		bool madeRoom = false;
		if (cv_bot_auto_vacate.GetBool() && !IsBot() )
		{
			if (UTIL_KickBotFromTeam( team, bQueue ) )
				madeRoom = true;
		}

		if ( !madeRoom && !bLoadingRoundBackupData )
		{
			// The specified team is full
			SendJoinTeamFailedMessage( ( team == TEAM_TERRORIST ) ?	TeamJoinFailedReason::TOO_MANY_TS : TeamJoinFailedReason::TOO_MANY_CTS, true );

			return false;
		}
	}

	// Show the appropriate Choose Appearance menu
	// This must come before ClientKill() for CheckWinConditions() to function properly

	SetPendingTeamNum( team );

	if ( bQueue )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "teamchange_pending" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetInt( "toteam", GetPendingTeamNumber() );
			gameeventmanager->FireEvent( event );
		}

		CommitSuicide();
	}
	else
	{
		// Switch their actual team...
		ChangeTeam( team );

		if ( IsAbleToInstantRespawn() && 
			( ( CSGameRules()->IsPlayingOffline() && CSGameRules()->GetCustomBotDifficulty() != CUSTOM_BOT_DIFFICULTY_NOBOTS ) || !CSGameRules()->IsPlayingOffline() ) )
		{
			// Handle the case when playing Arms Race and bots are in the game
			if ( team == TEAM_CT || team == TEAM_TERRORIST || team == TEAM_SPECTATOR )
			{
				// Fill in the spot left by the player going to another team
				if ( currTeam == TEAM_CT )
				{
					engine->ServerCommand( UTIL_VarArgs( "bot_add_ct\n" ) );
				}
				else if ( currTeam == TEAM_TERRORIST )
				{
					engine->ServerCommand( UTIL_VarArgs( "bot_add_t\n" ) );
				}
			}
		}
	}

	// If a player joined at halftime he would have missed the requirement to switch teams at round reset,
	// cause him to pick up that rule here:
	if ( CSGameRules() && CSGameRules()->IsSwitchingTeamsAtRoundReset() && !WillSwitchTeamsAtRoundReset() &&
		( ( GetAssociatedTeamNumber() == TEAM_CT ) || ( GetAssociatedTeamNumber() == TEAM_TERRORIST ) ) )
		SwitchTeamsAtRoundReset();

	//
	// Transfer all parameters that we know about
	//
	CSteamID const *pThisSteamId = engine->GetClientSteamID( this->edict() );
	if ( pThisSteamId && pThisSteamId->IsValid() && pThisSteamId->GetAccountID() && !IsBot() && CSGameRules() )
	{
		SetHumanPlayerAccountID( pThisSteamId->GetAccountID() );
	}

	return true;
}

bool CCSPlayer::HandleCommand_JoinClass( void )
{

	int iClass = CS_CLASS_NONE;
	// Choose a random class based on the team.
	switch ( GetTeamNumber() )
	{
		case TEAM_CT :
		case TEAM_TERRORIST :
			iClass = PlayerModelInfo::GetPtr()->GetNextClassForTeam( GetTeamNumber() );
			break;
	}

	// Reset the player's state
	if ( State_Get() == STATE_ACTIVE )
	{
		CSGameRules()->CheckWinConditions();
	}

	if ( !IsBot() && State_Get() == STATE_ACTIVE ) // Bots are responsible about only switching classes when they join.
	{
		// Kill player if switching classes while alive.
		// This mimics goldsrc CS 1.6, and prevents a player from hiding, and switching classes to
		// make the opposing team think there are more enemies than there really are.
		CommitSuicide();
	}

	m_iClass = iClass;

	//if ( State_Get() == STATE_PICKINGTEAM )
	{
		//State_Transition( STATE_ACTIVE );
		GetIntoGame();
	}
	return true;
}


/*
void CheckStartMoney( void )
{
	if ( mp_startmoney.GetInt() > 16000 )
	{
		mp_startmoney.SetInt( 16000 );
	}
	else if ( mp_startmoney.GetInt() < 800 )
	{
		mp_startmoney.SetInt( 800 );
	}
}
*/

void CCSPlayer::GetIntoGame()
{
	// Set their model and if they're allowed to spawn right now, put them into the world.
	//SetPlayerModel( iClass );
	
	SetFOV( this, 0 );
	m_flLastAction = gpGlobals->curtime;

	CCSGameRules *MPRules = CSGameRules();

/*	//MIKETODO: Escape gameplay ?
	if ( ( MPRules->m_bMapHasEscapeZone == true ) && ( m_iTeam == TEAM_CT ) )
	{
		ResetAccount();

		CheckStartMoney();
		AddAccount( (int )startmoney.value, false );
	}
	*/

//	bool bSpawnNow = IsAbleToInstantRespawn();
//	if ( CSGameRules() && !CSGameRules()->IsWarmupPeriod() && mp_use_respawn_waves.GetBool() && CSGameRules()->GetNextRespawnWave( GetTeamNumber(), NULL ) > gpGlobals->curtime )
//		bSpawnNow = false;

	
	//****************New Code by SupraFiend************
	if ( !MPRules->FPlayerCanRespawn( this ) )
	{
		// This player is joining in the middle of a round or is an observer. Put them directly into observer mode.
		//pev->deadflag		= DEAD_RESPAWNABLE;
		//pev->classname		= MAKE_STRING("player" );
		//pev->flags		   &= ( FL_PROXY | FL_FAKECLIENT );	// clear flags, but keep proxy and bot flags that might already be set
		//pev->flags		   |= FL_CLIENT | FL_SPECTATOR;
		//SetThink(PlayerDeathThink );
		
		State_Transition( STATE_OBSERVER_MODE );
				
		m_wasNotKilledNaturally = true;

		MPRules->CheckWinConditions();
	}
	else
	{
		State_Transition( STATE_ACTIVE );
		Spawn();
		
		MPRules->CheckWinConditions();

		// [menglish] Have the rules update anything related to a player spawning in late
		MPRules->SpawningLatePlayer(this );

		if( MPRules->GetRoundRestartTime() == 0.0f )
		{
			//Bomb target, no bomber and no bomb lying around.
			if( !MPRules->IsPlayingCoopMission() && !MPRules->IsPlayingGunGameProgressive() && 
				!MPRules->IsPlayingGunGameDeathmatch() && !MPRules->IsWarmupPeriod() && 
				MPRules->IsBombDefuseMap() && !MPRules->IsThereABomber() && !MPRules->IsThereABomb() )
			{
				MPRules->GiveC4ToRandomPlayer(); //Checks for terrorists.
			}
		}

		// If a new terrorist is entering the fray, then up the # of potential escapers.
		if ( GetTeamNumber() == TEAM_TERRORIST )
			MPRules->m_iNumEscapers++;

		// [menglish] Reset Round Based Achievement Variables
		ResetRoundBasedAchievementVariables();

		// HACK: Joining from the observer team happens here instead of with the rest of the team... set round start money
		m_unRoundStartEquipmentValue = RecalculateCurrentEquipmentValue();

		// resetting takes away the players' weapons on first spawn....
		//if ( MPRules && MPRules->IsWarmupPeriod() )
		//	Reset( false );

		if ( !IsObserver() )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "player_spawned" );

			// Check for a pending round restart in order to determine if the 'post team select' overlay needs to be shown
			int nRestartTime = static_cast<int>( ceil( CSGameRules( )->GetRoundRestartTime() ) );

			// If there are needed players then a round restart will be pending after they connect
			bool bNeeded = false;
			CSGameRules( )->NeededPlayersCheck( bNeeded );

			// If there are no bots on this map then bypass the overlay
			ConVarRef bot_quota( "bot_quota" );

			if ( event )
			{
				event->SetInt( "userid", GetUserID() );
				event->SetBool( "inrestart", ( !CSGameRules()->IsPlayingTraining() && ( bot_quota.GetInt() > 0 ) && ( nRestartTime > 0 || bNeeded ) ) );
				gameeventmanager->FireEvent( event );
			}
		}
	}
}


int CCSPlayer::PlayerClass() const
{
	return m_iClass;
}



bool CCSPlayer::SelectSpawnSpot( const char *pEntClassName, CBaseEntity* &pStartSpot )
{
	CBaseEntity* pSpot = pStartSpot;
	
	const int kNumSpawnSpotsToScan = 32;	// Scanning spawn points loops around, so it's ok to scan same points multiple times

	if ( V_strcmp( pEntClassName, "info_player_counterterrorist" ) == 0 )
	{
		for ( int i = kNumSpawnSpotsToScan; i > 0; i-- )
		{
			pSpot = ( CSGameRules()->GetNextSpawnpoint( TEAM_CT ) );

			if ( pSpot && g_pGameRules->IsSpawnPointValid( pSpot, this ) && pSpot->GetAbsOrigin() != Vector( 0, 0, 0 ) )
			{
				pStartSpot = pSpot;
				return true;
			}
		}
	}
	else if ( V_strcmp( pEntClassName, "info_player_terrorist" ) == 0 )
	{
		for ( int i = kNumSpawnSpotsToScan; i > 0; i-- )
		{
			pSpot = ( CSGameRules()->GetNextSpawnpoint( TEAM_TERRORIST ) );

			if ( pSpot && g_pGameRules->IsSpawnPointValid( pSpot, this ) && pSpot->GetAbsOrigin() != Vector( 0, 0, 0 ) )
			{
				pStartSpot = pSpot;
				return true;
			}
		}
	}
	else if ( V_strcmp( pEntClassName, "info_enemy_terrorist_spawn" ) == 0 )
	{
		if ( mp_randomspawn_los.GetBool() == false )
		{
			for ( int i = kNumSpawnSpotsToScan; i > 0; i-- )
			{
				pSpot = ( CSGameRules()->GetNextSpawnpoint( TEAM_TERRORIST ) );

				bool bIsValid = true;
				if ( !pSpot || g_pGameRules->IsSpawnPointValid( pSpot, this ) == false || pSpot->GetAbsOrigin() == Vector( 0, 0, 0 ) )
					bIsValid = false;

				if ( bIsValid )
				{
					pStartSpot = pSpot;
					return true;
				}
				else
				{
					Msg( "FAILED TO GET A VALID SPAWN SPOT\n" );
				}
			}
		}

		// if we made it down here, we just need to find any valid spot

		CBaseEntity* pSpotValidButVisible = NULL;

		while ( true )
		{
			pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
			SpawnPoint* pSpawnpoint = assert_cast< SpawnPoint* >( pSpot );

			bool bSpawnValid = g_pGameRules->IsSpawnPointValid( pSpawnpoint, this );
			// check if pSpot is valid
			if ( pSpawnpoint && pSpawnpoint->IsEnabled() && bSpawnValid && pSpawnpoint->GetAbsOrigin() != Vector( 0, 0, 0 ) )
			{
				if ( mp_randomspawn_los.GetBool() )
				{
					if ( CSGameRules() && CSGameRules()->IsSpawnPointHiddenFromOtherPlayers( pSpawnpoint, this, TEAM_CT ) 
						 && UTIL_IsRandomSpawnFarEnoughAwayFromTeam( pSpawnpoint->GetAbsOrigin(), TEAM_CT ) )
					{
						pStartSpot = pSpawnpoint;
						return true;
					}
					else	// the spawn point is either hidden, or we don't care
					{
						pSpotValidButVisible = pSpawnpoint;
					}
				}
				else	// the spawn point is either hidden, or we don't care
				{
					pSpotValidButVisible = pSpawnpoint;
				}
			}

			// if we're back to the start of the list
			if ( pSpawnpoint == pStartSpot )
			{
				// use the valid but unfortunately visible spot.
				if ( pSpotValidButVisible != NULL )
				{
					pStartSpot = pSpotValidButVisible;
					return true;
				}
				else
				{
					pStartSpot = ( CSGameRules()->GetNextSpawnpoint( TEAM_TERRORIST ) );
					return true;
				}
				break;
			}
		}

//		for ( int i = kNumSpawnSpotsToScan; i > 0; i-- )
// 		{
// 			pSpot = ( CSGameRules()->GetNextSpawnpoint( TEAM_TERRORIST ) );
// 
// 			bool bIsValid = true;
// 			if ( !pSpot || g_pGameRules->IsSpawnPointValid( pSpot, this ) == false || pSpot->GetAbsOrigin() == Vector( 0, 0, 0 ) )
// 				bIsValid = false;
// 				
// 			if ( CSGameRules()->IsPlayingCoopTilegen() && UTIL_IsVisibleToTeam( pSpot->GetAbsOrigin(), TEAM_CT ) == true )
// 				bIsValid = false;
// 			
// 			if ( CSGameRules()->IsPlayingCoopMission() )
// 			{
// 				if ( mp_randomspawn_los.GetBool() && GetTeamNumber() == TEAM_TERRORIST && 
// 					 CSGameRules()->IsSpawnPointHiddenFromOtherPlayers( pSpot, this, TEAM_CT ) == false )
// 				{
// 					bIsValid = false;
// 				}
// 			}
// 
// 			if ( bIsValid )
// 			{
// 				pStartSpot = pSpot;
// 				return true;
// 			}
// 			else
// 			{
// 				Msg( "FAILED TO GET A VALID SPAWN SPOT\n" );
// 			}
// 		}
	}
	else
	{
		pSpot = pStartSpot;

		CBaseEntity* pSpotValidButVisible = NULL;

		while ( true )
		{
			pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
			SpawnPoint* pSpawnpoint = assert_cast< SpawnPoint* >( pSpot );

			bool bSpawnValid = g_pGameRules->IsSpawnPointValid( pSpawnpoint, this );
			// check if pSpot is valid
			if ( pSpawnpoint && pSpawnpoint->IsEnabled() && bSpawnValid && pSpawnpoint->GetAbsOrigin() != Vector( 0, 0, 0 ) )
			{
				if ( mp_randomspawn_los.GetBool() )
				{
					if ( CSGameRules() && CSGameRules()->IsSpawnPointHiddenFromOtherPlayers( pSpawnpoint, this ) 
						 && UTIL_IsRandomSpawnFarEnoughAwayFromTeam( pSpawnpoint->GetAbsOrigin(), TEAM_CT ) )
					{
						pStartSpot = pSpawnpoint;
						return true;
					}
					else	// the spawn point is either hidden, or we don't care
					{
						pSpotValidButVisible = pSpawnpoint;
					}
				}
				else	// the spawn point is either hidden, or we don't care
				{
					pSpotValidButVisible = pSpawnpoint;
				}
			}

			// if we're back to the start of the list
			if ( pSpawnpoint == pStartSpot )
			{
				// use the valid but unfortunately visible spot.
				if ( pSpotValidButVisible != NULL )
				{
					pStartSpot = pSpotValidButVisible;
					return true;
				}
				else
				{
					if ( GetTeamNumber() == TEAM_CT )
						SelectSpawnSpot( "info_player_counterterrorist", pStartSpot );

					else if ( GetTeamNumber() == TEAM_TERRORIST )
						SelectSpawnSpot( "info_player_terrorist", pStartSpot );

					return true;
				}
				break;
			}
		}

		DevMsg( "CCSPlayer::SelectSpawnSpot: couldn't find valid spawn point.\n" );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called directly after we select a spawn point and teleport to it
//-----------------------------------------------------------------------------
void CCSPlayer::PostSpawnPointSelection( )
{
	if(m_storedSpawnAngle.LengthSqr( ) > 0.0f || m_storedSpawnPosition != vec3_origin )
	{
		Teleport( &m_storedSpawnPosition, &m_storedSpawnAngle, &vec3_origin );
		m_storedSpawnPosition = vec3_origin;
		m_storedSpawnAngle.Init( );
	}
}

CBaseEntity* CCSPlayer::EntSelectSpawnPoint()
{
	CBaseEntity *pSpot;
	
	pSpot = NULL;
	if ( CSGameRules()->IsLogoMap() )
	{
		// This is a logo map. Don't allow movement or logos or menus.
		SelectSpawnSpot( "info_player_logo", pSpot );
		LockPlayerInPlace();
		goto ReturnSpot;
	}
	else
	{	
		// The map maker can force use of map-placed spawns in deathmatch using an info_map_parameters entity.
		bool bUseNormalSpawnsForDM = ( g_pMapInfo ? g_pMapInfo->m_bUseNormalSpawnsForDM : false );
		if ( !bUseNormalSpawnsForDM &&
			(	mp_randomspawn.GetInt() == GetTeamNumber() ||
				mp_randomspawn.GetInt() == 1 ) )
		{
			pSpot = g_pLastCTSpawn; // reusing g_pLastCTSpawn.
			// Randomize the start spot
			for ( int i = random->RandomInt(1,10); i > 0; i-- )
			{
				pSpot = gEntList.FindEntityByClassname( pSpot, "info_deathmatch_spawn" );
			}
			if ( !pSpot )  // skip over the null point
				pSpot = gEntList.FindEntityByClassname( pSpot, "info_deathmatch_spawn" );

			if ( SelectSpawnSpot( "info_deathmatch_spawn", pSpot ))
			{
				g_pLastCTSpawn = pSpot;
				goto ReturnSpot;
			}
		}

		else if ( GetTeamNumber() == TEAM_CT )
		{
			pSpot = g_pLastCTSpawn;
// 			if ( CSGameRules()->IsPlayingGunGameProgressive() )
// 			{
// 				if ( SelectSpawnSpot( "info_armsrace_counterterrorist", pSpot ) )
// 				{
// 					g_pLastCTSpawn = pSpot;
// 					goto ReturnSpot;
// 				}
// 			}

			if ( SelectSpawnSpot( "info_player_counterterrorist", pSpot ) )
			{
				g_pLastCTSpawn = pSpot;
				goto ReturnSpot;
			}
		}

		/*********************************************************/
		// The terrorist spawn points
		else if ( GetTeamNumber() == TEAM_TERRORIST )
		{
			pSpot = g_pLastTerroristSpawn;
			
// 			if ( CSGameRules()->IsPlayingGunGameProgressive() )
// 			{
// 				if ( SelectSpawnSpot( "info_armsrace_terrorist", pSpot ) )
// 				{
// 					g_pLastCTSpawn = pSpot;
// 					goto ReturnSpot;
// 				}
// 			}

			const char* szTSpawnEntName = "info_player_terrorist";
			if ( CSGameRules()->IsPlayingCoopMission() )
				szTSpawnEntName = "info_enemy_terrorist_spawn";

			if ( SelectSpawnSpot( szTSpawnEntName, pSpot ) )
			{
				g_pLastTerroristSpawn = pSpot;
				goto ReturnSpot;
			}
		}
	}


	// If forced startspot is set, (re )spawn there.
	// never attempt to use any random spots because we don't want CTs spawning in Ts buyzone
	if ( !!gpGlobals->startspot && V_strlen( STRING(gpGlobals->startspot ) ) )
	{
		pSpot = gEntList.FindEntityByTarget( NULL, STRING(gpGlobals->startspot ) );
		if ( pSpot )
			goto ReturnSpot;
	}

ReturnSpot:
	if ( !pSpot )
	{
		if( CSGameRules()->IsLogoMap() )
			Warning( "PutClientInServer: no info_player_logo on level\n" );
		else
			Warning( "PutClientInServer: no info_player_start on level\n" );

		return CBaseEntity::Instance( INDEXENT(0 ) );
	}

	return pSpot;
} 


void CCSPlayer::SetProgressBarTime( int barTime )
{
	m_iProgressBarDuration = barTime;
	m_flProgressBarStartTime = this->m_flSimulationTime;
}


void CCSPlayer::PlayerDeathThink()
{
}

void CCSPlayer::ResetForceTeamThink()
{
	m_fForceTeam = -1.0f;
	SetContextThink( &CBasePlayer::PlayerForceTeamThink, TICK_NEVER_THINK, CS_FORCE_TEAM_THINK_CONTEXT );
}

void CCSPlayer::PlayerForceTeamThink()
{
	if ( GetTeamNumber() != TEAM_UNASSIGNED )
	{
		// player joined a team between last think and now
		ResetForceTeamThink();
		return;
	}

	SetContextThink( &CBasePlayer::PlayerForceTeamThink, gpGlobals->curtime + 0.5f, CS_FORCE_TEAM_THINK_CONTEXT );
	
	if ( m_fForceTeam == -1.0f )
	{
		ConVarRef cvTime( "mp_force_pick_time" );
		m_fForceTeam = gpGlobals->curtime + cvTime.GetFloat();
	}

	if ( gpGlobals->curtime > m_fForceTeam )
	{
		// selection time expired. try to auto team.
		if ( !HandleCommand_JoinTeam( 0, false ) )
		{
			// failed to join CT, T, or spectator. Force disconnect.
			engine->ClientCommand( edict(), "disconnect\n" );
		}
	}
}


void CCSPlayer::State_Transition( CSPlayerState newState )
{
	State_Leave();
	State_Enter( newState );
}


void CCSPlayer::State_Enter( CSPlayerState newState )
{
	m_iPlayerState = newState;
	m_pCurStateInfo = State_LookupInfo( newState );

	if ( cs_ShowStateTransitions.GetInt() == -1 || cs_ShowStateTransitions.GetInt() == entindex() )
	{
		if ( m_pCurStateInfo )
			Msg( "ShowStateTransitions: entering '%s'\n", m_pCurStateInfo->m_pStateName );
		else
			Msg( "ShowStateTransitions: entering #%d\n", newState );
	}
	
	// Initialize the new state.
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnEnterState )
		(this->*m_pCurStateInfo->pfnEnterState )();
}


void CCSPlayer::State_Leave()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnLeaveState )
	{
		(this->*m_pCurStateInfo->pfnLeaveState )();
	}
}


void CCSPlayer::State_PreThink()
{
	if ( m_pCurStateInfo && m_pCurStateInfo->pfnPreThink )
	{
		(this->*m_pCurStateInfo->pfnPreThink )();
	}
}


CCSPlayerStateInfo* CCSPlayer::State_LookupInfo( CSPlayerState state )
{
	// This table MUST match the 
	static CCSPlayerStateInfo playerStateInfos[] =
	{
		{ STATE_ACTIVE,			"STATE_ACTIVE",			&CCSPlayer::State_Enter_ACTIVE, NULL, &CCSPlayer::State_PreThink_ACTIVE },
		{ STATE_WELCOME,		"STATE_WELCOME",		&CCSPlayer::State_Enter_WELCOME, NULL, &CCSPlayer::State_PreThink_WELCOME },
		{ STATE_PICKINGTEAM,	"STATE_PICKINGTEAM",	&CCSPlayer::State_Enter_PICKINGTEAM, NULL,	&CCSPlayer::State_PreThink_OBSERVER_MODE },
		{ STATE_PICKINGCLASS,	"STATE_PICKINGCLASS",	&CCSPlayer::State_Enter_PICKINGCLASS, NULL,	&CCSPlayer::State_PreThink_OBSERVER_MODE },
		{ STATE_DEATH_ANIM,		"STATE_DEATH_ANIM",		&CCSPlayer::State_Enter_DEATH_ANIM,	NULL, &CCSPlayer::State_PreThink_DEATH_ANIM },
		{ STATE_DEATH_WAIT_FOR_KEY,	"STATE_DEATH_WAIT_FOR_KEY",	&CCSPlayer::State_Enter_DEATH_WAIT_FOR_KEY,	NULL, &CCSPlayer::State_PreThink_DEATH_WAIT_FOR_KEY },
		{ STATE_OBSERVER_MODE,	"STATE_OBSERVER_MODE",	&CCSPlayer::State_Enter_OBSERVER_MODE,	NULL, &CCSPlayer::State_PreThink_OBSERVER_MODE },
		{ STATE_GUNGAME_RESPAWN, "STATE_GUNGAME_RESPAWN", &CCSPlayer::State_Enter_GUNGAME_RESPAWN, NULL, &CCSPlayer::State_PreThink_GUNGAME_RESPAWN },
		{ STATE_DORMANT,		"STATE_DORMANT",		NULL, NULL, NULL },
	};

	for ( int i=0; i < ARRAYSIZE( playerStateInfos ); i++ )
	{
		if ( playerStateInfos[i].m_iPlayerState == state )
			return &playerStateInfos[i];
	}

	return NULL;
}


void CCSPlayer::PhysObjectSleep()
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj )
		pObj->Sleep();
}


void CCSPlayer::PhysObjectWake()
{
	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj )
		pObj->Wake();
}


void CCSPlayer::State_Enter_WELCOME()
{
	StartObserverMode( OBS_MODE_ROAMING );

	// Important to set MOVETYPE_NONE or our physics object will fall while we're sitting at one of the intro cameras.
	SetMoveType( MOVETYPE_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );

	PhysObjectSleep();

	const ConVar *hostname = cvar->FindVar( "hostname" );
	const char *title = (hostname ) ? hostname->GetString() : "MESSAGE OF THE DAY";

	// this is where we send the string table data down to the client
	// NOTE, we dont' show the panel here, we just send the data, the naming is bad....

	// Show info panel (if it's not a simple demo map ).
	if ( !CSGameRules()->IsLogoMap() )
	{
		const bool enableMOTD = true;
		if ( CommandLine()->FindParm( "-makereslists" ) ) // don't show the MOTD when making reslists
		{
			engine->ClientCommand( edict(), "jointeam 3\n" );
		}
		else if ( enableMOTD )
		{
			KeyValues *data = new KeyValues("data" );
			data->SetString( "title", title );		// info panel title
			data->SetString( "type", "1" );			// show userdata from stringtable entry
			data->SetString( "msg",	"motd" );		// use this stringtable entry
			// [Forrest] Replaced text window command string with TEXTWINDOW_CMD enumeration
			// of options.  Passing a command string is dangerous and allowed a server network
			// message to run arbitrary commands on the client.
			data->SetInt( "cmd", TEXTWINDOW_CMD_JOINGAME );	// exec this command if panel closed

			ShowViewPortPanel( PANEL_INFO, true, data );

			data->deleteThis();
		}
	}
}


void CCSPlayer::State_PreThink_WELCOME()
{
	// Verify some state.
	Assert( IsSolidFlagSet( FSOLID_NOT_SOLID ) );
#ifdef DBGFLAG_ASSERT
	if ( GetAbsVelocity().Length() != 0 && !engine->IsDedicatedServer() )
	{
		// This happens e.g. when player moves during warmup: the player is still in WELCOME state, but CPlayerMove::FinishMove() will happily set its velocity.
		ExecuteOnce( Warning( "Player velocity != 0 in State_PreThink_WELCOME.\n" ) );
	}
#endif

	// Update whatever intro camera it's at.
	if( m_pIntroCamera && (gpGlobals->curtime >= m_fIntroCamTime ) )
	{
		MoveToNextIntroCamera();
	}
}


void CCSPlayer::State_Enter_PICKINGTEAM()
{
	ShowViewPortPanel( "team" ); // show the team menu
}


void CCSPlayer::State_Enter_DEATH_ANIM()
{
	if ( HasWeapons() )
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		PackDeadPlayerItems();
	}

	// Used for a timer.
	m_flDeathTime = gpGlobals->curtime;

	m_bAbortFreezeCam = false;

	StartObserverMode( OBS_MODE_DEATHCAM );	// go to observer mode
	RemoveEffects( EF_NODRAW );	// still draw player body

	if ( mp_forcecamera.GetInt() == OBS_ALLOW_NONE )
	{
		color32_s clr = {0,0,0,255};
		UTIL_ScreenFade( this, clr, 0.3f, 3, FFADE_OUT | FFADE_STAYOUT );

		//Don't perform any freezecam stuff if we are fading to black
		State_Transition( STATE_DEATH_WAIT_FOR_KEY );
	}

	if ( m_bKilledByHeadshot )
	{
		m_iDeathPostEffect = 15; // POST_EFFECT_DEATH_CAM_HEADSHOT;
	}
	else
	{
		m_iDeathPostEffect = 14; // POST_EFFECT_DEATH_CAM_BODYSHOT;
	}
}


void CCSPlayer::State_PreThink_DEATH_ANIM()
{
	if ( IsAlive() )
		return;

	// If the anim is done playing, go to the next state (waiting for a keypress to 
	// either respawn the guy or put him into observer mode ).
	if ( GetFlags() & FL_ONGROUND )
	{
		float flForward = GetAbsVelocity().Length() - 20;
		if (flForward <= 0 )
		{
			SetAbsVelocity( vec3_origin );
		}
		else
		{
			Vector vAbsVel = GetAbsVelocity();
			VectorNormalize( vAbsVel );
			vAbsVel *= flForward;
			SetAbsVelocity( vAbsVel );
		}
	}

	float flDeathDelayDefault = spec_freeze_deathanim_time.GetFloat();
	//float flDeathDelayDefault = CS_DEATH_ANIMATION_TIME;

	CBaseEntity* pKiller = GetObserverTarget();

	// there is a bug here where if you are spectating another player and they die, you won't see the death anim because the deathanim delay is so show
	// we need to find a way to differentiate from the local player and a player that you are spectating.  this doesn't work below
	//if ( GetObserverMode() == OBS_MODE_DEATHCAM && pKiller && pKiller != this )
	//	flDeathDelayDefault = CS_DEATH_ANIMATION_TIME;

	float fDeathEnd = m_flDeathTime + flDeathDelayDefault;
	float fFreezeEnd = fDeathEnd + spec_freeze_traveltime.GetFloat() + spec_freeze_time.GetFloat();
	float fFreezeLock = fDeathEnd + spec_freeze_time_lock.GetFloat();

	// if we use repsawn waves, its time to respawn and we are ABLE to respawn, then do so
	// otherwise, just check to see if we are able to respawn
	bool bShouldRespawnNow = IsAbleToInstantRespawn();
	if ( CSGameRules() && !CSGameRules()->IsWarmupPeriod() && mp_use_respawn_waves.GetBool() && CSGameRules()->GetNextRespawnWave( GetTeamNumber(), NULL ) > gpGlobals->curtime )
		bShouldRespawnNow = false;

	// transition to Freezecam mode once the death animation is complete
	if ( gpGlobals->curtime >= fDeathEnd )
	{
		if ( !m_bAbortFreezeCam && gpGlobals->curtime < fFreezeEnd && ( GetObserverMode() != OBS_MODE_FREEZECAM ) )
		{
			CPlantedC4* pPlantedC4 = pKiller ? dynamic_cast< CPlantedC4* >( pKiller ) : NULL;
			
			if ( pPlantedC4 == NULL )
			{
				// before we can replay, we need to freezecam for a little while (1-2 seconds) to let the player see the killer and output the stats
				StartObserverMode( OBS_MODE_FREEZECAM );
			}
		}
		else if(GetObserverMode() == OBS_MODE_FREEZECAM )
		{
			if ( m_bAbortFreezeCam && ( mp_forcecamera.GetInt() != OBS_ALLOW_NONE || CSGameRules()->IsWarmupPeriod() ) )
			{
				if ( bShouldRespawnNow )
				{
					// Respawn in gun game progressive
					State_Transition( STATE_GUNGAME_RESPAWN );
				}
				else
				{
					State_Transition( STATE_OBSERVER_MODE );
				}
			}
		}
	}

	// Don't transfer to observer state until the freeze cam is done
	// Players in competitive mode may bypass this mode with a key press
	if ( ( gpGlobals->curtime > fFreezeEnd ) ||
	     ( gpGlobals->curtime > fFreezeLock && ( m_nButtons & ~IN_SCORE ) && mp_deathcam_skippable.GetBool() ) ||
		 m_bIsRespawningForDMBonus )
	{
		if ( bShouldRespawnNow )
		{
			// Transition to respawn in gun game progressive
			State_Transition( STATE_GUNGAME_RESPAWN );
		}
		else
		{
			State_Transition( STATE_OBSERVER_MODE );
		}
	}
}

bool CCSPlayer::StartHltvReplayEvent( const ClientReplayEventParams_t &params )
{
	float flEventTime = params.m_flEventTime;
	int nPrimaryTargetEntIndex = params.m_nPrimaryTargetEntIndex;
	
	if ( params.m_nEventType == REPLAY_EVENT_DEATH )
	{
		flEventTime = GetDeathTime();

		if ( flEventTime < gpGlobals->curtime - 10.0f )
			return false;

		if ( IsAlive() )
		{
			DevMsg( "Player is active, replaying last kill will interfere with the gameplay" );
			return false;
		}
		nPrimaryTargetEntIndex = params.m_nPrimaryTargetEntIndex > 0 ? params.m_nPrimaryTargetEntIndex : m_nLastKillerIndex;
	}

	float flPostEventAnimTime = spec_replay_winddown_time.GetFloat();
	static ConVarRef spec_replay_leadup_time( "spec_replay_leadup_time" );
	float flPreEventReplayLength = spec_replay_leadup_time.GetFloat();
	float flReplayStartTime = flEventTime - flPreEventReplayLength;
	float flReplayEndTime = flEventTime + flPostEventAnimTime;
	float flCurTime = gpGlobals->curtime;
	float flDelay = flCurTime - flReplayStartTime;

	if ( flDelay < 1.0f )
	{
		DevMsg( "Cannot replay with a delay of %.2f sec\n", flDelay );
		return false;
	}

	HltvReplayParams_t hltvReplay;
	hltvReplay.m_nPrimaryTargetEntIndex = nPrimaryTargetEntIndex;
	hltvReplay.m_flDelay = flDelay;
	hltvReplay.m_flStopAt = flReplayEndTime - flCurTime;
	int nPlayerEntIndex = this->entindex();
	if ( hltvReplay.m_nPrimaryTargetEntIndex > 0  // there is someone to observe in replay
		 && hltvReplay.m_flStopAt > 1.0f - flDelay // the replay should last a bit
	)
	{
		m_bAbortFreezeCam = true;

		if ( params.m_flSlowdownRate > 0.125f && params.m_flSlowdownRate < 8.0f && params.m_flSlowdownLength > 1.0f / 1024.0f )
		{
			hltvReplay.m_flSlowdownRate = params.m_flSlowdownRate;
			hltvReplay.m_flSlowdownBeginAt = flEventTime - flCurTime - params.m_flSlowdownLength;
			hltvReplay.m_flSlowdownEndAt = flEventTime - flCurTime;
		}

		// it only makes sense to replay if we have at least 1+ seconds of footage
		return engine->StartClientHltvReplay( nPlayerEntIndex - 1, hltvReplay );
	}
	else
	{
		DevMsg( "Cannot replay: last killer %d, player %d, stop at %g, delay %g\n", ( int )nPrimaryTargetEntIndex, nPlayerEntIndex, hltvReplay.m_flStopAt, hltvReplay.m_flDelay );
		return false;
	}
}


void CCSPlayer::State_Enter_DEATH_WAIT_FOR_KEY()
{
	// Remember when we died, so we can automatically put them into observer mode
	// if they don't hit a key soon enough.

	m_lifeState = LIFE_DEAD;
		
	StopAnimation();

	// Don't do this.  The ragdoll system expects to be able to read from this player on 
	// the next update and will read it at the new origin if this is set.
	// Since it is more complicated to redesign the ragdoll system to not need that data
	// it is easier to cause a less obvious bug than popping ragdolls
	//AddEffects( EF_NOINTERP );
}


void CCSPlayer::State_PreThink_DEATH_WAIT_FOR_KEY()
{
	// once we're done animating our death and we're on the ground, we want to set movetype to None so our dead body won't do collisions and stuff anymore
	// this prevents a bug where the dead body would go to a player's head if he walked over it while the dead player was clicking their button to respawn
	if ( GetMoveType() != MOVETYPE_NONE && (GetFlags() & FL_ONGROUND ) )
		SetMoveType( MOVETYPE_NONE );
	
	// if the player has been dead for one second longer than allowed by forcerespawn, 
	// forcerespawn isn't on. Send the player off to an intermission camera until they 
	// choose to respawn.

	bool fAnyButtonDown = (m_nButtons & ~IN_SCORE ) != 0;
	if ( mp_forcecamera.GetInt() == OBS_ALLOW_NONE )
		fAnyButtonDown = false;

	// after a certain amount of time switch to observer mode even if they don't press a key.
	else if (gpGlobals->curtime >= (m_flDeathTime + DEATH_ANIMATION_TIME + 3.0 ) )
	{
		fAnyButtonDown = true;
	}

	if ( fAnyButtonDown )
	{
		// if we use repsawn waves, its time to respawn and we are ABLE to respawn, then do so
		// otherwise, just check to see if we are able to respawn
		bool bShouldRespawnNow = IsAbleToInstantRespawn();
		if ( mp_use_respawn_waves.GetBool() && CSGameRules() && CSGameRules()->GetNextRespawnWave( GetTeamNumber(), NULL ) > gpGlobals->curtime )
			bShouldRespawnNow = false;

		if ( bShouldRespawnNow )
		{
			// Early out transition to respawn when playing death animation
			State_Transition( STATE_GUNGAME_RESPAWN );
		}
		else
		{

			State_Transition( STATE_OBSERVER_MODE );
		}
	}
}

void CCSPlayer::State_Enter_OBSERVER_MODE()
{
	// do we have fadetoblack on? (need to fade their screen back in )
	if ( mp_forcecamera.GetInt() == OBS_ALLOW_NONE )
	{
		color32_s clr = { 0,0,0,255 };
		UTIL_ScreenFade( this, clr, 0, 0, FFADE_IN | FFADE_PURGE );
	}

	m_iDeathPostEffect = 0;

	int observerMode = m_iObserverLastMode;
	if ( IsNetClient() )
	{
		const char *pIdealMode = engine->GetClientConVarValue( entindex(), "cl_spec_mode" );
		if ( pIdealMode )
		{
			int nIdealMode = atoi( pIdealMode );

			if ( nIdealMode < OBS_MODE_IN_EYE )
			{
				nIdealMode = OBS_MODE_IN_EYE;
			}
			else if ( nIdealMode > OBS_MODE_ROAMING )
			{
				nIdealMode = OBS_MODE_ROAMING;
			}

			observerMode = nIdealMode;
		}
	}

	StartObserverMode( observerMode );

	PhysObjectSleep();
}

void CCSPlayer::State_Leave_OBSERVER_MODE()
{
#if CS_CONTROLLABLE_BOTS_ENABLED
	m_bCanControlObservedBot = false;
#endif
}

void CCSPlayer::State_PreThink_OBSERVER_MODE()
{
// 	// check here first to see if we are on a team, our class isn't 0 ( that means we just spawn for the first time int he game) and our respawn wave lets us respawn
// 	// if we use repsawn waves, its time to respawn and we are ABLE to respawn, then do so
// 	// otherwise, just check to see if we are able to respawn
// 	bool bShouldRespawnNow = ( PlayerClass() != 0 && GetTeamNumber() > TEAM_SPECTATOR && mp_use_respawn_waves.GetBool() && CSGameRules() && CSGameRules()->GetNextRespawnWave( GetTeamNumber(), NULL ) <= gpGlobals->curtime ) && IsAbleToInstantRespawn();
// 	if ( bShouldRespawnNow )
// 	{
// 		State_Transition( STATE_GUNGAME_RESPAWN );
// 		return;
// 	}

	// Make sure nobody has changed any of our state.
//	Assert( GetMoveType() == MOVETYPE_FLY );
	Assert( m_takedamage == DAMAGE_NO );
	Assert( IsSolidFlagSet( FSOLID_NOT_SOLID ) );
//	Assert( IsEffectActive( EF_NODRAW ) );
	
	// Must be dead.
	Assert( m_lifeState == LIFE_DEAD );
	Assert( pl.deadflag );

#if CS_CONTROLLABLE_BOTS_ENABLED
	m_bCanControlObservedBot = false;
	if ( GetObserverMode() >= OBS_MODE_IN_EYE )
	{
		CCSBot * pBot = ToCSBot( GetObserverTarget() );
		if ( CanControlBot(pBot ) )
		{
			m_bCanControlObservedBot = true;
		}
	}
#endif
}

void CCSPlayer::State_Enter_GUNGAME_RESPAWN()
{
	TryGungameRespawn();
}

void CCSPlayer::TryGungameRespawn()
{
	int nPlayerEntIndex = this->entindex();
	if ( engine->GetClientHltvReplayDelay( nPlayerEntIndex - 1 ) == 0 )
	{
		// no delay, we can actually respawn
		if ( !m_bRespawning )
		{
			// Perform the respawn of the player in gun game progressive
			m_bRespawning = true;
			State_Transition( STATE_ACTIVE );
			respawn( this, false );
			m_nButtons = 0;
			SetNextThink( TICK_NEVER_THINK );

			int nTeamCheck = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
			//int nOtherTeam = IsHostageRescueMap() ? TEAM_CT : TEAM_TERRORIST;
			if ( (CSGameRules()->IsPlayingCoopGuardian() && GetTeamNumber() == nTeamCheck) ||
				 CSGameRules()->IsPlayingCoopMission() && GetTeamNumber() == TEAM_CT)
			{
				GiveHealthAndArmorForGuardianMode( false );
			}
		}
	}
}

void CCSPlayer::State_PreThink_GUNGAME_RESPAWN()
{
	TryGungameRespawn();
}


void CCSPlayer::State_Enter_PICKINGCLASS()
{
	if ( CommandLine()->FindParm( "-makereslists" ) ) // don't show the menu when making reslists
	{
		engine->ClientCommand( edict(), "joinclass\n" );
		return;
	}

	// go to spec mode, if dying keep deathcam
	if ( GetObserverMode() == OBS_MODE_DEATHCAM )
	{
		StartObserverMode( OBS_MODE_DEATHCAM );
	}

	m_iClass = (int )CS_CLASS_NONE;
	
	PhysObjectSleep();

	// show the class menu:
	if ( ( GetTeamNumber() == TEAM_TERRORIST ) 
		||  ( GetTeamNumber() == TEAM_CT ) )
	{
		engine->ClientCommand( edict(), "joinclass\n" );
	}
	else
	{
		HandleCommand_JoinClass();
	}
}

void CCSPlayer::State_Enter_ACTIVE()
{
	SetMoveType( MOVETYPE_WALK );
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	m_Local.m_iHideHUD = 0;
	PhysObjectWake();

	SetPendingTeamNum( GetTeamNumber() );

	m_bRespawning = false;
}


void CCSPlayer::State_PreThink_ACTIVE()
{
	// Calculate timeout for gun game immunity
	ConVarRef mp_respawn_immunitytime( "mp_respawn_immunitytime" );
	float flImmuneTime = mp_respawn_immunitytime.GetFloat();
	if ( flImmuneTime > 0 || ( CSGameRules() && CSGameRules()->IsWarmupPeriod() ) )
	{
		if ( m_bGunGameImmunity )
		{
			if ( gpGlobals->curtime > m_fImmuneToGunGameDamageTime )
			{
				// Player immunity has timed out
				ClearGunGameImmunity();
				m_fJustLeftImmunityTime = gpGlobals->curtime + 2.0f;

			}
			// or if we've moved and there's more than 1s of immunity left. Check for 1s because the above case adds 1s.
			else if ( IsAbleToInstantRespawn() && m_bHasMovedSinceSpawn && ( m_fImmuneToGunGameDamageTime - gpGlobals->curtime > 1.0f ) )
			{
				m_fImmuneToGunGameDamageTime = gpGlobals->curtime + 1.0f;
			}
		}

		// track that we just left immunity
		if( m_fJustLeftImmunityTime != 0.0f && gpGlobals->curtime > m_fJustLeftImmunityTime )
		{
			m_fJustLeftImmunityTime = 0.0f;
		}
	}

	// track low health achievement
	if( m_lowHealthGoalTime != 0.0f && gpGlobals->curtime > m_lowHealthGoalTime )
	{
		m_lowHealthGoalTime = 0.0f;
		AwardAchievement(CSStillAlive );
	}


	// We only allow noclip here only because noclip is useful for debugging.
	// It would be nice if the noclip command set some flag so we could tell that they
	// did it intentionally.
	if ( IsEFlagSet( EFL_NOCLIP_ACTIVE ) )
	{
//		Assert( GetMoveType() == MOVETYPE_NOCLIP );
	}
	else
	{
//		Assert( GetMoveType() == MOVETYPE_WALK );
	}

	Assert( !IsSolidFlagSet( FSOLID_NOT_SOLID ) );
}

bool CCSPlayer::StartObserverMode( int mode )
{
	if ( !BaseClass::StartObserverMode( mode ) )
		return false;

	// When you enter observer mode, you are no longer planting the bomb or crouch-jumping.
	m_bDuckOverride = false;
	m_duckUntilOnGround = false;

	return true;
}


bool CCSPlayer::SetObserverTarget(CBaseEntity *target)
{
	if ( target )
	{
		CCSPlayer *pPlayer = dynamic_cast<CCSPlayer*>( target );
		if ( pPlayer )
			pPlayer->RefreshCarriedHostage( false );
	}

	return BaseClass::SetObserverTarget(target);
}

void CCSPlayer::ValidateCurrentObserverTarget( void )
{
	if ( !m_bForcedObserverMode )
	{
		CCSPlayer *pPlayer = ToCSPlayer( m_hObserverTarget.Get() );
		if ( IsValidObserverTarget( pPlayer ) && ( pPlayer->IsTaunting() && pPlayer->IsThirdPersonTaunt() ) )
		{
			ForceObserverMode( OBS_MODE_CHASE );
		}
	}

	BaseClass::ValidateCurrentObserverTarget();
}

void CCSPlayer::CheckObserverSettings( void )
{
	BaseClass::CheckObserverSettings();

	if ( m_bForcedObserverMode )
	{
		CCSPlayer *pPlayer = ToCSPlayer( m_hObserverTarget.Get() );
		if ( IsValidObserverTarget( pPlayer ) && !( pPlayer->IsTaunting() && pPlayer->IsThirdPersonTaunt() ) )
		{
			SetObserverMode( m_iObserverLastMode ); // switch to last mode
			m_bForcedObserverMode = false;	// disable force mode
		}
	}
}

void CCSPlayer::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	CWeaponCSBase *pCSWeapon = dynamic_cast< CWeaponCSBase* >( pWeapon );
	if ( pCSWeapon )
	{
		// For rifles, pistols, or the knife, drop our old weapon in this slot.
		if ( pCSWeapon->GetSlot() == WEAPON_SLOT_RIFLE || 
			pCSWeapon->GetSlot() == WEAPON_SLOT_PISTOL )
		{
			CBaseCombatWeapon *pDropWeapon = Weapon_GetSlot( pCSWeapon->GetSlot() );
			if ( pDropWeapon )
			{
				CSWeaponDrop( pDropWeapon, false, true );
			}
		}
		else if( pCSWeapon->GetWeaponType() == WEAPONTYPE_GRENADE || pCSWeapon->GetWeaponType() == WEAPONTYPE_STACKABLEITEM )
		{
			//if we already have this weapon, just add the ammo and destroy it
			if( Weapon_OwnsThisType( pCSWeapon->GetClassname() ) )
			{
				Weapon_EquipAmmoOnly( pWeapon );
				UTIL_Remove( pCSWeapon );

				RecalculateCurrentEquipmentValue();
				return;
			}
		}

		pCSWeapon->SetSolidFlags( FSOLID_NOT_SOLID );
 		pCSWeapon->SetOwnerEntity( this );
	}

	BaseClass::Weapon_Equip( pWeapon );
	
	// old players don't know how to unhide their world models a little bit into their deploys,
	// because old players don't have deploy animations at all.
	if ( !m_bUseNewAnimstate && pWeapon && pWeapon->GetWeaponWorldModel() )
	{
		pWeapon->ShowWeaponWorldModel( true );
	}

	RecalculateCurrentEquipmentValue();
}

bool CCSPlayer::Weapon_CanUse( CBaseCombatWeapon *pBaseWeapon )
{
	CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* >( pBaseWeapon );

	if ( pWeapon )
	{
		// we don't want bots picking up items for players in the world
		if ( IsBot() && CSGameRules() && CSGameRules()->IsPlayingCoopMission() )
		{
			if ( pWeapon->IsConstrained() || pWeapon->IsA( WEAPON_TAGRENADE ) )
				return false;
		}

		if ( pWeapon->IsA(WEAPON_TASER) && !pWeapon->HasAnyAmmo() )
			return false;

		if ( CanAcquire( pWeapon->GetCSWeaponID(), AcquireMethod::PickUp ) != AcquireResult::Allowed )
			return false;

		bool bAllowCustomKnifeSpawns = false;

		if ( !bAllowCustomKnifeSpawns && pWeapon->IsMeleeWeapon() && pWeapon->GetEconItemView() && 
			 !( pWeapon->GetEconItemView()->GetItemDefinition()->IsDefaultSlotItem() || pWeapon->GetEconItemView()->GetItemID() != 0 || FClassnameIs( pWeapon, "weapon_knifegg" ) ) )
		{
			return false;
		}
	}

	return true;
}

bool CCSPlayer::BumpWeapon( CBaseCombatWeapon *pBaseWeapon )
{
	CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* >( pBaseWeapon );
	if ( !pWeapon )
	{
		Assert( !pWeapon );
		pBaseWeapon->AddSolidFlags( FSOLID_NOT_SOLID );
		pBaseWeapon->AddEffects( EF_NODRAW );
		Weapon_Equip( pBaseWeapon );
		return true;
	}
	
	CBaseCombatCharacter *pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
	if ( pOwner || !Weapon_CanUse( pWeapon ) || !g_pGameRules->CanHavePlayerItem( this, pWeapon ) )
	{
		extern int gEvilImpulse101;
		if ( gEvilImpulse101 || CSGameRules()->IsPlayingGunGameDeathmatch() )
		{
			UTIL_Remove( pWeapon );
		}
		return false;
	}

	// Even if we already have a grenade in this slot, we can pickup another one if we don't already
	// own this type of grenade.
	bool bPickupGrenade =  ( pWeapon->GetWeaponType() == WEAPONTYPE_GRENADE );

	bool bStackableItem =  ( pWeapon->GetWeaponType() == WEAPONTYPE_STACKABLEITEM );
	/*
	// ----------------------------------------
	// If I already have it just take the ammo
	// ----------------------------------------
	if ( !bPickupGrenade && Weapon_SlotOccupied( pWeapon ) )
	{
		Weapon_EquipAmmoOnly( pWeapon );
		// Only remove me if I have no ammo left
		// Can't just check HasAnyAmmo because if I don't use clips, I want to be removed, 
		if ( pWeapon->UsesClipsForAmmo1() && pWeapon->HasPrimaryAmmo() )
			return false;

		UTIL_Remove( pWeapon );
		return false;
	}
	*/

	if ( HasShield() && pWeapon->CanBeUsedWithShield() == false )
		 return false;

	bool bPickupTaser = ( pWeapon->IsA( WEAPON_TASER ) );
	if ( bPickupTaser )
	{
		CBaseCombatWeapon *pOwnedTaser = CSWeapon_OwnsThisType( pWeapon->GetEconItemView() );
		if ( pOwnedTaser )
			return false;
	}

	bool bPickupC4 = ( pWeapon->GetWeaponType() == WEAPONTYPE_C4 );
	if ( bPickupC4 )
	{
		// we're only allowed to pick up one c4 at a time
		CBaseCombatWeapon *pC4 = Weapon_OwnsThisType( "weapon_c4" );
		if ( pC4 )
			return false;

		// see if we're trying to pick up the bomb without being able to "see" it
		// prevent picking it up through a thin wall
		float flDist = (pWeapon->GetAbsOrigin() - GetAbsOrigin()).AsVector2D().Length();
		if ( flDist > 34 )
		{
			trace_t tr;
			UTIL_TraceLine( pWeapon->GetAbsOrigin(), EyePosition(), MASK_VISIBLE, this, COLLISION_GROUP_DEBRIS, &tr );
			if ( tr.fraction < 1.0 )
				return false;
		}
	}

	// don't let AFK players catch the bomb
	if ( bPickupC4 && !m_bHasMovedSinceSpawn && CSGameRules()->GetRoundElapsedTime() > sv_spawn_afk_bomb_drop_time.GetFloat() 
		 && !CSGameRules()->IsPlayingCoopMission() )
	{
		return false;
	}

//	bool bPickupCarriableItem = ( pWeapon->GetCSWpnData().m_WeaponType == WEAPONTYPE_CARRIABLEITEM );
//	if ( bPickupCarriableItem && Weapon_SlotOccupied( pWeapon ) )
//	{
//		CBaseCarribleItem *pPickupItem = static_cast<CBaseCarribleItem*>(pWeapon);
//		if ( pPickupItem )
//		{
//			CBaseCarribleItem *pOwnedItem = static_cast<CBaseCarribleItem*>(Weapon_OwnsThisType( pPickupItem->GetClassname(), pPickupItem->GetSubType() ));
//			if ( pOwnedItem && pOwnedItem->GetCurrentItems() < pOwnedItem->GetMaxItems() )
//			{
//				pOwnedItem->AddAmmo( pPickupItem->GetCurrentItems() );
//				UTIL_Remove( pPickupItem );
//			}
//			
//			return false;
//		}
//	}

	if( bPickupC4 || bStackableItem || bPickupGrenade || bPickupTaser || /*bPickupCarriableItem || */ !Weapon_SlotOccupied( pWeapon ) )
	{
		// we have to do this here because picking up weapons placed in the world don't have their clips set
		// TODO: give the weapon a clip on spawn and not when picked up!
		if ( !pWeapon->GetPreviousOwner() )
			StockPlayerAmmo( pWeapon );

		SetPickedUpWeaponThisRound( true );
		pWeapon->CheckRespawn();

		pWeapon->AddSolidFlags( FSOLID_NOT_SOLID );
		pWeapon->AddEffects( EF_NODRAW );

		CCSPlayer* donor = pWeapon->GetDonor();
		if (donor )
		{
			CCS_GameStats.Event_PlayerDonatedWeapon(donor );
			pWeapon->SetDonor(NULL );
		}

		Weapon_Equip( pWeapon );

		// Made obsolete when ammo was moved from player to weapon
// 		int iExtraAmmo = pWeapon->GetExtraAmmoCount();
// 		
// 		if( iExtraAmmo /*&& !bPickupGrenade*/ /*&& !bPickupCarriableItem*/ )
// 		{
// 			//Find out the index of the ammo
// 			int iAmmoIndex = pWeapon->GetPrimaryAmmoType();
// 
// 			if( iAmmoIndex != -1 )
// 			{
// 				//Remove the extra ammo from the weapon
// 				pWeapon->SetExtraAmmoCount(0 );
// 
// 				//Give it to the player
// 				SetAmmoCount( iExtraAmmo, iAmmoIndex );
// 			}
// 		}

		bool bIsSilentPickup = ShouldPickupItemSilently( this );
		IGameEvent * event = gameeventmanager->CreateEvent( "item_pickup" );
		if( event )
		{
			const char *weaponName = pWeapon->GetClassname();
			if ( IsWeaponClassname( weaponName ) )
			{
				weaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
			}
			event->SetInt( "userid", GetUserID() );
			event->SetString( "item", weaponName );
			event->SetBool( "silent", bIsSilentPickup );
			gameeventmanager->FireEvent( event );
		}

		if ( !bIsSilentPickup )
			EmitSound( "Player.PickupWeapon" );

		return true;
	}

	return false;
}


void CCSPlayer::ResetStamina( void )
{
	m_flStamina = 0.0f;
}

void CCSPlayer::RescueZoneTouch( inputdata_t &inputdata )
{
	m_bInHostageRescueZone = true;
	if ( GetTeamNumber() == TEAM_CT && !(m_iDisplayHistoryBits & DHF_IN_RESCUE_ZONE ) )
	{
		HintMessage( "#Hint_hostage_rescue_zone", false );
		m_iDisplayHistoryBits |= DHF_IN_RESCUE_ZONE;
	}

	// if the player is carrying a hostage when he touches the rescue zone, pass the touch input to it
	if ( m_hCarriedHostage && m_hCarriedHostage.Get() )
	{
		variant_t emptyVariant;
		m_hCarriedHostage.Get()->AcceptInput( "OnRescueZoneTouch", NULL, NULL, emptyVariant, 0 );
	}
}

//------------------------------------------------------------------------------------------
CON_COMMAND_F( timeleft, "prints the time remaining in the match", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS )
{
	CCSPlayer *pPlayer = ToCSPlayer( UTIL_GetCommandClient() );
	if ( pPlayer && pPlayer->m_iNextTimeCheck >= gpGlobals->curtime )
	{
		return; // rate limiting
	}

	int iTimeRemaining = (int )CSGameRules()->GetMapRemainingTime();

	if ( iTimeRemaining < 0 )
	{
		if ( pPlayer )
		{
			ClientPrint( pPlayer, HUD_PRINTTALK, "#Game_no_timelimit" );
		}
		else
		{
			Msg( "* No Time Limit *\n" );
		}
	}
	else if ( iTimeRemaining == 0 )
	{
		if ( pPlayer )
		{
			ClientPrint( pPlayer, HUD_PRINTTALK, "#Game_last_round" );
		}
		else
		{
			Msg( "* Last Round *\n" );
		}
	}
	else
	{
		int iMinutes, iSeconds;
		iMinutes = iTimeRemaining / 60;
		iSeconds = iTimeRemaining % 60;

		char minutes[8];
		char seconds[8];

		Q_snprintf( minutes, sizeof(minutes ), "%d", iMinutes );
		Q_snprintf( seconds, sizeof(seconds ), "%2.2d", iSeconds );

		if ( pPlayer )
		{
			ClientPrint( pPlayer, HUD_PRINTTALK, "#Game_timelimit", minutes, seconds );
		}
		else
		{
			Msg( "Time Remaining:  %s:%s\n", minutes, seconds );
		}
	}

	if ( pPlayer )
	{
		pPlayer->m_iNextTimeCheck = gpGlobals->curtime + 1;
	}
}

//------------------------------------------------------------------------------------------
/**
 * Emit given sound that only we can hear
 */
void CCSPlayer::EmitPrivateSound( const char *soundName )
{
	CSoundParameters params;
	if (!GetParametersForSound( soundName, params, NULL ) )
		return;

	CSingleUserRecipientFilter filter( this );
	EmitSound( filter, entindex(), soundName );
}

//=====================
//==============================================
//AutoBuy - do the work of deciding what to buy
//==============================================
void CCSPlayer::AutoBuy( const char *autobuyString )
{
	if ( !IsInBuyZone() )
	{
		EmitPrivateSound( "BuyPreset.CantBuy" );
		return;
	}

	if ( !autobuyString || !*autobuyString )
	{
		EmitPrivateSound( "BuyPreset.AlreadyBought" );
		return;
	}

	bool boughtPrimary = false, boughtSecondary = false;

	m_bIsInAutoBuy = true;
	ParseAutoBuyString(autobuyString, boughtPrimary, boughtSecondary );
	m_bIsInAutoBuy = false;

	m_bAutoReload = true;

	//TODO ?: stripped out all the attempts to buy career weapons.
	// as we're not porting cs:cz, these were skipped
}

int	CCSPlayer::GetAccountBalance( void )
{ 
	if ( CSGameRules() && CSGameRules()->IsPlayingGunGameDeathmatch() )
		return 99999;

	return m_iAccount;
}

void CCSPlayer::ParseAutoBuyString(const char *string, bool &boughtPrimary, bool &boughtSecondary )
{
	char command[32];
	int nBuffSize = sizeof(command ) - 1; // -1 to leave space for the NUL at the end of the string
	const char *c = string;

	if (c == NULL )
	{
		EmitPrivateSound( "BuyPreset.AlreadyBought" );
		return;
	}

	BuyResult_e overallResult = BUY_ALREADY_HAVE;

	// loop through the string of commands, trying each one in turn.
	while (*c != 0 )
	{
		int i = 0;
		// copy the next word into the command buffer.
		while ((*c != 0 ) && (*c != ' ' ) && (i < nBuffSize))
		{
			command[i] = *(c );
			++c;
			++i;
		}
		if (*c == ' ' )
		{
			++c; // skip the space.
		}

		command[i] = 0; // terminate the string.

		// clear out any spaces.
		i = 0;
		while (command[i] != 0 )
		{
			if (command[i] == ' ' )
			{
				command[i] = 0;
				break;
			}
			++i;
		}

		// make sure we actually have a command.
		if (strlen(command ) == 0 )
		{
			continue;
		}

		AutoBuyInfoStruct * commandInfo = GetAutoBuyCommandInfo(command );

		if (ShouldExecuteAutoBuyCommand(commandInfo, boughtPrimary, boughtSecondary ) )
		{
			BuyResult_e result = HandleCommand_Buy( command, commandInfo->m_LoadoutPosition );

			overallResult = CombineBuyResults( overallResult, result );

			// check to see if we actually bought a primary or secondary weapon this time.
			PostAutoBuyCommandProcessing(commandInfo, boughtPrimary, boughtSecondary );
		}
	}

	if ( overallResult == BUY_CANT_AFFORD )
	{
		EmitPrivateSound( "BuyPreset.CantBuy" );
	}
	else if ( overallResult == BUY_ALREADY_HAVE )
	{
		EmitPrivateSound( "BuyPreset.AlreadyBought" );
	}
}

BuyResult_e CCSPlayer::CombineBuyResults( BuyResult_e prevResult, BuyResult_e newResult )
{
	if ( newResult == BUY_BOUGHT )
	{
		prevResult = BUY_BOUGHT;
	}
	else if ( prevResult != BUY_BOUGHT &&
		(newResult == BUY_CANT_AFFORD || newResult == BUY_INVALID_ITEM || newResult == BUY_PLAYER_CANT_BUY ) )
	{
		prevResult = BUY_CANT_AFFORD;
	}

	return prevResult;
}

//==============================================
//PostAutoBuyCommandProcessing
//==============================================
void CCSPlayer::PostAutoBuyCommandProcessing(const AutoBuyInfoStruct *commandInfo, bool &boughtPrimary, bool &boughtSecondary )
{
	if (commandInfo == NULL )
	{
		return;
	}

	CBaseCombatWeapon *pPrimary = Weapon_GetSlot( WEAPON_SLOT_RIFLE );
	CBaseCombatWeapon *pSecondary = Weapon_GetSlot( WEAPON_SLOT_PISTOL );

	if ((pPrimary != NULL ) && (stricmp(pPrimary->GetClassname(), commandInfo->m_classname ) == 0))
	{
		// I just bought the gun I was trying to buy.
		boughtPrimary = true;
	}
	else if ((pPrimary == NULL ) && ((commandInfo->m_class & AUTOBUYCLASS_SHIELD ) == AUTOBUYCLASS_SHIELD) && HasShield())
	{
		// the shield is a primary weapon even though it isn't a "real" weapon.
		boughtPrimary = true;
	}
	else if ((pSecondary != NULL ) && (stricmp(pSecondary->GetClassname(), commandInfo->m_classname ) == 0))
	{
		// I just bought the pistol I was trying to buy.
		boughtSecondary = true;
	}
}

bool CCSPlayer::ShouldExecuteAutoBuyCommand(const AutoBuyInfoStruct *commandInfo, bool boughtPrimary, bool boughtSecondary )
{
	if (commandInfo == NULL )
	{
		return false;
	}

	if ((boughtPrimary ) && ((commandInfo->m_class & AUTOBUYCLASS_PRIMARY ) != 0) && ((commandInfo->m_class & AUTOBUYCLASS_AMMO) == 0))
	{
		// this is a primary weapon and we already have one.
		return false;
	}

	if ((boughtSecondary ) && ((commandInfo->m_class & AUTOBUYCLASS_SECONDARY ) != 0) && ((commandInfo->m_class & AUTOBUYCLASS_AMMO) == 0))
	{
		// this is a secondary weapon and we already have one.
		return false;
	}

	if( commandInfo->m_class & AUTOBUYCLASS_ARMOR && ArmorValue() >= 100 )
	{
		return false;
	}

	return true;
}

AutoBuyInfoStruct *CCSPlayer::GetAutoBuyCommandInfo(const char *command )
{
	int i = 0;
	AutoBuyInfoStruct *ret = NULL;
	AutoBuyInfoStruct *temp = &(g_autoBuyInfo[i] );

	// loop through all the commands till we find the one that matches.
	while ((ret == NULL ) && (temp->m_class != (AutoBuyClassType )0))
	{
		temp = &(g_autoBuyInfo[i] );
		++i;

		if (stricmp(temp->m_command, command ) == 0 )
		{
			ret = temp;
		}
	}

	return ret;
}

//==============================================
//PostAutoBuyCommandProcessing
//- reorders the tokens in autobuyString based on the order of tokens in the priorityString.
//==============================================
void CCSPlayer::PrioritizeAutoBuyString(char *autobuyString, const char *priorityString )
{
	char newString[256];
	int newStringPos = 0;
	char priorityToken[32];

	if ((priorityString == NULL ) || (autobuyString == NULL ))
	{
		return;
	}

	const char *priorityChar = priorityString;

	while (*priorityChar != 0 )
	{
		int i = 0;

		// get the next token from the priority string.
		while ((*priorityChar != 0 ) && (*priorityChar != ' ' ))
		{
			priorityToken[i] = *priorityChar;
			++i;
			++priorityChar;
		}
		priorityToken[i] = 0;

		// skip spaces
		while (*priorityChar == ' ' )
		{
			++priorityChar;
		}

		if (strlen(priorityToken ) == 0 )
		{
			continue;
		}

		// see if the priority token is in the autobuy string.
		// if  it is, copy that token to the new string and blank out
		// that token in the autobuy string.
		char *autoBuyPosition = strstr(autobuyString, priorityToken );
		if (autoBuyPosition != NULL )
		{
			while ((*autoBuyPosition != 0 ) && (*autoBuyPosition != ' ' ))
			{
				newString[newStringPos] = *autoBuyPosition;
				*autoBuyPosition = ' ';
				++newStringPos;
				++autoBuyPosition;
			}

			newString[newStringPos++] = ' ';
		}
	}

	// now just copy anything left in the autobuyString to the new string in the order it's in already.
	char *autobuyPosition = autobuyString;
	while (*autobuyPosition != 0 )
	{
		// skip spaces
		while (*autobuyPosition == ' ' )
		{
			++autobuyPosition;
		}

		// copy the token over to the new string.
		while ((*autobuyPosition != 0 ) && (*autobuyPosition != ' ' ))
		{
			newString[newStringPos] = *autobuyPosition;
			++newStringPos;
			++autobuyPosition;
		}

		// add a space at the end.
		newString[newStringPos++] = ' ';
	}

	// terminate the string.  Trailing spaces shouldn't matter.
	newString[newStringPos] = 0;

	Q_snprintf(autobuyString, sizeof(autobuyString ), "%s", newString );
}

bool CCSPlayer::AttemptToBuyDMBonusWeapon( void )
{
	if ( !CSGameRules() || !( CSGameRules()->IsDMBonusActive() ) )
		return false;

	loadout_positions_t unPosition = CSGameRules()->GetDMBonusWeaponLoadoutSlot();

	const CBaseCombatWeapon *pHaveWeapon = Weapon_GetPosition( unPosition );


	// If we already have the bonus weapon, switch to it.
	if ( pHaveWeapon )
	{
		switch( pHaveWeapon->GetSlot() )
		{
		case WEAPON_SLOT_RIFLE:
			engine->ClientCommand( edict(), "slot1\n");
			break;
		case WEAPON_SLOT_PISTOL:
			engine->ClientCommand( edict(), "slot2\n");
			break;
		case WEAPON_SLOT_KNIFE:
			engine->ClientCommand( edict(), "slot3\n");
			break;
		}

		m_bIsRespawningForDMBonus = false;
		m_bHasUsedDMBonusRespawn = true;
		return true;
	}
	
	// Otherwise, if we don't own it but can buy it, buy it.
	if ( CanPlayerBuy( false ) )
	{
		CEconItemView* pItem = Inventory()->GetItemInLoadout( GetTeamNumber(), unPosition );

		BuyResult_e buyresult = HandleCommand_Buy( pItem->GetItemDefinition()->GetDefinitionName(), unPosition, false ); 

		if ( buyresult == BUY_BOUGHT )
		{

			CSWeaponID wid = WeaponIdFromString( pItem->GetStaticData()->GetItemClass() );
			const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( wid );
			int iSlot = pWeaponInfo->GetBucketSlot( pItem );

			switch ( iSlot )
			{
			case WEAPON_SLOT_RIFLE:
				engine->ClientCommand( edict(), "slot1\n" );
				break;
			case WEAPON_SLOT_PISTOL:
				engine->ClientCommand( edict(), "slot2\n" );
				break;
			case WEAPON_SLOT_KNIFE:
				engine->ClientCommand( edict(), "slot3\n" );
				break;
			}
			
		}

		m_bIsRespawningForDMBonus = false;
		m_bHasUsedDMBonusRespawn = true;

		return true;
	}

	// otherwise respawn, which will end up in the 'buy it' case above.

	if ( !m_bHasUsedDMBonusRespawn )
	{
		m_bIsRespawningForDMBonus = true;
		m_bHasUsedDMBonusRespawn = true;

		RecordRebuyStructLastRound();
		ForceRespawn();

		return true;
	}

	return false;
}

//==============================================================
// ReBuy
// system for attempting to buy the weapons you had last round
//==============================================================


void CCSPlayer::AddToRebuy( CSWeaponID weaponId, int nPos )
{
	if ( weaponId == ITEM_NVG )
	{
		m_rebuyStruct.SetNightVision( true );
		return;
	}

	if ( weaponId == ITEM_KEVLAR )
	{
		m_rebuyStruct.SetArmor( 1 );
		return;
	}

	if ( weaponId == ITEM_ASSAULTSUIT )
	{
		m_rebuyStruct.SetArmor( 2 );
		return;
	}

	if ( weaponId == ITEM_DEFUSER )
	{
		m_rebuyStruct.SetDefuser( true );
		return;
	}

	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponId );

	// TODO: Add special handling for equipment without info data?
	if ( pWeaponInfo == NULL )
		return;

	switch ( pWeaponInfo->iSlot )
	{
	case WEAPON_SLOT_RIFLE:
		m_rebuyStruct.SetPrimary( nPos );
		break;

	case WEAPON_SLOT_PISTOL:
		m_rebuyStruct.SetSecondary( nPos );
		break;

	case WEAPON_SLOT_KNIFE:
		m_rebuyStruct.SetTertiary( weaponId );
		break;

	case WEAPON_SLOT_GRENADES:
		AddToGrenadeRebuy( weaponId );
		break;

	case WEAPON_SLOT_C4:
		break;
		
	default:
		Error( "Unhandled weapon slot (%i) in AddToRebuy\n", pWeaponInfo->iSlot );
		break;
	}
}

void CCSPlayer::AddToGrenadeRebuy( CSWeaponID weaponId )
{
	int iQueueSize = MIN( ammo_grenade_limit_total.GetInt(), m_rebuyStruct.numGrenades() );

	// see if it's already in the list
	for ( int i = 0; i < iQueueSize; ++i )
	{
		if ( m_rebuyStruct.GetGrenade( i ) == weaponId )
			return;
	}

	// shift the list down
	for ( int i = m_rebuyStruct.numGrenades() - 1; i > 0; --i )
	{
		m_rebuyStruct.SetGrenade( i, m_rebuyStruct.GetGrenade( i - 1 ) );
	}

	// add it to the front
	m_rebuyStruct.SetGrenade( 0, weaponId );
}

void CCSPlayer::Rebuy( const char *rebuyString )
{
	if ( !IsInBuyZone() )
	{
		EmitPrivateSound( "BuyPreset.CantBuy" );
		return;
	}

	if ( !rebuyString || !*rebuyString )
	{
		EmitPrivateSound( "BuyPreset.AlreadyBought" );
		return;
	}

	m_bIsInRebuy = true;
	BuyResult_e overallResult = BUY_ALREADY_HAVE;

	char token[256];
	rebuyString = engine->ParseFile( rebuyString, token, sizeof( token ) );

	while (rebuyString != NULL )
	{
		BuyResult_e result = BUY_ALREADY_HAVE;

		if ( Q_strcasecmp( token, "PrimaryWeapon" ) == 0 )
		{
			result = RebuyPrimaryWeapon();
		}
		else if ( Q_strcasecmp(token, "SecondaryWeapon" ) == 0 )
		{
			result = RebuySecondaryWeapon();
		}
		else if ( Q_strcasecmp(token, "Taser" ) == 0 )		// TODO[pmf]: handle this better
		{
			result = RebuyTaser();
		}
		else if ( Q_strcasecmp(token, "Armor" ) == 0 )
		{
			result = RebuyArmor();
		}
		else if ( Q_strcasecmp(token, "Defuser" ) == 0 )
		{
			result = RebuyDefuser();
		}
		else if ( Q_strcasecmp(token, "NightVision" ) == 0 )
		{
			result = RebuyNightVision();
		}
		else
		{
			CSWeaponID weaponId = AliasToWeaponID( token );
			if ( weaponId != WEAPON_NONE )
				result = RebuyGrenade( weaponId );
		}

		overallResult = CombineBuyResults( overallResult, result );

		rebuyString = engine->ParseFile( rebuyString, token, sizeof( token ) );
	}

	m_bIsInRebuy = false;

	// after we're done buying, the user is done with their equipment purchasing experience.
	// so we are effectively out of the buy zone.
//	if (TheTutor != NULL )
//	{
//		TheTutor->OnEvent(EVENT_PLAYER_LEFT_BUY_ZONE );
//	}

	m_bAutoReload = true;

	if ( overallResult == BUY_CANT_AFFORD )
	{
		EmitPrivateSound( "BuyPreset.CantBuy" );
	}
	else if ( overallResult == BUY_ALREADY_HAVE )
	{
		EmitPrivateSound( "BuyPreset.AlreadyBought" );
	}
}

BuyResult_e CCSPlayer::RebuyPrimaryWeapon()
{
	CBaseCombatWeapon *primary = Weapon_GetSlot( WEAPON_SLOT_RIFLE );
	if (primary != NULL )
		return BUY_ALREADY_HAVE;	// don't drop primary weapons via rebuy - if the player picked up a different weapon, he wants to keep it.

	int nPos = m_rebuyStructLastRound.GetPrimary();
	if ( nPos != 0 )
	{
		return HandleCommand_Buy( "", nPos );
	}

	return BUY_INVALID_ITEM;
}

BuyResult_e CCSPlayer::RebuySecondaryWeapon()
{
	CBaseCombatWeapon *pistol = Weapon_GetSlot( WEAPON_SLOT_PISTOL );
	if (pistol != NULL && !m_bUsingDefaultPistol )
		return BUY_ALREADY_HAVE;	// don't drop pistols via rebuy if we've bought one other than the default pistol

	int nPos = m_rebuyStructLastRound.GetSecondary();
	if ( nPos != 0 )
	{
		// skip pistol rebuy if the deathmatch bonus weapon is the default pistol
		if ( m_bIsRespawningForDMBonus &&
			( CSGameRules()->GetDMBonusWeaponLoadoutSlot() == LOADOUT_POSITION_SECONDARY0 ) )
		{
			return BUY_BOUGHT;
		}
		else
		{
			return HandleCommand_Buy( "", nPos );
		}
	}

	return BUY_INVALID_ITEM;
}

BuyResult_e CCSPlayer::RebuyTaser()
{
	if ( m_rebuyStructLastRound.GetTertiary() == WEAPON_TASER )
		return HandleCommand_Buy( "taser", LOADOUT_POSITION_EQUIPMENT2 );

	return BUY_INVALID_ITEM;
}

BuyResult_e CCSPlayer::RebuyGrenade( CSWeaponID weaponId )
{
	int iQueueSize = MIN( ammo_grenade_limit_total.GetInt(), m_rebuyStructLastRound.numGrenades() );

	// is it in the rebuy list
	for ( int i = 0; i < iQueueSize; ++i )
	{
		if ( m_rebuyStructLastRound.GetGrenade( i ) == weaponId )
		{
			int nPos = -1;

			char wpnClassName[ MAX_WEAPON_STRING ];
			wpnClassName[ 0 ] = '\0';
			V_sprintf_safe( wpnClassName, "weapon_%s", WeaponIDToAlias( weaponId ) );

			const CCStrike15ItemDefinition * pItemDef = dynamic_cast< const CCStrike15ItemDefinition * >( GetItemSchema()->GetItemDefinitionByName( wpnClassName ) );
			
			Assert( pItemDef );

			nPos = pItemDef->GetLoadoutSlot( GetTeamNumber() );
			
			return HandleCommand_Buy( WeaponIDToAlias( weaponId ), nPos );
		}
	}
	return BUY_INVALID_ITEM;
}

BuyResult_e CCSPlayer::RebuyDefuser()
{
	if ( m_rebuyStructLastRound.GetDefuser() )
	{
		if ( HasDefuser() )
			return BUY_ALREADY_HAVE;
		else 
			return HandleCommand_Buy( "defuser" );
	}

	return BUY_INVALID_ITEM;
}

BuyResult_e CCSPlayer::RebuyNightVision()
{
	if ( m_rebuyStructLastRound.GetNightVision() )
	{
		if ( m_bHasNightVision )
			return BUY_ALREADY_HAVE;
		else
			return HandleCommand_Buy( "nvgs" );
	}

	return BUY_INVALID_ITEM;
}
 

BuyResult_e CCSPlayer::RebuyArmor()
{
	if (m_rebuyStructLastRound.GetArmor() > 0 )
	{
		int armor = 0;

		if( m_bHasHelmet )
			armor = 2;
		else if( ArmorValue() > 0 )
			armor = 1;

		if( armor < m_rebuyStructLastRound.GetArmor() )
		{
			if (m_rebuyStructLastRound.GetArmor() == 1 )
			{
				return HandleCommand_Buy("vest" );
			}
			else
			{
				return HandleCommand_Buy("vesthelm" );
			}
		}
	}

	return BUY_ALREADY_HAVE;
}


static void BuyRandom( void )
{
	CCSPlayer *player = ToCSPlayer( UTIL_GetCommandClient() );

	if ( !player )
		return;

		player->BuyRandom();
}

static ConCommand buyrandom( "buyrandom", BuyRandom, "Buy random primary and secondary. Primarily for deathmatch where cost is not an issue.", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS );


void CCSPlayer::BuyRandom( void )
{
	if ( !IsInBuyZone() )
	{
		EmitPrivateSound( "BuyPreset.CantBuy" );
		return;
	}

	m_bIsInAutoBuy = true;
	// Make lists of primary and secondary weapons.
	CUtlVector< int > primaryweapons;
	CUtlVector< int > secondaryweapons;

	for ( int w = WEAPON_FIRST; w < WEAPON_LAST; w++ )
	{
		const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( (CSWeaponID)w );
		if ( pWeaponInfo )
		{
			bool isRifle = pWeaponInfo->iSlot == WEAPON_SLOT_RIFLE;
			bool isPistol = pWeaponInfo->iSlot == WEAPON_SLOT_PISTOL;
			bool isTeamAppropriate = ( ( pWeaponInfo->GetUsedByTeam() == GetTeamNumber() ) ||
										( pWeaponInfo->GetUsedByTeam() == TEAM_UNASSIGNED ) );

			if ( isRifle && isTeamAppropriate )
			{
				primaryweapons.AddToTail( w );
			}
			else if ( isPistol && isTeamAppropriate )
			{
				secondaryweapons.AddToTail( w );
			}

//			Msg( "%i, %s, %s, %i\n", w, pWeaponInfo->szClassName, ( isRifle ? "primary" : ( isPistol ? "secondary" : "other" ) ), isTeamAppropriate );
		}
//		else
//		{
//			Msg( "%i, %s\n", w, "*********DOESN'T EXIST" );
//		}
	}

	// randomly pick one of each.
	int primaryToBuy = random->RandomInt( 1, primaryweapons.Count() );
	int secondaryToBuy = random->RandomInt( 1, secondaryweapons.Count() );

//	Msg( "random pick: p: %i, s: %i", primaryweapons[primaryToBuy], secondaryweapons[secondaryToBuy] );

	// buy
	// TODO: get itemid
	HandleCommand_Buy( WeaponIDToAlias( primaryweapons[primaryToBuy - 1] ) );
	HandleCommand_Buy( WeaponIDToAlias( secondaryweapons[secondaryToBuy - 1] ) );
	m_bIsInAutoBuy = false;
}


bool CCSPlayer::IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps )
{
	// High priority entities go through a different use code path requiring
	// other conditions like distance and view angles to be satisfied
	CConfigurationForHighPriorityUseEntity_t cfgUseEntity;
	if ( GetUseConfigurationForHighPriorityUseEntity( pEntity, cfgUseEntity ) )
		return false;

	CWeaponCSBase *pCSWepaon = dynamic_cast<CWeaponCSBase*>(pEntity );

	if( pCSWepaon )
	{
		// we can't USE dropped weapons 
		return true;
	}

	CBaseCSGrenadeProjectile *pGrenade = dynamic_cast<CBaseCSGrenadeProjectile*>(pEntity );
	if ( pGrenade )
	{
		// we can't USE thrown grenades
	}

	CPropVehicle *pVehicle = dynamic_cast<CPropVehicle*>(pEntity );
	if ( pVehicle )
	{
		return true;
	}

	return BaseClass::IsUseableEntity( pEntity, requiredCaps );
}

CBaseEntity *CCSPlayer::FindUseEntity()
{
	CBaseEntity *entity = NULL;

	// Check to see if the bomb is close enough to use before attempting to use anything else.

	entity = GetUsableHighPriorityEntity();
	
	if ( entity== NULL && !CSGameRules()->IsPlayingGunGame() && !CSGameRules()->IsPlayingTraining() )
	{
		Vector aimDir;
		AngleVectors( EyeAngles(), &aimDir );

		trace_t result;
		UTIL_TraceLine( EyePosition(), EyePosition() + MAX_WEAPON_NAME_POPUP_RANGE * aimDir, MASK_ALL, this, COLLISION_GROUP_NONE, &result );

		if ( result.DidHitNonWorldEntity() && result.m_pEnt->IsBaseCombatWeapon() )
		{

				CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase * >( result.m_pEnt );
				CSWeaponType nType = pWeapon->GetWeaponType();
				if ( CSGameRules()->IsPlayingCoopMission() || IsPrimaryOrSecondaryWeapon( nType ) )
				{
					entity = pWeapon;
				}

		}
	}

	if ( entity == NULL )
	{
		entity = BaseClass::FindUseEntity();
	}

	return entity;
}

void CCSPlayer::StockPlayerAmmo( CBaseCombatWeapon *pNewWeapon )
{
	// this function not only gives extra ammo, but also the needed default ammo for the default clip
	CWeaponCSBase *pWeapon =  dynamic_cast< CWeaponCSBase * >( pNewWeapon );

	if ( pWeapon )
	{
		if ( pWeapon->GetWpnData().iFlags & ITEM_FLAG_EXHAUSTIBLE )
			return;

		int nAmmo = pWeapon->GetPrimaryAmmoType();

		if ( nAmmo != -1 )
		{
			if ( !CSGameRules()->IsPlayingTraining() )
				pWeapon->SetReserveAmmoCount( AMMO_POSITION_PRIMARY, 9999 );

			pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
		}
		
		return;
	}

	pWeapon = dynamic_cast< CWeaponCSBase * >(Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );

	if ( pWeapon )
	{
		int nAmmo = pWeapon->GetPrimaryAmmoType();

		if ( nAmmo != -1 )
		{
			if ( !CSGameRules()->IsPlayingTraining() )
				pWeapon->SetReserveAmmoCount( AMMO_POSITION_PRIMARY, 9999 );

			pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
		}
	}

	pWeapon = dynamic_cast< CWeaponCSBase * >(Weapon_GetSlot( WEAPON_SLOT_PISTOL ) );

	if ( pWeapon )
	{
		int nAmmo = pWeapon->GetPrimaryAmmoType();

		if ( nAmmo != -1 )
		{
			if ( !CSGameRules()->IsPlayingTraining() )
				pWeapon->SetReserveAmmoCount( AMMO_POSITION_PRIMARY, 9999 );

			pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
		}
	}
}

void CCSPlayer::FindMatchingWeaponsForTeamLoadout( const char *pchName, int nTeam, bool bMustBeTeamSpecific, CUtlVector< CEconItemView* > &matchingWeapons )
{
	/** Removed for partner depot **/
}

CBaseEntity	*CCSPlayer::GiveNamedItem( const char *pchName, int iSubType /*= 0*/, CEconItemView *pScriptItem /*= NULL*/, bool bForce /*= false*/ )
{
	if ( !pchName || !pchName[0] )
		return  NULL;

	CBaseEntity *pItem = NULL;

	if ( ( !pScriptItem || !pScriptItem->IsValid() ) && !( CSGameRules() && CSGameRules()->IsPlayingTraining() ) )
	{
		CUtlVector< CEconItemView* > matchingWeapons;
		FindMatchingWeaponsForTeamLoadout( pchName, GetTeamNumber(), false, matchingWeapons );

		if ( matchingWeapons.Count() )
		{
			pScriptItem = matchingWeapons[ RandomInt( 0, matchingWeapons.Count() - 1 ) ];
		}
		else if ( CSGameRules() && CSGameRules()->IsPlayingGunGame() )
		{
			// In gun game it can give the painted version of team specific items
			FindMatchingWeaponsForTeamLoadout( pchName, ( GetTeamNumber() != TEAM_TERRORIST ? TEAM_TERRORIST : TEAM_CT ), true, matchingWeapons );

			if ( matchingWeapons.Count() )
			{
				pScriptItem = matchingWeapons[ RandomInt( 0, matchingWeapons.Count() - 1 ) ];
			}
		}
	}
#if !defined( NO_STEAM_GAMECOORDINATOR )
	if ( pScriptItem && pScriptItem->IsValid() )
	{
		// Generate a weapon directly from that item
		pItem = ItemGeneration()->GenerateItemFromScriptData( pScriptItem, GetLocalOrigin(), vec3_angle, pScriptItem->GetStaticData()->GetItemClass() );
	}
	else
	{
		// Generate a base item of the specified type
		CItemSelectionCriteria criteria;
		criteria.SetQuality( AE_NORMAL );
		criteria.BAddCondition( "name", k_EOperator_String_EQ, pchName, true );
		pItem = ItemGeneration()->GenerateRandomItem( &criteria, GetAbsOrigin(), vec3_angle );

		if ( !pItem )
		{
			criteria.SetQuality( AE_UNIQUE );
			pItem = ItemGeneration()->GenerateRandomItem( &criteria, GetAbsOrigin(), vec3_angle );
		}
	}
#endif
	if ( pItem == NULL )
	{
		// Trap for guns that 'exist' but never shipped.
		// Doing this here rather than removing the entities
		// so we don't disrupt demos.
		if ( V_strcmp( pchName, "weapon_galil" ) &&
			V_strcmp( pchName, "weapon_mp5navy" ) &&
			V_strcmp( pchName, "weapon_p228" ) &&
			V_strcmp( pchName, "weapon_scar17" ) &&
			V_strcmp( pchName, "weapon_scout" ) &&
			V_strcmp( pchName, "weapon_sg550" ) &&
			V_strcmp( pchName, "weapon_tmp" ) &&
			V_strcmp( pchName, "weapon_usp" ) )
		{
			pItem = CreateEntityByName( pchName );
		}

		if ( pItem == NULL )
		{
			Msg( "NULL Ent in GiveNamedItem!\n" );
			return NULL;
		}
//		Msg( "%s is missing an item definition in the schema.\n", pchName );
	}

	Vector pos = ( GetLocalOrigin() + Weapon_ShootPosition() ) * 0.5f;
	QAngle angles;

	MDLCACHE_CRITICAL_SECTION();
	int weaponBoneAttachment = LookupAttachment( "weapon_hand_R" );

	if ( weaponBoneAttachment == 0 )
		weaponBoneAttachment = LookupAttachment( "weapon_bone" );

	if ( weaponBoneAttachment == 0 || !GetAttachment( weaponBoneAttachment, pos, angles ) )
	{
		Warning("Missing weapon hand bone attachment for player model.\n");
	}

	pItem->SetLocalOrigin( pos );
	pItem->AddSpawnFlags( SF_NORESPAWN );

	CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon*>( pItem );
	if ( pWeapon )
	{
		if ( iSubType )
		{
			pWeapon->SetSubType( iSubType );
		}
	}

	// is this item prohibited by mp_weapons_disallowed
	if ( CSGameRules() )
	{
		int nDefIndex = 0;

		if ( pScriptItem )
		{
			nDefIndex = pScriptItem->GetItemDefinition()->GetDefinitionIndex();
		}
		else
		{
			CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinitionByName( pItem->GetClassname() );

			if ( pItemDef )
			{
				nDefIndex = pItemDef->GetDefinitionIndex();
			}
		}

		if ( nDefIndex != 0 )
		{
			for ( int i = 0; i < MAX_PROHIBITED_ITEMS; i++ )
			{
				if ( CSGameRules()->m_arrProhibitedItemIndices[ i ] == nDefIndex )
					return NULL;
			}
		}
	}

	DispatchSpawn( pItem );

	m_bIsBeingGivenItem = true;
	if ( pItem != NULL && !(pItem->IsMarkedForDeletion() ) ) 
	{
		CItem *pItemEnt = dynamic_cast< CItem* >( pItem );
		if ( pItemEnt )
		{
			pItemEnt->ItemForceTouch( this );
		}
		else
		{
			pItem->Touch( this );
		}
	}
	m_bIsBeingGivenItem = false;

	// this function not only gives extra ammo, but also the needed default ammo for the default clip
	StockPlayerAmmo( pWeapon );

	if ( StringHasPrefix( pchName, "weapon_molotov" ) )
	{
		// Set up molotov use time since the molotov cannot be used right away
		ConVarRef mp_molotovusedelay( "mp_molotovusedelay" );
		m_fMolotovUseTime = gpGlobals->curtime + mp_molotovusedelay.GetFloat();
	}

	// Send a game event for getting the c4
	if ( StringHasPrefix( pchName, "weapon_c4" ) )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_given_c4" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			gameeventmanager->FireEvent( event );
		}
	}

#if WEARABLE_VEST_IFF_KEVLAR
	if ( ( StringHasPrefix( pchName, "item_kevlar" ) ) ||
		 ( StringHasPrefix( pchName, "item_heavyassaultsuit" ) ) ||
		 ( StringHasPrefix( pchName, "item_assaultsuit" ) ) )	 
	{
		GiveWearableFromSlot( LOADOUT_POSITION_VEST );
	}
#endif

	return pItem;
}

bool CCSPlayer::CanUseGrenade( CSWeaponID nID )
{
	if ( nID == WEAPON_MOLOTOV )
	{
		if ( gpGlobals->curtime < m_fMolotovUseTime )
		{
			// Can't use molotov until timer elapses
			return false;
		}
	}

	return true;
}

void CCSPlayer::DoAnimStateEvent( PlayerAnimEvent_t evt )
{
	m_PlayerAnimState->DoAnimationEvent( evt );
}

void CCSPlayer::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	if ( m_bUseNewAnimstate )
	{
		// run the event on the server
		m_PlayerAnimStateCSGO->DoAnimationEvent( event, nData );
		return;
	}
	else
	{
		if ( event == PLAYERANIMEVENT_THROW_GRENADE )
		{
			// Grenade throwing has to synchronize exactly with the player's grenade weapon going away,
			// and events get delayed a bit, so we let CCSPlayerAnimState pickup the change to this
			// variable.
			m_iThrowGrenadeCounter = (m_iThrowGrenadeCounter+1 ) % (1<<THROWGRENADE_COUNTER_BITS );
		}
		else
		{
			m_PlayerAnimState->DoAnimationEvent( event, nData );
			TE_PlayerAnimEvent( this, event, nData );	// Send to any clients who can see this guy.
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CCSPlayer::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT );
}

extern ConVar flashlight;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CCSPlayer::FlashlightTurnOn( bool playSound /*= false*/ )
{
	if( flashlight.GetInt() > 0 && IsAlive() )
	{
		AddEffects( EF_DIMLIGHT );
		EmitSound( "Player.FlashlightOn" );
	}
	else
	{
		engine->ClientCommand( edict(), "+lookatweapon\n" );
		engine->ClientCommand( edict(), "-lookatweapon\n" );
	}

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSPlayer::FlashlightTurnOff( bool playSound /*= false*/ )
{
	RemoveEffects( EF_DIMLIGHT );
	
	if( IsAlive() )
	{
		EmitSound( "Player.FlashlightOff" );
	}
}

void CCSPlayer::SetViewModelArms( const char *armsModel )
{
	V_strncpy( m_szArmsModel.GetForModify(), armsModel, MAX_MODEL_STRING_SIZE );
}

void CCSPlayer::ReportCustomClothingModels( void )
{
	if ( !CSInventoryManager() )
		return;

	for ( int nSlot = LOADOUT_POSITION_FIRST_COSMETIC; nSlot <= LOADOUT_POSITION_LAST_COSMETIC; ++nSlot )
	{
#if !defined(NO_STEAM)
		if ( steamgameserverapicontext->SteamGameServer() )
		{
			CSteamID steamIDForPlayer;
			if ( GetSteamID( &steamIDForPlayer ) )
			{
				CEconItemView *pItemData = CSInventoryManager()->GetItemInLoadoutForTeam( GetTeamNumber(), nSlot, &steamIDForPlayer );

				const char *pszItemName = pItemData->GetStaticData()->GetDefinitionName();

				DevMsg( "             %s: %s \n", g_szLoadoutStrings[ nSlot ], pszItemName );
			}
		}
	}
#endif
}

bool CCSPlayer::HandleDropWeapon( CBaseCombatWeapon *pWeapon, bool bSwapping )
{

	CWeaponCSBase *pCSWeapon = dynamic_cast< CWeaponCSBase* >( pWeapon ? pWeapon : GetActiveWeapon() );

	if( pCSWeapon )
	{
/*
		CBaseCarribleItem *pItem = dynamic_cast< CBaseCarribleItem * >( pCSWeapon );
		if ( pItem  )
		{
			pItem->DropItem();
				
			// decrement the ammo
			pItem->DecrementAmmo( this );
			// if that was the last item, delete this one
			if ( pItem->GetCurrentItems() <= 0 )
			{
				CSWeaponDrop( pItem, true, true );
				UTIL_Remove( pItem );
				UpdateAddonBits();
			}

			return false;
		}
*/
		if ( mp_death_drop_c4.GetBool() == 0 && pCSWeapon->IsA( WEAPON_C4 ) )
			return true;

		if ( mp_death_drop_gun.GetInt() == 0 && !pCSWeapon->IsA( WEAPON_C4 ) )
			return true;

		// [dwenger] Determine value of dropped item.
		if ( !pCSWeapon->IsAPriorOwner( this ) )
		{
			pCSWeapon->AddPriorOwner( this );
			CCS_GameStats.IncrementStat(this, CSTAT_ITEMS_DROPPED_VALUE, pCSWeapon->GetWeaponPrice() );
		}
		CEconItemView *pItem = pCSWeapon->GetEconItemView();

		if ( pCSWeapon->IsA( WEAPON_HEALTHSHOT ) )
		{
			CItem_Healthshot* pHealth = dynamic_cast< CItem_Healthshot* >( pCSWeapon );
			if ( pHealth )
			{
				pHealth->DropHealthshot();
				ClientPrint( this, HUD_PRINTCENTER, "#SFUI_Notice_YouDroppedWeapon", pCSWeapon->GetPrintName() );
				
			}
			return true;
		}

		CSWeaponType type = pCSWeapon->GetWeaponType();
		switch ( type )
		{
		// Only certail weapons can be dropped when drop is initiated by player
		case WEAPONTYPE_PISTOL:
		case WEAPONTYPE_SUBMACHINEGUN:
		case WEAPONTYPE_RIFLE:
		case WEAPONTYPE_SHOTGUN:
		case WEAPONTYPE_SNIPER_RIFLE:
		case WEAPONTYPE_MACHINEGUN:
		case WEAPONTYPE_C4:
		{
			if (CSGameRules()->GetCanDonateWeapon() && !pCSWeapon->GetDonated() )
			{
				pCSWeapon->SetDonated(true );
				pCSWeapon->SetDonor(this );
			}
			CSWeaponDrop( pCSWeapon, true, true );

			if ( IsAlive() && !bSwapping )
				ClientPrint( this, HUD_PRINTCENTER, "#SFUI_Notice_YouDroppedWeapon", ( pItem ? pItem->GetItemDefinition()->GetItemBaseName() : pCSWeapon->GetPrintName() ) );
		}
		break;

		default:
		{
			// let dedicated servers optionally allow droppable knives
			if ( type == WEAPONTYPE_KNIFE && mp_drop_knife_enable.GetBool( ) )
			{
				if ( CSGameRules( )->GetCanDonateWeapon( ) && !pCSWeapon->GetDonated( ) )
				{
					pCSWeapon->SetDonated( true );
					pCSWeapon->SetDonor( this );
				}
				CSWeaponDrop( pCSWeapon, true, true );

				if ( IsAlive( ) && !bSwapping )
					ClientPrint( this, HUD_PRINTCENTER, "#SFUI_Notice_YouDroppedWeapon", ( pItem ? pItem->GetItemDefinition( )->GetItemBaseName( ) : pCSWeapon->GetPrintName( ) ) );
			}
			else if ( IsAlive( ) && !bSwapping )
			{
				ClientPrint( this, HUD_PRINTCENTER, "#SFUI_Notice_CannotDropWeapon", ( pItem ? pItem->GetItemDefinition( )->GetItemBaseName( ) : pCSWeapon->GetPrintName( ) ) );
			}
		}
		break;
		}

		return true;
	}

	return false;
}

void CCSPlayer::DestroyWeapon( CBaseCombatWeapon *pWeapon )
{
	if ( pWeapon )
	{
		pWeapon->DestroyItem();
	}
}

void CCSPlayer::DestroyWeapons( bool bDropC4 /* = true */ )
{
	// Destroy the Defuser
	if( HasDefuser() && mp_death_drop_defuser.GetBool() )
	{
		RemoveDefuser();
	}

	CBaseCombatWeapon *pWeapon = NULL;

	// Destroy the primary weapon if it exists
	pWeapon = Weapon_GetSlot( WEAPON_SLOT_RIFLE );
	DestroyWeapon( pWeapon );

	// Destroy the secondary weapon if it exists
	pWeapon = Weapon_GetSlot( WEAPON_SLOT_PISTOL );
	DestroyWeapon( pWeapon );

	CBaseCombatWeapon *pC4 = Weapon_OwnsThisType( "weapon_c4" );
	if ( CSGameRules()->IsPlayingCoopMission() )
	{
		if ( pC4 )
			DestroyWeapon( pC4 );
	}

	// Destroy any grenades
	const char* GrenadePriorities[] =
	{
		"weapon_molotov",
		"weapon_incgrenade",
		"weapon_smokegrenade",
		"weapon_hegrenade",
		"weapon_flashbang",
		"weapon_tagrenade",
		"weapon_decoy",
	};

	CBaseCSGrenade *pGrenade = NULL;
	for ( int i = 0; i < ARRAYSIZE(GrenadePriorities ); ++i )
	{
		pGrenade = dynamic_cast< CBaseCSGrenade * >(Weapon_OwnsThisType(GrenadePriorities[i] ) );
		if ( pGrenade && pGrenade->HasAmmo() )
		{
			pGrenade->DestroyItem();
		}
	}

	if ( bDropC4 && pC4 )
	{
		// Drop the C4
		SetBombDroppedTime( gpGlobals->curtime );
		CSWeaponDrop( pC4, false, true );
	}
}

//Drop the appropriate weapons:
// Defuser if we have one
// C4 if we have one
// The best weapon we have, first check primary,
// then secondary and drop the best one

void CCSPlayer::DropWeapons( bool fromDeath, bool killedByEnemy )
{
	CBaseCombatWeapon *pC4 = Weapon_OwnsThisType( "weapon_c4" );
	if ( pC4 )
	{
		if ( mp_death_drop_c4.GetBool() == 1 )
		{
			SetBombDroppedTime( gpGlobals->curtime );
			CSWeaponDrop( pC4, false, true );
			if ( fromDeath )
			{
				if ( killedByEnemy )
				{
					( static_cast< CC4* > ( pC4 ) )->SetDroppedFromDeath( true );
				}
			}
		}
	}

	if( HasDefuser() && mp_death_drop_defuser.GetBool() )
	{

		if ( !CSGameRules()->IsWarmupPeriod() )
		{
			//Drop an item_defuser
			Vector vForward, vRight;
			AngleVectors( GetAbsAngles(), &vForward, &vRight, NULL );

			CBaseAnimating *pDefuser = NULL;
			if ( CSGameRules()->IsHostageRescueMap() )
				pDefuser = (CBaseAnimating * )CBaseEntity::Create( "item_cutters", WorldSpaceCenter(), GetLocalAngles(), NULL );
			else
				pDefuser = (CBaseAnimating * )CBaseEntity::Create( "item_defuser", WorldSpaceCenter(), GetLocalAngles(), NULL );
			pDefuser->ApplyAbsVelocityImpulse( vForward * 200 + vRight * random->RandomFloat( -50, 50 ) );
		}

		RemoveDefuser();
	}

	if ( HasShield() )
	{
		DropShield();
	}

	if ( mp_death_drop_gun.GetInt() != 0 )
	{
		CWeaponCSBase* pWeapon = NULL;

		if ( mp_death_drop_gun.GetInt() == 2 )
		{
			pWeapon = GetActiveCSWeapon();
			if ( pWeapon && !(pWeapon->GetSlot() == WEAPON_SLOT_PISTOL || pWeapon->GetSlot() == WEAPON_SLOT_RIFLE ) )
			{
				pWeapon = NULL;
			}
		}

		if ( pWeapon == NULL )
		{
			//drop the best weapon we have
			if( !DropWeaponSlot( WEAPON_SLOT_RIFLE, true ) )
				DropWeaponSlot( WEAPON_SLOT_PISTOL, true );

		}
	}

	
	// wills: note - this may seem counter-intuitive below,
	// but the player can only play grenade-related animations 
	// (like throwing) while 'holding' the grenade WEAPON. This means
	// it's possible to still be holding the grenade WEAPON even
	// after the actual grenade itself is flying away, so the
	// player's throw anim can smoothly finish. That's why we need
	// to check if the grenade has emitted a projectile; we don't
	// want to drop a duplicate of the thrown grenade if we're killed
	// AFTER the grenade is in flight but BEFORE the throw anim is over.

	bool bGrenadeDropped = false;

	// drop any live grenades so they explode
	CBaseCSGrenade *pGrenade = dynamic_cast< CBaseCSGrenade * >(GetActiveCSWeapon() );

	if ( pGrenade && pGrenade->HasEmittedProjectile() )
		pGrenade = NULL; // the currently active grenade weapon, while active, is NOT eligible to drop because it has thrown a projectile into the world.

	if ( pGrenade )
	{
		if ( pGrenade->IsPinPulled() || pGrenade->IsBeingThrown() ) 
		{
			// NOTE[pmf]: Molotov is excluded from this list. Consider making this a weapon property
			if (
				pGrenade->ClassMatches("weapon_hegrenade" ) || 
				pGrenade->ClassMatches("weapon_flashbang" ) || 
				pGrenade->ClassMatches("weapon_smokegrenade" ) || 
				pGrenade->ClassMatches("weapon_decoy" ) )
			{
				pGrenade->DropGrenade();
				pGrenade->DecrementAmmo( this );
				bGrenadeDropped = true;
			}
		}

		if ( mp_death_drop_grenade.GetInt() == 2 && !bGrenadeDropped )
		{
			// drop currently active grenade
			bGrenadeDropped = CSWeaponDrop(pGrenade, false );
		}
	}

	if ( mp_death_drop_grenade.GetInt() == 3 )
	{
		for ( int i = 0; i < MAX_WEAPONS; ++i )
		{
			CBaseCSGrenade *pCurGrenade = dynamic_cast< CBaseCSGrenade * >( GetWeapon( i ) );
			if ( pCurGrenade && pCurGrenade->HasAmmo() && !pCurGrenade->HasEmittedProjectile() )
			{
				bGrenadeDropped = CSWeaponDrop( pCurGrenade, false );
			}
		}
	}
	else if ( mp_death_drop_grenade.GetInt() != 0 && !bGrenadeDropped )
	{
		// drop the "best" grenade remaining according to the following priorities
		const char* GrenadePriorities[] =
		{
			"weapon_molotov",	// first slot might get overridden by player last held grenade type below
			"weapon_molotov",
			"weapon_incgrenade",
			"weapon_smokegrenade",
			"weapon_hegrenade",
			"weapon_flashbang",
			"weapon_tagrenade",
			"weapon_decoy",
		};

		switch ( m_nPreferredGrenadeDrop )
		{
		case WEAPON_FLASHBANG: GrenadePriorities[0] = "weapon_flashbang"; break;
		case WEAPON_MOLOTOV: GrenadePriorities[0] = "weapon_molotov"; break;
		case WEAPON_INCGRENADE: GrenadePriorities[0] = "weapon_incgrenade"; break;
		case WEAPON_HEGRENADE: GrenadePriorities[0] = "weapon_hegrenade"; break;
		case WEAPON_SMOKEGRENADE: GrenadePriorities[0] = "weapon_smokegrenade"; break;
		case WEAPON_DECOY: GrenadePriorities[0] = "weapon_decoy"; break;
		case WEAPON_TAGRENADE: GrenadePriorities[0] = "weapon_tagrenade"; break;
		}
		m_nPreferredGrenadeDrop = 0; // after we drop a preferred grenade make sure we reset the field

		for ( int i = 0; ( i < ARRAYSIZE(GrenadePriorities ) ) && !bGrenadeDropped; ++i )
		{
			pGrenade = dynamic_cast< CBaseCSGrenade * >(Weapon_OwnsThisType(GrenadePriorities[i] ) );
			if ( pGrenade && pGrenade->HasAmmo() && !pGrenade->HasEmittedProjectile() )
			{
				bGrenadeDropped = CSWeaponDrop(pGrenade, false );
			}
		}
	}
/*
	CBaseCarribleItem *pItem = dynamic_cast< CBaseCarribleItem * >( Weapon_OwnsThisType( "weapon_carriableitem" ) );
	if ( pItem && pItem->HasAmmo() )
	{
		for ( int i = 1; i < pItem->Clip1(); ++i )
		{
			pItem->DropItem();
			pItem->DecrementAmmo( this );
		}

		// drop the remaining item
		CSWeaponDrop( pItem, false );
	}
*/
	if ( m_hCarriedHostage != NULL && GetNumFollowers() > 0 )
	{
		CHostage *pHostage = dynamic_cast< CHostage * >( m_hCarriedHostage.Get() );
		if ( pHostage )
			pHostage->DropHostage( GetAbsOrigin() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Put the player in the specified team
//-----------------------------------------------------------------------------
void CCSPlayer::ChangeTeam( int iTeamNum )
{
	if ( !CSGameRules() )
		return;

	if ( !GetGlobalTeam( iTeamNum ) )
	{
		Warning( "CCSPlayer::ChangeTeam( %d ) - invalid team index.\n", iTeamNum );
		return;
	}

	int iOldTeam = GetTeamNumber();

	// if this is our current team, just abort
	if ( iTeamNum == iOldTeam )
		return;

	/*
	// [wills] Trying to squash T-pose orphaned wearables. Note: it isn't great to remove wearables all over the place, 
	// since it may trigger an unnecessary texture re-composite, which is potentially costly.
	RemoveAllWearables();
	*/

	if ( IsBot() && ( iTeamNum == TEAM_UNASSIGNED || iTeamNum == TEAM_SPECTATOR ) )
	{
		// Destroy weapons since bot is going away
		DestroyWeapons();
	}
	else
	{
		// [tj] Added a parameter so we know if it was death that caused the drop
		// Drop Our best weapon
		DropWeapons(false, false );
	}	

	// [tj] Clear out dominations
	RemoveNemesisRelationships();

	// Always allow a change to spectator, and don't count it as one of our team changes.
	// We now store the old team, so if a player changes once to one team, then to spectator,
	// they won't be able to change back to their old old team, but will still be able to join 
	// the team they initially changed to.
	if( iTeamNum != TEAM_SPECTATOR )
	{	
		// set the play time for the round since we won't be logging it after changing teams
		CCS_GameStats.IncrementStat( this, CSSTAT_PLAYTIME, (int )CSGameRules()->GetRoundElapsedTime(), true );

		m_bTeamChanged = true;
		m_bJustBecameSpectator = false;
	}
	else
	{
		m_iOldTeam = iOldTeam;
		m_bJustBecameSpectator = true;
	}

	// do the team change:
	BaseClass::ChangeTeam( iTeamNum );

	if ( !IsBot() )
	{
		if ( GetTeamNumber() != TEAM_UNASSIGNED )
		{
			m_iLastTeam = GetTeamNumber();
		}
	}

	//reset class
	m_iClass = (int )CS_CLASS_NONE;

	// reset addons updates
	m_iAddonBits = 0;

	// update client state 

	if ( iTeamNum == TEAM_UNASSIGNED )
	{
		// Never let a player sit idle as TEAM_UNASSIGNED. Start the auto team select timer.
		State_Transition( STATE_OBSERVER_MODE );
		ResetForceTeamThink();
		SetContextThink( &CBasePlayer::PlayerForceTeamThink, TICK_NEVER_THINK, CS_FORCE_TEAM_THINK_CONTEXT );
	}
	else if ( iTeamNum == TEAM_SPECTATOR )
	{
		//Reset money
		ResetAccount();

		RemoveAllItems( true );
		
		State_Transition( STATE_OBSERVER_MODE );
	}
	else // active player
	{
		if ( iOldTeam == TEAM_SPECTATOR )
		{
			// If they're switching from being a spectator to ingame player
			// [tj] Changed this so players either retain their existing money or, 
			//		if they have less than the default, give them the default.
			int startMoney = CSGameRules()->GetStartMoney();
			if ( startMoney > GetAccountBalance() )
			{
				InitializeAccount( startMoney );
			}
		}
		
		// bots get to this state on TEAM_UNASSIGNED, yet they are marked alive.  Don't kill them.
		else if ( iOldTeam != TEAM_UNASSIGNED  && !IsDead() )
		{
			// Kill player if switching teams while alive
			CommitSuicide();
		}

		// Put up the class selection menu.
		HandleCommand_JoinClass();
		// State_Transition( STATE_PICKINGCLASS );
	}

	// Initialize the player counts now that a player has switched teams
	int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
	CSGameRules()->InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );

	if ( !IsBot() )
	{
		// Fire a generic team change event to say that someone changed teams.
		// This is caught by the client, who then notifies their matchmaking framework
		// with the proper data to update matchmaking properties based on team distribution.
		IGameEvent *event = gameeventmanager->CreateEvent( "switch_team" );
		if ( event )
		{
			int numPlayers = GetGlobalTeam( TEAM_UNASSIGNED )->GetNumPlayers();
			int numSpectators = GetGlobalTeam( TEAM_SPECTATOR )->GetNumPlayers();
			int numCTs = 0, numTs = 0;

			CTeam *pTeam = GetGlobalTeam( TEAM_TERRORIST );
			for ( int i=0; i<pTeam->GetNumPlayers(); ++i )
			{
				if ( !pTeam->GetPlayer( i )->IsBot() )
				{
					++numPlayers;
					++numTs;
				}
			}
			pTeam = GetGlobalTeam( TEAM_CT );
			for ( int i=0; i<pTeam->GetNumPlayers(); ++i )
			{
				if ( !pTeam->GetPlayer( i )->IsBot() )
				{
					++numPlayers;
					++numCTs;
				}
			}
			
			event->SetInt( "numPlayers", numPlayers );
			event->SetInt( "numSpectators", numSpectators );

			CCSGameRules* pGameRules = CSGameRules();
			event->SetInt( "numTSlotsFree", pGameRules->MaxNumPlayersOnTerrTeam() - numTs );
			event->SetInt( "numCTSlotsFree", pGameRules->MaxNumPlayersOnCTTeam() - numCTs );

			// let the client know the average skill rank
			event->SetInt( "timeout", 0 );

			gameeventmanager->FireEvent( event );
		}
	}

	CSGameRules()->UpdateTeamClanNames( TEAM_TERRORIST ); 
	CSGameRules()->UpdateTeamClanNames( TEAM_CT ); 

	CCSPlayerResource *pResource = dynamic_cast< CCSPlayerResource * >( g_pPlayerResource );
	if ( /*iTeamNum <= TEAM_SPECTATOR && */pResource )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer* pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer == this )
				pResource->ResetPlayerTeammateColor( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Put the player in the specified team without penalty
//-----------------------------------------------------------------------------
void CCSPlayer::SwitchTeam( int iTeamNum )
{
	if ( !GetGlobalTeam( iTeamNum ) || (iTeamNum != TEAM_CT && iTeamNum != TEAM_TERRORIST ) )
	{
		Warning( "CCSPlayer::SwitchTeam( %d ) - invalid team index.\n", iTeamNum );
		return;
	}

	int iOldTeam = GetTeamNumber();

	// if this is our current team, just abort
	if ( iTeamNum == iOldTeam )
		return;

	// [wills] Trying to squash T-pose orphaned wearables. Note: it isn't great to remove wearables all over the place, 
	// since it may trigger an unnecessary texture re-composite, which is potentially costly.
	// RemoveAllWearables();

	// set the play time for the round since we won't be logging it after changing teams
	CCS_GameStats.IncrementStat( this, CSSTAT_PLAYTIME, (int )CSGameRules()->GetRoundElapsedTime(), true );

	// Always allow a change to spectator, and don't count it as one of our team changes.
	// We now store the old team, so if a player changes once to one team, then to spectator,
	// they won't be able to change back to their old old team, but will still be able to join 
	// the team they initially changed to.
	m_bTeamChanged = true;

	// do the team change:
	BaseClass::ChangeTeam( iTeamNum );

	if( HasDefuser() )
	{
		RemoveDefuser();
	}

	m_iClass = PlayerModelInfo::GetPtr()->GetNextClassForTeam( iTeamNum );

	if ( IsControllingBot() )
	{
		// Switch team + controlling bot should reset the model class to the new one
		m_PreControlData.m_iClass = m_iClass;
	}

	// Initialize the player counts now that a player has switched teams
	int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
	CSGameRules()->InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );

	CSGameRules()->UpdateTeamClanNames( TEAM_TERRORIST ); 
	CSGameRules()->UpdateTeamClanNames( TEAM_CT ); 
}

void CCSPlayer::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	// Fix up the model name for rule matching
	char modelName[MAX_PATH];
	V_FileBase( STRING( GetModelName() ), modelName, sizeof( modelName ) );
	char *pEnd = V_stristr( modelName, "_var" );
	if ( pEnd )
		*pEnd = 0;
	
	int myteam = GetTeamNumber();
	int otherTeam = ( myteam == TEAM_CT ) ? TEAM_TERRORIST : TEAM_CT;

	set.AppendCriteria( "team", myteam );
	set.AppendCriteria( "model", modelName );
	set.AppendCriteria( "liveallies", GetTeam()->GetAliveMembers() );
	set.AppendCriteria( "liveenemies", GetGlobalTeam( otherTeam )->GetAliveMembers() );
}

void CCSPlayer::ModifyOrAppendPlayerCriteria( AI_CriteriaSet& set )
{
	// this is for giving player info to the hostage response system
	// and is as yet unused.
	// Eventually we could give the hostage a few tidbits about this player,
	// eg their health, what weapons they have, and the hostage could 
	// comment accordingly.

	//do not append any player data to the Criteria!

	//we don't know which player we should be caring about
}

static uint32 s_BulletGroupCounter = 0;

void CCSPlayer::StartNewBulletGroup()
{
	s_BulletGroupCounter++;
}

uint32 CCSPlayer::GetBulletGroup()
{
	return s_BulletGroupCounter;
}

void CCSPlayer::ResetBulletGroup()
{
	s_BulletGroupCounter = 0;
}

CDamageRecord::CDamageRecord( CCSPlayer * pPlayerDamager, CCSPlayer * pPlayerRecipient, int iDamage, int iCounter, int iActualHealthRemoved )
{
	if ( pPlayerDamager )
	{
		m_PlayerDamager = pPlayerDamager;
		m_PlayerDamagerControlledBot = pPlayerDamager->IsControllingBot() ? pPlayerDamager->GetControlledBot() : NULL;
		Q_strncpy( m_szPlayerDamagerName, pPlayerDamager->GetPlayerName(), sizeof(m_szPlayerDamagerName) );
	}
	else
	{
		Q_strncpy( m_szPlayerDamagerName, "World", sizeof(m_szPlayerDamagerName) );
	}
	
	if ( pPlayerRecipient )
	{
		m_PlayerRecipient = pPlayerRecipient;
		m_PlayerRecipientControlledBot = pPlayerRecipient->IsControllingBot() ? pPlayerRecipient->GetControlledBot() : NULL;
		Q_strncpy( m_szPlayerRecipientName, pPlayerRecipient->GetPlayerName(), sizeof(m_szPlayerRecipientName) );
	}
	else
	{
		Q_strncpy( m_szPlayerRecipientName, "World", sizeof(m_szPlayerRecipientName) );
	}

	m_iDamage = iDamage;
	m_iActualHealthRemoved = iActualHealthRemoved;
	m_iNumHits = 1;
	m_iLastBulletUpdate = iCounter;
}

bool CDamageRecord::IsDamageRecordStillValidForDamagerAndRecipient( CCSPlayer * pPlayerDamager, CCSPlayer * pPlayerRecipient )
{
	if ( ( pPlayerDamager   != m_PlayerDamager )   || 
		 ( pPlayerRecipient != m_PlayerRecipient ) ||
		 ( pPlayerDamager != NULL   && pPlayerDamager->IsControllingBot()   && m_PlayerDamagerControlledBot   != pPlayerDamager->GetControlledBot() ) ||
		 ( pPlayerRecipient != NULL && pPlayerRecipient->IsControllingBot() && m_PlayerRecipientControlledBot != pPlayerRecipient->GetControlledBot() )  )
	{
		return false;
	}

	return true;
}

//=======================================================
// Remember this amount of damage that we dealt for stats
//=======================================================
void CCSPlayer::RecordDamage( CCSPlayer* damageDealer, CCSPlayer* damageTaker, int iDamageDealt, int iActualHealthRemoved )
{
	FOR_EACH_LL( m_DamageList, i )
	{

		if ( m_DamageList[i]->IsDamageRecordStillValidForDamagerAndRecipient( damageDealer, damageTaker ) )
		{
			m_DamageList[i]->AddDamage( iDamageDealt, s_BulletGroupCounter, iActualHealthRemoved );
			return;
		}
	}

	CDamageRecord *record = new CDamageRecord( damageDealer, damageTaker, iDamageDealt, s_BulletGroupCounter, iActualHealthRemoved );
	int k = m_DamageList.AddToTail();
	m_DamageList[k] = record;
}

int CCSPlayer::GetNumAttackersFromDamageList( void )
{
	//Doesn't distinguish friend or enemy, this will return total friendly and enemy attackers
	int nTotalAttackers = 0;
	FOR_EACH_LL( m_DamageList, i )
	{
		if ( m_DamageList[i]->GetPlayerDamagerPtr() && m_DamageList[i]->GetPlayerRecipientPtr() == this )
			nTotalAttackers++;
	}
	return nTotalAttackers;
}

int CCSPlayer::GetMostNumHitsDamageRecordFrom( CCSPlayer *pAttacker )
{
	int iNumHits = 0;
	FOR_EACH_LL( m_DamageList, i )
	{
		if ( m_DamageList[i]->GetPlayerDamagerPtr() == pAttacker )
		{
			if ( m_DamageList[i]->GetNumHits() >= iNumHits )
				iNumHits = m_DamageList[i]->GetNumHits();
		}
	}
	return iNumHits;
}

//=======================================================
// Reset our damage given and taken counters
//=======================================================
void CCSPlayer::ResetDamageCounters()
{
	m_DamageList.PurgeAndDeleteElements();
}

void CCSPlayer::RemoveSelfFromOthersDamageCounters()
{
	// Now clear out any reference of this player in other players' damage lists.
	CUtlVector< CCSPlayer * > playerVector;
	CollectPlayers( &playerVector );

	FOR_EACH_VEC( playerVector, i )
	{
		CCSPlayer *player = playerVector[ i ];

		if ( playerVector[ i ] == this )
			continue;

		FOR_EACH_LL( player->m_DamageList, j )
		{
			if ( player->m_DamageList[j]->GetPlayerDamagerPtr() == this || player->m_DamageList[j]->GetPlayerRecipientPtr() == this )
			{
				delete player->m_DamageList[ j ];
				player->m_DamageList.Remove( j );
				break;
			}
		}
	}
}

//=======================================================
// Output the damage that we dealt to other players
//=======================================================
void CCSPlayer::OutputDamageTaken( void )
{
	if ( sv_damage_print_enable.GetBool() == false )
		return;

	bool bPrintHeader = true;
	CDamageRecord *pRecord;
	char buf[64];
	int msg_dest = HUD_PRINTCONSOLE;

	FOR_EACH_LL( m_DamageList, i )
	{
		if( bPrintHeader )
		{
			ClientPrint( this, msg_dest, "Player: %s1 - Damage Taken\n", GetPlayerName() );
			ClientPrint( this, msg_dest, "-------------------------\n" );
			bPrintHeader = false;
		}
		pRecord = m_DamageList[i];

		if( pRecord && pRecord->GetPlayerRecipientPtr() == this )
		{
			if (pRecord->GetNumHits() == 1 )
			{
				Q_snprintf( buf, sizeof(buf ), "%d in %d hit", pRecord->GetDamage(), pRecord->GetNumHits() );
			}
			else
			{
				Q_snprintf( buf, sizeof(buf ), "%d in %d hits", pRecord->GetDamage(), pRecord->GetNumHits() );
			}
			ClientPrint( this, msg_dest, "Damage Taken from \"%s1\" - %s2\n", pRecord->GetPlayerDamagerName(), buf );
		}		
	}
}

//=======================================================
// Output the damage that we took from other players
//=======================================================
void CCSPlayer::OutputDamageGiven( void )
{
	int nDamageGivenThisRound = 0;
	//CDamageRecord *pDamageList = pAttacker->GetDamageGivenList();
	FOR_EACH_LL( m_DamageList, i )
	{
		if ( m_DamageList[i]->GetPlayerDamagerPtr() && 
			m_DamageList[i]->GetPlayerDamagerPtr() == this &&
			m_DamageList[i]->GetPlayerRecipientPtr() &&
			m_DamageList[i]->GetPlayerRecipientPtr() != this &&
			IsOtherSameTeam( m_DamageList[i]->GetPlayerRecipientPtr()->GetTeamNumber() ) &&
			!IsOtherEnemy( m_DamageList[i]->GetPlayerRecipientPtr() ) )
		{	
			nDamageGivenThisRound += m_DamageList[i]->GetActualHealthRemoved();
		}		
	}

	IncrementTeamDamagePoints( nDamageGivenThisRound );
	m_bTDGaveProtectionWarningThisRound = false;

	if ( sv_damage_print_enable.GetBool() == false )
		return;

	bool bPrintHeader = true;
	CDamageRecord *pRecord;
	char buf[64];
	int msg_dest = HUD_PRINTCONSOLE;

	FOR_EACH_LL( m_DamageList, i )
	{
		if( bPrintHeader )
		{
			ClientPrint( this, msg_dest, "Player: %s1 - Damage Given\n", GetPlayerName() );
			ClientPrint( this, msg_dest, "-------------------------\n" );
			bPrintHeader = false;
		}

		pRecord = m_DamageList[i];

		if( pRecord && pRecord->GetPlayerDamagerPtr() == this )
		{	
			if (pRecord->GetNumHits() == 1 )
			{
				Q_snprintf( buf, sizeof(buf ), "%d in %d hit", pRecord->GetDamage(), pRecord->GetNumHits() );
			}
			else
			{
				Q_snprintf( buf, sizeof(buf ), "%d in %d hits", pRecord->GetDamage(), pRecord->GetNumHits() );
			}
			ClientPrint( this, msg_dest, "Damage Given to \"%s1\" - %s2\n", pRecord->GetPlayerRecipientName(), buf );
		}		
	}
}

void CCSPlayer::SendLastKillerDamageToClient( CCSPlayer *pLastKiller )
{
	int nNumHitsGiven = 0;
	int nDamageGiven = 0;

	int nNumHitsTaken = 0;
	int nDamageTaken = 0;
	if ( sv_damage_print_enable.GetBool() )
	{
		FOR_EACH_LL( m_DamageList, i )
		{
			if( m_DamageList[i]->IsDamageRecordValidPlayerToPlayer() )
			{
				if ( m_DamageList[i]->IsDamageRecordStillValidForDamagerAndRecipient( this, pLastKiller ) )
				{
					nDamageGiven = m_DamageList[i]->GetDamage();
					nNumHitsGiven = m_DamageList[i]->GetNumHits();
				}
				if ( m_DamageList[i]->IsDamageRecordStillValidForDamagerAndRecipient( pLastKiller, this ) )
				{
					nDamageTaken = m_DamageList[i]->GetDamage();
					nNumHitsTaken = m_DamageList[i]->GetNumHits();
				}
			}
		}
	}

	// Send a user message to the local player with the hits/damage data
	//-----------------------------------------------------------------
	CSingleUserRecipientFilter filter( this );	
	filter.MakeReliable();			
	CCSUsrMsg_SendLastKillerDamageToClient msg;
	msg.set_num_hits_given( nNumHitsGiven );
	msg.set_damage_given( nDamageGiven );
	msg.set_num_hits_taken( nNumHitsTaken );
	msg.set_damage_taken( nDamageTaken );
	SendUserMessage( filter, CS_UM_SendLastKillerDamageToClient, msg );
	//-----------------------------------------------------------------
}

void CCSPlayer::CreateViewModel( int index /*=0*/ )
{
	Assert( index >= 0 && index < MAX_VIEWMODELS );

	if ( GetViewModel( index ) )
		return;

	CPredictedViewModel *vm = ( CPredictedViewModel * )CreateEntityByName( "predicted_viewmodel" );
	if ( vm )
	{
		vm->SetAbsOrigin( GetAbsOrigin() );
		vm->SetOwner( this );
		vm->SetIndex( index );
		DispatchSpawn( vm );
		vm->FollowEntity( this, false );
		m_hViewModel.Set( index, vm );
	}
}

bool CCSPlayer::HasC4() const
{
	return ( Weapon_OwnsThisType( "weapon_c4" ) != NULL );
}

int CCSPlayer::GetNextObserverSearchStartPoint( bool bReverse )
{
#if CS_CONTROLLABLE_BOTS_ENABLED
	// Brock H. - TR - 05/05/09
	// If the server is set up to allow controllable bots, 
	// and if we don't already have a target, 
	// then start with the nearest controllable bot.
	if ( cv_bot_controllable.GetBool() )
	{
		if ( !IsValidObserverTarget( m_hObserverTarget.Get() ) )
		{
			if ( CCSBot *pBot = FindNearestControllableBot(true ) )
			{
				return pBot->entindex();
			}
			else if ( m_nLastKillerIndex > 0 )
			{
				return m_nLastKillerIndex;
			}
		}
	}
#endif

	// If we are currently watching someone who is dead, they must have died while we were watching (since
	// a dead guy is not a valid pick to start watching ).  He was given his killer as an observer target
	// when he died, so let's start by trying to observe his killer.  If we fail, we'll use the normal way.
	// And this is just the start point anyway, but we want to start the search here in case it is okay.
	if( m_hObserverTarget && !m_hObserverTarget->IsAlive() )
	{
		CCSPlayer *targetPlayer = ToCSPlayer(m_hObserverTarget );
		if( targetPlayer && targetPlayer->GetObserverTarget() )
			return targetPlayer->GetObserverTarget()->entindex();
	}

	return BaseClass::GetNextObserverSearchStartPoint( bReverse );
}

void CCSPlayer::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
	BaseClass::PlayStepSound( vecOrigin, psurface, fvol, force );

	if ( !sv_footsteps.GetFloat() )
		return;

	if ( !psurface )
		return;

	m_iFootsteps++;
	IGameEvent * event = gameeventmanager->CreateEvent( "player_footstep" );
	if ( event )
	{
		event->SetInt("userid", GetUserID() );
		gameeventmanager->FireEvent( event );
	}

	m_bMadeFootstepNoise = true;
}

void CCSPlayer::ModifyTauntDuration( float flTimingChange )
{
	m_flLookWeaponEndTime -= flTimingChange;
	if ( !m_bUseNewAnimstate )
		m_PlayerAnimState->ModifyTauntDuration( flTimingChange );
}

void CCSPlayer::SelectDeathPose( const CTakeDamageInfo &info )
{
	MDLCACHE_CRITICAL_SECTION();
	if ( !GetModelPtr() )
		return;

	Activity aActivity = ACT_INVALID;
	int iDeathFrame = 0;

	if ( m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	{
		float flDeathYaw = 0;
		m_PlayerAnimStateCSGO->SelectDeathPose( info, m_LastHitGroup, aActivity, flDeathYaw );
		SetDeathPoseYaw( flDeathYaw );
	}
	else
	{
		SelectDeathPoseActivityAndFrame( this, info, m_LastHitGroup, aActivity, iDeathFrame );
	}

	if ( aActivity == ACT_INVALID )
	{
		SetDeathPose( ACT_INVALID );
		SetDeathPoseFrame( 0 );
		return;
	}

	SetDeathPose( SelectWeightedSequence( aActivity ) );
	SetDeathPoseFrame( iDeathFrame );
}


void CCSPlayer::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->Event() == 4001 || pEvent->Event() == 4002 )
	{
		// Ignore these for now - soon we will be playing footstep sounds based on these events
		// that mark footfalls in the anims.
	}
	else if ( pEvent->Event() == AE_WPN_UNHIDE )
	{
		CWeaponCSBase *pWeapon = GetActiveCSWeapon();
		if ( pWeapon && pWeapon->GetWeaponWorldModel() )
		{
			pWeapon->ShowWeaponWorldModel( true );
		}
	}
	else if ( pEvent->Event() == AE_CL_EJECT_MAG || pEvent->Event() == AE_CL_EJECT_MAG_UNHIDE )
	{
		CAnimationLayer *pWeaponLayer = GetAnimOverlay( ANIMATION_LAYER_WEAPON_ACTION );
		if ( pWeaponLayer && pWeaponLayer->m_nDispatchedDst != ACT_INVALID )
		{
			// If the weapon is running a dispatched animation, we can eat these events from the player.
			// The weapon itself assumes the responsibility for these events when dispatched.
		}
		else
		{
			CWeaponCSBase *pWeapon = GetActiveCSWeapon();
			if ( pWeapon && pWeapon->GetWeaponWorldModel() )
			{
				pWeapon->GetWeaponWorldModel()->HandleAnimEvent( pEvent );
			}
		}
	}
	else
	{
		BaseClass::HandleAnimEvent( pEvent );
	}
}


bool CCSPlayer::CanChangeName( void )
{
	if ( IsBot() )
		return true;

	// enforce the minimum interval
	if ( (m_flNameChangeHistory[0] + MIN_NAME_CHANGE_INTERVAL ) >= gpGlobals->curtime )
	{
		return false;
	}

	// enforce that we dont do more than NAME_CHANGE_HISTORY_SIZE 
	// changes within NAME_CHANGE_HISTORY_INTERVAL
	if ( (m_flNameChangeHistory[NAME_CHANGE_HISTORY_SIZE-1] + NAME_CHANGE_HISTORY_INTERVAL ) >= gpGlobals->curtime )
	{
		return false;
	}

	return true;
}

void CCSPlayer::ChangeName( const char *pszNewName )
{
	// make sure name is not too long
	char trimmedName[MAX_PLAYER_NAME_LENGTH];
	Q_strncpy( trimmedName, pszNewName, sizeof( trimmedName ) );

	const char *pszOldName = GetPlayerName();

	if ( IsBot() && CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() )
	{}
	else
	{
		// send colored message to everyone
		CReliableBroadcastRecipientFilter filter;
		UTIL_SayText2Filter( filter, this, kEUtilSayTextMessageType_AllChat, "#Cstrike_Name_Change", pszOldName, trimmedName );

		// broadcast event
		IGameEvent * event = gameeventmanager->CreateEvent( "player_changename" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetString( "oldname", pszOldName );
			event->SetString( "newname", trimmedName );
			gameeventmanager->FireEvent( event );
		}
	}

	// change shared player name
	SetPlayerName( trimmedName );
	if ( m_uiAccountId )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			Q_strncpy( pQMM->m_chPlayerName, this->GetPlayerName(), Q_ARRAYSIZE( pQMM->m_chPlayerName ) );
		}
	}

	// tell engine to use new name
	engine->ClientCommand( edict(), "name \"%s\"", GetPlayerName() );
	
	// remember time of name change
	for ( int i=NAME_CHANGE_HISTORY_SIZE-1; i>0; i-- )
	{
		m_flNameChangeHistory[i] = m_flNameChangeHistory[i-1];
	}

	m_flNameChangeHistory[0] = gpGlobals->curtime; // last change
}

bool CCSPlayer::StartReplayMode( float fDelay, float fDuration, int iEntity )
{
	if ( !BaseClass::StartReplayMode( fDelay, fDuration, iEntity ) )
		return false;

	CSingleUserRecipientFilter filter( this );
	filter.MakeReliable();

	CCSUsrMsg_KillCam msg;

	msg.set_obs_mode( OBS_MODE_IN_EYE );

		if ( m_hObserverTarget.Get() )
		{
		msg.set_first_target( m_hObserverTarget.Get()->entindex() );	// first target
		msg.set_second_target( entindex() );	//second target
		}
		else
		{
		msg.set_first_target( entindex() );	// first target
		msg.set_second_target( 0 );	//second target
		}

	SendUserMessage( filter, CS_UM_KillCam, msg );

	ClientPrint( this, HUD_PRINTCENTER, "Kill Cam Replay" );

	return true;
}

#if HLTV_REPLAY_ENABLED
CON_COMMAND_F( replay_start, "Start GOTV replay: replay_start <delay> [<player name or index>]", FCVAR_CHEAT )
{
	if ( CBasePlayer *pPlayer = UTIL_GetCommandClient() )
	{
		float flDelay = HLTV_MIN_DIRECTOR_DELAY + 1;
		int nPlayerEntIndex = pPlayer->entindex(), nObserverEntIndex = nPlayerEntIndex;
		if ( args.ArgC() > 1 )
		{
			flDelay = V_atof( args.Arg( 1 ) );
			if ( args.ArgC() > 2 )
			{
				const char *pOtherPlayerName = args.Arg( 2 );
				nObserverEntIndex = V_atoi( pOtherPlayerName );
				if ( nObserverEntIndex )
				{
					if ( CBasePlayer *pObserverPlayer = UTIL_PlayerByIndex( nObserverEntIndex ) )
					{
						Msg( "replpaying from %s (%d) pov\n", pObserverPlayer->GetPlayerName(), nObserverEntIndex );
					}
				}
				else
				{
					CBasePlayer *pObserverPlayer = UTIL_PlayerByName( pOtherPlayerName );
					if ( pObserverPlayer )
					{
						nObserverEntIndex = pObserverPlayer->entindex();
						Msg( "replpaying from %s (%d) pov\n", pObserverPlayer->GetPlayerName(), nObserverEntIndex );
					}
				}
			}
		}
		HltvReplayParams_t params;
		params.m_nPrimaryTargetEntIndex = nObserverEntIndex;
		params.m_flDelay = flDelay;
		engine->StartClientHltvReplay( nPlayerEntIndex - 1, params );
	}
}

CON_COMMAND_F( replay_death, "start hltv replay of last death", FCVAR_CHEAT )
{
	CBasePlayer *pBasePlayer = UTIL_GetCommandClient();
	if ( !pBasePlayer )
		return;

	CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
	if ( !pPlayer )
		return;
	ClientReplayEventParams_t params( REPLAY_EVENT_DEATH ) ;
	pPlayer->StartHltvReplayEvent( params );
}

CON_COMMAND_F( replay_stop, "stop hltv replay", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS )
{
	if ( CBasePlayer *pPlayer = UTIL_GetCommandClient() )
	{
		int nPlayerEntIndex = pPlayer->entindex();
		engine->StopClientHltvReplay( nPlayerEntIndex - 1 );
	}
}
#endif

void CCSPlayer::StopReplayMode()
{
	BaseClass::StopReplayMode();

	CSingleUserRecipientFilter filter( this );
	filter.MakeReliable();

	CCSUsrMsg_KillCam msg;

	msg.set_obs_mode( OBS_MODE_NONE );
	msg.set_first_target( 0 );	// first target
	msg.set_second_target( 0 );	//second target
	
	SendUserMessage( filter, CS_UM_KillCam, msg );	
}
	
void CCSPlayer::PlayUseDenySound()
{
	// Don't do a sound here because it can mute your footsteps giving you an advantage.
	// The CS:S content for this sound is silent anyways.
	//EmitSound( "Player.UseDeny" );
}

// [menglish, tj] This is where we reset all the per-round information for achievements for this player
void CCSPlayer::ResetRoundBasedAchievementVariables()
{
	m_KillingSpreeStartTime = -1;

	int numCTPlayers = 0, numTPlayers = 0;
	for (int i = 0; i < g_Teams.Count(); i++ )
	{
		if(g_Teams[i] )
		{
			if ( g_Teams[i]->GetTeamNumber() == TEAM_CT )
				numCTPlayers = g_Teams[i]->GetNumPlayers();
			else if(g_Teams[i]->GetTeamNumber() == TEAM_TERRORIST )
				numTPlayers = g_Teams[i]->GetNumPlayers();
		}
	}
	m_NumEnemiesKilledThisRound = 0;
	m_NumEnemiesKilledThisSpawn = 0;
	m_maxNumEnemiesKillStreak = 0;
	m_bLastKillUsedUniqueWeapon = false;

	m_NumChickensKilledThisSpawn = 0;

	if(GetTeamNumber() == TEAM_CT )
		m_NumEnemiesAtRoundStart = numTPlayers;
	else if(GetTeamNumber() == TEAM_TERRORIST )
		m_NumEnemiesAtRoundStart = numCTPlayers;


	//Clear the previous owner field for currently held weapons
	CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase * >(Weapon_GetSlot( WEAPON_SLOT_RIFLE ) );
	if ( pWeapon )
	{
		pWeapon->SetPreviousOwner(NULL );
	}
	pWeapon = dynamic_cast< CWeaponCSBase * >(Weapon_GetSlot( WEAPON_SLOT_PISTOL ) );
	if ( pWeapon )
	{
		pWeapon->SetPreviousOwner(NULL );
	}

	//Clear list of weapons used to get kills
	m_killWeapons.RemoveAll();

	//Clear sliding window of kill times
	m_killTimes.RemoveAll();

	//clear round kills
	m_enemyPlayersKilledThisRound.RemoveAll();

	m_killsWhileBlind = 0;
	m_bombCarrierkills = 0;
	m_knifeKillBombPlacer = false;

	m_bSurvivedHeadshotDueToHelmet = false;

	m_gooseChaseStep = GC_NONE;
	m_defuseDefenseStep = DD_NONE;
	m_pGooseChaseDistractingPlayer = NULL;

	m_bMadeFootstepNoise = false;
	m_knifeKillsWhenOutOfAmmo = 0;
	m_attemptedBombPlace = false;

	m_bombPickupTime = -1.0f;
	m_bombPlacedTime = -1.0f;
	m_bombDroppedTime = -1.0f;
	m_killedTime = -1.0f;
	m_spawnedTime = -1.0f;
	m_longestLife = -1.0f;
	m_triggerPulled = false;
	m_triggerPulls = 0;

	m_bMadePurchseThisRound = false;

	m_bKilledDefuser = false;
	m_bKilledRescuer = false;
	m_maxGrenadeKills = 0;
	m_grenadeDamageTakenThisRound = 0;
	m_firstShotKills = 0;
	m_hasReloaded = false;

	WieldingKnifeAndKilledByGun(false );
	SetWasKilledThisRound(false );
	m_WeaponTypesUsed.RemoveAll();
	m_WeaponTypesHeld.RemoveAll();
	m_WeaponTypesRunningOutOfAmmo.RemoveAll();
	m_BurnDamageDeltVec.RemoveAll();
	m_bPickedUpDefuser = false;
	m_bPickedUpWeapon = false;
	m_bDefusedWithPickedUpKit = false;	 
	m_bAttemptedDefusal = false;
	m_flDefusedBombWithThisTimeRemaining = 0;
}

void CCSPlayer::HandleEndOfRound()
{
	// store longest life time (for funfacts )
	if( gpGlobals->curtime - m_spawnedTime > m_longestLife )
	{
		m_longestLife = gpGlobals->curtime - m_spawnedTime;
	}

	AllowImmediateDecalPainting();

	RecordRebuyStructLastRound();
}

void CCSPlayer::RecordRebuyStructLastRound( void )
{

	if ( !m_rebuyStruct.isEmpty() )
	{
		m_rebuyStructLastRound = m_rebuyStruct;
		m_rebuyStruct.Clear();
	}
}

void CCSPlayer::SetKilledTime( float time )
{ 
	m_killedTime = time;
	if( m_killedTime - m_spawnedTime > m_longestLife )
	{
		m_longestLife = m_killedTime - m_spawnedTime;
	}
}

/**
 *	static public CCSPlayer::GetCSWeaponIDCausingDamange()
 *
 *		Helper function to get the weapon info for the weapon causing damage.
 *		This is slightly non-trivial because when the damage is from a 
 *		grenade, we don't have a pointer to the original weapon (and in any
 *		case, the object may no longer be around).
 */
const CCSWeaponInfo* CCSPlayer::GetWeaponInfoFromDamageInfo( const CTakeDamageInfo &info )
{
	CWeaponCSBase* pWeapon = dynamic_cast<CWeaponCSBase *>( info.GetWeapon() );
	if ( pWeapon != NULL )
		return &pWeapon->GetCSWpnData();

	// if the inflictor is a grenade, we won't have a weapon in the damageinfo structure, but we can get the weaponinfo directly from the projectile
	CBaseCSGrenadeProjectile* pGrenade = dynamic_cast<CBaseCSGrenadeProjectile *>( info.GetInflictor() );
	if ( pGrenade )
		return pGrenade->m_pWeaponInfo;

	CInferno* pInferno = dynamic_cast<CInferno*>( info.GetInflictor() );
	if ( pInferno )
		return pInferno->GetSourceWeaponInfo();

	return NULL;
}

void CCSPlayer::RestoreWeaponOnC4Abort( void )
{
	if ( !m_lastWeaponBeforeC4AutoSwitch )
	{
		return;
	}
	else
	{
		if ( GetActiveCSWeapon() )
		{
			GetActiveCSWeapon()->SendWeaponAnim( ACT_VM_IDLE );
		}

		Weapon_Switch( m_lastWeaponBeforeC4AutoSwitch );
	}
}

void CCSPlayer::PlayerUsedKnife( void )
{
	// Player immunity in gun game is cleared upon weapon use
	ClearGunGameImmunity();
}

void CCSPlayer::PlayerUsedGrenade( int nWeaponID )
{
	// Player immunity in gun game is cleared upon weapon use
	ClearGunGameImmunity();

	if ( CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		// Clear ownership flag for a grenade used in TR mode
		if ( nWeaponID == WEAPON_FLASHBANG && m_bGunGameTRModeHasFlashbang )
		{
			m_bGunGameTRModeHasFlashbang = false;
		}
		else if ( nWeaponID == WEAPON_HEGRENADE && m_bGunGameTRModeHasHEGrenade )
		{
			m_bGunGameTRModeHasHEGrenade = false;
		}
		else if ( nWeaponID == WEAPON_MOLOTOV && m_bGunGameTRModeHasMolotov )
		{
			m_bGunGameTRModeHasMolotov = false;
		}
		else if ( nWeaponID == WEAPON_INCGRENADE && m_bGunGameTRModeHasIncendiary )
		{
			m_bGunGameTRModeHasIncendiary = false;
		}
	}
}
void CCSPlayer::PlayerUsedFirearm( CBaseCombatWeapon* pBaseWeapon )
{
	// Player immunity in gun game is cleared upon weapon use
	ClearGunGameImmunity();

	if ( pBaseWeapon )
	{
		CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase* >( pBaseWeapon );

		if ( pWeapon )
		{
			CSWeaponType weaponType = pWeapon->GetWeaponType();
			CSWeaponID weaponID = static_cast<CSWeaponID>( pWeapon->GetCSWeaponID() );

			if ( weaponType != WEAPONTYPE_KNIFE && weaponType != WEAPONTYPE_C4 && weaponType != WEAPONTYPE_GRENADE )
			{
				if ( m_WeaponTypesUsed.Find( weaponID ) == -1 )
				{
					// Add this weapon to the list of weapons used by the player
					m_WeaponTypesUsed.AddToTail( weaponID );
				}
			}
		}
	}
}

void CCSPlayer::AddBurnDamageDelt( int entityIndex )
{
	if ( m_BurnDamageDeltVec.Find( entityIndex ) == -1 )
	{
		// Add this index to the list 
		m_BurnDamageDeltVec.AddToTail( entityIndex );
	}

}


int CCSPlayer::GetNumPlayersDamagedWithFire()
{
	return m_BurnDamageDeltVec.Count();
}
void CCSPlayer::PlayerEmptiedAmmoForFirearm( CBaseCombatWeapon* pBaseWeapon )
{
	if ( pBaseWeapon )
	{
		CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase* >( pBaseWeapon );

		if ( pWeapon )
		{
			CSWeaponType weaponType = pWeapon->GetWeaponType();
			CSWeaponID weaponID = static_cast<CSWeaponID>( pWeapon->GetCSWeaponID() );

			if ( weaponType != WEAPONTYPE_KNIFE && weaponType != WEAPONTYPE_C4 && weaponType != WEAPONTYPE_GRENADE )
			{
				if ( m_WeaponTypesRunningOutOfAmmo.Find( weaponID ) == -1 )
				{
					// Add this weapon to the list of weapons used by the player
					m_WeaponTypesRunningOutOfAmmo.AddToTail( weaponID );
				}
			}
		}
	}
}

bool CCSPlayer::DidPlayerEmptyAmmoForWeapon( CBaseCombatWeapon* pBaseWeapon )
{
	if ( pBaseWeapon )
	{
		CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase* >( pBaseWeapon );

		if ( pWeapon )
		{
			CSWeaponType weaponType = pWeapon->GetWeaponType();
			CSWeaponID weaponID = static_cast<CSWeaponID>( pWeapon->GetCSWeaponID() );

			if ( weaponType != WEAPONTYPE_KNIFE && weaponType != WEAPONTYPE_C4 && weaponType != WEAPONTYPE_GRENADE )
			{
				if ( m_WeaponTypesRunningOutOfAmmo.Find( weaponID ) != -1 )
				{
					return true;

				}
			}
		}
	}

	return false;
}

void CCSPlayer::SetWasKilledThisRound(bool wasKilled )
{
	m_wasKilledThisRound = wasKilled; 
	if( wasKilled )
	{
		m_numRoundsSurvived = 0;
	}
}
/**
 *	public CCSPlayer::ProcessPlayerDeathAchievements()
 *
 *		Do Achievement processing whenever a player is killed
 *
 *  Parameters:
 * 		pAttacker -
 * 		pVictim -
 * 		info -
 */
void CCSPlayer::ProcessPlayerDeathAchievements( CCSPlayer *pAttacker, CCSPlayer *pVictim, const CTakeDamageInfo &info )
{
	Assert(pVictim != NULL );
	CBaseEntity *pInflictor = info.GetInflictor();	
	if ( pVictim )
	{
		pVictim->SetWasKilledThisRound(true );
		pVictim->m_lowHealthGoalTime = 0.0f;

	}
	// all these achievements require a valid attacker on a different team
	if ( pAttacker != NULL && pVictim != NULL && pVictim->GetTeamNumber() != pAttacker->GetTeamNumber() )
	{
		// get the weapon used - some of the achievements will need this data
		CWeaponCSBase* pVictimWeapon = dynamic_cast< CWeaponCSBase* >(pVictim->GetActiveWeapon() );
		const CCSWeaponInfo* pAttackerWeaponInfo = GetWeaponInfoFromDamageInfo( info );
		CSWeaponID attackerWeaponId = pAttackerWeaponInfo ? pAttackerWeaponInfo->m_weaponId : WEAPON_NONE;

		bool bIsAttackerEligibleForAchievements = !pAttacker->HasControlledBotThisRound() && 
												  !pAttacker->HasBeenControlledThisRound();

		if (pVictim->m_bIsDefusing )
		{
			pAttacker->AwardAchievement( CSKilledDefuser );			
			pAttacker->m_bKilledDefuser = true;

			if (attackerWeaponId == WEAPON_HEGRENADE && bIsAttackerEligibleForAchievements)
			{
				pAttacker->AwardAchievement( CSKilledDefuserWithGrenade );
			}
		}

		// [pfreese] Achievement check for attacker killing player while reloading
		if (pVictim->IsReloading() && bIsAttackerEligibleForAchievements )
		{
			pAttacker->AwardAchievement( CSKillEnemyReloading );
		}

		// check for first shot kills
		const CWeaponCSBase* pAttackerWeapon = dynamic_cast<const CWeaponCSBase*>( info.GetWeapon() );
		if ( pAttackerWeapon && CSGameRules()->IsPlayingGunGameProgressive() )
		{
			// CSN-8954 - m_iClip1 gets decremented AFTER these achievement checks, so this was previously counting kills with the second bullet. 
			// also restricting to arms race to match the description.
			if ( pAttackerWeapon->UsesClipsForAmmo1() && (pAttackerWeapon->Clip1() /*+ 1*/ == pAttackerWeapon->GetMaxClip1() && bIsAttackerEligibleForAchievements ) )
			{
				pAttacker->IncrementFirstShotKills(1 );
				if ( pAttacker->GetFirstShotKills() >= 3 )
				{
					pAttacker->AwardAchievement( CSFirstBulletKills );
				}
			}
			else
			{ // if we missed one first shot kill, start our tracking over
				pAttacker->ResetFirstShotKills();
			}
		}

		if ( pAttacker->m_bulletsFiredSinceLastSpawn == 1 && CSGameRules()->IsPlayingGunGameProgressive() && bIsAttackerEligibleForAchievements )
		{
			pAttacker->AwardAchievement( CSBornReady );
		}

		if ( pVictim->IsRescuing() )
		{
			// Ensure the killer did not injure any hostages
			if ( !pAttacker->InjuredAHostage() && bIsAttackerEligibleForAchievements )
			{
				pAttacker->AwardAchievement( CSKilledRescuer );
				pAttacker->m_bKilledRescuer = true;
			}
		}

		// [menglish] Achievement check for doing 95% or more damage to a player and having another player kill them
		FOR_EACH_LL( pVictim->m_DamageList, i )
		{
			if ( pVictim->m_DamageList[i]->IsDamageRecordValidPlayerToPlayer() && 
				 pVictim->m_DamageList[i]->GetPlayerRecipientPtr() == pVictim && 
				 pVictim->m_DamageList[i]->GetPlayerDamagerPtr() != pAttacker &&
				 (pVictim->m_DamageList[i]->GetDamage() >= pVictim->GetMaxHealth() - AchievementConsts::DamageNoKill_MaxHealthLeftOnKill ) &&
				 pVictim->IsOtherEnemy( pVictim->m_DamageList[i]->GetPlayerDamagerPtr() ) &&
				 !pVictim->m_DamageList[i]->GetPlayerDamagerPtr()->HasControlledBotThisRound() &&
				 !pVictim->m_DamageList[i]->GetPlayerDamagerPtr()->HasBeenControlledThisRound() )
			{
				pVictim->m_DamageList[i]->GetPlayerDamagerPtr()->AwardAchievement( CSDamageNoKill );
			}
		}

		pAttacker->m_NumEnemiesKilledThisRound++;
		pAttacker->m_NumEnemiesKilledThisSpawn++;
		if ( pAttacker->m_NumEnemiesKilledThisSpawn > pAttacker->m_maxNumEnemiesKillStreak )
			pAttacker->m_maxNumEnemiesKillStreak = pAttacker->m_NumEnemiesKilledThisSpawn;

		//store a list of kill times for spree tracking
		pAttacker->m_killTimes.AddToTail(gpGlobals->curtime );

		//Add the victim to the list of players killed this round
		pAttacker->m_enemyPlayersKilledThisRound.AddToTail( pVictim );

		//Calculate Avenging for all players the victim has killed
		for ( int avengedIndex = 0; avengedIndex < pVictim->m_enemyPlayersKilledThisRound.Count(); avengedIndex++ )        
		{
			CCSPlayer* avengedPlayer = pVictim->m_enemyPlayersKilledThisRound[avengedIndex];

			if (avengedPlayer )
			{
				//Make sure you are avenging someone on your own team (This is the expected flow. Just here to avoid edge cases like team-switching ).
				if ( avengedPlayer->IsOtherSameTeam( pAttacker->GetTeamNumber() ) )
				{
					CCS_GameStats.Event_PlayerAvengedTeammate(pAttacker, pVictim->m_enemyPlayersKilledThisRound[avengedIndex] );
				}
			}
		}



		//remove elements older than a certain time
		while ( pAttacker->m_killTimes.Count() > 0 && pAttacker->m_killTimes[0] + AchievementConsts::KillingSpree_WindowTime < gpGlobals->curtime )
		{
			pAttacker->m_killTimes.Remove(0 );
		}

		//If we killed enough players in the time window, award the achievement
		if ( CSGameRules()->IsPlayingClassic() && pAttacker->m_killTimes.Count() >= AchievementConsts::KillingSpree_Kills && bIsAttackerEligibleForAchievements )
		{
			pAttacker->m_KillingSpreeStartTime = gpGlobals->curtime;
			pAttacker->AwardAchievement( CSKillingSpree );
		}

		// Did the attacker just kill someone on a killing spree?
		if ( pVictim->m_KillingSpreeStartTime >= 0 && pVictim->m_KillingSpreeStartTime - gpGlobals->curtime <= AchievementConsts::KillingSpreeEnder_TimeWindow && bIsAttackerEligibleForAchievements )
		{
			pAttacker->AwardAchievement( CSKillingSpreeEnder );
		}

		// Check the "killed someone with their own weapon" achievement
		if ( pAttackerWeapon && 
			bIsAttackerEligibleForAchievements &&
			pAttackerWeapon->GetPreviousOwner() == pVictim )
		{
			pAttacker->AwardAchievement( CSKillEnemyWithFormerGun );
		}

		// If this player has killed the entire team award him the achievement
		if ( pAttacker->m_NumEnemiesKilledThisRound == pAttacker->m_NumEnemiesAtRoundStart && 
			pAttacker->m_NumEnemiesKilledThisRound >= AchievementConsts::KillEnemyTeam_MinKills &&
			bIsAttackerEligibleForAchievements )
		{
			if ( CSGameRules()->IsPlayingClassic() )
			{
				pAttacker->AwardAchievement( CSKillEnemyTeam );
			}

			if ( CSGameRules()->IsPlayingGunGameTRBomb() && CSGameRules()->m_bBombPlanted == false )
			{
				if ( pAttacker->GetTeamNumber() == TEAM_CT )
				{
					pAttacker->AwardAchievement( CSKillEnemyTerrTeamBeforeBombPlant );
				}
				else
				{
					pAttacker->AwardAchievement( CSKillEnemyCTTeamBeforeBombPlant );
				}
			}
		}

		if ( pVictim->JustLeftSpawnImmunity() && bIsAttackerEligibleForAchievements && CSGameRules()->IsPlayingGunGameProgressive() )
		{
			pAttacker->AwardAchievement( CSSpawnCamper );
		}

		//If this is a posthumous kill award the achievement
		if ( !pAttacker->IsAlive() && attackerWeaponId == WEAPON_HEGRENADE && bIsAttackerEligibleForAchievements )
		{
			CCS_GameStats.IncrementStat(pAttacker, CSSTAT_GRENADE_POSTHUMOUSKILLS, 1 );
			ToCSPlayer(pAttacker )->AwardAchievement( CSPosthumousGrenadeKill );
		}

		if ( pAttacker->GetActiveWeapon() && 
			pAttacker->GetActiveWeapon()->Clip1() == 1 && 
			pAttackerWeapon && 
			pAttackerWeapon->GetWeaponType() != WEAPONTYPE_SNIPER_RIFLE &&
			pAttackerWeapon->GetWeaponType() != WEAPONTYPE_KNIFE &&
			attackerWeaponId != WEAPON_TASER )
		{
			if (pInflictor == pAttacker && bIsAttackerEligibleForAchievements )
			{
				pAttacker->AwardAchievement( CSKillEnemyLastBullet );
				CCS_GameStats.IncrementStat( pAttacker, CSSTAT_KILLS_WITH_LAST_ROUND, 1 );
			}
		}

		// [dwenger] Fun-fact processing
		if ( pVictimWeapon && pVictimWeapon->GetWeaponType() == WEAPONTYPE_KNIFE && !pVictimWeapon->IsA( WEAPON_TASER ) && 
			pInflictor == pAttacker && 
			pAttackerWeapon && 
			!pAttackerWeapon->IsA( WEAPON_KNIFE ) && 
			pAttackerWeapon->GetWeaponType() != WEAPONTYPE_C4 && 
			pAttackerWeapon->GetWeaponType() != WEAPONTYPE_GRENADE &&
			!pVictim->HasControlledBotThisRound() &&
			!pVictim->HasBeenControlledThisRound() )
		{
			// Victim was wielding knife when killed by a gun
			pVictim->WieldingKnifeAndKilledByGun( true );
		}

		//see if this is a unique weapon		
		if ( attackerWeaponId != WEAPON_NONE  )
		{
			if ( pAttacker->m_killWeapons.Find(attackerWeaponId ) == -1 )
			{
				pAttacker->m_bLastKillUsedUniqueWeapon = true;

				pAttacker->m_killWeapons.AddToTail(attackerWeaponId );
				if (pAttacker->m_killWeapons.Count() >= AchievementConsts::KillsWithMultipleGuns_MinWeapons && 
					bIsAttackerEligibleForAchievements && !CSGameRules()->IsPlayingGunGameProgressive() )
				{
					pAttacker->AwardAchievement( CSKillsWithMultipleGuns );					
				}
			}
			else
			{
				pAttacker->m_bLastKillUsedUniqueWeapon = false;
			}
		}

		//Check for kills while blind
		if ( pAttacker->IsBlindForAchievement() )
		{
			//if this is from a different blinding, restart the kill counter and set the time
			if ( pAttacker->m_blindStartTime != pAttacker->m_firstKillBlindStartTime )
			{
				pAttacker->m_killsWhileBlind = 0;
				pAttacker->m_firstKillBlindStartTime = pAttacker->m_blindStartTime;
			}

			++pAttacker->m_killsWhileBlind;
			if ( pAttacker->m_killsWhileBlind >= AchievementConsts::KillEnemiesWhileBlind_Kills && bIsAttackerEligibleForAchievements )
			{
				pAttacker->AwardAchievement( CSKillEnemiesWhileBlind );
			}

			if ( pAttacker->m_killsWhileBlind >= AchievementConsts::KillEnemiesWhileBlindHard_Kills && bIsAttackerEligibleForAchievements )
			{
				pAttacker->AwardAchievement( CSKillEnemiesWhileBlindHard );
			}
		}

		//Check sniper killing achievements
		bool victimZoomed = ( pVictim->GetFOV() != pVictim->GetDefaultFOV() );
		bool attackerZoomed = ( pAttacker->GetFOV() != pAttacker->GetDefaultFOV() );
		bool attackerUsedSniperRifle = pAttackerWeapon && pAttackerWeapon->GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE && pInflictor == pAttacker;
		if ( victimZoomed && attackerUsedSniperRifle && bIsAttackerEligibleForAchievements )
		{
			pAttacker->AwardAchievement( CSKillSniperWithSniper );
		}

		if ( attackerWeaponId == WEAPON_KNIFE && victimZoomed && bIsAttackerEligibleForAchievements )
		{
			pAttacker->AwardAchievement( CSKillSniperWithKnife );
		}
		if ( attackerUsedSniperRifle && !attackerZoomed && bIsAttackerEligibleForAchievements )
		{
			pAttacker->AwardAchievement( CSHipShot );
		}

		//Kill a player at low health
		if ( pAttacker->IsAlive() && pAttacker->GetHealth() <= AchievementConsts::KillWhenAtLowHealth_MaxHealth && bIsAttackerEligibleForAchievements )
		{
			pAttacker->AwardAchievement( CSKillWhenAtLowHealth );
		}
		//Kill a player at medium health
		if ( pAttacker->IsAlive() && pAttacker->GetHealth() <= AchievementConsts::KillWhenAtMediumHealth_MaxHealth && bIsAttackerEligibleForAchievements )
		{
			pAttacker->m_iMediumHealthKills++;
		}		
		//Kill a player with a knife during the pistol round
		if ( CSGameRules()->IsPistolRound() && CSGameRules()->IsPlayingClassic() )
		{
			if ( attackerWeaponId == WEAPON_KNIFE && bIsAttackerEligibleForAchievements )
			{
				pAttacker->AwardAchievement( CSPistolRoundKnifeKill );
			}
		}

		// [tj] Check for dual elites fight
		CWeaponCSBase* victimWeapon = pVictim->GetActiveCSWeapon();

		if ( victimWeapon )
		{
			CSWeaponID victimWeaponID = static_cast<CSWeaponID>( victimWeapon->GetCSWeaponID() );

			if (attackerWeaponId == WEAPON_ELITE && victimWeaponID == WEAPON_ELITE && bIsAttackerEligibleForAchievements )
			{
				pAttacker->AwardAchievement( CSWinDualDuel );
			}
		}

		// [tj] See if the attacker or defender are in the air [sbodenbender] dont include ladders
		bool attackerInAir = pAttacker->GetMoveType() != MOVETYPE_LADDER && pAttacker->GetNearestSurfaceBelow(AchievementConsts::KillInAir_MinimumHeight ) == NULL;
		bool victimInAir = pVictim->GetMoveType() != MOVETYPE_LADDER && pVictim->GetNearestSurfaceBelow(AchievementConsts::KillInAir_MinimumHeight ) == NULL;

		// [dkorus] in air achievements are only allowable when using a gun weapon (including taser)
		if ( pAttackerWeapon && IsGunWeapon(pAttackerWeapon->GetWeaponType()) && bIsAttackerEligibleForAchievements )
		{
			if ( attackerInAir )
			{
				pAttacker->AwardAchievement( CSKillWhileInAir );
			}
			if ( victimInAir )
			{
				pAttacker->AwardAchievement( CSKillEnemyInAir );
			}
			if ( attackerInAir && victimInAir )
			{
				pAttacker->AwardAchievement( CSKillerAndEnemyInAir );
			}
		}

		// [tj] advance to the next stage of the defuse defense achievement
		if (pAttacker->m_defuseDefenseStep == DD_STARTED_DEFUSE )
		{
			pAttacker->m_defuseDefenseStep = DD_KILLED_TERRORIST;
		}

		if (pVictim->HasC4() && pVictim->GetBombPickuptime() + AchievementConsts::KillBombPickup_MaxTime > gpGlobals->curtime && bIsAttackerEligibleForAchievements )
		{
			pAttacker->AwardAchievement( CSKillBombPickup );
		}
		
		// for progressive it's always the first round...  for TR/Demolition mode, we need to check it's the first round
		bool isFirstRoundOfArsenalMode = CSGameRules()->IsPlayingGunGameProgressive() || ( CSGameRules()->IsPlayingGunGameTRBomb() && CSGameRules()->m_iTotalRoundsPlayed == 0 );
		if ( isFirstRoundOfArsenalMode && CSGameRules()->m_bNoTerroristsKilled && CSGameRules()->m_bNoCTsKilled && bIsAttackerEligibleForAchievements )
		{
			// first kill of the match!  Check for achievements
			pAttacker->AwardAchievement( CSGunGameFirstKill );
		}

		// victim may have just dropped C4 or still have it...  increment kills either way
		if ( pVictim->HasC4() || pVictim->GetBombDroppedTime() > 0.0f )
		{
			pAttacker->m_bombCarrierkills++;
		}
	}

	//If you kill a friendly player while blind (from an enemy player ), give the guy that blinded you an achievement    
	if ( pAttacker != NULL && pVictim != NULL && pVictim->IsOtherSameTeam( pAttacker->GetTeamNumber() ) && pAttacker->IsBlind() )
	{
		CCSPlayer* flashbangAttacker = pAttacker->GetLastFlashbangAttacker();
		if (flashbangAttacker && 
			pAttacker->IsOtherEnemy( flashbangAttacker->entindex() ) && 
			!flashbangAttacker->HasControlledBotThisRound() && 
			!flashbangAttacker->HasBeenControlledThisRound() )
		{
			flashbangAttacker->AwardAchievement( CSCauseFriendlyFireWithFlashbang );
		}
	}

	// do a scan to determine count of players still alive
	int livePlayerCount = 0;
	// Zero initialize the arrays.
	int teamCount[TEAM_MAXCOUNT] = {};
	int teamIgnoreCount[TEAM_MAXCOUNT] = {};
	int teamAliveCount[TEAM_MAXCOUNT] = {};

	CCSPlayer *pAlivePlayer = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CCSPlayer* pPlayer = (CCSPlayer* )UTIL_PlayerByIndex( i );
		if (pPlayer )
		{
			int teamNum = pPlayer->GetTeamNumber();
			if ( teamNum >= 0 )
			{
				++teamCount[teamNum];
				if (pPlayer->WasNotKilledNaturally() )
				{
					teamIgnoreCount[teamNum]++;
				}
			}
			if (pPlayer->IsAlive() && pPlayer != pVictim )
			{
				if ( teamNum >= 0 )
				{
					++teamAliveCount[teamNum];
				}
				++livePlayerCount;
				pAlivePlayer = pPlayer;
			}
		}
	}

	// Achievement check for being the last player alive in a match
	if (pAlivePlayer )
	{		
		int alivePlayerTeam = pAlivePlayer->GetTeamNumber();
		int alivePlayerOpposingTeam = alivePlayerTeam == TEAM_CT ? TEAM_TERRORIST : TEAM_CT;
		if (livePlayerCount == 1 
			&& CSGameRules()->m_iRoundWinStatus == WINNER_NONE
			&& teamCount[alivePlayerTeam] - teamIgnoreCount[alivePlayerTeam] >= AchievementConsts::LastPlayerAlive_MinPlayersOnTeam
			&& teamCount[alivePlayerOpposingTeam] - teamIgnoreCount[alivePlayerOpposingTeam] >= AchievementConsts::DefaultMinOpponentsForAchievement
			&& ( !(pAlivePlayer->m_iDisplayHistoryBits & DHF_FRIEND_KILLED ) )
			&& !pAlivePlayer->HasControlledBotThisRound()
			&& !pAlivePlayer->HasBeenControlledThisRound() )
		{
			pAlivePlayer->AwardAchievement( CSLastPlayerAlive);
		}
	}

	// [tj] Added hook into player killed stat that happens before weapon drop
	CCS_GameStats.Event_PlayerKilled_PreWeaponDrop(pVictim, info );
}

//[tj]  traces up to maxTrace units down and returns any standable object it hits
//      (doesn't check slope for standability )
CBaseEntity* CCSPlayer::GetNearestSurfaceBelow(float maxTrace )
{
	trace_t trace;
	Ray_t ray;

	Vector traceStart = this->GetAbsOrigin();
	Vector traceEnd = traceStart;
	traceEnd.z -= maxTrace;

	Vector minExtent = this->m_Local.m_bDucked  ? VEC_DUCK_HULL_MIN : VEC_HULL_MIN;
	Vector maxExtent = this->m_Local.m_bDucked  ? VEC_DUCK_HULL_MAX : VEC_HULL_MAX;

	ray.Init( traceStart, traceEnd, minExtent, maxExtent );
	UTIL_TraceRay( ray, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

	return trace.m_pEnt;
}

// [tj] Added a way to react to the round ending before we reset.
//      It is important to note that this happens before the bomb explodes, so a player may die
//      after this from a bomb explosion or a late kill after a defuse/detonation/rescue.
void CCSPlayer::OnRoundEnd(int winningTeam, int reason )
{
	if ( IsAlive() && !m_bIsControllingBot )
	{
		m_numRoundsSurvived++;
		if ( m_numRoundsSurvived > m_maxNumRoundsSurvived )
		{
			m_maxNumRoundsSurvived = m_numRoundsSurvived;
		}
		
	}

	if ( CSGameRules()->IsPlayingGunGameProgressive() )
	{
		// Send a game event to clear out the round-start sound events
		IGameEvent *event = gameeventmanager->CreateEvent( "gg_reset_round_start_sounds" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			gameeventmanager->FireEvent( event );
		}
	}

	if (winningTeam == WINNER_CT || winningTeam == WINNER_TER )
	{
		int losingTeamId = (winningTeam == TEAM_CT ) ? TEAM_TERRORIST : TEAM_CT;
		
		if ( CSGameRules()->IsPlayingGunGameTRBomb() && !CSGameRules()->IsWarmupPeriod() )
		{
			ConVarRef mp_ggtr_bomb_pts_for_upgrade(	"mp_ggtr_bomb_pts_for_upgrade" );
			if ( m_iNumGunGameTRKillPoints >= mp_ggtr_bomb_pts_for_upgrade.GetInt() || CSGameRules()->GetGunGameTRBonusGrenade( this ) > 0 )
			{
				// Play client sound for impending weapon upgrade...
				SendGunGameWeaponUpgradeAlert();
			}

			if ( m_iNumGunGameTRKillPoints > mp_ggtr_bomb_pts_for_upgrade.GetInt() - 1 )
			{
				// Need to bump the player's TR Bomb Mode weapon to the next level
				m_bShouldProgressGunGameTRBombModeWeapon = true;
			}
		}

		CTeam* losingTeam = GetGlobalTeam(losingTeamId );

		int losingTeamPlayers = 0;

		if (losingTeam )
		{
			losingTeamPlayers = losingTeam->GetNumPlayers();
			
			int ignoreCount = 0;
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CCSPlayer* pPlayer = (CCSPlayer* )UTIL_PlayerByIndex( i );
				if (pPlayer )
				{
					int teamNum = pPlayer->GetTeamNumber();
					if ( teamNum == losingTeamId )
					{					
						if (pPlayer->WasNotKilledNaturally() )
						{
							ignoreCount++;
						}
					}
				}
			}

			losingTeamPlayers -= ignoreCount;
		}

		//Check fast round win achievement
		if (    IsAlive() && 
				gpGlobals->curtime - CSGameRules()->GetRoundStartTime() < AchievementConsts::FastRoundWin_Time &&
				GetTeamNumber() == winningTeam &&
				losingTeamPlayers >= AchievementConsts::DefaultMinOpponentsForAchievement )
		{
			AwardAchievement(CSFastRoundWin );
		}

		//Check goosechase achievement
		if (IsAlive() && reason == Target_Bombed && m_gooseChaseStep == GC_STOPPED_AFTER_GETTING_SHOT && m_pGooseChaseDistractingPlayer )
		{
			m_pGooseChaseDistractingPlayer->AwardAchievement(CSGooseChase );
		}

		//Check Defuse Defense achievement
		if (IsAlive() && reason == Bomb_Defused && m_defuseDefenseStep == DD_KILLED_TERRORIST )
		{
			AwardAchievement(CSDefuseDefense );
		}

		//Check silent win
		if (m_NumEnemiesKilledThisRound > 0 && GetTeamNumber() == winningTeam && !m_bMadeFootstepNoise )
		{
			AwardAchievement(CSSilentWin );
		}

		//Process && Check "win rounds without buying" achievement
		if ( ( GetTeamNumber() == winningTeam ) && !m_bMadePurchseThisRound )
		{
			m_roundsWonWithoutPurchase++;
			if ( ( m_roundsWonWithoutPurchase > AchievementConsts::WinRoundsWithoutBuying_Rounds ) &&
				CSGameRules() && CSGameRules()->IsPlayingClassic() )
			{
				AwardAchievement(CSWinRoundsWithoutBuying );
			}
		}
		else
		{
			m_roundsWonWithoutPurchase = 0;
		}
	}

	// reset cash spent this round
	m_iCashSpentThisRound = 0;

	m_lastRoundResult = reason;
}

void CCSPlayer::SendGunGameWeaponUpgradeAlert( void )
{
	// Send a game event for leveling up
	IGameEvent *event = gameeventmanager->CreateEvent( "gg_player_impending_upgrade" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		gameeventmanager->FireEvent( event );
	}
}

void CCSPlayer::OnPreResetRound()
{
#if 0 // removed this achievement
	//Check headshot survival achievement
	if (IsAlive() && m_bSurvivedHeadshotDueToHelmet && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
	{
		AwardAchievement(CSSurvivedHeadshotDueToHelmet );
	}
#endif

	if (IsAlive() && m_grenadeDamageTakenThisRound > AchievementConsts::SurviveGrenade_MinDamage )
	{
		AwardAchievement(CSSurviveGrenade );
	}

	//Check achievement for surviving attacks from multiple players.
	if (IsAlive() )
	{
		int numberOfEnemyDamagers = GetNumEnemyDamagers();

		if (numberOfEnemyDamagers >= AchievementConsts::SurviveManyAttacks_NumberDamagingPlayers )
		{
			AwardAchievement(CSSurviveManyAttacks );
		}
	}

	if ( IsAlive() && CSGameRules() && CSGameRules()->ShouldRecordMatchStats() )
	{
		int round = CSGameRules()->GetRoundsPlayed() - 1; // last round because roundsplayed has already incremented.

		if ( round >= 0 )
		{
			m_iMatchStats_LiveTime.GetForModify( round ) += CSGameRules()->GetRoundElapsedTime();

			// Keep track in QMM data
			if ( m_uiAccountId && CSGameRules() )
			{
				if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
				{
					pQMM->m_iMatchStats_LiveTime[ round ] = m_iMatchStats_LiveTime.Get( round );
				}
			}
		}
	}

	m_bJustBecameSpectator = false;

	m_isCurrentGunGameLeader = false;
	m_isCurrentGunGameTeamLeader = false;

	if ( m_switchTeamsOnNextRoundReset )
	{
		m_switchTeamsOnNextRoundReset = false;
		bool bSwitched = false;
		int teamNum = GetTeamNumber();
		if ( teamNum == TEAM_TERRORIST )
		{
			
			SwitchTeam( TEAM_CT );
			bSwitched = true;
		}
		else if ( teamNum == TEAM_CT )
		{			
			SwitchTeam( TEAM_TERRORIST );
			bSwitched = true;
		}
		else if ( teamNum == TEAM_SPECTATOR )
		{
			if ( GetCoachingTeam() == TEAM_CT )
			{
				m_iCoachingTeam = TEAM_TERRORIST;
			}
			else if ( GetCoachingTeam() == TEAM_TERRORIST )
			{
				m_iCoachingTeam = TEAM_CT;
			}

			return;	// We're done with spectators.
		}

		// do this for all modes, when you switch sides, you want the second half to start for the opposite team as if it were the first half
// 		if ( CSGameRules()->IsPlayingClassic() )
// 		{
			// Remove all weapons
			RemoveAllItems( true );

			// Reset money
			ResetAccount();
			InitializeAccount( CSGameRules()->GetStartMoney() );

			// For queued matchmaking mode reset money for start
			FOR_EACH_MAP( CSGameRules()->m_mapQueuedMatchmakingPlayersData, idxQueuedPlayer )
			{
				CSGameRules()->m_mapQueuedMatchmakingPlayersData.Element( idxQueuedPlayer )->m_cash = CSGameRules()->GetStartMoney();
			}

			// Make sure player doesn't receive any winnings from the prior round
			MarkAsNotReceivingMoneyNextRound();
/*		}*/

		if ( bSwitched )
		{
			// Send a game event for halftime team swap
			IGameEvent *event = gameeventmanager->CreateEvent( "gg_halftime" );
			if ( event )
			{
				event->SetInt( "userid", GetUserID() );
				gameeventmanager->FireEvent( event );
			}

		}
	}
}
void CCSPlayer::OnCanceledDefuse()
{
	if (m_gooseChaseStep == GC_SHOT_DURING_DEFUSE )
	{
		m_gooseChaseStep = GC_STOPPED_AFTER_GETTING_SHOT;
	}
}


void CCSPlayer::OnStartedDefuse()
{
	m_bAttemptedDefusal = true;

	if (m_defuseDefenseStep == DD_NONE )
	{
		m_defuseDefenseStep = DD_STARTED_DEFUSE;
	}

	if ( !IsBot() && m_flDefusingTalkTimer < gpGlobals->curtime )
	{
		Radio( "DefusingBomb", "#Cstrike_TitlesTXT_Defusing_Bomb" );
		m_flDefusingTalkTimer = gpGlobals->curtime + 6.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayer::AttemptToExitFreezeCam( void )
{
	float fEndFreezeTravel = m_flDeathTime + spec_freeze_deathanim_time.GetFloat() + spec_freeze_traveltime.GetFloat();
	if ( gpGlobals->curtime < fEndFreezeTravel )
		return;

	m_bAbortFreezeCam = true;
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether this player is dominating the specified other player
//-----------------------------------------------------------------------------
void CCSPlayer::SetPlayerDominated( CCSPlayer *pPlayer, bool bDominated )
{
	int iPlayerIndex = pPlayer->entindex();
	m_bPlayerDominated.Set( iPlayerIndex, bDominated );
	pPlayer->SetPlayerDominatingMe( this, bDominated );
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether this player is being dominated by the other player
//-----------------------------------------------------------------------------
void CCSPlayer::SetPlayerDominatingMe( CCSPlayer *pPlayer, bool bDominated )
{
	int iPlayerIndex = pPlayer->entindex();
	m_bPlayerDominatingMe.Set( iPlayerIndex, bDominated );
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether this player is dominating the specified other player
//-----------------------------------------------------------------------------
bool CCSPlayer::IsPlayerDominated( int iPlayerIndex )
{
	if ( CSGameRules()->IsPlayingGunGame() )
		return m_bPlayerDominated.Get( iPlayerIndex );

	return false;
}

bool CCSPlayer::IsPlayerDominatingMe( int iPlayerIndex )
{
	if ( CSGameRules()->IsPlayingGunGame() )
		return m_bPlayerDominatingMe.Get( iPlayerIndex );

	return false;
}


uint32 Helper_AddMusicKitMVPsPendingSend( CEconItemView *pEconItemView, CCSGameRules::CQMMPlayerData_t *pQMM, int nIncBy )
{
	if ( !pEconItemView || !pQMM )
		return 0;

	static CSchemaAttributeDefHandle pAttr_KillEater( "kill eater" );
	uint32 unCurrent = 0;
	if ( !pEconItemView->FindAttribute( pAttr_KillEater, &unCurrent ) )
		return 0;

	CCSGameRules::CQMMPlayerData_t::StattrakMusicKitValues_t::IndexType_t idx = pQMM->m_mapMusicKitUpdates.Find( pEconItemView->GetItemID() );
	if ( idx != CCSGameRules::CQMMPlayerData_t::StattrakMusicKitValues_t::InvalidIndex() )
	{
		pQMM->m_mapMusicKitUpdates[ idx ] += nIncBy;
	}
	else
	{
		idx = pQMM->m_mapMusicKitUpdates.Insert( pEconItemView->GetItemID() );
		pQMM->m_mapMusicKitUpdates[idx] = nIncBy;
	}

	return unCurrent + pQMM->m_mapMusicKitUpdates[idx];
}

extern ConVar sv_matchend_drops_enabled;

void CCSPlayer::IncrementNumMVPs( CSMvpReason_t mvpReason )
{
	// [Forrest] Allow MVP to be turned off for a server
	if ( sv_nomvp.GetBool() )
	{
		Msg( "Round MVP disabled: sv_nomvp is set.\n" );
		return;
	}

	m_iMVPs++;
	
	// Keep track in QMM data
	CCSGameRules::CQMMPlayerData_t *pQMM = NULL;
	if ( m_uiAccountId && CSGameRules() )
	{
		pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId );
		if ( pQMM )
			pQMM->m_numMVPs = m_iMVPs;
	}

	// Set the global MVP in CSGameRules
	if ( CSGameRules() )
	{
		CCSPlayer *pOtherMvpCheck = CSGameRules()->m_pMVP.Get();
		if ( pOtherMvpCheck && ( pOtherMvpCheck != this ) )
			Warning( "CCSPlayer: MVP override ( %s -> %s )\n", pOtherMvpCheck->GetPlayerName(), this->GetPlayerName() );
		CSGameRules()->m_pMVP = this;
	}

	CCS_GameStats.Event_MVPEarned( this );
	IGameEvent *mvpEvent = gameeventmanager->CreateEvent( "round_mvp" );

	CSteamID OwnerSteamID;
	uint32 unMusicKitMVPs = 0; // this will remain zero if we fail to send the stat increment to GC

	if ( mvpEvent )
	{
		mvpEvent->SetInt( "userid", GetUserID() );
		mvpEvent->SetInt( "reason", mvpReason );
		if ( unMusicKitMVPs > 0  )
			mvpEvent->SetInt( "musickitmvps", (int)unMusicKitMVPs );
		gameeventmanager->FireEvent( mvpEvent );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the number of rounds this player has caused to be won for their team
//-----------------------------------------------------------------------------
void CCSPlayer::SetNumMVPs( int iNumMVP )
{
	m_iMVPs = iNumMVP;

	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			pQMM->m_numMVPs = m_iMVPs;
		}
	}
}
//-----------------------------------------------------------------------------
// Purpose: Returns the number of rounds this player has caused to be won for their team
//-----------------------------------------------------------------------------
int CCSPlayer::GetNumMVPs()
{
	return m_iMVPs;
}
 
//-----------------------------------------------------------------------------
// Purpose: Removes all nemesis relationships between this player and others
//-----------------------------------------------------------------------------
void CCSPlayer::RemoveNemesisRelationships()
{
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CCSPlayer *pTemp = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( pTemp && pTemp != this )
		{        
			// set this player to be not dominating anyone else
			SetPlayerDominated( pTemp, false );

			// set no one else to be dominating this player		
			pTemp->SetPlayerDominated( this, false );
		}
	}	
}

void CCSPlayer::CheckMaxGrenadeKills(int grenadeKills )
{
	if (grenadeKills > m_maxGrenadeKills )
	{
		m_maxGrenadeKills = grenadeKills;
	}
}

void CCSPlayer::CommitSuicide( bool bExplode /*= false*/, bool bForce /*= false*/ )
{
	m_wasNotKilledNaturally = true;
	BaseClass::CommitSuicide(bExplode, bForce );
}

void CCSPlayer::CommitSuicide( const Vector &vecForce, bool bExplode /*= false*/, bool bForce /*= false*/ )
{
	m_wasNotKilledNaturally = true;
	BaseClass::CommitSuicide(vecForce, bExplode, bForce );
}

void CCSPlayer::DecrementProgressiveWeaponFromSuicide( void )
{
	if ( CSGameRules()->IsPlayingGunGameProgressive() )
	{
		if ( ( m_LastDamageType & DMG_FALL ) || m_wasNotKilledNaturally ) // Did we die from falling or changing teams?
		{
			// Reset kill count with respect to new weapon
			m_iNumGunGameKillsWithCurrentWeapon = 0;

			// Suicided, so drop one weapon class
			SubtractProgressiveWeaponIndex();

			CRecipientFilter filter;
			filter.AddRecipient( this );
			filter.MakeReliable();
			UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#Cstrike_TitlesTXT_Hint_lost_a_level_generic" );
		}
	}
}

int CCSPlayer::GetNumEnemyDamagers()
{
	int numberOfEnemyDamagers = 0;
	FOR_EACH_LL( m_DamageList, i )
	{
		if ( m_DamageList[i]->IsDamageRecordValidPlayerToPlayer() && 
			 m_DamageList[i]->GetPlayerRecipientPtr() == this &&
			 IsOtherEnemy( m_DamageList[i]->GetPlayerDamagerPtr() ) )
		{
			numberOfEnemyDamagers++;
		}
	}
	return numberOfEnemyDamagers;
}


int CCSPlayer::GetNumEnemiesDamaged()
{
	int numberOfEnemiesDamaged = 0;
	FOR_EACH_LL( m_DamageList, i )
	{
		if ( m_DamageList[i]->IsDamageRecordValidPlayerToPlayer() && 
			m_DamageList[i]->GetPlayerDamagerPtr() == this &&
			IsOtherEnemy( m_DamageList[i]->GetPlayerRecipientPtr() ) )
		{
			numberOfEnemiesDamaged++;
		}
	}
	return numberOfEnemiesDamaged;
}

int CCSPlayer::GetTotalActualHealthRemovedFromEnemies()
{
	int totalDamage = 0;
	FOR_EACH_LL( m_DamageList, i )
	{
		if ( m_DamageList[i]->IsDamageRecordValidPlayerToPlayer() && 
			m_DamageList[i]->GetPlayerDamagerPtr() == this &&
			IsOtherEnemy( m_DamageList[i]->GetPlayerRecipientPtr() ) )
		{
			totalDamage += m_DamageList[i]->GetActualHealthRemoved();
		}
	}
	return totalDamage;
}

bool CCSPlayer::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		unsigned int myTeamMask = ( PhysicsSolidMaskForEntity() & ( CONTENTS_TEAM1 | CONTENTS_TEAM2 ) );
		unsigned int otherTeamMask = ( contentsMask & ( CONTENTS_TEAM1 | CONTENTS_TEAM2 ) );
		
		// See if we have a team and we're on the same team.
		// If we are on the same team, then don't collide.
		if ( myTeamMask != 0x0 && myTeamMask == otherTeamMask  )
		{
			return false;
		}
	}

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

void CCSPlayer::GiveHealthAndArmorForGuardianMode( bool bAdditive )
{
	if ( !CSGameRules() )
		return;

	// we only do it every other, starting round 4
	int nRoundsPlayed = CSGameRules()->GetTotalRoundsPlayed();
	nRoundsPlayed = nRoundsPlayed - ( nRoundsPlayed % 2 );

	if ( CSGameRules()->IsPlayingCoopMission() )
	{
		m_iHealth = 100;
		m_bHasHelmet = true;
		SetArmorValue( 100 );
		m_bHasHeavyArmor = true;
		return;
	}
	else
	{
		int nStartHealth = bAdditive ? m_iHealth : 0;
		int nHealthToGive = 50;
		if ( nRoundsPlayed > 3 )
		{
			nHealthToGive = MIN( GetMaxHealth(), 50 + ( ( nRoundsPlayed - 3 ) * 15 ) );
		}

		m_iHealth = MIN( nStartHealth + nHealthToGive, GetMaxHealth() );
	}

	// armor?
	int nStartArmor = bAdditive ? ArmorValue() : 0;
	int nArmorToGive = 50;
	if ( CSGameRules()->IsPlayingCoopGuardian() )
	{
		if ( nRoundsPlayed > 3 )
		{
			nArmorToGive = MIN( 100, 20 + ( ( nRoundsPlayed - 3 ) * 20 ) );
		}
	}

	int nArmor = MIN( nStartArmor + nArmorToGive, 100 );
	SetArmorValue( nArmor );
}

void CCSPlayer::SetHealth( int amt )
{
	m_iHealth = amt;
}

//--------------------------------------------------------------------------------------------------------
/**
* player used healthshot, make them heal
*/
void CCSPlayer::OnHealthshotUsed( void )
{
	EmitSound( "Healthshot.Success" );
}

//--------------------------------------------------------------------------------------------------------
bool CCSPlayer::UpdateTeamLeaderPlaySound( int nTeam )
{
	if ( CSGameRules()->IsPlayingGunGameProgressive() == false )
		return false;

	bool bPlayedSound = false;

	//GetGGLeader( int nTeam )
	int nOtherTeam = (nTeam == TEAM_CT) ? TEAM_TERRORIST : TEAM_CT;
	int nLeaderUserID = -1;
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || pPlayer->GetTeamNumber() != nTeam )
			continue;

		if ( pPlayer->entindex() == GetGlobalTeam( pPlayer->GetTeamNumber() )->GetGGLeader( pPlayer->GetTeamNumber() ) )
		{
			//if we made it this far, we are the current leader
			IGameEvent *event = gameeventmanager->CreateEvent( "gg_team_leader" );
			if ( event )
			{
				event->SetInt( "playerid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}

			nLeaderUserID = pPlayer->GetUserID();

			break;
		}
	}

	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || pPlayer->GetTeamNumber() != nTeam )
			continue;

		if ( nLeaderUserID == pPlayer->GetUserID() )
		{
			bool bIsMatchLeader = false;
			CCSPlayer *pOtherLeader = ToCSPlayer( UTIL_PlayerByIndex( GetGlobalTeam( nOtherTeam )->GetGGLeader( nOtherTeam ) ) );
			int nOtherIndex = pOtherLeader ? pOtherLeader->m_iGunGameProgressiveWeaponIndex : -1;
			// if our GG index is higher than the other team's leader, we are the match leader
			if ( pPlayer->m_iGunGameProgressiveWeaponIndex > nOtherIndex )
			{
				// check if we aren't already the match leader and if this is ane
				if ( pPlayer->m_isCurrentGunGameLeader == false )
				{
					ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_Gun_Game_Leader" );
					CSingleUserRecipientFilter filter( pPlayer );
					EmitSound( filter, pPlayer->entindex(), "UI.ArmsRace.BecomeMatchLeader" );
					bPlayedSound = true;
				}

				// they are the leader
				pPlayer->m_isCurrentGunGameLeader = true;
			
				// now kick everyone else off
				int nLeaderIndex = pPlayer->entindex();
				for ( int j = 1; j <= MAX_PLAYERS; j++ )
				{
					CCSPlayer *pNonLeader = ToCSPlayer( UTIL_PlayerByIndex( j ) );
					if ( !pNonLeader )
						continue;

					if ( pNonLeader->entindex() != nLeaderIndex )
						pNonLeader->m_isCurrentGunGameLeader = false;
				}
			}

			// if we aren't the match leader and we just became the team lader, send them a message, hurrah!
			if ( !bIsMatchLeader && pPlayer->m_isCurrentGunGameTeamLeader == false )
			{
				ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_Gun_Game_Team_Leader" );
				CSingleUserRecipientFilter filter( pPlayer );
				EmitSound( filter, pPlayer->entindex(), "UI.ArmsRace.BecomeTeamLeader" );
				bPlayedSound = true;
			}

			pPlayer->m_isCurrentGunGameTeamLeader = true;
		}
		else
		{
			if ( pPlayer->m_isCurrentGunGameTeamLeader == true && nLeaderUserID != pPlayer->GetUserID() )
			{
				if ( CCSPlayer *pNewLeader = ( ( nLeaderUserID != -1 )
					? ToCSPlayer( UTIL_PlayerByUserId( nLeaderUserID ) )
					: NULL ) )
				{	// If we cannot resolve the name of the new leader then print no message, but still play the sound
					ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_Stolen_Leader", CFmtStr( "#ENTNAME[%d]%s", pNewLeader->entindex(), pNewLeader->GetPlayerName() ) );
				}

				CSingleUserRecipientFilter filter( pPlayer );
				EmitSound( filter, pPlayer->entindex(), "UI.ArmsRace.Demoted" );
				bPlayedSound = true;
			}

			pPlayer->m_isCurrentGunGameTeamLeader = false;
		}
	}

	return bPlayedSound;
}

void CCSPlayer::UpdateLeader( void )
{
	// this code isn't used anymore
/*
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer && pPlayer != this && GetPlayerGunGameWeaponIndex() <= pPlayer->GetPlayerGunGameWeaponIndex() ) 
		{
			return;
		}
	}

	//if we made it this far, we are the current leader
	IGameEvent *event = gameeventmanager->CreateEvent( "gg_leader" );
	if ( event )
	{
		event->SetInt( "playerid", GetUserID() );
		gameeventmanager->FireEvent( event );
	}
	*/
}

void CCSPlayer::ResetTRBombModeData( void )
{
	m_iGunGameProgressiveWeaponIndex = 0;
	m_iNumGunGameKillsWithCurrentWeapon = 0;
	m_iNumGunGameTRKillPoints = 0;
	m_iNumGunGameTRBombTotalPoints = 0;
	m_bShouldProgressGunGameTRBombModeWeapon = false;
	m_bMadeFinalGunGameProgressiveKill = false;
}


#if CS_CONTROLLABLE_BOTS_ENABLED

void CCSPlayer::SavePreControlData()
{
	m_PreControlData.m_iClass	= GetClass();	
	m_PreControlData.m_iAccount	= m_iAccount;	
	m_PreControlData.m_iAccountMoneyEarnedForNextRound = m_iAccountMoneyEarnedForNextRound;
	m_PreControlData.m_iFrags	= FragCount();
	m_PreControlData.m_iAssists	= AssistsCount();
	m_PreControlData.m_iDeaths	= DeathCount();

	if ( dev_reportmoneychanges.GetBool() )
		Msg( "**** %s is Caching Pre-Bot Control Data (total: %d)	\n", GetPlayerName(), m_PreControlData.m_iAccount );
}

bool CCSPlayer::CanControlBot( CCSBot *pBot, bool bSkipTeamCheck )
{
	if ( !cv_bot_controllable.GetBool() )
		return false;

	if ( !pBot )
		return false;

	if ( !pBot->IsAlive() )
		return false;

	if ( !bSkipTeamCheck && IsOtherEnemy( pBot->entindex() ) )
		return false;

	if ( !bSkipTeamCheck && !IsValidObserverTarget(pBot ) )
		return false;

	if ( pBot->HasControlledByPlayer() )
		return false;

	if ( pBot->IsDefusingBomb() )
		return false;

	// Can't control a bot that is setting a bomb
	const CC4 *pC4 = dynamic_cast<CC4*>( pBot->GetActiveWeapon() );
	if ( pC4 && pC4->m_bStartedArming )
		return false;

	if ( CSGameRules()->IsRoundOver() ) 
		return false;

	if ( CSGameRules()->IsFreezePeriod() )
		return false;

	if ( CSGameRules()->IsWarmupPeriod() )
		return false;

	if ( !bSkipTeamCheck && IsAlive() )
		return false;

	return true;
}

bool CCSPlayer::TakeControlOfBot( CCSBot *pBot, bool bSkipTeamCheck )
{
	if ( !CanControlBot(pBot, bSkipTeamCheck ) )
		return false;

	// First Save off our pre-control settings
	SavePreControlData();
	
	// don't save for bot, the account of the player who is taking over will transfer back 
	//pBot->SavePreControlData();

	// Save off stuff we want from the bot
	// Position / Orientation
	// Appearance
	// Health, Armor, Stamina
	const Vector vecBotPosition = pBot->GetAbsOrigin();
	QAngle vecBotAngles = pBot->GetAbsAngles();
	const int nBotClass = pBot->GetClass();
	const int nBotHealth = pBot->GetHealth();
	const float flBotStamina = pBot->m_flStamina;
	const float flBotVelocityModifier = pBot->m_flVelocityModifier;
	const bool bBotDucked = pBot->m_Local.m_bDucked;
	const bool bBotDucking = pBot->m_Local.m_bDucking;
	const bool bBotFL_DUCKING = (pBot->GetFlags() & FL_DUCKING ) != 0;
	const bool bBotFL_ANIMDUCKING = ( pBot->GetFlags() & FL_ANIMDUCKING ) != 0;
	const float flBotDuckAmount = pBot->m_flDuckAmount;
	const MoveType_t eBotMoveType = pBot->GetMoveType();



	CWeaponCSBase * pBotWeapon = pBot->GetActiveCSWeapon();
	CBaseViewModel * pBotVM = pBot->GetViewModel();
	
	const float flBotNextAttack = pBot->GetNextAttack();
	const float flBotWeaponNextPrimaryAttack = pBotWeapon ? pBotWeapon->m_flNextPrimaryAttack : gpGlobals->curtime;
	const float flBotWeaponNextSecondaryAttack = pBotWeapon ? pBotWeapon->m_flNextSecondaryAttack : gpGlobals->curtime;
	const float flBotWeaponTimeWeaponIdle = pBotWeapon ?  pBotWeapon->m_flTimeWeaponIdle : gpGlobals->curtime;
	const bool bBotWeaponInReload = pBotWeapon ? pBotWeapon->m_bInReload : false;
	//char szBotAnimExtension[32]; pBot->m_szAnimExtension;
	//pBotWeapon->m_IdealActivity;
	//pBotWeapon->m_nIdealSequence;
	const Activity eBotWeaponActivity = pBotWeapon ? pBotWeapon->GetActivity() : ACT_IDLE;
	//pBotWeapon->m_nSequence;
	const float flBotWeaponCycle = pBotWeapon ? pBotWeapon->GetCycle() : 0.0f;
	

	const float flBotVMCycle = pBotVM ? pBotVM->GetCycle() : 0.0f;
	char szBotWeaponClassname[64];
	szBotWeaponClassname[0] = 0;
	if ( pBotWeapon )
	{
		V_strncpy( szBotWeaponClassname, pBotWeapon->GetClassname(), sizeof(szBotWeaponClassname ) );
	}
	//const Activity eBotActivity = GetActivity();
	//pBotWeapon->m_flAccuracy;
	//pBot->m_iShotsFired;
	//pBotWeapon->m_bDelayFire;
	
	if ( bSkipTeamCheck && pBot->GetTeamNumber() != GetTeamNumber() )
	{
		// player needs to switch teams before controlling this bot
		SwitchTeam( pBot->GetTeamNumber() );
	}
	
	if ( bSkipTeamCheck && HasControlledBot() )
	{
		CCSBot *pOldBot = ToCSBot( GetControlledBot() );
		pBot->SwitchTeam( GetTeamNumber() );
		ReleaseControlOfBot();
		pOldBot->Spawn();
		pOldBot->Teleport( &GetAbsOrigin(), &GetAbsAngles(), &vec3_origin );
		pOldBot->State_Transition( STATE_ACTIVE );

	}

	// Next set the control EHANDLEs
	SetControlledBot( pBot );
	pBot->SetControlledByPlayer( this );
	m_bIsControllingBot = true;
	m_iControlledBotEntIndex = pBot->entindex();

	// [wills] Trying to squash T-pose orphaned wearables. Note: it isn't great to remove wearables all over the place, 
	// since it may trigger an unnecessary texture re-composite, which is potentially costly.
	// RemoveAllWearables();

	// If we have a ragdoll, cut it loose now
	if ( CCSRagdoll *pRagdoll = dynamic_cast< CCSRagdoll* >( m_hRagdoll.Get() ) )
	{
		pRagdoll->m_hPlayer = NULL;
		m_hRagdoll = NULL;
	}



	// Now copy over various things from the bot
	m_iClass = nBotClass;

	
	// Make the bot dormant, so he no longer thinks, transmits, or simulates
	pBot->MakeDormant();
	pBot->State_Transition( STATE_DORMANT );
	pBot->m_iHealth = 0;
	pBot->m_lifeState = LIFE_DEAD;
	pBot->RemoveAllWearables();
	pBot->m_flVelocityModifier = 0.0f;

	m_flVelocityModifier = flBotVelocityModifier;		// GET FROM BOT?!?!?

	// Finally, run some normal spawn logic 
	// Here, I'm trying to copy what happens when we call CCSPlayer::Spawn, in some places 
	// using values from the bot rather than init values

	StopObserverMode();
	State_Transition( STATE_ACTIVE );

	bool hasChangedTeamTemp = m_bTeamChanged;
	int numBotsControlled = m_botsControlled;

	// HACK: Bots sometimes have some roll applied when the player takes them over due to acceleration lean
	// which gets stuck on when the player takes them over. Easiest just to clear the roll on the bot when taking over
	vecBotAngles.z = 0;

	SetCSSpawnLocation( vecBotPosition, vecBotAngles );
	Spawn();
	m_bHasControlledBotThisRound = true;
	pBot->m_bHasBeenControlledByPlayerThisRound = true;

	m_fImmuneToGunGameDamageTime = 0;
	m_bGunGameImmunity = false;

	m_bTeamChanged = hasChangedTeamTemp; // dkorus: we want m_bTeamChanged to persist past the Spawn() call.  This is how we acomplish this
	m_botsControlled = numBotsControlled;

	m_flStamina = flBotStamina;		// FROM BOT
	State_Transition( m_iPlayerState );
	pBot->TransferInventory( this );

	m_iHealth = nBotHealth;
	m_lifeState = LIFE_ALIVE;

	m_nNumFastDucks = 0;

	m_bDuckOverride = false;

	// afk check disabled for players whose first action is taking over a bot
	m_bHasMovedSinceSpawn = true;

	SetMoveType( eBotMoveType );
	m_Local.m_bDucked = bBotDucked;
	m_Local.m_bDucking = bBotDucking;
	if ( bBotFL_DUCKING )
		AddFlag( FL_DUCKING );
	else
		RemoveFlag( FL_DUCKING );
	if ( bBotFL_ANIMDUCKING )
		AddFlag( FL_ANIMDUCKING );
	else
		RemoveFlag( FL_ANIMDUCKING );
	m_flDuckAmount = flBotDuckAmount;

	pBot->DispatchUpdateTransmitState();
	DispatchUpdateTransmitState();

	CBaseCombatWeapon* pWeapon = pBotWeapon ? CSWeapon_OwnsThisType( pBotWeapon->GetEconItemView() ) : NULL;

	if ( pWeapon )
	{
		Weapon_Switch( pWeapon );

		pWeapon->SendWeaponAnim( eBotWeaponActivity );
		pWeapon->SetCycle( flBotWeaponCycle );
		pWeapon->m_flTimeWeaponIdle = flBotWeaponTimeWeaponIdle;
		pWeapon->m_flNextPrimaryAttack = flBotWeaponNextPrimaryAttack;
		pWeapon->m_flNextSecondaryAttack = flBotWeaponNextSecondaryAttack;
		pWeapon->m_bInReload = bBotWeaponInReload;

		if ( CBaseViewModel * pVM = GetViewModel() )
		{
			pVM->SetCycle( flBotVMCycle );
		}


		SetNextAttack( flBotNextAttack );
	}

	if ( pBot->IsRescuing() )
	{
		// Tell the hostages controlled by the bot that they should now follow this player
		for ( int iHostage=0; iHostage < g_Hostages.Count(); iHostage++ )
		{
			CHostage *pHostage = g_Hostages[iHostage];

			if ( pHostage && pHostage->GetLeader() == pBot )
			{
				pHostage->Follow( this );
			}
		}

		if ( HOSTAGE_RULE_CAN_PICKUP && pBot->m_hCarriedHostageProp != NULL )
		{
			// transfer any carried hostages and refresh the viewmodel
			CHostageCarriableProp *pHostageProp = static_cast< CHostageCarriableProp* >( pBot->m_hCarriedHostageProp.Get() );
			if ( pHostageProp )
			{
				pBot->m_hCarriedHostageProp = NULL;
				pHostageProp->SetAbsOrigin( GetAbsOrigin() );
				pHostageProp->SetParent( this );
				pHostageProp->SetOwnerEntity( this );
				pHostageProp->FollowEntity( this );
				m_hCarriedHostageProp = pHostageProp;
			}
		
			CBaseViewModel *vm = pBot->GetViewModel( 1 );
			UTIL_Remove( vm );
			pBot->m_hViewModel.Set( 1, INVALID_EHANDLE );
		}
	}

	RefreshCarriedHostage( true );

	m_botsControlled++;

	IGameEvent * event = gameeventmanager->CreateEvent( "bot_takeover" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		event->SetInt( "botid", pBot->GetUserID() );
		event->SetInt( "index", GetClientIndex() );

		gameeventmanager->FireEvent( event );
	}

	return true;
}

void CCSPlayer::ReleaseControlOfBot()
{
	if( m_bIsControllingBot == false )
		return;

	CCSBot *pBot = ToCSBot( m_hControlledBot.Get() );


	if ( pBot )
	{
		pBot->SetControlledByPlayer( NULL );

		TransferInventory( pBot );
		Msg( "    %s RELEASED CONTROL of %s\n", GetPlayerName(), pBot->GetPlayerName() );

		pBot->RemoveEFlags( EFL_DORMANT );
	}
	else
	{
		// dkorus: make sure we clear out any items the player has and reset states.  This makes sure he doesn't keep the bot's items into the next round
		SetArmorValue( 0 );
		m_bHasHelmet = false;
		m_bHasHeavyArmor = false;
		m_bHasNightVision = false;
		m_bNightVisionOn = false;

		RemoveAllItems( true );
	}
	m_iClass = m_PreControlData.m_iClass;
	m_iAccount = m_PreControlData.m_iAccount;
	m_iAccountMoneyEarnedForNextRound = m_PreControlData.m_iAccountMoneyEarnedForNextRound;

	if ( dev_reportmoneychanges.GetBool() )
		Msg( "**** %s			(total: %d)	Restoring m_PreControlData from %s\n", GetPlayerName(), m_PreControlData.m_iAccount, pBot->GetPlayerName() );

	SetControlledBot( NULL );
	//UpdateAppearanceIndex();
	m_bIsControllingBot = false;
	m_iControlledBotEntIndex = -1;

	DispatchUpdateTransmitState();
}

/*
CBaseEntity * CCSPlayer::FindNearestThrownGrenade(bool bReverse)
{
	// early out if the option is disabled by the server
	if ( !cv_bot_controllable.GetBool() )
		return NULL;

	float32 flNearestDistSqr = 0.0f;
	CCSBot *pNearestBot = NULL;

	for ( int idx = 1; idx <= gpGlobals->maxClients; ++idx )
	{
		CCSBot *pBot = ToCSBot( UTIL_PlayerByIndex( idx ) );

		if ( !pBot )
			continue;

		if ( !CanControlBot( pBot ) )
			continue;

		if ( bMustBeValidObserverTarget && !IsValidObserverTarget( pBot ) )
			continue;

		const float flDistSqr = GetAbsOrigin().DistToSqr( pBot->GetAbsOrigin() );

		if ( pNearestBot == NULL || flDistSqr < flNearestDistSqr )
		{
			flNearestDistSqr = flDistSqr;
			pNearestBot = pBot;
		}
	}

	return pNearestBot;
}
*/

CCSBot* CCSPlayer::FindNearestControllableBot( bool bMustBeValidObserverTarget )
{
	// early out if the option is disabled by the server
	if ( !cv_bot_controllable.GetBool() )
		return NULL;

	float32 flNearestDistSqr = 0.0f;
	CCSBot *pNearestBot = NULL;

	for ( int idx = 1; idx <= gpGlobals->maxClients; ++idx )
	{
		CCSBot *pBot = ToCSBot( UTIL_PlayerByIndex( idx ) );

		if ( !pBot )
			continue;

		if ( !CanControlBot(pBot ) )
			continue;

		if ( bMustBeValidObserverTarget && !IsValidObserverTarget(pBot ) )
			continue;

		const float flDistSqr = GetAbsOrigin().DistToSqr( pBot->GetAbsOrigin() );

		if ( pNearestBot == NULL || flDistSqr < flNearestDistSqr )
		{
			flNearestDistSqr = flDistSqr;
			pNearestBot = pBot;
		}
	}

	return pNearestBot;
}

#endif // CONTROLABLE BOTS ENABLED



void CCSPlayer::UpdateInventory( bool bInit )
{
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	if ( IsFakeClient() )
		return;

	if ( bInit || !m_Inventory.GetSOC() )
	{
		if ( steamgameserverapicontext->SteamGameServer() )
		{
			CSteamID steamIDForPlayer;
			if ( GetSteamID( &steamIDForPlayer ) )
			{
				CSInventoryManager()->SteamRequestInventory( &m_Inventory, steamIDForPlayer );
			}
		}
	}

	// If we have an SOCache, we've got a connection to the GC
	bool bInvalid = true;
	if ( m_Inventory.GetSOC() )
	{
		bInvalid = m_Inventory.GetSOC()->BIsInitialized() == false;
	}
	//m_Shared.SetLoadoutUnavailable( bInvalid );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Request this player's inventories from the steam backend
//-----------------------------------------------------------------------------
void CCSPlayer::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

#if !defined(NO_STEAM) && !defined( NO_STEAM_GAMECOORDINATOR )
	m_Inventory.RemoveListener( this );
	m_Inventory.Shutdown();
#endif
}

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
//-----------------------------------------------------------------------------
// Purpose: Steam has just notified us that the player changed his inventory
//-----------------------------------------------------------------------------
void CCSPlayer::InventoryUpdated( CPlayerInventory *pInventory )
{
	UpdateEquippedCoinFromInventory();
	UpdateEquippedMusicFromInventory();
	UpdateEquippedPlayerSprayFromInventory();
	UpdatePersonaDataFromInventory();

	//m_Shared.SetLoadoutUnavailable( false );

	// Make sure we're wearing the right skin.
	//SetPlayerModel();

	GiveDefaultItems();
}

void OnInventoryUpdatedForSteamID( CSteamID steamID )
{
	if ( !steamID.IsValid() ) return;
	if ( !steamID.BIndividualAccount() ) return;
	if ( !steamID.GetAccountID() ) return;

	extern CCSPlayer* FindPlayerFromAccountID( uint32 account_id );
	if ( CCSPlayer *pPlayer = FindPlayerFromAccountID( steamID.GetAccountID() ) )
	{
		pPlayer->UpdateEquippedCoinFromInventory();
		pPlayer->UpdateEquippedMusicFromInventory();
		pPlayer->UpdateEquippedPlayerSprayFromInventory();
		pPlayer->UpdatePersonaDataFromInventory();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Requests that the GC confirm that this player is supposed to have 
//			an SO cache on this gameserver and send it again if so.
//-----------------------------------------------------------------------------
void CCSPlayer::VerifySOCache()
{
	/** Removed for partner depot **/
	return;
}

#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

void CCSPlayer::IncrementFragCount( int nCount, int nHeadshots )
{
	// calculate frag count properly for a bot-controlled player
	if( IsControllingBot() )
	{
		CCSPlayer* controlledPlayerScorer = GetControlledBot();
		if( controlledPlayerScorer )
		{
			controlledPlayerScorer->IncrementFragCount( nCount, nHeadshots );				
		}

		// Keep track in QMM data for aggregate kills even when using a bot
		if ( m_uiAccountId && CSGameRules() && !CSGameRules()->IsWarmupPeriod() )
		{
			++ m_iEnemyKillsAgg;
			if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
			{
				CCSGameRules::CQMMPlayerData_t &qmmPlayerData = *pQMM;
				qmmPlayerData.m_numEnemyKillsAgg = m_iEnemyKillsAgg;
			}
		}
		return;
	}

	m_iFrags += nCount;
	pl.frags = m_iFrags;
	
	if ( nCount == -1 )
	{
		++m_iNumRoundTKs;
	}
	else if ( ( nCount == 1 ) && ( nHeadshots != -1 ) )
	{
		++m_iNumRoundKills;
		if ( nHeadshots == 1 )
		{
			++m_iNumRoundKillsHeadshots;
		}

		if ( CSGameRules() && !CSGameRules()->IsWarmupPeriod() )
		{
			++ m_iEnemyKills;
			++ m_iEnemyKillsAgg;
			if ( nHeadshots == 1 )
			{
				++ m_iEnemyKillHeadshots;
			}

			if ( m_iNumRoundKills == 3 ) // We are now 3K!
			{
				++ m_iEnemy3Ks;
			}
			else if ( m_iNumRoundKills == 4 ) // We are now 4K!
			{
				-- m_iEnemy3Ks;
				++ m_iEnemy4Ks;
			}
			else if ( m_iNumRoundKills == 5 ) // We are now 5K!
			{
				-- m_iEnemy4Ks;
				++ m_iEnemy5Ks;
			}

			if ( CSGameRules()->m_bNoEnemiesKilled )
			{
				CSGameRules()->m_bNoEnemiesKilled = false;
				++ m_numFirstKills;
				DevMsg( "Player '%s'[%08X] got first kill of the round.\n",
					GetPlayerName(), GetHumanPlayerAccountID() );
			}

			int nMyTeamID = GetTeamNumber();
			if ( ( nMyTeamID == TEAM_TERRORIST ) || ( nMyTeamID == TEAM_CT ) )
			{
				// Is this a clutch kill? There must be no alive teammates if so:
				bool bFoundAliveTeammates = false;
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
					if ( pPlayer && ( pPlayer != this ) && pPlayer->IsAlive()
						&& ( pPlayer->GetTeamNumber() == nMyTeamID ) )
					{
						bFoundAliveTeammates = true;
						break;
					}
				}
				
				if ( !bFoundAliveTeammates )
				{
					++ m_numClutchKills;
				}
			}
		}

		if ( CSGameRules()->ShouldRecordMatchStats() )
		{
			++ m_iMatchStats_Kills.GetForModify( CSGameRules()->GetRoundsPlayed() );

			( nHeadshots == 1 ) ? ++ m_iMatchStats_HeadShotKills.GetForModify( CSGameRules()->GetRoundsPlayed() ) : 0;
		}
	}

	// calculate killstreak.
	//if ( gpGlobals->curtime < GetLastKillTime() + 8.0  )
	{
		IncrementKillStreak( nCount );
	}
	//else
	//{
	//	ResetKillStreak();
	//}
	SetLastKillTime( gpGlobals->curtime );



	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			CCSGameRules::CQMMPlayerData_t &qmmPlayerData = *pQMM;
			qmmPlayerData.m_numKills = m_iFrags;

			qmmPlayerData.m_numEnemyKills = m_iEnemyKills;
			qmmPlayerData.m_numEnemyKillHeadshots = m_iEnemyKillHeadshots;
			qmmPlayerData.m_numEnemy3Ks = m_iEnemy3Ks;
			qmmPlayerData.m_numEnemy4Ks = m_iEnemy4Ks;
			qmmPlayerData.m_numEnemy5Ks = m_iEnemy5Ks;
			qmmPlayerData.m_numEnemyKillsAgg = m_iEnemyKillsAgg;
			qmmPlayerData.m_numFirstKills = m_numFirstKills;
			qmmPlayerData.m_numClutchKills = m_numClutchKills;
			qmmPlayerData.m_numPistolKills = m_numPistolKills;
			qmmPlayerData.m_numSniperKills = m_numSniperKills;

			if ( CSGameRules()->ShouldRecordMatchStats() )
			{
				qmmPlayerData.m_iMatchStats_Kills[ CSGameRules()->GetRoundsPlayed() ] = m_iMatchStats_Kills.Get( CSGameRules()->GetRoundsPlayed() );
				qmmPlayerData.m_iMatchStats_HeadShotKills[ CSGameRules()->GetRoundsPlayed() ] = m_iMatchStats_HeadShotKills.Get( CSGameRules()->GetRoundsPlayed() );
			}

		}
	}
}

void CCSPlayer::IncrementTeamKillsCount( int nCount )
{
	m_iTeamKills += nCount;

	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			pQMM->m_numTeamKills = m_iTeamKills;
		}
	}
}

void CCSPlayer::IncrementHostageKillsCount( int nCount )
{
	m_iHostagesKilled += nCount;

	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			pQMM->m_numHostageKills = m_iHostagesKilled;
		}
	}
}

void CCSPlayer::IncrementTeamDamagePoints( int numDamagePoints )
{
	m_nTeamDamageGivenForMatch += numDamagePoints;

	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			pQMM->m_numTeamDamagePoints = m_nTeamDamageGivenForMatch;
		}
	}
}

void CCSPlayer::IncrementAssistsCount( int nCount )
{
	// calculate assists properly for a bot-controlled player
	if( IsControllingBot() )
	{
		CCSPlayer* controlledPlayerScorer = GetControlledBot();
		if( controlledPlayerScorer )
		{
			controlledPlayerScorer->IncrementAssistsCount( nCount );				
		}
		return;
	}
	m_iAssists += nCount;
	pl.assists = m_iAssists;

	if ( CSGameRules()->ShouldRecordMatchStats() )
	{
		++ m_iMatchStats_Assists.GetForModify( CSGameRules()->GetTotalRoundsPlayed() );
	}

	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			pQMM->m_numAssists = m_iAssists;

			if ( CSGameRules()->ShouldRecordMatchStats() )
			{
				pQMM->m_iMatchStats_Assists[ CSGameRules()->GetTotalRoundsPlayed() ] = m_iMatchStats_Assists.GetForModify( CSGameRules()->GetTotalRoundsPlayed() );
			}
		}
	}
}

void CCSPlayer::IncrementDeathCount( int nCount )
{

	// calculate death count properly for a bot-controlled player
	if( IsControllingBot() )
	{
		CCSPlayer* controlledPlayerScorer = GetControlledBot();
		if( controlledPlayerScorer )
		{
			controlledPlayerScorer->IncrementDeathCount( nCount );			
		}
		return;
	}

	m_iDeaths += nCount;
	pl.deaths = m_iDeaths;

	if ( CSGameRules()->ShouldRecordMatchStats() )
	{
		++ m_iMatchStats_Deaths.GetForModify( CSGameRules()->GetTotalRoundsPlayed() );
	}

	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			pQMM->m_numDeaths = m_iDeaths;

			if ( CSGameRules()->ShouldRecordMatchStats() )
			{
				pQMM->m_iMatchStats_Deaths[ CSGameRules()->GetTotalRoundsPlayed() ] = m_iMatchStats_Deaths.Get( CSGameRules()->GetTotalRoundsPlayed() );
			}
		}
	}
}

void CCSPlayer::SetLastKillTime( float time )
{
	m_flLastKillTime = time;
}

float CCSPlayer::GetLastKillTime()
{
	return m_flLastKillTime;
}

void CCSPlayer::IncrementKillStreak( int nCount )
{
	m_iKillStreak += nCount;

	if ( CSGameRules()->IsPlayingGunGameProgressive() && m_iKillStreak > 1 )
	{
		char strStreak[64];
		Q_snprintf( strStreak, sizeof( strStreak ), "%d", m_iKillStreak );
		if ( m_iKillStreak >= 4 )
		{
			Radio( "OnARollBrag" );

			ClientPrint( this, HUD_PRINTCENTER, "#Player_Killing_Spree_more", strStreak );

			CRecipientFilter filter;
			filter.AddAllPlayers();
			filter.MakeReliable();
			CFmtStr fmtEntName( "#ENTNAME[%d]%s", entindex(), GetPlayerName() );
			UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#Player_On_Killing_Spree", fmtEntName.Access(), strStreak );
		}
// 		else if ( m_iKillStreak >= 4 )
// 		{
// 			ClientPrint( this, HUD_PRINTCENTER, "#Player_Killing_Spree_4" );
// 		}
// 		else if ( m_iKillStreak >= 3 )
// 		{
// 			ClientPrint( this, HUD_PRINTCENTER, "#Player_Killing_Spree_3" );
// 		}
// 		else
// 		{
// 			ClientPrint( this, HUD_PRINTCENTER, "#Player_Killing_Spree_2" );
// 		}
	}
}

void CCSPlayer::ResetKillStreak()
{
	m_iKillStreak = 0;
}

int CCSPlayer::GetKillStreak()
{
	return m_iKillStreak;
}

void CCSPlayer::AddContributionScore( int iPoints )
{ 
	// calculate score count properly for a bot-controlled player
	if( IsControllingBot() )
	{
		CCSPlayer* controlledPlayerScorer = GetControlledBot();
		if( controlledPlayerScorer )
		{
			controlledPlayerScorer->AddContributionScore( iPoints );
		}
	}
	else
	{
		m_iContributionScore += iPoints; 	
		AddRoundContributionScore( iPoints );
		// note, the round score isn't capped to be positive...  on any given round we expect that it may go negative (example:  for determining griefers )
	}

	if ( m_iContributionScore < 0 )
		m_iContributionScore = 0; // cap negative points at zero
	pl.score = m_iContributionScore;

	// Keep track in QMM data
	if ( m_uiAccountId && CSGameRules() )
	{
		if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( m_uiAccountId ) )
		{
			pQMM->m_numScorePoints = m_iContributionScore;
		}
	}
}

void CCSPlayer::AddScore( int iPoints )
{
	// calculate score count properly for a bot-controlled player
	if( IsControllingBot() )
	{
		CCSPlayer* controlledPlayerScorer = GetControlledBot();
		if( controlledPlayerScorer )
		{
			controlledPlayerScorer->AddScore( iPoints );
		}
	}
	else
	{
		m_iScore += iPoints; 
		// note, the round score isn't capped to be positive...  on any given round we expect that it may go negative (example:  determining griefers )
	}

	if ( m_iScore < 0 )
		m_iScore = 0; // cap negative points at zero
	
}
void CCSPlayer::AddRoundContributionScore( int iPoints )
{
	m_iRoundContributionScore += iPoints;
}

void CCSPlayer::AddRoundProximityScore( int iPoints )
{
	m_iRoundProximityScore += iPoints;
}

int CCSPlayer::GetNumConcurrentDominations( )
{
	//Check concurrent dominations achievement
	int numConcurrentDominations = 0;
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && IsPlayerDominated( pPlayer->entindex() ) )
		{
			numConcurrentDominations++;
		}
	}
	return numConcurrentDominations;
}


CON_COMMAND_F( observer_use, "", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS )
{
	CCSPlayer *pPlayer = ToCSPlayer( UTIL_GetCommandClient() );

	if ( pPlayer )
		pPlayer->ObserverUse( true );
}

		  
//This effectively disables the rendering of the flashbang effect,
//but allows the server to finish and game rules processing.
//(Used to hide effect at the end of a match so that players can see the scoreboard. )
void CCSPlayer::Unblind( void )
{
	m_flFlashDuration = 0.0f;
	m_flFlashMaxAlpha = 0.0f;
}


// the client already has a players account, sending a team only message doesn't make it more secure and it breaks GOTV
// void CCSPlayer::UpdateTeamMoney()
// {	
// 	if ( ( m_flLastMoneyUpdateTime + 0.1f ) > gpGlobals->curtime )
// 		return;
// 
// 	m_flLastMoneyUpdateTime = gpGlobals->curtime;	
// 
// 	CSingleUserRecipientFilter user( this );
// 	UserMessageBegin( user, "UpdateTeamMoney" );
// 
// 	for ( int i=0; i < MAX_PLAYERS; i++ )
// 	{
// 		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i+1 ) );
// 
// 		if ( !pPlayer || !pPlayer->IsConnected() )
// 			continue; 
// 
// 		if ( pPlayer->GetTeamNumber() != GetTeamNumber() && GetTeamNumber() != TEAM_SPECTATOR )
// 			continue; 		
// 
// 		WRITE_BYTE( i+1 ); // player index as entity
// 		WRITE_SHORT( pPlayer->GetAccountForScoreboard() );
// 		
// 	}
// 
// 	WRITE_BYTE( 0 ); // end marker
// 
// 	MessageEnd();
// }

int CCSPlayer::GetAccountForScoreboard()
{
	if ( IsControllingBot() )
		return m_PreControlData.m_iAccount;

	CCSPlayer* pControllingPlayer = GetControlledByPlayer();
	if ( pControllingPlayer != NULL )
		return pControllingPlayer->m_iAccount;
	return m_iAccount;
}

void CCSPlayer::UpdateRankFromKV( KeyValues *pKV )
{
	if ( !pKV )
		return;

	for ( int i = 0; i < MEDAL_CATEGORY_ACHIEVEMENTS_END; ++i )
	{
		int rank = pKV->GetInt( CFmtStr( "rank%d", i), -1 );
		if ( rank >= 0 && rank < MEDAL_RANK_COUNT )
		{
			SetRank( (MedalCategory_t)i, (MedalRank_t)rank );
		}
	}
}

void CCSPlayer::SetRank( MedalCategory_t category, MedalRank_t rank )
{
	m_rank.Set( category, rank );
}

void CCSPlayer::UpdateEquippedCoinFromInventory()
{
	m_bNeedToUpdateCoinFromInventory = true;
}

void CCSPlayer::SetMusicID( uint16 unMusicID )
{
	m_unMusicID.Set( unMusicID );
}
void CCSPlayer::UpdateEquippedMusicFromInventory()
{
	m_bNeedToUpdateMusicFromInventory = true;
}

void CCSPlayer::UpdateEquippedPlayerSprayFromInventory()
{
	m_bNeedToUpdatePlayerSprayFromInventory = true;
}

void CCSPlayer::UpdatePersonaDataFromInventory()
{
	m_bNeedToUpdatePersonaDataPublicFromInventory = true;
}
CEconPersonaDataPublic const * CCSPlayer::GetPersonaDataPublic() const
{
	return m_pPersonaDataPublic;
}

bool CCSPlayer::CanKickFromTeam( int kickTeam )
{
	int oppositeKickTeam = ( kickTeam == TEAM_TERRORIST ) ? TEAM_CT : TEAM_TERRORIST;

	// Address issue with bots getting kicked during half-time (this was resulting in bots from the wrong team being kicked)
	return ( ( GetTeamNumber() == kickTeam && !WillSwitchTeamsAtRoundReset() ) ||
			 ( GetTeamNumber() == oppositeKickTeam && WillSwitchTeamsAtRoundReset() ) );
}

bool CCSPlayer::CanHearAndReadChatFrom( CBasePlayer *pPlayer )
{
	// can always hear the console unless we're ignoring all chat
	if ( !pPlayer )
		return m_iIgnoreGlobalChat != CHAT_IGNORE_EVERYTHING;

	// check if we're ignoring all chat
	if ( m_iIgnoreGlobalChat == CHAT_IGNORE_BROADCAST_AND_TEAM )
		return false;

	// check if we're ignoring all but teammates
	if ( m_iIgnoreGlobalChat == CHAT_IGNORE_BROADCAST && IsOtherEnemy( pPlayer->entindex() ) )
		return false;

	return true;
}

void CCSPlayer::ObserverUse( bool bIsPressed )
{
	if ( !bIsPressed )
		return;

#if CS_CONTROLLABLE_BOTS_ENABLED
 	CBasePlayer * target = ToBasePlayer( GetObserverTarget() );
 
 	if ( target && target->IsBot() )	
 	{
 		if ( m_bCanControlObservedBot )
 		{
			CCSPlayer *pPlayer = this;

			CCSBot *pBot = ToCSBot( pPlayer->GetObserverTarget() );

			if ( pBot != NULL && pBot->IsBot() )
			{
				if ( !pPlayer->IsDead() )
				{
					Msg( "Player %s tried to take control of bot %s but was disallowed by the server\n", pPlayer->GetPlayerName(), pBot->GetPlayerName() );
				}
				else if ( !cv_bot_controllable.GetBool() )
				{
					Msg( "Player %s tried to take control of bot %s but was disallowed by the server\n", pPlayer->GetPlayerName(), pBot->GetPlayerName() );
				}
				else if ( ( pPlayer->GetPendingTeamNumber() != TEAM_UNASSIGNED ) && ( pPlayer->GetPendingTeamNumber() != TEAM_INVALID ) && ( pPlayer->GetTeamNumber() != pPlayer->GetPendingTeamNumber() ) )
				{
					Msg( "Player %s tried to take control of bot %s but was disallowed due to a pending team switch\n", pPlayer->GetPlayerName(), pBot->GetPlayerName() );
				}
				else if ( engine->GetClientHltvReplayDelay( pPlayer->entindex() - 1 ) )
				{
					Msg( "Player %s tried to take control of bot %s but was in replay mode\n", pPlayer->GetPlayerName(), pBot->GetPlayerName() );
				}
				else if ( pPlayer->TakeControlOfBot( pBot ) )
				{
					Msg( "Player %s took control bot %s (%d)\n", pPlayer->GetPlayerName(), pBot->GetPlayerName(), pBot->entindex() );
				}
				else
				{
					Msg( "Player %s tried to take control of bot %s but failed\n", pPlayer->GetPlayerName(), pBot->GetPlayerName() );
				}
			}
			else 
			{
				Msg( "Player %s tried to take control of bot but none could be found\n", pPlayer->GetPlayerName() );
			}

 			return;
 		}
 	}
#endif

	BaseClass::ObserverUse( bIsPressed );

}

bool CCSPlayer::GetBulletHitLocalBoneOffset( const trace_t &tr, int &boneIndexOut, Vector &vecPositionOut, QAngle &angAngleOut )
{
	int nBoneIndex = GetHitboxBone( tr.hitbox );

	if ( nBoneIndex < 0 || tr.DidHit() == false )
		return false;

	// build a matrix from the trace hit start and end position
	matrix3x4_t matWorldSpaceBulletHit;
	VectorMatrix( tr.startpos - tr.endpos, matWorldSpaceBulletHit );
	PositionMatrix( tr.endpos, matWorldSpaceBulletHit );
	
	// get the transform of the bone that owns the hitbox
	matrix3x4_t matBoneToWorldTransform;
	GetBoneTransform( nBoneIndex, matBoneToWorldTransform );

	// get the local transform of the hit transform relative to the bone transform
	matrix3x4_t matHitLocal;
	MatrixInvert( matBoneToWorldTransform, matHitLocal );
	MatrixMultiply( matHitLocal, matWorldSpaceBulletHit, matHitLocal );

	boneIndexOut = nBoneIndex;
	MatrixAngles( matHitLocal, angAngleOut, vecPositionOut );

	return ( angAngleOut.IsValid() && vecPositionOut.IsValid() );
}

#if defined( DEBUG_FLASH_BANG )
void cc_CreateFlashbangAtEyes_f( const CCommand &args )
{
	CCSPlayer* pPlayer = ToCSPlayer(UTIL_PlayerByIndex(1));
	if ( pPlayer )
	{
		CBaseCombatCharacter *pBaseCombatCharacter = NULL;
		if ( pPlayer->GetObserverTarget() )
		{
			pBaseCombatCharacter = pPlayer->GetObserverTarget()->MyCombatCharacterPointer();
		}
		else
		{
			pBaseCombatCharacter = pPlayer->MyCombatCharacterPointer();
		}
		if ( pBaseCombatCharacter )
		{
			Vector vForward;
			pPlayer->EyeVectors( &vForward );
			Vector vecSrc = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset() + vForward * 16; 
			Vector vecVel = pPlayer->GetAbsVelocity();

			Vector vecThrow = vForward * 100 + (pPlayer->GetAbsVelocity() * 1.25);
			const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( WEAPON_FLASHBANG );

			CFlashbangProjectile::Create( 
				vecSrc,
				vec3_angle,
				vecThrow,
				AngularImpulse(600,random->RandomInt(-1200,1200),0),
				pBaseCombatCharacter,
				*pWeaponInfo );
		}
	}
}

ConCommand cc_CreateFlashbangAtEyes( "CreateFlashbangAtEyes", cc_CreateFlashbangAtEyes_f, "Create a prediction error", FCVAR_CHEAT );
#endif

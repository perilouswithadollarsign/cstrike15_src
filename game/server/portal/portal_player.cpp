//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for Portal.
//
//=============================================================================//

#include "cbase.h"
#include "portal_player.h"
#include "globalstate.h"
#include "game_timescale_shared.h"
#include "trains.h"
#include "game.h"
#include "portal_player_shared.h"
#include "predicted_viewmodel.h"
#include "in_buttons.h"
#include "portal_gamerules.h"
#include "portal_mp_gamerules.h"
#include "weapon_portalgun.h"
#include "portal/weapon_physcannon.h"
#include "KeyValues.h"
#include "team.h"
#include "eventqueue.h"
#include "weapon_portalbase.h"
#include "engine/IEngineSound.h"
#include "ai_basenpc.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "portal_base2d_shared.h"
#include "player_pickup.h"	// for player pickup code
#include "vphysics/player_controller.h"
#include "datacache/imdlcache.h"
#include "bone_setup.h"
#include "portal_gamestats.h"
#include "physicsshadowclone.h"
#include "physics_prop_ragdoll.h"
#include "soundenvelope.h"
#include "ai_baseactor.h"		// For expressors, vcd playing
#include "ai_speech.h"		// For expressors, vcd playing
#include "sceneentity.h"	// has the VCD precache function
#include "gamemovement.h"
#include "particle_parse.h"	// for dispatching particle effects
#include "collisionutils.h"
#include "mp_shareddefs.h"
#include "prop_portal_shared.h"
#include "world.h"
#include "pointsurvey.h"
#include "weapon_paintgun.h"
#include "paint_swap_guns.h"
#include "info_camera_link.h"
#include "prop_weightedcube.h"
#include "props.h"
#include "sendprop_priorities.h"
#include "env_portal_laser.h"
#include "npc_portal_turret_floor.h"
#include "dt_utlvector_send.h"
#include "portal_mp_stats.h"
#include "inetchannelinfo.h"
#include "trigger_catapult.h"
#include "portal_gamestats.h"
#include "portal_ui_controller.h"
#include "matchmaking/imatchframework.h"
#include "matchmaking/portal2/imatchext_portal2.h"
#include "portal2_research_data_tracker.h"

#if !defined(NO_STEAM) && !defined(_PS3)
#include "gc_serversystem.h"
#endif

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	#include "econ_gcmessages.h"
#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

#define PORTAL_RESPAWN_DELAY	1.0f	// Seconds

#define PORTAL_WALK_SPEED	175

#define CATCHPATNERNOTCONNECTING_THINK_CONTEXT			"CatchPatnerNotConnectingThinkContext"

//HACKHACK: Keep track of which player has which gun between levels
int g_iPortalGunPlayerTeam = TEAM_BLUE;

extern CBaseEntity	*g_pLastSpawn;

extern void respawn(CBaseEntity *pEdict, bool fCopyCorpse);

#if USE_SLOWTIME
ConVar slowtime_regen_per_second( "slowtime_regen_per_second", "4" );
ConVar slowtime_max( "slowtime_max", "8", FCVAR_REPLICATED );
ConVar slowtime_must_refill( "slowtime_must_refill", "0" );
ConVar slowtime_speed( "slowtime_speed", "0.1", FCVAR_REPLICATED );
#endif // USE_SLOWTIME

ConVar playtest_random_death( "playtest_random_death", "0", FCVAR_NONE );
float flNextDeathTime = 0.0f; // Used by the random death system to randomly kill a player to death

ConVar sv_portal_coop_ping_cooldown_time( "sv_portal_coop_ping_cooldown_time", "0.25", FCVAR_CHEAT, "Time (in seconds) between coop pings", true, 0.1f, false, 60.0f );
ConVar sv_portal_coop_ping_indicator_show_to_all_players( "sv_portal_coop_ping_indicator_show_to_all_players", "0" );
extern ConVar sv_player_funnel_gimme_dot;
ConVar sv_zoom_stop_movement_threashold("sv_zoom_stop_movement_threashold", "4.0", FCVAR_REPLICATED, "Move command amount before breaking player out of toggle zoom." );
ConVar sv_zoom_stop_time_threashold("sv_zoom_stop_time_threashold", "5.0", FCVAR_REPLICATED, "Time amount before breaking player out of toggle zoom." );
extern ConVar sv_player_funnel_into_portals;

#define sv_can_carry_both_guns		0	//ConVar sv_can_carry_both_guns("sv_can_carry_both_guns", "0", FCVAR_REPLICATED | FCVAR_CHEAT);
#define sv_can_swap_guns			1	//ConVar sv_can_swap_guns("sv_can_swap_guns", "1", FCVAR_REPLICATED | FCVAR_CHEAT);
#define sv_can_swap_guns_anytime	1	//ConVar sv_can_swap_guns_anytime( "sv_can_swap_guns_anytime", "1", FCVAR_CHEAT );

static ConVar portal_tauntcam_dist( "portal_tauntcam_dist", "75", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar sp_fade_and_force_respawn( "sp_fade_and_force_respawn", "1", FCVAR_CHEAT );

ConVar mp_taunt_item( "mp_taunt_item", "", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Temporary for testing what will happen when a taunt item is in inventory." );

extern ConVar mp_should_gib_bots;
extern ConVar breakable_disable_gib_limit;
extern ConVar breakable_multiplayer;
ConVar mp_server_player_team( "mp_server_player_team", "0", FCVAR_DEVELOPMENTONLY );
ConVar mp_wait_for_other_player_timeout( "mp_wait_for_other_player_timeout", "100", FCVAR_CHEAT, "Maximum time that we wait in the transition loading screen for the other player." );
ConVar mp_wait_for_other_player_notconnecting_timeout( "mp_wait_for_other_player_notconnecting_timeout", "10", FCVAR_CHEAT, "Maximum time that we wait in the transition loading screen after we fully loaded for partner to start loading." );
ConVar mp_dev_wait_for_other_player( "mp_dev_wait_for_other_player", "1", FCVAR_DEVELOPMENTONLY, "Force waiting for the other player." );

extern ConVar sv_speed_normal;
extern ConVar sv_post_teleportation_box_time;
extern ConVar sv_press_jump_to_bounce;
extern ConVar sv_use_trace_duration;

extern ConVar sv_bonus_challenge;

extern ConVar ai_debug_dyninteractions;

extern void PaintPowerPickup( int colorIndex, CBasePlayer *pPlayer );


#define COOP_PING_DECAL_NAME "overlays/coop_ping_decal"
#define COOP_PING_SOUNDSCRIPT_NAME "Player.Coop_Ping"
#define COOP_PING_PARTICLE_NAME "command_target_ping"

#define TLK_PLAYER_KILLED "TLK_PLAYER_KILLED"
#define TLK_PLAYER_SHOT "TLK_PLAYER_SHOT"
#define TLK_PLAYER_BURNED "TLK_PLAYER_BURNED"

#define ALLOWED_TEAM_TAUNT_Z_DIST 30.f

// FIXME: Used for temp damage scaling -- jdw
extern ConVar sk_dmg_take_scale1;


const char *g_pszBallBotHelmetModel = "models/player/ballbot/ballbot_cage.mdl";
const char *g_pszEggBotHelmetModel = "models/player/eggbot/eggbot_cage.mdl";

const char *g_pszBallBotAntennaModel = "models/player/ballbot/ballbot_flag.mdl";
const char *g_pszEggBotAntennaModel = "models/player/eggbot/eggbot_flag.mdl";


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
SendPropInt( SENDINFO( m_nData ), 32 ),
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent( "PlayerAnimEvent" );

void TE_PlayerAnimEvent( CBasePlayer *pPlayer, PlayerAnimEvent_t event, int nData )
{
	CPVSFilter filter( (const Vector&)pPlayer->EyePosition() );

	g_TEPlayerAnimEvent.m_hPlayer = pPlayer;
	g_TEPlayerAnimEvent.m_iEvent = event;
	g_TEPlayerAnimEvent.m_nData = nData;
	g_TEPlayerAnimEvent.Create( filter, 0 );
}



//=================================================================================
//
// Ragdoll Entity
//
class CPortalRagdoll : public CBaseAnimatingOverlay, public CDefaultPlayerPickupVPhysics
{
public:

	DECLARE_CLASS( CPortalRagdoll, CBaseAnimatingOverlay );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CPortalRagdoll()
	{
		m_hPlayer.Set( NULL );
		m_vecRagdollOrigin.Init();
		m_vecRagdollVelocity.Init();
	}

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	// In case the client has the player entity, we transmit the player index.
	// In case the client doesn't have it, we transmit the player's model index, origin, and angles
	// so they can create a ragdoll in the right place.
	CNetworkHandle( CBaseEntity, m_hPlayer );	// networked entity handle
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
};

LINK_ENTITY_TO_CLASS( portal_ragdoll, CPortalRagdoll );

IMPLEMENT_SERVERCLASS_ST_NOBASE( CPortalRagdoll, DT_PortalRagdoll )
SendPropVector( SENDINFO(m_vecRagdollOrigin), -1,  SPROP_COORD ),
SendPropEHandle( SENDINFO( m_hPlayer ) ),
SendPropModelIndex( SENDINFO( m_nModelIndex ) ),
SendPropInt		( SENDINFO(m_nForceBone), 8, 0 ),
SendPropVector	( SENDINFO(m_vecForce), -1, SPROP_NOSCALE ),
SendPropVector( SENDINFO( m_vecRagdollVelocity ) ),
END_SEND_TABLE()


BEGIN_DATADESC( CPortalRagdoll )

	DEFINE_FIELD( m_vecRagdollOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecRagdollVelocity, FIELD_VECTOR ),

END_DATADESC()


CEntityPortalledNetworkMessage::CEntityPortalledNetworkMessage( void )
{
	m_hEntity = NULL;
	m_hPortal = NULL;
	m_fTime = 0.0f;
	m_bForcedDuck = false;
	m_iMessageCount = 0;;
}

BEGIN_SEND_TABLE_NOBASE( CEntityPortalledNetworkMessage, DT_EntityPortalledNetworkMessage )
		SendPropEHandle( SENDINFO_NOCHECK(m_hEntity) ),
		SendPropEHandle( SENDINFO_NOCHECK(m_hPortal) ),
		SendPropFloat( SENDINFO_NOCHECK(m_fTime), -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT ),
		SendPropBool( SENDINFO_NOCHECK(m_bForcedDuck) ),
		SendPropInt( SENDINFO_NOCHECK(m_iMessageCount) ),
END_SEND_TABLE()

extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

// specific to the local player
BEGIN_SEND_TABLE_NOBASE( CPortal_Player, DT_PortalLocalPlayerExclusive )
	// send a hi-res origin and view offset to the local player for use in prediction
	SendPropVectorXY(SENDINFO(m_vecOrigin),               -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY, SENDPROP_LOCALPLAYER_ORIGINXY_PRIORITY ),
	SendPropFloat   (SENDINFO_VECTORELEM(m_vecOrigin, 2), -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ, SENDPROP_LOCALPLAYER_ORIGINZ_PRIORITY ),
	SendPropVector	(SENDINFO(m_vecViewOffset), -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT ),

	SendPropQAngles( SENDINFO( m_vecCarriedObjectAngles ) ),
	SendPropVector( SENDINFO( m_vecCarriedObject_CurPosToTargetPos )  ),
	SendPropQAngles( SENDINFO( m_vecCarriedObject_CurAngToTargetAng ) ),
	//a message buffer for entity teleportations that's guaranteed to be in sync with the post-teleport updates for said entities
	SendPropUtlVector( SENDINFO_UTLVECTOR( m_EntityPortalledNetworkMessages ), CPortal_Player::MAX_ENTITY_PORTALLED_NETWORK_MESSAGES, SendPropDataTable( NULL, 0, &REFERENCE_SEND_TABLE( DT_EntityPortalledNetworkMessage ) ) ),
	SendPropInt( SENDINFO( m_iEntityPortalledNetworkMessageCount ) ),
END_SEND_TABLE()

// all players except the local player
BEGIN_SEND_TABLE_NOBASE( CPortal_Player, DT_PortalNonLocalPlayerExclusive )
	// send a lo-res origin and view offset to other players
	// send a lo-res origin to other players
	SendPropVectorXY( SENDINFO( m_vecOrigin ), 				 CELL_BASEENTITY_ORIGIN_CELL_BITS, SPROP_CELL_COORD_LOWPRECISION, 0.0f, HIGH_DEFAULT, CBaseEntity::SendProxy_CellOriginXY, SENDPROP_NONLOCALPLAYER_ORIGINXY_PRIORITY ),
	SendPropFloat   ( SENDINFO_VECTORELEM( m_vecOrigin, 2 ), CELL_BASEENTITY_ORIGIN_CELL_BITS, SPROP_CELL_COORD_LOWPRECISION, 0.0f, HIGH_DEFAULT, CBaseEntity::SendProxy_CellOriginZ, SENDPROP_NONLOCALPLAYER_ORIGINZ_PRIORITY ),
	SendPropFloat	(SENDINFO_VECTORELEM(m_vecViewOffset, 0), 10, SPROP_CHANGES_OFTEN, -128.0, 128.0f),
	SendPropFloat	(SENDINFO_VECTORELEM(m_vecViewOffset, 1), 10, SPROP_CHANGES_OFTEN, -128.0, 128.0f),
	SendPropFloat	(SENDINFO_VECTORELEM(m_vecViewOffset, 2), 10, SPROP_CHANGES_OFTEN, -128.0, 128.0f),
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE( CPortalPlayerShared, DT_PortalPlayerShared )
	SendPropInt( SENDINFO( m_nPlayerCond ), PORTAL_COND_LAST, (SPROP_UNSIGNED|SPROP_CHANGES_OFTEN) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( player, CPortal_Player );

IMPLEMENT_SERVERCLASS_ST(CPortal_Player, DT_Portal_Player)

	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),
	SendPropExclude( "DT_LocalPlayerExclusive", "m_vecViewOffset[0]" ),
	SendPropExclude( "DT_LocalPlayerExclusive", "m_vecViewOffset[1]" ),
	SendPropExclude( "DT_LocalPlayerExclusive", "m_vecViewOffset[2]" ),

#ifdef PORTAL_PLAYER_PREDICTION
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	SendPropExclude( "DT_BaseFlex", "m_viewtarget" ),
	SendPropExclude( "DT_BaseFlex", "m_flexWeight" ),
	SendPropExclude( "DT_BaseFlex", "m_blinktoggle" ),

	// portal_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),
#endif // PORTAL_PLAYER_PREDICTION

	SendPropDataTable(SENDINFO_DT(m_PortalLocal), &REFERENCE_SEND_TABLE(DT_PortalLocal), SendProxy_SendLocalDataTable),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 11, SPROP_CHANGES_OFTEN ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 11, SPROP_CHANGES_OFTEN ),
	SendPropEHandle( SENDINFO( m_hRagdoll ) ),
	SendPropInt( SENDINFO( m_iSpawnInterpCounter), 4 ),
	SendPropInt( SENDINFO( m_iPlayerSoundType), 3 ),
	SendPropBool( SENDINFO( m_bHeldObjectOnOppositeSideOfPortal) ),
	SendPropBool( SENDINFO( m_bPitchReorientation ) ),
	SendPropEHandle( SENDINFO( m_hPortalEnvironment ) ),
	SendPropBool( SENDINFO( m_bIsHoldingSomething ) ),
	SendPropBool( SENDINFO( m_bPingDisabled ) ),
	SendPropBool( SENDINFO( m_bTauntDisabled ) ),
	SendPropBool( SENDINFO( m_bTauntRemoteView ) ),
	SendPropVector( SENDINFO( m_vecRemoteViewOrigin ) ),
	SendPropVector( SENDINFO( m_vecRemoteViewAngles ) ),
	SendPropFloat( SENDINFO( m_fTauntCameraDistance ) ),
	SendPropInt( SENDINFO( m_nTeamTauntState ) ),
	SendPropVector( SENDINFO( m_vTauntPosition ) ),
	SendPropQAngles( SENDINFO( m_vTauntAngles ) ),
	SendPropQAngles( SENDINFO( m_vPreTauntAngles ) ),
	SendPropBool( SENDINFO( m_bTrickFire ) ),
	SendPropEHandle( SENDINFO( m_hTauntPartnerInRange ) ),
	SendPropString( SENDINFO( m_szTauntForce ) ),
	SendPropBool( SENDINFO( m_bUseVMGrab ) ),
	SendPropBool( SENDINFO( m_bUsingVMGrabState ) ),
	SendPropEHandle( SENDINFO( m_hAttachedObject ) ),
	SendPropEHandle( SENDINFO( m_hHeldObjectPortal ) ),
	SendPropFloat( SENDINFO( m_flMotionBlurAmount ) ),

	// Data that only gets sent to the local player
	SendPropDataTable( "portallocaldata", 0, &REFERENCE_SEND_TABLE(DT_PortalLocalPlayerExclusive), SendProxy_SendLocalDataTable ),

	// Data that gets sent to all other players
	SendPropDataTable( "portalnonlocaldata", 0, &REFERENCE_SEND_TABLE(DT_PortalNonLocalPlayerExclusive), SendProxy_SendNonLocalDataTable ),

	SendPropBool( SENDINFO( m_bWantsToSwapGuns ) ),

	SendPropBool( SENDINFO( m_bPotatos ) ),

	// Shared info
	SendPropDataTable( SENDINFO_DT( m_Shared ), &REFERENCE_SEND_TABLE( DT_PortalPlayerShared ) ),

	SendPropFloat( SENDINFO( m_flHullHeight ) ),
	SendPropBool( SENDINFO( m_iSpawnCounter ) ),

	SendPropDataTable( SENDINFO_DT( m_StatsThisLevel ), &REFERENCE_SEND_TABLE(DT_PortalPlayerStatistics), SendProxy_SendLocalDataTable ),

END_SEND_TABLE()

BEGIN_DATADESC( CPortal_Player )

	DEFINE_SOUNDPATCH( m_pWooshSound ),
	DEFINE_SOUNDPATCH( m_pGrabSound ),

	DEFINE_FIELD( m_bHeldObjectOnOppositeSideOfPortal, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hHeldObjectPortal, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bIntersectingPortalPlane, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bStuckOnPortalCollisionObject, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fTimeLastNumSecondsUpdate, FIELD_TIME ),
	DEFINE_FIELD( m_iNumCamerasDetatched, FIELD_INTEGER ),
	DEFINE_FIELD( m_bPitchReorientation, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fNeuroToxinDamageTime, FIELD_TIME ),
	DEFINE_FIELD( m_hPortalEnvironment, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecTotalBulletForce, FIELD_VECTOR ),
	DEFINE_FIELD( m_bSilentDropAndPickup, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hRagdoll, FIELD_EHANDLE ),
	DEFINE_FIELD( m_angEyeAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_iPlayerSoundType, FIELD_INTEGER ),
	DEFINE_FIELD( m_vWorldSpaceCenterHolder, FIELD_POSITION_VECTOR ),
	//DEFINE_FIELD( m_hRemoteTauntCamera, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flLastPingTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_bClientCheckPVSDirty, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flUseKeyCooldownTime, FIELD_TIME ),
	DEFINE_FIELD( m_bIsHoldingSomething, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iLastWeaponFireUsercmd, FIELD_INTEGER ),
	DEFINE_FIELD( m_iSpawnInterpCounter, FIELD_INTEGER ),
	DEFINE_FIELD( m_bPingDisabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bTauntDisabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bTauntRemoteView, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bTauntRemoteViewFOVFixup, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecRemoteViewOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecRemoteViewAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_fTauntCameraDistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_nTeamTauntState, FIELD_INTEGER ),
	DEFINE_FIELD( m_vTauntPosition, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vTauntAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_vPreTauntAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_bTrickFire, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hTauntPartnerInRange, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flMotionBlurAmount, FIELD_FLOAT ),
	DEFINE_AUTO_ARRAY( m_szTauntForce, FIELD_CHARACTER ),
#if USE_SLOWTIME
	DEFINE_FIELD( m_bHasPlayedSlowTimeStopSound, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_pSlowTimeColorFX, FIELD_CLASSPTR ),
#endif // USE_SLOWTIME
	DEFINE_FIELD( m_hGrabbedEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hPortalThroughWhichGrabOccured, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bForcingDrop, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bUseVMGrab, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bUsingVMGrabState, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flUseKeyStartTime, FIELD_TIME ),
	DEFINE_FIELD( m_flAutoGrabLockOutTime, FIELD_TIME ),
	DEFINE_FIELD( m_hAttachedObject, FIELD_EHANDLE ),
	DEFINE_FIELD( m_ForcedGrabController, FIELD_INTEGER ),

	DEFINE_FIELD( m_flTimeLastTouchedGround, FIELD_TIME ),
	DEFINE_FIELD( m_nPortalsEnteredInAirFlags, FIELD_INTEGER ),
	DEFINE_FIELD( m_nAirTauntCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_nWheatleyMonitorDestructionCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_bPotatos, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_PlayerGunType, FIELD_INTEGER ),
	DEFINE_FIELD( m_bSpawnFromDeath, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_flHullHeight, FIELD_FLOAT ),

	DEFINE_FIELD( m_bWasDroppedByOtherPlayerWhileTaunting, FIELD_BOOLEAN ),

	DEFINE_EMBEDDED( m_PortalLocal ),

	//DEFINE_FIELD ( m_PlayerAnimState, CPortalPlayerAnimState ),
	DEFINE_EMBEDDED( m_StatsThisLevel ),
	//DEFINE_FIELD( m_bPlayUseDenySound, FIELD_BOOLEAN ),

	DEFINE_THINKFUNC( PlayerTransitionCompleteThink ),
	DEFINE_THINKFUNC( PlayerCatchPatnerNotConnectingThink ),

END_DATADESC()

BEGIN_ENT_SCRIPTDESC( CPortal_Player, CBaseMultiplayerPlayer , "Player" )
	DEFINE_SCRIPTFUNC( IncWheatleyMonitorDestructionCount, "Set number of wheatley monitors destroyed by the player." )
	DEFINE_SCRIPTFUNC( GetWheatleyMonitorDestructionCount, "Get number of wheatley monitors destroyed by the player." )
	DEFINE_SCRIPTFUNC( TurnOffPotatos, "Turns Off the Potatos material light" )
	DEFINE_SCRIPTFUNC( TurnOnPotatos, "Turns On the Potatos material light" )
END_SCRIPTDESC();

extern const char *g_pszPlayerModel;

const char* g_pszPlayerAnimations = "models/player_animations.mdl";
const char* g_pszBallBotAnimations = "models/ballbot_animations.mdl";
const char* g_pszEggBotAnimations = "models/eggbot_animations.mdl";


class CPortalPlayerModelPrecacher : public CBaseResourcePrecacher
{
public:
	CPortalPlayerModelPrecacher() : CBaseResourcePrecacher( GLOBAL, "CPortalPlayerModelPrecacher" ) {}

	virtual void Cache( IPrecacheHandler *pPrecacheHandler, bool bPrecache, ResourceList_t hResourceList, bool bIgnoreConditionals )
	{
		bool bIsMultiplayer;
		if( bPrecache )
		{
			bIsMultiplayer = g_pGameRules ? g_pGameRules->IsMultiplayer() : (Q_strnicmp( gpGlobals->mapname.ToCStr(), "mp", 2 ) == 0); //either gamerules says it's multiplayer, or the map name implies it
			m_bPreCacheWasMultiplayer = bIsMultiplayer;
		}
		else
		{
			bIsMultiplayer = m_bPreCacheWasMultiplayer;
		}

		if( bIsMultiplayer || bIgnoreConditionals )
		{
			int iModelIndex;
			pPrecacheHandler->CacheResource( MODEL, GetBallBotModel(), bPrecache, hResourceList, &iModelIndex );
			pPrecacheHandler->CacheResource( MODEL, g_pszBallBotAnimations, bPrecache, hResourceList, NULL );
			PrecacheGibsForModel( iModelIndex );

			pPrecacheHandler->CacheResource( MODEL, GetEggBotModel(), bPrecache, hResourceList, &iModelIndex );
			pPrecacheHandler->CacheResource( MODEL, g_pszEggBotAnimations, bPrecache, hResourceList, NULL );
			PrecacheGibsForModel( iModelIndex );

			pPrecacheHandler->CacheResource( MODEL, g_pszBallBotHelmetModel, bPrecache, hResourceList, NULL );
			pPrecacheHandler->CacheResource( MODEL, g_pszEggBotHelmetModel, bPrecache, hResourceList, NULL );
			pPrecacheHandler->CacheResource( MODEL, g_pszBallBotAntennaModel, bPrecache, hResourceList, NULL );
			pPrecacheHandler->CacheResource( MODEL, g_pszEggBotAntennaModel, bPrecache, hResourceList, NULL );
		}

		if( !bIsMultiplayer || bIgnoreConditionals )
		{
			pPrecacheHandler->CacheResource( MODEL, g_pszPlayerModel, bPrecache, hResourceList, NULL );
			pPrecacheHandler->CacheResource( MODEL, g_pszPlayerAnimations, bPrecache, hResourceList, NULL );
		}
	}

	bool m_bPreCacheWasMultiplayer; //just being a little paranoid that precaches and uncaches sync up consistently
};
CPortalPlayerModelPrecacher s_PortalModelPrecacher;


#define MAX_COMBINE_MODELS 4
#define MODEL_CHANGE_INTERVAL 5.0f
#define TEAM_CHANGE_INTERVAL 5.0f

#define PORTALPLAYER_PHYSDAMAGE_SCALE 4.0f

extern ConVar sv_turbophysics;

//----------------------------------------------------
// Player Physics Shadow
//----------------------------------------------------
#define VPHYS_MAX_DISTANCE		2.0
#define VPHYS_MAX_VEL			10
#define VPHYS_MAX_DISTSQR		(VPHYS_MAX_DISTANCE*VPHYS_MAX_DISTANCE)
#define VPHYS_MAX_VELSQR		(VPHYS_MAX_VEL*VPHYS_MAX_VEL)


extern float IntervalDistance( float x, float x0, float x1 );

//----------------------------------------------------
// Clear UI for both clients - useful on transitions
//----------------------------------------------------
void ClearClientUI()
{
	CReliableBroadcastRecipientFilter filter;
	filter.AddAllPlayers();
	UserMessageBegin( filter, "ChallengeModeCloseAllUI" );
	MessageEnd();
}

//disable 'this' : used in base member initializer list
#pragma warning( disable : 4355 )

CPortal_Player::CPortal_Player()
	: m_vInputVector( 0.0f, 0.0f, 0.0f ),
	m_flCachedJumpPowerTime( -FLT_MAX ),
	m_flSpeedDecelerationTime( 0.0f ),
	m_flPredictedJumpTime( 0.f ),
	m_flUsePostTeleportationBoxTime( 0.0f ),
	m_bJumpWasPressedWhenForced( false ),
	m_bWantsToSwapGuns( false ),
	m_bSendSwapProximityFailEvent( false ),
	m_PlayerGunType( PLAYER_NO_GUN ),
	m_bSpawnFromDeath( false ),
	m_nBounceCount( 0 ),
	m_LastGroundBouncePlaneDistance( 0.0f ),
	m_flLastSuppressedBounceTime( 0 ),
	m_bIsFullyConnected( false ),
	m_pGrabSound( NULL ),
	m_nAirTauntCount( 0 ),
	m_nWheatleyMonitorDestructionCount( 0 ),
	m_bPotatos( true ),
	m_flMotionBlurAmount( -1.0f ),
	m_bIsBendy( false )
{
	// Taunt code
	m_Shared.Init( this );
	m_Shared.m_flTauntRemoveTime = 0.0f;

	m_PlayerAnimState = CreatePortalPlayerAnimState( this );

	UseClientSideAnimation();

	m_angEyeAngles.Init();

	m_iLastWeaponFireUsercmd = 0;

	m_iSpawnInterpCounter = 0;

	m_bHeldObjectOnOppositeSideOfPortal = false;

	m_bIntersectingPortalPlane = false;

	m_bPitchReorientation = false;

	m_bSilentDropAndPickup = false;

	m_bClientCheckPVSDirty = false;

	m_flUseKeyCooldownTime = 0.0f;
	m_hGrabbedEntity = NULL;
	m_flLastPingTime = 0.0f;
	m_hPortalThroughWhichGrabOccured = NULL;

	m_ForcedGrabController = FORCE_GRAB_CONTROLLER_DEFAULT;

#if USE_SLOWTIME
	m_bHasPlayedSlowTimeStopSound = true;
#endif // USE_SLOWTIME

	m_flImplicitVerticalStepSpeed = 0.0f;

	m_flTimeSinceLastTouchedPower[0] = FLT_MAX;
	m_flTimeSinceLastTouchedPower[1] = FLT_MAX;
	m_flTimeSinceLastTouchedPower[2] = FLT_MAX;

	m_flHullHeight = GetHullHeight();

	m_EntityPortalledNetworkMessages.SetCount( MAX_ENTITY_PORTALLED_NETWORK_MESSAGES );
	m_PlayerGunTypeWhenDead = PLAYER_NO_GUN;

	m_bReadyForDLCItemUpdates = false;
}

CPortal_Player::~CPortal_Player( void )
{
#ifdef PORTAL2
	if ( GameRules() && GameRules()->IsMultiplayer() && !IsSplitScreenPlayer() )
	{
		CPortal_Player *pOtherPlayer = ToPortalPlayer( UTIL_OtherPlayer( this ) );
		if ( pOtherPlayer )
		{
			pOtherPlayer->RemovePictureInPicturePlayer( this );
		}
	}
#endif

	ClearSceneEvents( NULL, true );

	if ( m_PlayerAnimState )
		m_PlayerAnimState->Release();

	CPortalRagdoll *pRagdoll = dynamic_cast<CPortalRagdoll*>( m_hRagdoll.Get() );
	if( pRagdoll )
	{
		UTIL_Remove( pRagdoll );
	}
}

CEG_NOINLINE CPortal_Player *CPortal_Player::CreatePlayer( const char *className, edict_t *ed )
{
	CPortal_Player::s_PlayerEdict = ed;
	return (CPortal_Player*)CreateEntityByName( className );
}

CEG_PROTECT_STATIC_MEMBER_FUNCTION( CPortal_Player_CreatePlayer, CPortal_Player::CreatePlayer );

void CPortal_Player::UpdateOnRemove( void )
{
#if USE_SLOWTIME
	if ( m_pSlowTimeColorFX )
	{
		UTIL_Remove( m_pSlowTimeColorFX );
		m_pSlowTimeColorFX = NULL;
	}
#endif // USE_SLOWTIME

#if !defined(NO_STEAM) && !defined( NO_STEAM_GAMECOORDINATOR )
	m_Inventory.RemoveListener( this );
#endif

	BaseClass::UpdateOnRemove();
}


#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

//-----------------------------------------------------------------------------
// Purpose: Request this player's inventories from the steam backend
//-----------------------------------------------------------------------------
void CPortal_Player::UpdateInventory( bool bInit )
{
	if ( IsFakeClient() )
		return;

	if ( bInit )
	{
		if ( steamgameserverapicontext->SteamGameServer() )
		{
			CSteamID steamIDForPlayer;
			if ( GetSteamID( &steamIDForPlayer ) )
			{
				PortalInventoryManager()->SteamRequestInventory( &m_Inventory, steamIDForPlayer, this );
			}
		}
	}

	// If we have an SOCache, we've got a connection to the GC
	bool bInvalid = true;
	if ( m_Inventory.GetSOC() )
	{
		bInvalid = m_Inventory.GetSOC()->BIsInitialized() == false;
	}
	m_Shared.SetLoadoutUnavailable( bInvalid );
}

//-----------------------------------------------------------------------------
// Purpose: Steam has just notified us that the player changed his inventory
//-----------------------------------------------------------------------------
void CPortal_Player::InventoryUpdated( CPlayerInventory *pInventory )
{
	m_Shared.SetLoadoutUnavailable( false );

	// Make sure we're wearing the right skin.
	SetPlayerModel();

	if ( m_bReadyForDLCItemUpdates )
	{
		bool bMultiplayer = g_pGameRules->IsMultiplayer();
		bool bIs2GunsMap = ( V_stristr( gpGlobals->mapname.ToCStr(), "2guns" ) != NULL ) || ( GlobalEntity_GetState( "paintgun_map" ) == GLOBAL_ON );
		if ( !bMultiplayer || !bIs2GunsMap )
		{
			GiveDefaultItems();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Requests that the GC confirm that this player is supposed to have
//			an SO cache on this gameserver and send it again if so.
//-----------------------------------------------------------------------------
void CPortal_Player::VerifySOCache()
{
	if ( IsFakeClient() )
		return;

	CSteamID steamIDForPlayer;
	GetSteamID( &steamIDForPlayer );

	if( steamIDForPlayer.BIndividualAccount() )
	{
		// if we didn't find an inventory ask the GC to refresh us
		GCSDK::CGCMsg<MsgGCVerifyCacheSubscription_t> msgVerifyCache( k_EMsgGCVerifyCacheSubscription );
		msgVerifyCache.Body().m_ulSteamID = steamIDForPlayer.ConvertToUint64();
		GCClientSystem()->BSendMessage( msgVerifyCache );
	}
	else
	{
		Msg( "Cannot verify load for invalid steam ID %s\n", steamIDForPlayer.Render() );
	}
}

CEconItemView *CPortal_Player::GetItemInLoadoutSlot( int iLoadoutSlot )
{
	// Portal players just instantly equip things.
	int iBot = ( GetTeamNumber() == TEAM_BLUE ) ? P2BOT_ATLAS : P2BOT_PBODY;
	return m_Inventory.GetItemInLoadout( iBot, iLoadoutSlot );
}

#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )


void CPortal_Player::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "PortalPlayer.EnterPortal" );
	PrecacheScriptSound( "PortalPlayer.ExitPortal" );

	PrecacheScriptSound( "PortalPlayer.Woosh" );
	PrecacheScriptSound( "PortalPlayer.FallRecover" );

	PrecacheScriptSound( "PortalPlayer.ObjectUse" );
	PrecacheScriptSound( "PortalPlayer.UseDeny" );

	PrecacheScriptSound( "PortalPlayer.ObjectUseNoGun" );
	PrecacheScriptSound( "PortalPlayer.UseDenyNoGun" );

	PrecacheScriptSound( "JumpLand.HighVelocityImpactCeiling" );
	PrecacheScriptSound( "JumpLand.HighVelocityImpact" );

#if USE_SLOWTIME
	// Slow time
	PrecacheScriptSound( "Player.SlowTime_Start" );
	PrecacheScriptSound( "Player.SlowTime_Loop" );
	PrecacheScriptSound( "Player.SlowTime_Stop" );
#endif // USE_SLOWTIME

	// Precache based on our game type
	if ( GameRules()->IsMultiplayer() )
	{
		PrecacheParticleSystem( COOP_PING_PARTICLE_NAME );
		PrecacheParticleSystem( "command_target_ping_just_arrows" );
		PrecacheParticleSystem( "robot_point_beam" );
		PrecacheScriptSound( COOP_PING_SOUNDSCRIPT_NAME );
		UTIL_PrecacheDecal( COOP_PING_DECAL_NAME );

		// Player models
		PrecacheModel( GetBallBotModel() );
		PrecacheModel( g_pszBallBotAnimations );
		PrecacheModel( GetEggBotModel() );
		PrecacheModel( g_pszEggBotAnimations );

		PrecacheScriptSound( "CoopBot.WallSlam" );
		PrecacheScriptSound( "CoopBot.Explode_Gib" );
		PrecacheScriptSound( "CoopBot.CoopBotBulletImpact" );

		int iModelIndex = PrecacheModel( GetBallBotModel() );
		PrecacheGibsForModel( iModelIndex );

		iModelIndex = PrecacheModel( GetEggBotModel() );
		PrecacheGibsForModel( iModelIndex );

		PrecacheModel( g_pszBallBotHelmetModel );
		PrecacheModel( g_pszEggBotHelmetModel );
		PrecacheModel( g_pszBallBotAntennaModel );
		PrecacheModel( g_pszEggBotAntennaModel );
	}
	else
	{
		PrecacheModel( g_pszPlayerModel );
		PrecacheModel( g_pszPlayerAnimations );
	}

	// paint effect
	PrecacheParticleSystem( "boomer_vomit_screeneffect" );
	PrecacheParticleSystem( "boomer_vomit_survivor" );

	// paint sound
	PrecacheScriptSound( "Player.JumpPowerUse" );
	PrecacheScriptSound( "Player.EnterBouncePaint" );
	PrecacheScriptSound( "Player.ExitBouncePaint" );
	PrecacheScriptSound( "Player.EnterSpeedPaint" );
	PrecacheScriptSound( "Player.ExitSpeedPaint" );
	PrecacheScriptSound( "Player.EnterStickPaint" );
	PrecacheScriptSound( "Player.ExitStickPaint" );

	PrecacheParticleSystem( "electrical_arc_01" );
}

void CPortal_Player::CreateSounds()
{
	if ( !m_pWooshSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter( this );

		m_pWooshSound = controller.SoundCreate( filter, entindex(), "PortalPlayer.Woosh" );
		controller.Play( m_pWooshSound, 0, 100 );
	}
}

void CPortal_Player::StopLoopingSounds()
{
	if ( m_pWooshSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundDestroy( m_pWooshSound );
		m_pWooshSound = NULL;
	}

	BaseClass::StopLoopingSounds();
}

void CPortal_Player::GiveAllItems( void )
{
	CWeaponPortalgun *pPortalGun = static_cast<CWeaponPortalgun*>( GiveNamedItem( "weapon_portalgun" ) );

	if ( !pPortalGun )
	{
		pPortalGun = static_cast<CWeaponPortalgun*>( Weapon_OwnsThisType( "weapon_portalgun" ) );
	}

	if ( pPortalGun )
	{
		pPortalGun->SetCanFirePortal1();
		pPortalGun->SetCanFirePortal2();
	}
}

void CPortal_Player::GiveDefaultItems( void )
{
	if ( GameRules()->IsMultiplayer() )
	{
		if ( PortalMPGameRules() && !PortalMPGameRules()->SupressSpawnPortalgun( GetTeamNumber() ) )
		{
			// Give the player an upgraded portal gun.
			if ( !Weapon_OwnsThisType("weapon_portalgun", 0) )
			{
				CWeaponPortalgun *pPortalGun = (CWeaponPortalgun *)CreateEntityByName("weapon_portalgun");
				if ( pPortalGun != NULL )
				{
					pPortalGun->SetLocalOrigin( GetLocalOrigin() );
					pPortalGun->AddSpawnFlags( SF_NORESPAWN );
					pPortalGun->SetSubType( 0 );

					DispatchSpawn( pPortalGun );

					if ( !pPortalGun->IsMarkedForDeletion() )
					{
						pPortalGun->SetCanFirePortal1();
						pPortalGun->SetCanFirePortal2();

						Weapon_Equip( pPortalGun );
					}
				}
			}
		}

		if ( g_nPortal2PromoFlags & PORTAL2_PROMO_HELMETS )
		{
			// Don't give me a rollcage if I have a hat equipped
			bool bHasHeadgearEquipped = false;

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
 			CEconItemView *pItem = GetItemInLoadoutSlot( LOADOUT_POSITION_HEAD );
 			bHasHeadgearEquipped = ( pItem && pItem->IsValid() );
#endif

 			if ( !bHasHeadgearEquipped )
			{
				GivePlayerWearable( GetTeamNumber() == TEAM_BLUE ? "weapon_promo_helmet_ball" : "weapon_promo_helmet_egg" );
			}
			else
			{
				RemovePlayerWearable( GetTeamNumber() == TEAM_BLUE ? "weapon_promo_helmet_ball" : "weapon_promo_helmet_egg" );
			}
		}

		if ( g_nPortal2PromoFlags & PORTAL2_PROMO_ANTENNA )
		{
			bool bHasFlagEquipped = false;

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
			// Don't give me an antenna if I have a flag equipped
 			CEconItemView *pItem = GetItemInLoadoutSlot( LOADOUT_POSITION_MISC );
 			if ( pItem && pItem->IsValid() )
 			{
 				if ( pItem->GetStaticData() && pItem->GetStaticData()->GetItemTypeName() )
 				{
 					bHasFlagEquipped = Q_stricmp( pItem->GetStaticData()->GetItemTypeName(), "#P2_WearableType_Flag" ) == 0;
 				}
 			}
#endif

 			if ( !bHasFlagEquipped )
			{
				GivePlayerWearable( GetTeamNumber() == TEAM_BLUE ? "weapon_promo_antenna_ball" : "weapon_promo_antenna_egg" );
			}
			else
			{
				RemovePlayerWearable( GetTeamNumber() == TEAM_BLUE ? "weapon_promo_antenna_ball" : "weapon_promo_antenna_egg" );
			}
		}
	}

	m_bReadyForDLCItemUpdates = true;
}

//-----------------------------------------------------------------------------
// Purpose: Sets  specific defaults.
//-----------------------------------------------------------------------------
void CPortal_Player::Spawn(void)
{
	Precache();

	if( g_pGameRules->IsMultiplayer() )
	{
		switch( GetTeamNumber() )
		{
		case TEAM_UNASSIGNED:
		//case TEAM_SPECTATOR:
			PickTeam();
		}
	}

	SetPlayerModel();

	BaseClass::Spawn();

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	// Check the make sure we have our inventory each time we spawn
	UpdateInventory( false );

	if( m_Shared.IsLoadoutUnavailable() )
	{
		VerifySOCache();
	}
#endif

	// For the ratings, we don't need to bleed -- jdw
	// WE AINT GOT TIME TO BLEEED - mtw
	// Needed in Spawn for MP and Activate for SP
	SetBloodColor( DONT_BLEED );

	CreateSounds();

	pl.deadflag = false;
	RemoveSolidFlags( FSOLID_NOT_SOLID );

	RemoveEffects( EF_NODRAW );
	StopObserverMode();

	//GiveDefaultItems();

	m_nRenderFX = kRenderNormal;

	m_Local.m_iHideHUD = 0;

	AddFlag(FL_ONGROUND); // set the player on the ground at the start of the round.

	m_impactEnergyScale = PORTALPLAYER_PHYSDAMAGE_SCALE;

	RemoveFlag( FL_FROZEN );

	m_iSpawnInterpCounter = (m_iSpawnInterpCounter + 1) % 8;

	m_Local.m_bDucked = false;

	SetPlayerUnderwater(false);

	SetMaxSpeed( PORTAL_WALK_SPEED );

#if USE_SLOWTIME

	m_pSlowTimeColorFX = CreateEntityByName( "color_correction" );
	if ( m_pSlowTimeColorFX )
	{
		m_pSlowTimeColorFX->KeyValue( "filename", "scripts/colorcorrection/fling_color.raw" );
		m_pSlowTimeColorFX->KeyValue( "StartDisabled", "1" );
		m_pSlowTimeColorFX->KeyValue( "fadeInDuration", "0.05" );
		m_pSlowTimeColorFX->KeyValue( "fadeOutDuration", "0.1" );
		m_pSlowTimeColorFX->KeyValue( "minfalloff", "0.0" );
		m_pSlowTimeColorFX->KeyValue( "maxfalloff", "0.0" );
		m_pSlowTimeColorFX->KeyValue( "maxWeight", "1.0" );
		m_pSlowTimeColorFX->SetAbsOrigin( GetAbsOrigin() );
		m_pSlowTimeColorFX->SetParent( this );
		DispatchSpawn( m_pSlowTimeColorFX );
		m_pSlowTimeColorFX->Activate();
	}

#endif // USE_SLOWTIME

	SetMaxSpeed( sv_speed_normal.GetFloat() );

	m_vPrevGroundNormal = Vector(0,0,1);
	m_PortalLocal.m_PaintedPowerTimer.Invalidate();

	GivePortalPlayerItems();

	// Clear out taunt state on respawn
	m_bTauntRemoteView = false;
	m_hRemoteTauntCamera = NULL;
	m_nTeamTauntState = TEAM_TAUNT_NONE;
	m_bTrickFire = false;
	m_hTauntPartnerInRange = NULL;

	m_iSpawnCounter = !m_iSpawnCounter;

	// clear animation state
	m_PlayerAnimState->ClearAnimationState();

	if ( GameRules() && GameRules()->IsMultiplayer() )
	{
		bool bIsBlue = GetTeamNumber() == TEAM_BLUE;
		if ( IsFullyConnected() )
		{
			if ( bIsBlue )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "player_spawn_blue" );
				if ( event )
				{
					gameeventmanager->FireEvent( event );
				}
			}
			else
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "player_spawn_orange" );
				if ( event )
				{
					gameeventmanager->FireEvent( event );
				}
			}
		}
		else if ( !PortalMPGameRules()->IsPlayerDataReceived( 0 ) || !PortalMPGameRules()->IsPlayerDataReceived( 1 ) )
		{
			if ( !engine->GetSplitScreenPlayerAttachToEdict( 1 ) && !engine->GetSplitScreenPlayerAttachToEdict( 2 ) )
			{
				if ( bIsBlue )
				{
					engine->ClientCommand( edict(), "playvideo_end_level_transition coop_bluebot_load 1" );
				}
				else
				{
					engine->ClientCommand( edict(), "playvideo_end_level_transition coop_orangebot_load 1" );
				}
			}
		}
	}

	// Want to render the player models in the world imposter views and water views.
	AddEffects( EF_MARKED_FOR_FAST_REFLECTION );
	AddEffects( EF_SHADOWDEPTH_NOCACHE );

	// reset was dropped state
	m_bWasDroppedByOtherPlayerWhileTaunting = false;

	// Reset bounce count
	m_nBounceCount = 0;
	m_LastGroundBouncePlaneDistance = 0.0f;

	// remove conds and reset PIP
	m_Shared.RemoveAllCond();

	// init prev position
	m_vPrevPosition = GetAbsOrigin();

#if !defined( _GAMECONSOLE )
	g_Portal2ResearchDataTracker.SetPlayerName( this );
#endif // !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
}

void CPortal_Player::Activate( void )
{
	BaseClass::Activate();

	// For the ratings, we don't need to bleed -- jdw
	// WE AINT GOT TIME TO BLEEED - mtw
	// Needed in Spawn for MP and Activate for SP
	SetBloodColor( DONT_BLEED );

	m_fTimeLastNumSecondsUpdate = gpGlobals->curtime;

	SetMaxSpeed( sv_speed_normal.GetFloat() );

	// Turn off PIP for all players as a new level starts
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			engine->ClientCommand( pToPlayer->edict(), "-remote_view" );
		}
	}

	if ( GetModelPtr() )
	{
		ParseScriptedInteractions();
	}

	// Let's kill the player!
	if ( playtest_random_death.GetBool() )
	{
		flNextDeathTime = gpGlobals->curtime + random->RandomFloat( 0.5f*60.0f, 2*60.0f );
	}
}

void CPortal_Player::OnFullyConnected()
{
	// Don't worry about waiting for the other player in dev 0
	if ( GameRules()->IsMultiplayer() )
	{
		// Waiting for the other player
		m_takedamage = DAMAGE_NO;
		pl.deadflag = true;
		m_lifeState = LIFE_DEAD;
		SetMoveType( MOVETYPE_NONE );

		// Set any splitscreen players associated as waiting too
		Assert( GetSplitScreenPlayers().Count() == 0 || GetSplitScreenPlayers().Count() == 1 );

		// Respawn
		SetThink( &CPortal_Player::PlayerTransitionCompleteThink );
		SetNextThink( gpGlobals->curtime + 1.0f );

		if ( !PortalMPGameRules()->IsPlayerDataReceived( 0 ) || !PortalMPGameRules()->IsPlayerDataReceived( 1 ) )
		{
			bool bIsCommunityCoopHub = PortalMPGameRules()->IsCommunityCoopHub();
			float flOtherPlayerTimeout = bIsCommunityCoopHub ? 0.0f : mp_wait_for_other_player_timeout.GetFloat();
			float flNotConnectingTimeout = bIsCommunityCoopHub ? 0.0f : mp_wait_for_other_player_notconnecting_timeout.GetFloat();
			// Timeout and spawn eventually if the other player doesn't connect
			SetNextThink( gpGlobals->curtime + flOtherPlayerTimeout );	// Wait 40 seconds for other players to connect

			SetContextThink( &CPortal_Player::PlayerCatchPatnerNotConnectingThink, gpGlobals->curtime + flNotConnectingTimeout, CATCHPATNERNOTCONNECTING_THINK_CONTEXT );
		}

		// Self is not in this list. With 1 splitscreen partner this list has 1 player
		for ( int i = 0; i < GetSplitScreenPlayers().Count(); ++i )
		{
			CPortal_Player *pPlayer = static_cast< CPortal_Player* >( GetSplitScreenPlayers()[ i ].Get() );
			if ( pPlayer )
			{
				pPlayer->OnFullyConnected();
			}
		}
	}
	else
	{
		// Single player just needs to shut down the transition video
		const char *szVideoCommand = "stopvideos_fadeout";
		char szClientCmd[256];
		Q_snprintf( szClientCmd, sizeof(szClientCmd), "%s %f\n", szVideoCommand, 1.5f );
		engine->ClientCommand( edict(), szClientCmd );

		ChallengePlayersReady();
	}

	m_bIsFullyConnected = true;
}

void CPortal_Player::NotifySystemEvent(CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params )
{
	// On teleport, we send event for tracking fling achievements
	if ( eventType == NOTIFY_EVENT_TELEPORT )
	{
		CProp_Portal *pEnteredPortal = dynamic_cast<CProp_Portal*>( pNotify );
		IGameEvent *event = gameeventmanager->CreateEvent( "portal_player_portaled" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetBool( "portal2", pEnteredPortal->m_bIsPortal2 );
			gameeventmanager->FireEvent( event );
		}
	}

	BaseClass::NotifySystemEvent( pNotify, eventType, params );
}

void CPortal_Player::OnSave( IEntitySaveUtils *pUtils )
{
	char const *pchSaveFile = engine->GetSaveFileName();
	bool bIsQuicksave = V_stricmp( pchSaveFile, "SAVE\\quick.sav" ) == 0;
	bool bIsAutosave = V_stricmp( pchSaveFile, "SAVE\\autosave.sav" ) == 0 || V_stricmp( pchSaveFile, "SAVE\\autosavedangerous.sav" ) == 0;
	if ( bIsAutosave || bIsQuicksave )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( bIsQuicksave ? "quicksave" : "autosave" );
		if ( event )
		{
			gameeventmanager->FireEvent( event );
		}
	}

	BaseClass::OnSave( pUtils );
}

void CPortal_Player::OnRestore( void )
{
	BaseClass::OnRestore();
	// HACK: Designers have added the ability to override the type of grabcontroller...
	// these changes go across transitions if somebody forgets to change it back to default
	// and are generating bugs. By request, we're reverting the state to default after
	// each level transition in case some entity io fails to change it back.
	if ( gpGlobals->eLoadType == MapLoad_Transition )
	{
		SetForcedGrabControllerType( FORCE_GRAB_CONTROLLER_DEFAULT );
	}

	// Saving is not allowed and they loaded from a save.  Kill the player to prevent
	// bogus score in challenge mode.

	if ( GetBonusChallenge() != 0 )
	{
		// Make sure god mode is off
		RemoveFlag( FL_GODMODE );
		// Murder
		CTakeDamageInfo info(NULL, this, FLT_MAX, 0 );
		TakeDamage( info );

		// Force cheats on to make sure their score won't get recorded
		sv_cheats->SetValue( true );
	}
}

//bool CPortal_Player::StartObserverMode( int mode )
//{
//	//Do nothing.
//
//	return false;
//}

void CPortal_Player::ClearScriptedInteractions( void )
{
	m_ScriptedInteractions.RemoveAll();
}

void CPortal_Player::ParseScriptedInteractions( void )
{
	// Already parsed them?
	if ( m_ScriptedInteractions.Count() )
		return;

	// Parse the model's key values and find any dynamic interactions
	KeyValues *modelKeyValues = new KeyValues("");
	CUtlBuffer buf( 1024, 0, CUtlBuffer::TEXT_BUFFER );
	KeyValues::AutoDelete autodelete_key( modelKeyValues );

	if (! modelinfo->GetModelKeyValue( GetModel(), buf ))
		return;

	if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), buf ) )
	{
		// Do we have a dynamic interactions section?
		KeyValues *pkvInteractions = modelKeyValues->FindKey("dynamic_interactions");
		if ( pkvInteractions )
		{
			KeyValues *pkvNode = pkvInteractions->GetFirstSubKey();
			while ( pkvNode )
			{
				ScriptedNPCInteraction_t sInteraction;
				sInteraction.iszInteractionName = AllocPooledString( pkvNode->GetName() );

				// Trigger method
				const char *pszKeyString = pkvNode->GetString( "trigger", NULL );
				if ( pszKeyString )
				{
					if ( !Q_strncmp( pszKeyString, "auto_in_combat", 14) )
					{
						sInteraction.iTriggerMethod = SNPCINT_AUTOMATIC_IN_COMBAT;
					}
				}

				// Loop Break trigger method
				pszKeyString = pkvNode->GetString( "loop_break_trigger", NULL );
				if ( pszKeyString )
				{
					char szTrigger[256];
					Q_strncpy( szTrigger, pszKeyString, sizeof(szTrigger) );
					char *pszParam = strtok( szTrigger, " " );
					while (pszParam)
					{
						if ( !Q_strncmp( pszParam, "on_damage", 9) )
						{
							sInteraction.iLoopBreakTriggerMethod |= SNPCINT_LOOPBREAK_ON_DAMAGE;
						}
						if ( !Q_strncmp( pszParam, "on_flashlight_illum", 19) )
						{
							sInteraction.iLoopBreakTriggerMethod |= SNPCINT_LOOPBREAK_ON_FLASHLIGHT_ILLUM;
						}

						pszParam = strtok(NULL," ");
					}
				}

				// Origin
				pszKeyString = pkvNode->GetString( "origin_relative", "0 0 0" );
				UTIL_StringToVector( sInteraction.vecRelativeOrigin.Base(), pszKeyString );

				// Angles
				pszKeyString = pkvNode->GetString( "angles_relative", NULL );
				if ( pszKeyString )
				{
					sInteraction.iFlags |= SCNPC_FLAG_TEST_OTHER_ANGLES;
					UTIL_StringToVector( sInteraction.angRelativeAngles.Base(), pszKeyString );
				}

				// Velocity
				pszKeyString = pkvNode->GetString( "velocity_relative", NULL );
				if ( pszKeyString )
				{
					sInteraction.iFlags |= SCNPC_FLAG_TEST_OTHER_VELOCITY;
					UTIL_StringToVector( sInteraction.vecRelativeVelocity.Base(), pszKeyString );
				}

				// Camera Distance
				sInteraction.flCameraDistance = pkvNode->GetFloat( "distance_camera", portal_tauntcam_dist.GetFloat() );

				// Camera Angles
				pszKeyString = pkvNode->GetString( "angles_camera", NULL );
				if ( pszKeyString )
				{
					UTIL_StringToVector( sInteraction.angCameraAngles.Base(), pszKeyString );
				}
				else
				{
					sInteraction.angCameraAngles[ PITCH ] = 20.0f;
					sInteraction.angCameraAngles[ YAW ] = 160.0f;
					sInteraction.angCameraAngles[ ROLL ] = 0.0f;
				}

				// Entry Sequence
				pszKeyString = pkvNode->GetString( "entry_sequence", NULL );
				if ( pszKeyString )
				{
					sInteraction.sPhases[SNPCINT_ENTRY].iszSequence = AllocPooledString( pszKeyString );
				}
				// Entry Activity
				pszKeyString = pkvNode->GetString( "entry_activity", NULL );
				if ( pszKeyString )
				{
					sInteraction.sPhases[SNPCINT_ENTRY].iActivity = ACT_INVALID;
					DevWarning( "Activities not supported for player scripted sequences." );
				}

				// Sequence
				pszKeyString = pkvNode->GetString( "sequence", NULL );
				if ( pszKeyString )
				{
					sInteraction.sPhases[SNPCINT_SEQUENCE].iszSequence = AllocPooledString( pszKeyString );
				}

				// Activity
				pszKeyString = pkvNode->GetString( "activity", NULL );
				if ( pszKeyString )
				{
					sInteraction.sPhases[SNPCINT_SEQUENCE].iActivity = ACT_INVALID;
					DevWarning( "Activities not supported for player scripted sequences." );
				}

				// Exit Sequence
				pszKeyString = pkvNode->GetString( "exit_sequence", NULL );
				if ( pszKeyString )
				{
					sInteraction.sPhases[SNPCINT_EXIT].iszSequence = AllocPooledString( pszKeyString );
				}
				// Exit Activity
				pszKeyString = pkvNode->GetString( "exit_activity", NULL );
				if ( pszKeyString )
				{
					sInteraction.sPhases[SNPCINT_EXIT].iActivity = ACT_INVALID;
					DevWarning( "Activities not supported for player scripted sequences." );
				}

				// Delay
				sInteraction.flDelay = pkvNode->GetFloat( "delay", 10.0 );

				// Delta
				sInteraction.flDistSqr = pkvNode->GetFloat( "origin_max_delta", (DSS_MAX_DIST * DSS_MAX_DIST) );

				// Loop?
				if ( pkvNode->GetFloat( "loop_in_action", 0 ) )
				{
					sInteraction.iFlags |= SCNPC_FLAG_LOOP_IN_ACTION;
				}

				// Fixup position?
				pszKeyString = pkvNode->GetString( "dont_teleport_at_end", NULL );
				if ( pszKeyString )
				{
					if ( !Q_stricmp( pszKeyString, "me" ) || !Q_stricmp( pszKeyString, "both" ) )
					{
						sInteraction.iFlags |= SCNPC_FLAG_DONT_TELEPORT_AT_END_ME;
					}
					else if ( !Q_stricmp( pszKeyString, "them" ) || !Q_stricmp( pszKeyString, "both" ) )
					{
						sInteraction.iFlags |= SCNPC_FLAG_DONT_TELEPORT_AT_END_THEM;
					}
				}

				// Needs a weapon?
				pszKeyString = pkvNode->GetString( "needs_weapon", NULL );
				if ( pszKeyString )
				{
					if ( !Q_strncmp( pszKeyString, "ME", 2 ) )
					{
						sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_ME;
					}
					else if ( !Q_strncmp( pszKeyString, "THEM", 4 ) )
					{
						sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_THEM;
					}
					else if ( !Q_strncmp( pszKeyString, "BOTH", 4 ) )
					{
						sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_ME;
						sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_THEM;
					}
				}

				// Specific weapon types
				pszKeyString = pkvNode->GetString( "weapon_mine", NULL );
				if ( pszKeyString )
				{
					sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_ME;
					sInteraction.iszMyWeapon = AllocPooledString( pszKeyString );
				}
				pszKeyString = pkvNode->GetString( "weapon_theirs", NULL );
				if ( pszKeyString )
				{
					sInteraction.iFlags |= SCNPC_FLAG_NEEDS_WEAPON_THEM;
					sInteraction.iszTheirWeapon = AllocPooledString( pszKeyString );
				}

				// Add it to the list
				AddScriptedInteraction( &sInteraction );

				// Move to next interaction
				pkvNode = pkvNode->GetNextKey();
			}
		}
	}
}

void CPortal_Player::AddScriptedInteraction( ScriptedNPCInteraction_t *pInteraction )
{
	int nNewIndex = m_ScriptedInteractions.AddToTail();

	if ( ai_debug_dyninteractions.GetBool() )
	{
		Msg("%s(%s): Added dynamic interaction: %s\n", GetClassname(), GetDebugName(), STRING(pInteraction->iszInteractionName) );
	}

	// Copy the interaction over
	ScriptedNPCInteraction_t *pNewInt = &(m_ScriptedInteractions[nNewIndex]);
	memcpy( pNewInt, pInteraction, sizeof(ScriptedNPCInteraction_t) );

	// Calculate the local to world matrix
	m_ScriptedInteractions[nNewIndex].matDesiredLocalToWorld.SetupMatrixOrgAngles( pInteraction->vecRelativeOrigin, pInteraction->angRelativeAngles );
}

void CPortal_Player::FireConcept( const char *pConcept )
{
	// Since the player doesn't really speak we are just shortcutting this and having the sphere speak these lines directly.
	CAI_BaseActor *pSphere = dynamic_cast<CAI_BaseActor*>( gEntList.FindEntityByClassname( NULL, "npc_personality_core" ) );

	if ( pSphere == NULL )
		return;

	pSphere->Speak( pConcept );
}

void CPortal_Player::SetTeamTauntState( int nTeamTauntState )
{
	if ( m_nTeamTauntState == nTeamTauntState )
		return;

	m_nTeamTauntState = nTeamTauntState;
}

bool CPortal_Player::ValidatePlayerModel( const char *pModel )
{
	if ( !Q_stricmp( GetPlayerModelName(), pModel ) )
		return true;

	if ( !Q_stricmp( g_pszPlayerModel, pModel ) )
		return true;

	return false;
}

void CPortal_Player::SetPlayerModel( void )
{
	const char *szModelName = NULL;
	const char *pszCurrentModelName = modelinfo->GetModelName( GetModel());

	szModelName = engine->GetClientConVarValue( entindex(), "cl_playermodel" );

	if ( ValidatePlayerModel( szModelName ) == false )
	{
		char szReturnString[512];

		if ( ValidatePlayerModel( pszCurrentModelName ) == false )
		{
			pszCurrentModelName = GetPlayerModelName();
		}

		Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", pszCurrentModelName );
		engine->ClientCommand ( edict(), szReturnString );

		szModelName = pszCurrentModelName;
	}

	int modelIndex = modelinfo->GetModelIndex( szModelName );

	if ( modelIndex == -1 )
	{
		szModelName = GetPlayerModelName();

		char szReturnString[512];

		Q_snprintf( szReturnString, sizeof (szReturnString ), "cl_playermodel %s\n", szModelName );
		engine->ClientCommand ( edict(), szReturnString );
	}

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );
	PrecacheModel( GetPlayerModelName() );
	CBaseEntity::SetAllowPrecache( allowPrecache );

	SetModel( GetPlayerModelName() );

	if ( GameRules()->IsMultiplayer() )
	{
		if ( g_nPortal2PromoFlags & PORTAL2_PROMO_SKINS )
		{
			m_nSkin = 1;
		}
		else
		{
			m_nSkin = 0;
		}
	}

	m_iPlayerSoundType.Set( PLAYER_SOUNDS_CITIZEN );
}


bool CPortal_Player::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex )
{
	bool bRet = BaseClass::Weapon_Switch( pWeapon, viewmodelindex );

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Play a one-shot scene
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CPortal_Player::PlayScene( const char *pszScene, float flDelay, AI_Response *response, IRecipientFilter *filter )
{
	MDLCACHE_CRITICAL_SECTION();

	float flDuration = InstancedScriptedScene( this, pszScene, NULL, flDelay, false, response, true, filter );
	m_Shared.m_flTauntRemoveTime = gpGlobals->curtime + flDuration + 0.5f;
	m_Shared.AddCond( PORTAL_COND_TAUNTING );

	if ( V_strstr( pszScene, "_idle" ) != NULL )
	{
		if ( m_nTeamTauntState < TEAM_TAUNT_NEED_PARTNER )
		{
			SetTeamTauntState( TEAM_TAUNT_NEED_PARTNER );
		}
	}

	// Fire off achievements for any taunts and keep track of the number of times we do it
	if ( GameRules()->IsMultiplayer() )
	{
		CPortalMPStats *pStats = GetPortalMPStats();
		if ( response && response->m_szMatchingRule && pStats )
		{
			bool bHelmetOpener = false;

			// These match the exact name of the rule we want to fire achievements based on minus ballbot/eggbot prefix
			if ( V_strstr( response->m_szMatchingRule, "teamgesturehighfive_success") != NULL )
			{
				UTIL_RecordAchievementEvent( "ACH.TAUNTS[1]", this );
				pStats->IncrementPlayerTauntsUsedMap( this, TAUNT_HIGHFIVE );
			}
			else if ( V_strstr( response->m_szMatchingRule, "gesturesmallwave") != NULL ||
					V_strstr( response->m_szMatchingRule, "gestureportalgunsmallwave") != NULL )
			{
				UTIL_RecordAchievementEvent( "ACH.TAUNTS[2]", this );
				pStats->IncrementPlayerTauntsUsedMap( this, TAUNT_WAVE );
			}
			else if ( V_strstr( response->m_szMatchingRule, "teamgesturerps_success") != NULL )
			{
				UTIL_RecordAchievementEvent( "ACH.TAUNTS[3]", this );
				pStats->IncrementPlayerTauntsUsedMap( this, TAUNT_RPS );
			}
			else if ( V_strstr( response->m_szMatchingRule, "gesturelaugh") != NULL )
			{
				UTIL_RecordAchievementEvent( "ACH.TAUNTS[4]", this );
				pStats->IncrementPlayerTauntsUsedMap( this, TAUNT_LAUGH );
			}
			else if ( V_strstr( response->m_szMatchingRule, "gesturerobotdance") != NULL )
			{
				UTIL_RecordAchievementEvent( "ACH.TAUNTS[5]", this );
				pStats->IncrementPlayerTauntsUsedMap( this, TAUNT_ROBOTDANCE );
			}
			else if ( V_strstr( response->m_szMatchingRule, "teamgestureteamhug_success") != NULL )
			{
				UTIL_RecordAchievementEvent( "ACH.TAUNTS[7]", this );
				pStats->IncrementPlayerTauntsUsedMap( this, TAUNT_HUG );
			}
			else if ( V_strstr( response->m_szMatchingRule, "gesturetrickfire") != NULL )
			{
				UTIL_RecordAchievementEvent( "ACH.TAUNTS[8]", this );
				m_bTrickFire = true;
				pStats->IncrementPlayerTauntsUsedMap( this, TAUNT_TRICKFIRE );
			}
			else if ( V_strstr( response->m_szMatchingRule, "gesturebasketball") != NULL )
			{
				if ( GetTeamNumber() == TEAM_BLUE )
				{
					bHelmetOpener = true;
				}
			}
			else
			{
				bool bEggTease = ( V_strstr( response->m_szMatchingRule, "teamgestureteameggtease_success") != NULL );
				bool bBallTease = ( V_strstr( response->m_szMatchingRule, "teamgestureteamballtease_success") != NULL );
				if ( bEggTease || bBallTease )
				{
					UTIL_RecordAchievementEvent( "ACH.TAUNTS[6]", this );
					pStats->IncrementPlayerTauntsUsedMap( this, TAUNT_CORETEASE );

					if ( bEggTease && GetTeamNumber() == TEAM_BLUE )
					{
						bHelmetOpener = true;
					}
				}
			}

			if ( bHelmetOpener )
			{
				CBaseCombatWeapon *pWeapon = Weapon_OwnsThisType( "weapon_promo_helmet_ball", 0 );
				if ( pWeapon )
				{
					pWeapon->ResetSequence( pWeapon->LookupSequence( "taunt_teamEggTease" ) );
				}
			}
		}

		if ( ( V_strstr( pszScene, "rps") != NULL ) && ( V_strstr( pszScene, "win") != NULL ) )
		{
			PortalMPGameRules()->PlayerWinRPS( this );
		}
	}

	return flDuration;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#if USE_SLOWTIME

void CPortal_Player::StartSlowingTime( float flDuration )
{
	if( g_pGameRules->IsMultiplayer() )
		return; //no slow time in multiplayer


	IGameEvent *event = gameeventmanager->CreateEvent( "slowtime" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}

	// Start up our sounds
	EmitSound( "Player.SlowTime_Start" );
	EmitSound( "Player.SlowTime_Loop" );
	m_bHasPlayedSlowTimeStopSound = false;

	m_PortalLocal.m_bSlowingTime = true;

	// Make sure we start at out max if we're already higher
	if ( m_PortalLocal.m_flSlowTimeMaximum != flDuration  )
	{
		m_PortalLocal.m_flSlowTimeMaximum = ( flDuration > 0.0f ) ? flDuration : slowtime_max.GetFloat();
		m_PortalLocal.m_flSlowTimeRemaining  = m_PortalLocal.m_flSlowTimeMaximum;
	}

	GameTimescale()->SetCurrentTimescale( slowtime_speed.GetFloat() );

	SetFOV( this, 70.0f, 0.05f );

	/*
	if ( m_pSlowTimeColorFX )
	{
		variant_t emptyVariant;
		m_pSlowTimeColorFX->AcceptInput( "Enable", this, this, emptyVariant, USE_TOGGLE );
	}
	*/

	// Reset our fire times
	CWeaponPortalgun *pPortalGun = dynamic_cast<CWeaponPortalgun*>( GetActiveWeapon() );
	if ( pPortalGun )
	{
		pPortalGun->ResetRefireTime();
	}

	FirePlayerProxyOutput( "OnStartSlowingTime", variant_t(), this, this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortal_Player::StopSlowingTime( void )
{
	m_PortalLocal.m_bSlowingTime = false;

	GameTimescale()->SetDesiredTimescale( 1.0f );

	SetFOV( this, 0.0f, 0.5f );

	/*
	if ( m_pSlowTimeColorFX )
	{
		variant_t emptyVariant;
		m_pSlowTimeColorFX->AcceptInput( "Disable", this, this, emptyVariant, USE_TOGGLE );
	}
	*/

	// Stop our looping sound
	StopSound( entindex(), CHAN_STATIC, "Player.SlowTime_Loop" );
	m_bHasPlayedSlowTimeStopSound = true;

	FirePlayerProxyOutput( "OnStopSlowingTime", variant_t(), this, this );
}

#endif // USE_SLOWTIME

void CPortal_Player::ShowViewFinder( void )
{
	if( !g_pGameRules->IsMultiplayer() )
		return;

	m_PortalLocal.m_bShowingViewFinder = true;
}

void CPortal_Player::HideViewFinder( void )
{
	m_PortalLocal.m_bShowingViewFinder = false;
}

void CPortal_Player::PlayCoopPingEffect( void )
{
	Vector vecForward;
	AngleVectors( EyeAngles(), &vecForward );
	// Hit anything they can 'see' thats directly down their crosshair
	trace_t tr;
	Ray_t ray;
	ray.Init( EyePosition(), EyePosition() + vecForward*MAX_COORD_FLOAT );
	bool bPortalBulletTrace = g_bBulletPortalTrace;
	g_bBulletPortalTrace = true;

	CTraceFilterSimpleClassnameList traceFilter( this, COLLISION_GROUP_NONE );
	traceFilter.AddClassnameToIgnore( "projected_wall_entity" );
	traceFilter.AddClassnameToIgnore( "player" );
	UTIL_Portal_TraceRay( ray, MASK_OPAQUE_AND_NPCS, &traceFilter, &tr );
	g_bBulletPortalTrace = bPortalBulletTrace;
	if ( tr.DidHit() )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "portal_player_ping" );

		if ( event )
		{
			event->SetInt("userid", GetUserID() );
			event->SetFloat("ping_x", tr.endpos.x );
			event->SetFloat("ping_y", tr.endpos.y );
			event->SetFloat("ping_z", tr.endpos.z );
			gameeventmanager->FireEvent( event );
		}

		CDisablePredictionFiltering filter(true);
		DispatchParticleEffect( COOP_PING_PARTICLE_NAME, tr.endpos, vec3_angle );
		EmitSound( COOP_PING_SOUNDSCRIPT_NAME );
		UTIL_DecalTrace( &tr, "Portal2.CoopPingDecal" );

		CReliableBroadcastRecipientFilter allplayers;
		if ( sv_portal_coop_ping_indicator_show_to_all_players.GetBool() == false )
		{
			allplayers.RemoveRecipient( this );
		}
		UserMessageBegin( allplayers, "HudPingIndicator" );
			WRITE_FLOAT( tr.endpos.x );
			WRITE_FLOAT( tr.endpos.y );
			WRITE_FLOAT( tr.endpos.z );
		MessageEnd();
	}
	else
	{
		Warning( "Attempted to ping for player, but trace failed to hit anything.\n" );
	}

	// Note this in the player proxy
	FirePlayerProxyOutput( "OnCoopPing", variant_t(), this, this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortal_Player::PreThink( void )
{
	QAngle vOldAngles = GetLocalAngles();
	QAngle vTempAngles = GetLocalAngles();

	vTempAngles = EyeAngles();

	if ( vTempAngles[PITCH] > 180.0f )
	{
		vTempAngles[PITCH] -= 360.0f;
	}

	SetLocalAngles( vTempAngles );

	// Let's kill the player!
	if ( playtest_random_death.GetBool() )
	{
		if ( flNextDeathTime < gpGlobals->curtime )
		{
			TakeDamage( CTakeDamageInfo( this, this, NULL, Vector(0,0,100), WorldSpaceCenter(), 1000.0f, DMG_CLUB ) );
			flNextDeathTime = gpGlobals->curtime + random->RandomFloat( 0.5f*60.0f, 2*60.0f );
		}
	}

	// Decay the air control
	if ( IsSuppressingAirControl() )
	{
		m_PortalLocal.m_flAirControlSupressionTime -= 1000.0f * gpGlobals->frametime;
		if ( m_PortalLocal.m_flAirControlSupressionTime < 0.0f )
		{
			m_PortalLocal.m_flAirControlSupressionTime = 0.0f;
		}
	}

	if ( m_Local.m_bSlowMovement && m_Local.m_fTBeamEndTime != 0.0f && gpGlobals->curtime > m_Local.m_fTBeamEndTime + 1.0f )
	{
		m_Local.m_bSlowMovement = false;
		SetGravity( 1.0f );

		if ( VPhysicsGetObject() )
		{
			VPhysicsGetObject()->EnableGravity( true );
		}
	}

	BaseClass::PreThink();

	if( (m_afButtonPressed & IN_JUMP) )
	{
		Jump();
		FirePlayerProxyOutput( "OnJump", variant_t(), this, this );
	}

	if( (m_afButtonPressed & IN_DUCK) )
	{
		FirePlayerProxyOutput( "OnDuck", variant_t(), this, this );
	}

	if ( m_afButtonPressed & IN_GRENADE1 )
	{
		if ( !m_PortalLocal.m_bZoomedIn )
		{
			ZoomIn();
		}
	}
	else if ( m_afButtonPressed & IN_GRENADE2 )
	{
		if ( m_PortalLocal.m_bZoomedIn )
		{
			ZoomOut();
		}
	}
	else if ( m_afButtonPressed & IN_ZOOM )
	{
		if ( !m_PortalLocal.m_bZoomedIn )
		{
			ZoomIn();
		}
		else
		{
			ZoomOut();
		}
	}

#if USE_SLOWTIME
	// Update slow time
	if ( m_PortalLocal.m_bSlowingTime )
	{
		float flDrainAmount = ( gpGlobals->frametime / 0.1f );
		m_PortalLocal.m_flSlowTimeRemaining = clamp( m_PortalLocal.m_flSlowTimeRemaining - flDrainAmount, 0.0f, slowtime_max.GetFloat() );

		if ( m_bHasPlayedSlowTimeStopSound == false )
		{
			if ( m_PortalLocal.m_flSlowTimeRemaining < 1.5f )
			{
				EmitSound( "Player.SlowTime_Stop" );
				m_bHasPlayedSlowTimeStopSound = true;
			}
		}

		if ( m_PortalLocal.m_flSlowTimeRemaining <= 0.0f )
		{
			StopSlowingTime();
		}
	}
	else
	{
		m_PortalLocal.m_flSlowTimeRemaining = clamp( m_PortalLocal.m_flSlowTimeRemaining + ( gpGlobals->frametime * slowtime_regen_per_second.GetFloat() ), 0.0f, m_PortalLocal.m_flSlowTimeMaximum );
	}

	// Modulate time!
	if ( GlobalEntity_GetState( "slowtime_disabled" ) != GLOBAL_ON )
	{
		if ( m_afButtonPressed & IN_SLOWTIME )
		{
			if ( m_PortalLocal.m_bSlowingTime )
			{
				// Turn the effect off
				StopSlowingTime();
			}
			else
			{
				if ( slowtime_must_refill.GetBool() == false  || m_PortalLocal.m_flSlowTimeRemaining >= slowtime_max.GetFloat() )
				{
					StartSlowingTime( slowtime_max.GetFloat() );
				}
				else
				{
					EmitSound( "PortalPlayer.UseDeny" );
				}
			}
		}
	}

#endif // USE_SLOWTIME

	if ( GameRules()->IsMultiplayer() )
	{
		// Send a ping
		if ( m_afButtonPressed & IN_COOP_PING )
		{
			if ( ( m_flLastPingTime + sv_portal_coop_ping_cooldown_time.GetFloat() ) < gpGlobals->curtime )
			{
				PlayCoopPingEffect();
				m_flLastPingTime = gpGlobals->curtime;
			}
		}

		m_bPingDisabled = ( GetTeamNumber() == TEAM_BLUE && GlobalEntity_GetState( "no_pinging_blue" ) == GLOBAL_ON ||
							GetTeamNumber() == TEAM_RED && GlobalEntity_GetState( "no_pinging_orange" ) == GLOBAL_ON );

		m_bTauntDisabled = ( GetTeamNumber() == TEAM_BLUE && GlobalEntity_GetState( "no_taunting_blue" ) == GLOBAL_ON ||
							 GetTeamNumber() == TEAM_RED && GlobalEntity_GetState( "no_taunting_orange" ) == GLOBAL_ON );

		if ( !m_bTauntDisabled )
		{
			CBaseEntity *pGround = GetGroundEntity();
			if ( pGround && !pGround->GetAbsVelocity().IsZero() )
			{
				m_bTauntDisabled = true;
			}
		}

		bool bHasPartnerInRange = false;

		if ( !m_bTauntDisabled )
		{
			CPortal_Player *pOtherPlayer = ToPortalPlayer( UTIL_OtherConnectedPlayer( this ) );
			if ( pOtherPlayer )
			{
				if ( pOtherPlayer->m_nTeamTauntState == TEAM_TAUNT_NEED_PARTNER )
				{
					Vector vInitiatorPos, vAcceptorPos;
					QAngle angInitiatorAng, angAcceptorAng;
					bHasPartnerInRange = ValidateTeamTaunt( pOtherPlayer, vInitiatorPos, angInitiatorAng, vAcceptorPos, angAcceptorAng );
				}

				if ( bHasPartnerInRange )
				{
					m_hTauntPartnerInRange = pOtherPlayer;
				}
			}
		}

		if ( !bHasPartnerInRange )
		{
			m_hTauntPartnerInRange = NULL;
		}
	}

	if ( GetGroundEntity() )
	{
		m_nPortalsEnteredInAirFlags = 0;
	}
	UpdateVMGrab( m_hAttachedObject );

	//Reset bullet force accumulator, only lasts one frame
	m_vecTotalBulletForce = vec3_origin;

	SetLocalAngles( vOldAngles );

	// Cache the velocity before impact
	if( engine->HasPaintmap() )
		m_PortalLocal.m_vPreUpdateVelocity = GetAbsVelocity();

	// Update the painted power
	UpdatePaintedPower();

	// Fade the input scale back in if we lost some
	UpdateAirInputScaleFadeIn();

	// Attempt to resize the hull if there's a pending hull resize
	TryToChangeCollisionBounds( m_PortalLocal.m_CachedStandHullMinAttempt,
								m_PortalLocal.m_CachedStandHullMaxAttempt,
								m_PortalLocal.m_CachedDuckHullMinAttempt,
								m_PortalLocal.m_CachedDuckHullMaxAttempt );

	// reset remote view
	if ( !m_Shared.InCond( PORTAL_COND_TAUNTING ) && m_bTauntRemoteViewFOVFixup )
	{
		m_bTauntRemoteViewFOVFixup = false;
		SetFOV( this, 0, 0.0f, 0 );
	}

	if( m_hAttachedObject && !m_pGrabSound )
	{
		CSoundEnvelopeController& controller = CSoundEnvelopeController::GetController();
		CPASAttenuationFilter filter( this );
		char const* soundName = GetActivePortalWeapon() ? "PortalPlayer.ObjectUse" : "PortalPlayer.ObjectUseNoGun";
		m_pGrabSound = controller.SoundCreate( filter, entindex(), soundName );
		controller.Play( m_pGrabSound, VOL_NORM, PITCH_NORM );
	}

	if( !m_hAttachedObject && m_pGrabSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.Shutdown( m_pGrabSound );
		controller.SoundDestroy( m_pGrabSound );
		m_pGrabSound = NULL;
	}
}

void CPortal_Player::PlayerDeathThink( void )
{
	float flForward;

	SetNextThink( gpGlobals->curtime + 0.1f );

	// wait until the crush animation is over before we respawn the player
	if ( m_Shared.InCond( PORTAL_COND_DEATH_CRUSH ) || m_Shared.InCond( PORTAL_COND_DEATH_GIB ) )
	{
		return;
	}

	if (m_lifeState == LIFE_DYING)
	{
		m_bSpawnFromDeath = true;
	}

	// Clear any painted powers
	CleansePaint();

	if (GetFlags() & FL_ONGROUND)
	{
		flForward = GetAbsVelocity().Length() - 20;
		if (flForward <= 0)
		{
			SetAbsVelocity( vec3_origin );
		}
		else
		{
			Vector vecNewVelocity = GetAbsVelocity();
			VectorNormalize( vecNewVelocity );
			vecNewVelocity *= flForward;
			SetAbsVelocity( vecNewVelocity );
		}
	}

	if ( HasWeapons() )
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		PackDeadPlayerItems();
	}

	// We're not playing death animations right now-- no need to finish the cycle.
	// If we add these to portal MP in the future, this block will let them finish before
	// the player respawns.
#if 0
	if (GetModelIndex() && (!IsSequenceFinished()) && (m_lifeState == LIFE_DYING))
	{
		StudioFrameAdvance( );

		m_iRespawnFrames++;
		if ( m_iRespawnFrames < 60 )  // animations should be no longer than this
			return;
	}
#endif

	if (m_lifeState == LIFE_DYING)
		m_lifeState = LIFE_DEAD;

	StopAnimation();

	// AddEffects( EF_NOINTERP );
	m_flPlaybackRate = 0.0;

	int fAnyButtonDown = (m_nButtons & ~IN_SCORE);

	// Strip out the duck key from this check if it's toggled
	if ( (fAnyButtonDown & IN_DUCK) && GetToggledDuckState())
	{
		fAnyButtonDown &= ~IN_DUCK;
	}

	// Strip out zoom toggle
	fAnyButtonDown &= ~IN_ZOOM;

	if ( GameRules()->IsMultiplayer() == false && sp_fade_and_force_respawn.GetBool() )
	{
		const float flFadeAndResapwnTime = 3.0f;
		color32 clr;
		clr.r = clr.g = clr.b = 0;
		clr.a = 255;
		UTIL_ScreenFade( this, clr, flFadeAndResapwnTime, flFadeAndResapwnTime + 1.0f, FFADE_OUT | FFADE_STAYOUT );

		if ( gpGlobals->curtime > m_flDeathTime + flFadeAndResapwnTime )
		{
			RespawnPlayer();
			return;
		}
	}

	bool bMultiplayerForceRespawn = ( g_pGameRules->IsMultiplayer() && forcerespawn.GetInt() > 0 );

	// wait for all buttons released
	if ( m_lifeState == LIFE_DEAD )
	{
		if ( ( fAnyButtonDown && !bMultiplayerForceRespawn ) || gpGlobals->curtime < m_flDeathTime + PORTAL_RESPAWN_DELAY )
			return;

		if ( g_pGameRules->FPlayerCanRespawn( this ) )
		{
			m_lifeState = LIFE_RESPAWNABLE;
		}

		return;
	}

	// if the player has been dead for one second longer than allowed by forcerespawn,
	// forcerespawn isn't on. Send the player off to an intermission camera until they
	// choose to respawn.
	if ( g_pGameRules->IsMultiplayer() && forcerespawn.GetInt() == 0 && gpGlobals->curtime > m_flDeathTime + DEATH_ANIMATION_TIME && !IsObserver() )
	{
		// go to dead camera.
		StartObserverMode( m_iObserverLastMode );
	}

	// wait for any button down, or mp_forcerespawn is set and the respawn time is up
	if ( ( fAnyButtonDown || bMultiplayerForceRespawn ) && gpGlobals->curtime >= m_flDeathTime + PORTAL_RESPAWN_DELAY )
	{
		RespawnPlayer();
	}
}

void CPortal_Player::RespawnPlayer( void )
{
	m_nButtons = 0;
	m_iRespawnFrames = 0;

	// All condition should be removed
	m_Shared.RemoveAllCond();
	SetTeamTauntState( TEAM_TAUNT_NONE );

	if ( GameRules()->IsMultiplayer() )
	{
		// if the player was dropped by the other player and fell into goo, give PARTNER_DROP Ach. to the other player
		if ( m_bWasDroppedByOtherPlayerWhileTaunting && GetWaterLevel() != WL_NotInWater )
		{
			CPortal_Player *pOtherPlayer = ToPortalPlayer( UTIL_OtherConnectedPlayer( this ) );
			if ( pOtherPlayer )
			{
				UTIL_RecordAchievementEvent( "ACH.PARTNER_DROP", pOtherPlayer );
			}
		}
	}

	//Msg( "Respawn\n");

	if ( PortalGameRules() && GetBonusChallenge() > 0 )
	{
		// Single player challenge needs to respawn this way so we don't lose the session
		engine->ChangeLevel( gpGlobals->mapname.ToCStr(), NULL );
	}
	else
	{
		respawn( this, !IsObserver() );// don't copy a corpse if we're in deathcam.
	}

	SetNextThink( TICK_NEVER_THINK );
}

void CPortal_Player::PlayerTransitionCompleteThink( void )
{
	const char *szVideoCommand = "stop_transition_videos_fadeout";
	char szClientCmd[256];
	Q_snprintf( szClientCmd, sizeof(szClientCmd), "%s %f\n", szVideoCommand, 1.5f );
	engine->ClientCommand( edict(), szClientCmd );

	if ( mp_dev_wait_for_other_player.GetBool() && ( PortalMPGameRules() && !PortalMPGameRules()->IsCommunityCoopHub() ) )
	{
		// We wanted to wait for the other player
		if ( PortalMPGameRules() && ( !PortalMPGameRules()->IsPlayerDataReceived( 0 ) || !PortalMPGameRules()->IsPlayerDataReceived( 1 ) ) )
		{
			// Both players haven't sent their data, shut it down!
			DevMsg( "Player transitioned with no partner!\n" );
			engine->ClientCommand( this->edict(), "disconnect \"Partner disconnected\"" );
		}
	}

	ChallengePlayersReady();

	// Respawn other players who were waiting
	SetThink( &CBasePlayer::PlayerDeathThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	SetContextThink( NULL, 0, CATCHPATNERNOTCONNECTING_THINK_CONTEXT );
}

void CPortal_Player::PlayerCatchPatnerNotConnectingThink()
{
	int numValidNetChannels = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		INetChannelInfo *pNetChan = engine->GetPlayerNetInfo( i );
		if ( !pNetChan )
			continue;

		++ numValidNetChannels;
	}

	if ( numValidNetChannels < 2 )
	{
		SetContextThink( NULL, 0, CATCHPATNERNOTCONNECTING_THINK_CONTEXT );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
		SetContextThink( &CPortal_Player::PlayerCatchPatnerNotConnectingThink, gpGlobals->curtime + 1.0f, CATCHPATNERNOTCONNECTING_THINK_CONTEXT );
	}
}

void CPortal_Player::UpdatePortalPlaneSounds( void )
{
	CPortal_Base2D *pPortal = dynamic_cast<CProp_Portal *>(m_hPortalEnvironment.Get());
	if ( pPortal && pPortal->IsActive() )
	{
		Vector vVelocity;
		GetVelocity( &vVelocity, NULL );

		if ( !vVelocity.IsZero() )
		{
			Vector vMin, vMax;
			CollisionProp()->WorldSpaceAABB( &vMin, &vMax );

			Vector vEarCenter = ( vMax + vMin ) / 2.0f;
			Vector vDiagonal = vMax - vMin;

			if ( !m_bIntersectingPortalPlane )
			{
				vDiagonal *= 0.25f;

				if ( UTIL_IsBoxIntersectingPortal( vEarCenter, vDiagonal, pPortal ) )
				{
					m_bIntersectingPortalPlane = true;

					CPASAttenuationFilter filter( this );
					CSoundParameters params;
					if ( GetParametersForSound( "PortalPlayer.EnterPortal", params, NULL ) )
					{
						EmitSound_t ep( params );
						ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
						ep.m_flVolume = MIN( 0.3f + vVelocity.Length() * 0.00075f, 1.0f );

						EmitSound( filter, entindex(), ep );
					}
				}
			}
			else
			{
				vDiagonal *= 0.30f;

				if ( !UTIL_IsBoxIntersectingPortal( vEarCenter, vDiagonal, pPortal ) )
				{
					m_bIntersectingPortalPlane = false;

					CPASAttenuationFilter filter( this );
					CSoundParameters params;
					if ( GetParametersForSound( "PortalPlayer.ExitPortal", params, NULL ) )
					{
						EmitSound_t ep( params );
						ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
						ep.m_flVolume = MIN( 0.3f + vVelocity.Length() * 0.00075f, 1.0f );

						EmitSound( filter, entindex(), ep );
					}
				}
			}
		}
	}
	else if ( m_bIntersectingPortalPlane )
	{
		m_bIntersectingPortalPlane = false;

		CPASAttenuationFilter filter( this );
		CSoundParameters params;
		if ( GetParametersForSound( "PortalPlayer.ExitPortal", params, NULL ) )
		{
			EmitSound_t ep( params );
			Vector vVelocity;
			GetVelocity( &vVelocity, NULL );
			ep.m_nPitch = 80.0f + vVelocity.Length() * 0.03f;
			ep.m_flVolume = MIN( 0.3f + vVelocity.Length() * 0.00075f, 1.0f );

			EmitSound( filter, entindex(), ep );
		}
	}
}

void CPortal_Player::UpdateWooshSounds( void )
{
	if ( m_pWooshSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		float fWooshVolume = GetAbsVelocity().Length() - MIN_FLING_SPEED;

		if ( fWooshVolume < 0.0f )
		{
			controller.SoundChangeVolume( m_pWooshSound, 0.0f, 0.1f );
			return;
		}

		fWooshVolume /= 2000.0f;
		if ( fWooshVolume > 1.0f )
			fWooshVolume = 1.0f;

		controller.SoundChangeVolume( m_pWooshSound, fWooshVolume, 0.1f );
		//		controller.SoundChangePitch( m_pWooshSound, fWooshVolume + 0.5f, 0.1f );
	}
}

void CPortal_Player::FireBullets ( const FireBulletsInfo_t &info )
{
	NoteWeaponFired();

	BaseClass::FireBullets( info );
}

void CPortal_Player::NoteWeaponFired( void )
{
	Assert( m_pCurrentCommand );
	if( m_pCurrentCommand )
	{
		m_iLastWeaponFireUsercmd = m_pCurrentCommand->command_number;
	}
}

extern ConVar sv_maxunlag;

bool CPortal_Player::WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const
{
	// No need to lag compensate at all if we're not attacking in this command and
	// we haven't attacked recently.
	if ( !( pCmd->buttons & IN_ATTACK ) && (pCmd->command_number - m_iLastWeaponFireUsercmd > 5) )
		return false;

	// If this entity hasn't been transmitted to us and acked, then don't bother lag compensating it.
	if ( pEntityTransmitBits && !pEntityTransmitBits->Get( pPlayer->entindex() ) )
		return false;

	const Vector &vMyOrigin = GetAbsOrigin();
	const Vector &vHisOrigin = pPlayer->GetAbsOrigin();

	// get max distance player could have moved within max lag compensation time,
	// multiply by 1.5 to to avoid "dead zones"  (sqrt(2) would be the exact value)
	float maxDistance = 1.5 * pPlayer->MaxSpeed() * sv_maxunlag.GetFloat();

	// If the player is within this distance, lag compensate them in case they're running past us.
	if ( vHisOrigin.DistTo( vMyOrigin ) < maxDistance )
		return true;

	// If their origin is not within a 45 degree cone in front of us, no need to lag compensate.
	Vector vForward;
	AngleVectors( pCmd->viewangles, &vForward );

	Vector vDiff = vHisOrigin - vMyOrigin;
	VectorNormalize( vDiff );

	float flCosAngle = 0.707107f;	// 45 degree angle
	if ( vForward.Dot( vDiff ) < flCosAngle )
		return false;

	return true;
}


void CPortal_Player::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	m_PlayerAnimState->DoAnimationEvent( event, nData );
	TE_PlayerAnimEvent( this, event, nData );	// Send to any clients who can see this guy.
}

//-----------------------------------------------------------------------------
// Purpose: Override setup bones so that is uses the render angles from
//			the Portal animation state to setup the hitboxes.
//-----------------------------------------------------------------------------
void CPortal_Player::SetupBones( matrix3x4a_t *pBoneToWorld, int boneMask )
{
	VPROF_BUDGET( "CBaseAnimating::SetupBones", VPROF_BUDGETGROUP_SERVER_ANIM );

	// Set the mdl cache semaphore.
	MDLCACHE_CRITICAL_SECTION();

	// Get the studio header.
	Assert( GetModelPtr() );
	CStudioHdr *pStudioHdr = GetModelPtr( );

	Vector pos[MAXSTUDIOBONES];
	QuaternionAligned q[MAXSTUDIOBONES];

	// Adjust hit boxes based on IK driven offset.
	Vector adjOrigin = GetAbsOrigin() + Vector( 0, 0, m_flEstIkOffset );

	// FIXME: pass this into Studio_BuildMatrices to skip transforms
	CBoneBitList boneComputed;
	if ( m_pIk )
	{
		m_iIKCounter++;
		m_pIk->Init( pStudioHdr, GetAbsAngles(), adjOrigin, gpGlobals->curtime, m_iIKCounter, boneMask );
		GetSkeleton( pStudioHdr, pos, q, boneMask );

		m_pIk->UpdateTargets( pos, q, pBoneToWorld, boneComputed );
		CalculateIKLocks( gpGlobals->curtime );
		m_pIk->SolveDependencies( pos, q, pBoneToWorld, boneComputed );
	}
	else
	{
		GetSkeleton( pStudioHdr, pos, q, boneMask );
	}

	CBaseAnimating *pParent = dynamic_cast< CBaseAnimating* >( GetMoveParent() );
	if ( pParent )
	{
		// We're doing bone merging, so do special stuff here.
		CBoneCache *pParentCache = pParent->GetBoneCache();
		if ( pParentCache )
		{
			BuildMatricesWithBoneMerge(
				pStudioHdr,
				m_PlayerAnimState->GetRenderAngles(),
				adjOrigin,
				pos,
				q,
				pBoneToWorld,
				pParent,
				pParentCache );

			return;
		}
	}

	Studio_BuildMatrices(
		pStudioHdr,
		m_PlayerAnimState->GetRenderAngles(),
		adjOrigin,
		pos,
		q,
		-1,
		GetModelScale(), // Scaling
		pBoneToWorld,
		boneMask );
}


// Set the activity based on an event or current state
void CPortal_Player::SetAnimation( PLAYER_ANIM playerAnim )
{
	return;
}

extern int	gEvilImpulse101;

//-----------------------------------------------------------------------------
// Purpose: Player reacts to bumping a weapon.
// Input  : pWeapon - the weapon that the player bumped into.
// Output : Returns true if player picked up the weapon
//-----------------------------------------------------------------------------
bool CPortal_Player::BumpWeapon( CBaseCombatWeapon *pWeapon )
{
	CBaseCombatCharacter *pOwner = pWeapon->GetOwner();

	// Can I have this weapon type?
	if ( !IsAllowedToPickupWeapons() )
		return false;

	if ( pOwner || !Weapon_CanUse( pWeapon ) || !g_pGameRules->CanHavePlayerItem( this, pWeapon ) )
	{
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( pWeapon );
		}
		return false;
	}

	// Don't let the player fetch weapons through walls (use MASK_SOLID so that you can't pickup through windows)
	if( !pWeapon->FVisible( this, MASK_SOLID ) && !(GetFlags() & FL_NOTARGET) )
	{
		return false;
	}

	CWeaponPortalgun *pPickupPortalgun = dynamic_cast<CWeaponPortalgun*>( pWeapon );

	bool bOwnsWeaponAlready = !!Weapon_OwnsThisType( pWeapon->GetClassname(), pWeapon->GetSubType());

	if ( bOwnsWeaponAlready == true )
	{
		// If we picked up a second portal gun set the bool to alow secondary fire
		if ( pPickupPortalgun )
		{
			CWeaponPortalgun *pPortalGun = static_cast<CWeaponPortalgun*>( Weapon_OwnsThisType( pWeapon->GetClassname() ) );

			if ( pPickupPortalgun->CanFirePortal1() )
				pPortalGun->SetCanFirePortal1();

			if ( pPickupPortalgun->CanFirePortal2() )
				pPortalGun->SetCanFirePortal2();

			UTIL_Remove( pWeapon );
			return true;
		}

		//If we have room for the ammo, then "take" the weapon too.
		if ( Weapon_EquipAmmoOnly( pWeapon ) )
		{
			pWeapon->CheckRespawn();

			UTIL_Remove( pWeapon );
			return true;
		}
		else
		{
			return false;
		}
	}
	else if ( pPickupPortalgun )
	{
		// HACK HACK: In Portal 2's incenerator the gun wasn't set correctly and they fired an upgrade_portalgun
		// command to work around it. Now that cheat commands are protected correctly we need a way to give the
		// player both portals without modifying the map. So lets just check if it's the incenerator map and fix
		// it up. -Jeep
		if ( V_strcmp( gpGlobals->mapname.ToCStr(), "sp_a2_intro" ) == 0 )
		{
			pPickupPortalgun->SetCanFirePortal1();
			pPickupPortalgun->SetCanFirePortal2();
		}

		if ( pPickupPortalgun->CanFirePortal2() )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "portal_enabled" );
			if ( event )
			{
				event->SetInt( "userid", GetUserID() );
				event->SetBool( "leftportal", false );

				gameeventmanager->FireEvent( event );
			}
		}

		if ( pPickupPortalgun->CanFirePortal1() )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "portal_enabled" );
			if ( event )
			{
				event->SetInt( "userid", GetUserID() );
				event->SetBool( "leftportal", true );

				gameeventmanager->FireEvent( event );
			}
		}
	}

	pWeapon->CheckRespawn();
	Weapon_Equip( pWeapon );

	// If we're holding and object before picking up portalgun, drop it
	if ( pPickupPortalgun )
	{
		ForceDropOfCarriedPhysObjects( GetPlayerHeldEntity( this ) );
	}

	return true;
}

void CPortal_Player::ShutdownUseEntity( void )
{
	ShutdownPickupController( m_hUseEntity );
}


void CPortal_Player::Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity, bool bUseSlowHighAccuracyContacts )
{
	Vector oldOrigin = GetLocalOrigin();
	QAngle oldAngles = GetLocalAngles();
	BaseClass::Teleport( newPosition, newAngles, newVelocity, bUseSlowHighAccuracyContacts );
	m_angEyeAngles = pl.v_angle;

	m_PlayerAnimState->Teleport( newPosition, newAngles, this );

	m_flUsePostTeleportationBoxTime = sv_post_teleportation_box_time.GetFloat();

	const PaintPowerInfo_t& speedPower = GetPaintPower( SPEED_POWER );
	if( !IsInactivePower( speedPower ) )
	{
		m_PortalLocal.m_flAirInputScale = 0.0f;
	}

	if ( newPosition )
	{
		m_vPrevPosition = *newPosition;
	}
}


bool CPortal_Player::UseFoundEntity( CBaseEntity *pUseEntity, bool bAutoGrab )
{
	bool usedSomething = false;

	//!!!UNDONE: traceline here to prevent +USEing buttons through walls
	int caps = pUseEntity->ObjectCaps();
	variant_t emptyVariant;

	if ( ( (m_nButtons & IN_USE) && (caps & FCAP_CONTINUOUS_USE) ) ||
		 ( ( (m_afButtonPressed & IN_USE) || bAutoGrab ) && (caps & (FCAP_IMPULSE_USE|FCAP_ONOFF_USE)) ) )
	{
		if ( caps & FCAP_CONTINUOUS_USE )
		{
			m_afPhysicsFlags |= PFLAG_USING;
		}

		pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_TOGGLE );

		usedSomething = true;
	}
	// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
	else if ( (m_afButtonReleased & IN_USE) && (pUseEntity->ObjectCaps() & FCAP_ONOFF_USE) )	// BUGBUG This is an "off" use
	{
		pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_TOGGLE );

		usedSomething = true;
	}

	return usedSomething;
}

void CPortal_Player::PlayerUse( void )
{
	CBaseEntity *pUseEnt = m_hGrabbedEntity.Get();
	CPortal_Base2D *pUseThroughPortal = (CPortal_Base2D*)m_hPortalThroughWhichGrabOccured.Get();

	if ( gpGlobals->curtime < m_flUseKeyCooldownTime )
	{
		return;
	}

	bool bUsePressed = (m_afButtonPressed & IN_USE) != 0;

	if ( bUsePressed && m_hUseEntity )
	{
		// Currently using a latched entity?
		if ( !ClearUseEntity() )
		{
			m_bPlayUseDenySound = true;
		}
		else
		{
			m_flAutoGrabLockOutTime = gpGlobals->curtime;
		}
		return;
	}

	if ( !pUseEnt )
	{
		PollForUseEntity( bUsePressed, &pUseEnt, &pUseThroughPortal );
	}

	// Was use pressed or released?
	if ( !bUsePressed && !pUseEnt )
	{
		return;
	}

	// Only run the below if use is pressed and the client sent a grab entity
	bool bUsedSomething = false;
	if ( pUseEnt && m_hAttachedObject.Get() == NULL )
	{
		// Use the found entity, and skip the button down checks if we're forcing the grabbing of a client use entity
		bUsedSomething = UseFoundEntity( pUseEnt, !bUsePressed );

		if ( bUsedSomething && pUseThroughPortal )
		{
			SetHeldObjectOnOppositeSideOfPortal( true );
			SetHeldObjectPortal( pUseThroughPortal );
		}
		else
		{
			SetHeldObjectOnOppositeSideOfPortal( false );
			SetHeldObjectPortal( NULL );
		}

		// Debounce the use key
		if ( bUsedSomething )
		{
			m_Local.m_nOldButtons |= IN_USE;
			m_afButtonPressed &= ~IN_USE;
		}
	}

	if ( bUsePressed && !bUsedSomething )
	{
		// No entity passed up with the use command, play deny sound
		m_bPlayUseDenySound = true;

		// Make the weapon "dry fire" to show we tried to +use
		CWeaponPortalgun *pWeapon = dynamic_cast<CWeaponPortalgun *>(GetActivePortalWeapon());
		if ( pWeapon )
		{
			pWeapon->UseDeny();
		}
	}
}

extern ConVar sv_enableholdrotation;
ConVar sv_holdrotationsensitivity( "sv_holdrotationsensitivity", "0.1", FCVAR_ARCHIVE );
void CPortal_Player::PlayerRunCommand(CUserCmd *ucmd, IMoveHelper *moveHelper)
{
	if ( sv_enableholdrotation.GetBool() && !IsUsingVMGrab() )
	{
		if( (ucmd->buttons & IN_ATTACK2) && (GetPlayerHeldEntity( this ) != NULL) )
		{
			VectorCopy ( pl.v_angle, ucmd->viewangles );
			float x, y;
			if( abs( ucmd->mousedx ) > abs(ucmd->mousedy) )
			{
				x = ucmd->mousedx * sv_holdrotationsensitivity.GetFloat();
				y = 0;
			}
			else
			{
				x = 0;
				y = ucmd->mousedy * sv_holdrotationsensitivity.GetFloat();
			}
			RotatePlayerHeldObject( this, x, y, true );
		}
	}

	// Can't use stuff while dead
	if ( IsDead() )
	{
		ucmd->buttons &= ~IN_USE;
	}

	m_hGrabbedEntity = ucmd->player_held_entity > 0 ? CBaseEntity::Instance ( ucmd->player_held_entity ) : NULL;
	m_hPortalThroughWhichGrabOccured = ucmd->held_entity_was_grabbed_through_portal > 0 ? CBaseEntity::Instance ( ucmd->held_entity_was_grabbed_through_portal ) : NULL;

	//============================================================================
	// Fix the eye angles after portalling. The client may have sent commands with
	// the old view angles before it knew about the teleportation.

	//sorry for crappy name, the client sent us a command, we acknowledged it, and they are now telling us the latest one they received an acknowledgement for in this brand new command
	int iLastCommandAcknowledgementReceivedOnClientForThisCommand = ucmd->command_number - ucmd->command_acknowledgements_pending;
	while( (m_PendingPortalTransforms.Count() > 0) && (iLastCommandAcknowledgementReceivedOnClientForThisCommand >= m_PendingPortalTransforms[0].command_number) )
	{
		m_PendingPortalTransforms.Remove( 0 );
	}

	// The server changed the angles, and the user command was created after the teleportation, but before the client knew they teleported. Need to fix up the angles into the new space
	if( m_PendingPortalTransforms.Count() > ucmd->predictedPortalTeleportations )
	{
		matrix3x4_t matComputeFinalTransform[2];
		int iFlip = 0;

		//most common case will be exactly 1 transform
		matComputeFinalTransform[0] = m_PendingPortalTransforms[ucmd->predictedPortalTeleportations].matTransform;

		for( int i = ucmd->predictedPortalTeleportations + 1; i < m_PendingPortalTransforms.Count(); ++i )
		{
			ConcatTransforms( m_PendingPortalTransforms[i].matTransform, matComputeFinalTransform[iFlip], matComputeFinalTransform[1-iFlip] );
			iFlip = 1 - iFlip;
		}

		//apply the final transform
		matrix3x4_t matAngleTransformIn, matAngleTransformOut;
		AngleMatrix( ucmd->viewangles, matAngleTransformIn );
		ConcatTransforms( matComputeFinalTransform[iFlip], matAngleTransformIn, matAngleTransformOut );
		MatrixAngles( matAngleTransformOut, ucmd->viewangles );
	}

	PreventCrouchJump( ucmd );

	if ( m_PortalLocal.m_bZoomedIn )
	{
		if ( IsTaunting() )
		{
			// Pop out of zoom when I'm taunting
			ZoomOut();
		}
		else
		{
			float fThreshold = sv_zoom_stop_movement_threashold.GetFloat();
			if ( gpGlobals->curtime > GetFOVTime() + sv_zoom_stop_time_threashold.GetFloat() &&
				 ( fabsf( ucmd->forwardmove ) > fThreshold ||  fabsf( ucmd->sidemove ) > fThreshold ) )
			{
				// After 5 seconds, moving while zoomed will pop you back out
				// This is to fix people who accidentally switch into zoom mode, but don't know how to get back out...
				// Should give plenty of time to players who want to move while zoomed
				ZoomOut();
			}
		}
	}

	BaseClass::PlayerRunCommand( ucmd, moveHelper );
}

//-----------------------------------------------------------------------------
// Purpose: Deal with command coming in from the client-side
//-----------------------------------------------------------------------------
bool CPortal_Player::ClientCommand( const CCommand &args )
{
	const char *pcmd = args[0];
	if ( FStrEq( pcmd, "taunt" ) )
	{
		if ( args.ArgC() > 1 )
		{
			Taunt( args[1] );
		}
		else
		{
			Taunt();
		}
		return true;
	}
	else if ( FStrEq( pcmd, "taunt_auto" ) )
	{
		if ( args.ArgC() > 1 )
		{
			Taunt( args[1], true );
		}

		return true;
	}
	else if ( FStrEq( pcmd, "spectate" ) )
	{
		if( CommandLine()->FindParm( "-allowspectators" ) == NULL )
		{
			// do nothing
			return true;
		}
	}
	else if ( FStrEq( pcmd, "end_movie" ) )
	{
		if ( args.ArgC() == 2 )
		{
			const char *target = STRING( AllocPooledString( args[1] ) );
			const char *action = "__MovieFinished";
			variant_t value;

			g_EventQueue.AddEvent( target, action, value, 0, this, this );
		}
	}
	else if ( FStrEq( pcmd, "signify" ) )
	{
		// Verify our argument count
		if ( args.ArgC() != 10 )
		{
			Assert(args.ArgC() != 10);
			Msg("Ill-formed signify command from client!\n");
			return true;
		}

		// Now, pull the pieces out of the string
		const char *lpszCommand = args[1];
		int nIndex = V_atoi( args[2] );

		float fDelay = V_atof( args[3] );

		Vector vPosition;
		vPosition.x = V_atof( args[4] );
		vPosition.y = V_atof( args[5] );
		vPosition.z = V_atof( args[6] );

		Vector vNormal;
		vNormal.x = V_atof( args[7] );
		vNormal.y = V_atof( args[8] );
		vNormal.z = V_atof( args[9] );

		// Now send the message on to the client
		CReliableBroadcastRecipientFilter player;
		player.AddAllPlayers();

		if ( fDelay <= 0.0f )
		{
			player.RemoveRecipient( this );	// Remove us because we already predicted the results
		}

		CBaseEntity *pTargetEnt = ( nIndex != -1 ) ? UTIL_EntityByIndex( nIndex ) : NULL;

		UserMessageBegin( player, "AddLocator" );
			WRITE_SHORT( entindex() );
			WRITE_EHANDLE( pTargetEnt );
			WRITE_FLOAT( gpGlobals->curtime + fDelay );
			WRITE_VEC3COORD( vPosition );
			WRITE_VEC3NORMAL( vNormal );
			WRITE_STRING( lpszCommand );
		MessageEnd();

		IGameEvent *event = gameeventmanager->CreateEvent( "portal_player_ping" );
		if ( event )
		{
			Vector vecSpot = vPosition;
			if ( pTargetEnt )
				vecSpot = pTargetEnt->GetAbsOrigin();

			event->SetInt("userid", GetUserID() );
			event->SetFloat("ping_x", vecSpot.x );
			event->SetFloat("ping_y", vecSpot.y );
			event->SetFloat("ping_z", vecSpot.z );
			gameeventmanager->FireEvent( event );
		}

		// Denote that we're pointing
		m_Shared.AddCond( PORTAL_COND_POINTING, 1.0f );

		return true;
	}
	else if ( StringHasPrefix( pcmd, "CoopPingTool" ) )
	{
		CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, "@glados" );
		if ( pEntity )
		{
			pEntity->RunScript( args.GetCommandString(), "PingToolCommand" );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "survey_done" ) )
	{
		int nIndex = V_atoi( args[1] );
		CPointSurvey *pPointSurveyEnt = ( nIndex != -1 ) ? (CPointSurvey*)UTIL_EntityByIndex( nIndex ) : NULL;
		if ( pPointSurveyEnt )
		{
			pPointSurveyEnt->OnSurveyCompleted();
		}
		return true;
	}
	else if ( FStrEq( pcmd, "load_recent_checkpoint" ) )
	{
		if ( !PortalMPGameRules() )
		{
			RespawnPlayer();
		}
	}
	else if ( FStrEq( pcmd, "pre_go_to_hub" ) )
	{
		// Play transition video
		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
			if ( pToPlayer && !pToPlayer->IsSplitScreenPlayer() )
			{
				engine->ClientCommand( pToPlayer->edict(), "stopvideos" );
				engine->ClientCommand( pToPlayer->edict(), "playvideo_end_level_transition coop_bots_load 1" );
			}
		}

		return true;
	}
	else if ( FStrEq( pcmd, "go_to_hub" ) )
	{
		if ( PortalMPGameRules() )
		{
			PortalMPGameRules()->SaveMPStats();
		}

		bool bBothPlayersHaveDLC = true;

		if ( IsGameConsole() )
		{
			IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
			if ( pIMatchSession )
			{
				KeyValues *pFullSettings = pIMatchSession->GetSessionSettings();
				if ( pFullSettings )
				{
					if ( !( ( pFullSettings->GetUint64( "members/machine0/dlcmask" ) & PORTAL2_DLCID_RETAIL_DLC1 ) &&
						( XBX_GetNumGameUsers() > 1 ||
						( pFullSettings->GetUint64( "members/machine1/dlcmask" ) & PORTAL2_DLCID_RETAIL_DLC1 ) ) ) )
					{
						bBothPlayersHaveDLC = false;
					}
				}
			}
		}

		// clear out any outstanding UI for both players
		ClearClientUI();

		if ( bBothPlayersHaveDLC )
		{
			// They have the DLC!
			engine->ChangeLevel( "mp_coop_lobby_3", NULL );
		}
		else
		{
			// One is missing the DLC! Go to the old hub
			engine->ChangeLevel( "mp_coop_lobby_2", NULL );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "mp_restart_level" ) )
	{
		if ( PortalMPGameRules() )
		{
			PortalMPGameRules()->SaveMPStats();
		}

		// clear out any outstanding UI for both players
		ClearClientUI();
		engine->ChangeLevel( gpGlobals->mapname.ToCStr(), NULL );
	}
	else if ( FStrEq( pcmd, "mp_select_level" ) )
	{
		if ( PortalMPGameRules() )
		{
			PortalMPGameRules()->SaveMPStats();
		}

		if ( args.ArgC() > 1 )
		{
			// clear out any outstanding UI for both players
			ClearClientUI();
			engine->ChangeLevel( args[1], NULL );
		}

	}
	else if ( FStrEq( pcmd, "restart_level" ) )
	{
		sv_bonus_challenge.SetValue( GetBonusChallenge() );
#if !defined( _GAMECONSOLE )
		g_Portal2ResearchDataTracker.Event_PlayerGaveUp();
#endif // !defined( _GAMECONSOLE )

		// clear out any outstanding UI for both players
		ClearClientUI();
		engine->ChangeLevel( gpGlobals->mapname.ToCStr(), NULL );
	}
	else if ( FStrEq( pcmd, "erase_mp_progress" ) )
	{
		if ( PortalMPGameRules() )
		{
			PortalMPGameRules()->SetAllMapsComplete( false, atoi(args[1]) );
		}
		return true;
	}
	else if ( FStrEq( pcmd, "transition_map" ) )
	{
		CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, "@transition_script" );
		if ( !pEntity )
		{
			pEntity = gEntList.FindEntityByName( NULL, "script_check_finish_game" );	// Final level in coop
			if ( !pEntity )
			{
				CBaseEntity *pTempEntity = gEntList.FindEntityByClassname( NULL, "logic_script" );
				while ( pTempEntity )
				{
					if ( V_stristr( pTempEntity->GetEntityName().ToCStr(), "transition_script" ) )	// DLC levels
					{
						pEntity = pTempEntity;
						break;
					}
					pTempEntity = gEntList.FindEntityByClassname( pTempEntity, "logic_script" );
				}
			}
		}

		// clear out any outstanding UI for both players
		ClearClientUI();

		if ( pEntity )
		{
			pEntity->RunScript( "RealTransitionFromMap()" );
		}
	}
	else if ( FStrEq( pcmd, "select_map" ) )
	{
		// get the map to run
		if ( args.ArgC() > 1 )
		{
			// clear out any outstanding UI for both players
			ClearClientUI();

			const char *pMapName = args[1];
			sv_bonus_challenge.SetValue( GetBonusChallenge() );
			engine->ChangeLevel( pMapName, NULL );
		}

	}
	else if ( FStrEq( pcmd, "pre_go_to_calibration" ) )
	{
		// Play transition video
		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
			if ( pToPlayer && !pToPlayer->IsSplitScreenPlayer() )
			{
				engine->ClientCommand( pToPlayer->edict(), "stopvideos" );
				engine->ClientCommand( pToPlayer->edict(), "playvideo_end_level_transition coop_bots_load_wave 1" );
			}
		}
		return true;
	}
	else if ( FStrEq( pcmd, "go_to_calibration" ) )
	{
		if ( PortalMPGameRules() )
		{
			PortalMPGameRules()->SaveMPStats();
		}

		// clear out any outstanding UI for both players
		ClearClientUI();

		engine->ChangeLevel( "mp_coop_start", NULL );
		return true;
	}
	else if ( FStrEq( pcmd, "level_complete_data" ) )
	{
		CPortalMPGameRules *pRules = PortalMPGameRules();
		if ( pRules )
		{
			pRules->SetMapCompleteData( atoi(args[1]) );
		}

		return true;
	}
	else if ( FStrEq( pcmd, "mp_stats_data" ) )
	{
		CPortalMPStats *pStats = GetPortalMPStats();
		if ( pStats )
		{
			pStats->SetStats( atoi(args[1]), atoi(args[2]), atoi(args[3]), atoi(args[4]) );
		}

		return true;
	}

	return BaseClass::ClientCommand( args );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CPortal_Player::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT );
}

//-----------------------------------------------------------------------------
// Cheats only, this is for helping developers choose lighting for scenes
//-----------------------------------------------------------------------------
bool CPortal_Player::FlashlightTurnOn( bool playSound /*= false*/ )
{
	if ( sv_cheats->GetBool() == false )
		return false;

	AddEffects( EF_DIMLIGHT );
	return true;
}


//-----------------------------------------------------------------------------
// Cheats only, this is for helping developers choose lighting for scenes
//-----------------------------------------------------------------------------
void CPortal_Player::FlashlightTurnOff( bool playSound /*= false*/ )
{
	if ( sv_cheats->GetBool() == false )
		return;

	RemoveEffects( EF_DIMLIGHT );
}
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortal_Player::CheatImpulseCommands( int iImpulse )
{
	switch ( iImpulse )
	{
	case 101:
		{
			if( sv_cheats->GetBool() )
			{
				//GiveAllItems();
				// FIXME: Bring this back for DLC2
				//sv_can_carry_both_guns.SetValue( 1 );

				//GivePlayerPaintGun( true, false );
				GivePlayerPortalGun( true, true );
			}
		}
		break;

	default:
		BaseClass::CheatImpulseCommands( iImpulse );
	}
}

void CPortal_Player::CreateViewModel( int index /*=0*/ )
{
	if( !g_pGameRules->IsMultiplayer() )
		return BaseClass::CreateViewModel( index );

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

bool CPortal_Player::BecomeRagdollOnClient( const Vector &force )
{
	return true;//BaseClass::BecomeRagdollOnClient( force );
}

void CPortal_Player::CreateRagdollEntity( const CTakeDamageInfo &info )
{
	if ( m_hRagdoll )
	{
		UTIL_RemoveImmediate( m_hRagdoll );
		m_hRagdoll = NULL;
	}

#if defined PORTAL_HIDE_PLAYER_RAGDOLL
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW | EF_NOSHADOW );
	AddEFlags( EFL_NO_DISSOLVE );
#endif // PORTAL_HIDE_PLAYER_RAGDOLL
	CBaseEntity *pRagdoll = CreateServerRagdoll( this, m_nForceBone, info, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
	pRagdoll->m_takedamage = DAMAGE_NO;
	m_hRagdoll = pRagdoll;

/*
	// If we already have a ragdoll destroy it.
	CPortalRagdoll *pRagdoll = dynamic_cast<CPortalRagdoll*>( m_hRagdoll.Get() );
	if( pRagdoll )
	{
		UTIL_Remove( pRagdoll );
		pRagdoll = NULL;
	}
	Assert( pRagdoll == NULL );

	// Create a ragdoll.
	pRagdoll = dynamic_cast<CPortalRagdoll*>( CreateEntityByName( "portal_ragdoll" ) );
	if ( pRagdoll )
	{


		pRagdoll->m_hPlayer = this;
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_nModelIndex = m_nModelIndex;
		pRagdoll->m_nForceBone = m_nForceBone;
		pRagdoll->CopyAnimationDataFrom( this );
		pRagdoll->SetOwnerEntity( this );
		pRagdoll->m_flAnimTime = gpGlobals->curtime;
		pRagdoll->m_flPlaybackRate = 0.0;
		pRagdoll->SetCycle( 0 );
		pRagdoll->ResetSequence( 0 );

		float fSequenceDuration = SequenceDuration( GetSequence() );
		float fPreviousCycle = clamp(GetCycle()-( 0.1 * ( 1 / fSequenceDuration ) ),0.f,1.f);
		float fCurCycle = GetCycle();

		matrix3x4a_t bonetoworldnext[MAXSTUDIOBONES];
		SetupBones( bonetoworldnext, BONE_USED_BY_ANYTHING );
		SetCycle( fPreviousCycle );
		matrix3x4a_t bonetoworld[MAXSTUDIOBONES];
		SetupBones( bonetoworld, BONE_USED_BY_ANYTHING );
		SetCycle( fCurCycle );

		//pRagdoll->InitRagdoll( info.GetDamageForce(), m_nForceBone, info.GetDamagePosition(), bonetoworld, bonetoworldnext, 0.1f, COLLISION_GROUP_INTERACTIVE_DEBRIS, true );
		pRagdoll->SetMoveType( MOVETYPE_VPHYSICS );
		pRagdoll->SetSolid( SOLID_VPHYSICS );
		if ( IsDissolving() )
		{
			pRagdoll->TransferDissolveFrom( this );
		}

		Vector mins, maxs;
		mins = CollisionProp()->OBBMins();
		maxs = CollisionProp()->OBBMaxs();
		pRagdoll->CollisionProp()->SetCollisionBounds( mins, maxs );
		pRagdoll->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
	}

	// Turn off the player.
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddEffects( EF_NODRAW | EF_NOSHADOW );
	SetMoveType( MOVETYPE_NONE );

	// Save ragdoll handle.
	m_hRagdoll = pRagdoll;
*/
}

void CPortal_Player::Jump( void )
{
	#if !defined ( _GAMECONSOLE ) && !defined( NO_STEAM )
	g_PortalGameStats.Event_PlayerJump( GetAbsOrigin(), GetAbsVelocity() );
	#endif

	BaseClass::Jump();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortal_Player::ZoomIn()
{
	SetFOV( this, 45.0f, 0.15f );
	m_PortalLocal.m_bZoomedIn = true;

	IGameEvent * event = gameeventmanager->CreateEvent( "player_zoomed" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		gameeventmanager->FireEvent( event );
	}
}

void CPortal_Player::ZoomOut()
{
	SetFOV( this, 0.0f, 0.15f );
	m_PortalLocal.m_bZoomedIn = false;

	IGameEvent * event = gameeventmanager->CreateEvent( "player_unzoomed" );
	if ( event )
	{
		event->SetInt( "userid", GetUserID() );
		gameeventmanager->FireEvent( event );
	}
}

bool CPortal_Player::IsZoomed( void )
{
	return m_PortalLocal.m_bZoomedIn;
}

void CPortal_Player::Event_Killed( const CTakeDamageInfo &info )
{
	m_PlayerGunTypeWhenDead = m_PlayerGunType;

	//update damage info with our accumulated physics force
	CTakeDamageInfo subinfo = info;
	subinfo.SetDamageForce( m_vecTotalBulletForce );

	// if we're burned by a laser, but we haven't been told to gib, that means that we had fractional damage done to us
	// and we died before it was caught in OnTakeDamage.  Gib them, plz.
	if ( GameRules()->IsMultiplayer() && !m_Shared.InCond( PORTAL_COND_DROWNING ) )
	{
		if ( mp_should_gib_bots.GetBool() && !m_Shared.InCond( PORTAL_COND_DEATH_GIB ) )
		{
			m_Shared.AddCond( PORTAL_COND_DEATH_GIB );
			m_Shared.m_damageInfo = info;
			Break( info.GetAttacker(), info );
		}
	}

#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	g_PortalGameStats.Event_PlayerDeath( this );
#endif

#ifndef _GAMECONSOLE
	g_Portal2ResearchDataTracker.IncrementDeath( this );
#endif // !_GAMECONSOLE

	m_bTauntRemoteView = false;

	// show killer in death cam mode
	// chopped down version of SetObserverTarget without the team check
	//if( info.GetAttacker() )
	//{
	//	// set new target
	//	m_hObserverTarget.Set( info.GetAttacker() );
	//}
	//else
	//	m_hObserverTarget.Set( NULL );

	// UpdateExpression();

#if !defined PORTAL_HIDE_PLAYER_RAGDOLL
	CreateRagdollEntity( info );
#else
	// Fizzle all portals so they don't see the player disappear
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
	CWeaponPortalgun* pPortalgun = (CWeaponPortalgun*)GetActiveWeapon();
	if( pPortalgun )
	{
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			//HACKISH: Do this before the chain to base... basecombatcharacer will dump the weapon
			// and then we won't know what linkage ID to fizzle. This is relevant in multiplayer,
			// where we want only the dying player's portals to fizzle.
			if( pTempPortal && ( pPortalgun->GetLinkageGroupID() == pTempPortal->GetLinkageGroup() ) )
			{
				pTempPortal->DeactivatePortalOnThink();
			}
		}
	}

	if ( GameRules()->IsMultiplayer() && mp_should_gib_bots.GetBool() )
	{
		for ( int i = 0; i < WeaponCount(); ++i )
		{
			// remove the weapon, we don't need it anymore and it shows up attached to the non visuble player in third person
			CBaseCombatWeapon *pWeapon = GetWeapon( i );

			if ( pWeapon == NULL )
				continue;

			UTIL_Remove( pWeapon );
		}
	}

#endif // PORTAL_HIDE_PLAYER_RAGDOLL

	BaseClass::Event_Killed( subinfo );

	SetUseKeyCooldownTime( 3.0f );

	if ( (info.GetDamageType() & DMG_DISSOLVE) && ( m_hRagdoll.Get() ) && !(m_hRagdoll.Get()->GetEFlags() & EFL_NO_DISSOLVE) )
	{
		m_hRagdoll->GetBaseAnimating()->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
	}

	m_lifeState = LIFE_DYING;

	if ( GetObserverTarget() )
	{
		//StartReplayMode( 3, 3, GetObserverTarget()->entindex() );
		//StartObserverMode( OBS_MODE_DEATHCAM );
	}

	if ( GameRules() && GameRules()->IsMultiplayer() )
	{
		CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, "@glados" );
		if ( pEntity )
		{
			char szScriptCommand[ 64 ];
			V_snprintf( szScriptCommand, sizeof( szScriptCommand ), "BotDeath(%i,%i)", ( GetTeamNumber() == TEAM_BLUE ? 2 : 1 ), info.GetDamageType() );
			pEntity->RunScript( szScriptCommand, "PlayerDied" );
		}

		CPortalMPStats *pStats = GetPortalMPStats();
		if ( pStats )
		{
			pStats->IncrementPlayerDeathsMap( this );
		}
	}

	FireConcept( TLK_PLAYER_KILLED );
}

int CPortal_Player::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo inputInfoCopy( inputInfo );

	// If you shoot yourself, make it hurt but push you less
	if ( inputInfoCopy.GetAttacker() == this && inputInfoCopy.GetDamageType() == DMG_BULLET )
	{
		inputInfoCopy.ScaleDamage( 5.0f );
		inputInfoCopy.ScaleDamageForce( 0.05f );
	}

	CBaseEntity *pAttacker = inputInfoCopy.GetAttacker();
	CBaseEntity *pInflictor = inputInfoCopy.GetInflictor();

	bool bIsTurret = false;

	if ( pAttacker && ( FClassnameIs( pAttacker, "npc_portal_turret_floor" ) ||
						FClassnameIs( pAttacker, "npc_hover_turret" ) ) )
	{
		bIsTurret = true;
		FireConcept( TLK_PLAYER_SHOT );
	}

	// Refuse damage from prop_glados_core.
	if ( (pAttacker && FClassnameIs( pAttacker, "prop_glados_core" )) ||
		(pInflictor && FClassnameIs( pInflictor, "prop_glados_core" ))  )
	{
		inputInfoCopy.SetDamage(0.0f);
	}

	if ( bIsTurret && ( inputInfoCopy.GetDamageType() & ( DMG_BULLET | DMG_ENERGYBEAM ) ) )
	{
		Vector vLateralForce = inputInfoCopy.GetDamageForce();
		vLateralForce.z = 0.0f;

		// Push if the player is moving against the force direction
		if ( GetAbsVelocity().Dot( vLateralForce ) < 0.0f )
			ApplyAbsVelocityImpulse( vLateralForce );
	}
	else if ( ( inputInfoCopy.GetDamageType() & DMG_CRUSH ) )
	{
		if ( bIsTurret )
		{
			inputInfoCopy.SetDamage( inputInfoCopy.GetDamage() * 0.5f );
		}

		if ( inputInfoCopy.GetDamage() >= 10.0f )
		{
			EmitSound( "PortalPlayer.BonkYelp" );
		}

	}
	else if ( ( inputInfoCopy.GetDamageType() & DMG_SHOCK ) || ( inputInfoCopy.GetDamageType() & DMG_BURN ) )
	{
		EmitSound( "Player.PainSmall" );
		FireConcept( TLK_PLAYER_BURNED );
	}

	// FIXME: This is a hold-over from old Portal behavior -- we should adjust the health to compensate! -- jdw
	inputInfoCopy.ScaleDamage( sk_dmg_take_scale1.GetFloat() );

	if ( inputInfoCopy.GetDamage() >= m_iHealth && GameRules()->IsMultiplayer() &&
		!m_Shared.InCond( PORTAL_COND_DROWNING ) )  // don't gib after drowning
	{
		if ( bIsTurret || (mp_should_gib_bots.GetBool() && !m_Shared.InCond( PORTAL_COND_DEATH_GIB )) )
		{
			m_Shared.AddCond( PORTAL_COND_DEATH_GIB );
			m_Shared.m_damageInfo = inputInfoCopy;
			Break( pAttacker, inputInfo );
		}
		else if ( !mp_should_gib_bots.GetBool() && !m_Shared.InCond( PORTAL_COND_DEATH_CRUSH ) )
		{
			m_Shared.AddCond( PORTAL_COND_DEATH_CRUSH );
			m_Shared.m_damageInfo = inputInfoCopy;
		}
	}

	if ( bIsTurret )
	{
		#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
		g_PortalGameStats.Event_TurretDamage( inputInfoCopy.GetDamage() );
		#endif
	}

	int ret = BaseClass::OnTakeDamage( inputInfoCopy );

	// Copy the multidamage damage origin over what the base class wrote, because
	// that gets translated correctly though portals.
	m_DmgOrigin = inputInfo.GetDamagePosition();

	// check if we are drowning in goo
	bool bGooDamage = ( inputInfoCopy.GetDamageType() & DMG_RADIATION ) > 0;
	if ( !( GameRules()->IsMultiplayer() && ( bGooDamage || m_Shared.InCond( PORTAL_COND_DEATH_CRUSH ) ) ) )
	{
		// Play a flinch!
		DoAnimationEvent( PLAYERANIMEVENT_FLINCH_CHEST, 0 );
	}

	return ret;
}

int CPortal_Player::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// set damage type sustained
	m_bitsDamageType |= info.GetDamageType();

	if ( !CBaseCombatCharacter::OnTakeDamage_Alive( info ) )
		return 0;

	CBaseEntity * attacker = info.GetAttacker();

	if ( !attacker )
		return 0;

	Vector vecDir = vec3_origin;
	if ( info.GetInflictor() )
	{
		vecDir = info.GetInflictor()->WorldSpaceCenter() - Vector ( 0, 0, 10 ) - WorldSpaceCenter();
		VectorNormalize( vecDir );
	}

	if ( info.GetInflictor() && (GetMoveType() == MOVETYPE_WALK) &&
		( !attacker->IsSolidFlagSet(FSOLID_TRIGGER)) )
	{
		Vector force = vecDir;// * -DamageForce( WorldAlignSize(), info.GetBaseDamage() );
		if ( force.z > 250.0f )
		{
			force.z = 250.0f;
		}
		ApplyAbsVelocityImpulse( force );
	}

	// fire global game event

	IGameEvent * event = gameeventmanager->CreateEvent( "player_hurt" );
	if ( event )
	{
		event->SetInt("userid", GetUserID() );
		event->SetInt("health", MAX(0, m_iHealth) );
		event->SetInt("priority", 5 );	// HLTV event priority, not transmitted

		if ( attacker->IsPlayer() )
		{
			CBasePlayer *player = ToBasePlayer( attacker );
			event->SetInt("attacker", player->GetUserID() ); // hurt by other player
		}
		else
		{
			event->SetInt("attacker", 0 ); // hurt by "world"
		}

		gameeventmanager->FireEvent( event );
	}

	// Insert a combat sound so that nearby NPCs hear battle
	if ( attacker->IsNPC() )
	{
		CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 512, 0.5, this );//<<TODO>>//magic number
	}

	return 1;
}

void CPortal_Player::Break( CBaseEntity *pBreaker, const CTakeDamageInfo &info )
{
	// don't ever break unless we're in multiplayer
	if ( !GameRules()->IsMultiplayer() )
		return;

	// do a screen shake
	UTIL_ScreenShake( GetAbsOrigin(), 7.0f, 100.0, 1.5, 500.0f, SHAKE_START, true );
	EmitSound( "CoopBot.Explode_Gib" );

	//m_takedamage = DAMAGE_NO;
	//m_OnBreak.FireOutput( pBreaker, this );

	Vector velocity;
	AngularImpulse angVelocity;
	IPhysicsObject *pPhysics = VPhysicsGetObject();

	Vector origin;
	QAngle angles;
	//AddSolidFlags( FSOLID_NOT_SOLID );
	if ( pPhysics )
	{
		pPhysics->GetVelocity( &velocity, &angVelocity );
		pPhysics->GetPosition( &origin, &angles );
		pPhysics->RecheckCollisionFilter();
	}
	else
	{
		velocity = GetAbsVelocity();
		QAngleToAngularImpulse( GetLocalAngularVelocity(), angVelocity );
		origin = GetAbsOrigin();
		angles = GetAbsAngles();
	}

	//PhysBreakSound( this, VPhysicsGetObject(), GetAbsOrigin() );

	// Allow derived classes to emit special things
	//OnBreak( velocity, angVelocity, pBreaker );

	breakablepropparams_t params( origin, angles, velocity, angVelocity );
	params.impactEnergyScale = m_impactEnergyScale;
	params.defCollisionGroup = GetCollisionGroup();
	if ( params.defCollisionGroup == COLLISION_GROUP_NONE )
	{
		// don't automatically make anything COLLISION_GROUP_NONE or it will
		// collide with debris being ejected by breaking
		params.defCollisionGroup = COLLISION_GROUP_INTERACTIVE;
	}

	params.defBurstScale = 100;
	// in multiplayer spawn break models as clientside temp ents

	CPASFilter filter( WorldSpaceCenter() );

	//Vector velocity; velocity.Init();

	if ( pPhysics )
		pPhysics->GetVelocity( &velocity, NULL );

	/*
	switch ( GetMultiplayerBreakMode() )
	{
	case MULTIPLAYER_BREAK_DEFAULT:		// default is to break client-side
	case MULTIPLAYER_BREAK_CLIENTSIDE:
		te->PhysicsProp( filter, -1, GetModelIndex(), m_nSkin, GetAbsOrigin(), GetAbsAngles(), velocity, true, GetEffects() );
		break;
	case MULTIPLAYER_BREAK_SERVERSIDE:	// server-side break
		if ( m_PerformanceMode != PM_NO_GIBS || breakable_disable_gib_limit.GetBool() )
		{
			PropBreakableCreateAll( GetModelIndex(), pPhysics, params, this, -1, ( m_PerformanceMode == PM_FULL_GIBS ), false );
		}
		break;
	case MULTIPLAYER_BREAK_BOTH:	// pieces break from both dlls
		te->PhysicsProp( filter, -1, GetModelIndex(), m_nSkin, GetAbsOrigin(), GetAbsAngles(), velocity, true, GetEffects() );
		if ( m_PerformanceMode != PM_NO_GIBS || breakable_disable_gib_limit.GetBool() )
		{
			PropBreakableCreateAll( GetModelIndex(), pPhysics, params, this, -1, ( m_PerformanceMode == PM_FULL_GIBS ), false );
		}
		break;
	}

	// no damage/damage force? set a burst of 100 for some movement
	else if ( m_PerformanceMode != PM_NO_GIBS || breakable_disable_gib_limit.GetBool() )
	{
		PropBreakableCreateAll( GetModelIndex(), pPhysics, params, this, -1, ( m_PerformanceMode == PM_FULL_GIBS ) );
	}
	*/

	te->PhysicsProp( filter, -1, GetModelIndex(), m_nSkin, GetAbsOrigin(), GetAbsAngles(), velocity, true, GetEffects() );

	//UTIL_Remove( this );
}

//
//void CPortal_Player::UnDuck( void )
//{
//	if( m_Local.m_bDucked != false )
//	{
//		m_Local.m_bDucked = false;
//		UnforceButtons( IN_DUCK );
//		RemoveFlag( FL_DUCKING );
//		SetVCollisionState( GetAbsOrigin(), GetAbsVelocity(), VPHYS_WALK );
//	}
//}


//--------------------------------------------------------------------------------------------------
// Disable the use key for the specified time (in seconds)
//--------------------------------------------------------------------------------------------------
void CPortal_Player::SetUseKeyCooldownTime( float flCooldownDuration )
{
	const float flMaxCooldownDuration = 120.0f;
	Assert( flCooldownDuration < flMaxCooldownDuration );
	flCooldownDuration = clamp ( flCooldownDuration, 0.0f, flMaxCooldownDuration );
	m_flUseKeyCooldownTime = gpGlobals->curtime + flCooldownDuration;
}


void CPortal_Player::SetForcedGrabControllerType( ForcedGrabControllerType type )
{
	m_ForcedGrabController = type;
}


void CPortal_Player::UpdateVMGrab( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	switch( m_ForcedGrabController )
	{
	case FORCE_GRAB_CONTROLLER_VM:
		{
			m_bUseVMGrab = true;
		}
		break;
	case FORCE_GRAB_CONTROLLER_PHYSICS:
		{
			m_bUseVMGrab = false;
		}
		break;
	case FORCE_GRAB_CONTROLLER_DEFAULT:
	default:
		{
#if 0	// Turning this off for now... the FOV differences cause a visual pop that people are bugging
			// and we might not be getting that much for having it not collide while flinging.
			if ( GetAbsVelocity().Length() > 350.0f )
			{
				// Going fast! Use VM
				m_bUseVMGrab = true;
				return;
			}
#endif

			if ( FClassnameIs( pEntity, "npc_portal_turret_floor" ) )
			{
				// It's a turret, use physics
				m_bUseVMGrab = false;
				return;
			}

			if ( GameRules() && GameRules()->IsMultiplayer() )
			{
				// In MP our reflective cubes need to go physics when touching or near a laser
				if ( UTIL_IsReflectiveCube( pEntity ) ||  UTIL_IsSchrodinger( pEntity ) )
				{
					CPropWeightedCube *pCube = (CPropWeightedCube *)pEntity;
					if ( pCube->HasLaser() )
					{
						// VMGrabController'd laser cubes that were grabbed through portals still think they are being
						// held through portals.  Clear that out here, so when it comes time to ComputeError, we won't get
						// a bogus error and force drop the cube.
						SetHeldObjectOnOppositeSideOfPortal( false );
						SetHeldObjectPortal( NULL );

						// It's redirecting a laser, use physics
						m_bUseVMGrab = false;
						return;
					}
					else
					{
						// Check it's distance to each laser line so the depth renders properly when the laser is between the player and cube
						for ( int i = 0; i < IPortalLaserAutoList::AutoList().Count(); ++i )
						{
							CPortalLaser *pLaser = static_cast< CPortalLaser* >( IPortalLaserAutoList::AutoList()[ i ] );
							if ( pLaser )
							{
								Vector vClosest = pLaser->ClosestPointOnLineSegment( pCube->GetAbsOrigin() );
								if ( vClosest.DistToSqr( pCube->GetAbsOrigin() ) < ( 80.0f * 80.0f ) )
								{
									// VMGrabController'd laser cubes that were grabbed through portals still think they are being
									// held through portals.  Clear that out here, so when it comes time to ComputeError, we won't get
									// a bogus error and force drop the cube.
									SetHeldObjectOnOppositeSideOfPortal( false );
									SetHeldObjectPortal( NULL );

									// It's close to the laser
									m_bUseVMGrab = false;
									return;
								}
							}
						}
					}
				}

				// Multiplayer uses VM otherwise
				m_bUseVMGrab = true;
				return;
			}

			if ( FClassnameIs( pEntity, "npc_personality_core" ) )
			{
				// It's a personality core, use VM mode
				m_bUseVMGrab = true;
				return;
			}

			m_bUseVMGrab = false;
		}
		break;

	} //Switch forced grab controller
}

void CPortal_Player::IncrementPortalsPlaced( bool bSecondaryPortal )
{
	if ( bSecondaryPortal )
	{
		FirePlayerProxyOutput( "OnSecondaryPortalPlaced", variant_t(), this, this );
	}
	else
	{
		FirePlayerProxyOutput( "OnPrimaryPortalPlaced", variant_t(), this, this );
	}

	m_StatsThisLevel.iNumPortalsPlaced++;

#if !defined( _GAMECONSOLE )
	g_Portal2ResearchDataTracker.IncrementPortalFired( this );
#endif // !defined( _GAMECONSOLE ) && !defined( NO_STEAM )

	if ( m_iBonusChallenge == PORTAL_CHALLENGE_PORTALS )
		SetBonusProgress( static_cast<int>( m_StatsThisLevel.iNumPortalsPlaced ) );
}

void CPortal_Player::IncrementStepsTaken( void )
{
	m_StatsThisLevel.iNumStepsTaken++;
	if( GetPortalMPStats() )
	{
		GetPortalMPStats()->IncrementPlayerSteps( this );
	}

#if !defined( _GAMECONSOLE )
	g_Portal2ResearchDataTracker.IncrementStepsTaken( this );
#endif // !defined( _GAMECONSOLE ) && !defined( NO_STEAM )

	if ( m_iBonusChallenge == PORTAL_CHALLENGE_STEPS )
		SetBonusProgress( static_cast<int>( m_StatsThisLevel.iNumStepsTaken ) );
}


void CPortal_Player::IncrementDistanceTaken( void )
{
	m_StatsThisLevel.fDistanceTaken += VectorLength( GetAbsOrigin() - m_vPrevPosition );
	m_vPrevPosition = GetAbsOrigin();
}


void CPortal_Player::UpdateSecondsTaken( void )
{
	float fSecondsSinceLastUpdate = ( gpGlobals->curtime - m_fTimeLastNumSecondsUpdate );
	m_StatsThisLevel.fNumSecondsTaken += fSecondsSinceLastUpdate;
	m_fTimeLastNumSecondsUpdate = gpGlobals->curtime;

	if ( m_iBonusChallenge == PORTAL_CHALLENGE_TIME )
		SetBonusProgress( static_cast<int>( m_StatsThisLevel.fNumSecondsTaken ) );

	if ( m_fNeuroToxinDamageTime > 0.0f )
	{
		float fTimeRemaining = m_fNeuroToxinDamageTime - gpGlobals->curtime;

		if ( fTimeRemaining < 0.0f )
		{
			CTakeDamageInfo info;
			info.SetDamage( gpGlobals->frametime * 50.0f );
			info.SetDamageType( DMG_NERVEGAS );
			TakeDamage( info );
			fTimeRemaining = 0.0f;
		}

		PauseBonusProgress( false );
		SetBonusProgress( static_cast<int>( fTimeRemaining ) );
	}
}

void CPortal_Player::ResetThisLevelStats( void )
{
	m_StatsThisLevel.iNumPortalsPlaced = 0;
	m_StatsThisLevel.iNumStepsTaken = 0;
	m_StatsThisLevel.fNumSecondsTaken = 0.0f;
	m_StatsThisLevel.fDistanceTaken = 0.0f;

	if ( m_iBonusChallenge != PORTAL_CHALLENGE_NONE )
		SetBonusProgress( 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Update the area bits variable which is networked down to the client to determine
//			which area portals should be closed based on visibility.
// Input  : *pvs - pvs to be used to determine visibility of the portals
//-----------------------------------------------------------------------------
void CPortal_Player::UpdatePortalViewAreaBits( unsigned char *pvs, int pvssize )
{
	Assert ( pvs );

	int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
		return;

	CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
	int *portalArea = (int *)stackalloc( sizeof( int ) * iPortalCount );
	bool *bUsePortalForVis = (bool *)stackalloc( sizeof( bool ) * iPortalCount );

	unsigned char *portalTempBits = (unsigned char *)stackalloc( sizeof( unsigned char ) * 32 * iPortalCount );
	COMPILE_TIME_ASSERT( (sizeof( unsigned char ) * 32) >= sizeof( ((CPlayerLocalData*)0)->m_chAreaBits ) );

	// setup area bits for these portals
	for ( int i = 0; i < iPortalCount; ++i )
	{
		CPortal_Base2D* pLocalPortal = pPortals[ i ];
		// Make sure this portal is active before adding it's location to the pvs
		if ( pLocalPortal && pLocalPortal->IsActive() )
		{
			CPortal_Base2D* pRemotePortal = pLocalPortal->m_hLinkedPortal.Get();

			// Make sure this portal's linked portal is in the PVS before we add what it can see
			if ( pRemotePortal && pRemotePortal->IsActivedAndLinked() && pRemotePortal->NetworkProp() &&
				pRemotePortal->NetworkProp()->IsInPVS( edict(), pvs, pvssize ) )
			{
				portalArea[ i ] = engine->GetArea( pPortals[ i ]->GetAbsOrigin() );

				if ( portalArea [ i ] >= 0 )
				{
					bUsePortalForVis[ i ] = true;
				}

				engine->GetAreaBits( portalArea[ i ], &portalTempBits[ i * 32 ], sizeof( unsigned char ) * 32 );
			}
		}
	}

	// Use the union of player-view area bits and the portal-view area bits of each portal
	for ( int i = 0; i < m_Local.m_chAreaBits.Count(); i++ )
	{
		for ( int j = 0; j < iPortalCount; ++j )
		{
			// If this portal is active, in PVS and it's location is valid
			if ( bUsePortalForVis[ j ]  )
			{
				m_Local.m_chAreaBits.Set( i, m_Local.m_chAreaBits[ i ] | portalTempBits[ (j * 32) + i ] );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Recursive function to add any areas seen by portals in the pViewEnt's pvs (non-portal effected pvs) to the engine's fat pvs and networked area list.
// Input  : *pViewEnt - The viewing ent. All portals in this entity's pvs will have their areas networked.
//			area - area of the pViewEnt
//			*pvs - pvs bytes passed from the engine
//			pvssize - size of pvs in bytes
//			vec_AreasNetworked - List of areas already marked to prevent 1->2->1 re-enterancy.
//-----------------------------------------------------------------------------
void PortalSetupVisibility( CBaseEntity *pViewEnt, const Vector &vViewOrigin, unsigned char *pvs, int pvssize )
{
	CPVS_Extender::ComputeExtendedPVS( pViewEnt, vViewOrigin, pvs, pvssize, 10 );
}

void CPortal_Player::SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize )
{
	BaseClass::SetupVisibility( pViewEntity, pvs, pvssize );

	int area = pViewEntity ? pViewEntity->NetworkProp()->AreaNum() : NetworkProp()->AreaNum();

	// At this point the EyePosition has been added as a view origin, but if we are currently stuck
	// in a portal, our EyePosition may return a point in solid. Find the reflected eye position
	// and use that as a vis origin instead.
	if ( m_hPortalEnvironment )
	{
		CPortal_Base2D *pPortal = NULL, *pRemotePortal = NULL;
		pPortal = m_hPortalEnvironment;
		pRemotePortal = pPortal->m_hLinkedPortal;

		if ( pPortal && pRemotePortal && pPortal->IsActive() && pRemotePortal->IsActive() )
		{
			Vector ptPortalCenter = pPortal->GetAbsOrigin();
			Vector vPortalForward;
			pPortal->GetVectors( &vPortalForward, NULL, NULL );

			Vector eyeOrigin = EyePosition();
			Vector vEyeToPortalCenter = ptPortalCenter - eyeOrigin;

			float fPortalDist = vPortalForward.Dot( vEyeToPortalCenter );
			if( fPortalDist > 0.0f ) //eye point is behind portal
			{
				// Move eye origin to it's transformed position on the other side of the portal
				UTIL_Portal_PointTransform( pPortal->MatrixThisToLinked(), eyeOrigin, eyeOrigin );

				// Use this as our view origin (as this is where the client will be displaying from)
				engine->AddOriginToPVS( eyeOrigin );
				if ( !pViewEntity || pViewEntity->IsPlayer() )
				{
					area = engine->GetArea( eyeOrigin );
				}
			}
		}
	}

	PointCameraSetupVisibility( this, area, pvs, pvssize );

	PortalSetupVisibility( this, EyePosition(), pvs, pvssize );

	if ( m_bClientCheckPVSDirty )
	{
		UTIL_SetClientCheckPVS( edict(), pvs, pvssize );
		m_bClientCheckPVSDirty = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CPortal_Player::FindRemoteTauntViewpoint( Vector *pOriginOut, QAngle *pAnglesOut )
{
	const float flRadius = (38*12);
	CBaseEntity *pRemoteViewer = gEntList.FindEntityByClassnameNearest( "npc_security_camera", EyePosition(), flRadius );
	if ( pRemoteViewer == NULL )
		return false;

	CNPC_SecurityCamera *pNPC = assert_cast<CNPC_SecurityCamera *>(pRemoteViewer);
	if ( pNPC == NULL || !pNPC->IsActive() )
		return false;

	if ( pOriginOut && pAnglesOut )
	{
		if ( !pNPC->GetAttachment( "lens", *pOriginOut, *pAnglesOut ) )
			return false;

		Vector vecCam = *pOriginOut;

		// check if the camera is in line of sight
		trace_t tr;
		UTIL_TraceLine( vecCam, WorldSpaceCenter(), MASK_SHOT, pRemoteViewer, COLLISION_GROUP_NONE, &tr );
		if ( tr.m_pEnt != this )
			return false;

		Vector vecDir;
		VectorSubtract( WorldSpaceCenter(), vecCam, vecDir );
		VectorNormalize( vecDir );
		*pOriginOut = vecCam + (vecDir*32);
		VectorAngles( vecDir, *pAnglesOut );

		m_hRemoteTauntCamera = pNPC;
		pNPC->TauntedByPlayer( this );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortal_Player::Taunt( const char *pchTauntForce /*=NULL*/, bool bAuto /*= false*/ )
{
	// This doesn't work in singleplayer
	if ( g_pGameRules->IsMultiplayer() == false )
		return;

	// No taunting while holding stuff because it looks dumb, and if we drop that stuff it might get stuck behind walls
	if ( IsHoldingEntity( NULL ) )
		return;

	if ( !bAuto && m_bTauntDisabled )
	{
		return;
	}

	bool bTeamAccept = ( pchTauntForce && V_strcmp( pchTauntForce, "team_accept" ) == 0 );
	bool bTease = ( pchTauntForce && V_strcmp( pchTauntForce, "teamtease" ) == 0 );

	// Don't re-taunt!
	if ( m_Shared.InCond( PORTAL_COND_TAUNTING ) && !bTeamAccept )
		return;

	if ( m_nTeamTauntState == TEAM_TAUNT_SUCCESS )
		return;

	if ( !pchTauntForce )
	{
		m_szTauntForce.GetForModify()[ 0 ] = '\0';
		SetTeamTauntState( TEAM_TAUNT_NONE );
	}
	else
	{
		// Don't hold objects while doing non air taunts
		ForceDropOfCarriedPhysObjects( NULL );

		if ( V_strcmp( pchTauntForce, "item" ) == 0 )
		{
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
			// FIXME: Use the item to decide what taunt to do
			CEconItemView *pItem = GetItemInLoadoutSlot( LOADOUT_POSITION_GESTURE );
			if ( pItem && pItem->IsValid() )
			{
				pchTauntForce = pItem->GetStaticData()->GetBrassModelOverride();
			}
			else
#endif
			{
				pchTauntForce = mp_taunt_item.GetString();
			}
		}

		if ( bTeamAccept || bTease )
		{
			CPortal_Player *pOtherPlayer = ToPortalPlayer( UTIL_OtherConnectedPlayer( this ) );
			if ( pOtherPlayer )
			{
				if ( bTease )
				{
					if ( GetDistanceToEntity( pOtherPlayer ) < 150.0f )
					{
						V_strncpy( pOtherPlayer->m_szTauntForce.GetForModify(), GetTeamNumber() == TEAM_BLUE ? "TeamBallTease" : "TeamEggTease", PORTAL2_MP_TEAM_TAUNT_FORCE_LENGTH );
						bTeamAccept = true;
					}
					else
					{
						V_strncpy( m_szTauntForce.GetForModify(), pchTauntForce, PORTAL2_MP_TEAM_TAUNT_FORCE_LENGTH );
						SetTeamTauntState( TEAM_TAUNT_NONE );
					}
				}

				if ( bTeamAccept )
				{
					// Copy their taunt
					V_strncpy( m_szTauntForce.GetForModify(), pOtherPlayer->m_szTauntForce.Get(), PORTAL2_MP_TEAM_TAUNT_FORCE_LENGTH );

					if ( PortalMPGameRules() )
					{
						PortalMPGameRules()->ShuffleRPSOutcome();
					}

					if ( SolveTeamTauntPositionAndAngles( pOtherPlayer ) )
					{
						SetTeamTauntState( TEAM_TAUNT_SUCCESS );

						CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, "@glados" );
						if ( pEntity )
						{
							char szScriptCommand[ 64 ];
							V_snprintf( szScriptCommand, sizeof( szScriptCommand ), "CoopBotAnimation(%i,\"%s\")", ( GetTeamNumber() == TEAM_BLUE ? 2 : 1 ), m_szTauntForce.Get() );
							pEntity->RunScript( szScriptCommand, "BotAnimationCommand" );
						}

						// On server notify all players that team taunt happened
						KeyValues *kvNotifyTaunt = new KeyValues( "OnCoopBotTaunt" );
						kvNotifyTaunt->SetString( "taunt", m_szTauntForce.Get() );
						UTIL_SendClientCommandKVToPlayer( kvNotifyTaunt );

						// Track multiplayer stats for successful team taunts
						if( GetPortalMPStats() )
						{
							GetPortalMPStats()->TeamTauntSuccess( pOtherPlayer->m_szTauntForce.Get() );
						}
					}
					else
					{
						// Final positions weren't valid
						return;
					}
				}
			}
		}
		else
		{
			V_strncpy( m_szTauntForce.GetForModify(), pchTauntForce, PORTAL2_MP_TEAM_TAUNT_FORCE_LENGTH );
			SetTeamTauntState( TEAM_TAUNT_NONE );

			CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, "@glados" );
			if ( pEntity )
			{
				char szScriptCommand[ 64 ];
				V_snprintf( szScriptCommand, sizeof( szScriptCommand ), "CoopBotAnimation(%i,\"%s\")", ( GetTeamNumber() == TEAM_BLUE ? 2 : 1 ), m_szTauntForce.Get() );
				pEntity->RunScript( szScriptCommand, "BotAnimationCommand" );
			}
		}
	}

#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	// Record taunts used in gamestats (jeep says null means airtaunt)
	g_PortalGameStats.Event_PlayerTaunt( this, pchTauntForce ? pchTauntForce : "airtaunt" );
#endif //!defined( _GAMECONSOLE )

	StartTaunt();
}

bool CPortal_Player::SolveTeamTauntPositionAndAngles( CPortal_Player *pInitiator )
{
	if ( ValidateTeamTaunt( pInitiator, pInitiator->m_vTauntPosition.GetForModify(), pInitiator->m_vTauntAngles.GetForModify(),
										m_vTauntPosition.GetForModify(), m_vTauntAngles.GetForModify() ) )
	{
		// Start initiator's taunt
		pInitiator->SetTeamTauntState( TEAM_TAUNT_SUCCESS );
		pInitiator->StartTaunt();

		return true;
	}

	return false;
}


bool CPortal_Player::ValidateTeamTaunt( CPortal_Player *pInitiator, Vector &vInitiatorPos, QAngle &angInitiatorAng, Vector &vAcceptorPos, QAngle &angAcceptorAng, bool bRecursed /*= false*/ )
{
	if ( !pInitiator )
		return false;

	if ( !pInitiator->GetGroundEntity() )
		return false;

	if ( !GetGroundEntity() )
		return false;

	if ( GetAbsOrigin().DistTo( pInitiator->GetAbsOrigin() ) > 250.0f )
	{
		return false;
	}

	// Don't validate this team taunt if the two bots' z position varies by too much, even if they are close.
	float flDeltaZ = fabs( GetAbsOrigin().z - pInitiator->GetAbsOrigin().z );
	if ( flDeltaZ > ALLOWED_TEAM_TAUNT_Z_DIST )
	{
		return false;
	}

	// Don't validate this team taunt if something solid is in the way.
	trace_t wall_check_tr;
	UTIL_TraceLine( WorldSpaceCenter(), pInitiator->WorldSpaceCenter(), MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &wall_check_tr );
	if ( wall_check_tr.fraction < 1.0f && wall_check_tr.m_pEnt != pInitiator )
	{
		return false;
	}

	bool bValid = true;

	// Get the interaction params from ballbot
	CUtlVector<ScriptedNPCInteraction_t> *pScriptedInteractions = NULL;
	if ( GetTeamNumber() == TEAM_BLUE )
	{
		pScriptedInteractions = GetScriptedInteractions();
	}
	else
	{
		pScriptedInteractions = pInitiator->GetScriptedInteractions();
	}

	ScriptedNPCInteraction_t *pInteraction = NULL;
	for ( int nInteraction = 0; nInteraction < pScriptedInteractions->Count(); nInteraction++ )
	{
		ScriptedNPCInteraction_t *pTempInteraction = &((*pScriptedInteractions)[ nInteraction ]);
		if ( Q_stricmp( pTempInteraction->sPhases[SNPCINT_SEQUENCE].iszSequence.ToCStr(), pInitiator->m_szTauntForce.Get() ) == 0 )
		{
			pInteraction = pTempInteraction;
			break;
		}
	}

	// Reference forward is initiator to acceptor
	Vector vForward = GetAbsOrigin() - pInitiator->GetAbsOrigin();
	vForward.z = 0.0f;
	VectorNormalize( vForward );

	// Rotate initiator, leave him in position
	QAngle angNew;
	VectorAngles( vForward, angNew );
	angInitiatorAng = angNew;
	vInitiatorPos = pInitiator->GetAbsOrigin();

	if ( !pInteraction )
	{
		// Couldn't find an interaction! Make them overlap so we know there's a bug!
		VectorAngles( vForward, angNew );
		angAcceptorAng = angNew;
		vAcceptorPos = vInitiatorPos;
	}
	else
	{
		// Rot matrix for forward to initiator
		matrix3x4_t matToInitiator;
		AngleMatrix( angInitiatorAng, matToInitiator );

		// Build acceptor taunt angles
		Vector vRelativeForward;
		QAngle ang = pInteraction->angRelativeAngles;
		ang[ YAW ] *= -1.0f;

		if ( GetTeamNumber() == TEAM_BLUE )
		{
			// Invert the angles
			ang = -ang;
		}
		AngleVectors( ang, &vRelativeForward );

		Vector vRelativeForwardOffset;
		VectorTransform( vRelativeForward, matToInitiator, vRelativeForwardOffset );
		VectorAngles( vRelativeForwardOffset, angAcceptorAng );

		if ( GetTeamNumber() == TEAM_BLUE )
		{
			// Rot matrix for forward to acceptor
			AngleMatrix( angAcceptorAng, matToInitiator );
		}

		// Build acceptor taunt position
		Vector vAcceptorRelativeOriginOffset;
		Vector vec = pInteraction->vecRelativeOrigin;
		vec.y *= -1.0f;
		VectorTransform( vec, matToInitiator, vAcceptorRelativeOriginOffset );

		if ( GetTeamNumber() == TEAM_BLUE )
		{
			// Opposite offset
			vAcceptorPos = vInitiatorPos - vAcceptorRelativeOriginOffset;
		}
		else
		{
			vAcceptorPos = vInitiatorPos + vAcceptorRelativeOriginOffset;
		}

		// Make sure position touches floor
		trace_t tr;
		CTraceFilterSkipTwoEntities filter( this, pInitiator );
		Ray_t ray;
		ray.Init( vAcceptorPos + Vector( 0.0f, 0.0f, 1.0f ), vAcceptorPos + Vector( 0.0f, 0.0f, -ALLOWED_TEAM_TAUNT_Z_DIST ), GetPlayerMins(), GetPlayerMaxs() );
		UTIL_TraceRay( ray, MASK_PLAYERSOLID, &filter, &tr );

		if ( tr.fraction == 1.f )
			return false; // No ground beneath this target location, we would be floating in the air!

		if ( tr.startsolid || tr.fraction <= 0.0f )
		{
			// Scoot it up if needed
			ray.Init( vAcceptorPos + Vector( 0.0f, 0.0f, 50.0f ), vAcceptorPos, GetPlayerMins(), GetPlayerMaxs() );
			UTIL_TraceRay( ray, MASK_PLAYERSOLID, &filter, &tr );

			if ( tr.startsolid || tr.fraction <= 0.0f )
			{
				bValid = false;
			}
		}

		if ( bValid )
		{
			vAcceptorPos = tr.endpos;

			UTIL_TraceLine( vAcceptorPos + ( GetPlayerMins() + GetPlayerMaxs() ) * 0.5f, pInitiator->WorldSpaceCenter(), MASK_PLAYERSOLID, this, COLLISION_GROUP_NONE, &tr );

			if ( tr.fraction < 1.0f && tr.m_pEnt != pInitiator )
			{
				bValid = false;
			}
		}
	}

	if ( !bValid && !bRecursed )
	{
		// Try the inversion where the initiator moves and the acceptor holds still
		bValid = pInitiator->ValidateTeamTaunt( this, vAcceptorPos, angAcceptorAng, vInitiatorPos, angInitiatorAng, true );
	}

	return bValid;
}

void CPortal_Player::StartTaunt( void )
{
	CBaseEntity *pOldRemoteTauntCamera = m_hRemoteTauntCamera.Get();

	char szResponse[AI_Response::MAX_RESPONSE_NAME];
	if ( SpeakConceptIfAllowed( MP_CONCEPT_PLAYER_TAUNT, NULL, szResponse, AI_Response::MAX_RESPONSE_NAME ) )
	{
		// Get the duration of the scene.
		float flDuration = GetSceneDuration( szResponse );

		m_vPreTauntAngles = EyeAngles();

		// See if there's a security camera around, and if so, use it as our viewing target
		m_bTauntRemoteView = FindRemoteTauntViewpoint( &m_vecRemoteViewOrigin.GetForModify(), &m_vecRemoteViewAngles.GetForModify() );

		if ( m_bTauntRemoteView )
		{
			// Go into camera view
			m_bTauntRemoteViewFOVFixup = true;

			if ( !pOldRemoteTauntCamera )
			{
				SetFOV( this, 125, 0.0f );	// previous value was 110
				SetFOV( this, 105, 1.0f );	// previous value was 90
			}

			// let Glados know that the player is taunting a camera
			CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, "@glados" );
			if ( pEntity )
			{
				int nTeam = GetTeamNumber();
				pEntity->RunScript( UTIL_VarArgs( "PlayerTauntCamera(%i,\"%s\")", (nTeam == TEAM_BLUE) ? 2 : 1, m_szTauntForce.Get() ), "StartTaunt" );
			}

			UTIL_RecordAchievementEvent( UTIL_VarArgs( "ACH.TAUNT_CAMERA[%i]", PortalMPGameRules()->GetCoopSection() ), this );
		}
		else
		{
			// Taunt Defaults
			m_fTauntCameraDistance = portal_tauntcam_dist.GetFloat();
			m_vecRemoteViewAngles = QAngle( 20.0f, 160.0f, 0.0f );

			if ( m_szTauntForce[ 0 ] != '\0' )
			{
				CUtlVector<ScriptedNPCInteraction_t> *pScriptedInteractions = GetScriptedInteractions();

				for ( int nInteraction = 0; nInteraction < pScriptedInteractions->Count(); nInteraction++ )
				{
					ScriptedNPCInteraction_t *pInteraction = &((*pScriptedInteractions)[ nInteraction ]);
					if ( Q_stricmp( pInteraction->sPhases[SNPCINT_SEQUENCE].iszSequence.ToCStr(), m_szTauntForce.Get() ) == 0 )
					{
						// Custom camera angles
						m_fTauntCameraDistance = pInteraction->flCameraDistance;
						m_vecRemoteViewAngles = pInteraction->angCameraAngles;
						break;
					}
				}
			}
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "player_gesture" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetBool( "air", false );

			gameeventmanager->FireEvent( event );
		}

		m_Shared.m_flTauntRemoveTime = gpGlobals->curtime + flDuration + 0.5f;
		m_Shared.AddCond( PORTAL_COND_TAUNTING );

		// Check for circumstances to award the 'You monster' achievement.
		CBaseEntity* pEnt = NULL;
		while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "npc_portal_turret_floor" ) ) != NULL )
		{
			if ( PointWithinViewAngle( EyePosition(), pEnt->WorldSpaceCenter(), EyeDirection3D(), 0.85f ) )
			{
				CNPC_Portal_FloorTurret *pTurret = (CNPC_Portal_FloorTurret*)pEnt;
				Assert ( pTurret );
				if ( pTurret && pTurret->IsProjectedWallBlockingTurretFromPlayer( this ) )
				{
					UTIL_RecordAchievementEvent( "ACH.YOU_MONSTER", this );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CPortal_Player::IsHoldingEntity( CBaseEntity *pEnt )
{
	return PlayerPickupControllerIsHoldingEntity( m_hUseEntity, pEnt );
}

//-----------------------------------------------------------------------------
// Purpose: Queues up a use deny sound, played in ItemPostFrame.
//-----------------------------------------------------------------------------
void CPortal_Player::PlayUseDenySound()
{
	m_bPlayUseDenySound = true;
}

//---------------------------------------------------------
// Purpose:
//---------------------------------------------------------
Vector CPortal_Player::EyeDirection2D( void )
{
	Vector vecReturn = EyeDirection3D();
	vecReturn.z = 0;
	vecReturn.AsVector2D().NormalizeInPlace();

	return vecReturn;
}

//---------------------------------------------------------
// Purpose:
//---------------------------------------------------------
Vector CPortal_Player::EyeDirection3D( void )
{
	Vector vecForward;
	AngleVectors( EyeAngles(), &vecForward );
	return vecForward;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CPortal_Player::GetHeldObjectMass( IPhysicsObject *pHeldObject )
{
	float mass = PlayerPickupGetHeldObjectMass( m_hUseEntity, pHeldObject );
	if ( mass == 0.0f )
	{
		mass = PhysCannonGetHeldObjectMass( GetActiveWeapon(), pHeldObject );
	}
	return mass;
}

extern CBaseEntity *GetCoopSpawnLocation( int iTeam ); //in info_coop_spawn.cpp

CBaseEntity* CPortal_Player::EntSelectSpawnPoint( void )
{
	if ( !g_pGameRules->IsMultiplayer() )
	{
		return BaseClass::EntSelectSpawnPoint();
	}

	if( g_pGameRules->IsCoOp() )
	{
		switch( GetTeamNumber() )
		{
		case TEAM_UNASSIGNED:
		//case TEAM_SPECTATOR:
			PickTeam();
		}

		CBaseEntity *pSpawnLocation = GetCoopSpawnLocation( GetTeamNumber() );
		if( pSpawnLocation )
			return pSpawnLocation;
	}

	CBaseEntity *pSpot = NULL;
	CBaseEntity *pLastSpawnPoint = g_pLastSpawn;
	const char *pSpawnpointName = "info_player_start";

	pSpot = pLastSpawnPoint;
	// Randomize the start spot
	for ( int i = random->RandomInt(1,5); i > 0; i-- )
		pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );
	if ( !pSpot )  // skip over the null point
		pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );

	CBaseEntity *pFirstSpot = pSpot;

	do
	{
		if ( pSpot )
		{
			// check if pSpot is valid
			if ( g_pGameRules->IsSpawnPointValid( pSpot, this ) )
			{
				if ( pSpot->GetLocalOrigin() == vec3_origin )
				{
					pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );
					continue;
				}

				// if so, go to pSpot
				goto ReturnSpot;
			}
		}
		// increment pSpot
		pSpot = gEntList.FindEntityByClassname( pSpot, pSpawnpointName );
	} while ( pSpot != pFirstSpot ); // loop if we're not back to the start

#if 0
	// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
	edict_t		*player = edict();
	if ( pSpot )
	{
		CBaseEntity *ent = NULL;
		for ( CEntitySphereQuery sphere( pSpot->GetAbsOrigin(), 128 ); (ent = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
		{
			// if ent is a client, kill em (unless they are ourselves)
			if ( ent->IsPlayer() && !(ent->edict() == player) )
				ent->TakeDamage( CTakeDamageInfo( GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), 300, DMG_GENERIC ) );
		}
		goto ReturnSpot;
	}
#endif // 0

	if ( !pSpot  )
	{
		pSpot = gEntList.FindEntityByClassname( pSpot, "info_player_start" );

		if ( pSpot )
			goto ReturnSpot;
	}

ReturnSpot:

	g_pLastSpawn = pSpot;

	//m_flSlamProtectTime = gpGlobals->curtime + 0.5;

	return pSpot;
}


static int s_CoopTeamAssignments[MAX_PLAYERS] = { 0 };

void CPortal_Player::PickTeam( void )
{
	int iIndex = ENTINDEX( this ) - 1;
	if( g_pGameRules->IsCoOp() && (s_CoopTeamAssignments[iIndex] != 0) )
	{
		ChangeTeam( s_CoopTeamAssignments[iIndex] );
		return;
	}

	//picks lowest or random
	CTeam *pRed = g_Teams[TEAM_RED];
	CTeam *pBlue = g_Teams[TEAM_BLUE];

	if ( pRed->GetNumPlayers() > pBlue->GetNumPlayers() )
	{
		ChangeTeam( TEAM_BLUE );
	}
	else if ( pRed->GetNumPlayers() < pBlue->GetNumPlayers() )
	{
		ChangeTeam( TEAM_RED );
	}
	else
	{
		if ( mp_server_player_team.GetInt() == 0 )
		{
			ChangeTeam( TEAM_BLUE );
		}
		else if ( mp_server_player_team.GetInt() == 1 )
		{
			ChangeTeam( TEAM_RED );
		}
		else
		{
			ChangeTeam( random->RandomInt( TEAM_RED, TEAM_BLUE ) );
		}
	}


	if( g_pGameRules->IsCoOp() )
	{
		s_CoopTeamAssignments[iIndex] = GetTeamNumber();
	}
}

void CPortal_Player::ClientDisconnected( edict_t *pPlayer )
{
	s_CoopTeamAssignments[ENTINDEX( pPlayer ) - 1] = 0;
}

void CPortal_Player::ChangeTeam( int iTeamNum )
{
	BaseClass::ChangeTeam( iTeamNum );
	if ( g_pGameRules->IsMultiplayer() == false )
	{
		SetName( MAKE_STRING( "player" ) );
		return;
	}

	// Change our model at this point
	if ( iTeamNum == TEAM_BLUE )
	{
		SetName( MAKE_STRING( "blue" ) );
	}
	else if ( iTeamNum == TEAM_RED )
	{
		SetName( MAKE_STRING( "red" ) );
	}

	SetPlayerModel();

	ClearScriptedInteractions();
	ParseScriptedInteractions();
}


void CPortal_Player::ApplyPortalTeleportation( const CPortal_Base2D *pEnteredPortal, CMoveData *pMove )
{
#if PLAYERPORTALDEBUGSPEW == 1
	Warning( "SERVER CPortal_Player::ApplyPortalTeleportation( %f %i )\n", gpGlobals->curtime, m_pCurrentCommand->command_number );
#endif

	//catalog the pending transform
	{
		RecentPortalTransform_t temp;
		temp.command_number = GetCurrentUserCommand()->command_number;
		temp.Portal = pEnteredPortal;
		temp.matTransform = pEnteredPortal->m_matrixThisToLinked.As3x4();

		m_PendingPortalTransforms.AddToTail( temp );

		//prune the pending transforms so it doesn't get ridiculously huge if the client stops responding while in an infinite fall or something
		while( m_PendingPortalTransforms.Count() > 64 )
		{
			m_PendingPortalTransforms.Remove( 0 );
		}
	}

	CBaseEntity *pHeldEntity = GetPlayerHeldEntity( this );
	if ( pHeldEntity && !IsUsingVMGrab() )
	{
		ToggleHeldObjectOnOppositeSideOfPortal();
		SetHeldObjectPortal( IsHeldObjectOnOppositeSideOfPortal() ? pEnteredPortal->m_hLinkedPortal.Get() : NULL );
	}

	//transform m_PlayerAnimState yaws
	m_PlayerAnimState->TransformYAWs( pEnteredPortal->m_matrixThisToLinked.As3x4() );

	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *pViewModel = GetViewModel( i );
		if ( !pViewModel )
			continue;

		pViewModel->m_vecLastFacing = pEnteredPortal->m_matrixThisToLinked.ApplyRotation( pViewModel->m_vecLastFacing );
	}

	//physics transform
	{
		SetVCollisionState( pMove->GetAbsOrigin(), pMove->m_vecVelocity, IsDucked() ? VPHYS_CROUCH : VPHYS_WALK );
	}

	//transform local velocity
	{
		//Vector vTransformedLocalVelocity;
		//VectorRotate( GetAbsVelocity(), pEnteredPortal->m_matrixThisToLinked.As3x4(), vTransformedLocalVelocity );
		//SetAbsVelocity( vTransformedLocalVelocity );
		SetAbsVelocity( pMove->m_vecVelocity );
	}

	//transform base velocity
	{
		Vector vTransformedBaseVelocity;
		VectorRotate( GetBaseVelocity(), pEnteredPortal->m_matrixThisToLinked.As3x4(), vTransformedBaseVelocity );
		SetBaseVelocity( vTransformedBaseVelocity );
	}

	//transform previous position
	{
		UTIL_Portal_PointTransform( pEnteredPortal->m_matrixThisToLinked, m_vPrevPosition, m_vPrevPosition );
	}

	CollisionRulesChanged();


	// straighten out velocity if going nearly straight up/down out of a floor/ceiling portal
	{
		const CPortal_Base2D *pOtherPortal = pEnteredPortal->m_hLinkedPortal.Get();
		if( sv_player_funnel_into_portals.GetBool() && pOtherPortal )
		{
			// Make sure this portal is nearly facing straight up/down
			const Vector vNormal = pOtherPortal->m_PortalSimulator.GetInternalData().Placement.vForward;
			if( (1.f - fabs(vNormal.z)) < 0.001f )
			{
				const Vector vUp(0.f,0.f,1.f);
				const Vector vVel = pMove->m_vecVelocity;
				const float flVelDotUp = DotProduct( vVel.Normalized(), vUp );
				// We're going mostly straight up/down
				if( fabs( flVelDotUp ) > sv_player_funnel_gimme_dot.GetFloat() )
				{
					// Make us go exactly sraight up/down
					pMove->m_vecVelocity = ( DotProduct(vUp, vVel) * vUp );
				}
			}
		}
	}

	PostTeleportationCameraFixup( pEnteredPortal );

	// Use a slightly expanded box to search for stick surfaces as the player leaves the portal.
	m_flUsePostTeleportationBoxTime = sv_post_teleportation_box_time.GetFloat();

	m_nPortalsEnteredInAirFlags |= pEnteredPortal->m_nPortalColor;

	int nAllPortals = ( PORTAL_COLOR_FLAG_BLUE | PORTAL_COLOR_FLAG_PURPLE | PORTAL_COLOR_FLAG_ORANGE | PORTAL_COLOR_FLAG_RED );
	if ( ( m_nPortalsEnteredInAirFlags & nAllPortals ) == nAllPortals )
	{
		UTIL_RecordAchievementEvent( "ACH.FOUR_PORTALS", this );
	}

#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	g_PortalGameStats.Event_PortalTeleport( this );
#endif

	// Increment portal uses in MP stats
	if( GetPortalMPStats() )
	{
		GetPortalMPStats()->IncrementPlayerPortalsTraveled( this );
	}

	const CUtlVector<ITriggerCatapultAutoList*>& catapults = CTriggerCatapult::AutoList();
	for( int i = 0; i < catapults.Count(); ++i )
	{
		assert_cast<CTriggerCatapult*>( catapults[i] )->EndTouch( this );
	}
}

CON_COMMAND( displayportalplayerstats, "Displays current level stats for portals placed, steps taken, and seconds taken." )
{
	CPortal_Player *pPlayer = (CPortal_Player *)UTIL_GetCommandClient();
	if( pPlayer == NULL )
		pPlayer = GetPortalPlayer( 1 ); //last ditch effort

	if( pPlayer )
	{
		int iMinutes = static_cast<int>( pPlayer->NumSecondsTaken() / 60.0f );
		int iSeconds = static_cast<int>( pPlayer->NumSecondsTaken() ) % 60;

		CFmtStr msg;
		NDebugOverlay::ScreenText( 0.5f, 0.5f, msg.sprintf( "Portals Placed: %d\nSteps Taken: %d\nTime: %d:%d", pPlayer->NumPortalsPlaced(), pPlayer->NumStepsTaken(), iMinutes, iSeconds ), 255, 255, 255, 150, 5.0f );
	}
}

CON_COMMAND( startneurotoxins, "Starts the nerve gas timer." )
{
	CPortal_Player *pPlayer = (CPortal_Player *)UTIL_GetCommandClient();
	if( pPlayer == NULL )
		pPlayer = GetPortalPlayer( 1 ); //last ditch effort

	float fCoundownTime = 180.0f;

	if ( args.ArgC() > 1 )
		fCoundownTime = atof( args[ 1 ] );

	if( pPlayer )
		pPlayer->SetNeuroToxinDamageTime( fCoundownTime );
}

void CPortal_Player::InitialSpawn( void )
{
	BaseClass::InitialSpawn();

#if USE_SLOWTIME
	// Reset our slow timers
	m_PortalLocal.m_bSlowingTime = false;
	m_PortalLocal.m_flSlowTimeRemaining = m_PortalLocal.m_flSlowTimeMaximum = slowtime_max.GetFloat();
#endif // USE_SLOWTIME

	m_bReadyForDLCItemUpdates = false;

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	UpdateInventory( true );
#endif
}

extern ConVar *sv_cheats;
void CC_give_me_a_point( void )
{
	//static ConVarRef sv_cheatsref( "sv_cheats" );
	if( sv_cheats->GetBool() )
	{
		UTIL_GetCommandClient()->IncrementFragCount( 1 );
	}
}

static ConCommand give_a_point( "give_me_a_point", CC_give_me_a_point, "Give yourself a point", 0 );

#if USE_SLOWTIME

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CLogicPlayerSlowTime : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicPlayerSlowTime, CLogicalEntity );
	DECLARE_DATADESC();

private:

	void InputStartSlowTime( inputdata_t &data )
	{
		CPortal_Player *pPlayer = (CPortal_Player *) UTIL_GetLocalPlayer();
		if ( pPlayer )
		{
			pPlayer->StartSlowingTime( data.value.Float() );
		}
	}

	void InputStopSlowTime( inputdata_t &data )
	{
		CPortal_Player *pPlayer = (CPortal_Player *) UTIL_GetLocalPlayer();
		if ( pPlayer )
		{
			pPlayer->StopSlowingTime();
		}
	}
};

BEGIN_DATADESC( CLogicPlayerSlowTime )
	DEFINE_INPUTFUNC( FIELD_FLOAT, "StartSlowingTime", InputStartSlowTime ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopSlowingTime", InputStopSlowTime ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( logic_player_slowtime, CLogicPlayerSlowTime );

#endif // USE_SLOWTIME

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CLogicPlayerViewFinder : public CPointEntity
{
public:
	DECLARE_CLASS( CLogicPlayerViewFinder, CPointEntity );
	DECLARE_DATADESC();

private:

	void InputShowViewFinder( inputdata_t &data )
	{
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CPortal_Player *pPlayer = (CPortal_Player *) UTIL_PlayerByIndex( i );
			if ( pPlayer )
			{
				pPlayer->ShowViewFinder();
			}
		}
	}

	void InputHideViewFinder( inputdata_t &data )
	{
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CPortal_Player *pPlayer = (CPortal_Player *) UTIL_PlayerByIndex( i );
			if ( pPlayer )
			{
				pPlayer->HideViewFinder();
			}
		}
	}
};

BEGIN_DATADESC( CLogicPlayerViewFinder )
DEFINE_INPUTFUNC( FIELD_VOID, "ShowViewFinder", InputShowViewFinder ),
DEFINE_INPUTFUNC( FIELD_VOID, "HideViewFinder", InputHideViewFinder ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_player_viewfinder, CLogicPlayerViewFinder );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortal_Player::ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet )
{
	BaseClass::ModifyOrAppendCriteria( criteriaSet );

	// Determine if we're in the air
	criteriaSet.AppendCriteria( "in_air", ( GetGroundEntity() == NULL || m_PortalLocal.m_hTractorBeam.Get() ) ? 1 : 0 );

	// Determine if we're standing on something special
	CBaseEntity *pGroundEnt = GetGroundEntity();
	if ( pGroundEnt != NULL )
	{
		criteriaSet.AppendCriteria( "ground_entity", pGroundEnt->GetClassname() );
	}

	// Looking at various things
	trace_t tr;
	Vector vecOurForward;
	EyeVectors( &vecOurForward, NULL, NULL );
	UTIL_TraceLine( EyePosition(), EyePosition() + ( vecOurForward * MAX_TRACE_LENGTH ), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	if ( tr.DidHitNonWorldEntity() )
	{
		criteriaSet.AppendCriteria( "look_entity", tr.m_pEnt->GetClassname() );
	}

	// We need to find the other player
	CPortal_Player *pPartner = ToPortalPlayer( UTIL_OtherPlayer( this ) );

	// Any criteria after this point require a partner!
	if ( pPartner != NULL )
	{
		Vector vecPartnerForward;
		pPartner->GetVectors( &vecPartnerForward, NULL, NULL );
		vecOurForward.z = 0.0f;
		vecPartnerForward.z = 0.0f;

		float flDot = DotProduct( vecOurForward, vecPartnerForward );

		bool bFacingPartner = ( flDot < -DOT_20DEGREE );
		criteriaSet.AppendCriteria( "facing_partner", ( bFacingPartner ) ? 1 : 0 );

		float flDistToPartner = ( GetAbsOrigin() - pPartner->GetAbsOrigin() ).Length();
		criteriaSet.AppendCriteria( "dist_to_partner", flDistToPartner );
	}
	else
	{
		criteriaSet.AppendCriteria( "dist_to_partner", 0.0f );
	}

	criteriaSet.AppendCriteria( "rps_outcome", PortalMPGameRules() ? PortalMPGameRules()->GetRPSOutcome() : 0 );

	if ( m_szTauntForce[ 0 ] == '\0' )
	{
		criteriaSet.AppendCriteria( "force_taunt", "empty" );
	}
	else
	{
		criteriaSet.AppendCriteria( "force_taunt", m_szTauntForce.Get() );
	}

	criteriaSet.AppendCriteria( "taunt_partner", ( m_nTeamTauntState == TEAM_TAUNT_SUCCESS ) ? 1 : 0 );

	criteriaSet.AppendCriteria( "no_portalgun", ( !Weapon_OwnsThisType( "weapon_portalgun" ) ) ? 1 : 0 );

	// Check for if the player is ballbot or eggbot
	if ( g_pGameRules->IsMultiplayer() )
	{
		switch ( GetTeamNumber() )
		{
		case TEAM_RED:
			criteriaSet.AppendCriteria( "is_eggbot", 1 );
			break;
		case TEAM_BLUE:
			criteriaSet.AppendCriteria( "is_ballbot", 1 );
			break;
		}
	}
}

ConVar portal_use_player_avoidance( "portal_use_player_avoidance", "0", FCVAR_REPLICATED );

//-----------------------------------------------------------------------------
// Purpose: Don't collide with other players, we'll just push away from them
//-----------------------------------------------------------------------------
bool CPortal_Player::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	// Don't hit other players
	if ( portal_use_player_avoidance.GetBool() && ( ( collisionGroup == COLLISION_GROUP_PLAYER || collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT ) ) )
		return false;

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

//-----------------------------------------------------------------------------
// Purpose: Change a player from blue to orange and vice versa
//-----------------------------------------------------------------------------
void PlayerSwitchTeams( void )
{
	// Find all players and swap them around
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CPortal_Player *pPlayer = static_cast<CPortal_Player *>(UTIL_PlayerByIndex( i ));
		if ( pPlayer )
		{
			if ( pPlayer->GetTeamNumber() == TEAM_RED )
			{
				pPlayer->ChangeTeam( TEAM_BLUE );
			}
			else
			{
				pPlayer->ChangeTeam( TEAM_RED );
			}
		}
	}
}

// REMOVED FOR PORTAL2: ConCommand switch_teams( "switch_teams", PlayerSwitchTeams, "Change a player from blue to orange and vice versa.", FCVAR_NONE );


//////////////////////////////////////////////////////////////////////////
// PAINT SECTION
//////////////////////////////////////////////////////////////////////////

// This is a glorious hack to find free space when you've crouched into some solid space
// Our crouching collisions do not work correctly for some reason and this is easier
// than fixing the problem :(
// Note: This is a nasty copy/paste job from player.cpp, which replaces VEC_DUCK_HULL_MIN/MAX
//		 with the player's local hulls.
CEG_NOINLINE void FixPlayerCrouchStuck( CPortal_Player *pPlayer )
{
	trace_t trace;

	const Vector& duckHullMin = pPlayer->GetDuckHullMins();
	const Vector& duckHullMax = pPlayer->GetDuckHullMaxs();

	// Move up as many as 18 pixels if the player is stuck.
	Vector org = pPlayer->GetAbsOrigin();;
	for( int i = 0; i < 18; i++ )
	{
		UTIL_TraceHull( pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin(),
			duckHullMin, duckHullMax, MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
		if ( trace.startsolid )
		{
			Vector origin = pPlayer->GetAbsOrigin();
			origin.z += 1.0f;
			pPlayer->SetLocalOrigin( origin );
		}
		else
			return;
	}

	pPlayer->SetAbsOrigin( org );

	for( int i = 0; i < 18; i++ )
	{
		UTIL_TraceHull( pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin(),
			duckHullMin, duckHullMax, MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
		if ( trace.startsolid )
		{
			Vector origin = pPlayer->GetAbsOrigin();
			origin.z -= 1.0f;
			pPlayer->SetLocalOrigin( origin );
		}
		else
			return;
	}
}

CEG_PROTECT_FUNCTION( FixPlayerCrouchStuck );

int CPortal_Player::Restore( IRestore &restore )
{
	int status = 0;
	if( BaseClass::Restore( restore ) )
	{
		status = 1;

		if( GetFlags() & FL_DUCKING )
		{
			// Use the crouch HACK
			FixPlayerCrouchStuck( this );
			UTIL_SetSize( this, GetDuckHullMins(), GetDuckHullMaxs() );
			m_Local.m_bDucked = true;
		}
		else
		{
			m_Local.m_bDucked = false;
			UTIL_SetSize( this, GetStandHullMins(), GetStandHullMaxs() );
		}
	}

	return status;
}

void CPortal_Player::GivePortalPlayerItems( void )
{
	bool bSpawnWithPaintGun = false;
	bool bSpawnWithPortalGun = false;

	bool bMultiplayer = g_pGameRules->IsMultiplayer();

	bool bHadPaintGunOnDeath = m_PlayerGunTypeWhenDead == PLAYER_PAINT_GUN;
	bool bHadPortalGunOnDeath = m_PlayerGunTypeWhenDead == PLAYER_PORTAL_GUN;

	bool bIs2GunsMap = ( V_stristr( gpGlobals->mapname.ToCStr(), "2guns" ) != NULL ) || ( GlobalEntity_GetState( "paintgun_map" ) == GLOBAL_ON );

	//If this map has 2 guns in it
	if( bMultiplayer && bIs2GunsMap )
	{
		//If the player should spawn with a paint gun in this map
		if( bHadPaintGunOnDeath || //Spawn with paintgun in multiplayer if the player had paintgun on death
			( !m_bSpawnFromDeath && m_PlayerGunType == PLAYER_PAINT_GUN ) || //Spawn with paintgun if player is not spawning from death and had paintgun
			( m_PlayerGunType == PLAYER_NO_GUN && GetTeamNumber() != g_iPortalGunPlayerTeam ) ) //Red player gets paintgun if no gun is assigned
		{
			bSpawnWithPaintGun = true;
		}
		//Else If the player should spawn with a portal gun in this map
		else if( bHadPortalGunOnDeath || //Spawn with portalgun in multiplayer if the player had portalgun on death
			( !m_bSpawnFromDeath && m_PlayerGunType == PLAYER_PORTAL_GUN ) || //Spawn with portalgun if player is not spawning from death and had portalgun
			( m_PlayerGunType == PLAYER_NO_GUN && GetTeamNumber() == g_iPortalGunPlayerTeam ) ) //Blue player gets portalgun
		{
			bSpawnWithPortalGun = true;
		}
	}
	else
	{
		GiveDefaultItems();
	}

	//Check for the can carry both guns cheat
	if( sv_can_carry_both_guns )
	{
		bSpawnWithPaintGun = true;
		bSpawnWithPortalGun = true;
	}

	if( bSpawnWithPortalGun )
	{
		bool bSwitchToPortalGun = !bIs2GunsMap;

		GivePlayerPortalGun( true, bSwitchToPortalGun );
	}

	if( bSpawnWithPaintGun )
	{
		GivePlayerPaintGun( false, false );
	}

	m_bSpawnFromDeath = false;
	m_PlayerGunTypeWhenDead = PLAYER_NO_GUN;
}


void CPortal_Player::GivePlayerPaintGun( bool bActivatePaintPowers, bool bSwitchTo )
{
	CBaseCombatWeapon *pWeapon = Weapon_OwnsThisType( "weapon_paintgun", 0 );
	CWeaponPaintGun *pPaintGun = NULL;

	//If the player already has a paint gun
	if( pWeapon )
	{
		pPaintGun = static_cast<CWeaponPaintGun*>( pWeapon );
	}
	else //Give the player a paint gun
	{
		pPaintGun = (CWeaponPaintGun*)CreateEntityByName( "weapon_paintgun" );
		if ( pPaintGun )
		{
			DispatchSpawn( pPaintGun );

			if ( !pPaintGun->IsMarkedForDeletion() )
			{
				Weapon_Equip( pPaintGun );
			}
		}
	}

	//Activate all the paint powers if needed
	if( pPaintGun && bActivatePaintPowers )
	{
		pPaintGun->ActivatePaint(BOUNCE_POWER);
		pPaintGun->ActivatePaint(SPEED_POWER);
		//pPaintGun->ActivatePaint(REFLECT_POWER);
		pPaintGun->ActivatePaint(PORTAL_POWER);
		PaintPowerPickup( BOUNCE_POWER, this );
		PaintPowerPickup( SPEED_POWER, this );
		//PaintPowerPickup( REFLECT_POWER, this );
		PaintPowerPickup( PORTAL_POWER, this );
	}

	//Switch to the paint gun
	if( pPaintGun && bSwitchTo )
	{
		Weapon_Switch( pPaintGun );
	}
}


void CPortal_Player::GivePlayerPortalGun( bool bUpgraded, bool bSwitchTo )
{
	CBaseCombatWeapon *pWeapon = Weapon_OwnsThisType( "weapon_portalgun", 0 );
	CWeaponPortalgun *pPortalGun = NULL;

	//If the player already has a portal gun
	if( pWeapon )
	{
		pPortalGun = static_cast<CWeaponPortalgun*>( pWeapon );
	}
	else //Give the player a portal gun
	{
		pPortalGun = (CWeaponPortalgun*)CreateEntityByName( "weapon_portalgun" );
		if ( pPortalGun )
		{
			pPortalGun->SetLocalOrigin( GetLocalOrigin() );
			pPortalGun->AddSpawnFlags( SF_NORESPAWN );
			pPortalGun->SetSubType( 0 );

			DispatchSpawn( pPortalGun );

			if ( !pPortalGun->IsMarkedForDeletion() )
			{
				pPortalGun->SetCanFirePortal1();

				//Upgrade the portal gun if needed
				if( bUpgraded )
				{
					pPortalGun->SetCanFirePortal2();
				}

				//pPortalGun->Touch( this );
				Weapon_Equip( pPortalGun );
			}
		}
	}

	//Switch to the portal gun
	if( pPortalGun && bSwitchTo )
	{
		Weapon_Switch( pPortalGun );
	}
}

void CPortal_Player::RemovePlayerWearable( const char *pItemName )
{
	CBaseCombatWeapon *pWeapon = Weapon_OwnsThisType( pItemName, 0 );
	if ( pWeapon )
	{
		if ( Weapon_Detach(pWeapon) )
		{
			UTIL_Remove( pWeapon );
		}
	}
}

void CPortal_Player::GivePlayerWearable( const char *pItemName )
{
	CBaseCombatWeapon *pWeapon = Weapon_OwnsThisType( pItemName, 0 );

	if ( !pWeapon )
	{
		pWeapon = (CBaseCombatWeapon *)CreateEntityByName( pItemName );

		if ( pWeapon )
		{
			pWeapon->SetLocalOrigin( GetLocalOrigin() );
			pWeapon->AddSpawnFlags( SF_NORESPAWN );
			pWeapon->SetSubType( 0 );

			DispatchSpawn( pWeapon );

			if ( !pWeapon->IsMarkedForDeletion() )
			{
				Weapon_Equip( pWeapon );
			}
		}
	}

	if ( pWeapon )
	{
		// DON'T SWITCH TO THIS! We still draw it even when it's not the active weapon
		//Weapon_Switch( pWeapon );
		pWeapon->SetWeaponVisible( true );
	}
}

void CPortal_Player::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	if( pWeapon && FClassnameIs( pWeapon, "weapon_paintgun" ) )
	{
		m_PlayerGunType = PLAYER_PAINT_GUN;
	}
	else if( pWeapon && FClassnameIs( pWeapon, "weapon_portalgun" ) )
	{
		m_PlayerGunType = PLAYER_PORTAL_GUN;
	}

	//If the player is equipping the paint gun
	//CWeaponPaintGun *pPaintGun = dynamic_cast<CWeaponPaintGun*>( pWeapon );
	//if( pPaintGun )
	//{
	//	IGameEvent *event = gameeventmanager->CreateEvent( "equipped_paintgun" );
	//	if ( event )
	//	{
	//		event->SetInt("userid", GetUserID() );

	//		gameeventmanager->FireEvent( event );
	//	}

	//	m_PlayerGunType = PLAYER_PAINT_GUN;
	//}

	//// set portals owner here because picking up non trigger weapon doesn't call BumpWeapon
	//CWeaponPortalgun *pPortalGun = dynamic_cast< CWeaponPortalgun* >( pWeapon );

	////store old linkageID
	//unsigned int linkageID = 0;
	//if ( pPortalGun )
	//{
	//	linkageID = pPortalGun->m_iPortalLinkageGroupID;

	//	m_PlayerGunType = PLAYER_PORTAL_GUN;
	//}

	BaseClass::Weapon_Equip( pWeapon );

	//if ( pPortalGun )
	//{
	//	//reset the linkage ID because Weapon_Equip calls Deploy which changes the id
	//	pPortalGun->m_iPortalLinkageGroupID = linkageID;

	//	//Set the existing portals to have been fired by the new player holding the portal gun
	//	CProp_Portal *pPortal1 = CProp_Portal::FindPortal( linkageID, false );
	//	if( pPortal1 )
	//	{
	//		pPortal1->SetFiredByPlayer( this );
	//	}

	//	CProp_Portal *pPortal2 = CProp_Portal::FindPortal( linkageID, true );
	//	if( pPortal2 )
	//	{
	//		pPortal2->SetFiredByPlayer( this );
	//	}
	//}
}


void CPortal_Player::SetWantsToSwapGuns( bool bWantsToSwap )
{
	m_bWantsToSwapGuns = bWantsToSwap;
	m_bSendSwapProximityFailEvent = true;
}


void CPortal_Player::SwapThink()
{
	bool bIsMultiplayer = gpGlobals->maxClients > 1;
	//Check if this player wants to swap guns
	if( m_afButtonPressed & IN_ALT1 )
	{
		CBaseCombatWeapon *pPaintGun = Weapon_OwnsThisType( "weapon_paintgun" );
		CBaseCombatWeapon *pPortalGun = Weapon_OwnsThisType( "weapon_portalgun" );
		bool bHasBothGuns = !!pPaintGun && !!pPortalGun;
		if( ( !bIsMultiplayer && bHasBothGuns ) || sv_can_carry_both_guns )
		{
			//engine->ClientCommand( edict(), "lastinv" );

			if ( pPaintGun == GetActiveWeapon() )
			{
				Weapon_Switch( pPortalGun );
			}
			else
			{
				Weapon_Switch( pPaintGun );
			}
		}
		else
		{
			if( bIsMultiplayer && sv_can_swap_guns_anytime )
			{
				IGameEvent *event = gameeventmanager->CreateEvent( "wants_to_swap_guns" );
				if ( event )
				{
					event->SetInt( "userid", GetUserID() );

					gameeventmanager->FireEvent( event );
				}

				SetWantsToSwapGuns( true );
			}
		}
	}
	else if( m_afButtonReleased & IN_ALT1 && bIsMultiplayer && !sv_can_carry_both_guns && sv_can_swap_guns_anytime )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "doesnt_want_to_swap_guns" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );

			gameeventmanager->FireEvent( event );
		}

		SetWantsToSwapGuns( false );
	}

	bool bSwap = false;
	if( WantsToSwapGuns() && sv_can_swap_guns )
	{
		CPortal_Player *pOtherPlayer = ToPortalPlayer( UTIL_OtherConnectedPlayer( this ) );
		if( pOtherPlayer && pOtherPlayer->WantsToSwapGuns() )
		{
			if( sv_can_swap_guns_anytime )
			{
				//Check if the players are close enough to swap
				bSwap = CheckSwapProximity( this, pOtherPlayer );
			}
			else
			{
				bSwap = true;
			}

			if( bSwap )
			{
				//Reset the wants to swap flag for both players
				SetWantsToSwapGuns( false );
				pOtherPlayer->SetWantsToSwapGuns( false );

				SwapPaintAndPortalGuns( this, pOtherPlayer );

				//Send the event that the players swapped guns
				IGameEvent *event = gameeventmanager->CreateEvent( "swapped_guns" );
				if ( event )
				{
					gameeventmanager->FireEvent( event );
				}
			}
			else
			{
				//Check if the proximity check failed event should be send
				//We only want to send this event the first time the proximity check fails
				if( m_bSendSwapProximityFailEvent )
				{
					m_bSendSwapProximityFailEvent = false;

					IGameEvent *event = gameeventmanager->CreateEvent( "swap_guns_proximity_fail" );
					if( event )
					{
						gameeventmanager->FireEvent( event );
					}
				}
			}
		} //If other player wants to swap
	} //If this player wants to swap


}


Vector CPortal_Player::BodyTarget( const Vector &posSrc, bool bNoisy )
{
	if (bNoisy)
	{
		return WorldSpaceCenter() /*+ (GetViewOffset() * random->RandomFloat( 0.7, 1.0 ))*/;
	}
	else
	{
		return WorldSpaceCenter();
	}
}

void CPortal_Player::SetFogController( CFogController *pFogController )
{
	BaseClass::SetFogController( pFogController );

	// In portal multiplayer we need to for the master to be whatever the player was last set to
	// so when they respawn they still obey the master
	if ( GameRules() && GameRules()->IsMultiplayer() )
	{
		FogSystem()->SetMasterController( pFogController );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CPortal_Player::PlayGesture( const char *pGestureName )
{
	Activity nActivity = (Activity)LookupActivity( pGestureName );
	if ( nActivity != ACT_INVALID )
	{
		DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE, nActivity );
		return true;
	}

	int nSequence = LookupSequence( pGestureName );
	if ( nSequence != -1 )
	{
		DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE_SEQUENCE, nSequence );
		return true;
	}

	return false;
}

void CPortal_Player::InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity )
{
	// Cleanup any old vphysics stuff.
	VPhysicsDestroyObject();

	// in turbo physics players dont have a physics shadow
	if ( sv_turbophysics.GetBool() )
		return;

	CPhysCollide *pModel = PhysCreateBbox( GetStandHullMins(), GetStandHullMaxs() );
	CPhysCollide *pCrouchModel = PhysCreateBbox( GetDuckHullMins(), GetDuckHullMaxs() );

	SetupVPhysicsShadow( vecAbsOrigin, vecAbsVelocity, pModel, "player_stand", pCrouchModel, "player_crouch" );
}


void CPortal_Player::OnPlayerLanded()
{
	// make sure the player don't land on the floor at the bottom of the goo
	if ( GetWaterLevel() == WL_NotInWater )
	{
		m_bWasDroppedByOtherPlayerWhileTaunting = false;
	}
}

void CPortal_Player::NetworkPortalTeleportation( CBaseEntity *pOther, CPortal_Base2D *pPortal, float fTime, bool bForcedDuck )
{
	CEntityPortalledNetworkMessage &writeTo = m_EntityPortalledNetworkMessages[m_iEntityPortalledNetworkMessageCount % MAX_ENTITY_PORTALLED_NETWORK_MESSAGES];

	writeTo.m_hEntity = pOther;
	writeTo.m_hPortal = pPortal;
	writeTo.m_fTime = fTime;
	writeTo.m_bForcedDuck = bForcedDuck;
	writeTo.m_iMessageCount = m_iEntityPortalledNetworkMessageCount;
	++m_iEntityPortalledNetworkMessageCount;

	//NetworkProp()->NetworkStateChanged( offsetof( CPortal_Player, m_EntityPortalledNetworkMessages ) );
	NetworkProp()->NetworkStateChanged();
}


void cc_can_carry_both_guns( const CCommand &args )
{
	//sv_can_carry_both_guns.SetValue( args[1] );

	//if ( sv_can_carry_both_guns.GetBool() )
	{
		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CPortal_Player *pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer )
			{
				pPlayer->GivePlayerPaintGun( true, false );
				pPlayer->GivePlayerPortalGun( true, true );
			}
		}
	}
}
// FIXME: Bring this back for DLC2
//ConCommand can_carry_both_guns( "can_carry_both_guns", cc_can_carry_both_guns );

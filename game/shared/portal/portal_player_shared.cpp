//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include <functional>
#include "portal_player_shared.h"
//#include "portal_playeranimstate.h"
#include "debugoverlay_shared.h"
#include "mesh.h"
#include "in_buttons.h"
#include "portal_base2d_shared.h"
#include "movevars_shared.h"
#include "util_shared.h"
#include "portal_util_shared.h"
#include "collisionutils.h"
#include "portal_mp_gamerules.h"

#ifdef CLIENT_DLL

#include "c_portal_player.h"
#include "prediction.h"
#define CRecipientFilter C_RecipientFilter

#include "iclientvehicle.h"
#include "c_basedoor.h"
#include "c_world.h"
#include "view.h"
#include "iviewrender.h"
#include "ivieweffects.h"

#define CRecipientFilter C_RecipientFilter

extern IViewRender *view;
extern IViewEffects *GetViewEffects();

#include "cdll_util.h"
#include "c_portal_base2d.h"
#include "c_weapon_paintgun.h"
#include "c_trigger_catapult.h"
#include "c_trigger_tractorbeam.h"
#include "c_basetempentity.h"
#include "igameevents.h"
#include "cam_thirdperson.h"
#define CTriggerCatapult C_TriggerCatapult

#else

#include "portal_player.h"
#include "ai_basenpc.h"
#include "portal_gamestats.h"
#include "util.h"
#include "eventqueue.h"
#include "physics_bone_follower.h"
#include "prop_testchamber_door.h"
#include "iservervehicle.h"
#include "trains.h"
#include "world.h"
#include "doors.h"
#include "ai_basenpc.h"
#include "env_zoom.h"
#include "ammodef.h"
#include "npc_security_camera.h"

extern int TrainSpeed(int iSpeed, int iMax);

#include "portal_base2d.h"
#include "paint_database.h"

#include "basetempentity.h"
#include "paint_swap_guns.h"
#include "weapon_paintgun.h"
#include "weapon_portalgun.h"
#include "trigger_catapult.h"
#include "trigger_tractorbeam.h"
#include "physicsshadowclone.h"

#include "explode.h"
#include "props.h"

#endif

#include "vphysics/player_controller.h"
#include "in_buttons.h"
#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "collisionutils.h"
#include "portal2/portal_grabcontroller_shared.h"
#include "portal2/player_pickup.h"
#include "weapon_paintgun_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define TLK_PLAYER_PICKED_UP_ITEM "TLK_PLAYER_PICKED_UP_ITEM"

const char *g_pszChellConcepts[] =
{
	"CONCEPT_CHELL_IDLE",
	"CONCEPT_CHELL_DEAD",
};

extern ConVar sv_footsteps;
extern ConVar sv_debug_player_use;

char const* const JUMP_HELPER_CONTEXT = "Jump Helper Powers";
char const* const AIR_LOOK_AHEAD_CONTEXT = "Air Look Ahead Powers";
char const* const STICK_PREDICTION_CONTEXT = "Stick Prediction Powers";
char const* const PAINT_SCREEN_EFFECT = "boomer_vomit_screeneffect";//"paint_screen_effect";
char const* const PAINT_DRIP_EFFECT = "boomer_vomit_survivor";//"paint_drip_effect";

const float BOUNCE_PAINT_INPUT_DAMPING = 0.1f;
const float BOUNCE_PAINT_INPUT_DAMP_TIME = 2.0;
const float MIN_SPEED_PAINT_AIR_INPUT_START_DAMPING = 0.05f;
const float SPEED_PAINT_AIR_INPUT_DAMP_TIME = 5.0f;
const float RELATIVE_ERROR = 1.0e-6;
const float DEAD_INPUT_COS = 0.70710678118654752440084436210485f;
const Vector ABS_UP = Vector(0.f,0.f,1.f);


// The depth of the OBB on the portal surface within which we reject contacts
// HACK: Half width of player box + 1 for now. We'll have to do a projection of
//		 the collision shape onto the portal normal when we have oriented bounding data
const float PORTAL_PLANE_IGNORE_EPSILON = 17.0f;

extern ConVar sv_speed_normal;
extern ConVar sv_speed_paint_max;
extern ConVar portal_tauntcam_dist;
ConVar portal_deathcam_dist( "portal_deathcam_dist", "128", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );

ConVar sv_portal_coop_ping_hud_indicitator_duration( "sv_portal_coop_ping_hud_indicitator_duration", "5", FCVAR_REPLICATED );


char const* g_pszPredictedPowerIgnoreFilter[] =
{
	"prop_weighted_cube"
};

const char *GetEggBotModel( bool bLowRes )
{
	if ( bLowRes) 
		return "models/player/eggbot/eggbot_opt.mdl";

	return "models/player/eggbot/eggbot.mdl";
}

const char *GetBallBotModel( bool bLowRes )
{
	if ( bLowRes) 
		return "models/player/ballbot/ballbot_opt.mdl";

	return "models/player/ballbot/ballbot.mdl";
}

const char *g_pszPlayerModel = "models/player/chell/player.mdl";

int PAINTED_SURFACE_PROPERTY_INDEX = -1;

ConVar sv_enableholdrotation( "sv_enableholdrotation", "0", FCVAR_REPLICATED, "When enabled, hold attack2 to rotate held objects" );
ConVar sv_player_use_cone_size( "sv_player_use_cone_size", "0.6", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar sv_use_trace_duration( "sv_use_trace_duration", "0.5", FCVAR_REPLICATED | FCVAR_CHEAT );

//bounce convars
ConVar sv_bounce_paint_forward_velocity_bonus("sv_bounce_paint_forward_velocity_bonus", "0.375f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "What percentage of forward velocity to add onto a ground bounce");
ConVar bounce_paint_wall_jump_upward_speed("bounce_paint_wall_jump_upward_speed", "275", FCVAR_REPLICATED | FCVAR_CHEAT, "The upward velocity added when bouncing off a wall");
ConVar bounce_paint_min_speed("bounce_paint_min_speed", "500.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "For tweaking how high bounce paint launches the player.");
ConVar sv_wall_bounce_trade("sv_wall_bounce_trade", "0.73", FCVAR_REPLICATED | FCVAR_CHEAT, "How much outward velocity is traded for upward velocity on wall bounces");
ConVar jump_helper_enabled("jump_helper_enabled", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Use long jump helper code?");
ConVar jump_helper_look_ahead_time("jump_helper_look_ahead_time", "0.2f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Look ahead time for long jump helper. This assumes constant velocity and samples ahead in discrete chunks of time. Use paint_power_look_ahead_sample_density to adjust how many samples are taken over a given distance.");
ConVar paint_power_look_ahead_sample_density("paint_power_look_ahead_sample_density", "0.07f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Number of samples per game unit along the extent of look ahead vector.");
ConVar jump_helper_late_jump_max_time("jump_helper_late_jump_max_time", "0.2f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "How late a player can try to jump and still get a super jump she's no longer touching.");
ConVar jump_helper_debug_enabled("jump_helper_debug_enabled", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Toggle debug draw and and messages for jump helper.");
ConVar bounce_reflect_restitution("bounce_reflect_restitution", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "The elasticity of a collision with a bounce paint surface. Values should be in the range [0, 1].");
ConVar trampoline_bounce_off_walls_while_on_ground("trampoline_bounce_off_walls_while_on_ground", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Trampoline bounce activates a wall jump if the player walks into a wall.");
ConVar jump_button_can_activate_trampoline_bounce("jump_button_can_activate_trampoline_bounce", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "The space bar can activate the initial bounce in trampoline bounce mode.");
ConVar bounce_reflect_wall_jumps_enabled("bounce_reflect_wall_jumps_enabled", "1", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Enable/Disable wall jumps for reflection bounce.");
ConVar bounce_reflect_wall_jump_min_up_speed("bounce_reflect_wall_jump_min_up_speed", "10.0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "The minimum upward speed the player will jump off the wall with when reflection bounce wall jumps are enabled.");
ConVar trampoline_bounce_min_impact_speed("trampoline_bounce_min_impact_speed", "4.0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "The minimum speed into the surface to activate a trampoline bounce.");
ConVar bounce_ledge_fall_height_boost("bounce_ledge_fall_height_boost", "45", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Toggle whether bounce reflections after falling off a ledge without jumping add upward hop velocity on the first bounce.");
ConVar bounce_auto_trigger_min_speed("bounce_auto_trigger_min_speed", "500", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "At what speed the player will auto-bounce when running over bounce paint.");
ConVar look_dependent_auto_long_jump_enabled("look_dependent_auto_long_jump_enabled", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "The player must be looking roughly in the direction she's travelling to get activate an auto long jump.");
ConVar look_dependent_auto_long_jump_min_cos_angle("look_dependent_auto_long_jump_min_cos_angle", "0.7", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "The player must be looking roughly in the direction she's travelling to get activate an auto long jump.");

//speed convars
ConVar sv_speed_paint_acceleration("sv_speed_paint_acceleration", "500.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "How fast the player accelerates on speed paint.");
ConVar sv_speed_paint_ramp_acceleration("sv_speed_paint_ramp_acceleration", "1000.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "How fast the player accelerates on speed paint when on a ramp.");
ConVar sv_speed_paint_on_bounce_deceleration_delay("sv_speed_paint_on_bounce_deceleration_delay", "0.2f", FCVAR_REPLICATED | FCVAR_CHEAT, "How long before starting to decelerate if going from speed to bounce.");
ConVar sv_speed_paint_straf_accel_scale("sv_speed_paint_straf_accel_scale", "0.7f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Scale applied to acceleration when the player is strafing on speed paint.");

//stick convars
//ConVar stick_surface_transition_delay("stick_surface_transition_delay", ".5f", FCVAR_REPLICATED | FCVAR_CHEAT, "How long to wait after transitioning to a new stick surface before the player can transition again.");
#define player_can_unstick_by_pushing 1//ConVar player_can_unstick_by_pushing("player_can_unstick_by_pushing", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Allow/disallow players to walk off a sticky surface into the air with WASD.");

//stick camera
#define stick_cam_correct_pitch							1		//ConVar stick_cam_correct_pitch("stick_cam_correct_pitch", "1", FCVAR_REPLICATED);
#define sv_stick_input_cancel_threshold					1.f		//ConVar sv_stick_input_cancel_threshold("sv_stick_input_cancel_threshold", "1.f", FCVAR_REPLICATED | FCVAR_CHEAT, "Threshold of mouse_y input to cancel correct pitch" );
#define stick_cam_pitch_rate							0.2f	//ConVar stick_cam_pitch_rate("stick_cam_pitch_rate", "0.2f", FCVAR_REPLICATED | FCVAR_CHEAT );
#define stick_cam_roll_rate								0.125f	//ConVar stick_cam_roll_rate("stick_cam_roll_rate", "0.125f", FCVAR_REPLICATED | FCVAR_CHEAT );
#define stick_cam_over_the_top_threshold				0.75f	//ConVar stick_cam_over_the_top_threshold("stick_cam_over_the_top_threshold", "0.75", FCVAR_REPLICATED, "How far up/down the player must look to trigger pitch instead of roll correction" );	// Life won't meet you half way
#define stick_cam_pitch_vs_roll_good_angle_threshold	0.95f	//ConVar stick_cam_pitch_vs_roll_good_angle_threshold("stick_cam_pitch_vs_roll_good_angle_threshold", "0.95f", FCVAR_REPLICATED, "When not overpitched in the target orientation, how close to straight up/down we have to be looking to trigger the pitch transition" );
#define stick_cam_min_rotation_rate						10.f	//ConVar stick_cam_min_rotation_rate("stick_cam_min_rotation_rate", "10.f", FCVAR_REPLICATED, "The rotation rate with which to rotate the player's up vector" );

ConVar sv_contact_region_thickness( "sv_contact_region_thickness", "0.2f", FCVAR_REPLICATED | FCVAR_CHEAT, "The thickness of a contact region (how much the box expands)." );
ConVar sv_clip_contacts_to_portals( "sv_clip_contacts_to_portals", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Enable/Disable clipping contact regions to portal planes." );
ConVar sv_debug_draw_contacts( "sv_debug_draw_contacts", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "0: Dont draw anything.  1: Draw contacts.  2: Draw colored contacts" );
ConVar sv_post_teleportation_box_time( "sv_post_teleportation_box_time", ".0333333f", FCVAR_REPLICATED | FCVAR_CHEAT, "Time to use a slightly expanded box for contacts right after teleportation." );

// Paintable player ConVars
ConVar player_can_use_painted_power("player_can_use_painted_power", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Allow/disallow players to be painted.");
ConVar player_loses_painted_power_over_time("player_loses_painted_power_over_time", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "The player should lose the power after having it for player_paint_effects_duration.");
ConVar player_paint_effects_duration("player_paint_effects_duration", "4.0f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Amount of time player maintains her power after being painted.");
ConVar player_paint_effects_enabled("player_paint_effects_enabled", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Display the visual effects of being painted.");
ConVar player_paint_screen_effect_restart_delay("player_paint_screen_effect_restart_delay", "0.5f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Time to wait between particle system restarts when the player gets painted multiple times in rapid succession.");

//Playtesting convars
ConVar sv_press_jump_to_bounce("sv_press_jump_to_bounce", "3", FCVAR_REPLICATED, "0: Bounce on touch, 1: Bounce on press, 2: Bounce on hold");
ConVar sv_bounce_reflect_enabled("sv_bounce_reflect_enabled", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Enable/Disable reflection on bounce.");

//Wall jump convars
ConVar sv_wall_jump_help("sv_wall_jump_help", "1", FCVAR_REPLICATED, "Enable the wall jump helper to help keep players bouncing between two opposing walls");
ConVar sv_wall_jump_help_threshold("sv_wall_jump_help_threshold", "9.0f", FCVAR_REPLICATED, "Threshold at which the wall jump helper will bring the player's velocity in line with the surface normal");
ConVar sv_wall_jump_help_amount("sv_wall_jump_help_amount", "5.0f", FCVAR_REPLICATED, "Maximum correction amount per wall bounce");
ConVar sv_wall_jump_help_debug("sv_wall_jump_help_debug", "0", FCVAR_REPLICATED);

//Debug convars
ConVar show_player_paint_power_debug( "show_player_paint_power_debug", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar mp_should_gib_bots("mp_should_gib_bots", "1", FCVAR_REPLICATED | FCVAR_CHEAT);
ConVar sv_debug_bounce_reflection("sv_debug_bounce_reflection", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar sv_debug_bounce_reflection_time("sv_debug_bounce_reflection_time", "15.f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar paint_compute_contacts_simd("paint_compute_contacts_simd", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Compute the contacts with paint in fast SIMD (1) or with slower FPU (0)." );

ConVar prevent_crouch_jump("prevent_crouch_jump", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Enable/Disable crouch jump prevention.");

#if defined( PORTAL2_PUZZLEMAKER )
ConVar sv_use_bendy_model("sv_use_bendy_model", "1", FCVAR_REPLICATED, "Use the bendy stick-man as the player model" );
#endif // PORTAL2_PUZZLEMAKER

extern ConVar player_held_object_use_view_model;


extern float IntervalDistance( float x, float x0, float x1 );


//-----------------------------------------------------------------------------
// Consider the weapon's built-in accuracy, this character's proficiency with
// the weapon, and the status of the target. Use this information to determine
// how accurately to shoot at the target.
//-----------------------------------------------------------------------------
Vector CPortal_Player::GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	if ( pWeapon )
		return pWeapon->GetBulletSpread( WEAPON_PROFICIENCY_PERFECT );
	
	return VECTOR_CONE_15DEGREES;
}

void CPortal_Player::GetStepSoundVelocities( float *velwalk, float *velrun )
{
	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if ( ( GetFlags() & FL_DUCKING ) || ( GetMoveType() == MOVETYPE_LADDER ) )
	{
		*velwalk = 10;		// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		*velrun = 60;		
	}
	else
	{
		*velwalk = 90;
		*velrun = 220;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : step - 
//			fvol - 
//			force - force sound to play
//-----------------------------------------------------------------------------
void CPortal_Player::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
#ifndef CLIENT_DLL
	IncrementStepsTaken();
#endif

	// Cache off step side
	int const side = m_Local.m_nStepside;

	// Play the base material step sound
	BaseClass::PlayStepSound( vecOrigin, psurface, fvol, force );

	if ( gpGlobals->maxClients > 1 && !sv_footsteps.GetFloat() )
		return;

#if defined( CLIENT_DLL )
	// during prediction play footstep sounds only once
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;
#endif

	// Play the paint step sound if applicable
	bool shouldPlayPaintStepSound = false;
	if( engine->HasPaintmap() )
	{
		CBaseEntity const* pGroundEntity = GetGroundEntity();
		for( unsigned i = 0; i < PAINT_POWER_TYPE_COUNT; ++i )
		{
			CachedPaintPowerChoiceResult const& result = m_PortalLocal.m_CachedPaintPowerChoiceResults[i];
			CBaseEntity const* pSurfaceEntity = result.surfaceEntity.Get() != NULL ? EntityFromEntityHandle( result.surfaceEntity.Get() ) : NULL;
			if( result.wasValid && pSurfaceEntity == pGroundEntity && result.surfaceNormal.z >= STEEP_SLOPE )
			{
				shouldPlayPaintStepSound = true;
				break;
			}
		}
	}

	if( shouldPlayPaintStepSound )
	{
		surfacedata_t const* pPaintedSurface = physprops->GetSurfaceData( PAINTED_SURFACE_PROPERTY_INDEX );

		if( pPaintedSurface )
		{
			unsigned short const paintStepSoundIndex = side != 0 ? pPaintedSurface->sounds.runStepLeft : pPaintedSurface->sounds.runStepRight;
			CSoundParameters soundParams;
			if( paintStepSoundIndex != 0 &&
				GetParametersForSound( physprops->GetString( paintStepSoundIndex ), soundParams, NULL ) )
			{
				// Emit the paint step sound
				EmitSound_t emitParams( soundParams );
				emitParams.m_flVolume = fvol;
				emitParams.m_pOrigin = &vecOrigin;

				CRecipientFilter filter;
				filter.UsePredictionRules();
				filter.AddRecipientsByPAS( vecOrigin );

				EmitSound( filter, entindex(), emitParams );
			}
		}
	}

	// If this was forced, then the coop step sound probably won't play automatically from
	// an animation event. Make sure it plays in multiplayer.
	if( force )
		PlayCoopStepSound( vecOrigin, side, fvol );
}

void CPortal_Player::PlayCoopStepSound( const Vector& origin, int side, float volume )
{
	if( GameRules()->IsMultiplayer() && !m_bIsBendy )
	{
		const char* soundName = NULL;

		// Play the robotic footstep sounds over the surface footsteps for the surface material
		if( GetTeamNumber() == TEAM_BLUE )
		{
			// Ballbot
			soundName = "CoopBot.CoopBotBallFsRoboticsImpact";
		}
		else
		{
			// Eggbot
			soundName = side != 0 ? "CoopBot.CoopBotEggFsRoboticsL" : "CoopBot.CoopBotEggFsRoboticsR";
		}

		CSoundParameters soundParams;
		if( GetParametersForSound( soundName, soundParams, NULL ) )
		{
			EmitSound_t emitParams( soundParams );
			emitParams.m_flVolume = volume;
			emitParams.m_pOrigin = &origin;

			CRecipientFilter filter;
			filter.UsePredictionRules();
			filter.AddRecipientsByPAS( origin );

			EmitSound( filter, entindex(), emitParams );
		}

	}
}

Activity CPortal_Player::TranslateActivity( Activity baseAct, bool *pRequired /* = NULL */ )
{
	Activity translated = baseAct;

	if ( GetActiveWeapon() )
	{
		translated = GetActiveWeapon()->ActivityOverride( baseAct, pRequired );
	}
	else if (pRequired)
	{
		*pRequired = false;
	}

	return translated;
}

CWeaponPortalBase* CPortal_Player::GetActivePortalWeapon() const
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		return dynamic_cast< CWeaponPortalBase* >( pWeapon );
	}
	else
	{
		return NULL;
	}
}


const Vector& CPortal_Player::WorldSpaceCenter() const
{
	m_vWorldSpaceCenterHolder = GetAbsOrigin();
	//m_vWorldSpaceCenterHolder.z += 0.5f * ( (GetFlags() & FL_DUCKING) ? (VEC_DUCK_HULL_MAX.z) : (VEC_HULL_MAX.z) );
	m_vWorldSpaceCenterHolder += 0.5f * m_flHullHeight * m_PortalLocal.m_StickNormal;
	return m_vWorldSpaceCenterHolder;
}


bool CPortal_Player::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	Assert( ray.m_IsRay );

	// Transform the ray into local space (w.r.t. the player hull)
	VMatrix worldToLocal = SetupMatrixAxisToAxisRot( m_PortalLocal.m_StickNormal, Vector(0, 0, 1) );
	worldToLocal = worldToLocal * SetupMatrixTranslation( -WorldSpaceCenter() );
	Ray_t localRay;
	const Vector start = worldToLocal * ray.m_Start;
	const Vector end = start + worldToLocal.ApplyRotation( ray.m_Delta );
	localRay.Init( start, end );

	if( IntersectRayWithAACylinder( localRay, vec3_origin, 0.5f * GetHullWidth() * PLAYER_HULL_REDUCTION, GetHullHeight(), &tr ) )
	{
		// Transform the start, end, and normal of the trace back into world coordinates
		// These were changed in the call to IntersectRayWithAACylinder() when it succeeded
		const VMatrix localToWorld = worldToLocal.InverseTR();
		tr.startpos = localToWorld * tr.startpos;
		tr.endpos = localToWorld * tr.endpos;
		tr.plane.normal = localToWorld.ApplyRotation( tr.plane.normal );

		tr.hitbox = 0;
		CStudioHdr *pStudioHdr = GetModelPtr( );
		if (!pStudioHdr)
			return false;

		mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( m_nHitboxSet );
		if ( !set || !set->numhitboxes )
			return false;

		mstudiobbox_t *pbox = set->pHitbox( tr.hitbox );
		mstudiobone_t *pBone = pStudioHdr->pBone(pbox->bone);
		tr.surface.name = "**studio**";
		tr.surface.flags = SURF_HITBOX;
		tr.surface.surfaceProps = pBone->GetSurfaceProp();
	}

	return true;
}


float CPortal_Player::GetImplicitVerticalStepSpeed() const
{
	return m_flImplicitVerticalStepSpeed;
}


void CPortal_Player::SetImplicitVerticalStepSpeed( float speed )
{
	Assert( !IS_NAN( speed ) );
	m_flImplicitVerticalStepSpeed = speed;
}


void CPortal_Player::ForceDuckThisFrame( void )
{
	if( (GetFlags() & FL_DUCKING) == 0 )
	{
		EASY_DIFFPRINT( this, "CPortal_Player::ForceDuckThisFrame()" );
		SetGroundEntity( NULL );
		SetGroundChangeTime( gpGlobals->curtime + 0.5f );
		m_Local.m_bDucked = true;
		m_Local.m_bInDuckJump = true;
		/*if( m_Local.m_nDuckJumpTimeMsecs == 0 )
		{
		m_Local.m_nDuckJumpTimeMsecs = GAMEMOVEMENT_JUMP_TIME;
		}*/
		ForceButtons( IN_DUCK );
		ForceButtons( IN_JUMP );
		AddFlag( FL_DUCKING );
		SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
		SetViewOffset( VEC_DUCK_VIEW );
//#if defined( GAME_DLL )
		SetVCollisionState( GetAbsOrigin() + Vector( 0.0f, 0.0f, 18.0f ), GetAbsVelocity(), VPHYS_CROUCH ); //+18 on z to maintain centered state
		if( m_pPhysicsController )
		{
			m_pPhysicsController->SetObject( m_pShadowCrouch );
			m_pPhysicsController->Jump();
		}
		SetTouchedPhysics( true );
//#endif
	}
}


const CPortalPlayerLocalData& CPortal_Player::GetPortalPlayerLocalData() const
{
	return m_PortalLocal;
}


#if 0

//==========================
// ANIMATION CODE
//==========================

// Below this many degrees, slow down turning rate linearly
#define FADE_TURN_DEGREES	45.0f
// After this, need to start turning feet
#define MAX_TORSO_ANGLE		90.0f
// Below this amount, don't play a turning animation/perform IK
#define MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION		15.0f

static ConVar tf2_feetyawrunscale( "tf2_feetyawrunscale", "2", FCVAR_REPLICATED, "Multiplier on tf2_feetyawrate to allow turning faster when running." );
extern ConVar sv_backspeed;
extern ConVar mp_feetyawrate;
extern ConVar mp_facefronttime;
extern ConVar mp_ik;

CPlayerAnimState::CPlayerAnimState( CPortal_Player *outer )
	: m_pOuter( outer )
{
	m_flGaitYaw = 0.0f;
	m_flGoalFeetYaw = 0.0f;
	m_flCurrentFeetYaw = 0.0f;
	m_flCurrentTorsoYaw = 0.0f;
	m_flLastYaw = 0.0f;
	m_flLastTurnTime = 0.0f;
	m_flTurnCorrectionTime = 0.0f;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::Update()
{
	m_angRender = GetOuter()->GetLocalAngles();

	ComputePoseParam_BodyYaw();
	ComputePoseParam_BodyPitch( GetOuter()->GetModelPtr() );
	ComputePoseParam_BodyLookYaw();

	ComputePlaybackRate();

#ifdef CLIENT_DLL
	GetOuter()->UpdateLookAt();
#endif

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePlaybackRate()
{
	// Determine ideal playback rate
	Vector vel;
	GetOuterAbsVelocity( vel );

	float speed = vel.Length2D();

	bool isMoving = ( speed > 0.5f ) ? true : false;

	float maxspeed = GetOuter()->GetSequenceGroundSpeed( GetOuter()->GetSequence() );
	
	if ( isMoving && ( maxspeed > 0.0f ) )
	{
		float flFactor = 1.0f;

		// Note this gets set back to 1.0 if sequence changes due to ResetSequenceInfo below
		GetOuter()->SetPlaybackRate( ( speed * flFactor ) / maxspeed );

		// BUG BUG:
		// This stuff really should be m_flPlaybackRate = speed / m_flGroundSpeed
	}
	else
	{
		GetOuter()->SetPlaybackRate( 1.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBasePlayer
//-----------------------------------------------------------------------------
CPortal_Player *CPlayerAnimState::GetOuter()
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void CPlayerAnimState::EstimateYaw( void )
{
	float dt = gpGlobals->frametime;

	if ( !dt )
	{
		return;
	}

	Vector est_velocity;
	QAngle	angles;

	GetOuterAbsVelocity( est_velocity );

	angles = GetOuter()->GetLocalAngles();

	if ( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		float flYawDiff = angles[YAW] - m_flGaitYaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180)
			flYawDiff -= 360;
		if (flYawDiff < -180)
			flYawDiff += 360;

		if (dt < 0.25)
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		m_flGaitYaw += flYawDiff;
		m_flGaitYaw = m_flGaitYaw - (int)(m_flGaitYaw / 360) * 360;
	}
	else
	{
		m_flGaitYaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);

		if (m_flGaitYaw > 180)
			m_flGaitYaw = 180;
		else if (m_flGaitYaw < -180)
			m_flGaitYaw = -180;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override for backpeddling
// Input  : dt - 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePoseParam_BodyYaw( void )
{
	int iYaw = GetOuter()->LookupPoseParameter( "move_yaw" );
	if ( iYaw < 0 )
		return;

	// view direction relative to movement
	float flYaw;	 

	EstimateYaw();

	QAngle	angles = GetOuter()->GetLocalAngles();
	float ang = angles[ YAW ];
	if ( ang > 180.0f )
	{
		ang -= 360.0f;
	}
	else if ( ang < -180.0f )
	{
		ang += 360.0f;
	}

	// calc side to side turning
	flYaw = ang - m_flGaitYaw;
	// Invert for mapping into 8way blend
	flYaw = -flYaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;

	if (flYaw < -180)
	{
		flYaw = flYaw + 360;
	}
	else if (flYaw > 180)
	{
		flYaw = flYaw - 360;
	}
	
	GetOuter()->SetPoseParameter( iYaw, flYaw );

#ifndef CLIENT_DLL
	//Adrian: Make the model's angle match the legs so the hitboxes match on both sides.
	GetOuter()->SetLocalAngles( QAngle( GetOuter()->GetAnimEyeAngles().x, m_flCurrentFeetYaw, 0 ) );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr )
{
	// Get pitch from v_angle
	float flPitch = GetOuter()->GetLocalAngles()[ PITCH ];

	if ( flPitch > 180.0f )
	{
		flPitch -= 360.0f;
	}
	flPitch = clamp( flPitch, -90, 90 );

	QAngle absangles = GetOuter()->GetAbsAngles();
	absangles.x = 0.0f;
	m_angRender = absangles;

	// See if we have a blender for pitch
	GetOuter()->SetPoseParameter( pStudioHdr, "aim_pitch", -flPitch );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : goal - 
//			maxrate - 
//			dt - 
//			current - 
// Output : int
//-----------------------------------------------------------------------------
int CPlayerAnimState::ConvergeAngles( float goal,float maxrate, float dt, float& current )
{
	int direction = TURN_NONE;

	float anglediff = goal - current;
	float anglediffabs = fabs( anglediff );

	anglediff = AngleNormalize( anglediff );

	float scale = 1.0f;
	if ( anglediffabs <= FADE_TURN_DEGREES )
	{
		scale = anglediffabs / FADE_TURN_DEGREES;
		// Always do at least a bit of the turn ( 1% )
		scale = clamp( scale, 0.01f, 1.0f );
	}

	float maxmove = maxrate * dt * scale;

	if ( fabs( anglediff ) < maxmove )
	{
		current = goal;
	}
	else
	{
		if ( anglediff > 0 )
		{
			current += maxmove;
			direction = TURN_LEFT;
		}
		else
		{
			current -= maxmove;
			direction = TURN_RIGHT;
		}
	}

	current = AngleNormalize( current );

	return direction;
}

void CPlayerAnimState::ComputePoseParam_BodyLookYaw( void )
{
	QAngle absangles = GetOuter()->GetAbsAngles();
	absangles.y = AngleNormalize( absangles.y );
	m_angRender = absangles;

	// See if we even have a blender for pitch
	int upper_body_yaw = GetOuter()->LookupPoseParameter( "aim_yaw" );
	if ( upper_body_yaw < 0 )
	{
		return;
	}

	// Assume upper and lower bodies are aligned and that we're not turning
	float flGoalTorsoYaw = 0.0f;
	int turning = TURN_NONE;
	float turnrate = 360.0f;

	Vector vel;
	
	GetOuterAbsVelocity( vel );

	bool isMoving = ( vel.Length() > 1.0f ) ? true : false;

	if ( !isMoving )
	{
		// Just stopped moving, try and clamp feet
		if ( m_flLastTurnTime <= 0.0f )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
			m_flLastYaw			= GetOuter()->GetAnimEyeAngles().y;
			// Snap feet to be perfectly aligned with torso/eyes
			m_flGoalFeetYaw		= GetOuter()->GetAnimEyeAngles().y;
			m_flCurrentFeetYaw	= m_flGoalFeetYaw;
			m_nTurningInPlace	= TURN_NONE;
		}

		// If rotating in place, update stasis timer
		if ( m_flLastYaw != GetOuter()->GetAnimEyeAngles().y )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
			m_flLastYaw			= GetOuter()->GetAnimEyeAngles().y;
		}

		if ( m_flGoalFeetYaw != m_flCurrentFeetYaw )
		{
			m_flLastTurnTime	= gpGlobals->curtime;
		}

		turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );

		QAngle eyeAngles = GetOuter()->GetAnimEyeAngles();
		QAngle vAngle = GetOuter()->GetLocalAngles();

		// See how far off current feetyaw is from true yaw
		float yawdelta = GetOuter()->GetAnimEyeAngles().y - m_flCurrentFeetYaw;
		yawdelta = AngleNormalize( yawdelta );

		bool rotated_too_far = false;

		float yawmagnitude = fabs( yawdelta );

		// If too far, then need to turn in place
		if ( yawmagnitude > 45 )
		{
			rotated_too_far = true;
		}

		// Standing still for a while, rotate feet around to face forward
		// Or rotated too far
		// FIXME:  Play an in place turning animation
		if ( rotated_too_far || 
			( gpGlobals->curtime > m_flLastTurnTime + mp_facefronttime.GetFloat() ) )
		{
			m_flGoalFeetYaw		= GetOuter()->GetAnimEyeAngles().y;
			m_flLastTurnTime	= gpGlobals->curtime;

		/*	float yd = m_flCurrentFeetYaw - m_flGoalFeetYaw;
			if ( yd > 0 )
			{
				m_nTurningInPlace = TURN_RIGHT;
			}
			else if ( yd < 0 )
			{
				m_nTurningInPlace = TURN_LEFT;
			}
			else
			{
				m_nTurningInPlace = TURN_NONE;
			}

			turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );
			yawdelta = GetOuter()->GetAnimEyeAngles().y - m_flCurrentFeetYaw;*/

		}

		// Snap upper body into position since the delta is already smoothed for the feet
		flGoalTorsoYaw = yawdelta;
		m_flCurrentTorsoYaw = flGoalTorsoYaw;
	}
	else
	{
		m_flLastTurnTime = 0.0f;
		m_nTurningInPlace = TURN_NONE;
		m_flCurrentFeetYaw = m_flGoalFeetYaw = GetOuter()->GetAnimEyeAngles().y;
		flGoalTorsoYaw = 0.0f;
		m_flCurrentTorsoYaw = GetOuter()->GetAnimEyeAngles().y - m_flCurrentFeetYaw;
	}


	if ( turning == TURN_NONE )
	{
		m_nTurningInPlace = turning;
	}

	if ( m_nTurningInPlace != TURN_NONE )
	{
		// If we're close to finishing the turn, then turn off the turning animation
		if ( fabs( m_flCurrentFeetYaw - m_flGoalFeetYaw ) < MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION )
		{
			m_nTurningInPlace = TURN_NONE;
		}
	}

	// Rotate entire body into position
	absangles = GetOuter()->GetAbsAngles();
	absangles.y = m_flCurrentFeetYaw;
	m_angRender = absangles;

	GetOuter()->SetPoseParameter( upper_body_yaw, clamp( m_flCurrentTorsoYaw, -60.0f, 60.0f ) );

	/*
	// FIXME: Adrian, what is this?
	int body_yaw = GetOuter()->LookupPoseParameter( "body_yaw" );

	if ( body_yaw >= 0 )
	{
		GetOuter()->SetPoseParameter( body_yaw, 30 );
	}
	*/

}


 
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CPlayerAnimState::BodyYawTranslateActivity( Activity activity )
{
	// Not even standing still, sigh
	if ( activity != ACT_IDLE )
		return activity;

	// Not turning
	switch ( m_nTurningInPlace )
	{
	default:
	case TURN_NONE:
		return activity;
	/*
	case TURN_RIGHT:
		return ACT_TURNRIGHT45;
	case TURN_LEFT:
		return ACT_TURNLEFT45;
	*/
	case TURN_RIGHT:
	case TURN_LEFT:
		return mp_ik.GetBool() ? ACT_TURN : activity;
	}

	Assert( 0 );
	return activity;
}

const QAngle& CPlayerAnimState::GetRenderAngles()
{
	return m_angRender;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CPlayerAnimState::Teleport( Vector *pOldOrigin, QAngle *pOldAngles )
{
	QAngle absangles = GetOuter()->GetAbsAngles();
	absangles.x = 0.0f;
	m_angRender = absangles;

	m_flCurrentFeetYaw = m_flGoalFeetYaw = m_flLastYaw = m_angRender.y;
	m_flLastTurnTime = 0.0f;
	m_nTurningInPlace = TURN_NONE;
}

void CPlayerAnimState::GetOuterAbsVelocity( Vector& vel )
{
#if defined( CLIENT_DLL )
	GetOuter()->EstimateAbsVelocity( vel );
#else
	vel = GetOuter()->GetAbsVelocity();
#endif
}
#endif // #if 0

bool CPortal_Player::IsUsingVMGrab( void )
{
	return m_bUsingVMGrabState;
}

bool CPortal_Player::WantsVMGrab( void )
{
	if ( player_held_object_use_view_model.GetInt() >= 0 )
	{
		return player_held_object_use_view_model.GetBool();
	}

	return m_bUseVMGrab;
}

void CPortal_Player::ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldingThis )
{
	m_bHeldObjectOnOppositeSideOfPortal = false;

#if !defined CLIENT_DLL 
	if ( PhysIsInCallback() )
	{
		variant_t value;
		g_EventQueue.AddEvent( this, "ForceDropPhysObjects", value, 0.01f, pOnlyIfHoldingThis, this );
		return;
	}
#endif 

	m_bForcingDrop = true;

	// Drop any objects being handheld.
	ClearUseEntity();

	m_bForcingDrop = false;
}


//-----------------------------------------------------------------------------
// Purpose: Overload for portal-- Our player can lift his own mass.
// Input  : *pObject - The object to lift
//			bLimitMassAndSize - check for mass/size limits
//-----------------------------------------------------------------------------
void CPortal_Player::PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize )
{
	// can't pick up what you're standing on
	if ( GetGroundEntity() == pObject )
		return;


	if ( bLimitMassAndSize == true )
	{
		if ( CBasePlayer::CanPickupObject( pObject, PORTAL_PLAYER_MAX_LIFT_MASS, PORTAL_PLAYER_MAX_LIFT_SIZE ) == false )
			return;
	}

	// Can't be picked up if NPCs are on me
	if ( pObject->HasNPCsOnIt() )
		return;

	PlayerPickupObject( this, pObject );

#if !defined CLIENT_DLL 
	FireConcept( TLK_PLAYER_PICKED_UP_ITEM );
#endif 
}

bool CPortal_Player::IsInvalidHandoff( CBaseEntity *pObject )
{
	CBasePlayer *pPlayer = GetPlayerHoldingEntity( pObject );
	if ( !pPlayer || pPlayer == this )
	{
		// No one is holding it now or we are holding it, it's all good
		return false;
	}
	else
	{
		// Make sure it has a clear shot to the holding player
		trace_t tr;
		UTIL_TraceLine( pObject->GetAbsOrigin(), pPlayer->EyePosition(), MASK_SOLID, pObject, COLLISION_GROUP_NONE, &tr );

		if ( !tr.startsolid && ( tr.fraction >= 1.0f || tr.m_pEnt == pPlayer ) )
		{
			// Make sure it has a clear shot to the grabbing player
			trace_t tr;
			UTIL_TraceLine( pObject->GetAbsOrigin(), EyePosition(), MASK_SOLID, pObject, COLLISION_GROUP_NONE, &tr );

			if ( !tr.startsolid && ( tr.fraction >= 1.0f || tr.m_pEnt == this ) )
			{
				return false;
			}
			else
			{
				// It's in solid or colliding with something other than the grabbing player
				return true;
			}
		}
		else
		{
			// It's in solid or colliding with something other than the holding player
			return true;
		}
	}
}

// Calls FindUseEntity every tick for a period of time
// I'm REALLY sorry about this. This will clean up quite a bit
// once grab controllers are on the client.
void CPortal_Player::PollForUseEntity( bool bBasicUse, CBaseEntity **ppUseEnt, CPortal_Base2D **ppUseThroughPortal )
{
	bool bTryGrab = false;

	CBaseEntity *pSendEntity = NULL;

#ifdef CLIENT_DLL
	pSendEntity = m_hUseEntToSend.Get();
#endif

	if ( !m_bIsHoldingSomething && !pSendEntity && gpGlobals->curtime > m_flAutoGrabLockOutTime + 1.0f )
	{
		// We're not preventing new grabs... check the auto grab state

		if ( bBasicUse || ( GameRules() && GameRules()->IsMultiplayer() && GetAbsVelocity().AsVector2D().Length() > 275.0f ) )
		{
			// They either just pressed used or are flying fast in multiplayer... mark this time to start auto grabbing
			m_flUseKeyStartTime = gpGlobals->curtime;
		}

		// If they're currently autograbbing, try it out this frame
		bTryGrab = ( gpGlobals->curtime < m_flUseKeyStartTime + sv_use_trace_duration.GetFloat() );
	}

	if ( bTryGrab )
	{
		// Trace for a use entity
		CPortal_Base2D *pThroughPortal = NULL;
		*ppUseEnt = FindUseEntity( &pThroughPortal );
		*ppUseThroughPortal = pThroughPortal;

		if ( *ppUseEnt )
		{
			bool bIsPhysics = ( (*ppUseEnt)->GetMoveType() == MOVETYPE_VPHYSICS );

			if ( bIsPhysics || bBasicUse )
			{
				if ( bIsPhysics )
				{
					// Prevent redropping/regrabbing when mashing +use
					m_flAutoGrabLockOutTime = gpGlobals->curtime;
				}

				// You need to be looking in it's general direction to auto grab it
				Vector vObjectDir = (*ppUseEnt)->GetAbsOrigin() - EyePosition();
				VectorNormalize( vObjectDir );

				Vector vEyeForward;
				EyeVectors( &vEyeForward );
				if ( bBasicUse || vObjectDir.Dot( vEyeForward ) > 0.2f )
				{
					m_flUseKeyStartTime = -sv_use_trace_duration.GetFloat() - 1.0f;
				}
				else
				{
					// You're probably going to smack in to it, don't actually auto grab it
					*ppUseEnt  = NULL;
				}
			}
			else
			{
				// Don't autograb non-physics
				*ppUseEnt  = NULL;
			}
		}
	}
}

CBaseEntity *CPortal_Player::FindUseEntity( CPortal_Base2D **pThroughPortal )
{
	Vector forward, up;
	EyeVectors( &forward, NULL, &up );

	Vector vNetworkPosOffset = vec3_origin;
#ifdef CLIENT_DLL
	vNetworkPosOffset = GetNetworkOrigin() - GetAbsOrigin();
#endif

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition() + vNetworkPosOffset;

	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	// BUG 61818: Allowing pickup through playerclips because we'd like to be abled to drop through them.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS /*| CONTENTS_PLAYERCLIP*/;

	UTIL_TraceLine( searchCenter, searchCenter + forward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr );
	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = NULL;
	CBaseEntity *pObject = tr.m_pEnt;

#if 1
	int count = 0;
	// UNDONE: Might be faster to just fold this range into the sphere query
	const int NUM_TANGENTS = 10;
	while ( !IsUseableEntity(pObject, 0) && count < NUM_TANGENTS)
	{
		// trace a box at successive angles down
		//							45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
		// then up (useful in portal when flying past a use target quickly)
		//							-20 deg, -30 deg, -45 deg  
		const float tangents[NUM_TANGENTS] = { 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f,
			-0.3639702342f, -0.57735026919f, -1 };
		Vector down = forward - tangents[count]*up;
		VectorNormalize(down);
		UTIL_TraceHull( searchCenter, searchCenter + down * 72, -Vector(16,16,16), Vector(16,16,16), useableContents, this, COLLISION_GROUP_NONE, &tr );
		pObject = tr.m_pEnt;
		count++;
	}
#endif

	float nearestDot = CONE_90_DEGREES;
	if ( IsUseableEntity(pObject, 0) )
	{
		Vector delta = tr.endpos - tr.startpos;
		float centerZ = CollisionProp()->WorldSpaceCenter().z;
		delta.z = IntervalDistance( tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z );
		float dist = delta.Length();
		if ( dist < PLAYER_USE_RADIUS && !IsInvalidHandoff( pObject ) )
		{
			if ( sv_debug_player_use.GetBool() )
			{
				debugoverlay->AddLineOverlay( searchCenter, tr.endpos, 0, 255, 0, true, 30 );
				//FIXME: Supposed to be a cross
				Vector boxSize( 4,4,4 );
				debugoverlay->AddBoxOverlay( tr.endpos, -boxSize, boxSize, vec3_angle, 255, 0, 0, 255, 30 );
			}

			return pObject;
		}
	}

	CBaseEntity *pFoundByTrace = pObject;


	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if ( GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND) )
	{
		pNearest = GetGroundEntity();
		nearestDot = CONE_45_DEGREES;
	}

	for ( CEntitySphereQuery sphere( searchCenter, PLAYER_USE_RADIUS ); ( pObject = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( !pObject )
			continue;

		if ( !IsUseableEntity( pObject, FCAP_USE_IN_RADIUS ) )
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint( searchCenter, &point );

		Vector dir = point - searchCenter;
		VectorNormalize(dir);
		float dot = DotProduct( dir, forward );

		// Need to be looking at the object more or less
		if ( dot < sv_player_use_cone_size.GetFloat() )
			continue;

		if ( IsInvalidHandoff( pObject ) )
			continue;

		if ( dot > nearestDot )
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine( searchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded );

			if ( trCheckOccluded.fraction == 1.0 || trCheckOccluded.m_pEnt == pObject )
			{
				pNearest = pObject;
				nearestDot = dot;
			}
		}
	}

	if ( sv_debug_player_use.GetBool() )
	{
		if ( !pNearest )
		{
			debugoverlay->AddLineOverlay( searchCenter, tr.endpos, 0, 255, 0, true, 30 );
			Vector boxSize( 4,4,4 );
			debugoverlay->AddBoxOverlay( tr.endpos, -boxSize, boxSize, vec3_angle, 255, 0, 0, 255, 30 );
		}
		else if ( pNearest == pFoundByTrace )
		{
			debugoverlay->AddLineOverlay( searchCenter, tr.endpos, 0, 255, 0, true, 30 );
			Vector boxSize( 4,4,4 );
			debugoverlay->AddBoxOverlay( tr.endpos, -boxSize, boxSize, vec3_angle, 255, 0, 0, 255, 30 );
		}
		else
		{
			Vector boxSize( 8,8,8 );
			debugoverlay->AddBoxOverlay( pNearest->WorldSpaceCenter(), -boxSize, boxSize, vec3_angle, 255, 0, 0, 255, 30 );
		}
	}

	if ( pNearest == NULL )
	{
		Vector forward;
		EyeVectors( &forward, NULL, NULL );
		Vector start = EyePosition();

		Ray_t rayPortalTest;
		rayPortalTest.Init( start, start + forward * PLAYER_USE_RADIUS );

		float fMustBeCloserThan = 1.0f;
		SetHeldObjectPortal( UTIL_Portal_FirstAlongRay( rayPortalTest, fMustBeCloserThan ) );

		if ( GetHeldObjectPortal() )
		{
			pNearest = FindUseEntityThroughPortal();
			if ( pNearest )
			{
				if ( pThroughPortal )
				{
					*pThroughPortal = GetHeldObjectPortal();
				}
			}
		}
	}


	return pNearest;
}

CBaseEntity* CPortal_Player::FindUseEntityThroughPortal( void )
{
	Vector forward, up;
	EyeVectors( &forward, NULL, &up );

	CPortal_Base2D *pPortal = GetHeldObjectPortal();

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition();

	Vector vTransformedForward, vTransformedUp, vTransformedSearchCenter;

	VMatrix matThisToLinked = pPortal->MatrixThisToLinked();
	UTIL_Portal_PointTransform( matThisToLinked, searchCenter, vTransformedSearchCenter );
	UTIL_Portal_VectorTransform( matThisToLinked, forward, vTransformedForward );
	UTIL_Portal_VectorTransform( matThisToLinked, up, vTransformedUp );


	// NOTE: Some debris objects are useable too, so hit those as well
	// A button, etc. can be made out of clip brushes, make sure it's +useable via a traceline, too.
	int useableContents = MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_PLAYERCLIP;

	//UTIL_TraceLine( vTransformedSearchCenter, vTransformedSearchCenter + vTransformedForward * 1024, useableContents, this, COLLISION_GROUP_NONE, &tr );
	Ray_t rayLinked;
	rayLinked.Init( searchCenter, searchCenter + forward * 1024 );
	UTIL_PortalLinked_TraceRay( pPortal, rayLinked, useableContents, this, COLLISION_GROUP_NONE, &tr );

	// try the hit entity if there is one, or the ground entity if there isn't.
	CBaseEntity *pNearest = NULL;
	CBaseEntity *pObject = tr.m_pEnt;
	int count = 0;
	// UNDONE: Might be faster to just fold this range into the sphere query
	const int NUM_TANGENTS = 7;
	while ( !IsUseableEntity(pObject, 0) && count < NUM_TANGENTS)
	{
		// trace a box at successive angles down
		//							45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
		const float tangents[NUM_TANGENTS] = { 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
		Vector down = vTransformedForward - tangents[count]*vTransformedUp;
		VectorNormalize(down);
		UTIL_TraceHull( vTransformedSearchCenter, vTransformedSearchCenter + down * 72, -Vector(16,16,16), Vector(16,16,16), useableContents, this, COLLISION_GROUP_NONE, &tr );
		pObject = tr.m_pEnt;
		count++;
	}
	float nearestDot = CONE_90_DEGREES;
	if ( IsUseableEntity(pObject, 0) )
	{
		Vector delta = tr.endpos - tr.startpos;
		float centerZ = CollisionProp()->WorldSpaceCenter().z;
		delta.z = IntervalDistance( tr.endpos.z, centerZ + CollisionProp()->OBBMins().z, centerZ + CollisionProp()->OBBMaxs().z );
		float dist = delta.Length();
		if ( dist < PLAYER_USE_RADIUS && !IsInvalidHandoff( pObject ) )
		{
			return pObject;
		}
	}

	// check ground entity first
	// if you've got a useable ground entity, then shrink the cone of this search to 45 degrees
	// otherwise, search out in a 90 degree cone (hemisphere)
	if ( GetGroundEntity() && IsUseableEntity(GetGroundEntity(), FCAP_USE_ONGROUND) )
	{
		pNearest = GetGroundEntity();
		nearestDot = CONE_45_DEGREES;
	}

	for ( CEntitySphereQuery sphere( vTransformedSearchCenter, PLAYER_USE_RADIUS ); ( pObject = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( !pObject )
			continue;

		if ( !IsUseableEntity( pObject, FCAP_USE_IN_RADIUS ) )
			continue;

		// see if it's more roughly in front of the player than previous guess
		Vector point;
		pObject->CollisionProp()->CalcNearestPoint( vTransformedSearchCenter, &point );

		Vector dir = point - vTransformedSearchCenter;
		VectorNormalize(dir);
		float dot = DotProduct( dir, vTransformedForward );

		// Need to be looking at the object more or less
		if ( dot < 0.8 )
			continue;

		if ( IsInvalidHandoff( pObject ) )
			continue;

		if ( dot > nearestDot )
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			trace_t trCheckOccluded;
			UTIL_TraceLine( vTransformedSearchCenter, point, useableContents, this, COLLISION_GROUP_NONE, &trCheckOccluded );

			if ( trCheckOccluded.fraction == 1.0 || trCheckOccluded.m_pEnt == pObject )
			{
				pNearest = pObject;
				nearestDot = dot;
			}
		}
	}

	return pNearest;
}

//////////////////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////////////////
#if defined CLIENT_DLL 

bool CPortal_Player::IsPotatosOn( void )
{
	return m_bPotatos;
}

#endif

//////////////////////////////////////////////////////////////////////////
// PAINT SECTION
//////////////////////////////////////////////////////////////////////////

bool CPortal_Player::WantsToSwapGuns( void )
{
	return m_bWantsToSwapGuns;
}


const Vector& CPortal_Player::GetPrevGroundNormal() const
{
	return m_vPrevGroundNormal;
}


void CPortal_Player::SetPrevGroundNormal( const Vector& vPrevNormal )
{
	m_vPrevGroundNormal = vPrevNormal;
}


float CPortal_Player::PredictedAirTimeEnd( void )
{
	if ( m_PortalLocal.m_hTractorBeam.Get() )
	{
		// Flying in a TBeam!
		return 4.0f;
	}

	float fSeconds = 0.0f;

	if ( GetGroundEntity() == NULL )
	{
#ifdef CLIENT_DLL
		Vector vPos = GetNetworkOrigin();
#else
		Vector vPos = GetAbsOrigin();
#endif

		trace_t tr;
		tr.fraction = 1.0f;

		const float fStep = 0.4f;

		for ( float i = 0.0f; i < 4.0f && tr.fraction >= 1.0f; i += fStep )
		{
			Vector vEndPos = vPos + ( GetAbsVelocity() * fStep - Vector( 0.0f, 0.0f, sv_gravity.GetFloat() * i ) );
			UTIL_TraceEntity( this, vPos, vEndPos, MASK_PLAYERSOLID, &tr );

			fSeconds += tr.fraction * fStep;
			vPos = vEndPos;
		}

		return fSeconds;
	}
	else
	{
		return 0.0f;
	}
}

float CPortal_Player::PredictedBounce( void )
{
	float fSeconds = 0.0f;

	if ( m_PortalLocal.m_hTractorBeam.Get() || GetAirTime() < 1.0f )
	{
		// We haven't been falling long enough
		m_PlayerAnimState->m_fPrevBouncePredict = 4.0f;
		return m_PlayerAnimState->m_fPrevBouncePredict;
	}

	if ( m_PlayerAnimState->m_fPrevBouncePredict > 1.0f && gpGlobals->curtime < m_PlayerAnimState->m_fNextBouncePredictTime )
	{
		// We predicted too recently and aren't near a bounce, reuse the last value we calculated
		return m_PlayerAnimState->m_fPrevBouncePredict;
	}

	// Repredict only 5 times per second
	m_PlayerAnimState->m_fNextBouncePredictTime = gpGlobals->curtime + 0.2f;

#ifdef CLIENT_DLL
	Vector vPos = GetNetworkOrigin();
#else
	Vector vPos = GetAbsOrigin();
#endif

	trace_t tr;
	tr.fraction = 1.0f;

	const float fStep = 0.4f;

	for ( float i = 0.0f; i < 4.0f && tr.fraction >= 1.0f; i += fStep )
	{
		Vector vEndPos = vPos + ( GetAbsVelocity() * fStep - Vector( 0.0f, 0.0f, sv_gravity.GetFloat() * i ) );
		UTIL_TraceEntity( this, vPos, vEndPos, MASK_PLAYERSOLID, &tr );

		fSeconds += tr.fraction * fStep;
		vPos = vEndPos;
	}

	fSeconds -= tr.fraction * fStep;

	bool bTouched = false;
	float fLeastFraction = tr.fraction;

	Ray_t ray;
	ray.Init( tr.startpos, vPos, GetHullMins(), GetHullMaxs() );

	for ( int i = 0; i < ITriggerCatapultAutoList::AutoList().Count(); ++i )
	{
		CTriggerCatapult *pCatapult = static_cast< CTriggerCatapult* >( ITriggerCatapultAutoList::AutoList()[ i ] );

		trace_t trTemp;
		enginetrace->ClipRayToEntity( ray, MASK_SHOT, pCatapult, &trTemp );

		if ( trTemp.startsolid )
		{
			fLeastFraction = 0.0f;
			tr = trTemp;
			bTouched = true;
			break;
		}
		else if ( trTemp.fraction < 1.0f || trTemp.m_pEnt == pCatapult )
		{
			if ( fLeastFraction >= trTemp.fraction )
			{
				fLeastFraction = trTemp.fraction;
				tr = trTemp;
				bTouched = true;
			}
		}
	}

	fSeconds += tr.fraction * fStep;

	if ( bTouched )
	{
		m_PlayerAnimState->m_fPrevBouncePredict = fSeconds;
		return m_PlayerAnimState->m_fPrevBouncePredict;
	}

	m_PlayerAnimState->m_fPrevBouncePredict = 4.0f;
	return m_PlayerAnimState->m_fPrevBouncePredict;
}

void CPortal_Player::SharedSpawn()
{
	BaseClass::SharedSpawn();
	SetCollisionBounds( GetHullMins(), GetHullMaxs() );
	SetMaxSpeed( sv_speed_normal.GetFloat() );

	if( PAINTED_SURFACE_PROPERTY_INDEX == -1 )
		PAINTED_SURFACE_PROPERTY_INDEX = physprops->GetSurfaceIndex( "painted_surface" );
}


const char *CPortal_Player::GetPlayerModelName( void )
{	
	const char *pszCurrentModelName = g_pszPlayerModel;

#if defined( PORTAL2_PUZZLEMAKER )
    // Reconstruct the community map number from its component pieces
	extern ConVar cm_current_community_map;
    const uint64 unCommunityMapId = (uint64) atol(cm_current_community_map.GetString());
	char szFixedMapName[MAX_PATH];
#ifdef CLIENT_DLL
	const char *pszMapName = engine->GetLevelNameShort();
#else
	const char *pszMapName = gpGlobals->mapname.ToCStr();
#endif
	V_strncpy( szFixedMapName, pszMapName, ARRAYSIZE( szFixedMapName ) );
	V_FixSlashes( szFixedMapName );
	bool bPuzzleMakerMap = !V_strnicmp( szFixedMapName, "puzzlemaker\\", V_strlen("puzzlemaker\\") );

#endif // PORTAL2_PUZZLEMAKER
	if ( GameRules()->IsMultiplayer() )
	{
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
		int iBot = ( GetTeamNumber() == TEAM_BLUE ) ? P2BOT_ATLAS : P2BOT_PBODY;
		m_bIsBendy = false;

		// See if we have a misc item equipped that modifies our skin
		CEconItemView *pItem = m_Inventory.GetItemInLoadout( iBot, LOADOUT_POSITION_SKIN );
		if ( pItem && pItem->IsValid() )
		{
			if ( !Q_strcmp( pItem->GetStaticData()->GetItemClass(), "skin" ) )
				return pItem->GetPlayerDisplayModel(iBot);
		}
#endif

		if ( GetTeamNumber() == TEAM_BLUE )
		{
			pszCurrentModelName = GetBallBotModel();
		}
		else
		{
			pszCurrentModelName = GetEggBotModel();
		}
	}
#if defined( PORTAL2_PUZZLEMAKER )
	else if ( ( unCommunityMapId != 0 || bPuzzleMakerMap ) && sv_use_bendy_model.GetBool() )
	{
		pszCurrentModelName = "models/info_character/info_character_player.mdl";
		m_bIsBendy = true;
	}
#endif

	return pszCurrentModelName;
}


void CPortal_Player::UpdatePaintedPower()
{
#ifndef CLIENT_DLL

	if( player_loses_painted_power_over_time.GetBool() &&
		m_PortalLocal.m_PaintedPowerTimer.IsElapsed() )
	{
		CleansePaint();
	}

#else	// elif defined( CLIENT_DLL )

	// If there's still time remaining
	if( !m_PortalLocal.m_PaintedPowerTimer.IsElapsed() )
	{
		// Update paint screen effect if this player is in any split screen view
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
		{
			if( C_BasePlayer::GetLocalPlayer( nSlot ) == this && !m_PaintScreenSpaceEffect.IsValid() )
			{
				MDLCACHE_CRITICAL_SECTION();
				m_PaintScreenSpaceEffect = ParticleProp()->Create( PAINT_SCREEN_EFFECT, PATTACH_CUSTOMORIGIN );
				if( m_PaintScreenSpaceEffect.IsValid() )
				{
					// Make sure the particle system isn't automatically drawn with the viewmodel
					m_PaintScreenSpaceEffect->m_pDef->SetDrawThroughLeafSystem( false );
					m_PaintScreenSpaceEffect->m_pDef->m_nMaxParticles = 512;

					// Set the control points
					ParticleProp()->AddControlPoint( m_PaintScreenSpaceEffect, 1, this, PATTACH_CUSTOMORIGIN );
					m_PaintScreenSpaceEffect->SetControlPoint( 0, GetAbsOrigin() );
					m_PaintScreenSpaceEffect->SetControlPoint( 1, GetAbsOrigin() );
					m_PaintScreenSpaceEffect->SetControlPointEntity( 0, this );
					m_PaintScreenSpaceEffect->SetControlPointEntity( 1, this );
				}

				// Only do this once
				break;
			}
		}

		// commenting out the 3rd person drop effect
		/*
		// Update paint drip effect
		if( m_PaintDripEffect.IsValid() )
		{
			m_PaintDripEffect->SetNeedsBBoxUpdate( true );
			m_PaintDripEffect->SetSortOrigin( GetAbsOrigin() );
		}
		else
		{
			MDLCACHE_CRITICAL_SECTION();
			m_PaintDripEffect = ParticleProp()->Create( PAINT_DRIP_EFFECT, PATTACH_ABSORIGIN_FOLLOW );
			if( m_PaintDripEffect.IsValid() )
			{
				ParticleProp()->AddControlPoint( m_PaintDripEffect, 1, this, PATTACH_ABSORIGIN_FOLLOW );
				m_PaintDripEffect->SetControlPoint( 0, GetAbsOrigin() );
				m_PaintDripEffect->SetControlPoint( 1, GetAbsOrigin() );
				m_PaintDripEffect->SetControlPointEntity( 0, this );
				m_PaintDripEffect->SetControlPointEntity( 1, this );
			}
		}
		*/
	}
	// The painted power wore off
	else
	{
		InvalidatePaintEffects();
	}

#endif	// ifndef CLIENT_DLL
}


void CPortal_Player::UpdateAirInputScaleFadeIn()
{
	if( !IsInactivePower( GetPaintPower( BOUNCE_POWER ) ) )
	{
		const bool shouldReturnFullInput = IsDeactivatingPower( GetPaintPower( BOUNCE_POWER ) ) && GetGroundEntity() != NULL;
		m_PortalLocal.m_flAirInputScale = shouldReturnFullInput ? 1.0f : clamp( m_PortalLocal.m_flAirInputScale + gpGlobals->frametime / BOUNCE_PAINT_INPUT_DAMP_TIME, 0.0f, 1.0f );
	}
	else if( !IsInactivePower( GetPaintPower( SPEED_POWER ) ) )
	{
		//const bool doneDeactivating = IsDeactivatingPower( GetPaintPower( SPEED_POWER ) ) && MaxSpeed() == sv_speed_normal.GetFloat() && GetGroundEntity() != NULL;
		//m_PortalLocal.m_flAirInputScale = doneDeactivating ? 1.0f : clamp( m_PortalLocal.m_flAirInputScale + gpGlobals->frametime / SPEED_PAINT_AIR_INPUT_DAMP_TIME, 0.0f, 1.0f );

		// HACK: For the playtest, if speed and only speed is not in the inactive state,
		//		 the player has limited air control.
		m_PortalLocal.m_flAirInputScale = GetGroundEntity() ? 1.0f : MIN_SPEED_PAINT_AIR_INPUT_START_DAMPING;
	}
	else
	{
		m_PortalLocal.m_flAirInputScale = 1.0f;
	}

	//float inputScale = m_PortalLocal.m_flAirInputScale;
	//DevMsg( "Input Scale: %f\n", inputScale );
}


void CPortal_Player::UpdateInAirState()
{
	const InAirState state = m_PortalLocal.m_InAirState;

	// The player jumped
	if( JumpedThisFrame() )
		m_PortalLocal.m_InAirState = IN_AIR_JUMPED;

	// The player bounced
	if( BouncedThisFrame() )
		m_PortalLocal.m_InAirState = IN_AIR_BOUNCED;

	// The player is on the ground
	if( GetGroundEntity() )
		m_PortalLocal.m_InAirState = ON_GROUND;

	// If the player was on the ground but suddenly isn't and didn't jump or bounce
	if( state == ON_GROUND && GetGroundEntity() == NULL && !JumpedThisFrame() )
	{
		//EASY_DIFFPRINT( this, "CPortal_Player::UpdateInAirState() player fell" );
		// The player fell
		m_PortalLocal.m_InAirState = IN_AIR_FELL;
	}

	// Reset whether the player jumped or bounced this frame
	SetJumpedThisFrame( false );
	SetBouncedThisFrame( false );
}


void CPortal_Player::CachePaintPowerChoiceResults( const PaintPowerChoiceResultArray& choiceInfo )
{
	int const count = MIN( PAINT_POWER_TYPE_COUNT, choiceInfo.Count() );
	if( count == 0 )
	{
		for( int i = 0; i < PAINT_POWER_TYPE_COUNT; ++i )
		{
			m_PortalLocal.m_CachedPaintPowerChoiceResults[i].Initialize();
		}
	}
	else
	{
		for( int i = 0; i < count; ++i )
		{
			bool const wasValid = choiceInfo[i].pPaintPower != NULL;
			m_PortalLocal.m_CachedPaintPowerChoiceResults[i].wasValid = wasValid;
			m_PortalLocal.m_CachedPaintPowerChoiceResults[i].wasIgnored = choiceInfo[i].bWasIgnored;
			if( wasValid )
			{
				m_PortalLocal.m_CachedPaintPowerChoiceResults[i].surfaceEntity = choiceInfo[i].pPaintPower->m_HandleToOther;
				m_PortalLocal.m_CachedPaintPowerChoiceResults[i].surfaceNormal = choiceInfo[i].pPaintPower->m_SurfaceNormal;
			}
		}
	}
}


bool CPortal_Player::LateSuperJumpIsValid() const
{
	return jump_helper_enabled.GetBool() &&
		IsBouncePower( m_CachedJumpPower ) &&
		gpGlobals->curtime - m_flCachedJumpPowerTime < jump_helper_late_jump_max_time.GetFloat();
}


StickCameraState CPortal_Player::GetStickCameraState() const
{
	return m_PortalLocal.m_nStickCameraState;
}


Vector CPortal_Player::Weapon_ShootPosition()
{
	return EyePosition();
}


const Vector& CPortal_Player::GetInputVector() const
{
	return m_vInputVector;
}


void CPortal_Player::SetInputVector( const Vector& vInput )
{
	m_vInputVector = vInput;
}


PaintPowerType CPortal_Player::GetPaintPowerAtPoint( const Vector& worldContactPt ) const
{
	return m_PortalLocal.m_PaintedPowerType;
}


void CPortal_Player::Paint( PaintPowerType type, const Vector& worldContactPt )
{
	if( player_can_use_painted_power.GetBool() || player_paint_effects_enabled.GetBool() )
	{
#ifdef CLIENT_DLL
		// Invalidate if the timer ran out or this is a different paint type
		if( m_PortalLocal.m_PaintedPowerTimer.IsElapsed() || type != m_PortalLocal.m_PaintedPowerType )
		{
			InvalidatePaintEffects();
		}
		// If this is the same paint type, the timer hasn't elapsed, and the restart cooldown timer has, restart
		else if( m_PaintScreenEffectCooldownTimer.IsElapsed() )
		{
			// Restart paint screen space effect
			if( m_PaintScreenSpaceEffect.IsValid() )
			{
				m_PaintScreenSpaceEffect->Restart( RESTART_RESET_AND_MAKE_SURE_EMITS_HAPPEN );
			}

			// commenting out the 3rd person drop effect
			/*
			// Restart paint drip effect
			if( m_PaintDripEffect.IsValid() )
			{
				m_PaintDripEffect->Restart( RESTART_RESET_AND_MAKE_SURE_EMITS_HAPPEN );
			}
			*/

			m_PaintScreenEffectCooldownTimer.Start( player_paint_screen_effect_restart_delay.GetFloat() );
		}
#endif

		if( player_can_use_painted_power.GetBool() )
			BaseClass::Paint( type, worldContactPt );
		m_PortalLocal.m_PaintedPowerType = type;
		m_PortalLocal.m_PaintedPowerTimer.Start( player_paint_effects_duration.GetFloat() );
	}
}


void CPortal_Player::CleansePaint()
{
	BaseClass::CleansePaint();
	m_PortalLocal.m_PaintedPowerType = NO_POWER;
	m_PortalLocal.m_PaintedPowerTimer.Invalidate();

#ifdef CLIENT_DLL
	m_PaintScreenEffectCooldownTimer.Invalidate();
#endif
}


// Paint power debug
void CPortal_Player::DrawJumpHelperDebug( PaintPowerConstIter begin, PaintPowerConstIter end, float duration, bool noDepthTest, const PaintPowerInfo_t* pSelected ) const
{
	const Color bounceColor(255, 255, 0);
	const Color otherColor( 255, 255, 255 );
	const Color selectedColor( 255, 0, 255 );
	for( PaintPowerConstIter i = begin; i != end; ++i )
	{
		const PaintPowerInfo_t& powerInfo = *i;
		DrawPaintPowerContactInfo( powerInfo, (i == pSelected) ? selectedColor : ( IsBouncePower( powerInfo ) ? bounceColor : otherColor ), duration, noDepthTest );
	}
}


inline bool IsBetterPaintPowerVelocityInputScore( float flCurrentBestInputCos,
												 float flCurrentBestVelocityCos,
												 float flQueryInputCos,
												 float flQueryVelocityCos )
{
	return (flCurrentBestInputCos - flQueryInputCos > EQUAL_EPSILON && fabs(flQueryInputCos) > DEAD_INPUT_COS) ||	// Pushing most towards this one and pushing toward it significantly
		(flCurrentBestInputCos >= 0 && flCurrentBestVelocityCos - flQueryInputCos > EQUAL_EPSILON );				// Not pushing toward anything yet, but moving most toward this one
}


void CPortal_Player::ChooseBestPaintPowersInRange( PaintPowerChoiceResultArray& bestPowers,
												 PaintPowerConstIter begin,
												 PaintPowerConstIter end,
												 const PaintPowerChoiceCriteria_t& choiceCriteria ) const
{
	// Note: Valid input and velocity cosines should all be in the range [-1, 1), but we don't really want anything close to 1.
	// Theoretically, 1.0f would be an appropriate default value. However, in the case where the player jumps from a standstill
	// (no WASD input), she will not get a power on the bottom of a projected wall because we don't know about the contact until
	// after she starts falling, making the velocity cosine precisely 1.0f. Cases like this are the only reason I'm making the
	// default value 2.0f here. Feel free to change it if this is no longer the case. The input cosine default remains unchanged
	// because it should always reflect the actual input intentions. If its value is undesirable, velocity takes over.
	// -Ted
	bestPowers.EnsureCount( PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER );
	PaintPowerChoiceResult_t bestIgnoredPowers[PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER];
	for( unsigned i = 0; i < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER; ++i )
	{
		bestPowers[i].Initialize();
		bestIgnoredPowers[i].Initialize();
	}

	const Vector& vStickNormal = m_PortalLocal.m_StickNormal;

	// Iterate through all the surfaces to see if there's a more appropriate power to use
	for( PaintPowerConstIter iter = begin; iter != end; ++iter )
	{
		const PaintPowerInfo_t& powerInfo = *iter;
		const unsigned powerIndex = powerInfo.m_PaintPowerType;

		AssertMsg( powerIndex < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER, "Invalid paint power may cause out-of-bounds array access!" );

		// How much the player is pushing toward this surface and how different the normal is, respectively
		const float flInputCos = DotProduct( choiceCriteria.vNormInputDir, powerInfo.m_SurfaceNormal );
		const float flVelocityCos = DotProduct( choiceCriteria.vNormVelocity, powerInfo.m_SurfaceNormal );

		if( show_player_paint_power_debug.GetBool() )
		{
			DevMsg( "Paint Power #%d\n", iter - begin );
			PrintPowerInfoDebugMsg( powerInfo );
			DevMsg( "\n" );
			DevMsg( "Input Cosine: %f\n", flInputCos );
			DevMsg( "Velocity Cosine: %f\n", flVelocityCos );
			DevMsg("------------------------------\n");
		}

		// If this is the best choice so far
		if( powerIndex < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER &&
			IsBetterPaintPowerVelocityInputScore( bestPowers[powerIndex].flInputCos,
												  bestPowers[powerIndex].flVelocityCos,
												  flInputCos,
												  flVelocityCos ) )
		{
			bool ignorePower = false;

			// Special handling if you're in a portal environment
			if( choiceCriteria.bInPortal )
			{
				const Vector& portalNormal = m_hPortalEnvironment.Get()->m_plane_Origin.normal;
				const Vector& powerNormal = powerInfo.m_SurfaceNormal;

				const float normCosAngle = DotProduct( portalNormal, powerNormal );
				const float inputCosAngle = DotProduct( portalNormal, choiceCriteria.vNormInputDir);

				const bool nearPortal = UTIL_PointIsNearPortal( powerInfo.m_ContactPoint, m_hPortalEnvironment, PORTAL_PLANE_IGNORE_EPSILON, 0.5f * GetHullWidth() - 5.0f );
				const bool aboutParallel = CloseEnough( normCosAngle, 1.0f );
				const bool pushingTowardPortal = inputCosAngle + RELATIVE_ERROR < 0;

				// If it's no power on a portal surface, it doesn't matter
				ignorePower |= nearPortal && IsNoPower( powerInfo ) && aboutParallel;

				// If it's bounce or speed, the contact point is near the portal, and the normals are parallel or player is moving toward the portal.
				ignorePower |= nearPortal &&
							   ( IsBouncePower( powerInfo ) || IsSpeedPower( powerInfo ) ) &&
							   ( aboutParallel || pushingTowardPortal );
			}

			// If the the player already has a power which is neither stick nor is on the ground that she's moving (but not pushing) toward and this
			// is no power, just keep the power she has already.
			//ignorePower |= IsNoPower( powerInfo ) && pBestPower != 0 &&
			//			   (!IsStickPower( *pBestPower ) || pBestPower->m_SurfaceNormal.z >= STEEP_SLOPE) &&
			//			   fBestInputCos >= 0.0f && fBestVelocityCos < 0.0f;

			// Don't let speed paint on surfaces the player isn't standing on affect her, unless she's trying to leave stick paint and go
			// to a speed surface.
			const bool walkingOffStick = vStickNormal.z < STEEP_SLOPE && powerInfo.m_SurfaceNormal.z > STEEP_SLOPE;
			ignorePower |= !walkingOffStick && IsSpeedPower( powerInfo ) && DotProduct( vStickNormal, powerInfo.m_SurfaceNormal ) < STEEP_SLOPE;

			// For trampoline bounce, ignore surfaces the player isn't trying to use, so if the player pushes into a wall
			// she can't use, it doesn't take priority over the ground she's falling onto. This doesn't change anything
			// in any other bounce mode, since the power will never activate if the player isn't trying to use it.
			ignorePower |= !walkingOffStick && IsBouncePower( powerInfo ) && !IsTryingToSuperJump( &powerInfo );

			// Update the current best power
			PaintPowerChoiceResult_t& result = ignorePower ? bestIgnoredPowers[powerIndex] : bestPowers[powerIndex];

			// Make sure that this is actually better than the power to overwrite
			// Note: This is to ensure the ignored powers are consistent but have no effect on the actual best powers
			if( !ignorePower || IsBetterPaintPowerVelocityInputScore( result.flInputCos, result.flVelocityCos, flInputCos, flVelocityCos ) )
			{
				result.pPaintPower = iter;
				result.flInputCos = flInputCos;
				result.flVelocityCos = flVelocityCos;
				result.bWasIgnored = ignorePower;
			}
		}
	}

	// If any of the best powers were never set, replace it with the best ignored power of the same type
	for( unsigned i = 0; i < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER; ++i )
	{
		if( bestPowers[i].pPaintPower == NULL )
			bestPowers[i] = bestIgnoredPowers[i];
	}
}

//-----------------------------------------------------------------------------
// Paint Power User Implementation
//-----------------------------------------------------------------------------
void CPortal_Player::ChooseActivePaintPowers( PaintPowerInfoVector& activePowers )
{
	// Get all world contacts
	DeterminePaintContacts();

	// Cached stick power and new power
	PaintPowerChoiceResultArray bestTouchedPowers;
	PaintPowerConstRange touchedPowerRange = GetSurfacePaintPowerInfo();

	// Criteria to choose the best power
	PaintPowerChoiceCriteria_t choiceCriteria;
	choiceCriteria.bInPortal = m_hPortalEnvironment != 0;

	// If the player is touching anything
	if( HasAnySurfacePaintPowerInfo() || LateSuperJumpIsValid() )
	{
		// Figure out colors/powers
		MapSurfacesToPowers();

		// Sort the surfaces by priority
		PrioritySortSurfacePaintPowerInfo( &DescendingPaintPriorityCompare );

		// Compute the absolute velocity with the change due to gravity
		const Vector& vStickNormal = m_PortalLocal.m_StickNormal;
		const float gravityScale = (GetGravity() != 0) ? GetGravity() : 1.0f;
		const float gravityMagnitude = -(gravityScale * sv_gravity.GetFloat() * gpGlobals->frametime);
		const Vector dvStickGravity = gravityMagnitude * vStickNormal;
		choiceCriteria.vNormVelocity = (GetAbsVelocity() + dvStickGravity).Normalized();

		// The normalized input direction
		choiceCriteria.vNormInputDir = GetInputVector().Normalized();

		const char *dllString;
#ifdef CLIENT_DLL
		dllString = "Client";
#else
		dllString = "Server";
#endif

		if( show_player_paint_power_debug.GetBool() )
		{
			DevMsg( "DLL: %s\n", dllString );
			DevMsg( "Norm Input: (%f, %f, %f)\n", XYZ( choiceCriteria.vNormInputDir ) );
			DevMsg( "Abs Velocity: (%f, %f, %f)\n", XYZ( GetAbsVelocity() ) );
			//DevMsg( "Stick Gravity: (%f, %f, %f)\n", XYZ( dvStickGravity ) );
			DevMsg( "Norm Velocity: (%f, %f, %f)\n", XYZ( choiceCriteria.vNormVelocity ) );
			DevMsg("------------------------------\n");
		}

		// Get all the paint power info of the surfaces the player is touching
		ChooseBestPaintPowersInRange( bestTouchedPowers, touchedPowerRange.first, touchedPowerRange.second, choiceCriteria );
		if( show_player_paint_power_debug.GetBool() )
		{
			DevMsg( "Context: Touched\n" );
			DevMsg( "Paint Power Count: %d\n", touchedPowerRange.second - touchedPowerRange.first );
			DevMsg("------------------------------\n");
		}

		// Trying to jump and jump helper is enabled
		const bool bJumpHelperEnabled = jump_helper_enabled.GetBool();
		const bool bIsPressingJumpKey = IsPressingJumpKey();
		const PaintPowerInfo_t* pBestLookAheadSuperJumpPower = 0;
		PaintPowerConstRange jumpHelperLookAheadPowerRange = GetSurfacePaintPowerInfo( JUMP_HELPER_CONTEXT );
		if( bJumpHelperEnabled )
		{
			// Check the look ahead list for the super jump power
			PaintPowerChoiceResultArray bestJumpHelperLookAheadPowers;
			ChooseBestPaintPowersInRange( bestJumpHelperLookAheadPowers, jumpHelperLookAheadPowerRange.first, jumpHelperLookAheadPowerRange.second, choiceCriteria );
			const PaintPowerInfo_t* pBestTouchedSuperJumpPower = bestTouchedPowers[BOUNCE_POWER].pPaintPower;
			pBestLookAheadSuperJumpPower = bestJumpHelperLookAheadPowers[BOUNCE_POWER].pPaintPower;

			if( bIsPressingJumpKey )
			{
				if( show_player_paint_power_debug.GetBool() )
				{
					DevMsg( "Context: %s\n", JUMP_HELPER_CONTEXT );
					DevMsg( "Paint Power Count: %d\n", touchedPowerRange.second - touchedPowerRange.first );
					DevMsg("------------------------------\n");
				}

				// Didn't get a touched super jump
				if( !pBestTouchedSuperJumpPower )
				{
					// Got a predicted super jump
					if( pBestLookAheadSuperJumpPower && IsBouncePower( *pBestLookAheadSuperJumpPower ) )
					{
						if( GetGroundEntity() )
						{
							// Early jump from the ground
							bestTouchedPowers[BOUNCE_POWER].pPaintPower = pBestLookAheadSuperJumpPower;
							bestTouchedPowers[BOUNCE_POWER].bWasIgnored = false;
						}
						else
						{
							// Early jump from the air
							m_flPredictedJumpTime = gpGlobals->curtime;
						}
					}
					// Got a late super jump
					else if( LateSuperJumpIsValid() )
					{
						// Use the late super jump
						bestTouchedPowers[BOUNCE_POWER].pPaintPower = &m_CachedJumpPower;
						bestTouchedPowers[BOUNCE_POWER].bWasIgnored = false;
						m_flCachedJumpPowerTime = -FLT_MAX;
					}
				}
			}
		}

		// Jump helper debug draw
		if( jump_helper_debug_enabled.GetBool() )
		{
			const float duration = bIsPressingJumpKey ? 5.0f : 0.0f;
			DrawJumpHelperDebug( jumpHelperLookAheadPowerRange.first, jumpHelperLookAheadPowerRange.second, duration, false, bestTouchedPowers[BOUNCE_POWER].pPaintPower );

			// Late hit was available
			if( LateSuperJumpIsValid() )
			{
				if( bestTouchedPowers[BOUNCE_POWER].pPaintPower == &m_CachedJumpPower )
					DrawPaintPowerContactInfo( m_CachedJumpPower, Color( 255, 0, 255 ), duration, false );
				else if( IsBouncePower( m_CachedJumpPower ) )
					DrawPaintPowerContactInfo( m_CachedJumpPower, Color( 255, 0, 0 ), duration, false );
			}

			// If we got a new bounce power, draw it in case it was a surface we actually touched.
			if( bestTouchedPowers[BOUNCE_POWER].pPaintPower )
				DrawPaintPowerContactInfo( *bestTouchedPowers[BOUNCE_POWER].pPaintPower, Color( 255, 0, 255 ), duration, false );

			if( bestTouchedPowers[BOUNCE_POWER].pPaintPower )
			{
				IHandleEntity* pHandleEnt = bestTouchedPowers[BOUNCE_POWER].pPaintPower->m_HandleToOther.Get();
				CBaseEntity* pBaseEnt = pHandleEnt ? EntityFromEntityHandle( pHandleEnt ) : 0;
				char const* entName;
				entName = pBaseEnt ? pBaseEnt->GetClassname() : "";

				if( bestTouchedPowers[BOUNCE_POWER].pPaintPower == pBestLookAheadSuperJumpPower )
					DevMsg( "Early jump off of %s!\n", entName );
				else if( bestTouchedPowers[BOUNCE_POWER].pPaintPower == &m_CachedJumpPower )
					DevMsg( "Late jump off of %s!\n", entName );
				else if( IsTryingToSuperJump( bestTouchedPowers[BOUNCE_POWER].pPaintPower ) )
					DevMsg( "Punctual jump off of %s!\n", entName );
			}
		}
	}

	// Play transition sounds of walking onto/off of paint
	PlayPaintSounds( bestTouchedPowers );

	// Set the new powers if we found one
	for( int i = 0; i < bestTouchedPowers.Count(); ++i )
	{
		const PaintPowerInfo_t* pNewPower = bestTouchedPowers[i].pPaintPower;
		if( pNewPower )
		{
			//#ifndef CLIENT_DLL
			//// Only send the event if the powers are actually different
			//if( AreDifferentPowers( *pNewPower, GetPaintPower( pNewPower->m_PaintPowerType ) ) )
			//{
			//	IGameEvent *event = gameeventmanager->CreateEvent( "touched_paint" );
			//	if ( event )
			//	{
			//		event->SetInt("userid", GetUserID() );
			//		event->SetInt( "painttype", pNewPower->m_PaintPowerType );

			//		PaintSurfaceType surfaceType;
			//		if( pNewPower->m_SurfaceNormal.z > 0.5f )
			//		{
			//			surfaceType = FLOOR_SURFACE;
			//		}
			//		else if( pNewPower->m_SurfaceNormal.z < -0.5f )
			//		{
			//			surfaceType = CEILING_SURFACE;
			//		}
			//		else
			//		{
			//			surfaceType = WALL_SURFACE;
			//		}
			//		event->SetInt( "surfacedir", surfaceType );

			//		gameeventmanager->FireEvent( event );
			//	}
			//}
			//#endif //!CLIENT_DLL

			if( !bestTouchedPowers[i].bWasIgnored )
				activePowers.AddToTail( *pNewPower );
		}
	}

	// Cache off the choice results
	CachePaintPowerChoiceResults( bestTouchedPowers );

	// If the player is paintable and has a painted power, try to use it
	if( player_can_use_painted_power.GetBool() && m_PortalLocal.m_PaintedPowerType != NO_POWER )
	{
		// Copy all the surface data, and replace the powers with the painted power
		PaintPowerInfoVector paintedPowerQuery;
		paintedPowerQuery.EnsureCapacity( GetCountFromRange( touchedPowerRange ) );
		for( PaintPowerConstIter i = touchedPowerRange.first; i != touchedPowerRange.second; ++i )
		{
			if( i->m_PaintPowerType != INVALID_PAINT_POWER )
			{
				PaintPowerInfo_t paintedPower = *i;
				paintedPower.m_PaintPowerType = m_PortalLocal.m_PaintedPowerType;
				paintedPowerQuery.AddToTail( paintedPower );
			}
		}

		// Choose the best surface to use the painted power on
		PaintPowerChoiceResultArray bestPaintedPowers;
		PaintPowerChoiceCriteria_t paintedChoiceCriteria = choiceCriteria;
		ChooseBestPaintPowersInRange( bestPaintedPowers,
			GetConstBegin( paintedPowerQuery ),
			GetConstEnd( paintedPowerQuery ),
			paintedChoiceCriteria );

		// If they weren't all ignored, give the player her painted power
		if( bestPaintedPowers[m_PortalLocal.m_PaintedPowerType].pPaintPower &&
			!bestPaintedPowers[m_PortalLocal.m_PaintedPowerType].bWasIgnored )
		{
			activePowers.AddToTail( *bestPaintedPowers[m_PortalLocal.m_PaintedPowerType].pPaintPower );
		}
	}

	if( show_player_paint_power_debug.GetBool() )
	{
		DevMsg( "Active Powers:\n" );
		for( int i = 0; i < activePowers.Count(); ++i )
		{
			DevMsg( "%s Power\n", PowerTypeToString( activePowers[i] ) );
		}
		DevMsg("==============================\n");
	}
}


ConVar sv_paint_trigger_sound_delay( "sv_paint_trigger_sound_delay", "0.1f", FCVAR_REPLICATED );
void CPortal_Player::PlayPaintSounds( const PaintPowerChoiceResultArray& touchedPowers )
{
	const char* const soundList[] =
	{
		"Player.EnterBouncePaint",
		"Player.ExitBouncePaint",
		"Player.EnterStickPaint",
		"Player.ExitStickPaint",
		"Player.EnterSpeedPaint",
		"Player.ExitSpeedPaint"
	};

	CRecipientFilter filter;
	filter.AddRecipient( this );

	bool isTouchingPower[PAINT_POWER_TYPE_COUNT - 1] = { false };
	isTouchingPower[BOUNCE_POWER] = BOUNCE_POWER < touchedPowers.Count() &&
									touchedPowers[BOUNCE_POWER].pPaintPower != NULL;
	isTouchingPower[SPEED_POWER] = SPEED_POWER < touchedPowers.Count() &&
								   touchedPowers[SPEED_POWER].pPaintPower != NULL &&
								   !touchedPowers[SPEED_POWER].bWasIgnored;

	// Go through all usable powers and check if we just stepped onto or off of it
	for( int i = 0; i < PAINT_POWER_TYPE_COUNT - 1; ++i )
	{
		// If we're on this power
		if( isTouchingPower[i] )
		{
			// Just stepping onto this power
			if( m_flTimeSinceLastTouchedPower[i] > sv_paint_trigger_sound_delay.GetFloat() )
			{
				EmitSound( filter, this->entindex(), soundList[i*2] );
			}

			// Reset timer
			m_flTimeSinceLastTouchedPower[i] = 0.f;
		}
		// Not on this power
		else
		{
			// Stepping off of this power.  Use a delay so we don't deactivate when we very briefly lose a power
			if( m_flTimeSinceLastTouchedPower[i] < sv_paint_trigger_sound_delay.GetFloat() && 
				m_flTimeSinceLastTouchedPower[i] + gpGlobals->frametime >= sv_paint_trigger_sound_delay.GetFloat() )
			{
				EmitSound( filter, this->entindex(), soundList[i*2+1] );
			}

			// Increase timer
			m_flTimeSinceLastTouchedPower[i] += gpGlobals->frametime;
		}
	}
}

bool CPortal_Player::IsUsingPostTeleportationBox() const
{
	return m_flUsePostTeleportationBoxTime > 0.0f;
}


const Vector& CPortal_Player::GetHullMins() const
{
	return IsObserver() ? VEC_OBS_HULL_MIN : ( GetFlags() & FL_DUCKING ? GetDuckHullMins() : GetStandHullMins() );
}


const Vector& CPortal_Player::GetHullMaxs() const
{
	return IsObserver() ? VEC_OBS_HULL_MAX : ( GetFlags() & FL_DUCKING ? GetDuckHullMaxs() : GetStandHullMaxs() );
}


const Vector CPortal_Player::GetPlayerMins() const
{
	return GetHullMins();
}


const Vector CPortal_Player::GetPlayerMaxs() const
{
	return GetHullMaxs();
}

const Vector& CPortal_Player::GetStandHullMins() const
{
	return m_PortalLocal.m_StandHullMin;
}


const Vector& CPortal_Player::GetStandHullMaxs() const
{
	return m_PortalLocal.m_StandHullMax;
}


const Vector& CPortal_Player::GetDuckHullMins() const
{
	return m_PortalLocal.m_DuckHullMin;
}


const Vector& CPortal_Player::GetDuckHullMaxs() const
{
	return m_PortalLocal.m_DuckHullMax;
}


void CPortal_Player::UpdateCollisionBounds()
{
	SetCollisionBounds( GetHullMins(), GetHullMaxs() );
}


inline float ComputeHullHeight( const Vector& localUp, const Vector& hullMins, const Vector& hullMaxs )
{
	return fabs( DotProduct( localUp, hullMaxs - hullMins ) );
}


float CPortal_Player::GetHullHeight() const
{
	const Vector& localUp = m_PortalLocal.m_bAttemptHullResize ? static_cast<Vector>( m_PortalLocal.m_OldStickNormal ) : static_cast<Vector>( m_PortalLocal.m_StickNormal );
	return ComputeHullHeight( localUp, GetHullMins(), GetHullMaxs() );
}


float ComputeHullWidth( const Vector& localUp, const Vector& hullMins, const Vector& hullMaxs )
{
	Vector diffInPlane = hullMaxs - hullMins;
	diffInPlane -= DotProduct( localUp, diffInPlane ) * localUp;
	return fabs( diffInPlane[ diffInPlane.LargestComponent() ] );
}


float CPortal_Player::GetHullWidth() const
{
	const Vector& localUp = m_PortalLocal.m_bAttemptHullResize ? static_cast<Vector>( m_PortalLocal.m_OldStickNormal ) : static_cast<Vector>( m_PortalLocal.m_StickNormal );
	return ComputeHullWidth( localUp, GetHullMaxs(), GetHullMins() );
}


float CPortal_Player::GetStandHullHeight() const
{
	const Vector& localUp = m_PortalLocal.m_bAttemptHullResize ? static_cast<Vector>( m_PortalLocal.m_OldStickNormal ) : static_cast<Vector>( m_PortalLocal.m_StickNormal );
	return ComputeHullHeight( localUp, GetStandHullMins(), GetStandHullMaxs() );
}


float CPortal_Player::GetStandHullWidth() const
{
	const Vector& localUp = m_PortalLocal.m_bAttemptHullResize ? static_cast<Vector>( m_PortalLocal.m_OldStickNormal ) : static_cast<Vector>( m_PortalLocal.m_StickNormal );
	return ComputeHullWidth( localUp, GetStandHullMaxs(), GetStandHullMins() );
}


float CPortal_Player::GetDuckHullHeight() const
{
	const Vector& localUp = m_PortalLocal.m_bAttemptHullResize ? static_cast<Vector>( m_PortalLocal.m_OldStickNormal ) : static_cast<Vector>( m_PortalLocal.m_StickNormal );
	return ComputeHullHeight( localUp, GetDuckHullMins(), GetDuckHullMaxs() );
}


float CPortal_Player::GetDuckHullWidth() const
{
	const Vector& localUp = m_PortalLocal.m_bAttemptHullResize ? static_cast<Vector>( m_PortalLocal.m_OldStickNormal ) : static_cast<Vector>( m_PortalLocal.m_StickNormal );
	return ComputeHullWidth( localUp, GetDuckHullMaxs(), GetDuckHullMins() );
}


void CPortal_Player::AddSurfacePaintPowerInfo( const BrushContact& contact, char const* context )
{
	AddSurfacePaintPowerInfo( PaintPowerInfo_t( contact.normal,
												contact.point,
												contact.pBrushEntity,
												NO_POWER,
												contact.isOnThinSurface ),
												context );
}


void CPortal_Player::AddSurfacePaintPowerInfo( const trace_t& trace, char const* context )
{
	if( trace.m_pEnt )
	{
		if( trace.m_pEnt->IsBSPModel() )
		{
			const float DIM_OFFSET = sv_contact_region_thickness.GetFloat();
			const Vector AABB_OFFSET( DIM_OFFSET, DIM_OFFSET, DIM_OFFSET );

			// Compute the contacts with a slightly expanded swept AABB
			const Vector traceDelta = trace.endpos - trace.startpos;
			const Vector contactBoxMin = GetAbsOrigin() + traceDelta + WorldAlignMins() - AABB_OFFSET,
						 contactBoxMax = GetAbsOrigin() + traceDelta + WorldAlignMaxs() + AABB_OFFSET;

			const cplane_t *pClip = (m_hPortalEnvironment != 0 && sv_clip_contacts_to_portals.GetBool()) ? &m_hPortalEnvironment.Get()->m_plane_Origin : NULL;
			const int iClipCount = pClip ? 1 : 0;
			ContactVector contacts;
			ComputeAABBContactsWithBrushEntity( contacts, pClip, iClipCount, WorldSpaceCenter(), contactBoxMin, contactBoxMax, trace.m_pEnt );

			// Fill in the surface power info vector
			const int contactCount = contacts.Count();
			for( int i = 0; i < contactCount; ++i )
			{
				AddSurfacePaintPowerInfo( contacts[i], context );
			}
		}
		else if( trace.plane.normal != vec3_origin )
		{
			AddSurfacePaintPowerInfo( PaintPowerInfo_t( trace.plane.normal,
														UTIL_ProjectPointOntoPlane( trace.endpos, trace.plane ),
														trace.m_pEnt ),
														context );	
		}
	}
}


void CPortal_Player::DeterminePaintContacts()
{
	if( GetMoveType() == MOVETYPE_NOCLIP )
	{
		return;
	}

	const float DIM_OFFSET = sv_contact_region_thickness.GetFloat();
	const Vector AABB_OFFSET( DIM_OFFSET, DIM_OFFSET, DIM_OFFSET );

	// Compute the contacts with a slightly expanded swept AABB
	Vector contactBoxMin = GetAbsOrigin() + WorldAlignMins() - AABB_OFFSET,
		contactBoxMax = GetAbsOrigin() + WorldAlignMaxs() + AABB_OFFSET;
	const Vector traceBoxMin = contactBoxMin, traceBoxMax = contactBoxMax;

	// Compute the displacement and use it to sweep the box
	const Vector displacement = gpGlobals->frametime * m_PortalLocal.m_vPreUpdateVelocity;
	//ExpandAABB( contactBoxMin, contactBoxMax, displacement );

	// HACK: Sweep the box ahead an extra frame while in the air to catch the ground early for
	// trampoline/reflect bounce, so that the velocity doesn't get zeroed out before we can use it.
	float traceLookAheadFactor = 1.0f;
	if( ( sv_press_jump_to_bounce.GetInt() == TRAMPOLINE_BOUNCE || sv_bounce_reflect_enabled.GetBool() ) &&
		GetGroundEntity() == NULL )
	{
		traceLookAheadFactor = 2.0f;
		ExpandAABB( contactBoxMin, contactBoxMax, 2.0f * displacement );
	}

	//NDebugOverlay::Box( Vector(0, 0, 0), contactBoxMin, contactBoxMax, 0, 0, 255, 1, 0 );
	//NDebugOverlay::Box( GetAbsOrigin(), GetHullMins(), GetHullMaxs(), 0, 255, 0, 1, 0 );

	// Find contacts with the world
#ifdef CLIENT_DLL
	CWorld* pWorld = GetClientWorldEntity();
#else
	CWorld* pWorld = GetWorldEntity();
#endif	

	cplane_t *pClip = (m_hPortalEnvironment != 0 && sv_clip_contacts_to_portals.GetBool()) ? &m_hPortalEnvironment.Get()->m_plane_Origin : NULL;
	int iClipCount = pClip ? 1 : 0;
	ContactVector contacts;

	ComputeAABBContactsWithBrushEntity( contacts, pClip, iClipCount, WorldSpaceCenter(), contactBoxMin, contactBoxMax, pWorld );

	// Fill in the surface power info vector
	int contactCount = contacts.Count();
	for( int i = 0; i < contactCount; ++i )
	{
		AddSurfacePaintPowerInfo( contacts[i] );
	}

	// Find contacts with non-world entities
	{
		const Vector& center = WorldSpaceCenter();
		trace_t trace;
		TracePlayerBoxAgainstCollidables( trace, this, center, center + traceLookAheadFactor * displacement, traceBoxMin - center, traceBoxMax - center );

		if( trace.DidHitNonWorldEntity() )
		{
			AddSurfacePaintPowerInfo( trace );		
		}
	}

	// Look ahead for powers the player may want to use
	if( jump_helper_enabled.GetBool() || jump_helper_debug_enabled.GetBool() )
	{
		PredictPaintContacts( contactBoxMin, contactBoxMax, traceBoxMin, traceBoxMax, jump_helper_look_ahead_time.GetFloat(), JUMP_HELPER_CONTEXT );
	}
}


void CPortal_Player::PredictPaintContacts( const Vector& contactBoxMin,
										 const Vector& contactBoxMax,
										 const Vector& traceBoxMin,
										 const Vector& traceBoxMax,
										 float lookAheadTime,
										 char const* context )
{
	char const** filterBegin = g_pszPredictedPowerIgnoreFilter;
	char const** filterEnd = filterBegin + ARRAYSIZE( g_pszPredictedPowerIgnoreFilter );
	const Vector& worldSpaceCenter = WorldSpaceCenter();
	const Vector lookAheadVector = lookAheadTime * m_PortalLocal.m_vPreUpdateVelocity;
	Vector lookAheadDirection = lookAheadVector;
	const float totalDistance = lookAheadDirection.NormalizeInPlace();
	const int numSamples = static_cast<int>( fpmax( totalDistance * paint_power_look_ahead_sample_density.GetFloat(), 1.0f ) );
	const float stepSize = totalDistance / numSamples;
	ContactVector contacts;
	
#ifdef CLIENT_DLL
	CWorld* pWorld = GetClientWorldEntity();
#else
	CWorld* pWorld = GetWorldEntity();
#endif	
	cplane_t *pClip = (m_hPortalEnvironment != 0 && sv_clip_contacts_to_portals.GetBool()) ? &m_hPortalEnvironment.Get()->m_plane_Origin : NULL;
	int iClipCount = pClip ? 1 : 0;

	Vector prevCenter = worldSpaceCenter;
	float distance = stepSize;
	for( int sample = 0; sample < numSamples; distance += stepSize, ++sample )
	{
		// Find precise contacts with the world
		const Vector offset = distance * lookAheadDirection;
		const Vector center = worldSpaceCenter + offset;
		ComputeAABBContactsWithBrushEntity( contacts, pClip, iClipCount, center, contactBoxMin + offset, contactBoxMax + offset, pWorld );

		int contactCount = contacts.Count();
		for( int i = 0; i < contactCount; ++i )
		{
			AddSurfacePaintPowerInfo( contacts[i], context );
		}
		contacts.RemoveAll();

		// Trace for non-world entities
		trace_t trace;
		TracePlayerBoxAgainstCollidables( trace, this, prevCenter, center, traceBoxMin - worldSpaceCenter, traceBoxMax - worldSpaceCenter );

		if( trace.DidHitNonWorldEntity() &&
			std::find_if( filterBegin, filterEnd, StringCompare_t( trace.m_pEnt->GetClassname() ) ) == filterEnd )
		{
			//DevMsg( "Collided with %s\n", trace.m_pEnt->GetClassname() );
			AddSurfacePaintPowerInfo( trace, context );
		}

		prevCenter = center;
	}
}


PaintPowerState CPortal_Player::ActivateSpeedPower( PaintPowerInfo_t& speedInfo )
{
	// Speed is initially the walking speed or the current tangential speed, whichever is greater
	const Vector& localVelocity = GetLocalVelocity();
	const Vector tangentialVelocity = localVelocity - DotProduct( localVelocity, speedInfo.m_SurfaceNormal ) * speedInfo.m_SurfaceNormal;
	SetMaxSpeed( clamp( tangentialVelocity.Length(), sv_speed_normal.GetFloat(), sv_speed_paint_max.GetFloat() ) );

	m_flSpeedDecelerationTime = 0.0f;

	return ACTIVE_PAINT_POWER;
}

PaintPowerState CPortal_Player::UseSpeedPower( PaintPowerInfo_t& speedInfo )
{
	// Compute normalized forward direction in tangent plane
	const Vector& vInputVec = GetInputVector();
	const Vector vWishDirection = vInputVec.Normalized();
	const Vector vTangentRight = CrossProduct( vWishDirection, speedInfo.m_SurfaceNormal );
	const Vector vNormTangentForward = CrossProduct( speedInfo.m_SurfaceNormal, vTangentRight ).Normalized();
	const bool bStrafing = fabs( DotProduct( vWishDirection, Left() ) ) > 0.9f;

	// Figure out if the player is moving up a ramp
	const float flWorldUpAngle = vNormTangentForward.z;
	const bool bMovingUpRamp = flWorldUpAngle > 0 && flWorldUpAngle < 1.0f - RELATIVE_ERROR;
	const bool bInAir = GetGroundEntity() == NULL;

	// Compute the change in speed this frame
	float flAcceleration = bInAir ? 0.0f : ( bMovingUpRamp ? sv_speed_paint_ramp_acceleration.GetFloat() : sv_speed_paint_acceleration.GetFloat() );
	flAcceleration *= bStrafing ? sv_speed_paint_straf_accel_scale.GetFloat() : 1.f;
	float fldv = gpGlobals->frametime * flAcceleration;

	// If the player is drastically changing direction, knock the speed back down.
	// If the player is in the air his speed doesn't change
	// Otherwise, scale the change in speed by how consistent it is with the current direction
	Vector vLocalDirection = GetLocalVelocity();
	const float flWishDirSpeed = DotProduct( vLocalDirection, vWishDirection );
	const float flSpeed = vLocalDirection.NormalizeInPlace();
	const float flWishCos = DotProduct( vWishDirection, vLocalDirection );
	const float flSpeedFactor = vInputVec.IsZero() ? -1.0f : flWishCos;

	fldv = (flWishCos < 0.0f) ? ( flWishCos * fabs( flSpeed - sv_speed_normal.GetFloat() ) ) : (flSpeedFactor * fldv);

	float flDefaultNewMaxSpeed = clamp( MaxSpeed() + fldv,
		sv_speed_normal.GetFloat(),
		sv_speed_paint_max.GetFloat() );

	// Compute the new max speed
	float flNewMaxSpeed = SpeedPaintAcceleration( flDefaultNewMaxSpeed,
		flSpeed,
		flWishCos,
		flWishDirSpeed );

	// Use the new max speed or the lower bound if the player has stopped moving completely
	SetMaxSpeed( GetLocalVelocity().IsZero() ? sv_speed_normal.GetFloat() : flNewMaxSpeed );

	return ACTIVE_PAINT_POWER;
}


PaintPowerState CPortal_Player::DeactivateSpeedPower( PaintPowerInfo_t& speedInfo )
{
	const float walkSpeed = sv_speed_normal.GetFloat();
	const bool onGround = GetGroundEntity() != NULL;
	float newMaxSpeed = 0.f;

	// Deactivate in the air (and touching something besides speed paint)
	if( !onGround && !IsEmptyRange( GetSurfacePaintPowerInfo() ) )
	{
		const Vector& velocity = GetAbsVelocity();
		const Vector tangentialVelocity = velocity - DotProduct( velocity, speedInfo.m_SurfaceNormal ) * speedInfo.m_SurfaceNormal;
		newMaxSpeed = tangentialVelocity.Length();
	}
	// On the ground and not on speed paint or in the air and not touching anything
	else
	{
		if( IsActivePower( GetPaintPower( BOUNCE_POWER ) ) && m_flSpeedDecelerationTime < sv_speed_paint_on_bounce_deceleration_delay.GetFloat() )
		{
			m_flSpeedDecelerationTime += gpGlobals->frametime;
			newMaxSpeed = MaxSpeed();
		}
		else
		{
			// Compute new max speed
			float flDefaultNewMaxSpeed = MaxSpeed() - gpGlobals->frametime * sv_speed_paint_acceleration.GetFloat();
			Vector vLocalDirection = GetLocalVelocity();
			const Vector vWishDirection = GetInputVector().Normalized();
			const float flWishDirSpeed = DotProduct( vLocalDirection, vWishDirection );
			const float flSpeed = vLocalDirection.NormalizeInPlace();
			const float flWishCos = DotProduct( vWishDirection, vLocalDirection );

			newMaxSpeed = SpeedPaintAcceleration( flDefaultNewMaxSpeed,
				flSpeed,
				flWishCos,
				flWishDirSpeed );
		}
	}


	// Set the max speed. Done deactivating if it's normal walking speed
	newMaxSpeed = clamp( newMaxSpeed, walkSpeed, sv_speed_paint_max.GetFloat() );
	SetMaxSpeed( newMaxSpeed );
	return newMaxSpeed == walkSpeed && onGround ? INACTIVE_PAINT_POWER : DEACTIVATING_PAINT_POWER;
}

float CPortal_Player::SpeedPaintAcceleration( float flDefaultMaxSpeed,
											  float flSpeed,
											  float flWishCos,
											  float flWishDirSpeed ) const
{
	// Compute the new max speed and clamp it
	float flNewMaxSpeed = 0.f;

	// If they're not pushing any movement buttons, then cap their maxspeed at their current speed
	if( GetInputVector().IsZero() )
	{
		flNewMaxSpeed = clamp( flNewMaxSpeed, sv_speed_normal.GetFloat(), flSpeed );
	}
	// If they're trying to move mostly orthogonal to their velocity then penalize their maxspeed
	else if( flWishCos > 0.f && flWishCos < 0.3f )
	{
		// Cap their speed to a point between the speed they're moving in the direction they're pressing and
		// their total speed.
		flNewMaxSpeed = MAX( sv_speed_normal.GetFloat(), flWishDirSpeed + 0.3f * (flSpeed - flWishDirSpeed) );
	}
	else
	{
		flNewMaxSpeed = flDefaultMaxSpeed;
	}

	return flNewMaxSpeed;
}


bool CPortal_Player::IsPressingJumpKey() const
{
	return ( m_afButtonForced & IN_JUMP ) ? m_bJumpWasPressedWhenForced : ( m_afButtonPressed & IN_JUMP ) != 0;
}


bool CPortal_Player::IsHoldingJumpKey() const
{
	return ( m_afButtonForced & IN_JUMP ) ? m_bJumpWasPressedWhenForced : ( m_nButtons & IN_JUMP ) != 0;
}


CEG_NOINLINE bool CPortal_Player::IsTryingToSuperJump( const PaintPowerInfo_t* pInfo ) const
{
	if( !pInfo )
		return false;

	const int superJumpMode = sv_press_jump_to_bounce.GetInt();
	CEG_GCV_PRE();
	static const int CEG_SPEED_POWER = CEG_GET_CONSTANT_VALUE( PaintSpeedPower );
	CEG_GCV_POST();

	// For the trampoline bounce mode, the player can bounce if the velocity is significantly toward the surface.
	// Note that this condition requires the player be in the air. That is because we must always detect this case
	// for ground bounces before she gets a ground entity and her z velocity is zeroed out. Requiring this
	// necessary condition prevents misfires while walking on steep slopes, which occur because step code handles
	// movement along z, so the velocity is always in the xy-plane, making the velocity appear to point more toward
	// the surface than it actually does.
	// Long jumps are automatic at sufficient speed, optionally if the player is looking roughly in the direction of movement.
	// Optionally, the player can also super jump if the jump key is pressed or won't jump off a wall while simply walking into it.
	const Vector& velocity = m_PortalLocal.m_vPreUpdateVelocity;
	const float normalVelocity = DotProduct( pInfo->m_SurfaceNormal, velocity );
	const bool isWallJumpFromGround = AlmostEqual( DotProduct( pInfo->m_SurfaceNormal, m_PortalLocal.m_StickNormal ), 0.0f );
	const bool isInAir = GetGroundEntity() == NULL;
	const bool canTrampolineBounceOffWall = trampoline_bounce_off_walls_while_on_ground.GetBool() || isInAir;
	const bool spaceBarActivatedTrampolineJump = IsPressingJumpKey() && jump_button_can_activate_trampoline_bounce.GetBool();
	const bool canAutoLongJump = MaxSpeed() > bounce_auto_trigger_min_speed.GetFloat() &&
								 !IsActivatingPower( GetPaintPower( CEG_SPEED_POWER ) ) &&
								 !isInAir &&
								 ( !look_dependent_auto_long_jump_enabled.GetBool() || DotProduct( Forward(), velocity.Normalized() ) >= look_dependent_auto_long_jump_min_cos_angle.GetFloat() );
	const bool canTrampolineBounce = ( superJumpMode == TRAMPOLINE_BOUNCE ) &&
									 ( ( normalVelocity < -trampoline_bounce_min_impact_speed.GetFloat() && ( isInAir || pInfo->m_SurfaceNormal.z <= 0.0f ) ) ||
									   ( spaceBarActivatedTrampolineJump ) ||
									   ( canAutoLongJump ) ) &&
									 ( !isWallJumpFromGround || canTrampolineBounceOffWall || spaceBarActivatedTrampolineJump );

	// The conditions for the jump mode are satisfied
	const bool jumpConditionsSatisfied = ( superJumpMode == JUMP_ON_TOUCH ) ||
										 ( superJumpMode == PRESS_JUMP_TO_BOUNCE && IsPressingJumpKey() ) ||
										 ( superJumpMode == HOLD_JUMP_TO_BOUNCE && IsHoldingJumpKey() ) ||
										 ( canTrampolineBounce );

	// Should perform a late super jump
	const bool jumpHelperShouldLateJump = jumpConditionsSatisfied && LateSuperJumpIsValid();

	// If they're legitimately jumping, or performing a late jump but not crouching.
	return (jumpConditionsSatisfied || jumpHelperShouldLateJump);	   
}


void CPortal_Player::SetJumpedThisFrame( bool jumped )
{
	if( jumped != m_PortalLocal.m_bJumpedThisFrame )
	{
		EASY_DIFFPRINT( this, "CPortal_Player::SetJumpedThisFrame() %s", jumped ? "true" : "false" );
	}
	m_PortalLocal.m_bJumpedThisFrame = jumped;
}


bool CPortal_Player::JumpedThisFrame() const
{
	return m_PortalLocal.m_bJumpedThisFrame;
}


void CPortal_Player::SetBouncedThisFrame( bool bounced )
{
	m_PortalLocal.m_bBouncedThisFrame = bounced;
}


bool CPortal_Player::BouncedThisFrame() const
{
	return m_PortalLocal.m_bBouncedThisFrame;
}


InAirState CPortal_Player::GetInAirState() const
{
	return m_PortalLocal.m_InAirState;
}

// Extracted and noinlined to give us a standalone callsite to protect
CEG_NOINLINE void UpdatePaintZ( Vector &velocity, float inGravity )
{
	// Find the height the player will go at the current velocity
	// 0 = v0 + g * t
	// t = -v0 / g					- time for gravity to zero velocity, assuming g < 0
	// h = v * t + 0.5 * g * t^2	- height reached
	const float gravityScale = inGravity != 0 ? inGravity : 1.0f;
	const float gravity = -gravityScale * sv_gravity.GetFloat();
	const float time = -velocity.z / gravity;
	const float baseHeight = velocity.z * time + 0.5f * gravity * time * time;

	// Compute how fast to go to reach the same height plus the height boost
	// h = 0.5 * g * t^2			- distance traveled with linear accel (target height)
	// t = sqrt(2.0 * h / g)		- how long to fall h units
	// v = g * t					- velocity at the end (just invert it to jump up that high)
	// v = g * sqrt(2.0 * h / g)
	// v^2 = g * g * 2.0 * h / g
	// v = sqrt(g * 2.0 * h)
	const float targetHeight = baseHeight + bounce_ledge_fall_height_boost.GetFloat();
	velocity.z = sqrt( 2 * -gravity * targetHeight ); // Negate gravity so the value is valid
}

#ifdef CLIENT_DLL
CEG_PROTECT_FUNCTION( UpdatePaintZ );
#endif

bool CPortal_Player::CheckToUseBouncePower( PaintPowerInfo_t& info )
{	

	// Crouching opts out of using bounce powers
	const bool bIsCrouching = m_Local.m_bDucking || GetFlags() & FL_DUCKING || m_PortalLocal.m_bPreventedCrouchJumpThisFrame;
	const bool bShouldJump = IsTryingToSuperJump( &info );
#ifndef CLIENT_DLL
	// They're going to suppress an auto-bounce
	if( bIsCrouching && bShouldJump && gpGlobals->curtime - m_flLastSuppressedBounceTime > 1.f )
	{
		m_flLastSuppressedBounceTime = gpGlobals->curtime;

		IGameEvent *event = gameeventmanager->CreateEvent("player_suppressed_bounce");
		if( event )
		{
			event->SetInt("userid", GetUserID() );
			gameeventmanager->FireEvent( event );
		}
	}
#endif


	// Trying to use super jump 
	if( bShouldJump && !bIsCrouching )
	{
#ifndef CLIENT_DLL
		//IGameEvent *event = gameeventmanager->CreateEvent( "player_paint_jumped" );
		//if ( event )
		//{
		//	event->SetInt("userid", GetUserID() );
		//	gameeventmanager->FireEvent( event );
		//}

		if( info.m_SurfaceNormal.z > 0.9f )
		{
			// Reset the bounce count if this is a different surface than last time
			const float planeDistance = info.m_ContactPoint.z;
			if( fabs( planeDistance - m_LastGroundBouncePlaneDistance ) > sv_stepsize.GetFloat() )
				m_nBounceCount = 0;

			m_LastGroundBouncePlaneDistance = planeDistance;

			++m_nBounceCount;

			IGameEvent *event = gameeventmanager->CreateEvent("bounce_count");
			if( event )
			{
				event->SetInt("userid", GetUserID() );
				event->SetInt("bouncecount", m_nBounceCount );
				gameeventmanager->FireEvent( event );
			}
		}

		EmitSound( "Player.JumpPowerUse" );
#endif //!CLIENT_DLL

		m_PortalLocal.m_flAirInputScale = BOUNCE_PAINT_INPUT_DAMPING; 

		// In the air now.
		SetBouncedThisFrame( true );
		SetGroundEntity( NULL );

		// Compute velocity in the normal direction
		Vector velocity = m_PortalLocal.m_vPreUpdateVelocity;
		const float normalSpeed = DotProduct( velocity, info.m_SurfaceNormal );

		if ( sv_debug_bounce_reflection.GetBool() )
		{
			NDebugOverlay::VertArrow( info.m_ContactPoint - 50.f * velocity.Normalized(), info.m_ContactPoint, 2, 255, 0, 0, 64, true, sv_debug_bounce_reflection_time.GetFloat() );
		}
		// If the reflected normal velocity will be larger than the minimum bounce speed
		if ( ( sv_bounce_reflect_enabled.GetBool() || sv_press_jump_to_bounce.GetInt() == TRAMPOLINE_BOUNCE ) &&
			  bounce_reflect_restitution.GetFloat() * fabs( normalSpeed ) >= bounce_paint_min_speed.GetFloat() )
		{
			// Reflect the velocity
			velocity = ComputeBouncePostVelocityReflection( velocity, info.m_SurfaceNormal, m_PortalLocal.m_StickNormal );
		}
		else
		{
			// Otherwise, perform a jump
			velocity = ComputeBouncePostVelocityNoReflect( velocity, info.m_SurfaceNormal, m_PortalLocal.m_StickNormal );
		}

		if ( sv_debug_bounce_reflection.GetBool() )
		{
			NDebugOverlay::VertArrow( info.m_ContactPoint, info.m_ContactPoint + 50.f * info.m_SurfaceNormal, 2, 255, 255, 0, 64, true, sv_debug_bounce_reflection_time.GetFloat() );
			NDebugOverlay::VertArrow( info.m_ContactPoint, info.m_ContactPoint + 50.f * m_PortalLocal.m_StickNormal, 2, 0, 0, 255, 64, true, sv_debug_bounce_reflection_time.GetFloat() );
			NDebugOverlay::VertArrow( info.m_ContactPoint, info.m_ContactPoint + 50.f * velocity.Normalized(), 2, 0, 255, 0, 64, true, sv_debug_bounce_reflection_time.GetFloat() );
		}

		const bool bounceSurfaceIsGround = AlmostEqual( info.m_SurfaceNormal.z, 1.0f );

		// If the player jumped from the ground
		if( GetInAirState() == ON_GROUND && bounceSurfaceIsGround ) 
		{
			// Add in some bonus lateral velocity for a jump off the ground
			Vector vLateralVelocity = velocity;
			vLateralVelocity.z = 0;
			Vector vMaxLateralVel = vLateralVelocity.Normalized() * (sv_speed_normal.GetFloat() + sv_speed_normal.GetFloat() * sv_bounce_paint_forward_velocity_bonus.GetFloat());

			// If our velocity is lower than is allowed with the lateral bonus
			if( velocity.Length2DSqr() < vMaxLateralVel.Length2DSqr() )
			{
				Vector vBonusLateral = vLateralVelocity * sv_bounce_paint_forward_velocity_bonus.GetFloat();

				// Clip extra velocity that would make us go too fast
				if( (vBonusLateral + velocity).Length2DSqr() > vMaxLateralVel.Length2DSqr() )
				{
					vBonusLateral.x = vMaxLateralVel.x - velocity.x;
					vBonusLateral.y = vMaxLateralVel.y - velocity.y;
				}

				velocity += vBonusLateral;
			}
		}

		// Add some height if the player fell off a ledge onto the ground
		if( GetInAirState() == IN_AIR_FELL && bounceSurfaceIsGround )
		{
			UpdatePaintZ( velocity, GetGravity() );
		}

		SetAbsVelocity( velocity );

		if ( info.m_SurfaceNormal.z > 0.75f )
		{
			OnBounced();
		}

		return true;
	}

	return false;
}


PaintPowerState CPortal_Player::ActivateBouncePower( PaintPowerInfo_t& info )
{
	// Not dead
	bool bUsedPower = false;
	if ( !pl.deadflag )
	{
		bUsedPower = CheckToUseBouncePower( info );
	}

	return bUsedPower ? DEACTIVATING_PAINT_POWER : ACTIVE_PAINT_POWER;
}


PaintPowerState CPortal_Player::UseBouncePower( PaintPowerInfo_t& info )
{
	bool bUsedPower = false;

	if( !pl.deadflag )
	{
		bUsedPower = CheckToUseBouncePower( info );
	}

	return bUsedPower ? DEACTIVATING_PAINT_POWER : ACTIVE_PAINT_POWER;
}


PaintPowerState CPortal_Player::DeactivateBouncePower( PaintPowerInfo_t& info )
{
	// TODO: Fix late jumps. This needs to happen on the first frame of deactivation
	//if( jump_helper_enabled.GetBool() )
	//{
	//	// Cache off this jump in case of a late hit
	//	m_flCachedJumpPowerTime = gpGlobals->curtime;
	//	m_CachedJumpPower = info;
	//}

	// Bring input scale back to 1
	return IsEmptyRange( GetSurfacePaintPowerInfo() ) ? INACTIVE_PAINT_POWER : DEACTIVATING_PAINT_POWER;
}

void CPortal_Player::OnBounced( float fTimeOffset )
{
	m_PortalLocal.m_fBouncedTime = gpGlobals->curtime + fTimeOffset;
}

void CPortal_Player::RecomputeBoundsForOrientation()
{
	// Recompute local min, max based on stick normal.
	// Note: The stick normal is in world space, but since the player AABB doesn't
	//		 transform with the player (the problem this solves), it's OK to use it
	//		 as though it's in local space or world space.
	const Vector& currentStickNormal = m_PortalLocal.m_StickNormal;
	VMatrix rotation;
	MatrixBuildRotation( rotation, Vector(0, 0, 1), currentStickNormal );

	// Rotate the local space hull offsets
	Vector xformedStandMin = rotation.ApplyRotation( VEC_HULL_MIN );
	Vector xformedStandMax = rotation.ApplyRotation( VEC_HULL_MAX );
	Vector xformedDuckMin = rotation.ApplyRotation( VEC_DUCK_HULL_MIN );
	Vector xformedDuckMax = rotation.ApplyRotation( VEC_DUCK_HULL_MAX );

	// Make sure the local origin is at the "bottom" of the hull by offsetting the mins and maxes by the
	// distance along the stick normal from the local origin to the mins
	const Vector standOffset = DotProduct( xformedStandMin, currentStickNormal ) * currentStickNormal;
	const Vector duckOffset = DotProduct( xformedDuckMin, currentStickNormal ) * currentStickNormal;

	xformedStandMin -= standOffset;
	xformedStandMax -= standOffset;
	xformedDuckMin -= duckOffset;
	xformedDuckMax -= duckOffset;

	// Recompute min, max in each dimension
	const Vector newStandHullMin = xformedStandMin.Min( xformedStandMax );
	const Vector newStandHullMax = xformedStandMin.Max( xformedStandMax );
	const Vector newDuckHullMin = xformedDuckMin.Min( xformedDuckMax );
	const Vector newDuckHullMax = xformedDuckMin.Max( xformedDuckMax );

	// Attempt to update the collision bounds
	m_PortalLocal.m_bAttemptHullResize = true;
	TryToChangeCollisionBounds( newStandHullMin, newStandHullMax, newDuckHullMin, newDuckHullMax );
}


void CPortal_Player::TryToChangeCollisionBounds( const Vector& newStandHullMin,
											   const Vector& newStandHullMax,
											   const Vector& newDuckHullMin,
											   const Vector& newDuckHullMax )
{
	// If the bounds haven't already been changed
	if( m_PortalLocal.m_bAttemptHullResize )
	{
		// Trace to find the new abs origin, placing it at the feet
#ifdef CLIENT_DLL
		CTraceFilterSimple traceFilter( this, COLLISION_GROUP_PLAYER_MOVEMENT );
#else
		CTraceFilterSimple baseFilter( this, COLLISION_GROUP_PLAYER_MOVEMENT );
		CTraceFilterTranslateClones traceFilter( &baseFilter );
#endif

		const Vector& currentStickNormal = m_PortalLocal.m_StickNormal;
		const float halfHeight = 0.5f * ComputeHullHeight( m_PortalLocal.m_OldStickNormal, GetHullMins(), GetHullMaxs() );
		const Vector oldCenter = GetAbsOrigin() + halfHeight * m_PortalLocal.m_OldStickNormal;
		Ray_t ray;
		ray.Init( oldCenter, oldCenter - halfHeight * currentStickNormal );
		trace_t trace;
		UTIL_Portal_TraceRay_With( m_hPortalEnvironment, ray, MASK_PLAYERSOLID, &traceFilter, &trace, true );
		const Vector newAbsOrigin = trace.endpos /*+ EQUAL_EPSILON * currentStickNormal*/;

		// Trace to see if the new box fits where it needs to
		const bool isDucking = (GetFlags() & FL_DUCKING) != 0;
		const Vector& traceHullMin = isDucking ? newDuckHullMin : newStandHullMin;
		const Vector& traceHullMax = isDucking ? newDuckHullMax : newStandHullMax;
		ray.Init( newAbsOrigin, newAbsOrigin, traceHullMin, traceHullMax );
		UTIL_Portal_TraceRay_With( m_hPortalEnvironment, ray, MASK_PLAYERSOLID, &traceFilter, &trace, true );

		// If the trace is fine
		if( !trace.DidHit() )
		{
			// Give ourselves a punch offset, since our origin just snapped a noticeable amount
			SetEyeOffset( GetAbsOrigin(), newAbsOrigin );

#ifdef GAME_DLL
			// Apply the changes to the abs origin
			Teleport( &newAbsOrigin, NULL, NULL );
#endif

			// Apply the changes to the hull
			m_PortalLocal.m_StandHullMin = newStandHullMin;
			m_PortalLocal.m_StandHullMax = newStandHullMax;
			m_PortalLocal.m_DuckHullMin = newDuckHullMin;
			m_PortalLocal.m_DuckHullMax = newDuckHullMax;

			// Apply the changes to collision bounds
			SetCollisionBounds( GetHullMins(), GetHullMaxs() );

#ifdef GAME_DLL
			InitVCollision( newAbsOrigin, GetAbsVelocity() );
#endif

			// Don't attempt to resize again
			m_PortalLocal.m_bAttemptHullResize = false;
		}
		// If the trace hit something
		else
		{
			// Cache off the new hull and try again later
			m_PortalLocal.m_CachedStandHullMinAttempt = newStandHullMin;
			m_PortalLocal.m_CachedStandHullMaxAttempt = newStandHullMax;
			m_PortalLocal.m_CachedDuckHullMinAttempt = newDuckHullMin;
			m_PortalLocal.m_CachedDuckHullMaxAttempt = newDuckHullMax;
			m_PortalLocal.m_bAttemptHullResize = true;
		}
	}
}


CBaseCombatWeapon* CPortal_Player::Weapon_OwnsThisType( const char *pszWeapon, int iSubType ) const
{
	for ( int i = 0; i < MAX_WEAPONS; i++ ) 
	{
		CBaseCombatWeapon* pWeapon = GetWeapon(i);
		if ( pWeapon && FClassnameIs( pWeapon, pszWeapon ) )
		{
			// Make sure it matches the subtype
			return pWeapon;
		}
	}

	return NULL;
}


void CPortal_Player::SelectItem( const char *pstr, int iSubType )
{
	if (!pstr)
		return;

	CBaseCombatWeapon *pItem = Weapon_OwnsThisType( pstr, iSubType );

	if (!pItem)
		return;

	if( GetObserverMode() != OBS_MODE_NONE )
		return;// Observers can't select things.

	if ( !Weapon_ShouldSelectItem( pItem ) )
	{
		//always set subtype when select weapon to fix weird red color not being set
		GetActiveWeapon()->SetSubType( iSubType );

		return;
	}


	// FIX, this needs to queue them up and delay
	// Make sure the current weapon can be holstered
	if ( GetActiveWeapon() )
	{
		if ( !GetActiveWeapon()->CanHolster() )
			return;

		ResetAutoaim( );
	}

	Weapon_Switch( pItem );
}


void CPortal_Player::Touch( CBaseEntity *pOther )
{
#ifndef CLIENT_DLL
	// Check for doors closing on the player
	if ( GameRules()->IsMultiplayer() )
	{
		CBoneFollower *pBoneFollower = dynamic_cast< CBoneFollower* >( pOther );
		if ( pBoneFollower )
		{
			CPropTestChamberDoor *pDoor = dynamic_cast< CPropTestChamberDoor* >( pBoneFollower->GetOwnerEntity() );
			if ( pDoor && pDoor->IsClosed() )
			{
				// Make sure the door is closing on them, and they're not just touching the outside
				Ray_t ray;
				ray.Init( GetAbsOrigin(), GetAbsOrigin(), GetHullMins(), GetHullMaxs() );

				trace_t tr;
				bool bHit = IntersectRayWithOBB( ray, pDoor->GetAbsOrigin(), pDoor->GetAbsAngles(), Vector( -4.0f, -64.0f, 0.0f ), Vector( 4.0f, 64.0f, 128.0f ), 0.0f, &tr );

				if ( bHit )
				{
					// In multiplayer we CRUSH them, cause it's funny
					CTakeDamageInfo dmgInfo( GetWorldEntity(), GetWorldEntity(), vec3_origin, vec3_origin, 1000, DMG_CRUSH );
					dmgInfo.SetDamageForce( Vector( 0, 0, -1 ) );
					dmgInfo.SetDamagePosition( GetAbsOrigin() );
				
					TakeDamage( dmgInfo );
				}
			}
		}
	}
#endif

	BaseClass::Touch( pOther );
}


float CPortal_Player::GetReorientationProgress() const
{
	Vector vUp = m_PortalLocal.m_Up;
	Vector vStickUp = m_PortalLocal.m_vLocalUp;
	vUp.NormalizeInPlace();
	vStickUp.NormalizeInPlace();

	float fDot = DotProduct( vUp , vStickUp );
	fDot = clamp( fDot, 0.f, 1.f );
	return fDot;
}

bool CPortal_Player::IsDoneReorienting() const
{
	const QAngle& qPunch = m_PortalLocal.m_qQuaternionPunch;

	// To be considered done reorienting we have to not be pitching,
	// not be rotating our up vector, be fully reoriented, and not have any
	// quaternion punch going on
	return m_PortalLocal.m_bDoneCorrectPitch &&
		m_PortalLocal.m_bDoneStickInterp &&
		CloseEnough( GetReorientationProgress(), 1.f ) &&
		qPunch.LengthSqr() == 0.f;
}

float AngleBetween(Vector up, Vector forward)
{
	float flDot = DotProduct(up,forward);
	float flAngle = RAD2DEG(acos(flDot));
	return flAngle;
}

void CPortal_Player::PostTeleportationCameraFixup( const CPortal_Base2D *pEnteredPortal )
{
	CPortal_Base2D *pExitPortal = pEnteredPortal->m_hLinkedPortal;
	if( !pExitPortal )
		return;

	// Get transformed up
	Vector vTransformedUp, vTransformedForward, vForward, vUp;
	AngleVectors( pl.v_angle, &vForward, NULL, &vUp );

	VectorRotate( m_PortalLocal.m_Up, pEnteredPortal->m_matrixThisToLinked.As3x4(), vTransformedUp );

	const Vector vForwardCrossAbsUp = CrossProduct( vForward, ABS_UP );
	const Vector vForwardCrossViewUp = CrossProduct( vForward, vUp );

	// Dont bother if the vectors are the same (wall<->wall or ceiling<->floor portal)
	if( CloseEnough( vTransformedUp, m_PortalLocal.m_Up ) == false )
	{
		bool bLookingInBadDirection = false;
		const float flRightCrossNewRight = DotProduct( vForwardCrossAbsUp, vForwardCrossViewUp );
		// Check if our up vector is more upside down than rightside up
		if( flRightCrossNewRight < 0.f )
		{
			bLookingInBadDirection = true;
		}

		m_PortalLocal.m_Up = vTransformedUp;

		// Let SnapCamera mess with our up vector
		SnapCamera( STICK_CAMERA_PORTAL, bLookingInBadDirection );
	}
}

//#define DEBUG_STICK_CAM

void CPortal_Player::Reorient( QAngle& viewAngles )
{

	DecayEyeOffset();
	DecayQuaternionPunch();

	QAngle oldViewAngles = viewAngles;

	// Get vectors out of our angles
	Vector vForward, vUp, vRight;
	AngleVectors( oldViewAngles, &vForward, &vRight, NULL );
	vUp = m_PortalLocal.m_Up;

	RotateUpVector( vForward, vUp ); // Reorient the player
	m_PortalLocal.m_Up = vUp;

	// use up vector as our up when sticking on to new surface
	{
		QAngle newViewAngles;
		VectorAngles( vForward, vUp, newViewAngles );

		QAngle viewOffset( 0, 0, 0 );
		UTIL_NormalizedAngleDiff( oldViewAngles, newViewAngles, &viewOffset );

		// round angles to 0 if they are too small
		for ( int i=0; i<3; ++i )
		{
			if ( fabs( viewOffset[i] ) < 0.001f )
			{
				viewOffset[i] = 0.f;
			}
		}

		viewAngles = oldViewAngles + viewOffset;

#ifdef DEBUG_STICK_CAM
	#ifdef GAME_DLL
			engine->Con_NPrintf( 1, "%f, %f, %f", XYZ(vUp) );
			engine->Con_NPrintf( 2, "%f, %f, %f", XYZ( Vector(m_PortalLocal.m_vEyeOffset) ) );
			engine->Con_NPrintf( 3, "%f, %f, %f", XYZ( GetLocalOrigin() ) );
			QAngle quatAngles = m_PortalLocal.m_qQuaternionPunch;
			engine->Con_NPrintf( 4, "%f, %f, %f", quatAngles.x, quatAngles.y, quatAngles.z );
	#else
			engine->Con_NPrintf( 6, "%f, %f, %f", XYZ(vUp) );
			engine->Con_NPrintf( 7, "%f, %f, %f", XYZ( Vector(m_PortalLocal.m_vEyeOffset) ) );
			engine->Con_NPrintf( 8, "%f, %f, %f", XYZ( GetLocalOrigin() ) );
			QAngle quatAngles = m_PortalLocal.m_qQuaternionPunch;
			engine->Con_NPrintf( 9, "%f, %f, %f", quatAngles.x, quatAngles.y, quatAngles.z );
	#endif
#endif
	}

	// Set our camera state if we just finished the "easy" transitions
	if( m_PortalLocal.m_bDoneStickInterp )
	{
		if( m_PortalLocal.m_nStickCameraState == STICK_CAMERA_PORTAL )
		{
			m_PortalLocal.m_nStickCameraState = STICK_CAMERA_UPRIGHT;
		}
	}
}

void CPortal_Player::SetQuaternionPunch( const Quaternion& qPunch )
{
	QAngle qTempAngle;
	QuaternionAngles( qPunch, qTempAngle );

	qTempAngle.x = anglemod( qTempAngle.x );
	qTempAngle.y = anglemod( qTempAngle.y );
	qTempAngle.z = anglemod( qTempAngle.z );

	m_PortalLocal.m_qQuaternionPunch = qTempAngle;
}

void CPortal_Player::DecayQuaternionPunch()
{
	// Convert angles to quaternion
	Quaternion qOut;
	AngleQuaternion( m_PortalLocal.m_qQuaternionPunch, qOut );
	// Quaternion to axis angles
	Vector vAxis;
	float flAngle;
	QuaternionAxisAngle( qOut, vAxis, flAngle );

	// Decay the angle
	flAngle *= ExponentialDecay( stick_cam_pitch_rate, gpGlobals->frametime );
	// Snap to 0 if small enough
	flAngle = fabs(flAngle) < 0.15f ? 0.f : flAngle;

	// Convert back. Axis angle to quaternion
	AxisAngleQuaternion( vAxis, flAngle, qOut );
	// Convert quaternion to angles
	QAngle anglesOut;
	QuaternionAngles( qOut, anglesOut );

	anglesOut.x = anglemod( anglesOut.x );
	anglesOut.y = anglemod( anglesOut.y );
	anglesOut.z = anglemod( anglesOut.z );
	
	// Store it
	m_PortalLocal.m_qQuaternionPunch = anglesOut;
}


void CPortal_Player::SnapCamera( StickCameraState nCameraState, bool bOverPitched )
{	
	// Default to set the player's up view
	StickCameraCorrectionMethod correctionMethod = DO_NOTHING;

	// Get our vectors
	Vector vForward, vUp, vRight, vNewForward;
	Vector vQuaternionRollAxis = m_PortalLocal.m_OldStickNormal;
	AngleVectors( pl.v_angle, &vForward, &vRight, &vUp );
	bool bRollCorrect = false, bPitchCorrect = false;

	// Setup data we'll need to test for specific cases
	const float flForwardDotLocalUp = DotProduct( vForward, m_PortalLocal.m_vLocalUp );
	Vector vForwardInXY = vForward - ( m_PortalLocal.m_vLocalUp * flForwardDotLocalUp );
	vForwardInXY.NormalizeInPlace();

	const float flUpDotAbsUp = DotProduct( m_PortalLocal.m_Up, ABS_UP );

//	const float flUpDot = fabs( pEnterPortal->m_matrixThisToLinked.m[2][2] ); //How much does zUp still look like zUp after going through this portal

//	bool bSignificantAngle = (flUpDot < COS_PI_OVER_SIX && (sv_portal_new_player_trace.GetBool() || (flUpDot >= EQUAL_EPSILON)));

	// Portalling
	if( nCameraState == STICK_CAMERA_PORTAL )
	{
		// Portalling: Floor->Floor and Ceiling->Ceiling
		// Over pitched and looking within a threshold where we want to pitch rather than roll.
		if( bOverPitched && 
			fabs(flForwardDotLocalUp) > stick_cam_over_the_top_threshold &&
			AlmostEqual( fabs(flUpDotAbsUp), 1.f ) )
		{
			correctionMethod = QUATERNION_CORRECT;
			bPitchCorrect = true;
			bRollCorrect = false;

#ifdef DEBUG_STICK_CAM
			DevMsg("Portal: Floor->Floor or Ceiling->Ceiling.");
#endif
		}
		// Portalling: Wall->Floor and Wall->Ceiling
		// Over pitched and looking within a threshold where we want to pitch rather than roll or
		// they're not overpitched but are still within a threshold where we want to slightly roll them (which will appear
		// to be a yaw relative to their perspective) rather than actuall roll.
		else if( ( bOverPitched &&
			fabs(flForwardDotLocalUp) > stick_cam_over_the_top_threshold ) ||
			( fabs(flForwardDotLocalUp) > stick_cam_pitch_vs_roll_good_angle_threshold ) )
		{
			correctionMethod = QUATERNION_CORRECT;
			bRollCorrect = true;
			bPitchCorrect = bOverPitched;
			vQuaternionRollAxis = vUp;

#ifdef DEBUG_STICK_CAM
			DevMsg("Portal: Wall->Floor or Wall->Ceiling.");
			if( bOverPitched )
				DevMsg("\tPitching");
#endif
		}
		else	// Normal portalling where we want to roll
		{
			correctionMethod = SNAP_UP;

#ifdef DEBUG_STICK_CAM
			DevMsg("Portal: Snap up");
#endif
		}
	}

	// Instantly snap the player's view into a good state, and use a quaternion view punch to offset their
	//		view to where it was before the snap then decay the quaternion.
	if( correctionMethod == QUATERNION_CORRECT )
	{
		const Vector vForwardTarget = DotProduct( vForward, ABS_UP ) > 0 ? ABS_UP : -ABS_UP; 
		const float flForwardDotRollAxis = DotProduct( vForward, vQuaternionRollAxis );

		// Project forward vector onto the plane we just came off of
		Vector vForwardInPlane = vForward - ( vQuaternionRollAxis * flForwardDotRollAxis );
		vForwardInPlane.NormalizeInPlace();

		const float flRightDotTarget = DotProduct( vRight, vForwardTarget );
		const float flRightAngle = 90.0f - RAD2DEG( acos( flRightDotTarget ) );
		// Determine how rolled we are
		const float flRollAngle = -flRightAngle;

		// Create matrix to undo roll
		VMatrix rotMatrix;
		MatrixBuildRotationAboutAxis( rotMatrix, vQuaternionRollAxis, flRollAngle );
		// Create quaternion to preserve roll for punch
		Quaternion qRoll(0,0,0,1);	// Identity

		if( bRollCorrect )
		{
			AxisAngleQuaternion( vQuaternionRollAxis, -flRollAngle, qRoll );
			// Roll our forward vector
			vForward = rotMatrix * vForward;
		}

		// Determine how much pitch we need to do
		const float flRolledForwardDotUp = DotProduct( vForward, vForwardTarget );
		float flPitchAngle = RAD2DEG( acos( flRolledForwardDotUp ) );	// This angle will get us straight up
		// Over-pitch if we're looking in a "bad direction" (ie. over-pitched in target orientation)
		if( bOverPitched )
		{
			flPitchAngle = flPitchAngle > 0 ? flPitchAngle + 1 : flPitchAngle - 1;
		}
		else
		{
			flPitchAngle = flPitchAngle > 0 ? flPitchAngle - 1 : flPitchAngle + 1;
		}

		// Build our pitch matrix
		Vector vRotAxis = CrossProduct( vForward, vForwardTarget );
		vRotAxis.NormalizeInPlace();
		MatrixBuildRotationAboutAxis( rotMatrix, vRotAxis, flPitchAngle );

		// Build quaternion to preserve this pitch for punch
		Quaternion qPitch(0,0,0,1);	// Identity

		if( bPitchCorrect )
		{
			AxisAngleQuaternion( vRotAxis, -flPitchAngle, qPitch );

			// Pitch our forward vector
			vForward = rotMatrix * vForward;
		}

		// Accumulate rotations
		Quaternion qPunch;
		QuaternionNormalize( qRoll );
		QuaternionNormalize( qPitch );
		QuaternionMult( qRoll, qPitch, qPunch );
		// Punch ourselves in the face from the 4th dimension
		SetQuaternionPunch( qPunch );

		// Get our new angles
		QAngle vNewAngles;
		VectorAngles( vForward, vUp, vNewAngles );

		pl.v_angle = vNewAngles;

#ifdef GAME_DLL
		// Only snap player's eyes in a singleplayer game
		/*if( g_pGameRules->IsMultiplayer() == false )
		{
			pl.fixangle = FIXANGLE_ABSOLUTE;
		}*/
#else
		if( IsLocalPlayer() && (GET_ACTIVE_SPLITSCREEN_SLOT() == GetSplitScreenPlayerSlot()) )
		{
			engine->SetViewAngles( pl.v_angle );
			if( prediction->InPrediction() )
			{
				prediction->SetViewAngles( pl.v_angle );
			}
		}
#endif

		m_PortalLocal.m_Up = m_PortalLocal.m_StickNormal;
		m_PortalLocal.m_bDoneCorrectPitch = true;

	}
	// Snap up vector to be our view up then have it rotate from there to our local up
	else if( correctionMethod == SNAP_UP )
	{
		Vector vNewUp;
		AngleVectors( pl.v_angle, NULL, NULL, &vNewUp );

		// Check if our up is more abs down than abs up.  If so, don't let it get too far behind our forward
		if( DotProduct(vNewUp, m_PortalLocal.m_vLocalUp ) < -0.0f )
		{
			// Make up be coplanar with stick normal and orthogonal to forward
			vNewUp -= vForwardInXY * DotProduct( vForwardInXY, vNewUp );
			vNewUp.NormalizeInPlace();

			// Determine rotation axis
			const Vector vForwardCrossUp = CrossProduct( vForwardInXY, m_PortalLocal.m_vLocalUp );
			m_PortalLocal.m_vStickRotationAxis = DotProduct( vForwardCrossUp, vNewUp ) > 0.f ? -vForwardInXY.Normalized() : vForwardInXY.Normalized();
		}
		else
		{
			m_PortalLocal.m_vStickRotationAxis = CrossProduct( vUp, m_PortalLocal.m_vLocalUp ).Normalized();
		}

		m_PortalLocal.m_Up = vNewUp;
		m_PortalLocal.m_bDoneStickInterp = false;

	}	
	// Rotate our up vector from where it currently is to our local up
	else if( correctionMethod == ROTATE_UP )
	{
		m_PortalLocal.m_vStickRotationAxis = CrossProduct( m_PortalLocal.m_Up, m_PortalLocal.m_vLocalUp ).Normalized();
		m_PortalLocal.m_bDoneStickInterp = false;
	}


	m_PortalLocal.m_nStickCameraState = nCameraState;

#ifdef DEBUG_STICK_CAM
	DevMsg("\n");
#endif
}


void CPortal_Player::RotateUpVector( Vector& vForward, Vector& vUp )
{
	float flRotationAmt = 0.f;
	const Vector vStartUp = vUp.Normalized();
	const Vector vStartForward = vForward.Normalized();

	const Vector vForwardInRotationPlane = vForward - ( m_PortalLocal.m_vStickRotationAxis * DotProduct(vForward,m_PortalLocal.m_vStickRotationAxis) );
	const Vector vPreRotationRight = CrossProduct( vForwardInRotationPlane, vUp );
	const float vRotationRate = stick_cam_roll_rate;

	if( m_PortalLocal.m_bDoneStickInterp == false )
	{
		Vector vEndUp = m_PortalLocal.m_vLocalUp;
		vEndUp.NormalizeInPlace();

		// Find the angle between our current up and our destination up
		float flDot = DotProduct( vStartUp, vEndUp );
		flDot = clamp( flDot, -1, 1 );
		float flAngleBetween = RAD2DEG( acos( flDot ) );

		// If there's still significant difference between our current up and our destination up
		if( fabs( flAngleBetween ) > 0.1f )
		{
			float flDecayTo = 0.f;
			flDecayTo = flDecayTo > vRotationRate ? flDecayTo : vRotationRate;

			// Exponentially decay towards destination
			float flDecayAngle = flAngleBetween * ExponentialDecay( flDecayTo, gpGlobals->frametime );
			flRotationAmt = flAngleBetween - flDecayAngle;

			// Have constant rotation once the rotation rate becomes very small
			if( gpGlobals->frametime != 0.f &&
				( flRotationAmt / gpGlobals->frametime < stick_cam_min_rotation_rate ) )
			{
				flRotationAmt = stick_cam_min_rotation_rate * gpGlobals->frametime;
			}

			// Slow down rotation if forward vector and up vector get very close to each other
			if( m_PortalLocal.m_nStickCameraState == STICK_CAMERA_UPRIGHT )
			{
				float flAngle = DotProduct( vForward, vUp );

				if( flAngle > 0.9f )
				{
					float flDamp = RemapValClamped( flAngle, 0.9f, 1.f, 1.f, 5.f ); 
					flDamp *= flDamp;
					flRotationAmt /= flDamp;
				}
			}

			// Possibly flip the rotation axis if it's not on the side we expect it to be on
			if( DotProduct( m_PortalLocal.m_vStickRotationAxis, CrossProduct( vUp, m_PortalLocal.m_vLocalUp ) ) < 0.f )
			{
				Vector v = m_PortalLocal.m_vStickRotationAxis;
				m_PortalLocal.m_vStickRotationAxis = -v;
			}

			// Build rotation matrix
			VMatrix rotMatrix;
			MatrixBuildRotationAboutAxis( rotMatrix, m_PortalLocal.m_vStickRotationAxis, flRotationAmt );

			// Rotate up
			vUp = rotMatrix * vUp;
			vUp.NormalizeInPlace();

			if( m_PortalLocal.m_bDoneCorrectPitch == false )
			{
				vForward = rotMatrix * vForward;
				vForward.NormalizeInPlace();
			}
		}
		else
		{
			// Just snap
			vUp = vEndUp;
			m_PortalLocal.m_bDoneStickInterp = true;
		}

	}


	// Should we bother trying to correct pitch?
	if ( stick_cam_correct_pitch && m_PortalLocal.m_bDoneCorrectPitch == false )
	{
		// Check if we want to not correct the player's pitch because we think they're trying to correct it themselves
		const CUserCmd *cmd = m_pCurrentCommand;
		// If they're mousing around, don't correct pitch
		if( cmd && abs( cmd->mousedy ) > sv_stick_input_cancel_threshold && GetReorientationProgress() > 0.9f )
		{
			m_PortalLocal.m_bDoneCorrectPitch = true;
			return;
		}

		// Build the matrix
		VMatrix rotMatrix;
		MatrixBuildRotationAboutAxis( rotMatrix, m_PortalLocal.m_vStickRotationAxis, flRotationAmt);
		// Rotate forward along with up

	}// if !m_bDoneCorrectPitch

//#ifdef DEBUG_STICK_CAM
//#ifdef GAME_DLL
//	if( m_nButtons & IN_GRENADE2 )
//	{
//		Vector vRight = CrossProduct( vForward, vUp );
//		vRight.NormalizeInPlace();
//		//		NDebugOverlay::Sphere( EyePosition() + 20.f * vUp, 2.f, 255, 255, 255, true, 0.f );
//		//		NDebugOverlay::Sphere( EyePosition() + 20.f * m_PortalLocal.m_vLocalUp, 2.f, 0, 205, 50, true, 0.f );
//		NDebugOverlay::Line( GetAbsOrigin() , GetAbsOrigin() + (vForward*50) , 205, 50, 0, true, 100 );
//		NDebugOverlay::Line( GetAbsOrigin() , GetAbsOrigin() + (m_PortalLocal.m_vLocalUp*50) , 0, 205, 50, true, 100 );
//		NDebugOverlay::Line( GetAbsOrigin() , GetAbsOrigin() + (vRight*50),0,0,255,true,100);
//		NDebugOverlay::Line( GetAbsOrigin() , GetAbsOrigin() + (vUp*50),255,255,255,true,100);
//	}
//#endif
//#endif

}


void CPortal_Player::PreventCrouchJump( CUserCmd* ucmd )
{
	if( prevent_crouch_jump.GetBool() && GetInAirState() == IN_AIR_JUMPED )
	{
		m_PortalLocal.m_bPreventedCrouchJumpThisFrame = (ucmd->buttons & IN_DUCK) != 0;
		ucmd->buttons &= ~IN_DUCK;
	}
}

void CPortal_Player::BridgeRemovedFromUnder( void )
{
	m_PlayerAnimState->BridgeRemovedFromUnder();
}


const Vector ComputeJumpVelocity( const Vector& normal,
								 const Vector& localUp,
								 float desiredNormalSpeed )
{
	Vector outBounceVelocity = vec3_origin;
	const float flNormDot = DotProduct( localUp, normal );
	const float flTrade = sv_wall_bounce_trade.GetFloat();	// We trade some outward velocity for upward velocity

	// Add upward velocity if surface normal is facing up, relative to the player.
	float flBounceScale = 0.f;

	if( sv_press_jump_to_bounce.GetInt() != TRAMPOLINE_BOUNCE || bounce_reflect_wall_jumps_enabled.GetBool() )
	{
		if( flNormDot > -0.1f )
		{
			// Extra upward wall bounce velocity
			flBounceScale = 1.f - flNormDot;
			outBounceVelocity += localUp * bounce_paint_wall_jump_upward_speed.GetFloat() * flBounceScale;
		}
		// Downward facing wall. Add velocity in the XY plane.
		else
		{
			// Vector pointing out of the surface in the XY plane
			Vector vOut = ( normal - (localUp * flNormDot) ).Normalized();

			// Extra lateral velocity off the wall
			flBounceScale = DotProduct( vOut, normal );
			outBounceVelocity += vOut * bounce_paint_wall_jump_upward_speed.GetFloat() * flBounceScale;
		}
	}

	// Calculate how much bounce velocity is left to spend after the lateral
	float fWallBounceScale = flTrade + ( (1.f - flBounceScale) * (1.0 - flTrade) );

	// Velocity off of the surface
	outBounceVelocity += normal * desiredNormalSpeed * fWallBounceScale;

	return outBounceVelocity;
}


const Vector ComputeWallJumpHelperVelocity( const Vector& finalBounceVelocity,
										   const Vector& normal )
{
	Vector velocity = finalBounceVelocity;
	if( sv_wall_jump_help.GetBool() )
	{
		Vector vecOutVelocity = velocity;
		vecOutVelocity.z = 0.0f;
		VectorNormalize( vecOutVelocity );
		float flOutAngle = RAD2DEG( acos( clamp( DotProduct( normal, vecOutVelocity ), -1.0f, 1.0f ) ) );
		float flCorrection = 0.0f;

		//Do not correct if the out angle is the same as the normal
		if( flOutAngle == 0.0f )
		{
			flCorrection = 0.0f;
			return velocity;
		}

		float flSign = vecOutVelocity.x * normal.y - vecOutVelocity.y * normal.x;

		//The out angle is small enough to snap to the normal
		if( flOutAngle <= sv_wall_jump_help_amount.GetFloat() )
		{
			flCorrection = flOutAngle;
		}
		//The out angle needs to be corrected to the normal dir
		else if( flOutAngle <= sv_wall_jump_help_threshold.GetFloat() )
		{
			flCorrection = sv_wall_jump_help_amount.GetFloat();
		}

		//Rotate the out velocity towards the normal
		VMatrix matRotate;
		if( flSign > 0.0f )
		{
			MatrixBuildRotateZ( matRotate, flCorrection );
		}
		else
		{
			MatrixBuildRotateZ( matRotate, -flCorrection );
		}

		Vector rotatedVelocity;
		VectorRotate( velocity, matRotate.As3x4(), rotatedVelocity );
		velocity = rotatedVelocity;

		//#ifndef CLIENT_DLL
		//if( sv_wall_jump_help_debug.GetBool() )
		//{
		//	Vector vecInVelocity = velocity;
		//	vecInVelocity.z = 0.0f;
		//	VectorNormalize( vecInVelocity );
		//	float flInAngle = 180.0f - RAD2DEG( acos( DotProduct( normal, vecInVelocity ) ) );
		//	DevMsg( "InAngle: %f, OutAngle: %f, Correction: %f\n", flInAngle, flOutAngle, flCorrection );
		//}
		//#endif //!CLIENT_DLL
	}

	return velocity;
}


const Vector ComputeBouncePostVelocityReflection( const Vector& preVelocity,
												 const Vector& normal,
												 const Vector& localUp )
{
	// Cancel out velocity in the normal direction
	const float normalSpeed = DotProduct( preVelocity, normal );
	const Vector normalVelocity = normalSpeed * normal;
	Vector velocity = preVelocity;
	velocity -= normalVelocity;

	// The base bounce velocity is the remaining velocity required to reflect the
	// velocity when the player hit the surface.
	Vector bounceVelocity = -bounce_reflect_restitution.GetFloat() * normalVelocity;

	// Optionally trade some outward velocity for upward velocity on wall jumps
	const bool shouldWallJump = bounce_reflect_wall_jumps_enabled.GetBool() && CloseEnough( normal.z, 0.0f );
	if( shouldWallJump )
	{
		bounceVelocity = ComputeJumpVelocity( normal, localUp, fabs( normalSpeed ) );

		// Don't let the velocity tangent to the surface cancel out the wall jump
		const float minWallJumpUpSpeed = bounce_reflect_wall_jump_min_up_speed.GetFloat();
		if( bounceVelocity.z + velocity.z < minWallJumpUpSpeed )
		{
			bounceVelocity.z += minWallJumpUpSpeed - velocity.z;
		}
	}

	velocity += bounceVelocity;

	// If wall jumps are allowed, use the wall jump helper to bring
	// the post impact velocity closer to the normal direction
	if( shouldWallJump )
		velocity = ComputeWallJumpHelperVelocity( velocity, normal );

	return velocity;
}


const Vector ComputeBouncePostVelocityNoReflect( const Vector& preVelocity,
												const Vector& normal,
												const Vector& localUp )
{
	Vector vBounceVel(0,0,0);

	Vector velocity = preVelocity;
	velocity -= normal * DotProduct( velocity, normal );

	// Cancel out downward velocity (allows for going up parallel walls)
	velocity -= localUp * DotProduct( velocity, localUp );

	// Store this for later
	Vector velNorm = velocity;
	velNorm.NormalizeInPlace();

	// Add upward velocity if surface normal is facing perpendicular to up, relative to the player
	vBounceVel = ComputeJumpVelocity( normal, localUp, bounce_paint_min_speed.GetFloat() );

	// Don't let our velocity fight the new bounce velocity
	velocity -= velocity.Normalized() * DotProduct( velocity, vBounceVel.Normalized() );
	velocity += vBounceVel;

	// Use the wall jump helper to bring the post jump velocity closer to the normal direction
	velocity = ComputeWallJumpHelperVelocity( velocity, normal );

	return velocity;
}


void ExpandAABB( Vector& boxMin, Vector& boxMax, const Vector& sweepVector )
{
	for( unsigned i = 0; i < 3; ++i )
	{
		boxMin[i] += sweepVector[i] < 0 ? sweepVector[i] : 0;
		boxMax[i] += sweepVector[i] > 0 ? sweepVector[i] : 0;
	}
}

void BrushContact::Initialize( const Vector& contactPt,
							   const Vector& planeNormal,
							   CBaseEntity* pEntity,
							   bool onThinSurface )
{
	point = contactPt;
	normal = planeNormal;
	pBrushEntity = pEntity;
	isOnThinSurface = onThinSurface;
}

void BrushContact::Initialize( const fltx4& contactPt,
							  const fltx4& planeNormal,
							  CBaseEntity* pEntity,
							  bool onThinSurface )
{
	StoreUnaligned3SIMD( &point.x, contactPt );
	StoreUnaligned3SIMD( &normal.x, planeNormal );
	pBrushEntity = pEntity;
	isOnThinSurface = onThinSurface;
}

typedef CUtlVector<Vector4D> PlaneVector;

void AddBboxToPlaneList( Vector4D *pListInsertPosition, const Vector &vMins, const Vector &vMaxs )
{
	pListInsertPosition[0] = Vector4D(1, 0, 0, vMaxs.x);
	pListInsertPosition[1] = Vector4D(-1, 0, 0, -vMins.x);
	pListInsertPosition[2] = Vector4D(0, 1, 0, vMaxs.y);
	pListInsertPosition[3] = Vector4D(0, -1, 0, -vMins.y);
	pListInsertPosition[4] = Vector4D(0, 0, 1, vMaxs.z);
	pListInsertPosition[5] = Vector4D(0, 0, -1, -vMins.z);
}

void AddBboxToPlaneList_SIMD( fltx4 *pListInsertPosition, const fltx4 &vMins, const fltx4 &vMaxs )
{
	pListInsertPosition[0] = SetWFromXSIMD( g_SIMD_Identity[0], vMaxs );	//	Vector4D(1, 0, 0, vMaxs.x);
	pListInsertPosition[1] = -SetWFromXSIMD( g_SIMD_Identity[0], vMins );	//	Vector4D(-1, 0, 0, -vMins.x);
	pListInsertPosition[2] = SetWFromYSIMD( g_SIMD_Identity[1], vMaxs );	//  Vector4D(0, 1, 0, vMaxs.y);
	pListInsertPosition[3] = -SetWFromYSIMD( g_SIMD_Identity[1], vMins );	//	Vector4D(0, -1, 0, -vMins.y);
	pListInsertPosition[4] = SetWFromZSIMD( g_SIMD_Identity[2], vMaxs );	//	Vector4D(0, 0, 1, vMaxs.z);
	pListInsertPosition[5] = -SetWFromZSIMD( g_SIMD_Identity[2], vMins );	//	Vector4D(0, 0, -1, -vMins.z);
}

inline const Vector ToVector( const float* vec )
{
	return Vector( vec[0], vec[1], vec[2] );
}


void DebugDrawMesh( const CMesh& mesh )
{
	for ( int i = 0; i < mesh.TriangleCount(); ++i )
	{
		int i0 = mesh.m_pIndices[i*3 + 0];
		int i1 = mesh.m_pIndices[i*3 + 1];
		int i2 = mesh.m_pIndices[i*3 + 2];
		NDebugOverlay::Triangle( ToVector( mesh.GetVertex(i0) ),
			ToVector( mesh.GetVertex(i1) ),
			ToVector( mesh.GetVertex(i2) ),
			255, 0, 0, 128, true, 0 );

	}
}


const Vector ComputeCentroid( const CMesh& volume )
{
	Vector centroid( 0, 0, 0 );
	for ( int i = 0; i < volume.m_nVertexCount; ++i )
	{
		//NDebugOverlay::Sphere( ToVector( volume.GetVertex(i) ), 5.0f, 0, 255, 255, false, 0 );
		centroid += ToVector( volume.GetVertex(i) );
	}

	centroid *= (1.0f / volume.m_nVertexCount);
	return centroid;
}

const fltx4 ComputeCentroid_SIMD( const CMesh& volume )
{
	fltx4 centroid = Four_Zeros;
	int nCount = volume.m_nVertexCount;
	fltx4 f4NumberOfVertices = ReplicateX4( (float)nCount );
	int nIndex = 0;
	while ( nCount >= 4 )
	{
		fltx4 p0 = *(const fltx4 *)volume.GetVertex( nIndex );
		fltx4 p1 = *(const fltx4 *)volume.GetVertex( nIndex + 1 );
		fltx4 p2 = *(const fltx4 *)volume.GetVertex( nIndex + 2 );
		fltx4 p3 = *(const fltx4 *)volume.GetVertex( nIndex + 3 );
		// Try to reduce register dependency a bit
		fltx4 first2 = AddSIMD( p0, p1 );
		fltx4 second2 = AddSIMD( p2, p3 );
		centroid = AddSIMD( centroid, first2 );
		centroid = AddSIMD( centroid, second2 );
		nCount -= 4;
		nIndex += 4;
	}
	while ( nCount > 0 )
	{
		fltx4 p0 = *(const fltx4 *)volume.GetVertex( nIndex );
		centroid = AddSIMD( centroid, p0 );
		--nCount;
		++nIndex;
	}

	centroid = MulSIMD( centroid, ReciprocalSIMD( f4NumberOfVertices ) );
	return centroid;
}

// outsidePt is a point that should be on the positive side of the plane defined by the triangle
bool ComputeContactPlane( cplane_t& plane, int& contactTriangleIndex, const CMesh& contactRegion, const Vector& outsidePt )
{
	Vector contactNormal( 0, 0, 1 );
	float maxAreaSq = -FLT_MAX;
	int p0VertexIndex = 0;
	const int triangleCount = contactRegion.TriangleCount();
	contactTriangleIndex = triangleCount;

	for( int i = 0; i < triangleCount; ++i )
	{
		// Get the vertices of the triangle
		const int triangleIndex = i * 3;
		const int i0 = contactRegion.m_pIndices[triangleIndex];
		const int i1 = contactRegion.m_pIndices[triangleIndex + 1];
		const int i2 = contactRegion.m_pIndices[triangleIndex + 2];

		const Vector p0 = ToVector( contactRegion.GetVertex(i0) );
		const Vector p1 = ToVector( contactRegion.GetVertex(i1) );
		const Vector p2 = ToVector( contactRegion.GetVertex(i2) );

		// Compute the normal to the plane defined by the triangle
		const Vector planeNormal = CrossProduct( p1 - p0, p2 - p0 );

		// Choose the normal corresponding to the triangle with the largest area
		// (since this is the intersection volume of a slightly expanded AABB, which
		// is only penetrating the brush by the expansion amount). Also, make sure it
		// points in the general direction of the point outside the plane.
		const Vector outVector = outsidePt - p0;
		const float areaSq = planeNormal.LengthSqr();
		if( areaSq > maxAreaSq && DotProduct( planeNormal, outVector ) > 0 )
		{
			maxAreaSq = areaSq;
			contactNormal = planeNormal;
			p0VertexIndex = i0;
			contactTriangleIndex = i;
		}

		//NDebugOverlay::Triangle( p0, p1, p2, 255, 0, 0, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p0, p0 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p1, p1 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p2, p2 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
	}

	// Normalize the normal
	const bool isValid = maxAreaSq > 0;
	contactNormal *= isValid ? 1.0f / sqrt( maxAreaSq ) : 1.0f;

	// Fill out the plane
	plane.normal = contactNormal;
	plane.dist = DotProduct( contactNormal, ToVector( contactRegion.GetVertex(p0VertexIndex) ) );

	return isValid;
}

int ComputeContactPlane_SIMD( fltx4& plane, const CMesh& contactRegion, const fltx4& outsidePt )
{
	fltx4 contactNormal = g_SIMD_Identity[2];		//	( 0, 0, 1 );
	fltx4 maxAreaSq = Four_Zeros;
	const int nTriangleCount = contactRegion.TriangleCount();
	int contactTriangleIndex = -1;
	fltx4 pReferenceVertex = Four_Zeros;

	for( int i = 0; i < nTriangleCount; ++i )
	{
		// Get the vertices of the triangle
		const int triangleIndex = i * 3;
		const int i0 = contactRegion.m_pIndices[triangleIndex];
		const int i1 = contactRegion.m_pIndices[triangleIndex + 1];
		const int i2 = contactRegion.m_pIndices[triangleIndex + 2];

		// The mesh has been created from fltx4 (aligned with a stride of 4).
		const fltx4 p0 = *(const fltx4 *)( contactRegion.GetVertex(i0) );
		const fltx4 p1 = *(const fltx4 *)( contactRegion.GetVertex(i1) );
		const fltx4 p2 = *(const fltx4 *)( contactRegion.GetVertex(i2) );

		// Compute the normal to the plane defined by the triangle
		const fltx4 planeNormal = CrossProductSIMD( p1 - p0, p2 - p0 );

		// Choose the normal corresponding to the triangle with the largest area
		// (since this is the intersection volume of a slightly expanded AABB, which
		// is only penetrating the brush by the expansion amount). Also, make sure it
		// points in the general direction of the point outside the plane.
		const fltx4 outVector = outsidePt - p0;
		const fltx4 areaSq = Dot3SIMD( planeNormal, planeNormal );
		const fltx4 dotProd = Dot3SIMD( planeNormal, outVector );
		const fltx4 f4IsMax = (fltx4)CmpLeSIMD( areaSq, maxAreaSq );			//	if ( areaSq <= maxAreaSq )
		const fltx4 f4IsDotPos = (fltx4)CmpLeSIMD( dotProd, Four_Zeros );		//	if ( DotProduct( planeNormal, outVector ) <= 0 )
		//if ( areaSq > maxAreaSq && DotProduct( planeNormal, outVector ) > 0 )	- Except we do the opposite tests and look if the result is false
		if ( IsAllEqual( OrSIMD( f4IsMax, f4IsDotPos), Four_Zeros ) )
		{
			maxAreaSq = areaSq;
			contactNormal = planeNormal;
			pReferenceVertex = p0;
			contactTriangleIndex = i;
		}

		//NDebugOverlay::Triangle( p0, p1, p2, 255, 0, 0, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p0, p0 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p1, p1 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
		//NDebugOverlay::HorzArrow( p2, p2 + 20.0f * planeNormal.Normalized(), 1, 255, 0, 255, 128, true, 0 );
	}

	if (contactTriangleIndex < 0)
	{
		return -1;
	}

	// At that point the area is greater than zero (i.e. valid)
	// Normalize the normal
	// const bool isValid = maxAreaSq > 0;
	// contactNormal *= isValid ? 1.0f / sqrt( maxAreaSq ) : 1.0f;
	contactNormal = MulSIMD( contactNormal, ReciprocalSqrtSIMD( maxAreaSq ) );

	// Fill out the plane
	// plane.normal = contactNormal;
	// plane.dist = DotProduct( contactNormal, ToVector( contactRegion.GetVertex(p0VertexIndex) ) );
	fltx4 f4Dist = Dot3SIMD( contactNormal, pReferenceVertex );
	plane = SetWSIMD( contactNormal, f4Dist );

	return contactTriangleIndex;
}

void ComputeAABBContactsWithBrushEntity( ContactVector& contacts, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask )
{
	ComputeAABBContactsWithBrushEntity( contacts, NULL, 0, boxOrigin, boxMin, boxMax, pBrushEntity, contentsMask );
}

void ComputeAABBContactsWithBrushEntity_Old( ContactVector& contacts, const cplane_t *pClipPlanes, int iClipPlaneCount, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask)
{
	//typedef CUtlVector<int>	BrushIndexVector;
	typedef CUtlVector<uint32> PlaneIndexVector;
	//typedef CUtlVector<BrushSideInfo_t> BrushSideInfoVector;
	//typedef BrushSideInfo_t* BrushSideInfoIterator;
	// Get the collision model index of the brush entity
	AssertMsg( pBrushEntity->IsBSPModel(), "Your brush entity is not a brush entity." );
	ICollideable* pCollideable = enginetrace->GetCollideable( pBrushEntity );
	const int cmodelIndex = pCollideable->GetCollisionModelIndex() - 1;
	AssertMsg( !pBrushEntity->IsWorld() || cmodelIndex == 0, "World collision model index should be 0." );
	const matrix3x4_t& entityToWorld = pBrushEntity->EntityToWorldTransform();

	// The query box must be in local space for non-world brush entities
	Vector queryBoxMin = boxMin;
	Vector queryBoxMax = boxMax;
	if( !pBrushEntity->IsWorld() )
	{
		ITransformAABB( entityToWorld, boxMin, boxMax, queryBoxMin, queryBoxMax );
	}

	// Get the indices of all the colliding brushes
	//BrushIndexVector brushIndices;
	CBrushQuery brushQuery;
	enginetrace->GetBrushesInAABB( queryBoxMin, queryBoxMax, brushQuery, contentsMask, cmodelIndex );

	// Find the contact regions
	//BrushSideInfoVector brushSides;
	BrushSideInfo_t *brushSides = (BrushSideInfo_t *)stackalloc( sizeof( BrushSideInfo_t ) * brushQuery.MaxBrushSides() );
	//PlaneVector planes;
	Vector4D *planes = (Vector4D *)stackalloc( sizeof( Vector4D ) * (brushQuery.MaxBrushSides() + 6 /*bbox*/ + iClipPlaneCount) );
	for( int i = 0; i < brushQuery.Count(); ++i )
	{
		// Get the brush side info
		int iBrushContents;
		int iNumBrushSides = enginetrace->GetBrushInfo( brushQuery[i], iBrushContents, brushSides, brushQuery.MaxBrushSides() );
		Assert( iNumBrushSides > 0 );
		if( iNumBrushSides <= 0 )
			continue;

		//remove bevel planes
		{
			int iWriteIndex = 0;
			for( int sideIndex = 0; sideIndex < iNumBrushSides; ++sideIndex )
			{
				if( brushSides[sideIndex].bevel != 0 )
					continue;

				brushSides[iWriteIndex] = brushSides[sideIndex];
				++iWriteIndex;
			}
			iNumBrushSides = iWriteIndex;
		}

		// Transform the planes to world space
		for( int sideIndex = 0; sideIndex < iNumBrushSides; ++sideIndex )
		{
			cplane_t temp;
			MatrixTransformPlane( entityToWorld, brushSides[sideIndex].plane, temp );
			planes[sideIndex] = Vector4D( temp.normal.x, temp.normal.y, temp.normal.z, temp.dist );
		}

		int iPlaneCount = iNumBrushSides;

		// Add the box planes
		AddBboxToPlaneList( &planes[iNumBrushSides], boxMin, boxMax );
		iPlaneCount += 6;

		// Add the clip planes
		for( int j = 0; j < iClipPlaneCount; ++j )
		{
			const cplane_t& plane = pClipPlanes[j];
			planes[iPlaneCount] = Vector4D( -plane.normal.x, -plane.normal.y, -plane.normal.z, -plane.dist );
			++iPlaneCount;
		}

		Assert( iPlaneCount <= (brushQuery.MaxBrushSides() + 6 + iClipPlaneCount) );

		// Compute the contact region
		CMesh contactRegion;
		PlaneIndexVector trianglePlaneIndices;
		HullFromPlanes( &contactRegion, &trianglePlaneIndices, reinterpret_cast<const float*>( planes ), iPlaneCount, 4 );
		//DebugDrawMesh( contactRegion );

		// If the contact region exists
		if( contactRegion.m_nVertexCount > 0 )
		{
			// Compute the contact point and normal
			cplane_t plane;
			int triangleIndex;
			if( ComputeContactPlane( plane, triangleIndex, contactRegion, boxOrigin ) )
			{
				const Vector centroid = ComputeCentroid( contactRegion );
				const float dist = DotProduct( centroid, plane.normal );
				const float diff = dist - plane.dist;
				const Vector contactPt = centroid - diff * plane.normal;

				// Figure out if the contact is on a thin surface
				const int planeIndex = trianglePlaneIndices[triangleIndex];
				const bool isOnThinSurface = planeIndex < iNumBrushSides ? brushSides[planeIndex].thin != 0 : false;

				BrushContact contact;
				contact.Initialize( contactPt, plane.normal, pBrushEntity, isOnThinSurface );
				contacts.AddToTail( contact );

				if( sv_debug_draw_contacts.GetInt() == 1 )
				{
					Color color( contact.isOnThinSurface ? 0 : 255, 255, 255 );
					NDebugOverlay::Sphere( contact.point, 5.0f, color.r(), color.g(), color.b(), true, 0 );
					NDebugOverlay::Line( contact.point, contact.point + 20.0f * contact.normal, color.r(), color.g(), color.b(), true, 0 );
				}
				//DevMsg( "Contact: (%f, %f, %f)\n", XYZ( contact.point ) );
				//DevMsg( "Normal: (%f, %f, %f)\n", XYZ( contact.normal ) );
			}
		}
	}
}

void ComputeAABBContactsWithBrushEntity_SIMD( ContactVector& contacts, const cplane_t *pClipPlanes, int iClipPlaneCount, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask)
{
	//typedef CUtlVector<int>	BrushIndexVector;
	typedef CUtlVector<uint16> PlaneIndexVector;
	//typedef CUtlVector<BrushSideInfo_t> BrushSideInfoVector;
	//typedef BrushSideInfo_t* BrushSideInfoIterator;
	// Get the collision model index of the brush entity
	AssertMsg( pBrushEntity->IsBSPModel(), "Your brush entity is not a brush entity." );
	ICollideable* pCollideable = enginetrace->GetCollideable( pBrushEntity );
	const int cmodelIndex = pCollideable->GetCollisionModelIndex() - 1;
	AssertMsg( !pBrushEntity->IsWorld() || cmodelIndex == 0, "World collision model index should be 0." );
	const matrix3x4_t& entityToWorld = pBrushEntity->EntityToWorldTransform();

	// The query box must be in local space for non-world brush entities
	Vector queryBoxMin = boxMin;
	Vector queryBoxMax = boxMax;
	if( !pBrushEntity->IsWorld() )
	{
		ITransformAABB( entityToWorld, boxMin, boxMax, queryBoxMin, queryBoxMax );
	}

	// Get the indices of all the colliding brushes
	//BrushIndexVector brushIndices;
	CBrushQuery brushQuery;
	enginetrace->GetBrushesInAABB( queryBoxMin, queryBoxMax, brushQuery, contentsMask, cmodelIndex );

	// Find the contact regions
	//BrushSideInfoVector brushSides;
	BrushSideInfo_t *brushSides = (BrushSideInfo_t *)stackalloc( sizeof( BrushSideInfo_t ) * brushQuery.MaxBrushSides() );
	//PlaneVector planes;
	const int NUMBER_OF_FLTX4 = brushQuery.MaxBrushSides() + 6 /*bbox*/ + iClipPlaneCount;
	fltx4 *planes = (fltx4 *)stackalloc( sizeof( fltx4 ) * ( NUMBER_OF_FLTX4 + 1 ) );		// +1 for VMX alignment
	planes = (fltx4*)ALIGN_VALUE( (int)planes, sizeof(fltx4) );

	fltx4 f4BoxMin = LoadUnalignedSIMD( &boxMin.x );
	fltx4 f4BoxMax = LoadUnalignedSIMD( &boxMax.x );
	fltx4 f4BoxOrigin = LoadUnalignedSIMD( &boxOrigin.x );

	for( int i = 0; i < brushQuery.Count(); ++i )
	{
		// Get the brush side info
		int iBrushContents;
		int iNumBrushSides = enginetrace->GetBrushInfo( brushQuery[i], iBrushContents, brushSides, brushQuery.MaxBrushSides() );
		Assert( iNumBrushSides > 0 );
		if( iNumBrushSides <= 0 )
			continue;

		//remove bevel planes
		{
			int iWriteIndex = 0;
			for( int sideIndex = 0; sideIndex < iNumBrushSides; ++sideIndex )
			{
				if( brushSides[sideIndex].bevel != 0 )
					continue;

				brushSides[iWriteIndex] = brushSides[sideIndex];
				++iWriteIndex;
			}
			iNumBrushSides = iWriteIndex;
		}

		// Transform the planes to world space
		for( int sideIndex = 0; sideIndex < iNumBrushSides; ++sideIndex )
		{
			cplane_t temp;
			MatrixTransformPlane( entityToWorld, brushSides[sideIndex].plane, temp );		// Could be optimized further here...
			planes[sideIndex] = LoadUnalignedSIMD(&temp.normal);		// Read XYZ and dist of the plane
		}

		int iPlaneCount = iNumBrushSides;

		// Add the box planes
		AddBboxToPlaneList_SIMD( &planes[iNumBrushSides], f4BoxMin, f4BoxMax );
		iPlaneCount += 6;

		// Add the clip planes
		for( int j = 0; j < iClipPlaneCount; ++j )
		{
			const cplane_t& plane = pClipPlanes[j];
			planes[iPlaneCount] = -LoadUnalignedSIMD(&plane.normal);
			++iPlaneCount;
		}

		Assert( iPlaneCount <= NUMBER_OF_FLTX4 );

		// Compute the contact region
		CMesh contactRegion;
		PlaneIndexVector trianglePlaneIndices;
		HullFromPlanes_SIMD( &contactRegion, &trianglePlaneIndices, planes, iPlaneCount );
		//DebugDrawMesh( contactRegion );

		// If the contact region exists
		if( contactRegion.m_nVertexCount > 0 )
		{
			// Compute the contact point and normal
			fltx4 plane;
			int nTriangleIndex = ComputeContactPlane_SIMD( plane, contactRegion, f4BoxOrigin );
#if _DEBUG
			cplane_t slowPlane;
			int slowTriangleIndex;
			bool bSlowValid = ComputeContactPlane( slowPlane, slowTriangleIndex, contactRegion, boxOrigin );
			Assert( (nTriangleIndex >= 0) == bSlowValid );
			// For optimization reasons, we changed the API a bit.
			if (nTriangleIndex >= 0)
			{
				Assert( nTriangleIndex == slowTriangleIndex );

				// Because ComputeContactPlane_SIMD() does a square root reciprocal, there is some error involved that will slightly create divergence in the results.
				Assert( fabs( SubFloat( plane, 0 ) - slowPlane.normal.x ) < 0.001f );
				Assert( fabs( SubFloat( plane, 1 ) - slowPlane.normal.y ) < 0.001f );
				Assert( fabs( SubFloat( plane, 2 ) - slowPlane.normal.z ) < 0.001f );
				Assert( fabs( SubFloat( plane, 3 ) - slowPlane.dist ) < 0.001f );
			}
			else
			{
				Assert( slowTriangleIndex == contactRegion.TriangleCount() );
			}
#endif
			if ( nTriangleIndex >= 0 )
			{
				const fltx4 centroid = ComputeCentroid_SIMD( contactRegion );
				const fltx4 dist = Dot3SIMD( centroid, plane );
				const fltx4 diff = dist - SplatWSIMD( plane );
				const fltx4 contactPt = MsubSIMD(diff, plane, centroid);		//	centroid - diff * plane.normal;

				// Figure out if the contact is on a thin surface
				const int planeIndex = trianglePlaneIndices[nTriangleIndex];
				const bool isOnThinSurface = planeIndex < iNumBrushSides ? brushSides[planeIndex].thin != 0 : false;

				BrushContact contact;
				contact.Initialize( contactPt, plane, pBrushEntity, isOnThinSurface );
				contacts.AddToTail( contact );

				//DevMsg( "Contact: (%f, %f, %f)\n", XYZ( contact.point ) );
				//DevMsg( "Normal: (%f, %f, %f)\n", XYZ( contact.normal ) );
			}
		}
	}

	// Do the test only once instead of for every contacts. This is debug code after all.
	if( sv_debug_draw_contacts.GetInt() == 1 )
	{
		for ( int i  = 0 ; i < contacts.Count() ; ++i )
		{
			const BrushContact & contact = contacts[i];
			Color color( contact.isOnThinSurface ? 0 : 255, 255, 255 );
			NDebugOverlay::Sphere( contact.point, 5.0f, color.r(), color.g(), color.b(), true, 0 );
			NDebugOverlay::Line( contact.point, contact.point + 20.0f * contact.normal, color.r(), color.g(), color.b(), true, 0 );
		}
	}
}

void ComputeAABBContactsWithBrushEntity( ContactVector& contacts, const cplane_t *pClipPlanes, int iClipPlaneCount, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask)
{
	if ( paint_compute_contacts_simd.GetBool() )
	{
		ComputeAABBContactsWithBrushEntity_SIMD( contacts, pClipPlanes, iClipPlaneCount, boxOrigin, boxMin, boxMax, pBrushEntity, contentsMask );
#if _DEBUG
		ContactVector fpuContacts;
		ComputeAABBContactsWithBrushEntity_Old( fpuContacts, pClipPlanes, iClipPlaneCount, boxOrigin, boxMin, boxMax, pBrushEntity, contentsMask );

		// Now check that the official version and the SIMD version give the same results.
		Assert( contacts.Count() == fpuContacts.Count() );
		for (int i = 0 ; i < contacts.Count() ; ++i )
		{
			// Because there are some reciprocal called in the SIMD version, there is a bit of inaccuracy in the results.
			Assert( contacts[i].point.DistTo(fpuContacts[i].point) < 0.01 );
			Assert( contacts[i].normal.DistTo(fpuContacts[i].normal) < 0.01 );
			Assert( contacts[i].pBrushEntity == fpuContacts[i].pBrushEntity );
			Assert( contacts[i].isOnThinSurface == fpuContacts[i].isOnThinSurface );
		}
#endif
	}
	else
	{
		ComputeAABBContactsWithBrushEntity_Old( contacts, pClipPlanes, iClipPlaneCount, boxOrigin, boxMin, boxMax, pBrushEntity, contentsMask );
	}
}

void TracePlayerBoxAgainstCollidables( trace_t& trace,
									  const CPortal_Player* player,
									  const Vector& startPos,
									  const Vector& endPos,
									  const Vector& boxLocalMin,
									  const Vector& boxLocalMax )
{
#ifdef CLIENT_DLL
	CTraceFilterSimpleClassnameList traceFilter( player, COLLISION_GROUP_PLAYER_MOVEMENT );
	traceFilter.AddClassnameToIgnore("prop_weighted_cube");
#else
	CTraceFilterSimpleClassnameList baseFilter( player, COLLISION_GROUP_PLAYER_MOVEMENT );
	baseFilter.AddClassnameToIgnore( "prop_weighted_cube" );
	baseFilter.AddClassnameToIgnore( "prop_physics_paintable" );
	CTraceFilterTranslateClones traceFilter( &baseFilter );
#endif

	Ray_t ray;
	ray.Init( startPos, endPos, boxLocalMin, boxLocalMax );
	UTIL_ClearTrace( trace );
	UTIL_Portal_TraceRay_With( player->m_hPortalEnvironment, ray, MASK_PLAYERSOLID, &traceFilter, &trace, true );
}


//The weapon shoot position of the player
ConVar player_paint_shoot_pos_forward_scale( "player_paint_shoot_pos_forward_scale", "55.0f", FCVAR_REPLICATED );
ConVar player_paint_shoot_pos_right_scale( "player_paint_shoot_pos_right_scale", "12.0f", FCVAR_REPLICATED );
ConVar player_paint_shoot_pos_up_scale( "player_paint_shoot_pos_up_scale", "25.0f", FCVAR_REPLICATED );

#ifndef CLIENT_DLL
ConVar debug_player_paint_shoot_pos( "debug_player_paint_shoot_pos", "0" );
#endif

Vector CPortal_Player::GetPaintGunShootPosition()
{
	Vector vecEyePosition = EyePosition();

	Vector vecForward, vecRight, vecUp;

	EyeVectors( &vecForward, &vecRight, &vecUp );

	vecForward.NormalizeInPlace();
	vecUp.NormalizeInPlace();
	vecRight.NormalizeInPlace();

	Vector vecMuzzlePos = vecEyePosition +
		( player_paint_shoot_pos_forward_scale.GetFloat() * vecForward ) +
		( player_paint_shoot_pos_right_scale.GetFloat() * vecRight ) -
		( player_paint_shoot_pos_up_scale.GetFloat() * vecUp );

#ifndef CLIENT_DLL
	if( debug_player_paint_shoot_pos.GetBool() )
	{
		NDebugOverlay::Sphere( vecMuzzlePos, 4.0f, 255, 255, 255, false, 0 );
	}
#endif

	return vecMuzzlePos;
}


void CPortal_Player::DecayEyeOffset()
{
	const float flDecay = ExponentialDecay( 0.2f, gpGlobals->frametime );

	m_PortalLocal.m_vEyeOffset *= flDecay;	// Decay eye offset
}

void CPortal_Player::SetEyeOffset( const Vector& vOldOrigin, const Vector& vNewOrigin )
{
	const Vector vOriginOffset = vOldOrigin - vNewOrigin;
	m_PortalLocal.m_vEyeOffset = vOriginOffset;
}

#ifdef DEBUG_STICK_CAM
ConVar debug_client_cam("debug_client_cam", "0", FCVAR_REPLICATED);
#endif
Vector CPortal_Player::EyePosition()
{
	const Vector vEyeOffset = m_PortalLocal.m_vEyeOffset;
	Vector vUp = GetViewOffset();
	Vector vEyePosition = GetAbsOrigin() + vEyeOffset + vUp;

#ifdef DEBUG_STICK_CAM
#ifdef GAME_DLL
	debug_client_cam.SetValue(m_nButtons & IN_GRENADE2);
	if( m_nButtons & IN_GRENADE1 )
#else

	if( debug_client_cam.GetBool() )
#endif
	{
		// Yellow Line: Offset
		//NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + vEyeOffset, 255, 255, 0, true, 100 );
		// White Cross: Origin
		//NDebugOverlay::Cross( GetAbsOrigin() + vUp, 5, 255, 255, 255, true, 100 );
		// Red Cross: Eye Position
	//	NDebugOverlay::Cross( vEyePosition , 5, 255, 0, 0, true, 100 );
		// Teal Line: Eye offset
	//	NDebugOverlay::Line( vEyePosition, vEyePosition - vEyeOffset, 0, 255, 255, true, 100 );
		// Green Line: View offset (orign to eye)
		//NDebugOverlay::Line( GetAbsOrigin() + vEyeOffset, GetAbsOrigin() + vEyeOffset + vUp, 0, 255, 0, true, 100 ); 

	//	Vector vOriginToCenter = (GetHullMaxs() + GetHullMins()) * 0.5f;
	//	Vector vMoveCenter = GetAbsOrigin() + vOriginToCenter;
	//	NDebugOverlay::Cross( GetAbsOrigin(), 5, 255, 0, 255, true, 1000.f );
	}

	
	if( m_nButtons & IN_LOOKSPIN )
	{
		debugoverlay->ClearAllOverlays();
	}
#endif

	return vEyePosition;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// FIXME: Bring this back for DLC2
#define paintgun_ammo_type 0 //extern ConVar paintgun_ammo_type;
void CPortal_Player::ItemPostFrame()
{
	BaseClass::ItemPostFrame();

	CBaseCombatWeapon* pActiveWeapon = GetActiveWeapon();
	if( m_hUseEntity != NULL &&
		paintgun_ammo_type != PAINT_AMMO_NONE &&
		pActiveWeapon != NULL &&
		FClassnameIs( pActiveWeapon, "weapon_paintgun" ) )
	{
#ifdef CLIENT_DLL
		// Predicting this weapon
		if( pActiveWeapon->IsPredicted() )
#endif
		{
			pActiveWeapon->ItemPostFrame( );
		}
	}

#if defined( GAME_DLL )
	if ( m_bPlayUseDenySound )
	{
		m_bPlayUseDenySound = false;
		if( GetActivePortalWeapon() )
		{
			EmitSound( "PortalPlayer.UseDeny" );
		}
		else
		{
			EmitSound( "PortalPlayer.UseDenyNoGun" );
		}
	}
#endif
}

void CPortal_Player::PostThink( void )
{
	BaseClass::PostThink();

	// this was all moved from portal_player (GAME_DLL), but there are client checks in the code?  don't run it just in case
#if defined( GAME_DLL )
	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );

	UpdatePortalPlaneSounds();
	UpdateWooshSounds();

	m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	UpdateSecondsTaken();

	// Try to fix the player if they're stuck
	if ( m_bStuckOnPortalCollisionObject )
	{
		Vector vForward = ((CProp_Portal*)m_hPortalEnvironment.Get())->m_vPrevForward;
		Vector vNewPos = GetAbsOrigin() + vForward * gpGlobals->frametime * -1000.0f;
		Teleport( &vNewPos, NULL, &vForward );
		m_bStuckOnPortalCollisionObject = false;
	}

	// Try to update our crosshair
	m_bIsHoldingSomething = IsHoldingEntity( NULL );

	IncrementDistanceTaken();
	
#endif //GAME_DLL

	// Reset our jumping status
	m_bJumpWasPressedWhenForced = false;

	// Reset whether we prevented crouch jump
	m_PortalLocal.m_bPreventedCrouchJumpThisFrame = false;

	// Update in air state
	UpdateInAirState();

#if defined( CLIENT_DLL )
	engine->GetViewAngles( pl.v_angle );
#else
	if( pl.fixangle == FIXANGLE_NONE )
#endif
	{
		Reorient( pl.v_angle );
	}	

#if defined( CLIENT_DLL )
	engine->SetViewAngles( pl.v_angle );
#else
	SwapThink();
#endif
}

void CPortal_Player::SetAirDuck( bool bDuckedInAir )
{
	m_PortalLocal.m_bDuckedInAir = bDuckedInAir;
}


void CPortal_Player::UnDuck( void )
{
#ifndef CLIENT_DLL
	FirePlayerProxyOutput( "OnUnDuck", variant_t(), this, this );
#endif
}

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA_NO_BASE( CPortalPlayerShared )
	DEFINE_PRED_FIELD( m_nPlayerCond, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif // CLIENT_DLL

CPortalPlayerShared::CPortalPlayerShared()
{
}

void CPortalPlayerShared::Init( CPortal_Player *pPlayer )
{
	m_pOuter = pPlayer;
	m_bLoadoutUnavailable = false;
}


//-----------------------------------------------------------------------------
// Purpose: Add a condition and duration
// duration of PERMANENT_CONDITION means infinite duration
//-----------------------------------------------------------------------------
void CPortalPlayerShared::AddCond( int nCond, float flDuration /* = PERMANENT_CONDITION */ )
{
	Assert( nCond >= 0 && nCond < PORTAL_COND_LAST );
	m_nPlayerCond |= (1<<nCond);
	m_flCondExpireTimeLeft[nCond] = flDuration;
	OnConditionAdded( nCond );
}

//-----------------------------------------------------------------------------
// Purpose: Forcibly remove a condition
//-----------------------------------------------------------------------------
void CPortalPlayerShared::RemoveCond( int nCond )
{
	Assert( nCond >= 0 && nCond < PORTAL_COND_LAST );

	m_nPlayerCond &= ~(1<<nCond);
	m_flCondExpireTimeLeft[nCond] = 0;

	OnConditionRemoved( nCond );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CPortalPlayerShared::InCond( int nCond )
{
	Assert( nCond >= 0 && nCond < PORTAL_COND_LAST );

	return ( ( m_nPlayerCond & (1<<nCond) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CPortalPlayerShared::GetConditionDuration( int nCond )
{
	Assert( nCond >= 0 && nCond < PORTAL_COND_LAST );

	if ( InCond( nCond ) )
	{
		return m_flCondExpireTimeLeft[nCond];
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortalPlayerShared::DebugPrintConditions( void )
{
	const char *szDll;
#ifndef CLIENT_DLL
	szDll = "Server";
#else
	szDll = "Client";
#endif

	Msg( "( %s ) Conditions for player ( %d )\n", szDll, m_pOuter->entindex() );

	int i;
	int iNumFound = 0;
	for ( i=0;i<PORTAL_COND_LAST;i++ )
	{
		if ( m_nPlayerCond & (1<<i) )
		{
			if ( m_flCondExpireTimeLeft[i] == PERMANENT_CONDITION )
			{
				Msg( "( %s ) Condition %d - ( permanent cond )\n", szDll, i );
			}
			else
			{
				Msg( "( %s ) Condition %d - ( %.1f left )\n", szDll, i, m_flCondExpireTimeLeft[i] );
			}

			iNumFound++;
		}
	}

	if ( iNumFound == 0 )
	{
		Msg( "( %s ) No active conditions\n", szDll );
	}
}

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortalPlayerShared::OnPreDataChanged( void )
{
	m_nOldConditions = m_nPlayerCond;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortalPlayerShared::OnDataChanged( void )
{
	// Update conditions from last network change
	if ( m_nOldConditions != m_nPlayerCond )
	{
		UpdateConditions();

		m_nOldConditions = m_nPlayerCond;
	}	
}

//-----------------------------------------------------------------------------
// Purpose: check the newly networked conditions for changes
//-----------------------------------------------------------------------------
void CPortalPlayerShared::UpdateConditions( void )
{
	int nCondChanged = m_nPlayerCond ^ m_nOldConditions;
	int nCondAdded = nCondChanged & m_nPlayerCond;
	int nCondRemoved = nCondChanged & m_nOldConditions;

	int i;
	for ( i=0;i<PORTAL_COND_LAST;i++ )
	{
		if ( nCondAdded & (1<<i) )
		{
			OnConditionAdded( i );
		}
		else if ( nCondRemoved & (1<<i) )
		{
			OnConditionRemoved( i );
		}
	}
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Remove any conditions affecting players
//-----------------------------------------------------------------------------
void CPortalPlayerShared::RemoveAllCond()
{
	int i;
	for ( i=0;i<PORTAL_COND_LAST;i++ )
	{
		if ( m_nPlayerCond & (1<<i) )
		{
			RemoveCond( i );
		}
	}

	// Now remove all the rest
	m_nPlayerCond = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Called on both client and server. Server when we add the bit,
// and client when it receives the new cond bits and finds one added
//-----------------------------------------------------------------------------
void CPortalPlayerShared::OnConditionAdded( int nCond )
{
	float flDuration = 0.0f;

	switch( nCond )
	{
	case PORTAL_COND_TAUNTING:
		{
			// The reason m_bTauntRemoteView isn't evaluated here is because it is evaluated at the start of a normal taunt only in StartTaunt()
			// None of the other conditions go through the start taunt function when they are set
			// a good thing to do would be to make all conditions that make IsTaunting() true, go through the same code path
		}
		break;
	case PORTAL_COND_DEATH_GIB:
		{
			if ( m_pOuter )
			{
				flDuration = 3.5f; // change this to animation length
				m_pOuter->m_fTauntCameraDistance = portal_deathcam_dist.GetFloat();

				// if the player is not already taunting, set the remote view to false so when we die, we don't get residual data (because it's only evaluated at the start of a normal taunt on the server)
				if ( !InCond( PORTAL_COND_TAUNTING ) )
				{
					m_pOuter->m_bTauntRemoteView = false;
				}
			}
		}
		break;
	case PORTAL_COND_DEATH_CRUSH:
		{
			if ( m_pOuter )
			{
				m_pOuter->DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE, ACT_MP_DEATH_CRUSH );
				flDuration = 3.0f; // change this to animation length
				m_pOuter->m_fTauntCameraDistance = portal_deathcam_dist.GetFloat();

				// if the player is not already taunting, set the remote view to false so when we die, we don't get residual data (because it's only evaluated at the start of a normal taunt on the server)
				if ( !InCond( PORTAL_COND_TAUNTING ) )
				{
					m_pOuter->m_bTauntRemoteView = false;
				}
			}
		}
		break;
	case PORTAL_COND_DROWNING:
		{
			if ( m_pOuter )
			{
				// if the player is not already taunting, set the remote view to false so when we die, we don't get residual data (because it's only evaluated at the start of a normal taunt on the server)
				if ( !InCond( PORTAL_COND_TAUNTING ) )
				{
					m_pOuter->m_bTauntRemoteView = false;
				}

				m_pOuter->DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE, ACT_MP_DROWNING_PRIMARY );
				flDuration = 3.5f; // change this to animation length
				m_pOuter->m_fTauntCameraDistance = portal_deathcam_dist.GetFloat();
			}
		}
		break;
	case PORTAL_COND_POINTING:
		{
			m_pOuter->DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE, ACT_MP_GESTURE_VC_FINGERPOINT_PRIMARY );
		}
		break;
	default:
		break;
	}
	
#ifdef GAME_DLL
	// assign how long the taunt should last manually
	if ( flDuration > 0.0f )
	{
		m_flTauntRemoveTime = gpGlobals->curtime + flDuration;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Called on both client and server. Server when we remove the bit,
// and client when it receives the new cond bits and finds one removed
//-----------------------------------------------------------------------------
void CPortalPlayerShared::OnConditionRemoved( int nCond )
{
	switch( nCond )
	{
	case PORTAL_COND_TAUNTING:
		{
			if ( m_pOuter )
			{
#ifdef GAME_DLL
				if ( m_pOuter->m_hRemoteTauntCamera.Get() )
				{
					m_pOuter->m_hRemoteTauntCamera.Get()->TauntedByPlayerFinished( m_pOuter );
					m_pOuter->m_hRemoteTauntCamera = NULL;
				}
#endif
				// UNDONE: Do NOT set to false after removing the taunt condition!
				// We still need to have this state true when it turns off the taunt camera on the client!
				//m_bTauntRemoteView = false;
			}
		}
		break;
	case PORTAL_COND_DROWNING:
		{
			if ( m_pOuter )
			{
#ifdef GAME_DLL
				if ( m_pOuter->m_hRemoteTauntCamera.Get() )
				{
					m_pOuter->m_hRemoteTauntCamera.Get()->TauntedByPlayerFinished( m_pOuter );
					m_pOuter->m_hRemoteTauntCamera = NULL;
				}
#endif

				m_pOuter->m_bTauntRemoteView = false;
			}	
		}
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Runs SERVER SIDE only Condition Think
// If a player needs something to be updated no matter what do it here (invul, etc).
//-----------------------------------------------------------------------------
void CPortalPlayerShared::ConditionGameRulesThink( void )
{
#ifdef GAME_DLL
	int i;
	for ( i=0;i<PORTAL_COND_LAST;i++ )
	{
		if ( m_nPlayerCond & (1<<i) )
		{
			// Ignore permanent conditions
			if ( m_flCondExpireTimeLeft[i] != PERMANENT_CONDITION )
			{
				float flReduction = gpGlobals->frametime;

				m_flCondExpireTimeLeft[i] = MAX( m_flCondExpireTimeLeft[i] - flReduction, 0 );

				if ( m_flCondExpireTimeLeft[i] == 0 )
				{
					RemoveCond( i );
				}
			}
		}
	}

	// Taunt
	if ( InCond( PORTAL_COND_TAUNTING ) )
	{
		if ( gpGlobals->curtime > m_flTauntRemoveTime )
		{
			RemoveCond( PORTAL_COND_TAUNTING );
			m_pOuter->SetTeamTauntState( TEAM_TAUNT_NONE );

			// HEY JEEP: Should this be at the start or end of a taunt? Could do remove time / 2 to be more fair?
			// Increment the air taunt count if the player has no ground entity
			if ( m_pOuter->GetGroundEntity() == NULL )
			{
				m_pOuter->m_nAirTauntCount++;
				// Award 'With Style' if they reach two air taunts without getting a ground entity set.
				if ( m_pOuter->m_nAirTauntCount >= 2 )
				{
					UTIL_RecordAchievementEvent( "ACH.WITH_STYLE", m_pOuter );
				}
			}
		}
	}

	if ( GameRules()->IsMultiplayer() )
	{
		// Death Crush
		if ( InCond( PORTAL_COND_DEATH_CRUSH ) )
		{
			if ( gpGlobals->curtime > m_flTauntRemoveTime )
			{	
				Vector vecOrigin = m_pOuter->GetAbsOrigin();
				CPVSFilter filter( vecOrigin );
				for ( int i = 0; i < 4; i++ )
				{
					Vector gibVelocity = RandomVector(-100,100);
					int iModelIndex = modelinfo->GetModelIndex( g_PropDataSystem.GetRandomChunkModel( "MetalChunks" ) );
					float flPropLifeTime = 20.f;
					te->BreakModel( filter, 0.0, vecOrigin, m_pOuter->GetAbsAngles(), Vector(40,40,40), gibVelocity, iModelIndex, 150, 4, flPropLifeTime, BREAK_METAL );
				}

				ExplosionCreate( m_pOuter->WorldSpaceCenter(), vec3_angle, m_pOuter, 500, 300.f, 
					SF_ENVEXPLOSION_NODAMAGE | SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS|
					SF_ENVEXPLOSION_NOSMOKE  | SF_ENVEXPLOSION_NOFIREBALLSMOKE, 0 );
				RemoveCond( PORTAL_COND_DEATH_CRUSH );
			}
		}

		// Death Gib
		if ( InCond( PORTAL_COND_DEATH_GIB ) )
		{
			if ( gpGlobals->curtime > m_flTauntRemoveTime )
			{	
				RemoveCond( PORTAL_COND_DEATH_GIB );
			}
		}
		if ( InCond( PORTAL_COND_DROWNING ) )
		{
			if ( gpGlobals->curtime > m_flTauntRemoveTime )
			{	
				RemoveCond( PORTAL_COND_DROWNING );
			}
		}
	}
#endif
}


void CPortal_Player::SetInTractorBeam( CTrigger_TractorBeam *pTractorBeam )
{
	if ( !pTractorBeam )
		return;

	CTrigger_TractorBeam *pNewPrimaryTractorBeam = pTractorBeam;

	// Special check if we're already in a tbeam
	if ( m_PortalLocal.m_nTractorBeamCount > 0 )
	{
		CTrigger_TractorBeam *pOldTractorBeam = m_PortalLocal.m_hTractorBeam.Get();
		if ( pOldTractorBeam )
		{			
			CPortal_Base2D *pPortal = m_hPortalEnvironment.Get();
			if ( pPortal && UTIL_Portal_EntityIsInPortalHole( pPortal, this ) )
			{
				// If we're in a portal that points the opposite way, don't take this as the primary tbeam
				if ( pPortal->m_plane_Origin.normal.Dot( pTractorBeam->GetForceDirection() ) < -0.95f )
				{
					pNewPrimaryTractorBeam = pOldTractorBeam;
				}	
			}
			else if ( pOldTractorBeam->GetForceDirection().Dot( pTractorBeam->GetForceDirection() ) < -0.95f )
			{
				// If they new is opposite the old, don't take this as the primary tbeam
				pNewPrimaryTractorBeam = pOldTractorBeam;
			}
		}
	}

	m_PortalLocal.m_hTractorBeam = pNewPrimaryTractorBeam;
	m_PortalLocal.m_nTractorBeamCount++;

	m_Local.m_bSlowMovement = true;
	m_Local.m_fTBeamEndTime = 0.0f;
	SetGravity( FLT_MIN );

#ifdef GAME_DLL
	triggerevent_t event;
	if ( PhysGetTriggerEvent( &event, pTractorBeam ) && event.pObject )
	{
		// these all get done again on save/load, so check
		event.pObject->EnableGravity( false );
	}
#endif
}

void CPortal_Player::SetLeaveTractorBeam( CTrigger_TractorBeam *pTractorBeam, bool bKeepFloating )
{
	if ( !pTractorBeam || m_PortalLocal.m_hTractorBeam.Get() == pTractorBeam )
	{
		m_PortalLocal.m_hTractorBeam = NULL;
	}

	m_PortalLocal.m_nTractorBeamCount--;

	Assert( m_PortalLocal.m_nTractorBeamCount >= 0 );

	if ( m_PortalLocal.m_nTractorBeamCount <= 0 )
	{
		m_PortalLocal.m_nTractorBeamCount = 0;

		// Don't turn off gravity if we're going through a portal that the tbeam is also going through
		if ( !bKeepFloating )
		{
			m_Local.m_bSlowMovement = false;
			SetGravity( 1.0f );

#ifdef GAME_DLL
			triggerevent_t event;
			if ( PhysGetTriggerEvent( &event, pTractorBeam ) && event.pObject )
			{
				event.pObject->EnableGravity( true );
			}
#endif
		}
		
		m_Local.m_fTBeamEndTime = gpGlobals->curtime;
	}
	else if ( m_PortalLocal.m_hTractorBeam == NULL )
	{
		// We're probably touching another TBeam, lets find it
		Ray_t ray;
		ray.Init( GetAbsOrigin(), GetAbsOrigin(), GetPlayerMins(), GetPlayerMaxs() );

		for ( int i = 0; i < ITriggerTractorBeamAutoList::AutoList().Count(); ++i )
		{
			CTrigger_TractorBeam *pBeam = static_cast< CTrigger_TractorBeam* >( ITriggerTractorBeamAutoList::AutoList()[ i ] );
			if ( pBeam == pTractorBeam )
				continue;

			trace_t tr;
			enginetrace->ClipRayToEntity( ray, MASK_SHOT, pBeam, &tr );

			if ( tr.startsolid || ( tr.fraction < 1.0f && tr.m_pEnt == pTractorBeam ) )
			{
				 m_PortalLocal.m_hTractorBeam = pBeam;
				 break;
			}
		}
	}
}


void CPortal_Player::SetHullHeight( float flHeight )
{
	m_flHullHeight = flHeight;
}


bool CPortal_Player::IsTaunting()
{
	return ( m_Shared.InCond( PORTAL_COND_TAUNTING ) ||
			 m_Shared.InCond( PORTAL_COND_DROWNING ) ||
			 m_Shared.InCond( PORTAL_COND_DEATH_CRUSH ) ||
			 m_Shared.InCond( PORTAL_COND_DEATH_GIB ) );
}

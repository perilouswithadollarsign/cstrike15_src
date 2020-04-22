//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =====//
//
// Purpose: Client-side CBasePlayer.
//
//			- Manages the player's flashlight effect.
//
//===========================================================================//
#include "cbase.h"
#include "c_baseplayer.h"
#include "c_user_message_register.h"
#include "flashlighteffect.h"
#include "weapon_selection.h"
#include "history_resource.h"
#include "iinput.h"
#include "input.h"
#include "ammodef.h"
#include "view.h"
#include "iviewrender.h"
#include "iclientmode.h"
#include "in_buttons.h"
#include "engine/IEngineSound.h"
#include "c_soundscape.h"
#include "usercmd.h"
#include "c_playerresource.h"
#include "iclientvehicle.h"
#include "view_shared.h"
#include "movevars_shared.h"
#include "prediction.h"
#include "tier0/vprof.h"
#include "filesystem.h"
#include "bitbuf.h"
#include "keyvalues.h"
#include "particles_simple.h"
#include "fx_water.h"
#include "hltvcamera.h"
#include "hltvreplaysystem.h"
#include "netmessages.h"
#if defined( REPLAY_ENABLED )
#include "replaycamera.h"
#endif
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "view_scene.h"
#include "c_vguiscreen.h"
#include "datacache/imdlcache.h"
#include "vgui/ISurface.h"
#include "voice_status.h"
#include "fx.h"
#include "cellcoord.h"
#include "vphysics/player_controller.h"
#include "debugoverlay_shared.h"
#include "iclient.h"
#include "steam/steam_api.h"

#include "platforminputdevice.h"
#include "inputsystem/iinputsystem.h"

#if defined( INCLUDE_SCALEFORM ) && defined( CSTRIKE_DLL )
#include "HUD/sfweaponselection.h"
#include "Scaleform/HUD/sfhudfreezepanel.h"
#include "cs_weapon_parse.h"
#endif

#ifdef DEMOPOLISH_ENABLED
#include "demo_polish/demo_polish.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Don't alias here
#if defined( CBasePlayer )
#undef CBasePlayer	
#endif

int g_nKillCamMode = OBS_MODE_NONE;
int g_nKillCamTarget1 = 0;
int g_nKillCamTarget2 = 0;

extern ConVar mp_forcecamera; // in gamevars_shared.h
extern ConVar r_mapextents;
extern ConVar voice_icons_method;
extern ConVar view_recoil_tracking;



#define FLASHLIGHT_DISTANCE		1000
#define MAX_VGUI_INPUT_MODE_SPEED 30
#define MAX_VGUI_INPUT_MODE_SPEED_SQ (MAX_VGUI_INPUT_MODE_SPEED*MAX_VGUI_INPUT_MODE_SPEED)

static Vector WALL_MIN(-WALL_OFFSET,-WALL_OFFSET,-WALL_OFFSET);
static Vector WALL_MAX(WALL_OFFSET,WALL_OFFSET,WALL_OFFSET);

bool CommentaryModeShouldSwallowInput( C_BasePlayer *pPlayer );

extern ConVar default_fov;
extern ConVar sensitivity;
extern ConVar voice_all_icons;

static C_BasePlayer *s_pLocalPlayer[ MAX_SPLITSCREEN_PLAYERS ];

static ConVar	cl_customsounds ( "cl_customsounds", "0", 0, "Enable customized player sound playback" );
static ConVar	spec_track		( "spec_track", "0", 0, "Tracks an entity in spec mode" );
static ConVar	cl_smooth		( "cl_smooth", "1", 0, "Smooth view/eye origin after prediction errors" );
static ConVar	cl_smoothtime	( 
	"cl_smoothtime", 
	"0.1", 
	0, 
	"Smooth client's view after prediction error over this many seconds",
	true, 0.01,	// min/max is 0.01/2.0
	true, 2.0
	 );

#ifdef CSTRIKE15
ConVar	spec_freeze_time( "spec_freeze_time", "3.0", FCVAR_RELEASE | FCVAR_REPLICATED, "Time spend frozen in observer freeze cam." );
ConVar	spec_freeze_traveltime( "spec_freeze_traveltime", "0.3", FCVAR_RELEASE | FCVAR_REPLICATED, "Time taken to zoom in to frame a target in observer freeze cam.", true, 0.01, false, 0 );
ConVar	spec_freeze_traveltime_long( "spec_freeze_traveltime_long", "0.45", FCVAR_CHEAT | FCVAR_REPLICATED, "Time taken to zoom in to frame a target in observer freeze cam when they are far away.", true, 0.01, false, 0 );
ConVar	spec_freeze_distance_min( "spec_freeze_distance_min", "60", FCVAR_CHEAT, "Minimum random distance from the target to stop when framing them in observer freeze cam." );
ConVar	spec_freeze_distance_max( "spec_freeze_distance_max", "80", FCVAR_CHEAT, "Maximum random distance from the target to stop when framing them in observer freeze cam." );
ConVar  spec_freeze_panel_extended_time( "spec_freeze_panel_extended_time", "0.0", FCVAR_RELEASE | FCVAR_REPLICATED, "Time spent with the freeze panel still up after observer freeze cam is done." );
ConVar	spec_freeze_deathanim_time( "spec_freeze_deathanim_time", "0.8", FCVAR_RELEASE | FCVAR_REPLICATED, "The time that the death cam will spend watching the player's ragdoll before going into the freeze death cam." );
ConVar	spec_freeze_target_fov_long( "spec_freeze_target_fov_long", "90", FCVAR_CHEAT | FCVAR_REPLICATED, "The target FOV that the deathcam should use when the cam zoom far away on the target." );
ConVar	spec_freeze_target_fov( "spec_freeze_target_fov", "42", FCVAR_CHEAT | FCVAR_REPLICATED, "The target FOV that the deathcam should use." );
#else
ConVar	spec_freeze_time( "spec_freeze_time", "4.0",  FCVAR_CHEAT | FCVAR_REPLICATED, "Time spend frozen in observer freeze cam." );
ConVar	spec_freeze_traveltime( "spec_freeze_traveltime", "0.4", FCVAR_CHEAT | FCVAR_REPLICATED, "Time taken to zoom in to frame a target in observer freeze cam.", true, 0.01, false, 0 );
ConVar	spec_freeze_distance_min( "spec_freeze_distance_min", "96", FCVAR_CHEAT, "Minimum random distance from the target to stop when framing them in observer freeze cam." );
ConVar	spec_freeze_distance_max( "spec_freeze_distance_max", "200", FCVAR_CHEAT, "Maximum random distance from the target to stop when framing them in observer freeze cam." );
#endif

ConVar	cl_player_fullupdate_predicted_origin_fix( "cl_player_fullupdate_predicted_origin_fix", "1" );

ConVar spec_lock_to_accountid( "spec_lock_to_accountid", "", FCVAR_RELEASE, "As an observer, lock the spectate target to the given accountid." );


bool IsDemoPolishRecording();

// -------------------------------------------------------------------------------- //
// RecvTable for CPlayerState.
// -------------------------------------------------------------------------------- //

	BEGIN_RECV_TABLE_NOBASE(CPlayerState, DT_PlayerState)
		RecvPropInt		(RECVINFO(deadflag)),
	END_RECV_TABLE()


BEGIN_RECV_TABLE_NOBASE( CPlayerLocalData, DT_Local )
	RecvPropArray3( RECVINFO_ARRAY(m_chAreaBits), RecvPropInt(RECVINFO(m_chAreaBits[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_chAreaPortalBits), RecvPropInt(RECVINFO(m_chAreaPortalBits[0]))),
	RecvPropInt(RECVINFO(m_iHideHUD)),

	// View
	
	RecvPropFloat(RECVINFO(m_flFOVRate)),
	
	RecvPropInt		(RECVINFO(m_bDucked)),
	RecvPropInt		(RECVINFO(m_bDucking)),
	RecvPropFloat   (RECVINFO(m_flLastDuckTime)),
	RecvPropInt		(RECVINFO(m_bInDuckJump)),
	RecvPropInt		(RECVINFO(m_nDuckTimeMsecs)),
	RecvPropInt		(RECVINFO(m_nDuckJumpTimeMsecs)),
	RecvPropInt		(RECVINFO(m_nJumpTimeMsecs)),
	RecvPropFloat	(RECVINFO(m_flFallVelocity)),

#if PREDICTION_ERROR_CHECK_LEVEL > 1 
	RecvPropFloat	(RECVINFO_NAME( m_viewPunchAngle.m_Value[0], m_viewPunchAngle[0])),
	RecvPropFloat	(RECVINFO_NAME( m_viewPunchAngle.m_Value[1], m_viewPunchAngle[1])),
	RecvPropFloat	(RECVINFO_NAME( m_viewPunchAngle.m_Value[2], m_viewPunchAngle[2] )),
	RecvPropFloat	(RECVINFO_NAME( m_aimPunchAngle.m_Value[0], m_aimPunchAngle[0])),
	RecvPropFloat	(RECVINFO_NAME( m_aimPunchAngle.m_Value[1], m_aimPunchAngle[1])),
	RecvPropFloat	(RECVINFO_NAME( m_aimPunchAngle.m_Value[2], m_aimPunchAngle[2] )),
	RecvPropFloat	(RECVINFO_NAME( m_aimPunchAngleVel.m_Value[0], m_aimPunchAngleVel[0] )),
	RecvPropFloat	(RECVINFO_NAME( m_aimPunchAngleVel.m_Value[1], m_aimPunchAngleVel[1] )),
	RecvPropFloat	(RECVINFO_NAME( m_aimPunchAngleVel.m_Value[2], m_aimPunchAngleVel[2] )),
#else
	RecvPropVector	(RECVINFO(m_viewPunchAngle)),
	RecvPropVector	(RECVINFO(m_aimPunchAngle)),
	RecvPropVector	(RECVINFO(m_aimPunchAngleVel)),
#endif

	RecvPropInt		(RECVINFO(m_bDrawViewmodel)),
	RecvPropInt		(RECVINFO(m_bWearingSuit)),
	RecvPropBool	(RECVINFO(m_bPoisoned)),
	RecvPropFloat	(RECVINFO(m_flStepSize)),
	RecvPropInt		(RECVINFO(m_bAllowAutoMovement)),

	// 3d skybox data
	RecvPropInt(RECVINFO(m_skybox3d.scale)),
	RecvPropVector(RECVINFO(m_skybox3d.origin)),
	RecvPropInt(RECVINFO(m_skybox3d.area)),

	// 3d skybox fog data
	RecvPropInt( RECVINFO( m_skybox3d.fog.enable ) ),
	RecvPropInt( RECVINFO( m_skybox3d.fog.blend ) ),
	RecvPropVector( RECVINFO( m_skybox3d.fog.dirPrimary ) ),
	RecvPropInt( RECVINFO( m_skybox3d.fog.colorPrimary ), 0, RecvProxy_Int32ToColor32 ),
	RecvPropInt( RECVINFO( m_skybox3d.fog.colorSecondary ), 0, RecvProxy_Int32ToColor32 ),
	RecvPropFloat( RECVINFO( m_skybox3d.fog.start ) ),
	RecvPropFloat( RECVINFO( m_skybox3d.fog.end ) ),
	RecvPropFloat( RECVINFO( m_skybox3d.fog.maxdensity ) ),
	RecvPropFloat( RECVINFO( m_skybox3d.fog.HDRColorScale ) ),

	// audio data
	RecvPropVector( RECVINFO( m_audio.localSound[0] ) ),
	RecvPropVector( RECVINFO( m_audio.localSound[1] ) ),
	RecvPropVector( RECVINFO( m_audio.localSound[2] ) ),
	RecvPropVector( RECVINFO( m_audio.localSound[3] ) ),
	RecvPropVector( RECVINFO( m_audio.localSound[4] ) ),
	RecvPropVector( RECVINFO( m_audio.localSound[5] ) ),
	RecvPropVector( RECVINFO( m_audio.localSound[6] ) ),
	RecvPropVector( RECVINFO( m_audio.localSound[7] ) ),
	RecvPropInt( RECVINFO( m_audio.soundscapeIndex ) ),
	RecvPropInt( RECVINFO( m_audio.localBits ) ),
	RecvPropInt( RECVINFO( m_audio.entIndex ) ),

END_RECV_TABLE()

// -------------------------------------------------------------------------------- //
// This data only gets sent to clients that ARE this player entity.
// -------------------------------------------------------------------------------- //

	BEGIN_RECV_TABLE_NOBASE( C_BasePlayer, DT_LocalPlayerExclusive )

		RecvPropDataTable	( RECVINFO_DT(m_Local),0, &REFERENCE_RECV_TABLE(DT_Local) ),

		RecvPropFloat		( RECVINFO(m_vecViewOffset[0]) ),
		RecvPropFloat		( RECVINFO(m_vecViewOffset[1]) ),
		RecvPropFloat		( RECVINFO(m_vecViewOffset[2]) ),
		RecvPropFloat		( RECVINFO(m_flFriction) ),

		RecvPropInt			( RECVINFO(m_fOnTarget) ),

		RecvPropInt			( RECVINFO( m_nTickBase ) ),
		RecvPropInt			( RECVINFO( m_nNextThinkTick ) ),

		RecvPropEHandle		( RECVINFO( m_hLastWeapon ) ),

 		RecvPropFloat		( RECVINFO(m_vecVelocity[0]), 0, C_BasePlayer::RecvProxy_LocalVelocityX ),
 		RecvPropFloat		( RECVINFO(m_vecVelocity[1]), 0, C_BasePlayer::RecvProxy_LocalVelocityY ),
 		RecvPropFloat		( RECVINFO(m_vecVelocity[2]), 0, C_BasePlayer::RecvProxy_LocalVelocityZ ),

		RecvPropVector		( RECVINFO( m_vecBaseVelocity ) ),

		RecvPropEHandle		( RECVINFO( m_hConstraintEntity)),
		RecvPropVector		( RECVINFO( m_vecConstraintCenter) ),
		RecvPropFloat		( RECVINFO( m_flConstraintRadius )),
		RecvPropFloat		( RECVINFO( m_flConstraintWidth )),
		RecvPropFloat		( RECVINFO( m_flConstraintSpeedFactor )),
		RecvPropBool		( RECVINFO( m_bConstraintPastRadius )),

		RecvPropFloat		( RECVINFO( m_flDeathTime )),
		RecvPropFloat		( RECVINFO( m_flNextDecalTime )),

		RecvPropFloat		( RECVINFO( m_fForceTeam )),

		RecvPropInt			( RECVINFO( m_nWaterLevel ) ),
		RecvPropFloat		( RECVINFO( m_flLaggedMovementValue )),

		RecvPropEHandle		( RECVINFO( m_hTonemapController ) ),

	END_RECV_TABLE()

	
// -------------------------------------------------------------------------------- //
// DT_BasePlayer datatable.
// -------------------------------------------------------------------------------- //
	IMPLEMENT_CLIENTCLASS_DT(C_BasePlayer, DT_BasePlayer, CBasePlayer)
		// We have both the local and nonlocal data in here, but the server proxies
		// only send one.
		RecvPropDataTable( "localdata", 0, 0, &REFERENCE_RECV_TABLE(DT_LocalPlayerExclusive) ),

		RecvPropDataTable(RECVINFO_DT(pl), 0, &REFERENCE_RECV_TABLE(DT_PlayerState), DataTableRecvProxy_StaticDataTable),

		RecvPropInt		(RECVINFO(m_iFOV)),
		RecvPropInt		(RECVINFO(m_iFOVStart)),
		RecvPropFloat	(RECVINFO(m_flFOVTime)),
		RecvPropInt		(RECVINFO(m_iDefaultFOV)),
		RecvPropEHandle (RECVINFO(m_hZoomOwner)),

		RecvPropInt( RECVINFO(m_afPhysicsFlags) ),

		RecvPropEHandle( RECVINFO(m_hVehicle) ),
		RecvPropEHandle( RECVINFO(m_hUseEntity) ),

		RecvPropEHandle		( RECVINFO( m_hGroundEntity ) ),

		RecvPropInt		(RECVINFO(m_iHealth)),
		RecvPropInt		(RECVINFO(m_lifeState)),

		RecvPropArray3	( RECVINFO_ARRAY(m_iAmmo), RecvPropInt( RECVINFO(m_iAmmo[0])) ),

		RecvPropInt		(RECVINFO(m_iBonusProgress)),
		RecvPropInt		(RECVINFO(m_iBonusChallenge)),

		RecvPropFloat	(RECVINFO(m_flMaxspeed)),
		RecvPropInt		(RECVINFO(m_fFlags)),

		RecvPropInt		(RECVINFO(m_iObserverMode), 0, C_BasePlayer::RecvProxy_ObserverMode ),
		RecvPropBool	(RECVINFO(m_bActiveCameraMan)),
		RecvPropBool	(RECVINFO(m_bCameraManXRay)),
		RecvPropBool	(RECVINFO(m_bCameraManOverview)),
		RecvPropBool	(RECVINFO(m_bCameraManScoreBoard)),
		RecvPropInt		(RECVINFO(m_uCameraManGraphs)),

		RecvPropInt	(RECVINFO( m_iDeathPostEffect ) ),

		RecvPropEHandle	(RECVINFO(m_hObserverTarget), C_BasePlayer::RecvProxy_ObserverTarget ),
		RecvPropArray	( RecvPropEHandle( RECVINFO( m_hViewModel[0] ) ), m_hViewModel ),

		RecvPropInt		(RECVINFO(m_iCoachingTeam)),

		RecvPropString( RECVINFO(m_szLastPlaceName) ),
		RecvPropVector( RECVINFO(m_vecLadderNormal) ),
		RecvPropInt		(RECVINFO(m_ladderSurfaceProps) ),

		RecvPropInt( RECVINFO( m_ubEFNoInterpParity ) ),

		RecvPropEHandle( RECVINFO( m_hPostProcessCtrl ) ),		// Send to everybody - for spectating
		RecvPropEHandle( RECVINFO( m_hColorCorrectionCtrl ) ),	// Send to everybody - for spectating

		// fog data
		RecvPropEHandle( RECVINFO( m_PlayerFog.m_hCtrl ) ),

		RecvPropInt( RECVINFO( m_vphysicsCollisionState ) ),
#if defined( DEBUG_MOTION_CONTROLLERS )
		RecvPropVector( RECVINFO( m_Debug_vPhysPosition ) ),
		RecvPropVector( RECVINFO( m_Debug_vPhysVelocity ) ),
		RecvPropVector( RECVINFO( m_Debug_LinearAccel ) ),

		RecvPropVector( RECVINFO( m_vNewVPhysicsPosition ) ),
		RecvPropVector( RECVINFO( m_vNewVPhysicsVelocity ) ),
#endif

		RecvPropEHandle		( RECVINFO( m_hViewEntity ) ),		// L4D: send view entity to everyone for first-person spectating
		RecvPropBool		( RECVINFO( m_bShouldDrawPlayerWhileUsingViewEntity ) ),

		RecvPropFloat	(RECVINFO(m_flDuckAmount)),
		RecvPropFloat	(RECVINFO(m_flDuckSpeed)),

	END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( CPlayerState )

	DEFINE_PRED_FIELD(  deadflag, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	// DEFINE_FIELD( netname, string_t ),
	// DEFINE_FIELD( fixangle, FIELD_INTEGER ),
	// DEFINE_FIELD( anglechange, FIELD_FLOAT ),
	// DEFINE_FIELD( v_angle, FIELD_VECTOR ),

END_PREDICTION_DATA()	

BEGIN_PREDICTION_DATA_NO_BASE( CPlayerLocalData )

	// DEFINE_PRED_TYPEDESCRIPTION( m_skybox3d, sky3dparams_t ),
	// DEFINE_PRED_TYPEDESCRIPTION( m_fog, fogparams_t ),
	// DEFINE_PRED_TYPEDESCRIPTION( m_audio, audioparams_t ),
	DEFINE_FIELD( m_nStepside, FIELD_INTEGER ),

	DEFINE_PRED_FIELD( m_iHideHUD, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
#if PREDICTION_ERROR_CHECK_LEVEL > 1
	DEFINE_PRED_FIELD( m_viewPunchAngle, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_aimPunchAngle, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_aimPunchAngleVel, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
#else
	DEFINE_PRED_FIELD_TOL( m_viewPunchAngle, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.125f ),
	DEFINE_PRED_FIELD_TOL( m_aimPunchAngle, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.125f ),
	DEFINE_PRED_FIELD_TOL( m_aimPunchAngleVel, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.125f ),
#endif
	DEFINE_PRED_FIELD( m_bDrawViewmodel, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bWearingSuit, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bPoisoned, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bAllowAutoMovement, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_FIELD( m_bDucked, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDucking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flLastDuckTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bInDuckJump, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nDuckTimeMsecs, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nDuckJumpTimeMsecs, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nJumpTimeMsecs, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flFallVelocity, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.5f ),
//	DEFINE_PRED_FIELD( m_nOldButtons, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_FIELD( m_nOldButtons, FIELD_INTEGER ),
	DEFINE_PRED_FIELD( m_flStepSize, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_FIELD( m_flFOVRate, FIELD_FLOAT ),

END_PREDICTION_DATA()	

BEGIN_PREDICTION_DATA( C_BasePlayer )

	DEFINE_PRED_TYPEDESCRIPTION( m_Local, CPlayerLocalData ),
	DEFINE_PRED_TYPEDESCRIPTION( pl, CPlayerState ),

	DEFINE_PRED_FIELD( m_iFOV, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hZoomOwner, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flFOVTime, FIELD_FLOAT, 0 ),
	DEFINE_PRED_FIELD( m_iFOVStart, FIELD_INTEGER, 0 ),

	
	DEFINE_FIELD( m_oldOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_bTouchedPhysObject, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bPhysicsWasFrozen, FIELD_BOOLEAN ),
#if defined( DEBUG_MOTION_CONTROLLERS )
	DEFINE_PRED_FIELD( m_vNewVPhysicsPosition, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vNewVPhysicsVelocity, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
#else
	DEFINE_FIELD( m_vNewVPhysicsPosition, FIELD_VECTOR ),
	DEFINE_FIELD( m_vNewVPhysicsVelocity, FIELD_VECTOR ),
#endif
	DEFINE_PRED_FIELD( m_afPhysicsFlags, FIELD_INTEGER, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),

	DEFINE_PRED_FIELD( m_hVehicle, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flMaxspeed, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.5f ),
	DEFINE_PRED_FIELD( m_iHealth, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iBonusProgress, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iBonusChallenge, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fOnTarget, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nNextThinkTick, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_lifeState, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nWaterLevel, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),
	
	DEFINE_PRED_FIELD_TOL( m_vecBaseVelocity, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 0.05 ),

	DEFINE_FIELD( m_nButtons, FIELD_INTEGER ),
	DEFINE_FIELD( m_flWaterJumpTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_nImpulse, FIELD_INTEGER ),
	DEFINE_FIELD( m_flStepSoundTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flSwimSoundTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_ignoreLadderJumpTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_bHasWalkMovedSinceLastJump, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_vecLadderNormal, FIELD_VECTOR ),
	DEFINE_FIELD( m_ladderSurfaceProps, FIELD_INTEGER ),
	DEFINE_FIELD( m_flPhysics, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_szAnimExtension, FIELD_CHARACTER ),
	DEFINE_FIELD( m_afButtonLast, FIELD_INTEGER ),
	DEFINE_FIELD( m_afButtonPressed, FIELD_INTEGER ),
	DEFINE_FIELD( m_afButtonReleased, FIELD_INTEGER ),
	// DEFINE_FIELD( m_vecOldViewAngles, FIELD_VECTOR ),

	// DEFINE_ARRAY( m_iOldAmmo, FIELD_INTEGER,  MAX_AMMO_TYPES ),

	//DEFINE_FIELD( m_hOldVehicle, FIELD_EHANDLE ),
	// DEFINE_FIELD( m_pModelLight, dlight_t* ),
	// DEFINE_FIELD( m_pEnvironmentLight, dlight_t* ),
	// DEFINE_FIELD( m_pBrightLight, dlight_t* ),
	DEFINE_PRED_FIELD( m_hLastWeapon, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_FIELD( m_nTickBase, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_FIELD( m_hGroundEntity, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_ARRAY( m_hViewModel, FIELD_EHANDLE, MAX_VIEWMODELS, FTYPEDESC_INSENDTABLE ),

	DEFINE_FIELD( m_surfaceFriction, FIELD_FLOAT ),

	DEFINE_FIELD( m_vecPreviouslyPredictedOrigin, FIELD_VECTOR ),

	DEFINE_PRED_FIELD( m_vphysicsCollisionState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
#if defined( DEBUG_MOTION_CONTROLLERS )
	DEFINE_PRED_FIELD( m_Debug_vPhysPosition, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_Debug_vPhysVelocity, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_Debug_LinearAccel, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
#endif

	DEFINE_PRED_FIELD( m_flDuckAmount, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flDuckSpeed, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()

#if !defined( PORTAL2 )
LINK_ENTITY_TO_CLASS( player, C_BasePlayer );
#endif

// -------------------------------------------------------------------------------- //
// Functions.
// -------------------------------------------------------------------------------- //
C_BasePlayer::C_BasePlayer() : m_iv_vecViewOffset( "C_BasePlayer::m_iv_vecViewOffset" )
{
	AddVar( &m_vecViewOffset, &m_iv_vecViewOffset, LATCH_SIMULATION_VAR );
	
#ifdef _DEBUG																
	m_vecLadderNormal.Init();
	m_ladderSurfaceProps = 0;
	m_vecOldViewAngles.Init();
#endif
	m_hViewEntity = NULL;
	m_bShouldDrawPlayerWhileUsingViewEntity = false;

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; i++ )
	{
		m_bFlashlightEnabled[ i ] = false;
	}

	m_pCurrentVguiScreen = NULL;
	m_pCurrentCommand = NULL;

	m_flPredictionErrorTime = -100;
	m_StuckLast = 0;
	m_bWasFrozen = false;

	m_bResampleWaterSurface = true;
	
	ResetObserverMode();

	m_bActiveCameraMan = false;
	m_bCameraManXRay = false;
	m_bCameraManOverview = false;
	m_bCameraManScoreBoard = false;
	m_uCameraManGraphs = 0;
	m_bLastActiveCameraManState = false;
	m_bLastCameraManXRayState = false;
	m_bLastCameraManOverviewState = false;
	m_bLastCameraManScoreBoardState = false;
	m_uLastCameraManGraphsState = 0;

	m_iDeathPostEffect = 0;

	m_flTimeLastTouchedGround = 0.0f;
	m_vecPredictionError.Init();
	m_flPredictionErrorTime = 0;

	m_surfaceProps = 0;
	m_pSurfaceData = NULL;
	m_surfaceFriction = 1.0f;
	m_chTextureType = 0;
#if MAX_SPLITSCREEN_PLAYERS > 1
	m_nSplitScreenSlot = -1; // This is likely to catch bugs when running in splitscreen config and using local player before initializing the slots
#else
	m_nSplitScreenSlot = 0; // When we are building without splitscreen all sorts of SFUI are using local player and might index arrays with negative index otherwise, but we know that only one player exists so just initialize for the correct slot!
#endif
	m_bIsLocalPlayer = false;
	m_afButtonForced = 0;
	m_bDisableSimulationFix = true;
	m_flNextAchievementAnnounceTime = 0;

	m_flFreezePanelExtendedStartTime = 0;
	m_bWasFreezePanelExtended = false;
	m_bStartedFreezeFrame = false;
	m_bAbortedFreezeFrame = false;

	m_bFiredWeapon = false;

	m_vecHack_RecvProxy_LocalPlayerOrigin.Init( FLT_MAX, FLT_MAX, FLT_MAX );
	m_ignoreLadderJumpTime = 0.0f;

	m_bPlayerIsTalkingOverVOIP = false;

	m_bCanShowFreezeFrameNow = false;
	m_nLastKillerDamageTaken = 0;
	m_nLastKillerHitsTaken = 0;
	m_nLastKillerDamageGiven = 0;
	m_nLastKillerHitsGiven = 0;

	// Init eye and angle offsets - used for TrackIR and motion controllers
	m_vecEyeOffset.Init();
	m_EyeAngleOffset.Init();
	m_AimDirection.Init();

	m_flDuckAmount = 0.0f;
	m_flDuckSpeed = CS_PLAYER_DUCK_SPEED_IDEAL;
	m_vecLastPositionAtFullCrouchSpeed = vec2_origin;

	m_bHasWalkMovedSinceLastJump = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BasePlayer::~C_BasePlayer()
{
	if ( this == s_pLocalPlayer[ 0 ] )
	{
		extern void UntouchAllTriggerSoundOperator( C_BaseEntity *pEntity );
		UntouchAllTriggerSoundOperator( this );
	}

	DeactivateVguiScreen( m_pCurrentVguiScreen.Get() );
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if ( this == s_pLocalPlayer[ i ] )
		{	
			s_pLocalPlayer[ i ] = NULL;
		}
		else if ( s_pLocalPlayer[ i ] )
		{
			s_pLocalPlayer[ i ]->RemoveSplitScreenPlayer( this );
		}

		if ( m_bFlashlightEnabled[ i ] )
		{
			FlashlightEffectManager( i ).TurnOffFlashlight( true );
			m_bFlashlightEnabled[ i ] = false;
		}
	}
}

bool MsgFunc_SendLastKillerDamageToClient( const CCSUsrMsg_SendLastKillerDamageToClient &msg )
{
	int nNumHitsGiven = msg.num_hits_given();
	int nDamageGiven = msg.damage_given();
	int nNumHitsTaken = msg.num_hits_taken();
	int nDamageTaken = msg.damage_taken();

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		pPlayer->SetLastKillerDamageAndFreezeframe( nDamageTaken, nNumHitsTaken, nDamageGiven, nNumHitsGiven );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BasePlayer::Spawn( void )
{
	m_UMCMsg_SendLastKillerDamageToClient.Bind< CS_UM_SendLastKillerDamageToClient, CCSUsrMsg_SendLastKillerDamageToClient >
		( UtlMakeDelegate( MsgFunc_SendLastKillerDamageToClient ));

	// Clear all flags except for FL_FULLEDICT
	ClearFlags();
	AddFlag( FL_CLIENT );

	int effects = GetEffects() & EF_NOSHADOW;
	SetEffects( effects );

	m_iFOV	= 0;	// init field of view.

	SetModel( GetPlayerModelName() );

	Precache();

	SetThink(NULL);

	RANDOM_CEG_TEST_SECRET_LINE_PERIOD( 17, 0, 41, 0 );

	SharedSpawn();

	m_bWasFreezeFraming = false;
	
	m_bFiredWeapon = false;
}

void C_BasePlayer::UpdateOnRemove( void )
{


	if ( m_pPhysicsController )
	{
		physenv->DestroyPlayerController( m_pPhysicsController );
		m_pPhysicsController = NULL;
	}
	PhysRemoveShadow( this );

	VPhysicsSetObject( NULL );
	if( m_pShadowStand )
	{
		physenv->DestroyObject( m_pShadowStand );
		m_pShadowStand = NULL;
	}

	if( m_pShadowCrouch )
	{
		physenv->DestroyObject( m_pShadowCrouch );
		m_pShadowCrouch = NULL;
	}

	if ( m_speechVOIPParticleEffect.IsValid() )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_speechVOIPParticleEffect );
		m_speechVOIPParticleEffect = NULL;
	}

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BasePlayer::AudioStateIsUnderwater( Vector vecMainViewOrigin )
{
	if ( IsObserver() )
	{
		// Just check the view position
		int cont = enginetrace->GetPointContents_WorldOnly( vecMainViewOrigin, MASK_WATER );
		return (cont & MASK_WATER) ? true : false;
	}

	return ( GetWaterLevel() >= WL_Eyes );
}

#if defined( REPLAY_ENABLED )
bool C_BasePlayer::IsReplay() const
{
	return ( IsLocalPlayer( const_cast< C_BasePlayer * >( this ) ) && engine->IsReplay() );
}
#endif

bool C_BasePlayer::IsBot( void ) const 
{ 
	IGameResources* pGR = GameResources();

	return ( pGR && pGR->IsFakePlayer( entindex() ) );
}


CBaseEntity	*C_BasePlayer::GetObserverTarget() const	// returns players targer or NULL
{
	if ( IsHLTV() )
	{
		return HLTVCamera()->GetPrimaryTarget();
	}
#if defined( REPLAY_ENABLED )
	if ( IsReplay() )
	{
		return ReplayCamera()->GetPrimaryTarget();
	}
#endif
	
	return m_hObserverTarget;
}

void UpdateWorldmodelVisibility( C_BasePlayer *player )
{
	for ( int i = 0; i < player->WeaponCount(); i++ )
	{
		CBaseCombatWeapon *pWeapon = player->GetWeapon(i);
		if ( pWeapon )
		{
			CBaseWeaponWorldModel *pWeaponWorldModel = pWeapon->GetWeaponWorldModel();
			if ( pWeaponWorldModel )
			{
				pWeaponWorldModel->UpdateVisibility();
			}
		}
	}
}

// Helper method to fix up visiblity across split screen for view models when observer target or mode changes
void UpdateViewmodelVisibility( C_BasePlayer *player )
{
	// also update world models
	UpdateWorldmodelVisibility( player );

	// Update view model visibility
	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = player->GetViewModel( i );
		if ( !vm )
			continue;
		vm->UpdateVisibility();
	}
}

// Called from Recv Proxy, mainly to reset tone map scale
void C_BasePlayer::SetObserverTarget( EHANDLE hObserverTarget )
{
	// [msmith] We need to update the view model visibility status of the player we were observing because their view models
	// may no longer be rendering in our splitscreen viewport.
	C_BasePlayer* pOldObserverTarget = ToBasePlayer( m_hObserverTarget );

	C_BasePlayer *pNewObserverTarget = ToBasePlayer( hObserverTarget );

	// If the observer target is changing to an entity that the client doesn't know about yet,
	// it can resolve to NULL.  If the client didn't have an observer target before, then
	// comparing EHANDLEs directly will see them as equal, since it uses Get(), and compares
	// NULL to NULL.  To combat this, we need to check against GetEntryIndex() and
	// GetSerialNumber().
	if ( hObserverTarget.GetEntryIndex() != m_hObserverTarget.GetEntryIndex() ||
		hObserverTarget.GetSerialNumber() != m_hObserverTarget.GetSerialNumber())
	{
		// Init based on the new handle's entry index and serial number, so that it's Get()
		// has a chance to become non-NULL even if it currently resolves to NULL.
		m_hObserverTarget.Init( hObserverTarget.GetEntryIndex(), hObserverTarget.GetSerialNumber() );

		IGameEvent *event = gameeventmanager->CreateEvent( "spec_target_updated" );
		if ( event )
		{
			event->SetInt("userid", GetUserID() );
			gameeventmanager->FireEventClientSide( event );
		}

		if ( IsLocalPlayer( this ) )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );
			ResetToneMapping( -1.0f ); // This forces the tonemapping scalar to the average of min and max
		}
		UpdateViewmodelVisibility( this );
		UpdateVisibility();	

		if ( pNewObserverTarget )
		{
			pNewObserverTarget->UpdateVisibility();
			UpdateViewmodelVisibility( pNewObserverTarget );
		}
	}

	// [msmith] We need to wait until we've set a new observer target before updating the view model visibility of our
	// old observer target.
	// NOTE: We need to update this even if the observed target did not change because the observer mode may have changed.
	//       If the observer mode switched to third person for example, the view model should NOT be drawn.
	if ( pOldObserverTarget )
	{
		pOldObserverTarget->UpdateVisibility();
		UpdateViewmodelVisibility( pOldObserverTarget );
	}


}

int C_BasePlayer::GetObserverMode() const 
{ 
	if ( IsHLTV() )
	{
		return HLTVCamera()->GetMode();
	}
#if defined( REPLAY_ENABLED )
	if ( IsReplay() )
	{
		return ReplayCamera()->GetMode();
	}
#endif

	return m_iObserverMode; 
}


//-----------------------------------------------------------------------------
// Used by prediction, sets the view angles for the player
//-----------------------------------------------------------------------------
void C_BasePlayer::SetLocalViewAngles( const QAngle &viewAngles )
{
	pl.v_angle = viewAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ang - 
//-----------------------------------------------------------------------------
void C_BasePlayer::SetViewAngles( const QAngle& ang )
{
	SetLocalAngles( ang );
	SetNetworkAngles( ang );
}


surfacedata_t* C_BasePlayer::GetGroundSurface()
{
	//
	// Find the name of the material that lies beneath the player.
	//
	Vector start, end;
	VectorCopy( GetAbsOrigin(), start );
	VectorCopy( start, end );

	// Straight down
	end.z -= 64;

	// Fill in default values, just in case.
	
	Ray_t ray;
	ray.Init( start, end, GetPlayerMins(), GetPlayerMaxs() );

	trace_t	trace;
	UTIL_TraceRay( ray, MASK_PLAYERSOLID_BRUSHONLY, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

	if ( trace.fraction == 1.0f )
		return NULL;	// no ground
	
	return physprops->GetSurfaceData( trace.surface.surfaceProps );
}


//-----------------------------------------------------------------------------
// returns the player name
//-----------------------------------------------------------------------------
const char * C_BasePlayer::GetPlayerName()
{
	return g_PR ? g_PR->GetPlayerName( entindex() ) : "";
}

//-----------------------------------------------------------------------------
// Is the player dead?
//-----------------------------------------------------------------------------
bool C_BasePlayer::IsPlayerDead()
{
	return pl.deadflag == true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_BasePlayer::SetVehicleRole( int nRole )
{
	if ( !IsInAVehicle() )
		return;

	// HL2 has only a player in a vehicle.
	if ( nRole > VEHICLE_ROLE_DRIVER )
		return;

	char szCmd[64];
	Q_snprintf( szCmd, sizeof( szCmd ), "vehicleRole %i\n", nRole );
	engine->ServerCmd( szCmd );
}

//-----------------------------------------------------------------------------
// Purpose: Store original ammo data to see what has changed
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BasePlayer::OnPreDataChanged( DataUpdateType_t updateType )
{
	for (int i = 0; i < MAX_AMMO_TYPES; ++i)
	{
		m_iOldAmmo[i] = GetAmmoCount(i);
	}

	m_bWasFreezeFraming = (GetObserverMode() == OBS_MODE_FREEZECAM);
	m_hOldFogController = m_PlayerFog.m_hCtrl;

	m_iOldObserverMode = GetObserverMode();

	BaseClass::OnPreDataChanged( updateType );
}

void C_BasePlayer::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_ubOldEFNoInterpParity = m_ubEFNoInterpParity;
}

void C_BasePlayer::CheckForLocalPlayer( int nSplitScreenSlot )
{
	// Make sure s_pLocalPlayer is correct
	int iLocalPlayerIndex = ( nSplitScreenSlot != -1 ) ? engine->GetLocalPlayer() : 0;

	if ( g_nKillCamMode )
		iLocalPlayerIndex = g_nKillCamTarget1;

	if ( iLocalPlayerIndex == index && !g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		s_pLocalPlayer[ nSplitScreenSlot ] = this;
		m_bIsLocalPlayer = true;

		// Tell host player about the parasitic splitscreen user
		if ( nSplitScreenSlot != 0 )
		{
			Assert( s_pLocalPlayer[ 0 ] );
			m_nSplitScreenSlot = nSplitScreenSlot;
			m_hSplitOwner = s_pLocalPlayer[ 0 ];
			if ( s_pLocalPlayer[ 0 ] )
			{
				s_pLocalPlayer[ 0 ]->AddSplitScreenPlayer( this );
			}
		}
		else
		{
			// We're the host, not the parasite...
			m_nSplitScreenSlot = 0;
			m_hSplitOwner = NULL;
		}

		if ( nSplitScreenSlot == 0 )
		{
			// Reset our sound mixed in case we were in a freeze cam when we
			// changed level, which would cause the snd_soundmixer to be left modified.
			ConVar *pVar = (ConVar *)cvar->FindVar( "snd_soundmixer" );
			pVar->Revert();
		}

		UpdateVisibilityAllEntities();
	}
}





void C_BasePlayer::ClientThink()
{
	//////////////////////////////////////////////
	//  spec_lock_to_accountid
	//
	//	Lock observer target to the specified account id
	////////////////////////////////////////////////////////

	static float flNextCheck = Plat_FloatTime( ) + 5;

	if ( Plat_FloatTime( ) >= flNextCheck &&
		spec_lock_to_accountid.GetString( ) &&
		spec_lock_to_accountid.GetString( )[0] &&
		IsObserver( ) && 
		IsLocalPlayer( this ) )
	{
		bool bSwitchTargets = true;

		if ( GetObserverTarget( ) && ( GetObserverMode( ) == OBS_MODE_IN_EYE || GetObserverMode( ) == OBS_MODE_CHASE ) )
		{
			C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( GetObserverTarget( ) );

			if ( pPlayer )
			{
				CSteamID SteamID;
				pPlayer->GetSteamID( &SteamID );

				if ( SteamID.GetAccountID( ) == CSteamID( spec_lock_to_accountid.GetString( ) ).GetAccountID() )
				{
					bSwitchTargets = false;
				}
			}
		}

		if ( bSwitchTargets )
		{
			DevMsg( "spec_lock_to_accountid: Attempting to switch spec target to %s. Clear out the convar spec_lock_to_accountid to stop.\n", spec_lock_to_accountid.GetString( ) );

 			if ( g_bEngineIsHLTV )
			{
				// we can only switch primary spectator targets if PVS isnt locked by auto-director
				if ( !HLTVCamera( )->IsPVSLocked( ) )
				{
					HLTVCamera( )->SpecPlayerByAccountID( spec_lock_to_accountid.GetString( ) );

					HLTVCamera( )->SetMode( OBS_MODE_IN_EYE );
						
					//HLTVCamera()->SetAutoDirector( C_HLTVCamera::AUTODIRECTOR_PAUSED );
					HLTVCamera( )->SetAutoDirector( C_HLTVCamera::AUTODIRECTOR_OFF );
				}
			}
			else
			{
				char szCmd[ 64 ];
				Q_snprintf( szCmd, sizeof( szCmd ), "spec_player_by_accountid %s\n", spec_lock_to_accountid.GetString() );
				engine->ServerCmd( szCmd );

				Q_snprintf( szCmd, sizeof( szCmd ), "spec_mode %i\n", OBS_MODE_IN_EYE );
				engine->ServerCmd( szCmd );
			}			
		}

		flNextCheck = Plat_FloatTime( ) + 5;
	}
}


void C_BasePlayer::SetAsLocalPlayer()
{
	int nSplitScreenSlot = 0;
	Assert( s_pLocalPlayer[ nSplitScreenSlot ] == NULL );
	s_pLocalPlayer[ nSplitScreenSlot ] = this;
	m_bIsLocalPlayer = true;

	// We're the host
	m_nSplitScreenSlot = 0;
	m_hSplitOwner = NULL;

	if ( nSplitScreenSlot == 0 && !g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		// Reset our sound mixed in case we were in a freeze cam when we
		// changed level, which would cause the snd_soundmixer to be left modified.
		ConVar *pVar = ( ConVar * )cvar->FindVar( "snd_soundmixer" );
		pVar->Revert();
	}

	UpdateVisibilityAllEntities();
}




//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_BasePlayer::PostDataUpdate( DataUpdateType_t updateType )
{
	// This has to occur here as opposed to OnDataChanged so that EHandles to the player created
	//  on this same frame are not stomped because prediction thinks there
	//  isn't a local player yet!!!

	int nSlot = -1;

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		int nIndex = engine->GetSplitScreenPlayer( i );
		if ( nIndex == index )
		{
			nSlot = i;
			break;
		}
	}
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );

	bool bRecheck = false;
	if ( nSlot != -1 && !s_pLocalPlayer[ nSlot ] )
	{
		bRecheck = true;
	}
	if ( updateType == DATA_UPDATE_CREATED || bRecheck )
	{
		CheckForLocalPlayer( nSlot );
	}

	bool bForceEFNoInterp = ( m_ubOldEFNoInterpParity != m_ubEFNoInterpParity );

	if ( IsLocalPlayer( this ) )
	{
		if ( (updateType == DATA_UPDATE_CREATED) &&
			g_pGameRules->IsMultiplayer() && 
			cl_player_fullupdate_predicted_origin_fix.GetBool() )
		{
			if ( (m_vecHack_RecvProxy_LocalPlayerOrigin.x != FLT_MAX) && 
				(m_vecHack_RecvProxy_LocalPlayerOrigin.y != FLT_MAX) && 
				(m_vecHack_RecvProxy_LocalPlayerOrigin.z != FLT_MAX) )
			{
				SetNetworkOrigin( m_vecHack_RecvProxy_LocalPlayerOrigin );
			}
		}
		m_vecHack_RecvProxy_LocalPlayerOrigin.Init( FLT_MAX, FLT_MAX, FLT_MAX );
		SetSimulatedEveryTick( true );
	}
	else
	{
		SetSimulatedEveryTick( false );

		// estimate velocity for non local players
		float flTimeDelta = m_flSimulationTime - m_flOldSimulationTime;

		if (IsParentChanging())
		{
			bForceEFNoInterp = true;
		}

		if ( flTimeDelta > 0  &&  !( IsEffectActive(EF_NOINTERP) || bForceEFNoInterp ) )
		{
			Vector newVelo = (GetNetworkOrigin() - GetOldOrigin()  ) / flTimeDelta;
			// This code used to call SetAbsVelocity, which is a no-no since we are in networking and if
			//  in hieararchy, the parent velocity might not be set up yet.
			// On top of that GetNetworkOrigin and GetOldOrigin are local coordinates
			// So we'll just set the local vel and avoid an Assert here
			SetLocalVelocity( newVelo );
		}
	}

	BaseClass::PostDataUpdate( updateType );
			 
	// Only care about this for local player
	if ( IsLocalPlayer( this ) )
	{
		bool bHideFreezePanel = false;

		QAngle angles;
		engine->GetViewAngles( angles );
		if ( updateType == DATA_UPDATE_CREATED )
		{
			SetLocalViewAngles( angles );
			m_flOldPlayerZ = GetLocalOrigin().z;
		}
		SetLocalAngles( angles );

		if ( m_bCanShowFreezeFrameNow && !m_bWasFreezeFraming && GetObserverMode() == OBS_MODE_FREEZECAM )
		{
			m_vecFreezeFrameStart = MainViewOrigin( GetSplitScreenPlayerSlot() );
			m_flFreezeFrameStartTime = gpGlobals->curtime;
			m_flFreezeFrameDistance = RandomFloat( spec_freeze_distance_min.GetFloat(), spec_freeze_distance_max.GetFloat() );
			m_flFreezeZOffset = RandomFloat( -4, 4 );
			m_bSentFreezeFrame = false;
			m_bStartedFreezeFrame = false;
			m_bAbortedFreezeFrame = false;

			IGameEvent *pEvent = gameeventmanager->CreateEvent( "show_freezepanel" );
			if ( pEvent )
			{
				pEvent->SetInt( "killer", GetObserverTarget() ? GetObserverTarget()->entindex() : GetLastKillerIndex() );
				pEvent->SetInt( "victim", entindex() );
				pEvent->SetInt( "hits_taken", m_nLastKillerHitsTaken );
				pEvent->SetInt( "damage_taken", m_nLastKillerDamageTaken );
				pEvent->SetInt( "hits_given", m_nLastKillerHitsGiven );
				pEvent->SetInt( "damage_given", m_nLastKillerDamageGiven );
				gameeventmanager->FireEventClientSide( pEvent );
			}

			// Force the sound mixer to the freezecam mixer
			ConVar *pVar = (ConVar *)cvar->FindVar( "snd_soundmixer" );
			pVar->SetValue( "FreezeCam_Only" );

			m_bCanShowFreezeFrameNow = false;
		}
		else if ( m_bWasFreezeFraming && GetObserverMode() != OBS_MODE_FREEZECAM )
		{
			if ( spec_freeze_panel_extended_time.GetFloat() > 0 )
			{
				m_flFreezePanelExtendedStartTime = gpGlobals->curtime;
				m_bWasFreezePanelExtended = true;
			}
			else
			{
				bHideFreezePanel = true;
			}

			view->FreezeFrame(0);

			ConVar *pVar = ( ConVar * )cvar->FindVar( "snd_soundmixer" );
			pVar->Revert();
		}
		else if ( m_bWasFreezePanelExtended && gpGlobals->curtime >= m_flFreezePanelExtendedStartTime + spec_freeze_panel_extended_time.GetFloat() )
		{
			bHideFreezePanel = true;
			m_bWasFreezePanelExtended = false;
		}
		else if ( IsAlive() )
		{
			SFHudFreezePanel *pPanel = GET_HUDELEMENT( SFHudFreezePanel );
			if ( pPanel && pPanel->IsVisible() )
			{
				//pPanel->ShowPanel( false );
				bHideFreezePanel = true;
			}	
		}
		
		if ( bHideFreezePanel && !g_HltvReplaySystem.GetHltvReplayDelay() && !g_HltvReplaySystem.IsDelayedReplayRequestPending() )
		{
			IGameEvent *pEvent = gameeventmanager->CreateEvent( "hide_freezepanel" );
			if ( pEvent )
			{
				gameeventmanager->FireEventClientSide( pEvent );
			}
		}
	}
	// If we are updated while paused, allow the player origin to be snapped by the
	//  server if we receive a packet from the server
	if ( engine->IsPaused() || bForceEFNoInterp )
	{
		ResetLatched();
	}
#ifdef DEMOPOLISH_ENABLED
	if ( engine->IsRecordingDemo() && 
		 IsDemoPolishRecording() )
	{
		m_bBonePolishSetup = true;
		matrix3x4a_t dummyBones[MAXSTUDIOBONES];
		C_BaseEntity::SetAbsQueriesValid( true );
		ForceSetupBonesAtTime( dummyBones, gpGlobals->curtime );
		C_BaseEntity::SetAbsQueriesValid( false );
		m_bBonePolishSetup = false;
	}
#endif

	m_fLastUpdateServerTime = engine->GetLastTimeStamp();
	m_nLastUpdateTickBase = m_nTickBase;
	m_nLastUpdateServerTickCount = engine->GetServerTick();

	if ( IsActiveCameraMan() != m_bLastActiveCameraManState )
	{
		m_bLastActiveCameraManState = IsActiveCameraMan();

		if ( IsActiveCameraMan() && HLTVCamera()->AutoDirectorState() != C_HLTVCamera::AUTODIRECTOR_OFF )
		{
			// if a cameraman becomes active and the autodirector is on, just set it on again and it'll switch to the cameraman if enabled
			HLTVCamera()->SetAutoDirector( C_HLTVCamera::AUTODIRECTOR_ON );
		}
	}

	// force voice recording on for casters
	if ( ( GetLocalPlayer() == this ) && HLTVCamera() && ( HLTVCamera()->GetCameraMan() == this ) && !engine->IsVoiceRecording() )
	{
		engine->ForceVoiceRecordOn();
	}

	int eventType = -1;
	int nOptionalParam = 0;
	ConVarRef spec_show_xray( "spec_show_xray" );

	CSteamID steamID;
	// if the state changed for any of the cameraman stuff, send to the local player
	if ( IsActiveCameraMan() && GetSteamID( &steamID ) )
	{
		if ( spec_show_xray.GetBool() != m_bLastCameraManXRayState || m_bCameraManXRay != m_bLastCameraManXRayState )
		{
			m_bLastCameraManXRayState = m_bCameraManXRay;
			eventType = ( m_bCameraManXRay ) ? HLTV_UI_XRAY_ON : HLTV_UI_XRAY_OFF;
			if ( eventType != -1 )
				GetClientMode()->UpdateCameraManUIState( eventType, nOptionalParam, steamID.ConvertToUint64() );
		}

		if ( m_bCameraManOverview != m_bLastCameraManOverviewState )
		{
			m_bLastCameraManOverviewState = m_bCameraManOverview;
			eventType = ( m_bCameraManOverview ) ? HLTV_UI_OVERVIEW_ON : HLTV_UI_OVERVIEW_OFF;
			if ( eventType != -1 )
				GetClientMode()->UpdateCameraManUIState( eventType, nOptionalParam, steamID.ConvertToUint64() );
		}

		if ( m_bCameraManScoreBoard != m_bLastCameraManScoreBoardState )
		{
			m_bLastCameraManScoreBoardState = m_bCameraManScoreBoard;
			eventType = ( m_bCameraManScoreBoard ) ? HLTV_UI_SCOREBOARD_ON : HLTV_UI_SCOREBOARD_OFF;
			if ( eventType != -1 )
				GetClientMode()->UpdateCameraManUIState( eventType, nOptionalParam, steamID.ConvertToUint64() );
		}

		if ( m_uCameraManGraphs != m_uLastCameraManGraphsState )
		{
			m_uLastCameraManGraphsState = m_uCameraManGraphs;
			eventType = ( m_uCameraManGraphs != 0 ) ? HLTV_UI_GRAPHS_ON : HLTV_UI_GRAPHS_OFF;
			if ( eventType == HLTV_UI_GRAPHS_ON )
			{
				nOptionalParam = m_uCameraManGraphs - 1;
			}

			if ( eventType != -1 )
				GetClientMode()->UpdateCameraManUIState( eventType, nOptionalParam, steamID.ConvertToUint64() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BasePlayer::CanSetSoundMixer( void )
{
	// Can't set sound mixers when we're in freezecam mode, since it has a code-enforced mixer
	return (GetObserverMode() != OBS_MODE_FREEZECAM);
}

void C_BasePlayer::ReceiveMessage( int classID, bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}
}

void C_BasePlayer::OnRestore()
{
	BaseClass::OnRestore();

	if ( IsLocalPlayer( this ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

		// debounce the attack key, for if it was used for restore
		input->ClearInputButton( IN_ATTACK | IN_ATTACK2 | IN_ZOOM );
		// GetButtonBits() has to be called for the above to take effect
		input->GetButtonBits( false );
	}

	// For ammo history icons to current value so they don't flash on level transtions
	int ammoTypes = GetAmmoDef()->NumAmmoTypes();
	// ammodef is 1 based, use <=
	for ( int i = 0; i <= ammoTypes; i++ )
	{
		m_iOldAmmo[i] = GetAmmoCount(i);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Process incoming data
//-----------------------------------------------------------------------------
void C_BasePlayer::OnDataChanged( DataUpdateType_t updateType )
{
	bool isLocalPlayer = IsLocalPlayer( this );
#if !defined( NO_ENTITY_PREDICTION )
	if ( isLocalPlayer )
	{
		SetPredictionEligible( true );
	}
#endif

	BaseClass::OnDataChanged( updateType );

	bool bIsLocalOrHltvObserverPlayer = g_HltvReplaySystem.GetHltvReplayDelay() ? HLTVCamera()->GetCurrentTargetEntindex() == this->index : isLocalPlayer;

	// Only care about this for local player
	if ( bIsLocalOrHltvObserverPlayer )
	{
		int nSlot = GetSplitScreenPlayerSlot();
		// Reset engine areabits pointer, but only for main local player (not piggybacked split screen users)
		if ( nSlot == 0 )
		{
			render->SetAreaState( m_Local.m_chAreaBits, m_Local.m_chAreaPortalBits );
		}

#if !defined( INCLUDE_SCALEFORM ) || !defined( CSTRIKE_DLL )
		// Check for Ammo pickups.
		int ammoTypes = GetAmmoDef()->NumAmmoTypes();
		for ( int i = 0; i <= ammoTypes; i++ )
		{
			if ( GetAmmoCount(i) > m_iOldAmmo[i] )
			{
				// Only add this to the correct Hud
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );

				// Don't add to ammo pickup if the ammo doesn't do it
				const FileWeaponInfo_t *pWeaponData = gWR.GetWeaponFromAmmo(i);

				if ( !pWeaponData || !( pWeaponData->iFlags & ITEM_FLAG_NOAMMOPICKUPS ) )
				{
					// We got more ammo for this ammo index. Add it to the ammo history
					CHudHistoryResource *pHudHR = GET_HUDELEMENT( CHudHistoryResource );
					if( pHudHR )
					{
						pHudHR->AddToHistory( HISTSLOT_AMMO, i, abs(GetAmmoCount(i) - m_iOldAmmo[i]) );
					}
				}
			}
		}
#endif

		// Only add this to the correct Hud
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );
			Soundscape_Update( m_Local.m_audio );
		}
	}
	
	if ( isLocalPlayer ) // fog controller is always using local player to update fog; this seems wrong for spectating, because the observer target entity may have different fog parameters, but in practice this has never been a problem. Eventually we may need to address that by changing GetLocalPlayer in viewrender.cpp
	{
		if ( m_hOldFogController != m_PlayerFog.m_hCtrl )
		{
			FogControllerChanged( updateType == DATA_UPDATE_CREATED );
		}
	}

	
	if( (updateType == DATA_UPDATE_CREATED) && physenv->IsPredicted() )
	{
		if( engine->GetLocalPlayer() == index ) //C_BasePlayer::IsLocalPlayer() doesn't work this early when created. This works just as well
		{
			//create physics objects
			Vector vecAbsOrigin = GetAbsOrigin();
			Vector vecAbsVelocity = GetAbsVelocity();

			solid_t solid;
			Q_strncpy( solid.surfaceprop, "player", sizeof(solid.surfaceprop) );
			solid.params = g_PhysDefaultObjectParams;
			solid.params.mass = 85.0f;
			solid.params.inertia = 1e24f;
			solid.params.enableCollisions = false;
			//disable drag
			solid.params.dragCoefficient = 0;
			// create standing hull
			m_pShadowStand = PhysModelCreateCustom( this, PhysCreateBbox( VEC_HULL_MIN, VEC_HULL_MAX ), GetLocalOrigin(), GetLocalAngles(), "player_stand", false, &solid );
			m_pShadowStand->SetCallbackFlags( CALLBACK_GLOBAL_COLLISION | CALLBACK_SHADOW_COLLISION );

			// create crouchig hull
			m_pShadowCrouch = PhysModelCreateCustom( this, PhysCreateBbox( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX ), GetLocalOrigin(), GetLocalAngles(), "player_crouch", false, &solid );
			m_pShadowCrouch->SetCallbackFlags( CALLBACK_GLOBAL_COLLISION | CALLBACK_SHADOW_COLLISION );

			// default to stand
			VPhysicsSetObject( m_pShadowStand );

			// tell physics lists I'm a shadow controller object
			PhysAddShadow( this );	
			m_pPhysicsController = physenv->CreatePlayerController( m_pShadowStand );
			m_pPhysicsController->SetPushMassLimit( 350.0f );
			m_pPhysicsController->SetPushSpeedLimit( 50.0f );

			// Give the controller a valid position so it doesn't do anything rash.
			UpdatePhysicsShadowToPosition( vecAbsOrigin );

			// init state
			if ( GetFlags() & FL_DUCKING )
			{
				SetVCollisionState( vecAbsOrigin, vecAbsVelocity, VPHYS_CROUCH );
			}
			else
			{
				SetVCollisionState( vecAbsOrigin, vecAbsVelocity, VPHYS_WALK );
			}
		}
	}

	if( IsLocalPlayer() )
	{
		IPhysicsObject *pObject = VPhysicsGetObject();
		if ((pObject != NULL) &&
			(
				((m_vphysicsCollisionState == VPHYS_CROUCH) && (pObject == m_pShadowStand)) ||
				((m_vphysicsCollisionState == VPHYS_WALK) && (pObject == m_pShadowCrouch))
			) )
		{
			//apply networked changes to collision state
			SetVCollisionState( GetAbsOrigin(), GetAbsVelocity(), m_vphysicsCollisionState );
		}

		// [msmith] We want to disable prediction while spectating in first person because the server forcibly sets certain predicted fields on players while
		// they are spectating other players (viewOffset for example).  Keeping prediction enabled modifies these values and causes severe jitter.
		// This is most noticable while playing on a dedicated server and the specated player ducks (causing changes to viewOffset).
		int observationMode = GetObserverMode();
		bool isSpectating = GetObserverTarget() != NULL && ( observationMode == OBS_MODE_IN_EYE || observationMode == OBS_MODE_CHASE );
		extern bool g_bSpectatingForceCLPredictOff;
		g_bSpectatingForceCLPredictOff = isSpectating;
	}

}


//-----------------------------------------------------------------------------
// Did we just enter a vehicle this frame?
//-----------------------------------------------------------------------------
bool C_BasePlayer::JustEnteredVehicle()
{
	if ( !IsInAVehicle() )
		return false;

	return ( m_hOldVehicle == m_hVehicle );
}

//-----------------------------------------------------------------------------
// Are we in VGUI input mode?.
//-----------------------------------------------------------------------------
bool C_BasePlayer::IsInVGuiInputMode() const
{
	return (m_pCurrentVguiScreen.Get() != NULL);
}

//-----------------------------------------------------------------------------
// Are we inputing to a view model vgui screen
//-----------------------------------------------------------------------------
bool C_BasePlayer::IsInViewModelVGuiInputMode() const
{
	C_BaseEntity *pScreenEnt = m_pCurrentVguiScreen.Get();

	if ( !pScreenEnt )
		return false;

	Assert( dynamic_cast<C_VGuiScreen*>(pScreenEnt) );
	C_VGuiScreen *pVguiScreen = static_cast<C_VGuiScreen*>(pScreenEnt);

	return ( pVguiScreen->IsAttachedToViewModel() && pVguiScreen->AcceptsInput() );
}

//-----------------------------------------------------------------------------
// Check to see if we're in vgui input mode...
//-----------------------------------------------------------------------------
void C_BasePlayer::DetermineVguiInputMode( CUserCmd *pCmd )
{
	// If we're dead, close down and abort!
	if ( !IsAlive() )
	{
		DeactivateVguiScreen( m_pCurrentVguiScreen.Get() );
		m_pCurrentVguiScreen.Set( NULL );
		return;
	}

	// If we're in vgui mode *and* we're holding down mouse buttons,
	// stay in vgui mode even if we're outside the screen bounds
	if ( m_pCurrentVguiScreen.Get() && ( pCmd->buttons & ( IN_ATTACK | IN_ATTACK2 | IN_ZOOM ) ) )
	{
		SetVGuiScreenButtonState( m_pCurrentVguiScreen.Get(), pCmd->buttons );

		// Kill all attack inputs if we're in vgui screen mode
		pCmd->buttons &= ~( IN_ATTACK | IN_ATTACK2 | IN_ZOOM );
		return;
	}

	// We're not in vgui input mode if we're moving, or have hit a key
	// that will make us move...

	// Not in vgui mode if we're moving too quickly
	// ROBIN: Disabled movement preventing VGUI screen usage
	//if (GetVelocity().LengthSqr() > MAX_VGUI_INPUT_MODE_SPEED_SQ)
	if ( 0 )
	{
		DeactivateVguiScreen( m_pCurrentVguiScreen.Get() );
		m_pCurrentVguiScreen.Set( NULL );
		return;
	}

	// Don't enter vgui mode if we've got combat buttons held down
	bool bAttacking = false;
	if ( (( pCmd->buttons & IN_ATTACK ) || ( pCmd->buttons & IN_ATTACK2 ) || ( pCmd->buttons & IN_ZOOM ) ) && !m_pCurrentVguiScreen.Get() )
	{
		bAttacking = true;
	}

	// Not in vgui mode if we're pushing any movement key at all
	// Not in vgui mode if we're in a vehicle...
	// ROBIN: Disabled movement preventing VGUI screen usage
	//if ((pCmd->forwardmove > MAX_VGUI_INPUT_MODE_SPEED) ||
	//	(pCmd->sidemove > MAX_VGUI_INPUT_MODE_SPEED) ||
	//	(pCmd->upmove > MAX_VGUI_INPUT_MODE_SPEED) ||
	//	(pCmd->buttons & IN_JUMP) ||
	//	(bAttacking) )
	if ( bAttacking || IsInAVehicle() )
	{ 
		DeactivateVguiScreen( m_pCurrentVguiScreen.Get() );
		m_pCurrentVguiScreen.Set( NULL );
		return;
	}

	// Don't interact with world screens when we're in a menu
	if ( vgui::surface()->IsCursorVisible() )
	{
		DeactivateVguiScreen( m_pCurrentVguiScreen.Get() );
		m_pCurrentVguiScreen.Set( NULL );
		return;
	}

	// Not in vgui mode if there are no nearby screens
	C_BaseEntity *pOldScreen = m_pCurrentVguiScreen.Get();

	m_pCurrentVguiScreen = FindNearbyVguiScreen( EyePosition(), pCmd->viewangles, GetTeamNumber() );

	if (pOldScreen != m_pCurrentVguiScreen)
	{
		DeactivateVguiScreen( pOldScreen );
		ActivateVguiScreen( m_pCurrentVguiScreen.Get() );
	}

	if (m_pCurrentVguiScreen.Get())
	{
		SetVGuiScreenButtonState( m_pCurrentVguiScreen.Get(), pCmd->buttons );

		// Kill all attack inputs if we're in vgui screen mode
		pCmd->buttons &= ~(IN_ATTACK | IN_ATTACK2 | IN_ZOOM);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handling
//-----------------------------------------------------------------------------
bool C_BasePlayer::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	// Allow the vehicle to clamp the view angles
	if ( IsInAVehicle() )
	{
		IClientVehicle *pVehicle = m_hVehicle.Get()->GetClientVehicle();
		if ( pVehicle )
		{
			pVehicle->UpdateViewAngles( this, pCmd );
			engine->SetViewAngles( pCmd->viewangles );
		}
	}
	else 
	{
#ifndef _GAMECONSOLE
		if ( joy_autosprint.GetBool() )
#endif
		{
			if ( input->KeyState( &in_joyspeed ) != 0.0f )
			{
				pCmd->buttons |= IN_SPEED;
			}
		}

		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		if ( pWeapon )
		{
			pWeapon->CreateMove( flInputSampleTime, pCmd, m_vecOldViewAngles );
		}
	}

	// If the frozen flag is set, prevent view movement (server prevents the rest of the movement)
	if ( GetFlags() & FL_FROZEN )
	{
		// Don't stomp the first time we get frozen
		if ( m_bWasFrozen )
		{
			// Stomp the new viewangles with old ones
			pCmd->viewangles = m_vecOldViewAngles;
			engine->SetViewAngles( pCmd->viewangles );
		}
		else
		{
			m_bWasFrozen = true;
		}
	}
	else
	{
		m_bWasFrozen = false;
	}

	m_vecOldViewAngles = pCmd->viewangles;
	
	// Check to see if we're in vgui input mode...
	DetermineVguiInputMode( pCmd );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Player has changed to a new team
//-----------------------------------------------------------------------------
void C_BasePlayer::TeamChange( int iNewTeam )
{
	// Base class does nothing
}


//-----------------------------------------------------------------------------
// Purpose: Creates, destroys, and updates the flashlight effect as needed.
//-----------------------------------------------------------------------------
void C_BasePlayer::UpdateFlashlight()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int iSsPlayer = GET_ACTIVE_SPLITSCREEN_SLOT();

	// TERROR: if we're in-eye spectating, use that player's flashlight
	C_BasePlayer *pFlashlightPlayer = this;
	if ( !IsAlive() )
	{
		if ( GetObserverMode() == OBS_MODE_IN_EYE )
		{
			pFlashlightPlayer = ToBasePlayer( GetObserverTarget() );
		}
	}

	if ( pFlashlightPlayer )
	{
		FlashlightEffectManager().SetEntityIndex( pFlashlightPlayer->index );
	}

	// The dim light is the flashlight.
	if ( pFlashlightPlayer && pFlashlightPlayer->IsAlive() && pFlashlightPlayer->IsEffectActive( EF_DIMLIGHT ) && !pFlashlightPlayer->GetViewEntity() )
	{
		// Make sure we're using the proper flashlight texture
		const char *pszTextureName = pFlashlightPlayer->GetFlashlightTextureName();
		if ( !m_bFlashlightEnabled[ iSsPlayer ] )
		{
			// Turned on the headlight; create it.
			if ( pszTextureName )
			{
				FlashlightEffectManager().TurnOnFlashlight( pFlashlightPlayer->index, pszTextureName, pFlashlightPlayer->GetFlashlightFOV(),
					pFlashlightPlayer->GetFlashlightFarZ(), pFlashlightPlayer->GetFlashlightLinearAtten() );
			}
			else
			{
				FlashlightEffectManager().TurnOnFlashlight( pFlashlightPlayer->index );
			}
			m_bFlashlightEnabled[ iSsPlayer ] = true;
		}
	}
	else if ( m_bFlashlightEnabled[ iSsPlayer ] )
	{
		// Turned off the flashlight; delete it.
		FlashlightEffectManager().TurnOffFlashlight();
		m_bFlashlightEnabled[ iSsPlayer ] = false;
	}

	if ( pFlashlightPlayer && m_bFlashlightEnabled[ iSsPlayer ] )
	{
		Vector vecForward, vecRight, vecUp;
		Vector vecPos;
		//Check to see if we have an externally specified flashlight origin, if not, use eye vectors/render origin
		if ( pFlashlightPlayer->m_vecFlashlightOrigin != vec3_origin && pFlashlightPlayer->m_vecFlashlightOrigin.IsValid() )
		{
			vecPos = pFlashlightPlayer->m_vecFlashlightOrigin;
			vecForward = pFlashlightPlayer->m_vecFlashlightForward;
			vecRight = pFlashlightPlayer->m_vecFlashlightRight;
			vecUp = pFlashlightPlayer->m_vecFlashlightUp;
		}
		else
		{
			EyeVectors( &vecForward, &vecRight, &vecUp );
			vecPos = GetRenderOrigin() + m_vecViewOffset;
		}

		// Update the light with the new position and direction.		
		FlashlightEffectManager().UpdateFlashlight( vecPos, vecForward, vecRight, vecUp, pFlashlightPlayer->GetFlashlightFOV(), 
			pFlashlightPlayer->CastsFlashlightShadows(), pFlashlightPlayer->GetFlashlightFarZ(), pFlashlightPlayer->GetFlashlightLinearAtten(),
			pFlashlightPlayer->GetFlashlightTextureName() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Creates player flashlight if it's active
//-----------------------------------------------------------------------------
void C_BasePlayer::Flashlight( void )
{
	UpdateFlashlight();
}


//-----------------------------------------------------------------------------
// Purpose: Turns off flashlight if it's active (TERROR)
//-----------------------------------------------------------------------------
void C_BasePlayer::TurnOffFlashlight( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if ( m_bFlashlightEnabled[ nSlot ] )
	{
		FlashlightEffectManager().TurnOffFlashlight();
		m_bFlashlightEnabled[ nSlot ] = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BasePlayer::CreateWaterEffects( void )
{
	// Must be completely submerged to bother
	if ( GetWaterLevel() < WL_Eyes )
	{
		m_bResampleWaterSurface = true;
		return;
	}

	// Do special setup if this is our first time back underwater
	if ( m_bResampleWaterSurface )
	{
		// Reset our particle timer
		m_tWaterParticleTimer.Init( 32 );
		
		// Find the surface of the water to clip against
		m_flWaterSurfaceZ = UTIL_WaterLevel( WorldSpaceCenter(), WorldSpaceCenter().z, WorldSpaceCenter().z + 256 );
		m_bResampleWaterSurface = false;
	}

	// Make sure the emitter is setup
	if ( m_pWaterEmitter == NULL )
	{
		if ( ( m_pWaterEmitter = WaterDebrisEffect::Create( "splish" ) ) == NULL )
			return;
	}

	Vector vecVelocity;
	GetVectors( &vecVelocity, NULL, NULL );

	Vector offset = WorldSpaceCenter();

	m_pWaterEmitter->SetSortOrigin( offset );

	SimpleParticle	*pParticle;

	float curTime = gpGlobals->frametime;

	// Add as many particles as we need
	while ( m_tWaterParticleTimer.NextEvent( curTime ) )
	{
		offset = WorldSpaceCenter() + ( vecVelocity * 128.0f ) + RandomVector( -128, 128 );

		// Make sure we don't start out of the water!
		if ( offset.z > m_flWaterSurfaceZ )
		{
			offset.z = ( m_flWaterSurfaceZ - 8.0f );
		}

		pParticle = (SimpleParticle *) m_pWaterEmitter->AddParticle( sizeof(SimpleParticle), g_Mat_Fleck_Cement[random->RandomInt(0,1)], offset );

		if (pParticle == NULL)
			continue;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random->RandomFloat( 2.0f, 4.0f );

		pParticle->m_vecVelocity = RandomVector( -2.0f, 2.0f );

		//FIXME: We should tint these based on the water's fog value!
		float color = random->RandomInt( 32, 128 );
		pParticle->m_uchColor[0] = color;
		pParticle->m_uchColor[1] = color;
		pParticle->m_uchColor[2] = color;

		pParticle->m_uchStartSize	= 1;
		pParticle->m_uchEndSize		= 1;
		
		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 0;
		
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -0.5f, 0.5f );
	}
}

//-----------------------------------------------------------------------------
// Called when not in tactical mode. Allows view to be overriden for things like driving a tank.
//-----------------------------------------------------------------------------
void C_BasePlayer::OverrideView( CViewSetup *pSetup )
{
}

bool C_BasePlayer::IsCameraMan() const 
{ 
	return IsActiveCameraMan(); //( HLTVCamera() && ( HLTVCamera()->GetCameraMan() == this ) );
}

bool C_BasePlayer::ShouldInterpolate()
{
	// always interpolate myself
	if ( IsLocalPlayer( this ) )
		return true;

	// always interpolate entity if followed by HLTV/Replay
#if defined( REPLAY_ENABLED )
	if ( HLTVCamera()->GetCameraMan() == this ||
		 ReplayCamera()->GetCameraMan() == this )
		return true;
#else
	if ( HLTVCamera()->GetCameraMan() == this )
		return true;
#endif

	return BaseClass::ShouldInterpolate();
}

bool C_BasePlayer::ShouldDraw()
{
	// $FIXME(hpe) this was returning false in splitscreen mode making 2nd player invisible
#if defined (_GAMECONSOLE) && defined ( CSTRIKE15 )
	ConVarRef ss_enable( "ss_enable" );
	if ( ss_enable.GetInt() > 0 )
	{
		return ( IsLocalSplitScreenPlayer() || this != GetSplitScreenViewPlayer() || C_BasePlayer::ShouldDrawLocalPlayer() || (GetObserverMode() == OBS_MODE_DEATHCAM ) ) &&
			   BaseClass::ShouldDraw();
	}
#endif
	return ( this != GetSplitScreenViewPlayer() || C_BasePlayer::ShouldDrawLocalPlayer() || (GetObserverMode() == OBS_MODE_DEATHCAM ) ) &&
		   BaseClass::ShouldDraw();
}

int C_BasePlayer::DrawModel( int flags, const RenderableInstance_t &instance )
{
	// if local player is spectating this player in first person mode, don't draw it
	C_BasePlayer * player = C_BasePlayer::GetLocalPlayer();

	if ( player && player->IsObserver() )
	{
		if ( player->GetObserverMode() == OBS_MODE_IN_EYE &&
			player->GetObserverTarget() == this &&
			!input->CAM_IsThirdPerson() && 
			player->GetObserverInterpState() != OBSERVER_INTERP_TRAVELING )
			return 0;
	}

	return BaseClass::DrawModel( flags, instance );
}

bool C_BasePlayer::ShouldSuppressForSplitScreenPlayer( int nSlot )
{
	if ( BaseClass::ShouldSuppressForSplitScreenPlayer( nSlot ) )
		return true;

	PlayerRenderMode_t nMode = GetPlayerRenderMode( nSlot );
	return ( nMode == PLAYER_RENDER_FIRSTPERSON );
}

//-----------------------------------------------------------------------------
// Computes the render mode for this player
//-----------------------------------------------------------------------------
PlayerRenderMode_t C_BasePlayer::GetPlayerRenderMode( int nSlot )
{
	// check if local player chases owner of this weapon in first person
	C_BasePlayer *pSplitscreenPlayer = GetSplitScreenViewPlayer( nSlot );
	if ( !pSplitscreenPlayer )
		return PLAYER_RENDER_THIRDPERSON;

	if ( pSplitscreenPlayer->IsObserver() )
	{
		if ( pSplitscreenPlayer->GetObserverTarget() != this )
			return PLAYER_RENDER_THIRDPERSON;
		if ( pSplitscreenPlayer->GetObserverMode() != OBS_MODE_IN_EYE )
			return PLAYER_RENDER_THIRDPERSON;
	}
	else
	{
		if ( pSplitscreenPlayer != this )
			return PLAYER_RENDER_THIRDPERSON;
	}

	if ( input->CAM_IsThirdPerson( nSlot ) )
		return PLAYER_RENDER_THIRDPERSON;

	if ( (pSplitscreenPlayer->GetViewEntity() != NULL) && 
		(pSplitscreenPlayer->GetViewEntity() != pSplitscreenPlayer) &&
		pSplitscreenPlayer->m_bShouldDrawPlayerWhileUsingViewEntity )
		return PLAYER_RENDER_THIRDPERSON;

//	if ( IsInThirdPersonView() )
//		return PLAYER_RENDER_THIRDPERSON;

	return PLAYER_RENDER_FIRSTPERSON;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector C_BasePlayer::GetChaseCamViewOffset( CBaseEntity *target )
{
	C_BasePlayer *player = ToBasePlayer( target );
	
	if ( player && player->IsAlive() )
	{
		if( player->GetFlags() & FL_DUCKING )
			return VEC_DUCK_VIEW;

		return VEC_VIEW;
	}

#ifdef CSTRIKE_DLL
	CBaseCSGrenadeProjectile *pGrenade = dynamic_cast< CBaseCSGrenadeProjectile* >( target );
	if ( pGrenade )
		return Vector( 0, 0, 8 );
#endif

	// assume it's the players ragdoll
	return VEC_DEAD_VIEWHEIGHT;
}

void C_BasePlayer::CalcChaseCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	C_BaseEntity *target = GetObserverTarget();

	if ( !target ) 
	{
		// just copy a save in-map position
		VectorCopy( EyePosition(), eyeOrigin );
		VectorCopy( EyeAngles(), eyeAngles );
		return;
	};

	// If our target isn't visible, we're at a camera point of some kind.
	// Instead of letting the player rotate around an invisible point, treat
	// the point as a fixed camera.
	if ( !target->GetBaseAnimating() && !target->GetModel() )
	{
		CalcRoamingView( eyeOrigin, eyeAngles, fov );
		return;
	}

	
/*#ifdef CSTRIKE_DLL
	// weapon gun-cam go-pro chase camera
	C_CSPlayer *pPlayer = ToCSPlayer( target );
	if ( pPlayer && pPlayer->ShouldDraw() )
	{
		Vector vecSrc = target->GetObserverCamOrigin();
		VectorAdd( vecSrc, GetChaseCamViewOffset( target ), vecSrc );

		Vector vecObsForward, vecObsRight, vecObsUp;
		AngleVectors( EyeAngles(), &vecObsForward, &vecObsRight, &vecObsUp );

		trace_t playerEyeTrace;
		UTIL_TraceLine( vecSrc, vecSrc - vecObsForward * 75.0f, MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &playerEyeTrace );
		
		float flDistMax = playerEyeTrace.startpos.DistTo( playerEyeTrace.endpos + vecObsForward * 4 );
		m_flObserverChaseApproach = (m_flObserverChaseApproach >= flDistMax) ? flDistMax : Approach( 75.0f, m_flObserverChaseApproach, gpGlobals->frametime * 20 );

		Vector vecIdealCamEyePos = vecSrc - vecObsForward * m_flObserverChaseApproach;
		Vector vecIdealCamTargetPos = vecSrc + vecObsRight * RemapValClamped( m_flObserverChaseApproach, 20, 75, 6, 16 ) * abs(DotProduct(vecObsUp,Vector(0,0,1)));
		VectorAngles( (vecIdealCamTargetPos - vecIdealCamEyePos ).Normalized(), eyeAngles );

		eyeOrigin = vecIdealCamEyePos;
		return;
	}
#endif*/
	


#ifdef CSTRIKE_DLL
	CBaseCSGrenadeProjectile *pGrenade = dynamic_cast< CBaseCSGrenadeProjectile* >( target );
#endif

	// QAngle tmpangles;

	Vector forward, viewpoint;

	// GetObserverCamOrigin() returns ragdoll pos if player is ragdolled
	Vector origin = target->GetObserverCamOrigin();

	VectorAdd( origin, GetChaseCamViewOffset( target ), origin );

	QAngle viewangles;

	if ( GetObserverMode() == OBS_MODE_IN_EYE )
	{
		viewangles = eyeAngles;
	}
#ifdef CSTRIKE_DLL
	else if ( pGrenade && pGrenade->m_nBounces <= 0  )
	{
		Vector vecVel = pGrenade->GetLocalVelocity();
		VectorAngles( vecVel, viewangles );
	}
#endif
	else if ( IsLocalPlayer( this ) )
	{
		engine->GetViewAngles( viewangles );
	}
	else
	{
		viewangles = EyeAngles();
	}

	// [Forrest] Spectating someone who is headshotted by a teammate and then switching to chase cam leaves
	// you with a permanent roll to the camera that doesn't decay and is not reset
	// even when switching to different players or at the start of the next round
	// if you are still a spectator.  (If you spawn as a player, the view is reset.
	// if you switch spectator modes, the view is reset.)
#ifdef CSTRIKE_DLL
	// The chase camera adopts the yaw and pitch of the previous camera, but the camera
	// should not roll.
	viewangles.z = 0;
#endif

	m_flObserverChaseDistance += gpGlobals->frametime*48.0f;

	float flMaxDistance = CHASE_CAM_DISTANCE;
	if ( target && target->IsBaseTrain() )
	{
		// if this is a train, we want to be back a little further so we can see more of it
		flMaxDistance *= 2.5f;
	}
	else if ( pGrenade )
	{
		flMaxDistance = 64.0f;
	}
	m_flObserverChaseDistance = clamp( m_flObserverChaseDistance, 16, flMaxDistance );
	
	AngleVectors( viewangles, &forward );

	VectorNormalize( forward );

	VectorMA(origin, -m_flObserverChaseDistance, forward, viewpoint );

	trace_t trace;
	CTraceFilterNoNPCsOrPlayer filter( target, COLLISION_GROUP_NONE );
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace

	Vector hullMin = WALL_MIN, hullMax = WALL_MAX;

#ifdef CSTRIKE_DLL
	if ( target && Q_strcmp( target->GetClassname(), "class C_PlantedC4" ) == 0 )
	{
		hullMin *= 2.f; hullMax *= 2.f; 
	}
#endif

	UTIL_TraceHull( origin, viewpoint, hullMin, hullMax, MASK_SOLID, &filter, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();

	if (trace.fraction < 1.0)
	{
		viewpoint = trace.endpos;
		m_flObserverChaseDistance = VectorLength(origin - eyeOrigin);
	}
#ifdef CSTRIKE_DLL
	else if ( pGrenade )
	{
		m_flObserverChaseDistance = MAX( 64, m_flObserverChaseDistance );
	}
#endif

	VectorCopy( viewangles, eyeAngles );
	VectorCopy( viewpoint, eyeOrigin );

	fov = GetFOV();
}

void C_BasePlayer::CalcRoamingView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
#ifdef CSTRIKE_DLL
	// in CS, we can have a spec target, but in roaming, we always want to roam FREE!!!  despite the target
	C_BaseEntity *target = this;
#else
	C_BaseEntity *target = GetObserverTarget();
	
	if ( !target ) 
	{
		target = this;
	}
#endif

	m_flObserverChaseDistance = 0.0;

	eyeOrigin = target->EyePosition();
	eyeAngles = target->EyeAngles();

	if ( spec_track.GetInt() > 0 )
	{
		C_BaseEntity *target =  ClientEntityList().GetBaseEntity( spec_track.GetInt() );

		if ( target )
		{
			Vector v = target->GetAbsOrigin(); v.z += 54;
			QAngle a; VectorAngles( v - eyeOrigin, a );

			NormalizeAngles( a );
			eyeAngles = a;
			engine->SetViewAngles( a );
		}
	}

#ifdef CSTRIKE_DLL
	if ( GetObserverMode() == OBS_MODE_FIXED )
	{
		Vector viewpoint;
		// if we are in a fixed position, do a very simple check to make sure we aren't fixed inside the world below us
		trace_t trace;
		CTraceFilterNoNPCsOrPlayer filter( target, COLLISION_GROUP_NONE );
		C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace

		Vector hullMin = Vector( -2, -2, -24 ), hullMax = Vector( 2, 2, 8 );
		UTIL_TraceHull( eyeOrigin + Vector( 0, 0, 42 ), eyeOrigin + Vector( 0, 0, -16 ), hullMin, hullMax, MASK_SOLID, &filter, &trace );
		C_BaseEntity::PopEnableAbsRecomputations();

		if (trace.fraction < 1.0)
		{
			viewpoint = trace.endpos;
			VectorCopy( viewpoint, eyeOrigin );
		}
	}
#endif

	// Apply a smoothing offset to smooth out prediction errors.
	Vector vSmoothOffset;
	GetPredictionErrorSmoothingVector( vSmoothOffset );
	eyeOrigin += vSmoothOffset;

	fov = GetFOV();
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the view for the player while he's in freeze frame observer mode
//-----------------------------------------------------------------------------
void C_BasePlayer::CalcFreezeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	C_BaseEntity *pTarget = GetObserverTarget();
	if ( !pTarget )
	{
		CalcDeathCamView( eyeOrigin, eyeAngles, fov );
		return;
	}

	if ( !m_bStartedFreezeFrame )
	{
		m_bStartedFreezeFrame = true;
	}

	// Zoom towards our target
	float flCurTime = (gpGlobals->curtime - m_flFreezeFrameStartTime);
	float flBlendPerc = 0.0f;
	
	if ( !g_HltvReplaySystem.IsDelayedReplayRequestPending() ) // if we auto-start replay, we don't want to layer camera motion on top of fade on top of replay scene cut
	{
		clamp( flCurTime / spec_freeze_traveltime.GetFloat(), 0, 1 );
		//Msg( "Freezecam @%.2f\n", flCurTime / spec_freeze_traveltime.GetFloat() ); // replayfade
		flBlendPerc = SimpleSpline( flBlendPerc );
	}

	Vector vecCamDesired = pTarget->GetObserverCamOrigin();	// Returns ragdoll origin if they're ragdolled
	VectorAdd( vecCamDesired, GetChaseCamViewOffset( pTarget ), vecCamDesired );
	Vector vecCamTarget = vecCamDesired;
	if ( pTarget->IsAlive() )
	{
		// Look at their chest, not their head
		Vector maxs = GameRules()->GetViewVectors()->m_vHullMax;
		vecCamTarget.z -= (maxs.z * 0.5);
	}
	else
	{
		vecCamTarget.z += VEC_DEAD_VIEWHEIGHT.z;	// look over ragdoll, not through
	}

	// Figure out a view position in front of the target
	Vector vecEyeOnPlane = eyeOrigin;
	vecEyeOnPlane.z = vecCamTarget.z;
	Vector vecTargetPos = vecCamTarget;
	Vector vecToTarget = vecTargetPos - vecEyeOnPlane;
	VectorNormalize( vecToTarget );

	// Stop a few units away from the target, and shift up to be at the same height
	vecTargetPos = vecCamTarget - (vecToTarget * m_flFreezeFrameDistance);
	float flEyePosZ = pTarget->EyePosition().z;
	vecTargetPos.z = flEyePosZ + m_flFreezeZOffset;

	// Now trace out from the target, so that we're put in front of any walls
	trace_t trace;
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( vecCamTarget, vecTargetPos, WALL_MIN, WALL_MAX, MASK_SOLID, pTarget, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();
	if (trace.fraction < 1.0)
	{
		// The camera's going to be really close to the target. So we don't end up
		// looking at someone's chest, aim close freezecams at the target's eyes.
		vecTargetPos = trace.endpos;
		vecCamTarget = vecCamDesired;

		// To stop all close in views looking up at character's chins, move the view up.
		vecTargetPos.z += fabs(vecCamTarget.z - vecTargetPos.z) * 0.85;
		C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
		UTIL_TraceHull( vecCamTarget, vecTargetPos, WALL_MIN, WALL_MAX, MASK_SOLID, pTarget, COLLISION_GROUP_NONE, &trace );
		C_BaseEntity::PopEnableAbsRecomputations();
		vecTargetPos = trace.endpos;
	}

	// Look directly at the target
	vecToTarget = vecCamTarget - vecTargetPos;
	VectorNormalize( vecToTarget );
	VectorAngles( vecToTarget, eyeAngles );
	
	VectorLerp( m_vecFreezeFrameStart, vecTargetPos, flBlendPerc, eyeOrigin );

	if ( flCurTime >= spec_freeze_traveltime.GetFloat() && !m_bSentFreezeFrame )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "freezecam_started" );
		if ( pEvent )
		{
			gameeventmanager->FireEventClientSide( pEvent );
		}

		m_bSentFreezeFrame = true;
		view->FreezeFrame( spec_freeze_time.GetFloat() );
	}
}

void C_BasePlayer::CalcInEyeCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	C_BaseEntity *target = GetObserverTarget();

	if ( !target ) 
	{
		// just copy a save in-map position
		VectorCopy( EyePosition(), eyeOrigin );
		VectorCopy( EyeAngles(), eyeAngles );
		return;
	};

	if ( !target->IsAlive() )
	{
		// if dead, show from 3rd person
		CalcChaseCamView( eyeOrigin, eyeAngles, fov );
		return;
	}

	fov = GetFOV();	// TODO use tragets FOV

	m_flObserverChaseDistance = 0.0;

	eyeAngles = target->EyeAngles();
	eyeOrigin = target->GetAbsOrigin();
		
	CalcViewBob( eyeOrigin );
	CalcViewRoll( eyeAngles );

	CalcAddViewmodelCameraAnimation( eyeOrigin, eyeAngles );

	// Apply punch angle
	VectorAdd( eyeAngles, GetViewPunchAngle(), eyeAngles );

	// Apply aim punch angle
	VectorAdd( eyeAngles, GetAimPunchAngle() * view_recoil_tracking.GetFloat(), eyeAngles );

#if defined( REPLAY_ENABLED )
	if( g_bEngineIsHLTV || engine->IsReplay() )
#else
	if( g_bEngineIsHLTV )
#endif
	{
		if ( target->GetFlags() & FL_DUCKING )
		{
			eyeOrigin += VEC_DUCK_VIEW;
		}
		else
		{
			eyeOrigin += VEC_VIEW;
		}
	}
	else
	{
		Vector offset = m_vecViewOffset;
		eyeOrigin += offset; // hack hack
	}

	engine->SetViewAngles( eyeAngles );
}

float C_BasePlayer::GetDeathCamInterpolationTime()
{
	return DEATH_ANIMATION_TIME;
}


void C_BasePlayer::CalcDeathCamView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	CBaseEntity	* pKiller = NULL; 

	if ( mp_forcecamera.GetInt() == OBS_ALLOW_ALL )
	{
		// if mp_forcecamera is off let user see killer or look around
		pKiller = GetObserverTarget();
		eyeAngles = EyeAngles();
	}

	float interpolation = ( gpGlobals->curtime - m_flDeathTime ) / GetDeathCamInterpolationTime();
	interpolation = clamp( interpolation, 0.0f, 1.0f );

	m_flObserverChaseDistance += gpGlobals->frametime*48.0f;
	m_flObserverChaseDistance = clamp( m_flObserverChaseDistance, 16, CHASE_CAM_DISTANCE );

	QAngle aForward = eyeAngles;
	Vector origin = EyePosition();			

	IRagdoll *pRagdoll = GetRepresentativeRagdoll();
	if ( pRagdoll )
	{
		origin = pRagdoll->GetRagdollOrigin();
		origin.z += VEC_DEAD_VIEWHEIGHT.z; // look over ragdoll, not through
	}
	
	if ( pKiller && pKiller->IsPlayer() && (pKiller != this) ) 
	{
		Vector vKiller = pKiller->EyePosition() - origin;
		QAngle aKiller; VectorAngles( vKiller, aKiller );
		InterpolateAngles( aForward, aKiller, eyeAngles, interpolation );
	};

	Vector vForward; AngleVectors( eyeAngles, &vForward );

	VectorNormalize( vForward );

	VectorMA( origin, -m_flObserverChaseDistance, vForward, eyeOrigin );

	trace_t trace; // clip against world
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( origin, eyeOrigin, WALL_MIN, WALL_MAX, MASK_SOLID, this, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();

	if (trace.fraction < 1.0)
	{
		eyeOrigin = trace.endpos;
		m_flObserverChaseDistance = VectorLength(origin - eyeOrigin);
	}

	fov = GetFOV();
}



//-----------------------------------------------------------------------------
// Purpose: Return the weapon to have open the weapon selection on, based upon our currently active weapon
//			Base class just uses the weapon that's currently active.
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *C_BasePlayer::GetActiveWeaponForSelection( void )
{
	return GetActiveWeapon();
}

C_BaseAnimating* C_BasePlayer::GetRenderedWeaponModel()
{
	// Attach to either their weapon model or their view model.
	if ( ShouldDrawLocalPlayer() || !IsLocalPlayer( this ) )
	{
		return GetActiveWeapon();
	}
	else
	{
		return GetViewModel();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets a pointer to the local player, if it exists yet.
// static method
//-----------------------------------------------------------------------------
C_BasePlayer *C_BasePlayer::GetLocalPlayer( int nSlot /*= -1*/ )
{
	if ( nSlot == -1 )
	{
//		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return s_pLocalPlayer[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return s_pLocalPlayer[ nSlot ];
}

void C_BasePlayer::SetRemoteSplitScreenPlayerViewsAreLocalPlayer( bool bSet )
{
	for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if( !IsLocalSplitScreenPlayer( i ) )
		{
			s_pLocalPlayer[i] = bSet ? GetSplitScreenViewPlayer( i ) : NULL;
		}
	}
}

bool C_BasePlayer::HasAnyLocalPlayer()
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		if ( s_pLocalPlayer[ i ] )
			return true;
	}
	return false;
}

int C_BasePlayer::GetSplitScreenSlotForPlayer( C_BaseEntity *pl )
{
	C_BasePlayer *pPlayer = ToBasePlayer( pl );

	if ( !pPlayer )
	{
		Assert( 0 );
		return -1;
	}
	return pPlayer->GetSplitScreenPlayerSlot();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bThirdperson - 
//-----------------------------------------------------------------------------
void C_BasePlayer::ThirdPersonSwitch( bool bThirdperson )
{
	// We've switch from first to third, or vice versa.
	UpdateVisibility();
}

//-----------------------------------------------------------------------------
// Purpose: single place to decide whether the local player should draw
//-----------------------------------------------------------------------------
bool C_BasePlayer::ShouldDrawLocalPlayer()
{
	int nSlot = GetSplitScreenPlayerSlot();

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );

#if !defined( SPLIT_SCREEN_STUBS )
	nSlot = GetSplitScreenPlayerSlot();
#endif

#ifdef PORTAL2
	if( !IsLocalSplitScreenPlayer( (nSlot == -1) ? GET_ACTIVE_SPLITSCREEN_SLOT() : nSlot ) ) //HACKHACK: shortcut, avoid going into input and getting a bunch of asserts if the splitscreen view is not a local player
		return false;
#endif

	return ( GetPlayerRenderMode(nSlot) == PLAYER_RENDER_THIRDPERSON ) || input->CAM_IsThirdPerson() || ( ToolsEnabled() && ToolFramework_IsThirdPersonCamera() );
}

//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
IClientModelRenderable *C_BasePlayer::GetClientModelRenderable()
{

#if defined ( CSTRIKE15 )// Since cstrike15 does not do glow, we can go ahead and use fast path for teammates.
	
	// We can enable mostly opaque models to cause players to be rendered in both the opaque and the translucent fast paths
	// allowing both alpha and non alpha materials to show up.
	// However, since the bounding boxes are different for these "sub models" they have sorting issues when the player
	// is inside of a smoke cloud such that the alpha components sort in front of the smoke cloud.
	// Because of this, we no longer use two passes for players and instead cause all players to NOT use the fast path 
	// rendering by returning NULL here.
	return NULL;

#endif

	// Because of alpha sorting issues with smoke when we have mostlyopaque models.
	// Honor base class eligibility
	if ( !BaseClass::GetClientModelRenderable() )
		return NULL;

	// No fast path for firstperson local players
	if ( IsLocalPlayer( this ) )
	{
		bool bThirdPerson = input->CAM_IsThirdPerson() || ( ToolsEnabled() && ToolFramework_IsThirdPersonCamera() );
		if ( !bThirdPerson )
		{
			return NULL;
		}
	}

	// if local player is spectating this player in first person mode, don't use fast path, so we can skip drawing it
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( localPlayer && localPlayer->IsObserver() )
	{
		if ( localPlayer->GetObserverMode() == OBS_MODE_IN_EYE &&
			localPlayer->GetObserverTarget() == this &&
			!input->CAM_IsThirdPerson() )
			return NULL;
	}

	// Probably for the left 4 dead code.
	// don't use fastpath for teammates (causes extra work for glows)
	if ( localPlayer && localPlayer->GetTeamNumber() == GetTeamNumber() )
	{
		return NULL;
	}

	return this; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BasePlayer::IsLocalPlayer( const C_BaseEntity *pEntity )
{
	if ( !pEntity || 
		 !pEntity->IsPlayer() )
		return false;

	return static_cast< const C_BasePlayer * >( pEntity )->m_bIsLocalPlayer;
}

int	C_BasePlayer::GetUserID( void ) const
{
	player_info_t pi;

	if ( !engine->GetPlayerInfo( entindex(), &pi ) )
		return -1;

	return pi.userID;
}


// For weapon prediction
void C_BasePlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	// FIXME
}

void C_BasePlayer::UpdateClientData( void )
{
	// Update all the items
	for ( int i = 0; i < WeaponCount(); i++ )
	{
		if ( GetWeapon(i) )  // each item updates it's successors
			GetWeapon(i)->UpdateClientData( this );
	}
}

// Prediction stuff
void C_BasePlayer::PreThink( void )
{
#if !defined( NO_ENTITY_PREDICTION )
	ItemPreFrame();

	UpdateClientData();

	UpdateUnderwaterState();

	// Update the player's fog data if necessary.
	UpdateFogController();

	if (m_lifeState >= LIFE_DYING)
		return;

	//
	// If we're not on the ground, we're falling. Update our falling velocity.
	//
	if ( !( GetFlags() & FL_ONGROUND ) )
	{
		m_Local.m_flFallVelocity = -GetAbsVelocity().z;
	}
#endif

	if ( GetGroundEntity() )
	{
		m_flTimeLastTouchedGround = gpGlobals->curtime;
	}
}

void C_BasePlayer::PostThink( void )
{
#if !defined( NO_ENTITY_PREDICTION )
	MDLCACHE_CRITICAL_SECTION();

	if ( IsAlive())
	{
		UpdateCollisionBounds();

		if ( !CommentaryModeShouldSwallowInput( this ) )
		{
			// do weapon stuff
			ItemPostFrame();
		}

		if ( GetFlags() & FL_ONGROUND )
		{		
			m_Local.m_flFallVelocity = 0;
		}

		// Don't allow bogus sequence on player
		if ( GetSequence() == -1 )
		{
			SetSequence( 0 );
		}

		StudioFrameAdvance();
		PostThinkVPhysics();
	}

	// Even if dead simulate entities
	SimulatePlayerSimulatedEntities();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: send various tool messages - viewoffset, and base class messages (flex and bones)
//-----------------------------------------------------------------------------
void C_BasePlayer::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	VPROF_BUDGET( "C_BasePlayer::GetToolRecordingState", VPROF_BUDGETGROUP_TOOLS );

	BaseClass::GetToolRecordingState( msg );

	msg->SetBool( "baseplayer", true );
	msg->SetBool( "localplayer", IsLocalPlayer( this ) );
	msg->SetString( "playername", GetPlayerName() );

	static CameraRecordingState_t state;
	state.m_flFOV = GetFOV();

	float flZNear = view->GetZNear();
	float flZFar = view->GetZFar();
	CalcView( state.m_vecEyePosition, state.m_vecEyeAngles, flZNear, flZFar, state.m_flFOV );
	state.m_bThirdPerson = !engine->IsPaused() && ::input->CAM_IsThirdPerson();
	state.m_bPlayerEyeIsPortalled = false;

	// this is a straight copy from ClientModeShared::OverrideView,
	// When that method is removed in favor of rolling it into CalcView,
	// then this code can (should!) be removed
	if ( state.m_bThirdPerson )
	{
		Vector cam_ofs;
		::input->CAM_GetCameraOffset( cam_ofs );

		QAngle camAngles;
		camAngles[ PITCH ] = cam_ofs[ PITCH ];
		camAngles[ YAW ] = cam_ofs[ YAW ];
		camAngles[ ROLL ] = 0;

		Vector camForward, camRight, camUp;
		AngleVectors( camAngles, &camForward, &camRight, &camUp );

		VectorMA( state.m_vecEyePosition, -cam_ofs[ ROLL ], camForward, state.m_vecEyePosition );

		// Override angles from third person camera
		VectorCopy( camAngles, state.m_vecEyeAngles );
	}

	msg->SetPtr( "camera", &state );
}


//-----------------------------------------------------------------------------
// Purpose: Simulate the player for this frame
//-----------------------------------------------------------------------------
bool C_BasePlayer::Simulate()
{
	//Frame updates
	if ( C_BasePlayer::IsLocalPlayer( this ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

		//Update the flashlight
		Flashlight();

		// Update the player's fog data if necessary.
		UpdateFogController();
	}
	else
	{
		// update step sounds for all other players
		Vector vel;
		EstimateAbsVelocity( vel );
		UpdateStepSound( GetGroundSurface(), GetAbsOrigin(), vel );
	}

	BaseClass::Simulate();

	// Server says don't interpolate this frame, so set previous info to new info.
	if ( IsEffectActive( EF_NOINTERP ) || Teleported() )
	{
		ResetLatched();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseViewModel
//-----------------------------------------------------------------------------
C_BaseViewModel *C_BasePlayer::GetViewModel( int index /*= 0*/ ) const
{
	Assert( index >= 0 && index < MAX_VIEWMODELS );

	C_BaseViewModel *vm = m_hViewModel[ index ];
	
	if ( GetObserverMode() == OBS_MODE_IN_EYE )
	{
		C_BasePlayer *target =  ToBasePlayer( GetObserverTarget() );

		// get the targets viewmodel unless the target is an observer itself
		if ( target && target != this && !target->IsObserver() )
		{
			vm = target->GetViewModel( index );
		}
	}

	return vm;
}

C_BaseCombatWeapon	*C_BasePlayer::GetActiveWeapon( void ) const
{
	const C_BasePlayer *fromPlayer = this;

	// if localplayer is in InEye spectator mode, return weapon on chased player.
	if ( ( C_BasePlayer::IsLocalPlayer( const_cast< C_BasePlayer * >( fromPlayer) ) ) && ( GetObserverMode() == OBS_MODE_IN_EYE) )
	{
		C_BaseEntity *target =  GetObserverTarget();

		if ( target && target->IsPlayer() )
		{
			fromPlayer = ToBasePlayer( target );
		}
	}

	return fromPlayer->C_BaseCombatCharacter::GetActiveWeapon();
}

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector C_BasePlayer::GetAutoaimVector( float flScale )
{
	// Never autoaim a predicted weapon (for now)
	Vector	forward;
	AngleVectors( GetAbsAngles() + m_Local.m_viewPunchAngle, &forward );
	return	forward;
}

// Stuff for prediction
void C_BasePlayer::SetSuitUpdate(char *name, int fgroup, int iNoRepeat)
{
	// FIXME:  Do something here?
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BasePlayer::ResetAutoaim( void )
{
#if 0
	if (m_vecAutoAim.x != 0 || m_vecAutoAim.y != 0)
	{
		m_vecAutoAim = QAngle( 0, 0, 0 );
		engine->CrosshairAngle( edict(), 0, 0 );
	}
#endif
	m_fOnTarget = false;
}

bool C_BasePlayer::ShouldPredict( void )
{
#if !defined( NO_ENTITY_PREDICTION )
	// Do this before calling into baseclass so prediction data block gets allocated
	if ( IsLocalPlayer( this ) )
	{
		return true;
	}
#endif
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return the player who will predict this entity
//-----------------------------------------------------------------------------
C_BasePlayer *C_BasePlayer::GetPredictionOwner( void )
{
	return this;
}

//-----------------------------------------------------------------------------
// Purpose: Special processing for player simulation
// NOTE: Don't chain to BaseClass!!!!
//-----------------------------------------------------------------------------
void C_BasePlayer::PhysicsSimulate( void )
{
#if !defined( NO_ENTITY_PREDICTION )
	VPROF( "C_BasePlayer::PhysicsSimulate" );
	// If we've got a moveparent, we must simulate that first.
	CBaseEntity *pMoveParent = GetMoveParent();
	if (pMoveParent)
	{
		pMoveParent->PhysicsSimulate();
	}

	// Make sure not to simulate this guy twice per frame
	if (m_nSimulationTick == gpGlobals->tickcount)
		return;

	m_nSimulationTick = gpGlobals->tickcount;

	if ( !IsLocalPlayer( this ) )
		return;

	C_CommandContext *ctx = GetCommandContext();
	Assert( ctx );
	Assert( ctx->needsprocessing );
	if ( !ctx->needsprocessing )
		return;

	ctx->needsprocessing = false;

	m_bTouchedPhysObject = false;

	// Handle FL_FROZEN.
	if(GetFlags() & FL_FROZEN)
	{
		ctx->cmd.forwardmove = 0;
		ctx->cmd.sidemove = 0;
		ctx->cmd.upmove = 0;
		ctx->cmd.buttons = 0;
		ctx->cmd.impulse = 0;
		//VectorCopy ( pl.v_angle, ctx->cmd.viewangles );
	}

	// Run the next command
	MoveHelper()->SetHost( this );
	prediction->RunCommand( 
		this, 
		&ctx->cmd, 
		MoveHelper() );
	
	UpdateVPhysicsPosition( m_vNewVPhysicsPosition, m_vNewVPhysicsVelocity, TICK_INTERVAL );

	MoveHelper()->SetHost( NULL );
#endif
}

void C_BasePlayer::PhysicsTouchTriggers( const Vector *pPrevAbsOrigin )
{
	C_BaseCombatCharacter::PhysicsTouchTriggers( pPrevAbsOrigin );

	if ( this == GetLocalPlayer() )
	{
		extern void TouchTriggerSoundOperator( C_BaseEntity *pEntity );
		TouchTriggerSoundOperator( this );
	}
}

QAngle C_BasePlayer::GetViewPunchAngle()
{
	return m_Local.m_viewPunchAngle.Get();
}

void C_BasePlayer::SetViewPunchAngle( const QAngle &angle )
{
	m_Local.m_viewPunchAngle = angle;
}

QAngle C_BasePlayer::GetAimPunchAngle()
{
	return m_Local.m_aimPunchAngle.Get();
}

void C_BasePlayer::SetAimPunchAngle( const QAngle &angle )
{
	m_Local.m_aimPunchAngle = angle;
}

void C_BasePlayer::SetAimPunchAngleVelocity( const QAngle &angleVelocity )
{
	m_Local.m_aimPunchAngleVel = angleVelocity;
}

QAngle C_BasePlayer::GetFinalAimAngle()
{
	QAngle eyeAngles = EyeAngles();

	if ( PlatformInputDevice::IsInputDeviceAPointer( g_pInputSystem->GetCurrentInputDevice() ) )
	{
		// If we are using a pointing device, our final aim angle is based on where we're pointing and not where we're looking.
		VectorAngles( GetAimDirection(), eyeAngles );
	}

	return eyeAngles + GetAimPunchAngle();
}

float C_BasePlayer::GetWaterJumpTime() const
{
	return m_flWaterJumpTime;
}

void C_BasePlayer::SetWaterJumpTime( float flWaterJumpTime )
{
	m_flWaterJumpTime = flWaterJumpTime;
}

float C_BasePlayer::GetSwimSoundTime() const
{
	return m_flSwimSoundTime;
}

void C_BasePlayer::SetSwimSoundTime( float flSwimSoundTime )
{
	m_flSwimSoundTime = flSwimSoundTime;
}


//-----------------------------------------------------------------------------
// Purpose: Return true if this object can be +used by the player
//-----------------------------------------------------------------------------
bool C_BasePlayer::IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps )
{
	return false;
}

C_BaseEntity* C_BasePlayer::GetUseEntity( void ) const 
{ 
	return m_hUseEntity;
}

C_BaseEntity* C_BasePlayer::GetPotentialUseEntity( void ) const 
{ 
	return GetUseEntity();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_BasePlayer::GetFOV( void ) const
{
	if ( GetObserverMode() == OBS_MODE_IN_EYE )
	{
		C_BasePlayer *pTargetPlayer = ToBasePlayer( GetObserverTarget() );

		// get fov from observer target. Not if target is observer itself
		if ( pTargetPlayer && !pTargetPlayer->IsObserver() )
		{
			return pTargetPlayer->GetFOV();
		}
	}

	// Allow our vehicle to override our FOV if it's currently at the default FOV.
	float flDefaultFOV;
	IClientVehicle *pVehicle = const_cast< C_BasePlayer * >(this)->GetVehicle();
	if ( pVehicle )
	{
		if ( IsX360() == false )
			const_cast< C_BasePlayer * >(this)->CacheVehicleView();

		flDefaultFOV = ( m_flVehicleViewFOV == 0 ) ? GetDefaultFOV() : m_flVehicleViewFOV;
	}
	else
	{
		flDefaultFOV = GetDefaultFOV();
	}
	
	float fFOV = ( m_iFOV == 0 ) ? flDefaultFOV : m_iFOV;

	// Don't do lerping during prediction. It's only necessary when actually rendering,
	// and it'll cause problems due to prediction timing messiness.
	if ( !prediction->InPrediction() )
	{
		// See if we need to lerp the values for local player
		if ( IsLocalPlayer( this ) && ( fFOV != m_iFOVStart ) && (m_Local.m_flFOVRate > 0.0f ) )
		{
			float deltaTime = (float)( gpGlobals->curtime - m_flFOVTime ) / m_Local.m_flFOVRate;

#if !defined( NO_ENTITY_PREDICTION )
			if ( GetPredictable() )
			{
				// m_flFOVTime was set to a predicted time in the future, because the FOV change was predicted.
				deltaTime = (float)( GetFinalPredictedTime() - m_flFOVTime );
				deltaTime += ( gpGlobals->interpolation_amount * TICK_INTERVAL );
				deltaTime /= m_Local.m_flFOVRate;
			}
#endif

			if ( deltaTime >= 1.0f )
			{
				//If we're past the zoom time, just take the new value and stop lerping
				const_cast<C_BasePlayer *>(this)->m_iFOVStart = fFOV;
			}
			else
			{
				fFOV = SimpleSplineRemapValClamped( deltaTime, 0.0f, 1.0f, (float) m_iFOVStart, fFOV );
			}
		}
	}

	return fFOV;
}

void C_BasePlayer::RecvProxy_LocalVelocityX( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BasePlayer *pPlayer = (C_BasePlayer *) pStruct;

	Assert( pPlayer );

	float flNewVel_x = pData->m_Value.m_Float;

	Vector vecVelocity = pPlayer->GetLocalVelocity();

	if( vecVelocity.x != flNewVel_x )	// Should this use an epsilon check?
	{
		vecVelocity.x = flNewVel_x;
		pPlayer->SetLocalVelocity( vecVelocity );
	}
}

void C_BasePlayer::RecvProxy_LocalVelocityY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BasePlayer *pPlayer = (C_BasePlayer *) pStruct;

	Assert( pPlayer );

	float flNewVel_y = pData->m_Value.m_Float;

	Vector vecVelocity = pPlayer->GetLocalVelocity();

	if( vecVelocity.y != flNewVel_y )
	{
		vecVelocity.y = flNewVel_y;
		pPlayer->SetLocalVelocity( vecVelocity );
	}
}

void C_BasePlayer::RecvProxy_LocalVelocityZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BasePlayer *pPlayer = (C_BasePlayer *) pStruct;
	
	Assert( pPlayer );

	float flNewVel_z = pData->m_Value.m_Float;

	Vector vecVelocity = pPlayer->GetLocalVelocity();

	if( vecVelocity.z != flNewVel_z )
	{
		vecVelocity.z = flNewVel_z;
		pPlayer->SetLocalVelocity( vecVelocity );
	}
}

void C_BasePlayer::RecvProxy_ObserverMode( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	C_BasePlayer *pPlayer = (C_BasePlayer *) pStruct;
	Assert( pPlayer );
	
	if ( pPlayer )
		pPlayer->OnObserverModeChange( false );
}

void C_BasePlayer::OnObserverModeChange( bool bIsObserverTarget )
{
	C_BasePlayer *pPlayer = this;

	if ( C_BasePlayer::IsLocalPlayer( pPlayer ) || bIsObserverTarget )
	{
		pPlayer->UpdateVisibility();
		UpdateViewmodelVisibility( pPlayer );
	}

	if ( bIsObserverTarget )
		return;

	// [msmith] When the observer mode changes, we also need to update the visibility of the view models for the
	// target we are observing.  This is important when changing between first and third person when in split screen.
	C_BasePlayer* observerTarget = ToBasePlayer( pPlayer->GetObserverTarget() );
	if ( NULL != observerTarget )
	{
		observerTarget->UpdateVisibility();
		UpdateViewmodelVisibility( observerTarget );
	}
}

void C_BasePlayer::RecvProxy_ObserverTarget( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BasePlayer *pPlayer = (C_BasePlayer *) pStruct;

	Assert( pPlayer );

	EHANDLE hTarget;

	RecvProxy_IntToEHandle( pData, pStruct, &hTarget );

	pPlayer->SetObserverTarget( hTarget );
}

void C_BasePlayer::RecvProxy_LocalOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BasePlayer *player = (C_BasePlayer *) pStruct;
	player->m_vecHack_RecvProxy_LocalPlayerOrigin.x = pData->m_Value.m_Vector[0];
	player->m_vecHack_RecvProxy_LocalPlayerOrigin.y = pData->m_Value.m_Vector[1];

	((float*)pOut)[0] = pData->m_Value.m_Vector[0];
	((float*)pOut)[1] = pData->m_Value.m_Vector[1];
}

void C_BasePlayer::RecvProxy_LocalOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BasePlayer *player = (C_BasePlayer *) pStruct;
	player->m_vecHack_RecvProxy_LocalPlayerOrigin.z = pData->m_Value.m_Float;

	*((float*)pOut) = pData->m_Value.m_Float;
}

void C_BasePlayer::RecvProxy_NonLocalOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	((float*)pOut)[0] = pData->m_Value.m_Vector[0];
	((float*)pOut)[1] = pData->m_Value.m_Vector[1];
}

void C_BasePlayer::RecvProxy_NonLocalOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	*((float*)pOut) = pData->m_Value.m_Float;
}

void C_BasePlayer::RecvProxy_NonLocalCellOriginXY( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BasePlayer *player = (C_BasePlayer *) pStruct;

	player->m_vecCellOrigin.x = pData->m_Value.m_Vector[0];
	player->m_vecCellOrigin.y = pData->m_Value.m_Vector[1];

	register int const cellwidth = player->m_cellwidth; // Load it into a register
	((float*)pOut)[0] = CoordFromCell( cellwidth, player->m_cellX, pData->m_Value.m_Vector[0] );
	((float*)pOut)[1] = CoordFromCell( cellwidth, player->m_cellY, pData->m_Value.m_Vector[1] );
}

void C_BasePlayer::RecvProxy_NonLocalCellOriginZ( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_BasePlayer *player = (C_BasePlayer *) pStruct;

	player->m_vecCellOrigin.z = pData->m_Value.m_Float;

	register int const cellwidth = player->m_cellwidth; // Load it into a register
	*((float*)pOut) = CoordFromCell( cellwidth, player->m_cellZ, pData->m_Value.m_Float );
}

//-----------------------------------------------------------------------------
// Purpose: Remove this player from a vehicle
//-----------------------------------------------------------------------------
void C_BasePlayer::LeaveVehicle( void )
{
	if ( NULL == m_hVehicle.Get() )
		return;

// Let server do this for now
#if 0
	IClientVehicle *pVehicle = GetVehicle();
	Assert( pVehicle );

	int nRole = pVehicle->GetPassengerRole( this );
	Assert( nRole != VEHICLE_ROLE_NONE );

	SetParent( NULL );

	// Find the first non-blocked exit point:
	Vector vNewPos = GetAbsOrigin();
	QAngle qAngles = GetAbsAngles();
	pVehicle->GetPassengerExitPoint( nRole, &vNewPos, &qAngles );
	OnVehicleEnd( vNewPos );
	SetAbsOrigin( vNewPos );
	SetAbsAngles( qAngles );

	m_Local.m_iHideHUD &= ~HIDEHUD_WEAPONSELECTION;
	RemoveEffects( EF_NODRAW );

	SetMoveType( MOVETYPE_WALK );
	SetCollisionGroup( COLLISION_GROUP_PLAYER );

	qAngles[ROLL] = 0;
	SnapEyeAngles( qAngles );

	m_hVehicle = NULL;
	pVehicle->SetPassenger(nRole, NULL);

	Weapon_Switch( m_hLastWeapon );
#endif
}


float C_BasePlayer::GetMinFOV()	const
{
	if ( gpGlobals->maxClients == 1 )
	{
		// Let them do whatever they want, more or less, in single player
		return 5;
	}
	else
	{
		return 75;
	}
}

float C_BasePlayer::GetFinalPredictedTime() const
{
	return ( m_nFinalPredictedTick * TICK_INTERVAL );
}

float C_BasePlayer::PredictedServerTime() const
{
	return m_fLastUpdateServerTime + ((m_nTickBase - m_nLastUpdateTickBase) * TICK_INTERVAL);
}

void C_BasePlayer::NotePredictionError( const Vector &vDelta )
{
	// don't worry about prediction errors when dead
	if ( !IsAlive() )
		return;

#if !defined( NO_ENTITY_PREDICTION )
	Vector vOldDelta;

	GetPredictionErrorSmoothingVector( vOldDelta );

	// sum all errors within smoothing time
	m_vecPredictionError = vDelta + vOldDelta;

	// remember when last error happened
	m_flPredictionErrorTime = gpGlobals->curtime;
 
	ResetLatched(); 
#endif
}


// offset curtime and setup bones at that time using fake interpolation
// fake interpolation means we don't have reliable interpolation history (the local player doesn't animate locally)
// so we just modify cycle and origin directly and use that as a fake guess
void C_BasePlayer::ForceSetupBonesAtTimeFakeInterpolation( matrix3x4a_t *pBonesOut, float curtimeOffset )
{
	// we don't have any interpolation data, so fake it
	float cycle = m_flCycle;
	Vector origin = GetLocalOrigin();

	// blow the cached prev bones
	InvalidateBoneCache();
	// reset root position to flTime
	Interpolate( gpGlobals->curtime + curtimeOffset );

	// force cycle back by boneDt
	m_flCycle = fmod( 10 + cycle + GetPlaybackRate() * curtimeOffset, 1.0f );
	SetLocalOrigin( origin + curtimeOffset * GetLocalVelocity() );
	// Setup bone state to extrapolate physics velocity
	SetupBones( pBonesOut, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime + curtimeOffset );

	m_flCycle = cycle;
	SetLocalOrigin( origin );
}

void C_BasePlayer::GetRagdollInitBoneArrays( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt )
{
	if ( !C_BasePlayer::IsLocalPlayer( this ) )
	{
		BaseClass::GetRagdollInitBoneArrays(pDeltaBones0, pDeltaBones1, pCurrentBones, boneDt);
		return;
	}
	ForceSetupBonesAtTimeFakeInterpolation( pDeltaBones0, -boneDt );
	ForceSetupBonesAtTimeFakeInterpolation( pDeltaBones1, 0 );
	float ragdollCreateTime = PhysGetSyncCreateTime();
	if ( ragdollCreateTime != gpGlobals->curtime )
	{
		ForceSetupBonesAtTimeFakeInterpolation( pCurrentBones, ragdollCreateTime - gpGlobals->curtime );
	}
	else
	{
		SetupBones( pCurrentBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
	}
}


void C_BasePlayer::GetPredictionErrorSmoothingVector( Vector &vOffset )
{
#if !defined( NO_ENTITY_PREDICTION )
	if ( engine->IsPlayingDemo() || !cl_smooth.GetInt() || !cl_predict->GetInt() || engine->IsPaused() )
	{
		vOffset.Init();
		return;
	}

	float errorAmount = ( gpGlobals->curtime - m_flPredictionErrorTime ) / cl_smoothtime.GetFloat();

	if ( errorAmount >= 1.0f )
	{
		vOffset.Init();
		return;
	}
	
	errorAmount = 1.0f - errorAmount;

	vOffset = m_vecPredictionError * errorAmount;
#else
	vOffset.Init();
#endif
}


IRagdoll* C_BasePlayer::GetRepresentativeRagdoll() const
{
	return m_pRagdoll;
}

IMaterial *C_BasePlayer::GetHeadLabelMaterial( void )
{
	if ( GetClientVoiceMgr() == NULL )
		return NULL;

	return GetClientVoiceMgr()->GetHeadLabelMaterial();
}

void C_BasePlayer::UpdateSpeechVOIP( bool bVoice )
{
	m_bPlayerIsTalkingOverVOIP = ( bVoice && ShouldShowVOIPIcon() );

	if ( voice_icons_method.GetInt() == 2 )
		return;

	if ( m_bPlayerIsTalkingOverVOIP )
	{
		if ( !m_speechVOIPParticleEffect.IsValid() )
		{
			MDLCACHE_CRITICAL_SECTION();
			m_speechVOIPParticleEffect = GetVOIPParticleEffect();
		}
	}
	else
	{
		if ( m_speechVOIPParticleEffect.IsValid() )
		{
			ParticleProp()->StopEmissionAndDestroyImmediately( m_speechVOIPParticleEffect );
			m_speechVOIPParticleEffect = NULL;
		}
	}
}

bool C_BasePlayer::ShouldShowVOIPIcon() const
{
	return GameRules() && GameRules()->IsMultiplayer() && ( !IsLocalPlayer( this ) || voice_all_icons.GetBool() );
}

CNewParticleEffect *C_BasePlayer::GetVOIPParticleEffect( void )
{
	return ParticleProp()->Create( GetVOIPParticleEffectName(), PATTACH_ABSORIGIN_FOLLOW, -1, ( EyePosition() - GetAbsOrigin() ) + Vector( 0.0f, 0.0f, GetClientVoiceMgr()->GetHeadLabelOffset() ) );
}

bool IsInFreezeCam( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer && pPlayer->GetObserverMode() == OBS_MODE_FREEZECAM )
		return true;

	return false;
}

void C_BasePlayer::SetLastKillerDamageAndFreezeframe( int nLastKillerDamageTaken, int nLastKillerHitsTaken, int nLastKillerDamageGiven, int nLastKillerHitsGiven )
{
	m_nLastKillerDamageTaken = nLastKillerDamageTaken;
	m_nLastKillerHitsTaken = nLastKillerHitsTaken;
	m_nLastKillerDamageGiven = nLastKillerDamageGiven;
	m_nLastKillerHitsGiven = nLastKillerHitsGiven;

	m_bCanShowFreezeFrameNow = true;
	//m_bCheckHltvReplayAutoStart = spec_replay_autostart.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: Set the fog controller data per player.
// Input  : &inputdata -
//-----------------------------------------------------------------------------
void C_BasePlayer::FogControllerChanged( bool bSnap )
{
	if ( m_PlayerFog.m_hCtrl )
	{
		fogparams_t	*pFogParams = &(m_PlayerFog.m_hCtrl->m_fog);

		/*
		Msg("FOG %s (%d) Updating Fog Target: (%d,%d,%d) %.0f,%.0f -> (%d,%d,%d) %.0f,%.0f (%.2f seconds)\n", 
			 GetPlayerName(), entindex(),
					m_CurrentFog.colorPrimary.GetR(), m_CurrentFog.colorPrimary.GetB(), m_CurrentFog.colorPrimary.GetG(), 
					m_CurrentFog.start.Get(), m_CurrentFog.end.Get(), 
					pFogParams->colorPrimary.GetR(), pFogParams->colorPrimary.GetB(), pFogParams->colorPrimary.GetG(), 
					pFogParams->start.Get(), pFogParams->end.Get(), pFogParams->duration.Get() );*/
		

		// Setup the fog color transition.
		m_PlayerFog.m_OldColor = m_CurrentFog.colorPrimary;
		m_PlayerFog.m_flOldStart = m_CurrentFog.start;
		m_PlayerFog.m_flOldEnd = m_CurrentFog.end;
		m_PlayerFog.m_flOldMaxDensity = m_CurrentFog.maxdensity;
		m_PlayerFog.m_flOldHDRColorScale = m_CurrentFog.HDRColorScale;
		m_PlayerFog.m_flOldFarZ = m_CurrentFog.farz;

		m_PlayerFog.m_NewColor = pFogParams->colorPrimary;
		m_PlayerFog.m_flNewStart = pFogParams->start;
		m_PlayerFog.m_flNewEnd = pFogParams->end;
		m_PlayerFog.m_flNewMaxDensity = pFogParams->maxdensity;
		m_PlayerFog.m_flNewHDRColorScale = pFogParams->HDRColorScale;
		m_PlayerFog.m_flNewFarZ = pFogParams->farz;

		m_PlayerFog.m_flTransitionTime = bSnap ? -1 : gpGlobals->curtime;

		m_PlayerFog.m_flZoomFogScale = pFogParams->ZoomFogScale;

		m_CurrentFog = *pFogParams;

		// Update the fog player's local fog data with the fog controller's data if need be.
		UpdateFogController();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check to see that the controllers data is up to date.
//-----------------------------------------------------------------------------
void C_BasePlayer::UpdateFogController( void )
{
	if ( m_PlayerFog.m_hCtrl )
	{
		// Don't bother copying while we're transitioning, since it'll be stomped in UpdateFogBlend();
		if ( m_PlayerFog.m_flTransitionTime == -1 && (m_hOldFogController == m_PlayerFog.m_hCtrl) )
		{
			fogparams_t	*pFogParams = &(m_PlayerFog.m_hCtrl->m_fog);
			if ( m_CurrentFog != *pFogParams )
			{
				/*
					Msg("FOG %s (%d) FORCING UPDATE: (%d,%d,%d) %.0f,%.0f -> (%d,%d,%d) %.0f,%.0f (%.2f seconds)\n", 
							GetPlayerName(), entindex(),
										m_CurrentFog.colorPrimary.GetR(), m_CurrentFog.colorPrimary.GetB(), m_CurrentFog.colorPrimary.GetG(), 
										m_CurrentFog.start.Get(), m_CurrentFog.end.Get(), 
										pFogParams->colorPrimary.GetR(), pFogParams->colorPrimary.GetB(), pFogParams->colorPrimary.GetG(), 
										pFogParams->start.Get(), pFogParams->end.Get(), pFogParams->duration.Get() );*/
					

				m_CurrentFog = *pFogParams;
			}
		}
	}
	else
	{
		if ( m_CurrentFog.farz != -1 || m_CurrentFog.enable != false )
		{
			// No fog controller in this level. Use default fog parameters.
			m_CurrentFog.farz = -1;
			m_CurrentFog.enable = false;
		}
	}

	// Update the fog blending state - of necessary.
	UpdateFogBlend();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C_BasePlayer::UpdateFogBlend( void )
{
	float flNewStart = m_PlayerFog.m_flNewStart;
	float flNewEnd = m_PlayerFog.m_flNewEnd;

	// Transition.
	if ( m_PlayerFog.m_flTransitionTime != -1 )
	{
		float flTimeDelta = gpGlobals->curtime - m_PlayerFog.m_flTransitionTime;
		if ( flTimeDelta < m_CurrentFog.duration )
		{
			float flScale = flTimeDelta / m_CurrentFog.duration;
			m_CurrentFog.colorPrimary.SetR( ( m_PlayerFog.m_NewColor.r * flScale ) + ( m_PlayerFog.m_OldColor.r * ( 1.0f - flScale ) ) );
			m_CurrentFog.colorPrimary.SetG( ( m_PlayerFog.m_NewColor.g * flScale ) + ( m_PlayerFog.m_OldColor.g * ( 1.0f - flScale ) ) );
			m_CurrentFog.colorPrimary.SetB( ( m_PlayerFog.m_NewColor.b * flScale ) + ( m_PlayerFog.m_OldColor.b * ( 1.0f - flScale ) ) );
			m_CurrentFog.start.Set( ( flNewStart * flScale ) + ( ( m_PlayerFog.m_flOldStart * ( 1.0f - flScale ) ) ) );
			m_CurrentFog.end.Set( ( flNewEnd * flScale ) + ( ( m_PlayerFog.m_flOldEnd * ( 1.0f - flScale ) ) ) );
			m_CurrentFog.maxdensity.Set( ( m_PlayerFog.m_flNewMaxDensity * flScale ) + ( ( m_PlayerFog.m_flOldMaxDensity * ( 1.0f - flScale ) ) ) );
			m_CurrentFog.HDRColorScale.Set( ( m_PlayerFog.m_flNewHDRColorScale * flScale ) + ( ( m_PlayerFog.m_flOldHDRColorScale * ( 1.0f - flScale ) ) ) );

			// Lerp to a sane FarZ (default value comes from CViewRender::GetZFar())
			float newFarZ = m_PlayerFog.m_flNewFarZ;
			if ( newFarZ <= 0 )
				newFarZ = r_mapextents.GetFloat() * 1.73205080757f;

			float oldFarZ = m_PlayerFog.m_flOldFarZ;
			if ( oldFarZ <= 0 )
				oldFarZ = r_mapextents.GetFloat() * 1.73205080757f;

			m_CurrentFog.farz.Set( ( newFarZ * flScale ) + ( ( oldFarZ * ( 1.0f - flScale ) ) ) );
		}
		else
		{
			// Slam the final fog values.
			m_CurrentFog.colorPrimary.SetR( m_PlayerFog.m_NewColor.r );
			m_CurrentFog.colorPrimary.SetG( m_PlayerFog.m_NewColor.g );
			m_CurrentFog.colorPrimary.SetB( m_PlayerFog.m_NewColor.b );
			m_CurrentFog.start.Set( flNewStart );
			m_CurrentFog.end.Set( flNewEnd );
			m_CurrentFog.maxdensity.Set( m_PlayerFog.m_flNewMaxDensity );
			m_CurrentFog.HDRColorScale.Set( m_PlayerFog.m_flNewHDRColorScale );
			m_CurrentFog.farz.Set( m_PlayerFog.m_flNewFarZ );
			m_PlayerFog.m_flTransitionTime = -1;

			/*
				Msg("FOG %s (%d) Finished transition to (%d,%d,%d) %.0f,%.0f\n", 
					 GetPlayerName(), entindex(),
								m_CurrentFog.colorPrimary.GetR(), m_CurrentFog.colorPrimary.GetB(), m_CurrentFog.colorPrimary.GetG(), 
								m_CurrentFog.start.Get(), m_CurrentFog.end.Get() );*/
				
		}
	}

#if defined( CSTRIKE_DLL )
	float flFov = GetFOV();
	float flDefaultFov = GetDefaultFOV( );
	if ( flFov < flDefaultFov )
	{
		float frac = (1.0 - MAX( 0.1, flFov / flDefaultFov )) * m_PlayerFog.m_flZoomFogScale;
		flNewEnd = flNewEnd + (800 * frac);
		flNewStart = flNewStart + (200 * frac);

		m_CurrentFog.start.Set( flNewStart );
		m_CurrentFog.end.Set( flNewEnd );
	}
#endif
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
C_PostProcessController* C_BasePlayer::GetActivePostProcessController() const
{
	return m_hPostProcessCtrl.Get();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
C_ColorCorrection* C_BasePlayer::GetActiveColorCorrection() const
{
	return m_hColorCorrectionCtrl.Get();
}

bool C_BasePlayer::PreRender( int nSplitScreenPlayerSlot )
{
	if ( !IsVisible() || 
		!GetClientMode()->ShouldDrawLocalPlayer( this ) )
	{
		return true;
	}

	// Add in lighting effects
	return CreateLightEffects();
}

bool C_BasePlayer::IsSplitScreenPartner( C_BasePlayer *pPlayer )
{
	if ( !pPlayer || pPlayer == this )
		return false;

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		if ( s_pLocalPlayer[i] == pPlayer )
			return true;
	}	

	return false;
}

int C_BasePlayer::GetSplitScreenPlayerSlot()
{
	Assert( ( m_nSplitScreenSlot >= 0 ) && ( m_nSplitScreenSlot < MAX_SPLITSCREEN_PLAYERS ) );
	return m_nSplitScreenSlot;
}

bool C_BasePlayer::IsSplitScreenPlayer() const
{
	return m_nSplitScreenSlot >= 1;
}

CrossPlayPlatform_t C_BasePlayer::GetCrossPlayPlatform( void ) const
{
	return CROSSPLAYPLATFORM_THISPLATFORM;
}

bool C_BasePlayer::ShouldRegenerateOriginFromCellBits() const
{
	// Don't use cell bits for local players
	if ( 
#ifdef PORTAL2
		// HACK: In Portal 2, when we start recording a demo, the player is removed and recreated.
		//		 There's a brief window where there is no local player and the non-local data table
		//		 is sent across for the newly created player, containing the cell origin. This is
		//		 incorrectly interpreted and copied to the network origin. The new network origin
		//		 is copied to the local origin, which, among other things, screws up the player's eye
		//		 position until she moves enough for a network update to fix her position. During this
		//		 brief time, if we correctly regenerate the origin from the cell bits we received, it
		//		 prevents this problem. At this point, changing the portal player's network tables
		//		 could have a significant impact on perf and require a PS3 fix to maintain crossplay
		//		 compatibility, so this is less risky.
		//		 - Ted Rivera (2/25/2011)
		( C_BasePlayer::HasAnyLocalPlayer() ||
			( !engine->IsPlayingDemo() &&		
			  !engine->IsRecordingDemo() &&		
			  !engine->IsPlayingTimeDemo() ) ) &&	
#endif
		 (IsLocalPlayer( this ) ||
		  (!g_pGameRules->IsMultiplayer()) ) ) //SP load fails the IsLocalPlayer() test while creating the player. Resulting in a bad origin until you move
	{
		return false;
	}

	/*if ( g_pGameRules->IsMultiplayer() &&
		(GetCreationTick() == gpGlobals->tickcount) &&
		!C_BasePlayer::HasAnyLocalPlayer() &&
		(engine->GetLocalPlayer() == index) )
	{
		return false;
	}*/

	return BaseClass::ShouldRegenerateOriginFromCellBits();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BasePlayer::GetSteamID( CSteamID *pID )
{
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	// try to make this a little more efficient

	player_info_t pi;
	if ( engine->GetPlayerInfo( entindex(), &pi ) )
	{
		* pID = CSteamID( pi.xuid );
		if ( pID->GetEAccountType() == k_EAccountTypeIndividual )
		{
			pID->SetAccountInstance( 1 );
			return true;
		}

		if ( pi.friendsID && steamapicontext && steamapicontext->SteamUtils() )
		{
#if 1	// new
			static EUniverse universe = k_EUniverseInvalid;

			if ( universe == k_EUniverseInvalid )
				universe = steamapicontext->SteamUtils()->GetConnectedUniverse();

			pID->InstancedSet( pi.friendsID, 1, universe, k_EAccountTypeIndividual );
#else	// old
			pID->InstancedSet( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
#endif

			return true;
		}
	}
#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

	return false;
}

void C_BasePlayer::OnTimeJumpAllPlayers()
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			pPlayer->OnTimeJump();
		}
	}
}


void C_BasePlayer::OnTimeJump()
{
}

void CC_DumpClientSoundscapeData( const CCommand& args )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	Msg("Client Soundscape data dump:\n");
	Msg("   Position: %.2f %.2f %.2f\n", pPlayer->GetAbsOrigin().x, pPlayer->GetAbsOrigin().y, pPlayer->GetAbsOrigin().z );
	Msg("   soundscape index: %d\n", pPlayer->m_Local.m_audio.soundscapeIndex.Get() );
	Msg("   entity index: %d\n", pPlayer->m_Local.m_audio.entIndex.Get() );
	bool bFoundOne = false;
	for ( int i = 0; i < NUM_AUDIO_LOCAL_SOUNDS; i++ )
	{
		if ( pPlayer->m_Local.m_audio.localBits & (1<<i) )
		{
			if ( !bFoundOne )
			{
				Msg("   Sound Positions:\n");
				bFoundOne = true;
			}

			Vector vecPos = pPlayer->m_Local.m_audio.localSound[i];
			Msg("   %d: %.2f %.2f %.2f\n", i, vecPos.x,vecPos.y, vecPos.z );
		}
	}

	Msg("End dump.\n");
}
static ConCommand soundscape_dumpclient("soundscape_dumpclient", CC_DumpClientSoundscapeData, "Dumps the client's soundscape data.\n", FCVAR_CHEAT);

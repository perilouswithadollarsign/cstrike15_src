//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:		Player for .
//
//===========================================================================//

#include "cbase.h"
#include "vcollide_parse.h"
#include "c_portal_player.h"
#include "view.h"
#include "c_basetempentity.h"
#include "takedamageinfo.h"
#include "in_buttons.h"
#include "iviewrender_beams.h"
#include "r_efx.h"
#include "dlight.h"
#include "portalrender.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "tier1/keyvalues.h"
#include "ScreenSpaceEffects.h"
#include "portal_shareddefs.h"
#include "ivieweffects.h"		// for screenshake
#include "portal_base2d_shared.h"
#include "input.h"
#include "prediction.h"
#include "choreoevent.h"
#include "model_types.h"
#include "materialsystem/imaterialvar.h"
#include "portal_mp_gamerules.h"
#include "collisionutils.h"
#include "engine/ivdebugoverlay.h"
#include "c_physicsprop.h"
#include "portal2/portal_grabcontroller_shared.h"
#include "igameevents.h"
#include "inetchannelinfo.h"


#include "igamemovement.h"
#include "c_weapon_paintgun.h"
#include "c_weapon_portalgun.h"

#include "cam_thirdperson.h"

#include "vgui_int.h"
#include "dt_utlvector_recv.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Don't alias here
#if defined( CPortal_Player )
#undef CPortal_Player
#endif

ConVar cl_portal_camera_orientation_max_speed("cl_portal_camera_orientation_max_speed", "375.0f" );
ConVar cl_portal_camera_orientation_rate("cl_portal_camera_orientation_rate", "480.0f" );
ConVar cl_portal_camera_orientation_rate_base("cl_portal_camera_orientation_rate_base", "45.0f" );
ConVar cl_portal_camera_orientation_acceleration_rate("cl_portal_camera_orientation_acceleration_rate", "1000.0f" );
ConVar cl_skip_player_render_in_main_view("cl_skip_player_render_in_main_view", "1", FCVAR_ARCHIVE );
ConVar cl_auto_taunt_pip( "cl_auto_taunt_pip", "1", FCVAR_ARCHIVE );

ConVar sv_zoom_stop_movement_threashold("sv_zoom_stop_movement_threashold", "4.0", FCVAR_REPLICATED, "Move command amount before breaking player out of toggle zoom." );
ConVar sv_zoom_stop_time_threashold("sv_zoom_stop_time_threashold", "5.0", FCVAR_REPLICATED, "Time amount before breaking player out of toggle zoom." );
ConVar cl_taunt_finish_rotate_cam("cl_taunt_finish_rotate_cam", "1", FCVAR_CHEAT);
ConVar cl_taunt_finish_speed("cl_taunt_finish_speed", "0.8f", FCVAR_CHEAT);

ConVar mp_taunt_position_blend_rate( "mp_taunt_position_blend_rate", "4.0", FCVAR_CHEAT );
ConVar mp_bot_fling_trail( "mp_bot_fling_trail", "0", FCVAR_CHEAT, "When bots reach a certain velocity in the air, they will show a trail behind them (0 = off, 1 = on, 2 = fun)" );
ConVar mp_bot_fling_trail_kill_scaler( "mp_bot_fling_trail_kill_scaler", "0.01", FCVAR_CHEAT, "The scaler that determines how close to a portal a player has to be (when flinging towards it) before the trail turns off" );

ConVar player_held_object_keep_out_of_camera("player_held_object_keep_out_of_camera", "1", FCVAR_CHEAT );

ConVar mp_auto_taunt( "mp_auto_taunt", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar mp_auto_accept_team_taunt( "mp_auto_accept_team_taunt", "1", FCVAR_ARCHIVE );

ConVar cl_fov( "cl_fov", "90", FCVAR_NONE, "Client-side fov control that is global for all splitscreen players on this machine.  This gets overriden via splitscreen_config.txt for splitscreen." );

extern ConVar sv_debug_player_use;
extern ConVar mp_should_gib_bots;
extern ConVar player_held_object_use_view_model;
extern ConVar sv_use_trace_duration;
extern void RecieveEntityPortalledMessage( CHandle<C_BaseEntity> hEntity, CHandle<C_Portal_Base2D> hPortal, float fTime, bool bForcedDuck );
extern ConVar player_held_object_collide_with_player;
extern ConVar ss_force_primary_fullscreen;
extern ConVar sv_player_funnel_into_portals;
extern ConVar sv_player_funnel_gimme_dot;

//#define ENABLE_PORTAL_EYE_INTERPOLATION_CODE

extern ConVar sv_gravity;


// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //
class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	bool ShouldDoAnimationEvent( C_Portal_Player *pPlayer, PlayerAnimEvent_t event )
	{
		if ( !pPlayer->GetPredictable() )
		{
			return true;
		}

		if ( IsCustomPlayerAnimEvent( event ) )
		{
			return false;
		}

		if ( event == PLAYERANIMEVENT_JUMP || event == PLAYERANIMEVENT_ATTACK_PRIMARY || event == PLAYERANIMEVENT_ATTACK_SECONDARY )
		{
			return false;
		}

		return true;
	}

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		// Create the effect.
		C_Portal_Player *pPlayer = dynamic_cast< C_Portal_Player* >( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			PlayerAnimEvent_t event = (PlayerAnimEvent_t)m_iEvent.Get();
			if ( ShouldDoAnimationEvent( pPlayer, event ) )
			{
				pPlayer->DoAnimationEvent( event, m_nData );
			}
		}	
	}

public:
	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );

BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
RecvPropEHandle( RECVINFO( m_hPlayer ) ),
RecvPropInt( RECVINFO( m_iEvent ) ),
RecvPropInt( RECVINFO( m_nData ) )
END_RECV_TABLE()


//=================================================================================
//
// Ragdoll Entity
//
class C_PortalRagdoll : public C_BaseFlex
{
public:

	DECLARE_CLASS( C_PortalRagdoll, C_BaseFlex );
	DECLARE_CLIENTCLASS();

	C_PortalRagdoll();
	~C_PortalRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	int GetPlayerEntIndex() const;
	IRagdoll* GetIRagdoll() const;

	virtual void SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );

private:

	C_PortalRagdoll( const C_PortalRagdoll & ) {}

	void Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity );
	void CreatePortalRagdoll();

private:

	EHANDLE	m_hPlayer;
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );

};

IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_PortalRagdoll, DT_PortalRagdoll, CPortalRagdoll )
RecvPropVector( RECVINFO(m_vecRagdollOrigin) ),
RecvPropEHandle( RECVINFO( m_hPlayer ) ),
RecvPropInt( RECVINFO( m_nModelIndex ) ),
RecvPropInt( RECVINFO(m_nForceBone) ),
RecvPropVector( RECVINFO(m_vecForce) ),
RecvPropVector( RECVINFO( m_vecRagdollVelocity ) ),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_PortalRagdoll::C_PortalRagdoll()
{
	m_hPlayer = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
C_PortalRagdoll::~C_PortalRagdoll()
{
	( this );
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSourceEntity - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
{
	if ( !pSourceEntity )
		return;

	VarMapping_t *pSrc = pSourceEntity->GetVarMapping();
	VarMapping_t *pDest = GetVarMapping();

	// Find all the VarMapEntry_t's that represent the same variable.
	for ( int i = 0; i < pDest->m_Entries.Count(); i++ )
	{
		VarMapEntry_t *pDestEntry = &pDest->m_Entries[i];
		for ( int j=0; j < pSrc->m_Entries.Count(); j++ )
		{
			VarMapEntry_t *pSrcEntry = &pSrc->m_Entries[j];
			if ( !Q_strcmp( pSrcEntry->watcher->GetDebugName(), pDestEntry->watcher->GetDebugName() ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Setup vertex weights for drawing
//-----------------------------------------------------------------------------
void C_PortalRagdoll::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	// While we're dying, we want to mimic the facial animation of the player. Once they're dead, we just stay as we are.
	if ( (m_hPlayer && m_hPlayer->IsAlive()) || !m_hPlayer )
	{
		BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
	else if ( m_hPlayer )
	{
		m_hPlayer->SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::CreatePortalRagdoll()
{
	// First, initialize all our data. If we have the player's entity on our client,
	// then we can make ourselves start out exactly where the player is.
	C_Portal_Player *pPlayer = dynamic_cast<C_Portal_Player*>( m_hPlayer.Get() );

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// Move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->SnatchModelInstance( this );

		VarMapping_t *varMap = GetVarMapping();

		// This is the local player, so set them in a default
		// pose and slam their velocity, angles and origin
		SetAbsOrigin( /* m_vecRagdollOrigin : */ pPlayer->GetRenderOrigin() );			
		SetAbsAngles( pPlayer->GetRenderAngles() );
		SetAbsVelocity( m_vecRagdollVelocity );

		// Hack! Find a neutral standing pose or use the idle.
		int iSeq = LookupSequence( "ragdoll" );
		if ( iSeq == -1 )
		{
			Assert( false );
			iSeq = 0;
		}			
		SetSequence( iSeq );
		SetCycle( 0.0 );

		Interp_Reset( varMap );

		m_nBody = pPlayer->GetBody();
		CopySequenceTransitions(pPlayer);

		SetModelIndex( m_nModelIndex );	
		// Make us a ragdoll..
		m_bClientSideRagdoll = true;

		matrix3x4a_t boneDelta0[MAXSTUDIOBONES];
		matrix3x4a_t boneDelta1[MAXSTUDIOBONES];
		matrix3x4a_t currentBones[MAXSTUDIOBONES];
		const float boneDt = 0.05f;

		pPlayer->GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );

		InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
IRagdoll* C_PortalRagdoll::GetIRagdoll() const
{
	return m_pRagdoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : type - 
//-----------------------------------------------------------------------------
void C_PortalRagdoll::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		CreatePortalRagdoll();
	}
}

BEGIN_RECV_TABLE_NOBASE( C_EntityPortalledNetworkMessage, DT_EntityPortalledNetworkMessage )
	RecvPropEHandle( RECVINFO_NAME( m_hEntity, m_hEntity ) ),
	RecvPropEHandle( RECVINFO_NAME( m_hPortal, m_hPortal ) ),
	RecvPropFloat( RECVINFO_NAME( m_fTime, m_fTime ) ),
	RecvPropBool( RECVINFO_NAME( m_bForcedDuck, m_bForcedDuck ) ),
	RecvPropInt( RECVINFO_NAME( m_iMessageCount, m_iMessageCount ) ),
END_RECV_TABLE()

// specific to the local player
BEGIN_RECV_TABLE_NOBASE( C_Portal_Player, DT_PortalLocalPlayerExclusive )
	RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ), 0, C_BasePlayer::RecvProxy_LocalOriginXY ),
	RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ), 0, C_BasePlayer::RecvProxy_LocalOriginZ ),
	RecvPropVector( RECVINFO( m_vecViewOffset ) ),

	RecvPropQAngles( RECVINFO( m_vecCarriedObjectAngles ) ),
	RecvPropVector( RECVINFO( m_vecCarriedObject_CurPosToTargetPos )  ),
	RecvPropQAngles( RECVINFO( m_vecCarriedObject_CurAngToTargetAng ) ),

	RecvPropUtlVector( RECVINFO_UTLVECTOR( m_EntityPortalledNetworkMessages ), C_Portal_Player::MAX_ENTITY_PORTALLED_NETWORK_MESSAGES, RecvPropDataTable(NULL, 0, 0, &REFERENCE_RECV_TABLE( DT_EntityPortalledNetworkMessage ) ) ),
	RecvPropInt( RECVINFO( m_iEntityPortalledNetworkMessageCount ) ),
END_RECV_TABLE()

// all players except the local player
BEGIN_RECV_TABLE_NOBASE( C_Portal_Player, DT_PortalNonLocalPlayerExclusive )
	RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ), 0, C_BasePlayer::RecvProxy_NonLocalCellOriginXY ),
	RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ), 0, C_BasePlayer::RecvProxy_NonLocalCellOriginZ ),
	RecvPropFloat( RECVINFO( m_vecViewOffset[0] ) ),
	RecvPropFloat( RECVINFO( m_vecViewOffset[1] ) ),
	RecvPropFloat( RECVINFO( m_vecViewOffset[2] ) ),
END_RECV_TABLE()


BEGIN_RECV_TABLE_NOBASE( CPortalPlayerShared, DT_PortalPlayerShared )
	RecvPropInt( RECVINFO( m_nPlayerCond ) ),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT(C_Portal_Player, DT_Portal_Player, CPortal_Player)
	
	RecvPropDataTable( RECVINFO_DT(m_PortalLocal),0, &REFERENCE_RECV_TABLE(DT_PortalLocal) ),

	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),
	RecvPropEHandle( RECVINFO( m_hRagdoll ) ),
	RecvPropInt( RECVINFO( m_iSpawnInterpCounter ) ),
	RecvPropInt( RECVINFO( m_iPlayerSoundType ) ),
	RecvPropBool( RECVINFO( m_bHeldObjectOnOppositeSideOfPortal ) ),
	RecvPropBool( RECVINFO( m_bPitchReorientation ) ),
	RecvPropEHandle( RECVINFO( m_hPortalEnvironment ) ),
	RecvPropBool( RECVINFO( m_bIsHoldingSomething ) ),
	RecvPropBool( RECVINFO( m_bPingDisabled ) ),
	RecvPropBool( RECVINFO( m_bTauntDisabled ) ),
	RecvPropBool( RECVINFO( m_bTauntRemoteView ) ),
	RecvPropVector( RECVINFO( m_vecRemoteViewOrigin ) ),
	RecvPropVector( RECVINFO( m_vecRemoteViewAngles ) ),
	RecvPropFloat( RECVINFO( m_fTauntCameraDistance ) ),
	RecvPropInt( RECVINFO( m_nTeamTauntState ) ),
	RecvPropVector( RECVINFO( m_vTauntPosition ) ),
	RecvPropQAngles( RECVINFO( m_vTauntAngles ) ),
	RecvPropQAngles( RECVINFO( m_vPreTauntAngles ) ),
	RecvPropBool( RECVINFO( m_bTrickFire ) ),
	RecvPropEHandle( RECVINFO( m_hTauntPartnerInRange ) ),
	RecvPropString( RECVINFO( m_szTauntForce ) ),
	RecvPropDataTable( "portallocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_PortalLocalPlayerExclusive) ),
	RecvPropDataTable( "portalnonlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_PortalNonLocalPlayerExclusive) ),
	RecvPropBool( RECVINFO( m_bUseVMGrab ) ),
	RecvPropBool( RECVINFO( m_bUsingVMGrabState ) ),
	RecvPropEHandle( RECVINFO( m_hAttachedObject ) ),
	RecvPropEHandle( RECVINFO( m_hHeldObjectPortal ) ),
	RecvPropFloat( RECVINFO( m_flMotionBlurAmount ) ),

	RecvPropBool( RECVINFO( m_bWantsToSwapGuns ) ),
	RecvPropDataTable( RECVINFO_DT( m_Shared ), 0, &REFERENCE_RECV_TABLE( DT_PortalPlayerShared ) ),

	RecvPropFloat( RECVINFO( m_flHullHeight ) ),
	RecvPropBool( RECVINFO( m_iSpawnCounter ) ),

	RecvPropBool( RECVINFO( m_bPotatos ) ),

	RecvPropDataTable( RECVINFO_DT( m_StatsThisLevel ), 0, &REFERENCE_RECV_TABLE( DT_PortalPlayerStatistics ) ),

END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_Portal_Player )
	DEFINE_PRED_TYPEDESCRIPTION( m_PortalLocal, C_PortalPlayerLocalData ),
#ifdef PORTAL_PLAYER_PREDICTION
	DEFINE_PRED_FIELD( m_nSkin, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_nBody, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE ),
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flPlaybackRate, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_ARRAY_TOL( m_flEncodedController, FIELD_FLOAT, MAXSTUDIOBONECTRLS, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE, 0.02f ),
	DEFINE_PRED_FIELD( m_nNewSequenceParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nResetEventsParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),

	DEFINE_PRED_FIELD( m_hPortalEnvironment, FIELD_EHANDLE, FTYPEDESC_NOERRORCHECK ),
#endif // PORTAL_PLAYER_PREDICTION

	// PAINT
	DEFINE_PRED_TYPEDESCRIPTION( m_CachedJumpPower, PaintPowerInfo_t ),

	DEFINE_PRED_FIELD( m_flCachedJumpPowerTime, FIELD_FLOAT, 0 ),
	DEFINE_PRED_FIELD( m_bJumpWasPressedWhenForced, FIELD_BOOLEAN, 0 ),
	DEFINE_PRED_FIELD( m_flSpeedDecelerationTime, FIELD_FLOAT, 0 ),
	DEFINE_PRED_FIELD( m_flUsePostTeleportationBoxTime, FIELD_FLOAT, 0 ),

	DEFINE_PRED_FIELD( m_flHullHeight, FIELD_FLOAT, 0 ),

	DEFINE_FIELD( m_fLatestServerTeleport, FIELD_FLOAT ),
	DEFINE_FIELD( m_matLatestServerTeleportationInverseMatrix, FIELD_VMATRIX ),

END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( player, C_Portal_Player );

#define	_WALK_SPEED 150
#define	_NORM_SPEED 190
#define	_SPRINT_SPEED 320

static ConVar cl_playermodel( "cl_playermodel", "none", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, "Default Player Model");
extern ConVar sv_post_teleportation_box_time;

//EHANDLE g_eKillTarget1;
//EHANDLE g_eKillTarget2;

void SpawnBlood (Vector vecSpot, const Vector &vecDir, int bloodColor, float flDamage);

C_Portal_Player::C_Portal_Player()
	: m_iv_angEyeAngles( "C_Portal_Player::m_iv_angEyeAngles" ),
	m_iv_flHullHeight( "C_Portal_Player::m_iv_flHullHeight" ),
	m_iv_vecCarriedObject_CurPosToTargetPos_Interpolator( "C_BaseEntity::m_iv_vecCarriedObject_CurPosToTargetPos_Interpolator" ),
	m_iv_vecCarriedObject_CurAngToTargetAng_Interpolator( "C_BaseEntity::m_iv_vecCarriedObject_CurAngToTargetAng_Interpolator" ),
	m_iv_vEyeOffset( "C_Portal_Player::m_iv_vEyeOffset" ),
	m_flMotionBlurAmount( -1.0f ),
	m_bIsBendy( false )
{
	m_PlayerAnimState = CreatePortalPlayerAnimState( this );

	m_iSpawnInterpCounterCache = 0;

	m_hRagdoll.Set( NULL );
	m_flStartLookTime = 0.0f;

	m_bHeldObjectOnOppositeSideOfPortal = false;
	m_hHeldObjectPortal = NULL;

	m_bPitchReorientation = false;
	m_fReorientationRate = 0.0f;

	m_flPitchFixup = 0.0f;
	m_flUprightRotDist = 0.0f;

	m_angEyeAngles.Init();
	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );

	m_flHullHeight = GetHullHeight();
	AddVar( &m_flHullHeight, &m_iv_flHullHeight, LATCH_SIMULATION_VAR );

	AddVar( &m_PortalLocal.m_vEyeOffset, &m_iv_vEyeOffset, LATCH_SIMULATION_VAR  );

	m_iv_vecCarriedObject_CurPosToTargetPos_Interpolator.Setup( &m_vecCarriedObject_CurPosToTargetPos_Interpolated, INTERPOLATE_LINEAR_ONLY );
	m_iv_vecCarriedObject_CurAngToTargetAng_Interpolator.Setup( &m_vecCarriedObject_CurAngToTargetAng_Interpolated, INTERPOLATE_LINEAR_ONLY );

	m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;
	m_blinkTimer.Invalidate();

	m_flUseKeyStartTime = -sv_use_trace_duration.GetFloat() - 1.0f ;
	m_nUseKeyEntFoundCommandNum = -1;
	m_nUseKeyEntClearCommandNum = -1;
	m_nLastRecivedCommandNum = -1;
	m_hUseEntToSend = NULL;
	m_hUseEntThroughPortal = NULL;
	m_flAutoGrabLockOutTime = 0.0f;

	m_bForcingDrop = false;
	m_bUseVMGrab = false;
	m_bUsingVMGrabState = false;

	m_bUseWasDown = false;

	m_bForceFireNextPortal = false;

	m_flImplicitVerticalStepSpeed = 0.0f;
	m_flObjectOutOfEyeTransitionDT = 0.0f;

	m_vInputVector = vec3_origin;
	m_flCachedJumpPowerTime = -FLT_MAX;
	m_flUsePostTeleportationBoxTime = 0.0f;
	m_flSpeedDecelerationTime = 0.0f;
	m_flPredictedJumpTime = 0.f;
	m_bJumpWasPressedWhenForced = false;
	m_vPrevGroundNormal = vec3_origin;

	m_flTimeSinceLastTouchedPower[0] = FLT_MAX;
	m_flTimeSinceLastTouchedPower[1] = FLT_MAX;
	m_flTimeSinceLastTouchedPower[2] = FLT_MAX;

	m_bFaceTauntCameraEndAngles = false;
	m_bFinishingTaunt = false;

	m_bDoneAirTauntHint = false;

	m_bWasTaunting = false;
	m_bFlingTrailActive = false;
	m_bFlingTrailJustPortalled = false;
	m_bFlingTrailPrePortalled = false;

	m_fTeamTauntStartTime = 0.0f;
	m_nOldTeamTauntState = TEAM_TAUNT_NONE;

	m_angTauntPredViewAngles.Init();
	m_angTauntEngViewAngles.Init();

	m_bGibbed = false;

	m_Shared.Init( this );
}

C_Portal_Player::~C_Portal_Player( void )
{
	if ( m_PlayerAnimState )
	{
		m_PlayerAnimState->Release();
	}
}


void C_Portal_Player::UpdateOnRemove( void )
{
	if( g_pGameRules->IsMultiplayer() )
	{
		RemoveRemoteSplitScreenViewPlayer( this );
	}

	// Stop the taunt.
	if ( m_bWasTaunting )
	{
		TurnOffTauntCam_Finish();
	}

	if ( m_FlingTrailEffect && m_FlingTrailEffect.IsValid() )
	{
		// stop the effect
		m_FlingTrailEffect->StopEmission( false, true, false );
		m_FlingTrailEffect = NULL;
	}

	DestroyPingPointer();
	
	// All conditions should be removed
	m_Shared.RemoveAllCond();
	if ( m_pHeldEntityClone )
	{
		GetGrabController().DetachEntity( false );
		m_pHeldEntityClone->Release();
		m_pHeldEntityClone = NULL;
	}

	if ( m_pHeldEntityThirdpersonClone )
	{
		GetGrabController().DetachEntity( false );
		m_pHeldEntityThirdpersonClone->Release();
		m_pHeldEntityThirdpersonClone = NULL;
	}

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	m_Inventory.RemoveListener( this );

	RemoveClientsideWearables();
#endif

	BaseClass::UpdateOnRemove();
}

void C_Portal_Player::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	Vector vecOrigin = ptr->endpos - vecDir * 4;

	float flDistance = 0.0f;

	if ( info.GetAttacker() )
	{
		flDistance = (ptr->endpos - info.GetAttacker()->GetAbsOrigin()).Length();
	}

	if ( m_takedamage )
	{
		AddMultiDamage( info, this );

		int blood = BloodColor();

		if ( blood != DONT_BLEED )
		{
			SpawnBlood( vecOrigin, vecDir, blood, flDistance );// a little surface blood.
			TraceBleed( flDistance, vecDir, ptr, info.GetDamageType() );
		}
	}
}

void C_Portal_Player::Initialize( void )
{
	m_headYawPoseParam = LookupPoseParameter(  "head_yaw" );
	GetPoseParameterRange( m_headYawPoseParam, m_headYawMin, m_headYawMax );

	m_headPitchPoseParam = LookupPoseParameter( "head_pitch" );
	GetPoseParameterRange( m_headPitchPoseParam, m_headPitchMin, m_headPitchMax );

	CStudioHdr *hdr = GetModelPtr();
	for ( int i = 0; i < hdr->GetNumPoseParameters() ; i++ )
	{
		SetPoseParameter( hdr, i, 0.0 );
	}

	if( g_pGameRules->IsMultiplayer() )
	{
		// Tell the player how to use the portalgun
		C_WeaponPortalgun *pPortalGun = static_cast<C_WeaponPortalgun *>( Weapon_OwnsThisType( "weapon_portalgun", 0 ) );
		if ( pPortalGun )
		{
			if ( pPortalGun->CanFirePortal2() )
			{
				IGameEvent *event = gameeventmanager->CreateEvent( "portal_enabled" );
				if ( event )
				{
					event->SetInt( "userid", GetUserID() );
					event->SetBool( "leftportal", false );

					gameeventmanager->FireEventClientSide( event );
				}
			}

			if ( pPortalGun->CanFirePortal1() )
			{
				IGameEvent *event = gameeventmanager->CreateEvent( "portal_enabled" );
				if ( event )
				{
					event->SetInt( "userid", GetUserID() );
					event->SetBool( "leftportal", true );

					gameeventmanager->FireEventClientSide( event );
				}
			}
		}
	}
}

CStudioHdr *C_Portal_Player::OnNewModel( void )
{
	CStudioHdr *hdr = BaseClass::OnNewModel();

	Initialize( );

	return hdr;
}

//-----------------------------------------------------------------------------
/**
* Orient head and eyes towards m_lookAt.
*/
void C_Portal_Player::UpdateLookAt( void )
{
	// head yaw
	if (m_headYawPoseParam < 0 || m_headPitchPoseParam < 0)
		return;

	// This is buggy with dt 0, just skip since there is no work to do.
	if ( gpGlobals->frametime <= 0.0f )
		return;

	// Player looks at themselves through portals. Pick the portal we're turned towards.
	const int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
	float *fPortalDot = (float *)stackalloc( sizeof( float ) * iPortalCount );
	float flLowDot = 1.0f;
	int iUsePortal = -1;

	// defaults if no portals are around
	Vector vPlayerForward;
	GetVectors( &vPlayerForward, NULL, NULL );
	Vector vCurLookTarget = EyePosition();

	if ( !IsAlive() )
	{
		m_viewtarget = EyePosition() + vPlayerForward*10.0f;
		return;
	}

	bool bNewTarget = false;
	if ( UTIL_IntersectEntityExtentsWithPortal( this ) != NULL )
	{
		// player is in a portal
		vCurLookTarget = EyePosition() + vPlayerForward*10.0f;
	}
	else if ( pPortals && pPortals[0] )
	{
		// Test through any active portals: This may be a shorter distance to the target
		for( int i = 0; i != iPortalCount; ++i )
		{
			CPortal_Base2D *pTempPortal = pPortals[i];

			if( pTempPortal && pTempPortal->IsActive() && pTempPortal->m_hLinkedPortal.Get() )
			{
				Vector vEyeForward, vPortalForward;
				EyeVectors( &vEyeForward );
				pTempPortal->GetVectors( &vPortalForward, NULL, NULL );
				fPortalDot[i] = vEyeForward.Dot( vPortalForward );
				if ( fPortalDot[i] < flLowDot )
				{
					flLowDot = fPortalDot[i];
					iUsePortal = i;
				}
			}
		}

		if ( iUsePortal >= 0 )
		{
			CPortal_Base2D* pPortal = pPortals[iUsePortal];
			if ( pPortal )
			{
				vCurLookTarget = pPortal->MatrixThisToLinked()*vCurLookTarget;
				if ( vCurLookTarget != m_vLookAtTarget )
				{
					bNewTarget = true;
				}
			}
		}
	}
	else
	{
		// No other look targets, look straight ahead
		vCurLookTarget += vPlayerForward*10.0f;
	}

	// Figure out where we want to look in world space.
	QAngle desiredAngles;
	Vector to = vCurLookTarget - EyePosition();
	VectorAngles( to, desiredAngles );
	QAngle aheadAngles;
	VectorAngles( vCurLookTarget, aheadAngles );

	// Figure out where our body is facing in world space.
	QAngle bodyAngles( 0, 0, 0 );
	bodyAngles[YAW] = GetLocalAngles()[YAW];

	m_flLastBodyYaw = bodyAngles[YAW];

	// Set the head's yaw.
	float desiredYaw = AngleNormalize( desiredAngles[YAW] - bodyAngles[YAW] );
	desiredYaw = clamp( desiredYaw, m_headYawMin, m_headYawMax );

	float desiredPitch = AngleNormalize( desiredAngles[PITCH] );
	desiredPitch = clamp( desiredPitch, m_headPitchMin, m_headPitchMax );

	if ( bNewTarget )
	{
		m_flStartLookTime = gpGlobals->curtime;
	}

	float dt = (gpGlobals->frametime);
	float flSpeed	= 1.0f - ExponentialDecay( 0.7f, 0.033f, dt );

	m_flCurrentHeadYaw = m_flCurrentHeadYaw + flSpeed * ( desiredYaw - m_flCurrentHeadYaw );
	m_flCurrentHeadYaw	= AngleNormalize( m_flCurrentHeadYaw );
	SetPoseParameter( m_headYawPoseParam, m_flCurrentHeadYaw );	

	m_flCurrentHeadPitch = m_flCurrentHeadPitch + flSpeed * ( desiredPitch - m_flCurrentHeadPitch );
	m_flCurrentHeadPitch = AngleNormalize( m_flCurrentHeadPitch );
	SetPoseParameter( m_headPitchPoseParam, m_flCurrentHeadPitch );

	// This orients the eyes
	m_viewtarget = m_vLookAtTarget = vCurLookTarget;
}

bool C_Portal_Player::PortalledMessageIsPending() const
{
	return m_bPortalledMessagePending;
}

const char *C_Portal_Player::GetVOIPParticleEffectName( void ) const
{ 
	if ( GetTeamNumber() == TEAM_BLUE )
		return "coop_robot_talk_blue"; 

	return "coop_robot_talk_orange"; 
}

CNewParticleEffect *C_Portal_Player::GetVOIPParticleEffect( void )
{
	return ParticleProp()->Create( GetVOIPParticleEffectName(), PATTACH_POINT_FOLLOW, "antenna_light" );
}

//-----------------------------------------------------------------------------
// Purpose: Try to steer away from any players and objects we might interpenetrate
//-----------------------------------------------------------------------------
#define PORTAL_AVOID_MAX_RADIUS_SQR		5184.0f			// Based on player extents and max buildable extents.
#define PORTAL_OO_AVOID_MAX_RADIUS_SQR	0.00019f

ConVar portal_use_player_avoidance( "portal_use_player_avoidance", "0", FCVAR_REPLICATED );
ConVar portal_max_separation_force ( "portal_max_separation_force", "256", FCVAR_DEVELOPMENTONLY );

extern ConVar cl_forwardspeed;
extern ConVar cl_backspeed;
extern ConVar cl_sidespeed;

void C_Portal_Player::AvoidPlayers( CUserCmd *pCmd )
{
	// Don't test if the player doesn't exist or is dead.
	if ( IsAlive() == false )
		return;

	// Up vector.
	static Vector vecUp( 0.0f, 0.0f, 1.0f );

	Vector vecPlayerCenter	= GetAbsOrigin();
	Vector vecPlayerMin		= GetPlayerMins();
	Vector vecPlayerMax		= GetPlayerMaxs();
	float flZHeight = vecPlayerMax.z - vecPlayerMin.z;
	vecPlayerCenter.z += 0.5f * flZHeight;
	VectorAdd( vecPlayerMin, vecPlayerCenter, vecPlayerMin );
	VectorAdd( vecPlayerMax, vecPlayerCenter, vecPlayerMax );

	// Find an intersecting player or object.
	int nAvoidPlayerCount = 0;
	C_Portal_Player *pAvoidPlayerList[MAX_PLAYERS];

	C_Portal_Player *pIntersectPlayer = NULL;
	float flAvoidRadius = 0.0f;

	Vector vecAvoidCenter, vecAvoidMin, vecAvoidMax;
	// for ( int i = 0; i < pTeam->GetNumPlayers(); ++i )
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		// C_TFPlayer *pAvoidPlayer = static_cast< C_TFPlayer * >( pTeam->GetPlayer( i ) );
		C_Portal_Player *pAvoidPlayer = static_cast< C_Portal_Player * >( UTIL_PlayerByIndex(i) );
		if ( pAvoidPlayer == NULL )
			continue;
		
		// Is the avoid player me?
		if ( pAvoidPlayer == this )
			continue;

		// Save as list to check against for objects.
		pAvoidPlayerList[nAvoidPlayerCount] = pAvoidPlayer;
		++nAvoidPlayerCount;

		// Check to see if the avoid player is dormant.
		if ( pAvoidPlayer->IsDormant() )
			continue;

		// Is the avoid player solid?
		if ( pAvoidPlayer->IsSolidFlagSet( FSOLID_NOT_SOLID ) )
			continue;

		Vector t1, t2;

		vecAvoidCenter = pAvoidPlayer->GetAbsOrigin();
		vecAvoidMin = pAvoidPlayer->GetPlayerMins();
		vecAvoidMax = pAvoidPlayer->GetPlayerMaxs();
		flZHeight = vecAvoidMax.z - vecAvoidMin.z;
		vecAvoidCenter.z += 0.5f * flZHeight;
		VectorAdd( vecAvoidMin, vecAvoidCenter, vecAvoidMin );
		VectorAdd( vecAvoidMax, vecAvoidCenter, vecAvoidMax );

		if ( IsBoxIntersectingBox( vecPlayerMin, vecPlayerMax, vecAvoidMin, vecAvoidMax ) )
		{
			// Need to avoid this player.
			if ( !pIntersectPlayer )
			{
				pIntersectPlayer = pAvoidPlayer;
				break;
			}
		}
	}

	// Anything to avoid?
	if ( pIntersectPlayer == NULL )
	{
		// m_Shared.SetSeparation( false );
		// m_Shared.SetSeparationVelocity( vec3_origin );
		return;
	}

	// Calculate the push strength and direction.
	Vector vecDelta;

	// Avoid a player - they have precedence.
	VectorSubtract( pIntersectPlayer->WorldSpaceCenter(), vecPlayerCenter, vecDelta );

	Vector vRad = pIntersectPlayer->WorldAlignMaxs() - pIntersectPlayer->WorldAlignMins();
	vRad.z = 0;

	flAvoidRadius = vRad.Length();

	float flPushStrength = RemapValClamped( vecDelta.Length(), flAvoidRadius, 0, 0, portal_max_separation_force.GetInt() );

	// Check to see if we have enough push strength to make a difference.
	if ( flPushStrength < 0.01f )
		return;

	Vector vecPush;
	if ( GetAbsVelocity().Length2DSqr() > 0.1f )
	{
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity.z = 0.0f;
		CrossProduct( vecUp, vecVelocity, vecPush );
		VectorNormalize( vecPush );
	}
	else
	{
		// We are not moving, but we're still intersecting.
		QAngle angView = pCmd->viewangles;
		angView.x = 0.0f;
		AngleVectors( angView, NULL, &vecPush, NULL );
	}

	// Move away from the other player/object.
	Vector vecSeparationVelocity;
	if ( vecDelta.Dot( vecPush ) < 0 )
	{
		vecSeparationVelocity = vecPush * flPushStrength;
	}
	else
	{
		vecSeparationVelocity = vecPush * -flPushStrength;
	}

	// Don't allow the max push speed to be greater than the max player speed.
	float flMaxPlayerSpeed = MaxSpeed();
	float flCropFraction = 1.33333333f;

	if ( ( GetFlags() & FL_DUCKING ) && ( GetGroundEntity() != NULL ) )
	{	
		flMaxPlayerSpeed *= flCropFraction;
	}	

	float flMaxPlayerSpeedSqr = flMaxPlayerSpeed * flMaxPlayerSpeed;

	if ( vecSeparationVelocity.LengthSqr() > flMaxPlayerSpeedSqr )
	{
		vecSeparationVelocity.NormalizeInPlace();
		VectorScale( vecSeparationVelocity, flMaxPlayerSpeed, vecSeparationVelocity );
	}

	QAngle vAngles = pCmd->viewangles;
	vAngles.x = 0;
	Vector currentdir;
	Vector rightdir;

	AngleVectors( vAngles, &currentdir, &rightdir, NULL );

	Vector vDirection = vecSeparationVelocity;

	VectorNormalize( vDirection );

	float fwd = currentdir.Dot( vDirection );
	float rt = rightdir.Dot( vDirection );

	float forward = fwd * flPushStrength;
	float side = rt * flPushStrength;

	//Msg( "fwd: %f - rt: %f - forward: %f - side: %f\n", fwd, rt, forward, side );

	// m_Shared.SetSeparation( true );
	// m_Shared.SetSeparationVelocity( vecSeparationVelocity );

	pCmd->forwardmove	+= forward;
	pCmd->sidemove		+= side;

	// Clamp the move to within legal limits, preserving direction. This is a little
	// complicated because we have different limits for forward, back, and side

	//Msg( "PRECLAMP: forwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );

	float flForwardScale = 1.0f;
	if ( pCmd->forwardmove > fabs( cl_forwardspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_forwardspeed.GetFloat() ) / pCmd->forwardmove;
	}
	else if ( pCmd->forwardmove < -fabs( cl_backspeed.GetFloat() ) )
	{
		flForwardScale = fabs( cl_backspeed.GetFloat() ) / fabs( pCmd->forwardmove );
	}

	float flSideScale = 1.0f;
	if ( fabs( pCmd->sidemove ) > fabs( cl_sidespeed.GetFloat() ) )
	{
		flSideScale = fabs( cl_sidespeed.GetFloat() ) / fabs( pCmd->sidemove );
	}

	float flScale = MIN( flForwardScale, flSideScale );
	pCmd->forwardmove *= flScale;
	pCmd->sidemove *= flScale;

	//Msg( "Pforwardmove=%f, sidemove=%f\n", pCmd->forwardmove, pCmd->sidemove );
}

// Remote viewing
extern ConVar cl_enable_remote_splitscreen;

extern ConVar sv_enableholdrotation;

// When set to true, VGui_OnSplitScreenStateChanged will NOT change the current system level.
extern bool g_bSuppressConfigSystemLevelDueToPIPTransitions;
static ConVar cl_suppress_config_system_level_changes_on_pip_transitions( "cl_suppress_config_system_level_changes_on_pip_transitions", "1", FCVAR_DEVELOPMENTONLY );

bool C_Portal_Player::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	if ( sv_enableholdrotation.GetBool() && !IsUsingVMGrab() )
	{
		if( m_bIsHoldingSomething && (pCmd->buttons & IN_ATTACK2) )
		{
			//stomp view angle changes. When holding right click and holding something, we remap mouse movement to rotate the object instead of our view
			pCmd->viewangles = m_vecOldViewAngles;
			engine->SetViewAngles( m_vecOldViewAngles );
		}
	}

	// if we are in coop and not split screen, do PIP if the remote_view button is pressed
	if ( GameRules()->IsMultiplayer() && !( IsSplitScreenPlayer() || GetSplitScreenPlayers().Count() > 0 ) )
	{
		bool bOtherPlayerIsTaunting = false;
		bool bIsOtherPlayerRemoteViewTaunt = false;

		if ( cl_auto_taunt_pip.GetBool() )
		{
			C_Portal_Player *pOtherPlayer = ToPortalPlayer( UTIL_OtherPlayer( this ) );
			if ( pOtherPlayer )
			{
				bOtherPlayerIsTaunting = pOtherPlayer->m_Shared.InCond( PORTAL_COND_TAUNTING );
				bIsOtherPlayerRemoteViewTaunt = pOtherPlayer->IsRemoteViewTaunt();
			}
		}

		bool bRemoteViewPressed = ( pCmd->buttons & IN_REMOTE_VIEW ) != 0;

		bool bUsingPIP = m_nTeamTauntState < TEAM_TAUNT_HAS_PARTNER && 
						 ( ( bOtherPlayerIsTaunting && !bIsOtherPlayerRemoteViewTaunt ) || ( !bOtherPlayerIsTaunting && bRemoteViewPressed ) );
		
		// Hack: Suppress changes of the current system level during this transition (see vgui_int.cpp, VGui_OnSplitScreenStateChanged()).
		g_bSuppressConfigSystemLevelDueToPIPTransitions = cl_suppress_config_system_level_changes_on_pip_transitions.GetBool();
				
		cl_enable_remote_splitscreen.SetValue( bUsingPIP );

		g_bSuppressConfigSystemLevelDueToPIPTransitions = false;
	}

	static QAngle angMoveAngle( 0.0f, 0.0f, 0.0f );

	bool bNoTaunt = true;
	if ( IsTaunting() )
	{
		pCmd->forwardmove = 0.0f;
		pCmd->sidemove = 0.0f;
		pCmd->upmove = 0.0f;
		pCmd->buttons = 0;
		pCmd->weaponselect = 0;

		VectorCopy( angMoveAngle, pCmd->viewangles );

		bNoTaunt = false;
	}
	else
	{
		VectorCopy( pCmd->viewangles, angMoveAngle );
	}

	if ( GetFOV() != GetDefaultFOV() )
	{
		if ( IsTaunting() )
		{
			// Pop out of zoom when I'm taunting
			pCmd->buttons &= ~IN_ZOOM;
			KeyUp( &in_zoom, NULL );
		}
		else if ( GetVehicle() != NULL )
		{
			pCmd->buttons &= ~IN_ZOOM;
			KeyUp( &in_zoom, NULL );
		}
		else
		{
			float fThreshold = sv_zoom_stop_movement_threashold.GetFloat();
			if ( gpGlobals->curtime > GetFOVTime() + sv_zoom_stop_time_threashold.GetFloat() && 
				 ( fabsf( pCmd->forwardmove ) > fThreshold ||  fabsf( pCmd->sidemove ) > fThreshold ) )
			{
				// Pop out of the zoom if we're moving
				pCmd->buttons &= ~IN_ZOOM;
				KeyUp( &in_zoom, NULL );
			}
		}
	}

	// Bump away from other players
	AvoidPlayers( pCmd );
	
	PollForUseEntity( pCmd );
	m_bUseWasDown = (pCmd->buttons & IN_USE) != 0;

	pCmd->player_held_entity = ( m_hUseEntToSend ) ? ( m_hUseEntToSend->entindex() ) : ( 0 );
	pCmd->held_entity_was_grabbed_through_portal = ( m_hUseEntThroughPortal ) ? ( m_hUseEntThroughPortal->entindex() ) : ( 0 );
	
	pCmd->command_acknowledgements_pending = pCmd->command_number - engine->GetLastAcknowledgedCommand();
	pCmd->predictedPortalTeleportations = 0;
	for( int i = 0; i != m_PredictedPortalTeleportations.Count(); ++i )
	{
		if( m_PredictedPortalTeleportations[i].iCommandNumber > pCmd->command_number )
			break;

		++pCmd->predictedPortalTeleportations;
	}

	if ( m_bForceFireNextPortal )
	{
		C_WeaponPortalgun *pPortalGun = dynamic_cast< C_WeaponPortalgun* >( GetActiveWeapon() );
		if ( pPortalGun )
		{
			if ( pPortalGun->GetLastFiredPortal() == 1 )
			{
				pCmd->buttons |= IN_ATTACK2;
			}
			else
			{
				pCmd->buttons |= IN_ATTACK;
			}
		}

		if ( pCmd->command_number != 0 )
		{
			m_bForceFireNextPortal = false;
		}
	}

	BaseClass::CreateMove( flInputSampleTime, pCmd );
	
	return bNoTaunt;
}

// Calls FindUseEntity every tick for a period of time
// I'm REALLY sorry about this. This will clean up quite a bit
// once grab controllers are on the client.
void C_Portal_Player::PollForUseEntity( CUserCmd *pCmd )
{
	// Record the last received non-zero command number. 
	// Non-zero is a sloppy way of telling which CreateMoves came from ExtraMouseSample :/
	if ( pCmd->command_number != 0 )
	{
		m_nLastRecivedCommandNum = pCmd->command_number;
	}
	else if ( m_nLastRecivedCommandNum == -1 )
	{
		// If we just constructed, don't run anything below during extra mouse samples
		// until we've set m_nLastRecivedCommandNum to the correct command number from the engine
		return;
	}

	// 82077: Polling for the use entity causes the vehicle view to cache at a time
	// when the attachments are incorrect... We dont need use in a vehicle
	// so this hack prevents that caching from happening and keeps the view smooth
	// The only time we have a vehicle is during the ending sequence so this shouldn't be a big issue.
	if ( IsX360() && GetVehicle() )
		return;

	// Get rid of the use ent if we've already sent the last one up to the server
	if ( m_nLastRecivedCommandNum == m_nUseKeyEntClearCommandNum )	
	{
		m_hUseEntToSend = NULL;
		m_hUseEntThroughPortal = NULL;
		m_nUseKeyEntFoundCommandNum = -1;
		m_nUseKeyEntClearCommandNum = -1;
	}

	bool bBasicUse = ( ( pCmd->buttons & IN_USE ) != 0 && m_bUseWasDown == false );

	C_BaseEntity *pUseEnt = NULL;
	C_Portal_Base2D *pUseThroughPortal = NULL;
	PollForUseEntity( bBasicUse, &pUseEnt, &pUseThroughPortal );	// Call the shared poll logic in portal_player_shared

	if ( pUseEnt )
	{
		m_hUseEntToSend = pUseEnt;
		m_hUseEntThroughPortal = pUseThroughPortal;

		m_flUseKeyStartTime = -sv_use_trace_duration.GetFloat() - 1.0f;
		// Record the tick we found this, so we can invalidate the handle after it is sent
		m_nUseKeyEntFoundCommandNum = m_nLastRecivedCommandNum;
		// If we caught this during 'extra mouse sample' calls, clear on the cmd after next, otherwise clear next command.
		m_nUseKeyEntClearCommandNum = m_nUseKeyEntFoundCommandNum + ((pCmd->command_number == 0) ? ( 2 ) : ( 1 ));
	}
	
	// If we already found an ent this tick, fake a use key down so the server
	// will treat this as a normal +use 
	if ( m_nLastRecivedCommandNum < m_nUseKeyEntClearCommandNum && m_hUseEntToSend.Get() && 
		 // HACK: don't do this behaviour through a portal for now (54969)
		 // This will be really awkward to fix for real with grab controllers living on the server. 
		 m_hUseEntThroughPortal.Get() == NULL )
	{
		pCmd->buttons |= IN_USE;
	}
}
void C_Portal_Player::ClientThink( void )
{
	bool bIsMultiplayer = GameRules() && GameRules()->IsMultiplayer();
	Vector vForward;
	AngleVectors( GetLocalAngles(), &vForward );
	if( PortalledMessageIsPending() )
	{
		m_flUsePostTeleportationBoxTime = sv_post_teleportation_box_time.GetFloat();
		m_bPortalledMessagePending = false;
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

	// Air taunt lesson events
	if ( GetGroundEntity() )
	{
		if ( m_bDoneAirTauntHint )
		{
			m_bDoneAirTauntHint = false;

			IGameEvent *event = gameeventmanager->CreateEvent( "player_touched_ground" );
			if ( event )
			{
				event->SetInt( "userid", GetUserID() );
				gameeventmanager->FireEventClientSide( event );
			}
		}

		if ( bIsMultiplayer && IsLocalPlayer() )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );
			if( GET_ACTIVE_SPLITSCREEN_SLOT() == GetSplitScreenPlayerSlot() )
			{
				// Handle team taunts
				if ( mp_auto_taunt.GetBool() )
				{
					engine->ClientCmd( "taunt robotDance" );
				}
				else if ( mp_auto_accept_team_taunt.GetBool() || GetTeamTauntState() == TEAM_TAUNT_NEED_PARTNER )
				{
					C_Portal_Player *pPartnerPlayer = HasTauntPartnerInRange();
					if ( pPartnerPlayer && pPartnerPlayer->GetGroundEntity() && pPartnerPlayer->GetTauntForceName()[ 0 ] != '\0' && 
						 ( mp_auto_accept_team_taunt.GetBool() || strcmp( pPartnerPlayer->GetTauntForceName(), GetTauntForceName() ) == 0 ) )
					{
						engine->ClientCmd( "taunt team_accept" );
					}
				}
			}
		}
	}
	else if ( !m_bDoneAirTauntHint && GetAirTime() > 2.0f && PredictedAirTimeEnd() > 2.0f )
	{
		// We've been flying for a while and aren't headed very downward
		m_bDoneAirTauntHint = true;

		IGameEvent *event = gameeventmanager->CreateEvent( "player_long_fling" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			gameeventmanager->FireEventClientSide( event );
		}
	}

	// update gun's color
	Color color( 255, 255, 255 );
	C_WeaponPaintGun *pPaintGun = dynamic_cast< C_WeaponPaintGun* >( GetActiveWeapon() );
	if ( pPaintGun && pPaintGun->HasAnyPaintPower() )
	{
		pPaintGun->ChangeRenderColor();
	}
	else if ( GetViewModel() )
	{
		GetViewModel()->SetRenderColor( color.r(), color.g(), color.b() );
	}

	RANDOM_CEG_TEST_SECRET();

	if( IsLocalPlayer() )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );
		if( GET_ACTIVE_SPLITSCREEN_SLOT() == GetSplitScreenPlayerSlot() )
		{
			QAngle viewAngles;
			engine->GetViewAngles(pl.v_angle);

			Reorient( pl.v_angle );

			engine->SetViewAngles( pl.v_angle );
		}
		else
		{
			Reorient( pl.v_angle );
		}
	}
	else
	{
		Reorient( pl.v_angle );
	}


	if( !cl_predict->GetInt() )
	{
		UpdatePaintedPower();
	}

	if ( bIsMultiplayer && mp_bot_fling_trail.GetInt() > 0 )
	{
		if ( !m_bFlingTrailPrePortalled )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

			// don't show the trail in first person because it doesn't look good right now
			if ( C_BasePlayer::GetLocalPlayer() != this || input->CAM_IsThirdPerson() )
			{
				float fFlingTrail = GetAbsVelocity().Length() - MIN_FLING_SPEED;

				if ( !m_bFlingTrailActive && !m_bFlingTrailPrePortalled && (m_bFlingTrailJustPortalled || fFlingTrail > 0.0f) )
				{
					if ( m_FlingTrailEffect )
					{
						// stop the effect
						m_FlingTrailEffect->StopEmission( false, false, false );
						m_FlingTrailEffect = NULL;
					}
			
					if ( !m_FlingTrailEffect || !m_FlingTrailEffect.IsValid() )
					{
						C_BaseAnimating::PushAllowBoneAccess( true, false, "mpbottrails" );	
						// create it
						if ( mp_bot_fling_trail.GetInt() == 2 )
							m_FlingTrailEffect = this->ParticleProp()->Create( "bot_fling_trail_rainbow", PATTACH_POINT_FOLLOW, "forward" );
						else
							m_FlingTrailEffect = this->ParticleProp()->Create( "bot_fling_trail", PATTACH_POINT_FOLLOW, "forward" );

						m_bFlingTrailActive = true;
						if ( m_FlingTrailEffect )
						{
							m_FlingTrailEffect->SetControlPoint( 1, Vector( fFlingTrail, 0, 0 ) );
						}
						C_BaseAnimating::PopBoneAccess( "mpbottrails" );
					}

					m_bFlingTrailJustPortalled = false;
				}
				else if ( fFlingTrail <= 0.0f && m_bFlingTrailActive && m_FlingTrailEffect )
				{
					// stop the effect
					m_FlingTrailEffect->StopEmission( false, false, false );
					m_FlingTrailEffect = NULL;
					m_bFlingTrailActive = false;
				}
			}
		}
	}

	if ( IsLocalPlayer() )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );
		if ( GET_ACTIVE_SPLITSCREEN_SLOT() == GetSplitScreenPlayerSlot() )
		{
			MoveHeldObjectOutOfPlayerEyes();
	
			if ( bIsMultiplayer && IsTaunting() && IsRemoteViewTaunt() )
			{
				ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );
				
				Vector vTargetPos = GetThirdPersonViewPosition();
				Vector vecDir = vTargetPos - m_vecRemoteViewOrigin;
				float flDist = VectorNormalize( vecDir );
				QAngle vecViewAngles;
				VectorAngles( vecDir, vecViewAngles );

				trace_t trace;
				UTIL_TraceLine( vTargetPos, m_vecRemoteViewOrigin, (CONTENTS_SOLID|CONTENTS_MOVEABLE), NULL, COLLISION_GROUP_NONE, &trace );

				if ( !trace.startsolid && trace.DidHit() )
				{
					flDist *= trace.fraction;
				}

				m_TauntCameraData.m_flPitch = vecViewAngles.x;
				m_TauntCameraData.m_flYaw = vecViewAngles.y;
				m_TauntCameraData.m_flDist = flDist;
				m_TauntCameraData.m_vecHullMin.Init( -1.0f, -1.0f, -1.0f );
				m_TauntCameraData.m_vecHullMax.Init( 1.0f, 1.0f, 1.0f );

				QAngle vecCameraOffset( vecViewAngles.x, vecViewAngles.y, flDist );
				input->CAM_SetCameraThirdData( &m_TauntCameraData, vecCameraOffset );
			}
		}
	}
}

void C_Portal_Player::MoveHeldObjectOutOfPlayerEyes( void )
{
	if ( player_held_object_keep_out_of_camera.GetBool() == false )
		return;

	// Not needed if we're not holding something
	if ( m_hAttachedObject.Get() == NULL )
		return;

	C_BaseAnimating *pAnim = m_hAttachedObject.Get()->GetBaseAnimating();
	if ( !pAnim )
		return;

	if ( IsUsingVMGrab() )
	{
		pAnim->DisableRenderOriginOverride();
		return;
	}

	// HACK: This level does some odd toggling of vm mode/physics mode during
	// a scene where this behavior isn't needed or desired.
	if ( V_strcmp( "sp_a1_wakeup", engine->GetLevelNameShort() ) == 0 )
		return;	

	Assert ( player_held_object_collide_with_player.GetBool() == false );

	Vector vLook, vRight, vUp;
	GetVectors( &vLook, &vRight, &vUp );
	Vector vToObject = m_hAttachedObject.Get()->GetAbsOrigin() - EyePosition();
	float distSq = vToObject.LengthSqr();
	const float flBufferZone = 10.0f; // start moving into safty earlier to avoid penetrating the eyeposition
	float rad = m_hAttachedObject.Get()->BoundingRadius() + flBufferZone;
	
	// dt moves between 0 and transition time over time depending on proximity to the eyepos 
	float dt = m_flObjectOutOfEyeTransitionDT;
	const float flTransitionTime = 0.4;
	dt += ( distSq < rad*rad ) ? gpGlobals->frametime : -gpGlobals->frametime;
	dt = clamp( dt, 0.0f, flTransitionTime );
	m_flObjectOutOfEyeTransitionDT = dt;

	// Move between our hidden position and our real world position depending on our closeness
	float t = RemapVal( dt, 0.0f, flTransitionTime, 0.0f, 1.0f );
	Vector out;
	Vector vGoalPos = EyePosition() - vUp*rad;// - vLook*rad;
	Vector vMid		= EyePosition() - vUp*rad - 3.0f*vLook*rad;
	Hermite_Spline( vMid, pAnim->GetAbsOrigin(), vGoalPos, t, out );
	pAnim->SetRenderOriginOverride( out );
}


Vector C_Portal_Player::GetThirdPersonViewPosition( void )
{
	if ( m_nTeamTauntState >= TEAM_TAUNT_HAS_PARTNER )
	{
		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			C_Portal_Player *pOtherPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

			//If the other player does not exist or if the other player is the local player
			if( pOtherPlayer == NULL || pOtherPlayer == this )
				continue;

			return ( GetRenderOrigin() + GetViewOffset() * 0.75f + pOtherPlayer->GetRenderOrigin() + pOtherPlayer->GetViewOffset() * 0.75f ) * 0.5f;
		}
	}

	Vector vFinalPos = GetRenderOrigin() + GetViewOffset() * 0.75f;

	if ( m_Shared.InCond( PORTAL_COND_DROWNING ) && !IsRemoteViewTaunt() )
	{
		vFinalPos.z = UTIL_FindWaterSurface( vFinalPos, vFinalPos.z - 128.0f, vFinalPos.z + 128.0f ) + 32.0f;
	}

	return vFinalPos;
}

const Vector& C_Portal_Player::GetRenderOrigin( void )
{
	float flHullHeight = ( GetGroundEntity() != NULL ) ? m_flHullHeight : GetStandHullHeight();
	m_vRenderOrigin = WorldSpaceCenter();
	m_vRenderOrigin -= 0.5f * flHullHeight * m_PortalLocal.m_StickNormal;

	float fInterp = 0.0f;

	if ( m_nTeamTauntState >= TEAM_TAUNT_HAS_PARTNER )
	{
		fInterp = mp_taunt_position_blend_rate.GetFloat() * ( gpGlobals->curtime - m_fTeamTauntStartTime );

		if ( gpGlobals->curtime - m_fTeamTauntStartTime > 20.0f )
		{
			if ( !engine->GetNetChannelInfo()->IsTimingOut() )
			{
				// Fail safe
				fInterp = 0.0f;
				DevWarning( "Client player has been in the team taunt state for longer than 20 seconds!\n" );
			}
		}
		else
		{
			fInterp = clamp( fInterp, 0.0, 1.0f );
		}
	}
	else if ( m_fTeamTauntStartTime > 0.0f )
	{
		fInterp = 1.0f - clamp( mp_taunt_position_blend_rate.GetFloat() * ( gpGlobals->curtime - m_fTeamTauntStartTime ), 0.0, 1.0f );
	}

	if ( fInterp > 0.0f )
	{
		Vector vOldTempRenderOrigin = m_vTempRenderOrigin;
		m_vTempRenderOrigin = m_vRenderOrigin + ( m_vTauntPosition - GetAbsOrigin() ) * fInterp;

		if ( vOldTempRenderOrigin != m_vTempRenderOrigin )
		{
			MarkRenderHandleDirty();
		}

		return m_vTempRenderOrigin;
	}
	else if ( !m_vTempRenderOrigin.IsZero() )
	{
		m_vTempRenderOrigin.Zero();
		MarkRenderHandleDirty();
	}

	
	return m_vRenderOrigin;
}


const QAngle& C_Portal_Player::GetRenderAngles()
{
	if ( IsRagdoll() )
		return vec3_angle;

	float fInterp = 0.0f;

	if ( m_nTeamTauntState >= TEAM_TAUNT_HAS_PARTNER )
	{
		fInterp = mp_taunt_position_blend_rate.GetFloat() * ( gpGlobals->curtime - m_fTeamTauntStartTime );

		if ( gpGlobals->curtime - m_fTeamTauntStartTime > 20.0f )
		{
			// Fail safe
			if ( !engine->GetNetChannelInfo()->IsTimingOut() )
			{
				fInterp = 0.0f;
				DevWarning( "Client player has been in the team taunt state for longer than 20 seconds!\n" );
			}
		}
		else
		{
			fInterp = clamp( fInterp, 0.0, 1.0f );
		}
	}
	else if ( m_fTeamTauntStartTime > 0.0f )
	{
		fInterp = 1.0f - clamp( mp_taunt_position_blend_rate.GetFloat() * ( gpGlobals->curtime - m_fTeamTauntStartTime ), 0.0, 1.0f );
	}

	if ( fInterp > 0.0f )
	{
		m_TempRenderAngles[ PITCH ] = ApproachAngle( m_vTauntAngles[ PITCH ], m_PlayerAnimState->GetRenderAngles()[ PITCH ], fInterp * 360.0f );
		m_TempRenderAngles[ YAW ] = ApproachAngle( m_vTauntAngles[ YAW ], m_PlayerAnimState->GetRenderAngles()[ YAW ], fInterp * 360.0f );
		m_TempRenderAngles[ ROLL ] = ApproachAngle( m_vTauntAngles[ ROLL ], m_PlayerAnimState->GetRenderAngles()[ ROLL ], fInterp * 360.0f );

		return m_TempRenderAngles;
	}

	return m_PlayerAnimState->GetRenderAngles();
}


void C_Portal_Player::UpdateClientSideAnimation( void )
{
	UpdateLookAt();

	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if ( C_BasePlayer::IsLocalPlayer( this ) )
	{
		m_PlayerAnimState->Update( EyeAngles()[YAW], m_angEyeAngles[PITCH] );
	}
	else
	{
		QAngle qEffectiveAngles;
		
		if( m_iv_angEyeAngles.GetInterpolatedTime( GetEffectiveInterpolationCurTime( gpGlobals->curtime ) ) < m_fLatestServerTeleport )
		{
			qEffectiveAngles = TransformAnglesToLocalSpace( m_angEyeAngles, m_matLatestServerTeleportationInverseMatrix.As3x4() );
		}
		else
		{
			qEffectiveAngles = m_angEyeAngles;
		}

		m_PlayerAnimState->Update( qEffectiveAngles[YAW], qEffectiveAngles[PITCH] );
	}

	BaseClass::UpdateClientSideAnimation();
}

void C_Portal_Player::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	if ( GetPredictable() && IsLocalPlayer( this ) )
	{
		if ( !prediction->IsFirstTimePredicted() )
			return;
	}

	MDLCACHE_CRITICAL_SECTION();
	m_PlayerAnimState->DoAnimationEvent( event, nData );
}

void C_Portal_Player::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	switch( event )
	{
	case AE_WPN_PRIMARYATTACK:
		{
			if ( IsLocalPlayer() )
			{
				m_bForceFireNextPortal = true;
			}
			break;
		}

	default:
		BaseClass::FireEvent( origin, angles,event, options );
	}
}


bool Util_PIP_ShouldDrawPlayer( C_Portal_Player* pPortalPlayer )
{
	return VGui_IsSplitScreen() && !pPortalPlayer->IsLocalPlayer() && ( pPortalPlayer->m_Shared.InCond( PORTAL_COND_TAUNTING ) || 
																pPortalPlayer->m_Shared.InCond( PORTAL_COND_DROWNING ) || 
																pPortalPlayer->m_Shared.InCond( PORTAL_COND_DEATH_CRUSH ) ||
																pPortalPlayer->m_Shared.InCond( PORTAL_COND_DEATH_GIB ) );
}

bool C_Portal_Player::ShouldSkipRenderingViewpointPlayerForThisView( void )
{
	if( !cl_skip_player_render_in_main_view.GetBool() )
		return false;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

	if ( !ShouldDrawLocalPlayer() && !Util_PIP_ShouldDrawPlayer( this ) )
	{
		if( g_pPortalRender->GetViewRecursionLevel() == 0 )
		{				
			//never draw if eye is still in the player's head. Always draw if the eye is transformed
			if( !m_bEyePositionIsTransformedByPortal )
				return true;
		}
		else if( g_pPortalRender->GetViewRecursionLevel() == 1 )
		{
			//Always draw if eye is still in the player's head. Draw for all portals except the inverse transform if eye is transformed
			if( m_bEyePositionIsTransformedByPortal && (g_pPortalRender->GetCurrentViewEntryPortal() == m_pNoDrawForRecursionLevelOne) )
				return true;
		}
	}

	return false;
}

IClientModelRenderable*	C_Portal_Player::GetClientModelRenderable()
{
	if( (GetSplitScreenViewPlayer() == this) && ShouldSkipRenderingViewpointPlayerForThisView() )
		return NULL;

	return BaseClass::GetClientModelRenderable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConVar cl_draw_player_model("cl_draw_player_model", "1", FCVAR_DEVELOPMENTONLY);
int C_Portal_Player::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_bReadyToDraw )
		return 0;

	if ( !cl_draw_player_model.GetBool() )
		return 0;

	if( (GetSplitScreenViewPlayer() == this) && ShouldSkipRenderingViewpointPlayerForThisView() )
		return 0;

	if( flags & STUDIO_RENDER )
	{
		m_nLastFrameDrawn = gpGlobals->framecount;
		m_nLastDrawnStudioFlags = flags;
	}
	return BaseClass::DrawModel( flags, instance );
}


class CAutoInitPlayerSilhoutteMaterials : public CAutoGameSystem
{
public:
	CMaterialReference	m_Material;
	IMaterialVar		*m_pTintVariable;
	void LevelInitPreEntity()
	{
		m_Material.Init( "models/player/chell_silhoutte", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_pTintVariable = m_Material->FindVar( "$color", NULL, false );
		Assert( m_pTintVariable != NULL );
	}
};
static CAutoInitPlayerSilhoutteMaterials s_PlayerSilhoutteMaterials;


enum PortalPlayerSkins_t
{
	SKIN_RED_CHELL,
	SKIN_BLUE_MEL,
	SKIN_SILHOUTTE,
	SKIN_RED_CHELL_NOHAIRSTRANDS,
	SKIN_BLUE_MEL_NOHAIRSTRANDS
};


//-----------------------------------------------------------------------------
// Should this object receive shadows?
//-----------------------------------------------------------------------------
bool C_Portal_Player::ShouldReceiveProjectedTextures( int flags )
{
	Assert( flags & SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK );

	if ( IsEffectActive( EF_NODRAW ) )
		return false;

	if( flags & SHADOW_FLAGS_FLASHLIGHT )
	{
		return true;
	}

	return BaseClass::ShouldReceiveProjectedTextures( flags );
}

void C_Portal_Player::DoImpactEffect( trace_t &tr, int nDamageType )
{
	if ( GetActiveWeapon() )
	{
		GetActiveWeapon()->DoImpactEffect( tr, nDamageType );
		return;
	}

	BaseClass::DoImpactEffect( tr, nDamageType );
}

void C_Portal_Player::PreThink( void )
{
	QAngle vTempAngles = GetLocalAngles();

	if ( IsLocalPlayer( this ) )
	{
		vTempAngles[PITCH] = EyeAngles()[PITCH];
	}
	else
	{
		vTempAngles[PITCH] = m_angEyeAngles[PITCH];
	}

	if ( vTempAngles[YAW] < 0.0f )
	{
		vTempAngles[YAW] += 360.0f;
	}

	SetLocalAngles( vTempAngles );

	BaseClass::PreThink();

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

	FixPortalEnvironmentOwnership();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_Portal_Player::Simulate( void )
{
	BaseClass::Simulate();

	QAngle vTempAngles = GetLocalAngles();
	vTempAngles[PITCH] = m_angEyeAngles[PITCH];

	SetLocalAngles( vTempAngles );

	// Zero out model pitch, blending takes care of all of it.
	SetLocalAnglesDim( X_INDEX, 0 );

	if( !C_BasePlayer::IsLocalPlayer( this ) )
	{
		if ( IsEffectActive( EF_DIMLIGHT ) )
		{
			int iAttachment = LookupAttachment( "anim_attachment_RH" );

			if ( iAttachment < 0 )
				return true;

			Vector vecOrigin;
			QAngle eyeAngles = m_angEyeAngles;

			GetAttachment( iAttachment, vecOrigin, eyeAngles );

			Vector vForward;
			AngleVectors( eyeAngles, &vForward );

			trace_t tr;
			UTIL_TraceLine( vecOrigin, vecOrigin + (vForward * 200), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		}
	}

	if( g_pGameRules->IsMultiplayer() && !GetPredictable() && //should have handled the grab controller in prediction, but it's off
		(IsUsingVMGrab() || (GetGrabController().GetAttached() != NULL)) ) //already handling a grabbed object, or should be using VM grab
	{
		ManageHeldObject();
	}

	return true;
}

ShadowType_t C_Portal_Player::ShadowCastType( void ) 
{
	// Drawing player shadows looks bad in first person when they get close to walls
	// It doesn't make sense to have shadows in the portal view, but not in the main view
	// So no shadows for the player
	return SHADOWS_NONE;
}

bool C_Portal_Player::ShouldDraw( void )
{
	if ( !BaseClass::ShouldDraw() )
		return false;

	if ( !IsAlive() )
	{
		if ( m_Shared.InCond( PORTAL_COND_DEATH_CRUSH ) )
			return true;
		else
			return false;
	}

	if ( Util_PIP_ShouldDrawPlayer( this ) )
	{
		return true;
	}

	//return true;

	//	if( GetTeamNumber() == TEAM_SPECTATOR )
	//		return false;

	if( IsLocalPlayer( this ) && IsRagdoll() )
		return true;

	if ( IsRagdoll() )
		return false;

	return true;

	//return BaseClass::ShouldDraw();
}

bool C_Portal_Player::ShouldSuppressForSplitScreenPlayer( int nSlot )
{
	//To properly handle ghost animatings of players through a portal. We MUST draw the player's model in the main view if their eye is transformed by a portal
	//That requires that this function return true. C_Portal_PlayerfalseawModel() will sort out the nodraw cases
	C_Portal_Player *pSplitscreenPlayer = static_cast< C_Portal_Player* >( GetSplitScreenViewPlayer( nSlot ) );
	if ( pSplitscreenPlayer == this )
		return false;

	return BaseClass::ShouldSuppressForSplitScreenPlayer( nSlot );
}

//-----------------------------------------------------------------------------
// Computes the render mode for this player
//-----------------------------------------------------------------------------
PlayerRenderMode_t C_Portal_Player::GetPlayerRenderMode( int nSlot )
{
	// check if local player chases owner of this weapon in first person
	C_Portal_Player *pSplitscreenPlayer = static_cast< C_Portal_Player* >( GetSplitScreenViewPlayer( nSlot ) );
	if ( !pSplitscreenPlayer )
		return PLAYER_RENDER_THIRDPERSON;

	if ( !pSplitscreenPlayer->IsLocalPlayer() && 
		 ( pSplitscreenPlayer->m_Shared.InCond( PORTAL_COND_TAUNTING ) ||
		   pSplitscreenPlayer->m_Shared.InCond( PORTAL_COND_DROWNING ) ||
		   pSplitscreenPlayer->m_Shared.InCond( PORTAL_COND_DEATH_CRUSH ) ||
		   pSplitscreenPlayer->m_Shared.InCond( PORTAL_COND_DEATH_GIB ) ) )
		return PLAYER_RENDER_THIRDPERSON;

	return BaseClass::GetPlayerRenderMode( nSlot );
}


void C_Portal_Player::GetRenderBoundsWorldspace( Vector& absMins, Vector& absMaxs )
{
	Vector mins, maxs;
	GetRenderBounds( mins, maxs );

	const Vector& origin = GetRenderOrigin();
	VectorAdd( mins, origin, absMins );
	VectorAdd( maxs, origin, absMaxs );
}


const QAngle& C_Portal_Player::EyeAngles()
{
	static QAngle eyeAngles;

	if ( IsLocalPlayer( this ) && g_nKillCamMode == OBS_MODE_NONE )
	{
		eyeAngles = BaseClass::EyeAngles();
	}
	else
	{
		//C_BaseEntity *pEntity1 = g_eKillTarget1.Get();
		//C_BaseEntity *pEntity2 = g_eKillTarget2.Get();

		//Vector vLook = Vector( 0.0f, 0.0f, 0.0f );

		//if ( pEntity2 )
		//{
		//	vLook = pEntity1->GetAbsOrigin() - pEntity2->GetAbsOrigin();
		//	VectorNormalize( vLook );
		//}
		//else if ( pEntity1 )
		//{
		//	return BaseClass::EyeAngles();
		//	//vLook =  - pEntity1->GetAbsOrigin();
		//}

		//if ( vLook != Vector( 0.0f, 0.0f, 0.0f ) )
		//{
		//	VectorAngles( vLook, m_angEyeAngles );
		//}

		eyeAngles = m_angEyeAngles;
	}

	Vector vForward;
	AngleVectors( eyeAngles, &vForward );

	// Convert angles to quaternion
	Quaternion qPunch, qEyes;
	AngleQuaternion( m_PortalLocal.m_qQuaternionPunch, qPunch );
	AngleQuaternion( eyeAngles, qEyes );
	// Multiply quaternions to punch ourself in the face
	QuaternionMult( qPunch, qEyes, qEyes );
	// Convert back into angles
	QuaternionAngles( qEyes, eyeAngles );

	return eyeAngles;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : IRagdoll*
//-----------------------------------------------------------------------------
IRagdoll* C_Portal_Player::GetRepresentativeRagdoll() const
{
	if ( m_hRagdoll.Get() )
	{
		C_PortalRagdoll *pRagdoll = static_cast<C_PortalRagdoll*>( m_hRagdoll.Get() );
		if ( !pRagdoll )
			return NULL;

		return pRagdoll->GetIRagdoll();
	}
	else
	{
		return NULL;
	}
}


void C_Portal_Player::PlayerPortalled( C_Portal_Base2D *pEnteredPortal, float fTime, bool bForcedDuck )
{
	//Warning( "C_Portal_Player::PlayerPortalled( %s ) ent:%i slot:%i\n", IsLocalPlayer() ? "local" : "nonlocal", entindex(), engine->GetActiveSplitScreenPlayerSlot() );
#if ( PLAYERPORTALDEBUGSPEW == 1 )
	Warning( "C_Portal_Player::PlayerPortalled( %f %f %f %i ) %i\n", fTime, engine->GetLastTimeStamp(), GetTimeBase(), prediction->GetLastAcknowledgedCommandNumber(), m_PredictedPortalTeleportations.Count() );
#endif
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

	/*{
		QAngle qEngineView;
		engine->GetViewAngles( qEngineView );
		Warning( "Client player portalled %f   %f %f %f\n\t%f %f %f   %f %f %f\n", gpGlobals->curtime, XYZ( GetNetworkOrigin() ), XYZ( pl.v_angle ), XYZ( qEngineView ) ); 
	}*/

	if ( pEnteredPortal )
	{
		C_Portal_Base2D *pRemotePortal = pEnteredPortal->m_hLinkedPortal;

		m_bPortalledMessagePending = true;
		m_PendingPortalMatrix = pEnteredPortal->MatrixThisToLinked();

		if( IsLocalPlayer( this ) && pRemotePortal )
		{
			g_pPortalRender->EnteredPortal( GetSplitScreenPlayerSlot( ), pEnteredPortal );
		}

		if( !GetPredictable() )
		{
			//non-predicted case
			ApplyUnpredictedPortalTeleportation( pEnteredPortal, fTime, bForcedDuck );
		}
		else
		{
			if( m_PredictedPortalTeleportations.Count() == 0 )
			{
				//surprise teleportation
#if ( PLAYERPORTALDEBUGSPEW == 1 )
				Warning( "C_Portal_Player::PlayerPortalled()  No predicted teleportations %f %f\n", gpGlobals->curtime, fTime );
#endif
				ApplyUnpredictedPortalTeleportation( pEnteredPortal, fTime, bForcedDuck );

			}
			else
			{				
				PredictedPortalTeleportation_t shouldBeThisTeleport = m_PredictedPortalTeleportations.Head();

				if( pEnteredPortal != shouldBeThisTeleport.pEnteredPortal )
				{
					AssertMsg( false, "predicted teleportation through the wrong portal." ); //we don't have any test cases for this happening. So the logic is accordingly untested.
					Warning( "C_Portal_Player::PlayerPortalled()  Mismatched head teleportation %f, %f %f\n", gpGlobals->curtime, shouldBeThisTeleport.flTime, fTime );
					UnrollPredictedTeleportations( shouldBeThisTeleport.iCommandNumber );
					ApplyUnpredictedPortalTeleportation( pEnteredPortal, fTime, bForcedDuck );
				}
				else
				{
#if ( PLAYERPORTALDEBUGSPEW == 1 )
					Warning( "C_Portal_Player::PlayerPortalled()  Existing teleportation at %f correct, %f %f\n", m_PredictedPortalTeleportations[0].flTime, gpGlobals->curtime, fTime );
#endif
					m_PredictedPortalTeleportations.Remove( 0 );
				}
			}
		}

		if( pRemotePortal != NULL )
		{
			m_matLatestServerTeleportationInverseMatrix = pRemotePortal->MatrixThisToLinked();
		}
		else
		{
			m_matLatestServerTeleportationInverseMatrix.Identity();
		}
	}

	m_fLatestServerTeleport = fTime;
}

void C_Portal_Player::CheckPlayerAboutToTouchPortal( void )
{
	// don't run this code unless we are in MP and are using the robots
	if ( !GameRules()->IsMultiplayer() )
		return;
		
	int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	if( iPortalCount == 0 || m_bFlingTrailPrePortalled )
		return;

	float fFlingTrail = GetAbsVelocity().Length() - MIN_FLING_SPEED;
	// if we aren't going at least fling speed, don't both with the code below
	if ( fFlingTrail <= 0 )
		return;

	Vector vecVelocity = GetAbsVelocity();

	Vector vMin, vMax;
	CollisionProp()->WorldSpaceAABB( &vMin, &vMax );
	Vector ptCenter = ( vMin + vMax ) * 0.5f;
	Vector vExtents = ( vMax - vMin ) * 0.5f;
	// bloat the player's bounding box check based on the speed and direction that he's travelling
	float flScaler = mp_bot_fling_trail_kill_scaler.GetFloat();
	for ( int i = 0; i < 3; ++i )
	{
		if ( vecVelocity[i] >= 0 )
			vExtents[i] += vecVelocity[i] * flScaler;
		else
			vExtents[i] -= vecVelocity[i] * flScaler;
	}

	CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
	for( int i = 0; i != iPortalCount; ++i )
	{
		CPortal_Base2D *pTempPortal = pPortals[i];
		if( pTempPortal->IsActive() && 
			(pTempPortal->m_hLinkedPortal.Get() != NULL) &&
			UTIL_IsBoxIntersectingPortal( ptCenter, vExtents, pTempPortal )	)
		{
			Vector vecDirToPortal = ptCenter - pTempPortal->GetAbsOrigin();
			VectorNormalize(vecDirToPortal);
			Vector vecDirMotion = vecVelocity;
			VectorNormalize(vecDirMotion);
			float dot = DotProduct( vecDirToPortal, vecDirMotion );

			// If the portal is behind our direction of movement, then we probably just came out of it
			// IGNORE
			if ( dot > 0.0f )
				continue;
			
			// if we're flinging and we touched a portal
			if ( m_FlingTrailEffect && !m_bFlingTrailPrePortalled && !m_bFlingTrailJustPortalled )
			{
				// stop the effect linger effect if it exists
				m_FlingTrailEffect->SetOwner( NULL );
				ParticleProp()->StopEmission( m_FlingTrailEffect, false, true, false );
				m_FlingTrailEffect = NULL;
				m_bFlingTrailActive = false;
				m_bFlingTrailPrePortalled = true;
				return;
			}
		}
	}
}

void C_Portal_Player::OnPreDataChanged( DataUpdateType_t type )
{
	BaseClass::OnPreDataChanged( type );

	m_iOldSpawnCounter = m_iSpawnCounter;
	m_Shared.OnPreDataChanged();
	m_hPreDataChangedAttachedObject = m_hAttachedObject;

	m_bWasAlivePreUpdate = IsAlive();
}

void C_Portal_Player::PreDataUpdate( DataUpdateType_t updateType )
{
	PreDataChanged_Backup.m_hPortalEnvironment = m_hPortalEnvironment;
	PreDataChanged_Backup.m_qEyeAngles = m_iv_angEyeAngles.GetCurrent();
	//PreDataChanged_Backup.m_ptPlayerPosition = GetNetworkOrigin();
	PreDataChanged_Backup.m_iEntityPortalledNetworkMessageCount = m_iEntityPortalledNetworkMessageCount;

	BaseClass::PreDataUpdate( updateType );
}

void C_Portal_Player::FixPortalEnvironmentOwnership( void )
{
	CPortalSimulator *pExistingSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( this );
	C_Portal_Base2D *pPortalEnvironment = m_hPortalEnvironment;
	CPortalSimulator *pNewSimulator = pPortalEnvironment ? &pPortalEnvironment->m_PortalSimulator : NULL;
	if( pExistingSimulator != pNewSimulator )
	{
		if( pExistingSimulator )
		{
			pExistingSimulator->ReleaseOwnershipOfEntity( this );
		}

		if( pNewSimulator )
		{
			pNewSimulator->TakeOwnershipOfEntity( this );
		}
	}
}

#if ( PLAYERPORTALDEBUGSPEW == 1 )
ConVar cl_spewplayerpackets( "cl_spewplayerpackets", "0" );
#endif

void C_Portal_Player::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	FixPortalEnvironmentOwnership();

	bool bRespawn = ( m_iOldSpawnCounter != m_iSpawnCounter );

	if ( type == DATA_UPDATE_CREATED )
	{
		if ( IsLocalPlayer() )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );
			g_ThirdPersonManager.Init();
			bRespawn = true;

			CPortalMPGameRules *pRules = PortalMPGameRules();
			if ( pRules )
			{
				pRules->LoadMapCompleteData();
			}
		}
	}

	if ( bRespawn )
	{
		ClientPlayerRespawn();
	}

	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( m_bWasAlivePreUpdate && !IsAlive() )
		{
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
			RemoveClientsideWearables();
#endif
		}

		if ( !m_bGibbed && ( m_Shared.InCond( PORTAL_COND_DEATH_GIB ) || m_Shared.InCond( PORTAL_COND_DEATH_CRUSH ) ) /*&& mp_should_gib_bots.GetBool()*/ )
		{
			m_bGibbed = true;
			CUtlReference< CNewParticleEffect > pEffect;
			Vector vecOffSet = WorldSpaceCenter() - GetAbsOrigin();
			pEffect = this->ParticleProp()->Create( "bot_death_B_gib", PATTACH_POINT_FOLLOW, "damage_mainbody" );
			if ( pEffect )
			{
				pEffect->SetControlPointEntity( 0, this );
				pEffect->SetControlPoint( 1, WorldSpaceCenter() );
			}
		}
	}

	m_Shared.OnDataChanged();

	if ( m_hAttachedObject.Get() != m_hPreDataChangedAttachedObject.Get() && m_hPreDataChangedAttachedObject.Get() != NULL )
	{
		// We just lost our held object 
		C_BaseAnimating *pAnim = m_hPreDataChangedAttachedObject.Get()->GetBaseAnimating();
		Assert ( pAnim );
		if ( pAnim )
		{
			// Restore render origin to normal in case we modified it
			pAnim->DisableRenderOriginOverride();
		}
	}

	// Set held objects to draw in the view model
	if ( IsUsingVMGrab() && m_hAttachedObject.Get() && m_hOldAttachedObject == NULL )
	{
		m_hOldAttachedObject = m_hAttachedObject;
		m_hAttachedObject.Get()->UpdateVisibility();

		if ( GameRules()->IsMultiplayer() == false )
		{
			m_hAttachedObject.Get()->RenderWithViewModels( true );
		}
	}
	else if ( ( !IsUsingVMGrab() || m_hAttachedObject.Get() == NULL ) && m_hOldAttachedObject.Get() )
	{
		if ( GameRules()->IsMultiplayer() == false )
		{
			m_hOldAttachedObject.Get()->RenderWithViewModels( false );
		}
		m_hOldAttachedObject.Get()->UpdateVisibility();
		m_hOldAttachedObject = NULL;
		m_flAutoGrabLockOutTime = gpGlobals->curtime;
	}

#if ( PLAYERPORTALDEBUGSPEW == 1 )
	if( entindex() == 1 && cl_spewplayerpackets.GetBool() )
	{
		Msg( "C_Portal_Player::OnDataChanged( %f %f %f %i )\n", GetTimeBase(), gpGlobals->curtime, engine->GetLastTimeStamp(), prediction->GetLastAcknowledgedCommandNumber() );
	}
#endif


	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );
	if( GetPredictable() && (m_PredictedPortalTeleportations.Count() != 0) && (m_PredictedPortalTeleportations[0].fDeleteServerTimeStamp != -1.0f) )
	{
		//just because the server processed the message does not mean it also sent the teleportation temp ent
		//Give that temporary entity some slack time to show up.
		//We really should encode portal teleportation right into CBaseEntity to further clamp down the processing flow
		if( (engine->GetLastTimeStamp() - m_PredictedPortalTeleportations[0].fDeleteServerTimeStamp) > (TICK_INTERVAL * 10) ) //give the server an extra 10 ticks to send out the teleportation message 
		{
			//The server has acknowledged that it processed the command that we predicted this happened on. But we didn't get a teleportation notification. It must not have happened on the server
#if ( PLAYERPORTALDEBUGSPEW == 1 )
			Warning( "======================OnDataChanged removing a teleportation that didn't happen!!!! %f %i -=- %f %f %i======================\n", m_PredictedPortalTeleportations[0].flTime, m_PredictedPortalTeleportations[0].iCommandNumber, GetTimeBase(), engine->GetLastTimeStamp(), prediction->GetLastAcknowledgedCommandNumber() );
#endif
			UnrollPredictedTeleportations( m_PredictedPortalTeleportations[0].iCommandNumber );
		}
	}
}

void C_Portal_Player::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		
		if( g_pGameRules->IsMultiplayer() && !IsLocalPlayer() )
		{
			AddRemoteSplitScreenViewPlayer( this );
		}
	}
	else
	{
		if( m_iEntityPortalledNetworkMessageCount != PreDataChanged_Backup.m_iEntityPortalledNetworkMessageCount )
		{
			Assert( IsLocalPlayer() ); //this data should never have been sent down the wire

			if( IsLocalPlayer() && !IsSplitScreenPlayer() ) //this buffer is stored in each player entity and sent only to the owner player, therefore we will receive 2 copies in splitscreen. Discard second player's copy	
			{				
				uint32 iStopIndex = m_iEntityPortalledNetworkMessageCount%MAX_ENTITY_PORTALLED_NETWORK_MESSAGES;
				Assert( m_EntityPortalledNetworkMessages[(m_iEntityPortalledNetworkMessageCount - 1)%MAX_ENTITY_PORTALLED_NETWORK_MESSAGES].m_iMessageCount == (m_iEntityPortalledNetworkMessageCount - 1) );
				bool bOverFlowed = m_EntityPortalledNetworkMessages[PreDataChanged_Backup.m_iEntityPortalledNetworkMessageCount%MAX_ENTITY_PORTALLED_NETWORK_MESSAGES].m_iMessageCount != PreDataChanged_Backup.m_iEntityPortalledNetworkMessageCount;
				AssertMsg( !bOverFlowed, "Entity teleportation message overflow, increase CPortal_Player::MAX_ENTITY_PORTALLED_NETWORK_MESSAGES" );

				uint32 iIterator = (bOverFlowed ? m_iEntityPortalledNetworkMessageCount : //if overflowed, start from oldest entry in the buffer
													PreDataChanged_Backup.m_iEntityPortalledNetworkMessageCount) //else, start from the first new entry
													% MAX_ENTITY_PORTALLED_NETWORK_MESSAGES;
				
				do
				{
					C_EntityPortalledNetworkMessage &readFrom = m_EntityPortalledNetworkMessages[iIterator];
					RecieveEntityPortalledMessage( readFrom.m_hEntity, readFrom.m_hPortal, readFrom.m_fTime, readFrom.m_bForcedDuck );
					iIterator = (iIterator + 1) % MAX_ENTITY_PORTALLED_NETWORK_MESSAGES;
				} while( iIterator != iStopIndex );
			}
		}
	}

	if ( m_nOldTeamTauntState != m_nTeamTauntState )
	{
		if ( ( ( m_nOldTeamTauntState == TEAM_TAUNT_NONE || m_nOldTeamTauntState == TEAM_TAUNT_NEED_PARTNER ) && 
			 ( m_nTeamTauntState == TEAM_TAUNT_HAS_PARTNER || m_nTeamTauntState == TEAM_TAUNT_SUCCESS ) ) || 
			 m_nOldTeamTauntState == TEAM_TAUNT_SUCCESS )
		{
			m_fTeamTauntStartTime = gpGlobals->curtime;
		}

		m_nOldTeamTauntState = m_nTeamTauntState;
	}
	
	SetNetworkAngles( GetLocalAngles() );

	if ( m_iSpawnInterpCounter != m_iSpawnInterpCounterCache )
	{
		MoveToLastReceivedPosition( true );
		ResetLatched();
		m_iSpawnInterpCounterCache = m_iSpawnInterpCounter;
	}

#if ( PLAYERPORTALDEBUGSPEW == 1 )
	if( entindex() == 1 && cl_spewplayerpackets.GetBool() )
	{
		Msg( "C_Portal_Player::PostDataUpdate( %f %f %f %i )\n", GetTimeBase(), gpGlobals->curtime, engine->GetLastTimeStamp(), prediction->GetLastAcknowledgedCommandNumber() );
	}
#endif

	if( GetPredictable() && (m_PredictedPortalTeleportations.Count() != 0) && (m_PredictedPortalTeleportations[0].iCommandNumber < prediction->GetLastAcknowledgedCommandNumber()) )
	{
		int iAcknowledgedCommand = prediction->GetLastAcknowledgedCommandNumber();
		
		for( int i = 0; i != m_PredictedPortalTeleportations.Count(); ++i )
		{
			//we only mark instead of remove because the EntityPortalled message could still be in the stream, it'll have been processed by the time we get to OnDataChanged()
			if( m_PredictedPortalTeleportations[i].iCommandNumber < iAcknowledgedCommand )
			{
				if( m_PredictedPortalTeleportations[i].fDeleteServerTimeStamp == -1.0f )
				{
					m_PredictedPortalTeleportations[i].fDeleteServerTimeStamp = engine->GetLastTimeStamp(); //this is the engine update where we should also receive the teleportation message
				}
			}
			else
			{
				break;
			}
		}
	}

	UpdateVisibility();

	FixPortalEnvironmentOwnership();
}

float C_Portal_Player::GetFOV( void )
{
	//Find our FOV with offset zoom value
	float flFOVOffset = C_BasePlayer::GetFOV();

	// Clamp FOV in MP
	int min_fov = GetMinFOV();

	// Don't let it go too low
	flFOVOffset = MAX( min_fov, flFOVOffset );

	return flFOVOffset;
}

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector C_Portal_Player::GetAutoaimVector( float flDelta )
{
	// Never autoaim a predicted weapon (for now)
	Vector	forward;
	AngleVectors( EyeAngles() + m_Local.m_vecPunchAngle, &forward );
	return	forward;
}

void C_Portal_Player::ItemPreFrame( void )
{
	BaseClass::ItemPreFrame();
	ManageHeldObject();
}

// Only runs in MP. Creates a fake held object and uses a clientside grab controller
// to move it so the held object looks responsive under lag conditions.
void C_Portal_Player::ManageHeldObject()
{
	Assert ( GameRules()->IsMultiplayer() );


	CBaseEntity *pPlayerAttached = m_hAttachedObject.Get();

	//cleanup invalid clones.
	{
		if( (m_pHeldEntityClone != NULL) && 
			((pPlayerAttached == NULL) || !IsUsingVMGrab() || (m_pHeldEntityClone->m_hOriginal != pPlayerAttached)) )
		{
			//cloning wrong entity or don't want a clone
			if( GetGrabController().GetAttached() == m_pHeldEntityClone )
			{
				bool bOldForce = m_bForcingDrop;
				m_bForcingDrop = true;
				GetGrabController().DetachEntity( false );
				m_bForcingDrop = bOldForce;
			}
			UTIL_Remove( m_pHeldEntityClone );
			m_pHeldEntityClone = NULL;
		}

		if( (m_pHeldEntityThirdpersonClone != NULL) && 
			((pPlayerAttached == NULL) || !IsUsingVMGrab() || (m_pHeldEntityThirdpersonClone->m_hOriginal != pPlayerAttached)) )
		{
			//cloning wrong entity or don't want a clone
			if( GetGrabController().GetAttached() == m_pHeldEntityThirdpersonClone )
			{
				bool bOldForce = m_bForcingDrop;
				m_bForcingDrop = true;
				GetGrabController().DetachEntity( false );
				m_bForcingDrop = bOldForce;
			}
			UTIL_Remove( m_pHeldEntityThirdpersonClone );
			m_pHeldEntityThirdpersonClone = NULL;
		}
	}

	//create clones if necessary
	if( pPlayerAttached && IsUsingVMGrab() )
	{
		if( m_pHeldEntityClone == NULL )
		{
			m_pHeldEntityClone = new C_PlayerHeldObjectClone;
			if ( m_pHeldEntityClone )
			{
				if( !m_pHeldEntityClone->InitClone( pPlayerAttached, this ) )
				{
					UTIL_Remove( m_pHeldEntityClone );
					m_pHeldEntityClone = NULL;
				}
			}
		}

		if( m_pHeldEntityThirdpersonClone == NULL )
		{
			m_pHeldEntityThirdpersonClone = new C_PlayerHeldObjectClone;
			if ( m_pHeldEntityThirdpersonClone )
			{
				if( !m_pHeldEntityThirdpersonClone->InitClone( pPlayerAttached, this, false, m_pHeldEntityClone ) )
				{
					UTIL_Remove( m_pHeldEntityThirdpersonClone );
					m_pHeldEntityThirdpersonClone = NULL;
				}
			}
		}
	}


	//ensure correct entity is attached
	{
		C_BaseEntity *pShouldBeAttached = NULL;

		//figure out exactly what should be attached
		if( pPlayerAttached )
		{
			pPlayerAttached->SetPredictionEligible( true ); //open the floodgates of possible predictability. 
															//Not sure if there's a completely sane way to invert this operation for all possible cross sections of predictables and things you can pick up, so I simply wont.

			if( IsUsingVMGrab() && m_pHeldEntityClone )
			{
				pShouldBeAttached = m_pHeldEntityClone;
			}
			else
			{
				pShouldBeAttached = pPlayerAttached;
			}
		}

		//swap it in if necessary
		if( GetGrabController().GetAttached() != pShouldBeAttached )
		{
			//clear out offset history
			m_iv_vecCarriedObject_CurPosToTargetPos_Interpolator.ClearHistory();
			m_iv_vecCarriedObject_CurAngToTargetAng_Interpolator.ClearHistory();

			//release whatever the grab controller is attached to
			if( GetGrabController().GetAttached() )
			{
				if( (GetGrabController().GetAttached() != pPlayerAttached) && (GetGrabController().GetAttached() != m_pHeldEntityClone) )
				{
					GetGrabController().DetachUnknownEntity(); //the entity it has is not something we gave to it (or our player attached object invalidated under our feet)
				}
				else
				{
					bool bOldForce = m_bForcingDrop;
					m_bForcingDrop = true;
					GetGrabController().DetachEntity( false ); //we know the entity it's holding on to is valid, detach normally
					m_bForcingDrop = bOldForce;
				}
			}

			//if something should be attached, attach it now
			if( pShouldBeAttached )
			{
				GetGrabController().SetIgnorePitch( true );
				GetGrabController().SetAngleAlignment( 0.866025403784 );
				GetGrabController().AttachEntity( this, pShouldBeAttached, pShouldBeAttached->VPhysicsGetObject(), false, vec3_origin, false );
			}
		}
	}

	//only adding these on first-predicted frames to keep old results consistent, and ease into new changes
	if( pPlayerAttached && (!prediction->InPrediction() || prediction->IsFirstTimePredicted()) )
	{
		m_iv_vecCarriedObject_CurPosToTargetPos_Interpolator.AddToHead( gpGlobals->curtime, &m_vecCarriedObject_CurPosToTargetPos, true );
		m_iv_vecCarriedObject_CurAngToTargetAng_Interpolator.AddToHead( gpGlobals->curtime, &m_vecCarriedObject_CurAngToTargetAng, true );
		
		//need to interpolate these so they clear out old data. No direct access to clearing functions
		m_iv_vecCarriedObject_CurPosToTargetPos_Interpolator.Interpolate( gpGlobals->curtime );
		m_iv_vecCarriedObject_CurAngToTargetAng_Interpolator.Interpolate( gpGlobals->curtime );
	}

	//update the grab controller
	if ( GetGrabController().GetAttached() )
	{
		m_iv_vecCarriedObject_CurPosToTargetPos_Interpolator.Interpolate( gpGlobals->curtime );
		m_iv_vecCarriedObject_CurAngToTargetAng_Interpolator.Interpolate( gpGlobals->curtime );
		GetGrabController().m_attachedAnglesPlayerSpace = m_vecCarriedObjectAngles;
		GetGrabController().UpdateObject( this, 12 );
		GetGrabController().ClientApproachTarget( this );
	}
}



bool C_Portal_Player::IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps )
{
	if ( pEntity )
	{
		// Use the union of caps specified by client ents and those networked down from the server
		int caps = pEntity->ObjectCaps() | pEntity->GetServerObjectCaps();
		if ( caps & (FCAP_IMPULSE_USE|FCAP_CONTINUOUS_USE|FCAP_ONOFF_USE|FCAP_DIRECTIONAL_USE) )
		{
			if ( (caps & requiredCaps) == requiredCaps )
			{
				return true;
			}
		}
	}

	return false;
}

void C_Portal_Player::CreatePingPointer( Vector vecDestintaion )
{
	DestroyPingPointer();

	if ( !m_PointLaser || !m_PointLaser.IsValid() )
	{
		if ( !GetViewModel() )
		{
			Warning( "Trying to create a ping laser, but we have no view model!" );
			return;
		}
			
		MDLCACHE_CRITICAL_SECTION();
		C_BaseAnimating::PushAllowBoneAccess( false, true, "pingpointer" );	
		int iAttachment = GetViewModel()->LookupAttachment( "muzzle" );
		// we don't have the portalgun
		if ( iAttachment == -1 /*!GetActiveWeapon()*/ )
			m_PointLaser = ParticleProp()->Create( "robot_point_beam", PATTACH_EYES_FOLLOW, -1, Vector( 0, 0, -18 ) );
		else
			m_PointLaser = GetViewModel()->ParticleProp()->Create( "robot_point_beam", PATTACH_POINT_FOLLOW, "muzzle" );

		if ( m_PointLaser )
		{
			m_PointLaser->SetDrawOnlyForSplitScreenUser( GetSplitScreenPlayerSlot() );
			m_PointLaser->SetControlPoint( 1, vecDestintaion );
			int nTeam = GetTeamNumber();
			Color color( 255, 255, 255 );
			if ( nTeam == TEAM_RED )
				color = UTIL_Portal_Color( 2, 0 );  //orange
			else
				color = UTIL_Portal_Color( 1, 0 );  //blue

			Vector vColor;
			vColor.x = color.r();
			vColor.y = color.g();
			vColor.z = color.b();
			m_PointLaser->SetControlPoint( 2, vColor );
		}
		C_BaseAnimating::PopBoneAccess( "pingpointer" );
	}
}

CEG_NOINLINE void C_Portal_Player::DestroyPingPointer( void )
{
	if ( m_PointLaser )
	{
		CEG_PROTECT_MEMBER_FUNCTION( C_Portal_Player_DestroyPingPointer )

		// stop the effect
		m_PointLaser->StopEmission( false, true, false );
		m_PointLaser = NULL;

	}
}

C_BaseAnimating *C_Portal_Player::BecomeRagdollOnClient()
{
	// Let the C_CSRagdoll entity do this.
	// m_builtRagdoll = true;
	return NULL;
}

void C_Portal_Player::UpdatePortalEyeInterpolation( void )
{
#ifdef ENABLE_PORTAL_EYE_INTERPOLATION_CODE
	//PortalEyeInterpolation.m_bEyePositionIsInterpolating = false;
	if( PortalEyeInterpolation.m_bUpdatePosition_FreeMove )
	{
		PortalEyeInterpolation.m_bUpdatePosition_FreeMove = false;

		C_Portal_Base2D *pOldPortal = PreDataChanged_Backup.m_hPortalEnvironment.Get();
		if( pOldPortal )
		{
			UTIL_Portal_PointTransform( pOldPortal->MatrixThisToLinked(), PortalEyeInterpolation.m_vEyePosition_Interpolated, PortalEyeInterpolation.m_vEyePosition_Interpolated );
			//PortalEyeInterpolation.m_vEyePosition_Interpolated = pOldPortal->m_matrixThisToLinked * PortalEyeInterpolation.m_vEyePosition_Interpolated;

			//Vector vForward;
			//m_hPortalEnvironment.Get()->GetVectors( &vForward, NULL, NULL );

			PortalEyeInterpolation.m_vEyePosition_Interpolated = EyeFootPosition();

			PortalEyeInterpolation.m_bEyePositionIsInterpolating = true;
		}
	}

	if( IsInAVehicle() )
		PortalEyeInterpolation.m_bEyePositionIsInterpolating = false;

	if( !PortalEyeInterpolation.m_bEyePositionIsInterpolating )
	{
		PortalEyeInterpolation.m_vEyePosition_Uninterpolated = EyeFootPosition();
		PortalEyeInterpolation.m_vEyePosition_Interpolated = PortalEyeInterpolation.m_vEyePosition_Uninterpolated;
		return;
	}

	Vector vThisFrameUninterpolatedPosition = EyeFootPosition();

	//find offset between this and last frame's uninterpolated movement, and apply this as freebie movement to the interpolated position
	PortalEyeInterpolation.m_vEyePosition_Interpolated += (vThisFrameUninterpolatedPosition - PortalEyeInterpolation.m_vEyePosition_Uninterpolated);
	PortalEyeInterpolation.m_vEyePosition_Uninterpolated = vThisFrameUninterpolatedPosition;

	Vector vDiff = vThisFrameUninterpolatedPosition - PortalEyeInterpolation.m_vEyePosition_Interpolated;
	
	float fLength = vDiff.Length();
	float fFollowSpeed = gpGlobals->frametime * 100.0f;
	const float fMaxDiff = 150.0f;
	if( fLength > fMaxDiff )
	{
		//camera lagging too far behind, give it a speed boost to bring it within maximum range
		fFollowSpeed = fLength - fMaxDiff;
	}
	else if( fLength < fFollowSpeed )
	{
		//final move
		PortalEyeInterpolation.m_bEyePositionIsInterpolating = false;
		PortalEyeInterpolation.m_vEyePosition_Interpolated = vThisFrameUninterpolatedPosition;
		return;
	}

	if ( fLength > 0.001f )
	{
		vDiff *= (fFollowSpeed/fLength);
		PortalEyeInterpolation.m_vEyePosition_Interpolated += vDiff;
	}
	else
	{
		PortalEyeInterpolation.m_vEyePosition_Interpolated = vThisFrameUninterpolatedPosition;
	}



#else
	PortalEyeInterpolation.m_vEyePosition_Interpolated = BaseClass::EyePosition();
#endif
}


Vector C_Portal_Player::EyeFootPosition( const QAngle &qEyeAngles )
{
	return EyePosition() - m_vecViewOffset.Length() * m_PortalLocal.m_Up;
}


void C_Portal_Player::CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov )
{
	HandleTaunting();

	m_pNoDrawForRecursionLevelOne = NULL;
	m_bEyePositionIsTransformedByPortal = false; //assume it's not transformed until it provably is
	UpdatePortalEyeInterpolation();

	QAngle qEyeAngleBackup = EyeAngles();
	Vector ptEyePositionBackup = EyePosition();

	if ( Util_PIP_ShouldDrawPlayer( this ) )
	{
		eyeAngles = EyeAngles();
		eyeAngles[ PITCH ] = -m_vecRemoteViewAngles.x;
		eyeAngles[ YAW ] += m_vecRemoteViewAngles.y - 180.0f;
		Vector vForward;
		AngleVectors( eyeAngles, &vForward );
		
		Vector ptTargetPosition = EyePosition();
		
		CTraceFilterSkipTwoEntities filter( NULL, NULL );
		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			C_Portal_Player *pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

			// Exclude the taunting player and the other player if in team taunt
			if( pPlayer == NULL )
				continue;
			else if( pPlayer == this )
				filter.SetPassEntity( this );
			else if ( GetTeamTauntState() >= TEAM_TAUNT_HAS_PARTNER )
				filter.SetPassEntity2( pPlayer );
		}

		float flBestFraction = 0.0f;
		float flBestYaw = eyeAngles[ YAW ];

		int nTestNum = 0;
		const int nMaxTests = 15;
		const float fJitter = 180.0f / static_cast<float>( nMaxTests );

		bool bStartSolid = false;
		float fSolidStartDist = MIN( 16.0f, m_fTauntCameraDistance );

		while ( flBestFraction < 0.75f && nTestNum < nMaxTests )
		{
			// Test and modify goals if camera doesn't fit
			float fCurrentYaw = eyeAngles[ YAW ] + nTestNum * fJitter * ( ( nTestNum % 2 == 0 ) ? 1.0f : -1.0f );
			float TauntCamTargetYaw = GetAbsAngles()[ YAW ] + fCurrentYaw;

			Vector vTestForward;
			AngleVectors( QAngle( eyeAngles[ PITCH ], TauntCamTargetYaw, 0 ), &vTestForward, NULL, NULL );

			trace_t trace;

			if ( !bStartSolid )
			{
				// Start from the center
				UTIL_TraceHull( ptTargetPosition, ptTargetPosition + ( vTestForward * m_fTauntCameraDistance ), Vector( -9.f, -9.f, -9.f ), Vector( 9.f, 9.f, 9.f ), MASK_SOLID, &filter, &trace );

				if ( trace.startsolid )
				{
					bStartSolid = true;
				}
			}

			if ( bStartSolid )
			{
				// Start away from the center
				UTIL_TraceHull( ptTargetPosition + ( vTestForward * fSolidStartDist ), ptTargetPosition + ( vTestForward * m_fTauntCameraDistance ), Vector( -9.0f, -9.0f, -9.0f ), Vector( 9.0f, 9.0f, 9.0f ), MASK_SOLID, &filter, &trace );
			}

			if ( flBestFraction < trace.fraction && !trace.startsolid )
			{
				flBestFraction = trace.fraction;
				flBestYaw = fCurrentYaw;
			}

			nTestNum++;
		}

		// setup eye position and angle
		eyeAngles[ YAW ] = flBestYaw - 180.0f;
		AngleVectors( eyeAngles, &vForward );
		eyeOrigin = ptTargetPosition - vForward * m_fTauntCameraDistance * flBestFraction;
		fov = GetFOV();

		return;
	}

	if ( m_lifeState != LIFE_ALIVE )
	{
		if ( g_nKillCamMode != 0 )
		{
			return;
		}

		Vector origin = EyePosition();

		C_BaseEntity* pRagdoll = m_hRagdoll.Get();

		if ( pRagdoll )
		{
			origin = pRagdoll->GetAbsOrigin();
#if !defined PORTAL_HIDE_PLAYER_RAGDOLL
			origin.z += VEC_DEAD_VIEWHEIGHT.z; // look over ragdoll, not through
#endif //!PORTAL_HIDE_PLAYER_RAGDOLL
		}

		if ( !GameRules()->IsMultiplayer() )
			BaseClass::CalcView( eyeOrigin, eyeAngles, zNear, zFar, fov );

		eyeOrigin = origin;

		Vector vForward; 
		AngleVectors( eyeAngles, &vForward );

		VectorNormalize( vForward );
#if !defined PORTAL_HIDE_PLAYER_RAGDOLL
		VectorMA( origin, -CHASE_CAM_DISTANCE, vForward, eyeOrigin );
#endif //!PORTAL_HIDE_PLAYER_RAGDOLL

		Vector WALL_MIN( -WALL_OFFSET, -WALL_OFFSET, -WALL_OFFSET );
		Vector WALL_MAX( WALL_OFFSET, WALL_OFFSET, WALL_OFFSET );

		trace_t trace; // clip against world
		C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
		UTIL_TraceHull( origin, eyeOrigin, WALL_MIN, WALL_MAX, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trace );
		C_BaseEntity::PopEnableAbsRecomputations();

		if (trace.fraction < 1.0)
		{
			eyeOrigin = trace.endpos;
		}

		// in multiplayer we want to make sure we get the screenshakes and stuff
		if ( GameRules()->IsMultiplayer() )
			CalcPortalView( eyeOrigin, eyeAngles, fov );

	}
	else
	{
		IClientVehicle *pVehicle; 
		pVehicle = GetVehicle();

		if ( !pVehicle )
		{
			if ( IsObserver() )
			{
				CalcObserverView( eyeOrigin, eyeAngles, fov );
			}
			else
			{
				CalcPortalView( eyeOrigin, eyeAngles, fov );
			}
		}
		else
		{
			CalcVehicleView( pVehicle, eyeOrigin, eyeAngles, zNear, zFar, fov );
		}
	}

	if( !IsLocalPlayer() ) //HACKHACK: This is a quick hammer to fix the roll on nonlocal players
		eyeAngles[ROLL] = 0.0f;
}

void C_Portal_Player::CalcPortalView( Vector &eyeOrigin, QAngle &eyeAngles, float &fov )
{
	float fEffectiveCurTime = GetEffectiveInterpolationCurTime( gpGlobals->curtime );
	
	if ( !prediction->InPrediction() )
	{
		// FIXME: Move into prediction
		view->DriftPitch();
	}

	// TrackIR
	if ( IsHeadTrackingEnabled() )
	{
		VectorCopy( EyePosition() + GetEyeOffset(), eyeOrigin );
		VectorCopy( EyeAngles() + GetEyeAngleOffset(), eyeAngles );
	}
	else
	{
		VectorCopy( EyePosition(), eyeOrigin );
		VectorCopy( EyeAngles(), eyeAngles );
	}

	Vector vRenderOrigin = GetRenderOrigin();

	//if discontinuous eye position gets a transform, so do eye angles
	bool bEyeDiscontinuity = false;
	{
		matrix3x4_t matTemp;
		if( GetOriginInterpolator().GetDiscontinuityTransform( fEffectiveCurTime, matTemp ) )
		{
			eyeAngles = TransformAnglesToWorldSpace( eyeAngles, matTemp );
			bEyeDiscontinuity = true;
		}
	}

	VectorAdd( eyeAngles, m_Local.m_vecPunchAngle, eyeAngles );

	if ( !prediction->InPrediction() )
	{
		GetViewEffects()->CalcShake();
		GetViewEffects()->ApplyShake( eyeOrigin, eyeAngles, 1.0 );
	}

	if( !prediction->InPrediction() )
	{
		SmoothViewOnStairs( eyeOrigin );
	}

	// Apply a smoothing offset to smooth out prediction errors.
	Vector vSmoothOffset;
	GetPredictionErrorSmoothingVector( vSmoothOffset );
	eyeOrigin += vSmoothOffset;

	m_bEyePositionIsTransformedByPortal = false;
	C_Portal_Base2D *pTransformPortal = NULL;
	for( int i = 0; i != CPortal_Base2D_Shared::AllPortals.Count(); ++i )
	{
		C_Portal_Base2D *pPortal = CPortal_Base2D_Shared::AllPortals[i];
		if( !pPortal->IsActivedAndLinked() )
			continue;

		float fEyeDist = pPortal->m_plane_Origin.normal.Dot( eyeOrigin ) - pPortal->m_plane_Origin.dist;
		float fBodyDist = pPortal->m_plane_Origin.normal.Dot( vRenderOrigin ) - pPortal->m_plane_Origin.dist;

		if( (fEyeDist < 0.0f) && //eye behind portal
			(fBodyDist >= 0.0f) ) //body in front of portal
		{
			float fOOTotalDist = 1.0f / (fBodyDist - fEyeDist);
			Vector vIntersect = (eyeOrigin * (fBodyDist * fOOTotalDist)) - (vRenderOrigin * (fEyeDist * fOOTotalDist));
			Vector vCenterToIntersect = vIntersect - pPortal->m_ptOrigin;
			
			if( (fabs(vCenterToIntersect.Dot( pPortal->m_vRight )) > pPortal->GetHalfWidth()) ||
				(fabs(vCenterToIntersect.Dot( pPortal->m_vUp )) > pPortal->GetHalfHeight()) )
				continue;

			pTransformPortal = pPortal;
			break;
		}
	}

	if( !pTransformPortal && m_hPortalEnvironment.Get() != NULL )
	{
		C_Portal_Base2D *pPortal = m_hPortalEnvironment;
		if( pPortal->IsActivedAndLinked() )
		{
			if( GetOriginInterpolator().GetInterpolatedTime( fEffectiveCurTime ) < m_fLatestServerTeleport )
			{
				pPortal = pPortal->m_hLinkedPortal.Get();
			}
			float fEyeDist = pPortal->m_plane_Origin.normal.Dot( eyeOrigin ) - pPortal->m_plane_Origin.dist;
			
			if( fEyeDist < 0.0f )
			{
				pTransformPortal = pPortal;
			}
		}
	}

	if( pTransformPortal )
	{
		m_bEyePositionIsTransformedByPortal = true;
		m_pNoDrawForRecursionLevelOne = pTransformPortal->m_hLinkedPortal.Get();

		DevMsg( 2, "transforming portal view from <%f %f %f> <%f %f %f>\n", eyeOrigin.x, eyeOrigin.y, eyeOrigin.z, eyeAngles.x, eyeAngles.y, eyeAngles.z );

		UTIL_Portal_PointTransform( pTransformPortal->MatrixThisToLinked(), eyeOrigin, eyeOrigin );
		UTIL_Portal_AngleTransform( pTransformPortal->MatrixThisToLinked(), eyeAngles, eyeAngles );

		DevMsg( 2, "transforming portal view to   <%f %f %f> <%f %f %f>\n", eyeOrigin.x, eyeOrigin.y, eyeOrigin.z, eyeAngles.x, eyeAngles.y, eyeAngles.z );		
	}

	m_flObserverChaseDistance = 0.0;

	//if( !engine->IsPaused() && entindex() == 1 )
	//	Warning( "C_Portal_Player::CalcPortalView(%f) %s %s  %f %f %f\n", gpGlobals->curtime, m_bEyePositionIsTransformedByPortal ? "trans" : "normal", bEyeDiscontinuity ? "disc" : "linear", GetOriginInterpolator().GetInterpolatedTime( gpGlobals->curtime ), m_fLatestServerTeleport, GetOriginInterpolator().GetOldestEntry() );

	// calc current FOV
	fov = GetFOV();
}

void C_Portal_Player::GetToolRecordingState( KeyValues *msg )
{
	BaseClass::GetToolRecordingState( msg );

	if( m_bToolMode_EyeHasPortalled_LastRecord != m_bEyePositionIsTransformedByPortal )
	{
		BaseEntityRecordingState_t dummyState;
		BaseEntityRecordingState_t *pState = (BaseEntityRecordingState_t *)msg->GetPtr( "baseentity", &dummyState );
		pState->m_fEffects |= EF_NOINTERP; //If we interpolate, we'll be traversing an arbitrary line through the level at an undefined speed. That would be bad
	}

	m_bToolMode_EyeHasPortalled_LastRecord = m_bEyePositionIsTransformedByPortal;

	//record if the eye is on the opposite side of the portal from the body
	{
		CameraRecordingState_t dummyState;
		CameraRecordingState_t *pState = (CameraRecordingState_t *)msg->GetPtr( "camera", &dummyState );
		pState->m_bPlayerEyeIsPortalled = m_bEyePositionIsTransformedByPortal;
	}
}

void C_Portal_Player::SetAnimation( PLAYER_ANIM playerAnim )
{
	return;
}

void C_Portal_Player::CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles)
{
	C_Portal_Base2D *pTransformedByPortal = (m_bEyePositionIsTransformedByPortal) ? m_pNoDrawForRecursionLevelOne->m_hLinkedPortal.Get() : NULL;

	bool bInvertFacing = GetOriginInterpolator().GetInterpolatedTime( GetEffectiveInterpolationCurTime( gpGlobals->curtime ) ) < m_fLatestServerTeleport;
	
	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = GetViewModel( i );
		if ( !vm )
			continue;

		if( bInvertFacing )
		{
			Vector vTemp;
			VectorRotate( vm->m_vecLastFacing, m_matLatestServerTeleportationInverseMatrix.As3x4(), vTemp );
			vm->m_vecLastFacing = vTemp;
		}

		//if our eyes are behind a portal, then we transform the view by the portal when rendering.
		//This causes havoc when computing the deltas in view angle changes in the weapon. To fix this we transform the last facing direction by the same matrix
		if( pTransformedByPortal )
		{
			Vector vTemp;
			VectorRotate( vm->m_vecLastFacing, pTransformedByPortal->m_matrixThisToLinked.As3x4(), vTemp );
			vm->m_vecLastFacing = vTemp;

			vm->CalcViewModelView( this, eyeOrigin, eyeAngles );

			VectorRotate( vm->m_vecLastFacing, pTransformedByPortal->m_hLinkedPortal.Get()->m_matrixThisToLinked.As3x4(), vTemp );
			vm->m_vecLastFacing = vTemp;
		}
		else
		{
			vm->CalcViewModelView( this, eyeOrigin, eyeAngles );
		}

		if( bInvertFacing )
		{
			Vector vTemp;
			VectorIRotate( vm->m_vecLastFacing, m_matLatestServerTeleportationInverseMatrix.As3x4(), vTemp );
			vm->m_vecLastFacing = vTemp;
		}
	}
}

bool LocalPlayerIsCloseToPortal( void )
{
	return C_Portal_Player::GetLocalPlayer()->IsCloseToPortal();
}

static ConVar portal_tauntcam_yaw( "portal_tauntcam_yaw", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConVar portal_tauntcam_pitch( "portal_tauntcam_pitch", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConVar portal_tauntcam_speed( "portal_tauntcam_speed", "600", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

static ConVar portal_deathcam_pitch( "portal_deathcam_pitch", "45.f", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConVar portal_deathcam_gib_pitch( "portal_deathcam_gib_pitch", "25.f", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEG_NOINLINE void C_Portal_Player::TurnOnTauntCam( void )
{
	if ( !IsLocalPlayer( this ) )
		return;

	m_bFinishingTaunt = false;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

	// Save the old view angles.
	engine->GetViewAngles( m_angTauntEngViewAngles );
	prediction->GetViewAngles( m_angTauntPredViewAngles );

	m_bFaceTauntCameraEndAngles = false;

	if ( m_bTauntRemoteView )
	{
		Vector vTargetPos = GetThirdPersonViewPosition();
		float flDist = vTargetPos.DistTo( m_vecRemoteViewOrigin );
		
		trace_t trace;
		UTIL_TraceLine( vTargetPos, m_vecRemoteViewOrigin, (CONTENTS_SOLID|CONTENTS_MOVEABLE), NULL, COLLISION_GROUP_NONE, &trace );

		if ( !trace.startsolid && trace.DidHit() )
		{
			flDist *= trace.fraction;
		}
				
		m_TauntCameraData.m_flPitch = m_vecRemoteViewAngles.x;
		m_TauntCameraData.m_flYaw =  m_vecRemoteViewAngles.y;
		m_TauntCameraData.m_flDist = flDist;
		m_TauntCameraData.m_flLag = -1.0f;
		m_TauntCameraData.m_vecHullMin.Init( -1.0f, -1.0f, -1.0f );
		m_TauntCameraData.m_vecHullMax.Init( 1.0f, 1.0f, 1.0f );

		CEG_PROTECT_MEMBER_FUNCTION( C_Portal_Player_TurnOnTauntCam )

		QAngle vecCameraOffset( m_vecRemoteViewAngles.x, m_vecRemoteViewAngles.y, flDist );
		input->CAM_ToThirdPerson();
		ThirdPersonSwitch( true );
		input->CAM_SetCameraThirdData( &m_TauntCameraData, vecCameraOffset );
	}
	else
	{
		m_flTauntCamTargetDist = m_fTauntCameraDistance;
		m_flTauntCamCurrentDist = 0.f;

		m_TauntCameraData.m_flPitch = m_vecRemoteViewAngles.x;
		m_TauntCameraData.m_flYaw = m_vecRemoteViewAngles.y;
		m_TauntCameraData.m_flDist = m_flTauntCamTargetDist;
		m_TauntCameraData.m_flLag = 1.0f;
		m_TauntCameraData.m_vecHullMin.Init( -9.0f, -9.0f, -9.0f );
		m_TauntCameraData.m_vecHullMax.Init( 9.0f, 9.0f, 9.0f );

		QAngle angle = EyeAngles();
		float pitch;
		bool bInterpolateViewToAngle = true;
		if ( m_Shared.InCond( PORTAL_COND_DROWNING ) )
		{
			pitch = portal_deathcam_pitch.GetFloat();
			bInterpolateViewToAngle = false;
		}
		else if ( m_Shared.InCond( PORTAL_COND_DEATH_GIB ) )
		{
			pitch = portal_deathcam_gib_pitch.GetFloat();
			bInterpolateViewToAngle = false;
		}
		else
		{
			pitch = angle[PITCH];
		}

		g_ThirdPersonManager.UseCameraOffsets( true );
		g_ThirdPersonManager.SetCameraOffsetAngles( Vector( pitch, angle[YAW], m_flTauntCamCurrentDist ) );
		g_ThirdPersonManager.SetDesiredCameraOffset( Vector( pitch, angle[YAW], m_flTauntCamTargetDist ) );
		g_ThirdPersonManager.SetOverridingThirdPerson( true );

		input->CAM_ToThirdPerson();
		ThirdPersonSwitch( true );

		m_bTauntInterpolating = true;
		m_bTauntInterpolatingAngles = bInterpolateViewToAngle;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_Portal_Player::TurnOffTauntCam( void )
{
	if ( !IsLocalPlayer( this ) )
		return;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

	CEG_PROTECT_MEMBER_FUNCTION( C_Portal_Player_TurnOffTauntCam )

	if ( m_bTauntRemoteView )
	{
		TurnOffTauntCam_Finish();
	}
	else
	{
		// We want to interpolate back into the guy's head.
		m_flTauntCamTargetDist = 0.f;
		m_TauntCameraData.m_flDist = m_flTauntCamTargetDist;

		g_ThirdPersonManager.SetOverridingThirdPerson( false );
		
		m_bTauntInterpolating = true;

		if ( ( cl_taunt_finish_rotate_cam.GetInt() != 0 ) && !m_bFaceTauntCameraEndAngles )
		{
			m_TauntCameraData.m_flPitch = 0;
			m_TauntCameraData.m_flYaw = 0;
			m_TauntCameraData.m_flLag = -1.0f;
			m_bFinishingTaunt = true;
			m_bTauntInterpolatingAngles = true;
		}
	}
}


void C_Portal_Player::TurnOffTauntCam_Finish()
{
	if ( !IsLocalPlayer() )
		return;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

	Vector vecOffset = g_ThirdPersonManager.GetCameraOffsetAngles();
	portal_tauntcam_pitch.SetValue( vecOffset[PITCH] - m_angTauntPredViewAngles[PITCH] );
	portal_tauntcam_yaw.SetValue( vecOffset[YAW] - m_angTauntPredViewAngles[YAW] );

	QAngle angles;
	angles[PITCH] = vecOffset[PITCH];
	angles[YAW] = vecOffset[YAW];
	angles[DIST] = vecOffset[DIST];

	if ( m_bFaceTauntCameraEndAngles )
	{
		// Reset the old view angles.
		engine->SetViewAngles( angles );
		prediction->SetViewAngles( angles );
	}

	g_ThirdPersonManager.SetOverridingThirdPerson( false );

	if ( g_ThirdPersonManager.WantToUseGameThirdPerson() == false )
	{
		input->CAM_ToFirstPerson();
		ThirdPersonSwitch( false );
		angles = vec3_angle;
	}

	RANDOM_CEG_TEST_SECRET_PERIOD( 12, 19 )

	input->CAM_SetCameraThirdData( NULL, angles );

	// Force the feet to line up with the view direction post taunt.
	m_PlayerAnimState->m_bForceAimYaw = true;

	m_bTauntInterpolating = false;
	m_bTauntInterpolatingAngles = false;

	if ( GetViewModel() )
	{
		GetViewModel()->UpdateVisibility();
	}

	m_bFinishingTaunt = false;
}


void C_Portal_Player::TauntCamInterpolation()
{
	C_Portal_Player *pLocalPlayer = C_Portal_Player::GetLocalPortalPlayer();

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

	if ( pLocalPlayer && m_bTauntInterpolating )
	{
		float flSpeed = gpGlobals->frametime * portal_tauntcam_speed.GetFloat();

		if ( m_flTauntCamCurrentDist < m_flTauntCamTargetDist )
		{
			m_flTauntCamCurrentDist += flSpeed;
			m_flTauntCamCurrentDist = clamp( m_flTauntCamCurrentDist, m_flTauntCamCurrentDist, m_flTauntCamTargetDist );
		}
		else if ( m_flTauntCamCurrentDist > m_flTauntCamTargetDist )
		{
			m_flTauntCamCurrentDist -= flSpeed;
			m_flTauntCamCurrentDist = clamp( m_flTauntCamCurrentDist, m_flTauntCamTargetDist, m_flTauntCamCurrentDist );
		}

		Vector vecOrigin = pLocalPlayer->GetThirdPersonViewPosition();

		Vector vecCamOffset = g_ThirdPersonManager.GetCameraOffsetAngles();

		CTraceFilterSkipTwoEntities filter( pLocalPlayer, NULL );
		if ( pLocalPlayer->GetTeamTauntState() >= TEAM_TAUNT_HAS_PARTNER )
		{
			for( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				C_Portal_Player *pOtherPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

				//If the other player does not exist or if the other player is the local player
				if( pOtherPlayer == NULL || pOtherPlayer == pLocalPlayer )
					continue;

				filter.SetPassEntity2( pOtherPlayer );
				break;
			}
		}

		if ( m_bTauntInterpolatingAngles )
		{
			m_flTauntCamTargetPitch = m_TauntCameraData.m_flPitch;
			m_flTauntCamTargetYaw = 0.0f;

			float flBestFraction = 0.0f;
			float flBestYaw = m_TauntCameraData.m_flYaw;

			int nTestNum = 0;
			const int nMaxTests = 15;
			const float fJitter = 180.0f / static_cast<float>( nMaxTests );

			while ( flBestFraction < 0.75f && nTestNum < nMaxTests )
			{
				// Test and modify goals if camera doesn't fit
				float fCurrentYaw = m_TauntCameraData.m_flYaw + nTestNum * fJitter * ( ( nTestNum % 2 == 0 ) ? 1.0f : -1.0f );
				m_flTauntCamTargetYaw = GetAbsAngles()[ YAW ] + fCurrentYaw;

				Vector vTestForward;
				AngleVectors( QAngle( m_flTauntCamTargetPitch, m_flTauntCamTargetYaw, 0 ), &vTestForward, NULL, NULL );

				trace_t trace;
				UTIL_TraceHull( vecOrigin, vecOrigin - ( vTestForward * m_flTauntCamTargetDist ), Vector( -9.f, -9.f, -9.f ), Vector( 9.f, 9.f, 9.f ), MASK_SOLID, &filter, &trace );

				if ( flBestFraction < trace.fraction )
				{
					flBestFraction = trace.fraction;
					flBestYaw = fCurrentYaw;
				}

				nTestNum++;
			}

			m_TauntCameraData.m_flYaw = flBestYaw;

			float flRotMultiplier = m_bFinishingTaunt ? cl_taunt_finish_speed.GetFloat() : 0.5f;
			vecCamOffset[ PITCH ] = ApproachAngle( m_flTauntCamTargetPitch, vecCamOffset[ PITCH ], flSpeed * flRotMultiplier );
			vecCamOffset[ YAW ] = ApproachAngle( m_flTauntCamTargetYaw, vecCamOffset[ YAW ], flSpeed * flRotMultiplier );
			g_ThirdPersonManager.SetCameraOffsetAngles( vecCamOffset );

			if ( fabsf( AngleDiff( m_flTauntCamTargetPitch, vecCamOffset[ PITCH ] ) ) <= 1.0f && 
				 fabsf( AngleDiff( m_flTauntCamTargetYaw, vecCamOffset[ YAW ] ) ) <= 1.0f )
			{
				m_bTauntInterpolatingAngles = false;
			}
		}

		Vector vecForward;
		AngleVectors( QAngle( vecCamOffset[PITCH], vecCamOffset[YAW], 0 ), &vecForward, NULL, NULL );

		trace_t trace;
		UTIL_TraceHull( vecOrigin, vecOrigin - ( vecForward * m_flTauntCamCurrentDist ), Vector( -9.f, -9.f, -9.f ), Vector( 9.f, 9.f, 9.f ), MASK_SOLID, &filter, &trace );

		if ( trace.fraction < 1.0 )
			m_flTauntCamCurrentDist *= trace.fraction;

		QAngle angCameraOffset = QAngle( vecCamOffset[PITCH], vecCamOffset[YAW], m_flTauntCamCurrentDist );
		input->CAM_SetCameraThirdData( &m_TauntCameraData, angCameraOffset ); // Override camera distance interpolation.

		g_ThirdPersonManager.SetDesiredCameraOffset( Vector( m_flTauntCamCurrentDist, 0, 0 ) );

		if ( m_flTauntCamCurrentDist == m_flTauntCamTargetDist )
		{
			if ( m_flTauntCamTargetDist == 0.f )
			{
				TurnOffTauntCam_Finish();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_Portal_Player::HandleTaunting( void )
{
	C_Portal_Player *pLocalPlayer = C_Portal_Player::GetLocalPortalPlayer();

	// Clear the taunt slot.
	if ( !m_bWasTaunting && IsTaunting() )
	{
		m_bWasTaunting = true;

		// Handle the camera for the local player.
		if ( pLocalPlayer )
		{
			TurnOnTauntCam();
		}
	}

	if ( m_bWasTaunting && !IsTaunting() )
	{
		m_bWasTaunting = false;

		// Clear the vcd slot.
		m_PlayerAnimState->ResetGestureSlot( GESTURE_SLOT_VCD );

		// Handle the camera for the local player.
		if ( pLocalPlayer )
		{
			TurnOffTauntCam();
		}
	}

	TauntCamInterpolation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_Portal_Player::StartSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget )
{
	switch ( event->GetType() )
	{
	case CChoreoEvent::SEQUENCE:
	case CChoreoEvent::GESTURE:
		return StartGestureSceneEvent( info, scene, event, actor, pTarget );
	default:
		return BaseClass::StartSceneEvent( info, scene, event, actor, pTarget );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_Portal_Player::StartGestureSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget )
{
	// Get the (gesture) sequence.
	info->m_nSequence = LookupSequence( event->GetParameters() );
	if ( info->m_nSequence < 0 )
		return false;

	// Player the (gesture) sequence.
	m_PlayerAnimState->AddVCDSequenceToGestureSlot( GESTURE_SLOT_VCD, info->m_nSequence );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Don't collide with other players, we'll just push away from them
//-----------------------------------------------------------------------------
bool C_Portal_Player::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	// Don't hit other players
	if ( portal_use_player_avoidance.GetBool() && ( ( collisionGroup == COLLISION_GROUP_PLAYER || collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT ) ) )
		return false;

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

void C_Portal_Player::ApplyTransformToInterpolators( const VMatrix &matTransform, float fUpToTime, bool bIsRevertingPreviousTransform, bool bDuckForced )
{	
	Vector vOriginToCenter = (GetHullMaxs() + GetHullMins()) * 0.5f;
	Vector vCenterToOrigin = -vOriginToCenter;
	Vector vViewOffset = vec3_origin;
	VMatrix matCenterTransform = matTransform, matEyeTransform;
	Vector vOldEye = GetViewOffset();
	Vector vNewEye = GetViewOffset();

	if( bDuckForced )
	{
		// Going to be standing up
		if( bIsRevertingPreviousTransform )
		{
			vNewEye = VEC_VIEW;
			vViewOffset = VEC_VIEW - VEC_DUCK_VIEW;
			vOriginToCenter = (GetDuckHullMins() + GetDuckHullMaxs()) * 0.5f;
			vCenterToOrigin = -(GetStandHullMins() + GetStandHullMaxs()) * 0.5f;
		}
		// Going to be crouching
		else
		{
			vNewEye = VEC_DUCK_VIEW;
			vViewOffset = VEC_DUCK_VIEW - VEC_VIEW;
			vOriginToCenter = (GetStandHullMins() + GetStandHullMaxs()) * 0.5f;
			vCenterToOrigin = -(GetDuckHullMins() + GetDuckHullMaxs()) * 0.5f;
		}

		vOldEye = matTransform.ApplyRotation( vOldEye );
		Vector vEyeOffset = vOldEye - vNewEye - vCenterToOrigin;
		matEyeTransform = SetupMatrixTranslation(vEyeOffset);
	}
	else
	{
		vOldEye -= vOriginToCenter;
		vOldEye = matTransform.ApplyRotation( vOldEye );
		vOldEye += vOriginToCenter;

		Vector vEyeOffset = vOldEye - vNewEye;
		matEyeTransform = SetupMatrixTranslation(vEyeOffset);
	}

	// There's a 1-frame pop in multiplayer with lag when forced to duck.  WHAT THE FUCKKKKKKKKKKKKKKK

	// Translate origin to center
	matCenterTransform = matCenterTransform * SetupMatrixTranslation(vOriginToCenter);
	// Translate center to origin
	matCenterTransform = SetupMatrixTranslation( vCenterToOrigin ) * matCenterTransform;

	VMatrix matViewOffset = SetupMatrixTranslation( vViewOffset );

	if( bIsRevertingPreviousTransform )
	{
		GetOriginInterpolator().RemoveDiscontinuity( fUpToTime, &matCenterTransform.As3x4() );
		GetRotationInterpolator().RemoveDiscontinuity( fUpToTime, &matCenterTransform.As3x4() );
		m_iv_angEyeAngles.RemoveDiscontinuity( fUpToTime, &matCenterTransform.As3x4() );
		m_iv_vecViewOffset.RemoveDiscontinuity( fUpToTime, &matViewOffset.As3x4() );
		m_iv_vEyeOffset.RemoveDiscontinuity( fUpToTime, &matEyeTransform.As3x4() );
	}
	else
	{
		GetOriginInterpolator().InsertDiscontinuity( matCenterTransform.As3x4(), fUpToTime );
		GetRotationInterpolator().InsertDiscontinuity( matCenterTransform.As3x4(), fUpToTime );
		m_iv_angEyeAngles.InsertDiscontinuity( matCenterTransform.As3x4(), fUpToTime );
		m_iv_vecViewOffset.InsertDiscontinuity( matViewOffset.As3x4(), fUpToTime );
		m_iv_vEyeOffset.InsertDiscontinuity( matEyeTransform.As3x4(), fUpToTime );
	}	

	m_PlayerAnimState->TransformYAWs( matCenterTransform.As3x4() );
	
	AddEFlags( EFL_DIRTY_ABSTRANSFORM );
}

ConVar cl_resetportalledplayerinterp( "cl_resetportalledplayerinterp", "0" );

void C_Portal_Player::ApplyUnpredictedPortalTeleportation( const C_Portal_Base2D *pEnteredPortal, float flTeleportationTime, bool bForcedDuck )
{
	ApplyTransformToInterpolators( pEnteredPortal->m_matrixThisToLinked, flTeleportationTime, false, bForcedDuck );

	//Warning( "Applying teleportation view angle change %d, %f\n", m_PredictedPortalTeleportations.Count(), gpGlobals->curtime );

	if( IsLocalPlayer() && (GET_ACTIVE_SPLITSCREEN_SLOT() == GetSplitScreenPlayerSlot()) )
	{
		//Warning( "C_Portal_Player::ApplyUnpredictedPortalTeleportation() ent:%i slot:%i\n", entindex(), engine->GetActiveSplitScreenPlayerSlot() );
		matrix3x4_t matAngleTransformIn, matAngleTransformOut; //temps for angle transformation
		{
			QAngle qEngineAngles;
			engine->GetViewAngles( qEngineAngles );
			AngleMatrix( qEngineAngles, matAngleTransformIn );
			ConcatTransforms( pEnteredPortal->m_matrixThisToLinked.As3x4(), matAngleTransformIn, matAngleTransformOut );
			MatrixAngles( matAngleTransformOut, qEngineAngles );
			engine->SetViewAngles( qEngineAngles );
			pl.v_angle = qEngineAngles;
		}
	}

	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *pViewModel = GetViewModel( i );
		if ( !pViewModel )
			continue;

		pViewModel->m_vecLastFacing = pEnteredPortal->m_matrixThisToLinked.ApplyRotation( pViewModel->m_vecLastFacing );
	}

#if ( PLAYERPORTALDEBUGSPEW == 1 )
	Warning( "C_Portal_Player::ApplyUnpredictedPortalTeleportation( %f )\n", flTeleportationTime/*gpGlobals->curtime*/ );
#endif

	PostTeleportationCameraFixup( pEnteredPortal );

	if( IsToolRecording() )
	{		
		KeyValues *msg = new KeyValues( "entity_nointerp" );

		// Post a message back to all IToolSystems
		Assert( (int)GetToolHandle() != 0 );
		ToolFramework_PostToolMessage( GetToolHandle(), msg );

		msg->deleteThis();
	}

	// Use a slightly expanded box to search for stick surfaces as the player leaves the portal.
	m_flUsePostTeleportationBoxTime = sv_post_teleportation_box_time.GetFloat();

	SetOldPlayerZ( GetNetworkOrigin().z );
}

void C_Portal_Player::ApplyPredictedPortalTeleportation( C_Portal_Base2D *pEnteredPortal, CMoveData *pMove, bool bForcedDuck )
{
	if( pEnteredPortal->m_hLinkedPortal.Get() != NULL )
	{
		m_matLatestServerTeleportationInverseMatrix = pEnteredPortal->m_hLinkedPortal->MatrixThisToLinked();
	}
	else
	{
		m_matLatestServerTeleportationInverseMatrix.Identity();
	}

	m_fLatestServerTeleport = gpGlobals->curtime;

	C_PortalGhostRenderable *pGhost = pEnteredPortal->GetGhostRenderableForEntity( this );
	if( !pGhost )
	{
		//high velocity edge case. Entity portalled before it ever created a clone. But will need one for the interpolated origin history
		if( C_PortalGhostRenderable::ShouldCloneEntity( this, pEnteredPortal, false ) )
		{
			pGhost = C_PortalGhostRenderable::CreateGhostRenderable( this, pEnteredPortal );
			Assert( !pEnteredPortal->m_hGhostingEntities.IsValidIndex( pEnteredPortal->m_hGhostingEntities.Find( this ) ) );
			pEnteredPortal->m_hGhostingEntities.AddToTail( this );
			Assert( pEnteredPortal->m_GhostRenderables.IsValidIndex( pEnteredPortal->m_GhostRenderables.Find( pGhost ) ) );
			pGhost->PerFrameUpdate();
		}
	}

	if( pGhost )
	{
		C_PortalGhostRenderable::CreateInversion( pGhost, pEnteredPortal, gpGlobals->curtime );
	}

	//Warning( "C_Portal_Player::ApplyPredictedPortalTeleportation() ent:%i slot:%i\n", entindex(), engine->GetActiveSplitScreenPlayerSlot() );
	ApplyTransformToInterpolators( pEnteredPortal->m_matrixThisToLinked, gpGlobals->curtime, false, bForcedDuck );

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

	if( prediction->IsFirstTimePredicted() && IsToolRecording() )
	{
		KeyValues *msg = new KeyValues( "entity_nointerp" );
		
		// Post a message back to all IToolSystems
		Assert( (int)GetToolHandle() != 0 );
		ToolFramework_PostToolMessage( GetToolHandle(), msg );

		msg->deleteThis();
	}

	// Use a slightly expanded box to search for stick surfaces as the player leaves the portal.
	m_flUsePostTeleportationBoxTime = sv_post_teleportation_box_time.GetFloat();

	if ( m_bFlingTrailPrePortalled )
	{
		m_bFlingTrailPrePortalled = false;
		m_bFlingTrailJustPortalled = true;
	}

	SetOldPlayerZ( pMove->GetAbsOrigin().z );
}

void C_Portal_Player::UndoPredictedPortalTeleportation( const C_Portal_Base2D *pEnteredPortal, float fOriginallyAppliedTime, const VMatrix &matUndo, bool bForcedDuck )
{
	ApplyTransformToInterpolators( matUndo, fOriginallyAppliedTime, true, bForcedDuck );

	// Don't use the expanded box to search for stick surfaces, since the player hasn't teleported yet.
	m_flUsePostTeleportationBoxTime = 0.0f;

	SetOldPlayerZ( GetNetworkOrigin().z );
}

void C_Portal_Player::UnrollPredictedTeleportations( int iCommandNumber )
{
	//roll back changes that aren't automatically restored when rolling back prediction time
	//ACTIVE_SPLITSCREEN_PLAYER_GUARD( this );

	if( (m_PredictedPortalTeleportations.Count() != 0) && (iCommandNumber <= m_PredictedPortalTeleportations.Tail().iCommandNumber) )
	{
		matrix3x4_t matAngleTransformIn, matAngleTransformOut; //temps for angle transformation

		QAngle qEngineViewAngles;
		engine->GetViewAngles( qEngineViewAngles );
		//QAngle qVAngles = player->pl.v_angle;

		//crap, re-predicting teleportations. This is fine for the CMoveData, but CUserCmd/engine view angles are temporally sensitive.
		for( int i = m_PredictedPortalTeleportations.Count(); --i >= 0; )
		{
			if( iCommandNumber <= m_PredictedPortalTeleportations[i].iCommandNumber )
			{
				const VMatrix &matTransform = m_PredictedPortalTeleportations[i].matUnroll;
				//undo the view transformation this previous (but future) teleportation applied to the view angles.
				{
					AngleMatrix( qEngineViewAngles, matAngleTransformIn );
					ConcatTransforms( matTransform.As3x4(), matAngleTransformIn, matAngleTransformOut );
					MatrixAngles( matAngleTransformOut, qEngineViewAngles );
				}

				/*{
				AngleMatrix( qVAngles, matAngleTransformIn );
				ConcatTransforms( matTransform.As3x4(), matAngleTransformIn, matAngleTransformOut );
				MatrixAngles( matAngleTransformOut, qVAngles );
				}*/

				UndoPredictedPortalTeleportation( m_PredictedPortalTeleportations[i].pEnteredPortal, m_PredictedPortalTeleportations[i].flTime, matTransform, m_PredictedPortalTeleportations[i].bDuckForced );

				// Hack? Squash camera values back to neutral.
				m_PortalLocal.m_Up = Vector(0,0,1);
				m_PortalLocal.m_qQuaternionPunch.Init(0,0,0);
				m_PortalLocal.m_vEyeOffset = vec3_origin;

#if ( PLAYERPORTALDEBUGSPEW == 1 )
				Warning( "<--Rolling back predicted teleportation %d, %f %i %i\n", m_PredictedPortalTeleportations.Count(), m_PredictedPortalTeleportations[i].flTime, m_PredictedPortalTeleportations[i].iCommandNumber, iCommandNumber );
#endif
				m_PredictedPortalTeleportations.FastRemove( i );
			}
			else
			{
				break;
			}
		}

		if( IsLocalPlayer() )
		{
			engine->SetViewAngles( qEngineViewAngles );
			//Warning( "C_Portal_Player::UnrollPredictedTeleportations() ent:%i slot:%i\n", entindex(), engine->GetActiveSplitScreenPlayerSlot() );
		}
		//player->pl.v_angle = qVAngles;
	}
}

void C_Portal_Player::Precache( void )
{
	// load the bot textures in co-op
	if ( g_pGameRules->IsMultiplayer() )
	{
		PrecacheParticleSystem( "coop_robot_talk_blue" );
		PrecacheParticleSystem( "coop_robot_talk_orange" );
		PrecacheParticleSystem( "electrical_arc_01" );
		PrecacheParticleSystem( "bot_death_B_gib" );
		PrecacheParticleSystem( "bot_fling_trail_rainbow" );
		PrecacheParticleSystem( "bot_fling_trail" );
	}
}


//////////////////////////////////////////////////////////////////////////
// PAINT SECTION
//////////////////////////////////////////////////////////////////////////

bool C_Portal_Player::RenderLocalScreenSpaceEffect( PortalScreenSpaceEffect effect, IMatRenderContext *pRenderContext, int x, int y, int w, int h )
{
	C_Portal_Player *pLocalPlayer = C_Portal_Player::GetLocalPlayer();
	if( pLocalPlayer )
	{
		return pLocalPlayer->RenderScreenSpaceEffect( effect, pRenderContext, x, y, w, h );
	}

	return false;
}


bool C_Portal_Player::RenderScreenSpaceEffect( PortalScreenSpaceEffect effect, IMatRenderContext *pRenderContext, int x, int y, int w, int h )
{
	bool result = false;
	switch( effect )
	{
	case PAINT_SCREEN_SPACE_EFFECT:
		result = RenderScreenSpacePaintEffect( pRenderContext );
		break;
	}

	return result;
}

static void SetRenderTargetAndViewPort(ITexture *rt)
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetRenderTarget(rt);
	if ( rt )
	{
		pRenderContext->Viewport(0,0,rt->GetActualWidth(),rt->GetActualHeight());
	}
}


bool C_Portal_Player::ScreenSpacePaintEffectIsActive() const
{
	// The reference is valid and the referenced particle system is also valid
	return m_PaintScreenSpaceEffect.IsValid() && m_PaintScreenSpaceEffect->IsValid();
}


void C_Portal_Player::SetScreenSpacePaintEffectColors( IMaterialVar* pColor1, IMaterialVar* pColor2 ) const
{
	const Color visualColor = MapPowerToVisualColor( m_PortalLocal.m_PaintedPowerType );
	Vector vColor1( visualColor.r(), visualColor.g(), visualColor.b() );
	Vector vColor2 = vColor1;
	for( unsigned i = 0; i < 3; ++i )
	{
		vColor2[i] = clamp( vColor2[i] - 15.0f, 0, 255 );
	}

	vColor1.NormalizeInPlace();
	vColor2.NormalizeInPlace();

	pColor1->SetVecValue( vColor1.x, vColor1.y, vColor1.z );
	pColor2->SetVecValue( vColor2.x, vColor2.y, vColor2.z );
}


bool C_Portal_Player::RenderScreenSpacePaintEffect( IMatRenderContext *pRenderContext )
{
	if( ScreenSpacePaintEffectIsActive() )
	{
		pRenderContext->PushRenderTargetAndViewport();
		ITexture* pDestRenderTarget = materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );
		SetRenderTargetAndViewPort( pDestRenderTarget );
		pRenderContext->ClearColor4ub( 128, 128, 0, 0 );
		pRenderContext->ClearBuffers( true, false, false );
		RenderableInstance_t instance;
		instance.m_nAlpha = 255;
		m_PaintScreenSpaceEffect->DrawModel( 1, instance );

		if( IsGameConsole() )
		{
			pRenderContext->CopyRenderTargetToTextureEx( pDestRenderTarget, 0, NULL, NULL );
		}

		pRenderContext->PopRenderTargetAndViewport();

		return true;
	}

	return false;
}


void C_Portal_Player::InvalidatePaintEffects()
{
	// Remove paint screen space effect
	if( m_PaintScreenSpaceEffect.IsValid() )
	{
		m_PaintScreenSpaceEffect->StopEmission();
		STEAMWORKS_TESTSECRETALWAYS();
		m_PaintScreenSpaceEffect = NULL;
	}

	// commenting out the 3rd person drop effect
	/*
	// Remove paint drip effect
	if( m_PaintDripEffect.IsValid() )
	{
		m_PaintDripEffect->StopEmission();
		m_PaintDripEffect = NULL;
	}
	*/
}


void C_Portal_Player::ClientPlayerRespawn()
{
	if ( IsLocalPlayer( this ) )
	{
		m_bGibbed = false;

		// Dod called these, not sure why
		//MoveToLastReceivedPosition( true );
		//ResetLatched();

		// Reset the camera.
		m_bWasTaunting = false;

		if ( IsLocalPlayer( this ) )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

			g_ThirdPersonManager.SetOverridingThirdPerson( false );

			if ( g_ThirdPersonManager.WantToUseGameThirdPerson() == false )
			{
				input->CAM_ToFirstPerson();
				ThirdPersonSwitch( false );
				input->CAM_SetCameraThirdData( NULL, vec3_angle );
				g_ThirdPersonManager.UseCameraOffsets( false );
			}

			m_bTauntInterpolating = false;
			m_bTauntInterpolatingAngles = false;
		}

		//ResetToneMapping(1.0);

		//// Release the duck toggle key (
		//{
		//	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );
		//	KeyUp( &in_ducktoggle, NULL ); 
		//}

		//IGameEvent *event = gameeventmanager->CreateEvent( "localplayer_respawn" );
		//if ( event )
		//{
		//	gameeventmanager->FireEventClientSide( event );
		//}

		RANDOM_CEG_TEST_SECRET();

		CPortalMPGameRules *pRules = PortalMPGameRules();
		// we want to force the screen to not be split on the credits, but we want to keep the other player connected
		if ( pRules && pRules->IsCreditsMap() )
		{
			engine->ClientCmd( "ss_force_primary_fullscreen 1" );
		}
		else // otherwise, we don't want this to be the case
		{
			engine->ClientCmd( "ss_force_primary_fullscreen 0" );
		}
	}

	// clear animation state
	m_PlayerAnimState->ClearAnimationState();

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	UpdateInventory();
	UpdateClientsideWearables();
#endif
}


// Override the default fov so that the fov from the splitscreen_config.txt is the one used.
// FIXME: Need to make other splitscreen-capable games use this method.  
// It doesn't currently support user-controllable FOV overrides though like TF2, so only using this for L4D at the moment.
int C_Portal_Player::GetDefaultFOV( void ) const
{ 
	return cl_fov.GetFloat();
}


#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

//-----------------------------------------------------------------------------
// Purpose: Request this player's inventories from the steam backend
//-----------------------------------------------------------------------------
void C_Portal_Player::UpdateInventory( void )
{
	if ( !g_pGameRules->IsMultiplayer() )
		return;

	if ( !m_bInventoryReceived )
	{
		CSteamID steamIDForPlayer;
		if ( GetSteamID( &steamIDForPlayer ) )
		{
			PortalInventoryManager()->SteamRequestInventory( &m_Inventory, steamIDForPlayer, this );
		}

		// If we have an SOCache, we've got a connection to the GC
		bool bInvalid = true;
		if ( m_Inventory.GetSOC() )
		{
			bInvalid = (m_Inventory.GetSOC()->BIsInitialized() == false);
		}
		m_bInventoryReceived = !bInvalid;
	}
}

// Our inventory has changed. Make sure we're wearing the right things.
void C_Portal_Player::InventoryUpdated( CPlayerInventory *pInventory )
{
	UpdateClientsideWearables();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_Portal_Player::ItemsMatch( CEconItemView *pCurItem, CEconItemView *pNewItem )
{
	if ( !pNewItem || !pNewItem->IsValid() )
		return false;

	// If we already have an item in this slot but is not the same type, nuke it (changed classes)
	// We don't need to do this for non-base items because they've already been verified above.
	bool bHasNonBase = pNewItem ? pNewItem->GetItemQuality() != AE_NORMAL : false;
	if ( bHasNonBase )
	{
		// If the item isn't the one we're supposed to have, nuke it
		if ( pCurItem->GetItemID() != pNewItem->GetItemID() )
		{
			/*
			Msg("Removing %s because its global index (%d) doesn't match the loadout's (%d)\n", p->GetDebugName(), 
				pCurItem->GetItemID(),
				pNewItem->GetItemID() );
			*/
			return false;
		}
	}
	else
	{
		if ( pCurItem->GetItemQuality() != AE_NORMAL || (pCurItem->GetItemIndex() != pNewItem->GetItemIndex()) )
		{
			//Msg("Removing %s because it's not the right type for the class.\n", p->GetDebugName() );
			return false;
		}
	}

	return true;
}

void C_Portal_Player::UpdateClientsideWearables( void )
{
	if ( !g_pGameRules->IsMultiplayer() )
		return;

	C_Portal_Player *pHostPlayer = this;

	if ( IsSplitScreenPlayer() )
	{
		pHostPlayer = ToPortalPlayer( C_BasePlayer::GetLocalPlayer( 0 ) );
		if ( !pHostPlayer )
		{
			return;
		}
	}

	// Crap, what do we do about bodygroups? They're server authoritive.

	/*
	// First, reset all our bodygroups
	CStudioHdr studioHdr( GetStudioHdr(), g_pMDLCache );

	int iBodyGroup = FindBodygroupByName( &studioHdr, "hat" );
	if ( iBodyGroup > -1 )
	{
		::SetBodygroup( &studioHdr, m_nBody, iBodyGroup, 0 );
	}
	iBodyGroup = FindBodygroupByName( &studioHdr, "headphones" );
	if ( iBodyGroup > -1 )
	{
		::SetBodygroup( &studioHdr, m_nBody, iBodyGroup, 0 );
	}
	SetBody( m_nBody );
	*/

	CSteamID steamIDForPlayer;
	pHostPlayer->GetSteamID( &steamIDForPlayer );
	int iBot = ( GetTeamNumber() == TEAM_BLUE ) ? P2BOT_ATLAS : P2BOT_PBODY;

	// Go through our wearables and make sure we're supposed to be wearing them.
	// Need to move backwards because we'll be removing them as we find them.
	for ( int wbl = GetNumWearables()-1; wbl >= 0; wbl-- )
	{
		CEconWearable *pWearable = GetWearable(wbl);
		Assert( pWearable );
		if ( !pWearable )
			continue;

		int iLoadoutSlot = pWearable->GetAttributeContainer()->GetItem()->GetStaticData()->GetLoadoutSlot( iBot );
		CEconItemView *pItem = PortalInventoryManager()->GetItemInLoadoutForClass( iBot, iLoadoutSlot, &steamIDForPlayer );
		if ( !ItemsMatch( pWearable->GetAttributeContainer()->GetItem(), pItem ) || (pWearable->GetTeamNumber() != GetTeamNumber()) )
		{
			if ( !pWearable->AlwaysAllow() )
			{
				// We shouldn't have this wearable. Remove it.
				RemoveWearable( pWearable );
			}
		}
	}

	// Now go through all our loadout slots and find any wearables that we should be wearing.
	for ( int i = 0; i < LOADOUT_POSITION_COUNT; i++ )
	{
		m_EquippedLoadoutItemIndices[i] = LOADOUT_SLOT_USE_BASE_ITEM;

		CEconItemView *pItem = pHostPlayer->Inventory()->GetItemInLoadout( iBot, i );
		if ( !pItem || !pItem->IsValid() )
			continue;

		m_EquippedLoadoutItemIndices[i] = pItem->GetItemID();

		// See if we're already wearing it.
		bool bAlreadyHave = false;
		for ( int wbl = 0; wbl < GetNumWearables(); wbl++ )
		{
			C_EconWearable *pWearable = GetWearable(wbl);
			if ( !pWearable )
				continue;

			if ( ItemsMatch( pWearable->GetAttributeContainer()->GetItem(), pItem ) )
			{
				bAlreadyHave = true;
				break;
			}
		}

		if ( !bAlreadyHave )
		{
			// Only spawn wearables
			if ( !Q_strcmp( pItem->GetStaticData()->GetItemClass(), "wearable_item" ) )
			{
				CEconWearable *pNewItem = new CEconWearable();

				// Initialize with no model, because Spawn() will set it to the item's model afterwards.
				if ( pNewItem->InitializeAsClientEntity( NULL, RENDER_GROUP_OPAQUE ) )
				{
					pNewItem->GetAttributeContainer()->SetItem( pItem );
					pNewItem->Spawn();
					pNewItem->GiveTo( this );
				}
				else
				{
					delete pNewItem;
					m_EquippedLoadoutItemIndices[i] = LOADOUT_SLOT_USE_BASE_ITEM;
				}
			}
		}
	}

	for ( int i = 0; i < GetSplitScreenPlayers().Count(); ++i )
	{
		C_Portal_Player *pPlayer = static_cast< C_Portal_Player* >( GetSplitScreenPlayers()[ i ].Get() );
		if ( pPlayer )
		{
			pPlayer->UpdateClientsideWearables();
		}
	}
}

void C_Portal_Player::RemoveClientsideWearables( void )
{
	if ( !g_pGameRules->IsMultiplayer() )
		return;

	// Need to move backwards because we'll be removing them as we find them.
	for ( int wbl = GetNumWearables()-1; wbl >= 0; wbl-- )
	{
		CEconWearable *pWearable = GetWearable(wbl);
		if ( !pWearable )
			continue;

		RemoveWearable( pWearable );
	}

	for ( int i = 0; i < GetSplitScreenPlayers().Count(); ++i )
	{
		C_Portal_Player *pPlayer = static_cast< C_Portal_Player* >( GetSplitScreenPlayers()[ i ].Get() );
		if ( pPlayer )
		{
			pPlayer->RemoveClientsideWearables();
		}
	}
}

#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

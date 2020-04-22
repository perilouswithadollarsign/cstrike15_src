//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hltvcamera.h"
#include "cdll_client_int.h"
#include "util_shared.h"
#include "prediction.h"
#include "movevars_shared.h"
#include "in_buttons.h"
#include "text_message.h"
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#include "vguicenterprint.h"
#include "game/client/iviewport.h"
#include <keyvalues.h>
#include "matchmaking/imatchframework.h"
#include "iloadingdisc.h"
#include "view_shared.h"
#include "view.h"
#include "ivrenderview.h"
#include "c_plantedc4.h"
#include "basecsgrenade_projectile.h"
#include "ivieweffects.h"
#include "cs_hud_chat.h"
#include "in_buttons.h"
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include "hltvreplaysystem.h"

#ifdef CSTRIKE_DLL
	#include "c_cs_player.h"
	#include "cs_gamerules.h"
	#include "c_team.h"
#endif

static void Spec_Autodirector_Cameraman_Callback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( HLTVCamera() && HLTVCamera()->AutoDirectorState() != C_HLTVCamera::AUTODIRECTOR_OFF )
	{
		HLTVCamera()->SetAutoDirector(C_HLTVCamera::AUTODIRECTOR_ON);
	}
}

ConVar spec_autodirector( "spec_autodirector", "1", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "Auto-director chooses best view modes while spectating" );
ConVar spec_autodirector_pausetime( "spec_autodirector_pausetime", "10", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "Auto-director will pause for this long if a player is selected." );
ConVar spec_autodirector_cameraman( "spec_autodirector_cameraman", "-1", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "Cameraman account ID. If a cameraman is active then use them when spectating and autodirector is active, 0 = no caster", Spec_Autodirector_Cameraman_Callback );
ConVar spec_overwatch_skip_idle_ticks( "spec_overwatch_skip_idle_ticks", "10", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "Auto-director in overwatch mode will be skipping ticks when no subject observations are played." );

extern ConVar view_recoil_tracking;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define CHASE_CAM_DISTANCE		76.0f
#define WALL_OFFSET				6.0f

static Vector WALL_MIN(-WALL_OFFSET,-WALL_OFFSET,-WALL_OFFSET);
static Vector WALL_MAX(WALL_OFFSET,WALL_OFFSET,WALL_OFFSET);
static const ConVar	*tv_transmitall = NULL;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static C_HLTVCamera s_HLTVCamera;

C_HLTVCamera *HLTVCamera() { return &s_HLTVCamera; }

C_HLTVCamera::C_HLTVCamera()
{
	Reset();

	m_nNumSpectators = 0;
	m_szTitleText[0] = 0;
}

C_HLTVCamera::~C_HLTVCamera()
{

}

void C_HLTVCamera::Init()
{
	ListenForGameEvent( "game_newmap" );
#ifdef CAMERAMAN_OLD_WAY
	ListenForGameEvent( "hltv_cameraman" );
#endif
	ListenForGameEvent( "hltv_fixed" );
	ListenForGameEvent( "hltv_chase" );
	ListenForGameEvent( "hltv_message" );
	ListenForGameEvent( "hltv_title" );
	ListenForGameEvent( "hltv_status" );
	ListenForGameEvent( "player_connect" );
	ListenForGameEvent( "player_connect_full" );
	ListenForGameEvent( "player_team" );
	
	Reset();

	m_nNumSpectators = 0;
	m_szTitleText[0] = 0;

	// get a handle to the engine convar
	tv_transmitall = cvar->FindVar( "tv_transmitall" );
}

void C_HLTVCamera::Reset()
{
	m_nCameraMode = OBS_MODE_FIXED;
	m_iCameraMan  = 0;
	m_iTarget1 = m_iTarget2 = m_iLastTarget1 = 0;
	m_flFOV = 90;
	m_flDistance = m_flLastDistance = CHASE_CAM_DISTANCE;
	m_flInertia = 3.0f;
	m_flPhi = 0;
	m_flTheta = 0;
	m_flOffset = 0;
	m_bEntityPacketReceived = false;

	m_vCamOrigin.Zero();
	m_aCamAngle.Init();

	m_LastCmd.Reset();
	m_vecVelocity.Init();

	m_vIdealOverviewPos.Zero();
	m_vOldOverviewPos.Zero();
	m_vLastGrenadeVelocity.Zero();
	m_flLastGrenadeVelocityUpdate = 0;
	m_flLastCamZPos = 0;

	m_flAutodirectorPausedTime = -1.0f;
	m_flIdealOverviewScale = 1.0f;
	m_flNextIdealOverviewPosUpdate = 0;

	m_bIsSpecLerping = false;
	m_vecSpecLerpIdealPos = Vector( 0, 0, 0 );
	m_angSpecLerpIdealAng = QAngle( 0, 0, 0 );
	m_vecSpecLerpOldPos = Vector( 0, 0, 0 );
	m_angSpecLerpOldAng = QAngle( 0, 0, 0 );
	m_flSpecLerpEndTime = 0.0f;
	m_flSpecLerpTime = 1.0f;
	m_bIsFollowingGrenade = false;
}

void C_HLTVCamera::SetWatchingGrenade( C_BaseEntity *pGrenade, bool bWatching )
{
	if ( bWatching )
	{
		if ( pGrenade && m_bIsFollowingGrenade == false )
		{
			m_bIsFollowingGrenade = true;
			SetPrimaryTarget( pGrenade->entindex() );
		}
	}
	else
	{
		// only change state if we get a false
		//if ( pGrenade->entindex() == m_iTarget1 )
		{
			if ( m_bIsFollowingGrenade == true )
			{
				C_BaseEntity *pLastTarget = ClientEntityList().GetBaseEntity( m_iLastTarget1 );

				if ( !pLastTarget || pLastTarget->IsDormant() || !pLastTarget->IsAlive() )
				{
					// find a player that is alive to go to
					for ( int i = 1; i <= MAX_PLAYERS; i++ )
					{
						CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
						if ( pPlayer && pPlayer->IsAlive() && (pPlayer->GetTeamNumber() == TEAM_TERRORIST || pPlayer->GetTeamNumber() == TEAM_CT) )
						{
							m_iLastTarget1 = pPlayer->entindex();
							break;
						}
					}
				}

				m_iTarget1 = m_iLastTarget1;
			}

			m_bIsFollowingGrenade = false;
		}
	}
}

void C_HLTVCamera::CalcChaseCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
 	Vector targetOrigin1, targetOrigin2, cameraOrigin, forward;

 	if ( m_iTarget1 == 0 )
		return;

	// get primary target, also translates to ragdoll
	C_BaseEntity *target1 = GetPrimaryTarget();

	if ( !target1 ) 
	{
		bool bFoundTarget = false;
		if ( m_bIsFollowingGrenade )
		{
			// if we we3re following a grenade and lost our target, revert back to our previous target
			C_BaseEntity* oldTarget = ClientEntityList().GetEnt( m_iLastTarget1 );
			if ( oldTarget )
			{
				target1 = oldTarget;
				bFoundTarget = true;
			}
		}

		if ( !bFoundTarget )
			return;
	}
	
/*#ifdef CSTRIKE_DLL
	// weapon gun-cam go-pro chase camera
	C_CSPlayer *pPlayer = ToCSPlayer( target1 );
	if ( pPlayer && pPlayer->ShouldDraw() )
	{
		Vector vecSrc = target1->GetObserverCamOrigin();
		vecSrc += (target1->GetFlags() & FL_DUCKING) ? VEC_DUCK_VIEW : VEC_VIEW;

		Vector vecObsForward, vecObsRight, vecObsUp;
		AngleVectors( m_aCamAngle, &vecObsForward, &vecObsRight, &vecObsUp );

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

	CBaseCSGrenadeProjectile *pGrenade = dynamic_cast< CBaseCSGrenadeProjectile* >( target1 );

	if ( pGrenade )
	{
		m_iTarget2 = m_iTarget1;
		m_bIsFollowingGrenade = true;
	}

	bool bManual = !spec_autodirector.GetBool();
	if ( pGrenade && pGrenade->m_nBounces <= 0 )	// chase camera controlled manually
		bManual = false;

	if ( target1->IsPlayer() && target1->IsAlive() && target1->IsDormant() )
		return;

	targetOrigin1 = target1->GetRenderOrigin();

	if ( target1->IsPlayer() && !target1->IsAlive() )
	{
		targetOrigin1 += VEC_DEAD_VIEWHEIGHT;
	}
	else if ( target1->IsPlayer() && target1->GetFlags() & FL_DUCKING )
	{
		targetOrigin1 += VEC_DUCK_VIEW;
	}
	else if ( pGrenade )
	{
		targetOrigin1 += 2;//(VEC_DUCK_VIEW/2);
	}
	else
	{
		targetOrigin1 += VEC_VIEW;
	}

	// get secondary target if set
	C_BaseEntity *target2 = NULL;

	if ( m_iTarget2 > 0 && (m_iTarget2 != m_iTarget1) && !bManual )
	{
		target2 = ClientEntityList().GetBaseEntity( m_iTarget2 );

		// if target is out PVS and not dead, it's not valid
		if ( target2 && target2->IsDormant() && target2->IsAlive() )
			target2 = NULL;

		if ( target2 )
		{
			targetOrigin2 = target2->GetRenderOrigin();

			if ( !target2->IsAlive() )
			{
				targetOrigin2 += VEC_DEAD_VIEWHEIGHT;
			}
			else if ( target2->GetFlags() & FL_DUCKING )
			{
				targetOrigin2 += VEC_DUCK_VIEW;
			}
			else
			{
				targetOrigin2 += VEC_VIEW;
			}
		}
	}

		// apply angle offset & smoothing
	QAngle angleOffset(  m_flPhi, m_flTheta, 0 );
	QAngle cameraAngles = m_aCamAngle;

	if ( bManual )
	{
		// let spectator choose the view angles
 		engine->GetViewAngles( cameraAngles );
	}
	else if ( target2 )
	{
		// look into direction of second target
 		forward = targetOrigin2 - targetOrigin1;
        VectorAngles( forward, cameraAngles );
        cameraAngles.z = 0; // no ROLL
	}
	else if ( pGrenade || m_iTarget2 == 0 || m_iTarget2 == m_iTarget1 )
	{
		if ( pGrenade )
		{
			//QAngle angFacing = pGrenade->GetLocalVelocity();
			Vector vecVel = pGrenade->GetLocalVelocity();//pGrenade->GetLocalAngularVelocity();//targetOrigin1 - m_vLastTarget1Origin;
		
			//cameraAngles = angFacing;
//			engine->Con_NPrintf(30, "vel = (%f, %f, %f)", XYZ(vecVel) );
			//cameraAngles = angFacing;

			angleOffset.Init();

			//float flTransTime = 0.1f;
			float flInterp = clamp( ( gpGlobals->curtime - m_flLastGrenadeVelocityUpdate ) * 10, 0.0f, 1.0f );
			//float flActualinterp = Gain( flInterp, 0.6 );
			Vector vecActualVel = Lerp( flInterp, m_vLastGrenadeVelocity, vecVel );
			//cameraAngles.x = 0; // no PITCH

			VectorAngles( vecActualVel, cameraAngles );
			if ( m_vLastGrenadeVelocity != vecVel )
			{
				m_flLastGrenadeVelocityUpdate = gpGlobals->curtime;
				m_vLastGrenadeVelocity = vecActualVel;
			}

//			Msg( "vel = (%f, %f, %f) - flInterp = %f\n", XYZ(vecActualVel), flInterp );

			// set the max distance to 64 with grenades
			m_flDistance = 64;
		}
		else
		{
			// look into direction where primary target is looking
			cameraAngles = target1->EyeAngles();
			cameraAngles.x = 0; // no PITCH
		}

		cameraAngles.z = 0; // no ROLL
	}
	else
	{
		// target2 is missing, just keep angelsm, reset offset
		angleOffset.Init();
	}

	if ( !bManual && target1->IsPlayer() )
	{
		if ( !target1->IsAlive() )
		{
			angleOffset.x = 15;
		}

		cameraAngles += angleOffset;
	}

	AngleVectors( cameraAngles, &forward );

	VectorNormalize( forward );

	// calc optimal camera position
	VectorMA(targetOrigin1, -m_flDistance, forward, cameraOrigin );

 	targetOrigin1.z += m_flOffset; // add offset

	// clip against walls
  	trace_t trace;
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( targetOrigin1, cameraOrigin, WALL_MIN, WALL_MAX, MASK_SOLID, target1, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();

  	float dist = VectorLength( trace.endpos -  targetOrigin1 );

	// grow distance by 32 unit a second
  	m_flLastDistance += gpGlobals->frametime * 32.0f; 

  	if ( dist > m_flLastDistance )
	{
		VectorMA(targetOrigin1, -m_flLastDistance, forward, cameraOrigin );
	}
 	else
	{
		cameraOrigin = trace.endpos;
		m_flLastDistance = dist;
	}
	
  	if ( target2 /*|| pGrenade*/ )
	{
		if ( pGrenade )
		{
			// if we have 2 targets look at point between them
  			forward = targetOrigin1 - cameraOrigin;
  			QAngle angle;
  			VectorAngles( forward, angle );
  			cameraAngles = angle;

			NormalizeAngles( cameraAngles );
			//cameraAngles.x = clamp( cameraAngles.x, -30, 30 );
		}
		else
		{
			// if we have 2 targets look at point between them
			forward = ( targetOrigin1 + targetOrigin2 ) / 2 - cameraOrigin;
			QAngle angle;
			VectorAngles( forward, angle );
			cameraAngles.y = angle.y;

			NormalizeAngles( cameraAngles );
			cameraAngles.x = clamp( cameraAngles.x, -60, 60 );
		}

		SmoothCameraAngle( cameraAngles );
	}
	else
	{
		SetCameraAngle( cameraAngles );
	}

//	engine->Con_NPrintf(20, "pos = (%f, %f, %f)", XYZ(m_vCamOrigin) );
// 	engine->Con_NPrintf(22, "ang = (%f, %f, %f)", XYZ(m_aCamAngle) );

	VectorCopy( cameraOrigin, m_vCamOrigin );
	VectorCopy( m_aCamAngle, eyeAngles );
	VectorCopy( m_vCamOrigin, eyeOrigin );
}

Vector C_HLTVCamera::CalcIdealOverviewPosition( Vector vecStartPos, Vector vOldOverviewPos )
{
	Vector vecMinDist = vecStartPos + Vector( -1024.0f, -1024.0f, -256.0f );
	Vector vecMaxDist = vecStartPos + Vector( 1024.0f, 1024.0f, 256.0f );

	CBaseEntity	*pEntList[128];
	int count = UTIL_EntitiesInBox( pEntList, ARRAYSIZE(pEntList), vecMinDist, vecMaxDist, 0 );
	//CBaseEntity *pFarthestEnt = NULL;
	float flFarthestEntDist = 0;

	CBaseEntity *pFocusEnts[32];
	int focusCount = 0;

	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *pOther = pEntList[i];
		if ( dynamic_cast<C_CSPlayer*>(pOther) || dynamic_cast<C_PlantedC4*>(pOther) || dynamic_cast<CBaseCSGrenadeProjectile*>(pOther) )
		{
			pFocusEnts[focusCount] = pOther;
			focusCount++;

			float dist = VectorLength( vecStartPos - pOther->GetAbsOrigin() );
			if ( dist > flFarthestEntDist )
				flFarthestEntDist = dist;
		}
	}

	Vector vecAvPos = Vector( 0, 0, 0 );
	for ( int i = 0; i < focusCount; i++ )
	{
		vecAvPos += pFocusEnts[i]->GetAbsOrigin();
	}

	Vector vecNewPos = vecStartPos;
	if ( focusCount > 0 )
	{
		vecNewPos = (vecAvPos/focusCount);
	}

	float flDist = (vOldOverviewPos-vecNewPos).Length();
	float flDistScaler = clamp( 1+((flDist - 200) / 500), 1, 2 );

	m_flIdealOverviewScale = MAX( 0.1, (MIN( flFarthestEntDist, 1024 ) / 1024) ) * (1.5 * flDistScaler);

	return vecNewPos;
}

int C_HLTVCamera::GetMode()
{
	// hacky....
	if ( dynamic_cast< C_BaseCSGrenadeProjectile* >( GetPrimaryTarget() ) )
	{
		m_bIsFollowingGrenade = true;
		return OBS_MODE_CHASE;
	}

	if ( m_iCameraMan > 0 )
	{
		C_BasePlayer *pCameraMan = UTIL_PlayerByIndex( m_iCameraMan );

		if ( pCameraMan )
			return pCameraMan->GetObserverMode();
	}

	// to get here, our target is not a grenade, but we think we're still folowing one
 	if ( m_bIsFollowingGrenade == true )
 	{
 		if ( C_CSPlayer::GetLocalCSPlayer() )	
 		{
 			// if we're the cameraman and we're holding the shift key after a grenade has exired, 
 			// keep the camera where it is
 			bool bHoldingGrenadeKey = C_CSPlayer::GetLocalCSPlayer()->IsHoldingSpecGrenadeKey();
 			if ( bHoldingGrenadeKey )
 				return OBS_MODE_ROAMING;
 
 			// otherwise, we're not following a grenade anymore
 			m_bIsFollowingGrenade = false;
 		}
 	}

	return m_nCameraMode;	
}

C_BaseEntity* C_HLTVCamera::GetPrimaryTarget()
{
	if ( m_iCameraMan > 0 )
	{
		C_BasePlayer *pCameraMan = UTIL_PlayerByIndex( m_iCameraMan );
		
		if ( pCameraMan )
		{
			return pCameraMan->GetObserverTarget();
		}
	}

	if ( m_iTarget1 <= 0 )
	{
		return NULL;
	}

	C_BaseEntity* target = ClientEntityList().GetEnt( m_iTarget1 );

	if ( !target || (m_bIsFollowingGrenade && dynamic_cast< CBaseCSGrenadeProjectile* >( target ) == NULL) )
	{
		C_BaseEntity* oldTarget = ClientEntityList().GetEnt( m_iLastTarget1 );
		if ( oldTarget )
		{
			target = oldTarget;
			m_iTarget1 = m_iLastTarget1;
		}
	}

	return target;
}

C_BasePlayer *C_HLTVCamera::GetCameraMan()
{
	return m_iCameraMan ? UTIL_PlayerByIndex( m_iCameraMan ): NULL;
}

void C_HLTVCamera::CalcInEyeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	C_BasePlayer *pPlayer = dynamic_cast<C_CSPlayer*>(GetPrimaryTarget());

	if ( !pPlayer )
		return;

	if ( !pPlayer->IsAlive() )
	{
		// if dead, show from 3rd person
		C_CSPlayer *pCSPlayer =	static_cast<C_CSPlayer*>( pPlayer );
		if ( pCSPlayer && pCSPlayer->GetLastKillerIndex() )
			m_iTarget2 = pCSPlayer->GetLastKillerIndex();

		CalcChaseCamView( eyeOrigin, eyeAngles, fov );
		return;
	}

	m_aCamAngle	= pPlayer->EyeAngles();
	m_vCamOrigin = pPlayer->GetAbsOrigin();
	m_flFOV = pPlayer->GetFOV();

	// Apply punch angle
	VectorAdd( m_aCamAngle, pPlayer->GetViewPunchAngle(), m_aCamAngle );

	// Apply aim punch angle
	VectorAdd( m_aCamAngle, pPlayer->GetAimPunchAngle() * view_recoil_tracking.GetFloat(), m_aCamAngle );

	// Shake it up baby!
	GetViewEffects()->CalcShake();
	GetViewEffects()->ApplyShake( m_vCamOrigin, m_aCamAngle, 1.0 );

	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		m_vCamOrigin += VEC_DUCK_VIEW;
	}
	else
	{
		m_vCamOrigin += VEC_VIEW;
	}

	eyeOrigin = m_vCamOrigin;
	eyeAngles = m_aCamAngle;
	fov = m_flFOV;

	pPlayer->CalcViewModelView( eyeOrigin, eyeAngles);

	// Update view model visibility
	for ( int i = 0; i < MAX_VIEWMODELS; i++ )
	{
		CBaseViewModel *vm = pPlayer->GetViewModel( i );
		if ( !vm )
			continue;
		vm->UpdateVisibility();
	}
}

void C_HLTVCamera::CalcChaseOverview( CViewSetup &pSetup )
{
	C_BasePlayer *pPlayer = UTIL_PlayerByIndex( m_iTarget1 );

	if ( !pPlayer )
		return;

	m_aCamAngle	= QAngle( 90, 90, 0 );//pPlayer->EyeAngles();
	float flTransTime = 0.5f;

	if ( m_flNextIdealOverviewPosUpdate < gpGlobals->curtime )
	{
		m_vOldOverviewPos = m_vCamOrigin;
		Vector vNewIdealPos = CalcIdealOverviewPosition( pPlayer->GetAbsOrigin(), m_vOldOverviewPos ) + Vector( 0, 0, 128 );

		float flDist = (m_vOldOverviewPos-vNewIdealPos).Length();
		if ( flDist > 200.0f )
		{
			m_vIdealOverviewPos.x = vNewIdealPos.x;
			m_vIdealOverviewPos.y = vNewIdealPos.y;

			float flCamZDist = abs(m_flLastCamZPos - m_vIdealOverviewPos.z);
			if ( m_flLastCamZPos == 0 || flCamZDist > 64 )
			{
				trace_t	tr;
				UTIL_TraceLine( pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin() + Vector( 0, 0, 512 ), MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &tr );
				m_vIdealOverviewPos.z = MAX( tr.endpos.z - 32.0f, pPlayer->GetAbsOrigin().z + 80.0f );
			}
		}

		m_flNextIdealOverviewPosUpdate = gpGlobals->curtime + flTransTime;
	}

	float flInterp = clamp( ( gpGlobals->curtime - (m_flNextIdealOverviewPosUpdate - flTransTime) ) / flTransTime, 0.0f, 1.0f );
	float flActualinterp = Gain( flInterp, 0.6 );
	m_vCamOrigin = m_vOldOverviewPos + ((m_vIdealOverviewPos-m_vOldOverviewPos) * flActualinterp);

	m_flFOV = 180.0f;

	pSetup.origin = m_vCamOrigin;
	pSetup.angles = m_aCamAngle;
	pSetup.fov = m_flFOV;
}

void C_HLTVCamera::Accelerate( Vector& wishdir, float wishspeed, float accel )
{
	float addspeed, accelspeed, currentspeed;

	// See if we are changing direction a bit
	currentspeed =m_vecVelocity.Dot(wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of acceleration.
	accelspeed = accel * gpGlobals->frametime * wishspeed;

	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust velocity.
	for (int i=0 ; i<3 ; i++)
	{
		m_vecVelocity[i] += accelspeed * wishdir[i];	
	}
}

extern ConVar fov_cs_debug; 
// movement code is a copy of CGameMovement::FullNoClipMove()
void C_HLTVCamera::CalcRoamingView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	if ( !CSGameRules() )
		return;

	if ( m_bIsSpecLerping )
	{
		eyeOrigin = m_vCamOrigin;
		eyeAngles = m_aCamAngle;
		//fov = m_flFOV;
		fov = fov_cs_debug.GetInt() > 0 ? m_flFOV : CSGameRules()->DefaultFOV();

		if ( (m_vecSpecLerpIdealPos == m_vCamOrigin && m_angSpecLerpIdealAng == m_aCamAngle) || m_flSpecLerpEndTime <= gpGlobals->curtime )
		{
			m_bIsSpecLerping = false;
			SpecCameraGotoPos( m_vecSpecLerpIdealPos, m_angSpecLerpIdealAng );
		}
		else
		{
			float flTransTime = m_flSpecLerpTime;
			float flInterp = clamp( ( gpGlobals->curtime - (m_flSpecLerpEndTime - flTransTime) ) / flTransTime, 0.0f, 1.0f );
			float flActualinterp = Gain( flInterp, 0.6 );
			Vector vCamOrigin = m_vecSpecLerpOldPos + ((m_vecSpecLerpIdealPos-m_vecSpecLerpOldPos) * flActualinterp);
			QAngle aCamAngles = Lerp( flActualinterp, m_angSpecLerpOldAng, m_angSpecLerpIdealAng );//m_angSpecLerpOldAng + ((m_angSpecLerpIdealAng-m_angSpecLerpOldAng) * flActualinterp);
			SpecCameraGotoPos( vCamOrigin, aCamAngles );
		}
		return;
	}

	// only if PVS isn't locked by auto-director
	if ( !IsPVSLocked() )
	{
		Vector wishvel;
		Vector forward, right, up;
		Vector wishdir;
		float wishspeed;
		float factor = sv_specspeed.GetFloat();
		float maxspeed = sv_maxspeed.GetFloat() * factor;

		AngleVectors ( m_LastCmd.viewangles, &forward, &right, &up);  // Determine movement angles

		if ( m_LastCmd.buttons & IN_SPEED )
		{
			factor /= 2.0f;
		}

		// Copy movement amounts
		float fmove = m_LastCmd.forwardmove * factor;
		float smove = m_LastCmd.sidemove * factor;

		VectorNormalize (forward);  // Normalize remainder of vectors
		VectorNormalize (right);    // 

		for (int i=0 ; i<3 ; i++)       // Determine x and y parts of velocity
			wishvel[i] = forward[i]*fmove + right[i]*smove;
		wishvel[2] += m_LastCmd.upmove * factor;

		VectorCopy (wishvel, wishdir);   // Determine magnitude of speed of move
		wishspeed = VectorNormalize(wishdir);

		//
		// Clamp to server defined max speed
		//
		if (wishspeed > maxspeed )
		{
			VectorScale (wishvel, maxspeed/wishspeed, wishvel);
			wishspeed = maxspeed;
		}

		if ( sv_specaccelerate.GetFloat() > 0.0 )
		{
			// Set move velocity
			Accelerate ( wishdir, wishspeed, sv_specaccelerate.GetFloat() );

			float spd = VectorLength( m_vecVelocity );
			if (spd < 1.0f)
			{
				m_vecVelocity.Init();
			}
			else
			{
				// Bleed off some speed, but if we have less than the bleed
				//  threshold, bleed the threshold amount.
				float control = (spd < maxspeed/4.0) ? maxspeed/4.0 : spd;

				float friction = sv_friction.GetFloat();

				// Add the amount to the drop amount.
				float drop = control * friction * gpGlobals->frametime;

				// scale the velocity
				float newspeed = spd - drop;
				if (newspeed < 0)
					newspeed = 0;

				// Determine proportion of old speed we are using.
				newspeed /= spd;
				VectorScale( m_vecVelocity, newspeed, m_vecVelocity );
			}
		}
		else
		{
			VectorCopy( wishvel, m_vecVelocity );
		}

		// Just move ( don't clip or anything )
		VectorMA( m_vCamOrigin, gpGlobals->frametime, m_vecVelocity, m_vCamOrigin );
		
		// get camera angle directly from engine
		 engine->GetViewAngles( m_aCamAngle );

		// Zero out velocity if in noaccel mode
		if ( sv_specaccelerate.GetFloat() < 0.0f )
		{
			m_vecVelocity.Init();
		}
	}

	eyeOrigin = m_vCamOrigin;
	eyeAngles = m_aCamAngle;
	//fov = m_flFOV;
	fov = fov_cs_debug.GetInt() > 0 ? m_flFOV : CSGameRules()->DefaultFOV();
}

void C_HLTVCamera::CalcFixedView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	eyeOrigin = m_vCamOrigin;
	eyeAngles = m_aCamAngle;
	fov = m_flFOV;

	if ( m_bIsSpecLerping )
	{
		if ( (m_vecSpecLerpIdealPos == m_vCamOrigin && m_angSpecLerpIdealAng == m_aCamAngle) || m_flSpecLerpEndTime <= gpGlobals->curtime )
		{
			m_bIsSpecLerping = false;
			SpecCameraGotoPos( m_vecSpecLerpIdealPos, m_angSpecLerpIdealAng );
		}
		else
		{
			float flTransTime = m_flSpecLerpTime;
			float flInterp = clamp( ( gpGlobals->curtime - (m_flSpecLerpEndTime - flTransTime) ) / flTransTime, 0.0f, 1.0f );
			float flActualinterp = Gain( flInterp, 0.6 );
			Vector vCamOrigin = m_vecSpecLerpOldPos + ((m_vecSpecLerpIdealPos-m_vecSpecLerpOldPos) * flActualinterp);
			QAngle aCamAngles = Lerp( flActualinterp, m_angSpecLerpOldAng, m_angSpecLerpIdealAng );//m_angSpecLerpOldAng + ((m_angSpecLerpIdealAng-m_angSpecLerpOldAng) * flActualinterp);
			SpecCameraGotoPos( vCamOrigin, aCamAngles );
		}
	}
	else
	{
		int nTarget = m_iTarget1;
		if ( m_iTarget1 == 0 && m_iLastTarget1 == 0 )
			return;

		int nButtonBits = input->GetButtonBits( false );

		if (m_iTarget1 == 0 && !(nButtonBits & IN_FORWARD) )
			return;

		if ( (nButtonBits & IN_FORWARD) )
			nTarget = m_iLastTarget1;

		C_BaseEntity * target = ClientEntityList().GetBaseEntity( nTarget );

		if ( target && target->IsAlive() )
		{
			// if we're chasing a target, change viewangles
			QAngle angle;
			VectorAngles( (target->GetAbsOrigin()+VEC_VIEW) - m_vCamOrigin, angle );
			SmoothCameraAngle( angle );
		}
	}
}

void C_HLTVCamera::PostEntityPacketReceived()
{
	m_bEntityPacketReceived = true;
}

void C_HLTVCamera::FixupMovmentParents()
{
	// Find resource zone
	
	for (	ClientEntityHandle_t e = ClientEntityList().FirstHandle();
			e != ClientEntityList().InvalidHandle(); e = ClientEntityList().NextHandle( e ) )
	{
		C_BaseEntity *ent = C_BaseEntity::Instance( e );

		if ( !ent )
			continue;

		ent->HierarchyUpdateMoveParent();
	}
}

void C_HLTVCamera::CalcView(CViewSetup *pSetup)
{
	//CViewSetup *pSetup

	if ( m_bEntityPacketReceived )
	{
		// try to fixup movment pareents
		FixupMovmentParents();
		m_bEntityPacketReceived = false;
	}

	if ( m_iCameraMan > 0 )
	{
		C_BasePlayer *pCameraMan = UTIL_PlayerByIndex( m_iCameraMan );
		if ( pCameraMan )
		{
			//float zNear,zFar;
			pCameraMan->CalcView( pSetup->origin, pSetup->angles, pSetup->zNear, pSetup->zFar, pSetup->fov );
			pCameraMan->CalcViewModelView( pSetup->origin, pSetup->angles );
			return;
		}
	}

	if ( input->CAM_IsThirdPersonOverview() )
	{
		CalcChaseOverview( *pSetup );
	}
	else
	{
		switch ( GetMode() )
		{
		case OBS_MODE_ROAMING	:	CalcRoamingView( pSetup->origin, pSetup->angles, pSetup->fov );
			break;

		case OBS_MODE_FIXED		:	CalcFixedView( pSetup->origin, pSetup->angles, pSetup->fov );
			break;

		case OBS_MODE_IN_EYE	:	CalcInEyeCamView( pSetup->origin, pSetup->angles, pSetup->fov );
			break;

		case OBS_MODE_CHASE		:	CalcChaseCamView( pSetup->origin, pSetup->angles, pSetup->fov  );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetup - 
//-----------------------------------------------------------------------------
// void C_HLTVCamera::CalcOverview( CViewSetup *pSetup )
// {
// 	QAngle camAngles;
// 
// 	// Let the player override the view.
// 	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
// 	if(!pPlayer)
// 		return;
// 
// 	pPlayer->OverrideView( pSetup );
// 
// 	if( false/*::input->CAM_IsThirdPerson()*/ )
// 	{
// 		Vector cam_ofs;
// 
// 		::input->CAM_GetCameraOffset( cam_ofs );
// 
// 		camAngles[ PITCH ] = cam_ofs[ PITCH ];
// 		camAngles[ YAW ] = cam_ofs[ YAW ];
// 		camAngles[ ROLL ] = 0;
// 
// 		Vector camForward, camRight, camUp;
// 		AngleVectors( camAngles, &camForward, &camRight, &camUp );
// 
// 		pSetup->origin = pPlayer->GetThirdPersonViewPosition();
// 
// 		VectorMA( pSetup->origin, -cam_ofs[ ROLL ], camForward, pSetup->origin );
// 
// 		static ConVarRef c_thirdpersonshoulder( "c_thirdpersonshoulder" );
// 		if ( c_thirdpersonshoulder.GetBool() )
// 		{
// 			static ConVarRef c_thirdpersonshoulderoffset( "c_thirdpersonshoulderoffset" );
// 			static ConVarRef c_thirdpersonshoulderheight( "c_thirdpersonshoulderheight" );
// 			static ConVarRef c_thirdpersonshoulderaimdist( "c_thirdpersonshoulderaimdist" );
// 
// 			// add the shoulder offset to the origin in the cameras right vector
// 			VectorMA( pSetup->origin, c_thirdpersonshoulderoffset.GetFloat(), camRight, pSetup->origin );
// 
// 			// add the shoulder height to the origin in the cameras up vector
// 			VectorMA( pSetup->origin, c_thirdpersonshoulderheight.GetFloat(), camUp, pSetup->origin );
// 
// 			// adjust the yaw to the aim-point
// 			camAngles[ YAW ] += RAD2DEG( atan(c_thirdpersonshoulderoffset.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );
// 
// 			// adjust the pitch to the aim-point
// 			camAngles[ PITCH ] += RAD2DEG( atan(c_thirdpersonshoulderheight.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );
// 		}
// 
// 		// Override angles from third person camera
// 		VectorCopy( camAngles, pSetup->angles );
// 	}
// 	else if ( true /*::input->CAM_IsOrthographic()*/)
// 	{
// 		pSetup->m_bOrtho = true;
// 		float w, h;
// 		::input->CAM_OrthographicSize( w, h );
// 		w *= 0.5f;
// 		h *= 0.5f;
// 		pSetup->m_OrthoLeft   = -w;
// 		pSetup->m_OrthoTop    = -h;
// 		pSetup->m_OrthoRight  = w;
// 		pSetup->m_OrthoBottom = h;
// 	}
// }

void C_HLTVCamera::SetMode(int iMode)
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			iMode = OBS_MODE_IN_EYE;
		}
	}

	if ( m_nCameraMode == iMode )
		return;

    Assert( iMode > OBS_MODE_NONE && iMode <= LAST_PLAYER_OBSERVERMODE );

	int iOldMode = m_nCameraMode;
	m_nCameraMode = iMode;

	if ( m_nCameraMode == OBS_MODE_IN_EYE || m_nCameraMode == OBS_MODE_CHASE )
		m_bIsSpecLerping = false;

	IGameEvent *event = gameeventmanager->CreateEvent( "hltv_changed_mode" );
	if ( event )
	{
		event->SetInt( "oldmode", iOldMode );
		event->SetInt( "newmode", m_nCameraMode );
		event->SetInt( "obs_target", m_iTarget1 );
		gameeventmanager->FireEventClientSide( event );
	}

	// tell the target player to update the visibility of their view and world models
	CBaseEntity * target = UTIL_PlayerByIndex( m_iTarget1 );
	if ( target && target->IsPlayer() )
	{
		CBasePlayer * player = ToBasePlayer( target );
		if ( player )
			player->OnObserverModeChange( true );
	}

}

void C_HLTVCamera::SetPrimaryTarget( int nEntity ) 
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			// Make sure that the requested target is the player account
			// that PVS must be locked to
			bool bFoundValidPlayer = false;
			for ( int idxEntity = 1; idxEntity <= gpGlobals->maxClients; ++ idxEntity )
			{
				CBaseEntity * target = UTIL_PlayerByIndex( idxEntity );

				if ( target == NULL )
					continue;

				if ( !target->IsPlayer() )
					continue;

				CBasePlayer * player = ToBasePlayer( target );
				if ( !player )
					continue;

				CSteamID steamID;
				if ( player->GetSteamID( &steamID ) && steamID.IsValid() &&
					( steamID.GetAccountID() == pParameters->m_uiLockFirstPersonAccountID ) )
				{
					// when playback wants to lock to a specific account force PVS lock
					// regardless of player states to see exactly what that player was seeing
					bFoundValidPlayer = true;
					nEntity = idxEntity; // force the only entity allowed
					break;
				}
			}
			if ( !bFoundValidPlayer )
				return;
		}
	}

	if ( m_iTarget1 == nEntity )
		return;

	m_iLastTarget1 = m_iTarget1;

	m_iTarget1 = nEntity;

#if defined ( CSTRIKE15 )
	// BUG: This uses the values (mode, target, etc) of the local player, not
	// the hltv camera... These happen to match so it works, but could be the source
	// of bugs... Could turn the observer lerp code into it's own class and have hltv/replay/csplayer 
	// have an instance. 
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer && pLocalPlayer->ShouldInterpolateObserverChanges() )
	{
		pLocalPlayer->StartObserverInterpolation( m_aCamAngle );
	}

#endif

	if ( GetMode() == OBS_MODE_ROAMING )
	{
		Vector vOrigin;
		QAngle aAngles;
		float flFov;

		CalcChaseCamView( vOrigin,  aAngles, flFov );
	}
	else if ( GetMode() == OBS_MODE_CHASE )
	{
		C_BaseEntity* target = ClientEntityList().GetEnt( m_iTarget1 );
		if ( target && target->IsPlayer() )
		{
			QAngle eyeAngle = target->EyeAngles();
			prediction->SetViewAngles( eyeAngle );
		}
	}

	C_BasePlayer *pOldPlayer = ToBasePlayer( ClientEntityList().GetEnt( m_iLastTarget1 ) );
	if ( pOldPlayer )
	{
		// Update view model visibility
		for ( int i = 0; i < MAX_VIEWMODELS; i++ )
		{
			CBaseViewModel *vm = pOldPlayer->GetViewModel( i );
			if ( !vm )
				continue;
			vm->UpdateVisibility();
		}
	}

	m_flLastDistance = m_flDistance;
	m_flLastAngleUpdateTime = -1;

	IGameEvent *event = gameeventmanager->CreateEvent( "hltv_changed_target" );
	if ( event )
	{
		event->SetInt("userid", pLocalPlayer->GetUserID() );
		event->SetInt( "mode", m_nCameraMode );
		event->SetInt( "old_target", m_iLastTarget1 );
		event->SetInt( "obs_target", m_iTarget1 );
		gameeventmanager->FireEventClientSide( event );
	}
}

void C_HLTVCamera::SpecNextPlayer( bool bReverse )
{

	// Copy of GetNextObserverSearchStartPoint
	int iDir = bReverse ? -1 : 1; 

	int StartIndex = 1;

	if ( m_iTarget1 > 0 && m_iTarget1 <= gpGlobals->maxClients )
		StartIndex = m_iTarget1;
	// end copy

	int Index = StartIndex;
	bool bFoundValidPlayer = false;

	// copy of FindNextObserverTarget
	do
	{
		Index += iDir;

		// Loop through the clients
		if (Index > gpGlobals->maxClients)
			Index = 1;
		else if (Index < 1)
			Index = gpGlobals->maxClients;

		CBaseEntity * target = UTIL_PlayerByIndex( Index );

		if ( target == NULL )
			continue;

		if ( !target->IsPlayer() )
			continue;

		CBasePlayer * player = ToBasePlayer( target );

		if ( player->IsEffectActive( EF_NODRAW ) ) // don't watch invisible players
			continue;

		if ( player->m_lifeState == LIFE_RESPAWNABLE ) // target is dead, waiting for respawn
			continue;

		if ( player->m_lifeState == LIFE_DEAD )
			continue;

		// for HLTV replay, them local player is both IsAlive(), and also is an observer because it's HLTV. Checking for IsObserver here seems to be redundant because the dead or specating players are already filtered out
		//if ( player->IsObserver() )
		//	continue;

		if ( player->GetTeamNumber() == TEAM_SPECTATOR )
			continue;

		bFoundValidPlayer = true;
		break;	// found next valid player

	} while ( Index != StartIndex );
	// end copy

	if ( bFoundValidPlayer )
	{
		SetPrimaryTarget( Index );

		if ( AutoDirectorState() != AUTODIRECTOR_OFF )
		   SetAutoDirector( AUTODIRECTOR_OFF ); // SetAutoDirector( AUTODIRECTOR_PAUSED );
	}
}

void C_HLTVCamera::SpecPlayerByAccountID( const char *pszSteamID )
{
	for ( int index = 1; index <= gpGlobals->maxClients; ++index )
	{
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( index );

		if ( !pPlayer )
			continue;

		CSteamID steamID;
		pPlayer->GetSteamID( &steamID );

		if ( CSteamID( pszSteamID ).GetAccountID() != steamID.GetAccountID( ) )
			continue;

		// only follow living players or dedicated spectators
		if ( pPlayer->IsObserver() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			continue;

		SetPrimaryTarget( index );

		if ( AutoDirectorState() != AUTODIRECTOR_OFF )
		   SetAutoDirector( AUTODIRECTOR_OFF ); // SetAutoDirector( AUTODIRECTOR_PAUSED );

		return;
	}
}


void C_HLTVCamera::SpecNamedPlayer( const char *szPlayerName )
{
	for ( int index = 1; index <= gpGlobals->maxClients; ++index )
	{
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( index );

		if ( !pPlayer )
			continue;

		if ( !FStrEq( szPlayerName, pPlayer->GetPlayerName( ) ) )
			continue;

		// only follow living players or dedicated spectators
		if ( pPlayer->IsObserver( ) && pPlayer->GetTeamNumber( ) != TEAM_SPECTATOR )
			continue;

		SetPrimaryTarget( index );

		if ( AutoDirectorState( ) != AUTODIRECTOR_OFF )
			SetAutoDirector( AUTODIRECTOR_OFF ); // SetAutoDirector( AUTODIRECTOR_PAUSED );

		return;
	}
}

void C_HLTVCamera::SpecPlayerByIndex( int iIndex )
{
	C_BasePlayer *pPlayer =	UTIL_PlayerByIndex( iIndex );

	if ( !pPlayer )
		return;

	if ( g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		// In HLTV Replay case, just follow the living players explicitly
		if ( !pPlayer->IsAlive() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			return;
	}
	else
	{
		// only follow living players or dedicated spectators. TODO: Fold in the HLTV Replay case; just follow the living players explicitly
		if ( pPlayer->IsObserver() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			return;
	}

	if ( GetMode() == OBS_MODE_ROAMING || GetMode() == OBS_MODE_FIXED )
		SetMode( OBS_MODE_IN_EYE );

	SetPrimaryTarget( iIndex );

	if ( AutoDirectorState() != AUTODIRECTOR_OFF )
	   SetAutoDirector( AUTODIRECTOR_OFF ); // SetAutoDirector( AUTODIRECTOR_PAUSED );

	return;
}


void C_HLTVCamera::Update()
{
	bool bFastForwardNotification = false;

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		int numFastForwardTicks = 0;

		// Get the team score
		if ( CSGameRules() )
		{
			C_Team *tTeam = GetGlobalTeam( TEAM_TERRORIST );
			C_Team *ctTeam = GetGlobalTeam( TEAM_CT );

			int numRoundsCompleted = ( tTeam ? tTeam->Get_Score() : 0 ) + ( ctTeam ? ctTeam->Get_Score() : 0 );
			if ( ( pParameters->m_numRoundStop != ~0 ) && ( numRoundsCompleted >= ( int ) pParameters->m_numRoundStop ) )
			{
				// End of fraction is now reached, just end the playback
				if ( g_pMatchFramework )
				{
					g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnDemoFileEndReached" ) );
					g_pMatchFramework->CloseSession();
				}
				return;
			}
		}
		
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			// Skip portions of playback that aren't locking view to the requested account ID
			bool bLockedToRequestedAccount = false;
			for ( int iLockAttempt = 0; iLockAttempt < 2; ++ iLockAttempt )
			{
				if ( m_iTarget1 >= 1 && m_iTarget1 < gpGlobals->maxClients )
				{
					CBaseEntity * target = UTIL_PlayerByIndex( m_iTarget1 );
					if ( target && target->IsPlayer() )
					{
						CBasePlayer * player = ToBasePlayer( target );
						CSteamID steamID;
						if ( player && player->GetSteamID( &steamID ) && steamID.IsValid() &&
							( steamID.GetAccountID() == pParameters->m_uiLockFirstPersonAccountID ) )
						{
							// Check if the player that we are locked to is doing non-interesting stuff
							if ( player->IsEffectActive( EF_NODRAW ) || // don't watch invisible players
								( player->m_lifeState == LIFE_RESPAWNABLE ) || // target is dead, waiting for respawn
								( player->m_lifeState == LIFE_DEAD ) ||
								( player->GetFlags() & FL_FROZEN ) || // skip freezetime
								( player->IsObserver() ) ||
								( player->GetTeamNumber() == TEAM_SPECTATOR ) ||
								( CSGameRules() && CSGameRules()->IsFreezePeriod() ) )
								break; // break to skip some ticks

							bLockedToRequestedAccount = true;
						}
					}
				}

				// If we aren't locked then try locking now and check again
				if ( bLockedToRequestedAccount )
					break;
				// Try locking and loop one more time
				if ( !iLockAttempt )
					SetPrimaryTarget( 0 );
			}

			if ( !bLockedToRequestedAccount && ( spec_overwatch_skip_idle_ticks.GetInt() > numFastForwardTicks ) && pParameters->m_bAnonymousPlayerIdentity )
			{
				numFastForwardTicks = spec_overwatch_skip_idle_ticks.GetInt();
			}
		}

		if ( ( numFastForwardTicks > 1 ) && ( engine->GetDemoPlaybackTimeScale() >= 0.999f ) )
		{
			// Skip some ticks if demo playback is not being slowed down by the user
			engine->ClientCmd_Unrestricted( CFmtStr( "demo_gototick %u R;\n", numFastForwardTicks ) );
			bFastForwardNotification = true;
		}
	}

	if ( m_flAutodirectorPausedTime >= 0.0)
	{
		const float endTime = spec_autodirector_pausetime.GetFloat() + m_flAutodirectorPausedTime;
		if ( gpGlobals->curtime >= endTime)
		{
			HLTVCamera()->SetAutoDirector( AUTODIRECTOR_ON );
			m_flAutodirectorPausedTime = -1.0f;
		}
	}

	loadingdisc->SetFastForwardVisible( bFastForwardNotification );
}

void C_HLTVCamera::SpecCameraGotoPos( Vector vecPos, QAngle angAngle, int nPlayerIndex )
{
	m_iCameraMan  = 0;
	m_iLastTarget1 = (nPlayerIndex > 0) ? nPlayerIndex : (GetPrimaryTarget() ? GetPrimaryTarget()->entindex() : m_iLastTarget1);
	SetPrimaryTarget( 0 );

	QAngle oldang = m_aCamAngle;
	QAngle newang;
	newang.x = angAngle[0];
	newang.y = angAngle[1];
	newang.z = oldang.z;

	m_vCamOrigin = vecPos;

	SetMode( OBS_MODE_ROAMING );
	SetCameraAngle( newang );
	engine->SetViewAngles( newang );
	m_flFOV = 90.0f;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	IGameEvent *event = gameeventmanager->CreateEvent( "spec_target_updated" );
	if ( pPlayer && event )
	{
		event->SetInt("userid", pPlayer->GetUserID() );
		gameeventmanager->FireEventClientSide( event );
	}
}

void C_HLTVCamera::SpecCameraLerptoPos( const Vector &origin, const QAngle &angles, int nPlayerIndex, float flTime )
{
	m_iCameraMan  = 0;
	m_iLastTarget1 = (nPlayerIndex > 0) ? nPlayerIndex : (GetPrimaryTarget() ? GetPrimaryTarget()->entindex() : m_iLastTarget1);
	SetPrimaryTarget( 0 );
	SetMode( OBS_MODE_ROAMING );
	m_flFOV = 90.0f;

	QAngle oldang = m_aCamAngle;
	QAngle newang;
	newang.x = angles[0];
	newang.y = angles[1];
	newang.z = oldang.z;

	m_bIsSpecLerping = true;

	m_vecSpecLerpOldPos = m_vCamOrigin;
	m_angSpecLerpOldAng = m_aCamAngle;

	m_vecSpecLerpIdealPos = origin;
	m_angSpecLerpIdealAng = angles;

	m_flSpecLerpTime = flTime;
	m_flSpecLerpEndTime = gpGlobals->curtime + flTime;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	IGameEvent *event = gameeventmanager->CreateEvent( "spec_target_updated" );
	if ( pPlayer && event )
	{
		event->SetInt("userid", pPlayer->GetUserID() );
		gameeventmanager->FireEventClientSide( event );
	}
}

void C_HLTVCamera::FireGameEvent( IGameEvent * event)
{
	const char *type = event->GetName();

	if ( !engine->IsHLTV() )
	{
#ifdef CAMERAMAN_OLD_WAY
		if ( Q_strcmp( "hltv_cameraman", type ) == 0 )
		{
			m_iCameraMan = event->GetInt( "index" ); 
		}
#endif
		return;	// not in HLTV mode
	}

	// in HLTV mode
	if ( Q_strcmp( "game_newmap", type ) == 0 )
	{
		Reset();	// reset all camera settings

		// show spectator UI
		if ( !GetViewPortInterface() )
			return;

		if ( !engine->IsPlayingDemo() )
        {
			// during live broadcast only show black bars
			GetViewPortInterface()->ShowPanel( PANEL_SPECGUI, true );
		}

		SetMode( OBS_MODE_ROAMING );
		SetAutoDirector( C_HLTVCamera::AUTODIRECTOR_ON );

		return;
	}

	if ( Q_strcmp( "hltv_message", type ) == 0 )
	{
		wchar_t outputBuf[1024];
		const char *pszText = event->GetString( "text", "" );
		
		char *tmpStr = hudtextmessage->LookupString( pszText );
		const wchar_t *pBuf = g_pVGuiLocalize->Find( tmpStr );
		if ( pBuf )
		{
			// Copy pBuf into szBuf[i].
			int nMaxChars = sizeof( outputBuf ) / sizeof( wchar_t );
			wcsncpy( outputBuf, pBuf, nMaxChars );
			outputBuf[nMaxChars-1] = 0;
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( tmpStr, outputBuf, sizeof(outputBuf) );
		}

		GetCenterPrint()->Print( ConvertCRtoNL( outputBuf ) );
		return ;
	}

	if ( Q_strcmp( "hltv_title", type ) == 0 )
	{
		Q_strncpy( m_szTitleText, event->GetString( "text", "" ), sizeof(m_szTitleText) );
		return;
	}

	if ( Q_strcmp( "hltv_status", type ) == 0 )
	{
		int nNumProxies = event->GetInt( "proxies" );
		m_nNumSpectators = event->GetInt( "clients" ) - nNumProxies;
		return;
	}

	// after this only auto-director commands follow
	// don't execute them if the autodirector is off and PVS is unlocked
	//( hltv player is controlling the camera )
	if ( !spec_autodirector.GetBool() && !IsPVSLocked() )
		return;
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			// Whenever a player connects we will force lookup of the required player
			if ( !Q_strcmp( "player_connect", type ) ||
				!Q_strcmp( "player_connect_full", type ) ||
				!Q_strcmp( "player_team", type ) )
			{
				SetPrimaryTarget( 0 );
				return;
			}

			return;	// don't execute further commands if locked to a specific account forced PVS
		}
	}

	if ( m_iCameraMan > 0 )
	{
		return;
	}

#ifdef CAMERAMAN_OLD_WAY
	if ( Q_strcmp( "hltv_cameraman", type ) == 0 )
	{
		Reset();

		m_nCameraMode = OBS_MODE_ROAMING;
		m_iCameraMan = event->GetInt( "index" ); 

		return;
	}
#endif

	if ( Q_strcmp( "hltv_fixed", type ) == 0 )
	{
		m_iCameraMan  = 0;
		
		m_vCamOrigin.x = event->GetInt( "posx" );
		m_vCamOrigin.y = event->GetInt( "posy" );
		m_vCamOrigin.z = event->GetInt( "posz" );

		QAngle angle;
 		angle.x = event->GetInt( "theta" );
		angle.y = event->GetInt( "phi" );
		angle.z = 0; // no roll yet

		if ( m_nCameraMode != OBS_MODE_FIXED )
		{
			SetMode( OBS_MODE_FIXED );
			SetCameraAngle( angle );
			m_flFOV = event->GetFloat( "fov", 90 );
		}

		SetPrimaryTarget( event->GetInt( "target" ) );

		if ( m_iTarget1 == 0 )
		{
			SetCameraAngle( angle );
		}
						
		return;
	}

	if ( Q_strcmp( "hltv_chase", type ) == 0 )
	{
		bool bInEye	= event->GetBool( "ineye" );

		// check if we are already in a player chase mode
		bool bIsInChaseMode = (GetMode()==OBS_MODE_IN_EYE)|| (GetMode()==OBS_MODE_CHASE);

		// if we are in auto director or not in a valid chase mode, set new mode now
		if ( spec_autodirector.GetBool() || !bIsInChaseMode )
		{
			SetMode( bInEye?OBS_MODE_IN_EYE:OBS_MODE_CHASE );
		}

		m_iCameraMan  = 0;
				
		m_iTarget2		= event->GetInt( "target2" );
		m_flDistance	= event->GetFloat( "distance", m_flDistance );
		m_flOffset		= event->GetFloat( "offset", m_flOffset );
		m_flTheta		= event->GetFloat( "theta", m_flTheta );
		m_flPhi			= event->GetFloat( "phi", m_flPhi );
		m_flFOV			= event->GetFloat( "fov", 90 );
 		m_flInertia		= event->GetFloat( "inertia", 30.f ) / 10.f;

		// if inertia is not set use standard value
		if ( m_flInertia <= 0 )
			m_flInertia = 3.0f;

		SetPrimaryTarget( event->GetInt( "target1" ) );
							
		return;
	}

}

// this is a cheap version of FullNoClipMove():
void C_HLTVCamera::CreateMove( CUserCmd *cmd)
{
	if ( cmd )
	{
		m_LastCmd = *cmd;
	}
}

void C_HLTVCamera::SetCameraAngle( QAngle& targetAngle )
{
	m_aCamAngle	= targetAngle;
 	NormalizeAngles( m_aCamAngle );
	m_flLastAngleUpdateTime = gpGlobals->realtime;
}

void C_HLTVCamera::SmoothCameraAngle( QAngle& targetAngle )
{
	if ( m_flLastAngleUpdateTime > 0 )
	{
		float deltaTime = gpGlobals->realtime - m_flLastAngleUpdateTime;

		deltaTime = clamp( deltaTime*m_flInertia, 0.01, 1);

		InterpolateAngles( m_aCamAngle, targetAngle, m_aCamAngle, deltaTime );
	}
	else
	{
		m_aCamAngle = targetAngle;
	}

	m_flLastAngleUpdateTime = gpGlobals->realtime;
}

void C_HLTVCamera::ToggleChaseAsFirstPerson()
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			if ( GetMode() != OBS_MODE_IN_EYE )
				SetMode( OBS_MODE_IN_EYE );
			SetPrimaryTarget( 0 ); // will force correct computation of the target
			return;	// when playback wants to lock to a specific account force PVS lock to IN_EYE
		}
	}

	if ( GetMode() == OBS_MODE_CHASE )
	{
		SetMode( OBS_MODE_IN_EYE );
	}
	else if ( GetMode() == OBS_MODE_IN_EYE )
	{
		SetMode( OBS_MODE_CHASE );
	}

	if ( AutoDirectorState() != AUTODIRECTOR_OFF )
	   SetAutoDirector( AUTODIRECTOR_OFF ); //SetAutoDirector( AUTODIRECTOR_PAUSED );
}

bool C_HLTVCamera::IsPVSLocked()
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
			return true;	// when playback wants to lock to a specific account force PVS lock
	}

	if ( tv_transmitall != NULL )
	{
		return !tv_transmitall->GetBool();
	}
	else
	{
		//old style, assume locked unless we playback a demo
		return !engine->IsPlayingDemo();
	}
}

void C_HLTVCamera::SetAutoDirector( AutodirectorState_t eState )
{
	//m_iCameraMan = 0;
	if ( eState != AUTODIRECTOR_OFF && spec_autodirector_cameraman.GetInt() > 0 )
	{
		// find a cameraman and set m_iCameraMan
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			CSteamID compareSteamID;
			if ( pPlayer && pPlayer->GetSteamID( &compareSteamID ) )
			{
				// is this played the selected cameraman
				if ( ( uint32 )( spec_autodirector_cameraman.GetInt() ) == compareSteamID.GetAccountID() )
				{
					// validate that they are a tournament caster
					for ( int j = 0; j < MAX_TOURNAMENT_ACTIVE_CASTER_COUNT; j++ )
					{
						if ( compareSteamID.GetAccountID() == CSGameRules()->m_arrTournamentActiveCasterAccounts[ j ] )
						{
							if ( pPlayer->IsActiveCameraMan() )
							{
								m_iCameraMan = i;

								C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
								CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );
								if ( hudChat && pLocalPlayer && !IsAutoDirectorOn() )
								{
									hudChat->ChatPrintfW( pLocalPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pVGuiLocalize->Find( "#CSGO_Scoreboard_CasterControl_Camera_On" ) );
								
									pLocalPlayer->EmitSound("Vote.Passed");		
								}
								break;
							}
						}
					}
				}
				if ( m_iCameraMan )
					break;
			}
		}
	}

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			eState = AUTODIRECTOR_ON; // force auto-director first person mode
			m_iCameraMan = 0; // force no cameraman
		}
	}

	if ( eState == AUTODIRECTOR_ON)
	{
		spec_autodirector.SetValue( 1 );
	}
	else // OFF or PAUSED
	{
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );
		if ( hudChat && pLocalPlayer && m_iCameraMan != 0 && IsAutoDirectorOn() )
		{
			hudChat->ChatPrintfW( pLocalPlayer->entindex(), CHAT_FILTER_SERVERMSG, g_pVGuiLocalize->Find( "#CSGO_Scoreboard_CasterControl_Camera_Off" ) );
			
			pLocalPlayer->EmitSound("UI.ButtonRolloverLarge");		
		}

		spec_autodirector.SetValue( 0 );
		m_iCameraMan = 0;
	}

	if  ( eState == AUTODIRECTOR_PAUSED )
	{
		m_flAutodirectorPausedTime = gpGlobals->curtime;
	}
	else
	{
		m_flAutodirectorPausedTime = -1.0f;
	}
		
}

C_HLTVCamera::AutodirectorState_t C_HLTVCamera::AutoDirectorState() const
{
	if ( IsAutoDirectorOn() )
		return AUTODIRECTOR_ON;

	if ( m_flAutodirectorPausedTime < 0.0f )
		return AUTODIRECTOR_OFF;
	else
		return AUTODIRECTOR_PAUSED;
}

bool C_HLTVCamera::IsAutoDirectorOn() const
{
	return spec_autodirector.GetBool();
}

#ifdef CSTRIKE_DLL
CON_COMMAND_F( list_active_casters, "List currently active casters.", FCVAR_CLIENTDLL | FCVAR_RELEASE |FCVAR_HIDDEN )
{
	if ( !CSGameRules() )
	{
		Msg( "You need to be watching a game!\n" );
		return;
	}

	Msg( "Active Casters:\n" );

	CSteamID cameraManSteamID;
	if ( CBasePlayer *pPlayer = HLTVCamera()->GetCameraMan() )
	{
		pPlayer->GetSteamID( &cameraManSteamID );
	}

	int nActiveCasters = 0;
	for (int i = 0; i < MAX_TOURNAMENT_ACTIVE_CASTER_COUNT; i++ )
	{
		if ( CSGameRules()->m_arrTournamentActiveCasterAccounts[ i ] )
		{
			nActiveCasters++;

			if ( steamapicontext->SteamUser() && steamapicontext->SteamFriends() )
			{
				CSteamID steamID( CSGameRules()->m_arrTournamentActiveCasterAccounts[ i ], steamapicontext->SteamUser()->GetSteamID().GetEUniverse(), k_EAccountTypeIndividual );
				const char *pszName = steamapicontext->SteamFriends()->GetFriendPersonaName( steamID );
				Msg( "%d, ID: %d Name: %s  %s\n", i, CSGameRules()->m_arrTournamentActiveCasterAccounts[ i ], pszName, 
					 ( cameraManSteamID.GetAccountID() == CSGameRules()->m_arrTournamentActiveCasterAccounts[ i ] ) ? "*Camera Man*" : "" );
			}
			else
			{
				Msg( "%d, ID: %d  %s\n", i, CSGameRules()->m_arrTournamentActiveCasterAccounts[ i ], 
					 ( cameraManSteamID.GetAccountID() == CSGameRules()->m_arrTournamentActiveCasterAccounts[ i ] ) ? "*Camera Man*" : "" );
			}
		}
	}
	if ( nActiveCasters == 0 )
	{
		Msg( "None.\n" );
	}
}
#endif

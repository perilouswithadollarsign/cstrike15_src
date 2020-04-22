//===== Copyright © Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "portal_grabcontroller_shared.h"
#include "portal_player_shared.h"
#include "portal_player_shared.h"
#include "vphysics/friction.h"
#include "collisionutils.h"

#if defined ( CLIENT_DLL )
#include "c_portal_player.h"
#include "prediction.h"
#include "c_breakableprop.h"
#include "c_npc_portal_turret_floor.h"
typedef C_NPC_Portal_FloorTurret CNPC_Portal_FloorTurret;
#else
#include "player_pickup.h"
#include "portal_player.h"
#include "props.h"
#include "physics_saverestore.h"
#include "weapon_portalgun.h"
#include "npc_portal_turret_floor.h"
#endif // CLIENT_DLL

bool TestIntersectionVsHeldObjectCollide( CBaseEntity *pHeldObject, Vector vHeldObjectTestOrigin, CBaseEntity *pOther );

ConVar g_debug_physcannon( "g_debug_physcannon", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar debug_viewmodel_grabcontroller( "debug_viewmodel_grabcontroller", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar physcannon_maxmass( "physcannon_maxmass", "250", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar hide_gun_when_holding( "hide_gun_when_holding", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
ConVar player_held_object_offset_up_cube( "player_held_object_offset_up_cube", "-10", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "offest along the up axis for held objects.", true, -100.0f, true, 100.0f );
ConVar player_held_object_offset_up_cube_vm( "player_held_object_offset_up_cube_vm", "-20", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "offest along the up axis for held objects.", true, -100.0f, true, 100.0f );
ConVar player_held_object_offset_up_sphere( "player_held_object_offset_up_sphere", "-15", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "offest along the up axis for held objects.", true, -100.0f, true, 100.0f );
ConVar player_held_object_look_down_adjustment( "player_held_object_look_down_adjustment", "20", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Moves the box forward when looking down (viewmodel held object only.", true, 0.0f, true, 100.0f );
ConVar player_held_object_distance( "player_held_object_distance", "15", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Distance from player for held objects.", true, 0.0f, true, 100.0f );
ConVar player_held_object_distance_vm( "player_held_object_distance_vm", "65", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Distance from player for held objects.", true, 0.0f, true, 160.0f );
ConVar player_held_object_offset_up_turret_vm( "player_held_object_offset_up_turret_vm", "-20", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Offset for held turrets", true, -100.0f, true, 100.0f );
ConVar player_held_object_distance_turret_vm( "player_held_object_distance_turret_vm", "100", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Offset for held turrets", true, 0.0f, true, 200.0f );
ConVar player_held_object_min_distance( "player_held_object_min_distance", "50", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Minimum distance from player for held objects (used by viewmodel held objects).", true, 0.0f, true, 100.0f );
ConVar player_hold_object_in_column( "player_hold_object_in_column", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Hold object along a fixed column in front of player\n" );
ConVar player_hold_column_max_size( "player_hold_column_max_size", "96", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Furthest distance an object can be when held in colmn mode." );
ConVar player_held_object_max_knock_magnitude( "player_held_object_max_knock_magnitude", "30", FCVAR_REPLICATED | FCVAR_CHEAT, "For viewmodel grab controller, max velocity magnitude squared to apply to knocked objects." );
ConVar player_held_object_max_throw_magnitude( "player_held_object_max_throw_magnitude", "60", FCVAR_REPLICATED | FCVAR_CHEAT, "For viewmodel grab controller, max velocity magnitude squared to apply to knocked objects." );

ConVar player_held_object_use_view_model( "player_held_object_use_view_model", "-1", FCVAR_REPLICATED | FCVAR_CHEAT, "Use clone models in the view model instead of physics simulated grab controller." );
ConVar player_held_object_collide_with_player( "player_held_object_collide_with_player", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Should held objects collide with players" );
ConVar player_held_object_debug_error( "player_held_object_debug_error", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Spew information on dropping objects due to error" );

ConVar player_held_object_transform_bump_ray( "player_held_object_transform_bump_ray", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

#if defined ( GAME_DLL )
ConVar test_for_vphysics_clips_when_dropping( "test_for_vphysics_clips_when_dropping", "1", FCVAR_CHEAT );
#endif

#if USE_SLOWTIME
extern ConVar slowtime_speed;
#endif // USE_SLOWTIME

static void MatrixOrthogonalize( matrix3x4_t &matrix, int column )
{
	Vector columns[3];
	int i;

	for ( i = 0; i < 3; i++ )
	{
		MatrixGetColumn( matrix, i, columns[i] );
	}

	int index0 = column;
	int index1 = (column+1)%3;
	int index2 = (column+2)%3;

	columns[index2] = CrossProduct( columns[index0], columns[index1] );
	columns[index1] = CrossProduct( columns[index2], columns[index0] );
	VectorNormalize( columns[index2] );
	VectorNormalize( columns[index1] );
	MatrixSetColumn( columns[index1], index1, matrix );
	MatrixSetColumn( columns[index2], index2, matrix );
}

#define SIGN(x) ( (x) < 0 ? -1 : 1 )

static QAngle AlignAngles( const QAngle &angles, float cosineAlignAngle )
{
	matrix3x4_t alignMatrix;
	AngleMatrix( angles, alignMatrix );

	// NOTE: Must align z first
	for ( int j = 3; --j >= 0; )
	{
		Vector vec;
		MatrixGetColumn( alignMatrix, j, vec );
		for ( int i = 0; i < 3; i++ )
		{
			if ( fabs(vec[i]) > cosineAlignAngle )
			{
				vec[i] = SIGN(vec[i]);
				vec[(i+1)%3] = 0;
				vec[(i+2)%3] = 0;
				MatrixSetColumn( vec, j, alignMatrix );
				MatrixOrthogonalize( alignMatrix, j );
				break;
			}
		}
	}

	QAngle out;
	MatrixAngles( alignMatrix, out );
	return out;
}


static void TraceCollideAgainstBBox( const CPhysCollide *pCollide, const Vector &start, const Vector &end, const QAngle &angles, const Vector &boxOrigin, const Vector &mins, const Vector &maxs, trace_t *ptr )
{
	physcollision->TraceBox( boxOrigin, boxOrigin + (start-end), mins, maxs, pCollide, start, angles, ptr );

	if ( ptr->DidHit() )
	{
		ptr->endpos = start * (1-ptr->fraction) + end * ptr->fraction;
		ptr->startpos = start;
		ptr->plane.dist = -ptr->plane.dist;
		ptr->plane.normal *= -1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Computes a local matrix for the player clamped to valid carry ranges
//-----------------------------------------------------------------------------
// when looking level, hold bottom of object 8 inches below eye level
#define PLAYER_HOLD_LEVEL_EYES	-8

// when looking down, hold bottom of object 0 inches from feet
#define PLAYER_HOLD_DOWN_FEET	2

// when looking up, hold bottom of object 24 inches above eye level
#define PLAYER_HOLD_UP_EYES		24

// use a +/-30 degree range for the entire range of motion of pitch
#define PLAYER_LOOK_PITCH_RANGE	30

// player can reach down 2ft below his feet (otherwise he'll hold the object above the bottom)
#define PLAYER_REACH_DOWN_DISTANCE	24

void ComputePlayerMatrix( CBasePlayer *pPlayer, matrix3x4_t &out )
{
	if ( !pPlayer )
		return;

	QAngle angles = pPlayer->EyeAngles();
	Vector origin = pPlayer->EyePosition();
	
	// 0-360 / -180-180
	//angles.x = init ? 0 : AngleDistance( angles.x, 0 );
	//angles.x = clamp( angles.x, -PLAYER_LOOK_PITCH_RANGE, PLAYER_LOOK_PITCH_RANGE );
	angles.x = 0;

	float feet = pPlayer->GetAbsOrigin().z + pPlayer->WorldAlignMins().z;
	float eyes = origin.z;
	float zoffset = 0;
	// moving up (negative pitch is up)
	if ( angles.x < 0 )
	{
		zoffset = RemapVal( angles.x, 0, -PLAYER_LOOK_PITCH_RANGE, PLAYER_HOLD_LEVEL_EYES, PLAYER_HOLD_UP_EYES );
	}
	else
	{
		zoffset = RemapVal( angles.x, 0, PLAYER_LOOK_PITCH_RANGE, PLAYER_HOLD_LEVEL_EYES, PLAYER_HOLD_DOWN_FEET + (feet - eyes) );
	}
	origin.z += zoffset;
	angles.x = 0;
	AngleMatrix( angles, origin, out );
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

#if !defined CLIENT_DLL 
BEGIN_SIMPLE_DATADESC( game_shadowcontrol_params_t )
	
	DEFINE_FIELD( targetPosition,		FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( targetRotation,		FIELD_VECTOR ),
	DEFINE_FIELD( maxAngular, FIELD_FLOAT ),
	DEFINE_FIELD( maxDampAngular, FIELD_FLOAT ),
	DEFINE_FIELD( maxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( maxDampSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( dampFactor, FIELD_FLOAT ),
	DEFINE_FIELD( teleportDistance,	FIELD_FLOAT ),

END_DATADESC()
#endif 


void CGrabController::RotateObject( CBasePlayer *pPlayer, float fRotAboutUp, float fRotAboutRight, bool bUseWorldUpInsteadOfPlayerUp )
{
	if( (fRotAboutRight == 0.0f) && (fRotAboutUp == 0.0f) )
		return; //no actual rotation to do

	if ( !m_bHasPreferredCarryAngles )
	{
		Vector right, up;
		QAngle playerAngles = pPlayer->EyeAngles();		
		AngleVectors( playerAngles, NULL, &right, &up );

		if( bUseWorldUpInsteadOfPlayerUp )
			up.Init( 0.0f, 0.0f, 1.0f );

		Quaternion qRotationAboutUp;
		AxisAngleQuaternion( up, fRotAboutUp, qRotationAboutUp );

		Quaternion qRotationAboutRight;
		AxisAngleQuaternion( right, fRotAboutRight, qRotationAboutRight );

		Quaternion qRotation;
		QuaternionMult( qRotationAboutRight, qRotationAboutUp, qRotation );

		matrix3x4_t tmp;
		ComputePlayerMatrix( pPlayer, tmp );

		QAngle qTemp = TransformAnglesToWorldSpace( m_attachedAnglesPlayerSpace, tmp );
		Quaternion qExisting;
		AngleQuaternion( qTemp, qExisting );
		Quaternion qFinal;
		QuaternionMult( qRotation, qExisting, qFinal );

		QuaternionAngles( qFinal, qTemp );
		m_attachedAnglesPlayerSpace = TransformAnglesToLocalSpace( qTemp, tmp );
	}
}

#if !defined CLIENT_DLL 
BEGIN_SIMPLE_DATADESC( CGrabController )

	DEFINE_EMBEDDED( m_shadow ),

	DEFINE_FIELD( m_timeToArrive,		FIELD_FLOAT ),
	DEFINE_FIELD( m_errorTime,			FIELD_FLOAT ),
	DEFINE_FIELD( m_error,				FIELD_FLOAT ),
	DEFINE_FIELD( m_contactAmount,		FIELD_FLOAT ),
	DEFINE_AUTO_ARRAY( m_savedRotDamping,	FIELD_FLOAT ),
	DEFINE_AUTO_ARRAY( m_savedMass,	FIELD_FLOAT ),
	DEFINE_FIELD( m_flLoadWeight,		FIELD_FLOAT ),
	DEFINE_FIELD( m_bCarriedEntityBlocksLOS, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bIgnoreRelativePitch, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_attachedEntity,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_angleAlignment, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecPreferredCarryAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_bHasPreferredCarryAngles, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flDistanceOffset, FIELD_FLOAT ),
	DEFINE_FIELD( m_attachedAnglesPlayerSpace, FIELD_VECTOR ),
	DEFINE_FIELD( m_attachedPositionObjectSpace, FIELD_VECTOR ),
	DEFINE_FIELD( m_bAllowObjectOverhead, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bWasDragEnabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hHoldingPlayer,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_preVMModeCollisionGroup, FIELD_INTEGER ),
	DEFINE_FIELD( m_prePickupCollisionGroup, FIELD_INTEGER ),
	DEFINE_FIELD( m_oldTransmitState, FIELD_INTEGER ),
	DEFINE_FIELD( m_bOldShadowState, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hOldLightingOrigin, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flAngleOffset, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLengthOffset, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTimeOffset, FIELD_FLOAT ),

	// Physptrs can't be inside embedded classes
	// DEFINE_PHYSPTR( m_controller ),

END_DATADESC()
#endif 

const float DEFAULT_MAX_ANGULAR = 360.0f * 10.0f;
const float REDUCED_CARRY_MASS = 1.0f;

#if defined( GAME_DLL )
CGrabController::CGrabController( void )
#else
CGrabController::CGrabController( void ) : m_iv_predictedRenderOrigin( "CGrabController::m_iv_predictedRenderOrigin" )
#endif
{
	m_shadow.dampFactor = 1.0;
	m_shadow.teleportDistance = 0;
	// make this controller really stiff!
	m_shadow.maxSpeed = 1000;
	m_shadow.maxAngular = DEFAULT_MAX_ANGULAR;
	m_shadow.maxDampSpeed = m_shadow.maxSpeed*2;
	m_shadow.maxDampAngular = m_shadow.maxAngular;
	m_errorTime = 0;
	m_error = 0;
	m_attachedEntity = NULL;
	m_vecPreferredCarryAngles = vec3_angle;
	m_bHasPreferredCarryAngles = false;
	m_flDistanceOffset = 0;
	m_bOldUsingVMGrabState = false;

#if defined( CLIENT_DLL )
	m_iv_predictedRenderOrigin.Setup( &m_vHeldObjectRenderOrigin, INTERPOLATE_LINEAR_ONLY );
	m_iv_predictedRenderOrigin.SetInterpolationAmount( TICK_INTERVAL );
#endif
}

CGrabController::~CGrabController( void )
{
	if ( !IsUsingVMGrab() )
	{
		DetachEntity( false );
	}
}

#if !defined CLIENT_DLL 
void CGrabController::OnRestore()
{
	if ( m_controller )
	{
		m_controller->SetEventHandler( this );
	}
}
#endif 

void CGrabController::SetTargetPosition( const Vector &target, const QAngle &targetOrientation, bool bIsTeleport /*= false*/ )
{
	m_shadow.targetPosition = target;
	m_shadow.targetRotation = targetOrientation;

#if defined ( CLIENT_DLL )
	m_timeToArrive = gpGlobals->frametime;
#else
	if( bIsTeleport && PhysIsFinalTick() == false )
		m_timeToArrive = gpGlobals->interval_per_tick;
	else
		m_timeToArrive = UTIL_GetSimulationInterval();
#endif

	if ( !IsUsingVMGrab() )
	{
		CBaseEntity *pAttached = GetAttached();
		if ( pAttached )
		{
#if defined( GAME_DLL )
			((CPortal_Player *)m_hHoldingPlayer.Get())->m_GrabControllerPersistentVars.m_vLastTargetPosition = target;
			IPhysicsObject *pObj = pAttached->VPhysicsGetObject();
			
			if ( pObj != NULL )
			{
				pObj->Wake();
			}
			else
			{
				DetachEntity( false );
			}
#else
			//TODO
			//NDebugOverlay::BoxAngles( target, pAttached->CollisionProp()->OBBMins(), pAttached->CollisionProp()->OBBMaxs(), targetOrientation, 0, 255, 0, 25, 0.0f );
#endif
		}
	}
}

void CGrabController::GetTargetPosition( Vector *target, QAngle *targetOrientation )
{
	if ( target )
		*target = m_shadow.targetPosition;

	if ( targetOrientation )
		*targetOrientation = m_shadow.targetRotation;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CGrabController::ComputeError()
{
	if ( m_bOldUsingVMGrabState != IsUsingVMGrab() )
	{
		// Just switched states... give it a chance to correct itself before snapping
		m_bOldUsingVMGrabState = IsUsingVMGrab();
		m_errorTime = 0.0f;
	}

	if ( IsUsingVMGrab() )
	{
		// Don't break due to distance when in the viewmodel
		return 0;
	}

#if defined ( CLIENT_DLL )
	// Server decides when grab controllers break.
	return 0;
#endif
	if ( m_errorTime <= 0 )
		return 0;

	CBaseEntity *pAttached = GetAttached();
	if ( pAttached )
	{
		Vector pos;
		IPhysicsObject *pObj = pAttached->VPhysicsGetObject();
		
		if ( pObj )
		{	
			pObj->GetShadowPosition( &pos, NULL );

			float error = (m_shadow.targetPosition - pos).Length();
			if ( m_errorTime > 0 )
			{
				if ( m_errorTime > 1 )
				{
					m_errorTime = 1;
				}
				float speed = error / m_errorTime;
				if ( speed > m_shadow.maxSpeed )
				{
					error *= 0.5;
				}
				m_error = (1-m_errorTime) * m_error + error * m_errorTime;
			}
		}
		else
		{
			DevMsg( "Object attached to Physcannon has no physics object\n" );
			DetachEntity( false );
			return 9999; // force detach
		}
	}
	
	if ( pAttached->IsEFlagSet( EFL_IS_BEING_LIFTED_BY_BARNACLE ) )
	{
 		m_error *= 3.0f;
	}


	// If held across a portal but not looking at the portal multiply error
	CPortal_Player *pPortalPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pAttached );
	Assert( pPortalPlayer );
	if ( pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
	{
		Vector forward, right, up;
		QAngle playerAngles = pPortalPlayer->EyeAngles();

		float pitch = AngleDistance(playerAngles.x,0);
		playerAngles.x = clamp( pitch, -75, 75 );
		AngleVectors( playerAngles, &forward, &right, &up );

		Vector start = pPortalPlayer->Weapon_ShootPosition();

		// If the player is upside down then we need to hold the box closer to their feet.
		if ( up.z < 0.0f )
			start += pPortalPlayer->GetViewOffset() * up.z;
		if ( right.z < 0.0f )
			start += pPortalPlayer->GetViewOffset() * right.z;

		Ray_t rayPortalTest;
		rayPortalTest.Init( start, start + forward * 256.0f );

		if ( UTIL_IntersectRayWithPortal( rayPortalTest, pPortalPlayer->GetHeldObjectPortal() ) < 0.0f )
		{
			bool bHoldRayCrossesHeldPortal = false;
			
			CPortal_Base2D *pHeldPortal = pPortalPlayer->GetHeldObjectPortal();
			if( pHeldPortal && pHeldPortal->IsActivedAndLinked() )
			{
				//bring target position into local space
				Vector vLocalTarget = pHeldPortal->m_hLinkedPortal->m_matrixThisToLinked * m_shadow.targetPosition;
				Ray_t lastChanceRay;
				lastChanceRay.Init( start, vLocalTarget );
				float fLastChanceCloser = 2.0f;
				bHoldRayCrossesHeldPortal = ( UTIL_Portal_FirstAlongRay( lastChanceRay, fLastChanceCloser ) == pHeldPortal );
			}

			if( !bHoldRayCrossesHeldPortal )
			{
				m_error *= 2.5f;
				if ( player_held_object_debug_error.GetBool() )
				{
					engine->Con_NPrintf( 22, "Multiplying error due to portals" );
				}
			}
		}
	}

	// If we've given ourselves extra error distance to allow object-player penetration,
	// multiply that error if some obstruction gets in between the player and the object
	if ( player_held_object_collide_with_player.GetBool() == false )
	{
		trace_t tr;
		Ray_t ray;
		CTraceFilterSkipTwoEntities traceFilter( pPortalPlayer, pAttached, COLLISION_GROUP_NONE );
		Vector vObjectCenter = pAttached->GetAbsOrigin();
		if ( pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
		{
			Assert ( pPortalPlayer->GetHeldObjectPortal() && pPortalPlayer->GetHeldObjectPortal()->GetLinkedPortal() );
			if ( pPortalPlayer->GetHeldObjectPortal() && pPortalPlayer->GetHeldObjectPortal()->GetLinkedPortal()  )
			{
				Vector tmp;
				UTIL_Portal_PointTransform( pPortalPlayer->GetHeldObjectPortal()->GetLinkedPortal()->MatrixThisToLinked(), vObjectCenter, tmp ); 	
				vObjectCenter = tmp;
			}
		}
		ray.Init( pPortalPlayer->EyePosition(), vObjectCenter );
		UTIL_Portal_TraceRay( ray, MASK_SOLID, &traceFilter, &tr, false );
		if ( tr.DidHit() )
		{
			m_error *= 3.0f;
			if ( player_held_object_debug_error.GetBool() )
			{
				engine->Con_NPrintf( 23, "Multiplying error from obstruction" );
			}
		}
	}

	if ( player_held_object_debug_error.GetBool() )
	{
		engine->Con_NPrintf( 24, "Error: %f Time: %f", m_error, m_errorTime );
	}

	m_errorTime = 0;

	return m_error;
}


#define MASS_SPEED_SCALE	60
#define MAX_MASS			40

void CGrabController::ComputeMaxSpeed( CBaseEntity *pEntity, IPhysicsObject *pPhysics )
{
	m_shadow.maxSpeed = 1000;
	m_shadow.maxAngular = DEFAULT_MAX_ANGULAR;

	// Compute total mass...
	float flMass = PhysGetEntityMass( pEntity );
	float flMaxMass = physcannon_maxmass.GetFloat();
	if ( flMass <= flMaxMass )
		return;

	float flLerpFactor = clamp( flMass, flMaxMass, 500.0f );
	flLerpFactor = SimpleSplineRemapVal( flLerpFactor, flMaxMass, 500.0f, 0.0f, 1.0f );

	float invMass = pPhysics->GetInvMass();
	float invInertia = pPhysics->GetInvInertia().Length();

	float invMaxMass = 1.0f / MAX_MASS;
	float ratio = invMaxMass / invMass;
	invMass = invMaxMass;
	invInertia *= ratio;

	float maxSpeed = invMass * MASS_SPEED_SCALE * 200;
	float maxAngular = invInertia * MASS_SPEED_SCALE * 360;

	m_shadow.maxSpeed = Lerp( flLerpFactor, m_shadow.maxSpeed, maxSpeed );
	m_shadow.maxAngular = Lerp( flLerpFactor, m_shadow.maxAngular, maxAngular );
}


QAngle CGrabController::TransformAnglesToPlayerSpace( const QAngle &anglesIn, CBasePlayer *pPlayer )
{
	if ( m_bIgnoreRelativePitch )
	{
		matrix3x4_t test;
		QAngle angleTest = pPlayer->EyeAngles();
		angleTest.x = 0;
		AngleMatrix( angleTest, test );
		return TransformAnglesToLocalSpace( anglesIn, test );
	}
	return TransformAnglesToLocalSpace( anglesIn, pPlayer->EntityToWorldTransform() );
}

QAngle CGrabController::TransformAnglesFromPlayerSpace( const QAngle &anglesIn, CBasePlayer *pPlayer )
{
	if ( m_bIgnoreRelativePitch )
	{
		matrix3x4_t test;
		QAngle angleTest = pPlayer->EyeAngles();

		Vector vUp, vRight, vForward;
		CPortal_Player* pPortalPlayer = ToPortalPlayer( pPlayer );
		Vector vPlayerUp = pPortalPlayer->GetPortalPlayerLocalData().m_Up;
		AngleVectors( angleTest, &vForward, &vRight, &vUp );
		vForward -= vPlayerUp * DotProduct( vPlayerUp, vForward );
		VectorAngles( vForward, vUp, angleTest );

		AngleMatrix( angleTest, test );
		return TransformAnglesToWorldSpace( anglesIn, test );
	}
	return TransformAnglesToWorldSpace( anglesIn, pPlayer->EntityToWorldTransform() );
}


Vector CGrabController::TransformVectorToPlayerSpace( const Vector &vectorIn, CBasePlayer *pPlayer )
{
	Vector vRet;
	VectorIRotate( vectorIn, pPlayer->EntityToWorldTransform(), vRet );
	return vRet;
}

Vector CGrabController::TransformVectorFromPlayerSpace( const Vector &vectorIn, CBasePlayer *pPlayer )
{
	Vector vRet;
	VectorRotate( vectorIn, pPlayer->EntityToWorldTransform(), vRet );
	return vRet;
}


void CGrabController::AttachEntity( CBasePlayer *pPlayer, CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon, const Vector &vGrabPosition, bool bUseGrabPosition )
{
	// If any other player is holding it, they need to drop it now!
	/*for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pOtherPlayer = UTIL_PlayerByIndex( i );
		if ( pOtherPlayer == NULL || pOtherPlayer == pPlayer )
			continue;

		pOtherPlayer->ForceDropOfCarriedPhysObjects();
	}*/

	// play the impact sound of the object hitting the player
	// used as feedback to let the player know he picked up the object
	m_hHoldingPlayer = pPlayer;

	CPortal_Player *pPortalPlayer = (CPortal_Player *)pPlayer;
#if !defined CLIENT_DLL 
	if ( !pPortalPlayer->m_bSilentDropAndPickup )
	{
		int hitMaterial = pPhys->GetMaterialIndex();
		int playerMaterial = pPlayer->VPhysicsGetObject() ? pPlayer->VPhysicsGetObject()->GetMaterialIndex() : hitMaterial;
		PhysicsImpactSound( pPlayer, pPhys, CHAN_STATIC, hitMaterial, playerMaterial, 1.0, 64 );
	}
#else
	m_iv_predictedRenderOrigin.ClearHistory();
#endif
	Vector position;
	QAngle angles;
	if( pPhys )
	{
		pPhys->GetPosition( &position, &angles );
	}
	else
	{
		position = pEntity->GetAbsOrigin();
		angles = pEntity->GetAbsAngles();
	}

	// If it has a preferred orientation, use that instead.
	Pickup_GetPreferredCarryAngles( pEntity, pPlayer, pPlayer->EntityToWorldTransform(), angles );

#if !defined CLIENT_DLL 
	pPortalPlayer->UpdateVMGrab( pEntity );
	pPortalPlayer->SetUsingVMGrabState( pPortalPlayer->WantsVMGrab() );
#endif

	CPortal_Base2D *pHeldObjectPortal = NULL;
	if ( pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
	{
		pHeldObjectPortal = pPortalPlayer->GetHeldObjectPortal();
	}
	else
	{
		pHeldObjectPortal = static_cast< CPortal_Base2D* >( pPortalPlayer->m_hPortalThroughWhichGrabOccured.Get() );
	}

	if ( !IsUsingVMGrab() )
	{
		//Fix attachment orientation weirdness
		if ( pHeldObjectPortal )
		{
			Vector vPlayerForward;
			pPlayer->EyeVectors( &vPlayerForward );

			Vector radial;
			if( pPhys )
			{
				radial = physcollision->CollideGetExtent( pPhys->GetCollide(), vec3_origin, pEntity->GetAbsAngles(), -vPlayerForward );
			}
			else
			{
				vcollide_t *pVCollide = modelinfo->GetVCollide( pEntity->GetModelIndex() );
				if( pVCollide && (pVCollide->solidCount > 0) )
				{
					radial = physcollision->CollideGetExtent( pVCollide->solids[0], vec3_origin, pEntity->GetAbsAngles(), -vPlayerForward );
				}
				else
				{
					radial = vec3_origin;
				}
			}
			// The AABB "orients" to the surface, Length2D doesn't cut it.
			// Instead, we flatten the hull along the up vector and take the length of
			// the resulting vector.
			CPortal_Player* pPortalPlayer = ToPortalPlayer( pPlayer );
			const Vector& stickNormal = pPortalPlayer->GetPortalPlayerLocalData().m_StickNormal;
			Vector player2d = 0.5f * ( pPortalPlayer->GetHullMaxs() - pPortalPlayer->GetHullMins() );
			player2d -= DotProduct( stickNormal, player2d ) * stickNormal;
			float playerRadius = player2d.Length();
			float flDot = DotProduct( vPlayerForward, radial );

			float radius = playerRadius + fabs( flDot );

			float distance = 24 + ( radius * 2.0f );		

			//find out which portal the object is on the other side of....
			Vector start = pPlayer->Weapon_ShootPosition();		
			Vector end = start + ( vPlayerForward * distance );

			// If our end point hasn't gone into the portal yet we at least need to know what portal is in front of us
			Ray_t rayPortalTest;
			rayPortalTest.Init( start, start + vPlayerForward * 1024.0f );

			int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
			if( iPortalCount != 0 )
			{
				CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
				float fMinDist = 2.0f;
				for( int i = 0; i != iPortalCount; ++i )
				{
					CPortal_Base2D *pTempPortal = pPortals[i];
					if( pTempPortal->IsActive() &&
						(pTempPortal->m_hLinkedPortal.Get() != NULL) )
					{
						float fDist = UTIL_IntersectRayWithPortal( rayPortalTest, pTempPortal );
						if( (fDist >= 0.0f) && (fDist < fMinDist) )
						{
							fMinDist = fDist;
							pHeldObjectPortal = pTempPortal;
						}
					}
				}
			}

			if ( pHeldObjectPortal && pHeldObjectPortal->IsActivedAndLinked() )
			{
				UTIL_Portal_AngleTransform( pHeldObjectPortal->m_hLinkedPortal->MatrixThisToLinked(), angles, angles );
			}
		}
	}
	else
	{
		if ( pHeldObjectPortal )
		{
			Vector vTransofrmedPos;
			UTIL_Portal_PointTransform( pHeldObjectPortal->MatrixThisToLinked(), position, vTransofrmedPos );
			position = vTransofrmedPos;

#if !defined CLIENT_DLL 
			pEntity->Teleport( &position, NULL, NULL );
#endif
		}
	}

	VectorITransform( pEntity->WorldSpaceCenter(), pEntity->EntityToWorldTransform(), m_attachedPositionObjectSpace );
//	ComputeMaxSpeed( pEntity, pPhys );

	// Carried entities can never block LOS
	m_bCarriedEntityBlocksLOS = pEntity->BlocksLOS();
	pEntity->SetBlocksLOS( false );
	if( m_controller )
	{
		physenv->DestroyMotionController( m_controller );
	}
	m_controller = physenv->CreateMotionController( this );
	if( pPhys )
	{
		m_controller->AttachObject( pPhys, true );
	}
	// Don't do this, it's causing trouble with constraint solvers.
	//m_controller->SetPriority( IPhysicsMotionController::HIGH_PRIORITY );

	if( pPhys )
	{
		pPhys->Wake();
		PhysSetGameFlags( pPhys, FVPHYSICS_PLAYER_HELD );
	}
	SetTargetPosition( position, angles );
	m_attachedEntity = pEntity;
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
	m_flLoadWeight = 0;
	float flFactor = count / 7.5f;
	if ( flFactor < 1.0f )
	{
		flFactor = 1.0f;
	}
#if !defined CLIENT_DLL 
	float damping = 10;
	for ( int i = 0; i < count; i++ )
	{
		float mass = pList[i]->GetMass();
		pList[i]->GetDamping( NULL, &m_savedRotDamping[i] );
		m_flLoadWeight += mass;
		m_savedMass[i] = mass;

		// reduce the mass to prevent the player from adding crazy amounts of energy to the system
		pList[i]->SetMass( REDUCED_CARRY_MASS / flFactor );
		pList[i]->SetDamping( NULL, &damping );
	}
#endif 
	
	// Give extra mass to the phys object we're actually picking up
	if( pPhys )
	{
		pPhys->SetMass( REDUCED_CARRY_MASS );
		m_bWasDragEnabled = pPhys->IsDragEnabled();
		pPhys->EnableDrag( false );
	}

	m_errorTime = bIsMegaPhysCannon ? -1.5f : -1.0f; // 1 seconds until error starts accumulating
	m_error = 0;
	m_contactAmount = 0;

	m_attachedAnglesPlayerSpace = TransformAnglesToPlayerSpace( angles, pPlayer );
	if ( m_angleAlignment != 0 )
	{
		m_attachedAnglesPlayerSpace = AlignAngles( m_attachedAnglesPlayerSpace, m_angleAlignment );
	}

	// Ragdolls don't offset this way
	VectorITransform( pEntity->WorldSpaceCenter(), pEntity->EntityToWorldTransform(), m_attachedPositionObjectSpace );

	// If it's a prop, see if it has desired carry angles
#if !defined CLIENT_DLL
	CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(pEntity);
	if ( pProp )
	{
		m_bHasPreferredCarryAngles = pProp->GetPropDataAngles( "preferred_carryangles", m_vecPreferredCarryAngles );
		m_flDistanceOffset = pProp->GetCarryDistanceOffset();
	}
	else
	{
		m_bHasPreferredCarryAngles = false;
		m_flDistanceOffset = 0;
	}
#else
	C_BreakableProp *pProp = dynamic_cast<C_BreakableProp *>(pEntity);
	if( pProp )
	{
		m_bHasPreferredCarryAngles = (pProp->GetNetworkedPreferredPlayerCarryAngles().x < FLT_MAX);
		m_flDistanceOffset = 0;//pProp->GetCarryDistanceOffset();
	}
	else
	{
		m_bHasPreferredCarryAngles = false;
		m_flDistanceOffset = 0;
	}
#endif

	m_prePickupCollisionGroup = pEntity->GetCollisionGroup();

	if ( player_held_object_collide_with_player.GetBool() == false )
	{
		pEntity->SetCollisionGroup( COLLISION_GROUP_PLAYER_HELD );
	}

	AttachEntityVM( pPlayer, pEntity, pPhys, bIsMegaPhysCannon, vGrabPosition, bUseGrabPosition );
}

static void ClampPhysicsVelocity( IPhysicsObject *pPhys, float linearLimit, float angularLimit, const Vector *pRelativeVelocity = NULL )
{
	if( pRelativeVelocity == NULL )
		pRelativeVelocity = &vec3_origin;

	Vector vel;
	AngularImpulse angVel;
	pPhys->GetVelocity( &vel, &angVel );
	vel -= *pRelativeVelocity;
	float speed = VectorNormalize(vel);
	float angSpeed = VectorNormalize(angVel);
	speed = speed > linearLimit ? linearLimit : speed;
	angSpeed = angSpeed > angularLimit ? angularLimit : angSpeed;
	vel *= speed;
	vel += *pRelativeVelocity;
	angVel *= angSpeed;
	pPhys->SetVelocity( &vel, &angVel );
}

bool CGrabController::DetachEntity( bool bClearVelocity )
{
	CPortal_Player *pPortalPlayer = ToPortalPlayer( m_hHoldingPlayer.Get() );
#if defined( CLIENT_DLL )
	m_iv_predictedRenderOrigin.ClearHistory();
#else
	if( pPortalPlayer && !pPortalPlayer->m_bSilentDropAndPickup )
	{
		pPortalPlayer->m_GrabControllerPersistentVars.ResetOscillationWatch();
	}
#endif

	if ( IsUsingVMGrab() )
	{
		if ( !DetachEntityVM( bClearVelocity ) )
		{
			CBaseEntity *pEntity = GetAttached();
			Assert ( pEntity );
			if ( !pEntity )
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}



#if !defined CLIENT_DLL 
	Assert(!PhysIsInCallback());
#endif 
	CBaseEntity *pEntity = GetAttached();
	if ( pEntity )
	{
		// If we're not colliding with the player, refuse to drop when penetrating a player.
		// (or anything else... trusting FindSafePlacementLocation to not give false positives.. 
		// if this isn't the case we can change it to a straight object vs player check)
		if ( player_held_object_collide_with_player.GetBool() == false )
		{
			if ( pPortalPlayer && !pPortalPlayer->m_bSilentDropAndPickup && !pPortalPlayer->IsForcingDrop() )
			{
				if ( TestIntersectionVsHeldObjectCollide( pEntity, pEntity->GetAbsOrigin(), m_hHoldingPlayer.Get() ) )
					return false;
			}

			// Also, restore the original collision group
			pEntity->SetCollisionGroup( m_prePickupCollisionGroup );
		}

		// Restore the LS blocking state
		pEntity->SetBlocksLOS( m_bCarriedEntityBlocksLOS );
		IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
		int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
		for ( int i = 0; i < count; i++ )
		{
			IPhysicsObject *pPhys = pList[i];
			if ( !pPhys )
				continue;

			// on the odd chance that it's gone to sleep while under anti-gravity
			pPhys->EnableDrag( m_bWasDragEnabled );
			pPhys->Wake();
#if !defined CLIENT_DLL 
			pPhys->SetMass( m_savedMass[i] );
			pPhys->SetDamping( NULL, &m_savedRotDamping[i] );
#endif 
			PhysClearGameFlags( pPhys, FVPHYSICS_PLAYER_HELD );
			if ( bClearVelocity )
			{
				PhysForceClearVelocity( pPhys );
			}
			else
			{
				//find the player owning this grab controller. So we can pass along their velocity to the clamping function as a relative velocity
				CBasePlayer *pHoldingPlayer = NULL;
				for( int i = 1; i <= gpGlobals->maxClients; ++i )
				{
					CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
					if ( pPlayer )
					{
						if ( GetGrabControllerForPlayer( pPlayer ) == this )
						{
							pHoldingPlayer = pPlayer;
							break;
						}
					}
				}
				
				if( pHoldingPlayer )
				{
					ClampPhysicsVelocity( pPhys, pHoldingPlayer->MaxSpeed() * 1.5f, 2.0f * 360.0f, &pHoldingPlayer->GetAbsVelocity() );
				}
			}

		}
	}

	m_attachedEntity = NULL;
	if ( physenv != NULL )
	{
		physenv->DestroyMotionController( m_controller );
	}
	else
	{
		Warning( "%s(%d): Trying to dereference NULL physenv.\n", __FILE__, __LINE__ );
	}
	m_controller = NULL;
	m_hHoldingPlayer = NULL;

	return true;
}

static bool InContactWithHeavyObject( IPhysicsObject *pObject, float heavyMass )
{
	bool contact = false;
	IPhysicsFrictionSnapshot *pSnapshot = pObject->CreateFrictionSnapshot();
	while ( pSnapshot->IsValid() )
	{
		IPhysicsObject *pOther = pSnapshot->GetObject( 1 );
		if ( !pOther->IsMoveable() || pOther->GetMass() > heavyMass )
		{
			contact = true;
			break;
		}
		pSnapshot->NextFrictionData();
	}
	pObject->DestroyFrictionSnapshot( pSnapshot );
	return contact;
}

IMotionEvent::simresult_e CGrabController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	game_shadowcontrol_params_t shadowParams = m_shadow;
	shadowParams.maxSpeed += m_fPlayerSpeed; //fix for fast moving players being unable to hold objects.

	if ( InContactWithHeavyObject( pObject, GetLoadWeight() ) )
	{
		m_contactAmount = Approach( 0.1f, m_contactAmount, deltaTime*2.0f );
	}
	else
	{
		m_contactAmount = Approach( 1.0f, m_contactAmount, deltaTime*2.0f );
	}
	shadowParams.maxAngular = m_shadow.maxAngular * m_contactAmount * m_contactAmount * m_contactAmount;
	m_timeToArrive = pObject->ComputeShadowControl( shadowParams, m_timeToArrive, deltaTime );

#if defined DEBUG_SHADOW_CONTROLLER
	shadowParams.SpewState();
#endif
	
	// Slide along the current contact points to fix bouncing problems
	Vector velocity;
	AngularImpulse angVel;
	pObject->GetVelocity( &velocity, &angVel );
	PhysComputeSlideDirection( pObject, velocity, angVel, &velocity, &angVel, GetLoadWeight() );
	pObject->SetVelocityInstantaneous( &velocity, NULL );

	linear.Init();
	angular.Init();
	m_errorTime += deltaTime;

	return SIM_LOCAL_ACCELERATION;
}

#if defined( CLIENT_DLL )
//Goals of this algorithm
//1. For a player carrying an object, keep up with their input to keep the grab controller feeling smooth
//2. For an object that collides with a complex spatial interaction on the server, settle in the same position on the server in a smooth (even if wrong) way.
void CGrabController::ClientApproachTarget( CBasePlayer *pOwnerPlayer )
{
	CBaseEntity *pAttached = GetAttached();
	C_PlayerHeldObjectClone *pClone = NULL;

	if ( IsUsingVMGrab() )
	{
		if( pAttached == ((CPortal_Player *)pOwnerPlayer)->m_pHeldEntityClone )
		{
			//update clone as well as base object, clone first with this code, then swap out pAttached for base
			pClone = ((C_PlayerHeldObjectClone *)pAttached);
			pAttached = ((C_PlayerHeldObjectClone *)pAttached)->m_hOriginal;
			
			Vector vPlayerEye, vPlayerForward, vPlayerRight, vPlayerUp;
			pOwnerPlayer->EyePositionAndVectors( &vPlayerEye, &vPlayerForward, &vPlayerRight, &vPlayerUp );
			Vector vEyeRelative = m_shadow.targetPosition - vPlayerEye;
			pClone->m_vPlayerRelativeOrigin.x = vPlayerForward.Dot( vEyeRelative );
			pClone->m_vPlayerRelativeOrigin.y = vPlayerRight.Dot( vEyeRelative );
			pClone->m_vPlayerRelativeOrigin.z = vPlayerUp.Dot( vEyeRelative );
			
			pClone->SetNetworkOrigin( m_shadow.targetPosition );
			pClone->SetNetworkAngles( m_shadow.targetRotation );
			pClone->SetAbsOrigin( m_shadow.targetPosition );
			pClone->SetAbsAngles( m_shadow.targetRotation );
		}
	}

	if( prediction->InPrediction() && pAttached && pAttached->GetPredictable() )
	{
		//Not accounting for visual stuttering, this is the ideal position based on the most recent data from the server
		//Vector vServerTarget = m_shadow.targetPosition - TransformVectorFromPlayerSpace( ((CPortal_Player *)pOwnerPlayer)->m_vecCarriedObject_CurPosToTargetPos, pOwnerPlayer );

		//The same idea as above, but trying to eliminate visual stuttering by ramping into changes coming down from the server, and producing consistent results on repredictions even if the networked offset changes under our feet
		Vector vForwardServerTarget = m_shadow.targetPosition - TransformVectorFromPlayerSpace( ((CPortal_Player *)pOwnerPlayer)->m_vecCarriedObject_CurPosToTargetPos_Interpolated, pOwnerPlayer );
		
		//filter out the player and every version of the held object
		CTraceFilterSimpleList traceFilter( pAttached->GetCollisionGroup() );
		traceFilter.AddEntityToIgnore( pOwnerPlayer );
		traceFilter.AddEntityToIgnore( pAttached );
		if( ((CPortal_Player *)pOwnerPlayer)->m_pHeldEntityClone )
		{
			traceFilter.AddEntityToIgnore( ((CPortal_Player *)pOwnerPlayer)->m_pHeldEntityClone );
		}
		if( ((CPortal_Player *)pOwnerPlayer)->m_pHeldEntityThirdpersonClone )
		{
			traceFilter.AddEntityToIgnore( ((CPortal_Player *)pOwnerPlayer)->m_pHeldEntityThirdpersonClone );
		}

		//For fluidity, there are 2 distinct cases for where to put the object.
		//Case 1: If we think the swept space is collision-free, just put the object at the m_shadow targets. This is the most fluid movement
		//Case 2: If we think there's something for physics on the server to interact with, incorporate vForwardServerTarget somehow. This can move back and forth between start and end position as network data catches up with prediction
		
		Vector vFinalPos;

		ICollideable *pCollideable = pAttached->GetCollideable();
		if( pCollideable )
		{
			trace_t tr;
			enginetrace->SweepCollideable( pCollideable, pAttached->GetNetworkOrigin(), vForwardServerTarget, vec3_angle, MASK_SOLID, &traceFilter, &tr );
			
			if( tr.DidHit() )
			{
				vFinalPos = tr.endpos;				
			}
			else
			{
				enginetrace->SweepCollideable( pCollideable, pAttached->GetNetworkOrigin(), m_shadow.targetPosition, vec3_angle, MASK_SOLID, &traceFilter, &tr );
				vFinalPos = tr.endpos;
			}
		}
		else
		{
			vFinalPos = m_shadow.targetPosition - ((CPortal_Player *)pOwnerPlayer)->m_vecCarriedObject_CurPosToTargetPos_Interpolated;
		}


		if( prediction->IsFirstTimePredicted() )
		{
			if( bLastUpdateWasOnOppositeSideOfPortal != ((CPortal_Player *)pOwnerPlayer)->IsHeldObjectOnOppositeSideOfPortal() )
			{
				m_iv_predictedRenderOrigin.ClearHistory();
				bLastUpdateWasOnOppositeSideOfPortal = ((CPortal_Player *)pOwnerPlayer)->IsHeldObjectOnOppositeSideOfPortal();
			}
			m_iv_predictedRenderOrigin.AddToHead( gpGlobals->curtime, &vFinalPos, false );
		}
		
		pAttached->SetNetworkOrigin( vFinalPos );
		pAttached->SetAbsOrigin( vFinalPos );
		m_vHeldObjectRenderOrigin = vFinalPos;

		pAttached->SetAbsAngles( m_shadow.targetRotation - ((CPortal_Player *)pOwnerPlayer)->m_vecCarriedObject_CurAngToTargetAng_Interpolated );
	}
}

const Vector &CGrabController::GetHeldObjectRenderOrigin( void )
{
	float currentTime;
	C_BasePlayer *pHoldingPlayer = (C_BasePlayer *)m_hHoldingPlayer.Get();
	if( pHoldingPlayer )
	{
		currentTime = pHoldingPlayer->GetFinalPredictedTime();
		currentTime -= TICK_INTERVAL;
		currentTime += ( gpGlobals->interpolation_amount * TICK_INTERVAL );
	}
	else
	{
		currentTime = gpGlobals->curtime;
	}

	m_iv_predictedRenderOrigin.Interpolate( currentTime );
	return m_vHeldObjectRenderOrigin;
}
#endif

#if !defined CLIENT_DLL 
float CGrabController::GetSavedMass( IPhysicsObject *pObject )
{
	CBaseEntity *pHeld = m_attachedEntity;
	if ( pHeld )
	{
		if ( pObject->GetGameData() == (void*)pHeld )
		{
			IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
			int count = pHeld->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
			for ( int i = 0; i < count; i++ )
			{
				if ( pList[i] == pObject )
					return m_savedMass[i];
			}
		}
	}
	return 0.0f;
}
#endif 


void CGrabController::SetPortalPenetratingEntity( CBaseEntity *pPenetrated )
{
	m_PenetratedEntity = pPenetrated;
}

#if !defined CLIENT_DLL 
LINK_ENTITY_TO_CLASS( player_pickup, CPlayerPickupController );
//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CPlayerPickupController )

	DEFINE_EMBEDDED( m_grabController ),

	// Physptrs can't be inside embedded classes
	DEFINE_PHYSPTR( m_grabController.m_controller ),

	DEFINE_FIELD( m_pPlayer,		FIELD_CLASSPTR ),
	
END_DATADESC()
#else
LINK_ENTITY_TO_CLASS_CLIENTONLY( player_pickup, CPlayerPickupController );
#endif 

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			*pObject - 
//-----------------------------------------------------------------------------
void CPlayerPickupController::InitGrabController( CBasePlayer *pPlayer, CBaseEntity *pObject )
{
#if !defined CLIENT_DLL 
	// Holster player's weapon
	if ( pPlayer->GetActiveWeapon() )
	{
		// Don't holster the portalgun
		if ( hide_gun_when_holding.GetBool() == false && FClassnameIs( pPlayer->GetActiveWeapon(), "weapon_portalgun" ) )
		{
			CWeaponPortalgun *pPortalGun = (CWeaponPortalgun*)(pPlayer->GetActiveWeapon());
			pPortalGun->OpenProngs( true );
		}
		else
		{
			if ( !pPlayer->GetActiveWeapon()->Holster() )
			{
				Shutdown();
				return;
			}
		}
	}
#endif 

	CPortal_Player *pOwner = ToPortalPlayer( pPlayer );

	// If the target is debris, convert it to non-debris
	if ( pObject->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
	{
		// Interactive debris converts back to debris when it comes to rest
		pObject->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
	}

	// done so I'll go across level transitions with the player
	SetParent( pPlayer );
	GetGrabController().SetIgnorePitch( true );
	GetGrabController().SetAngleAlignment( 0.866025403784 );
	m_pPlayer = pPlayer;
	IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();
	
	if ( !pOwner->m_bSilentDropAndPickup )
	{
		Pickup_OnPhysGunPickup( pObject, m_pPlayer, PICKED_UP_BY_PLAYER );
	}
	
	GetGrabController().AttachEntity( pPlayer, pObject, pPhysics, false, vec3_origin, false );
	
	m_pPlayer->m_Local.m_iHideHUD |= HIDEHUD_WEAPONSELECTION;
	m_pPlayer->SetUseEntity( this );

#if !defined ( CLIENT_DLL )
	//pObject->DispatchUpdateTransmitState();

	pOwner->m_hAttachedObject = pObject;

#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
bool CPlayerPickupController::Shutdown( bool bThrown )
{
	CBaseEntity *pObject = GetGrabController().GetAttached();

	bool bClearVelocity = false;
	if ( !bThrown && pObject && pObject->VPhysicsGetObject() && pObject->VPhysicsGetObject()->GetContactPoint(NULL,NULL) )
	{
		bClearVelocity = true;
	}

	if ( GetGrabController().DetachEntity( bClearVelocity ) == false )
		return false;

	if ( pObject != NULL )
	{
		if ( !ToPortalPlayer( m_pPlayer )->m_bSilentDropAndPickup )
		{
			Pickup_OnPhysGunDrop( pObject, m_pPlayer, bThrown ? THROWN_BY_PLAYER : DROPPED_BY_PLAYER );
		}
	}
	CPortal_Player *pOwner = ToPortalPlayer( m_pPlayer );

	if ( pOwner )
	{
		pOwner->SetUseEntity( NULL );
		if ( !pOwner->m_bSilentDropAndPickup )
		{
			pOwner->SetHeldObjectOnOppositeSideOfPortal( false );

#if !defined CLIENT_DLL 
			if ( pOwner->GetActiveWeapon() )
			{
				float flAttackDelay = 0.5f;
				
#if USE_SLOWTIME
				if ( pOwner->IsSlowingTime() )
				{
					flAttackDelay *= slowtime_speed.GetFloat();
				}
#endif // USE_SLOWTIME

				pOwner->SetNextAttack( gpGlobals->curtime + flAttackDelay );
				CWeaponPortalgun *pPortalGun = (CWeaponPortalgun*)(m_pPlayer->GetActiveWeapon());
				pPortalGun->DelayAttack( flAttackDelay );

				if ( hide_gun_when_holding.GetBool() == false && FClassnameIs( m_pPlayer->GetActiveWeapon(), "weapon_portalgun" ) )
				{
					pPortalGun->OpenProngs( false );
				}
				else
				{
					if ( !pOwner->GetActiveWeapon()->Deploy() )
					{
						// We tried to restore the player's weapon, but we couldn't.
						// This usually happens when they're holding an empty weapon that doesn't
						// autoswitch away when out of ammo. Switch to next best weapon.
						pOwner->SwitchToNextBestWeapon( NULL );
					}
				}
			}
#endif 

			pOwner->m_Local.m_iHideHUD &= ~HIDEHUD_WEAPONSELECTION;
		}
	}
	Remove();

#if !defined ( CLIENT_DLL )
	if ( pObject != NULL )
	{
		pObject->DispatchUpdateTransmitState();
	}

	pOwner->m_hAttachedObject = NULL;
#endif

	return true;
}

bool CPlayerPickupController::UsePickupController( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ToBasePlayer(pActivator) == m_pPlayer )
	{
		CBaseEntity *pAttached = GetGrabController().GetAttached();

		// UNDONE: Use vphysics stress to decide to drop objects
		// UNDONE: Must fix case of forcing objects into the ground you're standing on (causes stress) before that will work
		float flMaxError = ( player_held_object_collide_with_player.GetBool() ) ? ( 12 ) : ( 40 ); // Some magic numbers here... what we want to allow is the object moving from the desired position into the player before we start trying to break the hold 
		if ( !pAttached || useType == USE_OFF || GetGrabController().ComputeError() > flMaxError )
		{
			return Shutdown();
		}
		
		//Adrian: Oops, our object became motion disabled, let go!
		IPhysicsObject *pPhys = pAttached->VPhysicsGetObject();
		if ( pPhys && pPhys->IsMoveable() == false )
		{
			return Shutdown();
		}

#if STRESS_TEST
		vphysics_objectstress_t stress;
		CalculateObjectStress( pPhys, pAttached, &stress );
		if ( stress.exertedStress > 250 )
		{
			return Shutdown();
		}
#endif

		if ( useType == USE_SET )
		{
			// update position
			if ( GetGrabController().UpdateObject( m_pPlayer, 12 ) == false )
			{
				Shutdown();
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnt - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPlayerPickupController::IsHoldingEntity( CBaseEntity *pEnt )
{
	if ( pEnt )
	{
		return ( GetGrabController().GetAttached() == pEnt );
	}

	return ( GetGrabController().GetAttached() != 0 );
}

CGrabController &CPlayerPickupController::GetGrabController()
{
	return m_grabController;
}

void PlayerPickupObject( CBasePlayer *pPlayer, CBaseEntity *pObject )
{
#if !defined ( CLIENT_DLL )
	//Don't pick up if we don't have a phys object.
	if ( pObject->VPhysicsGetObject() == NULL )
		return;
#endif

	CPlayerPickupController *pController = NULL;
#if !defined CLIENT_DLL 
	if ( pObject->GetBaseAnimating() && pObject->GetBaseAnimating()->IsDissolving() )
		return;

	pController = (CPlayerPickupController *)CreateEntityByName( "player_pickup" );
#endif 
	
	if ( !pController )
		return;

	pController->SetAbsOrigin( pObject->GetAbsOrigin() );
	pController->SetAbsAngles( vec3_angle );
	//pController->SetOwnerEntity( pPlayer );

	pController->InitGrabController( pPlayer, pObject );
}

float CGrabController::GetObjectOffset( CBaseEntity *pEntity ) const
{
	if ( IsUsingVMGrab() )
	{
		//return ( FClassnameIs( pEntity, "npc_personality_core") ) ? ( player_held_object_offset_up_sphere.GetFloat() ) : ( player_held_object_offset_up_vm.GetFloat() );
		if ( FClassnameIs( pEntity, "npc_personality_core") )
		{
			return player_held_object_offset_up_sphere.GetFloat();
		}
#ifdef GAME_DLL
		else if ( FClassnameIs( pEntity, "prop_weighted_cube") )				// Argh. MP will always use this one. 
#else																			// it keeps the object out of eyes but the 				
		else if ( dynamic_cast<C_PlayerHeldObjectClone*>( pEntity ) != NULL )	// convar's name doesn't really show this behavior well.
#endif
		{																		
			return player_held_object_offset_up_cube_vm.GetFloat();
		}
		else if ( dynamic_cast<CNPC_Portal_FloorTurret*>( pEntity ) != NULL )
		{
			return player_held_object_offset_up_turret_vm.GetFloat();
		}
		else
		{
			return 0.0f;
		}
	}
	else
	{
		if ( FClassnameIs( pEntity, "prop_weighted_cube") )
		{
			return player_held_object_offset_up_cube.GetFloat();
		}
		else
		{
			return 0.0f;
		}
	}
}

float CGrabController::GetObjectDistance( void ) const
{
	if ( IsUsingVMGrab() )
	{
		Assert ( m_attachedEntity );
		if ( m_attachedEntity && FClassnameIs( m_attachedEntity, "npc_portal_turret_floor" ) )
		{
			return player_held_object_distance_turret_vm.GetFloat();
		}
		else
		{
			return player_held_object_distance_vm.GetFloat();
		}
	}
	else
	{
		return player_held_object_distance.GetFloat();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CGrabController::UpdateObject( CBasePlayer *pPlayer, float flError, bool bIsTeleport /*= false*/ )
{
	CBaseEntity *pEntity = GetAttached();

	if ( pEntity )
	{
		bool bWantsVMGrab = WantsVMGrab( pPlayer );
		if ( IsUsingVMGrab() && !bWantsVMGrab )
		{
			DetachEntityVM( false );
		}
		else if ( !IsUsingVMGrab() && bWantsVMGrab )
		{
			AttachEntityVM( pPlayer, pEntity, pEntity->VPhysicsGetObject(), false, pEntity->GetAbsOrigin(), false );
		}

		if ( pEntity->GetMoveParent() != NULL && pEntity->GetMoveParent() != pPlayer )
		{
			// Parented to something else now! Detach!
			DetachEntity( false );
			return true;
		}
	}

	if ( IsUsingVMGrab() )
	{
		return UpdateObjectVM( pPlayer, flError );
	}

	m_fPlayerSpeed = pPlayer->GetAbsVelocity().Length();

	CBaseEntity *pPenetratedEntity = m_PenetratedEntity.Get();
	if( pPenetratedEntity )
	{
		//FindClosestPassableSpace( pPenetratedEntity, Vector( 0.0f, 0.0f, 1.0f ) );
		IPhysicsObject *pPhysObject = pPenetratedEntity->VPhysicsGetObject();
		if( pPhysObject )
			pPhysObject->Wake();

		m_PenetratedEntity = NULL; //assume we won
	}

	
#if !defined ( CLIENT_DLL )
	if ( !pEntity || ComputeError() > flError || pPlayer->GetGroundEntity() == pEntity || !pEntity->VPhysicsGetObject() )
	{
		return false;
	}
#endif 

#if !defined ( CLIENT_DLL )
	IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
	//Adrian: Oops, our object became motion disabled, let go!
	if ( pPhys && pPhys->IsMoveable() == false )
	{
		return false;
	}
#endif 

	Vector forward, right, up;
	QAngle playerAngles = pPlayer->EyeAngles();

	float pitch = AngleDistance(playerAngles.x,0);
	if( !m_bAllowObjectOverhead )
	{
		playerAngles.x = clamp( pitch, -75, 75 );
	}
	else
	{
		playerAngles.x = clamp( pitch, -90, 75 );
	}

	AngleVectors( playerAngles, &forward, &right, &up );

	Vector start = pPlayer->Weapon_ShootPosition();

	// If the player is upside down then we need to hold the box closer to their feet.
	if ( up.z < 0.0f )
		start += pPlayer->GetViewOffset() * up.z;
	if ( right.z < 0.0f )
		start += pPlayer->GetViewOffset() * right.z;

	CPortal_Player *pPortalPlayer = ToPortalPlayer( pPlayer );


	bool bLookingAtHeldPortal = true;
	CPortal_Base2D *pPortal = pPortalPlayer->GetHeldObjectPortal();

	if ( !IsUsingVMGrab() )
	{
		// Find out if it's being held across a portal
		if ( !pPortal )
		{
			// If the portal is invalid make sure we don't try to hold it across the portal
			pPortalPlayer->SetHeldObjectOnOppositeSideOfPortal( false );
		}
	 
		if ( pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
		{
			Ray_t rayPortalTest;
			rayPortalTest.Init( start, start + forward * 1024.0f );

			// Check if we're looking at the portal we're holding through
			if ( pPortal )
			{
				if ( UTIL_IntersectRayWithPortal( rayPortalTest, pPortal ) < 0.0f )
				{
					bLookingAtHeldPortal = false;
				}
			}
			// If our end point hasn't gone into the portal yet we at least need to know what portal is in front of us
			else
			{
				int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
				if( iPortalCount != 0 )
				{
					CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
					float fMinDist = 2.0f;
					for( int i = 0; i != iPortalCount; ++i )
					{
						CPortal_Base2D *pTempPortal = pPortals[i];
						if( pTempPortal->IsActive() &&
							(pTempPortal->m_hLinkedPortal.Get() != NULL) )
						{
							float fDist = UTIL_IntersectRayWithPortal( rayPortalTest, pTempPortal );
							if( (fDist >= 0.0f) && (fDist < fMinDist) )
							{
								fMinDist = fDist;
								pPortal = pTempPortal;
							}
						}
					}
				}
			}
		}
		else
		{
			pPortal = NULL;
		}
	}

#if defined( GAME_DLL )
	pPortalPlayer->m_GrabControllerPersistentVars.m_hLookingThroughPortalLastUpdate = bLookingAtHeldPortal ? pPortal : NULL; //bLookingAtHeldPortal is true for cases where we're simply looking at a portal as well as holding across a portal
	if( !pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() && (pPortal == pPortalPlayer->m_GrabControllerPersistentVars.m_hOscillationWatch) )
	{
		pPortalPlayer->m_GrabControllerPersistentVars.m_hOscillationWatch = NULL; //end oscillation watch if we actually look through the portal
	}
#endif

	QAngle qEntityAngles = pEntity->GetAbsAngles();

	if ( !IsUsingVMGrab() )
	{
		if ( pPortal )
		{
			// If the portal isn't linked we need to drop the object
			if ( !pPortal->m_hLinkedPortal.Get() )
			{
				pPlayer->ForceDropOfCarriedPhysObjects();
				return false;
			}

			UTIL_Portal_AngleTransform( pPortal->m_hLinkedPortal->MatrixThisToLinked(), qEntityAngles, qEntityAngles );
		}
	}

	// Now clamp a sphere of object radius at end to the player's bbox
#if 0
#if defined( GAME_DLL )
	Vector radial = physcollision->CollideGetExtent( pPhys->GetCollide(), vec3_origin, qEntityAngles, -forward );
#else
	Vector radial;
	{
		vcollide_t *pVCollide = modelinfo->GetVCollide( pEntity->GetModelIndex() );
		if( pVCollide && (pVCollide->solidCount > 0) )
		{
			radial = physcollision->CollideGetExtent( pVCollide->solids[0], vec3_origin, qEntityAngles, -forward );
		}
		else
		{
			CCollisionProperty *pCollisionProp = pEntity->CollisionProp();
			if( pCollisionProp )
			{
				pCollisionProp->CalcNearestPoint( start, &radial );
				radial -= start;
			}
			else
			{
				radial = vec3_origin;
			}
		}
	}
#endif
#endif

	// The AABB "orients" to the surface, Length2D doesn't cut it.
	// Instead, we flatten the hull along the up vector and take the length of
	// the resulting vector.
	const Vector& stickNormal = pPortalPlayer->GetPortalPlayerLocalData().m_StickNormal;
	Vector player2d = 0.5f * ( pPortalPlayer->GetHullMaxs() - pPortalPlayer->GetHullMins() );
	player2d -= DotProduct( stickNormal, player2d ) * stickNormal;
	float playerRadius = player2d.Length();

	float radius = playerRadius + pEntity->BoundingRadius();

	float distance = GetObjectDistance() + radius;

	// Add the prop's distance offset
	distance += m_flDistanceOffset;

	pitch = AngleDistance(playerAngles.x,0);

	float flUpOffset = RemapValClamped( fabs(pitch), 0.0f, 75.0f, 1.0f, 0.0f ) * GetObjectOffset( pEntity );

	Vector end;
	if ( player_hold_object_in_column.GetBool() )
	{
		Vector player2dForward = CrossProduct( stickNormal, right );
		Vector point = start + ( player2dForward * distance );
		float intersection = IntersectRayWithPlane( start, forward, player2dForward, -DotProduct( -player2dForward, point ) );
		distance = clamp( intersection, GetObjectDistance(), player_hold_column_max_size.GetFloat() );
	}

	trace_t	tr;
	CTraceFilterSkipTwoEntities traceFilter( pPlayer, pEntity, COLLISION_GROUP_NONE );
	Ray_t ray;
	ray.Init( start, start + forward * distance  + up * flUpOffset  );

	//enginetrace->TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );
	UTIL_Portal_TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );

#if defined( GAME_DLL )
	if( !tr.DidHit() )
	{
		pPortalPlayer->m_GrabControllerPersistentVars.m_hOscillationWatch = NULL; //end oscillation watch if we trace into open air
	}
	else if( pPortalPlayer->m_GrabControllerPersistentVars.m_hOscillationWatch.Get() != NULL )
	{
		//make sure it's also not obscenely far away from the oscillation watch portal
		CPortal_Base2D *pOscillationPortal = pPortalPlayer->m_GrabControllerPersistentVars.m_hOscillationWatch.Get();
		Vector vOscillationPortalToTraceHit = tr.endpos - pOscillationPortal->m_ptOrigin;

		if( (fabs(pOscillationPortal->m_vRight.Dot(vOscillationPortalToTraceHit)) >= (pOscillationPortal->GetHalfWidth() * 2.0f)) ||
			(fabs(pOscillationPortal->m_vUp.Dot(vOscillationPortalToTraceHit)) >= (pOscillationPortal->GetHalfHeight() * 2.0f)) ||
			(fabs(pOscillationPortal->m_vForward.Dot(vOscillationPortalToTraceHit)) >= 5.0f) )
		{
			pPortalPlayer->m_GrabControllerPersistentVars.m_hOscillationWatch = NULL; //trace point has gone reasonably far from the portal
		}
	}
#endif

	float flTraceDist = distance * tr.fraction;
	if ( flTraceDist < radius )
		flTraceDist = radius;

	Vector direction = ray.m_Delta.Normalized();
	end = start + ( direction * flTraceDist ) +
				  ( up * flUpOffset );

	Vector playerMins, playerMaxs, nearest;
	pPlayer->CollisionProp()->WorldSpaceAABB( &playerMins, &playerMaxs );
	Vector playerLine = pPlayer->CollisionProp()->WorldSpaceCenter();
	float fHeight = pPortalPlayer->GetHullHeight();
	Vector vUp = pPortalPlayer->GetPortalPlayerLocalData().m_Up;
	// Note: We can't use pPlayer->CollisionProp()->WorldSpaceCenter() because our player's bbox doesn't rotate on stick power
	playerLine = pPortalPlayer->EyePosition() - fHeight * vUp;

	CalcClosestPointOnLine( end, playerLine + vUp * fHeight, playerLine - vUp * fHeight, nearest, NULL );
	/*NDebugOverlay::Sphere( nearest, 10, 255, 0, 0, true, 0.1f );
	NDebugOverlay::Sphere( playerLine, 10, 0, 0, 255, true, 0.1f );*/

	//
	//
	//

	// Trace down from the held object and see if we need to bump up off the floor
	const float flHalfRadius = radius/2.0f;
	Vector vecMaxs( flHalfRadius, flHalfRadius, 0 );
	Vector vecMins = -vecMaxs;
	Vector vecEndPos = end - Vector(0,0,flHalfRadius+1);

	UTIL_ClearTrace( tr );

	ray.Init( end + Vector(0,0,flHalfRadius+1), vecEndPos );
	
	if ( !IsUsingVMGrab() && player_held_object_transform_bump_ray.GetBool() )
	{
		if ( pPortal != NULL && pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
		{
			VMatrix matThisToLinked = pPortal->MatrixThisToLinked();
			Ray_t portalRay = ray;
			UTIL_Portal_RayTransform( matThisToLinked, portalRay, ray );
		}
	}

	
	UTIL_Portal_TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );

	// Hold onto our last frame because there's a case where moving through a portal creates
	// a brief pop as you hit a solid "no man's land" while traversing the portal
	static float flLastDelta = 0.0f;

	if ( !tr.startsolid )
	{
		if ( tr.fraction < 1.0f )
		{
			flLastDelta = radius * (1.0f-tr.fraction);		
		}
		else
		{
			flLastDelta = 0.0f;
		}
	}

	end.z += flLastDelta;

	if( !m_bAllowObjectOverhead )
	{
		Vector delta = end - nearest;
		float len = VectorNormalize(delta);
		if ( len < radius )
		{
			end = nearest + radius * delta;
		}
	}

#if !defined ( CLIENT_DLL )
	// Send these down to the client
	pPortalPlayer->m_vecCarriedObjectAngles = m_attachedAnglesPlayerSpace;
#endif

	QAngle angles = TransformAnglesFromPlayerSpace( m_attachedAnglesPlayerSpace, pPlayer );

	//Show overlays of radius
	if ( g_debug_physcannon.GetBool() )
	{
		NDebugOverlay::Box( end, -Vector( 2,2,2 ), Vector(2,2,2), 0, 255, 0, true, 0 );

		NDebugOverlay::Box( GetAttached()->WorldSpaceCenter(), 
			-Vector( radius, radius, radius), 
			Vector( radius, radius, radius ),
			255, 0, 0,
			true,
			0.0f );
	}


	// If it has a preferred orientation, update to ensure we're still oriented correctly.
	Pickup_GetPreferredCarryAngles( pEntity, pPlayer, pPlayer->EntityToWorldTransform(), angles );

	// We may be holding a prop that has preferred carry angles
	if ( m_bHasPreferredCarryAngles )
	{
		matrix3x4_t tmp;
		ComputePlayerMatrix( pPlayer, tmp );
		angles = TransformAnglesToWorldSpace( m_vecPreferredCarryAngles, tmp );
	}



	matrix3x4_t attachedToWorld;
	Vector offset;
	AngleMatrix( angles, attachedToWorld );
	VectorRotate( m_attachedPositionObjectSpace, attachedToWorld, offset );

	//if the player is moving pretty fast. Start moving the object more towards where they're going to be instead of where they are.
	{
		Vector vCurrentOffsetDirection = end - pPlayer->WorldSpaceCenter();
		vCurrentOffsetDirection.NormalizeInPlace();

		Vector vSpeedAddon = pPlayer->GetAbsVelocity() * (gpGlobals->interval_per_tick * MIN( m_fPlayerSpeed / m_shadow.maxSpeed, 1.0f) );
		float fDot = vSpeedAddon.Dot( vCurrentOffsetDirection );
		end += vCurrentOffsetDirection * MAX( 0.0f, fDot );
	}

#if defined( GAME_DLL )
	pPortalPlayer->m_GrabControllerPersistentVars.m_bLastUpdateWasForcedPull = false;
#endif

	if ( !IsUsingVMGrab() )
	{
		// Translate hold position and angles across portal
		if ( pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
		{
			CPortal_Base2D *pPortalLinked = pPortal->m_hLinkedPortal;
			if ( pPortal && pPortal->IsActive() && pPortalLinked != NULL )
			{
				Vector vTeleportedPosition;
				QAngle qTeleportedAngles;

				bool bHoldRayCrossesHeldPortal;
				if( !bLookingAtHeldPortal && pPortalPlayer->IsHeldObjectOnOppositeSideOfPortal() )
				{
					Ray_t lastChanceRay;
					lastChanceRay.Init( start, end - offset );
					float fLastChanceCloser = 2.0f;
					bHoldRayCrossesHeldPortal = ( UTIL_Portal_FirstAlongRay( lastChanceRay, fLastChanceCloser ) == pPortalPlayer->GetHeldObjectPortal() );
				}
				else
				{
					bHoldRayCrossesHeldPortal = false;
				}

				if ( !bLookingAtHeldPortal && !bHoldRayCrossesHeldPortal && ( start - pPortal->GetAbsOrigin() ).Length() > distance - radius )
				{
					// Pull the object through the portal
					Vector vPortalLinkedForward;
					pPortalLinked->GetVectors( &vPortalLinkedForward, NULL, NULL );
					vTeleportedPosition = pPortalLinked->GetAbsOrigin() - vPortalLinkedForward * ( 1.0f + offset.Length() );
					qTeleportedAngles = pPortalLinked->GetAbsAngles();
#if defined( GAME_DLL )
					pPortalPlayer->m_GrabControllerPersistentVars.m_bLastUpdateWasForcedPull = true;
#endif
				}
				else
				{
					// Translate hold position and angles across the portal
					VMatrix matThisToLinked = pPortal->MatrixThisToLinked();
					UTIL_Portal_PointTransform( matThisToLinked, end - offset, vTeleportedPosition );
					UTIL_Portal_AngleTransform( matThisToLinked, angles, qTeleportedAngles );
				}

				SetTargetPosition( vTeleportedPosition, qTeleportedAngles, bIsTeleport );
				pPortalPlayer->SetHeldObjectPortal( pPortal );
			}
			else
			{
				pPlayer->ForceDropOfCarriedPhysObjects();
			}
		}
		else
		{
#if defined( GAME_DLL )
			if( pPortalPlayer->m_GrabControllerPersistentVars.m_hOscillationWatch.Get() != NULL )
			{
				//prevent an oscillation bug where our target position makes the held object teleport through a portal this tick, then teleport right back the next tick because we're not looking at the portal
				CPortal_Base2D *pOscillationPortal = pPortalPlayer->m_GrabControllerPersistentVars.m_hOscillationWatch;
				Vector vTargetPosition = end - offset;
				float fPlaneDist = pOscillationPortal->m_plane_Origin.normal.Dot( vTargetPosition ) - pOscillationPortal->m_plane_Origin.dist;
				if( fPlaneDist < 1.0f )
				{
					end += pOscillationPortal->m_plane_Origin.normal * (1.0f - fPlaneDist); //bump out to a minimum of 1 inch in front of the portal plane
				}
			}
#endif
			SetTargetPosition( end - offset, angles, bIsTeleport );
			pPortalPlayer->SetHeldObjectPortal( NULL );
		}
	}
	else
	{
		SetTargetPosition( end - offset, angles, bIsTeleport );
		pPortalPlayer->SetHeldObjectPortal( NULL );
	}

#if defined( GAME_DLL )
	if( GetAttached() )
	{
		pPortalPlayer->m_vecCarriedObject_CurPosToTargetPos = TransformVectorToPlayerSpace( m_shadow.targetPosition - GetAttached()->GetAbsOrigin(), pPlayer );
		pPortalPlayer->m_vecCarriedObject_CurAngToTargetAng = m_shadow.targetRotation - GetAttached()->GetAbsAngles();
	}
#endif


	return true;
}

bool PlayerPickupControllerIsHoldingEntity( CBaseEntity *pPickupControllerEntity, CBaseEntity *pHeldEntity )
{
	CPlayerPickupController *pController = dynamic_cast<CPlayerPickupController *>(pPickupControllerEntity);
	return pController ? pController->IsHoldingEntity( pHeldEntity ) : false;
}

void ShutdownPickupController( CBaseEntity *pPickupControllerEntity )
{
	CPlayerPickupController *pController = dynamic_cast<CPlayerPickupController *>(pPickupControllerEntity);
	pController->Shutdown( false );
}

CBaseEntity *GetPlayerHeldEntity( CBasePlayer *pPlayer )
{
	CBaseEntity *pObject = NULL;
#if !defined ( CLIENT_DLL )
	CPlayerPickupController *pPlayerPickupController = (CPlayerPickupController *)(pPlayer->GetUseEntity());

	if ( pPlayerPickupController )
	{
		pObject = pPlayerPickupController->GetGrabController().GetAttached();
	}
#else
	CPortal_Player *pPortalPlayer = (CPortal_Player*)pPlayer;
	Assert ( pPlayer );
	if ( pPlayer )
	{
		pObject = pPortalPlayer->m_pHeldEntityClone;
		if ( !pObject )
		{
			pObject = pPortalPlayer->GetAttachedObject();
		}
	}
#endif 

	return pObject;
}

CBasePlayer *GetPlayerHoldingEntity( const CBaseEntity *pEntity )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
#if defined( GAME_DLL )
			if ( GetPlayerHeldEntity( pPlayer ) == pEntity )
#else
			if( (((CPortal_Player*)pPlayer)->GetAttachedObject() == pEntity) ||
				(((CPortal_Player*)pPlayer)->m_pHeldEntityClone == pEntity) )
#endif
			{
				return pPlayer;
			}
		}
	}
	return NULL;
}

CGrabController *GetGrabControllerForPlayer( CBasePlayer *pPlayer )
{
	CPlayerPickupController *pPlayerPickupController = (CPlayerPickupController *)(pPlayer->GetUseEntity());
	if( pPlayerPickupController )
		return &(pPlayerPickupController->GetGrabController());

	return NULL;
}

void RotatePlayerHeldObject( CBasePlayer *pPlayer, float fRotAboutUp, float fRotAboutRight, bool bUseWorldUpInsteadOfPlayerUp )
{
	CPlayerPickupController *pPlayerPickupController = (CPlayerPickupController *)(pPlayer->GetUseEntity());
	if( pPlayerPickupController )
		pPlayerPickupController->GetGrabController().RotateObject( pPlayer, fRotAboutUp, fRotAboutRight, bUseWorldUpInsteadOfPlayerUp );
}

#if !defined CLIENT_DLL
void GetSavedParamsForCarriedPhysObject( CGrabController *pGrabController, IPhysicsObject *pObject, float *pSavedMassOut, float *pSavedRotationalDampingOut )
{
	if ( !pGrabController || !pObject )
	{
		Assert ( 0 );
		return;
	}

	CBaseEntity *pHeld = pGrabController->m_attachedEntity;
	if( pHeld )
	{
		if( pObject->GetGameData() == (void*)pHeld )
		{
			IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
			int count = pHeld->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
			for ( int i = 0; i < count; i++ )
			{
				if ( pList[i] == pObject )
				{
					if( pSavedMassOut )
						*pSavedMassOut = pGrabController->m_savedMass[i];

					if( pSavedRotationalDampingOut )
						*pSavedRotationalDampingOut = pGrabController->m_savedRotDamping[i];

					return;
				}
			}
		}
	}

	if( pSavedMassOut )
		*pSavedMassOut = 0.0f;

	if( pSavedRotationalDampingOut )
		*pSavedRotationalDampingOut = 0.0f;

	return;
}
#endif // !CLIENT_DLL

void UpdateGrabControllerTargetPosition( CBasePlayer *pPlayer, Vector *vPosition, QAngle *qAngles, bool bIsTeleport /*= true*/ )
{
	CGrabController *pGrabController = GetGrabControllerForPlayer( pPlayer );

	if ( !pGrabController )
		return;

	pGrabController->UpdateObject( pPlayer, 12, bIsTeleport );
	pGrabController->GetTargetPosition( vPosition, qAngles );
}


#if !defined ( CLIENT_DLL )
float PlayerPickupGetHeldObjectMass( CBaseEntity *pPickupControllerEntity, IPhysicsObject *pHeldObject )
{
	float mass = 0.0f;
	CPlayerPickupController *pController = dynamic_cast<CPlayerPickupController *>(pPickupControllerEntity);
	if ( pController )
	{
		CGrabController &grab = pController->GetGrabController();
		mass = grab.GetSavedMass( pHeldObject );
	}
	return mass;
}
#endif


void GrabController_SetPortalPenetratingEntity( CGrabController *pController, CBaseEntity *pPenetrated )
{
	pController->SetPortalPenetratingEntity( pPenetrated );
}

struct collidelist_t
{
	const CPhysCollide	*pCollide;
	Vector			origin;
	QAngle			angles;
};

// TODO: Blatantly stolen from triggers.cpp... should peel this out into an engine feature
bool TestIntersectionVsHeldObjectCollide( CBaseEntity *pHeldObject, Vector vHeldObjectTestOrigin, CBaseEntity *pOther )
{
	if ( !pHeldObject || !pOther || !pHeldObject->VPhysicsGetObject() || !pOther->CollisionProp() )
	{
		return false;
	}

	switch ( pOther->GetSolid() )
	{
	case SOLID_BBOX:
		{
			ICollideable *pCollide = pHeldObject->CollisionProp();
			Ray_t ray;
			trace_t tr;
			ray.Init( pOther->GetAbsOrigin(), pOther->GetAbsOrigin(), pOther->WorldAlignMins(), pOther->WorldAlignMaxs() );
			enginetrace->ClipRayToCollideable( ray, MASK_SOLID, pCollide, &tr );

			if ( tr.startsolid )
				return true;
		}
		break;
	case SOLID_BSP:
	case SOLID_VPHYSICS:
		{
			CPhysCollide *pHeldObjectVCollide = modelinfo->GetVCollide( pHeldObject->GetModelIndex() )->solids[0];
			Assert( pHeldObjectVCollide );

			CUtlVector<collidelist_t> collideList;
			IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
			int physicsCount = pHeldObject->VPhysicsGetObjectList( pList, ARRAYSIZE(pList) );
			if ( physicsCount )
			{
				for ( int i = 0; i < physicsCount; i++ )
				{
					const CPhysCollide *pCollide = pList[i]->GetCollide();
					if ( pCollide )
					{
						collidelist_t element;
						element.pCollide = pCollide;
						pList[i]->GetPosition( &element.origin, &element.angles );
						element.origin = element.origin - pHeldObject->GetAbsOrigin() + vHeldObjectTestOrigin;
						collideList.AddToTail( element );
					}
				}
			}
			else
			{
				vcollide_t *pVCollide = modelinfo->GetVCollide( pHeldObject->GetModelIndex() );
				if ( pVCollide && pVCollide->solidCount )
				{
					collidelist_t element;
					element.pCollide = pVCollide->solids[0];
					element.origin = vHeldObjectTestOrigin;
					element.angles = pHeldObject->GetAbsAngles();
					collideList.AddToTail( element );
				}
			}
			for ( int i = collideList.Count()-1; i >= 0; --i )
			{
				const collidelist_t &element = collideList[i];
				trace_t tr;
				physcollision->TraceCollide( element.origin, element.origin, element.pCollide, element.angles, pHeldObjectVCollide, vHeldObjectTestOrigin, pHeldObject->GetAbsAngles(), &tr );
				if ( tr.startsolid )
					return true;
			}
		}
		break;

	default:
		return false;
	}
	return false;
}

bool CGrabController::IsUsingVMGrab( CBasePlayer *pPlayer ) const
{
	if ( pPlayer == NULL )
	{
		pPlayer = (CPortal_Player*)m_hHoldingPlayer.Get();
	}

	CPortal_Player *pPortalPlayer = (CPortal_Player*)pPlayer;
	if ( pPortalPlayer )
	{
		return pPortalPlayer->IsUsingVMGrab();
	}

	return false;
}

bool CGrabController::WantsVMGrab( CBasePlayer *pPlayer ) const
{
	if ( pPlayer == NULL )
	{
		pPlayer = (CPortal_Player*)m_hHoldingPlayer.Get();
	}

	CPortal_Player *pPortalPlayer = (CPortal_Player*)pPlayer;
	if ( pPortalPlayer )
	{
		return pPortalPlayer->WantsVMGrab();
	}

	return false;
}

#if defined( GAME_DLL )
void CGrabController::CheckPortalOscillation( CPortal_Base2D *pWentThroughPortal, CBaseEntity *pTeleportingEntity, CPortal_Player *pHoldingPlayer )
{
	if( pHoldingPlayer->m_GrabControllerPersistentVars.m_hLookingThroughPortalLastUpdate.Get() != NULL ) //oscillation case only happens (for now) when you aren't looking into the portal
		return;

	if( pHoldingPlayer->m_GrabControllerPersistentVars.m_bLastUpdateWasForcedPull )
	{
		pHoldingPlayer->m_GrabControllerPersistentVars.m_hOscillationWatch = pWentThroughPortal->GetLinkedPortal();
	}
	else
	{
		float fPlaneDist = pWentThroughPortal->m_plane_Origin.normal.Dot( pHoldingPlayer->m_GrabControllerPersistentVars.m_vLastTargetPosition ) - pWentThroughPortal->m_plane_Origin.dist;
		
		if( fPlaneDist < 1.0f && fPlaneDist > -5.0f ) //magic numbers, "kinda near the plane"
		{
			//doublecheck that the target isn't within the quad
			Vector vPortalToTargetPos = pHoldingPlayer->m_GrabControllerPersistentVars.m_vLastTargetPosition - pWentThroughPortal->m_ptOrigin;
			if( (fabs(pWentThroughPortal->m_vRight.Dot(vPortalToTargetPos)) >= pWentThroughPortal->GetHalfWidth()) ||
				(fabs(pWentThroughPortal->m_vUp.Dot(vPortalToTargetPos)) >= pWentThroughPortal->GetHalfHeight()) )
			{
				pHoldingPlayer->m_GrabControllerPersistentVars.m_hOscillationWatch = pWentThroughPortal; //keep an eye on target positions for a while
			}
		}
	}
}
#endif

#if defined ( GAME_DLL )
bool TestRayVsFuncClipVPhysics( const Ray_t& ray )
{
	for( int i = 0; i != GetVPhysicsClipList().Count(); ++i )
	{
		VPhysicsClipEntry_t &checkEntry = GetVPhysicsClipList()[i];

		if ( checkEntry.hEnt == NULL )
			continue;

		IPhysicsObject* pPhys = checkEntry.hEnt.Get()->VPhysicsGetObject();
		// Don't check collision against vphys_clips that are disabled
		if( pPhys && !pPhys->IsCollisionEnabled() )
			continue;

		if ( IsBoxIntersectingRay( checkEntry.vAABBMins, checkEntry.vAABBMaxs, ray ) )
		{
			// If we hit the AABB, perform the more expensive test vs physics
			trace_t tr;
			enginetrace->ClipRayToEntity( ray, MASK_SOLID, checkEntry.hEnt, &tr );
			if ( tr.DidHit() )
			{
				return true;
			}
		}
	}
	return false;
}

bool TestHeldEntityVsFuncClipVPhysics( CBaseEntity* pEnt, Vector testOrg )
{
	Assert ( pEnt );
	if ( !pEnt )
		return false;

	for( int i = 0; i != GetVPhysicsClipList().Count(); ++i )
	{
		VPhysicsClipEntry_t &checkEntry = GetVPhysicsClipList()[i];

		if ( checkEntry.hEnt.Get() )
		{
			IPhysicsObject* pPhys = checkEntry.hEnt.Get()->VPhysicsGetObject();
			// Don't check collision against vphys_clips that are disabled
			if ( pPhys && !pPhys->IsCollisionEnabled() )
				continue;
		}

		CCollisionProperty* pColProp = pEnt->CollisionProp();
		Vector vMins = pColProp->GetCollisionOrigin() + pColProp->OBBMins();
		Vector vMaxs = pColProp->GetCollisionOrigin() + pColProp->OBBMaxs();
		if ( IsBoxIntersectingBox( vMins, vMaxs, checkEntry.vAABBMins, checkEntry.vAABBMaxs ) )
		{
			// If we hit the AABB, perform the more expensive test vs physics
			if ( TestIntersectionVsHeldObjectCollide( pEnt, testOrg, checkEntry.hEnt ) )
			{
				return true;
			}
		}
	}
	return false;
}
#endif

// When dropping, try to place this non-solid held object in a valid spot.
bool CGrabController::FindSafePlacementLocation( Vector *pVecPosition, bool bFinalPass )
{
	if ( !pVecPosition )
		return false;

	CBaseEntity *pEntity = GetAttached();
	if ( !pEntity )
		return false;

	if ( !pEntity->VPhysicsGetObject() )
		return false;

	// HACK: Some map logic will steal the held object out of our hands by setting its parent and motion disabling it
	// to let that work, we'll pretend its a safe spot when we get motion disabled.
	if ( pEntity->VPhysicsGetObject()->IsMotionEnabled() == false )
		return true;

	CPortal_Player *pPlayer = (CPortal_Player*)GetPlayerHoldingEntity( pEntity );
	Assert ( pPlayer );
	if ( !pPlayer )
		return false;

	Vector vecDropPosition = *pVecPosition;

	// TODO: Scan for appropriate places like buttons or on top of a nearby ledge.
	
	// Assume the player is in a safe space, then trace outwards to held position from there.
	trace_t tr;
	CTraceFilterSkipTwoEntities traceFilter( pPlayer, pEntity, COLLISION_GROUP_NONE );
	//CTraceFilterSimple traceFilter( pEntity, COLLISION_GROUP_NONE );
	Vector vecStartPosition = pPlayer->Weapon_ShootPosition();

	int placementMask = MASK_SOLID | (CONTENTS_DEBRIS);

	// Hrm.. Trace entity for portal is taking the 'portal' trace over the world trace which will in some situations
	// hit nothing when a world geo wall is in the way. Trace against the world only for now, and handle portals separately.
	// BUG: cant use the real angles with sweepcollideable... might cause bugs detecting obstructions.
	enginetrace->SweepCollideable( pEntity->GetCollideable(), vecDropPosition, vecStartPosition, vec3_angle, placementMask, &traceFilter, &tr );

	if ( debug_viewmodel_grabcontroller.GetBool() )
	{
		NDebugOverlay::Line( vecStartPosition, vecDropPosition, 200, 0, 0, false, 10.0f );
	}
	
	if ( tr.startsolid )
	{
		if ( tr.allsolid )
		{
			if ( bFinalPass && GameRules() && GameRules()->IsMultiplayer() )
			{
				// Drop it in their face!
				tr.fraction = 1.0f;
				tr.endpos = vecStartPosition;
				vecDropPosition = tr.endpos;

				if ( debug_viewmodel_grabcontroller.GetBool() )
				{
					NDebugOverlay::BoxAngles( vecDropPosition, pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs(), pEntity->CollisionProp()->GetCollisionAngles(), 255, 0, 0, 128, 10.0f );
					//NDebugOverlay::BoxAngles( vecStartPosition, pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs(), pEntity->CollisionProp()->GetCollisionAngles(), 255, 0, 0, 128, 10.0f );
					NDebugOverlay::EntityBounds( pPlayer, 255, 0, 0, 128, 10.0f );
				}
			}
			else
			{
				if ( debug_viewmodel_grabcontroller.GetBool() )
				{
					NDebugOverlay::BoxAngles( vecDropPosition, pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs(), pEntity->CollisionProp()->GetCollisionAngles(), 255, 0, 0, 128, 10.0f );
					//NDebugOverlay::BoxAngles( vecStartPosition, pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs(), pEntity->CollisionProp()->GetCollisionAngles(), 255, 0, 0, 128, 10.0f );
					NDebugOverlay::EntityBounds( pPlayer, 255, 0, 0, 128, 10.0f );
				}

				return false;
			}
		}
		else
		{
			tr.fraction = tr.fractionleftsolid;
			Vector vDelta = tr.endpos - tr.startpos;
			tr.endpos = tr.startpos + vDelta * tr.fraction;
			vecDropPosition = tr.endpos;
		}
	}

	if ( debug_viewmodel_grabcontroller.GetBool() && tr.DidHit() )
	{
		Msg("Found obstruction between player and drop position.\n" );
	}

	// SPECIAL CASE FOR PORTALS
	// For dropping through portals, test the original held distance for any portals
	// that might be closer than whatever obstruction we hit.
	Ray_t ray;
	ray.Init( pPlayer->Weapon_ShootPosition(), pEntity->GetLocalOrigin() );
	// BUG: No equivelant of 'sweepcollideable' through portals... Going to do a non-extruded ray
	// to test for collision in between the box and a potential portal. 
	trace_t trTestObstructionsNearPortals; 
	enginetrace->TraceRay( ray, placementMask, &traceFilter, &trTestObstructionsNearPortals );
	float flWallHitFraction = trTestObstructionsNearPortals.fraction + 0.01f;
	CPortal_Base2D *pPortal = UTIL_Portal_FirstAlongRay( ray, flWallHitFraction );
	bool bHeldOnOppositeSideOfPortal = false;
	// If this held object should drop on the other side of the portal.
	// Put the start and end position on the other side of the portal and run the rest of the
	// safe placement checks.
	if ( trTestObstructionsNearPortals.DidHit() && pPortal )
	{
		if ( debug_viewmodel_grabcontroller.GetBool() )
		{
			Msg("Found portal along drop position, teleporting drop position to the other side\n" );
		}
		// Make the held start point be on the other side of the portal
		float flRayHitFraction = UTIL_IntersectRayWithPortal( ray, pPortal );
		Vector vNewStart;
		Vector vHitPoint = ray.m_Start + ray.m_Delta*flRayHitFraction;
		UTIL_Portal_PointTransform( pPortal->MatrixThisToLinked(), vHitPoint, vNewStart );
		vecStartPosition = vNewStart;

		// Set the drop position to be on the other side of the portal
		Vector vTemp;
		UTIL_Portal_PointTransform( pPortal->MatrixThisToLinked(), pEntity->GetLocalOrigin(), vTemp );
		vecDropPosition = vTemp;

		// This flag is for any further portal-crossing specific modifications to the safe placement checks below
		bHeldOnOppositeSideOfPortal = true;
	}
	else
	{
		if ( debug_viewmodel_grabcontroller.GetBool() )
		{
			Msg("Not reorienting through portal. Trace hit: %s Found Portal %s\n", (tr.DidHit())?("yes"):("no"), (pPortal)?("yes"):("no") );
		}
	}

#if !defined ( CLIENT_DLL )
#if 0
	// If the player is in a portal environment, add this held object to it so it knows to trace properly
	CPortal_Base2D *pPlayerNearPortal = pPlayer->m_hPortalEnvironment.Get();
	CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity );
	if ( pPlayerNearPortal && pSimulator == NULL )
	{
		Assert ( pPortal == NULL || pPortal == pPlayerNearPortal );
		pPlayerNearPortal->m_PortalSimulator.TakeOwnershipOfEntity( pEntity );
		pSimulator = pPlayerNearPortal->m_PortalSimulator.GetLinkedPortalSimulator();
	}
#endif
#endif
	const Vector& stickNormal = pPlayer->GetPortalPlayerLocalData().m_StickNormal;

	// Move outside of the player's box. 
	if ( bHeldOnOppositeSideOfPortal == false )
	{
		Vector vecDisplacement = vecDropPosition - vecStartPosition;
		//vecDisplacement.z = 0;
		Vector forward, right, up;
		pPlayer->GetVectors( &forward, &right, &up );
		Vector vecPlayerBodyForward;
		CrossProduct( stickNormal, right, vecPlayerBodyForward );

		// Project displacement onto player forward
		vecDisplacement = vecPlayerBodyForward * ( vecDisplacement.Dot( vecPlayerBodyForward ) );

		Vector vecMoveDir = vecDisplacement.Normalized();

		Vector radial;
		
		if( pEntity->VPhysicsGetObject() )
		{
			radial = physcollision->CollideGetExtent( pEntity->VPhysicsGetObject()->GetCollide(), vec3_origin, pEntity->GetAbsAngles(), -forward );
		}
		else
		{
			vcollide_t *pVCollide = modelinfo->GetVCollide( pEntity->GetModelIndex() );
			if( pVCollide && (pVCollide->solidCount > 0) )
			{
				radial = physcollision->CollideGetExtent( pVCollide->solids[0], vec3_origin, pEntity->GetAbsAngles(), -forward );
			}
			else
			{
				radial = vec3_origin;
			}
		}

		float flPlayer2DBoundingRadius = 22.62f;
		float flRadius = radial.Length();
		float flOverlap = (flRadius + flPlayer2DBoundingRadius) - vecDisplacement.Length();
		if ( flOverlap < 0 )
			flOverlap = 0;

		vecDropPosition += vecMoveDir * flOverlap;

		//vecDropPosition += stickNormal * 5.0f;

		if ( debug_viewmodel_grabcontroller.GetBool() )
		{
			Msg("Moving drop position out of player's box, move dir: %f %f %f move length %f\n", XYZ(vecMoveDir), flOverlap );
		}
	}

	// One more stuck check, if we're still in solid then don't drop
	pEntity->SetCollisionGroup( m_preVMModeCollisionGroup );
	pEntity->CollisionRulesChanged();
	//UTIL_TraceEntity( pEntity, vecDropPosition, vecDropPosition, placementMask, &tr );
	//TEMP: Ignore portal environments... need to debug this, but right now it's extremley easy to drop the box
	// through the world because it gets simulated in the wrong environment and stays there.
	enginetrace->SweepCollideable( pEntity->GetCollideable(), vecDropPosition, vecDropPosition, vec3_angle, placementMask, &traceFilter, &tr );
	pEntity->SetCollisionGroup( COLLISION_GROUP_DEBRIS_TRIGGER );

	bool bStuckInSolid = false;
	if ( tr.DidHit() && TestIntersectionVsHeldObjectCollide( pEntity, vecDropPosition, tr.m_pEnt ) )
	{
		bStuckInSolid = true;
	}

#if defined ( GAME_DLL )
	if ( bStuckInSolid == false && test_for_vphysics_clips_when_dropping.GetBool() )
	{
		Ray_t toDropPos;
		toDropPos.Init( vecStartPosition, vecDropPosition );
		if ( TestRayVsFuncClipVPhysics( ray ) || TestHeldEntityVsFuncClipVPhysics( pEntity, vecDropPosition ) )
		{
			bStuckInSolid = true;
		}
	}
#endif

	bool bSuccess = ( bStuckInSolid == false ) && ( enginetrace->PointOutsideWorld( vecDropPosition ) == false );

	// Do one more valid placement check making sure this object can make it to the new drop position
	UTIL_TraceLine( vecStartPosition, vecDropPosition, placementMask, &traceFilter, &tr );

	// Place if we can make it to the spot, the spot is clear and is not outside the world.
	bSuccess = bSuccess && !tr.DidHit();

	if ( debug_viewmodel_grabcontroller.GetBool() )
	{
		if ( bSuccess )
		{
			Msg( "Object not stuck at drop position, dropping successfully.\n" );
		}
		else
		{
			Msg( "Object stuck at drop position %f %f %f, refusing to drop object\n", XYZ(vecDropPosition) );
		}

		//NDebugOverlay::EntityBounds( pEntity, (bSuccess)?(0):(255), (bSuccess)?(255):(0), 0, 128, 10.0f );
		NDebugOverlay::BoxAngles( vecDropPosition, pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs(), pEntity->CollisionProp()->GetCollisionAngles(), (bSuccess)?(0):(255), (bSuccess)?(255):(0), 0, 128, 10.0f );
		//NDebugOverlay::BoxAngles( vecStartPosition, pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs(), pEntity->CollisionProp()->GetCollisionAngles(), (bSuccess)?(0):(255), (bSuccess)?(255):(0), 0, 128, 10.0f );
		NDebugOverlay::EntityBounds( pPlayer, (bSuccess)?(0):(255), (bSuccess)?(255):(0), 0, 128, 10.0f );
	}


#if !defined ( CLIENT_DLL )
#if 0
	// When we're done with the traces, remove the object from the portal simulator (if any).
	// When it becomes solid again it will go about portal hole logic in the normal way
	if ( pSimulator )
	{
		pSimulator->ReleaseOwnershipOfEntity( pEntity );
	}
#endif
#endif 

	if ( bSuccess && pVecPosition )
		*pVecPosition = vecDropPosition;
	return bSuccess;

}

void CGrabController::AttachEntityVM( CBasePlayer *pPlayer, CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon, const Vector &vGrabPosition, bool bUseGrabPosition )
{
	if ( !WantsVMGrab( pPlayer ) )
	{
		return;
	}

	CPortal_Player *pPortalPlayer = (CPortal_Player*)pPlayer;
	pPortalPlayer->SetUsingVMGrabState( true );

	Assert( pEntity && pPhys );
	if ( !pEntity || !pPhys )
		return;

	//NOTE: This is really here just for picking up objects across portals...
	// this will prevent them from teleporting across the world. Could handle this
	// as a special case if for some reason we want to 'lerp' into the player's hands.
	UpdateObject( pPlayer, 12.0f );
#if defined( CLIENT_DLL )
	if( pEntity->GetPredictable() )
#endif
	{
		pEntity->Teleport( &m_shadow.targetPosition, &m_shadow.targetRotation, NULL );
	}

#if !defined ( CLIENT_DLL )
	m_preVMModeCollisionGroup = pEntity->GetCollisionGroup();
	pEntity->SetCollisionGroup( COLLISION_GROUP_DEBRIS_TRIGGER );
	m_oldTransmitState = pEntity->GetTransmitState();
	pEntity->VPhysicsGetObject()->EnableCollisions( false );
	pEntity->SetTransmitState( FL_EDICT_ALWAYS );
	m_bOldShadowState = ( pEntity->GetEffects() & EF_NOSHADOW ) == false;
	pEntity->AddEffects( EF_NOSHADOW );
	if ( GameRules()->IsMultiplayer() )
	{
		pEntity->AddEffects( EF_NODRAW );
	}
	CBaseAnimating* pAnim = pEntity->GetBaseAnimating();
	Assert ( pAnim );
	if ( pAnim )
	{
		m_hOldLightingOrigin = pAnim->GetLightingOrigin();
		pAnim->SetLightingOrigin( pPlayer );
	}
#endif
}

bool CGrabController::DetachEntityVM( bool bClearVelocity )
{
#if defined ( CLIENT_DLL )
	// Server is in charge of detaching... when it does it will network down
	// the change in held object and the client will react then.
	return false;
#else
	CBaseEntity *pEntity = GetAttached();
	Assert ( pEntity );
	if ( !pEntity )
	{
		if( m_controller )
		{
			if ( physenv != NULL )
			{
				physenv->DestroyMotionController( m_controller );
			}
			else
			{
				Warning( "%s(%d): Trying to dereference NULL physenv.\n", __FILE__, __LINE__ );
			}
			m_controller = NULL;
		}
		return true;
	}

	CPortal_Player *pPlayer = (CPortal_Player*)GetPlayerHoldingEntity( pEntity );
	if ( !pPlayer )
	{
		Assert( 0 );
		return false;
	}

	Vector vecDropPosition = pEntity->GetAbsOrigin();

	bool bFoundSafePlacementLocation = FindSafePlacementLocation( &vecDropPosition );

	if ( !bFoundSafePlacementLocation )
	{
		// Scoot it up and try again
		Vector vScootFromOriginal = pEntity->GetAbsOrigin() + Vector( 0.0f, 0.0f, ( pEntity->CollisionProp()->OBBMaxs().z - pEntity->CollisionProp()->OBBMins().z ) * 0.5f );
		bFoundSafePlacementLocation = FindSafePlacementLocation( &vScootFromOriginal );

		if ( bFoundSafePlacementLocation )
		{
			vecDropPosition = vScootFromOriginal;
		}
		else
		{
			// One more try... scoot up more
			vecDropPosition = pEntity->GetAbsOrigin() + Vector( 0.0f, 0.0f, pEntity->CollisionProp()->OBBMaxs().z - pEntity->CollisionProp()->OBBMins().z );
			bFoundSafePlacementLocation = FindSafePlacementLocation( &vecDropPosition, true );
		}
	}

	if ( !bFoundSafePlacementLocation )
	{
		if ( !pPlayer->IsForcingDrop()  )
		{
			ShowDenyPlacement();
			return false;
		}
#if !defined ( CLIENT_DLL )
		else if ( GameRules()->IsMultiplayer() && V_strcmp( "mp_coop_rat_maze", STRING( gpGlobals->mapname ) ) == 0 )
		{
			// 83939: We need to drop this (probably due to player death) but it's in solid.
			CBaseEntity* pEntInSafePlace = gEntList.FindEntityByName( NULL, "target_spawn_ratmaze_box" );
			if ( pEntInSafePlace )
			{
				vecDropPosition = pEntInSafePlace->GetAbsOrigin();
			}
		}
		else if ( GameRules()->IsMultiplayer() && V_strcmp( "mp_coop_tbeam_maze", STRING( gpGlobals->mapname ) ) == 0 )
		{
			// 85025: ..aaaand another one.
			CBaseEntity* pEntInSafePlace = gEntList.FindEntityByName( NULL, "dropper-proxy" );
			if ( pEntInSafePlace )
			{
				vecDropPosition = pEntInSafePlace->GetAbsOrigin();
			}
		}
#endif
	}

	pPlayer->SetUsingVMGrabState( false );

	pEntity->SetCollisionGroup( m_preVMModeCollisionGroup );
	pEntity->VPhysicsGetObject()->EnableCollisions( true );
	pEntity->SetTransmitState( m_oldTransmitState );
	if ( m_bOldShadowState )
	{
		pEntity->RemoveEffects( EF_NOSHADOW );
	}
	if ( GameRules()->IsMultiplayer() )
	{
		pEntity->RemoveEffects( EF_NODRAW );
	}
	CBaseAnimating* pAnim = pEntity->GetBaseAnimating();
	Assert ( pAnim );
	if ( pAnim )
	{
		pAnim->SetLightingOrigin( m_hOldLightingOrigin );
	}

	IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
	if ( !pPhys )
	{
		Assert( 0 );
		return false;
	}

	Vector vVelocity; 
	pPhys->GetVelocity( &vVelocity, NULL );
	pEntity->Teleport( &vecDropPosition, NULL, &vVelocity );
	pEntity->SetLocalVelocity( vec3_origin );
	return true;
#endif
}

#if defined( CLIENT_DLL )
void CGrabController::DetachUnknownEntity( void )
{
	if( m_controller )
	{
		physenv->DestroyMotionController( m_controller );
		m_controller = NULL;
	}
	m_attachedEntity = NULL;
}
#endif

bool CGrabController::UpdateObjectVM( CBasePlayer *pPlayer, float flError )
{
	CBaseEntity *pEnt = GetAttached();
	CPortal_Player *pPortalPlayer = (CPortal_Player*)pPlayer;
	m_hHoldingPlayer = pPortalPlayer;

#if defined( GAME_DLL )
	if( pPortalPlayer )
	{
		pPortalPlayer->m_GrabControllerPersistentVars.ResetOscillationWatch();
	}
#endif

	Assert ( pEnt );
	if ( !pEnt )
	{
		return false;
	}
#if !defined ( CLIENT_DLL )
	CBaseAnimating *pAnimating = (CBaseAnimating*)pEnt;
	if ( pAnimating->IsDissolving() )
	{
		return false;
	}
#endif 

	QAngle playerAngles = pPlayer->EyeAngles();
	Vector forward, right, up;
	AngleVectors( playerAngles, &forward, &right, &up );
	Vector start = pPlayer->Weapon_ShootPosition();

	float pitch = AngleDistance(playerAngles.x,0);
	float flUpOffset = RemapValClamped( fabs(pitch), 0.0f, 75.0f, 1.0f, 0.0f ) * GetObjectOffset( pEnt );
	// As we look down, hold the box more forward so it doesn't go under our 'feet'
	// Also doing this for 'up' so we can see the ceiling
	float scale = RemapValClamped( fabs(pitch), 0.0f, 75.0f, 0.0f, 1.0f );
	float flHoldDistAdjust = player_held_object_look_down_adjustment.GetFloat() * scale;
	const Vector& stickNormal = pPortalPlayer->GetPortalPlayerLocalData().m_StickNormal;
	Vector vecPlayerBodyForward;
	CrossProduct( stickNormal, right, vecPlayerBodyForward );


	Vector vHoldOffset = forward * GetObjectDistance() + // Move a bit out of our face
						vecPlayerBodyForward * flHoldDistAdjust + // As we look down move forward so it doesnt appear 'under' us
						up * flUpOffset; // move down out of our face

	// Run some traces to have a faux reaction to physical objects in the world
	Ray_t ray;
	ray.Init( start, start + vHoldOffset );
	trace_t	tr;
#if defined ( CLIENT_DLL )

	//filter out the player and every version of this object
	CTraceFilterSimpleList traceFilter( COLLISION_GROUP_DEBRIS );
	{
		traceFilter.AddEntityToIgnore( pPlayer );
		traceFilter.AddEntityToIgnore( pEnt );
		if( ((CPortal_Player *)pPlayer)->m_pHeldEntityClone )
		{
			if( (((CPortal_Player *)pPlayer)->m_pHeldEntityClone == pEnt) && //this ent is the held clone
				((C_PlayerHeldObjectClone *)pEnt)->m_hOriginal ) //clone has original
			{
				traceFilter.AddEntityToIgnore( ((C_PlayerHeldObjectClone *)pEnt)->m_hOriginal );
			}
			else
			{
				traceFilter.AddEntityToIgnore( ((CPortal_Player *)pPlayer)->m_pHeldEntityClone );
			}			
		}
		if( ((CPortal_Player *)pPlayer)->m_pHeldEntityThirdpersonClone )
		{
			traceFilter.AddEntityToIgnore( ((CPortal_Player *)pPlayer)->m_pHeldEntityThirdpersonClone );
		}
	}
#else
	CTraceFilterSkipTwoEntities traceFilter( pPlayer, pEnt, COLLISION_GROUP_DEBRIS );
#endif
	UTIL_Portal_TraceRay( ray, MASK_SOLID, &traceFilter, &tr );

	// reduce some of the forward held distance
	float offsetscale = tr.fraction;
	Vector vOffsetDir = vHoldOffset.Normalized();
	float flOffsetDist = vHoldOffset.Length();
	float scaledLength = flOffsetDist * offsetscale;
	if ( scaledLength < player_held_object_min_distance.GetFloat() )
	{
		scaledLength = player_held_object_min_distance.GetFloat();
	}

	vHoldOffset = vOffsetDir * scaledLength;

	Vector end = start + vHoldOffset;

#if !defined ( CLIENT_DLL )
	// Send these down to the client
	pPortalPlayer->m_vecCarriedObjectAngles = m_attachedAnglesPlayerSpace;
#endif

	QAngle angles = TransformAnglesFromPlayerSpace( m_attachedAnglesPlayerSpace, pPlayer );

	matrix3x4_t eyeMatrix;
	AngleMatrix( pPlayer->EyeAngles(), eyeMatrix );
	
	// If it has a preferred orientation, update to ensure we're still oriented correctly.
	Pickup_GetPreferredCarryAngles( pEnt, pPlayer, eyeMatrix, angles ); 

	// We may be holding a prop that has preferred carry angles
	if ( m_bHasPreferredCarryAngles )
	{
		matrix3x4_t tmp;
		ComputePlayerMatrix( pPlayer, tmp );
		angles = TransformAnglesToWorldSpace( m_vecPreferredCarryAngles, tmp );
	}


#if !defined ( CLIENT_DLL )
	AngularImpulse angImpulse;
	Vector vel;
	IPhysicsObject *pPhys = pEnt->VPhysicsGetObject();
	if ( !pPhys )
	{
		return false;
	}
	pPhys->GetVelocity( &vel, &angImpulse );
	pEnt->SetLocalVelocity( vel );
	
	// Don't let anything change the transmit state back to PVS_CHECK or we'll
	// start disappearing when going through portals or standing near walls.
	// HACK: This isn't ideal... maybe add a test in baseentity::UpdateTransmitState? Hard to do this for any
	// possible held object and guarentee it'll get called because they dont all chain to base
	pEnt->SetTransmitState( FL_EDICT_ALWAYS );
#endif 

	// If we use-denied, wiggle a bit to give feedback
	if ( m_flAngleOffset > 0 || m_flLengthOffset > 0 )
	{
		float flOutput = sin( 20 * ( gpGlobals->curtime + m_flTimeOffset ) );
		float flCurAngleOffset = m_flAngleOffset * flOutput;
		float flCurLengthOffset = m_flLengthOffset * flOutput;
		angles.y += flCurAngleOffset;
		Vector vHoldDir = vHoldOffset.Normalized();
		end += vHoldDir * flCurLengthOffset;

		float flDecay = ExponentialDecay( 0.5, 0.1, gpGlobals->frametime );
		m_flAngleOffset *= flDecay;
		m_flLengthOffset *= flDecay;

		if ( m_flAngleOffset < 0.1f )
			m_flAngleOffset = 0;

		if ( m_flLengthOffset < 0.1f )
			m_flLengthOffset = 0;
	}


	SetTargetPosition( end, angles );

	// Keep the object with this player even if they're moving too fast
	m_fPlayerSpeed = pPlayer->GetAbsVelocity().Length();

	PushNearbyTurrets();

	return true;
}

void CGrabController::PushNearbyTurrets( void )
{
	CTraceFilterSkipTwoEntities traceFilter( m_hHoldingPlayer, m_attachedEntity, COLLISION_GROUP_NONE );
	trace_t tr;
	enginetrace->SweepCollideable( m_attachedEntity->GetCollideable(), m_hHoldingPlayer->EyePosition(), m_shadow.targetPosition, vec3_angle, MASK_SOLID, &traceFilter, &tr );

	if ( tr.m_pEnt && FClassnameIs( tr.m_pEnt, "npc_portal_turret_floor" ) )
	{
		// Apply fake force to feign 'knocking over' the turret.
		Vector vHeldObjectVelocity = m_attachedEntity->GetAbsVelocity();

		// held objects 'snap' to position so their velocities can be incredibly high.
		// cap it at some sane max amount;
		float flMaxMag = player_held_object_max_knock_magnitude.GetFloat();

		if ( vHeldObjectVelocity.LengthSqr() > flMaxMag*flMaxMag )
		{
			vHeldObjectVelocity.NormalizeInPlace();
			vHeldObjectVelocity *= flMaxMag;
		}

		tr.m_pEnt->ApplyAbsVelocityImpulse( vHeldObjectVelocity );
	}
}


void CGrabController::ShowDenyPlacement( void )
{
	m_flAngleOffset = 10.0f;
	m_flLengthOffset = 5.0f;
	// Always start at (well, close to) sin( x*pi ) so we don't pop positions/angles
	m_flTimeOffset = -fmod( gpGlobals->curtime, 180.0f );
}
//////////////////////////////////////////////////////////////////////////
// C_PlayerHeldObjectClone
//////////////////////////////////////////////////////////////////////////
#if defined ( CLIENT_DLL )
LINK_ENTITY_TO_CLASS_CLIENTONLY( player_held_object_clone, C_PlayerHeldObjectClone );

C_PlayerHeldObjectClone::~C_PlayerHeldObjectClone()
{
	DestroyShadow();
	DestroyModelInstance();
	VPhysicsDestroyObject();
	Term();
}

bool C_PlayerHeldObjectClone::InitClone( C_BaseEntity *pObject, C_BasePlayer *pPlayer, bool bIsViewModel, C_PlayerHeldObjectClone *pVMToFollow )
{
	if ( !pObject )
		return false;

	m_hPlayer = pPlayer;
	m_hOriginal = pObject;
	m_pVMToFollow = pVMToFollow;

	const char *pModelName = modelinfo->GetModelName( pObject->GetModel() );

	Assert ( pModelName );

	if ( !pModelName )	
		return false;

	if ( InitializeAsClientEntity( pModelName, bIsViewModel ) == false )
	{
		return false;
	}

	m_bCanUseFastPath = false;

	SetAbsOrigin( pObject->GetAbsOrigin() );

	// This isn't needed... The old grab code assumes a physics object so
	// while we want to have the option of both types we need a 'dummy' object.
	solid_t tmpSolid;
	PhysModelParseSolid( tmpSolid, this, GetModelIndex() );
	SetSolid( SOLID_VPHYSICS );
	m_pPhysicsObject = VPhysicsInitShadow( false, false, &tmpSolid );

	if ( m_pPhysicsObject )
	{
		PhysSetGameFlags( m_pPhysicsObject, FVPHYSICS_PLAYER_HELD );
		m_pPhysicsObject->EnableCollisions( !bIsViewModel );
		SetCollisionGroup( COLLISION_GROUP_DEBRIS_TRIGGER );
	}
	
	if ( !bIsViewModel )
	{
		// make ghost renderables
		SetMoveType( MOVETYPE_CUSTOM );
	}
	else
	{
		// Let the non view model cast the shadow
		AddEffects( EF_NOSHADOW );
	}

	const model_t *mod = GetModel();
	if ( mod )
	{
		Vector mins, maxs;
		modelinfo->GetModelBounds( mod, mins, maxs );
		SetCollisionBounds( mins, maxs );
	}

	OnDataChanged( DATA_UPDATE_CREATED );
	CollisionProp()->UpdatePartition();
	CreateShadow();

	CreateModelInstance();
	pObject->SnatchModelInstance( this );

	m_nOldSkin = pObject->GetSkin();
	SetSkin( m_nOldSkin );

	m_bOnOppositeSideOfPortal = false;
	
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	return true;
}

void C_PlayerHeldObjectClone::ClientThink()
{
	if ( m_hOriginal.Get() )
	{
		if ( m_nOldSkin != m_hOriginal->GetSkin() )
		{
			m_nOldSkin = m_hOriginal->GetSkin();
			SetSkin( m_nOldSkin );
		}
	}

	if ( m_pVMToFollow )
	{
		SetAbsOrigin( m_pVMToFollow->GetAbsOrigin() );
		SetAbsAngles( m_pVMToFollow->GetAbsAngles() );

		m_bOnOppositeSideOfPortal = false;

		CPortal_Player *pPlayer = (CPortal_Player *)m_hPlayer.Get();

		if ( pPlayer )
		{
			Vector vForward;
			m_hPlayer->EyeVectors( &vForward, NULL, NULL );
			vForward.z = 0.0f;
			VectorNormalize( vForward );

			Vector vEyePos = m_hPlayer->EyePosition();
			Vector vStart( vEyePos.x, vEyePos.y, GetAbsOrigin().z );

			Ray_t ray;
			ray.Init( vStart, GetAbsOrigin() );
			float fCloser = 2.0f;
			CPortal_Base2D *pPortal = UTIL_Portal_FirstAlongRay( ray, fCloser );
			if( pPortal == NULL )
			{
				pPortal = pPlayer->m_hPortalEnvironment;
			}
			ray.Init( vStart + vForward * 20.f, GetAbsOrigin(), GetCollideable()->OBBMins(), GetCollideable()->OBBMaxs() );

			CTraceFilterSkipTwoEntities filter;
			filter.SetPassEntity( m_hPlayer );
			filter.SetPassEntity2( m_hOriginal );

			trace_t tr;
			UTIL_Portal_TraceRay_With( pPortal, ray, MASK_SOLID, &filter, &tr );

			if ( tr.startsolid )
			{
				tr.endpos = m_hPlayer->EyePosition();
			}

			Vector vNewPos = tr.endpos;
			float fZDiff = GetAbsOrigin().z - vNewPos.z;

			if ( fZDiff > 0.0f && tr.plane.normal.z > -0.5f || fZDiff < 0.0f && tr.plane.normal.z < 0.5f )
			{
				vNewPos.z += fZDiff * 0.5f; // Move us halfway to the intended z pos
			}

			if ( pPortal )
			{
				Vector vTracePos = vNewPos;

				// Make sure the center has passed through portal
				ray.Init( m_hPlayer->EyePosition(), vTracePos );
				pPortal = UTIL_Portal_TraceRay( ray, MASK_SOLID, &filter, &tr );
				if ( pPortal )
				{
					m_bOnOppositeSideOfPortal = true;

					QAngle angTransformed;
					UTIL_Portal_AngleTransform( pPortal->m_matrixThisToLinked, GetAbsAngles(), angTransformed );
					SetAbsAngles( angTransformed );

					UTIL_Portal_PointTransform( pPortal->m_matrixThisToLinked, vTracePos, vNewPos );
				}
			}

			SetAbsOrigin( vNewPos );
		}
	}
	else //!m_pVMToFollow
	{
		C_BasePlayer *pOwner = m_hPlayer;
		if( pOwner )
		{
			Vector vInterpolatedOrigin, vForward, vUp, vRight;
			pOwner->EyePositionAndVectors( &vInterpolatedOrigin, &vForward, &vRight, &vUp );
			vInterpolatedOrigin += vForward * m_vPlayerRelativeOrigin.x;
			vInterpolatedOrigin += vRight * m_vPlayerRelativeOrigin.y;
			vInterpolatedOrigin += vUp * m_vPlayerRelativeOrigin.z;
			SetAbsOrigin( vInterpolatedOrigin );
		}
	}
}


bool C_PlayerHeldObjectClone::OnInternalDrawModel( ClientModelRenderInfo_t *pInfo )
{
	Assert ( m_hPlayer.Get() );
	if ( m_hPlayer.Get() )
	{
		pInfo->pLightingOrigin = &(m_hPlayer->GetAbsOrigin());
	}
	return true;
}

int C_PlayerHeldObjectClone::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( IsRenderingWithViewModels() )
	{
		if ( m_hPlayer.Get() && m_hPlayer.Get() == C_BasePlayer::GetLocalPlayer() )
		{
			if ( g_pPortalRender->GetViewRecursionLevel() > 0 )
				return 0;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if ( m_hPlayer.Get() && (m_hPlayer.Get() == C_BasePlayer::GetLocalPlayer()) && !m_hPlayer->ShouldDrawLocalPlayer() )
		{
			if ( g_pPortalRender->GetViewRecursionLevel() < 1 )
			{
				if ( !m_bOnOppositeSideOfPortal )
					return 0;
			}
			else if ( g_pPortalRender->GetViewRecursionLevel() < 2 )
			{
				if ( m_bOnOppositeSideOfPortal )
					return 0;
			}
		}
	}

	return BaseClass::DrawModel( flags, instance );
}

#if 0
void C_PlayerHeldObjectClone::GetColorModulation( float* color )
{
	if( m_pVMToFollow )
	{
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 1.0f;
	}
	else
	{
		color[0] = 0.0f;
		color[1] = 1.0f;
		color[2] = 0.0f;
		return;
	}
}
#endif


bool C_PlayerHeldObjectClone::HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer )
{
	if ( m_hOriginal.Get() )
	{
		IPlayerPickupVPhysics *pPickupPhys = dynamic_cast<IPlayerPickupVPhysics *>(m_hOriginal.Get());
		if ( pPickupPhys )
		{
			return pPickupPhys->HasPreferredCarryAnglesForPlayer( pPlayer );
		}
	}

	return CDefaultPlayerPickupVPhysics::HasPreferredCarryAnglesForPlayer( pPlayer );
}

QAngle C_PlayerHeldObjectClone::PreferredCarryAngles( void )
{
	if ( m_hOriginal.Get() )
	{
		IPlayerPickupVPhysics *pPickupPhys = dynamic_cast<IPlayerPickupVPhysics *>(m_hOriginal.Get());
		if ( pPickupPhys )
		{
			return pPickupPhys->PreferredCarryAngles();
		}
	}

	return CDefaultPlayerPickupVPhysics::PreferredCarryAngles();
}

#endif



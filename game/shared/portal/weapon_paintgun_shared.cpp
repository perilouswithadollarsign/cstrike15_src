//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared variables, etc. for the paint gun.
//
//=============================================================================//

#include "cbase.h"
#include "paint_color_manager.h"
#include "shot_manipulator.h"
#include "in_buttons.h"
#include "debugoverlay_shared.h"
#include "weapon_paintgun_shared.h"
#include "paint_sprayer_shared.h"
#include "paint_stream_shared.h"


#ifdef CLIENT_DLL
	#include "c_weapon_paintgun.h"
	#include "c_portal_player.h"
	#include "igameevents.h"
#else
	#include "weapon_paintgun.h"
	#include "portal_player.h"
	#include "paint_database.h"
	#include "portal_base2d.h"
	#include "prop_portal_shared.h"
	#include "env_speaker.h"
	#include "rumble_shared.h"
	#include "paint_database.h"

	//ConVar sv_paint_erase_range("sv_paint_erase_range", "2000", FCVAR_CHEAT);
	//ConVar sv_num_erase_ray("sv_num_erase_ray", "10", FCVAR_CHEAT, "number of ray that shoots out per shot");
	//ConVar sv_debug_suck_erase("sv_debug_suck_erase", "0", FCVAR_CHEAT);

	extern void Paint( CBaseEntity* pEntity, const Vector& pos, uint8 colorIndex, int nPainted );
	extern CPaintDatabase PaintDatabase;

#endif


#define paintgun_blobs_spread_radius			0.f		//ConVar paintgun_blobs_spread_radius( "paintgun_blobs_spread_radius", "0.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The starting radius of the spread of the paint blobs from the gun" );
#define paintgun_blobs_spread_angle				10.f	//ConVar paintgun_blobs_spread_angle( "paintgun_blobs_spread_angle", "10.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The spread (in degrees) of the paint blobs from the gun" );
#define paintgun_blobs_per_second				40.f	//ConVar paintgun_blobs_per_second( "paintgun_blobs_per_second", "40.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "Number of blobs shot out of the paint gun per second" );
#define paintgun_blobs_min_speed				950.f	//ConVar paintgun_blobs_min_speed( "paintgun_blobs_min_speed", "950.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The min speed of the blobs shot out of the paint gun" );
#define paintgun_blobs_max_speed				1050.f	//ConVar paintgun_blobs_max_speed( "paintgun_blobs_max_speed", "1050.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The max speed of the blobs shot out of the paint gun" );
#define paintgun_shoot_position_trace_for_wall	1		//ConVar paintgun_shoot_position_trace_for_wall( "paintgun_shoot_position_trace_for_wall", "1", FCVAR_REPLICATED, "If the paint gun shooting position should test if it is inside a wall" );

#define paintgun_blobs_streak_percent			10.f	//ConVar paintgun_blobs_streak_percent( "paintgun_blobs_streak_percent", "10.0f", FCVAR_REPLICATED | FCVAR_CHEAT );
#define paintgun_blobs_min_streak_time			0.1f	//ConVar paintgun_blobs_min_streak_time( "paintgun_blobs_min_streak_time", "0.1f", FCVAR_REPLICATED | FCVAR_CHEAT );
#define paintgun_blobs_max_streak_time			0.5f	//ConVar paintgun_blobs_max_streak_time( "paintgun_blobs_max_streak_time", "0.5f", FCVAR_REPLICATED | FCVAR_CHEAT );
#define paintgun_blobs_min_streak_speed_dampen	4500.f	//ConVar paintgun_blobs_min_streak_speed_dampen( "paintgun_blobs_min_streak_speed_dampen", "4500.0f", FCVAR_REPLICATED | FCVAR_CHEAT );
#define paintgun_blobs_max_streak_speed_dampen	5500.f	//ConVar paintgun_blobs_max_streak_speed_dampen( "paintgun_blobs_max_streak_speed_dampen", "5500.0f", FCVAR_REPLICATED | FCVAR_CHEAT );

#define paintgun_max_ammo 60 //ConVar paintgun_max_ammo( "paintgun_max_ammo", "60", FCVAR_REPLICATED, "The maximum amount of paint ammo allowed." );
#define paintgun_ammo_type 0 //ConVar paintgun_ammo_type( "paintgun_ammo_type", "0", FCVAR_REPLICATED, "Type of paint ammo. 0: No ammo, 1: Global ammo per-gun, 2: Ammo per-paint type" );



acttable_t	CWeaponPaintGun::m_acttable[] = 
{
	{ ACT_MP_STAND_IDLE,				ACT_MP_STAND_PRIMARY,					false },
	{ ACT_MP_RUN,						ACT_MP_RUN_PRIMARY,						false },
	{ ACT_MP_CROUCH_IDLE,				ACT_MP_CROUCH_PRIMARY,					false },
	{ ACT_MP_CROUCHWALK,				ACT_MP_CROUCHWALK_PRIMARY,				false },
	{ ACT_MP_JUMP_START,				ACT_MP_JUMP_START_PRIMARY,				false },
	{ ACT_MP_JUMP_FLOAT,				ACT_MP_JUMP_FLOAT_PRIMARY,				false },
	{ ACT_MP_JUMP_LAND,					ACT_MP_JUMP_LAND_PRIMARY,				false },
	{ ACT_MP_AIRWALK,					ACT_MP_AIRWALK_PRIMARY,					false },
	{ ACT_MP_RUN_SPEEDPAINT,			ACT_MP_RUN_SPEEDPAINT_PRIMARY,			false },
	{ ACT_MP_DROWNING_PRIMARY,			ACT_MP_DROWNING_PRIMARY,				false },
	{ ACT_MP_LONG_FALL,					ACT_MP_LONG_FALL_PRIMARY,				false },
	{ ACT_MP_TRACTORBEAM_FLOAT,			ACT_MP_TRACTORBEAM_FLOAT_PRIMARY,		false },
	{ ACT_MP_DEATH_CRUSH,				ACT_MP_DEATH_CRUSH_PRIMARY,				false },
};

IMPLEMENT_ACTTABLE(CWeaponPaintGun);

void CWeaponPaintGun::ItemPostFrame()
{
	bool bWasFiringPaint = m_bFiringPaint;
	bool bWasFiringErase = m_bFiringErase;

	// Only the player fires this way so we can cast
	CPortal_Player *pPlayer = ToPortalPlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	// The paint clearing secondary function can always be used
	if( paintgun_ammo_type != PAINT_AMMO_NONE &&
		(pPlayer->m_nButtons & IN_ATTACK2) != 0 )
	{
		// Attack!
		SecondaryAttack();
	}
	else if( pPlayer->GetUseEntity() == NULL )
	{
		BaseClass::ItemPostFrame();
	}

	// Was shooting neither and is now shooting either
	if( !bWasFiringPaint && !bWasFiringErase && 
	   ( m_bFiringPaint || m_bFiringErase ) )
	{
#if !defined (CLIENT_DLL)
		StartShootingSound();
#else

		pPlayer->SetAnimation( PLAYER_ATTACK1 );
#endif
	}
	// Was shooting either and now is shooting neither
	else if( ( bWasFiringPaint || bWasFiringErase ) &&
			 ( !m_bFiringPaint && !m_bFiringErase ) )
	{
#if !defined (CLIENT_DLL)
		StopShootingSound();
#else

#endif
	}
}

void CWeaponPaintGun::PrimaryAttack()
{
	bool bHasSelectedColor = false;
#if !defined (CLIENT_DLL)
	bHasSelectedColor = HasPaintPower( (PaintPowerType)m_nCurrentColor.Get() );
#else // CLIENT_DLL
	bHasSelectedColor = HasPaintPower( (PaintPowerType)m_nCurrentColor );
#endif

	// Don't shoot if we dont have the selected color or any color at all
	if( !HasAnyPaintPower() || !bHasSelectedColor || !HasPaintAmmo( m_nCurrentColor ) )
	{
		m_bFiringPaint = m_bFiringErase = false;
		return;
	}


#if !defined (CLIENT_DLL)
	SprayPaint( gpGlobals->frametime, m_nCurrentColor );
	if( !m_bFiringPaint )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_painted" );
		if ( event )
		{

			CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
			assert( pPlayer );

			event->SetInt("userid", pPlayer->GetUserID() );

			gameeventmanager->FireEvent( event );
		}
	}
#else // CLIENT_DLL
	StartHoseEffect();
	//SprayPaint( gpGlobals->frametime, static_cast<PaintPowerType>( m_nCurrentColor ) );
#endif //CLIENT_DLL


	m_bFiringPaint = true;
	m_bFiringErase = false;
}


void CWeaponPaintGun::SecondaryAttack()
{
	if( paintgun_ammo_type == PAINT_AMMO_NONE )
	{
#	ifdef CLIENT_DLL
		StartHoseEffect();
#	else
		if( !m_bFiringErase )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "player_erased" );
			if ( event )
			{
				CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
				assert( pPlayer );

				event->SetInt("userid", pPlayer->GetUserID() );

				gameeventmanager->FireEvent( event );
			}
		}

		m_bFiringPaint = false;
		m_bFiringErase = true;

		SprayPaint(  gpGlobals->frametime, NO_POWER );
#	endif
	}
	else
	{
		ResetAmmo();
#ifdef GAME_DLL
		PaintDatabase.RemoveAllPaint();
#endif
	}
}


bool CWeaponPaintGun::HasPaintPower( PaintPowerType nIndex )
{
	return m_bHasPaint[nIndex];
}


bool CWeaponPaintGun::HasAnyPaintPower()
{
	for( int i = 0; i < PAINT_POWER_TYPE_COUNT; ++i )
	{
		if( HasPaintPower( (PaintPowerType)i ) )
		{
			return true;
		}
	}

	return false;
}


void CWeaponPaintGun::WeaponIdle()
{
#ifdef CLIENT_DLL
	StopHoseEffect();
#else
	if( m_bFiringPaint || m_bFiringErase )
	{
		StopShootingSound();
	}
#endif

	m_bFiringPaint = m_bFiringErase = false;
	m_flAccumulatedTime = 1.0f/paintgun_blobs_per_second;
#ifdef CLIENT_DLL
#endif

	m_nBlobRandomSeed = 0;

	BaseClass::WeaponIdle();
}


bool CWeaponPaintGun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bFiringPaint = m_bFiringErase = false;

#ifdef CLIENT_DLL
	ChangeRenderColor();
	StopHoseEffect();
#else
	StopShootingSound();

	IGameEvent *event = gameeventmanager->CreateEvent( "holstered_paintgun" );
	if ( event )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if( pPlayer )
		{
			event->SetInt("userid", pPlayer->GetUserID() );

			gameeventmanager->FireEvent( event );
		}
	}
#endif

	return BaseClass::Holster( pSwitchingTo );
}


void CWeaponPaintGun::Drop( const Vector &vecVelocity )
{
	m_bFiringPaint = m_bFiringErase = false;

	Color color = MapPowerToVisualColor( m_nCurrentColor );
	if ( !HasAnyPaintPower() )
		color = MapPowerToVisualColor( NO_POWER );

#ifdef CLIENT_DLL
	StopHoseEffect();
#else
	StopShootingSound();

	IGameEvent *event = gameeventmanager->CreateEvent( "dropped_paintgun" );
	if ( event )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if( pPlayer )
		{
			event->SetInt("userid", pPlayer->GetUserID() );

			gameeventmanager->FireEvent( event );
		}
	}
#endif

	SetRenderColor( color.r(), color.g(), color.b() );

	BaseClass::Drop( vecVelocity );
}


bool CWeaponPaintGun::Deploy()
{
#ifdef CLIENT_DLL
	ChangeRenderColor();
#endif

#ifndef CLIENT_DLL
	IGameEvent *event = gameeventmanager->CreateEvent( "deployed_paintgun" );
	if ( event )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if( pPlayer )
		{
			event->SetInt("userid", pPlayer->GetUserID() );
			event->SetInt("paintcount", GetPaintCount() );

			gameeventmanager->FireEvent( event );
		}
	}
#endif //Only on server

	return BaseClass::Deploy();
}


void CWeaponPaintGun::SetSubType( int iType )
{ 
	m_iSubType = iType;
	m_nCurrentColor = iType;

#ifdef CLIENT_DLL
	ChangeRenderColor();
#else
	EmitSound( "Player.WeaponSelected" );
#endif

	BaseClass::SetSubType( iType );
}


PaintPowerType CWeaponPaintGun::GetCurrentPaint()
{
#ifdef CLIENT_DLL
	return (PaintPowerType)( m_nCurrentColor );
#else //!CLIENT_DLL
	return (PaintPowerType)( m_nCurrentColor.Get() );
#endif
}


//Paint Ammo!
bool CWeaponPaintGun::HasPaintAmmo( unsigned paintType ) const
{
	switch( paintgun_ammo_type )
	{
		case PAINT_AMMO_NONE:
			return true;

		case PAINT_AMMO_GLOBAL:
			return m_nPaintAmmo > 0;

		case PAINT_AMMO_PER_TYPE:
			Assert( paintType < PAINT_POWER_TYPE_COUNT );
			return m_PaintAmmoPerType[MIN( paintType, PAINT_POWER_TYPE_COUNT )] > 0;

		default:
			return true;
	}
}

void CWeaponPaintGun::DecrementPaintAmmo( unsigned paintType )
{
	switch( paintgun_ammo_type )
	{
		case PAINT_AMMO_GLOBAL:
			--m_nPaintAmmo;
			break;

		case PAINT_AMMO_PER_TYPE:
			const int index = MIN( paintType, PAINT_POWER_TYPE_COUNT );
			m_PaintAmmoPerType.Set( index, m_PaintAmmoPerType[index] - 1 );
			break;
	}
}

void CWeaponPaintGun::ResetAmmo()
{
	m_nPaintAmmo = paintgun_max_ammo;

	const int maxAmmo = paintgun_max_ammo;
	for( int i = 0; i < PAINT_POWER_TYPE_COUNT; ++i )
	{
		m_PaintAmmoPerType.Set( i, maxAmmo );
	}
}

void CWeaponPaintGun::SprayPaint( float flDeltaTime, int paintType )
{
	if( flDeltaTime <= 0.0f )
	{
		return;
	}

	CPortal_Player *pOwner = ToPortalPlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	CPaintStream *pPaintStream = assert_cast< CPaintStream* >( m_hPaintStream.Get( paintType ).Get() );
	if ( !pPaintStream )
		return;

	m_flAccumulatedTime += flDeltaTime;

	Vector vecEyePosition = pOwner->EyePosition();
	Vector vecVelocity = pOwner->GetAbsVelocity();
	Vector vecAimDir = pOwner->GetAutoaimVector( 0 );
	Vector vecForwardVelocity = vecVelocity.Normalized() * DotProduct( vecVelocity, vecAimDir );
	Vector vecBlobFirePos = pOwner->GetPaintGunShootPosition();

	if( paintgun_shoot_position_trace_for_wall )
	{
		// Because the muzzle is so long, it can stick through a wall if the player is right up against it.
		// Make sure to adjust the shoot position in this condition by tracing a line between the eye point and the end of the muzzle.
		trace_t trace;
		Ray_t muzzleRay;
		muzzleRay.Init( vecEyePosition, vecBlobFirePos );
		CTraceFilterSimple traceFilter( pOwner, COLLISION_GROUP_NONE );
		UTIL_TraceRay( muzzleRay, MASK_SOLID, &traceFilter, &trace );

		//Check if there is a portal between the player's eye and the muzzle of the paint gun
		CPortal_Base2D *pInPortal = NULL;
		CPortal_Base2D *pOutPortal = NULL;
		if( UTIL_DidTraceTouchPortals( muzzleRay, trace, &pInPortal, &pOutPortal ) )
		{
			Vector vecPortalForward;
			AngleVectors( pInPortal->GetAbsAngles(), &vecPortalForward );
			Vector vecTraceDir = vecBlobFirePos - vecEyePosition;
			vecTraceDir.NormalizeInPlace();

			if( DotProduct( vecPortalForward, vecTraceDir ) < 0 )
			{
				UTIL_Portal_PointTransform( pInPortal->MatrixThisToLinked(), trace.endpos, vecBlobFirePos );
				UTIL_Portal_VectorTransform( pInPortal->MatrixThisToLinked(), vecAimDir, vecAimDir );
			}
		}
		else if ( trace.fraction < 1.0 && ( !trace.m_pEnt || trace.m_pEnt->m_takedamage == DAMAGE_NO ) )
		{
			// there is something between the eye and the end of the muzzle, most likely a wall
			// Move the muzzle position to the end position of the trace so that the wall gets painted
			vecBlobFirePos = trace.endpos;
		}
		//vecBlobFirePos = trace.endpos;
	}

	const float flBlobPerSecond = 1.0f/paintgun_blobs_per_second;
	while ( m_flAccumulatedTime >= flBlobPerSecond && HasPaintAmmo( paintType ) )
	{
		m_flAccumulatedTime -= flBlobPerSecond;
		CPaintBlob *pBlob = FirePaintBlob( vecBlobFirePos,
												vecBlobFirePos,
												vecForwardVelocity,
												vecAimDir,
												paintType,
												paintgun_blobs_spread_radius,
												paintgun_blobs_spread_angle,
												paintgun_blobs_min_speed,
												paintgun_blobs_max_speed,
												paintgun_blobs_streak_percent,
												paintgun_blobs_min_streak_time,
												paintgun_blobs_max_streak_time,
												paintgun_blobs_min_streak_speed_dampen,
												paintgun_blobs_max_streak_speed_dampen,
												false,
												false,
												pPaintStream,
												m_nBlobRandomSeed );
		
		pPaintStream->AddPaintBlob( pBlob );
		
		++m_nBlobRandomSeed;
		DecrementPaintAmmo( paintType );
	}
}

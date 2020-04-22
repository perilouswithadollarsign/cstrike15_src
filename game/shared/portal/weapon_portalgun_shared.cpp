//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_portalgun_shared.h"
#include "portal_mp_gamerules.h"
#include "npcevent.h"
#include "in_buttons.h"
#include "rumble_shared.h"
#include "portal_placement.h"
#include "collisionutils.h"
#include "prop_portal_shared.h"
#include "debugoverlay_shared.h"

#if defined( GAME_DLL )
#	include "info_placement_helper.h"
#	include "physicsshadowclone.h"
#	include "portal_player.h"
#	include "te_effect_dispatch.h"
#	include "BasePropDoor.h"
#	include "portal_gamestats.h"
#	include "triggers.h"
#	include "tier0/stackstats.h"
#	include "trigger_portal_cleanser.h"
#	include "portal_mp_stats.h"
#else
#	include "c_info_placement_helper.h"
#	include "c_te_effect_dispatch.h"
#	include "c_portal_player.h"
#	include "igameevents.h"
#	include "c_trigger_portal_cleanser.h"
#	include "prediction.h"
#endif

#ifdef CLIENT_DLL
	#define CWeaponPortalgun C_WeaponPortalgun
#endif // CLIENT_DLL

#if USE_SLOWTIME
	#ifdef CLIENT_DLL
		ConVar slowtime_speed( "slowtime_speed", "0.1", FCVAR_REPLICATED );
	#else
		extern ConVar slowtime_speed;
	#endif // CLIENT_DLL
#endif // USE_SLOWTIME

ConVar portalgun_fire_delay ( "portalgun_fire_delay", "0.20", FCVAR_CHEAT|FCVAR_REPLICATED );
ConVar portalgun_held_button_fire_delay ( "portalgun_held_button_fire_fire_delay", "0.50", FCVAR_CHEAT|FCVAR_REPLICATED );
extern ConVar sv_portal_placement_never_fail;
extern ConVar sv_portal_placement_debug;

#ifdef PORTAL2
ConVar portal2_square_portals( "portal2_square_portals", "0", FCVAR_REPLICATED );
ConVar portal2_portal_width( "portal2_portal_width", "72", FCVAR_REPLICATED );
ConVar use_server_portal_particles( "use_server_portal_particles", 0, FCVAR_REPLICATED );
#endif



#if defined( CLIENT_DLL ) //HACKHACK: Everything in this #ifdef is a hack for something that works on the server and needs to work on the client
extern int UTIL_EntitiesAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask );
#endif



acttable_t	CWeaponPortalgun::m_acttable[] = 
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

IMPLEMENT_ACTTABLE(CWeaponPortalgun);


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponPortalgun::CWeaponPortalgun( void )
{
	m_bReloadsSingly = true;

	// TODO: specify these in hammer instead of assuming every gun has blue chip
	m_bCanFirePortal1 = true;
	m_bCanFirePortal2 = false;

	m_iLastFiredPortal = 0;

	m_fMinRange1	= 0.0f;
	m_fMaxRange1	= MAX_TRACE_LENGTH;
	m_fMinRange2	= 0.0f;
	m_fMaxRange2	= MAX_TRACE_LENGTH;

	m_EffectState.Set( EFFECT_NONE );

#ifndef CLIENT_DLL
	ClearPortalPositions();
#endif // !CLIENT_DLL
}

void CWeaponPortalgun::Precache()
{
	BaseClass::Precache();

	PrecacheModel( PORTALGUN_BEAM_SPRITE );
	PrecacheModel( PORTALGUN_BEAM_SPRITE_NOZ );

	PrecacheMaterial( PORTALGUN_GLOW_SPRITE );
	PrecacheMaterial( PORTALGUN_ENDCAP_SPRITE );
	PrecacheMaterial( PORTALGUN_GRAV_ACTIVE_GLOW );
	PrecacheMaterial( PORTALGUN_PORTAL1_FIRED_LAST_GLOW );
	PrecacheMaterial( PORTALGUN_PORTAL2_FIRED_LAST_GLOW );
	PrecacheMaterial( PORTALGUN_PORTAL_TINTED_GLOW );
	PrecacheMaterial( PORTALGUN_PORTAL_MUZZLE_GLOW_SPRITE );
	PrecacheMaterial( PORTALGUN_PORTAL_TUBE_BEAM_SPRITE );

	PrecacheModel( "models/portals/portal1.mdl" );
	PrecacheModel( "models/portals/portal2.mdl" );

	PrecacheScriptSound( "Portal.ambient_loop" );

	PrecacheScriptSound( "Portal.open_blue" );
	PrecacheScriptSound( "Portal.open_red" );
	PrecacheScriptSound( "Portal.close_blue" );
	PrecacheScriptSound( "Portal.close_red" );
	PrecacheScriptSound( "Portal.fizzle_moved" );
	PrecacheScriptSound( "Portal.fizzle_invalid_surface" );
	PrecacheScriptSound( "Weapon_Portalgun.powerup" );
	PrecacheScriptSound( "Weapon_Portalgun.HoldSound" );
	PrecacheScriptSound( "Portal.PortalgunActivate" );
	PrecacheScriptSound( "Portal.FizzlerShimmy" );

#ifndef CLIENT_DLL
	PrecacheParticleSystem( "portal_1_projectile_stream" );
	PrecacheParticleSystem( "portal_1_projectile_stream_pedestal" );
	PrecacheParticleSystem( "portal_2_projectile_stream" );
	PrecacheParticleSystem( "portal_2_projectile_stream_pedestal" );
	// color changing version
	PrecacheParticleSystem( "portal_projectile_stream" );
	PrecacheParticleSystem( "portal_weapon_cleanser" );
#ifndef PORTAL2
	PrecacheParticleSystem( "portal_1_charge" );
	PrecacheParticleSystem( "portal_2_charge" );
#endif

	PrecacheEffect( "PortalBlast" );
	
	UTIL_PrecacheOther( "prop_portal" );
#endif
}

bool CWeaponPortalgun::ShouldDrawCrosshair( void )
{
	return true;//( m_fCanPlacePortal1OnThisSurface > 0.5f || m_fCanPlacePortal2OnThisSurface > 0.5f );
}

//-----------------------------------------------------------------------------
// Purpose: Override so only reload one shell at a time
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::Reload( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Play finish reload anim and fill clip
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CWeaponPortalgun::FillClip( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return;

	// Add them to the clip
	if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		if ( Clip1() < GetMaxClip1() )
		{
			m_iClip1++;
			pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::DryFire( void )
{
	WeaponSound(EMPTY);
	SendWeaponAnim( ACT_VM_DRYFIRE );
	
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::UseDeny( void )
{
	WeaponSound( EMPTY );
	SendWeaponAnim( ACT_VM_DRYFIRE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::ResetRefireTime( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer )
	{
		pPlayer->SetNextAttack( gpGlobals->curtime - 0.1f );
	}

	// Let us fire immediately
	m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	m_flNextSecondaryAttack = gpGlobals->curtime - 0.1f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::SetCanFirePortal1( bool bCanFire /*= true*/ )
{
#ifdef GAME_DLL
	bool bUpgraded = ( !m_bCanFirePortal1 && bCanFire );
#endif

	m_bCanFirePortal1 = bCanFire;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	if ( !m_bOpenProngs )
	{
		DoEffect( EFFECT_HOLDING );
		DoEffect( EFFECT_READY );
	}

	// TODO: Remove muzzle flash when there's an upgrade animation
	//pOwner->DoMuzzleFlash();

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.25f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.25f;

	// player "shoot" animation
	pOwner->SetAnimation( PLAYER_ATTACK1 );

	pOwner->ViewPunch( QAngle( random->RandomFloat( -1, -0.5f ), random->RandomFloat( -1, 1 ), 0 ) );

	EmitSound( "Weapon_Portalgun.powerup" );

#ifdef GAME_DLL
	if ( bUpgraded )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "portal_enabled" );
		if ( event )
		{
			event->SetInt( "userid", pOwner->GetUserID() );
			event->SetBool( "leftportal", true );

			gameeventmanager->FireEvent( event );
		}
	}
#endif
}

void CWeaponPortalgun::SetCanFirePortal2( bool bCanFire /*= true*/ )
{
#ifdef GAME_DLL
	bool bUpgraded = ( !m_bCanFirePortal2 && bCanFire );
#endif

	m_bCanFirePortal2 = bCanFire;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
	{
		Msg( "Weapon_portalgun has no owner when trying to upgrade!\n" );
		return;
	}

	if ( !m_bOpenProngs )
	{
		DoEffect( EFFECT_HOLDING );
		DoEffect( EFFECT_READY );
	}

	// TODO: Remove muzzle flash when there's an upgrade animation
	//pOwner->DoMuzzleFlash();

	// Don't fire again until fire animation has completed
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;

	// player "shoot" animation
	pOwner->SetAnimation( PLAYER_ATTACK1 );

	pOwner->ViewPunch( QAngle( random->RandomFloat( -1, -0.5f ), random->RandomFloat( -1, 1 ), 0 ) );

	EmitSound( "Weapon_Portalgun.powerup" );

#ifdef GAME_DLL
	if ( bUpgraded )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "portal_enabled" );
		if ( event )
		{
			event->SetInt( "userid", pOwner->GetUserID() );
			event->SetBool( "leftportal", false );

			gameeventmanager->FireEvent( event );
		}
	}
#endif
}


bool CWeaponPortalgun::CanFirePortal1( void ) const
{
	return m_bCanFirePortal1;
}


bool CWeaponPortalgun::CanFirePortal2( void ) const
{
	return m_bCanFirePortal2;
}

#if defined( CLIENT_DLL )
ConVar cl_predict_portal_placement( "cl_predict_portal_placement", "1", FCVAR_NONE, "Controls whether we attempt to compensate for lag by predicting portal placement on the client when playing multiplayer." );
#endif

void CWeaponPortalgun::PostAttack( void )
{
	// Only the player fires this way so we can cast
	CPortal_Player *pPlayer = ToPortalPlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

#if defined( GAME_DLL ) //TODO: client version would probably be a good idea
	pPlayer->RumbleEffect( RUMBLE_PORTALGUN_LEFT, 0, RUMBLE_FLAGS_NONE );
#endif


	int nSplitScreenSlot = pPlayer->GetSplitScreenPlayerSlot();
	CBaseAnimating *pModelView =  pPlayer->GetViewModel();

#if defined( CLIENT_DLL )
	if ( !prediction->InPrediction() || prediction->IsFirstTimePredicted() )
#endif
	{
		if ( ( pPlayer->IsSplitScreenPlayer() || pPlayer->HasAttachedSplitScreenPlayers() ) && nSplitScreenSlot != -1 )
		{
#if defined( CLIENT_DLL )
			if ( pModelView )
			{
				CUtlReference<CNewParticleEffect> m_hPortalGunMuzzle = pModelView->ParticleProp()->Create( "portalgun_muzzleflash_FP", PATTACH_POINT_FOLLOW, "muzzle" );
				m_hPortalGunMuzzle->SetDrawOnlyForSplitScreenUser( nSplitScreenSlot );
				m_hPortalGunMuzzle = NULL;
			}
			DispatchParticleEffect( "portalgun_muzzleflash", PATTACH_POINT_FOLLOW, this, "muzzle");
#endif
		}
		else
		{
			if ( pModelView )
				DispatchParticleEffect( "portalgun_muzzleflash_FP", PATTACH_POINT_FOLLOW, pModelView, "muzzle" );
			DispatchParticleEffect( "portalgun_muzzleflash", PATTACH_POINT_FOLLOW, this, "muzzle");
		}
	}

	float flFireDelay = portalgun_fire_delay.GetFloat();

#if USE_SLOWTIME
	if ( pPlayer->IsSlowingTime() )
	{
		flFireDelay *= slowtime_speed.GetFloat();
	}
	else
#endif // USE_SLOWTIME
	{
		QAngle qPunch;
		qPunch.x = SharedRandomFloat( "CWeaponPortalgun::PrimaryAttack() ViewPunchX", -1, -0.5f );
		qPunch.y = SharedRandomFloat( "CWeaponPortalgun::PrimaryAttack() ViewPunchY", -1, 1 );
		qPunch.z = 0.0f;
		pPlayer->ViewPunch( qPunch );
	}

	// Don't fire again too quickly
	m_flNextPrimaryAttack = gpGlobals->curtime + flFireDelay;
	m_flNextSecondaryAttack = gpGlobals->curtime + flFireDelay;

	// Held-button repeat fires get a different delay
	m_flNextRepeatPrimaryAttack = gpGlobals->curtime + portalgun_held_button_fire_delay.GetFloat();
	m_flNextRepeatSecondaryAttack = gpGlobals->curtime + portalgun_held_button_fire_delay.GetFloat();
	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponPortalgun::PrimaryAttack( void )
{
#if defined( PORTAL2 ) && defined ( CLIENT_DLL )
	CBaseCombatCharacter* pOwner = GetOwner();
	const bool bOwnerCanFire = !pOwner || !pOwner->IsPlayer() || !assert_cast<CPortal_Player*>(pOwner)->IsHoldingSomething();
	if ( !CanFirePortal1() || !bOwnerCanFire )
		return;
#else
	if ( !CanFirePortal1() )
		return;
#endif

	// Only the player fires this way so we can cast
	CPortal_Player *pPlayer = ToPortalPlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

#if defined( CLIENT_DLL )
	if( cl_predict_portal_placement.GetBool() )
#endif
	{
		FirePortal1();
	}
#if defined( GAME_DLL )
	m_OnFiredPortal1.FireOutput( pPlayer, this );
#endif

	PostAttack();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CWeaponPortalgun::SecondaryAttack( void )
{
#if defined( PORTAL2 ) && defined ( CLIENT_DLL )
	CBaseCombatCharacter* pOwner = GetOwner();
	const bool bOwnerCanFire = !pOwner || !pOwner->IsPlayer() || !assert_cast<CPortal_Player*>(pOwner)->IsHoldingSomething();
	if ( !CanFirePortal2() || !bOwnerCanFire )
		return;
#else
	if ( !CanFirePortal2() )
		return;
#endif

	// Only the player fires this way so we can cast
	CPortal_Player *pPlayer = ToPortalPlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

#if defined( CLIENT_DLL )
	if( cl_predict_portal_placement.GetBool() )
#endif
	{
		FirePortal2();
	}

	PostAttack();
}

void CWeaponPortalgun::DelayAttack( float fDelay )
{
	m_flNextPrimaryAttack = gpGlobals->curtime + fDelay;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::ItemHolsterFrame( void )
{
	// Must be player held
	if ( GetOwner() && GetOwner()->IsPlayer() == false )
		return;

	// We can't be active
	if ( GetOwner()->GetActiveWeapon() == this )
		return;

	// If it's been longer than three seconds, reload
	if ( ( gpGlobals->curtime - m_flHolsterTime ) > sk_auto_reload_time.GetFloat() )
	{
		// Reset the timer
		m_flHolsterTime = gpGlobals->curtime;
	
		if ( GetOwner() == NULL )
			return;

		if ( m_iClip1 == GetMaxClip1() )
			return;

		// Just load the clip with no animations
		int ammoFill = MIN( (GetMaxClip1() - m_iClip1), GetOwner()->GetAmmoCount( GetPrimaryAmmoType() ) );
		
		GetOwner()->RemoveAmmo( ammoFill, GetPrimaryAmmoType() );
		m_iClip1 += ammoFill;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	DestroyEffects();

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::Deploy( void )
{
	DoEffect( EFFECT_READY );

	bool bReturn = BaseClass::Deploy();

	m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->curtime;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner )
	{
		pOwner->SetNextAttack( gpGlobals->curtime );

#if defined( GAME_DLL )
		if( GameRules()->IsMultiplayer() )
		{
			m_iPortalLinkageGroupID = pOwner->entindex();

			m_hPrimaryPortal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, false, true );
			m_hSecondaryPortal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, true, true );

			Assert( (m_iPortalLinkageGroupID >= 0) && (m_iPortalLinkageGroupID < 256) );
		}
#endif
	}

	return bReturn;
}

void CWeaponPortalgun::WeaponIdle( void )
{
#if defined( PORTAL2 ) && defined ( CLIENT_DLL )
	CBaseCombatCharacter* pOwner = GetOwner();
	const bool bIsHolding = pOwner && pOwner->IsPlayer() && assert_cast<CPortal_Player*>(pOwner)->IsHoldingSomething();
	if ( GameRules()->IsMultiplayer() && bIsHolding )
		return;
#endif

	//See if we should idle high or low
	if ( WeaponShouldBeLowered() )
	{
		// Move to lowered position if we're not there yet
		if ( GetActivity() != ACT_VM_IDLE_LOWERED && GetActivity() != ACT_VM_IDLE_TO_LOWERED 
			&& GetActivity() != ACT_TRANSITION )
		{
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
		else if ( HasWeaponIdleTimeElapsed() )
		{
			// Keep idling low
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
	}
	else
	{
		// See if we need to raise immediately
		if ( m_flRaiseTime < gpGlobals->curtime && GetActivity() == ACT_VM_IDLE_LOWERED ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
		else if ( HasWeaponIdleTimeElapsed() ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::StopEffects( bool stopSound )
{
	// Turn off our effect state
	DoEffect( EFFECT_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : effectType - 
//-----------------------------------------------------------------------------
void CWeaponPortalgun::DoEffect( int effectType, Vector *pos )
{
	m_EffectState = effectType;

#ifdef CLIENT_DLL
	// Save predicted state
	m_nOldEffectState = m_EffectState;
#endif

	switch( effectType )
	{
	case EFFECT_READY:
		DoEffectReady();
		break;

	case EFFECT_HOLDING:
		DoEffectHolding();
		break;

	default:
	case EFFECT_NONE:
		DoEffectNone();
		break;
	}
}

void CWeaponPortalgun::DoEffectBlast( CBaseEntity *pOwner, bool bPortal2, int iPlacedBy, const Vector &ptStart, const Vector &ptFinalPos, const QAngle &qStartAngles, float fDelay )
{
	CEffectData	fxData;
	fxData.m_vOrigin = ptStart;
	fxData.m_vStart = ptFinalPos;
	fxData.m_flScale = gpGlobals->curtime + fDelay;
	fxData.m_vAngles = qStartAngles;
	fxData.m_nColor = ( ( bPortal2 ) ? ( 2 ) : ( 1 ) );
	fxData.m_nDamageType = iPlacedBy;
#ifdef CLIENT_DLL
	fxData.m_hEntity = ClientEntityList().EntIndexToHandle( pOwner->entindex() );
#else
	fxData.m_nEntIndex = pOwner ? pOwner->entindex() : entindex();
#endif
	DispatchEffect( "PortalBlast", fxData );
}

//-----------------------------------------------------------------------------
// Purpose: Restore
//-----------------------------------------------------------------------------
void CWeaponPortalgun::OnRestore()
{
	BaseClass::OnRestore();

	// Portalgun effects disappear through level transition, so
	//  just recreate any effects here
	if ( m_EffectState != EFFECT_NONE )
	{
		DoEffect( m_EffectState, NULL );
	}
}


//-----------------------------------------------------------------------------
// On Remove
//-----------------------------------------------------------------------------
void CWeaponPortalgun::UpdateOnRemove(void)
{
	DestroyEffects();
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// On Remove
//-----------------------------------------------------------------------------
Activity CWeaponPortalgun::GetPrimaryAttackActivity( void )
{
#if USE_SLOWTIME
	CPortal_Player *pPlayer = ToPortalPlayer( GetOwner() );
	if ( pPlayer && pPlayer->IsSlowingTime() )
		return ACT_VM_SECONDARYATTACK;
#endif // USE_SLOWTIME

	return BaseClass::GetPrimaryAttackActivity();
}

#if defined( GAME_DLL )
#define szDllName "server"
#else
#define szDllName "client"
#endif


void CWeaponPortalgun::FirePortal1( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	CPortal_Player *pPlayer = ToPortalPlayer( pOwner );

#ifdef GAME_DLL
	if ( !pPlayer )
	{
		// Pedistal portal guns need to set up the correct linkage
		for( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CPortal_Player *pOtherPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

			//If the other player does not exist or if the other player is the local player
			if( pOtherPlayer == NULL )
				continue;

			if( !pOtherPlayer->IsConnected() )
				continue;

			if ( pOtherPlayer->GetTeamNumber() == GetTeamNumber() )
			{
				if ( m_iPortalLinkageGroupID != pOtherPlayer->entindex() )
				{
					SetLinkageGroupID( pOtherPlayer->entindex() );
				}
				break;
			}
		}
	}

	if( m_hPrimaryPortal.Get() == NULL )
	{
		m_hPrimaryPortal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, false, true );
	}
#else
	if( m_hPrimaryPortal )
	{
		if( !m_hPrimaryPortal->GetPredictable() || !m_hSecondaryPortal.Get() || !m_hSecondaryPortal->GetPredictable() )
			return;

		if( m_hPrimaryPortal->m_hLinkedPortal.Get() == NULL )
		{
			CProp_Portal *pPortal = m_hPrimaryPortal.Get();
			CProp_Portal *pOtherPortal = m_hSecondaryPortal.Get();
			if( pOtherPortal && pOtherPortal->IsActive() )
			{
				pPortal->m_hLinkedPortal = pOtherPortal;
				pOtherPortal->m_hLinkedPortal = pPortal;
			}
		}
		else
		{
			if( !m_hPrimaryPortal->m_hLinkedPortal->GetPredictable() )
				return;
		}
	}
#endif

	PortalPlacementResult_t eResult = FirePortal( false );
	if ( PortalPlacementSucceeded( eResult ) )
	{
		CProp_Portal *pPortal = m_hPrimaryPortal.Get();//CProp_Portal::FindPortal( m_iPortalLinkageGroupID, false );
		if ( pPortal )
		{
			m_vecBluePortalPos = pPortal->m_vDelayedPosition;
			pPortal->SetActive( true );
			pPortal->ChangeTeam( GetTeamNumber() ); //copy team number from the gun
		}
		SetLastFiredPortal( 1 );

#if defined( GAME_DLL )
		IGameEvent *event = gameeventmanager->CreateEvent( "portal_fired" );
		if ( event )
		{
			event->SetInt( "userid", pPlayer ? pPlayer->GetUserID() : 0 );
			event->SetBool( "leftportal", true );

			gameeventmanager->FireEvent( event );
		}
		
		// Track multiplayer stats
		if( GetPortalMPStats() && pPlayer )
		{
			GetPortalMPStats()->IncrementPlayerPortals( pPlayer );
		}
#endif
	}

	if ( pPlayer )
	{
		WeaponSound( SINGLE );
	}
	else
	{
		WeaponSound( SINGLE_NPC );
	}

#if defined( GAME_DLL ) && !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	if ( pPlayer )
	{
		Vector vPortalPos(0,0,0);
		CProp_Portal *pPortal = m_hPrimaryPortal.Get();
		if ( pPortal )
		{
			vPortalPos = pPortal->m_vDelayedPosition;
		}

		g_PortalGameStats.Event_PortalPlacement( pPlayer, vPortalPos, eResult, false );
	}
#endif

}

void CWeaponPortalgun::FirePortal2( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	CPortal_Player *pPlayer = ToPortalPlayer( pOwner );

#if defined( GAME_DLL )
	if( m_hSecondaryPortal.Get() == NULL )
	{
		m_hSecondaryPortal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, true, true );
	}
#else
	if( m_hSecondaryPortal )
	{
		if( !m_hSecondaryPortal->GetPredictable() || !m_hPrimaryPortal.Get() || !m_hPrimaryPortal->GetPredictable() )
			return;

		if( m_hSecondaryPortal->m_hLinkedPortal.Get() == NULL )
		{
			CProp_Portal *pPortal = m_hSecondaryPortal.Get();
			CProp_Portal *pOtherPortal = m_hPrimaryPortal.Get();
			if( pOtherPortal && pOtherPortal->IsActive() )
			{
				pPortal->m_hLinkedPortal = pOtherPortal;
				pOtherPortal->m_hLinkedPortal = pPortal;
			}
		}
		else
		{
			if( !m_hSecondaryPortal->m_hLinkedPortal->GetPredictable() )
				return;
		}
	}
#endif

	PortalPlacementResult_t eResult = FirePortal( true );
	if ( PortalPlacementSucceeded( eResult ) )
	{
		CProp_Portal *pPortal = m_hSecondaryPortal.Get(); //CProp_Portal::FindPortal( m_iPortalLinkageGroupID, true );
		if ( pPortal )
		{
			m_vecOrangePortalPos = pPortal->m_vDelayedPosition;
			pPortal->SetActive( true );
			pPortal->ChangeTeam( GetTeamNumber() ); //copy team number from the gun
		}

		SetLastFiredPortal( 2 );

#if defined( GAME_DLL )
		IGameEvent *event = gameeventmanager->CreateEvent( "portal_fired" );
		if ( event )
		{
			event->SetInt( "userid", pPlayer ? pPlayer->GetUserID() : 0 );
			event->SetBool( "leftportal", false );

			gameeventmanager->FireEvent( event );
		}

		// Track multiplayer stats
		if( GetPortalMPStats() && pPlayer )
		{
			GetPortalMPStats()->IncrementPlayerPortals( pPlayer );
		}
#endif
	}

	if( pPlayer )
	{
		WeaponSound( WPN_DOUBLE );
	}
	else
	{
		WeaponSound( DOUBLE_NPC );
	}

#if defined( GAME_DLL ) && !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	if ( pPlayer )
	{
		Vector vPortalPos(0,0,0);
		CProp_Portal *pPortal = m_hPrimaryPortal.Get();
		if ( pPortal )
		{
			vPortalPos = pPortal->m_vDelayedPosition;
		}

		g_PortalGameStats.Event_PortalPlacement( pPlayer, vPortalPos, eResult, true );
	}
#endif
}

#if defined( GAME_DLL )
//#define STACK_ANALYZE_FIREPORTAL
#endif

#if defined( STACK_ANALYZE_FIREPORTAL )
struct PortalFired_t
{
	DECLARE_CALLSTACKSTATSTRUCT();
	DECLARE_CALLSTACKSTATSTRUCT_FIELDDESCRIPTION();

	int iPortal1;
	int iPortal2;
};

BEGIN_STATSTRUCTDESCRIPTION( PortalFired_t )
WRITE_STATSTRUCT_FIELDDESCRIPTION();
END_STATSTRUCTDESCRIPTION()

BEGIN_STATSTRUCTFIELDDESCRIPTION( PortalFired_t )
DEFINE_STATSTRUCTFIELD( iPortal1, BasicStatStructFieldDesc, ( BSSFT_INT32, BSSFCM_ADD ) )
DEFINE_STATSTRUCTFIELD( iPortal2, BasicStatStructFieldDesc, ( BSSFT_INT32, BSSFCM_ADD ) )
END_STATSTRUCTFIELDDESCRIPTION()

CCallStackStatsGatherer<PortalFired_t, 32, GetCallStack_Fast> s_PortalFiredStats;
extern CCallStackStatsGatherer_Standardized_t g_PlacementStats;


#endif

PortalPlacementResult_t CWeaponPortalgun::FirePortal( bool bPortal2, Vector *pVector /*= 0*/ )
{
#if defined( STACK_ANALYZE_FIREPORTAL )
	CCallStackStorage hereStack( s_PortalFiredStats.StackFunction );
	CCallStackStatsGatherer_StructAccessor_AutoLock<PortalFired_t> stats = s_PortalFiredStats.GetEntry( hereStack );
	if( bPortal2 )
	{
		++stats->iPortal2;
	}
	else
	{
		++stats->iPortal1;
	}

	CCallStackStats_PushSubTree_AutoPop treePusher( g_PlacementStats, s_PortalFiredStats, hereStack );
#endif

	Vector vEye;
	Vector vDirection;
	Vector vTracerOrigin;

	CBaseEntity *pOwner = GetOwner();
	CPortal_Player *pPlayer = NULL;
	if ( pOwner && pOwner->IsPlayer() )
	{
		pPlayer = ToPortalPlayer( pOwner );
	}

	if( pPlayer )
	{
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY, 0 );

		Vector forward, right, up;
		if ( pPlayer->IsTrickFiring() )
		{
			AngleVectors( pPlayer->GetTrickFireAngles(), &forward, &right, &up );
			pPlayer->ClearTrickFiring();
		}
		else
		{
			AngleVectors( pPlayer->EyeAngles(), &forward, &right, &up );
		}

		vDirection = forward;

		vEye = pPlayer->Weapon_ShootPosition();

		//EASY_DIFFPRINT( this, "FirePortal eye %f %f %f  dir %f %f %f\n", XYZ( vEye ), XYZ( vDirection ) );

		// Check if the players eye is behind the portal they're in and translate it
		VMatrix matThisToLinked;

		CProp_Portal *pPlayerPortal = (CProp_Portal *)pPlayer->m_hPortalEnvironment.Get();
		if ( pPlayerPortal )
		{
			Vector vEyeToPortalCenter = pPlayerPortal->m_ptOrigin - vEye;

			float fPortalDist = pPlayerPortal->m_vForward.Dot( vEyeToPortalCenter );
			if( fPortalDist > 0.0f )
			{
				// Eye is behind the portal
				matThisToLinked = pPlayerPortal->MatrixThisToLinked();
			}
			else
			{
				pPlayerPortal = NULL;
			}
		}

		if ( pPlayerPortal )
		{
			UTIL_Portal_VectorTransform( matThisToLinked, forward, forward );
			UTIL_Portal_VectorTransform( matThisToLinked, right, right );
			UTIL_Portal_VectorTransform( matThisToLinked, up, up );
			UTIL_Portal_VectorTransform( matThisToLinked, vDirection, vDirection );
			UTIL_Portal_PointTransform( matThisToLinked, vEye, vEye );

			if ( pVector )
			{
				UTIL_Portal_VectorTransform( matThisToLinked, *pVector, *pVector );
			}
		}

		//EASY_DIFFPRINT( this, "FirePortal forward %f %f %f\n", XYZ( forward ) );
		//EASY_DIFFPRINT( this, "FirePortal right %f %f %f\n", XYZ( right ) );
		//EASY_DIFFPRINT( this, "FirePortal up %f %f %f\n", XYZ( up ) );

		vTracerOrigin = vEye + GetAbsVelocity() * 0.1f + 
			forward * 30.0f +
			right * 4.0f +
			up * (-8.0f);

		//the up vector tends to have little bit of wiggle in it in the millionths decimal place. Stomp out even the smallest disagreement here.
#if 1
		vTracerOrigin.x = floor( vTracerOrigin.x * 512.0f ) / 512.0f;
		vTracerOrigin.y = floor( vTracerOrigin.y * 512.0f ) / 512.0f;
		vTracerOrigin.z = floor( vTracerOrigin.z * 512.0f ) / 512.0f;
#endif

		//EASY_DIFFPRINT( this, "FirePortal vTracerOrigin %f %f %f\n", XYZ( vTracerOrigin ) );
	}
	else
	{
		// This portalgun is not held by the player-- Fire using the muzzle attachment
		Vector vecShootOrigin;
		QAngle angShootDir;
		GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
		vEye = vecShootOrigin;
		vTracerOrigin = vecShootOrigin;
		AngleVectors( angShootDir, &vDirection, NULL, NULL );
	}

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	if ( pVector )
	{
		vDirection = *pVector;
	}

	CProp_Portal *pPortal = bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get(); //CProp_Portal::FindPortal( m_iPortalLinkageGroupID, bPortal2, true );
	Assert( pPortal );
	if ( pPortal )
	{
		pPortal->SetFiredByPlayer( pPlayer );
		pPortal->m_nPlacementAttemptParity = (pPortal->m_nPlacementAttemptParity + 1) & EF_PARITY_MASK; //no matter what, prod the network state so we can detect prediction errors
	}

	Vector vTraceStart = vEye + (vDirection * m_fMinRange1);

	PortalPlacedBy_t ePlacedBy = ( pPlayer ) ? ( PORTAL_PLACED_BY_PLAYER ) : ( PORTAL_PLACED_BY_PEDESTAL );

	// Attempt the portal fire
	TracePortalPlacementInfo_t placementInfo;
	bool bTraceSucceeded = TraceFirePortal( vTraceStart, vDirection, bPortal2, ePlacedBy, placementInfo );

	// Stomp the results if we want to always succeed
	if ( sv_portal_placement_never_fail.GetBool() )
	{
		placementInfo.ePlacementResult = PORTAL_PLACEMENT_SUCCESS;
		bTraceSucceeded = true;
	}

	if( pPortal == NULL )
	{
		return PORTAL_PLACEMENT_INVALID_VOLUME;
	}


	// If it was a failure, put the effect at exactly where the player shot instead of where the portal bumped to
	if ( bTraceSucceeded == false )
	{
		placementInfo.vecFinalPosition = placementInfo.tr.endpos;
	}

#if 1
	placementInfo.vecFinalPosition.x = floor( placementInfo.vecFinalPosition.x * 512.0f ) / 512.0f;
	placementInfo.vecFinalPosition.y = floor( placementInfo.vecFinalPosition.y * 512.0f ) / 512.0f;
	placementInfo.vecFinalPosition.z = floor( placementInfo.vecFinalPosition.z * 512.0f ) / 512.0f;
#endif

	// Otherwise, place the portal
	pPortal->PlacePortal( placementInfo.vecFinalPosition, placementInfo.angFinalAngles, placementInfo.ePlacementResult );

//#if defined( CLIENT_DLL )
//	Warning( "CWeaponPortalgun::FirePortal(client) %f     %.2f %.2f %.2f\n", gpGlobals->curtime, XYZ( placementInfo.vecFinalPosition ) );
//#else
//	Warning( "CWeaponPortalgun::FirePortal(SERVER) %f     %.2f %.2f %.2f\n", gpGlobals->curtime, XYZ( placementInfo.vecFinalPosition ) );
//#endif

	QAngle qFireAngles;
	VectorAngles( vDirection, qFireAngles );
	DoEffectBlast( pOwner, pPortal->m_bIsPortal2, ePlacedBy, vTracerOrigin, placementInfo.vecFinalPosition, qFireAngles, 0.0f );
	//NDebugOverlay::Cross( vTracerOrigin, 5, 255, 255, 255, true, 100 );
	

	pPortal->m_vDelayedPosition = placementInfo.vecFinalPosition;
#if defined( GAME_DLL )
	pPortal->m_hPlacedBy = this;

	int nTeam = 0;

	if( pPlayer )
	{
		nTeam = pPlayer->GetTeamNumber();
	}

	if ( nTeam == TEAM_BLUE )
	{
		pPortal->m_nPortalColor = ( !pPortal->m_bIsPortal2 ? PORTAL_COLOR_FLAG_BLUE : PORTAL_COLOR_FLAG_PURPLE );
	}
	else if ( nTeam == TEAM_RED )
	{
		pPortal->m_nPortalColor = ( !pPortal->m_bIsPortal2 ? PORTAL_COLOR_FLAG_ORANGE : PORTAL_COLOR_FLAG_RED );
	}
	else
	{
		pPortal->m_nPortalColor = ( !pPortal->m_bIsPortal2 ? PORTAL_COLOR_FLAG_BLUE : PORTAL_COLOR_FLAG_ORANGE );
	}
#endif

	//EASY_DIFFPRINT( this, "Shooting portal to position %f %f %f\n", XYZ( placementInfo.vecFinalPosition ) );

#if defined( GAME_DLL )
	// FIXME: Without the delay, this code doesn't make much sense now -- jdw
	//pPortal->SetContextThink( &CProp_Portal::DelayedPlacementThink, gpGlobals->curtime, CProp_Portal::s_szDelayedPlacementThinkContext );
	pPortal->DelayedPlacementThink();
#else
	pPortal->DelayedPlacementThink();
#endif

#if defined( PORTAL2 ) && defined( GAME_DLL )
	if ( portal2_square_portals.GetBool() )
	{
		float flWidth = portal2_portal_width.GetFloat();
		pPortal->Resize( flWidth, flWidth );
	}
#endif // PORTAL2

#if defined( GAME_DLL )
	if ( PortalPlacementSucceeded( placementInfo.ePlacementResult ) )
	{
		// OldPosition remains old until we've done all effects and movement
		// but don't update it unless it was a successful portal placement
		pPortal->m_vOldPosition = placementInfo.vecFinalPosition;
		pPortal->m_qOldAngles = placementInfo.angFinalAngles;

		if ( pPlayer && pPortal->IsActivedAndLinked() )
		{
			CPortal_Player *pOtherPlayer = ToPortalPlayer( UTIL_OtherPlayer( pPlayer ) );
			if ( pOtherPlayer && pOtherPlayer->IsTaunting() && pOtherPlayer->GetGroundEntity() != NULL )
			{
				Vector vPortalForward;
				pPortal->GetVectors( &vPortalForward, NULL, NULL );
				// check if portal is somewhat on the ground where the other player can stand
				if ( vPortalForward.z > 0.7f )
				{
					// check if the other player is falling through portal
					Ray_t ray;
					ray.Init( pOtherPlayer->GetAbsOrigin(), pOtherPlayer->GetAbsOrigin() + Vector( 0.0f, 0.0f, -5.0f ), pOtherPlayer->GetPlayerMins(), pOtherPlayer->GetPlayerMaxs() );

					trace_t tr;
					if ( pPortal == UTIL_Portal_TraceRay( ray, MASK_PLAYERSOLID_BRUSHONLY, pOtherPlayer, COLLISION_GROUP_NONE, &tr ) )
					{
						UTIL_RecordAchievementEvent( "ACH.PORTAL_TAUNT", pPlayer );
					}
				}
			}
		}

		PortalPlaced();
	}

	// Bind the helper to the portal so it can toggle its state based on the portal's state
	if ( placementInfo.pPlacementHelper != NULL  )
	{
		placementInfo.pPlacementHelper->BindToPortal( pPortal );
	}
#endif

	return placementInfo.ePlacementResult;
}



//-----------------------------------------------------------------------------
// Purpose: Try to fire a portal and make it fit at a position
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::TraceFirePortal( const Vector &vTraceStart, const Vector &vDirection, bool bPortal2, PortalPlacedBy_t ePlacedBy, TracePortalPlacementInfo_t &placementInfo )
{
	// Setup a trace filter that ignore / hits everything we care about
#if defined( GAME_DLL )
	CTraceFilterSimpleClassnameList baseFilter( this, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &baseFilter );
	CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );
#else
	CTraceFilterSimpleClassnameList traceFilterPortalShot( this, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &traceFilterPortalShot );
#endif

	/*
	Ray_t rayEyeArea;
	rayEyeArea.Init( vTraceStart + vDirection * 24.0f, vTraceStart + vDirection * -24.0f );

	const float fMustBeCloserThan = 2.0f;

	const int iPortalArrayCount = CProp_Portal_Shared::AllPortals.Count();
	CPortal_Base2D **pPortalArray = iPortalArrayCount > 0 ? (CPortal_Base2D **)CProp_Portal_Shared::AllPortals.Base() : NULL;

	CProp_Portal *pNearPortal = (CProp_Portal *)UTIL_Portal_FirstAlongRay( rayEyeArea, fMustBeCloserThan, pPortalArray, iPortalArrayCount );
	Assert( (pNearPortal == NULL) || (dynamic_cast<CProp_Portal *>((CPortal_Base2D *)pNearPortal) != NULL) ); //doublecheck that the cast was valid

	if ( !pNearPortal )
	{
	// Check for portal near and infront of you
	rayEyeArea.Init( vTraceStart + vDirection * -24.0f, vTraceStart + vDirection * 48.0f );

	fMustBeCloserThan = 2.0f;

	pNearPortal = (CProp_Portal *)UTIL_Portal_FirstAlongRay( rayEyeArea, fMustBeCloserThan, pPortalArray, iPortalArrayCount );
	Assert( (pNearPortal == NULL) || (dynamic_cast<CProp_Portal *>((CPortal_Base2D *)pNearPortal) != NULL) ); //doublecheck that the cast was valid
	}

	if ( pNearPortal && pNearPortal->IsActivedAndLinked() )
	{
	iPlacedBy = PORTAL_PLACED_BY_PEDESTAL;

	Vector vPortalForward;
	pNearPortal->GetVectors( &vPortalForward, 0, 0 );

	if ( vDirection.Dot( vPortalForward ) < 0.01f )
	{
	// If shooting out of the world, fizzle
	if ( !bTest )
	{
	CProp_Portal *pPortal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, bPortal2, true );

	pPortal->m_iDelayedFailure = ( ( pNearPortal->m_bIsPortal2 ) ? ( PORTAL_FIZZLE_NEAR_RED ) : ( PORTAL_FIZZLE_NEAR_BLUE ) );
	VectorAngles( vPortalForward, pPortal->m_qDelayedAngles );
	pPortal->m_vDelayedPosition = pNearPortal->GetAbsOrigin();

	vFinalPosition = pPortal->m_vDelayedPosition;
	qFinalAngles = pPortal->m_qDelayedAngles;

	UTIL_TraceLine( vTraceStart - vDirection * 16.0f, vTraceStart + (vDirection * m_fMaxRange1), MASK_SHOT_PORTAL, &traceFilterPortalShot, &tr );

	return PORTAL_ANALOG_SUCCESS_NEAR;
	}

	UTIL_TraceLine( vTraceStart - vDirection * 16.0f, vTraceStart + (vDirection * m_fMaxRange1), MASK_SHOT_PORTAL, &traceFilterPortalShot, &tr );

	return PORTAL_ANALOG_SUCCESS_OVERLAP_LINKED;
	}
	}
	*/

	// Trace to see where the portal hit
	Ray_t rayShot;
	rayShot.Init( vTraceStart, vTraceStart + (vDirection * m_fMaxRange1) );
	ComplexPortalTrace_t traceResults[16];
	int iResultSegments = UTIL_Portal_ComplexTraceRay( rayShot, MASK_SHOT_PORTAL, &traceFilterPortalShot, traceResults, ARRAYSIZE(traceResults) );

	/*
	for( int iDebugDraw = 0; iDebugDraw != iResultSegments; ++iDebugDraw )
	{
	NDebugOverlay::Line( traceResults[iDebugDraw].trSegment.startpos + Vector( 0,0,0.1), traceResults[iDebugDraw].trSegment.endpos + Vector( 0,0,0.1), 0, 255, 0, false, 15.0f );
	}
	*/

	// Stop segments early if it specifically hit a prop_portal
	for ( int i = 0; i != iResultSegments; ++i )
	{
		if( dynamic_cast<CProp_Portal *>( traceResults[i].pSegmentEndPortal ) != NULL )
		{
			// Stop the traces here, it'll make the verify placement code fizzle
			iResultSegments = i + 1;
			break;
		}
	}

	// Test for hitting a pass-thru surface
	if ( (iResultSegments == 0) || !traceResults[iResultSegments - 1].trSegment.DidHit() || traceResults[0].trSegment.startsolid )
	{
		if( iResultSegments > 0 )
		{
			placementInfo.tr = traceResults[iResultSegments - 1].trSegment;
		}

		// If it didn't hit anything, fizzle
		CProp_Portal *pPortal = bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get(); //CProp_Portal::FindPortal( m_iPortalLinkageGroupID, bPortal2, true );
		Assert( pPortal );
		if( !pPortal )
			return false;

		pPortal->m_iDelayedFailure = PORTAL_FIZZLE_NONE;
		VectorAngles( -vDirection, pPortal->m_qDelayedAngles );
		pPortal->m_vDelayedPosition = iResultSegments == 0 ? vec3_origin : traceResults[iResultSegments - 1].trSegment.endpos;

		// Give it data so the "fizzle" shot can hit somewhere
		placementInfo.vecFinalPosition	= pPortal->m_vDelayedPosition;
		placementInfo.angFinalAngles	= pPortal->m_qDelayedAngles;
		placementInfo.ePlacementResult	= PORTAL_PLACEMENT_PASSTHROUGH_SURFACE;
		return false;
	}

	// Clip this to any number of entities that can block us
	if ( PortalTraceClippedByBlockers( traceResults, iResultSegments, vDirection, bPortal2, placementInfo ) )
		return false;

	// Test for portal steal in coop after we've collided against likely targets
	if ( AttemptStealCoopPortal( placementInfo ) )
		return true;

	// Create a pseudo "up" vector from the portal if it's on the floor or ceiling
	Vector vUp( 0.0f, 0.0f, 1.0f );
	CPortal_Player *pPortalPlayer = ToPortalPlayer( GetOwner() );
	if( pPortalPlayer && ( traceResults[iResultSegments - 1].trSegment.plane.normal.x > -0.001f && traceResults[iResultSegments - 1].trSegment.plane.normal.x < 0.001f ) && ( traceResults[iResultSegments - 1].trSegment.plane.normal.y > -0.001f && traceResults[iResultSegments - 1].trSegment.plane.normal.y < 0.001f ) )
	{
		// Check if we're upright
		float dot = DotProduct( pPortalPlayer->GetPortalPlayerLocalData().m_vLocalUp, Vector(0.f,0.f,1.f) );
		if ( AlmostEqual( fabs( dot ), 1.0f ) )
		{
			// If we're upright, then the top of the portal should be away from us
			vUp = vDirection;
		}
		else
		{
			// If we're stuck on a wall, the top of the portal should be away from the wall.  This can be thought of as being
			// analog to standing on the floor and shooting a portal onto a wall.
			vUp = pPortalPlayer->GetPortalPlayerLocalData().m_StickNormal;
		}
		
	}

	// Now, build our information for placement
	VectorAngles( traceResults[iResultSegments - 1].trSegment.plane.normal, vUp, placementInfo.angFinalAngles );
	placementInfo.vecFinalPosition = traceResults[iResultSegments - 1].trSegment.endpos;

	// Find the portal we're moving
	CProp_Portal *pPlacePortal = bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get(); //CProp_Portal::FindPortal( m_iPortalLinkageGroupID, bPortal2 );

	// Portal size
	float fHalfWidth, fHalfHeight;
	CProp_Portal::GetPortalSize( fHalfWidth, fHalfHeight, pPlacePortal );

	// Hit any placement helpers at this point
	if ( AttemptSnapToPlacementHelper( pPlacePortal, traceResults, iResultSegments, ePlacedBy, placementInfo ) )
		return true;

	// Otherwise, just try to fit the portal here
	placementInfo.ePlacementResult = VerifyPortalPlacementAndFizzleBlockingPortals( pPlacePortal, placementInfo.vecFinalPosition, placementInfo.angFinalAngles, fHalfWidth, fHalfHeight, ePlacedBy );
	return PortalPlacementSucceeded( placementInfo.ePlacementResult );
}

extern int AllEdictsAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask );

//-----------------------------------------------------------------------------
// Purpose: Clips a complex trace segment against a variety of entities that
//			we'd like to collide with.
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::PortalTraceClippedByBlockers( ComplexPortalTrace_t *pTraceResults, int nNumResultSegments, const Vector &vecDirection, bool bIsSecondPortal, TracePortalPlacementInfo_t &placementInfo )
{
	// Deal with this segment of the trace
	placementInfo.tr = pTraceResults[nNumResultSegments - 1].trSegment;

	for ( int iTraceSegment = 0; iTraceSegment != nNumResultSegments; ++iTraceSegment ) //loop over all segments
	{
		ComplexPortalTrace_t &currentSegment = pTraceResults[iTraceSegment];

		// Trace to the surface to see if there's a rotating door in the way
		CBaseEntity *list[1024];
		CUtlVector<CTriggerPortalCleanser*> vFizzlersAlongRay;

		Ray_t ray;
		ray.Init( currentSegment.trSegment.startpos, currentSegment.trSegment.endpos );

		int nCount = AllEdictsAlongRay( list, 1024, ray, 0 );

		// Loop through all entities along the ray between the gun and the surface
		for ( int i = 0; i < nCount; i++ )
		{
#if 0
#if defined( GAME_DLL )
			Warning( "CWeaponPortalgun::PortalTraceClippedByBlockers(server) : %s\n", list[i]->m_iClassname );
#else
			Warning( "CWeaponPortalgun::PortalTraceClippedByBlockers(client) : %s\n", list[i]->GetClassname() );
#endif
#endif

			// If the entity is a rotating door
#if defined( GAME_DLL )
			if( FClassnameIs( list[i], "prop_door_rotating" ) )
#else
			if( FClassnameIs( list[i], "class C_PropDoorRotating" ) )
#endif			
			{
				// Check more precise door collision
				//CBasePropDoor *pRotatingDoor = static_cast<CBasePropDoor *>( list[i] );
				CBaseEntity *pRotatingDoor = list[i];

				Ray_t rayDoor;
				rayDoor.Init( currentSegment.trSegment.startpos, currentSegment.trSegment.startpos + (currentSegment.vNormalizedDelta * m_fMaxRange1) );

				trace_t trDoor;
				pRotatingDoor->TestCollision( rayDoor, 0, trDoor );

				if ( trDoor.DidHit() )
				{
					// There's a door in the way
					placementInfo.tr = trDoor;

					if ( sv_portal_placement_debug.GetBool() )
					{
						Vector vMin;
						Vector vMax;
						Vector vZero = Vector( 0.0f, 0.0f, 0.0f );
						list[ i ]->GetCollideable()->WorldSpaceSurroundingBounds( &vMin, &vMax );
						NDebugOverlay::Box( vZero, vMin, vMax, 0, 255, 0, 128, 0.5f );
					}

					CProp_Portal *pPortal = bIsSecondPortal ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get();//CProp_Portal::FindPortal( m_iPortalLinkageGroupID, bIsSecondPortal, true );
					if( pPortal )
					{
						pPortal->m_iDelayedFailure = PORTAL_FIZZLE_CANT_FIT;
						VectorAngles( trDoor.plane.normal, pPortal->m_qDelayedAngles );
						pPortal->m_vDelayedPosition = trDoor.endpos;

						placementInfo.vecFinalPosition	= pPortal->m_vDelayedPosition;
						placementInfo.angFinalAngles	= pPortal->m_qDelayedAngles;
						placementInfo.ePlacementResult	= PORTAL_PLACEMENT_CANT_FIT;
					}
					else
					{
						placementInfo.vecFinalPosition = trDoor.endpos;
						placementInfo.angFinalAngles = vec3_angle;
						placementInfo.ePlacementResult = PORTAL_PLACEMENT_CANT_FIT;
					}
					return true;
				}
			}
			else if( dynamic_cast<CTriggerPortalCleanser*>( list[i] ) != NULL )
			{
				CTriggerPortalCleanser *pTrigger = static_cast<CTriggerPortalCleanser*>( list[i] );

				if ( pTrigger && pTrigger->IsEnabled() )
				{
#if 0
#if defined( GAME_DLL )
					Warning( "CWeaponPortalgun::PortalTraceClippedByBlockers(server) : CLEANSER!!!!!\n" );
#else
					Warning( "CWeaponPortalgun::PortalTraceClippedByBlockers(client) : CLEANSER!!!!!\n" );
#endif
#endif
					vFizzlersAlongRay.AddToTail( pTrigger );
				}
			}
		}

		CTriggerPortalCleanser *pHitFizzler = NULL;
		trace_t nearestFizzlerTrace;
		float flNearestFizzler = FLT_MAX;
		bool bFizzlerHit = false;

		for ( int n = 0; n < vFizzlersAlongRay.Count(); ++n )
		{
			Vector vMin;
			Vector vMax;
			vFizzlersAlongRay[n]->GetCollideable()->WorldSpaceSurroundingBounds( &vMin, &vMax );

			trace_t fizzlerTrace;
			IntersectRayWithBox( currentSegment.trSegment.startpos, currentSegment.trSegment.endpos - currentSegment.trSegment.startpos, vMin, vMax, 0.0f, &fizzlerTrace );

			float flDist = ( fizzlerTrace.endpos - fizzlerTrace.startpos ).LengthSqr();

			if ( flDist < flNearestFizzler )
			{
				flNearestFizzler = flDist;
				pHitFizzler = vFizzlersAlongRay[n];
				nearestFizzlerTrace = fizzlerTrace;
				bFizzlerHit = true;
			}
		}

		if ( bFizzlerHit )
		{
			placementInfo.tr = nearestFizzlerTrace;

			placementInfo.tr.plane.normal = -vecDirection;

#if defined( GAME_DLL )
			pHitFizzler->SetPortalShot();
#endif

			CProp_Portal *pPortal = bIsSecondPortal ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get();//CProp_Portal::FindPortal( m_iPortalLinkageGroupID, bIsSecondPortal, true );
			if( pPortal )
			{
				pPortal->m_iDelayedFailure = PORTAL_FIZZLE_CLEANSER;
				VectorAngles( placementInfo.tr.plane.normal, pPortal->m_qDelayedAngles );
				pPortal->m_vDelayedPosition = placementInfo.tr.endpos;

				placementInfo.vecFinalPosition	= pPortal->m_vDelayedPosition;
				placementInfo.angFinalAngles	= pPortal->m_qDelayedAngles;
				placementInfo.ePlacementResult	= PORTAL_PLACEMENT_CLEANSER;
			}
			else
			{
				placementInfo.vecFinalPosition = placementInfo.tr.endpos;
				VectorAngles( placementInfo.tr.plane.normal, placementInfo.angFinalAngles );
				placementInfo.ePlacementResult = PORTAL_PLACEMENT_CLEANSER;
			}
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: In a coop game we'd like to steal our partner's portals if we try 
//			to place right on top of their's
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::AttemptStealCoopPortal( TracePortalPlacementInfo_t &placementInfo )
{
	if( !GameRules()->IsMultiplayer() )
	{
		return false;
	}

	CPortalMPGameRules *pRules = PortalMPGameRules();
	if( pRules && pRules->IsVS() )
	{
		return false;
	}

	const int iAllPortalCount = CProp_Portal_Shared::AllPortals.Count();
	CProp_Portal *pHitPortal = dynamic_cast<CProp_Portal *>( placementInfo.tr.m_pEnt );
	if ( pHitPortal == NULL && (CProp_Portal_Shared::AllPortals.Count() != 0) )
	{
		//also check active unlinked portals at end of trace
		CProp_Portal **pAllPortals = CProp_Portal_Shared::AllPortals.Base();
		CProp_Portal **pUnlinkedPortals = (CProp_Portal **)stackalloc( sizeof( CProp_Portal * ) * iAllPortalCount );
		int iUnlinkedPortalCount = 0;
		for ( int i = 0; i != iAllPortalCount; ++i )
		{
			if ( pAllPortals[i]->IsActive() && (pAllPortals[i]->m_hLinkedPortal.Get() == NULL) )
			{
				pUnlinkedPortals[iUnlinkedPortalCount] = pAllPortals[i];
				iUnlinkedPortalCount++;
			}
		}

		if ( iUnlinkedPortalCount )
		{
			pHitPortal = assert_cast<CProp_Portal *>( UTIL_PointIsOnPortalQuad( placementInfo.tr.endpos, 1.0f, (CPortal_Base2D **)pUnlinkedPortals, iUnlinkedPortalCount ) );
		}
	}

	// If we didn't hit a portal, we're done
	if ( pHitPortal == NULL )
		return false;

	// Otherwise, check for "stealing" conditions being met
	CBasePlayer *pFiredBy = pHitPortal->GetFiredByPlayer();
	CBaseCombatCharacter *pOwner = GetOwner();
	if( (pOwner != NULL) && (pFiredBy != NULL) && (pOwner != pFiredBy) )
	{
		// Take this portal's position exactly
		placementInfo.vecFinalPosition	= pHitPortal->GetAbsOrigin();
		placementInfo.angFinalAngles	= pHitPortal->GetAbsAngles();
		placementInfo.ePlacementResult	= PORTAL_PLACEMENT_SUCCESS;

#if defined( CLIENT_DLL )
		if( C_BasePlayer::IsLocalPlayer( pFiredBy ) )
#endif
		{
			pHitPortal->DoFizzleEffect( PORTAL_FIZZLE_CLEANSER );
			pHitPortal->Fizzle();
			pHitPortal->SetActive( false );	// HACK: For replacing the portal, we need this!

#ifdef GAME_DLL
			// Update the hud quickinfo to indicate that your portal was fizzled
			CBaseCombatWeapon *pWeapon = pFiredBy->GetActiveWeapon();
			if ( pWeapon )
			{
				CWeaponPortalgun *pPortalGun = dynamic_cast< CWeaponPortalgun* >( pWeapon );
				if ( pPortalGun )
				{
					pPortalGun->UpdatePortalAssociation();
				}
			}
#endif
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Try to fit a portal using placement helpers
//-----------------------------------------------------------------------------
bool CWeaponPortalgun::AttemptSnapToPlacementHelper( CProp_Portal *pPortal, ComplexPortalTrace_t *pTraceResults, int nNumResultSegments, PortalPlacedBy_t ePlacedBy, TracePortalPlacementInfo_t &placementInfo )
{
	// First, find a helper in the general area we hit
	CInfoPlacementHelper *pHelper = UTIL_FindPlacementHelper( placementInfo.vecFinalPosition, (GetOwner() && GetOwner()->IsPlayer()) ? (CBasePlayer *)GetOwner() : NULL );
	if ( pHelper == NULL )
		return false;

#if defined( GAME_DLL )
	if ( sv_portal_placement_debug.GetBool() )
	{
		Msg("PortalPlacement: Found placement helper centered at %f, %f, %f. Radius %f\n", XYZ(pHelper->GetAbsOrigin()), pHelper->GetTargetRadius() );
	}
#endif

	// this thing should be the results of the original trace (non-helped)
	trace_t& tr = pTraceResults[nNumResultSegments - 1].trSegment;

	Assert( !tr.plane.normal.IsZero() );

	// Setup a trace filter that ignore / hits everything we care about
#if defined( GAME_DLL )
	CTraceFilterSimpleClassnameList baseFilter( this, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &baseFilter );
	CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );
#else
	CTraceFilterSimpleClassnameList traceFilterPortalShot( this, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &traceFilterPortalShot );
#endif

	// re-hit the area near the center of the placement helper. Very small trace is fine
	Vector vecStartPos = tr.plane.normal + pHelper->GetAbsOrigin();
	Vector vecDir = -tr.plane.normal;
	VectorNormalize( vecDir );
	trace_t trHelper;
	UTIL_TraceLine( vecStartPos, vecStartPos + vecDir*m_fMaxRange1, MASK_SHOT_PORTAL, &traceFilterPortalShot, &trHelper );
	Assert ( trHelper.DidHit() );

	// Use the helper angles, if specified
	QAngle qHelperAngles = ( pHelper->ShouldUseHelperAngles() ) ? ( pHelper->GetTargetAngles() ) : placementInfo.angFinalAngles;

	if ( sv_portal_placement_debug.GetBool() )
	{
		Msg("PortalPlacement: Using placement helper angles %f %f %f\n", XYZ(pHelper->GetTargetAngles()));
	}

	float fHalfWidth, fHalfHeight;
	CProp_Portal::GetPortalSize( fHalfWidth, fHalfHeight, pPortal );

	Vector vHelperFinalPos = trHelper.endpos;

	bool bPlacementOnHelperValid = true;

	// make sure the normals match
	if ( VectorsAreEqual( trHelper.plane.normal, tr.plane.normal, FLT_EPSILON ) == false )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			Msg("PortalPlacement: Not using placement helper because the surface normal of the portal's resting surface and the placement helper's intended surface do not match\n" );
		}
		bPlacementOnHelperValid = false;
	}

	//make sure distance is a sane amount
	Vector vecHelperToHitPoint = tr.endpos - trHelper.endpos;
	float flLenSq = vecHelperToHitPoint.LengthSqr();
	if ( flLenSq > (pHelper->GetTargetRadius()*pHelper->GetTargetRadius()) )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			Msg("PortalPlacement:Not using placement helper because the Portal's final position was outside the helper's radius!\n" );
		}
		bPlacementOnHelperValid = false;
	}

	if( bPlacementOnHelperValid )
	{
		PortalPlacementResult_t eResult = VerifyPortalPlacementAndFizzleBlockingPortals( pPortal, vHelperFinalPos, qHelperAngles, fHalfWidth, fHalfHeight, ePlacedBy );
		
		// run normal placement validity checks
		if ( PortalPlacementSucceeded( eResult ) == false )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				Msg("PortalPlacement: Not using placement helper because portal could not fit in a valid spot at it's origin and angles\n" );
			}

			bPlacementOnHelperValid = false;
		}
	}

	if ( bPlacementOnHelperValid )
	{
		placementInfo.vecFinalPosition	= vHelperFinalPos;
		placementInfo.angFinalAngles	= qHelperAngles;
		placementInfo.pPlacementHelper	= pHelper;
		placementInfo.ePlacementResult	= PORTAL_PLACEMENT_USED_HELPER;
		return true;
	}

	return false;
}



CProp_Portal *CWeaponPortalgun::GetAssociatedPortal( bool bPortal2 )
{
	CProp_Portal *pRetVal = bPortal2 ? m_hSecondaryPortal.Get() : m_hPrimaryPortal.Get();

#if defined( GAME_DLL )
	if( pRetVal == NULL )
	{
		pRetVal = CProp_Portal::FindPortal( m_iPortalLinkageGroupID, bPortal2, true );

		if( pRetVal != NULL )
		{
			if( bPortal2 )
			{
				m_hSecondaryPortal = pRetVal;
			}
			else
			{
				m_hPrimaryPortal = pRetVal;
			}
		}
	}
#endif

	return pRetVal;
}

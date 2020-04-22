//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "gamerules.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "weapon_basecsgrenade.h"
#include "in_buttons.h"	
#include "datacache/imdlcache.h"

#include "cs_shareddefs.h"

#ifdef CLIENT_DLL

	#include "c_cs_player.h"
	#include "HUD/sfweaponselection.h"
	#include "c_rumble.h"
	#include "rumble_shared.h"
#else

	#include "cs_player.h"
	#include "items.h"
	#include "cs_gamestats.h"

#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


#define GRENADE_TIMER	1.5f //Seconds


IMPLEMENT_NETWORKCLASS_ALIASED( BaseCSGrenade, DT_BaseCSGrenade )

BEGIN_NETWORK_TABLE(CBaseCSGrenade, DT_BaseCSGrenade)

#ifndef CLIENT_DLL
	SendPropBool( SENDINFO(m_bRedraw) ),
	SendPropBool( SENDINFO(m_bIsHeldByPlayer) ),
	SendPropBool( SENDINFO(m_bPinPulled) ),
	SendPropFloat( SENDINFO(m_fThrowTime), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bLoopingSoundPlaying ) ),
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	SendPropFloat( SENDINFO(m_flThrowStrength), 0, SPROP_NOSCALE ),
#endif
#else
	RecvPropBool( RECVINFO(m_bRedraw) ),
	RecvPropBool( RECVINFO(m_bIsHeldByPlayer) ),
	RecvPropBool( RECVINFO(m_bPinPulled) ),
	RecvPropFloat( RECVINFO(m_fThrowTime) ),
	RecvPropBool( RECVINFO( m_bLoopingSoundPlaying ) ),
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	RecvPropFloat( RECVINFO(m_flThrowStrength) ),
#endif
#endif

END_NETWORK_TABLE()

#if defined CLIENT_DLL
BEGIN_PREDICTION_DATA( CBaseCSGrenade )
	DEFINE_PRED_FIELD( m_bRedraw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bPinPulled, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	DEFINE_PRED_FIELD( m_flThrowStrength, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_basecsgrenade, BaseCSGrenade );

#ifndef CLIENT_DLL
ConVar sv_ignoregrenaderadio( "sv_ignoregrenaderadio", "0", FCVAR_RELEASE, "Turn off Fire in the hole messages" );
#endif

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	#define GRENADE_SECONDARY_DAMPENING 0.3f
	#define GRENADE_SECONDARY_LOWER 12.0f
	#define GRENADE_SECONDARY_TRANSITION 1.3f
	#define GRENADE_SECONDARY_INTERP 2.0f
#endif

CBaseCSGrenade::CBaseCSGrenade()
{
	m_bRedraw = false;
	m_bIsHeldByPlayer = false;
	m_bPinPulled = false;
	m_fThrowTime = 0;
	m_bLoopingSoundPlaying = false;
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	m_flThrowStrength = 1.0f;
	m_flThrowStrengthClientSmooth = 1.0f;
#endif

#ifndef CLIENT_DLL
	m_bHasEmittedProjectile = false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::Precache()
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseCSGrenade::Deploy()
{
	m_bRedraw = false;
	m_bIsHeldByPlayer = true;
	m_bPinPulled = false;
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	m_flThrowStrength = 1.0f;
	m_flThrowStrengthClientSmooth = 1.0f;
#endif
	m_fThrowTime = 0;
#ifndef CLIENT_DLL
	// if we're officially out of grenades, ditch this weapon
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
	{
		pPlayer->Weapon_Drop( this, NULL, NULL );
		UTIL_Remove(this);
		return false;
	}
#endif

	return BaseClass::Deploy();
}

#ifdef CLIENT_DLL
int CBaseCSGrenade::DrawModel( int flags, const RenderableInstance_t &instance )
{
	//hide the grenade that's in the player's hand while playing grenade throwing animations
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		if ( !pPlayer->m_bUseNewAnimstate && pPlayer->m_PlayerAnimState && pPlayer->m_PlayerAnimState->ShouldHideGrenadeDuringThrow() )
		{
			return 0;
		}
	}
	
	return BaseClass::DrawModel( flags, instance );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseCSGrenade::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bRedraw = false;
	// we don't want to set m_bIsHeldByPlayer to true because the weapon actually holsters before it's removed from the inventory after the last grenade has been thrown
	// this causes a visual bug in the weapon selection UI
	m_bPinPulled = false; // when this is holstered make sure the pin isnt pulled.
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	m_flThrowStrength = 1.0f;
	m_flThrowStrengthClientSmooth = 1.0f;
#endif
	m_fThrowTime = 0;
#ifndef CLIENT_DLL
	// If they attempt to switch weapons before the throw animation is done, 
	// allow it, but kill the weapon if we have to.
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
	{
		CBaseCombatCharacter *pOwner = (CBaseCombatCharacter *)pPlayer;
		pOwner->Weapon_Drop( this );
		UTIL_Remove(this);
	}
#endif

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::PrimaryAttack()
{
	if ( !m_bIsHeldByPlayer || m_bPinPulled || m_fThrowTime > 0.0f )
		return;

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer || pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
		return;

	// Ensure that the player can use this grenade
	if ( !pPlayer->CanUseGrenade( GetCSWeaponID() ) )
	{
		return;
	}

#ifndef CLIENT_DLL
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_GRENADE_PULL_PIN );
#endif

	// The pull pin animation has to finish, then we wait until they aren't holding the primary
	// attack button, then throw the grenade.
	SendWeaponAnim( ACT_VM_PULLPIN );
	m_bPinPulled = true;

	// Don't let weapon idle interfere in the middle of a throw!
	MDLCACHE_CRITICAL_SECTION();
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

	m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::SecondaryAttack()
{

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	if ( !m_bPinPulled )
	{
		m_flThrowStrength = 0.0f;
		m_flThrowStrengthClientSmooth = 0.0f;
	}

	if ( CSGameRules()->IsFreezePeriod() )	// Don't let Brian molotov the team during freezetime
		return;

	PrimaryAttack();
	return;
#endif

	/*if ( m_bRedraw )
		return;

	CCSPlayer *pPlayer = GetPlayerOwner();
	
	if ( pPlayer == NULL )
		return;

	//See if we're ducking
	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		//Send the weapon animation
		SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	}
	else
	{
		//Send the weapon animation
		SendWeaponAnim( ACT_VM_HAULBACK );
	}

	// Don't let weapon idle interfere in the middle of a throw!
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

	m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseCSGrenade::Reload()
{
	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();

		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
		
		//Mark this as done
	//	m_bRedraw = false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPicker - 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	BaseClass::OnPickedUp( pNewOwner );

#if !defined( CLIENT_DLL )
	if ( pNewOwner )
	{
		m_bIsHeldByPlayer = true;
	}
#endif
}


void CBaseCSGrenade::ItemPreFrame()
{
	BaseClass::ItemPreFrame();

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

#ifdef CLIENT_DLL
	//we want to control the grenade model's visibility so opt out of the fast path
	if (GetBaseAnimating())
		GetBaseAnimating()->SetAllowFastPath(false);
#endif

}

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
float CBaseCSGrenade::ApproachThrownStrength()
{
	 m_flThrowStrengthClientSmooth = Approach( 
			m_flThrowStrength, 
			m_flThrowStrengthClientSmooth, 
			gpGlobals->frametime * GRENADE_SECONDARY_INTERP
			);
	 return m_flThrowStrengthClientSmooth;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCSGrenade::ItemPostFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	CBaseViewModel *vm = pPlayer->GetViewModel( m_nViewModelIndex );
	if ( !vm )
		return;

	bool bPrimaryHeld = (pPlayer->m_nButtons & IN_ATTACK) != 0;
	bool bSecondaryHeld = (pPlayer->m_nButtons & IN_ATTACK2) != 0;
	
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	if ( m_bPinPulled && ( bPrimaryHeld || bSecondaryHeld ) )
	{
		float flIdealThrowStrength = 0.5f;

		if ( bPrimaryHeld )
			flIdealThrowStrength += 0.5f;

		if ( bSecondaryHeld )
			flIdealThrowStrength -= 0.5f;

		m_flThrowStrength = Approach( flIdealThrowStrength, m_flThrowStrength, gpGlobals->frametime * GRENADE_SECONDARY_TRANSITION );
	}
#endif

	// If they let go of the fire buttons, they want to throw the grenade.
	if ( m_bPinPulled && !(bPrimaryHeld) && !(bSecondaryHeld) ) 
	{
#ifndef CLIENT_DLL
		#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
		if ( IsThrownUnderhand() )
		{
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_THROW_GRENADE_UNDERHAND );
		}
		else
		#endif
		{
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_THROW_GRENADE );
		}
#endif

		StartGrenadeThrow();
		
		MDLCACHE_CRITICAL_SECTION();
		m_bPinPulled = false;
		
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
		if ( IsThrownUnderhand() )
		{
			SendWeaponAnim( ACT_VM_RELEASE );
		}
		else
#endif
		{
			SendWeaponAnim( ACT_VM_THROW );	
		}
		
		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

		m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration(); // we're still throwing, so reset our next primary attack

#ifndef CLIENT_DLL
		IGameEvent * event = gameeventmanager->CreateEvent( "weapon_fire" );
		if( event )
		{
			const char *weaponName = STRING( m_iClassname );
			if ( IsWeaponClassname( weaponName ) )
			{
				weaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
			}

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetString( "weapon", weaponName );
			event->SetBool( "silenced", false );
			gameeventmanager->FireEvent( event );
		}

#else
		RumbleEffect( XBX_GetUserId( pPlayer->GetSplitScreenPlayerSlot() ), RUMBLE_CROWBAR_SWING, 0, RUMBLE_FLAG_RESTART );
#endif
	}
	else if ((m_fThrowTime > 0) && (m_fThrowTime < gpGlobals->curtime))
	{
		// only decrement our ammo when we actually create the projectile
		DecrementAmmo( pPlayer );

		ThrowGrenade();
	}
	else if( !m_bIsHeldByPlayer )
	{
		// Has the throw animation finished playing
		if( m_flTimeWeaponIdle < gpGlobals->curtime )
		{
			// if we're officially out of grenades, ditch this weapon
			int nAmmoCount = pPlayer->GetAmmoCount(m_iPrimaryAmmoType);
			if( nAmmoCount <= 0 )
			{
				pPlayer->Weapon_Drop( this, NULL, NULL );
#ifndef CLIENT_DLL
				//pPlayer->RemoveWeaponOnPlayer( this );
				UTIL_Remove(this);
#endif
			}

			else
			{
				pPlayer->SwitchToNextBestWeapon( this );
			}
#if defined (CLIENT_DLL)
			// when a grenade is removed, force the local player to update thier inventory screen
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( pLocalPlayer && pLocalPlayer == pPlayer )
			{
				SFWeaponSelection *pHudWS = GET_HUDELEMENT( SFWeaponSelection );
				if ( pHudWS )
				{
					int nAmmoCount = pPlayer->GetAmmoCount(m_iPrimaryAmmoType);
					if ( nAmmoCount <= 0 )
					{
						pHudWS->ShowAndUpdateSelection( WEPSELECT_DROP, this );
					}
					else
					{
						// we need to tell the hud that this weapon still exists and then update the selected weapon
						pHudWS->ShowAndUpdateSelection( WEPSELECT_PICKUP, this );
					}
				}
			}
#endif
			return;	//don't animate this grenade any more!
		}	
	}
	else if( !m_bRedraw )
	{
		BaseClass::ItemPostFrame();
	}
}



#ifdef CLIENT_DLL

	void CBaseCSGrenade::DecrementAmmo( CBaseCombatCharacter *pOwner )
	{
	}

	void CBaseCSGrenade::DropGrenade()
	{
		m_bRedraw = true;
		m_bIsHeldByPlayer = false;
		m_fThrowTime = 0.0f;
	}

	void CBaseCSGrenade::ThrowGrenade()
	{
		m_bRedraw = true;
		m_bIsHeldByPlayer = false;
		m_fThrowTime = 0.0f;

		CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
		if ( pHudSelection )
		{
			pHudSelection->OnWeaponDrop( this );
		}
	}

	void CBaseCSGrenade::StartGrenadeThrow()
	{
		m_fThrowTime = gpGlobals->curtime + 0.1f;
	}

#else

	BEGIN_DATADESC( CBaseCSGrenade )
		DEFINE_FIELD( m_bRedraw, FIELD_BOOLEAN ),
		DEFINE_FIELD( m_bIsHeldByPlayer, FIELD_BOOLEAN ),
	END_DATADESC()

	int CBaseCSGrenade::CapabilitiesGet()
	{
		return bits_CAP_WEAPON_RANGE_ATTACK1; 
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : *pOwner - 
	//-----------------------------------------------------------------------------
	void CBaseCSGrenade::DecrementAmmo( CBaseCombatCharacter *pOwner )
	{
		pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
	}

	void CBaseCSGrenade::StartGrenadeThrow()
	{
		m_fThrowTime = gpGlobals->curtime + 0.1f;

		CBroadcastRecipientFilter filter;
		CSoundParameters params;
		if ( GetParametersForSound( GetShootSound( SINGLE ), params, NULL ) )
		{
			//CPASAttenuationFilter filter( this );
			EmitSound( filter, entindex(), GetShootSound( SINGLE )); 
		}

		//WeaponSound(SINGLE, gpGlobals->curtime + 3.0f);
	}

	void CBaseCSGrenade::ThrowGrenade()
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( !pPlayer )
		{
			Assert( false );
			return;
		}

		QAngle angThrow = pPlayer->GetFinalAimAngle();

		if ( angThrow[PITCH] > 90.0f )
		{
			angThrow[PITCH] -= 360.0f;
		}
		else if ( angThrow[PITCH] < -90.0f )
		{
			angThrow[PITCH] += 360.0f;
		}

		AssertMsg( angThrow[PITCH] <= 90.0f && angThrow[PITCH] >= -90.0f, "Grenade throw pitch angle must be between -90 and 90 for the adustments to work.");

		// NB. a pitch of +90 is looking straight down, -90 is looking straight up

		// add a 10 degrees upwards angle to the throw when looking horizontal, lerp the upwards boost to 0 at the pitch extremes
		angThrow[PITCH] -= 10.0f * (90.0f - fabsf(angThrow[PITCH])) / 90.0f;

		const float kBaseVelocity = GetThrowVelocity();
		//const float kThrowVelocityClampRatio = 750.0f / 540.0f;	// from original CSS values
			
		//float flVel = clamp((90 - angThrow.x) / 90, 0.0f, kThrowVelocityClampRatio) * kBaseVelocity;
		float flVel = clamp( (kBaseVelocity * 0.9f), 15, 750 );

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
		//clamp the throw strength ranges just to be sure
		float flClampedThrowStrength = m_flThrowStrength;
		flClampedThrowStrength = clamp( flClampedThrowStrength, 0.0f, 1.0f );

		flVel *= Lerp( flClampedThrowStrength, GRENADE_SECONDARY_DAMPENING, 1.0f );
#endif
		Vector vForward;
		AngleVectors( angThrow, &vForward );

		Vector vecSrc = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset();

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
		vecSrc += Vector(0, 0, Lerp( flClampedThrowStrength, -GRENADE_SECONDARY_LOWER, 0.0f ) );
#endif
		// We want to throw the grenade from 16 units out.  But that can cause problems if we're facing
		// a thin wall.  Do a hull trace to be safe.
		// Wills: Moved the trace length out to 22 inches, then subtract 6. This way we default to 16, 
		// but pull back 6 from wherever we hit, so we don't emit from EXACTLY inside the close surface, which can lead to 
		// the grenade penetrating the wall anyway.
		trace_t trace;
		Vector mins( -2, -2, -2 );
		Vector maxs(  2,  2,  2 );
		UTIL_TraceHull( vecSrc, vecSrc + vForward * 22, mins, maxs, MASK_SOLID | CONTENTS_GRENADECLIP, pPlayer, COLLISION_GROUP_NONE, &trace );
		vecSrc = trace.endpos - (vForward * 6);

		Vector vecThrow = vForward * flVel + (pPlayer->GetAbsVelocity() * 1.25);

		EmitGrenade( vecSrc, vec3_angle, vecThrow, AngularImpulse(600,random->RandomInt(-1200,1200),0), pPlayer, GetCSWpnData() );

		m_bHasEmittedProjectile = true; // Flag the grenade weapon as having emitted a projectile. The 'grenade' is now flying away from the player, so we don't want to drop *this* grenade on death (that'll make a duplicate)

		m_bRedraw = true;
		m_bIsHeldByPlayer = false;
		m_fThrowTime = 0.0f;

		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

		if ( pCSPlayer )
		{
			int iWeaponId = GetCSWeaponID();

			pCSPlayer->PlayerUsedGrenade( iWeaponId );

			if ( !sv_ignoregrenaderadio.GetBool() )
			{
				if ( iWeaponId == WEAPON_FLASHBANG )
					pCSPlayer->Radio( "Radio.Flashbang",   "#SFUI_TitlesTXT_Flashbang_in_the_hole", true );
				else if ( iWeaponId == WEAPON_SMOKEGRENADE )
					pCSPlayer->Radio( "Radio.Smoke",   "#SFUI_TitlesTXT_Smoke_in_the_hole", true );
				else if ( iWeaponId == WEAPON_MOLOTOV )
					pCSPlayer->Radio( "Radio.Molotov",   "#SFUI_TitlesTXT_Molotov_in_the_hole", true );
				else if ( iWeaponId == WEAPON_INCGRENADE )
					pCSPlayer->Radio( "Radio.Incendiary",   "#SFUI_TitlesTXT_Incendiary_in_the_hole", true );
				else if ( iWeaponId == WEAPON_DECOY )
					pCSPlayer->Radio( "Radio.Decoy",   "#SFUI_TitlesTXT_Decoy_in_the_hole", true );
				else
					pCSPlayer->Radio( "Radio.FireInTheHole",   "#SFUI_TitlesTXT_Fire_in_the_hole", true );
			}
			CCS_GameStats.IncrementStat( pCSPlayer, CSSTAT_GRENADES_THROWN, 1 );
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "grenade_thrown" );
		if ( event )
		{
			const char *weaponName = STRING( m_iClassname );
			if ( IsWeaponClassname( weaponName ) )
			{
				weaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
			}

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetString( "weapon", weaponName );
			gameeventmanager->FireEvent( event );
		}
	}

	void CBaseCSGrenade::DropGrenade()
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( !pPlayer )
		{
			Assert( false );
			return;
		}

		Vector vForward;
		pPlayer->EyeVectors( &vForward );
		Vector vecSrc = pPlayer->GetAbsOrigin() + pPlayer->GetViewOffset() + vForward * 16; 

		Vector vecVel = pPlayer->GetAbsVelocity();

		EmitGrenade( vecSrc, vec3_angle, vecVel, AngularImpulse(600,random->RandomInt(-1200,1200),0), pPlayer, GetCSWpnData() );

		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
		if( pCSPlayer )
		{
			CCS_GameStats.IncrementStat( pCSPlayer, CSSTAT_GRENADES_THROWN, 1 );
		}

		m_bRedraw = true;
		m_bIsHeldByPlayer = false;
		m_fThrowTime = 0.0f;
	}

	void CBaseCSGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
	{
		Assert( 0 && "CBaseCSGrenade::EmitGrenade should not be called. Make sure to implement this in your subclass!\n" );
	}

	bool CBaseCSGrenade::AllowsAutoSwitchFrom( void ) const
	{
		return !m_bPinPulled;
	}

#endif


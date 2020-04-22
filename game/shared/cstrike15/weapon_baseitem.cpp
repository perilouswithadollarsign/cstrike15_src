//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: base class for belt items, eg pills and adrenaline
//
// $NoKeywords: $
//=====================================================================================//

#include "cbase.h"
#include "weapon_baseitem.h"
#include "cs_gamerules.h"

#if defined( CLIENT_DLL )
#include "c_cs_player.h"
#include "HUD/sfweaponselection.h"
#else
#include "cs_player.h"
#endif // CLIENT_DLL

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponBaseItem, DT_WeaponBaseItem )

BEGIN_NETWORK_TABLE( CWeaponBaseItem, DT_WeaponBaseItem )
#ifndef CLIENT_DLL
SendPropBool( SENDINFO( m_bRedraw ) ),
#else
RecvPropBool( RECVINFO( m_bRedraw ) ),
#endif

END_NETWORK_TABLE()

#if defined CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponBaseItem )
DEFINE_PRED_FIELD( m_bRedraw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif


#ifndef CLIENT_DLL
BEGIN_DATADESC( CWeaponBaseItem )
DEFINE_FIELD( m_bRedraw, FIELD_BOOLEAN ),
END_DATADESC()
#endif

CWeaponBaseItem::CWeaponBaseItem()
{
	m_bRedraw = false;
}

//--------------------------------------------------------------------------------------------------------
void CWeaponBaseItem::Spawn( void )
{
	m_UseTimer.Invalidate();
	BaseClass::Spawn();
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
}


//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::Deploy( void )
{
	m_bRedraw = false;
	m_UseTimer.Invalidate();
	return BaseClass::Deploy();
}


//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bRedraw = false;
	m_UseTimer.Invalidate();
	return BaseClass::Holster( pSwitchingTo );
}

//--------------------------------------------------------------------------------------------------
// bool CWeaponBaseItem::CanExtendHelpingHand( void ) const
// {
// 	return !m_UseTimer.HasStarted() && BaseClass::CanExtendHelpingHand();
// }


//--------------------------------------------------------------------------------------------------------
void CWeaponBaseItem::PrimaryAttack( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if (pPlayer == NULL)
		return;

	if ( m_UseTimer.HasStarted() )
	{
		return;
	}

// 	if ( HelpingHandPrimaryAttack() )
// 	{
// 		return;
// 	}

	if ( !CanUseOnSelf( pPlayer ) )
		return;

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	OnStartUse( pPlayer );

	m_UseTimer.Start( GetUseTimerDuration() );
}

float CWeaponBaseItem::GetUseTimerDuration( void )
{
	return SequenceDuration();
}


extern ConVar z_use_belt_item_tolerance;

//--------------------------------------------------------------------------------------------------------
void CWeaponBaseItem::SecondaryAttack( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if (pPlayer == NULL)
		return;

	if ( m_UseTimer.HasStarted() )
		return;

// 	static const float GiveRange = 256.0f;
// 	CCSPlayer *target = ToCSPlayer( pPlayer->FindUseEntity( GiveRange, 0.0f, z_use_belt_item_tolerance.GetFloat(), NULL, true ) ); // Prefer to hit players
// 	if ( target && target->IsOnASurvivorTeam() && !target->IsIncapacitated() )
// 	{
// #ifdef GAME_DLL
// 		pPlayer->GiveActiveWeapon( target );
// #endif
// 		return;
// 	}

	BaseClass::SecondaryAttack();
}


//--------------------------------------------------------------------------------------------------------
/**
* Called when no buttons are pressed
*/
void CWeaponBaseItem::WeaponIdle( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponBaseItem::Reload()
{
	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();

		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

		//Mark this as done
		//	m_bRedraw = false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------
/**
* Called each frame by the player PostThink
*/
void CWeaponBaseItem::ItemPostFrame( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

#ifndef CLIENT_DLL
	int buttons = pPlayer->m_nButtons;
#endif

	BaseClass::ItemPostFrame();

	if ( m_UseTimer.HasStarted() && m_UseTimer.IsElapsed() )
	{
		// pills can only help you so much
		CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
		if ( !pPlayer || !pPlayer->IsAlive() )
		{
			m_UseTimer.Invalidate();
			return;
		}
#ifndef CLIENT_DLL		
		CompleteUse( pPlayer );
		// BaseClass::ItemPostFrame() clears IN_ATTACK2, so we restore it here to prevent the next weapon from bashing immediately
		pPlayer->m_nButtons = buttons;

		m_bRedraw = true;
#endif
		m_UseTimer.Invalidate();

		// remove the ammo
		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );

		if ( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		{
			pPlayer->Weapon_Drop( this, NULL, NULL );
#ifndef CLIENT_DLL	
			UTIL_Remove( this );
#endif
		}
		else
		{
			pPlayer->SwitchToNextBestWeapon( this );
		}

#if defined (CLIENT_DLL)
		// when an item is removed, force the local player to update their inventory screen
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer && pLocalPlayer == pPlayer )
		{
			SFWeaponSelection *pHudWS = GET_HUDELEMENT( SFWeaponSelection );
			if ( pHudWS )
			{
				int nAmmoCount = pPlayer->GetAmmoCount( m_iPrimaryAmmoType );
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
	}

}

// //--------------------------------------------------------------------------------------------------------
// bool CWeaponBaseItem::OnHit( trace_t &trace, const Vector &swingVector, bool firstTime ) 
// {
// 	if ( trace.m_pEnt && trace.m_pEnt->IsPlayer() && IsASurvivorTeam( trace.m_pEnt->GetTeamNumber() ) )
// 		return false;	// don't hit survivors who are outside of heal range if we're trying to get close and heal them.
// 
// 	return BaseClass::OnHit( trace, swingVector, firstTime );
// }

//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::SendWeaponAnim( int iActivity )
{
	//iActivity = TranslateViewmodelActivity( (Activity)iActivity );
	return BaseClass::SendWeaponAnim( iActivity );
}

//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::CanFidget( void )
{
	return false;
}


//--------------------------------------------------------------------------------------------------------

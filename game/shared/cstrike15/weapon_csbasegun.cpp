//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"
#include "fx_cs_shared.h"
#include "in_buttons.h"	

#ifdef CLIENT_DLL
#include "c_cs_player.h"
#include "cs_client_gamestats.h"
#include "cdll_client_int.h"
#else
#include "cs_player.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define DETACHABLE_SILENCER 1
#define SILENCER_BODYGROUP_UNSET -2

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCSBaseGun, DT_WeaponCSBaseGun )

	BEGIN_NETWORK_TABLE( CWeaponCSBaseGun, DT_WeaponCSBaseGun )
#if defined( GAME_DLL )
	SendPropInt( SENDINFO( m_zoomLevel ), 2, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iBurstShotsRemaining ) ),
#else
	RecvPropInt( RECVINFO( m_zoomLevel ) ),
	RecvPropInt( RECVINFO( m_iBurstShotsRemaining ) ),
#endif

	END_NETWORK_TABLE()

#if defined( CLIENT_DLL )
	BEGIN_PREDICTION_DATA( CWeaponCSBaseGun )
	DEFINE_PRED_FIELD( m_zoomLevel, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iBurstShotsRemaining, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fNextBurstShot, FIELD_FLOAT, 0 ),
	END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_csbase_gun, WeaponCSBaseGun );

LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( AK47, WeaponCSBaseGun, DT_WeaponAK47, weapon_ak47 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponAug, WeaponCSBaseGun, DT_WeaponAug, weapon_aug );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponAWP, WeaponCSBaseGun, DT_WeaponAWP, weapon_awp );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponBizon, WeaponCSBaseGun, DT_WeaponBizon, weapon_bizon );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponFamas, WeaponCSBaseGun, DT_WeaponFamas, weapon_famas );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponFiveSeven, WeaponCSBaseGun, DT_WeaponFiveSeven, weapon_fiveseven );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponG3SG1, WeaponCSBaseGun, DT_WeaponG3SG1, weapon_g3sg1 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponGalil, WeaponCSBaseGun, DT_WeaponGalil, weapon_galil );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponGalilAR, WeaponCSBaseGun, DT_WeaponGalilAR, weapon_galilar );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponGlock, WeaponCSBaseGun, DT_WeaponGlock, weapon_glock );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponHKP2000, WeaponCSBaseGun, DT_WeaponHKP2000, weapon_hkp2000 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponM4A1, WeaponCSBaseGun, DT_WeaponM4A1, weapon_m4a1 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponMAC10, WeaponCSBaseGun, DT_WeaponMAC10, weapon_mac10 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponMag7, WeaponCSBaseGun, DT_WeaponMag7, weapon_mag7 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponMP5Navy, WeaponCSBaseGun, DT_WeaponMP5Navy, weapon_mp5navy );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponMP7, WeaponCSBaseGun, DT_WeaponMP7, weapon_mp7 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponMP9, WeaponCSBaseGun, DT_WeaponMP9, weapon_mp9 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponNegev, WeaponCSBaseGun, DT_WeaponNegev, weapon_negev );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponP228, WeaponCSBaseGun, DT_WeaponP228, weapon_p228 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponP250, WeaponCSBaseGun, DT_WeaponP250, weapon_p250 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponP90, WeaponCSBaseGun, DT_WeaponP90, weapon_p90 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( SCAR17, WeaponCSBaseGun, DT_WeaponSCAR17, weapon_scar17 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponSCAR20, WeaponCSBaseGun, DT_WeaponSCAR20, weapon_scar20 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponScout, WeaponCSBaseGun, DT_WeaponScout, weapon_scout );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponSG550, WeaponCSBaseGun, DT_WeaponSG550, weapon_sg550 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponSG556, WeaponCSBaseGun, DT_WeaponSG556, weapon_sg556 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponSSG08, WeaponCSBaseGun, DT_WeaponSSG08, weapon_ssg08 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponTec9, WeaponCSBaseGun, DT_WeaponTec9, weapon_tec9 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponTMP, WeaponCSBaseGun, DT_WeaponTMP, weapon_tmp );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponUMP45, WeaponCSBaseGun, DT_WeaponUMP45, weapon_ump45 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponM249, WeaponCSBaseGun, DT_WeaponM249, weapon_m249 );
LINK_ENTITY_TO_CLASS_SIMPLE_DERIVED( WeaponUSP, WeaponCSBaseGun, DT_WeaponUSP, weapon_usp );

CWeaponCSBaseGun::CWeaponCSBaseGun()
{
	m_pWeaponInfo = NULL;
	m_zoomLevel = 0;
	m_inPrecache = false;
#ifdef CLIENT_DLL
	m_iSilencerBodygroup = SILENCER_BODYGROUP_UNSET;
#endif
}

void CWeaponCSBaseGun::Spawn( )
{
	BaseClass::Spawn();

	m_bBurstMode = false;
	m_iBurstShotsRemaining = 0;
	m_fNextBurstShot = 0.0f;	
	ResetPostponeFireReadyTime();
}


void CWeaponCSBaseGun::Precache()
{
	m_inPrecache = true;
	BaseClass::Precache();
	m_inPrecache = false;
}


const char * CWeaponCSBaseGun::GetWorldModel( void ) const
{
	return BaseClass::GetWorldModel();
}

Activity CWeaponCSBaseGun::GetDeployActivity( void )
{
	if( IsSilenced() )
	{
		return ACT_VM_DRAW_SILENCED;
	}
	else
	{
		return BaseClass::GetDeployActivity();
	}
}


void CWeaponCSBaseGun::Drop( const Vector &vecVelocity )
{
	// re-deploying the weapon is punishment enough for canceling a silencer attach/detach before completion
	if ( (GetActivity() == ACT_VM_ATTACH_SILENCER && m_bSilencerOn == false) ||
		 (GetActivity() == ACT_VM_DETACH_SILENCER && m_bSilencerOn == true ) )
	{
		m_flDoneSwitchingSilencer = gpGlobals->curtime;
		m_flNextSecondaryAttack = gpGlobals->curtime;
		m_flNextPrimaryAttack = gpGlobals->curtime;
	}

	//make sure the world-model silencer bodygroup is correct, we might have hidden/unhidden it prematurely to make the 3rd-person animation look correct
	else if ( (GetActivity() == ACT_VM_ATTACH_SILENCER) || (GetActivity() == ACT_VM_DETACH_SILENCER) )
	{
		int iBodyGroup = FindBodygroupByName( "silencer" );
		if ( iBodyGroup != -1 )
			SetBodygroup( iBodyGroup, m_bSilencerOn ? 0 : 1 );
	}

	BaseClass::Drop( vecVelocity );
}

void CWeaponCSBaseGun::ItemBusyFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	// if we're scoped during a reload, pull us out of the scope for the duration (and set resumezoom so we'll re-zoom when reloading is done)
	if ( HasZoom() && (IsZoomed() || pPlayer->m_bIsScoped) && m_bInReload )
	{
		//m_zoomLevel = 0; //don't affect zoom level, so it'll restore when reloading is done
		pPlayer->m_bIsScoped = false;
		pPlayer->m_bResumeZoom = true;
		pPlayer->SetFOV( pPlayer, GetZoomFOV( 0 ), GetZoomTime( 0 ) );
		m_weaponMode = Primary_Mode;
	}

	BaseClass::ItemBusyFrame();
}

void CWeaponCSBaseGun::ItemPostFrame()
{
	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( !pPlayer )
		return;

	// smoother out the accuracy a bit
	//float flFOV = GetFOVForAccuracy();

	//GOOSEMAN : Return zoom level back to previous zoom level before we fired a shot. This is used only for the AWP.
	// And Scout.
	if ( (m_flNextPrimaryAttack <= gpGlobals->curtime) && (pPlayer->m_bResumeZoom == TRUE) 
		&& m_zoomLevel > 0 ) // only need to re-zoom the zoom when there's a zoom to re-zoom to. who knew?
	{
		if ( m_iClip1 != 0 || ( GetWeaponFlags() & ITEM_FLAG_NOAUTORELOAD ) )
		{
			m_weaponMode = Secondary_Mode;
			// the zoom amount is taking care of below
			pPlayer->SetFOV( pPlayer, GetZoomFOV( m_zoomLevel ), 0.1f );
			m_fScopeZoomEndTime = gpGlobals->curtime + 0.1;
			pPlayer->m_bIsScoped = true;
#ifdef CLIENT_DLL
			/*
			ScreenFade_t		fade;
			fade.duration = ( unsigned short )( ( float )( 1 << SCREENFADE_FRACBITS ) * 0.175 );
			fade.holdTime = ( unsigned short )( ( float )( 1 << SCREENFADE_FRACBITS ) * 0 );

			fade.fadeFlags = 0;
			fade.fadeFlags |= FFADE_IN;

			fade.r = 0;
			fade.g = 0;
			fade.b = 0;
			fade.a = 255;

			clientdll->View_Fade( &fade );
			*/
#endif
		}

		pPlayer->m_bResumeZoom = false;
	}

	/*
	// do this for sniper rifles only and only when the initial zoom has finished zooming
	if ( GetCSWpnData().m_iZoomLevels >= 2 && (m_fScopeZoomEndTime <= gpGlobals->curtime) && m_weaponMode == Secondary_Mode )
	{
		// if we're zoomed in
		if ( IsZoomed() )
		{
			// this is the zoom we are suppoed to be at if we're standing still

			float flFOVDiff = MAX( 0, flFOV - pPlayer->GetFOV() );

			flFOV = ceil( flFOV );
			if ( flFOV < 0 )
				flFOV *= -1;

			if ( flFOVDiff >= 1.0f )
			{
				flFOVDiff = MIN( flFOVDiff, 10.0f )/50;
				if ( flFOVDiff < 0.05f )
					flFOVDiff = 0;

				pPlayer->SetFOV( pPlayer, flFOV, flFOVDiff );
			}
		}
		else
		{
			m_fAccuracySmoothedForZoom = 0;
		}
	}
	*/

	if ( WeaponHasBurst() )
	{
		if ( m_iBurstShotsRemaining > 0 && gpGlobals->curtime >= m_fNextBurstShot )
		{
			BurstFireRemaining();
		}
	}


	BaseClass::ItemPostFrame();
}

bool CWeaponCSBaseGun::SendWeaponAnim( int iActivity )
{
#ifndef CLIENT_DLL
	// firing or reloading should interrupt weapon inspection
	if ( iActivity == ACT_VM_PRIMARYATTACK || iActivity == ACT_VM_RELOAD || iActivity == ACT_SECONDARY_VM_RELOAD || iActivity == ACT_VM_ATTACH_SILENCER || iActivity == ACT_VM_DETACH_SILENCER )
	{
		if ( CCSPlayer *pPlayer = GetPlayerOwner() )
		{
			pPlayer->StopLookingAtWeapon();
		}
	}
#endif

	return BaseClass::SendWeaponAnim( iActivity );
}

float CWeaponCSBaseGun::GetFOVForAccuracy( void )
{
	const CCSWeaponInfo& weaponInfo = GetCSWpnData();
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return 0;

	float flDefaultAccuracy = weaponInfo.GetInaccuracyStand( GetEconItemView(), m_weaponMode );
// 	if ( pPlayer->GetMoveType() == MOVETYPE_LADDER )
// 	{
// 		flDefaultAccuracy = weaponInfo.GetInaccuracyLadder( m_weaponMode, GetEconItemView() ) + weaponInfo.GetInaccuracyLadder( 0, GetEconItemView() );
// 	}
	if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
 	{
 		flDefaultAccuracy = weaponInfo.GetInaccuracyCrouch( GetEconItemView(), m_weaponMode );
 	}

	m_fAccuracySmoothedForZoom = Approach( GetInaccuracy(), m_fAccuracySmoothedForZoom, gpGlobals->frametime * 10.0f );
	float flTargetFOVForZoom = GetZoomFOV( m_zoomLevel );
	float flFOV = flTargetFOVForZoom;

	// and apply it to the player's fov
	if ( m_fAccuracySmoothedForZoom >= 0 )
	{
		flFOV = flTargetFOVForZoom - ( m_fAccuracySmoothedForZoom - flDefaultAccuracy ) * 10;//MIN( flTargetFOVForZoom * ( 1 + ( m_fAccuracySmoothedForZoom*2 ) ), flTargetFOVForZoom * ( 1 + ( m_fAccuracySmoothedForZoom ) ) );
		//Msg( "flFOV = %f\n", flFOV );
	}

	return flFOV;
}

void CWeaponCSBaseGun::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( CannotShootUnderwater() )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.15f;
		return;
	}

	float flCycleTime = GetCycleTime();

	// change a few things if we're in burst mode
	if ( IsInBurstMode() )
	{
		CALL_ATTRIB_HOOK_FLOAT( flCycleTime, cycletime_when_in_burst_mode );

		m_iBurstShotsRemaining = 2;

		m_fNextBurstShot = gpGlobals->curtime;
		CALL_ATTRIB_HOOK_FLOAT( m_fNextBurstShot, time_between_burst_shots );		
	}

	if ( IsZoomed() )
	{
		CALL_ATTRIB_HOOK_FLOAT( flCycleTime, cycletime_when_zoomed );
	}
																	
	if ( !CSBaseGunFire( flCycleTime, m_weaponMode ) )								// <--	'PEW PEW' HAPPENS HERE
		return;

	if ( IsSilenced() )
		SendWeaponAnim( ACT_VM_PRIMARYATTACK_SILENCED );

	// Does this gun unzoom after a shot, as in a bolt action rifle?
	if ( IsZoomed() && ( DoesUnzoomAfterShot() ) )
	{
		pPlayer->m_bIsScoped = false;
		pPlayer->m_bResumeZoom = true;
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), 0.05f );
		m_weaponMode = Primary_Mode;
	}
}

void CWeaponCSBaseGun::SecondaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer == NULL )
	{
		Assert(pPlayer != NULL);
		return;
	}

	if ( HasZoom() )
	{
		if ( ++m_zoomLevel > GetZoomLevels() )
			m_zoomLevel = 0;

		bool bIsSniperRifle = GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE;

		if ( IsZoomed() )
		{
			m_weaponMode = Secondary_Mode;

			//float flFOV = GetFOVForAccuracy();

			if ( bIsSniperRifle )
				pPlayer->SetFOV( pPlayer, GetZoomFOV( m_zoomLevel ), GetZoomTime( m_zoomLevel) );
			
			m_fAccuracyPenalty += GetCSWpnData().GetInaccuracyAltSwitch();
			m_fAccuracySmoothedForZoom = 0;

			pPlayer->m_bIsScoped = true;

#ifdef IRONSIGHT

			if ( pPlayer->GetActiveCSWeapon() )
			{
				CIronSightController *pIronSightController = pPlayer->GetActiveCSWeapon()->GetIronSightController();
				if (pIronSightController)
				{
					pPlayer->GetActiveCSWeapon()->UpdateIronSightController();
					pPlayer->SetFOV(pPlayer, pIronSightController->GetIronSightIdealFOV(), pIronSightController->GetIronSightPullUpDuration());
					pIronSightController->SetState( IronSight_should_approach_sighted );

					//stop looking at weapon when going into ironsights
#ifndef CLIENT_DLL
					pPlayer->StopLookingAtWeapon();

					//force idle animation
					CBaseViewModel *pViewModel = pPlayer->GetViewModel();
					if (pViewModel)
					{
						int nSequence = pViewModel->LookupSequence("idle");
						if (nSequence != ACTIVITY_NOT_AVAILABLE)
						{
							pViewModel->ForceCycle(0);
							pViewModel->ResetSequence(nSequence);
						}
					}
#endif
				}
			}
			
#endif

		}
		else
		{
			m_weaponMode = Primary_Mode;

			if ( bIsSniperRifle )
			{
				int iFOV = FBitSet( pPlayer->GetFlags(), FL_DUCKING ) ? pPlayer->GetDefaultCrouchedFOV() : pPlayer->GetDefaultFOV();
				pPlayer->SetFOV( pPlayer, iFOV, GetZoomTime( 0 ));
			}

			m_fAccuracySmoothedForZoom = 0;
			pPlayer->m_bIsScoped = false;

#ifdef IRONSIGHT
			if ( pPlayer->GetActiveCSWeapon() )
			{
				CIronSightController *pIronSightController = pPlayer->GetActiveCSWeapon()->GetIronSightController();
				if (pIronSightController)
				{
					pPlayer->GetActiveCSWeapon()->UpdateIronSightController();
					int iFOV = FBitSet(pPlayer->GetFlags(), FL_DUCKING) ? pPlayer->GetDefaultCrouchedFOV() : pPlayer->GetDefaultFOV();
					pPlayer->SetFOV(pPlayer, iFOV, pIronSightController->GetIronSightPutDownDuration());
					pIronSightController->SetState(IronSight_should_approach_unsighted);
					SendWeaponAnim(ACT_VM_FIDGET);
				}
			}
#endif

		}

#ifdef CLIENT_DLL
		/*
		if ( GetPlayerOwner() && ( bIsSniperRifle && IsZoomed() && m_zoomLevel == 1 ) )
		{
			ScreenFade_t		fade;
			fade.duration = ( unsigned short )( ( float )( 1 << SCREENFADE_FRACBITS ) * 0.22 );
			fade.holdTime = ( unsigned short )( ( float )( 1 << SCREENFADE_FRACBITS ) * 0 );

			fade.fadeFlags = 0;
			fade.fadeFlags |= FFADE_IN;

			fade.r = 0;
			fade.g = 0;
			fade.b = 0;
			fade.a = 255;

			clientdll->View_Fade( &fade );
		}
		*/
#endif

#ifndef CLIENT_DLL

		
		// If this isn't guarded, the sound will be emitted twice, once by the server and once by the client.
		// Let the server play it since if only the client plays it, it's liable to get played twice cause of
		// a prediction error. joy.

		// [tj] Playing this from the player so that we don't try to play the sound outside the level.
		if ( GetPlayerOwner() )
		{
			if ( IsZoomed() )
			{
				const char *pszZoomSound = GetZoomInSound();
				if ( pszZoomSound && pszZoomSound[0] )
				{
					GetPlayerOwner()->EmitSound( pszZoomSound );
				}

				//if ( !bIsSniperRifle )
				//{
				//	color32 clr = {0, 0, 0, 200};
				//	float flZoomTime = weaponInfo.m_fZoomTime[m_zoomLevel];
				//	float flBlackTime = MAX( flZoomTime/15, 0.02 );
				//	UTIL_ScreenFade( pPlayer, clr, flBlackTime, (flZoomTime - (flZoomTime/5)) - flBlackTime, FFADE_IN );
				//}
			}
			else
			{
				const char *pszZoomSound = GetZoomOutSound();
				if ( pszZoomSound && pszZoomSound[0] )
				{
					GetPlayerOwner()->EmitSound( pszZoomSound );
				}

				//if ( !bIsSniperRifle )
				//{
				//	color32 clr = {0, 0, 0, 175};
				//	float flZoomTime = weaponInfo.m_fZoomTime[0];
				//	float flBlackTime = MAX( flZoomTime/15, 0.02 );
				//	UTIL_ScreenFade( pPlayer, clr, flBlackTime, flZoomTime - flBlackTime, FFADE_OUT );
				//}
			}

			if ( bIsSniperRifle )
			{
				// let the bots hear the sniper rifle zoom
				IGameEvent * event = gameeventmanager->CreateEvent( "weapon_zoom" );
				if ( event )
				{
					event->SetInt( "userid", pPlayer->GetUserID() );
					gameeventmanager->FireEvent( event );
				}
			}
			else
			{
				// exists for the game instructor to let it know when the player zoomed in with a regular rifle
				// different from the above weapon_zoom because we don't use this event to notify bots
				IGameEvent * event = gameeventmanager->CreateEvent( "weapon_zoom_rifle" );
				if ( event )
				{
					event->SetInt( "userid", pPlayer->GetUserID() );
					gameeventmanager->FireEvent( event );
				}
			}
		}

#endif
		m_fScopeZoomEndTime = gpGlobals->curtime + GetZoomTime( m_zoomLevel );
	}
#ifndef CLIENT_DLL
	else if ( WeaponHasBurst() )
	{
		if ( IsInBurstMode() )
		{
			pPlayer->HintMessage( "#Cstrike_TitlesTXT_Switch_To_FullAuto", false );
			m_bBurstMode = false;
			m_weaponMode = Primary_Mode;
		}
		else
		{
			pPlayer->HintMessage( "#Cstrike_TitlesTXT_Switch_To_BurstFire", false );
			m_bBurstMode = true;
			m_weaponMode = Secondary_Mode;
		}

		pPlayer->EmitSound( "Weapon.AutoSemiAutoSwitch" );
	}
#endif
#if DETACHABLE_SILENCER
	else if ( HasSilencer() && m_flDoneSwitchingSilencer <= gpGlobals->curtime )
	{
		if ( m_bSilencerOn )
		{
			SendWeaponAnim( ACT_VM_DETACH_SILENCER );

#ifndef CLIENT_DLL
			SendActivityEvents( ACT_VM_DETACH_SILENCER );
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_SILENCER_DETACH );

			IGameEvent * event = gameeventmanager->CreateEvent( "silencer_detach" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				gameeventmanager->FireEvent( event );
			}

#endif
		}
		else
		{
			SendWeaponAnim( ACT_VM_ATTACH_SILENCER );

#ifndef CLIENT_DLL
			SendActivityEvents( ACT_VM_ATTACH_SILENCER );
			pPlayer->DoAnimationEvent( PLAYERANIMEVENT_SILENCER_ATTACH );
#endif
		}

		float nextAttackTime = gpGlobals->curtime + SequenceDuration();

		m_flDoneSwitchingSilencer = nextAttackTime;
		m_flNextSecondaryAttack = nextAttackTime;
		m_flNextPrimaryAttack = nextAttackTime;
		SetWeaponIdleTime( nextAttackTime );

	}
#endif
	else if ( IsRevolver() && m_flNextSecondaryAttack < gpGlobals->curtime )
	{
		float flCycletimeAlt = GetCycleTime( Secondary_Mode );
		m_weaponMode = Secondary_Mode;
		UpdateAccuracyPenalty();
#ifndef CLIENT_DLL
		// Logic for weapon_fire event mimics weapon_csbase.cpp CWeaponCSBase::ItemPostFrame() primary fire implementation
		IGameEvent * event = gameeventmanager->CreateEvent( ( HasAmmo() ) ? "weapon_fire" : "weapon_fire_on_empty" );
		if ( event )
		{
			const char *weaponName = GetDefinitionName();
			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetString( "weapon", weaponName );
			event->SetBool( "silenced", IsSilenced() );
			gameeventmanager->FireEvent( event );
		}
#endif
		CSBaseGunFire( flCycletimeAlt, Secondary_Mode );								// <--	'PEW PEW' HAPPENS HERE
		m_flNextSecondaryAttack = gpGlobals->curtime + flCycletimeAlt;
		return;
	}

	else
	{
		BaseClass::SecondaryAttack();
	}

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3f;
}

void CWeaponCSBaseGun::BurstFireRemaining()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer || m_iClip1 <= 0 )
	{
		m_iClip1 = 0;
		m_iBurstShotsRemaining = 0;
		m_fNextBurstShot = 0.0f;
		return;
	}

	uint16 nItemDefIndex = 0;

	FX_FireBullets(
		pPlayer->entindex(),
		nItemDefIndex,
		pPlayer->Weapon_ShootPosition(),
		pPlayer->GetFinalAimAngle(),
		GetCSWeaponID(),
		Secondary_Mode,
		CBaseEntity::GetPredictionRandomSeed( SERVER_PLATTIME_RNG ) & 255,
		GetInaccuracy(),
		GetSpread(),
		GetAccuracyFishtail(),
		m_fNextBurstShot,
		(HasSilencer() && IsSilenced()) ? SPECIAL1 : SINGLE,
		m_flRecoilIndex );

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->DoMuzzleFlash();
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	--m_iBurstShotsRemaining;

	if ( m_iBurstShotsRemaining > 0 )
	{
		CALL_ATTRIB_HOOK_FLOAT( m_fNextBurstShot, time_between_burst_shots );
	}
	else
	{
		m_fNextBurstShot = 0.0f;
	}

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	// update accuracy
	m_fAccuracyPenalty += weaponInfo.GetInaccuracyFire( GetEconItemView(), m_weaponMode );

	// table driven recoil
	Recoil( Secondary_Mode );

	++pPlayer->m_iShotsFired;
	m_flRecoilIndex += 1.0f;
	--m_iClip1;
}

bool CWeaponCSBaseGun::CSBaseGunFire( float flCycleTime, CSWeaponMode weaponMode )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	if ( m_iClip1 == 0 )
	{
		if ( m_bFireOnEmpty )
		{
			PlayEmptySound();

			m_iNumEmptyAttacks++;

			// NOTE[pmf]: we don't want to actually play the dry fire animations, as most seem to depict the weapon actually firing.
			// SendWeaponAnim( ACT_VM_DRYFIRE );

			//++pPlayer->m_iShotsFired;	// don't play "auto" empty clicks -- make the player release the trigger before clicking again
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;

			if ( IsRevolver() )
			{
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + GetCycleTime( weaponMode );
				BaseClass::SendWeaponAnim( ACT_VM_DRYFIRE ); // empty!
			}

		}

		return false;
	}

	float flCurAttack = CalculateNextAttackTime( flCycleTime );

	if ( (GetWeaponType() != WEAPONTYPE_SNIPER_RIFLE && IsZoomed()) || (IsRevolver() && weaponMode == Secondary_Mode) )
	{
		SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	}
	else if ( IsRevolver() )
	{
		BaseClass::SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}
	else
	{
		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}		

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	uint16 nItemDefIndex = 0;

	FX_FireBullets(
		pPlayer->entindex(),
		nItemDefIndex,
		pPlayer->Weapon_ShootPosition(),
		pPlayer->GetFinalAimAngle(),
		GetCSWeaponID(),
		weaponMode,
		CBaseEntity::GetPredictionRandomSeed( SERVER_PLATTIME_RNG ) & 255,
		GetInaccuracy(),
		GetSpread(), 
		GetAccuracyFishtail(),
		flCurAttack,
		(HasSilencer() && IsSilenced()) ? SPECIAL1 : SINGLE,
		m_flRecoilIndex );

	DoFireEffects();

#ifdef IRONSIGHT
#ifdef CLIENT_DLL
	if ( GetIronSightController() )
	{
		GetIronSightController()->IncreaseDotBlur( RandomFloat( 0.22f, 0.28f ) );
	}
#endif
#endif

	SetWeaponIdleTime( gpGlobals->curtime + weaponInfo.GetTimeToIdleAfterFire( GetEconItemView() ) );

	// update accuracy
	m_fAccuracyPenalty += weaponInfo.GetInaccuracyFire( GetEconItemView(), weaponMode );

	// table driven recoil
	Recoil( weaponMode );

	++pPlayer->m_iShotsFired;
	m_flRecoilIndex += 1.0f;
	--m_iClip1;

	return true;
}

bool CWeaponCSBaseGun::IsFullAuto() const
{ 
	if ( BaseClass::IsFullAuto() )
	{
		return !IsInBurstMode(); 
	}
	else
	{
		return false;
	}
}

void CWeaponCSBaseGun::DoFireEffects()
{
	if ( IsSilenced() )
		return;

	CCSPlayer *pPlayer = GetPlayerOwner();

	if ( pPlayer )
		pPlayer->DoMuzzleFlash();
}

bool CWeaponCSBaseGun::Reload()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 )
		return false;

	int iResult = 0;

		iResult = DefaultReload( GetMaxClip1(), GetMaxClip2(), m_iReloadActivityIndex );

	if ( !iResult )
		return false;

	pPlayer->SetAnimation( PLAYER_RELOAD );

	if ( HasZoom() )
	{
		m_zoomLevel = 0;
		m_weaponMode = Primary_Mode;
	}

	if ( pPlayer->GetFOV() != pPlayer->GetDefaultFOV() && pPlayer->m_bIsScoped)
	{
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), 0.0f );
		pPlayer->m_bIsScoped = false;
	}

	pPlayer->m_iShotsFired = 0;
	m_flRecoilIndex += 1.0f;

	return BaseClass::Reload();
}

void CWeaponCSBaseGun::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	// only idle if the slid isn't back
	if ( m_iClip1 != 0 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + GetCSWpnData().GetIdleInterval( GetEconItemView() ) );

		//silencers are bodygroups, so there is no longer a silencer-specific idle.
		SendWeaponAnim( ACT_VM_IDLE );
	}
}

bool CWeaponCSBaseGun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	// re-deploying the weapon is punishment enough for canceling a silencer attach/detach before completion
	if ( (GetActivity() == ACT_VM_ATTACH_SILENCER && m_bSilencerOn == false) ||
		 (GetActivity() == ACT_VM_DETACH_SILENCER && m_bSilencerOn == true ) )
	{
		m_flDoneSwitchingSilencer = gpGlobals->curtime;
		m_flNextSecondaryAttack = gpGlobals->curtime;
		m_flNextPrimaryAttack = gpGlobals->curtime;
	}

	if ( HasZoom() )
	{
		m_zoomLevel = 0;
		m_weaponMode = Primary_Mode;
	}

	// not sure we want to fully support animation cancelling
	if ( m_bInReload && !m_bReloadVisuallyComplete )
	{
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime;
	}
	return BaseClass::Holster(pSwitchingTo);
}

bool CWeaponCSBaseGun::Deploy()
{
	// don't allow weapon switching to shortcut cycle time (quickswitch exploit)
	float fOldNextPrimaryAttack	= m_flNextPrimaryAttack;
	float fOldNextSecondaryAttack = m_flNextSecondaryAttack;

	m_flDoneSwitchingSilencer = 0.0f;
	m_iBurstShotsRemaining = 0;
	m_fNextBurstShot = 0.0f;

	if ( !BaseClass::Deploy() )
		return false;

	if ( HasZoom() )
	{
		m_zoomLevel = 0;
		m_weaponMode = Primary_Mode;
	}

	if ( IsRevolver() )
	{
		m_weaponMode = Secondary_Mode;
	}

	m_flNextPrimaryAttack	= Max( m_flNextPrimaryAttack.Get(), fOldNextPrimaryAttack );
	m_flNextSecondaryAttack	= Max( m_flNextSecondaryAttack.Get(), fOldNextSecondaryAttack );
	return true;
}

bool CWeaponCSBaseGun::HasZoom()
{
	return GetZoomLevels() != 0;
}


#ifdef CLIENT_DLL


const char* CWeaponCSBaseGun::GetMuzzleFlashEffectName_1stPerson( void )
{
	if ( HasSilencer() && IsSilenced() )
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_1stPersonAlt( GetEconItemView() );
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_1stPerson( GetEconItemView() );
	}
}

const char* CWeaponCSBaseGun::GetMuzzleFlashEffectName_3rdPerson( void )
{
	if ( HasSilencer() && IsSilenced() )
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_3rdPersonAlt( GetEconItemView() );
	}
	else
	{
		return GetCSWpnData().GetMuzzleFlashEffectName_3rdPerson( GetEconItemView() );
	}
}
#endif


CCSWeaponInfo const	& CWeaponCSBaseGun::GetCSWpnData() const
{
	if ( m_pWeaponInfo != NULL )
		return *m_pWeaponInfo;
	else
		return BaseClass::GetCSWpnData();
}

bool CWeaponCSBaseGun::IsInBurstMode() const
{
	return m_bBurstMode;
}

bool CWeaponCSBaseGun::IsZoomed( void ) const
{
	return ( m_zoomLevel > 0 );
}

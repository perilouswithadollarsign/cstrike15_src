//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#include "cbase.h"
#include "predicted_viewmodel.h"

#ifdef CLIENT_DLL
#include "prediction.h"
#ifdef CSTRIKE15
#include "c_cs_player.h"
#endif
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( PredictedViewModel, DT_PredictedViewModel )
LINK_ENTITY_TO_CLASS_ALIASED( predicted_viewmodel, PredictedViewModel );

BEGIN_NETWORK_TABLE( CPredictedViewModel, DT_PredictedViewModel )
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
CPredictedViewModel::CPredictedViewModel() : m_LagAnglesHistory("CPredictedViewModel::m_LagAnglesHistory")
{
	m_vLagAngles.Init();
	m_LagAnglesHistory.Setup( &m_vLagAngles, INTERPOLATE_LINEAR_ONLY );
	m_vPredictedOffset.Init();
	m_flInaccuracyTilt = 0;
	m_flOldAccuracyDiffSmoothed = 0;
}
#else
CPredictedViewModel::CPredictedViewModel()
{
}
#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPredictedViewModel::~CPredictedViewModel()
{
}

#ifdef CLIENT_DLL
ConVar cl_wpn_sway_interp( "cl_wpn_sway_interp", "0.1", FCVAR_CLIENTDLL );
ConVar cl_wpn_sway_scale( "cl_wpn_sway_scale", "1.6", FCVAR_CLIENTDLL|FCVAR_CHEAT );

#ifdef CSTRIKE15
extern ConVar	cl_use_new_headbob;
#endif //CSTRIKE15

#endif //CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:  Adds head bob for off hand models
//-----------------------------------------------------------------------------
void CPredictedViewModel::AddViewModelBob( CBasePlayer *owner, Vector& eyePosition, QAngle& eyeAngles )
{
#ifdef CSTRIKE15
#ifdef CLIENT_DLL
	if ( cl_use_new_headbob.GetBool() == false )
		return;

	// if we are an off hand view model (index 1) and we have a model, add head bob.
	// (Head bob for main hand model added by the weapon itself.)
	if ( ViewModelIndex() == 1 && m_bShouldIgnoreOffsetAndAccuracy )
	{
		CalcViewModelBobHelper( owner, &m_BobState, 1 );
		AddViewModelBobHelper( eyePosition, eyeAngles, &m_BobState );
	}
#endif
#endif
}

void CPredictedViewModel::CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& /*original_angles*/ )
{
#ifdef CLIENT_DLL
	float interp = cl_wpn_sway_interp.GetFloat();
	if ( !interp || m_bShouldIgnoreOffsetAndAccuracy )
		return;

	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
	{
		origin += m_vPredictedOffset;
		return;
	}

	// Calculate our drift
	Vector	forward, right, up;
	AngleVectors( angles, &forward, &right, &up );
	
	// Add an entry to the history.
	m_vLagAngles = angles;
	m_LagAnglesHistory.NoteChanged( gpGlobals->curtime, gpGlobals->curtime, interp, false );
	
	// Interpolate back 100ms.
	m_LagAnglesHistory.Interpolate( gpGlobals->curtime, interp );
	
	// Now take the 100ms angle difference and figure out how far the forward vector moved in local space.
	Vector vLaggedForward;
	QAngle angleDiff = m_vLagAngles - angles;
	AngleVectors( -angleDiff, &vLaggedForward, 0, 0 );
	Vector vForwardDiff = Vector(1,0,0) - vLaggedForward;

	if ( ShouldFlipModel() )
		right = -right;

	// Now offset the origin using that.
	vForwardDiff *= cl_wpn_sway_scale.GetFloat();
	m_vPredictedOffset = forward*vForwardDiff.x + right*-vForwardDiff.y + up*vForwardDiff.z;

	// reduce offset as viewmodel angle approaches nearly vertical
	float flMult = clamp( abs(DotProduct(up, Vector(0,0,1))) - 0.02f, 0, 1 );

	origin += (m_vPredictedOffset * flMult);
#endif
}

#ifdef CLIENT_DLL
ConVar cl_gunlowerangle( "cl_gunlowerangle", "2", FCVAR_CLIENTDLL );
ConVar cl_gunlowerspeed( "cl_gunlowerspeed", "0.1", FCVAR_CLIENTDLL );
#endif //CLIENT_DLL

void CPredictedViewModel::ApplyViewModelPitchAndDip( CBasePlayer *owner, Vector& vecNewOrigin, QAngle& vecNewAngles )
{
	//orients and moves weapon to provide visual feedback on weapon accuracy and player motion (like jumping and landing)
#ifdef CLIENT_DLL
	// Check for lowering the weapon
	// C_CSPlayer *pPlayer = ToCSPlayer( owner );
	// Assert( pPlayer );

	bool bJumping = !( owner->GetFlags() & FL_ONGROUND );

	bool bLowered = bJumping;//pPlayer->IsWeaponLowered();

	QAngle vecLoweredAngles(0,0,0);

	m_vLoweredWeaponOffset.x = Approach( bLowered ? cl_gunlowerangle.GetFloat() : 0, m_vLoweredWeaponOffset.x, cl_gunlowerspeed.GetFloat() );
	vecLoweredAngles.x += m_vLoweredWeaponOffset.x;
	vecNewAngles -= vecLoweredAngles * 0.2f;
	vecNewOrigin.z -= vecLoweredAngles.x * 0.4f; // translation offset looks more natural than rotation


	float flDipAddAmt = 0.0f;
	float flOldFallVel = owner->m_Local.m_flOldFallVelocity;
	// This does the dip when you land
	// m_Local.m_bInLanding gets set on the client in baseplayer_shared.cpp  -- mtw
	if ( owner && owner->m_Local.m_bInLanding == true )
	{
		float flDipAmt = 0.005;//weapon_land_dip_amt.GetFloat();

		float landseconds = MAX(gpGlobals->curtime - (owner->m_Local.m_flLandingTime - 0.1f), 0.0f);
		float landFraction = SmoothCurve( landseconds / 0.25f );
		clamp( landFraction, 0.0f, 1.0f );

		float flDipAmount = (1 / flOldFallVel) * flDipAmt;

		int dipHighOffset = 64;
		int dipLowOffset = dipHighOffset - flDipAmt;

		Vector temp = owner->GetViewOffset();
		temp.z = ( ( dipLowOffset - flDipAmount ) * landFraction ) +
			( dipHighOffset * ( 1 - landFraction ) );

		if ( temp.z > dipHighOffset )
		{
			temp.z = dipHighOffset;
		}

		flDipAddAmt = ( dipHighOffset - temp.z );
	}

//	Replaced with CL #2056767 code in baseviewmodel_shared.cpp
//	that applies aimpunch to viewmodel angle
//////////////////////////////////////////
// 	C_WeaponCSBase *pWeapon = ( C_WeaponCSBase * )GetOwningWeapon();
// 	// pitch the viewmodel up to match where the bullets are going
// 	if ( pWeapon )
// 	{
// 		float flMaxMultiplier = 40;
// 		float flMultiplier = flMaxMultiplier;
// 
// 		if ( !m_bShouldIgnoreOffsetAndAccuracy )
// 			flMultiplier -= MAX( 0, (gpGlobals->curtime - pWeapon->m_flNextPrimaryAttack) * flMaxMultiplier );
// 
// 		float flMultMinClamp = 0;
// 		if ( bJumping )
// 			flMultMinClamp = 5.0f;
// 
// 		flMultiplier = clamp( flMultiplier, flMultMinClamp, flMaxMultiplier );
// 
// 		// get the player's default accuracy
// 		float flBaseAccuracy = pWeapon->GetCSWpnData().GetInaccuracyStand( 0, pWeapon->GetAttributeContainer()->GetItem() );
// 		// get the current accuracy of the player
// 		float flAccuracy = m_bShouldIgnoreOffsetAndAccuracy ? 0 : pWeapon->GetInaccuracy();
// 		//float flAccuracyFishtail = m_bShouldIgnoreOffsetAndAccuracy ? 0 : pWeapon->GetAccuracyFishtail();
// 		// get the difference between the two
// 		float flAccuracyDiff = m_bShouldIgnoreOffsetAndAccuracy ? 0 : abs((flAccuracy - flBaseAccuracy) * flMultiplier);
// 
// 		float flApproachSpeed = 25.0f;
// 		// if we are in the air, we just jumped, we are running really fast,
// 		// adjust the appraoch speed based on how fast it changed
// 		// this avoids big visual pops 
// 		if ( bJumping || (!m_bShouldIgnoreOffsetAndAccuracy && owner->GetAbsVelocity().Length2D() > 150.0f) )// && flAccuracyDiff > m_flOldAccuracyDiff )
// 		{
// 			float flMax = 7.5f;
// 			float flDiv = 10.0f;
// 			// if we're on the ground and moving fast
// 			if ( owner->GetAbsVelocity().Length2D() > 150.0f && !bJumping )
// 			{
// 				flMax = 25.0f;
// 				flDiv = 5.0f;
// 			}
// 
// 			float flOldToNewDiff = abs( ((flAccuracyDiff - m_flOldAccuracyDiffSmoothed) / flDiv) * flApproachSpeed );
// 			// small differences, we go faster, large differences and we go slower
// 			flApproachSpeed = clamp( flApproachSpeed - flOldToNewDiff, 5.0f, flMax );
// 		}
// 		
// 		// save off the accuracy difference
// 		m_flOldAccuracyDiffSmoothed = Approach( flAccuracyDiff, m_flOldAccuracyDiffSmoothed, abs( ((flAccuracyDiff)-m_flOldAccuracyDiffSmoothed )*gpGlobals->frametime) * 0.5f );
// 
// 		// smooth out the tilting
// 		m_flInaccuracyTilt = Approach( flAccuracyDiff, m_flInaccuracyTilt, abs( (flAccuracyDiff-m_flInaccuracyTilt)*gpGlobals->frametime) * flApproachSpeed );
// 		m_flInaccuracyTilt = MIN( m_flInaccuracyTilt, 3.0f );
// 		//Msg ( "owner->GetAbsVelocity().Length2D() = %f, flAccuracyDiff = %f,  m_flInaccuracyTilt = %f, flMultiplier = %f\n", owner->GetAbsVelocity().Length2D(), flAccuracyDiff, m_flInaccuracyTilt, flMultiplier );
// 		//Msg ( "flApproachSpeed = %f, flAccuracyDiff = %f,  m_flOldAccuracyDiffSmoothed = %f\n", flApproachSpeed, flAccuracyDiff, m_flOldAccuracyDiffSmoothed );
// 
// 		vecNewAngles.x -= m_flInaccuracyTilt;
// 		//vecNewAngles.y += flAccuracyFishtail;
// 		// dip the gun just a bit as well
// 		vecNewOrigin.z -= (m_flInaccuracyTilt * 0.5f) + flDipAddAmt;
// 	}
#endif
}

#ifdef CSTRIKE15

void CPredictedViewModel::CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles )
{
#if defined( CLIENT_DLL )
	if ( cl_use_new_headbob.GetBool() == false )
		return BaseClass::CalcViewModelView( owner, eyePosition, eyeAngles );

	Vector vecNewOrigin = eyePosition;
	QAngle vecNewAngles = eyeAngles;

	ApplyViewModelPitchAndDip( owner, vecNewOrigin, vecNewAngles );

	BaseClass::CalcViewModelView( owner, vecNewOrigin, vecNewAngles );
#endif //CLIENT_DLL

#ifdef IRONSIGHT
	CWeaponCSBase *pWeapon = ( CWeaponCSBase * )GetOwningWeapon();
	if ( pWeapon )
	{
		CIronSightController* pIronSightController = pWeapon->GetIronSightController();
		if ( pIronSightController )
		{
#ifndef CLIENT_DLL
			pIronSightController->UpdateIronSightAmount();
#else
			Vector vecEyeOrigin = eyePosition;
			QAngle angEyeAngles = eyeAngles;

			if ( !prediction->InPrediction() )
			{
				//Retrieve ironsight position and angles, then lerp the weapon there if we're at all ironsighted
				pIronSightController->ApplyIronSightPositioning( vecEyeOrigin, angEyeAngles, GetLocalOrigin(), GetLocalAngles() );
				if ( pIronSightController->IsInIronSight() )
				{
					SetLocalOrigin( Lerp( pIronSightController->GetIronSightAmountGained(), GetLocalOrigin(), vecEyeOrigin ) );
					SetLocalAngles( Lerp( pIronSightController->GetIronSightAmountGained(), GetLocalAngles(), angEyeAngles ) );
				}
			}
#endif

		}
	}
#endif //IRONSIGHT


#ifdef CLIENT_DLL
	// bias the position of the viewmodel during observer camera interpolation
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && pLocalPlayer->GetObserverInterpState() != C_BasePlayer::OBSERVER_INTERP_NONE )
	{
		C_CSPlayer *pCSPlayer = ToCSPlayer( pLocalPlayer );
		if ( pCSPlayer )
		{
			Vector vecOffset = pCSPlayer->GetObserverInterpolatedOffsetVector();

			// pull up from offscreen a little
			vecOffset.z -= vecOffset.Length() * 0.2f;
			
			// dampen overall
			vecOffset *= 0.5f;

			SetLocalOrigin( GetLocalOrigin() + vecOffset );
		}
	}
#endif

}

#endif //CSTRIKE15

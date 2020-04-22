//--------------------------------------------------------------------------------------------------------
// Copyright (c) 2007 Turtle Rock Studios, Inc.

#include "cbase.h"
#include "c_basetoggle.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_BaseToggle, DT_BaseToggle, CBaseToggle )
	RecvPropVector( RECVINFO( m_vecFinalDest ) ),
	RecvPropInt( RECVINFO( m_movementType ) ),
	RecvPropFloat( RECVINFO( m_flMoveTargetTime ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_BaseToggle )
END_PREDICTION_DATA()


ConVar cl_predict_basetoggles("cl_predict_basetoggles", "1" );

//--------------------------------------------------------------------------------------------------------
// Returns the velocity imparted to players standing on us.
void C_BaseToggle::GetGroundVelocityToApply( Vector &vecGroundVel )
{
	vecGroundVel = GetLocalVelocity();
	if( !GetPredictable() )
	{
		vecGroundVel.z = 0.0f; // don't give upward velocity, or it could predict players into the air.
	}
}

bool C_BaseToggle::ShouldPredict( void )
{
	return cl_predict_basetoggles.GetBool() && (m_movementType == MOVE_TOGGLE_LINEAR);
}

C_BasePlayer *C_BaseToggle::GetPredictionOwner( void )
{
	C_BasePlayer *pPlayer = (C_BasePlayer *)m_hPredictionOwner.Get();

	if( pPlayer && pPlayer->IsLocalPlayer() )
		return pPlayer;

	return NULL;
}

bool C_BaseToggle::PredictionIsPhysicallySimulated( void )
{
	return (m_movementType == MOVE_TOGGLE_LINEAR);
}

void C_BaseToggle::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );
	
	m_vLastNetworked = GetNetworkOrigin();
	m_fLastNetworkedTime = gpGlobals->curtime;

	if( GetPredictionEligible() && m_bWasPredictingMotion && (m_movementType == MOVE_TOGGLE_NONE) )
	{
		SetPredictionEligible( false );
		ResetLatched();
	}
	m_bWasPredictingMotion = (m_movementType != MOVE_TOGGLE_NONE);
}

void C_BaseToggle::PhysicsSimulate( void )
{
	BaseClass::PhysicsSimulate();

	if( GetPredictable() )
	{
		Vector vOrigin = PredictPosition( GetPredictionOwner()->PredictedServerTime() );

		if( vOrigin == m_vecFinalDest )
		{
			SetLocalVelocity( vec3_origin );
		}
		SetLocalOrigin( vOrigin );
		SetNetworkOrigin( vOrigin );
	}	
}

Vector C_BaseToggle::PredictPosition( float fCurTime )
{
	if( (m_movementType != MOVE_TOGGLE_LINEAR) || m_flMoveTargetTime <= 0.0f )
		return GetNetworkOrigin();

	if( m_flMoveTargetTime > fCurTime )
	{
		float fTargetTime = m_flMoveTargetTime;
		float fTimeRemaining = fTargetTime - fCurTime;
		float fTotalTime = fTargetTime - m_fLastNetworkedTime;

		if( fTotalTime <= 0.0f )
			return m_vecFinalDest;

		float fInterp = fTimeRemaining / fTotalTime;

		return (m_vLastNetworked * fInterp) + (m_vecFinalDest * (1.0f - fInterp));
	}
	else
	{
		return m_vecFinalDest;
	}
}


//--------------------------------------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT( C_BaseButton, DT_BaseButton, CBaseButton )
	RecvPropBool( RECVINFO( m_usable ) ),
END_RECV_TABLE()


//--------------------------------------------------------------------------------------------------------
bool C_BaseButton::IsPotentiallyUsable( void )
{
	return true;
}

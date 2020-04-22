//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_BREAKABLEPROP_H
#define C_BREAKABLEPROP_H
#ifdef _WIN32
#pragma once
#endif

#include "player_pickup.h" 

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_BreakableProp : public C_BaseAnimating, public CDefaultPlayerPickupVPhysics
{
public:
	DECLARE_CLASS( C_BreakableProp, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

	C_BreakableProp();

	virtual bool IsProp( void ) const
	{
		return true;
	};

	//IPlayerPickupVPhysics
	virtual bool HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle PreferredCarryAngles( void );

	virtual bool	ShouldPredict( void );
	virtual C_BasePlayer *GetPredictionOwner( void );
	virtual bool PredictionErrorShouldResetLatchedForAllPredictables( void ) { return false; }

	// Copy fade from another breakable prop
	void CopyFadeFrom( C_BreakableProp *pSource );
	virtual void OnDataChanged( DataUpdateType_t type );

	const QAngle &GetNetworkedPreferredPlayerCarryAngles( void ) { return m_qPreferredPlayerCarryAngles; }

protected:
	QAngle m_qPreferredPlayerCarryAngles;

private:
	bool m_bClientPhysics;
};

#endif // C_BREAKABLEPROP_H

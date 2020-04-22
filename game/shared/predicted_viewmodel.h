//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PREDICTED_VIEWMODEL_H
#define PREDICTED_VIEWMODEL_H
#ifdef _WIN32
#pragma once
#endif

#include "predictable_entity.h"
#include "utlvector.h"
#include "baseplayer_shared.h"
#include "shared_classnames.h"
#if defined( CSTRIKE15 )
#include "weapon_csbase.h"
	#ifdef CLIENT_DLL
	#include "c_cs_player.h"
	#endif
#endif

#if defined( CLIENT_DLL )
#define CPredictedViewModel C_PredictedViewModel
#endif

class CPredictedViewModel : public CBaseViewModel
{
	DECLARE_CLASS( CPredictedViewModel, CBaseViewModel );
public:

	DECLARE_NETWORKCLASS();

	CPredictedViewModel( void );
	virtual ~CPredictedViewModel( void );
							
	virtual void CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles );
	virtual void AddViewModelBob( CBasePlayer *owner, Vector& eyePosition, QAngle& eyeAngles );
	virtual void ApplyViewModelPitchAndDip( CBasePlayer *owner, Vector& origin, QAngle& angles );

#if defined( CSTRIKE15 )
	virtual void CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles );

#if defined( CLIENT_DLL )
	BobState_t	&GetBobState() { return m_BobState; }
#endif //CLIENT_DLL

#endif //CSTRIKE15


#if defined( CLIENT_DLL )
	virtual bool ShouldPredict( void )
	{
		if ( C_BasePlayer::IsLocalPlayer( GetOwner() ) )
			return true;

		return BaseClass::ShouldPredict();
	}
#endif

private:
	
#if defined( CLIENT_DLL )

	// This is used to lag the angles.
	CInterpolatedVar<QAngle> m_LagAnglesHistory;
	QAngle m_vLagAngles;
	Vector	m_vPredictedOffset;

	CPredictedViewModel( const CPredictedViewModel & ); // not defined, not accessible

#if defined( CSTRIKE15 )
protected:
	BobState_t		m_BobState;		// view model head bob state
	QAngle m_vLoweredWeaponOffset;
	float m_flInaccuracyTilt;
	float m_flOldAccuracyDiffSmoothed;
#endif //CSTRIKE15

#endif
};

#endif // PREDICTED_VIEWMODEL_H

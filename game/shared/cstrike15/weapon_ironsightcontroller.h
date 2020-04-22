//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Class to control 'aim-down-sights' aka "IronSight" weapon functionality
//
//=====================================================================================//


#ifndef WEAPON_IRONSIGHTCONTROLLER_H
#define WEAPON_IRONSIGHTCONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

#include "cs_shareddefs.h"

#ifdef IRONSIGHT


#ifdef CLIENT_DLL
	#define IRONSIGHT_ANGLE_AVERAGE_SIZE 8
	#define IRONSIGHT_ANGLE_AVERAGE_DIVIDE ( 1.0f / IRONSIGHT_ANGLE_AVERAGE_SIZE )
	#define IRONSIGHT_HIDE_CROSSHAIR_THRESHOLD 0.5f
#endif

enum CSIronSightMode
{
	IronSight_should_approach_unsighted = 0,
	IronSight_should_approach_sighted,
	IronSight_viewmodel_is_deploying,
	IronSight_weapon_is_dropped
};

// class to keep track of and update iron sight values and state information
class CIronSightController
{

public:

	CIronSightController();

	bool				Init( CWeaponCSBase *pWeaponToMonitor );
	inline bool			IsInitializedAndAvailable( void )		{ return m_bIronSightAvailable; }

	void				UpdateIronSightAmount( void );

	float				GetIronSightFOV( float flDefaultFOV, bool bUseBiasedValue = false );

	inline float		GetIronSightAmount( void )				{ return m_flIronSightAmount; }
	inline float		GetIronSightAmountGained( void )		{ return m_flIronSightAmountGained; }
	inline float		GetIronSightAmountBiased( void )		{ return m_flIronSightAmountBiased; }

	inline float		GetIronSightIdealFOV( void )			{ return m_flIronSightFOV; }
	inline float		GetIronSightPullUpDuration( void )		{ return (m_flIronSightPullUpSpeed > 0) ? (1.0f / m_flIronSightPullUpSpeed) : 0.0f; } // pull up duration is how long the pull up would take in seconds, not the speed
	inline float		GetIronSightPutDownDuration( void )		{ return (m_flIronSightPutDownSpeed > 0) ? (1.0f / m_flIronSightPutDownSpeed) : 0.0f; } // put down duration is how long the put down would take in seconds, not the speed

	inline bool			IsApproachingSighted( void ); //still true when weapon is 100% ironsighted
	inline bool			IsApproachingUnSighted( void ); //still true when weapon is 0% ironsighted
	inline bool			IsDeploying( void ); //is still playing deploy animation
	inline bool			IsDropped( void ); //is dropped loose in the world with no player owner

	bool				IsInIronSight( void ); //true if the weapon is ironsighted any positive amount

	void				SetState( CSIronSightMode newState );

#ifdef CLIENT_DLL
	void				ApplyIronSightPositioning( Vector &vecCleanEyePosition, QAngle &angCleanEyeAngle, const Vector &vecBobbedEyePosition, const QAngle &angBobbedEyeAngle );

	void				IncreaseDotBlur(float flAmount);
	float				GetDotBlur(void);
	float				GetDotWidth(void);
	Vector2D			GetDotCoords(void);
	const char			*GetDotMaterial(void);

	bool				ShouldHideCrossHair( void );

	bool				PrepareScopeEffect( int x, int y, int w, int h, CViewSetup *pViewSetup );
	void				RenderScopeEffect( int x, int y, int w, int h, CViewSetup *pViewSetup );

#endif


private:

	bool		m_bIronSightAvailable;				//only true if members are initialized, valid, and ready to use

	QAngle		m_angPivotAngle;
	Vector		m_vecEyePos;
	float		m_flIronSightAmount;				//0.0 (not ironsighted at all) to 1.0 (fully looking down the sights)
	float		m_flIronSightAmountGained;
	float		m_flIronSightAmountBiased;
	float		m_flIronSightPullUpSpeed;			//speed to pull the weapon up to the eye
	float		m_flIronSightPutDownSpeed;			//speed to put the weapon back down into normal viewmodel position
	float		m_flIronSightPivotForward;
	float		m_flIronSightFOV;
	float		m_flIronSightLooseness;				//how rigidly the viewmodel reacts to changes in m_vecViewDeltaVelocity
	
	CWeaponCSBase	*m_pAttachedWeapon;

#ifdef CLIENT_DLL
	void		AddToAngleAverage( QAngle newAngle );
	QAngle		GetAngleAverage( void );
	QAngle		m_angDeltaAverage[IRONSIGHT_ANGLE_AVERAGE_SIZE];
	QAngle		QAngleDiff( QAngle &angTarget, QAngle &angSrc );
	QAngle		m_angViewLast;

	Vector2D	m_vecDotCoords;
	float		m_flDotBlur;
	float		m_flSpeedRatio;
#endif

};



#endif //IRONSIGHT

#endif // WEAPON_IRONSIGHTCONTROLLER_H
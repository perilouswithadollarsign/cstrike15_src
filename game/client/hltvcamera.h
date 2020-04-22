//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HLTVCAMERA_H
#define HLTVCAMERA_H
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"

class C_HLTVCamera : CGameEventListener
{
public:
	C_HLTVCamera();
	virtual ~C_HLTVCamera();

	enum AutodirectorState_t
	{
		AUTODIRECTOR_ON = 0,
		AUTODIRECTOR_OFF,
		AUTODIRECTOR_PAUSED
	};

	void Init();
	void Reset();
	void Update();

	void CalcView(CViewSetup *pSetup);
	void FireGameEvent( IGameEvent *event );

	void SetMode(int iMode);
	void SetChaseCamParams( float flOffset, float flDistance, float flTheta, float flPhi  );
	void SpecNextPlayer( bool bInverse );
	void SpecNamedPlayer( const char *szPlayerName );
	void SpecPlayerByIndex( int iIndex );
	void SpecPlayerByAccountID( const char *pszSteamID );
	void ToggleChaseAsFirstPerson();
	bool IsPVSLocked();
	bool HasCameraMan() { return ( m_iCameraMan != 0 ); }
	
	void SetAutoDirector( AutodirectorState_t eState );
	AutodirectorState_t AutoDirectorState() const;
	bool IsAutoDirectorOn() const;
	
	int  GetMode();	// returns current camera mode
	C_BaseEntity *GetPrimaryTarget();  // return primary target
	void SetPrimaryTarget( int nEntity); // set the primary obs target
	C_BasePlayer *GetCameraMan();  // return camera entity if any
	Vector GetCameraPosition() { return m_vCamOrigin; }
	int GetCurrentOrLastTarget() { return (m_iTarget1 != 0) ? m_iTarget1 : m_iLastTarget1; }
	int GetCurrentTargetEntindex() { return m_iTarget1; }

	void SetWatchingGrenade( C_BaseEntity *pGrenade, bool bWatching );
	bool IsWatchingGrenade( void ) { return m_bIsFollowingGrenade; }
	void CreateMove(CUserCmd *cmd);
	void FixupMovmentParents();
	void PostEntityPacketReceived();
	const char* GetTitleText() { return m_szTitleText; }
	int  GetNumSpectators() { return m_nNumSpectators; }
	float	GetIdealOverviewScale( void ) { return m_flIdealOverviewScale; }
	void SpecCameraGotoPos( Vector vecPos, QAngle angAngle, int nPlayerIndex = 0 );
	void SpecCameraLerptoPos( const Vector &origin, const QAngle &angles, int nPlayerIndex = 0, float flTime = 0.0f );

protected:

	void CalcChaseCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void CalcFixedView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void CalcInEyeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	void CalcRoamingView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov);
	void CalcChaseOverview( CViewSetup &pSetup );
	Vector CalcIdealOverviewPosition( Vector vecStartPos, Vector vOldOverviewPos );

	//void CalcOverview( CViewSetup *pSetup );

	void SmoothCameraAngle( QAngle& targetAngle );
	void SetCameraAngle( QAngle& targetAngle );
	void Accelerate( Vector& wishdir, float wishspeed, float accel );

	int			m_nCameraMode; // current camera mode
	int			m_iCameraMan; // camera man entindex or 0
	Vector		m_vCamOrigin;  //current camera origin
	Vector		m_vLastGrenadeVelocity;  //current camera origin
	float		m_flLastGrenadeVelocityUpdate;
	QAngle		m_aCamAngle;   //current camera angle
	int			m_iTarget1;	// first tracked target or 0
	int			m_iLastTarget1; // the last target before we switched
	int			m_iTarget2; // second tracked target or 0
	float		m_flFOV; // current FOV
	float		m_flOffset;  // z-offset from target origin
	float		m_flDistance; // distance to traget origin+offset
	float		m_flLastDistance; // too smooth distance
	float		m_flTheta; // view angle horizontal 
	float		m_flPhi; // view angle vertical
	float		m_flInertia; // camera inertia 0..100
	float		m_flLastAngleUpdateTime;
	bool		m_bEntityPacketReceived;	// true after a new packet was received
	int			m_nNumSpectators;
	char		m_szTitleText[64];
	CUserCmd	m_LastCmd;
	Vector		m_vecVelocity;
	float       m_flAutodirectorPausedTime; // negative if autodirector is not paused
	float		m_flIdealOverviewScale;
	Vector		m_vOldOverviewPos;
	Vector		m_vIdealOverviewPos;
	float		m_flLastCamZPos;
	float		m_flNextIdealOverviewPosUpdate;
	bool		m_bIsFollowingGrenade;
	bool					m_bIsSpecLerping;
	float					m_flSpecLerpTime;
	float					m_flSpecLerpEndTime;
	Vector					m_vecSpecLerpIdealPos;
	QAngle					m_angSpecLerpIdealAng;
	Vector					m_vecSpecLerpOldPos;
	QAngle					m_angSpecLerpOldAng;
	float			m_flObserverChaseApproach;
	Vector			m_vecObserverEyeDirPrevious;
};


extern C_HLTVCamera *HLTVCamera();	// get Singleton



#endif // HLTVCAMERA_H

//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef C_PLANTEDC4_H
#define C_PLANTEDC4_H

#include "cbase.h"
#include "in_buttons.h"
#include "decals.h"

#include "c_cs_player.h"
#include "c_rope.h"
#include "utlvector.h"


// ------------------------------------------------------------------------------------------ //
// CPlantedC4 class.
// For now to show the planted c4 on the radar - client proxy to remove the CBaseAnimating 
// network vars?
// ------------------------------------------------------------------------------------------ //

class C_PlantedC4 : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_PlantedC4, CBaseAnimating );
	DECLARE_CLIENTCLASS();

	C_PlantedC4();
	virtual ~C_PlantedC4();

	void Explode( void );
	void Spawn( void );
	virtual void SetDormant( bool bDormant );

	void ClientThink( void );

	int GetSecondsRemaining( void ) { return ceil( m_flC4Blow - gpGlobals->curtime ); }

	inline bool IsBombActive( void ) { return m_bBombTicking; }
	CNetworkVar( bool, m_bBombTicking );

	float m_flNextGlow;
	float m_flNextBeep;

	float m_flC4Blow;
	float m_flTimerLength;
	float m_bTenSecWarning;
	float m_bTriggerWarning;
	float m_bExplodeWarning;

	CNetworkVar( float, m_flDefuseLength );	
	CNetworkVar( float, m_flDefuseCountDown ); 
	CNetworkVar( bool, m_bBombDefused ); 
	CNetworkVar( CHandle<CCSPlayer>, m_hBombDefuser );

	CGlowObject m_GlowObject;

	CUtlVector<CHandle<C_RopeKeyframe>> m_hDefuserRopes;
	CHandle<C_BaseAnimating> m_hDefuserMultimeter;

	float GetDefuseProgress( void )
	{	
		float flProgress = 1.0f;

		if( m_flDefuseLength > 0.0 )
		{
			flProgress = ( ( m_flDefuseCountDown - gpGlobals->curtime ) / m_flDefuseLength );
		}

		return flProgress;
	}

	// returns progress towards detonation in percent (detonation at 0%)
	float GetDetonationProgress( void );

	float	m_flNextRadarFlashTime;	// next time to change flash state
	bool	m_bRadarFlash;			// is the flash on or off

	virtual void UpdateOnRemove( void ) OVERRIDE;

private:
	bool CreateDefuserRopes( void );
	void DestroyDefuserRopes( void );
	CHandle<CCSPlayer> m_hLocalDefusingPlayerHandle;

};

extern CUtlVector< C_PlantedC4* > g_PlantedC4s;

#endif //C_PLANTEDC4_H
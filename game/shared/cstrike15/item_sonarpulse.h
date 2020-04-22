//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ITEM_SONARPULSE_H
#define ITEM_SONARPULSE_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "utlvector.h"


#if defined( CLIENT_DLL )
	#include "c_cs_player.h"
	#include "c_baseanimating.h"
	#include "GameEventListener.h"
	#define CSonarPulse C_SonarPulse
#else
	#include "cs_player.h"
	#include "baseanimating.h"
#endif


#ifdef CLIENT_DLL

#define SONARPULSE_ICON_LIFETIME 10.0f

struct sonarpulseicon_t
{
	Vector		m_vecPos;
	float		m_flTimeCreated;

	sonarpulseicon_t( Vector vecPos )
	{
		m_vecPos = vecPos;
		m_flTimeCreated = gpGlobals->curtime;
	}

	float GetLifeRemaining( void )
	{
		return SONARPULSE_ICON_LIFETIME - (gpGlobals->curtime - m_flTimeCreated);
	}
};
#endif


class CSonarPulse : public CBaseAnimating
#ifdef CLIENT_DLL
	, public CGameEventListener
#endif
{
	public:
	DECLARE_CLASS( CSonarPulse, CBaseAnimating );

	public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	CSonarPulse();
	virtual ~CSonarPulse();

	virtual void Spawn();

	virtual void Precache();

	float GetPulseRadius( void );
	Vector GetPulseOrigin( void );
	
	

#if defined( CLIENT_DLL )

	void FireGameEvent( IGameEvent *event );

	void RenderIcons( void );

	IMaterial *m_pIconMaterial; 

	void ClientThink( void );

#else

	// need to always transmit since we want to see the pulse edge even when the center is way out of pvs
	virtual int  UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }
	virtual int  ShouldTransmit( const CCheckTransmitInfo *pInfo ){ return FL_EDICT_ALWAYS; }

	virtual int	ObjectCaps() { return BaseClass::ObjectCaps() | (FCAP_ONOFF_USE); }

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void PulseStart( void );
	void PulseThink( void );
	void PulseReset( void );

	bool IsOkToPulse( CCSPlayer* pPlayer );

	CUtlVector<CCSPlayer*>m_vecPlayersOutsidePulse;
	CUtlVector<CCSPlayer*>m_vecPingedPlayers;

#endif

	CNetworkVar( bool, m_bPulseInProgress );
	CNetworkVar( float, m_flPulseInitTime );
	CNetworkVar( float, m_flPlaybackRate );

};


#endif // ITEM_CRATEBEACON_H

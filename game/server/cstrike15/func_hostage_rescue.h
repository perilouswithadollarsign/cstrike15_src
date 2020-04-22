//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#ifndef HOSTAGERESCUEZONE_H
#define HOSTAGERESCUEZONE_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "triggers.h"
#include "cvisibilitymonitor.h"
#include "cs_shareddefs.h"
#include "cs_player_resource.h"

class CHostageRescueZone : public CBaseTrigger
{
public:
	DECLARE_CLASS( CHostageRescueZone, CBaseTrigger );
	DECLARE_DATADESC();

	void CHostageRescue();
	void Spawn();
	virtual void ReInitOnRoundStart( void );

	void HostageRescueTouch( CBaseEntity* pOther );

	EHANDLE		m_hInstructorHint;		// Hint that's used by the instructor system
};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the hostage rescue zone
//-----------------------------------------------------------------------------
class CInfoInstructorHintHostageRescueZone : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintHostageRescueZone, CPointEntity );

	void Spawn( void );
	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	DECLARE_DATADESC();
};

#endif // HOSTAGERESCUEZONE_H

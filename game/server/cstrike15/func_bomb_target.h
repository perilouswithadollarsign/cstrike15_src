//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Bomb Target Area ent
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "triggers.h"
#include "cvisibilitymonitor.h"
//#include "cs_player_resource.h"

class CBombTarget : public CBaseTrigger
{
public:
	DECLARE_CLASS( CBombTarget, CBaseTrigger );
	DECLARE_DATADESC();

	CBombTarget();

	void Spawn();
	virtual void ReInitOnRoundStart( void );
	void EXPORT BombTargetTouch( CBaseEntity* pOther );
	void EXPORT BombTargetUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void OnBombExplode( inputdata_t &inputdata );
	void OnBombPlanted( inputdata_t &inputdata );
	void OnBombDefused( inputdata_t &inputdata );

	bool	IsHeistBombTarget( void ) { return m_bIsHeistBombTarget; }
	const char *GetBombMountTarget( void ){ return STRING( m_szMountTarget ); }

private:
	COutputEvent m_OnBombExplode;	//Fired when the bomb explodes
	COutputEvent m_OnBombPlanted;	//Fired when the bomb is planted
	COutputEvent m_OnBombDefused;	//Fired when the bomb is defused

	bool		m_bIsHeistBombTarget;
	bool		m_bBombPlantedHere;
	string_t	m_szMountTarget;
	EHANDLE		m_hInstructorHint;		// Hint that's used by the instructor system
};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
class CInfoInstructorHintBombTargetA : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintBombTargetA, CPointEntity );

	void Spawn( void );
	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	DECLARE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
class CInfoInstructorHintBombTargetB : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintBombTargetB, CPointEntity );

	void Spawn( void );
	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	DECLARE_DATADESC();
};
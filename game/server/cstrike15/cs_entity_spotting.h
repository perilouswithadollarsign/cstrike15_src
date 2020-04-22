//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A game system for tracking and updating entity spot state
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_ENTITY_SPOTTING_H
#define CS_ENTITY_SPOTTING_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"

class CCSPlayer;

class CCSEntitySpotting : public CAutoGameSystemPerFrame
{
public:
	enum SpottingRules_T
	{
		SPOT_RULE_ENEMY					= ( 1<<0 ),  // spotted when seen by an enemy
		SPOT_RULE_CT					= ( 1<<1 ),  // spotted when seen by CT
		SPOT_RULE_T						= ( 1<<2 ),  // spotted when seen by T
		SPOT_RULE_ALWAYS_SEEN_BY_FRIEND	= ( 1<<3 ),  // always visible to friend
		SPOT_RULE_ALWAYS_SEEN_BY_CT		= ( 1<<4 ),  // always visible to CT
		SPOT_RULE_ALWAYS_SEEN_BY_T		= ( 1<<5 ),  // always visible to T
	};

	CCSEntitySpotting( const char * szName );
	virtual ~CCSEntitySpotting( void ) {}
	
	virtual bool Init( void );
	virtual char const *Name() { return "CCSEntitySpotting"; }
	virtual void FrameUpdatePostEntityThink( void );

	void UpdateSpottedEntities( void );
	

protected:
	float m_fLastUpdate;
};

extern CCSEntitySpotting * g_EntitySpotting;



//===========================================================
// - GatherNonPVSSpottedEntitiesFunctor -
//
// Given a player, generate a list of spotted entities that
// exist outside of that player's PVS.
// Query GetSpotted for result
//===========================================================
class GatherNonPVSSpottedEntitiesFunctor
{
public:
	GatherNonPVSSpottedEntitiesFunctor( CCSPlayer * pPlayer );


	bool operator()( CBaseEntity * pEntity );

	const CBitVec<MAX_EDICTS> & GetSpotted( void )
	{
		return m_EntitySpotted;
	}

private:
	CCSPlayer          *m_pPlayer;
	int					m_nSourceTeam;
	CBitVec<MAX_EDICTS> m_EntitySpotted;
	byte				m_pSourcePVS[MAX_MAP_LEAFS/8];
	bool				m_bForceSpot;
};

#endif // ENTITY_RESOURCE_H

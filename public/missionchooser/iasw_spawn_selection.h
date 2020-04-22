//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// 
//
//===============================================================================
#ifndef IASW_SPAWN_SELECTION_H
#define IASW_SPAWN_SELECTION_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/strtools.h"
#include "mathlib/vector.h"

class IASWSpawnDefinitionEntry;

// NOTE: Text version of these enum types is in asw_spawn_selection.cpp!
enum
{
	ASW_NPC_SPAWN_TYPE_INVALID = -1,
	ASW_NPC_SPAWN_TYPE_ANY = 0,
	ASW_NPC_SPAWN_TYPE_FIXED = 1,
	ASW_NPC_SPAWN_TYPE_WANDERER,
	ASW_NPC_SPAWN_TYPE_SWARM,
	ASW_NPC_SPAWN_TYPE_BOSS,
	ASW_NPC_SPAWN_TYPE_PROP,
	ASW_NPC_SPAWN_TYPE_ARENAWAVE,
	ASW_NPC_SPAWN_TYPE_CONSOLE,
	ASW_NPC_SPAWN_TYPE_SPAWNER,
	ASW_NPC_SPAWN_TYPE_BIFURCATE,
	ASW_NPC_SPAWN_TYPE_SPECIAL1,
	ASW_NPC_SPAWN_TYPE_SPECIAL2,

	// Has to be last.
	ASW_NPC_SPAWN_TYPE_COUNT		
};

class IASWSpawnDefinitionEntry
{
public:

	virtual const char *GetNPCClassname() = 0;
	virtual void GetSpawnCountRange( int &nMin, int &nMax ) = 0;
	virtual float GetEliteNPCChance( void ) = 0;
	virtual bool UseSpawners() = 0;
};


class IASWSpawnDefinition
{
public:

	virtual int GetEntryCount() = 0;
	virtual IASWSpawnDefinitionEntry *GetEntry( int nEntry ) = 0;
};


class IASWSpawnSelection
{
public:

	virtual IASWSpawnDefinition *GetSpawnDefinition( int nSpawnType ) = 0;
	virtual bool IsAvailableNPC( const char *szName ) = 0;

	virtual void SetCurrentSpawnSet( int iMissionDifficulty ) = 0;
	virtual bool SetCurrentSpawnSet( const char *szSetName ) = 0;

	virtual void DumpCurrentSpawnSet() = 0;
};

// fixed spawn encounter in the mission
class IASW_Encounter
{
public:
	virtual const Vector&				GetEncounterPosition() = 0;
	virtual int							GetNumSpawnDefs() = 0;
	virtual IASWSpawnDefinition*		GetSpawnDef( int i ) = 0;
	virtual float						GetEncounterRadius() = 0;
};

#endif // IASW_SPAWN_SELECTION_H

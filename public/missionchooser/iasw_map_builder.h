//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Interface to the runtime map (VMF) building class
//
//===============================================================================

#ifndef IASW_MAP_BUILDER_H
#define IASW_MAP_BUILDER_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

class KeyValues;

class IASW_Map_Builder
{
public:
	//-----------------------------------------------------------------------------
	// Ticks the map builder and updates progress.
	//-----------------------------------------------------------------------------
	virtual void Update( float flEngineTime ) = 0;

	// @TODO: clean up these entrypoints to be more consistent.  It's confusing
	// and convoluted right now because of the sheer number of different ways 
	// to build a map.

	//-----------------------------------------------------------------------------
	// Schedules the random generation and BSP build of a map from its description
	// or a pre-defined layout file.
	//-----------------------------------------------------------------------------
	virtual void ScheduleMapGeneration( 
		const char *pMapName, 
		float flTime, 
		KeyValues *pMissionSettings, 
		KeyValues *pMissionDefinition ) = 0;

	//-----------------------------------------------------------------------------
	// Schedules the VMF export and BSP build of a .layout file.
	//-----------------------------------------------------------------------------
	virtual void ScheduleMapBuild(
		const char *pMapName, 
		const float flTime ) = 0;

	//-----------------------------------------------------------------------------
	// Gets a map build progress value in the range [0.0f, 1.0f] inclusive.
	//-----------------------------------------------------------------------------
	virtual float GetProgress() = 0; 

	//-----------------------------------------------------------------------------
	// Gets a string which describes the current status of the map builder.
	//-----------------------------------------------------------------------------
	virtual const char *GetStatusMessage() = 0;

	//-----------------------------------------------------------------------------
	// Gets the name of the map being currently built.
	//-----------------------------------------------------------------------------
	virtual const char *GetMapName() = 0;

	//-----------------------------------------------------------------------------
	// Gets a value indicating whether a map build is scheduled or in progress.
	//-----------------------------------------------------------------------------
	virtual bool IsBuildingMission() = 0;
};


#endif // IASW_MAP_BUILDER_H
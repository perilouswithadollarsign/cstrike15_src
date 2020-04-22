//===== Copyright c 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IMATCHEXT_SWARM_H
#define IMATCHEXT_SWARM_H

#ifdef _WIN32
#pragma once
#endif


//
//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData1 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
struct TitleData1
{
	uint64 unused;
};

//
//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData2 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
struct TitleData2
{
	// Achievement Counters
	uint8 iCountXxx;	// ACHIEVEMENT_XXX
	uint8 iCountYyy;	// ACHIEVEMENT_YYY
	
	// Achievement Component Bitfields
	uint8 iCompXxx;		// ACHIEVEMENT_XXX
};

//
//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData3 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
struct TitleData3
{
	uint64 unused; // unused, free for taking
};


abstract_class IMatchExtSwarm
{
public:
	// Get server map information for the session settings
	virtual KeyValues * GetAllMissions() = 0;
	virtual KeyValues * GetMapInfo( KeyValues *pSettings, KeyValues **ppMissionInfo = NULL ) = 0;
	virtual KeyValues * GetMapInfoByBspName( KeyValues *pSettings, char const *szBspMapName, KeyValues **ppMissionInfo = NULL ) = 0;
};

#define IMATCHEXT_SWARM_INTERFACE "IMATCHEXT_SWARM_INTERFACE_001"

// #define SWARM_DLC_NUMBER_XXX			20

#endif // IMATCHEXT_L4D_H

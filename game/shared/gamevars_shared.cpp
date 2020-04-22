//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "gamevars_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef GAME_DLL
void MPForceCameraCallback( IConVar *var, const char *pOldString, float flOldValue )
{
	if ( mp_forcecamera.GetInt() < OBS_ALLOW_ALL || mp_forcecamera.GetInt() >= OBS_ALLOW_NUM_MODES )
	{
		mp_forcecamera.SetValue( OBS_ALLOW_TEAM );
	}
}
#endif 

// some shared cvars used by game rules
ConVar mp_forcecamera( 
	"mp_forcecamera", 
	"1", 
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Restricts spectator modes for dead players. 0 = Any team. 1 = Only own team. 2 = No one; fade to black on death (previously mp_fadetoblack)."
#ifdef GAME_DLL
	, MPForceCameraCallback 
#endif
	);

ConVar mp_radar_showall(
	"mp_radar_showall",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines who should see all. 0 = default. 1 = both teams. 2 = Terrorists. 3 = Counter-Terrorists.",
	true, 0, //min
	true, 3	 //max
	);
	
ConVar mp_allowspectators(
	"mp_allowspectators", 
	"1.0", 
	FCVAR_REPLICATED,
	"toggles whether the server allows spectator mode or not" );

ConVar mp_friendlyfire(
	"mp_friendlyfire",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"Allows team members to injure other members of their team"
	);

ConVar mp_teammates_are_enemies(
	"mp_teammates_are_enemies",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"When set, your teammates act as enemies and all players are valid targets."
	);

ConVar mp_buy_anywhere(
	"mp_buy_anywhere",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"When set, players can buy anywhere, not only in buyzones. 0 = default. 1 = both teams. 2 = Terrorists. 3 = Counter-Terrorists."
	);

ConVar mp_buy_during_immunity(
	"mp_buy_during_immunity",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"When set, players can buy when immune, ignoring buytime. 0 = default. 1 = both teams. 2 = Terrorists. 3 = Counter-Terrorists."
	);

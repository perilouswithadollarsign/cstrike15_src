//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:	See memorylog.h
//
//=============================================================================//

#include "cbase.h"
#include "memorylog.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#if !defined( _CERT )

// Memory log auto game system instantiation
CMemoryLog g_MemoryLog;

const char *GetMapName( void )
{
	static char mapName[32];
	mapName[0] = 0;
	if ( gpGlobals->mapname.ToCStr() )
		V_strncpy( mapName, gpGlobals->mapname.ToCStr(), sizeof( mapName ) );
	if ( !mapName[ 0 ] )
		V_strncpy( mapName, "none", sizeof( mapName ) );
	return mapName;
}

void CMemoryLog::LevelInitPostEntity( void )
{
//#include "entitylist.h"
#if defined( PORTAL2 )
	const char *mapName = GetMapName();
	if ( V_stristr( mapName, "sp_" ) == mapName )
	{
		// In order to ensure that the map loop never fails, spawn a script entity with the transition script if none exists in the map:
		for ( CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "logic_script" ); pEnt; pEnt = gEntList.FindEntityByClassname( pEnt, "logic_script" ) )
		{
			if ( pEnt && V_stristr( pEnt->GetEntityNameAsCStr(), "transition_script" ) )
				return;
		}
		CBaseEntity *pScriptEntity = (CBaseEntity *)CreateEntityByName( "logic_script" );
		pScriptEntity->SetName( MAKE_STRING( "failsafe_transition_script" ) );
		pScriptEntity->KeyValue( "thinkfunction", "Think" );
		pScriptEntity->KeyValue( "vscripts", "transitions/sp_transition_list.nut" );
		DispatchSpawn( pScriptEntity );
	}
#endif
}

#endif // !defined( _CERT )

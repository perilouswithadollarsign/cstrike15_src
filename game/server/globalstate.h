//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GLOBALSTATE_H
#define GLOBALSTATE_H
#ifdef _WIN32
#pragma once
#endif

typedef enum { GLOBAL_OFF = 0, GLOBAL_ON = 1, GLOBAL_DEAD = 2 } GLOBALESTATE;

void		GlobalEntity_SetState( int globalIndex, GLOBALESTATE state );
void		GlobalEntity_SetMap( int globalIndex, string_t mapname );
int			GlobalEntity_Add( const char *pGlobalname, const char *pMapName, GLOBALESTATE state );

int			GlobalEntity_GetIndex( const char *pGlobalname );
GLOBALESTATE GlobalEntity_GetState( int globalIndex );
const char	*GlobalEntity_GetMap( int globalIndex );
const char	*GlobalEntity_GetName( int globalIndex );

int GlobalEntity_GetCounter( int globalIndex );
void GlobalEntity_SetCounter( int globalIndex, int counter );
int GlobalEntity_AddToCounter( int globalIndex, int delta );

int GlobalEntity_GetFlags( int globalIndex );
void GlobalEntity_SetFlags( int globalIndex, int flags );
void GlobalEntity_AddFlags( int globalIndex, int flags );
void GlobalEntity_RemoveFlags( int globalIndex, int flags );

int			GlobalEntity_GetNumGlobals( void );
void		GlobalEntity_EnableStateUpdates( bool bEnable );

inline int GlobalEntity_Add( string_t globalname, string_t mapName, GLOBALESTATE state )
{
	return GlobalEntity_Add( STRING(globalname), STRING(mapName), state );
}

inline int GlobalEntity_GetIndex( string_t globalname )
{
	return GlobalEntity_GetIndex( STRING(globalname) );
}

inline int GlobalEntity_IsInTable( string_t globalname )
{
	return GlobalEntity_GetIndex( STRING(globalname) ) >= 0 ? true : false;
}

inline int GlobalEntity_IsInTable( const char *pGlobalname )
{
	return GlobalEntity_GetIndex( pGlobalname ) >= 0 ? true : false;
}

inline void GlobalEntity_SetState( string_t globalname, GLOBALESTATE state )
{
	GlobalEntity_SetState( GlobalEntity_GetIndex( globalname ), state );
}

inline void GlobalEntity_SetMap( string_t globalname, string_t mapname )
{
	GlobalEntity_SetMap( GlobalEntity_GetIndex( globalname ), mapname );
}

inline GLOBALESTATE GlobalEntity_GetState( string_t globalname )
{
	return GlobalEntity_GetState( GlobalEntity_GetIndex( globalname ) );
}

inline GLOBALESTATE GlobalEntity_GetState( const char *pGlobalName )
{
	return GlobalEntity_GetState( GlobalEntity_GetIndex( pGlobalName ) );
}

inline int GlobalEntity_GetCounter( string_t globalname )
{
	return GlobalEntity_GetCounter( GlobalEntity_GetIndex( globalname ) );
}

inline int GlobalEntity_GetCounter( const char *pGlobalName )
{
	return GlobalEntity_GetCounter( GlobalEntity_GetIndex( pGlobalName ) );
}

inline void GlobalEntity_SetCounter( string_t globalname, int counter )
{
	GlobalEntity_SetCounter( GlobalEntity_GetIndex( globalname ), counter );
}

inline void GlobalEntity_SetCounter( const char *pGlobalName, int counter )
{
	GlobalEntity_SetCounter( GlobalEntity_GetIndex( pGlobalName ), counter );
}

inline int GlobalEntity_AddToCounter( string_t globalname, int delta )
{
	return GlobalEntity_AddToCounter( GlobalEntity_GetIndex( globalname ), delta );
}

inline int GlobalEntity_AddToCounter( const char *pGlobalName, int delta )
{
	return GlobalEntity_AddToCounter( GlobalEntity_GetIndex( pGlobalName ), delta );
}

inline int GlobalEntity_GetFlags( string_t globalname )
{
	return GlobalEntity_GetFlags( GlobalEntity_GetIndex( globalname ) );
}

inline int GlobalEntity_GetFlags( const char *pGlobalName )
{
	return GlobalEntity_GetFlags( GlobalEntity_GetIndex( pGlobalName ) );
}

inline void GlobalEntity_SetFlags( string_t globalname, int flags )
{
	GlobalEntity_SetFlags( GlobalEntity_GetIndex( globalname ), flags );
}

inline void GlobalEntity_SetFlags( const char *pGlobalName, int flags )
{
	GlobalEntity_SetFlags( GlobalEntity_GetIndex( pGlobalName ), flags );
}

inline void GlobalEntity_AddFlags( string_t globalname, int flags )
{
	GlobalEntity_AddFlags( GlobalEntity_GetIndex( globalname ), flags );
}

inline void GlobalEntity_AddFlags( const char *pGlobalName, int flags )
{
	GlobalEntity_AddFlags( GlobalEntity_GetIndex( pGlobalName ), flags );
}

inline void GlobalEntity_RemoveFlags( string_t globalname, int flags )
{
	GlobalEntity_RemoveFlags( GlobalEntity_GetIndex( globalname ), flags );
}

inline void GlobalEntity_RemoveFlags( const char *pGlobalName, int flags )
{
	GlobalEntity_RemoveFlags( GlobalEntity_GetIndex( pGlobalName ), flags );
}


inline GLOBALESTATE GlobalEntity_GetStateByIndex( int iIndex )
{
	return GlobalEntity_GetState( iIndex );
}

void ResetGlobalState( void );

#endif // GLOBALSTATE_H

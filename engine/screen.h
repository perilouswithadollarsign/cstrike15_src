//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( SCREEN_H )
#define SCREEN_H
#ifdef _WIN32
#pragma once
#endif

void SCR_Init( void );
void SCR_Shutdown( void );

void SCR_UpdateScreen( void );
void SCR_BeginLoadingPlaque( const char *levelName = 0 );
void SCR_EndLoadingPlaque( void );
void SCR_FatalDiskError( void );

extern	bool	scr_disabled_for_loading;
extern  int		scr_nextdrawtick;

#endif // SCREEN_H


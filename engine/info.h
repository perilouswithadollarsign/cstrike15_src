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
#ifndef INFO_H
#define INFO_H
#ifdef _WIN32
#pragma once
#endif

#include "filesystem.h"

#define	MAX_INFO_STRING			256

void			Info_SetValueForStarKey ( char *s, const char *key, const char *value, int maxsize);
// userinfo functions
const char		*Info_ValueForKey ( const char *s, const char *key );
void			Info_RemoveKey ( char *s, const char *key );
void			Info_RemovePrefixedKeys ( char *start, char prefix );
void			Info_SetValueForKey ( char *s, const char *key, const char *value, int maxsize );
void			Info_Print ( const char *s );

void			Info_WriteVars( char *s, FileHandle_t fp );

#endif // INFO_H

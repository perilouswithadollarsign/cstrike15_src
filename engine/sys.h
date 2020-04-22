//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef SYS_H
#define SYS_H
#pragma once

#ifndef SYSEXTERNAL_H
#include "sysexternal.h"
#endif

// sys.h -- non-portable functions

//
// system IO
//
void Sys_mkdir (const char *path, const char *pPathID = 0 );
int Sys_CompareFileTime(long ft1, long ft2);
char const* Sys_FindFirst(const char *path, char *basename, int namelength );
char const* Sys_FindNext(char *basename, int namelength);
void Sys_FindClose (void);

// Takes a path ID filter
char const* Sys_FindFirstEx( const char *pWildcard, const char *pPathID, char *basename, int namelength );

void Sys_ShutdownMemory( void );
void Sys_InitMemory( void );

void Sys_LoadHLTVDLL( void );
void Sys_UnloadHLTVDLL( void );

void Sys_Sleep ( int msec );
void Sys_GetRegKeyValue( char *pszSubKey, char *pszElement, char *pszReturnString, int nReturnLength, char *pszDefaultValue);
void Sys_GetRegKeyValueInt( char *pszSubKey, char *pszElement, long *pulReturnValue, long dwDefaultValue);
void Sys_SetRegKeyValue( char *pszSubKey, char *pszElement,	const char *pszValue );

extern "C" void Sys_SetFPCW (void);
extern "C" void Sys_TruncateFPU( void );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct FileAssociationInfo
{
	char const  *extension;
	char const  *command_to_issue;
};

void Sys_CreateFileAssociations( int count, FileAssociationInfo *list );

// disables the system crash dialogs on windows, stub otherwise
void Sys_NoCrashDialog();
void Sys_TestSendKey( const char *pKey );
void Sys_OutputDebugString(const char *msg);

#endif			// SYS_H

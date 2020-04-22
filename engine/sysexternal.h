//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef SYSEXTERNAL_H
#define SYSEXTERNAL_H
#ifdef _WIN32
#pragma once
#endif

// an error will cause the entire program to exit
void Sys_Error(PRINTF_FORMAT_STRING const char *psz, ...) FMTFUNCTION( 1, 2 );
// an error will cause the entire program to exit but WITHOUT uploading a report to the crash backend, so for fatal but user controlled error
void Sys_Exit( PRINTF_FORMAT_STRING const char *error, ... ) FMTFUNCTION( 1, 2 );
#endif // SYSEXTERNAL_H

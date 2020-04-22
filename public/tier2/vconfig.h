//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Utilities for setting vproject settings
//
//===========================================================================//

#ifndef _VCONFIG_H
#define _VCONFIG_H

#ifdef _WIN32
#pragma once
#endif


// The registry keys that vconfig uses to store the current vproject directory.
#define VPROJECT_REG_KEY	"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"

// For accessing the environment variables we store the current vproject in.
void SetVConfigRegistrySetting( const char *pName, const char *pValue, bool bNotify = true );
bool GetVConfigRegistrySetting( const char *pName, char *pReturn, int size );
#ifdef _WIN32
bool RemoveObsoleteVConfigRegistrySetting( const char *pValueName, char *pOldValue = NULL , int size = 0 ); 
#endif
bool ConvertObsoleteVConfigRegistrySetting( const char *pValueName );


#endif // _VCONFIG_H

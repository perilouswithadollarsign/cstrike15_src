//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== h_export.cpp ========================================================

  Entity classes exported by Halflife.

*/

#if defined(_WIN32)

#include "winlite.h"
#include "datamap.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

HMODULE win32DLLHandle;

// Required DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	// ensure data sizes are stable
	if ( IsPlatformWindowsPC32() && sizeof( inputfunc_t ) != sizeof( int ) ||
		 IsPlatformWindowsPC64() && sizeof( inputfunc_t ) != sizeof( void* ) )
	{
		Assert( !IsPlatformWindowsPC32() || sizeof( inputfunc_t ) == sizeof( int ) );
		Assert( !IsPlatformWindowsPC64() || sizeof( inputfunc_t ) == sizeof( void* ) );
		return FALSE;
	}

	if ( fdwReason == DLL_PROCESS_ATTACH )
    {
		win32DLLHandle = hinstDLL;
    }
	else if ( fdwReason == DLL_PROCESS_DETACH )
    {
    }
	return TRUE;
}

#endif


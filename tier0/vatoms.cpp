//========== Copyright © Valve Corporation, All rights reserved. ========
// NOTE: DO NOT INCLUDE vatoms.h, we don't want to rebuild tier0 every time
// the header changes. 
// 
#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier0/memdbgon.h"

PLATFORM_INTERFACE void** GetVAtom( int nAtomIndex );

static void* g_atoms[16] = {NULL}; // all pointers must be initialized to NULL

void** GetVAtom( int nAtomIndex )
{
	if( uint( nAtomIndex ) >= ARRAYSIZE( g_atoms ) )
	{
		
		ConMsg ( 
			"*******************************************************************\n"
			"                      ***  ERROR  ***                              \n"
			"VATOM index %d out of range, recompile tier0 with larger atom table\n"
			"*******************************************************************\n",
			nAtomIndex );
		return NULL;
	}
	return &g_atoms[nAtomIndex];
}

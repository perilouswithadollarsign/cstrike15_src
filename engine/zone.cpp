//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//						ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks, and there will never be two
// contiguous free memblocks.
//
// The rover can be left pointing at a non-empty block
//
// The zone calls are pretty much only used for small strings and structures,
// all big things are allocated on the hunk.
//===========================================================================//

#include "basetypes.h"
#include "zone.h"
#include "host.h"
#include "tier1/strtools.h"
#include "tier1/utldict.h"
#include "tier0/icommandline.h"
#include "memstack.h"
#include "datacache/idatacache.h"
#include "sys_dll.h"
#include "tier3/tier3.h"
#include "tier0/memalloc.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define KB (1024)
#define MB (1024*1024)

#define MINIMUM_WIN_MEMORY			(48*MB)	// FIXME: copy from sys_dll.cpp, find a common header at some point

// PORTAL 2 SHIPPING CHANGE
// We're sacrificing a little perf (~1% in pathological case) for 12 MB of memory back on X360
// #ifdef _X360
// #define HUNK_USE_16MB_PAGE
// #endif

CMemoryStack g_HunkMemoryStack;
#ifdef HUNK_USE_16MB_PAGE
CMemoryStack g_HunkOverflow;
static bool g_bWarnedOverflow;
#define SIZE_PHYSICAL_HUNK (16*MB)
#endif

const int HUNK_COMMIT_FLOOR = ( IsGameConsole() ? 4/*18*/ : 40 )*MB;

const char *CHunkAllocCredit::s_DbgInfoStack[ DBG_INFO_STACK_DEPTH ];
int			CHunkAllocCredit::s_DbgInfoStackDepth = -1;

#if !defined( _CERT )
ConVar hunk_track_allocation_types( "hunk_track_allocation_types", "1", FCVAR_CHEAT );
#else
ConVar hunk_track_allocation_types( "hunk_track_allocation_types", "0", FCVAR_CHEAT );
#endif
CUtlDict<int, int> g_HunkAllocationsByName;
struct hunkalloc_t { int index, size; };
int HunkAllocSortFunc( const void *a, const void *b )
{
	const hunkalloc_t *A = (const hunkalloc_t *)a, *B = (const hunkalloc_t *)b;
	return ( A->size > B->size ) ? -1 : +1;
}
CON_COMMAND_F( hunk_print_allocations, "", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	Msg( "Hunk allocations:\n");
	hunkalloc_t *items = new hunkalloc_t[ g_HunkAllocationsByName.Count() ];
	int numItems = 0, total = 0;
	for ( int i = g_HunkAllocationsByName.First(); i != g_HunkAllocationsByName.InvalidIndex(); i = g_HunkAllocationsByName.Next( i ) )
	{
		if ( !g_HunkAllocationsByName.Element( i ) )
			continue;
		hunkalloc_t item = { i, g_HunkAllocationsByName.Element( i ) };
		items[numItems++] = item;
		total += item.size;
	}
	qsort( items, numItems, sizeof( hunkalloc_t ), HunkAllocSortFunc );
	Msg( "    %55s:%10d\n", "TOTAL:", total );
	for ( int i = 0; i < numItems; i++ )
	{
		Msg( "    %55s:%10d\n", g_HunkAllocationsByName.GetElementName( items[i].index ), items[i].size );
	}
	delete [] items;
	
#if defined( _X360 )
	xBudgetInfo_t budgetInfo;

	budgetInfo.BSPSize = total;
	XBX_rBudgetInfo( &budgetInfo );
#endif

}



static int GetTargetCacheSize()
{
	int nMemLimit = host_parms.memsize - Hunk_Size();
	if ( nMemLimit < 0x100000 )
	{
		nMemLimit = 0x100000;
	}
	return nMemLimit;
}

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName(int size, const char *name, bool bClear)
{
	if ( hunk_track_allocation_types.GetBool() )
	{
		MEM_ALLOC_CREDIT();
		if ( !name )
		{
			name = "unknown";
		}

		int i = g_HunkAllocationsByName.Find( name );
		if ( i == g_HunkAllocationsByName.InvalidIndex() )
		{
			i = g_HunkAllocationsByName.Insert( name );
			g_HunkAllocationsByName[i] = size;
		}
		else
		{
			g_HunkAllocationsByName[i] += size;
		}
	}
	void *p = g_HunkMemoryStack.Alloc( size, bClear );
#ifdef _GAMECONSOLE
	int overflowAmt = g_HunkMemoryStack.GetCurrentAllocPoint() - HUNK_COMMIT_FLOOR;
	if ( ( overflowAmt > 0 ) && ( overflowAmt <= size ) )
		Warning( "HUNK OVERFLOW! Map BSP data consuming %d bytes more memory than expected...\n", overflowAmt );
#endif
	if ( p )
		return p;
#ifdef HUNK_USE_16MB_PAGE
	if ( !g_bWarnedOverflow )
	{
		g_bWarnedOverflow = true;
		DevMsg( "Note: Hunk base page exhausted\n" );
	}

	p = g_HunkOverflow.Alloc( size, bClear );
	if ( p )
		return p;
#endif
	Error( "Engine hunk overflow!\n" );
	return NULL;
}

/*
===================
Hunk_Alloc
===================
*/

int	Hunk_LowMark(void)
{
	return (int)( g_HunkMemoryStack.GetCurrentAllocPoint() );
}

void Hunk_FreeToLowMark(int mark)
{
	Assert( mark < g_HunkMemoryStack.GetSize() );
#ifdef HUNK_USE_16MB_PAGE
	g_HunkOverflow.FreeAll( false );
	g_bWarnedOverflow = false;
#endif
	g_HunkMemoryStack.FreeToAllocPoint( mark, false );
	g_HunkAllocationsByName.RemoveAll();
}

int Hunk_MallocSize()
{
#ifdef HUNK_USE_16MB_PAGE
	return g_HunkMemoryStack.GetSize() + g_HunkOverflow.GetSize();
#else
	return g_HunkMemoryStack.GetSize();
#endif
}

int Hunk_Size()
{
#ifdef HUNK_USE_16MB_PAGE
	return g_HunkMemoryStack.GetUsed() + g_HunkOverflow.GetUsed();
#else
	return g_HunkMemoryStack.GetUsed();
#endif
}

void Hunk_Print()
{
#ifdef HUNK_USE_16MB_PAGE
	Msg( "Total used memory:      %d (%d/%d)\n", Hunk_Size(), g_HunkMemoryStack.GetUsed(), g_HunkOverflow.GetUsed() );
	Msg( "Total committed memory: %d (%d/%d)\n", Hunk_MallocSize(), g_HunkMemoryStack.GetSize(), g_HunkOverflow.GetSize() );
#else
	Msg( "Total used memory:      %d\n", Hunk_Size() );
	Msg( "Total committed memory: %d\n", Hunk_MallocSize() );
#endif
}



void Hunk_OnMapStart( int nEstimatedBytes )
{
	int nToCommit = MAX( nEstimatedBytes, HUNK_COMMIT_FLOOR );

#ifndef HUNK_USE_16MB_PAGE
	CMemoryStack *pStack = &g_HunkMemoryStack;
#else
	CMemoryStack *pStack = &g_HunkOverflow;
	nToCommit -= SIZE_PHYSICAL_HUNK;
#endif

	if ( developer.GetBool() )
	{
		DevMsg( "Hunk_OnMapStart: %d\n", nToCommit );
	}
	if ( nToCommit > 0 )
	{
		pStack->CommitSize( nToCommit );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Memory_Init( void )
{
	MEM_ALLOC_CREDIT();
#ifdef PLATFORM_64BITS
	// Seems to need to be larger to not get exhausted on
	// 64-bit. Perhaps because of larger pointer sizes.
	int nMaxBytes = 128*MB;
#else
	int nMaxBytes = 64*MB;
#endif
	const int commitIncrement = 64*KB;
#ifndef HUNK_USE_16MB_PAGE
	const int nInitialCommit = MIN( HUNK_COMMIT_FLOOR, nMaxBytes );
	while ( !g_HunkMemoryStack.Init( "g_HunkMemoryStack", nMaxBytes, commitIncrement, nInitialCommit ) )	 
	{
		Warning( "Unable to allocate %d MB of memory, trying %d MB instead\n", nMaxBytes, nMaxBytes/2 );
		nMaxBytes /= 2;
		if ( nMaxBytes < MINIMUM_WIN_MEMORY )
		{
			Error( "Failed to allocate minimum memory requirement for game (%d MB)\n", MINIMUM_WIN_MEMORY/MB);
		}
	}
#else
	if ( !g_HunkMemoryStack.InitPhysical( "g_HunkMemoryStack", SIZE_PHYSICAL_HUNK, 4096 ) || !g_HunkOverflow.Init( "g_HunkOverflow", nMaxBytes - SIZE_PHYSICAL_HUNK, commitIncrement, (SIZE_PHYSICAL_HUNK < HUNK_COMMIT_FLOOR ) ? HUNK_COMMIT_FLOOR - SIZE_PHYSICAL_HUNK : 0 ) )
	{
		Error( "Failed to allocate minimum memory requirement for game (%d MB)\n", nMaxBytes );
	}

#endif
	g_pDataCache->SetSize( GetTargetCacheSize() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Memory_Shutdown( void )
{
	g_HunkMemoryStack.FreeAll();

	// This disconnects the engine data cache
	g_pDataCache->SetSize( 0 );
}

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
#include "quakedef.h"
#include <stddef.h>
#include "vengineserver_impl.h"
#include "server.h"
#include "pr_edict.h"
#include "world.h"
#include "ispatialpartition.h"
#include "utllinkedlist.h"
#include "framesnapshot.h"
#include "tier0/cache_hints.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Edicts won't get reallocated for this many seconds after being freed.
#define EDICT_FREETIME	1.0



static ConVar		sv_useexplicitdelete( "sv_useexplicitdelete", "1", FCVAR_DEVELOPMENTONLY, "Explicitly delete dormant client entities caused by AllowImmediateReuse()." );
static float		g_EdictFreeTime[MAX_EDICTS];
static int			g_nLowestFreeEdict = 0;
void ED_ClearTimes()
{
	V_memset( g_EdictFreeTime, 0, sizeof(g_EdictFreeTime) );
	g_nLowestFreeEdict = 0;
}

/*
=================
ED_ClearEdict

Sets everything to NULL, done when new entity is allocated for game.dll
=================
*/
void ED_ClearEdict (edict_t *e)
{
	e->ClearFree();

	e->ClearStateChanged();
	
	serverGameEnts->FreeContainingEntity(e);
	InitializeEntityDLLFields(e);
	e->m_NetworkSerialNumber = -1;  // must be filled by game.dll
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/


edict_t *ED_Alloc( int iForceEdictIndex )
{
	if ( iForceEdictIndex >= 0 )
	{
		if ( iForceEdictIndex >= sv.num_edicts )
		{
			Warning( "ED_Alloc( %d ) - invalid edict index specified.", iForceEdictIndex );
			return NULL;
		}
		
		edict_t *e = &sv.edicts[iForceEdictIndex];
		if ( e->IsFree() )
		{
			ED_ClearEdict( e );
			return e;
		}
		else
		{
			return NULL;
		}
	}

	// Check the free list first.
	int nFirstIndex = sv.GetMaxClients() + 1;

#if _DEBUG
	for ( int i = nFirstIndex; i < g_nLowestFreeEdict; i++ )
	{
		if ( sv.edicts[i].IsFree() )
		{
			Assert(0);
			DebuggerBreakIfDebugging();
		}
	}
#endif

	nFirstIndex = imax( nFirstIndex, g_nLowestFreeEdict );
	edict_t *pEdict = sv.edicts + nFirstIndex;

	// This misses cache a lot because it has to touch the entire table (32KB in the worst case)
	// We could use a free list here!!!  For now, try to prefetch it and keep an "lowest free" index to help
#if defined(_GAMECONSOLE)
	int nPrefetchCount = sv.num_edicts - nFirstIndex;
	nPrefetchCount = imin( nPrefetchCount, 8 );
	int nLastPrefetch = sv.num_edicts - 8;
	for ( int i = 0; i < nPrefetchCount; i++ )
	{
		PREFETCH_128( ( (byte *)pEdict ) + i * 128, 0 );
	}
#endif
	g_nLowestFreeEdict = sv.num_edicts;
	for ( int i = nFirstIndex; i < sv.num_edicts; i++ )
	{
#if defined(_GAMECONSOLE)
		if ( !(i & 7) && i < nLastPrefetch )
		{
			PREFETCH_128( ( (byte *)pEdict ) + 128, 0 );
		}
#endif
		if ( pEdict->IsFree() )
		{
			g_nLowestFreeEdict = imin( i, g_nLowestFreeEdict );
			if ( (g_EdictFreeTime[i] < 2 || sv.GetTime() - g_EdictFreeTime[i] >= EDICT_FREETIME) )
			{
				// If we have no freetime, we've had AllowImmediateReuse() called. We need
				// to explicitly delete this old entity.
				if ( g_EdictFreeTime[i] == 0 && sv_useexplicitdelete.GetBool() )
				{
					//Warning("ADDING SLOT to snapshot: %d\n", i );
					framesnapshotmanager->AddExplicitDelete( i );
				}
				ED_ClearEdict( pEdict );
				return pEdict;
			}
		}

		pEdict++;
	}
	
	// Allocate a new edict.
	if ( sv.num_edicts >= sv.max_edicts )
	{
		if ( sv.max_edicts != 0 )
		{
			// We don't have any available edicts that are newer than
			// EDICT_FREETIME. Rather than crash try to find an edict that
			// was deleted less than EDICT_FREETIME ago. This will protect us
			// against potential server hacks like those used to crash
			// dota2 servers.
			pEdict = sv.edicts + nFirstIndex;
			for ( int i = nFirstIndex; i < sv.num_edicts; i++ )
			{
				if ( pEdict->IsFree() )
				{
					ED_ClearEdict( pEdict );
					return pEdict;
				}

				pEdict++;
			}
		}

		AssertMsg( 0, "Can't allocate edict" );
		if ( sv.max_edicts == 0 )
			Sys_Error( "ED_Alloc: No edicts yet" );
		Sys_Error ("ED_Alloc: no free edicts");
	}

	// Do this before clearing since clear now needs to call back into the edict to deduce the index so can get the changeinfo data in the parallel structure
	sv.num_edicts++;

	ED_ClearEdict( pEdict );
	
	return pEdict;
}


void ED_AllowImmediateReuse()
{
	edict_t *pEdict = sv.edicts + sv.GetMaxClients() + 1;
	for ( int i=sv.GetMaxClients()+1; i < sv.num_edicts; i++ )
	{
		if ( pEdict->IsFree() )
		{
			g_EdictFreeTime[i] = 0;
		}

		pEdict++;
	}
}


/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED_Free (edict_t *ed)
{
	if ( !sv.edicts )
	{
		// During l4d2 ship cycle we crashed in this code, being called from CleanupDeleteList on the server for a single entity
		// We don't know what was causing the entity to persist after sv.edicts was shut down, so hopefully this guard will let
		// us catch it in the debugger if we ever see it again.
		Warning( "ED_Free(0x%p) called after sv.edicts == NULL\n", ed );
		if ( !IsX360() )
		{
			DebuggerBreak();
		}
		return;
	}

	if (ed->IsFree())
	{
#ifdef _DEBUG
//		ConDMsg("duplicate free on '%s'\n", pr_strings + ed->classname );
#endif
		return;
	}

	// don't free player edicts
	int edictIndex = ed - sv.edicts;
	if ( edictIndex >= 1 && edictIndex <= sv.GetMaxClients() )
		return;

	g_nLowestFreeEdict = imin( g_nLowestFreeEdict, edictIndex );

	// release the DLL entity that's attached to this edict, if any
	serverGameEnts->FreeContainingEntity( ed );

	ed->SetFree();
	g_EdictFreeTime[edictIndex] = sv.GetTime();

	// Increment the serial number so it knows to send explicit deletes the clients.
	ed->m_NetworkSerialNumber++; 
}

//
// 	serverGameEnts->FreeContainingEntity( pEdict )  frees up memory associated with a DLL entity.
// InitializeEntityDLLFields clears out fields to NULL or UNKNOWN.
// Release is for terminating a DLL entity.  Initialize is for initializing one.
//
void InitializeEntityDLLFields( edict_t *pEdict )
{
	// clear all the game variables
	size_t sz = offsetof( edict_t, m_pUnk ) + sizeof( void* );
	memset( ((byte*)pEdict) + sz, 0, sizeof(edict_t) - sz );
	int edictIndex = pEdict - sv.edicts;
	g_EdictFreeTime[edictIndex] = 0;
}

edict_t *EDICT_NUM(int n)
{
	Assert( n >= 0 && n < sv.max_edicts );
	return &sv.edicts[n];
}

int NUM_FOR_EDICT(const edict_t *e)
{
	int b = e - sv.edicts;
	Assert( b >= 0 && b < sv.num_edicts );
	return b;
}

// Special version which allows accessing unused sv.edictchangeinfo slots
int NUM_FOR_EDICTINFO( const edict_t * e )
{
	int b = e - sv.edicts;
	
	Assert( b >= 0 && b < sv.max_edicts );
	return b;
}

IChangeInfoAccessor *CBaseEdict::GetChangeAccessor()
{
	int idx = NUM_FOR_EDICTINFO( (const edict_t * )this );
	return &sv.edictchangeinfo[ idx ];
}

const IChangeInfoAccessor *CBaseEdict::GetChangeAccessor() const
{
	int idx = NUM_FOR_EDICTINFO( (const edict_t * )this );
	return &sv.edictchangeinfo[ idx ];
}

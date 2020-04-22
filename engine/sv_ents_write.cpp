//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "server_pch.h"
#include <eiface.h>
#include <dt_send.h>
#include <utllinkedlist.h>
#include "dt_send_eng.h"
#include "dt.h"
#include "net_synctags.h"
#include "dt_instrumentation_server.h"
#include "LocalNetworkBackdoor.h"
#include "ents_shared.h"
#include "hltvserver.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#endif
#include "framesnapshot.h"
#include "changeframelist.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar g_CV_DTWatchEnt;

//-----------------------------------------------------------------------------
// Delta timing stuff.
//-----------------------------------------------------------------------------

static ConVar		sv_deltatime( "sv_deltatime", "0", 0, "Enable profiling of CalcDelta calls" );
static ConVar		sv_deltaprint( "sv_deltaprint", "0", 0, "Print accumulated CalcDelta profiling data (only if sv_deltatime is on)" );

#if defined( DEBUG_NETWORKING )
ConVar  sv_packettrace( "sv_packettrace", "1", 0, "For debugging, print entity creation/deletion info to console." );
#endif

class CChangeTrack
{
public:
	char		*m_pName;
	int			m_nChanged;
	int			m_nUnchanged;
	
	CCycleCount	m_Count;
	CCycleCount	m_EncodeCount;
};


static CUtlLinkedList<CChangeTrack*, int> g_Tracks;


// These are the main variables used by the SV_CreatePacketEntities function.
// The function is split up into multiple smaller ones and they pass this structure around.
class CEntityWriteInfo : public CEntityInfo
{
public:
	bf_write		*m_pBuf;
	int				m_nClientEntity;

	const PackedEntity	*m_pOldPack;
	const PackedEntity	*m_pNewPack;

	// For each entity handled in the to packet, mark that's it has already been deleted if that's the case
	CBitVec<MAX_EDICTS>	m_DeletionFlags;
	
	CFrameSnapshot	*m_pFromSnapshot; // = m_pFrom->GetSnapshot();
	CFrameSnapshot	*m_pToSnapshot; // = m_pTo->GetSnapshot();

	CFrameSnapshot	*m_pBaseline; // the clients baseline

	CBaseServer		*m_pServer;	// the server who writes this entity

	int				m_nFullProps;	// number of properties send as full update (Enter PVS)
	bool			m_bCullProps;	// filter props by clients in recipient lists
	
	/* Some profiling data
	int				m_nTotalGap;
	int				m_nTotalGapCount; */
};



//-----------------------------------------------------------------------------
// Delta timing helpers.
//-----------------------------------------------------------------------------

CChangeTrack* GetChangeTrack( const char *pName )
{
	FOR_EACH_LL( g_Tracks, i )
	{
		CChangeTrack *pCur = g_Tracks[i];

		if ( stricmp( pCur->m_pName, pName ) == 0 )
			return pCur;
	}

	CChangeTrack *pCur = new CChangeTrack;
	int len = strlen(pName)+1;
	pCur->m_pName = new char[len];
	Q_strncpy( pCur->m_pName, pName, len );
	pCur->m_nChanged = pCur->m_nUnchanged = 0;
	
	g_Tracks.AddToTail( pCur );
	
	return pCur;
}


void PrintChangeTracks()
{
	ConMsg( "\n\n" );
	ConMsg( "------------------------------------------------------------------------\n" );
	ConMsg( "CalcDelta MS / %% time / Encode MS / # Changed / # Unchanged / Class Name\n" );
	ConMsg( "------------------------------------------------------------------------\n" );

	CCycleCount total, encodeTotal;
	FOR_EACH_LL( g_Tracks, i )
	{
		CChangeTrack *pCur = g_Tracks[i];
		CCycleCount::Add( pCur->m_Count, total, total );
		CCycleCount::Add( pCur->m_EncodeCount, encodeTotal, encodeTotal );
	}

	FOR_EACH_LL( g_Tracks, j )
	{
		CChangeTrack *pCur = g_Tracks[j];
	
		ConMsg( "%6.2fms       %5.2f    %6.2fms    %4d        %4d          %s\n", 
			pCur->m_Count.GetMillisecondsF(),
			pCur->m_Count.GetMillisecondsF() * 100.0f / total.GetMillisecondsF(),
			pCur->m_EncodeCount.GetMillisecondsF(),
			pCur->m_nChanged, 
			pCur->m_nUnchanged, 
			pCur->m_pName
			);
	}

	ConMsg( "\n\n" );
	ConMsg( "Total CalcDelta MS: %.2f\n\n", total.GetMillisecondsF() );
	ConMsg( "Total Encode    MS: %.2f\n\n", encodeTotal.GetMillisecondsF() );
}


//-----------------------------------------------------------------------------
// Purpose: Entity wasn't dealt with in packet, but it has been deleted, we'll flag
//  the entity for destruction
// Input  : type - 
//			entnum - 
//			*from - 
//			*to - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
static inline bool SV_NeedsExplicitDestroy( int entnum, CFrameSnapshot *from, CFrameSnapshot *to )
{
	// Never on uncompressed packet

	if( entnum >= to->m_nNumEntities || to->m_pEntities[entnum].m_pClass == NULL ) // doesn't exits in new
	{
		if ( entnum >= from->m_nNumEntities )
			return false; // didn't exist in old

		// in old, but not in new, destroy.
		if( from->m_pEntities[ entnum ].m_pClass != NULL ) 
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Creates a delta header for the entity
//-----------------------------------------------------------------------------
static inline void SV_UpdateHeaderDelta( 
	CEntityWriteInfo &u,
	int entnum )
{
	// Profiling info
	//	u.m_nTotalGap += entnum - u.m_nHeaderBase;
	//	u.m_nTotalGapCount++;

	// Keep track of number of headers so we can tell the client
	u.m_nHeaderCount++;
	u.m_nHeaderBase = entnum;
}


//
// Write the delta header. Also update the header delta info if bUpdateHeaderDelta is true.
//
// There are some cases where you want to tenatively write a header, then possibly back it out.
// In these cases:
// - pass in false for bUpdateHeaderDelta
// - store the return value from SV_WriteDeltaHeader
// - call SV_UpdateHeaderDelta ONLY if you want to keep the delta header it wrote
//
static inline void SV_WriteDeltaHeader(
	CEntityWriteInfo &u,
	int entnum,
	int flags )
{
	bf_write *pBuf = u.m_pBuf;

	// int startbit = pBuf->GetNumBitsWritten();

	int offset = entnum - u.m_nHeaderBase - 1;

	Assert ( offset >= 0 );

	SyncTag_Write( u.m_pBuf, "Hdr" );

	pBuf->WriteUBitVar( offset );

	if ( flags & FHDR_LEAVEPVS )
	{
		pBuf->WriteOneBit( 1 ); // leave PVS bit
		pBuf->WriteOneBit( flags & FHDR_DELETE );
	}
	else
	{
		pBuf->WriteOneBit( 0 ); // delta or enter PVS
		pBuf->WriteOneBit( flags & FHDR_ENTERPVS );
	}
	
	SV_UpdateHeaderDelta( u, entnum );
}

ConVar sv_show_cull_props( "sv_show_cull_props", "0", FCVAR_RELEASE, "Print out props that are being culled/added by recipent proxies." );


// NOTE: to optimize this, it could store the bit offsets of each property in the packed entity.
// It would only have to store the offsets for the entities for each frame, since it only reaches 
// into the current frame's entities here.
static inline void SV_WritePropsFromPackedEntity( CEntityWriteInfo &u, CalcDeltaResultsList_t &checkProps, CHLTVServer *hltv )
{
	const PackedEntity * pTo = u.m_pNewPack;
	const PackedEntity * pFrom = u.m_pOldPack;
	SendTable *pSendTable = pTo->m_pServerClass->m_pTable;

	CServerDTITimer timer( pSendTable, SERVERDTI_WRITE_DELTA_PROPS );
#if defined( REPLAY_ENABLED )
	if ( g_bServerDTIEnabled && !u.m_pServer->IsHLTV() && !u.m_pServer->IsReplay() )
#else
	if ( g_bServerDTIEnabled && !u.m_pServer->IsHLTV() )
#endif
	{
		ICollideable *pEnt = sv.edicts[pTo->m_nEntityIndex].GetCollideable();
		ICollideable *pClientEnt = sv.edicts[u.m_nClientEntity].GetCollideable();
		if ( pEnt && pClientEnt )
		{
			float flDist = (pEnt->GetCollisionOrigin() - pClientEnt->GetCollisionOrigin()).Length();
			ServerDTI_AddEntityEncodeEvent( pSendTable, flDist );
		}
	}

	SerializedEntityHandle_t toEntity = pTo->GetPackedData();
	Assert( toEntity != SERIALIZED_ENTITY_HANDLE_INVALID );

	// Cull out the properties that their proxies said not to send to this client.
	CalcDeltaResultsList_t sendProps;
	bf_write bufStart;

	// cull properties that are removed by SendProxies for this client.
	// don't do that for HLTV relay proxies
	if ( u.m_bCullProps )
	{
		SendTable_CullPropsFromProxies( 
		pSendTable, 
		checkProps,
		u.m_nClientEntity-1,
		
		pFrom->GetRecipients(),
		pFrom->GetNumRecipients(),
		
		pTo->GetRecipients(),
		pTo->GetNumRecipients(),

		sendProps
		);

		if ( sv_show_cull_props.GetBool() )
		{
			// find difference
			CalcDeltaResultsList_t culledProps;
			CalcDeltaResultsList_t addProps;

			FOR_EACH_VEC( checkProps, i )
			{
				if ( sendProps.Find( checkProps[i] ) == -1 )
				{
					culledProps.AddToTail( checkProps[i] );
				}
			}

			FOR_EACH_VEC( sendProps, i )
			{
				if ( checkProps.Find( sendProps[i] ) == -1 )
				{
					addProps.AddToTail( sendProps[i] );
				}
			}

			if ( culledProps.Count() || addProps.Count() )
			{
				Msg("CullPropsFromProxies for class %s, client=%d:\n", pSendTable->GetName(), u.m_nClientEntity - 1 );
				FOR_EACH_VEC( culledProps, i )
				{
					const SendProp *pProp = pSendTable->m_pPrecalc->GetProp( culledProps[i] );
					if ( pProp )
					{
						Msg("   CULL: %s type=%d\n", pProp->GetName(), (int)pProp->GetType() );
					}
				}

				FOR_EACH_VEC( addProps, i )
				{
					const SendProp *pProp = pSendTable->m_pPrecalc->GetProp( addProps[i] );
					if ( pProp )
					{
						Msg("    ADD: %s type=%d\n", pProp->GetName(), (int)pProp->GetType() );
					}
				}

			}
		}
	}
	else
	{
		// this is a HLTV relay proxy
		bufStart = *u.m_pBuf;
		sendProps.CopyArray( checkProps.Base(), checkProps.Count() );
	}
		
	SendTable_WritePropList(
		pSendTable, 
		toEntity,
		u.m_pBuf, 
		pTo->m_nEntityIndex,
		&sendProps
		);

	if ( !u.m_bCullProps )
	{
		if( hltv )
		{
			// this is a HLTV relay proxy, cache delta bits
			int nBits = u.m_pBuf->GetNumBitsWritten() - bufStart.GetNumBitsWritten();
			hltv->m_DeltaCache.AddDeltaBits( pTo->m_nEntityIndex, u.m_pFromSnapshot->m_nTickCount, nBits, &bufStart );
		}

#if defined( REPLAY_ENABLED )
		if ( replay )
		{
			// this is a replay relay proxy, cache delta bits
			int nBits = u.m_pBuf->GetNumBitsWritten() - bufStart.GetNumBitsWritten();
			replay->m_DeltaCache.AddDeltaBits( pTo->m_nEntityIndex, u.m_pFromSnapshot->m_nTickCount, nBits, &bufStart );
		}
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: See if the entity needs a "hard" reset ( i.e., and explicit creation tag )
//  This should only occur if the entity slot deleted and re-created an entity faster than
//  the last two updates toa  player.  Should never or almost never occur.  You never know though.
// Input  : type - 
//			entnum - 
//			*from - 
//			*to - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
static inline bool SV_NeedsExplicitCreate( CEntityWriteInfo &u )
{
	// Never on uncompressed packet
	if ( !u.m_bAsDelta )
	{
		return false;
	}

	const int index = u.m_nNewEntity;

	if ( index >= u.m_pFromSnapshot->m_nNumEntities )
		return true; // entity didn't exist in old frame, so create

	// Server thinks the entity was continues, but the serial # changed, so we might need to destroy and recreate it
	const CFrameSnapshotEntry *pFromEnt = &u.m_pFromSnapshot->m_pEntities[index];
	const CFrameSnapshotEntry *pToEnt = &u.m_pToSnapshot->m_pEntities[index];

	bool bNeedsExplicitCreate = (pFromEnt->m_pClass == NULL) || pFromEnt->m_nSerialNumber != pToEnt->m_nSerialNumber;

#ifdef _DEBUG
	if ( !bNeedsExplicitCreate )
	{
		// If it doesn't need explicit create, then the classnames should match.
		// This assert is analagous to the "Server / Client mismatch" one on the client.
		static int nWhines = 0;
		if ( pFromEnt->m_pClass->GetName() != pToEnt->m_pClass->GetName() )
		{
			if ( ++nWhines < 4 )
			{
				Msg( "ERROR in SV_NeedsExplicitCreate: ent %d from/to classname (%s/%s) mismatch.\n", u.m_nNewEntity, pFromEnt->m_pClass->GetName(), pToEnt->m_pClass->GetName() );
			}
		}
	}
#endif

	return bNeedsExplicitCreate;
}

//given a packed entity, this will determine how many properties have a change flag, or -1 if there is no change flag set
static int GetPackedEntityChangedProps( const PackedEntity* pEntity, int nTick, CalcDeltaResultsList_t& Results )
{
	const CChangeFrameList* pChangeList = pEntity->GetChangeFrameList();
	if( !pChangeList )
		return -1;

	//note that this is called a LOT (by each client for each changed object) so performance is very important in here
	const int* RESTRICT pCurrBucket = pChangeList->GetBucketChangeTicks();
	const int* RESTRICT pEndBucket	= pCurrBucket + pChangeList->GetNumBuckets();

	const int* RESTRICT pStartProps = pChangeList->GetChangeTicks();
	const int* RESTRICT pCurrProp	= pStartProps;
	const int* RESTRICT pEndProps	= pCurrProp + pChangeList->GetNumProps();

	//go through each bucket, which has the maximum tick count of the properties in its range
	for( ; pCurrBucket != pEndBucket; ++pCurrBucket )
	{
		//if the bucket hasn't changed since the time we care about, we can just ignore the bucket and carry on (big win! X props skipped with 1 check!)
		if( *pCurrBucket > nTick )
		{
			//we have a change in our bucket we need to find, so scan through each prop in the bucket range
			const int* RESTRICT pPropBucketEnd = MIN( pCurrProp + CChangeFrameList::knBucketSize, pEndProps );
			for( ; pCurrProp != pPropBucketEnd; ++pCurrProp )
			{
				if( *pCurrProp > nTick )
					Results.AddToTail( pCurrProp - pStartProps );
			}
		}
		else
		{
			//no changed props in our bucket, so skip ahead
			pCurrProp += CChangeFrameList::knBucketSize;
		}
	}

	return Results.Count();
}


static inline void SV_DetermineUpdateType( CEntityWriteInfo &u, CHLTVServer *hltv )
{
	// Figure out how we want to update the entity.
	if( u.m_nNewEntity < u.m_nOldEntity )
	{
		// If the entity was not in the old packet (oldnum == 9999), then 
		// delta from the baseline since this is a new entity.
		u.m_UpdateType = EnterPVS;
		return;
	}
	
	if( u.m_nNewEntity > u.m_nOldEntity )
	{
		// If the entity was in the old list, but is not in the new list 
		// (newnum == 9999), then construct a special remove message.
		u.m_UpdateType = LeavePVS;
		return;
	}
	
	Assert( u.m_pToSnapshot->m_pEntities[ u.m_nNewEntity ].m_pClass );

	bool recreate = SV_NeedsExplicitCreate( u );
	
	if ( recreate )
	{
		u.m_UpdateType = EnterPVS;
		return;
	}

	// These should be the same! If they're not, then it should detect an explicit create message.
	Assert( u.m_pOldPack->m_pServerClass == u.m_pNewPack->m_pServerClass);
	
	// We can early out with the delta bits if we are using the same pack handles...
	if ( u.m_pOldPack == u.m_pNewPack )
	{
		Assert( u.m_pOldPack != NULL );
		u.m_UpdateType = PreserveEnt;
		return;
	}

#ifndef _X360
	if ( !u.m_bCullProps )
	{
		int nBits = 0;
		Assert( u.m_pServer->IsHLTV() ); // because !u.m_bCullProps only happens when u.m_pServer->IsHLTV() AND sv is INactive, at least currently.So, replay should never play a role... ?
			
		unsigned char *pBuffer = hltv ? hltv->m_DeltaCache.FindDeltaBits( u.m_nNewEntity, u.m_pFromSnapshot->m_nTickCount, nBits ) :
#if defined( REPLAY_ENABLED )
									  replay ? replay->m_DeltaCache.FindDeltaBits( u.m_nNewEntity, u.m_pFromSnapshot->m_nTickCount, nBits ) :
#else
			                          NULL;
#endif

		if ( pBuffer )
		{
			if ( nBits > 0 )
			{
				// Write a header.
				SV_WriteDeltaHeader( u, u.m_nNewEntity, FHDR_ZERO );

				// just write the cached bit stream 
				u.m_pBuf->WriteBits( pBuffer, nBits );

				u.m_UpdateType = DeltaEnt;
			}
			else
			{
				u.m_UpdateType = PreserveEnt;
			}

			return; // we used the cache, great
		}
	}
#endif

	CalcDeltaResultsList_t checkProps;

	int nCheckProps = GetPackedEntityChangedProps( u.m_pNewPack, u.m_pFromSnapshot->m_nTickCount, checkProps );	
	if ( nCheckProps == -1 )
	{
		// check failed, we have to recalc delta props based on from & to snapshot
		// that should happen only in HLTV/Replay demo playback mode, this code is really expensive
		SendTable_CalcDelta(
			u.m_pOldPack->m_pServerClass->m_pTable, 
			u.m_pOldPack->GetPackedData(),
			u.m_pNewPack->GetPackedData(),
			u.m_nNewEntity, checkProps			
			);
	}

	if ( nCheckProps > 0 )
	{
		// Write a header.
		SV_WriteDeltaHeader( u, u.m_nNewEntity, FHDR_ZERO );
#if defined( DEBUG_NETWORKING )
		int startBit = u.m_pBuf->GetNumBitsWritten();
#endif
		SV_WritePropsFromPackedEntity( u, checkProps, hltv );
#if defined( DEBUG_NETWORKING )
		int endBit = u.m_pBuf->GetNumBitsWritten();
		TRACE_PACKET( ( "    Delta Bits (%d) = %d (%d bytes)\n", u.m_nNewEntity, (endBit - startBit), ( (endBit - startBit) + 7 ) / 8 ) );
#endif
		// If the numbers are the same, then the entity was in the old and new packet.
		// Just delta compress the differences.
		u.m_UpdateType = DeltaEnt;
	}
	else
	{
#ifndef _X360
		if ( !u.m_bCullProps )
		{
			if ( hltv )
			{
				// no bits changed, PreserveEnt
				hltv->m_DeltaCache.AddDeltaBits( u.m_nNewEntity, u.m_pFromSnapshot->m_nTickCount, 0, NULL );
			}

#if defined( REPLAY_ENABLED )
			if ( replay )
			{
				// no bits changed, PreserveEnt
				replay->m_DeltaCache.AddDeltaBits( u.m_nNewEntity, u.m_pFromSnapshot->m_nTickCount, 0, NULL );
			}
#endif
		}
#endif
		u.m_UpdateType = PreserveEnt;
	}
}

static inline ServerClass* GetEntServerClass(edict_t *pEdict)
{
	return pEdict->GetNetworkable()->GetServerClass();
}



static inline void SV_WriteEnterPVS( CEntityWriteInfo &u )
{
	TRACE_PACKET(( "  SV Enter PVS (%d) %s\n", u.m_nNewEntity, u.m_pNewPack->m_pServerClass->m_pNetworkName ) );

	SV_WriteDeltaHeader( u, u.m_nNewEntity, FHDR_ENTERPVS );

	Assert( u.m_nNewEntity < u.m_pToSnapshot->m_nNumEntities );

	CFrameSnapshotEntry *entry = &u.m_pToSnapshot->m_pEntities[u.m_nNewEntity];

	ServerClass *pClass = entry->m_pClass;

	if ( !pClass )
	{
		Host_Error("SV_CreatePacketEntities: GetEntServerClass failed for ent %d.\n", u.m_nNewEntity);
	}
	
	TRACE_PACKET(( "  SV Enter Class %s\n", pClass->m_pNetworkName ) );

	if ( pClass->m_ClassID >= u.m_pServer->serverclasses )
	{
		ConMsg( "pClass->m_ClassID(%i) >= %i\n", pClass->m_ClassID, u.m_pServer->serverclasses );
		Assert( 0 );
	}

	u.m_pBuf->WriteUBitLong( pClass->m_ClassID, u.m_pServer->serverclassbits );
	
	// Write some of the serial number's bits. 
	u.m_pBuf->WriteUBitLong( entry->m_nSerialNumber, NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS );

	// Get the baseline.
	// Since the ent is in the fullpack, then it must have either a static or an instance baseline.
	PackedEntity *pBaseline = ( u.m_bAsDelta && u.m_pBaseline ) ? framesnapshotmanager->GetPackedEntity( *u.m_pBaseline, u.m_nNewEntity ) : NULL;
	SerializedEntityHandle_t fromEntity = SERIALIZED_ENTITY_HANDLE_INVALID;

	if ( pBaseline && (pBaseline->m_pServerClass == u.m_pNewPack->m_pServerClass) )
	{
		fromEntity = pBaseline->GetPackedData();
	}
	else
	{
		// Since the ent is in the fullpack, then it must have either a static or an instance baseline.
		if ( !u.m_pServer->GetClassBaseline( pClass, &fromEntity ) )
		{
			Error( "SV_WriteEnterPVS: missing instance baseline for '%s'.", pClass->m_pNetworkName );
		}

		ErrorIfNot( fromEntity != SERIALIZED_ENTITY_HANDLE_INVALID,
			("SV_WriteEnterPVS: missing pFromData for '%s'.", pClass->m_pNetworkName)
		);
	}

	if ( u.m_pTo->from_baseline )
	{
		// remember that we sent this entity as full update from entity baseline
		u.m_pTo->from_baseline->Set( u.m_nNewEntity );
	}
	
	SerializedEntityHandle_t toEntity = u.m_pNewPack->GetPackedData();
	Assert( SERIALIZED_ENTITY_HANDLE_INVALID != toEntity );

	// send all changed properties when entering PVS (no SendProxy culling since we may use it as baseline
	u.m_nFullProps +=  SendTable_WriteAllDeltaProps( pClass->m_pTable, fromEntity, toEntity, u.m_pNewPack->m_nEntityIndex, u.m_pBuf );

	if ( u.m_nNewEntity == u.m_nOldEntity )
		u.NextOldEntity();  // this was a entity recreate

	u.NextNewEntity();
}


static inline void SV_WriteLeavePVS( CEntityWriteInfo &u )
{
	int headerflags = FHDR_LEAVEPVS;
	bool deleteentity = false;
	
	if ( u.m_bAsDelta )
	{
		deleteentity = SV_NeedsExplicitDestroy( u.m_nOldEntity, u.m_pFromSnapshot, u.m_pToSnapshot );	
	}
	
	if ( deleteentity )
	{
		// Mark that we handled deletion of this index
		u.m_DeletionFlags.Set( u.m_nOldEntity );

		headerflags |= FHDR_DELETE;
	}

	TRACE_PACKET( ( "  SV Leave PVS (%d) %s %s\n", u.m_nOldEntity, 
		deleteentity ? "deleted" : "left pvs",
		u.m_pOldPack->m_pServerClass->m_pNetworkName ) );

	SV_WriteDeltaHeader( u, u.m_nOldEntity, headerflags );

	u.NextOldEntity();
}


static inline void SV_WriteDeltaEnt( CEntityWriteInfo &u )
{
	TRACE_PACKET( ( "  SV Delta PVS (%d %d) %s\n", u.m_nNewEntity, u.m_nOldEntity, u.m_pOldPack->m_pServerClass->m_pNetworkName ) );

	// NOTE: it was already written in DetermineUpdateType. By doing it this way, we avoid an expensive
	// (non-byte-aligned) copy of the data.

	u.NextOldEntity();
	u.NextNewEntity();
}


static inline void SV_PreserveEnt( CEntityWriteInfo &u )
{
	TRACE_PACKET( ( "  SV Preserve PVS (%d) %s\n", u.m_nOldEntity, u.m_pOldPack->m_pServerClass->m_pNetworkName ) );

	// updateType is preserveEnt. The client will detect this because our next entity will have a newnum
	// that is greater than oldnum, in which case the client just keeps the current entity alive.
	u.NextOldEntity();
	u.NextNewEntity();
}


static inline void SV_WriteEntityUpdate( CEntityWriteInfo &u )
{
	switch( u.m_UpdateType )
	{
		case EnterPVS:
		{
			SV_WriteEnterPVS( u );
		}
		break;

		case LeavePVS:
		{
			SV_WriteLeavePVS( u );
		}
		break;

		case DeltaEnt:
		{
			SV_WriteDeltaEnt( u );
		}
		break;

		case PreserveEnt:
		{
			SV_PreserveEnt( u );
		}
		break;
	}
}


static inline int SV_WriteDeletions( CEntityWriteInfo &u )
{
	if( !u.m_bAsDelta )
		return 0;

	CFrameSnapshot *pFromSnapShot = u.m_pFromSnapshot;
	CFrameSnapshot *pToSnapShot = u.m_pToSnapshot;

	CUtlVector< int > deletions;

	int nLast = MAX( pFromSnapShot->m_nNumEntities, pToSnapShot->m_nNumEntities );
	for ( int i = 0; i < nLast; i++ )
	{
		// Packet update didn't clear it out expressly
		if ( u.m_DeletionFlags.Get( i ) ) 
			continue;

		// Looks like it should be gone
		bool bNeedsExplicitDelete = SV_NeedsExplicitDestroy( i, pFromSnapShot, pToSnapShot );
		if ( !bNeedsExplicitDelete && u.m_pTo )
		{
			bNeedsExplicitDelete = (pToSnapShot->m_iExplicitDeleteSlots.Find(i) != pToSnapShot->m_iExplicitDeleteSlots.InvalidIndex() );
			if ( bNeedsExplicitDelete )
			{
				const CFrameSnapshotEntry *pFromEnt = ( i < pFromSnapShot->m_nNumEntities ) ? &pFromSnapShot->m_pEntities[i] : NULL;
				const CFrameSnapshotEntry *pToEnt = ( i < pToSnapShot->m_nNumEntities ) ? &pToSnapShot->m_pEntities[i] : NULL;

				if ( pFromEnt && pToEnt )
				{
					bool bWillBeExplicitlyCreated = (pFromEnt->m_pClass == NULL) || pFromEnt->m_nSerialNumber != pToEnt->m_nSerialNumber;
					if ( bWillBeExplicitlyCreated && u.m_pTo->transmit_entity.Get(i) )
					{
						//Warning("Entity %d is being explicitly deleted, but it will be explicitly created.\n", i );
						bNeedsExplicitDelete = false;
					}
					else
					{
						//Warning("Entity %d is being explicitly deleted.\n", i );
					}
				}
			}
		}

		// Check conditions
		if ( bNeedsExplicitDelete )
		{
			TRACE_PACKET( ( "  SV Explicit Destroy (%d)\n", i ) );
			Assert( !u.m_pTo->transmit_entity.Get(i) );
			deletions.AddToTail( i );
		}
	}

	u.m_pBuf->WriteUBitVar( deletions.Count() );
	int nBase = -1;
	FOR_EACH_VEC( deletions, i )
	{
		int nSlot = deletions[ i ];
		int nDelta = nSlot - nBase;
		u.m_pBuf->WriteUBitVar( nDelta );
		nBase = nSlot;
	}
	return deletions.Count();
}



//internal implementation of writing the delta entities. This takes a buffer to use for writing the message to, in order to handle variable sized buffers more efficiently, and will return whether or not the message was able to fit into the
//provided buffer or not
static bool InternalWriteDeltaEntities( CBaseServer* pServer, CBaseClient *client, CClientFrame *to, CClientFrame *from, CSVCMsg_PacketEntities_t &msg, uint8* pScratchBuffer, uint32 nScratchBufferSize )
{
	VPROF_BUDGET( "WriteDeltaEntities", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	msg.Clear();

	// allocate the temp buffer for the packet ents
	bf_write entity_data_buf( pScratchBuffer, nScratchBufferSize );

	//note that we can intentionally overflow in certain cases, so turn off asserts
	entity_data_buf.SetAssertOnOverflow( false );

	// Setup the CEntityWriteInfo structure.
	CEntityWriteInfo u;
	u.m_pBuf = &entity_data_buf;
	u.m_pTo = to;
	u.m_pToSnapshot = to->GetSnapshot();
	u.m_pBaseline = client->m_pBaseline;
	u.m_nFullProps = 0;
	u.m_pServer = pServer;
	u.m_nClientEntity = client->GetPropCullClient()->m_nEntityIndex;

	CHLTVServer *hltv = pServer->IsHLTV() ? static_cast< CHLTVServer* >( pServer ) : NULL;
#ifndef _XBOX
#if defined( REPLAY_ENABLED )
	if ( hltv || pServer->IsReplay() )
#else
	if ( hltv )
#endif
	{
		// cull props only on master proxy
		u.m_bCullProps = sv.IsActive();
	}
	else
#endif
	{
		u.m_bCullProps = true;	// always cull props for players
	}

	if ( from != NULL )
	{
		u.m_bAsDelta = true;	
		u.m_pFrom = from;
		u.m_pFromSnapshot = from->GetSnapshot();
		Assert( u.m_pFromSnapshot );
	}
	else
	{
		u.m_bAsDelta = false;
		u.m_pFrom = NULL;
		u.m_pFromSnapshot = NULL;
	}

	u.m_nHeaderCount = 0;

	// Write the header, TODO use class SVC_PacketEntities

	TRACE_PACKET(( "WriteDeltaEntities (%d)\n", u.m_pToSnapshot->m_nNumEntities ));

	msg.set_max_entries( u.m_pToSnapshot->m_nNumEntities );
	msg.set_is_delta( u.m_bAsDelta );

	if ( u.m_bAsDelta )
	{
		msg.set_delta_from( u.m_pFrom->tick_count ); // This is the sequence # that we are updating from.
	}

	msg.set_baseline( client->m_nBaselineUsed );

#ifndef NO_SERVER_NET
	// Don't work too hard if we're using the optimized single-player mode.
	if ( !g_pLocalNetworkBackdoor )
	{
		// Iterate through the in PVS bitfields until we find an entity 
		// that was either in the old pack or the new pack
		u.NextOldEntity();
		u.NextNewEntity();

		while ( (u.m_nOldEntity != ENTITY_SENTINEL) || (u.m_nNewEntity != ENTITY_SENTINEL) )
		{
			u.m_pNewPack = (u.m_nNewEntity != ENTITY_SENTINEL) ? framesnapshotmanager->GetPackedEntity( *u.m_pToSnapshot, u.m_nNewEntity ) : NULL;
			u.m_pOldPack = (u.m_nOldEntity != ENTITY_SENTINEL) ? framesnapshotmanager->GetPackedEntity( *u.m_pFromSnapshot, u.m_nOldEntity ) : NULL;

			// Figure out how we want to write this entity.
			SV_DetermineUpdateType( u, hltv );
			SV_WriteEntityUpdate( u );
		}

		// Now write out the express deletions
		SV_WriteDeletions( u );
	}
#endif //NO_SERVER_NET

	msg.set_updated_entries( u.m_nHeaderCount );

	if( u.m_pBuf->IsOverflowed() )
	{
		return false;
	}

	// resize the buffer to the actual byte size
	int nBytesWritten = Bits2Bytes( u.m_pBuf->GetNumBitsWritten() );
	msg.mutable_entity_data()->assign( (const char*)pScratchBuffer, nBytesWritten);

	bool bUpdateBaseline = ( (client->m_nBaselineUpdateTick == -1) && 
		(u.m_nFullProps > 0 || !u.m_bAsDelta) );

	if ( bUpdateBaseline && u.m_pBaseline )
	{
		// tell client to use this snapshot as baseline update
		msg.set_update_baseline( true );
		client->m_nBaselineUpdateTick = to->tick_count;
	}
	else
	{
		msg.set_update_baseline( false );
	}

	return true;
}

/*
=============
WritePacketEntities

Computes either a compressed, or uncompressed delta buffer for the client.
Returns the size IN BITS of the message buffer created.
=============
*/

static ConVar sv_delta_entity_buffer_size( "sv_delta_entity_full_buffer_size", "196608", 0, "Buffer size for delta entities" );

void CBaseServer::WriteDeltaEntities( CBaseClient *client, CClientFrame *to, CClientFrame *from, CSVCMsg_PacketEntities_t &msg )
{
	// set from_baseline pointer if this snapshot may become a baseline update
	if ( client->m_nBaselineUpdateTick == -1 )
	{
		client->m_BaselinesSent.ClearAll();
		to->from_baseline = &client->m_BaselinesSent;
	}

	net_scratchbuffer_t scratch;
	int nScratchUseSize = MIN( scratch.Size(), sv_delta_entity_buffer_size.GetInt() );

	bool bFitIntoBuffer = InternalWriteDeltaEntities( this, client, to, from, msg, scratch.GetBuffer(), nScratchUseSize );
	if( !bFitIntoBuffer )
	{
		static double s_dblLastWriteDeltaEntitiesError = 0;
		double dblPlatFloatTime = Plat_FloatTime();
		if ( dblPlatFloatTime > s_dblLastWriteDeltaEntitiesError + 10 )
		{
			s_dblLastWriteDeltaEntitiesError = dblPlatFloatTime;
			Warning( "Cannot write delta entity message for client %s<%i><%s>, try increasing sv_delta_entity_full_buffer_size\n", client->GetClientName(), client->m_nEntityIndex, client->GetNetworkIDString() );
		}

		//perform overflow handling here
		AssertMsg1( bFitIntoBuffer, "Error: Unable to fit a delta entity message into the %d bytes. Try upping the variable sv_delta_entity_full_buffer_size.", nScratchUseSize );
	}
}


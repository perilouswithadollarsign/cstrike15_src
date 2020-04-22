//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "server_pch.h"
#include "client.h"
#include "sv_packedentities.h"
#include "bspfile.h"
#include "eiface.h"
#include "dt_send_eng.h"
#include "dt_common_eng.h"
#include "changeframelist.h"
#include "sv_main.h"
#include "hltvserver.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#endif
#include "dt_instrumentation_server.h"
#include "LocalNetworkBackdoor.h"
#include "tier0/vprof.h"
#include "host.h"
#include "networkstringtableserver.h"
#include "networkstringtable.h"
#include "utlbuffer.h"
#include "dt.h"
#include "con_nprint.h"
#include "smooth_average.h"
#include "vengineserver_impl.h"
#include "vstdlib/jobthread.h"
#include "enginethreads.h"
#include "networkvar.h"

#include "serializedentity.h"


#ifdef DEDICATED
IClientEntityList *entitylist = NULL;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sv_validate_edict_change_infos( "sv_validate_edict_change_infos", "0", FCVAR_RELEASE, "Verify that edict changeinfos are being calculated properly (used to debug local network backdoor mode)." );
ConVar sv_enable_delta_packing( "sv_enable_delta_packing", "0", FCVAR_RELEASE, "When enabled, this allows for entity packing to use the property changes for building up the data. This is many times faster, but can be disabled for error checking." );
ConVar sv_debugmanualmode( "sv_debugmanualmode", "0", FCVAR_RELEASE, "Make sure entities correctly report whether or not their network data has changed." );

static struct SPackedEntityStats
{
	uint32 m_numFastPathEncodes;
	uint32 m_numSlowPathEncodes;
} g_PackedEntityStats;

// Returns false and calls Host_Error if the edict's pvPrivateData is NULL.
static inline bool SV_EnsurePrivateData(edict_t *pEdict)
{
	if(pEdict->GetUnknown())
	{
		return true;
	}
	else
	{
		Host_Error("SV_EnsurePrivateData: pEdict->pvPrivateData==NULL (ent %d).\n", pEdict - sv.edicts);
		return false;
	}
}

// This function makes sure that this entity class has an instance baseline.
// If it doesn't have one yet, it makes a new one.
void SV_EnsureInstanceBaseline( ServerClass *pServerClass, int iEdict, SerializedEntityHandle_t handle )
{
	edict_t *pEnt = &sv.edicts[iEdict];
	ErrorIfNot( pEnt->GetNetworkable(),
		("SV_EnsureInstanceBaseline: edict %d missing ent", iEdict)
	);

	ServerClass *pClass = pEnt->GetNetworkable()->GetServerClass();

	// See if we already have a baseline for this class.
	if ( pClass->m_InstanceBaselineIndex == INVALID_STRING_INDEX )
	{
		AUTO_LOCK_FM( g_svInstanceBaselineMutex );

		// We need this second check in case multiple instances of the same class have grabbed the lock.
		if ( pClass->m_InstanceBaselineIndex == INVALID_STRING_INDEX )
		{
			char packedData[ MAX_PACKEDENTITY_DATA ];
			bf_write buf( "SV_EnsureInstanceBaseline", packedData, sizeof( packedData ) );

			// Write all fields (NULL) change list
			SendTable_WritePropList( pServerClass->m_pTable, handle, &buf, iEdict, NULL );

			char idString[32];
			Q_snprintf( idString, sizeof( idString ), "%d", pClass->m_ClassID );

			// Ok, make a new instance baseline so they can reference it.
			int temp = sv.GetInstanceBaselineTable()->AddString( 
				true, idString,	// Note we're sending a string with the ID number, not the class name.
				buf.GetNumBytesWritten(), packedData );
			Assert( temp != INVALID_STRING_INDEX );
			// Copy the baseline data into the handles table
			sv.m_BaselineHandles.Insert( temp, g_pSerializedEntities->CopySerializedEntity( handle, __FILE__, __LINE__ ) );
			// Insert a compiler and/or CPU memory barrier to ensure that all side-effects have
			// been published before the index is published. Otherwise the string index may
			// be visible before its initialization has finished. This potential problem is caused
			// by the use of double-checked locking -- the problem is that the code outside of the
			// lock is looking at the variable that is protected by the lock. See this article for details:
			// http://en.wikipedia.org/wiki/Double-checked_locking
			// Write-release barrier
			ThreadMemoryBarrier();
			pClass->m_InstanceBaselineIndex = temp;
		}
	}
	// Read-acquire barrier. This should be safe to omit because of dependencies
	// which enforce read ordering.
	//ThreadMemoryBarrier();
}


static inline bool DoesEdictChangeInfoContainPropIndex( SendTable *pSendTable, const CEdictChangeInfo *pCI, int nProp )
{
	CSendTablePrecalc *pPrecalc = pSendTable->m_pPrecalc;

	bool bFound = false;
	for ( int i=0; i < pCI->m_nChangeOffsets && !bFound; i++ )
	{
		unsigned short index = pPrecalc->m_PropOffsetToIndexMap.Find( pCI->m_ChangeOffsets[i] );
		if ( index == pPrecalc->m_PropOffsetToIndexMap.InvalidIndex() )
			continue;

		const PropIndicesCollection_t &coll = pPrecalc->m_PropOffsetToIndexMap[index];
		for ( int nIndex=0; nIndex < ARRAYSIZE( coll.m_Indices ); nIndex++ )
		{
			if ( coll.m_Indices[nIndex] == nProp )
			{
				bFound = true;
				break;
			}
		}
	}

	return bFound;
}

//given a previous frame and a change list of the delta properties, this will return a change list (either by taking it from the last frame if possible, or creating
//a copy) with the specified delta properties with an updated tick count
static inline CChangeFrameList * GetMergedChangeFrameList( PackedEntity* pPrevFrame, uint32 nNumProps, const CalcDeltaResultsList_t& ChangeList, uint32 nTick )
{
	//if we have a previous frame, we need to try and reuse it's change list, either as a copy or a baseline
	CChangeFrameList *pChangeFrame = NULL;

	if( pPrevFrame )
	{
		CActiveHltvServerIterator hltv;

#ifndef _XBOX	
#if defined( REPLAY_ENABLED )
		if ( hltv || (replay && replay->IsActive()) )
#else
		if ( hltv )
#endif	
		{
			// in HLTV or Replay mode every PackedEntity keeps it's own ChangeFrameList
			// we just copy the ChangeFrameList from prev frame and update it
			pChangeFrame = pPrevFrame->GetChangeFrameList()->Copy();
		}
		else
#endif
		{
			// Ok, now snag the changeframe from the previous frame and update the 'last frame changed'
			// for the properties in the delta.
			pChangeFrame = pPrevFrame->SnagChangeFrameList();
		}

		ErrorIfNot( pChangeFrame, ("SV_PackEntity: SnagChangeFrameList returned null") );
		ErrorIfNot( pChangeFrame->GetNumProps() == nNumProps,
			("SV_PackEntity: SnagChangeFrameList mismatched number of props[%d vs %d]", nNumProps, pChangeFrame->GetNumProps() ) );

		pChangeFrame->SetChangeTick( ChangeList.Base(), ChangeList.Count(), nTick );
	}
	else
	{
		pChangeFrame = new CChangeFrameList( nNumProps, nTick );
	}

	return pChangeFrame;
}


static void ValidateIncrementalChanges( ServerClass* pServerClass, const CalcDeltaResultsList_t& incremental, edict_t* edict, int edictIdx, SerializedEntityHandle_t tPrevProps )
{
	const SendTable *pSendTable = pServerClass->m_pTable;

	unsigned char tempData[ sizeof( CSendProxyRecipients ) * MAX_DATATABLE_PROXIES ];
	CUtlMemory< CSendProxyRecipients > recip( (CSendProxyRecipients*)tempData, pSendTable->m_pPrecalc->GetNumDataTableProxies() );

	//debug path, useful for detecting properties that differ from the binary diff and the change list
	SerializedEntityHandle_t comparePackedProps = g_pSerializedEntities->AllocateSerializedEntity(__FILE__, __LINE__);
	CalcDeltaResultsList_t compareDeltaProps;

	//generate a change list using the normal route
	SendTable_Encode( pSendTable, comparePackedProps, edict->GetUnknown(), edictIdx, &recip );
	SendTable_CalcDelta( pSendTable, tPrevProps, comparePackedProps, edictIdx, compareDeltaProps );

	//check to see if we have any missing properties or differing properties
	for( uint32 nCurrIncremental = 0; nCurrIncremental < incremental.Count(); ++nCurrIncremental )
	{
		int nPropIndex = incremental[ nCurrIncremental ];
		if( compareDeltaProps.Find( nPropIndex ) == -1 )
		{
			//missing a prop from our diff			
			const SendProp* pProp = pSendTable->m_pPrecalc->GetProp( nPropIndex );
			Msg( "SV_PackEntity: Encountered property %s:%s for class %s (idx:%d) in incremental change that was not in the diff change. This can lead to slight network inefficiency (Index: %d, Array: %s)\n", 
					pSendTable->GetName(), pProp->GetName(), pServerClass->GetName(), edictIdx, nPropIndex, (pProp->m_pParentArrayPropName ? pProp->m_pParentArrayPropName : "N/A") );
		}
	}

	for( uint32 nCurrDiff = 0; nCurrDiff < compareDeltaProps.Count(); ++nCurrDiff )
	{
		int nPropIndex = compareDeltaProps[ nCurrDiff ];
		if( incremental.Find( nPropIndex ) == -1 )
		{
			//missing a prop from our incremental
			const SendProp* pProp = pSendTable->m_pPrecalc->GetProp( nPropIndex );

			//some properties are encoded against the tick count, of which can change with out the property changing. However, the value on the receiving end stays constants, so we need to
			//be able to filter these out
			if( pProp->GetFlags() & SPROP_ENCODED_AGAINST_TICKCOUNT )
				continue;

			Msg( "SV_PackEntity: Encountered property %s:%s for class %s (idx:%d) in diff change that was not in the incremental change. This can lead to values not making it down to the client (Index: %d, Array: %s)\n", 
					pSendTable->GetName(), pProp->GetName(), pServerClass->GetName(), edictIdx, nPropIndex, (pProp->m_pParentArrayPropName ? pProp->m_pParentArrayPropName : "N/A") );
		}
	}

	g_pSerializedEntities->ReleaseSerializedEntity( comparePackedProps );
}


//-----------------------------------------------------------------------------
// Pack the entity....
//-----------------------------------------------------------------------------

static inline void SV_PackEntity( 
	int edictIdx, 
	edict_t* edict, 
	ServerClass* pServerClass,
	CFrameSnapshot *pSnapshot )
{
	Assert( edictIdx < pSnapshot->m_nNumEntities );

	MEM_ALLOC_CREDIT();

	const int iSerialNum = pSnapshot->m_pEntities[ edictIdx ].m_nSerialNumber;

	//if the entity has no state changes, we can just reuse the previously sent packet
	if( !edict->HasStateChanged() )
	{
		// Now this may not work if we didn't previously send a packet;
		// if not, then we gotta compute it
		bool bUsedPrev = framesnapshotmanager->UsePreviouslySentPacket( pSnapshot, edictIdx, iSerialNum );
		if ( bUsedPrev && !sv_debugmanualmode.GetInt() )
		{
			edict->ClearStateChanged();
			return;
		}
	}

	//try and get our last frame
	PackedEntity *pPrevFrame = framesnapshotmanager->GetPreviouslySentPacket( edictIdx, iSerialNum );
	const SendTable *pSendTable = pServerClass->m_pTable;
	
	//memory to hold onto our recipient list on the stack to avoid allocator overhead.
	unsigned char tempData[ sizeof( CSendProxyRecipients ) * MAX_DATATABLE_PROXIES ];
	CUtlMemory< CSendProxyRecipients > recip( (CSendProxyRecipients*)tempData, pSendTable->m_pPrecalc->GetNumDataTableProxies() );
	
	//the new packed up properties and the delta list from the last frame we will be building
	SerializedEntityHandle_t newPackedProps = SERIALIZED_ENTITY_HANDLE_INVALID;
	CalcDeltaResultsList_t deltaProps;

	//--------------------------------------
	// Fast path - Partial updating (catches about 95% of entities)
	//--------------------------------------

	//see if we can take the fast path (requires a previous frame)
	bool bCanFastPath = ( pPrevFrame && ( pPrevFrame->GetPackedData() != SERIALIZED_ENTITY_HANDLE_INVALID ) );
	//also, we can't fast path if the entire object has changed
	bCanFastPath &= !( edict->m_fStateFlags & FL_FULL_EDICT_CHANGED );
	//and we need a valid change list set
	bCanFastPath &= ( edict->GetChangeInfoSerialNumber() == g_pSharedChangeInfo->m_iSerialNumber );
	//and allow for disabling this through the console
	bCanFastPath &= ( sv_enable_delta_packing.GetBool() || sv_validate_edict_change_infos.GetBool() );

	//see if we can do an incremental update from our last frame
	if( bCanFastPath )
	{
		//see if we are in a situation where we are just doing diagnostics, meaning that we want to go through the work, but ultimately throw the results away after validating them
		bool bDiscardResults = !( sv_enable_delta_packing.GetBool() );

		//we can attempt a partial update
		bool bCanReuseOldData;
		newPackedProps = SendTable_BuildDeltaProperties( edict, edictIdx, pSendTable, pPrevFrame->GetPackedData(), deltaProps, &recip, bCanReuseOldData );
		
		//see if we were unable to merge (which may have been a result of us being able to reuse the original data)
		if( ( newPackedProps == SERIALIZED_ENTITY_HANDLE_INVALID ) )
		{
			//it wasn't able to overlay, so we need to fall back to the old way, unless it says nothing changed, at which point we can just reuse
			if( bCanReuseOldData && pPrevFrame->CompareRecipients( recip ) )
			{
				if( framesnapshotmanager->UsePreviouslySentPacket( pSnapshot, edictIdx, iSerialNum ) )
				{
					//we are going to use the previous, but make sure that there is no delta that we are missing
					if( sv_validate_edict_change_infos.GetBool() )
						ValidateIncrementalChanges( pServerClass, deltaProps, edict, edictIdx, pPrevFrame->GetPackedData() );	

					//successfully used the last one
					edict->ClearStateChanged();
					return;
				}
			}
		}
		else 
		{
			++g_PackedEntityStats.m_numFastPathEncodes;

			if( sv_validate_edict_change_infos.GetBool() )
			{
				ValidateIncrementalChanges( pServerClass, deltaProps, edict, edictIdx, pPrevFrame->GetPackedData() );	
				//we need to throw away our data if this is just for diagnostics
				if( bDiscardResults )
				{
					g_pSerializedEntities->ReleaseSerializedEntity( newPackedProps );
					newPackedProps = SERIALIZED_ENTITY_HANDLE_INVALID;
				}
			}		
		}
	}

	//--------------------------------------
	// Slow path - Must build a full data block and do a binary diff to spot the changes
	//--------------------------------------

	//if we made it through the fast path with no packed data, we need to pack it via the slow path
	if( newPackedProps == SERIALIZED_ENTITY_HANDLE_INVALID )
	{	
		++g_PackedEntityStats.m_numSlowPathEncodes;

		//build all the properties for this object into a serialized entity data block
		newPackedProps = g_pSerializedEntities->AllocateSerializedEntity(__FILE__, __LINE__);
		if( !SendTable_Encode( pSendTable, newPackedProps, edict->GetUnknown(), edictIdx, &recip ) )
		{							 
			Host_Error( "SV_PackEntity: SendTable_Encode returned false (ent %d).\n", edictIdx );
		}

		// A "SetOnly", etc. for indicating the entity should be sent to player B who is a piggybacked split screen player sharing the network pipe for for player A, needs to be changed to go to player player A
		SV_EnsureInstanceBaseline( pServerClass, edictIdx, newPackedProps );
		
		// If this entity was previously in there, then it should have a valid CChangeFrameList 
		// which we can delta against to figure out which properties have changed.
		if( pPrevFrame )
		{
			//do a binary diff and see which properties changed that way
			SendTable_CalcDelta( pSendTable, pPrevFrame->GetPackedData(), newPackedProps, edictIdx, deltaProps );

			// If it's non-manual-mode, but we detect that there are no changes here, then just
			// use the previous pSnapshot if it's available (as though the entity were manual mode).
			if ( ( deltaProps.Count() == 0 ) && pPrevFrame->CompareRecipients( recip ) )
			{
				if ( framesnapshotmanager->UsePreviouslySentPacket( pSnapshot, edictIdx, iSerialNum ) )
				{
					edict->ClearStateChanged();
					g_pSerializedEntities->ReleaseSerializedEntity( newPackedProps );
					return;
				}
			}		
		}
	}

	// Now make a PackedEntity and store the new packed data in there.
	{
		//get our change frame list for our new property set
		int nFlatProps = SendTable_GetNumFlatProps( pSendTable );
		CChangeFrameList* pChangeFrame = GetMergedChangeFrameList( pPrevFrame, nFlatProps, deltaProps, pSnapshot->m_nTickCount );

		//and setup our packed entity to hold all of our data
		PackedEntity *pPackedEntity = framesnapshotmanager->CreatePackedEntity( pSnapshot, edictIdx );
		pPackedEntity->SetChangeFrameList( pChangeFrame );
		pPackedEntity->SetServerAndClientClass( pServerClass, NULL );
		pPackedEntity->SetPackedData( newPackedProps );
		pPackedEntity->SetRecipients( recip );
	}

	edict->ClearStateChanged();
}

CON_COMMAND( sv_dump_entity_pack_stats, "Show stats on entity packing." )
{
	Msg("Entity Packing stats:\n");
	Msg("  numFastPathEncodes=%u\n", g_PackedEntityStats.m_numFastPathEncodes );
	Msg("  numSlowPathEncodes=%u\n", g_PackedEntityStats.m_numSlowPathEncodes );
}


// in HLTV mode we ALWAYS have to store position and PVS info, even if entity didnt change
void SV_FillHLTVData( CFrameSnapshot *pSnapshot, edict_t *edict, int iValidEdict )
{
	if ( pSnapshot->m_pHLTVEntityData && edict )
	{
		CHLTVEntityData *pHLTVData = &pSnapshot->m_pHLTVEntityData[iValidEdict];

		PVSInfo_t *pvsInfo = edict->GetNetworkable()->GetPVSInfo();

		if ( pvsInfo->m_nClusterCount == 1 )
		{
			// store cluster, if entity spawns only over one cluster
			pHLTVData->m_nNodeCluster = pvsInfo->m_pClusters[0];
		}
		else
		{
			// otherwise save PVS head node for larger entities
			pHLTVData->m_nNodeCluster = pvsInfo->m_nHeadNode | (1<<31);
		}

		// remember origin
		pHLTVData->origin[0] = pvsInfo->m_vCenter[0];
		pHLTVData->origin[1] = pvsInfo->m_vCenter[1];
		pHLTVData->origin[2] = pvsInfo->m_vCenter[2];
	}
}

#if defined( REPLAY_ENABLED )
// in Replay mode we ALWAYS have to store position and PVS info, even if entity didnt change
void SV_FillReplayData( CFrameSnapshot *pSnapshot, edict_t *edict, int iValidEdict )
{
	if ( pSnapshot->m_pReplayEntityData && edict )
	{
		CReplayEntityData *pReplayData = &pSnapshot->m_pReplayEntityData[iValidEdict];

		PVSInfo_t *pvsInfo = edict->GetNetworkable()->GetPVSInfo();

		if ( pvsInfo->m_nClusterCount == 1 )
		{
			// store cluster, if entity spawns only over one cluster
			pReplayData->m_nNodeCluster = pvsInfo->m_pClusters[0];
		}
		else
		{
			// otherwise save PVS head node for larger entities
			pReplayData->m_nNodeCluster = pvsInfo->m_nHeadNode | (1<<31);
		}

		// remember origin
		pReplayData->origin[0] = pvsInfo->m_vCenter[0];
		pReplayData->origin[1] = pvsInfo->m_vCenter[1];
		pReplayData->origin[2] = pvsInfo->m_vCenter[2];
	}
}
#endif

// Returns the SendTable that should be used with the specified edict.
SendTable* GetEntSendTable(edict_t *pEdict)
{
	if ( pEdict->GetNetworkable() )
	{
		ServerClass *pClass = pEdict->GetNetworkable()->GetServerClass();
		if ( pClass )
		{
			return pClass->m_pTable;
		}
	}

	return NULL;
}


void PackEntities_NetworkBackDoor( 
	int clientCount, 
	CGameClient **clients,
	CFrameSnapshot *snapshot )
{
	Assert( clientCount == 1 );

	CGameClient *client = clients[0];	// update variables cl, pInfo, frame for current client
	CCheckTransmitInfo *pInfo =  &client->m_PackInfo;

	//Msg ( " Using local network back door" );

	for ( int iValidEdict=0; iValidEdict < snapshot->m_nValidEntities; iValidEdict++ )
	{
		int index = snapshot->m_pValidEntities[iValidEdict];
		edict_t* edict = &sv.edicts[ index ];
		
		// this is a bit of a hack to ensure that we get a "preview" of the
		//  packet timstamp that the server will send so that things that
		//  are encoded relative to packet time will be correct
		Assert( edict->m_NetworkSerialNumber != -1 );

		bool bShouldTransmit = pInfo->m_pTransmitEdict->Get( index ) ? true : false;

		//CServerDTITimer timer( pSendTable, SERVERDTI_ENCODE );
		// If we're using the fast path for a single-player game, just pass the entity
		// directly over to the client.
		Assert( index < snapshot->m_nNumEntities );
		ServerClass *pSVClass = snapshot->m_pEntities[ index ].m_pClass;
		g_pLocalNetworkBackdoor->EntState( index, edict->m_NetworkSerialNumber, 
			pSVClass->m_ClassID, pSVClass->m_pTable, edict->GetUnknown(), edict->HasStateChanged(), bShouldTransmit );
		edict->ClearStateChanged();
	}
	
	// Tell the client about any entities that are now dormant.
	g_pLocalNetworkBackdoor->ProcessDormantEntities();
	InvalidateSharedEdictChangeInfos();
}

static ConVar sv_parallel_packentities( "sv_parallel_packentities", 
#ifndef DEDICATED
										"1", 
#else //DEDICATED
										"1", 
#endif //DEDICATED
										FCVAR_RELEASE );

struct PackWork_t
{
	int				nIdx;
	edict_t			*pEdict;
	CFrameSnapshot	*pSnapshot;

	static void Process( PackWork_t &item )
	{
		SV_PackEntity( item.nIdx, item.pEdict, item.pSnapshot->m_pEntities[ item.nIdx ].m_pClass, item.pSnapshot );
	}
};


void PackEntities_Normal( 
	int clientCount, 
	CGameClient **clients,
	CFrameSnapshot *snapshot )
{
	SNPROF("PackEntities_Normal");

	Assert( snapshot->m_nValidEntities <= MAX_EDICTS );

	PackWork_t workItems[MAX_EDICTS];
	int workItemCount = 0;

	// check for all active entities, if they are seen by at least on client, if
	// so, bit pack them 
	for ( int iValidEdict=0; iValidEdict < snapshot->m_nValidEntities; ++iValidEdict )
	{
		int index = snapshot->m_pValidEntities[ iValidEdict ];
		
		Assert( index < snapshot->m_nNumEntities );

		edict_t* edict = &sv.edicts[ index ];

		// if HLTV is running save PVS info for each entity
		SV_FillHLTVData( snapshot, edict, iValidEdict );
		
#if defined( REPLAY_ENABLED )
		// if Replay is running save PVS info for each entity
		SV_FillReplayData( snapshot, edict, iValidEdict );
#endif

		// Check to see if the entity changed this frame...
		//ServerDTI_RegisterNetworkStateChange( pSendTable, ent->m_bStateChanged );

		for ( int iClient = 0; iClient < clientCount; ++iClient )
		{
			// entities is seen by at least this client, pack it and exit loop
			CGameClient *client = clients[iClient];	// update variables cl, pInfo, frame for current client
			if ( client->IsHltvReplay() )
				continue; // clients in HLTV replay use HLTV stream that has already been pre-packed for them by HLTV master client. No need to do any packing while streaming HLTV contents

			CClientFrame *frame = client->m_pCurrentFrame;

			if( frame->transmit_entity.Get( index ) )
			{	
				workItems[workItemCount].nIdx = index;
				workItems[workItemCount].pEdict = edict;
				workItems[workItemCount].pSnapshot = snapshot;
				workItemCount++;
				break;
			}
		}
	}

	// Process work
	if ( sv_parallel_packentities.GetBool() )
	{
		ParallelProcess( workItems, workItemCount, &PackWork_t::Process );
	}
	else
	{
		int c = workItemCount;
		for ( int i = 0; i < c; ++i )
		{
			PackWork_t &w = workItems[ i ];
			SV_PackEntity( w.nIdx, w.pEdict, w.pSnapshot->m_pEntities[ w.nIdx ].m_pClass, w.pSnapshot );
		}
	}

	InvalidateSharedEdictChangeInfos();
}


//-----------------------------------------------------------------------------
// Writes the compressed packet of entities to all clients
//-----------------------------------------------------------------------------
void SV_ComputeClientPacks( 
	int clientCount, 
	CGameClient **clients,
	CFrameSnapshot *snapshot )
{
	SNPROF( "SV_ComputeClientPacks" );
	MDLCACHE_CRITICAL_SECTION_(g_pMDLCache);
	// Do some setup for each client
	{
		for (int iClient = 0; iClient < clientCount; ++iClient)
		{
			TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "CheckTransmit:%d", iClient );

			if ( clients[ iClient ]->IsHltvReplay() )
			{
				clients[ iClient ]->SetupHltvFrame( snapshot->m_nTickCount);
				continue; // skip all the transmit checks if we already have packs prepared by HLTV that we'll be sending anyway
			}

			CCheckTransmitInfo *pInfo = &clients[iClient]->m_PackInfo;
			clients[iClient]->SetupPackInfo( snapshot );
			if ( clients[iClient]->m_nDeltaTick < 0 )
			{
				serverGameEnts->PrepareForFullUpdate( clients[iClient]->edict );
			}
			serverGameEnts->CheckTransmit( pInfo, snapshot->m_pValidEntities, snapshot->m_nValidEntities );
			clients[iClient]->SetupPrevPackInfo();
		}
	}

	if ( g_pLocalNetworkBackdoor )
	{
#if !defined( DEDICATED )
		if ( GetBaseLocalClient().m_pServerClasses == NULL )
		{
			// Edge case - the local client has been cleaned up but we have a deferred tick to execute.
			// As far as I know, this can only happen if the player quits the game with a synchronous cbuf_execute while a server tick is in flight.
			// (e.g. user pauses the game, files a bug with vxconsole, then vxconsole unpauses the game while the user is still in the menu, then the user quits -> CRASH)
			Warning( "Attempting to pack entities on server with invalid local client state. Probably a result of VXConsole or con commands. Aborting SV_ComputeClientPacks.\n" );
			return;
		}
#endif

		// This will force network string table updates for local client to go through the backdoor if it's active
#ifdef SHARED_NET_STRING_TABLES
		sv.m_StringTables->TriggerCallbacks( clients[0]->m_nDeltaTick );
#else
		sv.m_StringTables->DirectUpdate( clients[0]->GetMaxAckTickCount() );
#endif
		
		g_pLocalNetworkBackdoor->StartEntityStateUpdate();

#ifndef DEDICATED
		int saveClientTicks = GetBaseLocalClient().GetClientTickCount();
		int saveServerTicks = GetBaseLocalClient().GetServerTickCount();
		bool bSaveSimulation = GetBaseLocalClient().insimulation;
		float flSaveLastServerTickTime = GetBaseLocalClient().m_flLastServerTickTime;

		GetBaseLocalClient().insimulation = true;
		GetBaseLocalClient().SetClientTickCount( sv.m_nTickCount );
		GetBaseLocalClient().SetServerTickCount( sv.m_nTickCount );

		GetBaseLocalClient().m_flLastServerTickTime = sv.m_nTickCount * host_state.interval_per_tick;
		g_ClientGlobalVariables.tickcount = GetBaseLocalClient().GetClientTickCount();
		g_ClientGlobalVariables.curtime = GetBaseLocalClient().GetTime();
#endif

		PackEntities_NetworkBackDoor( clientCount, clients, snapshot );

		g_pLocalNetworkBackdoor->EndEntityStateUpdate();

#ifndef DEDICATED
		GetBaseLocalClient().SetClientTickCount( saveClientTicks );
		GetBaseLocalClient().SetServerTickCount( saveServerTicks );
		GetBaseLocalClient().insimulation = bSaveSimulation;
		GetBaseLocalClient().m_flLastServerTickTime = flSaveLastServerTickTime;

		g_ClientGlobalVariables.tickcount = GetBaseLocalClient().GetClientTickCount();
		g_ClientGlobalVariables.curtime = GetBaseLocalClient().GetTime();
#endif

		PrintPartialChangeEntsList();
	}
	else
	{
		PackEntities_Normal( clientCount, clients, snapshot );
	}
}



// If the table's ID is -1, writes its info into the buffer and increments curID.
void SV_MaybeWriteSendTable( SendTable *pTable, bf_write &pBuf, bool bNeedDecoder )
{
	// Already sent?
	if ( pTable->GetWriteFlag() )
		return;

	pTable->SetWriteFlag( true );

	// write send table properties into stream
	if ( !SendTable_WriteInfos(pTable, pBuf, bNeedDecoder, false ) )
	{
		Host_Error( "Send Table buffer for class '%s' overflowed!!!\n", pTable->GetName() );
	}
}

// Calls SV_MaybeWriteSendTable recursively.
void SV_MaybeWriteSendTable_R( SendTable *pTable, bf_write &pBuf )
{
	SV_MaybeWriteSendTable( pTable, pBuf, false );

	// Make sure we send child send tables..
	for(int i=0; i < pTable->m_nProps; i++)
	{
		SendProp *pProp = &pTable->m_pProps[i];

		if( pProp->m_Type == DPT_DataTable )
			SV_MaybeWriteSendTable_R( pProp->GetDataTable(), pBuf );
	}
}


// Sets up SendTable IDs and sends an svc_sendtable for each table.
void SV_WriteSendTables( ServerClass *pClasses, bf_write &pBuf )
{
	ServerClass *pCur;

	DataTable_ClearWriteFlags( pClasses );

	// First, we send all the leaf classes. These are the ones that will need decoders
	// on the client.
	for ( pCur=pClasses; pCur; pCur=pCur->m_pNext )
	{
		SV_MaybeWriteSendTable( pCur->m_pTable, pBuf, true );
	}

	// Now, we send their base classes. These don't need decoders on the client
	// because we will never send these SendTables by themselves.
	for ( pCur=pClasses; pCur; pCur=pCur->m_pNext )
	{
		SV_MaybeWriteSendTable_R( pCur->m_pTable, pBuf );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : crc - 
//-----------------------------------------------------------------------------
void SV_ComputeClassInfosCRC( CRC32_t* crc )
{
	ServerClass *pClasses = serverGameDLL->GetAllServerClasses();

	for ( ServerClass *pClass=pClasses; pClass; pClass=pClass->m_pNext )
	{
		CRC32_ProcessBuffer( crc, (void *)pClass->m_pNetworkName, Q_strlen( pClass->m_pNetworkName ) );
		CRC32_ProcessBuffer( crc, (void *)pClass->m_pTable->GetName(), Q_strlen(pClass->m_pTable->GetName() ) );
	}
}

void CGameServer::AssignClassIds()
{
	ServerClass *pClasses = serverGameDLL->GetAllServerClasses();

	// Count the number of classes.
	int nClasses = 0;
	for ( ServerClass *pCount=pClasses; pCount; pCount=pCount->m_pNext )
	{
		++nClasses;
	}
	
	// These should be the same! If they're not, then it should detect an explicit create message.
	ErrorIfNot( nClasses <= MAX_SERVER_CLASSES,
		("CGameServer::AssignClassIds: too many server classes (%i, MAX = %i).\n", nClasses, MAX_SERVER_CLASSES );
	);

	serverclasses = nClasses;
	serverclassbits = Q_log2( serverclasses ) + 1;

	bool bSpew = CommandLine()->FindParm( "-netspike" ) ? true : false;

	int curID = 0;
	for ( ServerClass *pClass=pClasses; pClass; pClass=pClass->m_pNext )
	{
		pClass->m_ClassID = curID++;

		if ( bSpew )
		{
			Msg( "%d == '%s'\n", pClass->m_ClassID, pClass->GetName() );
		}
	}
}

// Assign each class and ID and write an svc_classinfo for each one.
void SV_WriteClassInfos(ServerClass *pClasses, bf_write &pBuf)
{
	// Assert( sv.serverclasses < MAX_SERVER_CLASSES );

	CSVCMsg_ClassInfo_t classinfomsg;

	classinfomsg.set_create_on_client( false );

	for ( ServerClass *pClass=pClasses; pClass; pClass=pClass->m_pNext )
	{
		CSVCMsg_ClassInfo::class_t *svclass = classinfomsg.add_classes();

		svclass->set_class_id( pClass->m_ClassID );
		svclass->set_data_table_name( pClass->m_pTable->GetName() );
		svclass->set_class_name( pClass->m_pNetworkName );
	}

	classinfomsg.WriteToBuffer( pBuf );
}

// This is implemented for the datatable code so its warnings can include an object's classname.
const char* GetObjectClassName( int objectID )
{
	if ( objectID >= 0 && objectID < sv.num_edicts )
	{
		return sv.edicts[objectID].GetClassName();
	}
	else
	{
		return "[unknown]";
	}
}


//-----------------------------------------------------------------------------
CON_COMMAND( sv_dump_class_info, "Dump server class infos." )
{
	ServerClass *pClasses = serverGameDLL->GetAllServerClasses();

	Msg( "ServerClassInfo:\n");
	Msg( "\tNetName(TableName):nProps PreCalcProps\n");

	for ( ServerClass *pCount=pClasses; pCount; pCount=pCount->m_pNext )
	{
		Msg("\t%s(%s):%d %d\n", pCount->GetName(), pCount->m_pTable->GetName(), pCount->m_pTable->m_nProps, pCount->m_pTable->m_pPrecalc ? pCount->m_pTable->m_pPrecalc->GetNumProps() : 0 );
	}
}


//-----------------------------------------------------------------------------
#include "tier1/tokenset.h"

const tokenset_t< SendPropType > gSendPropTypeTokenSet[] =
{
	{ "Int",       DPT_Int },
	{ "Float",     DPT_Float },
	{ "Vector",    DPT_Vector },
	{ "VectorXY",  DPT_VectorXY }, // Only encodes the XY of a vector, ignores Z
	{ "String",    DPT_String },
	{ "Array",     DPT_Array },	// An array of the base types (can't be of datatables).
	{ "DataTable", DPT_DataTable },
#if 0 // We can't ship this since it changes the size of DTVariant to be 20 bytes instead of 16 and that breaks MODs!!!
	{ "Quaternion,DPT_Quaternion },
#endif               
	{ "Int64",     DPT_Int64 },
	{ NULL, (SendPropType)0 }
};

//-----------------------------------------------------------------------------
static void PrintSendProp( int index, const SendProp *pSendProp )
{
	CUtlString flags;

	if ( pSendProp->GetFlags() & SPROP_VARINT ) { flags += "|VARINT"; }
	if ( pSendProp->GetFlags() & SPROP_UNSIGNED	) { flags += "|UNSIGNED"; }
	if ( pSendProp->GetFlags() & SPROP_COORD ) { flags += "|COORD"; }                       
	if ( pSendProp->GetFlags() & SPROP_NOSCALE ) { flags += "|NOSCALE"; }                     
	if ( pSendProp->GetFlags() & SPROP_ROUNDDOWN ) { flags += "|ROUNDDOWN"; }                   
	if ( pSendProp->GetFlags() & SPROP_ROUNDUP ) { flags += "|ROUNDUP"; }                     
	if ( pSendProp->GetFlags() & SPROP_NORMAL ) { flags += "|NORMAL"; }                      
	if ( pSendProp->GetFlags() & SPROP_EXCLUDE ) { flags += "|EXCLUDE"; }                     
	if ( pSendProp->GetFlags() & SPROP_XYZE ) { flags += "|XYZE"; }                        
	if ( pSendProp->GetFlags() & SPROP_INSIDEARRAY ) { flags += "|INSIDEARRAY"; }                 
	if ( pSendProp->GetFlags() & SPROP_PROXY_ALWAYS_YES ) { flags += "|PROXY_ALWAYS_YES"; }            
	if ( pSendProp->GetFlags() & SPROP_IS_A_VECTOR_ELEM ) { flags += "|IS_A_VECTOR_ELEM"; }            
	if ( pSendProp->GetFlags() & SPROP_COLLAPSIBLE ) { flags += "|COLLAPSIBLE"; }                 
	if ( pSendProp->GetFlags() & SPROP_COORD_MP ) { flags += "|COORD_MP"; }                    
	if ( pSendProp->GetFlags() & SPROP_COORD_MP_LOWPRECISION ) { flags += "|COORD_MP_LOWPRECISION"; }       
	if ( pSendProp->GetFlags() & SPROP_COORD_MP_INTEGRAL ) { flags += "|COORD_MP_INTEGRAL"; }           
	if ( pSendProp->GetFlags() & SPROP_CELL_COORD ) { flags += "|CELL_COORD"; }                  
	if ( pSendProp->GetFlags() & SPROP_CELL_COORD_LOWPRECISION ) { flags += "|CELL_COORD_LOWPRECISION"; }     
	if ( pSendProp->GetFlags() & SPROP_CELL_COORD_INTEGRAL ) { flags += "|CELL_COORD_INTEGRAL"; }         
	if ( pSendProp->GetFlags() & SPROP_CHANGES_OFTEN ) { flags += "|CHANGES_OFTEN"; }               
	if ( pSendProp->GetFlags() & SPROP_ENCODED_AGAINST_TICKCOUNT ) { flags += "|ENCODED_AGAINST_TICKCOUNT"; }   

	const char *pFlags = flags.Get();
	if ( !flags.IsEmpty() && *flags.Get() == '|' )
	{
		pFlags = flags.Get() + 1;
	}

	CUtlString name;
	if ( pSendProp->GetParentArrayPropName() )
	{
		name.Format( "%s:%s", pSendProp->GetParentArrayPropName(), pSendProp->GetName() );
	}
	else
	{
		name = pSendProp->GetName();
	}

	Msg( "\t%d,%s,%d,%d,%s,%s,%d\n", 
		 index,
		 name.String(),
		 pSendProp->GetOffset(), 
		 pSendProp->m_nBits,
		 gSendPropTypeTokenSet->GetNameByToken( pSendProp->GetType() ), 
		 pFlags,
		 pSendProp->GetPriority()
		 );
}

//-----------------------------------------------------------------------------
CON_COMMAND( sv_dump_class_table, "Dump server class table matching the pattern (substr)." )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Provide a substr to match class info against.\n" );
		return;
	}

	ServerClass *pClasses = serverGameDLL->GetAllServerClasses();

	for ( int a = 1; a < args.ArgC(); ++a )
	{
		for ( ServerClass *pCount=pClasses; pCount; pCount=pCount->m_pNext )
		{
			if ( pCount->m_pTable->m_pPrecalc && Q_stristr( pCount->GetName(), args[a] ) != NULL )
			{
				Msg( "%s:\n", pCount->GetName() );
				Msg( "\tIndex,Name,Offset,Bits,Type,Flags,Priority\n" );

				const CSendTablePrecalc *pPrecalc = pCount->m_pTable->m_pPrecalc;

				for ( int i = 0; i < pPrecalc->GetNumProps(); ++i )
				{
					const SendProp *pSendProp = pPrecalc->GetProp( i );

					PrintSendProp( i, pSendProp );

				}
			}
		}
	}
}


//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DT_LOCALTRANSFER_H
#define DT_LOCALTRANSFER_H
#ifdef _WIN32
#pragma once
#endif


#include "dt_send.h"
#include "dt_recv.h"
#include "dt.h"


// We have at most MAX_CHANGE_OFFSETS indices in an edict.
// We need 3 entries because of SPROP_IS_A_VECTOR_ELEM (in which case we could generate 3 props for each offset).
// Then we need 2 entries because we store up to 2 SendProp indices for each offset in PropIndicesCollection_t.
#define MAX_PROP_OFFSET_TO_INDICES_RESULTS (MAX_CHANGE_OFFSETS * 3 * PROP_INDICES_COLLECTION_NUM_INDICES)


class CBaseEdict;


// This sets up the ability to copy an entity with the specified SendTable directly
// into an entity with the specified RecvTable, thus avoiding compression overhead.
void LocalTransfer_InitFastCopy( 
	const SendTable *pSendTable, 
	const CStandardSendProxies *pSendProxies,
	RecvTable *pRecvTable,
	const CStandardRecvProxies *pRecvProxies,
	int &nSlowCopyProps,		// These are incremented to tell you how many fast copy props it found.
	int &nFastCopyProps
	);

// Transfer the data from pSrcEnt to pDestEnt using the specified SendTable and RecvTable.
void LocalTransfer_TransferEntity( 
	const CBaseEdict *pEdict, 
	const SendTable *pSendTable, 
	const void *pSrcEnt, 
	RecvTable *pRecvTable, 
	void *pDestEnt,
	bool bNewlyCreated,
	bool bJustEnteredPVS,
	int objectID );

// This returns at most MAX_PROP_OFFSET_TO_INDICES_RESULTS results into pOut, so make sure it has at least that much room.
int MapPropOffsetsToIndices( 
	const CBaseEdict *pEdict,
	CSendTablePrecalc *pPrecalc, 
	const unsigned short *pOffsets,
	unsigned short nOffsets,
	unsigned short *pOut );


// Call this after packing all the entities in a frame.
void PrintPartialChangeEntsList();


#endif // DT_LOCALTRANSFER_H

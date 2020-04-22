//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_RECV_ENG_H
#define DATATABLE_RECV_ENG_H
#ifdef _WIN32
#pragma once
#endif


#include "dt_recv.h"
#include "bitbuf.h"
#include "dt.h"
#include "tier1/utlvector.h"

class CStandardSendProxies;
class CSVCMsg_SendTable;

typedef intp SerializedEntityHandle_t;

// ------------------------------------------------------------------------------------------ //
// RecvTable functions.
// ------------------------------------------------------------------------------------------ //

// These are the module startup and shutdown routines.
bool		RecvTable_Init( RecvTable **pTables, int nTables );
void		RecvTable_Term( bool clearall = true );

// After calling RecvTable_Init to provide the list of RecvTables, call this
// as the server sends its SendTables over. If msg needs decoder
// it will precalculate the necessary data to actually decode this type of
// SendTable from the server. nDemoProtocol = 0 means current version.
bool RecvTable_RecvClassInfos( const CSVCMsg_SendTable& msg, int nDemoProtocol = 0 );

// After ALL the SendTables have been received, call this and it will create CRecvDecoders
// for all the SendTable->RecvTable matches it finds.
// Returns false if there is an unrecoverable error.
//
// bAllowMismatches is true when playing demos back so we can change datatables without breaking demos.
// If pAnyMisMatches is non-null, it will be set to true if the client's recv tables mismatched the server's ones.
bool		RecvTable_CreateDecoders( const CStandardSendProxies *pSendProxies, bool bAllowMismatches, bool *pAnyMismatches=NULL );

// objectID gets passed into proxies and can be used to track data on particular objects.
// NOTE: this function can ONLY decode a buffer outputted from RecvTable_MergeDeltas
//       or RecvTable_CopyEncoding because if the way it follows the exclude prop bits.
bool		RecvTable_Decode( 
	RecvTable *pTable, 
	void *pStruct, 
	SerializedEntityHandle_t handle,
	int objectID
	);

// This acts like a RecvTable_Decode() call where all properties are written and all their values are zero.
void RecvTable_DecodeZeros( RecvTable *pTable, void *pStruct, int objectID );

// This writes all pNewState properties into pOut.
//
// If pOldState is non-null and contains any properties that aren't in pNewState, then
// those properties are written to the output as well. Returns # of changed props
bool RecvTable_MergeDeltas(
	RecvTable *pTable,

	SerializedEntityHandle_t oldState, // Can be invalid
	SerializedEntityHandle_t newState,
	SerializedEntityHandle_t mergedState,

	int objectID = - 1,
	
	CUtlVector< int >	*pChangedProps = NULL
	);


bool RecvTable_ReadFieldList( 
	RecvTable *pTable, 

	bf_read &buf, 
	SerializedEntityHandle_t dest, 
	int nObjectId, 
	bool bUpdateDTI
	);

#endif // DATATABLE_RECV_ENG_H

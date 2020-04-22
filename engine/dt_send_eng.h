//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_SEND_ENG_H
#define DATATABLE_SEND_ENG_H
#ifdef _WIN32
#pragma once
#endif


#include "dt_send.h"
#include "bitbuf.h"
#include "utlmemory.h"
#include "tier1/utlvector.h"
#include "tier0/platform.h"


typedef uint32 CRC32_t;

class CStandardSendProxies;
struct edict_t;
typedef intp SerializedEntityHandle_t;

#define MAX_DELTABITS_SIZE 2048

// ------------------------------------------------------------------------ //
// SendTable functions.
// ------------------------------------------------------------------------ //

// Precalculate data that enables the SendTable to be used to encode data.
bool		SendTable_Init( SendTable **pTables, int nTables );
void		SendTable_Term();
CRC32_t		SendTable_GetCRC();
int			SendTable_GetNum();
SendTable	*SendTabe_GetTable(int index);


// Return the number of unique properties in the table.
int	SendTable_GetNumFlatProps( const SendTable *pTable );

// compares properties and writes delta properties
int SendTable_WriteAllDeltaProps(
	const SendTable *pTable,					
	SerializedEntityHandle_t fromEntity, // Can be invalid
	SerializedEntityHandle_t toEntity,
	const int nObjectID,
	bf_write *pBufOut );

//the buffer used to hold a list of delta changes between objects
typedef CUtlVectorFixedGrowable< int, 128 > CalcDeltaResultsList_t;

// Write the properties listed in pCheckProps and nCheckProps.
void SendTable_WritePropList(
	const SendTable *pTable,
	SerializedEntityHandle_t toEntity,
	bf_write *pOut,
	const int objectID,
	CalcDeltaResultsList_t *pCheckProps // NULL == Send ALL!
	);

//given an entity that has had a partial change, this will take the previous data block, and splice in the new values, setting new change fields to the specified tick for ones that have actually changed. This will
//return false if a delta table is not applicable and instead a full pack needs to be done
SerializedEntityHandle_t SendTable_BuildDeltaProperties(	edict_t *pEdict, int nObjectID, const SendTable* pTable, SerializedEntityHandle_t OldProps, 
															CalcDeltaResultsList_t &deltaProps, CUtlMemory<CSendProxyRecipients> *pRecipients, bool& bCanReuseOldData );


//
// Writes the property indices that must be written to move from pFromState to pToState into pDeltaProps.
// Returns the number of indices written to pDeltaProps.
//
void SendTable_CalcDelta(
	const SendTable *pTable,	
	SerializedEntityHandle_t fromEntity, // Can be invalid
	SerializedEntityHandle_t toEntity,
	const int objectID,

	CalcDeltaResultsList_t &deltaProps );


// This function takes the list of property indices in startProps and the values from
// SendProxies in pProxyResults, and fills in a new array in outProps with the properties
// that the proxies want to allow for iClient's client.
//
// If pOldStateProxies is non-null, this function adds new properties into the output list
// if a proxy has turned on from the previous state.
int SendTable_CullPropsFromProxies( 
	const SendTable *pTable,
	
	const CalcDeltaResultsList_t &startProps,

	const int iClient,
	
	const CSendProxyRecipients *pOldStateProxies,
	const int nOldStateProxies, 
	
	const CSendProxyRecipients *pNewStateProxies,
	const int nNewStateProxies,
	
	CalcDeltaResultsList_t &outProps
	);


// Encode the properties that are referenced in the delta bits.
// If pDeltaBits is NULL, then all the properties are encoded.
bool SendTable_Encode(
	const SendTable *pTable,
	SerializedEntityHandle_t handle,
	const void *pStruct, 
	int objectID = -1,
	CUtlMemory<CSendProxyRecipients> *pRecipients = NULL	// If non-null, this is an array of CSendProxyRecipients.
															// The array must have room for pTable->GetNumDataTableProxies().
	);


// In order to receive a table, you must send it from the server and receive its info
// on the client so the client knows how to unpack it.
bool SendTable_WriteInfos( const SendTable *pTable, bf_write& bfWrite, bool bNeedsDecoder, bool bIsEnd );

// do all kinds of checks on a packed entity bitbuffer
bool SendTable_CheckIntegrity( SendTable *pTable, const void *pData, const int nDataBits );


#endif // DATATABLE_SEND_ENG_H

//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <tier0/dbg.h>
#include <tier0/vprof.h>
#include <tier0/icommandline.h>
#include <commonmacros.h>
#include <checksum_crc.h>

#include "dt_send_eng.h"
#include "dt_encode.h"
#include "dt_instrumentation_server.h"
#include "dt_stack.h"
#include "common.h"
#include "packed_entity.h"
#include "serializedentity.h"
#include "edict.h"
#include "netmessages.h"
#include <algorithm>

// Define this to get a report on which props are being sent frequently and
// have high-numbered indices. Marking these with SPROP_CHANGES_OFTEN can
// improve CPU and network efficiency.
//#define PROP_SKIPPED_REPORT
#ifdef PROP_SKIPPED_REPORT
#include <map>
#include <string>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

extern int host_framecount;
CRC32_t SendTable_ComputeCRC();

#define CRC_SEND_TABLE_VERBOSE 0

#if CRC_SEND_TABLE_VERBOSE
#define CRCMSG DevMsg
#else
#define CRCMSG (void)
#endif
#define CRCMEMx( var, nByte ) ( unsigned int )( ( reinterpret_cast< unsigned char * >( &var ) )[nByte] )
#define CRCMEM4( var ) CRCMEMx( var, 0 ), CRCMEMx( var, 1 ), CRCMEMx( var, 2 ), CRCMEMx( var, 3 )
#define CRCFMT4 "%02X%02X%02X%02X"

class CSendTablePrecalc;
class CSendNode;

// This stack doesn't actually call any proxies. It uses the CSendProxyRecipients to tell
// what can be sent to the specified client.
class CPropCullStack : public CDatatableStack
{
public:
						CPropCullStack( 
							CSendTablePrecalc *pPrecalc, 
							int iClient, 
							const CSendProxyRecipients *pOldStateProxies,
							const int nOldStateProxies, 
							const CSendProxyRecipients *pNewStateProxies,
							const int nNewStateProxies
							) :
							
							CDatatableStack( pPrecalc, (unsigned char*)1, -1 ),
							m_pOldStateProxies( pOldStateProxies ),
							m_nOldStateProxies( nOldStateProxies ),
							m_pNewStateProxies( pNewStateProxies ),
							m_nNewStateProxies( nNewStateProxies )
						{
							m_pPrecalc = pPrecalc;
							m_iClient = iClient;
						}

	inline unsigned char*	CallPropProxy( CSendNode *pNode, int iProp, unsigned char *pStructBase )
	{
		if ( pNode->GetDataTableProxyIndex() == DATATABLE_PROXY_INDEX_NOPROXY )
		{
			return (unsigned char*)1;
		}
		else
		{
			Assert( pNode->GetDataTableProxyIndex() < m_nNewStateProxies );
			bool bCur = m_pNewStateProxies[ pNode->GetDataTableProxyIndex() ].m_Bits.Get( m_iClient ) != 0;

			if ( m_pOldStateProxies )
			{
				Assert( pNode->GetDataTableProxyIndex() < m_nOldStateProxies );
				bool bPrev = m_pOldStateProxies[ pNode->GetDataTableProxyIndex() ].m_Bits.Get( m_iClient ) != 0;
				if ( bPrev != bCur )
				{
					if ( bPrev )
					{
						// Old state had the data and the new state doesn't.
						return 0;
					}
					else
					{
						// Add ALL properties under this proxy because the previous state didn't have any of them.
						for ( int i=0; i < pNode->m_nRecursiveProps; i++ )
						{
							if ( m_nNewProxyProps < ARRAYSIZE( m_NewProxyProps ) )
							{
								m_NewProxyProps[m_nNewProxyProps] = pNode->m_iFirstRecursiveProp + i;
							}
							else
							{
								Error( "CPropCullStack::CallPropProxy - overflowed m_nNewProxyProps" );
							}

							++m_nNewProxyProps;
						}

						// Tell the outer loop that writes to m_pOutProps not to write anything from this
						// proxy since we just did.
						return 0;
					}
				}
			}

			return (unsigned char*)bCur;
		}
	}

	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase )
	{
		// Remember where the game code pointed us for this datatable's data so 
		m_pProxies[ pNode->GetRecursiveProxyIndex() ] = pStructBase;

		for ( int iChild=0; iChild < pNode->GetNumChildren(); iChild++ )
		{
			CSendNode *pCurChild = pNode->GetChild( iChild );
			
			unsigned char *pNewStructBase = NULL;
			if ( pStructBase )
			{
				pNewStructBase = CallPropProxy( pCurChild, pCurChild->m_iDatatableProp, pStructBase );
			}

			RecurseAndCallProxies( pCurChild, pNewStructBase );
		}
	}

	// Replay: when client is in replay, we substitute the client index and cull props as if they were being sent to a regular HLTV client
	void CullPropsFromProxies( const CalcDeltaResultsList_t &startProps, CalcDeltaResultsList_t &outProps )
	{
		m_pOutProps = &outProps;
		m_nNewProxyProps = 0;

		Init( false, false );

		// This list will have any newly available props written into it. Write a sentinel at the end.
		m_NewProxyProps[m_nNewProxyProps] = PROP_SENTINEL;
		int *pCurNewProxyProp = m_NewProxyProps;

		for ( int i=0; i < startProps.Count(); i++ )
		{
			int iProp = startProps[i];

			// Fill in the gaps with any properties that are newly enabled by the proxies.
			while ( *pCurNewProxyProp < iProp )
			{
				m_pOutProps->AddToTail( *pCurNewProxyProp );
				++pCurNewProxyProp;
			}

			// Now write this property's index if the proxies are allowing this property to be written.
			if ( IsPropProxyValid( iProp ) )
			{
				m_pOutProps->AddToTail( iProp );

				// avoid that we add it twice.
				if ( *pCurNewProxyProp == iProp )
					++pCurNewProxyProp;
			}
		}

		// add any remaining new proxy props
		while ( *pCurNewProxyProp < PROP_SENTINEL )
		{
			m_pOutProps->AddToTail( *pCurNewProxyProp );
			++pCurNewProxyProp;
		}
	}

	int GetNumOutProps()
	{
		return m_pOutProps->Count();
	}


private:
	CSendTablePrecalc		*m_pPrecalc;
	int						m_iClient;	// Which client it's encoding out for.
	const CSendProxyRecipients	*m_pOldStateProxies;
	const int					m_nOldStateProxies;
	
	const CSendProxyRecipients	*m_pNewStateProxies;
	const int					m_nNewStateProxies;

	// The output property list.
	CalcDeltaResultsList_t *m_pOutProps;

	int m_NewProxyProps[MAX_DATATABLE_PROPS+1];
	int m_nNewProxyProps;
};



// ----------------------------------------------------------------------------- //
// CDeltaCalculator encapsulates part of the functionality for calculating deltas between entity states.
//
// To use it, initialize it with the from and to states, then walk through the 'to' properties using
// SeekToNextToProp(). 
//
// At each 'to' prop, either call SkipToProp or CalcDelta
// ----------------------------------------------------------------------------- //
class CDeltaCalculator
{
public:

						CDeltaCalculator(
							CSendTablePrecalc *pPrecalc,
							const void *pFromState,
							const int nFromBits,
							const void *pToState,
							const int nToBits,
							int *pDeltaProps,
							int nMaxDeltaProps,
							const int objectID );
						
						~CDeltaCalculator();

	// Returns the next prop index in the 'to' buffer. Returns PROP_SENTINEL if there are no more.
	// If a valid property index is returned, you MUST call PropSkip() or PropCalcDelta() after calling this.
	int					SeekToNextProp();	

	// Ignore the current 'to' prop.
	void				PropSkip();

	// Seek the 'from' buffer up to the current property, determine if there is a delta, and
	// write the delta (and the property data) into the buffer if there is a delta.
	void				PropCalcDelta();

	// check if current propert is non zero, if so put it into delta indices list
	void				PropCalcNonZero();

	// Returns the number of deltas detected throughout the 
	int					GetNumDeltasDetected();
		

private:
	CSendTablePrecalc	*m_pPrecalc;

	bf_read				m_bfFromState;
	bf_read				m_bfToState;
	
	CDeltaBitsReader	m_FromBitsReader;
	CDeltaBitsReader	m_ToBitsReader;

	int					m_ObjectID;
	
	// Current property indices.
	int					m_iFromProp;
	int					m_iToProp;

	// Bit position into m_bfToState where the current prop (m_iToProp) starts.
	int					m_iToStateStart;

	// Output..
	int					*m_pDeltaProps;
	int					m_nMaxDeltaProps;
	int					m_nDeltaProps;
};


CDeltaCalculator::CDeltaCalculator(
	CSendTablePrecalc *pPrecalc,
	const void *pFromState,
	const int nFromBits,
	const void *pToState,
	const int nToBits,
	int *pDeltaProps,
	int nMaxDeltaProps,
	const int objectID ) 
	
	: m_bfFromState( "CDeltaCalculator->m_bfFromState", pFromState, Bits2Bytes( nFromBits ), nFromBits ),
	m_FromBitsReader( &m_bfFromState ),
	m_bfToState( "CDeltaCalculator->m_bfToState", pToState, Bits2Bytes( nToBits ), nToBits ),
	m_ToBitsReader( &m_bfToState )
{
	m_pPrecalc = pPrecalc;
	m_ObjectID = objectID;

	m_pDeltaProps = pDeltaProps;
	m_nMaxDeltaProps = nMaxDeltaProps;
	m_nDeltaProps = 0;
	
	// Walk through each property in the to state, looking for ones in the from 
	// state that are missing or that are different.
	if ( pFromState)
	{
		m_iFromProp = NextProp( &m_FromBitsReader );
	}
	else
	{
		m_iFromProp = PROP_SENTINEL;
	}
	
	m_iToProp = -1;
}


CDeltaCalculator::~CDeltaCalculator()
{
	// Make sure we didn't overflow.
	ErrorIfNot( 
		m_nDeltaProps <= m_nMaxDeltaProps, 
		( "SendTable_CalcDelta: overflowed props %d max %d on datatable '%s'.", m_nDeltaProps, m_nMaxDeltaProps, m_pPrecalc->GetSendTable()->GetName() ) 
		);

	ErrorIfNot( 
		!m_bfFromState.IsOverflowed(), 
		( "SendTable_CalcDelta: m_bfFromState overflowed %d max %d on datatable '%s'.", m_nDeltaProps, m_nMaxDeltaProps, m_pPrecalc->GetSendTable()->GetName() ) 
		);

	ErrorIfNot( 
		!m_bfToState.IsOverflowed(), 
		( "SendTable_CalcDelta: m_bfToState overflowed %d max %d on datatable '%s'.", m_nDeltaProps, m_nMaxDeltaProps, m_pPrecalc->GetSendTable()->GetName() ) 
		);

	// We may not have read to the end of our input bits, but we don't care.
	m_FromBitsReader.ForceFinished();
	m_ToBitsReader.ForceFinished();
}


inline int CDeltaCalculator::SeekToNextProp()
{
	m_iToProp = NextProp( &m_ToBitsReader );
	return m_iToProp;
}
		

inline void CDeltaCalculator::PropSkip()
{
	Assert( m_iToProp != -1 );
	SkipPropData( &m_bfToState, m_pPrecalc->GetProp( m_iToProp ) );
}

void CDeltaCalculator::PropCalcNonZero()
{
	const SendProp *pProp = m_pPrecalc->GetProp( m_iToProp );

	if ( !g_PropTypeFns[pProp->m_Type].IsEncodedZero( pProp, &m_bfToState ) )
	{
		// add non-zero property to index list
		if ( m_nDeltaProps < m_nMaxDeltaProps )
		{
			m_pDeltaProps[m_nDeltaProps] = m_iToProp;
		}
		++m_nDeltaProps;
	}
}

void CDeltaCalculator::PropCalcDelta()
{
	// Skip any properties in the from state that aren't in the to state.
	while ( m_iFromProp < m_iToProp )
	{
		SkipPropData( &m_bfFromState, m_pPrecalc->GetProp( m_iFromProp ) );
		m_iFromProp = NextProp( &m_FromBitsReader );
	}

	const SendProp *pProp = m_pPrecalc->GetProp( m_iToProp );

	int bChange = true;

	if ( m_iFromProp == m_iToProp )
	{
		// The property is in both states, so compare them and write the index 
		// if the states are different.
		bChange = g_PropTypeFns[pProp->m_Type].CompareDeltas( pProp, &m_bfFromState, &m_bfToState );
		
		// Seek to the next properties.
		m_iFromProp = NextProp( &m_FromBitsReader );
	}
	else
	{
		// Only the 'to' state has this property, so just skip its data and register a change.
		SkipPropData( &m_bfToState, pProp );
	}

	if ( bChange )
	{
		// Write the property index.
		if ( m_nDeltaProps < m_nMaxDeltaProps )
		{
			m_pDeltaProps[m_nDeltaProps] = m_iToProp;
			m_nDeltaProps++;
		}
	}
}


inline int CDeltaCalculator::GetNumDeltasDetected()
{
	return m_nDeltaProps;
}


// Returns true if bits are the same
static bool CompareBits( bf_read &from, bf_read &to, int numbits )
{
	unsigned int temp1, temp2;

	int remaining = numbits;
	while ( remaining > 0 )
	{
		int count = MIN( remaining, sizeof( unsigned int ) << 3 );
		temp1 = from.ReadUBitLong( count );
		temp2 = to.ReadUBitLong( count );
		if ( temp1 != temp2 )
			return false;
		remaining -= count;
	}

	return true;
}

// ----------------------------------------------------------------------------- //
// CEncodeInfo
// Used by SendTable_Encode.
// ----------------------------------------------------------------------------- //
class CEncodeInfo : public CServerDatatableStack
{
public:
	CEncodeInfo( CSendTablePrecalc *pPrecalc, unsigned char *pStructBase, int objectID ) :
	  CServerDatatableStack( pPrecalc, pStructBase, objectID )
	  {
	  }

public:

	CSerializedEntity *m_pEntity;

	bf_write	*m_pOut;
};


// ------------------------------------------------------------------------ //
// Globals.
// ------------------------------------------------------------------------ //

CUtlVector< SendTable* > g_SendTables;
#if CRC_SEND_TABLE_VERBOSE
CUtlVector< SendTable* > g_SendTablesReferenced;
#endif
CRC32_t	g_SendTableCRC = 0;



// ------------------------------------------------------------------------ //
// SendTable functions.
// ------------------------------------------------------------------------ //

bool s_debug_info_shown = false;
int  s_debug_bits_start = 0;


static inline void ShowEncodeDeltaWatchInfo( 
	const SendTable *pTable,
	const SendProp *pProp, 
	bf_read &buffer,
	const int objectID,
	const int index )
{
	if ( !ShouldWatchThisProp( pTable, objectID, pProp->GetName()) )
		return;
	
	static int lastframe = -1;
	if ( host_framecount != lastframe )
	{
		lastframe = host_framecount;
		ConDMsg( "E: delta entity: %i %s\n", objectID, pTable->GetName() );
	}

	// work on copy of bitbuffer
	bf_read copy = buffer;

	s_debug_info_shown = true;

	DecodeInfo info;
	info.m_pStruct = NULL;
	info.m_pData = NULL;
	info.m_pRecvProp = NULL;
	info.m_pProp = pProp;
	info.m_pIn = &copy;
	info.m_ObjectID = objectID;
	info.m_Value.m_Type = (SendPropType)pProp->m_Type;
	
	int startBit = copy.GetNumBitsRead();

	g_PropTypeFns[pProp->m_Type].Decode( &info );

	int bits = copy.GetNumBitsRead() - startBit;

	const char *type = g_PropTypeFns[pProp->m_Type].GetTypeNameString();
	const char *value = info.m_Value.ToString();

	ConDMsg( "+ %s %s, %s, index %i, bits %i, value %s\n", pTable->GetName(), pProp->GetName(), type, index, bits, value );
}



static inline void SendTable_EncodeProp( CEncodeInfo *pInfo, unsigned long iProp )
{
	// Write the index.
	// Encode it.
	const int iStartPos = pInfo->m_pOut->GetNumBitsWritten();

	pInfo->m_pEntity->AddPathAndOffset( iProp, iStartPos );

	const SendProp *pProp = pInfo->GetCurProp();

	// Call their proxy to get the property's value.
	DVariant var;

	pProp->GetProxyFn()( 
		pProp,
		pInfo->GetCurStructBase(), 
		pInfo->GetCurStructBase() + pProp->GetOffset(), 
		&var, 
		0, // iElement
		pInfo->GetObjectID()
		);

	g_PropTypeFns[pProp->m_Type].Encode( 
		pInfo->GetCurStructBase(), 
		&var, 
		pProp, 
		pInfo->m_pOut, 
		pInfo->GetObjectID()
		); 
}


static bool SendTable_IsPropZero( CEncodeInfo *pInfo, unsigned long iProp )
{
	const SendProp *pProp = pInfo->GetCurProp();

	// Call their proxy to get the property's value.
	DVariant var;
	
	pProp->GetProxyFn()( 
		pProp,
		pInfo->GetCurStructBase(), 
		pInfo->GetCurStructBase() + pProp->GetOffset(), 
		&var, 
		0, // iElement
		pInfo->GetObjectID()
		);

	return g_PropTypeFns[pProp->m_Type].IsZero( pInfo->GetCurStructBase(), &var, pProp );
}


int SendTable_CullPropsFromProxies( 
	const SendTable *pTable,
	
	const CalcDeltaResultsList_t &startProps,

	const int iClient,
	
	const CSendProxyRecipients *pOldStateProxies,
	const int nOldStateProxies, 
	
	const CSendProxyRecipients *pNewStateProxies,
	const int nNewStateProxies,
	
	CalcDeltaResultsList_t &outProps
	)
{
	Assert( !( nNewStateProxies && !pNewStateProxies ) );
	CPropCullStack stack( pTable->m_pPrecalc, iClient, pOldStateProxies, nOldStateProxies, pNewStateProxies, nNewStateProxies );
	
	stack.CullPropsFromProxies( startProps, outProps );

	ErrorIfNot( stack.GetNumOutProps() <= MAX_DATATABLE_PROPS, ("CullPropsFromProxies: overflow in '%s'.", pTable->GetName()) );
	return stack.GetNumOutProps();
}


// compares properties and writes delta properties, it ignores reciepients
int SendTable_WriteAllDeltaProps(
	const SendTable *pTable,					
	SerializedEntityHandle_t fromEntity, // Can be invalid
	SerializedEntityHandle_t toEntity,
	const int nObjectID,
	bf_write *pBufOut )
{
	CalcDeltaResultsList_t deltaProps;

	SendTable_CalcDelta( pTable, fromEntity, toEntity, nObjectID, deltaProps );

	// Write all properties.
	SendTable_WritePropList( 
		pTable,
		toEntity,
		pBufOut,				// output buffer
		nObjectID,
		&deltaProps );

	return deltaProps.Count();
}


bool SendTable_Encode(
	const SendTable *pTable,
	SerializedEntityHandle_t handle,
	const void *pStruct, 
	int objectID,
	CUtlMemory<CSendProxyRecipients> *pRecipients
	)
{
	CSerializedEntity *pEntity = reinterpret_cast< CSerializedEntity * >( handle );
	Assert( pEntity );
	pEntity->Clear();

	CSendTablePrecalc *pPrecalc = pTable->m_pPrecalc;
	ErrorIfNot( pPrecalc, ("SendTable_Encode: Missing m_pPrecalc for SendTable %s.", pTable->m_pNetTableName) );
	if ( pRecipients )
	{
		ErrorIfNot(	pRecipients->NumAllocated() >= pPrecalc->GetNumDataTableProxies(), ("SendTable_Encode: pRecipients array too small.") );
	}

	VPROF( "SendTable_Encode" );

	uint8 packedData[MAX_PACKEDENTITY_DATA];
	bf_write writeBuf( "CFlattenedSerializer::Encode writeBuf", packedData, sizeof( packedData ) );


	CServerDTITimer timer( pTable, SERVERDTI_ENCODE );

	// Setup all the info we'll be walking the tree with.
	CEncodeInfo info( pPrecalc, (unsigned char*)pStruct, objectID );

	info.m_pOut = &writeBuf;
	info.m_pEntity = pEntity;
	info.m_pRecipients = pRecipients;	// optional buffer to store the bits for which clients get what data.

	info.Init( false, false );

	const int iNumProps = pPrecalc->GetNumProps();

	//reserve memory for our path and offset information in our entity to avoid a lot of needless resizes
	info.m_pEntity->ReservePathAndOffsetMemory( iNumProps );

	for ( int iProp=0; iProp < iNumProps; iProp++ )
	{
		// skip if we don't have a valid prop proxy
		if ( !info.IsPropProxyValid( iProp ) )
			continue;

		info.SeekToProp( iProp );
		SendTable_EncodeProp( &info, iProp );
	}

	pEntity->PackWithFieldData( writeBuf.GetBasePointer(), writeBuf.GetNumBitsWritten() );

	return !writeBuf.IsOverflowed();
}

#ifdef PROP_SKIPPED_REPORT
static int s_skippedProps;
static int s_frameCount;

std::map<std::string, int> s_highProps;

const int kFrameReportInterval = 128;
const int kMinPropOffset = 200;
const int kReportLength = 10;
const bool bReportDetails = true;

void PrintPropSkippedReport()
{
	++s_frameCount;
	if ( s_frameCount < kFrameReportInterval )
		return;

	Msg( "%5d skipped props per frame\n", s_skippedProps / s_frameCount );
	s_frameCount = 0;
	s_skippedProps = 0;

	// Copy the data from s_highProps to a vector so that we can sort it by frequency.
	std::vector<std::pair<int, std::string>> sorted;
	for( auto data : s_highProps )
		sorted.push_back(std::pair<int, std::string>( data.second, data.first));
	std::sort( sorted.begin(), sorted.end() );
	std::reverse( sorted.begin(), sorted.end() );

	if ( bReportDetails )
	{
		for ( int i = 0; i < kReportLength && i < (int)sorted.size(); ++i )
		{
			Msg("      Sent %4d times, %s\n", sorted[i].first, sorted[i].second.c_str());
		}
	}

	s_highProps.clear();
}
#else
void PrintPropSkippedReport()
{
}
#endif

template< bool bDebugWatch >
void SendTable_WritePropList_Guts(
	const SendTable *pTable,
	SerializedEntityHandle_t toEntity,
	bf_write *pOut,
	const int objectID,
	CalcDeltaResultsList_t *pVecChanges // NULL == Send ALL! 
	)
{
	CSerializedEntity *pToEntity = (CSerializedEntity *)toEntity;
	if ( !pToEntity )
		return;

	CDeltaBitsWriter deltaBitsWriter( pOut );

	uint8 packedData[MAX_PACKEDENTITY_DATA];
	bf_write fieldDataBuf( "CFlattenedSerializer::WriteFieldList fieldDataBuf", packedData, sizeof( packedData ) );

	CSendTablePrecalc *pPrecalc = pTable->m_pPrecalc;

	if ( !pVecChanges || pVecChanges->Count() > 0 )
	{
		CSerializedEntityFieldIterator iterator( pToEntity );
		iterator.First();

		bf_read inputFieldData;
		inputFieldData.SetDebugName( "CFlattenedSerializer::WriteFieldList" );
		pToEntity->StartReading( inputFieldData );

		// Ok, they want to specify a small list of properties to check.
		CFieldPath toField = iterator.GetField();
		int i = 0;
		while ( !pVecChanges || i < pVecChanges->Count() )
		{
			if ( pVecChanges && toField < pVecChanges->Element( i ) )
			{
				// Keep nFieldIndex, nFieldCount, fieldPaths, and nextElement as local variables
				// instead of members of iterator to improve code-gen. Otherwise VC++ or gcc
				// will refetch some of them on each iteration.
				int nFieldIndex = iterator.GetIndex();
				int nFieldCount = pToEntity->GetFieldCount();
				const short* fieldPaths = pToEntity->GetFieldPaths();
				int nextElement = pVecChanges->Element(i);
				while ( toField < nextElement )
				{
#ifdef PROP_SKIPPED_REPORT
					++s_skippedProps;
#endif
					++nFieldIndex;
					if ( nFieldIndex >= nFieldCount )
						break;
					toField = fieldPaths[ nFieldIndex ];
				}
				// Update the state of iterator for the new index
				toField = iterator.SetIndex( nFieldIndex );
			}

			if ( toField == PROP_SENTINEL )
			{
				break;
			}
			else if ( !pVecChanges ||
				toField == pVecChanges->Element( i ) )
			{
				// See how many bits the data for this property takes up.
				int nTotalBits = iterator.GetLength();
				Assert( nTotalBits > 0 );
				inputFieldData.Seek( iterator.GetOffset() );

#ifdef PROP_SKIPPED_REPORT
					if ( toField > kMinPropOffset && pVecChanges && pVecChanges->Count() < 20 )
					{
						const SendProp *pProp = pPrecalc->GetProp( toField );
						char buffer[1000];
						V_sprintf_safe( buffer, "%s, field %d", pProp->GetName(), toField );
						s_highProps[ buffer ] += 1;
					}
#endif

				if ( bDebugWatch )
				{
					const SendProp *pProp = pPrecalc->GetProp( toField );
					Assert( pProp );
					ShowEncodeDeltaWatchInfo( pTable, pProp, inputFieldData, objectID, toField );
				}

				fieldDataBuf.WriteBitsFromBuffer( &inputFieldData, nTotalBits );

				TRACE_PACKET( ( "    Send Field (%s) = %d (%d bytes)\n", pPrecalc->GetProp( toField )->GetName(), nTotalBits, Bits2Bytes( nTotalBits ) ) );

				// Write the data into the output.
				deltaBitsWriter.WritePropIndex( toField );

				toField = iterator.Next();
			}

			++i;
		}
	}

	deltaBitsWriter.Finish();

	// Now append the field data to the stream of paths
	pOut->WriteBits( packedData, fieldDataBuf.GetNumBitsWritten() );
}

void SendTable_WritePropList(
	const SendTable *pTable,
	SerializedEntityHandle_t toEntity,
	bf_write *pOut,
	const int objectID,
	CalcDeltaResultsList_t *pVecChanges // NULL == Send ALL! 
	)
{
	bool bDebugWatch = Sendprop_UsingDebugWatch();
	if ( bDebugWatch )
	{
		s_debug_info_shown = false;
		s_debug_bits_start = pOut->GetNumBitsWritten();

		SendTable_WritePropList_Guts< true >( pTable, toEntity, pOut, objectID, pVecChanges );

		if ( s_debug_info_shown )
		{
			int  bits = pOut->GetNumBitsWritten() - s_debug_bits_start;
			ConDMsg( "= %i bits (%i bytes)\n", bits, Bits2Bytes(bits) );
		}
	}
	else
	{
		SendTable_WritePropList_Guts< false >( pTable, toEntity, pOut, objectID, pVecChanges );
	}
}


//given two data pointers and a bit range, this will determine if there is any difference between the bits. Note that this assumes that these can be interpreted as valid uint32's for speed
static inline bool DoesBitRangeMatch( const uint32* RESTRICT pOld, const uint32* RESTRICT pNew, uint32 nStartBit, uint32 nEndBit )
{
	//determine the starting index
	const uint32 nStartIndex	= nStartBit / 32;
	const uint32 nEndIndex		= ( nEndBit + 31 ) / 32;

	//determine bit overlaps
	const uint32 nStartIgnoreBits = nStartBit % 32;
	const uint32 nEndIgnoreBits   = nEndIndex * 32 - nEndBit;

	//now run through until we find a diff
	const uint32* pCurrOld = pOld + nStartIndex;
	const uint32* pCurrNew = pNew + nStartIndex;
	const uint32* pEndNew  = pNew + nEndIndex; 

	for(; pCurrNew != pEndNew; ++pCurrNew, ++pCurrOld )
	{
#if defined (_PS3) || defined (_X360)
        uint32 nDiff = DWordSwap((*pCurrNew ^ *pCurrOld));
#else
        uint32 nDiff = (*pCurrNew ^ *pCurrOld);
#endif
		if( nDiff )
		{
			//we have a diff, handle masking our range
			uint32 nDeltaStart = ( pCurrNew - pNew ) * 32;
			uint32 nDeltaEnd   = nDeltaStart + 32;

			if( nDeltaStart < nStartBit )
				nDiff &= ( 0xFFFFFFFF << nStartIgnoreBits );
			if( nEndBit < nDeltaEnd )
				nDiff &= ( 0xFFFFFFFF >> nEndIgnoreBits );

			if( nDiff )
				return false;
		}
	}

	//no difference
	return true;
}

//given a serialized entity and a property field, this will attempt to find the property field within that entity using the fact that field = index in nearly all cases. This will account for minor adjustments. If the
//serialized entities ever become sparse though, this should likely be replaced with a sorted iteration over the field, rather than this sparse lookup, but for now this is significantly faster. This will return -1 if
//the field is not in the entity
static inline int32 GetFieldIndex( const short* RESTRICT pFieldStart, const short* RESTRICT pFieldEnd, short nDesiredField )
{
	//determine our starting pointer (likely to be the field itself)
	const short* pCurr = MIN( pFieldEnd - 1, pFieldStart + nDesiredField );
	
	//check for a direct hit (most common case)
	if( *pCurr == nDesiredField )
		return (pCurr - pFieldStart);

	//determine the direction that we need to head
	if( *pCurr < nDesiredField )
	{
		//advance forward looking for a match (or when our numbers become larger than our target)
		for( pCurr++; ( pCurr < pFieldEnd ) && ( *pCurr <= nDesiredField ) ; pCurr++ )
		{
			if( *pCurr == nDesiredField )
				return (pCurr - pFieldStart);
		}
	}
	else
	{
		//move backwards (or when our numbers become smaller than our target)
		for( pCurr--; ( pCurr >= pFieldStart ) && ( *pCurr >= nDesiredField ) ; pCurr-- )
		{
			if( *pCurr == nDesiredField )
				return (pCurr - pFieldStart);
		}
	}

	//no match, hit the end of the list
	return -1;
}

//given an entity that has had a partial change, this will take the previous data block, and splice in the new values, setting new change fields to the specified tick for ones that have actually changed. This will
//return false if a delta table is not applicable and instead a full pack needs to be done
SerializedEntityHandle_t SendTable_BuildDeltaProperties( edict_t *pEdict, int nObjectID, const SendTable* pTable, SerializedEntityHandle_t oldProps, CalcDeltaResultsList_t &deltaProps, CUtlMemory<CSendProxyRecipients> *pRecipients, bool& bCanReuseOldData )
{
	const CSerializedEntity* RESTRICT pOldProps = ( const CSerializedEntity* )oldProps;
	
	//assume that we can't reuse the old frame for now
	bCanReuseOldData = false;

	// Setup all the info we'll be walking the tree with.
	CServerDatatableStack info( pTable->m_pPrecalc, (unsigned char*)pEdict->GetUnknown(), nObjectID );
	info.m_pRecipients = pRecipients;	// optional buffer to store the bits for which clients get what data.
	info.Init( false, false );

	//get the change list associated with this object
	const CEdictChangeInfo * RESTRICT pCI = &g_pSharedChangeInfo->m_ChangeInfos[ pEdict->GetChangeInfo() ];

	//cache our indices for our fields
	const uint32 nOldProps = pOldProps->GetFieldCount();
	const short* RESTRICT pFieldListStart	= pOldProps->GetFieldPaths();
	const short* RESTRICT pFieldListEnd		= pFieldListStart + nOldProps;
	
	//now copy over the old property block into our new buffer
	uint8 packedData[MAX_PACKEDENTITY_DATA];
	memcpy( packedData, pOldProps->GetFieldData(), Bits2Bytes( pOldProps->GetFieldDataBitCount() ) );
	bf_write fieldDataBuf( "BuildDeltaProperties fieldDataBuf", packedData, sizeof( packedData ) );

	//we now need to build a list of the properties that have changed, converting them over to indices, and sorting them
	for( uint32 nCurrOffset = 0; nCurrOffset < (uint32)pCI->m_nChangeOffsets; ++nCurrOffset )
	{
		int propOffsets[MAX_PROP_INDEX_OFFSETS];
		int numPropsMatchingIndex = pTable->m_pPrecalc->GetPropertyIndexFromOffset( pCI->m_ChangeOffsets[ nCurrOffset ], propOffsets );

		if ( !numPropsMatchingIndex )
		{
			continue;
		}

		for ( int propOffsetIndex = 0; propOffsetIndex < numPropsMatchingIndex; ++propOffsetIndex )
		{
			int nPropField = propOffsets[propOffsetIndex];

			//we can safely skip any properties that are not considered valid by the proxy
			if( !info.IsPropProxyValid( nPropField ) )
				continue;

			//we now need to map this property field to an index in our serialized entity
			int nFieldIndex = GetFieldIndex( pFieldListStart, pFieldListEnd, ( short )nPropField );
			//if we didn't find a match here, we have a problem of mismatch between the blocks and we need to fall back to the slow route
			if( nFieldIndex == -1 )
				return SERIALIZED_ENTITY_HANDLE_INVALID;

			//we have a match, so we need to splice this new property onto our original property
			const uint32 nPropBitStart = pOldProps->GetFieldDataBitOffset( nFieldIndex );
			const uint32 nPropBitEnd   = pOldProps->GetFieldDataBitEndOffset( nFieldIndex );
			fieldDataBuf.SeekToBit( nPropBitStart );
			info.SeekToProp( nPropField );

			const SendProp* RESTRICT pProp = pTable->m_pPrecalc->GetProp( nPropField );

			// Call their proxy to get the property's value.
			DVariant var;
			pProp->GetProxyFn()( 
				pProp, info.GetCurStructBase(), info.GetCurStructBase() + pProp->GetOffset(), 
				&var, 0, info.GetObjectID()	);

			//now write it out
			g_PropTypeFns[pProp->m_Type].Encode( 
				info.GetCurStructBase(), &var, pProp, 
				&fieldDataBuf, info.GetObjectID()	); 

			//handle if the property changed in size (does this happen? If so, we just need to fall back to the slow, slow route, and should return NULL)
			if( nPropBitEnd != ( uint32 )fieldDataBuf.GetNumBitsWritten() )
				return SERIALIZED_ENTITY_HANDLE_INVALID;

			//we now need to compare if we actually changed values between these bits
			if( !DoesBitRangeMatch( ( const uint32* )pOldProps->GetFieldData(), ( const uint32* )packedData, nPropBitStart, nPropBitEnd ) )
				deltaProps.AddToTail( nPropField );
		}
	}

	//if we have no changes, we can just reuse the old one, don't bother making a copy
	if( deltaProps.Count() == 0 )
	{
		bCanReuseOldData = true;
		return SERIALIZED_ENTITY_HANDLE_INVALID;
	}

	//at this point, we have all of our new data. So setup our new serialized version to return
	CSerializedEntity* RESTRICT pNewProps = (CSerializedEntity*)g_pSerializedEntities->AllocateSerializedEntity(__FILE__, __LINE__);
	pNewProps->SetupPackMemory( nOldProps, pOldProps->GetFieldDataBitCount() );

	//and blit on over
	memcpy( pNewProps->GetFieldPaths(), pOldProps->GetFieldPaths(), sizeof( uint16 ) * nOldProps );
	memcpy( pNewProps->GetFieldDataBitOffsets(), pOldProps->GetFieldDataBitOffsets(), sizeof( uint32 ) * nOldProps );
	memcpy( pNewProps->GetFieldData(), packedData, Bits2Bytes( pNewProps->GetFieldDataBitCount() ) );

	return ( SerializedEntityHandle_t )pNewProps;
}

static void BuildNonZeroDelta( const SendTable *pTable,
								const CSerializedEntity *pToEntity,
								const int objectID,
								CalcDeltaResultsList_t &deltaProps )
{
	//setup a reader for this data
	bf_read ToBits;
	pToEntity->StartReading( ToBits );

	//just run through each source property, and for each one that is non-zero, add it to the delta props
	const uint32 nNumProps = (uint32)pToEntity->GetFieldCount();
	for( uint32 nCurrProp = 0; nCurrProp < nNumProps; ++nCurrProp )
	{
		CFieldPath nPath = pToEntity->GetFieldPath( nCurrProp );
		ToBits.Seek(pToEntity->GetFieldDataBitOffset( nCurrProp ) );

		const SendProp *pProp = pTable->m_pPrecalc->GetProp( nPath );
		if ( !g_PropTypeFns[pProp->m_Type].IsEncodedZero( pProp, &ToBits ) )
		{
			// add non-zero property to index list
			deltaProps.AddToTail( nPath );
		}
	}
}

//given data, this will continue to advance the pointers so long as from = to, and will update the necessary values
static FORCEINLINE void AdvanceToNextChange( const uint32* RESTRICT pFrom, const uint32* RESTRICT pTo, uint32 nMaxInts, uint32* RESTRICT pnIntOffset, uint32* RESTRICT pnDataDiff, uint32* RESTRICT pnDataStartBit )
{
	const uint32* RESTRICT pEndFrom	= pFrom + nMaxInts;
	const uint32* RESTRICT pCurrFrom = pFrom + *pnIntOffset;
	const uint32* RESTRICT pCurrTo	= pTo + *pnIntOffset;

	for( ; pCurrFrom != pEndFrom; ++pCurrFrom, ++pCurrTo )
	{
		if( *pCurrFrom != *pCurrTo )
		{
#if defined(_PS3) || defined(_X360)
			*pnDataDiff = DWordSwap((*pCurrFrom ^ *pCurrTo));
#else
            *pnDataDiff = *pCurrFrom ^ *pCurrTo;
#endif
			break;
		}
	}

	*pnIntOffset	= ( pCurrFrom - pFrom );
	*pnDataStartBit = ( pCurrFrom - pFrom ) * 32;
}

//given a starting bit index, the dirty bit window, and the range that the property covers, this will determine if this property overlaps the dirty bit
static FORCEINLINE bool DoesPropertyOverlapDirtyBit( uint32 nDataStartBit, uint32 nFieldStartBit, uint32 nFieldEndBit, uint32 nDataDiff )
{
	//Note, we assume that any properties that share this integer with us have been tested prior, and as a result, also cleared out the data diff
	//when they checked so we do not need to mask off the lower bound
	Assert( nFieldEndBit > nDataStartBit );
	Assert( nDataStartBit + 32 > nFieldStartBit );

	//see if this integer spans the boundary where we end, if so, we need to only check the area that we cover
	uint32 nDataEndBit = nDataStartBit + 32;
	if( nFieldEndBit < nDataEndBit )
	{
		uint32 nEndBits = ( nDataEndBit - nFieldEndBit );
		Assert( nEndBits < 32 );
		uint32 nEndMask = ( 0xFFFFFFFF >> nEndBits );
		nDataDiff &= nEndMask;
	}

	return ( nDataDiff != 0 );
}

//this is a fast path that will handle scanning through the properties, identifying differences in the values. This will build a list of properties that
//have differing values, and if it encounters a misalignment between the two property lists (like a property added, removed, or changed in size) it will
//stop processing and return the property index it had to stop on so that the slower unaligned path can be used from that point on
static inline uint32 FastPathCalcDelta( const CSerializedEntity *pFromEntity, const CSerializedEntity *pToEntity, CalcDeltaResultsList_t &deltaProps )
{
	//cache common data
	const uint32 nFromFieldCount	= pFromEntity->GetFieldCount();
	const uint32 nToFieldCount		= pToEntity->GetFieldCount();

	const uint32* RESTRICT pFromData = (const uint32*)pFromEntity->GetFieldData();
	const uint32* RESTRICT pToData	= (const uint32*)pToEntity->GetFieldData();

	//determine how many integers we have in our buffer
	const uint32 nEndDataInts = ( MIN( pToEntity->GetFieldDataBitCount(), pFromEntity->GetFieldDataBitCount() ) + 31 ) / 32;

	//scan through our data to find the first area where we detect a change
	uint32 nIntOffset = 0;
	uint32 nDataDiff = 0;
	uint32 nDataStartBit = 0;
	AdvanceToNextChange( pFromData, pToData, nEndDataInts, &nIntOffset, &nDataDiff, &nDataStartBit );

	//handle a special case where all the data was the same
	if( ( nIntOffset == nEndDataInts ) && ( nFromFieldCount == nToFieldCount ) && ( pToEntity->GetFieldDataBitCount() == pFromEntity->GetFieldDataBitCount() ) )
	{
		//odds are very good that we have the same property layout, so just do an entire comparison of the values
		if( ( V_memcmp( pFromEntity->GetFieldDataBitOffsets(), pToEntity->GetFieldDataBitOffsets(), nFromFieldCount * sizeof( uint32 ) ) == 0 ) && 
			( V_memcmp( pFromEntity->GetFieldPaths(), pToEntity->GetFieldPaths(), nFromFieldCount * sizeof( short ) ) == 0 ) )
		{
			return nFromFieldCount;
		}
		//the field layout wasn't the same. VERY rare, fall through and let the function below catch where
	}

	//determine how many overlapping fields we can test against
	uint32 nMinFieldOverlap = MIN( nFromFieldCount, nToFieldCount );
	if( nMinFieldOverlap == 0 )
		return 0;

	//pointers to each of the data blocks we are reading from
	const short* RESTRICT pFromPath		= pFromEntity->GetFieldPaths();
	const short* RESTRICT pToPath		= pToEntity->GetFieldPaths();
	//since we want the END of the properties for most operations
	const uint32* RESTRICT pFromStartOffset	= pFromEntity->GetFieldDataBitOffsets();
	const uint32* RESTRICT pFromEndOffset	= pFromEntity->GetFieldDataBitOffsets() + 1;
	const uint32* RESTRICT pToEndOffset		= pToEntity->GetFieldDataBitOffsets() + 1;


	//run through all the properties...except the last one. The reason for this is because we don't want to have to range check on all of our property bit offsets
	uint32 nLastField = nMinFieldOverlap - 1;
	for(uint32 nCurrField = 0 ; nCurrField < nLastField; ++nCurrField )
	{
		//extract information about our fields to make sure that they match
		const CFieldPath nFromPath	= pFromPath[ nCurrField ];
		const CFieldPath nToPath	= pToPath[ nCurrField ];
		const uint32 nFromEnd		= pFromEndOffset[ nCurrField ];
		const uint32 nToEnd			= pToEndOffset[ nCurrField ];
		
		//we can do the fast path so long as the field indices match (no dropped properties) and the offsets are the same (which creates unaligned properties)
		if( ( nFromPath != nToPath ) || ( nFromEnd != nToEnd ) ) 
			return nCurrField;

		//see if we have completely clear data already (we have clear data already past where we end)
		if( nDataStartBit >= nFromEnd )
			continue;

		//we can now tell if we have a dirty prop
		if( DoesPropertyOverlapDirtyBit( nDataStartBit, pFromStartOffset[ nCurrField ], nFromEnd, nDataDiff ) )
			deltaProps.AddToTail( nFromPath );
				
		//and handle reading to the next diff which is either at the end of our property, or the next integer if we have no more dirty bits in this one
		uint32 nPropEndInt = nFromEnd / 32;
		if( nPropEndInt > nIntOffset )
		{
			//we have advanced past our previous buffer, so scan to our next change
			nIntOffset = nPropEndInt;
			AdvanceToNextChange( pFromData, pToData, nEndDataInts, &nIntOffset, &nDataDiff, &nDataStartBit );
		}
		
		//the property may have dirty data in its last section, so see if we overlap, if so, clear our portion, and see if we need to continue the scan
		if( nIntOffset == nPropEndInt )
		{
			//clear any portion of this integer that we overlap with
			uint32 nOverlapBits = nFromEnd - nDataStartBit;
			Assert( nOverlapBits < 32 );
			uint32 nKeepMask	= 0xFFFFFFFFF << nOverlapBits;
			nDataDiff &= nKeepMask;

			//and if we have no more dirty data in the integer, we need to scan to the next change
			if( nDataDiff == 0 )
			{
				nIntOffset++;
				AdvanceToNextChange( pFromData, pToData, nEndDataInts, &nIntOffset, &nDataDiff, &nDataStartBit );
			}		
		}
	}

	//and test our last one
	{
		//extract information about our fields to make sure that they match
		const CFieldPath nFromPath	= pFromPath[ nLastField ];
		const CFieldPath nToPath	= pToPath[ nLastField ];
		const uint32 nFromEnd		= pFromEntity->GetFieldDataBitCount();
		const uint32 nToEnd			= pToEntity->GetFieldDataBitCount();

		//we can do the fast path so long as the field indices match (no dropped properties) and the offsets are the same (which creates unaligned properties)
		if( ( nFromPath != nToPath ) || ( nFromEnd != nToEnd ) ) 
			return nLastField;

		//see if we have completely clear data already (we have clear data already past where we end)
		if( nDataStartBit < nFromEnd )
		{
			//we can now tell if we have a dirty prop
			if( DoesPropertyOverlapDirtyBit( nDataStartBit, pFromStartOffset[ nLastField ], nFromEnd, nDataDiff ) )
				deltaProps.AddToTail( nFromPath );
		}
	}

	//let the caller know how many properties we took care of
	return nMinFieldOverlap;
}

//this will handle further delta detection on the properties using a much slower approach to handle misalignments
static void SlowPathCompareProps( const CSerializedEntity *pFromEntity, const CSerializedEntity *pToEntity, uint32 nStartField, CalcDeltaResultsList_t &deltaProps )
{
	//run through each of our target properties that we have left
	const uint32 nNumToFields	= ( uint32 )pToEntity->GetFieldCount();
	const uint32 nNumFromFields = ( uint32 )pFromEntity->GetFieldCount();
	
	uint32 nCurrFromField = nStartField;

	bf_read ToData, FromData;
	pToEntity->StartReading( ToData );
	pFromEntity->StartReading( FromData );

	for( uint32 nCurrToField = nStartField; nCurrToField < nNumToFields; ++nCurrToField )
	{
		//get our path for our target property
		const CFieldPath nToPath = pToEntity->GetFieldPath( nCurrToField );

		//find a possible match in our from path
		for( ; ( nCurrFromField < nNumFromFields ) && ( pFromEntity->GetFieldPath( nCurrFromField ) < nToPath ) ;  nCurrFromField++ )
		{
		}

		//handle the case where this is a new property
		if( ( nCurrFromField < nNumFromFields ) && ( pFromEntity->GetFieldPath( nCurrFromField ) == nToPath ) )
		{
			//position our buffers to the start of each property
			uint32 nToDataStartBit		= pToEntity->GetFieldDataBitOffset( nCurrToField );
			uint32 nFromDataStartBit	= pFromEntity->GetFieldDataBitOffset( nCurrFromField );
			uint32 nToDataSize			= pToEntity->GetFieldDataBitEndOffset( nCurrToField ) - nToDataStartBit;
			uint32 nFromDataSize		= pFromEntity->GetFieldDataBitEndOffset( nCurrFromField ) - nFromDataStartBit;

			ToData.Seek( nToDataStartBit );
			FromData.Seek( nFromDataStartBit );
						
			if( ( nToDataSize != nFromDataSize ) || !CompareBits( FromData, ToData, nToDataSize ) )
			{
				deltaProps.AddToTail( nToPath );
			}
		}
		else
		{
			//this is a new property that we need to encode
			deltaProps.AddToTail( nToPath );
		}
	}
}

void SendTable_CalcDelta(
	const SendTable *pTable,
	SerializedEntityHandle_t fromEntity, // can be null
	SerializedEntityHandle_t toEntity,
	const int objectID,
	CalcDeltaResultsList_t &deltaProps )
{
	const CSerializedEntity *pFromEntity = (CSerializedEntity *)fromEntity;
	const CSerializedEntity *pToEntity = (CSerializedEntity *)toEntity;

	//if we don't have a from entity, the problem domain is to just go through the current entity and build a change list of which properties are non-zero
	if( pFromEntity == NULL )
	{
		BuildNonZeroDelta( pTable, pToEntity, objectID, deltaProps );
		return;
	}

	//fast path compare as many properties as we can, this handles the vast majority of properties which only have value changes
	uint32 nFastPathProps = FastPathCalcDelta( pFromEntity, pToEntity, deltaProps );

	//see if we have any properties not yet checked, meaning we had differences in the property layout
	if( nFastPathProps < ( uint32 )pToEntity->GetFieldCount() )
	{	
		//now we need to fall back to the slow path for the remaining un-aligned data, this will either have zero iterations (made it all the way through with fast properties),
		//or will cover all the properties after a divergence
		SlowPathCompareProps( pFromEntity, pToEntity, nFastPathProps, deltaProps );
	}
}

bool SendTable_WriteInfos( const SendTable *pTable, bf_write& bfWrite, bool bNeedsDecoder, bool bIsEnd )
{
	CSVCMsg_SendTable_t msg;

	if( !pTable || bIsEnd )
	{
		// Write the end bit to signal no more send tables
		Assert( !pTable && bIsEnd );
		msg.set_is_end( true );
		return msg.WriteToBuffer( bfWrite );
	}

	if ( bNeedsDecoder ) // only set if true, let the default false
	{
		msg.set_needs_decoder( bNeedsDecoder );
	}

	msg.set_net_table_name( pTable->GetName() );

	// Send each property.
	for ( int iProp=0; iProp < pTable->m_nProps; iProp++ )
	{
		const SendProp *pProp = &pTable->m_pProps[iProp];
		CSVCMsg_SendTable::sendprop_t *pSendProp = msg.add_props();

		pSendProp->set_type( pProp->m_Type );
		pSendProp->set_var_name( pProp->GetName() );
		// we now have some flags that aren't networked so strip them off
		unsigned int networkFlags = pProp->GetFlags() & ((1<<SPROP_NUMFLAGBITS_NETWORKED)-1);
		pSendProp->set_flags( networkFlags );
		pSendProp->set_priority( pProp->GetPriority() );

		if( pProp->m_Type == DPT_DataTable )
		{
			// Just write the name and it will be able to reuse the table with a matching name.
			pSendProp->set_dt_name( pProp->GetDataTable()->m_pNetTableName );
		}
		else if ( pProp->IsExcludeProp() )
		{
			pSendProp->set_dt_name( pProp->GetExcludeDTName() );
		}
		else if ( pProp->GetType() == DPT_Array )
		{
			pSendProp->set_num_elements( pProp->GetNumElements() );
		}
		else
		{			
			pSendProp->set_low_value( pProp->m_fLowValue );
			pSendProp->set_high_value( pProp->m_fHighValue );
			pSendProp->set_num_bits( pProp->m_nBits );
		}
	}

	return msg.WriteToBuffer( bfWrite );
}

// Spits out warnings for invalid properties and forces property values to
// be in valid ranges for the encoders and decoders.
static void SendTable_Validate( CSendTablePrecalc *pPrecalc )
{
	SendTable *pTable = pPrecalc->m_pSendTable;
	for( int i=0; i < pTable->m_nProps; i++ )
	{
		SendProp *pProp = &pTable->m_pProps[i];
		
		if ( pProp->GetArrayProp() )
		{
			if ( pProp->GetArrayProp()->GetType() == DPT_DataTable )
			{
				Error( "Invalid property: %s/%s (array of datatables) [on prop %d of %d (%s)].", pTable->m_pNetTableName, pProp->GetName(), i, pTable->m_nProps, pProp->GetArrayProp()->GetName() );
			}
		}
		else
		{
			ErrorIfNot( pProp->GetNumElements() == 1, ("Prop %s/%s has an invalid element count for a non-array.", pTable->m_pNetTableName, pProp->GetName()) );
		}
			
		// Check for 1-bit signed properties (their value doesn't get down to the client).
		if ( pProp->m_nBits == 1 && !(pProp->GetFlags() & SPROP_UNSIGNED) )
		{
			DataTable_Warning("SendTable prop %s::%s is a 1-bit signed property. Use SPROP_UNSIGNED or the client will never receive a value.\n", pTable->m_pNetTableName, pProp->GetName());
		}
	}

	for ( int i = 0; i < pPrecalc->GetNumProps(); ++i )
	{
		const SendProp *pProp = pPrecalc->GetProp( i );
		if ( pProp->GetFlags() & SPROP_ENCODED_AGAINST_TICKCOUNT )
		{
			pTable->SetHasPropsEncodedAgainstTickcount( true );
			break;
		}
	}
}


static void SendTable_CalcNextVectorElems( SendTable *pTable )
{
	for ( int i=0; i < pTable->GetNumProps(); i++ )
	{
		SendProp *pProp = pTable->GetProp( i );
		
		if ( pProp->GetType() == DPT_DataTable )
		{
			SendTable_CalcNextVectorElems( pProp->GetDataTable() );
		}
		else if ( pProp->GetOffset() < 0 )
		{
			pProp->SetOffset( -pProp->GetOffset() );
			pProp->SetFlags( pProp->GetFlags() | SPROP_IS_A_VECTOR_ELEM );
		}
	}
}

// This helps us figure out which properties can use the super-optimized mode
// where they are tracked in a list when they change. If their m_pProxies pointers
// are set to 1, then it means that this property is gotten to by means of SendProxy_DataTableToDataTable.
// If it's set to 0, then we can't directly take the property's offset.
class CPropOffsetStack : public CDatatableStack
{
public:
	CPropOffsetStack( CSendTablePrecalc *pPrecalc ) :
		CDatatableStack( pPrecalc, NULL, -1 )
	{
		m_pPropMapStackPrecalc = pPrecalc;
	}

	inline unsigned char*	CallPropProxy( CSendNode *pNode, int iProp, unsigned char *pStructBase )
	{
		const SendProp *pProp = m_pPropMapStackPrecalc->GetDatatableProp( iProp );
		return pStructBase + pProp->GetOffset();


	}

	virtual void RecurseAndCallProxies( CSendNode *pNode, unsigned char *pStructBase )
	{
		// Remember where the game code pointed us for this datatable's data so 
		m_pProxies[ pNode->GetRecursiveProxyIndex() ] = pStructBase;

		for ( int iChild=0; iChild < pNode->GetNumChildren(); iChild++ )
		{
			CSendNode *pCurChild = pNode->GetChild( iChild );
			
			unsigned char *pNewStructBase = CallPropProxy( pCurChild, pCurChild->m_iDatatableProp, pStructBase );

			RecurseAndCallProxies( pCurChild, pNewStructBase );
		}
	}

public:
	CSendTablePrecalc *m_pPropMapStackPrecalc;
};

static void BuildPropOffsetToIndexMap( CSendTablePrecalc *pPrecalc )
{
	CPropOffsetStack pmStack( pPrecalc );
	pmStack.Init( false, false );

	const int nNumProps = pPrecalc->m_Props.Count();
	
	pPrecalc->m_PropOffsetToIndex.SetSize( nNumProps );

	for ( int i=0; i < nNumProps; i++ )
	{
		pmStack.SeekToProp( i );

		const SendProp *pProp = pPrecalc->m_Props[i];
		int offset = size_cast<int>( pProp->GetOffset() + (intp)pmStack.GetCurStructBase() );

		pPrecalc->m_PropOffsetToIndex[ i ].m_nIndex		= i;
		pPrecalc->m_PropOffsetToIndex[ i ].m_nOffset	= offset;
	}

	//now sort our list to ensure it is efficient for lookup
	std::sort( pPrecalc->m_PropOffsetToIndex.Base(), pPrecalc->m_PropOffsetToIndex.Base() + pPrecalc->m_PropOffsetToIndex.Count() );
}

static bool SendTable_InitTable( SendTable *pTable )
{
	if( pTable->m_pPrecalc )
		return true;
	
	// Create the CSendTablePrecalc.	
	CSendTablePrecalc *pPrecalc = new CSendTablePrecalc;
	pTable->m_pPrecalc = pPrecalc;

	pPrecalc->m_pSendTable = pTable;
	pTable->m_pPrecalc = pPrecalc;

	SendTable_CalcNextVectorElems( pTable );

	// Bind the instrumentation if -dti was specified.
	pPrecalc->m_pDTITable = ServerDTI_HookTable( pTable );

	// Setup its flat property array.
	if ( !pPrecalc->SetupFlatPropertyArray() )
		return false;

	BuildPropOffsetToIndexMap( pPrecalc );

	SendTable_Validate( pPrecalc );
	return true;
}


static void SendTable_TermTable( SendTable *pTable )
{
	if( !pTable->m_pPrecalc )
		return;

	delete pTable->m_pPrecalc;
	Assert( !pTable->m_pPrecalc ); // Make sure it unbound itself.
}


int SendTable_GetNumFlatProps( const SendTable *pSendTable )
{
	const CSendTablePrecalc *pPrecalc = pSendTable->m_pPrecalc;
	ErrorIfNot( pPrecalc,
		("SendTable_GetNumFlatProps: missing pPrecalc.")
	);
	return pPrecalc->GetNumProps();
}

CRC32_t SendTable_CRCTable( CRC32_t &crc, SendTable *pTable )
{
	CRC32_ProcessBuffer( &crc, (void *)pTable->m_pNetTableName, Q_strlen( pTable->m_pNetTableName) );
	CRCMSG( "Table: %s\n", pTable->m_pNetTableName );

	int nProps = LittleLong( pTable->m_nProps );
	CRC32_ProcessBuffer( &crc, (void *)&nProps, sizeof( pTable->m_nProps ) );
	CRCMSG( "  nProps: " CRCFMT4 "\n", CRCMEM4( nProps ) );

	// Send each property.
	for ( int iProp=0; iProp < pTable->m_nProps; iProp++ )
	{
		const SendProp *pProp = &pTable->m_pProps[iProp];

		int type = LittleLong( pProp->m_Type );
		CRC32_ProcessBuffer( &crc, (void *)&type, sizeof( type ) );
		CRC32_ProcessBuffer( &crc, (void *)pProp->GetName() , Q_strlen( pProp->GetName() ) );

		int flags = LittleLong( pProp->GetFlags() );
		CRC32_ProcessBuffer( &crc, (void *)&flags, sizeof( flags ) );

		CRCMSG( "  %d. %s : type = " CRCFMT4 ", flags = " CRCFMT4 ", bits = %d\n",
			iProp, pProp->GetName(),
			CRCMEM4( type ), CRCMEM4( flags ), pProp->m_nBits );

		if( pProp->m_Type == DPT_DataTable )
		{
			CRC32_ProcessBuffer( &crc, (void *)pProp->GetDataTable()->m_pNetTableName, Q_strlen( pProp->GetDataTable()->m_pNetTableName ) );
			CRCMSG( "      DPT_DataTable: %s\n", pProp->GetDataTable()->m_pNetTableName );
			
			if ( SendProp *pArrayPropInside = pProp->GetArrayProp() )
			{
				int arPropType = pArrayPropInside->m_Type;
				int arPropFlags = pArrayPropInside->GetFlags();
				int arElementsCount = pProp->GetDataTable()->m_nProps;
				CRC32_ProcessBuffer( &crc, (void *)&arPropType, sizeof( arPropType ) );
				CRC32_ProcessBuffer( &crc, (void *)&arPropFlags, sizeof( arPropFlags ) );
				CRCMSG( "      ArrayPropData: type = " CRCFMT4 ", flags = " CRCFMT4 ", bits = %d, size = %d\n",
					CRCMEM4( arPropType ), CRCMEM4( arPropFlags ), pArrayPropInside->m_nBits, arElementsCount );
			}
			#if CRC_SEND_TABLE_VERBOSE
			else if ( StringHasPrefixCaseSensitive( pProp->GetDataTable()->m_pNetTableName, "_ST_" ) )
			{
				SendProp *pArrayPropInside = pProp->GetDataTable()->GetProp( 1 );
				int arPropType = pArrayPropInside->m_Type;
				int arPropFlags = pArrayPropInside->GetFlags();
				int arElementsCount = pProp->GetDataTable()->m_nProps - 1; // first is length proxy
				CRC32_ProcessBuffer( &crc, ( void * ) &arPropType, sizeof( arPropType ) );
				CRC32_ProcessBuffer( &crc, ( void * ) &arPropFlags, sizeof( arPropFlags ) );
				CRCMSG( "      UtlVectorData: type = " CRCFMT4 ", flags = " CRCFMT4 ", bits = %d, size = %d\n",
					CRCMEM4( arPropType ), CRCMEM4( arPropFlags ), pArrayPropInside->m_nBits, arElementsCount );
			}
			else
			{
				SendTable *pSendTableReferenced = pProp->GetDataTable();
				if ( ( g_SendTables.Find( pSendTableReferenced ) == g_SendTables.InvalidIndex() )
					&& ( g_SendTablesReferenced.Find( pSendTableReferenced ) == g_SendTablesReferenced.InvalidIndex() ) )
					g_SendTablesReferenced.AddToTail( pSendTableReferenced );
			}
			#endif
		}
		else
		{
			if ( pProp->IsExcludeProp() )
			{
				CRC32_ProcessBuffer( &crc, (void *)pProp->GetExcludeDTName(), Q_strlen( pProp->GetExcludeDTName() ) );
				CRCMSG( "      ExcludeProp: %s\n", pProp->GetExcludeDTName() );
			}
			else if ( pProp->GetType() == DPT_Array )
			{
				int numelements = LittleLong( pProp->GetNumElements() );
				CRC32_ProcessBuffer( &crc, (void *)&numelements, sizeof( numelements ) );
				CRCMSG( "      DPT_Array: " CRCFMT4 "\n", CRCMEM4( numelements ) );
			}
			else
			{	
				float lowvalue;
				LittleFloat( &lowvalue, &pProp->m_fLowValue );
				CRC32_ProcessBuffer( &crc, (void *)&lowvalue, sizeof( lowvalue ) );

				float highvalue;
				LittleFloat( &highvalue, &pProp->m_fHighValue );
				CRC32_ProcessBuffer( &crc, (void *)&highvalue, sizeof( highvalue ) );

				int	bits = LittleLong( pProp->m_nBits );
				CRC32_ProcessBuffer( &crc, (void *)&bits, sizeof( bits ) );

				CRCMSG( "      DPT: lv " CRCFMT4 ", hv " CRCFMT4 ", bits " CRCFMT4 "\n",
					CRCMEM4( lowvalue ), CRCMEM4( highvalue ), CRCMEM4( bits ) );
			}
		}
	}

	return crc;
}

void SendTable_PrintStats( void )
{
	int numTables = 0;
	int numFloats = 0;
	int numStrings = 0;
	int numArrays = 0;
	int numInts = 0;
	int numVecs = 0;
	int numVecXYs = 0;
	int numSubTables = 0;
	int numSendProps = 0;
	int numFlatProps = 0;
	int numExcludeProps = 0;

	for ( int i=0; i < g_SendTables.Count(); i++ )
	{
		SendTable *st =  g_SendTables[i];
		
		numTables++;
		numSendProps += st->GetNumProps();
		numFlatProps += st->m_pPrecalc->GetNumProps();

		for ( int j=0; j < st->GetNumProps(); j++ )
		{
			SendProp* sp = st->GetProp( j );

			if ( sp->IsExcludeProp() )
			{
				numExcludeProps++;
				continue; // no real sendprops
			}

			if ( sp->IsInsideArray() )
				continue;

			switch( sp->GetType() )
			{
				case DPT_Int	: numInts++; break;
				case DPT_Float	: numFloats++; break;
				case DPT_Vector : numVecs++; break;
				case DPT_VectorXY : numVecXYs++; break;
				case DPT_String : numStrings++; break;
				case DPT_Array	: numArrays++; break;
				case DPT_DataTable : numSubTables++; break;
			}
		}
	}

	Msg("Total Send Table stats\n");
	Msg("Send Tables   : %i\n", numTables );
	Msg("Send Props    : %i\n", numSendProps );
	Msg("Flat Props    : %i\n", numFlatProps );
	Msg("Int Props     : %i\n", numInts );
	Msg("Float Props   : %i\n", numFloats );
	Msg("Vector Props  : %i\n", numVecs );
	Msg("VectorXY Props: %i\n", numVecXYs );
	Msg("String Props  : %i\n", numStrings );
	Msg("Array Props   : %i\n", numArrays );
	Msg("Table Props   : %i\n", numSubTables );
	Msg("Exclu Props   : %i\n", numExcludeProps );
}



bool SendTable_Init( SendTable **pTables, int nTables )
{
	ErrorIfNot( g_SendTables.Count() == 0,
		("SendTable_Init: called twice.")
	);

	bool bFullSendTableInfo = false;
	if ( !IsRetail() )
	{
		bFullSendTableInfo = ( CommandLine()->FindParm("-dtix" ) != 0 );
	}

	// Initialize them all.
	for ( int i=0; i < nTables; i++ )
	{
		if ( !SendTable_InitTable( pTables[i] ) )
			return false;

		if ( bFullSendTableInfo && pTables[i] )
			Msg( "SendTable[%03d] = %s\n", i, pTables[i]->GetName() );
	}

	// Store off the SendTable list.
	g_SendTables.CopyArray( pTables, nTables );

	g_SendTableCRC = SendTable_ComputeCRC( );

	if ( CommandLine()->FindParm("-dti" ) )
	{
		SendTable_PrintStats();
	}

	return true;	
}
void SendTable_Term()
{
	// Term all the SendTables.
	for ( int i=0; i < g_SendTables.Count(); i++ )
		SendTable_TermTable( g_SendTables[i] );

	// Clear the list of SendTables.
	g_SendTables.Purge();
	g_SendTableCRC = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Computes the crc for all sendtables for the data sent in the class/table definitions
// Output : CRC32_t
//-----------------------------------------------------------------------------
CRC32_t SendTable_ComputeCRC()
{
	CRCMSG( "-----------------------------------------------------\n" );
	CRCMSG( "----------------SendTable_ComputeCRC-----------------\n" );
	CRCMSG( "-----------------------------------------------------\n" );

	CRC32_t result;
	CRC32_Init( &result );

	// walk the tables and checksum them
	int c = g_SendTables.Count();
	for ( int i = 0 ; i < c; i++ )
	{
		SendTable *st = g_SendTables[ i ];
		result = SendTable_CRCTable( result, st );
	}

#if CRC_SEND_TABLE_VERBOSE
	for ( int i = 0; i < g_SendTablesReferenced.Count(); i++ )
	{
		SendTable *st = g_SendTablesReferenced[ i ];
		result = SendTable_CRCTable( result, st );
	}
#endif

	CRC32_Final( &result );
	
	CRCMSG( "-----------------------------------------------------\n" );
	CRCMSG( "-------------------CRC=%08X----------------------\n", result );
	CRCMSG( "-----------------------------------------------------\n" );

	return result;
}

SendTable *SendTabe_GetTable(int index)
{
	return  g_SendTables[index];
}

int	SendTable_GetNum()
{
	return g_SendTables.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CRC32_t
//-----------------------------------------------------------------------------
CRC32_t SendTable_GetCRC()
{
	return g_SendTableCRC;
}


//-----------------------------------------------------------------------------
// Purpose: check integrity of an unpacked entity send table
//-----------------------------------------------------------------------------
bool SendTable_CheckIntegrity( SendTable *pTable, const void *pData, const int nDataBits )
{
#ifdef _DEBUG
	if ( pData == NULL && nDataBits == 0 )
		return true;

	bf_read bfRead(	"SendTable_CheckIntegrity", pData, Bits2Bytes(nDataBits), nDataBits );
	CDeltaBitsReader bitsReader( &bfRead );

	int iProp = PROP_SENTINEL;
	int iLastProp = -1;
	int nMaxProps = pTable->m_pPrecalc->GetNumProps();
	int nPropCount = 0;

	Assert( nMaxProps > 0 && nMaxProps < MAX_DATATABLE_PROPS );

	while( -1 != (iProp = bitsReader.ReadNextPropIndex()) )
	{
		Assert( (iProp>=0) && (iProp<nMaxProps) );

		// must be larger
		Assert( iProp > iLastProp );

		const SendProp *pProp = pTable->m_pPrecalc->GetProp( iProp );

		Assert( pProp );

		// ok check that SkipProp & IsEncodedZero read the same bit length
		int iStartBit = bfRead.GetNumBitsRead();
		g_PropTypeFns[ pProp->GetType() ].SkipProp( pProp, &bfRead );
		int nLength = bfRead.GetNumBitsRead() - iStartBit;

		Assert( nLength > 0 ); // a prop must have some bits

		bfRead.Seek( iStartBit ); // read it again

		g_PropTypeFns[ pProp->GetType() ].IsEncodedZero( pProp, &bfRead );

		Assert( nLength == (bfRead.GetNumBitsRead() - iStartBit) ); 

		nPropCount++;
		iLastProp = iProp;
	}

	Assert( nPropCount <= nMaxProps );
	Assert( bfRead.GetNumBytesLeft() < 4 ); 
	Assert( !bfRead.IsOverflowed() );

#endif

	return true;
}






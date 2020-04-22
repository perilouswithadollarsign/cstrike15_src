//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "server.h"
#include "dt_stack.h"
#include "dt_localtransfer.h"
#include "mathlib/vector.h"
#include "edict.h"
#include "convar.h"
#include "con_nprint.h"
#include "utldict.h"
#include "dt_localtransfer.h"
#include "networkvar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



static ConVar dt_UsePartialChangeEnts( 
	"dt_UsePartialChangeEnts",
	"1",
	0,
	"(SP only) - enable FL_EDICT_PARTIAL_CHANGE optimization."
	);

static ConVar dt_ShowPartialChangeEnts( 
	"dt_ShowPartialChangeEnts", 
	"0", 
	0, 
	"(SP only) - show entities that were copied using small optimized lists (FL_EDICT_PARTIAL_CHANGE)." 
	);

// Negative numbers represent entities that were fully copied.
static CUtlVector<int> g_PartialChangeEnts;
static int g_nTotalPropChanges = 0;
static int g_nTotalEntChanges = 0;


template<class T>
inline void LocalTransfer_FastType(
	T *pBlah,
	CServerDatatableStack &serverStack,
	CClientDatatableStack &clientStack,
	CFastLocalTransferPropInfo *pPropList,
	int nProps )
{
	CFastLocalTransferPropInfo *pCur = pPropList;
	for ( int i=0; i < nProps; i++, pCur++ )
	{
		serverStack.SeekToProp( pCur->m_iProp );

		unsigned char *pServerBase = serverStack.GetCurStructBase();
		if ( pServerBase )
		{
			clientStack.SeekToProp( pCur->m_iProp );
			unsigned char *pClientBase = clientStack.GetCurStructBase();
			Assert( pClientBase );

			const T *pSource = (const T*)( pServerBase + pCur->m_iSendOffset );
			T *pDest = (T*)( pClientBase + pCur->m_iRecvOffset );
			*pDest = *pSource;
		}
	}
}

void AddPropOffsetToMap( CSendTablePrecalc *pPrecalc, int iInProp, int iInOffset, const char *pszPropName )
{
	Assert( iInProp < 0xFFFF && iInOffset < 0xFFFF );	
	unsigned short iProp = (unsigned short)iInProp;
	unsigned short iOffset = (unsigned short)iInOffset;
	
	unsigned short iOldIndex = pPrecalc->m_PropOffsetToIndexMap.Find( iOffset );

	if ( iOldIndex == pPrecalc->m_PropOffsetToIndexMap.InvalidIndex() )
	{
		PropIndicesCollection_t coll;
		coll.m_Indices[0] = iProp;
		for ( int i=1; i < ARRAYSIZE( coll.m_Indices ); i++ )
		{
			coll.m_Indices[i] = 0xFFFF;
		}

		pPrecalc->m_PropOffsetToIndexMap.Insert( iOffset, coll );
	}
	else
	{
		// At least one SendProp is pointing at this offset. No problem. We'll
		// remember all of them and trigger a change in all the SendProps attached
		// to this offset.
		//
		// Look for a slot in the PropIndicesCollection_t to hold the prop index.
		PropIndicesCollection_t &coll = pPrecalc->m_PropOffsetToIndexMap[iOldIndex];
		int i;
		for ( i=0; i < ARRAYSIZE( coll.m_Indices ); i++ )
		{
			if ( coll.m_Indices[i] == 0xFFFF )
			{
				coll.m_Indices[i] = iProp;
				break;
			}
		}

		if ( i >= ARRAYSIZE( coll.m_Indices ) )
		{
			// If there wasn't enough space to hold this SendProp index, then we have a CNetworkVar that won't 
			// cause this SendProp to get sent. The fixes are to increase the length of PropIndicesCollection_t::m_Indices
			// or to have less references to this SendProp in the class' tree.
			Error( "Overflowed a PropIndicesCollection_t on %s\n", pPrecalc->m_pSendTable->GetName() );
		}
	}
}

// This helps us figure out which properties can use the super-optimized mode
// where they are tracked in a list when they change. If their m_pProxies pointers
// are set to 1, then it means that this property is gotten to by means of SendProxy_DataTableToDataTable.
// If it's set to 0, then we can't directly take the property's offset.
class CPropMapStack : public CDatatableStack
{
public:
						CPropMapStack( CSendTablePrecalc *pPrecalc, const CStandardSendProxies *pSendProxies ) :
							CDatatableStack( pPrecalc, (unsigned char*)1, -1 )
						{
							m_pPropMapStackPrecalc = pPrecalc;
							m_pSendProxies = pSendProxies;
						}

	bool IsNonPointerModifyingProxy( SendTableProxyFn fn, const CStandardSendProxies *pSendProxies )
	{
		if ( fn == m_pSendProxies->m_DataTableToDataTable )
		{
			return true;
		}
		
		if( pSendProxies->m_ppNonModifiedPointerProxies )
		{
			CNonModifiedPointerProxy *pCur = *pSendProxies->m_ppNonModifiedPointerProxies;
			while ( pCur )
			{
				if ( pCur->m_Fn == fn )
					return true;
				pCur = pCur->m_pNext;
			}
		}

		return false;
	}

	inline unsigned char*	CallPropProxy( CSendNode *pNode, int iProp, unsigned char *pStructBase )
	{
		if ( !pStructBase )
			return 0;
		
		const SendProp *pProp = m_pPropMapStackPrecalc->GetDatatableProp( iProp );
		if ( IsNonPointerModifyingProxy( pProp->GetDataTableProxyFn(), m_pSendProxies ) )
		{
			// Note: these are offset by 1 (see the constructor), otherwise it won't recurse
			// during the Init call because pCurStructBase is 0.
			return pStructBase + pProp->GetOffset();
		}
		else
		{
			return 0;
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

public:
	CSendTablePrecalc *m_pPropMapStackPrecalc;
	const CStandardSendProxies *m_pSendProxies;
};


void BuildPropOffsetToIndexMap( CSendTablePrecalc *pPrecalc, const CStandardSendProxies *pSendProxies )
{
	CPropMapStack pmStack( pPrecalc, pSendProxies );
	pmStack.Init( false, false );
	
	for ( int i=0; i < pPrecalc->m_Props.Count(); i++ )
	{
		pmStack.SeekToProp( i );
		if ( pmStack.GetCurStructBase() != 0 )
		{
			const SendProp *pProp = pPrecalc->m_Props[i];
			
			int offset = size_cast< int >( pProp->GetOffset() + (intp)pmStack.GetCurStructBase() - 1 );
			int elementCount = 1;
			int elementStride = 0;
			if ( pProp->GetType() == DPT_Array )
			{
				offset = size_cast< int >( pProp->GetArrayProp()->GetOffset() + (intp)pmStack.GetCurStructBase() - 1 );
				elementCount = pProp->m_nElements;
				elementStride = pProp->m_ElementStride;
			}
			if ( offset != 0 )
			{
				for ( int j = 0; j < elementCount; j++ )
				{
					if ( pProp->GetType() == DPT_Vector )
					{
						/*
						// Disabled this, the warning is benign -Zoid
						if ( !pProp->AreNetworkVarFlagsSet( NETWORKVAR_IS_A_VECTOR ) )
						{
							// TODO: This warning can probably go away. It shows up frequently when using SENDINFO_STRUCTARRAYELEM
							// or a CNetworkArray of Vectors, which aren't using CNetworkVector but will report their change offsets properly.
							Warning( "SendProp %s::%s is referencing a non-CNetworkVector-based network var\n", pPrecalc->m_pSendTable->GetName(), pProp->GetName() );
						}
						*/

						if ( pProp->AreNetworkVarFlagsSet( NETWORKVAR_VECTOR_XYZ_FLAG ) )
						{
							// The CNetworkVarXYZ will generate change offsets for each component of the vector,
							// and we want all 3 change offsets to point right back to us.
							AddPropOffsetToMap( pPrecalc, i, offset, pProp->m_pVarName );
							AddPropOffsetToMap( pPrecalc, i, offset + sizeof(float), pProp->m_pVarName );
							AddPropOffsetToMap( pPrecalc, i, offset + sizeof(float) * 2, pProp->m_pVarName );
						}
						else if ( pProp->AreNetworkVarFlagsSet( NETWORKVAR_VECTOR_XY_SEPARATEZ_FLAG ) )
						{
							// The CNetworkVarXY_SeparateZ will generate change offsets for &x (which represents changes to X or Y),
							// and &z. We want both change offsets to point right back to us.
							AddPropOffsetToMap( pPrecalc, i, offset, pProp->m_pVarName );
							AddPropOffsetToMap( pPrecalc, i, offset + sizeof(float) * 2, pProp->m_pVarName );
						}
						else
						{
							// We're referenced by a CNetworkVector, which will only generate change offsets
							// at the offset of the variable itself (not for each component).
							AddPropOffsetToMap( pPrecalc, i, offset, pProp->m_pVarName );
						}
					}
					else if ( pProp->GetType() == DPT_VectorXY )
					{
						if ( pProp->AreNetworkVarFlagsSet( NETWORKVAR_VECTOR_XYZ_FLAG ) )
						{
							// The CNetworkVarXYZ will generate change offsets for each component of the vector,
							// and we want the first 2 change offsets to point right back to us.
							AddPropOffsetToMap( pPrecalc, i, offset, pProp->m_pVarName );
							AddPropOffsetToMap( pPrecalc, i, offset + sizeof(float), pProp->m_pVarName );
						}
						else if ( pProp->AreNetworkVarFlagsSet( NETWORKVAR_VECTOR_XY_SEPARATEZ_FLAG ) )
						{
							// The CNetworkVarXY_SeparateZ will generate change offsets for &x (which represents changes to X or Y),
							// and &z. We care about X and Y so we'll use the first one.
							AddPropOffsetToMap( pPrecalc, i, offset, pProp->m_pVarName );
						}
						else
						{
							// We're referenced by a CNetworkVector, which will only generate change offsets
							// at the offset of the variable itself (not for each component).
							AddPropOffsetToMap( pPrecalc, i, offset, pProp->m_pVarName );
						}
					}
					else
					{
						AddPropOffsetToMap( pPrecalc, i, offset, pProp->m_pVarName );
					}

					offset += elementStride;
				}
			}
		}
	}
}


void LocalTransfer_InitFastCopy( 
	const SendTable *pSendTable, 
	const CStandardSendProxies *pSendProxies,
	RecvTable *pRecvTable,
	const CStandardRecvProxies *pRecvProxies,
	int &nSlowCopyProps,		// These are incremented to tell you how many fast copy props it found.
	int &nFastCopyProps
	)
{
	CSendTablePrecalc *pPrecalc = pSendTable->m_pPrecalc;

	// Setup the offset-to-index map.
	pPrecalc->m_PropOffsetToIndexMap.RemoveAll();
	BuildPropOffsetToIndexMap( pPrecalc, pSendProxies );

	// Clear the old lists.
	pPrecalc->m_FastLocalTransfer.m_FastInt32.Purge();
	pPrecalc->m_FastLocalTransfer.m_FastInt16.Purge();
	pPrecalc->m_FastLocalTransfer.m_FastInt8.Purge();
	pPrecalc->m_FastLocalTransfer.m_FastVector.Purge();
	pPrecalc->m_FastLocalTransfer.m_OtherProps.Purge();
	
	CRecvDecoder *pDecoder = pRecvTable->m_pDecoder;
	int iNumProp = pPrecalc->GetNumProps();
	for ( int iProp=0; iProp < iNumProp; iProp++ )
	{
		const SendProp *pSendProp = pPrecalc->GetProp( iProp );
		const RecvProp *pRecvProp = pDecoder->GetProp( iProp );

		if ( pRecvProp )
		{
			Assert( stricmp( pSendProp->GetName(), pRecvProp->GetName() ) == 0 );

			CUtlVector<CFastLocalTransferPropInfo> *pList = &pPrecalc->m_FastLocalTransfer.m_OtherProps;

			if ( pSendProp->GetType() == DPT_Int &&
				(pSendProp->GetProxyFn() == pSendProxies->m_Int32ToInt32 || pSendProp->GetProxyFn() == pSendProxies->m_UInt32ToInt32) &&
				pRecvProp->GetProxyFn() == pRecvProxies->m_Int32ToInt32 )
			{
				pList = &pPrecalc->m_FastLocalTransfer.m_FastInt32;
				++nFastCopyProps;
			}
			else if( pSendProp->GetType() == DPT_Int && 
				(pSendProp->GetProxyFn() == pSendProxies->m_Int16ToInt32 || pSendProp->GetProxyFn() == pSendProxies->m_UInt16ToInt32) &&
				pRecvProp->GetProxyFn() == pRecvProxies->m_Int32ToInt16 )
			{
				pList = &pPrecalc->m_FastLocalTransfer.m_FastInt16;
				++nFastCopyProps;
			}
			else if( pSendProp->GetType() == DPT_Int && 
				(pSendProp->GetProxyFn() == pSendProxies->m_Int8ToInt32 || pSendProp->GetProxyFn() == pSendProxies->m_UInt8ToInt32) &&
				pRecvProp->GetProxyFn() == pRecvProxies->m_Int32ToInt8 )
			{
				pList = &pPrecalc->m_FastLocalTransfer.m_FastInt8;
				++nFastCopyProps;
			}
			else if( pSendProp->GetType() == DPT_Float && 
				pSendProp->GetProxyFn() == pSendProxies->m_FloatToFloat &&
				pRecvProp->GetProxyFn() == pRecvProxies->m_FloatToFloat )
			{
				Assert( sizeof( int ) == sizeof( float ) );
				pList = &pPrecalc->m_FastLocalTransfer.m_FastInt32;
				++nFastCopyProps;
			}
			else if ( pSendProp->GetType() == DPT_Vector && 
				pSendProp->GetProxyFn() == pSendProxies->m_VectorToVector &&
				pRecvProp->GetProxyFn() == pRecvProxies->m_VectorToVector )
			{
				pList = &pPrecalc->m_FastLocalTransfer.m_FastVector;
				++nFastCopyProps;
			}
			else
			{
				++nSlowCopyProps;
			}

			CFastLocalTransferPropInfo toAdd;
			toAdd.m_iProp = iProp;
			toAdd.m_iRecvOffset = pRecvProp->GetOffset();
			toAdd.m_iSendOffset = pSendProp->GetOffset();
			pList->AddToTail( toAdd );
		}
	}
}



static inline bool FindInList( uint16 *pList, int nCount, uint16 lookFor )
{
	for ( int i=0; i < nCount; i++ )
	{
		if ( pList[i] == lookFor )
			return true;
	}
	return false;
}


// This returns at most MAX_PROP_OFFSET_TO_INDICES_RESULTS results into pOut, so make sure it has at least that much room.
inline int MapPropOffsetsToIndices( 
	const CBaseEdict *pEdict,
	CSendTablePrecalc *pPrecalc, 
	const unsigned short *pOffsets,
	unsigned short nOffsets,
	unsigned short *pOut )
{
	// We could get an overflow if this happens.
	Assert( nOffsets <= MAX_CHANGE_OFFSETS );

	int nOut = 0;
	
	for ( unsigned short i=0; i < nOffsets; i++ )
	{
		unsigned short index = pPrecalc->m_PropOffsetToIndexMap.Find( pOffsets[i] );
		if ( index == pPrecalc->m_PropOffsetToIndexMap.InvalidIndex() )
		{
			// Note: this SHOULD be fine. In all known cases, when NetworkStateChanged is called with
			// an offset, there should be a corresponding SendProp in order for that NetworkStateChanged
			// call to mean anything. 
			//
			// It means that if we can't find an offset here, then there isn't a SendProp 
			// associated with the CNetworkVar that triggered the change. Therefore,
			// the change doesn't matter and we can skip past it. 
			//
			// If we wanted to be anal, we could force them to use DISABLE_NETWORK_VAR_FOR_DERIVED
			// appropriately in all these cases, but then we'd need a ton of them for certain classes
			// (like CBaseViewModel, which has a slew of CNetworkVars in its base classes that
			// it doesn't want to transmit).

			if ( dt_ShowPartialChangeEnts.GetInt() )
			{
				static CUtlDict<int,int> testDict;
				char str[512];
				Q_snprintf( str, sizeof( str ), "LocalTransfer offset miss - class: %s, DT: %s, offset: %d", pEdict->GetClassName(), pPrecalc->m_pSendTable->m_pNetTableName, pOffsets[i] );
				if ( testDict.Find( str ) == testDict.InvalidIndex() )
				{
					testDict.Insert( str );
					Warning( "%s\n", str );
				}
			}
		}
		else
		{
			const PropIndicesCollection_t &coll = pPrecalc->m_PropOffsetToIndexMap[index];
			
			for ( int nIndex=0; nIndex < ARRAYSIZE( coll.m_Indices ); nIndex++ )
			{
				unsigned short propIndex = coll.m_Indices[nIndex];
				if ( propIndex == 0xFFFF )
				{
					continue;
				}		
				else
				{
					if ( !FindInList( pOut, nOut, propIndex ) )
					{
						if ( nOut >= MAX_PROP_OFFSET_TO_INDICES_RESULTS )
							Error( "Overflowed output list in MapPropOffsetsToIndices" );

						pOut[nOut++] = propIndex;
					}
				}
			}
		}
	}
	
	return nOut;
}


inline void FastSortList( unsigned short *pList, unsigned short nEntries )
{
	if ( nEntries == 1 )
		return;
	
	unsigned short i = 0;
	while ( 1 )
	{
		if ( pList[i+1] < pList[i] )
		{
			unsigned short tmp = pList[i+1];
			pList[i+1] = pList[i];
			pList[i] = tmp;

			if ( i > 0 )
				--i;
		}
		else
		{
			++i;
			if ( i >= (nEntries-1) )
				return;
		}
	}
}


inline void AddToPartialChangeEntsList( int iEnt, bool bPartial )
{
	if ( !dt_ShowPartialChangeEnts.GetInt() )
		return;

	if ( !bPartial )
		iEnt = -iEnt;
	
	if ( g_PartialChangeEnts.Find( iEnt ) == -1 )
		g_PartialChangeEnts.AddToTail( iEnt );
}


void PrintPartialChangeEntsList()
{
	if ( !dt_ShowPartialChangeEnts.GetInt() )
		return;

	int iCurRow = 15;
	Con_NPrintf( iCurRow++, "----- dt_ShowPartialChangeEnts -----" );
	Con_NPrintf( iCurRow++, "" );
	Con_NPrintf( iCurRow++, "Ent changes: %3d, prop changes: %3d", 
		g_nTotalEntChanges, g_nTotalPropChanges );
	Con_NPrintf( iCurRow++, "" );
		
	char str[512];
	bool bFirst = true;
	
	
	// Write the partial ents.
	str[0] = 0;
	for ( int i=0; i < g_PartialChangeEnts.Count(); i++ )
	{
		if ( g_PartialChangeEnts[i] >= 0 )
		{
			if ( !bFirst )
				Q_strncat( str, ", ", sizeof( str ) );
			
			char tempStr[512];
			Q_snprintf( tempStr, sizeof( tempStr ), "%d", g_PartialChangeEnts[i] );
			Q_strncat( str, tempStr, sizeof( str ) );
						
			bFirst = false;
		}
	}
	Q_strncat( str, " - PARTIAL", sizeof( str ) );
	Con_NPrintf( iCurRow++, "%s", str );

	
	// Write the full ents.
	bFirst = true;
	str[0] = 0;
	for ( int i=0; i < g_PartialChangeEnts.Count(); i++ )
	{
		if ( g_PartialChangeEnts[i] < 0 )
		{
			if ( !bFirst )
				Q_strncat( str, ", ", sizeof( str ) );
			
			char tempStr[512];
			Q_snprintf( tempStr, sizeof( tempStr ), "%d", -g_PartialChangeEnts[i] );
			Q_strncat( str, tempStr, sizeof( str ) );
						
			bFirst = false;
		}
	}
	Q_strncat( str, " -    FULL", sizeof( str ) );
	Con_NPrintf( iCurRow++, "%s", str );
	
	g_PartialChangeEnts.Purge();
	g_nTotalPropChanges = g_nTotalEntChanges = 0;
}


void LocalTransfer_TransferEntity( 
	const CBaseEdict *pEdict,
	const SendTable *pSendTable, 
	const void *pSrcEnt, 
	RecvTable *pRecvTable, 
	void *pDestEnt,
	bool bNewlyCreated,
	bool bJustEnteredPVS,
	int objectID )
{
	++g_nTotalEntChanges;
	CEdictChangeInfo *pCI = &g_pSharedChangeInfo->m_ChangeInfos[pEdict->GetChangeInfo()];

	unsigned short propIndices[MAX_PROP_OFFSET_TO_INDICES_RESULTS];
	
	unsigned char tempData[ sizeof( CSendProxyRecipients ) * MAX_DATATABLE_PROXIES ];
	CUtlMemory< CSendProxyRecipients > recip( (CSendProxyRecipients*)tempData, pSendTable->m_pPrecalc->GetNumDataTableProxies() );

	// This code tries to only copy fields expressly marked as "changed" (by having the field offsets added to the changeoffsets vectors)
	if ( pEdict->GetChangeInfoSerialNumber() == g_pSharedChangeInfo->m_iSerialNumber &&
		 !bNewlyCreated &&
		 !bJustEnteredPVS &&
		 dt_UsePartialChangeEnts.GetInt()
		 )
	{
		CSendTablePrecalc *pPrecalc = pSendTable->m_pPrecalc;

		int nChangeOffsets = MapPropOffsetsToIndices( pEdict, pPrecalc, pCI->m_ChangeOffsets, pCI->m_nChangeOffsets, propIndices );
		if ( nChangeOffsets == 0 )
			return;
		
		AddToPartialChangeEntsList( (edict_t*)pEdict - sv.edicts, true );
		FastSortList( propIndices, nChangeOffsets );

		// Setup the structure to traverse the source tree.
		ErrorIfNot( pPrecalc, ("SendTable_Encode: Missing m_pPrecalc for SendTable %s.", pSendTable->m_pNetTableName) );
		CServerDatatableStack serverStack( pPrecalc, (unsigned char*)pSrcEnt, objectID, &recip );
		serverStack.Init( true, true );

		// Setup the structure to traverse the dest tree.
		CRecvDecoder *pDecoder = pRecvTable->m_pDecoder;
		ErrorIfNot( pDecoder, ("RecvTable_Decode: table '%s' missing a decoder.", pRecvTable->GetName()) );
		CClientDatatableStack clientStack( pDecoder, (unsigned char*)pDestEnt, objectID );
		clientStack.Init( true, true );

		// Cool. We can get away with just transferring a few.
		for ( int iChanged=0; iChanged < nChangeOffsets; iChanged++ )
		{
			int iProp = propIndices[iChanged];

			++g_nTotalPropChanges;
			
			serverStack.SeekToProp( iProp );
			const SendProp *pSendProp = serverStack.GetCurProp();
			unsigned char *pSendBase = serverStack.UpdateRoutesExplicit();
			if ( pSendBase )
			{
				const RecvProp *pRecvProp = pDecoder->GetProp( iProp );
				Assert( pRecvProp );

				clientStack.SeekToProp( iProp );
				unsigned char *pRecvBase = clientStack.UpdateRoutesExplicit();
				Assert( pRecvBase );
				
				g_PropTypeFns[pRecvProp->GetType()].FastCopy( pSendProp, pRecvProp, pSendBase, pRecvBase, objectID );
			}
		}
	}
	// Whereas the below code copies _all_ fields, regardless of whether they were changed or not.  We run this only newly created entities, or entities
	//  which were previously dormant/outside the pvs but are now back in the PVS since we could have missed field updates since the changeoffsets get cleared every
	//  frame.
	else
	{
		// Setup the structure to traverse the source tree.
		CSendTablePrecalc *pPrecalc = pSendTable->m_pPrecalc;
		ErrorIfNot( pPrecalc, ("SendTable_Encode: Missing m_pPrecalc for SendTable %s.", pSendTable->m_pNetTableName) );
		CServerDatatableStack serverStack( pPrecalc, (unsigned char*)pSrcEnt, objectID, &recip );
		serverStack.Init( false, true );

		// Setup the structure to traverse the dest tree.
		CRecvDecoder *pDecoder = pRecvTable->m_pDecoder;
		ErrorIfNot( pDecoder, ("RecvTable_Decode: table '%s' missing a decoder.", pRecvTable->GetName()) );
		CClientDatatableStack clientStack( pDecoder, (unsigned char*)pDestEnt, objectID );
		clientStack.Init( false, true );

		AddToPartialChangeEntsList( (edict_t*)pEdict - sv.edicts, false );

		// Copy the properties that require proxies.
		CFastLocalTransferPropInfo *pPropList = pPrecalc->m_FastLocalTransfer.m_OtherProps.Base();
		int nProps = pPrecalc->m_FastLocalTransfer.m_OtherProps.Count();
		
		for ( int i=0; i < nProps; i++ )
		{
			int iProp = pPropList[i].m_iProp;

			serverStack.SeekToProp( iProp );
			const SendProp *pSendProp = serverStack.GetCurProp();
			unsigned char *pSendBase = serverStack.GetCurStructBase();

			if ( pSendBase )
			{
				const RecvProp *pRecvProp = pDecoder->GetProp( iProp );
				Assert( pRecvProp );

				clientStack.SeekToProp( iProp );
				unsigned char *pRecvBase = clientStack.GetCurStructBase();
				Assert( pRecvBase );
				
				g_PropTypeFns[pRecvProp->GetType()].FastCopy( pSendProp, pRecvProp, pSendBase, pRecvBase, objectID );
			}
		}

 		// Transfer over the fast properties.
		LocalTransfer_FastType( (int*)0, serverStack, clientStack, pPrecalc->m_FastLocalTransfer.m_FastInt32.Base(), pPrecalc->m_FastLocalTransfer.m_FastInt32.Count() );
		LocalTransfer_FastType( (short*)0, serverStack, clientStack, pPrecalc->m_FastLocalTransfer.m_FastInt16.Base(), pPrecalc->m_FastLocalTransfer.m_FastInt16.Count() );
		LocalTransfer_FastType( (char*)0, serverStack, clientStack, pPrecalc->m_FastLocalTransfer.m_FastInt8.Base(), pPrecalc->m_FastLocalTransfer.m_FastInt8.Count() );
		LocalTransfer_FastType( (Vector*)0, serverStack, clientStack, pPrecalc->m_FastLocalTransfer.m_FastVector.Base(), pPrecalc->m_FastLocalTransfer.m_FastVector.Count() );
	}		

	
	// The old, slow method to copy all props using their proxies.		
	/*
	int iEndProp = pPrecalc->m_Root.GetLastPropIndex();
	for ( int iProp=0; iProp <= iEndProp; iProp++ )
	{
		serverStack.SeekToProp( iProp );
		clientStack.SeekToProp( iProp );

		const SendProp *pSendProp = serverStack.GetCurProp();
		const RecvProp *pRecvProp = pDecoder->GetProp( iProp );
		if ( pRecvProp )
		{
			unsigned char *pSendBase = serverStack.GetCurStructBase();
			unsigned char *pRecvBase = clientStack.GetCurStructBase();
			if ( pSendBase && pRecvBase )
			{
				Assert( stricmp( pSendProp->GetName(), pRecvProp->GetName() ) == 0 );

				g_PropTypeFns[pRecvProp->GetType()].FastCopy( pSendProp, pRecvProp, pSendBase, pRecvBase, objectID );
			}
		}
	}
	*/
}




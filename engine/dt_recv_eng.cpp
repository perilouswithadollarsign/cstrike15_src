//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "dt.h"
#include "dt_recv_eng.h"
#include "dt_encode.h"
#include "dt_instrumentation.h"
#include "dt_stack.h"
#include "utllinkedlist.h"
#include "tier0/dbg.h"
#include "dt_recv_decoder.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "dt_common_eng.h"
#include "common.h"
#include "serializedentity.h"
#include "netmessages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CClientSendTable;


// Testing out this pattern.. you can write simple code blocks inside of
// codeToRun. The thing that sucks is that you can't access your function's
// local variables inside of codeToRun.
//
// If it used an iterator class, it could access local function variables,
// but the iterator class might be more trouble than it's worth.
#define FOR_EACH_PROP_R( TableType, pTablePointer, tableCode, propCode )	\
	class CPropVisitor 										\
	{ 														\
		public:												\
		static void Visit_R( TableType *pTable )					\
		{													\
			tableCode;										\
															\
			for ( int i=0; i < pTable->GetNumProps(); i++ )	\
			{												\
				TableType::PropType *pProp = pTable->GetProp( i );	\
															\
				propCode;									\
															\
				if ( pProp->GetType() == DPT_DataTable )	\
					Visit_R( pProp->GetDataTable() );		\
			}												\
		}													\
	};														\
	CPropVisitor::Visit_R( pTablePointer );				

#define SENDPROP_VISIT( pTablePointer, tableCode, propCode )  FOR_EACH_PROP_R( SendTable, pTablePointer, tableCode, propCode )
#define RECVPROP_VISIT( pTablePointer, tableCode, propCode )  FOR_EACH_PROP_R( RecvTable, pTablePointer, tableCode, propCode )
#define SETUP_VISIT() class CDummyClass {} // Workaround for parser bug in VC7.1

// ------------------------------------------------------------------------------------ //
// Globals.
// ------------------------------------------------------------------------------------ //

CUtlLinkedList< RecvTable*, unsigned short > g_RecvTables;
CUtlLinkedList< CRecvDecoder *, unsigned short > g_RecvDecoders;
CUtlLinkedList< CClientSendTable*, unsigned short > g_ClientSendTables;

// ------------------------------------------------------------------------------------ //
// Static helper functions.
// ------------------------------------------------------------------------------------ //

RecvTable* FindRecvTable( const char *pName )
{
	FOR_EACH_LL( g_RecvTables, i )
	{
		if ( stricmp( g_RecvTables[i]->GetName(), pName ) == 0 )
			return g_RecvTables[i];
	}
	return 0;
}


static CClientSendTable* FindClientSendTable( const char *pName )
{
	FOR_EACH_LL( g_ClientSendTables, i )
	{
		CClientSendTable *pTable = g_ClientSendTables[i];

		if ( stricmp( pTable->GetName(), pName ) == 0 )
			return pTable;
	}

	return NULL;
}


// Find all child datatable properties for the send tables.
bool SetupClientSendTableHierarchy()
{
	FOR_EACH_LL( g_ClientSendTables, iClientTable )
	{
		CClientSendTable *pTable = g_ClientSendTables[iClientTable];
		
		// For each datatable property, find the table it references.
		for ( int iProp=0; iProp < pTable->GetNumProps(); iProp++ )
		{
			CClientSendProp *pClientProp = pTable->GetClientProp( iProp );
			SendProp *pProp = &pTable->m_SendTable.m_pProps[iProp];

			if ( pProp->m_Type == DPT_DataTable )
			{
				const char *pTableName = pClientProp->GetTableName();
				ErrorIfNot( pTableName,
					("SetupClientSendTableHierarchy: missing table name for prop '%s'.", pProp->GetName())
				);

				CClientSendTable *pChild = FindClientSendTable( pTableName );
				if ( !pChild )
				{
					DataTable_Warning( "SetupClientSendTableHierarchy: missing SendTable '%s' (referenced by '%s').\n", pTableName, pTable->GetName() );
					return false;
				}

				pProp->SetDataTable( &pChild->m_SendTable );
			}
		}
	}
	
	return true;
}


static RecvProp* FindRecvProp( RecvTable *pTable, const char *pName )
{
	for ( int i=0; i < pTable->GetNumProps(); i++ )
	{
		RecvProp *pProp = pTable->GetProp( i );

#ifdef DBGFLAG_ASSERT
		// Debug validation to handle that no network fields get created with colliding names to special UTL vector networking fields
		// because we will have custom receive table remapping below and don't want to mistakenly route bytes into wrong memory
		// See dt_utlvector_recv.cpp / RecvPropUtlVector for details of this remapping
		if ( char const *szLength = StringAfterPrefix( pProp->GetName(), "lengthprop" ) )
		{
			// *pLengthProp = RecvPropInt( AllocateStringHelper( "lengthprop%d", nMaxElements ), 0, 0, 0, RecvProxy_UtlVectorLength );
			Assert( pProp->GetType() == DPT_Int );
			Assert( *szLength );
			for ( char const *szCheck = szLength; szCheck && *szCheck; ++ szCheck )
			{
				Assert( V_isdigit( *szCheck ) ); // assert that the number forms the length of array
				Assert( szCheck - szLength < 5 ); // arrays are never that large!
			}
		}
		else if ( char const *szLPT = StringAfterPrefix( pProp->GetName(), "_LPT_" ) )
		{
			// char *pLengthProxyTableName = AllocateUniqueDataTableName( false, "_LPT_%s_%d", pVarName, nMaxElements );
			Assert( pProp->GetType() == DPT_DataTable );
			char const *szLPTsize = strrchr( szLPT, '_' );
			Assert( szLPTsize );
			if ( szLPTsize )
			{
				++ szLPTsize;
				Assert( *szLPTsize );
			}
			for ( char const *szCheck = szLPTsize; szCheck && *szCheck; ++szCheck )
			{
				Assert( V_isdigit( *szCheck ) ); // assert that the number forms the length of array
				Assert( szCheck - szLength < 5 ); // arrays are never that large!
			}
		}
		else if ( char const *szST = StringAfterPrefix( pProp->GetName(), "_ST_" ) )
		{
			// AllocateUniqueDataTableName( false, "_ST_%s_%d", pVarName, nMaxElements )
			Assert( pProp->GetType() == DPT_DataTable );
			char const *szSTsize = strrchr( szST, '_' );
			Assert( szSTsize );
			if ( szSTsize )
			{
				++szSTsize;
				Assert( *szSTsize );
			}
			for ( char const *szCheck = szSTsize; szCheck && *szCheck; ++szCheck )
			{
				Assert( V_isdigit( *szCheck ) ); // assert that the number forms the length of array
				Assert( szCheck - szLength < 5 ); // arrays are never that large!
			}
		}
#endif

		if ( !V_stricmp( pProp->GetName(), pName ) )
			return pProp;

		//
		// Special case to receive UTL vector networked prop into a larger UTL vector networked prop on the client
		// See dt_utlvector_recv.cpp / RecvPropUtlVector for details of this remapping
		//
		if ( char const *p_SEND_Length = StringAfterPrefix( pName, "lengthprop" ) )
		{
			// We are being sent a lengthprop##
			if ( char const *p_RECV_Length = StringAfterPrefix( pProp->GetName(), "lengthprop" ) )
			{
				if ( Q_atoi( p_SEND_Length ) <= Q_atoi( p_RECV_Length ) )
					return pProp;
			}
		}
		else if ( char const *p_SEND_LPT = StringAfterPrefix( pName, "_LPT_" ) )
		{
			// We are being sent an _LPT_(varname)_## field
			if ( char const *p_RECV_LPT = StringAfterPrefix( pProp->GetName(), "_LPT_" ) )
			{
				// Trim the length from the field
				char const *p_SEND_LPT_size = strrchr( p_SEND_LPT, '_' );
				char const *p_RECV_LPT_size = strrchr( p_RECV_LPT, '_' );
				if ( p_SEND_LPT_size && p_RECV_LPT_size &&
					( p_SEND_LPT_size - p_SEND_LPT == p_RECV_LPT_size - p_RECV_LPT ) &&
					!V_strnicmp( p_SEND_LPT, p_RECV_LPT, p_RECV_LPT_size - p_RECV_LPT ) &&
					( Q_atoi( p_SEND_LPT_size + 1 ) <= Q_atoi( p_RECV_LPT_size + 1 ) ) )
					return pProp;
			}
		}
		else if ( char const *p_SEND_ST = StringAfterPrefix( pName, "_ST_" ) )
		{
			// We are being sent an _ST_(varname)_## field
			if ( char const *p_RECV_ST = StringAfterPrefix( pProp->GetName(), "_ST_" ) )
			{
				// Trim the length from the field
				char const *p_SEND_ST_size = strrchr( p_SEND_ST, '_' );
				char const *p_RECV_ST_size = strrchr( p_RECV_ST, '_' );
				if ( p_SEND_ST_size && p_RECV_ST_size &&
					( p_SEND_ST_size - p_SEND_ST == p_RECV_ST_size - p_RECV_ST ) &&
					!V_strnicmp( p_SEND_ST, p_RECV_ST, p_RECV_ST_size - p_RECV_ST ) &&
					( Q_atoi( p_SEND_ST_size + 1 ) <= Q_atoi( p_RECV_ST_size + 1 ) ) )
					return pProp;
			}
		}
		//
		// End of UTL vector backwards compatibility receiving remap
		//
	}

	// Support recursing into base classes to find the required field:
	if ( pTable->GetNumProps() )
	{
		RecvProp *pSubProp = pTable->GetProp( 0 );
		if ( ( pSubProp->GetType() == DPT_DataTable ) &&
			!V_stricmp( pSubProp->GetName(), "baseclass" ) )
			return FindRecvProp( pSubProp->GetDataTable(), pName );
	}
	
	return NULL;
}


// See if the RecvProp is fit to receive the SendProp's data.
bool CompareRecvPropToSendProp( const RecvProp *pRecvProp, const SendProp *pSendProp )
{
	while ( 1 )
	{
		ErrorIfNot( pRecvProp && pSendProp,
			("CompareRecvPropToSendProp: missing a property.")
		);

		if ( pRecvProp->GetType() != pSendProp->GetType() || pRecvProp->IsInsideArray() != pSendProp->IsInsideArray() )
		{
			return false;
		}

		if ( pRecvProp->GetType() == DPT_Array )
		{
			// It should be OK to receive into a larger array, just later elements
			// will not ever be received
			if ( pRecvProp->GetNumElements() < pSendProp->GetNumElements() )
				return false;
			
			pRecvProp = pRecvProp->GetArrayProp();
			pSendProp = pSendProp->GetArrayProp();
		}
		else
		{
			return true;
		}
	}
}

struct MatchingProp_t
{
	SendProp	*m_pProp;
	RecvProp	*m_pMatchingRecvProp;

	static bool LessFunc( const MatchingProp_t& lhs, const MatchingProp_t& rhs )
	{
		return lhs.m_pProp < rhs.m_pProp;
	}
};

static bool MatchRecvPropsToSendProps_R( CUtlRBTree< MatchingProp_t, unsigned short >& lookup, char const *sendTableName, SendTable *pSendTable, RecvTable *pRecvTable, bool bAllowMismatches, bool *pAnyMismatches )
{
	for ( int i=0; i < pSendTable->m_nProps; i++ )
	{
		SendProp *pSendProp = &pSendTable->m_pProps[i];

		if ( pSendProp->IsExcludeProp() || pSendProp->IsInsideArray() )
			continue;

		// Find a RecvProp by the same name and type.
		RecvProp *pRecvProp = 0;
		if ( pRecvTable )
			pRecvProp = FindRecvProp( pRecvTable, pSendProp->GetName() );

		if ( pRecvProp )
		{
			if ( !CompareRecvPropToSendProp( pRecvProp, pSendProp ) )
			{
				Warning( "RecvProp type doesn't match server type for %s/%s\n", pSendTable->GetName(), pSendProp->GetName() );
				return false;
			}

			MatchingProp_t info;
			info.m_pProp = pSendProp;
			info.m_pMatchingRecvProp = pRecvProp;

			lookup.Insert( info );
		}
		else
		{
			if ( pAnyMismatches )
			{
				*pAnyMismatches = true;
			}

			DevWarning( "Missing RecvProp for %s - %s/%s\n", sendTableName, pSendTable->GetName(), pSendProp->GetName() );
			if ( !bAllowMismatches )
			{
				return false;
			}
		}

		// Recurse.
		if ( pSendProp->GetType() == DPT_DataTable )
		{
			if ( !MatchRecvPropsToSendProps_R( lookup, sendTableName, pSendProp->GetDataTable(), pRecvProp ? pRecvProp->GetDataTable() : 0, bAllowMismatches, pAnyMismatches ) )
				return false;
		}
	}

	return true;
}


extern bool s_debug_info_shown;
extern int  s_debug_bits_start;

static inline void ShowDecodeDeltaWatchInfo( 
	char *what,
	const RecvTable *pTable,
	const SendProp *pProp, 
	bf_read &buffer,
	const int objectID,
	const int index )
{
	if ( !ShouldWatchThisProp( pTable, objectID, pProp->GetName()) )
		return;

	extern int host_framecount;

	static int lastframe = -1;
	if ( host_framecount != lastframe )
	{
		lastframe = host_framecount;
		ConDMsg( "D: delta entity: %i %s\n", objectID, pTable->GetName() );
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

	ConDMsg( "D[%s]:%s %s, %s, index %i, offset %i, bits %i, value %s\n", what, pTable->GetName(), pProp->GetName(), type, index, startBit, bits, value );
}




// ------------------------------------------------------------------------------------ //
// Interface functions.
// ------------------------------------------------------------------------------------ //
bool RecvTable_Init( RecvTable **pTables, int nTables )
{
	SETUP_VISIT();

	for ( int i=0; i < nTables; i++ )
	{
		RECVPROP_VISIT( pTables[i], 
			{
				if ( pTable->IsInMainList() )
					return;
				
				// Shouldn't have a decoder yet.
				ErrorIfNot( !pTable->m_pDecoder,
					("RecvTable_Init: table '%s' has a decoder already.", pTable->GetName()));
				
				pTable->SetInMainList( true );
				g_RecvTables.AddToTail( pTable );
			},

			{}
		);
	}

	return true;
}


void RecvTable_Term( bool clearall /*= true*/ )
{
	DTI_Term();

	SETUP_VISIT();

	FOR_EACH_LL( g_RecvTables, i )
	{
		RECVPROP_VISIT( g_RecvTables[i],
			{
				if ( !pTable->IsInMainList() )
					return;

				pTable->SetInMainList( false );
				pTable->m_pDecoder = 0;
			},

			{}
		);
	}

	if ( clearall )
	{
		g_RecvTables.Purge();
	}
	g_RecvDecoders.PurgeAndDeleteElements();
	g_ClientSendTables.PurgeAndDeleteElements();
}

void RecvTable_FreeSendTable( SendTable *pTable )
{
	for ( int iProp=0; iProp < pTable->m_nProps; iProp++ )
	{
		SendProp *pProp = &pTable->m_pProps[iProp];

		delete [] pProp->m_pVarName;

		if ( pProp->m_pExcludeDTName )
			delete [] pProp->m_pExcludeDTName;
	}

	if ( pTable->m_pProps )
		delete [] pTable->m_pProps;

	delete [] pTable->m_pNetTableName;
	delete pTable;
}

static char* AllocString( const char *pStr )
{
	int allocLen = strlen( pStr ) + 1;

	char *pOut = new char[allocLen];
	V_strncpy( pOut, pStr, allocLen );
	return pOut;
}

SendTable *RecvTable_ReadInfos( const CSVCMsg_SendTable& msg, int nDemoProtocol )
{
	SendTable *pTable = new SendTable;

	pTable->m_pNetTableName = AllocString( msg.net_table_name().c_str() );

	// Read the property list.
	pTable->m_nProps = msg.props_size();
	pTable->m_pProps = pTable->m_nProps ? new SendProp[ pTable->m_nProps ] : NULL;

	for ( int iProp=0; iProp < pTable->m_nProps; iProp++ )
	{
		SendProp *pProp = &pTable->m_pProps[iProp];
		const CSVCMsg_SendTable::sendprop_t& sendProp = msg.props( iProp );

		pProp->m_Type = (SendPropType)sendProp.type();
		pProp->m_pVarName = AllocString( sendProp.var_name().c_str() );

		pProp->SetFlags( sendProp.flags() );
		pProp->SetPriority( sendProp.priority() );

		if ( ( pProp->m_Type == DPT_DataTable ) || ( pProp->IsExcludeProp() ) )
		{
			pProp->m_pExcludeDTName = AllocString( sendProp.dt_name().c_str() );
		}
		else if ( pProp->GetType() == DPT_Array )
		{
			pProp->SetNumElements( sendProp.num_elements() );
		}
		else
		{
			pProp->m_fLowValue = sendProp.low_value();
			pProp->m_fHighValue = sendProp.high_value();
			pProp->m_nBits = sendProp.num_bits();
		}
	}

	return pTable;
}

bool RecvTable_RecvClassInfos( const CSVCMsg_SendTable& msg, int nDemoProtocol )
{
	SendTable *pSendTable = RecvTable_ReadInfos( msg, nDemoProtocol );

	if ( !pSendTable )
		return false;

	bool ret = DataTable_SetupReceiveTableFromSendTable( pSendTable, msg.needs_decoder() );

	RecvTable_FreeSendTable( pSendTable );

	return ret;
}

static void CopySendPropsToRecvProps( 
	CUtlRBTree< MatchingProp_t, unsigned short >& lookup, 
	const CUtlVector<const SendProp*> &sendProps, 
	CUtlVector<const RecvProp*> &recvProps 
	)
{
	recvProps.SetSize( sendProps.Count() );
	for ( int iSendProp=0; iSendProp < sendProps.Count(); iSendProp++ )
	{
		const SendProp *pSendProp = sendProps[iSendProp];
		MatchingProp_t search;
		search.m_pProp = (SendProp *)pSendProp;
		int idx = lookup.Find( search );
		if ( idx == lookup.InvalidIndex() )
		{
			recvProps[iSendProp] = 0;
		}
		else
		{
			recvProps[iSendProp] = lookup[ idx ].m_pMatchingRecvProp;
		}
	}
}

bool RecvTable_CreateDecoders( const CStandardSendProxies *pSendProxies, bool bAllowMismatches, bool *pAnyMismatches )
{
	DTI_Init();

	SETUP_VISIT();

	if ( pAnyMismatches )
	{
		*pAnyMismatches = false;
	}

	// First, now that we've supposedly received all the SendTables that we need,
	// set their datatable child pointers.
	if ( !SetupClientSendTableHierarchy() )
		return false;

	bool bRet = true;

	FOR_EACH_LL( g_RecvDecoders, i )
	{
		CRecvDecoder *pDecoder = g_RecvDecoders[i];


		// It should already have been linked to its ClientSendTable.
		Assert( pDecoder->m_pClientSendTable );
		if ( !pDecoder->m_pClientSendTable )
			return false;


		// For each decoder, precalculate the SendTable's flat property list.
		if ( !pDecoder->m_Precalc.SetupFlatPropertyArray() )
			return false;

		CUtlRBTree< MatchingProp_t, unsigned short >	PropLookup( 0, 0, MatchingProp_t::LessFunc );

		// Now match RecvProp with SendProps.
		if ( !MatchRecvPropsToSendProps_R( PropLookup, pDecoder->GetSendTable()->m_pNetTableName, pDecoder->GetSendTable(), pDecoder->GetRecvTable(), bAllowMismatches, pAnyMismatches ) )
		{
			bRet = false;
		}
		else
		{
	
			// Now fill out the matching RecvProp array.
			CSendTablePrecalc *pPrecalc = &pDecoder->m_Precalc;
			CopySendPropsToRecvProps( PropLookup, pPrecalc->m_Props, pDecoder->m_Props );
			CopySendPropsToRecvProps( PropLookup, pPrecalc->m_DatatableProps, pDecoder->m_DatatableProps );
		
			DTI_HookRecvDecoder( pDecoder );
		}
	}

	return bRet;
}

bool RecvTable_Decode( 
	RecvTable *pTable, 
	void *pStruct, 
	SerializedEntityHandle_t dest,
	int objectID
	)
{
	CRecvDecoder *pDecoder = pTable->m_pDecoder;
	ErrorIfNot( pDecoder,
		("RecvTable_Decode: table '%s' missing a decoder.", pTable->GetName())
		);

	CSerializedEntity *pEntity = reinterpret_cast< CSerializedEntity * >( dest );
	Assert( pEntity );

	// While there are properties, decode them.. walk the stack as you go.
	CClientDatatableStack theStack( pDecoder, (unsigned char*)pStruct, objectID );
	theStack.Init( false, false );

	bf_read buf;
	buf.SetDebugName( "CFlattenedSerializer::Decode" );
	pEntity->StartReading( buf );

	CFieldPath path;

	int nDataOffset;
	int nNextDataOffset;

	for ( int nFieldIndex = 0 ; nFieldIndex < pEntity->GetFieldCount() ; ++nFieldIndex )
	{
		pEntity->GetField( nFieldIndex, path, &nDataOffset, &nNextDataOffset );
		buf.Seek( nDataOffset );

		theStack.SeekToProp( path );

		const RecvProp *pProp = pDecoder->GetProp( path );

		DecodeInfo decodeInfo;
		decodeInfo.m_pStruct = theStack.GetCurStructBase();

		if ( pProp )
		{
			decodeInfo.m_pData = theStack.GetCurStructBase() + pProp->GetOffset();
		}
		else
		{
			// They're allowed to be missing props here if they're playing back a demo.
			// This allows us to change the datatables and still preserve old demos.
			decodeInfo.m_pData = NULL;
		}

		decodeInfo.m_pRecvProp = theStack.IsCurProxyValid() ? pProp : NULL; // Just skip the data if the proxies are screwed.
		decodeInfo.m_pProp = pDecoder->GetSendProp( path );
		decodeInfo.m_pIn = &buf;
		decodeInfo.m_ObjectID = objectID;

		g_PropTypeFns[ decodeInfo.m_pProp->GetType() ].Decode( &decodeInfo );
	}

	return !buf.IsOverflowed();			
}

void RecvTable_DecodeZeros( RecvTable *pTable, void *pStruct, int objectID )
{
	CRecvDecoder *pDecoder = pTable->m_pDecoder;
	ErrorIfNot( pDecoder,
		("RecvTable_DecodeZeros: table '%s' missing a decoder.", pTable->GetName())
	);

	// While there are properties, decode them.. walk the stack as you go.
	CClientDatatableStack theStack( pDecoder, (unsigned char*)pStruct, objectID );

	theStack.Init( false, false );

	for ( int iProp=0; iProp < pDecoder->GetNumProps(); iProp++ )
	{	
		theStack.SeekToProp( iProp );
	
		const RecvProp *pProp = pDecoder->GetProp( iProp );

		DecodeInfo decodeInfo;
		decodeInfo.m_pStruct = theStack.GetCurStructBase();
		decodeInfo.m_pData = theStack.GetCurStructBase() + pProp->GetOffset();
		decodeInfo.m_pRecvProp = theStack.IsCurProxyValid() ? pProp : NULL; // Just skip the data if the proxies are screwed.
		decodeInfo.m_pProp = pDecoder->GetSendProp( iProp );
		decodeInfo.m_pIn = NULL;
		decodeInfo.m_ObjectID = objectID;

		g_PropTypeFns[pProp->GetType()].DecodeZero( &decodeInfo );
	}
}


// Copies pProp's state from pIn to pOut. pDecodeInfo MUST be setup by calling InitDecodeInfoForSkippingProps
// with pIn.
//
// NOTE: this routine isn't optimal. If it shows up on the profiles, then it's easy to
// make this fast by adding a special routine to copy a property's state to PropTypeFns.
static void CopyPropState( 
	CRecvDecoder *pDecoder, 
	int iSendProp, 
	bf_read *pIn, 
	CDeltaBitsWriter *pOut
	)
{
	const SendProp *pProp = pDecoder->GetSendProp( iSendProp );

	int iStartBit = pIn->GetNumBitsRead();

	// skip over data
	SkipPropData( pIn, pProp );
	
	// Figure out how many bits it took.
	int nBits = pIn->GetNumBitsRead() - iStartBit;
	
	// Copy the data 
	pIn->Seek( iStartBit );

	pOut->WritePropIndex( iSendProp );
	pOut->GetBitBuf()->WriteBitsFromBuffer( pIn, nBits );
}


bool RecvTable_MergeDeltas(
	RecvTable *pTable,

	SerializedEntityHandle_t oldState, // Can be invalid
	SerializedEntityHandle_t newState,
	SerializedEntityHandle_t mergedState,

	int objectID,
	CUtlVector< int > *pVecChanges 
	)
{
	Assert( SERIALIZED_ENTITY_HANDLE_INVALID != newState );
	Assert( SERIALIZED_ENTITY_HANDLE_INVALID != mergedState );

	CSerializedEntity *pOldState = oldState != SERIALIZED_ENTITY_HANDLE_INVALID ? reinterpret_cast< CSerializedEntity * >( oldState ) : NULL;

	CSerializedEntity *pNewState = reinterpret_cast< CSerializedEntity * >( newState );
	Assert( pNewState );

	CSerializedEntity *pMergedState = reinterpret_cast< CSerializedEntity * >( mergedState );
	Assert( pMergedState );

	ErrorIfNot( pTable && pNewState && pMergedState, ("RecvTable_MergeDeltas: invalid parameters passed.")
	);

	CRecvDecoder *pDecoder = pTable->m_pDecoder;
	ErrorIfNot( pDecoder, ("RecvTable_MergeDeltas: table '%s' is missing its decoder.", pTable->GetName()) );

	CSerializedEntityFieldIterator oldIterator( pOldState );
	CSerializedEntityFieldIterator newIterator( pNewState );
	bf_read oldBits, newBits;
	oldBits.SetDebugName( "CFlattenedSerializer::MergeDeltas: oldBits" );
	newBits.SetDebugName( "CFlattenedSerializer::MergeDeltas: newBits" );

	if ( pOldState )
	{
		pOldState->StartReading( oldBits );
	}
	pNewState->StartReading( newBits );

	pMergedState->Clear();

	const CFieldPath *oldFieldPath = oldIterator.FirstPtr();
	const CFieldPath *newFieldPath = newIterator.FirstPtr();

	uint8 packedData[MAX_PACKEDENTITY_DATA];
	bf_write fieldDataBuf( "CFlattenedSerializer::WriteFieldList fieldDataBuf", packedData, sizeof( packedData ) );

	while ( 1 )
	{
		// Write any properties in the previous state that aren't in the new state.
		while ( *oldFieldPath < *newFieldPath )
		{
			pMergedState->AddPathAndOffset( *oldFieldPath, fieldDataBuf.GetNumBitsWritten() );

			oldBits.Seek( oldIterator.GetOffset() );
			fieldDataBuf.WriteBitsFromBuffer( &oldBits, oldIterator.GetLength() );

			oldFieldPath = oldIterator.NextPtr();
		}

		if ( *newFieldPath == PROP_SENTINEL )
			break;

		// Check if we're at the end here so the while() statement above can seek the old buffer
		// to its end too.
		bool bBoth = ( *oldFieldPath == *newFieldPath ); 
		// If the old state has this property too, then just skip over its data.
		if ( bBoth )
		{
			oldFieldPath = oldIterator.NextPtr();
		}
		
		pMergedState->AddPathAndOffset( *newFieldPath, fieldDataBuf.GetNumBitsWritten() );
		newBits.Seek( newIterator.GetOffset() );
		fieldDataBuf.WriteBitsFromBuffer( &newBits, newIterator.GetLength() );

		if ( pVecChanges )
		{
			pVecChanges->AddToTail( *newFieldPath );
		}

		newFieldPath = newIterator.NextPtr();
	}

	pMergedState->PackWithFieldData( packedData, fieldDataBuf.GetNumBitsWritten() );

	ErrorIfNot( !fieldDataBuf.IsOverflowed(), ("RecvTable_MergeDeltas: overflowed in RecvTable '%s'.", pTable->GetName() ) );

	return true;
}

template< bool bDTIEnabled >
bool RecvTable_ReadFieldList_Guts( 
	RecvTable *pTable, 

	bf_read &buf, 
	SerializedEntityHandle_t dest, 
	int nObjectId 
	)
{
	CSerializedEntity *pEntity = reinterpret_cast< CSerializedEntity * >( dest );
	Assert( pEntity );

	pEntity->Clear();

	CUtlVector< int > fieldBits;
	pEntity->ReadFieldPaths( &buf, bDTIEnabled ? &fieldBits : NULL );

	ErrorIfNot( pTable, ("RecvTable_ReadFieldListt: Missing RecvTable for class\n" ) );
	if ( !pTable )
		return false;

	CRecvDecoder *pDecoder = pTable->m_pDecoder;
	ErrorIfNot( pDecoder, ("RecvTable_ReadFieldList: table '%s' missing a decoder.", pTable->GetName()) );

	// Remember where the "data" payload started
	int nStartBit = buf.GetNumBitsRead();

	CFieldPath path;
	for ( int nFieldIndex = 0; nFieldIndex < pEntity->GetFieldCount(); ++nFieldIndex )
	{
		int nDataOffset = buf.GetNumBitsRead();

		path = pEntity->GetFieldPath( nFieldIndex );
		pEntity->SetFieldDataBitOffset( nFieldIndex, nDataOffset - nStartBit ); // Offset from start of data payload

		const SendProp *pSendProp = pDecoder->GetSendProp( path );
		g_PropTypeFns[ pSendProp->GetType() ].SkipProp( pSendProp, &buf );
		// buffer now just after payload

		if ( bDTIEnabled )
		{
			DTI_HookDeltaBits( pDecoder, path, buf.GetNumBitsRead() - nDataOffset, fieldBits[ nFieldIndex ] );
		}
	}

	int nLastBit = buf.GetNumBitsRead();

	// Rewind
	buf.Seek( nStartBit );
	// Copy
	pEntity->PackWithFieldData( buf, nLastBit - nStartBit );
	// Put head back to end
	buf.Seek( nLastBit );

	return true;
}

bool RecvTable_ReadFieldList( 
	RecvTable *pTable, 

	bf_read &buf, 
	SerializedEntityHandle_t dest, 
	int nObjectId, 
	bool bUpdateDTI 
	)
{
	if ( g_bDTIEnabled && bUpdateDTI )
	{
		return RecvTable_ReadFieldList_Guts< true >( pTable, buf, dest, nObjectId );
	}

	return RecvTable_ReadFieldList_Guts< false >( pTable, buf, dest, nObjectId );
}


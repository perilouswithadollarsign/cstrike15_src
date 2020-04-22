//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_H
#define DATATABLE_H
#ifdef _WIN32
#pragma once
#endif


#include "dt_common.h"
#include "dt_recv_eng.h"
#include "dt_send_eng.h"
#include "utlvector.h"
#include "dt_encode.h"
#include "utlmap.h"
#include "tier1/bitbuf.h"
#include <algorithm>


class SendTable;
class RecvTable;
class CDTISendTable;


// (Temporary.. switch to something more efficient). Number of bits to
// encode property indices in the delta bits.
#define PROP_SENTINEL	0x7FFFFFFF

#define MAX_EXCLUDE_PROPS		512

class ExcludeProp
{
public:
	char const	*m_pTableName;
	char const	*m_pPropName;
};


#define PROP_INDICES_COLLECTION_NUM_INDICES 2
struct PropIndicesCollection_t
{
	unsigned short m_Indices[PROP_INDICES_COLLECTION_NUM_INDICES];
};


// ------------------------------------------------------------------------------------ //
// CDeltaBitsReader.
// ------------------------------------------------------------------------------------ //

class CDeltaBitsReader
{
public:
				CDeltaBitsReader( bf_read *pBuf );
				~CDeltaBitsReader();

	// Write the next property index. Returns the number of bits used.
	int		ReadNextPropIndex();

	// If you know you're done but you're not at the end (you haven't called until
	// ReadNextPropIndex returns -1), call this so it won't assert in its destructor.
	void		ForceFinished();

	int			GetFieldPathBits() const;
private:

	bf_read		*m_pBuf;
	bool		m_bFinished;
	int			m_iLastProp;
	int			m_nLastFieldPathBits;
	bool		m_bUsingNewScheme;
};


// ------------------------------------------------------------------------------------ //
// CDeltaBitsWriter.
// ------------------------------------------------------------------------------------ //

class CDeltaBitsWriter
{
public:
				CDeltaBitsWriter( bf_write *pBuf );
				~CDeltaBitsWriter();

	// Write the next property index. Returns the number of bits used.
	void		WritePropIndex( int iProp );

	// Access the buffer it's outputting to.
	bf_write*	GetBitBuf();

	void		Finish();

private:
	
	bf_write	*m_pBuf;
	int			m_iLastProp;
	bool		m_bUsingNewScheme;
};


inline bf_write* CDeltaBitsWriter::GetBitBuf()
{
	return m_pBuf;
}


// ----------------------------------------------------------------------------- //
// 
// CSendNode
//
// Each datatable gets a tree of CSendNodes. There is one CSendNode
// for each datatable property that was in the original SendTable.
//
// ----------------------------------------------------------------------------- //

class CSendNode
{
public:

					CSendNode();
					~CSendNode();

	int				GetNumChildren() const;
	CSendNode*		GetChild( int i ) const;
	
	
	// Returns true if the specified prop is in this node or any of its children.
	bool			IsPropInRecursiveProps( int i ) const;

	// Each datatable property (without SPROP_PROXY_ALWAYS_YES set) gets a unique index here.
	// The engine stores arrays of CSendProxyRecipients with the results of the proxies and indexes the results
	// with this index.
	//
	// Returns DATATABLE_PROXY_INDEX_NOPROXY if the property has SPROP_PROXY_ALWAYS_YES set.
	unsigned short	GetDataTableProxyIndex() const;
	void			SetDataTableProxyIndex( unsigned short val );

	// Similar to m_DataTableProxyIndex, but doesn't use DATATABLE_PROXY_INDEX_INVALID,
	// so this can be used to index CDataTableStack::m_pProxies. 
	unsigned short	GetRecursiveProxyIndex() const;
	void			SetRecursiveProxyIndex( unsigned short val );

public:

	// Child datatables.
	CUtlVector<CSendNode*>	m_Children;

	// The datatable property that leads us to this CSendNode.
	// This indexes the CSendTablePrecalc or CRecvDecoder's m_DatatableProps list.
	// The root CSendNode sets this to -1.
	short					m_iDatatableProp;

	// The SendTable that this node represents.
	// ALL CSendNodes have this.
	const SendTable	*m_pTable;

	//
	// Properties in this table.
	//

	// m_iFirstRecursiveProp to m_nRecursiveProps defines the list of propertise
	// of this node and all its children.
	unsigned short	m_iFirstRecursiveProp;
	unsigned short	m_nRecursiveProps;


	// See GetDataTableProxyIndex().
	unsigned short	m_DataTableProxyIndex;
	
	// See GetRecursiveProxyIndex().
	unsigned short	m_RecursiveProxyIndex;
};


inline int CSendNode::GetNumChildren() const
{
	return m_Children.Count(); 
}

inline CSendNode* CSendNode::GetChild( int i ) const
{
	return m_Children[i];
}


inline bool CSendNode::IsPropInRecursiveProps( int i ) const
{
	int index = i - (int)m_iFirstRecursiveProp;
	return index >= 0 && index < m_nRecursiveProps;
}

inline unsigned short CSendNode::GetDataTableProxyIndex() const
{
	Assert( m_DataTableProxyIndex != DATATABLE_PROXY_INDEX_INVALID );	// Make sure it's been set before.
	return m_DataTableProxyIndex;
}

inline void CSendNode::SetDataTableProxyIndex( unsigned short val )
{
	m_DataTableProxyIndex = val;
}

inline unsigned short CSendNode::GetRecursiveProxyIndex() const
{
	return m_RecursiveProxyIndex;
}

inline void CSendNode::SetRecursiveProxyIndex( unsigned short val )
{
	m_RecursiveProxyIndex = val;
}



class CFastLocalTransferPropInfo
{
public:
	unsigned short	m_iRecvOffset;
	unsigned short	m_iSendOffset;
	unsigned short	m_iProp;
};


class CFastLocalTransferInfo
{
public:
	CUtlVector<CFastLocalTransferPropInfo> m_FastInt32;
	CUtlVector<CFastLocalTransferPropInfo> m_FastInt16;
	CUtlVector<CFastLocalTransferPropInfo> m_FastInt8;
	CUtlVector<CFastLocalTransferPropInfo> m_FastVector;
	CUtlVector<CFastLocalTransferPropInfo> m_OtherProps;	// Props that must be copied slowly (proxies and all).
};


// ----------------------------------------------------------------------------- //
// CSendTablePrecalc
// ----------------------------------------------------------------------------- //
class CSendTablePrecalc
{
public:
						CSendTablePrecalc();
	virtual				~CSendTablePrecalc();

	// This function builds the flat property array given a SendTable.
	bool				SetupFlatPropertyArray();

	int					GetNumProps() const;
	const SendProp*		GetProp( int i ) const;

	int					GetNumDatatableProps() const;
	const SendProp*		GetDatatableProp( int i ) const;

	SendTable*			GetSendTable() const;
	CSendNode*			GetRootNode();

	int			GetNumDataTableProxies() const;
	void		SetNumDataTableProxies( int count );

	//given the offset of a property, this will return the property index, or -1 if it is not found
	//returns number of prop indexes that match
	#define MAX_PROP_INDEX_OFFSETS 4
	inline int GetPropertyIndexFromOffset( uint16 nOffset, int propOffsets[MAX_PROP_INDEX_OFFSETS] ) const;

public:

	class CProxyPathEntry
	{
	public:
		unsigned short m_iDatatableProp;	// Lookup into CSendTablePrecalc or CRecvDecoder::m_DatatableProps.
		unsigned short m_iProxy;
	};
	class CProxyPath
	{
	public:
		unsigned short m_iFirstEntry;	// Index into m_ProxyPathEntries.
		unsigned short m_nEntries;
	};
	
	CUtlVector<CProxyPathEntry> m_ProxyPathEntries;	// For each proxy index, this is all the DT proxies that generate it.
	CUtlVector<CProxyPath> m_ProxyPaths;			// CProxyPathEntries lookup into this.
	
	// These are what CSendNodes reference.
	// These are actual data properties (ints, floats, etc).
	CUtlVector<const SendProp*>	m_Props;

	// Each datatable in a SendTable's tree gets a proxy index, and its properties reference that.
	CUtlVector<unsigned char> m_PropProxyIndices;
	
	// CSendNode::m_iDatatableProp indexes this.
	// These are the datatable properties (SendPropDataTable).
	CUtlVector<const SendProp*>	m_DatatableProps;

	//we need an efficient way to map from a property's byte offset to the property index. To support this,
	//we have a sorted (on byte offset) list of property index and offset
	struct PropOffset_t
	{
		bool operator< ( const PropOffset_t& rhs ) const { return m_nOffset < rhs.m_nOffset; }
		uint16	m_nOffset;
		uint16	m_nIndex;
	};
	CUtlVector< PropOffset_t >	m_PropOffsetToIndex;

	// This is the property hierarchy, with the nodes indexing m_Props.
	CSendNode				m_Root;

	// From whence we came.
	SendTable				*m_pSendTable;

	// For instrumentation.
	CDTISendTable			*m_pDTITable;

	// This is precalculated in single player to allow faster direct copying of the entity data
	// from the server entity to the client entity.
	CFastLocalTransferInfo	m_FastLocalTransfer;

	// This tells how many data table properties there are without SPROP_PROXY_ALWAYS_YES.
	// Arrays allocated with this size can be indexed by CSendNode::GetDataTableProxyIndex().
	int						m_nDataTableProxies;
	
	// Map prop offsets to indices for properties that can use it.
	CUtlMap< unsigned short, PropIndicesCollection_t > m_PropOffsetToIndexMap;
};


inline int CSendTablePrecalc::GetNumProps() const
{
	return m_Props.Count(); 
}

inline const SendProp* CSendTablePrecalc::GetProp( int i ) const
{
	return m_Props[i]; 
}

inline int CSendTablePrecalc::GetNumDatatableProps() const
{
	return m_DatatableProps.Count();
}

inline const SendProp* CSendTablePrecalc::GetDatatableProp( int i ) const
{
	return m_DatatableProps[i];
}

inline SendTable* CSendTablePrecalc::GetSendTable() const
{
	return m_pSendTable; 
}

inline CSendNode* CSendTablePrecalc::GetRootNode()
{
	return &m_Root; 
}

inline int CSendTablePrecalc::GetNumDataTableProxies() const
{
	return m_nDataTableProxies;
}


inline void CSendTablePrecalc::SetNumDataTableProxies( int count )
{
	m_nDataTableProxies = count;
}					   

//given the offset of a property, this will return the property index, or -1 if it is not found
inline int CSendTablePrecalc::GetPropertyIndexFromOffset( uint16 nOffset, int propOffsets[MAX_PROP_INDEX_OFFSETS] ) const
{
	//do a binary search to find the lowest bound of our value (binary search is just a bool, and range does two checks)
	PropOffset_t Lookup;
	Lookup.m_nOffset = nOffset;
	const PropOffset_t* pBegin = m_PropOffsetToIndex.Base();
	const PropOffset_t* pEnd  = pBegin + m_PropOffsetToIndex.Count();

	const PropOffset_t* pProp = std::lower_bound( pBegin, pEnd, Lookup );

	//we can get properties that aren't in our map (such as a disabled var being set), so handle it not being in the map
	if( pProp == pEnd )
		return 0;

	int numMatchingProps = 0;
	do
	{
		propOffsets[numMatchingProps++] = pProp->m_nIndex;

		if ( numMatchingProps == MAX_PROP_INDEX_OFFSETS )
		{
			AssertMsg( false, "Too many properties matching same offset!" );
			return numMatchingProps;
		}

		++pProp;
	} while ( pProp != pEnd && pProp->m_nOffset == nOffset );

	return numMatchingProps;
}

// ------------------------------------------------------------------------ //
// Helpers.
// ------------------------------------------------------------------------ //

// Used internally by various datatable modules.
void DataTable_Warning( PRINTF_FORMAT_STRING const char *pInMessage, ... ) FMTFUNCTION( 1, 2 );
bool ShouldWatchThisProp( const SendTable *pTable, int objectID, const char *pPropName );
bool ShouldWatchThisProp( const RecvTable *pTable, int objectID, const char *pPropName );

// Same as AreBitArraysEqual but does a trivial test to make sure the 
// two arrays are equally sized.
bool CompareBitArrays(
	void const *pPacked1,
	void const *pPacked2,
	int nBits1,
	int nBits2
	);


// Helper routines for seeking through encoded buffers.
inline int NextProp( CDeltaBitsReader *pDeltaBitsReader )
{
	int iProp = pDeltaBitsReader->ReadNextPropIndex();
	if ( iProp >= 0 )
	{
		return iProp;
	}
	else
	{
		return PROP_SENTINEL;
	}
}

// to skip of a Property we just IsEncodedZero to read over it
// this is faster then doing a full Decode()
inline void SkipPropData( bf_read *pIn, const SendProp *pProp )
{
	g_PropTypeFns[ pProp->GetType() ].SkipProp( pProp, pIn );
}


// This is to be called on SendTables and RecvTables to setup array properties
// to point at their property templates and to set the SPROP_INSIDEARRAY flag
// on the properties inside arrays.
// We make the proptype an explicit template parameter because
// gcc templating cannot deduce typedefs from classes in templates properly

template< class TableType, class PropType >
void SetupArrayProps_R( TableType *pTable )
{
	// If this table has already been initialized in here, then jump out.
	if ( pTable->IsInitialized() )
		return;

	pTable->SetInitialized( true );

	for ( int i=0; i < pTable->GetNumProps(); i++ )
	{
		PropType *pProp = pTable->GetProp( i );

		if ( pProp->GetType() == DPT_Array )
		{
			ErrorIfNot( i >= 1,
				("SetupArrayProps_R: array prop '%s' is at index zero.", pProp->GetName())
			);

			// Get the property defining the elements in the array.
			PropType *pArrayProp = pTable->GetProp( i-1 );
			pArrayProp->SetInsideArray();
			pProp->SetArrayProp( pArrayProp );
		}
		else if ( pProp->GetType() == DPT_DataTable )
		{
			// Recurse into children datatables.
			SetupArrayProps_R<TableType,PropType>( pProp->GetDataTable() );
		}
	}
}


void FlushDeltaBitsTrackingData();
bool Sendprop_UsingDebugWatch();

#endif // DATATABLE_H

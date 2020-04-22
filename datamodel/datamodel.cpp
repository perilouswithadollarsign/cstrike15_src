//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "datamodel/idatamodel.h"
#include "datamodel/dmattributevar.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel.h"
#include "dependencygraph.h"
#include "dmattributeinternal.h"
#include "dmserializerkeyvalues.h"
#include "dmserializerkeyvalues2.h"
#include "dmserializerbinary.h"
#include "undomanager.h"
#include "clipboardmanager.h"
#include "DmElementFramework.h"
#include "vstdlib/iprocessutils.h"
#include "tier0/dbg.h"
#include "tier1/utlvector.h"
#include "tier1/utlqueue.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"
#include "tier2/utlstreambuffer.h"
#include "tier2/fileutils.h"
#include <time.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CUtlBuffer;
class IDmEditMessage;
class KeyValues;

#define UNNAMED_ELEMENT_NAME	"unnamed"



//-----------------------------------------------------------------------------
// Class factory for the default element
//-----------------------------------------------------------------------------
class CDmElementFactoryDefault : public IDmElementFactory
{
public:
	// Creation, destruction
	virtual CDmElement* Create( DmElementHandle_t handle, const char *pElementType, const char *pElementName, DmFileId_t fileid, const DmObjectId_t &id )
	{
		return new CDmElement( handle, pElementType, id, pElementName, fileid );
	}

	virtual void Destroy( DmElementHandle_t hElement )
	{
		if ( hElement != DMELEMENT_HANDLE_INVALID )
		{
			CDmElement *pElement = g_pDataModel->GetElement( hElement );
			delete static_cast<CDmElement*>( pElement );
		}
	}
	virtual void AddOnElementCreatedCallback( IDmeElementCreated *callback ) {};
	virtual void RemoveOnElementCreatedCallback( IDmeElementCreated *callback ) {};
	virtual void OnElementCreated( CDmElement* pElement ) {};
};

static CDmElementFactoryDefault s_DefaultElementFactory;



//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CDataModel g_DataModel;
CDataModel *g_pDataModelImp = &g_DataModel;
IDataModel *g_pDataModel = &g_DataModel;


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
CDataModel::CDataModel() :
	m_elementIds( 4096 ),
	m_unloadedIdElementMap( 16, 0, 0, ElementIdHandlePair_t::Compare, ElementIdHandlePair_t::HashKey )
{
	m_pDefaultFactory = &s_DefaultElementFactory;
	m_bUnableToSetDefaultFactory = false;
	m_bOnlyCreateUntypedElements = false;
	m_bUnableToCreateOnlyUntypedElements = false;
	m_pKeyvaluesCallbackInterface = NULL;
	m_nElementsAllocatedSoFar = 0;
	m_nMaxNumberOfElements = 0;
	m_bIsUnserializing = false;
	m_bDeleteOrphanedElements = false;
}

CDataModel::~CDataModel()
{
	m_UndoMgr.WipeUndo();

	if ( GetAllocatedElementCount() > 0 )
	{
		Warning( "Leaking %i elements\n", GetAllocatedElementCount() );
	}
}


//-----------------------------------------------------------------------------
// Methods of IAppSystem
//-----------------------------------------------------------------------------
bool CDataModel::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	if ( !factory( FILESYSTEM_INTERFACE_VERSION, NULL ) )
	{
		Warning( "DataModel needs the file system to function" );
		return false;
	}

	return true;
}


void *CDataModel::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strcmp( pInterfaceName, VDATAMODEL_INTERFACE_VERSION ) )
		return (IDataModel*)this;

	return NULL;
}

	
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *databasePath - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
InitReturnVal_t CDataModel::Init( )
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	InstallKeyValuesSerializer( this );
	InstallKeyValues2Serializer( this );
	InstallBinarySerializer( this );

	m_UndoMgr.SetUndoDepth( 256 );

	return INIT_OK;
}


//#define _ELEMENT_HISTOGRAM_
#ifdef _ELEMENT_HISTOGRAM_
CUtlMap< CUtlSymbolLarge, int > g_typeHistogram( 0, 100, DefLessFunc( CUtlSymbolLarge ) );
#endif _ELEMENT_HISTOGRAM_


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDataModel::Shutdown()
{
#ifdef _ELEMENT_HISTOGRAM_
	Msg( "element type histogram for %d elements allocated so far:\n", GetElementsAllocatedSoFar() );
	for ( int i = g_typeHistogram.FirstInorder(); g_typeHistogram.IsValidIndex( i ); i = g_typeHistogram.NextInorder( i ) )
	{
		Msg( "%d\t%s\n", g_typeHistogram.Element( i ), GetString( g_typeHistogram.Key( i ) ) );
	}
	Msg( "\n" );
#endif _ELEMENT_HISTOGRAM_

	int c = GetAllocatedElementCount();
	if ( c > 0 )
	{
		Warning( "CDataModel:  %i elements left in memory!!!\n", c );
	}

	m_Factories.Purge();
	m_Serializers.Purge();
	m_UndoMgr.Shutdown();
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Sets the undo context size
//-----------------------------------------------------------------------------
void CDataModel::SetUndoDepth( int nSize )
{
	m_UndoMgr.SetUndoDepth( nSize );
}


//-----------------------------------------------------------------------------
// force creation of untyped elements, ignoring type
//-----------------------------------------------------------------------------
void CDataModel::OnlyCreateUntypedElements( bool bEnable )
{
	if ( m_bUnableToCreateOnlyUntypedElements )
	{
		Assert( 0 );
		return;
	}

	m_bOnlyCreateUntypedElements = bEnable;
}

int CDataModel::GetElementsAllocatedSoFar()
{
	return m_nElementsAllocatedSoFar;
}

int CDataModel::GetMaxNumberOfElements()
{
	return m_nMaxNumberOfElements;
}

int CDataModel::GetAllocatedAttributeCount()
{
	return ::GetAllocatedAttributeCount();
}


//-----------------------------------------------------------------------------
// Returns the total number of elements allocated at the moment
//-----------------------------------------------------------------------------
int CDataModel::GetAllocatedElementCount()
{
	return ( int )m_Handles.GetValidHandleCount();
}

DmElementHandle_t CDataModel::FirstAllocatedElement()
{
	int nHandles = ( int )m_Handles.GetHandleCount();
	for ( int i = 0; i < nHandles; ++i )
	{
		DmElementHandle_t hElement = ( DmElementHandle_t )m_Handles.GetHandleFromIndex( i );
		if ( CDmElement *pElement = GetElement( hElement ) )
			return hElement;
	}
	return DMELEMENT_HANDLE_INVALID;
}

DmElementHandle_t CDataModel::NextAllocatedElement( DmElementHandle_t hElement )
{
	int nHandles = ( int )m_Handles.GetHandleCount();
	for ( int i = m_Handles.GetIndexFromHandle( hElement ) + 1; i < nHandles; ++i )
	{
		DmElementHandle_t hElement = ( DmElementHandle_t )m_Handles.GetHandleFromIndex( i );
		if ( CDmElement *pElement = GetElement( hElement ) )
			return hElement;
	}

	return DMELEMENT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// estimate memory overhead
//-----------------------------------------------------------------------------
int CDataModel::EstimateMemoryOverhead() const
{
	int nHandlesOverhead = sizeof( int ) + sizeof( CDmElement* ); // m_Handles
	int nElementIdsOverhead = sizeof( DmElementHandle_t ); // this also has a 80k static overhead, since hash tables can't grow
	return nHandlesOverhead + nElementIdsOverhead;
}

static bool HandleCompare( const DmElementHandle_t & a, const DmElementHandle_t &b )
{
	return a == b;
}

static unsigned int HandleHash( const DmElementHandle_t &h )
{
	return (unsigned int)h;
}

int CDataModel::EstimateMemoryUsage( DmElementHandle_t hElement, TraversalDepth_t depth )
{
	CUtlHash< DmElementHandle_t > visited( 1024, 0, 0, HandleCompare, HandleHash );
	CDmElement *pElement = m_Handles.GetHandle( hElement );
	if ( !pElement )
		return 0;

	return CDmeElementAccessor::EstimateMemoryUsage( pElement, visited, depth, NULL );
}


//-----------------------------------------------------------------------------
// Displays stats for datamodel
//-----------------------------------------------------------------------------
struct DmMemoryInfo_t
{
	int m_nCount;
	int m_nSize;
	int m_pCategories[ MEMORY_CATEGORY_COUNT ];
};

struct DmMemorySortInfo_t
{
	int m_nIndex;
	int m_nTotalSize;
};

int DmMemorySortFunc( const void * lhs, const void * rhs )
{
	DmMemorySortInfo_t &info1 = *(DmMemorySortInfo_t*)lhs;
	DmMemorySortInfo_t &info2 = *(DmMemorySortInfo_t*)rhs;
	return info1.m_nTotalSize - info2.m_nTotalSize;
}

// this method assumes that all elements that should be traversed have a parity of !bParity
static void GatherElements_R( CDmElement *pElement, CUtlVector< DmElementHandle_t > &list, CUtlHash< DmElementHandle_t > &visited )
{
	DmElementHandle_t h = pElement->GetHandle();

	if ( visited.Find( h ) != visited.InvalidHandle() )
		return;
	visited.Insert( h );

	list.AddToTail( h );

	for ( const CDmAttribute *pAttr = pElement->FirstAttribute(); pAttr != NULL; pAttr = pAttr->NextAttribute() )
	{
		if ( pAttr->GetType() == AT_ELEMENT )
		{
			CDmElement *pChild = pAttr->GetValueElement<CDmElement>();
			if ( !pChild )
				continue;
			GatherElements_R( pChild, list, visited );
		}
		else if ( pAttr->GetType() == AT_ELEMENT_ARRAY )
		{
			const CDmrElementArrayConst<> elementArrayAttr( pAttr );
			int nChildren = elementArrayAttr.Count();
			for ( int i = 0; i < nChildren; ++i )
			{
				CDmElement *pChild = elementArrayAttr[ i ];
				if ( !pChild )
					continue;
				GatherElements_R( pChild, list, visited );
			}
		}
	}
}

void CDataModel::BuildHistogramForHandles( CUtlMap< CUtlSymbolLarge, DmMemoryInfo_t > &typeHistogram, CUtlVector< DmElementHandle_t > &handles )
{
	CUtlHash< DmElementHandle_t > visited( 1024, 0, 0, HandleCompare, HandleHash );

	int c = handles.Count();
	for ( int i = 0; i < c; ++i )
	{
		DmElementHandle_t h = handles[ i  ];
		if ( h == DMELEMENT_HANDLE_INVALID )
			continue;

		CDmElement *pElement = m_Handles.GetHandle( h );
		if ( !pElement )
			continue;

		unsigned short j = typeHistogram.Find( pElement->GetType() );
		if ( !typeHistogram.IsValidIndex( j ) )
		{
			j = typeHistogram.Insert( pElement->GetType() );
			typeHistogram[j].m_nCount = 0;
			typeHistogram[j].m_nSize = 0;
			memset( typeHistogram[j].m_pCategories, 0, sizeof(typeHistogram[j].m_pCategories) );
		}

		int nMemory = CDmeElementAccessor::EstimateMemoryUsage( pElement, visited, TD_NONE, typeHistogram[j].m_pCategories );

		++typeHistogram[j].m_nCount;
		typeHistogram[j].m_nSize += nMemory;
	}
}

void CDataModel::DisplayMemoryStats( DmElementHandle_t hElement /*= DMELEMENT_HANDLE_INVALID*/ )
{
	CUtlMap< CUtlSymbolLarge, DmMemoryInfo_t > typeHistogram( 0, 100, DefLessFunc( CUtlSymbolLarge ) );
	CUtlHash< DmElementHandle_t > visited( 1024, 0, 0, HandleCompare, HandleHash );

	CUtlVector< DmElementHandle_t > handles;

	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		int c = (int)m_Handles.GetHandleCount();
		for ( int i = 0; i < c; ++i )
		{
			DmElementHandle_t h = (DmElementHandle_t)m_Handles.GetHandleFromIndex( i );
			if ( !m_Handles.IsHandleValid( h ) )
				continue;
			handles.AddToTail( h );
		}
	}
	else
	{
		// Do a recursive build
		if ( !m_Handles.IsHandleValid( hElement ) )
		{
			Msg( "CDataModel::DisplayMemoryStats with invalid handle %u\n", (int)hElement );
			return;
		}

		CDmElement *pElement = m_Handles.GetHandle( hElement );
		if( pElement )
		{
			GatherElements_R( pElement, handles, visited );
		}
	}

	BuildHistogramForHandles( typeHistogram, handles );

	// Sort
	DmMemorySortInfo_t* pSortInfo = (DmMemorySortInfo_t*)_alloca( typeHistogram.Count() * sizeof(DmMemorySortInfo_t) );
	int nCount = 0;
	for ( int i = typeHistogram.FirstInorder(); typeHistogram.IsValidIndex( i ); i = typeHistogram.NextInorder( i ) )
	{
		pSortInfo[nCount].m_nIndex = i;
		pSortInfo[nCount].m_nTotalSize = typeHistogram.Element( i ).m_nSize;
		++nCount;
	}
	qsort( pSortInfo, nCount, sizeof(DmMemorySortInfo_t), DmMemorySortFunc );
	     
	int pTotals[ MEMORY_CATEGORY_COUNT ] = { 0 };
	int nLastTwoSize = 0;
	int nTotalSize = 0;
	int nTotalCount = 0;
	int nTotalData = 0;
	ConMsg( "Dm Memory usage: type\t\t\t\tcount\ttotalsize\twastage %%\touter\t\tinner\t\tdatamodel\trefs\t\ttree\t\tatts\t\tdata\t(att count)\n" );
	for ( int i = 0; i < nCount; ++i )
	{
		const DmMemoryInfo_t& info = typeHistogram.Element( pSortInfo[i].m_nIndex );
		float flPercentOverhead = 1.0f - ( ( info.m_nSize != 0 ) ? ( (float)info.m_pCategories[MEMORY_CATEGORY_ATTRIBUTE_DATA] / (float)info.m_nSize ) : 0.0f );
		flPercentOverhead *= 100.0f;
		 
		ConMsg( "%-40s\t%6d\t%9d\t\t%5.2f", typeHistogram.Key( pSortInfo[i].m_nIndex ).String(), 
			info.m_nCount, info.m_nSize, flPercentOverhead );
		int nTotal = 0;
		for ( int j = 0; j < MEMORY_CATEGORY_COUNT; ++j )
		{
			ConColorMsg( Color( 255, 192, 0, 255 ), "\t%8d", info.m_pCategories[j] );
			if ( j != MEMORY_CATEGORY_ATTRIBUTE_COUNT )
			{
				nTotal += info.m_pCategories[j];
			}
			pTotals[j] += info.m_pCategories[j];
		}
		ConMsg( "\n" );
		Assert( nTotal == info.m_nSize );
		nTotalSize += info.m_nSize;
		nTotalCount += info.m_nCount;
		nTotalData += info.m_pCategories[MEMORY_CATEGORY_ATTRIBUTE_DATA];

		if ( i >= nCount - 2 )
		{
			nLastTwoSize += info.m_nSize;
		}
	}
	  
	ConMsg( "\n" );
	ConMsg( "%-40s\t%6d\t%9d\t\t%5.2f", "Totals", nTotalCount, nTotalSize, 100.0f * ( 1.0f - (float)nTotalData / (float)nTotalSize ) );
	for ( int j = 0; j < MEMORY_CATEGORY_COUNT; ++j )
	{
		ConColorMsg( Color( 255, 192, 0, 255 ), "\t%8d", pTotals[j] );
	}

	ConMsg( "\n" );

	float flPercent = 0.0f;
	if ( nTotalSize > 0 )
	{
		flPercent =  100.0 *((float)nLastTwoSize/(float)nTotalSize);
	}

	ConMsg( "Last two rows as percentage of total size %f %%:\n", flPercent );

	uint64 symbolBytes = m_SymbolTable.GetMemoryUsage();
	int symbolCount = m_SymbolTable.GetNumStrings();

	// Estimate string table usage
	ConMsg( "Symbol table:  %d entries %I64u bytes\n", symbolCount, symbolBytes );
}




void IncrementSymbolRefCount( CUtlSymbolLarge symbol, CUtlMap< CUtlSymbolLarge, int, int > &refCountMap )
{
	int nIndex = refCountMap.Find( symbol );
	if ( nIndex == refCountMap.InvalidIndex() )
	{
		nIndex = refCountMap.Insert( symbol );
		refCountMap[ nIndex ] = 1;
	}
	else
	{
		refCountMap[ nIndex ]++;
	}
}

int CompareSymbolsAcending( const CUtlSymbolLarge *symA, const CUtlSymbolLarge *symB )
{
	return V_stricmp( symA->String(), symB->String() );
}

// Dump the symbol table to the console
void CDataModel::DumpSymbolTable()
{
	uint64 symbolBytes = m_SymbolTable.GetMemoryUsage();
	int symbolCount = m_SymbolTable.GetNumStrings();

	// Estimate string table usage
	ConMsg( "Datamodel symbol table: %d entries %I64u bytes\n", symbolCount, symbolBytes );

	CUtlMap< CUtlSymbolLarge, int, int > symbolRefCountMap( 0, symbolCount, DefLessFunc( CUtlSymbolLarge ) );
	
	int nNumElements = (int)m_Handles.GetHandleCount();
	for ( int iElement = 0; iElement < nNumElements; ++iElement )
	{
		CDmElement *pElement = m_Handles.GetHandle( iElement );
		if ( pElement == NULL )
			continue;
			
		IncrementSymbolRefCount( pElement->GetType(), symbolRefCountMap );
	
		for ( CDmAttribute *pAttr = pElement->FirstAttribute(); pAttr; pAttr = pAttr->NextAttribute() )
		{
			IncrementSymbolRefCount( pAttr->GetNameSymbol(), symbolRefCountMap );

			if ( pAttr->GetType() == AT_STRING )
			{
				IncrementSymbolRefCount( pAttr->GetValue< CUtlSymbolLarge >(), symbolRefCountMap );
			}
		}
	}
	
	CUtlBuffer buf( 0, symbolBytes + ( symbolCount * 32 ), CUtlBuffer::TEXT_BUFFER );

	buf.Printf( "Datamodel symbol table: %d entries %I64u bytes\n", symbolCount, symbolBytes );
	CUtlVector< CUtlSymbolLarge > elements;
	elements.EnsureCount( symbolCount );
	m_SymbolTable.GetElements( 0, symbolCount, elements.Base() );
	elements.Sort( CompareSymbolsAcending );

	
	buf.Printf( "Index      Address       References   String\n" );
	for ( int i = 0; i < symbolCount; ++i )
	{
		CUtlSymbolLarge symbol = elements[ i ];
		int nIndex = symbolRefCountMap.Find( symbol );
		int nRefCount = ( nIndex != symbolRefCountMap.InvalidIndex() ) ? symbolRefCountMap[ nIndex ] : 0;

		const char *pString = symbol.String();
		buf.Printf( "%05d,     0x%p,     %5d,      '%s'\n", i, pString, nRefCount, pString );
	}
	
	int nFileIndex = 0;
	int nMaxFileIndex = 1000;
	const char baseFilename[] = "DatamodelSymbols";
	CUtlString filename;

	char directory[ 256 ];
	g_pFullFileSystem->GetCurrentDirectory( directory, sizeof( directory ) );

	while ( nFileIndex < nMaxFileIndex )
	{
		filename = CFmtStr( "%s\\%s%i.txt", directory, baseFilename, nFileIndex );
		if ( !g_pFullFileSystem->FileExists( filename ) )
			break;
		++nFileIndex;
	}	

	if ( nFileIndex < nMaxFileIndex )
	{
		g_pFullFileSystem->WriteFile( filename, NULL, buf );
		Msg( "Wrote symbol stats to file: %s", filename.Get() );
	}
}

//-----------------------------------------------------------------------------
// Global symbol table for the datamodel system
//-----------------------------------------------------------------------------
CUtlSymbolLarge CDataModel::GetSymbol( const char *pString )
{
	return m_SymbolTable.AddString( pString );
}


//-----------------------------------------------------------------------------
// file format methods
//-----------------------------------------------------------------------------
const char* CDataModel::GetFormatExtension( const char *pFormatName )
{
	IDmFormatUpdater *pUpdater = FindFormatUpdater( pFormatName );
	Assert( pUpdater );
	if ( !pUpdater )
		return NULL;

	return pUpdater->GetExtension();
}

const char* CDataModel::GetFormatDescription( const char *pFormatName )
{
	IDmFormatUpdater *pUpdater = FindFormatUpdater( pFormatName );
	Assert( pUpdater );
	if ( !pUpdater )
		return NULL;

	return pUpdater->GetDescription();
}

int CDataModel::GetFormatCount() const
{
	return m_FormatUpdaters.Count();
}

const char* CDataModel::GetFormatName( int i ) const
{
	IDmFormatUpdater *pUpdater = m_FormatUpdaters[ i ];
	if ( !pUpdater )
		return NULL;

	return pUpdater->GetName();
}

const char *CDataModel::GetDefaultEncoding( const char *pFormatName )
{
	IDmFormatUpdater *pUpdater = FindFormatUpdater( pFormatName );
	if ( !pUpdater )
		return NULL;

	return pUpdater->GetDefaultEncoding();
}

//-----------------------------------------------------------------------------
// Adds various serializers
//-----------------------------------------------------------------------------
void CDataModel::AddSerializer( IDmSerializer *pSerializer )
{
	Assert( Q_strlen( pSerializer->GetName() ) <= DMX_MAX_FORMAT_NAME_MAX_LENGTH );

	if ( FindSerializer( pSerializer->GetName() ) )
	{
		Warning("Attempted to add two serializers with the same file encoding (%s)!\n", pSerializer->GetName() );
		return;
	}

	m_Serializers.AddToTail( pSerializer );
}

void CDataModel::AddLegacyUpdater( IDmLegacyUpdater *pUpdater )
{
	Assert( Q_strlen( pUpdater->GetName() ) <= DMX_MAX_FORMAT_NAME_MAX_LENGTH );

	if ( FindLegacyUpdater( pUpdater->GetName() ) )
	{
		Warning( "Attempted to add two legacy updaters with the same file format (%s)!\n", pUpdater->GetName() );
		return;
	}

	m_LegacyUpdaters.AddToTail( pUpdater );
}

void CDataModel::AddFormatUpdater( IDmFormatUpdater *pUpdater )
{
	Assert( Q_strlen( pUpdater->GetName() ) <= DMX_MAX_FORMAT_NAME_MAX_LENGTH );

	if ( FindFormatUpdater( pUpdater->GetName() ) )
	{
		Warning( "Attempted to add two format updaters with the same file format (%s)!\n", pUpdater->GetName() );
		return;
	}

	m_FormatUpdaters.AddToTail( pUpdater );
}

//-----------------------------------------------------------------------------
// encoding-related methods
//-----------------------------------------------------------------------------
int CDataModel::GetEncodingCount() const
{
	return m_Serializers.Count();
}

const char *CDataModel::GetEncodingName( int i ) const
{
	return m_Serializers[ i ]->GetName();
}

bool CDataModel::IsEncodingBinary( const char *pEncodingName ) const
{
	IDmSerializer *pSerializer = FindSerializer( pEncodingName );
	if ( !pSerializer )
	{
		Warning("Serialize: File encoding %s is undefined!\n", pEncodingName );
		return false;
	}
	return pSerializer->IsBinaryFormat(); 
}

bool CDataModel::DoesEncodingStoreVersionInFile( const char *pEncodingName ) const
{
	IDmSerializer *pSerializer = FindSerializer( pEncodingName );
	if ( !pSerializer )
	{
		Warning("Serialize: File encoding %s is undefined!\n", pEncodingName );
		return false;
	}
	return pSerializer->StoresVersionInFile(); 
}


IDmSerializer* CDataModel::FindSerializer( const char *pEncodingName ) const
{
	int nSerializers = m_Serializers.Count();
	for ( int i = 0; i < nSerializers; ++i )
	{
		IDmSerializer *pSerializer = m_Serializers[ i ];
		Assert( pSerializer );
		if ( !pSerializer )
			continue;

		if ( !V_strcmp( pEncodingName, pSerializer->GetName() ) )
			return pSerializer;
	}

	return NULL;
}

IDmLegacyUpdater* CDataModel::FindLegacyUpdater( const char *pLegacyFormatName ) const
{
	int nUpdaters = m_LegacyUpdaters.Count();
	for ( int i = 0; i < nUpdaters; ++i )
	{
		IDmLegacyUpdater *pUpdater = m_LegacyUpdaters[ i ];
		Assert( pUpdater );
		if ( !pUpdater )
			continue;

		if ( !V_strcmp( pLegacyFormatName, pUpdater->GetName() ) )
			return pUpdater;
	}

	return NULL;
}

IDmFormatUpdater* CDataModel::FindFormatUpdater( const char *pFormatName ) const
{
	int nUpdaters = m_FormatUpdaters.Count();
	for ( int i = 0; i < nUpdaters; ++i )
	{
		IDmFormatUpdater *pUpdater = m_FormatUpdaters[ i ];
		Assert( pUpdater );
		if ( !pUpdater )
			continue;

		if ( !V_strcmp( pFormatName, pUpdater->GetName() ) )
			return pUpdater;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the name of the DME element to create in keyvalues serialization
//-----------------------------------------------------------------------------
void CDataModel::SetKeyValuesElementCallback( IElementForKeyValueCallback *pCallbackInterface )
{
	m_pKeyvaluesCallbackInterface = pCallbackInterface;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CDataModel::GetKeyValuesElementName( const char *pszKeyName, int iNestingLevel )
{
	if ( m_pKeyvaluesCallbackInterface )
		return m_pKeyvaluesCallbackInterface->GetElementForKeyValue( pszKeyName, iNestingLevel );

	return NULL;
}


//-----------------------------------------------------------------------------
// For serialization, set the delimiter rules
//-----------------------------------------------------------------------------
void CDataModel::SetSerializationDelimiter( CUtlCharConversion *pConv )
{
	::SetSerializationDelimiter( pConv );
}

void CDataModel::SetSerializationArrayDelimiter( const char *pDelimiter )
{
	::SetSerializationArrayDelimiter( pDelimiter );
}

bool CDataModel::SaveToFile( char const *pFileName, char const *pPathID, const char *pEncodingName, const char *pFormatName, CDmElement *pRoot )
{
	// NOTE: This guarantees full path names for pathids
	char pFullPath[ MAX_PATH ];
	if ( !GenerateFullPath( pFileName, pPathID, pFullPath, sizeof( pFullPath ) ) )
	{
		Warning( "CDataModel: Unable to generate full path for file %s\n", pFileName );
		return false;
	}

	if ( g_pFullFileSystem->FileExists( pFullPath, pPathID ) )
	{
		if ( !g_pFullFileSystem->IsFileWritable( pFullPath, pPathID ) )
		{
			Warning( "CDataModel: Unable to overwrite readonly file %s\n", pFullPath );
			return false;
		}
	}

	if ( !pEncodingName )
	{
		pEncodingName = GetDefaultEncoding( pFormatName );
	}

	bool bIsBinary = IsEncodingBinary( pEncodingName );
	if ( bIsBinary )
	{
		CUtlStreamBuffer buf( pFullPath, pPathID, 0, true );
		if ( !buf.IsValid() )
		{
			Warning( "CDataModel: Unable to open file \"%s\"\n", pFullPath );
			return false;
		}

		bool bOk = Serialize( buf, pEncodingName, pFormatName, pRoot->GetHandle() );
		if ( !bOk )
			return false;

	}
	else
	{
		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

		bool bOk = Serialize( buf, pEncodingName, pFormatName, pRoot->GetHandle() );
		if ( !bOk )
			return false;

		if ( !g_pFullFileSystem->WriteFile( pFullPath, pPathID, buf ) )
		{
			Warning( "CDataModel: Unable to open file \"%s\"\n", pFullPath );
			return false;
		}
	}

	DmFileId_t fileid = GetFileId( pFullPath );
	if ( fileid != DMFILEID_INVALID )
	{
		SetFileModificationUTCTime( fileid, GetCurrentUTCTime() );
	}

	return true;	
}

DmFileId_t CDataModel::RestoreFromFile( char const *pFileName, char const *pPathID, const char *pEncodingHint, CDmElement **ppRoot, DmConflictResolution_t idConflictResolution /*= CR_DELETE_NEW*/, DmxHeader_t *pHeaderOut /*= NULL*/ )
{
	// NOTE: This guarantees full path names for pathids
	char pFullPath[ MAX_PATH ];
	if ( !GenerateFullPath( pFileName, pPathID, pFullPath, sizeof( pFullPath ) ) )
	{
		Warning( "CDataModel: Unable to generate full path for file %s\n", pFileName );
		return DMFILEID_INVALID;
	}

	char *pTemp = (char*)_alloca( DMX_MAX_HEADER_LENGTH + 1 );
	CUtlBuffer typeBuf( pTemp, DMX_MAX_HEADER_LENGTH );
	if ( !g_pFullFileSystem->ReadFile( pFullPath, pPathID, typeBuf, DMX_MAX_HEADER_LENGTH ) )
	{
		Warning( "CDataModel: Unable to open file %s\n", pFullPath );
		return DMFILEID_INVALID;
	}

	DmxHeader_t _header;
	DmxHeader_t *pHeader = pHeaderOut ? pHeaderOut : &_header;
	bool bSuccess = ReadDMXHeader( typeBuf, pHeader );
	if ( !bSuccess )
	{
		if ( !pEncodingHint )
		{
			Warning( "CDataModel: Unable to determine DMX encoding for file %s\n", pFullPath );
			return DMFILEID_INVALID;
		}

		if ( !IsValidNonDMXFormat( pEncodingHint ) )
		{
			Warning( "CDataModel: Invalid DMX encoding hint '%s' for file %s\n", pEncodingHint, pFullPath );
			return DMFILEID_INVALID;
		}

		// non-dmx file importers don't have versions or formats, just encodings
		V_strncpy( pHeader->encodingName, pEncodingHint, sizeof( pHeader->encodingName ) );
		pHeader->formatName[0] = 0;
	}

	DmElementHandle_t hRootElement;
	bool bIsBinary = IsEncodingBinary( pHeader->encodingName );
	if ( bIsBinary )
	{
		CUtlStreamBuffer buf( pFullPath, pPathID, CUtlBuffer::READ_ONLY );
		if ( !buf.IsValid() )
		{
			Warning( "CDataModel: Unable to open file '%s'\n", pFullPath );
			return DMFILEID_INVALID;
		}

		if ( !Unserialize( buf, pHeader->encodingName, pHeader->formatName, pEncodingHint, pFullPath, idConflictResolution, hRootElement ) )
			return DMFILEID_INVALID;

	}
	else
	{
		CUtlBuffer buf( 0, 0, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );

		if ( !g_pFullFileSystem->ReadFile( pFullPath, pPathID, buf ) )
		{
			Warning( "CDataModel: Unable to open file '%s'\n", pFullPath );
			return DMFILEID_INVALID;
		}

		if ( !Unserialize( buf, pHeader->encodingName, pHeader->formatName, pEncodingHint, pFullPath, idConflictResolution, hRootElement ) )
			return DMFILEID_INVALID;
	}

	

	*ppRoot = g_pDataModel->GetElement( hRootElement );

	DmFileId_t fileid = g_pDataModel->GetFileId( pFullPath );
	Assert( fileid != DMFILEID_INVALID );
	if ( fileid != DMFILEID_INVALID )
	{
		SetFileModificationUTCTime( fileid, g_pFullFileSystem->GetFileTime( pFullPath ) );
	}

	return fileid;
}


//-----------------------------------------------------------------------------
// Is this a DMX file format?
//-----------------------------------------------------------------------------
bool CDataModel::IsDMXFormat( CUtlBuffer &buf )	const
{
	DmxHeader_t header;
	bool bSuccess = ReadDMXHeader( buf, &header );
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	return bSuccess;
}


//-----------------------------------------------------------------------------
// Serialization of a element tree into a utlbuffer
//-----------------------------------------------------------------------------
bool CDataModel::Serialize( CUtlBuffer &outBuf, const char *pEncodingName, const char *pFormatName, DmElementHandle_t hRoot )
{
	if ( !pEncodingName )
	{
		pEncodingName = outBuf.IsText() ? "keyvalues2" : "binary";
	}

	// Find a serializer appropriate for the file format.
	IDmSerializer *pSerializer = FindSerializer( pEncodingName );
	if ( !pSerializer )
	{
		Warning("Serialize: File encoding '%s' is undefined!\n", pEncodingName );
		return false;
	}

	// Ensure the utlbuffer is in the appropriate format (binary/text)
	bool bIsText = outBuf.IsText();
	bool bIsCRLF = outBuf.ContainsCRLF();

	CUtlBuffer outTextBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	CUtlBuffer *pActualOutBuf = &outBuf;

	if ( pSerializer->IsBinaryFormat() )
	{
		if ( outBuf.IsText() )
		{
			if ( !outBuf.ContainsCRLF() )
			{
				Warning( "Serialize: Format %s expects to be written to a binary format, but the buffer is a text-format buffer\n", pFormatName );
				return false;
			}
			outBuf.SetBufferType( false, false );
		}
	}
	else
	{
		// If we want text, but the binbuf is binary; recast it to a text buffer w/ CRLF
		if ( !outBuf.IsText() )
		{
			outBuf.SetBufferType( true, true );
		}

		if ( outBuf.ContainsCRLF() )
		{
			// If we want text, but the binbuf expects CRLF, then we must do a conversion pass
			pActualOutBuf = &outTextBuffer;
		}
	}

	if ( pSerializer->StoresVersionInFile() )
	{
		// Write the format name into the file using XML format so that 
		// 3rd-party XML readers can read the file without fail

		pActualOutBuf->Printf( "%s encoding %s %d format %s %d %s\n",
								DMX_VERSION_STARTING_TOKEN, pEncodingName, pSerializer->GetCurrentVersion(),
								pFormatName, GetCurrentFormatVersion( pFormatName ), DMX_VERSION_ENDING_TOKEN );
	}

	// Now write the file using the appropriate format
	CDmElement *pRoot = GetElement( hRoot );
	bool bOk = pSerializer->Serialize( *pActualOutBuf, pRoot );
	if ( bOk )
	{
		if ( pActualOutBuf == &outTextBuffer )
		{
			outTextBuffer.ConvertCRLF( outBuf );
		}
	}

	outBuf.SetBufferType( bIsText, bIsCRLF );
	return bOk;
}


//-----------------------------------------------------------------------------
// Read the header, return the version (or false if it's not a DMX file)
//-----------------------------------------------------------------------------
bool CDataModel::ReadDMXHeader( CUtlBuffer &inBuf, DmxHeader_t *pHeader ) const
{
	Assert( pHeader );
	if ( !pHeader )
		return false;

	// Make the buffer capable of being read as text
	bool bIsText = inBuf.IsText();
	bool bHasCRLF = inBuf.ContainsCRLF();
	inBuf.SetBufferType( true, !bIsText || bHasCRLF );

	char headerStr[ DMX_MAX_HEADER_LENGTH ];
	bool bOk = inBuf.ParseToken( DMX_VERSION_STARTING_TOKEN, DMX_VERSION_ENDING_TOKEN, headerStr, sizeof( headerStr ) );
	if ( bOk )
	{
#ifdef _WIN32
		int nAssigned = sscanf_s( headerStr, "encoding %s %d format %s %d\n",
			pHeader->encodingName, DMX_MAX_FORMAT_NAME_MAX_LENGTH, &( pHeader->nEncodingVersion ),
			pHeader->formatName, DMX_MAX_FORMAT_NAME_MAX_LENGTH, &( pHeader->nFormatVersion ) );
#else
		int nAssigned = sscanf( headerStr, "encoding %s %d format %s %d\n",
			pHeader->encodingName, &( pHeader->nEncodingVersion ),
			pHeader->formatName, &( pHeader->nFormatVersion ) );
#endif
		bOk = nAssigned == 4;
	}

	if ( !bOk )
	{
		inBuf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
		bOk = inBuf.ParseToken( DMX_LEGACY_VERSION_STARTING_TOKEN, DMX_LEGACY_VERSION_ENDING_TOKEN, pHeader->formatName, DMX_MAX_FORMAT_NAME_MAX_LENGTH );
		if ( bOk )
		{
			const char *pEncoding = GetEncodingFromLegacyFormat( pHeader->formatName );
			if ( pEncoding )
			{
				V_strncpy( pHeader->encodingName, pEncoding, DMX_MAX_FORMAT_NAME_MAX_LENGTH );
				pHeader->nEncodingVersion = 0; // the first encoding version
				pHeader->nFormatVersion = -1; // this value is ignored for legacy formats
			}
			else
			{
				bOk = false;
			}
		}
	}

	inBuf.SetBufferType( bIsText, bHasCRLF );
	return bOk;
}

const char *CDataModel::GetEncodingFromLegacyFormat( const char *pLegacyFormatName ) const
{
	if ( StringHasPrefixCaseSensitive( pLegacyFormatName, "binary_v" ) )
		return "binary";
	if ( StringHasPrefixCaseSensitive( pLegacyFormatName, "sfm_v" ) )
		return "binary";
	if ( StringHasPrefixCaseSensitive( pLegacyFormatName, "keyvalues2_v" ) )
		return "keyvalues2";
	if ( StringHasPrefixCaseSensitive( pLegacyFormatName, "keyvalues2_flat_v" ) )
		return "keyvalues2_flat";
	return NULL;
}

bool CDataModel::IsLegacyFormat( const char *pFormatName ) const
{
	return GetEncodingFromLegacyFormat( pFormatName ) != NULL;
}

bool CDataModel::IsValidNonDMXFormat( const char *pEncodingName ) const
{
	IDmSerializer *pSerializer = FindSerializer( pEncodingName );
	return pSerializer && !pSerializer->StoresVersionInFile();
}


// used to skip auto-creation of child elements during unserialization
bool CDataModel::IsUnserializing()
{
	return m_bIsUnserializing;
}

int CDataModel::GetCurrentFormatVersion( const char *pFormatName )
{
	IDmFormatUpdater *pUpdater = FindFormatUpdater( pFormatName );
	if ( !pUpdater )
		return -1; // invalid version #

	return pUpdater->GetCurrentVersion();
}

//-----------------------------------------------------------------------------
// Unserializes, returns the root of the unserialized tree in ppRoot 
//-----------------------------------------------------------------------------
bool CDataModel::Unserialize( CUtlBuffer &inBuf, const char *pEncodingName, const char *pRequiredFormat, const char *pUnused,
							  const char *pFileName, DmConflictResolution_t idConflictResolution, DmElementHandle_t &hRoot )
{
	ClearUndo();
	CDisableUndoScopeGuard sg;

	if ( !pEncodingName )
	{
		pEncodingName = inBuf.IsText() ? "keyvalues2" : "binary";
	}

	// Find a serializer appropriate for the file format.
	IDmSerializer *pSerializer = FindSerializer( pEncodingName );
	if ( !pSerializer )
	{
		Warning( "Unerialize: DMX file encoding %s is undefined!\n", pEncodingName );
		return false;
	}

	g_pMemAlloc->heapchk();

	DmxHeader_t header;
	bool bStoresVersionInFile = pSerializer->StoresVersionInFile();
	bool bIsCurrentVersion = false; // for formats that don't store a format, files are currently always *not* 
									// at the current version since they need to be converted to dmx
	const char *pDestFormatName;
	if ( bStoresVersionInFile )
	{
		bool bOk = ReadDMXHeader( inBuf, &header );
		if ( !bOk )
		{
			Warning( "Unserialize: unable to read DMX header!\n" );
			return false;
		}

		int nCurrentFormatVersion = GetCurrentFormatVersion( header.formatName );
		if ( header.nFormatVersion > nCurrentFormatVersion )
		{
			Warning( "Unserialize: file format newer than executable's!\n" );
			return false;
		}

		bIsCurrentVersion = !IsLegacyFormat( header.formatName ) && nCurrentFormatVersion == header.nFormatVersion;
		pDestFormatName = IsLegacyFormat( header.formatName ) ? GENERIC_DMX_FORMAT : header.formatName;
	}
	else
	{
		pDestFormatName = pSerializer->GetImportedFormat();
		Q_strncpy( header.formatName, pDestFormatName, sizeof(header.formatName) );
		header.nFormatVersion = pSerializer->GetImportedVersion();
		header.nEncodingVersion = pSerializer->GetCurrentVersion();
		Q_strncpy( header.encodingName, pSerializer->GetName(), sizeof(header.encodingName) );
	}

	// if we're not in dmxconvert, and we're not at the latest version, call dmxconvert and unserialize from the converted file
	if ( !m_bOnlyCreateUntypedElements && !bIsCurrentVersion )
	{
		char path[ MAX_PATH ];
		V_ExtractFilePath( pFileName, path, sizeof( path ) );

		if ( !V_IsAbsolutePath( path ) )
		{
			g_pFullFileSystem->GetCurrentDirectory( path, sizeof( path ) );
		}

		bool bFileFound = g_pFullFileSystem->FileExists( pFileName );
		if ( !bFileFound )
		{
			char *pTempFileName = ( char* )stackalloc( MAX_PATH ); // NOTE - stackalloc frees on function return, not scope return
			V_ComposeFileName( path, "_temp_input_file_.dmx", pTempFileName, MAX_PATH );
			pFileName = pTempFileName;

			g_pFullFileSystem->WriteFile( pFileName, NULL, inBuf );
		}

		char tempFileName[ MAX_PATH ];
		V_ComposeFileName( path, "_temp_conversion_file_.dmx", tempFileName, sizeof( tempFileName ) );
		V_RemoveDotSlashes( tempFileName );

		const char *pDestEncodingName = "binary";
		char cmdline[ 2 * MAX_PATH + 256 ];
		V_snprintf( cmdline, sizeof( cmdline ), "dmxconvert -allowdebug -i \"%s\" -ie %s -o \"%s\" -oe %s -of %s", pFileName, header.encodingName, tempFileName, pDestEncodingName, pDestFormatName );

		IProcess *pProcess = g_pProcessUtils->StartProcess( cmdline, 0 );
		if ( !pProcess )
		{
			Warning( "Unerialize: Unable to start dmxconvert process\n" );
			return false;
		}

		pProcess->WaitUntilComplete();
		pProcess->Release();

		bool bSuccess;
		{
			if ( !inBuf.IsText() )
			{
				CUtlStreamBuffer buf( tempFileName, NULL, CUtlBuffer::READ_ONLY );
				if ( !buf.IsValid() )
				{
					Warning( "Unerialize: Unable to open temp file \"%s\"\n", tempFileName );
					return false;
				}

				// yes, this passes in pFileName, even though it read from tempFileName - pFileName is only used for marking debug messages and setting fileid
				bSuccess = Unserialize( buf, pDestEncodingName, pDestFormatName, pDestFormatName, pFileName, idConflictResolution, hRoot );

			}
			else
			{
				CUtlBuffer buf( 0, 0, CUtlBuffer::READ_ONLY );
				if ( !g_pFullFileSystem->ReadFile( tempFileName, NULL, buf ) )
				{
					Warning( "Unerialize: Unable to open temp file \"%s\"\n", tempFileName );
					return false;
				}

				// yes, this passes in pFileName, even though it read from tempFileName - pFileName is only used for marking debug messages and setting fileid
				bSuccess = Unserialize( buf, pDestEncodingName, pDestFormatName, pDestFormatName, pFileName, idConflictResolution, hRoot );
			}

		}

		if ( !bFileFound )
		{
			g_pFullFileSystem->RemoveFile( pFileName );
		}
		g_pFullFileSystem->RemoveFile( tempFileName );
		return bSuccess;
	}

	// advance the buffer the the end of the header
	if ( bStoresVersionInFile )
	{
		if ( V_strcmp( pEncodingName, header.encodingName ) != 0 )
			return false;

		if ( pRequiredFormat && pRequiredFormat[0] )
		{
			if ( V_strcmp( pRequiredFormat, header.formatName ) != 0 )
			{
				Warning( "Unexpected format '%s' reading file '%s' - expected format '%s'\n", pRequiredFormat, pFileName, header.formatName );
			}
		}

		if ( pSerializer->IsBinaryFormat() )
		{
			// For binary formats, we gotta keep reading until we hit the string terminator
			// that occurred after the version line.
			while( inBuf.GetChar() != 0 )
			{
				if ( !inBuf.IsValid() )
					break;
			}
		}
	}

	m_bIsUnserializing = true;

	DmFileId_t fileid = FindOrCreateFileId( pFileName );

	// Now read the file using the appropriate format
	CDmElement *pRoot;
	bool bOk = pSerializer->Unserialize( inBuf, pEncodingName, header.nEncodingVersion, header.formatName, header.nFormatVersion,
															fileid, idConflictResolution, &pRoot );
	if ( bOk )
	{
		hRoot = pRoot ? pRoot->GetHandle() : DMELEMENT_HANDLE_INVALID;

		SetFileModificationUTCTime( fileid, 0 );
		SetFileFormat( fileid, header.formatName );
		SetFileRoot( fileid, hRoot );
	}
	else
	{
		RemoveFileId( fileid );
	}

	m_bIsUnserializing = false;
	return bOk;
}

bool CDataModel::UpdateUnserializedElements( const char *pSourceFormatName, int nSourceFormatVersion,
											 DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	if ( IsLegacyFormat( pSourceFormatName ) )
	{
		IDmLegacyUpdater *pLegacyUpdater = FindLegacyUpdater( pSourceFormatName );
		if ( pLegacyUpdater )
		{
			if ( !pLegacyUpdater->Update( ppRoot ) )
				return false;
		}

		// if there's no legacy updater found, then this is already the latest legacy format
		pSourceFormatName = GENERIC_DMX_FORMAT;
	}

	IDmFormatUpdater *pFormatUpdater = FindFormatUpdater( pSourceFormatName );
	if ( !pFormatUpdater )
		return true;	// Used to return false, if no format updater exists, simply means a format updater hasn't been required yet

	return pFormatUpdater->Update( ppRoot, nSourceFormatVersion );
}

//-----------------------------------------------------------------------------
// file id reference methods
//-----------------------------------------------------------------------------

int CDataModel::NumFileIds()
{
	return m_openFiles.GetHandleCount();
}

DmFileId_t CDataModel::GetFileId( int i )
{
	Assert( i >= 0 && i < ( int )m_openFiles.GetHandleCount() );
	if ( i < 0 || i >= ( int )m_openFiles.GetHandleCount() )
		return DMFILEID_INVALID;

	return ( DmFileId_t )m_openFiles.GetHandleFromIndex( i );
}

void CleanupFileName( char *pOutFileName, size_t nOutLen, const char *pInFileName )
{
	char fixedFileName[ MAX_PATH ];
	V_FixupPathName( fixedFileName, sizeof( fixedFileName ), pInFileName );

	if ( !V_IsAbsolutePath( fixedFileName ) )
	{
		char currentDir[ MAX_PATH ];
		g_pFullFileSystem->GetCurrentDirectory( currentDir, sizeof( currentDir ) );
		V_MakeAbsolutePath( pOutFileName, nOutLen, fixedFileName, currentDir );
	}
	else
	{
		V_strncpy( pOutFileName, fixedFileName, nOutLen );
	}
}

DmFileId_t CDataModel::FindOrCreateFileId( const char *pFileName )
{
	Assert( pFileName && *pFileName );
	if ( !pFileName || !*pFileName )
		return DMFILEID_INVALID;

	char fixedFileName[ MAX_PATH ];
	CleanupFileName( fixedFileName, sizeof( fixedFileName ), pFileName );

	DmFileId_t fileid = GetFileId( fixedFileName );
	if ( fileid != DMFILEID_INVALID )
	{
//		Assert( IsFileLoaded( fileid ) );
		MarkFileLoaded( fileid ); // this is sort of a hack, but I'm planning a rewrite phase on all this anyways - joe
		return fileid;
	}

	fileid = ( DmFileId_t )m_openFiles.AddHandle();
	m_openFiles.SetHandle( fileid, new FileElementSet_t( GetSymbol( fixedFileName ) ) );
	return fileid;
}

void CDataModel::RemoveFileId( DmFileId_t fileid )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes || fileid == DMFILEID_INVALID );
	if ( !fes )
		return;

	if ( fes->m_bLoaded )
	{
		UnloadFile( fileid, true );
	}
	delete fes;

	m_openFiles.RemoveHandle( fileid );
}

DmFileId_t CDataModel::GetFileId( const char *pFileName )
{
	char fixedFileName[ MAX_PATH ];
	CleanupFileName( fixedFileName, sizeof( fixedFileName ), pFileName );

	CUtlSymbolLarge filenameSym = m_SymbolTable.Find( fixedFileName );
	if ( filenameSym == UTL_INVAL_SYMBOL_LARGE )
		return DMFILEID_INVALID;

	int nFiles = m_openFiles.GetHandleCount();
	for ( int i = 0; i < nFiles; ++i )
	{
		DmFileId_t fileid = ( DmFileId_t )m_openFiles.GetHandleFromIndex( i );
		FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
		Assert( fes || !m_openFiles.IsHandleValid( fileid ) );
		if ( fes && fes->m_filename == filenameSym )
			return fileid;
	}

	return DMFILEID_INVALID;
}

const char *CDataModel::GetFileName( DmFileId_t fileid )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes || fileid == DMFILEID_INVALID );
	return fes ? fes->m_filename.String() : NULL;
}

void CDataModel::SetFileName( DmFileId_t fileid, const char *pFileName )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes );
	if ( !fes )
		return;

	char pFixedFileName[ MAX_PATH ];
	CleanupFileName( pFixedFileName, sizeof( pFixedFileName ), pFileName );

	fes->m_filename = GetSymbol( pFixedFileName );
}

const char *CDataModel::GetFileFormat( DmFileId_t fileid )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes || fileid == DMFILEID_INVALID );
	return fes ? fes->m_format.String() : NULL;
}

void CDataModel::SetFileFormat( DmFileId_t fileid, const char *pFormat )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes );
	if ( !fes )
		return;

	fes->m_format = GetSymbol( pFormat );
}

DmElementHandle_t CDataModel::GetFileRoot( DmFileId_t fileid )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes || fileid == DMFILEID_INVALID );
	return fes ? (DmElementHandle_t)fes->m_hRoot : DMELEMENT_HANDLE_INVALID;
}

void CDataModel::SetFileRoot( DmFileId_t fileid, DmElementHandle_t hRoot )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes );
	if ( !fes )
		return;

	if ( fes->m_hRoot == hRoot )
		return;

	fes->m_hRoot = hRoot;
}

long CDataModel::GetFileModificationUTCTime( DmFileId_t fileid )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes || fileid == DMFILEID_INVALID );
	return fes ? fes->m_fileModificationTime : 0;
}

void CDataModel::SetFileModificationUTCTime( DmFileId_t fileid, long fileModificationTime )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes || fileid == DMFILEID_INVALID );
	if ( !fes )
		return;

#ifdef _DEBUG
	char oldFileTime[ 256 ];
	char newFileTime[ 256 ];
	UTCTimeToString( oldFileTime, sizeof( oldFileTime ), fes->m_fileModificationTime );
	UTCTimeToString( newFileTime, sizeof( newFileTime ), fileModificationTime );
	Msg( "Setting file modification time for '%s' from %d: %s to %d: %s\n",
		fes->m_filename.String(), fes->m_fileModificationTime, oldFileTime, fileModificationTime, newFileTime );
#endif // _DEBUG

	fes->m_fileModificationTime = fileModificationTime;
}

long CDataModel::GetCurrentUTCTime()
{
	return _time32( NULL );
}

void CDataModel::UTCTimeToString( char *pString, int maxChars, long fileTime )
{
	return g_pFullFileSystem->FileTimeToString( pString, maxChars, fileTime );
}

bool CDataModel::IsFileLoaded( DmFileId_t fileid )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes || fileid == DMFILEID_INVALID );
	return fes ? fes->m_bLoaded : false;
}

void CDataModel::UnloadFile( DmFileId_t fileid, bool bDeleteElements )
{
	ClearUndo();
	CDisableUndoScopeGuard sg;

	int nHandles = ( int )m_Handles.GetHandleCount();
	for ( int i = 0; i < nHandles; ++i )
	{
		DmElementHandle_t hElement = ( DmElementHandle_t )m_Handles.GetHandleFromIndex( i );
		if ( hElement == DMELEMENT_HANDLE_INVALID )
			continue;

		CDmElement *pElement = GetElement( hElement );
		if ( !pElement || pElement->GetFileId() != fileid )
			continue;

		DeleteElement( hElement, bDeleteElements ? HR_ALWAYS : HR_IF_NOT_REFERENCED );
	}

	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	if ( fes )
	{
		fes->m_bLoaded = false;
	}
}

void CDataModel::UnloadFile( DmFileId_t fileid )
{
	UnloadFile( fileid, false );
}

void CDataModel::MarkFileLoaded( DmFileId_t fileid )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes );
	if ( !fes )
		return;

	fes->m_bLoaded = true;
}

int CDataModel::NumElementsInFile( DmFileId_t fileid )
{
	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes );
	if ( !fes )
		return 0;

	return fes->m_nElements;
}

//-----------------------------------------------------------------------------
// file id reference methods not in IDataModel
//-----------------------------------------------------------------------------
void CDataModel::RemoveElementFromFile( DmElementHandle_t hElement, DmFileId_t fileid )
{
	if ( fileid == DMFILEID_INVALID )
		return;

	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes );
	if ( !fes )
		return;

	--fes->m_nElements;
}

void CDataModel::AddElementToFile( DmElementHandle_t hElement, DmFileId_t fileid )
{
	if ( fileid == DMFILEID_INVALID )
		return;

	FileElementSet_t *fes = m_openFiles.GetHandle( fileid );
	Assert( fes );
	if ( !fes )
		return;

	++fes->m_nElements;
}

// search id->handle table (both loaded and unloaded) for id, and if not found, create a new handle, map it to the id and return it
DmElementHandle_t CDataModel::FindOrCreateElementHandle( const DmObjectId_t &id )
{
	UtlHashHandle_t h = m_elementIds.Find( id );
	if ( h != m_elementIds.InvalidHandle() )
		return m_elementIds[ h ];

	h = m_unloadedIdElementMap.Find( ElementIdHandlePair_t( id ) ); // TODO - consider optimizing find to take just an id
	if ( h != m_unloadedIdElementMap.InvalidHandle() )
		return m_unloadedIdElementMap[ h ].m_ref.m_hElement;

	DmElementHandle_t hElement = AcquireElementHandle();
	m_unloadedIdElementMap.Insert( ElementIdHandlePair_t( id, DmElementReference_t( hElement ) ) );
	MarkHandleInvalid( hElement );
	return hElement;
}

// changes an element's id and associated mappings - generally during unserialization
DmElementHandle_t CDataModel::ChangeElementId( DmElementHandle_t hElement, const DmObjectId_t &oldId, const DmObjectId_t &newId )
{
	UtlHashHandle_t oldHash = m_elementIds.Find( oldId );
	Assert( oldHash != m_elementIds.InvalidHandle() );
	if ( oldHash == m_elementIds.InvalidHandle() )
		return hElement;

	Assert( m_elementIds[ oldHash ] == hElement );

	// can't change an element's id once it has attributes or handles linked to it
	CDmElement *pElement = GetElement( hElement );
	Assert( pElement );
	if ( !pElement )
		return DMELEMENT_HANDLE_INVALID;

	Assert( !CDmeElementAccessor::GetReference( pElement )->IsWeaklyReferenced() );

	UtlHashHandle_t newHash = m_elementIds.Find( newId );
	if ( newHash != m_elementIds.InvalidHandle() )
		return DMELEMENT_HANDLE_INVALID; // can't change an element's id to the id of an existing element

	// remove old element entry
	m_elementIds.Remove( oldHash );

	// change the element id
	CDmeElementAccessor::SetId( pElement, newId );

	newHash = m_unloadedIdElementMap.Find( ElementIdHandlePair_t( newId ) );
	if ( newHash == m_unloadedIdElementMap.InvalidHandle() )
	{
		// the newId has never been seen before - keep the element handle the same and rehash into the id->handle map
		m_elementIds.Insert( hElement );
		return hElement;
	}

	// else, the newId is being referenced by some other element
	// change element to use newId and the associated handle from an element reference

	DmElementReference_t &newRef = m_unloadedIdElementMap[ newHash ].m_ref;
	DmElementHandle_t newHandle = newRef.m_hElement;
	Assert( newHandle != hElement ); // no two ids should have the same handle
	Assert( !m_Handles.IsHandleValid( newHandle ) ); // unloaded elements shouldn't have valid handles

	m_Handles.SetHandle( newHandle, GetElement( hElement ) );
	CDmeElementAccessor::ChangeHandle( pElement, newHandle );
	CDmeElementAccessor::SetReference( pElement, newRef );
	ReleaseElementHandle( hElement );

	// move new element entry from the unloaded map to the loaded map
	m_elementIds.Insert( newHandle );
	m_unloadedIdElementMap.Remove( newHash );

	return newHandle;
}

DmElementReference_t *CDataModel::FindElementReference( DmElementHandle_t hElement, DmObjectId_t **ppId /* = NULL */ )
{
	if ( ppId )
	{
		*ppId = NULL;
	}

	CDmElement* pElement = GetElement( hElement );
	if ( pElement )
		return CDmeElementAccessor::GetReference( pElement );

	for ( UtlHashHandle_t h = m_unloadedIdElementMap.GetFirstHandle(); h != m_unloadedIdElementMap.InvalidHandle(); h = m_unloadedIdElementMap.GetNextHandle( h ) )
	{
		DmElementReference_t &ref = m_unloadedIdElementMap[ h ].m_ref;
		if ( ref.m_hElement == hElement )
		{
			if ( ppId )
			{
				*ppId = &m_unloadedIdElementMap[ h ].m_id;
			}
			return &ref;
		}
	}

	return NULL;
}

void CDataModel::DontAutoDelete( DmElementHandle_t hElement )
{
	// this artificially adds a strong reference to the element, so it won't ever get unref'ed to 0
	// the only ways for this element to go away are explicit deletion, or file unload
	OnElementReferenceAdded( hElement, HT_STRONG );
}

void CDataModel::OnElementReferenceAdded( DmElementHandle_t hElement, CDmAttribute *pAttribute )
{
	Assert( pAttribute );
	if ( !pAttribute )
		return;

	if ( hElement == DMELEMENT_HANDLE_INVALID )
		return;

	DmElementReference_t *pRef = FindElementReference( hElement );
	if ( !pRef )
		return;

// 	Msg( "OnElementReferenceAdded: %s 0x%x '%s' referenced by %s 0x%x '%s'\n",
// 		GetElementType( hElement ).String(), hElement, GetElementName( hElement ),
// 		pAttribute->GetOwner()->GetTypeString(), pAttribute->GetOwner()->GetHandle(), pAttribute->GetName() );

	if ( pRef->m_bHasEverBeenReferenced && !pRef->IsStronglyReferenced() )
	{
		CDmElement *pElement = GetElement( hElement );
		if ( !pElement || pElement->GetFileId() != DMFILEID_INVALID )
		{
			m_bDeleteOrphanedElements = true;
		}
	}
	pRef->m_bHasEverBeenReferenced = true;

	pRef->AddAttribute( pAttribute );
}

void CDataModel::OnElementReferenceAdded( DmElementHandle_t hElement, HandleType_t handleType )
{
	if ( hElement == DMELEMENT_HANDLE_INVALID )
		return;

	DmElementReference_t *pRef = FindElementReference( hElement );
	if ( !pRef )
		return;

// 	if ( handleType != HT_WEAK )
// 	{
// 		int nCount = handleType == HT_STRONG ? pRef->m_nStrongHandleCount : pRef->m_nUndoHandleCount;
// 		Msg( "OnElementReferenceAdded: %s 0x%x \"%s\" referenced by %s handle\n",
// 			GetElementType( hElement ).String(), hElement, GetElementName( hElement ),
// 			handleType == HT_STRONG ? "strong" : "undo" );
// 	}

	if ( handleType != HT_WEAK )
	{
		bool bUpdate = handleType == HT_STRONG ? pRef->m_bHasEverBeenReferenced && !pRef->IsStronglyReferenced() : !pRef->IsReferencedByUndo();
		if ( bUpdate )
		{
			CDmElement *pElement = GetElement( hElement );
			if ( !pElement || pElement->GetFileId() != DMFILEID_INVALID )
			{
				m_bDeleteOrphanedElements = true;
			}
		}
		pRef->m_bHasEverBeenReferenced = true;
	}

	switch ( handleType )
	{
	case HT_WEAK:	++pRef->m_nWeakHandleCount;	break;
	case HT_STRONG: ++pRef->m_nStrongHandleCount; break;
	case HT_UNDO:	++pRef->m_nUndoHandleCount; break;
	}
}

void CDataModel::OnElementReferenceRemoved( DmElementHandle_t hElement, CDmAttribute *pAttribute )
{
	MEM_ALLOC_CREDIT();

	Assert( pAttribute );
	if ( !pAttribute )
		return;

	if ( hElement == DMELEMENT_HANDLE_INVALID )
		return;

	DmElementReference_t *pRef = FindElementReference( hElement );
	if ( !pRef )
		return;

	pRef->RemoveAttribute( pAttribute );

// 	Msg( "OnElementReferenceRemoved: %s 0x%x '%s' referenced by %s 0x%x '%s'\n",
// 		GetElementType( hElement ).String(), hElement, GetElementName( hElement ),
// 		pAttribute->GetOwner()->GetTypeString(), pAttribute->GetOwner()->GetHandle(), pAttribute->GetName() );

	if ( !pRef->IsStronglyReferenced() )
	{
		CDmElement *pElement = GetElement( hElement );
		if ( !pElement || pElement->GetFileId() != DMFILEID_INVALID )
		{
			m_bDeleteOrphanedElements = true;
		}
	}
}

void CDataModel::OnElementReferenceRemoved( DmElementHandle_t hElement, HandleType_t handleType )
{
	MEM_ALLOC_CREDIT();

	if ( hElement == DMELEMENT_HANDLE_INVALID )
		return;

	DmElementReference_t *pRef = FindElementReference( hElement );
	if ( !pRef )
		return;

	switch ( handleType )
	{
	case HT_WEAK:	--pRef->m_nWeakHandleCount;	break;
	case HT_STRONG: --pRef->m_nStrongHandleCount; break;
	case HT_UNDO:	--pRef->m_nUndoHandleCount; break;
	}

// 	if ( handleType != HT_WEAK )
// 	{
// 		int nCount = handleType == HT_STRONG ? pRef->m_nStrongHandleCount : pRef->m_nUndoHandleCount;
// 		Msg( "OnElementReferenceRemoved: %s 0x%x \"%s\" referenced by %s handle\n",
// 			GetElementType( hElement ).String(), hElement, GetElementName( hElement ),
// 			handleType == HT_STRONG ? "strong" : "undo" );
// 	}

	if ( handleType != HT_WEAK )
	{
		bool bUpdate = handleType == HT_STRONG ? !pRef->IsStronglyReferenced() : !pRef->IsReferencedByUndo();
		if ( bUpdate )
		{
			CDmElement *pElement = GetElement( hElement );
			if ( !pElement || pElement->GetFileId() != DMFILEID_INVALID )
			{
				m_bDeleteOrphanedElements = true;
			}
		}
	}
}

void CDataModel::RemoveUnreferencedElements()
{
	CDisableUndoScopeGuard sg;

	if ( m_bDeleteOrphanedElements )
	{
		FindAndDeleteOrphanedElements();
		m_bDeleteOrphanedElements = false;
	}
}

void UpdateReferenceToElementsNotInUndoWithParityBitSet( CDmAttribute *pAttr, CDmElement *pChild, int nParityBit, bool bDetach )
{
	if ( !pAttr || !pChild )
		return;

	if ( pChild->IsOnlyInUndo() || !pChild->GetParity( nParityBit ) )
		return;

	DmElementReference_t *pRef = CDmeElementAccessor::GetReference( pChild );

	if ( bDetach )
	{
// 		Msg( "removing " );
		pRef->RemoveAttribute( pAttr );
	}
	else
	{
		Assert( !CDmeElementAccessor::GetReference( pChild )->FindAttribute( pAttr ) );
// 		Msg( "adding   " );
		pRef->AddAttribute( pAttr );
	}

// 	CDmElement *pOwner = pAttr->GetOwner();
// 	Msg( "reference to %s 0x%x '%s' from %s 0x%x '%s' %s\n",
// 		pChild->GetTypeString(), pChild->GetHandle(), pChild->GetName(),
// 		pOwner->GetTypeString(), pOwner->GetHandle(), pOwner->GetName(),
// 		pAttr->GetName() );
}

void UpdateReferencesToElementsNotInUndoWithParityBitSet( CDmElement *pElement, int nParityBit, bool bDetach )
{
	for ( CDmAttribute *pAttr = pElement->FirstAttribute(); pAttr; pAttr = pAttr->NextAttribute() )
	{
		DmAttributeType_t type = pAttr->GetType();
		if ( type == AT_ELEMENT )
		{
			UpdateReferenceToElementsNotInUndoWithParityBitSet( pAttr, pAttr->GetValueElement< CDmElement >(), nParityBit, bDetach );
		}
		else if ( type == AT_ELEMENT_ARRAY )
		{
			const CDmrElementArrayConst<> elementArray( pAttr );
			int nChildren = elementArray.Count();
			for ( int i = 0; i < nChildren; ++i )
			{
				UpdateReferenceToElementsNotInUndoWithParityBitSet( pAttr, elementArray[ i ], nParityBit, bDetach );
			}
		}
	}
}


void CDataModel::FindAndDeleteOrphanedElements()
{
	// mark & sweep algorithm for elements

	enum
	{
		BIT_STRONG = 0,
		BIT_UNDO = 1,
	};

	// clear accessible flag from all elements
	int nHandles = ( int )m_Handles.GetHandleCount();
	for ( int i = 0; i < nHandles; ++i )
	{
		DmElementHandle_t hElement = ( DmElementHandle_t )m_Handles.GetHandleFromIndex( i );
		CDmElement *pElement = m_Handles.GetHandle( hElement, false ); // walk through invalidated handles as well (such as deleted elements)
		if ( !pElement )
			continue;

		DmFileId_t fileid = pElement->GetFileId();
		if ( fileid == DMFILEID_INVALID )
			continue;

		pElement->SetParity( false, BIT_STRONG );
		pElement->SetParity( false, BIT_UNDO );
	}

	// mark elements accessible from undo system
	for ( int i = 0; i < nHandles; ++i )
	{
		DmElementHandle_t hElement = ( DmElementHandle_t )m_Handles.GetHandleFromIndex( i );
		CDmElement *pElement = m_Handles.GetHandle( hElement, false ); // walk through invalidated handles as well (such as deleted elements)
		if ( !pElement )
			continue;

		DmFileId_t fileid = pElement->GetFileId();
		if ( fileid == DMFILEID_INVALID )
			continue;

		if ( CDmeElementAccessor::GetReference( pElement )->m_nUndoHandleCount == 0 )
			continue;

		pElement->SetParity( true, TD_ALL, BIT_UNDO );
	}

	// mark elements accessible from file roots
	int nFiles = NumFileIds();
	for ( int i = 0; i < nFiles; ++i )
	{
		DmFileId_t fileid = GetFileId( i );
		if ( fileid == DMFILEID_INVALID )
			continue;

		DmElementHandle_t hRoot = GetFileRoot( fileid );
		CDmElement *pRoot = GetElement( hRoot );
		if ( !pRoot )
			continue;

		pRoot->SetParity( true, TD_ALL, BIT_STRONG );
	}

	// mark elements accessible from counted handles
	for ( DmElementHandle_t hElement = FirstAllocatedElement(); hElement != DMELEMENT_HANDLE_INVALID; hElement = NextAllocatedElement( hElement ) )
	{
		CDmElement *pElement = GetElement( hElement );
		if ( !pElement )
			continue;

		DmFileId_t fileid = pElement->GetFileId();
		if ( fileid == DMFILEID_INVALID )
			continue;

		// an element is deleted when its strong handle count goes to zero, but if it never leaves zero, it shouldn't be deleted
		DmElementReference_t *pRef = CDmeElementAccessor::GetReference( pElement );
		if ( pRef->m_bHasEverBeenReferenced && pRef->m_nStrongHandleCount == 0 )
			continue;

		pElement->SetParity( true, TD_ALL, BIT_STRONG );
	}


	// BIT_STRONG,  IsOnlyInUndo() => being moved out of undo
	// BIT_STRONG, !IsOnlyInUndo() => remaining out of undo
	// BIT_UNDO,    IsOnlyInUndo() => remaining in undo
	// BIT_UNDO,   !IsOnlyInUndo() => being moved into undo

	// update element references
	for ( DmElementHandle_t hElement = FirstAllocatedElement(); hElement != DMELEMENT_HANDLE_INVALID; hElement = NextAllocatedElement( hElement ) )
	{
		CDmElement *pElement = GetElement( hElement );
		if ( !pElement )
			continue;

		DmFileId_t fileid = pElement->GetFileId();
		if ( fileid == DMFILEID_INVALID )
			continue;

		if ( pElement->GetParity( BIT_STRONG ) )
		{
			if ( pElement->IsOnlyInUndo() )
			{
				// this element is leaving the undo system - add references to elements that aren't in undo (and weren't before)
				UpdateReferencesToElementsNotInUndoWithParityBitSet( pElement, BIT_STRONG, false );
			}
		}
		else if ( pElement->GetParity( BIT_UNDO ) )
		{
			if ( !pElement->IsOnlyInUndo() )
			{
				// this element is entering the undo system - remove references to elements that aren't in undo (and weren't before)
				UpdateReferencesToElementsNotInUndoWithParityBitSet( pElement, BIT_STRONG, true );
			}
		}
	}

	// delete elements that aren't accessible
	for ( DmElementHandle_t hElement = FirstAllocatedElement(); hElement != DMELEMENT_HANDLE_INVALID; hElement = NextAllocatedElement( hElement ) )
	{
		CDmElement *pElement = GetElement( hElement );
		if ( !pElement )
			continue;

		DmFileId_t fileid = pElement->GetFileId();
		if ( fileid == DMFILEID_INVALID )
			continue;

// 		Msg( "%s 0x%08x '%s': %s %s - %s\n",
// 			pElement->GetTypeString(), hElement, pElement->GetName(),
// 			pElement->GetParity( BIT_STRONG ) ? "strong" : "!strong",
// 			pElement->GetParity( BIT_UNDO ) ? "undo" : "!undo",
// 			pElement->IsOnlyInUndo() ? "only in undo" : "not only in undo" );

		if ( pElement->GetParity( BIT_STRONG ) )
		{
			if ( pElement->IsOnlyInUndo() )
			{
// 				Msg( "adopting  %s 0x%08x '%s'\n", pElement->GetTypeString(), hElement, pElement->GetName() );
				pElement->SetOnlyInUndo( false );
				CDmeElementAccessor::OnAdoptedFromUndo( pElement );
			}
		}
		else if ( pElement->GetParity( BIT_UNDO ) )
		{
			if ( !pElement->IsOnlyInUndo() )
			{
// 				Msg( "orphaning %s 0x%08x '%s'\n", pElement->GetTypeString(), hElement, pElement->GetName() );
				pElement->SetOnlyInUndo( true );
				CDmeElementAccessor::OnOrphanedToUndo( pElement );
			}
		}
		else
		{
//			Msg( "deleting %s 0x%08x '%s'\n", pElement->GetTypeString(), hElement, pElement->GetName() );
			DeleteElement( hElement );
		}
	}
}


DmElementHandle_t CDataModel::FindElement( const DmObjectId_t &id )
{
	UtlHashHandle_t h = m_elementIds.Find( id );
	if ( h == m_elementIds.InvalidHandle() )
		return DMELEMENT_HANDLE_INVALID;

	return m_elementIds[ h ];
}

void CDataModel::GetExistingElements( CElementIdHash &hash ) const
{
	DMX_PROFILE_SCOPE( GetExistingElements_Copy );
	hash = m_elementIds;
}


DmAttributeReferenceIterator_t CDataModel::FirstAttributeReferencingElement( DmElementHandle_t hElement )
{
	DmElementReference_t *pRef = FindElementReference( hElement );
	if ( !pRef || pRef->m_attributes.m_hAttribute == DMATTRIBUTE_HANDLE_INVALID )
		return DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;

	return ( DmAttributeReferenceIterator_t )( intp )&pRef->m_attributes;
}

DmAttributeReferenceIterator_t CDataModel::NextAttributeReferencingElement( DmAttributeReferenceIterator_t hAttrIter )
{
	DmAttributeList_t *pList = ( DmAttributeList_t* )hAttrIter;
	if ( !pList )
		return DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;

	return ( DmAttributeReferenceIterator_t )( intp )pList->m_pNext;
}

CDmAttribute *CDataModel::GetAttribute( DmAttributeReferenceIterator_t hAttrIter )
{
	DmAttributeList_t *pList = ( DmAttributeList_t* )hAttrIter;
	if ( !pList )
		return NULL;

	return GetAttribute( pList->m_hAttribute );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
// Output : IDmElementInternal
//-----------------------------------------------------------------------------
CDmElement *CDataModel::Unserialize( CUtlBuffer& buf )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *element - 
//			buf - 
//-----------------------------------------------------------------------------
void CDataModel::Serialize( CDmElement *element, CUtlBuffer& buf )
{
}


//-----------------------------------------------------------------------------
// Sets a factory to use if the element type can't be found
//-----------------------------------------------------------------------------
void CDataModel::SetDefaultElementFactory( IDmElementFactory *pFactory )
{
	if ( m_bUnableToSetDefaultFactory )
	{
		Assert( 0 );
		return;
	}

	m_pDefaultFactory = pFactory ? pFactory : &s_DefaultElementFactory;
}

	
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *elementName - 
//			factory - 
//-----------------------------------------------------------------------------
void CDataModel::AddElementFactory( CDmElementFactoryHelper *pFactoryHelper )
{
	Assert( pFactoryHelper );
	const char *pClassName = pFactoryHelper->GetClassname();
	int idx = m_Factories.Find( pClassName );
	if ( idx == m_Factories.InvalidIndex() )
	{
		m_Factories.Insert( pClassName, pFactoryHelper );
	}
	else
	{
		// Override the factory?
		m_Factories[idx] = pFactoryHelper;
		Warning( "Factory for element type '%s' already exists\n", pClassName );
	}
}

CDmElementFactoryHelper	*CDataModel::GetElementFactoryHelper( const char *pElementType )
{
	int idx = m_Factories.Find( pElementType );
	if ( idx == m_Factories.InvalidIndex() )
		return NULL;

	return m_Factories[ idx ];
}

bool CDataModel::HasElementFactory( const char *pElementType ) const
{
	int idx = m_Factories.Find( pElementType );
	return ( idx != m_Factories.InvalidIndex() );
}

int CDataModel::GetFirstFactory() const
{
	return m_Factories.First();
}

int CDataModel::GetNextFactory( int index ) const
{
	return m_Factories.Next( index );
}

bool CDataModel::IsValidFactory( int index ) const
{
	return m_Factories.IsValidIndex( index );
}

char const *CDataModel::GetFactoryName( int index ) const
{
	return m_Factories.GetElementName( index );
}

//-----------------------------------------------------------------------------
// Purpose: Creates a scene object 
//-----------------------------------------------------------------------------
DmElementHandle_t CDataModel::CreateElement( CUtlSymbolLarge typeSymbol, const char *pElementName, DmFileId_t fileid, const DmObjectId_t *pObjectID )
{
	return CreateElement( typeSymbol.String(), pElementName, fileid, pObjectID );
}

DmElementHandle_t CDataModel::CreateElement( const char *pElementType, const char *pElementName, DmFileId_t fileid, const DmObjectId_t *pObjectID )
{
	DMX_PROFILE_SCOPE( CDataModel_CreateElement1 );

	Assert( !pObjectID || m_elementIds.Find( *pObjectID ) == m_elementIds.InvalidHandle() );

	UtlHashHandle_t h = pObjectID ? m_unloadedIdElementMap.Find( ElementIdHandlePair_t( *pObjectID ) ) : m_unloadedIdElementMap.InvalidHandle();
	if ( h != m_unloadedIdElementMap.InvalidHandle() )
	{
		CDmElement *pElement = CreateElement( m_unloadedIdElementMap[ h ].m_ref, pElementType, pElementName, fileid, pObjectID );
		if ( pElement )
		{
			m_unloadedIdElementMap.Remove( h );
			return pElement->GetHandle();
		}
	}
	else
	{
		DmElementHandle_t hElement = AcquireElementHandle();
		CDmElement *pElement = CreateElement( DmElementReference_t( hElement ), pElementType, pElementName, fileid, pObjectID );
		if ( pElement )
			return pElement->GetHandle();

		ReleaseElementHandle( hElement );
	}

	return DMELEMENT_HANDLE_INVALID;
}

class CUndoCreateElement : public CUndoElement
{
	typedef CUndoElement BaseClass;
public:
	CUndoCreateElement() : 
		BaseClass( "CUndoCreateElement" ),
		m_bKill( false ),
		m_hElement()
	{
	}

	~CUndoCreateElement()
	{
		if ( m_bKill )
		{
			g_pDataModelImp->MarkHandleValid( m_hElement );
			g_pDataModelImp->DeleteElement( m_hElement );
		}
	}

	void SetElement( DmElementHandle_t hElement )
	{
		Assert( GetElement<CDmElement>( hElement ) && GetElement<CDmElement>( hElement )->GetFileId() != DMFILEID_INVALID );
		m_hElement = hElement; // this has to be delayed so that the element's ref count can be incremented
	}

	virtual void Undo()
	{
		m_bKill = true;
		g_pDataModelImp->MarkHandleInvalid( m_hElement );
	}

	virtual void Redo()
	{
		m_bKill = false;
		g_pDataModelImp->MarkHandleValid( m_hElement );
	}

private:
	CDmeUndoHandle	m_hElement;
	bool			m_bKill;
};

//-----------------------------------------------------------------------------
// CreateElement references the attribute list passed in via ref, so don't edit or purge ref's attribute list afterwards
// this is kosher because the ref either is created on the fly and has no attributes, or is being removed from m_unloadedIdElementMap
//-----------------------------------------------------------------------------
CDmElement* CDataModel::CreateElement( const DmElementReference_t &ref, const char *pElementType, const char *pElementName, DmFileId_t fileid, const DmObjectId_t *pObjectID )
{
	DMX_PROFILE_SCOPE( CDataModel_CreateElement2 );

//	Msg( "Creating %s 0x%x '%s' in file \"%s\" - %d elements loaded\n", pElementType, ref.m_hElement, pElementName ? pElementName : "", GetFileName( fileid ), m_elementIds.Count() );

	MEM_ALLOC_CREDIT();

	DmPhase_t phase = g_pDmElementFramework->GetPhase();
	if ( phase != PH_EDIT )
	{
		Assert( 0 );
		return NULL;
	}

	// Create a new id if we weren't given one to use
	DmObjectId_t newId;
	if ( !pObjectID )
	{
		CreateUniqueId( &newId );
		pObjectID = &newId;
	}

	if ( !pElementName )
	{
		pElementName = UNNAMED_ELEMENT_NAME;
	}

	IDmElementFactory *pFactory = NULL;
	if ( m_bOnlyCreateUntypedElements )
	{
		// As soon as we create something from the default factory,
		// we can no longer change the default factory
		m_bUnableToSetDefaultFactory = true;

		pFactory = m_pDefaultFactory;
	}
	else
	{
		DMX_PROFILE_SCOPE( CreateElement_FindFactory );

		int idx = m_Factories.Find( pElementType );
		if ( idx == m_Factories.InvalidIndex() )
		{
			if ( !m_pDefaultFactory )
			{
				Warning( "Unable to create unknown element %s!\n", pElementType );
				return NULL;
			}
			m_bUnableToSetDefaultFactory = true;
			pFactory = m_pDefaultFactory;
		}
		else
		{
			m_bUnableToCreateOnlyUntypedElements = true;
			pFactory = m_Factories[ idx ]->GetFactory();
		}
	}
	Assert( pFactory );

	// Create an undo element
	CUndoCreateElement *pUndo = NULL;
	{
		if ( fileid != DMFILEID_INVALID && g_pDataModel->IsUndoEnabled()  ) // elements not in any file don't participate in undo
		{
			DMX_PROFILE_SCOPE( CreateElement_Undo );
			pUndo = new CUndoCreateElement();
			g_pDataModel->AddUndoElement( pUndo );
		}
	}

	CDisableUndoScopeGuard sg;

	CDmElement *pElement = NULL;
	
	{
		DMX_PROFILE_SCOPE( CreateElement_pFactoryCreate );
		pElement = pFactory->Create( ref.m_hElement, pElementType, pElementName, fileid, *pObjectID );
	}

	if ( pElement )
	{
		DMX_PROFILE_SCOPE( CreateElement_PerformConstruction );

		++m_nElementsAllocatedSoFar;
		m_nMaxNumberOfElements = MAX( m_nMaxNumberOfElements, GetAllocatedElementCount() );

		CDmeElementAccessor::SetReference( pElement, ref );
		m_Handles.SetHandle( ref.m_hElement, pElement );
		m_elementIds.Insert( ref.m_hElement );

		CDmeElementAccessor::PerformConstruction( pElement );
		CDmeElementAccessor::EnableOnChangedCallbacks( pElement );

		if ( pUndo )
		{
			pUndo->SetElement( ref.m_hElement );
		}

		NotifyState( NOTIFY_CHANGE_TOPOLOGICAL );

		// Notify any supplied callbacks of this element's creation
		pFactory->OnElementCreated( pElement );

#ifdef _ELEMENT_HISTOGRAM_
		CUtlSymbolLarge typeSym = GetSymbol( pElementType );
		short i = g_typeHistogram.Find( typeSym );
		if ( g_typeHistogram.IsValidIndex( i ) )
		{
			++g_typeHistogram[ i ];
		}
		else
		{
			g_typeHistogram.Insert( typeSym, 1 );
		}
#endif _ELEMENT_HISTOGRAM_
	}

	return pElement;
}

void CDataModel::UpdateReferenceToElements( CDmAttribute *pAttr, CDmElement *pChild, bool bDetach )
{
	if ( !pAttr || !pChild )
		return;

	if ( bDetach )
	{
// 		Msg( "removing " );
		OnElementReferenceRemoved( pChild->GetHandle(), pAttr );
	}
	else
	{
		Assert( !CDmeElementAccessor::GetReference( pChild )->FindAttribute( pAttr ) );
// 		Msg( "adding   " );
		OnElementReferenceAdded( pChild->GetHandle(), pAttr );
	}

// 	CDmElement *pOwner = pAttr->GetOwner();
// 	Msg( "reference to %s 0x%x '%s' from %s 0x%x '%s' %s\n",
// 		pChild->GetTypeString(), pChild->GetHandle(), pChild->GetName(),
// 		pOwner->GetTypeString(), pOwner->GetHandle(), pOwner->GetName(),
// 		pAttr->GetName() );
}

void CDataModel::UpdateReferencesToElements( CDmElement *pElement, bool bDetach )
{
	for ( CDmAttribute *pAttr = pElement->FirstAttribute(); pAttr; pAttr = pAttr->NextAttribute() )
	{
		DmAttributeType_t type = pAttr->GetType();
		if ( type == AT_ELEMENT )
		{
			UpdateReferenceToElements( pAttr, pAttr->GetValueElement< CDmElement >(), bDetach );
		}
		else if ( type == AT_ELEMENT_ARRAY )
		{
			const CDmrElementArrayConst<> elementArray( pAttr );
			int nChildren = elementArray.Count();
			for ( int i = 0; i < nChildren; ++i )
			{
				UpdateReferenceToElements( pAttr, elementArray[ i ], bDetach );
			}
		}
	}
}


class CUndoDestroyElement : public CUndoElement
{
	typedef CUndoElement BaseClass;
public:
	CUndoDestroyElement( DmElementHandle_t hElement ) : 
		BaseClass( "CUndoDestroyElement" ),
		m_bKill( true ),
		m_hElement( hElement )
	{
		CDmElement *pElement = GetElement<CDmElement>( hElement );
		Assert( pElement && pElement->GetFileId() != DMFILEID_INVALID );
		g_pDataModelImp->UpdateReferencesToElements( pElement, true );
		pElement->SetOnlyInUndo( true );
		CDmeElementAccessor::OnOrphanedToUndo( pElement );

		g_pDataModelImp->MarkHandleInvalid( m_hElement );
	}

	~CUndoDestroyElement()
	{
		if ( m_bKill )
		{
			g_pDataModelImp->MarkHandleValid( m_hElement );
			g_pDataModelImp->DeleteElement( m_hElement );
		}
	}

	virtual void Undo()
	{
		m_bKill = false;
		g_pDataModelImp->MarkHandleValid( m_hElement );

		CDmElement *pElement = GetElement<CDmElement>( m_hElement );
		pElement->SetOnlyInUndo( false );
		CDmeElementAccessor::OnAdoptedFromUndo( pElement );
		g_pDataModelImp->UpdateReferencesToElements( pElement, false );
	}

	virtual void Redo()
	{
		m_bKill = true;

		CDmElement *pElement = GetElement<CDmElement>( m_hElement );
		g_pDataModelImp->UpdateReferencesToElements( pElement, true );
		pElement->SetOnlyInUndo( true );
		CDmeElementAccessor::OnOrphanedToUndo( pElement );

		g_pDataModelImp->MarkHandleInvalid( m_hElement );
	}

private:
	CDmeUndoHandle	m_hElement;
	bool				m_bKill;
};

//-----------------------------------------------------------------------------
// Purpose: Destroys a scene object 
//-----------------------------------------------------------------------------
void CDataModel::DestroyElement( DmElementHandle_t hElement )
{
	DmPhase_t phase = g_pDmElementFramework->GetPhase();
	if ( phase != PH_EDIT && phase != PH_EDIT_APPLY ) // need to allow edit_apply to delete elements, so that cascading deletes can occur in one phase
	{
		Assert( 0 );
		return;
	}

	if ( hElement == DMELEMENT_HANDLE_INVALID )
		return;

	CDmElement *pElement = m_Handles.GetHandle( hElement );
	if ( pElement == NULL )
		return;

	// Create an undo element
	if ( UndoEnabledForElement( pElement ) )
	{
		m_bDeleteOrphanedElements = true;

		CUndoDestroyElement *pUndo = new CUndoDestroyElement( hElement );
		g_pDataModel->AddUndoElement( pUndo );
		NotifyState( NOTIFY_CHANGE_TOPOLOGICAL );
		return; // if undo is enabled, just toss this onto the undo stack, rather than actually destroying it
	}

	DeleteElement( hElement );
}

void CDataModel::DeleteElement( DmElementHandle_t hElement, DmHandleReleasePolicy hrp /* = HR_ALWAYS */ )
{
	DmPhase_t phase = g_pDmElementFramework->GetPhase();
	if ( phase != PH_EDIT && phase != PH_EDIT_APPLY)
	{
		Assert( 0 );
		return;
	}

	if ( hElement == DMELEMENT_HANDLE_INVALID )
		return;

	CDmElement *pElement = m_Handles.GetHandle( hElement );
	if ( pElement == NULL )
		return;

	// In order for DestroyElement to work, then, we need to cache off the element type
	// because that's stored in an attribute

	const char *pElementType = pElement->GetTypeString();
	Assert( pElementType );

// 	Msg( "Deleting %s element 0x%x \'%s\' in file \"%s\" - %d elements loaded\n", pElementType, hElement, pElement->GetName(), GetFileName( pElement->GetFileId() ), m_elementIds.Count() );

	UtlHashHandle_t h = m_elementIds.Find( pElement->GetId() );
	Assert( h != m_elementIds.InvalidHandle() );
	if ( h != m_elementIds.InvalidHandle() )
	{
		m_elementIds.Remove( h );
	}

	DmElementReference_t *pRef = CDmeElementAccessor::GetReference( pElement );
	bool bReleaseHandle = hrp == HR_ALWAYS || ( hrp == HR_IF_NOT_REFERENCED && !pRef->IsWeaklyReferenced() );
	if ( !bReleaseHandle )
	{
		m_unloadedIdElementMap.Insert( ElementIdHandlePair_t( GetElementId( hElement ), *pRef ) );
	}

	IDmElementFactory *pFactory = NULL;
	if ( m_bOnlyCreateUntypedElements )
	{
		pFactory = m_pDefaultFactory;
	}
	else
	{
		int idx = m_Factories.Find( pElementType );
		pFactory = idx == m_Factories.InvalidIndex() ? m_pDefaultFactory : m_Factories[ idx ]->GetFactory();
	}

	CDmeElementAccessor::PerformDestruction( pElement );

	// NOTE: Attribute destruction has to happen before the containing object is destroyed
	// because the inline optimization will crash otherwise, and after PerformDestruction
	// or else PerformDestruction will crash
	CDmeElementAccessor::Purge( pElement );

	pFactory->Destroy( hElement );
	if ( bReleaseHandle )
	{
		ReleaseElementHandle( hElement );
	}
	else
	{
		MarkHandleInvalid( hElement );
	}
}


//-----------------------------------------------------------------------------
// handle-related methods
//-----------------------------------------------------------------------------
DmElementHandle_t CDataModel::AcquireElementHandle()
{
	NotifyState( NOTIFY_CHANGE_TOPOLOGICAL );
	return ( DmElementHandle_t )m_Handles.AddHandle();
}

void CDataModel::ReleaseElementHandle( DmElementHandle_t hElement )
{
	m_Handles.RemoveHandle( hElement );
	NotifyState( NOTIFY_CHANGE_TOPOLOGICAL );
}

void CDataModel::MarkHandleInvalid( DmElementHandle_t hElement )
{
	m_Handles.MarkHandleInvalid( hElement );
	NotifyState( NOTIFY_CHANGE_TOPOLOGICAL );
}

void CDataModel::MarkHandleValid( DmElementHandle_t hElement )
{
	m_Handles.MarkHandleValid( hElement );
	NotifyState( NOTIFY_CHANGE_TOPOLOGICAL );
}

void CDataModel::GetInvalidHandles( CUtlVector< DmElementHandle_t > &handles )
{
	unsigned int nHandles = m_Handles.GetHandleCount();
	for ( unsigned int i = 0; i < nHandles; ++i )
	{
		DmElementHandle_t h = ( DmElementHandle_t )m_Handles.GetHandleFromIndex( i );
		if ( !m_Handles.IsHandleValid( h ) )
		{
			handles.AddToTail( h );
		}
	}
}

void CDataModel::MarkHandlesValid( CUtlVector< DmElementHandle_t > &handles )
{
	int nHandles = handles.Count();
	for ( int i = 0; i < nHandles; ++i )
	{
		m_Handles.MarkHandleValid( handles[ i ] );
	}
}

void CDataModel::MarkHandlesInvalid( CUtlVector< DmElementHandle_t > &handles )
{
	int nHandles = handles.Count();
	for ( int i = 0; i < nHandles; ++i )
	{
		m_Handles.MarkHandleInvalid( handles[ i ] );
	}
}


CDmElement *CDataModel::GetElement( DmElementHandle_t hElement ) const
{
	return ( hElement != DMELEMENT_HANDLE_INVALID ) ? m_Handles.GetHandle( hElement ) : NULL;
}

CUtlSymbolLarge CDataModel::GetElementType( DmElementHandle_t hElement ) const
{
	CDmElement *pElement = ( hElement != DMELEMENT_HANDLE_INVALID ) ? m_Handles.GetHandle( hElement ) : NULL;
	if ( pElement == NULL )
		return UTL_INVAL_SYMBOL_LARGE;
	return pElement->GetType();
}

const char* CDataModel::GetElementName( DmElementHandle_t hElement ) const
{
	CDmElement *pElement = ( hElement != DMELEMENT_HANDLE_INVALID ) ? m_Handles.GetHandle( hElement ) : NULL;
	if ( pElement == NULL )
		return "";
	return pElement->GetName();
}

const DmObjectId_t& CDataModel::GetElementId( DmElementHandle_t hElement ) const
{
	CDmElement *pElement = ( hElement != DMELEMENT_HANDLE_INVALID ) ? m_Handles.GetHandle( hElement ) : NULL;
	if ( pElement == NULL )
	{
		static DmObjectId_t s_id;
		InvalidateUniqueId( &s_id );
		return s_id;
	}
	return pElement->GetId();
}


//-----------------------------------------------------------------------------
// Attribute types 
//-----------------------------------------------------------------------------
const char *CDataModel::GetAttributeNameForType( DmAttributeType_t attType ) const
{
	return AttributeTypeName( attType );
}

DmAttributeType_t CDataModel::GetAttributeTypeForName( const char *name ) const
{
	return AttributeType( name );
}


//-----------------------------------------------------------------------------
//
// Methods related to notification callbacks
//
//-----------------------------------------------------------------------------
bool CDataModel::InstallNotificationCallback( IDmNotify *pNotify )
{
	return m_UndoMgr.InstallNotificationCallback( pNotify );
}

void CDataModel::RemoveNotificationCallback( IDmNotify *pNotify )
{
	m_UndoMgr.RemoveNotificationCallback( pNotify );
}

bool CDataModel::IsSuppressingNotify( ) const
{
	return GetUndoMgr()->IsSuppressingNotify( );
}

void CDataModel::SetSuppressingNotify( bool bSuppress )
{
	GetUndoMgr()->SetSuppressingNotify( bSuppress );
}

void CDataModel::PushNotificationScope( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	GetUndoMgr()->PushNotificationScope( pReason, nNotifySource, nNotifyFlags );
}

void CDataModel::PopNotificationScope( bool bAbort )
{
	GetUndoMgr()->PopNotificationScope( bAbort );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Undo/Redo support
//-----------------------------------------------------------------------------
void CDataModel::SetUndoEnabled( bool enable )
{
	if ( enable )
	{
		GetUndoMgr()->EnableUndo();
	}
	else
	{
		GetUndoMgr()->DisableUndo();
	}
}

bool CDataModel::IsUndoEnabled() const
{
	return GetUndoMgr()->IsEnabled();
}

bool CDataModel::UndoEnabledForElement( const CDmElement *pElement ) const
{
	// elements not in any file don't participate in undo
	Assert( pElement );
	return IsUndoEnabled() && pElement && pElement->GetFileId() != DMFILEID_INVALID;
}

bool CDataModel::IsDirty() const
{
	return GetUndoMgr()->HasUndoData();
}

bool CDataModel::CanUndo() const
{
	return GetUndoMgr()->HasUndoData();
}

bool CDataModel::CanRedo() const
{
	return GetUndoMgr()->HasRedoData();
}

void CDataModel::StartUndo( const char *undodesc, const char *redodesc, int nChainingID /* = 0 */ )
{
	GetUndoMgr()->PushUndo( undodesc, redodesc, nChainingID );
}

void CDataModel::FinishUndo()
{
	GetUndoMgr()->PushRedo();
}

void CDataModel::AbortUndoableOperation()
{
	GetUndoMgr()->AbortUndoableOperation();
}

void CDataModel::ClearRedo()
{
	if ( GetUndoMgr()->HasRedoData() )
	{
		GetUndoMgr()->WipeRedo();
	}
}

const char *CDataModel::GetUndoDesc()
{
	return GetUndoMgr()->UndoDesc();
}

const char *CDataModel::GetRedoDesc()
{
	return GetUndoMgr()->RedoDesc();
}

// From the UI, perform the Undo operation
void CDataModel::Undo()
{
	GetUndoMgr()->Undo();
}

void CDataModel::Redo()
{
	GetUndoMgr()->Redo();
}

// if true, undo records spew as they are added
void CDataModel::TraceUndo( bool state )
{
	GetUndoMgr()->TraceUndo( state );
}

void CDataModel::AddUndoElement( IUndoElement *pElement )
{
	GetUndoMgr()->AddUndoElement( pElement );
}

void CDataModel::ClearUndo()
{
	GetUndoMgr()->WipeUndo();
	GetUndoMgr()->WipeRedo();

	m_bDeleteOrphanedElements = true; // next time we delete unreferenced elements, delete orphaned subtrees as well
}

CUtlSymbolLarge CDataModel::GetUndoDescInternal( const char *context )
{
	return GetUndoMgr()->GetUndoDescInternal( context );
}

CUtlSymbolLarge CDataModel::GetRedoDescInternal( const char *context )
{
	return GetUndoMgr()->GetRedoDescInternal( context );
}

void CDataModel::GetUndoInfo( CUtlVector< UndoInfo_t >& list )
{
	GetUndoMgr()->GetUndoInfo( list );
}

const char *CDataModel::GetUndoString( CUtlSymbolLarge sym )
{
	return sym.String();
}


//-----------------------------------------------------------------------------
//
// Methods related to attribute handles
//
//-----------------------------------------------------------------------------
DmAttributeHandle_t	CDataModel::AcquireAttributeHandle( CDmAttribute *pAttribute )
{
	DmAttributeHandle_t hAttribute = (DmAttributeHandle_t)m_AttributeHandles.AddHandle();
	m_AttributeHandles.SetHandle( hAttribute, pAttribute );
	return hAttribute;
}

void CDataModel::ReleaseAttributeHandle( DmAttributeHandle_t hAttribute )
{
	if ( hAttribute != DMATTRIBUTE_HANDLE_INVALID )
	{
		m_AttributeHandles.RemoveHandle( hAttribute );
	}
}

CDmAttribute *CDataModel::GetAttribute( DmAttributeHandle_t h )
{
	return m_AttributeHandles.GetHandle( h );
}

bool CDataModel::IsAttributeHandleValid( DmAttributeHandle_t h ) const
{
	return m_AttributeHandles.IsHandleValid( h );
}


//-----------------------------------------------------------------------------
//
// Methods related to clipboard contexts
//
//-----------------------------------------------------------------------------
void CDataModel::EmptyClipboard()
{
	GetClipboardMgr()->EmptyClipboard( true );
}

void CDataModel::SetClipboardData( CUtlVector< KeyValues * >& data, IClipboardCleanup *pfnOptionalCleanuFunction /*= 0*/ )
{
	GetClipboardMgr()->SetClipboardData( data, pfnOptionalCleanuFunction );
}

void CDataModel::AddToClipboardData( KeyValues *add )
{
	GetClipboardMgr()->AddToClipboardData( add );
}

void CDataModel::GetClipboardData( CUtlVector< KeyValues * >& data )
{
	GetClipboardMgr()->GetClipboardData( data );
}

bool CDataModel::HasClipboardData() const
{
	return GetClipboardMgr()->HasClipboardData();
}

// Commits symbols in symbol table
void CDataModel::CommitSymbols()
{
	m_SymbolTable.Commit();
}




void CDataModel::AddOnElementCreatedCallback( const char *pElementType, IDmeElementCreated *callback )
{

	IDmElementFactory *pFactory = NULL;

	int idx = m_Factories.Find( pElementType );
	if ( idx == m_Factories.InvalidIndex() ) return;
	
	pFactory = m_Factories[ idx ]->GetFactory();
	
	Assert( pFactory );

	pFactory->AddOnElementCreatedCallback( callback );

}

void CDataModel::RemoveOnElementCreatedCallback( const char *pElementType, IDmeElementCreated *callback )
{

	IDmElementFactory *pFactory = NULL;

	int idx = m_Factories.Find( pElementType );
	if ( idx == m_Factories.InvalidIndex() ) return;
	
	pFactory = m_Factories[ idx ]->GetFactory();
	
	Assert( pFactory );

	pFactory->RemoveOnElementCreatedCallback( callback );
	
}


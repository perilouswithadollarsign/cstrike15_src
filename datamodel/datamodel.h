//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DATAMODEL_H
#define DATAMODEL_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmattribute.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmehandle.h"
#include "tier1/uniqueid.h"
#include "tier1/utlsymbol.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utldict.h"
#include "tier1/utlstring.h"
#include "tier1/utlhandletable.h"
#include "tier1/utlhash.h"
#include "tier2/tier2.h"
#include "clipboardmanager.h"
#include "undomanager.h"
#include "tier1/convar.h"
#include "tier0/vprof.h" 


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IDmElementFramework;
class IUndoElement;
class CDmElement;

enum DmHandleReleasePolicy
{
	HR_ALWAYS,
	HR_NEVER,
	HR_IF_NOT_REFERENCED,
};


//-----------------------------------------------------------------------------
// memory categories
//-----------------------------------------------------------------------------
enum
{
	MEMORY_CATEGORY_OUTER,
	MEMORY_CATEGORY_ELEMENT_INTERNAL,
	MEMORY_CATEGORY_DATAMODEL,
	MEMORY_CATEGORY_REFERENCES,
	MEMORY_CATEGORY_ATTRIBUTE_TREE,
	MEMORY_CATEGORY_ATTRIBUTE_OVERHEAD,
	MEMORY_CATEGORY_ATTRIBUTE_DATA,
	MEMORY_CATEGORY_ATTRIBUTE_COUNT,

	MEMORY_CATEGORY_COUNT,
};


//-----------------------------------------------------------------------------
// hash map of id->element, with the id storage optimized out
//-----------------------------------------------------------------------------
class CElementIdHash : public CUtlHash< DmElementHandle_t >
{
public:
	CElementIdHash( int nBucketCount = 0, int nGrowCount = 0, int nInitCount = 0 )
		: CUtlHash< DmElementHandle_t >( nBucketCount, nGrowCount, nInitCount, CompareFunc, KeyFunc )
	{
	}

protected:
	typedef CUtlHash< DmElementHandle_t > BaseClass;

	static bool CompareFunc( DmElementHandle_t const& a, DmElementHandle_t const& b ) { return a == b; }
	static bool IdCompareFunc( DmElementHandle_t const& hElement, DmObjectId_t const& id )
	{
		CDmElement *pElement = g_pDataModel->GetElement( hElement );
		Assert( pElement );
		if ( !pElement )
			return false;

		return IsUniqueIdEqual( id, pElement->GetId() );
	}

	static unsigned int KeyFunc( DmElementHandle_t const& hElement )
	{
		CDmElement *pElement = g_pDataModel->GetElement( hElement );
		Assert( pElement );
		if ( !pElement )
			return 0;

		return *( unsigned int* )&pElement->GetId();
	}
	static unsigned int IdKeyFunc( DmObjectId_t const &src )
	{
		return *(unsigned int*)&src;
	}

protected:
	bool DoFind( DmObjectId_t const &src, unsigned int *pBucket, int *pIndex )
	{
		// generate the data "key"
		unsigned int key = IdKeyFunc( src );

		// hash the "key" - get the correct hash table "bucket"
		unsigned int ndxBucket;
		if( m_bPowerOfTwo )
		{
			*pBucket = ndxBucket = ( key & m_ModMask );
		}
		else
		{
			int bucketCount = m_Buckets.Count();
			*pBucket = ndxBucket = key % bucketCount;
		}

		int ndxKeyData;
		CUtlVector< DmElementHandle_t > &bucket = m_Buckets[ndxBucket];
		int keyDataCount = bucket.Count();
		for( ndxKeyData = 0; ndxKeyData < keyDataCount; ndxKeyData++ )
		{
			if( IdCompareFunc( bucket.Element( ndxKeyData ), src ) )
				break;
		}

		if( ndxKeyData == keyDataCount )
			return false;

		*pIndex = ndxKeyData;
		return true;
	}

public:
	UtlHashHandle_t Find( DmElementHandle_t const &src ) { return BaseClass::Find( src ); }
	UtlHashHandle_t Find( DmObjectId_t const &src )
	{
		unsigned int ndxBucket;
		int ndxKeyData;

		if ( DoFind( src, &ndxBucket, &ndxKeyData ) )
			return BuildHandle( ndxBucket, ndxKeyData );

		return InvalidHandle();
	}
};

//-----------------------------------------------------------------------------
// struct to hold the set of elements in any given file
//-----------------------------------------------------------------------------

struct FileElementSet_t
{
	FileElementSet_t( CUtlSymbolLarge filename = UTL_INVAL_SYMBOL_LARGE, CUtlSymbolLarge format = UTL_INVAL_SYMBOL_LARGE ) :
		m_filename( filename ), m_format( format ),
		m_hRoot( DMELEMENT_HANDLE_INVALID ),
		m_bLoaded( true ),
		m_nElements( 0 ),
		m_fileModificationTime( 0 )
	{
	}
	FileElementSet_t( const FileElementSet_t& that ) : m_filename( that.m_filename ), m_format( that.m_format ), m_hRoot( DMELEMENT_HANDLE_INVALID ), m_bLoaded( that.m_bLoaded ), m_nElements( that.m_nElements )
	{
		// the only time this should be copy constructed is when passing in an empty set to the parent array
		// otherwise it could get prohibitively expensive time and memory wise
		Assert( that.m_nElements == 0 );
	}

	CUtlSymbolLarge m_filename;
	CUtlSymbolLarge m_format;
	CDmeCountedHandle m_hRoot;
	bool m_bLoaded;
	int m_nElements;
	long m_fileModificationTime;
};


//-----------------------------------------------------------------------------
// Purpose: Versionable factor for element types
//-----------------------------------------------------------------------------
class CDataModel : public CBaseAppSystem< IDataModel >
{
	typedef CBaseAppSystem< IDataModel > BaseClass;

public:
	CDataModel();
	virtual ~CDataModel();

// External interface
public:
	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of IDataModel
	virtual void				AddElementFactory( CDmElementFactoryHelper *pFactoryHelper );
	virtual CDmElementFactoryHelper	*GetElementFactoryHelper( const char *pClassName );
	virtual bool				HasElementFactory( const char *pElementType ) const;
	virtual void				SetDefaultElementFactory( IDmElementFactory *pFactory );
	virtual int					GetFirstFactory() const;
	virtual int					GetNextFactory( int index ) const;
	virtual bool				IsValidFactory( int index ) const;
	virtual char const			*GetFactoryName( int index ) const;
	virtual DmElementHandle_t	CreateElement( CUtlSymbolLarge typeSymbol, const char *pElementName, DmFileId_t fileid, const DmObjectId_t *pObjectID = NULL );
	virtual DmElementHandle_t	CreateElement( const char *pTypeName, const char *pElementName, DmFileId_t fileid, const DmObjectId_t *pObjectID = NULL );
	virtual void				DestroyElement( DmElementHandle_t hElement );
	virtual	CDmElement*			GetElement( DmElementHandle_t hElement ) const;
	virtual CUtlSymbolLarge			GetElementType( DmElementHandle_t hElement ) const;
	virtual const char*			GetElementName( DmElementHandle_t hElement ) const;
	virtual const DmObjectId_t&	GetElementId( DmElementHandle_t hElement ) const;
	virtual const char			*GetAttributeNameForType( DmAttributeType_t attType ) const;
	virtual DmAttributeType_t	GetAttributeTypeForName( const char *name ) const;

	virtual void				AddSerializer( IDmSerializer *pSerializer );
	virtual void				AddLegacyUpdater( IDmLegacyUpdater *pUpdater );
	virtual void				AddFormatUpdater( IDmFormatUpdater *pUpdater );
	virtual const char*			GetFormatExtension( const char *pFormatName );
	virtual const char*			GetFormatDescription( const char *pFormatName );
	virtual int					GetFormatCount() const;
	virtual const char *		GetFormatName( int i ) const;
	virtual const char *		GetDefaultEncoding( const char *pFormatName );
	virtual int					GetEncodingCount() const;
	virtual const char *		GetEncodingName( int i ) const;
	virtual bool				IsEncodingBinary( const char *pEncodingName ) const;
	virtual bool				DoesEncodingStoreVersionInFile( const char *pEncodingName ) const;

	virtual void				SetSerializationDelimiter( CUtlCharConversion *pConv );
	virtual void				SetSerializationArrayDelimiter( const char *pDelimiter );
	virtual bool				IsUnserializing();
	virtual bool				Serialize( CUtlBuffer &outBuf, const char *pEncodingName, const char *pFormatName, DmElementHandle_t hRoot );
	virtual bool				Unserialize( CUtlBuffer &buf, const char *pEncodingName, const char *pSourceFormatName, const char *pFormatHint,
											 const char *pFileName, DmConflictResolution_t idConflictResolution, DmElementHandle_t &hRoot );
	virtual bool				UpdateUnserializedElements( const char *pSourceFormatName, int nSourceFormatVersion,
															DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot );
	virtual IDmSerializer*		FindSerializer( const char *pEncodingName ) const;
	virtual IDmLegacyUpdater*	FindLegacyUpdater( const char *pLegacyFormatName ) const;
	virtual IDmFormatUpdater*	FindFormatUpdater( const char *pFormatName ) const;
	virtual bool				SaveToFile( char const *pFileName, char const *pPathID, const char *pEncodingName, const char *pFormatName, CDmElement *pRoot );
	virtual DmFileId_t			RestoreFromFile( char const *pFileName, char const *pPathID, const char *pFormatHint, CDmElement **ppRoot, DmConflictResolution_t idConflictResolution = CR_DELETE_NEW, DmxHeader_t *pHeaderOut = NULL );
	virtual bool				IsDMXFormat( CUtlBuffer &buf ) const;

	virtual void				SetKeyValuesElementCallback( IElementForKeyValueCallback *pCallbackInterface );
	virtual const char *		GetKeyValuesElementName( const char *pszKeyName, int iNestingLevel );
	virtual CUtlSymbolLarge			GetSymbol( const char *pString );
	virtual int					GetElementsAllocatedSoFar();
	virtual int					GetMaxNumberOfElements();
	virtual int					GetAllocatedAttributeCount();
	virtual int					GetAllocatedElementCount();
	virtual DmElementHandle_t	FirstAllocatedElement();
	virtual DmElementHandle_t	NextAllocatedElement( DmElementHandle_t hElement );
	virtual int					EstimateMemoryUsage( DmElementHandle_t hElement, TraversalDepth_t depth = TD_DEEP );
	virtual void				SetUndoEnabled( bool enable );
	virtual bool				IsUndoEnabled() const;
	virtual bool				UndoEnabledForElement( const CDmElement *pElement ) const;
	virtual bool				IsDirty() const;
	virtual bool				CanUndo() const;
	virtual bool				CanRedo() const;
	virtual void				StartUndo( const char *undodesc, const char *redodesc, int nChainingID = 0 );
	virtual void				FinishUndo();
	virtual void				AbortUndoableOperation();
	virtual void				ClearRedo();
	virtual const char			*GetUndoDesc();
	virtual const char			*GetRedoDesc();
	virtual void				Undo();
	virtual void				Redo();
	virtual void				TraceUndo( bool state ); // if true, undo records spew as they are added
	virtual void				ClearUndo();
	virtual void				GetUndoInfo( CUtlVector< UndoInfo_t >& list );
	virtual const char *		GetUndoString( CUtlSymbolLarge sym );
	virtual void				AddUndoElement( IUndoElement *pElement );
	virtual CUtlSymbolLarge			GetUndoDescInternal( const char *context );
	virtual CUtlSymbolLarge			GetRedoDescInternal( const char *context );
	virtual void				EmptyClipboard();
	virtual void				SetClipboardData( CUtlVector< KeyValues * >& data, IClipboardCleanup *pfnOptionalCleanuFunction = 0 );
	virtual void				AddToClipboardData( KeyValues *add );
	virtual void				GetClipboardData( CUtlVector< KeyValues * >& data );
	virtual bool				HasClipboardData() const;

 	virtual CDmAttribute *		GetAttribute( DmAttributeHandle_t h );
	virtual bool				IsAttributeHandleValid( DmAttributeHandle_t h ) const;
	virtual void				OnlyCreateUntypedElements( bool bEnable );
	virtual int					NumFileIds();
	virtual DmFileId_t			GetFileId( int i );
	virtual DmFileId_t			FindOrCreateFileId( const char *pFilename );
	virtual void				RemoveFileId( DmFileId_t fileid );
	virtual DmFileId_t			GetFileId( const char *pFilename );
	virtual const char *		GetFileName( DmFileId_t fileid );
	virtual void				SetFileName( DmFileId_t fileid, const char *pFileName );
	virtual const char *		GetFileFormat( DmFileId_t fileid );
	virtual void				SetFileFormat( DmFileId_t fileid, const char *pFormat );
	virtual DmElementHandle_t	GetFileRoot( DmFileId_t fileid );
	virtual void				SetFileRoot( DmFileId_t fileid, DmElementHandle_t hRoot );
	virtual long				GetFileModificationUTCTime( DmFileId_t fileid );
	virtual long				GetCurrentUTCTime();
	virtual void				UTCTimeToString( char *pString, int maxChars, long fileTime );
	virtual bool				IsFileLoaded( DmFileId_t fileid );
	virtual void				MarkFileLoaded( DmFileId_t fileid );
	virtual void				UnloadFile( DmFileId_t fileid );
	virtual int					NumElementsInFile( DmFileId_t fileid );
	virtual void				DontAutoDelete( DmElementHandle_t hElement );
	virtual void				MarkHandleInvalid( DmElementHandle_t hElement );
	virtual void				MarkHandleValid( DmElementHandle_t hElement );
	virtual DmElementHandle_t	FindElement( const DmObjectId_t &id );
	virtual void				GetExistingElements( CElementIdHash &hash ) const;
	virtual	DmAttributeReferenceIterator_t	FirstAttributeReferencingElement( DmElementHandle_t hElement );
	virtual DmAttributeReferenceIterator_t	NextAttributeReferencingElement( DmAttributeReferenceIterator_t hAttrIter );
	virtual CDmAttribute *					GetAttribute( DmAttributeReferenceIterator_t hAttrIter );
	virtual bool				InstallNotificationCallback( IDmNotify *pNotify );
	virtual void				RemoveNotificationCallback( IDmNotify *pNotify );
	virtual bool				IsSuppressingNotify( ) const;
	virtual void				SetSuppressingNotify( bool bSuppress );
	virtual void				PushNotificationScope( const char *pReason, int nNotifySource, int nNotifyFlags );
	virtual void				PopNotificationScope( bool bAbort );
	virtual void				SetUndoDepth( int nSize );
	virtual void				DisplayMemoryStats(DmElementHandle_t hElement = DMELEMENT_HANDLE_INVALID);
	// Dump the symbol table to the console
	virtual void				DumpSymbolTable();
	virtual void				AddOnElementCreatedCallback( const char* pElementType, IDmeElementCreated *callback );
	virtual void				RemoveOnElementCreatedCallback( const char* pElementType, IDmeElementCreated *callback );
	
public:

	// Commits symbols in symbol table
	void						CommitSymbols();

	// Internal public methods
	int GetCurrentFormatVersion( const char *pFormatName );

	// CreateElement references the attribute list passed in via ref, so don't edit or purge ref's attribute list afterwards
	CDmElement* CreateElement( const DmElementReference_t &ref, const char *pElementType, const char *pElementName, DmFileId_t fileid, const DmObjectId_t *pObjectID );
	void DeleteElement( DmElementHandle_t hElement, DmHandleReleasePolicy hrp = HR_ALWAYS );

	// element handle related methods
	DmElementHandle_t AcquireElementHandle();
	void ReleaseElementHandle( DmElementHandle_t hElement );

	// Handles to attributes
	DmAttributeHandle_t AcquireAttributeHandle( CDmAttribute *pAttribute );
	void ReleaseAttributeHandle( DmAttributeHandle_t hAttribute );

	// remove orphaned element subtrees
	void FindAndDeleteOrphanedElements();

	void GetInvalidHandles( CUtlVector< DmElementHandle_t > &handles );
	void MarkHandlesValid( CUtlVector< DmElementHandle_t > &handles );
	void MarkHandlesInvalid( CUtlVector< DmElementHandle_t > &handles );

	// search id->handle table (both loaded and unloaded) for id, and if not found, create a new handle, map it to the id and return it
	DmElementHandle_t FindOrCreateElementHandle( const DmObjectId_t &id );

	// changes an element's id and associated mappings - generally during unserialization
	DmElementHandle_t ChangeElementId( DmElementHandle_t hElement, const DmObjectId_t &oldId, const DmObjectId_t &newId );

	DmElementReference_t *FindElementReference( DmElementHandle_t hElement, DmObjectId_t **ppId = NULL );

	void RemoveUnreferencedElements();

	void RemoveElementFromFile( DmElementHandle_t hElement, DmFileId_t fileid );
	void AddElementToFile( DmElementHandle_t hElement, DmFileId_t fileid );

	void NotifyState( int nNotifyFlags );

	int EstimateMemoryOverhead() const;

	bool IsCreatingUntypedElements() const { return m_bOnlyCreateUntypedElements; }

	void UpdateReferenceToElements( CDmAttribute *pAttr, CDmElement *pChild, bool bDetach );
	void UpdateReferencesToElements( CDmElement *pElement, bool bDetach );

private:
	struct MailingList_t
	{
		CUtlVector<DmElementHandle_t> m_Elements;
	};

	struct ElementIdHandlePair_t
	{
		DmObjectId_t m_id;
		DmElementReference_t m_ref;
		ElementIdHandlePair_t() {}
		explicit ElementIdHandlePair_t( const DmObjectId_t &id ) : m_ref()
		{
			CopyUniqueId( id, &m_id );
		}
		ElementIdHandlePair_t( const DmObjectId_t &id, const DmElementReference_t &ref ) : m_ref( ref )
		{
			CopyUniqueId( id, &m_id );
		}
		ElementIdHandlePair_t( const ElementIdHandlePair_t& that ) : m_ref( that.m_ref )
		{
			CopyUniqueId( that.m_id, &m_id );
		}
		ElementIdHandlePair_t &operator=( const ElementIdHandlePair_t &that )
		{
			CopyUniqueId( that.m_id, &m_id );
			m_ref = that.m_ref;
			return *this;
		}
		static unsigned int HashKey( const ElementIdHandlePair_t& that )
		{
			return *( unsigned int* )&that.m_id.m_Value;
		}
		static bool Compare( const ElementIdHandlePair_t& a, const ElementIdHandlePair_t& b )
		{
			return IsUniqueIdEqual( a.m_id, b.m_id );
		}
	};

private:
	CDmElement *Unserialize( CUtlBuffer& buf );
	void Serialize( CDmElement *element, CUtlBuffer& buf );

	// Read the header, return the version (or false if it's not a DMX file)
	bool ReadDMXHeader( CUtlBuffer &inBuf, DmxHeader_t *pHeader ) const;
	const char *GetEncodingFromLegacyFormat( const char *pLegacyFormatName ) const;
	bool IsValidNonDMXFormat( const char *pFormatName ) const;
	bool IsLegacyFormat( const char *pFormatName ) const;

	// Returns the current undo manager
	CUndoManager* GetUndoMgr();
	const CUndoManager* GetUndoMgr() const;
	CClipboardManager *GetClipboardMgr();
	const CClipboardManager *GetClipboardMgr() const;

	void UnloadFile( DmFileId_t fileid, bool bDeleteElements );
	void SetFileModificationUTCTime( DmFileId_t fileid, long fileModificationTime );

	friend class CDmeElementRefHelper;
	friend class CDmAttribute;
	template< class T > friend class CDmArrayAttributeOp;

	void OnElementReferenceAdded  ( DmElementHandle_t hElement, CDmAttribute *pAttribute );
	void OnElementReferenceRemoved( DmElementHandle_t hElement, CDmAttribute *pAttribute );
	void OnElementReferenceAdded  ( DmElementHandle_t hElement, HandleType_t handleType );
	void OnElementReferenceRemoved( DmElementHandle_t hElement, HandleType_t handleType );

	void BuildHistogramForHandles( CUtlMap< CUtlSymbolLarge, struct DmMemoryInfo_t > &typeHistogram, CUtlVector< DmElementHandle_t > &handles );

private:
	CUtlVector< IDmSerializer* >		m_Serializers;
	CUtlVector< IDmLegacyUpdater* >		m_LegacyUpdaters;
	CUtlVector< IDmFormatUpdater* >		m_FormatUpdaters;

	IDmElementFactory *m_pDefaultFactory;
	CUtlDict< CDmElementFactoryHelper*, int >	m_Factories;
	CUtlSymbolTableLargeMT m_SymbolTable;
	CUtlHandleTable< CDmElement, 21 > m_Handles;
	CUtlHandleTable< CDmAttribute, 21 > m_AttributeHandles;
	CUndoManager m_UndoMgr;

	bool m_bIsUnserializing : 1;
	bool m_bUnableToSetDefaultFactory : 1;
	bool m_bOnlyCreateUntypedElements : 1;
	bool m_bUnableToCreateOnlyUntypedElements : 1;
	bool m_bDeleteOrphanedElements : 1;

	CUtlHandleTable< FileElementSet_t, 20 > m_openFiles;

	CElementIdHash m_elementIds;
	CUtlHash< ElementIdHandlePair_t > m_unloadedIdElementMap;

	CClipboardManager m_ClipboardMgr;
	IElementForKeyValueCallback *m_pKeyvaluesCallbackInterface;

	int m_nElementsAllocatedSoFar;
	int m_nMaxNumberOfElements;
};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
extern CDataModel *g_pDataModelImp;


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline CUndoManager* CDataModel::GetUndoMgr()
{
	return &m_UndoMgr;
}

inline const CUndoManager* CDataModel::GetUndoMgr() const
{
	return &m_UndoMgr;
}

inline void CDataModel::NotifyState( int nNotifyFlags )
{
	GetUndoMgr()->NotifyState( nNotifyFlags );
}

inline CClipboardManager *CDataModel::GetClipboardMgr()
{
	return &m_ClipboardMgr;
}

inline const CClipboardManager *CDataModel::GetClipboardMgr() const
{
	return &m_ClipboardMgr;
}


//-----------------------------------------------------------------------------
// Methods of DmElement which are public to datamodel
//-----------------------------------------------------------------------------
class CDmeElementAccessor
{
public:
	static void Purge( CDmElement *pElement )													{ pElement->Purge(); }
	static void SetId( CDmElement *pElement, const DmObjectId_t &id )							{ pElement->SetId( id ); }
	static bool IsDirty( const CDmElement *pElement )											{ return pElement->IsDirty(); }
	static void MarkDirty( CDmElement *pElement, bool dirty = true )							{ pElement->MarkDirty( dirty ); }
	static void MarkAttributesClean( CDmElement *pElement )										{ pElement->MarkAttributesClean(); }
	static void DisableOnChangedCallbacks( CDmElement *pElement )								{ pElement->DisableOnChangedCallbacks(); }
	static void EnableOnChangedCallbacks( CDmElement *pElement )								{ pElement->EnableOnChangedCallbacks(); }
	static bool AreOnChangedCallbacksEnabled( CDmElement *pElement )							{ return pElement->AreOnChangedCallbacksEnabled(); }
	static void FinishUnserialization( CDmElement *pElement )									{ pElement->FinishUnserialization(); }
	static void AddAttributeByPtr( CDmElement *pElement, CDmAttribute *ptr )					{ pElement->AddAttributeByPtr( ptr ); }
	static void RemoveAttributeByPtrNoDelete( CDmElement *pElement, CDmAttribute *ptr )			{ pElement->RemoveAttributeByPtrNoDelete( ptr); }
	static void ChangeHandle( CDmElement *pElement, DmElementHandle_t handle )					{ pElement->ChangeHandle( handle ); }
	static DmElementReference_t	*GetReference( CDmElement *pElement )							{ return pElement->GetReference(); }
	static void SetReference( CDmElement *pElement, const DmElementReference_t &ref )			{ pElement->SetReference( ref ); }
	static int EstimateMemoryUsage( CDmElement *pElement, CUtlHash< DmElementHandle_t > &visited, TraversalDepth_t depth, int *pCategories ) { return pElement->EstimateMemoryUsage( visited, depth, pCategories ); }
	static void PerformConstruction( CDmElement *pElement )										{ pElement->PerformConstruction(); }
	static void PerformDestruction( CDmElement *pElement )										{ pElement->PerformDestruction(); }
	static void OnAdoptedFromUndo( CDmElement *pElement )										{ pElement->OnAdoptedFromUndo(); }
	static void OnOrphanedToUndo( CDmElement *pElement )										{ pElement->OnOrphanedToUndo(); }
};

#endif // DATAMODEL_H

//====== Copyright � 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMELEMENT_H
#define DMELEMENT_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlmap.h"
#include "tier1/utlhash.h"
#include "tier1/utlvector.h"
#include "tier1/utlsymbol.h"
#include "tier1/mempool.h"
#include "datamodel/attributeflags.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmvar.h"
#include "tier0/vprof.h" 
#include "tier1/utlsymbollarge.h"

//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------
class CDmAttribute;
class Color;
class Vector;
class QAngle;
class Quaternion;
class VMatrix;
class CDmElement;


//-----------------------------------------------------------------------------
// Suppress some SWIG warnings, only for SWIG.  Here because many SWIG
// projects %import this header directly
//-----------------------------------------------------------------------------
#ifdef SWIG
%ignore	CAttributeReferenceIterator::operator++;
%warnfilter( 302 ) FindReferringElement;
%warnfilter( 302 ) CopyElements;
%warnfilter( 509 ) CDmElement::SetParity;
#endif // SWIG

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
typedef bool (CDmElement::*pfnCommandMethod)( const char *command, const char *args );

// element/element array traversal path item - assumes the full path does NOT contain cycles
struct ElementPathItem_t
{
	ElementPathItem_t( DmElementHandle_t hElem = DMELEMENT_HANDLE_INVALID,
						DmAttributeHandle_t hAttr = DMATTRIBUTE_HANDLE_INVALID,
						int idx = -1 )
		: hElement( hElem ), hAttribute( hAttr ), nIndex( idx )
	{
	}

	// only uses hElement so that it can be used to search for elements
	bool operator==( const ElementPathItem_t &that ) const
	{
		return hElement == that.hElement;
	}

	DmElementHandle_t hElement;
	DmAttributeHandle_t hAttribute;
	int nIndex;
};


//-----------------------------------------------------------------------------
// singly-linked attribute list
//-----------------------------------------------------------------------------
struct DmAttributeList_t
{
	DmAttributeList_t() : m_hAttribute( DMATTRIBUTE_HANDLE_INVALID ), m_pNext( NULL ) {}
	DmAttributeHandle_t m_hAttribute;
	DmAttributeList_t *m_pNext;

private:
	DECLARE_FIXEDSIZE_ALLOCATOR( DmAttributeList_t );
};

//-----------------------------------------------------------------------------
// helper class to allow CDmeHandle access to g_pDataModelImp
//-----------------------------------------------------------------------------
enum HandleType_t
{
	HT_WEAK,
	HT_STRONG,
	HT_UNDO,
};

class CDmeElementRefHelper
{
protected:
	void Ref  ( DmElementHandle_t hElement, HandleType_t handleType );
	void Unref( DmElementHandle_t hElement, HandleType_t handleType );
};

//-----------------------------------------------------------------------------
// element reference struct - containing attribute referrers and handle refcount
//-----------------------------------------------------------------------------
struct DmElementReference_t
{
	explicit DmElementReference_t( DmElementHandle_t hElement = DMELEMENT_HANDLE_INVALID ) :
		m_hElement( hElement ), m_nWeakHandleCount( 0 ), m_nStrongHandleCount( 0 ), m_nUndoHandleCount( 0 ), m_bHasEverBeenReferenced( false )
	{
	}
	DmElementReference_t( const DmElementReference_t &that ) :
		m_hElement( that.m_hElement ), m_nWeakHandleCount( that.m_nWeakHandleCount ),
		m_nStrongHandleCount( that.m_nStrongHandleCount ), m_nUndoHandleCount( that.m_nUndoHandleCount ),
		m_bHasEverBeenReferenced( that.m_bHasEverBeenReferenced ), m_attributes( that.m_attributes )
	{
	}
	DmElementReference_t &operator=( const DmElementReference_t &that )
	{
		m_hElement = that.m_hElement;
		m_nWeakHandleCount = that.m_nWeakHandleCount;
		m_nStrongHandleCount = that.m_nStrongHandleCount;
		m_nUndoHandleCount = that.m_nUndoHandleCount;
		m_bHasEverBeenReferenced = that.m_bHasEverBeenReferenced;
		m_attributes.m_hAttribute = that.m_attributes.m_hAttribute;
		m_attributes.m_pNext = that.m_attributes.m_pNext;
		return *this;
	}
	~DmElementReference_t()
	{
		//		Assert( !IsStronglyReferenced() );
	}

	void AddAttribute( CDmAttribute *pAttribute );
	void RemoveAttribute( CDmAttribute *pAttribute );
	bool FindAttribute( CDmAttribute *pAttribute );

	bool IsStronglyReferenced() // should this element be kept around (even if it's DmElementHandle_t is invalidated)
	{
		return m_attributes.m_hAttribute != DMATTRIBUTE_HANDLE_INVALID || m_nStrongHandleCount > 0;
	}

	bool IsWeaklyReferenced() // should we keep this element's DmElementHandle_t mapped to it's id (even if the element is deleted)
	{
		return IsStronglyReferenced() || IsReferencedByUndo() || m_nWeakHandleCount > 0;
	}

	bool IsReferencedByUndo()
	{
		return m_nUndoHandleCount > 0;
	}

	int EstimateMemoryOverhead()
	{
		int nBytes = 0;
		for ( DmAttributeList_t *pLink = m_attributes.m_pNext; pLink; pLink = pLink->m_pNext )
		{
			nBytes += sizeof( DmAttributeList_t );
		}
		return nBytes;
	}

	DmElementHandle_t m_hElement;
	unsigned int m_nWeakHandleCount   : 10;	// CDmeHandle<T> - for auto-hookup once the element comes back, mainly used by UI
	unsigned int m_nStrongHandleCount : 10;	// CDmeCountedElementRef - for preventing elements from being truly deleted, mainly used by undo and file root
	unsigned int m_nUndoHandleCount   : 10; // CDmeUndoHandle<T> - for undo only, to allow it to keep handles to elements that may be conceptually deleted
	bool m_bHasEverBeenReferenced     : 1;
	DmAttributeList_t m_attributes;
};


//-----------------------------------------------------------------------------
// Base DmElement we inherit from in higher-level classes 
//-----------------------------------------------------------------------------
class CDmElement
{
public:
	// Can be overridden by derived classes
	virtual	void		OnAttributeChanged( CDmAttribute *pAttribute ) {}
	virtual void		OnAttributeArrayElementAdded( CDmAttribute *pAttribute, int nFirstElem, int nLastElem ) {}
	virtual void		OnAttributeArrayElementRemoved( CDmAttribute *pAttribute, int nFirstElem, int nLastElem ) {}
	virtual void		Resolve() {}
	virtual	bool		IsA( CUtlSymbolLarge typeSymbol ) const;
	virtual int			GetInheritanceDepth( CUtlSymbolLarge typeSymbol ) const;
	virtual void		OnElementUnserialized() {}
	virtual void		OnElementSerialized() {}
	virtual int			AllocatedSize() const { return sizeof( CDmElement ); }

	// Returns the element handle
	DmElementHandle_t	GetHandle() const;

	// Attribute iteration, finding
	// NOTE: Passing a type into GetAttribute will return NULL if the attribute exists but isn't that type
	bool				HasAttribute( const char *pAttributeName, DmAttributeType_t type = AT_UNKNOWN ) const;
	CDmAttribute		*GetAttribute( const char *pAttributeName, DmAttributeType_t type = AT_UNKNOWN );
	const CDmAttribute	*GetAttribute( const char *pAttributeName, DmAttributeType_t type = AT_UNKNOWN ) const;
	int					AttributeCount() const;
	CDmAttribute*		FirstAttribute();
	const CDmAttribute*	FirstAttribute() const;

	// Element name, type, ID
	// WARNING: SetType() should only be used by format conversion methods (dmxconvert)
	CUtlSymbolLarge			GetType() const;
	const char *		GetTypeString() const;
	const char *		GetName() const;
	const DmObjectId_t&	GetId() const;
	void				SetType( const char *pType );
	void				SetName( const char* pName );

	// Attribute management
	CDmAttribute *		AddAttribute( const char *pAttributeName, DmAttributeType_t type );
	template< class E > CDmAttribute* AddAttributeElement( const char *pAttributeName );
	template< class E > CDmAttribute* AddAttributeElementArray( const char *pAttributeName );
	void				RemoveAttribute( const char *pAttributeName );
	void				RemoveAttributeByPtr( CDmAttribute *pAttributeName );
	void				RenameAttribute( const char *pAttributeName, const char *pNewName );

	// get attribute value
	template< class T > const T& GetValue( const char *pAttributeName ) const;
	template< class T > const T& GetValue( const char *pAttributeName, const T& defaultValue ) const;
	const char *		GetValueString( const char *pAttributeName ) const;
	template< class E > E* GetValueElement( const char *pAttributeName ) const;

	// set attribute value
	CDmAttribute*		SetValue( const char *pAttributeName, const void *value, size_t size, bool bCreateIfNotFound = true );
	template< class T > CDmAttribute* SetValue( const char *pAttributeName, const T& value, bool bCreateIfNotFound = true );
	template< class E >	CDmAttribute* SetValue( const char *pAttributeName, E* value, bool bCreateIfNotFound = true );

	// set attribute value if the attribute doesn't already exist
	CDmAttribute*		InitValue( const char *pAttributeName, const void *value, size_t size );
	template< class T > CDmAttribute* InitValue( const char *pAttributeName, const T& value );
	template< class E >	CDmAttribute* InitValue( const char *pAttributeName, E* value );

	// Parses an attribute from a string
	// Doesn't create an attribute if it doesn't exist and always preserves attribute type
	void				SetValueFromString( const char *pAttributeName, const char *value );
	const char			*GetValueAsString( const char *pAttributeName, char *pBuffer, size_t buflen ) const;

	// Helpers for our RTTI
	template< class E > bool IsA() const;
	bool				IsA( const char *pTypeName ) const;
	int					GetInheritanceDepth( const char *pTypeName ) const;
	static CUtlSymbolLarge	GetStaticTypeSymbol();

	// Indicates whether this element should be copied or not
	void				SetShared( bool bShared );
	bool				IsShared() const;

	// Copies an element and all its attributes
	CDmElement*			Copy( TraversalDepth_t depth = TD_DEEP ) const;

	// Copies attributes from a specified element
	void				CopyAttributesTo( CDmElement *pCopy, TraversalDepth_t depth = TD_DEEP ) const;

	// recursively set fileid's, with option to only change elements in the matched file
	void				SetFileId( DmFileId_t fileid, TraversalDepth_t depth, bool bOnlyIfMatch = false );
	DmFileId_t			GetFileId() const;

	bool				GetParity( int bit = 0 ) const;
	void				SetParity( bool bParity, int bit = 0 );
	void				SetParity( bool bParity, TraversalDepth_t depth, int bit = 0 ); // assumes that all elements that should be traversed have a parity of !bParity

	bool				IsOnlyInUndo() const;
	void				SetOnlyInUndo( bool bOnlyInUndo );

	// returns the first path to the element found traversing all element/element 
	// array attributes - not necessarily the shortest.
	// cycle-safe (skips any references to elements in the current path) 
	// but may re-traverse elements via different paths
	bool				FindElement( const CDmElement *pElement, CUtlVector< ElementPathItem_t > &elementPath, TraversalDepth_t depth ) const;
	bool				FindReferer( DmElementHandle_t hElement, CUtlVector< ElementPathItem_t > &elementPath, TraversalDepth_t depth ) const;
	void				RemoveAllReferencesToElement( CDmElement *pElement );
	bool				IsStronglyReferenced() { return m_ref.IsStronglyReferenced(); }

	// Estimates the memory usage of the element, its attributes, and child elements
	int					EstimateMemoryUsage( TraversalDepth_t depth = TD_DEEP );

	// mostly used for internal stuff, but it's occasionally useful to mark yourself dirty...
	bool				IsDirty() const;
	void				MarkDirty( bool dirty = true );

protected:
	// NOTE: These are protected to ensure that the factory is the only thing that can create these
						CDmElement( DmElementHandle_t handle, const char *objectType, const DmObjectId_t &id, const char *objectName, DmFileId_t fileid );
	virtual				~CDmElement();

	// Used by derived classes to do construction and setting up CDmaVars
	void				OnConstruction() { }
	void				OnDestruction() { }																
	virtual void		PerformConstruction();
	virtual void		PerformDestruction();

	virtual void		OnAdoptedFromUndo() {}
	virtual void		OnOrphanedToUndo() {}

	// Internal methods related to RTII
	static void			SetTypeSymbol( CUtlSymbolLarge sym );
	static bool			IsA_Implementation( CUtlSymbolLarge typeSymbol );
	static int			GetInheritanceDepth_Implementation( CUtlSymbolLarge typeSymbol, int nCurrentDepth );

	// Internal method for creating a copy of this element
	CDmElement*			CopyInternal( TraversalDepth_t depth = TD_DEEP ) const;

	// helper for making attributevarelementarray cleanup easier
	template< class T >	static void DeleteAttributeVarElementArray( T &array );

private:
	typedef CUtlMap< DmElementHandle_t, DmElementHandle_t, int > CRefMap;

	// Bogus constructor
	CDmElement();

	// internal recursive copy method - builds refmap of old element's handle -> copy's handle, and uses it to fixup references
	void				CopyAttributesTo( CDmElement *pCopy, CRefMap &refmap, TraversalDepth_t depth ) const;
	void				CopyElementAttribute( const CDmAttribute *pAttr, CDmAttribute *pCopyAttr, CRefMap &refmap, TraversalDepth_t depth ) const;
	void				CopyElementArrayAttribute( const CDmAttribute *pAttr, CDmAttribute *pCopyAttr, CRefMap &refmap, TraversalDepth_t depth ) const;
	void				FixupReferences( CUtlHashFast< DmElementHandle_t > &visited, const CRefMap &refmap, TraversalDepth_t depth );

	void				SetFileId( DmFileId_t fileid );
	void				SetFileId_R( CUtlHashFast< DmElementHandle_t > &visited, DmFileId_t fileid, TraversalDepth_t depth, DmFileId_t match, bool bOnlyIfMatch );

	CDmAttribute*		CreateAttribute( const char *pAttributeName, DmAttributeType_t type );
	void				RemoveAttribute( CDmAttribute **pAttrRef );
	CDmAttribute*		AddExternalAttribute( const char *pAttributeName, DmAttributeType_t type, void *pMemory );
	CDmAttribute		*FindAttribute( const char *pAttributeName ) const;

	void				Purge();
	void				SetId( const DmObjectId_t &id );

	void				MarkAttributesClean();

	void				DisableOnChangedCallbacks();
	void				EnableOnChangedCallbacks();
	bool				AreOnChangedCallbacksEnabled();
	void				FinishUnserialization();

	// Used by the undo system only.
	void				AddAttributeByPtr( CDmAttribute *ptr );
	void				RemoveAttributeByPtrNoDelete( CDmAttribute *ptr );

	// Should only be called from datamodel, who will take care of changing the fileset entry as well
	void				ChangeHandle( DmElementHandle_t handle );

	// returns element reference struct w/ list of referrers and handle count
	DmElementReference_t* GetReference();
	void				SetReference( const DmElementReference_t &ref );

	// Estimates memory usage
	int					EstimateMemoryUsage( CUtlHash< DmElementHandle_t > &visited, TraversalDepth_t depth, int *pCategories );

private:
	DmObjectId_t		m_Id; // UUID's like to be quad-aligned

protected:
	CDmaString			m_Name;

private:
	DmElementReference_t m_ref;
	CDmAttribute		*m_pAttributes;
	CUtlSymbolLarge		m_Type;
	DmFileId_t			m_fileId;

	bool				m_bDirty : 1;
	bool				m_bOnChangedCallbacksEnabled : 1;
	bool				m_bOnlyInUndo : 1; // only accessibly from the undo system
	uint				m_nParityBits : 28; // used as temporary state during traversal to avoid searching

	// Stores the type symbol
	static CUtlSymbolLarge	m_classType;

	// Factories can access our constructors
	template <class T> friend class CDmElementFactory;
	template <class T> friend class CDmAbstractElementFactory;
	template< class T > friend class CDmaVar;
	template< class T >	friend class CDmaArray;
	template< class T > friend class CDmaElementArray;
	template< class T, class B > friend class CDmaDecorator;
	template< class T > friend class CDmrElementArray;

	friend class CDmElementFactoryDefault;
	friend class CDmeElementAccessor;
	friend class CDmeOperator;

	template< class T >
	friend void CopyElements( const CUtlVector< T* > &from, CUtlVector< T* > &to, TraversalDepth_t depth );

	DECLARE_FIXEDSIZE_ALLOCATOR( CDmElement );

	typedef CDmElement BaseClass; // only CDmElement has itself as it's BaseClass - this lets us know we're at the top of the hierarchy
};


//-----------------------------------------------------------------------------
// Fast dynamic cast
//-----------------------------------------------------------------------------
template< class E >
inline E *CastElement( CDmElement *pElement )
{
	if ( pElement && pElement->IsA( E::GetStaticTypeSymbol() ) )
		return static_cast< E* >( pElement );
	return NULL;
}

template< class E >
inline const E *CastElement( const CDmElement *pElement )
{
	if ( pElement && pElement->IsA( E::GetStaticTypeSymbol() ) )
		return static_cast< const E* >( pElement );
	return NULL;
}


//-----------------------------------------------------------------------------
// Constant fast dynamic cast
//-----------------------------------------------------------------------------
template< class E >
const inline E *CastElementConst( const CDmElement *pElement )
{
	if ( pElement && pElement->IsA( E::GetStaticTypeSymbol() ) )
		return static_cast< const E* >( pElement );
	return NULL;
}


//-----------------------------------------------------------------------------
// type-safe element creation and accessor helpers - infers type name string from actual type
//-----------------------------------------------------------------------------
template< class E >
inline E *GetElement( DmElementHandle_t hElement )
{
	CDmElement *pElement = g_pDataModel->GetElement( hElement );
	return CastElement< E >( pElement );
}


//-----------------------------------------------------------------------------
// Typesafe element creation + destruction
//-----------------------------------------------------------------------------
template< class E >
inline E *CreateElement( const char *pObjectName, DmFileId_t fileid, const DmObjectId_t *pObjectID = NULL )
{
	return GetElement< E >( g_pDataModel->CreateElement( E::GetStaticTypeSymbol(), pObjectName, fileid, pObjectID ) );
}

template< class E >
inline E *CreateElement( const char *pElementType, const char *pObjectName, DmFileId_t fileid, const DmObjectId_t *pObjectID = NULL )
{
	return GetElement< E >( g_pDataModel->CreateElement( pElementType, pObjectName, fileid, pObjectID ) );
}

inline void DestroyElement( CDmElement *pElement )
{
	if ( pElement )
	{
		g_pDataModel->DestroyElement( pElement->GetHandle() );
	}
}

void DestroyElement( CDmElement *pElement, TraversalDepth_t depth );


//-----------------------------------------------------------------------------
// allows elements to chain OnAttributeChanged up to their parents (or at least, referrers)
//-----------------------------------------------------------------------------
void InvokeOnAttributeChangedOnReferrers( DmElementHandle_t hElement, CDmAttribute *pChangedAttr );


//-----------------------------------------------------------------------------
// Gets attributes
//-----------------------------------------------------------------------------
inline CDmAttribute *CDmElement::GetAttribute( const char *pAttributeName, DmAttributeType_t type )
{
	CDmAttribute *pAttribute = FindAttribute( pAttributeName );
	if ( ( type != AT_UNKNOWN ) && pAttribute && ( pAttribute->GetType() != type ) )
		return NULL;
	return pAttribute;
}

inline const CDmAttribute *CDmElement::GetAttribute( const char *pAttributeName, DmAttributeType_t type ) const
{
	CDmAttribute *pAttribute = FindAttribute( pAttributeName );
	if ( ( type != AT_UNKNOWN ) && pAttribute && ( pAttribute->GetType() != type ) )
		return NULL;
	return pAttribute;
}


//-----------------------------------------------------------------------------
// AddAttribute calls
//-----------------------------------------------------------------------------
inline CDmAttribute *CDmElement::AddAttribute( const char *pAttributeName, DmAttributeType_t type )
{
	CDmAttribute *pAttribute = FindAttribute( pAttributeName );
	if ( pAttribute )
		return ( pAttribute->GetType() == type ) ? pAttribute : NULL;
	pAttribute = CreateAttribute( pAttributeName, type );
	return pAttribute;
}

template< class E > inline CDmAttribute *CDmElement::AddAttributeElement( const char *pAttributeName )
{
	CDmAttribute *pAttribute = AddAttribute( pAttributeName, AT_ELEMENT );
	if ( !pAttribute )
		return NULL;

	// FIXME: If the attribute exists but has a different element type symbol, should we complain?
	pAttribute->SetElementTypeSymbol( E::GetStaticTypeSymbol() );
	return pAttribute;
}

template< class E > inline CDmAttribute *CDmElement::AddAttributeElementArray( const char *pAttributeName )
{
	CDmAttribute *pAttribute = AddAttribute( pAttributeName, AT_ELEMENT_ARRAY );
	if ( !pAttribute )
		return NULL;

	// FIXME: If the attribute exists but has a different element type symbol, should we complain?
	pAttribute->SetElementTypeSymbol( E::GetStaticTypeSymbol() );
	return pAttribute;
}


//-----------------------------------------------------------------------------
// GetValue methods
//-----------------------------------------------------------------------------
template< class T >
inline const T& CDmElement::GetValue( const char *pAttributeName, const T& defaultVal ) const
{
	const CDmAttribute *pAttribute = FindAttribute( pAttributeName );
	if ( pAttribute != NULL )
		return pAttribute->GetValue<T>();
	return defaultVal;
}

template< class T >
inline const T& CDmElement::GetValue( const char *pAttributeName ) const
{
	static CDmaVar<T> defaultVal;
	return GetValue( pAttributeName, defaultVal.Get() );
}

inline const char *CDmElement::GetValueString( const char *pAttributeName ) const
{
	CUtlSymbolLarge symbol = GetValue<CUtlSymbolLarge>( pAttributeName );
	if ( symbol == UTL_INVAL_SYMBOL_LARGE )
		return NULL;

	return symbol.String();
}

template< class E >
inline E* CDmElement::GetValueElement( const char *pAttributeName ) const
{
	DmElementHandle_t h = GetValue< DmElementHandle_t >( pAttributeName );
	return GetElement<E>( h );
}


//-----------------------------------------------------------------------------
// SetValue methods
//-----------------------------------------------------------------------------
template< class T >
inline CDmAttribute* CDmElement::SetValue( const char *pAttributeName, const T& value, bool bCreateIfNotFound /*= true*/ )
{
	CDmAttribute *pAttribute = FindAttribute( pAttributeName );
	if ( !pAttribute && bCreateIfNotFound )
	{
		pAttribute = CreateAttribute( pAttributeName, CDmAttributeInfo<T>::AttributeType() );
	}
	if ( pAttribute )
	{
		pAttribute->SetValue( value );
		return pAttribute;
	}
	return NULL;
}

template< class E >
inline CDmAttribute* CDmElement::SetValue( const char *pAttributeName, E* pElement, bool bCreateIfNotFound /*= true*/ )
{
	DmElementHandle_t hElement = pElement ? pElement->GetHandle() : DMELEMENT_HANDLE_INVALID;
	return SetValue( pAttributeName, hElement, bCreateIfNotFound );
}

template<>
inline CDmAttribute* CDmElement::SetValue( const char *pAttributeName, const char *pValue, bool bCreateIfNotFound /*= true*/ )
{
	// We don't want to add any extra entries into the string table so if bCreateIfNotFound is 
	// false, then check to see if the attribute exists before adding the string to the table.
	if ( !bCreateIfNotFound )
	{
		if ( HasAttribute( pAttributeName, AT_STRING ) == false )
			return NULL;
	}

	CUtlSymbolLarge symbol = g_pDataModel->GetSymbol( pValue );
	return SetValue( pAttributeName, symbol );
}

template<>
inline CDmAttribute* CDmElement::SetValue( const char *pAttributeName, char *pValue, bool bCreateIfNotFound /*= true*/ )
{
	return SetValue( pAttributeName, (const char *)pValue, bCreateIfNotFound );
}

inline CDmAttribute* CDmElement::SetValue( const char *pAttributeName, const void *pValue, size_t nSize, bool bCreateIfNotFound /*= true*/ )
{
	CUtlBinaryBlock buf( pValue, nSize );
	return SetValue( pAttributeName, buf, bCreateIfNotFound );
}


//-----------------------------------------------------------------------------
// AddValue methods( set value if not found )
//-----------------------------------------------------------------------------
template< class T >
inline CDmAttribute* CDmElement::InitValue( const char *pAttributeName, const T& value )
{
	CDmAttribute *pAttribute = GetAttribute( pAttributeName );
	if ( !pAttribute )
		return SetValue( pAttributeName, value );
	return pAttribute;
}

template< class E >
inline CDmAttribute* CDmElement::InitValue( const char *pAttributeName, E* pElement )
{
	DmElementHandle_t hElement = pElement ? pElement->GetHandle() : DMELEMENT_HANDLE_INVALID;
	return InitValue( pAttributeName, hElement );
}

template<>
inline CDmAttribute* CDmElement::InitValue( const char *pAttributeName, const char *pValue )
{
	CUtlSymbolLarge symbol = g_pDataModel->GetSymbol( pValue );
	return InitValue( pAttributeName, symbol );
}

inline  CDmAttribute* CDmElement::InitValue( const char *pAttributeName, const void *pValue, size_t size )
{
	CDmAttribute *pAttribute = GetAttribute( pAttributeName );
	if ( !pAttribute )
		return SetValue( pAttributeName, pValue, size );
	return pAttribute;
}


//-----------------------------------------------------------------------------
// Returns the type, name, id, fileId
//-----------------------------------------------------------------------------
inline CUtlSymbolLarge CDmElement::GetType() const 
{ 
	return m_Type;
}

inline const char *CDmElement::GetTypeString() const
{
	return m_Type.String();
}

inline const char *CDmElement::GetName() const 
{ 
	return m_Name.Get(); 
}

inline void CDmElement::SetName( const char* pName )
{
	m_Name.Set( pName );
}

inline const DmObjectId_t& CDmElement::GetId() const 
{ 
	return m_Id;
}

inline DmFileId_t CDmElement::GetFileId() const
{
	return m_fileId;
}


//-----------------------------------------------------------------------------
// Controls whether the element should be copied by default
//-----------------------------------------------------------------------------
inline void CDmElement::SetShared( bool bShared )
{
	if ( bShared )
	{
		SetValue< bool >( "shared", true );
	}
	else
	{
		RemoveAttribute( "shared" );
	}
}

inline bool CDmElement::IsShared() const
{
	return GetValue< bool >( "shared" ); // if attribute doesn't exist, returns default bool value, which is false
}


//-----------------------------------------------------------------------------
// Copies attributes from a specified element
//-----------------------------------------------------------------------------
inline CDmElement* CDmElement::Copy( TraversalDepth_t depth ) const
{
	return CopyInternal( depth );
}


//-----------------------------------------------------------------------------
// RTTI
//-----------------------------------------------------------------------------
inline bool CDmElement::IsA_Implementation( CUtlSymbolLarge typeSymbol )
{
	return ( m_classType == typeSymbol ) || ( UTL_INVAL_SYMBOL_LARGE == typeSymbol );
}

inline int CDmElement::GetInheritanceDepth_Implementation( CUtlSymbolLarge typeSymbol, int nCurrentDepth )
{
	return IsA_Implementation( typeSymbol ) ? nCurrentDepth : -1;
}

inline CUtlSymbolLarge CDmElement::GetStaticTypeSymbol()
{
	return m_classType;
}

inline bool CDmElement::IsA( const char *pTypeName ) const
{												
	CUtlSymbolLarge typeSymbol = g_pDataModel->GetSymbol( pTypeName ); 
	return IsA( typeSymbol );				
}

template< class E > inline bool CDmElement::IsA() const		
{											
	return IsA( E::GetStaticTypeSymbol() ); 
}


//-----------------------------------------------------------------------------
// Helper for finding elements that refer to this element
//-----------------------------------------------------------------------------
class CAttributeReferenceIterator
{
public:
	explicit CAttributeReferenceIterator( const CDmElement *pElement ) :
		m_curr  ( pElement ? g_pDataModel->FirstAttributeReferencingElement( pElement->GetHandle() ) : DMATTRIBUTE_REFERENCE_ITERATOR_INVALID ),
		m_fileid( pElement ? pElement->GetFileId() : DMFILEID_INVALID )
	{
	}

	operator bool() const { return m_curr != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID; }

	CDmAttribute* operator*() const { return GetAttribute(); }

	CDmAttribute *GetAttribute() const { return g_pDataModel->GetAttribute( m_curr ); }
	CDmElement *GetOwner() const { if ( CDmAttribute *pAttr = GetAttribute() ) return pAttr->GetOwner(); return NULL; }

	CAttributeReferenceIterator& operator++() // prefix
	{
		m_curr = g_pDataModel->NextAttributeReferencingElement( m_curr );
		return *this;
	}
	CAttributeReferenceIterator operator++( int ) // postfix
	{
		CAttributeReferenceIterator prev = *this;
		m_curr = g_pDataModel->NextAttributeReferencingElement( m_curr );
		return prev;
	}

	template< class T >
	T *FilterReference( CUtlSymbolLarge symAttrName = UTL_INVAL_SYMBOL_LARGE, bool bMustBeInSameFile = false, TraversalDepth_t depth = TD_ALL ) const
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( m_curr );
		Assert( pAttribute );
		if ( !pAttribute )
			return NULL;

		if ( !ShouldTraverse( pAttribute, depth ) )
			return NULL;

		T *pParent = CastElement< T >( pAttribute->GetOwner() );
		if ( !pParent )
			return NULL;

		if ( symAttrName != UTL_INVAL_SYMBOL_LARGE && pAttribute->GetNameSymbol() != symAttrName )
			return NULL;

		if ( bMustBeInSameFile && ( pParent->GetFileId() != m_fileid ) )
			return NULL;

		return pParent;
	}

private:
	DmAttributeReferenceIterator_t m_curr;
	DmFileId_t m_fileid;
};

template< class T >
T *FindReferringElement( const CDmElement *pElement, const char *pAttrName = NULL, bool bMustBeInSameFile = true, TraversalDepth_t depth = TD_ALL )
{
	CUtlSymbolLarge sym = pAttrName ? g_pDataModel->GetSymbol( pAttrName ) : (CUtlSymbolLarge)UTL_INVAL_SYMBOL_LARGE;
	return FindReferringElement< T >( pElement, sym, bMustBeInSameFile, depth );
}

template< class T >
T *FindReferringElement( const CDmElement *pElement, CUtlSymbolLarge symAttrName = UTL_INVAL_SYMBOL_LARGE, bool bMustBeInSameFile = true, TraversalDepth_t depth = TD_ALL )
{
	for ( CAttributeReferenceIterator it( pElement ); it; ++it )
	{
		if ( T *pParent = it.FilterReference< T >( symAttrName, bMustBeInSameFile, depth ) )
			return pParent;
	}

	return NULL;
}

template< class T >
bool FindReferringElements( CUtlVector< T * >& list, const CDmElement *pElement, CUtlSymbolLarge symAttrName, bool bMustBeInSameFile = true, TraversalDepth_t depth = TD_ALL )
{
	for ( CAttributeReferenceIterator it( pElement ); it; ++it )
	{
		if ( T *pParent = it.FilterReference< T >( symAttrName, bMustBeInSameFile, depth ) )
		{
			list.AddToTail( pParent );
		}
	}

	return list.Count() > 0;
}


void RemoveElementFromRefereringAttributes( CDmElement *pElement, bool bPreserveOrder = true );


template< class T >
void FindAncestorsReferencingElement( const CDmElement *target, CUtlVector< T* >& list )
{
	FindReferringElements( list, target, UTL_INVAL_SYMBOL_LARGE, false );
}

template< class T >
T *FindAncestorReferencingElement( const CDmElement *target )
{
	return FindReferringElement< T >( target, UTL_INVAL_SYMBOL_LARGE, false );
}

template< class T >
T *FindAncestorReferencingElement_R_Impl( CUtlRBTree< CDmElement * >& visited, CDmElement *check )
{
	if ( visited.Find( check ) != visited.InvalidIndex() )
		return NULL;
		
	visited.Insert( check );

	// Pass one, see if it's in this ancestor list
	for ( CAttributeReferenceIterator it( check ); it; ++it )
	{
		if ( T *pParent = it.FilterReference< T >() )
			return pParent;
	}

	for ( CAttributeReferenceIterator it( check ); it; ++it )
	{
		if ( CDmElement *pParent = it.GetOwner() )
		{
			T *found = FindAncestorReferencingElement_R_Impl< T >( visited, pParent );
			if ( found )
				return found;
		}
	}
	return NULL;
}

template< class T >
T *FindAncestorReferencingElement_R( CDmElement *target )
{
	if ( !target )
		return NULL;

	CUtlRBTree< CDmElement * > visited( 0, 0, DefLessFunc( CDmElement * ) );
	return FindAncestorReferencingElement_R_Impl< T >( visited, target );
}

// finds elements of type T that indirectly reference pElement
template< class T >
void FindAncestorsOfElement_Impl( CUtlRBTree< CDmElement * >& visited, CDmElement *pElement, CUtlVector< T* > &ancestors, bool bRecursePastFoundAncestors )
{
	// Pass one, see if it's in this ancestor list
	for ( CAttributeReferenceIterator it( pElement ); it; ++it )
	{
		if ( CDmElement *pParent = it.GetOwner() )
		{
			if ( visited.Find( pParent ) != visited.InvalidIndex() )
				continue;

			visited.Insert( pParent );

			if ( T *pT = CastElement< T >( pParent ) )
			{
				ancestors.AddToTail( pT );
				if ( !bRecursePastFoundAncestors )
					continue;
			}
			FindAncestorsOfElement_Impl( visited, pParent, ancestors, bRecursePastFoundAncestors );
		}
	}
}

// finds elements of type T that indirectly reference pElement
template< class T >
void FindAncestorsOfElement( CDmElement *pElement, CUtlVector< T* > &ancestors, bool bRecursePastFoundAncestors )
{
	if ( !pElement )
		return;

	CUtlRBTree< CDmElement * > visited( 0, 0, DefLessFunc( CDmElement * ) );
	FindAncestorsOfElement_Impl< T >( visited, pElement, ancestors, bRecursePastFoundAncestors );
}


//-----------------------------------------------------------------------------
//
// generic element tree traversal helper class
//
//-----------------------------------------------------------------------------

class CElementTreeTraversal
{
public:
	CElementTreeTraversal( CDmElement *pRoot, const char *pAttrName );

	enum { NOT_VISITED = -2, VISITING = -1 };

	void Reset( CDmElement *pRoot, const char *pAttrName );

	bool IsValid() { return m_state.Count() > 0; }
	CDmElement *Next( bool bSkipChildren = false );

	int CurrentDepth() { return m_state.Count() - 1; }
	CDmElement *GetElement();
	CDmElement *GetParent    ( int i );
	int         GetChildIndex( int i );

private:
	struct State_t
	{
		State_t( CDmElement *p, int i ) : pElement( p ), nIndex( i ) {}
		CDmElement *pElement;
		int nIndex; // -2: not yet visited, -1: visiting self, 0+: visiting children
	};

	CUtlVector< State_t > m_state;
	const char *m_pAttrName;
};


//-----------------------------------------------------------------------------
//
// element-specific unique name generation methods
//
//-----------------------------------------------------------------------------

template< class T >
struct ElementArrayNameAccessor
{
	ElementArrayNameAccessor( const CUtlVector< T > &array ) : m_array( array ) {}
	int Count() const
	{
		return m_array.Count();
	}
	const char *operator[]( int i ) const
	{
		CDmElement *pElement = GetElement< CDmElement >( m_array[ i ] );
		return pElement ? pElement->GetName() : NULL;
	}
private:
	const CUtlVector< T > &m_array;
};

template< class E >
struct ElementArrayNameAccessor< E* >
{
	ElementArrayNameAccessor( const CUtlVector< E* > &array ) : m_array( array ) {}
	int Count() const
	{
		return m_array.Count();
	}
	const char *operator[]( int i ) const
	{
		E *pElement = m_array[ i ];
		return pElement ? pElement->GetName() : NULL;
	}
private:
	const CUtlVector< E* > &m_array;
};


// returns startindex if none found, 2 if only "prefix" found, and n+1 if "prefixn" found
int GenerateUniqueNameIndex( const char *prefix, const CUtlVector< DmElementHandle_t > &array, int startindex = 0 );

bool GenerateUniqueName( char *name, int memsize, const char *prefix, const CUtlVector< DmElementHandle_t > &array );

int SplitStringIntoBaseAndIntegerSuffix( const char *pName, int len, char *pBaseName );

void MakeElementNameUnique( CDmElement *pElement, const CUtlVector< DmElementHandle_t > &array );


//-----------------------------------------------------------------------------
// helper for making attributevarelementarray cleanup easier
//-----------------------------------------------------------------------------
template< class T >
inline void CDmElement::DeleteAttributeVarElementArray( T &array )
{
	int nElements = array.Count();
	for ( int i = 0; i < nElements; ++i )
	{
		g_pDataModel->DestroyElement( array.GetHandle( i ) );
	}
	array.RemoveAll();
}


//-----------------------------------------------------------------------------
// Default size computation
//-----------------------------------------------------------------------------
template< class T >
int DmeEstimateMemorySize( T* pElement )
{
	return sizeof( T );
}

//-----------------------------------------------------------------------------
// copy groups of elements together so that references between them are maintained
//-----------------------------------------------------------------------------
template< class T >
void CopyElements( const CUtlVector< T* > &from, CUtlVector< T* > &to, TraversalDepth_t depth = TD_DEEP )
{
	CDisableUndoScopeGuard sg;

	CUtlMap< DmElementHandle_t, DmElementHandle_t, int > refmap( DefLessFunc( DmElementHandle_t ) );

	int c = from.Count();
	for ( int i = 0; i < c; ++i )
	{
		T *pCopy = NULL;

		if ( CDmElement *pFrom = from[ i ] )
		{
			int idx = refmap.Find( pFrom->GetHandle() );
			if ( idx != refmap.InvalidIndex() )
			{
				pCopy = GetElement< T >( refmap[ idx ] );
			}
			else
			{
				pCopy = GetElement< T >( g_pDataModel->CreateElement( pFrom->GetType(), pFrom->GetName(), pFrom->GetFileId() ) );
				if ( pCopy )
				{
					pFrom->CopyAttributesTo( pCopy, refmap, depth );
				}
			}
		}

		to.AddToTail( pCopy );
	}

	CUtlHashFast< DmElementHandle_t > visited;
	uint nPow2Size = 1;
	while( nPow2Size < refmap.Count() )
	{
		nPow2Size <<= 1;
	}
	visited.Init( nPow2Size );

	for ( int i = 0; i < c; ++i )
	{
		CDmElement *pTo = to[ i ];
		if ( !pTo )
			continue;

		to[ i ]->FixupReferences( visited, refmap, depth );
	}
}


//-----------------------------------------------------------------------------
// Helper macro to create an element; this is used for elements that are helper base classes 
//-----------------------------------------------------------------------------
#define DEFINE_UNINSTANCEABLE_ELEMENT( className, baseClassName )	\
	protected:														\
		className( DmElementHandle_t handle, const char *pElementTypeName, const DmObjectId_t &id, const char *pElementName, DmFileId_t fileid ) :	\
			baseClassName( handle, pElementTypeName, id, pElementName, fileid )					\
		{																						\
		}																						\
		virtual ~className()																	\
		{																						\
		}																						\
		void OnConstruction();																	\
		void OnDestruction();																	\
		virtual void PerformConstruction()														\
		{																						\
			BaseClass::PerformConstruction();													\
			OnConstruction();																	\
		}																						\
		virtual void PerformDestruction()														\
		{																						\
			OnDestruction();																	\
			BaseClass::PerformDestruction();													\
		}																						\
		virtual int AllocatedSize() const { return DmeEstimateMemorySize( this ); }				\
																								\
	private:																					\
		typedef baseClassName BaseClass; 														\


//-----------------------------------------------------------------------------
// Helper macro to create the class factory 
//-----------------------------------------------------------------------------
#define DEFINE_ELEMENT( className, baseClassName )	\
	public:											\
		virtual bool IsA( CUtlSymbolLarge typeSymbol ) const	\
		{											\
			return IsA_Implementation( typeSymbol );\
		}											\
													\
		bool IsA( const char *pTypeName ) const		\
		{											\
			CUtlSymbolLarge typeSymbol = g_pDataModel->GetSymbol( pTypeName ); \
			return IsA( typeSymbol );				\
		}											\
													\
		template< class T > bool IsA() const		\
		{											\
			return IsA( T::GetStaticTypeSymbol() ); \
		}											\
													\
		virtual int GetInheritanceDepth( CUtlSymbolLarge typeSymbol ) const	\
		{											\
			return GetInheritanceDepth_Implementation( typeSymbol, 0 );	\
		}											\
													\
		static CUtlSymbolLarge GetStaticTypeSymbol( )	\
		{											\
			return m_classType;						\
		}											\
													\
		className* Copy( TraversalDepth_t depth = TD_DEEP ) const		\
		{																\
			return static_cast< className* >( CopyInternal( depth ) );	\
		}																\
	protected:															\
		className( DmElementHandle_t handle, const char *pElementTypeName, const DmObjectId_t &id, const char *pElementName, DmFileId_t fileid ) :	\
			baseClassName( handle, pElementTypeName, id, pElementName, fileid )					\
		{																						\
		}																						\
		virtual ~className()																	\
		{																						\
		}																						\
		void OnConstruction();																	\
		void OnDestruction();																	\
		virtual void PerformConstruction()														\
		{																						\
			BaseClass::PerformConstruction();													\
			OnConstruction();																	\
		}																						\
		virtual void PerformDestruction()														\
		{																						\
			OnDestruction();																	\
			BaseClass::PerformDestruction();													\
		}																						\
		static void SetTypeSymbol( CUtlSymbolLarge typeSymbol )										\
		{																						\
			m_classType = typeSymbol;															\
		}																						\
																								\
		static bool IsA_Implementation( CUtlSymbolLarge typeSymbol )									\
		{																						\
			if ( typeSymbol == m_classType )													\
				return true;																	\
			return BaseClass::IsA_Implementation( typeSymbol );									\
		}																						\
																								\
		static int GetInheritanceDepth_Implementation( CUtlSymbolLarge typeSymbol, int nCurrentDepth )	\
		{																						\
			if ( typeSymbol == m_classType )													\
				return nCurrentDepth;															\
			return BaseClass::GetInheritanceDepth_Implementation( typeSymbol, nCurrentDepth+1 );\
		}																						\
		virtual int AllocatedSize() const { return DmeEstimateMemorySize( this ); }				\
																								\
	private:																					\
		DECLARE_FIXEDSIZE_ALLOCATOR( className );												\
		typedef baseClassName BaseClass; 														\
		typedef className ThisClass;															\
		template <class T> friend class CDmElementFactory;										\
		template <class T> friend class CDmAbstractElementFactory;								\
		static CUtlSymbolLarge m_classType

#define IMPLEMENT_ELEMENT( className ) \
	CUtlSymbolLarge className::m_classType = UTL_INVAL_SYMBOL_LARGE; \
	DEFINE_FIXEDSIZE_ALLOCATOR( className, 1024, CUtlMemoryPool::GROW_SLOW );


#endif // DMELEMENT_H

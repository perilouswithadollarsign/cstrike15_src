//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dmattributeinternal.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "dmelementdictionary.h"
#include "datamodel/idatamodel.h"
#include "datamodel.h"
#include "tier1/uniqueid.h"
#include "Color.h"
#include "mathlib/vector.h"
#include "tier1/utlstring.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlbufferutil.h"
#include "tier1/KeyValues.h"
#include "tier1/mempool.h"
#include "mathlib/vmatrix.h"
#include "datamodel/dmattributevar.h"
#include <ctype.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Tests equality
//-----------------------------------------------------------------------------
template< class T >
bool IsAttributeEqual( const T& src1, const T& src2 )
{
	return src1 == src2;
}

template< class T >
bool IsAttributeEqual( const CUtlVector<T> &src1, const CUtlVector<T> &src2 )
{
	if ( src1.Count() != src2.Count() )
		return false;
		
	for ( int i=0; i < src1.Count(); i++ )
	{
		if ( !( src1[i] == src2[i] ) )
			return false;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Typesafety check for element handles
//-----------------------------------------------------------------------------
static inline bool IsA( DmElementHandle_t hElement, CUtlSymbolLarge type )
{
	// treat NULL, deleted, and unloaded elements as being of any type - 
	// when set, undeleted or loaded, this should be checked again
	CDmElement *pElement = g_pDataModel->GetElement( hElement );
	return pElement ? pElement->IsA( type ) : true;
}


//-----------------------------------------------------------------------------
// Element attributes are never directly unserialized
//-----------------------------------------------------------------------------
static bool Serialize( CUtlBuffer &buf, DmElementHandle_t src )
{
	Assert( 0 );
	return false;
}

static bool Unserialize( CUtlBuffer &buf, DmElementHandle_t &dest )
{
	Assert( 0 );
	return false;
}

static bool Serialize( CUtlBuffer &buf, const DmUnknownAttribute_t& src )
{
	Assert( 0 );
	return false;
}

static bool Unserialize( CUtlBuffer &buf, DmUnknownAttribute_t &dest )
{
	Assert( 0 );
	return false;
}

#include "tier1/utlbufferutil.h"

//-----------------------------------------------------------------------------
// Internal interface for dealing with generic attribute operations
//-----------------------------------------------------------------------------
abstract_class IDmAttributeOp
{
public:
	virtual void* CreateAttributeData() = 0;
	virtual void DestroyAttributeData( void *pData ) = 0;
	virtual void SetDefaultValue( void *pData ) = 0;
	virtual int DataSize() = 0;
	virtual int ValueSize() = 0;
	virtual bool SerializesOnMultipleLines() = 0;
	virtual bool SkipUnserialize( CUtlBuffer& buf ) = 0;
	virtual const char *AttributeTypeName() = 0;

	virtual void SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue ) = 0;
	virtual void SetMultiple( CDmAttribute *pAttribute, int i, int nCount, DmAttributeType_t valueType, const void *pValue ) = 0;
	virtual void Set( CDmAttribute *pAttribute, int i, DmAttributeType_t valueType, const void *pValue ) = 0;
	virtual void SetToDefaultValue( CDmAttribute *pAttribute ) = 0;
	virtual bool Serialize( const CDmAttribute *pAttribute, CUtlBuffer &buf ) = 0;
	virtual bool Unserialize( CDmAttribute *pAttribute, CUtlBuffer &buf ) = 0;
	virtual bool SerializeElement( const CDmAttribute *pAttribute, int nElement, CUtlBuffer &buf ) = 0;
	virtual bool UnserializeElement( CDmAttribute *pAttribute, CUtlBuffer &buf ) = 0;
	virtual bool UnserializeElement( CDmAttribute *pAttribute, int nElement, CUtlBuffer &buf ) = 0;
	virtual void OnUnserializationFinished( CDmAttribute *pAttribute ) = 0;
	virtual bool IsIdenticalToSerializedValue( const CDmAttribute *pAttribute, CUtlBuffer &buf ) const = 0;
};


//-----------------------------------------------------------------------------
// Global table of generic attribute operations looked up by type
//-----------------------------------------------------------------------------
static IDmAttributeOp* s_pAttrInfo[ AT_TYPE_COUNT ];


//-----------------------------------------------------------------------------
//
// Implementation of IDmAttributeOp for single-valued attributes
//
//-----------------------------------------------------------------------------
template< class T >
class CDmAttributeOp : public IDmAttributeOp
{
public:
	virtual void* CreateAttributeData();
	virtual void DestroyAttributeData( void *pData );
	virtual void SetDefaultValue( void *pData );
	virtual int DataSize();
	virtual int ValueSize();
	virtual bool SerializesOnMultipleLines();
	virtual bool SkipUnserialize( CUtlBuffer& buf );
	virtual const char *AttributeTypeName();

	virtual void SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue );
	virtual void SetMultiple( CDmAttribute *pAttribute, int i, int nCount, DmAttributeType_t valueType, const void *pValue );
	virtual void Set( CDmAttribute *pAttribute, int i, DmAttributeType_t valueType, const void *pValue );
	virtual void SetToDefaultValue( CDmAttribute *pAttribute );
	virtual bool Serialize( const CDmAttribute *pData, CUtlBuffer &buf );
	virtual bool Unserialize( CDmAttribute *pAttribute, CUtlBuffer &buf );
	virtual bool SerializeElement( const CDmAttribute *pAttribute, int nElement, CUtlBuffer &buf );
	virtual bool UnserializeElement( CDmAttribute *pAttribute, CUtlBuffer &buf );
	virtual bool UnserializeElement( CDmAttribute *pAttribute, int nElement, CUtlBuffer &buf );
	virtual void OnUnserializationFinished( CDmAttribute *pAttribute );
	virtual bool IsIdenticalToSerializedValue( const CDmAttribute *pAttribute, CUtlBuffer &buf ) const;
};


//-----------------------------------------------------------------------------
// Memory pools used for CDmAttribute data
// Over 8 bytes, use the small-block allocator (it aligns to 16 bytes)
//-----------------------------------------------------------------------------
CUtlMemoryPool g_DataAlloc4( 4, 1024, CUtlMemoryPool::GROW_SLOW, "4-byte data pool" );
CUtlMemoryPool g_DataAlloc8( 8, 1024, CUtlMemoryPool::GROW_SLOW, "8-byte data pool" );
#if defined( PLATFORM_64BITS )
CUtlMemoryPool g_DataAlloc16( 16, 1024, CUtlMemoryPool::GROW_SLOW, "16-byte data pool" );
#endif

template< class T > void* NewData()
{
	return new typename CDmAttributeInfo< T >::StorageType_t;
}

template< class T > void DeleteData( void *pData )
{
	delete reinterpret_cast< typename CDmAttributeInfo< T >::StorageType_t * >( pData );
}

#define USE_SPECIAL_ALLOCATOR( _className, _allocator )		\
	template<> void* NewData< _className >()				\
	{														\
		void* pData = _allocator.Alloc( sizeof( CDmAttributeInfo< _className >::StorageType_t ) );	\
		return ::new( pData ) CDmAttributeInfo< _className >::StorageType_t(); \
	}														\
	template<> void DeleteData<_className>( void *pData )	\
	{														\
		typedef CDmAttributeInfo< _className >::StorageType_t D;	\
		( ( D * )pData )->~D();								\
		_allocator.Free( pData );							\
	}

// make sure that the attribute data type sizes are what we think they are to choose the right allocator
struct CSizeTest
{
	CSizeTest()
	{
		// test internal value attribute sizes
		COMPILE_TIME_ASSERT( sizeof( int )			== 4 );
		COMPILE_TIME_ASSERT( sizeof( float )		== 4 );
		COMPILE_TIME_ASSERT( sizeof( bool )			<= 4 );
		COMPILE_TIME_ASSERT( sizeof( Color )		== 4 );
#if defined( PLATFORM_64BITS )
		COMPILE_TIME_ASSERT( sizeof( DmElementAttribute_t ) <= 16 );
#else
		COMPILE_TIME_ASSERT( sizeof( DmElementAttribute_t ) <= 8 );
#endif
		COMPILE_TIME_ASSERT( sizeof( Vector2D )		== 8 );
	}
};
static CSizeTest g_sizeTest;

// turn memdbg off temporarily so we can get at placement new
#include "tier0/memdbgoff.h"

USE_SPECIAL_ALLOCATOR( bool, g_DataAlloc4 )
USE_SPECIAL_ALLOCATOR( int, g_DataAlloc4 )
USE_SPECIAL_ALLOCATOR( float, g_DataAlloc4 )
#if defined( PLATFORM_64BITS )
USE_SPECIAL_ALLOCATOR( DmElementHandle_t, g_DataAlloc16 )
#else
USE_SPECIAL_ALLOCATOR( DmElementHandle_t, g_DataAlloc8 )
#endif
USE_SPECIAL_ALLOCATOR( Color, g_DataAlloc4 )
USE_SPECIAL_ALLOCATOR( DmeTime_t, g_DataAlloc4 )
USE_SPECIAL_ALLOCATOR( Vector2D, g_DataAlloc8 )

#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Create, destroy attribute data
//-----------------------------------------------------------------------------
template< class T >
void* CDmAttributeOp<T>::CreateAttributeData()
{
	void *pData = NewData< T >();
	CDmAttributeInfo< T >::SetDefaultValue( *reinterpret_cast<T*>( pData ) );
	return pData;
}

template<> void* CDmAttributeOp< DmUnknownAttribute_t >::CreateAttributeData()
{
	// Fail if someone tries to create an AT_UNKNOWN attribute
	Assert(0);
	return NULL;
}

template< class T >
void CDmAttributeOp<T>::DestroyAttributeData( void *pData )
{
	DeleteData< T >( pData );
}


//-----------------------------------------------------------------------------
// Sets the data to a default value, no undo (used for construction)
//-----------------------------------------------------------------------------
template< class T >
void CDmAttributeOp<T>::SetDefaultValue( void *pData )
{
	CDmAttributeInfo< T >::SetDefaultValue( *reinterpret_cast<T*>( pData ) );
}


//-----------------------------------------------------------------------------
// Attribute type name, data size, value size
//-----------------------------------------------------------------------------
template< class T >
const char *CDmAttributeOp<T>::AttributeTypeName()
{
	return CDmAttributeInfo<T>::AttributeTypeName();
}

template< class T >
int CDmAttributeOp<T>::DataSize()
{
	return sizeof( typename CDmAttributeInfo< T >::StorageType_t );
}

template< class T >
int CDmAttributeOp<T>::ValueSize()
{
	return sizeof( T );
}


//-----------------------------------------------------------------------------
// Value-setting methods
//-----------------------------------------------------------------------------
template< class T >
void CDmAttributeOp<T>::SetToDefaultValue( CDmAttribute *pAttribute )
{
	T newValue;
	CDmAttributeInfo< T >::SetDefaultValue( newValue );
	pAttribute->SetValue( newValue );
}

template< class T >
void CDmAttributeOp<T>::SetMultiple( CDmAttribute *pAttribute, int i, int nCount, DmAttributeType_t valueType, const void *pValue )
{
	Assert(0);
}

template< class T >
void CDmAttributeOp<T>::Set( CDmAttribute *pAttribute, int i, DmAttributeType_t valueType, const void *pValue )
{
	Assert(0);
}

template< class T >
void CDmAttributeOp<T>::SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue )
{
	Assert( pAttribute->GetType() == valueType );
	if ( pAttribute->GetType() == valueType )
	{
		pAttribute->SetValue( *reinterpret_cast< const T* >( pValue ) );
	}
}

#define SET_VALUE_TYPE( _srcType )												\
	case CDmAttributeInfo< _srcType >::ATTRIBUTE_TYPE:							\
	pAttribute->SetValue( *reinterpret_cast< const _srcType* >( pValue ) );	\
	break;

template<>
void CDmAttributeOp<int>::SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue )
{
	switch( valueType )
	{
	SET_VALUE_TYPE( int );
	SET_VALUE_TYPE( float );
	SET_VALUE_TYPE( bool );
	SET_VALUE_TYPE( DmeTime_t );

	default:
		Assert(0);
		break;
	}
}

template<>
void CDmAttributeOp<float>::SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue )
{
	switch( valueType )
	{
	SET_VALUE_TYPE( int );
	SET_VALUE_TYPE( float );
	SET_VALUE_TYPE( bool );
	SET_VALUE_TYPE( DmeTime_t );

	default:
		Assert(0);
		break;
	}
}

template<>
void CDmAttributeOp<bool>::SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue )
{
	switch( valueType )
	{
	SET_VALUE_TYPE( int );
	SET_VALUE_TYPE( float );
	SET_VALUE_TYPE( bool );
	SET_VALUE_TYPE( DmeTime_t );

	default:
		Assert(0);
		break;			 			  
	}
}

template<>
void CDmAttributeOp<DmeTime_t>::SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue )
{
	switch( valueType )
	{
		SET_VALUE_TYPE( int );
		SET_VALUE_TYPE( float );
		SET_VALUE_TYPE( bool );
		SET_VALUE_TYPE( DmeTime_t );

	default:
		Assert(0);
		break;			 			  
	}
}


template<>
void CDmAttributeOp<QAngle>::SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue )
{
	switch( valueType )
	{
		SET_VALUE_TYPE( QAngle );
		SET_VALUE_TYPE( Quaternion );

	default:
		Assert(0);
		break;
	}
}

template<>
void CDmAttributeOp<Quaternion>::SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue )
{
	switch( valueType )
	{
		SET_VALUE_TYPE( QAngle );
		SET_VALUE_TYPE( Quaternion );

	default:
		Assert(0);
		break;
	}
}

//-----------------------------------------------------------------------------
// CUtlSymbolLarge specialization of utlvector unserialize function defined by
// utlBufferUtil, allows the unserialize to correctly add the string to the 
// global symbol table.
//-----------------------------------------------------------------------------
template<>
bool Unserialize( CUtlBuffer &buf, CUtlVector< CUtlSymbolLarge > &dest )
{
	dest.RemoveAll();

	MEM_ALLOC_CREDIT_FUNCTION();

	CUtlString tempString;

	if ( !buf.IsText() )
	{
		int nCount = buf.GetInt();
		if ( nCount )
		{
			dest.EnsureCapacity( nCount );
			for ( int i = 0; i < nCount; ++i )
			{
				if ( !::Unserialize( buf, tempString ) )
					return false;

				dest.AddToTail( g_pDataModel->GetSymbol( tempString.Get() ) );
			}
		}
		return buf.IsValid();
	}

	while ( true )
	{
		buf.EatWhiteSpace();
		if ( !buf.IsValid() )
			break;

		if ( ! ::Unserialize( buf, tempString ) )
			return false;

		dest.AddToTail( g_pDataModel->GetSymbol( tempString.Get() ) );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Methods related to serialization
//-----------------------------------------------------------------------------
template< class T >
bool CDmAttributeOp<T>::SerializesOnMultipleLines()
{
	return ::SerializesOnMultipleLines< T >();
}

template< class T >
bool CDmAttributeOp<T>::SkipUnserialize( CUtlBuffer& buf )
{
	T dummy;
	return ::Unserialize( buf, dummy );
}

template<>
bool CDmAttributeOp< CUtlSymbolLarge >::SkipUnserialize( CUtlBuffer& buf )
{
	CUtlString dummy;
	return ::Unserialize( buf, dummy );
}

template< class T >
bool CDmAttributeOp<T>::Serialize( const CDmAttribute *pAttribute, CUtlBuffer &buf )
{
	// NOTE: For this to work, the class must have a function defined of type
	// bool Serialize( CUtlBuffer &buf, T &src )
	return ::Serialize( buf, pAttribute->GetValue<T>() );
}

template< class T >
bool CDmAttributeOp<T>::Unserialize( CDmAttribute *pAttribute, CUtlBuffer &buf )
{
	// NOTE: For this to work, the class must have a function defined of type
	// bool Unserialize( CUtlBuffer &buf, T &src )

	T tempVal;
	if ( !::Unserialize( buf, tempVal ) )
		return false;

	// Don't need undo hook since this goes through SetValue route
	pAttribute->SetValue( tempVal );

	return true;
}

template<>
bool CDmAttributeOp< CUtlSymbolLarge >::Unserialize( CDmAttribute *pAttribute, CUtlBuffer &buf )
{
	CUtlString tempString;
	if ( !::Unserialize( buf, tempString ) )
		return false;

	pAttribute->SetValue( tempString.Get() );
	
	return true;
}


// Helper function to compare two DM attribute values for equality
// Needs to be done in a separate function because we can't do templated template specialization
// Otherwise we'd do: template< class T > bool CDmAttributeOp< CUtlVector<T> >
// So instead we use function overload resolution to do the dirty work for us...
template< class T > bool CompareAttrValuesHelper( const T& A, const T& B )
{
	return ( A == B );
}

template< class T > bool CompareAttrValuesHelper( const CUtlVector<T>& A, const CUtlVector<T>& B )
{
	if ( A.Count() != B.Count() )
		return false;

	for ( int i = 0; i < A.Count(); ++i )
	{
		if ( !CompareAttrValuesHelper( A[i], B[i] ) )
		{
			return false;
		}
	}

	return true;
}

template< class T >
bool CDmAttributeOp<T>::IsIdenticalToSerializedValue( const CDmAttribute *pAttribute, CUtlBuffer &buf ) const
{
	T tempVal;
	if ( !::Unserialize( buf, tempVal ) )
		return false;

	return CompareAttrValuesHelper( tempVal, pAttribute->GetValue<T>() );
}

template <>
bool CDmAttributeOp< CUtlSymbolLarge >::IsIdenticalToSerializedValue( const CDmAttribute *pAttribute, CUtlBuffer &buf ) const
{
	CUtlString tempVal;
	if ( !::Unserialize( buf, tempVal ) )
		return false;

	if ( V_stricmp( pAttribute->GetValue< CUtlSymbolLarge >().String(), tempVal.Get() ) != 0 )
		return false;

	return true;
}

template< class T >
bool CDmAttributeOp<T>::SerializeElement( const CDmAttribute *pData, int nElement, CUtlBuffer &buf )
{
	Assert( 0 );
	return false;
}

template< class T >
bool CDmAttributeOp<T>::UnserializeElement( CDmAttribute *pData, CUtlBuffer &buf )
{
	Assert( 0 );
	return false;
}

template< class T >
bool CDmAttributeOp<T>::UnserializeElement( CDmAttribute *pData, int nElement, CUtlBuffer &buf )
{
	Assert( 0 );
	return false;
}

template< class T >
void CDmAttributeOp<T>::OnUnserializationFinished( CDmAttribute *pAttribute )
{
	CDmAttributeAccessor::OnChanged( pAttribute, false, true );
}



//-----------------------------------------------------------------------------
//
// Implementation of IDmAttributeOp for array attributes
//
//-----------------------------------------------------------------------------
template< class T >
class CDmArrayAttributeOp : public CDmAttributeOp< CUtlVector< T > >
{
	typedef typename CDmAttributeInfo< CUtlVector< T > >::StorageType_t D;

public:
	// Inherited from IDmAttributeOp
	virtual void SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue );
	virtual void SetMultiple( CDmAttribute *pAttribute, int i, int nCount, DmAttributeType_t valueType, const void *pValue );
	virtual void Set( CDmAttribute *pAttribute, int i, DmAttributeType_t valueType, const void *pValue );
	virtual bool Unserialize( CDmAttribute *pAttribute, CUtlBuffer &buf );
	virtual bool SerializeElement( const CDmAttribute *pData, int nElement, CUtlBuffer &buf );
	virtual bool UnserializeElement( CDmAttribute *pData, CUtlBuffer &buf );
	virtual bool UnserializeElement( CDmAttribute *pData, int nElement, CUtlBuffer &buf );
	virtual void OnUnserializationFinished( CDmAttribute *pAttribute );

	// Other methods used by CDmaArrayBase
	CDmArrayAttributeOp() : m_pAttribute( NULL ), m_pData( NULL ) {}
	CDmArrayAttributeOp( CDmAttribute *pAttribute ) : m_pAttribute( pAttribute ), m_pData( (D*)m_pAttribute->GetAttributeData() ) {}

	// Count
	int		Count() const;

	// Insertion
	int		AddToTail( const T& src );
	int		InsertBefore( int elem, const T& src );
	int		InsertMultipleBefore( int elem, int num );

	// Removal
	void	FastRemove( int elem );
	void	Remove( int elem );
	void	RemoveAll();
	void	RemoveMultiple( int elem, int num );
	void	Purge();

	// Element Modification
	void	Set( int i, const T& value );
	void	SetMultiple( int i, int nCount, const T* pValue );
	void	Swap( int i, int j );

	// Copy related methods
	void	CopyArray( const T *pArray, int size );
	void	SwapArray( CUtlVector< T >& src );	// Performs a pointer swap

	void	OnAttributeArrayElementAdded( int nFirstElem, int nLastElem, bool bUpdateElementReferences = true );
	void	OnAttributeArrayElementRemoved( int nFirstElem, int nLastElem );

private:
	bool	ShouldInsertElement( const T& src );
	bool	ShouldInsert( const T& src );
	void	PerformCopyArray( const T *pArray, int nCount );
	D& Data() { return *m_pData; }
	const D& Data() const { return *m_pData; }

	CDmAttribute *m_pAttribute;
	D* m_pData;
};


//-----------------------------------------------------------------------------
//
// Undo-related classes
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Undo attribute name change
//-----------------------------------------------------------------------------
class CUndoAttributeRenameElement : public CUndoElement
{
	typedef CUndoElement BaseClass;

public:
	CUndoAttributeRenameElement( CDmAttribute *pAttribute, const char *newName )
		: BaseClass( "CUndoAttributeRenameElement" )
	{
		Assert( pAttribute->GetOwner() && pAttribute->GetOwner()->GetFileId() != DMFILEID_INVALID );
		m_hOwner = pAttribute->GetOwner()->GetHandle();
		m_symAttributeOld = pAttribute->GetNameSymbol();
		m_symAttributeNew = g_pDataModel->GetSymbol( newName );
	}

	virtual void Undo()
	{
		CDmElement *pOwner = GetOwner();
		if ( pOwner )
		{
			pOwner->RenameAttribute( m_symAttributeNew.String(), m_symAttributeOld.String() );
		}
	}

	virtual void Redo()
	{
		CDmElement *pOwner = GetOwner();
		if ( pOwner )
		{
			pOwner->RenameAttribute( m_symAttributeOld.String(), m_symAttributeNew.String() );
		}
	}

	virtual const char	*GetDesc()
	{
		static char buf[ 128 ];

		const char *base = BaseClass::GetDesc();
		Q_snprintf( buf, sizeof( buf ), "%s (%s -> %s)", base, m_symAttributeOld.String(), m_symAttributeNew.String() );
		return buf;
	}

private:
	CDmElement *GetOwner()
	{
		return g_pDataModel->GetElement( m_hOwner );
	}

	CUtlSymbolLarge				m_symAttributeOld;
	CUtlSymbolLarge				m_symAttributeNew;
	DmElementHandle_t		m_hOwner;
};

//-----------------------------------------------------------------------------
// Undo single-valued attribute value changed
//-----------------------------------------------------------------------------
template< class T >
class CUndoAttributeSetValueElement : public CUndoElement
{
	typedef CUndoElement BaseClass;
public:
	CUndoAttributeSetValueElement( CDmAttribute *pAttribute, const T &newValue )
		: BaseClass( "CUndoAttributeSetValueElement" )
	{
		Assert( pAttribute->GetOwner() && pAttribute->GetOwner()->GetFileId() != DMFILEID_INVALID );
		m_hOwner = pAttribute->GetOwner()->GetHandle();
		m_OldValue = pAttribute->GetValue<T>();
		m_Value = newValue;
		m_symAttribute = pAttribute->GetNameSymbol( );
	}

	CUndoAttributeSetValueElement( CDmAttribute *pAttribute )
		: BaseClass( "CUndoAttributeSetValueElement" )
	{
		Assert( pAttribute->GetOwner() && pAttribute->GetOwner()->GetFileId() != DMFILEID_INVALID );
		m_hOwner = pAttribute->GetOwner()->GetHandle();
		m_OldValue = pAttribute->GetValue<T>();
		m_symAttribute = pAttribute->GetNameSymbol( );
	}

	void SetEndValue( CDmAttribute *pAttribute )
	{
		m_Value = pAttribute->GetValue<T>();
	}

	CDmElement *GetOwner()
	{
		return g_pDataModel->GetElement( m_hOwner );
	}

	virtual void Undo()
	{
		CDmAttribute *pAttribute = GetAttribute();
		if ( pAttribute && !pAttribute->IsFlagSet( FATTRIB_READONLY ) )
		{
			pAttribute->SetValue<T>( m_OldValue );
		}
	}
	virtual void Redo()
	{
		CDmAttribute *pAttribute = GetAttribute();
		if ( pAttribute && !pAttribute->IsFlagSet( FATTRIB_READONLY ) )
		{
			pAttribute->SetValue<T>( m_Value );
		}
	}

	virtual const char	*GetDesc()
	{
		static char buf[ 128 ];

		const char *base = BaseClass::GetDesc();
		CDmAttribute *pAtt = GetAttribute();
		CUtlBuffer serialized( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( pAtt && pAtt->GetType() != AT_ELEMENT )
		{
			::Serialize( serialized, m_Value );
		}
		V_sprintf_safe( buf, "%s(%s) = %s", base, m_symAttribute.String(), serialized.Base() ? (const char*)serialized.Base() : "\"\"" );
		return buf;
	}

private:
	CDmAttribute *GetAttribute()
	{
		CDmElement *pOwner = GetOwner();
		if ( pOwner )
		{
			const char *pAttributeName = m_symAttribute.String();
			return pOwner->GetAttribute( pAttributeName );
		}
		return NULL;
	}

	typedef typename CDmAttributeUndoStorageType< T >::UndoStorageType StorageType_t;

	CUtlSymbolLarge			m_symAttribute;
	DmElementHandle_t	m_hOwner;
	StorageType_t		m_OldValue;
	StorageType_t		m_Value;
};



//-----------------------------------------------------------------------------
// Base undo for array attributes
//-----------------------------------------------------------------------------
template< class T >
class CUndoAttributeArrayBase : public CUndoElement
{
	typedef CUndoElement BaseClass;

public:
	CUndoAttributeArrayBase( CDmAttribute *pAttribute, const char *pUndoName ) : BaseClass( pUndoName )
	{
		m_hOwner = pAttribute->GetOwner()->GetHandle();
		m_symAttribute = pAttribute->GetNameSymbol( );
	}

protected:
	typedef typename CDmAttributeUndoStorageType< T >::UndoStorageType StorageType_t;

	CDmElement *GetOwner()
	{
		return g_pDataModel->GetElement( m_hOwner );
	}

	const char *GetAttributeName()
	{
		return m_symAttribute.String();
	}

	CDmAttribute *GetAttribute()
	{
		const char *pAttributeName = GetAttributeName();
		CDmElement *pOwner = GetOwner();
		if ( pOwner )
			return pOwner->GetAttribute( pAttributeName );
		Assert( 0 );
		return NULL;
	}

private:
	CUtlSymbolLarge				m_symAttribute;
	DmElementHandle_t		m_hOwner;
};


//-----------------------------------------------------------------------------
// Undo for setting a single element
//-----------------------------------------------------------------------------
template< class T >
class CUndoArrayAttributeSetValueElement : public CUndoAttributeArrayBase<T>
{
	typedef CUndoAttributeArrayBase<T> BaseClass;

public:
	CUndoArrayAttributeSetValueElement( CDmAttribute *pAttribute, int slot, const T &newValue ) : 
		BaseClass( pAttribute, "CUndoArrayAttributeSetValueElement" ),
		m_nSlot( slot )
	{
		Assert( pAttribute->GetOwner() && pAttribute->GetOwner()->GetFileId() != DMFILEID_INVALID );

		CDmrArray<T> array( pAttribute );
		m_OldValue = array[ slot ];
		m_Value = newValue;
	}

	virtual void Undo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			array.Set( m_nSlot, m_OldValue );
		}
	}

	virtual void Redo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			array.Set( m_nSlot, m_Value );
		}
	}

private:
	int				m_nSlot;
	typename CUndoAttributeArrayBase<T>::StorageType_t	m_OldValue;
	typename CUndoAttributeArrayBase<T>::StorageType_t	m_Value;
};


//-----------------------------------------------------------------------------
// Undo for setting a multiple elements
//-----------------------------------------------------------------------------
template< class T >
class CUndoArrayAttributeSetMultipleValueElement : public CUndoAttributeArrayBase<T>
{
	typedef CUndoAttributeArrayBase<T> BaseClass;

public:
	CUndoArrayAttributeSetMultipleValueElement( CDmAttribute *pAttribute, int nSlot, int nCount, const T *pNewValue ) : 
		BaseClass( pAttribute, "CUndoArrayAttributeSetMultipleValueElement" ), 
		m_nSlot( nSlot ), m_nCount( nCount )
	{
		Assert( pAttribute->GetOwner() && pAttribute->GetOwner()->GetFileId() != DMFILEID_INVALID );
		m_pOldValue = new typename CUndoAttributeArrayBase<T>::StorageType_t[nCount];
		m_pValue    = new typename CUndoAttributeArrayBase<T>::StorageType_t[nCount];

		CDmrArray<T> array( pAttribute );
		for ( int i = 0; i < nCount; ++i )
		{
			m_pOldValue[i] = array[ nSlot+i ];
			m_pValue[i] = pNewValue[ i ];
		}
	}

	~CUndoArrayAttributeSetMultipleValueElement()
	{
		// this is a hack necessitated by MSVC's lack of partially specialized member template support
		// (ie otherwise I'd just create a CUndoArrayAttributeSetMultipleValueElement< DmElementHandle_t,BaseClass> version with this code)
		// anyways, the casting hackiness only happens when the value is actually a DmElementHandle_t, so it's completely safe
		if ( CDmAttributeInfo< T >::AttributeType() == AT_ELEMENT )
		{
			DmElementHandle_t value = DMELEMENT_HANDLE_INVALID;
			for ( int i = 0; i < m_nCount; ++i )
			{
				m_pOldValue[ i ] = m_pValue[ i ] = *( T* )&value;
			}
		}

		delete[] m_pOldValue;
		delete[] m_pValue;
	}

	virtual void Undo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			for ( int i = 0; i < m_nCount; ++i )
			{
				array.Set( m_nSlot+i, m_pOldValue[i] );
			}
		}
	}

	virtual void Redo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			for ( int i = 0; i < m_nCount; ++i )
			{
				array.Set( m_nSlot+i, m_pValue[i] );
			}
		}
	}

private:
	int				m_nSlot;
	int				m_nCount;
	typename CUndoAttributeArrayBase<T>::StorageType_t	*m_pOldValue;
	typename CUndoAttributeArrayBase<T>::StorageType_t	*m_pValue;
};


//-----------------------------------------------------------------------------
//
// Implementation Undo for CDmAttributeTyped
//
//-----------------------------------------------------------------------------
template< class T >
class CUndoAttributeArrayInsertBefore : public CUndoAttributeArrayBase<T>
{
	typedef CUndoAttributeArrayBase<T> BaseClass;
public:
	CUndoAttributeArrayInsertBefore( CDmAttribute *pAttribute, int slot, int count = 1 ) : 
		BaseClass( pAttribute, "CUndoAttributeArrayInsertBefore" ),
		m_nIndex( slot ), m_nCount( count )
	{
		Assert( pAttribute->GetOwner() && pAttribute->GetOwner()->GetFileId() != DMFILEID_INVALID );
	}

	virtual void Undo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			array.RemoveMultiple( m_nIndex, m_nCount );
		}
	}

	virtual void Redo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			T defaultVal;
			CDmAttributeInfo<T>::SetDefaultValue( defaultVal );

			array.InsertMultipleBefore( m_nIndex, m_nCount );
			for( int i = 0; i < m_nCount; ++i )
			{
				array.Set( m_nIndex + i, defaultVal );
			}
		}
	}

private:
	int	m_nIndex;
	int m_nCount;
};


//-----------------------------------------------------------------------------
//
// Implementation Undo for inserting a copy
//
//-----------------------------------------------------------------------------
template< class T >
class CUndoAttributeArrayInsertCopyBefore : public CUndoAttributeArrayBase<T>
{
	typedef CUndoAttributeArrayBase<T> BaseClass;

public:
	CUndoAttributeArrayInsertCopyBefore( CDmAttribute *pAttribute, int slot, const T& newValue ) : 
		BaseClass( pAttribute, "CUndoAttributeArrayInsertCopyBefore" ),
		m_nIndex( slot ),
		m_newValue( newValue )
	{
		Assert( pAttribute->GetOwner() && pAttribute->GetOwner()->GetFileId() != DMFILEID_INVALID );
	}

	virtual void Undo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			array.Remove( m_nIndex );
		}
	}

	virtual void Redo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			array.InsertBefore( m_nIndex, m_newValue );
		}
	}

private:
	int				m_nIndex;
	typename CUndoAttributeArrayBase<T>::StorageType_t	m_newValue;
};


//-----------------------------------------------------------------------------
//
// Implementation Undo for remove
//
//-----------------------------------------------------------------------------
template< class T >
class CUndoAttributeArrayRemoveElement : public CUndoAttributeArrayBase<T>
{
	typedef CUndoAttributeArrayBase<T> BaseClass;

public:
	CUndoAttributeArrayRemoveElement( CDmAttribute *pAttribute, bool fastRemove, int elem, int count ) : 
		BaseClass( pAttribute, "CUndoAttributeArrayRemoveElement" ),
		m_bFastRemove( fastRemove ), m_nIndex( elem ), m_nCount( count )
	{
		Assert( pAttribute->GetOwner() && pAttribute->GetOwner()->GetFileId() != DMFILEID_INVALID );
		Assert( m_nCount >= 1 );
		// If it's fastremove, count must == 1
		Assert( !m_bFastRemove || m_nCount == 1 );
		CDmrArray< T > array( pAttribute );
		Assert( array.IsValid() );
		for ( int i = 0 ; i < m_nCount; ++i )
		{
			m_OldValues.AddToTail( array[ elem + i ] );
		}
	}

	~CUndoAttributeArrayRemoveElement()
	{
		// this is a hack necessitated by MSVC's lack of partially specialized member template support
		// (ie otherwise I'd just create a CUndoArrayAttributeSetMultipleValueElement< DmElementHandle_t,BaseClass> version with this code)
		// anyways, the casting hackiness only happens when the value is actually a DmElementHandle_t, so it's completely safe
		if ( CDmAttributeInfo< T >::AttributeType() == AT_ELEMENT )
		{
			DmElementHandle_t value = DMELEMENT_HANDLE_INVALID;
			for ( int i = 0; i < m_nCount; ++i )
			{
				m_OldValues[ i ] = *( T* )&value;
			}
			m_OldValues.RemoveAll();
		}
	}

	virtual void Undo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			if ( m_bFastRemove )
			{
				Assert( m_nCount == 1 );
				Assert( m_OldValues.Count() == 1 );

				if ( array.Count() > m_nIndex )
				{
					// Get value at previous index (it was moved down from the "end" before
					T m_EndValue = array.Get( m_nIndex );

					// Restore previous value
					array.Set( m_nIndex, m_OldValues[ 0 ] );

					// Put old value back to end of array
					array.AddToTail( m_EndValue );
				}
				else
				{
					Assert( array.Count() == m_nIndex );
					array.AddToTail( m_OldValues[ 0 ] );
				}
			}
			else
			{
				int insertPos = m_nIndex;
				for ( int i = 0; i < m_nCount; ++i )
				{
					array.InsertBefore( insertPos++, m_OldValues[ i ] );
				}
			}
		}
	}

	virtual void Redo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			if ( m_bFastRemove )
			{
				Assert( m_nCount == 1 );
				Assert( m_OldValues.Count() == 1 );

				array.FastRemove( m_nIndex );
			}
			else
			{
				array.RemoveMultiple( m_nIndex, m_nCount );
			}
		}
	}

	virtual const char *GetDesc()
	{
		static char buf[ 128 ];

		const char *base = BaseClass::GetDesc();
		Q_snprintf( buf, sizeof( buf ), "%s (%s) = remove( pos %i, count %i )", base, GetAttributeName(), m_nIndex, m_nCount );
		return buf;
	}

private:	
	bool						m_bFastRemove;
	int							m_nIndex;
	int							m_nCount;
	CUtlVector< typename CUndoAttributeArrayBase<T>::StorageType_t >	m_OldValues;
};


template< class T >
class CUndoAttributeArrayCopyAllElement : public CUndoAttributeArrayBase<T>
{
	typedef CUndoAttributeArrayBase<T> BaseClass;
public:
	CUndoAttributeArrayCopyAllElement( CDmAttribute *pAttribute, const T *pNewValues, int nNewSize, bool purgeOnRemove = false )
		: BaseClass( pAttribute, "CUndoAttributeArrayCopyAllElement" ),
		m_bPurge( purgeOnRemove )
	{
		Assert( pAttribute->GetOwner() && pAttribute->GetOwner()->GetFileId() != DMFILEID_INVALID );
		CDmrArray< T > att( pAttribute );
		Assert( att.IsValid() );

		if ( pNewValues != NULL && nNewSize > 0 )
		{
			m_pNewValues = new typename CUndoAttributeArrayBase<T>::StorageType_t[ nNewSize ];
			for ( int i = 0; i < nNewSize; ++i )
			{
				m_pNewValues[ i ] = pNewValues[ i ];
			}
			m_nNewSize = nNewSize;
		}
		else
		{
			m_pNewValues = NULL;
			m_nNewSize = 0;
		}

		int nOldSize = att.Count();
		const T *pOldValues = att.Base();
		if ( pOldValues != NULL && nOldSize > 0 )
		{
			m_pOldValues = new typename CUndoAttributeArrayBase<T>::StorageType_t[ nOldSize ];
			for ( int i = 0; i < nOldSize; ++i )
			{
				m_pOldValues[ i ] = pOldValues[ i ];
			}
			m_nOldSize = nOldSize;
		}
		else
		{
			m_pOldValues = NULL;
			m_nOldSize = 0;
		}
	}

	~CUndoAttributeArrayCopyAllElement()
	{
		// this is a hack necessitated by MSVC's lack of partially specialized member template support
		// (ie otherwise I'd just create a CUndoArrayAttributeSetMultipleValueElement< DmElementHandle_t,BaseClass> version with this code)
		// anyways, the casting hackiness only happens when the value is actually a DmElementHandle_t, so it's completely safe
		if ( CDmAttributeInfo< T >::AttributeType() == AT_ELEMENT )
		{
			DmElementHandle_t value = DMELEMENT_HANDLE_INVALID;
			for ( int i = 0; i < m_nOldSize; ++i )
			{
				m_pOldValues[ i ] = *( T* )&value;
			}
			for ( int i = 0; i < m_nNewSize; ++i )
			{
				m_pNewValues[ i ] = *( T* )&value;
			}
		}

		delete[] m_pOldValues;
		delete[] m_pNewValues;
	}

	virtual void Undo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			array.RemoveAll();
			for ( int i = 0; i < m_nOldSize; ++i )
			{
				array.AddToTail( m_pOldValues[ i ] );
			}
		}
	}

	virtual void Redo()
	{
		CDmrArray<T> array( GetAttribute() );
		if ( array.IsValid() )
		{
			array.RemoveAll();
			for ( int i = 0; i < m_nNewSize; ++i )
			{
				array.AddToTail( m_pNewValues[ i ] );
			}

			if ( m_bPurge )
			{
				Assert( array.Count() == 0 );
				array.Purge();
			}
		}
	}

private:
	typename CUndoAttributeArrayBase<T>::StorageType_t		*m_pOldValues;
	int					m_nOldSize;
	typename CUndoAttributeArrayBase<T>::StorageType_t		*m_pNewValues;
	int					m_nNewSize;
	bool				m_bPurge;
};



//-----------------------------------------------------------------------------
// CDmArrayAttributeOp implementation.
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Callbacks when elements are added + removed
//-----------------------------------------------------------------------------
template< class T >
void CDmArrayAttributeOp<T>::OnAttributeArrayElementAdded( int nFirstElem, int nLastElem, bool bUpdateElementReferences )
{
	CDmElement *pOwner = m_pAttribute->GetOwner();
	if ( m_pAttribute->IsFlagSet( FATTRIB_HAS_CALLBACK ) && CDmeElementAccessor::AreOnChangedCallbacksEnabled( pOwner ) )
	{
		pOwner->OnAttributeArrayElementAdded( m_pAttribute, nFirstElem, nLastElem );
	}
}

template< > inline void CDmArrayAttributeOp< DmElementHandle_t >::OnAttributeArrayElementAdded( int nFirstElem, int nLastElem, bool bUpdateElementReferences )
{
	CDmElement *pOwner = m_pAttribute->GetOwner();
	if ( m_pAttribute->IsFlagSet( FATTRIB_HAS_CALLBACK ) && CDmeElementAccessor::AreOnChangedCallbacksEnabled( pOwner ) )
	{
		pOwner->OnAttributeArrayElementAdded( m_pAttribute, nFirstElem, nLastElem );
	}

	if ( bUpdateElementReferences )
	{
		for ( int i = nFirstElem; i <= nLastElem; ++i )
		{
			g_pDataModelImp->OnElementReferenceAdded( Data()[ i ], m_pAttribute );
		}
	}
}

template< class T >
void CDmArrayAttributeOp<T>::OnAttributeArrayElementRemoved( int nFirstElem, int nLastElem )
{
	CDmElement *pOwner = m_pAttribute->GetOwner();
	if ( m_pAttribute->IsFlagSet( FATTRIB_HAS_CALLBACK ) && CDmeElementAccessor::AreOnChangedCallbacksEnabled( pOwner ) )
	{
		pOwner->OnAttributeArrayElementRemoved( m_pAttribute, nFirstElem, nLastElem );
	}
}

template< > void CDmArrayAttributeOp< DmElementHandle_t >::OnAttributeArrayElementRemoved( int nFirstElem, int nLastElem )
{
	CDmElement *pOwner = m_pAttribute->GetOwner();
	if ( m_pAttribute->IsFlagSet( FATTRIB_HAS_CALLBACK ) && CDmeElementAccessor::AreOnChangedCallbacksEnabled( pOwner ) )
	{
		pOwner->OnAttributeArrayElementRemoved( m_pAttribute, nFirstElem, nLastElem );
	}

	for ( int i = nFirstElem; i <= nLastElem; ++i )
	{
		g_pDataModelImp->OnElementReferenceRemoved( Data()[ i ], m_pAttribute );
	}
}


//-----------------------------------------------------------------------------
// Count
//-----------------------------------------------------------------------------
template< class T >
int CDmArrayAttributeOp<T>::Count() const
{
	return Data().Count();
}


//-----------------------------------------------------------------------------
// Should we insert this element into the list?
//-----------------------------------------------------------------------------
template< class T >
inline bool CDmArrayAttributeOp<T>::ShouldInsertElement( const T& src )
{
	return true;
}

template<> inline bool CDmArrayAttributeOp<DmElementHandle_t>::ShouldInsertElement( const DmElementHandle_t& src )
{
	// For element, we need to check that the type matches
	if ( !IsA( src, Data().m_ElementType ) )
		return false;

	return true;
}

template< class T >
inline bool CDmArrayAttributeOp<T>::ShouldInsert( const T& src )
{
	if ( !ShouldInsertElement( src ) )
		return false;

	return m_pAttribute->MarkDirty();
}


//-----------------------------------------------------------------------------
// Insert Before
//-----------------------------------------------------------------------------
template< class T >
int CDmArrayAttributeOp<T>::InsertBefore( int elem, const T& src )
{
	if ( !ShouldInsert( src ) )
		return Data().InvalidIndex();

	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoAttributeArrayInsertCopyBefore<T> *pUndo = new CUndoAttributeArrayInsertCopyBefore<T>( m_pAttribute, elem, src );
		g_pDataModel->AddUndoElement( pUndo );
	}

	int nIndex = Data().InsertBefore( elem, src );
	OnAttributeArrayElementAdded( nIndex, nIndex );
	m_pAttribute->OnChanged( true );
	return nIndex;
}

template< class T >
inline int CDmArrayAttributeOp<T>::AddToTail( const T& src )
{
	return InsertBefore( Data().Count(), src );
}


//-----------------------------------------------------------------------------
// Insert Multiple Before
//-----------------------------------------------------------------------------
template< class T >
int CDmArrayAttributeOp<T>::InsertMultipleBefore( int elem, int num )
{
	if ( !m_pAttribute->MarkDirty() )
		return Data().InvalidIndex();

	// UNDO HOOK
	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoAttributeArrayInsertBefore<T> *pUndo = new CUndoAttributeArrayInsertBefore<T>( m_pAttribute, elem, num );
		g_pDataModel->AddUndoElement( pUndo );
	}

	int index = Data().InsertMultipleBefore( elem, num );
	for ( int i = 0; i < num; ++i )
	{
		CDmAttributeInfo<T>::SetDefaultValue( Data()[ index + i ] );	
	}
	OnAttributeArrayElementAdded( index, index + num - 1 );
	m_pAttribute->OnChanged( true );
	return index;
}


//-----------------------------------------------------------------------------
// Removal
//-----------------------------------------------------------------------------
template< class T >
void CDmArrayAttributeOp<T>::FastRemove( int elem )
{
	if ( !m_pAttribute->MarkDirty() )
		return;

	// UNDO HOOK
	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoAttributeArrayRemoveElement<T> *pUndo = new CUndoAttributeArrayRemoveElement<T>( m_pAttribute, true, elem, 1 );
		g_pDataModel->AddUndoElement( pUndo );
	}

	OnAttributeArrayElementRemoved( elem, elem );
	Data().FastRemove( elem );
	m_pAttribute->OnChanged( true );
}

template< class T >
void CDmArrayAttributeOp<T>::Remove( int elem )
{
	if ( !Data().IsValidIndex( elem ) )
		return;

	if ( !m_pAttribute->MarkDirty() )
		return;

	// UNDO HOOK
	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoAttributeArrayRemoveElement<T> *pUndo = new CUndoAttributeArrayRemoveElement<T>( m_pAttribute, false, elem, 1 );
		g_pDataModel->AddUndoElement( pUndo );
	}

	OnAttributeArrayElementRemoved( elem, elem );
	Data().Remove( elem );
	m_pAttribute->OnChanged( true );
}

template< class T >
void CDmArrayAttributeOp<T>::RemoveAll()
{
	if ( !m_pAttribute->MarkDirty() )
		return;

	// UNDO HOOK
	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoAttributeArrayCopyAllElement<T> *pUndo = new CUndoAttributeArrayCopyAllElement<T>( m_pAttribute, NULL, 0 );
		g_pDataModel->AddUndoElement( pUndo );
	}

	OnAttributeArrayElementRemoved( 0, Data().Count() - 1 );
	Data().RemoveAll();
	m_pAttribute->OnChanged( true );
}

template< class T >
void CDmArrayAttributeOp<T>::RemoveMultiple( int elem, int num )
{
	if ( !m_pAttribute->MarkDirty() )
		return;

	// UNDO HOOK
	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoAttributeArrayRemoveElement<T> *pUndo = new CUndoAttributeArrayRemoveElement<T>( m_pAttribute, false, elem, num );
		g_pDataModel->AddUndoElement( pUndo );
	}

	OnAttributeArrayElementRemoved( elem, elem + num - 1 );
	Data().RemoveMultiple( elem, num );
	m_pAttribute->OnChanged( true );
}

// Memory deallocation
template< class T >
void CDmArrayAttributeOp<T>::Purge()
{
	if ( !m_pAttribute->MarkDirty() )
		return;

	// UNDO HOOK
	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoAttributeArrayCopyAllElement<T> *pUndo = new CUndoAttributeArrayCopyAllElement<T>( m_pAttribute, NULL, true );
		g_pDataModel->AddUndoElement( pUndo );
	}

	OnAttributeArrayElementRemoved( 0, Data().Count() - 1 );
	Data().Purge();
	m_pAttribute->OnChanged( true );
}


//-----------------------------------------------------------------------------
// Copy Array
//-----------------------------------------------------------------------------
template< class T >
void CDmArrayAttributeOp<T>::PerformCopyArray( const T *pArray, int nCount )
{
	Data().CopyArray( pArray, nCount );
}

template<> void CDmArrayAttributeOp<DmElementHandle_t>::PerformCopyArray( const DmElementHandle_t *pArray, int nCount )
{
	Data().RemoveAll();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( ShouldInsertElement( pArray[ i ] ) )
		{
			Data().AddToTail( pArray[ i ] );
		}
	}
}

template< class T >
void CDmArrayAttributeOp<T>::CopyArray( const T *pArray, int nCount )
{
	if ( Data().Base() == pArray )
	{
		int nCurrentCount = Data().Count();
		if ( nCurrentCount > nCount )
		{
			RemoveMultiple( nCount, nCurrentCount - nCount );
		}
		else if ( nCurrentCount < nCount )
		{
			InsertMultipleBefore( nCurrentCount, nCount - nCurrentCount );
		}
		return;
	}

	if ( !m_pAttribute->MarkDirty() )
		return;

	// UNDO HOOK
	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoAttributeArrayCopyAllElement<T> *pUndo = new CUndoAttributeArrayCopyAllElement<T>( m_pAttribute, pArray, nCount );
		g_pDataModel->AddUndoElement( pUndo );
	}

	OnAttributeArrayElementRemoved( 0, Data().Count() - 1 );
	PerformCopyArray( pArray, nCount );
	OnAttributeArrayElementAdded( 0, Data().Count() - 1 );
	m_pAttribute->OnChanged( true );
}


//-----------------------------------------------------------------------------
// Swap Array
//-----------------------------------------------------------------------------
template< class T >
void CDmArrayAttributeOp<T>::SwapArray( CUtlVector< T >& src )
{
	// this is basically just a faster version of CopyArray
	// the end result (for purposes of undo) are the same
	// but there's no copy - just a pointer/etc swap
	if ( !m_pAttribute->MarkDirty() )
		return;

	// UNDO HOOK
	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoAttributeArrayCopyAllElement<T> *pUndo = new CUndoAttributeArrayCopyAllElement<T>( m_pAttribute, src.Base(), src.Count() );
		g_pDataModel->AddUndoElement( pUndo );
	}

	OnAttributeArrayElementRemoved( 0, Data().Count() - 1 );
	Data().Swap( src );
	OnAttributeArrayElementAdded( 0, Data().Count() - 1 );
	m_pAttribute->OnChanged( true );
}


template< > void CDmArrayAttributeOp<DmElementHandle_t>::SwapArray( CUtlVector< DmElementHandle_t >& src )
{
	// This feature doesn't work for elements..
	// Can't do it owing to typesafety reasons as well as supporting the NODUPLICATES feature.
	Assert( 0 );
}


//-----------------------------------------------------------------------------
// Set value
//-----------------------------------------------------------------------------
template< class T >
void CDmArrayAttributeOp<T>::Set( int i, const T& value )
{
	if ( i < 0 || i >= Data().Count() )
	{
		Assert( !"CDmAttributeArray<T>::Set out of range value!\n" );
		return;
	}

	// Don't bother doing anything if the attribute is equal
	if ( IsAttributeEqual( Data()[i], value ) )
		return;

	if ( !ShouldInsert( value ) )
		return;

	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoArrayAttributeSetValueElement<T> *pUndo = new CUndoArrayAttributeSetValueElement<T>( m_pAttribute, i, value );
		g_pDataModel->AddUndoElement( pUndo );
	}

	OnAttributeArrayElementRemoved( i, i ); 
	Data()[i] = value;
	OnAttributeArrayElementAdded( i, i ); 
	m_pAttribute->OnChanged( false );
}

template< class T >
void CDmArrayAttributeOp<T>::Set( CDmAttribute *pAttribute, int i, DmAttributeType_t valueType, const void *pValue )
{
	if ( valueType == ArrayTypeToValueType( pAttribute->GetType() ) )
	{
		// This version is in IDmAttributeOp
		CDmArrayAttributeOp< T > array( pAttribute );
		array.Set( i, *(const T*)pValue );
	}
}


//-----------------------------------------------------------------------------
// Set multiple values
//-----------------------------------------------------------------------------
template< class T >
void CDmArrayAttributeOp<T>::SetMultiple( int i, int nCount, const T* pValue )
{
	if ( i < 0 || ( i+nCount ) > Data().Count() )
	{
		AssertMsg( 0, "CDmAttributeArray<T>::SetMultiple out of range value!\n" );
		return;
	}

	// Test for equality
	bool bEqual = true;
	for ( int j = 0; j < nCount; ++j )
	{
		if ( !IsAttributeEqual( Data()[i+j], pValue[j] ) )
		{
			bEqual = false;
			break;
		}
	}
	if ( bEqual )
		return;

	if ( !m_pAttribute->MarkDirty() )
		return;

	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoArrayAttributeSetMultipleValueElement<T> *pUndo = new CUndoArrayAttributeSetMultipleValueElement<T>( m_pAttribute, i, nCount, pValue );
		g_pDataModel->AddUndoElement( pUndo );
	}

	OnAttributeArrayElementRemoved( i, i+nCount-1 ); 
	for ( int j = 0; j < nCount; ++j )
	{
		if ( ShouldInsertElement( pValue[j] ) )
		{
			Data()[i+j] = pValue[j];
		}
	}
	OnAttributeArrayElementAdded( i, i+nCount-1 ); 
	m_pAttribute->OnChanged( false );
}

template< class T >
void CDmArrayAttributeOp<T>::SetMultiple( CDmAttribute *pAttribute, int i, int nCount, DmAttributeType_t valueType, const void *pValue )
{
	if ( valueType == ArrayTypeToValueType( pAttribute->GetType() ) )
	{
		// This version is in IDmAttributeOp
		CDmArrayAttributeOp< T > array( pAttribute );
		array.SetMultiple( i, nCount, (const T*)pValue );
	}
}


//-----------------------------------------------------------------------------
// Version of SetValue that's in IDmAttributeOp
//-----------------------------------------------------------------------------
template< class T >
void CDmArrayAttributeOp<T>::SetValue( CDmAttribute *pAttribute, DmAttributeType_t valueType, const void *pValue )
{
	Assert( pAttribute->GetType() == valueType );
	if ( pAttribute->GetType() == valueType )
	{
		CDmArrayAttributeOp<T> accessor( pAttribute );
		const CUtlVector<T>* pArray = reinterpret_cast< const CUtlVector<T>* >( pValue );
		accessor.CopyArray( pArray->Base(), pArray->Count() );
	}
}


//-----------------------------------------------------------------------------
// Swap
//-----------------------------------------------------------------------------
template< class T >
void CDmArrayAttributeOp<T>::Swap( int i, int j )
{
	if ( i == j )
		return;

	// TODO - define Swap<T> for all attribute types to make swapping strings 
	// and voids fast (via pointer swaps, rather than 3 copies!)
	T vk = Data()[ i ];
	if ( IsAttributeEqual( vk, Data()[j] ) )
		return;

	if ( !m_pAttribute->MarkDirty() )
		return;

	if ( g_pDataModel->UndoEnabledForElement( m_pAttribute->GetOwner() ) )
	{
		CUndoArrayAttributeSetValueElement<T> *pUndo = new CUndoArrayAttributeSetValueElement<T>( m_pAttribute, i, Data()[ j ] );
		g_pDataModel->AddUndoElement( pUndo );
		pUndo = new CUndoArrayAttributeSetValueElement<T>( m_pAttribute, j, vk );
		g_pDataModel->AddUndoElement( pUndo );
	}

	OnAttributeArrayElementRemoved( i, i ); 
	Data()[i] = Data()[j];
	OnAttributeArrayElementAdded( i, i ); 

	OnAttributeArrayElementRemoved( j, j ); 
	Data()[j] = vk;
	OnAttributeArrayElementAdded( j, j ); 

	m_pAttribute->OnChanged( false );
}


//-----------------------------------------------------------------------------
// Methods related to serialization
//-----------------------------------------------------------------------------
template< class T >
bool CDmArrayAttributeOp<T>::Unserialize( CDmAttribute *pAttribute, CUtlBuffer &buf )
{
	if ( !pAttribute->MarkDirty() )
		return false;

	MEM_ALLOC_CREDIT_CLASS();

	CUtlVector< T > tempVal;
	if ( !::Unserialize( buf, tempVal ) )
		return false;

	// Don't need undo hook since this goes through Swap route
	CDmArrayAttributeOp<T> accessor( pAttribute );
	accessor.SwapArray( tempVal );

	return true;
}

template<> bool CDmArrayAttributeOp<DmElementHandle_t>::Unserialize( CDmAttribute *pAttribute, CUtlBuffer &buf )
{
	// Need to specialize this because element handles can't use SwapArray
	// because it's incapable of doing type safety checks
	if ( !CDmAttributeAccessor::MarkDirty( pAttribute ) )
		return false;

	MEM_ALLOC_CREDIT_CLASS();

	CUtlVector< DmElementHandle_t > tempVal;
	if ( !::Unserialize( buf, tempVal ) )
		return false;

	// Don't need undo hook since this goes through copy route
	CDmArrayAttributeOp<DmElementHandle_t> accessor( pAttribute );
	accessor.CopyArray( tempVal.Base(), tempVal.Count() );

	return true;
}

// Serialization of a single element
template< class T >
bool CDmArrayAttributeOp<T>::SerializeElement( const CDmAttribute *pAttribute, int nElement, CUtlBuffer &buf )
{
	CDmrArrayConst<T> array( pAttribute );
	return ::Serialize( buf, array[ nElement ] );
}

template< class T >
bool CDmArrayAttributeOp<T>::UnserializeElement( CDmAttribute *pAttribute, CUtlBuffer &buf )
{
	if ( !CDmAttributeAccessor::MarkDirty( pAttribute ) )
		return false;

	MEM_ALLOC_CREDIT_CLASS();

	T temp;
	if ( !::Unserialize( buf, temp ) )
		return false;

	CDmArrayAttributeOp<T> accessor( pAttribute );
	accessor.AddToTail( temp );

	pAttribute->OnChanged( true );

	return true;
}

template<>
bool CDmArrayAttributeOp< CUtlSymbolLarge >::UnserializeElement( CDmAttribute *pAttribute, CUtlBuffer &buf )
{
	if ( !CDmAttributeAccessor::MarkDirty( pAttribute ) )
		return false;

	MEM_ALLOC_CREDIT_CLASS();

	CUtlString tempString;
	if ( !::Unserialize( buf, tempString ) )
		return false;

	CDmrStringArray stringArray( pAttribute );
	stringArray.AddToTail( tempString.Get() );

	pAttribute->OnChanged( true );

	return true;
}

template< class T >
bool CDmArrayAttributeOp<T>::UnserializeElement( CDmAttribute *pAttribute, int nElement, CUtlBuffer &buf )
{
	if ( !CDmAttributeAccessor::MarkDirty( pAttribute ) )
		return false;

	CDmrArray<T> array( pAttribute );
	if ( array.Count() <= nElement )
		return false;

	MEM_ALLOC_CREDIT_CLASS();

	if ( !::Unserialize( buf, *const_cast<T*>( &array[nElement] ) ) )
		return false;

	pAttribute->OnChanged();

	return true;
}

template<>
bool CDmArrayAttributeOp< CUtlSymbolLarge >::UnserializeElement( CDmAttribute *pAttribute, int nElement, CUtlBuffer &buf )
{
	if ( !CDmAttributeAccessor::MarkDirty( pAttribute ) )
		return false;

	CDmrStringArray array( pAttribute );
	if ( array.Count() <= nElement )
		return false;

	MEM_ALLOC_CREDIT_CLASS();

	CUtlString tempString;
	if ( !::Unserialize( buf, tempString ) )
		return false;

	array.Set( nElement, tempString.Get() );

	pAttribute->OnChanged();

	return true;
}


template< class T >
void CDmArrayAttributeOp<T>::OnUnserializationFinished( CDmAttribute *pAttribute )
{
	CDmArrayAttributeOp<T> ref( pAttribute );
	int nCount = ref.Count();
	if ( nCount > 0 )
	{
		ref.OnAttributeArrayElementAdded( 0, nCount - 1, false );
	}
	CDmAttributeAccessor::OnChanged( pAttribute, true, true );
}


//-----------------------------------------------------------------------------
//
// CDmAttribute begins here
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Memory pool used for CDmAttribute
//-----------------------------------------------------------------------------
CUtlMemoryPool g_AttrAlloc( sizeof( CDmAttribute ), 4096, CUtlMemoryPool::GROW_SLOW, "CDmAttribute pool" );


//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------

// turn memdbg off temporarily so we can get at placement new
#include "tier0/memdbgoff.h"

CDmAttribute *CDmAttribute::CreateAttribute( CDmElement *pOwner, DmAttributeType_t type, const char *pAttributeName )
{
	switch( type )
	{
	case AT_UNKNOWN:
		Assert( 0 );
		return NULL;

	default:
		{
			void *pMem = 0;
			{
				DMX_PROFILE_SCOPE( CreateAttribute_Alloc );
				pMem = g_AttrAlloc.Alloc( sizeof( CDmAttribute ) );
			}
			{
				DMX_PROFILE_SCOPE( CreateAttribute_new_CDmAttribute );
				return ::new( pMem ) CDmAttribute( pOwner, type, pAttributeName );
			}
		}
	}
}

CDmAttribute *CDmAttribute::CreateExternalAttribute( CDmElement *pOwner, DmAttributeType_t type, const char *pAttributeName, void *pExternalMemory )
{
	switch( type )
	{
	case AT_UNKNOWN:
		Assert( 0 );
		return NULL;

	default:
		{
			void *pMem = 0;
			{
				DMX_PROFILE_SCOPE( CreateExternalAttribute_Alloc );
				pMem = g_AttrAlloc.Alloc( sizeof( CDmAttribute ) );
			}
			{
				DMX_PROFILE_SCOPE( CreateExternalAttribute_new_CDmAttribute );
				return ::new( pMem ) CDmAttribute( pOwner, type, pAttributeName, pExternalMemory );
			}
		}
	}
}

void CDmAttribute::DestroyAttribute( CDmAttribute *pAttribute )
{
	if ( !pAttribute )
		return;

	switch( pAttribute->GetType() )
	{
	case AT_UNKNOWN:
		break;

	default:
		pAttribute->~CDmAttribute();

#ifdef _DEBUG
		memset( pAttribute, 0xDD, sizeof(CDmAttribute) );
#endif

		g_AttrAlloc.Free( pAttribute );
		break;
	}
}

// turn memdbg back on after using placement new
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmAttribute::CDmAttribute( CDmElement *pOwner, DmAttributeType_t type, const char *pAttributeName ) :
	m_pData( NULL )
{
	{
		DMX_PROFILE_SCOPE( CDmAttribute_Init );
		Init( pOwner, type, pAttributeName );
	}
	{
		DMX_PROFILE_SCOPE( CDmAttribute_CreateAttributeData );
		CreateAttributeData();
	}
}

CDmAttribute::CDmAttribute( CDmElement *pOwner, DmAttributeType_t type, const char *pAttributeName, void *pMemory ) :
	m_pData( pMemory )
{
	{
		DMX_PROFILE_SCOPE( CDmAttributeExternal_Init );
		Init( pOwner, type, pAttributeName );
	}
	{
		DMX_PROFILE_SCOPE( CDmAttributeExternal_SetDefaultValue );
		s_pAttrInfo[ GetType() ]->SetDefaultValue( m_pData );
	}

	AddFlag( FATTRIB_EXTERNAL );
}


void CDmAttribute::Init( CDmElement *pOwner, DmAttributeType_t type, const char *pAttributeName )
{
	m_pOwner = pOwner;
	{
		DMX_PROFILE_SCOPE( CDmAttribute_m_Name_GetSymbol );
		m_Name = g_pDataModel->GetSymbol( pAttributeName );
	}
	m_nFlags = type;
	m_Handle = DMATTRIBUTE_HANDLE_INVALID;
	m_pNext = NULL;

	switch ( type )
	{
	case AT_ELEMENT:
	case AT_ELEMENT_ARRAY:
		m_nFlags |= FATTRIB_TOPOLOGICAL;
		break;
	}
}

CDmAttribute::~CDmAttribute()
{
	switch( GetType() )
	{
	case AT_ELEMENT:
		g_pDataModelImp->OnElementReferenceRemoved( GetValue<DmElementHandle_t>(), this );
		break;
	
	case AT_ELEMENT_ARRAY:
		{
			CDmrElementArray<> array( this );
			int nElements = array.Count();
			for ( int i = 0; i < nElements; ++i )
			{
				g_pDataModelImp->OnElementReferenceRemoved( array.GetHandle( i ), this );
			}
		}
		break;
	}
	
	InvalidateHandle();
	DeleteAttributeData();
}


//-----------------------------------------------------------------------------
// Creates the attribute data
//-----------------------------------------------------------------------------
void CDmAttribute::CreateAttributeData()
{
	// Free the attribute memory
	if ( !IsFlagSet( FATTRIB_EXTERNAL ) )
	{
		Assert( !m_pData );
		m_pData = s_pAttrInfo[ GetType() ]->CreateAttributeData( );
	}
}


//-----------------------------------------------------------------------------
// Deletes the attribute data
//-----------------------------------------------------------------------------
void CDmAttribute::DeleteAttributeData()
{
	// Free the attribute memory
	if ( m_pData && !IsFlagSet( FATTRIB_EXTERNAL ) )
	{
		s_pAttrInfo[ GetType() ]->DestroyAttributeData( m_pData );
		m_pData = NULL;
	}
}

// ChangeType fails (and returns false) for external attributes (ones who's data is owned by their element)
bool CDmAttribute::ChangeType_UNSAFE( DmAttributeType_t type )
{
	DmAttributeType_t oldType = GetType();
	if ( type == oldType )
		return true;

	if ( IsFlagSet( FATTRIB_EXTERNAL ) )
		return false;

	if ( IsArrayType( oldType ) || IsArrayType( type ) )
	{
		if ( !IsArrayType( oldType ) || !IsArrayType( type ) )
			return false;

		// shortcut the conversion and callbacks for array type changes - this only works for bitwise exact type changes (currently only int <-> time)
		m_nFlags = ( m_nFlags & ( 0xffff & ~FATTRIB_TYPEMASK ) ) | type;
		return true;
	}

	int nNewSize = s_pAttrInfo[ type ]->DataSize();
	int nOldSize = s_pAttrInfo[ oldType ]->DataSize();
	void *pOldData = stackalloc( nOldSize );
	V_memcpy( pOldData, m_pData, nOldSize );

	// force any necessary callbacks to be called (ie OnElementReferenceRemoved)
	s_pAttrInfo[ oldType ]->SetToDefaultValue( this ); // may trigger callbacks

	uint16 nFlags = ( m_nFlags & ( 0xffff & ~FATTRIB_TYPEMASK ) ) | type;
	if ( nOldSize != nNewSize )
	{
		DeleteAttributeData();
		m_nFlags = nFlags;
		CreateAttributeData();
	}
	else
	{
		m_nFlags = nFlags;
	}

	// put data in state s.t. when callbacks are called, no harm is done (OnElementReferenceAdded)
	void *pDefaultData = stackalloc( nNewSize );
	s_pAttrInfo[ type ]->SetDefaultValue( pDefaultData ); // does NOT trigger callbacks

	SetValue( oldType, pOldData ); // may trigger callbacks, but only if the types are convertable
	return true;
}


//-----------------------------------------------------------------------------
// Used only in attribute element arrays
//-----------------------------------------------------------------------------
void CDmAttribute::SetElementTypeSymbol( CUtlSymbolLarge typeSymbol )
{
	switch ( GetType() )
	{
	case AT_ELEMENT:
		{
			DmElementAttribute_t *pData = GetData< DmElementHandle_t >();
			Assert( pData->m_Handle == DMELEMENT_HANDLE_INVALID || ::IsA( pData->m_Handle, typeSymbol ) );
			pData->m_ElementType = typeSymbol;
		}
		break;

	case AT_ELEMENT_ARRAY:
		{
#ifdef _DEBUG
			CDmrElementArray<> array( this );
			if ( array.GetElementType() != UTL_INVAL_SYMBOL_LARGE )
			{
				int i;
				int c = array.Count();
				for ( i = 0; i < c; ++i )
				{
					Assert( array.GetHandle( i ) == DMELEMENT_HANDLE_INVALID || ::IsA( array.GetHandle( i ), typeSymbol ) );
				}
			}
#endif

			DmElementArray_t *pData = GetArrayData< DmElementHandle_t >();
			pData->m_ElementType = typeSymbol;
		}
		break;

	default:
		Assert(0);
		break;
	}
}

CUtlSymbolLarge CDmAttribute::GetElementTypeSymbol() const
{
	switch ( GetType() )
	{
	case AT_ELEMENT:
		return GetData< DmElementHandle_t >()->m_ElementType;

	case AT_ELEMENT_ARRAY:
		return GetArrayData< DmElementHandle_t >()->m_ElementType;

	default:
		Assert(0);
		break;
	}

	return UTL_INVAL_SYMBOL_LARGE;
}


//-----------------------------------------------------------------------------
// Is modification allowed in this phase?
//-----------------------------------------------------------------------------
bool CDmAttribute::ModificationAllowed() const 
{ 
	if ( IsFlagSet( FATTRIB_READONLY ) )
		return false;

	DmPhase_t phase = g_pDmElementFramework->GetPhase();
	if ( phase == PH_EDIT || phase == PH_EDIT_APPLY || phase == PH_EDIT_RESOLVE )
		return true;
	if ( ( phase == PH_OPERATE ) && !IsFlagSet( FATTRIB_TOPOLOGICAL ) )
		return true;

	return false;
}

bool CDmAttribute::MarkDirty() 
{ 
	if ( !ModificationAllowed() )
	{
		Assert( 0 );
		return false;
	}

	AddFlag( FATTRIB_DIRTY | FATTRIB_OPERATOR_DIRTY );
	CDmeElementAccessor::MarkDirty( m_pOwner );

	return true;
}


//-----------------------------------------------------------------------------
// Called after the attribute has changed
//-----------------------------------------------------------------------------
void CDmAttribute::OnChanged( bool bArrayCountChanged, bool bIsTopological )
{
	if ( IsFlagSet( FATTRIB_HAS_CALLBACK ) && CDmeElementAccessor::AreOnChangedCallbacksEnabled( m_pOwner ) )
	{
		m_pOwner->OnAttributeChanged( this );
	}

	if ( bIsTopological || IsTopological( GetType() ) )
	{
		g_pDataModelImp->NotifyState( NOTIFY_CHANGE_TOPOLOGICAL );
	}
	else
	{
		g_pDataModelImp->NotifyState( bArrayCountChanged ? NOTIFY_CHANGE_ATTRIBUTE_ARRAY_SIZE : NOTIFY_CHANGE_ATTRIBUTE_VALUE );
	}
}


//-----------------------------------------------------------------------------
// Type conversion related methods
//-----------------------------------------------------------------------------
template< class T > bool CDmAttribute::IsTypeConvertable() const
{
	return CDmAttributeInfo< T >::ATTRIBUTE_TYPE == GetType();
}

template<> bool CDmAttribute::IsTypeConvertable<bool>() const
{
	DmAttributeType_t type = GetType();
	return type == AT_TIME || type == AT_FLOAT || type == AT_INT || type == AT_BOOL;
}

template<> bool CDmAttribute::IsTypeConvertable<int>() const
{
	DmAttributeType_t type = GetType();
	return type == AT_TIME || type == AT_FLOAT || type == AT_INT || type == AT_BOOL;
}

template<> bool CDmAttribute::IsTypeConvertable<float>() const
{
	DmAttributeType_t type = GetType();
	return type == AT_TIME || type == AT_FLOAT || type == AT_INT || type == AT_BOOL;
}

template<> bool CDmAttribute::IsTypeConvertable<DmeTime_t>() const
{
	DmAttributeType_t type = GetType();
	return type == AT_TIME || type == AT_FLOAT || type == AT_INT || type == AT_BOOL;
}

template<> bool CDmAttribute::IsTypeConvertable<QAngle>() const
{
	DmAttributeType_t type = GetType();
	return type == AT_QANGLE || type == AT_QUATERNION;
}

template<> bool CDmAttribute::IsTypeConvertable<Quaternion>() const
{
	DmAttributeType_t type = GetType();
	return type == AT_QUATERNION || type == AT_QANGLE;
}

template< class T > void CDmAttribute::CopyData( const T& value )
{
	*reinterpret_cast< T* >( m_pData ) = value;
}

template< class T > void CDmAttribute::CopyDataOut( T& value ) const
{
	value = *reinterpret_cast< const T* >( m_pData );
}

template<> void CDmAttribute::CopyData( const bool& value )
{
	switch( GetType() )
	{
	case AT_BOOL:
		*reinterpret_cast< bool* >( m_pData ) = value;
		break;

	case AT_INT:
		*reinterpret_cast< int* >( m_pData ) = value ? 1 : 0;
		break;

	case AT_FLOAT:
		*reinterpret_cast< float* >( m_pData ) = value ? 1.0f : 0.0f;
		break;

	case AT_TIME:
		*reinterpret_cast< DmeTime_t* >( m_pData ) = value ? DmeTime_t( 1.0f ) : DMETIME_ZERO;
		break;
	}
}

template<> void CDmAttribute::CopyDataOut( bool& value ) const
{
	switch( GetType() )
	{
	case AT_BOOL:
		value = *reinterpret_cast< bool* >( m_pData );
		break;

	case AT_INT:
		value = *reinterpret_cast< int* >( m_pData ) != 0;
		break;

	case AT_FLOAT:
		value = *reinterpret_cast< float* >( m_pData ) != 0.0f;
		break;

	case AT_TIME:
		value = *reinterpret_cast< DmeTime_t* >( m_pData ) != DMETIME_ZERO;
		break;
	}
}

template<> void CDmAttribute::CopyData( const int& value )
{
	switch( GetType() )
	{
	case AT_BOOL:
		*reinterpret_cast< bool* >( m_pData ) = value != 0;
		break;

	case AT_INT:
		*reinterpret_cast< int* >( m_pData ) = value;
		break;

	case AT_FLOAT:
		*reinterpret_cast< float* >( m_pData ) = value;
		break;

	case AT_TIME:
		reinterpret_cast< DmeTime_t* >( m_pData )->SetTenthsOfMS( value );
		break;
	}
}

template<> void CDmAttribute::CopyDataOut( int& value ) const
{
	switch( GetType() )
	{
	case AT_BOOL:
		value = *reinterpret_cast< bool* >( m_pData ) ? 1 : 0;
		break;

	case AT_INT:
		value = *reinterpret_cast< int* >( m_pData );
		break;

	case AT_FLOAT:
		value = *reinterpret_cast< float* >( m_pData );
		break;

	case AT_TIME:
		value = reinterpret_cast< DmeTime_t* >( m_pData )->GetTenthsOfMS();
		break;
	}
}

template<> void CDmAttribute::CopyData( const float& value )
{
	switch( GetType() )
	{
	case AT_BOOL:
		*reinterpret_cast< bool* >( m_pData ) = value != 0.0f;
		break;

	case AT_INT:
		*reinterpret_cast< int* >( m_pData ) = value;
		break;

	case AT_FLOAT:
		*reinterpret_cast< float* >( m_pData ) = value;
		break;

	case AT_TIME:
		reinterpret_cast< DmeTime_t* >( m_pData )->SetSeconds( value );
		break;
	}
}

template<> void CDmAttribute::CopyDataOut( float& value ) const
{
	switch( GetType() )
	{
	case AT_BOOL:
		value = *reinterpret_cast< bool* >( m_pData ) ? 1.0f : 0.0f;
		break;

	case AT_INT:
		value = *reinterpret_cast< int* >( m_pData );
		break;

	case AT_FLOAT:
		value = *reinterpret_cast< float* >( m_pData );
		break;

	case AT_TIME:
		value = reinterpret_cast< DmeTime_t* >( m_pData )->GetSeconds();
		break;
	}
}

template<> void CDmAttribute::CopyData( const DmeTime_t& value )
{
	switch( GetType() )
	{
	case AT_BOOL:
		*reinterpret_cast< bool* >( m_pData ) = value != DMETIME_ZERO;
		break;

	case AT_INT:
		*reinterpret_cast< int* >( m_pData ) = value.GetTenthsOfMS();
		break;

	case AT_FLOAT:
		*reinterpret_cast< float* >( m_pData ) = value.GetSeconds();
		break;

	case AT_TIME:
		*reinterpret_cast< DmeTime_t* >( m_pData ) = value;
		break;
	}
}

template<> void CDmAttribute::CopyDataOut( DmeTime_t& value ) const
{
	switch( GetType() )
	{
	case AT_BOOL:
		value = *reinterpret_cast< bool* >( m_pData ) ? DmeTime_t( 1.0f ) : DMETIME_ZERO;
		break;

	case AT_INT:
		value.SetTenthsOfMS( *reinterpret_cast< int* >( m_pData ) );
		break;

	case AT_FLOAT:
		value.SetSeconds( *reinterpret_cast< float* >( m_pData ) );
		break;

	case AT_TIME:
		value = *reinterpret_cast< DmeTime_t* >( m_pData );
		break;
	}
}

template<> void CDmAttribute::CopyData( const QAngle& value )
{
	switch( GetType() )
	{
	case AT_QANGLE:
		*reinterpret_cast< QAngle* >( m_pData ) = value;
		break;

	case AT_QUATERNION:
		{
			Quaternion qValue;
			AngleQuaternion( value, qValue );
			*reinterpret_cast< Quaternion* >( m_pData ) = qValue;
		}
		break;
	}
}

template<> void CDmAttribute::CopyDataOut( QAngle& value ) const
{
	switch( GetType() )
	{
	case AT_QANGLE:
		value = *reinterpret_cast< QAngle* >( m_pData );
		break;

	case AT_QUATERNION:
		QuaternionAngles( *reinterpret_cast< Quaternion* >( m_pData ), value );
		break;
	}
}

template<> void CDmAttribute::CopyData( const Quaternion& value )
{
	switch( GetType() )
	{
	case AT_QANGLE:
		{
			QAngle aValue;
			QuaternionAngles( value, aValue );
			*reinterpret_cast< QAngle* >( m_pData ) = aValue;
		}
		break;

	case AT_QUATERNION:
		*reinterpret_cast< Quaternion* >( m_pData ) = value;
		break;
	}
}

template<> void CDmAttribute::CopyDataOut( Quaternion& value ) const
{
	switch( GetType() )
	{
	case AT_QANGLE:
		AngleQuaternion( *reinterpret_cast< QAngle* >( m_pData ), value );
		break;

	case AT_QUATERNION:
		value = *reinterpret_cast< Quaternion* >( m_pData );
		break;
	}
}

template<> void CDmAttribute::CopyData( const DmElementHandle_t& value )
{
	g_pDataModelImp->OnElementReferenceRemoved( GetValue<DmElementHandle_t>(), this );
	*reinterpret_cast< DmElementHandle_t* >( m_pData ) = value;
	g_pDataModelImp->OnElementReferenceAdded( value, this );
}


//-----------------------------------------------------------------------------
// Should we be allowed to modify the attribute data?
//-----------------------------------------------------------------------------
template< class T > 
bool CDmAttribute::ShouldModify( const T& value )
{
	if ( !IsTypeConvertable<T>() )
		return false;

	if ( ( GetType() == CDmAttributeInfo<T>::ATTRIBUTE_TYPE ) && IsAttributeEqual( GetValue<T>(), value ) )
		return false;

	return MarkDirty();
}

template<> bool CDmAttribute::ShouldModify( const DmElementHandle_t& value )
{
	if ( !IsTypeConvertable<DmElementHandle_t>() )
		return false;

	if ( IsAttributeEqual( GetValue<DmElementHandle_t>(), value ) )
		return false;

	DmElementAttribute_t *pData = GetData<DmElementHandle_t>();
	if ( pData->m_ElementType != UTL_INVAL_SYMBOL_LARGE && !::IsA( value, pData->m_ElementType ) )
		return false;

	return MarkDirty();
}


//-----------------------------------------------------------------------------
// Main entry point for single-valued SetValue
//-----------------------------------------------------------------------------
template< class T >
void CDmAttribute::SetValue( const T &value )
{
	if ( !ShouldModify( value ) )
		return;

	// UNDO Hook
	if ( g_pDataModel->UndoEnabledForElement( m_pOwner ) )
	{
		CUndoAttributeSetValueElement<T> *pUndo = new CUndoAttributeSetValueElement<T>( this, value );
		g_pDataModel->AddUndoElement( pUndo );
	}

	CopyData< T >( value );

	if ( IsFlagSet( FATTRIB_HAS_CALLBACK ) && CDmeElementAccessor::AreOnChangedCallbacksEnabled( m_pOwner ) )
	{
		m_pOwner->OnAttributeChanged( this );
	}

	g_pDataModelImp->NotifyState( IsTopological( GetType() ) ? NOTIFY_CHANGE_TOPOLOGICAL : NOTIFY_CHANGE_ATTRIBUTE_VALUE );
}


//-----------------------------------------------------------------------------
// Main entry point for single-valued SetValue
//-----------------------------------------------------------------------------
template< class T > T& 
CDmAttribute::BeginModifyValueInPlace( DmAttributeModifyHandle_t *pHandle )
{
	*pHandle = NULL;

	// Type conversion is not allowed
	if ( (int)GetType() != (int)CDmAttributeInfo< T >::ATTRIBUTE_TYPE )
	{
		Assert( 0 );
		return *( T* )NULL;
	}

	// UNDO Hook
	if ( g_pDataModel->UndoEnabledForElement( m_pOwner ) )
	{
		CUndoAttributeSetValueElement<T> *pUndo = new CUndoAttributeSetValueElement<T>( this );
		*pHandle = (DmAttributeModifyHandle_t)pUndo;
	}

	return *reinterpret_cast< T* >( m_pData );
}

template< class T >
void CDmAttribute::EndModifyValueInPlace( DmAttributeModifyHandle_t handle )
{
	if ( (int)GetType() != (int)CDmAttributeInfo< T >::ATTRIBUTE_TYPE )
		return;

	CUndoAttributeSetValueElement<T> *pUndo = ( CUndoAttributeSetValueElement<T>* )( handle );
	if ( pUndo )
	{
		pUndo->SetEndValue( this );
		g_pDataModel->AddUndoElement( pUndo );
	}

	if ( IsFlagSet( FATTRIB_HAS_CALLBACK ) && CDmeElementAccessor::AreOnChangedCallbacksEnabled( m_pOwner ) )
	{
		m_pOwner->OnAttributeChanged( this );
	}

	g_pDataModelImp->NotifyState( IsTopological( GetType() ) ? NOTIFY_CHANGE_TOPOLOGICAL : NOTIFY_CHANGE_ATTRIBUTE_VALUE );
}


//-----------------------------------------------------------------------------
// Versions that work on arrays
//-----------------------------------------------------------------------------
#define ATTRIBUTE_SET_VALUE_ARRAY( _type )										\
	template<> void CDmAttribute::SetValue( const CUtlVector< _type >& value )	\
	{																			\
		CDmArrayAttributeOp< _type > accessor( this );							\
		accessor.CopyArray( value.Base(), value.Count() );						\
	}

void CDmAttribute::SetValue( const CDmAttribute *pAttribute )
{
	s_pAttrInfo[ GetType() ]->SetValue( this, pAttribute->GetType(), pAttribute->GetAttributeData() );
}

void CDmAttribute::SetValue( CDmAttribute *pAttribute )
{
	s_pAttrInfo[ GetType() ]->SetValue( this, pAttribute->GetType(), pAttribute->GetAttributeData() );
}

void CDmAttribute::SetValue( DmAttributeType_t valueType, const void *pValue )
{
	s_pAttrInfo[ GetType() ]->SetValue( this, valueType, pValue );
}


//-----------------------------------------------------------------------------
// Sets the attribute to its default value based on its type
//-----------------------------------------------------------------------------
void CDmAttribute::SetToDefaultValue()
{
	s_pAttrInfo[ GetType() ]->SetToDefaultValue( this );
}


//-----------------------------------------------------------------------------
// Convert to and from string
//-----------------------------------------------------------------------------
void CDmAttribute::SetValueFromString( const char *pValue )
{
	switch ( GetType() )
	{
	case AT_STRING:
		SetValue( pValue );
		break;

	default:
		{
			int nLen = pValue ? Q_strlen( pValue ) : 0;
			if ( nLen == 0 )
			{
				SetToDefaultValue();
				break;
			}

			CUtlBuffer buf( pValue, nLen + 1, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
			if ( !Unserialize( buf ) )
			{
				SetToDefaultValue();
			}
		}
		break;
	}
}

const char *CDmAttribute::GetValueAsString( char *pBuffer, size_t nBufLen ) const
{
	Assert( pBuffer );
	CUtlBuffer buf( pBuffer, nBufLen, CUtlBuffer::TEXT_BUFFER );
	Serialize( buf );
	return pBuffer;
}


//-----------------------------------------------------------------------------
// Name, type
//-----------------------------------------------------------------------------
const char* CDmAttribute::GetTypeString() const
{
	return ::GetTypeString( GetType() );
}

const char *GetTypeString( DmAttributeType_t type )
{
	if ( ( type >= 0 ) && ( type < AT_TYPE_COUNT ) )
		return s_pAttrInfo[ type ]->AttributeTypeName();
	return "unknown";
}

bool CDmAttribute::IsStandard() const
{
	static CUtlSymbolLarge nameSym = g_pDataModel->GetSymbol( "name" );
	return m_Name == nameSym;
}

void CDmAttribute::SetName( const char *pNewName )
{
	if ( m_pOwner->HasAttribute( pNewName ) && Q_stricmp( GetName(), pNewName ) )
	{
		Warning( "Tried to rename from '%s' to '%s', but '%s' already exists\n",
			GetName(), pNewName, pNewName );
		return;
	}

	if ( !MarkDirty() )
		return;

	// UNDO Hook
	if ( g_pDataModel->UndoEnabledForElement( m_pOwner ) )
	{
		CUndoAttributeRenameElement *pUndo = new CUndoAttributeRenameElement( this, pNewName );
		g_pDataModel->AddUndoElement( pUndo );
	}

	m_Name = g_pDataModel->GetSymbol( pNewName );
	g_pDataModelImp->NotifyState( NOTIFY_CHANGE_TOPOLOGICAL );
}


//-----------------------------------------------------------------------------
// Serialization
//-----------------------------------------------------------------------------
bool CDmAttribute::SerializesOnMultipleLines() const
{
	return s_pAttrInfo[ GetType() ]->SerializesOnMultipleLines();
}

bool CDmAttribute::Serialize( CUtlBuffer &buf ) const
{
	return s_pAttrInfo[ GetType() ]->Serialize( this, buf );
}

bool CDmAttribute::Unserialize( CUtlBuffer &buf )
{
	return s_pAttrInfo[ GetType() ]->Unserialize( this, buf );
}

bool CDmAttribute::IsIdenticalToSerializedValue( CUtlBuffer &buf ) const
{
	return s_pAttrInfo[ GetType() ]->IsIdenticalToSerializedValue( this, buf );
}

bool CDmAttribute::SerializeElement( int nElement, CUtlBuffer &buf ) const
{
	return s_pAttrInfo[ GetType() ]->SerializeElement( this, nElement, buf );
}

bool CDmAttribute::UnserializeElement( CUtlBuffer &buf )
{
	return s_pAttrInfo[ GetType() ]->UnserializeElement( this, buf );
}

bool CDmAttribute::UnserializeElement( int nElement, CUtlBuffer &buf )
{
	return s_pAttrInfo[ GetType() ]->UnserializeElement( this, nElement, buf );
}

// Called by elements after unserialization of their attributes is complete
void CDmAttribute::OnUnserializationFinished()
{
	return s_pAttrInfo[ GetType() ]->OnUnserializationFinished( this );
}


//-----------------------------------------------------------------------------
// Get the attribute/create an attribute handle
//-----------------------------------------------------------------------------
DmAttributeHandle_t CDmAttribute::GetHandle( bool bCreate )
{
	if ( (m_Handle == DMATTRIBUTE_HANDLE_INVALID) && bCreate )
	{
		m_Handle = g_pDataModelImp->AcquireAttributeHandle( this );
	}

	Assert( (m_Handle == DMATTRIBUTE_HANDLE_INVALID) || g_pDataModel->IsAttributeHandleValid( m_Handle ) );
	return m_Handle;
}

void CDmAttribute::InvalidateHandle()
{
	g_pDataModelImp->ReleaseAttributeHandle( m_Handle );
	m_Handle = DMATTRIBUTE_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Memory usage estimations
//-----------------------------------------------------------------------------
bool HandleCompare( const DmElementHandle_t &a, const DmElementHandle_t &b )
{
	return a == b;
}

unsigned int HandleHash( const DmElementHandle_t &h )
{
	return (unsigned int)h;
}

int CDmAttribute::EstimateMemoryUsage( TraversalDepth_t depth ) const
{
	CUtlHash< DmElementHandle_t > visited( 1024, 0, 0, HandleCompare, HandleHash );
	return EstimateMemoryUsageInternal( visited, depth, 0 ) ;
}

int CDmAttribute::EstimateMemoryUsageInternal( CUtlHash< DmElementHandle_t > &visited, TraversalDepth_t depth, int *pCategories ) const
{
	int nOverhead = sizeof( *this );
	int nAttributeDataSize = s_pAttrInfo[ GetType() ]->DataSize();
	int nTotalMemory = nOverhead + nAttributeDataSize;
	int nAttributeExtraDataSize = 0;

	if ( IsArrayType( GetType() ) )
	{
		CDmrGenericArrayConst array( this );
		int nCount = array.Count();
		DmAttributeType_t valueType = ArrayTypeToValueType( GetType() );
		nAttributeExtraDataSize = nCount * s_pAttrInfo[ valueType ]->ValueSize();	// Data in the UtlVector
		int nMallocOverhead = ( nCount == 0 ) ? 0 : 8;	// malloc overhead inside the vector
		nOverhead += nMallocOverhead;
		nTotalMemory += nAttributeExtraDataSize + nMallocOverhead;
	}

	if ( pCategories )
	{
		++pCategories[MEMORY_CATEGORY_ATTRIBUTE_COUNT];
		pCategories[MEMORY_CATEGORY_ATTRIBUTE_DATA] += nAttributeDataSize + nAttributeExtraDataSize;
		pCategories[MEMORY_CATEGORY_ATTRIBUTE_OVERHEAD] += nOverhead;
		if ( !IsDataInline() )
		{
			pCategories[MEMORY_CATEGORY_OUTER] -= nAttributeDataSize;
			Assert( pCategories[MEMORY_CATEGORY_OUTER] >= 0 );
			nTotalMemory -= nAttributeDataSize;
		}
	}

	switch ( GetType() )
	{
	case AT_VOID:
		{
			const CUtlBinaryBlock &value = GetValue< CUtlBinaryBlock >();
			if ( pCategories )
			{
				pCategories[MEMORY_CATEGORY_ATTRIBUTE_DATA] += value.Length();
				pCategories[MEMORY_CATEGORY_ATTRIBUTE_OVERHEAD] += 8;
			}
			return nTotalMemory + value.Length() + 8;
		}

	case AT_VOID_ARRAY:
		{
			const CUtlVector< CUtlBinaryBlock > &array = GetValue< CUtlVector< CUtlBinaryBlock > >();
			for ( int i = 0; i < array.Count(); ++i )
			{
				if ( pCategories )
				{
					pCategories[MEMORY_CATEGORY_ATTRIBUTE_DATA] += array[ i ].Length();
					pCategories[MEMORY_CATEGORY_ATTRIBUTE_OVERHEAD] += 8;
				}
				nTotalMemory += array[ i ].Length() + 8;
			}
			return nTotalMemory;
		}

	case AT_ELEMENT:
		if ( ShouldTraverse( this, depth ) )
		{
			CDmElement *pElement = GetValueElement<CDmElement>();
			if ( pElement )
			{
				nTotalMemory += CDmeElementAccessor::EstimateMemoryUsage( pElement, visited, depth, pCategories );
			}
		}
		return nTotalMemory;

	case AT_ELEMENT_ARRAY:
		if ( ShouldTraverse( this, depth ) )
		{
			CDmrElementArrayConst<> array( this );
			for ( int i = 0; i < array.Count(); ++i )
			{
				CDmElement *pElement = array[ i ];
				if ( pElement )
				{
					nTotalMemory += CDmeElementAccessor::EstimateMemoryUsage( pElement, visited, depth, pCategories );
				}
			}
		}
		return nTotalMemory;
	}

	return nTotalMemory;
}


//-----------------------------------------------------------------------------
//
// CDmaArrayBase starts here
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
template< class T, class B >
CDmaArrayConstBase<T,B>::CDmaArrayConstBase( )
{
	m_pAttribute = NULL;
}


//-----------------------------------------------------------------------------
// Search
//-----------------------------------------------------------------------------
template< class T, class B >
int CDmaArrayConstBase<T,B>::Find( const T &value ) const
{
	return Value().Find( value );
}


//-----------------------------------------------------------------------------
// Insertion
//-----------------------------------------------------------------------------
template< class T, class B >
int CDmaArrayBase<T,B>::AddToTail()
{
	T defaultVal;
	CDmAttributeInfo<T>::SetDefaultValue( defaultVal );	
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	return accessor.InsertBefore( Value().Count(), defaultVal );
}

template< class T, class B >
int	CDmaArrayBase<T,B>::InsertBefore( int elem )
{
	T defaultVal;
	CDmAttributeInfo<T>::SetDefaultValue( defaultVal );	
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	return accessor.InsertBefore( elem, defaultVal );
}

template< class T, class B >
int	CDmaArrayBase<T,B>::AddToTail( const T& src )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	return accessor.InsertBefore( Value().Count(), src );
}

template< class T, class B >
int	CDmaArrayBase<T,B>::InsertBefore( int elem, const T& src )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	return accessor.InsertBefore( elem, src );
}

template< class T, class B >
int	CDmaArrayBase<T,B>::AddMultipleToTail( int num )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	return accessor.InsertMultipleBefore( Value().Count(), num );
}

template< class T, class B >
int CDmaArrayBase<T,B>::InsertMultipleBefore( int elem, int num )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	return accessor.InsertMultipleBefore( elem, num );
}

template< class T, class B >
void CDmaArrayBase<T,B>::EnsureCount( int num )
{
	int nCurrentCount = Value().Count();
	if ( nCurrentCount < num )
	{
		AddMultipleToTail( num - nCurrentCount );
	}
}


//-----------------------------------------------------------------------------
// Element modification
//-----------------------------------------------------------------------------
template< class T, class B >
void CDmaArrayBase<T,B>::Set( int i, const T& value )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	return accessor.Set( i, value );
}

template< class T, class B >
void CDmaArrayBase<T,B>::SetMultiple( int i, int nCount, const T* pValue )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	accessor.SetMultiple( i, nCount, pValue );
}

template< class T, class B >
void CDmaArrayBase<T,B>::Swap( int i, int j )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	accessor.Swap( i, j );
}

template< class T, class B >
void CDmaArrayBase<T,B>::SwapArray( CUtlVector< T > &array )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	accessor.SwapArray( array );
}


//-----------------------------------------------------------------------------
// Copy
//-----------------------------------------------------------------------------
template< class T, class B >
void CDmaArrayBase<T,B>::CopyArray( const T *pArray, int nCount )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	accessor.CopyArray( pArray, nCount );
}


//-----------------------------------------------------------------------------
// Removal
//-----------------------------------------------------------------------------
template< class T, class B >
void CDmaArrayBase<T,B>::FastRemove( int elem )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	accessor.FastRemove( elem );
}

template< class T, class B >
void CDmaArrayBase<T,B>::Remove( int elem )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	accessor.Remove( elem );
}

template< class T, class B >
void CDmaArrayBase<T,B>::RemoveAll()
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	accessor.RemoveAll();
}

template< class T, class B >
void CDmaArrayBase<T,B>::RemoveMultiple( int elem, int num )
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	accessor.RemoveMultiple( elem, num );
}


//-----------------------------------------------------------------------------
// Memory management
//-----------------------------------------------------------------------------
template< class T, class B >
void CDmaArrayBase<T,B>::EnsureCapacity( int num )
{
	Value().EnsureCapacity( num );
}

template< class T, class B >
void CDmaArrayBase<T,B>::Purge()
{
	CDmArrayAttributeOp<T> accessor( this->m_pAttribute );
	accessor.Purge();
}


//-----------------------------------------------------------------------------
// Attribute initialization
//-----------------------------------------------------------------------------
template< class T, class B >
void CDmaDecorator<T,B>::Init( CDmElement *pOwner, const char *pAttributeName, int nFlags = 0 )
{
	Assert( pOwner );
	this->m_pAttribute = pOwner->AddExternalAttribute( pAttributeName, CDmAttributeInfo<CUtlVector<T> >::AttributeType(), &Value() );
	Assert( m_pAttribute );
	if ( nFlags )
	{
		this->m_pAttribute->AddFlag( nFlags );
	}
}


//-----------------------------------------------------------------------------
// Attribute attribute reference
//-----------------------------------------------------------------------------
template< class T, class BaseClass >
void CDmrDecoratorConst<T,BaseClass>::Init( const CDmAttribute* pAttribute )
{
	if ( pAttribute && pAttribute->GetType() == CDmAttributeInfo< CUtlVector< T > >::AttributeType() )
	{
		this->m_pAttribute = const_cast<CDmAttribute*>( pAttribute );
		Attach( this->m_pAttribute->GetAttributeData() );
	}
	else
	{
		this->m_pAttribute = NULL;
		Attach( NULL );
	}
}

template< class T, class BaseClass >
void CDmrDecoratorConst<T,BaseClass>::Init( const CDmElement *pElement, const char *pAttributeName )
{
	const CDmAttribute *pAttribute = NULL;
	if ( pElement && pAttributeName && pAttributeName[0] )
	{
		pAttribute = pElement->GetAttribute( pAttributeName );
	}
	Init( pAttribute );
}

template< class T, class BaseClass >
bool CDmrDecoratorConst<T,BaseClass>::IsValid() const
{
	return this->m_pAttribute != NULL;
}


template< class T, class BaseClass >
void CDmrDecorator<T,BaseClass>::Init( CDmAttribute* pAttribute )
{
	if ( pAttribute && pAttribute->GetType() == CDmAttributeInfo< CUtlVector< T > >::AttributeType() )
	{
		this->m_pAttribute = pAttribute;
		Attach( this->m_pAttribute->GetAttributeData() );
	}
	else
	{
		this->m_pAttribute = NULL;
		Attach( NULL );
	}
}

template< class T, class BaseClass >
void CDmrDecorator<T,BaseClass>::Init( CDmElement *pElement, const char *pAttributeName, bool bAddAttribute )
{
	CDmAttribute *pAttribute = NULL;
	if ( pElement && pAttributeName && pAttributeName[0] )
	{
		if ( bAddAttribute )
		{
			pAttribute = pElement->AddAttribute( pAttributeName, CDmAttributeInfo< CUtlVector< T > >::AttributeType() );
		}
		else
		{
			pAttribute = pElement->GetAttribute( pAttributeName );
		}
	}
	Init( pAttribute );
}

template< class T, class BaseClass >
bool CDmrDecorator<T,BaseClass>::IsValid() const
{
	return this->m_pAttribute != NULL;
}


//-----------------------------------------------------------------------------
//
// Generic array access
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Helper macros to make switch statements based on type 
//-----------------------------------------------------------------------------
#define ARRAY_METHOD_VOID( _type, _func )														\
	case CDmAttributeInfo< CUtlVector< _type > >::ATTRIBUTE_TYPE:								\
		{																						\
			CDmrArray< _type > &array = *reinterpret_cast< CDmrArray< _type > * >( &arrayShared );	\
			array.Init( m_pAttribute );															\
			array._func;																		\
		}																						\
		break;

#define APPLY_ARRAY_METHOD_VOID( _func )					\
	CDmrArray<int> arrayShared;								\
	switch( m_pAttribute->GetType() )						\
	{														\
		ARRAY_METHOD_VOID( bool, _func )					\
		ARRAY_METHOD_VOID( int, _func )						\
		ARRAY_METHOD_VOID( float, _func )					\
		ARRAY_METHOD_VOID( Color, _func )					\
		ARRAY_METHOD_VOID( Vector2D, _func )				\
		ARRAY_METHOD_VOID( Vector, _func )					\
		ARRAY_METHOD_VOID( Vector4D, _func )				\
		ARRAY_METHOD_VOID( QAngle, _func )					\
		ARRAY_METHOD_VOID( Quaternion, _func )				\
		ARRAY_METHOD_VOID( VMatrix, _func )					\
		ARRAY_METHOD_VOID( CUtlSymbolLarge, _func )			\
		ARRAY_METHOD_VOID( CUtlBinaryBlock, _func )			\
		ARRAY_METHOD_VOID( DmeTime_t, _func )				\
		ARRAY_METHOD_VOID( DmElementHandle_t, _func )		\
	default:												\
		break;												\
	}

#define ARRAY_METHOD_RET( _type, _func )														\
	case CDmAttributeInfo< CUtlVector< _type > >::ATTRIBUTE_TYPE:								\
		{																						\
			CDmrArray< _type > &array = *reinterpret_cast< CDmrArray< _type > * >( &arrayShared );	\
			array.Init( m_pAttribute );															\
			return array._func;																	\
		}

#define APPLY_ARRAY_METHOD_RET( _func )					\
	CDmrArray<int> arrayShared;							\
	switch( m_pAttribute->GetType() )					\
	{													\
		ARRAY_METHOD_RET( bool, _func );				\
		ARRAY_METHOD_RET( int, _func );					\
		ARRAY_METHOD_RET( float, _func );				\
		ARRAY_METHOD_RET( Color, _func );				\
		ARRAY_METHOD_RET( Vector2D, _func );			\
		ARRAY_METHOD_RET( Vector, _func );				\
		ARRAY_METHOD_RET( Vector4D, _func );			\
		ARRAY_METHOD_RET( QAngle, _func );				\
		ARRAY_METHOD_RET( Quaternion, _func );			\
		ARRAY_METHOD_RET( VMatrix, _func );				\
		ARRAY_METHOD_RET( CUtlSymbolLarge, _func );		\
		ARRAY_METHOD_RET( CUtlBinaryBlock, _func );		\
		ARRAY_METHOD_RET( DmeTime_t, _func );			\
		ARRAY_METHOD_RET( DmElementHandle_t, _func );	\
		default:										\
			break;										\
	}

CDmrGenericArrayConst::CDmrGenericArrayConst() : m_pAttribute( NULL )
{
}

CDmrGenericArrayConst::CDmrGenericArrayConst( const CDmAttribute* pAttribute )
{
	Init( pAttribute );
}

CDmrGenericArrayConst::CDmrGenericArrayConst( const CDmElement *pElement, const char *pAttributeName )
{
	Init( pElement, pAttributeName );
}

void CDmrGenericArrayConst::Init( const CDmAttribute *pAttribute )
{
	if ( pAttribute && IsArrayType( pAttribute->GetType() ) )
	{
		m_pAttribute = const_cast<CDmAttribute*>( pAttribute );
	}
	else
	{
		m_pAttribute = NULL;
	}
}

void CDmrGenericArrayConst::Init( const CDmElement *pElement, const char *pAttributeName )
{
	const CDmAttribute *pAttribute = ( pElement && pAttributeName && pAttributeName[0] ) ? pElement->GetAttribute( pAttributeName ) : NULL;
	Init( pAttribute );
}

int CDmrGenericArrayConst::Count() const
{
	APPLY_ARRAY_METHOD_RET( Count() );
	return 0;
}

const void* CDmrGenericArrayConst::GetUntyped( int i ) const
{
	APPLY_ARRAY_METHOD_RET( GetUntyped( i ) );
	return NULL;
}

const char* CDmrGenericArrayConst::GetAsString( int i, char *pBuffer, size_t nBufLen ) const
{
	if ( ( Count() > i ) && ( i >= 0 ) )
	{
		CUtlBuffer buf( pBuffer, nBufLen, CUtlBuffer::TEXT_BUFFER );
		m_pAttribute->SerializeElement( i, buf );
	}
	else
	{
		pBuffer[0] = 0;
	}
	return pBuffer;
}


CDmrGenericArray::CDmrGenericArray( CDmAttribute* pAttribute )
{
	Init( pAttribute );
}

CDmrGenericArray::CDmrGenericArray( CDmElement *pElement, const char *pAttributeName )
{
	Init( pElement, pAttributeName );
}

void CDmrGenericArray::EnsureCount( int num )
{
	APPLY_ARRAY_METHOD_VOID( EnsureCount(num) );
}

int CDmrGenericArray::AddToTail()
{
	APPLY_ARRAY_METHOD_RET( AddToTail() );
	return -1;
}

void CDmrGenericArray::Remove( int elem )
{
	APPLY_ARRAY_METHOD_VOID( Remove(elem) );
}

void CDmrGenericArray::RemoveAll()
{
	APPLY_ARRAY_METHOD_VOID( RemoveAll() );
}

void CDmrGenericArray::SetMultiple( int i, int nCount, DmAttributeType_t valueType, const void *pValue )
{
	s_pAttrInfo[ m_pAttribute->GetType() ]->SetMultiple( m_pAttribute, i, nCount, valueType, pValue );
}

void CDmrGenericArray::Set( int i, DmAttributeType_t valueType, const void *pValue )
{
	s_pAttrInfo[ m_pAttribute->GetType() ]->Set( m_pAttribute, i, valueType, pValue );
}

void CDmrGenericArray::SetFromString( int i, const char *pValue )
{
	if ( ( Count() > i ) && ( i >= 0 ) )
	{
		if ( m_pAttribute->GetType() == AT_STRING_ARRAY )
		{
			CDmArrayAttributeOp< CUtlSymbolLarge > array( m_pAttribute );
			CUtlSymbolLarge symbol = g_pDataModel->GetSymbol( pValue );
			array.Set( i, symbol );
		}
		else
		{
			int nLen = pValue ? Q_strlen( pValue ) : 0;
			CUtlBuffer buf( pValue, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
			m_pAttribute->UnserializeElement( i, buf );
		}
	}
}


//-----------------------------------------------------------------------------
// Skip unserialization for an attribute type (unserialize into a dummy variable)
//-----------------------------------------------------------------------------
bool SkipUnserialize( CUtlBuffer &buf, DmAttributeType_t type )
{
	if ( type == AT_UNKNOWN )
		return false;

	return s_pAttrInfo[ type ]->SkipUnserialize( buf );
}


//-----------------------------------------------------------------------------
// returns the number of attributes currently allocated
//-----------------------------------------------------------------------------
int GetAllocatedAttributeCount()
{
	return g_AttrAlloc.Count();
}


//-----------------------------------------------------------------------------
// Attribute type->name and name->attribute type
//-----------------------------------------------------------------------------
const char *AttributeTypeName( DmAttributeType_t type )
{
	if ( ( type >= 0 ) && ( type < AT_TYPE_COUNT ) ) 
		return s_pAttrInfo[ type ]->AttributeTypeName();
	return "unknown";
}

DmAttributeType_t AttributeType( const char *pName )
{
	for ( int i = 0; i < AT_TYPE_COUNT; ++i )
	{
		if ( !Q_stricmp( s_pAttrInfo[ i ]->AttributeTypeName(), pName ) )
			return (DmAttributeType_t)i;
	}

	return AT_UNKNOWN;
}


//-----------------------------------------------------------------------------
// Explicit template instantiation for the known attribute types
//-----------------------------------------------------------------------------
template <class T>
class CInstantiateOp
{
public:
	CInstantiateOp()
	{
		s_pAttrInfo[ CDmAttributeInfo<T>::ATTRIBUTE_TYPE ] = new CDmAttributeOp< T >;
	}
};
static CInstantiateOp<DmUnknownAttribute_t> __s_AttrDmUnknownAttribute_t;

#define INSTANTIATE_GENERIC_OPS( _className )	\
	template< > class CInstantiateOp< CUtlVector< _className > >	\
	{														\
	public:													\
		CInstantiateOp()									\
		{													\
			s_pAttrInfo[ CDmAttributeInfo< CUtlVector< _className > >::ATTRIBUTE_TYPE ] = new CDmArrayAttributeOp< _className >; \
		}													\
	};														\
	static CInstantiateOp< _className > __s_Attr ## _className;	\
	static CInstantiateOp< CUtlVector< _className > > __s_AttrArray ## _className;	

#define DEFINE_ATTRIBUTE_TYPE( _type )	\
	INSTANTIATE_GENERIC_OPS( _type )	\
	ATTRIBUTE_SET_VALUE_ARRAY( _type )	\
	template void CDmAttribute::SetValue< _type >( const _type& value );										\
	template _type &CDmAttribute::BeginModifyValueInPlace< _type >( DmAttributeModifyHandle_t *pHandle );		\
	template void CDmAttribute::EndModifyValueInPlace< _type >( DmAttributeModifyHandle_t handle );				\
	template class CDmArrayAttributeOp< _type >;																\
	template class CDmaArrayBase< _type, CDmaDataInternal< CUtlVector< _type > > >;								\
	template class CDmaArrayBase< _type, CDmaDataExternal< CUtlVector< _type > > >;								\
	template class CDmaArrayConstBase< _type, CDmaDataInternal< CUtlVector< _type > > >;						\
	template class CDmaArrayConstBase< _type, CDmaDataExternal< CUtlVector< _type > > >;						\
	template class CDmaDecorator< _type, CDmaArrayBase< _type, CDmaDataInternal< CUtlVector< _type > > > >;		\
	template class CDmrDecorator< _type, CDmaArrayBase< _type, CDmaDataExternal< CUtlVector< _type > > > >;		\
	template class CDmrDecoratorConst< _type, CDmaArrayConstBase< _type, CDmaDataExternal< CUtlVector< _type > > > >;


DEFINE_ATTRIBUTE_TYPE( int )
DEFINE_ATTRIBUTE_TYPE( float )
DEFINE_ATTRIBUTE_TYPE( bool )
DEFINE_ATTRIBUTE_TYPE( Color )
DEFINE_ATTRIBUTE_TYPE( Vector2D )
DEFINE_ATTRIBUTE_TYPE( Vector )
DEFINE_ATTRIBUTE_TYPE( Vector4D )
DEFINE_ATTRIBUTE_TYPE( QAngle )
DEFINE_ATTRIBUTE_TYPE( Quaternion )
DEFINE_ATTRIBUTE_TYPE( VMatrix )
DEFINE_ATTRIBUTE_TYPE( CUtlSymbolLarge )
DEFINE_ATTRIBUTE_TYPE( CUtlBinaryBlock )
DEFINE_ATTRIBUTE_TYPE( DmeTime_t )
DEFINE_ATTRIBUTE_TYPE( DmElementHandle_t )

template class CDmaDecorator< CUtlSymbolLarge, CDmaStringArrayBase< CDmaDataInternal< CUtlVector< CUtlSymbolLarge > > > >;
template class CDmrDecorator< CUtlSymbolLarge, CDmaStringArrayBase< CDmaDataExternal< CUtlVector< CUtlSymbolLarge > > > >;
template class CDmrDecoratorConst< CUtlSymbolLarge, CDmaStringArrayConstBase< CDmaArrayConstBase< CUtlSymbolLarge, CDmaDataExternal< CUtlVector< CUtlSymbolLarge > > > > >;
//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMATTRIBUTE_H
#define DMATTRIBUTE_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/attributeflags.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmattributetypes.h"
#include "datamodel/dmvar.h"
#include "tier1/utlhash.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CUtlBuffer;


//-----------------------------------------------------------------------------
// Used for in-place modification
//-----------------------------------------------------------------------------
typedef void *DmAttributeModifyHandle_t;


//-----------------------------------------------------------------------------
// Purpose: A general purpose pAttribute.  Eventually will be extensible to arbitrary user types
//-----------------------------------------------------------------------------
class CDmAttribute
{
public:
	// Returns the type
	DmAttributeType_t GetType() const;
	const char *GetTypeString() const;
	template< class T > bool IsA() const;

	// ChangeType fails (and returns false) for external attributes (ones who's data is owned by their element)
	bool ChangeType_UNSAFE( DmAttributeType_t type );

	bool IsStandard() const;

	// Returns the name. NOTE: The utlsymbol
	// can be turned into a string by using g_pDataModel->String();
	const char *GetName() const;
	CUtlSymbolLarge	GetNameSymbol() const;
	void		SetName( const char *newName );

	// Gets the attribute value
	// NOTE: GetValueUntyped is used with GetType() for use w/ SetValue( type, void* )
	template< class T > const T& GetValue() const;
	template< class T > const T& GetValue( const T& defaultValue ) const;
	const char					*GetValueString() const;
	template< class E > E		*GetValueElement() const;
	const void					*GetValueUntyped() const; 

	// Sets the attribute value
	template< class T > void SetValue( const T &value );
	template< class E > void SetValue( E* pValue );
	void	SetValue( const void *pValue, size_t nSize );

	// Copies w/ type conversion (if possible) from another attribute
	void	SetValue( const CDmAttribute *pAttribute );
	void	SetValue( CDmAttribute *pAttribute );
	void	SetValue( DmAttributeType_t valueType, const void *pValue );

	// Sets the attribute to its default value based on its type
	void	SetToDefaultValue();

	// Convert to and from string
	void SetValueFromString( const char *pValue );
	const char *GetValueAsString( char *pBuffer, size_t nBufLen ) const;

	// Used for in-place modification of the data; preferable to set value
	// for large data like arrays or binary blocks
	template< class T > T& BeginModifyValueInPlace( DmAttributeModifyHandle_t *pHandle );
	template< class T > void EndModifyValueInPlace( DmAttributeModifyHandle_t handle );

	// Used for element and element array attributes; it specifies which type of
	// elements are valid to be referred to by this attribute
	void		SetElementTypeSymbol( CUtlSymbolLarge typeSymbol );
	CUtlSymbolLarge	GetElementTypeSymbol() const;

	// Returns the next attribute
	CDmAttribute *NextAttribute();
	const CDmAttribute *NextAttribute() const;

	// Returns the owner
	CDmElement *GetOwner();

	// Methods related to flags
	void	AddFlag( int flags );
	void	RemoveFlag( int flags );
	void	ClearFlags();
	int		GetFlags() const;
	bool	IsFlagSet( int flags ) const;

	// Serialization
	bool	Serialize( CUtlBuffer &buf ) const;
	bool	Unserialize( CUtlBuffer &buf );
	bool    IsIdenticalToSerializedValue( CUtlBuffer &buf ) const;

	// Serialization of a single element. 
	// First version of UnserializeElement adds to tail if it worked
	// Second version overwrites, but does not add, the element at the specified index 
	bool	SerializeElement( int nElement, CUtlBuffer &buf ) const;
	bool	UnserializeElement( CUtlBuffer &buf );
	bool	UnserializeElement( int nElement, CUtlBuffer &buf );

	// Does this attribute serialize on multiple lines?
	bool	SerializesOnMultipleLines() const;

	// Get the attribute/create an attribute handle
	DmAttributeHandle_t GetHandle( bool bCreate = true );

	// estimate memory overhead
	int		EstimateMemoryUsage( TraversalDepth_t depth ) const;

private:
	// Class factory
	static CDmAttribute *CreateAttribute( CDmElement *pOwner, DmAttributeType_t type, const char *pAttributeName );
	static CDmAttribute *CreateExternalAttribute( CDmElement *pOwner, DmAttributeType_t type, const char *pAttributeName, void *pExternalMemory );
	static void DestroyAttribute( CDmAttribute *pAttribute );

	// Constructor, destructor
	CDmAttribute( CDmElement *pOwner, DmAttributeType_t type, const char *pAttributeName );
	CDmAttribute( CDmElement *pOwner, DmAttributeType_t type, const char *pAttributeName, void *pMemory );
	~CDmAttribute();

	// Used when constructing CDmAttributes
	void Init( CDmElement *pOwner, DmAttributeType_t type, const char *pAttributeName );

	// Used when shutting down, indicates DmAttributeHandle_t referring to this are invalid
	void InvalidateHandle();

	// Called when the attribute changes
	void OnChanged( bool bArrayCountChanged = false, bool bIsTopological = false );

	// Is modification allowed in this phase?
	bool ModificationAllowed() const;

	// Mark the attribute as being dirty
	bool MarkDirty();

	// Is the data inline in a containing element class?
	bool IsDataInline() const;

	// Allocates, frees internal data storage
	void CreateAttributeData();
	void DeleteAttributeData();

	// Gets at the internal data storage 
	void* GetAttributeData();
	const void*	GetAttributeData() const;
	template < class T > typename CDmAttributeInfo< T >::StorageType_t* GetData();
	template < class T > const typename CDmAttributeInfo< T >::StorageType_t* GetData() const;
	template < class T > typename CDmAttributeInfo< CUtlVector< T > >::StorageType_t* GetArrayData();
	template < class T > const typename CDmAttributeInfo< CUtlVector< T > >::StorageType_t* GetArrayData() const;

	// Used by CDmElement to manage the list of attributes it owns
	CDmAttribute **GetNextAttributeRef();

	// Implementational function used for memory consumption estimation computation
	int EstimateMemoryUsageInternal( CUtlHash< DmElementHandle_t > &visited, TraversalDepth_t depth, int *pCategories ) const;

	// Called by elements after unserialization of their attributes is complete
	void OnUnserializationFinished();

	template< class T > bool IsTypeConvertable() const;
	template< class T > bool ShouldModify( const T& src );
	template< class T > void CopyData( const T& src );
	template< class T > void CopyDataOut( T& dest ) const;

private:
	CDmAttribute *m_pNext;
	void *m_pData;
	CDmElement *m_pOwner;
	DmAttributeHandle_t m_Handle;
	uint16 m_nFlags;
	CUtlSymbolLarge m_Name;

	friend class CDmElement;
	friend class CDmAttributeAccessor;
	template< class T > friend class CDmrElementArray;
	template< class E > friend class CDmrElementArrayConst;
	template< class T > friend class CDmaArrayAccessor;
	template< class T, class B > friend class CDmrDecorator;
	template< class T, class B > friend class CDmrDecoratorConst;
	template< class T > friend class CDmArrayAttributeOp;
};

	 
//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline DmAttributeType_t CDmAttribute::GetType() const
{
	return (DmAttributeType_t)( m_nFlags & FATTRIB_TYPEMASK );
}

template< class T > inline bool CDmAttribute::IsA() const
{
	return GetType() == CDmAttributeInfo< T >::AttributeType();
}

inline const char *CDmAttribute::GetName() const
{
	return m_Name.String();
}

inline CUtlSymbolLarge CDmAttribute::GetNameSymbol() const
{
	return m_Name;
}


//-----------------------------------------------------------------------------
// Iteration
//-----------------------------------------------------------------------------
inline CDmAttribute *CDmAttribute::NextAttribute()
{
	return m_pNext;
}

inline const CDmAttribute *CDmAttribute::NextAttribute() const
{
	return m_pNext;
}


//-----------------------------------------------------------------------------
// Returns the owner
//-----------------------------------------------------------------------------
inline CDmElement *CDmAttribute::GetOwner()
{
	return m_pOwner;
}


//-----------------------------------------------------------------------------
// Value getting methods
//-----------------------------------------------------------------------------
template< class T > 
inline const T& CDmAttribute::GetValue( const T& defaultValue ) const
{
	if ( (int)GetType() == (int)CDmAttributeInfo< T >::ATTRIBUTE_TYPE )
		return *reinterpret_cast< const T* >( m_pData );

	if ( IsTypeConvertable< T >() )
	{
		static T tempVal;
		CopyDataOut( tempVal );
		return tempVal;
	}

	Assert( 0 );
	return defaultValue;
}

template< class T > 
inline const T& CDmAttribute::GetValue() const
{
	static CDmaVar< T > defaultVal;
	return GetValue( defaultVal.Get() );
}

inline const char *CDmAttribute::GetValueString() const
{
	Assert( GetType() == AT_STRING );
	if ( GetType() != AT_STRING )
		return NULL;

	CUtlSymbolLarge symbol = GetValue< CUtlSymbolLarge >();
	return symbol.String();
}

// used with GetType() for use w/ SetValue( type, void* )
inline const void* CDmAttribute::GetValueUntyped() const
{ 
	return m_pData; 
} 

#ifndef POSIX
template< class E > 
inline E* CDmAttribute::GetValueElement() const
{
	Assert( GetType() == AT_ELEMENT );
	if ( GetType() == AT_ELEMENT )
		return GetElement<E>( this->GetValue<DmElementHandle_t>( ) );
	return NULL;
}
#endif


//-----------------------------------------------------------------------------
// Value setting methods
//-----------------------------------------------------------------------------
template< class E > 
inline void CDmAttribute::SetValue( E* pValue )
{
	Assert( GetType() == AT_ELEMENT );
	if ( GetType() == AT_ELEMENT )
	{
		SetValue( pValue ? pValue->GetHandle() : DMELEMENT_HANDLE_INVALID );
	}
}

template<>
inline void CDmAttribute::SetValue( const char *pValue )
{
	CUtlSymbolLarge symbol = g_pDataModel->GetSymbol( pValue );
	return SetValue( symbol );
}

template<>
inline void CDmAttribute::SetValue( char *pValue )
{
	return SetValue( (const char *)pValue );
}

inline void CDmAttribute::SetValue( const void *pValue, size_t nSize )
{
	CUtlBinaryBlock buf( pValue, nSize );
	return SetValue( buf );
}


//-----------------------------------------------------------------------------
// Methods related to flags
//-----------------------------------------------------------------------------
inline void CDmAttribute::AddFlag( int nFlags )
{
	m_nFlags |= ( nFlags & ~FATTRIB_TYPEMASK );
}

inline void CDmAttribute::RemoveFlag( int nFlags )
{
	m_nFlags &= ~nFlags | FATTRIB_TYPEMASK;
}

inline void CDmAttribute::ClearFlags()
{
	RemoveFlag( 0xffffffff );
}

inline int CDmAttribute::GetFlags() const
{
	return m_nFlags & ~FATTRIB_TYPEMASK;
}

inline bool CDmAttribute::IsFlagSet( int nFlags ) const
{
	return ( nFlags & GetFlags() ) != 0;
}

inline bool CDmAttribute::IsDataInline() const
{ 
	return !IsFlagSet( FATTRIB_EXTERNAL ); 
}


//-----------------------------------------------------------------------------
// Gets at the internal data storage 
//-----------------------------------------------------------------------------
inline void* CDmAttribute::GetAttributeData() 
{ 
	return m_pData; 
}

inline const void* CDmAttribute::GetAttributeData() const 
{ 
	return m_pData; 
}

template < class T >
inline typename CDmAttributeInfo< T >::StorageType_t* CDmAttribute::GetData()
{
	return ( typename CDmAttributeInfo< T >::StorageType_t* )m_pData;
}

template < class T >
inline typename CDmAttributeInfo< CUtlVector< T > >::StorageType_t* CDmAttribute::GetArrayData()
{
	return ( typename CDmAttributeInfo< CUtlVector< T > >::StorageType_t* )m_pData;
}

template < class T >
inline const typename CDmAttributeInfo< T >::StorageType_t* CDmAttribute::GetData() const
{
	return ( const typename CDmAttributeInfo< T >::StorageType_t* )m_pData;
}

template < class T >
inline const typename CDmAttributeInfo< CUtlVector< T > >::StorageType_t* CDmAttribute::GetArrayData() const
{
	return ( const typename CDmAttributeInfo< CUtlVector< T > >::StorageType_t* )m_pData;
}


//-----------------------------------------------------------------------------
// Used by CDmElement to manage the list of attributes it owns
//-----------------------------------------------------------------------------
inline CDmAttribute **CDmAttribute::GetNextAttributeRef()
{
	return &m_pNext;
}


//-----------------------------------------------------------------------------
// helper function for determining which attributes/elements to traverse during copy/find/save/etc.
//-----------------------------------------------------------------------------
inline bool ShouldTraverse( const CDmAttribute *pAttr, TraversalDepth_t depth )
{
	switch ( depth )
	{
	case TD_NONE:
		return false;

	case TD_SHALLOW:
		if ( !pAttr->IsFlagSet( FATTRIB_MUSTCOPY ) )
			return false;
		// fall-through intentional
	case TD_DEEP:
		if ( pAttr->IsFlagSet( FATTRIB_NEVERCOPY ) )
			return false;
		// fall-through intentional
	case TD_ALL:
		return true;
	}

	Assert( 0 );
	return false;
}

#endif // DMATTRIBUTE_H

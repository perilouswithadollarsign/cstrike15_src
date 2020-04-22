//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMXATTRIBUTE_H
#define DMXATTRIBUTE_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmattributetypes.h"
#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlsymbol.h"
#include "tier1/mempool.h"
#include "dmxloader/dmxloader.h"


//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------
class CDmxElement;





#define DECLARE_DMX_ATTRIBUTE_TYPE_INTERNAL( _className, _storageType, _attributeType, _attributeName, _defaultSetStatement ) \
	template< > class CDmAttributeInfo< _className >						\
	{																		\
	private:																\
		enum { ATTRIBUTE_TYPE = _attributeType };							\
		typedef _storageType StorageType_t;									\
		static DmAttributeType_t AttributeType() { return _attributeType; }	\
		static const char *AttributeTypeName() { return _attributeName; }	\
		static void SetDefaultValue( _className& value ) { _defaultSetStatement }	\
		friend class CDmxAttribute;											\
		friend class CDmxElement;											\
	};																		\

#define DECLARE_DMX_ATTRIBUTE_ARRAY_TYPE_INTERNAL( _className, _storageType, _attributeType, _attributeName ) \
	template< > class CDmAttributeInfo< CUtlVector<_className> >				\
	{																			\
	private:																	\
		enum { ATTRIBUTE_TYPE = _attributeType };								\
		typedef _storageType StorageType_t;										\
		static DmAttributeType_t AttributeType() { return _attributeType; }		\
		static const char *AttributeTypeName() { return _attributeName; }		\
		static void SetDefaultValue( CUtlVector< _className >& value ) { value.RemoveAll(); }	\
		friend class CDmxAttribute;												\
		friend class CDmxElement;												\
	};																			\

#define DECLARE_DMX_ATTRIBUTE_TYPE( _className, _attributeType, _attributeName, _defaultSetStatement ) \
	DECLARE_DMX_ATTRIBUTE_TYPE_INTERNAL( _className, _className, _attributeType, _attributeName, _defaultSetStatement )

#define DECLARE_DMX_ATTRIBUTE_ARRAY_TYPE( _className, _attributeType, _attributeName )\
	DECLARE_DMX_ATTRIBUTE_ARRAY_TYPE_INTERNAL( _className, CUtlVector< _className >, _attributeType, _attributeName )



//-----------------------------------------------------------------------------
// Attribute info, modified for use in mod code
//-----------------------------------------------------------------------------
DECLARE_DMX_ATTRIBUTE_TYPE( CDmxElement*,		AT_ELEMENT,				"element",		value = 0; )
DECLARE_DMX_ATTRIBUTE_ARRAY_TYPE( CDmxElement*,	AT_ELEMENT_ARRAY,		"element_array" )

DECLARE_DMX_ATTRIBUTE_TYPE( CUtlString,			AT_STRING,				"string",		value.Set( NULL ); )
DECLARE_DMX_ATTRIBUTE_ARRAY_TYPE( CUtlString,	AT_STRING_ARRAY,		"string_array" )


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDmxAttribute
{
	DECLARE_DMX_ALLOCATOR( );

public:
	// Returns attribute name and type
	DmAttributeType_t GetType() const;
	const char *GetTypeString() const;
	template< class T > bool IsA() const;

	// Returns the name. NOTE: The utlsymbol
	// can be turned into a string by using g_pDataModel->String();
	const char *GetName() const;
	CUtlSymbolLarge GetNameSymbol() const;
	void SetName( const char *pName );

	// Gets values
	template< class T >	const T& GetValue( ) const;
	template< class T > const CUtlVector< T >& GetArray( ) const;
	const char *GetValueString() const;

	// Sets values (+ type)
	template< class T > void SetValue( const T& value );
	void SetValue( const char *pString );
	void SetValue( char *pString );
	void SetValue( const void *pBuffer, size_t nLen );
	void SetValue( const CDmxAttribute *pAttribute );

	// Method to set values in an array (just directly operate on the array)
	// NOTE: This will create a new array of the appropriate type if 
	// the type doesn't match the current type
	template< class T > CUtlVector< T >& GetArrayForEdit();

	// Sets the attribute to its default value based on its type
	void	SetToDefaultValue();

	// Convert to and from string
	void SetValueFromString( const char *pValue );
	const char *GetValueAsString( char *pBuffer, size_t nBufLen ) const;

	// Gets the size of an array, returns 0 if it's not an array type
	int GetArrayCount() const;

	// Read from file
	bool Unserialize( DmAttributeType_t type, CUtlBuffer &buf );
	bool UnserializeElement( DmAttributeType_t type, CUtlBuffer &buf );
	bool Serialize( CUtlBuffer &buf ) const;
	bool SerializeElement( int nIndex, CUtlBuffer &buf ) const;
	bool SerializesOnMultipleLines() const;

	// Returns the size of the variables storing the various attribute types
	static int AttributeDataSize( DmAttributeType_t type );
	// Gets the basic type for a given array attribute type (e.g. AT_INT_ARRAY -> AT_INT)
	static DmAttributeType_t ArrayAttributeBasicType( DmAttributeType_t type );

private:
	CDmxAttribute( const char *pAttributeName );
	CDmxAttribute( CUtlSymbolLarge attributeName );
	~CDmxAttribute();

	// Allocate, free memory for data
	void AllocateDataMemory( DmAttributeType_t type );
	void AllocateDataMemory_AndConstruct( DmAttributeType_t type );
	void FreeDataMemory( );


	// Untyped methods for getting/setting used by unpack
	void SetValue( DmAttributeType_t type, const void *pSrc, int nLen );
	// NOTE: [Get|Set]ArrayValue don't currently support AT_STRING_ARRAY, AT_STRING_VOID or AT_ELEMENT_ARRAY
	void SetArrayValue( DmAttributeType_t type, const void *pSrc, int nDataTypeSize, int nArrayLength, int nSrcStride );
	void GetArrayValue( DmAttributeType_t type, void *pDest, int nDataTypeSize, int nArrayLength, const char *pDefaultString = NULL ) const;
	void SetArrayCount( int nArrayCount );
	const void *GetArrayBase( void ) const;

	// Helper templated methods called from untyped methods (VT is vector datatype, T is basic datatype, VT will be the same as T if the attribute is non-array)
	template < class VT, class T > void ConstructDataMemory( void );
	template < class VT, class T > void DestructDataMemory( void );
	template < class VT, class T > void SetArrayCount( int nArrayCount );
	template < class VT, class T > void GetArrayCount( int &nArrayCount ) const;
	template < class VT, class T > void GetArrayBase( const void * &pBasePtr ) const;
	template < class VT, class T > void SerializesOnMultipleLines( bool &bResult ) const;
	template < class VT, class T > void SerializeType( bool &bSuccess, CUtlBuffer &buf ) const;
	template < class VT, class T > void SerializeTypedElement( bool &bSuccess, int nIndex, CUtlBuffer &buf ) const;
	template < class VT, class T > void UnserializeType( bool &bSuccess, CUtlBuffer &buf );
	template < class VT, class T > void UnserializeTypedElement( bool &bSuccess, CUtlBuffer &buf );
	template < class VT, class T > void SetDefaultValue( void );


	DmAttributeType_t m_Type;
	CUtlSymbolLarge m_Name;
	void *m_pData;

	static CUtlSymbolTableLargeMT s_AttributeNameSymbols;

	friend class CDmxElement;

public: 
	
	static const char *s_pAttributeTypeName[AT_TYPE_COUNT];

};


//-----------------------------------------------------------------------------
// Inline methods 
//-----------------------------------------------------------------------------
inline DmAttributeType_t CDmxAttribute::GetType() const
{
	return m_Type;
}

template< class T > inline bool CDmxAttribute::IsA() const
{
	return GetType() == CDmAttributeInfo< T >::ATTRIBUTE_TYPE;
}

inline CUtlSymbolLarge CDmxAttribute::GetNameSymbol() const
{
	return m_Name;
}


//-----------------------------------------------------------------------------
// Sets a value in the attribute
//-----------------------------------------------------------------------------
template< class T > void CDmxAttribute::SetValue( const T& value )
{
	AllocateDataMemory( CDmAttributeInfo<T>::AttributeType() );
	CopyConstruct( (T*)m_pData, value );
}


//-----------------------------------------------------------------------------
// Returns data in the attribute
//-----------------------------------------------------------------------------
inline const char *CDmxAttribute::GetValueString() const
{
	if ( m_Type == AT_STRING )
		return *(CUtlString*)m_pData;
	return "";
}

template< class T >
inline const T& CDmxAttribute::GetValue( ) const
{
	if ( CDmAttributeInfo<T>::AttributeType() == m_Type )
		return *(T*)m_pData;

	static T defaultValue;
	CDmAttributeInfo<T>::SetDefaultValue( defaultValue );
	return defaultValue;
}

template< class T > 
inline const CUtlVector< T >& CDmxAttribute::GetArray( ) const
{
	if ( CDmAttributeInfo< CUtlVector< T > >::AttributeType() == m_Type )
		return *( CUtlVector< T > *)m_pData;

	static CUtlVector<T> defaultArray;
	return defaultArray;
}

template< class T > 
inline CUtlVector< T >& CDmxAttribute::GetArrayForEdit( )
{
	if ( CDmAttributeInfo< CUtlVector< T > >::AttributeType() == m_Type )
		return *( CUtlVector< T > *)m_pData;

	AllocateDataMemory( CDmAttributeInfo< CUtlVector< T > >::AttributeType() );
	Construct( (CUtlVector<T>*)m_pData );
	return *(CUtlVector< T > *)m_pData;
}

#endif // DMXATTRIBUTE_H

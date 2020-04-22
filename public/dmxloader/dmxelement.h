//====== Copyright � 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMXELEMENT_H
#define DMXELEMENT_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmattributetypes.h"
#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlsymbol.h"
#include "tier1/mempool.h"
#include "tier1/utlsortvector.h"
#include "dmxloader/dmxattribute.h"


//-----------------------------------------------------------------------------
// Sort functor class for attributes 
//-----------------------------------------------------------------------------
class CDmxAttributeLess
{
public:
	bool Less( const CDmxAttribute * pAttribute1, const CDmxAttribute *pAttribute2, void *pContext )
	{
		return (pAttribute1?pAttribute1->GetNameSymbol():CUtlSymbolLarge(UTL_INVAL_SYMBOL_LARGE)) < (pAttribute2?pAttribute2->GetNameSymbol():CUtlSymbolLarge(UTL_INVAL_SYMBOL_LARGE));
	}
};


enum BitfieldType_t
{
	BITFIELD_TYPE_NONE,
	BITFIELD_TYPE_BOOL,
	BITFIELD_TYPE_CHAR,
	BITFIELD_TYPE_UNSIGNED_CHAR,
	BITFIELD_TYPE_BYTE = BITFIELD_TYPE_UNSIGNED_CHAR,
	BITFIELD_TYPE_SHORT,
	BITFIELD_TYPE_UNSIGNED_SHORT,
	BITFIELD_TYPE_INT,
	BITFIELD_TYPE_UNSIGNED_INT,
};

//-----------------------------------------------------------------------------
// Used to unpack elements into a structure. Does not recurse
// Also does not work with arrays.
//-----------------------------------------------------------------------------
struct DmxElementUnpackStructure_t
{
	const char *m_pAttributeName;
	const char *m_pDefaultString;
	DmAttributeType_t m_AttributeType;
	int m_nOffset;
	int m_nSize;				// If size is -1 the AT_STRING datatype is considered to be a UtlString, rather than a char array.
	int m_nBitOffset;			// Default value for this should be -1. A non-negative value indicates that the attribute is a bitfield and
								// m_nSize should be interpreted as number of bits
	BitfieldType_t m_BitfieldType; // the data type of your variable in the bitfield.
	const void *m_pUserData;	// If you want to associate some app-specific data with each field

	// Embedded structure / Baseclass ptr
	const char					*m_pTypeName;
	const DmxElementUnpackStructure_t *m_pSub;

	int m_nArrayLength;			// For fixed-size arrays, this is a positive value (default is 0). For arrays, m_nSize is the size of an array element and m_pDefaultString is the default element value.
};

#define NO_BIT_OFFSET	-1
#define UTL_STRING_SIZE	-1
#define NO_USER_DATA	NULL
#define NO_EMBEDDED_TYPENAME	NULL
#define NO_EMBEDDED_STRUCT_PTR	NULL
#define NOT_A_BITFIELD	BITFIELD_TYPE_NONE
#define NOT_AN_ARRAY 0

#define DECLARE_DMXELEMENT_UNPACK() \
	template <typename T> friend DmxElementUnpackStructure_t *DmxElementUnpackInit(T *);

#define BEGIN_DMXELEMENT_UNPACK( _structName )				\
	template <typename T> DmxElementUnpackStructure_t *DmxElementUnpackInit(T *); \
	template <> DmxElementUnpackStructure_t *DmxElementUnpackInit<_structName>( _structName * ); \
	namespace _structName##_UnpackInit \
	{ \
		static DmxElementUnpackStructure_t *s_pUnpack = DmxElementUnpackInit( (_structName *)NULL ); \
	} \
	\
	template <> DmxElementUnpackStructure_t *DmxElementUnpackInit<_structName>( _structName * ) \
	{ \
		typedef _structName DestStructType_t; \
		static DmxElementUnpackStructure_t unpack[] = \
		{ \

#define DMXELEMENT_UNPACK_FLTX4( _attributeName, _defaultString, _varName )	\
	{ _attributeName, _defaultString, CDmAttributeInfo<float>::AttributeType(), offsetof( DestStructType_t, _varName ), sizeof( fltx4 ), NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, NOT_AN_ARRAY },

#define DMXELEMENT_UNPACK_EMBEDDED( _typeName, _attributeName, _varName, _embeddedUnpackStructure ) \
	{ _attributeName, "", AT_TYPE_COUNT, offsetof( DestStructType_t, _varName ), sizeof( ((DestStructType_t *)0)->_varName), NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, _typeName, _embeddedUnpackStructure, NOT_AN_ARRAY },

#define DMXELEMENT_UNPACK_BASECLASS( _structName, _baseClass ) \
	{ "Baseclass unpack", "", AT_TYPE_COUNT, size_cast< int >((intp) static_cast< _baseClass * >( (_structName*)0)), 0, NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, #_baseClass, DmxElementUnpackInit<_baseClass>( (_baseClass *)0 ), NOT_AN_ARRAY },

#define DMXELEMENT_UNPACK_BASECLASS_NAMESPACE( _namespace, _structName, _baseClass ) \
	{ "Baseclass unpack", "", AT_TYPE_COUNT, size_cast< int >((intp) static_cast< _baseClass * >( (_structName*)0)), 0, NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, #_baseClass, DmxElementUnpackInit##_namespace<_baseClass>( (_baseClass *)0 ), NOT_AN_ARRAY },

#define VGUI_UNPACK_BASEPANEL() \
	DMXELEMENT_UNPACK_BASECLASS_NAMESPACE( vgui, DestStructType_t, DestStructType_t::BaseClass ) \

#define DMXELEMENT_UNPACK_FIELD( _attributeName, _defaultString, _type, _varName )	\
	{ _attributeName, _defaultString, CDmAttributeInfo<_type>::AttributeType(), offsetof( DestStructType_t, _varName ), sizeof( ((DestStructType_t *)0)->_varName), NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, NOT_AN_ARRAY },
// Use for preallocated char array
#define DMXELEMENT_UNPACK_FIELD_STRING( _attributeName, _defaultString, _varName )	\
	{ _attributeName, _defaultString, AT_STRING, offsetof( DestStructType_t, _varName ), sizeof( ((DestStructType_t *)0)->_varName), NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, NOT_AN_ARRAY },
// Use for UtlString datatype
#define DMXELEMENT_UNPACK_FIELD_UTLSTRING( _attributeName, _defaultString, _varName )	\
	{ _attributeName, _defaultString, AT_STRING, offsetof( DestStructType_t, _varName ), UTL_STRING_SIZE, NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, NOT_AN_ARRAY },
#define DMXELEMENT_UNPACK_SHORT( _attributeName, _defaultString, _varName )   \
	{ _attributeName, _defaultString, AT_INT, offsetof( DestStructType_t, _varName ), sizeof( short ), NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, NOT_AN_ARRAY },
#define DMXELEMENT_UNPACK_CHAR( _attributeName, _defaultString, _varName )    \
	{ _attributeName, _defaultString, AT_INT, offsetof( DestStructType_t, _varName ), sizeof( char ), NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, NOT_AN_ARRAY },
#define DMXELEMENT_UNPACK_BITFIELD( _attributeName, _defaultString, _bitfieldType, _varName ) \
	{ _attributeName, _defaultString, CDmAttributeInfo<int>::AttributeType(), DestStructType_t::Get##_varName##ByteOffset(), DestStructType_t::Get##_varName##BitCount(), DestStructType_t::Get##_varName##BitOffset(), _bitfieldType, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, NOT_AN_ARRAY },

// NOTE: DMXELEMENT_UNPACK_FIELD_ARRAY is for fixed-size arrays, not pointers or CUtlVectors (TODO: doesn't work for strings or bitfields yet!)
#define DMXELEMENT_UNPACK_FIELD_ARRAY( _attributeName, _defaultString, _type, _varName )	\
	{ _attributeName, _defaultString, CDmAttributeInfo< CUtlVector< _type > >::AttributeType(), offsetof( DestStructType_t, _varName ), sizeof( ((DestStructType_t *)0)->_varName[0]), NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, ARRAYSIZE( ((DestStructType_t *)0)->_varName ) },


#define DMXELEMENT_UNPACK_FIELD_USERDATA( _attributeName, _defaultString, _type, _varName, _userData )	\
	{ _attributeName, _defaultString, CDmAttributeInfo<_type>::AttributeType(), offsetof( DestStructType_t, _varName ), sizeof( ((DestStructType_t *)0)->_varName), NO_BIT_OFFSET, NOT_A_BITFIELD, _userData, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, NOT_AN_ARRAY },
#define DMXELEMENT_UNPACK_FIELD_STRING_USERDATA( _attributeName, _defaultString, _varName, _userData )	\
	{ _attributeName, _defaultString, AT_STRING, offsetof( DestStructType_t, _varName ), sizeof( ((DestStructType_t *)0)->_varName), NO_BIT_OFFSET, NOT_A_BITFIELD, _userData, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, NOT_AN_ARRAY },
#define DMXELEMENT_UNPACK_FIELD_UTLSTRING_USERDATA( _attributeName, _defaultString, _varName, _userData )	\
	{ _attributeName, _defaultString, AT_STRING, offsetof( DestStructType_t, _varName ), UTL_STRING_SIZE, NO_BIT_OFFSET, NOT_A_BITFIELD, _userData, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR, NOT_AN_ARRAY },

#define END_DMXELEMENT_UNPACK( _structName, _varName )			\
			{ NULL, NULL, AT_UNKNOWN, 0, 0, NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR }				\
		};														\
		return unpack;											\
	}															\
	DmxElementUnpackStructure_t *_varName = _structName##_UnpackInit::s_pUnpack;

#define END_DMXELEMENT_UNPACK_TEMPLATE( _structName, _varName )			\
			{ NULL, NULL, AT_UNKNOWN, 0, 0, NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR }				\
		};														\
		return unpack;											\
	}															\
	 template<> DmxElementUnpackStructure_t *_varName = _structName##_UnpackInit::s_pUnpack;

// Macros for when your class is inside a namespace.
#define BEGIN_DMXELEMENT_NAMESPACE_UNPACK( _nameSpace, _structName )				\
	template <typename T> DmxElementUnpackStructure_t *DmxElementUnpackInit(T *); \
	template <> DmxElementUnpackStructure_t *_nameSpace::DmxElementUnpackInit<_nameSpace::_structName>( _nameSpace::_structName * ); \
	namespace _structName##_UnpackInit \
	{ \
		static DmxElementUnpackStructure_t *s_pUnpack = _nameSpace::DmxElementUnpackInit( (_nameSpace::_structName *)NULL ); \
	} \
	\
	template <> DmxElementUnpackStructure_t *_nameSpace::DmxElementUnpackInit<_nameSpace::_structName>( _nameSpace::_structName * ) \
	{ \
		typedef _nameSpace::_structName DestStructType_t; \
		static DmxElementUnpackStructure_t unpack[] = \
	{ \

//#define DECLARE_DMXELEMENT_UNPACK_NAMESPACE( _namespace ) \
//	template <typename T> friend DmxElementUnpackStructure_t *DmxElementUnpackInit##_namespace(T *);

// Adds serialization unpack structure and unpack func to your class.
#if defined( _PS3 ) && defined( __GCC__ )
#define DECLARE_DMXELEMENT_UNPACK_NAMESPACE( _namespace ) \
	template <typename T> friend DmxElementUnpackStructure_t *_namespace::DmxElementUnpackInit##_namespace(T *); \
	private: \
		static DmxElementUnpackStructure_t *s_pUnpackParams; \
	public:	 \
		virtual const DmxElementUnpackStructure_t* GetUnpackStructure() const { return s_pUnpackParams; }
#else
#define DECLARE_DMXELEMENT_UNPACK_NAMESPACE( _namespace ) \
	template <typename T> friend DmxElementUnpackStructure_t *DmxElementUnpackInit##_namespace(T *); \
	private: \
		static DmxElementUnpackStructure_t *s_pUnpackParams; \
	public:	 \
		virtual const DmxElementUnpackStructure_t* GetUnpackStructure() const { return s_pUnpackParams; }
#endif

// Use when your panel class is derived from another baseclass
#define BEGIN_DMXELEMENT_UNPACK_NAMESPACE_SIMPLE( _namespace, _structName ) \
	BEGIN_DMXELEMENT_UNPACK_NAMESPACE( _namespace, _structName )  \
	VGUI_UNPACK_BASEPANEL() \

// Use when  your panel class has no base class
#define BEGIN_DMXELEMENT_UNPACK_NAMESPACE_SIMPLE_NO_BASE( _namespace, _structName ) \
	BEGIN_DMXELEMENT_UNPACK_NAMESPACE( _namespace, _structName )  \

#define BEGIN_DMXELEMENT_UNPACK_NAMESPACE( _namespace, _structName )                      \
	namespace _namespace { \
	template <typename T> DmxElementUnpackStructure_t *DmxElementUnpackInit##_namespace(T *); \
	template <> DmxElementUnpackStructure_t *DmxElementUnpackInit##_namespace<_structName>( _namespace::_structName * ); \
	namespace _namespace##_structName##_UnpackInit \
	{ \
		static DmxElementUnpackStructure_t *s_pUnpack = _namespace::DmxElementUnpackInit##_namespace( (_namespace::_structName *)NULL ); \
	} \
	\
	template <> DmxElementUnpackStructure_t *DmxElementUnpackInit##_namespace<_structName>( _namespace::_structName * ) \
	{ \
		typedef _structName DestStructType_t;			\
		static DmxElementUnpackStructure_t unpack[] =	\
	{ \

// Use to end BEGIN_DMXELEMENT_UNPACK_NAMESPACE* macros
#define END_DMXELEMENT_UNPACK_NAMESPACE( _namespace, _structName, _varName )			\
			{ NULL, NULL, AT_UNKNOWN, 0, 0, NO_BIT_OFFSET, NOT_A_BITFIELD, NO_USER_DATA, NO_EMBEDDED_TYPENAME, NO_EMBEDDED_STRUCT_PTR }				\
		};													\
		return unpack;										\
	}														\
	DmxElementUnpackStructure_t *_structName::_varName = _namespace##_structName##_UnpackInit::s_pUnpack; \
	} \

// Return a variable of type 'T' with all bits set
template <typename T> inline T GetAllOnesNumber()
{
	T temp = 0;	/* Temp variable to avoid gcc warnings */
	return (T)~temp;
}

// Special case of GetAllOnesVar for bool to avoid
// warnings from VC++.
template <> inline bool GetAllOnesNumber<bool>()
{
	return true;
}

#define DECLARE_DMXELEMENT_BITFIELD( _fieldName, _type, _structName )								\
	class CBitFieldInfo_##_fieldName 											\
	{																			\
	public:																		\
		CBitFieldInfo_##_fieldName ()											\
		{																		\
			const int nSize = ( sizeof(_structName) + 3 ) & ~0x3;				\
			unsigned char pBuf[nSize] = {};										\
			(( _structName * )pBuf)->_fieldName = GetAllOnesNumber<_type>();	\
			_type *pTest = (_type *)pBuf;										\
			for ( int i = 0; i < sizeof(_structName); ++i )						\
			{																	\
				if ( pTest[i] == 0 )											\
					continue;													\
																				\
				for ( int j = 0; j < 8*sizeof(_type); ++j )						\
				{																\
					unsigned int temp = ((unsigned int)pTest[i]) & ( 1 << j ) ;	\
					if ( temp == 0 )											\
						continue;												\
																				\
					m_nByteOffset = i*sizeof(_type) + j / 8;					\
					m_nBitOffset = j & 0x7;										\
																				\
					int k;														\
					for ( k = j+1; k < 8*sizeof(_type); ++k )					\
					{															\
						unsigned int temp = ((unsigned int)pTest[i]) & ( 1 << k ) ;		\
						if ( temp != 0 )										\
							continue;											\
						break;													\
					}															\
					m_nBitCount = k - j;										\
					break;														\
				}																\
				break;															\
			}																	\
		}																		\
																				\
		int GetByteOffset() const												\
		{																		\
			return m_nByteOffset;												\
		}																		\
																				\
		int GetBitCount() const													\
		{																		\
			return m_nBitCount;													\
		}																		\
																				\
		int GetBitOffset() const												\
		{																		\
			return m_nBitOffset;												\
		}																		\
																				\
	private:																	\
		int m_nByteOffset;														\
		int m_nBitCount;														\
		int m_nBitOffset;													    \
	};																			\
																				\
	static int Get##_fieldName##BitCount()										\
	{																			\
		CBitFieldInfo_##_fieldName info;										\
		return info.GetBitCount();												\
	}																			\
																				\
	static int Get##_fieldName##ByteOffset()									\
	{																			\
		CBitFieldInfo_##_fieldName info;										\
		return info.GetByteOffset();											\
	}																			\
																				\
	static int Get##_fieldName##BitOffset()										\
	{																			\
		CBitFieldInfo_##_fieldName info;										\
		return info.GetBitOffset();												\
	}																			\
	friend class CBitFieldInfo_##_fieldName; 									\


// A bit of a hack, but we don't have access to 
extern CUtlSymbolTableLargeMT g_DmxAttributeStrings;

//-----------------------------------------------------------------------------
// Element used to read dmx files from mod code. Similar to keyvalues.
//-----------------------------------------------------------------------------
class CDmxElement
{
	DECLARE_DMX_ALLOCATOR( );

public:
	bool				HasAttribute( const char *pAttributeName ) const;
	CDmxAttribute		*GetAttribute( const char *pAttributeName );
	const CDmxAttribute *GetAttribute( const char *pAttributeName ) const;
	int					AttributeCount() const;
	CDmxAttribute		*GetAttribute( int nIndex );
	const CDmxAttribute *GetAttribute( int nIndex ) const;
	CUtlSymbolLarge			GetType() const;
	const char*			GetTypeString() const;
	const char*			GetName() const;
	const DmObjectId_t &GetId() const;

	// Add+remove+rename can only occur during lock
	// NOTE: AddAttribute will find or add; returning an existing attribute if
	// one with the appropriate name exists
	void				LockForChanges( bool bLock );
	CDmxAttribute		*AddAttribute( const char *pAttributeName );
	void				RemoveAttribute( const char *pAttributeName );
	void				RemoveAttributeByPtr( CDmxAttribute *pAttribute );
	void				RemoveAllAttributes();
	void				RenameAttribute( const char *pAttributeName, const char *pNewName );

	// Simple methods to read attributes
	const char *GetValueString( const char *pAttributeName ) const;
	template< class T > const T& GetValue( const char *pAttributeName ) const;
	template< class T > const T& GetValue( const char *pAttributeName, const T& defaultValue ) const;

	template< class T > const CUtlVector<T>& GetArray( const char *pAttributeName ) const;
	template< class T > const CUtlVector<T>& GetArray( const char *pAttributeName, const CUtlVector<T>& defaultValue ) const;

	// Set methods
	void SetName( const char *pName );
	CDmxAttribute* SetValue( const char *pAttributeName, const char *pString );
	CDmxAttribute* SetValue( const char *pAttributeName, void *pBuffer, int nLen );
	template< class T > CDmxAttribute* SetValue( const char *pAttributeName, const T& value );

	// Method to unpack data into a structure
	void UnpackIntoStructure( void *pData, const DmxElementUnpackStructure_t *pUnpack ) const;

	// Creates attributes based on the unpack structure
	void AddAttributesFromStructure( const void *pData, const DmxElementUnpackStructure_t *pUnpack );

private:
	typedef CUtlSortVector< CDmxAttribute*, CDmxAttributeLess > AttributeList_t;

	CDmxElement( const char *pType );
	~CDmxElement();

	// Removes all elements recursively
	void RemoveAllElementsRecursive();

	// Adds elements to delete to the deletion list
	void AddElementsToDelete( CUtlVector< CDmxElement * >& elementsToDelete );

	// Sorts the vector when a change has occurred
	void Resort( ) const;

	// Finds an attribute by name
	int FindAttribute( const char *pAttributeName ) const;
	int FindAttribute( CUtlSymbolLarge attributeName ) const;

	// Sets the object id
	void SetId( const DmObjectId_t &id );

	// Are we locked?
	bool IsLocked() const;

	template <typename T> void UnpackBitfield( T *pDest2, const DmxElementUnpackStructure_t *pUnpack, const CDmxAttribute *pAttribute ) const;

	AttributeList_t m_Attributes;
	DmObjectId_t m_Id;	// We need this strictly because we support serialization
	CUtlSymbolLarge m_Type;
	char m_nLockCount;
	mutable bool m_bResortNeeded : 1;
	bool m_bIsMarkedForDeletion : 1;

	static CUtlSymbolTableLargeMT s_TypeSymbols;

	friend class CDmxSerializer;
	friend class CDmxSerializerKeyValues2;
	friend void CleanupDMX( CDmxElement* pElement );
	friend CDmxElement* CreateDmxElement( const char *pType );
};


//-----------------------------------------------------------------------------
// inline methods
//-----------------------------------------------------------------------------

// Are we locked?
inline bool CDmxElement::IsLocked() const
{
	return m_nLockCount > 0;
}

inline const char *CDmxElement::GetValueString( const char *pAttributeName ) const
{
	const CDmxAttribute* pAttribute = GetAttribute( pAttributeName );
	if ( pAttribute )
		return pAttribute->GetValueString();
	return "";
}

template< class T > 
inline const T& CDmxElement::GetValue( const char *pAttributeName ) const
{
	const CDmxAttribute* pAttribute = GetAttribute( pAttributeName );
	if ( pAttribute )
		return pAttribute->GetValue<T>();

	static T defaultValue;
	CDmAttributeInfo<T>::SetDefaultValue( defaultValue );
	return defaultValue;
}

template< class T >
inline const T& CDmxElement::GetValue( const char *pAttributeName, const T& defaultValue ) const
{
	const CDmxAttribute* pAttribute = GetAttribute( pAttributeName );
	if ( pAttribute )
		return pAttribute->GetValue<T>();
	return defaultValue;
}

template< class T > 
inline const CUtlVector<T>& CDmxElement::GetArray( const char *pAttributeName ) const
{
	const CDmxAttribute* pAttribute = GetAttribute( pAttributeName );
	if ( pAttribute )
		return pAttribute->GetArray<T>();

	static CUtlVector<T> defaultValue;
	return defaultValue;
}

template< class T > 
inline const CUtlVector<T>& CDmxElement::GetArray( const char *pAttributeName, const CUtlVector<T>& defaultValue ) const
{
	const CDmxAttribute* pAttribute = GetAttribute( pAttributeName );
	if ( pAttribute )
		return pAttribute->GetArray<T>();
	return defaultValue;
}


//-----------------------------------------------------------------------------
// Creates a dmx element
//-----------------------------------------------------------------------------
CDmxElement* CreateDmxElement( const char *pType );


//-----------------------------------------------------------------------------
// Helper class to lock elements for changes
//-----------------------------------------------------------------------------
class CDmxElementModifyScope
{
public:
	CDmxElementModifyScope( CDmxElement *pElement ) : m_pElement( pElement )
	{
		m_pElement->LockForChanges( true );
	}
	~CDmxElementModifyScope()
	{
		Release();
	}
	void Release()
	{
		if ( m_pElement )
		{
			m_pElement->LockForChanges( false );
			m_pElement = NULL;
		}
	}
private:
	CDmxElement *m_pElement;
};


//-----------------------------------------------------------------------------
// Set methods
//-----------------------------------------------------------------------------
inline CDmxAttribute* CDmxElement::SetValue( const char *pAttributeName, const char *pString )
{
	CDmxElementModifyScope modify( this );
	CDmxAttribute *pAttribute = AddAttribute( pAttributeName );
	pAttribute->SetValue( pString );
	return pAttribute;
}

inline CDmxAttribute* CDmxElement::SetValue( const char *pAttributeName, void *pBuffer, int nLen )
{
	CDmxElementModifyScope modify( this );
	CDmxAttribute *pAttribute = AddAttribute( pAttributeName );
	pAttribute->SetValue( pBuffer, nLen );
	return pAttribute;
}

template< class T > 
inline CDmxAttribute* CDmxElement::SetValue( const char *pAttributeName, const T& value )
{
	CDmxElementModifyScope modify( this );
	CDmxAttribute *pAttribute = AddAttribute( pAttributeName );
	pAttribute->SetValue( value );
	return pAttribute;
}


#endif // DMXELEMENT_H

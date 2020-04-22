//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dmxloader/dmxattribute.h"
#include "tier1/utlbufferutil.h"
#include "tier1/uniqueid.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------
CUtlSymbolTableLargeMT CDmxAttribute::s_AttributeNameSymbols;


//-----------------------------------------------------------------------------
// Attribute size
//-----------------------------------------------------------------------------
static size_t s_pAttributeSize[AT_TYPE_COUNT] = 
{
	0,							// AT_UNKNOWN,
	sizeof(CDmxElement*),		// AT_ELEMENT,
	sizeof(int),				// AT_INT,
	sizeof(float),				// AT_FLOAT,
	sizeof(bool),				// AT_BOOL,
	sizeof(CUtlString),			// AT_STRING,
	sizeof(CUtlBinaryBlock),	// AT_VOID,
	sizeof(DmeTime_t),			// AT_TIME,
	sizeof(Color),				// AT_COLOR,
	sizeof(Vector2D),			// AT_VECTOR2,
	sizeof(Vector),				// AT_VECTOR3,
	sizeof(Vector4D),			// AT_VECTOR4,
	sizeof(QAngle),				// AT_QANGLE,
	sizeof(Quaternion),			// AT_QUATERNION,
	sizeof(VMatrix),			// AT_VMATRIX,
	sizeof(CUtlVector<CDmxElement*>),		// AT_ELEMENT_ARRAY,
	sizeof(CUtlVector<int>),				// AT_INT_ARRAY,
	sizeof(CUtlVector<float>),				// AT_FLOAT_ARRAY,
	sizeof(CUtlVector<bool>),				// AT_BOOL_ARRAY,
	sizeof(CUtlVector<CUtlString>),			// AT_STRING_ARRAY,
	sizeof(CUtlVector<CUtlBinaryBlock>),	// AT_VOID_ARRAY,
	sizeof(CUtlVector<DmeTime_t>),			// AT_TIME_ARRAY,
	sizeof(CUtlVector<Color>),				// AT_COLOR_ARRAY,
	sizeof(CUtlVector<Vector2D>),			// AT_VECTOR2_ARRAY,
	sizeof(CUtlVector<Vector>),				// AT_VECTOR3_ARRAY,
	sizeof(CUtlVector<Vector4D>),			// AT_VECTOR4_ARRAY,
	sizeof(CUtlVector<QAngle>),				// AT_QANGLE_ARRAY,
	sizeof(CUtlVector<Quaternion>),			// AT_QUATERNION_ARRAY,
	sizeof(CUtlVector<VMatrix>),			// AT_VMATRIX_ARRAY,
};


//-----------------------------------------------------------------------------
// make sure that the attribute data type sizes are what we think they are to choose the right allocator
//-----------------------------------------------------------------------------
struct CSizeTest
{
	CSizeTest()
	{
		// test internal value attribute sizes
		COMPILE_TIME_ASSERT( sizeof( int )			== 4 );
		COMPILE_TIME_ASSERT( sizeof( float )		== 4 );
		COMPILE_TIME_ASSERT( sizeof( bool )			<= 4 );
		COMPILE_TIME_ASSERT( sizeof( Color )		== 4 );
		COMPILE_TIME_ASSERT( sizeof( DmeTime_t )	== 4 );
		COMPILE_TIME_ASSERT( sizeof( Vector2D )		== 8 );
		COMPILE_TIME_ASSERT( sizeof( Vector )		== 12 );
		COMPILE_TIME_ASSERT( sizeof( Vector4D )		== 16 );
		COMPILE_TIME_ASSERT( sizeof( QAngle )		== 12 );
		COMPILE_TIME_ASSERT( sizeof( Quaternion )	== 16 );
		COMPILE_TIME_ASSERT( sizeof( VMatrix )		== 64 );
#if !defined( PLATFORM_64BITS )
		COMPILE_TIME_ASSERT( sizeof( CUtlString )	== 16 );
		COMPILE_TIME_ASSERT( sizeof( CUtlBinaryBlock ) == 16 );
#endif
	};
};
static CSizeTest g_sizeTest;


//-----------------------------------------------------------------------------
// Returns attribute name for type
//-----------------------------------------------------------------------------
const char *CDmxAttribute::s_pAttributeTypeName[AT_TYPE_COUNT] = 
{
	"unknown", // AT_UNKNOWN
	CDmAttributeInfo<DmElementHandle_t>::AttributeTypeName(), // AT_ELEMENT,
	CDmAttributeInfo<int>::AttributeTypeName(), // AT_INT,
	CDmAttributeInfo<float>::AttributeTypeName(), // AT_FLOAT,
	CDmAttributeInfo<bool>::AttributeTypeName(), // AT_BOOL,
	CDmAttributeInfo<CUtlString>::AttributeTypeName(), // AT_STRING,
	CDmAttributeInfo<CUtlBinaryBlock>::AttributeTypeName(), // AT_VOID,
	CDmAttributeInfo<DmeTime_t>::AttributeTypeName(), // AT_TIME,
	CDmAttributeInfo<Color>::AttributeTypeName(), // AT_COLOR,
	CDmAttributeInfo<Vector2D>::AttributeTypeName(), // AT_VECTOR2,
	CDmAttributeInfo<Vector>::AttributeTypeName(), // AT_VECTOR3,
	CDmAttributeInfo<Vector4D>::AttributeTypeName(), // AT_VECTOR4,
	CDmAttributeInfo<QAngle>::AttributeTypeName(), // AT_QANGLE,
	CDmAttributeInfo<Quaternion>::AttributeTypeName(), // AT_QUATERNION,
	CDmAttributeInfo<VMatrix>::AttributeTypeName(), // AT_VMATRIX,
	CDmAttributeInfo< CUtlVector< DmElementHandle_t > >::AttributeTypeName(), // AT_ELEMENT_ARRAY,
	CDmAttributeInfo< CUtlVector< int > >::AttributeTypeName(), // AT_INT_ARRAY,
	CDmAttributeInfo< CUtlVector< float > >::AttributeTypeName(), // AT_FLOAT_ARRAY,
	CDmAttributeInfo< CUtlVector< bool > >::AttributeTypeName(), // AT_BOOL_ARRAY,
	CDmAttributeInfo< CUtlVector< CUtlString > >::AttributeTypeName(), // AT_STRING_ARRAY,
	CDmAttributeInfo< CUtlVector< CUtlBinaryBlock > >::AttributeTypeName(), // AT_VOID_ARRAY,
	CDmAttributeInfo< CUtlVector< DmeTime_t > >::AttributeTypeName(), // AT_TIME_ARRAY,
	CDmAttributeInfo< CUtlVector< Color > >::AttributeTypeName(), // AT_COLOR_ARRAY,
	CDmAttributeInfo< CUtlVector< Vector2D > >::AttributeTypeName(), // AT_VECTOR2_ARRAY,
	CDmAttributeInfo< CUtlVector< Vector > >::AttributeTypeName(), // AT_VECTOR3_ARRAY,
	CDmAttributeInfo< CUtlVector< Vector4D > >::AttributeTypeName(), // AT_VECTOR4_ARRAY,
	CDmAttributeInfo< CUtlVector< QAngle > >::AttributeTypeName(), // AT_QANGLE_ARRAY,
	CDmAttributeInfo< CUtlVector< Quaternion > >::AttributeTypeName(), // AT_QUATERNION_ARRAY,
	CDmAttributeInfo< CUtlVector< VMatrix > >::AttributeTypeName(), // AT_VMATRIX_ARRAY,
};


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
CDmxAttribute::CDmxAttribute( const char *pAttributeName )
{
	m_Name = s_AttributeNameSymbols.AddString( pAttributeName );
	m_Type = AT_UNKNOWN;
	m_pData = NULL;
}

CDmxAttribute::CDmxAttribute( CUtlSymbolLarge attributeName )
{
	m_Name = attributeName;
	m_Type = AT_UNKNOWN;
	m_pData = NULL;
}

CDmxAttribute::~CDmxAttribute()
{
	FreeDataMemory();
}


//-----------------------------------------------------------------------------
// Returns the size of the variables storing the various attribute types
//-----------------------------------------------------------------------------
int CDmxAttribute::AttributeDataSize( DmAttributeType_t type )
{
	return s_pAttributeSize[type];
}

//-----------------------------------------------------------------------------
// Gets the basic type for a given array attribute type
//-----------------------------------------------------------------------------
DmAttributeType_t CDmxAttribute::ArrayAttributeBasicType( DmAttributeType_t type )
{
	COMPILE_TIME_ASSERT( ( AT_FIRST_ARRAY_TYPE - AT_FIRST_VALUE_TYPE ) == ( AT_TYPE_COUNT - AT_FIRST_ARRAY_TYPE ) );
	if ( IsArrayType( type ) )
		type = (DmAttributeType_t)( type - ( AT_FIRST_ARRAY_TYPE - AT_FIRST_VALUE_TYPE ) ); // Array -> array element
	return type;
}


//-----------------------------------------------------------------------------
// Macros to tersify operations on attribute data of any type
//-----------------------------------------------------------------------------
#define NON_ARRAY_TYPE_CASES(	_func_, _params_, _errCase_ )												\
	case AT_INT:				_func_< int							,	int				>_params_;	break;	\
	case AT_FLOAT:				_func_< float						,	float			>_params_;	break;	\
	case AT_BOOL:				_func_< bool						,	bool			>_params_;	break;	\
	case AT_STRING:				_func_< CUtlString					,	CUtlString		>_params_;	break;	\
	case AT_VOID:				_func_< CUtlBinaryBlock				,	CUtlBinaryBlock	>_params_;	break;	\
	case AT_TIME:				_func_< DmeTime_t					,	DmeTime_t		>_params_;	break;	\
	case AT_COLOR:				_func_< Color						,	Color			>_params_;	break;	\
	case AT_VECTOR2:			_func_< Vector2D					,	Vector2D		>_params_;	break;	\
	case AT_VECTOR3:			_func_< Vector						,	Vector			>_params_;	break;	\
	case AT_VECTOR4:			_func_< Vector4D					,	Vector4D		>_params_;	break;	\
	case AT_QANGLE:				_func_< QAngle						,	QAngle			>_params_;	break;	\
	case AT_QUATERNION:			_func_< Quaternion					,	Quaternion		>_params_;	break;	\
	case AT_VMATRIX:			_func_< VMatrix						,	VMatrix			>_params_;	break;

#define ARRAY_TYPE_CASES(		_func_, _params_, _errCase_ )												\
	case AT_INT_ARRAY:			_func_< CUtlVector<int				>,	int				>_params_;	break;	\
	case AT_FLOAT_ARRAY:		_func_< CUtlVector<float			>,	float			>_params_;	break;	\
	case AT_BOOL_ARRAY:			_func_< CUtlVector<bool				>,	bool			>_params_;	break;	\
	case AT_STRING_ARRAY:		_func_< CUtlVector<CUtlString		>,	CUtlString		>_params_;	break;	\
	case AT_VOID_ARRAY:			_func_< CUtlVector<CUtlBinaryBlock	>,	CUtlBinaryBlock	>_params_;	break;	\
	case AT_TIME_ARRAY:			_func_< CUtlVector<DmeTime_t		>,	DmeTime_t		>_params_;	break;	\
	case AT_COLOR_ARRAY:		_func_< CUtlVector<Color			>,	Color			>_params_;	break;	\
	case AT_VECTOR2_ARRAY:		_func_< CUtlVector<Vector2D			>,	Vector2D		>_params_;	break;	\
	case AT_VECTOR3_ARRAY:		_func_< CUtlVector<Vector			>,	Vector			>_params_;	break;	\
	case AT_VECTOR4_ARRAY:		_func_< CUtlVector<Vector4D			>,	Vector4D		>_params_;	break;	\
	case AT_QANGLE_ARRAY:		_func_< CUtlVector<QAngle			>,	QAngle			>_params_;	break;	\
	case AT_QUATERNION_ARRAY:	_func_< CUtlVector<Quaternion		>,	Quaternion		>_params_;	break;	\
	case AT_VMATRIX_ARRAY:		_func_< CUtlVector<VMatrix			>,	VMatrix			>_params_;	break;

#define NON_ARRAY_ELEMENT_CASE(	_func_, _params_, _errCase_ )												\
	case AT_ELEMENT:			_func_< CDmxElement*				,	CDmxElement*	>_params_;	break;
	
#define ARRAY_ELEMENT_CASE(		_func_, _params_, _errCase_ )												\
	case AT_ELEMENT_ARRAY:		_func_< CUtlVector<CDmxElement*		>,	CDmxElement*	>_params_;	break;

#define CALL_ARRAY_TYPE_TEMPLATIZED_FUNCTION_NOELEMENTS( _func_, _params_, _errCase_ )						\
	switch( m_Type )																						\
	{																										\
		ARRAY_TYPE_CASES(		_func_, _params_, _errCase_ )												\
		default:	_errCase_;																				\
	}

#define CALL_ARRAY_TYPE_TEMPLATIZED_FUNCTION( _func_, _params_, _errCase_ )									\
	switch( m_Type )																						\
	{																										\
		ARRAY_TYPE_CASES(		_func_, _params_, _errCase_ )												\
		ARRAY_ELEMENT_CASE(	_func_, _params_, _errCase_ )													\
		default:	_errCase_;																				\
	}

#define CALL_TYPE_TEMPLATIZED_FUNCTION_NOELEMENTS( _func_, _params_, _errCase_ )							\
	switch( m_Type )																						\
	{																										\
		NON_ARRAY_TYPE_CASES(	_func_, _params_, _errCase_ )												\
		ARRAY_TYPE_CASES(		_func_, _params_, _errCase_ )												\
		default:	_errCase_;																				\
	}

#define CALL_TYPE_TEMPLATIZED_FUNCTION( _func_, _params_, _errCase_ )										\
	switch( m_Type )																						\
	{																										\
		NON_ARRAY_TYPE_CASES(	_func_, _params_, _errCase_ )												\
		NON_ARRAY_ELEMENT_CASE(_func_, _params_, _errCase_ )												\
		ARRAY_TYPE_CASES(		_func_, _params_, _errCase_ )												\
		ARRAY_ELEMENT_CASE(	_func_, _params_, _errCase_ )													\
		default:	_errCase_;																				\
	}

//-----------------------------------------------------------------------------
// Allocate, free memory for data
//-----------------------------------------------------------------------------
void CDmxAttribute::AllocateDataMemory( DmAttributeType_t type )
{
	FreeDataMemory();

	m_Type = type;
	m_pData = DMXAlloc( s_pAttributeSize[m_Type] );
}

template < class VT, class T >
void CDmxAttribute::ConstructDataMemory( void )
{
	Construct( (VT *)m_pData );
}
void CDmxAttribute::AllocateDataMemory_AndConstruct( DmAttributeType_t type )
{
	AllocateDataMemory( type );
	Assert( m_pData != NULL );

	// Process array and non-array types, including elements
	CALL_TYPE_TEMPLATIZED_FUNCTION( ConstructDataMemory, (), );
}

template < class VT, class T >
void CDmxAttribute::DestructDataMemory( void )
{
	Destruct( (VT *)m_pData );
}
void CDmxAttribute::FreeDataMemory()
{
	if ( m_Type != AT_UNKNOWN )
	{
		Assert( m_pData != NULL );
		// Process array and non-array types, including elements
		CALL_TYPE_TEMPLATIZED_FUNCTION( DestructDataMemory, (), );
		m_Type = AT_UNKNOWN;
	}
}


//-----------------------------------------------------------------------------
// Returns attribute type string
//-----------------------------------------------------------------------------
inline const char* CDmxAttribute::GetTypeString() const
{
	return s_pAttributeTypeName[ m_Type ];
}


//-----------------------------------------------------------------------------
// Returns attribute name
//-----------------------------------------------------------------------------
const char *CDmxAttribute::GetName() const
{
	return m_Name.String();
}


//-----------------------------------------------------------------------------
// Gets the size of an array, returns 0 if it's not an array type
//-----------------------------------------------------------------------------
template < class VT, class T >
void CDmxAttribute::GetArrayCount( int &nCount ) const
{
	nCount = ((VT *)m_pData)->Count();
}
int CDmxAttribute::GetArrayCount() const
{
	int nCount = 0;
	// Process array types only, including elements
	if ( IsArrayType( m_Type ) && m_pData )
		CALL_ARRAY_TYPE_TEMPLATIZED_FUNCTION( GetArrayCount, (nCount), );
	return nCount;
}

//-----------------------------------------------------------------------------
// Sets the size of an array, non-destructively
//-----------------------------------------------------------------------------
template < class VT, class T >
void CDmxAttribute::SetArrayCount( int nArrayCount )
{
	((VT *)m_pData)->SetCountNonDestructively( nArrayCount );
}
void CDmxAttribute::SetArrayCount( int nArrayCount )
{
	// Process array types only, including elements
	CALL_ARRAY_TYPE_TEMPLATIZED_FUNCTION( SetArrayCount, (nArrayCount), Assert(0) );
}

//-----------------------------------------------------------------------------
// Gets the base data pointer of an array
//-----------------------------------------------------------------------------
template < class VT, class T >
void CDmxAttribute::GetArrayBase( const void * &pBasePtr ) const
{
	pBasePtr = ((VT *)m_pData)->Base();
}
const void *CDmxAttribute::GetArrayBase( void ) const
{
	const void *pBasePtr = NULL;
	// Process array types only, including elements
	CALL_ARRAY_TYPE_TEMPLATIZED_FUNCTION( GetArrayBase, (pBasePtr), Assert(0) );
	return pBasePtr;
}

//-----------------------------------------------------------------------------
// Gets whether a given data type serializes to multiple lines
//-----------------------------------------------------------------------------
template < class VT, class T >
void CDmxAttribute::SerializesOnMultipleLines( bool &bResult ) const
{
	bResult = ::SerializesOnMultipleLines<VT>();
}
bool CDmxAttribute::SerializesOnMultipleLines() const
{
	bool bResult = false;
	// Process array and non-array types, including elements
	CALL_TYPE_TEMPLATIZED_FUNCTION( SerializesOnMultipleLines, (bResult), );
	return bResult;
}


//-----------------------------------------------------------------------------
// Write to file
//-----------------------------------------------------------------------------
template < class VT, class T >
void CDmxAttribute::SerializeType( bool &bSuccess, CUtlBuffer &buf ) const
{
	if ( m_pData )
	{
		bSuccess = ::Serialize( buf, *(VT *)m_pData );
	}
	else
	{
		VT temp;
		CDmAttributeInfo< VT >::SetDefaultValue( temp );
		bSuccess = ::Serialize( buf, temp );
	}
}

bool CDmxAttribute::Serialize( CUtlBuffer &buf ) const
{
	bool bSuccess = false;
	// Process array and non-array types, excluding elements
	CALL_TYPE_TEMPLATIZED_FUNCTION_NOELEMENTS( SerializeType, (bSuccess, buf), AssertMsg( 0, "Cannot serialize elements or element arrays!\n" ) );
	return bSuccess;
}


//-----------------------------------------------------------------------------
// Serialize a single element in an array attribute
//-----------------------------------------------------------------------------
template < class VT, class T >
void CDmxAttribute::SerializeTypedElement( bool &bSuccess, int nIndex, CUtlBuffer &buf ) const
{
	if ( m_pData )
	{
		const VT &array = *(VT *)m_pData;
		bSuccess = ::Serialize( buf, array[nIndex] );
	}
	else
	{
		T temp;
		CDmAttributeInfo<T>::SetDefaultValue( temp );
		bSuccess = ::Serialize( buf, temp );
	}
}
bool CDmxAttribute::SerializeElement( int nIndex, CUtlBuffer &buf ) const
{
	if ( !IsArrayType( m_Type ) )
		return false;

	bool bSuccess = false;
	// Process array types only, excluding elements
	CALL_ARRAY_TYPE_TEMPLATIZED_FUNCTION_NOELEMENTS( SerializeTypedElement, (bSuccess, nIndex, buf), AssertMsg( 0, "Cannot serialize elements!\n" ); );
	return bSuccess;
}

//-----------------------------------------------------------------------------
// Read from file
//-----------------------------------------------------------------------------
template < class VT, class T >
void CDmxAttribute::UnserializeType( bool &bSuccess, CUtlBuffer &buf )
{
	bSuccess = ::Unserialize( buf, *(VT *)m_pData );
}
bool CDmxAttribute::Unserialize( DmAttributeType_t type, CUtlBuffer &buf )
{
	AllocateDataMemory_AndConstruct( type );

	bool bSuccess = false;
	// Process array and non-array types, excluding elements
	CALL_TYPE_TEMPLATIZED_FUNCTION_NOELEMENTS( UnserializeType, (bSuccess, buf), AssertMsg( 0, "Cannot unserialize elements or element arrays!\n" ); );
	return bSuccess;
}


//-----------------------------------------------------------------------------
// Read element from file
//-----------------------------------------------------------------------------
template < class VT, class T >
void CDmxAttribute::UnserializeTypedElement( bool &bSuccess, CUtlBuffer &buf )
{
	T temp;
	bSuccess = ::Unserialize( buf, temp );
	if ( bSuccess )
		((VT *)m_pData)->AddToTail( temp );
}
bool CDmxAttribute::UnserializeElement( DmAttributeType_t type, CUtlBuffer &buf )
{
	if ( !IsArrayType( type ) )
		return false;

	if ( m_Type != type )
		AllocateDataMemory_AndConstruct( type );

	bool bSuccess = false;
	// Process array types only, excluding elements
	CALL_ARRAY_TYPE_TEMPLATIZED_FUNCTION_NOELEMENTS( UnserializeTypedElement, (bSuccess, buf), AssertMsg( 0, "Cannot unserialize elements!\n" ) );
	return bSuccess;
}


//-----------------------------------------------------------------------------
// Sets name
//-----------------------------------------------------------------------------
void CDmxAttribute::SetName( const char *pAttributeName )
{
	m_Name = s_AttributeNameSymbols.Find( pAttributeName );
}

//-----------------------------------------------------------------------------
// Sets values
//-----------------------------------------------------------------------------
void CDmxAttribute::SetValue( const char *pString )
{
	AllocateDataMemory( AT_STRING );
	CUtlString* pUtlString = (CUtlString*)m_pData;
	Construct( pUtlString );
	pUtlString->Set( pString );
}

void CDmxAttribute::SetValue( char *pString )
{
	SetValue( (const char*)pString );
}

void CDmxAttribute::SetValue( const void *pBuffer, size_t nLen )
{
	AllocateDataMemory( AT_VOID );
	CUtlBinaryBlock* pBlob = (CUtlBinaryBlock*)m_pData;
	Construct( pBlob );
	pBlob->Set( pBuffer, nLen );
}

// Untyped method for setting used by unpack
void CDmxAttribute::SetValue( DmAttributeType_t type, const void *pSrc, int nLen )
{
	if ( m_Type != type )
	{
		AllocateDataMemory( type );
	}
	if ( nLen > CDmxAttribute::AttributeDataSize( type ) )
	{
		nLen = CDmxAttribute::AttributeDataSize( type );
	}
	memcpy( m_pData, pSrc, nLen );
}

// Untyped method for setting arrays, used by unpack
void CDmxAttribute::SetArrayValue( DmAttributeType_t type, const void *pSrc, int nDataTypeSize, int nArrayLength, int nSrcStride )
{
	if ( !IsArrayType( type ) )
		return;

	if ( m_Type != type )
	{
		AllocateDataMemory( type );
	}

	// NOTE: nDestStride will be 4 for char/short/int values, and the below code is designed to work in all those cases
	DmAttributeType_t basicType = ArrayAttributeBasicType( type );
	int nDestStride = CDmxAttribute::AttributeDataSize( basicType );
	Assert( nDataTypeSize <= nDestStride );
	nDataTypeSize = MIN( nDataTypeSize, nDestStride );

	SetArrayCount( nArrayLength );
	void *pDest = (void *)GetArrayBase();
	if ( !pDest )
		return;
	if ( nDataTypeSize != nDestStride )
	{
		// Avoid writing junk, keep the data clean in case we inspect the memory or a serialized file:
		Q_memset( pDest, 0, nDestStride*nArrayLength );
	}
	if ( ( nSrcStride == nDestStride ) && ( nDataTypeSize == nSrcStride ) )
	{
		memcpy( pDest, pSrc, nDestStride*nArrayLength );
	}
	else
	{
		byte       *pByteDest =       (byte *)pDest;
		const byte *pByteSrc  = (const byte *)pSrc;
		for ( int i = 0; i < nArrayLength; i++ )
		{
			memcpy( pByteDest, pByteSrc, nDataTypeSize );
			pByteDest += nDestStride;
			pByteSrc  += nSrcStride;
		}
	}
}

void CDmxAttribute::GetArrayValue( DmAttributeType_t type, void *pDest, int nDataTypeSize, int nDestArrayLength, const char *pDefaultString ) const
{
	if ( !IsArrayType( type ) || ( m_Type != type ) )
		return;

	// NOTE: nDestStride will be 4 for char/short/int values, and the below code is designed to work in all those cases
	DmAttributeType_t basicType = ArrayAttributeBasicType( type );
	int nSrcStride = CDmxAttribute::AttributeDataSize( basicType );
	Assert( nDataTypeSize <= nSrcStride );
	nDataTypeSize = MIN( nDataTypeSize, nSrcStride );

	int nSrcArrayLength = GetArrayCount();
	const void *pSrc = GetArrayBase();
	if ( nSrcArrayLength && pSrc )
	{
		if ( nSrcStride == nDataTypeSize )
		{
			memcpy( pDest, pSrc, nSrcArrayLength*nDataTypeSize );
		}
		else
		{
			byte       *pByteDst =       (byte *)pDest;
			const byte *pByteSrc = (const byte *)pSrc;
			for ( int i = 0; i < nSrcArrayLength; i++ )
			{
				memcpy( pByteDst, pByteSrc, nDataTypeSize );
				pByteDst += nDataTypeSize;
				pByteSrc += nSrcStride;
			}
		}
	}
	if ( ( nSrcArrayLength < nDestArrayLength ) && pDefaultString )
	{
		CDmxAttribute temp( NULL );
		temp.AllocateDataMemory_AndConstruct( basicType );
		temp.SetValueFromString( pDefaultString );

		byte *pByteDst = ( (byte *)pDest ) + nSrcArrayLength*nDataTypeSize;
		for ( int i = nSrcArrayLength; i < nDestArrayLength; i++ )
		{
			memcpy( pByteDst, temp.m_pData, nDataTypeSize );
			pByteDst += nDataTypeSize;
		}
	}
}



void CDmxAttribute::SetValue( const CDmxAttribute *pAttribute )
{
	DmAttributeType_t type = pAttribute->GetType();
	if ( !IsArrayType( type ) )
	{
		SetValue( type, pAttribute->m_pData, CDmxAttribute::AttributeDataSize( type ) );
	}
	else
	{
		// Copying array attributes not done yet..
		Assert( 0 );
	}
}

// Sets the attribute to its default value based on its type
template < class VT, class T >
void CDmxAttribute::SetDefaultValue( void )
{
	CDmAttributeInfo< VT >::SetDefaultValue( *(VT *)( m_pData ) );
}
void CDmxAttribute::SetToDefaultValue()
{
	// Process array and non-array types, including elements
	CALL_TYPE_TEMPLATIZED_FUNCTION( SetDefaultValue, (), );
}


//-----------------------------------------------------------------------------
// Convert to and from string
//-----------------------------------------------------------------------------
void CDmxAttribute::SetValueFromString( const char *pValue )
{
	switch ( GetType() )
	{
	case AT_UNKNOWN:
		break;

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

			CUtlBuffer buf( pValue, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
			if ( !Unserialize( GetType(), buf ) )
			{
				SetToDefaultValue();
			}
		}
		break;
	}
}

const char *CDmxAttribute::GetValueAsString( char *pBuffer, size_t nBufLen ) const
{
	Assert( pBuffer );
	CUtlBuffer buf( pBuffer, nBufLen, CUtlBuffer::TEXT_BUFFER );
	Serialize( buf );
	return pBuffer;
}

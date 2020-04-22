//====== Copyright 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dmxloader/dmxelement.h"
#include "dmxloader/dmxattribute.h"
#include "tier1/utlbuffer.h"
#include "mathlib/ssemath.h"
#include "tier1/utlbufferutil.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef OSX
#pragma GCC diagnostic ignored "-Wtautological-compare"
#endif

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------
CUtlSymbolTableLargeMT CDmxElement::s_TypeSymbols;


//-----------------------------------------------------------------------------
// Creates a dmx element
//-----------------------------------------------------------------------------
CDmxElement* CreateDmxElement( const char *pType )
{
	return new CDmxElement( pType );
}


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
CDmxElement::CDmxElement( const char *pType )
{
	m_Type = s_TypeSymbols.AddString( pType );
	m_nLockCount = 0;
	m_bResortNeeded = false;
	m_bIsMarkedForDeletion = false;
	CreateUniqueId( &m_Id );
}

CDmxElement::~CDmxElement()
{
	CDmxElementModifyScope modify( this );
	RemoveAllAttributes();
}


//-----------------------------------------------------------------------------
// Utility method for getting at the type
//-----------------------------------------------------------------------------
CUtlSymbolLarge CDmxElement::GetType()  const
{
	return m_Type;
}

const char* CDmxElement::GetTypeString() const
{
	return m_Type.String();
}

const char* CDmxElement::GetName() const
{
	return GetValue< CUtlString >( "name" );
}

const DmObjectId_t &CDmxElement::GetId() const
{
	return m_Id;
}


//-----------------------------------------------------------------------------
// Sets the object id, name
//-----------------------------------------------------------------------------
void CDmxElement::SetId( const DmObjectId_t &id )
{
	CopyUniqueId( id, &m_Id );
}

void CDmxElement::SetName( const char *pName )
{
	SetValue< CUtlString >( "name", pName );
}


//-----------------------------------------------------------------------------
// Sorts the vector when a change has occurred
//-----------------------------------------------------------------------------
void CDmxElement::Resort( )	const
{
	if ( m_bResortNeeded )
	{
		AttributeList_t *pAttributes = const_cast< AttributeList_t *>( &m_Attributes );
		pAttributes->RedoSort();
		m_bResortNeeded = false;

		// NOTE: This checks for duplicate attribute names
		int nCount = m_Attributes.Count();
		for ( int i = nCount; --i >= 1; )
		{
			if ( m_Attributes[i] == NULL || m_Attributes[i-1] == NULL )
			{
				continue;
			}

			if ( m_Attributes[i]->GetNameSymbol() == m_Attributes[i-1]->GetNameSymbol() )
			{
				Warning( "Duplicate attribute name %s encountered!\n", m_Attributes[i]->GetName() );
				pAttributes->Remove(i);
				Assert( 0 );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Enables modification of the DmxElement
//-----------------------------------------------------------------------------
void CDmxElement::LockForChanges( bool bLock )
{
	if ( bLock )
	{
		++m_nLockCount;
	}
	else
	{
		if ( --m_nLockCount == 0 )
		{
			Resort();
		}
		Assert( m_nLockCount >= 0 );
	}
}


//-----------------------------------------------------------------------------
// Adds, removes attributes
//-----------------------------------------------------------------------------
CDmxAttribute *CDmxElement::AddAttribute( const char *pAttributeName )
{
	int nIndex = FindAttribute( pAttributeName );
	if ( nIndex >= 0 )
		return m_Attributes[nIndex];

	CDmxElementModifyScope modify( this );
	m_bResortNeeded = true;
	CDmxAttribute *pAttribute = new CDmxAttribute( pAttributeName );
	m_Attributes.InsertNoSort( pAttribute );
	return pAttribute;
}

void CDmxElement::RemoveAttribute( const char *pAttributeName )
{
	CDmxElementModifyScope modify( this );
	int idx = FindAttribute( pAttributeName );
	if ( idx >= 0 )
	{
		delete m_Attributes[idx];
		m_Attributes.Remove( idx );
	}
}

void CDmxElement::RemoveAttributeByPtr( CDmxAttribute *pAttribute )
{	
	CDmxElementModifyScope modify( this );
	int nCount = m_Attributes.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Attributes[i] != pAttribute )
			continue;

		delete pAttribute;
		m_Attributes.Remove( i );
		break;
	}
}

void CDmxElement::RemoveAllAttributes()
{
	int nCount = m_Attributes.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		delete m_Attributes[i];
	}
	m_Attributes.RemoveAll();
	m_bResortNeeded = false;
}


//-----------------------------------------------------------------------------
// Rename an attribute
//-----------------------------------------------------------------------------
void CDmxElement::RenameAttribute( const char *pAttributeName, const char *pNewName )
{
	CDmxElementModifyScope modify( this );

	// No change...
	if ( !Q_stricmp( pAttributeName, pNewName ) )
		return;

	int idx = FindAttribute( pAttributeName );
	if ( idx < 0 )
		return;

	if ( HasAttribute( pNewName ) )
	{
		Warning( "Tried to rename from \"%s\" to \"%s\", but \"%s\" already exists!\n",
			pAttributeName, pNewName, pNewName );
		return;
	}

	m_bResortNeeded = true;
	m_Attributes[ idx ]->SetName( pNewName );
}


//-----------------------------------------------------------------------------
// Find an attribute by name-based lookup
//-----------------------------------------------------------------------------
int CDmxElement::FindAttribute( const char *pAttributeName ) const
{
	// FIXME: The cost here is O(log M) + O(log N)
	// where log N is the binary search for the symbol match
	// and log M is the binary search for the attribute name->symbol
	// We can eliminate log M by using a hash table in the symbol lookup
	Resort();
	CDmxAttribute search( pAttributeName );
	return m_Attributes.Find( &search );
}


//-----------------------------------------------------------------------------
// Find an attribute by name-based lookup
//-----------------------------------------------------------------------------
int CDmxElement::FindAttribute( CUtlSymbolLarge attributeName ) const
{
	Resort();
	CDmxAttribute search( attributeName );
	return m_Attributes.Find( &search );
}


//-----------------------------------------------------------------------------
// Attribute finding
//-----------------------------------------------------------------------------
bool CDmxElement::HasAttribute( const char *pAttributeName ) const
{
	int idx = FindAttribute( pAttributeName );
	return ( idx >= 0 );
}

CDmxAttribute *CDmxElement::GetAttribute( const char *pAttributeName )
{
	int idx = FindAttribute( pAttributeName );
	if ( idx >= 0 )
		return m_Attributes[ idx ];
	return NULL;
}

const CDmxAttribute *CDmxElement::GetAttribute( const char *pAttributeName ) const
{
	int idx = FindAttribute( pAttributeName );
	if ( idx >= 0 )
		return m_Attributes[ idx ];
	return NULL;
}


//-----------------------------------------------------------------------------
// Attribute interation
//-----------------------------------------------------------------------------
int CDmxElement::AttributeCount() const
{
	return m_Attributes.Count();
}

CDmxAttribute *CDmxElement::GetAttribute( int nIndex )
{
	return m_Attributes[ nIndex ];
}

const CDmxAttribute *CDmxElement::GetAttribute( int nIndex ) const
{
	return m_Attributes[ nIndex ];
}


//-----------------------------------------------------------------------------
// Removes all elements recursively
//-----------------------------------------------------------------------------
void CDmxElement::AddElementsToDelete( CUtlVector< CDmxElement * >& elementsToDelete )
{
	if ( m_bIsMarkedForDeletion )
		return;

	m_bIsMarkedForDeletion = true;
	elementsToDelete.AddToTail( this );

	int nCount = AttributeCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = GetAttribute(i);
		if ( pAttribute->GetType() == AT_ELEMENT )
		{
			CDmxElement *pElement = pAttribute->GetValue< CDmxElement* >();
			if ( pElement )
			{
				pElement->AddElementsToDelete( elementsToDelete );
			}
			continue;
		}

		if ( pAttribute->GetType() == AT_ELEMENT_ARRAY )
		{
			const CUtlVector< CDmxElement * > &elements = pAttribute->GetArray< CDmxElement* >();
			int nElementCount = elements.Count();
			for ( int j = 0; j < nElementCount; ++j )
			{
				if ( elements[j] )
				{
					elements[j]->AddElementsToDelete( elementsToDelete );
				}
			}
			continue;
		}
	}
}


//-----------------------------------------------------------------------------
// Removes all elements recursively
//-----------------------------------------------------------------------------
void CDmxElement::RemoveAllElementsRecursive()
{
	CUtlVector< CDmxElement * > elementsToDelete; 
	AddElementsToDelete( elementsToDelete );
	int nCount = elementsToDelete.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		delete elementsToDelete[i];
	}
}

//-----------------------------------------------------------------------------
// Template for unpacking a bitfield inside of an unpack structure.
// pUnpack->m_nSize is the number of bits in the bitfield
// pUnpack->m_nBitOffset is the number of bits to offset from pDest
//-----------------------------------------------------------------------------
template <typename T>
void CDmxElement::UnpackBitfield( T *pDest, const DmxElementUnpackStructure_t *pUnpack, const CDmxAttribute *pAttribute ) const
{
	// Determine if T is a signed type
	const bool bIsSigned = ( 0 > (T)(-1) );
	if ( bIsSigned )
	{
		// signed types need to be larger than 1 in size or else you get sign extension problems
		Assert( pUnpack->m_nSize > 1 );   
	}

	// Right now the max size bitfield we handle is 32.
	Assert( pUnpack->m_nSize <= 32 );
	Assert( pUnpack->m_nSize <= 8*sizeof(T) );
	Assert( pUnpack->m_nBitOffset + pUnpack->m_nSize <= 8*sizeof(T) );

	// Create a mask that covers the bitfield.
	T mask;
	T maskBeforeShift;
	if ( pUnpack->m_nSize == 8*sizeof(T) )
	{
		maskBeforeShift = (T)~0;
		mask = maskBeforeShift;
	}
	else
	{
		maskBeforeShift = ( 1 << pUnpack->m_nSize ) - 1;
		mask = maskBeforeShift << pUnpack->m_nBitOffset;	
	}
	mask = ~mask;

	T value = ( *(T *)pAttribute->m_pData );

	// Determine if value is in the range that this variable can hold.
	T signedMaskBeforeShift = bIsSigned ? ( 1 << (pUnpack->m_nSize-1) ) - 1 : maskBeforeShift;
	if ( !bIsSigned || ( value >= 0 ) )
	{
		if ( ( value & ~signedMaskBeforeShift ) != 0 )
		{
			Warning( "Value %s exeeds size of datatype. \n", pUnpack->m_pAttributeName );
			value = 0;    // Clear value
		}
	}
	else
	{
		if ( !bIsSigned || ( ( value & ~signedMaskBeforeShift ) != ~signedMaskBeforeShift ) )
		{
			Warning( "Value %s exeeds size of datatype. \n", pUnpack->m_pAttributeName );
			value = 0;    // Clear value
		}
	}

	// Mask value to the correct number of bits (important if value is a neg number!)
	value &= maskBeforeShift;
	
	// Pack it together.
	// Clear value
	*pDest &= mask;
	// Install value
	*pDest |=  ( value << pUnpack->m_nBitOffset );

}

//-----------------------------------------------------------------------------
// Method to unpack data into a structure
//-----------------------------------------------------------------------------
void CDmxElement::UnpackIntoStructure( void *pData, const DmxElementUnpackStructure_t *pUnpack ) const
{
	for ( ; pUnpack->m_AttributeType != AT_UNKNOWN; ++pUnpack )
	{
		char *pDest = (char*)pData + pUnpack->m_nOffset;

		// Recurse?
		if ( pUnpack->m_pSub )
		{
			UnpackIntoStructure( (void *)pDest, pUnpack->m_pSub );
			continue;
		}

		if ( IsArrayType( pUnpack->m_AttributeType ) )
		{
			// NOTE: This does not work with string/bitfield array data at the moment
			if ( ( pUnpack->m_AttributeType == AT_STRING_ARRAY ) || ( pUnpack->m_nBitOffset != NO_BIT_OFFSET ) )
			{
				AssertMsg( 0, ( "CDmxElement::UnpackIntoStructure: String and bitfield array attribute types not currently supported!\n" ) );
				continue;
			}
		}

		if ( ( pUnpack->m_AttributeType == AT_VOID ) || ( pUnpack->m_AttributeType == AT_VOID_ARRAY ) )
		{
			AssertMsg( 0, ( "CDmxElement::UnpackIntoStructure: Binary blob attribute types not currently supported!\n" ) );
			continue;
		}

		CDmxAttribute temp( NULL );
		const CDmxAttribute *pAttribute = GetAttribute( pUnpack->m_pAttributeName );
		if ( !pAttribute )
		{
			if ( !pUnpack->m_pDefaultString )
				continue;

			temp.AllocateDataMemory_AndConstruct( pUnpack->m_AttributeType );
			if ( !IsArrayType( pUnpack->m_AttributeType ) )
			{
				// Convert the default string into the target (array types do this inside GetArrayValue below)
				temp.SetValueFromString( pUnpack->m_pDefaultString );
			}
			pAttribute = &temp;
		}

		if ( pUnpack->m_AttributeType != pAttribute->GetType() ) 
		{
			Warning( "CDmxElement::UnpackIntoStructure: Mismatched attribute type in attribute \"%s\"!\n", pUnpack->m_pAttributeName );
			continue;
		}

		if ( pAttribute->GetType() == AT_STRING )
		{
			if ( pUnpack->m_nSize == UTL_STRING_SIZE )  // the string is a UtlString.
			{
				*(CUtlString *)pDest = pAttribute->GetValueString();		
			}
			else  // the string is a preallocated char array.
			{
				// Strings get special treatment: they are stored as in-line arrays of chars
				Q_strncpy( pDest, pAttribute->GetValueString(), pUnpack->m_nSize );
			}
			continue;
		}

		// Get the basic type, if the attribute is an array:
		DmAttributeType_t basicType = CDmxAttribute::ArrayAttributeBasicType( pAttribute->GetType() );

		// Special case - if data type is float, but dest size == 16, we are unpacking into simd by replication
		if ( ( basicType == AT_FLOAT ) && ( pUnpack->m_nSize == sizeof( fltx4 ) ) )
		{
			if ( IsArrayType( pUnpack->m_AttributeType ) )
			{
				// Copy from the attribute into a fixed-size array:
				float *pfDest = (float *)pDest;
				const CUtlVector< float > &floatVector = pAttribute->GetArray< float >();
				for ( int i = 0; i < pUnpack->m_nArrayLength; i++ )
				{
					for ( int j = 0; j < 4; j++ ) memcpy( pfDest++, &floatVector[ i ], sizeof( float ) );
				}
			}
			else
			{
				memcpy( pDest + 0 * sizeof( float ), pAttribute->m_pData, sizeof( float ) );
				memcpy( pDest + 1 * sizeof( float ), pAttribute->m_pData, sizeof( float ) );
				memcpy( pDest + 2 * sizeof( float ), pAttribute->m_pData, sizeof( float ) );
				memcpy( pDest + 3 * sizeof( float ), pAttribute->m_pData, sizeof( float ) );
			}
		}
		else
		{
			int nDataTypeSize = pUnpack->m_nSize;
			if ( basicType == AT_INT )
			{
				if ( pUnpack->m_nBitOffset == NO_BIT_OFFSET ) // This test is not for bitfields
				{
					AssertMsg( nDataTypeSize <= CDmxAttribute::AttributeDataSize( basicType ), 
						( "CDmxElement::UnpackIntoStructure: Incorrect size to unpack data into in attribute \"%s\"!\n", pUnpack->m_pAttributeName ) );
				}
			}
			else
			{
				AssertMsg( nDataTypeSize == CDmxAttribute::AttributeDataSize( basicType ), 
					( "CDmxElement::UnpackIntoStructure: Incorrect size to unpack data into in attribute \"%s\"!\n", pUnpack->m_pAttributeName ) );
			}

			if ( IsArrayType( pUnpack->m_AttributeType ) )
			{
				// Copy from the attribute into a fixed-size array (padding with the default value if need be):
				pAttribute->GetArrayValue( pUnpack->m_AttributeType, pDest, nDataTypeSize, pUnpack->m_nArrayLength, pUnpack->m_pDefaultString );
			}
			else if ( pUnpack->m_nBitOffset == NO_BIT_OFFSET )
			{
				memcpy( pDest, pAttribute->m_pData, pUnpack->m_nSize );
			}
			else
			{
				if ( pAttribute->GetType() == AT_INT )
				{
					// Int attribute types are used for char/short/int.
					switch ( pUnpack->m_BitfieldType )
					{
					case BITFIELD_TYPE_BOOL:
						// Note: unsigned char handles bools as bitfields.
						UnpackBitfield( (unsigned char *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_CHAR : 		
						UnpackBitfield( (char *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_UNSIGNED_CHAR : 		
						UnpackBitfield( (unsigned char *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_SHORT : 		
						UnpackBitfield( (short *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_UNSIGNED_SHORT : 		
						UnpackBitfield( (unsigned short *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_INT : 		
						UnpackBitfield( (int *)pDest, pUnpack, pAttribute );
						break;
					case BITFIELD_TYPE_UNSIGNED_INT : 		
						UnpackBitfield( (unsigned int *)pDest, pUnpack, pAttribute );
						break;
					default:
						Assert(0);
						break;
					};
				}
				else
				{
					UnpackBitfield( (char *)pDest, pUnpack, pAttribute );
				}	
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Creates attributes based on the unpack structure
//-----------------------------------------------------------------------------
void CDmxElement::AddAttributesFromStructure( const void *pData, const DmxElementUnpackStructure_t *pUnpack )
{
	for ( ; pUnpack->m_AttributeType != AT_UNKNOWN; ++pUnpack )
	{
		const char *pSrc = (const char*)pData + pUnpack->m_nOffset;
		if ( pUnpack->m_pSub )
		{
			CDmxElement *pDest = CreateDmxElement( pUnpack->m_pTypeName );
			pDest->AddAttributesFromStructure( pSrc, pUnpack->m_pSub );
			SetValue( pUnpack->m_pAttributeName, pDest );
			continue;
		}

		if ( IsArrayType( pUnpack->m_AttributeType ) )
		{
			// NOTE: This does not work with string/bitfield array data at the moment
			if ( ( pUnpack->m_AttributeType == AT_STRING_ARRAY ) || ( pUnpack->m_nBitOffset != NO_BIT_OFFSET ) )
			{
				AssertMsg( 0, ( "CDmxElement::AddAttributesFromStructure: String and bitfield array attribute types not currently supported!\n" ) );
				continue;
			}
		}

		if ( ( pUnpack->m_AttributeType == AT_VOID ) || ( pUnpack->m_AttributeType == AT_VOID_ARRAY ) )
		{
			AssertMsg( 0, ( "CDmxElement::AddAttributesFromStructure: Binary blob attribute types not currently supported!\n" ) );
			continue;
		}

		if ( HasAttribute( pUnpack->m_pAttributeName ) )
		{
			AssertMsg( 0, ( "CDmxElement::AddAttributesFromStructure: Attribute %s already exists!\n", pUnpack->m_pAttributeName ) );
			continue;
		}

		{
			CDmxElementModifyScope modify( this );
			CDmxAttribute *pAttribute = AddAttribute( pUnpack->m_pAttributeName );
			if ( pUnpack->m_AttributeType == AT_STRING )
			{
				if ( pUnpack->m_nSize == UTL_STRING_SIZE )	  // it is a UtlString. 
				{
					const char *test = (*(CUtlString *)pSrc).Get();
					pAttribute->SetValue( test );
				}
				else
				{
					pAttribute->SetValue( pSrc );
				}
			}
			else
			{
				// Get the basic data type, if the attribute is an array:
				DmAttributeType_t basicType = CDmxAttribute::ArrayAttributeBasicType( pUnpack->m_AttributeType );
				int nDataTypeSize = pUnpack->m_nSize;

				// handle float attrs stored as replicated fltx4's
				if ( ( basicType == AT_FLOAT ) && ( nDataTypeSize == sizeof( fltx4 ) ) )
				{
					nDataTypeSize = sizeof( float );
				}

				if ( basicType == AT_INT )
				{
					if ( pUnpack->m_nBitOffset == NO_BIT_OFFSET ) // This test is not for bitfields.
					{
						AssertMsg( nDataTypeSize <= CDmxAttribute::AttributeDataSize( basicType ), 
							( "CDmxElement::UnpackIntoStructure: Incorrect size to unpack data into %s attribute \"%s\"!\n", CDmxAttribute::s_pAttributeTypeName[ pUnpack->m_AttributeType ], pUnpack->m_pAttributeName ) );
					}
				}
				else
				{
					AssertMsg( nDataTypeSize == CDmxAttribute::AttributeDataSize( basicType ), 
							( "CDmxElement::UnpackIntoStructure: Incorrect size to unpack data into %s attribute \"%s\"!\n", CDmxAttribute::s_pAttributeTypeName[ pUnpack->m_AttributeType ], pUnpack->m_pAttributeName ) );
				}

				if ( IsArrayType( pUnpack->m_AttributeType ) )
				{
					// Copy from a fixed-size array into the attribute:
					int nArrayStride = pUnpack->m_nSize;
					pAttribute->SetArrayValue( pUnpack->m_AttributeType, pSrc, nDataTypeSize, pUnpack->m_nArrayLength, nArrayStride );
				}
				else if ( pUnpack->m_nBitOffset == NO_BIT_OFFSET )
				{
					pAttribute->SetValue( pUnpack->m_AttributeType, pSrc, nDataTypeSize );
				}
				else
				{
					
					// Right now the max size bitfield we handle is 32.
					Assert( pUnpack->m_nBitOffset + pUnpack->m_nSize <= 32 );

					int mask = 0;
					if ( pUnpack->m_nSize == 32 )
					{
						mask = ~0;
					}
					else
					{
						mask = ( 1 << pUnpack->m_nSize ) - 1;
						mask <<= pUnpack->m_nBitOffset;
					}
					
					int value = ( ( *(int *)pSrc ) & mask );
					if ( ( pUnpack->m_BitfieldType == BITFIELD_TYPE_CHAR ) ||
						( pUnpack->m_BitfieldType == BITFIELD_TYPE_SHORT ) ||
						( pUnpack->m_BitfieldType == BITFIELD_TYPE_INT ) )
					{
						// move it all the way up to make sure we get the correct sign extension.
						value <<= ( 32 - ( pUnpack->m_nBitOffset + pUnpack->m_nSize ) );
						// move it back down into position.
						value >>= ( 32 - ( pUnpack->m_nBitOffset + pUnpack->m_nSize ) );	
					}
					value >>= pUnpack->m_nBitOffset;
					pAttribute->SetValue( pUnpack->m_AttributeType, &value, sizeof( int ) );
				}
			}
		}
	}
}

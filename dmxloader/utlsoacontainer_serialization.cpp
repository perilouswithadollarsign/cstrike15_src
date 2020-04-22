//========== Copyright (c) Valve Corporation, All rights reserved. ==========//
//
// Purpose: see utlsoacontainer_serialization.h
//
//===========================================================================//

#include "tier0/platform.h"
#include "tier1/utlsoacontainer.h"
#include "dmxloader/dmxloader.h"
#include "dmxloader/dmxelement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



// Simple wrapper class to provide access to the CSOAContainer innards
class CSOAContainer_Serializable : public CSOAContainer
{
	DECLARE_DMXELEMENT_UNPACK();
public:
	bool Serialize( CDmxElement *pRootElement );
	bool Unserialize( const CDmxElement *pRootElement );
private:
	bool ContainsValidAttributes( void );
	enum SoaContainerDmxVersion_t { SOA_CONTAINER_DMX_VERSION = 1 };
};


BEGIN_DMXELEMENT_UNPACK( CSOAContainer_Serializable )
	// NOTE: All other member variables are recomputed from the below values, after unpack, by 'AllocateData' (the stored data is copyied in after that)
	DMXELEMENT_UNPACK_FIELD( "num_columns", "0", int, m_nColumns )
	DMXELEMENT_UNPACK_FIELD( "num_rows", "0", int, m_nRows )
	DMXELEMENT_UNPACK_FIELD( "num_slices", "0", int, m_nSlices )
	DMXELEMENT_UNPACK_FIELD_ARRAY( "attribute_types", "-1", int, m_nDataType )
	DMXELEMENT_UNPACK_FIELD( "field_present_mask", "0", int, m_nFieldPresentMask )
	DMXELEMENT_UNPACK_FIELD( "thread_mode", "-1", int, m_eThreadMode )
END_DMXELEMENT_UNPACK( CSOAContainer_Serializable, s_pSOAContainerUnpack )


bool CSOAContainer_Serializable::Serialize( CDmxElement *pRootElement )
{
	CDmxElementModifyScope modifyRoot( pRootElement );

	COMPILE_TIME_ASSERT( ATTRDATATYPE_NONE  == -1 ); // If this changes, modify the line for m_nDataType in the BEGIN_DMXELEMENT_UNPACK block above
	COMPILE_TIME_ASSERT( SOATHREADMODE_AUTO == -1 ); // If this changes, modify the line for m_eThreadMode in the BEGIN_DMXELEMENT_UNPACK block above

	if ( !ContainsValidAttributes() || ( !m_pDataMemory && !m_pConstantDataMemory ) )
	{
		Warning( "ERROR: CSOAContainer_Serializable::Unserialize - no data to serialize!\n" );
		return false;
	}

	// Write the version number first
	const int nDmxVersion = SOA_CONTAINER_DMX_VERSION;
	pRootElement->SetValue( "version", nDmxVersion );

	// Write some member variables (enough to recompute the rest)
	pRootElement->AddAttributesFromStructure( this, s_pSOAContainerUnpack );

	// Now write out the data, as floats (NOTE: we have to at least init the attribute as a zero-element vector or it won't serialize!)
	CDmxAttribute *pDataMemoryAttribute = pRootElement->AddAttribute( "memory_data" );
	CUtlVector< float >& dataVector = pDataMemoryAttribute->GetArrayForEdit< float >();
	size_t nDataMemorySize = DataMemorySize();
	dataVector.SetCount( nDataMemorySize  / sizeof( float ) );

	// NOTE: Constant data is always zero for now, but we write it out in case that changes (non-zero constant values seem potentially useful)
	CDmxAttribute *pConstantMemoryAttribute = pRootElement->AddAttribute( "constant_data" );
	CUtlVector< float >& constantVector = pConstantMemoryAttribute->GetArrayForEdit< float >();
	size_t nConstantMemorySize = ConstantMemorySize();
	constantVector.SetCount( nConstantMemorySize / sizeof( float ) );

	// To account for 'separate' memory allocations, we need to copy each attribute one at a time
	byte *pBaseDataPtr     = (byte *)dataVector.Base();
	byte *pBaseConstantPtr = (byte *)constantVector.Base();
	for( int i = 0; i < MAX_SOA_FIELDS; i++ )
	{
		if ( m_nDataType[i] == ATTRDATATYPE_NONE )
			continue;

		if ( m_nFieldPresentMask & ( 1 << i ) )
		{
			memcpy( pBaseDataPtr, m_pAttributePtrs[ i ], AttributeMemorySize( i ) );
			pBaseDataPtr += AttributeMemorySize( i );
		}
		else
		{
			memcpy( pBaseConstantPtr, m_pAttributePtrs[ i ], AttributeMemorySize( i ) );
			pBaseConstantPtr += AttributeMemorySize( i );
		}
	}

	return true;
}

bool CSOAContainer_Serializable::Unserialize( const CDmxElement *pRootElement )
{
	// Read the version number
	int nVersion = pRootElement->GetValue( "version", -1 );
	if ( nVersion == -1 )
	{
		Warning( "ERROR: CSOAContainer_Serializable::Unserialize - missing version field!\n" );
		return false;
	}

	// Clear, then unpack the stored members
	Purge();
	pRootElement->UnpackIntoStructure( this, s_pSOAContainerUnpack );

	// Check that we have enough data to create a valid container!
	int nError = 0;
	const CDmxAttribute *pDataMemoryAttribute     = pRootElement->GetAttribute( "memory_data" );
	const CDmxAttribute *pConstantMemoryAttribute = pRootElement->GetAttribute( "constant_data" );
	if ( !pDataMemoryAttribute || !pConstantMemoryAttribute )
		nError = 1;
	if ( !ContainsValidAttributes() )
		nError = 1;

	if ( !nError )
	{
		// Update files saved in old versions
		switch( nVersion )
		{
		case SOA_CONTAINER_DMX_VERSION:
			break; // Up to date - nothing to do.
		default:
			// The DMX unpack structure will set reasonable defaults or flag stuff that needs fixing up
			nError = 1; // TODO: add code when versions are bumped and fixup needs to happen
			break;
		}
	}

	if ( !nError )
	{
		SOAThreadMode_t eSavedThreadMode = m_eThreadMode;

		// Allocate memory (fills in all the pointers and strides)
		AllocateData( NumCols(), NumRows(), NumSlices() );

		// Set thread mode from the unserialized value (AllocateData stomps on it)
		SetThreadMode( eSavedThreadMode );

		// Now copy in the data
		if ( m_pDataMemory )
		{
			const CUtlVector< float >& floatVector = pDataMemoryAttribute->GetArray< float >();
			size_t nDataMemorySize = DataMemorySize();
			int nFloats = nDataMemorySize / sizeof( float );
			if ( nFloats == floatVector.Count() )
				memcpy( m_pDataMemory, floatVector.Base(), nDataMemorySize );
			else
				nError = 2;
		}
		if ( m_pConstantDataMemory )
		{
			const CUtlVector< float >& floatVector = pConstantMemoryAttribute->GetArray< float >();
			size_t nConstantMemorySize = ConstantMemorySize();
			int nFloats = nConstantMemorySize / sizeof( float );
			if ( nFloats == floatVector.Count() )
				memcpy( m_pConstantDataMemory, floatVector.Base(), nConstantMemorySize );
			else
				nError = 3;
		}
	}

	if ( nError )
	{
		switch( nError )
		{
			case 1: Warning( "ERROR: CSOAContainer_Serializable::Unserialize - DMX data does not represent a valid container!\n" ); break;
			case 2: Warning( "ERROR: CSOAContainer_Serializable::Unserialize - found wrong amount of memory data!\n" ); break;
			case 3: Warning( "ERROR: CSOAContainer_Serializable::Unserialize - found wrong amount of constant data!\n" ); break;
		}
		Assert( 0 );
		Purge();
		return false;
	}

	return true;
}

bool CSOAContainer_Serializable::ContainsValidAttributes( void )
{
	if ( ( NumCols() <= 0 ) || ( NumRows() <= 0 ) || ( NumSlices() <= 0 ) )
		return false;
	for ( int i = 0; i < MAX_SOA_FIELDS; i++ )
	{
		if ( m_nDataType[ i ] != ATTRDATATYPE_NONE )
			return true;
	}
	return false;
}


bool SerializeCSOAContainer(   const CSOAContainer *pContainer, CDmxElement *pRootElement )
{
	return ((CSOAContainer_Serializable*)pContainer)->Serialize( pRootElement );
}

bool UnserializeCSOAContainer( const CSOAContainer *pContainer, const CDmxElement *pRootElement )
{
	return ((CSOAContainer_Serializable*)pContainer)->Unserialize( pRootElement );
}

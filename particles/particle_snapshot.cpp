//===== Copyright � 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: see CParticleSnapshot declaration (in particles.h)
//
//===========================================================================//

#include "tier0/platform.h"
#include "particles/particles.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier2/fileutils.h"
#include "tier1/utlbuffer.h"
#include "tier1/UtlStringMap.h"
#include "tier1/strtools.h"
#include "dmxloader/dmxloader.h"
#include "dmxloader/utlsoacontainer_serialization.h"
#include "tier1/lzmaDecoder.h"
#include "tier0/vprof.h"
#include "particles_internal.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// This macro is used to process the attribute mapping parameters, for both vararg versions of Init():
#define ATTRIBUTE_MAPPING_LOOP( _lastarg_ )						\
	va_list args;												\
	va_start( args, _lastarg_ );								\
	for(;;)														\
	{															\
		int nFieldNumber = va_arg( args, int );					\
		if ( nFieldNumber == -1 )								\
			break;												\
		/* Associate nParticleAttribute with nFieldNumber */	\
		int nParticleAttribute = va_arg( args, int );


//-----------------------------------------------------------------------------
// Purge, clear the container back to its initial state
//-----------------------------------------------------------------------------
void CParticleSnapshot::Purge( void )
{
	m_Container.Purge();
	m_pContainer = NULL;
	for ( int i = 0; i < ARRAYSIZE( m_ParticleAttributeToContainerAttribute ); i++ ) m_ParticleAttributeToContainerAttribute[ i ] = -1;
	for ( int i = 0; i < ARRAYSIZE( m_ContainerAttributeToParticleAttribute ); i++ ) m_ContainerAttributeToParticleAttribute[ i ] = -1;
}

//-----------------------------------------------------------------------------
// AddAttributeMapping, add a new field/attribute pair
//-----------------------------------------------------------------------------
bool CParticleSnapshot::AddAttributeMapping( int nFieldNumber, int nParticleAttribute, const char *pFunc )
{
	if ( ( ( m_ParticleAttributeToContainerAttribute[ nParticleAttribute ] != -1 ) && ( m_ParticleAttributeToContainerAttribute[ nParticleAttribute ] != nFieldNumber       ) ) ||
		 ( ( m_ContainerAttributeToParticleAttribute[ nFieldNumber       ] != -1 ) && ( m_ContainerAttributeToParticleAttribute[ nFieldNumber       ] != nParticleAttribute ) ) )
	{
		Warning( "CParticleSnapshot::%s - Invalid attribute mapping specified (must be one-to-one)!\n", pFunc );
		Assert( 0 );
		Purge();
		return false;
	}
	m_ParticleAttributeToContainerAttribute[ nParticleAttribute ] = nFieldNumber;
	m_ContainerAttributeToParticleAttribute[ nFieldNumber ] = nParticleAttribute;
	return true;
}

//-----------------------------------------------------------------------------
// ValidateAttributeMapping, check datatypes for a field/attribute pair
//-----------------------------------------------------------------------------
bool CParticleSnapshot::ValidateAttributeMapping( int nFieldNumber, int nParticleAttribute, const char *pFunc )
{
	// Check the presence/datatype of each specified container field
	// TODO: support unallocated attributes (requires different 'copy' implementations in operators - can't use memcpy!)
	EAttributeDataType nExpectedDataType = g_pParticleSystemMgr->GetParticleAttributeDataType( nParticleAttribute );
	if ( ( m_pContainer->GetAttributeType( nFieldNumber ) != nExpectedDataType ) || !m_pContainer->HasAllocatedMemory( nFieldNumber ) )
	{
		Warning( "CParticleSnapshot::%s - Invalid attribute mapping specified for the provided container!\n", pFunc );
		if ( m_pContainer->GetAttributeType( nFieldNumber ) != nExpectedDataType )
			Warning( "  (data type of container field %d does not match particle attribute %s)\n", nFieldNumber, g_pParticleSystemMgr->GetParticleAttributeName( nParticleAttribute ) );
		if ( !m_pContainer->HasAllocatedMemory( nFieldNumber ) )
			Warning( "  (container field %d has no allocated data)\n", nFieldNumber );
		Assert( 0 );
		Purge();
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Init, creating a new container with the specified attribute mapping
//-----------------------------------------------------------------------------
bool CParticleSnapshot::Init( int nX, int nY, int nZ, const AttributeMapVector &attributeMaps )
{
	Assert( attributeMaps.Count() > 0 );
	if ( attributeMaps.Count() <= 0 )
		return false;
	Assert( ( nX >= 1 ) && ( nY >= 1 ) && ( nZ >= 1 ) );
	if ( ( nX < 1 ) || ( nY < 1 ) || ( nZ < 1 ) )
		return false;

	Purge();
	m_pContainer = &m_Container;

	for ( int i = 0; i < attributeMaps.Count(); i++ )
	{
		int nFieldNumber       = attributeMaps[ i ].m_nContainerAttribute;
		int nParticleAttribute = attributeMaps[ i ].m_nParticleAttribute;

		if ( !AddAttributeMapping( nFieldNumber, nParticleAttribute, "Init" ) )
			return false;

		// Set the datatype of each specified container field
		EAttributeDataType nDataType = g_pParticleSystemMgr->GetParticleAttributeDataType( nParticleAttribute );
		m_Container.SetAttributeType( nFieldNumber, nDataType );
	}
	m_Container.AllocateData( nX, nY, nZ );

	return true;
}

//-----------------------------------------------------------------------------
// Init, creating a new container with the specified attribute mapping
//-----------------------------------------------------------------------------
bool CParticleSnapshot::Init( int nX, int nY, int nZ, ... )
{
	AttributeMapVector attributeMaps;
	ATTRIBUTE_MAPPING_LOOP( nZ )
	//{
		attributeMaps.AddToTail( AttributeMap( nFieldNumber, nParticleAttribute ) );
	}
	return Init( nX, nY, nZ, attributeMaps );
}

//-----------------------------------------------------------------------------
// Init using an existing container, with the specified attribute mapping
//-----------------------------------------------------------------------------
bool CParticleSnapshot::InitExternal( CSOAContainer *pContainer, const AttributeMapVector &attributeMaps )
{
	Assert( attributeMaps.Count() > 0 );
	if ( attributeMaps.Count() <= 0 )
		return false;

	Purge();
	m_pContainer = pContainer;

	for ( int i = 0; i < attributeMaps.Count(); i++ )
	{
		int nFieldNumber       = attributeMaps[ i ].m_nContainerAttribute;
		int nParticleAttribute = attributeMaps[ i ].m_nParticleAttribute;

		if ( !AddAttributeMapping(      nFieldNumber, nParticleAttribute, "InitExternal" ) ||
			 !ValidateAttributeMapping( nFieldNumber, nParticleAttribute, "InitExternal" ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Init using an existing container, with the specified attribute mapping
//-----------------------------------------------------------------------------
bool CParticleSnapshot::InitExternal( CSOAContainer *pContainer, ... )
{
	AttributeMapVector attributeMaps;
	ATTRIBUTE_MAPPING_LOOP( pContainer )
	//{
		attributeMaps.AddToTail( AttributeMap( nFieldNumber, nParticleAttribute ) );
	}
	return InitExternal( pContainer, attributeMaps );
}

//-----------------------------------------------------------------------------
// Unpack structure for CParticleSnapshot
//-----------------------------------------------------------------------------
BEGIN_DMXELEMENT_UNPACK( CParticleSnapshot )
	DMXELEMENT_UNPACK_FIELD_ARRAY( "particle_attribute_to_container_attribute", "-1", int, m_ParticleAttributeToContainerAttribute )
	DMXELEMENT_UNPACK_FIELD_ARRAY( "container_attribute_to_particle_attribute", "-1", int, m_ContainerAttributeToParticleAttribute )
END_DMXELEMENT_UNPACK( CParticleSnapshot, s_pParticleSnapshotUnpack )

//-----------------------------------------------------------------------------
// Check whether the particle system's defined attributes have changed
//-----------------------------------------------------------------------------
void CParticleSnapshot::CheckParticleAttributesForChanges( void )
{
	// If this doesn't compile, then we need to bump the file version and perform fixup on files saved w/ the old attribute definitions:
	// TODO: store out an array of attribute names (g_pParticleSystemMgr->GetParticleAttributeName()) with the data so the fixup can be automatic and general
	COMPILE_TIME_ASSERT( MAX_PARTICLE_ATTRIBUTES == 24 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_XYZ					==  0 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_LIFE_DURATION		==  1 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_PREV_XYZ			==  2 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_RADIUS				==  3 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_ROTATION			==  4 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_ROTATION_SPEED		==  5 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_TINT_RGB			==  6 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_ALPHA				==  7 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_CREATION_TIME		==  8 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER		==  9 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_TRAIL_LENGTH		== 10 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_PARTICLE_ID			== 11 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_YAW					== 12 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1	== 13 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_HITBOX_INDEX		== 14 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ	== 15 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_ALPHA2				== 16 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_SCRATCH_VEC			== 17 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_SCRATCH_FLOAT		== 18 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_UNUSED				== 19 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_PITCH				== 20 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_NORMAL				== 21 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_GLOW_RGB			== 22 );
	COMPILE_TIME_ASSERT( PARTICLE_ATTRIBUTE_GLOW_ALPHA			== 23 );
}

//-----------------------------------------------------------------------------
// Init from a DMX (.psf) file
//-----------------------------------------------------------------------------
bool CParticleSnapshot::Unserialize( const char *pFullPath )
{
	DECLARE_DMX_CONTEXT();

	CheckParticleAttributesForChanges();

	CDmxElement *pRootElement = NULL;
	bool bTextMode = true, bBinaryMode = false;
	if ( !UnserializeDMX( pFullPath, "GAME", bTextMode,   &pRootElement ) &&
		 !UnserializeDMX( pFullPath, "GAME", bBinaryMode, &pRootElement ) ) // TODO: shouldn't UnserializeDMX automatically detect text mode?
	{
		Warning( "ERROR: CParticleSnapshot::Unserialize - could not load file %s!\n", pFullPath );
		return false;
	}

	bool bSuccess = true;

	CDmxElement *pParticleSnapshotElement = pRootElement->GetValue< CDmxElement * >( "particle_snapshot" );
	if ( !pParticleSnapshotElement )
	{
		Warning( "ERROR: CParticleSnapshot::Unserialize - %s is not a particle snapshot (.psf) file!\n", pFullPath );
		bSuccess = false;
	}

	int nVersion = -1;
	if ( bSuccess )
	{
		// Read the version number
		nVersion = pParticleSnapshotElement->GetValue( "version", -1 );
		if ( nVersion == -1 )
		{
			Warning( "ERROR: CParticleSnapshot::Unserialize - missing version field in file %s\n", pFullPath );
			bSuccess = false;
		}
	}

	if ( bSuccess )
	{
		// Read the CParticleSnapshot structure
		Purge();
		pParticleSnapshotElement->UnpackIntoStructure( this, s_pParticleSnapshotUnpack );
		// Unserialize the embedded CSOAContainer:
		CDmxElement *pContainerElement = pParticleSnapshotElement->GetValue< CDmxElement * >( "container" );
		if ( !UnserializeCSOAContainer( &m_Container, pContainerElement ) )
		{
			Warning( "ERROR: CParticleSnapshot::Unserialize - error reading embedded CSOAContainer in file %s\n", pFullPath );
			bSuccess = false;
		}
	}

	if ( bSuccess )
	{
		// Update files saved in old versions
		switch( nVersion )
		{
		case PARTICLE_SNAPSHOT_DMX_VERSION:
			// Up to date - nothing to do.
			break;
		default:
			// The DMX unpack structure will set reasonable defaults or flag stuff that needs fixing up
			// TODO: add code when versions are bumped and fixup needs to happen
			bSuccess = false;
			break;
		}
	}

	if ( bSuccess )
	{
		m_pContainer = &m_Container;

		// Validate the attribute mapping (re-'add' it, in both directions, to be paranoid)
		int nForwardMaps = 0, nReverseMaps = 0;
		for ( int i = 0; i < ARRAYSIZE( m_ParticleAttributeToContainerAttribute ); i++ )
		{
			int nFieldNumber = m_ParticleAttributeToContainerAttribute[ i ];
			if ( nFieldNumber == -1 )
				continue;
			if ( !AddAttributeMapping(      nFieldNumber, i, "Unserialize" ) ||
				 !ValidateAttributeMapping( nFieldNumber, i, "Unserialize" ) )
			{
				bSuccess = false;
				break;
			}
			nForwardMaps++;
		}
		for ( int i = 0; i < ARRAYSIZE( m_ContainerAttributeToParticleAttribute ); i++ )
		{
			int nParticleAttribute = m_ContainerAttributeToParticleAttribute[ i ];
			if ( nParticleAttribute == -1 )
				continue;
			if ( !AddAttributeMapping(      i, nParticleAttribute, "Unserialize" ) ||
				 !ValidateAttributeMapping( i, nParticleAttribute, "Unserialize" ) )
			{
				bSuccess = false;
				break;
			}
			nReverseMaps++;
		}

		if ( bSuccess && ( !nForwardMaps || !nReverseMaps ) )
		{
			bSuccess = false;
			Warning( "ERROR: CParticleSnapshot::Unserialize - error in data in file %s (no attribute mapping specified)\n", pFullPath );
			Assert( 0 );
		}
	}

	if ( !bSuccess )
	{
		// Leave a beautiful corpse
		Purge();
	}

	CleanupDMX( pRootElement );

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Write out to a DMX (.psf) file
//-----------------------------------------------------------------------------
bool CParticleSnapshot::Serialize( const char *pFullPath, bool bTextMode )
{
	DECLARE_DMX_CONTEXT();

	if ( !IsValid() )
	{
		Warning( "ERROR: CParticleSnapshot::Serialize - cannot serialize an uninitialized CParticleSnapshot! (%s)\n", pFullPath );
		return false;
	}
	const char *pExtension = V_GetFileExtension( pFullPath );
	if ( !pExtension || Q_stricmp( pExtension, "psf" ) )
	{
		Warning( "ERROR: CParticleSnapshot::Serialize - file extension should be '.psf' (%s)\n", pFullPath );
		return false;
	}

	bool bSuccess = true;
	CDmxElement *pRootElement = CreateDmxElement( "CDmeElement" );
	{
		CDmxElementModifyScope modifyRoot( pRootElement );

		// Write the version number first
		CDmxElement *pParticleSnapshotElement = CreateDmxElement( "CDmeParticleSnapshot" );
		pRootElement->SetValue( "particle_snapshot", pParticleSnapshotElement );
		
		int nDmxVersion = PARTICLE_SNAPSHOT_DMX_VERSION;
		pParticleSnapshotElement->SetValue( "version", nDmxVersion );

		// Then all our member variables
		pParticleSnapshotElement->AddAttributesFromStructure( this, s_pParticleSnapshotUnpack );

		// Then the embedded container
		CDmxElement *pContainerElement = CreateDmxElement( "CDmeSOAContainer" );
		pParticleSnapshotElement->SetValue( "container", pContainerElement );
		if ( !SerializeCSOAContainer( m_pContainer, pContainerElement ) )
		{
			Warning( "ERROR: CParticleSnapshot::Serialize - error serializing embedded CSOAContainer for file %s\n", pFullPath );
			bSuccess = false;
		}
	}

	if ( bSuccess )
	{
		// Write out the file
		bSuccess = SerializeDMX( pFullPath, "GAME", bTextMode, pRootElement );
	}

	CleanupDMX( pRootElement );

	return bSuccess;
}

//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "resourcefile/schema.h"
#include "resourcesystem/iresourcesystem.h"
#include "tier1/generichash.h"
#include "tier0/dbg.h"

// Must be last
#include "tier0/memdbgon.h"


#define STRUCT_NAME_HASH_SEED 0xBAADFEED
#define FIELD_CRC_HASH_SEED 0x12345678


//-----------------------------------------------------------------------------
// Computes a resource structure id given a structure name string
//-----------------------------------------------------------------------------
ResourceStructureId_t ComputeStructureNameHash( const char *pStructName )
{
	int nLength = Q_strlen( pStructName );
	return (ResourceStructureId_t)MurmurHash2( pStructName, nLength, STRUCT_NAME_HASH_SEED );
}

//////////////////////////////////////////////////////////////////////////


CResourceIntrospection* CResourceIntrospection::AddToStream( CResourceStream *pStream, ResourceFileHeader_t *pHeader, int nBlockIndex )
{
	CResourceIntrospection* pResult = Resource_AllocateBlock< CResourceIntrospection >( pStream, pHeader, nBlockIndex );
	pResult->m_nVersion = RESOURCE_INTROSPECTION_VERSION;
	pResult->m_Metadata = NULL;
	return pResult;
}

const CResourceIntrospection* CResourceIntrospection::FindInFile( const ResourceFileHeader_t *pHeader )
{
	const CResourceIntrospection* pResult = Resource_GetBlock<CResourceIntrospection>( pHeader );
	Assert( !pResult || pResult->m_nVersion == RESOURCE_INTROSPECTION_VERSION );
	return pResult;
}

uint32 CResourceIntrospection::GetVersion() const
{
	return m_nVersion;
}

const CResourceStructIntrospection* CResourceIntrospection::FindStructIntrospectionForResourceType( ResourceType_t nType ) const
{
	// FIXME: Assumes identical ResourceTypeEngine_t + metadata

	const CResourceStructIntrospection* pLocalStruct = g_pResourceSystem->FindStructIntrospectionForResourceType( nType );
	
	if ( pLocalStruct )
	{
		return FindStructIntrospection( pLocalStruct->m_nId );
	}
	else
	{
		return NULL;
	}

	/*
	const CResourceEnumIntrospection* pEnumIntrospect = FindEnumIntrospection( "ResourceTypeEngine_t" );

	if ( pEnumIntrospect == NULL )
	{
		Warning( "Encountered a CResourceIntrospection with no ResourceTypeEngine_t entry.\n" );
		return NULL;
	}

	const char *pResourceType = pEnumIntrospect->FindEnumString( nType ); 
	for ( int i = 0; i < m_StructIntrospection.Count(); ++i )
	{
		if ( !Q_stricmp( m_StructIntrospection[i].m_pResourceType, pResourceType ) )
			return &m_StructIntrospection[i];
	}
	return NULL;
	*/
}

const CResourceStructIntrospection* CResourceIntrospection::FindPermanentStructIntrospectionForResourceType( ResourceType_t nType ) const
{
	// FIXME: Assumes identical ResourceTypeEngine_t + metadata

	const CResourceStructIntrospection* pLocalStruct = g_pResourceSystem->FindPermanentStructIntrospectionForResourceType( nType );

	if ( pLocalStruct )
	{
		return FindStructIntrospection( pLocalStruct->m_nId );
	}
	else
	{
		return NULL;
	}

	/*
	const CResourceEnumIntrospection* pEnumIntrospect = FindEnumIntrospection( "ResourceTypeEngine_t" );

	if ( pEnumIntrospect == NULL )
	{
		// See above
		Warning( "Encountered a CResourceIntrospection with no ResourceTypeEngine_t entry.\n" );
		return NULL;
	}

	const char *pResourceType = pEnumIntrospect->FindEnumString( nType ); 
	for ( int i = 0; i < m_StructIntrospection.Count(); ++i )
	{
		if ( !Q_stricmp( m_StructIntrospection[i].m_pResourceType, pResourceType ) )
			return FindStructIntrospection( m_StructIntrospection[i].m_nUncacheableStructId );
	}
	return NULL;
	*/
}

//-----------------------------------------------------------------------------
// Iteration of resource introspection structs
//-----------------------------------------------------------------------------
int CResourceIntrospection::GetStructCount() const 
{
	return m_StructIntrospection.Count(); 
}

const CResourceStructIntrospection* CResourceIntrospection::GetStructIntrospection( int nIndex ) const
{
	return &m_StructIntrospection[nIndex];
}

CResourceStructIntrospection* CResourceIntrospection::GetWritableStructIntrospection( int nIndex )
{
	return &m_StructIntrospection[nIndex];
}


//-----------------------------------------------------------------------------
// Finds a struct introspection record
//-----------------------------------------------------------------------------
const CResourceStructIntrospection* CResourceIntrospection::FindStructIntrospection( ResourceStructureId_t id ) const
{
	int nCount = GetStructCount();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_StructIntrospection[i].m_nId == id )
			return &m_StructIntrospection[i];
	}
	return NULL;
}

const CResourceStructIntrospection* CResourceIntrospection::FindStructIntrospection( const char *pStructName ) const
{
	ResourceStructureId_t id = ComputeStructureNameHash( pStructName );
	return FindStructIntrospection( id );
}


//-----------------------------------------------------------------------------
// Iteration of resource introspection enums
//-----------------------------------------------------------------------------
int CResourceIntrospection::GetEnumCount() const 
{
	return m_EnumIntrospection.Count(); 
}

const CResourceEnumIntrospection* CResourceIntrospection::GetEnumIntrospection( int nIndex ) const
{
	return &m_EnumIntrospection[nIndex];
}

CResourceEnumIntrospection* CResourceIntrospection::GetWritableEnumIntrospection( int nIndex )
{
	return &m_EnumIntrospection[nIndex];
}

//-----------------------------------------------------------------------------
// Finds an enum introspection record
//-----------------------------------------------------------------------------
const CResourceEnumIntrospection* CResourceIntrospection::FindEnumIntrospection( ResourceStructureId_t id ) const
{
	int nCount = GetEnumCount();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_EnumIntrospection[i].m_nId == id )
			return &m_EnumIntrospection[i];
	}
	return NULL;
}

const CResourceEnumIntrospection* CResourceIntrospection::FindEnumIntrospection( const char *pEnumName ) const
{
	ResourceStructureId_t id = ComputeStructureNameHash( pEnumName );
	return FindEnumIntrospection( id );
}


//-----------------------------------------------------------------------------
// Iteration of resource introspection typedefs
//-----------------------------------------------------------------------------
int CResourceIntrospection::GetTypedefCount() const 
{
	return m_TypedefIntrospection.Count(); 
}

const CResourceTypedefIntrospection* CResourceIntrospection::GetTypedefIntrospection( int nIndex ) const
{
	return &m_TypedefIntrospection[nIndex];
}

CResourceTypedefIntrospection* CResourceIntrospection::GetWritableTypedefIntrospection( int nIndex )
{
	return &m_TypedefIntrospection[nIndex];
}


//-----------------------------------------------------------------------------
// Methods used for writing data
//-----------------------------------------------------------------------------

void CResourceIntrospection::AllocateStructs( CResourceStream *pStream, int nCount )
{
	m_StructIntrospection = pStream->Allocate< CResourceStructIntrospection >( nCount );
}

void CResourceIntrospection::AllocateEnums( CResourceStream *pStream, int nCount )
{
	m_EnumIntrospection = pStream->Allocate< CResourceEnumIntrospection >( nCount );
}

void CResourceIntrospection::AllocateTypedefs( CResourceStream *pStream, int nCount )
{
	m_TypedefIntrospection = pStream->Allocate< CResourceTypedefIntrospection >( nCount );
}

IntrospectionCompatibilityType_t CResourceIntrospection::CalculateCompatibility( ) const
{
	IntrospectionCompatibilityType_t nCompatibilityType = INTROSPECTION_COMPAT_IDENTICAL;

	for ( int i = 0; i < m_StructIntrospection.Count(); ++i )
	{
		const CResourceStructIntrospection *pMine = &m_StructIntrospection[i];
		const CResourceStructIntrospection *pGlobal = g_pResourceSystem->FindStructIntrospection( pMine->m_nId );

		if ( pGlobal == NULL )
		{
			Warning( "Resource introspection block contained a struct '%s' which doesn't exist anymore.\n", pMine->m_pName.GetPtr() );
			return INTROSPECTION_COMPAT_REQUIRES_CONVERSION;
		}

		if ( pMine->m_nCrc != pGlobal->m_nCrc )
		{
			return INTROSPECTION_COMPAT_REQUIRES_CONVERSION;
		}

		if ( pGlobal->m_nDiskSize != pGlobal->m_nMemorySize )
		{
			if ( !V_strcmp( pMine->m_pName, "MaterialDrawDescriptor_t" ) )
			{
				Warning( "****** HARDCODED CHECK FOR MaterialDrawDescriptor_t - REPLACE WITH METADATA LOOKUP WHEN AVAILABLE ******\n" );
				continue;
			}

			if ( nCompatibilityType == INTROSPECTION_COMPAT_IDENTICAL )
			{
				// don't want to say scatter if other things require full conversion
				nCompatibilityType = INTROSPECTION_COMPAT_REQUIRES_SCATTER;
			}
		}
	}

	for ( int i = 0; i < m_EnumIntrospection.Count(); ++i )
	{
		const CResourceEnumIntrospection *pMine = &m_EnumIntrospection[i];
		const CResourceEnumIntrospection *pGlobal = g_pResourceSystem->FindEnumIntrospection( pMine->m_nId );

		if ( pMine->m_nCrc != pGlobal->m_nCrc )
		{
			return INTROSPECTION_COMPAT_REQUIRES_CONVERSION;
		}
	}

	return nCompatibilityType;
}

//-----------------------------------------------------------------------------
//
// Introspection data associated with a single structure 
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Iteration of fields of a particular struct
//-----------------------------------------------------------------------------
int CResourceStructIntrospection::GetFieldCount() const 
{ 
	return m_FieldIntrospection.Count(); 
}

const CResourceFieldIntrospection* CResourceStructIntrospection::GetField( int nIndex ) const
{
	return &m_FieldIntrospection[nIndex];
}

CResourceFieldIntrospection* CResourceStructIntrospection::GetWritableField( int nIndex )
{
	return &m_FieldIntrospection[nIndex];
}


//-----------------------------------------------------------------------------
// Find a field
//-----------------------------------------------------------------------------
const CResourceFieldIntrospection* CResourceStructIntrospection::FindField( const char *pFieldName ) const
{
	int nCount = GetFieldCount();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( m_FieldIntrospection[i].m_pFieldName, pFieldName ) )
			return &m_FieldIntrospection[i];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Methods used for writing data
//-----------------------------------------------------------------------------
static char s_pEmptyBlockType[4] = { 0, 0, 0, 0 };

void CResourceStructIntrospection::SetStructInfo( CResourceStream *pStream, 
	const char *pStructName, const char *pBaseStruct, const char *pUncacheableStruct, uint32 nMemorySize, uint32 nDiskSize,
	uint32 nAlignment, const char *pDmeElementType, const char *pBlockType, const char *pResourceType, bool bHasVTable )
{
	ResourceStructureId_t id = ComputeStructureNameHash( pStructName );
	ResourceStructureId_t baseId = pBaseStruct ? ComputeStructureNameHash( pBaseStruct ) : 0;
	ResourceStructureId_t uncacheableId = pUncacheableStruct ? ComputeStructureNameHash( pUncacheableStruct ) : 0;

	m_pName = pStream->WriteString( pStructName );
	m_nId = id;
	m_nBaseStructId = baseId;
	m_nUncacheableStructId = uncacheableId;
	m_pDmeElementType = pStream->WriteString( pDmeElementType );
	m_nMemorySize = nMemorySize;
	m_nDiskSize = nDiskSize;
	m_nAlignment = nAlignment;

	if ( !pBlockType || !pBlockType[0] )
	{
		pBlockType = s_pEmptyBlockType;
	}
	memcpy( m_ResourceBlockType, pBlockType, 4 * sizeof(uint8) );

	m_pResourceType = pStream->WriteString( pResourceType );

	m_nStructFlags = bHasVTable ? RESOURCE_STRUCT_HAS_VTABLE : 0;
}

bool CResourceStructIntrospection::HasVTable() const
{
	return (m_nStructFlags & RESOURCE_STRUCT_HAS_VTABLE);
}

void CResourceStructIntrospection::SetStructInfo( CResourceStream *pStream, const CResourceStructIntrospection *src )
{
	SetStructInfo( pStream, src->m_pName, NULL, NULL, src->m_nMemorySize, src->m_nDiskSize, src->m_nAlignment, src->m_pDmeElementType, (const char*)src->m_ResourceBlockType, src->m_pResourceType, src->HasVTable() );
	m_nBaseStructId = src->m_nBaseStructId;
	m_nUncacheableStructId = src->m_nUncacheableStructId;
}

void CResourceStructIntrospection::ComputeCRC( CResourceStream *pStream )
{
	m_nCrc = MurmurHash2( m_FieldIntrospection.GetPtr(),
		m_FieldIntrospection.Count() * sizeof(CResourceFieldIntrospection), 
		FIELD_CRC_HASH_SEED );
}

void CResourceStructIntrospection::AllocateFields( CResourceStream *pStream, int nCount )
{
	m_FieldIntrospection = pStream->Allocate< CResourceFieldIntrospection >( nCount );
}

//-----------------------------------------------------------------------------
//
// Introspection data associated with a field in a structure
//
//-----------------------------------------------------------------------------

DEFINE_SCHEMA_DATA_CLASS( CResourceFieldIntrospection );

void CResourceFieldIntrospection::SetFieldInfo( CResourceStream *pStream, 
	const char *pFieldName, int nMemoryOffset, int nDiskOffset, int nArrayCount )
{
	m_pFieldName = pStream->WriteString( pFieldName );
	m_nInMemoryOffset = nMemoryOffset;
	m_nOnDiskOffset = nDiskOffset;
	m_nCount = nArrayCount;
}

void CResourceFieldIntrospection::SetFieldType( CResourceStream *pStream,
											    const CUtlVector<ResourceFieldType_t>& TypeChain, uint32 nRootFieldData )
{
	Assert( TypeChain.Count() > 0 );

	m_nTypeChainCount = TypeChain.Count();
	m_nFieldType = TypeChain[0];

	if ( m_nTypeChainCount == 1 )
	{
		m_nTypeChain = nRootFieldData;
	}
	else
	{
		CLockedResource<uint32> pChainAlloc = pStream->Allocate<uint32>( m_nTypeChainCount );

		for ( int i = 1; i < m_nTypeChainCount; ++i )
		{
			pChainAlloc[i-1] = TypeChain[i];
		}

		pChainAlloc[m_nTypeChainCount-1] = nRootFieldData;

		CResourcePointer<uint32>* pResult = reinterpret_cast< CResourcePointer<uint32>* >( &m_nTypeChain );
		*pResult = pChainAlloc;
	}
}

void CResourceFieldIntrospection::SetFieldInfo( CResourceStream *pStream, const CResourceFieldIntrospection *src )
{
	SetFieldInfo( pStream, src->m_pFieldName, src->m_nInMemoryOffset, src->m_nOnDiskOffset, src->m_nCount );

	m_nTypeChainCount = src->m_nTypeChainCount;
	m_nFieldType = src->ReadTypeChain(0);

	if ( m_nTypeChainCount == 1 )
	{
		m_nTypeChain = src->GetRootTypeData();
	}
	else
	{
		CLockedResource<uint32> pChainAlloc = pStream->Allocate<uint32>( m_nTypeChainCount );

		for ( int i = 1; i < m_nTypeChainCount; ++i )
		{
			pChainAlloc[i-1] = src->ReadTypeChain(i);
		}

		pChainAlloc[m_nTypeChainCount-1] = src->GetRootTypeData();

		CResourcePointer<uint32>* pResult = reinterpret_cast< CResourcePointer<uint32>* >( &m_nTypeChain );
		*pResult = pChainAlloc;
	}
}

ResourceFieldType_t CResourceFieldIntrospection::ReadTypeChain( int nChainIndex ) const
{
	Assert ( nChainIndex < m_nTypeChainCount );

	if ( nChainIndex == 0 )
	{
		// fieldtype is always the first element
		return (ResourceFieldType_t)m_nFieldType;
	}
	else
	{
		return (ResourceFieldType_t)(((const CResourcePointer<const uint32>*)( &m_nTypeChain ))->GetPtr())[nChainIndex-1];
	}
}

uint32 CResourceFieldIntrospection::GetRootTypeData() const
{
	if ( m_nTypeChainCount == 1 )
	{
		return m_nTypeChain;
	}
	else
	{
		const uint32* pChain = ((const CResourcePointer<const uint32>*)( &m_nTypeChain ))->GetPtr();
		return pChain[m_nTypeChainCount-1];
	}
}

ResourceFieldType_t CResourceFieldIntrospection::GetRootType() const
{
	if ( m_nTypeChainCount == 1 )
	{
		return (ResourceFieldType_t)m_nFieldType;
	}
	else
	{
		Assert( m_nTypeChainCount >= 2 );
		const uint32* pChain = ((const CResourcePointer<const uint32>*)( &m_nTypeChain ))->GetPtr();
		return (ResourceFieldType_t)pChain[m_nTypeChainCount-2];
	}
}

int CResourceFieldIntrospection::GetElementMemorySize( int nTypeChainIndex, const CResourceIntrospection* pIntroDct ) const
{
	ResourceFieldType_t fieldType = ReadTypeChain( nTypeChainIndex );

	if ( fieldType == RESOURCE_FIELD_TYPE_STRUCT )
	{
		// a struct must be at the end of a type chain
		Assert( nTypeChainIndex == m_nTypeChainCount - 1 );

		const CResourceStructIntrospection* pStructIntro = NULL;

		if ( pIntroDct != NULL )
		{
			pStructIntro = pIntroDct->FindStructIntrospection( (ResourceStructureId_t)GetRootTypeData() );
		}
		else
		{
			pStructIntro = g_pResourceSystem->FindStructIntrospection( (ResourceStructureId_t)GetRootTypeData() );
		}
		
		Assert( pStructIntro != NULL );

		return pStructIntro->m_nMemorySize;
	}
	else
	{
		return g_pResourceSystem->GetFieldSize( fieldType );
	}
}

int CResourceFieldIntrospection::GetElementDiskSize( int nTypeChainIndex, const CResourceIntrospection* pIntroDct ) const
{
	ResourceFieldType_t fieldType = ReadTypeChain( nTypeChainIndex );

	if ( fieldType == RESOURCE_FIELD_TYPE_STRUCT )
	{
		// a struct must be at the end of a type chain
		Assert( nTypeChainIndex == m_nTypeChainCount - 1 );

		const CResourceStructIntrospection* pStructIntro = NULL;

		if ( pIntroDct != NULL )
		{
			pStructIntro = pIntroDct->FindStructIntrospection( (ResourceStructureId_t)GetRootTypeData() );
		}
		else
		{
			pStructIntro = g_pResourceSystem->FindStructIntrospection( (ResourceStructureId_t)GetRootTypeData() );
		}

		Assert( pStructIntro != NULL );

		return pStructIntro->m_nDiskSize;
	}
	else
	{
		return g_pResourceSystem->GetFieldSize( fieldType );
	}
}

int CResourceFieldIntrospection::GetElementSize( const ResourceDataLayout_t nDataLocation, int nTypeChainIndex, const CResourceIntrospection* pIntroDct ) const
{
	if ( nDataLocation == RESOURCE_DATA_LAYOUT_MEMORY )
	{
		return GetElementMemorySize(nTypeChainIndex,pIntroDct);
	}
	else
	{
		Assert( nDataLocation == RESOURCE_DATA_LAYOUT_DISK );
		return GetElementDiskSize(nTypeChainIndex,pIntroDct);
	}
}

int CResourceFieldIntrospection::GetElementAlignment( int nTypeChainIndex, const CResourceIntrospection* pIntroDct ) const
{
	ResourceFieldType_t fieldType = ReadTypeChain( nTypeChainIndex );

	if ( fieldType == RESOURCE_FIELD_TYPE_STRUCT )
	{
		// a struct must be at the end of a type chain
		Assert( nTypeChainIndex == m_nTypeChainCount - 1 );

		const CResourceStructIntrospection* pStructIntro = NULL;

		if ( pIntroDct != NULL )
		{
			pStructIntro = pIntroDct->FindStructIntrospection( (ResourceStructureId_t)GetRootTypeData() );
		}
		else
		{
			pStructIntro = g_pResourceSystem->FindStructIntrospection( (ResourceStructureId_t)GetRootTypeData() );
		}

		Assert( pStructIntro != NULL );

		return pStructIntro->m_nAlignment;
	}
	else
	{
		return g_pResourceSystem->GetFieldAlignment( fieldType );
	}
}


//-----------------------------------------------------------------------------
//
// Introspection data associated with a single enumerated type 
//
//-----------------------------------------------------------------------------

DEFINE_SCHEMA_DATA_CLASS( CResourceEnumIntrospection );

int CResourceEnumIntrospection::GetEnumValueCount() const 
{ 
	return m_EnumValueIntrospection.Count(); 
}

const CResourceEnumValueIntrospection* CResourceEnumIntrospection::GetEnumValue( int nIndex ) const
{
	return &m_EnumValueIntrospection[nIndex];
}

CResourceEnumValueIntrospection* CResourceEnumIntrospection::GetWritableEnumValue( int nIndex )
{
	return &m_EnumValueIntrospection[nIndex];
}

const CResourceEnumValueIntrospection* CResourceEnumIntrospection::FindEnumValue( const char *pEnumValueName ) const
{
	int nCount = GetEnumValueCount();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( m_EnumValueIntrospection[i].m_pEnumValueName, pEnumValueName ) )
			return &m_EnumValueIntrospection[i];
	}
	return NULL;
}

const char *CResourceEnumIntrospection::FindEnumString( int nValue ) const
{
	int nCount = GetEnumValueCount();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_EnumValueIntrospection[i].m_nEnumValue == nValue )
			return m_EnumValueIntrospection[i].m_pEnumValueName;
	}
	return NULL;
}

void CResourceEnumIntrospection::SetEnumName( CResourceStream *pStream, const char *pEnumName )
{
	m_pName = pStream->WriteString( pEnumName );
	m_nId = ComputeStructureNameHash( pEnumName );
}

void CResourceEnumIntrospection::ComputeCRC( CResourceStream *pStream )
{
	m_nCrc = MurmurHash2( m_EnumValueIntrospection.GetPtr(),
		m_EnumValueIntrospection.Count() * sizeof(CResourceEnumValueIntrospection), 
		FIELD_CRC_HASH_SEED );
}

void CResourceEnumIntrospection::AllocateEnumValues( CResourceStream *pStream, int nCount )
{
	m_EnumValueIntrospection = pStream->Allocate< CResourceEnumValueIntrospection >( nCount );
}

void CResourceEnumValueIntrospection::SetEnumValueInfo( CResourceStream *pStream, const char *pEnumValueName, int32 nValue )
{
	m_pEnumValueName = pStream->WriteString( pEnumValueName );
	m_nEnumValue = nValue;
}

const char *CResourceEnumValueIntrospection::GetName() const
{
	return m_pEnumValueName;
}

int CResourceEnumValueIntrospection::GetValue() const
{
	return m_nEnumValue;
}

//-----------------------------------------------------------------------------
//
// Introspection data associated with a typedef 
//
//-----------------------------------------------------------------------------

DEFINE_SCHEMA_DATA_CLASS( CResourceTypedefIntrospection );

void CResourceTypedefIntrospection::SetTypedefInfo( CResourceStream *pStream, const char *pTypedefName, const char *pTypedefType )
{
	m_nId = ComputeStructureNameHash( pTypedefName );
	m_pName = pStream->WriteString( pTypedefName );
	m_pType = pStream->WriteString( pTypedefType );
}

//-----------------------------------------------------------------------------
//
// Helper for traversal of an introspected piece of memory
//
//-----------------------------------------------------------------------------

CResourceIntrospectionTraversal::CResourceIntrospectionTraversal( const CResourceIntrospection *pResIntro ) :
	m_pResIntro( pResIntro ),
	m_bTraverseDiskLayout( false )
{

}

void CResourceIntrospectionTraversal::TraverseStruct( const void *pStruct, const CResourceStructIntrospection *pStructIntro )
{
	Assert( pStruct != NULL );
	Assert( pStructIntro != NULL );

	if ( !VisitStruct( pStruct, pStructIntro ) )
	{
		return;
	}

	int nFields = pStructIntro->GetFieldCount();

	for ( int i = 0; i < nFields; ++i )
	{
		const CResourceFieldIntrospection *pFieldIntro = pStructIntro->GetField(i);
		void *pField = (uint8*)pStruct + ( m_bTraverseDiskLayout ?
										   pFieldIntro->m_nOnDiskOffset :
										   pFieldIntro->m_nInMemoryOffset );
		TraverseField( pField, pFieldIntro, (ResourceFieldType_t)pFieldIntro->m_nFieldType, 0 );
	}
}

void CResourceIntrospectionTraversal::TraverseRootField( const void *pField, const CResourceFieldIntrospection *pFieldIntro )
{
	if ( !VisitRootField( pField, pFieldIntro ) )
	{
		return;
	}

	if ( pField == NULL )
	{
		return;
	}

	ResourceFieldType_t nFieldType = pFieldIntro->GetRootType();
	uint32 nFieldData = pFieldIntro->GetRootTypeData();

	switch( nFieldType )
	{
		case RESOURCE_FIELD_TYPE_STRUCT:
			{
				const CResourceStructIntrospection* pStructIntro = NULL;
				
				if ( m_pResIntro )
				{
					pStructIntro = m_pResIntro->FindStructIntrospection( (ResourceStructureId_t)nFieldData );
				}
				else
				{
					pStructIntro = g_pResourceSystem->FindStructIntrospection( (ResourceStructureId_t)nFieldData );
				}

				TraverseStruct( pField, pStructIntro );
				break;
			}
		case RESOURCE_FIELD_TYPE_ENUM:
			{
				const CResourceEnumIntrospection* pEnumIntro = NULL;

				if ( m_pResIntro )
				{
					pEnumIntro = m_pResIntro->FindEnumIntrospection( (ResourceStructureId_t)nFieldData );
				}
				else
				{
					pEnumIntro = g_pResourceSystem->FindEnumIntrospection( (ResourceStructureId_t)nFieldData );
				}

				VisitEnum( pField, pEnumIntro );
				break;
			}
	}
}

void CResourceIntrospectionTraversal::TraverseField( const void *pField, const CResourceFieldIntrospection *pFieldIntro, ResourceFieldType_t fieldType, int nTypeChainIndex )
{
	if ( !VisitField( pField, pFieldIntro, nTypeChainIndex ) )
	{
		return;
	}

	int nElementSize = m_bTraverseDiskLayout ? pFieldIntro->GetElementDiskSize( nTypeChainIndex, m_pResIntro )
											 : pFieldIntro->GetElementMemorySize( nTypeChainIndex, m_pResIntro );

	int nInlineCount = 1;

	if ( nTypeChainIndex == 0 )
	{
		nInlineCount = pFieldIntro->m_nCount > 0 ? pFieldIntro->m_nCount : 1;
	}

	if ( nTypeChainIndex == pFieldIntro->m_nTypeChainCount-1 )
	{
		for ( int i = 0; i < nInlineCount; ++i )
		{
			void* pFieldInstance = (byte*)pField + nElementSize * i;
			TraverseRootField( pFieldInstance, pFieldIntro );
		}
	}
	else
	{
		ResourceFieldType_t subFieldType = pFieldIntro->ReadTypeChain(nTypeChainIndex);

		for ( int i = 0; i < nInlineCount; ++i )
		{
			void* pFieldInstance = (byte*)pField + nElementSize * i;
			switch ( fieldType )
			{
				case RESOURCE_FIELD_TYPE_RESOURCE_POINTER:
					{
						void *pPointedTo = ( ( CResourcePointer< char >* )pFieldInstance )->GetPtr();
						TraverseField( pPointedTo, pFieldIntro, subFieldType, nTypeChainIndex + 1 );
						break;
					}
				case RESOURCE_FIELD_TYPE_RESOURCE_REFERENCE:
					{
						// SCHEMAFIXME: Sometimes we might want to follow resource references during a traversal
						break;
					}
				case RESOURCE_FIELD_TYPE_RESOURCE_ARRAY:
					{
						int nStride = m_bTraverseDiskLayout ? pFieldIntro->GetElementDiskSize( nTypeChainIndex+1, m_pResIntro )
															: pFieldIntro->GetElementMemorySize( nTypeChainIndex+1, m_pResIntro );

						// can't do a CResourceArray cast because element size is unknown
						
						uint32 nArrayCount = ((uint32*)pFieldInstance)[1];

						if ( nArrayCount )
						{
							byte* pArrayRoot = ResolveOffsetFast( (const int32*)pFieldInstance );

							for ( uint32 i = 0; i < nArrayCount; ++i )
							{
								TraverseField( pArrayRoot+i*nStride, pFieldIntro, subFieldType, nTypeChainIndex + 1 );
							}
						}

						break;
					}
				case RESOURCE_FIELD_TYPE_C_POINTER:
					{
						void *pPointedTo = *((void**)pFieldInstance);
						TraverseField( pPointedTo, pFieldIntro, subFieldType, nTypeChainIndex + 1 );
						break;
					}
					break;
				default:
					AssertMsg(false, "Unrecognized non-root resource field type" );
					break;
			}
		}
	}
	
	PostVisitField( pField, pFieldIntro, nTypeChainIndex );
}

//////////////////////////////////////////////////////////////////////////

#define DEFINE_TYPE( _type_name, _resource_field_type, _memory_size, _disk_size, _alignment )\
{ _type_name, _resource_field_type, _memory_size, _disk_size, _alignment }

#define DEFINE_ATOMIC_TYPE_SIMPLE( _type_name, _resource_field_type ) DEFINE_TYPE( #_type_name, _resource_field_type, sizeof(_type_name), sizeof(_type_name), sizeof(_type_name) )
#define DEFINE_ATOMIC_TYPE_NOSERIALIZE( _type_name, _resource_field_type ) DEFINE_TYPE( #_type_name, _resource_field_type, sizeof(_type_name), 0, sizeof(_type_name) )
#define DEFINE_ATOMIC_TYPE_SPECIAL_ALIGN( _type_name, _resource_field_type, _align ) DEFINE_TYPE( #_type_name, _resource_field_type, sizeof(_type_name), sizeof(_type_name), _align )

#define DEFINE_PARAMETERIZED_TYPE( _type_name, _representative_type, _resource_field_type )\
	DEFINE_TYPE( _type_name, _resource_field_type, sizeof(_representative_type), sizeof(_representative_type), sizeof(_representative_type) )

static ResourceFieldProperties_t s_pFieldTypes[] =
{
	{ "unknown", RESOURCE_FIELD_TYPE_UNKNOWN, 0, 0, 0 },

	DEFINE_PARAMETERIZED_TYPE( "CResourcePointer", CResourcePointer<char>, RESOURCE_FIELD_TYPE_RESOURCE_POINTER ),
	DEFINE_PARAMETERIZED_TYPE( "CResourceArray", CResourceArray<char>, RESOURCE_FIELD_TYPE_RESOURCE_ARRAY ), 
	{ "struct", RESOURCE_FIELD_TYPE_STRUCT, 0, 0, 0 },
	{ "enum", RESOURCE_FIELD_TYPE_ENUM, 0, 0, 0 },
	DEFINE_PARAMETERIZED_TYPE( "CResourceReference", CResourceReference<char>, RESOURCE_FIELD_TYPE_RESOURCE_REFERENCE ), 

	{ "char", RESOURCE_FIELD_TYPE_CHAR, sizeof(char), 0, sizeof(char) },
	{ "pointer", RESOURCE_FIELD_TYPE_C_POINTER, sizeof(void*), 0, sizeof(void*) },

	{ "int", RESOURCE_FIELD_TYPE_INT, sizeof(int), RESOURCE_PLAIN_INT_SERIALIZATION_SIZE, sizeof(int) },
	{ "uint", RESOURCE_FIELD_TYPE_UINT, sizeof(uint), RESOURCE_PLAIN_INT_SERIALIZATION_SIZE, sizeof(uint) },

	{ "float", RESOURCE_FIELD_TYPE_FLOAT, sizeof(float), RESOURCE_PLAIN_FLOAT_SERIALIZATION_SIZE, sizeof(float) },

	DEFINE_ATOMIC_TYPE_SIMPLE( int8, RESOURCE_FIELD_TYPE_INT8 ),
	DEFINE_ATOMIC_TYPE_SIMPLE( uint8, RESOURCE_FIELD_TYPE_UINT8 ),
	DEFINE_ATOMIC_TYPE_SIMPLE( int16, RESOURCE_FIELD_TYPE_INT16 ),
	DEFINE_ATOMIC_TYPE_SIMPLE( uint16, RESOURCE_FIELD_TYPE_UINT16 ),
	DEFINE_ATOMIC_TYPE_SIMPLE( int32, RESOURCE_FIELD_TYPE_INT32 ),
	DEFINE_ATOMIC_TYPE_SIMPLE( uint32, RESOURCE_FIELD_TYPE_UINT32 ),
	DEFINE_ATOMIC_TYPE_SIMPLE( int64, RESOURCE_FIELD_TYPE_INT64 ),
	DEFINE_ATOMIC_TYPE_SIMPLE( uint64, RESOURCE_FIELD_TYPE_UINT64 ),
	DEFINE_ATOMIC_TYPE_SIMPLE( float32, RESOURCE_FIELD_TYPE_FLOAT32 ),
	DEFINE_ATOMIC_TYPE_SIMPLE( float64, RESOURCE_FIELD_TYPE_FLOAT64 ),

	{ "time", RESOURCE_FIELD_TYPE_TIME, 0, 0, 0 }, // SCHEMAFIXME: Time?

	DEFINE_ATOMIC_TYPE_SPECIAL_ALIGN( Vector2D, RESOURCE_FIELD_TYPE_VECTOR2D, sizeof(vec_t) ),
	DEFINE_ATOMIC_TYPE_SPECIAL_ALIGN( Vector, RESOURCE_FIELD_TYPE_VECTOR3D, sizeof(vec_t) ),
	DEFINE_ATOMIC_TYPE_SPECIAL_ALIGN( Vector4D, RESOURCE_FIELD_TYPE_VECTOR4D, sizeof(vec_t) ),
	DEFINE_ATOMIC_TYPE_SPECIAL_ALIGN( QAngle, RESOURCE_FIELD_TYPE_QANGLE, sizeof(vec_t) ),
	DEFINE_ATOMIC_TYPE_SPECIAL_ALIGN( Quaternion, RESOURCE_FIELD_TYPE_QUATERNION, sizeof(vec_t) ),
	DEFINE_ATOMIC_TYPE_SPECIAL_ALIGN( VMatrix, RESOURCE_FIELD_TYPE_VMATRIX, sizeof(vec_t) ),
	DEFINE_ATOMIC_TYPE_SIMPLE( fltx4, RESOURCE_FIELD_TYPE_FLTX4 ),

	DEFINE_ATOMIC_TYPE_SIMPLE( bool, RESOURCE_FIELD_TYPE_BOOL ),
	DEFINE_ATOMIC_TYPE_SIMPLE( CResourceString, RESOURCE_FIELD_TYPE_STRING ),

	{ "void", RESOURCE_FIELD_TYPE_VOID, 0, 0, 0 },
};

const ResourceFieldProperties_t* ResourceFieldProperties_t::GetFieldProperties( ResourceFieldType_t nFieldType )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_pFieldTypes ) == RESOURCE_FIELD_TYPE_COUNT );
	Assert( nFieldType >= 0 && nFieldType < RESOURCE_FIELD_TYPE_COUNT );
	Assert( s_pFieldTypes[nFieldType].m_nFieldType == nFieldType );
	return &(s_pFieldTypes[nFieldType]);
}

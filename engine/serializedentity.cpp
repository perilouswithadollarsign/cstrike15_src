//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//
//
//==================================================================================================
#include "tier1/bitbuf.h"
#include "serializedentity.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
DEFINE_FIXEDSIZE_ALLOCATOR_MT( CSerializedEntity, 2048, CUtlMemoryPool::GROW_FAST );

//-----------------------------------------------------------------------------
CSerializedEntity::CSerializedEntity()	
: m_pFields( NULL )
, m_pFieldDataOffsets( NULL )
, m_nFieldCount( 0 )
, m_nNumReservedFields( 0 )
, m_nFieldDataBits( 0 )
, m_pFieldData( NULL )
{
}

CSerializedEntity::~CSerializedEntity()
{
	Clear();
}

void CSerializedEntity::SetupPackMemory( int nNumFields, int nDataBits )
{
	AssertMsg( !IsPacked(), "Attempted to pack the memory inside of a Serialized entity that was already allocated. This is a memory leak." );

	//determine the total size for the buffer
	int fieldSize = PAD_NUMBER( nNumFields * sizeof( *m_pFields ), 4 );
	int dataOffsetsSize = PAD_NUMBER( nNumFields * sizeof( *m_pFieldDataOffsets ), 4 );
	int fieldDataSize = PAD_NUMBER( Bits2Bytes( nDataBits ), 4 );
	int size = fieldSize + dataOffsetsSize + fieldDataSize;

	if ( size )
	{
		//allocate a block of memory for it
		uint8* pMemBlock = (uint8*) g_pMemAlloc->Alloc( size );

		//and setup our pointers
		m_pFields			= ( short * ) pMemBlock;
		m_pFieldDataOffsets = ( uint32 * ) ( pMemBlock + fieldSize );
		m_pFieldData		= ( uint8 * )( pMemBlock + fieldSize + dataOffsetsSize );
	}
	else
	{
		// NOTE: We can't rely on the Source 1 XBox allocator returning NULL for a zero byte allocation
		m_pFields			= NULL;
		m_pFieldDataOffsets = NULL;
		m_pFieldData		= NULL;
	}

	m_nFieldCount			= nNumFields;
	m_nNumReservedFields	= knPacked;
	m_nFieldDataBits		= nDataBits;
}

void CSerializedEntity::Pack( short *pFields, uint32 *pFieldDataOffsets, int fieldCount, uint32 nFieldDataBits, uint8 *pFieldData )
{
	//setup the memory appropriately
	SetupPackMemory( fieldCount, nFieldDataBits );

	memcpy( m_pFields, pFields, m_nFieldCount * sizeof( *m_pFields ) );
	memcpy( m_pFieldDataOffsets, pFieldDataOffsets, m_nFieldCount * sizeof( *m_pFieldDataOffsets ) );
	memcpy( m_pFieldData, pFieldData, Bits2Bytes( nFieldDataBits ) );
}

void CSerializedEntity::Copy( const CSerializedEntity &other )
{
	if( this != &other )
	{
		Clear();
		Pack( other.m_pFields, other.m_pFieldDataOffsets, other.m_nFieldCount, other.m_nFieldDataBits, other.m_pFieldData );
	}
}

void CSerializedEntity::Swap( CSerializedEntity& other )
{
	//just swap all the pointers between the two fields
	V_swap(m_pFields, other.m_pFields);
	V_swap(m_pFieldDataOffsets, other.m_pFieldDataOffsets);
	V_swap(m_nFieldDataBits, other.m_nFieldDataBits);
	V_swap(m_pFieldData, other.m_pFieldData);
	V_swap(m_nFieldCount, other.m_nFieldCount);
	V_swap(m_nNumReservedFields, other.m_nNumReservedFields);

	#ifdef PARANOID_SERIALIZEDENTITY
		V_swap(m_File, other.m_File);
		V_swap(m_Line, other.m_Line);
	#endif
}

void CSerializedEntity::Clear()
{
	if ( IsPacked() )
	{
		g_pMemAlloc->Free( m_pFields );
		//the other two were allocated contiguously after the fields
	}
	else
	{
		//fields and data offsets are always allocated in a block
		g_pMemAlloc->Free( m_pFields );
		delete[] m_pFieldData;
	}

	m_nFieldDataBits = 0;
	m_pFields = NULL;
	m_pFieldDataOffsets = NULL;
	m_pFieldData = NULL;
	m_nFieldCount = 0;
	m_nNumReservedFields = 0;
}

void CSerializedEntity::ReservePathAndOffsetMemory( uint32 nNumElements )
{
	Assert( !IsPacked() );

	//don't allow them to reserve less memory than what we have already allocated
	m_nNumReservedFields = MAX( nNumElements, m_nFieldCount );

	//save our old pointers so we can copy from them
	short* pOldFields	= m_pFields;
	uint32* pOldOffsets	= m_pFieldDataOffsets;

	//allocate the new block
	uint32 nFieldSize	= PAD_NUMBER( sizeof( *m_pFields ) * m_nNumReservedFields, 4 );
	uint32 nOffsetSize	= sizeof( uint32 ) * m_nNumReservedFields;
	int nSize = nFieldSize + nOffsetSize;

	if ( nSize )
	{
		uint8* pMem			= ( uint8* )g_pMemAlloc->Alloc( nSize );

		//update our pointers
		m_pFields			= ( short* )pMem;
		m_pFieldDataOffsets = ( uint32* )( pMem + nFieldSize);

		//copy over the old data
		if( pOldFields )
		{	
			memcpy( m_pFields, pOldFields, sizeof( short ) * m_nFieldCount );
			memcpy( m_pFieldDataOffsets, pOldOffsets, sizeof( uint32 ) * m_nFieldCount );
		}
	}
	else
	{
		// NOTE: We can't rely on the Source 1 XBox allocator returning NULL for a zero byte allocation
		m_pFields			= NULL;
		m_pFieldDataOffsets = NULL;
	}

	//cleanup the old (which is over allocated)
	g_pMemAlloc->Free( pOldFields );
}

void CSerializedEntity::Grow()
{
	Assert( !IsPacked() );

	//determine our updated size (start at 2, grow in multiples of 2)
	if ( !m_pFields )
		ReservePathAndOffsetMemory( 2 );
	else
		ReservePathAndOffsetMemory( m_nNumReservedFields * 2 );	
}

 
// pvecFieldPathBits is used by DTI mode
bool CSerializedEntity::ReadFieldPaths( bf_read *pBuf, CUtlVector< int > *pvecFieldPathBits /*= NULL*/ )
{
	Assert( !IsPacked() );

	CDeltaBitsReader reader( pBuf );
	int iProp;
	while ( -1 != (iProp = reader.ReadNextPropIndex()) )
	{
		if ( m_nFieldCount == m_nNumReservedFields )
		{
			Grow();
		}
		m_pFields[m_nFieldCount++] = iProp;
		if ( pvecFieldPathBits )
		{
			pvecFieldPathBits->AddToTail( reader.GetFieldPathBits() );
		}
	}
	return true;
}

//given a data chunk, this will make sure that the resulting bits in the last byte are cleared to zero to ensure that they are consistent across compares
void ClearRemainingBits( uint32 nNumBits, uint8* pBitData )
{
	//determine how many bits beyond the last byte we need to keep
	const uint32 nRemainingBits	= nNumBits % 8;

	//if the remaining bits is zero, we are byte aligned, and accessing that byte can be beyond the array
	if( nRemainingBits > 0 )
	{
		//determine the byte to index into
		const uint32 nByte			= nNumBits / 8;
		const uint32 nKeepMask		= ( 1 << nRemainingBits ) - 1;
		pBitData[ nByte ] &= nKeepMask;
	}
}

void CSerializedEntity::PackWithFieldData( void *pData, int nDataBits )
{
	Assert( !IsPacked() );
	Assert( m_pFieldData == NULL );

	short *pStartFields = m_pFields;
	uint32 *pStartDataOffsets = m_pFieldDataOffsets;

	//setup the memory appropriately
	SetupPackMemory( m_nFieldCount, nDataBits );

	memcpy( m_pFields, pStartFields, m_nFieldCount * sizeof( *m_pFields ) );
	memcpy( m_pFieldDataOffsets, pStartDataOffsets, m_nFieldCount * sizeof( *m_pFieldDataOffsets ) );
	memcpy( m_pFieldData, pData, Bits2Bytes( nDataBits ) );
	ClearRemainingBits( nDataBits, m_pFieldData );

	//free our original memory
	g_pMemAlloc->Free( pStartFields );
}

void CSerializedEntity::PackWithFieldData( bf_read &buf, int nDataBits )
{
	Assert( !IsPacked() );
	Assert( m_pFieldData == NULL );

	short *pStartFields = m_pFields;
	uint32 *pStartDataOffsets = m_pFieldDataOffsets;

	//setup the memory appropriately
	SetupPackMemory( m_nFieldCount, nDataBits );

	memcpy( m_pFields, pStartFields, m_nFieldCount * sizeof( *m_pFields ) );
	memcpy( m_pFieldDataOffsets, pStartDataOffsets, m_nFieldCount * sizeof( *m_pFieldDataOffsets ) );
	buf.ReadBits( m_pFieldData, nDataBits );
	ClearRemainingBits( nDataBits, m_pFieldData );

	//free our original memory
	g_pMemAlloc->Free( pStartFields );
}

void CSerializedEntity::StartReading( bf_read& bitReader ) const
{
	bitReader.StartReading( m_pFieldData, PAD_NUMBER( Bits2Bytes( m_nFieldDataBits ), 4 ), 0, m_nFieldDataBits );
}

void CSerializedEntity::StartWriting( bf_write& bitWriter )
{
	bitWriter.StartWriting( m_pFieldData, PAD_NUMBER( Bits2Bytes( m_nFieldDataBits ), 4 ), 0, m_nFieldDataBits );
}

void CSerializedEntity::DumpMemInfo()
{
	CUtlMemoryPool &pool = s_Allocator;

	Msg("Pool: Count,Peak,Size,Total\n");
	Msg("      %d,%d,%d,%d\n", pool.Count(), pool.PeakCount(), pool.BlockSize(), pool.Size() );
}

class CSerializedEntities : public ISerializedEntities
{
public:

#ifdef PARANOID_SERIALIZEDENTITY
	CUtlRBTree< CSerializedEntity *, int > m_Current;
	CThreadFastMutex m_Mutex;
	CInterlockedInt m_Allocated;

	void Report()
	{
		if ( m_Allocated > 0 )
		{
			// If you see this, uncomment the #define PARANOID above and run again to see what's causing leaks
			Msg( "%d CSerializedEntity object leaked!!!\n", (int)m_Allocated );
		}
	}
#endif //PARANOID_SERIALIZEDENTITY

	CSerializedEntities() 
#ifdef PARANOID_SERIALIZEDENTITY
	: m_Current( 0, 0, DefLessFunc( CSerializedEntity * ) )
	, m_Allocated( 0 )
#endif
	{
	}

	~CSerializedEntities()
	{
#ifdef PARANOID_SERIALIZEDENTITY
		Report();
		FOR_EACH_UTLRBTREE( m_Current, i )
		{
			Msg( "Leak %s:%d\n", m_Current[ i ]->m_File, m_Current[ i ]->m_Line );
		}
#endif
	}

	virtual SerializedEntityHandle_t AllocateSerializedEntity(char const *pFile, int nLine )
	{
		CSerializedEntity *pEntity = new CSerializedEntity();
#ifdef PARANOID_SERIALIZEDENTITY
		pEntity->m_File = pFile;
		pEntity->m_Line = nLine;
		m_Allocated++;
		{
			AUTO_LOCK_FM( m_Mutex );
			m_Current.Insert( pEntity );
		}
#endif
		return reinterpret_cast< SerializedEntityHandle_t >( pEntity );
	}

	virtual void ReleaseSerializedEntity( SerializedEntityHandle_t handle )
	{
		if ( handle == SERIALIZED_ENTITY_HANDLE_INVALID )
			return;

		CSerializedEntity *pEntity = reinterpret_cast< CSerializedEntity * >( handle );
#ifdef PARANOID_SERIALIZEDENTITY
		m_Allocated--;
		{
			AUTO_LOCK_FM( m_Mutex );
			m_Current.Remove( pEntity );
		}
#endif
		delete pEntity;
	}

	virtual SerializedEntityHandle_t CopySerializedEntity( SerializedEntityHandle_t handle, char const *pFile, int nLine )
	{
		if ( handle == SERIALIZED_ENTITY_HANDLE_INVALID )
		{
			Assert( 0 );
			return SERIALIZED_ENTITY_HANDLE_INVALID;
		}

		CSerializedEntity *pSrc = reinterpret_cast< CSerializedEntity * >( handle );
		Assert( pSrc );

		CSerializedEntity *pDest = reinterpret_cast< CSerializedEntity * >( AllocateSerializedEntity(pFile, nLine) );
		pDest->Copy( *pSrc );
		return reinterpret_cast< SerializedEntityHandle_t >( pDest );
	}

};

static CSerializedEntities g_SerializedEntities;
ISerializedEntities *g_pSerializedEntities = &g_SerializedEntities;

CON_COMMAND( sv_dump_serialized_entities_mem, "Dump serialized entity allocations stats." )
{
	CSerializedEntity::DumpMemInfo();

#ifdef PARANOID_SERIALIZEDENTITY
	// Use int64 to avoid integer overflow when we do the calculations incorrectly.
	int64 totalBytes = 0;

	FOR_EACH_UTLRBTREE( g_SerializedEntities.m_Current, i )
	{
		CSerializedEntity *pEntity = g_SerializedEntities.m_Current[ i ];
		// m_nNumReservedFields indicates whether the entity is packed, which then determines how
		// we retrieve the size.
		int numFields = pEntity->IsPacked() ? pEntity->m_nFieldCount : pEntity->m_nNumReservedFields;
		int fieldSize = PAD_NUMBER( numFields * sizeof( *pEntity->m_pFields ), 4 );
		int dataOffsetsSize = PAD_NUMBER( numFields * sizeof( *pEntity->m_pFieldDataOffsets ), 4 );
		int fieldDataSize = PAD_NUMBER( Bits2Bytes( pEntity->m_nFieldDataBits ), 4 );
		int size = fieldSize + dataOffsetsSize + fieldDataSize;

		totalBytes += size;
	}

	double totalKB = totalBytes / 1024.0;
	unsigned int count = g_SerializedEntities.m_Current.Count();
	Msg( "SerializedEntity: %u, %1.1f KB, %1.1f KB/CSerializedEntity,\n", count, totalKB, totalKB / count );
#endif
}

#ifdef PARANOID_SERIALIZEDENTITY
CON_COMMAND( sv_dump_serialized_entities, "Dump serialized entity allocations." )
{
	FOR_EACH_UTLRBTREE( g_SerializedEntities.m_Current, i )
	{
		Msg( "SerializedEntity from %s:%d: %d fields, %d bits\n", 
			 g_SerializedEntities.m_Current[ i ]->m_File, 
			 g_SerializedEntities.m_Current[ i ]->m_Line, 
			 g_SerializedEntities.m_Current[ i ]->m_nFieldCount, 
			 g_SerializedEntities.m_Current[ i ]->m_nFieldDataBits );
	}
}
#endif


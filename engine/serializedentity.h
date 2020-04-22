//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//
//
//==================================================================================================

#ifndef SERIALIZEDENTITY_H
#define SERIALIZEDENTITY_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlmap.h"
#include "tier1/utlvector.h"
#include "dt.h"
#include "iclientnetworkable.h"
#include "ents_shared.h"

typedef int CFieldPath; // It's just an index into the flattened SendProp list!!!

//#define PARANOID_SERIALIZEDENTITY

class CSerializedEntity
{
public:
	CSerializedEntity();
	~CSerializedEntity();

	//gets the number of fields contained within this serialized entity
	int		GetFieldCount() const							{ return m_nFieldCount; }
	//determines the total number of bits used to hold all the field data
	uint32	GetFieldDataBitCount() const					{ return m_nFieldDataBits; }
	//determines if this entity is in a packed state. If so, it must be cleared and reallocated to change the size of any of the fields
	bool	IsPacked() const								{ return ( m_nNumReservedFields == knPacked ); }

	//given an index, this will return the 'path' or identifier for that field
	CFieldPath	GetFieldPath( int nIndex ) const			{ Assert( nIndex < m_nFieldCount ); return m_pFields[ nIndex ]; }
	void		SetFieldPath( int nIndex, CFieldPath path ) { Assert( nIndex < m_nFieldCount ); m_pFields[ nIndex ] = path; }

	//given a field index, this will return the starting bit within the field data for that field
	uint32		GetFieldDataBitOffset( int nIndex ) const			{ Assert( nIndex < m_nFieldCount ); return m_pFieldDataOffsets[ nIndex ]; }
	uint32		GetFieldDataBitEndOffset( int nIndex ) const		{ Assert( nIndex < m_nFieldCount ); return ( nIndex + 1 < m_nFieldCount ) ? m_pFieldDataOffsets[ nIndex + 1 ] : m_nFieldDataBits; }
	void		SetFieldDataBitOffset( int nIndex, uint32 nOffset ) { Assert( nIndex < m_nFieldCount ); m_pFieldDataOffsets[ nIndex ] = nOffset; }

	//access to the direct memory buffers for those that need direct access
	uint8*			GetFieldData()							{ return m_pFieldData; }
	const uint8*	GetFieldData() const					{ return m_pFieldData; }
	short*			GetFieldPaths()							{ return m_pFields; }
	const short*	GetFieldPaths() const					{ return m_pFields; }
	uint32*			GetFieldDataBitOffsets()				{ return m_pFieldDataOffsets; }
	const uint32*	GetFieldDataBitOffsets() const			{ return m_pFieldDataOffsets; }
	
	//given another serialized entity, this will just copy over the contents of the other entity, making its own copy
	void Copy( const CSerializedEntity &other );

	//given another serialized entity, this will handle swapping the contents of this one with the other serialized entity
	void Swap( CSerializedEntity& other );

	//clears all the contents of this serialized entity
	void Clear();

	//this sets up the memory within this serialized entity for the specified number of fields and total data size. This is useful
	//for in place writing of results to avoid intermediate allocations, but care should be taken that the entity is already cleared
	//prior to this call
	void SetupPackMemory( int nNumFields, int nDataBits );

	//given a field index, this will return all the information associated with it
	void GetField( int nFieldIndex, CFieldPath &path, int *pnDataOffset, int *pnNextDataOffset ) const
	{
		path = GetFieldPath( nFieldIndex );
		*pnDataOffset = GetFieldDataBitOffset( nFieldIndex );
		*pnNextDataOffset = GetFieldDataBitEndOffset( nFieldIndex );		
	}

	//---------------------------------------------------------------
	//the following are only valid on unpacked serialized entities. When done with adding data, they should be packed with their field data

	//called to reserve an amount of memory for the path and offset information
	void ReservePathAndOffsetMemory( uint32 nNumElements );
	//adds a path and offset to the list of fields in this set
	void AddPathAndOffset( const CFieldPath &path, int nStartBit )
	{
		//inlined for performance when packing snapshots for networking (called for EVERY property)
		Assert( !IsPacked() );
		if ( m_nFieldCount == m_nNumReservedFields )
			Grow();

		m_pFields[m_nFieldCount] = path;
		m_pFieldDataOffsets[m_nFieldCount] = nStartBit;
		++m_nFieldCount;
	}

	//called to read the path and offset information form the field path, adding them to the list
	bool ReadFieldPaths( bf_read *pBuf, CUtlVector< int > *pvecFieldPathBits = NULL ); // pvecFieldPathBits needed for DTI modes
	//called to seal a dynamic list, packing it into a compact memory block along with its actual value data
	void PackWithFieldData( void *pData, int nDataBits );
	void PackWithFieldData( bf_read &buf, int nDataBits );
	//---------------------------------------------------------------


	// Sets up reading from the m_pFieldData
	void StartReading( bf_read& bitReader ) const;
	void StartWriting( bf_write& bitWriter );

	//given an index (assumed in range), this will determine the size in bits of that field's data
	int GetFieldDataSizeInBits( int nField ) const		{ return GetFieldDataBitEndOffset( nField ) - GetFieldDataBitOffset( nField ); }
	
	static void DumpMemInfo();

	bool operator ==( const CSerializedEntity &other ) const
	{
		if ( this == &other )
			return true;

		if ( m_nFieldCount != other.m_nFieldCount )
			return false;

		if ( m_nFieldDataBits != other.m_nFieldDataBits )
			return false;

		if ( V_memcmp( m_pFieldData, other.m_pFieldData, Bits2Bytes( m_nFieldDataBits ) ) )
			return false;

		for ( int i = 0; i < m_nFieldCount; ++i )
		{
			if ( m_pFields[ i ] != other.m_pFields[ i ] )
				return false;
		}

		return true;
	}

#ifndef PARANOID_SERIALIZEDENTITY
// Hack to make these accessible to helper classes/functions when using PARANOID_SERIALIZEDENTITY
private:
#endif

	//constant used to designate when this entity is in a packed state or not
	static const uint16 knPacked = (uint16)0xFFFF;

	//the total number of fields that we have
	uint16	m_nFieldCount;
	//this is the internal count of how many fields we have preallocated, should always be >= field count. This will be knPacked if we are in a packed format
	uint16	m_nNumReservedFields;
	//the number of bits for our data
	uint32	m_nFieldDataBits;
	//the following fields are laid out in a memory block in the following order when packed, so only the first should be freed
	short	*m_pFields;
	uint32	*m_pFieldDataOffsets;
	uint8	*m_pFieldData;		

	//memory tracking information for development builds
	#ifdef PARANOID_SERIALIZEDENTITY
		char const *m_File;
		int		m_Line;
	#endif

	void Grow();
	void Pack( short *pFields, uint32 *pFieldDataOffsets, int fieldCount, uint32 nFieldDataBits, uint8 *pFieldData );

	CSerializedEntity( const CSerializedEntity &other );
	DISALLOW_OPERATOR_EQUAL( CSerializedEntity );

	DECLARE_FIXEDSIZE_ALLOCATOR_MT( CSerializedEntity );
};

class CSerializedEntityFieldIterator
{
public:
	CSerializedEntityFieldIterator( CSerializedEntity *pEntity ) : 
	  m_pEntity( pEntity )
	  , m_entityFieldCount( pEntity ? pEntity->GetFieldCount() : 0 )
	  , m_nFieldIndex( -1 )
	  , m_pCurrent( NULL )
	  {
		  m_Sentinel = PROP_SENTINEL;
	  }

	  const CFieldPath &First()
	  {
		  m_nFieldIndex = 0;
		  Update();
		  return GetField();
	  }

	  const CFieldPath *FirstPtr()
	  {
		  m_nFieldIndex = 0;
		  Update();
		  return GetFieldPtr();
	  }

	  const CFieldPath &Prev()
	  {
		  --m_nFieldIndex;
		  Update();
		  return GetField();
	  }

	  const CFieldPath *PrevPtr()
	  {
		  --m_nFieldIndex;
		  Update();
		  return GetFieldPtr();
	  }

	  const CFieldPath &Next()
	  {
		  ++m_nFieldIndex;
		  Update();
		  return GetField();
	  }

	  const CFieldPath *NextPtr()
	  {
		  ++m_nFieldIndex;
		  Update();
		  return GetFieldPtr();
	  }

	  bool IsValid() const
	  {
		  // Use unsigned math so there is only one comparison.
		  return (unsigned)m_nFieldIndex < (unsigned)m_entityFieldCount;
	  }

	  int GetIndex() const
	  {
		  return m_nFieldIndex;
	  }

	  CFieldPath SetIndex( int nFieldIndex )
	  {
		  m_nFieldIndex = nFieldIndex;
		  Update();
		  return GetField();
	  }

	  const CFieldPath &GetField() const
	  {
		  return *m_pCurrent;
	  }

	  const CFieldPath *GetFieldPtr() const
	  {
		  return m_pCurrent;
	  }

	  int GetOffset() const
	  {
		  return m_nOffset;
	  }

	  int	GetLength() const
	  {
		  return m_nNextOffset - m_nOffset;
	  }

	  int	GetNextOffset() const
	  {
		  return m_nNextOffset;
	  }

private:

	void Update()
	{
		if ( !IsValid() )
		{
			Assert( m_Sentinel == PROP_SENTINEL );
			m_pCurrent = &m_Sentinel;
			m_nOffset = m_nNextOffset = -1;
		}
		else
		{
			m_pCurrent		= &m_Current;
			m_pEntity->GetField( m_nFieldIndex, m_Current, &m_nOffset, &m_nNextOffset );
		}
	}

	CSerializedEntity	* const m_pEntity;
	// Cache the entity field count to avoid extra branches and pointer dereferences in IsValid()
	const int			m_entityFieldCount;
	int					m_nFieldIndex;

	CFieldPath			m_Sentinel;
	CFieldPath			*m_pCurrent;
	CFieldPath			m_Current;
	int					m_nOffset;
	int					m_nNextOffset;
};

#endif // SERIALIZEDENTITY_H

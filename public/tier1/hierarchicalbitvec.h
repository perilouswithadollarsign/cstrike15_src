//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef HIERARCHICAL_BIT_VEC_HDR
#define HIERARCHICAL_BIT_VEC_HDR


class CHierarchicalBitVector
{
public:
	CHierarchicalBitVector( ){}
	CHierarchicalBitVector( int nInitialBitCount )
	{
		if ( nInitialBitCount > 0 )
		{
			EnsureBitExists( nInitialBitCount - 1 );
		}
	}
	CHierarchicalBitVector( const CHierarchicalBitVector &other )
	{
		m_Level0.CopyArray( other.m_Level0.Base( ), other.m_Level0.Count( ) );
		m_Level1.CopyArray( other.m_Level1.Base( ), other.m_Level1.Count( ) );
	}


	void Set( int nBit )
	{
		m_Level0[ nBit >>  5 ] |= 1 << (   nBit & 31 );
		m_Level1[ nBit >> 10 ] |= 1 << ( ( nBit >> 5 ) & 31 );
	}
	void Reset( int nBit )
	{
		m_Level0[ nBit >> 5 ] &= ~( 1 << ( nBit & 31 ) );
	}

	uint32 operator[] ( int nBit ) const
	{
		uint32 nBitSet = ( m_Level0[ nBit >> 5 ] >> ( nBit & 31 ) ) & 1;
		AssertDbg( ( ( m_Level1[ nBit >> 10 ] >> ( ( nBit >> 5 ) & 31 ) ) & 1 ) || !m_Level0[ nBit >> 5 ] );
		return nBitSet;
	}
	const uint32 * Base() const
	{
		return m_Level0.Base();
	}

	void Validate()const
	{
#ifdef DBGFLAG_ASSERT
		for( int i = 0; i < m_Level0.Count(); ++i )
		{
			uint nLevel0Bits = m_Level0[ i ];
			uint nLevel1Bits = m_Level1[ i >> 5 ];
			Assert( nLevel1Bits & ( 1 << ( i & 31 ) ) ? nLevel0Bits : !nLevel0Bits );
		}
#endif
	}
	
	void EnsureBitExists( int nBit ) // reallocate if necessary
	{
		int nLevel0Size = ( nBit >> 5 ) + 1, nLevel0OldSize = m_Level0.Count();
		if( nLevel0OldSize < nLevel0Size )
		{
			m_Level0.AddMultipleToTail( nLevel0Size - nLevel0OldSize );
			for( int i = nLevel0OldSize; i < nLevel0Size; ++i )
			{
				m_Level0[i] = 0;
			}
			int nLevel1Size = ( nBit >> 10 ) + 1, nLevel1OldSize = m_Level1.Count();
			if( nLevel1OldSize < nLevel1Size )
			{
				m_Level1.AddMultipleToTail( nLevel1Size - nLevel1OldSize );
				for( int i = nLevel1OldSize; i < nLevel1Size; ++i )
				{
					m_Level1[i] = 0;
				}
			}
		}
	}

	int Count()
	{
		return m_Level0.Count() << 5;
	}

	template < typename Functor >
	void ScanBits( Functor functor )
	{
		for( int nItLevel1 = 0; nItLevel1 < m_Level1.Count(); ++nItLevel1 )
		{
			uint nLevel1Bits = m_Level1[ nItLevel1 ];
			uint nBaseLevel1 = nItLevel1 * 32;
			while( nLevel1Bits )
			{
#ifdef COMPILER_GCC
				uint32 nOffsetLevel1 = __builtin_ctz( nLevel1Bits );
#else
				unsigned long nOffsetLevel1;
				_BitScanForward( &nOffsetLevel1, nLevel1Bits );
#endif
				AssertDbg( nLevel1Bits & ( 1 << nOffsetLevel1 ) );
				nLevel1Bits ^= 1 << nOffsetLevel1;

				uint nItLevel0 = nBaseLevel1 + nOffsetLevel1;
				uint nLevel0Bits = m_Level0[ nItLevel0 ];
				if( nLevel0Bits != 0 ) // if the bit is set in level1, it doesn't mean the uint32 in level0 is always non-zero: it may have been reset
				{
					uint nBaseLevel0 = nItLevel0 * 32;
					do
					{
#ifdef COMPILER_GCC
						uint32 nOffsetLevel0 = __builtin_ctz( nLevel0Bits );
#else
						unsigned long nOffsetLevel0;
						_BitScanForward( &nOffsetLevel0, nLevel0Bits );
#endif
						AssertDbg( nLevel0Bits & ( 1 << nOffsetLevel0 ) );
						nLevel0Bits ^= 1 << nOffsetLevel0;
						// Perform tree queries for all moving objects
						functor( nBaseLevel0 + nOffsetLevel0 );
					}
					while( nLevel0Bits );
				}
			}
		}
	}
	void Clear( )
	{
		m_Level0.FillWithValue( 0 );
		m_Level1.FillWithValue( 0 );
	}
protected:
	CUtlVector< uint32 > m_Level1; // one bit for each 32 bits  in m_MoveBufferLevel0
	CUtlVector< uint32 > m_Level0; // one bit for each proxy

public:
#ifdef AUTO_SERIALIZE
	AUTO_SERIALIZE;
#endif
};




#endif

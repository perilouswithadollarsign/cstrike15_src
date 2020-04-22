//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======//
#ifndef INCREMENTAL_VECTOR_H
#define INCREMENTAL_VECTOR_H

// this class facilitates an array of pointers that get added and removed frequently;
// it's intrusive: the class that is pointed to must contain and maintain an index
#include "tier1/utlvector.h"

template < class CLASS >
struct DefaultIncrementalVectorAccessor_t
{
	static int GetIndex( const CLASS * p ) { return p->m_nIncrementalVectorIndex; }
	static void SetIndex( CLASS * p, int nIndex ) { p->m_nIncrementalVectorIndex = nIndex; }
};


template <class T, class Accessor = DefaultIncrementalVectorAccessor_t< T > , class Allocator = CUtlMemory< T* > >
class CUtlIncrementalVector
{
public:
	T** Base(){ return m_data.Base(); }
	T*const* Base()const{ return m_data.Base(); }
	typedef T* ElemType_t;
	typedef T** iterator;
	typedef T*const* const_iterator;
	enum { IsUtlVector = true }; // Used to match this at compiletime 

	iterator begin()						{ return Base(); }
	const_iterator begin() const			{ return Base(); }
	iterator end()							{ return Base() + Count(); }
	const_iterator end() const				{ return Base() + Count(); }

	void SetCountAndCreateOrDelete( int nNewCount )
	{
		int nOldCount = Count();
		for( int i = nNewCount; i < nOldCount; ++i )
		{
			delete m_data[i];
		}
		m_data.SetCount( nNewCount );
		for( int i = nOldCount; i < nNewCount; ++i )
		{
			m_data[i] = new T;
			Accessor::SetIndex( m_data[i], i );
		}
		Validate();
	}

	int Count() const
	{
		return m_data.Count();
	}

	T* operator[]( int i )
	{
		Assert( !m_data[i] || Accessor::GetIndex( m_data[i] ) == i );
		return m_data[i];
	}

	const T* operator[]( int i )const
	{
		Assert( !m_data[i] || Accessor::GetIndex( m_data[i] ) == i );
		return m_data[i];
	}

	void SetElementToNull( int i )
	{
		m_data[ i ] = NULL;
	}

	T* AddToTail( T* p )
	{
		Validate();
		Accessor::SetIndex( p, m_data.Count() );
		m_data.AddToTail( p );
		Validate();
		return p;
	}

	void MoveToHead( T *pMid )
	{
		Validate();
		int nMidIndex = Accessor::GetIndex( pMid );
		Accessor::SetIndex( pMid, 0 );
		T *pHead = m_data[0];
		Accessor::SetIndex( pHead, nMidIndex );
		m_data[0] = pMid;
		m_data[nMidIndex] = pHead;
		Validate();
	}

	void SwapItems( int i1, int i2 )
	{
		Validate();
		T *p1 = m_data[i1];
		T *p2 = m_data[i2];

		Accessor::SetIndex( p1, i2 );
		Accessor::SetIndex( p2, i1 );
		m_data[i1] = p2;
		m_data[i2] = p1;
		Validate();
	}

	void InsertNewMultipleBefore( int nIndex, int nCount )
	{
		if( nCount )
		{
			for( int i = nIndex; i < m_data.Count(); ++i )
			{
				T *pElement = m_data[i];
				Accessor::SetIndex( pElement, Accessor::GetIndex( pElement ) + nCount );
			}
			m_data.InsertMultipleBefore( nIndex, nCount );
			for( int i = 0; i < nCount; ++i )
			{
				T *p = new T;
				Accessor::SetIndex( p, nIndex + i );
				m_data[ nIndex + i ] = p;
			}
		}
	}
	void RemoveAndDeleteMultiple( int nIndex, int nCount )
	{
		if( nCount )
		{
			for( int i = nIndex, nEnd = MIN( nIndex + nCount, m_data.Count() ); i < nEnd; ++i )
			{
				delete m_data[i];
			}
			m_data.RemoveMultiple( nIndex, nCount );
			for ( int i = nIndex; i < m_data.Count(); ++i )
			{
				Accessor::SetIndex( m_data[ i ], i );
			}
		}
	}

	void RemoveMultiple( int nIndex, int nCount )
	{
		if ( nCount )
		{
			m_data.RemoveMultiple( nIndex, nCount );
			for ( int i = nIndex; i < Count(); ++i )
			{
				Accessor::SetIndex( m_data[ i ], i );
			}
		}
	}
	void RemoveMultipleFromHead( int num )
	{
		if ( num )
		{
			m_data.RemoveMultipleFromHead( num );
			for ( int i = 0; i < Count(); ++i )
			{
				Accessor::SetIndex( m_data[ i ], i );
			}
		}
	}

	void RemoveAndDelete( int nIndex )		///< preserves order, shifts elements
	{
		if( nIndex < Count() )
		{
			delete m_data[nIndex];
			for( int i = nIndex + 1; i < Count(); ++i )
			{
				Accessor::SetIndex( m_data[ i ], i - 1 );
				m_data[ i - 1 ] = m_data[ i ];
			}
			m_data.RemoveMultipleFromTail( 1 );
		}
	}

	void FastRemove( int elem )	// doesn't preserve order
	{
		Validate();
		//T* pDeletable = m_data[elem];
		T* pLast = m_data.Tail(); // Optimization Warning: pLast and pDelete pointers are aliased
		m_data[elem] = pLast;
		m_data.RemoveMultipleFromTail( 1 );
		Accessor::SetIndex( pLast, elem );
		// Accessor::SetIndex( pDeletable, -1 );   // this is not really necessary, except for debugging; there's also a danger that client deleted the object before calling FastRemove
		Validate();
	}
	T* FindAndFastRemove( T* p )   // removes first occurrence of src, doesn't preserve order
	{
		int nIndex = Accessor::GetIndex( p );
		Assert( m_data[nIndex] == p );
		FastRemove( nIndex );
		return p;
	}

	int Find( const T* p )const
	{
		int nIndex = Accessor::GetIndex( p );
		if ( uint( nIndex ) < uint( Count() ) && m_data[ nIndex ] == p )
			return nIndex;
		else
			return -1;
	}

	bool Has( const T *p )const
	{
		int nIndex = Accessor::GetIndex( p );
		return uint( nIndex ) < uint( Count() ) && m_data[ nIndex ] == p;
	}

	void FastRemoveAndDelete( T* p )
	{
		int nIndex = Accessor::GetIndex( p );
		FastRemove( nIndex );
		delete p;
	}

	void PurgeAndDeleteElements()
	{
		Validate();
		m_data.PurgeAndDeleteElements();
	}
	
	inline void Validate()
	{
		#if defined( DBGFLAG_ASSERT ) && defined( _DEBUG )
		for( int i =0 ;i < Count(); ++i )
		{
			Assert( !m_data[i] || Accessor::GetIndex( m_data[i] ) == i );
		}
		#endif
	}

	void Purge()
	{
		m_data.Purge();
	}

	bool IsEmpty()const
	{
		return m_data.IsEmpty();
	}

	T *Tail()
	{
		return m_data.Tail();
	}

	void EnsureCapacity( int nCap )
	{
		m_data.EnsureCapacity( nCap );
	}

	void Sort( int (__cdecl *pfnCompare)(const ElemType_t *, const ElemType_t *) )
	{
		m_data.Sort( pfnCompare );
		for( int i = 0; i < m_data.Count(); ++i )
		{
			Accessor::SetIndex( m_data[i], i );
		}
	}
protected:
	CUtlVector< T*, Allocator > m_data;	
};

#define DECLARE_INCREMENTAL_VECTOR_TYPE(CLASS, MEMBER)											\
	struct CLASS##_##NAME##_IncrementalVectorAccessor_t											\
	{																							\
		static int GetIndex( const CLASS * p ) { return p->MEMBER; }							\
		static void SetIndex( const CLASS * p, int nIndex ) { p->MEMBER = nIndex; }				\
	};																							\
	typedef CUtlIncrementalVector < CLASS, CLASS##_##NAME##_IncrementalVectorAccessor_t > 

#endif
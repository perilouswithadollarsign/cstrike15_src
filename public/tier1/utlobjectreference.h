//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef UTLOBJECTREFERENCE_H
#define UTLOBJECTREFERENCE_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlintrusivelist.h"
#include "mathlib/mathlib.h"
#include "tier1/utlvector.h"


// Purpose: class for keeping track of all the references that exist to an object.  When the object
// being referenced is freed, all of the references pointing at it will become null.
//
// To Use:
//   Add a DECLARE_REFERENCED_CLASS to the class that you want to use CutlReferences with.
//   Replace pointers to that class with CUtlReferences.
//   Check these references for null in appropriate places.
//
//  NOTE : You can still happily use pointers instead of references where you want to - these
//  pointers will not magically become null like references would, but if you know no one is going
//  to delete the underlying object during a partcular section of code, it doesn't
//  matter. Basically, CUtlReferences don't rely on every use of an object using one.




template<class T> class CUtlReference
{
public:
	FORCEINLINE CUtlReference(void)
	{
		m_pNext = m_pPrev = NULL;
		m_pObject = NULL;
	}
  
	FORCEINLINE CUtlReference(T *pObj)
	{
		m_pNext = m_pPrev = NULL;
		AddRef( pObj );
	}

	FORCEINLINE CUtlReference( const CUtlReference<T>& other ) 
	{
		CUtlReference();

		if ( other.IsValid() )
		{
			AddRef( (T*)( other.GetObject() ) );
		}
	}

	FORCEINLINE ~CUtlReference(void)
	{
		KillRef();
	}
  
	FORCEINLINE void Set(T *pObj)
	{
		if ( m_pObject != pObj )
		{
			KillRef();
			AddRef( pObj );
		}
	}
  
	FORCEINLINE T * operator()(void) const
	{
		return m_pObject;
	}

	FORCEINLINE bool IsValid( void) const
	{
		return ( m_pObject != NULL );
	}

	FORCEINLINE operator T*()
	{
		return m_pObject;
	}

	FORCEINLINE operator const T*() const
	{
		return m_pObject;
	}


	FORCEINLINE T * GetObject( void )
	{
		return m_pObject;
	}

	FORCEINLINE const T* GetObject( void ) const
	{
		return m_pObject;
	}


	FORCEINLINE T* operator->()
	{ 
		return m_pObject; 
	}

	FORCEINLINE const T* operator->() const
	{ 
		return m_pObject; 
	}

	FORCEINLINE CUtlReference &operator=( const CUtlReference& otherRef )
	{
		Set( otherRef.m_pObject );
		return *this;
	}

	FORCEINLINE CUtlReference &operator=( T *pObj )
	{
		Set( pObj );
		return *this;
	}


	FORCEINLINE bool operator==( T const *pOther ) const
	{
		return ( pOther == m_pObject );
	}	

	FORCEINLINE bool operator==( T *pOther ) const
	{
		return ( pOther == m_pObject );
	}	

	FORCEINLINE bool operator==( const CUtlReference& o ) const
	{
		return ( o.m_pObject == m_pObject );
	}	

public:
	CUtlReference *m_pNext;
	CUtlReference *m_pPrev;

	T *m_pObject;

	FORCEINLINE void AddRef( T *pObj )
	{
		m_pObject = pObj;
		if ( pObj )
		{
			pObj->m_References.AddToHead( this );
		}
	}

	FORCEINLINE void KillRef(void)
	{
		if ( m_pObject )
		{
			m_pObject->m_References.RemoveNode( this );
			m_pObject = NULL;
		}
	}
};

template<class T> class CUtlReferenceList : public CUtlIntrusiveDList< CUtlReference<T> >
{
public:
	~CUtlReferenceList( void )
	{
		CUtlReference<T> *i = CUtlIntrusiveDList<CUtlReference<T> >::m_pHead;
		while( i )
		{
			CUtlReference<T> *n = i->m_pNext;
			i->m_pNext = NULL;
			i->m_pPrev = NULL;
			i->m_pObject = NULL;
			i = n;
		}
		CUtlIntrusiveDList<CUtlReference<T> >::m_pHead = NULL;
	}
};


//-----------------------------------------------------------------------------
// Put this macro in classes that are referenced by CUtlReference
//-----------------------------------------------------------------------------
#define DECLARE_REFERENCED_CLASS( _className )				\
	private:												\
		CUtlReferenceList< _className > m_References;		\
		template<class T> friend class CUtlReference;


template < class T >
class CUtlReferenceVector : public CUtlBlockVector< CUtlReference< T > >
{
public:
	void RemoveAll()
	{
		for ( int i = 0; i < this->Count(); i++ )
		{
			this->Element( i ).KillRef();
		}

		CUtlBlockVector< CUtlReference< T > >::RemoveAll(); 
	}

	void FastRemove( int elem )
	{
		Assert( this->IsValidIndex(elem) );

		if ( this->m_Size > 0 )
		{
			if ( elem != this->m_Size -1 )
			{
				this->Element( elem ).Set( this->Element( this->m_Size - 1 ).GetObject() );
			}
			Destruct( &Element( this->m_Size - 1 ) );
			--this->m_Size;
		}
	}

	bool FindAndFastRemove( const CUtlReference< T >& src )
	{
		int elem = Find( src );
		if ( elem != -1 )
		{
			FastRemove( elem );
			return true;
		}
		return false;
	}

private:
	//
	// Disallow methods of CUtlBlockVector that can cause element addresses to change, thus
	// breaking assumptions of CUtlReference
	//
	void Remove( int elem );		
	bool FindAndRemove( const T& src );	
	void RemoveMultiple( int elem, int num );	
	void RemoveMultipleFromHead(int num); 
	void RemoveMultipleFromTail(int num); 
	void Swap( CUtlReferenceVector< T > &vec );
	void Purge();
	void PurgeAndDeleteElements();
	void Compact();
};

#endif






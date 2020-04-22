//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef ZONE_H
#define ZONE_H
#pragma once

#include "tier0/dbg.h"

void Memory_Init (void);
void Memory_Shutdown( void );

void Hunk_OnMapStart( int nEstimatedBytes );

void *Hunk_AllocName (int size, const char *name, bool bClear = true );

int	Hunk_LowMark (void);
void Hunk_FreeToLowMark (int mark);

void Hunk_Check (void);

int Hunk_MallocSize();
int Hunk_Size();

void Hunk_Print();

// Deal with memory attribution for CHunkMemory
#define HUNK_ALLOC_CREDIT_( _name_ )	MEM_ALLOC_CREDIT_( _name_ ); CHunkAllocCredit hunkAllocAttributeAlloction( _name_ );
class CHunkAllocCredit
{
public:
	 CHunkAllocCredit( const char *name )	{ PushAllocDbgInfo( name ); };
	 ~CHunkAllocCredit( void )				{ PopAllocDbgInfo(); };

	static void PushAllocDbgInfo( const char *name )
	{
		Assert( name && name[0] );
		++s_DbgInfoStackDepth;
		Assert( s_DbgInfoStackDepth < DBG_INFO_STACK_DEPTH );
		if ( s_DbgInfoStackDepth < DBG_INFO_STACK_DEPTH )
			s_DbgInfoStack[s_DbgInfoStackDepth] = ( name && name[0] ) ? name : "CHunkMemory";
	}
	static void PopAllocDbgInfo( void )
	{
		Assert( s_DbgInfoStackDepth >= 0 );
		if ( s_DbgInfoStackDepth >= 0 )
			s_DbgInfoStack[ s_DbgInfoStackDepth-- ] = NULL;
	}
	static const char *GetAllocDbgInfo( void )
	{
		int index = MIN( s_DbgInfoStackDepth, (DBG_INFO_STACK_DEPTH-1) );
		return ( index >= 0 ) ? s_DbgInfoStack[index] : "CHunkMemory";
	}

	static const int DBG_INFO_STACK_DEPTH = 8;
	static const char *s_DbgInfoStack[ DBG_INFO_STACK_DEPTH ];
	static int s_DbgInfoStackDepth;
};

template< typename T >
class CHunkMemory
{
public:
	// constructor, destructor
	CHunkMemory( int nGrowSize = 0, int nInitSize = 0 )		{ m_pMemory = NULL; m_nAllocated = 0; if ( nInitSize ) Grow( nInitSize ); }
	CHunkMemory( T* pMemory, int numElements )				{ Assert( 0 ); }

	// Can we use this index?
	bool IsIdxValid( int i ) const							{ return (i >= 0) && (i < m_nAllocated); }

	// Gets the base address
	T* Base()												{ return (T*)m_pMemory; }
	const T* Base() const									{ return (T*)m_pMemory; }

	// element access
	T& operator[]( int i )									{ Assert( IsIdxValid(i) ); return Base()[i];	}
	const T& operator[]( int i ) const						{ Assert( IsIdxValid(i) ); return Base()[i];	}
	T& Element( int i )										{ Assert( IsIdxValid(i) ); return Base()[i];	}
	const T& Element( int i ) const							{ Assert( IsIdxValid(i) ); return Base()[i];	}

	// Attaches the buffer to external memory....
	void SetExternalBuffer( T* pMemory, int numElements )	{ Assert( 0 ); }

	// Size
	int NumAllocated() const								{ return m_nAllocated; }
	int Count() const										{ return m_nAllocated; }

	// Grows the memory, so that at least allocated + num elements are allocated
	void Grow( int num = 1 )								{ Assert( !m_nAllocated ); m_pMemory = (T *)Hunk_AllocName( num * sizeof(T), CHunkAllocCredit::GetAllocDbgInfo(), false ); m_nAllocated = num; }

	// Makes sure we've got at least this much memory
	void EnsureCapacity( int num )							{ Assert( num <= m_nAllocated ); }

	// Memory deallocation
	void Purge()											{ m_nAllocated = 0; }

	// Purge all but the given number of elements (NOT IMPLEMENTED IN )
	void Purge( int numElements )							{ Assert( 0 ); }

	// is the memory externally allocated?
	bool IsExternallyAllocated() const						{ return false; }

	// Set the size by which the memory grows
	void SetGrowSize( int size )							{}

private:
	T *m_pMemory;
	int m_nAllocated;
};

#endif // ZONE_H

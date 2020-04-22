//--------------------------------------------------------------------------------------------------
// qhMemory.inl
//
// Copyright(C) 2011 by D. Gregorius. All rights reserved.
//--------------------------------------------------------------------------------------------------

#include "qhTypes.h"
#ifndef NULL
#define NULL 0
#endif

//--------------------------------------------------------------------------------------------------
// qhPool
//--------------------------------------------------------------------------------------------------
template < typename T > inline
qhPool< T >::qhPool( void )
	: mSize( 0 )
	, mPool( NULL )
	, mFree( -1 )
	{

	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
qhPool< T >::~qhPool( void )
	{
	qhFree( mPool );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhPool< T >::Clear( void )
	{
	mSize = 0;
	qhFree( mPool );
	mPool = NULL;
	mFree = -1;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhPool< T >::Resize( int Size )
	{
	QH_ASSERT( mSize == 0 && mPool == NULL );

	mSize = Size;
	mPool = (T*)qhAlloc( Size * sizeof( T ) );
	mFree = 0;

	for ( int i = 0; i < mSize - 1; ++i )
		{
		int* Next = (int*)( mPool + i );
		*Next = i + 1;
		}

	int* Next = (int*)( mPool + mSize - 1 );
	*Next = -1;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
T* qhPool< T >::Allocate( void )
	{
	QH_ASSERT( mFree >= 0 );
	int Next = mFree;
	mFree = *(int*)( mPool + mFree );

#ifdef QH_DEBUG
	memset( mPool + Next, 0xcd, sizeof( T ) );
#endif

	return mPool + Next;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhPool< T >::Free( T* Address )
	{
#ifdef QH_DEBUG
	QH_ASSERT( Address != NULL );
	memset( Address, 0xdb, sizeof( T ) );
#endif

	QH_ASSERT( mPool <= Address && Address < mPool + mSize );
	int* Next = (int*)Address;
	*Next = mFree;
	
	mFree = int( Address - mPool );
	QH_ASSERT( 0 <= mFree && mFree < mSize );
	}


//--------------------------------------------------------------------------------------------------
// Memory utilities
//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhConstruct( T* Address )
	{
	new ( Address ) T;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhConstruct( T* Address, int N )
	{
	// If n < 0 no objects are constructed (e.g. if shrinking an array)
	for ( int i = 0; i < N; ++i )
		{
		new ( Address + i ) T;
		}
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhCopyConstruct( T* Address, const T& Other )
	{
	new ( Address ) T( Other );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhDestroy( T* Address )
	{
	( Address )->~T();
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhDestroy( T* Address, int N )
	{
	// If n < 0 no objects are destroyed (e.g. if growing an array)
	for ( int i = 0; i < N; ++i ) 
		{
		( Address + i )->~T();
		}
	}


//-------------------------------------------------------------------------------------------------
template < typename T >
void qhMove( T* Address, T* Begin, T* End )
	{
	while ( Begin != End )
		{
		new ( Address ) T;
		qhSwap( *Address++, *Begin++ );
		}
	}


//-------------------------------------------------------------------------------------------------
template <typename T> inline
void qhSwap( T& Lhs, T& Rhs )
	{
	T Temp = Lhs;
	Lhs = Rhs;
	Rhs = Temp;
	}


//--------------------------------------------------------------------------------------------------
inline const void* qhAddByteOffset( const void* Address, size_t Bytes )
{
	return reinterpret_cast< const unsigned char* >( Address ) + Bytes;
}

//--------------------------------------------------------------------------------------------------
/**
	@file		qhMemory.h

	@author		Dirk Gregorius
	@version	0.1
	@date		30/11/2011

	Copyright(C) 2011 by D. Gregorius. All rights reserved.
*/
//--------------------------------------------------------------------------------------------------
#pragma once

#include <new>

//--------------------------------------------------------------------------------------------------
// qhMemory
//--------------------------------------------------------------------------------------------------
extern void* (*qhAllocHook)( std::size_t );
extern void (*qhFreeHook)( void* );

void* qhAlloc( std::size_t Bytes );
void qhFree( void* Address );


//--------------------------------------------------------------------------------------------------
// qhPool
//--------------------------------------------------------------------------------------------------
template < typename T >
class qhPool
	{
	public:
		qhPool( void );
		~qhPool( void );

		void Clear( void );
		void Resize( int Size );
		T* Allocate( void );
		void Free( T* Address );

	private:
		int mSize;
		T* mPool;
		int mFree;

		// Non-copyable
		qhPool( const qhPool& );
		qhPool& operator=( const qhPool& );
	};


//--------------------------------------------------------------------------------------------------
// Memory utilities
//--------------------------------------------------------------------------------------------------
template < typename T >
void qhConstruct( T* Address );

template < typename T >
void qhConstruct( T* Address, int N );

template < typename T >
void qhCopyConstruct( T* Address, const T& Other );

template < typename T >
void qhDestroy( T* Address );

template < typename T >
void qhDestroy( T* Address, int N );

template < typename T >
void qhMove( T* address, T* Begin, T* End );

template < typename T >
void qhSwap( T& Lhs, T& Rhs );

const void* qhAddByteOffset( const void* Address, std::size_t Bytes );


#include "qhMemory.inl"
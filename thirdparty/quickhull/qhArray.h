//--------------------------------------------------------------------------------------------------
/**
	@file		qhArray.h

	@author		Dirk Gregorius
	@version	0.1
	@date		03/12/2011

	Copyright(C) 2011 by D. Gregorius. All rights reserved.
*/
//--------------------------------------------------------------------------------------------------
#pragma once

#include "qhTypes.h"
#include "qhMemory.h"


//--------------------------------------------------------------------------------------------------
// qhArray
//--------------------------------------------------------------------------------------------------
template < typename T >
class qhArray
	{
	public:
		qhArray( void );
		~qhArray( void );

		int Capacity( void ) const;
		int Size( void ) const;
		bool Empty( void ) const;

		void Clear( void );
		void Reserve( int Count );
		void Resize( int Count );
		
		T& Expand( void );
		void PushBack( const T& Other );
		void PopBack( void );

		int IndexOf( const T& Element ) const;

		T& operator[]( int Offset );
		const T& operator[]( int Offset ) const;

		T& Front( void );
		const T& Front( void ) const;
		T& Back( void );
		const T& Back( void ) const;

		T* Begin( void );
		const T* Begin( void ) const;
		T* End( void );
		const T* End( void ) const;

		void Swap( qhArray< T >& Other );

	private:
		T* mBegin;
		T* mEnd;
		T* mCapacity;

		// Non-copyable
		qhArray( const qhArray& );
		qhArray& operator=( const qhArray& );
	};


template < typename T >
void qhSwap( qhArray< T >& Lhs, qhArray< T >& Rhs );


#include "qhArray.inl"
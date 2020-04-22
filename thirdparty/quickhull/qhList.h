//--------------------------------------------------------------------------------------------------
/**
	@file		qhList.h

	@author		Dirk Gregorius
	@version	0.1
	@date		30/11/2011

	Copyright(C) 2011 by D. Gregorius. All rights reserved.
*/
//--------------------------------------------------------------------------------------------------
#pragma once


//--------------------------------------------------------------------------------------------------
// qhListNode	
//--------------------------------------------------------------------------------------------------
template < typename T >
void qhInsert( T* Node, T* Where );

template < typename T >
void qhRemove( T* Node );

template < typename T >
bool qhInList( T* Node );


//--------------------------------------------------------------------------------------------------
// qhList	
//--------------------------------------------------------------------------------------------------
template < typename T >
class qhList
	{
	public:
		qhList( void );

		int Size( void ) const;
		bool Empty( void ) const;

		void Clear( void );
		void PushFront( T* Node );
		void PopFront( void );
		void PushBack( T* Node );
		void PopBack( void );

		void Insert( T* Node, T* Where );
		void Remove( T* Node );
		int IndexOf( const T* Node ) const;
		
		T* Begin( void );
		const T* Begin( void ) const;
		T* End( void );
		const T* End( void ) const;

	private:
		T mHead;

		// Non-copyable
		qhList( const qhList< T >& );
		qhList< T >& operator=( const qhList< T >& );
	};



#include "qhList.inl"
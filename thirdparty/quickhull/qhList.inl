//--------------------------------------------------------------------------------------------------
// qhList.inl
//
// Copyright(C) 2011 by D. Gregorius. All rights reserved.
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
// qhListNode
//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhInsert( T* Node, T* Where )
	{
	QH_ASSERT( !qhInList( Node ) && qhInList( Where ) );

	Node->Prev = Where->Prev;
	Node->Next = Where;

	Node->Prev->Next = Node; 
	Node->Next->Prev = Node;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhRemove( T* Node )
	{
	QH_ASSERT( qhInList( Node ) );

	Node->Prev->Next = Node->Next; 
	Node->Next->Prev = Node->Prev;

	Node->Prev = NULL;
	Node->Next = NULL;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
bool qhInList( T* Node )
	{
	return Node->Prev != NULL && Node->Next != NULL;
	}


//--------------------------------------------------------------------------------------------------
// qhList
//--------------------------------------------------------------------------------------------------
template < typename T > inline
qhList< T >::qhList( void ) 
	{
	mHead.Prev = &mHead;
	mHead.Next = &mHead;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
int qhList< T >::Size( void ) const
	{
	int Count = 0;
	for ( const T* Node = Begin(); Node != End(); Node = Node->Next )
		{
		Count++;
		}

	return Count;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
bool qhList< T >::Empty( void ) const
	{
	return mHead.Next == &mHead;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhList< T >::Clear( void ) 
	{
	mHead.Prev = &mHead;
	mHead.Next = &mHead;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhList< T >::PushFront( T* Node )
	{
	qhInsert( Node, mHead.Next );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhList< T >::PopFront( void )
	{
	QH_ASSERT( !Empty() );

	T* Node = mHead.Next;
	qhRemove( Node );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhList< T >::PushBack( T* Node )
	{
	qhInsert( Node, mHead.Prev );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhList< T >::PopBack( void )
	{
	QH_ASSERT( !Empty() );

	T* Node = mHead.Prev;
	qhRemove( Node );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhList< T >::Insert( T* Node, T* Where )
	{
	qhInsert( Node, Where );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhList< T >::Remove( T* Node )
	{
	qhRemove( Node );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
int qhList< T >::IndexOf( const T* Node ) const
	{
	int Index = 0;
	for ( const T* First = Begin(); First != End(); First = First->Next )
		{
		if ( First == Node )
			{
			return Index;
			}

		Index++;
		}

	return -1;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
T* qhList< T >::Begin( void )
	{
	return mHead.Next;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
const T* qhList< T >::Begin( void ) const 
	{
	return mHead.Next;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
T* qhList< T >::End( void )
	{
	return &mHead;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
const T* qhList< T >::End( void ) const
	{
	return &mHead;
	}











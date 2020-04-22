//--------------------------------------------------------------------------------------------------
// qhArray.inl
//
// Copyright(C) 2011 by D. Gregorius. All rights reserved.
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
// qhArray
//--------------------------------------------------------------------------------------------------
template < typename T > inline
qhArray< T >::qhArray( void )
	: mBegin( NULL )
	, mEnd( NULL )
	, mCapacity( NULL )
	{

	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
qhArray< T >::~qhArray( void )
	{
	qhDestroy( mBegin, Size() );
	qhFree( mBegin );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
int qhArray< T >::Capacity( void ) const
	{
	return int( mCapacity - mBegin );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
int qhArray< T >::Size( void ) const
	{
	return int( mEnd - mBegin );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
bool qhArray< T >::Empty( void ) const
	{
	return mEnd == mBegin;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhArray< T >::Clear( void )
	{
	qhDestroy( mBegin, Size() );
	mEnd = mBegin;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhArray< T >::Reserve( int Count )
	{
	if ( Count > Capacity() )
		{
		T* Begin = (T*)qhAlloc( Count * sizeof( T ) );
		qhMove( Begin, mBegin, mEnd );
		qhFree( mBegin );

		mCapacity = Begin + Count;
		mEnd = Begin + Size();
		mBegin = Begin;
		}
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhArray< T >::Resize( int Count )
	{
	Reserve( Count );

	qhDestroy( mBegin + Count, Size() - Count );
	qhConstruct( mEnd, Count - Size() );
	mEnd = mBegin + Count;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
T& qhArray< T >::Expand( void )
	{
	if ( mEnd == mCapacity  )
		{
		Reserve( 2 * Capacity() + 1 );
		}

	qhConstruct( mEnd );
	return *mEnd++;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhArray< T >::PushBack( const T& Other )
	{
	if ( mEnd == mCapacity )
		{
		Reserve( 2 * Capacity() + 1 );
		}

	qhCopyConstruct( mEnd++, Other );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhArray< T >::PopBack( void )
	{
	qhDestroy( --mEnd );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
int qhArray< T >::IndexOf( const T& Element ) const
	{
	for ( int i = 0; i < Size(); ++i )
		{
		if ( mBegin[ i ] == Element )
			{
			return i;
			}
		}

	return -1;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
T& qhArray< T >::operator[]( int Offset )
	{
	QH_ASSERT( 0 <= Offset && Offset < Size() );
	return *( mBegin + Offset );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
const T& qhArray< T >::operator[]( int Offset ) const
	{
	QH_ASSERT( 0 <= Offset && Offset < Size() );
	return *( mBegin + Offset );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
T& qhArray< T >::Front( void )
	{
	QH_ASSERT( !Empty() );
	return *mBegin;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
const T& qhArray< T >::Front( void ) const
	{
	QH_ASSERT( !Empty() );
	return *mBegin;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
T& qhArray< T >::Back( void )
	{
	QH_ASSERT( !Empty() );
	return *( mEnd - 1 );
	}	


//--------------------------------------------------------------------------------------------------
template < typename T > inline
T* qhArray< T >::Begin( void )
	{
	return mBegin;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
const T* qhArray< T >::Begin( void ) const
	{
	return mBegin;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
T* qhArray< T >::End( void )
	{
	return mEnd;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
const T* qhArray< T >::End( void ) const
	{
	return mEnd;
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
const T& qhArray< T >::Back( void ) const
	{
	QH_ASSERT( !Empty() );
	return *( mEnd - 1 );
	}


//--------------------------------------------------------------------------------------------------
template < typename T > inline
void qhArray< T >::Swap( qhArray< T >& Other )
	{
	qhSwap( mBegin, Other.mBegin );
	qhSwap( mEnd, Other.mEnd );
	qhSwap( mCapacity, Other.mCapacity );
	}


//--------------------------------------------------------------------------------------------------
template < typename T >
void qhSwap( qhArray< T >& Lhs, qhArray< T >& Rhs )
	{
	Lhs.Swap( Rhs );
	}



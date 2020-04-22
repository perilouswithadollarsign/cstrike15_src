//===== Copyright © Valve Corporation, All rights reserved. ======//
//
// Purpose: Resource Stream is conceptually memory file where you write the data
//          that you want to live both on disk and in memory. The constraint then is
//          that the data must be movable in memory, since you don't know in advance,
//          when you're creating the data, what will be its start address once it gets
//          loaded. To accomplish this, we use CResourcePointer and CResourceArray
//          in places of pointers and dynamic arrays. They store offsets rather than
//          absolute addresses of the referenced objects, thus they are moveable.
//          On Intel, the available addressing modes make the access to CResourcePointer
//          as fast as pointer dereferencing in most cases. On architectures with
//          rigid addressing, adding the base address is still virtually free.
//
// $NoKeywords: $
//===========================================================================//

#include "resourcefile/schema.h"

#ifndef RESOURCESTREAM_H
#define RESOURCESTREAM_H


#ifdef COMPILER_MSVC
#pragma once
#endif

#include "tier0/platform.h"
#include "tier0/basetypes.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"

class CResourceStream;


inline byte *ResolveOffset( const int32 *pOffset )
{
	int offset = *pOffset;
	return offset ? ( ( byte* )pOffset ) + offset : NULL;
}

inline byte *ResolveOffsetFast( const int32 *pOffset )
{
	int offset = *pOffset;
	AssertDbg( offset != 0 );
	return ( ( byte* )pOffset ) + offset;
}


template <typename T>
class CLockedResource
{
private:
	T *m_pData;	 // data; may be const
	uint m_nCount; // number of allocated data elements
	//uint m_nStride; // normally sizeof(T), but may be not
	//uint m_nClassCRC;
public:
	CLockedResource(): m_pData( NULL ), m_nCount( 0 )  {}
	CLockedResource( T *pData, uint nCount ): m_pData( pData ), m_nCount( nCount )  {}

	// emulates pointer arithmetics
	CLockedResource<T> operator + ( int nOffset ) { Assert( m_nCount <= (uint)nOffset); return CLockedResource<T>( m_pData + nOffset, m_nCount - (uint)nOffset ); }

	operator const T* ()const { return m_pData; }
	operator T* () { return m_pData; }

	T* operator ->() { return m_pData; }
	const T* operator ->() const { return m_pData; }

	uint Count()const {return m_nCount;}
};



template <typename T>
class CUnlockedResource
{
private:
	CResourceStream *m_pStream;	 // data; may be const
	uint32 m_nOffset;
	uint32 m_nCount; // number of allocated data elements
	//uint m_nStride; // normally sizeof(T), but may be not
	//uint m_nClassCRC;
public:
	CUnlockedResource( ): m_pStream( NULL ), m_nOffset( 0 ), m_nCount( 0 )  {}
	CUnlockedResource( CResourceStream *pStream, T *pData, uint nCount );
	bool IsValid( )const { return m_pStream != NULL;  }
	void Reset( ) { m_pStream = NULL; }

	// emulates pointer arithmetics
	CUnlockedResource<T> operator + ( int nOffset )
	{
		Assert( m_nCount <= ( uint ) nOffset );
		return CUnlockedResource<T>( m_pStream, GetPtr() + nOffset, m_nCount - ( uint ) nOffset );
	}

	operator const T* ( )const { return GetPtr(); }
	operator T* ( ) { return GetPtr(); }

	T* operator ->( ) { return GetPtr(); }
	const T* operator ->( ) const { return GetPtr(); }

	uint Count( )const { return m_nCount; }
	T* GetPtr( );
	const T* GetPtr( )const;
};




// AT RUN-TIME ONLY the offset converts automatically into pointers to the appropritate type
// at tool-time, you should use LinkSource_t and LinkTarget_t
class CResourcePointerBase
{
protected:
	int32 m_nOffset;
public:
	CResourcePointerBase() : m_nOffset( 0 ) {}

	bool operator == ( int zero )const
	{
		AssertDbg( zero == 0 );
		return m_nOffset == zero;
	}

	bool IsNull()const
	{
		return m_nOffset == 0;
	}

	int32 NotNull()const
	{
		return m_nOffset;
	}

	int32 GetOffset() const
	{
		return m_nOffset;
	}

	const byte* GetUncheckedRawPtr() const
	{
		// assumes non-null; returns garbage if this is a null pointer
		return ResolveOffsetFast( &m_nOffset );
	}

	byte* GetUncheckedRawPtr()
	{
		// assumes non-null; returns garbage if this is a null pointer
		return ResolveOffsetFast( &m_nOffset );
	}

	const byte* GetRawPtr() const
	{
		return ResolveOffset( &m_nOffset );
	}

	byte* GetRawPtr()
	{
		return ResolveOffset( &m_nOffset );
	}

	void SetRawPtr( const void* p )
	{
		if ( p == NULL )
		{
			m_nOffset = 0;
		}
		else
		{
			intp nOffset = ( intp )p - ( intp )&m_nOffset;
			m_nOffset = ( int32 )nOffset;
			AssertDbg( m_nOffset == nOffset );
		}
	}

	void SetNull()
	{
		m_nOffset = 0;
	}
};

template <typename T>
class CResourcePointer: public CResourcePointerBase
{
public:
	FORCEINLINE const T* GetUncheckedPtr() const
	{
		// assumes non-null; returns garbage if this is a null pointer
		AssertDbg( m_nOffset != 0 );
		byte *ptr = ResolveOffsetFast( &m_nOffset );
		return ( const T* )ptr;
	}

	FORCEINLINE const T* GetPtr() const
	{
		byte *ptr = ResolveOffset( &m_nOffset );
		return ( const T* )ptr;
	}

	FORCEINLINE operator const T*() const
	{
		return GetPtr();
	}

	FORCEINLINE const T* operator->() const
	{
		return GetPtr();
	}

	void operator = ( const T* pT )
	{
		SetRawPtr( pT );
	}

	void SetPtr( const T* pT )
	{
		SetRawPtr( pT );
	}

	// FIXME:	Should these be in a 'CResourceWritablePointer' subclass?
	//			There are plenty of cases where we know that a CResourcePointer should be read-only

public:
	FORCEINLINE T* GetUncheckedPtr()
	{
		// assumes non-null; returns garbage if this is a null pointer
		Assert( m_nOffset != 0 );
		byte *ptr = ResolveOffsetFast( &m_nOffset );
		return ( T* )ptr;
	}

	FORCEINLINE T* GetPtr()
	{
		byte *ptr = ResolveOffset( &m_nOffset );
		return ( T* )ptr;
	}

	FORCEINLINE operator T*()
	{
		return GetPtr();
	}

	FORCEINLINE T* operator->()
	{
		return GetPtr();
	}

public:
	void Unsafe_OutOfBoundsAllocate()
	{
		SetPtr( new T() );
	}

	void Unsafe_OutOfBoundsFree()
	{
		delete GetPtr();
	}
	
	void SwapBytes()
	{
		m_nOffset = DWordSwapC( m_nOffset );
	}
};

// never construct this - only the Resource stream may construct or load data that contains ResourceArray or ResourcePointer; use LockedResource or naked pointer or CUtlBuffer or something like that instead

class CResourceArrayBase
{
public:
	CResourceArrayBase()
	{
		m_nOffset = 0;
		m_nCount = 0;
	}

	bool operator == ( int zero )const
	{
		AssertDbg( zero == 0 );
		return m_nOffset == zero;
	}

	bool IsNull()const
	{
		return m_nOffset == 0;
	}

	int32 NotNull()const
	{
		return m_nOffset;
	}

	const byte* GetRawPtr() const
	{
		// validate
		return ResolveOffset( &m_nOffset );
	}

	byte* GetRawPtr()
	{
		// validate
		return ResolveOffset( &m_nOffset );
	}

	int Count() const
	{
		return m_nCount;
	}

	void WriteDirect( int nCount, void *pData )
	{
		if ( pData == NULL )
		{
			AssertDbg( nCount == 0 );
			m_nOffset = 0;
			m_nCount = 0;
		}
		else
		{
			m_nOffset = ((intp)pData) - (intp)&m_nOffset;
			m_nCount = nCount;
		}
	}

	void SwapMemberBytes()
	{
		m_nCount = DWordSwapC( m_nCount );
		m_nOffset = DWordSwapC( m_nOffset );
	}

private:
	CResourceArrayBase(const CResourceArrayBase&rThat ){} // private: we don't want to recompute offsets every time we copy a structure
protected:
	int32 m_nOffset;
	uint32 m_nCount;
};

template < typename T >
class CResourceArray: public CResourceArrayBase
{
public:
	CResourceArray(): CResourceArrayBase() {}

private:
	CResourceArray( const CResourceArray<T>&rThat ){} // private: we don't want to recompute offsets every time we copy a structure
public:
	const T& operator []( int nIndex ) const
	{
		AssertDbg( (uint)nIndex < m_nCount );
		return this->GetPtr()[nIndex];
	}

	T& operator []( int nIndex ) 
	{
		AssertDbg( (uint)nIndex < m_nCount );
		return this->GetPtr()[nIndex];
	}

	const T& Element( int nIndex ) const
	{
		AssertDbg( (uint)nIndex < m_nCount );
		return this->GetPtr()[nIndex];
	}

	T& Element( int nIndex ) 
	{
		AssertDbg( (uint)nIndex < m_nCount );
		return this->GetPtr()[nIndex];
	}

	CResourceArray<T>& operator = ( const CLockedResource<T> & lockedResource )
	{
		m_nOffset = ( lockedResource.Count() ) ? ( ((intp)(const T*)lockedResource) - (intp)&m_nOffset ) : 0;
		m_nCount = lockedResource.Count();
		return *this;
	}

	CResourceArray<T>& operator = ( const CResourceArray<T> & that )
	{
		m_nOffset = ( that.Count() ) ? ( ((intp)(const T*)that.GetPtr()) - (intp)&m_nOffset ) : 0;
		m_nCount = that.Count();
		return *this;
	}

	const T* GetPtr() const
	{
		return ( const T* )GetRawPtr();
	}

	T* GetPtr()
	{
		return ( T* )GetRawPtr();
	}

	const T* Base() const
	{
		return ( const T* )GetRawPtr();
	}

	T* Base()
	{
		return ( T* )GetRawPtr();
	}

	bool IsEmpty() const
	{
		return m_nCount == 0;
	}

	operator CLockedResource<T> () {return CLockedResource<T>( GetPtr(), Count() ) ; }

	// Temporary functions

	void Unsafe_OutOfBoundsAllocate( int nCount )
	{
		const T* pAlloc = new T[nCount];
		m_nOffset = ((intp)(const T*)pAlloc) - (intp)&m_nOffset;
		m_nCount = nCount;
	}

	void Unsafe_OutOfBoundsPurgeAndFreeRPs()
	{
		for ( uint32 i = 0; i < m_nCount; ++i )
		{
			Element(i).Unsafe_OutOfBoundsFree();
		}

		delete[] GetPtr();
	}

	// Remove all the NULL pointers from this resource array and shorten it (without changing any allocations)
	// (assumes that the elements of the array can be equality-tested with NULL)
	inline void CoalescePointerArrayInPlace()
	{
		int nValidElements = 0;
		int nArrayLen = Count();
		T* pBase = GetPtr();

		for ( int i = 0; i < nArrayLen; ++i )
		{
			if ( pBase[i] == 0 )
				continue;

			if ( nValidElements != i )
			{
				pBase[nValidElements].SetRawPtr( pBase[i].GetRawPtr() );
			}

			nValidElements++;
		}

		m_nCount = nValidElements;
	}

	// Enable support for range-based loops in C++ 11
	inline T* begin()
	{
		return GetPtr();
	}

	inline const T* begin() const
	{
		return GetPtr( );
	}

	inline T* end()
	{
		return GetPtr() + m_nCount;
	}

	inline const T* end() const
	{
		return GetPtr( ) + m_nCount;
	}
};

//////////////////////////////////////////////////////////////////////////
// this class may be useful at runtime to use fast serialize interface (without data linking)
//
class CResourceStream
{
public:
	// Constructor, destructor
	CResourceStream( );
	virtual ~CResourceStream( ) {} // make sure to implement proper cleanup in derived classes

	// Methods used to allocate space in the stream
	// just use Allocate<uint8>(100) to simply allocate 100 bytes of crap
	void *AllocateBytes( uint nCount );
	template <typename T> CLockedResource<T> Allocate( uint count = 1 ); 
	template <typename T> CUnlockedResource<T> AllocateUnaligned( uint count = 1 ); 

	// Methods that write data into the stream
	template <typename T> CLockedResource<T> Write( const T &x );
	template <typename T> CUnlockedResource<T> WriteUnaligned( const T &x );
	CLockedResource<float> WriteFloat( float x );
	CLockedResource<uint64> WriteU64( uint64 x );
	CLockedResource<uint32> WriteU32( uint32 x );
	CLockedResource<uint16> WriteU16( uint16 x );
	CLockedResource<uint8> WriteByte( byte x );
	CLockedResource<char> WriteString( const char *pString );
	CLockedResource<char> WriteStringMaxLen( const char *pString, int nMaxLen ); // will never read beyond pString[nMaxLen-1] and will force-null-terminate if necessary

	// Methods to force alignment of the next data written into the stream
	void Align( uint nAlignment, int nOffset = 0 );
	void AlignPointer();

	// How much data have we written into the stream?
	uint32 Tell() const;
	uint32 Tell( const void * pPast )const;
	const void *TellPtr() const;
	void Rollback( uint32 nPreviousTell );
	void Rollback( const void *pPreviousTell );
	
	void* Compile( );
	template <class Memory>
	void* CompileToMemory( Memory &memory );	
	uint GetTotalSize() const;
	void Barrier() {} // no pointers crossing barriers allowed! all pointers are invalidated
	void PrintStats();
	void ClearStats();

	// This is the max alignment ever used within the stream
	uint GetStreamAlignment() const;
	
	void* GetDataPtr( uint Offset = 0 );
	template <typename T> T* GetDataPtrTypeAligned( uint Offset );

	void Clear();

protected:
	void EnsureAvailable( uint nAddCapacity )
	{
		Assert( nAddCapacity < 0x40000000 ); // we don't support >1Gb of data yet
		uint nNewCommit = m_nUsed + nAddCapacity;
		if ( nNewCommit > m_nCommitted )
		{
			Commit( nNewCommit ); // Commit is only called when necessary, as it may be expensive
		}
	}
	// the only logic entry the derived class needs to reimplement, besides constructor+destructor pair. 
	// depending on how expensive it is to call this function, it may commit (allocate) large chunks of extra memory, that's totally ok
	virtual void Commit( uint nNewCommit ) = 0;

	template <typename T> friend class CUnlockedResource;

protected:
	uint8 *m_pData;       // the reserved virtual address space address; or just the start of allocated block of memory if virtual memory is not being used
	
	uint m_nCommitted; // how much memory was already committed (or allocated, in case no virtual memory is being used)
	uint m_nUsed;   // this is the amount of currently used/allocated data, in bytes; may be unaligned

	uint m_nAlignBits; // the ( max alignment - 1 ) of the current block of data
	uint m_nMaxAlignment;
};


class CResourceStreamVM: public CResourceStream
{
public:
	// Constructor, destructor
	CResourceStreamVM( uint nReserveSize = 16 * 1024 * 1024 );
	virtual ~CResourceStreamVM( ) OVERRIDE;
	void CloneStream( CResourceStreamVM& copyFromStream );
protected:
	virtual void Commit( uint nNewCommit ) OVERRIDE;
	void ReserveVirtualMemory( uint nAddressSize );
	void ReleaseVirtualMemory( );
	enum
	{
		COMMIT_STEP = 64 * 1024
	};
protected:
	uint m_nReserved; // the reserved virtual address space for the block of data
};


// use this class to create resources efficiently in runtime using your own allocator
// supply a fixed buffer; it belongs to the caller
class CResourceStreamFixed: public CResourceStream
{
private:
	bool m_bOwnMemory;
public:
	CResourceStreamFixed( uint nPreallocateDataSize );// NOTE: the preallocated data is Zeroed in this constructor
	CResourceStreamFixed( void *pPreallocatedData, uint nPreallocatedDataSize ); // NOTE: the preallocated data is Zeroed in this constructor
	virtual ~CResourceStreamFixed() OVERRIDE;

	int GetSlack(); // remaining bytes
	virtual void Commit( uint nNewCommit );
};

class CResourceStreamGrowable: public CResourceStream
{
public:
	CResourceStreamGrowable( uint nReserveDataSize );
	~CResourceStreamGrowable( );

	virtual void Commit( uint nNewCommit );
	uint8 *Detach( )
	{
		uint8 *pData = m_pData;
		m_pData = 0;
		m_nCommitted = 0;
		m_nUsed = 0;
		m_pData = NULL;
		return pData;
	}
};


class CLockedResourceAutoAggregator
{
protected:
	CResourceStreamVM *m_pStream;
	const void * m_pTellStart;
	uint m_nTellStart;
public:
	CLockedResourceAutoAggregator( CResourceStreamVM *pStream, const void * pTellStart )
	{
		m_pStream = pStream;
		m_pTellStart = pTellStart;
		m_nTellStart = pStream->Tell( pTellStart );
	}

	CLockedResource<uint8> GetAggregate()
	{
		return CLockedResource<uint8>( ( uint8* )m_pTellStart, m_pStream->Tell() - m_nTellStart );
	}
};

//-----------------------------------------------------------------------------
// Specialization for strings
//-----------------------------------------------------------------------------

class CResourceString: public CResourcePointer<char>
{
public:
	CResourceString& operator = ( const CLockedResource<char> & lockedCharResource )
	{
		SetPtr( (const char*)lockedCharResource );
		return *this;
	}

	CResourceString& operator = ( const CResourcePointerBase & lockedCharResource )
	{
		SetPtr( ( const char* )lockedCharResource.GetRawPtr() );
		return *this;
	}

	// empty strings can be serialized as a null pointer (instead of a pointer to  '\0')

	const char* GetPtr() const
	{
		if ( GetRawPtr() == NULL )
		{
			return "";
		}
		else
		{
			return ( const char* )GetRawPtr();
		}
	}

	char* GetPtr()
	{
		if ( GetRawPtr() == NULL )
		{
			return ( char* )"";
		}
		else
		{
			return ( char* )GetRawPtr();
		}
	}

	operator const char* ()const
	{
		if ( GetRawPtr() == NULL )
		{
			return "";
		}
		else
		{
			return ( char* )GetRawPtr();
		}
	}

	void Unsafe_OutOfBoundsCopy( const char* pStr )
	{
		int nLen = V_strlen(pStr);
		char* pAlloc = new char[nLen+1];
		V_strcpy( pAlloc, pStr );
		SetPtr( pAlloc );
	}

	bool IsEmpty() const
	{
		return IsNull() || !*GetUncheckedRawPtr();
	}


private:
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// Disable comparison operators for ResourceStrings, otherwise they'll
	// be implicitly converted to char* (we don't want to actually implement
	// these because it's ambiguous whether it's case-sensitive or not.
	bool operator==( const CResourceString &rhs ) const;
	bool operator!=( const CResourceString &rhs ) const;
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
};

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
template <typename T>
inline CLockedResource<T> CResourceStream::Allocate( uint nCount )
{
	// TODO: insert reflection code here
	uint nAlignment = VALIGNOF( T );
	Align( nAlignment );
	T *ptr = AllocateUnaligned< T >( nCount );
	// Construct
	for ( uint i = 0; i < nCount; i++ )
	{
		Construct< T >( &ptr[ i ] );
	}

	return CLockedResource<T>( ptr, nCount );
}

template <typename T>
inline CUnlockedResource<T> CResourceStream::AllocateUnaligned( uint nCount )
{
	// Allocate
	return CUnlockedResource< T >( this, ( T* )AllocateBytes( nCount * sizeof( T ) ), nCount );
}

inline void CResourceStream::AlignPointer()
{
	Align( sizeof( intp ) );
}

template <typename T>
inline CUnlockedResource<T> CResourceStream::WriteUnaligned( const T &x )
{
	CUnlockedResource<T> pMemory = AllocateUnaligned<T>( );
	V_memcpy( pMemory, &x, sizeof( T ) );
	return pMemory;
}


template <typename T>
inline CLockedResource<T> CResourceStream::Write( const T &x )
{
	CLockedResource<T> memory = Allocate<T>();
	*memory = x;
	return memory;
}

inline CLockedResource<float> CResourceStream::WriteFloat( float x )
{
	return Write( x );
}


inline CLockedResource<uint64> CResourceStream::WriteU64( uint64 x )
{
	return Write( x );
}

inline CLockedResource<uint32> CResourceStream::WriteU32( uint32 x )
{
	return Write( x );
}

inline CLockedResource<uint16> CResourceStream::WriteU16( uint16 x )
{
	return Write( x );
}

inline CLockedResource<uint8> CResourceStream::WriteByte( byte x )
{
	return Write( x );
}

inline CLockedResource<char> CResourceStream::WriteString( const char *pString )
{
	int nLength = pString ? V_strlen( pString ) : 0;

	if ( nLength == 0 )
	{
		return CLockedResource<char>( NULL, 0 );
	}
	else
	{
		CLockedResource<char> memory = Allocate<char>(nLength+1);
		memcpy( (char*)memory, pString, nLength+1 );
		return memory;
	}
}

inline CLockedResource<char> CResourceStream::WriteStringMaxLen( const char *pString, int nMaxLen )
{
	int nStrLen = 0;
	while ( pString && nStrLen < nMaxLen && pString[nStrLen] != '\0' )
	{
		nStrLen++;
	}

	if ( nStrLen == 0 )
	{
		return CLockedResource<char>( NULL, 0 );
	}
	else
	{
		CLockedResource<char> memory = Allocate<char>( nStrLen + 1 ); // +1 for null term
		memcpy( (char*)memory, pString, nStrLen );
		((char*)memory)[nStrLen] = '\0';
		return memory;
	}
}

inline uint32 CResourceStream::Tell() const
{
	return m_nUsed;
}


inline void CResourceStream::Rollback( uint32 nPreviousTell )
{
	Assert( nPreviousTell <= m_nUsed );
	m_nUsed = nPreviousTell;
}

inline void CResourceStream::Rollback( const void *pPreviousTell )
{
	Assert( pPreviousTell > m_pData && pPreviousTell <= m_pData + m_nUsed );
	m_nUsed = ( ( uint8* ) pPreviousTell ) - m_pData;
}


inline uint32 CResourceStream::Tell( const void * pPast )const
{
	// we do not reallocate the reserved memory , so the pointers do not invalidate and m_pData stays the same, and all absolute addresses stay the same in memory
	uint nTell = uintp( pPast ) - uintp( m_pData );
	AssertDbg( nTell <= m_nUsed );
	return nTell;
}

inline const void *CResourceStream::TellPtr() const
{
	return m_pData + m_nUsed; 
}

inline void* CResourceStream::Compile( )
{
	return m_pData;
}

template< class Memory >
inline void* CResourceStream::CompileToMemory( Memory &memory )
{
	memory.Init( 0, GetTotalSize() );
	V_memcpy( memory.Base(), Compile(), GetTotalSize() );
	return memory.Base();
}

inline uint CResourceStream::GetTotalSize() const
{
	return m_nUsed;
}

inline uint CResourceStream::GetStreamAlignment() const
{
	return m_nMaxAlignment;
}

inline void* CResourceStream::GetDataPtr( uint nOffset )
{
	if ( nOffset > m_nUsed )
	{
		return NULL;
	}
	else 
	{
		return m_pData + nOffset;
	}
}


inline void CResourceStream::Clear()
{
	V_memset( m_pData, 0, m_nCommitted );
	m_nUsed = 0;
}

template <typename T> 
inline T* CResourceStream::GetDataPtrTypeAligned( uint Offset )
{
	uint nAlignment = VALIGNOF( T );
	Offset += ( ( 0 - Offset ) & ( nAlignment - 1 ) );
	return ( T* ) GetDataPtr( Offset );
}

template <typename T>
inline const T* OffsetPointer( const T *p, intp nOffsetBytes )
{
	return p ? ( const T* )( intp( p ) + nOffsetBytes ) : NULL;
}

template <typename T>
inline T* ConstCastOffsetPointer( const T *p, intp nOffsetBytes )
{
	return p ? ( T* ) ( intp( p ) + nOffsetBytes ) : NULL;
}

template < typename T >
inline CLockedResource< T > CloneArray( CResourceStream *pStream, const T *pArray, uint nCount )
{
	if ( nCount > 0 && pArray )
	{
		CLockedResource< T > pOut = pStream->Allocate< T >( nCount );
		V_memcpy( pOut, pArray, nCount * sizeof( T ) );
		return pOut;
	}
	else
	{
		return CLockedResource< T >( );
	}
}


inline int CResourceStreamFixed::GetSlack( )
{
	return m_nCommitted - m_nUsed;
}


template < typename T >
CUnlockedResource< T >::CUnlockedResource( CResourceStream *pStream, T *pData, uint nCount ):
	m_pStream( pStream ),
	m_nOffset( ( ( uint8* ) pData ) - pStream->m_pData ),
	m_nCount( nCount )
{
	AssertDbg( ( ( uint8* ) pData ) >= pStream->m_pData && ( ( uint8* ) ( pData + nCount ) <= pStream->m_pData + pStream->m_nUsed ) );
	AssertDbg( m_nOffset <= pStream->m_nUsed && m_nOffset + nCount * sizeof( T ) <= pStream->m_nUsed );
}

template < typename T >
inline T* CUnlockedResource< T >::GetPtr( )
{
	return ( T* )m_pStream->GetDataPtr( m_nOffset );
}

template < typename T >			 
inline const T* CUnlockedResource< T >::GetPtr( )const
{
	return ( const T * )m_pStream->GetDataPtr( m_nOffset );
}





#endif // RESOURCESTREAM_H

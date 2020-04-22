//====== Copyright (c) Valve Corporation, All rights reserved. =======//
#ifndef TIER1_UTL_BUFFER_STRIDER_HDR
#define TIER1_UTL_BUFFER_STRIDER_HDR

#include "tier1/strtools.h"

//#include "tier0/memdbg_for_headers.h"

template< class T, class A >
class CUtlVector;


class CBufferStrider
{
public:
	CBufferStrider( void *pBuffer );

	template <typename T> T *Stride( int nCount = 1 );
	template <typename T> T StrideUnaligned( );
	template <typename T> void StrideUnaligned( T* pOut, int nCount = 1 );
	char* StrideString( );
	char* StrideString( const void *pEnd );
	template <typename T> T Peek( ){ return *(T*)Get(); }
	uint8 *Get(){ return m_pBuffer; }
	void Set( void *p ){ m_pBuffer = ( uint8 *)p; }
	void Align( uint nAlignment ){ m_pBuffer = ( uint8* )( ( uintp( m_pBuffer ) + nAlignment - 1 ) & ~uintp( nAlignment - 1 ) ); }
protected:
	uint8 *m_pBuffer;
};


template <typename T>
inline T *CBufferStrider::Stride( int nCount )
{
	uint nAlignMask = VALIGNOF( T ) - 1; NOTE_UNUSED( nAlignMask );
	AssertDbg( !( uintp( m_pBuffer ) & nAlignMask ) );
	T* p = ( T* ) m_pBuffer;
	m_pBuffer += sizeof( T ) * nCount;
	return p;
}

template <typename T>
inline T CBufferStrider::StrideUnaligned( )
{
	T out;
	memcpy( &out, m_pBuffer, sizeof( T ) );
	m_pBuffer += sizeof( T );
	return out;
}


template <typename T>
inline void CBufferStrider::StrideUnaligned( T* pOut, int nCount )
{
	memcpy( pOut, m_pBuffer, sizeof( T ) * nCount );
	m_pBuffer += sizeof( T ) * nCount;
}

inline char* CBufferStrider::StrideString( const void *pEnd )
{
	uint8 *pNextItem = m_pBuffer;
	do
	{
		if ( pNextItem >= ( uint8* ) pEnd )
		{
			return NULL; // we steppend past end of buffer
		}
	}
	while ( *( pNextItem++ ) );
	// found end of string
	char *pString = ( char* ) m_pBuffer;
	m_pBuffer = pNextItem;
	return pString;
}


inline char* CBufferStrider::StrideString( )
{
	uint8 *pNextItem = m_pBuffer;
	while ( *( pNextItem++ ) ) // this will crash in case of malformed (no terminating NULL) buffer
	{
		continue;
	}
	// found end of string
	char *pString = ( char* ) m_pBuffer;
	m_pBuffer = pNextItem;
	return pString;
}


inline
CBufferStrider::CBufferStrider( void *pBuffer )
: m_pBuffer( ( uint8* ) pBuffer )
{
}


#define MULTIBUFFER_ASSERTS

class CMultiBufferCounter
{
public:
	CMultiBufferCounter( )
		:m_nByteCount( 0 )
#ifdef MULTIBUFFER_ASSERTS
		, m_nMaxAlignment( 0 )
		, m_nRunAlignment( 0xFFFFFFFF )
#endif
	{}
	template <typename T> 
	T* operator()( T *& pDummy, uint nCount )
	{
#ifdef MULTIBUFFER_ASSERTS
		const uint nAlignMask = VALIGNOF( T ) - 1;
		Assert( m_nRunAlignment >= nAlignMask );
		m_nRunAlignment &= nAlignMask;
		m_nMaxAlignment |= nAlignMask;
#endif
		m_nByteCount += sizeof( T ) * nCount;
		return NULL; // we didn't allocate anything yet!
	}
	template< typename T, typename A >
	T* operator()( T*& pDummy, const CUtlVector< T, A > &arr )
	{
		( *this )( pDummy, arr.Count( ) );
		return NULL; // we didn't allocate anything yet
	}
	char *String( const char *& pDummy, const char *pSourceString )
	{
		if ( pSourceString )
		{
			( *this )( pDummy, uint( V_strlen( pSourceString ) + 1 ) );
		}
		return NULL; // we didn't allocate anything yet
	}
	uint GetByteCount( )const { return m_nByteCount; }
protected:
	uint m_nByteCount;
#ifdef MULTIBUFFER_ASSERTS
	uint m_nMaxAlignment;
	uint m_nRunAlignment;
#endif
};


class CMultiBufferStrider: public CBufferStrider
{
public:
	CMultiBufferStrider( void *pBuffer ): CBufferStrider( pBuffer ){}
	template <typename T>
	T* operator() ( T*& refPtr, uint nCount )
	{
		refPtr = nCount ? Stride< T >( nCount ) : NULL;
		return refPtr;
	}
	template < typename T, typename A >
	T* operator()( T *&refPtr, const CUtlVector< T, A > &arr )
	{
		if ( arr.IsEmpty( ) )
		{
			refPtr = NULL;
		}
		else
		{
			refPtr = Stride< T >( arr.Count( ) );
			V_memcpy( refPtr, arr.Base( ), sizeof( T ) * arr.Count( ) );
		}
		return refPtr;
	}

	char *String( const char *& refPtr, const char *pSourceString )
	{
		if ( pSourceString )
		{
			uint nBufLen = V_strlen( pSourceString ) + 1;
			char *pOut = const_cast< char* >( ( *this )( refPtr, nBufLen ) );
			V_memcpy( pOut, pSourceString, nBufLen );
			AssertDbg( refPtr == pOut );
			return pOut;
		}
		else
		{
			return NULL; // we didn't allocate anything
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// Derive from this buffer and override function Enum( T& ) that should
// enumerate all the pointers and sizes of buffers to be allocated
//
template <typename Derived>
class CMultiBufferHelper
{
public:
	CMultiBufferHelper( )
	{
		m_pMultiBuffer = NULL;
		m_nMultiBufferSize = 0;
	}
	~CMultiBufferHelper( )
	{
		if ( m_pMultiBuffer )
		{
			MemAlloc_FreeAligned( m_pMultiBuffer );
			m_pMultiBuffer = NULL;
			m_nMultiBufferSize = 0;
		}
	}
	uint8 *GetBuffer( ) { return m_pMultiBuffer; }
	uint GetBufferSize( ) { return m_nMultiBufferSize; }
	uint8 *TakeBuffer( )
	{
		// forget the buffer
		uint8 *pBuffer = m_pMultiBuffer;
		m_pMultiBuffer = NULL;
		m_nMultiBufferSize = 0;
		return pBuffer;
	}

protected:

	template <typename Ctx >
	void ReallocateMultiBuffer( Ctx &context )
	{
		if ( m_pMultiBuffer )
		{
			MemAlloc_FreeAligned( m_pMultiBuffer );
		}

		CMultiBufferCounter byteCounter;
		static_cast< Derived* >( this )->OnAllocateMultiBuffer( byteCounter, context );
		m_nMultiBufferSize = byteCounter.GetByteCount( );
		m_pMultiBuffer = ( uint8* )MemAlloc_AllocAligned( m_nMultiBufferSize, 16 );
		CMultiBufferStrider bufferStrider( m_pMultiBuffer );
		static_cast< Derived* >( this )->OnAllocateMultiBuffer( bufferStrider, context );
		Assert( m_pMultiBuffer + m_nMultiBufferSize == bufferStrider.Get( ) );
	}

protected:
	uint8 *m_pMultiBuffer;
	uint m_nMultiBufferSize;
};


#endif
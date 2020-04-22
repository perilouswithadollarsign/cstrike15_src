//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implementation of various variants of resource stream
//
// $NoKeywords: $
//===========================================================================//

#if defined( _WIN32 ) 
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include "resourcefile/resourcestream.h"
#include "dbg.h"
#include "memalloc.h"


CResourceStream::CResourceStream(  )
{
	m_pData = NULL;
	m_nCommitted = 0;
	m_nUsed = 0;
	m_nAlignBits = 3;
	m_nMaxAlignment = 4;
}

										   
CResourceStreamVM::CResourceStreamVM( uint nReserveSize )
{
	m_pData = NULL;
	m_nReserved = 0;
	ReserveVirtualMemory( nReserveSize );
	m_nAlignBits = 3;			// alignment 4 is default
	m_nMaxAlignment = 4;
	m_nUsed = 0;
	m_nCommitted = 0;
}

CResourceStreamVM::~CResourceStreamVM( )
{
	ReleaseVirtualMemory( );
}



CResourceStreamFixed::CResourceStreamFixed( void *pPreallocatedData, uint nPreallocatedDataSize ):
	m_bOwnMemory( false )
{
	m_pData = ( uint8* )pPreallocatedData;
	m_nCommitted = nPreallocatedDataSize;
	// pre-initialized stream; the memory passed to this function may not be initialized to zero, and resource stream assumes it to be, so clear it
	Assert( pPreallocatedData == m_pData );
	V_memset( pPreallocatedData, 0, nPreallocatedDataSize );
}


CResourceStreamFixed::CResourceStreamFixed( uint nPreallocatedDataSize ):
	m_bOwnMemory( true )
{
	m_pData = new uint8[ nPreallocatedDataSize ];
	m_nCommitted = nPreallocatedDataSize;
	// pre-initialized stream; the memory passed to this function may not be initialized to zero, and resource stream assumes it to be, so clear it
	V_memset( m_pData, 0, nPreallocatedDataSize );
}

CResourceStreamFixed::~CResourceStreamFixed()
{
	// prevent the parent class from trying to deallocate the buffer that doesn't belong to it
	m_pData = NULL;
	m_nCommitted = 0;
}

void CResourceStreamFixed::Commit( uint nNewCommit )
{
	if ( nNewCommit > m_nCommitted )
	{
		Plat_FatalError( "CResourceStreamFixed: trying to write past the end of allocated memory (new commit %u, allocated %u)\n", nNewCommit, m_nCommitted );
	}
}


CResourceStreamGrowable::CResourceStreamGrowable( uint nReserveDataSize )
{
	m_pData = nReserveDataSize ? new uint8[ nReserveDataSize ] : NULL;
	m_nCommitted = nReserveDataSize;
}


CResourceStreamGrowable::~CResourceStreamGrowable( )
{
	delete[] m_pData;
}


void CResourceStreamGrowable::Commit( uint nNewCommit )
{
	if ( nNewCommit > m_nCommitted )
	{
		uint nPaddedCommit = ( nNewCommit * 2 + 63 ) & ~63;
		uint8 *pNewData = new uint8[ nPaddedCommit ];
		V_memcpy( pNewData, m_pData, m_nUsed );
		AssertDbg( !( ( ( uintp ) pNewData ) & 0xF ) );
		delete[]m_pData;
		m_pData = pNewData;
		m_nCommitted = nPaddedCommit;
	}
}



void CResourceStreamVM::ReserveVirtualMemory( uint nAddressSize )
{
	Assert ( m_pData == NULL );

	nAddressSize = ( nAddressSize + COMMIT_STEP - 1 ) & ~( COMMIT_STEP - 1 );
	m_nReserved = MAX( nAddressSize, COMMIT_STEP );
	for ( ;; )
	{
#if defined( PLATFORM_WINDOWS )
		m_pData = ( uint8* )VirtualAlloc( NULL, m_nReserved, MEM_RESERVE, PAGE_READWRITE );
#else
#ifdef PLATFORM_OSX
		int nFlags = MAP_ANON | MAP_PRIVATE;
#else
		int nFlags = MAP_ANONYMOUS | MAP_PRIVATE;
#endif
		m_pData = ( uint8* )::mmap( NULL, m_nReserved, PROT_WRITE | PROT_READ, nFlags, -1, 0 );
#endif
		if ( !m_pData )
		{
			m_nReserved /= 2;
			if ( m_nReserved < COMMIT_STEP )
			{
				Plat_FatalError( "Cannot allocate virtual address space in CResourceStreamVM::ReserveVirtualMemory, error %u\n", errno );
			}
		}
		else
		{
			return;
		}
	}
}


void CResourceStreamVM::ReleaseVirtualMemory()
{
	if ( m_pData != NULL )
	{
#if defined( PLATFORM_WINDOWS )
		VirtualFree( m_pData, m_nReserved, MEM_RELEASE );
#else
		munmap( m_pData, m_nReserved );
#endif
	}

	m_pData = NULL;	
	m_nReserved = 0;
	m_nUsed = 0;
	m_nCommitted = 0;
}


void CResourceStreamVM::CloneStream( CResourceStreamVM& copyFromStream )
{
	// see if we need to reinitialize
	if ( m_nReserved != copyFromStream.m_nReserved )
	{
		ReleaseVirtualMemory();
		ReserveVirtualMemory( copyFromStream.m_nReserved );
	}
	else
	{
		if ( m_nUsed > 0) 
		{
			memset( m_pData, 0, m_nUsed );
		}
		m_nUsed = 0;	
	}
	
	Assert ( m_nReserved >= copyFromStream.m_nUsed );
	
	memcpy ( AllocateBytes( copyFromStream.m_nUsed ), copyFromStream.m_pData, copyFromStream.m_nUsed );
	
	m_nAlignBits = copyFromStream.m_nAlignBits;
	m_nMaxAlignment = copyFromStream.m_nMaxAlignment;
	
}


void* CResourceStream::AllocateBytes( uint numBytes )
{
	EnsureAvailable( numBytes );
	void *pBlock = m_pData + m_nUsed;
	m_nUsed += numBytes;
	return pBlock;
}


void CResourceStreamVM::Commit( uint nNewCommit )
{
	if ( nNewCommit > m_nReserved )
	{
		Warning( "--------------------WARNING----------------------\n" );
		Warning( "--------------------WARNING----------------------\n" );
		Warning( "--------------------WARNING----------------------\n" );
		Warning( "--  Increase the size of the resource stream   --\n" );
		Warning( "--  Need %d bytes, but only have %d bytes\n", nNewCommit, m_nReserved );
		Warning( "--------------------WARNING----------------------\n" );
		Warning( "--------------------WARNING----------------------\n" );
		Warning( "--------------------WARNING----------------------\n" );

		Plat_FatalError( "Failed trying to Commit %u bytes, used %u bytes, reserved %u bytes\n", nNewCommit, m_nUsed, m_nReserved );
	}
	else if ( nNewCommit > m_nCommitted )
	{
		nNewCommit = ( nNewCommit + COMMIT_STEP - 1 ) & ~( COMMIT_STEP - 1 );
		// expensive call. Not to be called frequently
#if defined( PLATFORM_WINDOWS )
		VirtualAlloc( m_pData + m_nCommitted, nNewCommit - m_nCommitted, MEM_COMMIT, PAGE_READWRITE );
#else
		mprotect( m_pData + m_nCommitted, nNewCommit - m_nCommitted, PROT_READ|PROT_WRITE );
#endif
		m_nCommitted = nNewCommit;
	}
}






void CResourceStream::Align( uint nAlignment, int nOffset )
{
	m_nMaxAlignment = MAX( m_nMaxAlignment, nAlignment );
	if ( uintp( m_pData ) & ( nAlignment - 1 ) )
	{
		Plat_FatalError( "You can't align data in resource stream more than its memory buffer. Please allocate the memory aligned." );
	}
	else if ( nAlignment & ( nAlignment - 1 ) )
	{
		Plat_FatalError( "Wrong alignment %d\n", nAlignment );
	}
	else
	{
		int padSize = ( nOffset - m_nUsed ) & ( nAlignment - 1 );
		if ( padSize > 0 )
		{
			// Extend the buffer to align and fill the hole with a constant value
			void *pBuffer = AllocateBytes( padSize );
			V_memset( pBuffer, 0, padSize );
		}
	}
}



void CResourceStream::PrintStats()
{
	Msg( "ResourceFile: %d/%d @%p align %u\n", m_nUsed, m_nCommitted, m_pData, m_nAlignBits );
}

void CResourceStream::ClearStats()
{
}


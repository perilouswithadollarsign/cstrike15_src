//===== Copyright © 2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: Virtual memory sections management!
//
// $NoKeywords: $
//===========================================================================//


#include "pch_tier0.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

//#include <malloc.h>
#include <string.h>
#include "tier0/dbg.h"
#include "tier0/stacktools.h"
#include "tier0/memalloc.h"
#include "tier0/memvirt.h"
#include "tier0/fasttimer.h"
#include "mem_helpers.h"
#ifdef PLATFORM_WINDOWS_PC
#undef WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <crtdbg.h>
#endif
#ifdef OSX
#include <malloc/malloc.h>
#include <stdlib.h>
#endif

#include <map>
#include <set>
#include <limits.h>
#include "tier0/threadtools.h"
#ifdef _X360
#include "xbox/xbox_console.h"
#endif

#ifdef _PS3
#include "tls_ps3.h"
#include "memoverride_ps3.h"
#include "sys/memory.h"
#include "sys/mempool.h"
#include "sys/process.h"
#include "sys/vm.h"
#endif

CInterlockedInt VmmMsgFlag = 0; // Prevents re-entrancy within VmmMsg (printf allocates a large buffer!)

#ifdef _DEBUG
#ifdef _PS3 // _DEBUG
#define VmmMsg( mutex, ... ) ( (VmmMsgFlag|mutex.GetOwnerId()) ? 0 : ( ++VmmMsgFlag, Msg( __VA_ARGS__ ), VmmMsgFlag-- ) )
#else
#define VmmMsg( mutex, ... ) ( (VmmMsgFlag|mutex.GetOwnerId()) ? 0 : ( ++VmmMsgFlag, DevMsg( __VA_ARGS__ ), VmmMsgFlag-- ) )
#endif
#else
#define VmmMsg( mutex, ... ) ((void)0)
#endif

#if 0
#define TRACE_CALL( ... ) do { FILE *fPs3Trace = fopen( "/app_home/tracevmm.txt", "a+" ); if( fPs3Trace ) { fprintf( fPs3Trace, __VA_ARGS__ ); fclose( fPs3Trace ); } } while( 0 )
#else
#define TRACE_CALL( ... ) ((void)0)
#endif

#ifdef _PS3
#define VIRTUAL_MEMORY_MANAGER_SUPPORTED

#define VMM_SYSTEM_PAGE_POLICY SYS_MEMORY_PAGE_SIZE_64K
#define VMM_SYSTEM_PAGE_ALLOCFLAGS SYS_MEMORY_GRANULARITY_64K

#define VMM_POLICY_SYS_VM 0			// sys_vm_* system is really buggy and console sometimes hardlocks or crashes in OS
#define VMM_POLICY_SYS_MMAPPER 1	// sys_mmapper_* seems fairly stable

#define VMM_POLICY_SYS_VM_NO_RETURN 1	// looks like returning memory under sys_vm_* is the main reason for hardlocks

#if !(VMM_POLICY_SYS_VM) == !(VMM_POLICY_SYS_MMAPPER)
#error
#endif

class CVirtualMemoryManager
{
public:
	CVirtualMemoryManager();
	~CVirtualMemoryManager() { Shutdown(); }

	void Shutdown();
	IVirtualMemorySection * AllocateVirtualMemorySection( size_t numMaxBytes );
	IVirtualMemorySection * NewSection( byte *pBase, size_t numMaxBytes );
	IVirtualMemorySection * GetMemorySectionForAddress( void *pAddress );
	void GetStats( size_t &nReserved, size_t &nReservedMax, size_t &nCommitted, size_t &nCommittedMax );

public:
	byte *m_pBase;				// base address of the virtual address space
	size_t m_nPhysicalSize;		// physical memory committed
	size_t m_nPhysicalSizeMax;	// physical memory committed (max since startup)
	size_t m_nVirtualSize;		// virtual memory reserved
	size_t m_nVirtualSizeMax;	// virtual memory reserved (max since startup)
#if VMM_POLICY_SYS_VM
	size_t m_nSparePhysicalSize; // physical memory that has been decommitted, but hasn't been returned
#endif
#if VMM_POLICY_SYS_MMAPPER
	sys_memory_t m_sysMemPages[ VMM_VIRTUAL_SIZE / VMM_PAGE_SIZE ];
#endif
	IVirtualMemorySection *m_uiVirtualMap[ VMM_VIRTUAL_SIZE / VMM_PAGE_SIZE ];	// map of pages to sections
	uint32 m_uiCommitMask[ VMM_VIRTUAL_SIZE / ( VMM_PAGE_SIZE * 32 ) ];			// mask of committed pages
	
	inline bool CommitTest( int iPage ) const
	{
		const uint32 mask = 1u << ( iPage % 32 ), &word = m_uiCommitMask[ iPage / 32 ];
		return !!( word & mask );
	}
	inline void CommitSet( int iPage, bool bSet )
	{
		uint32 mask = 1u << ( iPage % 32 ), &word = m_uiCommitMask[ iPage / 32 ];
		word = ( word & ~mask ) | ( bSet ? mask : 0 );
	}
	inline size_t CountReservedPages()
	{
		size_t uResult = 0;
		for ( int k = 0; k < VMM_VIRTUAL_SIZE / VMM_PAGE_SIZE; ++ k )
			uResult += m_uiVirtualMap[k] ? 1 : 0;
		return uResult;
	}
	inline size_t CountCommittedPages() const
	{
		size_t uResult = 0;
		for ( int k = 0; k < VMM_VIRTUAL_SIZE / VMM_PAGE_SIZE; ++ k )
			uResult += CommitTest(k) ? 1 : 0;
		return uResult;
	}

public:
	bool CommitPage( byte *pbAddress );
	bool DecommitPage( byte *pbAddress );

public:
	CThreadFastMutex m_Mutex;
};

static CVirtualMemoryManager& GetVirtualMemoryManager()
{
	static CVirtualMemoryManager s_VirtualMemoryManager;
	return s_VirtualMemoryManager;
}

CVirtualMemoryManager::CVirtualMemoryManager() :
	m_pBase( NULL ),
#if VMM_POLICY_SYS_VM
	m_nSparePhysicalSize( 0 ),
#endif
	m_nPhysicalSize( 0 ),
	m_nPhysicalSizeMax( 0 ),
	m_nVirtualSize( 0 ),
	m_nVirtualSizeMax( 0 )
{
	memset( m_uiVirtualMap, 0, sizeof( m_uiVirtualMap ) );
	memset( m_uiCommitMask, 0, sizeof( m_uiCommitMask ) );

#if VMM_POLICY_SYS_VM

	TRACE_CALL( "sys_vm_memory_map( 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X ) ... ",
		VMM_VIRTUAL_SIZE,
		VMM_PAGE_SIZE,
		SYS_MEMORY_CONTAINER_ID_INVALID,
		VMM_SYSTEM_PAGE_POLICY,
		SYS_VM_POLICY_AUTO_RECOMMENDED,
		reinterpret_cast<sys_addr_t*>( &m_pBase ) );

	int retval = sys_vm_memory_map(
		VMM_VIRTUAL_SIZE,
		VMM_PAGE_SIZE,
		SYS_MEMORY_CONTAINER_ID_INVALID,
		VMM_SYSTEM_PAGE_POLICY,
		SYS_VM_POLICY_AUTO_RECOMMENDED,
		reinterpret_cast<sys_addr_t*>( &m_pBase ) );
	
	TRACE_CALL( "ret = 0x%08X, base = 0x%08X\n",
		retval, m_pBase );

	if ( retval < CELL_OK || !m_pBase )
	{
		Error( "sys_vm_memory_map failed( size=%dKB, page=%dKB ), error=0x%08X\n", VMM_VIRTUAL_SIZE / VMM_KB, VMM_PAGE_SIZE / VMM_KB, retval );
		Assert( 0 );
		m_pBase = NULL;
		return;
	}

#endif

#if VMM_POLICY_SYS_MMAPPER

	memset( m_sysMemPages, 0, sizeof( m_sysMemPages ) );

	TRACE_CALL( "sys_mmapper_allocate_address( 0x%08X, 0x%08X, 0x%08X, 0x%08X ) ... ",
		VMM_VIRTUAL_SIZE,
		VMM_SYSTEM_PAGE_POLICY,
		VMM_VIRTUAL_SIZE,
		reinterpret_cast<sys_addr_t*>( &m_pBase ) );

	int retval = sys_mmapper_allocate_address(
		VMM_VIRTUAL_SIZE,
		VMM_SYSTEM_PAGE_POLICY,
		VMM_VIRTUAL_SIZE,
		reinterpret_cast<sys_addr_t*>( &m_pBase ) );

	TRACE_CALL( "ret = 0x%08X, base = 0x%08X\n",
		retval, m_pBase );

	if ( retval < CELL_OK || !m_pBase )
	{
		Error( "sys_mmapper_allocate_address failed( size=%dKB, page=%dKB ), error=0x%08X\n", VMM_VIRTUAL_SIZE / VMM_KB, VMM_PAGE_SIZE / VMM_KB, retval );
		Assert( 0 );
		m_pBase = NULL;
		return;
	}

#endif

	VmmMsg( m_Mutex, "Virtual Memory Manager: reserved %uKB block at address 0x%08X.\n", VMM_VIRTUAL_SIZE / VMM_KB, m_pBase );
}

void CVirtualMemoryManager::Shutdown()
{
	if ( !m_pBase )
		return;

	if ( m_nPhysicalSize )
	{
		VmmMsg( m_Mutex, "Virtual Memory Manager: shutting down with %uKB allocated!\n", m_nPhysicalSize / VMM_KB );
	}

#if VMM_POLICY_SYS_VM
	TRACE_CALL( "sys_vm_unmap( 0x%08X ) ... ", (sys_addr_t) m_pBase );
	int retval = sys_vm_unmap( (sys_addr_t) m_pBase );
	TRACE_CALL( "ret = 0x%08X\n", retval );
#endif

#if VMM_POLICY_SYS_MMAPPER
	TRACE_CALL( "sys_mmapper_free_address( 0x%08X ) ... ", (sys_addr_t) m_pBase );
	int retval = sys_mmapper_free_address( (sys_addr_t) m_pBase );
	TRACE_CALL( "ret = 0x%08X\n", retval );
#endif

	VmmMsg( m_Mutex, "Virtual Memory Manager: unmapped %uKB block at address 0x%08X (result=0x%08X).\n", VMM_VIRTUAL_SIZE / VMM_KB, m_pBase, retval );
	m_pBase = NULL;
}

IVirtualMemorySection * CVirtualMemoryManager::GetMemorySectionForAddress( void *pAddress )
{
	Assert( ( pAddress >= m_pBase ) && ( pAddress < ( m_pBase + VMM_VIRTUAL_SIZE ) ) );
	int iPage = ( (byte*)pAddress - m_pBase ) / VMM_PAGE_SIZE;
	return m_uiVirtualMap[ iPage ];
}

IVirtualMemorySection * CVirtualMemoryManager::AllocateVirtualMemorySection( size_t numMaxBytes )
{
	Assert( m_pBase );
	if ( !m_pBase )
		return NULL;

	// Find the smallest free block with size >= numMaxBytes
	IVirtualMemorySection *pResult = NULL;
	int iGoodPage = -1;
	int iGoodSize = 0;
	size_t numPages = numMaxBytes / VMM_PAGE_SIZE;
	if ( !numPages )
		return NULL;

	{
		AUTO_LOCK( m_Mutex );

		// TODO: fill the address range with reserved/free sections (so we iterate 50 sections rather than 8000 pages!)
		for ( int iTryPage = 0; iTryPage < VMM_VIRTUAL_SIZE / VMM_PAGE_SIZE; ++ iTryPage )
		{
			if ( m_uiVirtualMap[ iTryPage ] )
				continue; // page is taken
			
			int iTryPageStart = iTryPage;
			int iTryPageStride = 1;
			for ( ++ iTryPage; iTryPage < VMM_VIRTUAL_SIZE / VMM_PAGE_SIZE; ++ iTryPage, ++ iTryPageStride )
				if ( m_uiVirtualMap[ iTryPage ] )
					break;

			if ( iTryPageStride < numPages )
				continue;

			if ( ( iGoodPage < 0 ) || ( iTryPageStride < iGoodSize ) )
			{
				iGoodPage = iTryPageStart;
				iGoodSize = iTryPageStride;
			}
		}

		if ( iGoodPage >= 0 )
		{
			byte *pbAddress = m_pBase + iGoodPage * VMM_PAGE_SIZE;
			pResult = NewSection( pbAddress, numMaxBytes );
			m_nVirtualSize += numPages*VMM_PAGE_SIZE;
			m_nVirtualSizeMax = MAX( m_nVirtualSize, m_nVirtualSizeMax );

			// Mark pages used
			for ( int k = 0; k < numPages; ++ k )
				m_uiVirtualMap[ iGoodPage + k ] = pResult;
		}
	}
	
	if ( pResult )
	{
		// NOTE: don't spew while the mutex is held!
		VmmMsg( m_Mutex, "Virtual Memory Manager [ reserved %uKB, committed %uKB ]: new reservation %uKB @ 0x%08X\n",
			CountReservedPages()*VMM_PAGE_SIZE / VMM_KB,
			CountCommittedPages()*VMM_PAGE_SIZE / VMM_KB,
			numMaxBytes / VMM_KB, pResult->GetBaseAddress() );

		return pResult;
	}

	Error( "CVirtualMemoryManager::AllocateVirtualMemorySection has no memory for %u bytes!\n", numMaxBytes );
	Assert( 0 );
	return NULL;
}

bool CVirtualMemoryManager::CommitPage( byte *pbAddress )
{
	Assert( m_pBase );
	int iPage = ( pbAddress - m_pBase ) / VMM_PAGE_SIZE;
	if ( CommitTest( iPage ) )
		return true;

	CommitSet( iPage, true );

#if VMM_POLICY_SYS_VM
	if ( m_nPhysicalSize )
	{
		if ( m_nSparePhysicalSize > 0 )
		{
			m_nSparePhysicalSize -= VMM_PAGE_SIZE;
		}
		else
		{
			TRACE_CALL( "sys_vm_append_memory( 0x%08X, 0x%08X ) ... ", (sys_addr_t) m_pBase, VMM_PAGE_SIZE );
			int retval = sys_vm_append_memory( (sys_addr_t) m_pBase, VMM_PAGE_SIZE );
			TRACE_CALL( "ret = 0x%08X\n", retval );
			if ( retval < CELL_OK )
			{
				VmmMsg( m_Mutex, "Virtual Memory Manager: CommitPage: sys_vm_append_memory failed (result=0x%08X), %uKB committed so far.\n", retval, m_nPhysicalSize / VMM_KB );
				return false;
			}
		}
	}
	m_nPhysicalSize += VMM_PAGE_SIZE;
	m_nPhysicalSizeMax = MAX( m_nPhysicalSize, m_nPhysicalSizeMax );

	TRACE_CALL( "sys_vm_touch( 0x%08X, 0x%08X ) ... ", (sys_addr_t) pbAddress, VMM_PAGE_SIZE );
	int retval = sys_vm_touch( (sys_addr_t) pbAddress, VMM_PAGE_SIZE );
	TRACE_CALL( "ret = 0x%08X\n", retval );

	if ( retval < CELL_OK )
	{
		VmmMsg( m_Mutex, "Virtual Memory Manager: CommitPage: sys_vm_touch failed (result=0x%08X), %uKB committed so far.\n", retval, m_nPhysicalSize / VMM_KB );
		return false;
	}
#endif

#if VMM_POLICY_SYS_MMAPPER
	TRACE_CALL( "sys_mmapper_allocate_memory( 0x%08X, 0x%08X, page=%d ) ... ", VMM_PAGE_SIZE, VMM_SYSTEM_PAGE_ALLOCFLAGS, iPage );
	int retval = sys_mmapper_allocate_memory( VMM_PAGE_SIZE, VMM_SYSTEM_PAGE_ALLOCFLAGS, &m_sysMemPages[iPage] );
	TRACE_CALL( "ret = 0x%08X, mem = 0x%08X\n", retval, m_sysMemPages[iPage] );
	if ( retval < CELL_OK )
	{
		VmmMsg( m_Mutex, "Virtual Memory Manager: CommitPage: sys_mmapper_allocate_memory failed (result=0x%08X), %uKB committed so far.\n", retval, m_nPhysicalSize / VMM_KB );
		return false;
	}
	m_nPhysicalSize += VMM_PAGE_SIZE;
	m_nPhysicalSizeMax = MAX( m_nPhysicalSize, m_nPhysicalSizeMax );

	TRACE_CALL( "sys_mmapper_map_memory( 0x%08X, 0x%08X, 0x%08X, page=%d ) ... ", (sys_addr_t) pbAddress, m_sysMemPages[iPage], SYS_MEMORY_PROT_READ_WRITE, iPage );
	retval = sys_mmapper_map_memory( (sys_addr_t) pbAddress, m_sysMemPages[iPage], SYS_MEMORY_PROT_READ_WRITE );
	TRACE_CALL( "ret = 0x%08X\n", retval );
	if ( retval < CELL_OK )
	{
		VmmMsg( m_Mutex, "Virtual Memory Manager: CommitPage: sys_mmapper_map_memory failed (result=0x%08X), %uKB committed so far.\n", retval, m_nPhysicalSize / VMM_KB );
		return false;
	}
#endif

	return true;
}

bool CVirtualMemoryManager::DecommitPage( byte *pbAddress )
{
	Assert( m_pBase );
	int iPage = ( pbAddress - m_pBase ) / VMM_PAGE_SIZE;
	if ( !CommitTest( iPage ) )
		return false;

	CommitSet( iPage, false );

#if VMM_POLICY_SYS_VM
	TRACE_CALL( "sys_vm_invalidate( 0x%08X, 0x%08X ) ... ", (sys_addr_t) pbAddress, VMM_PAGE_SIZE );
	int retval = sys_vm_invalidate( (sys_addr_t) pbAddress, VMM_PAGE_SIZE );
	TRACE_CALL( "ret = 0x%08X\n", retval );
	if ( retval < CELL_OK )
	{
		VmmMsg( m_Mutex, "Virtual Memory Manager: DecommitPage: sys_vm_invalidate failed (result=0x%08X), %uKB committed so far.\n", retval, m_nPhysicalSize / VMM_KB );
		return false;
	}

	m_nPhysicalSize -= VMM_PAGE_SIZE;
#if VMM_POLICY_SYS_VM_NO_RETURN
	m_nSparePhysicalSize += VMM_PAGE_SIZE;
	return false;
#else
	return m_nPhysicalSize >= VMM_PAGE_SIZE;
#endif
#endif

#if VMM_POLICY_SYS_MMAPPER
	TRACE_CALL( "sys_mmapper_unmap_memory( 0x%08X, 0x%08X, page=%d ) ... ", (sys_addr_t) pbAddress, m_sysMemPages[iPage], iPage );
	int retval = sys_mmapper_unmap_memory( (sys_addr_t) pbAddress, &m_sysMemPages[iPage] );
	TRACE_CALL( "ret = 0x%08X, mem = 0x%08X\n", retval, m_sysMemPages[iPage] );
	if ( retval < CELL_OK )
	{
		VmmMsg( m_Mutex, "Virtual Memory Manager: DecommitPage: sys_mmapper_unmap_memory failed (result=0x%08X), %uKB committed so far.\n", retval, m_nPhysicalSize / VMM_KB );
		return false;
	}

	TRACE_CALL( "sys_mmapper_free_memory( 0x%08X, page=%d ) ... ", m_sysMemPages[iPage], iPage );
	retval = sys_mmapper_free_memory( m_sysMemPages[iPage] );
	TRACE_CALL( "ret = 0x%08X\n", retval );
	m_sysMemPages[iPage] = 0;
	if ( retval < CELL_OK )
	{
		VmmMsg( m_Mutex, "Virtual Memory Manager: DecommitPage: sys_mmapper_free_memory failed (result=0x%08X), %uKB committed so far.\n", retval, m_nPhysicalSize / VMM_KB );
		return false;
	}
	m_nPhysicalSize -= VMM_PAGE_SIZE;
	return true;
#endif
}

void CVirtualMemoryManager::GetStats( size_t &nReserved, size_t &nReservedMax, size_t &nCommitted, size_t &nCommittedMax )
{
	AUTO_LOCK( m_Mutex );
	nReserved     = m_nVirtualSize;
	nReservedMax  = m_nVirtualSizeMax;
	nCommitted    = m_nPhysicalSize;
	nCommittedMax = m_nPhysicalSizeMax;
}

class CVirtualMemorySectionImpl : public IVirtualMemorySection
{
public:
	CVirtualMemorySectionImpl( byte *pbAddress, size_t numMaxBytes ) :
	  m_pBase( pbAddress ),
	  m_numMaxBytes( numMaxBytes ),
	  m_numPhysical( 0 )
	  {
	  }


	// Information about memory section
	virtual void * GetBaseAddress() { return m_pBase; }
	virtual size_t GetPageSize() { return VMM_PAGE_SIZE; }
	virtual size_t GetTotalSize() { return m_numMaxBytes; }

	// Functions to manage physical memory mapped to virtual memory
	virtual bool CommitPages( void *pvBase, size_t numBytes );
	virtual void DecommitPages( void *pvBase, size_t numBytes );
	bool CommitPages_Inner( void *pvBase, size_t numBytes );

	// Release the physical memory and associated virtual address space
	virtual void Release();


	byte *m_pBase;
	size_t m_numMaxBytes;
	size_t m_numPhysical;
};

bool CVirtualMemorySectionImpl::CommitPages_Inner( void *pvBase, size_t numBytes )
{
	Assert( pvBase >= m_pBase );
	Assert( ( (byte*)pvBase ) + numBytes <= m_pBase + m_numMaxBytes );

	{
		AUTO_LOCK( GetVirtualMemoryManager().m_Mutex );

		int startPage = ( ( (byte*)pvBase ) - m_pBase ) / VMM_PAGE_SIZE;
		int endPage   = ( ( (byte*)pvBase ) + numBytes + ( VMM_PAGE_SIZE - 1 ) - m_pBase ) / VMM_PAGE_SIZE;
		for ( int k = startPage; k < endPage; k++ )
		{
			if ( !GetVirtualMemoryManager().CommitPage( m_pBase + k * VMM_PAGE_SIZE ) )
			{
				// Failure! Decommit the pages we have committed so far:
				for ( k = k - 1; k >= startPage; k-- )
				{
					GetVirtualMemoryManager().DecommitPage( m_pBase + k * VMM_PAGE_SIZE );
				}
				return false;
			}
		}

#if VMM_POLICY_SYS_VM
		TRACE_CALL( "sys_vm_sync( 0x%08X, 0x%08X ) ... ", (sys_addr_t) pvBase, numBytes );
		int retval = sys_vm_sync( (sys_addr_t) pvBase, numBytes );
		TRACE_CALL( "ret = 0x%08X\n", retval );
		if ( retval < CELL_OK )
		{
			VmmMsg( GetVirtualMemoryManager().m_Mutex, "Virtual Memory Manager: CommitPages: sys_vm_sync failed (result=0x%08X).\n", retval );
			return false;
		}
#endif
	}

	// NOTE: we don't spew while the mutex is held!
	VmmMsg( GetVirtualMemoryManager().m_Mutex, "Virtual Memory Manager [ reserved %uKB, committed %uKB ]: committed %uKB @ 0x%08X\n",
		GetVirtualMemoryManager().CountReservedPages() * VMM_PAGE_SIZE / VMM_KB,
		GetVirtualMemoryManager().CountCommittedPages() * VMM_PAGE_SIZE / VMM_KB,
		numBytes / VMM_KB, pvBase );

	return true;
}

bool CVirtualMemorySectionImpl::CommitPages( void *pvBase, size_t numBytes )
{
	if ( CommitPages_Inner( pvBase, numBytes ) )
		return true;

	// On failure, compact the heap and try one last time:
	Msg( "\n\nVirtual Memory Manager: COMMIT FAILED! (%d) Last-ditch effort: compacting the heap and re-trying...\n", numBytes );
	g_pMemAllocInternalPS3->CompactHeap();
	bool success = CommitPages_Inner( pvBase, numBytes );
	if ( !success )
	{
		Msg( "Virtual Memory Manager: COMMIT FAILED! (%d) Fatal error.\n\n\n", numBytes );
		g_pMemAllocInternalPS3->OutOfMemory( numBytes );
	}
	Msg("\n\n");

	return success;
}

void CVirtualMemorySectionImpl::DecommitPages( void *pvBase, size_t numBytes )
{
	Assert( pvBase >= m_pBase );
	Assert( ( (byte*)pvBase ) + numBytes <= m_pBase + m_numMaxBytes );

	{
		AUTO_LOCK( GetVirtualMemoryManager().m_Mutex );

		int numPagesDecommitted = 0;

		for ( int k = ( ( (byte*)pvBase ) - m_pBase ) / VMM_PAGE_SIZE;
			m_pBase + k * VMM_PAGE_SIZE < ( (byte*)pvBase ) + numBytes; ++ k )
		{
			numPagesDecommitted += !!GetVirtualMemoryManager().DecommitPage( m_pBase + k * VMM_PAGE_SIZE );
		}

#if VMM_POLICY_SYS_VM
		TRACE_CALL( "sys_vm_sync( 0x%08X, 0x%08X ) ... ", (sys_addr_t) pvBase, numBytes );
		int retval = sys_vm_sync( (sys_addr_t) pvBase, numBytes );
		TRACE_CALL( "ret = 0x%08X\n", retval );
		if ( retval < CELL_OK )
		{
			VmmMsg( GetVirtualMemoryManager().m_Mutex, "Virtual Memory Manager: DecommitPages: sys_vm_sync failed (result=0x%08X).\n", retval );
		}

		if ( numPagesDecommitted > 0 )
		{
			TRACE_CALL( "sys_vm_return_memory( 0x%08X, 0x%08X ) ... ", (sys_addr_t) GetVirtualMemoryManager().m_pBase, numPagesDecommitted * VMM_PAGE_SIZE );
			retval = sys_vm_return_memory( (sys_addr_t) GetVirtualMemoryManager().m_pBase, numPagesDecommitted * VMM_PAGE_SIZE );
			TRACE_CALL( "ret = 0x%08X\n", retval );
			if ( retval < CELL_OK )
			{
				VmmMsg( GetVirtualMemoryManager().m_Mutex, "Virtual Memory Manager: DecommitPages: sys_vm_return_memory failed (result=0x%08X).\n", retval );
			}
		}
#endif
	}

	// NOTE: we don't spew while the mutex is held!
	VmmMsg( GetVirtualMemoryManager().m_Mutex, "Virtual Memory Manager [ reserved %uKB, committed %uKB ]: decommitted %uKB @ 0x%08X\n",
		GetVirtualMemoryManager().CountReservedPages() * VMM_PAGE_SIZE / VMM_KB,
		GetVirtualMemoryManager().CountCommittedPages() * VMM_PAGE_SIZE / VMM_KB,
		numBytes / VMM_KB, pvBase );
}

void CVirtualMemorySectionImpl::Release()
{
	DecommitPages( m_pBase, m_numMaxBytes );

	{
		AUTO_LOCK( GetVirtualMemoryManager().m_Mutex );

		int iPageStart = ( m_pBase - GetVirtualMemoryManager().m_pBase ) / VMM_PAGE_SIZE;
		for ( int k = 0; m_pBase + k * VMM_PAGE_SIZE < m_pBase + m_numMaxBytes; ++ k )
		{
			Assert( GetVirtualMemoryManager().m_uiVirtualMap[ iPageStart + k ] == this );
			GetVirtualMemoryManager().m_uiVirtualMap[ iPageStart + k ] = NULL;
			GetVirtualMemoryManager().m_nVirtualSize -= VMM_PAGE_SIZE;
		}
	}

	// NOTE: we don't spew while the mutex is held!
	VmmMsg( GetVirtualMemoryManager().m_Mutex, "Virtual Memory Manager [ reserved %uKB, committed %uKB ]: released %uKB @ 0x%08X\n",
		GetVirtualMemoryManager().CountReservedPages() * VMM_PAGE_SIZE / VMM_KB,
		GetVirtualMemoryManager().CountCommittedPages() * VMM_PAGE_SIZE / VMM_KB,
		m_numMaxBytes / VMM_KB, m_pBase );

	delete this;
}

IVirtualMemorySection * CVirtualMemoryManager::NewSection( byte *pBase, size_t numMaxBytes )
{
	return new CVirtualMemorySectionImpl( pBase, numMaxBytes );
}

#endif


#ifdef VIRTUAL_MEMORY_MANAGER_SUPPORTED

IVirtualMemorySection * VirtualMemoryManager_AllocateVirtualMemorySection( size_t numMaxBytes )
{
	return GetVirtualMemoryManager().AllocateVirtualMemorySection( numMaxBytes );
}

void VirtualMemoryManager_Shutdown()
{
	GetVirtualMemoryManager().Shutdown();
}

IVirtualMemorySection *GetMemorySectionForAddress( void *pAddress )
{
	return GetVirtualMemoryManager().GetMemorySectionForAddress( pAddress );
}

void VirtualMemoryManager_GetStats( size_t &nReserved, size_t &nReservedMax, size_t &nCommitted, size_t &nCommittedMax )
{
	GetVirtualMemoryManager().GetStats( nReserved, nReservedMax, nCommitted, nCommittedMax );
}

#else

IVirtualMemorySection * VirtualMemoryManager_AllocateVirtualMemorySection( size_t numMaxBytes )
{
	return NULL;
}

void VirtualMemoryManager_Shutdown()
{
}

IVirtualMemorySection *GetMemorySectionForAddress( void *pAddress )
{
	return NULL;
}

#endif


#endif // !STEAM && !NO_MALLOC_OVERRIDE


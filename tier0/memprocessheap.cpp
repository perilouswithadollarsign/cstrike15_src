//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Memory allocation!
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"
#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "memstd.h"

#if !defined(NO_MALLOC_OVERRIDE)

#if defined( _WIN32 )

#define	OVERRIDE override
// warning C4481: nonstandard extension used: override specifier 'override'
#pragma warning( disable : 4481 )

#ifdef _WIN64
// Set the new-style define that indicates a a 64-bit Windows PC
#define PLATFORM_WINDOWS_PC64 1
LONGLONG
FORCEINLINE
InterlockedExchangeAdd64(
						 __inout LONGLONG volatile *Addend,
						 __in    LONGLONG Value
						 );
#else
// Set the new-style define that indicates a a 32-bit Windows PC
#define PLATFORM_WINDOWS_PC32 1
#endif

// Support for CHeapMemAlloc for easy switching to using the process heap.

// Track this to decide how to handle out-of-memory.
static bool s_bPageHeapEnabled = false;

//-----------------------------------------------------------------------------
// IMemAlloc must guarantee 16-byte alignment for 16n-byte allocations, so we just
// force 16-byte alignment under win32 (the win64 system heap already 16-byte aligns).
// TODO: this padding negates some of the buffer-overrun protection provided by pageheap, so...
//       we should fill padding bytes with a known pattern which is checked in realloc/free
#ifdef PLATFORM_WINDOWS_PC32
#define FORCED_ALIGNMENT 16
#else
#define FORCED_ALIGNMENT 0
#endif


// Round a size up to a multiple of 4 KB to aid in calculating how much
// memory is required if full pageheap is enabled.
static size_t RoundUpToPage( size_t nSize )
{
	nSize += 0xFFF;
	nSize &= ~0xFFF;
	return nSize;
}

static void InterlockedAddSizeT( size_t volatile *Addend, size_t Value )
{
#ifdef PLATFORM_WINDOWS_PC32
// Convenience function to deal with the necessary type-casting
	InterlockedExchangeAdd( ( LONG* )Addend, LONG( Value ) );
#else
	InterlockedExchangeAdd64( ( LONGLONG* )Addend, LONGLONG( Value ) );
#endif
}

// CHeapDefault supplies default implementations for as many functions as
// possible so that a heap implementation can be as simple as possible.
class CHeapDefault : public IMemAlloc
{
	// Since we define the debug versions of Alloc/Realloc/Free in this class but
	// not the release versions we implicitly hide the release implementations, which
	// makes it impossible for us to call them in order to implement the debug
	// versions. These using directives pull these three names into this namespace
	// so that we can call them.
	using IMemAlloc::Alloc;
	using IMemAlloc::Realloc;
	using IMemAlloc::Free;

	// Release versions
	// Alloc, Realloc, and Free must be implemented
	virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize ) OVERRIDE { return 0; }

	// Debug versions
	virtual void *Alloc( size_t nSize, const char *pFileName, int nLine ) OVERRIDE { return Alloc( nSize ); }
	virtual void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine ) OVERRIDE { return Realloc(pMem, nSize); }
	virtual void  Free( void *pMem, const char *pFileName, int nLine ) OVERRIDE { Free( pMem ); }
	virtual void *Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine ) OVERRIDE { return 0; }

	// GetSize must be implemented

	// Force file + line information for an allocation
	virtual void PushAllocDbgInfo( const char *pFileName, int nLine ) OVERRIDE {}
	virtual void PopAllocDbgInfo() OVERRIDE {}

	// FIXME: Remove when we have our own allocator
	// these methods of the Crt debug code is used in our codebase currently
	virtual int32 CrtSetBreakAlloc( int32 lNewBreakAlloc ) OVERRIDE { return 0; }
	virtual	int CrtSetReportMode( int nReportType, int nReportMode ) OVERRIDE { return 0; }
	virtual int CrtIsValidHeapPointer( const void *pMem ) OVERRIDE { return 0; }
	virtual int CrtIsValidPointer( const void *pMem, unsigned int size, int access ) OVERRIDE { return 0; }
	virtual int CrtCheckMemory( void ) OVERRIDE { return 0; }
	virtual int CrtSetDbgFlag( int nNewFlag ) OVERRIDE { return 0; }
	virtual void CrtMemCheckpoint( _CrtMemState *pState ) OVERRIDE {}

	// FIXME: Make a better stats interface
	virtual void DumpStats() OVERRIDE {}
	virtual void DumpStatsFileBase( char const *pchFileBase, DumpStatsFormat_t nFormat = FORMAT_TEXT ) OVERRIDE { DumpStats(); }
	virtual size_t ComputeMemoryUsedBy( char const *pchSubStr ) OVERRIDE { return 0; }

	// FIXME: Remove when we have our own allocator
	virtual void* CrtSetReportFile( int nRptType, void* hFile ) OVERRIDE { return 0; }
	virtual void* CrtSetReportHook( void* pfnNewHook ) OVERRIDE { return 0; }
	virtual int CrtDbgReport( int nRptType, const char * szFile,
		int nLine, const char * szModule, const char * pMsg ) OVERRIDE { return 0; }

	virtual int heapchk() OVERRIDE { return _HEAPOK; }
	virtual bool IsDebugHeap() OVERRIDE { return 0; }

	virtual void GetActualDbgInfo( const char *&pFileName, int &nLine ) OVERRIDE { pFileName = ""; nLine = 0; }
	virtual void RegisterAllocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) OVERRIDE {}
	virtual void RegisterDeallocation( const char *pFileName, int nLine, size_t nLogicalSize, size_t nActualSize, unsigned nTime ) OVERRIDE {}

	virtual int GetVersion() OVERRIDE { return 0; }

	virtual void CompactHeap() OVERRIDE {}

	virtual MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler ) OVERRIDE { return 0; }

	virtual void DumpBlockStats( void * ) OVERRIDE {}

	virtual void SetStatsExtraInfo( const char *pMapName, const char *pComment ) OVERRIDE {}

	// Returns 0 if no failure, otherwise the size_t of the last requested chunk
	virtual size_t MemoryAllocFailed() OVERRIDE { return 0; }

	virtual void CompactIncremental() OVERRIDE {}

	virtual void OutOfMemory( size_t nBytesAttempted = 0 ) OVERRIDE {}

	// Region-based allocations
	virtual void *RegionAlloc( int region, size_t nSize ) OVERRIDE { return 0; }
	virtual void *RegionAlloc( int region, size_t nSize, const char *pFileName, int nLine ) OVERRIDE { return 0; }

	// Replacement for ::GlobalMemoryStatus which accounts for unused memory in our system
	virtual void GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) OVERRIDE {}

	// Obtain virtual memory manager interface
	virtual IVirtualMemorySection * AllocateVirtualMemorySection( size_t numMaxBytes ) OVERRIDE { return 0; }

	// Request 'generic' memory stats (returns a list of N named values; caller should assume this list will change over time)
	virtual int GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats ) { return 0; }

	// handles storing allocation info for coroutines
	virtual uint32 GetDebugInfoSize() { return 0; }
	virtual void SaveDebugInfo( void *pvDebugInfo ) {}
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) {}
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) {}
};

class CHeapMemAlloc : public CHeapDefault
{
public:
	CHeapMemAlloc()
	{
		// Do all allocations with the shared process heap so that we can still
		// allocate from one DLL and free in another.
		m_heap = GetProcessHeap();
	}

	// minimal IMemAlloc implementation

	// Release API
public:
	virtual void *Alloc( size_t nSize ) OVERRIDE;
	virtual void *Realloc( void *pMem, size_t nSize ) OVERRIDE;
	virtual void  Free( void *pMem ) OVERRIDE;

	// Returns size of a particular allocation
	// BUGBUG: this function should be 'const'
	virtual size_t GetSize( void *pMem ) OVERRIDE;

	// Return 1 to indicate a healthy heap.
	// BUGBUG: this function should be 'const'
	virtual int CrtCheckMemory( void ) OVERRIDE;

	// BUGBUG: this function should be 'const'
	virtual void DumpStats() OVERRIDE;

	void Init(bool bZeroMemory);

private:

	void OutOfMemory( size_t nBytesAttempted = 0 );

	// Internal allocation calls used to support alignment
	void * Alloc_Unaligned( size_t nSize );
	void * Realloc_Unaligned( void *pMem, size_t nSize );
	void   Free_Unaligned( void *pMem );
	size_t GetSize_Unaligned( void *pMem ) const;

	// Handle to the process heap.
	HANDLE m_heap;
	uint32 m_HeapFlags;

	// Total outstanding bytes allocated.
	volatile size_t m_nOutstandingBytes;

	// Total outstanding committed bytes assuming that all allocations are
	// put on individual 4-KB pages (true when using full PageHeap from
	// App Verifier).
	volatile size_t m_nOutstandingPageHeapBytes;

	// Total outstanding allocations. With PageHeap enabled each allocation
	// requires an extra 4-KB page of address space.
	volatile LONG m_nOutstandingAllocations;
	LONG m_nOldOutstandingAllocations;

	// Total allocations without subtracting freed memory.
	volatile LONG m_nLifetimeAllocations;
	LONG m_nOldLifetimeAllocations;
};

void CHeapMemAlloc::Init( bool bZeroMemory )
{
	m_HeapFlags = bZeroMemory ? HEAP_ZERO_MEMORY : 0;

	// Can't use Msg here because it isn't necessarily initialized yet.
	if ( s_bPageHeapEnabled )
	{
		OutputDebugStringA("PageHeap is on. Memory use will be larger than normal.\n" );
	}
	else
	{
		OutputDebugStringA("PageHeap is off. Memory use will be normal.\n" );
	}
	if( bZeroMemory )
	{
		OutputDebugStringA( "  HEAP_ZERO_MEMORY is specified.\n" );
	}
}

inline size_t CHeapMemAlloc::GetSize_Unaligned( void *pMem ) const
{
	return HeapSize( m_heap, 0, pMem ) - FORCED_ALIGNMENT;
}

inline void *CHeapMemAlloc::Alloc_Unaligned( size_t nSize )
{
	// Ensure that the constructor has run already. Poorly defined
	// order of construction can result in the allocator being used
	// before it is constructed. Which could be bad.
	if ( !m_heap )
		__debugbreak();

	size_t nAdjustedSize = nSize + FORCED_ALIGNMENT;

	void* pMem = HeapAlloc( m_heap, m_HeapFlags, nAdjustedSize );
	if ( !pMem )
	{
		OutOfMemory( nSize );
	}

	InterlockedAddSizeT( &m_nOutstandingBytes, nSize );
	InterlockedAddSizeT( &m_nOutstandingPageHeapBytes, RoundUpToPage( nAdjustedSize ) );
	InterlockedIncrement( &m_nOutstandingAllocations );
	InterlockedIncrement( &m_nLifetimeAllocations );
	return pMem;
}

inline void *CHeapMemAlloc::Realloc_Unaligned( void *pMem, size_t nSize )
{
	size_t nOldSize = GetSize_Unaligned( pMem );
	size_t nOldAdjustedSize = nOldSize + FORCED_ALIGNMENT;
	size_t nAdjustedSize = nSize + FORCED_ALIGNMENT;

	void* pNewMem = HeapReAlloc( m_heap, m_HeapFlags, pMem, nAdjustedSize );
	if ( !pNewMem )
	{
		OutOfMemory( nSize );
	}

	InterlockedAddSizeT( &m_nOutstandingBytes, nSize - nOldSize );
	InterlockedAddSizeT( &m_nOutstandingPageHeapBytes, RoundUpToPage( nAdjustedSize ) );
	InterlockedAddSizeT( &m_nOutstandingPageHeapBytes, 0 - RoundUpToPage( nOldAdjustedSize ) );
	// Outstanding allocation count isn't affected by Realloc, but lifetime allocation count is
	InterlockedIncrement( &m_nLifetimeAllocations );

	return pNewMem;
}

inline void CHeapMemAlloc::Free_Unaligned( void *pMem )
{
	size_t nOldSize = GetSize_Unaligned( pMem );
	size_t nOldAdjustedSize = nOldSize + FORCED_ALIGNMENT;
	InterlockedAddSizeT( &m_nOutstandingBytes, 0 - nOldSize );
	InterlockedAddSizeT( &m_nOutstandingPageHeapBytes, 0 - RoundUpToPage( nOldAdjustedSize ) );
	InterlockedDecrement( &m_nOutstandingAllocations );

	HeapFree( m_heap, 0, pMem );
}

inline void CHeapMemAlloc::OutOfMemory( size_t nBytesAttempted )
{
	// It is crucial to stop here, before calling DumpStats, because if an OOM failure happens
	// in the logging system then DumpStats will trigger it again. This is made more complicated
	// because CUtlBuffer will have updated its size but not its pointer, leading to a buffer
	// that thinks it has more room than it actually does.
	DebuggerBreakIfDebugging();
	// Having PageHeap enabled leads to lots of allocation failures. These
	// these crashes we either need to halt immediately on allocation failures,
	// or print a message and exit. Printing a message and exiting is better
	// for stress testing purposes.

	DumpStats();
	char buffer[256];
	_snprintf( buffer, sizeof( buffer ), FILE_LINE_STRING "***** OUT OF MEMORY! attempted allocation size: %I64d ****\n", (uint64)nBytesAttempted );
	buffer[ ARRAYSIZE(buffer) - 1 ] = 0;
	// Can't use Msg() in a situation like this.
	Plat_DebugString( buffer );

	// If page heap is enabled then exit cleanly to simplify stress testing.
	if ( !s_bPageHeapEnabled )
	{
		DebuggerBreakIfDebugging();
	}

	Plat_ExitProcess( EXIT_FAILURE );
}

inline void CHeapMemAlloc::DumpStats()
{
	const size_t MB = 1024 * 1024;
	Msg( "Sorry -- no stats saved to file memstats.txt when the heap allocator is enabled.\n" );
	// Print requested memory.
	Msg( "%u MB allocated.\n", ( unsigned )( m_nOutstandingBytes / MB ) );
	// Print memory after rounding up to pages.
	Msg( "%u MB memory used assuming maximum PageHeap overhead.\n", ( unsigned )( m_nOutstandingPageHeapBytes / MB ));
	// Print memory after adding in reserved page after every allocation. Do 64-bit calculations
	// because the pageHeap required memory can easily go over 4 GB.
	__int64 pageHeapBytes = m_nOutstandingPageHeapBytes + m_nOutstandingAllocations * 4096LL;
	Msg( "%u MB address space used assuming maximum PageHeap overhead.\n", ( unsigned )( pageHeapBytes / MB ));
	Msg( "%u outstanding allocations (%d delta).\n", ( unsigned )m_nOutstandingAllocations, ( int )( m_nOutstandingAllocations - m_nOldOutstandingAllocations ) );
	Msg( "%u lifetime allocations (%u delta).\n", ( unsigned )m_nLifetimeAllocations, ( unsigned )( m_nLifetimeAllocations - m_nOldLifetimeAllocations ) );

	// Update the numbers on outstanding and lifetime allocation counts so
	// that we can print out deltas.
	m_nOldOutstandingAllocations = m_nOutstandingAllocations;
	m_nOldLifetimeAllocations = m_nLifetimeAllocations;
}

int CHeapMemAlloc::CrtCheckMemory( void )
{
#ifdef _WIN32
	// HeapValidate is supposed to check the entire heap for validity. However testing with
	// intentional heap corruption suggests that it does not. If a block is corrupted and
	// then HeapValidate is called on that block then this is detected, but the same corruption
	// is not detected when passing NULL as the pointer. But, better to have this functionality
	// supported than not.
	BOOL result = HeapValidate( m_heap, 0, NULL );

	return (result != 0) ? 1 : 0;
#else
	// HeapValidate does not exist on the Xbox 360.
	return 1;
#endif
}

// Alignment-enforcing wrappers
#if ( FORCED_ALIGNMENT > 0 )
ASSERT_INVARIANT( FORCED_ALIGNMENT < 256 ); // Alignment offset has to fit in 1 byte
inline void *AlignPointer( void *pUnaligned )
{
	// Offset the pointer to align it and store the offset in the previous byte
	byte nOffset = FORCED_ALIGNMENT - ( ((uintptr_t)pUnaligned) & ( FORCED_ALIGNMENT - 1 ) );
	byte *pAligned = ((byte*)pUnaligned) + nOffset;
	pAligned[ -1 ] = nOffset;
	return pAligned;
}
inline void *UnalignPointer( void *pAligned )
{
	// Get the original unaligned pointer, using the offset stored by AlignPointer()
	byte *pUnaligned = (byte *)pAligned;
	byte nOffset = pUnaligned[ -1 ];
	pUnaligned -= nOffset;
	// Detect corruption of the offset byte (valid offsets range from 1 to FORCED_ALIGNMENT):
	if ( ((uintptr_t)pAligned) % FORCED_ALIGNMENT ) DebuggerBreakIfDebugging();
	if ( ( nOffset < 1 ) || ( nOffset > FORCED_ALIGNMENT ) ) DebuggerBreakIfDebugging();
	return pUnaligned;
}
#else  // FORCED_ALIGNMENT
inline void *AlignPointer( void *pUnaligned ) { return pUnaligned; }
inline void *UnalignPointer( void *pAligned ) { return pAligned; }
#endif // FORCED_ALIGNMENT

inline void *CHeapMemAlloc::Alloc( size_t nSize )
{
	// NOTE: see IMemAlloc 'API Rules'
	//if ( !nSize )
	//	return NULL;
	if ( !nSize )
		nSize = 1;

	return AlignPointer( Alloc_Unaligned( nSize ) );
}

inline void CHeapMemAlloc::Free( void *pMem )
{
	// NOTE: see IMemAlloc 'API Rules'
	if ( !pMem )
		return;
	return Free_Unaligned( UnalignPointer( pMem ) );
}

inline void *CHeapMemAlloc::Realloc( void *pMem, size_t nSize )
{
	// NOTE: see IMemAlloc 'API Rules'
	if ( !pMem )
	{
		return Alloc( nSize );
	}
	if ( !nSize )
	{
		Free( pMem );
		return NULL;
	}

#if ( FORCED_ALIGNMENT == 0 )

	return Realloc_Unaligned( pMem, nSize );

#else  // FORCED_ALIGNMENT

	// Can't use ReAlloc_Unaligned because the leading padding varies, so it will memcpy incorrectly.
	void * pUnaligned = UnalignPointer( pMem );
	size_t nOldSize   = GetSize_Unaligned( pUnaligned );
	void * pAligned   = AlignPointer( Alloc_Unaligned( nSize ) );
	memcpy( pAligned, pMem, MIN( nSize, nOldSize ) );
	Free_Unaligned( pUnaligned );
	return pAligned;

#endif // FORCED_ALIGNMENT
}

inline size_t CHeapMemAlloc::GetSize( void *pMem )
{
	// NOTE: see IMemAlloc 'API Rules'
	if ( !pMem )
		return 0;
	return GetSize_Unaligned( UnalignPointer( pMem ) );
}

void EnableHeapMemAlloc( bool bZeroMemory )
{
	// Place this here to guarantee it is constructed
	// before we call Init.
	static CHeapMemAlloc s_HeapMemAlloc;
	static bool s_initCalled = false;

	if ( !s_initCalled )
	{
		s_HeapMemAlloc.Init( bZeroMemory );
		SetAllocatorObject( &s_HeapMemAlloc );
		s_initCalled = true;
	}
}

void ReserveBottomMemory()
{
	// If we are running a 64-bit build then reserve all addresses below the
	// 4 GB line to push as many pointers as possible above the line.
#ifdef PLATFORM_WINDOWS_PC64
	// Avoid the cost of calling this multiple times.
	static bool s_initialized = false;
	if ( s_initialized )
		return;
	s_initialized = true;

	// Start by reserving large blocks of memory. When those reservations
	// have exhausted the bottom 4 GB then halve the size and try again.
	// The granularity for reserving address space is 64 KB so if we wanted
	// to reserve every single page we would need to continue down to 64 KB.
	// However stopping at 1 MB is sufficient because it prevents the Windows
	// heap (and dlmalloc and the small block heap) from grabbing address space

	// from the bottom 4 GB, while still allowing Steam to allocate a few pages
	// for setting up detours.
	const size_t LOW_MEM_LINE = 0x100000000LL;
	size_t totalReservation = 0;
	size_t numVAllocs = 0;
	size_t numHeapAllocs = 0;
	for ( size_t blockSize = 256 * 1024 * 1024; blockSize >= 1024 * 1024; blockSize /= 2 )
	{
		for (;;)
		{
			void* p = VirtualAlloc( 0, blockSize, MEM_RESERVE, PAGE_NOACCESS );
			if ( !p )
				break;

			if ( (size_t)p >= LOW_MEM_LINE )
			{
				// We don't need this memory, so release it completely.
				VirtualFree( p, 0, MEM_RELEASE );
				break;
			}

			totalReservation += blockSize;
			++numVAllocs;
		}
	}

	// Now repeat the same process but making heap allocations, to use up the
	// already committed heap blocks that are below the 4 GB line. Now we start
	// with 64-KB allocations and proceed down to 16-byte allocations.
	HANDLE heap = GetProcessHeap();
	for ( size_t blockSize = 64 * 1024; blockSize >= 16; blockSize /= 2 )
	{
		for (;;)
		{
			void* p = HeapAlloc( heap, 0, blockSize );
			if ( !p )
				break;

			if ( (size_t)p >= LOW_MEM_LINE )
			{
				// We don't need this memory, so release it completely.
				HeapFree( heap, 0, p );
				break;
			}

			totalReservation += blockSize;
			++numHeapAllocs;
		}
	}

	// Print diagnostics showing how many allocations we had to make in order to
	// reserve all of low memory. In one test run it took 55 virtual allocs and
	// 85 heap allocs. Note that since the process may have multiple heaps (each
	// CRT seems to have its own) there is likely to be a few MB of address space
	// that was previously reserved and is available to be handed out by some allocators.
	//char buffer[1000];
	//sprintf_s( buffer, "Reserved %1.3f MB (%d vallocs, %d heap allocs) to keep allocations out of low-memory.\n",
	//			totalReservation / (1024 * 1024.0), (int)numVAllocs, (int)numHeapAllocs );
	// Can't use Msg here because it isn't necessarily initialized yet.
	//OutputDebugString( buffer );
#endif
}
// Check whether PageHeap (part of App Verifier) has been enabled for this process.
// It specifically checks whether it was enabled by the EnableAppVerifier.bat
// batch file. This can be used to automatically enable -processheap when
// App Verifier is in use.
static bool IsPageHeapEnabled( bool& bETWHeapEnabled )
{
	// Assume false.
	bool result = false;
	bETWHeapEnabled = false;

	// First we get the application's name so we can look in the registry
	// for App Verifier settings.
	HMODULE exeHandle = GetModuleHandle( 0 );
	if ( exeHandle )
	{
		char appName[ MAX_PATH ];
		if ( GetModuleFileNameA( exeHandle, appName, ARRAYSIZE( appName ) ) )
		{
			// Guarantee null-termination -- not guaranteed on Windows XP!
			appName[ ARRAYSIZE( appName ) - 1 ] = 0;
			// Find the file part of the name.
			const char* pFilePart = strrchr( appName, '\\' );
			if ( pFilePart )
			{
				++pFilePart;
				size_t len = strlen( pFilePart );
				if ( len > 0 && pFilePart[ len - 1 ] == ' ' )
				{
					OutputDebugStringA( "Trailing space on executable name! This will cause Application Verifier and ETW Heap tracing to fail!\n" );
					DebuggerBreakIfDebugging();
				}

				// Generate the key name for App Verifier settings for this process.
				char regPathName[ MAX_PATH ];
				_snprintf( regPathName, ARRAYSIZE( regPathName ),
							"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\%s",
							pFilePart );
				regPathName[ ARRAYSIZE( regPathName ) - 1 ] = 0;

				HKEY key;
				LONG regResult = RegOpenKeyA( HKEY_LOCAL_MACHINE,
							regPathName,
							&key );
				if ( regResult == ERROR_SUCCESS )
				{
					// If PageHeapFlags exists then that means that App Verifier is enabled
					// for this application. The StackTraceDatabaseSizeInMB is only
					// set by Valve's enabling batch file so this indicates that
					// a developer at Valve is using App Verifier.
					if ( RegQueryValueExA( key, "StackTraceDatabaseSizeInMB", 0, NULL, NULL, NULL ) == ERROR_SUCCESS &&
								RegQueryValueExA( key, "PageHeapFlags", 0, NULL, NULL, NULL) == ERROR_SUCCESS )
					{
						result = true;
					}

					if ( RegQueryValueExA( key, "TracingFlags", 0, NULL, NULL, NULL) == ERROR_SUCCESS )
						bETWHeapEnabled = true;

					RegCloseKey( key );
				}
			}
		}
	}

	return result;
}

// Check for various allocator overrides such as -processheap and -reservelowmem.
// Returns true if -processheap is enabled, by a command line switch or other method.
bool CheckWindowsAllocSettings( const char* upperCommandLine )
{
	// Are we doing ETW heap profiling?
	bool bETWHeapEnabled = false;
	s_bPageHeapEnabled = IsPageHeapEnabled( bETWHeapEnabled );

	// Should we reserve the bottom 4 GB of RAM in order to flush out pointer
	// truncation bugs? This helps ensure 64-bit compatibility.
	// However this needs to be off by default to avoid causing compatibility problems,
	// with Steam detours and other systems. It should also be disabled when PageHeap
	// is on because for some reason the combination turns into 4 GB of working set, which
	// can easily cause problems.
	if ( strstr( upperCommandLine, "-RESERVELOWMEM" ) && !s_bPageHeapEnabled )
		ReserveBottomMemory();

	// Uninitialized data, including pointers, is often set to 0xFFEEFFEE.
	// If we reserve that block of memory then we can turn these pointer
	// dereferences into crashes a little bit earlier and more reliably.
	// We don't really care whether this allocation succeeds, but it's
	// worth trying. Note that we do this in all cases -- whether we are using
	// -processheap or not.
	VirtualAlloc( (void*)0xFFEEFFEE, 1, MEM_RESERVE, PAGE_NOACCESS );

	// Enable application termination (breakpoint) on heap corruption. This is
	// better than trying to patch it up and continue, both from a security and
	// a bug-finding point of view. Do this always on Windows since the heap is
	// used by video drivers and other in-proc components.
	//HeapSetInformation( NULL, HeapEnableTerminationOnCorruption, NULL, 0 );
	// The HeapEnableTerminationOnCorruption requires a recent platform SDK,
	// so fake it up.
#if defined(PLATFORM_WINDOWS_PC)
	HeapSetInformation( NULL, (HEAP_INFORMATION_CLASS)1, NULL, 0 );
#endif

	bool bZeroMemory = false;
	bool bProcessHeap = false;
	// Should we force using the process heap? This is handy for gathering memory
	// statistics with ETW/xperf. When using App Verifier -processheap is automatically
	// turned on.
	if ( strstr( upperCommandLine, "-PROCESSHEAP" ) )
	{
		bProcessHeap = true;
		bZeroMemory = !!strstr( upperCommandLine, "-PROCESSHEAPZEROMEM" );
	}

	// Unless specifically disabled, turn on -processheap if pageheap or ETWHeap tracing
	// are enabled.
	if ( !strstr( upperCommandLine, "-NOPROCESSHEAP" ) && ( s_bPageHeapEnabled || bETWHeapEnabled ) )
		bProcessHeap = true;

	if ( bProcessHeap )
	{
		// Now all allocations will go through the system heap.
		EnableHeapMemAlloc( bZeroMemory );
	}

	return bProcessHeap;
}

#endif // _WIN32

#endif // !NO_MALLOC_OVERRIDE

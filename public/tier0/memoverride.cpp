//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Insert this file into all projects using the memory system
// It will cause that project to use the shader memory allocator
//
// $NoKeywords: $
//=============================================================================//

#ifdef SN_TARGET_PS3

#ifdef PS3MEMOVERRIDEWRAP
/*
	=========   PS3 LAUNCHER MAIN ELF SECTION  ==================
	=========   Vitaliy   =======   May 2010  ===================

	The implementation of memory allocation wraps in the scenario
	of bootstrapping ELF and PRXs is as follows:

	ELF wraps all memory allocation CRT function here (see --wrap below)

	ELF lets all statically linked libraries to initialize before
	main function runs, it is required in order to be able to load
	any PRXs to initialize some CRT globals and sysmodules data.
	
	ELF tries to load tier0.prx containing the custom memory
	management allocator as soon as possible in main function.

	So this is when allocations can be happening:
	1) global CRT contructors calling malloc (IOBUFs,mutexes,etc.)
	2) main calling malloc indirectly trying to determine boot
	   parameters and where to load tier0.prx module
	>>--->>-- here we already inject tier0 custom allocator!! <<----
    3) global consturctors of tier0.prx before tier0 prx entry
	   point function runs
    4) tier0 prx entry point function returns to bootstrapper's main
	5) everything else

	****** WHAT HAPPENS BEFORE TIER0 CUSTOM ALLOCATOR IS AVAILABLE?

		Since all memory allocation calls are going into our custom
		wrapped malloc/free implementation that doesn't yet have real
		custom allocator, we create a static .bss section array (256Kb)
		and we just hand out 16-byte aligned addresses from this array.
		The memory allocated from this .bss section is essentially never
		freed. Test runs show that roughly 68Kb is actually allocated
		before tier0 custom allocator becomes available.

	****** HOW DO WE INJECT TIER0 CUSTOM ALLOCATOR SO EARLY?

		For this we utilize two tricks.
		
		RUN SOME CODE IN TIER0 BEFORE EVERYTHING ELSE:
			We use attribute "init_priority(101)" on a global struct
			in tier0 to run its constructor function before any other
			global objects in tier0 construct.
			That function calls into ELF to obtain real addresses of
			real CRT functions, constructs the custom memory allocator
			and signals the ELF the pointer to allocator's interface,
			so all other tier0 global objects will already use the
			custom allocator's interface even though tier0's entry point
			didn't run yet.

		EARLY COMMUNICATION ELF-PRX BEFORE PRX ENTRY POINT
			We use CRT wrap function "malloc_stats", we call it with
			special parameters passing data from PRX into ELF wrapper.
			Return value of the function essentially passes data back
			from ELF into PRX thus allowing to handshake in the guts
			of PRX loading code between ELF calling prx_start_module
			and prx entry point.

	=============================================================

	We must be using --wrap=<symbol> on PS3 SNC compiler to wrap
	all calls to malloc-related functions.

	--wrap=<symbol>
		
		Use wrapper functions for <symbol>. If the linker is passed
		the argument '-wrap=foo', it will resolve references to the
		symbol foo to __wrap_foo; in addition, a new symbol __real_foo
		will be created which will resolve to the original foo.
		
		For example, by defining a function __wrap_fopen() that
		implements any desired wrapping logic and that invokes the
		real fopen() call through the __real_foo symbol, we can give
		the linker the argument '--wrap=fopen' and easily intercept
		calls to fopen().

		extern FILE *__real_fopen (const char * restrict filename, const char * restrict mode);

		FILE *__wrap_fopen (const char * restrict filename, const char * restrict mode)
		{
			printf ('open %s\n', filename);
			return __real_fopen (filename, mode);
		}
*/

#define _PS3 1
#include <stdlib.h>
#include "memalloc.h"
#include "dbg.h"
#include "memoverride_ps3.h"
#include "../steam/steam_platform_ps3/steamps3params.h"

#define WRAP_FN_HOOK( name ) __wrap_##name
#define WRAP_FN_REAL( name ) __real_##name

static IMemAlloc *g_p_tier0_memalloc_interface = NULL;
#ifdef g_pMemAlloc
	#undef g_pMemAlloc
#endif
#define g_pMemAlloc g_p_tier0_memalloc_interface

char g_ps3_malloc_startup_buffer[ 256 * 1024 ];
int g_ps3_malloc_startup_index = 0;
inline void * MallocStartupBuffer()
{
	return &g_ps3_malloc_startup_buffer[ g_ps3_malloc_startup_index ];
}
inline void MallocStartupUsed( size_t size )
{
	size = ( size + 0xF )&~0xF;	// round up to a multiple of 16 bytes
	g_ps3_malloc_startup_index += size;
	
	if ( g_ps3_malloc_startup_index > sizeof( g_ps3_malloc_startup_buffer ) )
		abort();
}
inline bool IsMallocStartupAllocation( void *p )
{
	return ( p >= g_ps3_malloc_startup_buffer ) && ( p < &g_ps3_malloc_startup_buffer[ g_ps3_malloc_startup_index ] );
}
inline size_t GetMallocStartupAllocationSize( void *p )
{
	char *pCh = ( char * ) p;
	pCh -= 16;
	size_t *pSize = reinterpret_cast< size_t * >( pCh );
	return *pSize;
}

MemOverrideRawCrtFunctions_t * GetRawCrtFunctions();
SteamPS3Memory_t *GetSteamMemoryFunctions();

_C_STD_BEGIN
extern "C" {

void WRAP_FN_REAL(abort)(void);
void WRAP_FN_HOOK(abort)(void)
{
#ifndef _CERT
	DebuggerBreak();
#endif

	WRAP_FN_REAL(abort)();
}

void * WRAP_FN_REAL(malloc)( size_t size );
void * WRAP_FN_HOOK(malloc)( size_t size )
{
	if ( g_pMemAlloc )
	{
		return g_pMemAlloc->Alloc( size );
	}
	else
	{
		size_t *pSize = (size_t*) MallocStartupBuffer();
		*pSize = size;
		MallocStartupUsed( sizeof( size_t ) );

		void *pvResult = MallocStartupBuffer();
		MallocStartupUsed( size );
		return pvResult;
	}
}

void * WRAP_FN_REAL(calloc)( size_t nelem, size_t size );
void * WRAP_FN_HOOK(calloc)( size_t nelem, size_t size )
{
	void *p = g_pMemAlloc ? g_pMemAlloc->Alloc( nelem * size ) : malloc( nelem * size );
	memset( p, 0, nelem * size );
	return p;
}

void * WRAP_FN_REAL(memalign)(size_t boundary, size_t size_arg);
void * WRAP_FN_HOOK(memalign)(size_t boundary, size_t size_arg)
{
	if ( g_pMemAlloc )
	{
		void * p = g_pMemAlloc->AllocAlign( size_arg, boundary );
		Assert( 0 == ( reinterpret_cast< size_t >( p ) % boundary ) );
		return p;
	}
	else
	{
		return malloc( size_arg );
	}
}

void * WRAP_FN_REAL(realloc)( void *p, size_t size_arg );
void * WRAP_FN_HOOK(realloc)( void *p, size_t size_arg )
{
	if ( !IsMallocStartupAllocation( p ) && g_pMemAlloc )
	{
		return g_pMemAlloc->Realloc( p, size_arg );
	}
	else
	{
		void *pNewData = malloc( size_arg );
		size_t nOldSize = GetMallocStartupAllocationSize( p );
		memcpy( pNewData, p, nOldSize < size_arg ? nOldSize : size_arg );
		free( p );
		return pNewData;
	}
}

void * WRAP_FN_REAL(reallocalign)(void *p, size_t size_arg, size_t boundary);
void * WRAP_FN_HOOK(reallocalign)(void *p, size_t size_arg, size_t boundary)
{
	if ( !IsMallocStartupAllocation( p ) && g_pMemAlloc )
	{
		p = g_pMemAlloc->ReallocAlign( p, size_arg, boundary );
		Assert( 0 == ( reinterpret_cast< size_t >( p ) % boundary ) );
		return p;
	}
	else
	{
		void *pNewData = memalign( boundary, size_arg );
		size_t nOldSize = GetMallocStartupAllocationSize( p );
		memcpy( pNewData, p, nOldSize < size_arg ? nOldSize : size_arg );
		free( p );
		return pNewData;
	}
}

size_t WRAP_FN_REAL(malloc_usable_size)( void *p );
size_t WRAP_FN_HOOK(malloc_usable_size)( void *p )
{
	if ( !IsMallocStartupAllocation( p ) && g_pMemAlloc )
		return g_pMemAlloc->GetSize( p );
	return GetMallocStartupAllocationSize( p );
}

int WRAP_FN_REAL(malloc_stats)(struct malloc_managed_size *mms);
int WRAP_FN_HOOK(malloc_stats)(struct malloc_managed_size *mms)
{
	if ( mms &&
		 mms->current_inuse_size == 0x12345678 &&
		 mms->current_system_size == 0x09ABCDEF )
	{
		// This is our passthrough function to establish
		// real memalloc interface from tier0
		g_pMemAlloc = reinterpret_cast< IMemAlloc * >( mms->max_system_size );
		return reinterpret_cast<int>( GetRawCrtFunctions() );
	}

	if ( mms &&
		mms->current_inuse_size == STEAMPS3_MALLOC_INUSE &&
		mms->current_system_size == STEAMPS3_MALLOC_SYSTEM )
	{
		// Steam is requesting memory struct
		mms->current_inuse_size = STEAMPS3_MALLOC_OK;
		return reinterpret_cast<int>( GetSteamMemoryFunctions() );
	}

	memset( mms, 0, sizeof( *mms ) );
	return 0;
}

void WRAP_FN_REAL(free)( void *p );
void WRAP_FN_HOOK(free)( void *p )
{
	if ( !IsMallocStartupAllocation( p ) && g_pMemAlloc )
		g_pMemAlloc->Free( p );
}

}
_C_STD_END

MemOverrideRawCrtFunctions_t * GetRawCrtFunctions()
{
	using namespace std;

	static MemOverrideRawCrtFunctions_t g_rawfns =
	{
		WRAP_FN_REAL( malloc ),
		WRAP_FN_REAL( calloc ),
		WRAP_FN_REAL( memalign ),
		WRAP_FN_REAL( realloc ),
		WRAP_FN_REAL( reallocalign ),
		WRAP_FN_REAL( malloc_usable_size ),
		WRAP_FN_REAL( malloc_stats ),
		WRAP_FN_REAL( free )
	};

	return &g_rawfns;
}

SteamPS3Memory_t *GetSteamMemoryFunctions()
{
	using namespace std;

	static SteamPS3Memory_t g_steamPS3Memory =
	{
#if defined( STEAM_SHARES_GAME_ALLOCATOR )
		false,								// false=[Steam and game share the tier0 memory allocator]
#else 
		true,								// true=[Steam manages 6Mb chunk, allocated from the game on startup]
#endif
		WRAP_FN_HOOK( malloc ),
		WRAP_FN_HOOK( realloc ),
		WRAP_FN_HOOK( free ),
		WRAP_FN_HOOK( malloc_usable_size )
	};

	return &g_steamPS3Memory;
}

#elif !defined( TIER0_DLL_EXPORT )
/*
	=========   PS3 SECTION FOR ALL MODULES EXCEPT TIER0 ==================
*/

#include "tier0/dbg.h"
#include "tier0/memalloc.h"

#if (defined(_DEBUG) || defined(USE_MEM_DEBUG))
void *operator new( unsigned int nSize, int nBlockUse_UNUSED, const char *pFileName, int nLine )
{
	return MemAlloc_Alloc( nSize, pFileName, nLine );
}

void *operator new[] ( unsigned int nSize, int nBlockUse_UNUSED, const char *pFileName, int nLine )
{
	return MemAlloc_Alloc( nSize, pFileName, nLine );
}
#endif

#endif // PS3MEMOVERRIDEWRAP

#else

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
#define AVOID_INCLUDING_ALGORITHM

#undef PROTECTED_THINGS_ENABLE   // allow use of _vsnprintf

#include <stdlib.h>
#include "platform.h"
extern "C" void __cdecl WriteMiniDump( void	);
inline void __cdecl VPurecallHandler()
{
	DebuggerBreakIfDebugging();	// give the debugger a chance to catch first
	WriteMiniDump();
	Plat_ExitProcess( EXIT_FAILURE );
}

#if defined( _WIN32 ) && !defined( _X360 )
#define WIN_32_LEAN_AND_MEAN
#include <windows.h>
// set Windows pure virtual handler
_purecall_handler OldPurecallHandler = _set_purecall_handler( VPurecallHandler );
#elif defined( POSIX ) && !defined( _PS3 )
// set OSX/Linux pure virtual handler
extern "C" void __cxa_pure_virtual() { VPurecallHandler(); }
#endif

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include <string.h>
#include <stdio.h>
#include "memdbgoff.h"

#ifdef _WIN32
// ARG: crtdbg is necessary for certain definitions below,
// but it also redefines malloc as a macro in release.
// To disable this, we gotta define _DEBUG before including it.. BLEAH!
#define _DEBUG 1
#include "crtdbg.h"
#ifdef NDEBUG
#undef _DEBUG
#endif

// Turn this back off in release mode.
#ifdef NDEBUG
#undef _DEBUG
#endif
#elif POSIX
#define __cdecl
#endif

#if defined(USE_MEM_DEBUG)
#pragma optimize( "", off )
#define inline
#endif

const char *g_pszModule = MKSTRING( MEMOVERRIDE_MODULE );
inline void *AllocUnattributed( size_t nSize )
{
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	return MemAlloc_Alloc(nSize);
#else
	return MemAlloc_Alloc(nSize, ::g_pszModule, 0);
#endif
}

inline void *ReallocUnattributed( void *pMem, size_t nSize )
{
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	return g_pMemAlloc->Realloc(pMem, nSize);
#else
	return g_pMemAlloc->Realloc(pMem, nSize, ::g_pszModule, 0);
#endif
}

#undef inline


//-----------------------------------------------------------------------------
// Standard functions in the CRT that we're going to override to call our allocator
//-----------------------------------------------------------------------------
#if defined(_WIN32) && !defined(_STATIC_LINKED)

// this magic only works under win32
// under linux this malloc() overrides the libc malloc() and so we
// end up in a recursion (as MemAlloc_Alloc() calls malloc)
#if _MSC_VER >= 1400

#if _MSC_VER >= 1900
#define _CRTNOALIAS
#endif

#define ALLOC_CALL _CRTNOALIAS _CRTRESTRICT 
#define FREE_CALL _CRTNOALIAS 
#else
#define ALLOC_CALL
#define FREE_CALL
#endif

extern "C"
{
	
ALLOC_CALL void *malloc( size_t nSize )
{
	return AllocUnattributed( nSize );
}

FREE_CALL void free( void *pMem )
{
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	g_pMemAlloc->Free(pMem);
#else
	g_pMemAlloc->Free(pMem, ::g_pszModule, 0 );
#endif
}

ALLOC_CALL void *realloc( void *pMem, size_t nSize )
{
	return ReallocUnattributed( pMem, nSize );
}

ALLOC_CALL void *calloc( size_t nCount, size_t nElementSize )
{
	void *pMem = AllocUnattributed( nElementSize * nCount );
	memset(pMem, 0, nElementSize * nCount);
	return pMem;
}

} // end extern "C"

//-----------------------------------------------------------------------------
// Non-standard MSVC functions that we're going to override to call our allocator
//-----------------------------------------------------------------------------
extern "C"
{

// 64-bit
#ifdef _WIN64
void* __cdecl _malloc_base( size_t nSize )
{
	return AllocUnattributed( nSize );
}
#else
void *_malloc_base( size_t nSize )
{
	return AllocUnattributed( nSize );
}
#endif

#if ( defined ( _MSC_VER ) && _MSC_VER >= 1900 )
_CRTRESTRICT void *_calloc_base(size_t nCount, size_t nSize)
{
	void *pMem = AllocUnattributed(nCount*nSize);
	memset(pMem, 0, nCount*nSize);
	return pMem;
}
#else
void *_calloc_base( size_t nSize )
{
	void *pMem = AllocUnattributed( nSize );
	memset(pMem, 0, nSize);
	return pMem;
}
#endif

void *_realloc_base( void *pMem, size_t nSize )
{
	return ReallocUnattributed( pMem, nSize );
}

void *_recalloc_base( void *pMem, size_t nSize )
{
	void *pMemOut = ReallocUnattributed( pMem, nSize );
	if (!pMem)
	{
		memset(pMemOut, 0, nSize);
	}
	return pMemOut;
}

void _free_base( void *pMem )
{
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	g_pMemAlloc->Free(pMem);
#else
	g_pMemAlloc->Free(pMem, ::g_pszModule, 0 );
#endif
}

void *__cdecl _expand_base( void *pMem, size_t nNewSize, int nBlockUse )
{
	Assert( 0 );
	return NULL;
}

// crt
void * __cdecl _malloc_crt(size_t size)
{
	return AllocUnattributed( size );
}

void * __cdecl _calloc_crt(size_t count, size_t size)
{
#if (defined( _MSC_VER ) && _MSC_VER >= 1900)
	return _calloc_base(count, size);
#else
	return _calloc_base( count * size );
#endif
}

void * __cdecl _realloc_crt(void *ptr, size_t size)
{
	return _realloc_base( ptr, size );
}

void * __cdecl _recalloc_crt(void *ptr, size_t count, size_t size)
{
	return _recalloc_base( ptr, size * count );
}

ALLOC_CALL void * __cdecl _recalloc ( void * memblock, size_t count, size_t size )
{
	void *pMem = ReallocUnattributed( memblock, size * count );
	if (!memblock)
	{
		memset(pMem, 0, size * count);
	}
	return pMem;
}

size_t _msize_base( void *pMem )
{
	return g_pMemAlloc->GetSize(pMem);
}

size_t _msize( void *pMem )
{
	return _msize_base(pMem);
}

size_t msize( void *pMem )
{
	return g_pMemAlloc->GetSize(pMem);
}

void *__cdecl _heap_alloc( size_t nSize )
{
	return AllocUnattributed( nSize );
}

void *__cdecl _nh_malloc( size_t nSize, int )
{
	return AllocUnattributed( nSize );
}

void *__cdecl _expand( void *pMem, size_t nSize )
{
	Assert( 0 );
	return NULL;
}

unsigned int _amblksiz = 16; //BYTES_PER_PARA;

#if _MSC_VER >= 1400
HANDLE _crtheap = (HANDLE)1;	// PatM Can't be 0 or CRT pukes
int __active_heap = 1;
#endif //  _MSC_VER >= 1400

size_t __cdecl _get_sbh_threshold( void )
{
	return 0;
}

int __cdecl _set_sbh_threshold( size_t )
{
	return 0;
}

int _heapchk()
{
	return g_pMemAlloc->heapchk();
}

int _heapmin()
{
	return 1;
}

int __cdecl _heapadd( void *, size_t )
{
	return 0;
}

int __cdecl _heapset( unsigned int )
{
	return 0;
}

size_t __cdecl _heapused( size_t *, size_t * )
{
	return 0;
}

#ifdef _WIN32
#include <malloc.h>
int __cdecl _heapwalk( _HEAPINFO * )
{
	return 0;
}
#endif

} // end extern "C"


//-----------------------------------------------------------------------------
// Debugging functions that we're going to override to call our allocator
// NOTE: These have to be here for release + debug builds in case we
// link to a debug static lib!!!
//-----------------------------------------------------------------------------

extern "C"
{
	
void *malloc_db( size_t nSize, const char *pFileName, int nLine )
{
	return MemAlloc_Alloc(nSize, pFileName, nLine);
}

void free_db( void *pMem, const char *pFileName, int nLine )
{
	g_pMemAlloc->Free(pMem, pFileName, nLine);
}

void *realloc_db( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	return g_pMemAlloc->Realloc(pMem, nSize, pFileName, nLine);
}
	
} // end extern "C"

//-----------------------------------------------------------------------------
// These methods are standard MSVC heap initialization + shutdown methods
//-----------------------------------------------------------------------------
extern "C"
{

#if !defined( _X360 )
	int __cdecl _heap_init()
	{
		return g_pMemAlloc != NULL;
	}

	void __cdecl _heap_term()
	{
	}
#endif

}
#endif


//-----------------------------------------------------------------------------
// Prevents us from using an inappropriate new or delete method,
// ensures they are here even when linking against debug or release static libs
//-----------------------------------------------------------------------------
#ifndef NO_MEMOVERRIDE_NEW_DELETE
#if !defined( _OSX )

void *__cdecl operator new( size_t nSize )
{
	return AllocUnattributed( nSize );
}

void *__cdecl operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_Alloc(nSize, pFileName, nLine );
}

void *__cdecl operator new[] ( size_t nSize )
{
	return AllocUnattributed( nSize );
}

void *__cdecl operator new[] ( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_Alloc(nSize, pFileName, nLine);
}

#else

void *__cdecl operator new( size_t nSize ) throw (std::bad_alloc)
{
	return AllocUnattributed( nSize );
}

void *__cdecl operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_Alloc(nSize, pFileName, nLine );
}

void *__cdecl operator new[] ( size_t nSize ) throw (std::bad_alloc)
{
	return AllocUnattributed( nSize );
}

void *__cdecl operator new[] ( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_Alloc(nSize, pFileName, nLine);
}
#endif // !_OSX

void __cdecl operator delete( void *pMem ) throw()
{
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	g_pMemAlloc->Free(pMem);
#else
	g_pMemAlloc->Free(pMem, ::g_pszModule, 0 );
#endif
}

void __cdecl operator delete[] ( void *pMem ) throw()
{
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	g_pMemAlloc->Free(pMem);
#else
	g_pMemAlloc->Free(pMem, ::g_pszModule, 0 );
#endif
}
#endif


//-----------------------------------------------------------------------------
// Override some debugging allocation methods in MSVC
// NOTE: These have to be here for release + debug builds in case we
// link to a debug static lib!!!
//-----------------------------------------------------------------------------
#ifndef _STATIC_LINKED
#ifdef _WIN32

// This here just hides the internal file names, etc of allocations
// made in the c runtime library
#define CRT_INTERNAL_FILE_NAME "C-runtime internal"

class CAttibCRT
{
public:
	CAttibCRT(int nBlockUse) : m_nBlockUse(nBlockUse)
	{
		if (m_nBlockUse == _CRT_BLOCK)
		{
			g_pMemAlloc->PushAllocDbgInfo(CRT_INTERNAL_FILE_NAME, 0);
		}
	}
	
	~CAttibCRT()
	{
		if (m_nBlockUse == _CRT_BLOCK)
		{
			g_pMemAlloc->PopAllocDbgInfo();
		}
	}
	
private:
	int m_nBlockUse;
};


#define AttribIfCrt() CAttibCRT _attrib(nBlockUse)
#elif defined(POSIX) || defined( _PS3 )
#define AttribIfCrt()
#endif // _WIN32


extern "C"
{
	
void *__cdecl _nh_malloc_dbg( size_t nSize, int nFlag, int nBlockUse,
								const char *pFileName, int nLine )
{
	AttribIfCrt();
	return MemAlloc_Alloc(nSize, pFileName, nLine);
}

void *__cdecl _malloc_dbg( size_t nSize, int nBlockUse,
							const char *pFileName, int nLine )
{
	AttribIfCrt();
	return MemAlloc_Alloc(nSize, pFileName, nLine);
}

#if ( defined(_MSC_VER) && ( _MSC_VER >= 1600 ) ) || defined( _X360 )
void *__cdecl _calloc_dbg_impl( size_t nNum, size_t nSize, int nBlockUse, 
								const char * szFileName, int nLine, int * errno_tmp )
{
	return _calloc_dbg( nNum, nSize, nBlockUse, szFileName, nLine );
}
#endif

void *__cdecl _calloc_dbg( size_t nNum, size_t nSize, int nBlockUse,
							const char *pFileName, int nLine )
{
	AttribIfCrt();
	void *pMem = MemAlloc_Alloc(nSize * nNum, pFileName, nLine);
	memset(pMem, 0, nSize * nNum);
	return pMem;
}

void *__cdecl _realloc_dbg( void *pMem, size_t nNewSize, int nBlockUse,
							const char *pFileName, int nLine )
{
	AttribIfCrt();
	return g_pMemAlloc->Realloc(pMem, nNewSize, pFileName, nLine);
}

void *__cdecl _expand_dbg( void *pMem, size_t nNewSize, int nBlockUse,
							const char *pFileName, int nLine )
{
	Assert( 0 );
	return NULL;
}

void __cdecl _free_dbg( void *pMem, int nBlockUse )
{
	AttribIfCrt();
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
	g_pMemAlloc->Free(pMem);
#else
	g_pMemAlloc->Free(pMem, ::g_pszModule, 0 );
#endif
}

size_t __cdecl _msize_dbg( void *pMem, int nBlockUse )
{
#ifdef _WIN32
	return _msize(pMem);
#elif POSIX || _PS3
	Assert( "_msize_dbg unsupported" );
	return 0;
#endif
}


#ifdef _WIN32

#if defined(_DEBUG) && _MSC_VER >= 1300
// X360TBD: aligned and offset allocations may be important on the 360

// aligned base
ALLOC_CALL void *__cdecl _aligned_malloc_base( size_t size, size_t align )
{
	return MemAlloc_AllocAligned( size, align );
}

ALLOC_CALL void *__cdecl _aligned_realloc_base( void *ptr, size_t size, size_t align )
{
	return MemAlloc_ReallocAligned( ptr, size, align );
}

ALLOC_CALL void *__cdecl _aligned_recalloc_base( void *ptr, size_t size, size_t align )
{
	Error( "Unsupported function\n" );
	return NULL;
}

FREE_CALL void __cdecl _aligned_free_base( void *ptr )
{
	MemAlloc_FreeAligned( ptr );
}

// aligned
ALLOC_CALL void * __cdecl _aligned_malloc( size_t size, size_t align )
{
	return _aligned_malloc_base(size, align);
}

ALLOC_CALL void *__cdecl _aligned_realloc(void *memblock, size_t size, size_t align)
{
    return _aligned_realloc_base(memblock, size, align);
}

ALLOC_CALL void * __cdecl _aligned_recalloc( void * memblock, size_t count, size_t size, size_t align )
{
    return _aligned_recalloc_base(memblock, count * size, align);
}

FREE_CALL void __cdecl _aligned_free( void *memblock )
{
    _aligned_free_base(memblock);
}

// aligned offset base
ALLOC_CALL void * __cdecl _aligned_offset_malloc_base( size_t size, size_t align, size_t offset )
{
	Assert( IsPC() || 0 );
	return NULL;
}

ALLOC_CALL void * __cdecl _aligned_offset_realloc_base( void * memblock, size_t size, size_t align, size_t offset)
{
	Assert( IsPC() || 0 );
	return NULL;
}

ALLOC_CALL void * __cdecl _aligned_offset_recalloc_base( void * memblock, size_t size, size_t align, size_t offset)
{
	Assert( IsPC() || 0 );
	return NULL;
}

// aligned offset
ALLOC_CALL void *__cdecl _aligned_offset_malloc(size_t size, size_t align, size_t offset)
{
    return _aligned_offset_malloc_base( size, align, offset );
}

ALLOC_CALL void *__cdecl _aligned_offset_realloc(void *memblock, size_t size, size_t align, size_t offset)
{
    return _aligned_offset_realloc_base( memblock, size, align, offset );
}

ALLOC_CALL void * __cdecl _aligned_offset_recalloc( void * memblock, size_t count, size_t size, size_t align, size_t offset )
{
    return _aligned_offset_recalloc_base( memblock, count * size, align, offset );
}

#endif // _MSC_VER >= 1400

#endif

} // end extern "C"


//-----------------------------------------------------------------------------
// Override some the _CRT debugging allocation methods in MSVC
//-----------------------------------------------------------------------------
#ifdef _WIN32

extern "C"
{
	
int _CrtDumpMemoryLeaks(void)
{
	return 0;
}

_CRT_DUMP_CLIENT _CrtSetDumpClient( _CRT_DUMP_CLIENT dumpClient )
{
	return NULL;
}

int _CrtSetDbgFlag( int nNewFlag )
{
	return g_pMemAlloc->CrtSetDbgFlag( nNewFlag );
}

// 64-bit port.
#define AFNAME(var) __p_ ## var
#define AFRET(var)  &var

#if ( defined( _MSC_VER ) && _MSC_VER >= 1900)
//Do we need to do anything here for VS2015?
#else
int _crtDbgFlag = _CRTDBG_ALLOC_MEM_DF;
int* AFNAME(_crtDbgFlag)(void)
{
	return AFRET(_crtDbgFlag);
}

long _crtBreakAlloc;      /* Break on this allocation */
long* AFNAME(_crtBreakAlloc) (void)
{
	return AFRET(_crtBreakAlloc);
}
#endif

void __cdecl _CrtSetDbgBlockType( void *pMem, int nBlockUse )
{
	DebuggerBreak();
}

_CRT_ALLOC_HOOK __cdecl _CrtSetAllocHook( _CRT_ALLOC_HOOK pfnNewHook )
{
	DebuggerBreak();
	return NULL;
}

long __cdecl _CrtSetBreakAlloc( long lNewBreakAlloc )
{
	return g_pMemAlloc->CrtSetBreakAlloc( lNewBreakAlloc );
}
					 
int __cdecl _CrtIsValidHeapPointer( const void *pMem )
{
	return g_pMemAlloc->CrtIsValidHeapPointer( pMem );
}

int __cdecl _CrtIsValidPointer( const void *pMem, unsigned int size, int access )
{
	return g_pMemAlloc->CrtIsValidPointer( pMem, size, access );
}

int __cdecl _CrtCheckMemory( void )
{
	// FIXME: Remove this when we re-implement the heap
	return g_pMemAlloc->CrtCheckMemory( );
}

int __cdecl _CrtIsMemoryBlock( const void *pMem, unsigned int nSize,
    long *plRequestNumber, char **ppFileName, int *pnLine )
{
	DebuggerBreak();
	return 1;
}

int __cdecl _CrtMemDifference( _CrtMemState *pState, const _CrtMemState * oldState, const _CrtMemState * newState )
{
	DebuggerBreak();
	return FALSE;
}

void __cdecl _CrtMemDumpStatistics( const _CrtMemState *pState )
{
	DebuggerBreak();	
}

void __cdecl _CrtMemCheckpoint( _CrtMemState *pState )
{
	// FIXME: Remove this when we re-implement the heap
	g_pMemAlloc->CrtMemCheckpoint( pState );
}

void __cdecl _CrtMemDumpAllObjectsSince( const _CrtMemState *pState )
{
	DebuggerBreak();
}

void __cdecl _CrtDoForAllClientObjects( void (*pfn)(void *, void *), void * pContext )
{
	DebuggerBreak();
}


//-----------------------------------------------------------------------------
// Methods in dbgrpt.cpp 
//-----------------------------------------------------------------------------
long _crtAssertBusy = -1;

int __cdecl _CrtSetReportMode( int nReportType, int nReportMode )
{
	return g_pMemAlloc->CrtSetReportMode( nReportType, nReportMode );
}

_HFILE __cdecl _CrtSetReportFile( int nRptType, _HFILE hFile )
{
	return (_HFILE)g_pMemAlloc->CrtSetReportFile( nRptType, hFile );
}

_CRT_REPORT_HOOK __cdecl _CrtSetReportHook( _CRT_REPORT_HOOK pfnNewHook )
{
	return (_CRT_REPORT_HOOK)g_pMemAlloc->CrtSetReportHook( pfnNewHook );
}

int __cdecl _CrtDbgReport( int nRptType, const char * szFile,
        int nLine, const char * szModule, const char * szFormat, ... )
{
	static char output[1024];
	va_list args;
	va_start( args, szFormat );
	_vsnprintf( output, sizeof( output )-1, szFormat, args );
	va_end( args );

	return g_pMemAlloc->CrtDbgReport( nRptType, szFile, nLine, szModule, output );
}

#if _MSC_VER >= 1400

#if defined( _DEBUG ) && _MSC_VER < 1900
 
// wrapper which passes no debug info; not available in debug
void __cdecl _invalid_parameter_noinfo(void)
{
    Assert(0);
}

#endif /* defined( _DEBUG ) */

#if defined( _DEBUG ) || defined( USE_MEM_DEBUG )

int __cdecl __crtMessageWindowW( int nRptType, const wchar_t * szFile, const wchar_t * szLine,
								 const wchar_t * szModule, const wchar_t * szUserMessage )
{
	Assert(0);
	return 0;
}

int __cdecl _CrtDbgReportV( int nRptType, const wchar_t *szFile, int nLine, 
						    const wchar_t *szModule, const wchar_t *szFormat, va_list arglist )
{
	Assert(0);
	return 0;
}

int __cdecl _CrtDbgReportW( int nRptType, const wchar_t *szFile, int nLine, 
						    const wchar_t *szModule, const wchar_t *szFormat, ...)
{
	Assert(0);
	return 0;
}

#if ( defined(_MSC_VER) && _MSC_VER >= 1900)
int __cdecl _VCrtDbgReportA(int nRptType, void *pReturnAddr, const char* szFile, int nLine,
                            const char *szModule, const char *szFormat, va_list arglist)
{
	Assert(0);
	return 0;
}
#else
int __cdecl _VCrtDbgReportA( int nRptType, const wchar_t * szFile, int nLine, 
							 const wchar_t * szModule, const wchar_t * szFormat, va_list arglist )
{
	Assert(0);
	return 0;
}
#endif

int __cdecl _CrtSetReportHook2( int mode, _CRT_REPORT_HOOK pfnNewHook )
{
	_CrtSetReportHook( pfnNewHook );
	return 0;
}
 

#endif  /* defined( _DEBUG ) || defined( USE_MEM_DEBUG ) */

extern "C" int __crtDebugCheckCount = FALSE;

extern "C" int __cdecl _CrtSetCheckCount( int fCheckCount )
{
    int oldCheckCount = __crtDebugCheckCount;
    return oldCheckCount;
}

extern "C" int __cdecl _CrtGetCheckCount( void )
{
    return __crtDebugCheckCount;
}

// aligned offset debug
extern "C" void * __cdecl _aligned_offset_recalloc_dbg( void * memblock, size_t count, size_t size, size_t align, size_t offset, const char * f_name, int line_n )
{
	Assert( IsPC() || 0 );
	void *pMem = ReallocUnattributed( memblock, size * count );
	if (!memblock)
	{
		memset(pMem, 0, size * count);
	}

	return pMem;
}

extern "C" void * __cdecl _aligned_recalloc_dbg( void *memblock, size_t count, size_t size, size_t align, const char * f_name, int line_n )
{
    return _aligned_offset_recalloc_dbg(memblock, count, size, align, 0, f_name, line_n);
}

extern "C" void * __cdecl _recalloc_dbg ( void * memblock, size_t count, size_t size, int nBlockUse, const char * szFileName, int nLine )
{
	return _aligned_offset_recalloc_dbg(memblock, count, size, 0, 0, szFileName, nLine);
}

_CRT_REPORT_HOOK __cdecl _CrtGetReportHook( void )
{
	return NULL;
}

#endif
int __cdecl _CrtReportBlockType(const void * pUserData)
{
	return 0;
}


} // end extern "C"
#endif // _WIN32

// Most files include this file, so when it's used it adds an extra .ValveDbg section,
// to help identify debug binaries.
#ifdef _WIN32
	#ifndef NDEBUG // _DEBUG
		#pragma data_seg("ValveDBG") 
		volatile const char* DBG = "*** DEBUG STUB ***";                     
	#endif
#endif

#endif

// Extras added prevent dbgheap.obj from being included - DAL
#ifdef _WIN32

extern "C"
{
size_t __crtDebugFillThreshold = 0;

extern "C" void * __cdecl _heap_alloc_base (size_t size) 
{
    Assert(0);
	return NULL;
}


void * __cdecl _heap_alloc_dbg( size_t nSize, int nBlockUse, const char * szFileName, int nLine)
{
		return _heap_alloc(nSize);
}

// 64-bit
#ifdef _WIN64
static void * __cdecl realloc_help( void * pUserData, size_t * pnNewSize, int nBlockUse,const char * szFileName,
				int nLine, int fRealloc )
{
		Assert(0); // Shouldn't be needed
		return NULL;
}
#else
static void * __cdecl realloc_help( void * pUserData, size_t nNewSize, int nBlockUse, const char * szFileName,
                  int nLine, int fRealloc)
{
		Assert(0); // Shouldn't be needed
		return NULL;
}
#endif

void __cdecl _free_nolock( void * pUserData)
{
		// I don't think the second param is used in memoverride
        _free_dbg(pUserData, 0);
}

void __cdecl _free_dbg_nolock( void * pUserData, int nBlockUse)
{
        _free_dbg(pUserData, 0);
}

_CRT_ALLOC_HOOK __cdecl _CrtGetAllocHook ( void)
{
		Assert(0); 
        return NULL;
}

static int __cdecl CheckBytes( unsigned char * pb, unsigned char bCheck, size_t nSize)
{
        int bOkay = TRUE;
        return bOkay;
}


_CRT_DUMP_CLIENT __cdecl _CrtGetDumpClient ( void)
{
		Assert(0); 
        return NULL;
}

#if _MSC_VER >= 1400
static void __cdecl _printMemBlockData( _locale_t plocinfo, _CrtMemBlockHeader * pHead)
{
}

static void __cdecl _CrtMemDumpAllObjectsSince_stat( const _CrtMemState * state, _locale_t plocinfo)
{
}
#endif
void * __cdecl _aligned_malloc_dbg( size_t size, size_t align, const char * f_name, int line_n)
{
    return _aligned_malloc(size, align);
}

void * __cdecl _aligned_realloc_dbg( void *memblock, size_t size, size_t align,
               const char * f_name, int line_n)
{
    return _aligned_realloc(memblock, size, align);
}

void * __cdecl _aligned_offset_malloc_dbg( size_t size, size_t align, size_t offset,
              const char * f_name, int line_n)
{
    return _aligned_offset_malloc(size, align, offset);
}

void * __cdecl _aligned_offset_realloc_dbg( void * memblock, size_t size, size_t align, 
                 size_t offset, const char * f_name, int line_n)
{
    return _aligned_offset_realloc(memblock, size, align, offset);
}

void __cdecl _aligned_free_dbg( void * memblock)
{
    _aligned_free(memblock);
}

#if _MSC_VER < 1900
size_t __cdecl _CrtSetDebugFillThreshold( size_t _NewDebugFillThreshold)
{
	Assert(0);
    return 0;
}
#endif

//===========================================
// NEW!!! 64-bit

#if (_MSC_VER < 1900) || !defined( _DEBUG )

char * __cdecl _strdup ( const char * string )
{
	int nSize = strlen(string) + 1;
	char *pCopy = (char*)AllocUnattributed( nSize );
	if ( pCopy )
		memcpy( pCopy, string, nSize );
	return pCopy;
}

#endif

#if 0
_TSCHAR * __cdecl _tfullpath_dbg ( _TSCHAR *UserBuf, const _TSCHAR *path, size_t maxlen, int nBlockUse, const char * szFileName, int nLine )
{
	Assert(0);
	return NULL;
}

_TSCHAR * __cdecl _tfullpath ( _TSCHAR *UserBuf, const _TSCHAR *path, size_t maxlen )
{
	Assert(0);
	return NULL;
}

_TSCHAR * __cdecl _tgetdcwd_lk_dbg ( int drive, _TSCHAR *pnbuf, int maxlen, int nBlockUse, const char * szFileName, int nLine )
{
	Assert(0);
	return NULL;
}

_TSCHAR * __cdecl _tgetdcwd_nolock ( int drive, _TSCHAR *pnbuf, int maxlen )
{
	Assert(0);
	return NULL;
}

errno_t __cdecl _tdupenv_s_helper ( _TSCHAR **pBuffer, size_t *pBufferSizeInTChars, const _TSCHAR *varname, int nBlockUse, const char * szFileName, int nLine )
{
	Assert(0);
	return 0;
}

errno_t __cdecl _tdupenv_s_helper ( _TSCHAR **pBuffer, size_t *pBufferSizeInTChars, const _TSCHAR *varname )
{
	Assert(0);
	return 0;
}

_TSCHAR * __cdecl _ttempnam_dbg ( const _TSCHAR *dir, const _TSCHAR *pfx, int nBlockUse, const char * szFileName, int nLine )
{
	Assert(0);
	return 0;
}

_TSCHAR * __cdecl _ttempnam ( const _TSCHAR *dir, const _TSCHAR *pfx )
{
	Assert(0);
	return 0;
}

wchar_t * __cdecl _wcsdup_dbg ( const wchar_t * string, int nBlockUse, const char * szFileName, int nLine )
{
	Assert(0);
	return 0;
}

wchar_t * __cdecl _wcsdup ( const wchar_t * string )
{
	Assert(0);
	return 0;
}
#endif
} // end extern "C"

#if _MSC_VER >= 1400

//-----------------------------------------------------------------------------
// 	XBox Memory Allocator Override
//-----------------------------------------------------------------------------
#if defined( _X360 )
#if defined( _DEBUG ) || defined( USE_MEM_DEBUG )
#include "utlmap.h"

MEMALLOC_DEFINE_EXTERNAL_TRACKING( XMem );

CThreadFastMutex g_XMemAllocMutex;

void XMemAlloc_RegisterAllocation( void *p, DWORD dwAllocAttributes )
{
	if ( !g_pMemAlloc )
	{
		// core xallocs cannot be journaled until system is ready
		return;
	}

	AUTO_LOCK_FM( g_XMemAllocMutex );
	int size = XMemSize( p, dwAllocAttributes );
	MemAlloc_RegisterExternalAllocation( XMem, p, size );
}

void XMemAlloc_RegisterDeallocation( void *p, DWORD dwAllocAttributes )
{
	if ( !g_pMemAlloc )
	{
		// core xallocs cannot be journaled until system is ready
		return;
	}

	AUTO_LOCK_FM( g_XMemAllocMutex );
	int size = XMemSize( p, dwAllocAttributes );
	MemAlloc_RegisterExternalDeallocation( XMem, p, size );
}

#else

#define XMemAlloc_RegisterAllocation( p, a )	((void)0)
#define XMemAlloc_RegisterDeallocation( p, a )	((void)0)

#endif

//-----------------------------------------------------------------------------
//	XMemAlloc
//
//	XBox Memory Allocator Override
//-----------------------------------------------------------------------------
LPVOID WINAPI XMemAlloc( SIZE_T dwSize, DWORD dwAllocAttributes )
{
	LPVOID	ptr;
	XALLOC_ATTRIBUTES *pAttribs = (XALLOC_ATTRIBUTES *)&dwAllocAttributes;
	bool bPhysical = ( pAttribs->dwMemoryType == XALLOC_MEMTYPE_PHYSICAL );

	if ( !bPhysical && !pAttribs->dwHeapTracksAttributes && pAttribs->dwAllocatorId != eXALLOCAllocatorId_XUI )
	{
		MEM_ALLOC_CREDIT();
		switch ( pAttribs->dwAlignment )
		{
		case XALLOC_ALIGNMENT_4:
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
			ptr = MemAlloc_Alloc( dwSize );
#else
			ptr = MemAlloc_Alloc(dwSize, ::g_pszModule, 0);
#endif
			break;
		case XALLOC_ALIGNMENT_8:
			ptr = MemAlloc_AllocAligned( dwSize, 8 );
			break;
		case XALLOC_ALIGNMENT_DEFAULT:
		case XALLOC_ALIGNMENT_16:
		default:
			ptr = MemAlloc_AllocAligned( dwSize, 16 );
			break;
		}
		if ( pAttribs->dwZeroInitialize != 0 )
		{
			memset( ptr, 0, XMemSize( ptr, dwAllocAttributes ) );
		}
		return ptr;
	}

	ptr = XMemAllocDefault( dwSize, dwAllocAttributes );
	if ( ptr )
	{
		XMemAlloc_RegisterAllocation( ptr, dwAllocAttributes );
	}

	return ptr;
}

//-----------------------------------------------------------------------------
//	XMemFree
//
//	XBox Memory Allocator Override
//-----------------------------------------------------------------------------
VOID WINAPI XMemFree( PVOID pAddress, DWORD dwAllocAttributes )
{
	if ( !pAddress )
	{
		return;
	}

	XALLOC_ATTRIBUTES *pAttribs = (XALLOC_ATTRIBUTES *)&dwAllocAttributes;
	bool bPhysical = ( pAttribs->dwMemoryType == XALLOC_MEMTYPE_PHYSICAL );

	if ( !bPhysical && !pAttribs->dwHeapTracksAttributes && pAttribs->dwAllocatorId != eXALLOCAllocatorId_XUI )
	{
		switch ( pAttribs->dwAlignment )
		{
		case XALLOC_ALIGNMENT_4:
#if !defined(USE_LIGHT_MEM_DEBUG) && !defined(USE_MEM_DEBUG)
			return g_pMemAlloc->Free( pAddress );
#else
			return g_pMemAlloc->Free( pAddress, ::g_pszModule, 0 );
#endif
		default:
			return MemAlloc_FreeAligned( pAddress );
		}
		return;
	}

	XMemAlloc_RegisterDeallocation( pAddress, dwAllocAttributes );

	XMemFreeDefault( pAddress, dwAllocAttributes );
}

//-----------------------------------------------------------------------------
//	XMemSize
//
//	XBox Memory Allocator Override
//-----------------------------------------------------------------------------
SIZE_T WINAPI XMemSize( PVOID pAddress, DWORD dwAllocAttributes )
{
	XALLOC_ATTRIBUTES *pAttribs = (XALLOC_ATTRIBUTES *)&dwAllocAttributes;
	bool bPhysical = ( pAttribs->dwMemoryType == XALLOC_MEMTYPE_PHYSICAL );

	if ( !bPhysical && !pAttribs->dwHeapTracksAttributes && pAttribs->dwAllocatorId != eXALLOCAllocatorId_XUI )
	{
		switch ( pAttribs->dwAlignment )
		{
		case XALLOC_ALIGNMENT_4:
			return g_pMemAlloc->GetSize( pAddress );
		default:
			return MemAlloc_GetSizeAligned( pAddress );
		}
	}

	return XMemSizeDefault( pAddress, dwAllocAttributes );
}
#endif // _X360

#define MAX_LANG_LEN        64  /* max language name length */
#define MAX_CTRY_LEN        64  /* max country name length */
#define MAX_MODIFIER_LEN    0   /* max modifier name length - n/a */
#define MAX_LC_LEN          (MAX_LANG_LEN+MAX_CTRY_LEN+MAX_MODIFIER_LEN+3)

#if _MSC_VER >= 1700 // VS 11 (VS 2012)
// Copied from C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\crt\src\mtdll.h
#ifndef _SETLOC_STRUCT_DEFINED
struct _is_ctype_compatible {
        unsigned long id;
        int is_clike;
};

typedef struct setloc_struct {
    /* getqloc static variables */
    wchar_t *pchLanguage;
    wchar_t *pchCountry;
    int iLocState;
    int iPrimaryLen;
    BOOL bAbbrevLanguage;
    BOOL bAbbrevCountry;
    UINT        _cachecp;
    wchar_t     _cachein[MAX_LC_LEN];
    wchar_t     _cacheout[MAX_LC_LEN];
    /* _setlocale_set_cat (LC_CTYPE) static variable */
    struct _is_ctype_compatible _Loc_c[5];
    wchar_t _cacheLocaleName[LOCALE_NAME_MAX_LENGTH];
} _setloc_struct, *_psetloc_struct;
#define _SETLOC_STRUCT_DEFINED
#endif  /* _SETLOC_STRUCT_DEFINED */

_CRTIMP extern unsigned long __cdecl __threadid(void);
#define _threadid   (__threadid())
_CRTIMP extern uintptr_t __cdecl __threadhandle(void);
#define _threadhandle   (__threadhandle())

/* Structure for each thread's data */

struct _tiddata {
    unsigned long   _tid;       /* thread ID */


    uintptr_t _thandle;         /* thread handle */

    int     _terrno;            /* errno value */
    unsigned long   _tdoserrno; /* _doserrno value */
    unsigned int    _fpds;      /* Floating Point data segment */
    unsigned long   _holdrand;  /* rand() seed value */
    char *      _token;         /* ptr to strtok() token */
    wchar_t *   _wtoken;        /* ptr to wcstok() token */
    unsigned char * _mtoken;    /* ptr to _mbstok() token */

    /* following pointers get malloc'd at runtime */
    char *      _errmsg;        /* ptr to strerror()/_strerror() buff */
    wchar_t *   _werrmsg;       /* ptr to _wcserror()/__wcserror() buff */
    char *      _namebuf0;      /* ptr to tmpnam() buffer */
    wchar_t *   _wnamebuf0;     /* ptr to _wtmpnam() buffer */
    char *      _namebuf1;      /* ptr to tmpfile() buffer */
    wchar_t *   _wnamebuf1;     /* ptr to _wtmpfile() buffer */
    char *      _asctimebuf;    /* ptr to asctime() buffer */
    wchar_t *   _wasctimebuf;   /* ptr to _wasctime() buffer */
    void *      _gmtimebuf;     /* ptr to gmtime() structure */
    char *      _cvtbuf;        /* ptr to ecvt()/fcvt buffer */
    unsigned char _con_ch_buf[MB_LEN_MAX];
                                /* ptr to putch() buffer */
    unsigned short _ch_buf_used;   /* if the _con_ch_buf is used */

    /* following fields are needed by _beginthread code */
    void *      _initaddr;      /* initial user thread address */
    void *      _initarg;       /* initial user thread argument */

    /* following three fields are needed to support signal handling and
     * runtime errors */
    void *      _pxcptacttab;   /* ptr to exception-action table */
    void *      _tpxcptinfoptrs; /* ptr to exception info pointers */
    int         _tfpecode;      /* float point exception code */

#if ( defined( _MSC_VER ) && _MSC_VER >= 1900)
	void *ptmbcinfo_dummy;
	void *ptlocinfo_dummy;
#else
    /* pointer to the copy of the multibyte character information used by
     * the thread */
    pthreadmbcinfo  ptmbcinfo;

    /* pointer to the copy of the locale informaton used by the thead */
    pthreadlocinfo  ptlocinfo;
#endif

    int         _ownlocale;     /* if 1, this thread owns its own locale */

    /* following field is needed by NLG routines */
    unsigned long   _NLG_dwCode;

    /*
     * Per-Thread data needed by C++ Exception Handling
     */
    void *      _terminate;     /* terminate() routine */
    void *      _unexpected;    /* unexpected() routine */
    void *      _translator;    /* S.E. translator */
    void *      _purecall;      /* called when pure virtual happens */
    void *      _curexception;  /* current exception */
    void *      _curcontext;    /* current exception context */
    int         _ProcessingThrow; /* for uncaught_exception */
    void *      _curexcspec;    /* for handling exceptions thrown from std::unexpected */
#if defined (_M_X64) || defined (_M_ARM)
    void *      _pExitContext;
    void *      _pUnwindContext;
    void *      _pFrameInfoChain;
#if defined (_WIN64)
    unsigned __int64    _ImageBase;
    unsigned __int64    _ThrowImageBase;
#else  /* defined (_WIN64) */
    unsigned __int32    _ImageBase;
    unsigned __int32    _ThrowImageBase;
#endif  /* defined (_WIN64) */
    void *      _pForeignException;
#elif defined (_M_IX86)
    void *      _pFrameInfoChain;
#endif  /* defined (_M_IX86) */
    _setloc_struct _setloc_data;

    void *      _reserved1;     /* nothing */
    void *      _reserved2;     /* nothing */
    void *      _reserved3;     /* nothing */
#ifdef _M_IX86
    void *      _reserved4;     /* nothing */
    void *      _reserved5;     /* nothing */
#endif  /* _M_IX86 */

    int _cxxReThrow;        /* Set to True if it's a rethrown C++ Exception */

    unsigned long __initDomain;     /* initial domain used by _beginthread[ex] for managed function */
};
#else // _MSC_VER >= 1700 // VS 11 (VS 2012)
struct _is_ctype_compatible {
        unsigned long id;
        int is_clike;
};
typedef struct setloc_struct {
    /* getqloc static variables */
    char *pchLanguage;
    char *pchCountry;
    int iLcidState;
    int iPrimaryLen;
    BOOL bAbbrevLanguage;
    BOOL bAbbrevCountry;
    LCID lcidLanguage;
    LCID lcidCountry;
    /* expand_locale static variables */
    LC_ID       _cacheid;
    UINT        _cachecp;
    char        _cachein[MAX_LC_LEN];
    char        _cacheout[MAX_LC_LEN];
    /* _setlocale_set_cat (LC_CTYPE) static variable */
    struct _is_ctype_compatible _Lcid_c[5];
} _setloc_struct, *_psetloc_struct;

struct _tiddata {
    unsigned long   _tid;       /* thread ID */


    uintptr_t _thandle;         /* thread handle */

    int     _terrno;            /* errno value */
    unsigned long   _tdoserrno; /* _doserrno value */
    unsigned int    _fpds;      /* Floating Point data segment */
    unsigned long   _holdrand;  /* rand() seed value */
    char *      _token;         /* ptr to strtok() token */
    wchar_t *   _wtoken;        /* ptr to wcstok() token */
    unsigned char * _mtoken;    /* ptr to _mbstok() token */

    /* following pointers get malloc'd at runtime */
    char *      _errmsg;        /* ptr to strerror()/_strerror() buff */
    wchar_t *   _werrmsg;       /* ptr to _wcserror()/__wcserror() buff */
    char *      _namebuf0;      /* ptr to tmpnam() buffer */
    wchar_t *   _wnamebuf0;     /* ptr to _wtmpnam() buffer */
    char *      _namebuf1;      /* ptr to tmpfile() buffer */
    wchar_t *   _wnamebuf1;     /* ptr to _wtmpfile() buffer */
    char *      _asctimebuf;    /* ptr to asctime() buffer */
    wchar_t *   _wasctimebuf;   /* ptr to _wasctime() buffer */
    void *      _gmtimebuf;     /* ptr to gmtime() structure */
    char *      _cvtbuf;        /* ptr to ecvt()/fcvt buffer */
    unsigned char _con_ch_buf[MB_LEN_MAX];
                                /* ptr to putch() buffer */
    unsigned short _ch_buf_used;   /* if the _con_ch_buf is used */

    /* following fields are needed by _beginthread code */
    void *      _initaddr;      /* initial user thread address */
    void *      _initarg;       /* initial user thread argument */

    /* following three fields are needed to support signal handling and
     * runtime errors */
    void *      _pxcptacttab;   /* ptr to exception-action table */
    void *      _tpxcptinfoptrs; /* ptr to exception info pointers */
    int         _tfpecode;      /* float point exception code */

    /* pointer to the copy of the multibyte character information used by
     * the thread */
    pthreadmbcinfo  ptmbcinfo;

    /* pointer to the copy of the locale informaton used by the thead */
    pthreadlocinfo  ptlocinfo;
    int         _ownlocale;     /* if 1, this thread owns its own locale */

    /* following field is needed by NLG routines */
    unsigned long   _NLG_dwCode;

    /*
     * Per-Thread data needed by C++ Exception Handling
     */
    void *      _terminate;     /* terminate() routine */
    void *      _unexpected;    /* unexpected() routine */
    void *      _translator;    /* S.E. translator */
    void *      _purecall;      /* called when pure virtual happens */
    void *      _curexception;  /* current exception */
    void *      _curcontext;    /* current exception context */
    int         _ProcessingThrow; /* for uncaught_exception */
    void *              _curexcspec;    /* for handling exceptions thrown from std::unexpected */
#if defined (_M_IA64) || defined (_M_AMD64)
    void *      _pExitContext;
    void *      _pUnwindContext;
    void *      _pFrameInfoChain;
    unsigned __int64    _ImageBase;
#if defined (_M_IA64)
    unsigned __int64    _TargetGp;
#endif  /* defined (_M_IA64) */
    unsigned __int64    _ThrowImageBase;
    void *      _pForeignException;
#elif defined (_M_IX86)
    void *      _pFrameInfoChain;
#endif  /* defined (_M_IX86) */
    _setloc_struct _setloc_data;

    void *      _encode_ptr;    /* EncodePointer() routine */
    void *      _decode_ptr;    /* DecodePointer() routine */

    void *      _reserved1;     /* nothing */
    void *      _reserved2;     /* nothing */
    void *      _reserved3;     /* nothing */

    int _cxxReThrow;        /* Set to True if it's a rethrown C++ Exception */

    unsigned long __initDomain;     /* initial domain used by _beginthread[ex] for managed function */
};
#endif // _MSC_VER >= 1700 // VS 11 (VS 2012)

typedef struct _tiddata * _ptiddata;

#if (defined( _MSC_VER ) && _MSC_VER >= 1900)
//Do we need anything in here?
#else
class _LocaleUpdate
{
    _locale_tstruct localeinfo;
    _ptiddata ptd;
    bool updated;
    public:
    _LocaleUpdate(_locale_t plocinfo)
        : updated(false)
    {
		/*
        if (plocinfo == NULL)
        {
            ptd = _getptd();
            localeinfo.locinfo = ptd->ptlocinfo;
            localeinfo.mbcinfo = ptd->ptmbcinfo;

            __UPDATE_LOCALE(ptd, localeinfo.locinfo);
            __UPDATE_MBCP(ptd, localeinfo.mbcinfo);
            if (!(ptd->_ownlocale & _PER_THREAD_LOCALE_BIT))
            {
                ptd->_ownlocale |= _PER_THREAD_LOCALE_BIT;
                updated = true;
            }
        }
        else
        {
            localeinfo=*plocinfo;
        }
		*/
    }
    ~_LocaleUpdate()
    {
//        if (updated)
//	        ptd->_ownlocale = ptd->_ownlocale & ~_PER_THREAD_LOCALE_BIT;
    }
    _locale_t GetLocaleT()
    {
        return &localeinfo;
    }
};
#endif //_MSC_VER

#pragma warning(push)
#pragma warning(disable: 4483)
#if _MSC_FULL_VER >= 140050415
#define _NATIVE_STARTUP_NAMESPACE  __identifier("<CrtImplementationDetails>")
#else  /* _MSC_FULL_VER >= 140050415 */
#define _NATIVE_STARTUP_NAMESPACE __CrtImplementationDetails
#endif  /* _MSC_FULL_VER >= 140050415 */

namespace _NATIVE_STARTUP_NAMESPACE
{
    class NativeDll
    {
    private:
        static const unsigned int ProcessDetach   = 0;
        static const unsigned int ProcessAttach   = 1;
        static const unsigned int ThreadAttach    = 2;
        static const unsigned int ThreadDetach    = 3;
        static const unsigned int ProcessVerifier = 4;

    public:

        inline static bool IsInDllMain()
        {
            return false;
        }

        inline static bool IsInProcessAttach()
        {
            return false;
        }

        inline static bool IsInProcessDetach()
        {
            return false;
        }

        inline static bool IsInVcclrit()
        {
            return false;
        }

        inline static bool IsSafeForManagedCode()
        {
            if (!IsInDllMain())
            {
                return true;
            }

            if (IsInVcclrit())
            {
                return true;
            }

            return !IsInProcessAttach() && !IsInProcessDetach();
        }
    };
}
#pragma warning(pop)

#endif // _MSC_VER >= 1400

#endif // !STEAM && !NO_MALLOC_OVERRIDE

#endif // _WIN32


#endif // _PS3

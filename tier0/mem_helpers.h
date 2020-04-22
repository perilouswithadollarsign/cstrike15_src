//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MEM_HELPERS_H
#define MEM_HELPERS_H
#ifdef _WIN32
#pragma once
#endif


// Normally, the runtime libraries like to mess with the memory returned by malloc(), 
// which can create problems trying to repro bugs in debug builds or in the debugger.
//
// If the debugger is present, it initializes data to 0xbaadf00d, which makes floating
// point numbers come out to about 0.1.
//
// If the debugger is not present, and it's a debug build, then you get 0xcdcdcdcd,
// which is about 25 million.
//
// Otherwise, you get uninitialized memory.
//
// In here, we make sure the memory is either random garbage, or it's set to
// 0xffeeffee, which casts to a NAN.
extern bool g_bInitMemory;
#define ApplyMemoryInitializations( pMem, nSize ) if ( !g_bInitMemory ) ; else { DoApplyMemoryInitializations( pMem, nSize ); }
void DoApplyMemoryInitializations( void *pMem, size_t nSize );

#if IsPlatformWindowsPC()
// Use this to override the allocator. This must be called before the first
// allocation. This may not be available on all platforms.
class IMemAlloc;
void SetAllocatorObject( IMemAlloc* pAllocator );
// Check for various allocator overrides such as -processheap and -reservelowmem.
// Returns true if -processheap is enabled, by a command line switch or other method.
bool CheckWindowsAllocSettings( const char* upperCommandLine );
#else
inline bool CheckWindowsAllocSettings( const char* upperCommandLine ) { return false; }
#endif

size_t CalcHeapUsed();

#endif // MEM_HELPERS_H

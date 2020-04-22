//========= Copyright c 2009, Valve Corporation, All rights reserved. ============//
#ifndef TIER1_MINIPROFILER_HASH_HDR
#define TIER1_MINIPROFILER_HASH_HDR

#include "tier0/miniprofiler.h"


#if ENABLE_HARDWARE_PROFILER
extern CLinkedMiniProfiler *HashMiniProfiler( const char *pString ); // may be defined in tier1; not very elegant, but it seems overkill to move this to a new tier1 header
extern CLinkedMiniProfiler *HashMiniProfilerF( const char *pFormat, ... ); // may be defined in tier1; not very elegant, but it seems overkill to move this to a new tier1 header
#define MINI_PROFILE(NAME) CMiniProfilerGuard miniProfilerGuard##__LINE__(HashMiniProfiler(NAME))
#define MINI_PROFILE_F(PRINT,...) CMiniProfilerGuard miniProfilerGuard##__LINE__(HashMiniProfilerF(PRINT,__VA_ARGS__))
#define MINI_PROFILE_CALLS(NAME,NUM_CALLS) CMiniProfilerGuard miniProfilerGuard##__LINE__(HashMiniProfiler(NAME),(NUM_CALLS))
#else
#define MINI_PROFILE(NAME)
#define MINI_PROFILE_F(PRINT,...)
#define MINI_PROFILE_CALLS(NAME,NUM_CALLS)
#endif



#endif
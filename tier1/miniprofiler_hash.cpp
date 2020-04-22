//========= Copyright (c) 2009, Valve Corporation, All rights reserved. ============//
#include "tier0/platform.h"
#include "tier1/miniprofiler_hash.h"

#if ENABLE_HARDWARE_PROFILER
#include "tier1/UtlStringMap.h"
CUtlStringMap<CLinkedMiniProfiler*> g_mapMiniProfilers;
DLL_IMPORT CLinkedMiniProfiler *g_pOtherMiniProfilers;
CLinkedMiniProfiler *HashMiniProfiler( const char *pString )
{
	UtlSymId_t nString = g_mapMiniProfilers.Find( pString );
	if( nString == g_mapMiniProfilers.InvalidIndex() )
	{
		nString = g_mapMiniProfilers.AddString( pString );
		
		// this profiler does not exist yet
		CLinkedMiniProfiler ** ppProfiler = &g_mapMiniProfilers[nString];
		*ppProfiler = new CLinkedMiniProfiler( g_mapMiniProfilers.String( nString ), &g_pOtherMiniProfilers );
		return *ppProfiler;
	}
	return g_mapMiniProfilers[nString];
}
CLinkedMiniProfiler *HashMiniProfilerF( const char *pFormat, ... )
{
	va_list args;
	va_start( args, pFormat );
	char buffer[2048];
	Q_vsnprintf( buffer, sizeof( buffer ), pFormat, args );
	CLinkedMiniProfiler *pProfiler = HashMiniProfiler( buffer );
	va_end( args )   ;
	return pProfiler;
}
#else
CMiniProfiler *HashMiniProfiler( const char *pString )
{
	return NULL;
}
#endif

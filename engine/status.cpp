//===== Copyright 1996-2013, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifdef _WIN32
#if !defined(_X360)
#include "winlite.h"
#endif
#include "tier0/memdbgon.h" // needed because in release builds crtdbg.h is handled specially if USE_MEM_DEBUG is defined
#include "tier0/memdbgoff.h"
#include <crtdbg.h>   // For getting at current heap size
#endif

#include "tier0/vprof.h"
#include "tier0/etwprof.h"
#include "tier0/icommandline.h"
#include "tier0/systeminformation.h"
#include "tier1/utlbuffer.h"
#include "tier1/keyvalues.h"
#include "matchmaking/imatchframework.h"
#include "edict.h"
#include "cdll_engine_int.h"
#include "igame.h"
#include "host_cmd.h"
#include "status.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------

class CKeyValuesDumpForStatusReport : public IKeyValuesDumpContextAsText
{
public:
	CKeyValuesDumpForStatusReport( CUtlBuffer &buffer ) : m_buffer( buffer ) {}

public:
	virtual bool KvWriteText( char const *szText ) { m_buffer.Printf( "%s", szText ); return true; }

protected:
	CUtlBuffer &m_buffer;
};

//-----------------------------------------------------------------------------

static char g_szCombinedStatusBuffer[10240] = {0};
static char g_szMinidumpInfoBuffer[4094] = {0}; // 4094 because ETW marks are limited to this.
static char g_szHostStatusBuffer[1024] = {0};
static char g_szMemoryStatusBuffer[1024] = {0};
static char g_szMatchmakingStatusBuffer[4094] = {0}; // 4094 because ETW marks are limited to this.
static char* g_szAudioStatusBuffer; // Returned by audio code
extern CGlobalVars g_ServerGlobalVariables;
extern int  gHostSpawnCount;
extern int g_nMapLoadCount;

#ifdef DEDICATED
PAGED_POOL_INFO_t g_pagedpoolinfo;
char g_minidumpinfo[ 4094 ] = {0};
#else
extern PAGED_POOL_INFO_t g_pagedpoolinfo;
extern char g_minidumpinfo[ 4094 ];
#endif
extern bool g_bUpdateMinidumpComment;
static PAGED_POOL_INFO_t final;

//-----------------------------------------------------------------------------

static void Status_UpdateMemoryStatus( bool bIncludeFullMemoryInfo )
{
	g_szMemoryStatusBuffer[0] = 0;
	CUtlBuffer buf( g_szMemoryStatusBuffer, sizeof(g_szMemoryStatusBuffer),  CUtlBuffer::TEXT_BUFFER );

	if ( bIncludeFullMemoryInfo )
	{
#ifdef _WIN32
		MEMORYSTATUSEX	memStat;
		ZeroMemory(&memStat, sizeof(MEMORYSTATUSEX));
		memStat.dwLength = sizeof(MEMORYSTATUSEX);
		if ( GlobalMemoryStatusEx( &memStat ) )
		{
			double MbDiv = 1024.0 * 1024.0;

			buf.Printf( "Windows OS memory status:\nmemusage( %d %% )\ntotalPhysical Mb(%.2f)\nfreePhysical Mb(%.2f)\ntotalPaging Mb(%.2f)\nfreePaging Mb(%.2f)\ntotalVirtualMem Mb(%.2f)\nfreeVirtualMem Mb(%.2f)\nextendedVirtualFree Mb(%.2f)\n\n",
						memStat.dwMemoryLoad,
						(double)memStat.ullTotalPhys/MbDiv,
						(double)memStat.ullAvailPhys/MbDiv,
						(double)memStat.ullTotalPageFile/MbDiv,
						(double)memStat.ullAvailPageFile/MbDiv,
						(double)memStat.ullTotalVirtual/MbDiv,
						(double)memStat.ullAvailVirtual/MbDiv,
						(double)memStat.ullAvailExtendedVirtual/MbDiv);
		}
#endif
	}

	if ( g_pMemAlloc->MemoryAllocFailed() )
	{
		buf.Printf( "*** Memory allocation failed for %d bytes! ***\n", g_pMemAlloc->MemoryAllocFailed() );
	}

	GenericMemoryStat_t *pMemoryStats = NULL;
	int nMemoryStatCount = g_pMemAlloc->GetGenericMemoryStats( &pMemoryStats );
	if ( nMemoryStatCount > 0 )
	{
		buf.Printf( "g_pMemAlloc->GetGenericMemoryStats(): %d\n", nMemoryStatCount );

		for ( int i = 0; i < nMemoryStatCount; i++ )
		{
			buf.Printf( "%d. %s : %d \n", i + 1, pMemoryStats[i].name, pMemoryStats[i].value );
		}
	}
	buf.Printf( "\n" );
}

static void Status_UpdateMinidumpCommentBuffer()
{
	g_szMinidumpInfoBuffer[0] = 0;
	CUtlBuffer minidumpInfoBuffer( g_szMinidumpInfoBuffer, sizeof(g_szMinidumpInfoBuffer),  CUtlBuffer::TEXT_BUFFER );

	minidumpInfoBuffer.Printf( "Uptime: %f\n", Plat_FloatTime() );
	if ( g_minidumpinfo[0] != 0 )
	{
		minidumpInfoBuffer.Printf( "%s", g_minidumpinfo );
	}
	minidumpInfoBuffer.Printf( "Active: %s\nSpawnCount %d\nMapLoad Count %d\n", ( game && game->IsActiveApp() ) ? "active" : "inactive", gHostSpawnCount, g_nMapLoadCount );
}

static CUtlBuffer *s_pStatusBuffer = NULL;

static void Status_PrintCallback( const char *fmt, ... )
{
	if ( s_pStatusBuffer )
	{
		va_list		argptr;
		va_start (argptr,fmt);
		s_pStatusBuffer->VaPrintf( fmt, argptr );
		va_end (argptr);
	}
}

static void Status_UpdateHostStatusBuffer()
{
	g_szHostStatusBuffer[0] = 0;
	CUtlBuffer hostStatusBuffer( g_szHostStatusBuffer, sizeof(g_szHostStatusBuffer), CUtlBuffer::TEXT_BUFFER );
	s_pStatusBuffer = &hostStatusBuffer;
	Host_PrintStatus( kCommandSrcCode, Status_PrintCallback, false );
	s_pStatusBuffer = NULL;
}

static void Status_UpdateMatchmakingBuffer()
{
	// matchmaking status text
	g_szMatchmakingStatusBuffer[0] = 0;
	if ( g_pMatchFramework )
	{
		if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
		{
			CUtlBuffer matchmakingStatusBuffer( g_szMatchmakingStatusBuffer, sizeof(g_szMatchmakingStatusBuffer), CUtlBuffer::TEXT_BUFFER );
			
			matchmakingStatusBuffer.Printf( "match session %p\n", pMatchSession );

			CKeyValuesDumpForStatusReport kvDumpContext( matchmakingStatusBuffer );

			matchmakingStatusBuffer.Printf( "session system data:\n" );
			pMatchSession->GetSessionSystemData()->Dump( &kvDumpContext );

			matchmakingStatusBuffer.Printf( "session settings:\n" );
			pMatchSession->GetSessionSettings()->Dump( &kvDumpContext );
		}
	}

	if ( !g_szMatchmakingStatusBuffer[0] )
	{
		V_sprintf_safe( g_szMatchmakingStatusBuffer, "No matchmaking.\n" );
	}
}

static void ConvertNewlinesToTabsinPlace( char *pBuffer )
{
	while( pBuffer && *pBuffer != 0 )
	{
		pBuffer = V_strstr(pBuffer, "\n");
		if ( pBuffer )
		{
			*pBuffer = '\t';
		}
	}
}

static void Status_UpdateBuffers( bool bForETW )
{
	Status_UpdateMinidumpCommentBuffer();
	Status_UpdateMemoryStatus( !bForETW );
	Status_UpdateHostStatusBuffer();
	Status_UpdateMatchmakingBuffer();

#ifndef DEDICATED
	extern char* Status_UpdateAudioBuffer();
	g_szAudioStatusBuffer = Status_UpdateAudioBuffer();
#else
	static char chNoAudio[8] = { 'n', '/', 'a', 0 };
	g_szAudioStatusBuffer = chNoAudio;
#endif

	if ( bForETW )
	{
		// walk through buffers converting \n to \t
		ConvertNewlinesToTabsinPlace( g_szMinidumpInfoBuffer );
		ConvertNewlinesToTabsinPlace( g_szMemoryStatusBuffer );
		ConvertNewlinesToTabsinPlace( g_szHostStatusBuffer );
		ConvertNewlinesToTabsinPlace( g_szMatchmakingStatusBuffer );
		ConvertNewlinesToTabsinPlace( g_szAudioStatusBuffer );
	}
}

static void Status_UpdateCombinedBuffer()
{
	Status_UpdateBuffers( false );

	g_szCombinedStatusBuffer[0] = 0;
	CUtlBuffer combinedStatusBuffer( g_szCombinedStatusBuffer, sizeof(g_szCombinedStatusBuffer),  CUtlBuffer::TEXT_BUFFER );

	combinedStatusBuffer.Printf( "\nGame Info:\n%s", g_szMinidumpInfoBuffer );
	combinedStatusBuffer.Printf( "\nMemory Info:\n%s", g_szMemoryStatusBuffer );
	combinedStatusBuffer.Printf( "\nHost Info:\n%s", g_szHostStatusBuffer );
	combinedStatusBuffer.Printf( "\nMatchmaking Info:\n%s", g_szMatchmakingStatusBuffer );
	combinedStatusBuffer.Printf( "\nAudio Info:\n%s", g_szAudioStatusBuffer );
}

// public functions

const char *Status_GetBuffer()
{
	return g_szCombinedStatusBuffer;
}

void Status_Update()
{
	Status_UpdateCombinedBuffer();
}

void Status_CheckSendETWMark()
{
	// If ETW (Event Tracing for Windows) is available, 
	// collect status text every 5 seconds
#ifdef ETW_MARKS_ENABLED
	// Don't emit status events if nobody is listening.
	if ( ETWIsTracingEnabled() )
	{
		static double fLastETWUpdateTime = -5.0;
		double fCurrentTime = Plat_FloatTime();

		// update status buffer every 5 seconds and write to vtrace/xperf mark
		if ( fCurrentTime - fLastETWUpdateTime > 5.0 )
		{
			fLastETWUpdateTime = fCurrentTime;
			Status_UpdateBuffers( true );
			ETWMark( g_szMinidumpInfoBuffer );
			ETWMark( g_szMemoryStatusBuffer );
			ETWMark( g_szHostStatusBuffer );
			ETWMark( g_szMatchmakingStatusBuffer );
			ETWMark( g_szAudioStatusBuffer );
		}
	}
#endif
}


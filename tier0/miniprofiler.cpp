//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "tier0/platform.h"
#include "tier0/miniprofiler.h"
#include "tier0/cache_hints.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

#ifdef _PS3
#include "ps3/ps3_helpers.h"
#endif

#if defined( PLATFORM_WINDOWS_PC )
#define WIN_32_LEAN_AND_MEAN
#include <windows.h>				// Currently needed for LARGE_INTEGER
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#ifdef IS_WINDOWS_PC
CTHREADLOCALPTR( CMiniProfiler ) s_pLastMiniProfilerTS;
#else
CMiniProfiler *s_pLastMiniProfilerTS;
#endif

static CLinkedMiniProfiler *s_pDummyList = NULL;

class CRootMiniProfiler : public CLinkedMiniProfiler
{
public:
	CRootMiniProfiler( void ) : CLinkedMiniProfiler( "DummyRoot", &s_pDummyList )
	{
		s_pLastMiniProfilerTS = this;
	}
};
CRootMiniProfiler g_rootMiniProfiler;

#if defined( STATIC_LINK ) || defined( _LINUX )
// in static link scenario, we don't need any extra linkage specified where we define our variables
#undef MINIPROFILER_DLL_LINKAGE
#define MINIPROFILER_DLL_LINKAGE
#else
extern "C"
{
#endif
	MINIPROFILER_DLL_LINKAGE CMiniProfiler *g_pRootMiniProfiler = &g_rootMiniProfiler;
	MINIPROFILER_DLL_LINKAGE CLinkedMiniProfiler *g_pGlobalMiniProfilers = NULL;
	MINIPROFILER_DLL_LINKAGE CLinkedMiniProfiler *g_pAssertMiniProfilers = NULL;
	MINIPROFILER_DLL_LINKAGE CMiniProfiler *g_pLastMiniProfiler = &g_rootMiniProfiler;
	MINIPROFILER_DLL_LINKAGE uint32 g_nMiniProfilerFrame = 0;
#if defined( STATIC_LINK ) || defined( _LINUX )
#else
}
#endif

int64 GetHardwareClockReliably()
{
	int64 res = 0;
#if ENABLE_MINI_PROFILER && IS_WINDOWS_PC
	QueryPerformanceCounter( ( LARGE_INTEGER* )&res );
#endif
	return res;
}

CMiniProfiler* PushMiniProfilerTS( CMiniProfiler *pProfiler )
{
	CMiniProfiler *pLast = s_pLastMiniProfilerTS;
	if ( !pLast )
	{
		pLast = &g_rootMiniProfiler;
	}
	s_pLastMiniProfilerTS = pProfiler;
	return pLast;
}


CThreadFastMutex g_ProfilerListMutex;
//CInterlockedInt g_LinkedMiniProfilerIdCount;

void AppendMiniProfilerToList( CLinkedMiniProfiler *pProfiler, CLinkedMiniProfiler **ppList )
{
#if ENABLE_MINI_PROFILER
	g_ProfilerListMutex.Lock();
	//pProfiler->m_nId = g_LinkedMiniProfilerIdCount++;
	if( IsDebug() )
	{
		int nProfilerCount = 0; NOTE_UNUSED( nProfilerCount );
		CLinkedMiniProfiler *pTest;
		for( pTest = *ppList; pTest; pTest = pTest->m_pNext )
		{
			nProfilerCount++;
			if( pTest == pProfiler )
			{
				break;
			}
		}
		if( pProfiler == pTest && !pProfiler->m_ppPrev )
			DebuggerBreak(); // the miniprofiler is not yet added to any list (pprev == 0) but it's already in this list (pTest == pProfiler)
		if( !pTest && pProfiler->m_ppPrev )
			DebuggerBreak(); // the profiler is not in this list, but it's already in some list (pNext!=0)
	}

	if( !pProfiler->m_ppPrev )
	{
		// the profiler is not yet in any list

		if ( ppList )
		{
			pProfiler->m_pNext = *ppList;
			pProfiler->m_ppPrev = ppList;
			if( pProfiler->m_pNext )
			{
				pProfiler->m_pNext->m_ppPrev = &pProfiler->m_pNext;
			}
			*ppList = pProfiler;
		}
	}
	else
	{
		if( !ppList )
		{
			// Warning: unhooking something that wasn't removed from the previous list?
			if( IsDebug() )
			{
				DebuggerBreak();
			}
			pProfiler->m_pNext = NULL;
			pProfiler->m_ppPrev = NULL;
		}
	}
	g_ProfilerListMutex.Unlock();
#endif // ENABLE_MINI_PROFILER
}


void RemoveMiniProfilerFromList( CLinkedMiniProfiler *pProfiler ) 
{
#if ENABLE_MINI_PROFILER
	g_ProfilerListMutex.Lock();
	// We need to remove miniprofiler from the list properly. This is an issue because we unload DLLs sometimes. 
	if ( pProfiler->m_ppPrev )
	{
		*pProfiler->m_ppPrev = pProfiler->m_pNext; // that's it: we just remove this object from the list by linking previous object with the next
	}
	if ( pProfiler->m_pNext )
	{
		pProfiler->m_pNext->m_ppPrev = pProfiler->m_ppPrev;
	}

	// unhook the profiler from the list completely, so that we don't try to do it twice
	pProfiler->m_ppPrev = NULL;
	pProfiler->m_pNext = NULL;
	
	g_ProfilerListMutex.Unlock();
#endif // ENABLE_MINI_PROFILER
}

#if ENABLE_HARDWARE_PROFILER
DLL_CLASS_EXPORT  void CMiniProfiler::Publish(const char *szMessage, ...)
{
#ifdef _X360
	if(m_numCalls >= 100 || m_numTimeBaseTicks > 50) // 500 timebase ticks is 1 microsecond
	{
		char szBuf[256];
		va_list args;
		va_start(args, szMessage);
		vsnprintf(szBuf, sizeof(szBuf), szMessage, args);
		PIXAddNamedCounter(float(INT32(m_numTimeBaseTicks-m_numTimeBaseTicksInCallees))*0.02f, "Ex:%s,mcs", szBuf);
		PIXAddNamedCounter(float(INT32(m_numTimeBaseTicks))*0.02f, "%s,mcs", szBuf);
		if(m_numCalls)
			PIXAddNamedCounter((float)(64*INT32(m_numTimeBaseTicks)/INT32(m_numCalls)), "%s,ticks", szBuf);
		PIXAddNamedCounter((float)(INT32(m_numCalls)), "%s,calls", szBuf);
	}
#endif
	Reset();
}
#endif


void CLinkedMiniProfiler::Publish(uint nHistoryMax)
{
#if ENABLE_HARDWARE_PROFILER
	if(nHistoryMax != m_nHistoryMax)
	{
		PurgeHistory();
		delete[]m_pHistory;
		m_nHistoryMax = nHistoryMax;
		if(nHistoryMax)
			m_pHistory = new CMiniProfiler[nHistoryMax];
		else
			m_pHistory = NULL;
		m_nHistoryLength = 0;
		m_nFrameHistoryBegins = g_nMiniProfilerFrame;
	}
	else
	if(m_nHistoryLength >= nHistoryMax)
	{
		PurgeHistory();
	}
	if(m_pHistory)
		m_pHistory[m_nHistoryLength++] = *this;

	CMiniProfiler::Publish(m_szName);
#endif
}


#if ENABLE_HARDWARE_PROFILER
static char g_szFileName[128] = "";
static FILE *g_pPurgeFile = NULL;
#endif

void CLinkedMiniProfiler::PurgeHistory()
{
#if ENABLE_HARDWARE_PROFILER
	if(m_nHistoryLength && g_pPurgeFile)
	{
		size_t len = (strlen(m_szName) + 3) & ~3;
		fwrite(&len, sizeof(len), 1, g_pPurgeFile);
		fwrite(m_szName, len, 1, g_pPurgeFile);
		fwrite(&m_nFrameHistoryBegins, sizeof(m_nFrameHistoryBegins), 1, g_pPurgeFile);
		fwrite(&m_nHistoryLength, sizeof(m_nHistoryLength), 1, g_pPurgeFile);
		fwrite(m_pHistory, sizeof(CMiniProfiler), m_nHistoryLength, g_pPurgeFile);
		m_nHistoryLength = 0; // reset the history, nothing else
		m_nFrameHistoryBegins = g_nMiniProfilerFrame;
	}
#endif
}

extern "C"
MINIPROFILER_DLL_LINKAGE void PublishAll( CLinkedMiniProfiler*pList, uint32 nHistoryMax )
{
#if ENABLE_HARDWARE_PROFILER
	for(CLinkedMiniProfiler *prof = pList; prof; prof = prof->m_pNext)
		prof->Publish(nHistoryMax);	
#endif		
}

void MicroProfilerAddTS( CMicroProfiler *pProfiler, uint64 numTimeBaseTicks )
{
#if ENABLE_MICRO_PROFILER > 0
	ThreadInterlockedExchangeAdd64( ( int64* )&pProfiler->m_numTimeBaseTicks, numTimeBaseTicks );
	ThreadInterlockedIncrement( ( int32* )&pProfiler->m_numCalls );
#endif
}

void PopMiniProfilerTS( CMiniProfiler *pProfiler )
{
	s_pLastMiniProfilerTS = pProfiler;
}






static void GetPerformanceFrequency( int64 *pFreqOut )
{
#ifdef PLATFORM_POSIX
	*pFreqOut = 2000000000;
#elif defined( _PS3 )
	*pFreqOut = 3200000000ll;
#else
	QueryPerformanceFrequency( ( LARGE_INTEGER* ) pFreqOut );
#endif
}

DLL_EXPORT void PublishAllMiniProfilers(int nHistoryMax)
{
#if ENABLE_HARDWARE_PROFILER
	if(nHistoryMax >= 0/*cv_phys_enable_PIX_counters.GetBool()*/)
	{
		if(nHistoryMax && !g_pPurgeFile)
		{
			if(!g_szFileName[0])
			{
				tm lt;
				Plat_GetLocalTime( &lt );
				//SYSTEMTIME st;
				//GetLocalTime(&st);
				//sprintf(g_szFileName, "D:\\mp%02d-%02d-%02d-%02d-%02d-%02d.dmp", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
				sprintf(g_szFileName, "miniprofile%02d%02d-%02d_%02d_%02d.prf", /*lt->tm_year+1900, */lt.tm_mon+1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
			}
			g_pPurgeFile = fopen(g_szFileName, "ab");
			if(g_pPurgeFile)
			{
				int nVersion = 0x0101;
				fwrite(&nVersion, 4, 1, g_pPurgeFile);
#ifdef _X360
				int nFrequency = 49875; // ticks per millisecond
#else
				// even though this is not correct on older computers, I think most modern computers have the multimedia clock that has the same frequency as the CPU
				int64 nActualFrequency = GetCPUInformation().m_Speed;
				int nFrequency = int((nActualFrequency+500) / 1000);
#endif
				fwrite(&nFrequency, 4, 1, g_pPurgeFile);
			}
		}

		PublishAll(g_pPhysicsMiniProfilers,nHistoryMax);
		PublishAll(g_pOtherMiniProfilers,nHistoryMax);
		g_rootMiniProfiler.Reset();
		g_nMiniProfilerFrame ++;

		if(g_pPurgeFile)
		{
			if(nHistoryMax)
				fflush(g_pPurgeFile);
			else
			{
				Msg("Closing profile: '%s'\n", g_szFileName);
				fclose(g_pPurgeFile);
				g_pPurgeFile = NULL;
				g_szFileName[0] = '\0';
			}
		}
	}
#endif
}
 

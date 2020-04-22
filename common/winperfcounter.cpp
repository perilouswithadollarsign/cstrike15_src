//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#ifdef _WIN32
#include "winperfcounter.h"
#ifdef _WIN32
#include <pdh.h>
#include <pdhmsg.h>
#endif

#ifdef GC
#include "gclogger.h"
using namespace GCSDK;
#endif

//-----------------------------------------------------------------------------
// Purpose: threaded reading & access of PDH data
//-----------------------------------------------------------------------------
class CWinPerfCounterReadThread : public CThread
{
public:
	CWinPerfCounterReadThread( CWinPerfCountersPriv *pPrivData ) : m_pPrivData( pPrivData ), m_pPerfCounterMap( NULL )
	{
	}

	void SetPerfCounterMap( const PerfCounter_t *pPerfCounterMap )
	{
		m_pPerfCounterMap = pPerfCounterMap;
	}

	virtual int Run();

private:
	CWinPerfCountersPriv *m_pPrivData;
	const PerfCounter_t *m_pPerfCounterMap;
};


//-----------------------------------------------------------------------------
// Purpose: Pimpl pattern - Hides all the Pdh* stuff from the global include
//-----------------------------------------------------------------------------
class CWinPerfCountersPriv
{
#pragma warning(suppress : 4355) // warning C4355: 'this' : used in base member initializer list
	CWinPerfCountersPriv( int nCounters )  : m_threadQueryPerfCounters( this )
	{
		m_ppdhHCounters = NULL;
#ifdef _WIN32
		m_ppdhHCounters = new PDH_HCOUNTER[nCounters];
#endif
	}

	~CWinPerfCountersPriv()
	{
		delete[] m_ppdhHCounters;
	}

#ifdef DBGFLAG_VALIDATE
	//-----------------------------------------------------------------------------
	// Purpose: Run a global validation pass on all of our data structures and memory
	//			allocations.
	// Input:	validator -		Our global validator object
	//			pchName -		Our name (typically a member var in our container)
	//-----------------------------------------------------------------------------
	void CWinPerfCountersPriv::Validate( CValidator &validator, const char *pchName )
	{
		VALIDATE_SCOPE();

		AUTO_LOCK( m_mutexDataAccess );
		if ( m_ppdhHCounters )
			validator.ClaimArrayMemory( m_ppdhHCounters );

		ValidateObj( m_vecData );
		ValidateObj( m_vecDataInFlight );
	}
#endif // DBGFLAG_VALIDATE

	friend class CWinPerfCounters;
	friend class CWinPerfCounterReadThread;

protected:
#ifdef _WIN32
	PDH_HQUERY m_pdhHQuery;
	PDH_HCOUNTER *m_ppdhHCounters;
	CUtlVector<uint32> m_vecData, m_vecDataInFlight;
	CThreadMutex m_mutexDataAccess;
	CThreadEvent m_eventStartQuery;
	volatile bool m_bThreadRunning;
	CWinPerfCounterReadThread m_threadQueryPerfCounters;
#endif
};


//-----------------------------------------------------------------------------
// Purpose: c'tor
//-----------------------------------------------------------------------------
CWinPerfCounters::CWinPerfCounters( )
{
	m_pPrivData = NULL;
	m_bInited = false;
}


//-----------------------------------------------------------------------------
// Purpose: d'tor
//-----------------------------------------------------------------------------
CWinPerfCounters::~CWinPerfCounters()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: registers the interesting counters via the Pdh (perf data handling) API
//-----------------------------------------------------------------------------
bool CWinPerfCounters::Init(  const PerfCounter_t *counterMap, int nCounters )
{
	VPROF_BUDGET( "CWinPerfCounters::Init", VPROF_BUDGETGROUP_STEAM );
	Assert( nCounters > 0 );
	Assert( counterMap );
	Assert( !m_pPrivData );

	bool bRet = false;
#ifdef _WIN32

	m_pPerfCounterMap = counterMap;
	m_nCounters = nCounters;
	m_pPrivData = new CWinPerfCountersPriv( nCounters );
	AUTO_LOCK( m_pPrivData->m_mutexDataAccess );
	m_pPrivData->m_threadQueryPerfCounters.SetPerfCounterMap( counterMap );
	m_pPrivData->m_vecData.SetSize( m_nCounters );
	m_pPrivData->m_vecDataInFlight.SetSize( m_nCounters );
	m_pPrivData->m_bThreadRunning = true;

	bRet = true;
	PDH_STATUS pdhStat = PdhOpenQuery( NULL, NULL, &m_pPrivData->m_pdhHQuery );
	AssertMsg( ERROR_SUCCESS == pdhStat, "CWinPerfCounters::Init(): Failed to initalize performance data gathering.\n" );
	for ( int i = 0; i < m_nCounters; ++i )
	{
		pdhStat = PdhAddCounter( m_pPrivData->m_pdhHQuery, m_pPerfCounterMap[i].m_rgchPerfObject, NULL, &(m_pPrivData->m_ppdhHCounters[i]) );
		if ( ERROR_SUCCESS != pdhStat && NULL != m_pPerfCounterMap[i].m_rgchPerfObjectAlternative )
		{
			// first counter name didn't work; try our backup if we have one
			pdhStat = PdhAddCounter( m_pPrivData->m_pdhHQuery, m_pPerfCounterMap[i].m_rgchPerfObjectAlternative, NULL, &(m_pPrivData->m_ppdhHCounters[i]) );
		}

		m_pPrivData->m_vecData[i] = 0;
		m_pPrivData->m_vecDataInFlight[i] = 0;

		if ( ERROR_SUCCESS != pdhStat )
		{
			char rgchCounterNames[ 1024 ];

			if ( NULL != m_pPerfCounterMap[ i ].m_rgchPerfObjectAlternative )
			{
				Q_snprintf( rgchCounterNames, Q_ARRAYSIZE( rgchCounterNames ), "%s or %s",
					m_pPerfCounterMap[ i ].m_rgchPerfObject,
					m_pPerfCounterMap[ i ].m_rgchPerfObjectAlternative );
			}
			else
			{
				Q_snprintf( rgchCounterNames, Q_ARRAYSIZE( rgchCounterNames ), "%s", m_pPerfCounterMap[ i ].m_rgchPerfObject );
			}
			if ( m_pPerfCounterMap[i].m_bAssertOnFailure )
				AssertMsg1( false, "CWinPerfCounters::Init():Failed to add %s to performance monitoring.",
					rgchCounterNames );
			else
			{
#ifdef GC
				EmitError( SPEW_GC, "CWinPerfCounters::Init():Failed to add %s to performance monitoring.\n", rgchCounterNames );
#else
				Warning( "CWinPerfCounters::Init():Failed to add %s to performance monitoring.\n", rgchCounterNames );
#endif
				pdhStat = ERROR_SUCCESS;
			}
		}
		bRet &= ( ERROR_SUCCESS == pdhStat );
	}
	m_bInited = true;
	Assert( m_pPrivData->m_vecDataInFlight.Count() == m_nCounters );
	Assert( m_pPrivData->m_vecData.Count() == m_nCounters );

	// take a sample immediately
	TakeSample();
#endif
	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Performs the read of the perf counters being monitored
//-----------------------------------------------------------------------------
bool CWinPerfCounters::TakeSample()
{
#ifdef _WIN32
	if ( m_bInited )  // this gets called before InitOnServerRunning
	{
		if ( !m_pPrivData->m_threadQueryPerfCounters.IsAlive() )
		{
			m_pPrivData->m_threadQueryPerfCounters.Start();
		}
		m_pPrivData->m_eventStartQuery.Set();
		return true;
	}
#endif
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: pokes the measured values into the provided stats struct
//-----------------------------------------------------------------------------
bool CWinPerfCounters::WriteStats( void *pStatsStruct )
{
	bool bRet = false;
#ifdef _WIN32
	AUTO_LOCK( m_pPrivData->m_mutexDataAccess );

	// pull out our already retrieved data
	for ( int i = 0; i < m_nCounters; ++i )
	{
		//Get destination
		byte *dst = (byte *) pStatsStruct + m_pPerfCounterMap[i].m_statsOffset;
		*(int32 *)dst = m_pPrivData->m_vecData[i];

		//perform rollup if necessary
		if ( m_pPerfCounterMap[i].m_bCounterRequiresRollup == true && ( i + 1 < m_nCounters ) )
		{
			while( m_pPerfCounterMap[i].m_rgchPerfObject == m_pPerfCounterMap[i+1].m_rgchPerfObject )
			{
				++i;
				*(int32 *)dst += m_pPrivData->m_vecData[i];
				if ( m_nCounters == i ) break;
			}
		}
	}

	bRet = true;

#endif // _WIN32
	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: collects data on a thread
//-----------------------------------------------------------------------------
int CWinPerfCounterReadThread::Run()
{
	while ( m_pPrivData->m_bThreadRunning )
	{
		// wait to be signalled
		m_pPrivData->m_eventStartQuery.Wait();
		if ( !m_pPrivData->m_bThreadRunning )
			break;

		PDH_STATUS pdhStat = PdhCollectQueryData( m_pPrivData->m_pdhHQuery );

		// pull out all the stats
		PDH_FMT_COUNTERVALUE pdhCounterVal;
		for ( int i = 0; i < m_pPrivData->m_vecDataInFlight.Count(); ++i )
		{
			DWORD fmt = PDH_FMT_LONG;
			switch ( m_pPerfCounterMap[i].m_eFmt )
			{
			case k_EFormatInt:
				fmt = PDH_FMT_LONG;
				break;
			case k_EFormatFloat:
				fmt = PDH_FMT_DOUBLE;
				break;
			default:
				Assert(false);
			}

			pdhStat = PdhGetFormattedCounterValue( m_pPrivData->m_ppdhHCounters[i], fmt, NULL, &pdhCounterVal );
			byte *dst = (byte *)&m_pPrivData->m_vecDataInFlight[i];
			switch ( m_pPerfCounterMap[i].m_eFmt )
			{
			case k_EFormatInt:
				*(int32 *)dst = ERROR_SUCCESS == pdhStat ? pdhCounterVal.longValue : (int) m_pPerfCounterMap[i].m_fUnsetValue;
				break;
			case k_EFormatFloat:
				*(float *)dst = ERROR_SUCCESS == pdhStat ? (float) pdhCounterVal.doubleValue : (float) m_pPerfCounterMap[i].m_fUnsetValue;
				break;
			default:
				Assert(false);
			}
		}

		// swap in the new data
		AUTO_LOCK( m_pPrivData->m_mutexDataAccess );
		m_pPrivData->m_vecData.Swap( m_pPrivData->m_vecDataInFlight );
	}

	return 0;
}



//-----------------------------------------------------------------------------
// Purpose: closes the Pdh Query (which frees system resources)
//-----------------------------------------------------------------------------
void CWinPerfCounters::Shutdown()
{
	if ( m_pPrivData )
	{
		m_pPrivData->m_bThreadRunning = false;
		if ( m_pPrivData->m_threadQueryPerfCounters.IsAlive() )
		{
			m_pPrivData->m_eventStartQuery.Set();
			m_pPrivData->m_threadQueryPerfCounters.Join( 200 );
		}

#ifdef _WIN32
		if (m_bInited)
		{
			PdhCloseQuery( m_pPrivData->m_pdhHQuery );
			m_bInited = false;
		}
#endif
	}

	m_pPerfCounterMap = NULL;

	SAFE_DELETE( m_pPrivData );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Run a global validation pass on all of our data structures and memory
//			allocations.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CWinPerfCounters::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidatePtr( m_pPrivData );
}
#endif // DBGFLAG_VALIDATE


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWinNetworkPerfCounters::CWinNetworkPerfCounters( )
{
	m_unNumInterfaces = 0;
	m_bInited = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWinNetworkPerfCounters::~CWinNetworkPerfCounters()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWinNetworkPerfCounters::Init()
{
	// Enumerate all network interfaces and create counters for each

	bool bRet = false;
	HQUERY hQuery;
	PDH_STATUS pdhStatus = PdhOpenQuery( NULL, 1, & hQuery );

	if ( pdhStatus != ERROR_SUCCESS )
	{
		return false;
	}

	CUtlBuffer  bufCounterList;
    DWORD       dwCounterListSize			= 0;
    CUtlBuffer	bufInstanceList;
    DWORD       dwInstanceListSize			= 0;
    LPTSTR      pszThisInstance				= NULL;

	// Determine the required buffer size for the data. 
    pdhStatus = PdhEnumObjectItems
					(
					NULL,						// reserved
					NULL,						// local machine
					TEXT("Network Interface"),  // object to enumerate
					NULL,	// pass in NULL buffers
					& dwCounterListSize,		// an 0 length to get
					NULL,// required size 
					& dwInstanceListSize,		// of the buffers in chars
					PERF_DETAIL_WIZARD,			// counter detail level
					0
					); 

// Note: old MSDN example tests against ERROR_SUCCESS (works on Win2k, fails on XP, 
	// new (.NET) MSDN example tests against PDH_MORE_DATA (works on XP, fails on Win2k Server).
	// A usenet post has code that tests against both.
    if	(	pdhStatus != ERROR_SUCCESS
		&&	pdhStatus != PDH_MORE_DATA
		)									//lint !e650 !e737 constant out of range for '!=' (code from MSDN)
    {
		// failed to determine buffer size
		return bRet;
    }
	else
	{
		// Allocate the buffers and try the call again.
		bufCounterList.EnsureCapacity( dwCounterListSize * sizeof(TCHAR) );
		bufInstanceList.EnsureCapacity( dwInstanceListSize * sizeof (TCHAR) );

		pdhStatus = PdhEnumObjectItems
						(
						NULL,						// reserved
						NULL,						// local machine
						TEXT("Network Interface"),	// object to enumerate
						(LPTSTR) bufCounterList.Base(),
						& dwCounterListSize,
						(LPTSTR) bufInstanceList.Base(),   
						& dwInstanceListSize,    
						PERF_DETAIL_WIZARD,     // counter detail level
						0
						);     

		if ( pdhStatus != ERROR_SUCCESS ) 
		{
			return bRet;
		}
		else
		{
			// If the machine has multiple network cards with identical names then we need 
			// to count them and append '#n' to the name, beginning with #1 for the first 
			// duplicate.
			typedef CUtlDict< uint >				mapIdenticalInstanceCount_t;
			mapIdenticalInstanceCount_t				mapIdenticalInstanceCount;

			// Walk the return instance list.
			for	(	
				pszThisInstance = (LPTSTR) bufInstanceList.Base();
				* pszThisInstance != 0;
				pszThisInstance += lstrlen( pszThisInstance ) + 1
				)
			{
				// reached our limit
				if ( m_unNumInterfaces >= sm_unMaxNetworkInterfacesToMeasure )
					break;

				CUtlString sThisInstance;

				// If the machine has multiple network cards with identical names then we need 
				// to count them and append '#n' to the name, beginning with #1 for the first 
				// duplicate.
				// note 11/20/2012 I'm not sure this is true anymore, Windows might finally
				// be giving us the instance names we actually need. Which is good because sometimes
				// it's reall " 2" or " _2" on the end. . .
				int iDict = mapIdenticalInstanceCount.Find( pszThisInstance );
				if ( iDict == mapIdenticalInstanceCount.InvalidIndex() )
				{
					mapIdenticalInstanceCount.Insert( pszThisInstance, 1 );
					sThisInstance = pszThisInstance;
				}
				else
				{
					mapIdenticalInstanceCount[iDict]++;
					sThisInstance.Format( "%s #%d", pszThisInstance, mapIdenticalInstanceCount[iDict] );
				}

				CUtlString sBytesSentCounterName;
				sBytesSentCounterName.Format( "\\Network Interface(%s)\\Bytes Sent/sec", sThisInstance.String() );
				CUtlString sBytesRecvCounterName;
				sBytesRecvCounterName.Format( "\\Network Interface(%s)\\Bytes Received/sec", sThisInstance.String() );

				PerfCounter_t &BytesSentCounter = m_rgPerfCounterInfo[ 2 * m_unNumInterfaces ];
				PerfCounter_t &BytesReceivedCounter = m_rgPerfCounterInfo[ 2 * m_unNumInterfaces + 1 ];

				BytesSentCounter.m_rgchPerfObject = strdup( sBytesSentCounterName.Get() );
				BytesSentCounter.m_rgchPerfObjectAlternative = NULL;
				BytesSentCounter.m_statsOffset = offsetof( Stats_t, m_rgunNetworkBytesSentStats ) + ( m_unNumInterfaces * sizeof( uint32 ) );
				BytesSentCounter.m_eFmt = k_EFormatInt;
				BytesSentCounter.m_fUnsetValue = 0;
				BytesSentCounter.m_bAssertOnFailure = false;
				BytesSentCounter.m_bCounterRequiresRollup = false;

				BytesReceivedCounter.m_rgchPerfObject = strdup( sBytesRecvCounterName.Get() );
				BytesReceivedCounter.m_rgchPerfObjectAlternative = NULL;
				BytesReceivedCounter.m_statsOffset = offsetof( Stats_t, m_rgunNetworkBytesReceivedStats ) + ( m_unNumInterfaces * sizeof( uint32 ) );
				BytesReceivedCounter.m_eFmt = k_EFormatInt;
				BytesReceivedCounter.m_fUnsetValue = 0;
				BytesReceivedCounter.m_bAssertOnFailure = false;
				BytesReceivedCounter.m_bCounterRequiresRollup = false;

				m_unNumInterfaces++;

			}

			bRet = m_PerfCounters.Init( m_rgPerfCounterInfo, m_unNumInterfaces*2 );
			m_bInited = bRet;
		}
	}

	PdhCloseQuery( hQuery );
	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWinNetworkPerfCounters::TakeSample()
{
	if ( m_bInited )
		return m_PerfCounters.TakeSample();
	else
		return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWinNetworkPerfCounters::WriteStats( uint64 *pu64BytesSentPerSec, uint64 *pu64BytesRecvPerSec )
{
	if ( !m_bInited )
		return false;

	bool bRet = false;
	if ( m_PerfCounters.WriteStats( &m_Stats ) )
	{
		*pu64BytesSentPerSec = 0;
		*pu64BytesRecvPerSec = 0;
		for ( uint32 i=0; i < m_unNumInterfaces; ++i )
		{
			*pu64BytesSentPerSec += m_Stats.m_rgunNetworkBytesSentStats[i];
			*pu64BytesRecvPerSec += m_Stats.m_rgunNetworkBytesReceivedStats[i];
		}

		bRet = true;
	}

	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWinNetworkPerfCounters::Shutdown()
{
	if ( m_bInited )
		m_PerfCounters.Shutdown();

}
#ifdef DBGFLAG_VALIDATE

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWinNetworkPerfCounters::Validate( CValidator &validator, const char *pchName )
{
	ValidateObj( m_PerfCounters );
	for ( uint32 i=0; i < m_unNumInterfaces; ++i )
	{
		validator.ClaimMemory( (void*) m_rgPerfCounterInfo[2*i].m_rgchPerfObject );
		validator.ClaimMemory( (void*) m_rgPerfCounterInfo[2*i + 1].m_rgchPerfObject );
	}
}

#endif // DBGFLAG_VALIDATE

#endif // _WIN32
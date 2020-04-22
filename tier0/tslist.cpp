//========== Copyright © 2007, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "pch_tier0.h"
#include "tier0/tslist.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#include <stdlib.h>
#include "tier0/threadtools.h"											// for rand()
// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

extern ThreadHandle_t * CreateTestThreads( ThreadFunc_t fnThread, int numThreads, int nProcessorsToDistribute );
extern void JoinTestThreads( ThreadHandle_t *pHandles );

namespace TSListTests
{
int NUM_TEST = 10000;
int NUM_THREADS;
int MAX_THREADS = 8;
int NUM_PROCESSORS = 1;

CInterlockedInt g_nTested;
CInterlockedInt g_nThreads;
CInterlockedInt g_nPushThreads;
CInterlockedInt g_nPopThreads;
CInterlockedInt g_nPushes;
CInterlockedInt g_nPops;
CTSQueue<int, true> g_TestQueue;
CTSList<int> g_TestList;
volatile bool g_bStart;

int *g_pTestBuckets;

CTSListBase g_Test;
TSLNodeBase_t **g_nodes;
int idx = 0;

const char *g_pListType;

class CTestOps
{
public:
	virtual void Push( int item ) = 0;
	virtual bool Pop( int *pResult ) = 0;
	virtual bool Validate() { return true; }
	virtual bool IsEmpty() = 0;
};

bool g_bUseMutex = false;
CThreadConditionalMutex< CThreadFastMutex, &g_bUseMutex > g_TestLock;

class CQueueOps : public CTestOps
{
	void Push( int item )
	{
		g_TestLock.Lock();
		g_TestQueue.PushItem( item );
		g_TestLock.Unlock();
		g_nPushes++;
	}
	bool Pop( int *pResult )
	{
		g_TestLock.Lock();
		if ( g_TestQueue.PopItem( pResult ) )
		{
			g_TestLock.Unlock();
			g_nPops++;
			return true;
		}
		g_TestLock.Unlock();
		return false;
	}
	bool Validate()
	{
		return true; //g_TestQueue.Validate();
	}
	bool IsEmpty()
	{
		return ( g_TestQueue.Count() == 0 );
	}
} g_QueueOps;

class CListOps : public CTestOps
{
	void Push( int item )
	{
		g_TestLock.Lock();
		g_TestList.PushItem( item );
		g_nPushes++;
	}
	bool Pop( int *pResult )
	{
		g_TestLock.Lock();
		if ( g_TestList.PopItem( pResult ) )
		{
			g_TestLock.Unlock();
			g_nPops++;
			return true;
		}
		g_TestLock.Unlock();
		return false;
	}
	bool Validate()
	{
		return true;
	}
	bool IsEmpty()
	{
		return ( g_TestList.Count() == 0 );
	}
} g_ListOps;

CTestOps *g_pTestOps;

void ClearBuckets()
{
	memset( g_pTestBuckets, 0, sizeof(int) * NUM_TEST );
}

void IncBucket( int i )
{
	if ( i < NUM_TEST ) // tests can slop over a bit
	{
		ThreadInterlockedIncrement( &g_pTestBuckets[i] );
	}
}

void DecBucket( int i )
{
	if ( i < NUM_TEST ) // tests can slop over a bit
	{
		ThreadInterlockedDecrement( &g_pTestBuckets[i] );
	}
}

void ValidateBuckets()
{
	for ( int i = 0; i < NUM_TEST; i++ )
	{
		if ( g_pTestBuckets[i] != 0 )
		{
			Msg( "Test bucket %d has an invalid value %d\n", i, g_pTestBuckets[i] );
			DebuggerBreakIfDebugging();
			return;
		}
	}
}

uintp PopThreadFunc( void *)
{
	//ThreadSetDebugName( "PopThread" );
	g_nPopThreads++;
	g_nThreads++;
	while ( !g_bStart )
	{
		ThreadSleep( 1 );
	}
	int ignored;
	for (;;)
	{
		if ( !g_pTestOps->Pop( &ignored ) )
		{
			ThreadPause();
			ThreadSleep(0);
			if ( g_nPushThreads == 0 )
			{
				// Pop the rest 
				while ( g_pTestOps->Pop( &ignored ) )
				{
					ThreadPause();
					ThreadSleep( 0 );
				}
				break;
			}
		}
	}
	g_nThreads--;
	g_nPopThreads--;
	return 0;
}

uintp PushThreadFunc( void * )
{
	//ThreadSetDebugName( "PushThread" );
	g_nPushThreads++;
	g_nThreads++;
	while ( !g_bStart )
	{
		ThreadPause();
		ThreadSleep( 0 );
	}

	while ( ++g_nTested <= NUM_TEST )
	{
		g_pTestOps->Push( g_nTested );
	}
	g_nThreads--;
	g_nPushThreads--;
	return 0;
}

void TestStart()
{
	g_nTested = 0;
	g_nThreads = 0;
	g_nPushThreads = 0;
	g_nPopThreads = 0;
	g_bStart = false;
	g_nPops = g_nPushes = 0;
	ClearBuckets();
}

void TestWait()
{
	while ( g_nThreads < NUM_THREADS )
	{
		ThreadSleep( 0 );
	}
	g_bStart = true;
	while ( g_nThreads > 0 )
	{
		ThreadSleep( 0 );
	}
}

void TestEnd( bool bExpectEmpty = true )
{
	ValidateBuckets();

	if ( g_nPops != g_nPushes )
	{
		Msg( "FAIL: Not all items popped\n" );
		return;
	}

	if ( g_pTestOps->Validate() )
	{
		if ( !bExpectEmpty || g_pTestOps->IsEmpty() )
		{
			Msg("pass\n");
		}
		else
		{
			Msg("FAIL: !IsEmpty()\n");
		}
	}
	else
	{
		Msg("FAIL: !Validate()\n");
	}
}


//--------------------------------------------------
//
//	Shared Tests for CTSQueue and CTSList
//
//--------------------------------------------------
void PushPopTest()
{
	Msg( "%s test: single thread push/pop, in order... ", g_pListType );
	ClearBuckets();
	g_nTested = 0;
	int value;
	while ( g_nTested < NUM_TEST )
	{
		value = g_nTested++;
		g_pTestOps->Push( value );
		IncBucket( value );
	}

	g_pTestOps->Validate();

	while ( g_pTestOps->Pop( &value ) )
	{
		DecBucket( value );
	}
	TestEnd();
}

void PushPopInterleavedTestGuts()
{
	int value;
	for (;;)
	{
		bool bPush = ( rand() % 2 == 0 );
		if ( bPush && ( value = g_nTested++ ) < NUM_TEST )
		{
			g_pTestOps->Push( value );
			IncBucket( value );
		}
		else if ( g_pTestOps->Pop( &value ) )
		{
			DecBucket( value );
		}
		else
		{
			if ( g_nTested >= NUM_TEST )
			{
				break;
			}
		}
	}
}

void PushPopInterleavedTest()
{
	Msg( "%s test: single thread push/pop, interleaved... ", g_pListType );
	srand( Plat_MSTime() );
	g_nTested = 0;
	ClearBuckets();
	PushPopInterleavedTestGuts();
	TestEnd();
}

uintp PushPopInterleavedTestThreadFunc( void * )
{
	ThreadSetDebugName( "PushPopThread" );
	g_nThreads++;
	while ( !g_bStart )
	{
		ThreadSleep( 0 );
	}
	PushPopInterleavedTestGuts();
	g_nThreads--;
	return 0;
}



void STPushMTPop( bool bDistribute )
{
	Msg( "%s test: single thread push, multithread pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	TestStart();
	ThreadHandle_t hPush = CreateSimpleThread( &PushThreadFunc, NULL );
	ThreadHandle_t *arrPops = CreateTestThreads( PopThreadFunc, NUM_THREADS - 1, ( bDistribute ) ? NUM_PROCESSORS : 0 );

	TestWait();
	TestEnd();
	JoinTestThreads( arrPops );
	ThreadJoin( hPush );
	ReleaseThreadHandle( hPush );
}

void MTPushSTPop( bool bDistribute )
{
	Msg( "%s test: multithread push, single thread pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	TestStart();
	ThreadHandle_t hPop = CreateSimpleThread( &PopThreadFunc, NULL );
	ThreadHandle_t* arrPushes = CreateTestThreads( PushThreadFunc, NUM_THREADS - 1, ( bDistribute ) ? NUM_PROCESSORS : 0 );
	
	TestWait();
	TestEnd();
	JoinTestThreads( arrPushes );
	ThreadJoin( hPop );
	ReleaseThreadHandle( hPop );
}


void MTPushMTPop( bool bDistribute )
{
	Msg( "%s test: multithread push, multithread pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	TestStart();
	int ct = 0;
	ThreadHandle_t *threadHandles = (ThreadHandle_t *)stackalloc( NUM_THREADS * sizeof(ThreadHandle_t) );
	int nHandles = 0;

	for ( int i = 0; i < NUM_THREADS / 2 ; i++ )
	{
		ThreadHandle_t hThread = CreateSimpleThread( &PopThreadFunc, NULL );
		threadHandles[nHandles++] = hThread;
		if ( bDistribute )
		{
			int32 mask = 1 << (ct++ % NUM_PROCESSORS);
			ThreadSetAffinity( hThread, mask );
		}
	}
	for ( int i = 0; i < NUM_THREADS / 2 ; i++ )
	{
		ThreadHandle_t hThread = CreateSimpleThread( &PushThreadFunc, NULL );
		threadHandles[nHandles++] = hThread;
		if ( bDistribute )
		{
			int32 mask = 1 << (ct++ % NUM_PROCESSORS);
			ThreadSetAffinity( hThread, mask );
		}
	}

	TestWait();
	TestEnd();

	for ( int i = 0; i < nHandles; i++ )
	{
		ReleaseThreadHandle( threadHandles[i] );
	}
}

void MTPushPopPopInterleaved( bool bDistribute )
{
	Msg( "%s test: multithread interleaved push/pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	srand( Plat_MSTime() );
	TestStart();
	ThreadHandle_t * arrPushPops = CreateTestThreads( &PushPopInterleavedTestThreadFunc, NUM_THREADS, ( bDistribute ) ? NUM_PROCESSORS : 0 );
	TestWait();
	TestEnd();
	JoinTestThreads( arrPushPops );
}



void MTPushSeqPop( bool bDistribute )
{
	Msg( "%s test: multithread push, sequential pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	TestStart();
	ThreadHandle_t * arrPushes = CreateTestThreads( PushThreadFunc, NUM_THREADS, ( bDistribute ) ? NUM_PROCESSORS : 0 );

	TestWait();
	int ignored;
	g_pTestOps->Validate();
	int nPopped = 0;
	while ( g_pTestOps->Pop( &ignored ) )
	{
		nPopped++;
	}
	if ( nPopped != NUM_TEST )
	{
		Msg( "Pops != pushes?\n" );
		DebuggerBreakIfDebugging();
	}
	TestEnd();
	
	JoinTestThreads( arrPushes );
}


void SeqPushMTPop( bool bDistribute )
{
	Msg( "%s test: sequential push, multithread pop, %s", g_pListType, bDistribute ? "distributed..." : "no affinity..." );
	TestStart();
	while ( g_nTested++ < NUM_TEST )
	{
		g_pTestOps->Push( g_nTested );
	}

	ThreadHandle_t * arrPops = CreateTestThreads( PopThreadFunc, NUM_THREADS, ( bDistribute ) ? NUM_PROCESSORS : 0 );

	TestWait();
	TestEnd();
	
	JoinTestThreads( arrPops );
}


#ifdef _PS3
void TestThreadProc( uint64_t id )
{
	printf( "(TS)Hello from PPU thread %lld @%p\n", id, &id );
	sys_ppu_thread_exit( id );
}
uintp TestThreadProc2( void *p )
{
	printf( "(TS)Hello from PPU thread %lld @%p\n", (int64)p, &p );
	return (uintp)p;
}
#endif

void TestThreads()
{
#ifdef _PS3
	printf("(TS)testing threads\n");
	const int numThreads = 40;
	ThreadHandle_t * arrTests = CreateTestThreads( TestThreadProc2, numThreads, false );
	JoinTestThreads( arrTests );
#endif
}


}


void RunSharedTests( int nTests )
{
	using namespace TSListTests;
	TestThreads();
	const CPUInformation &pi = GetCPUInformation();
	NUM_PROCESSORS = pi.m_nLogicalProcessors;
	MAX_THREADS = NUM_PROCESSORS * 2;
	g_pTestBuckets = new int[NUM_TEST];
	while ( nTests-- )
	{
		for ( NUM_THREADS = 2; NUM_THREADS <= MAX_THREADS; NUM_THREADS *= 2)
		{
			Msg( "\nTesting %d threads:\n", NUM_THREADS );
			PushPopTest();
			PushPopInterleavedTest();
			SeqPushMTPop( false );
			STPushMTPop( false );
			MTPushSeqPop( false );
			MTPushSTPop( false );
			MTPushMTPop( false );
			MTPushPopPopInterleaved( false );
			if ( NUM_PROCESSORS > 1 )
			{
				SeqPushMTPop( true );
				STPushMTPop( true );
				MTPushSeqPop( true );
				MTPushSTPop( true );
				MTPushMTPop( true );
				MTPushPopPopInterleaved( true );
			}
		}
	}
	delete[] g_pTestBuckets;
}

bool RunTSListTests( int nListSize, int nTests )
{
	using namespace TSListTests;
	NUM_TEST = nListSize;

#ifdef USE_NATIVE_SLIST

#ifdef _WIN64
	int maxSize = 65536; // FIXME: How should this be computed?
#else
	int maxSize = ( 1 << (sizeof( ((TSLHead_t *)(0))->Depth ) * 8) ) - 1;
#endif

#else
	int maxSize = ( 1 << (sizeof( ((TSLHead_t *)(0))->value.Depth ) * 8) ) - 1;
#endif
	if ( NUM_TEST > maxSize )
	{
		Msg( "TSList cannot hold more that %d nodes\n", maxSize );
		return false;
	}


	g_pTestOps = &g_ListOps;
	g_pListType = "CTSList";

	RunSharedTests( nTests );

	Msg("Tests done, purging test memory..." );
	g_TestList.Purge();
	Msg( "done\n");
	return true;
}

bool RunTSQueueTests( int nListSize, int nTests )
{
	using namespace TSListTests;
	NUM_TEST = nListSize;

	g_pTestOps = &g_QueueOps;
	g_pListType = "CTSQueue";

	RunSharedTests( nTests );

	Msg("Tests done, purging test memory..." );
	g_TestQueue.Purge();
	Msg( "done\n");
	return true;
}

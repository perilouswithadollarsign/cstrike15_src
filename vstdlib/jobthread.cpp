//========== Copyright 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#if defined( _WIN32 ) && !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "tier0/dbg.h"
#include "tier0/tslist.h"
#include "tier0/icommandline.h"
#include "tier0/threadtools.h"
#include "vstdlib/jobthread.h"
#include "vstdlib/random.h"
#include "tier1/functors.h"
#include "tier1/fmtstr.h"
#include "tier1/utlvector.h"
#include "tier1/generichash.h"
#include "tier0/fasttimer.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#ifdef _PS3
#include "tls_ps3.h"
#include "ps3/ps3_win32stubs.h"
#endif

#include "tier0/memdbgon.h"


class CJobThread;

//-----------------------------------------------------------------------------

inline void ServiceJobAndRelease( CJob *pJob, int iThread = -1 )
{
	// TryLock() would only fail if another thread has entered
	// Execute() or Abort()
	if ( !pJob->IsFinished() && pJob->TryLock() )
	{
		// ...service the request
		pJob->SetServiceThread( iThread );
		pJob->Execute();
		pJob->Unlock();
	}
	pJob->Release();
}

//-----------------------------------------------------------------------------

#pragma pack(push)
#pragma pack(8)
class ALIGN16 CJobQueue
{
public:
	CJobQueue() :
		m_nItems( 0 ),
		m_nMaxItems( INT_MAX )
	{
	}

	int Count()
	{
		return m_nItems;
	}

	int Count( JobPriority_t priority )
	{
		return m_queues[priority].Count();
	}

	static JobPriority_t GetMinPriority()
	{
		return (JobPriority_t)m_MinPriority;
	}

	static void SetMinPriority( JobPriority_t priority )
	{
		m_MinPriority = priority;
	}

	CJob *PrePush()
	{
		if ( m_nItems >= m_nMaxItems )
		{
			CJob *pOverflowJob;
			if ( Pop( &pOverflowJob ) )
			{
				return pOverflowJob;
			}
		}
		return NULL;
	}

	int Push( CJob *pJob, int iThread = -1 )
	{
		pJob->AddRef();

		CJob *pOverflowJob;
		int nOverflow = 0;
		while ( ( pOverflowJob = PrePush() ) != NULL )
		{
			ServiceJobAndRelease( pJob );
			nOverflow++;
		}

		m_queues[pJob->GetPriority()].PushItem( pJob );

		m_mutex.Lock();
		if ( ++m_nItems == 1 )
		{
			m_JobAvailableEvent.Set();
		}
		m_mutex.Unlock();

		return nOverflow;
	}

	bool Pop( CJob **ppJob )
	{
		m_mutex.Lock();
		if ( !m_nItems )
		{
			m_mutex.Unlock();
			*ppJob = NULL;
			return false;
		}
		if ( --m_nItems == 0 )
		{
			m_JobAvailableEvent.Reset();
		}
		m_mutex.Unlock();

		for ( int i = JP_NUM_PRIORITIES - 1; i >= m_MinPriority; --i )
		{
			if ( m_queues[i].PopItem( ppJob ) )
			{
				return true;
			}
		}

		m_mutex.Lock();
		AssertMsg( m_MinPriority != JP_LOW, "Expected at least one queue item" );

		if ( ++m_nItems == 1 )
		{
			m_JobAvailableEvent.Set();
			ThreadSleep();
		}
		m_mutex.Unlock();

		*ppJob = NULL;
		return false;
	}

	CThreadEvent &GetEventHandle()
	{
		return m_JobAvailableEvent;
	}

	void Flush()
	{
		// Only safe to call when system is suspended
		m_mutex.Lock();
		m_nItems = 0;
		m_JobAvailableEvent.Reset();
		CJob *pJob = NULL;
		for ( int i = JP_NUM_PRIORITIES - 1; i >= 0; --i )
		{
			while ( m_queues[i].PopItem( &pJob ) )
			{
				pJob->Abort();
				pJob->Release();
			}
		}
		m_mutex.Unlock();
	}

private:
	CTSQueue<CJob *>	m_queues[JP_NUM_PRIORITIES];
	int					m_nItems;
	int					m_nMaxItems;
	CThreadMutex	m_mutex;
	CThreadManualEvent	m_JobAvailableEvent;
	static int			m_MinPriority;
} ALIGN16_POST;

int	CJobQueue::m_MinPriority = JP_LOW;

#pragma pack(pop)

class CJobThread;

//-----------------------------------------------------------------------------
//
// CThreadPool
//
//-----------------------------------------------------------------------------

class CThreadPool : public CRefCounted1<IThreadPool, CRefCountServiceMT>
{
public:
	CThreadPool();
	~CThreadPool();

	//-----------------------------------------------------
	// Thread functions
	//-----------------------------------------------------
	bool Start( const ThreadPoolStartParams_t &startParams = ThreadPoolStartParams_t() ) { return Start( startParams, NULL ); }
	bool Start( const ThreadPoolStartParams_t &startParams, const char *pszNameOverride );
	bool Stop( int timeout = TT_INFINITE );
	void Distribute( bool bDistribute = true, int *pAffinityTable = NULL );

	//-----------------------------------------------------
	// Functions for any thread
	//-----------------------------------------------------
	unsigned GetJobCount()							{ return m_nJobs; }
	int NumThreads();
	int NumIdleThreads();

	//-----------------------------------------------------
	// Pause/resume processing jobs
	//-----------------------------------------------------
	int SuspendExecution();
	int ResumeExecution();

	//-----------------------------------------------------
	// Offer the current thread to the pool
	//-----------------------------------------------------
	virtual int YieldWait( CThreadEvent **pEvents, int nEvents, bool bWaitAll = true, unsigned timeout = TT_INFINITE );
	virtual int YieldWait( CJob **, int nJobs, bool bWaitAll = true, unsigned timeout = TT_INFINITE );
	void Yield( unsigned timeout );
	virtual int YieldWaitPerFrameJobs( );

	//-----------------------------------------------------
	// Add a native job to the queue (master thread)
	//-----------------------------------------------------
	void AddJob( CJob * );
	virtual void AddPerFrameJob( CJob * );
	void InsertJobInQueue( CJob * );

	//-----------------------------------------------------
	// Add an function object to the queue (master thread)
	//-----------------------------------------------------
	void AddFunctorInternal( CFunctor *, CJob ** = NULL, const char *pszDescription = NULL, unsigned flags = 0 );

	//-----------------------------------------------------
	// Remove a job from the queue (master thread)
	//-----------------------------------------------------
	virtual void ChangePriority( CJob *p, JobPriority_t priority );

	//-----------------------------------------------------
	// Bulk job manipulation (blocking)
	//-----------------------------------------------------
	int ExecuteToPriority( JobPriority_t toPriority, JobFilter_t pfnFilter = NULL  );
	int AbortAll();

private:
	enum
	{
		IO_STACKSIZE = ( 64 * 1024 ),
		COMPUTATION_STACKSIZE = 0,
	};

	//-----------------------------------------------------
	//
	//-----------------------------------------------------
	CJob *PeekJob();
	CJob *GetDummyJob();

	//-----------------------------------------------------
	// Thread functions
	//-----------------------------------------------------
	int Run();

private:
	friend class CJobThread;

	CJobQueue				m_SharedQueue;
	CInterlockedInt			m_nIdleThreads;
	CUtlVector<CJobThread *> m_Threads;

	CThreadMutex		m_SuspendMutex;
	int						m_nSuspend;
	CInterlockedInt			m_nJobs;

	// Some jobs should only be executed on the threadpool thread(s). Ie: the rendering thread has the GL context
	//	and the main thread coming in and "helping" with jobs breaks that pretty nicely. This flag states that
	//	only the threadpool threads should execute these jobs.
	bool					m_bExecOnThreadPoolThreadsOnly;

	CThreadMutex		m_PerFrameJobListMutex;
	CUtlVectorFixedGrowable< CJob *, 2048 >	m_PerFrameJobs;
};

//-----------------------------------------------------------------------------

JOB_INTERFACE IThreadPool *CreateNewThreadPool()
{
	return new CThreadPool;
}

JOB_INTERFACE void DestroyThreadPool( IThreadPool *pPool )
{
	delete static_cast<CThreadPool*>( pPool );
}

//-----------------------------------------------------------------------------

class CGlobalThreadPool : public CThreadPool
{
public:
	virtual bool Start( const ThreadPoolStartParams_t &startParamsIn )
	{
		int nThreads = ( CommandLine()->ParmValue( "-threads", -1 ) - 1 );
		ThreadPoolStartParams_t startParams = startParamsIn;

		if ( nThreads >= 0 )
		{
			startParams.nThreads = nThreads;
		}
		else
		{
			// Cap the GlobPool threads at 4.
			startParams.nThreadsMax = 4;
		}
		return CThreadPool::Start( startParams, "GlobPool" );
	}

	virtual bool OnFinalRelease()
	{
		AssertMsg( 0, "Releasing global thread pool object!" );
		return false;
	}
};

//-----------------------------------------------------------------------------

class CJobThread : public CWorkerThread
{
public:
	CJobThread( CThreadPool *pOwner, int iThread ) : 
		m_SharedQueue( pOwner->m_SharedQueue ),
		m_pOwner( pOwner ),
		m_iThread( iThread )
	{
	}

	CThreadEvent &GetIdleEvent()
	{
		return m_IdleEvent;
	}

	CJobQueue &AccessDirectQueue()
	{ 
		return m_DirectQueue;
	}

private:
	unsigned Wait()
	{
		unsigned waitResult;
#ifdef WIN32
		enum Event_t
		{
			CALL_FROM_MASTER,
			SHARED_QUEUE,
			DIRECT_QUEUE,
			
			NUM_EVENTS
		};

		CThreadEvent *waitHandles[NUM_EVENTS];
		
		waitHandles[CALL_FROM_MASTER]	= &GetCallHandle();
		waitHandles[SHARED_QUEUE]		= &m_SharedQueue.GetEventHandle();
		waitHandles[DIRECT_QUEUE] 		= &m_DirectQueue.GetEventHandle();
		
#ifdef _DEBUG
		while ( ( waitResult = CThreadEvent::WaitForMultiple( ARRAYSIZE(waitHandles), waitHandles , FALSE, 10 ) ) == TW_TIMEOUT )
		{
			waitResult = waitResult; // break here
		}
#else
		waitResult = CThreadEvent::WaitForMultiple( ARRAYSIZE(waitHandles), waitHandles , FALSE, TT_INFINITE );
#endif
#else // !win32
		bool bSet = false;
		int nWaitTime = 100;
		
		while ( !bSet )
		{
			// jobs are typically enqueued to the shared job queue so wait on it
			bSet = m_SharedQueue.GetEventHandle().Wait( nWaitTime );
			if ( !bSet )
				bSet = m_DirectQueue.GetEventHandle().Wait( 0 );
			if ( !bSet )
				bSet = GetCallHandle().Wait( 0 );
		}
		
		if ( !bSet )
			waitResult = WAIT_TIMEOUT;
		else
			waitResult = WAIT_OBJECT_0;		
#endif
		return waitResult;
	}

	int Run()
	{
		// Wait for either a call from the master thread, or an item in the queue...
		unsigned waitResult;
		bool	 bExit = false;

		m_pOwner->m_nIdleThreads++;
		m_IdleEvent.Set();
		while ( !bExit && ( waitResult = Wait() ) != TW_FAILED )
		{
			if ( PeekCall() )
			{
				switch ( GetCallParam() )
				{
				case TPM_EXIT:
					Reply( true );
					bExit = TRUE;
					break;

				case TPM_SUSPEND:
					Reply( true );
					Suspend();
					break;

				default:
					AssertMsg( 0, "Unknown call to thread" );
					Reply( false );
					break;
				}
			}
			else
			{
				CJob *pJob;
				bool bTookJob = false;
				do
				{
					if ( !m_DirectQueue.Pop( &pJob) )
					{
						if ( !m_SharedQueue.Pop( &pJob ) )
						{
							// Nothing to process, return to wait state
							break;
						}
					}
					if ( !bTookJob )
					{
						m_IdleEvent.Reset();
						m_pOwner->m_nIdleThreads--;
						bTookJob = true;
					}
					ServiceJobAndRelease( pJob, m_iThread );
					m_pOwner->m_nJobs--;
				} while ( !PeekCall() );

				if ( bTookJob )
				{
					m_pOwner->m_nIdleThreads++;
					m_IdleEvent.Set();
				}
			}
		}
		m_pOwner->m_nIdleThreads--;
		m_IdleEvent.Reset();
		return 0;
	}

	CJobQueue			m_DirectQueue;
	CJobQueue &			m_SharedQueue;
	CThreadPool *		m_pOwner;
	CThreadManualEvent	m_IdleEvent;
	int					m_iThread;
};

//-----------------------------------------------------------------------------

CGlobalThreadPool g_ThreadPool;
IThreadPool *g_pThreadPool = &g_ThreadPool;
IThreadPool *g_pAlternateThreadPool;

//-----------------------------------------------------------------------------
//
// CThreadPool
//
//-----------------------------------------------------------------------------

CThreadPool::CThreadPool() :
	m_nIdleThreads( 0 ),
	m_nJobs( 0 ),
	m_nSuspend( 0 )
{
}

//---------------------------------------------------------

CThreadPool::~CThreadPool()
{
}

//---------------------------------------------------------
// 
//---------------------------------------------------------
int CThreadPool::NumThreads()
{
	return m_Threads.Count();
}

//---------------------------------------------------------
// 
//---------------------------------------------------------
int CThreadPool::NumIdleThreads()
{
	return m_nIdleThreads;
}

//---------------------------------------------------------
// Pause/resume processing jobs
//---------------------------------------------------------
int CThreadPool::SuspendExecution()
{
	AUTO_LOCK( m_SuspendMutex );

	// If not already suspended
	if ( m_nSuspend == 0 )
	{
		int i;
		for ( i = 0; i < m_Threads.Count(); i++ )
		{
			m_Threads[i]->CallWorker( TPM_SUSPEND, 0 );
		}

		for ( i = 0; i < m_Threads.Count(); i++ )
		{
			m_Threads[i]->WaitForReply();
		}

		// Because worker must signal before suspending, we could reach
		// here with the thread not actually suspended
		for ( i = 0; i < m_Threads.Count(); i++ )
		{
			while ( !m_Threads[i]->IsSuspended() )
			{
				ThreadSleep();
			}   	
		}
	}

	return m_nSuspend++;
}

//---------------------------------------------------------

int CThreadPool::ResumeExecution()
{
	AUTO_LOCK( m_SuspendMutex );
	AssertMsg( m_nSuspend >= 1, "Attempted resume when not suspended");
	int result = m_nSuspend--;
	if (m_nSuspend == 0 )
	{
		for ( int i = 0; i < m_Threads.Count(); i++ )
		{
			m_Threads[i]->Resume();
		}
	}
	return result;
}

//---------------------------------------------------------

int CThreadPool::YieldWait( CThreadEvent **pEvents, int nEvents, bool bWaitAll, unsigned timeout )
{
	Assert( timeout == TT_INFINITE ); // unimplemented
	int result;
	CJob *pJob;
	// Always wait for zero milliseconds initially, to let us process jobs on this thread.
	timeout = 0;
	while ( ( result = CThreadEvent::WaitForMultiple( nEvents, pEvents, bWaitAll, timeout ) ) == TW_TIMEOUT )
	{
		if (!m_bExecOnThreadPoolThreadsOnly && m_SharedQueue.Pop( &pJob ) )
		{
			ServiceJobAndRelease( pJob );
			m_nJobs--;
		}
		else
		{
			// Since there are no jobs for the main thread set the timeout to infinite.
			// The only disadvantage to this is that if a job thread creates a new job
			// then the main thread will not be available to pick it up, but if that
			// is a problem you can just create more worker threads. Debugging test runs
			// of TF2 suggests that jobs are only ever added from the main thread which
			// means that there is no disadvantage.
			// Waiting on the events instead of busy spinning has multiple advantages.
			// It avoids wasting CPU time/electricity, it makes it more obvious in profiles
			// when the main thread is idle versus busy, and it allows ready thread analysis
			// in xperf to find out what woke up a waiting thread.
			// It also avoids unnecessary CPU starvation -- seen on customer traces of TF2.
			timeout = TT_INFINITE;
		}
	}
	return result;
}

//---------------------------------------------------------

int CThreadPool::YieldWait( CJob **ppJobs, int nJobs, bool bWaitAll, unsigned timeout )
{
	CUtlVectorFixed<CThreadEvent*, 64> handles;
	if ( nJobs > handles.NumAllocated() - 2 )
	{
		return TW_FAILED;
	}

	for ( int i = 0; i < nJobs; i++ )
	{
		handles.AddToTail( ppJobs[i]->AccessEvent() );
	}

	return YieldWait( (CThreadEvent **)handles.Base(), handles.Count(), bWaitAll, timeout);
}

//---------------------------------------------------------

void CThreadPool::Yield( unsigned timeout )
{
	// @MULTICORE (toml 10/24/2006): not implemented
	Assert( ThreadInMainThread() );
	if ( !ThreadInMainThread() )
	{
		ThreadSleep( timeout );
		return;
	}
	ThreadSleep( timeout );
}


//---------------------------------------------------------
// Block until all per-frame jobs are done
//---------------------------------------------------------
int CThreadPool::YieldWaitPerFrameJobs( )
{
	// NOTE: Per-Frame jobs may spawn other per-frame jobs.
	// Therefore we must copy the list off and block on that list
	// because more jobs may be added while we're yielding
	int nRetVal = 0;
	while ( true )
	{
		m_PerFrameJobListMutex.Lock();
		int nCount = m_PerFrameJobs.Count();
		if ( nCount == 0 )
		{
			m_PerFrameJobListMutex.Unlock();
			break;
		}

		size_t nSize = nCount * sizeof( CJob* );
		CJob **ppJobs = ( CJob** )stackalloc( nSize );
		memcpy( ppJobs, m_PerFrameJobs.Base(), nSize );
		m_PerFrameJobs.RemoveAll();
		m_PerFrameJobListMutex.Unlock();

		nRetVal = YieldWait( ppJobs, nCount );

		for ( int i = 0; i < nCount; ++i )
		{
			ppJobs[i]->Release();
		}
	}
	return nRetVal;
}


//---------------------------------------------------------
// Add a job to the queue
//---------------------------------------------------------
void CThreadPool::AddJob( CJob *pJob )
{
	if ( !pJob )
	{
		return;
	}

	if ( pJob->m_ThreadPoolData != JOB_NO_DATA )
	{
		Warning( "Cannot add a thread job already committed to another thread pool\n" );
		return;
	}

	if ( m_Threads.Count() == 0 )
	{
		// So only threadpool jobs are supposed to execute the jobs, but there are no threadpool threads?
		Assert( !m_bExecOnThreadPoolThreadsOnly );

		pJob->Execute();
		return;
	}

	int flags = pJob->GetFlags();

	if ( !m_bExecOnThreadPoolThreadsOnly && ( ( flags & ( JF_IO | JF_QUEUE ) )  == 0 ) /* @TBD && !m_queue.Count() */ )
	{
		if ( !NumIdleThreads() )
		{
			pJob->Execute();
			return;
		}
		pJob->SetPriority( (JobPriority_t)(JP_NUM_PRIORITIES - 1) );
	}

	if ( !pJob->CanExecute() )
	{
		// Already handled
		ExecuteOnce( Warning( "Attempted to add job to job queue that has already been completed\n" ) );
		return;
	}

	pJob->m_pThreadPool = this;
	pJob->m_status = JOB_STATUS_PENDING;
#if defined( THREAD_PARENT_STACK_TRACE_ENABLED )
	{
		int iValidEntries = GetCallStack_Fast( pJob->m_ParentStackTrace, ARRAYSIZE( pJob->m_ParentStackTrace ), 0 );
		for( int i = iValidEntries; i < ARRAYSIZE( pJob->m_ParentStackTrace ); ++i )
		{
			pJob->m_ParentStackTrace[i] = NULL;
		}
	}
#endif
	InsertJobInQueue( pJob );
	++m_nJobs;
}


//-----------------------------------------------------
// Add a native job to the queue (master thread)
// Call YieldWaitPerFrameJobs() to wait only until all per-frame jobs are done
//-----------------------------------------------------
void CThreadPool::AddPerFrameJob( CJob *pJob )
{
	m_PerFrameJobListMutex.Lock();
	pJob->AddRef();
	m_PerFrameJobs.AddToTail( pJob );
	m_PerFrameJobListMutex.Unlock();

	AddJob( pJob );
}


//---------------------------------------------------------
//
//---------------------------------------------------------
void CThreadPool::InsertJobInQueue( CJob *pJob )
{
	CJobQueue *pQueue;

	if ( !( pJob->GetFlags() & JF_SERIAL ) )
	{
		int iThread = pJob->GetServiceThread();
		if ( iThread == -1 || !m_Threads.IsValidIndex( iThread ) )
		{
			pQueue = &m_SharedQueue;
		}
		else
		{
			pQueue = &(m_Threads[iThread]->AccessDirectQueue());
		}
	}
	else
	{
		pQueue = &(m_Threads[0]->AccessDirectQueue());
	}


#ifdef SN_TARGET_PS3
	// GSidhu
	// Make sure render job goes on shared q rather than direct q.
	// Direct q jobs get picked up only after jobs appear on shared q or
	// wait times out on shared q.
	// This is a fix for Eurogamer, look at this in more detail later

	pQueue = &m_SharedQueue;
#endif 

	m_nJobs -= pQueue->Push( pJob );
}

//---------------------------------------------------------
// Add an function object to the queue (master thread)
//---------------------------------------------------------

void CThreadPool::AddFunctorInternal( CFunctor *pFunctor, CJob **ppJob, const char *pszDescription, unsigned flags )
{
	// Note: assumes caller has handled refcount
	CJob *pJob = new CFunctorJob( pFunctor, pszDescription );

	pJob->SetFlags( flags );

	AddJob( pJob );

	if ( ppJob )
	{
		*ppJob = pJob;
	}
	else
	{
		pJob->Release();
	}
}

//---------------------------------------------------------
// Remove a job from the queue
//---------------------------------------------------------

void CThreadPool::ChangePriority( CJob *pJob, JobPriority_t priority )
{
	// Right now, only support upping the priority
	if ( pJob->GetPriority() < priority )
	{
		pJob->SetPriority( priority );
		m_SharedQueue.Push( pJob );
	}
	else
	{
		ExecuteOnce( if ( pJob->GetPriority() != priority ) DevMsg( "CThreadPool::RemoveJob not implemented right now" ) );
	}

}

//---------------------------------------------------------
// Execute to a specified priority
//---------------------------------------------------------

#define THREADED_EXECUTETOPRIORITY 0 //  Not ready for general consumption [8/4/2010 tom]

int CThreadPool::ExecuteToPriority( JobPriority_t iToPriority, JobFilter_t pfnFilter )
{
	if ( !THREADED_EXECUTETOPRIORITY || pfnFilter )
	{
		SuspendExecution();

		CJob *pJob;
		int i;
		int nExecuted = 0;
		int nJobsTotal = GetJobCount();
		CUtlVector<CJob *> jobsToPutBack;

		for ( int iCurPriority = JP_NUM_PRIORITIES - 1; iCurPriority >= iToPriority; --iCurPriority )
		{
			for ( i = 0; i < m_Threads.Count(); i++ )
			{
				CJobQueue &queue = m_Threads[i]->AccessDirectQueue();
				while ( queue.Count( (JobPriority_t)iCurPriority ) )
				{
					queue.Pop( &pJob );
					if ( pfnFilter && !(*pfnFilter)( pJob ) )
					{
						if ( pJob->CanExecute() )
						{
							jobsToPutBack.EnsureCapacity( nJobsTotal );
							jobsToPutBack.AddToTail( pJob );
						}
						else
						{
							m_nJobs--;
							pJob->Release(); // an already serviced job in queue, may as well ditch it (as in, main thread probably force executed)
						}
						continue;
					}
					ServiceJobAndRelease( pJob );
					m_nJobs--;
					nExecuted++;
				}

			}

			while ( m_SharedQueue.Count( (JobPriority_t)iCurPriority ) )
			{
				m_SharedQueue.Pop( &pJob );
				if ( pfnFilter && !(*pfnFilter)( pJob ) )
				{
					if ( pJob->CanExecute() )
					{
						jobsToPutBack.EnsureCapacity( nJobsTotal );
						jobsToPutBack.AddToTail( pJob );
					}
					else
					{
						m_nJobs--;
						pJob->Release(); // see above
					}
					continue;
				}

				ServiceJobAndRelease( pJob );
				m_nJobs--;
				nExecuted++;
			}
		}

		for ( i = 0; i < jobsToPutBack.Count(); i++ )
		{
			InsertJobInQueue( jobsToPutBack[i] );
			jobsToPutBack[i]->Release();
		}

		ResumeExecution();

		return nExecuted;
	}
	else
	{
		JobPriority_t prevPriority = CJobQueue::GetMinPriority();

		CJobQueue::SetMinPriority( iToPriority );

		CUtlVectorFixedGrowable<CThreadEvent*, 64> handles;

		for ( int i = 0; i < m_Threads.Count(); i++ )
		{
			handles.AddToTail( &m_Threads[i]->GetIdleEvent() );
		}

		CJob *pJob = NULL;
		do
		{
			YieldWait( (CThreadEvent **)handles.Base(), handles.Count(), true, TT_INFINITE );
			if ( m_SharedQueue.Pop( &pJob ) )
			{
				ServiceJobAndRelease( pJob );
				m_nJobs--;
			}
		} while ( pJob );

		CJobQueue::SetMinPriority( prevPriority );

		return 1;
	}
}

//---------------------------------------------------------
//
//---------------------------------------------------------

int CThreadPool::AbortAll()
{
	SuspendExecution();
	CJob *pJob;

	int iAborted = 0;
	while ( m_SharedQueue.Pop( &pJob ) )
	{
		pJob->Abort();
		pJob->Release();
		iAborted++;
	}

	for ( int i = 0; i < m_Threads.Count(); i++ )
	{
		CJobQueue &queue = m_Threads[i]->AccessDirectQueue();
		while ( queue.Pop( &pJob ) )
		{
			pJob->Abort();
			pJob->Release();
			iAborted++;
		}

	}

	m_nJobs = 0;

	ResumeExecution();

	return iAborted;
}

//---------------------------------------------------------
// CThreadPool thread functions
//---------------------------------------------------------

bool CThreadPool::Start( const ThreadPoolStartParams_t &startParams, const char *pszName )
{
#if defined( DEDICATED ) && IsPlatformLinux()
	if ( !startParams.bEnableOnLinuxDedicatedServer )
		return false;
#endif

	int nThreads = startParams.nThreads;

	m_bExecOnThreadPoolThreadsOnly = startParams.bExecOnThreadPoolThreadsOnly;


	if ( nThreads < 0 )
	{
		const CPUInformation &ci = GetCPUInformation();
		if ( startParams.bIOThreads )
		{
			nThreads = ci.m_nLogicalProcessors;
		}
		else
		{
			// One worker thread per logical processor minus main thread and graphic driver
			nThreads = ci.m_nLogicalProcessors  - 2;
			if ( IsPC() )
			{
				if ( nThreads > 3 )
				{
					DevMsg( "Defaulting to limit of 3 worker threads, use -threads on command line if want more\n" ); // Current >4 processor configs don't really work so well, probably due to cache issues? (toml 7/12/2007)
					nThreads = 3;
				}
			}
		}

		if ( ( startParams.nThreadsMax >= 0 ) && ( nThreads > startParams.nThreadsMax ) )
		{
			nThreads = startParams.nThreadsMax;
		}
	}

	if ( nThreads <= 0 )
	{
		return true;
	}

	int nStackSize = startParams.nStackSize;

	if ( nStackSize < 0 )
	{
		if ( startParams.bIOThreads )
		{
			nStackSize = IO_STACKSIZE;
		}
		else
		{
			nStackSize = COMPUTATION_STACKSIZE;
		}
	}

	int priority = startParams.iThreadPriority;

	if ( priority == SHRT_MIN )
	{
		if ( startParams.bIOThreads )
		{
#if defined( _WIN32 )
			priority = THREAD_PRIORITY_HIGHEST;
#endif
		}
		else
		{
			priority = ThreadGetPriority();
		}
	}

	if ( IsPS3() && priority < ThreadGetPriority() )
	{
		// On PS3 all thread pools should be the same priority as creator or less demanding
		priority = ThreadGetPriority();
	}

	bool bDistribute;
	if ( startParams.fDistribute != TRS_NONE )
	{
		bDistribute = ( startParams.fDistribute == TRS_TRUE );
	}
	else
	{
		bDistribute = !startParams.bIOThreads;
	}

	//--------------------------------------------------------

	m_Threads.EnsureCapacity( nThreads );
	
	if ( !pszName )
	{
		pszName = ( startParams.bIOThreads ) ? "IOJob" : "CmpJob";
	}
	while ( nThreads-- )
	{
		int iThread = m_Threads.AddToTail();
		m_Threads[iThread] = new CJobThread( this, iThread );
		CFmtStr formattedName( "%s%d", pszName, iThread );
		m_Threads[iThread]->SetName( formattedName );
		m_Threads[iThread]->Start( nStackSize );
		m_Threads[iThread]->GetIdleEvent().Wait();
		ThreadSetDebugName( m_Threads[iThread]->GetThreadHandle(), formattedName );
		ThreadSetPriority( (ThreadHandle_t)m_Threads[iThread]->GetThreadHandle(), priority );
	}

	Distribute( bDistribute, startParams.bUseAffinityTable ? (int *)startParams.iAffinityTable : NULL );

	return true;
}

//---------------------------------------------------------

void CThreadPool::Distribute( bool bDistribute, int *pAffinityTable )
{
	if ( bDistribute )
	{
		const CPUInformation &ci = GetCPUInformation();
		int nHwThreadsPer = (( ci.m_bHT ) ? 2 : 1);
		if ( ci.m_nLogicalProcessors > 1 )
		{
			if ( !pAffinityTable )
			{
#if defined( IS_WINDOWS_PC )
				// no affinity table, distribution is cycled across all available
				HINSTANCE hInst = LoadLibrary( "kernel32.dll" );
				if ( hInst )
				{
					typedef DWORD (WINAPI *SetThreadIdealProcessorFn)(ThreadHandle_t hThread, DWORD dwIdealProcessor);
					SetThreadIdealProcessorFn Thread_SetIdealProcessor = (SetThreadIdealProcessorFn)GetProcAddress( hInst, "SetThreadIdealProcessor" );
					if ( Thread_SetIdealProcessor )
					{
						ThreadHandle_t hMainThread = ThreadGetCurrentHandle();
						Thread_SetIdealProcessor( hMainThread, 0 );
						int iProc = 0;
						for ( int i = 0; i < m_Threads.Count(); i++ )
						{
							iProc += nHwThreadsPer;
							if ( iProc >= ci.m_nLogicalProcessors )
							{
								iProc %= ci.m_nLogicalProcessors;
								if ( nHwThreadsPer > 1 )
								{
									iProc = ( iProc + 1 ) % nHwThreadsPer;
								}
							}
							Thread_SetIdealProcessor((ThreadHandle_t)m_Threads[i]->GetThreadHandle(), iProc);
						}
					}
					FreeLibrary( hInst );
				}
#else
				// no affinity table, distribution is cycled across all available
				int iProc = 0;
				for ( int i = 0; i < m_Threads.Count(); i++ )
				{
					iProc += nHwThreadsPer;
					if ( iProc >= ci.m_nLogicalProcessors )
					{
						iProc %= ci.m_nLogicalProcessors;
						if ( nHwThreadsPer > 1 )
						{
							iProc = ( iProc + 1 ) % nHwThreadsPer;
						}
					}
					ThreadSetAffinity( (ThreadHandle_t)m_Threads[i]->GetThreadHandle(), 1 << iProc );
				}
#endif
			}
			else
			{
				// distribution is from affinity table
				for ( int i = 0; i < m_Threads.Count(); i++ )
				{
					ThreadSetAffinity( (ThreadHandle_t)m_Threads[i]->GetThreadHandle(), pAffinityTable[i] );
				}
			}
		}
	}
	else
	{
#if defined( _WIN32 )
		DWORD_PTR dwProcessAffinity, dwSystemAffinity;
		if ( GetProcessAffinityMask( GetCurrentProcess(), &dwProcessAffinity, &dwSystemAffinity ) )
		{
			for ( int i = 0; i < m_Threads.Count(); i++ )
			{
				ThreadSetAffinity( (ThreadHandle_t)m_Threads[i]->GetThreadHandle(), dwProcessAffinity );
			}
		}
#endif
	}
}

//---------------------------------------------------------

bool CThreadPool::Stop( int timeout )
{
#ifdef _PS3
	if ( GetTLSGlobals()->bNormalQuitRequested )
	{
		// When PS3 system requests a quit we
		// might leave some of our threads suspended
		// we need to resume them so that they could
		// receive TPM_EXIT command
		AUTO_LOCK( m_SuspendMutex );
		if ( m_nSuspend >= 1 )
		{
			m_nSuspend = 1;
			ResumeExecution();
		}
	}
#endif

	CUtlVector< ThreadHandle_t > arrHandles;
	arrHandles.SetCount( m_Threads.Count() );
	for ( int i = 0; i < m_Threads.Count(); i++ )
	{
		arrHandles[i] = m_Threads[i]->GetThreadHandle();
		#ifdef _WIN32
		if( arrHandles[i] )
		{
			// due to weird legacy reasons, the worker thread keeps its handle ownership.
			// it closes the handle BEFORE exiting, which renders that handle useless to join on Win32
			// this leads to (a) invalid handle before the thread exits (b) potentially unmapping the code running worker thread before the thread exits
			// The worker thread even has a hacky workaround: it sets an event before exiting! Obviously, that event is useless to fix this race condition
			
			// The right solution would be to have the worker NOT close its own handle, rendering that handle useful. And make ThreadJoin close that handle,
			// mimicking pthreads semantics (joinable thread enters zombie state until it's properly joined). But it leads to another problem,
			// namely handle leak in cases when we potentially stop the workers in other systems, and potentially incorrect joins on closed handles
			// (because in Win32, it's valid to wait for thread handle twice, but it's not valid in pthreads to join the same thread twice).
			// So for now, I'm just fixing it with local fix here.
			// 
			HANDLE hDup;
			DuplicateHandle( GetCurrentProcess(), arrHandles[i], GetCurrentProcess(), &hDup, DUPLICATE_SAME_ACCESS, FALSE, 0 );
			arrHandles[i] = (ThreadHandle_t)hDup;
		}
		#endif
		
		m_Threads[i]->CallWorker( TPM_EXIT );
	}

	for ( int i = 0; i < m_Threads.Count(); ++i )
	{
		if ( arrHandles[i] )
		{
			ThreadJoin( arrHandles[i] );

#ifdef WIN32
			Assert( !m_Threads[i]->GetThreadHandle() );
			// because we duplicated the handle above, due to the historical reasons described above, we have to close this handle on Win32
			CloseHandle( arrHandles[i] );
#else
			Assert( !m_Threads[i]->IsAlive() );
#endif
		}

#ifdef WIN32
		while( m_Threads[i]->GetThreadHandle() )
#else
		while( m_Threads[i]->IsAlive() )
#endif
		{
			ThreadSleep( 0 );
		}
		delete m_Threads[i];
	}

	m_nJobs = 0;
	m_SharedQueue.Flush();
	m_nIdleThreads = 0;
	m_Threads.RemoveAll();

	return true;
}

//---------------------------------------------------------

CJob *CThreadPool::GetDummyJob()
{
	class CDummyJob : public CJob
	{
	public:
		CDummyJob()
		{
			Execute();
		}

		virtual JobStatus_t DoExecute() { return JOB_OK; }
	};

	static CDummyJob dummyJob;

	dummyJob.AddRef();
	return &dummyJob;
}


namespace ThreadPoolTest 
{
int g_iSleep;

CThreadEvent g_done;
int g_nTotalToComplete;
CThreadPool *g_pTestThreadPool;

class CCountJob : public CJob
{
public:
	virtual JobStatus_t DoExecute()
	{
		m_nCount++;
		ThreadPause();
		if ( g_iSleep >= 0)
			ThreadSleep( g_iSleep );
		if ( bDoWork )
		{
			byte pMemory[1024];
			int i;
			for ( i = 0; i < 1024; i++ )
			{
				pMemory[i] = rand();
			}
			for ( i = 0; i < 50; i++ )
			{
				sqrt( (float)HashBlock( pMemory, 1024 ) + HashBlock( pMemory, 1024 ) + 10.0 );
			}
			bDoWork = false;
		}
		if ( m_nCount == g_nTotalToComplete )
			g_done.Set();
		return 0;
	}

	static CInterlockedInt m_nCount;
	bool bDoWork;
};
CInterlockedInt CCountJob::m_nCount;
int g_nTotalAtFinish;

void Test( bool bDistribute, bool bSleep = true, bool bFinishExecute = false, bool bDoWork = false, bool bIncludeMain = false, bool bPrioritized = false )
{
	int nJobCount = 4000;
	CCountJob *jobs = new CCountJob[4000];
	for ( int bInterleavePushPop = 0; bInterleavePushPop < 2; bInterleavePushPop++ )
	{
		for ( g_iSleep = -10; g_iSleep <= 10; g_iSleep += 10 )
		{
			Msg( "ThreadPoolTest:         Testing! Sleep %d, interleave %d, prioritized %d \n", g_iSleep, bInterleavePushPop, bPrioritized );
			int nMaxThreads = ( IsX360() ) ? 6 : 8;
			int nIncrement = ( IsX360() ) ? 1 : 2;
			for ( int i = 1; i <= nMaxThreads; i += nIncrement )
			{
				CCountJob::m_nCount = 0;
				g_nTotalAtFinish = 0;
				ThreadPoolStartParams_t params;
				params.nThreads = i;
				params.fDistribute = ( bDistribute) ? TRS_TRUE : TRS_FALSE;
				g_pTestThreadPool->Start( params, "Tst" );
				if ( !bInterleavePushPop )
				{
					g_pTestThreadPool->SuspendExecution();
				}

				g_nTotalToComplete = nJobCount;

				CFastTimer timer, suspendTimer;

				suspendTimer.Start();
				timer.Start();
				for ( int j = 0; j < nJobCount; j++ )
				{
					jobs[j].SetFlags( JF_QUEUE );
					jobs[j].bDoWork = bDoWork;
					if ( bPrioritized )
					{
						jobs[j].SetPriority( (JobPriority_t)RandomInt( JP_LOW, JP_IMMEDIATE ) );
					}
					g_pTestThreadPool->AddJob( &jobs[j] );
					if ( bSleep && j % 16 == 0 )
					{
						ThreadSleep( 0 );
					}
				}
				if ( !bInterleavePushPop )
				{
					g_pTestThreadPool->ResumeExecution();
				}
				if ( bFinishExecute && g_iSleep <= 1 )
				{
					if ( bDoWork && bIncludeMain )
					{
						while ( g_pTestThreadPool->GetJobCount() && g_pTestThreadPool->NumIdleThreads() == g_pTestThreadPool->NumIdleThreads() )
						{

						}
						CThreadEvent *pEvent = &g_done;
						g_pTestThreadPool->YieldWait( &pEvent, 1 );
					}
					else
					{
						g_done.Wait();
					}
				}
				g_nTotalAtFinish = CCountJob::m_nCount;
				timer.End();
				g_pTestThreadPool->SuspendExecution();
				suspendTimer.End();
				g_pTestThreadPool->ResumeExecution();
				g_pTestThreadPool->Stop();
				g_done.Reset();

				int counts[9] = { 0 };
				for ( int j = 0; j < nJobCount; j++ )
				{
					if ( jobs[j].GetServiceThread() != -1 )
					{
						counts[jobs[j].GetServiceThread()+1]++;
						jobs[j].ClearServiceThread();
					}
					else if ( jobs[j].GetStatus() == JOB_OK )
					{
						counts[0]++;
					}
				}

				Msg( "ThreadPoolTest:         %d threads -- %d (%d) jobs processed in %fms, %fms to suspend (%f/%f) [ (main) %d, %d, %d, %d, %d, %d, %d, %d, %d]\n", 
					i, (int)g_nTotalAtFinish, (int)CCountJob::m_nCount, timer.GetDuration().GetMillisecondsF(), suspendTimer.GetDuration().GetMillisecondsF() - timer.GetDuration().GetMillisecondsF(),
					timer.GetDuration().GetMillisecondsF() / (float)CCountJob::m_nCount, (suspendTimer.GetDuration().GetMillisecondsF())/(float)g_nTotalAtFinish,
					counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], counts[7], counts[8] );
			}
		}
	}
	delete[]jobs;
}


bool g_bOutputError;
volatile int g_ReadyToExecute;
CInterlockedInt g_nReady;

class CExecuteTestJob : public CJob
{
public:
	virtual JobStatus_t DoExecute()
	{
		byte pMemory[1024];
		int i;
		for ( i = 0; i < 1024; i++ )
		{
			pMemory[i] = rand();
		}
		for ( i = 0; i < 50; i++ )
		{
			sqrt( (float)HashBlock( pMemory, 1024 ) + HashBlock( pMemory, 1024 ) + 10.0 );
		}
		if ( AccessEvent()->Check() || IsFinished() )
		{
			if ( !g_bOutputError )
			{
				Msg( "Forced execute test failed!\n" );
				DebuggerBreakIfDebugging();
			}
		}
		return 0;
	}
};

class CExecuteTestExecuteJob : public CJob
{
public:
	virtual JobStatus_t DoExecute()
	{
		bool bAbort = ( RandomInt(  1, 10 ) == 1 );
		g_nReady++;
		while ( !g_ReadyToExecute )
		{
			ThreadPause();
		}

		if ( !bAbort )
			m_pTestJob->Execute();
		else
			m_pTestJob->Abort();
		g_nReady--;
		return 0;
	}

	CExecuteTestJob *m_pTestJob;
};


void TestForcedExecute()
{
	Msg( "TestForcedExecute\n" );
	for ( int tests = 0; tests < 30; tests++ )
	{
		for ( int i = 1; i <= 5; i += 2 )
		{
			g_nReady = 0;
			ThreadPoolStartParams_t params;
			params.nThreads = i;
			params.fDistribute = TRS_TRUE;
			g_pTestThreadPool->Start( params, "Tst" );

			int nJobCount = 4000;
			CExecuteTestJob *jobs = new CExecuteTestJob[nJobCount];
			for ( int j = 0; j < nJobCount; j++ )
			{
				g_ReadyToExecute = false;
				for ( int k = 0; k < i; k++ )
				{
					CExecuteTestExecuteJob *pJob = new CExecuteTestExecuteJob;
					pJob->SetFlags( JF_QUEUE );
					pJob->m_pTestJob = &jobs[j];
					g_pTestThreadPool->AddJob( pJob );
					pJob->Release();
				}
				while ( g_nReady < i )
				{
					ThreadPause();
				}
				g_ReadyToExecute = true;
				ThreadSleep();
				jobs[j].Execute();
				while ( g_nReady > 0 )
				{
					ThreadPause();
				}
			}
			g_pTestThreadPool->Stop();
			delete []jobs;
		}
	}
	Msg( "TestForcedExecute DONE\n" );
}

} // namespace ThreadPoolTest

void RunThreadPoolTests()
{
	CThreadPool pool;
	ThreadPoolTest::g_pTestThreadPool = &pool;
	RunTSQueueTests(10000);
	RunTSListTests(10000);

	intp mask1=-1;
#ifdef _WIN32
	intp mask2 = -1;
	GetProcessAffinityMask( GetCurrentProcess(), (DWORD_PTR *) &mask1, (DWORD_PTR *) &mask2 );
#endif
	Msg( "ThreadPoolTest: Job distribution speed\n" );
	for ( int i = 0; i < 2; i++ )
	{
		bool bToCompletion = ( i % 2 != 0 );
		Msg( bToCompletion ? "ThreadPoolTest:   To completion\n" : "ThreadPoolTest:   NOT to completion\n" );
		if ( !IsX360() )
		{
			Msg( "ThreadPoolTest:     Non-distribute\n" );
			ThreadPoolTest::Test( false, true, bToCompletion );
		}

		Msg( "ThreadPoolTest:     Distribute\n" );
		ThreadPoolTest::Test( true, true, bToCompletion  );

// 		Msg( "ThreadPoolTest:     One core\n" );
// 		ThreadSetAffinity( 0, 1 );
// 		ThreadPoolTest::Test( false, true, bToCompletion  );
// 		ThreadSetAffinity( 0, mask1 );

		Msg( "ThreadPoolTest:     NO Sleep\n" );
		ThreadPoolTest::Test( false, false, bToCompletion  );

		Msg( "ThreadPoolTest:     Distribute NO Sleep\n" );
		ThreadPoolTest::Test( true, false, bToCompletion  );

//		Not plumbed correctly
// 		Msg( "ThreadPoolTest:     One core\n" );
// 		ThreadSetAffinity( 0, 1 );
// 		ThreadPoolTest::Test( false, false, bToCompletion  );
// 		ThreadSetAffinity( 0, mask1 );
	}

	for ( int bMain = 0; bMain < 2; bMain++ )
	{
		Msg( "ThreadPoolTest: Jobs doing work, %s main thread\n", bMain ? "WITH" : "without" );
		for ( int i = 0; i < 2; i++ )
		{
			bool bToCompletion = true;// = ( i % 2 != 0 );
			if ( !IsX360() )
			{
				Msg( "ThreadPoolTest:     Non-distribute\n" );
				ThreadPoolTest::Test( false, true, bToCompletion, true, !!bMain );
			}

			Msg( "ThreadPoolTest:     Distribute\n" );
			ThreadPoolTest::Test( true, true, bToCompletion, true, !!bMain );

// 			Msg( "ThreadPoolTest:     One core\n" );
// 			ThreadSetAffinity( 0, 1 );
// 			ThreadPoolTest::Test( false, true, bToCompletion, true, !!bMain );
// 			ThreadSetAffinity( 0, mask1 );

			Msg( "ThreadPoolTest:     NO Sleep\n" );
			ThreadPoolTest::Test( false, false, bToCompletion, true, !!bMain );

			Msg( "ThreadPoolTest:     Distribute NO Sleep\n" );
			ThreadPoolTest::Test( true, false, bToCompletion, true, !!bMain );

// 			Msg( "ThreadPoolTest:     One core\n" );
// 			ThreadSetAffinity( 0, 1 );
// 			ThreadPoolTest::Test( false, false, bToCompletion, true, !!bMain );
// 			ThreadSetAffinity( 0, mask1 );
		}
	}
#ifdef _WIN32
	GetProcessAffinityMask( GetCurrentProcess(), (DWORD_PTR *) &mask1, (DWORD_PTR *) &mask2 );
#endif

	ThreadPoolTest::TestForcedExecute();
}

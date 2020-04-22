//========= Copyright 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef GC_JOB_H
#define GC_JOB_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/memdbgon.h"
#include "tier1/functors.h"
#include "workthreadpool.h"

class GCConVar;

namespace GCSDK
{
class CJobMgr;
class CLock;
class CJob;
class IMsgNetPacket;

// job creation function responsible for allocating the job of the specific type
typedef CJob *(*JobCreationFunc_t)( void *pvServerParent, void * pvStartParam );
// job routing function, which allows for controlling message routing through the system. The GCs that should have the message sent to it should be added to the vector. It is fine to add the actual self GC
typedef void (*JobRoutingFunc_t)( CUtlVector< uint32 >& vecGCsToSendTo, IMsgNetPacket *pNetPacket );

//-----------------------------------------------------------------------------
// Purpose: static job information
//                      contains information relevant to one type of CJob
//-----------------------------------------------------------------------------
struct JobType_t
{
	const char *m_pchName;              // name of this type of job
	MsgType_t m_eCreationMsg;           // network message that creates this job
	uint32 m_nValidContexts;			// a bit field indicating which contexts this message can be called from (i.e. from a server, or from a client, etc)
	JobCreationFunc_t m_pJobFactory;	// virtual constructor
	JobRoutingFunc_t m_pJobRouter;		// routing function for this job
	const GCConVar*	m_pControlCV;		// optional console variable that can be used to disable this message
};


//-----------------------------------------------------------------------------
// Purpose: reason as to why the current job has yielded to the main thread (paused)
//			if this is updated, k_prgchJobPauseReason[] in job.cpp also needs to be updated
//-----------------------------------------------------------------------------
enum EJobPauseReason
{
	k_EJobPauseReasonNone,
	k_EJobPauseReasonNotStarted,
	k_EJobPauseReasonNetworkMsg,
	k_EJobPauseReasonSleepForTime,
	k_EJobPauseReasonWaitingForLock,
	k_EJobPauseReasonYield,
	k_EJobPauseReasonSQL,
	k_EJobPauseReasonWorkItem,

	k_EJobPauseReasonCount
};


//-----------------------------------------------------------------------------
// Purpose: contains information used to route a message to a job, or to
//			create a new job from that message type
//-----------------------------------------------------------------------------
struct JobMsgInfo_t
{
	MsgType_t m_eMsg;
	JobID_t m_JobIDSource;
	JobID_t m_JobIDTarget;

	JobMsgInfo_t()
	{
		m_eMsg = (MsgType_t)0;
		m_JobIDSource = k_GIDNil;
		m_JobIDTarget = k_GIDNil;
	}

	JobMsgInfo_t( MsgType_t eMsg, JobID_t jobIDSource, JobID_t jobIDTarget )
	{
		m_eMsg = eMsg;
		m_JobIDSource = jobIDSource;
		m_JobIDTarget = jobIDTarget;
	}
};

typedef void (CJob::*JobThreadFunc_t)();

#define BYieldingAcquireLock( lock ) _BYieldingAcquireLock( lock, __FILE__, __LINE__ ) 
#define BAcquireLockImmediate( lock ) _BAcquireLockImmediate( lock, __FILE__, __LINE__ )
#define ReleaseLock( lock ) _ReleaseLock( lock, false, __FILE__, __LINE__ )

//-----------------------------------------------------------------------------
// Purpose: A job is any server operation that requires state.  Typically, we use jobs for
//			operations that need to pause waiting for responses from other servers.  The
//			job object persists the state of the operation while it waits, and the reply
//			from the remote server re-activates the job.
//-----------------------------------------------------------------------------
class CJob
{
public:
	// Constructors & destructors, when overriding job name a static string pointer must be used
	explicit CJob( CJobMgr &jobMgr, char const *pchJobName = NULL );
	virtual ~CJob();

	// starts the job, storing off the network msg and calling it's Run() function
	void StartJobFromNetworkMsg( IMsgNetPacket *pNetPacket, const JobID_t &gidJobIDSrc, uint32 nContextMask );

	// accessors
	JobID_t GetJobID() const { return m_JobID; }

	// called to start a job. The default behavior of starting a job is to start it scheduled (i.e. delayed). This is largely deprecated, and the more explicit versions
	// below should be used instead
	void StartJob( void * pvStartParam );
	// schedules the job for execution, but does not interrupt the currently running job. Effectively starts the job on the yielding list as if it had immediately yielded
	// although is more efficient than actually doing so
	void StartJobDelayed( void * pvStartParam );
	// starts a job immediately, interrupting the current job if one is already running. This should only be used in special cases, and in a lot of ways should be considered
	// yielding in that another job can run and perform modifications, although the caller will receive attention back the first time that the inner job itself yields
	void StartJobImmediate( void * pvStartParam );

	// string name of the job
	const char *GetName() const;
	// return reason why we're paused
	EJobPauseReason GetPauseReason() const		{ return m_ePauseReason; }
	// string description of why we're paused
	const char *GetPauseReasonDescription() const;
	// return time at which this job was last paused or continued
	const CJobTime& GetTimeSwitched() const		{ return m_STimeSwitched; }
	// return microseconds run since we were last continued
	uint64 GetMicrosecondsRun() const			{ return m_FastTimerDelta.GetDurationInProgress().GetUlMicroseconds(); }
	bool BJobNeedsToHeartbeat() const			{ return ( m_STimeNextHeartbeat.CServerMicroSecsPassed() >= 0 ); }

	// --- locking pointers
	bool _BYieldingAcquireLock( CLock *pLock, const char *filename = "unknown", int line = 0 );
	bool _BAcquireLockImmediate( CLock *pLock, const char *filename = "unknown", int line = 0 );
	void _ReleaseLock( CLock *pLock, bool bForce = false, const char *filename = "unknown", int line = 0 );
	void ReleaseLocks();
	bool BJobHoldsLock( uint16 nType, uint64 nSubType ) const;
	bool BJobHoldsLock( const CLock* pLock ) const;

	/// If we hold any locks, spew about them and release them.
	/// This is useful for long running jobs, to make sure they don't leak
	/// locks that never get cleaned up
	void ShouldNotHoldAnyLocks();

	// --- general methods for waiting for events
	// Simple yield to other jobs until Run() called again
	bool BYield();
	// Yield IF JobMgr thinks we need to based on how long we've run and our priority
	bool BYieldIfNeeded( bool *pbYielded = NULL );
	// waits for a set amount of time
	bool BYieldingWaitTime( uint32 m_cMicrosecondsToSleep );
	bool BYieldingWaitOneFrame();
	// waits for another network msg, returning false if none returns
	bool BYieldingWaitForMsg( IMsgNetPacket **ppNetPacket );
	bool BYieldingWaitForMsg( CGCMsgBase *pMsg, MsgType_t eMsg );
	bool BYieldingWaitForMsg( CProtoBufMsgBase *pMsg, MsgType_t eMsg );

	// waits for another job(s) to complete
	bool BYieldingWaitForJob( JobID_t jobToWaitFor );
	bool BYieldingWaitForJobs( const CUtlVector<JobID_t> &vecJobsToWaitFor );

#ifdef GC
	void SOVALIDATE_SetSteamID( const CSteamID steamID )	{ m_SOCacheValidSteamID = steamID; }
	CSteamID SOVALIDATE_GetSteamID() const					{ return m_SOCacheValidSteamID; }
	void VALIDATE_SetJobAccessType( uint32 nAccess)			{ m_nGCJobAccessType = nAccess; }
	uint32 VALIDATE_GetJobAccessType() const				{ return m_nGCJobAccessType; }
	bool BYieldingWaitForMsg( CGCMsgBase *pMsg, MsgType_t eMsg, const CSteamID &expectedID );
	bool BYieldingWaitForMsg( CProtoBufMsgBase *pMsg, MsgType_t eMsg, const CSteamID &expectedID );
	bool BYieldingRunQuery( CGCSQLQueryGroup *pQueryGroup, ESchemaCatalog eSchemaCatalog );
#endif

	void RecordWaitTimeout() { m_flags.m_bits.m_bWaitTimeout = true; }

	// wait for pending work items before deleting job
	void WaitForThreadFuncWorkItemBlocking();

	// waits for a work item completion callback
	// You can pass a string that describes what sort of work item you are waiting on.
	// WARNING: This function saves the pointer to the string, it doesn't copy the string
	bool BYieldingWaitForWorkItem( const char *pszWorkItemName = NULL );

	// adds this work item to threaded work pool and waits for it
	bool BYieldingWaitForThreadFuncWorkItem( CWorkItem * );

	// calls a local function in a thread, and yields until it's done
	bool BYieldingWaitForThreadFunc( CFunctor *jobFunctor );

	// creates a job
	template <typename JOB_TYPE, typename PARAM_TYPE>
	static JOB_TYPE *AllocateJob( PARAM_TYPE *pParam )
	{
		return new JOB_TYPE( pParam );
	}
	// delete a job (the job knows what allocator to use)
	static void DeleteJob( CJob *pJob );

	void SetStartParam( void * pvStartParam )		{ Assert( NULL == m_pvStartParam ); m_pvStartParam = pvStartParam; }
	void SetFromFromMsg( bool bRunFromMsg )			{ m_bRunFromMsg = true; }

	void AddPacketToList( IMsgNetPacket *pNetPacket, const JobID_t gidJobIDSrc );
	// marks a packet as being finished with, releases the packet and frees the memory
	void ReleaseNetPacket( IMsgNetPacket *pNetPacket );

	void EndPause( EJobPauseReason eExpectedState );

	// Generate an assertion in the coroutine of this job
	// (creating a minidump).  Useful for inspecting stuck jobs
	void GenerateAssert( const char *pchMsg = NULL );

	//called to determine if the requested context is valid. If multiple contexts are provided, this will return true only if ALL the contexts are valid
	bool BHasContext( uint32 nContext ) const			{ return ( m_nContextMask & nContext ) == nContext; }
	uint32 GetContexts() const							{ return m_nContextMask; }

	//called to control the default behavior for starting jobs, immediate, or delayed
	static void SetStartDefaultJobsDelayed( bool bStartJobsDelayed )		{ s_bStartDefaultJobsDelayed = bStartJobsDelayed; }

	// accessor to get access to the JobMgr from the server we belong to
	CJobMgr &GetJobMgr();

protected:
	// main job implementation, in the coroutine.  Every job must implement at least one of these methods.
	virtual bool BYieldingRunJob( void * pvStartParam )				{ Assert( false ); return true; }	// implement this if your job can be started directly
	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket * pNetPacket )	{ Assert( false ); return true; }	// implement this if your job can be started by a network message

	// Can be overridden to return a different timeout per job class
	virtual uint32 CHeartbeatsBeforeTimeout();

	// Called by CJobMgr to send heartbeat message to our listeners during long operations
	void Heartbeat();


	uint32 m_bRunFromMsg:1,
			m_bWorkItemCanceled:1,			// true if the work item we were waiting on was canceled
			m_bIsTest:1,
			m_bIsLongRunning:1;

private:
	// starts the coroutine that activates the job
	void InitCoroutine();

	// continues the current job
	void Continue();

	// break into this coroutine - can only be called from OUTSIDE this coroutine
	void Debug();

	// pauses the current job - can only be called from inside a coroutine
	void Pause( EJobPauseReason eReason, const char *pszResourceName );

	static void BRunProxy( void *pvThis );

	JobID_t m_JobID;					// Our unique identifier.
	HCoroutine m_hCoroutine;
	void * m_pvStartParam;				// Start params for our job, if any
	// all these flags indicate some kind of failure and we will want to report them
	union  {
		struct {
			uint32
				m_bJobFailed:1,					// true if BYieldingRunJob returned false
				m_bLocksFailed:1,
				m_bLocksLongHeld:1,
				m_bLocksLongWait:1,
				m_bWaitTimeout:1,
				m_bLongInterYield:1,
				m_bTimeoutNetMsg:1,
				m_bTimeoutOther:1,
				m_uUnused:24;
			} m_bits;
		uint32 m_uFlags;
		} m_flags;
	int m_cLocksAttempted;
	int m_cLocksWaitedFor;
	EJobPauseReason m_ePauseReason;
	const char *m_pszPauseResourceName;
	MsgType_t	m_unWaitMsgType;
	CJobTime m_STimeStarted;				// time (frame) at which this job started
	CJobTime m_STimeSwitched;				// time (frame) at which we were last paused or continued
	CJobTime m_STimeNextHeartbeat;		// Time at which next heartbeat should be performed
	CFastTimer m_FastTimerDelta;		// How much time we've been running for without yielding
	CCycleCount m_cyclecountTotal;		// Total runtime
	uint32		m_nContextMask;			// The context that this job was created in. Typically only used for message jobs to indicate the initiator of the message
	CJob *m_pJobPrev;					// the job that launched us

	// lock manipulation
	void _SetLock( CLock *pLock, const char *filename, int line );
	void UnsetLock( CLock *pLock );
	void PassLockToJob( CJob *pNewJob, CLock *pLock );
	void OnLockDeleted( CLock *pLock );
	void AddJobToNotifyOnLockRelease( CJob *pJob );
	CUtlVectorFixedGrowable< CLock *, 2 > m_vecLocks;
	CLock *m_pWaitingOnLock;			// lock we're waiting on, if any
	const char *m_pWaitingOnLockFilename;
	int m_waitingOnLockLine;
	CJob *m_pJobToNotifyOnLockRelease;	// other job that wants this lock
	CWorkItem *m_pWaitingOnWorkItem;	// set if job is waiting for this work item

	#ifdef GC
		//context flags indicating what this job can access. Temporary and only for validating access on the GC
		uint32		m_nGCJobAccessType;
		//the steam ID that we are stating is safe to access. This is temporary to validate jobs during the split of the GC
		CSteamID	m_SOCacheValidSteamID;
	#endif

	CJobMgr &m_JobMgr;					// our job manager
	CUtlVectorFixedGrowable< IMsgNetPacket *, 1 > m_vecNetPackets;			// list of tcp packets currently held by this job (ie, needing release on job exit)

	// pointer to our own static job info
	struct JobType_t const *m_pJobType;

	// Name of the job for when it's not registered
	const char *m_pchJobName;

	// setting the job info
	friend void Job_SetJobType( CJob &job, const JobType_t *pJobType );
	friend class CJobMgr;
	friend class CLock;

	// used to store the memory allocation stack
	CUtlMemory< unsigned char > m_memAllocStack;

	static bool	s_bStartDefaultJobsDelayed;
};


// Only one job can be running at a time.  We keep a global accessor to it.
extern CJob *g_pJobCur;
inline CJob &GJobCur() { Assert( g_pJobCur != NULL ); return *g_pJobCur; }

#define AssertRunningJob() { Assert( NULL != g_pJobCur ); }
#define AssertRunningThisJob() { Assert( this == g_pJobCur ); }
#define AssertNotRunningThisJob() { Assert( this != g_pJobCur ); }
#define AssertNotRunningJob() { Assert( NULL == g_pJobCur ); }


//-----------------------------------------------------------------------------
// Purpose: simple locking class
//			add this object to any classes you want jobs to be able to lock
//-----------------------------------------------------------------------------
#if defined( GC )
#include "tier0/memdbgoff.h"
#endif

class CLock
{
	#if defined( GC )
		DECLARE_CLASS_MEMPOOL( CLock );
	#endif
public:
	CLock( );
	~CLock();
	
	bool BIsLocked() const			{ return m_pJob != NULL; }
	CJob *GetJobLocking() 			{ return m_pJob; }
	CJob *GetJobWaitingQueueHead()	{ return m_pJobToNotifyOnLockRelease; }
	CJob *GetJobWaitingQueueTail()	{ return m_pJobWaitingQueueTail; }
	void AddToWaitingQueue( CJob *pJob );
	
	const char *GetName() const;
	void SetName( const char *pchName );
	
	int16 GetLockType() const { return m_nsLockType; }
	void SetLockType( int16 nsLockType ) { m_nsLockType = nsLockType; }
	uint64 GetLockSubType() const { return m_unLockSubType; }
	void SetLockSubType( uint64 unLockSubType ) { m_unLockSubType = unLockSubType; }
	int32 GetWaitingCount() const { return m_nWaitingCount; }
	int64 GetMicroSecondsSinceLock() const { return m_sTimeAcquired.CServerMicroSecsPassed(); }
	void IncrementReference();
	int DecrementReference();
	void ClearReference() { m_nRefCount = 0; }
	int32 GetReferenceCount() const { return m_nRefCount; }

	void Dump( const char *pszPrefix = "\t\t", int nPrintMax = 1, bool bPrintWaiting = true ) const;

private:
	CJob *m_pJob;						// the job that's currently locking us
	CJob *m_pJobToNotifyOnLockRelease;	// Pointer to the first job waiting on us
	CJob *m_pJobWaitingQueueTail;		// Pointer to the last job waiting on us
	
	const char *m_pchConstStr;			// Prefix part of the lock's name

	int32 m_nRefCount;					// # of times locked
	int32 m_nWaitingCount;				// Count of jobs waiting on the lock
	CJobTime m_sTimeAcquired;			// Time the lock was last locked
	uint64 m_unLockSubType;

	const char *m_pFilename;			// Filename of the source file who acquired this lock
	int m_line;							// Line number of the filename
	int16 m_nsLockType;					// Lock priority for safely waiting on multiple locks

	friend class CJob;
};

#if defined( GC )
#include "tier0/memdbgon.h"
#endif


//-----------------------------------------------------------------------------------------
// An auto lock class which handles auto lifetime management of a lock
//-----------------------------------------------------------------------------------------
class CGCAutoLock
{
public:
	CGCAutoLock() : m_pLock( NULL )			{}
	CGCAutoLock( const CGCAutoLock& rhs )	{ Acquire( rhs.m_pLock ); }
	~CGCAutoLock()							{ Release(); }

	//determines if this lock is currently locked or not
	bool IsLocked() const					{ return ( m_pLock != NULL ); }

	//swaps two locks (faster than doing reassignments due to not needing all the reference counting)
	void Swap( CGCAutoLock& rhs )				{ std::swap( m_pLock, rhs.m_pLock ); }

	CGCAutoLock& operator=( const CGCAutoLock& rhs );

	//called to acquire a lock (the odd naming convention is to match the macro format to automatically provide the file and line of the call site)
	bool _BYieldingAcquireLock( CLock& lock, const char* pszFile, uint32 nLine );

	//called to release the current lock
	void Release();

private:
	void Acquire( CLock* pLock );
	CLock*	m_pLock;
};


//-----------------------------------------------------------------------------
// Purpose: Use these macros to declare blocks where it is unsafe to yield.
//	The job will assert if it pauses within the block
//-----------------------------------------------------------------------------
#define DO_NOT_YIELD_THIS_SCOPE()	CDoNotYieldScopeImpl doNotYieldScope_##line( FILE_AND_LINE )
#define BEGIN_DO_NOT_YIELD()		CDoNotYieldScopeImpl::InternalPush( FILE_AND_LINE )
#define END_DO_NOT_YIELD()			CDoNotYieldScopeImpl::InternalPop()

class CDoNotYieldScopeImpl
{
public:
	explicit CDoNotYieldScopeImpl( const char *pchLocation ) { InternalPush( pchLocation ); }
	~CDoNotYieldScopeImpl() { InternalPop(); }

	static void InternalPush( const char *pchLocation );
	static void InternalPop();
private:
	// Disallow these constructors and operators
	CDoNotYieldScopeImpl( const CDoNotYieldScopeImpl &that );
	CDoNotYieldScopeImpl &operator=( const CDoNotYieldScopeImpl &that );
};

} // namespace GCSDK

#include "tier0/memdbgoff.h"

#endif // GC_JOB_H

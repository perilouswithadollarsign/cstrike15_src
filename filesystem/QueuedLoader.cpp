//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Queued Loading of map resources. !!!!Specifically!!! designed for the map loading process.
//
// Not designed for application startup or gameplay time. Layered on top of async i/o.
// Queued loading is allowed during the map load process until full connection only,
// but can complete the remaining low priority jobs during the game render.
// The normal loading path can run in units of seconds if it does not have to do I/O,
// which is why this system runs first and gets all the data in memory unhindered
// by dependency blocking. The I/O delivery process achieves its speed by having all the I/O
// requests at once, performing the I/O, and handing the actual consumption
// of the I/O buffer to another available core/thread (via job pool) for computation work.
// The I/O (should be all unbuffered) is then only throttled by physical transfer rates.
//
// The Load process is broken into three phases. The first phase build up I/O requests.
// The second phase fulfills only the high priority I/O requests. This gets the critical
// data in memory, that has to be there for the normal load path to query, or the renderer
// to run (i.e. models and shaders). The third phase is the normal load process.
// The low priority jobs run concurrently with the normal load process.  Low priority jobs
// are those that have been specially built such that the game or loading can operate unblocked
// without the actual data (i.e. d3d texture bits).
//
// Phase 1: The reslist is parsed into seperate lists based on handled extensions. Each list
// call its own loader which in turn generates its own dictionaries and I/O requests through
// "AddJob". A single reslist entry could cause a laoder to request multiple jobs. ( i.e. models )
// A loader marks its jobs as high or low priority.
// Phase 2: The I/O requests are sorted (which achieves seek offset order) and
// async i/o commences. Phase 2 does not end until all the high priority jobs
// are complete. This ensures critical data is resident.
// Phase 3: The !!!NORMAL!!! loading path can commence. The legacy loading path then
// is not expected to do I/O (it can, but that's a hole in the reslist), as all of the data
// that it queries, should be resident.
//
// Late added jobs are non-optimal (should have been in reslist), warned, but handled.
//
//===========================================================================//

#include "basefilesystem.h"

#include "tier0/vprof.h"
#include "tier0/tslist.h"
#include "tier1/utlbuffer.h"
#include "tier1/convar.h"
#include "tier1/keyvalues.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlstring.h"
#include "tier1/utlsortvector.h"
#include "tier1/utldict.h"
#include "basefilesystem.h"
#include "tier0/icommandline.h"
#include "vstdlib/jobthread.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/tier2.h"
#include "characterset.h"
#include "tier1/lzmaDecoder.h"
#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif
#ifdef _PS3
#include "tls_ps3.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PRIORITY_HIGH		1
#define PRIORITY_NORMAL		0
#define PRIORITY_LOW		-1

// main thread has reason to block and wait for thread pool to finish jobs
#define MAIN_THREAD_YIELD_TIME	20

// discrete stages in the preload process to tick the progress bar
#define PROGRESS_START				0.10f
#define PROGRESS_GOTRESLIST			0.12f
#define PROGRESS_PARSEDRESLIST		0.15f
#define PROGRESS_CREATEDRESOURCES	0.20f
#define PROGRESS_PREPURGE			0.22f
#define PROGRESS_FILESORT			0.23f
#define PROGRESS_IO					0.25f
#define PROGRESS_END				0.999f	// up to 1.0

struct FileJob_t
{
	FileJob_t()
	{
		Q_memset( this, 0, sizeof( FileJob_t ) );
	}

	FileNameHandle_t		m_hFilename;
	QueuedLoaderCallback_t	m_pCallback;
	FSAsyncControl_t		m_hAsyncControl;
	void					*m_pContext;
	void					*m_pContext2;
	void					*m_pTargetData;
	int						m_nBytesToRead;
	unsigned int			m_nStartOffset;
	LoaderPriority_t		m_Priority;

	unsigned int			m_SubmitTime;
	unsigned int			m_FinishTime;
	int						m_SubmitTag;
	int						m_nActualBytesRead;
	LoaderError_t			m_LoaderError;
	unsigned int			m_ThreadId;

	unsigned int			m_bFinished : 1;
	unsigned int			m_bFreeTargetAfterIO : 1;
	unsigned int			m_bFileExists : 1;
	unsigned int			m_bClaimed : 1;
	unsigned int			m_bLateQueued : 1;
	unsigned int			m_bAnonymousDecode : 1;
};

// dummy stubbed progress interface
class CDummyProgress : public ILoaderProgress
{
	void BeginProgress() {}
	void UpdateProgress( float progress, bool bForce = false ) {}
	void PauseNonInteractiveProgress( bool bPause ) {}
	void EndProgress() {}
};
static CDummyProgress s_DummyProgress;

class CQueuedLoader : public CTier2AppSystem< IQueuedLoader >
{
	typedef CTier2AppSystem< IQueuedLoader > BaseClass;

public:
	CQueuedLoader();
	virtual ~CQueuedLoader();

	// Inherited from IAppSystem
	virtual InitReturnVal_t				Init();
	virtual void						Shutdown();

	// IQueuedLoader
	virtual void						InstallLoader( ResourcePreload_t type, IResourcePreload *pLoader );
	virtual void						InstallProgress( ILoaderProgress *pProgress );
	// Set bOptimizeReload if you want appropriate data (such as static prop lighting)
	// to persist - rather than being purged and reloaded - when going from map A to map A.
	virtual bool						BeginMapLoading( const char *pMapName, bool bLoadForHDR, bool bOptimizeMapReload, void (*pfnBeginMapLoadingCallback)( int nStage ) );
	virtual void						EndMapLoading( bool bAbort );
	virtual bool						AddJob( const LoaderJob_t *pLoaderJob );
	virtual void						AddMapResource( const char *pFilename );
	virtual bool						ClaimAnonymousJob( const char *pFilename, QueuedLoaderCallback_t pCallback, void *pContext, void *pContext2 );
	virtual bool						ClaimAnonymousJob( const char *pFilename, void **pData, int *pDataSize, LoaderError_t *pError );
	virtual bool						IsMapLoading() const;
	virtual bool						IsSameMapLoading() const;
	virtual bool						IsFinished() const;
	virtual bool						IsBatching() const;
	virtual int							GetSpewDetail() const;

	char								*GetFilename( const FileNameHandle_t hFilename, char *pBuff, int nBuffSize );	
	FileNameHandle_t					FindFilename( const char *pFilename );
	void								SpewInfo();
	unsigned int						GetStartTime();

	// submit any queued jobs to the async loader, called by main or async thread to get more work
	void								SubmitPendingJobs();

	void								PurgeAll( ResourcePreload_t *pDontPurgeList = NULL, int nPurgeListSize = 0 );
#ifdef _PS3
	// hack to prevent PS/3 deadlock on queued loader render mutex when quitting during loading a map
	// PLEASE REMOVE THIS (AND THE MUTEX) AFTER WE SHIP
	virtual uint UnlockProgressBarMutex()
	{
		uint nCount = m_nRendererMutexProgressBarUnlockCount;
		for( uint i = 0; i < nCount ;  ++i )
		{
			m_sRendererMutex.Unlock();
		}
		return nCount;
	}
	virtual void LockProgressBarMutex( uint nLockCount )
	{
		for( uint i = 0; i < nLockCount ;  ++i )
		{
			m_sRendererMutex.Lock();
		}
	}
#endif
private:

	class CFileJobsLessFunc
	{
	public:
		int GetLayoutOrderForFilename( const char *pFilename );
		bool Less( FileJob_t* const &pFileJobLHS, FileJob_t* const &pFileJobRHS, void *pCtx );
	};

	class CResourceNameLessFunc
	{
	public:
		bool Less( const FileNameHandle_t &hFilenameLHS, const FileNameHandle_t &hFilenameRHS, void *pCtx );
	};
	typedef CUtlSortVector< FileNameHandle_t, CResourceNameLessFunc > ResourceList_t;

	ILoaderProgress *			GetProgress() { return m_pProgress; };

	static void							BuildResources( IResourcePreload *pLoader, ResourceList_t *pList, float *pBuildTime );
	static void							BuildMaterialResources( IResourcePreload *pLoader, ResourceList_t *pList, float *pBuildTime );

	void								PurgeQueue();
	void								CleanQueue();
	void								SubmitBatchedJobsAndWait();
	void								ParseResourceList( CUtlBuffer &resourceList );
	void								GetJobRequests();
	void								PurgeUnreferencedResources();
	void								AddResourceToTable( const char *pFilename );
	void								GetDVDLayout();

	bool								m_bStarted;
	bool								m_bActive;
	bool								m_bBatching;
	bool								m_bCanBatch;
	bool								m_bLoadForHDR;
	bool								m_bDoProgress;
	bool								m_bSameMap;
	int									m_nSubmitCount;
	unsigned int						m_StartTime;
	unsigned int						m_EndTime;
	char								m_szMapNameToCompareSame[MAX_PATH];

	CUtlFilenameSymbolTable				m_Filenames;
	CTSList< FileJob_t* >				m_PendingJobs;
	CTSList< FileJob_t* >				m_BatchedJobs;	
	CUtlLinkedList< FileJob_t* >		m_SubmittedJobs;
	CUtlDict< FileJob_t*, int >			m_AnonymousJobs;
	CUtlSymbolTable						m_AdditionalResources;

	CUtlSortVector< FileNameHandle_t, CResourceNameLessFunc >	m_ResourceNames[RESOURCEPRELOAD_COUNT];
	CUtlSortVector< FileNameHandle_t, CResourceNameLessFunc >	m_ExcludeResourceNames;

	IResourcePreload					*m_pLoaders[RESOURCEPRELOAD_COUNT];
	float								m_LoaderTimes[RESOURCEPRELOAD_COUNT];
	ILoaderProgress						*m_pProgress;
	CThreadFastMutex					m_Mutex;
#if defined( _PS3 )
	// PLEASE REMOVE THIS MUTEX AFTER WE SHIP, IT IS NOT NEEDED. But leaving it here because it's too close to ship date.
	static CThreadFastMutex				m_sRendererMutex;
	// this is the counter of locks we lock the renderer mutex for the ProgressBar call
	// it's used to unlock the mutex when going into Host_Error state to prevent deadlocks (it's a hack)
	static CInterlockedInt              m_nRendererMutexProgressBarUnlockCount;
#endif
};

#if defined( _PS3 )
CThreadFastMutex CQueuedLoader::m_sRendererMutex;
CInterlockedInt CQueuedLoader::m_nRendererMutexProgressBarUnlockCount;
class CInterlockedIntAutoIncrementer
{
protected:
	CInterlockedInt &m_refInt;
public:
	CInterlockedIntAutoIncrementer(CInterlockedInt &refInt): m_refInt( refInt ){ m_refInt ++; }
	~CInterlockedIntAutoIncrementer(){ m_refInt --;}
};
#endif

static CQueuedLoader g_QueuedLoader;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CQueuedLoader, IQueuedLoader, QUEUEDLOADER_INTERFACE_VERSION, g_QueuedLoader );

class CResourcePreloadAnonymous : public IResourcePreload
{
	virtual void PrepareForCreate( bool bSameMap ) {}

	virtual bool CreateResource( const char *pName )
	{
		// create an anonymous job to get the data in memory, claimed during load, or auto-freed
		LoaderJob_t loaderJob;
		loaderJob.m_pFilename = pName;
		loaderJob.m_pPathID = "GAME";
		loaderJob.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
		g_QueuedLoader.AddJob( &loaderJob );
		return true;
	}

	virtual void PurgeUnreferencedResources() {}
	virtual void OnEndMapLoading( bool bAbort ) {}
	virtual void PurgeAll() {}
#if defined( _PS3 )
	virtual bool RequiresRendererLock() { return true; }	// do we know that anonymous resource loads won't hit the renderer?
#endif // _PS3
};
static CResourcePreloadAnonymous s_ResourcePreloadAnonymous;

const char *g_ResourceLoaderNames[RESOURCEPRELOAD_COUNT] = 
{
	"???",			// RESOURCEPRELOAD_UNKNOWN
	"Sounds",		// RESOURCEPRELOAD_SOUND
	"Materials",	// RESOURCEPRELOAD_MATERIAL
	"Models",		// RESOURCEPRELOAD_MODEL
	"Cubemaps",		// RESOURCEPRELOAD_CUBEMAP
	"PropLighting",	// RESOURCEPRELOAD_STATICPROPLIGHTING
	"Anonymous",	// RESOURCEPRELOAD_ANONYMOUS
};

static CInterlockedInt	g_nActiveJobs;
static CInterlockedInt	g_nQueuedJobs;
static CInterlockedInt	g_nHighPriorityJobs;		// tracks jobs that must finish during preload
static CInterlockedInt	g_nJobsToFinishBeforePlay;	// tracks jobs that must finish before gameplay
static CInterlockedInt	g_nIOMemory;				// tracks I/O data from async delivery until consumed
static CInterlockedInt	g_nAnonymousIOMemory;		// tracks anonymous I/O data from async delivery until consumed
static CInterlockedInt	g_SuspendIO;				// used to throttle the I/O
static int				g_nIOMemoryPeak;
static int				g_nAnonymousIOMemoryPeak;
static int				g_nHighIOSuspensionMark;
static int				g_nLowIOSuspensionMark;
static CInterlockedInt	g_nForceSuspendIO;
static CUtlVector< CUtlString >	g_DVDLayout;

ConVar loader_spew_info( "loader_spew_info", "0", 0, "0:Off, 1:Timing, 2:Completions, 3:Late Completions, 4:Creations/Purges, -1:All" );
ConVar loader_throttle_io( "loader_throttle_io", "1" );

// debugging only state to defer all non high priority jobs until the very end of loading
// essentially making the EndMapLoading() do a bunch of work to stress the loading bar
ConVar loader_defer_non_critical_jobs( "loader_defer_non_critical_jobs", "0" );

CON_COMMAND( loader_dump_table, "" )
{
	g_QueuedLoader.SpewInfo();
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CQueuedLoader::CQueuedLoader()
{
	m_bStarted = false;
	m_bActive = false;
	m_bSameMap = false;
	m_szMapNameToCompareSame[0] = '\0';

	m_pProgress = &s_DummyProgress;
	V_memset( m_pLoaders, 0, sizeof( m_pLoaders ) );

	// set resource dictionaries sort context
	m_ExcludeResourceNames.SetLessContext( (void *)RESOURCEPRELOAD_UNKNOWN );
	for ( intp i = 0; i < RESOURCEPRELOAD_COUNT; i++ )
	{
		m_ResourceNames[i].SetLessContext( (void *)i );
	}

	InstallLoader( RESOURCEPRELOAD_ANONYMOUS, &s_ResourcePreloadAnonymous );
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CQueuedLoader::~CQueuedLoader()
{
}

//-----------------------------------------------------------------------------
// Computation job to build out objects
//-----------------------------------------------------------------------------
void CQueuedLoader::BuildResources( IResourcePreload *pLoader, ResourceList_t *pList, float *pBuildTime )
{
	float t0 = Plat_FloatTime();

	Assert( pLoader );
	if ( pLoader )
	{
		pLoader->PrepareForCreate( g_QueuedLoader.IsSameMapLoading() );

		pList->RedoSort();

		for ( int i = 0; i < pList->Count(); i++ )
		{
			char szFilename[MAX_PATH];
			g_QueuedLoader.GetFilename( pList->Element( i ), szFilename, sizeof( szFilename ) );
			if ( szFilename[0] )
			{
				if ( g_QueuedLoader.GetSpewDetail() & LOADER_DETAIL_CREATIONS )
				{
					Msg( "QueuedLoader: Creating: %s\n", szFilename );
				}

				bool bResourceCreated = false;
#if defined( _PS3 )
				// PSGL does not allow us to update its memory from two threads simultaneously,
				// even for unrelated textures, so we need to throttle these down to one at a time
				if( pLoader->RequiresRendererLock() )
				{
					AUTO_LOCK_FM( m_sRendererMutex );
					bResourceCreated = pLoader->CreateResource( szFilename );
				}
				else
#endif
				{
					bResourceCreated = pLoader->CreateResource( szFilename );
				}

				if ( !bResourceCreated )
				{
					Warning( "QueuedLoader: Failed to create resource %s\n", szFilename );
				}
			}
		}
	}

	// finished with list
	pList->Purge();

	*pBuildTime = Plat_FloatTime() - t0;
}

//-----------------------------------------------------------------------------
// Computation job to build out material objects
//-----------------------------------------------------------------------------
void CQueuedLoader::BuildMaterialResources( IResourcePreload *pLoader, ResourceList_t *pList, float *pBuildTime )
{
	float t0 = Plat_FloatTime();

	char szLastFilename[MAX_PATH];
	szLastFilename[0] = '\0';

	// ensure cubemaps are first
	pList->RedoSort();

	// run a clean operation to cull the non-patched env_cubemap materials, which are not built directly
	for ( int i = 0; i < pList->Count(); i++ )
	{
		char szFilename[MAX_PATH];
		char *pFilename = g_QueuedLoader.GetFilename( pList->Element( i ), szFilename, sizeof( szFilename ) );
		if ( !V_stristr( pFilename, "maps\\" ) )
		{
			// list is sorted, first non-cubemap marks end of relevant list
			break;
		}

		// skip past maps/mapname/
		pFilename += 5;
		pFilename = strchr( pFilename, '\\' ) + 1;
		// back up until end of material name is found, need to strip off _%d_%d_%d.vmt
		char *pEndFilename = V_stristr( pFilename, ".vmt" );
		if ( !pEndFilename )
		{
			pEndFilename = pFilename + strlen( pFilename );
		}
		int numUnderscores = 3;
		while ( pEndFilename != pFilename && numUnderscores > 0 )
		{
			pEndFilename--;	
			if ( pEndFilename[0] == '_' )
			{
				numUnderscores--;
			}
		}		
		if ( numUnderscores == 0 )
		{
			*pEndFilename = '\0';
			if ( !V_strcmp( szLastFilename, pFilename ) )
			{
				// same cubemap material base already processed, skip it
				continue;
			}
			V_strncpy( szLastFilename, pFilename, sizeof( szLastFilename ) );		

			strcat( pFilename, ".vmt" );
			FileNameHandle_t hFilename = g_QueuedLoader.FindFilename( pFilename );
			if ( hFilename )
			{
				pList->Remove( hFilename );
			}
		}
	}

	// process clean list
	BuildResources( pLoader, pList, pBuildTime );

	*pBuildTime = Plat_FloatTime() - t0;
}

//-----------------------------------------------------------------------------
// Called by multiple worker threads.  Throttle the I/O to ensure too many
// buffers don't flood the work queue. Anonymous I/O is allowed to grow unbounded.
//-----------------------------------------------------------------------------
void AdjustAsyncIOSpeed()
{
	// throttle back the I/O to keep the pending buffers from exhausting memory
	if ( g_SuspendIO == 0 )
	{
		if ( ( g_nForceSuspendIO == 1 || g_nIOMemory >= g_nHighIOSuspensionMark ) && g_nActiveJobs != 0 )
		{
			// protect against another worker thread
			if ( g_SuspendIO.AssignIf( 0, 1 ) )
			{
				if ( g_QueuedLoader.GetSpewDetail() )
				{
					Msg( "QueuedLoader: Suspending I/O at %.2f MB\n", (float)g_nIOMemory / ( 1024.0f * 1024.0f ) );
				}
				g_pFullFileSystem->AsyncSuspend();
			}
		}
	}
	else if ( g_SuspendIO == 1 )
	{
		if ( g_nForceSuspendIO != 1 && g_nIOMemory <= g_nLowIOSuspensionMark )
		{
			// protect against another worker thread
			if ( g_SuspendIO.AssignIf( 1, 0 ) )
			{
				if ( g_QueuedLoader.GetSpewDetail() )
				{
					Msg( "QueuedLoader: Resuming I/O at %.2f MB\n", (float)g_nIOMemory / ( 1024.0f * 1024.0f ) );
				}
				g_pFullFileSystem->AsyncResume();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Computation job to do work after IO, runs callback
//-----------------------------------------------------------------------------
void IOComputationJob( FileJob_t *pFileJob, void *pData, int nSize, LoaderError_t loaderError )
{
	int spewDetail = g_QueuedLoader.GetSpewDetail();
	if ( spewDetail & ( LOADER_DETAIL_COMPLETIONS|LOADER_DETAIL_LATECOMPLETIONS ) )
	{
		const char *pLateString = "";
		if ( !g_QueuedLoader.IsMapLoading() )
		{
			// completed outside of load process
			pLateString = "(Late) ";
		}
		
		if ( ( spewDetail & LOADER_DETAIL_COMPLETIONS ) || ( ( spewDetail & LOADER_DETAIL_LATECOMPLETIONS ) && pLateString[0] ) )
		{
			char szFilename[MAX_PATH];
			g_QueuedLoader.GetFilename( pFileJob->m_hFilename, szFilename, sizeof( szFilename ) );
			Msg( "QueuedLoader: Computation:%8.8llx, Size:%7d %s%s\n", (unsigned long long)ThreadGetCurrentId(), nSize, pLateString, szFilename );
		}
	}

	if ( loaderError != LOADERERROR_NONE && pFileJob->m_bFileExists )
	{
		char szFilename[MAX_PATH];
		g_QueuedLoader.GetFilename( pFileJob->m_hFilename, szFilename, sizeof( szFilename ) );
		Warning( "QueuedLoader:: I/O Error on %s\n", szFilename );
	}

	pFileJob->m_nActualBytesRead = nSize;
	pFileJob->m_LoaderError = loaderError;

	if ( !pFileJob->m_pCallback )
	{
		// absent callback means resource loader want this system to delay buffer until ready for it
		if ( !pFileJob->m_pTargetData )
		{
			if ( pFileJob->m_bAnonymousDecode )
			{
				// caller want us to decode
				// the actual inflated memory is not tracked, it should be
				CLZMA lzma;
				if ( lzma.IsCompressed( (unsigned char *)pData ) )
				{
					int originalSize = lzma.GetActualSize( (unsigned char *)pData );
					unsigned char *pDecodedData = (unsigned char *)g_pFullFileSystem->AllocOptimalReadBuffer( FILESYSTEM_INVALID_HANDLE, originalSize, 0 );
					lzma.Uncompress( (unsigned char *)pData, pDecodedData );

					// release the i/o buffer
					g_pFullFileSystem->FreeOptimalReadBuffer( pData );
					g_nAnonymousIOMemory -= pFileJob->m_nActualBytesRead;

					// now own the decoded data as if it was the i/o buffer
					pData = pDecodedData;
					pFileJob->m_nActualBytesRead = originalSize;
					g_nAnonymousIOMemory += originalSize;
				}
			}

			// track it for later, unclaimed buffers will get freed
			pFileJob->m_pTargetData = pData;
		}
	}
	else
	{
		// regardless of error, call job callback so caller can do cleanup of their context
		pFileJob->m_pCallback( pFileJob->m_pContext, pFileJob->m_pContext2, pData, nSize, loaderError );
		if ( pFileJob->m_bFreeTargetAfterIO && pData )
		{	
			// free our data only
			g_pFullFileSystem->FreeOptimalReadBuffer( pData );
		}

		// memory has been consumed
		g_nIOMemory -= nSize;
	}

	// mark as completed
	pFileJob->m_bFinished = true;
	pFileJob->m_FinishTime = Plat_MSTime();
	pFileJob->m_ThreadId = ThreadGetCurrentId();

	if ( pFileJob->m_Priority == LOADERPRIORITY_DURINGPRELOAD )
	{
		g_nHighPriorityJobs--;
	}
	else if ( pFileJob->m_Priority == LOADERPRIORITY_BEFOREPLAY )
	{
		g_nJobsToFinishBeforePlay--;
	}

	g_nQueuedJobs--;

	if ( g_nQueuedJobs == 0 && ( spewDetail & LOADER_DETAIL_TIMING ) )
	{
		Warning( "QueuedLoader: Finished I/O of all queued jobs! at: %u\n", Plat_MSTime() - g_QueuedLoader.GetStartTime() );
	}

	if ( ( g_nForceSuspendIO == -1 ) && g_nJobsToFinishBeforePlay && ( g_nHighPriorityJobs == 0 ) )
	{
		// can only suspend when there are no high priority jobs
		// these cannot be delayed, they must finish unhindered, otherwise we would spin lock
		if ( g_nForceSuspendIO.AssignIf( -1, 1 ) )
		{
			Warning( "QueuedLoader: Force Suspending I/O at: %u\n", Plat_MSTime() - g_QueuedLoader.GetStartTime() );
		}
	}

	AdjustAsyncIOSpeed();
}

//-----------------------------------------------------------------------------
// Computation job to do work after anonymous job was asynchronously claimed, runs callback.
//-----------------------------------------------------------------------------
void FinishAnonymousJob( FileJob_t *pFileJob, QueuedLoaderCallback_t pCallback, void *pContext, void *pContext2 )
{
	// regardless of error, call job callback so caller can do cleanup of their context
	pCallback( pContext, pContext2, pFileJob->m_pTargetData, pFileJob->m_nActualBytesRead, pFileJob->m_LoaderError );
	if ( pFileJob->m_bFreeTargetAfterIO && pFileJob->m_pTargetData )
	{	
		// free our data only
		g_pFullFileSystem->FreeOptimalReadBuffer( pFileJob->m_pTargetData );
		pFileJob->m_pTargetData = NULL;
	}

	pFileJob->m_bClaimed = true;

	// memory has been consumed
	g_nAnonymousIOMemory -= pFileJob->m_nActualBytesRead;
}

//-----------------------------------------------------------------------------
// Callback from I/O job thread. Purposely lightweight as possible to keep i/o from stalling.
//-----------------------------------------------------------------------------
void IOAsyncCallback( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	FileJob_t *pFileJob = (FileJob_t *)asyncRequest.pContext;

	// interpret the async error
	LoaderError_t loaderError;
	switch ( asyncStatus )
	{
	case FSASYNC_OK:
		loaderError = LOADERERROR_NONE;
		break;
	case FSASYNC_ERR_FILEOPEN:
		loaderError = LOADERERROR_FILEOPEN;
		break;
	default:
		loaderError = LOADERERROR_READING;
	}

	// track how much i/o data is in flight, consumption will decrement
	if ( !pFileJob->m_pCallback )
	{
		// anonymous io memory is tracked seperatley
		g_nAnonymousIOMemory += numReadBytes;
		if ( g_nAnonymousIOMemory > g_nAnonymousIOMemoryPeak )
		{
			g_nAnonymousIOMemoryPeak = g_nAnonymousIOMemory;
		}
	}
	else
	{
		g_nIOMemory += numReadBytes;
		if ( g_nIOMemory > g_nIOMemoryPeak )
		{
			g_nIOMemoryPeak = g_nIOMemory;
		}
	}

	// have data or error, do callback as a computation job
	g_pThreadPool->QueueCall( IOComputationJob, pFileJob, asyncRequest.pData, numReadBytes, loaderError )->Release();
	
	// don't let the i/o starve, possibly get some more work from the pending queue
	g_QueuedLoader.SubmitPendingJobs();

	// possibly goes to zero atomically, AFTER submission
	// prevents contention between main thread
	--g_nActiveJobs;
}

//-----------------------------------------------------------------------------
// Public method to filename dictionary
//-----------------------------------------------------------------------------
char *CQueuedLoader::GetFilename( const FileNameHandle_t hFilename, char *pBuff, int nBuffSize )
{
	m_Filenames.String( hFilename, pBuff, nBuffSize );
	return pBuff;
}

//-----------------------------------------------------------------------------
// Public method to filename dictionary
//-----------------------------------------------------------------------------
FileNameHandle_t CQueuedLoader::FindFilename( const char *pFilename )
{
	return m_Filenames.FindFileName( pFilename );
}

//-----------------------------------------------------------------------------
// Sort function for resource names.
//-----------------------------------------------------------------------------
bool CQueuedLoader::CResourceNameLessFunc::Less( const FileNameHandle_t &hFilenameLHS, const FileNameHandle_t &hFilenameRHS, void *pCtx )
{
	switch ( (intp)pCtx )
	{
	case RESOURCEPRELOAD_MATERIAL:
		{
			// Cubemap materials are expected to be at top of list
			char szNameLHS[MAX_PATH];
			char szNameRHS[MAX_PATH];

			const char *pNameLHS = g_QueuedLoader.GetFilename( hFilenameLHS, szNameLHS, sizeof( szNameLHS ) );
			const char *pNameRHS = g_QueuedLoader.GetFilename( hFilenameRHS, szNameRHS, sizeof( szNameRHS ) );

			bool bIsCubemapLHS = V_stristr( pNameLHS, "maps\\" ) != NULL;
			bool bIsCubemapRHS = V_stristr( pNameRHS, "maps\\" ) != NULL;
			if ( bIsCubemapLHS != bIsCubemapRHS )
			{
				return ( bIsCubemapLHS == true && bIsCubemapRHS == false );
			}
			return ( V_stricmp( pNameLHS, pNameRHS ) < 0 );
		}
		break;

	default:
		// sort not really needed, just use numeric handles
		return ( hFilenameLHS < hFilenameRHS );
	}
}

//-----------------------------------------------------------------------------
// The layout order of zips is assumed to match the search path order. The zip
// at the head of the search path should be the closest to the outer (faster)
// edge progressing to the tail of the search path whose zips should be
// towards the inner (slower) edge. The validity and necessity of this is really 
// specific to the actual game's data distribution in the zips. For now,
// good enough.
//-----------------------------------------------------------------------------
void CQueuedLoader::GetDVDLayout()
{
	g_DVDLayout.Purge();

	char searchPaths[32*MAX_PATH];
	g_pFullFileSystem->GetSearchPath( NULL, true, searchPaths, sizeof( searchPaths ) );

	for ( char *pPath = strtok( searchPaths, ";" ); pPath; pPath = strtok( NULL, ";" ) )
	{
		if ( V_stristr( pPath, ".zip" ) || V_stristr( pPath, ".bsp" ) )
		{
			// only want zip paths
			g_DVDLayout.AddToTail( pPath );
		}
	}
}

//-----------------------------------------------------------------------------
// Resolve filenames to expected disc layout order. Files within the same zip will
// resolve to the same numeric order and be alpha sub sorted, same as zip constructed.
// Files that are from different zips should sort to match the relative placement
// of the zips on the disc image.
//-----------------------------------------------------------------------------
int CQueuedLoader::CFileJobsLessFunc::GetLayoutOrderForFilename( const char *pFilename )
{
	for ( int i = 0; i < g_DVDLayout.Count(); i++ )
	{
		if ( V_stristr( pFilename, g_DVDLayout[i].String() ) )
		{
			return i;
		}
	}

	return INT_MAX;
}

//-----------------------------------------------------------------------------
bool CQueuedLoader::CFileJobsLessFunc::Less( FileJob_t* const &pFileJobLHS, FileJob_t* const &pFileJobRHS, void *pCtx )
{
	static int nCalls = 0;

	nCalls++; // 60,000 times loading background map for l4d.

	if ( ( nCalls % 200 ) == 0 )
	{
		static float flLastUpdateTime = -1.0f;
		float flTime = Plat_FloatTime();

		if ( flTime - flLastUpdateTime > .06f )
		{
			flLastUpdateTime = flTime;
			float t = ( ( float )nCalls ) * ( 1.0f / 1000000.0f );
			g_QueuedLoader.GetProgress()->UpdateProgress( PROGRESS_FILESORT + t * ( PROGRESS_IO - PROGRESS_FILESORT ) );
		}
	}

	if ( pFileJobLHS->m_Priority != pFileJobRHS->m_Priority )
	{
		// higher priorities sort to top
		return ( pFileJobLHS->m_Priority > pFileJobRHS->m_Priority );
	}

	if ( pFileJobLHS->m_hFilename == pFileJobRHS->m_hFilename )
	{
		// same file (zip), sort by offset
		return pFileJobLHS->m_nStartOffset < pFileJobRHS->m_nStartOffset;
	}

	char szFilenameLHS[MAX_PATH];
	char szFilenameRHS[MAX_PATH];
	g_QueuedLoader.GetFilename( pFileJobLHS->m_hFilename, szFilenameLHS, sizeof( szFilenameLHS ) );
	g_QueuedLoader.GetFilename( pFileJobRHS->m_hFilename, szFilenameRHS, sizeof( szFilenameRHS ) );

	// resolve filename to match disk layout of zips
	int layoutLHS = GetLayoutOrderForFilename( szFilenameLHS );
	int layoutRHS = GetLayoutOrderForFilename( szFilenameRHS );
	if ( layoutLHS != layoutRHS )
	{
		return layoutLHS < layoutRHS;
	}

	return CaselessStringLessThan( szFilenameLHS, szFilenameRHS );
}

//-----------------------------------------------------------------------------
// Dump the queue contents to the file system.
//-----------------------------------------------------------------------------
void CQueuedLoader::SubmitPendingJobs()
{
	// prevents contention between I/O and main thread attempting to submit
	if ( ThreadInMainThread() && g_nActiveJobs != 0 )
	{
		// main thread can only kick start work if the I/O is idle
		// once the I/O is kicked off, the I/O thread is responsible for continual draining
		return;
	}
	else if ( !ThreadInMainThread() && g_nActiveJobs != 1 )
	{
		// I/O thread requests more work, but will only fall through and get some when it expects to go idle
		// I/O thread still has jobs and doesn't need any more yet
		return;
	}

	CTSList<FileJob_t *>::Node_t *pNode = m_PendingJobs.Detach();
	if ( !pNode )
	{
		return;
	}

	// used by spew to indicate submission blocks
	m_nSubmitCount++;

	// sort entries
	CUtlSortVector< FileJob_t*, CFileJobsLessFunc > sortedFiles( 0, 128 );
	while ( pNode )
	{
		FileJob_t *pFileJob = pNode->elem;

		sortedFiles.InsertNoSort( pFileJob );

		CTSList<FileJob_t *>::Node_t *pNext = (CTSList<FileJob_t *>::Node_t*)pNode->Next;
		delete pNode;
		pNode = pNext;
	}
	sortedFiles.RedoSort();

	FileAsyncRequest_t asyncRequest;
	asyncRequest.pfnCallback = IOAsyncCallback;

	char szFilename[MAX_PATH];
	for ( int i = 0; i<sortedFiles.Count(); i++ )
	{
		FileJob_t *pFileJob = sortedFiles[i];
		
		pFileJob->m_SubmitTag = m_nSubmitCount;
		pFileJob->m_SubmitTime = Plat_MSTime();

		m_SubmittedJobs.AddToTail( pFileJob );

		// build an async request
		if ( pFileJob->m_Priority == LOADERPRIORITY_DURINGPRELOAD )
		{
			// must finish during preload
			asyncRequest.priority = PRIORITY_HIGH;
			g_nHighPriorityJobs++;
		}
		else if ( pFileJob->m_Priority == LOADERPRIORITY_BEFOREPLAY )
		{
			// must finish before gameplay
			asyncRequest.priority = PRIORITY_NORMAL;
			g_nJobsToFinishBeforePlay++;
		}
		else 
		{
			// can finish during gameplay, normal priority
			asyncRequest.priority = PRIORITY_NORMAL;
		}
		
		// async will allocate unless caller provided a target
		// loader always takes ownership of buffer
		asyncRequest.pData = pFileJob->m_pTargetData;
		asyncRequest.flags = pFileJob->m_pTargetData ? 0 : FSASYNC_FLAGS_ALLOCNOFREE;
		asyncRequest.nOffset = pFileJob->m_nStartOffset;
		asyncRequest.nBytes = pFileJob->m_nBytesToRead;
		asyncRequest.pszFilename = GetFilename( pFileJob->m_hFilename, szFilename, sizeof( szFilename ) );
		asyncRequest.pContext = (void *)pFileJob;

		while( g_SuspendIO )
		{
			ThreadSleep( MAIN_THREAD_YIELD_TIME );
		}

		if ( pFileJob->m_bFileExists )
		{
			// start the valid async request
			g_nActiveJobs++;
			g_pFullFileSystem->AsyncRead( asyncRequest, &pFileJob->m_hAsyncControl );
		}
		else
		{
			// prevent dragging the i/o system down for known failures
			// still need to do callback so subsystems can do the right thing based on file absence
			g_pThreadPool->QueueCall( IOComputationJob, pFileJob, pFileJob->m_pTargetData, 0, LOADERERROR_FILEOPEN )->Release();
		}
	}
}

//-----------------------------------------------------------------------------
// Add to queue
//-----------------------------------------------------------------------------
bool CQueuedLoader::AddJob( const LoaderJob_t *pLoaderJob )
{
	if ( !m_bActive )
	{
		return false;
	}

	Assert( pLoaderJob && pLoaderJob->m_pFilename );
	
	bool bLateQueued = false;
	if ( m_bCanBatch && !m_bBatching )
	{
		// should have been part of pre-load batch
		DevWarning( "QueuedLoader: Late Queued Job: %s\n", pLoaderJob->m_pFilename );
		bLateQueued = true;
	}

	// anonymous jobs lack callbacks and are heavily restricted to ensure their stability
	// the caller is expected to claim these before load ends (which auto-purges them)
	if ( !pLoaderJob->m_pCallback && pLoaderJob->m_Priority == LOADERPRIORITY_ANYTIME )
	{
		Assert( 0 );
		DevWarning( "QueuedLoader: Ignoring Anonymous Job: %s\n", pLoaderJob->m_pFilename );
		return false;	
	}

	FileNameHandle_t hFilename = m_Filenames.FindFileName( pLoaderJob->m_pFilename );
	if ( hFilename && m_ExcludeResourceNames.Find( hFilename ) != m_ExcludeResourceNames.InvalidIndex() )
	{
		// marked as excluded, ignoring
		return false;
	}

	MEM_ALLOC_CREDIT();

	// all bsp based files get forced to a higher priority in order to achieve a clustered sort
	// the bsp files are not going to be anywhere near the zips, thus we don't want head thrashing
	bool bFileIsFromBSP;
	bool bExists = false;

	char *pFullPath;
	char szFullPath[MAX_PATH];
	if ( V_IsAbsolutePath( pLoaderJob->m_pFilename ) )
	{
		// an absolute path is trusted, take as is
		pFullPath = (char *)pLoaderJob->m_pFilename;
		bFileIsFromBSP = V_stristr( pFullPath, ".bsp" ) != NULL;
		bExists = true;
	}
	else
	{
		// must resolve now, all submitted paths must be absolute for proper sort which achieves seek linearization
		// a resolved absolute file ensures its existence
		PathTypeFilter_t pathFilter = FILTER_NONE;
		if ( IsX360() )
		{
			if ( V_stristr( pLoaderJob->m_pFilename, ".bsp" ) || V_stristr( pLoaderJob->m_pFilename, ".ain" ) )
			{
				// only the bsp/ain are allowed to be external
				pathFilter = FILTER_CULLPACK;
			}
			else
			{
				// all files are expected to be in zip
				pathFilter = FILTER_CULLNONPACK;
			}
		}

		PathTypeQuery_t pathType;
		g_pFullFileSystem->RelativePathToFullPath( pLoaderJob->m_pFilename, pLoaderJob->m_pPathID, szFullPath, sizeof( szFullPath ), pathFilter, &pathType );
		bExists = V_IsAbsolutePath( szFullPath );
		pFullPath = szFullPath;
		bFileIsFromBSP = ( (pathType & PATH_IS_MAPPACKFILE) != 0 );
	}

	// create a file job
	FileJob_t *pFileJob = new FileJob_t;

	pFileJob->m_hFilename = m_Filenames.FindOrAddFileName( pFullPath );
	pFileJob->m_bFileExists = bExists;
	pFileJob->m_pCallback = pLoaderJob->m_pCallback;
	pFileJob->m_pContext = pLoaderJob->m_pContext;
	pFileJob->m_pContext2 = pLoaderJob->m_pContext2;
	pFileJob->m_pTargetData = pLoaderJob->m_pTargetData;
	pFileJob->m_nBytesToRead = pLoaderJob->m_nBytesToRead;
	pFileJob->m_nStartOffset = pLoaderJob->m_nStartOffset;
	pFileJob->m_Priority = bFileIsFromBSP ? LOADERPRIORITY_DURINGPRELOAD : pLoaderJob->m_Priority;
	pFileJob->m_bLateQueued = bLateQueued;
	pFileJob->m_bAnonymousDecode = pLoaderJob->m_bAnonymousDecode;

	if ( pLoaderJob->m_pTargetData )
	{
		// never free caller's buffer, if they provide, they have to free it
		pFileJob->m_bFreeTargetAfterIO = false;
	}
	else
	{
		// caller can take over ownership, otherwise it gets freed after I/O
		pFileJob->m_bFreeTargetAfterIO = ( pLoaderJob->m_bPersistTargetData == false );
	}

	if ( !pLoaderJob->m_pCallback )
	{
		// track anonymous jobs
		AUTO_LOCK_FM( m_Mutex );
		char szFixedName[MAX_PATH];
		V_strncpy( szFixedName, pLoaderJob->m_pFilename, sizeof( szFixedName ) );
		V_FixSlashes( szFixedName );
		m_AnonymousJobs.Insert( szFixedName, pFileJob );
	}

	g_nQueuedJobs++;

	if ( m_bBatching )
	{
		m_BatchedJobs.PushItem( pFileJob );
	}
	else
	{
		m_PendingJobs.PushItem( pFileJob );
		SubmitPendingJobs();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Allows an external system to append to a map's reslist. The next map load
// will append these specified files. Unhandled resources will just get
// quietly discarded.  An external system could use this to patch a hole
// or prevent a purge.
//-----------------------------------------------------------------------------
void CQueuedLoader::AddMapResource( const char *pFilename )
{
	if ( !pFilename || !pFilename[0] )
	{
		// pointless
		return;
	}
	
	// normalize the provided name as a filename
	char szFilename[MAX_PATH];
	V_strncpy( szFilename, pFilename, sizeof( szFilename ) );
	V_FixSlashes( szFilename );
	V_strlower( szFilename );

	if ( m_AdditionalResources.Find( szFilename ) != UTL_INVAL_SYMBOL )
	{
		// already added
		return;
	}

	m_AdditionalResources.AddString( szFilename );
}

//-----------------------------------------------------------------------------
// Asynchronous claim for an anonymous job.
// This allows loaders with deep dependencies to get their data in flight, and then claim it
// when the they are in a state to consume it.
//-----------------------------------------------------------------------------
bool CQueuedLoader::ClaimAnonymousJob( const char *pFilename, QueuedLoaderCallback_t pCallback, void *pContext, void *pContext2 )
{
	Assert( ThreadInMainThread() );
	Assert( pFilename && pCallback && !m_bBatching );

	char szFixedName[MAX_PATH];
	V_strncpy( szFixedName, pFilename, sizeof( szFixedName ) );
	V_FixSlashes( szFixedName );
	pFilename = szFixedName;

	int iIndex = m_AnonymousJobs.Find( pFilename );
	if ( iIndex == m_AnonymousJobs.InvalidIndex() )
	{
		// unknown
		DevWarning( "QueuedLoader: Anonymous Job '%s' not found\n", pFilename );
		return false;
	}

	// caller is claiming
	FileJob_t *pFileJob = m_AnonymousJobs[iIndex];
	if ( !pFileJob->m_bFinished )
	{
		// unfinished shouldn't happen and caller can't have it
		// anonymous jobs and their claims are very restrictive in such a way to provide stability
		// this dead job will get auto-cleaned at end of map loading
		Assert( 0 );
		return false;
	}

	m_AnonymousJobs.RemoveAt( iIndex );
	CJob *pClaimedAnonymousJob = g_pThreadPool->QueueCall( FinishAnonymousJob, pFileJob, pCallback, pContext, pContext2 );
	if ( IsPS3() )
	{
		// PS3 static prop lighting (legacy async IO still in flight catching
		// non reslist-lighting buffers) is writing data into raw pointers
		// to RSX memory which have been acquired before material system
		// switches to multithreaded mode. During switch to multithreaded
		// mode RSX moves its memory so pointers become invalid and thus
		// all IO must be finished and callbacks fired before
		// Host_AllowQueuedMaterialSystem
		pClaimedAnonymousJob->WaitForFinishAndRelease();
	}
	else
	{
		pClaimedAnonymousJob->Release();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Synchronous claim for an anonymous job. This allows loaders
// with deep dependencies to get their data in flight, and then claim it
// when the they are in a state to consume it.
//-----------------------------------------------------------------------------
bool CQueuedLoader::ClaimAnonymousJob( const char *pFilename, void **pData, int *pDataSize, LoaderError_t *pError )
{
	Assert( ThreadInMainThread() );
	Assert( pFilename && !m_bBatching );

	char szFixedName[MAX_PATH];
	V_strncpy( szFixedName, pFilename, sizeof( szFixedName ) );
	V_FixSlashes( szFixedName );
	pFilename = szFixedName;

	int iIndex = m_AnonymousJobs.Find( pFilename );
	if ( iIndex == m_AnonymousJobs.InvalidIndex() )
	{
		// unknown
		DevWarning( "QueuedLoader: Anonymous Job '%s' not found\n", pFilename );
		return false;
	}

	// caller is claiming
	FileJob_t *pFileJob = m_AnonymousJobs[iIndex];
	if ( !pFileJob->m_bFinished )
	{
		// unfinished shouldn't happen and caller can't have it
		// anonymous jobs and their claims are very restrictive in such a way to provide stability
		// this dead job will get auto-cleaned at end of map loading
		Assert( 0 );
		return false;
	}

	pFileJob->m_bClaimed = true;

	m_AnonymousJobs.RemoveAt( iIndex );

	*pData = pFileJob->m_pTargetData;
	*pDataSize = pFileJob->m_LoaderError == LOADERERROR_NONE ? pFileJob->m_nActualBytesRead : 0;
	if ( pError )
	{
		*pError = pFileJob->m_LoaderError;
	}
	
	// caller owns the data, regardless of how the job was setup
	pFileJob->m_pTargetData = NULL;

	// memory has been consumed
	g_nAnonymousIOMemory -= pFileJob->m_nActualBytesRead;

	return true;
}

//-----------------------------------------------------------------------------
// End of batching. High priority jobs are guaranteed completed before function returns.
//-----------------------------------------------------------------------------
void CQueuedLoader::SubmitBatchedJobsAndWait()
{
	// end of batching
	m_bBatching = false;

	CTSList<FileJob_t *>::Node_t *pNode = m_BatchedJobs.Detach();
	if ( !pNode )
	{
		return;
	}

	// must wait for any initial i/o to finish
	// i/o thread must stop in order to submit all the batched jobs atomically
	// and get an accurate accounting of high priority jobs
	while ( g_nActiveJobs != 0 )
	{
		g_pThreadPool->Yield( MAIN_THREAD_YIELD_TIME );
	}

	// dump batched jobs to pending jobs
	while ( pNode )
	{
		FileJob_t *pFileJob = pNode->elem;

		m_PendingJobs.PushItem( pFileJob );

		CTSList<FileJob_t *>::Node_t *pNext = (CTSList<FileJob_t *>::Node_t*)pNode->Next;
		delete pNode;
		pNode = pNext;
	}

	SubmitPendingJobs();

	if ( GetSpewDetail() )
	{
		Msg( "QueuedLoader: High Priority Jobs: %d\n", ( int ) g_nHighPriorityJobs );
	}
	
	if ( loader_defer_non_critical_jobs.GetBool() )
	{
		// allow to trigger at next available oppportunity
		g_nForceSuspendIO = -1;
	}

	// finish only the high priority jobs
	// high priority jobs are expected to be complete at the conclusion of batching
	int total = g_nHighPriorityJobs;
	while ( g_nHighPriorityJobs != 0 )
	{
		float t = (float)( total - g_nHighPriorityJobs ) / (float)total;
		m_pProgress->UpdateProgress( PROGRESS_IO + t * ( PROGRESS_END - PROGRESS_IO ) );

		// yield some time
		g_pThreadPool->Yield( MAIN_THREAD_YIELD_TIME );
	}
}

//-----------------------------------------------------------------------------
// Clean queue of stale entries. Active entries are skipped.
//-----------------------------------------------------------------------------
void CQueuedLoader::CleanQueue()
{
	for ( int i = 0; i<RESOURCEPRELOAD_COUNT; i++ )
	{
		m_ResourceNames[i].Purge();
	}

	m_ExcludeResourceNames.Purge();
	m_BatchedJobs.Purge();

	int iIndex = m_SubmittedJobs.Head();
	while ( iIndex != m_SubmittedJobs.InvalidIndex() )
	{
		int iNext = m_SubmittedJobs.Next( iIndex );

		FileJob_t *pFileJob = m_SubmittedJobs[iIndex];	
		if ( pFileJob->m_bFinished )
		{
			// job is complete, safe to free
			m_SubmittedJobs.Free( iIndex );
			g_pFullFileSystem->AsyncRelease( pFileJob->m_hAsyncControl );
			delete pFileJob;
		}
		iIndex = iNext;
	}

	m_Filenames.RemoveAll();
}

//-----------------------------------------------------------------------------
// Abandon queue
//-----------------------------------------------------------------------------
void CQueuedLoader::PurgeQueue()
{
}

//-----------------------------------------------------------------------------
// Spew info abut queued load
//-----------------------------------------------------------------------------
void CQueuedLoader::SpewInfo()
{
	Msg( "Queued Loader:\n\n" );

	int totalClaimed = 0;
	int totalUnclaimed = 0;

	if ( IsFinished() )
	{
		// can only access submitted jobs safely when io thread complete
		int lastPriority = -1;
		int lastLateState = -1;
		int iIndex = m_SubmittedJobs.Head();
		while ( iIndex != m_SubmittedJobs.InvalidIndex() )
		{
			FileJob_t *pFileJob = m_SubmittedJobs[iIndex];	

			int asyncDuration = -1;
			if ( pFileJob->m_FinishTime )
			{
				asyncDuration = pFileJob->m_FinishTime - pFileJob->m_SubmitTime;
			}
			
			if ( pFileJob->m_bLateQueued != lastLateState )
			{
				if ( pFileJob->m_bLateQueued )
				{
					Warning( "---- LATE QUEUED JOBS ----\n" );
				}
				lastLateState = pFileJob->m_bLateQueued;
			}
			
			if ( !pFileJob->m_bLateQueued && pFileJob->m_Priority != lastPriority )
			{
				switch ( pFileJob->m_Priority )
				{
				case LOADERPRIORITY_DURINGPRELOAD:
					Msg( "---- FINISH DURING PRELOAD ( HIGH PRIORITY )----\n" );
					break;
				case LOADERPRIORITY_BEFOREPLAY:
					Msg( "---- FINISH BEFORE GAMEPLAY ( NORMAL PRIORITY )----\n" );
					break;
				case LOADERPRIORITY_ANYTIME:
					Msg( "---- FINISH ANYTIME ( NORMAL PRIORITY )----\n" );
					break;
				}
				lastPriority = pFileJob->m_Priority;
			}

			char szAnonymousString[MAX_PATH];
			const char *pAnonymousStatus = "";
			if ( !pFileJob->m_pCallback )
			{
				V_snprintf( szAnonymousString, sizeof( szAnonymousString ), "(%s) ", pFileJob->m_bClaimed ? "Claimed" : "Unclaimed" );
				pAnonymousStatus = szAnonymousString;
			
				if ( pFileJob->m_bClaimed )
				{
					totalClaimed += pFileJob->m_nActualBytesRead;
				}
				else
				{
					totalUnclaimed += pFileJob->m_nActualBytesRead;
				}
			}

			char szFilename[MAX_PATH];
			char szMessage[1024];
			V_snprintf( 
				szMessage, 
				sizeof( szMessage ), 
				"Submit:%5dms AsyncDuration:%5dms Tag:%d Thread:%8.8x Size:%7d %s%s", 
				pFileJob->m_SubmitTime - m_StartTime, 
				asyncDuration, 
				pFileJob->m_SubmitTag,
				pFileJob->m_ThreadId,
				pFileJob->m_nActualBytesRead,
				pAnonymousStatus,
				GetFilename( pFileJob->m_hFilename, szFilename, sizeof( szFilename ) ) );

			if ( pFileJob->m_bLateQueued )
			{
				Warning( "%s\n", szMessage );
			}
			else
			{
				Msg( "%s\n", szMessage );
			}

			iIndex = m_SubmittedJobs.Next( iIndex );
		}

		Msg( "%d Total Jobs\n", m_SubmittedJobs.Count() );
	}

	Msg( "\nExcludes:\n" );
	for ( int i = 0; i < m_ExcludeResourceNames.Count(); i++ )
	{
		char szFilename[MAX_PATH] = "";
		GetFilename( m_ExcludeResourceNames[i], szFilename, sizeof( szFilename ) );
		Msg( "%s\n", szFilename );
	}

	Msg( "\nLayout Order:\n" );
	for ( int i = 0; i < g_DVDLayout.Count(); i++ )
	{
		Msg( "%s\n", g_DVDLayout[i].String() );
	}

	Msg( "\n" );
	Msg( "%d Queued Jobs\n", ( int ) g_nQueuedJobs );
	Msg( "%d Active Jobs\n", ( int ) g_nActiveJobs );
	Msg( "Peak IO Memory: %.2f MB\n", (float)g_nIOMemoryPeak / ( 1024.0f * 1024.0f ) );
	Msg( "Peak Anonymous IO Memory: %.2f MB\n", (float)g_nAnonymousIOMemoryPeak / ( 1024.0f * 1024.0f ) );
	Msg( "  Total Anonymous Claimed: %d\n", totalClaimed );
	Msg( "  Total Anonymous Unclaimed: %d\n", totalUnclaimed );
	if ( m_EndTime )
	{
		Msg( "Queuing Duration: %dms\n", m_EndTime - m_StartTime );
	}
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
InitReturnVal_t CQueuedLoader::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

	return INIT_OK; 
}

//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CQueuedLoader::Shutdown()
{
	BaseClass::Shutdown();
}

//-----------------------------------------------------------------------------
// Install a type specific interface from managing system.
//-----------------------------------------------------------------------------
void CQueuedLoader::InstallLoader( ResourcePreload_t type, IResourcePreload *pLoader )
{
	m_pLoaders[type] = pLoader;
}

void CQueuedLoader::InstallProgress( ILoaderProgress *pProgress )
{
	m_pProgress = pProgress;
}

//-----------------------------------------------------------------------------
// Invoke the loader systems to purge dead resources
//-----------------------------------------------------------------------------
void CQueuedLoader::PurgeUnreferencedResources()
{
	ResourcePreload_t purgeOrder[RESOURCEPRELOAD_COUNT];
	
	// the purge operations require a specific order (models and cubemaps before materials)
	int numPurges = 0;
	purgeOrder[numPurges++] = RESOURCEPRELOAD_SOUND;
	purgeOrder[numPurges++] = RESOURCEPRELOAD_STATICPROPLIGHTING;
	purgeOrder[numPurges++] = RESOURCEPRELOAD_MODEL;
	purgeOrder[numPurges++] = RESOURCEPRELOAD_CUBEMAP;
	purgeOrder[numPurges++] = RESOURCEPRELOAD_MATERIAL;
	// 'gpubufferallocator' must be LAST so that it can reclaim released physical memory
	purgeOrder[numPurges++] = RESOURCEPRELOAD_GPUBUFFERALLOCATOR;
	
	// iterate according to order
	for ( int i = 0; i < numPurges; i++ )
	{
		ResourcePreload_t loader = purgeOrder[i];
		if ( m_pLoaders[loader] )
		{
			m_pLoaders[loader]->PurgeUnreferencedResources();
		}
	}

	m_pProgress->UpdateProgress( PROGRESS_PREPURGE );
}


//-----------------------------------------------------------------------------
// Invoke the loader systems to purge all resources, if possible.
// NOT meant to be used when transitioning to the same map.
// This trips expensive resource eviction.
//-----------------------------------------------------------------------------
void CQueuedLoader::PurgeAll( ResourcePreload_t *pDontPurgeList, int nPurgeListSize ) 
{
	ResourcePreload_t purgeOrder[RESOURCEPRELOAD_COUNT];

	// default purge all
	unsigned int bPurgeMask = 0xFFFFFFFF;

	// caller can narrow purge and inhibit specific resources
	if ( pDontPurgeList && nPurgeListSize > 0 )
	{
		for ( int i = 0; i < nPurgeListSize; i++ )
		{
			bPurgeMask &= ~( 1 << pDontPurgeList[i] );
		}
	}

	// the purge operations require a specific order (models and cubemaps before materials)
	int numPurges = 0;
	if ( bPurgeMask & ( 1 << RESOURCEPRELOAD_SOUND ) )
	{
		purgeOrder[numPurges++] = RESOURCEPRELOAD_SOUND;
	}
	if ( bPurgeMask & ( 1 << RESOURCEPRELOAD_STATICPROPLIGHTING ) )
	{
		purgeOrder[numPurges++] = RESOURCEPRELOAD_STATICPROPLIGHTING;
	}
	if ( bPurgeMask & ( 1 << RESOURCEPRELOAD_MODEL ) )
	{
		purgeOrder[numPurges++] = RESOURCEPRELOAD_MODEL;
	}
	if ( bPurgeMask & ( 1 << RESOURCEPRELOAD_CUBEMAP ) )
	{
		purgeOrder[numPurges++] = RESOURCEPRELOAD_CUBEMAP;
	}
	if ( bPurgeMask & ( 1 << RESOURCEPRELOAD_MATERIAL ) )
	{
		purgeOrder[numPurges++] = RESOURCEPRELOAD_MATERIAL;
	}
	// 'gpubufferallocator' must be LAST so that it can reclaim released physical memory
	if ( bPurgeMask & ( 1 << RESOURCEPRELOAD_GPUBUFFERALLOCATOR ) )
	{
		purgeOrder[numPurges++] = RESOURCEPRELOAD_GPUBUFFERALLOCATOR;
	}

	// iterate according to order
	for ( int i = 0; i < numPurges; i++ )
	{
		ResourcePreload_t loader = purgeOrder[i];
		if ( m_pLoaders[loader] )
		{
			m_pLoaders[loader]->PurgeAll();
		}
	}

	// any purge means that we can no longer supply the "same map" loading hint
	// hopefully the caller does not invoke a PurgeAll() when transitioning to the same map
	*m_szMapNameToCompareSame = '\0';
}


//-----------------------------------------------------------------------------
// Invoke the loader systems to request i/o jobs, which are batched.
//-----------------------------------------------------------------------------
void CQueuedLoader::GetJobRequests()
{
	COM_TimestampedLog( "CQueuedLoader::GetJobRequests - Start" );

	// causes the batch queue to fill with i/o requests
	m_bCanBatch = true;
	m_bBatching = true;

	float t0, flLastUpdateT;
	t0 = Plat_FloatTime();
	CJob *jobs[5];

	// cubemap textures must be first to install correctly before their cubemap materials are built (and precache the cubmeap textures)
	// cannot be overlapped, must run serially
	BuildResources( m_pLoaders[RESOURCEPRELOAD_CUBEMAP], &m_ResourceNames[RESOURCEPRELOAD_CUBEMAP], &m_LoaderTimes[RESOURCEPRELOAD_CUBEMAP] );	

	// Overlapping these is not critical in any way, total time is currently < 2 seconds.
	// These operations flood calls (AddJob) back into the queued loader (which has to mutex its lists),
	// so in fact it's slightly slower to queue these at this stage. As these routines age they may become more heavyweight.
	jobs[0] = g_pThreadPool->QueueCall( BuildResources, m_pLoaders[RESOURCEPRELOAD_SOUND], &m_ResourceNames[RESOURCEPRELOAD_SOUND], &m_LoaderTimes[RESOURCEPRELOAD_SOUND] );	
	jobs[1] = g_pThreadPool->QueueCall( BuildMaterialResources, m_pLoaders[RESOURCEPRELOAD_MATERIAL], &m_ResourceNames[RESOURCEPRELOAD_MATERIAL], &m_LoaderTimes[RESOURCEPRELOAD_MATERIAL] );
	jobs[2] = g_pThreadPool->QueueCall( BuildResources, m_pLoaders[RESOURCEPRELOAD_STATICPROPLIGHTING], &m_ResourceNames[RESOURCEPRELOAD_STATICPROPLIGHTING], &m_LoaderTimes[RESOURCEPRELOAD_STATICPROPLIGHTING] );	
	jobs[3] = g_pThreadPool->QueueCall( BuildResources, m_pLoaders[RESOURCEPRELOAD_MODEL], &m_ResourceNames[RESOURCEPRELOAD_MODEL], &m_LoaderTimes[RESOURCEPRELOAD_MODEL] );	
	jobs[4] = g_pThreadPool->QueueCall( BuildResources, m_pLoaders[RESOURCEPRELOAD_ANONYMOUS], &m_ResourceNames[RESOURCEPRELOAD_ANONYMOUS], &m_LoaderTimes[RESOURCEPRELOAD_ANONYMOUS] );

	// all jobs must finish
	flLastUpdateT = -1000.0f;
	// Update as if this takes 2 seconds
	float flDelta = ( PROGRESS_CREATEDRESOURCES - PROGRESS_PARSEDRESLIST ) * 0.03 / 2.0f;
	float flProgress = PROGRESS_PARSEDRESLIST;
	while( true )
	{
		bool bIsDone = true;
		for ( int i=0; i<ARRAYSIZE( jobs ); i++ )
		{
			if ( !jobs[i]->IsFinished() )
			{
				bIsDone = false;
				break;
			}
		}
		if ( bIsDone )
			break;

		// Can't sleep; that will allow this thread to be used by the thread pool
		float newt = Plat_FloatTime();
		if ( newt - flLastUpdateT > .03 )
		{
#if defined( _PS3 )
			AUTO_LOCK_FM( m_sRendererMutex );
			CInterlockedIntAutoIncrementer autoIncrementRendererMutex( m_nRendererMutexProgressBarUnlockCount );
#endif
			m_pProgress->UpdateProgress( flProgress );
			flProgress = clamp( flProgress + flDelta, PROGRESS_PARSEDRESLIST, PROGRESS_CREATEDRESOURCES );

			// Necessary to take into account any waits for vsync
			flLastUpdateT = Plat_FloatTime();
		}
	}

	for ( int i=0; i<ARRAYSIZE( jobs ); i++ )
	{
		jobs[i]->Release();
	}

	if ( g_QueuedLoader.GetSpewDetail() & LOADER_DETAIL_TIMING )
	{
		for ( int i = RESOURCEPRELOAD_UNKNOWN+1; i<RESOURCEPRELOAD_COUNT; i++ )
		{	
			Msg( "QueuedLoader: %s Creating: %.2f seconds\n", g_ResourceLoaderNames[i], m_LoaderTimes[i] );
		}
		Msg( "QueuedLoader: Total Creating: %.2f seconds\n", Plat_FloatTime() - t0 );
	}

	m_pProgress->UpdateProgress( PROGRESS_CREATEDRESOURCES );

	COM_TimestampedLog( "CQueuedLoader::GetJobRequests - End" );
}

void CQueuedLoader::AddResourceToTable( const char *pFilename )
{
	const char *pExt = V_GetFileExtension( pFilename );
	if ( !pExt )
	{
		// unknown
		// all resources are identified by their extension
		return;
	}

	if ( pFilename[0] == '*' )
	{
		// this is a job exclusion, add to exclusion list
		FileNameHandle_t hFilename = m_Filenames.FindOrAddFileName( pFilename + 1 );
		m_ExcludeResourceNames.InsertNoSort( hFilename );
		return;
	}

	const char *pTypeDir = NULL;
	const char *pName = pFilename;
	ResourcePreload_t type = RESOURCEPRELOAD_UNKNOWN;

	if ( !V_stricmp( pExt, "wav" ) )
	{
		type = RESOURCEPRELOAD_SOUND;
		pTypeDir = "sound\\";
	}
	else if ( !V_stricmp( pExt, "vmt" ) )
	{
		type = RESOURCEPRELOAD_MATERIAL;
		pTypeDir = "materials\\";
	}
	else if ( !V_stricmp( pExt, "vtf" ) )
	{
		if ( V_stristr( pFilename, "maps\\" ) )
		{
			// only want cubemap textures
			if ( !m_bLoadForHDR && V_stristr( pFilename, ".hdr." ) )
			{
				return;
			}
			else if ( m_bLoadForHDR && !V_stristr( pFilename, ".hdr." ) )
			{
				return;
			}
			type = RESOURCEPRELOAD_CUBEMAP;
			pTypeDir = "materials\\";
		}
		else
		{
			return;
		}
	}
	else if ( !V_stricmp( pExt, "mdl" ) )
	{
		type = RESOURCEPRELOAD_MODEL;
		pTypeDir = "models\\";	
	}
	else if ( !V_stricmp( pExt, "vhv" ) )
	{
		// want static props only
		pName = V_stristr( pFilename, "sp_" );
		if ( !pName )
		{
			return;
		}

		if ( !m_bLoadForHDR && V_stristr( pFilename, "_hdr_" ) )
		{
			return;
		}
		else if ( m_bLoadForHDR && !V_stristr( pFilename, "_hdr_" ) )
		{
			return;
		}
		type = RESOURCEPRELOAD_STATICPROPLIGHTING;
	}
	else
	{
		// unknown, ignored
		return;
	}

	if ( pTypeDir )
	{
		// want object name only
		// skip past game/type directory prefixing
		const char *pDir = V_stristr( pName, pTypeDir );
		if ( pDir )
		{
			pName = pDir + strlen( pTypeDir );
		}
	}

	FileNameHandle_t hFilename = m_Filenames.FindOrAddFileName( pName );
	m_ResourceNames[type].InsertNoSort( hFilename );
}


//-----------------------------------------------------------------------------
// Parse the raw resource list into resource dictionaries
//-----------------------------------------------------------------------------
void CQueuedLoader::ParseResourceList( CUtlBuffer &resourceList )
{
	// parse resource list into known types
	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );
	char szToken[MAX_PATH];
	int nCount = 0;
	for ( ;; )
	{
		int nTokenSize = resourceList.ParseToken( &breakSet, szToken, sizeof( szToken ) );
		if ( nTokenSize <= 0 )
		{
			break;
		}
		AddResourceToTable( szToken );
		nCount++;
		// Keep the spinner going!
		if ( ( nCount % 1200 ) == 0 )
		{
			m_pProgress->UpdateProgress( PROGRESS_GOTRESLIST );
		}
	}

	// add any additional resources
	// duplicates don't need to be culled, loaders are supposed to handle resources that already exist
	for ( int i = 0; i < m_AdditionalResources.GetNumStrings(); i++ )
	{
		if ( g_QueuedLoader.GetSpewDetail() )
		{
			Msg( "QueuedLoader: Appending: %s\n", m_AdditionalResources.String( i ) );
		}
		AddResourceToTable( m_AdditionalResources.String( i ) );
	}

	// all done with adds
	m_ExcludeResourceNames.RedoSort();

	if ( g_QueuedLoader.GetSpewDetail() )
	{
		for ( int i = RESOURCEPRELOAD_UNKNOWN+1; i < RESOURCEPRELOAD_COUNT; i++ )
		{
			Msg( "QueuedLoader: %s: %d Entries\n", g_ResourceLoaderNames[i], m_ResourceNames[i].Count() );
		}

		Msg( "QueuedLoader: Excludes: %d Entries\n", m_ExcludeResourceNames.Count() );
	}

	m_pProgress->UpdateProgress( PROGRESS_PARSEDRESLIST );
}

//-----------------------------------------------------------------------------
// Mark the start of the queued loading process.
//-----------------------------------------------------------------------------
bool CQueuedLoader::BeginMapLoading( const char *pMapName, bool bLoadForHDR, bool bOptimizeMapReload, void (*pfnBeginMapLoadingCallback)( int nStage ) )
{
	if ( IsPC() )
	{
		return false;
	}

	if ( CommandLine()->FindParm( "-noqueuedload" ) ||  
		CommandLine()->FindParm( "-noqueuedloader" ) )
	{
		return false;
	}

	if ( m_bStarted )
	{
		// already started, shouldn't be started more than once
		Assert( 0 );
		return true;
	}

	COM_TimestampedLog( "CQueuedLoader::BeginMapLoading" );

	GetDVDLayout();

	// set the IO throttle markers based on available memory
	// these safety watermarks throttle the i/o from flooding memory, when the cores cannot keep up
	// the delta must be larger than any single operation, otherwise deadlock
	// markers that are too close will cause excessive suspension
	if ( loader_throttle_io.GetInt() )
	{
		size_t usedMemory, freeMemory;
		g_pMemAlloc->GlobalMemoryStatus( &usedMemory, &freeMemory );
		if ( freeMemory >= 64*1024*1024 )
		{
			// lots of available memory, can afford to have let the i/o get ahead
			g_nHighIOSuspensionMark = 10*1024*1024;
			g_nLowIOSuspensionMark = 2*1024*1024;
		}
		else
		{
			// low memory, suspend the i/o more frequently 
			g_nHighIOSuspensionMark = 5*1024*1024;
			g_nLowIOSuspensionMark = 1*1024*1024;
		}
	}
	else
	{
		// no IO throttling
		g_nHighIOSuspensionMark = 512*1024*1024;
		g_nLowIOSuspensionMark = 512*1024*1024;
	}

	// default disabled
	// can force an artificial i/o suspension, for debugging only
	g_nForceSuspendIO = 0;

	if ( GetSpewDetail() )
	{
		Msg( "QueuedLoader: Suspend I/O at [%.2f,%.2f] MB\n", (float)g_nLowIOSuspensionMark/(1024.0f*1024.0f), (float)g_nHighIOSuspensionMark/(1024.0f*1024.0f) );
	}

	m_bStarted = true;
	m_bLoadForHDR = bLoadForHDR;

	// map pak will be accessed asynchronously throughout loading and into game frame
	g_pFullFileSystem->BeginMapAccess();

	// remove any prior stale entries
	CleanQueue();
	Assert( m_SubmittedJobs.Count() == 0 && g_nActiveJobs == 0 && g_nQueuedJobs == 0 );

	m_bActive = true;
	m_nSubmitCount = 0;
	m_StartTime = Plat_MSTime();
	m_EndTime = 0;
	m_bCanBatch = false;
	m_bBatching = false;
	m_bDoProgress = false;

	g_nIOMemory = 0;
	g_nAnonymousIOMemory = 0;
	g_nIOMemoryPeak = 0;
	g_nAnonymousIOMemoryPeak = 0;

	m_bSameMap = bOptimizeMapReload && ( V_stricmp( pMapName, m_szMapNameToCompareSame ) == 0 );
	if ( m_bSameMap )
	{
		// Data will persist (so reloading a map is fast)
	}
	else
	{
		// Full load of the new map's data
		V_strncpy( m_szMapNameToCompareSame, pMapName, sizeof( m_szMapNameToCompareSame ) );
	}	

	m_pProgress->BeginProgress();
	m_pProgress->UpdateProgress( PROGRESS_START );

	// load this map's resource list before any other i/o
	char szBaseName[MAX_PATH];
	char szFilename[MAX_PATH];
	V_FileBase( pMapName, szBaseName, sizeof( szBaseName ) );
	V_snprintf( szFilename, sizeof( szFilename ), "reslists_xbox/%s%s.lst", szBaseName, GetPlatformExt() );

	CUtlBuffer resListBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( szFilename, "GAME", resListBuffer, 0, 0 ) )
	{
		// very bad, a valid reslist is critical
		DevWarning( "QueuedLoader: Failed to get reslist '%s', Non-Optimal Loading.\n", szFilename );
		if ( IsGameConsole() )
		{
			// We can't ship with this; it's fatal.
			Msg( "\n\n\n########\n######## BAD IMAGE: cannot load map reslist (%s)!\n########\n\n\n\n", szFilename );
			DebuggerBreakIfDebugging();
		}
		m_bActive = false;
		return false;
	}

	if ( XBX_IsLocalized() )
	{
		// find optional localized reslist fixup
		V_snprintf( szFilename, sizeof( szFilename ), "reslists_xbox/%s%s.lst", XBX_GetLanguageString(), GetPlatformExt() );
		CUtlBuffer localizedBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( g_pFullFileSystem->ReadFile( szFilename, "GAME", localizedBuffer, 0, 0 ) )
		{
			// append it
			resListBuffer.EnsureCapacity( resListBuffer.TellPut() + localizedBuffer.TellPut() );
			resListBuffer.Put( localizedBuffer.PeekGet(), localizedBuffer.TellPut() );
		}
	}

	m_pProgress->UpdateProgress( PROGRESS_GOTRESLIST );

	// due to its size, the bsp load is a lengthy i/o operation
	// this causes a non-batched async i/o operation to commence immediately
	m_pLoaders[RESOURCEPRELOAD_MODEL]->PrepareForCreate( m_bSameMap );
	if ( !m_pLoaders[RESOURCEPRELOAD_MODEL]->CreateResource( pMapName ) )
	{
		// very bad, a valid bsp is critical
		DevWarning( "QueuedLoader: Failed to mount BSP '%s', Non-Optimal Loading.\n", pMapName );
		m_bActive = false;
		return false;
	}

	// parse the raw resource list into loader specific dictionaries
	ParseResourceList( resListBuffer );
		
	// run the distributed precache loaders, generating a batch of i/o requests
	GetJobRequests();

	// event each loader to discard dead resources
	PurgeUnreferencedResources();

	if ( pfnBeginMapLoadingCallback )
	{
		(*pfnBeginMapLoadingCallback)( 1 );
	}

	// sort and start async fulfilling the i/o requests
	// waits for all "must complete" jobs to finish
	SubmitBatchedJobsAndWait();

	// progress is only relevant during preload
	// normal load process takes over any progress bar
	// disable progress tracking to prevent any late queued operation from updating
	m_pProgress->EndProgress();

	return m_bActive;
}

//-----------------------------------------------------------------------------
// Signal the end of the queued loading process, i/o will still be in progress.
//-----------------------------------------------------------------------------
void CQueuedLoader::EndMapLoading( bool bAbort )
{
	if ( !m_bStarted )
	{
		// already stopped or never started
		return;
	}

	/////////////////////////////////////////////////////
	// TBD: Cannot abort!!!! feature has not been done //
	/////////////////////////////////////////////////////
	bAbort = false;

	bool bShutdownAppRequest = false;
#ifdef _PS3
	bShutdownAppRequest = GetTLSGlobals()->bNormalQuitRequested;
#endif

	// To satisfy system shutdown request, purge all pending jobs
	// that haven't been submitted yet
	if ( bShutdownAppRequest )
	{
		m_PendingJobs.Purge();
	}

	if ( m_bActive )
	{
		if ( bAbort )
		{
			PurgeQueue();
		}
		else
		{
			if ( GetSpewDetail() )
			{
				// Under normal HDD circumstances this would be 0, the HDD can deliver fast enough
				// that the jobs are done before the legacy load finishes. Under DVD maybe a few.
				// When the debugging loader_defer_non_critical_jobs convar is active this will
				// be a substantial portion due to it deferring all those jobs to now, to stress test
				// this final phase.
				Msg( "QueuedLoader: Finishing %d Jobs\n", (int)g_nJobsToFinishBeforePlay );
			}

			// We have unexplained GPU hangs, all of them die inside the CLoaderMemAlloc
			// as we are trying to drive the loading progres below.
			m_pProgress->PauseNonInteractiveProgress( true );

			// clear any possible suspension and prevent any possible suspension from occurring
			g_nForceSuspendIO = 0;
			AdjustAsyncIOSpeed();

			// finish all outstanding priority jobs
			SubmitPendingJobs();

			// Upon shutdown we need to abort all jobs that have already been
			// submitted since they can take a very long time to finish
			if ( bShutdownAppRequest )
			{
				// we need to flush IO before aborting thread pool, because IO jobs are constantly spawning compute jobs in g_pThreadPool
				// in case we quit during loading a level
				g_pFullFileSystem->AsyncFlush();

				g_pThreadPool->AbortAll();
				g_pFullFileSystem->AsyncFlush();
				g_pFullFileSystem->RemoveAllSearchPaths();
				m_pProgress->UpdateProgress( PROGRESS_END, true );
			}
			else
			{
				while ( g_nHighPriorityJobs != 0 || g_nJobsToFinishBeforePlay != 0 )
				{
					// yield some time
					g_pThreadPool->Yield( MAIN_THREAD_YIELD_TIME );
					m_pProgress->UpdateProgress( PROGRESS_END, true );
				}
			}

			// Restore
			m_pProgress->PauseNonInteractiveProgress( false );
		}

		m_EndTime = Plat_MSTime();
		m_bActive = false;

		// transmit the end map event
		for ( int i = RESOURCEPRELOAD_UNKNOWN+1; i < RESOURCEPRELOAD_COUNT; i++ )
		{
			if ( m_pLoaders[i] )
			{
				m_pLoaders[i]->OnEndMapLoading( bAbort );
			}
		}

		// free any unclaimed anonymous buffers
		int iIndex = m_AnonymousJobs.First();
		while ( iIndex != m_AnonymousJobs.InvalidIndex() )
		{
			FileJob_t *pFileJob = m_AnonymousJobs[iIndex];	
			if ( pFileJob->m_bFreeTargetAfterIO && pFileJob->m_pTargetData )
			{
				g_pFullFileSystem->FreeOptimalReadBuffer( pFileJob->m_pTargetData );
				pFileJob->m_pTargetData = NULL;
			}
			g_nAnonymousIOMemory -= pFileJob->m_nActualBytesRead;
			iIndex = m_AnonymousJobs.Next( iIndex );
		}
		m_AnonymousJobs.Purge();

		if ( g_nIOMemory || g_nAnonymousIOMemory )
		{
			// expected to be zero, otherwise logic flaw
			DevWarning( "QueuedLoader: Unclaimed I/O memory: total:%d anonymous:%d\n", ( int ) g_nIOMemory, ( int ) g_nAnonymousIOMemory );
			g_nIOMemory = 0;
			g_nAnonymousIOMemory = 0;
		}

		// no longer needed
		m_AdditionalResources.RemoveAll();
	}

	g_pFullFileSystem->EndMapAccess();
	m_bStarted = false;
}

//-----------------------------------------------------------------------------
// Returns true if loader is accepting queue requests.
//-----------------------------------------------------------------------------
bool CQueuedLoader::IsMapLoading() const
{
	return m_bActive;
}

//-----------------------------------------------------------------------------
// Returns true if loader is working on same map as last load
//-----------------------------------------------------------------------------
bool CQueuedLoader::IsSameMapLoading() const
{
	return m_bActive && m_bSameMap;
}

//-----------------------------------------------------------------------------
// Returns true if the loader is idle, indicates all i/o and work has completed.
//-----------------------------------------------------------------------------
bool CQueuedLoader::IsFinished() const
{
	return ( m_bActive == false && g_nActiveJobs == 0 && g_nQueuedJobs == 0 );
}

//-----------------------------------------------------------------------------
// Returns true if loader is batching
//-----------------------------------------------------------------------------
bool CQueuedLoader::IsBatching() const
{
	return m_bBatching;
}

int CQueuedLoader::GetSpewDetail() const
{
	int spewDetail = loader_spew_info.GetInt();
	if ( spewDetail <= 0 )
	{
		return spewDetail;
	}

	return 1 << ( spewDetail - 1 );
}

unsigned int CQueuedLoader::GetStartTime()
{
	return m_StartTime;
}

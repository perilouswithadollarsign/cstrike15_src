//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef QUEUEDLOADER_H
#define QUEUEDLOADER_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "appframework/iappsystem.h"

enum LoaderError_t
{
	LOADERERROR_NONE     = 0,
	LOADERERROR_FILEOPEN = -1,
	LOADERERROR_READING  = -2,
};

enum LoaderPriority_t
{
	LOADERPRIORITY_ANYTIME    = 0,		// low priority, job can finish during gameplay
	LOADERPRIORITY_BEFOREPLAY = 1,		// job must complete before load ends
	LOADERPRIORITY_DURINGPRELOAD = 2,	// job must be complete during preload phase
};

typedef void ( *QueuedLoaderCallback_t )( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError );

struct LoaderJob_t
{
	LoaderJob_t()
	{
		memset( this, 0, sizeof( *this ) );
	}

	const char				*m_pFilename;				// path to resource
	const char				*m_pPathID;					// optional, can be NULL
	QueuedLoaderCallback_t	m_pCallback;				// called at i/o delivery
	void					*m_pContext;				// caller provided data
	void					*m_pContext2;				// caller provided data
	void					*m_pTargetData;				// optional, caller provided target buffer
	int						m_nBytesToRead;				// optional read clamp, otherwise 0
	unsigned int			m_nStartOffset;				// optional start offset, otherwise 0
	LoaderPriority_t		m_Priority;					// data must arrive by specified interval
	bool					m_bPersistTargetData;		// caller wants ownership of i/o buffer
	bool					m_bAnonymousDecode;			// anonymous job wants a lzma decode
};

enum ResourcePreload_t
{
	RESOURCEPRELOAD_UNKNOWN,
	RESOURCEPRELOAD_SOUND,
	RESOURCEPRELOAD_MATERIAL,
	RESOURCEPRELOAD_MODEL,
	RESOURCEPRELOAD_CUBEMAP,
	RESOURCEPRELOAD_STATICPROPLIGHTING,
	RESOURCEPRELOAD_GPUBUFFERALLOCATOR,
	RESOURCEPRELOAD_ANONYMOUS,
	RESOURCEPRELOAD_COUNT
};

abstract_class IResourcePreload
{
public:
	// Sent as a warning shot. Some callers may choose to do pre-emptive purging.
	virtual void PrepareForCreate( bool bSameMap ) = 0;

	// Called during preload phase for ALL the resources expected by the level.
	// Caller should not do i/o but generate AddJob() requests. Resources that already exist
	// and are not referenced by this function would be candidates for purge.
	virtual bool CreateResource( const char *pName ) = 0;

	// Sent as an event hint during preload, that creation has completed, AddJob() i/o is about to commence.
	// Caller should purge any unreferenced resources before the AddJobs are performed.
	// "Must Complete" data will be guaranteed finished, at preload conclusion, before the normal load phase commences.
	virtual void PurgeUnreferencedResources() = 0;

	// Sent as an event hint that gameplay rendering is imminent.
	// Low priority jobs may still be in async flight.
	virtual void OnEndMapLoading( bool bAbort ) = 0;

	virtual void PurgeAll() = 0;

#if defined( _PS3 )
	virtual bool RequiresRendererLock() = 0;
#endif // _PS3
};

// Default implementation
class CResourcePreload : public IResourcePreload
{
	void PrepareForCreate( bool bSameMap ) {}
	void PurgeUnreferencedResources()	{}
	void OnEndMapLoading( bool bAbort )	{}
	void PurgeAll() {}
#if defined( _PS3 )
	virtual bool RequiresRendererLock() { return false; }
#endif // _PS3
};

// UI can install progress notification
abstract_class ILoaderProgress
{
public:
	// implementation must ignore UpdateProgress() if not scoped by Begin/End
	virtual void BeginProgress() = 0;
	virtual void EndProgress() = 0;
	virtual void UpdateProgress( float progress, bool bForce = false ) = 0;
	virtual void PauseNonInteractiveProgress( bool bPause ) = 0;
};

// spew detail
#define LOADER_DETAIL_NONE				0
#define LOADER_DETAIL_TIMING			(1<<0)
#define LOADER_DETAIL_COMPLETIONS		(1<<1)
#define LOADER_DETAIL_LATECOMPLETIONS	(1<<2)
#define LOADER_DETAIL_PURGES			(1<<3)
#define LOADER_DETAIL_CREATIONS			(1<<3)

abstract_class IQueuedLoader : public IAppSystem
{
public:
	virtual void				InstallLoader( ResourcePreload_t type, IResourcePreload *pLoader ) = 0;
	virtual void				InstallProgress( ILoaderProgress *pProgress ) = 0;

	// Set bOptimizeReload if you want appropriate data (such as static prop lighting)
	// to persist - rather than being purged and reloaded - when going from map A to map A.
	virtual bool				BeginMapLoading( const char *pMapName, bool bLoadForHDR, bool bOptimizeMapReload, void (*pfnBeginMapLoadingCallback)( int nStage ) ) = 0;
	virtual void				EndMapLoading( bool bAbort ) = 0;
	virtual bool				AddJob( const LoaderJob_t *pLoaderJob ) = 0;

	// injects a resource into the map's reslist, rejected if not understood
	virtual void				AddMapResource( const char *pFilename ) = 0;

	// callback is asynchronous
	virtual bool				ClaimAnonymousJob( const char *pFilename, QueuedLoaderCallback_t pCallback, void *pContext, void *pContext2 = NULL ) = 0;
	// provides data if loaded, caller owns data
	virtual bool				ClaimAnonymousJob( const char *pFilename, void **pData, int *pDataSize, LoaderError_t *pError = NULL ) = 0;

	virtual bool				IsMapLoading() const = 0;
	virtual bool				IsSameMapLoading() const = 0;
	virtual bool				IsFinished() const = 0;

	// callers can expect that jobs are not immediately started when batching
	virtual bool				IsBatching() const = 0;

	// callers can conditionalize operational spew
	virtual int					GetSpewDetail() const = 0;

	virtual void				PurgeAll( ResourcePreload_t *pDontPurgeList = NULL, int nPurgeListSize = 0 ) = 0;
#ifdef _PS3
	// hack to prevent PS/3 deadlock on queued loader render mutex when quitting during loading a map
	virtual uint                UnlockProgressBarMutex() = 0;
	virtual void                LockProgressBarMutex( uint nLockCount ) = 0;
#endif
};

DECLARE_TIER2_INTERFACE( IQueuedLoader, g_pQueuedLoader );

#endif // QUEUEDLOADER_H

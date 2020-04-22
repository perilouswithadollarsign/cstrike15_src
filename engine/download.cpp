//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//--------------------------------------------------------------------------------------------------------------
// download.cpp
// 
// Implementation file for optional HTTP asset downloading
// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2004
//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Includes
//--------------------------------------------------------------------------------------------------------------

// fopen is needed for the bzip code
#undef PROTECT_FILEIO_FUNCTIONS
#undef fopen

#if defined( WIN32 ) && !defined( _X360 )
#include "winlite.h"
#include <WinInet.h>
#endif

#include <assert.h>

#include "download.h"
#include "tier0/platform.h"
#include "download_internal.h"

#include "client.h"

#include <keyvalues.h>
#include "filesystem.h"
#include "filesystem_engine.h"
#include "server.h"
#include "vgui_baseui_interface.h"
#include "vprof.h"
#include "net_chan.h"
#include "tier1/interface.h"
#include "interfaces/interfaces.h"
#include "vgui/ILocalize.h"

#include "../utils/bzip2/bzlib.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#if defined( _PS3 )
#include "ps3/ps3_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IFileSystem *g_pFileSystem;
static const char *CacheDirectory = "cache";
static const char *CacheFilename = "cache/DownloadCache.db";
Color DownloadColor			(   0, 200, 100, 255 );
Color DownloadErrorColor	( 200, 100, 100, 255 );
Color DownloadCompleteColor	( 100, 200, 100, 255 );

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_DownloadManager, "DownloadManager" );

//--------------------------------------------------------------------------------------------------------------
static char * CloneString( const char *original )
{
	char *newString = new char[ Q_strlen( original ) + 1 ];
	Q_strcpy( newString, original );

	return newString;
}

//--------------------------------------------------------------------------------------------------------------
// Class Definitions
//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Purpose: Implements download cache manager
//--------------------------------------------------------------------------------------------------------------
class DownloadCache
{
public:
	DownloadCache();
	~DownloadCache();
	void Init();

	void GetCachedData( RequestContext *rc );			///< Loads cached data, if any
	void PersistToDisk( const RequestContext *rc );		///< Writes out a completed download to disk
	void PersistToCache( const RequestContext *rc );	///< Writes out a partial download (lost connection, user abort, etc) to cache

private:
	KeyValues *m_cache;

	void GetCacheFilename( const RequestContext *rc, char cachePath[_MAX_PATH] );
	void GenerateCacheFilename( const RequestContext *rc, char cachePath[_MAX_PATH] );

	void BuildKeyNames( const char *gamePath );			///< Convenience function to build the keys to index into m_cache
	char m_cachefileKey[BufferSize + 64];
	char m_timestampKey[BufferSize + 64];
};
static DownloadCache *TheDownloadCache = NULL;

//--------------------------------------------------------------------------------------------------------------
DownloadCache::DownloadCache()
{
	m_cache = NULL;
}

//--------------------------------------------------------------------------------------------------------------
DownloadCache::~DownloadCache()
{
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::BuildKeyNames( const char *gamePath )
{
	if ( !gamePath )
	{
		m_cachefileKey[0] = 0;
		m_timestampKey[0] = 0;
		return;
	}

	char *tmpGamePath = CloneString( gamePath );
	char *tmp = tmpGamePath;
	while ( *tmp )
	{
		if ( *tmp == '/' || *tmp == '\\' )
		{
			*tmp = '_';
		}
		++tmp;
	}
	Q_snprintf( m_cachefileKey, sizeof( m_cachefileKey ), "cachefile_%s", tmpGamePath );
	Q_snprintf( m_timestampKey, sizeof( m_timestampKey ), "timestamp_%s", tmpGamePath );

	delete[] tmpGamePath;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::Init()
{
	if ( m_cache )
	{
		m_cache->deleteThis();
	}

	m_cache = new KeyValues( "DownloadCache" );
	m_cache->LoadFromFile( g_pFileSystem, CacheFilename, NULL );
	g_pFileSystem->CreateDirHierarchy( CacheDirectory, "DEFAULT_WRITE_PATH" );
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::GetCachedData( RequestContext *rc )
{
	if ( !m_cache )
		return;

	char cachePath[_MAX_PATH];
	GetCacheFilename( rc, cachePath );

	if ( !(*cachePath) )
		return;

	FileHandle_t fp = g_pFileSystem->Open( cachePath, "rb" );

	if ( fp == FILESYSTEM_INVALID_HANDLE )
		return;

	int size = g_pFileSystem->Size(fp);
	rc->cacheData = new unsigned char[size];
	int status = g_pFileSystem->Read( rc->cacheData, size, fp );
	g_pFileSystem->Close( fp );
	if ( !status )
	{
		delete[] rc->cacheData;
		rc->cacheData = NULL;
	}
	else
	{
		BuildKeyNames( rc->gamePath );
		rc->nBytesCached = size;
		strncpy( rc->cachedTimestamp, m_cache->GetString( m_timestampKey, "" ), BufferSize );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 *  Takes a data stream compressed with bzip2, and writes it out to disk, uncompresses it, and deletes the
 *  compressed version.
 */
static bool DecompressBZipToDisk( const char *outFilename, const char *srcFilename, char *data, int bytesTotal )
{
	if ( g_pFileSystem->FileExists( outFilename ) || !data || bytesTotal < 1 )
	{
		return false;
	}

	// Create the subdirs
	char * tmpDir = CloneString( outFilename );
	COM_CreatePath( tmpDir );
	delete[] tmpDir;

	// open the file for writing
	char fullSrcPath[MAX_PATH];
	Q_MakeAbsolutePath( fullSrcPath, sizeof( fullSrcPath ), srcFilename, com_gamedir );

	if ( !g_pFileSystem->FileExists( fullSrcPath ) )
	{
		// Write out the .bz2 file, for simplest decompression
		FileHandle_t ifp = g_pFileSystem->Open( fullSrcPath, "wb" );
		if ( !ifp )
		{
			return false;
		}
		int bytesWritten = g_pFileSystem->Write( data, bytesTotal, ifp );
		g_pFileSystem->Close( ifp );
		if ( bytesWritten != bytesTotal )
		{
			// couldn't write out all of the .bz2 file
			g_pFileSystem->RemoveFile( srcFilename );
			return false;
		}
	}

	// Prepare the uncompressed filehandle
	FileHandle_t ofp = g_pFileSystem->Open( outFilename, "wb" );
	if ( !ofp )
	{
		g_pFileSystem->RemoveFile( srcFilename );
		return false;
	}

	// And decompress!
	const int OutBufSize = 65536;
	char    buf[ OutBufSize ];
	BZFILE *bzfp = BZ2_bzopen( fullSrcPath, "rb" );
	int totalBytes = 0;

	while ( 1 )
	{
		int bytesRead = BZ2_bzread( bzfp, buf, OutBufSize );
		if ( bytesRead < 0 )
		{
			break; // error out
		}

		if ( bytesRead > 0 )
		{
			int bytesWritten = g_pFileSystem->Write( buf, bytesRead, ofp );
			if ( bytesWritten != bytesRead )
			{
				break; // error out
			}
			else
			{
				totalBytes += bytesWritten;
				
				static const int s_numMaxFileSizeBytes = CommandLine()->ParmValue( "-maxdownloadfilesizemb", 150 )*1024*1024;
				if ( totalBytes > s_numMaxFileSizeBytes )
				{
					Warning( "DecompressBZipToDisk: '%s' too big (max %.1f megabytes, use launch option -maxdownloadfilesizemb N to override).\n", srcFilename, float( s_numMaxFileSizeBytes )/float( 1024*1024 ) );
					break; // error out
				}
			}
		}
		else
		{
			g_pFileSystem->Close( ofp );
			BZ2_bzclose( bzfp );
			g_pFileSystem->RemoveFile( srcFilename );
			return true;
		}
	}

	// We failed somewhere, so clean up and exit
	g_pFileSystem->Close( ofp );
	BZ2_bzclose( bzfp );
	g_pFileSystem->RemoveFile( srcFilename );
	g_pFileSystem->RemoveFile( outFilename );
	return false;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::PersistToDisk( const RequestContext *rc )
{
	if ( !m_cache )
		return;

	if ( rc && rc->data && rc->nBytesTotal )
	{
		char gamePath[MAX_PATH];
		if ( rc->bIsBZ2 )
		{
			Q_StripExtension( rc->gamePath, gamePath, sizeof( gamePath ) );
		}
		else
		{
			Q_strncpy( gamePath, rc->gamePath, sizeof( gamePath ) );
		}

		if ( !g_pFileSystem->FileExists( gamePath ) )
		{
			// Create the subdirs
			char * tmpDir = CloneString( gamePath );
			COM_CreatePath( tmpDir );
			delete[] tmpDir;

			bool success = false;
			if ( rc->bIsBZ2 )
			{
				success = DecompressBZipToDisk( gamePath, rc->gamePath, reinterpret_cast< char * >(rc->data), rc->nBytesTotal );
			}
			else
			{
				FileHandle_t fp = g_pFileSystem->Open( gamePath, "wb" );
				if ( fp )
				{
					g_pFileSystem->Write( rc->data, rc->nBytesTotal, fp );
					g_pFileSystem->Close( fp );
					success = true;
				}
			}

			if ( success )
			{
				// write succeeded.  remove any old data from the cache.
				char cachePath[_MAX_PATH];
				GetCacheFilename( rc, cachePath );
				if ( cachePath[0] )
				{
					g_pFileSystem->RemoveFile( cachePath, NULL );
				}

				BuildKeyNames( rc->gamePath );
				KeyValues *kv = m_cache->FindKey( m_cachefileKey, false );
				if ( kv )
				{
					m_cache->RemoveSubKey( kv );
				}
				kv = m_cache->FindKey( m_timestampKey, false );
				if ( kv )
				{
					m_cache->RemoveSubKey( kv );
				}
			}
		}
	}

	m_cache->SaveToFile( g_pFileSystem, CacheFilename, NULL );
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::PersistToCache( const RequestContext *rc )
{
	if ( !m_cache || !rc || !rc->data || !rc->nBytesTotal || !rc->nBytesCurrent )
		return;

	char cachePath[_MAX_PATH];
	GenerateCacheFilename( rc, cachePath );

	FileHandle_t fp = g_pFileSystem->Open( cachePath, "wb" );
	if ( fp )
	{
		g_pFileSystem->Write( rc->data, rc->nBytesCurrent, fp );
		g_pFileSystem->Close( fp );

		m_cache->SaveToFile( g_pFileSystem, CacheFilename, NULL );
	}
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::GetCacheFilename( const RequestContext *rc, char cachePath[_MAX_PATH] )
{
	BuildKeyNames( rc->gamePath );
	const char *path = m_cache->GetString( m_cachefileKey, NULL );
	if ( !path || !StringHasPrefixCaseSensitive( path, CacheDirectory ) )
	{
		cachePath[0] = 0;
		return;
	}
	strncpy( cachePath, path, _MAX_PATH );
	cachePath[_MAX_PATH-1] = 0;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadCache::GenerateCacheFilename( const RequestContext *rc, char cachePath[_MAX_PATH] )
{
	GetCacheFilename( rc, cachePath );
	BuildKeyNames( rc->gamePath );

	m_cache->SetString( m_timestampKey, rc->cachedTimestamp );
	
	if ( !*cachePath )
	{
		const char * lastSlash = strrchr( rc->gamePath, '/' );
		const char * lastBackslash = strrchr( rc->gamePath, '\\' );
		const char *gameFilename = rc->gamePath;
		if ( lastSlash || lastBackslash )
		{
			gameFilename = MAX( lastSlash, lastBackslash ) + 1;
		}
		for( int i=0; i<1000; ++i )
		{
			Q_snprintf( cachePath, _MAX_PATH, "%s/%s%4.4d", CacheDirectory, gameFilename, i );
			if ( !g_pFileSystem->FileExists( cachePath ) )
			{
				m_cache->SetString( m_cachefileKey, cachePath );
				return;
			}
		}
		// all 1000 were invalid?!?
		Q_snprintf( cachePath, _MAX_PATH, "%s/overflow", CacheDirectory );
		m_cache->SetString( m_cachefileKey, cachePath );
	}
}

//--------------------------------------------------------------------------------------------------------------
// Purpose: Implements download manager class
//--------------------------------------------------------------------------------------------------------------
class DownloadManager
{
public:
	DownloadManager();
	~DownloadManager();

	void Queue( const char *baseURL, const char *gamePath );
	void Stop() { Reset(); }
	int GetQueueSize() { return m_queuedRequests.Count(); }

	bool Update();	///< Monitors download thread, starts new downloads, and updates progress bar

	bool FileReceived( const char *filename, unsigned int requestID, bool isReplayDemoFile );
	bool FileDenied( const char *filename, unsigned int requestID, bool isReplayDemoFile );

	bool HasMapBeenDownloadedFromServer( const char *serverMapName );
	void MarkMapAsDownloadedFromServer( const char *serverMapName );

private:
	void Reset();						///< Cancels any active download, as well as any queued ones

	void PruneCompletedRequests();		///< Check download requests that have been completed to see if their threads have exited
	void CheckActiveDownload();			///< Checks download status, and updates progress bar
	void StartNewDownload();			///< Starts a new download if there are queued requests

	void UpdateProgressBar();

	typedef CUtlVector< RequestContext * > RequestVector;
	RequestVector m_queuedRequests;		///< these are requests waiting to be spawned
	RequestContext *m_activeRequest;	///< this is the active request being downloaded in another thread
	RequestVector m_completedRequests;	///< these are waiting for the thread to exit

	int m_lastPercent;					///< last percent value the progress bar was updated with (to avoid spamming it)
	int m_totalRequests;				///< Total number of requests (used to set the top progress bar)

	int m_RequestIDCounter;				///< global increasing request ID counter

	typedef CUtlVector< char * > StrVector;
	StrVector m_downloadedMaps;			///< List of maps for which we have already tried to download assets.
};

//--------------------------------------------------------------------------------------------------------------
static DownloadManager TheDownloadManager;

//--------------------------------------------------------------------------------------------------------------
DownloadManager::DownloadManager()
{
	m_activeRequest = NULL;
	m_lastPercent = 0;
	m_totalRequests = 0;
}

//--------------------------------------------------------------------------------------------------------------
DownloadManager::~DownloadManager()
{
	Reset();

	for ( int i=0; i<m_downloadedMaps.Count(); ++i )
	{
		delete[] m_downloadedMaps[i];
	}
	m_downloadedMaps.RemoveAll();
}

//--------------------------------------------------------------------------------------------------------------
bool DownloadManager::HasMapBeenDownloadedFromServer( const char *serverMapName )
{
	if ( !serverMapName )
		return false;

	for ( int i=0; i<m_downloadedMaps.Count(); ++i )
	{
		const char *oldServerMapName = m_downloadedMaps[i];
		if ( oldServerMapName && !stricmp( serverMapName, oldServerMapName ) )
		{
			return true;
		}
	}
	return false;
}

bool DownloadManager::FileDenied( const char *filename, unsigned int requestID, bool isReplayDemoFile )
{
	if ( !m_activeRequest )
		return false;

	if ( m_activeRequest->nRequestID != requestID )
		return false;

	if ( m_activeRequest->bAsHTTP )
		return false;

	Log_Msg( LOG_DownloadManager, DownloadErrorColor, "Error downloading %s\n", m_activeRequest->gamePath );
	UpdateProgressBar();

	// try to download the next file
	m_completedRequests.AddToTail( m_activeRequest );
	m_activeRequest = NULL;

	return true;
}

// INFESTED_DLL
#if !defined( DEDICATED )
extern bool g_bASW_Waiting_For_Map_Build;
#endif

bool DownloadManager::FileReceived( const char *filename, unsigned int requestID, bool isReplayDemoFile )
{
	if ( !m_activeRequest )
		return false;

	if ( m_activeRequest->nRequestID != requestID )
		return false;

	if ( m_activeRequest->bAsHTTP )
		return false;

	Log_Msg( LOG_DownloadManager, DownloadCompleteColor, "Download finished!\n" );
	UpdateProgressBar();

	m_completedRequests.AddToTail( m_activeRequest );
	m_activeRequest = NULL;

	// INFESTED_DLL
	static char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );
	if ( !Q_stricmp( gamedir, "infested" ) )
	{
		// see if we just recieved a map layout
		const char *pExt = V_GetFileExtension( filename );
		if ( !Q_stricmp( pExt, "layout" ) )
		{
#if !defined( DEDICATED )
			// start compiling the map
			g_bASW_Waiting_For_Map_Build = true;
#endif

			char cmd[ 256 ];
			Q_snprintf( cmd, sizeof( cmd ), "asw_build_map %s connecting\n", filename );
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), cmd );
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::MarkMapAsDownloadedFromServer( const char *serverMapName )
{
	if ( !serverMapName )
		return;

	if ( HasMapBeenDownloadedFromServer( serverMapName ) )
		return;

	m_downloadedMaps.AddToTail( CloneString( serverMapName ) );


	return;
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::Queue( const char *baseURL, const char *gamePath )
{
	if ( !CNetChan::IsValidFileForTransfer( gamePath ) )
		return;

	bool bAsHTTP = false;
	if ( !gamePath )
	{
		return;
	}
	VPROF_BUDGET( "DownloadManager::Queue", VPROF_BUDGETGROUP_STEAM );

	if ( sv.IsActive() )
	{
		return;	// don't try to download things for the local server (in case a map is missing sounds etc that
				// aren't needed to play.
	}

	// only http downloads
	if ( baseURL && ( StringHasPrefix( baseURL, "http://" ) || StringHasPrefix( baseURL, "https://" ) ) )
	{
		bAsHTTP = true;
	}

	if ( g_pFileSystem->FileExists( gamePath ) && !CL_ShouldRedownloadFile( gamePath ) )
	{	
		return; // don't download existing files
	}

	if ( bAsHTTP && !g_pFileSystem->FileExists( va( "%s.bz2", gamePath ) ) )
	{
		// Queue up an HTTP download of the bzipped asset, in case it exists.
		// When a bzipped download finishes, we'll uncompress the file to it's
		// original destination, and the queued download of the uncompressed
		// file will abort.

		++m_totalRequests;
		if ( !TheDownloadCache )
		{
			TheDownloadCache = new DownloadCache;
			TheDownloadCache->Init();
		}

		RequestContext *rc = new RequestContext;
		m_queuedRequests.AddToTail( rc );

		memset( rc, 0, sizeof(RequestContext) );

		rc->status = HTTP_CONNECTING;

		Q_strncpy( rc->basePath, com_gamedir, BufferSize );
		Q_strncpy( rc->gamePath, gamePath, BufferSize );
		Q_strncat( rc->gamePath, ".bz2", BufferSize, COPY_ALL_CHARACTERS );
		Q_FixSlashes( rc->gamePath, '/' ); // only matters for debug prints, which are full URLS, so we want forward slashes
#ifndef DEDICATED
		Q_strncpy( rc->serverURL, GetBaseLocalClient().m_NetChannel->GetAddress(), BufferSize );
#endif

		rc->bIsBZ2 = true;
		rc->bAsHTTP = true;
		Q_strncpy( rc->baseURL, baseURL, BufferSize );
		Q_strncat( rc->baseURL, "/", BufferSize, COPY_ALL_CHARACTERS );
	}

	++m_totalRequests;
	if ( !TheDownloadCache )
	{
		TheDownloadCache = new DownloadCache;
		TheDownloadCache->Init();
	}

	RequestContext *rc = new RequestContext;
	m_queuedRequests.AddToTail( rc );

	memset( rc, 0, sizeof(RequestContext) );

	rc->status = HTTP_CONNECTING;

	Q_strncpy( rc->basePath, com_gamedir, BufferSize );
	Q_strncpy( rc->gamePath, gamePath, BufferSize );
	Q_FixSlashes( rc->gamePath, '/' ); // only matters for debug prints, which are full URLS, so we want forward slashes
#ifndef DEDICATED
	Q_strncpy( rc->serverURL, GetBaseLocalClient().m_NetChannel->GetAddress(), BufferSize );
#endif

	if ( bAsHTTP )
	{
		rc->bAsHTTP = true;
		Q_strncpy( rc->baseURL, baseURL, BufferSize );
		Q_strncat( rc->baseURL, "/", BufferSize, COPY_ALL_CHARACTERS );
	}
	else
	{
		rc->bAsHTTP = false;
	}
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::Reset()
{
	// ask the active request to bail
	if ( m_activeRequest )
	{
		Log_Msg( LOG_DownloadManager, DownloadColor, "Aborting download of %s\n", m_activeRequest->gamePath );
		if ( m_activeRequest->nBytesTotal && m_activeRequest->nBytesCurrent )
		{
			// Persist partial data to cache
			TheDownloadCache->PersistToCache( m_activeRequest );
		}
		m_activeRequest->shouldStop = true;
		m_completedRequests.AddToTail( m_activeRequest );
		m_activeRequest = NULL;
		//TODO: StopLoadingProgressBar();
	}

	// clear out any queued requests
	for ( int i=0; i<m_queuedRequests.Count(); ++i )
	{
		Log_Msg( LOG_DownloadManager, DownloadColor, "Discarding queued download of %s\n", m_queuedRequests[i]->gamePath );
		delete m_queuedRequests[i];
	}
	m_queuedRequests.RemoveAll();

	if ( TheDownloadCache )
	{
		delete TheDownloadCache;
		TheDownloadCache = NULL;
	}

	m_lastPercent = 0;
	m_totalRequests = 0;
}

//--------------------------------------------------------------------------------------------------------------
// Check download requests that have been completed to see if their threads have exited
void DownloadManager::PruneCompletedRequests()
{
	for ( int i=m_completedRequests.Count()-1; i>=0; --i )
	{
		if ( m_completedRequests[i]->threadDone || !m_completedRequests[i]->bAsHTTP )
		{
			if ( m_completedRequests[i]->cacheData )
			{
				delete[] m_completedRequests[i]->cacheData;
			}
			delete m_completedRequests[i];
			m_completedRequests.Remove( i );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
// Checks download status, and updates progress bar
void DownloadManager::CheckActiveDownload()
{
	if ( !m_activeRequest )
		return;

	if ( !m_activeRequest->bAsHTTP )
	{
		UpdateProgressBar();
		return;
	}

	
	// check active request for completion / error / progress update
	switch ( m_activeRequest->status )
	{
	case HTTP_DONE:
		Log_Msg( LOG_DownloadManager, DownloadCompleteColor, "Download finished!\n" );
		UpdateProgressBar();
		if ( m_activeRequest->nBytesTotal )
		{
// 			// change it to be updating steam resources
#ifndef DEDICATED
 			EngineVGui()->UpdateSecondaryProgressBarWithFile( 1, m_activeRequest->gamePath, m_activeRequest->nBytesTotal );
#endif

			// Persist complete data to disk, and remove cache entry
			TheDownloadCache->PersistToDisk( m_activeRequest );
			m_activeRequest->shouldStop = true;
			m_completedRequests.AddToTail( m_activeRequest );
			m_activeRequest = NULL;
			if ( !m_queuedRequests.Count() )
			{
				//TODO: StopLoadingProgressBar();
				//TODO: Cbuf_AddText("retry\n");
			}
		}
		break;
	case HTTP_ERROR:
		Log_Msg( LOG_DownloadManager, DownloadErrorColor, "Error downloading %s%s\n", m_activeRequest->baseURL, m_activeRequest->gamePath );
		UpdateProgressBar();

		// try to download the next file
		m_activeRequest->shouldStop = true;
		m_completedRequests.AddToTail( m_activeRequest );
		m_activeRequest = NULL;
		if ( !m_queuedRequests.Count() )
		{
			//TODO: StopLoadingProgressBar();
			//TODO: Cbuf_AddText("retry\n");
		}
		break;
	case HTTP_FETCH:
		UpdateProgressBar();
		// Update progress bar
		if ( m_activeRequest->nBytesTotal )
		{
			int percent = ( m_activeRequest->nBytesCurrent * 100 / m_activeRequest->nBytesTotal );
			if ( percent != m_lastPercent )
			{
				m_lastPercent = percent;
#ifndef DEDICATED
				EngineVGui()->UpdateSecondaryProgressBarWithFile( m_lastPercent * 0.01f, m_activeRequest->gamePath, m_activeRequest->nBytesTotal );
#endif
			}
		}
		break;
	}
}

//--------------------------------------------------------------------------------------------------------------
// Starts a new download if there are queued requests
void DownloadManager::StartNewDownload()
{
	if ( m_activeRequest || !m_queuedRequests.Count() )
		return;

	while ( !m_activeRequest && m_queuedRequests.Count() )
	{
		// Remove one request from the queue and make it active
		m_activeRequest = m_queuedRequests[0];
		m_queuedRequests.Remove( 0 );

		if ( g_pFileSystem->FileExists( m_activeRequest->gamePath ) && !CL_ShouldRedownloadFile( m_activeRequest->gamePath ) )
		{
			Log_Msg( LOG_DownloadManager, DownloadColor, "Skipping existing file %s%s.\n", m_activeRequest->baseURL, m_activeRequest->gamePath );
			m_activeRequest->shouldStop = true;
			m_activeRequest->threadDone = true;
			m_completedRequests.AddToTail( m_activeRequest );
			m_activeRequest = NULL;
		}
	}

	if ( !m_activeRequest )
		return;

	if ( g_pFileSystem->FileExists( m_activeRequest->gamePath ) && !CL_ShouldRedownloadFile( m_activeRequest->gamePath ) )
	{
		m_activeRequest->shouldStop = true;
		m_activeRequest->threadDone = true;
		m_completedRequests.AddToTail( m_activeRequest );
		m_activeRequest = NULL;
		return; // don't download existing files
	}

	if ( m_activeRequest->bAsHTTP )
	{
		// Check cache for partial match
		TheDownloadCache->GetCachedData( m_activeRequest );

		//TODO: ContinueLoadingProgressBar( "Http", m_totalRequests - m_queuedRequests.Count(), 0.0f );
		//TODO: SetLoadingProgressBarStatusText( "#GameUI_VerifyingAndDownloading" );
#ifndef DEDICATED
 		EngineVGui()->UpdateSecondaryProgressBarWithFile( 0, m_activeRequest->gamePath, m_activeRequest->nBytesTotal );
#endif
		UpdateProgressBar();

		Log_Msg( LOG_DownloadManager, DownloadColor, "Downloading %s%s.\n", m_activeRequest->baseURL, m_activeRequest->gamePath );
		m_lastPercent = 0;

		// Start the thread
		ThreadHandle_t hSimpleThread = CreateSimpleThread( DownloadThread, m_activeRequest );
		ReleaseThreadHandle( hSimpleThread );
	}
	else
	{
		UpdateProgressBar();
		Log_Msg( LOG_DownloadManager, DownloadColor, "Downloading %s.\n", m_activeRequest->gamePath );
		m_lastPercent = 0;
		
#ifndef DEDICATED
		m_activeRequest->nRequestID = GetBaseLocalClient().m_NetChannel->RequestFile( m_activeRequest->gamePath, false );
#endif
	}
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::UpdateProgressBar()
{
	if ( !m_activeRequest )
	{
		return;
	}

	//wchar_t filenameBuf[MAX_OSPATH];
	float progress = 0.0f;
#ifndef DEDICATED
	int received, total = 0;
#endif
	if ( m_activeRequest->bAsHTTP )
	{
		int overallPercent = (m_totalRequests - m_queuedRequests.Count() - 1) * 100 / m_totalRequests;
		int filePercent = 0;
		if ( m_activeRequest->nBytesTotal > 0 )
		{	
			filePercent = ( m_activeRequest->nBytesCurrent * 100 / m_activeRequest->nBytesTotal );
		}

		progress = (overallPercent + filePercent * 1.0f / m_totalRequests) * 0.01f;
	}
	else
	{
#ifndef DEDICATED
		GetBaseLocalClient().m_NetChannel->GetStreamProgress( FLOW_INCOMING, &received, &total );
		
		progress = (float)(received)/(float)(total);
#endif
	}

#ifndef DEDICATED
	EngineVGui()->UpdateSecondaryProgressBarWithFile( progress, m_activeRequest->gamePath, total );
#endif
}

//--------------------------------------------------------------------------------------------------------------
// Monitors download thread, starts new downloads, and updates progress bar
bool DownloadManager::Update()
{
	PruneCompletedRequests();
	CheckActiveDownload();
	StartNewDownload();

	return m_activeRequest != NULL;
}

//--------------------------------------------------------------------------------------------------------------
// Externally-visible function definitions
//--------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
bool CL_DownloadUpdate(void)
{
	return TheDownloadManager.Update();
}

//--------------------------------------------------------------------------------------------------------------
void CL_HTTPStop_f(void)
{
	TheDownloadManager.Stop();
}

bool CL_FileReceived( const char *filename, unsigned int requestID, bool isReplayDemoFile )
{
	return TheDownloadManager.FileReceived( filename, requestID, isReplayDemoFile );
}

bool CL_FileDenied( const char *filename, unsigned int requestID, bool isReplayDemoFile )
{
	return TheDownloadManager.FileDenied( filename, requestID, isReplayDemoFile );
}

//--------------------------------------------------------------------------------------------------------------
extern ConVar sv_downloadurl;
void CL_QueueDownload( const char *filename )
{
	TheDownloadManager.Queue( sv_downloadurl.GetString(), filename );
}

//--------------------------------------------------------------------------------------------------------------
int CL_GetDownloadQueueSize(void)
{
	return TheDownloadManager.GetQueueSize();
}

//--------------------------------------------------------------------------------------------------------------
int CL_CanUseHTTPDownload(void)
{
	if ( sv_downloadurl.GetString()[0] )
	{
#ifndef DEDICATED
		const char *serverMapName = va( "%s:%s", sv_downloadurl.GetString(), GetBaseLocalClient().m_szLevelName );
		return !TheDownloadManager.HasMapBeenDownloadedFromServer( serverMapName );
#endif
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------------------
void CL_MarkMapAsUsingHTTPDownload(void)
{
#ifndef DEDICATED
	const char *serverMapName = va( "%s:%s", sv_downloadurl.GetString(), GetBaseLocalClient().m_szLevelName );
	TheDownloadManager.MarkMapAsDownloadedFromServer( serverMapName );
#endif
}


//--------------------------------------------------------------------------------------------------------------
bool CL_ShouldRedownloadFile( const char *filename )
{
	// INFESTED_DLL - allow redownloading of the .layout or .bsp files, as these may have changed since we last downloaded them
	static char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );
	if ( !Q_stricmp( gamedir, "infested" ) && ( StringHasPrefix( filename + 5, "gridrandom" ) || StringHasPrefix( filename + 5, "output" ) ) )
	{
		char extension[12];
		Q_ExtractFileExtension( filename, extension, sizeof( extension ) );
		if ( !Q_stricmp( extension, "layout" ) || !Q_stricmp( extension, "bsp" ) )
		{
			return true;
		}
	}
	return false;
}


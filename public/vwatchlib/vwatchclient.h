//================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
//
//
//==================================================================================================

#ifndef VWATCHCLIENT_H
#define VWATCHCLIENT_H
#ifdef _WIN32
#pragma once
#endif


#include "tier0/threadtools.h"
#include "tier1/utlvector.h"
#include "tier1/checksum_crc.h"
#include "tier1/utlstring.h"


class CSharedMemoryMgr;
class CVWatchHeader;
class IFileSystem;
class CAppSignal;


enum GetSnapshotStatus_t
{
	GETSNAPSHOT_OK,					// Got the snapshot.
	GETSNAPSHOT_NOTDONESCANNING,	// The vwatch service is still scanning this tree.
	GETSNAPSHOT_ERROR
};

enum GetCRCStatus_t
{
	GETCRC_GOT_CACHED,						// This is the best case. We did a fast lookup and got a valid CRC.
	GETCRC_CALCULATED_AND_CACHED,			// 2nd best. We had to calculate the CRC but vwatch was around so we cached the value.
	GETCRC_CALCULATED_AND_DIDNT_CACHE,		// 3rd best. We got the CRC but no vwatch, so it's not cached for next time.
	GETCRC_FAIL								// Fail.
};

enum StartWatchingDirStatus_t
{
	STARTWATCHINGDIR_STARTED,
	STARTWATCHINGDIR_ALREADYWATCHING,
	STARTWATCHINGDIR_ERROR
};


class CVWatchStats
{
public:
	uint32 m_nFileEntriesCreated;
	uint32 m_nDirectoryEntriesCreated;

	uint64 m_nMemoryBytesUsed;
	uint64 m_nMemoryBytesLimit;

	// If this is true, then it has run out of memory and it's going to sit there and not scan anything 
	// until CVWatchClient::SendRestartSignal() has been called.
	bool m_bHasRunOutOfMemory;

	// Some breakdown of how the memory is used.
	uint64 m_nWatchedFileBytes;
	uint64 m_nWatchedDirBytes;
	uint64 m_nWatchedDirLongNameBytes;	// This is part of m_nWatchedDirBytes.

	uint32 m_nFilesIterated;

	uint32 m_nClientLocks; // # of times CVWatchClient has locked the shared memory buffer
};


// ------------------------------------------------------------------------------------------------ //
// COffsetPtr is the most common type and convenient type of pointer used in shared memory.
// It can be used between any two nonmoving structures (even if they're not in shared memory).
// ------------------------------------------------------------------------------------------------ //
template< class T >
class COffsetPtr
{
public:
	COffsetPtr& operator=( T *p )
	{
		if ( p )
			m_nOffset = (char*)p - (char*)this;
		else
			m_nOffset = 0;

		return *this;
	}

	COffsetPtr& operator=( COffsetPtr<T> &p )
	{
		return ( *this = (T*)p );
	}

	T* operator->()
	{
		return ( m_nOffset ? (T*)( ((char*)this) + m_nOffset ) : (T*)NULL );
	}

	operator const T*() const
	{
		return ( m_nOffset ? (T*)( ((char*)this) + m_nOffset ) : (T*)NULL );
	}

	operator T*()
	{
		return ( m_nOffset ? (T*)( ((char*)this) + m_nOffset ) : (T*)NULL );
	}

	const T* Get() const
	{
		return ( m_nOffset ? (T*)( ((char*)this) + m_nOffset ) : (T*)NULL );
	}
	
	T* Get()
	{
		return ( m_nOffset ? (T*)( ((char*)this) + m_nOffset ) : (T*)NULL );
	}

private:
	intp m_nOffset;
};



class CSnapshotDir;

class CSnapshotFile
{
public:
	CSnapshotFile();

	// Get the full filename.
	void GetLongName( char *pOut, int maxLen );

public:
	uint64 m_nFileSize;
	COffsetPtr<char> m_pShortName;
	COffsetPtr<CSnapshotFile> m_pNextFile;
	COffsetPtr<CSnapshotDir> m_pDir;
};

class CSnapshotDir
{
public:
	CSnapshotDir();

	uint64 GetFileDataSize_R();
	int GetNumFiles_R();

	CSnapshotFile* GetFile( const char *pShortName );
	CSnapshotDir* GetDir( const char *pLongName );

public:
	// This does NOT include a trailing slash.
	COffsetPtr<char> m_pLongName;
	
	COffsetPtr<CSnapshotDir> m_pFirstDir;
	COffsetPtr<CSnapshotDir> m_pNextDir;

	COffsetPtr<CSnapshotFile> m_pFirstFile;
};


class CVWatchSnapshot
{
public:
	
	CVWatchSnapshot();

	void AddRef();
	void Release();

	// This can return null if there are no files in this snapshot.
	virtual CSnapshotDir* GetRootDir();

	// Figure out what we'd need to send and delete from pTo in order to make it look like us.
	static void CalcDelta( CVWatchSnapshot *pFrom, CVWatchSnapshot *pTo, CUtlVector<CSnapshotFile*> &filesToSend, CUtlVector<CSnapshotFile*> &filesToDelete );

	int GetNumFiles();
	uint64 GetFileDataSize();

protected:
	virtual ~CVWatchSnapshot();

protected:
	int m_nRefCount;
};



// Flags for CVWatchClient::GetSnapshot.

// GETSNAPSHOT_MACHINE_LEVEL_ROOT means that a GetSnapshot of c:\test will look like this:
//		+- (empty string)
//			+- c:
//				+- c:\test
//					etc...
//
// Without this flag, the CVWatchSnapshot's root directory would be c:\test in the example above.
#define GETSNAPSHOT_MACHINE_LEVEL_ROOT	0x01


class CVWatchClient
{
public:

	CVWatchClient();
	~CVWatchClient();

	//
	// Connection-management functions.
	//
	
	// Connect to the running vwatch service.
	// nTimeout tells it how long to wait. Use TT_INFINITE to wait forever.
	bool Connect( uint32 nTimeout );
	void Term();

	// Note: This can return false even if Connect() returned true because the vwatch_service
	// process might have died.
	bool IsConnected();

	// You can do this to tell the service to completely drop all its current results and start scanning again. Mostly for debugging.
	bool SendRestartSignal();


	//
	// Control what directories VWatch is scanning.
	//

	// Start and stop watching specific directories.
	StartWatchingDirStatus_t StartWatchingDir( const char *pDirName );
	bool StopWatchingDir( const char *pDirName );

	// Tells us if vwatch_service is watching the specified directory.
	bool IsWatchingDir( const char *pDirName );

	// Get a list of the directories that it's currently watching.
	bool GetWatchedDirectories( CUtlVector<CUtlString> &dirs );



	//
	// Status.
	// 

	// This is mostly here for testing. It communicates with the service to see
	// how many files it has scanned so far.
	bool GetNumFilesScanned( int *pnFilesScanned, uint32 nTimeout=TT_INFINITE );

	// This is only updated periodically to get an idea of the stats. It may not be 100% up-to-date at all times.
	// Also, it'll return null if you aren't connected to the service.
	const CVWatchStats* GetStats();



	//
	// CRC query.
	//

	// This gets the file's CRC. If it's cached and we can access it, we use that.
	// If not, we calculate it (and cache it if vwatch_service is running).
	GetCRCStatus_t GetFileCRC( const char *pFilename, CRC32_t &crc );


	//
	// Snapshots.
	//

	// Get a snapshot of the specified directory.
	// Flags is a combination of GETSNAPSHOT_ flags.
	GetSnapshotStatus_t GetSnapshotForDir( const char *pDirName, CVWatchSnapshot **pSnapshot, int nFlags );

	// Load/save snapshots.
	CVWatchSnapshot* LoadSnapshot( IFileSystem *pFileSystem, const char *pFilename );
	bool SaveSnapshot( CVWatchSnapshot *pSnapshot, IFileSystem *pFileSystem, const char *pFilename );

	// Calculate a snapshot given a starting one that has added and removed files.
	// This is used by RemoteMirror to remember what files the remote machine has on it.
	CVWatchSnapshot* CalcMergedSnapshot( CVWatchSnapshot *pFrom, CUtlVector<CSnapshotFile*> &filesSent, CUtlVector<CSnapshotFile*> &filesDeleted );

	// Used by RemoteMirror if it can't load a previous snapshot. It simplifies the code if we can have a non-null snapshot
	// that's rooted in the right place.
	CVWatchSnapshot* CreateEmptySnapshot( const char *pLongRootDirName );


private:
	
	// This assumes you have the mutex locked!
	CVWatchHeader* GetVWatchHeader();

	// Start writing a signal to overwatch.
	// If it returns NULL, then that means it couldn't access vwatch or it couldn't get an app signal slot.
	// If it returns a pointer, then you MUST UnlockMutex( m_hDataMutex ) afterwards.
	CAppSignal* StartAppSignal( int nTimeout );

	// Lock the shared data. This also increments CVWatchHeader::m_nClientLocks.
	bool LockSharedDataMutex( uint32 nTimeout=TT_INFINITE );
	void UnlockSharedDataMutex();


private:
	
	// Library fallback routine - generates a manual snapshot for when we're not connected to service
	GetSnapshotStatus_t GetSlowSnapshotForDir( const char *pDirName, CVWatchSnapshot **pSnapshot, int nFlags );

	CSharedMemoryMgr *m_pSharedMemoryMgr;
	void *m_hDataMutex;	// From VWATCH_MUTEX_NAME

	// Process handle for vwatch_service.
	void *m_hServiceProcess;

	// Set in and returned by GetStats().
	CVWatchStats m_BackedUpVWatchStats;

	// The buffer and event for AppSignal responses.
	char m_AppSignalMemoryName[32];
	char m_AppSignalEventName[32];
	CSharedMemoryMgr *m_pAppSignalMemory;
	void *m_hAppSignalEvent;
};


#endif // VWATCHCLIENT_H

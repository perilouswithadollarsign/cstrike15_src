//========= Copyright 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: A collection of utility classes to simplify file I/O, and
//			as much as possible contain portability problems. Here avoiding 
//			including windows.h.
//
//=============================================================================

#ifndef FILEIO_H
#define FILEIO_H

#if defined (_WIN32)
typedef __time64_t time64_t;
#else
#include <sys/types.h>
#include <sys/stat.h>
typedef int64_t time64_t;
#if !defined( _PS3 )
#include <signal.h>
#endif // _PS3
#endif

#include "tier0/platform.h"
#include "tier0/t0constants.h"
#include "tier1/utlstring.h"
#include "tier1/utllinkedlist.h"

class CPathString
{
public:

	// Constructors: Automatically fixes slashes and removes double slashes when the object is
	// constructed, and then knows how to append magic \\?\ on Windows for unicode paths
	CPathString( const char *pchUTF8Path );
	~CPathString();

	// Gets the path in UTF8
	const char *GetUTF8Path();

	// Gets wchar_t based path, with \\?\ pre-pended (allowing long paths on Win32, should only be used with unicode aware filesystem calls)
	const wchar_t *GetWCharPathPrePended();

private:

	void PopulateWCharPath();

	char *m_pchUTF8Path;
	wchar_t *m_pwchWideCharPathPrepended;

};

// iterator class, initialize with the path & pattern you want to want files/dirs for.
//
// all string setters and accessors use UTF-8 encoding.
class CDirIterator
{
public:

#if !defined( _PS3 )
	CDirIterator( const char *pchSearchPath );
#endif

	CDirIterator( const char *pchPath, const char *pchPattern );
	~CDirIterator();

	bool IsValid() const;

	// fetch the next file
	bool BNextFile();

	// name of the current file - file portion only, not full path
	const char *CurrentFileName();

	// size of the current file
	int64 CurrentFileLength() const;

	// creation time of the current file
	time64_t CurrentFileCreateTime() const;

	// mod time of the current file
	time64_t CurrentFileWriteTime() const;

	// mode/type checks:

	// is the current file actually a directory?
	bool BCurrentIsDir() const;

	// is the current file hidden?
	bool BCurrentIsHidden() const;

	// is the current file read-only?
	bool BCurrentIsReadOnly() const;

	// is the current file a system file?
	bool BCurrentIsSystem() const;

	// is the current file's archive bit set?
	bool BCurrentIsMarkedForArchive() const;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
#if defined( _PS3 )
		ValidateObj( m_strPattern );
#else
		validator.ClaimMemory( m_pFindData );
#endif
	}
#endif

private:
	void Init( const char *pchSearchPath );
	bool BValidFilename();
	bool m_bNoFiles, m_bUsedFirstFile;

#if defined(_PS3)
	bool BFindNextPS3();
	bool m_bOpenHandle;		// don't see an invalid value for the FD returned from OpenDir
	int m_hFind;
	CellFsDirectoryEntry *m_pDirEntry;
	CUtlString m_strPattern;
#elif defined(_WIN32)
	HANDLE m_hFind;
	struct _WIN32_FIND_DATAW *m_pFindData;
	char m_rgchFileName[MAX_PATH * 4];
#else
	int64 m_hFind;
	struct _finddata_t *m_pFindData;
#endif
};

//-----------------------------------------------------------------------------
// Purpose: Encapsulates buffered async writing to a large file (one that will require multiple write calls)
//			calling Close() or destructing this object will block until the file is completely written
//-----------------------------------------------------------------------------
class CFileWriter
{
public:

	// Possible seek types
	enum ESeekOrigin
	{
		k_ESeekSet,
		k_ESeekCur,
		k_ESeekEnd
	};

	CFileWriter( bool bAsync = false );
	virtual ~CFileWriter();

	bool BFileOpen();
	bool BSetFile( const char *pchFile, bool bAllowOpenExisting = false );
	bool Write( const void *pvData, uint32 cubData );
	int  Printf( char *pDest, int bufferLen, PRINTF_FORMAT_STRING char const *pFormat, ... );
	bool Seek( uint64 offset, ESeekOrigin eOrigin );
	void Flush();
	void Close();
	uint64 GetBytesWritten();

#ifdef _WIN32
	static void __stdcall ThreadedWriteFileCompletionFunc( unsigned long dwErrorCode, unsigned long dwBytesTransfered, struct _OVERLAPPED *pOverlapped );
#elif defined( _PS3 )
	// not implemented on PS3
#elif defined(POSIX)
	static void __stdcall ThreadedWriteFileCompletionFunc( sigval sigval );
#else
#error
#endif

	void Sleep( uint nMSec ); // system specific sleep call

private:
	HANDLE m_hFileDest;
	uint64 m_cubWritten;
	volatile int m_cubOutstanding;
	bool m_bAsync;
	bool m_bDefaultAsync;
	uint32 m_unThreadID; // main thread for this FileWriter. On this thread we support correct async IO
	
	// if the CFileWriter is called from any other thread, we block until the write is complete
	// this is not great but a good enough for log files and we didn't need a full blow IOCP manager for this.
	volatile int m_cPendingCallbacksFromOtherThreads; 

};

// data accessor
inline uint64 CFileWriter::GetBytesWritten()
{ 
	return m_cubWritten; 
}

#if !defined(_PS3)
//-----------------------------------------------------------------------------
// Purpose: Encapsulates watching a directory for file changes
//-----------------------------------------------------------------------------
class CDirWatcher
{
public:
	CDirWatcher();
	~CDirWatcher();

	// only one directory can be watched at a time
	void SetDirToWatch( const char *pchDir );

	// retrieve any changes
	bool GetChangedFile( CUtlString *psFile );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif

private:
	CUtlLinkedList<CUtlString> m_listChangedFiles;
	void *m_hFile;
	void *m_pOverlapped;
	void *m_pFileInfo;
#ifdef OSX
public:
	struct timespec m_modTime;
	void AddFileToChangeList( const char *pchFile );
	CUtlString m_BaseDir;
private:
	void *m_WatcherStream;
#endif
	friend class CDirWatcherFriend;

#ifdef LINUX
	void AddFileToChangeList( const char *pchFile );
#endif
#ifdef WIN32
	// used by callback functions to push a file onto the list
	void AddFileToChangeList( const char *pchFile );
	void PostDirWatch();
#endif
};
#endif // _PS3

bool CreateDirRecursive( const char *pchPathIn );
bool BFileExists( const char *pchFileNameIn );
bool BCreateDirectory( const char *path );
bool BRemoveDirectoryRecursive( const char *pchPath );

#endif // FILEIO_H

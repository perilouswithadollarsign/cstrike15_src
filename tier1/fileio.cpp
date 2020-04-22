//========= Copyright 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: A collection of utility classes to simplify file I/O, and
//			as much as possible contain portability problems. Here avoiding 
//			including windows.h.
//
//=============================================================================

#if defined(_WIN32)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0502		// ReadDirectoryChangesW
#endif

#include <sys/stat.h>

#if defined(OSX)
#include <CoreServices/CoreServices.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/time.h>
#endif

#define ASYNC_FILEIO
#if defined( LINUX ) || defined ( OSX )
// Linux hasn't got a good AIO library that we have found yet, so lets punt for now
#undef ASYNC_FILEIO
#endif

#if defined(_WIN32)
//#include <direct.h>
#include <io.h>
// unset to force to use stdio implementation 
#define WIN32_FILEIO

#if defined(ASYNC_FILEIO) 
#if defined(_WIN32) && !defined(WIN32_FILEIO)
#error "trying to use async io without win32 filesystem API usage, that isn't doable"
#endif
#endif

#else /* not defined (_WIN32) */
#include <utime.h>
#include <dirent.h>
#include <unistd.h> // for unlink
#include <limits.h> // defines PATH_MAX
#include <alloca.h> // 'cause we like smashing the stack
#if defined( _PS3 )
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#include <sys/statvfs.h>
#endif
#include <sched.h>
#define int64 int64_t

// On OSX the native API file offset is always 64-bit
// and things like stat64 are deprecated.
// PS3 doesn't have anything other than the native API.
#if defined(OSX) || defined(_PS3)
typedef off_t offBig_t;
typedef struct stat statBig_t;
typedef struct statvfs statvfsBig_t;
typedef struct dirent direntBig_t;
#define openBig open
#define lseekBig lseek
#define preadBig pread
#define pwriteBig pwrite
#define statBig stat
#define lstatBig lstat
#define readdirBig readdir
#define scandirBig scandir
#define alphasortBig alphasort
#define fopenBig fopen
#define fseekBig fseeko
#define ftellBig ftello
#define ftruncateBig ftruncate
#define fstatBig fstat
#define statvfsBig statvfs
#define mmapBig mmap
#else
// Use the 64-bit file I/O API.
typedef off64_t offBig_t;
typedef struct stat64 statBig_t;
typedef struct statvfs64 statvfsBig_t;
typedef struct dirent64 direntBig_t;
#define openBig open64
#define lseekBig lseek64
#define preadBig pread64
#define pwriteBig pwrite64
#define statBig stat64
#define lstatBig lstat64
#define readdirBig readdir64
#define scandirBig scandir64
#define alphasortBig alphasort64
#define fopenBig fopen64
#define fseekBig fseeko64
#define ftellBig ftello64
#define ftruncateBig ftruncate64
#define fstatBig fstat64
#define statvfsBig statvfs64
#define mmapBig mmap64
#endif

struct _finddata_t
{   
	_finddata_t()
	{
		name[0] = '\0';
		dirBase[0] = '\0';
		curName = 0;
		numNames = 0;
		namelist = NULL;
	}
	// public data
	char name[PATH_MAX]; // the file name returned from the call
	char dirBase[PATH_MAX];
	offBig_t size;
	mode_t attrib;
	time_t time_write;
	time_t time_create;
	int curName;
	int numNames;
	direntBig_t **namelist;
};

#define _A_SUBDIR S_IFDIR

// FUTURE map _A_HIDDEN via checking filename against .*
#define _A_HIDDEN 0

// FUTURE check 'read only' by checking mode against S_IRUSR
#define _A_RDONLY 0

// no files under posix are 'system' or 'archive'
#define _A_SYSTEM 0
#define _A_ARCH   0

int _findfirst( const char *pchBasePath, struct _finddata_t *pFindData );
int _findnext( const int64 hFind, struct _finddata_t *pFindData );
bool _findclose( int64 hFind );
static int FileSelect( const char *name, const char *mask );

#endif 

#include "tier1/fileio.h"
#include "tier1/utlbuffer.h"
#include "tier1/strtools.h"
#include <errno.h>
#include "vstdlib/vstrtools.h"

#if defined( WIN32_FILEIO )
#include "winlite.h"
#endif

#if defined( ASYNC_FILEIO )
#ifdef _WIN32
#include "winlite.h"
#elif defined(_PS3)
// bugbug ps3 - see some aio files under libfs.. skipping for the moment
#elif defined(POSIX)
#include <aio.h>
#else 
#error "aio please"
#endif
#endif

#if defined ( POSIX )
#define INVALID_HANDLE_VALUE NULL

#define _rmdir rmdir

#if !defined( _PS3 )
#define _S_IREAD S_IREAD
#define _S_IWRITE S_IWRITE
#else
#define _S_IREAD S_IRUSR
#define _S_IWRITE S_IWUSR
#endif

#endif

#define PvAlloc( cub )  malloc( cub )
#define PvRealloc( pv, cub ) realloc( pv, cub )
#define FreePv( pv ) free( pv )

//-----------------------------------------------------------------------------
// Purpose: Constructor from UTF8
//-----------------------------------------------------------------------------
CPathString::CPathString( const char *pchUTF8Path )
{
	// Need to first turn into an absolute path, so \\?\ pre-pended paths will be ok
	m_pchUTF8Path = new char[ MAX_UNICODE_PATH_IN_UTF8 ];
	m_pwchWideCharPathPrepended = NULL;

	// First, convert to absolute path, which also does Q_FixSlashes for us.
	Q_MakeAbsolutePath( m_pchUTF8Path, MAX_UNICODE_PATH * 4, pchUTF8Path );

	// Second, fix any double slashes
	V_FixDoubleSlashes( m_pchUTF8Path );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPathString::~CPathString()
{
	if ( m_pwchWideCharPathPrepended )
	{
		delete[] m_pwchWideCharPathPrepended;
		m_pwchWideCharPathPrepended = NULL;
	}

	if ( m_pchUTF8Path )
	{
		delete[] m_pchUTF8Path;
		m_pchUTF8Path = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Access UTF8 path
//-----------------------------------------------------------------------------
const char * CPathString::GetUTF8Path()
{
	return m_pchUTF8Path;
}


//-----------------------------------------------------------------------------
// Purpose: Gets wchar_t based path, with \\?\ pre-pended (allowing long paths 
// on Win32, should only be used with unicode extended path aware filesystem calls)
//-----------------------------------------------------------------------------
const wchar_t *CPathString::GetWCharPathPrePended()
{
	PopulateWCharPath();
	return m_pwchWideCharPathPrepended; 
}


//-----------------------------------------------------------------------------
// Purpose: Builds wchar path string
//-----------------------------------------------------------------------------
void CPathString::PopulateWCharPath()
{
	if ( m_pwchWideCharPathPrepended )
		return;

	// Check if the UTF8 path starts with \\, which on Win32 means it's a UNC path, and then needs a different prefix
	if ( m_pchUTF8Path[0] == '\\' && m_pchUTF8Path[1] == '\\' )
	{
		m_pwchWideCharPathPrepended = new wchar_t[MAX_UNICODE_PATH+8];
		Q_memcpy( m_pwchWideCharPathPrepended, L"\\\\?\\UNC\\", 8*sizeof(wchar_t) );
#ifdef DBGFLAG_ASSERT
		int cchResult =
#endif
			Q_UTF8ToUnicode( m_pchUTF8Path+2, m_pwchWideCharPathPrepended+8, MAX_UNICODE_PATH*sizeof(wchar_t) );
		Assert( cchResult );

		// Be sure we NULL terminate within our allocated region incase Q_UTF8ToUnicode failed, though we're already in bad shape then.
		m_pwchWideCharPathPrepended[MAX_UNICODE_PATH+7] = 0;
	}
	else
	{
		m_pwchWideCharPathPrepended = new wchar_t[MAX_UNICODE_PATH+4];
		Q_memcpy( m_pwchWideCharPathPrepended, L"\\\\?\\", 4*sizeof(wchar_t) );
#ifdef DBGFLAG_ASSERT
		int cchResult =
#endif
			Q_UTF8ToUnicode( m_pchUTF8Path, m_pwchWideCharPathPrepended+4, MAX_UNICODE_PATH*sizeof(wchar_t) );
		Assert( cchResult );

		// Be sure we NULL terminate within our allocated region incase Q_UTF8ToUnicode failed, though we're already in bad shape then.
		m_pwchWideCharPathPrepended[MAX_UNICODE_PATH+3] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Helper on PS3 to find next entry that matches the provided pattern
//-----------------------------------------------------------------------------
#if defined( _PS3 )
bool CDirIterator::BFindNextPS3()
{
	while (true)
	{
		uint32 unDataCount = 0;
		if (cellFsGetDirectoryEntries( m_hFind, m_pDirEntry, sizeof(CellFsDirectoryEntry), &unDataCount ) != CELL_FS_SUCCEEDED || unDataCount == 0)
			return false;

		// if we found a new file/directory, need to make sure it matches our desired pattern
		if (FileSelect( m_pDirEntry->entry_name.d_name, m_strPattern.String() ) != 0)
			return true;
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
#if defined( _PS3 )

CDirIterator::CDirIterator( const char *pchPath, const char *pchPattern )
{
	// init for failure
	m_bOpenHandle = false;
	m_bNoFiles = true;
	m_bUsedFirstFile = true;

	// always create a new entry.. matches win32/posix (guessing so BCurrent functions won't crash?)
	m_pDirEntry = new CellFsDirectoryEntry;
	memset( m_pDirEntry, 0, sizeof(CellFsDirectoryEntry) );

	if (!pchPath || !pchPattern)
		return;

	// fix up path
	CPathString strPath( pchPath );

	// save pattern
	m_strPattern = pchPattern;

	// we have a path.. init
	CellFsErrno e = cellFsOpendir( strPath.GetUTF8Path(), &m_hFind );
	if (e != CELL_FS_SUCCEEDED)
		return;

	m_bOpenHandle = true;

	// find first entry
	if (!BFindNextPS3())
		return;

	// found at least 1 file
	m_bNoFiles = false;

	// if we're pointing at . or .., set it as used
	// so we'll look for the next item when BNextFile() is called
	m_bUsedFirstFile = !BValidFilename();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDirIterator::~CDirIterator()
{
	if (m_bOpenHandle)
		cellFsClosedir( m_hFind );

	if (m_pDirEntry)
		delete m_pDirEntry;
}


#else

CDirIterator::CDirIterator( const char *pchPath, const char *pchPattern )
{
	CPathString strPath( pchPath );
	m_pFindData = NULL;

	// +2 so we can potentially add path separator as well as null termination
	char *pchPathAndPattern = new char[Q_strlen( strPath.GetUTF8Path() ) + Q_strlen( pchPattern ) + 2];

	// be resilient about whether the caller passes us a path with a terminal path separator or not.

	// put in the path
	if (pchPath)
	{
		Q_strncpy( pchPathAndPattern, strPath.GetUTF8Path(), Q_strlen( strPath.GetUTF8Path() ) + 1 );

		// identify whether we've got a terminal separator. add one if not.
		char *pchRest = pchPathAndPattern + Q_strlen( pchPathAndPattern ) - 1;
		if (*pchRest != CORRECT_PATH_SEPARATOR)
		{
			*++pchRest = CORRECT_PATH_SEPARATOR;
		}
		pchRest++;

		// now put in the search pattern.
		Q_strncpy( pchRest, pchPattern, Q_strlen( pchPattern ) + 1 );

		Init( pchPathAndPattern );
	}
	else
	{
		pchPathAndPattern[0] = 0;
		m_bNoFiles = true;
		m_bUsedFirstFile = true;

#if defined(_WIN32)
		m_hFind = INVALID_HANDLE_VALUE;
		m_pFindData = new WIN32_FIND_DATAW;
		m_rgchFileName[0] = 0;
#else
		m_hFind = -1;
		m_pFindData = new _finddata_t;
#endif
		memset( m_pFindData, 0, sizeof(*m_pFindData) );

	}
	delete[] pchPathAndPattern;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDirIterator::CDirIterator( const char *pchSearchPath )
{
	Init( pchSearchPath );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize iteration structure
//-----------------------------------------------------------------------------
void CDirIterator::Init( const char *pchSearchPath )
{
	CPathString strBasePath( pchSearchPath );

#if defined(_WIN32)
	m_pFindData = new WIN32_FIND_DATAW;
	memset( m_pFindData, 0, sizeof(*m_pFindData) );

	m_rgchFileName[0] = 0;
	m_hFind = FindFirstFileW( strBasePath.GetWCharPathPrePended(), m_pFindData );
	bool bSuccess = (m_hFind != INVALID_HANDLE_VALUE);
	// Conversion should never fail with valid filenames...
	if (bSuccess && !Q_UnicodeToUTF8( m_pFindData->cFileName, m_rgchFileName, sizeof(m_rgchFileName) ))
	{
		AssertMsg( false, "Q_UnicodeToUTF8 failed on m_pFindData->cFileName in CDirIterator" );
		bSuccess = false;
	}
#else
	m_pFindData = new _finddata_t;
	memset( m_pFindData, 0, sizeof(*m_pFindData) );

	m_hFind = _findfirst( strBasePath.GetUTF8Path(), m_pFindData );
	bool bSuccess = (m_hFind != -1);
#endif

	if (!bSuccess)
	{
		m_bNoFiles = true;
		m_bUsedFirstFile = true;
	}
	else
	{
		m_bNoFiles = false;
		// if we're pointing at . or .., set it as used
		// so we'll look for the next item when BNextFile() is called
		m_bUsedFirstFile = !BValidFilename();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDirIterator::~CDirIterator()
{
#if defined(_WIN32)
	if (m_hFind != INVALID_HANDLE_VALUE)
	{
		FindClose( m_hFind );
	}
	delete m_pFindData;
#else
	if (m_hFind != -1)
	{
		_findclose( m_hFind );
	}
	if (m_pFindData)
	{
		for (int i = 0; i < m_pFindData->numNames; i++)
		{
			// scandir allocates with malloc, so free with free
			free( m_pFindData->namelist[i] );
		}
		free( m_pFindData->namelist );
		delete m_pFindData;
	}
#endif
}

#endif // _PS3

//-----------------------------------------------------------------------------
// Purpose: Check for successful construction
//-----------------------------------------------------------------------------
bool CDirIterator::IsValid() const
{
#if defined(_WIN32)
	return m_hFind != INVALID_HANDLE_VALUE;
#elif defined(_PS3)
	return m_bOpenHandle;
#else
	return m_hFind != -1;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Filter out . and ..
//-----------------------------------------------------------------------------
bool CDirIterator::BValidFilename()
{
#if defined( _WIN32 )
	const char *pch = m_rgchFileName;
#elif defined( _PS3 )
	const char *pch = m_pDirEntry->entry_name.d_name;
#else
	const char *pch = m_pFindData->name;
#endif

	if ((pch[0] == '.' && pch[1] == 0) ||
		(pch[0] == '.' && pch[1] == '.' && pch[2] == 0))
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if there is a file to read
//-----------------------------------------------------------------------------
bool CDirIterator::BNextFile()
{
	if (m_bNoFiles)
		return false;

	// use the first result
	if (!m_bUsedFirstFile)
	{
		m_bUsedFirstFile = true;
		return true;
	}

	// find the next item
	for (;;)
	{
#if defined( _WIN32 )
		bool bFound = (FindNextFileW( m_hFind, m_pFindData ) != FALSE);
		// Conversion should never fail with valid filenames...
		if (bFound && !Q_UnicodeToUTF8( m_pFindData->cFileName, m_rgchFileName, sizeof(m_rgchFileName) ))
		{
			AssertMsg( false, "Q_UnicodeToUTF8 failed on m_pFindData->cFileName in CDirIterator" );
			bFound = false;
		}
#elif defined( _PS3 )
		bool bFound = BFindNextPS3();
#else
		bool bFound = (_findnext( m_hFind, m_pFindData ) == 0);
#endif

		if (!bFound)
		{
			// done
			m_bNoFiles = true;
			return false;
		}

		// skip over the '.' and '..' paths
		if (!BValidFilename())
			continue;

		break;
	}

	// have one more file
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns name (filename portion only) of the current file.
// Name is emitted in UTF-8 encoding.
// NOTE: This method returns a pointer into a static buffer, either a member
// or the buffer inside the _finddata_t.
//-----------------------------------------------------------------------------
const char *CDirIterator::CurrentFileName()
{
#if defined( _WIN32 )
	return m_rgchFileName;
#elif defined( _PS3 )
	return m_pDirEntry->entry_name.d_name;
#else
	return m_pFindData->name;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: returns size of the file
//-----------------------------------------------------------------------------
int64 CDirIterator::CurrentFileLength() const
{
#if defined( _WIN32 )
	LARGE_INTEGER li = { { m_pFindData->nFileSizeLow, m_pFindData->nFileSizeHigh } };
	return li.QuadPart;
#elif defined( _PS3 )
	return m_pDirEntry->attribute.st_size;
#else
	return (int64)m_pFindData->size;
#endif
}

#if defined( _WIN32 )
//-----------------------------------------------------------------------------
// Purpose: utility for converting a system filetime to a regular time
//-----------------------------------------------------------------------------
time64_t FileTimeToUnixTime( FILETIME filetime )
{
	long long int t = filetime.dwHighDateTime;
	t <<= 32;
	t += (unsigned long)filetime.dwLowDateTime;
	t -= 116444736000000000LL;
	return t / 10000000;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: returns last write time of the file
//-----------------------------------------------------------------------------
time64_t CDirIterator::CurrentFileWriteTime() const
{
#if defined( _WIN32 )
	return FileTimeToUnixTime( m_pFindData->ftLastWriteTime );
#elif defined( _PS3 )
	return m_pDirEntry->attribute.st_mtime;
#else
	return m_pFindData->time_write;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: returns the creation time of the file
//-----------------------------------------------------------------------------
time64_t CDirIterator::CurrentFileCreateTime() const
{
#if defined( _WIN32 )
	return FileTimeToUnixTime( m_pFindData->ftCreationTime );
#elif defined( _PS3 )
	return m_pDirEntry->attribute.st_ctime;
#else
	return m_pFindData->time_create;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: returns whether current item under examination is a directory
//-----------------------------------------------------------------------------
bool CDirIterator::BCurrentIsDir() const
{
#if defined( _WIN32 )
	return (m_pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#elif defined( _PS3 )
	return (m_pDirEntry->attribute.st_mode & CELL_FS_S_IFDIR ? true : false);
#else
	return (m_pFindData->attrib & _A_SUBDIR ? true : false);
#endif
}


//-----------------------------------------------------------------------------
// Purpose: returns whether current item under examination is a hidden file
//-----------------------------------------------------------------------------
bool CDirIterator::BCurrentIsHidden() const
{
#if defined( _WIN32 )
	return (m_pFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
#elif defined( _PS3 )
	return false;
#else
	return (m_pFindData->attrib & _A_HIDDEN ? true : false);
#endif
}


//-----------------------------------------------------------------------------
// Purpose: returns whether current item under examination is read-only
//-----------------------------------------------------------------------------
bool CDirIterator::BCurrentIsReadOnly() const
{
#if defined( _WIN32 )
	return (m_pFindData->dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
#elif defined( _PS3 )
	// assume this is windows version of read only.. can execute. Is it writable?
	return (m_pDirEntry->attribute.st_mode & CELL_FS_S_IWUSR == 0);
#else
	return (m_pFindData->attrib & _A_RDONLY ? true : false);
#endif
}


//-----------------------------------------------------------------------------
// Purpose: returns whether current item under examination is marked as a system file
//-----------------------------------------------------------------------------
bool CDirIterator::BCurrentIsSystem() const
{
#if defined( _WIN32 )
	return (m_pFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
#elif defined( _PS3 )
	return false;
#else
	return (m_pFindData->attrib & _A_SYSTEM ? true : false);
#endif
}


//-----------------------------------------------------------------------------
// Purpose: returns whether current item under examination is marked for archiving
//-----------------------------------------------------------------------------
bool CDirIterator::BCurrentIsMarkedForArchive() const
{
#if defined( _WIN32 )
	return (m_pFindData->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) != 0;
#elif defined( _PS3 )
	return false;
#else
	return (m_pFindData->attrib & _A_ARCH ? true : false);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFileWriter::CFileWriter( bool bAsync ) 
{ 
#ifdef ASYNC_FILEIO
    m_bDefaultAsync = bAsync;
#else
    m_bDefaultAsync = false;
#endif
    m_hFileDest = INVALID_HANDLE_VALUE; 
    m_bAsync = m_bDefaultAsync; 
	m_cPendingCallbacksFromOtherThreads = 0;
    m_cubOutstanding = 0;
    m_cubWritten = 0;
    m_unThreadID = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFileWriter::~CFileWriter() 
{ 
    Close(); 
}


#ifdef ASYNC_FILEIO
#ifdef _WIN32
// our own version of overlapped structure passed through async writes
struct FileWriterOverlapped_t : public OVERLAPPED
{
    CFileWriter *m_pFileWriter;
    void *m_pvData;
    size_t m_cubData;
};
#elif defined(_PS3)
// bugbug ps3 - impement?
#elif defined(POSIX)
// our own version of overlapped structure passed through async writes
struct FileWriterOverlapped_t : public aiocb
{
    CFileWriter *m_pFileWriter;
    void *m_pvData;
    size_t m_cubData;
};
#else
#error "struct me"
#endif
#endif


//-----------------------------------------------------------------------------
// Purpose: sets which file to write to
//-----------------------------------------------------------------------------
bool CFileWriter::BSetFile( const char *pchFile, bool bAllowOpenExisting )
{
    CPathString strPath( pchFile );

    // make sure the full path to file exists
    CUtlString strCopyUTF8 = strPath.GetUTF8Path();
    Q_StripFilename( const_cast<char*>(strCopyUTF8.Access()) );
    CreateDirRecursive( strCopyUTF8.Access() );
    
    Close();
    m_bAsync = m_bDefaultAsync;
    m_cubWritten = 0;
	m_cubOutstanding = 0;
	m_unThreadID = 0;
	m_cPendingCallbacksFromOtherThreads = 0;

#ifdef _WIN32
    DWORD dwFlags = FILE_ATTRIBUTE_NORMAL;
    if ( m_bAsync )
        dwFlags |= FILE_FLAG_OVERLAPPED;

    // First try to open existing file, if specified that we should allow that
    if ( bAllowOpenExisting )
    {
        m_hFileDest = ::CreateFileW( strPath.GetWCharPathPrePended(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, dwFlags, NULL );
        if ( m_hFileDest == INVALID_HANDLE_VALUE )
        {
            // clear overlapped and try again
            dwFlags &= ~FILE_FLAG_OVERLAPPED;
            m_hFileDest = ::CreateFileW( strPath.GetWCharPathPrePended(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, dwFlags, NULL );
            if ( m_hFileDest != INVALID_HANDLE_VALUE )
            {
                m_bAsync = false;
            }
        }

        if ( m_hFileDest != INVALID_HANDLE_VALUE )
        {
            LARGE_INTEGER liOffset;
            liOffset.QuadPart = 0;
            LARGE_INTEGER liFilePtr;

            ::SetFilePointerEx( m_hFileDest, liOffset, &liFilePtr, FILE_END );
            m_cubWritten = liFilePtr.QuadPart;
        }
    }

    // If we didn't already open existing, then move on to creation
    if ( m_hFileDest == INVALID_HANDLE_VALUE )
    {
        // make sure the full path to file exists
        CUtlString strPathCopyUTF8 = strPath.GetUTF8Path();
        Q_StripFilename( const_cast<char*>(strPathCopyUTF8.Access()) );
        CreateDirRecursive( strPathCopyUTF8.Access() );

        // Reset back to try overlapped below incase we tried without above
        if ( m_bAsync )
            dwFlags |= FILE_FLAG_OVERLAPPED;

        m_hFileDest = ::CreateFileW( strPath.GetWCharPathPrePended(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, dwFlags, NULL );

        if ( m_hFileDest == INVALID_HANDLE_VALUE )
        {
            // clear overlapped and try again
            m_bAsync = false;
            dwFlags &= ~FILE_FLAG_OVERLAPPED;
            m_hFileDest = ::CreateFileW( strPath.GetWCharPathPrePended(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, dwFlags, NULL );
            if ( m_hFileDest == INVALID_HANDLE_VALUE )
                return false;
        }
    }

#elif defined(POSIX)

    int flags = O_WRONLY;
    if ( bAllowOpenExisting )
        flags |= O_CREAT;
    else
        flags |= O_CREAT | O_TRUNC;

    m_hFileDest = (HANDLE)open( strPath.GetUTF8Path(), flags, S_IRWXU );
    if ( bAllowOpenExisting )
    {
        off_t offset = lseek( (intptr_t)m_hFileDest, 0, SEEK_END );
        m_cubWritten = offset;
    }
#else
#error
#endif
    
    m_unThreadID = ThreadGetCurrentId();
    return ( m_hFileDest != INVALID_HANDLE_VALUE );
}


void CFileWriter::Sleep( uint nMSec )
{
#ifdef _WIN32
    ::SleepEx( nMSec, TRUE );
#elif PLATFORM_PS3
    sys_timer_usleep( nMSec * 1000 );
#elif defined(POSIX)
    if ( nMSec == 0 )
        sched_yield();
    else 
        usleep( nMSec * 1000 );
#else
#error
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Seeks to a specific location in the file
//-----------------------------------------------------------------------------
bool CFileWriter::Seek( uint64 offset, ESeekOrigin eOrigin )
{
    if ( m_bAsync )
    {
        AssertMsg( false, "Seeking to a position not supported with async io" );
        return false;
    }

    bool bSuccess = false;

#ifdef _WIN32
    DWORD dwMoveMethod = FILE_BEGIN;
    switch( eOrigin )
    {
    case k_ESeekCur:
        dwMoveMethod = FILE_CURRENT;
        break;
    case k_ESeekEnd:
        dwMoveMethod = FILE_END;
        break;
    default:
        dwMoveMethod = FILE_BEGIN;
    }

    LARGE_INTEGER largeIntOffset;
    largeIntOffset.QuadPart = offset;

    if ( ::SetFilePointerEx( m_hFileDest, largeIntOffset, NULL, dwMoveMethod ) )
        bSuccess = true;

#elif defined(POSIX)
    int orgin = SEEK_SET;
    switch( eOrigin )
    {
    case k_ESeekCur:
        orgin = SEEK_CUR;
        break;
    case k_ESeekEnd:
        orgin = SEEK_END;
        break;
    default:
        orgin = SEEK_SET;
    }

    // fseeko will work on 64 bit file offsets if _FILE_OFFSET_BITS 64 is defined, is this the best way
    // to do this on posix builds?
    bSuccess = lseek( (intptr_t)m_hFileDest, (off_t)offset, orgin ) != -1;
#else
#error
#endif

    return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: posts a buffer to be written to the file
//-----------------------------------------------------------------------------
bool CFileWriter::Write( const void *pvData, uint32 cubData )
{
    if  ( cubData == 0 )
        return true;

#if defined( _PS3 )
    if ( write( (int)m_hFileDest, pvData, cubData ) == cubData )
        return true;

    return false;
#else
    BOOL bRet = 0;
#ifdef ASYNC_FILEIO
    if ( m_bAsync )
    {
		// get any outstanding write callbacks
		if ( m_cubOutstanding > 0 )
		{
			::SleepEx( 0, TRUE );
		}

		// make sure we don't have too much data outstanding
		while ( m_cubOutstanding > (10*k_nMegabyte) )
		{
			::SleepEx( 10, TRUE );
		}
	
        // build the overlapped info that will get passed through the write
        FileWriterOverlapped_t *pFileWriterOverlapped = new FileWriterOverlapped_t;
        memset( pFileWriterOverlapped, 0x0, sizeof(FileWriterOverlapped_t) );
        pFileWriterOverlapped->m_pFileWriter = this;
        pFileWriterOverlapped->m_pvData = PvAlloc( cubData );
        pFileWriterOverlapped->m_cubData = cubData;
        memcpy( pFileWriterOverlapped->m_pvData, pvData, cubData );

        // work out where to write to
#ifdef _WIN32
        pFileWriterOverlapped->Offset = ( uint32 ) ( m_cubWritten & 0xffffffff );
        pFileWriterOverlapped->OffsetHigh = ( uint32 ) ( m_cubWritten >> 32 );
#elif defined(POSIX)
        pFileWriterOverlapped->aio_offset = m_cubWritten;
        pFileWriterOverlapped->aio_buf = pFileWriterOverlapped->m_pvData;
        pFileWriterOverlapped->aio_nbytes = pFileWriterOverlapped->m_cubData;
        pFileWriterOverlapped->aio_fildes = (int)m_hFileDest;
    
        /* Link the AIO request with a thread callback */
        pFileWriterOverlapped->aio_sigevent.sigev_notify = SIGEV_THREAD;
        pFileWriterOverlapped->aio_sigevent.sigev_notify_function = &CFileWriter::ThreadedWriteFileCompletionFunc;
        pFileWriterOverlapped->aio_sigevent.sigev_notify_attributes = NULL;
        pFileWriterOverlapped->aio_sigevent.sigev_value.sival_ptr = pFileWriterOverlapped;
                 
#else
#error
#endif

      
#ifdef _WIN32
        // post write
        bRet = ::WriteFileEx( m_hFileDest, pFileWriterOverlapped->m_pvData, cubData, pFileWriterOverlapped, &CFileWriter::ThreadedWriteFileCompletionFunc );
    
#elif defined(POSIX)
        bRet = aio_write( pFileWriterOverlapped );
        bRet = !bRet; // aio_read returns 0 on success, this func returns success if bRet != 0
#else
#error
#endif
        if ( bRet )
		{
			ThreadInterlockedExchangeAdd( &m_cubOutstanding, cubData );
		
			if ( ThreadGetCurrentId() != m_unThreadID  )
			{
				// this is not the main thread so we have to wait here 
				ThreadInterlockedIncrement( &m_cPendingCallbacksFromOtherThreads );

				while ( m_cPendingCallbacksFromOtherThreads )
				{
					// we have to wait here since the OS can signal us only
					// on this current thread
					::SleepEx( 10, TRUE );
				}
			}
		}
    }
    else
#endif // ASYNC_FILEIO
    {
#ifdef _WIN32
        // normal write
        DWORD dwBytesWritten = 0;
        ::WriteFile( m_hFileDest, pvData, cubData, &dwBytesWritten, NULL );
        bRet = ( dwBytesWritten == cubData );
#elif defined(POSIX)
        bRet = write( (intptr_t)m_hFileDest, pvData, cubData );
#else
#error
#endif
    }

    // increment
    m_cubWritten += cubData;

    return ( bRet != 0 );
#endif // _PS3
}

//-----------------------------------------------------------------------------
// Purpose: Convenient printf with no dynamic memory allocation
//-----------------------------------------------------------------------------
int CFileWriter::Printf( char *pDest, int bufferLen, char const *pFormat, ... )
{
	va_list marker;

	va_start( marker, pFormat );
	// _vsnprintf will not write a terminator if the output string uses the entire buffer you provide
	int len = _vsnprintf( pDest, bufferLen-1, pFormat, marker );
	va_end( marker );

	// Len < 0 represents an overflow on windows; len > buffer length on posix
	if (( len < 0 ) || (len >= bufferLen ) )
	{
		len = bufferLen-1;
	}
	pDest[len] = 0;

	if ( !Write( pDest, len	) )
		return 0;

	return len;
}


//-----------------------------------------------------------------------------
// Purpose: ensures any writes have been completed
//-----------------------------------------------------------------------------
void CFileWriter::Flush()
{
#ifdef WIN32
    FlushFileBuffers( m_hFileDest );
#endif

    if ( m_unThreadID == ThreadGetCurrentId() )
	{
		// wait for all writes to be complete
		int cWaits = 0;
		const int k_nMaxWaits = 60000; /* roughly one minute */

		while ( m_cubOutstanding && cWaits < k_nMaxWaits   )
		{
			Sleep( 10 );
			cWaits++;
		}
		AssertMsg1( cWaits < k_nMaxWaits, "Waited 60k iterations in CFileWriter::Flush - m_cubOutstanding = %u", m_cubOutstanding );
	}
}


//-----------------------------------------------------------------------------
// Purpose: check if file is open
//-----------------------------------------------------------------------------
bool CFileWriter::BFileOpen()
{
    if ( m_hFileDest != INVALID_HANDLE_VALUE )
        return true;

    return false;
}


//-----------------------------------------------------------------------------
// Purpose: closes the file
//-----------------------------------------------------------------------------
void CFileWriter::Close()
{
    if ( m_hFileDest != INVALID_HANDLE_VALUE )
    {
		Flush();

		// temp handle to avoid double close in threaded environment
		HANDLE hFileDest = m_hFileDest;
       	m_hFileDest = INVALID_HANDLE_VALUE; 
#ifdef _WIN32
        ::CloseHandle( hFileDest );
#elif defined(POSIX)
        close( (intptr_t)hFileDest );
#else
#error
#endif
    }

	// Close has to be called from thread that called BSetFile
	Assert( m_cPendingCallbacksFromOtherThreads == 0 );
}

//-----------------------------------------------------------------------------
// Purpose: async callback for when a file write has completed
//-----------------------------------------------------------------------------
#ifdef ASYNC_FILEIO
#ifdef _WIN32
void CFileWriter::ThreadedWriteFileCompletionFunc( unsigned long dwErrorCode, unsigned long dwBytesTransfered, struct _OVERLAPPED *pOverlapped )
{
	FileWriterOverlapped_t *pFileWriterOverlapped = (FileWriterOverlapped_t *)pOverlapped;
	ThreadInterlockedExchangeAdd( &pFileWriterOverlapped->m_pFileWriter->m_cubOutstanding, (int)(0-pFileWriterOverlapped->m_cubData) );
	if ( pFileWriterOverlapped->m_pFileWriter->m_unThreadID != ThreadGetCurrentId() )
	{
		// this was not the main thread, reduce counter
		ThreadInterlockedDecrement( &pFileWriterOverlapped->m_pFileWriter->m_cPendingCallbacksFromOtherThreads ); 
	}
	FreePv( pFileWriterOverlapped->m_pvData );
	delete pFileWriterOverlapped;


}
#elif defined( _PS3 )
// bugbug PS3
#elif defined(POSIX)
void CFileWriter::ThreadedWriteFileCompletionFunc( sigval sigval )
{
	FileWriterOverlapped_t *pFileWriterOverlapped = (FileWriterOverlapped_t *)sigval.sival_ptr;
	if ( aio_error( pFileWriterOverlapped ) == 0 ) 
	{
		uint nBytesWrite = aio_return( pFileWriterOverlapped );
		Assert( nBytesWrite == pFileWriterOverlapped->m_cubData );

		pFileWriterOverlapped->m_pFileWriter->m_cOutstandingWrites--;
		pFileWriterOverlapped->m_pFileWriter->m_cubOutstanding += ( 0 - pFileWriterOverlapped->m_cubData );
		FreePv( pFileWriterOverlapped->m_pvData );
		delete pFileWriterOverlapped;
	}
}
#else
#error
#endif
#endif // ASYNC_FILEIO


#ifdef WIN32
struct DirWatcherOverlapped : public OVERLAPPED
{
	CDirWatcher *m_pDirWatcher;
};
#endif

#if !defined(_PS3) && !defined(_X360)
// a buffer full of file names
static const int k_cubDirWatchBufferSize = 8 * 1024;


//-----------------------------------------------------------------------------
// Purpose: directory watching
//-----------------------------------------------------------------------------
CDirWatcher::CDirWatcher()
{
	m_hFile = NULL;
	m_pOverlapped = NULL;
	m_pFileInfo = NULL;
#ifdef OSX
	m_WatcherStream = 0;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: directory watching
//-----------------------------------------------------------------------------
CDirWatcher::~CDirWatcher()
{
#ifdef WIN32
	if ( m_pOverlapped )
	{
		// mark the overlapped structure as gone
		DirWatcherOverlapped *pDirWatcherOverlapped = (DirWatcherOverlapped *)m_pOverlapped;
		pDirWatcherOverlapped->m_pDirWatcher = NULL;
	}

	if ( m_hFile )
	{
		// make sure we flush any pending I/O's on the handle
		::CancelIo( m_hFile );
		::SleepEx( 0, TRUE );
		// close the handle
		::CloseHandle( m_hFile );
	}
#elif defined(OSX)
	if ( m_WatcherStream )
	{
		FSEventStreamStop( (FSEventStreamRef)m_WatcherStream );
		FSEventStreamInvalidate( (FSEventStreamRef)m_WatcherStream );
		FSEventStreamRelease( (FSEventStreamRef)m_WatcherStream );		
		m_WatcherStream = 0;
	}
#endif
	if ( m_pFileInfo )
	{
		free( m_pFileInfo );
	}
	if ( m_pOverlapped )
	{
		free( m_pOverlapped );
	}
}


#ifdef WIN32
//-----------------------------------------------------------------------------
// Purpose: callback watch
//			gets called on the same thread whenever a SleepEx() occurs
//-----------------------------------------------------------------------------
class CDirWatcherFriend
{
public:
	static void WINAPI DirWatchCallback( DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, OVERLAPPED *pOverlapped )
	{
		DirWatcherOverlapped *pDirWatcherOverlapped = (DirWatcherOverlapped *)pOverlapped;

		// see if we've been cancelled
		if ( !pDirWatcherOverlapped->m_pDirWatcher )
			return;

		// parse and pass back
		if ( dwNumberOfBytesTransfered > sizeof(FILE_NOTIFY_INFORMATION) )
		{
			FILE_NOTIFY_INFORMATION *pFileNotifyInformation = (FILE_NOTIFY_INFORMATION *)pDirWatcherOverlapped->m_pDirWatcher->m_pFileInfo;
			do 
			{
				// null terminate the string and turn it to UTF-8
				int cNumWChars = pFileNotifyInformation->FileNameLength / sizeof(wchar_t);
				wchar_t *pwchT = new wchar_t[cNumWChars + 1];
				memcpy( pwchT, pFileNotifyInformation->FileName, pFileNotifyInformation->FileNameLength );
				pwchT[cNumWChars] = 0;
				CStrAutoEncode strAutoEncode( pwchT );

				// add it to our list
				pDirWatcherOverlapped->m_pDirWatcher->AddFileToChangeList( strAutoEncode.ToString() );
				delete[] pwchT;
				if ( pFileNotifyInformation->NextEntryOffset == 0 )
					break;

				// move to the next file
				pFileNotifyInformation = (FILE_NOTIFY_INFORMATION *)(((byte*)pFileNotifyInformation) + pFileNotifyInformation->NextEntryOffset);
			} while ( 1 );
		}


		// watch again
		pDirWatcherOverlapped->m_pDirWatcher->PostDirWatch();
	}
};
#elif defined(OSX)
void CheckDirectoryForChanges( const char *path_buff, CDirWatcher *pDirWatch, bool bRecurse )
{
	DIR *dir = opendir(path_buff);
	char fullpath[MAX_PATH];
	struct dirent *dirent;
	struct timespec ts = { 0, 0 };
	bool bTimeSet = false;
	
	while ( (dirent = readdir(dir)) != NULL ) 
	{
		if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
			continue;
		
		snprintf( fullpath, PATH_MAX, "%s/%s", path_buff, dirent->d_name );
		
		struct stat    st;
		if (lstat(fullpath, &st) != 0)
			continue;
		
		if ( S_ISDIR(st.st_mode) && bRecurse )
		{
			CheckDirectoryForChanges( fullpath, pDirWatch, bRecurse );
		}
		else if ( st.st_mtimespec.tv_sec > pDirWatch->m_modTime.tv_sec ||
				 ( st.st_mtimespec.tv_sec == pDirWatch->m_modTime.tv_sec && st.st_mtimespec.tv_nsec > pDirWatch->m_modTime.tv_nsec ) )
		{
			ts = st.st_mtimespec;
			bTimeSet = true;
			// the win32 size only sends up the dir relative to the watching dir, so replicate that here
			pDirWatch->AddFileToChangeList( fullpath + pDirWatch->m_BaseDir.Length() + 1 );
		}
	}

	if ( bTimeSet )
		pDirWatch->m_modTime = ts;
	closedir(dir);	
}

static void fsevents_callback( ConstFSEventStreamRef streamRef, void *clientCallBackInfo, size_t numEvents,void *eventPaths, 
							  const FSEventStreamEventFlags eventMasks[], const FSEventStreamEventId eventIDs[] )
{
    char  path_buff[PATH_MAX];
	for (int i=0; i < numEvents; i++) 
	{
		char **paths = (char **)eventPaths;
		
        strcpy(path_buff, paths[i]);
        int len = strlen(path_buff);
        if (path_buff[len-1] == '/') 
		{
            // chop off a trailing slash
            path_buff[--len] = '\0';
        }
		
		bool bRecurse = false;
		
        if (eventMasks[i] & kFSEventStreamEventFlagMustScanSubDirs
			|| eventMasks[i] & kFSEventStreamEventFlagUserDropped
			|| eventMasks[i] & kFSEventStreamEventFlagKernelDropped) 
		{
            bRecurse = true;
        } 
		
		CDirWatcher *pDirWatch = (CDirWatcher *)clientCallBackInfo;
		// make sure its in our subdir
		if ( !V_strnicmp( path_buff, pDirWatch->m_BaseDir.String(), pDirWatch->m_BaseDir.Length() ) )
			CheckDirectoryForChanges( path_buff, pDirWatch, bRecurse );
    }
}




#endif

//-----------------------------------------------------------------------------
// Purpose: only one directory can be watched at a time
//-----------------------------------------------------------------------------
void CDirWatcher::SetDirToWatch( const char *pchDir )
{
	if ( !pchDir || !*pchDir )
		return;
	
	CPathString strPath( pchDir );
#ifdef WIN32
	// open the directory
	m_hFile = ::CreateFileW( strPath.GetWCharPathPrePended(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS, NULL );

	// create our buffers
	m_pFileInfo = malloc( k_cubDirWatchBufferSize );
	m_pOverlapped = malloc( sizeof( DirWatcherOverlapped ) );

	// post a watch
	PostDirWatch();
#elif defined(OSX)
	CFStringRef mypath = CFStringCreateWithCString( NULL, strPath.GetUTF8Path(), kCFStringEncodingMacRoman );
	if ( !mypath )
	{
		Assert( !"Failed to CFStringCreateWithCString watcher path" );
		return;
	}
	
    CFArrayRef pathsToWatch = CFArrayCreate(NULL, (const void **)&mypath, 1, NULL);
    FSEventStreamContext callbackInfo = {0, this, NULL, NULL, NULL};
    CFAbsoluteTime latency = 1.0; // Latency in seconds

    m_WatcherStream = (void *)FSEventStreamCreate(NULL,
								 &fsevents_callback,
								 &callbackInfo,
								 pathsToWatch,
								 kFSEventStreamEventIdSinceNow, 
								 latency,
								 kFSEventStreamCreateFlagNoDefer
								 );
	
    FSEventStreamScheduleWithRunLoop( (FSEventStreamRef)m_WatcherStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	CFRelease(pathsToWatch );
	CFRelease( mypath );
	
	FSEventStreamStart( (FSEventStreamRef)m_WatcherStream );

	char szFullPath[MAX_PATH];
	Q_MakeAbsolutePath( szFullPath, sizeof(szFullPath), pchDir );
	m_BaseDir = szFullPath;
	
	struct timeval tv;
	gettimeofday( &tv, NULL );
	TIMEVAL_TO_TIMESPEC( &tv, &m_modTime );
		
#else
	Assert( !"Impl me" );
#endif
}


#ifdef WIN32
//-----------------------------------------------------------------------------
// Purpose: used by callback functions to push a file onto the list
//-----------------------------------------------------------------------------
void CDirWatcher::PostDirWatch()
{
	memset( m_pOverlapped, 0, sizeof(DirWatcherOverlapped) );
	DirWatcherOverlapped *pDirWatcherOverlapped = (DirWatcherOverlapped *)m_pOverlapped;
	pDirWatcherOverlapped->m_pDirWatcher = this;

	DWORD dwBytes;
	::ReadDirectoryChangesW( m_hFile, m_pFileInfo, k_cubDirWatchBufferSize, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME, &dwBytes, (OVERLAPPED *)m_pOverlapped, &CDirWatcherFriend::DirWatchCallback );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: used by callback functions to push a file onto the list
//-----------------------------------------------------------------------------
void CDirWatcher::AddFileToChangeList( const char *pchFile )
{
	// make sure it isn't already in the list
	FOR_EACH_LL( m_listChangedFiles, i )
	{
		if ( !Q_stricmp( m_listChangedFiles[i], pchFile ) )
			return;
	}

	m_listChangedFiles.AddToTail( pchFile );
}


//-----------------------------------------------------------------------------
// Purpose: retrieve any changes
//-----------------------------------------------------------------------------
bool CDirWatcher::GetChangedFile( CUtlString *psFile )
{
#ifdef WIN32
	// this will trigger any pending directory reads
	// this does get hit other places in the code; so the callback can happen at any time
	::SleepEx( 0, TRUE );
#endif

	if ( !m_listChangedFiles.Count() )
		return false;

	*psFile = m_listChangedFiles[m_listChangedFiles.Head()];
	m_listChangedFiles.Remove( m_listChangedFiles.Head() );
	return true;
}



#ifdef DBGFLAG_VALIDATE
void CDirWatcher::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	validator.ClaimMemory( m_pOverlapped );
	validator.ClaimMemory( m_pFileInfo );
	ValidateObj( m_listChangedFiles );
	FOR_EACH_LL( m_listChangedFiles, i )
	{
		ValidateObj( m_listChangedFiles[i] );
	}
}
#endif

#endif // _PS3 || _X360

//-----------------------------------------------------------------------------
// Purpose: utility function to create dirs & subdirs
//-----------------------------------------------------------------------------
bool CreateDirRecursive( const char *pchPathIn )
{
	CPathString strPath( pchPathIn );

	// Cast away const, we're going to modify in place even though that's kind of evil
	char *path = (char *)strPath.GetUTF8Path();

	// Does it already exist?
	if ( BFileExists( path ) )
		return true;

	// Walk backwards to first non-existing dir that we find
	char *s = path + Q_strlen(path) - 1;

	while ( s > path )
	{
		if ( *s == CORRECT_PATH_SEPARATOR )
		{
			*s = '\0';
			bool bExists = BFileExists( path );
			*s = CORRECT_PATH_SEPARATOR;

			if ( bExists )
			{
				++s;
				break;
			}
		}
		--s;
	}

	// and then move forwards from there

	while ( *s )
	{
		if ( *s == CORRECT_PATH_SEPARATOR )
		{
			*s = '\0';
			BCreateDirectory( path );
			*s = CORRECT_PATH_SEPARATOR;
		}
		s++;
	}

	if ( !BCreateDirectory( path )  )
	{
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Creates the directory, returning true if it is created, or if it already existed
//-----------------------------------------------------------------------------
bool BCreateDirectory( const char *path )
{
	CPathString pathStr( path );
#ifdef WIN32
	if ( ::CreateDirectoryW( pathStr.GetWCharPathPrePended(), NULL ) )
		return true;

	if ( ::GetLastError() == ERROR_ALREADY_EXISTS )
		return true;

	return false;
#else
	int i = mkdir( pathStr.GetUTF8Path(), S_IRWXU | S_IRWXG | S_IRWXO );
	if ( i == 0 )
		return true;
	if ( errno == EEXIST )
		return true;

	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: make a file writable
//-----------------------------------------------------------------------------
bool MakeFileWriteable( const char *pszFileNameIn )
{
	CPathString strPath( pszFileNameIn );
#if defined( WIN32_FILEIO )
	DWORD dwFileAttributes = ::GetFileAttributesW( strPath.GetWCharPathPrePended() );

	if (dwFileAttributes != INVALID_FILE_ATTRIBUTES)
	{
		// remove flags that make it read only, if necessary
		if (dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
		{
			dwFileAttributes &= ~(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY);
			::SetFileAttributesW( strPath.GetWCharPathPrePended(), dwFileAttributes );
		}
		return true;
	}
	return false;
#else
	statBig_t statBuf;
	if (statBig( strPath.GetUTF8Path(), &statBuf ) != 0)
		return false;
	if (statBuf.st_mode & _S_IWRITE)
		return true;
	int ret = chmod( strPath.GetUTF8Path(), statBuf.st_mode | _S_IWRITE );
	return (ret == 0);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: deletes a file
//-----------------------------------------------------------------------------
bool UnlinkFile( const char *pchFileIn )
{
	CPathString strPath( pchFileIn );
#ifdef _WIN32
	if (::DeleteFileW( strPath.GetWCharPathPrePended() ))
		return true;

	return false;
#else
	return (0 == _unlink( strPath.GetUTF8Path() ));
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Checks if a file exists
// Input:   pchFileName - file name to check existence of (UTF8 - unqualified, relative, or fully qualified)
// Output:  true if successful (file did not exist, or it existed and was deleted);
//          false if unsuccessful (file existed but could not be deleted)
//-----------------------------------------------------------------------------
bool BFileExists( const char *pchFileNameIn )
{
	CPathString strPath( pchFileNameIn );

#if defined( WIN32_FILEIO )
	// Checking file attributes is fastest way to determine existence
	return (INVALID_FILE_ATTRIBUTES != ::GetFileAttributesW( strPath.GetWCharPathPrePended() ));
#else
	statBig_t buf;
	return (0 == statBig( strPath.GetUTF8Path(), &buf ));
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Deletes a file if it exists.  If the file is read-only, will attempt
//          to change file attributes and delete it.
// Input:   pchFileName - file name to delete (unqualified, relative, or fully qualified)
// Output:  true if successful (file did not exist, or it existed and was deleted);
//          false if unsuccessful (file existed but could not be deleted)
//-----------------------------------------------------------------------------
bool BDeleteFileIfExists( const char * pchFileName )
{
	// vast majority don't need to be touched/tested to delete them, so don't
	// take the penalty in the common case.
	if (UnlinkFile( pchFileName ))
		return true;

	if (BFileExists( pchFileName ))
	{
		MakeFileWriteable( pchFileName );
		return UnlinkFile( pchFileName );
	}
	else
	{
		return true; // doesn't exist
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes an empty directory that works on multiple platforms.
//-----------------------------------------------------------------------------
bool BRemoveDirectory( const char *pchPathIn )
{
	MakeFileWriteable( pchPathIn );

	CPathString strPath( pchPathIn );
#if defined( WIN32_FILEIO )
	if (::RemoveDirectoryW( strPath.GetWCharPathPrePended() ))
		return true;
	return false;
#else
	return _rmdir( pchPathIn ) == 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Removes a directory and all subdirectories and all files in those directories
//-----------------------------------------------------------------------------
bool BRemoveDirectoryRecursive( const char *pchPathIn )
{
	CDirIterator dirIter( pchPathIn, "*" );

	while (dirIter.BNextFile())
	{
		uint32 unLenPath = Q_strlen( pchPathIn ) + Q_strlen( dirIter.CurrentFileName() ) + 2;
		char *pchPath = new char[unLenPath];
		Q_snprintf( pchPath, unLenPath, "%s%c%s", pchPathIn, CORRECT_PATH_SEPARATOR, dirIter.CurrentFileName() );

		if (dirIter.BCurrentIsDir())
		{
			BRemoveDirectoryRecursive( pchPath );
		}
		else
		{
			// Shouldn't have files in the root dir, delete them if found
			BDeleteFileIfExists( pchPath );
		}
		delete[] pchPath;
	}

	return BRemoveDirectory( pchPathIn );
}

#ifdef POSIX

// findfirst/findnext implementation from filesystem/linux_support.[h|cpp]
// modified a bit for PS3

#if !defined(_PS3)

static char selectBuf[PATH_MAX];

#if defined(OSX) && !defined(__MAC_10_8)
static int FileSelect( direntBig_t *ent )
#elif defined(LINUX) || defined(OSX)
static int FileSelect( const direntBig_t *ent )
#else
#error
#endif
{
	const char *mask = selectBuf;
	const char *name = ent->d_name;

	return FileSelect( name, mask );
}

#endif // !_PS3

static int FileSelect( const char *name, const char *mask )
{
	//printf("Test:%s %s\n",mask,name);

	if (!strcmp( name, "." ) || !strcmp( name, ".." )) return 0;

	if (!strcmp( mask, "*.*" ) || !strcmp( mask, "*" )) return 1;

	while (*mask && *name)
	{
		if (*mask == '*')
		{
			mask++; // move to the next char in the mask
			if (!*mask) // if this is the end of the mask its a match 
			{
				return 1;
			}
			while (*name && toupper( *name ) != toupper( *mask ))
			{ // while the two don't meet up again
				name++;
			}
			if (!*name)
			{ // end of the name
				break;
			}
		}
		else if (*mask != '?')
		{
			if (toupper( *mask ) != toupper( *name ))
			{   // mismatched!
				return 0;
			}
			else
			{
				mask++;
				name++;
				if (!*mask && !*name)
				{ // if its at the end of the buffer
					return 1;
				}

			}

		}
		else /* mask is "?", we don't care*/
		{
			mask++;
			name++;
		}
	}

	return(!*mask && !*name); // both of the strings are at the end
}

#if !defined(_PS3)

int FillDataStruct( _finddata_t *dat )
{
	statBig_t fileStat;

	if (dat->curName >= dat->numNames)
		return -1;

	Q_strncpy( dat->name, dat->namelist[dat->curName]->d_name, sizeof(dat->name) );
	char szFullPath[MAX_PATH];
	Q_snprintf( szFullPath, sizeof(szFullPath), "%s%c%s", dat->dirBase, CORRECT_PATH_SEPARATOR, dat->name );
	if (statBig( szFullPath, &fileStat ) == 0)
	{
		dat->attrib = fileStat.st_mode;
		dat->size = fileStat.st_size;
		dat->time_write = fileStat.st_mtime;
		dat->time_create = fileStat.st_ctime;
	}
	else
	{
		dat->attrib = 0;
		dat->size = 0;
		dat->time_write = 0;
		dat->time_create = 0;
	}
	free( dat->namelist[dat->curName] );
	dat->namelist[dat->curName] = NULL;
	dat->curName++;
	return 1;
}

int _findfirst( const char *fileName, _finddata_t *dat )
{
	char nameStore[PATH_MAX];
	char *dir = NULL;
	int n, iret = -1;

	Q_strncpy( nameStore, fileName, sizeof(nameStore) );

	if (strrchr( nameStore, '/' ))
	{
		dir = nameStore;
		while (strrchr( dir, '/' ))
		{
			statBig_t dirChk;

			// zero this with the dir name
			dir = strrchr( nameStore, '/' );
			*dir = '\0';
			if (dir == nameStore)
			{
				dir = "/";
			}
			else
			{
				dir = nameStore;
			}

			if (statBig( dir, &dirChk ) == 0 && S_ISDIR( dirChk.st_mode ))
			{
				break;
			}
		}
	}
	else
	{
		// couldn't find a dir separator...
		return -1;
	}

	if (strlen( dir ) > 0)
	{
		if (strlen( dir ) == 1)
			Q_strncpy( selectBuf, fileName + 1, sizeof(selectBuf) );
		else
			Q_strncpy( selectBuf, fileName + strlen( dir ) + 1, sizeof(selectBuf) );

		n = scandirBig( dir, &dat->namelist, FileSelect, alphasortBig );
		if (n < 0)
		{
			// silently return, nothing interesting
		}
		else
		{
			dat->curName = 0;
			dat->numNames = n; // n is the number of matches
			Q_strncpy( dat->dirBase, dir, sizeof(dat->dirBase) );
			iret = FillDataStruct( dat );
			if (iret < 0)
			{
				free( dat->namelist );
				dat->namelist = NULL;
				dat->curName = 0;
				dat->numNames = 0;
			}
		}
	}

	//  printf("Returning: %i \n",iret);
	return iret;
}

int _findnext( int64 handle, _finddata_t *dat )
{
	if (dat->curName >= dat->numNames)
	{
		free( dat->namelist );
		dat->namelist = NULL;
		dat->curName = 0;
		dat->numNames = 0;
		return -1; // no matches left
	}

	FillDataStruct( dat );
	return 0;
}

bool _findclose( int64 handle )
{
	return true;
}

#endif // !_PS3
#endif


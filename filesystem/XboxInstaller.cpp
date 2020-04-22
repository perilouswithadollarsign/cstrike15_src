//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
//
//===========================================================================//

#include "basefilesystem.h"
#include "filesystem/ixboxinstaller.h"
#include "tier1/utlbuffer.h"
#include "tier0/icommandline.h"
#include "tier2/tier2.h"
#include "characterset.h"
#include "xbox/xbox_launch.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Uncomment to allow system to operate (Image must be built with expected segmented zips)
#if defined( _X360 )
#define SUPPORTS_INSTALL_TO_XBOX_HDD
#endif

#define INSTALL_MANAGER_PROCESSOR	3
#define INSTALL_READ_PROCESSOR		3
#define INSTALL_WRITE_PROCESSOR		4

#if !defined( _CERT )
ConVar xbox_install_fake_readerror( "xbox_install_fake_readerror", "0", 0 );
ConVar xbox_install_fake_writeerror( "xbox_install_fake_writeerror", "0", 0 );
#endif

// artifical condition to disallow a qualified installation
// this allows an install to be re-enabled
ConVar xbox_install_allowed( "xbox_install_allowed", "1", 0 );

struct installEntry_t
{
	const char	*pSource;
	const char	*pTarget;
};
installEntry_t g_InstallScript[] =
{
	{"D:\\csgo\\zip0.360.zip",			"csgo\\zip0.360.zip"},
// no localiztion install
#if 0
	{"D:\\csgo_%\\zip0.360.zip",		"csgo_%\\zip0.360.zip"},
#endif
	// put down last as final complete marker
	{"D:\\version.xtx",					"version.txt"},
};

bool ReadFileTest( const char *pFilename, DWORD nMaxReadSize, DWORD nRandomOffset )
{
	OVERLAPPED Overlapped = { 0 };
	HANDLE hFile = INVALID_HANDLE_VALUE;
	void *pBuffer = NULL;
	bool bSuccess = false;
	char fixedFilename[MAX_PATH];

	V_strncpy( fixedFilename, pFilename, sizeof( fixedFilename ) );
	V_FixSlashes( fixedFilename );
	pFilename = fixedFilename;

	// validate the source file
	hFile = CreateFile( pFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, NULL );
	if ( hFile == INVALID_HANDLE_VALUE )
	{
		// failure
		goto cleanUp;
	}

	DWORD nFileSize = GetFileSize( hFile, NULL );
	if ( nFileSize == (DWORD)-1 )
	{
		// failure
		goto cleanUp;
	}

	DWORD nBufferSize = AlignValue( nFileSize, SOURCE_SECTOR_SIZE );
	nBufferSize = min( nBufferSize, nMaxReadSize );
	pBuffer = malloc( nBufferSize );
	if ( !pBuffer )
	{
		// failure
		goto cleanUp;
	}

	if ( nRandomOffset + nBufferSize > nFileSize )
	{
		nRandomOffset = 0;
	}
	Overlapped.Offset = nRandomOffset;

	float startTime = Plat_FloatTime();

	// read file
	BOOL bResult = ReadFile( hFile, pBuffer, nBufferSize, NULL, &Overlapped );
	DWORD dwError = GetLastError();
	if ( !bResult && dwError != ERROR_IO_PENDING )
	{
		if ( dwError != ERROR_HANDLE_EOF )
		{
			goto cleanUp;
		}
	}

	DWORD dwBytesRead = 0;
	bResult = GetOverlappedResult( hFile, &Overlapped, &dwBytesRead, TRUE );
	dwError = GetLastError();
	if ( dwBytesRead != nBufferSize )
	{
		goto cleanUp;
	}

	float elapsed, totalSizeMB;
	elapsed = Plat_FloatTime() - startTime;
	totalSizeMB = (float)nBufferSize/(1024.0f*1024.0f);
	Msg( "Read: %s, %.2f MB in %.2f secs, %.2f MB/sec\n", pFilename, totalSizeMB, elapsed, totalSizeMB/elapsed );

	bSuccess = true;

cleanUp:
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		CloseHandle( hFile );
	}

	if ( pBuffer )
	{
		free( pBuffer );
	}

	return bSuccess;
}

struct FileDetails_t
{
	FileDetails_t()
	{
		m_nRealFileSize = 0;
		m_bFileIsValid = false;
	}

	CUtlString		m_SourceName;
	CUtlString		m_TargetName;

	// this is the size we want the target file to be at completion
	DWORD			m_nRealFileSize;
	// this is the reserved size of the target file
	bool			m_bFileIsValid;
};

struct InstallData_t
{
	void Reset()
	{
		m_Files.Purge();
		m_nTotalSize = 0;
		m_nVersion = 0;
		m_bValid = false;
		m_bCompleted = false;
		m_bFailed = false;
	}

	CUtlVector< FileDetails_t >	m_Files;
	DWORD						m_nTotalSize;
	DWORD						m_nVersion;
	bool						m_bValid;
	bool						m_bCompleted;
	bool						m_bFailed;
};

// copying onto HDD
#define INSTALL_BUFFER_SIZE	(512*1024)
#define INSTALL_NUM_BUFFERS	4

struct CopyFile_t
{
	// source file
	HANDLE				m_hSrcFile;
	DWORD				m_srcFileSize;
	int					m_readBufferSize;

	// target file
	HANDLE				m_hDstFile;
	DWORD				m_dstCurrentFileSize;

	CopyStats_t			*m_pCopyStats;
};

struct Buffer_t
{
	unsigned char	*pData;
	DWORD			dwSize;
	Buffer_t*		pNext;
	int				id;
};

class CXboxInstaller : public CTier2AppSystem< IXboxInstaller >
{
	typedef CTier2AppSystem< IXboxInstaller > BaseClass;

public:
	CXboxInstaller();
	virtual ~CXboxInstaller();

	// Inherited from IAppSystem
	virtual InitReturnVal_t		Init();
	virtual void				Shutdown();

	virtual bool				Setup( bool bForceInstall );
	virtual void				ResetSetup();

	virtual bool				Start();
	virtual void				Stop();
	virtual bool				IsStopped( bool bForceStop );

	virtual DWORD				GetTotalSize();
	virtual DWORD				GetVersion();
	virtual const CopyStats_t	*GetCopyStats();
	virtual bool				IsInstallEnabled();
	virtual bool				IsFullyInstalled();

	virtual bool				ShouldRestart();
	virtual bool				ForceCachePaths();
	virtual void				SpewStatus();

	DWORD				ReadFileThread( CopyFile_t *pCopyFile );
	DWORD				WriteFileThread( CopyFile_t *pCopyFile );
	DWORD				InstallThreadFunc();	

private:
	bool				BuildInstallScript( bool bForceFullInstall );
	bool				IsTargetFileValid( const char *pFilename, DWORD dwSrcFileSize );
	bool				DoesFileExist( const char *pFilename, DWORD *pSize );
	bool				CopyFileOverlapped( FileDetails_t *pFileDetails, CopyStats_t *pCopyStats );
	bool				CreateFilePath( const char *inPath );
	bool				FixupNamespaceFilename( const char *pLanguage, const char *pFilename, char *pOutFilename, int outFilenameSize );
	Buffer_t			*LockBufferForRead();
	Buffer_t			*LockBufferForWrite();
	void				AddBufferForRead( Buffer_t *pBuffer );
	void				AddBufferForWrite( Buffer_t *pBuffer );
	bool				PrepareCachePartitionForInstall();

	CRITICAL_SECTION	m_CriticalSection;

	HANDLE				m_hReadEvent;
	HANDLE				m_hWriteEvent;
	HANDLE				m_hInstallThread;

	Buffer_t			*m_pReadBuffers;
	Buffer_t			*m_pWriteBuffers;
	DWORD				*m_pNumReadBuffers;
	DWORD				*m_pNumWriteBuffers;

	InstallData_t		m_InstallData;
	CopyStats_t			m_CopyStats;

	unsigned char		*m_pCopyBuffers[INSTALL_NUM_BUFFERS];

	bool				m_bHasHDD;
	bool				m_bInit;
	bool				m_bAppSystemInit;
	bool				m_bForcedCachePaths;

	// used to stop the process under NON-ERROR conditions
	CInterlockedInt		m_bStopping;
	// stops the process, read errors are fatal
	CInterlockedInt		m_bReadError;
	// stops the process, write errors just permanently halt the install
	CInterlockedInt		m_bWriteError;
};

static CXboxInstaller g_XboxInstaller;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CXboxInstaller, IXboxInstaller, XBOXINSTALLER_INTERFACE_VERSION, g_XboxInstaller );

CON_COMMAND( xbox_install_testcache, "" )
{
	if ( !g_XboxInstaller.IsFullyInstalled() )
	{
		return;
	}

	DWORD nRandomOffset = 0;
	DWORD nInitialSize = 16*1024;
	for ( int i = 0; i < 8; i++ )
	{
		Msg( "\nRead Test at %.2f MB\n", (float)nInitialSize/(1024.0f*1024.0f) );

		nRandomOffset += 4*1024*1024;
		ReadFileTest( CFmtStr( "%s/csgo/zip0.360.zip", CACHE_PATH_CSTIKRE15 ), nInitialSize, nRandomOffset );
		ReadFileTest( "d:/csgo/zip0.360.zip", nInitialSize, nRandomOffset );

		nInitialSize <<= 1;
	}
}

// for thrash testing only
CON_COMMAND( xbox_install_teststop, "" )
{
	g_XboxInstaller.Stop();
}

// for thrash testing only
CON_COMMAND( xbox_install_teststart, "" )
{
	g_XboxInstaller.Start();
}

//-----------------------------------------------------------------------------
// Isolated all mounting/unmounting here.
//-----------------------------------------------------------------------------
static bool MountCachePartition( bool bFormat )
{
#if defined( _DEMO ) || !defined( SUPPORTS_INSTALL_TO_XBOX_HDD )
	// under demo conditions cannot allow any HDD access
	return false;
#endif

	static bool s_bMounted = false;
	if ( s_bMounted )
	{
		if ( bFormat )
		{
			// must unmount before format
			XUnmountUtilityDrive();
			s_bMounted = false;
		}
		else
		{
			// already mounted
			return true;
		}
	}

	COM_TimestampedLog( "XMountUtilityDrive( %s )", ( bFormat ? "format" : "" ) );

	// small cluster size not useful, as we are block installing large files
	DWORD dwResult = XMountUtilityDrive( bFormat, 64*1024, 256*1024 );
	if ( dwResult == ERROR_SUCCESS )
	{
		s_bMounted = true;
	}

	COM_TimestampedLog( "XMountUtilityDrive() - Finish" );

	return s_bMounted;
}

//-----------------------------------------------------------------------------
// USED BY THE FILESYSTEM ONLY!!!! AT IT'S INIT/BOOT TIME
// This is here as a back door, it has to solve the query during the filesystem's
// init method, without using the filesystem.
//-----------------------------------------------------------------------------
bool IsAlreadyInstalledToXboxHDDCache()
{
#if defined( _DEMO ) || !defined( SUPPORTS_INSTALL_TO_XBOX_HDD )
	// under demo conditions cannot allow any HDD access
	return false;
#endif

	// TCR best practices allows a backdoor (LS+RS) to reforce HDD cache installers.
	// Tech support may tell users with flaky HDD to do this.
	// Our inputsystem wrapper is not available yet due to init chain, so go directly to hardware.
	bool bForceInstall = false;
	DWORD dwResult;    
	for ( DWORD i=0; i < XUSER_MAX_COUNT; i++ )
	{
		XINPUT_STATE state;
		ZeroMemory( &state, sizeof( XINPUT_STATE ) );

		dwResult = XInputGetState( i, &state );
		if ( dwResult != ERROR_SUCCESS )
			continue;

		// it must be (LS+RS) exactlty only on any controller, and not a mashing of buttons
		if ( state.Gamepad.wButtons == ( XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER ) &&
			state.Gamepad.bLeftTrigger == 0 && 
			state.Gamepad.bRightTrigger == 0 )
		{
			bForceInstall = true;
		}
		else if ( state.Gamepad.wButtons || state.Gamepad.bLeftTrigger || state.Gamepad.bRightTrigger )
		{
			// other buttons are being held, sorry
			bForceInstall = false;
			break;
		}
	}

	if ( CommandLine()->FindParm( "-forceinstall" ) != 0 )
	{
		bForceInstall = true;
	}

	if ( !xbox_install_allowed.GetBool() )
	{
		// ignore any prior install by clearing any existing install
		bForceInstall = true;
	}

	if ( g_XboxInstaller.Setup( bForceInstall ) )
	{
		return g_XboxInstaller.IsFullyInstalled();
	}

	return false;
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CXboxInstaller::CXboxInstaller()
{
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CXboxInstaller::~CXboxInstaller()
{
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
InitReturnVal_t CXboxInstaller::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

	// NOTHING CAN GO HERE!!!!
	// This subverts normal startup procedures and performs a few operations
	// for the filesystem's init() before it gets inited.
	m_bAppSystemInit = true;

	return INIT_OK; 
}

//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CXboxInstaller::Shutdown()
{
	BaseClass::Shutdown();
}

//-----------------------------------------------------------------------------
// CreateFilePath
//
// Create full path to specified file.
//-----------------------------------------------------------------------------
bool CXboxInstaller::CreateFilePath( const char *inPath )
{
	char*	ptr;
	char	dirPath[MAX_PATH];
	BOOL	bSuccess;

	// prime and skip to first seperator after the drive path
	strcpy( dirPath, inPath );
	ptr = strchr( dirPath, '\\' );
	while ( ptr )
	{		
		ptr = strchr( ptr+1, '\\' );
		if ( ptr )
		{
			*ptr = '\0';
			bSuccess = ::CreateDirectory( dirPath, NULL );
			*ptr = '\\';
		}
	}

	// ensure read-only is cleared
	SetFileAttributes( inPath, FILE_ATTRIBUTE_NORMAL );

	return true;
}

//-----------------------------------------------------------------------------
// FixupNamespaceFilename
//-----------------------------------------------------------------------------
bool CXboxInstaller::FixupNamespaceFilename( const char *pLanguage, const char *pFilename, char *pOutFilename, int outFilenameSize )
{
	char newFilename[MAX_PATH];

	bool bFixup = false;
	int dstLen = 0;
	int srcLen = strlen( pFilename );
	for ( int i=0; i<srcLen+1; i++ )
	{
		// replace every occurrence of % with language
		if ( pFilename[i] == '%' )
		{
			int len = strlen( pLanguage );
			memcpy( newFilename + dstLen, pLanguage, len );
			dstLen += len;
			bFixup = true;
		}
		else
		{
			newFilename[dstLen] = pFilename[i];
			dstLen++;
		}
	}

	V_strncpy( pOutFilename, newFilename, outFilenameSize );
	return bFixup;
}

//-----------------------------------------------------------------------------
// LockBufferForRead
//
//-----------------------------------------------------------------------------
Buffer_t *CXboxInstaller::LockBufferForRead()
{
	EnterCriticalSection( &m_CriticalSection );

	if ( m_pReadBuffers )
	{
		ResetEvent( m_hReadEvent );
	}
	else
	{
		do
		{
			// prevent any possible block
			if ( ( m_bStopping || m_bReadError || m_bWriteError ) )
			{
				LeaveCriticalSection( &m_CriticalSection );
				return NULL;
			}

			LeaveCriticalSection( &m_CriticalSection );

			// out of data, wait for it
			WaitForSingleObject( m_hReadEvent, INFINITE );

			EnterCriticalSection( &m_CriticalSection );
		}
		while ( !m_pReadBuffers );
	}

	Buffer_t *pBuffer = m_pReadBuffers;
	m_pReadBuffers = pBuffer->pNext;

	(*m_pNumReadBuffers)--;

	LeaveCriticalSection( &m_CriticalSection );

	return pBuffer;
}

//-----------------------------------------------------------------------------
// LockBufferForWrite
//
//-----------------------------------------------------------------------------
Buffer_t *CXboxInstaller::LockBufferForWrite()
{
	EnterCriticalSection( &m_CriticalSection );

	if ( m_pWriteBuffers )
	{
		ResetEvent( m_hWriteEvent );
	}
	else
	{
		do
		{
			// prevent any possible block
			if ( ( m_bStopping || m_bReadError || m_bWriteError ) )
			{
				LeaveCriticalSection( &m_CriticalSection );
				return NULL;
			}

			LeaveCriticalSection( &m_CriticalSection );

			// out of data, wait for more
			WaitForSingleObject( m_hWriteEvent, INFINITE );

			EnterCriticalSection( &m_CriticalSection );
		}
		while ( !m_pWriteBuffers );
	}

	Buffer_t *pBuffer = m_pWriteBuffers;
	m_pWriteBuffers = pBuffer->pNext;

	(*m_pNumWriteBuffers)--;

	LeaveCriticalSection( &m_CriticalSection );

	return pBuffer;
}

//-----------------------------------------------------------------------------
// AddBufferForRead
//
//-----------------------------------------------------------------------------
void CXboxInstaller::AddBufferForRead( Buffer_t *pBuffer )
{
	EnterCriticalSection( &m_CriticalSection );

	// add to end of list
	Buffer_t *pCurrent = m_pReadBuffers;
	while ( pCurrent && pCurrent->pNext )
	{
		pCurrent = pCurrent->pNext;
	}
	if ( pCurrent )
	{
		pBuffer->pNext  = pCurrent->pNext;
		pCurrent->pNext = pBuffer;
	}
	else
	{
		pBuffer->pNext = NULL;
		m_pReadBuffers = pBuffer;
	}

	(*m_pNumReadBuffers)++;

	LeaveCriticalSection( &m_CriticalSection );

	SetEvent( m_hReadEvent );
}

//-----------------------------------------------------------------------------
// AddBufferForWrite
//
//-----------------------------------------------------------------------------
void CXboxInstaller::AddBufferForWrite( Buffer_t *pBuffer )
{
	EnterCriticalSection( &m_CriticalSection );

	// add to end of list
	Buffer_t* pCurrent = m_pWriteBuffers;
	while ( pCurrent && pCurrent->pNext )
	{
		pCurrent = pCurrent->pNext;
	}
	if ( pCurrent )
	{
		pBuffer->pNext  = pCurrent->pNext;
		pCurrent->pNext = pBuffer;
	}
	else
	{
		pBuffer->pNext = NULL;
		m_pWriteBuffers = pBuffer;
	}

	(*m_pNumWriteBuffers)++;

	LeaveCriticalSection( &m_CriticalSection );

	SetEvent( m_hWriteEvent );
}

//-----------------------------------------------------------------------------
// ReadFileThread
//
//-----------------------------------------------------------------------------
DWORD CXboxInstaller::ReadFileThread( CopyFile_t *pCopyFile )
{
	OVERLAPPED		overlappedRead = {0};
	DWORD			startTime;
	DWORD			dwBytesRead;
	DWORD			dwError;
	BOOL			bResult;
	Buffer_t		*pBuffer = NULL;

	// start reading from resume point
	overlappedRead.Offset = pCopyFile->m_dstCurrentFileSize;

	// Copy from the buffer to the Hard Drive
	while ( overlappedRead.Offset < pCopyFile->m_srcFileSize )
	{
		pBuffer = LockBufferForRead();
		if ( !pBuffer )
		{
			break;
		}
		if ( m_bStopping || m_bReadError || m_bWriteError )
		{
			// stopping or
			// errors occuring, cease all reading
			break;
		}

		startTime = GetTickCount();
		dwBytesRead = 0;

		int numAttempts = 0;
retry:
		// read file from DVD
		bResult = ReadFile( pCopyFile->m_hSrcFile, pBuffer->pData, pCopyFile->m_readBufferSize, NULL, &overlappedRead );
		dwError = GetLastError();
		if ( !bResult && dwError != ERROR_IO_PENDING )
		{
			if ( dwError == ERROR_HANDLE_EOF )
			{
				// nothing more to read
				break;
			}

			numAttempts++;
			if ( numAttempts == 3 )
			{
				// error
				m_bReadError = true;
				break;
			}
			else
			{
				goto retry;
			}
		}
		else
		{
			// Wait for the operation to finish
			GetOverlappedResult( pCopyFile->m_hSrcFile, &overlappedRead, &dwBytesRead, TRUE );
		}

#if !defined( _CERT )
		if ( xbox_install_fake_readerror.GetBool() )
		{
			dwBytesRead = 0;
		}
#endif

		if ( !dwBytesRead )
		{
			m_bReadError = true;
			break;
		}

		overlappedRead.Offset += dwBytesRead;
		pCopyFile->m_pCopyStats->m_BufferReadSize = dwBytesRead;
		pCopyFile->m_pCopyStats->m_BufferReadTime = GetTickCount() - startTime;
		pCopyFile->m_pCopyStats->m_TotalReadSize += pCopyFile->m_pCopyStats->m_BufferReadSize;
		pCopyFile->m_pCopyStats->m_TotalReadTime += pCopyFile->m_pCopyStats->m_BufferReadTime;

		pBuffer->dwSize = dwBytesRead;
		AddBufferForWrite( pBuffer );
	}

	if ( ( m_bStopping || m_bReadError || m_bWriteError ) && pBuffer )
	{
		// the aborted buffer must be returned to the pool
		// this unblocks the write thread who will detect the error 
		AddBufferForWrite( pBuffer );
	}

	return 0;
}

static DWORD WINAPI ReadFileThread( LPVOID lParam )
{
	return g_XboxInstaller.ReadFileThread( (CopyFile_t*)lParam );
}

//-----------------------------------------------------------------------------
// WriteFileThread
//
//-----------------------------------------------------------------------------
DWORD CXboxInstaller::WriteFileThread( CopyFile_t *pCopyFile )
{
	OVERLAPPED		overlappedWrite = {0};
	DWORD			startTime;
	DWORD			dwBytesWrite;
	DWORD			dwWriteSize;
	DWORD			dwError;
	BOOL			bResult;
	Buffer_t		*pBuffer = NULL;
	unsigned char	*pWriteBuffer;

	// start writing from the resume point
	overlappedWrite.Offset = pCopyFile->m_dstCurrentFileSize;

	pCopyFile->m_pCopyStats->m_BytesCopied += pCopyFile->m_dstCurrentFileSize;
	pCopyFile->m_pCopyStats->m_WriteSize += pCopyFile->m_dstCurrentFileSize;

	while ( overlappedWrite.Offset < pCopyFile->m_srcFileSize )
	{
		// wait for wake-up event
		pBuffer = LockBufferForWrite();
		if ( !pBuffer )
		{
			break;
		}
		if ( m_bStopping || m_bReadError || m_bWriteError )
		{
			// stopping or
			// errors occuring, cease all writing
			break;
		}

		dwWriteSize = pBuffer->dwSize;
		pWriteBuffer = pBuffer->pData;

		if ( overlappedWrite.Offset + dwWriteSize >= pCopyFile->m_srcFileSize )
		{
			// last buffer, ensure all data is written
			dwWriteSize = ALIGN_VALUE( dwWriteSize, TARGET_SECTOR_SIZE );
		}

		startTime = GetTickCount();
		dwBytesWrite = 0;

		int numAttempts = 0;
retry:
		// write file to HDD
		bResult = WriteFile( pCopyFile->m_hDstFile, pWriteBuffer, (dwWriteSize/TARGET_SECTOR_SIZE) * TARGET_SECTOR_SIZE, NULL, &overlappedWrite );
		dwError = GetLastError();
		if ( !bResult && dwError != ERROR_IO_PENDING )
		{
			numAttempts++;
			if ( numAttempts == 3 )
			{
				// error
				m_bWriteError = true;
				break;
			}
			else
			{
				goto retry;
			}
		}
		else
		{
			// Wait for the operation to finish
			bResult = GetOverlappedResult( pCopyFile->m_hDstFile, &overlappedWrite, &dwBytesWrite, TRUE );
			dwError = GetLastError();
		}

#if !defined( _CERT )
		if ( xbox_install_fake_writeerror.GetBool() )
		{
			dwBytesWrite = 0;
		}
#endif

		if ( !dwBytesWrite )
		{
			m_bWriteError = true;
			break;
		}		

		// track expected size
		overlappedWrite.Offset += dwBytesWrite;
		pCopyFile->m_pCopyStats->m_BytesCopied += dwBytesWrite;
		pCopyFile->m_pCopyStats->m_WriteSize += dwBytesWrite;
		pCopyFile->m_pCopyStats->m_BufferWriteSize = dwBytesWrite;
		pCopyFile->m_pCopyStats->m_BufferWriteTime = GetTickCount() - startTime;
		pCopyFile->m_pCopyStats->m_TotalWriteSize += pCopyFile->m_pCopyStats->m_BufferWriteSize;
		pCopyFile->m_pCopyStats->m_TotalWriteTime += pCopyFile->m_pCopyStats->m_BufferWriteTime;

		// done with buffer
		AddBufferForRead( pBuffer );
	}

	if ( ( m_bStopping || m_bReadError || m_bWriteError ) && pBuffer )
	{
		// the aborted buffer must be returned to the pool
		// this unblocks the read thread who will detect the error 
		AddBufferForRead( pBuffer );
	}

	return 0;
}

static DWORD WINAPI WriteFileThread( LPVOID lParam )
{
	return g_XboxInstaller.WriteFileThread( (CopyFile_t*)lParam );
}

//-----------------------------------------------------------------------------
// CopyFileOverlapped
//
//-----------------------------------------------------------------------------
bool CXboxInstaller::CopyFileOverlapped( FileDetails_t *pFileDetails, CopyStats_t *pCopyStats )
{
	CopyFile_t	copyFile = {0};
	Buffer_t	buffers[INSTALL_NUM_BUFFERS] = {0};
	HANDLE		hReadThread = NULL;
	HANDLE		hWriteThread = NULL;
	bool		bSuccess = false;
	DWORD		startCopyTime;
	DWORD		dwResult;

	const char *pSrcFilename = pFileDetails->m_SourceName.String();
	const char *pDstFilename = pFileDetails->m_TargetName.String();

	startCopyTime = GetTickCount();

	m_pReadBuffers  = NULL;
	m_pWriteBuffers = NULL;

	pCopyStats->m_NumReadBuffers = 0;
	pCopyStats->m_NumWriteBuffers = 0;

	m_pNumReadBuffers  = &pCopyStats->m_NumReadBuffers;
	m_pNumWriteBuffers = &pCopyStats->m_NumWriteBuffers;

	strcpy( pCopyStats->m_srcFilename, pSrcFilename );
	strcpy( pCopyStats->m_dstFilename, pDstFilename );

	copyFile.m_hSrcFile = INVALID_HANDLE_VALUE;
	copyFile.m_hDstFile = INVALID_HANDLE_VALUE;

	copyFile.m_pCopyStats = pCopyStats;

	if ( !m_hReadEvent )
	{
		m_hReadEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		if ( !m_hReadEvent )
		{
			goto cleanUp;
		}
	}

	if ( !m_hWriteEvent )
	{
		m_hWriteEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		if ( !m_hWriteEvent )
		{
			goto cleanUp;
		}
	}

	// expected startup state
	ResetEvent( m_hReadEvent );
	ResetEvent( m_hWriteEvent );

	// validate the source file
	copyFile.m_hSrcFile = CreateFile( pSrcFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, NULL );
	if ( copyFile.m_hSrcFile == INVALID_HANDLE_VALUE )
	{
		// failure
		goto cleanUp;
	}

	// ensure the target file path exists
	CreateFilePath( pDstFilename );

	// validate the target file
	copyFile.m_hDstFile = CreateFile( pDstFilename, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, NULL );
	if ( copyFile.m_hDstFile == INVALID_HANDLE_VALUE )
	{
		// failure
		goto cleanUp;
	}

	pCopyStats->m_ReadSize = pFileDetails->m_nRealFileSize;
	pCopyStats->m_WriteSize = 0;

	// setup for copy
	copyFile.m_readBufferSize = INSTALL_BUFFER_SIZE;
	copyFile.m_srcFileSize = pFileDetails->m_nRealFileSize;
	// not supporting resume, always copy entire file
	copyFile.m_dstCurrentFileSize = 0;

	// setup read buffers
	for ( int i = 0; i < INSTALL_NUM_BUFFERS; i++ )
	{
		buffers[i].pData  = m_pCopyBuffers[i];
		buffers[i].dwSize = 0;
		buffers[i].pNext  = NULL;
		AddBufferForRead( &buffers[i] );
	}

	// pre-size the target file in aligned buffers
	DWORD dwAligned = ALIGN_VALUE( copyFile.m_srcFileSize, TARGET_SECTOR_SIZE );
	dwResult = SetFilePointer( copyFile.m_hDstFile, dwAligned, NULL, FILE_BEGIN );
	if ( dwResult == INVALID_SET_FILE_POINTER )
	{
		// failure
		goto cleanUp;
	}
	SetEndOfFile( copyFile.m_hDstFile );

	// start the read thread
	hReadThread = CreateThread( 0, 0, &::ReadFileThread, &copyFile, CREATE_SUSPENDED, 0 );
	if ( !hReadThread )
	{
		// failure
		goto cleanUp;
	}
	XSetThreadProcessor( hReadThread, INSTALL_READ_PROCESSOR );
	ResumeThread( hReadThread );

	// start the write thread
	hWriteThread = CreateThread( 0, 0, &::WriteFileThread, &copyFile, CREATE_SUSPENDED, 0 );
	if ( !hWriteThread )
	{
		// failure
		goto cleanUp;
	}
	XSetThreadProcessor( hWriteThread, INSTALL_WRITE_PROCESSOR );
	ResumeThread( hWriteThread );

	// wait for threads to finish
	WaitForSingleObject( hWriteThread, INFINITE );
	WaitForSingleObject( hReadThread, INFINITE );

	CloseHandle( copyFile.m_hDstFile );
	copyFile.m_hDstFile = INVALID_HANDLE_VALUE;

	if ( m_bReadError || m_bWriteError )
	{
		goto cleanUp;
	}

	// only change the file to its true size if file copying has completed
	if ( pCopyStats->m_WriteSize >= copyFile.m_srcFileSize )
	{
		// file has completed copying, fixup to the correct final file size
		// no need to change the file size if sector aligned
		if ( copyFile.m_srcFileSize % TARGET_SECTOR_SIZE )
		{
			// re-open file as non-buffered to adjust to correct file size
			HANDLE hFile = CreateFile( pDstFilename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
			if ( hFile == INVALID_HANDLE_VALUE )
			{
				goto cleanUp;
			}
			SetFilePointer( hFile, copyFile.m_srcFileSize, NULL, FILE_BEGIN );
			SetEndOfFile( hFile );
			CloseHandle( hFile );
		}

		pCopyStats->m_WriteSize = copyFile.m_srcFileSize;

		// copy is complete
		pFileDetails->m_bFileIsValid = true;
	}

	// finished
	bSuccess = true;

cleanUp:
	if ( copyFile.m_hSrcFile != INVALID_HANDLE_VALUE )
	{
		CloseHandle( copyFile.m_hSrcFile );
	}

	if ( copyFile.m_hDstFile != INVALID_HANDLE_VALUE )
	{
		CloseHandle( copyFile.m_hDstFile );
	}

	if ( hReadThread )
	{
		CloseHandle( hReadThread );
	}

	if ( hWriteThread )
	{
		CloseHandle( hWriteThread );
	}

	if ( !bSuccess )
	{
		pCopyStats->m_CopyErrors++;
	}

	pCopyStats->m_CopyTime = GetTickCount() - startCopyTime;

	if ( m_bReadError )
	{
		FSDirtyDiskReportFunc_t func = g_pFullFileSystem->GetDirtyDiskReportFunc();
		if ( func )
		{
			func();
		}
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// DoesFileExist
//-----------------------------------------------------------------------------
bool CXboxInstaller::DoesFileExist( const char *pFilename, DWORD *pSize )
{
	if ( pSize )
	{
		*pSize = 0;
	}

	HANDLE hFile = CreateFile( pFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		// must be able to get file size
		DWORD dwFileSize = GetFileSize( hFile, NULL );
		CloseHandle( hFile );
		if ( dwFileSize != (DWORD)-1 )
		{
			// file exists and can get file size
			if ( pSize )
			{
				*pSize = dwFileSize;
			}
			return true;
		}
	}

	// not present
	return false;
}

//-----------------------------------------------------------------------------
// IsTargetFileValid
//
// Optional non-zero expected source file size must match.
// Passing in 0 for srcFileSize is the simpler existence check.
//-----------------------------------------------------------------------------
bool CXboxInstaller::IsTargetFileValid( const char *pFilename, DWORD dwSrcFileSize )
{
	DWORD	dwTargetFileSize = 0;

	// all valid target files are non-zero, CANNOT allow 0 sized files
	// the target file size could possibly be larger if a copy was aborted
	if ( !DoesFileExist( pFilename, &dwTargetFileSize ) || !dwTargetFileSize )
	{
		return false;
	}

	// valid means the target file's contents are finalized and can be trusted
	// all the sizes must be in agreement
	if ( dwSrcFileSize && dwSrcFileSize != dwTargetFileSize )
	{
		return false;
	}

	// valid (or if source size not supplied, then present)
	return true;
}

//-----------------------------------------------------------------------------
// Ensure the cache partition has enough space for the install. Possibly
// unmounts and reformats.
//-----------------------------------------------------------------------------
bool CXboxInstaller::PrepareCachePartitionForInstall()
{	
#if defined( _DEMO ) || !defined( SUPPORTS_INSTALL_TO_XBOX_HDD )
	// under demo conditions cannot allow any HDD access
	return false;
#endif

	if ( m_InstallData.m_bValid )
	{
		// already installed, nothing to do
		return true;
	}

	// Always reformat - the cache partition will fragment easily
	// a reformat erases any existing data
	// the full install will be needed
	DWORD nBytesNeeded = m_InstallData.m_nTotalSize + 16*1024*1024;

	// force a re-format
	// expect ~2GB free, according to docs
	if ( !MountCachePartition( true ) )
	{
		// can't remount
		return false;
	}

	ULARGE_INTEGER FreeBytesAvailable = { 0 };
	ULARGE_INTEGER TotalNumberOfBytes = { 0 };
	ULARGE_INTEGER TotalNumberOfFreeBytes = { 0 };

	// check the partition again
	if ( !GetDiskFreeSpaceEx(
			"cache:\\",
			&FreeBytesAvailable,
			&TotalNumberOfBytes,
			&TotalNumberOfFreeBytes ) )
	{
		return false;
	}

	if ( nBytesNeeded > FreeBytesAvailable.LowPart )
	{
		// huh? could not get enough free bytes after a re-format!
		// very bad
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// BuildInstallScript
//
// Parse filenames to be copied. Builds install script.
// Returns false if install is not possible (i.e. no HDD).
// Returns true with script. Script may be empty as install is valid.
//-----------------------------------------------------------------------------
bool CXboxInstaller::BuildInstallScript( bool bForceFullInstall )
{
	char			srcFile[MAX_PATH];
	char			dstFile[MAX_PATH];

#if defined( _DEMO ) || !defined( SUPPORTS_INSTALL_TO_XBOX_HDD )
	// under demo conditions cannot allow any HDD access
	return false;
#endif

	if ( m_bAppSystemInit )
	{
		if ( g_pFullFileSystem->IsInstalledToXboxHDDCache() )
		{
			// nothing to do, already validated, cache is in use
			return true;
		}

		if ( !g_pFullFileSystem->IsInstallAllowed() )
		{
			// the filesystem has trumped, we cannot run
			return false;
		}
	}

	// clear out any prior results
	// setup default state
	ResetSetup();

	// mount cache partition as-is
	// need to do this to validate possible existing install (i.e. at powerup)
	if ( !MountCachePartition( false ) )
	{
		// no HDD
		return false;
	}

	// get the version, expected to be at root of disk
	CUtlBuffer sourceVersionBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( "d:/version.xtx", NULL, sourceVersionBuffer ) )
	{
		// missing critical install file
		return false;
	}
	m_InstallData.m_nVersion = atoi( (const char *)sourceVersionBuffer.Base() );

	// only interested in installing a localized audio component
	// not all supported languages have a localized audio component
	const char *pLanguageString = "";
	if ( XBX_IsAudioLocalized() )
	{
		pLanguageString = XBX_GetLanguageString();
		// paranoid check
		if ( !V_stricmp( pLanguageString, "english" ) )
		{
			// bad, system is confused
			pLanguageString = "";
		}
	}

	bool bForce = bForceFullInstall;

	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );

	// scan install script
	// ensure every expected file exists properly at the target, otherwise an install is necessary
	for ( int i = 0; i < ARRAYSIZE( g_InstallScript ); i++ )
	{
		V_strncpy( srcFile, g_InstallScript[i].pSource, sizeof( srcFile ) );
		V_ComposeFileName( CACHE_PATH_CSTIKRE15, g_InstallScript[i].pTarget, dstFile, sizeof( dstFile ) );

		// name may be optionally decorated
		// replace with language token
		bool bIsLocalizedFile = FixupNamespaceFilename( pLanguageString, srcFile, srcFile, sizeof( srcFile ) );
		if ( bIsLocalizedFile )
		{
			// this is a localized file
			if ( !pLanguageString[0] )
			{
				// ignore installing localized files when not configured for that language
				continue;
			}
			FixupNamespaceFilename( pLanguageString, dstFile, dstFile, sizeof( dstFile ) );
		}

		// explicitly attempting to open file at source, same as overlapped copy to ensure i/o will succeed later
		DWORD dwSrcSize;
		if ( !DoesFileExist( srcFile, &dwSrcSize ) )
		{
			// can't validate source
			return false;
		}

		// target file may exist, but aborted
		bool bIsValid = IsTargetFileValid( dstFile, dwSrcSize );
		if ( !bIsValid )
		{
			bForce = true;
		}

		if ( !bForce )
		{
			// the version must be present and match, otherwise full re-install	
			const char *pVersion = V_stristr( dstFile, "version" );
			if ( pVersion )
			{
				// compare contents of version file
				CUtlBuffer targetVersionBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
				if ( !g_pFullFileSystem->ReadFile( dstFile, NULL, targetVersionBuffer ) )
				{
					return false;
				}

				if ( V_strcmp( (const char*)sourceVersionBuffer.Base(), (const char *)targetVersionBuffer.Base() ) )
				{
					// differing contents, full re-install
					bForce = true;
				}
			}
		}

		int iIndex = m_InstallData.m_Files.AddToTail();
		m_InstallData.m_Files[iIndex].m_SourceName = srcFile;
		m_InstallData.m_Files[iIndex].m_TargetName = dstFile;
		m_InstallData.m_Files[iIndex].m_nRealFileSize = dwSrcSize;
		m_InstallData.m_Files[iIndex].m_bFileIsValid = bIsValid;

		m_InstallData.m_nTotalSize += AlignValue( dwSrcSize, TARGET_SECTOR_SIZE );
	}

	// cannot support indeterminate installs, either all of its valid, or it gets wiped
	// base on above logic, any invalid file would have set force
	bool bValid = true;
	if ( bForce )
	{
		// force discard all
		// any invalid file will trigger a reformat 
		for ( int i = 0; i < m_InstallData.m_Files.Count(); i++ )
		{
			m_InstallData.m_Files[i].m_bFileIsValid = false;
		}
		bValid = false;
	}

	m_InstallData.m_bValid = bValid;
	m_InstallData.m_bCompleted = bValid;

	return true;
}

//-----------------------------------------------------------------------------
// Copies all install files to the hard drive
//-----------------------------------------------------------------------------
DWORD CXboxInstaller::InstallThreadFunc() 
{
	bool	bSuccess;

	m_CopyStats.m_InstallStartTime = GetTickCount();

	// allocate buffers
	for ( int i=0; i < INSTALL_NUM_BUFFERS; i++ )
	{
		m_pCopyBuffers[i] = new unsigned char[INSTALL_BUFFER_SIZE];
	}

	for ( int i = 0; i < m_InstallData.m_Files.Count(); ++i )
	{
		if ( m_bStopping )
		{
			break;
		}

		if ( m_InstallData.m_Files[i].m_bFileIsValid )
		{
			// this file is valid
			m_CopyStats.m_BytesCopied += AlignValue( m_InstallData.m_Files[i].m_nRealFileSize, TARGET_SECTOR_SIZE );
			continue;
		}

		bSuccess = CopyFileOverlapped( &m_InstallData.m_Files[i], &m_CopyStats );
		if ( !bSuccess )
		{
			// quit
			break;
		}
	}

	// all files must be valid
	bool bValid = true;
	for ( int i = 0; i < m_InstallData.m_Files.Count(); ++i )
	{
		if ( !m_InstallData.m_Files[i].m_bFileIsValid )
		{
			bValid = false;
			break;
		}
	}

	if ( bValid )
	{
		// Despite what the docs say, which do NOT have our best interest,
		// this will cause fragmentation if it is called frequently.
		// We only call it ONCE here at the end of the successful install operation.
		// The resume pattern must re-format and restart.
		XFlushUtilityDrive();
	}

	// release buffers
	for ( int i=0; i < INSTALL_NUM_BUFFERS; i++ )
	{
		delete [] m_pCopyBuffers[i];
	}

	m_CopyStats.m_InstallStopTime = GetTickCount();

	// set when install operation is complete, regardless of error
	m_InstallData.m_bValid = bValid;
	m_InstallData.m_bCompleted = true;
	m_InstallData.m_bFailed = (m_CopyStats.m_CopyErrors != 0);

	return 0;
}

DWORD WINAPI InstallThreadFunc( LPVOID lpParam )
{
	return g_XboxInstaller.InstallThreadFunc();
}

//-----------------------------------------------------------------------------
// Starts installation to disk.
//-----------------------------------------------------------------------------
bool CXboxInstaller::Start()
{
#if defined( _DEMO ) || !defined( SUPPORTS_INSTALL_TO_XBOX_HDD )
	// under demo conditions cannot allow any HDD access
	return false;
#endif

	if ( !m_bHasHDD || !m_InstallData.m_nTotalSize || m_InstallData.m_bValid || m_InstallData.m_bFailed )
	{
		// nothing to do, cannot be started or
		// either completed or failed
		return false;
	}

	if ( !xbox_install_allowed.GetBool() )
	{
		// artifical condition to disallow a qualified installation
		return false;
	}

	if ( m_bStopping )
	{
		// we must be completely stopped before restarting
		// force the stop process to complete
		// this will cause a hitch if the time between Stop() and Start() is very short
		IsStopped( true );
	}
	else if ( m_hInstallThread )
	{
		// already started
		return true;
	}

	if ( !m_bInit )
	{
		m_bInit = true;
		InitializeCriticalSection( &m_CriticalSection );
	}

	Msg( "Xbox Install: Starting...\n" );

	// reset expected state
	V_memset( &m_CopyStats, 0, sizeof( CopyStats_t ) );
	m_InstallData.m_bCompleted = false;

	if ( m_bStopping )
	{
		// Cannot support resuming
		// reset any prior results
		for ( int i = 0; i < m_InstallData.m_Files.Count(); i++ )
		{
			m_InstallData.m_Files[i].m_bFileIsValid = false;
		}
		// reformat prior to restart
		// prevents ANY fragmentation
		if ( !MountCachePartition( true ) )
		{
			return false;
		}
	}

	m_bStopping = false;
	m_bReadError = false;
	m_bWriteError = false;

	// Start the install thread
	m_hInstallThread = CreateThread( NULL, 0, &::InstallThreadFunc, NULL, CREATE_SUSPENDED, 0 );
	if ( !m_hInstallThread )
	{
		// failed, install operation is not running
		m_InstallData.m_bCompleted = true;
		m_InstallData.m_bFailed = true;
		return false;
	}
	XSetThreadProcessor( m_hInstallThread, INSTALL_MANAGER_PROCESSOR );
	ResumeThread( m_hInstallThread );

	// started
	return true;
}

//-----------------------------------------------------------------------------
// Poll for install stoppage. Optionally synchronously force the stop.
//-----------------------------------------------------------------------------
bool CXboxInstaller::IsStopped( bool bForceStop )
{
#if defined( _DEMO ) || !defined( SUPPORTS_INSTALL_TO_XBOX_HDD )
	// under demo conditions cannot allow any HDD access
	// installer is not running and will never run
	return true;
#endif

	if ( !m_bHasHDD || !m_hInstallThread )
	{
		// not running or already stopped
		return true;
	}

	if ( bForceStop )
	{
		// caller may not have invoked
		// start the stopping procedure
		Stop();
	}

	// poll or wait for the install thread to terminate
	if ( m_hInstallThread )
	{
		DWORD dwValue = WaitForSingleObject( m_hInstallThread, bForceStop ? INFINITE : 0 );
		if ( dwValue != WAIT_OBJECT_0 )
		{
			return false;
		}
		CloseHandle( m_hInstallThread );
		m_hInstallThread = NULL;
	}

	Msg( "Xbox Install: Stopped.\n" );

	return true;
}

//-----------------------------------------------------------------------------
// Signal to stop the install. Use IsStopped() to poll or synchronously stop.
//-----------------------------------------------------------------------------
void CXboxInstaller::Stop()
{
#if defined( _DEMO ) || !defined( SUPPORTS_INSTALL_TO_XBOX_HDD )
	// under demo conditions cannot allow any HDD access
	return;
#endif

	if ( !m_bHasHDD )
	{
		return;
	}

	if ( m_bStopping )
	{
		// already stopping
		return;
	}

	if ( !m_hInstallThread || m_InstallData.m_bCompleted || m_InstallData.m_bFailed )
	{
		// already stopped
		return;
	}

	Msg( "Xbox Install: Stopping...\n" );
	m_bStopping = true;
}

//-----------------------------------------------------------------------------
// Discards the setup.
//-----------------------------------------------------------------------------
void CXboxInstaller::ResetSetup()
{
	V_memset( &m_CopyStats, 0, sizeof( CopyStats_t ) );
	m_InstallData.Reset();

	m_bHasHDD = false;
}

//-----------------------------------------------------------------------------
// Setup for install.
//-----------------------------------------------------------------------------
bool CXboxInstaller::Setup( bool bForceInstall )
{
	if ( BuildInstallScript( bForceInstall ) )
	{
		m_bHasHDD = PrepareCachePartitionForInstall();
	}

	return m_bHasHDD;
}

bool CXboxInstaller::IsFullyInstalled()
{
	return m_InstallData.m_bValid;
}

DWORD CXboxInstaller::GetTotalSize()
{
	return m_InstallData.m_nTotalSize;
}

DWORD CXboxInstaller::GetVersion()
{
	return m_InstallData.m_nVersion;
}

const CopyStats_t *CXboxInstaller::GetCopyStats()
{
	return &m_CopyStats;
}

//-----------------------------------------------------------------------------
// Identifies that we may or can be installing.
// Cannot go TRUE unless the filesystem allows the install.
//-----------------------------------------------------------------------------
bool CXboxInstaller::IsInstallEnabled()
{
	return m_bHasHDD;
}

//-----------------------------------------------------------------------------
// Returns TRUE if the install has completed and a restart is desireable
// to achieve a remount.
//-----------------------------------------------------------------------------
bool CXboxInstaller::ShouldRestart()
{
	// strong exact checking for paranoia reasons
	if ( IsFullyInstalled() && 
		!g_pFullFileSystem->IsInstalledToXboxHDDCache() &&
		g_pFullFileSystem->IsDVDHosted() )
	{
		// Install is valid and..
		// the game did not startup with the install already intact and...
		// the game is running with its primary paths set to the DVD and..
		return true;
	}

	return false;
}

bool CXboxInstaller::ForceCachePaths()
{
	bool bDidFixup = false;

	// restarting is valid also indicates we can patch the search paths
	// when the image is already installed, but the search paths haven't been patched, do it now
	if ( !m_bForcedCachePaths && ( ShouldRestart() || g_pFullFileSystem->IsInstalledToXboxHDDCache() ) )
	{
		// regardless of outcome, we only do it once
		// if it didn't occur, too bad, we don't do it ever again
		m_bForcedCachePaths = true;	
		bDidFixup = g_pFullFileSystem->FixupSearchPathsAfterInstall();
	}

	return bDidFixup;
}

void CXboxInstaller::SpewStatus()
{
	Msg( "Install Status:\n" );

	Msg( "Version: %d (%s) (Xbox)\n", GetVersion(), XBX_GetLanguageString() );
	Msg( "DVD Hosted: %s\n", g_pFullFileSystem->IsDVDHosted() ? "Enabled" : "Disabled" );

	if ( g_pFullFileSystem->IsInstalledToXboxHDDCache() )
	{
		Msg( "Existing Image Found.\n" );
	}

	if ( !IsInstallEnabled() || !xbox_install_allowed.GetBool() )
	{
		Msg( "Install Disabled.\n" );
	}

	if ( IsFullyInstalled() )
	{
		Msg( "Fully Installed.\n" );
	}

	Msg( "Progress: %d/%d MB\n", GetCopyStats()->m_BytesCopied/(1024*1024), GetTotalSize()/(1024*1024) );
}
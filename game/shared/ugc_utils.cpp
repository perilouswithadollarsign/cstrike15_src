//========= Copyright  Valve Corporation, All rights reserved. ============//
//
// Utility helper functions for dealing with UGC files
//
//==========================================================================//

#include "cbase.h"
#include "ugc_utils.h"
#include "logging.h"

#include "globalvars_base.h"
#include "strtools.h"

#ifdef CLIENT_DLL
	#include "imageutils.h"
#endif

#include "zip/XUnzip.h"
#include "vstdlib/jobthread.h"
#include "tier2/fileutils.h"

#if defined( _WIN32 )
#include <sys/utime.h>
#elif defined(OSX)
#include <utime.h>
#else
#include <sys/types.h>
#include <utime.h>
#endif

// FIXME: These need to be properly arranged to make this viable!
extern IFileSystem	*filesystem;


#define FILEREQUEST_IO_STALL_DELAY	30.0f // Seconds

#define DOWNLOAD_CHUNK_SIZE 10485760 // 10MB

#define THUMBNAIL_SMALL_WIDTH 256
#define THUMBNAIL_SMALL_HEIGHT 144

#if !defined( NO_STEAM )
extern CSteamAPIContext	*steamapicontext; // available on game clients
#endif // !NO_STEAM

#if !defined( NO_STEAM ) && !defined ( _PS3 )

Color g_WorkshopLogColor( 0, 255, 255, 255 );
BEGIN_DEFINE_LOGGING_CHANNEL( LOG_WORKSHOP, "Workshop", LCF_CONSOLE_ONLY, LS_WARNING, g_WorkshopLogColor );
ADD_LOGGING_CHANNEL_TAG( "UGCOperation" );
ADD_LOGGING_CHANNEL_TAG( "WorkshopOperation" );
END_DEFINE_LOGGING_CHANNEL();

ConVar cl_remove_old_ugc_downloads( "cl_remove_old_ugc_downloads", "1", FCVAR_RELEASE );

//-----------------------------------------------------------------------------
// Purpose: Helper function for Steam's remote storage interface
//-----------------------------------------------------------------------------
ISteamRemoteStorage *GetISteamRemoteStorage()
{
	return ( steamapicontext != NULL ) ? steamapicontext->SteamRemoteStorage() : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Helper function to get the map Published File ID from a map path
//-----------------------------------------------------------------------------
PublishedFileId_t GetMapIDFromMapPath( const char *pMapPath )
{
	char tmp[MAX_PATH];
	V_strcpy_safe( tmp, pMapPath );
	V_FixSlashes( tmp, '/' ); // internal path strings use forward slashes, make sure we compare like that.
	if ( V_strstr( tmp, "workshop/" ) )
	{
		V_StripFilename(tmp);
		V_StripTrailingSlash(tmp);
		const char* szDirName = V_GetFileName(tmp);
		return (PublishedFileId_t)V_atoui64(szDirName);
	}

	return 0;
}

bool UGCUtil_TimestampFile( const char *pFileRelativePath, uint32 uTimestamp )
{
	char chFullFilePathForTimestamp[ MAX_PATH ] = {0};
	if ( char const *pchFullPath = g_pFullFileSystem->RelativePathToFullPath( pFileRelativePath, "MOD", chFullFilePathForTimestamp, sizeof( chFullFilePathForTimestamp ) ) )
	{
		struct utimbuf tbuffer;
		tbuffer.modtime = tbuffer.actime = uTimestamp;
		int iResultCode = utime( pchFullPath, &tbuffer );

		// There is an inconsistency between what utime writes and what stat returns due to daylight savings. 
		// Check if what we wrote is being offset, then re-set the time to make it match what steam has recorded for last modify time. 
		uint32 unFileTimeFromStat = (uint32)g_pFullFileSystem->GetFileTime( pFileRelativePath, "MOD" );
		if ( unFileTimeFromStat != uTimestamp )
		{
			int32 nDLSOffset = unFileTimeFromStat - uTimestamp;
			tbuffer.modtime = tbuffer.actime = uTimestamp - nDLSOffset;
			iResultCode = utime( pchFullPath, &tbuffer );
#if defined ( DEBUG )
			unFileTimeFromStat = (uint32)g_pFullFileSystem->GetFileTime( pFileRelativePath, "MOD" );
			Assert( unFileTimeFromStat == uTimestamp );
#endif 
		}
		
		return ( iResultCode == 0 );
	}
	return false;
}

inline bool IsZip( void *z )
{
	return ( z && *(unsigned int *)z == 0x04034b50 );
}

//-----------------------------------------------------------------------------
// CUGCUnzipper
//-----------------------------------------------------------------------------

class CUGCUnzipJob : public CJob
{
public:
	CUGCUnzipJob( const char *szTargetFile, const char *szTempFile );
	virtual JobStatus_t DoExecute();
	bool IsFinished ( void ) const { return m_bIsFinished; }

private:
	bool m_bIsFinished;
	char m_szTargetFile[MAX_PATH];	// Game dir relative path for unzipped file
	char m_szTempFile[MAX_PATH];

};

CUGCUnzipJob::CUGCUnzipJob( const char *szTargetFile, const char *szTempFile )
{
	V_strcpy_safe( m_szTargetFile, szTargetFile );
	V_strcpy_safe( m_szTempFile, szTempFile );
	m_bIsFinished = false;
}

JobStatus_t CUGCUnzipJob::DoExecute()
{
	CUtlBuffer unzipBuf;
	const uint32 unUnzipBufSize = 10 * 1024 * 1024;
	unzipBuf.EnsureCapacity( unUnzipBufSize );

	if ( HZIP hz = OpenZip( m_szTempFile, 0, ZIP_FILENAME ) )
	{
		ZIPENTRY ze;
		ZRESULT zr = GetZipItem( hz, -1, &ze ); // get count
		if ( zr == ZR_OK )
		{
			Assert( ze.index == 1 ); // This code assumes there is just a single file in the zip.
			if ( ZR_OK == GetZipItem( hz, 0, &ze ) )
			{
				FileHandle_t fh = g_pFullFileSystem->Open( m_szTargetFile, "wb", "MOD" );
				uint32 unBytesWritten = 0;
				do 
				{
					zr = UnzipItem( hz, 0, unzipBuf.Base(), unUnzipBufSize, ZIP_MEMORY );
					uint32 unBytesToWrite = MIN( unUnzipBufSize, ze.unc_size - unBytesWritten );
					if ( unBytesToWrite > 0 )
						unBytesWritten += g_pFullFileSystem->Write( unzipBuf.Base(), unBytesToWrite, fh );
				} while ( zr == ZR_MORE );

				if ( zr != ZR_OK )	
				{
					char errorBuf[256];
					FormatZipMessage( zr, errorBuf, sizeof( errorBuf ) );
					Warning( "Failed unzipping entry '%s'. Reason: %s \n", ze.name, errorBuf );
				}
				g_pFullFileSystem->Close( fh );
			}
		}
		else
		{
			char errorBuf[256];
			FormatZipMessage( zr, errorBuf, sizeof( errorBuf ) );
			Warning( "Failed to get count of items. Reason: %s \n", errorBuf );
		}
		CloseZip( hz );
	}
	
	m_bIsFinished = true;
	return JOB_OK;
}

IThreadPool *g_pUGCUnzipThreadPool = NULL;

void UGCUtil_Shutdown()
{
	if ( g_pUGCUnzipThreadPool )
	{
		g_pUGCUnzipThreadPool->Stop();
		DestroyThreadPool( g_pUGCUnzipThreadPool );
		g_pUGCUnzipThreadPool = NULL;
	}
}

void UGCUtil_Init()
{
	Assert( g_pUGCUnzipThreadPool == NULL );
	if ( !g_pUGCUnzipThreadPool )
	{
		ThreadPoolStartParams_t params;
		params.nThreads = 1;
		params.nStackSize = 1024*1024;
		params.fDistribute = TRS_FALSE;
		g_pUGCUnzipThreadPool= CreateNewThreadPool();
		g_pUGCUnzipThreadPool->Start( params, "UGCUnzipThreadPool" );
	}
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CUGCFileRequest::CUGCFileRequest( void ) :
	m_hCloudID( k_UGCHandleInvalid ),
	m_UGCStatus( UGCFILEREQUEST_READY ),
	m_AsyncControl( NULL ),
	m_flIOStartTime( 0 ),
	m_flDownloadProgress( 0.0f ),
	m_tFileUpdateTime( 0 ),
	m_pUnzipJob( NULL )
{
	// Start with these disabled
	m_szFileName[0] = '\0';
	m_szTargetDirectory[0] = '\0';
	m_szTargetFilename[0] = '\0';
	m_szErrorText[0] = '\0';

#ifdef FILEREQUEST_IO_STALL
	m_nIOStallType = FILEREQUEST_STALL_DOWNLOAD;//FILEREQUEST_STALL_WRITE;
	m_flIOStallDuration = FILEREQUEST_IO_STALL_DELAY;	// seconds
#endif // FILEREQUEST_IO_STALL
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CUGCFileRequest::~CUGCFileRequest( void )
{
	// Finish the file i/o
	if ( m_AsyncControl != NULL )
	{
		g_pFullFileSystem->AsyncFinish( m_AsyncControl );
		g_pFullFileSystem->AsyncRelease( m_AsyncControl );
		m_AsyncControl = NULL;
	}

	// Clear our internal buffer
	m_bufContents.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Check if the file is in sync with the cloud
//-----------------------------------------------------------------------------
bool CUGCFileRequest::FileInSync( const char *lpszTargetDirectory, const char *lpszTargetFilename, uint32 timeUpdated )
{
	if ( lpszTargetFilename == NULL )
		return false;

	char chCheckTargetDirectory[MAX_PATH] = {0};
	V_strncpy( chCheckTargetDirectory, lpszTargetDirectory, sizeof( chCheckTargetDirectory ) );
	V_FixSlashes( chCheckTargetDirectory, '/' );
	if ( const char *pszWorkshopMapId = StringAfterPrefix( chCheckTargetDirectory, "maps/workshop/" ) )
	{
		PublishedFileId_t uiWorkshopMapId = Q_atoui64( pszWorkshopMapId );
		if ( UGCUtil_IsOfficialMap( uiWorkshopMapId ) )
			return true;
	}

#ifdef FILEREQUEST_IO_STALL
	return false;
#endif // FILEREQUEST_IO_STALL

	char szFilename[MAX_PATH];
	V_SafeComposeFilename( lpszTargetDirectory, lpszTargetFilename, szFilename, ARRAYSIZE(szFilename) );

	// If the file exists, we need to check it's information
	if ( g_pFullFileSystem->FileExists( szFilename ) )
	{
		if ( timeUpdated != 0 )
		{
			// mtime needs to match the time last updated exactly, as we slam the file time when we download
			// so an earlier time is out of date and a later time may be modified due to file copying. 
			uint32 fileTime = (uint32) g_pFullFileSystem->GetFileTime( szFilename, "MOD" );
			if ( timeUpdated == fileTime )
				return true;
		}
		else
		{
			// We didn't supply a time to check against, so we only cared about its existence
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Start a download by handle
//-----------------------------------------------------------------------------
UGCFileRequestStatus_t CUGCFileRequest::StartDownload(	UGCHandle_t hFileHandle, 
	const char *lpszTargetDirectory /*= NULL*/, 
	const char *lpszTargetFilename /*= NULL*/, 
	uint32 timeUpdated /*=0*/,
	bool bForceUpdate /*=false*/ )
{
	// Start with the assumption of failure
	m_UGCStatus = UGCFILEREQUEST_ERROR;
	m_tFileUpdateTime = timeUpdated;

	// First, see if this file is already down on the disk (unless we're overriding the call)
	if ( bForceUpdate == false && FileInSync( lpszTargetDirectory, lpszTargetFilename, timeUpdated ) )
	{
		m_hCloudID = hFileHandle;

		// Take a target directory for the file
		if ( lpszTargetDirectory != NULL )
		{
			V_strncpy( m_szTargetDirectory, lpszTargetDirectory, MAX_PATH );
			V_FixSlashes( m_szTargetDirectory );
		}

		// Take a target filename for the file
		if ( lpszTargetFilename != NULL )
		{
			V_strncpy( m_szTargetFilename,	lpszTargetFilename, MAX_PATH );
		}

#ifdef LOG_FILEREQUEST_PROGRESS
		Log_Msg( LOG_WORKSHOP, "[UGC] File %s%c%s already in sync on client. (Duration: %f seconds)\n", lpszTargetDirectory, CORRECT_PATH_SEPARATOR, lpszTargetFilename, gpGlobals->realtime-m_flIOStartTime );
#endif // LOG_FILEREQUEST_PROGRESS

		MarkCompleteAndFree( false );
		return m_UGCStatus;
	}

#ifdef LOG_FILEREQUEST_PROGRESS
	Log_Msg( LOG_WORKSHOP, "[UGC] Beginning download of %s%c%s. (%f)\n", lpszTargetDirectory, CORRECT_PATH_SEPARATOR, lpszTargetFilename, gpGlobals->realtime );
#endif // LOG_FILEREQUEST_PROGRESS

	// Start the download request
	uint32 nPriority = 0; // FIXME: For now, we always download at an equal priority
	SteamAPICall_t hSteamAPICall = GetISteamRemoteStorage()->UGCDownload( hFileHandle, nPriority );
	m_callbackUGCDownload.Set( hSteamAPICall, this, &CUGCFileRequest::Steam_OnUGCDownload );	

	if ( hSteamAPICall != k_uAPICallInvalid )
	{
		// Mark download as in progress
		m_UGCStatus = UGCFILEREQUEST_DOWNLOADING;
		m_hCloudID = hFileHandle;
		m_flIOStartTime = gpGlobals->realtime;

		// Take a target directory for the file
		if ( lpszTargetDirectory != NULL )
		{
			V_strncpy( m_szTargetDirectory, lpszTargetDirectory, MAX_PATH );
			V_FixSlashes( m_szTargetDirectory );
		}

		// Take a target filename for the file
		if ( lpszTargetFilename != NULL )
		{
			V_strncpy( m_szTargetFilename,	lpszTargetFilename, MAX_PATH );
		}

#ifdef FILEREQUEST_IO_STALL
		m_flIOStallStart = gpGlobals->realtime;
#endif // FILEREQUEST_IO_STALL

		// Start with an initialized value for our progress
		m_flDownloadProgress = 0.0f;

		// Done!
		return m_UGCStatus;
	}

	// We were unable to start our download through the Steam API
	return ThrowError( "[UGC] Failed to initiate download of file (%s%s%s) from cloud\n", lpszTargetDirectory, CORRECT_PATH_SEPARATOR, lpszTargetFilename );
}

//-----------------------------------------------------------------------------
// Purpose: Start an upload of a buffer by filename
//-----------------------------------------------------------------------------

UGCFileRequestStatus_t CUGCFileRequest::StartUpload( CUtlBuffer &buffer, const char *lpszFilename )
{
	// Start with the assumption of failure
	m_UGCStatus = UGCFILEREQUEST_ERROR;

#ifdef LOG_FILEREQUEST_PROGRESS
	Log_Msg( LOG_WORKSHOP, "[UGC] Saving %s to user cloud. (%f)\n", lpszFilename, gpGlobals->realtime );
#endif // LOG_FILEREQUEST_PROGRESS

	// Write the local copy of the file
	ISteamRemoteStorage *pRemoteStorage = GetISteamRemoteStorage();
	if ( pRemoteStorage == NULL )
		return ThrowError( "[UGC] Failed to write file to cloud\n" );

	UGCFileWriteStreamHandle_t hWriteStream = pRemoteStorage->FileWriteStreamOpen( lpszFilename );
	if ( hWriteStream == k_UGCFileStreamHandleInvalid )
		return ThrowError( "[UGC] Failed to write file to cloud\n" );

	uint32 nBytesWritten = 0;
	uint32 nBytesToWrite = buffer.TellPut();
	while ( nBytesToWrite )
	{
		uint32 nChunkSize = MIN( nBytesToWrite, (100*1024*1024) ); // 100Mb limit
		if ( !pRemoteStorage->FileWriteStreamWriteChunk( hWriteStream, buffer.PeekGet(nBytesWritten), nChunkSize ) )
		{
			// NOTE: This won't be necessary in future updates
			pRemoteStorage->FileWriteStreamCancel( hWriteStream );
			return ThrowError( "[UGC] Failed to write file to cloud\n" );
		}

		// Decrement the amount of bytes remaining
		nBytesToWrite -= nChunkSize;
	}

	if ( !pRemoteStorage->FileWriteStreamClose( hWriteStream ) )
	{
		return ThrowError( "[UGC] Failed to write file to cloud\n" );
	}

#ifdef LOG_FILEREQUEST_PROGRESS
	Log_Msg( LOG_WORKSHOP, "[UGC] Sharing %s to user cloud. (%f)\n", lpszFilename, gpGlobals->realtime );
#endif // LOG_FILEREQUEST_PROGRESS

	// Now share the file (uploads it to the cloud)
	SteamAPICall_t hSteamAPICall = pRemoteStorage->FileShare( lpszFilename );
	m_callbackFileShare.Set( hSteamAPICall, this, &CUGCFileRequest::Steam_OnFileShare );

#ifdef FILEREQUEST_IO_STALL
	m_flIOStallStart = gpGlobals->realtime;
#endif // FILEREQUEST_IO_STALL

	// Now, hold onto the filename
	V_ExtractFilePath( lpszFilename, m_szTargetDirectory, ARRAYSIZE( m_szTargetDirectory ) );
	V_StripTrailingSlash( m_szTargetDirectory );
	V_FixSlashes( m_szTargetDirectory );
	V_strncpy( m_szTargetFilename, V_UnqualifiedFileName( lpszFilename ), ARRAYSIZE( m_szTargetFilename ) );

	m_UGCStatus = UGCFILEREQUEST_UPLOADING;
	m_flIOStartTime = gpGlobals->realtime;

	return m_UGCStatus;
}

//-----------------------------------------------------------------------------
// Purpose: FileShare complete for a file request
//-----------------------------------------------------------------------------
void CUGCFileRequest::Steam_OnFileShare( RemoteStorageFileShareResult_t *pResult, bool bError )
{
	char szFilename[MAX_PATH];
	GetFullPath( szFilename, ARRAYSIZE(szFilename) );
	
	if ( bError )
	{
		ThrowError( "[UGC] Upload of file %s to Steam cloud failed!\n", szFilename );
		return;
	}

#ifdef LOG_FILEREQUEST_PROGRESS
	Log_Msg( LOG_WORKSHOP, "[UGC] File %s shared to user cloud. UGC ID: %llu (%f)\n", szFilename, pResult->m_hFile, gpGlobals->realtime );
#endif // LOG_FILEREQUEST_PROGRESS

	// Save the return handle
	m_hCloudID = pResult->m_hFile;

	MarkCompleteAndFree();
}

//-----------------------------------------------------------------------------
// Purpose: UGDownload complete for a file request
//-----------------------------------------------------------------------------
void CUGCFileRequest::Steam_OnUGCDownload( RemoteStorageDownloadUGCResult_t *pResult, bool bError )
{
	// Completed.  Did we succeed?
	if ( bError || pResult->m_eResult != k_EResultOK )
	{
		ThrowError( "[UGC] Download of file %s from cloud failed with result %d (%llu)\n", pResult->m_pchFileName, pResult->m_eResult, m_hCloudID );
		return;
	}

	// Make sure we got back the file we were expecting
	Assert( pResult->m_hFile == m_hCloudID );

	// Fetch file details
	AppId_t nAppID;
	char *pchName;
	int32 nFileSizeInBytes = -1;
	CSteamID steamIDOwner;
	ISteamRemoteStorage *pRemoteStorage = GetISteamRemoteStorage();
	if ( !pRemoteStorage->GetUGCDetails( m_hCloudID, &nAppID, &pchName, &nFileSizeInBytes, &steamIDOwner ) || nFileSizeInBytes <= 0 )
	{
		ThrowError( "[UGC] Unable to retrieve cloud file %s (%llu) info from Steam\n", pResult->m_pchFileName, pResult->m_hFile );
		return;
	}

	// Save our name
	V_strncpy( m_szFileName, pchName, sizeof(m_szFileName) );

	bool bBSPFile = false;
	if ( V_strnicmp( V_GetFileExtensionSafe(m_szFileName), "bsp", 3 ) == 0 )
	{
		bBSPFile = true;
	}

	// Take this as our target if we haven't specified one
	if ( m_szTargetFilename[0] == '\0' )
	{		
		V_strncpy( m_szTargetFilename, V_GetFileName( pchName ), sizeof(m_szTargetFilename) );
	}

#ifdef LOG_FILEREQUEST_PROGRESS
	Log_Msg( LOG_WORKSHOP, "[UGC] Read cloud file %s%c%s (%llu) (%f)\n", m_szTargetDirectory, CORRECT_PATH_SEPARATOR, m_szTargetFilename, m_hCloudID, gpGlobals->realtime );
#endif // LOG_FILEREQUEST_PROGRESS

	char szLocalFullPath[MAX_PATH];
	GetFullPath( szLocalFullPath, sizeof(szLocalFullPath) );

	// Make sure the directory exists if we're creating one
	if ( m_szTargetDirectory != NULL )
	{
		filesystem->CreateDirHierarchy( m_szTargetDirectory, "MOD" );
	}
	else
	{
		char szDirectory[MAX_PATH];
		Q_FileBase( szLocalFullPath, szDirectory, sizeof(szDirectory) );
		filesystem->CreateDirHierarchy( szDirectory, "MOD" );
	}

	// Allocate a temporary buffer
	m_bufContents.Purge();
	if ( bBSPFile )
	{
		m_bufContents.EnsureCapacity( DOWNLOAD_CHUNK_SIZE );
		m_bufContents.SeekPut( CUtlBuffer::SEEK_HEAD, DOWNLOAD_CHUNK_SIZE );
	}
	else
	{
		m_bufContents.EnsureCapacity( nFileSizeInBytes );
		m_bufContents.SeekPut( CUtlBuffer::SEEK_HEAD, nFileSizeInBytes );
	}

	// Read in the data and save to tmp file
	FileHandle_t fh = NULL;
	char szZipFullPath[ MAX_PATH ];
	if ( bBSPFile )
	{
		GetFullPath( szZipFullPath, sizeof(szZipFullPath) );
		V_SetExtension( szZipFullPath, ".zip", MAX_PATH );
		fh = g_pFullFileSystem->Open( szZipFullPath, "wb", "MOD" );
	}

	bool bZipFile = false;
	uint32 nOffset = 0;
	while ( (int32) nOffset < nFileSizeInBytes )
	{
		uint32 nChunkSize = MIN( (nFileSizeInBytes-nOffset), DOWNLOAD_CHUNK_SIZE ); // 10Mb 
		void *pDest = NULL;
		if ( bBSPFile )
		{
			pDest = (char *) m_bufContents.Base();
		}
		else
		{
			pDest = (char *) m_bufContents.Base() + nOffset;
		}

		int32 nBytesRead = pRemoteStorage->UGCRead( m_hCloudID, pDest, nChunkSize, nOffset, k_EUGCRead_ContinueReadingUntilFinished );
		if ( nBytesRead <= 0 )
		{
			ThrowError( "[UGC] Failed call to UGCRead on cloud file %s (%llu)\n", pResult->m_pchFileName, m_hCloudID );
			return;
		}

		if ( bBSPFile )
		{
			g_pFullFileSystem->Write( m_bufContents.Base(), nBytesRead, fh );
			if ( nOffset == 0 && IsZip( m_bufContents.Base() ) )
			{
				bZipFile = true;
			}
		}

		nOffset += nBytesRead;
	}

	if ( bBSPFile )
	{
		g_pFullFileSystem->Close( fh );
		m_bufContents.Purge();
	}

#ifdef LOG_FILEREQUEST_PROGRESS
	Log_Msg( LOG_WORKSHOP, "[UGC] Start unzip file %s%c%s (%llu) (%f)\n", m_szTargetDirectory, CORRECT_PATH_SEPARATOR, m_szTargetFilename, m_hCloudID, gpGlobals->realtime );
#endif // LOG_FILEREQUEST_PROGRESS

	// If authors rename the map file, old versions get orphaned in the workshop directory. Nuke any bsp here.
	if ( cl_remove_old_ugc_downloads.GetBool() )
	{
		CUtlVector<CUtlString> outList;
		AddFilesToList( outList, m_szTargetDirectory, "MOD", "bsp" );
		FOR_EACH_VEC( outList, i )
		{
			filesystem->RemoveFile( outList[i] );
		}
	}

	if ( bZipFile )
	{
		g_pFullFileSystem->RelativePathToFullPath( szZipFullPath, "MOD", szZipFullPath, MAX_PATH );
		m_UGCStatus = UGCFILEREQUEST_UNZIPPING;
		m_pUnzipJob = new CUGCUnzipJob( szLocalFullPath, szZipFullPath );
		m_pUnzipJob->SetFlags( JF_IO );
		g_pUGCUnzipThreadPool->AddJob( m_pUnzipJob );

#ifdef LOG_FILEREQUEST_PROGRESS
			Log_Msg( LOG_WORKSHOP, "[UGC] Unzipping started for %s (%llu) (%f)\n", szLocalFullPath, m_hCloudID, gpGlobals->realtime );
#endif // LOG_FILEREQUEST_PROGRESS
	}
	else if ( bBSPFile )
	{
		// file was downloaded with .zip extension, but it's not zipped, so rename it to the target name (bsp) and we're done
		filesystem->RenameFile( szZipFullPath, szLocalFullPath, "MOD" );
		MarkCompleteAndFree();
	}
	else
	{
		char szLocalFullPath[MAX_PATH];
		V_strcpy_safe( szLocalFullPath, GetDirectory() );
		g_pFullFileSystem->RelativePathToFullPath( szLocalFullPath, "MOD", szLocalFullPath, MAX_PATH );
		V_SafeComposeFilename( szLocalFullPath, GetFilename(), szLocalFullPath, sizeof(szLocalFullPath) );

		// Async write this to disc with monitoring
		if ( g_pFullFileSystem->AsyncWrite( szLocalFullPath, m_bufContents.Base(), m_bufContents.TellPut(), false, false, &m_AsyncControl ) < 0 )
		{
			// Async write failed immediately!
			ThrowError( "[UGC] Async write of downloaded file %s failed\n", szLocalFullPath );
			return;
		}				

#ifdef LOG_FILEREQUEST_PROGRESS
		Log_Msg( LOG_WORKSHOP, "[UGC] Async write started for %s (%llu) (%f)\n", szLocalFullPath, m_hCloudID, gpGlobals->realtime );
#endif // LOG_FILEREQUEST_PROGRESS

		// Mark us as having started out download
		m_UGCStatus = UGCFILEREQUEST_DOWNLOAD_WRITING;
	}
}

void CUGCFileRequest::UpdateUnzip()
{
	if ( m_pUnzipJob->IsFinished() )
	{
		// clean up zip file
		char szZipFullPath[ MAX_PATH ];
		GetFullPath( szZipFullPath, sizeof(szZipFullPath) );
		V_SetExtension( szZipFullPath, ".zip", MAX_PATH );
		filesystem->RemoveFile( szZipFullPath, "MOD" );

		MarkCompleteAndFree();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Poll for status and drive the process forward
//-----------------------------------------------------------------------------

UGCFileRequestStatus_t CUGCFileRequest::Update( void )
{
	switch ( m_UGCStatus )
	{
	case UGCFILEREQUEST_UNZIPPING:
		{
			UpdateUnzip();
			return m_UGCStatus;
		}
		break;

		// Handle the async write of the file to disc
	case UGCFILEREQUEST_DOWNLOAD_WRITING:
		{
#ifdef FILEREQUEST_IO_STALL
			if ( m_nIOStallType == FILEREQUEST_STALL_WRITE )
			{
				// If we're stalling, then pretend that we're going at a uniformly slow pace through the duration of the stall time
				const float flStallTime = gpGlobals->realtime - m_flIOStallStart;
				m_flDownloadProgress = RemapValClamped( flStallTime, 0, m_flIOStallDuration, 0.0f, 1.0f );
				
				if ( flStallTime < m_flIOStallDuration )
					return UGCFILEREQUEST_DOWNLOAD_WRITING;
			}
#endif // FILEREQUEST_IO_STALL

			// Monitor the async write progress and clean up after we're done
			if ( m_AsyncControl )
			{
				FSAsyncStatus_t status = g_pFullFileSystem->AsyncStatus( m_AsyncControl );
				switch ( status )
				{
				case FSASYNC_STATUS_PENDING:
				case FSASYNC_STATUS_INPROGRESS:
				case FSASYNC_STATUS_UNSERVICED:
					return UGCFILEREQUEST_DOWNLOAD_WRITING;

				case FSASYNC_ERR_FILEOPEN:
					return ThrowError( "[UGC] Unable to write file to disc!\n" );
				}

				// Finish the read
				g_pFullFileSystem->AsyncFinish( m_AsyncControl );
				g_pFullFileSystem->AsyncRelease( m_AsyncControl );
				m_AsyncControl = NULL;

#ifdef LOG_FILEREQUEST_PROGRESS
				Log_Msg( LOG_WORKSHOP, "[UGC] Async write completed for %s%c%s (%llu) (%f)\n", m_szTargetDirectory, CORRECT_PATH_SEPARATOR, m_szTargetFilename, m_hCloudID, gpGlobals->realtime );
#endif // LOG_FILEREQUEST_PROGRESS

				MarkCompleteAndFree();
				return m_UGCStatus;
			}

			// Somehow we lost the handle to our async status or got a spurious call in here!
			return ThrowError( "[UGC] Lost handle to async handle for downloaded file write!\n" );
		}
		break;

		// Handle starting up a download
	case UGCFILEREQUEST_READY:
	case UGCFILEREQUEST_UPLOADING:
		return m_UGCStatus;
		break;

	case UGCFILEREQUEST_FINISHED:
		// Progress is complete
		m_flDownloadProgress = 1.0f;
		return m_UGCStatus;
		break;

	case UGCFILEREQUEST_DOWNLOADING:
		{
#ifdef FILEREQUEST_IO_STALL
			// If we're stalling, then pretend that we're going at a uniformly slow pace through the duration of the stall time
			m_flDownloadProgress = RemapValClamped( ( gpGlobals->realtime - m_flIOStallStart ), 0, m_flIOStallDuration, 0.0f, 1.0f );
#else
			// Find the progress of our current download
			int32 nBytesDownloaded, nBytesExpected;
			GetISteamRemoteStorage()->GetUGCDownloadProgress( m_hCloudID, &nBytesDownloaded, &nBytesExpected );
			if ( nBytesExpected != 0 )
			{
				// Store off our progress on this file
				m_flDownloadProgress = ((float)nBytesDownloaded/(float)nBytesExpected);
			}
#endif // FILEREQUEST_IO_STALL

			return m_UGCStatus;
		}
		break;

		// An error has occurred while trying to handle the user's request
	default:
	case UGCFILEREQUEST_ERROR:
		return UGCFILEREQUEST_ERROR;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the local file name on disk, accounting for target directories and filenames
//-----------------------------------------------------------------------------
void CUGCFileRequest::GetFullPath( char *pDest, size_t strSize ) const
{
	V_SafeComposeFilename( GetDirectory(), GetFilename(), pDest, strSize );
}

//-----------------------------------------------------------------------------
// Purpose: Name on disk if not the same as in the cloud
//-----------------------------------------------------------------------------
const char *CUGCFileRequest::GetFilename( void ) const 
{ 
	return ( m_szTargetFilename[0] == '\0' ) ? m_szFileName : m_szTargetFilename; 
}

//-----------------------------------------------------------------------------
// Purpose: Get the local directory on disk, accounting for target directories
//-----------------------------------------------------------------------------
const char *CUGCFileRequest::GetDirectory( void ) const
{
	return ( m_szTargetDirectory[0] == '\0' ) ? NULL : m_szTargetDirectory;
}

//
// Marks the file request as complete and frees its internal buffers
//

void CUGCFileRequest::MarkCompleteAndFree( bool bUpdated /*= true*/ ) 
{ 
	m_bufContents.Purge();
	m_UGCStatus = UGCFILEREQUEST_FINISHED;

#ifdef LOG_FILEREQUEST_PROGRESS
	char szFilename[MAX_PATH];
	GetFullPath( szFilename, ARRAYSIZE(szFilename) );

	Log_Msg( LOG_WORKSHOP, "[UGC] File %s (%llu) finished all operations! (Duration: %f seconds)\n", szFilename, m_hCloudID, gpGlobals->realtime-m_flIOStartTime );
#endif // LOG_FILEREQUEST_PROGRESS

#ifdef CLIENT_DLL
	if ( StringHasPrefix( GetFilename(), "thumb" ) && V_strstr( GetFilename(), ".jpg" ) )
	{
		CreateSmallThumbNail( bUpdated );
	}
#endif

	//
	// Timestamp the file to match workshop updated timestamp
	//
	if ( m_tFileUpdateTime )
		UGCUtil_TimestampFile( szFilename, m_tFileUpdateTime );

	if ( m_pUnzipJob )
	{
		m_pUnzipJob->Release();
		m_pUnzipJob = NULL;
	}
}

#ifdef CLIENT_DLL
void CUGCFileRequest::CreateSmallThumbNail( bool bForce )
{
	char szFilename[ MAX_PATH ];
	GetFullPath( szFilename, sizeof(szFilename) );

	char szFullFilename[ MAX_PATH ];
	g_pFullFileSystem->RelativePathToFullPath( szFilename, "MOD", szFullFilename, sizeof( szFullFilename ) );

	char szSmallFilename[ MAX_PATH ];
	V_strncpy( szSmallFilename, szFullFilename, sizeof(szSmallFilename) );

	char *pchExt = V_strrchr( szSmallFilename, '.' );
	if ( !pchExt )
		return;

	*pchExt = '\0';
	V_strncat( szSmallFilename, "_s.jpg", sizeof( szSmallFilename ) );

	if ( !bForce && g_pFullFileSystem->FileExists( szSmallFilename ) )
		return;

	int width, height;
	ConversionErrorType errCode;
	unsigned char *pThumbnailData = ImgUtl_ReadImageAsRGBA( szFullFilename, width, height, errCode );
	if ( errCode != CE_SUCCESS )
	{
		DevMsg( "Failed to read thumbnail %s.\n", szFullFilename );
		return;
	}

	// Now convert the image to an appropriate size for preview
	unsigned char *pThumbnailSmallData = NULL;
	const unsigned int nThumbnailSmallWidth = THUMBNAIL_SMALL_WIDTH;
	const unsigned int nThumbnailSmallHeight = THUMBNAIL_SMALL_HEIGHT;

	if ( !ResizeRGBAImage( pThumbnailData, (width*height*4), width, height, &pThumbnailSmallData, nThumbnailSmallWidth, nThumbnailSmallHeight ) )
	{
		DevMsg( "Failed to resize small thumbnail %s.\n", szSmallFilename );
		free( pThumbnailData );
		return;
	}

	if ( ImgUtl_WriteRGBAToJPEG( pThumbnailSmallData, nThumbnailSmallWidth, nThumbnailSmallHeight, szSmallFilename ) != CE_SUCCESS )
	{
		DevMsg( "Failed to write small thumbnail %s.\n", szSmallFilename );
	}

	free( pThumbnailData );
	free( pThumbnailSmallData );
}

//-----------------------------------------------------------------------------
// Purpose: Validate, resize and convert a RGBA to a properly formatted size
//-----------------------------------------------------------------------------
bool CUGCFileRequest::ResizeRGBAImage( const unsigned char *pData, unsigned int nDataSize, unsigned int nWidth, unsigned int nHeight, 
									  unsigned char **pDataOut, unsigned int nNewWidth, unsigned int nNewHeight )
{
	// Find out how to squish the image to fit within our necessary borders
	float flFrameRatio = ( (float) nNewWidth / (float) nNewWidth );
	float flSourceRatio = ( (float) nWidth / (float) nHeight );
	unsigned int nScaleWidth;
	unsigned int nScaleHeight;

	if ( flSourceRatio < flFrameRatio )
	{
		nScaleWidth = nNewWidth;
		nScaleHeight = ( nNewWidth / flSourceRatio );
	}
	else if ( flSourceRatio > flFrameRatio )
	{
		nScaleWidth = ( nNewHeight * flSourceRatio );
		nScaleHeight = nNewHeight;
	}
	else
	{
		nScaleWidth = nNewWidth;
		nScaleHeight = nNewHeight;
	}

	// Allocate a buffer to hold the scaled image
	unsigned char *pScaleBuf = (unsigned char *) malloc( nScaleWidth * nScaleHeight * 4 );
	if ( pScaleBuf == NULL )
		return false;

	// FIXME: Combine these helper functions into one operation, rather than multiple!
	// Scale the image to the proper size
	if ( ImgUtl_StretchRGBAImage( pData, nWidth, nHeight, pScaleBuf, nScaleWidth, nScaleHeight ) == CE_SUCCESS )
	{
		// Allocate a buffer to pad this image out to
		*pDataOut = (unsigned char *) malloc( nNewWidth * nNewHeight * 4 );
		if ( *pDataOut == NULL )
		{
			free( pScaleBuf );
			return false;
		}

		// Calc the offset for the image to be centered after cropping
		unsigned int cropX = ( nScaleWidth - nNewWidth ) / 2;
		unsigned int cropY = ( nScaleHeight - nNewHeight ) / 2;

		// Crop the image down to size
		if ( ImgUtl_CropRGBA( cropX, cropY, nScaleWidth, nScaleHeight, nNewWidth, nNewHeight, pScaleBuf, *pDataOut ) != CE_SUCCESS )
		{
			free( *pDataOut );
			return false;
		}
	}

	// Release it!
	free( pScaleBuf );

	return true;
}
#endif

//
//  Sets the file request into an error state
//

UGCFileRequestStatus_t CUGCFileRequest::ThrowError( const char *lpszFormat, ... )
{
	va_list marker;
	va_start( marker, lpszFormat );
	Q_vsnprintf( m_szErrorText, sizeof( m_szErrorText ), lpszFormat, marker );
	va_end( marker );	

#ifdef LOG_FILEREQUEST_PROGRESS
	Log_Warning( LOG_WORKSHOP, "%s", m_szErrorText );
#endif // LOG_FILEREQUEST_PROGRESS

	m_UGCStatus = UGCFILEREQUEST_ERROR;

	return m_UGCStatus;
}

#endif // !NO_STEAM

//-----------------------------------------------------------------------------
// Purpose: Same as V_ComposeFilename but can deal with NULL pointers for the directory (meaning non-existant)
//-----------------------------------------------------------------------------
void V_SafeComposeFilename( const char *pPathIn, const char *pFilenameIn, char *pDest, size_t nDestSize )
{
	// If we've passed in a directory, then start with it
	if ( pPathIn != NULL )
	{
		V_strncpy( pDest, pPathIn, nDestSize );
		V_FixSlashes( pDest );
		V_AppendSlash( pDest, nDestSize );
	}
	else
	{
		// Make sure we're clear
		pDest[0] = '\0';
	}

	if ( pFilenameIn != NULL )
	{
		// Tack on the filename and fix slashes
		V_strncat( pDest, pFilenameIn, nDestSize, COPY_ALL_CHARACTERS );
		V_FixSlashes( pDest );
	}
}


bool UnzipFile( const char* szPathToZipFile,  const char* szOutputDir /*= NULL*/)
{	
	char outPath[MAX_PATH];
	if ( szOutputDir )
	{
		V_strcpy_safe( outPath, szOutputDir );
	}
	else
	{
		V_ExtractFilePath( szPathToZipFile, outPath, sizeof(outPath) );
	}

	bool bSuccess = false;
	HZIP hz = OpenZip( (void*)szPathToZipFile, 0, ZIP_FILENAME );
	if ( hz )
	{
		ZIPENTRY ze;
		ZRESULT zr = GetZipItem( hz, -1, &ze );
		if ( zr == ZR_OK )
		{
			uint32 count = ze.index;
			uint32 successCount = 0;
			for ( uint32 i = 0; i < count; ++i )
			{
				if ( ZR_OK == GetZipItem( hz, i, &ze ) )
				{
					char dest[MAX_PATH];
					V_ComposeFileName( outPath, ze.name, dest, sizeof(dest) );
					zr = UnzipItem( hz, i, (void*)dest, 0, ZIP_FILENAME );
					if ( zr == ZR_OK )	
					{
						successCount++;
					}
					else
					{
						char errorBuf[256];
						FormatZipMessage( zr, errorBuf, sizeof( errorBuf ) );
						Warning( "Failed unzipping entry '%s' in zip file '%s'. Reason: %s \n", ze.name, szPathToZipFile, errorBuf );
					}
				}
			}

			bSuccess = count == successCount;
		}
		else
		{
			char errorBuf[256];
			FormatZipMessage( zr, errorBuf, sizeof( errorBuf ) );
			Warning( "Failed to get count of items in zip file '%s'. Reason: %s \n", szPathToZipFile, errorBuf );
		}
		CloseZip( hz );
	}
	else
	{
		char errorBuf[256];
		FormatZipMessage( ZR_RECENT, errorBuf, sizeof(errorBuf ) );
		Warning( "Failed to open zip file '%s'. Reason: %s\n", szPathToZipFile, errorBuf );
	}

	return bSuccess;
}

bool UGCUtil_IsOfficialMap( PublishedFileId_t id )
{
	/** Removed for partner depot **/
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the local file name on disk, accounting for target directories and filenames
//-----------------------------------------------------------------------------
void CUGCFileRequest::GetLocalFileName( char *pDest, size_t strSize )
{
	if ( m_szTargetDirectory[0] == '\0' )
	{
		V_strncpy( pDest, GetFilename(), strSize );
	}
	else
	{
		V_snprintf( pDest, strSize, "%s/%s", m_szTargetDirectory, GetFilename() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the local directory on disk, accounting for target directories
//-----------------------------------------------------------------------------
void CUGCFileRequest::GetLocalDirectory( char *pDest, size_t strSize )
{
	if ( m_szTargetDirectory[0] == '\0' )
	{
		V_strncpy( pDest, "\0", strSize );
	}
	else
	{
		V_strncpy( pDest, m_szTargetDirectory, strSize );
	}
}

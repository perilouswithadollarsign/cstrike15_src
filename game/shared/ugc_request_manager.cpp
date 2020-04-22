//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Manager for handling UGC file requests
//
//========================================================================//

#include "cbase.h"
#include "ugc_request_manager.h"

#if !defined(NO_STEAM) && !defined(_PS3)

static uint64 g_TimeStampIncr = 0;

//-----------------------------------------------------------------------------
// LessFunc for UGC operation (priority / timestamp)
//-----------------------------------------------------------------------------
bool UGCOperationsLessFunc( UGCFileRequest_t * const &lhs, UGCFileRequest_t * const &rhs )
{ 
	// If the priorities are equal, then we tie-break on the time they were submitted (rhs wins if another tie occurs)
	if ( lhs->GetPriority() == rhs->GetPriority() )
		return ( lhs->GetTimestamp() >= rhs->GetTimestamp() );

	return ( lhs->GetPriority() < rhs->GetPriority() );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CUGCFileRequestManager::CUGCFileRequestManager( void )
{
	m_PendingFileOperations.SetLessFunc( UGCOperationsLessFunc );
	m_FileRequests.SetLessFunc( DefLessFunc( UGCHandle_t ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUGCFileRequestManager::DeleteFileRequest( UGCHandle_t handle, bool bRemoveFromDisk /*= false*/ )
{
	if ( handle == k_UGCHandleInvalid )
		return false;

	const UGCFileRequest_t *pRequest = GetFileRequestByHandle( handle );
	if ( pRequest == NULL )
		return false;

	// Clear it from our pending work	
	for ( int i=0; i < m_PendingFileOperations.Count(); i++ )
	{
		UGCFileRequest_t *pQueueRequest = m_PendingFileOperations.Element( i );
		if ( pQueueRequest && pQueueRequest->fileHandle == handle )
		{
			m_PendingFileOperations.RemoveAt( i );
			break;
		}
	}

	// Remove it from our library
	if ( m_FileRequests.Remove( handle ) == false )
		return false;

	// Clean it off disk as well
	if ( bRemoveFromDisk )
	{

		char szLocalFilename[MAX_PATH];
		pRequest->fileRequest.GetFullPath( szLocalFilename, sizeof(szLocalFilename) );
		g_pFullFileSystem->RemoveFile( szLocalFilename );
	}
	
	delete pRequest;
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUGCFileRequestManager::Update( void )
{
	while ( m_PendingFileOperations.Count() )
	{
		UGCFileRequest_t *pFileRequest = m_PendingFileOperations.ElementAtHead();
	
		Assert( pFileRequest != NULL );
		if ( pFileRequest == NULL )
		{
			// FIXME: Throw a warning			
			m_PendingFileOperations.RemoveAtHead();
			continue;
		}

		UGCFileRequestStatus_t ugcStatus = pFileRequest->fileRequest.Update();
		switch ( ugcStatus )
		{
		case UGCFILEREQUEST_ERROR:
			{
				Warning("An error occurred while attempting to download a file from the UGC server!\n");
				// Msg("- Deleted: %llu\tPriority:%u\tTimestamp:%u\n", pFileRequest->fileHandle, pFileRequest->unPriority, pFileRequest->unTimestamp );
				m_PendingFileOperations.RemoveAtHead();				
			}
			break;

		case UGCFILEREQUEST_FINISHED:
			{
				// If we finished an upload, we need to move the file over into the main library
				// FIXME: The library is usually downloaded files ready on disk. Now it means things in the clouds or things on disk...
				if ( pFileRequest->nType == UGC_REQUEST_UPLOAD )
				{
					// If this is invalid, we didn't capture our final file handle properly
					Assert( pFileRequest->fileRequest.GetCloudHandle() != k_UGCHandleInvalid );
					if ( pFileRequest->fileRequest.GetCloudHandle() != k_UGCHandleInvalid )
					{
						pFileRequest->fileHandle = pFileRequest->fileRequest.GetCloudHandle();
						// Add this into the main list now that it's completed
						m_FileRequests.Insert( pFileRequest->fileHandle, pFileRequest );
						
						Log_Msg( LOG_WORKSHOP, "[CUGCRequestManager] Finished uploading %llu\n", pFileRequest->fileHandle );
					}
				}
				else
				{

					Log_Msg( LOG_WORKSHOP, "[CUGCRequestManager] Finished downloading %llu\n", pFileRequest->fileHandle );

					IGameEvent *pEvent = gameeventmanager->CreateEvent( "ugc_file_download_finished" );
					if ( pEvent )
					{
						pEvent->SetUint64( "hcontent", pFileRequest->GetFileHandle() );
						gameeventmanager->FireEventClientSide( pEvent );
					}	
				}

				// We're done, continue on!
				// Msg("- Deleted: %llu\tPriority:%u\tTimestamp:%u\n", pFileRequest->fileHandle, pFileRequest->unPriority, pFileRequest->unTimestamp );
				m_PendingFileOperations.RemoveAtHead();
			}
			break;

		case UGCFILEREQUEST_READY:
			{
				if ( pFileRequest->nType == UGC_REQUEST_DOWNLOAD )
				{
					// Pass along target directory and filename unless they're not set
					const char *lpszTargetDirectory = ( pFileRequest->szTargetDirectory[0] != '\0' ) ? pFileRequest->szTargetDirectory : NULL;
					const char *lpszTargetFilename  = ( pFileRequest->szTargetFilename[0] != '\0' )  ? pFileRequest->szTargetFilename : NULL;
				
					// We're ready to download, so start us off
					UGCFileRequestStatus_t status = pFileRequest->fileRequest.StartDownload( pFileRequest->fileHandle, lpszTargetDirectory, lpszTargetFilename, pFileRequest->unLastUpdateTime, pFileRequest->bForceUpdate );
					if ( status == UGCFILEREQUEST_FINISHED )
					{
						// We're already done (file was on disk)
						// FIXME: Roll this into the function call above!
						// Msg("- Deleted: %llu\tPriority:%u\tTimestamp:%u\n", pFileRequest->fileHandle, pFileRequest->unPriority, pFileRequest->unTimestamp );
						m_PendingFileOperations.RemoveAtHead();
					}


					if ( status == UGCFILEREQUEST_DOWNLOADING )
					{
						IGameEvent *pEvent = gameeventmanager->CreateEvent( "ugc_file_download_start" );
						if ( pEvent )
						{
							pEvent->SetUint64( "hcontent", pFileRequest->GetFileHandle() );
							pEvent->SetUint64( "published_file_id", pFileRequest->GetPublishedFileID() );
							gameeventmanager->FireEventClientSide( pEvent );
						}
					}

					Log_Msg( LOG_WORKSHOP, "[CUGCRequestManager] Beginning download of %llu\n", pFileRequest->fileHandle );

					return;
				}
				else if ( pFileRequest->nType == UGC_REQUEST_UPLOAD )
				{
					const char *lpszTargetDirectory = ( pFileRequest->szTargetDirectory[0] != '\0' ) ? pFileRequest->szTargetDirectory : NULL;
					const char *lpszTargetFilename  = ( pFileRequest->szTargetFilename[0] != '\0' )  ? pFileRequest->szTargetFilename : NULL;

					char szFullPath[MAX_PATH];
					V_SafeComposeFilename( lpszTargetDirectory, lpszTargetFilename, szFullPath, ARRAYSIZE(szFullPath) );

					// FIXME: Bleh, this makes all kinds of contracts we don't like!
					CUtlBuffer buffer;
					// FIXME: Swap for an async read!
					if ( !g_pFullFileSystem->ReadFile( pFileRequest->szSourceFilename, "GAME", buffer ) )
					{
						// We failed to read this off the disk
						buffer.Purge();
						pFileRequest->fileRequest.ThrowError( "Unable to read file: %s\n", pFileRequest->szSourceFilename );
						return;
					}

					// We're ready to download, so start us off
					UGCFileRequestStatus_t status = pFileRequest->fileRequest.StartUpload( buffer, szFullPath );
					if ( status == UGCFILEREQUEST_ERROR )
					{
						// FIXME: Now what?
						// m_PendingFileOperations.RemoveAtHead();
						Assert( 0 );
					}

					Log_Msg( LOG_WORKSHOP, "[CUGCRequestManager] Beginning upload of %s\n", szFullPath );

					// Done with the memory
					buffer.Purge();
					
					return;
				}
			}
			break;
		
		default:
			// Working, continue to wait...
			return;
			break;
		}
		
		// The request is complete, continue to the next!
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUGCFileRequestManager::CreateFileDownloadRequest( UGCHandle_t unFileHandle, PublishedFileId_t fileID, const char *lpszTargetDirectory, const char *lpszTargetFilename, uint32 unPriority, uint32 unLastUpdateTime /*=0*/, bool bForceUpdate /*= false*/ )
{
	// Must pass in a valid handle if we're downloading
	if ( unFileHandle == k_UGCHandleInvalid )
		return false;

	// Make sure we don't already have a request by this handle
	if ( FileRequestExists( unFileHandle ) )
		return true;

	UGCFileRequest_t *pRequest = new UGCFileRequest_t;
	pRequest->nType = UGC_REQUEST_DOWNLOAD;
	pRequest->fileHandle = unFileHandle;
	pRequest->publishedFileID = fileID;
	
	if ( lpszTargetDirectory != NULL )
	{
		V_strncpy( pRequest->szTargetDirectory, lpszTargetDirectory, ARRAYSIZE(pRequest->szTargetDirectory) );
		V_FixSlashes( pRequest->szTargetDirectory );
	}

	if ( lpszTargetFilename != NULL )
	{
		V_strncpy( pRequest->szTargetFilename, lpszTargetFilename, ARRAYSIZE(pRequest->szTargetFilename) );
	}

	pRequest->unLastUpdateTime	= unLastUpdateTime;
	pRequest->bForceUpdate		= bForceUpdate;
	pRequest->unTimestamp		= g_TimeStampIncr++;	// FIXME: This is to get around some timestamping, in essence larger numbers = newer additions
	pRequest->unPriority		= unPriority;

	// This insert will sort the request into the list properly
	m_PendingFileOperations.Insert( pRequest );

	// For debugging insertion into priority queue
	Log_Msg( LOG_WORKSHOP, "[CUGCFileRequestManager] Inserted Download: %llu\tPriority:%u\tTimestamp:%u\n", pRequest->fileHandle, pRequest->unPriority, pRequest->unTimestamp );
	Debug_LogPendingOperations();

	// Keep this in our records now
	m_FileRequests.Insert( unFileHandle, pRequest );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUGCFileRequestManager::CreateFileUploadRequest( const char *lpszSourceFilename, const char *lpszTargetDirectory, const char *lpszTargetFilename, uint32 unPriority )
{
	if ( lpszSourceFilename == NULL )
		return false;

	// Make sure we don't already have a request for this
	char szFullPath[MAX_PATH];
	V_SafeComposeFilename( lpszTargetDirectory, lpszTargetFilename, szFullPath, ARRAYSIZE(szFullPath) );

	// Make sure we don't have another upload by the same filename in progress
	const UGCFileRequest_t *pDuplicateRequest = GetFileRequestByFilename( szFullPath );
	if ( pDuplicateRequest != NULL && pDuplicateRequest->nType == UGC_REQUEST_UPLOAD )
		return true;

	UGCFileRequest_t *pRequest = new UGCFileRequest_t;
	pRequest->nType = UGC_REQUEST_UPLOAD;
	pRequest->fileHandle = k_UGCHandleInvalid;

	if ( lpszTargetDirectory != NULL )
	{
		V_strncpy( pRequest->szTargetDirectory, lpszTargetDirectory, ARRAYSIZE(pRequest->szTargetDirectory) );
		V_FixSlashes( pRequest->szTargetDirectory );
	}

	if ( lpszTargetFilename != NULL )
	{
		V_strncpy( pRequest->szTargetFilename, lpszTargetFilename, ARRAYSIZE(pRequest->szTargetFilename) );
	}

	// Save where we're going to read from
	V_strncpy( pRequest->szSourceFilename, lpszSourceFilename, ARRAYSIZE(pRequest->szSourceFilename) );
	V_FixSlashes( pRequest->szSourceFilename );

	pRequest->unLastUpdateTime	= 0;
	pRequest->bForceUpdate		= false;
	pRequest->unTimestamp		= g_TimeStampIncr++;	// FIXME: This is to get around some timestamping, in essence larger numbers = newer additions
	pRequest->unPriority		= unPriority;

	// This insert will sort the request into the list properly
	m_PendingFileOperations.Insert( pRequest );

	// For debugging insertion into priority queue
	Log_Msg( LOG_WORKSHOP, "[CUGCFileRequestManager] Inserted Upload: %llu\tPriority:%u\tTimestamp:%u\n", pRequest->fileHandle, pRequest->unPriority, pRequest->unTimestamp );
	Debug_LogPendingOperations();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const UGCFileRequest_t *CUGCFileRequestManager::GetFileRequestByHandle( UGCHandle_t unFileHandle ) const
{
	int nIndex = m_FileRequests.Find( unFileHandle );
	if ( nIndex == m_FileRequests.InvalidIndex() )
		return NULL;

	return m_FileRequests[nIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUGCFileRequestManager::FileRequestExists( UGCHandle_t handle ) const
{
	int nIndex = m_FileRequests.Find( handle );
	return ( nIndex != m_FileRequests.InvalidIndex() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUGCFileRequestManager::GetFullPath( UGCHandle_t unFileHandle, char *pDest, size_t nSize ) const
{
	const UGCFileRequest_t *pRequest = GetFileRequestByHandle( unFileHandle );
	if ( pRequest == NULL )
	{
		// Clear the return so it's obvious it failed
		V_memset( pDest, 0, nSize );
		return;
	}

	pRequest->fileRequest.GetFullPath( pDest, nSize );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CUGCFileRequestManager::GetDirectory( UGCHandle_t unFileHandle ) const
{
	const UGCFileRequest_t *pRequest = GetFileRequestByHandle( unFileHandle );
	if ( pRequest == NULL )
		return NULL;

	return pRequest->fileRequest.GetDirectory();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CUGCFileRequestManager::GetFilename( UGCHandle_t unFileHandle ) const
{
	const UGCFileRequest_t *pRequest = GetFileRequestByHandle( unFileHandle );
	if ( pRequest == NULL )
		return NULL;

	return pRequest->fileRequest.GetFilename();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
UGCFileRequestStatus_t CUGCFileRequestManager::GetStatus( UGCHandle_t unFileHandle ) const
{
	const UGCFileRequest_t *pRequest = GetFileRequestByHandle( unFileHandle );
	if ( pRequest == NULL )
		return UGCFILEREQUEST_INVALID;

	return pRequest->fileRequest.GetStatus();
}

//-----------------------------------------------------------------------------
// Purpose: Get the file handle for a request by its target filename
//-----------------------------------------------------------------------------
UGCHandle_t	CUGCFileRequestManager::GetFileRequestHandleByFilename( const char *lpszFilename ) const
{
	// Get the request by name
	const UGCFileRequest_t *pRequest = GetFileRequestByFilename( lpszFilename );
	if ( pRequest != NULL )
	{
		return pRequest->fileHandle;
	}

	return k_UGCHandleInvalid;
}

//-----------------------------------------------------------------------------
// Purpose: Get the file handle for a request by its target filename
//-----------------------------------------------------------------------------
const UGCFileRequest_t *CUGCFileRequestManager::GetFileRequestByFilename( const char *lpszFilename ) const
{
	// FIXME: This is a slow crawl through a list doing stricmps :(
	char szFullPath[MAX_PATH];
	for ( unsigned int i=0; i < m_FileRequests.Count(); i++ )
	{
		const UGCFileRequest_t *pRequest = m_FileRequests[i];
		pRequest->fileRequest.GetFullPath( szFullPath, ARRAYSIZE(szFullPath) );
		if ( !V_stricmp( szFullPath, lpszFilename ) )
			return pRequest;
	}

	// Now move through all the pending operations to see if it's living in there
	// We need to do this because uploads don't live in the normal system until they're done uploading
	// FIXME: This is going to be doing duplicate work since items can straddle both the known requests and the pending ones
	for ( int i=0; i < m_PendingFileOperations.Count(); i++ )
	{
		const UGCFileRequest_t *pRequest = m_PendingFileOperations.Element(i);
		pRequest->fileRequest.GetFullPath( szFullPath, ARRAYSIZE(szFullPath) );
		if ( !V_stricmp( szFullPath, lpszFilename ) )
			return pRequest;
	}


	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
UGCFileRequestStatus_t CUGCFileRequestManager::GetStatus( const char *lpszFilename ) const
{
	const UGCFileRequest_t *pRequest = GetFileRequestByFilename( lpszFilename );
	if ( pRequest == NULL )
		return UGCFILEREQUEST_INVALID;

	return pRequest->fileRequest.GetStatus();
}

// 
//-----------------------------------------------------------------------------
// Purpose: Returns the progress of a file being downloaded from the Steam cloud. 
//	Return: Always 0 if nothing has begun, or 1 if past the point of downloading, otherwise, the percentage downloaded
//-----------------------------------------------------------------------------
float CUGCFileRequestManager::GetDownloadProgress( UGCHandle_t handle ) const
{
	const UGCFileRequest_t *pRequest = GetFileRequestByHandle( handle );
	if ( pRequest == NULL )
		return 0.0f;

	return pRequest->GetProgress();
}

//-----------------------------------------------------------------------------
// Purpose: Promote the specified handle to the top of the priority list
//-----------------------------------------------------------------------------
bool CUGCFileRequestManager::PromoteRequestToTop( UGCHandle_t handle )
{
	// The request must be in the system
	const UGCFileRequest_t *pRequest = GetFileRequestByHandle( handle );
	if ( pRequest == NULL )
		return false;

	// There must be pending operations to bother continuing
	if ( m_PendingFileOperations.Count() == 0 )
		return false;

	// If we're already at the top, don't bother
	const UGCFileRequest_t *pTopRequest = m_PendingFileOperations.ElementAtHead();
	if ( pTopRequest == pRequest )
		return true;

	// This is the top priority currently
	uint32 unTopPriority = pTopRequest->unPriority;

	// Now we need to find this request in the pending operations
	for ( int i = 0; i < m_PendingFileOperations.Count(); i++ )
	{
		const UGCFileRequest_t *pFoundRequest = m_PendingFileOperations.Element(i);
		if ( pRequest == pFoundRequest )
		{
			// Bump our priority up
			// We cast away the const reference because we're the controlling class for this type
			((UGCFileRequest_t *)pRequest)->unPriority = unTopPriority+1;
			m_PendingFileOperations.RemoveAt(i);
			m_PendingFileOperations.Insert( ((UGCFileRequest_t *)pRequest) );
			Log_Msg( LOG_WORKSHOP, "[CUGCFileRequestManager] Promoted %llu to top of queue\n", pRequest->fileHandle );
			Debug_LogPendingOperations();
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Dump our priority queue so we can debug it
//-----------------------------------------------------------------------------
void  CUGCFileRequestManager::Debug_LogPendingOperations( void )
{
#if 0
	// Must have something to operate on
	if ( m_PendingFileOperations.Count() == 0 )
		return;

	// For debugging insertion into priority queue
	Log_Msg( LOG_WORKSHOP, "\n==[Pending UGC Operations]==\n");
	
	// The queue cannot be walked through trivially, so we need to actually pop each member off the top, the reinsert at the end
	CUtlVector< UGCFileRequest_t * > vecOverflow;
	while ( m_PendingFileOperations.Count() )
	{
		UGCFileRequest_t *pQueuedRequest = m_PendingFileOperations.ElementAtHead();
		Log_Msg( LOG_WORKSHOP, "o File: %llu\tPriority:%u\tTimestamp:%u\n", pQueuedRequest->fileHandle, pQueuedRequest->unPriority, pQueuedRequest->unTimestamp );
		vecOverflow.AddToTail( pQueuedRequest );
		m_PendingFileOperations.RemoveAtHead();
	}

	// Put them all back
	for ( int i=0; i < vecOverflow.Count(); i++ )
	{
		m_PendingFileOperations.Insert( vecOverflow[i] );
	}

	Log_Msg( LOG_WORKSHOP, "==============================\n\n");
#endif //
}

bool CUGCFileRequestManager::HasPendingDownloads( void ) const
{
	return m_PendingFileOperations.Count() > 0;
}

#endif // ! NO_STEAM
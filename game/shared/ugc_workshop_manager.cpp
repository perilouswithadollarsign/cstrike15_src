//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//============================================================================//

#include "cbase.h"

#include "ugc_workshop_manager.h"

#if !defined( NO_STEAM )

CWorkshopManager::CWorkshopManager( IWorkshopManagerCallbackInterface *pCallbackInterface ) :
#if !defined( _GAMECONSOLE )
	m_callbackFileSubscribed( this, &CWorkshopManager::Steam_OnFileSubscribed ),
	m_callbackFileUnsubscribed( this, &CWorkshopManager::Steam_OnFileUnsubscribed ),
#endif
	m_WorkshopFileInfoManager( pCallbackInterface )
{
	m_pCallbackInterface = pCallbackInterface;
}

CWorkshopManager::~CWorkshopManager( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Requests voting information for a published file
// Return:	If true, the voting data already exists and can be immediately accessed. Otherwise, the caller will need to wait
//-----------------------------------------------------------------------------
bool CWorkshopManager::AddPublishedFileVoteInfoRequest( const PublishedFileInfo_t *pInfo, bool bForceUpdate /*=false*/ )
{
	return m_WorkshopFileInfoManager.AddFileVoteInfoRequest( pInfo, bForceUpdate );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a published file to the query 
// Return:	If true, the published file already exists and the data can be immediately queried for. Otherwise, the caller will need to wait
//-----------------------------------------------------------------------------
bool CWorkshopManager::AddFileInfoQuery( CBasePublishedFileRequest *pRequest, bool bAllowUpdate /*= false*/ )
{
	return m_WorkshopFileInfoManager.AddFileInfoQuery( pRequest, bAllowUpdate );
}

//-----------------------------------------------------------------------------
// Purpose: Determine if there's already a request for a file
//-----------------------------------------------------------------------------
bool CWorkshopManager::UGCFileRequestExists( UGCHandle_t handle )
{
	return m_UGCFileRequestManager.FileRequestExists( handle );
}

//-----------------------------------------------------------------------------
// Purpose: Get the filename for a UGC handle
//	Return: NULL if the file is not found
//-----------------------------------------------------------------------------
void CWorkshopManager::GetUGCFullPath( UGCHandle_t handle, char *pDest, size_t nSize )
{
	m_UGCFileRequestManager.GetFullPath( handle, pDest, nSize );
}

//-----------------------------------------------------------------------------
// Purpose: Get the directory for a UGC handle
//-----------------------------------------------------------------------------
const char *CWorkshopManager::GetUGCFileDirectory( UGCHandle_t handle )
{
	return m_UGCFileRequestManager.GetDirectory( handle );
}

//-----------------------------------------------------------------------------
// Purpose: Get the filename for a UGC handle
//-----------------------------------------------------------------------------
const char *CWorkshopManager::GetUGCFilename( UGCHandle_t handle )
{
	return m_UGCFileRequestManager.GetFilename( handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
UGCFileRequestStatus_t CWorkshopManager::GetUGCFileRequestStatus( UGCHandle_t handle )
{
	return m_UGCFileRequestManager.GetStatus( handle );
}

//-----------------------------------------------------------------------------
// Purpose: Update all our Workshop work
//-----------------------------------------------------------------------------
void CWorkshopManager::Update( void )
{
	// Update our file upload/download requests from Steam
	UpdateUGCRequests();

	// Update our Workshop file info queries
	m_WorkshopFileInfoManager.Update();
}

//-----------------------------------------------------------------------------
// Purpose: Update our UGC file requests
//-----------------------------------------------------------------------------
void CWorkshopManager::UpdateUGCRequests( void )
{
	// If we're not allowing UGC updates, just wait
	if ( m_bUGCRequestsPaused )
		return;

	// Push our requests ahead
	m_UGCFileRequestManager.Update();
}

//-----------------------------------------------------------------------------
// Purpose: Delete a file request from the system (optionally removing its downloaded content form the disk)
//-----------------------------------------------------------------------------
bool CWorkshopManager::DeleteUGCFileRequest( UGCHandle_t handle, bool bRemoveFromDisk /*= false*/ )
{
	return m_UGCFileRequestManager.DeleteFileRequest( handle, bRemoveFromDisk );
}

//-----------------------------------------------------------------------------
// Purpose: Get published file information by its ID number
//-----------------------------------------------------------------------------
const PublishedFileInfo_t *CWorkshopManager::GetPublishedFileInfoByID( PublishedFileId_t nID ) const
{
	return m_WorkshopFileInfoManager.GetPublishedFileInfoByID( nID );
}


#if !defined( _GAMECONSOLE )

//-----------------------------------------------------------------------------
// Purpose: A file has been added to the user's queue
//-----------------------------------------------------------------------------
void CWorkshopManager::Steam_OnFileSubscribed( RemoteStoragePublishedFileSubscribed_t *pCallback )
{
	// Emit a callback denoting our change
	m_pCallbackInterface->OnPublishedFileSubscribed( pCallback->m_nPublishedFileId );
}

//-----------------------------------------------------------------------------
// Purpose: A file has been removed from the user's queue
//-----------------------------------------------------------------------------
void CWorkshopManager::Steam_OnFileUnsubscribed( RemoteStoragePublishedFileUnsubscribed_t *pCallback )
{
	// Emit a callback denoting our change
	m_pCallbackInterface->OnPublishedFileUnsubscribed( pCallback->m_nPublishedFileId );
}

#endif // !_GAMECONSOLE

//-----------------------------------------------------------------------------
// Purpose: Get the current download status of a file that's downloading
//-----------------------------------------------------------------------------
float CWorkshopManager::GetUGCFileDownloadProgress( UGCHandle_t handle ) const
{
	return m_UGCFileRequestManager.GetDownloadProgress( handle );
}

//-----------------------------------------------------------------------------
// Purpose: Given a published file, build a thumbnail file request for it
//-----------------------------------------------------------------------------
bool CWorkshopManager::CreateFileDownloadRequest(	UGCHandle_t hFileHandle, 
													PublishedFileId_t fileID,
													const char *lpszDirectory, 
													const char *lpszFilename, 
													uint32 unPriority /*= UGC_PRIORITY_GENERIC*/, 
													uint32 unTimeLastUpdated /*=0*/, 
													bool bForceUpdate /*=false*/ ) 
{
	// Create a request for a generic asset
	return m_UGCFileRequestManager.CreateFileDownloadRequest(
										hFileHandle,
										fileID,
										lpszDirectory,
										lpszFilename,
										unPriority,
										unTimeLastUpdated,
										bForceUpdate );
}

//-----------------------------------------------------------------------------
// Purpose: Given a published file, build a thumbnail file request for it
//-----------------------------------------------------------------------------
bool CWorkshopManager::CreateFileUploadRequest(	const char *lpszSourceFilename,
												const char *lpszDirectory, 
												const char *lpszFilename, 
												uint32 unPriority /*= UGC_PRIORITY_GENERIC*/ ) 
{
	// Create a request for a generic asset
	return m_UGCFileRequestManager.CreateFileUploadRequest(
		lpszSourceFilename,
		lpszDirectory,
		lpszFilename,
		unPriority );
}

//-----------------------------------------------------------------------------
// Purpose: Promote this request to the top of the list
//-----------------------------------------------------------------------------
bool CWorkshopManager::PromoteUGCFileRequestToTop( UGCHandle_t handle )
{
	return m_UGCFileRequestManager.PromoteRequestToTop( handle );
}

//-----------------------------------------------------------------------------
// Purpose: Get the UGC handle for a filename (if any)
//-----------------------------------------------------------------------------
UGCHandle_t CWorkshopManager::GetUGCFileHandleByFilename( const char *lpszFilename )
{
	return m_UGCFileRequestManager.GetFileRequestHandleByFilename( lpszFilename );
}

//-----------------------------------------------------------------------------
// Purpose: Return the status of a request by its name
//	  Note: This is a very slow call and should only be used in instances where the UGCHandle_t
//			cannot be known at the time of the call (i.e. uploading to cloud)
//-----------------------------------------------------------------------------
UGCFileRequestStatus_t CWorkshopManager::GetUGCFileRequestStatusByFilename( const char *lpszFilename )
{
	return m_UGCFileRequestManager.GetStatus( lpszFilename );	
}

//-----------------------------------------------------------------------------
// Purpose: Callback for file deletion
//-----------------------------------------------------------------------------
void CWorkshopManager::Steam_OnDeletePublishedFile( RemoteStorageDeletePublishedFileResult_t *pResult, bool bError )
{
	// Forget it from our info depot
	m_WorkshopFileInfoManager.RemovePublishedFileInfo( pResult->m_nPublishedFileId );
	
	// Emit a callback denoting our change
	m_pCallbackInterface->OnPublishedFileDeleted( pResult->m_nPublishedFileId );
}

//-----------------------------------------------------------------------------
// Purpose: Delete a published file from our 
//-----------------------------------------------------------------------------
bool CWorkshopManager::DeletePublishedFile( PublishedFileId_t nID )
{
	if ( steamapicontext == NULL || steamapicontext->SteamRemoteStorage() == NULL )
	{
		Log_Warning( LOG_WORKSHOP, "[WorkshopManager] Unable to delete the file (%llu) from Steam! (Not Connected)", nID );
		return false;
	}

	// Make the call and we'll wait
	SteamAPICall_t hSteamAPICall = steamapicontext->SteamRemoteStorage()->DeletePublishedFile( nID );
	if ( hSteamAPICall == k_uAPICallInvalid )
	{
		Log_Warning( LOG_WORKSHOP, "[WorkshopManager] Unable to delete the file (%llu) from Steam!", nID );
		return false;
	}
	
	// Setup our callbacks
	m_callbackDeletePublishedFile.Set( hSteamAPICall, this, &CWorkshopManager::Steam_OnDeletePublishedFile );	

	return true;
}

#if !defined( _GAMECONSOLE )

//-----------------------------------------------------------------------------
// Purpose: Handle the callback when Steam has finished updating our user vote
//-----------------------------------------------------------------------------
void CWorkshopManager::Steam_OnUpdateUserPublishedItemVote( RemoteStorageUpdateUserPublishedItemVoteResult_t *pResult, bool bError )
{
	if ( bError || pResult->m_eResult != k_EResultOK )
	{
		Assert(0);
		Log_Warning( LOG_WORKSHOP, "[WorkshopManager] Failed to update user vote for %llu (Error: %d)\n", pResult->m_nPublishedFileId, pResult->m_eResult );
		return;
	}

	// Update our voting information with the new data
	const PublishedFileInfo_t *pInfo = GetPublishedFileInfoByID( pResult->m_nPublishedFileId );
	if ( pInfo != NULL )
	{
		AddPublishedFileVoteInfoRequest( pInfo, true );
	}
}

#endif // USE_BETA_STEAM_APIS && !_GAMECONSOLE

//-----------------------------------------------------------------------------
// Purpose: The base class handles book keeping we'd like all of our requests to do
//-----------------------------------------------------------------------------
void CWorkshopManager::UpdatePublishedItemVote( PublishedFileId_t nFileID, bool bVoteUp )
{
#if !defined( _GAMECONSOLE )
	// TODO: Figure out how to track this call through the update
	SteamAPICall_t hSteamAPICall = steamapicontext->SteamRemoteStorage()->UpdateUserPublishedItemVote( nFileID, bVoteUp );
	if ( hSteamAPICall == k_uAPICallInvalid )
	{
		Assert(0);
		Log_Warning( LOG_WORKSHOP, "[WorkshopManager] UpdatePublishedItemVote() call failed for %llu!\n", nFileID );
		return;
	}

	// Setup our callback
	m_callbackUpdateUserPublishedItemVote.Set( hSteamAPICall, this, &CWorkshopManager::Steam_OnUpdateUserPublishedItemVote );
#endif // !_GAMECONSOLE
}

bool CWorkshopManager::IsFileInfoRequestStillPending( PublishedFileId_t nID ) const
{
	return m_WorkshopFileInfoManager.IsInfoRequestStillPending( nID );
}

bool CWorkshopManager::HasPendingDownloads( void ) const
{
	return m_UGCFileRequestManager.HasPendingDownloads();
}


#endif // !NO_STEAM
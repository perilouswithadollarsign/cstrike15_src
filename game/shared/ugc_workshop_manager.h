//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//============================================================================//

#ifndef _WORKSHOP_MANAGER_H__
#define _WORKSHOP_MANAGER_H__

#include "ugc_request_manager.h"
#include "ugc_file_info_manager.h"

#if !defined( NO_STEAM )

class IWorkshopManagerCallbackInterface : public IWorkshopFileInfoManagerCallbackInterface
{
public:
	// Published files
	virtual void OnPublishedFileSubscribed( PublishedFileId_t nID ) = 0;
	virtual void OnPublishedFileUnsubscribed( PublishedFileId_t nID ) = 0;
	virtual void OnPublishedFileDeleted( PublishedFileId_t nID ) = 0;
};

class CBaseWorkshopManagerCallbackInterface : public IWorkshopManagerCallbackInterface
{
public:

	// File requests
	virtual void OnFileRequestFinished( UGCHandle_t hFileHandle ) {}
	virtual void OnFileRequestError( UGCHandle_t hFileHandle ) {}

	// Published files
	virtual void OnPublishedFileSubscribed( PublishedFileId_t nID ) {}
	virtual void OnPublishedFileUnsubscribed( PublishedFileId_t nID ) {}
	virtual void OnPublishedFileDeleted( PublishedFileId_t nID ) {}
};

class CWorkshopManager 
{

public:

	CWorkshopManager( IWorkshopManagerCallbackInterface *pCallbackInterface );
	~CWorkshopManager( void );

	void Update( void );

#if !defined( NO_STEAM )

	//
	// PFI Depot
	//

	const PublishedFileInfo_t	*GetPublishedFileInfoByID( PublishedFileId_t nID ) const;
	bool						IsFileInfoRequestStillPending( PublishedFileId_t nID ) const;
	bool						AddFileInfoQuery( CBasePublishedFileRequest *pRequest, bool bAllowUpdate = false );
	bool						AddPublishedFileVoteInfoRequest( const PublishedFileInfo_t *pInfo, bool bForceUpdate = false );
	void						UpdatePublishedItemVote( PublishedFileId_t nFileID, bool bVoteUp );

	bool						HasPendingDownloads( void ) const;

	// 
	// UGC methods
	// 

	bool						DeletePublishedFile( PublishedFileId_t nID );
	bool						DeleteUGCFileRequest( UGCHandle_t handle, bool bRemoveFromDisk = false );
	bool						UGCFileRequestExists( UGCHandle_t handle );
	void						GetUGCFullPath( UGCHandle_t handle, char *pDest, size_t nSize );
	const char					*GetUGCFileDirectory( UGCHandle_t handle );
	const char					*GetUGCFilename( UGCHandle_t handle );
	UGCFileRequestStatus_t		GetUGCFileRequestStatus( UGCHandle_t handle );
	bool						PromoteUGCFileRequestToTop( UGCHandle_t handle );

	// These are special cases where th UGCHandle_t is yet unknown (as in the case of uploads)
	UGCHandle_t					GetUGCFileHandleByFilename( const char *lpszFilename );
	UGCFileRequestStatus_t		GetUGCFileRequestStatusByFilename( const char *lpszFilename );
	float						GetUGCFileDownloadProgress( UGCHandle_t handle ) const;

	bool						CreateFileDownloadRequest(	UGCHandle_t hFileHandle, 
															PublishedFileId_t fileID,
															const char *lpszDirectory, 
															const char *lpszFilename, 
															uint32 unPriority = 0, 
															uint32 unTimeLastUpdated = 0, 
															bool bForceUpdate = false );

	bool						CreateFileUploadRequest(	const char *lpszSourceFilename,
															const char *lpszDirectory, 
															const char *lpszFilename, 
															uint32 unPriority = 0 );

#endif // !NO_STEAM

private:

	// Callback interface
	IWorkshopManagerCallbackInterface	*m_pCallbackInterface;

	//
	// Community map files
	//

	void UpdateUGCRequests( void );

	// Callback for deleting files
	CCallResult<CWorkshopManager, RemoteStorageDeletePublishedFileResult_t> m_callbackDeletePublishedFile;
	void Steam_OnDeletePublishedFile( RemoteStorageDeletePublishedFileResult_t *pResult, bool bError );

#if !defined( _GAMECONSOLE )
	void Steam_OnUpdateUserPublishedItemVote( RemoteStorageUpdateUserPublishedItemVoteResult_t *pResult, bool bError );
	void Steam_OnFileSubscribed( RemoteStoragePublishedFileSubscribed_t *pCallback );
	void Steam_OnFileUnsubscribed( RemoteStoragePublishedFileUnsubscribed_t *pCallback );
	void QueryForCommunityMaps( void );
	CCallResult<CWorkshopManager, RemoteStorageUpdateUserPublishedItemVoteResult_t> m_callbackUpdateUserPublishedItemVote;
	CCallback< CWorkshopManager, RemoteStoragePublishedFileSubscribed_t, false> m_callbackFileSubscribed;
	CCallback< CWorkshopManager, RemoteStoragePublishedFileUnsubscribed_t, false> m_callbackFileUnsubscribed;
#endif // !_GAMECONSOLE

	// 
	// File Information Manager
	//

	CWorkshopFileInfoManager		m_WorkshopFileInfoManager;

	// 
	// UGC Request Manager
	//

	CUGCFileRequestManager			m_UGCFileRequestManager;
	bool							m_bUGCRequestsPaused;

	int					m_nTotalSubscriptionsLoaded;				// Number of subscriptions we've received from the Steam server. This may not be the total number available, meaning we need to requery.
	bool				m_bReceivedQueueBaseline;					// Whether or not we've heard back successfully from Steam on our queue status
	float				m_flQueueBaselineRequestTime;				// Time that the baseline was requested from the GC

};

#endif // !NO_STEAM

#endif // _WORKSHOP_MANAGER_H__

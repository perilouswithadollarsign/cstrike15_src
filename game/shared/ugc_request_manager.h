//===== Copyright Valve Corporation, All rights reserved. ======//
//
// 
//
//==============================================================//

#ifndef UGC_REQUEST_MANAGER
#define UGC_REQUEST_MANAGER

#ifdef _WIN32
#pragma once
#endif

#include "ugc_utils.h"
#include "utlpriorityqueue.h"
#include "utlmap.h"

#if !defined( NO_STEAM ) && !defined ( _PS3 )

class CUGCFileRequestManager;

//
//
//

enum UGCRequestType_t {
	UGC_REQUEST_INVALID = -1,	// Uninitialized request!
	UGC_REQUEST_DOWNLOAD = 0,
	UGC_REQUEST_UPLOAD,
};

struct UGCFileRequest_t
{
public:

	uint32	GetPriority() const { return unPriority; }
	uint32	GetTimestamp() const { return unTimestamp; }
	float	GetProgress() const { return fileRequest.GetDownloadProgress(); }
	UGCHandle_t GetFileHandle() const { return fileHandle; }
	PublishedFileId_t GetPublishedFileID() const { return publishedFileID; }

private:

	UGCFileRequest_t() :
		fileHandle( k_UGCHandleInvalid ),
		publishedFileID( 0 ),
		unLastUpdateTime( 0 ),
		bForceUpdate( false ),
		unPriority( 0 ),
		unTimestamp( 0 ),
		nType( UGC_REQUEST_INVALID )
	{
	   szTargetFilename[0] = '\0';
	   szTargetDirectory[0] = '\0';
	   szSourceFilename[0] = '\0';
	}

	// Core data
	UGCHandle_t			fileHandle;
	PublishedFileId_t	publishedFileID;							// id for the whole content package this download is associated with 

	char				szSourceFilename[MAX_PATH];		// Only used for uploads
	char				szTargetFilename[MAX_PATH];
	char				szTargetDirectory[MAX_PATH];

	UGCRequestType_t	nType;							// Download or upload type

	uint32				unLastUpdateTime;				// Set to 0 if only existence of file matters
	bool				bForceUpdate;					// If true, always download a new version

	// Priority data
	uint32				unPriority;		// Higher is better
	uint32				unTimestamp;	// When this request was initiated (for sorting)

	CUGCFileRequest		fileRequest;

	friend class CUGCFileRequestManager;
};


//
//
//

class CUGCFileRequestManager
{
public:

	CUGCFileRequestManager( void );

	// Move the processing of file requests forward (yielding where appropriate)
	void Update( void );


	// Create a file request
	bool CreateFileDownloadRequest(	UGCHandle_t unFileHandle, 
									PublishedFileId_t publishedFileID, 
									const char *lpszTargetDirectory, 
									const char *lpszTargetFilename,
									uint32 unPriority,
									uint32 unLastUpdateTime = 0,	// Last time the file was updated (used for sync checks)
									bool bForceUpdate = false );	// Override sync checks and always grab the file

	bool CreateFileUploadRequest(	const char *lpszSourceFilename,	
									const char *lpszTargetDirectory, 
									const char *lpszTargetFilename,
									uint32 unPriority );

	// Delete a file request from the system
	bool DeleteFileRequest( UGCHandle_t handle, bool bRemoveFromDisk = false );

	// Determine if there is a request for the handle provided
	bool FileRequestExists( UGCHandle_t handle ) const;

	// Get the full (local) path (including filename) for this file
	void GetFullPath( UGCHandle_t unFileHandle, char *pDest, size_t nSize ) const;
	
	// Get the (local path) directory this file resides in (can be "")
	const char *GetDirectory( UGCHandle_t unFileHandle ) const;
	
	// The filename, minus the directory
	const char *GetFilename( UGCHandle_t unFileHandle ) const;
	
	// Get the file handle for a request by its target filename
	UGCHandle_t	GetFileRequestHandleByFilename( const char *lpszFilane ) const;
	
	// Get the status of the request. Note: Will return INVALID if there is no request by that handle found
	UGCFileRequestStatus_t GetStatus( UGCHandle_t unFileHandle ) const;
	UGCFileRequestStatus_t GetStatus( const char *lpszFilename ) const;

	// Returns the progress of a file being downloaded from the Steam cloud. Always 0 if nothing has begun, or 1 if past the point of downloading
	float GetDownloadProgress( UGCHandle_t handle ) const;

	// Promote the request to the top of the priority list (only works for pending requests)
	bool PromoteRequestToTop( UGCHandle_t handle );

	// Any pending operations
	bool HasPendingDownloads( void ) const;
	
private:
	
	void Debug_LogPendingOperations( void );

	// Get the underlying data for the request, referenced by its file handle
	const UGCFileRequest_t *GetFileRequestByHandle( UGCHandle_t unFileHandle ) const;
	const UGCFileRequest_t *GetFileRequestByFilename( const char *lpszFilename ) const;

	// Map of all requested files by their Steam Cloud handles
	CUtlMap<UGCHandle_t, const UGCFileRequest_t *>	m_FileRequests;

	// Prioritized queue of pending file operations, serviced by order of importance
	CUtlPriorityQueue<UGCFileRequest_t *>			m_PendingFileOperations;
};

#endif // !NO_STEAM

#endif // UGC_REQUEST_MANAGER
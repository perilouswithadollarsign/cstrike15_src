//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Utility helper functions for dealing with UGC files
//
//==========================================================================//

#ifndef UGC_UTILS_H
#define UGC_UTILS_H

#ifdef _WIN32
#pragma once
#endif

#include "steam/isteamremotestorage.h"

// Version of V_ComposeFilename which can account for NULL path or filename
void V_SafeComposeFilename( const char *pPathIn, const char *pFilenameIn, char *pDest, size_t nDestSize );

// Unzip a file. If output dir is omitted, unzips to zip file's directory.
// Use paths relative to GetCurrentDirectory, or absolute paths.
bool UnzipFile( const char* szPathToZipFile, const char* szOutputDir = NULL );

bool UGCUtil_IsOfficialMap( PublishedFileId_t id );

#if !defined( NO_STEAM ) && !defined ( _PS3 )

#include "steam/steam_api.h"
#include "filesystem.h"

DECLARE_LOGGING_CHANNEL( LOG_WORKSHOP );

// This will log the UGC file requests as they're serviced
#define LOG_FILEREQUEST_PROGRESS

// Simulates stalling of the file IO for testing
//#define FILEREQUEST_IO_STALL

class CUGCFileRequestManager;

extern ISteamRemoteStorage *GetISteamRemoteStorage();

PublishedFileId_t GetMapIDFromMapPath( const char *pMapPath ); // get the map workshop ID by passing the full map path

bool UGCUtil_TimestampFile( const char *pFileRelativePath, uint32 uTimestamp );

void UGCUtil_Init();
void UGCUtil_Shutdown();

enum UGCFileRequestStatus_t
{
	UGCFILEREQUEST_INVALID = -2,		// This was an invalid request for some reason
	UGCFILEREQUEST_ERROR = -1,			// An error occurred while processing the file operation
	UGCFILEREQUEST_READY,				// File request is ready to do work
	UGCFILEREQUEST_DOWNLOADING,			// Currently downloading a file
	UGCFILEREQUEST_UNZIPPING,			// Threaded unzipping
	UGCFILEREQUEST_DOWNLOAD_WRITING,	// Async write of the downloaded file to the disc
	UGCFILEREQUEST_UPLOADING,			// Currently uploading a file
	UGCFILEREQUEST_FINISHED				// Operation complete, no work waiting
};

#ifdef FILEREQUEST_IO_STALL
enum 
{
	FILEREQUEST_STALL_NONE,
	FILEREQUEST_STALL_DOWNLOAD,			// Download from UGC server
	FILEREQUEST_STALL_WRITE,			// Write to disc
};
#endif // FILEREQUEST_IO_STALL

class CUGCUnzipJob;  

class CUGCFileRequest
{
public:
	CUGCFileRequest( void );
	~CUGCFileRequest( void );
	
	UGCFileRequestStatus_t StartDownload( UGCHandle_t hFileHandle, const char *lpszTargetDirectory = NULL, const char *lpszTargetFilename = NULL, uint32 timeUpdated = 0, bool bForceUpdate = false );
	UGCFileRequestStatus_t StartUpload( CUtlBuffer &buffer, const char *lpszFilename );
	UGCFileRequestStatus_t Update( void );
	UGCFileRequestStatus_t GetStatus( void ) const { return m_UGCStatus; }

	// Accessors
	const char *GetLastError( void ) const { return m_szErrorText; }
	UGCHandle_t	GetCloudHandle( void ) const { return m_hCloudID; }

	void GetFullPath( char *pDest, size_t nSize ) const;	// Filename on disk (including relative path)
	const char *GetFilename( void ) const; 					// Name on disk if not the same as in the cloud
	const char *GetDirectory( void ) const;					// File directory on disk

	float GetDownloadProgress( void ) const { return m_flDownloadProgress; }

	void GetLocalFileName( char *pDest, size_t strSize );
	void GetLocalDirectory( char *pDest, size_t strSize );

private:

	void MarkCompleteAndFree( bool bUpdated = true );

#ifdef CLIENT_DLL
	void CreateSmallThumbNail( bool bForce );
	bool ResizeRGBAImage( const unsigned char *pData, unsigned int nDataSize, unsigned int nWidth, unsigned int nHeight, 
						  unsigned char **pDataOut, unsigned int nNewWidth, unsigned int nNewHeight );
#endif

	bool FileInSync( const char *lpszTargetDirectory, const char *lpszTargetFilename, uint32 timeUpdated );

	void UpdateUnzip();

	UGCFileRequestStatus_t ThrowError( const char *lpszFormat, ... );

	void Steam_OnUGCDownload( RemoteStorageDownloadUGCResult_t *pResult, bool bError );
	CCallResult<CUGCFileRequest, RemoteStorageDownloadUGCResult_t>	m_callbackUGCDownload;
	
	void Steam_OnFileShare( RemoteStorageFileShareResult_t *pResult, bool bError );
	CCallResult<CUGCFileRequest, RemoteStorageFileShareResult_t>	m_callbackFileShare;

	char					m_szTargetDirectory[MAX_PATH];	// If specified, the directory the file will be placed in
	char					m_szTargetFilename[MAX_PATH];	// If specified, this name overrides the UGC filename
	char					m_szFileName[MAX_PATH];			// Filename of in the cloud structure

	SteamAPICall_t			m_hSteamAPICall;				// Used to track Steam API calls which are non-blocking
	CUtlBuffer				m_bufContents;					// Contents of the file once read from the cloud
	UGCHandle_t				m_hCloudID;						// Cloud handle of this request
	FSAsyncControl_t		m_AsyncControl;					// Handle for the async requests this class can initiate

	char					m_szErrorText[512];				// Holds information if an error occurred

	UGCFileRequestStatus_t	m_UGCStatus;					// The current status of this request
	uint32					m_tFileUpdateTime;				// The update timestamp of the workshop file, 0 if not available

	// Timing information for logging purposes
	float					m_flIOStartTime;				// Time when the current request was initiated
	float					m_flDownloadProgress;			// Progress (only valid for downloads)

	friend class CUGCFileRequestManager;

	CUGCUnzipJob*			m_pUnzipJob;					// Pointer to threaded unzip job. 

#ifdef FILEREQUEST_IO_STALL
	// Debug data
	float					m_flIOStallDuration;			// Amount of time (in seconds) to stall all IO operations
	int						m_nIOStallType;					// Type of stall (0 - none, 1 - download, 2 - write )
	float					m_flIOStallStart;
#endif // FILEREQUEST_IO_STALL
};

#endif //!NO_STEAM

#endif //UGC_UTILS_H

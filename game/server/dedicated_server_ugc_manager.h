//========== Copyright © Valve Corporation, All rights reserved. ========
//
// Dedicated server's object for managing UGC file subscriptions.
//
//=============================================================================

#ifndef DEDICATED_SERVER_UGC_MANAGER
#define DEDICATED_SERVER_UGC_MANAGER


#if defined( COMPILER_MSVC )
#pragma once
#endif

#define AUTH_KEY_MAX_LEN 64

#include "igamesystem.h"				// autogamesystem base class
#include "steam/isteamremotestorage.h"	// ugc types/constants

// Class for holding info about a UGC file the server is 'subscribed' to. 
struct DedicatedServerUGCFileInfo_t
{
	// Pass in a 'publishfiledetails' kv blob returned by the 'GetPublishedFileDetails' web api.
	bool BuildFromKV( KeyValues *pPublishedFileDetails );

	char m_szFileName[k_cchFilenameMax];				// name_on_disk.extention
	uint32 m_unFileSizeInBytes;							// size in bytes 
	char m_szTitle[k_cchPublishedDocumentTitleMax];		// Display name
	char m_szUrl[k_cchPublishedFileURLMax];				// File url
	uint32 m_unTimeLastUpdated;							// Last update to this content
	PublishedFileId_t fileId;							// UGC id for this content package (file + preview + metadata). Always the same after updates.
	UGCHandle_t contentHandle;							// UGC content handle to the file we're downloading. This can change if the author updates the file
	char m_szFilePath[MAX_PATH];						// Path to store UGC file relative to game directory
	bool m_bIsValid;									// Did we get all the fields we need from the publish file details KV?
	double m_dblPlatFloatTimeReceived;					// Plat_FloatTime of when we received this information
	EResult m_result;									// Result for this entry we got back from steam (is 0 if no problems)
};

// Requests a file from steam over http one unChunkSize at a time, async writes to disk.
class CStreamingUGCDownloader
{
public:
	virtual ~CStreamingUGCDownloader();
	CStreamingUGCDownloader();
	void StartFileDownload( const DedicatedServerUGCFileInfo_t *pFileInfo, uint32 unChunkSize );
	bool IsFinished( void ) { return m_bIsFinished; }
	void Update ( void );
	PublishedFileId_t GetPublishedFileId( void ) { return m_pFileInfo ? m_pFileInfo->fileId : 0; }
	const DedicatedServerUGCFileInfo_t* GetFileInfo( void ) { return m_pFileInfo; }
			
private:
	CCallResult< CStreamingUGCDownloader, HTTPRequestCompleted_t > m_httpRequestCallback;
	void OnHTTPRequestComplete( HTTPRequestCompleted_t *arg, bool bFailed );
	void HTTPRequestPartialContent( uint32 rangeStart, uint32 rangeEnd );
	void Cleanup( void );

	uint32 m_unChunkSize;
	uint32 m_unBytesReceived;
	uint32 m_unFileSizeInBytes;
	char m_szTempFileName[k_cchFilenameMax];			// stream to a temp file as we download, copy to final dest once we get it all
	CUtlBuffer	m_fileBuffer;
	FSAsyncControl_t m_ioAsyncControl;
	const DedicatedServerUGCFileInfo_t *m_pFileInfo;
	bool m_bIsFinished;
	bool m_bHTTPRequestPending;
	HTTPRequestHandle m_hReq;
	float m_flTimeLastMessage;
	char m_szMapTitle[k_cchPublishedDocumentTitleMax];

	// no copy, no assign.
	CStreamingUGCDownloader( const CStreamingUGCDownloader& src );
	CStreamingUGCDownloader& operator=( const CStreamingUGCDownloader& src );
};

// Wrapper for steam api file info requests. 
class CBaseWorkshopHTTPRequest
{
public:
	CBaseWorkshopHTTPRequest( const CUtlVector<PublishedFileId_t> &vecFileIDs );
	virtual ~CBaseWorkshopHTTPRequest();

	void OnHTTPRequestComplete( HTTPRequestCompleted_t *arg, bool bFailed );
	bool IsFinished( void ) const { return m_bFinished; }

	EHTTPStatusCode GetLastHTTPResult( void ) const { return m_lastHTTPResult; }

	const CUtlVector<PublishedFileId_t> & GetItemsQueried( void ) const { return m_vecItemsQueried; }

protected:
	virtual void ProcessHTTPResponse( KeyValues *pResponseKV ) { Assert( 0 ); };

	CUtlVector< PublishedFileId_t >			m_vecItemsQueried;
	HTTPRequestHandle						m_handle;

	EHTTPStatusCode							m_lastHTTPResult;
	bool									m_bFinished;

	CCallResult< CBaseWorkshopHTTPRequest, HTTPRequestCompleted_t > m_httpCallback;

	CBaseWorkshopHTTPRequest( const CBaseWorkshopHTTPRequest& src );
	CBaseWorkshopHTTPRequest& operator=( const CBaseWorkshopHTTPRequest& src );
};

// Turns a list of file ids into filled out info structs
class CPublishedFileInfoHTTPRequest : public CBaseWorkshopHTTPRequest
{
public:
	CPublishedFileInfoHTTPRequest( const CUtlVector<PublishedFileId_t>& vecFileIDs );
	virtual ~CPublishedFileInfoHTTPRequest();
	
	const CUtlVector<DedicatedServerUGCFileInfo_t*>& GetFileInfoList( void ) const { return m_vecFileInfos; }

	HTTPRequestHandle CreateHTTPRequest( const char* szAuthKey = NULL );
	virtual void ProcessHTTPResponse( KeyValues *pResponseKV ) OVERRIDE;

protected:

	CUtlVector<DedicatedServerUGCFileInfo_t*> m_vecFileInfos;
};

// wrapper for collection info http requests
class CCollectionInfoHTTPRequest : public CBaseWorkshopHTTPRequest
{
public:
	CCollectionInfoHTTPRequest( const CUtlVector<PublishedFileId_t>& vecFileIDs );
	virtual ~CCollectionInfoHTTPRequest();

	HTTPRequestHandle CreateHTTPRequest( const char* szAuthKey = NULL );
	virtual void ProcessHTTPResponse( KeyValues *pResponseKV ) OVERRIDE;

	KeyValues* GetResponseKV( void ) { return m_pResponseKV; } 
protected:

	KeyValues* m_pResponseKV;
};

// Keeps track of a map or collection of maps to turn into a map group in the GameTypes system once they've all been updated.
class CWorkshopMapGroupBuilder
{
public:
	CWorkshopMapGroupBuilder( PublishedFileId_t id, const CUtlVector< PublishedFileId_t >& m_mapFileIDs );

	void MapOnDisk( PublishedFileId_t id, const char* szPath );
	void OnMapDownloaded( const DedicatedServerUGCFileInfo_t* pInfo );
	bool IsFinished ( void ) const { return m_pendingMapInfos.Count() == 0; }
	PublishedFileId_t GetId() const { return m_id; }
	const char* GetFirstMap( void ) const;
	const char* GetMapMatchingId( PublishedFileId_t id ) const;
	void CreateOrUpdateMapGroup( void );
	void RemoveRequiredMap( PublishedFileId_t id );

private:
	CUtlVector< PublishedFileId_t > m_pendingMapInfos; // Maps we still need the latest version of. Removes entries when map is at latest version on disk.
	CUtlStringList m_Maps;
	PublishedFileId_t m_id; // collection id, or map id if a single map.
	
	CWorkshopMapGroupBuilder( const CWorkshopMapGroupBuilder& src );
	CWorkshopMapGroupBuilder& operator=( const CWorkshopMapGroupBuilder& src );
};

class CDedicatedServerWorkshopManager : public CAutoGameSystem
{
public:
	// Autogamesystem overrides.
	virtual bool Init( void ) OVERRIDE;
	virtual void LevelInitPreEntity( void ) OVERRIDE;
	virtual const char* Name( void ) OVERRIDE { return "CDedicatedServerMapWorkshop"; }
	virtual void Shutdown( void ) OVERRIDE;

	// Gathers all the file IDs this server needs to stay up to date with and sends file info queries to steam.
	// EXPENSIVE: does file io.
	void GetNewestSubscribedFiles( void );
	
	// To support code addressing maps by mapname and assuming the /maps/ directory,
	// this method will return a list of DedicatedServerUGCFileInfos whose mapname matches the given string.
	// returns true if any maps are filled out. Only returns map infos whose map is already on disk (no pending downloads). 
	bool GetMapsMatchingName( const char* szMapName, CUtlVector<const DedicatedServerUGCFileInfo_t*>& outVec ) const;

	// Get the file ID of a UGC map. Returns 0 if non-ugc map or if not in subscription list. 
	// Parameter is path to the map relative to the game directory.
	PublishedFileId_t GetUGCMapPublishedFileID( const char* szPathToUGCMap ) const;

	// Returns the path to the map file for a given file id, or NULL if map is not on disk.
	const char* GetUGCMapPath( PublishedFileId_t id ) const;

	// This ticks from ServerGameDll::Think... ideally this would be an event
	// driven system but we'd need callbacks from the async append for that to work.
	void Update( void );

	// List of file IDs for subscribed maps on disk.
	const CUtlVector< PublishedFileId_t >& GetWorkshopMapList( void ) const;

	// Get the latest version of a map or collection and switch to it once the latest version is downloaded.
	void HostWorkshopMap( PublishedFileId_t id );
	void HostWorkshopMapCollection( PublishedFileId_t id );

	bool HasPendingMapDownloads( void ) const;

	void CheckForNewVersion( PublishedFileId_t id );

	void CheckIfCurrentLevelNeedsUpdate( void );
	bool CurrentLevelNeedsUpdate( void ) const;

	void SetTargetStartMap( PublishedFileId_t id ) { m_unTargetStartMap = id; }

	// Get the maps for which we downloaded UGC information successfully
	void GetWorkshopMasWithValidUgcInformation( CUtlVector<const DedicatedServerUGCFileInfo_t *>& outVec ) const;

protected:
	void UpdatePublishedFileInfoRequests( void );				// Empties pending url list, calls webapi requesting info for them

	// Returns the collection id if sucessful, 0 otherwise. 
	PublishedFileId_t ParseCollectionInfo( KeyValues * pDetails ); 

	void UpdateUGCDownloadRequests( void );						// http get for the content's URL, stream to disk
	bool IsFileLatestVersion( const DedicatedServerUGCFileInfo_t* ugcInfo );	// Test file timestamp vs last update time.

	void UpdateFiles( const CUtlVector<PublishedFileId_t>& vecFileIDs );
	void UpdateFile( PublishedFileId_t id );

	void OnFileInfoReceived( const DedicatedServerUGCFileInfo_t *pInfo );
	void OnFileInfoRequestFailed( PublishedFileId_t id );
	void OnCollectionInfoReceived( PublishedFileId_t collectionId, const CUtlVector< PublishedFileId_t > & vecCollectionItems );
	void OnCollectionInfoRequestFailed( PublishedFileId_t id );
	void OnFileDownloaded( const DedicatedServerUGCFileInfo_t *pInfo );

	// Record we have this bsp on disk (may not be up to date)
	void NoteWorkshopMapOnDisk( PublishedFileId_t id, const char* szPath );

	bool ShouldUpdateCollection( PublishedFileId_t id, const CUtlVector<PublishedFileId_t>& vecMaps );

	void RemoveFileInfo ( PublishedFileId_t id );

	void Cleanup( void );

	void QueueDownloadFile( const DedicatedServerUGCFileInfo_t *pFileInfo );

	// List of downloads in progress
	CUtlVector< CStreamingUGCDownloader* > m_PendingFileDownloads;

	CUtlVector< PublishedFileId_t > m_FileInfoQueries; // info requests yet to be sent
	CUtlVector< CPublishedFileInfoHTTPRequest* > m_PendingFileInfoRequests; // info requests in flight

	CUtlVector< PublishedFileId_t > m_CollectionInfoQueries; // both use file ids, but if it's an id for a collection we need to call a different webapi
	CUtlVector< CCollectionInfoHTTPRequest* > m_PendingCollectionInfoRequests; // collection info requests in flight

	CUtlVector< PublishedFileId_t > m_vecMapsBeingUpdated; // IDs of maps either waiting for newest file info or in the process of downloading

	CUtlVector< PublishedFileId_t > m_vecWorkshopMapList; // Maps we have on disk
	CUtlMap< PublishedFileId_t, CUtlString > m_mapWorkshopIdsToMapNames;

	CUtlVector< PublishedFileId_t > m_vecFileQueryRetries;
	CUtlVector< PublishedFileId_t > m_vecCollectionQueryRetries;

	// Map of files we have queried info for.
	typedef CUtlMap< PublishedFileId_t, DedicatedServerUGCFileInfo_t* > MapFileIdToUgcFileInfo_t;
	MapFileIdToUgcFileInfo_t m_UGCFileInfos;

	// Helper class to keep track of our updating/downloading of a set of maps, then create a map group when we have them all.
	CWorkshopMapGroupBuilder* m_pMapGroupBuilder;
	PublishedFileId_t	m_desiredHostCollection;
	PublishedFileId_t	m_unTargetStartMap;  // supports server ops specifying which map in a collection they wish to start on.

	char m_szWebAPIAuthKey[AUTH_KEY_MAX_LEN];
	bool m_bFoundAuthKey;

	bool m_bCurrentLevelNeedsUpdate; // Only gets updated if external caller requests it... only needed for a special case in cs_gamerules's restarting logic.
	PublishedFileId_t m_hackCurrentMapInfoCheck; // HACK: Will skip the next download for a file matching this publish ID, then zero this member.

	bool m_bHostedCollectionUpdatePending;
	double m_fTimeLastVersionCheck;

	CUtlMap< PublishedFileId_t, KeyValues* > m_mapPreviousCollectionQueryCache;  // Old collection infos for use when we can't talk to steam.
};



CDedicatedServerWorkshopManager& DedicatedServerWorkshop( void );

#endif // DEDICATED_SERVER_UGC_MANAGER

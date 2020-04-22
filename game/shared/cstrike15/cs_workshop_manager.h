//====== Copyright  Valve Corporation, All rights reserved. =================
//
// Requests subscribed maps from the workshop, holds a list of them along with metadata.
// NOTE: This could probably all just live in whatever UI element lets the user see/edit
// the list of subscribed maps.
//
//=============================================================================
#if !defined CS_WORKSHOP_MANAGER_H
#define CS_WORKSHOP_MANAGER_H
#if defined( COMPILER_MSVC )
#pragma once
#endif

#if !defined ( _GAMECONSOLE ) && !defined ( NO_STEAM )

#include "ugc_request_manager.h"
#include "ugc_file_info_manager.h"
#include "ugc_workshop_manager.h"
#include "igamesystem.h"

enum {
	UGC_PRIORITY_GENERIC = 0,
	UGC_PRIORITY_BSP,
	UGC_PRIORITY_THUMBNAIL
};

// Handle file requests for community maps (downloads thumbnail / content)
class CBaseCommunityRequest : public CBasePublishedFileRequest
{
public:
	CBaseCommunityRequest( PublishedFileId_t nFileID ) : 
		CBasePublishedFileRequest( nFileID )
	{}

	virtual void OnLoaded( PublishedFileInfo_t &info );
};

// Handle file requests for community maps (downloads thumbnail / content)
class CCommunityMapRequest : public CBaseCommunityRequest
{
public:

	typedef CBaseCommunityRequest BaseClass;

	CCommunityMapRequest( PublishedFileId_t nFileID, uint32 nSubscribeTime ) :
		BaseClass( nFileID ),
		m_unSubscribeTime( nSubscribeTime )
	  {}


	virtual void OnLoaded( PublishedFileInfo_t &info );
	virtual void OnError( EResult nErrorCode );

	uint32 m_unSubscribeTime;
};


// Autogamesystem to request user maps on startup and call update on the workshop manager.
class CSGOWorkshopMaps : public CAutoGameSystemPerFrame, public CBaseWorkshopManagerCallbackInterface
{
public:
	CSGOWorkshopMaps();

	virtual bool Init( void ) OVERRIDE;
	virtual void Shutdown( void ) OVERRIDE;
	virtual void Update( float frametime ) OVERRIDE;
	virtual const char* Name( void ) OVERRIDE { return "CSGOWorkshop"; }

	bool CreateThumbnailFileRequest( const PublishedFileInfo_t &info );
	bool CreateMapFileRequest( const PublishedFileInfo_t &info );
	bool AddPublishedFileVoteInfoRequest( const PublishedFileInfo_t *pInfo, bool bForceUpdate = false );
	bool UnsubscribeFromMap( PublishedFileId_t nMapID );
	bool RemoveCommunityMap( PublishedFileId_t nID );

	// Enumeration of subscribed files by this user
	CCallResult<CSGOWorkshopMaps, RemoteStorageEnumerateUserSubscribedFilesResult_t> m_callbackEnumerateSubscribedMaps;
	void Steam_OnEnumerateSubscribedMaps( RemoteStorageEnumerateUserSubscribedFilesResult_t *pResult, bool bError );

	// CBaseWorkshopManagerCallbackInterface
	virtual void OnFileRequestFinished( UGCHandle_t hFileHandle ) OVERRIDE;
	virtual void OnFileRequestError( UGCHandle_t hFileHandle ) OVERRIDE;

	// Published files
	virtual void OnPublishedFileSubscribed( PublishedFileId_t nID ) OVERRIDE;
	virtual void OnPublishedFileUnsubscribed( PublishedFileId_t nID ) OVERRIDE;
	virtual void OnPublishedFileDeleted( PublishedFileId_t nID ) OVERRIDE;

	bool IsSubscribedMap( const char *pchMapName, bool bOnlyOnDisk );
	bool IsFeaturedMap( const char *pchMapName, bool bOnlyOnDisk );

	// Makes sure the latest version of the given ugc item is on disk.
	// This does not subscribe to the content and is intended for downloading 
	// maps the server requires in order to play.
	void DownloadMapFile( PublishedFileId_t id );

	// Return download progress from 0.0 - 1.0, or -1.0 on error
	float GetFileDownloadProgress( PublishedFileId_t id ) const;

	bool ViewCommunityMapInWorkshop( PublishedFileId_t nFileID );
	bool ViewAllCommunityMapsInWorkshop( void );

	// returns reference to a list of subscribed community maps
	const CUtlVector< PublishedFileId_t >& GetSubscribedMapList() { return m_vecCommunityMapsQueue; }

	bool WasFileRecentlyUpdated( PublishedFileId_t id ) const;
	
	bool EnumerateMapsFailed( void ) const { return m_bEnumerateMapsFailed; }

private:
	bool						m_bUGCRequestsPaused;

	bool						m_bEnumerateMapsFailed;

	CUtlVector< PublishedFileId_t >	m_vecCommunityMapsQueue;

	int					m_nTotalSubscriptionsLoaded;				// Number of subscriptions we've received from the Steam server. This may not be the total number available, meaning we need to requery.
	bool				m_bReceivedQueueBaseline;					// Whether or not we've heard back successfully from Steam on our queue status
	float				m_flQueueBaselineRequestTime;				// Time that the baseline was requested from the GC

	void UpdateWorkshopRequests( void );
	void QueryForCommunityMaps( void );
	void AddCommunityMap( PublishedFileId_t nMapID, uint32 nSubscribeTime );
};

extern CSGOWorkshopMaps g_CSGOWorkshopMaps;
CWorkshopManager &WorkshopManager( void );

#endif // !_GAMECONSOLE && !NO_STEAM
#endif // CS_WORKSHOP_MANAGER_H
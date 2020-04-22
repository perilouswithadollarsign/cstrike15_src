//====== Copyright  Valve Corporation, All rights reserved. =================
//
//=============================================================================
#include "cbase.h"
#include "cs_workshop_manager.h"

#include "tier2/fileutils.h"

#if !defined ( _GAMECONSOLE ) && !defined ( NO_STEAM )
#define COMMUNITY_MAP_PATH				"maps/workshop"	// Path to Workshop maps downloaded from Steam
#define COMMUNITY_MAP_THUMBNAIL_PREFIX	"thumb"			// Prefix for thumbnail filename

extern ConVar cl_debug_ugc_downloads;

CSGOWorkshopMaps g_CSGOWorkshopMaps;
CWorkshopManager g_WorkshopManager( &g_CSGOWorkshopMaps );
CWorkshopManager &WorkshopManager( void )
{ 
	return g_WorkshopManager; 
}

CON_COMMAND( cl_remove_all_workshop_maps, "Removes all maps from the workshop directory." )
{
	CUtlVector<CUtlString> outList;
	RecursiveFindFilesMatchingName( &outList, "maps/workshop", "*.*", "GAME" );
	FOR_EACH_VEC( outList, i )
	{
		CUtlString &curMap = outList[i];
		// skip '.' and '..' results
		if ( curMap.Length() > 0 && curMap.Access()[curMap.Length()-1] == '.' )
			continue;

		PublishedFileId_t id = GetMapIDFromMapPath( curMap.Access() );
		if ( id != 0 )
		{
			filesystem->RemoveFile( outList[i] );
		}
	}

}

CSGOWorkshopMaps::CSGOWorkshopMaps()
{
	m_bUGCRequestsPaused = false;
	m_bEnumerateMapsFailed = false;
}

bool CSGOWorkshopMaps::Init( void )
{
	UGCUtil_Init();
	QueryForCommunityMaps();

	return true;
}

void CSGOWorkshopMaps::Shutdown( void )
{
	UGCUtil_Shutdown();
}

void CSGOWorkshopMaps::Update( float frametime )
{
	UpdateWorkshopRequests();
}
//-----------------------------------------------------------------------------
// Purpose: Update all our Workshop work
//-----------------------------------------------------------------------------
void CSGOWorkshopMaps::UpdateWorkshopRequests( void )
{
	// Update our file upload/download requests from Steam
	WorkshopManager().Update();
}

//-----------------------------------------------------------------------------
// Purpose: Request all subscribed files for a user
//-----------------------------------------------------------------------------
void CSGOWorkshopMaps::QueryForCommunityMaps( void )
{
	Log_Msg( LOG_WORKSHOP, "[CSGOWorkshopMaps] Querying for subscribed files\n" );

	//AddCommunityMap(  2747, 0 );
	// Start our call for subscribed maps 
	if ( ISteamRemoteStorage *pRemoteStorage = GetISteamRemoteStorage() )
	{
		m_flQueueBaselineRequestTime = gpGlobals->curtime;

		// FIXME: JDW - Remove to test timeout
		// return;

		int nStartIndex = ( m_nTotalSubscriptionsLoaded > 0 ) ? (m_nTotalSubscriptionsLoaded-1) : 0;
		SteamAPICall_t hSteamAPICall = pRemoteStorage->EnumerateUserSubscribedFiles( nStartIndex );
		m_callbackEnumerateSubscribedMaps.Set( hSteamAPICall, this, &CSGOWorkshopMaps::Steam_OnEnumerateSubscribedMaps );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Callback for completion of enumerating the subscribed maps for the user
//-----------------------------------------------------------------------------
void CSGOWorkshopMaps::Steam_OnEnumerateSubscribedMaps( RemoteStorageEnumerateUserSubscribedFilesResult_t *pResult, bool bError )
{
	// Make sure we succeeded
	if ( bError || pResult->m_eResult != k_EResultOK )
	{
		Warning( "Unable to enumerate user's subscribed maps!\n" );
		m_bEnumerateMapsFailed = true;
		return;
	}

	m_bEnumerateMapsFailed = false;

	// Queue up all the known subscribed files to work through over subsequent frames
	const int nNumMaps = pResult->m_nResultsReturned;
	for ( int i = 0; i < nNumMaps; i++ )
	{
		AddCommunityMap( pResult->m_rgPublishedFileId[i], pResult->m_rgRTimeSubscribed[i] );
	}

	m_nTotalSubscriptionsLoaded += nNumMaps;
	m_bReceivedQueueBaseline = true;

	// If our queue is bigger than this call could return in one go, call again to receive more
	if ( m_nTotalSubscriptionsLoaded < pResult->m_nTotalResultCount )
	{
		QueryForCommunityMaps();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add a new map into the system
//-----------------------------------------------------------------------------
void CSGOWorkshopMaps::AddCommunityMap( PublishedFileId_t nMapID, uint32 nSubscribeTime )
{
	// Queue up a new request for this file's history
	CCommunityMapRequest *pMapRequest = new CCommunityMapRequest( nMapID, nSubscribeTime );
	WorkshopManager().AddFileInfoQuery( pMapRequest, true );

	// Add this into our list of history items for this user
	if ( m_vecCommunityMapsQueue.Find( nMapID ) == m_vecCommunityMapsQueue.InvalidIndex() )
	{
		m_vecCommunityMapsQueue.AddToTail( nMapID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Requests voting information for a published file
// Return:	If true, the voting data already exists and can be immediately accessed. Otherwise, the caller will need to wait
//-----------------------------------------------------------------------------
bool CSGOWorkshopMaps::AddPublishedFileVoteInfoRequest( const PublishedFileInfo_t *pInfo, bool bForceUpdate /*=false*/ )
{
	return WorkshopManager().AddPublishedFileVoteInfoRequest( pInfo, bForceUpdate );
}

//-----------------------------------------------------------------------------
// Purpose: Tell the GC to unsubscribe from this map
//-----------------------------------------------------------------------------
bool CSGOWorkshopMaps::UnsubscribeFromMap( PublishedFileId_t nMapID )
{
	if ( steamapicontext == NULL || steamapicontext->SteamRemoteStorage() == NULL )
		return false;

	if ( steamapicontext->SteamRemoteStorage()->UnsubscribePublishedFile( nMapID ) == k_uAPICallInvalid )
	{
		//TODO: Handle the error case
		Log_Warning( LOG_WORKSHOP, "[BaseModPanel] Failed to unsubscribe from published file: %llu\n", nMapID );
		return false;
	}

	Log_Msg( LOG_WORKSHOP, "[BaseModPanel] Unsubscribing from published file: %llu\n", nMapID );

	// NOTE: We listen for unsubscription notifications already, so we'll pick up that case down the road

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Delete a file request from the system (optionally removing its downloaded content form the disk)
//-----------------------------------------------------------------------------
bool CSGOWorkshopMaps::RemoveCommunityMap( PublishedFileId_t nID )
{
	/*
	// FIXME: Grr, we need to handle the current query being the one we're trying to nuke!
	if ( m_nFilesToQuery.Count() && m_nFilesToQuery.Head() == nID )
	{
		Assert(0);
	}
	*/

	// Remove this from our queue, but leave the file information intact
	if ( m_vecCommunityMapsQueue.FindAndFastRemove( nID ) )
	{
		// Notify the dialog that something has changed underneath it and it needs to refresh
		// UNDONE -- 
		
		return true;
	}

	return false;
}

void CSGOWorkshopMaps::OnFileRequestFinished( UGCHandle_t hFileHandle )
{

}

void CSGOWorkshopMaps::OnFileRequestError( UGCHandle_t hFileHandle )
{

}

void CSGOWorkshopMaps::OnPublishedFileSubscribed( PublishedFileId_t nID )
{
	/** Removed for partner depot **/
}

void CSGOWorkshopMaps::OnPublishedFileUnsubscribed( PublishedFileId_t nID )
{
	RemoveCommunityMap( nID );

	IGameEvent* pEvent = gameeventmanager->CreateEvent( "ugc_map_unsubscribed" ); 
	if ( pEvent )
	{
		pEvent->SetUint64( "published_file_id", nID );
		gameeventmanager->FireEventClientSide( pEvent );
	}
}

void CSGOWorkshopMaps::OnPublishedFileDeleted( PublishedFileId_t nID )
{

}

bool CSGOWorkshopMaps::IsSubscribedMap( const char *pchMapName, bool bOnlyOnDisk )
{
	for ( int i = 0; i < m_vecCommunityMapsQueue.Count(); ++i )
	{
		const PublishedFileInfo_t *pFileInfo = WorkshopManager().GetPublishedFileInfoByID( m_vecCommunityMapsQueue[ i ] );
		if ( pFileInfo )
		{
			char szBaseFilename[ MAX_PATH ];
			V_FileBase( pFileInfo->m_pchFileName, szBaseFilename, sizeof( szBaseFilename ) );

			if ( V_stricmp( pchMapName, szBaseFilename ) == 0 )
			{
				return true;
			}
		}
	}

	return false;
}

bool CSGOWorkshopMaps::IsFeaturedMap( const char *pchMapName, bool bOnlyOnDisk )
{
	for ( int i = 0; i < m_vecCommunityMapsQueue.Count(); ++i )
	{
		if ( false )
			return true;
	}

	return false;
}

bool CSGOWorkshopMaps::CreateMapFileRequest( const PublishedFileInfo_t &info )
{
	// Grab the map file
	return WorkshopManager().CreateFileDownloadRequest(	info.m_hFile,
														info.m_nPublishedFileId,
														CFmtStr( "%s/%llu", COMMUNITY_MAP_PATH, info.m_nPublishedFileId ),
														V_GetFileName( info.m_pchFileName ),
														UGC_PRIORITY_BSP,
														info.m_rtimeUpdated );	
}

bool CSGOWorkshopMaps::CreateThumbnailFileRequest( const PublishedFileInfo_t &info )
{
	// Grab the thumbnail file
	return WorkshopManager().CreateFileDownloadRequest(	info.m_hPreviewFile,
														info.m_nPublishedFileId,
														CFmtStr( "%s/%llu", COMMUNITY_MAP_PATH, info.m_nPublishedFileId ),
														CFmtStr( "%s%llu.jpg", COMMUNITY_MAP_THUMBNAIL_PREFIX, info.m_nPublishedFileId ),
														UGC_PRIORITY_THUMBNAIL,
														info.m_rtimeUpdated );
}

// Gets a map file from the workshop without subscribing to it. This is needed for joining servers
// running community maps
void CSGOWorkshopMaps::DownloadMapFile( PublishedFileId_t id )
{
	CCommunityMapRequest *pMapRequest = new CCommunityMapRequest( id, 0 );
	WorkshopManager().AddFileInfoQuery( pMapRequest, true );
	// WorkshopManager().SendAllFileInfoQueries(); // 
	UpdateWorkshopRequests();	// HACK-- When called from engine during level load, we don't get our update calls yet
								// so the queries never get sent and the download never begins... Force an update now.
}

float CSGOWorkshopMaps::GetFileDownloadProgress( PublishedFileId_t id ) const
{
	// Haven't got the file info yet
	if ( WorkshopManager().IsFileInfoRequestStillPending( id ) )
		return 0.0f;

	const PublishedFileInfo_t *pInfo = WorkshopManager().GetPublishedFileInfoByID( id );
	Assert( pInfo );
	if ( !pInfo )
		return -1.0f; // No info for this file

	// Once file is on disk, return 100%
	if ( WorkshopManager().GetUGCFileRequestStatus( pInfo->m_hFile ) != UGCFILEREQUEST_FINISHED )
	{
		float flProgress = WorkshopManager().GetUGCFileDownloadProgress( pInfo->m_hFile );
		return clamp( flProgress, 0, 0.99f ); // HACK: Don't return 100% completion until we copy the file to it's final path on disk
	}
	else
	{
		return 1.0f; // return 100% once the status is UGCFILEREQUEST_FINISHED 
	}
}

//-----------------------------------------------------------------------------
// Purpose: The base class handles book keeping we'd like all of our requests to do
//-----------------------------------------------------------------------------
void CBaseCommunityRequest::OnLoaded( PublishedFileInfo_t &info )
{
	// Ask Steam for more information about this user, since we'll be displaying their persona / avatar later
	steamapicontext->SteamFriends()->RequestUserInformation( info.m_ulSteamIDOwner, false );

	// Get voting information about it
	g_CSGOWorkshopMaps.AddPublishedFileVoteInfoRequest( (const PublishedFileInfo_t *) &info);


}

//-----------------------------------------------------------------------------
// Purpose: Grab the thumbnail for our queue history
//-----------------------------------------------------------------------------
void CCommunityMapRequest::OnLoaded( PublishedFileInfo_t &info ) 
{ 
	// Install our data here
	info.m_rtimeSubscribed = m_unSubscribeTime;

	BaseClass::OnLoaded( info );

	// Grab the thumbnail
	bool bCreatedThumbRequest = g_CSGOWorkshopMaps.CreateThumbnailFileRequest( info );
	Assert( bCreatedThumbRequest );
	if ( bCreatedThumbRequest == false )
	{
		Log_Warning( LOG_WORKSHOP, "Unable to create thumbnail request for %llu!\n", info.m_nPublishedFileId );
	}

	// Grab the content file
	bool bCreateMapRequest = g_CSGOWorkshopMaps.CreateMapFileRequest( info );
	Assert( bCreateMapRequest );
	if ( bCreateMapRequest == false )
	{
		Log_Warning( LOG_WORKSHOP, "Unable to create content file request for %llu!\n", info.m_nPublishedFileId );
	}

	IGameEvent* pEvent = gameeventmanager->CreateEvent( "ugc_map_info_received" ); 
	if ( pEvent )
	{
		pEvent->SetUint64( "published_file_id", GetTargetID() );
		gameeventmanager->FireEventClientSide( pEvent );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle error case for community map requests
//-----------------------------------------------------------------------------
void CCommunityMapRequest::OnError( EResult nErrorCode )
{
	// If the file is now gone, unsubscribe from it
	if ( nErrorCode == k_EResultFileNotFound )
	{
		g_CSGOWorkshopMaps.UnsubscribeFromMap( GetTargetID() );
	}

	IGameEvent* pEvent = gameeventmanager->CreateEvent( "ugc_map_download_error" ); 
	if ( pEvent )
	{
		pEvent->SetUint64( "published_file_id", GetTargetID() );
		pEvent->SetInt( "error_code", nErrorCode );
		gameeventmanager->FireEventClientSide( pEvent );
	}

	g_CSGOWorkshopMaps.RemoveCommunityMap( GetTargetID() );
}
//-----------------------------------------------------------------------------
// Purpose: Pop up the overlay showing the requested community map page
//-----------------------------------------------------------------------------
bool CSGOWorkshopMaps::ViewCommunityMapInWorkshop( PublishedFileId_t nFileID )
{
	if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamUtils() && steamapicontext->SteamFriends() && steamapicontext->SteamUtils() )
	{
		// Overlay is disabled
		if( !steamapicontext->SteamUtils()->IsOverlayEnabled() )
			return false;

		EUniverse eUniverse = steamapicontext && steamapicontext->SteamUtils()
			? steamapicontext->SteamUtils()->GetConnectedUniverse()
			: k_EUniverseInvalid;

		if ( eUniverse == k_EUniverseInvalid )
			return false;

		char szDestURL[MAX_PATH];
		const char *lpszDomanPrefix = ( eUniverse == k_EUniverseBeta ) ? "beta" : "www";
		V_snprintf( szDestURL, ARRAYSIZE(szDestURL), "http://%s.steamcommunity.com/sharedfiles/filedetails/?id=%llu", lpszDomanPrefix, nFileID );

		steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( szDestURL );

		return true;
	}	

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Pop up the overlay to allow users to browse the Workshop
//-----------------------------------------------------------------------------
bool CSGOWorkshopMaps::ViewAllCommunityMapsInWorkshop( void )
{
	if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamFriends() && steamapicontext->SteamUtils() )
	{
		// Overlay is disabled
		if( !steamapicontext->SteamUtils()->IsOverlayEnabled() )
			return false;

		EUniverse eUniverse = steamapicontext && steamapicontext->SteamUtils()
			? steamapicontext->SteamUtils()->GetConnectedUniverse()
			: k_EUniverseInvalid;

		if ( eUniverse == k_EUniverseInvalid )
			return false;

		char szDestURL[MAX_PATH];
		const char *lpszDomanPrefix = ( eUniverse == k_EUniverseBeta ) ? "beta" : "www";
		V_snprintf( szDestURL, ARRAYSIZE(szDestURL), "http://%s.steamcommunity.com/workshop/browse?appid=%d", lpszDomanPrefix, steamapicontext->SteamUtils()->GetAppID() );

		steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( szDestURL );

		return true;
	}	

	return false;
}

bool CSGOWorkshopMaps::WasFileRecentlyUpdated( PublishedFileId_t id ) const
{
	/** Removed for partner depot **/
	return false;
}

#endif // !_GAMECONSOLE

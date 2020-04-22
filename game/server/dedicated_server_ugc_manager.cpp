//========== Copyright  Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================
#include "cbase.h"
#include "dedicated_server_ugc_manager.h"
#include "steam/isteamhttp.h"
#include "ugc_utils.h"
#include "tier2/fileutils.h"
#include "gametypes.h"

// TODO: can we swap this out based on steam universe?
const char* g_szAuthKeyFilename = "webapi_authkey.txt";
const char* g_szCollectionCacheFileName = "ugc_collection_cache.txt";

const char* g_szSubscribedFilesList = "subscribed_file_ids.txt";
const char* g_szSubscribedCollectionsList = "subscribed_collection_ids.txt";

// Subdir relative to game dir to store workshop maps in
const char* g_szWorkshopMapBasePath = "maps/workshop";

const char* GetApiBaseUrl( void )
{
	return "https://api.steampowered.com";
	/*
	if ( steamapicontext && steamapicontext->SteamUtils() )
	{
		if ( steamapicontext->SteamUtils()->GetConnectedUniverse() == k_EUniverseBeta )
			return "https://api-beta.steampowered.com";
		else if ( steamapicontext->SteamUtils()->GetConnectedUniverse() == k_EUniversePublic )
			return "https://api.steampowered.com";
	}

	Assert( 0 );
	return "";
	*/
}

ConVar sv_debug_ugc_downloads( "sv_debug_ugc_downloads", "0", FCVAR_RELEASE );
ConVar sv_broadcast_ugc_downloads( "sv_broadcast_ugc_downloads", "0", FCVAR_RELEASE );
ConVar sv_broadcast_ugc_download_progress_interval( "sv_broadcast_ugc_download_progress_interval", "8", FCVAR_RELEASE );
ConVar sv_ugc_manager_max_new_file_check_interval_secs( "sv_ugc_manager_max_new_file_check_interval_secs", "1000", FCVAR_RELEASE );
ConVar sv_remove_old_ugc_downloads( "sv_remove_old_ugc_downloads", "1", FCVAR_RELEASE );
ConVar sv_test_steam_connection_failure( "sv_test_steam_connection_failure", "0" );

CDedicatedServerWorkshopManager g_DedicatedServerWorkshopManager;
CDedicatedServerWorkshopManager& DedicatedServerWorkshop( void )
{
	return g_DedicatedServerWorkshopManager;
}

CON_COMMAND( workshop_start_map, "Sets the first map to load once a workshop collection been hosted. Takes the file id of desired start map as a parameter." )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() != 2 )
	{
		Msg( "Usage: workshop_start_map <fileid>\n");
		return;
	}

	PublishedFileId_t id = (PublishedFileId_t)V_atoui64( args[1] );
	if ( id == 0 )
	{
		Msg( "Invalid file id.\n");
		return;
	}

	DedicatedServerWorkshop().SetTargetStartMap( id );
}

CON_COMMAND( host_workshop_map, "Get the latest version of the map and host it on this server." ) 
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() != 2 )
	{
		Msg( "Usage: host_workshop_map <fileid>\n");
		return;
	}

	PublishedFileId_t id = (PublishedFileId_t)V_atoui64( args[1] );
	if ( id == 0 )
	{
		Msg( "Invalid file id.\n");
		return;
	}

	// HACK: Need to load a map for steam server api to be available and download maps... peeling out the init code 
	// would be better, but loading dust works for now.
	if ( !steamgameserverapicontext || !steamgameserverapicontext->SteamHTTP() )
		engine->ServerCommand( CFmtStr( "map de_dust server_is_unavailable\n" ).Access() );

	DedicatedServerWorkshop().HostWorkshopMap( id );
}

CON_COMMAND( host_workshop_collection, "Get the latest version of maps in a workshop collection and host them as a maplist." ) 
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() != 2 )
	{
		Msg( "Usage: host_workshop_collection <fileid>\n");
		return;
	}

	PublishedFileId_t id = (PublishedFileId_t)V_atoui64( args[1] );
	if ( id == 0 )
	{
		Msg( "Invalid file id.\n");
		return;
	}

	// HACK: Need to load a map for steam server api to be available and download maps... peeling out the init code 
	// would be better, but loading dust works for now.
	if ( !steamgameserverapicontext || !steamgameserverapicontext->SteamHTTP() )
		engine->ServerCommand( CFmtStr( "map de_dust server_is_unavailable\n" ).Access() );

	DedicatedServerWorkshop().HostWorkshopMapCollection( id );
}


bool DedicatedServerUGCFileInfo_t::BuildFromKV( KeyValues *pPublishedFileDetails )
{
	m_bIsValid = false;
	m_result = (EResult)pPublishedFileDetails->GetInt( "result", -1 );

	// Parse file id first, we should get this even on failures.
	fileId = pPublishedFileDetails->GetUint64( "publishedfileid", 0ll ); 
	if ( !fileId )
		return false;

	if ( m_result != k_EResultOK )
		return false;

	int appId = pPublishedFileDetails->GetInt( "consumer_appid", 0 );
	if ( appId != engine->GetAppID() )
			return false;

	if ( pPublishedFileDetails->GetInt( "banned", 0 ) != 0 )
		return false;

	contentHandle = pPublishedFileDetails->GetUint64( "hcontent_file", 0ll ); 
	if ( !contentHandle )
		return false;
	
	const char* szUrl = pPublishedFileDetails->GetString( "file_url", NULL );
	if ( !szUrl )
		return false;
	V_strcpy_safe( m_szUrl, szUrl );

	const char* szName = V_UnqualifiedFileName( pPublishedFileDetails->GetString( "filename", NULL ) );
	if ( !szName )
		return false;
	V_strcpy_safe( m_szFileName, szName );

	m_unFileSizeInBytes = pPublishedFileDetails->GetInt( "file_size", 0 );
	if ( !m_unFileSizeInBytes )
		return false;
	
	const char* szTitle = pPublishedFileDetails->GetString( "title", NULL );
	if ( !szTitle )
		return false;
	V_strcpy_safe( m_szTitle, szTitle );

	m_unTimeLastUpdated = pPublishedFileDetails->GetInt( "time_updated", 0 );
	if ( !m_unTimeLastUpdated )
		return false;

	// Assuming we're downloading maps here...
	V_snprintf( m_szFilePath, ARRAYSIZE(m_szFilePath), "%s/%llu/%s", g_szWorkshopMapBasePath, fileId, szName );

	// TODO tags	

	m_bIsValid = true;
	m_dblPlatFloatTimeReceived = Plat_FloatTime();
	return true;
}

void ParseFileIds( const char* szFileName, CUtlVector<PublishedFileId_t>& outVec )
{
	if ( filesystem->FileExists( szFileName ) )
	{
		int fileSize;
		char* szFileBuf = (char*)UTIL_LoadFileForMe( szFileName, &fileSize );
		if ( szFileBuf && fileSize > 0 )
		{
			CUtlStringList fileIdList;
			V_SplitString( szFileBuf, "\n", fileIdList );
			for ( int i = 0; i < fileIdList.Count(); ++i )
			{
				PublishedFileId_t id = V_atoui64( fileIdList[i] );
				if ( !outVec.HasElement( id ) )
					outVec.AddToTail( id );

				if ( sv_debug_ugc_downloads.GetBool() )
					Msg( "CDedicatedServerWorkshopManager::Init: Subscribing to file id %llu\n", id );
			}
			delete[] szFileBuf;
		}
	}
}

bool CDedicatedServerWorkshopManager::Init( void )
{ 
	m_UGCFileInfos.SetLessFunc( DefLessFunc( PublishedFileId_t ) );
	m_mapWorkshopIdsToMapNames.SetLessFunc( DefLessFunc( PublishedFileId_t) );
	m_mapPreviousCollectionQueryCache.SetLessFunc( DefLessFunc( PublishedFileId_t) );
	m_fTimeLastVersionCheck = 0;
	GetNewestSubscribedFiles();

	// HACK: So if we load a map before we hear back from steam about what files we have subscribed, 
	// we won't submit any workshop map IDs to matchmaking. Scan for any maps in the workshop subdirectory
	// and use any bsps found as available maps. 
	CUtlVector<CUtlString> outList;
	RecursiveFindFilesMatchingName( &outList, g_szWorkshopMapBasePath, "*.bsp", "MOD" );
	FOR_EACH_VEC( outList, i )
	{
		CUtlString &curMap = outList[i];
		PublishedFileId_t id = GetUGCMapPublishedFileID( curMap.Access() );
		if ( id != 0 )
		{
			NoteWorkshopMapOnDisk( id, curMap.Access() );
		}
	}
	
	m_bHostedCollectionUpdatePending = false ;
	m_unTargetStartMap = 0;

	if ( g_pFullFileSystem->FileExists( g_szCollectionCacheFileName, "MOD" ) )
	{
		KeyValues* pCollectionCacheKV = new KeyValues("");
		KeyValues::AutoDelete autodelete( pCollectionCacheKV );
		pCollectionCacheKV->LoadFromFile( g_pFullFileSystem, g_szCollectionCacheFileName, "MOD" );
		for ( KeyValues *pDetails = pCollectionCacheKV->GetFirstSubKey(); pDetails != NULL; pDetails = pDetails->GetNextKey() )
		{
			PublishedFileId_t collectionId = pDetails->GetUint64( "publishedfileid", 0 );
			if ( collectionId != 0 )
			{
				m_mapPreviousCollectionQueryCache.Insert( collectionId, pDetails->MakeCopy() );
			}
		}
	}

	return true;
}

void CDedicatedServerWorkshopManager::LevelInitPreEntity( void )
{
	// reset these every level change. 
	m_hackCurrentMapInfoCheck = 0;
	m_bCurrentLevelNeedsUpdate = false;
	m_bHostedCollectionUpdatePending = false;
}

void CDedicatedServerWorkshopManager::CheckIfCurrentLevelNeedsUpdate( void )
{
	m_bCurrentLevelNeedsUpdate = false;
	PublishedFileId_t id = GetUGCMapPublishedFileID( gpGlobals->mapname.ToCStr() );
	if ( id != 0 )
	{
		m_hackCurrentMapInfoCheck = id;
		if ( !m_FileInfoQueries.HasElement( id ) )
			m_FileInfoQueries.AddToTail( id );
	}
}

CON_COMMAND_F( ds_get_newest_subscribed_files, "Re-reads web api auth key and subscribed file lists from disk and downloads the latest updates of those files from steam", FCVAR_RELEASE )
{
	g_DedicatedServerWorkshopManager.GetNewestSubscribedFiles();
}

void CDedicatedServerWorkshopManager::GetNewestSubscribedFiles( void )
{
	if ( sv_debug_ugc_downloads.GetBool() )
		Msg("CDedicatedServerWorkshopManager::GetNewestSubscribedFiles\n");

	if ( engine->IsDedicatedServer() )
	{
		Q_memset( m_szWebAPIAuthKey, 0, ARRAYSIZE( m_szWebAPIAuthKey ) );
		const char *szAuthKey = CommandLine()->ParmValue( "-authkey", "" );
		if ( !StringIsEmpty( szAuthKey ) )
		{
			V_strcpy_safe( m_szWebAPIAuthKey, szAuthKey );	
		}	
		else if ( filesystem->FileExists( g_szAuthKeyFilename, "MOD" ) )
		{
			int nLength;
			szAuthKey = (const char *)UTIL_LoadFileForMe( g_szAuthKeyFilename, &nLength );
			if ( szAuthKey != NULL )
			{
				if ( !StringIsEmpty( szAuthKey ) )
				{
					V_strcpy_safe( m_szWebAPIAuthKey, szAuthKey );	
					int len = strlen(m_szWebAPIAuthKey);
					while ( len > 0 && V_isspace(m_szWebAPIAuthKey[len-1]) )
					{
						m_szWebAPIAuthKey[len-1] = 0;
						len--;
					}
					if (len>0)
					{
						Msg( "Loaded authkey from %s: %s\n", g_szAuthKeyFilename, m_szWebAPIAuthKey );
					}
				}
				
				UTIL_FreeFile( (byte *)szAuthKey );
				szAuthKey = NULL;
			}
			if ( StringIsEmpty(m_szWebAPIAuthKey) )
			{
				Msg( "Auth key file %s not valid\n", g_szAuthKeyFilename );
			}
		}

		if ( ! StringIsEmpty(m_szWebAPIAuthKey) )
		{
			m_bFoundAuthKey = true;
			if ( sv_debug_ugc_downloads.GetBool() )
				Msg( "CDedicatedServerWorkshopManager::Init: Using auth key [%s]\n", m_szWebAPIAuthKey );
		}
		else
		{
			m_bFoundAuthKey = false;
			Msg( "No web api auth key specified - workshop downloads will be disabled.\n" );
		}

		if ( m_bFoundAuthKey )
		{
			//TODO: protect double adds?
			ParseFileIds( g_szSubscribedFilesList, m_FileInfoQueries );
			m_vecMapsBeingUpdated.AddVectorToTail( m_FileInfoQueries );
			ParseFileIds( g_szSubscribedCollectionsList, m_CollectionInfoQueries );

			// If we're hosting a workshop map collection, get the latest version of those maps
			PublishedFileId_t id =  V_atoui64( gpGlobals->mapGroupName.ToCStr() );
			if ( g_pGameTypes->IsWorkshopMapGroup( gpGlobals->mapGroupName.ToCStr() ) && id != 0 )
			{
				// Clumsy special case for single maps: If there's one entry and it's ID matches the collection name, it's really just a map and not a collection
				const CUtlStringList * pMapList = g_pGameTypes->GetMapGroupMapList( gpGlobals->mapGroupName.ToCStr() );
				if ( pMapList->Count() == 1 && GetUGCMapPublishedFileID( (*pMapList)[0] ) == id )
				{
					UpdateFile( id );
				}
				else
				{
					m_CollectionInfoQueries.AddToTail( id );
				}
			}

			m_fTimeLastVersionCheck = Plat_FloatTime();
		}
	}
}

void CDedicatedServerWorkshopManager::Shutdown( void )
{
	KeyValues* pOutKV = new KeyValues( "CollectionInfoCache" );
	KeyValues::AutoDelete autodelete( pOutKV );
	FOR_EACH_MAP( m_mapPreviousCollectionQueryCache, i )
	{
		pOutKV->AddSubKey( m_mapPreviousCollectionQueryCache[i]->MakeCopy() );
		m_mapPreviousCollectionQueryCache[i]->deleteThis();
	}
	pOutKV->SaveToFile( g_pFullFileSystem, g_szCollectionCacheFileName, "MOD" );

	Cleanup();
}

void CDedicatedServerWorkshopManager::Cleanup( void )
{
	FOR_EACH_VEC_BACK( m_PendingFileDownloads, i )
	{
		delete m_PendingFileDownloads[i];
		m_PendingFileDownloads.Remove( i );
	}
	m_UGCFileInfos.PurgeAndDeleteElements();
	m_FileInfoQueries.RemoveAll();
	m_CollectionInfoQueries.RemoveAll();
	m_vecWorkshopMapList.RemoveAll();
	m_mapWorkshopIdsToMapNames.RemoveAll();
	m_vecMapsBeingUpdated.RemoveAll();
	m_bFoundAuthKey = false;
	m_desiredHostCollection = 0;
	m_bHostedCollectionUpdatePending = false;

	if ( m_pMapGroupBuilder )
	{
		delete m_pMapGroupBuilder;
		m_pMapGroupBuilder = NULL;
	}
}


void CDedicatedServerWorkshopManager::Update( void )
{
	if ( !m_bFoundAuthKey )
		return;

	if ( steamgameserverapicontext == NULL )
		return;

	UpdatePublishedFileInfoRequests();
	UpdateUGCDownloadRequests();

	if ( m_pMapGroupBuilder )
	{
		if ( m_pMapGroupBuilder->IsFinished() )
		{
			m_pMapGroupBuilder->CreateOrUpdateMapGroup();

			if ( m_desiredHostCollection != 0 && m_pMapGroupBuilder->GetFirstMap() != NULL )
			{
				// Set the map group and changelevel if this was our target hosting map group
				const char* szStartMap = m_unTargetStartMap ?  m_pMapGroupBuilder->GetMapMatchingId( m_unTargetStartMap ) : m_pMapGroupBuilder->GetFirstMap();
				engine->ServerCommand( CFmtStr( "mapgroup %llu;map %s\n", m_pMapGroupBuilder->GetId(), szStartMap ).Access() );
				m_unTargetStartMap = 0;
			}

			delete m_pMapGroupBuilder;
			m_pMapGroupBuilder = NULL;
			m_desiredHostCollection = 0;
		}
	}
}

bool CDedicatedServerWorkshopManager::ShouldUpdateCollection( PublishedFileId_t id, const CUtlVector<PublishedFileId_t>& vecMaps )
{
	// Special case for hosted collection updates: Don't re-query all the file infos if our collection contents hasn't changed.
	// For large collections in locations with high ping to steam, getting all that file info takes too long and hangs up level changes.
	const char *szMapGroup = gpGlobals->mapGroupName.ToCStr();
	bool bUpdateCollectionFiles = true;
	if ( m_bHostedCollectionUpdatePending && g_pGameTypes->IsWorkshopMapGroup( szMapGroup ) )
	{
		PublishedFileId_t curHostedCollectionID = V_atoui64( szMapGroup );
		Assert ( curHostedCollectionID != 0 ); // map groups hosted by dedicated servers should always have a uint64 name
		if ( curHostedCollectionID == id )
		{
			const CUtlStringList &maplist = *g_pGameTypes->GetMapGroupMapList( szMapGroup );
			// NOTE: this gives false positives if the collection contains invalid ids (eg, removed from workshop, etc)
			// because bad items in the list won't end up in the final map group, causing the count mismatch... 
			bool bChanged = maplist.Count() != vecMaps.Count();
			if ( !bChanged )
			{
				// If count matches, make sure the contents are the same. If so, we can skip updating the collection
				FOR_EACH_VEC( maplist, i )
				{
					PublishedFileId_t id = GetUGCMapPublishedFileID( maplist[i] );
					if ( vecMaps.Find( id ) == -1 )
					{
						bChanged = true;
						break;
					}
				}
			}

			bUpdateCollectionFiles = bChanged;
		}
		
		// Make sure we mark our collection as no longer updating
		if ( ( id == m_desiredHostCollection ) || ( id == curHostedCollectionID ) )
		{
			m_bHostedCollectionUpdatePending = false;
		}

	}

	return bUpdateCollectionFiles;
}

void CDedicatedServerWorkshopManager::UpdatePublishedFileInfoRequests( void )
{
	// Process finished queries
	FOR_EACH_VEC_BACK( m_PendingFileInfoRequests, i )
	{
		CPublishedFileInfoHTTPRequest* pCurRequest = m_PendingFileInfoRequests[i];
		if( !pCurRequest->IsFinished() )	// still waiting on a response 
			continue;

		if ( pCurRequest->GetLastHTTPResult() != k_EHTTPStatusCode200OK || sv_test_steam_connection_failure.GetBool() )
		{
			// Handle http errors, retries
			
			// Remove failed map ids from the pending list
			if ( sv_debug_ugc_downloads.GetBool() )
				Msg( "Failed to get file info information from steam, HTTP status: %d\n. Missing info for file ids: ", pCurRequest->GetLastHTTPResult() );
			FOR_EACH_VEC( pCurRequest->GetItemsQueried(), i )
			{
				PublishedFileId_t id = pCurRequest->GetItemsQueried()[i];
				OnFileInfoRequestFailed( id );
				if ( sv_debug_ugc_downloads.GetBool() )
					Msg( "%llu ", id );
			}
			if ( sv_debug_ugc_downloads.GetBool() )
				Msg( "\n" );
		}
		else
		{
			FOR_EACH_VEC( pCurRequest->GetFileInfoList(), j )
			{
				const DedicatedServerUGCFileInfo_t* pCurInfo = pCurRequest->GetFileInfoList()[j];

				OnFileInfoReceived( pCurInfo );

				if ( pCurInfo->m_result == k_EResultOK && pCurInfo->m_bIsValid )
				{
					if ( sv_debug_ugc_downloads.GetBool() )
						Msg( "CDedicatedServerWorkshopManager: received file details for id %llu: '%s'.\n", pCurInfo->fileId, pCurInfo->m_szTitle ? pCurInfo->m_szTitle : "<no title>" );

					RemoveFileInfo( pCurInfo->fileId );	// Clear any existing entry and cancel any pending download.
					DedicatedServerUGCFileInfo_t* pNewFileInfo = new DedicatedServerUGCFileInfo_t;
					V_memcpy( (void*)pNewFileInfo, (void*)pCurInfo, sizeof ( DedicatedServerUGCFileInfo_t ) );
					m_UGCFileInfos.Insert( pNewFileInfo->fileId, pNewFileInfo );

					// Skip downloading if this is an 'info only' id.
					if ( m_hackCurrentMapInfoCheck == pNewFileInfo->fileId ) 
					{
						m_hackCurrentMapInfoCheck = 0;
						continue;
					}

					if ( !IsFileLatestVersion( pNewFileInfo ) )								
					{
						QueueDownloadFile( pNewFileInfo );
					}
					else
					{
						OnFileDownloaded( pNewFileInfo );
						if ( sv_debug_ugc_downloads.GetBool() )
							Msg( "Skipping download for file id %llu:'%s' - version on disk is latest.\n", pNewFileInfo->fileId, pNewFileInfo->m_szTitle ? pNewFileInfo->m_szTitle : "<no title>" );
					}
				}
				else
				{
					if ( sv_debug_ugc_downloads.GetBool() )
						Msg( "Failed to parse file details KV for id %llu. Result enum: %d\n", pCurInfo->fileId, pCurInfo->m_result );

					if ( pCurInfo->m_result == k_EResultFileNotFound )
						Msg( "File id %llu not found. Probably removed from workshop\n", pCurInfo->fileId );
				}
			}
		}

		delete pCurRequest; // request dealt with
		m_PendingFileInfoRequests.Remove( i );
	}

	FOR_EACH_VEC_BACK( m_PendingCollectionInfoRequests, i )
	{
		CCollectionInfoHTTPRequest *pCurRequest = m_PendingCollectionInfoRequests[i];

		if ( !pCurRequest->IsFinished() )
			continue; 

		if ( pCurRequest->GetLastHTTPResult() != k_EHTTPStatusCode200OK || sv_test_steam_connection_failure.GetBool() )
		{
			if ( sv_debug_ugc_downloads.GetBool() )
				Msg( "Failed to get file info information from steam, HTTP status: %d\n. Missing info for collection ids: ", pCurRequest->GetLastHTTPResult() );
			FOR_EACH_VEC( pCurRequest->GetItemsQueried(), i )
			{
				PublishedFileId_t id = pCurRequest->GetItemsQueried()[i];
				OnCollectionInfoRequestFailed( id );
				if ( sv_debug_ugc_downloads.GetBool() )
					Msg( "%llu ", id );

				if ( id == m_desiredHostCollection )
				{
					int idx = m_mapPreviousCollectionQueryCache.Find( id );
					if ( idx != m_mapPreviousCollectionQueryCache.InvalidIndex() )
					{
						ParseCollectionInfo( m_mapPreviousCollectionQueryCache[idx] );
					}
					else
					{
						m_bHostedCollectionUpdatePending = false; // failed to get info on host collection, and we didn't have it in our cache
					}
				}
			}
			if ( sv_debug_ugc_downloads.GetBool() )
				Msg( "\n" );
		}
		else
		{
			KeyValues* pCollectionDetails = pCurRequest->GetResponseKV();
			for ( KeyValues *pDetails = pCollectionDetails->GetFirstSubKey(); pDetails != NULL; pDetails = pDetails->GetNextKey() )
			{
				PublishedFileId_t collectionId = ParseCollectionInfo( pDetails );
				if ( collectionId != 0 )
				{
					// Save previously queried collection infos to disk in case we lose connection to steam
					int idx = m_mapPreviousCollectionQueryCache.Find( collectionId );
					if ( idx == m_mapPreviousCollectionQueryCache.InvalidIndex() )
						idx = m_mapPreviousCollectionQueryCache.Insert( collectionId );
					else
						m_mapPreviousCollectionQueryCache[idx]->deleteThis();

					m_mapPreviousCollectionQueryCache[idx] = pDetails->MakeCopy();
				}

			}
		}

		delete pCurRequest;
		m_PendingCollectionInfoRequests.Remove( i );
	}

	if ( m_FileInfoQueries.Count() > 0 )
	{
		CPublishedFileInfoHTTPRequest *pRequest = new CPublishedFileInfoHTTPRequest( m_FileInfoQueries );
		pRequest->CreateHTTPRequest( m_szWebAPIAuthKey );
		m_PendingFileInfoRequests.AddToTail( pRequest );
		m_FileInfoQueries.RemoveAll();
	}

	if ( m_CollectionInfoQueries.Count() > 0 )
	{
		CCollectionInfoHTTPRequest *pRequest = new CCollectionInfoHTTPRequest( m_CollectionInfoQueries );
		pRequest->CreateHTTPRequest( m_szWebAPIAuthKey );
		m_PendingCollectionInfoRequests.AddToTail( pRequest );
		m_CollectionInfoQueries.RemoveAll();
	}
}

void CDedicatedServerWorkshopManager::UpdateUGCDownloadRequests( void )
{
	if ( m_PendingFileDownloads.Count() )
	{
		// TODO: Handle timeouts/errors?
		m_PendingFileDownloads[0]->Update();
		if ( m_PendingFileDownloads[0]->IsFinished() )
		{
			OnFileDownloaded( m_PendingFileDownloads[0]->GetFileInfo() );
			delete m_PendingFileDownloads[0];
			m_PendingFileDownloads.Remove( 0 );
		}
	}
	/*
	BUG/TODO: Downloading lots of files at the same time runs out of memory
	in GetHTTPResponseBodyData growing a buffer... Only downloading one at a time for now.
	FOR_EACH_VEC_BACK( m_PendingFileDownloads, i )
	{
		m_PendingFileDownloads[i]->Update();
		if ( m_PendingFileDownloads[i]->IsFinished() )
		{
			delete m_PendingFileDownloads[i];
			m_PendingFileDownloads.Remove( i );
		}
	}
	*/
}

void CDedicatedServerWorkshopManager::QueueDownloadFile( const DedicatedServerUGCFileInfo_t *pFileInfo )
{
	CStreamingUGCDownloader *pDownloader = new CStreamingUGCDownloader;
	pDownloader->StartFileDownload( pFileInfo, 1024*1024 );
	m_PendingFileDownloads.AddToTail( pDownloader );
}

bool CDedicatedServerWorkshopManager::IsFileLatestVersion( const DedicatedServerUGCFileInfo_t* ugcInfo )
{
	// never try to update an official map, they're shipped with the depot
	if ( UGCUtil_IsOfficialMap( ugcInfo->fileId ) )
		return true;

	if ( g_pFullFileSystem->FileExists( ugcInfo->m_szFilePath ) )
	{
		// mtime needs to match the time last updated exactly, as we slam the file time when we download
		// so an earlier time is out of date and a later time may be modified due to file copying. 
		uint32 fileTime = (uint32)g_pFullFileSystem->GetFileTime( ugcInfo->m_szFilePath );
		if ( ugcInfo->m_unTimeLastUpdated == fileTime )
			return true;
	}
	return false;
}

void CDedicatedServerWorkshopManager::RemoveFileInfo( PublishedFileId_t id )
{
	unsigned short idx = m_UGCFileInfos.Find( id );
	if ( idx != m_UGCFileInfos.InvalidIndex() )
	{
		// Cancel any pending download
		FOR_EACH_VEC_BACK( m_PendingFileDownloads, i )
		{
			if ( m_PendingFileDownloads[i]->GetPublishedFileId() == id )
			{
				delete m_PendingFileDownloads[i];
				m_PendingFileDownloads.Remove( i );
			}
		}
		delete m_UGCFileInfos[idx];
		m_UGCFileInfos.RemoveAt( idx );
	}
}

bool CDedicatedServerWorkshopManager::GetMapsMatchingName( const char* szMapName, CUtlVector<const DedicatedServerUGCFileInfo_t*>& outVec ) const
{
	bool bFoundAny = false;
	FOR_EACH_MAP( m_UGCFileInfos, i )
	{
		const DedicatedServerUGCFileInfo_t *pInfo = m_UGCFileInfos[i];
		
		// compare just map name (ignore extension and paths passed in)
		char szBaseQueryMapName[MAX_PATH], szBaseUGCMapName[MAX_PATH];
		V_FileBase( szMapName, szBaseQueryMapName, ARRAYSIZE( szBaseQueryMapName ) );
		V_FileBase( pInfo->m_szFileName, szBaseUGCMapName, ARRAYSIZE( szBaseUGCMapName ) );

		if ( V_strcmp( szBaseQueryMapName, szBaseUGCMapName ) == 0 )
		{
			outVec.AddToTail( pInfo );
			bFoundAny = true;
		}
	}

	return bFoundAny;
}

// Get the maps for which we downloaded UGC information successfully
void CDedicatedServerWorkshopManager::GetWorkshopMasWithValidUgcInformation( CUtlVector<const DedicatedServerUGCFileInfo_t *>& outVec ) const
{
	FOR_EACH_MAP( m_UGCFileInfos, i )
	{
		const DedicatedServerUGCFileInfo_t *pInfo = m_UGCFileInfos[i];
		if ( !pInfo )
			continue;

		outVec.AddToTail( pInfo );
	}
}

// HACK: Using the map's directory to get the published file id... 
PublishedFileId_t CDedicatedServerWorkshopManager::GetUGCMapPublishedFileID( const char* szPathToUGCMap ) const
{
	char tmp[MAX_PATH];
	V_strcpy_safe( tmp, szPathToUGCMap );
	V_FixSlashes( tmp, '/' ); // internal path strings use forward slashes, make sure we compare like that.
	//if ( !V_strncmp( tmp, g_szWorkshopMapBasePath, strlen( g_szWorkshopMapBasePath ) ) )
	if ( V_strstr( tmp, "workshop/" ) )
	{
		V_StripFilename(tmp);
		V_StripTrailingSlash(tmp);
		const char* szDirName = V_GetFileName(tmp);
		return (PublishedFileId_t)V_atoui64(szDirName);
	}
	/*
	char szBspNoExtension[MAX_PATH];
	V_strcpy_safe( szBspNoExtension, szPathToUGCMap );
	V_StripExtension( szBspNoExtension, szBspNoExtension, sizeof( szBspNoExtension ) );

	char szElemPathNoExt[MAX_PATH];
	FOR_EACH_MAP( m_UGCFileInfos, i )
	{
		DedicatedServerUGCFileInfo_t* pElem = m_UGCFileInfos.Element(i);
		
		if ( !V_stricmp( szPathToUGCMap, szBSPPath ) )
		{
			return m_UGCFileInfos.Key(i);
		}
	}
	*/
	return 0;
}

const CUtlVector< PublishedFileId_t > & CDedicatedServerWorkshopManager::GetWorkshopMapList( void ) const
{
	return m_vecWorkshopMapList;
}

// Records all workshop maps we have on disk (may or may not be up to date).
void CDedicatedServerWorkshopManager::NoteWorkshopMapOnDisk( PublishedFileId_t id, const char* szPath )
{
	if ( m_vecWorkshopMapList.Find( id ) == m_vecWorkshopMapList.InvalidIndex() )
	{
		m_vecWorkshopMapList.AddToTail( id );
		int idx = m_mapWorkshopIdsToMapNames.Insert( id );
		m_mapWorkshopIdsToMapNames[idx].Set( szPath );
		V_FixSlashes( m_mapWorkshopIdsToMapNames[idx].Get(), '/' ); // Always refer to map paths with forward slashes internally for consistancy.
	}
}

void CDedicatedServerWorkshopManager::HostWorkshopMap( PublishedFileId_t id )
{
	UpdateFile( id );
	m_desiredHostCollection = id;
}

void CDedicatedServerWorkshopManager::HostWorkshopMapCollection( PublishedFileId_t id )
{
	if ( m_bFoundAuthKey == false )
	{
		Warning( "host_workshop_collection: Web API auth key not found!\n" );
	}

	if ( !m_CollectionInfoQueries.HasElement( id ) )
		m_CollectionInfoQueries.AddToTail( id );

	m_desiredHostCollection = id;
}

// Called each time we get ugc file metadata from steam. Assumes this will get called before OnFileDownloaded.
void CDedicatedServerWorkshopManager::OnFileInfoReceived( const DedicatedServerUGCFileInfo_t *pInfo )
{
	if ( pInfo->m_result != k_EResultOK )
	{
		if ( m_pMapGroupBuilder )
		{
			m_pMapGroupBuilder->RemoveRequiredMap( pInfo->fileId );
		}
		m_vecMapsBeingUpdated.FindAndFastRemove( pInfo->fileId );
	}

	// Host single maps as a collection of one
	if ( pInfo->fileId == m_desiredHostCollection )
	{
		if ( m_pMapGroupBuilder )	
			delete m_pMapGroupBuilder;

		CUtlVector< PublishedFileId_t > vecMapFile;
		vecMapFile.AddToTail( pInfo->fileId );
		m_pMapGroupBuilder = new CWorkshopMapGroupBuilder( pInfo->fileId, vecMapFile );
	}

	if( pInfo->fileId == m_hackCurrentMapInfoCheck )
	{
		m_bCurrentLevelNeedsUpdate = !IsFileLatestVersion( pInfo );
	}
}

void CDedicatedServerWorkshopManager::OnFileInfoRequestFailed( PublishedFileId_t id )
{
	m_vecMapsBeingUpdated.FindAndRemove( id ); // no longer being updated
	if ( m_pMapGroupBuilder )
	{
		m_pMapGroupBuilder->RemoveRequiredMap( id );
	}
}

void CDedicatedServerWorkshopManager::OnCollectionInfoReceived( PublishedFileId_t collectionId, const CUtlVector< PublishedFileId_t > & vecCollectionItems )
{
	if ( vecCollectionItems.Count() > 0 )
	{
		PublishedFileId_t curHostedCollection = 0;
		if( g_pGameTypes->IsWorkshopMapGroup( gpGlobals->mapGroupName.ToCStr() ) )
		{
			curHostedCollection = V_atoui64( gpGlobals->mapGroupName.ToCStr() );
		}

		// Make/refresh a mapgroup if it's the one we want to/are hosting.
		if ( collectionId == m_desiredHostCollection || collectionId == curHostedCollection )
		{
			if ( m_pMapGroupBuilder )	
				delete m_pMapGroupBuilder;

			m_pMapGroupBuilder = new CWorkshopMapGroupBuilder( collectionId, vecCollectionItems );
			m_bHostedCollectionUpdatePending = false;
		}
	}
}

void CDedicatedServerWorkshopManager::OnCollectionInfoRequestFailed( PublishedFileId_t id )
{
}

void CDedicatedServerWorkshopManager::OnFileDownloaded( const DedicatedServerUGCFileInfo_t *pInfo )
{
	if ( m_pMapGroupBuilder )
	{
		m_pMapGroupBuilder->OnMapDownloaded( pInfo );
	}

	m_vecMapsBeingUpdated.FindAndFastRemove( pInfo->fileId );
	NoteWorkshopMapOnDisk( pInfo->fileId, pInfo->m_szFilePath );
}

bool CDedicatedServerWorkshopManager::HasPendingMapDownloads( void ) const
{
	if ( !steamgameserverapicontext || !steamgameserverapicontext->SteamHTTP() || !steamgameserverapicontext->SteamGameServer() || !steamgameserverapicontext->SteamGameServer()->BLoggedOn() || (engine->GetGameServerSteamID() && engine->GetGameServerSteamID()->GetEAccountType() == k_EAccountTypeInvalid) )
		return false;
	
	// TODO: Timeouts, errors 

	// If we have maps in the list waiting on info/downloads, or if we are trying to get the lastest info on our hosted collection
	return ( m_vecMapsBeingUpdated.Count() != 0 || m_bHostedCollectionUpdatePending );
}

void CDedicatedServerWorkshopManager::UpdateFile( PublishedFileId_t id )
{
	if ( !m_FileInfoQueries.HasElement( id ) )
		m_FileInfoQueries.AddToTail( id );

	if ( !m_vecMapsBeingUpdated.HasElement( id ) )
		m_vecMapsBeingUpdated.AddToTail( id );
}

void CDedicatedServerWorkshopManager::UpdateFiles( const CUtlVector<PublishedFileId_t>& vecFileIDs )
{
	FOR_EACH_VEC( vecFileIDs, i )
	{
		UpdateFile( vecFileIDs[i] );
	}
}

bool CDedicatedServerWorkshopManager::CurrentLevelNeedsUpdate( void ) const
{
	return m_bCurrentLevelNeedsUpdate;
}

void CDedicatedServerWorkshopManager::CheckForNewVersion( PublishedFileId_t id )
{
	bool bUpdateWorkshopCollectionToo = true;
	if ( m_fTimeLastVersionCheck != 0 && ( Plat_FloatTime() - m_fTimeLastVersionCheck < sv_ugc_manager_max_new_file_check_interval_secs.GetFloat() ) )
	{
		// If we don't have the map then we have to actually go and download it regardless of the timeout
		MapFileIdToUgcFileInfo_t::IndexType_t idxFileInfo = m_UGCFileInfos.Find( id );
		if ( ( idxFileInfo != m_UGCFileInfos.InvalidIndex() ) && m_UGCFileInfos.Element( idxFileInfo ) )
		{
			if ( sv_debug_ugc_downloads.GetBool() )
				Msg( "Skipping new version check for file id %llu, next check in %.2f seconds\n", id,  sv_ugc_manager_max_new_file_check_interval_secs.GetFloat() - (Plat_FloatTime() - m_fTimeLastVersionCheck) );
			return;
		}

		// We have recently checked the contents of workshop collection,
		// this time we are downloading some other map that users want
		// to play, so don't check collection
		bUpdateWorkshopCollectionToo = false;
	}

	UpdateFile( id );

	if ( !bUpdateWorkshopCollectionToo )
		return;

	// Remember last time we did full update
	m_fTimeLastVersionCheck = Plat_FloatTime();

	// check if the map collection changed
	if ( g_pGameTypes->IsWorkshopMapGroup( gpGlobals->mapGroupName.ToCStr() ) )
	{
		PublishedFileId_t hostedCollectionID = V_atoui64( gpGlobals->mapGroupName.ToCStr() );

		// If collection's id is the map's id, then this is a mapgroup of one... no need to check for collection changes.
		if ( hostedCollectionID != id )
		{
			if ( !m_CollectionInfoQueries.HasElement( hostedCollectionID ) )
				m_CollectionInfoQueries.AddToTail( hostedCollectionID );

			m_bHostedCollectionUpdatePending = true;
		}
	}
}

const char* CDedicatedServerWorkshopManager::GetUGCMapPath( PublishedFileId_t id ) const
{
	int idx = m_mapWorkshopIdsToMapNames.Find( id );
	if ( idx != m_mapWorkshopIdsToMapNames.InvalidIndex() )
	{
		return m_mapWorkshopIdsToMapNames[idx];
	}
	else
	{
		return NULL;
	}
}

PublishedFileId_t CDedicatedServerWorkshopManager::ParseCollectionInfo( KeyValues * pDetails )
{
	int collection_detail_result = pDetails->GetInt( "result", 0 );
	EResult hResult = (EResult)collection_detail_result;
	KeyValues *pChildren = pDetails->FindKey( "children" );
	PublishedFileId_t ret = 0;
	if ( hResult == k_EResultOK && pChildren )
	{
		PublishedFileId_t collectionId = pDetails->GetUint64( "publishedfileid", 0 );
		if ( sv_debug_ugc_downloads.GetBool() )
		{
			Msg( "Received info for collection id %llu:\n", collectionId );
		}

		CUtlVector<PublishedFileId_t> vecCollectionIDs;
		for ( KeyValues *pFile = pChildren->GetFirstSubKey(); pFile != NULL; pFile = pFile->GetNextKey() )
		{
			PublishedFileId_t id = pFile->GetUint64( "publishedfileid" );
			vecCollectionIDs.AddToTail( id );
			if ( sv_debug_ugc_downloads.GetBool() )
				Msg( "    file ID: %llu\n", id );
		}

		if ( ShouldUpdateCollection( collectionId, vecCollectionIDs ) )
		{
			UpdateFiles( vecCollectionIDs );
			OnCollectionInfoReceived( collectionId, vecCollectionIDs );
		}

		ret = collectionId;
	}

	return ret;
}


CStreamingUGCDownloader::CStreamingUGCDownloader():m_fileBuffer( 1024*1024, 1024*1024, 0 )
{
	m_ioAsyncControl = NULL;
	m_unChunkSize = 0;
	m_unBytesReceived = 0;
	m_unFileSizeInBytes = 0;
	m_pFileInfo = NULL;
	m_bIsFinished = false;
	m_bHTTPRequestPending = false;
	m_flTimeLastMessage = 0.0f;
}

void CStreamingUGCDownloader::Cleanup( void )
{
	if ( m_ioAsyncControl )
	{
		filesystem->AsyncAbort( m_ioAsyncControl );
		filesystem->AsyncFinish( m_ioAsyncControl, true );
		filesystem->AsyncRelease( m_ioAsyncControl );
		m_ioAsyncControl = NULL;
	}
	m_fileBuffer.Clear();

	if ( filesystem->FileExists( m_szTempFileName ) )
	{
		if ( sv_debug_ugc_downloads.GetBool() )
			Msg( "Clearing temp file(%s) for %llu : %s\n", m_szTempFileName, m_pFileInfo->fileId, m_pFileInfo->m_szFileName );
		filesystem->RemoveFile( m_szTempFileName );
	}

	if ( steamgameserverapicontext )
	{
		ISteamHTTP *pHTTP = steamgameserverapicontext->SteamHTTP();
		if ( pHTTP && m_bHTTPRequestPending )
		{
			if ( sv_debug_ugc_downloads.GetBool() )
				Msg( "Canceling download for %llu : %s\n", m_pFileInfo->fileId, m_pFileInfo->m_szFileName );
			pHTTP->ReleaseHTTPRequest( m_hReq );
		}
	}

	m_httpRequestCallback.Cancel();
}

CStreamingUGCDownloader::~CStreamingUGCDownloader()
{
	Cleanup();
}

void CStreamingUGCDownloader::StartFileDownload( const DedicatedServerUGCFileInfo_t *pFileInfo, uint32 unChunkSize )
{
	m_bIsFinished = false;
	V_snprintf( m_szTempFileName, ARRAYSIZE( m_szTempFileName ), "%s/%llu/%llu.tmp", g_szWorkshopMapBasePath, pFileInfo->fileId, pFileInfo->fileId );

	// Make sure target directory exists
	char buf[MAX_PATH];
	V_ExtractFilePath( m_szTempFileName, buf, sizeof( buf ) );
	g_pFullFileSystem->CreateDirHierarchy( buf, "DEFAULT_WRITE_PATH" );

	if ( filesystem->FileExists( m_szTempFileName, "MOD" ) )
	{
		filesystem->RemoveFile(m_szTempFileName, "MOD" );
	}

	m_pFileInfo = pFileInfo;
	m_unBytesReceived = 0;
	m_unFileSizeInBytes = pFileInfo->m_unFileSizeInBytes;

	m_unChunkSize = unChunkSize;
	m_fileBuffer.EnsureCapacity( m_unChunkSize );

	// Doing one download at a time-- don't start requesting content until it's this downloader's turn.
	// HTTPRequestPartialContent( 0, unChunkSize );


	V_strcpy_safe( m_szMapTitle, pFileInfo->m_szTitle );

	if ( sv_debug_ugc_downloads.GetBool() )
	{
		Msg( "Starting download for file id %llu:'%s'.\n", pFileInfo->fileId, pFileInfo->m_szTitle ? pFileInfo->m_szTitle : "<no title>" );
	}

	if ( sv_broadcast_ugc_downloads.GetBool() )
	{
		UTIL_ClientPrintAll( HUD_PRINTTALK, CFmtStr( "Server: Downloading new map '%s', please wait...", pFileInfo->m_szTitle ) );
		m_flTimeLastMessage = gpGlobals->curtime;
	}
}

void CStreamingUGCDownloader::HTTPRequestPartialContent( uint32 rangeStart, uint32 rangeEnd )
{
	Assert( steamgameserverapicontext );
	if ( steamgameserverapicontext == NULL )
		return;

	ISteamHTTP *pHTTP = steamgameserverapicontext->SteamHTTP();
	Assert( pHTTP );
	if ( !pHTTP ) 
		return;

	m_hReq = pHTTP->CreateHTTPRequest( k_EHTTPMethodGET, m_pFileInfo->m_szUrl );
	SteamAPICall_t hCall;
	CFmtStr byteRange( "bytes=%d-%d", rangeStart, rangeEnd );
	pHTTP->SetHTTPRequestHeaderValue( m_hReq, "range", byteRange.Access() );
	pHTTP->SendHTTPRequest( m_hReq, &hCall );
	m_httpRequestCallback.SetGameserverFlag();
	m_httpRequestCallback.Set( hCall, this, &CStreamingUGCDownloader::OnHTTPRequestComplete );
	m_bHTTPRequestPending = true;

	if ( sv_broadcast_ugc_downloads.GetBool() && gpGlobals->curtime - m_flTimeLastMessage > sv_broadcast_ugc_download_progress_interval.GetFloat() )
	{
		UTIL_ClientPrintAll( HUD_PRINTTALK, CFmtStr( "Server: %.0f%% downloaded for '%s'...", ((float)rangeStart / (float)m_unFileSizeInBytes) * 100.0f, m_szMapTitle ) );
		m_flTimeLastMessage = gpGlobals->curtime;
	}
}

void CStreamingUGCDownloader::OnHTTPRequestComplete( HTTPRequestCompleted_t *arg, bool bFailed )
{
	Assert( steamgameserverapicontext );
	if ( steamgameserverapicontext == NULL )
		return;

	ISteamHTTP *pHTTP = steamgameserverapicontext->SteamHTTP();
	Assert( pHTTP );
	if ( !pHTTP ) 
		return;

	Assert( arg );
	if ( !arg )
		return;

	if ( arg->m_eStatusCode == k_EHTTPStatusCode206PartialContent )
	{
		uint32 unBodySize;
		if ( pHTTP->GetHTTPResponseBodySize( arg->m_hRequest, &unBodySize ) )
		{
			if ( sv_debug_ugc_downloads.GetBool() )
				Msg( "Receiving bytes %u-%u for file %s (%s)\n", m_unBytesReceived, m_unBytesReceived + unBodySize, m_szTempFileName, m_pFileInfo->m_szFileName );

			m_unBytesReceived += unBodySize;
			m_fileBuffer.EnsureCapacity( unBodySize );
			m_fileBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, unBodySize );
			if ( pHTTP->GetHTTPResponseBodyData( arg->m_hRequest, (uint8*)m_fileBuffer.Base(), unBodySize ) )
			{
				filesystem->AsyncAppend( m_szTempFileName, (void*)m_fileBuffer.Base(), unBodySize, false, &m_ioAsyncControl );
			}
		}

		// todo FAIL-- abort 
	}
	pHTTP->ReleaseHTTPRequest( arg->m_hRequest );
	m_bHTTPRequestPending = false;
}


void CStreamingUGCDownloader::Update( void )
{
	Assert( steamgameserverapicontext );
	if ( steamgameserverapicontext == NULL )
		return;

	ISteamHTTP *pHTTP = steamgameserverapicontext->SteamHTTP();
	Assert( pHTTP );
	if ( !pHTTP ) 
		return;

	// Free to ask for more content if async write is done, or if we haven't started writing yet.
	bool bDoneWriting =  m_ioAsyncControl == NULL || filesystem->AsyncStatus( m_ioAsyncControl ) == FSASYNC_OK;
	if ( bDoneWriting == true && m_bHTTPRequestPending == false )
	{
		if ( m_unBytesReceived <  m_unFileSizeInBytes ) 
		{
			HTTPRequestPartialContent( m_unBytesReceived, MIN( m_unBytesReceived + m_unChunkSize, m_unFileSizeInBytes ) - 1 );
		}
		else
		{
			// remove the older file if it exists
			//BUG: If we're running this map, the copy will fail. Defer in that case.
			if ( filesystem->FileExists( m_pFileInfo->m_szFilePath ) )
			{
				filesystem->RemoveFile( m_pFileInfo->m_szFilePath );
			}

			// If authors rename the map file, old versions get orphaned in the workshop directory. Nuke any bsp here.
			if ( sv_remove_old_ugc_downloads.GetBool() )
			{
				CUtlVector<CUtlString> outList;
				AddFilesToList( outList, CFmtStr( "%s/%llu/", g_szWorkshopMapBasePath, m_pFileInfo->fileId ).Access(), "MOD", "bsp" );
				FOR_EACH_VEC( outList, i )
				{
					filesystem->RemoveFile( outList[i] );
				}
			}

			char szFullPathToTempFile[MAX_PATH];
			g_pFullFileSystem->RelativePathToFullPath( m_szTempFileName, "MOD", szFullPathToTempFile, sizeof( szFullPathToTempFile ) );
			if ( UnzipFile( szFullPathToTempFile ) == false )
			{
				// Not a zip file, just rename it
				g_pFullFileSystem->RenameFile( m_szTempFileName, m_pFileInfo->m_szFilePath );
			}

			//
			// Timestamp the file to match workshop updated timestamp
			//
			UGCUtil_TimestampFile( m_pFileInfo->m_szFilePath, m_pFileInfo->m_unTimeLastUpdated );

			m_bIsFinished = true;
			if ( 1 )//sv_debug_ugc_downloads.GetBool() )
			{
				Msg( "Download finished for %llu:'%s'. Moving %s to %s.\n", m_pFileInfo->fileId, m_pFileInfo->m_szTitle ? m_pFileInfo->m_szTitle : "<no title>", m_szTempFileName, m_pFileInfo->m_szFilePath );
			}
		}
	}
}

void CWorkshopMapGroupBuilder::OnMapDownloaded( const DedicatedServerUGCFileInfo_t *pInfo )
{
	MapOnDisk( pInfo->fileId, pInfo->m_szFilePath );
}

CWorkshopMapGroupBuilder::CWorkshopMapGroupBuilder( PublishedFileId_t id, const CUtlVector< PublishedFileId_t >& mapFileIDs )
{
	m_id = id;
	m_pendingMapInfos.AddVectorToTail( mapFileIDs );
	FOR_EACH_VEC( m_pendingMapInfos, i )
	{
		const char* szPath = DedicatedServerWorkshop().GetUGCMapPath( m_pendingMapInfos[i] );
		if ( szPath )
		{
			MapOnDisk( m_pendingMapInfos[i], szPath );
		}
	}
}

void CWorkshopMapGroupBuilder::CreateOrUpdateMapGroup( void )
{
	g_pGameTypes->CreateOrUpdateWorkshopMapGroup( CFmtStr( "%llu", m_id ).Access(), m_Maps );
}

const char* CWorkshopMapGroupBuilder::GetFirstMap( void ) const
{
	return m_Maps.Count() > 0 ? m_Maps.Head() : NULL;
}

const char* CWorkshopMapGroupBuilder::GetMapMatchingId( PublishedFileId_t id ) const
{
	FOR_EACH_VEC( m_Maps, i )
	{
		if ( id == GetMapIDFromMapPath( m_Maps[i] ) )
			return m_Maps[i];
	}
	return GetFirstMap();
}

void CWorkshopMapGroupBuilder::RemoveRequiredMap( PublishedFileId_t id )
{
	m_pendingMapInfos.FindAndFastRemove( id );
}

void CWorkshopMapGroupBuilder::MapOnDisk( PublishedFileId_t id, const char* szPath )
{
	int idx = m_pendingMapInfos.Find( id ); 
	if ( idx != m_pendingMapInfos.InvalidIndex() )
	{
		m_pendingMapInfos.FastRemove( idx );

		// Build path to the map file without any extensions and without the 'maps' dir in the path (maps dir is assumed by other systems).
		char szMapPath[MAX_PATH];
		char szInputPath[MAX_PATH];
		V_strcpy_safe( szInputPath, szPath );
		V_FixSlashes( szInputPath, '/' );
		const char* szMapsPrefix = "maps/";
		if ( V_stristr( szInputPath, szMapsPrefix ) == szInputPath )
		{
			V_strcpy_safe( szMapPath, szInputPath + strlen( szMapsPrefix ) );
		}
		V_StripExtension( szMapPath, szMapPath, sizeof( szMapPath ) );

		m_Maps.CopyAndAddToTail( szMapPath ); // CUtlStringList auto purges on destruct
	}
}

CBaseWorkshopHTTPRequest::CBaseWorkshopHTTPRequest( const CUtlVector<PublishedFileId_t> &vecFileIDs )
{
	m_handle = INVALID_HTTPREQUEST_HANDLE;
	m_bFinished = false;

	m_vecItemsQueried.AddVectorToTail( vecFileIDs );
}

CBaseWorkshopHTTPRequest::~CBaseWorkshopHTTPRequest()
{
	if ( steamgameserverapicontext )
	{
		ISteamHTTP *pHTTP = steamgameserverapicontext->SteamHTTP();
		if ( pHTTP && m_handle != INVALID_HTTPREQUEST_HANDLE )
		{
			pHTTP->ReleaseHTTPRequest( m_handle );
		}
	}

	m_httpCallback.Cancel();
}

void CBaseWorkshopHTTPRequest::OnHTTPRequestComplete( HTTPRequestCompleted_t *arg, bool bFailed ) 
{	
	m_bFinished = true;
	m_lastHTTPResult = arg->m_eStatusCode;

	ISteamHTTP *pHTTP = steamgameserverapicontext->SteamHTTP();

	if ( arg->m_bRequestSuccessful == false )
	{
		Warning( "Server UGC Manager: Failed to get file info. Internal IHTTP error or clientside internet connection problem." );
	}
	else if ( arg->m_eStatusCode != k_EHTTPStatusCode200OK || sv_test_steam_connection_failure.GetBool() )
	{
		Warning( "Server UGC Manager: Failed to get file info. HTTP status %d \n", arg->m_eStatusCode );
	}
	else
	{
		uint32 unBodySize;
		if ( !pHTTP->GetHTTPResponseBodySize( arg->m_hRequest, &unBodySize ) )
		{
			Assert( 0 ); 
			Warning( "Server UGC Manager: GetHTTPResponseBodySize failed\n" );
		}
		else
		{
			if ( sv_debug_ugc_downloads.GetBool() )
				Msg( "Fetched %d bytes via HTTP:\n", unBodySize );
			if ( unBodySize > 0 )
			{
				CUtlBuffer resBuffer( 0, unBodySize, 0 );
				resBuffer.SetBufferType( true, true );
				resBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, unBodySize );
				pHTTP->GetHTTPResponseBodyData( arg->m_hRequest, (uint8*)resBuffer.Base(), resBuffer.TellPut() );
				KeyValues *pResponseKV = new KeyValues("");
				pResponseKV->UsesEscapeSequences( true );
				KeyValuesAD autodelete( pResponseKV );
				bool bLoadSucessful = pResponseKV->LoadFromBuffer( NULL, resBuffer );

				if ( sv_debug_ugc_downloads.GetBool() )
					KeyValuesDumpAsDevMsg( pResponseKV, 1 );

				if ( !bLoadSucessful )
					Msg( "CDedicatedServerWorkshopManager: Failed to load http result to KV buffer\n" );

				if ( bLoadSucessful )
				{
					ProcessHTTPResponse( pResponseKV );
				}
			}
		}
	}

	pHTTP->ReleaseHTTPRequest( arg->m_hRequest );
}

CPublishedFileInfoHTTPRequest::CPublishedFileInfoHTTPRequest( const CUtlVector<PublishedFileId_t>& vecFileIDs )
	: CBaseWorkshopHTTPRequest( vecFileIDs )
{
}

CPublishedFileInfoHTTPRequest::~CPublishedFileInfoHTTPRequest()
{
	m_vecFileInfos.PurgeAndDeleteElements();
}

HTTPRequestHandle CPublishedFileInfoHTTPRequest::CreateHTTPRequest( const char* szAuthKey ) 
{
	if ( steamgameserverapicontext && steamgameserverapicontext->SteamHTTP() )
	{
		CFmtStr strItemCount( "%d", m_vecItemsQueried.Count() ); 
		const char* szUrl = CFmtStr("%s/Service/PublishedFile/GetDetails/v1/", GetApiBaseUrl()).Access();
		ISteamHTTP *pHTTP = steamgameserverapicontext->SteamHTTP();
		m_handle = pHTTP->CreateHTTPRequest( k_EHTTPMethodGET, szUrl );
		pHTTP->SetHTTPRequestGetOrPostParameter( m_handle, "format", "vdf" );
		FOR_EACH_VEC( m_vecItemsQueried, i )
		{
			CFmtStr entry( "publishedfileids[%d]", i );
			pHTTP->SetHTTPRequestGetOrPostParameter( m_handle, entry.Access(), CFmtStr("%llu", m_vecItemsQueried[i] ).Access() );
		}
		pHTTP->SetHTTPRequestGetOrPostParameter( m_handle, "key", szAuthKey );
		pHTTP->SetHTTPRequestGetOrPostParameter( m_handle, "minimal_details", "1" );
		SteamAPICall_t hCall;
		pHTTP->SendHTTPRequest( m_handle, &hCall);
		m_httpCallback.SetGameserverFlag();
		m_httpCallback.Set( hCall, this, &CBaseWorkshopHTTPRequest::OnHTTPRequestComplete );
	}
	else
	{
		m_bFinished = true;
	}
	return m_handle;
}

void CPublishedFileInfoHTTPRequest::ProcessHTTPResponse( KeyValues *pResponseKV ) 
{
	KeyValues *pPublishedFileDetails = pResponseKV->FindKey( "publishedfiledetails", false );
	if ( pPublishedFileDetails )
	{
		for ( KeyValues *fileDetails = pPublishedFileDetails->GetFirstSubKey(); fileDetails != NULL; fileDetails = fileDetails->GetNextKey() )
		{
			DedicatedServerUGCFileInfo_t * pNewFileInfo = new DedicatedServerUGCFileInfo_t;
			pNewFileInfo->BuildFromKV( fileDetails );
			m_vecFileInfos.AddToTail( pNewFileInfo );
		}
	}
}

CCollectionInfoHTTPRequest::CCollectionInfoHTTPRequest( const CUtlVector<PublishedFileId_t>& vecFileIDs )
	: CBaseWorkshopHTTPRequest( vecFileIDs )
{
	m_pResponseKV = NULL;
}

CCollectionInfoHTTPRequest::~CCollectionInfoHTTPRequest()
{
	if ( m_pResponseKV )
	{
		m_pResponseKV->deleteThis();
		m_pResponseKV = NULL;
	}
}

HTTPRequestHandle CCollectionInfoHTTPRequest::CreateHTTPRequest( const char* szAuthKey /*= NULL */ ) 
{
	if ( steamgameserverapicontext && steamgameserverapicontext->SteamHTTP() )
	{
		CFmtStr strItemCount( "%d", m_vecItemsQueried.Count() ); 
		const char* szUrl = CFmtStr("%s/ISteamRemoteStorage/GetCollectionDetails/v0001/", GetApiBaseUrl()).Access();
		ISteamHTTP *pHTTP = steamgameserverapicontext->SteamHTTP();
		m_handle = pHTTP->CreateHTTPRequest( k_EHTTPMethodPOST, szUrl );
		pHTTP->SetHTTPRequestGetOrPostParameter( m_handle, "format", "vdf" );
		pHTTP->SetHTTPRequestGetOrPostParameter( m_handle, "collectioncount", strItemCount.Access() );
		FOR_EACH_VEC( m_vecItemsQueried, i )
		{
			CFmtStr entry( "publishedfileids[%d]", i );
			pHTTP->SetHTTPRequestGetOrPostParameter( m_handle, entry.Access(), CFmtStr("%llu", m_vecItemsQueried[i] ).Access() );
		}
		pHTTP->SetHTTPRequestGetOrPostParameter( m_handle, "key", szAuthKey );
		SteamAPICall_t hCall;
		pHTTP->SendHTTPRequest( m_handle, &hCall);
		m_httpCallback.SetGameserverFlag();
		m_httpCallback.Set( hCall, this, &CBaseWorkshopHTTPRequest::OnHTTPRequestComplete );
	}
	else
	{
		m_bFinished = true;
	}
	
	return m_handle;
}

void CCollectionInfoHTTPRequest::ProcessHTTPResponse( KeyValues *pResponseKV )
{
	KeyValues *pCollectionDetails = pResponseKV->FindKey( "collectiondetails" );
	if ( pCollectionDetails )
	{	
		Assert( m_pResponseKV == NULL );
		if ( m_pResponseKV )
			m_pResponseKV->deleteThis();

		m_pResponseKV = pCollectionDetails->MakeCopy();
	}
	else
	{
		Assert( 0 );
		Msg( "CCollectionInfoHTTPRequest: Could not parse response for collection info\n" );
	}
}

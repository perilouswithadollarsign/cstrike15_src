//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Manager for handling UGC file information requests
//
//========================================================================//

#include "cbase.h"
#include "ugc_file_info_manager.h"

#if !defined (NO_STEAM) && !defined ( _PS3 )

CWorkshopFileInfoManager::CWorkshopFileInfoManager( IWorkshopFileInfoManagerCallbackInterface *pCallbackInterface ) :
	m_bActiveVotingRequest( false ),
	m_pActivePublishedFileRequest( NULL ),
	m_pCallbackInterface( pCallbackInterface )
{
	m_mapPublishedFileInfoDepot.SetLessFunc( DefLessFunc( PublishedFileId_t ) );
}

CWorkshopFileInfoManager::~CWorkshopFileInfoManager( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Drive our requests forward
//-----------------------------------------------------------------------------
void CWorkshopFileInfoManager::Update( void )
{
	UpdatePublishedFileInfoQueries();
	UpdatePublishedFileVotingInfoQueries();
}

//-----------------------------------------------------------------------------
// Purpose: Adds a published file to the query 
// Return:	If true, the published file already exists and the data can be immediately queried for. Otherwise, the caller will need to wait
//-----------------------------------------------------------------------------
bool CWorkshopFileInfoManager::AddFileInfoQuery( CBasePublishedFileRequest *pRequest, bool bAllowUpdate /*= false*/ )
{
#ifndef NO_STEAM
	Log_Msg( LOG_WORKSHOP, "[BaseModPanel] Added query for published file %llu\n", pRequest->GetTargetID() );

	// If they don't want redundant calls in the hopper, early out
	if ( bAllowUpdate == false && m_mapPublishedFileInfoDepot.Find( pRequest->GetTargetID() ) != m_mapPublishedFileInfoDepot.InvalidIndex() )
	{
		// FIXME: ICK!
		delete pRequest;
		return true;
	}

	// FIXME: How do we ensure we're not colliding with data already present?
	m_vecPublishedFileInfoQueryList.Insert( pRequest );
#endif
	
	return false;
}

#if !defined( _GAMECONSOLE )

//-----------------------------------------------------------------------------
// Purpose: Callbacks for retrieving information about a published file
//-----------------------------------------------------------------------------
void CWorkshopFileInfoManager::Steam_OnGetPublishedFileDetails( RemoteStorageGetPublishedFileDetailsResult_t *pResult, bool bError )
{
	if ( bError || pResult->m_eResult != k_EResultOK )
	{
		// This file may have been revoked from the network, don't display it
		if ( pResult->m_eResult == k_EResultFileNotFound )
		{
			// FIXME: Do file clean-up at this point!
			Log_Msg( LOG_WORKSHOP, "[BaseModPanel] Error: Failed to retrieve data for %llu (File Not Found)\n", m_pActivePublishedFileRequest->GetTargetID() );
		}
		else
		{
			Log_Msg( LOG_WORKSHOP, "[BaseModPanel] Error: Failed to retrieve data for %llu (Error: %d)\n", m_pActivePublishedFileRequest->GetTargetID(), pResult->m_eResult );
		}

		// Clean up and move forward
		m_pActivePublishedFileRequest->OnError( pResult->m_eResult );
		
		if ( m_pCallbackInterface )
		{
			m_pCallbackInterface->OnFileRequestError( pResult->m_nPublishedFileId );
		}

		delete m_pActivePublishedFileRequest;
		m_pActivePublishedFileRequest = NULL;
		return;
	}

	// If this happens, it means that the request was nuked mid-flight!
	Assert( m_pActivePublishedFileRequest != NULL );
	if ( m_pActivePublishedFileRequest == NULL )
		return;

	// We're going to either update a current copy of this information, or create a new entry
	int nIndex = m_mapPublishedFileInfoDepot.Find( pResult->m_nPublishedFileId );
	if ( nIndex != m_mapPublishedFileInfoDepot.InvalidIndex() )
	{
		Log_Msg( LOG_WORKSHOP, "[BaseModPanel] Updated file information for %llu\n", pResult->m_nPublishedFileId );

		m_mapPublishedFileInfoDepot[nIndex].m_eVisibility = pResult->m_eVisibility;
		m_mapPublishedFileInfoDepot[nIndex].m_hFile = pResult->m_hFile;
		m_mapPublishedFileInfoDepot[nIndex].m_hPreviewFile = pResult->m_hPreviewFile;
		m_mapPublishedFileInfoDepot[nIndex].m_rtimeCreated = pResult->m_rtimeCreated;
		m_mapPublishedFileInfoDepot[nIndex].m_rtimeUpdated = pResult->m_rtimeUpdated;
		m_mapPublishedFileInfoDepot[nIndex].m_ulSteamIDOwner = pResult->m_ulSteamIDOwner;
		m_mapPublishedFileInfoDepot[nIndex].m_bTagsTruncated = pResult->m_bTagsTruncated;
		memcpy( m_mapPublishedFileInfoDepot[nIndex].m_rgchTitle, pResult->m_rgchTitle, ARRAYSIZE( m_mapPublishedFileInfoDepot[nIndex].m_rgchTitle ) );
		memcpy( m_mapPublishedFileInfoDepot[nIndex].m_rgchDescription, pResult->m_rgchDescription, ARRAYSIZE( m_mapPublishedFileInfoDepot[nIndex].m_rgchDescription ) );
		memcpy( m_mapPublishedFileInfoDepot[nIndex].m_pchFileName, pResult->m_pchFileName, ARRAYSIZE( m_mapPublishedFileInfoDepot[nIndex].m_pchFileName ) );
		memcpy( m_mapPublishedFileInfoDepot[nIndex].m_rgchTags, pResult->m_rgchTags, ARRAYSIZE( m_mapPublishedFileInfoDepot[nIndex].m_rgchTags ) );
	}
	else
	{
		Log_Msg( LOG_WORKSHOP, "[BaseModPanel] Added file information for %llu\n", pResult->m_nPublishedFileId );

		// Package up the info block for insertion into our master depot
		PublishedFileInfo_t info( pResult->m_nPublishedFileId );
		info.m_eVisibility = pResult->m_eVisibility;
		info.m_hFile = pResult->m_hFile;
		info.m_hPreviewFile = pResult->m_hPreviewFile;
		info.m_rtimeCreated = pResult->m_rtimeCreated;
		info.m_rtimeUpdated = pResult->m_rtimeUpdated;
		info.m_ulSteamIDOwner = pResult->m_ulSteamIDOwner;
		info.m_bTagsTruncated = pResult->m_bTagsTruncated;
		memcpy( info.m_rgchTitle, pResult->m_rgchTitle, ARRAYSIZE( info.m_rgchTitle ) );
		memcpy( info.m_rgchDescription, pResult->m_rgchDescription, ARRAYSIZE( info.m_rgchDescription ) );
		memcpy( info.m_pchFileName, pResult->m_pchFileName, ARRAYSIZE( info.m_pchFileName ) );
		memcpy( info.m_rgchTags, pResult->m_rgchTags, ARRAYSIZE( info.m_rgchTags ) );

		// Add it into our master list
		nIndex = m_mapPublishedFileInfoDepot.Insert( pResult->m_nPublishedFileId, info );
	}

	//Store all the tags
	m_mapPublishedFileInfoDepot[nIndex].m_vTags.PurgeAndDeleteElements();
	V_SplitString( m_mapPublishedFileInfoDepot[nIndex].m_rgchTags, ",", m_mapPublishedFileInfoDepot[nIndex].m_vTags );

	// Call our post-load operation
	m_pActivePublishedFileRequest->OnLoaded( m_mapPublishedFileInfoDepot[nIndex] );

	if ( m_pCallbackInterface )
	{
		m_pCallbackInterface->OnFileRequestFinished( pResult->m_nPublishedFileId );
	}

	// FIXME: Can we assure that the request is done at this point?
	delete m_pActivePublishedFileRequest;
	m_pActivePublishedFileRequest = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Update our voting info queries
//-----------------------------------------------------------------------------
void CWorkshopFileInfoManager::Steam_OnGetPublishedItemVoteDetails( RemoteStorageGetPublishedItemVoteDetailsResult_t *pResult, bool bError )
{
	// Make sure this callback was successful
	if ( bError || pResult->m_eResult != k_EResultOK )
	{
		Assert(0);
		return;
	}

	// Take this information into the published file info (if present)
	PublishedFileInfo_t *pInfo = (PublishedFileInfo_t *) GetPublishedFileInfoByID( pResult->m_unPublishedFileId );	
	if ( pInfo == NULL )
	{
		// This means that you've requested voting data for a piece of information we no longer hold
		// The info is going to leak at this point
		Assert( pInfo != NULL );
		return;
	}

	// Update the data
	pInfo->m_bVotingDataValid = true;
	pInfo->m_flVotingScore = pResult->m_fScore;
	pInfo->m_unUpVotes = pResult->m_nVotesFor;
	pInfo->m_unDownVotes = pResult->m_nVotesAgainst;
	pInfo->m_unDownVotes = pResult->m_nReports;

	// Clear our mutex for more requests
	m_bActiveVotingRequest = false;
}

#endif // !_GAMECONSOLE

//-----------------------------------------------------------------------------
// Purpose: Move our information requests forward
//-----------------------------------------------------------------------------
void CWorkshopFileInfoManager::UpdatePublishedFileInfoQueries( void )
{
#ifndef NO_STEAM
#if !defined( _GAMECONSOLE )
	// If we have queries to service and none are in flight, start a new query
	if ( m_vecPublishedFileInfoQueryList.Count() && m_pActivePublishedFileRequest == NULL )
	{
		// Iterate over all our files until we hit one that's valid
		while ( 1 ) 
		{
			CBasePublishedFileRequest *pRequest = m_vecPublishedFileInfoQueryList.RemoveAtHead();

			SteamAPICall_t hSteamAPICall = GetISteamRemoteStorage()->GetPublishedFileDetails( pRequest->GetTargetID(), 0 );
			if ( hSteamAPICall == k_uAPICallInvalid )
			{
				//TODO: Handle the error case
				Log_Msg( LOG_WORKSHOP, "[BaseModPanel] Failed to query for details on published file: %llu\n", pRequest->GetTargetID() );
				continue;
			}

			Log_Msg( LOG_WORKSHOP, "[BaseModPanel] Querying for details of published file: %llu\n", pRequest->GetTargetID() );

			// Query for the details of this file and mark it as the active query
			// NOTE: Only only query may be active at any given time, so these requests are done serially
			m_callbackGetPublishedFileDetails.Set( hSteamAPICall, this, &CWorkshopFileInfoManager::Steam_OnGetPublishedFileDetails );
			m_pActivePublishedFileRequest = pRequest;
			break;
		}
	}
#endif // !_GAMECONSOLE
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Update our voting info queries
//-----------------------------------------------------------------------------
void CWorkshopFileInfoManager::UpdatePublishedFileVotingInfoQueries( void )
{
	// We must have work to do...
	if ( m_vecVotingInfoRequests.Count() == 0 )
		return;

#if !defined( _GAMECONSOLE )
	
	// If we have queries to service and none are in flight, start a new query
	if ( m_bActiveVotingRequest == false ) // FIXME: Need to only let one be active at a time
	{
		// Iterate over all our files until we hit one that's valid
		while ( 1 )
		{
			PublishedFileId_t nTargetID = m_vecVotingInfoRequests.RemoveAtHead();

			SteamAPICall_t hSteamAPICall = GetISteamRemoteStorage()->GetPublishedItemVoteDetails( nTargetID );
			if ( hSteamAPICall == k_uAPICallInvalid )
			{
				//TODO: Handle the error case
				Log_Msg( LOG_WORKSHOP, "[CWorkshopFileInfoManager] Failed to query for voting details on published file: %llu\n", nTargetID );
				continue;
			}

			Log_Msg( LOG_WORKSHOP, "[CWorkshopFileInfoManager] Querying for voting details of published file: %llu\n", nTargetID );

			// Query for the details of this file and mark it as the active query
			// NOTE: Only only query may be active at any given time, so these requests are done serially
			m_callbackGetPublishedItemVoteDetails.Set( hSteamAPICall, this, &CWorkshopFileInfoManager::Steam_OnGetPublishedItemVoteDetails );
			m_bActiveVotingRequest = true;
			break;
		}
	}

#endif // !_GAMECONSOLE
}

//-----------------------------------------------------------------------------
// Purpose: Get published file information by its ID number
//-----------------------------------------------------------------------------
const PublishedFileInfo_t *CWorkshopFileInfoManager::GetPublishedFileInfoByID( PublishedFileId_t nID ) const
{
	int nIndex = m_mapPublishedFileInfoDepot.Find( nID );
	if ( nIndex == m_mapPublishedFileInfoDepot.InvalidIndex() )
		return NULL;

	return &(m_mapPublishedFileInfoDepot[nIndex]);
}

//-----------------------------------------------------------------------------
// Purpose: Requests voting information for a published file
// Return:	If true, the voting data already exists and can be immediately accessed. Otherwise, the caller will need to wait
//-----------------------------------------------------------------------------
bool CWorkshopFileInfoManager::AddFileVoteInfoRequest( const PublishedFileInfo_t *pInfo, bool bForceUpdate /*=false*/ )
{
	Assert( pInfo );
	if ( pInfo == NULL )
		return false;

	// If we're not forcing the update, check our current data first
	if ( bForceUpdate == false )
	{
		// We've already got this data
		if ( pInfo->HasVoteData() )
			return true;
	}

#if !defined( _GAMECONSOLE )	
		m_vecVotingInfoRequests.Insert( pInfo->m_nPublishedFileId );
#endif

	Log_Msg( LOG_WORKSHOP, "[BaseModPanel] Added voting info query for %llu\n", pInfo->m_nPublishedFileId );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Remove a published file's information from our depot
//-----------------------------------------------------------------------------
bool CWorkshopFileInfoManager::RemovePublishedFileInfo( PublishedFileId_t nID )
{
	// Nuke this from our mapping
	bool bFound = m_mapPublishedFileInfoDepot.Remove( nID );

	// Delete it from our pending queries
	if ( m_vecPublishedFileInfoQueryList.Count() )
	{
		// FIXME: This is an abomination -- JDW

		// Move through the queue looking for our ID to delete
		CUtlQueue< CBasePublishedFileRequest * > tempQueue;
		CBasePublishedFileRequest *pRequest = NULL;
		while ( ( pRequest = m_vecPublishedFileInfoQueryList.RemoveAtHead() ) != NULL )
		{
			if ( pRequest->GetTargetID() == nID )
			{
				// Nuke our target
				delete pRequest;
				bFound = true;
				continue;
			}
			
			tempQueue.Insert( pRequest );
		}

		// Clear our current queue (should be done already)
		m_vecPublishedFileInfoQueryList.RemoveAll();
		
		// Now add them all back -- and we cry
		pRequest = NULL;
		while ( ( pRequest = tempQueue.RemoveAtHead() ) != NULL )
		{
			m_vecPublishedFileInfoQueryList.Insert( pRequest );
		}
	}

	return bFound;
}


// Check if the given id is in the list of info queries we haven't yet sent off
bool CWorkshopFileInfoManager::IsInfoRequestStillPending( PublishedFileId_t id ) const
{
	// currently sent? 
	if ( m_pActivePublishedFileRequest && m_pActivePublishedFileRequest->GetTargetID() == id )
		return true; 

	// not yet sent?  
	for ( int i = 0; i < m_vecPublishedFileInfoQueryList.Count(); ++i )
	{
		CBasePublishedFileRequest* pReq = m_vecPublishedFileInfoQueryList[i];
		if ( pReq && pReq->GetTargetID() == id )
		{
			return true;
		}
	}

	return false;
}

#endif // !NO_STEAM

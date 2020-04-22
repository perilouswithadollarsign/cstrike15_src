//===== Copyright Valve Corporation, All rights reserved. ======//
//
// Helper class to manager clients making requests for published file information and polling the results at a later time 
//
//==============================================================//

#ifndef UGC_FILE_INFO_MANAGER
#define UGC_FILE_INFO_MANAGER

#ifdef _WIN32
#pragma once
#endif

#include "ugc_utils.h"

#if !defined( NO_STEAM ) && !defined( _PS3 )

class IWorkshopFileInfoManagerCallbackInterface
{
public:
	// File requests
	virtual void OnFileRequestFinished( UGCHandle_t hFileHandle ) = 0;
	virtual void OnFileRequestError( UGCHandle_t hFileHandle ) = 0;
};

//
// Holds information about published files on the Workshop
//
class CWorkshopFileInfoManager;

struct PublishedFileInfo_t
{
	PublishedFileInfo_t( PublishedFileId_t nID = 0 ) :
		m_nPublishedFileId( nID ),
		m_hFile( k_UGCHandleInvalid ),
		m_hPreviewFile( k_UGCHandleInvalid ),
		m_ulSteamIDOwner( 0 ),
		m_rtimeCreated( 0 ),
		m_rtimeUpdated( 0 ),
		m_eVisibility( k_ERemoteStoragePublishedFileVisibilityPublic ),
		m_rtimeSubscribed( 0 ),
		m_rtimeLastPlayed( 0 ),
		m_rtimeCompleted( 0 ),
		m_unUpVotes( 0 ),
		m_unDownVotes( 0 ),
		m_flVotingScore( 0.0f ),
		m_bVotingDataValid( false ),
		m_unNumReports( 0 )
	{
		memset( m_rgchTitle, 0, ARRAYSIZE( m_rgchTitle ) );
		memset( m_rgchDescription, 0, ARRAYSIZE( m_rgchDescription ) );
		memset( m_pchFileName, 0, ARRAYSIZE( m_pchFileName ) );
	}

	friend class CWorkshopFileInfoManager;

	// Basic info
	PublishedFileId_t m_nPublishedFileId;
	char m_rgchTitle[k_cchPublishedDocumentTitleMax];
	UGCHandle_t m_hFile;
	UGCHandle_t m_hPreviewFile;
	uint64 m_ulSteamIDOwner;
	uint32 m_rtimeCreated;
	uint32 m_rtimeUpdated;
	ERemoteStoragePublishedFileVisibility m_eVisibility;
	uint32 m_rtimeSubscribed;
	uint32 m_rtimeLastPlayed;
	uint32 m_rtimeCompleted;
	char m_rgchDescription[k_cchPublishedDocumentDescriptionMax];
	char m_pchFileName[k_cchFilenameMax];
	char m_rgchTags[k_cchTagListMax];
	bool m_bTagsTruncated;
	CCopyableUtlVector<char *> m_vTags;

	// Whether or not the voting data has been properly collected for this item
	bool HasVoteData( void ) const { return m_bVotingDataValid; }

	// Get the vote information for this item
	// NOTE: Because this isn't native to the published file, it must be requested separately. 
	//		 This function protects from the case where the data is not valid when requested (returns false)
	bool GetVoteData( float *pScore, uint32 *pUpVotes, uint32 *pDownVotes ) const 
	{
		// We must have real data here to allow a query for it
		if ( HasVoteData() == false )
			return false;

		if ( pScore != NULL )
		{
			*pScore = m_flVotingScore;
		}

		if ( pUpVotes != NULL )
		{
			*pUpVotes = m_unUpVotes;
		}

		if ( pDownVotes != NULL )
		{
			*pDownVotes = m_unDownVotes;
		}

		return true;
	}

	bool HasTag( const char *pszTag ) const
	{
		for ( int i = 0; i < m_vTags.Count(); ++i )
		{
			if ( !V_stricmp( m_vTags[i], pszTag ) )
			{
				return true;
			}
		}

		return false;
	}

	private:

	// Voting info
	bool	m_bVotingDataValid;	// If this isn't true, then no request has been done to retrieve it
	uint32	m_unUpVotes;
	uint32  m_unDownVotes;
	float	m_flVotingScore;
	uint32	m_unNumReports;		// FIXME: This isn't exposed anywhere, currently

};

//
// Class which packages up info requests and how to deal with errors and completion
// FIXME: May be easier to switch some of these over to callback instead of this system
//

class CBasePublishedFileRequest
{
public:
	CBasePublishedFileRequest( PublishedFileId_t targetID ) :
	  m_nTargetID( targetID )
	  {}

	  virtual void OnError( EResult nErrorCode ) { /* Do nothing */ }
	  virtual void OnLoaded( PublishedFileInfo_t &info ) { /* Do nothing */ }

	  PublishedFileId_t	GetTargetID( void ) const { return m_nTargetID; }

private:
	PublishedFileId_t	m_nTargetID;			// File ID to get information about
};

//
//
// Workshop file info manager
//
//

class CWorkshopFileInfoManager
{
public:
	CWorkshopFileInfoManager( IWorkshopFileInfoManagerCallbackInterface *pCallbackInterface );
	~CWorkshopFileInfoManager( void );

	// Add a query for a published file's information
	bool AddFileInfoQuery( CBasePublishedFileRequest *pRequest, bool bAllowUpdate = false );
	bool AddFileVoteInfoRequest( const PublishedFileInfo_t *pInfo, bool bForceUpdate = false );
	
	bool RemovePublishedFileInfo( PublishedFileId_t nID );

	void Update( void );

	const PublishedFileInfo_t *GetPublishedFileInfoByID( PublishedFileId_t nID ) const;

	// Check if an info request hasn't been sent off yet
	bool IsInfoRequestStillPending( PublishedFileId_t id ) const;

private:

	void UpdatePublishedFileInfoQueries( void );
	void UpdatePublishedFileVotingInfoQueries( void );

#if !defined( _GAMECONSOLE )
	CCallResult<CWorkshopFileInfoManager, RemoteStorageGetPublishedFileDetailsResult_t> m_callbackGetPublishedFileDetails;
	void Steam_OnGetPublishedFileDetails( RemoteStorageGetPublishedFileDetailsResult_t *pResult, bool bError );

	CCallResult<CWorkshopFileInfoManager, RemoteStorageGetPublishedItemVoteDetailsResult_t> m_callbackGetPublishedItemVoteDetails;
	void Steam_OnGetPublishedItemVoteDetails( RemoteStorageGetPublishedItemVoteDetailsResult_t *pResult, bool bError );
#endif // !_GAMECONSOLE

	CUtlMap< PublishedFileId_t, PublishedFileInfo_t >	m_mapPublishedFileInfoDepot;			// Master list of all published file information we've queried for
	CUtlQueue< CBasePublishedFileRequest * >			m_vecPublishedFileInfoQueryList;		// List of file IDs that need to be queried for information
	CBasePublishedFileRequest							*m_pActivePublishedFileRequest;			// Currently active request query (used as a mutex)
	CUtlQueue< PublishedFileId_t >						m_vecVotingInfoRequests;				// List of published file IDs we'd like voting information on
	bool												m_bActiveVotingRequest;					// Whether or not there's a voting request already pending
	IWorkshopFileInfoManagerCallbackInterface			*m_pCallbackInterface;					// User-supplied callback interface
};

#endif // !NO_STEAM

#endif // UGC_FILE_INFO_MANAGER
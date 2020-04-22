//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"

#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if !defined( NO_STEAM ) && !defined( SWDS )

CInterlockedInt g_numSteamLeaderboardWriters;


class CSteamLeaderboardWriter
{
public:
	CSteamLeaderboardWriter( KeyValues *pViewDescription, KeyValues *pViewData );
	~CSteamLeaderboardWriter();

protected:
	void UploadScore(SteamLeaderboard_t leaderboardHandle);

protected:
	CCallResult< CSteamLeaderboardWriter, LeaderboardFindResult_t > m_CallbackOnLeaderboardFindResult;
	void Steam_OnLeaderboardFindResult( LeaderboardFindResult_t *p, bool bError );

	CCallResult< CSteamLeaderboardWriter, LeaderboardScoreUploaded_t > m_CallbackOnLeaderboardScoreUploaded;
	void Steam_OnLeaderboardScoreUploaded( LeaderboardScoreUploaded_t *p, bool bError );

	CCallResult< CSteamLeaderboardWriter, LeaderboardScoresDownloaded_t > m_CallbackOnLeaderboardScoresDownloaded;
	void Steam_OnLeaderboardScoresDownloaded( LeaderboardScoresDownloaded_t *p, bool bError );

protected:
	KeyValues *m_pViewDescription;
	KeyValues *m_pViewData;

	int m_nViewDescriptionPayloadFormatSize;
};

CSteamLeaderboardWriter::CSteamLeaderboardWriter( KeyValues *pViewDescription, KeyValues *pViewData ) :
	m_pViewDescription( pViewDescription->MakeCopy() ),
	m_pViewData( pViewData->MakeCopy() ),
	m_nViewDescriptionPayloadFormatSize( 0 )
{
	SteamAPICall_t hCall;
	
	if ( m_pViewDescription->GetBool( ":nocreate" ) )
		hCall = steamapicontext->SteamUserStats()->FindLeaderboard( m_pViewData->GetName() );
	else
		hCall = steamapicontext->SteamUserStats()->FindOrCreateLeaderboard(
			m_pViewData->GetName(),
			( ELeaderboardSortMethod ) m_pViewDescription->GetInt( ":sort" ),
			( ELeaderboardDisplayType ) m_pViewDescription->GetInt( ":format" ) );

	m_CallbackOnLeaderboardFindResult.Set( hCall, this, &CSteamLeaderboardWriter::Steam_OnLeaderboardFindResult );

	++ g_numSteamLeaderboardWriters;
}

void CSteamLeaderboardWriter::Steam_OnLeaderboardFindResult( LeaderboardFindResult_t *p, bool bError )
{
	if ( bError )
	{
		Warning( "Failed to contact leaderboard server for '%s'\n", m_pViewData->GetName() );
		delete this;
		return;
	}

	if ( !p->m_bLeaderboardFound )
	{
		DevWarning( "Leaderboard '%s' was not found on server\n", m_pViewData->GetName() );
		delete this;
		return;
	}

	// If the view description contains the keyvalue ":sumscore 1" or anything in the ":payloadformat"
	// section containes ":upload sum" then we want to treat the score in the view data as a delta of
	// the data that is currently uploaded to the scoreboard.  So first we have to read the data, then
	// aggregate the values.  Also compute the payload size.
	bool bHasSum = m_pViewDescription->GetBool( ":scoresum" );
	bHasSum = bHasSum || m_pViewDescription->GetString( ":scoreformula", NULL );
	int nPayloadSize = 0;
	
	KeyValues *pPayloadFormat = m_pViewDescription->FindKey( ":payloadformat" );

	if ( pPayloadFormat )
	{
		// Iterate over each payload entry to see if any of them need to be summed.
		for ( int payloadIndex=0; ; ++payloadIndex )
		{
			KeyValues *pPayload = pPayloadFormat->FindKey( CFmtStr( "payload%d", payloadIndex ) );
			if ( !pPayload )
			{
				// No more payload entries specified.
				break;
			}

			const char* pszFormat = pPayload->GetString( ":format", NULL );
			if ( V_stricmp( pszFormat, "int" ) == 0 )
			{
				nPayloadSize += sizeof( int );
			}
			else if ( V_stricmp( pszFormat, "uint64" ) == 0 )
			{
				nPayloadSize += sizeof( uint64 );
			}
			else
			{
				Warning( "Leaderboard description '%s' contains an invalid payload :format '%s'", m_pViewData->GetName(), pPayload->GetName() );
				delete this;
				return;
			}

			const char* pszUpload = pPayload->GetString( ":upload", NULL );
			if ( !pszUpload || V_stricmp( pszUpload, "last" ) == 0 )
			{
				// just overwrite the leaderboard value with our current value
			}
			else if ( V_stricmp( pszUpload, "sum" ) == 0 )
			{
				bHasSum = true;
			}
			else
			{
				Warning( "Leaderboard description '%s' contains an invalid payload :format '%s'", m_pViewData->GetName(), pPayload->GetName() );
				delete this;
				return;
			}
		}

		m_nViewDescriptionPayloadFormatSize = nPayloadSize;
	}

	if ( bHasSum )
	{
		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();

		// We need to download this user's current leaderboard data first.
		DevMsg( "Downloading score for leaderboard '%s', steam id '%llu'...\n", m_pViewData->GetName(), steamID.ConvertToUint64() );
		SteamAPICall_t hCall = steamapicontext->SteamUserStats()->DownloadLeaderboardEntriesForUsers( p->m_hSteamLeaderboard, &steamID, 1 );
		m_CallbackOnLeaderboardScoresDownloaded.Set( hCall, this, &CSteamLeaderboardWriter::Steam_OnLeaderboardScoresDownloaded );
	}
	else
	{
		UploadScore( p->m_hSteamLeaderboard );
	}
}

void CSteamLeaderboardWriter::Steam_OnLeaderboardScoreUploaded( LeaderboardScoreUploaded_t *p, bool bError )
{
	if ( bError )
	{
		Warning( "Failed to upload leaderboard score for '%s'\n", m_pViewData->GetName() );
	}
	else if ( !p->m_bSuccess )
	{
		Warning( "Failed to update leaderboard score for '%s'\n", m_pViewData->GetName() );
	}
	else if ( !p->m_bScoreChanged )
	{
		DevMsg( "Leaderboard score uploaded, but not changed for '%s'\n", m_pViewData->GetName() );
	}
	else
	{
		DevMsg( "Leaderboard score uploaded for '%s', new rank %d, old rank %d\n", m_pViewData->GetName(), p->m_nGlobalRankNew, p->m_nGlobalRankPrevious );
	}

	delete this;
}

void CSteamLeaderboardWriter::Steam_OnLeaderboardScoresDownloaded( LeaderboardScoresDownloaded_t *p, bool bError )
{
	if ( bError )
	{
		Warning( "Failed to download leaderboard score for '%s'\n", m_pViewData->GetName() );
		delete this;
		return;
	}

	// Now that we have the downloaded leaderboard data, we need to extract it, and sum the appropriate values
	// then write the data back out.
	if ( p->m_cEntryCount == 1 )
	{
		DevMsg( "Parsing downloaded scores for '%s'\n", m_pViewData->GetName() );

		// We have the one entry we were looking for, so extract the current data from it.
		LeaderboardEntry_t leaderboardEntry;
		int32 *pPayloadData = new int32[m_nViewDescriptionPayloadFormatSize];
		if ( steamapicontext->SteamUserStats()->GetDownloadedLeaderboardEntry( p->m_hSteamLeaderboardEntries, 0, &leaderboardEntry, pPayloadData, m_nViewDescriptionPayloadFormatSize ) )
		{
			unsigned char *pCurrentPayload = (unsigned char*)pPayloadData;

			// If the ranked score should be summed with the current ranked score from the leaderboard entry,
			// do that now.
			if ( m_pViewDescription->GetBool( ":scoresum" ) )
			{
				const char *pScore = m_pViewDescription->GetString( ":score", NULL );
				m_pViewData->SetInt( pScore, leaderboardEntry.m_nScore + m_pViewData->GetInt( pScore ) );
			}

			// Iterate over payload item and perform the appropriate aggregation method, then write the result
			// to the view data.
			// Iterate over all of the payload format information and extract the corresponding data from the view data.
			for ( int payloadIndex=0; ; ++payloadIndex )
			{
				KeyValues *pPayload = m_pViewDescription->FindKey( CFmtStr( ":payloadformat/payload%d", payloadIndex ) );
				if ( !pPayload )
				{
					// No more payload entries specified.
					break;
				}

				// Get payload format, aggregate it if necessary, then advance the current payload pointer.
				const char *pFormat = pPayload->GetString( ":format", NULL );
				const char *pScore = pPayload->GetString( ":score", NULL );
				const char *pUpload = pPayload->GetString( ":upload", NULL );

				if ( V_stricmp( pFormat, "int" ) == 0 )
				{
					if ( pUpload && V_stricmp( pUpload, "sum" ) == 0 )
					{
						int score = *(int*)pCurrentPayload;
						m_pViewData->SetInt( pScore, score + m_pViewData->GetInt( pScore ) );
					}

					pCurrentPayload += sizeof( int );
				}
				else if ( V_stricmp( pFormat, "uint64" ) == 0 )
				{
					if ( pUpload && V_stricmp( pUpload, "sum" ) == 0 )
					{
						uint64 score = *(uint64*)pCurrentPayload;
						m_pViewData->SetUint64( pScore, score + m_pViewData->GetUint64( pScore ) );
					}

					pCurrentPayload += sizeof( uint64 );
				}
				else
				{
					Warning( "Leaderboard description '%s' contains an invalid payload :format '%s'", m_pViewData->GetName(), pPayload->GetName() );
					delete [] pPayloadData;
					delete this;
					return;
				}
			}

			// Now that we've aggregated all of the data, we need to check to see if the ranking data
			// (the data associated with ":score") needs to have a forumla applied to it.
			const char* pszScoreFormula = m_pViewDescription->GetString( ":scoreformula", NULL );
			if ( pszScoreFormula )
			{
				// Create an expression from the string.
				CExpressionCalculator calc( pszScoreFormula );

				// Set variables that correspond to each payload.
				for ( int payloadIndex=0; ; ++payloadIndex )
				{
					CFmtStr payloadName = CFmtStr( "payload%d", payloadIndex );
					KeyValues *pPayload = m_pViewDescription->FindKey( CFmtStr( ":payloadformat/%s", payloadName.Access() ) );
					if ( !pPayload )
					{
						// No more payload entries specified.
						break;
					}

					// Get payload format and score.
					const char *pFormat = pPayload->GetString( ":format", NULL );
					const char *pScore = pPayload->GetString( ":score", NULL );

					if ( V_stricmp( pFormat, "int" ) == 0 )
					{
						int value = m_pViewData->GetInt( pScore );
						calc.SetVariable( payloadName, value );
					}
					else if ( V_stricmp( pFormat, "uint64" ) == 0 )
					{
						uint64 value = m_pViewData->GetUint64( pScore );
						calc.SetVariable( payloadName, value );
					}
					else
					{
						Warning( "Leaderboard description '%s' contains an invalid payload :format '%s'", m_pViewData->GetName(), pPayload->GetName() );
						delete [] pPayloadData;
						delete this;
						return;
					}
				}

				// Evaluate the expression and apply it to our view data.
				float value = 0.0f;
				if ( calc.Evaluate( value ) )
				{
					m_pViewData->SetInt( m_pViewDescription->GetString( ":score", NULL ), (int)value );
				}
				else
				{
					Warning( "Failed to evaluate leaderboard expression.\n"
							 "\tLeaderboard = %s\n"
							 "\tExpression: %s\n",
							 m_pViewData->GetName(),
							 pszScoreFormula
						   );
					delete [] pPayloadData;
					delete this;
					return;
				}
			}

			delete [] pPayloadData;
		}
		else
		{
			Warning( "Failed to download leaderboard score for '%s'\n", m_pViewData->GetName() );
			delete [] pPayloadData;
			delete this;
			return;
		}
	}

	UploadScore( p->m_hSteamLeaderboard );
}

void CSteamLeaderboardWriter::UploadScore(SteamLeaderboard_t leaderboardHandle)
{
	unsigned char *pvPayloadPtr = (unsigned char *)m_pViewData->GetPtr( ":payloadptr" );
	int nPayloadSize = m_pViewData->GetInt( ":payloadsize" );

	// If the view description contains ":payloadformat", then we need to construct the payload pointer
	// based on that description and the view data.
	KeyValues *pPayloadFormat = m_pViewDescription->FindKey( ":payloadformat" );
	if ( pPayloadFormat )
	{
		if ( pvPayloadPtr )
		{
			Warning( "Leaderboard data and description '%s' contain both :payloadptr and :payloadformat.\n", m_pViewData->GetName() );
			delete this;
			return;
		}

		// Allocate a buffer for the payload.
		if ( m_nViewDescriptionPayloadFormatSize > 0 )
		{
			nPayloadSize = m_nViewDescriptionPayloadFormatSize;
			pvPayloadPtr = new unsigned char[nPayloadSize];

			unsigned char *pvCurPayloadPtr = (unsigned char*)pvPayloadPtr;
			memset( pvCurPayloadPtr, 0, nPayloadSize );

			// Iterate over all of the payload format information and extract the corresponding data from the view data.
			for ( int payloadIndex=0; ; ++payloadIndex )
			{
				KeyValues *pPayload = pPayloadFormat->FindKey( CFmtStr( "payload%d", payloadIndex ) );
				if ( !pPayload )
				{
					// No more payload entries specified.
					break;
				}

				// Add assert to make sure payload pointer is still in bounds.

				// Get the appropriate entry from the view data and write it to our payload buffer.
				const char* pszFormat = pPayload->GetString( ":format", NULL );
				const char* pszScore = pPayload->GetString( ":score", NULL );
				if ( V_stricmp( pszFormat, "int" ) == 0 )
				{
					int score = m_pViewData->GetInt( pszScore );
#if defined( PLAT_BIG_ENDIAN )
					// On big-endian platforms, byteswap our scores so we always write the online data as little endian
					*(int*)pvCurPayloadPtr = DWordSwap( score );
#else
					*(int*)pvCurPayloadPtr = score;
#endif
					pvCurPayloadPtr += sizeof( int );
				}
				else if ( V_stricmp( pszFormat, "uint64" ) == 0 )
				{
					uint64 score = m_pViewData->GetUint64( pszScore );
#if defined( PLAT_BIG_ENDIAN )
					// On big-endian platforms, byteswap our scores so we always write the online data as little endian
					*(int*)pvCurPayloadPtr = QWordSwap( score );
#else
					*(uint64*)pvCurPayloadPtr = score;
#endif
					pvCurPayloadPtr += sizeof( uint64 );
				}
				else
				{
					Warning( "Leaderboard description '%s' contains an invalid payload :format '%s'", m_pViewData->GetName(), pPayload->GetName() );
					delete [] pvPayloadPtr;
					delete this;
					return;
				}
			}

			// Add our constructed payload and the size to the view data.
			m_pViewData->SetPtr( ":payloadptr", pvPayloadPtr );
			m_pViewData->SetInt( ":payloadsize", nPayloadSize );
		}
	}

	//
	// Upload score
	//
	int32 nScore = 0;
	nScore = ( uint32 ) m_pViewData->GetUint64( m_pViewDescription->GetString( ":score" ) );

	DevMsg( "Uploading score for leaderboard '%s'...\n", m_pViewData->GetName() );
	KeyValuesDumpAsDevMsg( m_pViewData, 1 );

	SteamAPICall_t hCall = steamapicontext->SteamUserStats()->UploadLeaderboardScore(
		leaderboardHandle,
		( ELeaderboardUploadScoreMethod ) m_pViewDescription->GetInt( ":upload" ),
		nScore,
		( int32 const *) pvPayloadPtr,
		( nPayloadSize + sizeof( int32 ) - 1 ) / sizeof( int32 ) );
	m_CallbackOnLeaderboardScoreUploaded.Set( hCall, this, &CSteamLeaderboardWriter::Steam_OnLeaderboardScoreUploaded );
}

CSteamLeaderboardWriter::~CSteamLeaderboardWriter()
{
	// We need to delete leaderboard payload allocated by caller
	delete [] ( char * ) m_pViewData->GetPtr( ":payloadptr" );

	if ( m_pViewDescription )
		m_pViewDescription->deleteThis();

	if ( m_pViewData )
		m_pViewData->deleteThis();

	-- g_numSteamLeaderboardWriters;
}

void Steam_WriteLeaderboardData( KeyValues *pViewDescription, KeyValues *pViewData )
{
	MEM_ALLOC_CREDIT();

	// CSteamLeaderboardWriter is driven by Steam callbacks and will
	// delete itself when finished
	new CSteamLeaderboardWriter( pViewDescription, pViewData );
}


#endif

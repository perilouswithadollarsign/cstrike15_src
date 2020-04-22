//==== Copyright © 1996-2005, Valve Corporation, All rights reserved. =========//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "fmtstr.h"

#if defined( INCLUDE_SCALEFORM )

#include "basepanel.h"
#include "leaderboardsdialog_scaleform.h"
#include <vgui/ILocalize.h>

#ifdef _X360
#include "xbox/xbox_launch.h"
#include "ixboxsystem.h"
#include "../../common/xlast_csgo/csgo.spa.h"
#endif


#include "keyvalues.h"
#include "engineinterface.h"
#include "modinfo.h"
#include "gameui_interface.h"

#include "tier1/utlbuffer.h"
#include "filesystem.h"

using namespace vgui;

// for SRC
#include <vstdlib/random.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define LEADERBOARD_SHOW_RANK_UNDER		1000

CCreateLeaderboardsDialogScaleform* CCreateLeaderboardsDialogScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnOk ),
	SFUI_DECL_METHOD( SetQuery ),
	SFUI_DECL_METHOD( Query_NumResults ),
	SFUI_DECL_METHOD( Query_GetCurrentPlayerRow ),
	SFUI_DECL_METHOD( QueryRow_GamerTag ),
	SFUI_DECL_METHOD( QueryRow_ColumnValue ),
	SFUI_DECL_METHOD( QueryRow_ColumnRatio ),
	SFUI_DECL_METHOD( DisplayUserInfo ),
	SFUI_END_GAME_API_DEF( CCreateLeaderboardsDialogScaleform, LeaderBoards )
	;

// For tracking the last panel the user viewed on this screen
ConVar player_last_leaderboards_panel( "player_last_leaderboards_panel", "0",  FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Last opened panel in the Leaderboards screen" );
ConVar player_last_leaderboards_mode( "player_last_leaderboards_mode", "0",  FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Last mode setting in the Leaderboards screen" );
ConVar player_last_leaderboards_filter( "player_last_leaderboards_filter", "0",  FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Last mode setting in the Leaderboards screen" );

const float QUERY_DELAY_TIME = 0.5f;
const float QUERY_SHORT_DELAY_TIME = 0.35f;

// Enable this define for asynch debug output
// #define DIAGNOSE_ASYNCH_PROBLEMS 1

void CCreateLeaderboardsDialogScaleform::LoadDialog( void )
{
	if ( !m_pInstance )
	{
		STEAMWORKS_TESTSECRETALWAYS();

		m_pInstance = new CCreateLeaderboardsDialogScaleform( );
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CCreateLeaderboardsDialogScaleform, m_pInstance, LeaderBoards );
	}
}

void CCreateLeaderboardsDialogScaleform::UnloadDialog( void )
{
	if ( m_pInstance )
	{
		m_pInstance->RemoveFlashElement();
	}
}

void CCreateLeaderboardsDialogScaleform::UpdateDialog( void )
{
	if ( m_pInstance )
	{
		m_pInstance->Tick();
	}
}

CCreateLeaderboardsDialogScaleform::CCreateLeaderboardsDialogScaleform( void ) :
	m_hAsyncQuery(0),
	m_currentFilterType(eLBFilter_Overall),
	m_startingRowIndex(-1),
	m_rowsPerPage(0),
	m_bCheckForQueryResults(false),
	m_iNumFriends(0),
	m_iNextFriend(-1),
	m_bEnumeratingFriends(false),
	m_bResultsValid(false),
	m_iTotalViewRows(0)
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	m_iPlayerSlot = XBX_GetActiveUserId();

	IPlayerLocal *pProfile = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( m_iPlayerSlot );
	if ( pProfile )
		m_PlayerXUID = pProfile->GetXUID();

#if defined (_X360)
	m_pResultsBuffer = NULL;
	m_pFriends = NULL;
	ZeroMemory( m_pFriendsResult, sizeof(m_pFriendsResult[0]) * (MAX_FRIENDS+1) );
#endif // _X360

#if !defined( NO_STEAM )
	m_currentLeaderboardHandle = (SteamLeaderboard_t)0;
	m_currentLeaderboardName = NULL;
	V_memset( &m_cachedLeaderboardScores, 0, sizeof(LeaderboardScoresDownloaded_t) );
	m_pLeaderboardDescription = NULL;
	V_memset( m_payloadSizes, 0, sizeof(int)*kMaxPayloadEntries );

	SetDefLessFunc( m_LeaderboardHandles );
#endif // !NO_STEAM
}

void CCreateLeaderboardsDialogScaleform::OnOk( SCALEFORM_CALLBACK_ARGS_DECL )
{
	CCreateLeaderboardsDialogScaleform::UnloadDialog();
}

void CCreateLeaderboardsDialogScaleform::FlashLoaded( void )
{
	if ( FlashAPIIsValid() )
	{
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "InitDialogData", NULL, 0 );
	}
}

void CCreateLeaderboardsDialogScaleform::FlashReady( void )
{
	SFVALUE PanelHandle = m_pScaleformUI->Value_GetMember( m_FlashAPI, "ScoreBoard" );

	if ( PanelHandle )
	{
		SFVALUE AnimatedPanelHandle = m_pScaleformUI->Value_GetMember( PanelHandle, "Panel" );

		if ( AnimatedPanelHandle )
		{
			// Grab any references you need now...

			m_pScaleformUI->ReleaseValue( AnimatedPanelHandle );
		}

		m_pScaleformUI->ReleaseValue( PanelHandle );
	}

	m_bCheckForQueryResults = false;
	m_bResultsValid = false;
	m_iNumFriends = 0;
	m_iNextFriend = -1;

#if defined (_X360)
	m_pResultsBuffer = NULL;
	m_pFriends = NULL;
	ZeroMemory( m_pFriendsResult, sizeof(m_pFriendsResult[0]) * (MAX_FRIENDS+1) );
#endif // _X360

	Show();
}

bool CCreateLeaderboardsDialogScaleform::PreUnloadFlash( void )
{
	return true;
}

void CCreateLeaderboardsDialogScaleform::PostUnloadFlash( void )
{
#ifdef _X360
	// release the async handle
	if ( m_hAsyncQuery != 0 )
	{
		xboxsystem->CancelOverlappedOperation( m_hAsyncQuery );
		xboxsystem->ReleaseAsyncHandle( m_hAsyncQuery );
	}

	// Wipe out any previous results
	if ( m_pResultsBuffer != NULL )
	{
		delete [] m_pResultsBuffer;
		m_pResultsBuffer = NULL;
	}

	if ( m_pFriends )
	{
		delete [] m_pFriends;
		m_pFriends = NULL;

		// Free the structures that track our friends' statistics
		for ( int idx = 0; idx < MAX_FRIENDS+1; ++idx )
		{
			if ( m_pFriendsResult[idx] != NULL )
			{
				delete [] m_pFriendsResult[idx];
				m_pFriendsResult[idx] = NULL;
			}
		}
	}

#endif // _X360
	
#if !defined( NO_STEAM )
	if ( !m_pLeaderboardDescription )
		m_pLeaderboardDescription->deleteThis();
#endif // !NO_STEAM

	if ( GameUI().IsInLevel() )
	{
		BasePanel()->RestorePauseMenu();
	}
	else
	{
		BasePanel()->RestoreMainMenuScreen();
	}

	m_pInstance = NULL;
	delete this;
}

void CCreateLeaderboardsDialogScaleform::Show( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", 0, NULL );
		}
	}
}

void CCreateLeaderboardsDialogScaleform::Hide( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", 0, NULL );
		}
	}
}

void CCreateLeaderboardsDialogScaleform::Tick( void )
{
	if ( FlashAPIIsValid() )
	{
		QueryUpdate();
		CheckForQueryResults();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called from scaleform whenever any of our search parameters change:
//		game mode & type, filter (me/friends/overall), position in board
//-----------------------------------------------------------------------------
void CCreateLeaderboardsDialogScaleform::SetQuery( SCALEFORM_CALLBACK_ARGS_DECL )
{
#if defined (_X360)

	// As soon as we change any parameters, cancel any previous queries
	if ( m_hAsyncQuery != 0 )
	{
#ifdef	DIAGNOSE_ASYNCH_PROBLEMS
		printf( "Leaderboard: SetQuery: Aborting a pending query.\n" );
#endif

		xboxsystem->CancelOverlappedOperation( m_hAsyncQuery );
		xboxsystem->ReleaseAsyncHandle( m_hAsyncQuery );
		m_hAsyncQuery = 0;
	}
	m_bCheckForQueryResults = false;

	int nIdx = 0;

	int filterType = (int)( pui->Params_GetArgAsNumber( obj, nIdx++ ) );
	m_currentFilterType = (eLeaderboardFiltersType) clamp( filterType, (int)eLBFilter_Overall, (int)eLBFilter_Friends );

	// if this is a new friends query, free all of the tracking structures from previous friends stat queries
	if ( m_currentFilterType == eLBFilter_Friends )
	{
		for ( int idx = 0; idx < MAX_FRIENDS+1; ++idx )
		{
			if ( m_pFriendsResult[idx] != NULL )
			{
				delete [] m_pFriendsResult[idx];
				m_pFriendsResult[idx] = NULL;
			}
		}
	
		// Do not reset m_pFriends list here! It's slow to read out, so once we've gotten it keep
		//		it around until the dialog goes away (not when we're flicking through filters)
		m_bEnumeratingFriends = false;
		m_iNextFriend = -1;
	}

	m_startingRowIndex = (int)( pui->Params_GetArgAsNumber( obj, nIdx++ ) );
	m_rowsPerPage = (int)( pui->Params_GetArgAsNumber( obj, nIdx++ ) );	

	DWORD dwViewId = (DWORD)( pui->Params_GetArgAsNumber( obj, nIdx++ ) );
	DWORD dwNumColumns = (DWORD)( pui->Params_GetArgAsNumber( obj, nIdx++ ) );

	// Sanity check the column list passed in:
	SFVALUE columnIdArray = pui->Params_GetArg( obj, nIdx++ );
	if ( pui->Value_GetType( columnIdArray ) != IScaleformUI::VT_Array )
	{
		Warning( "CCreateLeaderboardsDialogScaleform::SetQuery: Invalid final argument - expected an array of columnIds!\n" );
		dwNumColumns = 0;
	}
	else
	{
		int arrayLen = pui->Value_GetArraySize( columnIdArray );
		if ( arrayLen != (int)dwNumColumns )
		{
			Warning( "CCreateLeaderboardsDialogScaleform::SetQuery: columnIds length differs from param count - using %d columns instead!\n", arrayLen );
			dwNumColumns = arrayLen;
		}
	}
	

	// Wait a second before we handle this query, in case user is flicking through options
	m_fQueryDelayTime = Plat_FloatTime() + QUERY_DELAY_TIME;

	// Sanity check the board we are reading from
	Assert( dwViewId >= STATS_VIEW_WINS_ONLINE_CASUAL && dwViewId <= STATS_VIEW_GP_ONLINE_GG_BOMB );

	// Clear the query, and copy over our search settings
	ZeroMemory( &m_statsSpec, sizeof(m_statsSpec) );

	m_statsSpec.dwViewId = dwViewId;
	m_statsSpec.dwNumColumnIds = clamp( dwNumColumns, (DWORD)0, (DWORD)XUSER_STATS_ATTRS_IN_SPEC - 1 );

	for (DWORD columnIdx = 0; columnIdx < m_statsSpec.dwNumColumnIds; ++columnIdx )
	{
		SFVALUE element = pui->Value_GetArrayElement( columnIdArray, columnIdx );

		m_statsSpec.rgwColumnIds[columnIdx] = (WORD)(pui->Value_GetNumber( element ));
	}

#endif 

#if !defined( NO_STEAM )

	// As soon as we change any parameters, cancel any previous queries	
	m_bCheckForQueryResults = false;

	int nIdx = 0;

	int filterType = (int)( pui->Params_GetArgAsNumber( obj, nIdx++ ) );
	m_currentFilterType = (eLeaderboardFiltersType) clamp( filterType, (int)eLBFilter_Overall, (int)eLBFilter_Friends );
	
	m_startingRowIndex = (int)( pui->Params_GetArgAsNumber( obj, nIdx++ ) );
	m_rowsPerPage = (int)( pui->Params_GetArgAsNumber( obj, nIdx++ ) );	

	m_currentLeaderboardName = ( pui->Params_GetArgAsString( obj, nIdx++ ) );

	// DevMsg("  Querying Steam board \"%s\" starting at row %d, %d total rows..\n\n", m_currentLeaderboardName, m_startingRowIndex, m_rowsPerPage );

	// Wait a second before we handle this query, in case user is flicking through options
	m_fQueryDelayTime = Plat_FloatTime() + QUERY_DELAY_TIME;


#endif
}

//-----------------------------------------------------------------------------
// Purpose: Determine how many results we have obtained from our previous query
//		Returns -1 while the query is still pending.
//-----------------------------------------------------------------------------
void CCreateLeaderboardsDialogScaleform::Query_NumResults( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int numResults = 0;

#if defined (_X360)
	if ( m_fQueryDelayTime > 0.f )
		numResults = -1; // Query is still in progress; display the "Searching..." msg
	else
		numResults = m_pResults.Count();
#endif  // _X360

#if !defined( NO_STEAM )
	if ( m_fQueryDelayTime > 0.f )
		numResults = -1; // Query is still in progress; display the "Searching..." msg
	else if ( m_bCheckForQueryResults )
		numResults = ( m_cachedLeaderboardScores.m_cEntryCount );
#endif

	m_pScaleformUI->Params_SetResult( obj, numResults );
}


//-----------------------------------------------------------------------------
// Purpose: Retrieve the row that contains information about the current player
//-----------------------------------------------------------------------------
void CCreateLeaderboardsDialogScaleform::Query_GetCurrentPlayerRow( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int playerRow = 0;

#if defined (_X360)
	for ( int Idx = 0; Idx < m_pResults.Count(); Idx ++ )
	{
		if ( m_pResults[Idx]->xuid == m_PlayerXUID )
		{
			playerRow = Idx;
			break;
		}
	}
#endif  // _X360

#if !defined (NO_STEAM)
	
	if ( m_currentLeaderboardHandle != 0 && m_cachedLeaderboardScores.m_hSteamLeaderboardEntries != 0 )
	{
		for ( int Idx = 0; Idx < m_cachedLeaderboardScores.m_cEntryCount; Idx++ )
		{
			LeaderboardEntry_t leaderboardEntry;
			if ( steamapicontext->SteamUserStats()->GetDownloadedLeaderboardEntry( m_cachedLeaderboardScores.m_hSteamLeaderboardEntries, Idx, &leaderboardEntry, NULL, 0 ) )
			{
				if ( leaderboardEntry.m_steamIDUser.ConvertToUint64() == m_PlayerXUID )
				{
					playerRow = Idx;
					break;
				}		
			}
		}
	}

#endif  // !NO_STEAM

	m_pScaleformUI->Params_SetResult( obj, playerRow );
}

//-----------------------------------------------------------------------------
// Purpose: Get the name of the user in our results list at the row specified
//-----------------------------------------------------------------------------
void CCreateLeaderboardsDialogScaleform::QueryRow_GamerTag( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int rowNumber = (int)( pui->Params_GetArgAsNumber( obj, 0 ) );

	// We retrieve all friend info as one big list, so offset into that list by the current row we're filling in Flash
	if ( m_currentFilterType == eLBFilter_Friends )
		rowNumber += m_startingRowIndex;

	const char *gamerTag = NULL;

#if defined( _X360 )	
	
	if ( rowNumber >= 0 && rowNumber < m_pResults.Count() )
	{
		gamerTag = m_pResults[rowNumber]->szGamertag;
	}

#endif // _X360

#if !defined( NO_STEAM )

	LeaderboardEntry_t leaderboardEntry;

	if ( steamapicontext->SteamUserStats()->GetDownloadedLeaderboardEntry( m_cachedLeaderboardScores.m_hSteamLeaderboardEntries, rowNumber, &leaderboardEntry, NULL, 0 ) )
	{
		gamerTag = steamapicontext->SteamFriends()->GetFriendPersonaName( leaderboardEntry.m_steamIDUser );
	}

#endif // !NO_STEAM

	if ( !gamerTag )
		gamerTag = "ERROR";

	wchar_t unicodeName[MAX_NETWORKID_LENGTH];
	g_pVGuiLocalize->ConvertANSIToUnicode( gamerTag, unicodeName, sizeof(unicodeName) );

	wchar_t safe_wide_name[MAX_NETWORKID_LENGTH * 10 ]; // add enough room to safely escape all 64 name characters
	safe_wide_name[0] = L'\0';			
	g_pScaleformUI->MakeStringSafe( unicodeName, safe_wide_name, sizeof( safe_wide_name ) );

	SFVALUE sfGamertag = CreateFlashString( safe_wide_name );

	m_pScaleformUI->Params_SetResult( obj, sfGamertag );

	SafeReleaseSFVALUE( sfGamertag );
}

//-----------------------------------------------------------------------------
// Purpose: Get the column value in our results list at the row specified
//-----------------------------------------------------------------------------
void CCreateLeaderboardsDialogScaleform::QueryRow_ColumnValue( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int numArgs = pui->Params_GetNumArgs( obj );

	if ( numArgs < 2 )
	{
		Warning( "QueryRow_ColumnValue: Invalid number of arguments in call to function: %d\n", numArgs );
		return;
	}

	int rowNumber = (int)( pui->Params_GetArgAsNumber( obj, 0 ) );
	
	// We retrieve all friend info as one big list, so offset into that list by the current row we're filling in Flash
	if ( m_currentFilterType == eLBFilter_Friends )
		rowNumber += m_startingRowIndex;

	int columnId = (int)( pui->Params_GetArgAsNumber( obj, 1 ) );

	int columnValue = -1;	
	char largeNumberString[64] = {0}; // if the field is a 64-bit value, we have to retrieve it as a string

#if defined( _X360 )

	if ( rowNumber >= 0 && rowNumber < m_pResults.Count() )
	{
		XUSER_STATS_ROW *pRow = m_pResults[rowNumber];
				
		if ( columnId == 0 )
		{
			// 0 retrieves just the player's Ranking (1st place player is 1, 2nd place is 2, etc)
			columnValue = pRow->dwRank;
		}
		else if ( columnId == -1 )
		{
			// -1 gets the Rating column, used to Rank the player
			V_snprintf( largeNumberString, sizeof(largeNumberString), "%lld", pRow->i64Rating );
		}
		else if ( columnId == -2 )
		{
			// -2 gets the row's overall board "Ranking / TotalRankedUsers"
			DWORD totalViewRows = 0;

			// Try to find the total view rows for this board
			if ( m_pResultsBuffer && m_pResultsBuffer->dwNumViews > 0 )
			{
				totalViewRows = m_pResultsBuffer->pViews[0].dwTotalViewRows;
			}
			else if ( m_pFriendsResult[0] && m_pFriendsResult[0]->dwNumViews > 0 )
			{
				totalViewRows = m_pFriendsResult[0]->pViews[0].dwTotalViewRows;
			}

			if ( totalViewRows > 0 )
			{
				V_snprintf( largeNumberString, sizeof(largeNumberString), "%d / %d",  pRow->dwRank,  totalViewRows );
			}
			else
			{
				// Couldn't locate valid totalViewRows, so just return this row's rank
				columnValue = pRow->dwRank;
			}
		}
		else if ( columnId == -3 )
		{
			// -3 retrieves the total number of users ranked on this view
			if ( m_pResultsBuffer && m_pResultsBuffer->dwNumViews > 0 )
			{
				columnValue = m_pResultsBuffer->pViews[0].dwTotalViewRows;
			}
		}
		else
		{
			for ( DWORD colIdx = 0; colIdx < pRow->dwNumColumns; ++colIdx )
			{
				if ( pRow->pColumns[colIdx].wColumnId == columnId )
				{
					switch ( pRow->pColumns[colIdx].Value.type )
					{
					case XUSER_DATA_TYPE_INT64:
						V_snprintf( largeNumberString, sizeof(largeNumberString), "%lld", pRow->pColumns[colIdx].Value.i64Data );
						break;
					case XUSER_DATA_TYPE_INT32:
						columnValue = pRow->pColumns[colIdx].Value.nData;
						break;
					default:
						Warning( "CCreateLeaderboardsDialogScaleform::QueryRow_ColumnValue - the type for columnId %d is not currently support: %d\n", columnId, pRow->pColumns[colIdx].Value.type );
						break;
					}
				}
			}
		}
	}	

#endif // _X360

#if !defined( NO_STEAM )

	LeaderboardEntry_t leaderboardEntry;

	int lbDetails[kMaxPayloadEntries] = { 0 };
	int numDetails = kMaxPayloadEntries;

	if ( columnId == -3 )
	{
		// -3 retrieves the total number of users ranked on this view
		columnValue = steamapicontext->SteamUserStats()->GetLeaderboardEntryCount( m_currentLeaderboardHandle );
	}
	else if ( columnId == -4 )
	{
		columnValue = ( steamapicontext->SteamUser()->BLoggedOn() ? 1 : 0 );
	}
	else
	if ( m_currentLeaderboardHandle != 0 && 
		 m_cachedLeaderboardScores.m_hSteamLeaderboardEntries != 0 &&
		 steamapicontext->SteamUserStats()->GetDownloadedLeaderboardEntry( m_cachedLeaderboardScores.m_hSteamLeaderboardEntries, rowNumber, &leaderboardEntry, lbDetails, numDetails ) )
	{
		// DevMsg( "Reading from row %d columnId: %d, extra details count = %d.\n", rowNumber, columnId, numDetails );

		if ( columnId == 0 )
		{
			int totalViewRows = steamapicontext->SteamUserStats()->GetLeaderboardEntryCount( m_currentLeaderboardHandle );

			if ( totalViewRows > 0 )
			{
				if ( leaderboardEntry.m_nGlobalRank <= LEADERBOARD_SHOW_RANK_UNDER )
					V_snprintf( largeNumberString, sizeof(largeNumberString), "%d", leaderboardEntry.m_nGlobalRank );
				else
					V_snprintf( largeNumberString, sizeof(largeNumberString), "%.1f%%", MAX( 0.1f, ((float)leaderboardEntry.m_nGlobalRank / (float)totalViewRows) * 100 ));
			}
			else
			{
				// Couldn't locate valid totalViewRows, so just return this row's rank
				if ( leaderboardEntry.m_nGlobalRank <= LEADERBOARD_SHOW_RANK_UNDER )
					columnValue = leaderboardEntry.m_nGlobalRank;
				else
					V_snprintf( largeNumberString, sizeof(largeNumberString), "%s", "--" );
			}
		}
		else if ( columnId == -1 )
		{
			columnValue = leaderboardEntry.m_nScore;
		}
		else if ( columnId == -2 )
		{
			// -2 gets the row's overall board "Ranking / TotalRankedUsers"
			int totalViewRows = steamapicontext->SteamUserStats()->GetLeaderboardEntryCount( m_currentLeaderboardHandle );

			if ( totalViewRows > 0 )
			{
				if ( leaderboardEntry.m_nGlobalRank <= LEADERBOARD_SHOW_RANK_UNDER )
					V_snprintf( largeNumberString, sizeof(largeNumberString), "%d", leaderboardEntry.m_nGlobalRank );
				else
					V_snprintf( largeNumberString, sizeof(largeNumberString), "%.1f%%", MAX( 0.1f, ((float)leaderboardEntry.m_nGlobalRank / (float)totalViewRows) * 100 ));
			}
			else
			{
				// Couldn't locate valid totalViewRows, so just return this row's rank
				if ( leaderboardEntry.m_nGlobalRank <= LEADERBOARD_SHOW_RANK_UNDER )
					columnValue = leaderboardEntry.m_nGlobalRank;
				else
					V_snprintf( largeNumberString, sizeof(largeNumberString), "%s", "--" );
			}
		}
		else if ( columnId == -3 )
		{
			// -3 retrieves the total number of users ranked on this view
			columnValue = steamapicontext->SteamUserStats()->GetLeaderboardEntryCount( m_currentLeaderboardHandle );
		}
		else
		{
			V_snprintf( largeNumberString, sizeof(largeNumberString), "%lld", ExtractPayloadDataByColumnID( lbDetails, columnId-1 ) );
		}
	}

#endif // !NO_STEAM

	if ( largeNumberString[0] != 0 )
	{
		SFVALUE sfNumberString = CreateFlashString( largeNumberString );

		m_pScaleformUI->Params_SetResult( obj, sfNumberString );

		SafeReleaseSFVALUE( sfNumberString );
	}
	else
	{
		m_pScaleformUI->Params_SetResult( obj, columnValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the ratio of 2 column values in our results list at the row specified
//		(for example, to retrieve the ratio of Kills to Deaths by querying
//		those two columns in unison and returning their ratio).
//-----------------------------------------------------------------------------
void CCreateLeaderboardsDialogScaleform::QueryRow_ColumnRatio( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int numArgs = pui->Params_GetNumArgs( obj );

	if ( numArgs < 3 )
	{
		Warning( "QueryRow_ColumnRatio: Invalid number of arguments in call to function: %d\n", numArgs );
		return;
	}

	int rowNumber = (int)( pui->Params_GetArgAsNumber( obj, 0 ) );
	
	// We retrieve all friend info as one big list, so offset into that list by the current row we're filling in Flash
	if ( m_currentFilterType == eLBFilter_Friends )
		rowNumber += m_startingRowIndex;

	int columnIdA = (int)( pui->Params_GetArgAsNumber( obj, 1 ) );
	int columnIdB = (int)( pui->Params_GetArgAsNumber( obj, 2 ) );
	
	// Optional param: specify formatting string for result
	const char *formatStr = "%3.3f";
	if ( numArgs > 3 )
	{
		formatStr = pui->Params_GetArgAsString( obj, 3 );
	}

	// Optional param: If true, return the value scaled up by 100 to be a percentage
	bool bPercentage = false;
	if ( numArgs > 4 )
	{
		bPercentage = pui->Params_GetArgAsBool( obj, 4 );
	}

	// Optional param: If true, we want the ratio of columnA / (columnA+columnB) - useful for calculating win percentage
	bool bRatioSumAB = false;
	if ( numArgs > 5 )
	{
		bRatioSumAB = pui->Params_GetArgAsBool( obj, 5 );
	}

#if defined( _X360 )

	// Our resulting ratio will be "columnA / columnB" or "column A / (columnA + columnB)"
	LONGLONG columnValueA = 0;
	LONGLONG columnValueB = 0;

	if ( rowNumber >= 0 && rowNumber < m_pResults.Count() )
	{
		XUSER_STATS_ROW *pRow = m_pResults[rowNumber];

		if ( columnIdA == -1 )
		{
			columnValueA = pRow->i64Rating;
		}
		if ( columnIdB == -1 )
		{
			columnValueB = pRow->i64Rating;
		}
		if ( columnIdA == -2 )
		{
			columnValueA = pRow->dwRank;
		}
		if ( columnIdB == -2 )
		{
			columnValueB = pRow->dwRank;
		}
		
		for ( DWORD colIdx = 0; colIdx < pRow->dwNumColumns; ++colIdx )
		{
			bool bIsColumnA = pRow->pColumns[colIdx].wColumnId == columnIdA;
			bool bIsColumnB = pRow->pColumns[colIdx].wColumnId == columnIdB;
			
			if ( bIsColumnA || bIsColumnB )
			{
				switch ( pRow->pColumns[colIdx].Value.type )
				{
					case XUSER_DATA_TYPE_INT64:
						if ( bIsColumnA )
							columnValueA = pRow->pColumns[colIdx].Value.i64Data;
						if ( bIsColumnB )
							columnValueB = pRow->pColumns[colIdx].Value.i64Data;
						break;

					case XUSER_DATA_TYPE_INT32:
						if ( bIsColumnA )
							columnValueA = pRow->pColumns[colIdx].Value.nData;
						if ( bIsColumnB )
							columnValueB = pRow->pColumns[colIdx].Value.nData;
						break;

					default:
						Warning( "CCreateLeaderboardsDialogScaleform::QueryRow_ColumnValue - the type for columnId %d is not currently support: %d\n", pRow->pColumns[colIdx].wColumnId, pRow->pColumns[colIdx].Value.type );
						break;
				}
			}
		}
	}

#endif // _X360

#if !defined( NO_STEAM )

	// Our resulting ratio will be "columnA / columnB" or "column A / (columnA + columnB)"
	uint64 columnValueA = 0;
	uint64 columnValueB = 0;

	LeaderboardEntry_t leaderboardEntry;

	int lbDetails[kMaxPayloadEntries] = { 0 };
	int numDetails = kMaxPayloadEntries;

	if ( m_currentLeaderboardHandle != 0 && 
		m_cachedLeaderboardScores.m_hSteamLeaderboardEntries != 0 &&
		steamapicontext->SteamUserStats()->GetDownloadedLeaderboardEntry( m_cachedLeaderboardScores.m_hSteamLeaderboardEntries, rowNumber, &leaderboardEntry, lbDetails, numDetails ) )
	{
		if ( columnIdA == -1 )
		{
			columnValueA = leaderboardEntry.m_nScore;
		}
		if ( columnIdB == -1 )
		{
			columnValueB = leaderboardEntry.m_nScore;
		}
		if ( columnIdA == -2 )
		{
			columnValueA = leaderboardEntry.m_nGlobalRank;
		}
		if ( columnIdB == -2 )
		{
			columnValueB = leaderboardEntry.m_nGlobalRank;
		}
		if ( columnIdA > 0 )
		{
			// 1+ are the details of the entry, subtract 1 to lookup in the array			
			columnValueA = ExtractPayloadDataByColumnID( lbDetails, columnIdA-1 );
		}
		if ( columnIdB > 0 )
		{
			// 1+ are the details of the entry, subtract 1 to lookup in the array			
			columnValueB = ExtractPayloadDataByColumnID( lbDetails, columnIdB-1 );
		}
	}

#endif // !NO_STEAM

#if !defined( NO_STEAM ) || defined( _X360 ) 
	float fResult = (float)columnValueA;
	float fDivisor = (float)columnValueB;

	if ( bRatioSumAB )
	{
		// When this flag is set, we want to divide by the sum of the two columns, so we return the ratio of A to the sum of A and B
		fDivisor = (float)columnValueA + (float)columnValueB;
	}

	if ( fDivisor != 0 )
	{
		fResult = (float)columnValueA / fDivisor;
	}

	if ( bPercentage )
	{
		fResult *= 100.f;
	}

	char ratioString[64];
	V_snprintf( ratioString, sizeof(ratioString), formatStr, fResult );

	SFVALUE sfRatioString = CreateFlashString( ratioString );

	m_pScaleformUI->Params_SetResult( obj, sfRatioString );

	SafeReleaseSFVALUE( sfRatioString );
#endif  // !defined( NO_STEAM ) || defined( _X360 ) 
}

//-----------------------------------------------------------------------------
// Purpose: Trigger the display of additional info about a user in the specified
//		row of our query results.  i.e. On XBox this brings up the Gamer Card
//-----------------------------------------------------------------------------
void CCreateLeaderboardsDialogScaleform::DisplayUserInfo( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int rowNumber = (int)( pui->Params_GetArgAsNumber( obj, 0 ) );
	
	// We retrieve all friend info as one big list, so offset into that list by the current row we're filling in Flash
	if ( m_currentFilterType == eLBFilter_Friends )
		rowNumber += m_startingRowIndex;

#if defined( _X360 )
	if ( rowNumber >= 0 && rowNumber < m_pResults.Count() )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		XShowGamerCardUI( XBX_GetActiveUserId(), m_pResults[rowNumber]->xuid );	
	}
#endif // _X360

// Steam overlay not available on console (PS3)
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM ) 
	// Show the steam user id in the overlay
	LeaderboardEntry_t leaderboardEntry;
	if ( m_currentLeaderboardHandle != 0 && 
		m_cachedLeaderboardScores.m_hSteamLeaderboardEntries != 0 &&
		steamapicontext->SteamUserStats()->GetDownloadedLeaderboardEntry( m_cachedLeaderboardScores.m_hSteamLeaderboardEntries, rowNumber, &leaderboardEntry, NULL, 0 ) )
	{
		steamapicontext->SteamFriends()->ActivateGameOverlayToUser( "steamid", leaderboardEntry.m_steamIDUser );
	}
#endif // !_GAMECONSOLE && !NO_STEAM

}

//-----------------------------------------------------------------------------
// Purpose: Cancels any existing queries and issues a new query.
//-----------------------------------------------------------------------------
void CCreateLeaderboardsDialogScaleform::QueryUpdate( void )
{
	// If we have no new query, early out
	if ( m_fQueryDelayTime <= 0.f )
		return;

	// Don't actually update the query until our delay has passed
	if ( Plat_FloatTime() < m_fQueryDelayTime )
		return;

#if defined (_X360)

	// Cancel any existing queries
	if ( m_hAsyncQuery != 0 )
	{
		Warning( "Leaderboard: QueryUpdate: Triggering a new update with an AsynchHandle still in flight!\n" );
		xboxsystem->CancelOverlappedOperation( m_hAsyncQuery );
		xboxsystem->ReleaseAsyncHandle( m_hAsyncQuery );
		m_hAsyncQuery = 0;
	}

#ifdef	DIAGNOSE_ASYNCH_PROBLEMS
	printf( "Leaderboard: QueryUpdate: Starting a new network query; allocating an AsyncHandle.\n" );
#endif

	// Create a new query handle
	m_hAsyncQuery = xboxsystem->CreateAsyncHandle();
	m_bCheckForQueryResults = false;

	// Wipe out any previous results
	if ( m_pResultsBuffer != NULL )
	{
		delete [] m_pResultsBuffer;
		m_pResultsBuffer = NULL;
	}

	if ( m_currentFilterType != eLBFilter_Friends )
	{
		m_pResults.RemoveAll();
	}

	// Invalidate any results
	m_bResultsValid = false;

	m_fQueryDelayTime = 0.f;

	// Write any XBox API results into this value
	int asynchResult = ERROR_SUCCESS;
	
	switch ( m_currentFilterType )
	{
	case eLBFilter_Me:
		{
			asynchResult = xboxsystem->EnumerateStatsByXuid( m_PlayerXUID, m_rowsPerPage, 1, &m_statsSpec, (void**)&m_pResultsBuffer, true, &m_hAsyncQuery );
		}
		break;
	
	case eLBFilter_Overall:
		{
			asynchResult = xboxsystem->EnumerateStatsByRank( m_startingRowIndex, m_rowsPerPage, 1, &m_statsSpec, (void**)&m_pResultsBuffer, true, &m_hAsyncQuery );
		}
		break;

	case eLBFilter_Friends:
		{
			// If we haven't retrieved our friends list yet, do that first
			if ( m_pFriends == NULL )
			{
				// Reset our friend tracking members
				m_iNextFriend = -1;
				m_bEnumeratingFriends = true;

				asynchResult = xboxsystem->EnumerateFriends( m_iPlayerSlot, (void**)&m_pFriends, true, &m_hAsyncQuery );

				if ( asynchResult != ERROR_SUCCESS && asynchResult != ERROR_IO_PENDING )
				{
					// Free the buffer, and error out now!
					delete [] m_pFriends;
					m_pFriends = NULL;

					Warning( "LEADERBOARDS: QueryUpdate - Failed to enumerate our friends!" );
				}
			}
			else
			{
				// Once we have a friends list, start querying each user individually
				if ( m_iNextFriend < 0 )
				{
					// Reset the results list now - from here on out, we're writing to this incrementally for each friend in our list
					m_pResults.RemoveAll();
					m_iNextFriend = 0;

					// Local user comes first, first entry in friends result list
					asynchResult = xboxsystem->EnumerateStatsByXuid( m_PlayerXUID, 1, 1, &m_statsSpec, (void**)&(m_pFriendsResult[m_iNextFriend]), true, &m_hAsyncQuery );
				}
				else
				{
					// Now, once for each friend in the list
					// NOTE! m_pFriends has exactly NumFriends entries, but m_pFriendsResult has an extra leading entry for the local user!
					asynchResult = xboxsystem->EnumerateStatsByXuid( m_pFriends[m_iNextFriend - 1].xuid, 1, 1, &m_statsSpec, (void**)&(m_pFriendsResult[m_iNextFriend]), true, &m_hAsyncQuery );
				}
			}
		}
		break;
	}

	// Check return value
	if ( asynchResult != ERROR_SUCCESS && asynchResult != ERROR_IO_PENDING )
	{
		// Free the buffer, and error out now!
		delete [] m_pResultsBuffer;
		m_pResultsBuffer = NULL;

		Warning( "LEADERBOARDS: QueryUpdate - Failed to enumerate leaderboard results!" );
		return;
	}

	// Begin waiting for results to come back
	m_bCheckForQueryResults = true;

#endif // _X360

#if !defined( NO_STEAM )

	if ( !steamapicontext || !steamapicontext->SteamUserStats() )
		return;

	// Invalidate any results
	m_bResultsValid = false;
	V_memset( &m_cachedLeaderboardScores, 0, sizeof(LeaderboardScoresDownloaded_t) );

	m_fQueryDelayTime = 0.f;

	// Read out the leaderboard format description from matchmaking
	if ( !m_pLeaderboardDescription )
		m_pLeaderboardDescription->deleteThis();

	m_pLeaderboardDescription = g_pMatchFramework->GetMatchTitle()->DescribeTitleLeaderboard( m_currentLeaderboardName );

	// Extract payload data size info
	V_memset( m_payloadSizes, 0, sizeof(int)*kMaxPayloadEntries );

	KeyValues *pPayloadFormat = m_pLeaderboardDescription ? m_pLeaderboardDescription->FindKey( ":payloadformat" ) : NULL;

	if ( pPayloadFormat )
	{
		for ( int payloadIndex=0; payloadIndex < kMaxPayloadEntries; ++payloadIndex )
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
				m_payloadSizes[payloadIndex] += sizeof( uint32 );
			}
			else if ( V_stricmp( pszFormat, "uint64" ) == 0 )
			{
				m_payloadSizes[payloadIndex] += sizeof( uint64 );
			}
			else
			{
				Warning( " LEADERBOARDS: WARNING! Unsupported data type in leaderboard payload data - assuming 32 bit\n\n" );
				m_payloadSizes[payloadIndex] += sizeof( uint32 );
			}
		}
	}

	m_currentLeaderboardHandle = GetLeaderboardHandle( m_currentLeaderboardName );

	// If we haven't loaded this leaderboard yet, look up its handle first
	if ( m_currentLeaderboardHandle == 0 )
	{
		SteamAPICall_t hSteamAPICall = steamapicontext->SteamUserStats()->FindLeaderboard( m_currentLeaderboardName );
		if ( hSteamAPICall != 0 )
		{
			m_SteamCallResultFindLeaderboard.Set( hSteamAPICall, this, &CCreateLeaderboardsDialogScaleform::Steam_OnFindLeaderboard );
		}
	}
	else
	{
		// We can perform our query for this leaderboard immediately
		QueryLeaderboard( );
	}	

#endif // !NO_STEAM
}

#if !defined ( NO_STEAM )

uint64	CCreateLeaderboardsDialogScaleform::ExtractPayloadDataByColumnID( int *pData, int columnId )
{
	char* pPtr = (char*)pData;
	int curColumnId = 0;

	if ( !pPtr )
		return 0ull;

	while ( curColumnId < columnId && curColumnId < kMaxPayloadEntries )
	{
		pPtr += m_payloadSizes[curColumnId++];
	}

	if ( curColumnId >= kMaxPayloadEntries )
		return 0ull;

#if defined( PLAT_BIG_ENDIAN )
	// On big-endian platforms, we need to byteswap the values written to Steam since we always write them little-endian 
	//		and Steam doesn't swap them for us
	return ( m_payloadSizes[curColumnId] == sizeof(uint32) ? DWordSwap(*(uint32*)pPtr) : QWordSwap(*(uint64*)pPtr) );
#else
	return ( m_payloadSizes[curColumnId] == sizeof(uint32) ? (*(uint32*)pPtr) : (*(uint64*)pPtr) );
#endif
}


SteamLeaderboard_t CCreateLeaderboardsDialogScaleform::GetLeaderboardHandle( const char* szLeaderboardName )
{
	// make a mapping of map contexts to leaderboard handles
	unsigned short index = m_LeaderboardHandles.Find( szLeaderboardName );

	if ( index != m_LeaderboardHandles.InvalidIndex() )
	{
		return m_LeaderboardHandles.Element(index);
	}

	return (SteamLeaderboard_t)0;
}

void CCreateLeaderboardsDialogScaleform::SetLeaderboardHandle( const char* szLeaderboardName, SteamLeaderboard_t hLeaderboard )
{
	m_LeaderboardHandles.InsertOrReplace( szLeaderboardName, hLeaderboard );
}

//=============================================================================
// Callback for FindLeaderboard
//=============================================================================
void CCreateLeaderboardsDialogScaleform::Steam_OnFindLeaderboard( LeaderboardFindResult_t *pFindLeaderboardResult, bool bIOFailure )
{
	// see if we encountered an error during the call
	if ( !pFindLeaderboardResult->m_bLeaderboardFound || bIOFailure )
	{
		Warning( " Leaderboards: Steam error trying to retrieve leaderboard \"%s\"\n\n", m_currentLeaderboardName );
		
		// Report the error now
		m_bCheckForQueryResults = true;

		return;
	}

	// check to see which leaderboard handle we just retrieved
	const char *pszReturnedName = steamapicontext->SteamUserStats()->GetLeaderboardName( pFindLeaderboardResult->m_hSteamLeaderboard );
	Assert( StringHasPrefix( pszReturnedName, m_currentLeaderboardName ) );

	SetLeaderboardHandle( pszReturnedName, pFindLeaderboardResult->m_hSteamLeaderboard );

	m_currentLeaderboardHandle = pFindLeaderboardResult->m_hSteamLeaderboard;

	// Now we can perform our query for this leaderboard
	QueryLeaderboard( );

}

//=============================================================================
// Callback for DownloadLeaderboardEntries
//=============================================================================
void CCreateLeaderboardsDialogScaleform::Steam_OnLeaderboardScoresDownloaded( LeaderboardScoresDownloaded_t *p, bool bError )
{
	if ( bError )
		Warning( " Leaderboard \"%s\": Steam_OnLeaderboardScoresDownloaded received an error.\n", m_currentLeaderboardName );

	// Fetch the data if found and no error	
	if ( !bError )
	{
		// DevMsg( " Leaderboard \"%s\": %d Entries found.\n", m_currentLeaderboardName, p->m_cEntryCount );

		V_memcpy( &m_cachedLeaderboardScores, p, sizeof( LeaderboardScoresDownloaded_t ) );
	}

	m_bCheckForQueryResults = true;
}


void CCreateLeaderboardsDialogScaleform::QueryLeaderboard( void )
{
	SteamAPICall_t hSteamAPICall = (SteamAPICall_t)0;

	if ( m_currentLeaderboardHandle == 0 )
	{
		Warning( " CCreateLeaderboardsDialogScaleform::QueryLeaderboard - leaderboard not found: \"%s\"\n", m_currentLeaderboardName );
		// Report the error now
		m_bCheckForQueryResults = true;
		return;
	}

	switch ( m_currentFilterType )
	{
	case eLBFilter_Me:
		{
			hSteamAPICall = steamapicontext->SteamUserStats()->DownloadLeaderboardEntries( m_currentLeaderboardHandle, k_ELeaderboardDataRequestGlobalAroundUser, -(m_rowsPerPage/2), (m_rowsPerPage/2) );
		}
		break;

	case eLBFilter_Overall:
		{
			hSteamAPICall = steamapicontext->SteamUserStats()->DownloadLeaderboardEntries( m_currentLeaderboardHandle, k_ELeaderboardDataRequestGlobal, m_startingRowIndex, m_startingRowIndex + m_rowsPerPage - 1 );
		}
		break;

	case eLBFilter_Friends:
		{
			hSteamAPICall = steamapicontext->SteamUserStats()->DownloadLeaderboardEntries( m_currentLeaderboardHandle, k_ELeaderboardDataRequestFriends, 0, 0 );
		}
		break;
	}	

	if ( hSteamAPICall )
	{
		// Register for the async callback
		m_SteamCallbackOnLeaderboardScoresDownloaded.Set( hSteamAPICall, this, &CCreateLeaderboardsDialogScaleform::Steam_OnLeaderboardScoresDownloaded );
	}
	else
	{
		// Report the error now
		m_bCheckForQueryResults = true;
	}
}

#endif // !NO_STEAM

#if defined (_X360)
//=============================================================================
// Purpose: Sorts a vector of rank/indexes in to the results buffer so we can
// sort friend lists
//=============================================================================
int FriendsSortFunc( XUSER_STATS_ROW *const *a, XUSER_STATS_ROW *const *b )
{
	if ( (*a)->dwRank < (*b)->dwRank )
		return -1;

	if ( (*a)->dwRank > (*b)->dwRank )
		return 1;

	return 0;
}
#endif // _X360

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCreateLeaderboardsDialogScaleform::CheckForQueryResults( void )
{
#if defined (_X360)

	// No new query to check on
	if ( !m_bCheckForQueryResults )
		return;

	// Check our async handle to see if it's done
	uint iResultCode = 0;
	int result = xboxsystem->GetOverlappedResult( m_hAsyncQuery, &iResultCode, false );

	// Abort on non-recoverable error codes
	if ( result == ERROR_INVALID_HANDLE ||
		 result == ERROR_FUNCTION_FAILED )
	{
		Warning( "LEADERBOARDS: CheckForQueryResults got error code: %s. Aborting query.", result == ERROR_INVALID_HANDLE ? "ERROR_INVALID_HANDLE" : "ERROR_FUNCTION_FAILED ");
	
		if ( m_hAsyncQuery != 0 )
		{
			xboxsystem->ReleaseAsyncHandle( m_hAsyncQuery );
			m_hAsyncQuery = 0;
		}

		// Signal flash to terminate
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "NotifyResults", 0, NULL );
		}
		m_bCheckForQueryResults = false;
		
		return;
	}

#ifdef	DIAGNOSE_ASYNCH_PROBLEMS
	printf( "Leaderboard: CheckForQueryResults - GetOverlappedResult returns %d, (result code %d)\n", result, iResultCode );
#endif

	// If we are still retrieving friends info, check on that operation first
	if ( m_bEnumeratingFriends )
	{
		if ( result == ERROR_SUCCESS )
		{
			m_iNumFriends = iResultCode;
			m_bEnumeratingFriends = false;
			m_bCheckForQueryResults = false;

			// Queue up the next query
			m_fQueryDelayTime = Plat_FloatTime() + QUERY_SHORT_DELAY_TIME;
		}

		return;
	}

	// Our query is complete, now we need to see if the query ran into any errors
	if ( result == ERROR_SUCCESS )
	{
		// Filter: FRIENDS
		// Handle friends differently, by pushing multiple queries until we've read out everyone
		if ( m_currentFilterType == eLBFilter_Friends )
		{
			// Check that the friend result is valid, and add it to our vector
			if ( m_pFriendsResult[m_iNextFriend]  &&
				 m_pFriendsResult[m_iNextFriend]->dwNumViews > 0 &&
				 m_pFriendsResult[m_iNextFriend]->pViews[0].dwNumRows > 0 )
			{
#ifdef	DIAGNOSE_ASYNCH_PROBLEMS
				printf( "Found FRIEND result user = %s: xuid: %d columns: %d (rank: %d with rating: %lld) \n", 
					m_pFriendsResult[m_iNextFriend]->pViews[0].pRows[0].szGamertag,		m_pFriendsResult[m_iNextFriend]->pViews[0].pRows[0].xuid, 
					m_pFriendsResult[m_iNextFriend]->pViews[0].pRows[0].dwNumColumns,	m_pFriendsResult[m_iNextFriend]->pViews[0].pRows[0].dwRank,
					m_pFriendsResult[m_iNextFriend]->pViews[0].pRows[0].i64Rating );
#endif

				if ( m_pFriendsResult[m_iNextFriend]->pViews[0].pRows[0].dwRank > 0 )
				{
					m_pResults.AddToTail( &m_pFriendsResult[m_iNextFriend]->pViews[0].pRows[0] );
				}
			}

			m_iNextFriend++;

			if ( m_iNextFriend < m_iNumFriends + 1 )
			{
				m_bCheckForQueryResults = false;

				// Queue up the next query
				m_fQueryDelayTime = Plat_FloatTime() + QUERY_SHORT_DELAY_TIME;

				return;
			}
			else
			{
				// Now sort the results list by the ranking column
				m_pResults.Sort( FriendsSortFunc );

				// we have all results, return now!
				m_bResultsValid = true;
			}
		}

		// Filter: Overall/Me
		// Non-friends query - all results come back in one batch
		else
		{
			// We assume the results are valid now, and push only ranked values into our m_pResults list
			m_bResultsValid = true;

			// Reset the list, fill it with current data
			m_pResults.RemoveAll();

			// Make sure to update the starting index based on the actual starting index we get back from the results
			if ( m_pResultsBuffer )
			{
				if ( m_pResultsBuffer->dwNumViews > 0 &&
					 m_pResultsBuffer->pViews[0].dwNumRows > 0 )
				{
					// Copy pointers to our results buffer into a vector so we can sort and handle friends properly
					for ( uint i=0; i<m_pResultsBuffer->pViews[0].dwNumRows; i++ )
					{
						// Only add players that are ranked
						if ( m_pResultsBuffer->pViews[0].pRows[i].dwRank > 0 )
						{
							m_pResults.AddToTail( &m_pResultsBuffer->pViews[0].pRows[i] );

#ifdef	DIAGNOSE_ASYNCH_PROBLEMS
							printf( "Found result user = %s: xuid: %d columns: %d (rank: %d with rating: %lld) \n", 
								m_pResultsBuffer->pViews[0].pRows[i].szGamertag,	m_pResultsBuffer->pViews[0].pRows[i].xuid, 
								m_pResultsBuffer->pViews[0].pRows[i].dwNumColumns,	m_pResultsBuffer->pViews[0].pRows[i].dwRank,
								m_pResultsBuffer->pViews[0].pRows[i].i64Rating );

							for ( DWORD col = 0; col < m_pResultsBuffer->pViews[0].pRows[i].dwNumColumns; col++ )
							{
								printf( "    column[%d] (type %d) = %d \n", col, 
									m_pResultsBuffer->pViews[0].pRows[i].pColumns[col].wColumnId, 
									m_pResultsBuffer->pViews[0].pRows[i].pColumns[col].Value.nData );
							}
#endif
						}
					}
				}
			}
		}

		// Signal Flash that we are ready to show results
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "NotifyResults", 0, NULL );
		}

		m_bCheckForQueryResults = false;
	}

#endif // _X360

#if !defined( NO_STEAM )
	// No new query to check on
	if ( !m_bCheckForQueryResults )
		return;

	// Signal Flash that we are ready to show results
	WITH_SLOT_LOCKED
	{
		ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "NotifyResults", 0, NULL );
	}

	m_bCheckForQueryResults = false;
#endif // !NO_STEAM
}

#endif // INCLUDE_SCALEFORM

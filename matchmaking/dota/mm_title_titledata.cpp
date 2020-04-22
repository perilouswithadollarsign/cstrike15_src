//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


TitleDataFieldsDescription_t const * CMatchTitle::DescribeTitleDataStorage()
{
#define TD_ENTRY( szName, nTD, eDataType, numBytesOffset ) \
	{ szName, TitleDataFieldsDescription_t::nTD, TitleDataFieldsDescription_t::eDataType, numBytesOffset }

	static TitleDataFieldsDescription_t tdfd[] =
	{
#if 0
		TD_ENTRY( "TD1.Easy.Games.Total",		DB_TD1,	DT_U64,		offsetof( TitleData1, mGames[0] ) ),
		TD_ENTRY( "TD1.Normal.Games.Total",		DB_TD1,	DT_U64,		offsetof( TitleData1, mGames[1] ) ),
		TD_ENTRY( "TD1.Advanced.Games.Total",	DB_TD1,	DT_U64,		offsetof( TitleData1, mGames[2] ) ),
		TD_ENTRY( "TD1.Expert.Games.Total",		DB_TD1,	DT_U64,		offsetof( TitleData1, mGames[3] ) ),
#endif
		TD_ENTRY( NULL, DB_TD1, DT_U8, 0 )
	};

#undef TD_ENTRY

	return tdfd;
}

TitleAchievementsDescription_t const * CMatchTitle::DescribeTitleAchievements()
{
	static TitleAchievementsDescription_t tad[] =
	{
//#include "left4dead2.xhelp.achtitledesc.txt"
		// END MARKER
		{ NULL, 0 }
	};

	return tad;
}

TitleAvatarAwardsDescription_t const * CMatchTitle::DescribeTitleAvatarAwards()
{
	static TitleAvatarAwardsDescription_t taad[] =
	{
//#include "left4dead2.xhelp.avawtitledesc.txt"
		// END MARKER
		{ NULL, 0 }
	};

	return taad;
}

// Title leaderboards
KeyValues * CMatchTitle::DescribeTitleLeaderboard( char const *szLeaderboardView )
{
/*
	// Check if this is a survival leaderboard
	if ( char const *szSurvivalMap = StringAfterPrefix( szLeaderboardView, "survival_" ) )
	{
		if ( IsX360() )
		{
			// Find the corresponding record in the mission script
			KeyValues *pSettings = new KeyValues( "settings" );
			KeyValues::AutoDelete autodelete_pSettings( pSettings );
			pSettings->SetString( "game/mode", "survival" );

			KeyValues *pMissionInfo = NULL;
			KeyValues *pMapInfo = g_pMatchExtL4D->GetMapInfoByBspName( pSettings, szSurvivalMap, &pMissionInfo );
			if ( !pMapInfo || !pMissionInfo )
				return NULL;

			// Find the leaderboard description in the map info
			KeyValues *pLbDesc = pMapInfo->FindKey( "x360leaderboard" );
			if ( !pLbDesc )
				return NULL;
			
			// Insert the required keys
			pLbDesc = pLbDesc->MakeCopy();

			static KeyValues *s_pRatingKey = KeyValues::FromString( ":rating",			// X360 leaderboards are rated
				" name besttime "														// game name of the rating field is "besttime"
				" type uint64 "															// type is uint64
				" rule max"																// rated field must be greater than cached value so that it can be written
				);
			pLbDesc->AddSubKey( s_pRatingKey->MakeCopy() );
			pLbDesc->SetString( "besttime/type", "uint64" );

			return pLbDesc;
		}

		if ( IsPC() )
		{
			KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
				" :score besttime "														// :score is the leaderboard value mapped to game name "besttime"
				);

			pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
			pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeTimeMilliSeconds );	// Note: this is actually 1/100th seconds type, Steam change pending
			pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodKeepBest );	// Upload method when writing to leaderboard

			return pSettings;
		}
	}
*/

	return NULL;
}


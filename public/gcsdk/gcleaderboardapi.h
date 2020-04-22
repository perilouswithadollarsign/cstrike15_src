//========= Copyright (c), Valve LLC, All rights reserved. ============
//
// Purpose: API to interact with Steam leaderboards on the GC.
//
// $NoKeywords: $
//=============================================================================

#ifndef GCLEADERBOARDAPI_H
#define GCLEADERBOARDAPI_H

namespace GCSDK
{
	class CGCBase;

	/**
	 * Yielding call that attempts to find a leaderboard by name, creating one if necessary.
	 * @param pName
	 * @param eLeaderboardSortMethod
	 * @param eLeaderboardDisplayType
	 * @param bCreateIfNotFound
	 * @return 0 if the leaderboard was not found, > 0 otherwise
	 */
	uint32 Leaderboard_YieldingFind( const char *pName, ELeaderboardSortMethod eLeaderboardSortMethod, ELeaderboardDisplayType eLeaderboardDisplayType, bool bCreateIfNotFound );

	/**
	 * Yielding call that attempts to set the score for the steamID in the leaderboard.
	 * @param unLeaderboardID
	 * @param steamID
	 * @param eLeaderboardUploadScoreMethod
	 * @param score
	 * @param pDetails
	 * @param unDetailsLength
	 * @return true if successful, false otherwise.
	 */
	bool Leaderboard_YieldingSetScore( uint32 unLeaderboardID, const CSteamID &steamID, ELeaderboardUploadScoreMethod eLeaderboardUploadScoreMethod, int score, uint8* pDetails = 0, uint32 unDetailsLength = 0 );

	/**
	 * @param pKVOutput
	 * @return true if successful, false otherwise.
	 */
	bool Leaderboard_YieldingGetLeaderboardsForGame( KeyValuesAD* pKVOutput );

	/**
	 * Yielding call that attempts to get leaderboard entries by leaderboard name
	 * @param unRangeStart
	 * @param unRangeEnd
	 * @param pSteamID
	 * @param unLeaderboardID
	 * @param eDataRequestType
	 * @param pKVOutput
	 * @return number of entries found from specified range.
	 */
	bool Leaderbaord_YieldingGetLeaderboardEntries( int32 nRangeStart, int32 nRangeEnd, CSteamID* pSteamID, int32 unLeaderboardID, ELeaderboardDataRequest eDataRequestType, KeyValuesAD* pKVOutput );
}; // namespace GCSDK

#endif // GCLEADERBOARDAPI_H

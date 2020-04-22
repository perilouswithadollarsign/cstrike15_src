//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "matchext_swarm.h"
#include "swarm.spa.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

TitleDataFieldsDescription_t const * CMatchTitle::DescribeTitleDataStorage()
{
#define TD_ENTRY( szName, nTD, eDataType, numBytesOffset ) \
	{ szName, TitleDataFieldsDescription_t::nTD, TitleDataFieldsDescription_t::eDataType, numBytesOffset }

	static TitleDataFieldsDescription_t tdfd[] =
	{
#if 0
		// ACHIEVEMENTS
		TD_ENTRY( "TD2.COUNT.ACH_BEAT_CAMPAIGNS_EXPERT_MODE",	DB_TD2,	DT_U8,		offsetof( TitleData2, iCountBeatCampaignsExpertMode		) ),
		TD_ENTRY( "TD2.COUNT.ACH_CUT_OFF_HEADS_MELEE",			DB_TD2,	DT_U8,		offsetof( TitleData2, iCountCutOffHeadsMelee			) ),
		TD_ENTRY( "TD2.COUNT.ACH_KILL_WITH_EVERY_MELEE",		DB_TD2,	DT_U8,		offsetof( TitleData2, iCountKillWithEveryMelee			) ),
		TD_ENTRY( "TD2.COUNT.ACH_KILL_INFECTED_WITH_CHAINSAW",	DB_TD2,	DT_U8,		offsetof( TitleData2, iCountKillInfectedWithChainsaw	) ),
		TD_ENTRY( "TD2.COUNT.ACH_RES_SURVIVORS_WITH_DEFIB",		DB_TD2,	DT_U8,		offsetof( TitleData2, iCountResSurvivorsWithDefib		) ),
		TD_ENTRY( "TD2.COUNT.ACH_SPEED_REVIVE_WITH_ADRENALINE",	DB_TD2,	DT_U8,		offsetof( TitleData2, iCountSpeedReviveWithAdrenaline	) ),
		TD_ENTRY( "TD2.COUNT.ACH_IGNITE_INFECTED_FIRE_AMMO",	DB_TD2,	DT_U8,		offsetof( TitleData2, iCountIgniteInfectedFireAmmo		) ),
		TD_ENTRY( "TD2.COUNT.ACH_KILL_EVERY_UNCOMMON_INFECTED",	DB_TD2,	DT_U8,		offsetof( TitleData2, iCountKillEveryUncommonInfected	) ),
		TD_ENTRY( "TD2.COUNT.ACH_KILL_SUBMERGED_MUDMEN",		DB_TD2,	DT_U8,		offsetof( TitleData2, iCountKillSubmergedMudmen			) ),
		TD_ENTRY( "TD2.COUNT.ACH_COLLECT_CEDA_VIALS",			DB_TD2,	DT_U8,		offsetof( TitleData2, iCountCollectCEDAVials			) ),
		TD_ENTRY( "TD2.COUNT.ACH_SCAVENGE_COLLECT_CAN_GRIND",	DB_TD2,	DT_U8,		offsetof( TitleData2, iCountScavengeCollectCanGrind		) ),
		TD_ENTRY( "TD2.COUNT.ACH_SCAVENGE_CAN_DROP_GRIND",		DB_TD2,	DT_U8,		offsetof( TitleData2, iCountScavengeCanDropGrind		) ),
		TD_ENTRY( "TD2.COUNT.ACH_HONK_A_CLOWNS_NOSE",			DB_TD2,	DT_U8,		offsetof( TitleData2, iCountHonkAClownsNose				) ),
		TD_ENTRY( "TD2.COMP.ACH_BEAT_CAMPAIGNS_EXPERT_MODE",	DB_TD2,	DT_U8,		offsetof( TitleData2, iCompBeatCampaignsExpertMode		) ),
		TD_ENTRY( "TD2.COMP.ACH_KILL_WITH_EVERY_MELEE",			DB_TD2,	DT_U16,		offsetof( TitleData2, iCompKillWithEveryMelee			) ),
		TD_ENTRY( "TD2.COMP.ACH_KILL_EVERY_UNCOMMON_INFECTED",	DB_TD2,	DT_U16,		offsetof( TitleData2, iCompKillEveryUncommonInfected	) ),

		// AVATAR AWARDS
		TD_ENTRY( "TD2.COUNT.ASSET_MED_KIT",					DB_TD2,	DT_U8,		offsetof( TitleData2, iCountBeatCampaignsAnyMode		) ),
		TD_ENTRY( "TD2.COMP.ASSET_MED_KIT",						DB_TD2,	DT_U8,		offsetof( TitleData2, iCompBeatCampaignsAnyMode			) ),
		TD_ENTRY( "TD2.COUNT.ASSET_PIPE_BOMB",					DB_TD2,	DT_U16,		offsetof( TitleData2, iCountKillTenThousandZombies		) ),
		TD_ENTRY( "TD2.COUNT.ASSET_GAS_CAN",					DB_TD2,	DT_U8,		offsetof( TitleData2, iCountWinScavengerMatches			) ),
		TD_ENTRY( "TD2.COUNT.ASSET_FRYING_PAN",					DB_TD2, DT_U8,		offsetof( TitleData2, iCountWinVersusMatches			) ),

		// ZOMBIE PANEL SEEN COUNTS
		TD_ENTRY( "TD2.COUNT.ZOMBIE_PANEL_INTRO",				DB_TD2, DT_U8,		offsetof( TitleData2, iCountZombiePanelIntro			) ),
		TD_ENTRY( "TD2.COUNT.ZOMBIE_PANEL_SMOKER",				DB_TD2, DT_U8,		offsetof( TitleData2, iCountZombiePanelSmoker			) ),
		TD_ENTRY( "TD2.COUNT.ZOMBIE_PANEL_BOOMER",				DB_TD2, DT_U8,		offsetof( TitleData2, iCountZombiePanelBoomer			) ),
		TD_ENTRY( "TD2.COUNT.ZOMBIE_PANEL_HUNTER",				DB_TD2, DT_U8,		offsetof( TitleData2, iCountZombiePanelHunter			) ),
		TD_ENTRY( "TD2.COUNT.ZOMBIE_PANEL_SPITTER",				DB_TD2, DT_U8,		offsetof( TitleData2, iCountZombiePanelSpitter			) ),
		TD_ENTRY( "TD2.COUNT.ZOMBIE_PANEL_JOCKEY",				DB_TD2, DT_U8,		offsetof( TitleData2, iCountZombiePanelJockey			) ),
		TD_ENTRY( "TD2.COUNT.ZOMBIE_PANEL_CHARGER",				DB_TD2, DT_U8,		offsetof( TitleData2, iCountZombiePanelCharger			) ),
#endif

		// END MARKER
		TD_ENTRY( NULL, DB_TD1, DT_U8, 0 )
	};

#undef TD_ENTRY

	return tdfd;
}

TitleAchievementsDescription_t const * CMatchTitle::DescribeTitleAchievements()
{
	static TitleAchievementsDescription_t tad[] =
	{
#include "swarm.xhelp.achtitledesc.txt"
		// END MARKER
		{ NULL, 0 }
	};

	return tad;
}

TitleAvatarAwardsDescription_t const * CMatchTitle::DescribeTitleAvatarAwards()
{
	static TitleAvatarAwardsDescription_t taad[] =
	{
#include "swarm.xhelp.avawtitledesc.txt"
		// END MARKER
		{ NULL, 0 }
	};

	return taad;
}

// Title leaderboards
KeyValues * CMatchTitle::DescribeTitleLeaderboard( char const *szLeaderboardView )
{
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
			KeyValues *pMapInfo = g_pMatchExtSwarm->GetMapInfoByBspName( pSettings, szSurvivalMap, &pMissionInfo );
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

	return NULL;
}


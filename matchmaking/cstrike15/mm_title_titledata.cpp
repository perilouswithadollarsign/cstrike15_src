//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "matchmaking/cstrike15/imatchext_cstrike15.h"
#include "inputsystem/iinputsystem.h"
#include "platforminputdevice.h"
#include "netmessages_signon.h"

#ifndef NO_STEAM
#include "steam/isteamuserstats.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar cl_titledataversionblock1( "cl_titledataversionblock1", "14", FCVAR_DEVELOPMENTONLY, "stats for console title data block1 i/o version." );
static ConVar cl_titledataversionblock2( "cl_titledataversionblock2", "8", FCVAR_DEVELOPMENTONLY, "stats for console title data block2 i/o version." );
static ConVar cl_titledataversionblock3( "cl_titledataversionblock3", "48", FCVAR_DEVELOPMENTONLY, "stats for console title data block3 i/o version." );

static TitleDataFieldsDescription_t const * PrepareTitleDataStorageDescription()
{

#if defined( _X360 )

#define TD_ENTRY( szName, nTD, eDataType, numBytesOffset ) \
	{ \
	TitleDataFieldsDescription_t aTDFD = { szName, TitleDataFieldsDescription_t::nTD, TitleDataFieldsDescription_t::eDataType, numBytesOffset }; \
	s_tdfd.AddToTail( aTDFD ); \
	if ( numBytesOffset >= XPROFILE_SETTING_MAX_SIZE ) \
		Warning( "\nnumBytesOffset %d is > XPROFILE_SETTING_MAX_SIZE in TD_ENTRY for %s\n\n", numBytesOffset, szName ); \
	}

#else

#define TD_ENTRY( szName, nTD, eDataType, numBytesOffset ) \
	{ \
	TitleDataFieldsDescription_t aTDFD = { szName, TitleDataFieldsDescription_t::nTD, TitleDataFieldsDescription_t::eDataType, numBytesOffset }; \
	s_tdfd.AddToTail( aTDFD ); \
}

#endif // _X360

	static CUtlVector< TitleDataFieldsDescription_t > s_tdfd;

#if defined( _X360 )

	// versioning info for the title data blocks
	char *pTitleDataBlock[3];
	pTitleDataBlock[0] = new char[30];
	pTitleDataBlock[1] = new char[30];
	pTitleDataBlock[2] = new char[30];
	Q_snprintf( pTitleDataBlock[0], 30, "TITLEDATA.BLOCK1.VERSION" );
	Q_snprintf( pTitleDataBlock[1], 30, "TITLEDATA.BLOCK2.VERSION" );
	Q_snprintf( pTitleDataBlock[2], 30, "TITLEDATA.BLOCK3.VERSION" );
	TD_ENTRY( pTitleDataBlock[0], DB_TD1, DT_uint16, offsetof( TitleData1, versionNumber ) );
	TD_ENTRY( pTitleDataBlock[1], DB_TD2, DT_uint16, offsetof( TitleData2, versionNumber ) );
	TD_ENTRY( pTitleDataBlock[2], DB_TD3, DT_uint16, offsetof( TitleData3, versionNumber ) );

	// stats
#define CFG( cppType, name ) \
	TD_ENTRY( "STATS.usr." #name, DB_TD1, DT_##cppType, offsetof( TitleData1, usrStats.name ) )
#include "xlast_csgo/inc_stats_usr.inc"
#undef CFG

	// loadouts
	//we are using an array so we can pack this info as tight as possible to fit into the 1K block
#define CFG( loadoutnum, equipmentnum ) \
	int numLoadouts = loadoutnum; \
	int numEquipmentSlots = equipmentnum;
#include "xlast_csgo/inc_loadouts_usr.inc"
#undef CFG

	int loadoutDataIndex = 0;
	for ( int team=0; team<2; ++team )
	{
		char teamName[10];
		if ( team == 0 )
			Q_snprintf( teamName, 10, "CT" );
		else
			Q_snprintf( teamName, 10, "T" );
		for (int i=0; i<numLoadouts; ++i)
		{
			for (int j=0; j<numEquipmentSlots; ++j)
			{
				char *loadoutName = new char[30];
				Q_snprintf( loadoutName, 30, "%s.LOAD%.1d.EQUIP%.1d.ID", teamName, i, j );
				TD_ENTRY( loadoutName, DB_TD3, DT_uint8, offsetof( TitleData3, loadoutData[ loadoutDataIndex++ ] ) );
				loadoutName = new char[30];
				Q_snprintf( loadoutName, 30, "%s.LOAD%.1d.EQUIP%.1d.QUANTITY", teamName, i, j );
				TD_ENTRY( loadoutName, DB_TD3, DT_uint8, offsetof( TitleData3, loadoutData[ loadoutDataIndex++ ] ) );
			}
			char *loadoutName = new char[30];
			Q_snprintf( loadoutName, 30, "%s.LOAD%.1d.PRIMARY", teamName, i );
			TD_ENTRY( loadoutName, DB_TD3, DT_uint8, offsetof( TitleData3, loadoutData[ loadoutDataIndex++ ] ) );
			loadoutName = new char[30];
			Q_snprintf( loadoutName, 30, "%s.LOAD%.1d.SECONDARY", teamName, i );
			TD_ENTRY( loadoutName, DB_TD3, DT_uint8, offsetof( TitleData3, loadoutData[ loadoutDataIndex++ ] ) );
			loadoutName = new char[30];
			Q_snprintf( loadoutName, 30, "%s.LOAD%.1d.FLAGS", teamName, i );
			TD_ENTRY( loadoutName, DB_TD3, DT_uint8, offsetof( TitleData3, loadoutData[ loadoutDataIndex++ ] ) );
		}
	}

	// medals
#define CFG( buffer ) \
	for ( int i=0; i< buffer; ++i ) \
	{ \
		char *pAwardedName = new char[ 20 ]; \
		Q_snprintf( pAwardedName, 20, "MEDALS.AWARDED%.3d", i ); \
		TD_ENTRY( pAwardedName, DB_TD2, DT_uint8, offsetof( TitleData2, CSMedalsAwarded[i] ) ) \
		char *pMedalInfoName = new char[ 25 ]; \
		Q_snprintf( pMedalInfoName, 25, "MEDALS.MEDALINFO%.3d", i ); \
		TD_ENTRY( pMedalInfoName, DB_TD2, DT_uint32, offsetof( TitleData2, CSMedalsMedalInfo[i] ) ) \
	}
#include "xlast_csgo/inc_medals_usr.inc"
#undef CFG

#endif // _X360 

#if defined( _GAMECONSOLE )

	// system convars
#define CFG( name, scfgType, cppType ) \
	TD_ENTRY( TITLE_DATA_PREFIX "CFG.sys." #name,	DB_TD3,	DT_##cppType,	offsetof( TitleData3, cvSystem.name ) )
#include "xlast_csgo/inc_gameconsole_settings_sys.inc"
#undef CFG

	// profile-specific convars
#define CFG( name, scfgType, cppType ) \
	TD_ENTRY( TITLE_DATA_PREFIX "CFG.usr." #name,	DB_TD3,	DT_##cppType,	offsetof( TitleData3, cvUser.name ) ) \
	TD_ENTRY( TITLE_DATA_PREFIX "CFG.usrSS." #name,	DB_TD3,	DT_##cppType,	offsetof( TitleData3, cvUserSS.name ) )
#include "xlast_csgo/inc_gameconsole_settings_usr.inc"
#include "xlast_csgo/inc_gameconsole_device_specific_settings_usr.inc"
#undef CFG


	// joystick bindings
#define ACTION( name )
#define BINDING( name, cppType ) \
	TD_ENTRY( TITLE_DATA_PREFIX "BINDING." #name, DB_TD3, DT_##cppType, offsetof( TitleData3, JoystickBindings.name ) )
	#include "xlast_csgo/inc_bindings_usr.inc"
#undef BINDING

#if defined( _PS3 )

	// PS3 also has keyboard bindings.
#define BINDING( name, cppType ) \
	TD_ENTRY( TITLE_DATA_PREFIX "BINDING." #name, DB_TD3, DT_##cppType, offsetof( TitleData3, JoystickBindings.name ) )
	#include "xlast_csgo/inc_ps3_key_bindings_usr.inc"
#undef BINDING

	// For PS3 we have two additional sets of button bindings one for Sharp Shooter, the other for Move.
	// We also have a few device specific convar settings.

	// PS Move specific bindings and settings
#define BINDING( name, cppType ) \
	TD_ENTRY( TITLE_DATA_PREFIX TITLE_DATA_DEVICE_MOVE_PREFIX "BINDING." #name, DB_TD3, DT_##cppType, offsetof( TitleData3, JoystickBindings.PSMove.name ) )
	#include "xlast_csgo/inc_bindings_usr.inc"
#undef BINDING

#define CFG( name, scfgType, cppType ) \
	TD_ENTRY( TITLE_DATA_PREFIX TITLE_DATA_DEVICE_MOVE_PREFIX "CFG.usr." #name,	DB_TD3,	DT_##cppType, offsetof( TitleData3, cvUser.PSMove.name ) )
	#include "xlast_csgo/inc_gameconsole_device_specific_settings_usr.inc"
#undef CFG

	// Sharp Shooter specific bindings and settings
#define BINDING( name, cppType ) \
	TD_ENTRY( TITLE_DATA_PREFIX TITLE_DATA_DEVICE_SHARP_SHOOTER_PREFIX "BINDING." #name, DB_TD3, DT_##cppType, offsetof( TitleData3, JoystickBindings.SharpShooter.name ) )
	#include "xlast_csgo/inc_bindings_usr.inc"
#undef BINDING

#define CFG( name, scfgType, cppType ) \
	TD_ENTRY( TITLE_DATA_PREFIX TITLE_DATA_DEVICE_SHARP_SHOOTER_PREFIX "CFG.usr." #name, DB_TD3, DT_##cppType, offsetof( TitleData3, cvUser.SharpShooter.name ) )
	#include "xlast_csgo/inc_gameconsole_device_specific_settings_usr.inc"
#undef CFG


#endif

#undef ACTION

	// Player Rankings by mode, controller, w/ optional history
	int rankIndex = 0;
	int numControllers = PlatformInputDevice::GetInputDeviceCountforPlatform();

	for ( int m = 0; m < ELOTitleData::NUM_GAME_MODES_ELO_RANKED; m++ )
	{
		for ( int c = 1; c <= numControllers; c++ )
		{
			char *pRankingName = new char[ 30 ];
			V_snprintf( pRankingName, 30, TITLE_DATA_PREFIX "ELO.MODE%d.CTR%d", m, c );

			TD_ENTRY( pRankingName, DB_TD3, DT_ELO, offsetof( TitleData3, playerRankingsData[ rankIndex ] ) );

			rankIndex++;
		}

		// Record the bracket and some info for calculating it. Only legal controllers are game console controllers.
		char *pBracketInfoName = new char[ 30 ];
		V_snprintf( pBracketInfoName, 30, TITLE_DATA_PREFIX"ELO.MODE%d.BRACKETINFO", m );
		TD_ENTRY( pBracketInfoName, DB_TD3, DT_uint16, offsetof( TitleData3, EloBracketInfo[ m ] ) );
	}

#endif // _GAMECONSOLE

#if defined ( _X360 )
	// matchmaking data
#define CFG( cppType, name ) \
	TD_ENTRY( "MMDATA.usr." #name, DB_TD3, DT_##cppType, offsetof( TitleData3, usrMMData.name ) )
#include "xlast_csgo/inc_mmdata_usr.inc"
#undef CFG
#endif // #if defined ( _X360 )

	// END MARKER
	TD_ENTRY( (const char*) NULL, DB_TD3, DT_0, 0 )

#undef TD_ENTRY

#if defined( _X360 )

	COMPILE_TIME_ASSERT( sizeof( TitleData1 ) < XPROFILE_SETTING_MAX_SIZE );
	COMPILE_TIME_ASSERT( sizeof( TitleData2 ) < XPROFILE_SETTING_MAX_SIZE );
	COMPILE_TIME_ASSERT( sizeof( TitleData3 ) < XPROFILE_SETTING_MAX_SIZE );

#endif

	return s_tdfd.Base();
}

TitleDataFieldsDescription_t const * CMatchTitle::DescribeTitleDataStorage()
{
	static TitleDataFieldsDescription_t const *s_pTDFD = PrepareTitleDataStorageDescription();
	return s_pTDFD;
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

TitleDlcDescription_t const * CMatchTitle::DescribeTitleDlcs()
{
	static TitleDlcDescription_t tdlcs[] =
	{
		//{ PORTAL2_DLCID_COOP_BOT_SKINS,		PORTAL2_DLC_APPID_COOP_BOT_SKINS,	PORTAL2_DLC_PKGID_COOP_BOT_SKINS,	"DLC.0x12" },
		//{ PORTAL2_DLCID_COOP_BOT_HELMETS,	PORTAL2_DLC_APPID_COOP_BOT_HELMETS,	PORTAL2_DLC_PKGID_COOP_BOT_HELMETS,	"DLC.0x13" },
		//{ PORTAL2_DLCID_COOP_BOT_ANTENNA,	PORTAL2_DLC_APPID_COOP_BOT_ANTENNA,	PORTAL2_DLC_PKGID_COOP_BOT_ANTENNA,	"DLC.0x14" },
		// END MARKER
		{ 0, 0, 0 }
	};

	return tdlcs;
}

// Title leaderboards
KeyValues * CMatchTitle::DescribeTitleLeaderboard( char const *szLeaderboardView )
{
#if !defined( NO_STEAM )
	if ( StringAfterPrefix( szLeaderboardView, "WINS_" ) )
	{
		if ( IsPC() || IsPS3() )
		{
			KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
				" :score wins_ratio "								// :score is the leaderboard value mapped to game name "besttime"
				" :payloadformat { "										// This describes the payload format.
					" payload0 { "
						" :score total_wins"
						" :format int "
						" :upload sum "
					" } "
					" payload1 { "
						" :score total_losses "
						" :format int "
						" :upload sum "
					" } "
					" payload2 { "
						" :score win_as_ct "
						" :format int "
						" :upload sum "
					" } "
					" payload3 { "
						" :score win_as_t "
						" :format int "
						" :upload sum "
					" } "
					" payload4 { "
						" :score loss_as_ct "
						" :format int "
						" :upload sum "
					" } "
					" payload5 { "
						" :score loss_as_t "
						" :format int "
						" :upload sum "
					" } "
				" } "
				);

			pSettings->SetString( ":scoreformula", "( payload0 / max( payload0 + payload1, 1 ) ) * ( min( payload0 + payload1, 20 ) / 20 ) * 10000000" );
			pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
			pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeNumeric );	// Note: this is actually 1/100th seconds type, Steam change pending
			pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodForceUpdate );	// Upload method when writing to leaderboard

			return pSettings;
		}
	}
	else if ( StringAfterPrefix( szLeaderboardView, "CS_" ) )
	{
		if ( IsPC() || IsPS3() )
		{
			KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
				" :score average_contribution "								// :score is the leaderboard value mapped to game name "besttime"
				" :payloadformat { "										// This describes the payload format.
					" payload0 { "
						" :score mvp_awards"
						" :format int "
						" :upload sum "
					" } "
					" payload1 { "
						" :score rounds_played "
						" :format int "
						" :upload sum "
					" } "
					" payload2 { "
						" :score total_contribution "
						" :format int "
						" :upload sum "
					" } "
					" payload3 { "
						" :score kills "
						" :format int "
						" :upload sum "
					" } "
					" payload4 { "
						" :score deaths "
						" :format int "
						" :upload sum "
					" } "
					" payload5 { "
						" :score damage "
						" :format int "
						" :upload sum "
					" } "
				" } "
				);

			pSettings->SetString( ":scoreformula", "( payload2 /  max( payload1, 1 ) )" );
			pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
			pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeNumeric );	// Note: this is actually 1/100th seconds type, Steam change pending
			pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodForceUpdate );	// Upload method when writing to leaderboard

			return pSettings;
		}
	}
	else if ( StringAfterPrefix( szLeaderboardView, "KD_" ) )
	{
		if ( IsPC() || IsPS3() )
		{
			KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
				" :score kd_ratio "														// :score is the leaderboard value mapped to game name "besttime"
				" :payloadformat { "													// This describes the payload format.
					" payload0 { "
						" :score kills"
						" :format int "
						" :upload sum "
					" } "
					" payload1 { "
						" :score deaths "
						" :format int "
						" :upload sum "
					" } "
					" payload2 { "
						" :score rounds_played "
						" :format int "
						" :upload sum "
					" } "
					" payload3 { "
						" :score shots_fired "
						" :format int "
						" :upload sum "
					" } "
					" payload4 { "
						" :score head_shots "
						" :format int "
						" :upload sum "
					" } "
					" payload5 { "
						" :score shots_hit "
						" :format int "
						" :upload sum "
					" } "
				" } "
				);

			pSettings->SetString( ":scoreformula", "( payload0 / max( payload1, 1 ) ) * ( min( payload2, 20 ) / 20 ) * 10000000" );
			pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
			pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeNumeric );	// Note: this is actually 1/100th seconds type, Steam change pending
			pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodForceUpdate );	// Upload method when writing to leaderboard

			return pSettings;
		}
	}
	else if ( StringAfterPrefix( szLeaderboardView, "STARS_" ) )
	{
		if ( IsPC() || IsPS3() )
		{
			KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
				" :score numstars "														// :score is the leaderboard value mapped to game name "besttime"
				" :scoresum 1 "
				" :payloadformat { "													// This describes the payload format.
					" payload0 { "
						" :score bombs_planted "
						" :format int "
						" :upload sum "
					" } "
					" payload1 { "
						" :score bombs_detonated "
						" :format int "
						" :upload sum "
					" } "
					" payload2 { "
						" :score bombs_defused "
						" :format int "
						" :upload sum "
					" } "
					" payload3 { "
						" :score hostages_rescued "
						" :format int "
						" :upload sum "
					" } "
				" } "
				);

			pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
			pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeNumeric );	// Note: this is actually 1/100th seconds type, Steam change pending
			pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodKeepBest );	// Upload method when writing to leaderboard

			return pSettings;
		}
	}
	else if ( StringAfterPrefix( szLeaderboardView, "GP_" ) )
	{
		if ( IsPC() || IsPS3() )
		{
			KeyValues *pSettings = KeyValues::FromString( "SteamLeaderboard",
				" :score num_rounds "														// :score is the leaderboard value mapped to game name "besttime"
				" :scoresum 1 "
				" :payloadformat { "													// This describes the payload format.
					" payload0 { "
						" :score time_played "
						" :format uint64 "
						" :upload sum "
					" } "
					" payload1 { "
						" :score time_played_ct "
						" :format uint64 "
						" :upload sum "
					" } "
					" payload2 { "
						" :score time_played_t "
						" :format uint64 "
						" :upload sum "
					" } "
					" payload3 { "
						" :score total_medals "
						" :format int "
						" :upload last "  // the last value written is the authoritative value of total achievement medals unlocked
					" } "
				" } "
				);

			pSettings->SetInt( ":sort", k_ELeaderboardSortMethodDescending );			// Sort order when fetching and displaying leaderboard data
			pSettings->SetInt( ":format", k_ELeaderboardDisplayTypeNumeric );	// Note: this is actually 1/100th seconds type, Steam change pending
			pSettings->SetInt( ":upload", k_ELeaderboardUploadScoreMethodKeepBest );	// Upload method when writing to leaderboard

			return pSettings;
		}
	}

#endif // !NO_STEAM

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

		if ( IsPC() || IsPS3() )
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


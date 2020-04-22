//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "mm_title_richpresence.h"
#include "portal2.spa.h"
#include "matchmaking/portal2/imatchext_portal2.h"
#include "filesystem/ixboxinstaller.h"

#include "filesystem.h"
#include "vstdlib/random.h"
#include "checksum_crc.h"
#include "fmtstr.h"

#ifndef _PS3
#include <locale>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar mm_matchmaking_version( "mm_matchmaking_version", "9" );
ConVar mm_matchmaking_dlcsquery( "mm_matchmaking_dlcsquery", "2" );

class CMatchTitleGameSettingsMgr : public IMatchTitleGameSettingsMgr
{
public:
	// Extends server game details
	virtual void ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest );

	// Adds the essential part of game details to be broadcast
	virtual void ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings );

	// Extends game settings update packet for lobby transition,
	// either due to a migration or due to an endgame condition
	virtual void ExtendGameSettingsForLobbyTransition( KeyValues *pSettings, KeyValues *pSettingsUpdate, bool bEndGame );

	// Adds data for datacenter reporting
	virtual void ExtendDatacenterReport( KeyValues *pReportMsg, char const *szReason );


	// Rolls up game details for matches grouping
	virtual KeyValues * RollupGameDetails( KeyValues *pDetails, KeyValues *pRollup, KeyValues *pQuery );


	// Defines session search keys for matchmaking
	virtual KeyValues * DefineSessionSearchKeys( KeyValues *pSettings );

	// Defines dedicated server search key
	virtual KeyValues * DefineDedicatedSearchKeys( KeyValues *pSettings );


	// Initializes full game settings from potentially abbreviated game settings
	virtual void InitializeGameSettings( KeyValues *pSettings, char const *szReason );

	// Extends game settings update packet before it gets merged with
	// session settings and networked to remote clients
	virtual void ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys );

	// Prepares system for session creation
	virtual KeyValues * PrepareForSessionCreate( KeyValues *pSettings );


	// Executes the command on the session settings, this function on host
	// is allowed to modify Members/Game subkeys and has to fill in modified players KeyValues
	// When running on a remote client "ppPlayersUpdated" is NULL and players cannot
	// be modified
	virtual void ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated );

	// Prepares the lobby for game or adjust settings of new players who
	// join a game in progress, this function is allowed to modify
	// Members/Game subkeys and has to fill in modified players KeyValues
	virtual void PrepareLobbyForGame( KeyValues *pSettings, KeyValues **ppPlayersUpdated );

	// Prepares the host team lobby for game adjusting the game settings
	// this function is allowed to prepare modification package to update
	// Game subkeys.
	// Returns the update/delete package to be applied to session settings
	// and pushed to dependent two sesssion of the two teams.
	virtual KeyValues * PrepareTeamLinkForGame( KeyValues *pSettingsLocal, KeyValues *pSettingsRemote );

	// Prepares the client lobby for migration
	// this function is called when the client session is still in the state
	// of "client" while handling the original host disconnection and decision
	// has been made that local machine will be elected as new "host"
	// Returns NULL if migration should proceed normally
	// Returns [ kvroot { "error" "n/a" } ] if migration should be aborted.
	virtual KeyValues * PrepareClientLobbyForMigration( KeyValues *pSettingsLocal, KeyValues *pMigrationInfo );

	// Prepares the session for server disconnect
	// this function is called when the session is still in the active gameplay
	// state and while localhost is handling the disconnection from game server.
	// Returns NULL to allow default flow
	// Returns [ kvroot { "disconnecthdlr" "<opt>" } ] where <opt> can be:
	//		"destroy" : to trigger a disconnection error and destroy the session
	//		"lobby" : to initiate a "salvaging" lobby transition
	virtual KeyValues * PrepareClientLobbyForGameDisconnect( KeyValues *pSettingsLocal, KeyValues *pDisconnectInfo );

	// Validates if client profile can set a stat or get awarded an achievement
	virtual bool AllowClientProfileUpdate( KeyValues *kvUpdate );
};

CMatchTitleGameSettingsMgr g_MatchTitleGameSettingsMgr;
IMatchTitleGameSettingsMgr *g_pIMatchTitleGameSettingsMgr = &g_MatchTitleGameSettingsMgr;


//
// Mission information block
//

// Transfer mission info keys from mission manifest into key values
#define NO_MISSION_INFO
#ifdef NO_MISSION_INFO
// Portal 2 doesn't have map versioning like L4D
#define TransferMissionInformationToInfo( ... ) ((void)0)
#else
static void TransferMissionInformationToInfo( char const *szMissionName, KeyValues *pInfo )
{
	KeyValues *pNewMission = NULL;
	if ( !pInfo )
		return;

	if ( !szMissionName || !*szMissionName )
	{
		pInfo->SetString( "version",		pNewMission->GetString( "version", "1" ) );
#ifndef _X360
		pInfo->SetString( "builtin",		pNewMission->GetString( "builtin", "1" ) );
		pInfo->SetString( "displaytitle",	pNewMission->GetString( "displaytitle", "#L4D360UI_Campaign_Any" ) );
		pInfo->SetString( "author",			pNewMission->GetString( "author", "" ) );
		pInfo->SetString( "website",		pNewMission->GetString( "website", "" ) );
#endif
		return;
	}

	// Determine the new MissionInfo
	/*
	if ( KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions() )
	{
		pNewMission = pAllMissions->FindKey( szMissionName );
	}
	*/

	pInfo->SetString( "version",		pNewMission->GetString( "version", "" ) );
#ifndef _X360
	pInfo->SetString( "builtin",		pNewMission->GetString( "builtin", "1" ) );
	pInfo->SetString( "displaytitle",	pNewMission->GetString( "displaytitle", "" ) );
	pInfo->SetString( "author",			pNewMission->GetString( "author", "" ) );
	pInfo->SetString( "website",		pNewMission->GetString( "website", "" ) );
#endif
}
#endif

// Determine the DLC mask for the current settings
static uint64 DetermineDlcRequiredMask( KeyValues *pAggregateSettings )
{
	if ( !Q_stricmp( pAggregateSettings->GetString( "game/mode" ), "coop_challenge" ) ||
		 !Q_stricmp( pAggregateSettings->GetString( "options/play" ), "challenge" ) )
		return PORTAL2_DLCID_RETAIL_DLC1;

	return 0;
}


//
// Implementation of CMatchTitleGameSettingsMgr
//

// Adds the essential part of game details to be broadcast
void CMatchTitleGameSettingsMgr::ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings )
{
	static KeyValues *pkvExt = KeyValues::FromString(
		"settings",
		" game { "
			" map #empty# "
			" mode #empty# "
			" state #empty# "
			" mp_coop_start #empty# "
#ifndef NO_MISSION_INFO
			// Portal 2 doesn't have map versioning like L4D
			" missioninfo { "
				" version #empty# "
#ifndef _X360
				" builtin #empty# "
				" displaytitle #empty# "
				" author #empty# "
				" website #empty# "
#endif
#endif // NO_MISSION_INFO
			" } "
		" } "
		);

	pDetails->MergeFrom( pkvExt, KeyValues::MERGE_KV_UPDATE );

	if ( szReason && !Q_stricmp( szReason, "reserve" ) )
	{
		// Single player reservation might need to pass the savegame name
		if ( char const *szSaveGame = pFullSettings->GetString( "game/save", NULL ) )
		{
			pDetails->SetString( "game/save", szSaveGame );
		}

		// For coop reservation we have to analyze all DLCs of the session machines
		// and set special keys for the server reservation
		const char *szGameMode = pFullSettings->GetString( "game/mode" );
		if ( !V_stricmp( szGameMode, "coop" ) || !V_stricmp( szGameMode, "coop_challenge" ) )
		{
			uint64 uiDLCor = pFullSettings->GetUint64( "members/machine0/dlcmask" ) | pFullSettings->GetUint64( "members/machine1/dlcmask" );
			pDetails->SetInt( "gameextras/skins", ( uiDLCor & PORTAL2_DLCID_COOP_BOT_SKINS ) ? 1 : -1 );
			pDetails->SetInt( "gameextras/helmets", ( uiDLCor & PORTAL2_DLCID_COOP_BOT_HELMETS ) ? 1 : -1 );
			pDetails->SetInt( "gameextras/antenna", ( uiDLCor & PORTAL2_DLCID_COOP_BOT_ANTENNA ) ? 1 : -1 );
		}
	}
}

// Extends server game details
void CMatchTitleGameSettingsMgr::ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest )
{
	// Query server info
	INetSupport::ServerInfo_t si;
	g_pMatchExtensions->GetINetSupport()->GetServerInfo( &si );

	// Determine map name
	char const *szMapName = si.m_szMapName;
	if ( !si.m_bActive )
	{
		if ( IVEngineClient *pIVEngineClient = g_pMatchExtensions->GetIVEngineClient() )
		{
			szMapName = pIVEngineClient->GetLevelNameShort();
		}
	}
	if ( !szMapName )
		szMapName = "";

	// Server is always in game
	pDetails->SetString( "game/state", "game" );

	//
	// Set game mode based on convars
	//
	pDetails->SetString( "game/mode", "coop" );

	//
	// Determine game campaign and map info
	//
	pDetails->SetString( "game/map", szMapName );

	//
	// Determine required dlc mask
	//
	pDetails->SetUint64( "game/dlcrequired", DetermineDlcRequiredMask( pDetails ) );
}

// Extends game settings update packet for lobby transition,
// either due to a migration or due to an endgame condition
void CMatchTitleGameSettingsMgr::ExtendGameSettingsForLobbyTransition( KeyValues *pSettings, KeyValues *pSettingsUpdate, bool bEndGame )
{
	pSettingsUpdate->SetString( "game/state", "lobby" );
}

// Adds data for datacenter reporting
void CMatchTitleGameSettingsMgr::ExtendDatacenterReport( KeyValues *cmd, char const *szReason )
{
#ifdef _X360
	if ( XBX_GetPrimaryUserId() == XBX_INVALID_USER_ID )
		return;
	if ( !XBX_GetNumGameUsers() || XBX_GetPrimaryUserIsGuest() )
		return;

	IPlayerLocal *pLocalPlayer = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() );
	if ( !pLocalPlayer )
		return;

	// Achievements info
	uint64 uiAchMask1 = 0;	// 100 bits for achievements
	uint64 uiAchMask2 = 0;	// 27 bits for asset awards
	{
		KeyValues *kvAchInfo = new KeyValues( "", "@achievements", 0, "@awards", 0 );
		KeyValues::AutoDelete autodelete_kvAchInfo( kvAchInfo );
		pLocalPlayer->GetAwardsData( kvAchInfo );
		for ( KeyValues *val = kvAchInfo->FindKey( "@achievements" )->GetFirstValue(); val; val = val->GetNextValue() )
		{
			int iVal = val->GetInt( "", 0 );
			if ( iVal <= 0 )
				continue;
			else if ( iVal < 64 )
				uiAchMask1 |= ( 1ull << iVal );
			else if ( iVal <= 100 )
				uiAchMask2 |= ( 1ull << ( iVal - 64 ) );
		}
		for ( KeyValues *val = kvAchInfo->FindKey( "@awards" )->GetFirstValue(); val; val = val->GetNextValue() )
		{
			int iVal = val->GetInt( "", 0 );
			if ( iVal <= 0 )
				continue;
			else if ( iVal < ( 128 - 100 ) )
				uiAchMask2 |= ( 1ull << ( iVal + 100 - 64 ) );
		}
	}
	cmd->SetUint64( "ach1", uiAchMask1 );
	cmd->SetUint64( "ach2", uiAchMask2 );

	if ( !V_stricmp( szReason, "datarequest" ) )
	{
		// Add game information
		TitleData1 * td1 = ( TitleData1 * ) pLocalPlayer->GetPlayerTitleData( TitleDataFieldsDescription_t::DB_TD1 );
		TitleData3 * td3 = ( TitleData3 * ) pLocalPlayer->GetPlayerTitleData( TitleDataFieldsDescription_t::DB_TD3 );

		// sp progress
		cmd->SetInt( "map_s", td1->uiSinglePlayerProgressChapter );

		// coop completion
		int numMapBitsFields = sizeof( td1->coop.mapbits ) / sizeof( uint64 );
		for ( int imap = 0; imap < numMapBitsFields; ++ imap )
		{
			cmd->SetUint64( CFmtStr( "map_%d", imap ), ( reinterpret_cast< uint64 * >( td1->coop.mapbits ) )[imap] );
		}

		if ( td3->cvUser.version )
		{
			// profile settings
			cmd->SetFloat( "cfg_pitch", td3->cvUser.joy_pitchsensitivity );
			cmd->SetFloat( "cfg_yaw", td3->cvUser.joy_yawsensitivity );
			cmd->SetInt( "cfg_joy", td3->cvUser.joy_cfg_preset );
			cmd->SetInt( "cfg_bit", td3->cvUser.bitfields[0] );
		}

		if ( td3->cvSystem.version )
		{
			// audio/video
			cmd->SetFloat( "sys_vol", td3->cvSystem.volume );
			cmd->SetFloat( "sys_mus", td3->cvSystem.snd_musicvolume );
			cmd->SetFloat( "sys_gam", td3->cvSystem.mat_monitorgamma );
			cmd->SetInt( "sys_ssm", td3->cvSystem.ss_splitmode );
			cmd->SetInt( "sys_bit", td3->cvSystem.bitfields[0] );
		}

		if ( g_pXboxInstaller )
		{
			cmd->SetInt( "inst",
				( g_pXboxInstaller->IsFullyInstalled() ? 1 : 0 ) |
				( g_pXboxInstaller->IsInstallEnabled() ? 2 : 0 )
				);
		}
	}
#endif
}

// Adds a lowercased string into crc
void AppendToRollup( char const *sz, CRC32_t &u )
{
	if ( !sz )
		return;
	
	char const *p1 = sz;
	char const *p2 = p1;
	while ( *p2 )
	{
		while ( *p2 && !isupper( *p2 ) )
		{
			++ p2;
		}
		
		if ( p2 > p1 )
		{
			CRC32_ProcessBuffer( &u, p1, p2 - p1 );
		}
		
		if ( *p2 )
		{
			char ch = tolower( *p2 );
			++ p2;
			CRC32_ProcessBuffer( &u, &ch, sizeof( ch ) );
		}

		p1 = p2;
	}
}

// Rolls up game details for matches grouping
KeyValues * CMatchTitleGameSettingsMgr::RollupGameDetails( KeyValues *pDetails, KeyValues *pRollup, KeyValues *pQuery )
{
	if ( !pDetails && !pRollup )
		return NULL;

	MEM_ALLOC_CREDIT();
	if ( !pDetails )
	{
		// Check if the rollup is for a not installed addon with website
		if ( pRollup->GetInt( "game/missioninfo/builtin", 0 ) )
			return NULL;	// builtin, not interested

		char const *szWebsite = pRollup->GetString( "game/missioninfo/website", NULL );
		if ( !szWebsite || !*szWebsite )
			return NULL;	// no website, not interested

		int const nInstalledVersion = 1; // g_pMatchExtSwarm->GetAllMissions()->GetInt(
			// CFmtStr( "%s/version", pRollup->GetString( "game/campaign" ) ), -1 );
		if ( pRollup->GetInt( "game/missioninfo/version", 0 ) <= nInstalledVersion )
			return NULL;	// addon installed with same or newer version, not interested

		// Zero out games information
		if ( KeyValues *pAgg = pRollup->FindKey( "rollup" ) )
		{
			pRollup->RemoveSubKey( pAgg );
			pAgg->deleteThis();
		}

		return pRollup;
	}

	if ( !pRollup )
	{
		// Determine the game mode
		char const *szGameMode = pDetails->GetString( "game/mode" );
		if ( !szGameMode || !*szGameMode )
			return NULL;

		// Determine campaign name
		char const *szCampaign = pDetails->GetString( "game/map" );
		if ( !szCampaign || !*szCampaign )
			return NULL;

		// Prepare the rollup
		pRollup = new KeyValues( "rollup" );
		CRC32_t uRollupKey = 0;
		CRC32_Init( &uRollupKey );

		// Game/mode
		pRollup->SetString( "game/mode", szGameMode );
		AppendToRollup( szGameMode, uRollupKey );

		// Game/campaign
		pRollup->SetString( "game/map", szCampaign );
		AppendToRollup( szCampaign, uRollupKey );

		// Game/state, only if queried for specific game state
		if ( char const *szGameState = pQuery->GetString( "game/state", NULL ) )
		{
			char const *szState = pDetails->GetString( "game/state", NULL );
			if ( szState && *szState )
			{
				pRollup->SetString( "game/state", szState );
				// AppendToRollup( szState, uRollupKey ); <- state doesn't affect the rollup
			}
		}

		// Let the aggregation system know the key
		pRollup->SetUint64( "rollupkey", uRollupKey );
		return pRollup;
	}

	// We need to rollup the formula
	if ( pDetails && pRollup )
	{
		// Add the rollup mission info
		int iVersion = pRollup->GetInt( "game/missioninfo/version", -1 ), iVersionD = pDetails->GetInt( "game/missioninfo/version" );
		if ( iVersionD > iVersion )
		{
			pRollup->FindKey( "game/missioninfo", true )->MergeFrom( pDetails->FindKey( "game/missioninfo" ), KeyValues::MERGE_KV_UPDATE );
		}

		// Add the rollup formula
		char const *szGameState = pDetails->GetString( "game/state", "game" );
		if ( Q_stricmp( szGameState, "lobby" ) && Q_stricmp( szGameState, "game" ) )
			szGameState = "game"; // force all other states like "finale" to be game
		CFmtStr strRollupKey( "rollup/%s", szGameState );
		pRollup->SetInt( strRollupKey, 1 + pRollup->GetInt( strRollupKey ) );

		// Rolled up
		return pRollup;
	}

	return NULL;
}

// Defines dedicated server search key
KeyValues * CMatchTitleGameSettingsMgr::DefineDedicatedSearchKeys( KeyValues *pSettings )
{
	// No dedicated servers support
	return NULL;
}

// Helper function to set filter for BuiltIn criteria on PC
static void DefineSessionSearchKeys_BuiltIn( KeyValues *pSettings, KeyValues *pSearchKeys )
{
	return; // don't set additional keys for Portal 2

	MEM_ALLOC_CREDIT();
	char const *szValue = pSettings->GetString( "game/missioninfo/builtin", NULL );

	// Check if we need to force "official" setting
	char const *szCampaign = pSettings->GetString( "game/campaign" );
	if ( !*szCampaign && IsPC() )
	{
		// Any campaign - candidate for "official" restriction
		char const *szAction = pSettings->GetString( "options/action" );
		if ( !Q_stricmp( szAction, "quickmatch" ) ||
			 !Q_stricmp( szAction, "custommatch" ) )
		{
			szValue = "official";	// matchmaking on "ANY" = "official"
		}

		// Team matchmaking also means "official"
		if ( StringHasPrefix( pSettings->GetString( "game/mode" ), "team" ) )
		{
			szValue = "official";
		}
	}

	if ( szValue )
	{
		// Different values mean different search criteria:
		if ( !Q_stricmp( szValue, "addon" ) )
			pSearchKeys->SetInt( "Filter=/game:missioninfo:builtin", 0 );
		else if ( !Q_stricmp( szValue, "official" ) )
			pSearchKeys->SetInt( "Filter=/game:missioninfo:builtin", 1 );
		else if ( !Q_stricmp( szValue, "installedaddon" ) )
			pSearchKeys->SetInt( "Filter=/game:missioninfo:builtin", 0 );
		else if ( !Q_stricmp( szValue, "notinstalledaddon" ) )
		{
			pSearchKeys->SetInt( "Filter=/game:missioninfo:builtin", 0 );
			/*
			KeyValues *pAllMissions = g_pMatchExtSwarm->GetAllMissions();
			for ( KeyValues *pMission = pAllMissions ? pAllMissions->GetFirstTrueSubKey() : NULL; pMission; pMission = pMission->GetNextTrueSubKey() )
			{
				if ( !pMission->GetBool( "builtin" ) )
					pSearchKeys->FindKey( "Filter<>", true )->AddSubKey(
					new KeyValues( "game:campaign", NULL, pMission->GetString( "name" ) ) );
			}
			*/
			pSearchKeys->SetString( "Filter<>/game:missioninfo:website", "" );
		}
	}
}

// Defines session search keys for matchmaking
KeyValues * CMatchTitleGameSettingsMgr::DefineSessionSearchKeys( KeyValues *pSettings )
{
	MEM_ALLOC_CREDIT();
	KeyValues *pResult = new KeyValues( "SessionSearch" );

	pResult->SetInt( "numPlayers", pSettings->GetInt( "members/numPlayers", XBX_GetNumGameUsers() ) );
	
	if ( IsX360() )
	{

		if ( char const *szValue = pSettings->GetString( "game/mode", NULL ) )
		{
			static ContextValue_t values[] = {
				{ "coop",			SESSION_MATCH_QUERY_COOP },
				{ "coop_challenge",	SESSION_MATCH_QUERY_COOP },
				{ NULL,				0xFFFF },
			};

			pResult->SetInt( "rule", values->ScanValues( szValue ) );
		}

		// Set the matchmaking version
		pResult->SetInt( CFmtStr( "Properties/%d", PROPERTY_MMVERSION ), mm_matchmaking_version.GetInt() );

#ifdef _X360
		// Set the installed DLCs masks
		uint64 uiDlcsMask = g_pMatchFramework->GetMatchSystem()->GetDlcManager()->GetDataInfo()->GetUint64( "@info/installed" );
		for ( int k = 1; k <= mm_matchmaking_dlcsquery.GetInt(); ++ k )
		{
			pResult->SetInt( CFmtStr( "Properties/%d", PROPERTY_INSTALLED_DLC1 - 1 + k ), !!( uiDlcsMask & ( 1ull << k ) ) );
		}
		pResult->SetInt( "dlc1", PROPERTY_MMVERSION + 1 );
		pResult->SetInt( "dlcN", PROPERTY_MMVERSION + mm_matchmaking_dlcsquery.GetInt() );
#endif

		// X_CONTEXT_GAME_TYPE 
		pResult->SetInt( CFmtStr( "Contexts/%d", X_CONTEXT_GAME_TYPE ), X_CONTEXT_GAME_TYPE_STANDARD );

		// X_CONTEXT_GAME_MODE 
		if ( char const *szValue = pSettings->GetString( "game/mode", NULL ) )
		{
			pResult->SetInt( CFmtStr( "Contexts/%d", X_CONTEXT_GAME_MODE ), g_pcv_CONTEXT_GAME_MODE->ScanValues( szValue ) );
		}

	}
	else
	{

		if ( char const *szValue = pSettings->GetString( "game/state", NULL ) )
		{
			pResult->SetString( "Filter=/game:state", szValue );
		}

		if ( char const *szValue = pSettings->GetString( "game/mode", NULL ) )
		{
			pResult->SetString( "Filter=/game:mode", szValue );
		}

		// Set the mp_coop_start search criteria if requested
		if ( KeyValues *kv_mp_coop_start = pSettings->FindKey( "game/mp_coop_start" ) )
		{
			pResult->SetInt( "Near/game:mp_coop_start", kv_mp_coop_start->GetInt() );
		}

		// When matchmaking on Steam, only matchmake within same platform (PS3-PS3 or PC-PC)
		if ( char const *szPlatform = pSettings->GetString( "game/platform", NULL ) )
		{
			pResult->SetString( "Filter=/game:platform", szPlatform );
		}

		// BuiltIn search keys
		DefineSessionSearchKeys_BuiltIn( pSettings, pResult );

	}

	return pResult;
}

#ifdef _DEBUG
ConVar mm_test_slots( "mm_test_slots", "0", FCVAR_DEVELOPMENTONLY, "Force the game to support a different number of max slots.\n" );
#endif

template < typename T >
static T AggregatePolicyAvg( int iCount, T iValue, T const *iArg )
{
	if ( iArg )
		return iValue + *iArg;
	else
		return ( iCount > 0 ) ? ( iValue / iCount ) : iValue;
}

template < typename T >
static T AggregatePolicyMax( int iCount, T iValue, T const *iArg )
{
	if ( iArg )
		return MAX( iValue, *iArg );
	else
		return iValue;
}

template < typename T >
static T AggregatePolicyMin( int iCount, T iValue, T const *iArg )
{
	if ( iArg )
		return MIN( iValue, *iArg );
	else
		return iValue;
}

template < typename T >
static T AggregateMembersField( KeyValues *pGameSettings, char const *szField, T iDefault, T ( *pfnPolicy )( int iCount, T iValue, T const *iArg ), T (KeyValues::*pfnGet)( const char *szKey, T getDefault ) )
{
	int iCount = 0;
	T iAggregate = iDefault;

	for ( int iMachine = 0, numMachines = pGameSettings->GetInt( "members/numMachines" ); iMachine < numMachines; ++ iMachine )
	{
		KeyValues *pMachine = pGameSettings->FindKey( CFmtStr( "members/machine%d", iMachine ) );
		for ( int iPlayer = 0, numPlayers = pMachine->GetInt( "numPlayers" ); iPlayer < numPlayers; ++ iPlayer )
		{
			KeyValues *pPlayer = pMachine->FindKey( CFmtStr( "player%d", iPlayer ) );
			if ( !pPlayer )
				continue;

			T iField = (pPlayer->*pfnGet)( CFmtStr( "game/%s", szField ), iDefault );
			iAggregate = (*pfnPolicy)( iCount, iAggregate, &iField );
			++ iCount;
		}
	}

	return (*pfnPolicy)( iCount, iAggregate, NULL );
}

void UpdateAggregateMembersSettings( KeyValues *pFullGameSettings, KeyValues *pUpdate )
{
	bool bSplitscreen = ( XBX_GetNumGameUsers() > 1 );
	pUpdate->SetInt( "game/mp_coop_start", AggregateMembersField<int>( pFullGameSettings, "mp_coop_start", bSplitscreen ? 0 : 1, 
																	   bSplitscreen ? (int (*)(int,int,int const*))AggregatePolicyMax<int> : (int (*)(int,int,int const*))AggregatePolicyMin<int>, 
																	   &KeyValues::GetInt ) );
}

void InitializeDlcMachineSettings( KeyValues *pSettings, KeyValues *pMachine )
{
	if ( !pMachine )
		return; // don't have required machine to update

	IDlcManager *pDlcManager = g_pMatchFramework->GetMatchSystem()->GetDlcManager();

	pDlcManager->RequestDlcUpdate();
	pDlcManager->IsDlcUpdateFinished( true ); // force synchronous update

	uint64 uiDlcMaskDiscovered = pDlcManager->GetDataInfo()->GetUint64( "@info/installed" );
	uiDlcMaskDiscovered &= PORTAL2_DLC_ALLMASK;

	DevMsg( "InitializeDlcMachineSettings: [mask:0x%llX]\n", uiDlcMaskDiscovered );
	if ( uiDlcMaskDiscovered )
	{
		pMachine->SetUint64( "dlcmask", pMachine->GetUint64( "dlcmask" ) | uiDlcMaskDiscovered );
	}
}

void InitializeMemberSettings( KeyValues *pSettings )
{
	IPlayerLocal *playerPrimary = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() );
	if ( !playerPrimary )
		return;

	//
	// Find local machine in session settings
	//
	KeyValues *pMachine = NULL;
	KeyValues *pPlayer = SessionMembersFindPlayer( pSettings, playerPrimary->GetXUID(), &pMachine );
	if ( !pMachine )
		return;

	// Initialize DLC for the machine
	InitializeDlcMachineSettings( pSettings, pMachine );

	//
	// Set title-specific data
	//
	for ( int jj = 0; jj < XBX_GetNumGameUsers(); ++ jj )
	{
		int iController = XBX_GetUserId( jj );
		IPlayerLocal *player = g_pPlayerManager->GetLocalPlayer( iController );
		if ( !player )
			continue;

		pPlayer = pMachine->FindKey( CFmtStr( "player%d", jj ) );
		if ( !pPlayer )
			continue;

		// Set mp_coop_start
		TitleData1 const td = * ( TitleData1 const * ) player->GetPlayerTitleData( 0 );
		COMPILE_TIME_ASSERT( TitleData1::CoopData_t::mp_coop_start < 8 );
		pPlayer->SetInt( "game/mp_coop_start", ( td.coop.mapbits[0] != 0 || td.coop.mapbits[1] != 0 ) ); // As long as user completed any of the first 64 maps consider his training complete
	}
}

// Initializes full game settings from potentially abbreviated game settings
void CMatchTitleGameSettingsMgr::InitializeGameSettings( KeyValues *pSettings, char const *szReason )
{
	if ( !Q_stricmp( pSettings->GetString( "system/netflag" ), "teamlink" ) )
		// No configuration on teamlinks
		return;

	InitializeMemberSettings( pSettings );

	if ( !Q_stricmp( szReason, "client" ) )
		// For client session it is sufficient to just initialize member settings
		return;

	// For hosts and searches we need to set member aggregates
	UpdateAggregateMembersSettings( pSettings, pSettings );

	if ( StringHasPrefix( szReason, "search" ) )
		// No additional configuration on search queries required
		return;

	MEM_ALLOC_CREDIT();
	char const *szNetwork = pSettings->GetString( "system/network", "LIVE" );

	if ( KeyValues *kv = pSettings->FindKey( "game", true ) )
	{
		kv->SetString( "state", "lobby" );

		KeyValuesAddDefaultString( kv, "mode", "coop" );
		KeyValuesAddDefaultString( kv, "map", "default" );
	}

	// Setup the mission info keys
	TransferMissionInformationToInfo(
		pSettings->GetString( "game/map" ),
		pSettings->FindKey( "game/missioninfo", true ) );

	pSettings->SetUint64( "game/dlcrequired", DetermineDlcRequiredMask( pSettings ) );

	// Offline games don't need slots and player setup
	if ( !Q_stricmp( "offline", szNetwork ) )
		return;

	//
	// Set the number of slots
	//
	int numSlots = Q_stricmp( pSettings->GetString( "game/mode", "coop" ), "sp" ) ? 2 : 1;

#ifdef _DEBUG
	if ( int nForceDebugSlots = mm_test_slots.GetInt() )
		numSlots = nForceDebugSlots;
#endif

	pSettings->SetInt( "members/numSlots", numSlots );
}

// Extends game settings update packet before it gets merged with
// session settings and networked to remote clients
void CMatchTitleGameSettingsMgr::ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys )
{
	MEM_ALLOC_CREDIT();
	// Check if the map key is deleted
	if ( pUpdateDeleteKeys->FindKey( "delete/game/map" ) )
	{
		pUpdateDeleteKeys->SetString( "delete/game/missioninfo", "delete" );
	}

	// Check if the campaign key is modified
	if ( char const *szNewMission = pUpdateDeleteKeys->GetString( "update/game/map", NULL ) )
	{
		TransferMissionInformationToInfo( szNewMission,
			pUpdateDeleteKeys->FindKey( "update/game/missioninfo", true ) );
	}

	// Check if the campaign or chapter key is modified
	if ( pUpdateDeleteKeys->GetString( "update/game/map" ) )
	{
		KeyValues *pAggregateSettings = pSettings->MakeCopy();
		KeyValues::AutoDelete autodelete( pAggregateSettings );
		pAggregateSettings->MergeFrom( pUpdateDeleteKeys );
		uint64 uiDlcMaskRequired = DetermineDlcRequiredMask( pAggregateSettings );
		if ( uiDlcMaskRequired != pAggregateSettings->GetUint64( "game/dlcrequired" ) )
			pUpdateDeleteKeys->SetUint64( "update/game/dlcrequired", uiDlcMaskRequired );
	}
}

// Prepares system for session creation
KeyValues * CMatchTitleGameSettingsMgr::PrepareForSessionCreate( KeyValues *pSettings )
{
	return MM_Title_RichPresence_PrepareForSessionCreate( pSettings );
}

// Prepares the lobby for game or adjust settings of new players who
// join a game in progress, this function is allowed to modify
// Members/Game subkeys and has to write modified players XUIDs
void CMatchTitleGameSettingsMgr::PrepareLobbyForGame( KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
	IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
	if ( !pSession )
		return;
	KeyValues *kvUpdate = new KeyValues( "update" );
	KeyValues::AutoDelete autodelete_kvUpdate( kvUpdate );

	// Set the flag to prevent players leave tracked while game is in progress
	kvUpdate->SetString( "update/system/netflag", "noleave" );

	// Figure out if we want to start in mp_coop_start or mp_coop_lobby_3 (or mp_coop_lobby_2 for consoles missing DLC)?
	char const *szMap = pSettings->GetString( "game/map" );
	char const *szGameMode = pSettings->GetString( "game/mode" );
	if ( !Q_stricmp( szGameMode, "coop" ) || !Q_stricmp( szGameMode, "coop_challenge" ) )
	{
		// Challenge mode never goes to start map 
		bool bLobby = ( Q_stricmp( szGameMode, "coop_challenge" ) == 0 || pSettings->GetInt( "game/mp_coop_start" ) != 0 );
		char const *szMapRequired = bLobby ? "mp_coop_lobby_3" : "mp_coop_start";

		if ( IsGameConsole() && bLobby )
		{
			if ( !( ( pSettings->GetUint64( "members/machine0/dlcmask" ) & PORTAL2_DLCID_RETAIL_DLC1 ) &&
					( XBX_GetNumGameUsers() > 1 || 
						( pSettings->GetUint64( "members/machine1/dlcmask" ) & PORTAL2_DLCID_RETAIL_DLC1 ) ) ) )
			{
				// One of the players doesn't have the DLC!
				szMapRequired = "mp_coop_lobby_2";
			}
		}

		if ( Q_stricmp( szMapRequired, szMap ) && !Q_stricmp( szMap, "default" ) ) // map set to default, figure out where to start
		{
			// Need to start the game on a different map
			if ( ( pSession->GetSessionSettings() == pSettings ) && !Q_stricmp( "lobby", pSettings->GetString( "game/state" ) ) )
			{
				kvUpdate->SetString( "update/game/map", szMapRequired );
			}
		}
	}
	else if ( !Q_stricmp( szGameMode, "sp" ) )
	{
		if ( !Q_stricmp( szMap, "default" ) )
		{
			// Need to start the game on the first sp map
			if ( ( pSession->GetSessionSettings() == pSettings ) && !Q_stricmp( "lobby", pSettings->GetString( "game/state" ) ) )
			{
				kvUpdate->SetString( "update/game/map", "sp_a1_intro1" );
			}
		}
	}

	// Commit the update
	pSession->UpdateSessionSettings( kvUpdate );
}

// Prepares the host team lobby for game adjusting the game settings
// this function is allowed to prepare modification package to update
// Game subkeys.
// Returns the update/delete package to be applied to session settings
// and pushed to dependent two sesssion of the two teams.
KeyValues * CMatchTitleGameSettingsMgr::PrepareTeamLinkForGame( KeyValues *pSettingsLocal, KeyValues *pSettingsRemote )
{
	Assert( 0 );
	return NULL;
}

// Prepares the client lobby for migration
// this function is called when the client session is still in the state
// of "client" while handling the original host disconnection and decision
// has been made that local machine will be elected as new "host"
// Returns NULL if migration should proceed normally
// Returns [ kvroot { "error" "n/a" } ] if migration should be aborted.
KeyValues * CMatchTitleGameSettingsMgr::PrepareClientLobbyForMigration( KeyValues *pSettingsLocal, KeyValues *pMigrationInfo )
{
	return new KeyValues( "disconnecthdrl", "error", "n/a" );
}

// Prepares the session for server disconnect
// this function is called when the session is still in the active gameplay
// state and while localhost is handling the disconnection from game server.
// Returns NULL to allow default flow
// Returns [ kvroot { "disconnecthdlr" "<opt>" } ] where <opt> can be:
//		"destroy" : to trigger a disconnection error and destroy the session
//		"lobby" : to initiate a "salvaging" lobby transition
KeyValues * CMatchTitleGameSettingsMgr::PrepareClientLobbyForGameDisconnect( KeyValues *pSettingsLocal, KeyValues *pDisconnectInfo )
{
	return new KeyValues( "disconnecthdrl", "disconnecthdlr", "destroy" );
}

// Validates if client profile can set a stat or get awarded an achievement
bool CMatchTitleGameSettingsMgr::AllowClientProfileUpdate( KeyValues *kvUpdate )
{
	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	bool bAllowed = pIMatchSession && !pIMatchSession->GetSessionSettings()->GetBool( "game/sv_cheats" );

	if ( !bAllowed )
	{
		char const *szName = kvUpdate->GetString( "name" );
		if ( StringHasPrefix( szName, "GI.lesson." ) || 
			 StringHasPrefix( szName, "CFG." ) ||
			 StringHasPrefix( szName, "DLC." ) )
		{
			bAllowed = true; // allow achievement unrelated stuff to save with cheats on
		}
	}
	if ( !bAllowed )
	{
		Warning( "Stats and achievements are disabled: cheats turned on in this app session\n" );
	}
	return bAllowed;
}


static void OnRunCommand_PlayerInfo( KeyValues *pCommand, KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
	MEM_ALLOC_CREDIT();
	XUID xuid = pCommand->GetUint64( "xuid" );
	KeyValues *kvGameNew = pCommand->FindKey( "game" );
	if ( !kvGameNew )
		return;

	KeyValues *pPlayer = SessionMembersFindPlayer( pSettings, xuid );
	if ( !pPlayer )
		return;

	KeyValues *kvGame = pPlayer->FindKey( "game", true );
	if ( !kvGame )
		return;

	// Merge in the client fields
	bool bModified = false;
	char const * arrClientFields[] = { "mp_coop_start" };
	for ( int jj = 0; jj < ARRAYSIZE( arrClientFields ); ++ jj )
	{
		char const *szVal = arrClientFields[jj];
		KeyValues *val = kvGameNew->FindKey( szVal );
		if ( !val )
			continue;

		int iValNew = val->GetInt();
		int iVal = kvGame->GetInt( szVal );

		if ( iValNew != iVal )
		{
			kvGame->SetInt( szVal, iValNew );
			bModified = true;
		}
	}

	if ( bModified )
	{
		* ( ppPlayersUpdated ++ ) = pPlayer;

		// Also recompute aggregate session fields
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( !pMatchSession )
			return;
		KeyValues *kvPackage = new KeyValues( "Update" );
		if ( KeyValues *kvUpdate = kvPackage->FindKey( "update", true ) )
		{
			UpdateAggregateMembersSettings( pMatchSession->GetSessionSettings(), kvUpdate );
		}
		pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( kvPackage ) );
	}
}

// Executes the command on the session settings, this function on host
// is allowed to modify Members/Game subkeys and has to fill in modified players KeyValues
// When running on a remote client "ppPlayersUpdated" is NULL and players cannot
// be modified
void CMatchTitleGameSettingsMgr::ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
	char const *szCommand = pCommand->GetName();

	if ( !Q_stricmp( "Game::PlayerInfo", szCommand ) )
	{
		if ( !Q_stricmp( "host", pSessionSystemData->GetString( "type", "host" ) ) &&
			ppPlayersUpdated )
		{
			OnRunCommand_PlayerInfo( pCommand, pSettings, ppPlayersUpdated );
			return;
		}
	}
}



//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "mm_title_richpresence.h"
#include "matchmaking/cstrike15/imatchext_cstrike15.h"

#include "vstdlib/random.h"
#include "fmtstr.h"

#include "../engine/filesystem_engine.h"
#include "filesystem.h"
#include "gametypes/igametypes.h"
#include "mathlib/expressioncalculator.h"

#include "csgo.spa.h"

#include "mm_title_contextvalues.h"

#include "inputsystem/iinputsystem.h"

#if !defined (NO_STEAM)
#include "steam/steam_api.h"
extern CSteamAPIContext *steamapicontext;
#endif

#include "csgo_limits.h"
#include "csgo_limits.inl"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameTypes *g_pGameTypes;

struct AggregateSkillProperty
{
	char const *szSkill;
	unsigned int dwSkillPropertyId;
	unsigned int dwSkillMinPropertyId;
	unsigned int dwSkillMaxPropertyId;
};

static AggregateSkillProperty g_AggregateSkillProperties[] =
{
	{ "skill0",		PROPERTY_CSS_AGGREGATE_SKILL0,		PROPERTY_CSS_SEARCH_SKILL0_MIN,		PROPERTY_CSS_SEARCH_SKILL0_MAX },
	{ "skill1",		PROPERTY_CSS_AGGREGATE_SKILL1,		PROPERTY_CSS_SEARCH_SKILL1_MIN,		PROPERTY_CSS_SEARCH_SKILL1_MAX },
	{ "skill2",		PROPERTY_CSS_AGGREGATE_SKILL2,		PROPERTY_CSS_SEARCH_SKILL2_MIN,		PROPERTY_CSS_SEARCH_SKILL2_MAX },
	{ "skill3",		PROPERTY_CSS_AGGREGATE_SKILL3,		PROPERTY_CSS_SEARCH_SKILL3_MIN,		PROPERTY_CSS_SEARCH_SKILL3_MAX },
	{ "skill4",		PROPERTY_CSS_AGGREGATE_SKILL4,		PROPERTY_CSS_SEARCH_SKILL4_MIN,		PROPERTY_CSS_SEARCH_SKILL4_MAX },
	NULL,
};

#define MATCH_MAX_SKILL_FIELDS	5

ConVar mm_sv_load_test( "mm_sv_load_test", "0", FCVAR_DEVELOPMENTONLY );
ConVar mm_title_debug_version( "mm_title_debug_version", "0", FCVAR_DEVELOPMENTONLY, "This matchmaking version will override .res file version for isolating matchmaking" );
ConVar mm_title_debug_dccheck( "mm_title_debug_dccheck", "0", FCVAR_DEVELOPMENTONLY, "This matchmaking query will override datacenter connectivity: -1 for local, 1 for dedicated" );
ConVar mm_title_debug_minquery( "mm_title_debug_minquery", "0", FCVAR_DEVELOPMENTONLY, "This matchmaking query will run with minimal set of parameters" );
ConVar mm_csgo_community_search_players_min( "mm_csgo_community_search_players_min", "3", FCVAR_RELEASE | FCVAR_ARCHIVE, "When performing CSGO community matchmaking look for servers with at least so many human players" );

class CMatchTitleGameSettingsMgr : public IMatchTitleGameSettingsMgr
{
public:

	CMatchTitleGameSettingsMgr()
	{
		m_pMatchSystemData = NULL;
	}

	~CMatchTitleGameSettingsMgr()
	{
		if ( m_pMatchSystemData )
		{
			m_pMatchSystemData->deleteThis();
		}
	}

	// Extends server game details
	virtual void ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest );

	// Adds the essential part of game details to be broadcast
	virtual void ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings );

	// Extends game settings update packet for lobby transition,
	// either due to a migration or due to an endgame condition
	virtual void ExtendGameSettingsForLobbyTransition( KeyValues *pSettings, KeyValues *pSettingsUpdate, bool bEndGame );

	// Allows title to migrate data cached in client sys session data
	// into host sys session data
	virtual void MigrateSysSessionData( IMatchSession *pNewMatchSession, KeyValues *pSysSessionData );

	// Adds data for datacenter reporting
	virtual void ExtendDatacenterReport( KeyValues *pReportMsg, char const *szReason );

	// Rolls up game details for matches grouping
	virtual KeyValues * RollupGameDetails( KeyValues *pDetails, KeyValues *pRollup, KeyValues *pQuery );


	// Defines session search keys for matchmaking
	virtual KeyValues * DefineSessionSearchKeys( KeyValues *pSettings );

	// Defines dedicated server search key
	virtual KeyValues * DefineDedicatedSearchKeys( KeyValues *pSettings, bool bNeedOfficialServer, int nSearchPass );


	// Extends game settings update packet before it gets merged with
	// session settings and networked to remote clients
	virtual void ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys );

	// Update a team session to be a game session by filling in map name, updating number of slots etc
	virtual KeyValues * ExtendTeamLobbyToGame( KeyValues *pSettings );

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

	// Initializes full game settings from potentially abbreviated game settings
	virtual void InitializeGameSettings( KeyValues *pSettings, char const *szReason );

	// Sets the bspname key given a mapgroup
	virtual void SetBspnameFromMapgroup( KeyValues *pSettings );

	// Prepares the client lobby for migration
	// this function is called when the client session is still in the state
	// of "client" while handling the original host disconnection and decision
	// has been made that local machine will be elected as new "host"
	// Returns NULL if migration should proceed normally
	// Returns [ kvroot { "error" "n/a" } ] if migration should be aborted.
	virtual KeyValues * PrepareClientLobbyForMigration( KeyValues *pSettingsLocal, KeyValues *pMigrationInfo ) { return NULL; }

	// Prepares the session for server disconnect
	// this function is called when the session is still in the active gameplay
	// state and while localhost is handling the disconnection from game server.
	// Returns NULL to allow default flow
	// Returns [ kvroot { "disconnecthdlr" "<opt>" } ] where <opt> can be:
	//		"destroy" : to trigger a disconnection error and destroy the session
	//		"lobby" : to initiate a "salvaging" lobby transition
	virtual KeyValues * PrepareClientLobbyForGameDisconnect( KeyValues *pSettingsLocal, KeyValues *pDisconnectInfo )
	{
		// Every event that causes a disconnection from game server is unsalvagable in CS:GO
		// in other products it should be possible to keep all players that were playing on game server
		// in the lobby together and send them to lobby UI
		return new KeyValues( "disconnecthdrl", "disconnecthdlr", "destroy" );
	}

	// Retrieves the indexed formula from the match system settings file. (MatchSystem.360.res)
	virtual char const * GetFormulaAverage( int index );

	// Called by the client to notify matchmaking that it should update matchmaking properties based
	// on player distribution among the teams.
	virtual void UpdateTeamProperties( KeyValues *pCurrentSettings, KeyValues *pTeamProperties );

	// Validates if client profile can set a stat or get awarded an achievement
	virtual bool AllowClientProfileUpdate( KeyValues *kvUpdate )
	{
		return true;			// Always all profile to be updated for CStrike15, all platforms
	}


protected:
	// Loads the games match settings from the KeyValues object.  The match settings contain
	// data on how to expand the search passes formulas to use for computing skill.
	void LoadMatchSettings( void );

	// Add filters necessary to implement matchmaking rule on Steam
	void AddSteamMatchmakingRule( KeyValues *pResult, bool bAllSessions, KeyValues *pSettings, 
		bool bCssMatchVersion, bool bCssLevel, bool bCssGameType, bool bCssGameMode,
		bool bTeamMatch );
		
	KeyValues *m_pMatchSystemData;

	CUtlVector< CUtlString > m_FormulaAverage;

	CUtlVector< CUtlString > m_FormulaExperience;
	int m_nFormulaExperienceRangeMin;
	int m_nFormulaExperienceRangeMax;

	struct SkillFormulas
	{
		CUtlVector< CUtlString > formulas;
		int rangeMin;
		int rangeMax;
	};
	CUtlVector< SkillFormulas* > m_FormulaSkill;

	struct SearchPass
	{
		bool checkExperience;
		int experienceRange;
		CUtlVector< bool > checkSkill;
		CUtlVector< int > skillRange;
	};
	CUtlVector< SearchPass* > m_SearchPass;
};

CMatchTitleGameSettingsMgr g_MatchTitleGameSettingsMgr;
IMatchTitleGameSettingsMgr *g_pIMatchTitleGameSettingsMgr = &g_MatchTitleGameSettingsMgr;


//
// Implementation of CMatchTitleGameSettingsMgr
//

// Extends server game details
void CMatchTitleGameSettingsMgr::ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest )
{
	// Query server info
	INetSupport::ServerInfo_t si;
	g_pMatchExtensions->GetINetSupport()->GetServerInfo( &si );

	// Server is always in game
	pDetails->SetString( "game/state", "game" );

	//
	// Determine map info
	//
	{
		int mode = g_pGameTypes->GetCurrentGameMode();
		int type = g_pGameTypes->GetCurrentGameType();
		const char *modeName = g_pGameTypes->GetGameModeFromInt( type,  mode );
		const char *typeName = g_pGameTypes->GetGameTypeFromInt( type );

		int numHumanSlots = g_pGameTypes->GetMaxPlayersForTypeAndMode( type, mode );
		pDetails->SetInt( "members/numSlots", numHumanSlots );

		pDetails->SetString( "game/map", si.m_szMapName );
		pDetails->SetString( "game/mapgroupname", si.m_szMapGroupName );
		pDetails->SetString( "game/mode", modeName );
		pDetails->SetString( "game/type", typeName );
		if ( !g_pMatchExtensions->GetIServerGameDLL()->IsValveDS() )
		{
			pDetails->SetInt( "game/hosted", 1 );
			pDetails->SetString( "options/server", "dedicated" );
		}
	}
}

// Adds the essential part of game details to be broadcast
void CMatchTitleGameSettingsMgr::ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings )
{
	static KeyValues *pkvExt = KeyValues::FromString(
		"settings",
		" game { "
			" mapgroupname #empty# "
			" map #empty# "
			" mode #empty# "
			" type #empty# "
			" state #empty# "
			" hosted 0 "
			" spectate 0 "
			" apr 0 "
			" ark 0 "
			" loc #empty# "
			" clanid #empty# "
			" clantag #empty# "
		" } "
		);

	// TODO: Is this the appropriate spot to add in game values to initialize the dedicated server state?

	pDetails->MergeFrom( pkvExt, KeyValues::MERGE_KV_UPDATE );
}

// Extends game settings update packet for lobby transition,
// either due to a migration or due to an endgame condition
void CMatchTitleGameSettingsMgr::ExtendGameSettingsForLobbyTransition( KeyValues *pSettings, KeyValues *pSettingsUpdate, bool bEndGame )
{
	pSettingsUpdate->SetString( "game/state", "lobby" );

	extern void UpdateAggregateMembersSettings( KeyValues *pFullGameSettings, KeyValues *pUpdate );
	UpdateAggregateMembersSettings( pSettings, pSettingsUpdate );
}

// Allows title to migrate data cached in client sys session data
// into host sys session data
void CMatchTitleGameSettingsMgr::MigrateSysSessionData( IMatchSession *pNewMatchSession, KeyValues *pSysSessionData )
{
#ifdef _PS3
	KeyValues *kvSystemData = pNewMatchSession->GetSessionSystemData();
	if ( !kvSystemData )
		return;

	if ( KeyValues *kvTimeout = pSysSessionData->FindKey( "timeout", false ) )
	{
		int avgRank = kvTimeout->GetInt();
		kvSystemData->SetInt( "timeout", avgRank );

		DevMsg( "Session timeout value=%d (%s)\n", avgRank, "migrated" );
		steamapicontext->SteamMatchmaking()->SetLobbyData( kvSystemData->GetUint64( "xuidReserve", 0ull ), "game:timeout", CFmtStr( "%u", avgRank ) );
	}

	if ( KeyValues *kvNumOpenSlots = pSysSessionData->FindKey( "numOpenSlots", false ) )
	{
		int numOpenSlots = kvNumOpenSlots->GetInt();
		kvSystemData->SetInt( "numOpenSlots", numOpenSlots );

		DevMsg( "Session numOpenSlots=%d (%s)\n", numOpenSlots, "migrated" );
		steamapicontext->SteamMatchmaking()->SetLobbyData( kvSystemData->GetUint64( "xuidReserve", 0ull ), "game:numOpenSlots", CFmtStr( "%u", numOpenSlots ) );
	}
#endif
}

// Adds data for datacenter reporting
void CMatchTitleGameSettingsMgr::ExtendDatacenterReport( KeyValues *cmd, char const *szReason )
{
#ifdef _X360
	//if ( XBX_GetPrimaryUserId() == XBX_INVALID_USER_ID )
	//	return;
	//if ( !XBX_GetNumGameUsers() || XBX_GetPrimaryUserIsGuest() )
	//	return;

	//IPlayerLocal *pLocalPlayer = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() );
	//if ( !pLocalPlayer )
	//	return;

	//// Achievements info
	//uint64 uiAchMask1 = 0;	// 100 bits for achievements
	//uint64 uiAchMask2 = 0;	// 27 bits for asset awards
	//{
	//	KeyValues *kvAchInfo = new KeyValues( "", "@achievements", 0, "@awards", 0 );
	//	KeyValues::AutoDelete autodelete_kvAchInfo( kvAchInfo );
	//	pLocalPlayer->GetAwardsData( kvAchInfo );
	//	for ( KeyValues *val = kvAchInfo->FindKey( "@achievements" )->GetFirstValue(); val; val = val->GetNextValue() )
	//	{
	//		int iVal = val->GetInt( "", 0 );
	//		if ( iVal <= 0 )
	//			continue;
	//		else if ( iVal < 64 )
	//			uiAchMask1 |= ( 1ull << iVal );
	//		else if ( iVal <= 100 )
	//			uiAchMask2 |= ( 1ull << ( iVal - 64 ) );
	//	}
	//	for ( KeyValues *val = kvAchInfo->FindKey( "@awards" )->GetFirstValue(); val; val = val->GetNextValue() )
	//	{
	//		int iVal = val->GetInt( "", 0 );
	//		if ( iVal <= 0 )
	//			continue;
	//		else if ( iVal < ( 128 - 100 ) )
	//			uiAchMask2 |= ( 1ull << ( iVal + 100 - 64 ) );
	//	}
	//}
	//cmd->SetUint64( "ach1", uiAchMask1 );
	//cmd->SetUint64( "ach2", uiAchMask2 );

	//if ( !V_stricmp( szReason, "datarequest" ) )
	//{
	//	// Add game information
	//	TitleData1 * td1 = ( TitleData1 * ) pLocalPlayer->GetPlayerTitleData( TitleDataFieldsDescription_t::DB_TD1 );
	//	TitleData3 * td3 = ( TitleData3 * ) pLocalPlayer->GetPlayerTitleData( TitleDataFieldsDescription_t::DB_TD3 );

	//	// sp progress
	//	cmd->SetInt( "map_s", td1->uiSinglePlayerProgressChapter );

	//	// coop completion
	//	int numMapBitsFields = sizeof( td1->coop.mapbits ) / sizeof( uint64 );
	//	for ( int imap = 0; imap < numMapBitsFields; ++ imap )
	//	{
	//		cmd->SetUint64( CFmtStr( "map_%d", imap ), ( reinterpret_cast< uint64 * >( td1->coop.mapbits ) )[imap] );
	//	}

	//	if ( td3->cvUser.version )
	//	{
	//		// profile settings
	//		cmd->SetFloat( "cfg_pitch", td3->cvUser.joy_pitchsensitivity );
	//		cmd->SetFloat( "cfg_yaw", td3->cvUser.joy_yawsensitivity );
	//		cmd->SetInt( "cfg_joy", td3->cvUser.joy_cfg_preset );
	//		cmd->SetInt( "cfg_bit", td3->cvUser.bitfields[0] );
	//	}

	//	if ( td3->cvSystem.version )
	//	{
	//		// audio/video
	//		cmd->SetFloat( "sys_vol", td3->cvSystem.volume );
	//		cmd->SetFloat( "sys_mus", td3->cvSystem.snd_musicvolume );
	//		cmd->SetFloat( "sys_gam", td3->cvSystem.mat_monitorgamma );
	//		cmd->SetInt( "sys_ssm", td3->cvSystem.ss_splitmode );
	//		cmd->SetInt( "sys_bit", td3->cvSystem.bitfields[0] );
	//	}

	//	if ( g_pXboxInstaller )
	//	{
	//		cmd->SetInt( "inst",
	//			( g_pXboxInstaller->IsFullyInstalled() ? 1 : 0 ) |
	//			( g_pXboxInstaller->IsInstallEnabled() ? 2 : 0 )
	//			);
	//	}
	//}
#endif
}

// Rolls up game details for matches grouping
KeyValues * CMatchTitleGameSettingsMgr::RollupGameDetails( KeyValues *pDetails, KeyValues *pRollup, KeyValues *pQuery )
{
	// TODO: keep each individual results, roll up to the party leader XUID
	return NULL;
}

// Defines dedicated server search key
KeyValues * CMatchTitleGameSettingsMgr::DefineDedicatedSearchKeys( KeyValues *pSettings, bool bNeedOfficialServer, int nSearchPass )
{
#if defined ( _X360 )

	return NULL;

#else

	static ConVarRef sv_search_key( "sv_search_key" );

	KeyValues *pKeys = new KeyValues( "SearchKeys" );

	CFmtStr fmtExtraGameData;

	if ( bNeedOfficialServer )
	{
		pKeys->SetString( "gametype", "valve_ds,empty" );
	}
	else
	{
		pKeys->SetString( "gametype", "empty" );

		if ( char const *szMapGroup = pSettings->GetString( "game/mapgroupname", NULL ) )
		{
			if ( char const *szWorkshop = Q_stristr( szMapGroup, "@workshop" ) )
			{
				if ( szWorkshop != szMapGroup )
				{	// When searching for a workshop map only consider servers that are empty and advertise support for it
					// and are running the same game type and mode as requested
					// First search will be for explicitly supported map in rotation, but second search will be for
					// servers that are willing to host public players
					if ( 0 == ( nSearchPass % 2 ) )
					{	// First search pass: request map ID tag
						fmtExtraGameData.AppendFormat( "%.*s,", szWorkshop - szMapGroup, szMapGroup );
					}
					else
					{	// Second search pass: request any map support
						fmtExtraGameData.AppendFormat( "wks:1," );
					}

					// if ( !pSettings->GetBool( "options/anytypemode" ) ) -- maybe allow reserving community servers regardless of game modes?
					{
						int nGameType = 0, nGameMode = 0;
						g_pGameTypes->GetGameModeAndTypeIntsFromStrings( pSettings->GetString( "game/type" ), pSettings->GetString( "game/mode" ),
							nGameType, nGameMode );
						fmtExtraGameData.AppendFormat( "gt:%u,gm:%u,", nGameType, nGameMode );
					}
				}
			}
		}
	}

	pKeys->SetString( "gamedata", CFmtStr( "%s%skey:%s%d",
		fmtExtraGameData.Access(),
		bNeedOfficialServer ? "v" : "c",
		sv_search_key.GetString(),
		g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() ) );

	return pKeys;

#endif
}


static void DescribeX360QueryDefineSessionSearchKeys( KeyValues *pkv )
{
#ifdef _X360
	DevMsg( "======== DescribeX360QueryDefineSessionSearchKeys ==============\n" );
	DevMsg( "  numPlayers = %d\n", pkv->GetInt( "numPlayers" ) );
	DevMsg( "  rule       = %s\n", ( pkv->GetInt( "rule" ) == SESSION_MATCH_QUERY_PLAYER_MATCH ) ? "playermatch" : "UNKNOWN" );
	
	if ( KeyValues *val = pkv->FindKey( CFmtStr( "Contexts/%d", CONTEXT_CSS_GAME_TYPE ) ) )
	{
		char const *szVal = "UNKNOWN";
		switch ( val->GetInt() )
		{
		case CONTEXT_CSS_GAME_TYPE_CLASSIC: szVal = "classic"; break;
		case CONTEXT_CSS_GAME_TYPE_GUNGAME: szVal = "gungame"; break;
		default: Assert( false ); break;
		}
		DevMsg( "  CONTEXT_CSS_GAME_TYPE = %s (%d)\n", szVal, val->GetInt() );
	}

	if ( KeyValues *val = pkv->FindKey( CFmtStr( "Contexts/%d", CONTEXT_CSS_GAME_MODE ) ) )
	{
		char const *szVal = "UNKNOWN";
		switch ( val->GetInt() )
		{
		case CONTEXT_CSS_GAME_MODE_CASUAL: szVal = "casual"; break;
		case CONTEXT_CSS_GAME_MODE_COMPETITIVE: szVal = "competitive"; break;
		case CONTEXT_CSS_GAME_MODE_FREESTYLE: szVal = "freestyle"; break;
		case CONTEXT_CSS_GAME_MODE_GUNGAMEPROGRESSIVE: szVal = "gungameprogressive"; break;
		case CONTEXT_CSS_GAME_MODE_GUNGAMEBOMB: szVal = "gungamebomb"; break;
		default: Assert( false ); break;
		}
		DevMsg( "  CONTEXT_CSS_GAME_MODE = %s (%d)\n", szVal, val->GetInt() );
	}
	else if ( KeyValues *val = pkv->FindKey( CFmtStr( "Properties/%d", PROPERTY_CSS_GAME_MODE_AS_NUMBER ) ) )
	{
		DevMsg( "  PROPERTY_CSS_GAME_MODE_AS_NUMBER near 0x%X (%d)\n", val->GetInt(), val->GetInt() );
	}

	if ( KeyValues *val = pkv->FindKey( CFmtStr( "Contexts/%d", CONTEXT_GAME_STATE ) ) )
	{
		char const *szVal = "UNKNOWN";
		switch ( val->GetInt() )
		{
		case CONTEXT_GAME_STATE_IN_MENUS: szVal = "in_menus"; Assert( false ); break;
		case CONTEXT_GAME_STATE_SINGLE_PLAYER: szVal = "singleplayer"; Assert( false ); break;
		case CONTEXT_GAME_STATE_MULTIPLAYER: szVal = "multiplayer"; break;
		default: Assert( false ); break;
		}
		DevMsg( "  CONTEXT_GAME_STATE = %s (%d)\n", szVal, val->GetInt() );
	}

	if ( KeyValues *val = pkv->FindKey( CFmtStr( "Contexts/%d", CONTEXT_CSS_MAP_GROUP ) ) )
	{
		DevMsg( "  CONTEXT_CSS_MAP_GROUP = %s (%d)\n", "", val->GetInt() );
	}

	if ( KeyValues *val = pkv->FindKey( CFmtStr( "Properties/%d", PROPERTY_CSS_MATCH_VERSION ) ) )
	{
		DevMsg( "  PROPERTY_CSS_MATCH_VERSION = 0x%X (%d)\n", val->GetInt(), val->GetInt() );
	}

	if ( KeyValues *val = pkv->FindKey( CFmtStr( "Properties/%d", PROPERTY_CSS_SEARCH_LISTEN_SERVER ) ) )
	{
		DevMsg( "  PROPERTY_CSS_SEARCH_LISTEN_SERVER = 0x%X (%d)\n", val->GetInt(), val->GetInt() );
	}

	if ( KeyValues *val = pkv->FindKey( CFmtStr( "Properties/%d", PROPERTY_CSS_MAX_OPEN_TEAM_SLOTS ) ) )
	{
		DevMsg( "  PROPERTY_CSS_MAX_OPEN_TEAM_SLOTS >= 0x%X (%d)\n", val->GetInt(), val->GetInt() );
	}

	if ( KeyValues *val = pkv->FindKey( CFmtStr( "Properties/%d", PROPERTY_CSS_AGGREGATE_SKILL0 ) ) )
	{
		DevMsg( "  PROPERTY_CSS_AGGREGATE_SKILL0 near 0x%X (%d)\n", val->GetInt(), val->GetInt() );
	}

	DWORD validsettings[] = { X_CONTEXT_GAME_TYPE, X_CONTEXT_GAME_MODE, CONTEXT_CSS_GAME_TYPE, CONTEXT_CSS_GAME_MODE, PROPERTY_CSS_GAME_MODE_AS_NUMBER,
	CONTEXT_GAME_STATE, CONTEXT_CSS_MAP_GROUP, PROPERTY_CSS_MATCH_VERSION, PROPERTY_CSS_SEARCH_LISTEN_SERVER, PROPERTY_CSS_MAX_OPEN_TEAM_SLOTS,
	PROPERTY_CSS_AGGREGATE_SKILL0 };
	char const *categories[] = { "Contexts", "Properties" };
	for ( int iCategory = 0; iCategory < Q_ARRAYSIZE( categories ); ++ iCategory )
	{
		for ( KeyValues *val = pkv->FindKey( categories[iCategory] )->GetFirstSubKey(); val; val = val->GetNextKey() )
		{
			DWORD idx = atoi( val->GetName() );
			bool bValid = false;
			for ( int jj = 0; jj < Q_ARRAYSIZE( validsettings ); ++ jj )
			{
				if ( validsettings[jj] == idx )
				{
					bValid = true;
					break;
				}
			}
			if ( !bValid )
			{
				DevMsg( "Unexpected entry in query %s: %s (0x%X)\n", categories[iCategory], val->GetName(), idx );
			}
			Assert( bValid );
		}
	}

	DevMsg( "======== ********X360Query*********************** ==============\n" );
#endif
}

// Defines session search keys for matchmaking
KeyValues * CMatchTitleGameSettingsMgr::DefineSessionSearchKeys( KeyValues *pSettings )
{
	MEM_ALLOC_CREDIT();

	DevMsg( "DefineSessionSearchKeys settings:\n" );
	KeyValuesDumpAsDevMsg( pSettings, 1 );

	// Process the match system data here.  This will bail early without loading if the
	// input file from above doesn't bump it's version number.
	LoadMatchSettings();

	KeyValues *pResult = new KeyValues( "SessionSearch" );

	int numPlayers = pSettings->GetInt( "members/numPlayers", XBX_GetNumGameUsers() );
	pResult->SetInt( "numPlayers",  numPlayers);

	// Certain contexts and properties can not be part of the search parameters unless they are explictly
	// required by the matchmaking query.  Setting contexts/parameters that the query doesn't expect will
	// cause the query to fail.
	bool bCssMatchVersion = true;
	bool bCssLevel = true;
	bool bCssGameType = true;
	bool bCssGameMode = true;
#if defined _X360
	bool bCssGameModeAsNumber = true;
#endif
	bool bTeamMatch = true;
	bool bTeamMatchTypeClan = false;
	bool bTeamMatchTypeClanPreferred = false;

	// Determine if this is a team matchmaking query: 
	//  On Consoles we define "conteammatch" for team lobbies; 
	//  On PC, we do NOT set bypasslobby for team lobbies
#if defined( _GAMECONSOLE )
	bTeamMatch = ( pSettings->GetString( "options/conteammatch", NULL ) != NULL );
#else
	bTeamMatch = ( pSettings->GetBool( "options/bypasslobby", false ) == false );
#endif

#if defined _X360
	pResult->SetInt( "rule", rule );
#endif

	// Set the appropriate query based on if we're quickmatching or custommatching.
	char const *szAction = pSettings->GetString( "options/action", "" );
	bool bMatchmakingQueryForGameInProgress = true;
	if ( !Q_stricmp( "quickmatch", szAction ) )
	{
		bCssLevel = false;
		bMatchmakingQueryForGameInProgress = true;
	}
	else if ( !Q_stricmp( "custommatch", szAction ) )
	{
		bMatchmakingQueryForGameInProgress = true;
		// If a mapgroup is specified use the player match query, but if no mapgroup is specified,
		// use the player query that doesn't filter based on mapgroup name.
		const char *pszMapGroupName = pSettings->GetString( "game/mapgroupname", NULL );
		if ( pszMapGroupName != NULL && Q_strlen( pszMapGroupName ) > 0 )
		{
		}
		else
		{
			bCssLevel = false;
		}
	}

#if defined _X360
	// X_CONTEXT_GAME_TYPE 
	pResult->SetInt( CFmtStr( "Contexts/%d", X_CONTEXT_GAME_TYPE ), X_CONTEXT_GAME_TYPE_STANDARD );
	pResult->SetInt( CFmtStr( "Contexts/%d", X_CONTEXT_GAME_MODE ), CONTEXT_GAME_MODE_CSS_GAME_MODE_MULTIPLAYER );

	// X_CONTEXT_GAME_MODE 
	if ( char const *szValue = pSettings->GetString( "game/mode", NULL ) )
	{
		DWORD dwValue = g_GameModeContexts->ScanValues( szValue );
		if ( bCssGameMode && dwValue != 0xFFFF )
		{
			pResult->SetInt( CFmtStr( "Contexts/%d", CONTEXT_CSS_GAME_MODE ), dwValue );
		}

		// Omit this property based on the rule.
		// Set the PROPERTY_CSS_GAME_MODE_AS_NUMBER value as the square of the CONTEXT_CSS_GAME_MODE Difficulty setting.
		// The resulting sequence of numbers (0, 1, 4, 9, ...) ensures that if an exact match can't be found
		// then the 'near' sort operation will prefer the next easier match to the next harder match.  For example,
		// if COMPETITIVE is requested (1), then CASUAL matches (0) would be preferred to PRO matches (4).
		dwValue = g_GameModeAsNumberContexts->ScanValues( szValue );
		if ( bCssGameModeAsNumber && dwValue != 0xFFFF )
		{
			pResult->SetInt( CFmtStr( "Properties/%d", PROPERTY_CSS_GAME_MODE_AS_NUMBER ), dwValue );
		}
	}

	// Set the game type (classic or gungame)
	if ( char const *szGameType = pSettings->GetString( "game/type", NULL ) )
	{
		DWORD dwValue = g_GameTypeContexts->ScanValues( szGameType );
		if ( bCssGameType && dwValue != 0xFFFF )
		{
			pResult->SetInt( CFmtStr( "Contexts/%d", CONTEXT_CSS_GAME_TYPE ), dwValue );
		}
	}

	// Set the matchmaking version.
	if ( bCssMatchVersion )
	{
		pResult->SetInt( CFmtStr( "Properties/%d", PROPERTY_CSS_MATCH_VERSION ), mm_title_debug_version.GetInt() );
	}

	if ( 0 && mm_title_debug_minquery.GetBool() )
	{
		DescribeX360QueryDefineSessionSearchKeys( pResult );
		return pResult; // don't run the rest of filters, run with minimal set of parameters
	}


	// Set the mapgroup name we're using, if any.
	if ( char const *szMapGroupName = pSettings->GetString( "game/mapgroupname", NULL ) )
	{
		// First check if the rich presence context for this map was set in gamemodes.txt
		DWORD dwValue = dwValue = pSettings->GetInt( "game/mapRichPresence", 0xFFFF );
		if ( dwValue == 0xFFFF )
		{
			dwValue = g_MapGroupContexts->ScanValues( szMapGroupName );
		}
		if ( bCssLevel && dwValue != 0xFFFF )
		{
			pResult->SetInt( CFmtStr( "Contexts/%d", CONTEXT_CSS_MAP_GROUP ), dwValue );
		}
	}

	// Set desire to find games in lobbies
	if ( bMatchmakingQueryForGameInProgress )
	{
		pResult->SetInt( CFmtStr( "Contexts/%d", CONTEXT_GAME_STATE ), CONTEXT_GAME_STATE_MULTIPLAYER );
	}

#endif

	// Determine if we're playing gungameprogressive mode so we can use the proper matchmaking data fields.
	MatchmakingDataType mmDataType = MMDATA_TYPE_GENERAL;
	if ( !V_stricmp( "gungameprogressive", pSettings->GetString( "game/mode" ) ) )
	{
		mmDataType = MMDATA_TYPE_GGPROGRESSIVE;
	}

	// Add steam version of rule
	AddSteamMatchmakingRule(pResult, !bMatchmakingQueryForGameInProgress, pSettings, bCssMatchVersion, bCssLevel, bCssGameType, bCssGameMode, bTeamMatch);

	// If we want a clan-preferred match, duplicate search keys. First search for clan only
	// matches and then regular matches
	const char *szOppTeamType = pSettings->GetString( "game/opp_team_type", "");
	bTeamMatchTypeClan = !Q_stricmp( "clan", szOppTeamType );
	if ( !bTeamMatchTypeClan )
	{
		bTeamMatchTypeClanPreferred = !Q_stricmp( "clan_preferred", szOppTeamType );
	}

	KeyValues *pTemplate = pResult->MakeCopy();
	KeyValues::AutoDelete autoDeleteEvent( pTemplate );

	if ( IsX360() )
	{
#ifdef _X360
		if ( bConTeamMatch )
		{
			pResult->SetInt( CFmtStr( "Properties/%d", PROPERTY_CSS_MAX_OPEN_TEAM_SLOTS ), numPlayers );
		}
#endif
	}
	else
	{
		if ( uint64 uiDependentLobby = pSettings->GetUint64( "System/dependentlobby", 0ull ) )
		{
			pResult->SetUint64( "DependentLobby", uiDependentLobby );
		}	
	}

	DescribeX360QueryDefineSessionSearchKeys( pResult );
	return pResult;
}

// Initializes full game settings from potentially abbreviated game settings
void CMatchTitleGameSettingsMgr::InitializeGameSettings( KeyValues *pSettings, const char *szReason )
{
	// GS - Make sure match settings are loaded. For team lobbies we create a lobby without first
	// searching for one, so this function will be called before others (like DefineSessionSearchKeys)
	// that also call LoadMatchSettings()
	LoadMatchSettings();

	//char const *szNetwork = pSettings->GetString( "system/network", "LIVE" );

	if ( KeyValues *kv = pSettings->FindKey( "game", true ) )
	{
		kv->SetString( "state", "lobby" );
	}

	const char *pMapGroupName = pSettings->GetString( "game/mapgroupname", NULL );
	// if no mapgroup specified, randomly select a mapgroup based on type and mode
	if ( !pMapGroupName )
	{
		const char *pGameTypeName = pSettings->GetString( "game/type", NULL );
		const char *pGameModeName = pSettings->GetString( "game/mode", NULL );
		if ( pGameTypeName && pGameModeName )
		{
			pMapGroupName = g_pGameTypes->GetRandomMapGroup( pGameTypeName, pGameModeName );
			if ( pMapGroupName )
			{
				pSettings->SetString( "game/mapgroupname", pMapGroupName );
			}
		}
	}

	// map name should not be coming in here, only mapgroup, so set mapname from mapgroupname
	SetBspnameFromMapgroup( pSettings );

	// Set the number of slots and the rich presence context based on the map.
	const char *szMap = pSettings->GetString( "game/map", NULL );
	int numSlots = pSettings->GetInt( "members/numSlots", 10 );
	uint32 dwRichPresenceContext = 0xFFFF;

	if ( szMap )
	{
		g_pGameTypes->GetMapInfo( szMap, dwRichPresenceContext );
	}

	// If this is an official game, then we know how many slots we will force
	if ( !Q_stricmp( "searchempty", pSettings->GetString( "options/createreason" ) ) &&
		!pSettings->GetBool( "game/hosted" ) )
	{
		const char *pGameTypeName = pSettings->GetString( "game/type" );
		const char *pGameModeName = pSettings->GetString( "game/mode" );
		int iType, iMode;
		if ( g_pGameTypes->GetGameModeAndTypeIntsFromStrings( pGameTypeName, pGameModeName, iType, iMode ) )
			numSlots = g_pGameTypes->GetMaxPlayersForTypeAndMode( iType, iMode );
	}
	
	pSettings->SetInt( "members/numSlots", numSlots );

#ifdef _X360
	int extraSpectators = 2;
	pSettings->SetInt( "members/numExtraSpectatorSlots", extraSpectators );
	pSettings->SetInt( "game/mapRichPresence", dwRichPresenceContext );

	pSettings->SetInt( "game/matchversion", mm_title_debug_version.GetInt() );
	pSettings->SetInt("game/experience", 0);
#endif

	if ( !IsX360() )
	{
		// Add search key as a filter
		static ConVarRef sv_search_key( "sv_search_key" );
		CFmtStr searchKey( "k%s%d",
			sv_search_key.GetString(),
			g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() );

		pSettings->SetString( "game/search_key", searchKey.Access());
		
		// Temp: Check for sv_load_test

#if !defined (NO_STEAM)

		if ( IsPC() && mm_sv_load_test.GetBool() )
		{
			const char* playerCountry = steamapicontext->SteamUtils()->GetIPCountry();
			if ( !Q_stricmp( playerCountry, "US") )
			{
				pSettings->SetInt( "options/sv_load_test", 1 );
			}
		}

#endif

	}

	if ( KeyValues *kvLocalPlayer = pSettings->FindKey( "members/machine0/player0" ) )
	{
#if !defined (NO_STEAM)
		//
		// Clan information
		//
		SplitScreenConVarRef varOption( "cl_clanid" );
		const char *pClanID = varOption.GetString( 0 );
		uint32 iPlayerClanID = atoi( pClanID );

		kvLocalPlayer->SetInt( "game/clanID", iPlayerClanID );
		ISteamFriends *pFriends = steamapicontext->SteamFriends();
		if ( pFriends )
		{
			int iGroupCount = pFriends->GetClanCount();
			for ( int k = 0; k < iGroupCount; ++ k )
			{
				CSteamID clanID = pFriends->GetClanByIndex( k );
				if ( clanID.GetAccountID() == iPlayerClanID )
				{
					CSteamID clanID( iPlayerClanID, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeClan );
					// valid clan, accept the change
					const char *szClanTag = pFriends->GetClanTag( clanID );
					char chLimitedTag[ MAX_CLAN_TAG_LENGTH ];
					CopyStringTruncatingMalformedUTF8Tail( chLimitedTag, szClanTag, MAX_CLAN_TAG_LENGTH );
					
					const char *szClanName = pFriends->GetClanName( clanID );

					kvLocalPlayer->SetString( "game/clantag", chLimitedTag );
					kvLocalPlayer->SetString( "game/clanname", szClanName );
				}
			}
		}

		//
		// Ticketing information for friends
		//
		if ( g_pMatchExtensions->GetIBaseClientDLL() )
		{
			g_pMatchExtensions->GetIBaseClientDLL()->DetermineSubscriptionKvToAdvertise( kvLocalPlayer );
		}

		// If we are joining via friend discovery then pass it to the host
		if ( uint64 xuidFriendJoin = pSettings->GetUint64( "options/friendxuid" ) )
		{
			kvLocalPlayer->SetUint64( "game/jfriend", xuidFriendJoin );
		}

		// If we are joining via nearby discovery then pass it to the host
		if ( int nNearbyJoin = pSettings->GetInt( "options/nby" ) )
		{
			kvLocalPlayer->SetInt( "game/nby", nNearbyJoin );
		}

		// If we are joining via clan discovery then pass it to the host
		if ( char const *szClanIdOption = pSettings->GetString( "options/clanid", NULL ) )
		{
			kvLocalPlayer->SetString( "game/jclanid", szClanIdOption );

			if ( pFriends )
			{
				int iGroupCount = pFriends->GetClanCount();
				for ( int k = 0; k < iGroupCount; ++k )
				{
					CSteamID clanID = pFriends->GetClanByIndex( k );
					if ( clanID.GetAccountID() == iPlayerClanID )
					{
						CSteamID clanID( iPlayerClanID, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeClan );
						// valid clan, accept the change
						const char *szClanTag = pFriends->GetClanTag( clanID );
						char chLimitedTag[ MAX_CLAN_TAG_LENGTH ];
						CopyStringTruncatingMalformedUTF8Tail( chLimitedTag, szClanTag, MAX_CLAN_TAG_LENGTH );
						kvLocalPlayer->SetString( "game/jclantag", chLimitedTag );
					}
				}
			}
		}

		// After we initialized local player transfer that data into session
		pSettings->SetInt( "game/ark", kvLocalPlayer->GetInt( "game/ranking" )*10 );
		pSettings->SetInt( "game/apr", kvLocalPlayer->GetInt( "game/prime" ) );
		pSettings->SetString( "game/loc", kvLocalPlayer->GetString( "game/loc" ) );
#endif
	}
}

void CMatchTitleGameSettingsMgr::SetBspnameFromMapgroup( KeyValues *pSettings )
{
	const char *pMapGroupName = pSettings->GetString( "game/mapgroupname", NULL );
	const char *pMapName = pSettings->GetString( "game/map", NULL );
	if ( !pMapName && pMapGroupName )
	{
		pMapName = g_pGameTypes->GetRandomMap( pMapGroupName );
		if ( pMapName && pMapName[0] )
		{			
			pSettings->SetString( "game/map", pMapName );
		}
	}
}

// Extends game settings update packet before it gets merged with
// session settings and networked to remote clients
void CMatchTitleGameSettingsMgr::ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys )
{
	if ( char const *szClanIdUpdate = pUpdateDeleteKeys->GetString( "update/game/clanid", NULL ) )
	{	// Ensure that clantag is also set when setting clanid
		if ( uint64 xuid = V_atoui64( szClanIdUpdate ) )
		{
			CSteamID steamIdClan( xuid );
			const char *pTag = steamapicontext->SteamFriends()->GetClanTag( steamIdClan );
			if ( pTag && *pTag )
			{
				char chLimitedTag[ MAX_CLAN_TAG_LENGTH ];
				CopyStringTruncatingMalformedUTF8Tail( chLimitedTag, pTag, MAX_CLAN_TAG_LENGTH );
				pUpdateDeleteKeys->SetString( "update/game/clantag", chLimitedTag );
			}
			else
			{
				pUpdateDeleteKeys->SetString( "update/game/clantag", "" );
			}
		}
	}

	if ( char const *szClanIdUpdate = pUpdateDeleteKeys->GetString( "delete/game/clanid", NULL ) )
	{
		pUpdateDeleteKeys->SetString( "delete/game/clantag", "" );
	}

	if ( char const *szMmQueueUpdate = pUpdateDeleteKeys->GetString( "update/game/mmqueue", NULL ) )
	{
		int nAPR = pSettings->GetInt( "game/apr" );
		int nUpdateAlready = pUpdateDeleteKeys->GetInt( "update/game/apr", -1 );
		if ( nUpdateAlready >= 0 )
			nAPR = nUpdateAlready;
		pUpdateDeleteKeys->SetInt( "update/game/apr", nAPR | 0x2 );
	}

	if ( char const *szMmQueueUpdate = pUpdateDeleteKeys->GetString( "delete/game/mmqueue", NULL ) )
	{
		int nAPR = pSettings->GetInt( "game/apr" );
		int nUpdateAlready = pUpdateDeleteKeys->GetInt( "update/game/apr", -1 );
		if ( nUpdateAlready >= 0 )
			nAPR = nUpdateAlready;
		pUpdateDeleteKeys->SetInt( "update/game/apr", nAPR & ~0x2 );
	}
}

KeyValues *CMatchTitleGameSettingsMgr::ExtendTeamLobbyToGame( KeyValues *pSettings )
{
	KeyValues *pUpdate = KeyValues::FromString(
		"update",
		" update { "
			" system { "
				" network LIVE "
				" netFlag #empty#"
			" } "
			" options { "
				" bypasslobby 1"
			" } "
			" game {"				
			" } "
			" members {"
			" } "
		" } "
	);

	// Add in bsp name from map group name
	const char *pMapGroupName = pSettings->GetString( "game/mapgroupname", NULL );
	Assert( pMapGroupName );
	const char *pMapName = pSettings->GetString( "game/map", NULL );
	Assert( pMapName );
	if ( !pMapName )
	{
		pMapName = g_pGameTypes->GetRandomMap( pMapGroupName );
		pUpdate->SetString( "udpate/game/map", pMapName );
	}

	DevMsg( "CMatchTitleGameSettingsMgr::ExtendTeamLobbyToGame\n" );
	KeyValuesDumpAsDevMsg( pUpdate );

	return pUpdate;
}

// Prepares system for session creation
KeyValues * CMatchTitleGameSettingsMgr::PrepareForSessionCreate( KeyValues *pSettings )
{
	// It's at this point that we need to set all of the appropriate properties on the local machine
	// for matchmaking searches.

	return MM_Title_RichPresence_PrepareForSessionCreate( pSettings );
}

// Prepares the lobby for game or adjust settings of new players who
// join a game in progress, this function is allowed to modify
// Members/Game subkeys and has to write modified players XUIDs
void CMatchTitleGameSettingsMgr::PrepareLobbyForGame( KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
	// set player avatar/teams, etc	
}

// Prepares the host team lobby for game adjusting the game settings
// this function is allowed to prepare modification package to update
// Game subkeys.
// Returns the update/delete package to be applied to session settings
// and pushed to dependent two sesssion of the two teams.
KeyValues * CMatchTitleGameSettingsMgr::PrepareTeamLinkForGame( KeyValues *pSettingsLocal, KeyValues *pSettingsRemote )
{
	return NULL;
}

void UpdateAggregateMembersSettings( KeyValues *pFullGameSettings, KeyValues *pUpdate )
{
	bool bAllPrime = true;
	int nAvgRank = 0;
	int nHaveRank = 0;
	int nTotalPlayers = 0;

	char const *szBestCountry = "";
	float flBestCountryWeight = 0.0f;
	CUtlStringMap< float > mapPlayerCountries;
	static CSteamID s_mysteamid = steamapicontext->SteamUser()->GetSteamID();
	for ( int iMachine = 0, numMachines = pFullGameSettings->GetInt( "members/numMachines" ); iMachine < numMachines; ++iMachine )
	{
		KeyValues *pMachine = pFullGameSettings->FindKey( CFmtStr( "members/machine%d", iMachine ) );
		for ( int iPlayer = 0, numPlayers = pMachine->GetInt( "numPlayers" ); iPlayer < numPlayers; ++iPlayer )
		{
			KeyValues *pPlayer = pMachine->FindKey( CFmtStr( "player%d", iPlayer ) );
			if ( !pPlayer )
				continue;

			if ( !pPlayer->GetInt( "game/prime" ) )
				bAllPrime = false;

			int nRanking = pPlayer->GetInt( "game/ranking" );
			if ( nRanking )
			{
				nAvgRank += nRanking;
				++ nHaveRank;
			}
			++ nTotalPlayers;

			char const *szLocation = pPlayer->GetString( "game/loc" );
			if ( !*szLocation )
				continue;

			UtlSymId_t symid = mapPlayerCountries.Find( szLocation );
			if ( symid == UTL_INVAL_SYMBOL )
				symid = mapPlayerCountries.Insert( szLocation, 0.0f );
			float flNewWeightOfThisCountry = (
				mapPlayerCountries[symid] += ( 1.0f + ( ( CSteamID( pPlayer->GetUint64( "xuid" ) ).GetAccountID() == s_mysteamid.GetAccountID() ) ? 0.5f : 0.0f ) )
				);
			if ( flNewWeightOfThisCountry > flBestCountryWeight )
			{
				szBestCountry = szLocation;
				flBestCountryWeight = flNewWeightOfThisCountry;
			}
		}
	}

	if ( !nAvgRank )
	{
		nAvgRank = 7 * 10; // Nova II
	}
	else if ( nHaveRank == nTotalPlayers )
	{
		nAvgRank = ( nAvgRank * 10 ) / nHaveRank;
	}
	else
	{
		int nAvgRankedPeople = ( nAvgRank * 10 );
		for ( ; nHaveRank < nTotalPlayers; ++ nHaveRank )
		{
			nAvgRankedPeople += ( nAvgRankedPeople * 9 / nHaveRank / 10 );
		}
		nAvgRank = nAvgRankedPeople / nHaveRank;
	}

	int nAPR = bAllPrime ? 1 : 0;
	if ( *pFullGameSettings->GetString( "game/mmqueue" ) )
		nAPR |= 0x2;

	int numSlots = 5;
	if ( !V_stricmp( pFullGameSettings->GetString( "game/mode" ), "cooperative" ) )
		numSlots = 2;
	if ( nTotalPlayers >= numSlots )
		nAPR |= 0x4;

	pUpdate->SetInt( "game/ark", nAvgRank );
	pUpdate->SetInt( "game/apr", nAPR );
	pUpdate->SetString( "game/loc", szBestCountry );

#ifdef _DEBUG
	DevMsg( "UpdateAggregateMembersSettings: ark %d->%d, apr %d->%d, loc %s->%s\n",
		pFullGameSettings->GetInt( "game/ark" ), nAvgRank,
		pFullGameSettings->GetInt( "game/apr" ), nAPR,
		pFullGameSettings->GetString( "game/loc" ), szBestCountry );
#endif
}

// Executes the command on the session settings, this function on host
// is allowed to modify Members/Game subkeys and has to fill in modified players KeyValues
// When running on a remote client "ppPlayersUpdated" is NULL and players cannot
// be modified
void CMatchTitleGameSettingsMgr::ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated )
{
	char const *szCommand = pCommand->GetName();

	if ( !Q_stricmp( "Game::SetPlayerRanking", szCommand ) )
	{
		if ( !Q_stricmp( "host", pSessionSystemData->GetString( "type", "host" ) ) &&
			// !Q_stricmp( "lobby", pSessionSystemData->GetString( "state", "lobby" ) ) && - avatars also update when players change team ingame
			ppPlayersUpdated )
		{
			XUID xuidPlayer = pCommand->GetUint64( "xuid" );

			// We know that the current session is the host. Validate that it is either updating itself, or a remote user
			// is updating their own data
			if ( !pSessionSystemData	// offline session OK
				|| ( ( xuidPlayer == pSessionSystemData->GetUint64( "xuidHost" ) ) && !pCommand->GetUint64( "_remote_xuidsrc" ) ) // host player update issued locally
				|| ( xuidPlayer == pCommand->GetUint64( "_remote_xuidsrc" ) ) // remote command on behalf of matching XUID
				)
				; // ok to process this update
			else
				return;

			// Find the layer that is going to be updated
			KeyValues *pPlayer = SessionMembersFindPlayer( pSettings, xuidPlayer );
			if ( !pPlayer )
				return;

			KeyValues *kvGameKey = pCommand->FindKey( "game" );
			if ( pPlayer && kvGameKey )
			{
				KeyValues *kvPlayerGame = pPlayer->FindKey( "game", true );
				if ( !kvPlayerGame )
					return;

				kvPlayerGame->MergeFrom( kvGameKey, KeyValues::MERGE_KV_UPDATE );

				// Notify the sessions of a player update
				* ( ppPlayersUpdated ++ ) = pPlayer;

				// Also recompute aggregate session fields
				if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
				{
					KeyValues *kvPackage = new KeyValues( "Update" );
					if ( KeyValues *kvUpdate = kvPackage->FindKey( "update", true ) )
					{
						UpdateAggregateMembersSettings( pMatchSession->GetSessionSettings(), kvUpdate );
					}
					pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( kvPackage ) );
				}
			}
			return;
		}
	}
}

void CMatchTitleGameSettingsMgr::LoadMatchSettings( void )
{
	// Only load the match settings once.
	if ( m_pMatchSystemData )
	{
		return;
	}

	m_pMatchSystemData = new KeyValues( "" );

	// Load the match system keyvalues file if we haven't already.
	if ( !m_pMatchSystemData->LoadFromFile( g_pFullFileSystem, "resource\\MatchSystem.res", "GAME" ) )
	{
		m_pMatchSystemData->deleteThis();
		m_pMatchSystemData = NULL;
	}

	if ( !m_pMatchSystemData )
		return;

	// Get the version of the match file and compare to our latest parsed version.
	// Will be used for PROPERTY_CSS_MATCH_VERSION.
	int version = m_pMatchSystemData->GetInt( "version", -1 );
	if ( ( mm_title_debug_version.GetInt() < 100 ) && ( mm_title_debug_version.GetInt() >= version ) )
	{
		// Version file is not newer, so use what we already have.
		AssertMsg( mm_title_debug_version.GetInt() >= 0, "Failed to load any match settings.  Matchmaking will likely fail." );
		return;
	}
	if ( mm_title_debug_version.GetInt() < 100 )
		mm_title_debug_version.SetValue( version );

	// Remove all previous formulas.
	m_FormulaExperience.Purge();

	// Load the experience formula.
	KeyValues *pExperienceFormula = m_pMatchSystemData->FindKey( "ExperienceFormula" );
	if ( pExperienceFormula )
	{
		// Search for keys that are a number that corresponds to the skill number they should be used for.
		for ( int nSkillIndex=0; ; ++nSkillIndex )
		{
			char const *pszFormula = pExperienceFormula->GetString( CFmtStr( "%d", nSkillIndex ), NULL );
			if ( !pszFormula || !*pszFormula )
			{
				// No more formulas specified.
				break;
			}
			m_FormulaExperience.AddToTail( pszFormula );
		}

		// Get the min/max range for valid values for these formulas.
		m_nFormulaExperienceRangeMin = pExperienceFormula->GetInt( "minvalue", 0 );
		m_nFormulaExperienceRangeMax = pExperienceFormula->GetInt( "maxvalue", INT_MAX );
	}

	// Remove all previous formulas.
	for ( int i=0; i<m_FormulaSkill.Count(); ++i )
	{
		delete m_FormulaSkill[i];
	}
	m_FormulaSkill.Purge();

	// Load the skill formulas.
	for ( int nSkillFormulaIndex=0; ; ++nSkillFormulaIndex )
	{
		KeyValues *pSkillFormulaInfo = m_pMatchSystemData->FindKey( CFmtStr( "Skill%dFormula", nSkillFormulaIndex ) );
		if ( !pSkillFormulaInfo )
		{
			// No more formulas specified.
			break;
		}

		SkillFormulas *pSkillFormulas = new SkillFormulas();

		// Search for keys that are a number that corresponds to the skill number they should be used for.
		for ( int nSkillIndex=0; ; ++nSkillIndex )
		{
			char const *pszFormula = pSkillFormulaInfo->GetString( CFmtStr( "%d", nSkillIndex ), NULL );
			if ( !pszFormula || !*pszFormula )
			{
				// No more formulas specified.
				break;
			}
			pSkillFormulas->formulas.AddToTail( pszFormula );
		}

		// Get the min/max range for valid values for these formulas.
		pSkillFormulas->rangeMin = pSkillFormulaInfo->GetInt( "minvalue", 0 );
		pSkillFormulas->rangeMax = pSkillFormulaInfo->GetInt( "maxvalue", INT_MAX );

		m_FormulaSkill.AddToTail( pSkillFormulas );
	}

	// Load the average formula.
	KeyValues *pAverageFormula = m_pMatchSystemData->FindKey( "AvgFormula" );
	if ( pAverageFormula )
	{
		// Search for keys that are a number that corresponds to the skill number they should be used for.
		for ( int nSkillIndex=0; ; ++nSkillIndex )
		{
			char const *pszFormula = pAverageFormula->GetString( CFmtStr( "%d", nSkillIndex ), NULL );
			if ( !pszFormula || !*pszFormula )
			{
				// No more formulas specified.
				break;
			}
			m_FormulaAverage.AddToTail( pszFormula );
		}
	}

	// Remove all previous search passes.
	for ( int i=0; i<m_SearchPass.Count(); ++i )
	{
		delete m_SearchPass[i];
	}
	m_SearchPass.Purge();

	// Load the search passes.
	for ( int nSearchPassIndex=0; ; ++nSearchPassIndex )
	{
		KeyValues *pSearchPassInfo = m_pMatchSystemData->FindKey( CFmtStr( "SearchPass%d", nSearchPassIndex ) );
		if ( !pSearchPassInfo )
		{
			// No more passes specified.
			break;
		}

		SearchPass *pSearchPass = new SearchPass();

		// Get the experience checks for this search pass.
		pSearchPass->checkExperience = ( pSearchPassInfo->GetInt( "ExpCheck", 0 ) != 0 );
		pSearchPass->experienceRange = pSearchPassInfo->GetInt( "ExperienceRange", 0 );

		// Loop over all of the possible Skill#Check and Skill#Range entries
		// so we can store their values for quick reference.
		for ( int nSkillIndex=0; nSkillIndex<MATCH_MAX_SKILL_FIELDS; ++nSkillIndex )
		{
			pSearchPass->checkSkill.AddToTail( ( pSearchPassInfo->GetInt( CFmtStr( "Skill%dCheck", nSkillIndex ), 0 ) != 0 ) );
			pSearchPass->skillRange.AddToTail(  pSearchPassInfo->GetInt( CFmtStr( "Skill%dRange", nSkillIndex ), 0 ) );
		}

		m_SearchPass.AddToTail( pSearchPass );
	}
}

// Retrieves the indexed formula from the match system settings file. (MatchSystem.360.res)
char const * CMatchTitleGameSettingsMgr::GetFormulaAverage( int index )
{
	// Ensure the matchmaking settings are loaded.
	LoadMatchSettings();

	if ( !m_FormulaAverage.Count() )
		return "newValue";

	int indexClamped = clamp( index, 0, m_FormulaAverage.Count() - 1 );
	return m_FormulaAverage[ indexClamped ].String();
}

// Add Steam version of X360 matchmaking rules. Writes to pResult
void CMatchTitleGameSettingsMgr::AddSteamMatchmakingRule( KeyValues *pResult, bool bAllSessions, 
	KeyValues *pSettings, bool bCssMatchVersion, bool bCssLevel, bool bCssGameType, 
	bool bCssGameMode, bool bTeamMatch )
{
	// Check for official server
	char const *serverType = pSettings->GetString( "options/server", "");
	if ( !Q_stricmp( serverType, "official" ))
	{
		pResult->SetString( "Filter=/options:server", serverType);
	}

	// Determine whether we are looking for community games or not
	int iHosted = pSettings->GetInt( "game/hosted", 0 );
	pResult->SetInt( "Filter=/game:hosted", iHosted );
	int minPlayers = mm_csgo_community_search_players_min.GetInt();

	// even if the key is not set on the lobby the MMS code for numerical compares
	// must pass the check due to comparing it against default zero value

	// Check for sv_search_key
	char const *searchKey = pSettings->GetString( "game/search_key", NULL );
	if ( searchKey )
	{
		pResult->SetString( "Filter=/game:search_key", searchKey );
	}

	// Set up key values that are common across SESSION_MATCH_QUERY_PLAYER_MATCH 
	// and SESSION_MATCH_QUERY_PLAYER_MATCH_ANY_LEVEL
	if ( !bAllSessions )
	{
		const char *pMapGroupName = pSettings->GetString( "game/mapgroupname", NULL );
		if ( pMapGroupName && strstr( pMapGroupName, "@workshop" ) && pSettings->GetBool( "options/anytypemode" ) )
		{
			bCssGameMode = false;
			bCssGameType = false;
		}

		// Game mode
		if (bCssGameMode)
		{
			char const *gameMode = pSettings->GetString( "game/mode", NULL );
			AssertMsg(gameMode, "Matchmaking: Rule SESSION_MATCH_QUERY_PLAYER_MATCH - no game mode; ignoring this filter");
			if ( gameMode )
			{
				pResult->SetString("Filter=/game:mode", gameMode);
			}
		}

#ifdef _X360
		// Matchmaking rules version
		if (bCssMatchVersion)
		{
			pResult->SetInt("Filter=/game:matchversion", mm_title_debug_version.GetInt() );
		}
#endif

		// Privacy
		char const *privacy = pSettings->GetString( "system/access", NULL );
		AssertMsg(privacy, "Matchmaking: Rule SESSION_MATCH_QUERY_PLAYER_MATCH - no access mode; ignoring this filter");
		if ( privacy )
		{
			pResult->SetString("Filter=/system:access", privacy);
		}

		// Game type
		if (bCssGameType)
		{
			char const *gameType = pSettings->GetString( "game/type", NULL );
			//AssertMsg(gameType, "Matchmaking: Rule SESSION_MATCH_QUERY_PLAYER_MATCH - no game type; ignoring this filter");
			if (gameType)
			{
				pResult->SetString("Filter=/game:type", gameType);
			}
		}
	}

	// Map name
	if (bCssLevel)
	{
		const char *pMapGroupName = pSettings->GetString( "game/mapgroupname", NULL );
		//AssertMsg(pMapGroupName, "Matchmaking: Rule SESSION_MATCH_QUERY_PLAYER_MATCH - no mapgroup name; ignoring this filter");
		if ( pMapGroupName )
		{
			// Check for workshop map matchmaking case
			if ( char const *pszWorkshopMapGroup = strstr( pMapGroupName, "@workshop" ) )
			{
				// search based on workshop map group regardless of the collection
				pResult->SetString( "Filter=/game:map", pszWorkshopMapGroup + 1 );
				minPlayers = 0; // when searching for a workshop map don't require min players
			}
			else
			{
				pResult->SetString("Filter=/game:mapgroupname", pMapGroupName);
			}
		}
	}

	if ( iHosted && ( minPlayers > 0 ) )
	{
		pResult->SetInt( "Filter>=/members:numPlayers", minPlayers );
	}
	else
	{
		pResult->SetInt( "Filter</members:numPlayers", 5 );	// always look for lobbies that are not full
	}

	if ( const char *pGameState = pSettings->GetString( "game/state", NULL ) )
	{
		pResult->SetString( "Filter=/game:state", pGameState );
	}

	if ( KeyValues *kvAllPrime = pSettings->FindKey( "game/apr" ) )
	{
		pResult->SetInt( "Filter=/game:apr", kvAllPrime->GetInt() );
	}

	if ( KeyValues *kvNearRank = pSettings->FindKey( "game/ark" ) )
	{
		if ( !kvNearRank->GetInt() )
		{
			pResult->SetInt( "Filter>=/game:ark", 7*10 );	// Nova 1
			pResult->SetInt( "Filter<=/game:ark", 12*10 );	// MG II
			pResult->SetInt( "Near/game:ark", 9*10 );	// Nova III
		}
		else
		{
			pResult->SetInt( "Filter>=/game:ark", ( kvNearRank->GetInt() - 2 ) * 10 );
			pResult->SetInt( "Filter<=/game:ark", ( kvNearRank->GetInt() + 2 ) * 10 );
			pResult->SetInt( "Near/game:ark", kvNearRank->GetInt()*10 );
		}
		pResult->SetInt( "Near/members:numPlayers", 5 );	// always look for lobbies that are close to full
	}

	if ( const char *pMMQueue = pSettings->GetString( "game/mmqueue", NULL ) )
	{
		pResult->SetString( "Filter=/game:mmqueue", pMMQueue );
	}

	if ( const char *szClanID = pSettings->GetString( "game/clanid", NULL ) )
	{
		pResult->SetString( "Filter=/game:clanid", szClanID );
	}
}

// Called by the client to notify matchmaking that it should update matchmaking properties based
// on player distribution among the teams.
void CMatchTitleGameSettingsMgr::UpdateTeamProperties( KeyValues *pCurrentSettings, KeyValues *pTeamProperties )
{
	MM_Title_RichPresence_UpdateTeamPropertiesCSGO( pCurrentSettings, pTeamProperties );
}

#ifdef _X360
void MM_dumpcontextsandproperties( void )
{
	IXboxSystem *pXboxSystem = g_pMatchExtensions->GetIXboxSystem();
	if ( pXboxSystem )
	{
		DWORD userIndex = XBX_GetPrimaryUserId();

		uint value = 0;

		KeyValues *pkv = new KeyValues( "ContextsAndProperties" );
		KeyValues::AutoDelete autoDeleteEvent( pkv );

		pXboxSystem->UserGetContext( userIndex, X_CONTEXT_GAME_TYPE, value );
		pkv->SetInt( CFmtStr( "Contexts/%d\t\t(X_CONTEXT_GAME_TYPE)", X_CONTEXT_GAME_TYPE ), value );

		pXboxSystem->UserGetContext( userIndex, X_CONTEXT_GAME_MODE, value );
		pkv->SetInt( CFmtStr( "Contexts/%d\t\t(X_CONTEXT_GAME_MODE)", X_CONTEXT_GAME_MODE ), value );

		pXboxSystem->UserGetContext( userIndex, CONTEXT_CSS_LEVEL, value );
		pkv->SetInt( CFmtStr( "Contexts/%d\t\t(CONTEXT_CSS_LEVEL)", CONTEXT_CSS_LEVEL ), value );

		pXboxSystem->UserGetContext( userIndex, CONTEXT_CSS_MAP_GROUP, value );
		pkv->SetInt( CFmtStr( "Contexts/%d\t\t(CONTEXT_CSS_MAP_GROUP)", CONTEXT_CSS_MAP_GROUP ), value );

		pXboxSystem->UserGetContext( userIndex, CONTEXT_GAME_STATE, value );
		pkv->SetInt( CFmtStr( "Contexts/%d\t\t(CONTEXT_GAME_STATE)", CONTEXT_GAME_STATE ), value );

		pXboxSystem->UserGetContext( userIndex, CONTEXT_CSS_GAME_MODE, value );
		pkv->SetInt( CFmtStr( "Contexts/%d\t\t(CONTEXT_CSS_GAME_MODE)", CONTEXT_CSS_GAME_MODE ), value );

		pXboxSystem->UserGetContext( userIndex, CONTEXT_CSS_TEAM, value );
		pkv->SetInt( CFmtStr( "Contexts/%d\t\t(CONTEXT_CSS_TEAM)", CONTEXT_CSS_TEAM ), value );

		pXboxSystem->UserGetContext( userIndex, CONTEXT_CSS_PRIVACY, value );
		pkv->SetInt( CFmtStr( "Context/%d\t\t(CONTEXT_CSS_PRIVACY)", CONTEXT_CSS_PRIVACY ), value );

		pXboxSystem->UserGetContext( userIndex, CONTEXT_CSS_GAME_TYPE, value );
		pkv->SetInt( CFmtStr( "Context/%d\t\t(CONTEXT_CSS_GAME_TYPE)", CONTEXT_CSS_GAME_TYPE ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_AGGREGATE_EXPERIENCE, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_AGGREGATE_EXPERIENCE)", PROPERTY_CSS_AGGREGATE_EXPERIENCE ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_AGGREGATE_SKILL0, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_AGGREGATE_SKILL0)", PROPERTY_CSS_AGGREGATE_SKILL0 ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_AGGREGATE_SKILL1, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_AGGREGATE_SKILL1)", PROPERTY_CSS_AGGREGATE_SKILL1 ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_AGGREGATE_SKILL2, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_AGGREGATE_SKILL2)", PROPERTY_CSS_AGGREGATE_SKILL2 ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_AGGREGATE_SKILL3, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_AGGREGATE_SKILL3)", PROPERTY_CSS_AGGREGATE_SKILL3 ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_AGGREGATE_SKILL4, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_AGGREGATE_SKILL4)", PROPERTY_CSS_AGGREGATE_SKILL4 ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_MATCH_VERSION, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_MATCH_VERSION)", PROPERTY_CSS_MATCH_VERSION ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_OPEN_SLOTS, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_OPEN_SLOTS)", PROPERTY_CSS_OPEN_SLOTS ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_EXP_MAX, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_EXP_MAX)", PROPERTY_CSS_SEARCH_EXP_MAX ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_EXP_MIN, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_EXP_MIN)", PROPERTY_CSS_SEARCH_EXP_MIN ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_SKILL0_MAX, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_SKILL0_MAX)", PROPERTY_CSS_SEARCH_SKILL0_MAX ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_SKILL0_MIN, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_SKILL0_MIN)", PROPERTY_CSS_SEARCH_SKILL0_MIN ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_SKILL1_MAX, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_SKILL1_MAX)", PROPERTY_CSS_SEARCH_SKILL1_MAX ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_SKILL1_MIN, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_SKILL1_MIN)", PROPERTY_CSS_SEARCH_SKILL1_MIN ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_SKILL2_MAX, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_SKILL2_MAX)", PROPERTY_CSS_SEARCH_SKILL2_MAX ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_SKILL2_MIN, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_SKILL2_MIN)", PROPERTY_CSS_SEARCH_SKILL2_MIN ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_SKILL3_MAX, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_SKILL3_MAX)", PROPERTY_CSS_SEARCH_SKILL3_MAX ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_SKILL3_MIN, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_SKILL3_MIN)", PROPERTY_CSS_SEARCH_SKILL3_MIN ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_SKILL4_MAX, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_SKILL4_MAX)", PROPERTY_CSS_SEARCH_SKILL4_MAX ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_SEARCH_SKILL4_MIN, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_SEARCH_SKILL4_MIN)", PROPERTY_CSS_SEARCH_SKILL4_MIN ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_GAME_MODE_AS_NUMBER, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_GAME_MODE_AS_NUMBER)", PROPERTY_CSS_GAME_MODE_AS_NUMBER ), value );

		pXboxSystem->UserGetPropertyInt( userIndex, PROPERTY_CSS_MAX_OPEN_TEAM_SLOTS, value );
		pkv->SetInt( CFmtStr( "Properties/%d\t\t(PROPERTY_CSS_MAX_OPEN_TEAM_SLOTS)", PROPERTY_CSS_MAX_OPEN_TEAM_SLOTS ), value );

		KeyValuesDumpAsDevMsg( pkv );
	}
}

static ConCommand mm_dumpcontextsandproperties("mm_dumpcontextsandproperties", MM_dumpcontextsandproperties, "Dump the current values for all of the title's contexts and properties.", FCVAR_DEVELOPMENTONLY );
#endif // _X360

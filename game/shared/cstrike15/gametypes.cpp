//========= Copyright (c) 1996-2011, Valve Corporation, All rights reserved. ============//
//
// Purpose: Game types and modes
//
// $NoKeywords: $
//=============================================================================//


//
// NOTE: This is horrible design to have this file included in multiple projects (client.dll, server.dll, matchmaking.dll)
// which don't have same interfaces applicable to interact with the engine and don't have sufficient context here
// hence these compile-time conditionals to use appropriate interfaces
//
#if defined( CLIENT_DLL ) || defined( GAME_DLL )
#include "cbase.h"
#endif

#if defined( MATCHMAKING_DS_DLL )
#include "eiface.h"
#include "matchmaking/imatchframework.h"
#endif

#include "gametypes.h"

#include "strtools.h"
#include "dlchelper.h"
#include "../engine/filesystem_engine.h"
#include "filesystem.h"
#include "tier2/fileutils.h"

#include "matchmaking/cstrike15/imatchext_cstrike15.h"

#if defined ( MATCHMAKING_DLL )
#include "../../../matchmaking/mm_extensions.h"
#endif


#include "fmtstr.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

void DisplayGameModeConvars( void );

// The following convars depend on the order of the game types and modes in GameModes.txt.
ConVar game_online( "game_online", "1", FCVAR_REPLICATED | FCVAR_HIDDEN | FCVAR_GAMEDLL | FCVAR_CLIENTDLL, "The current game is online." );
ConVar game_public( "game_public", "1", FCVAR_REPLICATED | FCVAR_HIDDEN | FCVAR_GAMEDLL | FCVAR_CLIENTDLL, "The current game is public." );
ConVar game_type( "game_type", "0", FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_CLIENTDLL, "The current game type. See GameModes.txt." );
ConVar game_mode( "game_mode", "0", FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_CLIENTDLL, "The current game mode (based on game type). See GameModes.txt." );
ConVar custom_bot_difficulty( "custom_bot_difficulty", "0", FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_CLIENTDLL, "Bot difficulty for offline play." );

#if defined( CLIENT_DLL )
ConCommand cl_game_mode_convars( "cl_game_mode_convars", DisplayGameModeConvars, "Display the values of the convars for the current game_mode." );
#elif defined( GAME_DLL )
ConCommand sv_game_mode_convars( "sv_game_mode_convars", DisplayGameModeConvars, "Display the values of the convars for the current game_mode." );
#endif

// HACKY: Ok, so this file is compiled into 3 different modules so we get three different
// static game types objects... Unfortunately needs to do different things for servers and clients.
// These helpers test if we're on a client and/or server... The exposed interface is always pointing
// to the matchmaking dll instance so using its interfaces to tell where we are.
bool GameTypes_IsOnServer( void )
{
#if defined ( GAME_DLL )
	return true;
#endif
#if defined ( MATCHMAKING_DLL )
	return ( g_pMatchExtensions->GetIVEngineServer() != NULL );
#endif
	return false;
}
bool GameTypes_IsOnClient( void )
{
#if defined ( CLIENT_DLL )
	return true;
#endif
#if defined ( MATCHMAKING_DLL )
	return ( g_pMatchExtensions->GetIVEngineClient() != NULL );
#endif
	return false;
}

static const int g_invalidInteger = -1;
static uint32 g_richPresenceDefault = 0xFFFF;

// ============================================================================================ //
// GameType
// ============================================================================================ //

// -------------------------------------------------------------------------------------------- //
// Purpose: Constructor
// -------------------------------------------------------------------------------------------- //
GameTypes::GameType::GameType()
	: m_Index( g_invalidInteger )
{
	m_Name[0] = '\0';
	m_NameID[0] = '\0';
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Destructor
// -------------------------------------------------------------------------------------------- //
GameTypes::GameType::~GameType()
{
	m_GameModes.PurgeAndDeleteElements();
}

// ============================================================================================ //
// GameMode
// ============================================================================================ //

// -------------------------------------------------------------------------------------------- //
// Purpose: Constructor
// -------------------------------------------------------------------------------------------- //
GameTypes::GameMode::GameMode()
	: m_Index( g_invalidInteger ),
	  m_pExecConfings( NULL )
{
	m_Name[0] = '\0';
	m_NameID[0] = '\0';
	m_DescID[0] = '\0';
	m_NameID_SP[0] = '\0';
	m_DescID_SP[0] = '\0';
	m_MaxPlayers = 1;
	m_NoResetVoteThresholdCT = -1;
	m_NoResetVoteThresholdT = -1;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Destructor
// -------------------------------------------------------------------------------------------- //
GameTypes::GameMode::~GameMode()
{
	if ( m_pExecConfings )
	{
		m_pExecConfings->deleteThis();
	}
}

// ============================================================================================ //
// Map
// ============================================================================================ //

// -------------------------------------------------------------------------------------------- //
// Purpose: Constructor
// -------------------------------------------------------------------------------------------- //
GameTypes::Map::Map()
	: m_Index( g_invalidInteger ),
	  m_RichPresence( g_richPresenceDefault )
{
	m_Name[0] = '\0';
	m_NameID[0] = '\0';
	m_ImageName[0] = '\0';
	m_RequiresAttr[0] = 0;
	m_RequiresAttrValue = -1;
	m_RequiresAttrReward[0] = 0;
	m_nRewardDropList = -1;
}

// ============================================================================================ //
// MapGroup
// ============================================================================================ //

// -------------------------------------------------------------------------------------------- //
// Purpose: Constructor
// -------------------------------------------------------------------------------------------- //
GameTypes::MapGroup::MapGroup()
{
	m_Name[0] = '\0';
	m_NameID[0] = '\0';
	m_ImageName[0] = '\0';
	m_bIsWorkshopMapGroup = false;
}

// ============================================================================================ //
// CustomBotDifficulty
// ============================================================================================ //

// -------------------------------------------------------------------------------------------- //
// Purpose: Constructor
// -------------------------------------------------------------------------------------------- //
GameTypes::CustomBotDifficulty::CustomBotDifficulty()
	: m_Index( g_invalidInteger ),
	  m_pConvars( NULL ),
	  m_HasBotQuota( false )
{
	m_Name[0] = '\0';
	m_NameID[0] = '\0';
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Destructor
// -------------------------------------------------------------------------------------------- //
GameTypes::CustomBotDifficulty::~CustomBotDifficulty()
{
	if ( m_pConvars )
	{
		m_pConvars->deleteThis();
	}
}

// ============================================================================================ //
// GameTypes
// ============================================================================================ //

// Singleton
static GameTypes s_GameTypes;
IGameTypes *g_pGameTypes = &s_GameTypes;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( GameTypes, IGameTypes, VENGINE_GAMETYPES_VERSION, s_GameTypes );

// -------------------------------------------------------------------------------------------- //
// Purpose: Constructor
// -------------------------------------------------------------------------------------------- //
GameTypes::GameTypes()
	: m_Initialized( false ),
	m_pExtendedServerInfo( NULL ),
	m_pServerMap( NULL ),
	m_pServerMapGroup( NULL ),
	m_iCurrentServerNumSlots( 0 ),
	m_bRunMapWithDefaultGametype( false ),
	m_bLoadingScreenDataIsCorrect( true )
{
	m_randomStream.SetSeed( (int)Plat_MSTime() );
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Destructor
// -------------------------------------------------------------------------------------------- //
GameTypes::~GameTypes()
{
	m_GameTypes.PurgeAndDeleteElements();
	m_Maps.PurgeAndDeleteElements();
	m_MapGroups.PurgeAndDeleteElements();
	m_CustomBotDifficulties.PurgeAndDeleteElements();

	if ( m_pExtendedServerInfo )
		m_pExtendedServerInfo->deleteThis();
	m_pExtendedServerInfo = NULL;

	ClearServerMapGroupInfo();
}

void GameTypes::ClearServerMapGroupInfo( void )
{
	if ( m_pServerMap )
		delete m_pServerMap;
	m_pServerMap = NULL;

	if ( m_pServerMapGroup )
		delete m_pServerMapGroup;
	m_pServerMapGroup = NULL;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Loads the contents of GameModes.txt
// -------------------------------------------------------------------------------------------- //
bool GameTypes::Initialize( bool force /* = false*/ )
{
	const char *fileName = "GameModes.txt";

	if ( m_Initialized &&
		 !force )
	{
		return true;
	}

	m_GameTypes.PurgeAndDeleteElements();
	m_Maps.PurgeAndDeleteElements();
	m_CustomBotDifficulties.PurgeAndDeleteElements();

	ClearServerMapGroupInfo();

	KeyValues *pKV = new KeyValues( "" );

	KeyValues::AutoDelete autodelete( pKV );

	DevMsg( "GameTypes: initializing game types interface from %s.\n", fileName );

	// Load the key values from the disc.
	if ( !pKV->LoadFromFile( g_pFullFileSystem, fileName ) )
	{
		Warning( "GameTypes: error loading %s.", fileName );
		return false;
	}

//	pKV->SaveToFile( g_pFullFileSystem, "maps/gamemodes-pre.txt", "GAME" );

	// Load key values from any DLC on disc.
	DLCHelper::AppendDLCKeyValues( pKV, fileName );

	// Merge map sidecar files ( map.kv )
	// Map sidecar loading has been moved to CCSGameRules::InitializeGameTypeAndMode
	// to eliminate large workshop subscription load times
	//	InitMapSidecars( pKV );

	// Lastly, merge key values from gamemodes_server.txt or from the file specified on the
	// command line.
	const char *svfileName;

	svfileName = CommandLine()->ParmValue( "-gamemodes_serverfile" );

	if ( !svfileName )
		svfileName = "gamemodes_server.txt";

	DevMsg( "GameTypes: merging game types interface from %s.\n", svfileName );

	KeyValues *pKV_sv = new KeyValues( "" );

	if ( pKV_sv->LoadFromFile( g_pFullFileSystem, svfileName ) )
	{
		// Merge the section that exec's configs in a special way
		for ( KeyValues *psvGameType = pKV_sv->FindKey( "gametypes" )->GetFirstTrueSubKey();
			psvGameType; psvGameType = psvGameType->GetNextTrueSubKey() )
		{
			for ( KeyValues *psvGameMode = psvGameType->FindKey( "gamemodes" )->GetFirstTrueSubKey();
				psvGameMode; psvGameMode = psvGameMode->GetNextTrueSubKey() )
			{
				if ( KeyValues *psvExec = psvGameMode->FindKey( "exec" ) )
				{
					// We have an override for gametype-mode-exec
					if ( KeyValues *pOurExec = pKV->FindKey( CFmtStr( "gametypes/%s/gamemodes/%s/exec", psvGameType->GetName(), psvGameMode->GetName() ) ) )
					{
						for ( KeyValues *psvConfigEntry = psvExec->GetFirstValue();
							psvConfigEntry; psvConfigEntry = psvConfigEntry->GetNextValue() )
						{
							pOurExec->AddSubKey( psvConfigEntry->MakeCopy() );
						}

						psvGameMode->RemoveSubKey( psvExec );
						psvExec->deleteThis();
					}
				}
			}
		}

		// for modes that have weapon progressions remove pre-existing weapon progressions if the server file has them
		for ( KeyValues *psvGameType = pKV_sv->FindKey( "gametypes" )->GetFirstTrueSubKey();
			psvGameType; psvGameType = psvGameType->GetNextTrueSubKey() )
		{
			for ( KeyValues *psvGameMode = psvGameType->FindKey( "gamemodes" )->GetFirstTrueSubKey();
				psvGameMode; psvGameMode = psvGameMode->GetNextTrueSubKey() )
			{
				if ( KeyValues *psvProgressionCT = psvGameMode->FindKey( "weaponprogression_ct" ) )
				{
					// We have an override for gametype-mode-weaponprogression_ct
					if ( KeyValues *pOurProgressionCT = pKV->FindKey( CFmtStr( "gametypes/%s/gamemodes/%s/weaponprogression_ct", psvGameType->GetName(), psvGameMode->GetName() ) ) )
					{
						// remove the pre-existing progression
						pOurProgressionCT->Clear();
					}
				}
				if ( KeyValues *psvProgressionT = psvGameMode->FindKey( "weaponprogression_t" ) )
				{
					// We have an override for gametype-mode-weaponprogression_ct
					if ( KeyValues *pOurProgressionT = pKV->FindKey( CFmtStr( "gametypes/%s/gamemodes/%s/weaponprogression_t", psvGameType->GetName(), psvGameMode->GetName() ) ) )
					{
						// remove the pre-existing progression
						pOurProgressionT->Clear();
					}
				}
			}
		}

		pKV->MergeFrom( pKV_sv, KeyValues::MERGE_KV_UPDATE );
	}
	else
	{
		DevMsg( "Failed to load %s\n", svfileName );
	}

	if ( pKV_sv )
	{
		pKV_sv->deleteThis();
		pKV_sv = NULL;
	}

	// Load the game types.
	if ( !LoadGameTypes( pKV ) )
	{
		return false;
	}

	// Load the maps.
	if ( !LoadMaps( pKV ) )
	{
		return false;
	}

	// Load the map groups.
	if ( !LoadMapGroups( pKV ) )
	{
		return false;
	}

	// Load the bot difficulty levels for Offline games.
	if ( !LoadCustomBotDifficulties( pKV ) )
	{
		return false;
	}

	m_Initialized = true;

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Purpose: load and merge key values from loose map sidecar files ( "de_dust_joeblow.kv" )
void GameTypes::InitMapSidecars( KeyValues* pKV )
{
	KeyValues *pKVMaps = pKV->FindKey( "maps" );

	char mapwild[MAX_PATH];
	Q_snprintf( mapwild, sizeof( mapwild ), "*.%sbsp", IsX360() ? "360." : "" );
	CUtlVector<CUtlString> outList;
	// BEGIN Search the maps dir for .kv files that correspond to bsps.
	RecursiveFindFilesMatchingName( &outList, "maps", mapwild, "GAME" );
	FOR_EACH_VEC( outList, i )
	{
		const char* curMap = outList[i].Access();
		AddMapKVs( pKVMaps, curMap );
	}
	 // END Search
}

////////////////////////////////////////////////////////////////////////////////////////////
// Purpose: shove a map KV data into the main gamemodes data
void GameTypes::AddMapKVs( KeyValues* pKVMaps, const char* curMap )
{
	char filename[ MAX_PATH ];
	char kvFilename[ MAX_PATH ];
	KeyValues *pKVMap;

	V_StripExtension( curMap, filename, MAX_PATH );
	V_FixSlashes( filename, '/' );
	V_snprintf( kvFilename, sizeof( kvFilename ), "%s.kv", filename );

	if ( !g_pFullFileSystem->FileExists( kvFilename ) )
	{
		if ( !StringHasPrefix( filename, "maps/workshop/" ) )
			return;

		char *pchNameBase = strrchr( filename, '/' );
		if ( !pchNameBase )
			return;

		// For workshop maps attempt sidecars by bare non-ID name too
		V_snprintf( kvFilename, sizeof( kvFilename ), "maps/%s.kv", pchNameBase + 1 );
	}

	if ( !g_pFullFileSystem->FileExists( kvFilename ) ) 
		return;
	
	//
	// Load the Map sidecar entry
	//
	const char* szMapNameBase = filename;
	// Strip off the "maps/" to find the correct entry.
	if ( !Q_strnicmp( szMapNameBase, "maps/", 5 ) )
	{
		szMapNameBase += 5;	
	}
	// Delete the existing map subkey if it exists. A map sidecar file stomps existing data for that map.
	KeyValues *pKVOld = pKVMaps->FindKey( szMapNameBase );

	if ( pKVOld )
	{
		// Keep the one defined in gamemodes.txt
		return;
// 
// 			Msg( "GameTypes: Replacing existing entry for %s.\n",  kvFilename );
// 
// 			pKVMaps->RemoveSubKey( pKVOld );
// 
		//				pKV->SaveToFile( g_pFullFileSystem, "maps/map_removed.txt", "GAME" );
	}
	else
	{
		DevMsg( "GameTypes: Creating new entry for %s.\n",  kvFilename );
	}

	pKVMap = pKVMaps->CreateNewKey();

	if ( pKVMap->LoadFromFile( g_pFullFileSystem, kvFilename ) )
	{

		//				pKV->SaveToFile( g_pFullFileSystem, "maps/map_added.txt", "GAME" );
	}
	else
	{
		Warning( "Failed to load %s\n", kvFilename );
	}
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Parse the given key values for the weapon progressions
// -------------------------------------------------------------------------------------------- //
void GameTypes::LoadWeaponProgression( KeyValues * pKV_WeaponProgression, CUtlVector< WeaponProgression > & vecWeaponProgression, const char * szGameType, const char * szGameMode )
{
	if ( pKV_WeaponProgression )
	{
		for ( KeyValues *pKV_Weapon = pKV_WeaponProgression->GetFirstTrueSubKey(); pKV_Weapon; pKV_Weapon = pKV_Weapon->GetNextTrueSubKey() )
		{
			// Get the weapon name.
			WeaponProgression wp;
			wp.m_Name.Set( pKV_Weapon->GetName() );

			// Get the kills.
			const char *killsEntry = "kills";
			wp.m_Kills = pKV_Weapon->GetInt( killsEntry, g_invalidInteger );
			if ( wp.m_Kills == g_invalidInteger )
			{
				wp.m_Kills = 0;
				Warning( "GameTypes: missing %s entry for weapon \"%s\" for game type/mode (%s/%s).\n", killsEntry, pKV_Weapon->GetName(), szGameType, szGameMode );
			}

			vecWeaponProgression.AddToTail( wp );
		}

		if ( vecWeaponProgression.Count() == 0 )
		{
			Warning( "GameTypes: empty %s entry for game type/mode (%s/%s).\n", pKV_WeaponProgression->GetName(), szGameType, szGameMode );
		}
	}
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Finds the index at which the named weapon resides in the progression index.
// -------------------------------------------------------------------------------------------- //
int GameTypes::FindWeaponProgressionIndex( CUtlVector< WeaponProgression > & vecWeaponProgression, const char * szWeaponName )
{
	FOR_EACH_VEC( vecWeaponProgression, tWeapon )
	{
		if ( !V_strcmp( vecWeaponProgression[tWeapon].m_Name, szWeaponName ) )
		{
			return tWeapon;			
		}
	}
	return -1;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Parse the given key values for the game types and modes.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::LoadGameTypes( KeyValues *pKV )
{
	Assert( pKV );
	if ( !pKV )
	{
		return false;
	}

	Assert( m_GameTypes.Count() == 0 );
	if ( m_GameTypes.Count() > 0 )
	{
		m_GameTypes.PurgeAndDeleteElements();
	}

	// Get the game types.
	const char *gameTypesEntry = "gameTypes";
	KeyValues *pKV_GameTypes = pKV->FindKey( gameTypesEntry );
	if ( !pKV_GameTypes )
	{
		Warning( "GameTypes: could not find entry %s.\n", gameTypesEntry );
		return false;
	}

	// Parse the game types.
	for ( KeyValues *pKV_GameType = pKV_GameTypes->GetFirstTrueSubKey(); pKV_GameType; pKV_GameType = pKV_GameType->GetNextTrueSubKey() )
	{
		GameType *pGameType = new GameType();

		// Set the name.
		V_strncpy( pGameType->m_Name, pKV_GameType->GetName(), sizeof( pGameType->m_Name ) );

		// Set the name ID.
		const char *nameIDEntry = "nameID";
		const char *pTypeNameID = pKV_GameType->GetString( nameIDEntry );
		if ( pTypeNameID )
		{
			V_strncpy( pGameType->m_NameID, pTypeNameID, sizeof( pGameType->m_NameID ) );
		}
		else
		{
			Warning( "GameTypes: missing %s entry for game type %s.\n", nameIDEntry, pKV_GameType->GetName() );
		}

		// Get the modes.
		const char *gameModesEntry = "gameModes";
		KeyValues *pKV_GameModes = pKV_GameType->FindKey( gameModesEntry );
		if ( pKV_GameModes )
		{
			for ( KeyValues *pKV_GameMode = pKV_GameModes->GetFirstTrueSubKey(); pKV_GameMode; pKV_GameMode = pKV_GameMode->GetNextTrueSubKey() )
			{
				GameMode *pGameMode = new GameMode();

				// Set the name.
				V_strncpy( pGameMode->m_Name, pKV_GameMode->GetName(), sizeof( pGameMode->m_Name ) );

				// Set the name ID.
				const char *pModeNameID = pKV_GameMode->GetString( nameIDEntry );
				if ( pModeNameID && *pModeNameID != 0 )
				{
					V_strncpy( pGameMode->m_NameID, pModeNameID, sizeof( pGameMode->m_NameID ) );
				}
				else
				{
					Warning( "GameTypes: missing %s entry for game type/mode (%s/%s).\n", nameIDEntry, pKV_GameType->GetName(), pKV_GameMode->GetName() );
				}

				// Set the SP name ID.
				const char *nameIDEntrySP = "nameID_SP";
				const char *pModeNameID_SP = pKV_GameType->GetString( nameIDEntrySP );
				if ( pModeNameID_SP && *pModeNameID_SP != 0 )
				{
					V_strncpy( pGameMode->m_NameID_SP, pModeNameID_SP, sizeof( pGameMode->m_NameID_SP ) );
				}
				else
				{
					if ( pModeNameID && *pModeNameID != 0 )
					{
						V_strncpy( pGameMode->m_NameID_SP, pModeNameID, sizeof( pGameMode->m_NameID_SP ) );
					}	
				}

				// Set the description ID.
				const char *descIDEntry = "descID";
				const char *pDescID = pKV_GameMode->GetString( descIDEntry );
				if ( pDescID && *pDescID != 0 )
				{
					V_strncpy( pGameMode->m_DescID, pDescID, sizeof( pGameMode->m_DescID ) );
				}
				else
				{
					Warning( "GameTypes: missing %s entry for game type/mode (%s/%s).\n", descIDEntry, pKV_GameType->GetName(), pKV_GameMode->GetName() );
				}

				// Set the SP name ID.
				const char *descIDEntrySP = "descID_SP";
				const char *pDescID_SP = pKV_GameMode->GetString( descIDEntrySP );
				if ( pDescID_SP && *pDescID_SP != 0 )
				{
					V_strncpy( pGameMode->m_DescID_SP, pDescID_SP, sizeof( pGameMode->m_DescID_SP ) );
				}
				else
				{
					if ( pDescID && *pDescID != 0 )
					{
						V_strncpy( pGameMode->m_DescID_SP, pDescID, sizeof( pGameMode->m_DescID_SP ) );
					}	
				}

				// check for the command line override first. Otherwise use gamemodes.txt values.
				int maxplayers_override = CommandLine()->ParmValue( "-maxplayers_override", -1 );

				if ( maxplayers_override >= 1 )
				{
					pGameMode->m_MaxPlayers = maxplayers_override;
				}
				else
				{
					// Set the maxplayers for the type/mode.
					const char* maxplayersEntry = "maxplayers";
					int maxplayers = pKV_GameMode->GetInt( maxplayersEntry );
					if ( maxplayers && maxplayers >= 1 )
					{
						pGameMode->m_MaxPlayers = maxplayers;
					}
					else
					{
						Warning( "GameTypes: missing, < 1, or invalid %s entry for game type/mode (%s/%s).\n", maxplayersEntry, pKV_GameType->GetName(), pKV_GameMode->GetName() );
						pGameMode->m_MaxPlayers = 1;
					}

				}


				// Get the single player convars.
				const char *configsEntry = "exec";
				KeyValues *pKVExecConfig = pKV_GameMode->FindKey( configsEntry );
				if ( pKVExecConfig )
				{
					pGameMode->m_pExecConfings = pKVExecConfig->MakeCopy();
				}
				else
				{
					Warning( "GameTypes: missing entry %s for game type/mode (%s/%s).\n", configsEntry, pKV_GameType->GetName(), pKV_GameMode->GetName() );
				}

				// Get the single player mapgroups.
				const char *mapgroupsEntrySP = "mapgroupsSP";
				KeyValues *pKV_MapGroupsSP = pKV_GameMode->FindKey( mapgroupsEntrySP );
				if ( pKV_MapGroupsSP )
				{
					for ( KeyValues *pKV_MapGroup = pKV_MapGroupsSP->GetFirstValue(); pKV_MapGroup; pKV_MapGroup = pKV_MapGroup->GetNextValue() )
					{
						// Ignore the "random" entry.
						if ( V_stricmp( pKV_MapGroup->GetName(), "random" ) == 0 )
						{
							continue;
						}

						pGameMode->m_MapGroupsSP.CopyAndAddToTail( pKV_MapGroup->GetName() );
					}

					if ( pGameMode->m_MapGroupsSP.Count() == 0 )
					{
						Warning( "GameTypes: empty %s entry for game type/mode (%s/%s).\n", mapgroupsEntrySP, pKV_GameType->GetName(), pKV_GameMode->GetName() );
					}
				}
				else
				{
					Warning( "GameTypes: missing %s entry for game type/mode (%s/%s).\n", mapgroupsEntrySP, pKV_GameType->GetName(), pKV_GameMode->GetName() );
				}

				// Get the multiplayer mapgroups.
				const char *mapgroupsEntryMP = "mapgroupsMP";
				KeyValues *pKV_MapGroupsMP = pKV_GameMode->FindKey( mapgroupsEntryMP );
				if ( pKV_MapGroupsMP )
				{
					for ( KeyValues *pKV_MapGroup = pKV_MapGroupsMP->GetFirstValue(); pKV_MapGroup; pKV_MapGroup = pKV_MapGroup->GetNextValue() )
					{
						// Ignore the "random" entry.
						if ( V_stricmp( pKV_MapGroup->GetName(), "random" ) == 0 )
						{
							continue;
						}

						pGameMode->m_MapGroupsMP.CopyAndAddToTail( pKV_MapGroup->GetName() );
					}
				}
				
				// Get the CT weapon progression (optional).
				KeyValues * pKV_WeaponProgressionCT = pKV_GameMode->FindKey( "weaponprogression_ct" );
				LoadWeaponProgression( pKV_WeaponProgressionCT, pGameMode->m_WeaponProgressionCT, pKV_GameType->GetName(), pKV_GameMode->GetName() );

				KeyValues * pKV_WeaponProgressionT = pKV_GameMode->FindKey( "weaponprogression_t" );
				LoadWeaponProgression( pKV_WeaponProgressionT, pGameMode->m_WeaponProgressionT, pKV_GameType->GetName(), pKV_GameMode->GetName() );

				KeyValues * pKV_noResetVoteThresholdT = pKV_GameMode->FindKey( "no_reset_vote_threshold_t" );	
				if ( pKV_noResetVoteThresholdT )
				{
					pGameMode->m_NoResetVoteThresholdT = FindWeaponProgressionIndex( pGameMode->m_WeaponProgressionT, pKV_noResetVoteThresholdT->GetString() );
				}

				KeyValues * pKV_noResetVoteThresholdCT = pKV_GameMode->FindKey( "no_reset_vote_threshold_ct" );
				if ( pKV_noResetVoteThresholdCT )
				{
					pGameMode->m_NoResetVoteThresholdCT = FindWeaponProgressionIndex( pGameMode->m_WeaponProgressionCT, pKV_noResetVoteThresholdCT->GetString() );
				}		

				pGameMode->m_Index = pGameType->m_GameModes.Count();
				pGameType->m_GameModes.AddToTail( pGameMode );
			}				
		}
		else
		{
			Warning( "GameTypes: missing %s entry for game type %s.\n", gameModesEntry, pKV_GameType->GetName() );
		}

		if ( pGameType->m_GameModes.Count() == 0 )
		{
			Warning( "GameTypes: empty %s entry for game type %s.\n", gameModesEntry, pKV_GameType->GetName() );
		}

		pGameType->m_Index = m_GameTypes.Count();
		m_GameTypes.AddToTail( pGameType );
	}

	if ( m_GameTypes.Count() == 0 )
	{
		Warning( "GameTypes: empty %s entry.\n", gameTypesEntry );
	}

	return true;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Parse the given key values for the maps.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::LoadMaps( KeyValues *pKV )
{
	Assert( pKV );
	if ( !pKV )
	{
		return false;
	}

	Assert( m_Maps.Count() == 0 );
	if ( m_Maps.Count() > 0 )
	{
		m_Maps.PurgeAndDeleteElements();
	}

	// Get the maps.
	const char *mapsEntry = "maps";
	KeyValues *pKV_Maps = pKV->FindKey( mapsEntry );
	if ( !pKV_Maps )
	{
		Warning( "GameTypes: could not find entry %s.\n", mapsEntry );
		return false;
	}

	// Parse the maps.
	for ( KeyValues *pKV_Map = pKV_Maps->GetFirstTrueSubKey(); pKV_Map; pKV_Map = pKV_Map->GetNextTrueSubKey() )
	{
		LoadMapEntry( pKV_Map );
	}

	if ( m_Maps.Count() == 0 )
	{
		Warning( "GamesTypes: empty %s entry.\n", mapsEntry );
	}

	return true;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Loads a single map entry.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::LoadMapEntry( KeyValues *pKV_Map )
{
	Assert( pKV_Map );
	if ( !pKV_Map )
	{
		return false;
	}

	Map *pMap = new Map();

	// Set the name.
	V_strcpy_safe( pMap->m_Name, pKV_Map->GetName() );

	// Set the name ID.
	const char *nameIDEntry = "nameID";
	const char *pNameID = pKV_Map->GetString( nameIDEntry );
	if ( pNameID )
	{
		V_strncpy( pMap->m_NameID, pNameID, sizeof( pMap->m_NameID ) );
	}
	else
	{
		Warning( "GameTypes: missing %s entry for map %s.\n", nameIDEntry, pKV_Map->GetName() );
	}

	// Set the image name.
	const char *imageNameEntry = "imagename";
	const char *pImageName = pKV_Map->GetString( imageNameEntry );
	if ( pImageName )
	{
		V_strncpy( pMap->m_ImageName, pImageName, sizeof( pMap->m_ImageName ) );
	}
	else
	{
		Warning( "GameTypes: missing %s entry for map %s.\n", imageNameEntry, pKV_Map->GetName() );
	}

	// Set the economy item requirements
	if ( const char *pszRequiresItem = pKV_Map->GetString( "requires_attr", NULL ) )
	{
		V_strcpy_safe( pMap->m_RequiresAttr, pszRequiresItem );
	}

	pMap->m_RequiresAttrValue = pKV_Map->GetInt( "requires_attr_value", -1 );

	if ( const char *pszRequiresItemAttr = pKV_Map->GetString( "requires_attr_reward", NULL ) )
	{
		V_strcpy_safe( pMap->m_RequiresAttrReward, pszRequiresItemAttr );
	}

	pMap->m_nRewardDropList = pKV_Map->GetInt( "reward_drop_list", -1 );

	// Set the rich presence (optional).
	pMap->m_RichPresence = static_cast<uint32>( pKV_Map->GetInt( "richpresencecontext", g_richPresenceDefault ) );

	// Get the list of terrorist models.
	const char *tModelsEntry = "t_models";
	KeyValues *pKV_TModels = pKV_Map->FindKey( tModelsEntry );
	if ( pKV_TModels )
	{
		for ( KeyValues *pKV_Model = pKV_TModels->GetFirstValue(); pKV_Model; pKV_Model = pKV_Model->GetNextValue() )
		{
			pMap->m_TModels.CopyAndAddToTail( pKV_Model->GetName() );
		}
	}
	else
	{
		Warning( "GameTypes: missing %s entry for map %s.\n", tModelsEntry, pKV_Map->GetName() );
	}

	// Get the list of counter-terrorist models.
	const char *ctModelsEntry = "ct_models";
	KeyValues *pKV_CTModels = pKV_Map->FindKey( ctModelsEntry );
	if ( pKV_CTModels )
	{
		for ( KeyValues *pKV_Model = pKV_CTModels->GetFirstValue(); pKV_Model; pKV_Model = pKV_Model->GetNextValue() )
		{
			pMap->m_CTModels.CopyAndAddToTail( pKV_Model->GetName() );
		}
	}
	else
	{
		Warning( "GameTypes: missing %s entry for map %s.\n", ctModelsEntry, pKV_Map->GetName() );
	}

	// Get names for the view model arms
	pMap->m_TViewModelArms.Set( pKV_Map->GetString( "t_arms" ) );
	pMap->m_CTViewModelArms.Set( pKV_Map->GetString( "ct_arms" ) );

	pMap->m_nDefaultGameType = pKV_Map->GetInt( "default_game_type" );
	pMap->m_nDefaultGameMode = pKV_Map->GetInt( "default_game_mode", 0 );

	// Get the list of hostage models if there is one.
	const char *hostageModelsEntry = "hostage_models";
	KeyValues *pKV_HostageModels = pKV_Map->FindKey( hostageModelsEntry );
	if ( pKV_HostageModels )
	{
		for ( KeyValues *pKV_Model = pKV_HostageModels->GetFirstValue(); pKV_Model; pKV_Model = pKV_Model->GetNextValue() )
		{
			pMap->m_HostageModels.CopyAndAddToTail( pKV_Model->GetName() );
		}
	}

	// Add the map to the list.
	pMap->m_Index = m_Maps.Count();
	m_Maps.AddToTail( pMap );

	return true;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Parse the given key values for the mapgroups.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::LoadMapGroups( KeyValues *pKV )
{
	Assert( pKV );
	if ( !pKV )
	{
		return false;
	}

	//Assert( m_MapGroups.Count() == 0 );
	if ( m_MapGroups.Count() > 0 )
	{
		//m_MapGroups.PurgeAndDeleteElements();
		FOR_EACH_VEC_BACK( m_MapGroups, i )
		{
			if ( !m_MapGroups[i]->m_bIsWorkshopMapGroup )
			{
				delete m_MapGroups[i];
				m_MapGroups.Remove( i );
			}
		}
	}

	// Get the mapgroups.
	const char *mapGroupsEntry = "mapGroups";
	KeyValues *pKV_MapGroups = pKV->FindKey( mapGroupsEntry );
	if ( !pKV_MapGroups )
	{
		Warning( "GameTypes: could not find entry %s.\n", mapGroupsEntry );
		return false;
	}

	// Parse the mapgroups.
	for ( KeyValues *pKV_MapGroup = pKV_MapGroups->GetFirstTrueSubKey(); pKV_MapGroup; pKV_MapGroup = pKV_MapGroup->GetNextTrueSubKey() )
	{
		MapGroup *pMapGroup = new MapGroup();

		// Set the name.
		V_strncpy( pMapGroup->m_Name, pKV_MapGroup->GetName(), sizeof( pMapGroup->m_Name ) );

		// Set the name ID.
		const char *nameIDEntry = "nameID";
		const char *pNameID = pKV_MapGroup->GetString( nameIDEntry );
		if ( pNameID )
		{
			V_strncpy( pMapGroup->m_NameID, pNameID, sizeof( pMapGroup->m_NameID ) );
		}
		else
		{
			Warning( "GameTypes: missing %s entry for map group %s.\n", nameIDEntry, pKV_MapGroup->GetName() );
		}

		// Set the image name.
		const char *imageNameEntry = "imagename";
		const char *pImageName = pKV_MapGroup->GetString( imageNameEntry );
		if ( pImageName )
		{
			V_strncpy( pMapGroup->m_ImageName, pImageName, sizeof( pMapGroup->m_ImageName ) );
		}
		else
		{
			Warning( "GameTypes: missing %s entry for map group %s.\n", imageNameEntry, pKV_MapGroup->GetName() );
		}

		// Get the maps.
		const char *mapsEntry = "maps";
		KeyValues *pKV_Maps = pKV_MapGroup->FindKey( mapsEntry );
		if ( pKV_Maps )
		{
			for ( KeyValues *pKV_Map = pKV_Maps->GetFirstValue(); pKV_Map; pKV_Map = pKV_Map->GetNextValue() )
			{
				pMapGroup->m_Maps.CopyAndAddToTail( pKV_Map->GetName() );
			}
		}
		else
		{
			Warning( "GameTypes: missing %s entry for map group %s.\n", mapsEntry, pKV_MapGroup->GetName() );
		}

		// Add the map group to the list.
		m_MapGroups.AddToTail( pMapGroup );
	}

	if ( m_MapGroups.Count() == 0 )
	{
		Warning( "GamesTypes: empty %s entry.\n", mapGroupsEntry );
	}

	return true;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Parse the given key values for the bot difficulties.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::LoadCustomBotDifficulties( KeyValues *pKV )
{
	Assert( pKV );
	if ( !pKV )
	{
		return false;
	}

	Assert( m_CustomBotDifficulties.Count() == 0 );
	if ( m_CustomBotDifficulties.Count() > 0 )
	{
		m_CustomBotDifficulties.PurgeAndDeleteElements();
	}

	// Get the bot difficulty levels.
	const char *botDifficultyEntry = "botDifficulty";
	KeyValues *pKV_BotDiffs = pKV->FindKey( botDifficultyEntry );
	if ( !pKV_BotDiffs )
	{
		Warning( "GameTypes: could not find entry %s.\n", botDifficultyEntry );
		return false;
	}

	// Parse the bot difficulty levels.
	for ( KeyValues *pKV_BotDiff = pKV_BotDiffs->GetFirstTrueSubKey(); pKV_BotDiff; pKV_BotDiff = pKV_BotDiff->GetNextTrueSubKey() )
	{
		CustomBotDifficulty *pBotDiff = new CustomBotDifficulty();

		// Set the name.
		V_strncpy( pBotDiff->m_Name, pKV_BotDiff->GetName(), sizeof( pBotDiff->m_Name ) );

		// Set the name ID.
		const char *nameIDEntry = "nameID";
		const char *pNameID = pKV_BotDiff->GetString( nameIDEntry );
		if ( pNameID )
		{
			V_strncpy( pBotDiff->m_NameID, pNameID, sizeof( pBotDiff->m_NameID ) );
		}
		else
		{
			Warning( "GameTypes: missing %s entry for bot difficulty %s.\n", nameIDEntry, pKV_BotDiff->GetName() );
		}

		// Get the convars.
		const char *convarsEntry = "convars";
		KeyValues *pKV_Convars = pKV_BotDiff->FindKey( convarsEntry );
		if ( pKV_Convars )
		{
			pBotDiff->m_pConvars = pKV_Convars->MakeCopy();
		}
		else
		{
			Warning( "GameTypes: missing entry %s for bot difficulty %s.\n", convarsEntry, pKV_BotDiff->GetName() );
		}

		// Check to see if this difficulty level has a bot quota convar.
		if ( pKV_Convars )
		{
			pBotDiff->m_HasBotQuota = ( pKV_Convars->GetInt( "bot_quota", g_invalidInteger ) != g_invalidInteger );
		}

		pBotDiff->m_Index = m_CustomBotDifficulties.Count();
		m_CustomBotDifficulties.AddToTail( pBotDiff );
	}

	if ( m_CustomBotDifficulties.Count() == 0 )
	{
		Warning( "GamesTypes: empty %s entry.\n", botDifficultyEntry );
	}

	return true;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the game type matching the given name.
// -------------------------------------------------------------------------------------------- //
GameTypes::GameType *GameTypes::GetGameType_Internal( const char *gameType )
{
	Assert( gameType );
	if ( gameType &&
		 gameType[0] != '\0' )
	{
		// Find the game type.
		FOR_EACH_VEC( m_GameTypes, iType )
		{
			GameType *pGameType = m_GameTypes[iType];
			Assert( pGameType );
			if ( pGameType &&
				 V_stricmp( pGameType->m_Name, gameType ) == 0 )
			{
				// Found it.
				return pGameType;
			}
		}
	}

	// Not found.
	Warning( "GameTypes: could not find matching game type \"%s\".\n", gameType );
	return NULL;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the game mode matching the given name.
// -------------------------------------------------------------------------------------------- //
GameTypes::GameMode *GameTypes::GetGameMode_Internal( GameType *pGameType, const char *gameMode )
{
	Assert( pGameType && gameMode );
	if ( pGameType && 
		 gameMode &&
		 gameMode[0] != '\0' )
	{
		// Find the game mode.
		FOR_EACH_VEC( pGameType->m_GameModes, iMode )
		{
			GameMode *pGameMode = pGameType->m_GameModes[iMode];
			Assert( pGameMode );
			if ( pGameMode &&
				 V_stricmp( pGameMode->m_Name, gameMode ) == 0 )
			{
				// Found it.
				return pGameMode;
			}
		}
	}

	// Not found.
	Warning( "GameTypes: could not find matching game mode \"%s\" for type \"%s\".\n", 
		gameMode, ( pGameType ? pGameType->m_Name : "null" ) );
	return NULL;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get the current game type matching the game_type convar.
// -------------------------------------------------------------------------------------------- //
GameTypes::GameType *GameTypes::GetCurrentGameType_Internal( void )
{
	if ( m_GameTypes.Count() == 0 )
	{
		Warning( "GamesTypes: no game types have been loaded.\n" );
		return NULL;
	}

	int gameType = game_type.GetInt();
	if ( gameType < 0 || gameType >= m_GameTypes.Count() )
	{
		Warning( "GamesTypes: game_type is set to an invalid value (%d). Range [%d,%d].\n", gameType, 0, m_GameTypes.Count() - 1 );
		return NULL;
	}

	return m_GameTypes[gameType];
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get the current game mode matching the game_mode convar.
// -------------------------------------------------------------------------------------------- //
GameTypes::GameMode *GameTypes::GetCurrentGameMode_Internal( GameType *pGameType )
{
	Assert( pGameType );
	if ( !pGameType )
	{
		return NULL;
	}

	int gameMode = game_mode.GetInt();
	if ( gameMode < 0 || gameMode >= pGameType->m_GameModes.Count() )
	{
		Warning( "GamesTypes: game_mode is set to an invalid value (%d). Range [%d,%d].\n", gameMode, 0, pGameType->m_GameModes.Count() - 1 );
		return NULL;
	}

	return pGameType->m_GameModes[gameMode];
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get the MapGroup from the mapgroup name
// -------------------------------------------------------------------------------------------- //
GameTypes::MapGroup *GameTypes::GetMapGroup_Internal( const char *mapGroup )
{
	Assert( mapGroup );
	if ( mapGroup &&
		 mapGroup[0] != '\0' )
	{

#if defined ( MATCHMAKING_DLL )
		// On the client and connected to a server, check the networked server values
		if ( g_pMatchExtensions->GetIVEngineClient() && g_pMatchExtensions->GetIVEngineClient()->IsConnected() )
		{
			if ( m_pServerMapGroup && V_stricmp( m_pServerMapGroup->m_Name, mapGroup ) == 0 )
			{
				return m_pServerMapGroup;
			}
		}
#endif
		// Find the game type in our local list
		char const *pchMapGroupToFind = mapGroup;
		if ( char const *pchComma = strchr( mapGroup, ',' ) )
		{
			// Use only the first mapgroup from the list for search
			char *pchCopy = ( char * ) stackalloc( pchComma - mapGroup + 1 );
			Q_strncpy( pchCopy, mapGroup, pchComma - mapGroup + 1 );
			pchCopy[ pchComma - mapGroup ] = 0;
			pchMapGroupToFind = pchCopy;
		}

		FOR_EACH_VEC( m_MapGroups, iMapGroup )
		{
			MapGroup *pMapGroup = m_MapGroups[iMapGroup];
			Assert( pMapGroup );
			if ( pMapGroup &&
				 V_stricmp( pMapGroup->m_Name, pchMapGroupToFind ) == 0 )
			{
				// Found it.
				return pMapGroup;
			}
		}

		// Not found in pre-built array, fake one for workshop
		if ( char const *pszWorkshopMapgroup = strstr( pchMapGroupToFind, "@workshop" ) )
		{
			char const *pszMapId = pszWorkshopMapgroup + 9;
			if ( *pszMapId ) ++ pszMapId;
			CFmtStr fmtGroupName( "%llu", Q_atoui64( pszMapId ) );
			char const *szIndividualMapName = pszWorkshopMapgroup + 1;
			
			CUtlStringList lstMapNames;
			lstMapNames.CopyAndAddToTail( szIndividualMapName );
			return CreateWorkshopMapGroupInternal( fmtGroupName, lstMapNames );
		}

		// Not found.
		//Warning( "GameTypes: could not find matching mapGroup \"%s\".\n", mapGroup );
	}

	return NULL;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get the map matching the given name.
// -------------------------------------------------------------------------------------------- //
GameTypes::Map *GameTypes::GetMap_Internal( const char *mapName )
{
	if ( m_Maps.Count() == 0 )
	{
		Warning( "GamesTypes: no maps have been loaded.\n" );
		return NULL;
	}

	Assert( mapName );
	if ( !mapName ||
		 mapName[0] == '\0' )
	{
		Warning( "GamesTypes: invalid map name.\n" );
		return NULL;
	}

	char mapNameNoExt[ MAX_MAP_NAME ];
	//V_strcpy_safe( mapNameNoExt, mapName );
	V_FileBase( mapName, mapNameNoExt, sizeof( mapNameNoExt ) );

	const int extLen = 4;

	// Remove the .360 extension from the map name.
	char *pExt = mapNameNoExt + V_strlen( mapNameNoExt ) - extLen;
	if ( pExt >= mapNameNoExt &&
		 V_strnicmp( pExt, ".360", extLen ) == 0 )
	{
		*pExt = '\0';
	}

	// Remove the .bsp extension from the map name.
	pExt = mapNameNoExt + V_strlen( mapNameNoExt ) - extLen;
	if ( pExt >= mapNameNoExt &&
		 V_strnicmp( pExt , ".bsp", extLen ) == 0 )
	{
		*pExt = '\0';
	}

#if defined ( MATCHMAKING_DLL )
	// On the client and connected to a server, check the networked server values
	if ( m_pServerMap && g_pMatchExtensions->GetIVEngineClient() && g_pMatchExtensions->GetIVEngineClient()->IsConnected() )
	{
		if( !V_stricmp( m_pServerMap->m_Name, mapNameNoExt ) || !V_stricmp( m_pServerMap->m_Name, mapName ) )
		{
			return m_pServerMap;
		}
	}
#endif

	// Find the map.
	FOR_EACH_VEC( m_Maps, iMap )
	{
		Map *pMap = m_Maps[iMap];
		Assert( pMap );
		if ( pMap &&
			 V_stricmp( pMap->m_Name, mapNameNoExt ) == 0 )
		{
			// Found it.
			return pMap;
		}
	}

	// Not found.
	// Squelching this-- community maps won't be found
	// Warning( "GameTypes: could not find matching map \"%s\".\n", mapNameNoExt );
	return NULL;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get the current bot difficulty based on the custom_bot_difficulty convar.
// -------------------------------------------------------------------------------------------- //
GameTypes::CustomBotDifficulty *GameTypes::GetCurrentCustomBotDifficulty_Internal( void )
{
	if ( m_CustomBotDifficulties.Count() == 0 )
	{
		Warning( "GamesTypes: no bot difficulties have been loaded.\n" );
		return NULL;
	}

	int botDiff = custom_bot_difficulty.GetInt();
	if ( botDiff < 0 || botDiff >= m_CustomBotDifficulties.Count() )
	{
		Warning( "GamesTypes: custom_bot_difficulty is set to an invalid value (%d). Range [%d,%d].\n", botDiff, 0, m_CustomBotDifficulties.Count() - 1 );
		return NULL;
	}

	return m_CustomBotDifficulties[botDiff];
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Set the game type and mode convars from the given strings.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::SetGameTypeAndMode( const char *gameType, const char *gameMode )
{	
	int iType = g_invalidInteger;
	int iMode = g_invalidInteger;
	if ( GetGameModeAndTypeIntsFromStrings( gameType, gameMode, iType, iMode ) )
	{
		// Set the game type.
		return SetGameTypeAndMode( iType, iMode );
	}
		
	Warning( "GamesTypes: unable to set game type and mode. Could not find type/mode matching type:%s/mode:%s.\n", gameType, gameMode );
	return false;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Set the game type and mode convars from passing and alias.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::GetGameTypeAndModeFromAlias( const char *modeAlias, int& iType, int& iMode )
{	
	iType = g_invalidInteger;
	iMode = g_invalidInteger;
	if ( Q_strcmp( "competitive", modeAlias ) == 0 || Q_strcmp( "comp", modeAlias ) == 0 )
	{
		iType = 0;
		iMode = 1;
	}
	else if ( Q_strcmp( "casual", modeAlias ) == 0 )
	{
		iType = 0;
		iMode = 0;
	}
	else if ( Q_strcmp( "armsrace", modeAlias ) == 0 || Q_strcmp( "arms", modeAlias ) == 0 || Q_strcmp( "gungame", modeAlias ) == 0 || Q_strcmp( "gg", modeAlias ) == 0 )
	{
		iType = 1;
		iMode = 0;
	}
	else if ( Q_strcmp( "demolition", modeAlias ) == 0 || Q_strcmp( "demo", modeAlias ) == 0 )
	{
		iType = 1;
		iMode = 1;
	}
	else if ( Q_strcmp( "deathmatch", modeAlias ) == 0 || Q_strcmp( "dm", modeAlias ) == 0 )
	{
		iType = 1;
		iMode = 2;
	}
	else if ( Q_strcmp( "training", modeAlias ) == 0 )
	{
		iType = 2;
		iMode = 0;
	}
	else if ( Q_strcmp( "custom", modeAlias ) == 0 )
	{
		iType = 3;
		iMode = 0;
	}
	else if ( Q_strcmp( "guardian", modeAlias ) == 0 || Q_strcmp( "guard", modeAlias ) == 0 || Q_strcmp( "cooperative", modeAlias ) == 0 )
	{
		iType = 4;
		iMode = 0;
	}
	else if ( Q_strcmp( "default", modeAlias ) == 0 || Q_strcmp( "auto", modeAlias ) == 0 )
	{
		SetRunMapWithDefaultGametype( true );
		return false;
	}

	// return if we matched an alias
	return ( iType != g_invalidInteger && iMode != g_invalidInteger );
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Set the game type and mode convars.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::SetGameTypeAndMode( int nGameType, int nGameMode )
{	
	// if we've launched the map through the menu, we are specifically saying that we should use these settings
	// otherwise, use the game type/mode that the made defines as the default

	if ( nGameType < CS_GameType_Min || nGameType > CS_GameType_Max )
	{
		Warning( "GamesTypes: unable to set game type and mode. Game type value is outside valid range.  (value == %d)\n", nGameType );
		return false;
	}

	// Set the game type.
	DevMsg( "GameTypes: setting game type to %d.\n", nGameType );
	game_type.SetValue( nGameType );

	// Set the game mode.
	DevMsg( "GameTypes: setting game mode to %d.\n", nGameMode );
	game_mode.SetValue( nGameMode );

	return true;
}

void GameTypes::SetAndParseExtendedServerInfo( KeyValues *pExtendedServerInfo )
{
	if ( m_pExtendedServerInfo )
		m_pExtendedServerInfo->deleteThis();

	ClearServerMapGroupInfo();

	m_pExtendedServerInfo = pExtendedServerInfo ? pExtendedServerInfo->MakeCopy() : NULL;

	// BUGBUG: Not networking the complete state of a map/mapgroup struct so these have default values
	// which may not match the server... Would like to avoid a ton of network traffic if it's not needed, so 
	// will only clean this up if needed.
	if ( m_pExtendedServerInfo )
	{
		//const char* szMapNameBase = V_GetFileName( m_pServerMap->m_Name );

		m_iCurrentServerNumSlots = m_pExtendedServerInfo->GetInt( "numSlots", 0 );
		m_pServerMap = new Map;
		V_strncpy( m_pServerMap->m_Name, m_pExtendedServerInfo->GetString( "map", "" ), ARRAYSIZE(m_pServerMap->m_Name) );
		V_FixSlashes( m_pServerMap->m_Name, '/' );
		m_pServerMap->m_TViewModelArms.Set( m_pExtendedServerInfo->GetString( "t_arms", "" ) );
		m_pServerMap->m_CTViewModelArms.Set( m_pExtendedServerInfo->GetString( "ct_arms", "" ) );
		m_pServerMap->m_nDefaultGameType = m_pExtendedServerInfo->GetInt( "default_game_type" );
		m_pServerMap->m_nDefaultGameMode = m_pExtendedServerInfo->GetInt( "default_game_mode", 0 );
		KeyValues *pCTModels = m_pExtendedServerInfo->FindKey( "ct_models", false );
		if ( pCTModels )
		{
			for ( KeyValues *pKV = pCTModels->GetFirstValue(); pKV; pKV = pKV->GetNextValue() )
			{
				m_pServerMap->m_CTModels.CopyAndAddToTail( pKV->GetString() );
			}
		}
		KeyValues *pTModels = m_pExtendedServerInfo->FindKey( "t_models", false );
		if ( pTModels )
		{
			for ( KeyValues *pKV = pTModels->GetFirstValue(); pKV; pKV = pKV->GetNextValue() )
			{
				m_pServerMap->m_TModels.CopyAndAddToTail( pKV->GetString() );
			}
		}
		
		if ( m_pExtendedServerInfo->GetBool( "official" ) && !m_pExtendedServerInfo->GetBool( "gotv" ) )
		{
			V_strcpy_safe( m_pServerMap->m_RequiresAttr, m_pExtendedServerInfo->GetString( "requires_attr" ) );
			m_pServerMap->m_RequiresAttrValue = m_pExtendedServerInfo->GetInt( "requires_attr_value" );
			V_strcpy_safe( m_pServerMap->m_RequiresAttrReward, m_pExtendedServerInfo->GetString( "requires_attr_reward" ) );
			m_pServerMap->m_nRewardDropList = m_pExtendedServerInfo->GetInt( "reward_drop_list" );
		}

		m_pServerMapGroup = new MapGroup;
		V_strncpy( m_pServerMapGroup->m_Name, m_pExtendedServerInfo->GetString( "mapgroup", "" ), ARRAYSIZE(m_pServerMapGroup->m_Name) );
		KeyValues *pMapsInGroup = m_pExtendedServerInfo->FindKey( "maplist", false );
		if ( pMapsInGroup )
		{
			for ( KeyValues *pKV = pMapsInGroup->GetFirstValue(); pKV; pKV = pKV->GetNextValue() )
			{
				m_pServerMapGroup->m_Maps.CopyAndAddToTail( pKV->GetString() );
			}
		}

#if defined( MATCHMAKING_DLL ) || defined( MATCHMAKING_DS_DLL )
		if ( g_pMatchFramework && g_pMatchFramework->GetEventsSubscription() )
		{
			//char fileBase[MAX_MAP_NAME];
			//V_FileBase( m_pServerMap->m_Name, fileBase, sizeof ( fileBase ) );
			// Set game type and game mode convars appropriately:
			if ( m_pExtendedServerInfo->FindKey( "c_game_type" ) && m_pExtendedServerInfo->FindKey( "c_game_mode" ) )
			{
				SetGameTypeAndMode( m_pExtendedServerInfo->GetInt( "c_game_type" ), m_pExtendedServerInfo->GetInt( "c_game_mode" ) );
			}

			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
				"OnLevelLoadingSetDefaultGameModeAndType", "mapname", m_pServerMap->m_Name
				) );
		}
#endif
	}
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the index of the current game type.
// -------------------------------------------------------------------------------------------- //
void GameTypes::CheckShouldSetDefaultGameModeAndType( const char* szMapNameFull )
{
	// this is only called from CSGameRules()

	int iType = 0;
	int iMode = 0;
	bool bShouldSet = false;

	// check we don't have a launch option that defines what game type/mode we should be playing with
	KeyValues *mode = NULL;
#if defined( CLIENT_DLL )
	return;
#elif defined( GAME_DLL )
	mode = engine->GetLaunchOptions();
#elif defined( MATCHMAKING_DLL )
	if ( IVEngineServer *pIVEngineServer = ( IVEngineServer * ) g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( INTERFACEVERSION_VENGINESERVER ) )
	{
		mode = pIVEngineServer->GetLaunchOptions();
	}
#elif defined( MATCHMAKING_DS_DLL )
	if ( IVEngineServer *pIVEngineServer = ( IVEngineServer * ) g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( INTERFACEVERSION_VENGINESERVER ) )
	{
		mode = pIVEngineServer->GetLaunchOptions();
	}
#endif

	bool bCommandLineAlias = false;
	if ( mode )
	{
		// start at the third value
		KeyValues *kv = mode->GetFirstSubKey()->GetNextKey();
		//KeyValuesDumpAsDevMsg( mode );
		for ( KeyValues *arg = kv->GetNextKey(); arg != NULL; arg = arg->GetNextKey() )
		{
			// if "default" gets passed, we set should run with default to true inside this function and return false
			if ( GetGameTypeAndModeFromAlias( arg->GetString(), iType, iMode ) )
			{
				// don't use the default map game mode
				bCommandLineAlias = true;
				bShouldSet = true;
			}
		}
	}

	const char* szMapNameBase = V_GetFileName( szMapNameFull );

	if ( !bCommandLineAlias && GetRunMapWithDefaultGametype() )
	{
		// the default game type hasn't been loaded yet so load it now (pulling it straight from the bsp) if it exists
		if ( GetDefaultGameTypeForMap( szMapNameFull ) == -1 )
		{
			//V_FixSlashes( szMapNameBase, '/' );
			char kvFilename[ MAX_PATH ];
			V_snprintf( kvFilename, sizeof( kvFilename ), "maps/%s.kv", szMapNameBase );
			if ( g_pFullFileSystem->FileExists( kvFilename ) )
			{
				KeyValues *pKV = new KeyValues( "convars" );
				if ( pKV->LoadFromFile( g_pFullFileSystem, kvFilename ) )
				{
					KeyValuesDumpAsDevMsg( pKV, 1 );
					LoadMapEntry( pKV );
				}
			}
		}

		iType = GetDefaultGameTypeForMap( szMapNameFull );
		iMode = GetDefaultGameModeForMap( szMapNameFull );

		bShouldSet = true;
	}

	// a mode has not been set before this function was called, so set it here
	if ( bShouldSet )
	{
		SetGameTypeAndMode( iType, iMode );
	}

	// this is the end of the line for loading game types
	// once we have all of the data, if you just type "map mapname" in the console, it'll run with the map's default mode
	SetRunMapWithDefaultGametype( false );

#if defined( MATCHMAKING_DLL ) || defined( MATCHMAKING_DS_DLL )
	if ( g_pMatchFramework && g_pMatchFramework->GetEventsSubscription() )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnLevelLoadingSetDefaultGameModeAndType", "mapname", szMapNameFull
			) );
	}
#endif

#ifndef CLIENT_DLL
	// now force the loading screen to show the correct ghing
// 	char const *pGameType = GetGameTypeFromInt( iType );
// 	char const *pGameMode = GetGameModeFromInt( iMode, iType );
// 	PopulateLevelInfo( szMapNameBase, pGameType, pGameMode );
// 	BaseModUI::CBaseModPanel *pBaseModPanel = BaseModUI::CBaseModPanel::GetSingletonPtr();
// 	if ( pBaseModPanel && pBaseModPanel->IsVisible() )
// 	{
// 		pBaseModPanel->CreateAndLoadDialogForKeyValues( szMapNameBase );
// 	}
#endif
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the index of the current game type.
// -------------------------------------------------------------------------------------------- //
int GameTypes::GetCurrentGameType() const
{
	return game_type.GetInt();
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the index of the current game mode.
// -------------------------------------------------------------------------------------------- //
int GameTypes::GetCurrentGameMode() const
{
	return game_mode.GetInt();
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get the current game type UI string.
// -------------------------------------------------------------------------------------------- //
const char *GameTypes::GetCurrentGameTypeNameID( void )
{
	GameType *pGameType = GetCurrentGameType_Internal();
	Assert( pGameType );
	if ( !pGameType )
	{
		return NULL;
	}

	return pGameType->m_NameID;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get the current game mode UI string.
// -------------------------------------------------------------------------------------------- //
const char *GameTypes::GetCurrentGameModeNameID( void )
{
	GameType *pGameType = GetCurrentGameType_Internal();
	Assert( pGameType );
	if ( !pGameType )
	{
		return NULL;
	}

	GameMode *pGameMode = GetCurrentGameMode_Internal( pGameType );
	Assert( pGameMode );
	if ( !pGameMode )
	{
		return NULL;
	}

	if ( pGameMode->m_NameID_SP[0] == '\0' )
		return pGameMode->m_NameID_SP;

	return pGameMode->m_NameID;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Apply the game mode convars for the given type and mode.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::ApplyConvarsForCurrentMode( bool isMultiplayer )
{
	GameType *pGameType = GetCurrentGameType_Internal();
	Assert( pGameType );
	if ( !pGameType || pGameType->m_Index == CS_GameType_Custom )
	{
		return false;
	}

	GameMode *pGameMode = GetCurrentGameMode_Internal( pGameType );
	Assert( pGameMode );
	if ( !pGameMode )
	{
		return false;
	}

	// Get the convars.
	// If there are no multiplayer convars, fall back to single player.
	KeyValues *pKV_Convars = pGameMode->m_pExecConfings;

	// Validate the convars.
	if ( !pKV_Convars )
	{
		Warning( "GamesTypes: unable to set convars. There are no convars for game type/mode (%s:%d/%s:%d).\n", 
			pGameType->m_Name, game_type.GetInt(), pGameMode->m_Name, game_mode.GetInt() );
		return false;
	}

	// Apply the convars for this mode.
	for ( KeyValues *pKV_Convar = pKV_Convars->GetFirstValue(); pKV_Convar; pKV_Convar = pKV_Convar->GetNextValue() )
	{
		if ( !Q_stricmp( "exec", pKV_Convar->GetName() ) )
		{
			CFmtStr sExecCmd( "exec \"%s\"\n", pKV_Convar->GetString() );
			//
			// NOTE: This is horrible design to have this file included in multiple projects (client.dll, server.dll, matchmaking.dll)
			// which don't have same interfaces applicable to interact with the engine and don't have sufficient context here
			// hence these compile-time conditionals to use appropriate interfaces
			// ---
			// to be fair, only server.dll calls this method, just the horrible design is that this method is compiled in a bunch
			// of dlls that don't need it...
			//
#if defined( CLIENT_DLL )
			engine->ExecuteClientCmd( sExecCmd );
#elif defined( GAME_DLL )
			engine->ServerCommand( sExecCmd );
			engine->ServerExecute();
#elif defined( MATCHMAKING_DLL )
			if ( IVEngineClient *pIVEngineClient = ( IVEngineClient * ) g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( VENGINE_CLIENT_INTERFACE_VERSION ) )
			{
				pIVEngineClient->ExecuteClientCmd( sExecCmd );
			}
			else if ( IVEngineServer *pIVEngineServer = ( IVEngineServer * ) g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( INTERFACEVERSION_VENGINESERVER ) )
			{
				pIVEngineServer->ServerCommand( sExecCmd );
				pIVEngineServer->ServerExecute();
			}
#elif defined( MATCHMAKING_DS_DLL )
			if ( IVEngineServer *pIVEngineServer = ( IVEngineServer * ) g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( INTERFACEVERSION_VENGINESERVER ) )
			{
				pIVEngineServer->ServerCommand( sExecCmd );
				pIVEngineServer->ServerExecute();
			}
#else
#error "gametypes.cpp included in an unexpected project"
#endif
		}
	}

	DevMsg( "GameTypes: set convars for game type/mode (%s:%d/%s:%d):\n",
		pGameType->m_Name, game_type.GetInt(), pGameMode->m_Name, game_mode.GetInt() );
	KeyValuesDumpAsDevMsg( pKV_Convars, 1 );

	// If this is offline, then set the bot difficulty convars.
	if ( !isMultiplayer )
	{
		CustomBotDifficulty *pBotDiff = GetCurrentCustomBotDifficulty_Internal();
		Assert( pBotDiff );
		if ( pBotDiff )
		{
			KeyValues *pKV_ConvarsBotDiff = pBotDiff->m_pConvars;
			if ( pKV_ConvarsBotDiff )
			{
				// Apply the convars for the bot difficulty.
				for ( KeyValues *pKV_Convar = pKV_ConvarsBotDiff->GetFirstValue(); pKV_Convar; pKV_Convar = pKV_Convar->GetNextValue() )
				{
					// Only allow a certain set of convars to control bot difficulty
					char const *arrBotConvars[] = { "bot_difficulty", "bot_dont_shoot", "bot_quota" };
					bool bBotConvar = false;
					for ( int jj = 0; jj < Q_ARRAYSIZE( arrBotConvars ); ++ jj )
					{
						if ( !Q_stricmp( pKV_Convar->GetName(), arrBotConvars[jj] ) )
						{
							bBotConvar = true;
							break;
						}
					}
					if ( !bBotConvar )
					{
						Warning( "GamesTypes: invalid bot difficulty convar [%s] for bot difficulty (%s:%d).\n",
							pKV_Convar->GetName(),
							pBotDiff->m_Name, custom_bot_difficulty.GetInt() );
						continue;
					}

					ConVarRef conVarRef( pKV_Convar->GetName() );
					conVarRef.SetValue( pKV_Convar->GetString() );
				}

				DevMsg( "GameTypes: set convars for bot difficulty (%s:%d):\n", pBotDiff->m_Name, custom_bot_difficulty.GetInt() );
				KeyValuesDumpAsDevMsg( pKV_ConvarsBotDiff, 1 );
			}
			else
			{
				Warning( "GamesTypes: unable to set bot difficulty convars. There are no convars for bot difficulty (%s:%d).\n", 
					pBotDiff->m_Name, custom_bot_difficulty.GetInt() );
			}
		}
	}

	return true;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Output the values of the convars for the current game mode.
// -------------------------------------------------------------------------------------------- //
void GameTypes::DisplayConvarsForCurrentMode( void )
{
	GameType *pGameType = GetCurrentGameType_Internal();
	Assert( pGameType );
	if ( !pGameType )
	{
		return;
	}

	GameMode *pGameMode = GetCurrentGameMode_Internal( pGameType );
	Assert( pGameMode );
	if ( !pGameMode )
	{
		return;
	}

	// Display the configs
	KeyValuesDumpAsDevMsg( pGameMode->m_pExecConfings, 0, 0 );

	// Display the offline bot difficulty convars.
	CustomBotDifficulty *pBotDiff = GetCurrentCustomBotDifficulty_Internal();
	if ( pBotDiff )
	{
		KeyValues *pKV_ConvarsBotDiff = pBotDiff->m_pConvars;
		if ( pKV_ConvarsBotDiff )
		{
			Msg( "GameTypes: dumping convars for bot difficulty (%s:%d):", 
				pBotDiff->m_Name, custom_bot_difficulty.GetInt() );
			KeyValuesDumpAsDevMsg( pKV_ConvarsBotDiff, 0, 0 );
		}
	}
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the CT weapon progression for the current game type and mode.
// -------------------------------------------------------------------------------------------- //
const CUtlVector< IGameTypes::WeaponProgression > *GameTypes::GetWeaponProgressionForCurrentModeCT( void )
{
	GameType *pGameType = GetCurrentGameType_Internal();
	Assert( pGameType );
	if ( !pGameType )
	{
		return NULL;
	}

	GameMode *pGameMode = GetCurrentGameMode_Internal( pGameType );
	Assert( pGameMode );
	if ( !pGameMode )
	{
		return NULL;
	}

	return &(pGameMode->m_WeaponProgressionCT);
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the T weapon progression for the current game type and mode.
// -------------------------------------------------------------------------------------------- //
const CUtlVector< IGameTypes::WeaponProgression > *GameTypes::GetWeaponProgressionForCurrentModeT( void )
{
	GameType *pGameType = GetCurrentGameType_Internal();
	Assert( pGameType );
	if ( !pGameType )
	{
		return NULL;
	}

	GameMode *pGameMode = GetCurrentGameMode_Internal( pGameType );
	Assert( pGameMode );
	if ( !pGameMode )
	{
		return NULL;
	}

	return &(pGameMode->m_WeaponProgressionT);
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get a random mapgroup for the given mode and type.
// -------------------------------------------------------------------------------------------- //
const char *GameTypes::GetRandomMapGroup( const char *gameType, const char *gameMode )
{
	Assert( gameType && gameMode );
	if ( !gameType || !gameMode )
	{
		return NULL;
	}

	GameType *pGameType = GetGameType_Internal( gameType );
	Assert( pGameType );
	if ( !pGameType )
	{
		return NULL;
	}

	GameMode *pGameMode = GetGameMode_Internal( pGameType, gameMode );
	Assert( pGameMode );
	if ( !pGameMode )
	{
		return NULL;
	}

	if ( pGameMode->m_MapGroupsMP.Count() == 0 )
	{
		return NULL;
	}

	// Randomly choose a mapgroup from our map list.
	int iRandom = m_randomStream.RandomInt( 0, pGameMode->m_MapGroupsMP.Count() - 1 );
	return pGameMode->m_MapGroupsMP[iRandom];
}


// -------------------------------------------------------------------------------------------- //
// Purpose: Get the first map from the mapgroup
// -------------------------------------------------------------------------------------------- //
const char *GameTypes::GetFirstMap( const char *mapGroup )
{
	Assert( mapGroup );
	if ( !mapGroup )
	{
		return NULL;
	}

	MapGroup *pMapGroup = GetMapGroup_Internal( mapGroup );
	Assert( pMapGroup );
	if ( !pMapGroup )
	{
		return NULL;
	}

	if ( pMapGroup->m_Maps.Count() == 0 )
	{
		return NULL;
	}

	return pMapGroup->m_Maps[0];
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get a random map from the mapgroup
// -------------------------------------------------------------------------------------------- //
const char *GameTypes::GetRandomMap( const char *mapGroup )
{
	Assert( mapGroup );
	if ( !mapGroup )
	{
		return NULL;
	}

	MapGroup *pMapGroup = GetMapGroup_Internal( mapGroup );
	Assert( pMapGroup );
	if ( !pMapGroup )
	{
		return NULL;
	}

	if ( pMapGroup->m_Maps.Count() == 0 )
	{
		return NULL;
	}

	// Randomly choose a map from our map list.
	int iRandom = m_randomStream.RandomInt( 0, pMapGroup->m_Maps.Count() - 1 );
	return pMapGroup->m_Maps[iRandom];
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get the next map from the mapgroup; wrap around to beginning from the end of list
// -------------------------------------------------------------------------------------------- //
const char *GameTypes::GetNextMap( const char *mapGroup, const char *mapName )
{
	Msg( "Looking for next map in mapgroup '%s'...\n", mapGroup );
	Assert( mapGroup );
	if ( !mapGroup )
	{
		return NULL;
	}

	MapGroup *pMapGroup = GetMapGroup_Internal( mapGroup );
	Assert( pMapGroup );
	if ( !pMapGroup )
	{
		return NULL;
	}

	if ( pMapGroup->m_Maps.Count() == 0 )
	{
		return NULL;
	}

	int mapIndex = 0;
	for ( ; mapIndex < pMapGroup->m_Maps.Count(); ++mapIndex )
	{
		char szInputName[MAX_PATH];
		V_strcpy_safe( szInputName, mapName );
		V_FixSlashes( szInputName, '/' );
		if ( mapName && !V_stricmp( mapName, pMapGroup->m_Maps[mapIndex] ) )
		{
			break;
		}
	}

	// get the next map in the list
	mapIndex++;
	if ( mapIndex >= pMapGroup->m_Maps.Count() )
	{	// wrap to the beginning of the list, use first map if the passed map wasn't in the mapgroup
		mapIndex = 0;
	}

	return pMapGroup->m_Maps[mapIndex];
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get the maxplayers value for the type and mode 
// NOTE: This is from the local KV file... This will not match the server you're connected to
// remotely if they've changed it from the default!
// -------------------------------------------------------------------------------------------- //
int GameTypes::GetMaxPlayersForTypeAndMode( int iType, int iMode )
{
	GameType *pGameType;
	GameMode *pGameMode;

	const char* szGameType = GetGameTypeFromInt( iType );
	const char* szGameMode = GetGameModeFromInt( iType, iMode );

	GetGameModeAndTypeFromStrings( szGameType, szGameMode, pGameType, pGameMode );

	if ( !pGameMode )
	{
		return 1;
	}

	return pGameMode->m_MaxPlayers;
}
// -------------------------------------------------------------------------------------------- //
// Purpose: Is this a valid mapgroup name 
// -------------------------------------------------------------------------------------------- //
bool GameTypes::IsValidMapGroupName( const char * mapGroup )
{
	if ( !mapGroup || mapGroup[0] == '\0' )
	{
		return false;
	}

	MapGroup *pMapGroup = GetMapGroup_Internal( mapGroup );
	if ( !pMapGroup )
	{
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Does this mapname exist within this mapgroup
// -------------------------------------------------------------------------------------------- //
bool GameTypes::IsValidMapInMapGroup( const char * mapGroup, const char *mapName )
{
	if ( !IsValidMapGroupName( mapGroup ) )
	{
		return false;
	}

	if ( !mapName )
	{
		return false;
	}

	MapGroup *pMapGroup = GetMapGroup_Internal( mapGroup );
	if ( !pMapGroup )
	{
		return false;
	}

	char fileBase[MAX_MAP_NAME];
	V_FileBase( mapName, fileBase, sizeof ( fileBase ) );
	for ( int mapIndex = 0 ; mapIndex < pMapGroup->m_Maps.Count(); ++mapIndex )
	{
		if ( !V_stricmp( fileBase, pMapGroup->m_Maps[mapIndex] ) ||
			 !V_stricmp( mapName, pMapGroup->m_Maps[mapIndex] ) ) 
		{
			return true;
		}
	}

	return false;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Is this mapgroup part of this type and mode
// -------------------------------------------------------------------------------------------- //
bool GameTypes::IsValidMapGroupForTypeAndMode( const char * mapGroup, const char *gameType, const char *gameMode )
{
	if ( !IsValidMapGroupName( mapGroup ) )
	{
		return false;
	}

	if ( !gameType || !gameMode )
	{
		return false;
	}

	GameType *pGameType = GetGameType_Internal( gameType );
	if ( !pGameType )
	{
		return false;
	}

	GameMode *pGameMode = GetGameMode_Internal( pGameType, gameMode );
	if ( !pGameMode )
	{
		return false;
	}

	for ( int i = 0 ; i < pGameMode->m_MapGroupsMP.Count(); ++i )
	{
		if ( V_strcmp( mapGroup, pGameMode->m_MapGroupsMP[i] ) == 0 )
		{
			return true;
		}
	}

	for ( int i = 0 ; i < pGameMode->m_MapGroupsSP.Count(); ++i )
	{
		if ( V_strcmp( mapGroup, pGameMode->m_MapGroupsSP[i] ) == 0 )
		{
			return true;
		}
	}

	return false;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Apply the convars for the given map.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::ApplyConvarsForMap( const char *mapName, bool isMultiplayer )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( pMap )
	{
		// Determine if we need to set the bot quota.
		bool setBotQuota = true;

		if ( !isMultiplayer )
		{
			CustomBotDifficulty *pBotDiff = GetCurrentCustomBotDifficulty_Internal();
			Assert( pBotDiff );
			if ( pBotDiff )
			{
				setBotQuota = !pBotDiff->m_HasBotQuota;
			}
		}

		return true;
	}


	// Warning( "GamesTypes: unable to set convars for map %s. Could not find matching map name.\n", mapName );
	return false;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Get specifics about a map.
// -------------------------------------------------------------------------------------------- //
bool GameTypes::GetMapInfo( const char *mapName, uint32 &richPresence )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return false;
	}

	richPresence = pMap->m_RichPresence;

	return true;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the available terrorist character model names for the given map.
// -------------------------------------------------------------------------------------------- //
const CUtlStringList *GameTypes::GetTModelsForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return NULL;
	}

	return &pMap->m_TModels;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the available counter-terrorist character model names for the given map.
// -------------------------------------------------------------------------------------------- //
const CUtlStringList *GameTypes::GetCTModelsForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return NULL;
	}

	return &pMap->m_CTModels;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the available terrorist view model arms name for the given map.
// -------------------------------------------------------------------------------------------- //
const char *GameTypes::GetTViewModelArmsForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return NULL;
	}

	return pMap->m_TViewModelArms.String();
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the available counter-terrorist view model arms name for the given map.
// -------------------------------------------------------------------------------------------- //
const char *GameTypes::GetCTViewModelArmsForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return NULL;
	}

	return pMap->m_CTViewModelArms.String();
}

// Item requirements for the map
const char *GameTypes::GetRequiredAttrForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return NULL;
	}

	return pMap->m_RequiresAttr;
}

int GameTypes::GetRequiredAttrValueForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return -1;
	}

	return pMap->m_RequiresAttrValue;
}

const char *GameTypes::GetRequiredAttrRewardForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return NULL;
	}

	return pMap->m_RequiresAttrReward;
}

int GameTypes::GetRewardDropListForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return -1;
	}

	return pMap->m_nRewardDropList;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the available terrorist character model names for the given map.
// -------------------------------------------------------------------------------------------- //
const CUtlStringList *GameTypes::GetHostageModelsForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return NULL;
	}

	return &pMap->m_HostageModels;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the default game type defined for the map
// -------------------------------------------------------------------------------------------- //
const int GameTypes::GetDefaultGameTypeForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return -1;
	}

	return pMap->m_nDefaultGameType;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the default game mode defined for the map
// -------------------------------------------------------------------------------------------- //
const int GameTypes::GetDefaultGameModeForMap( const char *mapName )
{
	Map *pMap = GetMap_Internal( mapName );
	if ( !pMap )
	{
		return -1;
	}

	return pMap->m_nDefaultGameMode;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the maps for the given map group name.
// -------------------------------------------------------------------------------------------- //
const CUtlStringList *GameTypes::GetMapGroupMapList( const char *mapGroup )
{
	Assert( mapGroup );
	if ( !mapGroup || StringIsEmpty( mapGroup ) )
	{
		return NULL;
	}

	MapGroup *pMapGroup = GetMapGroup_Internal( mapGroup );
	Assert( pMapGroup );
	if ( !pMapGroup )
	{
		return NULL;
	}

	return &pMapGroup->m_Maps;
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Returns the bot difficulty for Offline Games (where players choose difficulty).
// -------------------------------------------------------------------------------------------- //
int GameTypes::GetCustomBotDifficulty( void )
{
	return custom_bot_difficulty.GetInt();
}

// -------------------------------------------------------------------------------------------- //
// Purpose: Sets the bot difficulty for Offline Games (where players choose difficulty).
// -------------------------------------------------------------------------------------------- //
bool GameTypes::SetCustomBotDifficulty( int botDiff )
{
	if ( botDiff < 0 ||
		 botDiff >= m_CustomBotDifficulties.Count() )
	{
		Warning( "GameTypes: invalid custom bot difficulty (%d). Range [%d,%d].\n", botDiff, 0, m_CustomBotDifficulties.Count() - 1 );
		return false;
	}

	DevMsg( "GameTypes: setting custom_bot_difficulty to %d.\n", botDiff );
	custom_bot_difficulty.SetValue( botDiff );

	return true;
}

const char* GameTypes::GetGameTypeFromInt( int gameType )
{
	// Find the game type.
	FOR_EACH_VEC( m_GameTypes, iType )
	{
		GameType *pGameType = m_GameTypes[iType];
		Assert( pGameType );
		if ( pGameType && pGameType->m_Index == gameType )
		{
			// Found it.
			return pGameType->m_Name;
		}
	}

	// Not found.
	DevWarning( "GameTypes: could not find matching game type for value \"%d\".\n", gameType );
	return NULL;
}

const char* GameTypes::GetGameModeFromInt( int gameType, int gameMode )
{
	// Find the game type.
	FOR_EACH_VEC( m_GameTypes, iType )
	{
		GameType *pGameType = m_GameTypes[iType];
		Assert( pGameType );
		if ( pGameType && pGameType->m_Index == gameType )
		{
			// Find the game mode.
			FOR_EACH_VEC( pGameType->m_GameModes, iMode )
			{
				GameMode *pGameMode = pGameType->m_GameModes[iMode];
				Assert( pGameMode );
				if ( pGameMode && pGameMode->m_Index == gameMode )
				{
					// Found it.
					return pGameMode->m_Name;
				}
			}
		}
	}

	// Not found.
	DevWarning( "GameTypes: could not find matching game mode value of \"%d\" and type value of \"%d\".\n", 
		gameType, gameMode );
	return NULL;
}

bool GameTypes::GetGameTypeFromMode( const char *szGameMode, const char *&pszGameTypeOut )
{
	FOR_EACH_VEC( m_GameTypes, iType )
	{
		GameType *pGameType = m_GameTypes[ iType ];
		Assert( pGameType );
		if ( pGameType )
		{
			// Find the game mode.
			FOR_EACH_VEC( pGameType->m_GameModes, iMode )
			{
				GameMode *pGameMode = pGameType->m_GameModes[ iMode ];
				Assert( pGameMode );
				if ( pGameMode && !V_strcmp( pGameMode->m_Name, szGameMode ) )
				{
					// Found it.
					pszGameTypeOut = pGameType->m_Name;
					return true;
				}
			}
		}
	}

	// Not found.
	DevWarning( "GameTypes: could not find matching game mode value of \"%s\" in any game type.\n",
		szGameMode );
	return false;

}

bool GameTypes::GetGameModeAndTypeIntsFromStrings( const char* szGameType, const char* szGameMode, int& iOutGameType, int& iOutGameMode )
{
	GameType* type = NULL;
	GameMode* mode = NULL;     
	iOutGameType = g_invalidInteger;
	iOutGameMode = g_invalidInteger;
	
	if ( V_stricmp( szGameType, "default" ) == 0 )
	{
		return false;
	}

	if ( GetGameModeAndTypeFromStrings( szGameType, szGameMode, type, mode ) )
	{
		if ( mode && type )
		{
			Assert( type->m_Index >= 0 && type->m_Index < m_GameTypes.Count() );
			Assert( mode->m_Index >= 0 && mode->m_Index < type->m_GameModes.Count() );

			if ( type->m_Index >= 0 && type->m_Index < m_GameTypes.Count() &&
				 mode->m_Index >= 0 && mode->m_Index < type->m_GameModes.Count() )						
			{
				iOutGameType = type->m_Index;
				iOutGameMode = mode->m_Index;
				return true;				
			}			
		}
	}
	return false;	
}

bool GameTypes::GetGameModeAndTypeNameIdsFromStrings( const char* szGameType, const char* szGameMode, const char*& szOutGameTypeNameId, const char*& szOutGameModeNameId )
{
	GameType* type = NULL;
	GameMode* mode = NULL;     
	szOutGameTypeNameId = NULL;
	szOutGameModeNameId = NULL;
		
	if ( GetGameModeAndTypeFromStrings( szGameType, szGameMode, type, mode ) )
	{
		Assert ( mode && mode->m_NameID );
		Assert ( type && type->m_NameID );

		if ( mode && mode->m_NameID && type && type->m_NameID )
		{
			szOutGameTypeNameId = type->m_NameID;

			if ( mode->m_NameID_SP[0] != '\0' )
				szOutGameModeNameId = mode->m_NameID_SP;
			else
				szOutGameModeNameId = mode->m_NameID;
			
			return true;
		}
	}
	return false;
}


bool GameTypes::GetGameModeAndTypeFromStrings( const char* szGameType, const char* szGameMode, GameType*& outGameType, GameMode*& outGameMode )
{      
	outGameType = NULL;
	outGameMode = NULL;
	Assert( szGameType && szGameMode );
	if ( !szGameType || !szGameMode )
	{
		return false;
	}

	// we want to use the map's default settings, so don't set the game mode here, we'll do it later
	if ( V_stricmp( szGameType, "default" ) == 0 )
	{
		return false;
	}

	outGameType = GetGameType_Internal( szGameType );
	Assert( outGameType );
	if ( outGameType )
	{
		outGameMode = GetGameMode_Internal( outGameType, szGameMode );
		Assert( outGameMode );
		if ( outGameMode )
		{
			return true;
		}		
	}

	Warning( "GamesTypes: unable to get game type and mode. Could not find type/mode matching type:%s/mode:%s.\n", szGameType, szGameMode );
	return false;
}

int GameTypes::GetNoResetVoteThresholdForCurrentModeCT( void )
{
	GameType *pGameType = GetCurrentGameType_Internal();
	Assert( pGameType );
	if ( !pGameType )
	{
		return -1;
	}

	GameMode *pGameMode = GetCurrentGameMode_Internal( pGameType );
	Assert( pGameMode );
	if ( !pGameMode )
	{
		return -1;
	}

	return pGameMode->m_NoResetVoteThresholdCT;
}

int GameTypes::GetNoResetVoteThresholdForCurrentModeT( void )
{
	GameType *pGameType = GetCurrentGameType_Internal();
	Assert( pGameType );
	if ( !pGameType )
	{
		return -1;
	}

	GameMode *pGameMode = GetCurrentGameMode_Internal( pGameType );
	Assert( pGameMode );
	if ( !pGameMode )
	{
		return -1;
	}

	return pGameMode->m_NoResetVoteThresholdT;
}

int GameTypes::GetCurrentServerNumSlots( void )
{
	// This is only valid if we are connected to a server and received the extended info blob
#if defined ( MATCHMAKING_DLL )
	Assert(  m_pExtendedServerInfo && g_pMatchExtensions->GetIVEngineClient() && g_pMatchExtensions->GetIVEngineClient()->IsConnected() );
#endif
	Assert ( GameTypes_IsOnClient() );

	return m_iCurrentServerNumSlots;
}

int GameTypes::GetCurrentServerSettingInt( const char *szSetting, int iDefaultValue )
{
	return m_pExtendedServerInfo->GetInt( szSetting, iDefaultValue );
}

bool GameTypes::CreateOrUpdateWorkshopMapGroup( const char* szName, const CUtlStringList & vecMapNames )
{
	return !!CreateWorkshopMapGroupInternal( szName, vecMapNames );
}

GameTypes::MapGroup * GameTypes::CreateWorkshopMapGroupInternal( const char* szName, const CUtlStringList & vecMapNames )
{
	MapGroup *pMapGroup = GetMapGroup_Internal( szName );
	if ( !pMapGroup )
	{
		pMapGroup = new MapGroup;
		V_strcpy_safe( pMapGroup->m_Name, szName );
		m_MapGroups.AddToTail( pMapGroup );
	}

	// Workshop map groups are named their publishfileid, so a nonzero integer. 
	Assert( V_atoui64( szName ) != 0 );
	pMapGroup->m_bIsWorkshopMapGroup = true;

	// Clear old map list, stomp with new one
	pMapGroup->m_Maps.PurgeAndDeleteElements();

	FOR_EACH_VEC( vecMapNames, i )
	{
		const char* szMap = vecMapNames[i];
		pMapGroup->m_Maps.CopyAndAddToTail( szMap );
		V_FixSlashes( pMapGroup->m_Maps.Tail(), '/' );
	}
	return pMapGroup;
}

bool GameTypes::IsWorkshopMapGroup( const char* szMapGroupName )
{
	MapGroup *pMapGroup = GetMapGroup_Internal( szMapGroupName );
	return pMapGroup && pMapGroup->m_bIsWorkshopMapGroup;
}

// ============================================================================================ //
// Helper functions
// ============================================================================================ //

// -------------------------------------------------------------------------------------------- //
// Purpose: Display the convars for the current game mode.
// -------------------------------------------------------------------------------------------- //
void DisplayGameModeConvars( void )
{
	if ( g_pGameTypes )
	{
		g_pGameTypes->DisplayConvarsForCurrentMode();
	}
}

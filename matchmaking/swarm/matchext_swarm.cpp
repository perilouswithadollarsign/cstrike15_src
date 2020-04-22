//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "matchext_swarm.h"
#include "swarm.spa.h"

#include "utlvector.h"
#include "utlstringmap.h"
#include "fmtstr.h"
#include "filesystem.h"

#define g_pFileSystem g_pFullFileSystem

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CMatchExtSwarm::CMatchExtSwarm()
{
	m_pKeyValues = NULL;
}

CMatchExtSwarm::~CMatchExtSwarm()
{
	if ( m_pKeyValues )
	{
		m_pKeyValues->deleteThis();
		m_pKeyValues = NULL;
	}
}

static CMatchExtSwarm g_MatchExtSwarm;
CMatchExtSwarm *g_pMatchExtSwarm = &g_MatchExtSwarm;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMatchExtSwarm, IMatchExtSwarm,
								   IMATCHEXT_SWARM_INTERFACE, g_MatchExtSwarm );


//
// Implementation
//

void CMatchExtSwarm::ParseMissionFromFile( char const *szFile, bool bBuiltIn )
{
	KeyValues *pMissionModes = m_pKeyValues->FindKey( "GameModes" );
	KeyValues *pMissionsRoot = m_pKeyValues->FindKey( "Missions" );
	if ( !pMissionsRoot || !pMissionModes )
		return;

	// See if we already have this mission
	if ( m_mapFilesLoaded.Find( szFile ) != m_mapFilesLoaded.InvalidIndex() )
		return;
	m_mapFilesLoaded[ szFile ] = NULL;

	KeyValues *missionKeys = new KeyValues( "mission" );
	KeyValues::AutoDelete autodelete_missionKeys( missionKeys );	// allows for early error-return

	bool bLoadResult = missionKeys->LoadFromFile( g_pFileSystem, szFile );
	if ( !bLoadResult )
	{
		Warning( "MissionManager:  Mission file \"%s\" is malformed, failed to parse.\n", szFile );
		return;
	}

	const char *name = missionKeys->GetString( "name", NULL );
	const char *campaignVersion = missionKeys->GetString( "version", NULL );

	if ( !name || !campaignVersion )
	{
		Warning( "MissionManager:  Mission file \"%s\" is missing name and version\n", szFile );
		return;
	}

	// Check invalid characters
	for ( char const *pCharCheck = name; *pCharCheck; ++ pCharCheck )
	{
		char const c = *pCharCheck;
		if ( !( ( c >= 'a' && c <= 'z' ) ||
				( c >= 'A' && c <= 'Z' ) ||
				( c >= '0' && c <= '9' ) ) )
		{
			Warning( "MissionManager:  Only alphanumeric characters allowed in mission name: \"%s\" in \"%s\"\n", name, szFile );
			return;
		}
	}
	for ( char const *pCharCheck = campaignVersion; *pCharCheck; ++ pCharCheck )
	{
		char const c = *pCharCheck;
		if ( !( c >= '0' && c <= '9' ) )
		{
			Warning( "MissionManager:  Only numeric characters allowed in mission version: \"%s\" in \"%s\"\n", campaignVersion, szFile );
			return;
		}
	}

	// Now go through the game modes
	KeyValues *modes = missionKeys->FindKey( "modes" );
	if ( !modes )
	{
		Warning( "MissionManager:  Mission file \"%s\" is missing data for any game modes\n", szFile );
		return;
	}

	CFmtStr sNameVersion( "%s_%s", name, campaignVersion );

	// Don't allow duplicates
	if ( m_mapMissionsLoaded.Find( name ) != m_mapMissionsLoaded.InvalidIndex() )
	{
		Warning( "MissionManager:  Duplicate mission \"%s\" in file \"%s\", already loaded from \"%s\".\n",
			name, szFile, m_mapMissionsLoaded[ name ]->GetString( "cfgfile" ) );
		return;
	}

	DevMsg( "\tMission %s ver %s loading...\n", name, campaignVersion );

	// Validate DisplayTitle and Description
	char const *szDisplayTitle = missionKeys->GetString( "displaytitle" );
	if ( !szDisplayTitle || !*szDisplayTitle )
		missionKeys->SetString( "displaytitle", name );

	// Set auto-generated fields
	missionKeys->SetName( name );
	missionKeys->SetInt( "builtin", bBuiltIn ? 1 : 0 );
	missionKeys->SetString( "cfgfile", szFile );
	missionKeys->SetString( "cfgtag", sNameVersion );

	// Load game modes
	int numGameModes = 0;
	for ( KeyValues *modeName = pMissionModes->GetFirstTrueSubKey(); modeName; modeName = modeName->GetNextTrueSubKey() )
	{
		KeyValues *mode = modes->FindKey( modeName->GetName() );
		if ( !mode )
			continue;

		int numChapters = 0;
		while ( KeyValues *pChapterKey = mode->FindKey( CFmtStr( "%d", numChapters + 1 ) ) )
		{
			// Check required fields
			char const *szMap = pChapterKey->GetString( "map" );
			if ( !szMap || !*szMap )
			{
				Warning( "MissionManager:  Mission file \"%s\" has invalid map specified for modes/%s/%s\n",
					szFile, modeName->GetName(), pChapterKey->GetName() );
				numChapters = 0;
				break;
			}

			// DisplayName
			char const *szDisplayName = pChapterKey->GetString( "displayname" );
			if ( !szDisplayName || !*szDisplayName )
			{
				pChapterKey->SetString( "displayname", CFmtStr( "%s-%s", name, pChapterKey->GetName() ) );
			}

			// Image
			char const *szImage = pChapterKey->GetString( "image" );
			if ( !szImage || !*szImage )
			{
				pChapterKey->SetString( "image", "maps/unknown" );
			}

			// Set the automatic fields
			pChapterKey->SetInt( "chapter", numChapters + 1 );
			
			// This chapter was valid
			++ numChapters;
		}

		if ( !numChapters )
		{
			modes->RemoveSubKey( mode );
			mode->deleteThis();
			mode = NULL;

			Warning( "MissionManager:  Mission file \"%s\" has invalid settings for game mode %s\n",
				szFile, modeName->GetName() );
			continue;
		}

		mode->SetInt( "chapters", numChapters );
		DevMsg( "\t\tloaded %d %s chapters.\n", numChapters, modeName->GetName() );

		++ numGameModes;
	}

	if ( !numGameModes )
	{
		Warning( "MissionManager:  Mission file \"%s\" does not have valid data for any supported game mode\n", szFile );
		return;
	}

	//
	// Bind the loaded mission keys into the system
	//
	m_mapFilesLoaded[ szFile ] = missionKeys;
	m_mapMissionsLoaded[ name ] = missionKeys;

	pMissionsRoot->AddSubKey( missionKeys );
	autodelete_missionKeys.Assign( NULL ); // prevent automatic deletion

	// Register all the loaded game modes
	for ( KeyValues *modeName = pMissionModes->GetFirstTrueSubKey(); modeName; modeName = modeName->GetNextTrueSubKey() )
	{
		KeyValues *mode = modes->FindKey( modeName->GetName() );
		if ( !mode )
			continue;

		modeName->SetPtr( name, missionKeys );
	}

	DevMsg( "\tMission %s ver %s loaded %d game modes.\n", name, campaignVersion, numGameModes );
}

void CMatchExtSwarm::MakeGameModeCopy( char const *szGameMode, char const *szCopyName )
{
	// Fix the GameModes key
	if ( KeyValues *pKeyMode = m_pKeyValues->FindKey( CFmtStr( "GameModes/%s", szGameMode ) ) )
	{
		pKeyMode = pKeyMode->MakeCopy();
		pKeyMode->SetName( szCopyName );
		m_pKeyValues->FindKey( "GameModes" )->AddSubKey( pKeyMode );
	}

	// Fix all missions
	KeyValues *pMission = GetAllMissions();
	for ( pMission = pMission ? pMission->GetFirstTrueSubKey() : NULL;
		  pMission; pMission = pMission->GetNextTrueSubKey() )
	{
		if ( KeyValues *pKeyMode = pMission->FindKey( CFmtStr( "modes/%s", szGameMode ) ) )
		{
			pKeyMode = pKeyMode->MakeCopy();
			pKeyMode->SetName( szCopyName );
			pMission->FindKey( "modes" )->AddSubKey( pKeyMode );
		}
	}
}

void CMatchExtSwarm::Initialize()
{
	DevMsg( "Loading Mission Data\n" );
	MEM_ALLOC_CREDIT();

	if ( m_pKeyValues )
	{
		m_pKeyValues->deleteThis();
		m_pKeyValues = NULL;
	}

	m_mapFilesLoaded.Purge();
	m_mapMissionsLoaded.Purge();

	m_pKeyValues = KeyValues::FromString(
		"AlienSwarm",
		" GameModes { "
			" coop { } "
#ifndef _DEMO
			" versus { } "
			" survival { } "
			" scavenge { } "
#endif
		" } "
		" Missions { "
			// read from mission files
		" } "
		);

	//
	// Parse built-in missions
	//

#ifndef _DEMO
	ParseMissionFromFile( "missions/campaign1.txt", true );
	ParseMissionFromFile( "missions/campaign2.txt", true );
	ParseMissionFromFile( "missions/campaign3.txt", true );
	ParseMissionFromFile( "missions/campaign4.txt", true );
	ParseMissionFromFile( "missions/campaign5.txt", true );
	ParseMissionFromFile( "missions/credits.txt", true );

	//
	// Search missions using the wildcards
	//
	char szMissionPath[_MAX_PATH];
	Q_snprintf( szMissionPath, sizeof( szMissionPath ), "missions/*.txt" );
	Q_FixSlashes( szMissionPath );

	FileFindHandle_t handle;
	const char *pFoundFile = g_pFileSystem->FindFirst( szMissionPath, &handle );
	while ( pFoundFile )
	{
		char pFilename[ MAX_PATH ];
		V_snprintf( pFilename, ARRAYSIZE(pFilename), "missions/%s", pFoundFile );
		pFoundFile = g_pFileSystem->FindNext( handle );

		ParseMissionFromFile( pFilename, false );
	}

#else
	ParseMissionFromFile( "missions/demo.txt", true );
#endif
	

#ifndef _DEMO
	// Make game mode copies
	MakeGameModeCopy( "versus", "teamversus" );
	MakeGameModeCopy( "scavenge", "teamscavenge" );
	MakeGameModeCopy( "coop", "realism" );
#endif

	DevMsg( "Loading Mission Data Finished\n" );
}

void CMatchExtSwarm::DebugPrint()
{
	KeyValuesDumpAsDevMsg( m_pKeyValues, 1, 0 );
}

//--------------------------------------------------------------------------------------------------------
CON_COMMAND( mission_reload, "Reload mission metadata" )
{
	g_MatchExtSwarm.Initialize();
}

CON_COMMAND_F( mission_debug_print, "Print all mission metadata", FCVAR_DEVELOPMENTONLY )
{
	g_MatchExtSwarm.DebugPrint();
}

KeyValues * CMatchExtSwarm::GetAllMissions()
{
	if ( !m_pKeyValues )
		return NULL;

	return m_pKeyValues->FindKey( "Missions" );
}

// Get server map information for the session settings
KeyValues * CMatchExtSwarm::GetMapInfo( KeyValues *pSettings, KeyValues **ppMissionInfo )
{
	if ( !m_pKeyValues )
		return NULL;

	char const *szGameMode = pSettings->GetString( "game/mode", NULL );
	if ( !szGameMode || !*szGameMode )
		return NULL;

	char const *szCampaign = pSettings->GetString( "game/campaign", NULL );
	if ( !szCampaign || !*szCampaign )
		return NULL;

	int nMapNumber = pSettings->GetInt( "game/chapter", 0 );
	if ( nMapNumber <= 0 )
		return NULL;

	// Find the campaign key
	KeyValues *pMissionKey = ( KeyValues * ) m_pKeyValues->GetPtr( CFmtStr( "GameModes/%s/%s", szGameMode, szCampaign ), NULL );
	if ( !pMissionKey )
		return NULL;

	// Find the total number of chapters in that mission's game mode
	int numChapters = pMissionKey->GetInt( CFmtStr( "modes/%s/chapters", szGameMode ), 0 );
	if ( nMapNumber > numChapters )
		return NULL;

	KeyValues *pChapterKey = pMissionKey->FindKey( CFmtStr( "modes/%s/%d", szGameMode, nMapNumber ) );
	if ( !pChapterKey )
		return NULL;

	if ( ppMissionInfo )
		*ppMissionInfo = pMissionKey;

	return pChapterKey;
}

KeyValues * CMatchExtSwarm::GetMapInfoByBspName( KeyValues *pSettings, char const *szBspMapName, KeyValues **ppMissionInfo )
{
	if ( !m_pKeyValues )
		return NULL;

	Assert( szBspMapName );
	if ( !szBspMapName || !*szBspMapName )
		return NULL;

	char const *szGameMode = pSettings->GetString( "game/mode", NULL );
	if ( !szGameMode || !*szGameMode )
		return NULL;

	// Walk all the missions in that game mode
	KeyValues *pModeMissions = m_pKeyValues->FindKey( CFmtStr( "GameModes/%s", szGameMode ) );
	if ( !pModeMissions )
		return NULL;

	for ( KeyValues *pMissionName = pModeMissions->GetFirstValue(); pMissionName; pMissionName = pMissionName->GetNextValue() )
	{
		KeyValues *pMission = ( KeyValues * ) pMissionName->GetPtr();
		if ( !pMission )
			continue;

		KeyValues *pChapters = pMission->FindKey( CFmtStr( "modes/%s", szGameMode ) );
		if ( !pChapters )
			continue;

		int numChapters = pChapters->GetInt( "chapters" );
		for ( int k = 1; k <= numChapters; ++ k )
		{
			KeyValues *pMap = pChapters->FindKey( CFmtStr( "%d", k ) );
			if ( !pMap )
				break;

			char const *szBspName = pMap->GetString( "map" );
			if ( !Q_stricmp( szBspName, szBspMapName ) )
			{
				if ( ppMissionInfo )
					*ppMissionInfo = pMission;

				return pMap;
			}
		}
	}

	return NULL;
}
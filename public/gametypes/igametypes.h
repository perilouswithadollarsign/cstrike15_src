//========= Copyright (c) 1996-2011, Valve Corporation, All rights reserved. ============//
//
// Purpose: Game types and modes
//
// $NoKeywords: $
//=============================================================================//

#ifndef IGAME_TYPES_H
#define IGAME_TYPES_H

#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "utlstring.h"
#include "tier1/keyvalues.h"

abstract_class IGameTypes
{
public:
	struct WeaponProgression
	{
		WeaponProgression()
			: m_Kills( 0 )
		{
		}

		CUtlString m_Name;
		int m_Kills;
	};

public:
	virtual ~IGameTypes() {}

	// Initialization. "force" will reload the data from file even if this interface has already been initialized.
	virtual bool Initialize( bool force = false ) = 0;
	virtual bool IsInitialized( void ) const = 0;

	//
	// Game Types and Modes
	//

	// Set the game type and mode convars from the given strings.
	virtual bool SetGameTypeAndMode( const char *gameType, const char *gameMode ) = 0;
	virtual bool GetGameTypeAndModeFromAlias( const char *modeAlias, int& iOutGameType, int& iOutGameMode ) = 0;
	virtual bool SetGameTypeAndMode( int nGameType, int nGameMode ) = 0;
	virtual void SetAndParseExtendedServerInfo( KeyValues *pExtendedServerInfo ) = 0;
	virtual void CheckShouldSetDefaultGameModeAndType( const char* szMapNameFull ) = 0;

	// Get the indexes for the current game type and mode.
	virtual int GetCurrentGameType() const = 0;
	virtual int GetCurrentGameMode() const = 0;

	virtual const char *GetCurrentMapName() = 0;

	// Get the current game type and mode UI strings.
	virtual const char *GetCurrentGameTypeNameID( void ) = 0;
	virtual const char *GetCurrentGameModeNameID( void ) = 0;

	// Apply the game mode convars for the current game type and mode.
	virtual bool ApplyConvarsForCurrentMode( bool isMultiplayer ) = 0;

	// Output the values of the convars for the current game mode.
	virtual void DisplayConvarsForCurrentMode( void ) = 0;

	// Returns the weapon progression for the current game type and mode.
	virtual const CUtlVector< WeaponProgression > *GetWeaponProgressionForCurrentModeCT( void ) = 0;
	virtual const CUtlVector< WeaponProgression > *GetWeaponProgressionForCurrentModeT( void ) = 0;

	virtual int GetNoResetVoteThresholdForCurrentModeCT( void ) = 0;
	virtual int GetNoResetVoteThresholdForCurrentModeT( void ) = 0;

	virtual const char *GetGameTypeFromInt( int gameType ) = 0;
	virtual const char *GetGameModeFromInt( int gameType, int gameMode ) = 0;

	virtual bool GetGameModeAndTypeIntsFromStrings( const char* szGameType, const char* szGameMode, int& iOutGameType, int& iOutGameMode ) = 0;
	virtual bool GetGameModeAndTypeNameIdsFromStrings( const char* szGameType, const char* szGameMode, const char*& szOutGameTypeNameId, const char*& szOutGameModeNameId ) = 0;


	virtual bool CreateOrUpdateWorkshopMapGroup( const char* szName, const CUtlStringList& vecMapNames ) = 0;
	virtual bool IsWorkshopMapGroup( const char* szMapGroupName ) = 0;

	//
	// Maps
	//

	// Get maps from mapgroup and get mapgroup from type and mode
	virtual const char *GetRandomMapGroup( const char *gameType, const char *gameMode ) = 0;
	virtual const char *GetFirstMap( const char *mapGroup ) = 0;
	virtual const char *GetRandomMap( const char *mapGroup ) = 0;
	virtual const char *GetNextMap( const char *mapGroup, const char *mapName ) = 0;

	virtual int GetMaxPlayersForTypeAndMode( int iType, int iMode ) = 0;

	virtual bool IsValidMapGroupName( const char * mapGroup ) = 0;
	virtual bool IsValidMapInMapGroup( const char * mapGroup, const char *mapName ) = 0;
	virtual bool IsValidMapGroupForTypeAndMode( const char * mapGroup, const char *gameType, const char *gameMode ) = 0;

	// Apply the convars for the given map.
	virtual bool ApplyConvarsForMap( const char *mapName, bool isMultiplayer ) = 0;

	// Get specifics about a map.
	virtual bool GetMapInfo( const char *mapName, uint32 &richPresence ) = 0;

	// Returns the available character model names (T or CT) for the given map.
	virtual const CUtlStringList *GetTModelsForMap( const char *mapName ) = 0;
	virtual const CUtlStringList *GetCTModelsForMap( const char *mapName ) = 0;
	virtual const CUtlStringList *GetHostageModelsForMap( const char *mapName ) = 0;
	virtual const int GetDefaultGameTypeForMap( const char *mapName ) = 0;
	virtual const int GetDefaultGameModeForMap( const char *mapName ) = 0;

	// Returns the view model arms name (T or CT) for the given map.
	virtual const char *GetTViewModelArmsForMap( const char *mapName ) = 0;
	virtual const char *GetCTViewModelArmsForMap( const char *mapName ) = 0;

	// Item requirements for the map
	virtual const char *GetRequiredAttrForMap( const char *mapName ) = 0;
	virtual int GetRequiredAttrValueForMap( const char *mapName ) = 0;
	virtual const char *GetRequiredAttrRewardForMap( const char *mapName ) = 0;
	virtual int GetRewardDropListForMap( const char *mapName ) = 0;

	// Map group properties
	virtual const CUtlStringList *GetMapGroupMapList( const char *mapGroup ) = 0;
	
	virtual bool GetRunMapWithDefaultGametype() = 0;
	virtual void SetRunMapWithDefaultGametype( bool bDefaultGametype ) = 0;

	virtual bool GetLoadingScreenDataIsCorrect() = 0;
	virtual void SetLoadingScreenDataIsCorrect( bool bLoadingScreenDataIsCorrect ) = 0;

	//
	// Custom Bot Difficulty for Offline games
	//

	// Sets the bot difficulty for Offline games.
	virtual bool SetCustomBotDifficulty( int botDiff ) = 0;

	// Returns the bot difficulty for Offline games.
	virtual int GetCustomBotDifficulty( void ) = 0;

	virtual int GetCurrentServerNumSlots( void ) = 0;
	virtual int GetCurrentServerSettingInt( const char *szSetting, int iDefaultValue = 0 ) = 0;

	virtual bool GetGameTypeFromMode( const char *szGameMode, const char *&pszGameTypeOut ) = 0;

	virtual bool LoadMapEntry( KeyValues *pKV ) = 0;
};

#define VENGINE_GAMETYPES_VERSION "VENGINE_GAMETYPES_VERSION002"

#endif // IGAME_TYPES_H
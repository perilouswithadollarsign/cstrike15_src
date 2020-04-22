//===== Copyright c 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IMATCHTITLE_H
#define IMATCHTITLE_H

#ifdef _WIN32
#pragma once
#endif

#if defined ( _PS3 )

#define TITLE_DATA_PREFIX "PS3."
#define TITLE_DATA_DEVICE_MOVE_PREFIX "MOVE."
#define TITLE_DATA_DEVICE_SHARP_SHOOTER_PREFIX "SHARP_SHOOTER."

#else

#define TITLE_DATA_PREFIX ""

#endif

struct TitleDataFieldsDescription_t
{
	enum DataType_t
	{
		DT_0		= 0,
		DT_uint8	= 8,
		DT_BITFIELD = 9,
		DT_uint16	= 16,
		DT_uint32	= 32,
		DT_float	= 33,
		DT_uint64	= 64
	};

	enum DataBlock_t
	{
		DB_TD1		= 0,
		DB_TD2		= 1,
		DB_TD3		= 2,

		DB_TD_COUNT  = 3
	};

	char const *m_szFieldName;
	DataBlock_t m_iTitleDataBlock;
	DataType_t m_eDataType;
	union
	{
		int m_numBytesOffset;
		int m_nUserDataValue0;
	};
};

struct TitleAchievementsDescription_t
{
	char const *m_szAchievementName;			// Name by which achievement can be awarded and queried
	int m_idAchievement;						// Achievement ID on the platform
	int m_numComponents;						// Number of achievement component title data bits
};

struct TitleAvatarAwardsDescription_t
{
	char const *m_szAvatarAwardName;			// Name by which avatar award can be awarded and queried
	int m_idAvatarAward;						// Avatar award ID on the platform
	char const *m_szTitleDataBitfieldStatName;	// Name of a bitfield in title data storage representing whether avatar award has been earned
};

struct TitleDlcDescription_t
{
	uint64 m_uiLicenseMaskId;
	int m_idDlcAppId;
	int m_idDlcPackageId;
	char const *m_szTitleDataBitfieldStatName;	// Name of a bitfield in title data storage representing whether dlc has been discovered installed
};

enum TitleSettingsFlags_t
{
	MATCHTITLE_SETTING_MULTIPLAYER		= ( 1 << 0 ),	// Title wants network sockets initialization
	MATCHTITLE_SETTING_NODEDICATED		= ( 1 << 1 ),	// Title doesn't support dedicated servers
	MATCHTITLE_PLAYERMGR_DISABLED		= ( 1 << 2 ),	// Title doesn't need friends presence poll
	MATCHTITLE_SERVERMGR_DISABLED		= ( 1 << 3 ),	// Title doesn't need group servers poll
	MATCHTITLE_INVITE_ONLY_SINGLE_USER	= ( 1 << 4 ),	// Accepted game invite forcefully disables splitscreen (e.g.: 1-on-1 games)
	MATCHTITLE_VOICE_INGAME				= ( 1 << 5 ),	// When in active gameplay lobby system doesn't transmit voice
	MATCHTITLE_XLSPPATCH_SUPPORTED		= ( 1 << 6 ),	// Title supports xlsp patch containers
	MATCHTITLE_PLAYERMGR_ALLFRIENDS		= ( 1 << 7 ),	// Player manager by default fetches only friends for same game, this will force all friends to be fetched
	MATCHTITLE_PLAYERMGR_FRIENDREQS		= ( 1 << 8 ),	// Player manager by default fetches only real friends, this will force friend requests to also be fetched
};

abstract_class IMatchTitle
{
public:
	// Title ID
	virtual uint64 GetTitleID() = 0;

	// Service ID for XLSP
	virtual uint64 GetTitleServiceID() = 0;

	// Describe title settings using a bitwise combination of flags
	virtual uint64 GetTitleSettingsFlags() = 0;

	// Prepare network startup params for the title
	virtual void PrepareNetStartupParams( void *pNetStartupParams ) = 0;

	// Get total number of players supported by the title
	virtual int GetTotalNumPlayersSupported() = 0;

	// Get a guest player name
	virtual char const * GetGuestPlayerName( int iUserIndex ) = 0;

	// Decipher title data fields
	virtual TitleDataFieldsDescription_t const * DescribeTitleDataStorage() = 0;

	// Title achievements
	virtual TitleAchievementsDescription_t const * DescribeTitleAchievements() = 0;

	// Title avatar awards
	virtual TitleAvatarAwardsDescription_t const * DescribeTitleAvatarAwards() = 0;

	// Title leaderboards
	virtual KeyValues * DescribeTitleLeaderboard( char const *szLeaderboardView ) = 0;

	// Sets up all necessary client-side convars and user info before
	// connecting to server
	virtual void PrepareClientForConnect( KeyValues *pSettings ) = 0;

	// Start up a listen server with the given settings
	virtual bool StartServerMap( KeyValues *pSettings ) = 0;

	// Title DLC description
	virtual TitleDlcDescription_t const * DescribeTitleDlcs() = 0;

	// Run every frame
	virtual void RunFrame() = 0;
};

//
// Matchmaking title settings extension interface
//

abstract_class IMatchTitleGameSettingsMgr
{
public:
	// Extends server game details
	virtual void ExtendServerDetails( KeyValues *pDetails, KeyValues *pRequest ) = 0;
	
	// Adds the essential part of game details to be broadcast
	virtual void ExtendLobbyDetailsTemplate( KeyValues *pDetails, char const *szReason, KeyValues *pFullSettings ) = 0;

	// Extends game settings update packet for lobby transition,
	// either due to a migration or due to an endgame condition
	virtual void ExtendGameSettingsForLobbyTransition( KeyValues *pSettings, KeyValues *pSettingsUpdate, bool bEndGame ) = 0;

	// Adds data for datacenter reporting
	virtual void ExtendDatacenterReport( KeyValues *pReportMsg, char const *szReason ) = 0;


	// Rolls up game details for matches grouping
	//	valid pDetails, null pRollup
	//		returns a rollup representation of pDetails to be used as an indexing key
	//	valid pDetails, valid pRollup (usually called second time)
	//		rolls the details into the rollup, aggregates some values, when
	//		the aggregate values are missing in pRollup, then this is the first
	//		details entry being aggregated and would establish the first rollup
	//		returns pRollup
	//	null pDetails, valid pRollup
	//		tries to determine if the rollup should remain even though no details
	//		matched it, adjusts pRollup to represent no aggregated data
	//		returns null to drop pRollup, returns pRollup to keep rollup
	virtual KeyValues * RollupGameDetails( KeyValues *pDetails, KeyValues *pRollup, KeyValues *pQuery ) = 0;


	// Defines session search keys for matchmaking
	virtual KeyValues * DefineSessionSearchKeys( KeyValues *pSettings ) = 0;

	// Defines dedicated server search key
	virtual KeyValues * DefineDedicatedSearchKeys( KeyValues *pSettings, bool bNeedOfficialServer, int nSearchPass ) = 0;


	// Initializes full game settings from potentially abbreviated game settings
	virtual void InitializeGameSettings( KeyValues *pSettings, char const *szReason ) = 0;

	// Sets the bspname key given a mapgroup
	virtual void SetBspnameFromMapgroup( KeyValues *pSettings ) = 0;

	// Extends game settings update packet before it gets merged with
	// session settings and networked to remote clients
	virtual void ExtendGameSettingsUpdateKeys( KeyValues *pSettings, KeyValues *pUpdateDeleteKeys ) = 0;

	virtual KeyValues * ExtendTeamLobbyToGame( KeyValues *pSettings ) = 0;

	// Prepares system for session creation
	virtual KeyValues * PrepareForSessionCreate( KeyValues *pSettings ) = 0;


	// Executes the command on the session settings, this function on host
	// is allowed to modify Members/Game subkeys and has to fill in modified players KeyValues
	// When running on a remote client "ppPlayersUpdated" is NULL and players cannot
	// be modified
	virtual void ExecuteCommand( KeyValues *pCommand, KeyValues *pSessionSystemData, KeyValues *pSettings, KeyValues **ppPlayersUpdated ) = 0;

	// Prepares the host lobby for game or adjust settings of new players who
	// join a game in progress, this function is allowed to modify
	// Members/Game subkeys and has to fill in modified players KeyValues
	virtual void PrepareLobbyForGame( KeyValues *pSettings, KeyValues **ppPlayersUpdated ) = 0;

	// Prepares the host team lobby for game adjusting the game settings
	// this function is allowed to prepare modification package to update
	// Game subkeys.
	// Returns the update/delete package to be applied to session settings
	// and pushed to dependent two sesssion of the two teams.
	virtual KeyValues * PrepareTeamLinkForGame( KeyValues *pSettingsLocal, KeyValues *pSettingsRemote ) = 0;

	
	// Prepares the client lobby for migration
	// this function is called when the client session is still in the state
	// of "client" while handling the original host disconnection and decision
	// has been made that local machine will be elected as new "host"
	// Returns NULL if migration should proceed normally
	// Returns [ kvroot { "error" "n/a" } ] if migration should be aborted.
	virtual KeyValues * PrepareClientLobbyForMigration( KeyValues *pSettingsLocal, KeyValues *pMigrationInfo ) = 0;

	// Prepares the session for server disconnect
	// this function is called when the session is still in the active gameplay
	// state and while localhost is handling the disconnection from game server.
	// Returns NULL to allow default flow
	// Returns [ kvroot { "disconnecthdlr" "<opt>" } ] where <opt> can be:
	//		"destroy" : to trigger a disconnection error and destroy the session
	//		"lobby" : to initiate a "salvaging" lobby transition
	virtual KeyValues * PrepareClientLobbyForGameDisconnect( KeyValues *pSettingsLocal, KeyValues *pDisconnectInfo ) = 0;

	// Validates if client profile can set a stat or get awarded an achievement
	virtual bool AllowClientProfileUpdate( KeyValues *kvUpdate ) = 0;
	
	// Retrieves the indexed formula from the match system settings file. (MatchSystem.360.res)
	virtual char const * GetFormulaAverage( int index ) = 0;

	// Called by the client to notify matchmaking that it should update matchmaking properties based
	// on player distribution among the teams.
	virtual void UpdateTeamProperties( KeyValues *pCurrentSettings, KeyValues *pTeamProperties ) = 0;
};

#endif // IMATCHTITLE_H

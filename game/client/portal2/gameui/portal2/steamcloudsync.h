//========= Copyright © 1996-2011, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __STEAMCLOUDSYNC_H__
#define __STEAMCLOUDSYNC_H__

struct GameSteamCloudSyncInfo_t
{
	bool m_bUploadingToCloud;
	wchar_t m_wszDescription[256];
	float m_flProgress;
};

struct GameSteamCloudPreferences_t
{
	uint8 m_numSaveGamesToSync;
};

class IGameSteamCloudSync
{
public:
	enum Sync_t
	{
		SYNC_DEFAULT,
		SYNC_GAMEBOOTREADY
	};

public:
	virtual void Sync( Sync_t eSyncReason = SYNC_DEFAULT ) = 0;
	virtual void AbortAll() = 0;

	virtual void RunFrame() = 0;
	virtual bool IsSyncInProgress( GameSteamCloudSyncInfo_t *pGSCSI ) = 0;

	virtual void GetPreferences( GameSteamCloudPreferences_t &gscp ) = 0;
	virtual void OnEvent( KeyValues *pEvent ) = 0;

	virtual bool IsFileInCloud( char const *szInternalName ) = 0;
};

#if defined( _PS3 ) && !defined( NO_STEAM )
#define GAME_STEAM_CLOUD_SYNC_SUPPORTED
extern IGameSteamCloudSync *g_pGameSteamCloudSync;
#else
#define g_pGameSteamCloudSync ( ( IGameSteamCloudSync * ) NULL )
#endif

#endif

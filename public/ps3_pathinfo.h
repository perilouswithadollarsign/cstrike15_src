//===== Copyright © 1996-2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: Utility class for discovering and caching path info on the PS3.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PS3_PATHINFO_H
#define PS3_PATHINFO_H

#include <sysutil/sysutil_gamecontent.h>
#include <sysutil/sysutil_syscache.h>
#include "../common/ps3_cstrike15/ps3_title_id.h"

#define HDD_BOOT

/// Contains the game root / data / patch / etc paths. 
/// Needs to be initialized once and then you can access at will.
class CPs3ContentPathInfo 
{
public:
	// Call this once, when you've got the filesystem DLL loaded and ready to go. 
	// Returns CELL_GAME_RET_OK on success; see cpp for error codes
	enum Flags_t
	{
		INIT_RETAIL_MODE	= ( 1 << 0 ),
		INIT_IMAGE_ON_BDVD	= ( 1 << 1 ),
		INIT_IMAGE_ON_HDD	= ( 1 << 2 ),
		INIT_IMAGE_APP_HOME	= ( 1 << 3 ),
		INIT_PRX_ON_BDVD	= ( 1 << 4 ),
		INIT_PRX_ON_HDD		= ( 1 << 5 ),
		INIT_PRX_APP_HOME	= ( 1 << 6 ),
		INIT_SYS_CACHE_CLEAR = ( 1 << 7 ),
	};
	int Init( unsigned int uiFlagsMask );

	inline const char *GameAppVer() const;
	inline const char *GamePatchAppVer() const;

	inline const char *PrxPath() const; // where the PRXes can be found  (eg, yadda/bin)
	inline const char *GameBasePath() const; // the directory equivalent to game/... in a PC/Steam install.
	inline const char *GamePatchBasePath() const; // the directory equivalent to game/... in a PC/Steam install.
	inline const char *SelfDirectory() const; /// the directory where the .self file can be found.
	inline const char *HDDataPath() const; /// the "game data" path which we create on the local hd
	inline const char *SystemCachePath() const; /// the "system cache" path as returned by cellSysCacheMount()

	inline const char *GameImagePath() const; // the directory tree under which to hunt for the .zip s. This can be different things depending on command line overrides.
	inline const char *SaveShadowPath() const; // the directory in which save files are temporarily stored. returned by engine->GetSaveDirName(). 

	inline int GetFreeHDDSpace( ) const ; /// the const version can only use the cached value

	// some helpful state queries:
	inline unsigned int GetGameBootAttributes() const { return m_nBootAttribs; }
	inline bool IsBeingDebugged() const { return (m_nBootAttribs & CELL_GAME_ATTRIBUTE_DEBUG) != 0; }
	inline bool IsPatched() const { return (m_nBootAttribs & CELL_GAME_ATTRIBUTE_PATCH) != 0; }
	inline bool IsInitialized() const { return m_bInitialized; }

	inline unsigned int BootType() const { return m_nBootType; }

	inline const char *GetParamSFO_Title() { return m_gameTitle; }
	inline const char *GetParamSFO_TitleID() { return m_gameTitleID; } // aka "product code" in the saveutil docs

	inline const char *GetWWMASTER_TitleID() { return PS3_GAME_TITLE_ID_WW_MASTER; }

	CPs3ContentPathInfo();
private:
	char m_gameTitle[CELL_GAME_SYSP_TITLE_SIZE];	// CELL_GAME_PARAMID_TITLE
	char m_gameTitleID[CELL_GAME_SYSP_TITLEID_SIZE];	// CELL_GAME_PARAMID_TITLE_ID
	char m_gameAppVer[CELL_GAME_SYSP_VERSION_SIZE]; // CELL_GAME_PARAMID_APP_VER
	char m_gamePatchAppVer[CELL_GAME_SYSP_VERSION_SIZE]; // CELL_GAME_PARAMID_APP_VER

	int m_gameParentalLevel; // CELL_GAME_PARAMID_PARENTAL_LEVEL
	int m_gameResolution; // CELL_GAME_PARAMID_RESOLUTION
	int m_gameSoundFormat; // CELL_GAME_PARAMID_SOUND_FORMAT

	char m_gameContentPath[CELL_GAME_PATH_MAX]; // as returned by contentPermit but usually meaningless (?)
	char m_gamePatchContentPath[CELL_GAME_PATH_MAX]; // as returned by contentPermit but usually meaningless (?)
	char m_gameBasePath[CELL_GAME_PATH_MAX];
	char m_gamePatchBasePath[CELL_GAME_PATH_MAX];
	char m_gameExesPath[CELL_GAME_PATH_MAX];
	char m_gameHDDataPath[CELL_GAME_PATH_MAX];
	char m_gameImageDataPath[CELL_GAME_PATH_MAX];
	char m_gameSystemCachePath[CELL_SYSCACHE_PATH_MAX]; // as returned by cellSysCacheMount()
	char m_gameSavesShadowPath[CELL_SYSCACHE_PATH_MAX]; // as returned by cellSysCacheMount()

	unsigned int m_nBootType; /// either CELL_GAME_GAMETYPE_DISC or CELL_GAME_GAMETYPE_HDD
	unsigned int m_nBootAttribs; /// some combination of attribute masks -- see .cpp for details

	int m_nHDDFreeSizeKb; /// cached value for free hard disk space in kb. 

	bool m_bInitialized;
};

const char * CPs3ContentPathInfo::GameAppVer() const
{
	return m_gameAppVer;
}

const char * CPs3ContentPathInfo::GamePatchAppVer() const
{
	return m_gamePatchAppVer;
}

const char * CPs3ContentPathInfo::GameBasePath() const
{
	return m_gameBasePath;
}

const char * CPs3ContentPathInfo::GamePatchBasePath() const
{
	return m_gamePatchBasePath;
}

const char * CPs3ContentPathInfo::SelfDirectory() const
{
	return m_gameBasePath; // happens to be the same in the current implementation
}

const char * CPs3ContentPathInfo::HDDataPath() const
{
	return m_gameHDDataPath; // happens to be the same in the current implementation
}

const char * CPs3ContentPathInfo::SystemCachePath() const
{
	return m_gameSystemCachePath; // happens to be the same in the current implementation
}

const char * CPs3ContentPathInfo::GameImagePath() const
{
	return m_gameImageDataPath; // happens to be the same in the current implementation
}


const char * CPs3ContentPathInfo::PrxPath() const
{
	return m_gameExesPath;
}

const char * CPs3ContentPathInfo::SaveShadowPath() const
{
	return m_gameSavesShadowPath;
}

int CPs3ContentPathInfo::GetFreeHDDSpace( ) const 
{
	return m_nHDDFreeSizeKb;
}

extern CPs3ContentPathInfo *g_pPS3PathInfo;

#endif // PS3_PATHINFO_H

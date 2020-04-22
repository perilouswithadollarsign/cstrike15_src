//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Save game read and write. Any *.hl? files may be stored in memory, so use
//			g_pSaveRestoreFileSystem when accessing them. The .sav file is always stored
//			on disk, so use g_pFileSystem when accessing it.
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
// Save / Restore System

#ifndef CSAVERESTORE_H
#define CSAVERESTORE_H

#ifdef _PS3
#include "ps3/saverestore_ps3_api_ui.h"
#endif

extern ConVar save_noxsave;


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
struct GAME_HEADER
{
	DECLARE_SIMPLE_DATADESC();

	char	mapName[32];
	char	comment[80];
	int		mapCount;		// the number of map state files in the save file.  This is usually number of maps * 3 (.hl1, .hl2, .hl3 files)
	char	originMapName[32];
	char	landmark[256];
};

struct SAVE_HEADER 
{
	DECLARE_SIMPLE_DATADESC();

	int		saveId;
	int		version;
	int		skillLevel;
	int		connectionCount;
	int		lightStyleCount;
	int		mapVersion;
	float	time;
	char	mapName[32];
	char	skyName[32];
};

struct SAVELIGHTSTYLE 
{
	DECLARE_SIMPLE_DATADESC();

	int		index;
	char	style[64];
};

extern void FinishAsyncSave();

struct RecentSaveInfo_t
{
	RecentSaveInfo_t()
	{
		m_MostRecentSavePath[0] = 0;
		m_MostRecentSaveComment[0] = 0;
		m_LastAutosaveDangerousComment[0] = 0;
		m_bValid = false;
	}

	char	m_MostRecentSavePath[MAX_OSPATH];
	char	m_MostRecentSaveComment[80];
	char	m_LastAutosaveDangerousComment[80];
	bool	m_bValid;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSaveRestore : public ISaveRestore
{
public:
	CSaveRestore()
	{
		m_bClearSaveDir = false;
		m_szSaveGameScreenshotFile[0] = 0;
		SetMostRecentElapsedMinutes( 0 );
		SetMostRecentElapsedSeconds( 0 );
		m_szMostRecentSaveLoadGame[0] = 0;
		m_szSaveGameName[ 0 ] = 0;
		m_bIsXSave = IsX360();
		m_bOverrideLoadGameEntsOn = false;
#ifdef _PS3
		m_PS3AutoSaveAsyncStatus.m_bUseSystemDialogs = true;
#endif
	}

	void					Init( void );
	void					Shutdown( void );
	void					OnFrameRendered();
	virtual bool			SaveFileExists( const char *pName );
	bool					LoadGame( const char *pName, bool bLetToolsOverrideLoadGameEnts );
	virtual bool			IsOverrideLoadGameEntsOn();
	virtual const char		*GetSaveDir(void);
	void					ClearSaveDir( void );
	void					DoClearSaveDir( bool bIsXSave );
	void					RequestClearSaveDir( void );
	int						LoadGameState( char const *level, bool createPlayers );
	void					LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName );
	const char				*FindRecentSave( char *pNameBuf, int nameBufLen );
	void					ForgetRecentSave( void );
	int						SaveGameSlot( const char *pSaveName, const char *pSaveComment, bool onlyThisLevel, bool bSetMostRecent, const char *pszDestMap = NULL, const char *pszLandmark = NULL );
	bool					SaveGameState( bool bTransition, ISaveRestoreDataCallback *pCallback = NULL, bool bOpenContainer = true, bool bIsAutosaveOrDangerous = false );
	void					RestoreClientState( char const *fileName, bool adjacent );
	void					RestoreAdjacenClientState( char const *map );
	int						IsValidSave( void );
	void					Finish( CSaveRestoreData *save );
	void					ClearRestoredIndexTranslationTables();
	void					OnFinishedClientRestore();
	void					AutoSaveDangerousIsSafe();
	virtual void			UpdateSaveGameScreenshots();
	virtual char const		*GetMostRecentlyLoadedFileName();
	virtual char const		*GetSaveFileName();

	virtual void			SetIsXSave( bool bIsXSave );
#ifdef _PS3
#pragma message("TODO: fix querying of user ID in ps3 (sony has them too)")
	virtual bool			IsXSave() { return ( m_bIsXSave && !save_noxsave.GetBool()  ); }
#else
	virtual bool			IsXSave() { return ( m_bIsXSave && !save_noxsave.GetBool() && XBX_GetPrimaryUserId() != -1 ); }
#endif

	virtual void			FinishAsyncSave() { ::FinishAsyncSave(); }

	void					AddDeferredCommand( char const *pchCommand );
	virtual bool			StorageDeviceValid( void );

	virtual bool			IsSaveInProgress();
	virtual bool			IsAutoSaveDangerousInProgress();
	virtual bool			IsAutoSaveInProgress();

	virtual bool			SaveGame( const char *pSaveName, bool bIsXSave, char *pOutName, int nOutNameSize, char *pOutComment, int nOutCommentSize );

	void					SetMostRecentSaveInfo( const char *pMostRecentSavePath, const char *pMostRecentSaveComment );
	bool					GetMostRecentSaveInfo( const char **ppMostRecentSavePath, const char **ppMostRecentSaveComment, const char **ppLastAutosaveDangerousComment );
	void					MarkMostRecentSaveInfoInvalid();

protected:
	bool					CalcSaveGameName( const char *pName, char *output, int outputStringLength );

private:
	bool					SaveClientState( const char *name );

	void					EntityPatchWrite( CSaveRestoreData *pSaveData, const char *level, bool bAsync = false );
	void					EntityPatchRead( CSaveRestoreData *pSaveData, const char *level );
	void					DirectoryCount( const char *pPath, int *pResult );
	void					DirectoryCopy( const char *pPath, const char *pDestFileName, bool bIsXSave );
	bool					DirectoryExtract( FileHandle_t pFile, int mapCount );
	void					DirectoryClear( const char *pPath );

	void					AgeSaveList( const char *pName, int count, bool bIsXSave );
	void					AgeSaveFile( const char *pName, const char *ext, int count, bool bIsXSave );
	int						SaveReadHeader( FileHandle_t pFile, GAME_HEADER *pHeader, int readGlobalState );
	CSaveRestoreData		*LoadSaveData( const char *level );
	void					ParseSaveTables( CSaveRestoreData *pSaveData, SAVE_HEADER *pHeader, int updateGlobals );
	int						FileSize( FileHandle_t pFile );

	CSaveRestoreData *		SaveGameStateInit( void );
	void 					SaveGameStateGlobals( CSaveRestoreData *pSaveData );
	int						SaveReadNameAndComment( FileHandle_t f, char *name, char *comment );
	void					BuildRestoredIndexTranslationTable( char const *mapname, CSaveRestoreData *pSaveData, bool verbose );
	char const				*GetSaveGameMapName( char const *level );

	void					SetMostRecentSaveGame( const char *pSaveName );
	int						GetMostRecentElapsedMinutes( void );
	int						GetMostRecentElapsedSeconds( void );
	int						GetMostRecentElapsedTimeSet( void );
	void					SetMostRecentElapsedMinutes( const int min );
	void					SetMostRecentElapsedSeconds( const int sec );

	struct SaveRestoreTranslate
	{
		string_t classname;
		int savedindex;
		int restoredindex;
	};

	struct RestoreLookupTable
	{
		RestoreLookupTable() :
			m_vecLandMarkOffset( 0, 0, 0 )
		{
		}

		void Clear()
		{
			lookup.RemoveAll();
			m_vecLandMarkOffset.Init();
		}

		RestoreLookupTable( const RestoreLookupTable& src )
		{
			int c = src.lookup.Count();
			for ( int i = 0 ; i < c; i++ )
			{
				lookup.AddToTail( src.lookup[ i ] );
			}

			m_vecLandMarkOffset = src.m_vecLandMarkOffset;
		}

		RestoreLookupTable& operator=( const RestoreLookupTable& src )
		{
			if ( this == &src )
				return *this;

			int c = src.lookup.Count();
			for ( int i = 0 ; i < c; i++ )
			{
				lookup.AddToTail( src.lookup[ i ] );
			}

			m_vecLandMarkOffset = src.m_vecLandMarkOffset;

			return *this;
		}

		CUtlVector< SaveRestoreTranslate >	lookup;
		Vector								m_vecLandMarkOffset;
	};

	RestoreLookupTable		*FindOrAddRestoreLookupTable( char const *mapname );
	int						LookupRestoreSpotSaveIndex( RestoreLookupTable *table, int save );
	void					ReapplyDecal( bool adjacent, RestoreLookupTable *table, decallist_t *entry );

	CUtlDict< RestoreLookupTable, int >	m_RestoreLookup;

	char	m_szMostRecentSaveLoadGame[MAX_OSPATH];
	char	m_szSaveGameName[MAX_OSPATH];

	char	m_szSaveGameScreenshotFile[MAX_OSPATH];
	float	m_flClientSaveRestoreTime;
	bool	m_bClearSaveDir;

	int		m_MostRecentElapsedMinutes;
	int		m_MostRecentElapsedSeconds;
	int		m_MostRecentElapsedTimeSet;

	bool	m_bWaitingForSafeDangerousSave;
	bool	m_bIsXSave;

	bool	m_bOverrideLoadGameEntsOn;

	int		m_nDeferredCommandFrames;
	CUtlVector< CUtlSymbol > m_sDeferredCommands;

protected:
	RecentSaveInfo_t m_MostRecentSaveInfo;

#ifdef _PS3
	CPS3SaveRestoreAsyncStatus m_PS3AutoSaveAsyncStatus;
#endif
};

#endif
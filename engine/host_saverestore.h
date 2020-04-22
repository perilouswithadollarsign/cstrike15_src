//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( HOST_SAVERESTORE_H )
#define HOST_SAVERESTORE_H
#ifdef _WIN32
#pragma once
#endif

class CSaveRestoreData;

abstract_class ISaveRestoreDataCallback
{
public:
	// Called by SaveGameState after building the server's CSaveRestoreData
	virtual void Execute( CSaveRestoreData *save ) = 0;
};

abstract_class ISaveRestore
{
public:
	virtual void					Init( void ) = 0;
	virtual void					Shutdown( void ) = 0;
	virtual void					OnFrameRendered() = 0;
	virtual bool					SaveFileExists( const char *pName ) = 0;
	virtual bool					LoadGame( const char *pName, bool bLetToolsOverrideLoadGameEnts ) = 0;
	virtual bool					IsOverrideLoadGameEntsOn() = 0;
	virtual const char				*GetSaveDir(void) = 0;
	virtual void					ClearSaveDir( void ) = 0;
	virtual void					RequestClearSaveDir( void ) = 0;
	virtual int						LoadGameState( char const *level, bool createPlayers ) = 0;
	virtual void					LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName ) = 0;
	virtual const char				*FindRecentSave( char *pNameBuf, int nameBufLen ) = 0;
	virtual void					ForgetRecentSave() = 0;
	virtual int						SaveGameSlot( const char *pSaveName, const char *pSaveComment, bool onlyThisLevel = false, bool bSetMostRecent = true, const char *pszDestMap = NULL, const char *pszLandmark = NULL ) = 0;
	virtual bool					SaveGameState( bool bTransition, ISaveRestoreDataCallback *pCallback = NULL, bool bOpenContainer = true, bool bIsAutosaveOrDangerous = false ) = 0;
	virtual int						IsValidSave( void ) = 0; // returns true if this is a valid time to make a save. (it doesn't ask if a particular save file is valid.)
	virtual void					Finish( CSaveRestoreData *save ) = 0;

	virtual void					RestoreClientState( char const *fileName, bool adjacent ) = 0;
	virtual void					RestoreAdjacenClientState( char const *map ) = 0;
	virtual int						SaveReadNameAndComment( FileHandle_t f, char *name, char *comment ) = 0;
	virtual int						GetMostRecentElapsedMinutes( void ) = 0;
	virtual int						GetMostRecentElapsedSeconds( void ) = 0;
	virtual int						GetMostRecentElapsedTimeSet( void ) = 0;
	virtual void					SetMostRecentElapsedMinutes( const int min ) = 0;
	virtual void					SetMostRecentElapsedSeconds( const int sec ) = 0;

	virtual void					UpdateSaveGameScreenshots() = 0;

	virtual void					OnFinishedClientRestore() = 0;

	virtual void					AutoSaveDangerousIsSafe() = 0;

	virtual char const				*GetMostRecentlyLoadedFileName() = 0;
	virtual char const				*GetSaveFileName() = 0;

	virtual bool					IsXSave( void ) = 0;
	virtual void					SetIsXSave( bool bState ) = 0;

	virtual void					FinishAsyncSave() = 0;
	virtual bool					StorageDeviceValid() = 0;
	virtual void					SetMostRecentSaveGame( const char *lpszFilename ) = 0;

	virtual bool					IsSaveInProgress() = 0;
	virtual bool					IsAutoSaveDangerousInProgress() = 0;

	virtual bool					SaveGame( const char *pSaveName, bool bIsXSave, char *pOutName, int nOutNameSize, char *pOutComment, int nOutCommentSize ) = 0;

	virtual bool					IsAutoSaveInProgress() = 0;
};

void *SaveAllocMemory( size_t num, size_t size, bool bClear = false );
void SaveFreeMemory( void *pSaveMem );

#ifndef DEDICATED
extern ISaveRestore *saverestore;
#endif

#endif // HOST_SAVERESTORE_H

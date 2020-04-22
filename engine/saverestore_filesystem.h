//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface for filesystem calls used by the saverestore system
//			to manupulate the save directory.
//
//=============================================================================//
#ifndef SAVERESTOREFILESYSTEM_H 
#define SAVERESTOREFILESYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "utlmap.h"
#include "utlsymbol.h"
#include "filesystem.h"

abstract_class ISaveRestoreFileSystem
{
public:
	virtual FileHandle_t	Open( const char *pFileName, const char *pOptions, const char *pathID = NULL ) = 0;
	virtual void			Close( FileHandle_t ) = 0;
	virtual int				Read( void *pOutput, int size, FileHandle_t file ) = 0;
	virtual int				Write( void const* pInput, int size, FileHandle_t file ) = 0;
	virtual void			Seek( FileHandle_t file, int pos, FileSystemSeek_t method ) = 0;
	virtual unsigned int	Tell( FileHandle_t file ) = 0;
	virtual unsigned int	Size( FileHandle_t file ) = 0;
	virtual unsigned int	Size( const char *pFileName, const char *pPathID = NULL ) = 0;

	virtual bool			FileExists( const char *pFileName, const char *pPathID = NULL ) = 0;
	virtual void			RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID = NULL ) = 0;
	virtual void			RemoveFile( char const* pRelativePath, const char *pathID = NULL ) = 0;

	virtual void			AsyncFinishAllWrites( void ) = 0;
	virtual void			AsyncRelease( FSAsyncControl_t hControl ) = 0;
	virtual FSAsyncStatus_t	AsyncWrite( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl = NULL ) = 0;
	virtual FSAsyncStatus_t	AsyncFinish( FSAsyncControl_t hControl, bool wait = false ) = 0;
	virtual FSAsyncStatus_t	AsyncAppend( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl = NULL ) = 0;
	virtual FSAsyncStatus_t	AsyncAppendFile( const char *pDestFileName, const char *pSrcFileName, FSAsyncControl_t *pControl = NULL ) = 0;

	virtual void			DirectoryCopy( const char *pPath, const char *pDestFileName, bool bIsXSave ) = 0;
	virtual	bool			DirectoryExtract( FileHandle_t pFile, int fileCount, bool bIsXSave ) = 0;
	virtual int				DirectoryCount( const char *pPath ) = 0;
	virtual void			DirectoryClear( const char *pPath, bool bIsXSave ) = 0;

	virtual void			AuditFiles( void ) = 0;
	virtual bool			LoadFileFromDisk( const char *pFilename ) = 0;
};

extern ISaveRestoreFileSystem *g_pSaveRestoreFileSystem;

#ifdef _X360
#define PREPARE_XSAVE_FILENAME_EX( iCtrlr, chBuffer, iBufferLen ) \
	XBX_MakeStorageContainerRoot( iCtrlr, XBX_USER_SAVES_CONTAINER_DRIVE, chBuffer, iBufferLen ); \
	Q_snprintf( chBuffer + Q_strlen( chBuffer ), iBufferLen - Q_strlen( chBuffer ), ":\\"
#elif defined (_PS3)
#define PREPARE_XSAVE_FILENAME_EX( iCtrlr, chBuffer, iBufferLen ) \
	chBuffer[0] = 0; \
	Q_snprintf( chBuffer + Q_strlen( chBuffer ), iBufferLen - Q_strlen( chBuffer ), "//PS3SAVE/"
#else
#define PREPARE_XSAVE_FILENAME_EX( iCtrlr, chBuffer, iBufferLen ) \
	chBuffer[0] = 0; \
	Q_snprintf( chBuffer + Q_strlen( chBuffer ), iBufferLen - Q_strlen( chBuffer ), "//mod/"
#endif
#define PREPARE_XSAVE_FILENAME( iCtrlr, chBuffer ) PREPARE_XSAVE_FILENAME_EX( iCtrlr, chBuffer, sizeof( chBuffer ) )




extern ConVar save_spew;

#define SaveMsg if ( !save_spew.GetBool() ) ; else Msg

#endif // SAVERESTOREFILESYSTEM_H

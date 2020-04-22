//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: Filesystem abstraction for CSaveRestore - allows for storing temp save files
//			either in memory or on disk.
//
//===========================================================================//

#ifdef _WIN32
#include "winerror.h"
#endif
#include "filesystem_engine.h"
#include "saverestore_filesystem.h"
#include "saverestore_filesystem_passthrough.h"
#include "host_saverestore.h"
#include "host.h"
#include "sys.h"
#include "tier1/utlbuffer.h"
#include "tier1/lzss.h"
#include "tier1/convar.h"
#include "ixboxsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define MOD_DIR "DEFAULT_WRITE_PATH"


//-----------------------------------------------------------------------------
// Purpose: Implementation to execute traditional save to disk behavior
//-----------------------------------------------------------------------------


CSaveRestoreFileSystemPassthrough::CSaveRestoreFileSystemPassthrough() :  m_iContainerOpens( 0 ) {}

bool CSaveRestoreFileSystemPassthrough::FileExists( const char *pFileName, const char *pPathID )
{
	return g_pFileSystem->FileExists( pFileName, pPathID );
}

void CSaveRestoreFileSystemPassthrough::RemoveFile( char const* pRelativePath, const char *pathID )
{
	g_pFileSystem->RemoveFile( pRelativePath, pathID );
}

void CSaveRestoreFileSystemPassthrough::RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID )
{
	g_pFileSystem->RenameFile( pOldPath, pNewPath, pathID );
}

void CSaveRestoreFileSystemPassthrough::AsyncFinishAllWrites( void )
{
	g_pFileSystem->AsyncFinishAllWrites();
}

FileHandle_t CSaveRestoreFileSystemPassthrough::Open( const char *pFullName, const char *pOptions, const char *pathID )
{
	return g_pFileSystem->OpenEx( pFullName, pOptions, FSOPEN_NEVERINPACK, pathID );
}

void CSaveRestoreFileSystemPassthrough::Close( FileHandle_t hSaveFile )
{
	g_pFileSystem->Close( hSaveFile );
}

int CSaveRestoreFileSystemPassthrough::Read( void *pOutput, int size, FileHandle_t hFile )
{
	return g_pFileSystem->Read( pOutput, size, hFile );
}

int CSaveRestoreFileSystemPassthrough::Write( void const* pInput, int size, FileHandle_t hFile )
{
	return g_pFileSystem->Write( pInput, size, hFile );
}

FSAsyncStatus_t CSaveRestoreFileSystemPassthrough::AsyncWrite( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl )
{
	SaveMsg( "AsyncWrite (%s/%d)...\n", pFileName, nSrcBytes );
	return g_pFileSystem->AsyncWrite( pFileName, pSrc, nSrcBytes, bFreeMemory, bAppend, pControl );
}

void CSaveRestoreFileSystemPassthrough::Seek( FileHandle_t hFile, int pos, FileSystemSeek_t method )
{
	g_pFileSystem->Seek( hFile, pos, method );
}

unsigned int CSaveRestoreFileSystemPassthrough::Tell( FileHandle_t hFile )
{
	return g_pFileSystem->Tell( hFile );
}

unsigned int CSaveRestoreFileSystemPassthrough::Size( FileHandle_t hFile )
{
	return g_pFileSystem->Size( hFile );
}

unsigned int CSaveRestoreFileSystemPassthrough::Size( const char *pFileName, const char *pPathID )
{
	return g_pFileSystem->Size( pFileName, pPathID );
}

FSAsyncStatus_t CSaveRestoreFileSystemPassthrough::AsyncFinish( FSAsyncControl_t hControl, bool wait )
{
	return g_pFileSystem->AsyncFinish( hControl, wait );
}

void CSaveRestoreFileSystemPassthrough::AsyncRelease( FSAsyncControl_t hControl )
{
	g_pFileSystem->AsyncRelease( hControl );
}

FSAsyncStatus_t CSaveRestoreFileSystemPassthrough::AsyncAppend(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl )
{
	return g_pFileSystem->AsyncAppend( pFileName, pSrc, nSrcBytes, bFreeMemory, pControl );
}

FSAsyncStatus_t CSaveRestoreFileSystemPassthrough::AsyncAppendFile(const char *pDestFileName, const char *pSrcFileName, FSAsyncControl_t *pControl )
{
	return g_pFileSystem->AsyncAppendFile( pDestFileName, pSrcFileName, pControl );
}

//-----------------------------------------------------------------------------
// Purpose: Copies the contents of the save directory into a single file
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystemPassthrough::DirectoryCopy( const char *pPath, const char *pDestFileName, bool bIsXSave )
{
	SaveMsg( "DirectoryCopy....\n");

	CUtlVector<filelistelem_t> list;

	// force the writes to finish before trying to get the size/existence of a file
	// @TODO: don't need this if retain sizes for files written earlier in process
	SaveMsg( "DirectoryCopy: AsyncFinishAllWrites\n");
	g_pFileSystem->AsyncFinishAllWrites();

	// build the directory list
	char basefindfn[ MAX_PATH ];
	const char *findfn = Sys_FindFirstEx(pPath, MOD_DIR, basefindfn, sizeof( basefindfn ) );
	while ( findfn )
	{
		int index = list.AddToTail();
		memset( list[index].szFileName, 0, sizeof(list[index].szFileName) );
		Q_strncpy( list[index].szFileName, findfn, sizeof(list[index].szFileName) );

		findfn = Sys_FindNext( basefindfn, sizeof( basefindfn ) );
	}
	Sys_FindClose();

	// write the list of files to the save file
	char szName[MAX_PATH];
	for ( int i = 0; i < list.Count(); i++ )
	{
		if ( !bIsXSave )
		{
			Q_snprintf( szName, sizeof( szName ), "%s%s", saverestore->GetSaveDir(), list[i].szFileName );
		}
		else
		{
			PREPARE_XSAVE_FILENAME( XBX_GetPrimaryUserId(), szName ) "%s", list[i].szFileName );
		}

		Q_FixSlashes( szName );

		int fileSize = g_pFileSystem->Size( szName );
		if ( fileSize )
		{
			Assert( sizeof(list[i].szFileName) == MAX_PATH );

			SaveMsg( "DirectoryCopy: AsyncAppend %s, %s\n", szName, pDestFileName );
			g_pFileSystem->AsyncAppend( pDestFileName, memcpy( new char[MAX_PATH], list[i].szFileName, MAX_PATH), MAX_PATH, true );		// Filename can only be as long as a map name + extension
			g_pFileSystem->AsyncAppend( pDestFileName, new int(fileSize), sizeof(int), true );
			g_pFileSystem->AsyncAppendFile( pDestFileName, szName );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Extracts all the files contained within pFile
//-----------------------------------------------------------------------------
bool CSaveRestoreFileSystemPassthrough::DirectoryExtract( FileHandle_t pFile, int fileCount, bool bIsXSave )
{
	int				fileSize;
	FileHandle_t	pCopy;
	char			szName[ MAX_PATH ], fileName[ MAX_PATH ];
	bool			success = true;

	for ( int i = 0; i < fileCount && success; i++ )
	{
		// Filename can only be as long as a map name + extension
		if ( g_pSaveRestoreFileSystem->Read( fileName, MAX_PATH, pFile ) != MAX_PATH )
			return false;

		if ( g_pSaveRestoreFileSystem->Read( &fileSize, sizeof(int), pFile ) != sizeof(int) )
			return false;

		if ( !fileSize )
			return false;

		if ( !bIsXSave )
		{
			Q_snprintf( szName, sizeof( szName ), "%s%s", saverestore->GetSaveDir(), fileName );
		}
		else
		{
			PREPARE_XSAVE_FILENAME( XBX_GetPrimaryUserId(), szName ) "%s", fileName );
		}

		Q_FixSlashes( szName );
		pCopy = g_pSaveRestoreFileSystem->Open( szName, "wb", MOD_DIR );
		if ( !pCopy )
			return false;
		success = FileCopy( pCopy, pFile, fileSize );
		g_pSaveRestoreFileSystem->Close( pCopy );
	}

	return success;
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of files in the specified filter
//-----------------------------------------------------------------------------
int CSaveRestoreFileSystemPassthrough::DirectoryCount( const char *pPath )
{
	int count = 0;
	const char *findfn = Sys_FindFirstEx( pPath, MOD_DIR, NULL, 0 );

	while ( findfn != NULL )
	{
		count++;
		findfn = Sys_FindNext(NULL, 0 );
	}
	Sys_FindClose();

	return count;
}

//-----------------------------------------------------------------------------
// Purpose: Clears the save directory of all temporary files (*.hl)
//-----------------------------------------------------------------------------
void CSaveRestoreFileSystemPassthrough::DirectoryClear( const char *pPath, bool bIsXSave )
{
	char const	*findfn;
	char		szPath[ MAX_PATH ];

	findfn = Sys_FindFirstEx( pPath, MOD_DIR, NULL, 0 );
	while ( findfn != NULL )
	{
		if ( !bIsXSave )
		{
			Q_snprintf( szPath, sizeof( szPath ), "%s%s", saverestore->GetSaveDir(), findfn );
		}
		else
		{
			PREPARE_XSAVE_FILENAME( XBX_GetPrimaryUserId(), szPath ) "%s", findfn );
		}

		// Delete the temporary save file
		g_pFileSystem->RemoveFile( szPath, MOD_DIR );

		// Any more save files
		findfn = Sys_FindNext( NULL, 0 );
	}
	Sys_FindClose();
}

void CSaveRestoreFileSystemPassthrough::AuditFiles( void )
{
	Msg("Not using save-in-memory path!\n" );
}

bool CSaveRestoreFileSystemPassthrough::LoadFileFromDisk( const char *pFilename )
{
	Msg("Not using save-in-memory path!\n" );
	return true;
}

bool CSaveRestoreFileSystemPassthrough::FileCopy( FileHandle_t pOutput, FileHandle_t pInput, int fileSize )
{
	// allocate a reasonably large file copy buffer, since otherwise write performance under steam suffers
	char	*buf = (char *)malloc(FILECOPYBUFSIZE);
	int		size;
	int		readSize;
	bool	success = true;

	while ( fileSize > 0 )
	{
		if ( fileSize > FILECOPYBUFSIZE )
			size = FILECOPYBUFSIZE;
		else
			size = fileSize;
		if ( ( readSize = g_pSaveRestoreFileSystem->Read( buf, size, pInput ) ) < size )
		{
			Warning( "Unexpected end of file expanding save game\n" );
			fileSize = 0;
			success = false;
			break;
		}
		g_pSaveRestoreFileSystem->Write( buf, readSize, pOutput );

		fileSize -= size;
	}

	free(buf);
	return success;
}
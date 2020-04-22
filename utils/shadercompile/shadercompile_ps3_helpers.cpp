//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Functionality to handle collation of shader debugging metadata 
// (i.e. shader PDBs) sent from shadercompile workers to the master.
//
//===============================================================================

#include "shadercompile_ps3_helpers.h"

#include <windows.h>
#include "utlsymbollarge.h"

// TOC file is a list of all files sent from worker to master and their locations
// in the giant 1 GB pack files
char g_PS3DebugTOCFilename[ MAX_PATH ];
HANDLE g_hPS3DebugTOCFile = INVALID_HANDLE_VALUE;

// Keep this in the range of a 32-bit integer, preferably a signed one for simplicity
const size_t MAX_PS3_DEBUG_INFO_PACK_FILE_SIZE = 1024 * 1024 * 1024;

// Max # of pack files to generate.  If we're exceeding this number, something is probably wrong.
const int MAX_PS3_DEBUG_INFO_PACK_FILE_COUNT = 64;

// Names of each of the pack files: ps3shaderdebug_packN for N = 0 to MAX_PS3_DEBUG_INFO_PACK_FILE_COUNT-1
char g_PS3DebugInfoPackFilenames[ MAX_PS3_DEBUG_INFO_PACK_FILE_COUNT ][ MAX_PATH ];

// Handle to the currently open pack file
HANDLE g_hPS3DebugInfoCurrentPackFile = INVALID_HANDLE_VALUE;
// Current size of the pack files
size_t g_nPS3DebugInfoPackFileSizes[ MAX_PS3_DEBUG_INFO_PACK_FILE_COUNT ];
// Index into the previous arrays for the current pack file we are operating on (valid during build-up phase)
int g_nCurrentPS3DebugInfoFile;

// Defined in shadercompile.cpp
extern const char *g_pShaderPath;

// The set of all filenames we have encountered so far
CUtlSymbolTableLarge_CI g_PS3DebugInfoFileSet;

int g_nDuplicatePS3DebugFileCount = 0;
int g_nTotalPS3DebugFileCount = 0;

//-----------------------------------------------------------------------------
// An entry in the Table-Of-Contents file which is built during compilation.
//
// Each entry corresponds to one tiny PS3 debug metadata file generated
// on a worker machine during Cg shader compile and sent back to the host.
//-----------------------------------------------------------------------------
struct PS3DebugFileTOCEntry_t
{
	// Length of destination filename, including NULL terminator (data follows immediately after this structure)
	int32 m_nFilenameLength;
	// Which of the debug info files this entry was stored in
	int32 m_nFileIndex;
	// Offset in ps3shaderdebug_pack%d.bin (where %d is m_nFileIndex)
	int32 m_nFileOffset;
	// Length of the debug file (usually on the order of kilobytes, so 32-bits is fine)
	int32 m_nFileSize;

	// Filename immediately follows:
	// char m_Filename[m_nFilenameLength]
};

// Appends a single tiny (few KB) file received from the worker to the end fo the current giant pack file.
// These will be expanded later after shader compilation is done.
static void WritePS3DebugInfo( const char *pFullPath, byte *pFileData, DWORD nFileSize )
{
	if ( g_nPS3DebugInfoPackFileSizes[ g_nCurrentPS3DebugInfoFile ] + nFileSize > MAX_PS3_DEBUG_INFO_PACK_FILE_SIZE )
	{
		// These files should be on the order of a few kilobytes, but let's just make sure nothing insane happens here
		Assert( nFileSize < MAX_PS3_DEBUG_INFO_PACK_FILE_SIZE ); 
		CloseHandle( g_hPS3DebugInfoCurrentPackFile );
		g_hPS3DebugInfoCurrentPackFile = INVALID_HANDLE_VALUE;
		++ g_nCurrentPS3DebugInfoFile;
	}
	if ( g_hPS3DebugInfoCurrentPackFile == INVALID_HANDLE_VALUE )
	{
		char filename[ MAX_PATH ];
		Q_snprintf( filename, MAX_PATH, "ps3shaderdebug_pack%02d.bin", g_nCurrentPS3DebugInfoFile );
		Q_ComposeFileName( g_pShaderPath, filename, g_PS3DebugInfoPackFilenames[ g_nCurrentPS3DebugInfoFile ], MAX_PATH );

		g_hPS3DebugInfoCurrentPackFile = CreateFile( g_PS3DebugInfoPackFilenames[ g_nCurrentPS3DebugInfoFile ], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );		
	}

	if ( g_hPS3DebugInfoCurrentPackFile == INVALID_HANDLE_VALUE )
	{
		Error( "Could not write to PS3 debug info file.  Make sure you have enough disk space, these things can be huuuge." );
		return;
	}

	DWORD nBytesWritten = 0;
	WriteFile( g_hPS3DebugInfoCurrentPackFile, pFileData, nFileSize, &nBytesWritten, NULL );
	if ( nBytesWritten != nFileSize )
	{
		Error( "Error writing to PS3 debug info file.  Make sure you have enough disk space, these things can be huuuge." );
		return;
	}

	PS3DebugFileTOCEntry_t tocEntry;
	tocEntry.m_nFilenameLength = Q_strlen( pFullPath ) + 1;
	tocEntry.m_nFileIndex = g_nCurrentPS3DebugInfoFile;
	tocEntry.m_nFileOffset = g_nPS3DebugInfoPackFileSizes[ g_nCurrentPS3DebugInfoFile ];
	tocEntry.m_nFileSize = nFileSize;

	g_nPS3DebugInfoPackFileSizes[ g_nCurrentPS3DebugInfoFile ] += nBytesWritten;

	WriteFile( g_hPS3DebugTOCFile, &tocEntry, sizeof( tocEntry ), &nBytesWritten, NULL );
	if ( nBytesWritten != sizeof ( tocEntry ) )
	{
		Error( "Error writing to PS3 debug TOC file.  Make sure you have enough disk space, these things can be huuuge." );
		return;
	}

	WriteFile( g_hPS3DebugTOCFile, pFullPath, tocEntry.m_nFilenameLength, &nBytesWritten, NULL );
	if ( nBytesWritten != ( DWORD )tocEntry.m_nFilenameLength )
	{
		Error( "Error writing to PS3 debug TOC file.  Make sure you have enough disk space, these things can be huuuge." );
		return;
	}
}

bool PS3ShaderDebugInfoDispatch( MessageBuffer *pBuf, int nSource, int nPacketID )
{
	// Received packet from worker containing list of files generated by PS3 CG compiler
	PS3ShaderDebugInfoPacket_t *pPacket = ( PS3ShaderDebugInfoPacket_t * )pBuf->data;
	const char *pFilename = ( char * )pBuf->data + sizeof( PS3ShaderDebugInfoPacket_t );

	++g_nTotalPS3DebugFileCount;

	if ( g_PS3DebugInfoFileSet.Find( pFilename ) == UTL_INVAL_SYMBOL_LARGE )
	{
		g_PS3DebugInfoFileSet.AddString( pFilename );

		// Re-create the file locally (beneath g_pShaderPath \cgc-capture), exactly as it was on the worker machine
		// by writing it into a giant bin file which will later be decompressed
		if ( Q_strnicmp( pFilename, "cgc-capture", 11 ) == 0 )
		{
			char fullPath[MAX_PATH];
			Q_ComposeFileName( g_pShaderPath, pFilename, fullPath, MAX_PATH );
			WritePS3DebugInfo( fullPath, ( byte * )pBuf->data + sizeof( PS3ShaderDebugInfoPacket_t ) + pPacket->m_nFileNameLength, pPacket->m_nFileDataLength );
		}	
	}
	else
	{
		++ g_nDuplicatePS3DebugFileCount;
	}

	return true;
}

void InitializePS3ShaderDebugPackFiles()
{
	// Create files for debug information being returned from workers
	Q_ComposeFileName( g_pShaderPath, "ps3shaderdebug_toc.bin", g_PS3DebugTOCFilename, MAX_PATH );
	g_hPS3DebugTOCFile = CreateFile( g_PS3DebugTOCFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
}

static void DisplayFileUnpackProgress( unsigned int nTotalSize, unsigned int nLast, unsigned int nCurrent )
{
	int nLastProgress = ( int )( 100.0f * ( double )nLast / ( double )nTotalSize );
	int nCurrentProgress = ( int )( 100.0f * ( double )nCurrent / ( double )nTotalSize );
	for ( int i = nLastProgress + 1; i <= nCurrentProgress; ++ i )
	{
		if ( i % 10 == 0 )
		{
			Msg( "%d", ( i / 10 ) );
		}
		else if ( i % 2 == 0 )
		{
			Msg( "." );
		}
	}
}

// Expand the giant ps3shaderdebug_toc.bin and ps3shaderdebug_packN.bin files into a directory tree of little files.
// Writing out these files takes too long to do in-line with the shader compile (the master gets bogged down with file IO requests otherwise)
void ExpandPS3DebugInfo()
{
	if ( g_hPS3DebugTOCFile != INVALID_HANDLE_VALUE )
	{
		Msg( "Unpacking giant shader debug info files into sub-directory tree.\n" );
		Msg( "0" );

		// Close the last files we were working on
		CloseHandle( g_hPS3DebugTOCFile );
		if ( g_hPS3DebugInfoCurrentPackFile != INVALID_HANDLE_VALUE )
		{
			CloseHandle( g_hPS3DebugInfoCurrentPackFile );
		}

		g_hPS3DebugTOCFile = CreateFile( g_PS3DebugTOCFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		DWORD nTocFileSizeHigh;
		DWORD nTocFileSize = GetFileSize( g_hPS3DebugTOCFile, &nTocFileSizeHigh );
		if ( nTocFileSizeHigh != 0 )
		{
			Error( "PS3 debug info TOC File is greater than 4 GB.  This is probably not a good thing." );
			return;
		}

		// A set of directories we have already created, so we know not to re-create them
		CUtlSymbolTableLarge_CI createdPS3DebugInfoDirectories;

		// Scratch space big enough to store any single sub-file, usually on the order of kilobytes
		CUtlVector< byte > scratchSpace;

		g_hPS3DebugInfoCurrentPackFile = INVALID_HANDLE_VALUE;

		DWORD nCurrentTocEntryOffset = 0;
		DWORD nCurrentDebugInfoOffset = 0;
		int nCurrentDebugInfoFile = -1;
		while ( nCurrentTocEntryOffset < nTocFileSize )
		{
			// There must be at least enough room for a TOC entry plus some string data afterwards
			Assert( nCurrentTocEntryOffset + sizeof( PS3DebugFileTOCEntry_t ) < nTocFileSize );

			DWORD nBytesRead = 0;
			PS3DebugFileTOCEntry_t tocEntry;
			ReadFile( g_hPS3DebugTOCFile, &tocEntry, sizeof( PS3DebugFileTOCEntry_t ), &nBytesRead, NULL );
			Assert( nBytesRead == sizeof( PS3DebugFileTOCEntry_t ) );

			char fileNameBuffer[ MAX_PATH ];
			Assert( tocEntry.m_nFilenameLength < MAX_PATH );
			ReadFile( g_hPS3DebugTOCFile, fileNameBuffer, MIN( tocEntry.m_nFilenameLength, MAX_PATH ), &nBytesRead, NULL );
			Assert( nBytesRead == (DWORD)tocEntry.m_nFilenameLength );

			// Create any necessary directories recursively
			char dirToCreate[MAX_PATH];
			Q_ExtractFilePath( fileNameBuffer, dirToCreate, MAX_PATH );
			Q_StripTrailingSlash( dirToCreate );

			if ( ( UtlSymLargeId_t )createdPS3DebugInfoDirectories.Find( dirToCreate ) == UTL_INVAL_SYMBOL_LARGE )
			{
				const char *pNextDir = strchr( fileNameBuffer, '\\' );
				while ( pNextDir != NULL )
				{
					size_t nCharsToCopy = pNextDir - fileNameBuffer;
					memcpy( dirToCreate, fileNameBuffer, nCharsToCopy );
					dirToCreate[nCharsToCopy] = '\0';
					if ( ( UtlSymLargeId_t )createdPS3DebugInfoDirectories.Find( dirToCreate ) == UTL_INVAL_SYMBOL_LARGE )
					{
						CreateDirectory( dirToCreate, NULL );
						createdPS3DebugInfoDirectories.AddString( dirToCreate );
					}
					pNextDir = strchr( pNextDir + 1, '\\' );
				}
			}

			// Read the data out of the corresponding debug info file
			if ( nCurrentDebugInfoFile != tocEntry.m_nFileIndex )
			{
				if ( g_hPS3DebugInfoCurrentPackFile != INVALID_HANDLE_VALUE )
				{
					Assert( nCurrentDebugInfoOffset == g_nPS3DebugInfoPackFileSizes[ nCurrentDebugInfoFile ] );
					CloseHandle( g_hPS3DebugInfoCurrentPackFile );
					DeleteFile( g_PS3DebugInfoPackFilenames[ nCurrentDebugInfoFile ] );
				}
				nCurrentDebugInfoOffset = 0;
				nCurrentDebugInfoFile = tocEntry.m_nFileIndex;
				g_hPS3DebugInfoCurrentPackFile = CreateFile( g_PS3DebugInfoPackFilenames[ nCurrentDebugInfoFile ], GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
			}

			Assert( nCurrentDebugInfoOffset == (DWORD)tocEntry.m_nFileOffset );
			scratchSpace.EnsureCount( tocEntry.m_nFileSize );
			ReadFile( g_hPS3DebugInfoCurrentPackFile, scratchSpace.Base(), tocEntry.m_nFileSize, &nBytesRead, NULL );
			Assert( nBytesRead == (DWORD)tocEntry.m_nFileSize );
			nCurrentDebugInfoOffset += nBytesRead;

			DWORD nBytesWritten = 0;
			HANDLE hNewFile = CreateFile( fileNameBuffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
			if ( hNewFile == INVALID_HANDLE_VALUE )
			{
				Error( "Unable to create PS3 shader debug info file: %s.  Ensure you have enoug disk space.\n", fileNameBuffer );
			}
			WriteFile( hNewFile, scratchSpace.Base(), tocEntry.m_nFileSize, &nBytesWritten, NULL );
			Assert( nBytesWritten == (DWORD)tocEntry.m_nFileSize );
			CloseHandle( hNewFile );

			DWORD nEntrySize = sizeof( PS3DebugFileTOCEntry_t ) + tocEntry.m_nFilenameLength;
			DisplayFileUnpackProgress( nTocFileSize, nCurrentTocEntryOffset, nCurrentTocEntryOffset + nEntrySize );
			nCurrentTocEntryOffset += nEntrySize;
		}

		if ( g_hPS3DebugInfoCurrentPackFile != INVALID_HANDLE_VALUE )
		{
			CloseHandle( g_hPS3DebugInfoCurrentPackFile );
			DeleteFile( g_PS3DebugInfoPackFilenames[ nCurrentDebugInfoFile ] );
		}

		CloseHandle( g_hPS3DebugTOCFile );
		DeleteFile( g_PS3DebugTOCFilename );
	}
	Msg( "\nTotal shader debug files returned: %d, duplicates: %d\n", g_nTotalPS3DebugFileCount, g_nDuplicatePS3DebugFileCount );
}


static void SendFileContentsToMaster( const char *pFilename )
{
	HANDLE fileHandle = CreateFile( pFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( fileHandle != INVALID_HANDLE_VALUE )
	{
		DWORD fileSize = GetFileSize( fileHandle, NULL );
		byte *pFileData = new byte[fileSize];
		DWORD bytesRead;
		ReadFile( fileHandle, pFileData, fileSize, &bytesRead, NULL );
		if ( bytesRead == fileSize )
		{
			PS3ShaderDebugInfoPacket_t filePacket;
			filePacket.m_PacketID = PS3_SHADER_DEBUG_INFO_PACKETID;
			filePacket.m_nFileNameLength = Q_strlen( pFilename ) + 1;
			filePacket.m_nFileDataLength = bytesRead;
			CUtlBuffer myBuffer;
			myBuffer.Put( &filePacket, sizeof( filePacket ) );
			myBuffer.Put( pFilename, filePacket.m_nFileNameLength );
			myBuffer.Put( pFileData, filePacket.m_nFileDataLength );
			VMPI_SendData( myBuffer.Base(), sizeof( filePacket ) + filePacket.m_nFileNameLength + filePacket.m_nFileDataLength, VMPI_MASTER_ID );
		}

		delete[] pFileData;
		CloseHandle( fileHandle );
		unlink( pFilename );
	}

	fopen( pFilename, "r" );
}

template< typename TFunctor >
static void ForEachFileRecursive( const char *pStartingPath, TFunctor callbackFunction )
{
	WIN32_FIND_DATA findFileData;
	char searchPath[MAX_PATH];
	Q_strncpy( searchPath, pStartingPath, MAX_PATH );
	Q_ComposeFileName( pStartingPath, "*.*", searchPath, MAX_PATH );
	HANDLE findHandle = FindFirstFile( searchPath, &findFileData );
	bool bKeepSearching = ( findHandle != INVALID_HANDLE_VALUE );
	while ( bKeepSearching )
	{
		if ( Q_stricmp( findFileData.cFileName, "." ) != 0 && Q_stricmp( findFileData.cFileName, ".." ) != 0 )
		{
			char fullFilePath[MAX_PATH];
			Q_ComposeFileName( pStartingPath, findFileData.cFileName, fullFilePath, MAX_PATH );
			printf( "Found file: %s\n\n", fullFilePath );
			if ( ( findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
			{
				ForEachFileRecursive( fullFilePath, callbackFunction );
			}
			else
			{
				callbackFunction( fullFilePath );
			}
		}

		bKeepSearching = !!FindNextFile( findHandle, &findFileData );
	}
	FindClose( findHandle );
}

void SendSubDirectoryToMaster( const char *pStartingPath )
{
	ForEachFileRecursive( pStartingPath, SendFileContentsToMaster );
}

static bool SendShaderCompileLogContentsToMaster( const char *pFilename )
{
	FILE *pFile = fopen( pFilename, "r" );
	if ( !pFile )
		return false;
	
	const uint nPacketBufSize = 8192;
	char packetBuf[nPacketBufSize];
	
	PS3ShaderCompileLogPacket_t &filePacket = *reinterpret_cast< PS3ShaderCompileLogPacket_t * >( &packetBuf );
	filePacket.m_PacketID = PS3_SHADER_COMPILE_LOG_PACKETID;
	filePacket.m_nPacketSize = 0;
	uint nCurBufSize = sizeof( filePacket );
	
	while ( !feof( pFile ) )
	{
		const uint nMaxLineSize = 512;

		char szLine[nMaxLineSize];
		if ( !fgets( szLine, nMaxLineSize, pFile ) )
			break;

		int nCurLineSize = V_strlen( szLine );

		V_memcpy( packetBuf + nCurBufSize, szLine, nCurLineSize );
		nCurBufSize += nCurLineSize;
		Assert( nCurBufSize <= nPacketBufSize );

		if ( ( nPacketBufSize - nCurBufSize ) < nMaxLineSize )
		{
			filePacket.m_nPacketSize = nCurBufSize;

			VMPI_SendData( &filePacket, nCurBufSize, VMPI_MASTER_ID );
			nCurBufSize = sizeof( filePacket );
		}
	}

	if ( nCurBufSize > sizeof( filePacket ) )
	{
		filePacket.m_nPacketSize = nCurBufSize;
		VMPI_SendData( &filePacket, nCurBufSize, VMPI_MASTER_ID );
	}

	fclose( pFile );

	return true;
}

void PS3SendShaderCompileLogContentsToMaster()
{
	char szLogFilename[MAX_PATH];
	if ( GetEnvironmentVariableA( "PS3COMPILELOG", szLogFilename, sizeof( szLogFilename ) ) )
	{
		HANDLE hMutex = CreateMutex( NULL, FALSE, "PS3COMPILELOGMUTEX" );
		if ( ( hMutex ) && ( WaitForSingleObject( hMutex, 10000 ) == WAIT_OBJECT_0 ) )
		{
			SendShaderCompileLogContentsToMaster( szLogFilename );

			_unlink( szLogFilename );

			ReleaseMutex( hMutex );
		}
	}
}

bool PS3ShaderCompileLogDispatch( MessageBuffer *pBuf, int nSource, int nPacketID )
{
	if ( pBuf->getLen() < sizeof( PS3ShaderCompileLogPacket_t ) )
		return false;

	PS3ShaderCompileLogPacket_t *pPacket = ( PS3ShaderCompileLogPacket_t * )pBuf->data;
	const uint8 *pData = ( const uint8 * )pBuf->data + sizeof( PS3ShaderCompileLogPacket_t );
	int nDataSize = pPacket->m_nPacketSize - sizeof( PS3ShaderCompileLogPacket_t );

	if ( ( pPacket->m_nPacketSize >= sizeof( PS3ShaderCompileLogPacket_t ) ) && ( pBuf->getLen() >= pPacket->m_nPacketSize ) )
	{
		FILE *pFile = fopen( "ps3compilelog.txt", "ab" );
		if ( pFile )
		{
			fwrite( pData, nDataSize, 1, pFile );
			
			fclose( pFile );
		}
	}

	return true;
}








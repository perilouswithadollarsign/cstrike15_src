//===== Copyright Valve Corporation, All rights reserved. ======//
//
// Purpose: Implementation to execute traditional save to disk behavior
//
//===========================================================================//

#ifndef SAVERESTORE_FILESYSTEM_PASSTHROUGH_H
#define SAVERESTORE_FILESYSTEM_PASSTHROUGH_H

//-----------------------------------------------------------------------------
// Purpose: Implementation to execute traditional save to disk behavior
//-----------------------------------------------------------------------------
class CSaveRestoreFileSystemPassthrough : public ISaveRestoreFileSystem
{
public:
	CSaveRestoreFileSystemPassthrough();
	
	bool FileExists( const char *pFileName, const char *pPathID );

	void RemoveFile( char const* pRelativePath, const char *pathID );

	void RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID );

	void AsyncFinishAllWrites( void );

	FileHandle_t Open( const char *pFullName, const char *pOptions, const char *pathID );

	void Close( FileHandle_t hSaveFile );

	int Read( void *pOutput, int size, FileHandle_t hFile );
	int Write( void const* pInput, int size, FileHandle_t hFile );

	FSAsyncStatus_t AsyncWrite( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl );

	void Seek( FileHandle_t hFile, int pos, FileSystemSeek_t method );

	unsigned int Tell( FileHandle_t hFile );

	unsigned int Size( FileHandle_t hFile );

	unsigned int Size( const char *pFileName, const char *pPathID );

	FSAsyncStatus_t AsyncFinish( FSAsyncControl_t hControl, bool wait );

	void AsyncRelease( FSAsyncControl_t hControl );

	FSAsyncStatus_t AsyncAppend(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl );

	FSAsyncStatus_t AsyncAppendFile(const char *pDestFileName, const char *pSrcFileName, FSAsyncControl_t *pControl );

	//-----------------------------------------------------------------------------
	// Purpose: Copies the contents of the save directory into a single file
	//-----------------------------------------------------------------------------
	void DirectoryCopy( const char *pPath, const char *pDestFileName, bool bIsXSave );

	//-----------------------------------------------------------------------------
	// Purpose: Extracts all the files contained within pFile
	//-----------------------------------------------------------------------------
	bool DirectoryExtract( FileHandle_t pFile, int fileCount, bool bIsXSave );

	//-----------------------------------------------------------------------------
	// Purpose: returns the number of files in the specified filter
	//-----------------------------------------------------------------------------
	int DirectoryCount( const char *pPath );

	//-----------------------------------------------------------------------------
	// Purpose: Clears the save directory of all temporary files (*.hl)
	//-----------------------------------------------------------------------------
	void DirectoryClear( const char *pPath, bool bIsXSave );

	void AuditFiles( void );

	bool LoadFileFromDisk( const char *pFilename );


	struct filelistelem_t
	{
		char szFileName[MAX_PATH];
	};

private:
	int m_iContainerOpens;
	enum { FILECOPYBUFSIZE = (1024 * 1024) };

	//-----------------------------------------------------------------------------
	// Purpose: Copy one file to another file
	//-----------------------------------------------------------------------------
	static bool FileCopy( FileHandle_t pOutput, FileHandle_t pInput, int fileSize );
};

#endif
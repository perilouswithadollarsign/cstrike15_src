//===== Copyright © 1996-2011, Valve Corporation, All rights reserved. ======//

#include "ps3_saveutil_v2.h"
#include "memdbgon.h"
#include <vjobs/jobparams_shared.h>
#include <vjobs/root.h>
#include <ps3/vjobutils.h>


class CSaveUtilV2Job_Load : public ISaveUtilV2Job
{
public:	// Job entry point
	virtual JobStatus_t DoExecute();

public: // Data passed from main thread
	char m_chFileName[VALVE_CONTAINER_FILENAME_LEN];
	char m_chFullPathOut[MAX_PATH];
	bool m_bForCloud;

public: // Data resolved from the main thread
	CSaveUtilV2ContainerTOC::TocEntry_t *m_pTocEntry;
	int m_nSubFileIndex;
	job_zlibinflate::JobDescriptor_t * m_pJobInflate;	

protected: // Data used for loading file contents
	CUtlBuffer m_bufScratch;
	int WriteFile( char const *szFile );

protected: // Stat callback
	virtual void DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS );

protected: // Load and write to disk
	void DoDataFile_LoadToBuffer( SONY_SAVEUTIL_FILE_PARAMS );
	void DoDataFile_WriteToDisk( SONY_SAVEUTIL_FILE_PARAMS );
};

//////////////////////////////////////////////////////////////////////////

void SaveUtilV2_Load( CPS3SaveRestoreAsyncStatus *pAsync, const char *pFilename, const char *pDestFullPath )
{
	if ( !SaveUtilV2_CanStartJob() )
		return;

	// Find the file that the caller wants
	int nSubFileIndex = -1;
	int k = g_SaveUtilV2TOC.FindByEmbeddedFileName( pFilename, &nSubFileIndex );
	if ( ( nSubFileIndex < 0 ) || ( k < 0 ) || ( k >= g_SaveUtilV2TOC.m_arrEntries.Count() ) )
	{
		pAsync->m_nSonyRetValue = CELL_SAVEDATA_ERROR_FAILURE;
		pAsync->m_bDone = 1;
		Warning( "ERROR: SaveUtilV2_Load: attempted to load file '%s' which doesn't exist in container!\n", pFilename );
		return;
	}

	// Start the job
	CSaveUtilV2Job_Load *pJob = new CSaveUtilV2Job_Load;
	V_strncpy( pJob->m_chFileName, pFilename, sizeof( pJob->m_chFileName ) );
	
	pJob->m_bForCloud = false;
	switch ( pDestFullPath[0] )
	{
	case '@':
		pJob->m_bForCloud = true;
		++ pDestFullPath;
		break;
	}
	
	V_strncpy( pJob->m_chFullPathOut, pDestFullPath, sizeof( pJob->m_chFullPathOut ) );
	pJob->m_pTocEntry = &g_SaveUtilV2TOC.m_arrEntries[k].m_entry;
	pJob->m_nSubFileIndex = nSubFileIndex;

	SaveUtilV2_EnqueueJob( pAsync, pJob );
}

//////////////////////////////////////////////////////////////////////////

JobStatus_t CSaveUtilV2Job_Load::DoExecute()
{
	float flTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Load @%.3f\n", flTimeStamp );

	// Allocate required buffer
	if ( m_bForCloud )
	{
		int numBytesRequired = sizeof( CSaveUtilV2ContainerTOC::TocStorageReserved_t );
		for ( int iPart = 0; iPart < VALVE_CONTAINER_FPARTS; ++ iPart )
			numBytesRequired += m_pTocEntry->m_numBytesFile[iPart];
		m_bufScratch.EnsureCapacity( numBytesRequired );
	}
	else
	{
		m_bufScratch.EnsureCapacity( m_pTocEntry->m_numBytesFile[m_nSubFileIndex] + m_pTocEntry->m_numBytesDecompressedFile[m_nSubFileIndex] );
	}

	m_pJobInflate = NewJob128( *g_saveUtilVjobInstance.m_pRoot->m_pJobZlibInflate );
	m_pJobInflate->header.sizeScratch = ( 16 * 1024 ) / 16 ;

	// Call saveutil
	int retv = cellSaveDataAutoSave2(
		CELL_SAVEDATA_VERSION_CURRENT,
		g_pszSaveUtilContainerName,
		CELL_SAVEDATA_ERRDIALOG_NONE,
		&m_SaveDirInfo,	
		csDataStatCallback,
		csDataFileCallback,
		SYS_MEMORY_CONTAINER_ID_INVALID,
		this );

	DeleteJob( m_pJobInflate );

	float flEndTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Load: cellSaveDataAutoSave2 returned %x @%.3f ( total time = %.3f sec )\n", retv, flEndTimeStamp, flEndTimeStamp - flTimeStamp );

	return SaveUtilV2_JobDone( retv );
}

void CSaveUtilV2Job_Load::DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS )
{
	Msg( "CSaveUtilV2Job_Load::DoDataStatCallback @%.3f\n", Plat_FloatTime() );

	SetDataFileCallback( &CSaveUtilV2Job_Load::DoDataFile_LoadToBuffer );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
}

void CSaveUtilV2Job_Load::DoDataFile_LoadToBuffer( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Load::DoDataFile_LoadToBuffer @%.3f\n", Plat_FloatTime() );

	// Load the file contents
	set->fileOperation = CELL_SAVEDATA_FILEOP_READ;
	set->fileName = m_pTocEntry->m_chContainerName;
	set->fileType = CELL_SAVEDATA_FILETYPE_SECUREFILE;
	memcpy( set->secureFileId, g_pszSaveUtilSecureFileId, CELL_SAVEDATA_SECUREFILEID_SIZE );
	set->reserved = NULL;

	set->fileOffset = CSaveUtilV2ContainerTOC::kStorageCapacity;
	if ( m_bForCloud )
	{
		set->fileSize = 0;
		for ( int iPart = 0; iPart < VALVE_CONTAINER_FPARTS; ++ iPart )
			set->fileSize += m_pTocEntry->m_numBytesFile[iPart];
		m_bufScratch.SeekPut( CUtlBuffer::SEEK_HEAD, sizeof( CSaveUtilV2ContainerTOC::TocStorageReserved_t ) + set->fileSize );
		set->fileBuf = ( ( uint8 * ) m_bufScratch.Base() ) + sizeof( CSaveUtilV2ContainerTOC::TocStorageReserved_t );
		set->fileBufSize = m_bufScratch.Size() - sizeof( CSaveUtilV2ContainerTOC::TocStorageReserved_t );
	}
	else
	{
		for ( int k = 0; k < m_nSubFileIndex; ++ k )
			set->fileOffset += m_pTocEntry->m_numBytesFile[k];
		set->fileSize = m_pTocEntry->m_numBytesFile[m_nSubFileIndex];
		m_bufScratch.SeekPut( CUtlBuffer::SEEK_HEAD, set->fileSize );
		set->fileBuf = m_bufScratch.Base();
		set->fileBufSize = m_bufScratch.Size();
	}

	// keep reading
	SetDataFileCallback( &CSaveUtilV2Job_Load::DoDataFile_WriteToDisk );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_Load::DoDataFile_LoadToBuffer will load %u bytes of '%s' from '%s'...\n", set->fileSize, m_chFileName, set->fileName );
}

void CSaveUtilV2Job_Load::DoDataFile_WriteToDisk( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Load::DoDataFile_WriteToDisk '%s' @%.3f\n", m_chFileName, Plat_FloatTime() );

	int ret = WriteFile( m_chFullPathOut );
	if ( ret < 0 )
	{
		Msg( "ERROR: CSaveUtilV2Job_Load::DoDataFile_WriteToDisk failed to write file to disk!\n" );
		g_pSaveUtilAsyncStatus->m_nSonyRetValue = CELL_SAVEDATA_ERROR_FAILURE;
		cbResult->result = CELL_SAVEDATA_CBRESULT_ERR_FAILURE;
		return;
	}

	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
}

int CSaveUtilV2Job_Load::WriteFile( char const *szFile )
{
	if ( !szFile || !*szFile )
		return 0;

	float flTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Load::WriteFile : %s @%.3f\n", szFile, flTimeStamp );

	unsigned char *pWriteData = ( unsigned char * ) m_bufScratch.Base();
	unsigned int numBytesWrite = m_pTocEntry->m_numBytesFile[m_nSubFileIndex]; // the compressed size
	if ( m_bForCloud )
	{
		numBytesWrite = m_bufScratch.TellPut();
		V_memcpy( pWriteData, m_pTocEntry, sizeof( CSaveUtilV2ContainerTOC::TocStorageReserved_t ) );

		//
		// Signature
		//

		// Generate sult into filename field
		CSaveUtilV2ContainerTOC::TocEntry_t *pSignature = ( CSaveUtilV2ContainerTOC::TocEntry_t * ) pWriteData;
		for ( int isult = 0; isult < sizeof( pSignature->m_chFile[0] ); ++ isult )
			pSignature->m_chFile[0][isult] = ( 1 + rand() ) % 220;

		// Put the version of our save header
		V_memset( pSignature->m_chContainerName, 0, sizeof( pSignature->m_chContainerName ) );
		pSignature->m_chContainerName[0] = 'S';
		pSignature->m_chContainerName[1] = 'A';
		pSignature->m_chContainerName[2] = 'V';
		pSignature->m_chContainerName[3] = '1';

		// Temporarily put our cryptokey in place of hash
		V_memcpy( pWriteData + 8, &g_uiSteamCloudCryptoKey, sizeof( g_uiSteamCloudCryptoKey ) );
		uint32 uiHash = SaveUtilV2_ComputeBufferHash( pWriteData, numBytesWrite );

		// Store the hash
		for ( int isult = 0; isult < sizeof( g_uiSteamCloudCryptoKey ) - sizeof( uiHash ); ++ isult )
			pWriteData[8 + isult] = ( 1 + rand() ) % 220;
		V_memcpy( pWriteData + 8 + sizeof( g_uiSteamCloudCryptoKey ) - sizeof( uiHash ), &uiHash, sizeof( uiHash ) );
	}
	else if ( m_pTocEntry->m_numBytesDecompressedFile[m_nSubFileIndex] )
	{
		// The file is actually compressed

		if( g_saveUtilVjobInstance.m_pRoot )
		{
			double flStartInflateJob = Plat_FloatTime();

			job_zlibinflate::JobParams_t * pJobParams = job_zlibinflate::GetJobParams( m_pJobInflate );

			pJobParams->m_eaUncompressedOutput = pWriteData + numBytesWrite;
			pJobParams->m_eaCompressed         = pWriteData;
			pJobParams->m_nCompressedSize      = numBytesWrite;
			pJobParams->m_nExpectedUncompressedSize = m_pTocEntry->m_numBytesDecompressedFile[m_nSubFileIndex];

			int nError = g_saveUtilVjobInstance.m_pRoot->m_queuePortSound.pushJob( &m_pJobInflate->header, sizeof( *m_pJobInflate ), 0, CELL_SPURS_JOBQUEUE_FLAG_SYNC_JOB );
			if( nError != CELL_OK )
			{
				Warning("job_zlibinflate failed to push through port, error 0x%X\n", nError );
				return -1;
			}

			while( !pJobParams->IsDone() )
			{
				ThreadSleep( 1 );
			}

			double flEndInflateJob = Plat_FloatTime();

			if( pJobParams->m_nError != 0 )
			{
				Warning( "CSaveUtilV2Job_Load::WriteFile failed to uncompress!\n" );
				return -1;
			}
			else
			{
				Msg( "job_zlibInflate took %.3f sec : %u -> %u KiB (%.2f MiB/s)\n", flEndInflateJob - flStartInflateJob, pJobParams->m_nCompressedSize/1024, pJobParams->m_nExpectedUncompressedSize/1024, pJobParams->m_nExpectedUncompressedSize / ( 1024 * 1024 * ( flEndInflateJob - flStartInflateJob ) ) );

				pWriteData += numBytesWrite;
				numBytesWrite = m_pTocEntry->m_numBytesDecompressedFile[m_nSubFileIndex];
			}
		}
		else
		{
			return -1;
		}
	}

	int ret;
	int fd;

	ret = cellFsOpen( szFile, CELL_FS_O_CREAT | CELL_FS_O_TRUNC | CELL_FS_O_WRONLY, &fd, NULL, 0 );
	if ( ret < 0 )
	{
		Msg( "ERROR: CSaveUtilV2Job_Load::DoDataFile_WriteToDisk : %s : cellFsOpen failed : %d\n", szFile, ret );
		return ret;
	}

	uint64_t numBytesActuallyWritten = 0;
	ret = cellFsWrite( fd, pWriteData, numBytesWrite, &numBytesActuallyWritten );
	cellFsClose( fd );
	if ( ret < 0 )
	{
		Msg( "ERROR: CSaveUtilV2Job_Load::DoDataFile_WriteToDisk : %s : cellFsWrite failed : %d\n", szFile, ret );
		return ret;
	}
	if ( numBytesActuallyWritten != numBytesWrite )
	{
		Msg( "ERROR: CSaveUtilV2Job_Load::DoDataFile_WriteToDisk : %s : cellFsWrite wrote incorrect file : %ull bytes written, %d bytes expected\n",
			szFile, numBytesActuallyWritten, numBytesWrite );
		return -1;
	}

	float flEndTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Load::WriteFile finished writing %s @%.3f (%.3f sec)\n", szFile, flEndTimeStamp, flEndTimeStamp - flTimeStamp );
	return 0;
}




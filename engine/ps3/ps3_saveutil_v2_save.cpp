//===== Copyright © 1996-2011, Valve Corporation, All rights reserved. ======//

#include "ps3_saveutil_v2.h"
#include "memdbgon.h"
#include <vjobs/jobparams_shared.h>
#include <vjobs/root.h>
#include <ps3/vjobutils.h>

ConVar ps3_saveutil2_compress( "ps3_saveutil2_compress", "1", FCVAR_DEVELOPMENTONLY );

class CCellFsFileDescriptorAutoClose
{
public:
	CCellFsFileDescriptorAutoClose( int fd = -1 ) : m_fd( fd ), m_pJobDeflate( NULL ) {}
	~CCellFsFileDescriptorAutoClose() { Close(); EndDeflate(); }
	void Close()
	{
		if ( m_fd != -1 )
		{
			cellFsClose( m_fd ); m_fd = -1;
		}
	}
	int m_fd;
	job_zlibdeflate::JobDescriptor_t * m_pJobDeflate;

	void BeginDeflate()
	{
		m_pJobDeflate = NewJob128( *g_saveUtilVjobInstance.m_pRoot->m_pJobZlibDeflate );
		m_pJobDeflate->header.sizeScratch = ( 32 * 1024 ) / 16 ;
		job_zlibdeflate::GetJobParams( m_pJobDeflate )->m_nStatus = 2; // status: the job isn't queued yet, so it's considered "done"
	}

	void EndDeflate()
	{
		if( m_pJobDeflate )
		{
			while( !job_zlibdeflate::GetJobParams( m_pJobDeflate )->IsDone() )
			{
				ThreadSleep( 1 );
			}
			DeleteJob( m_pJobDeflate );
			m_pJobDeflate = NULL;
		}
	}
};

class CSaveUtilV2Job_Save : public ISaveUtilV2Job
{
public:	// Job entry point
	virtual JobStatus_t DoExecute();

public: // Data passed from main thread
	char m_chComment[VALVE_CONTAINER_STRLEN];
	char m_chFile[VALVE_CONTAINER_FPARTS][VALVE_CONTAINER_STRLEN];
	int m_numAutoSavesCount;
	int m_numCloudFiles;

protected: // Buffer used for file data
	CSaveUtilV2ContainerTOC m_newTOC;
	CUtlBuffer m_bufFiles;

	CSaveUtilV2ContainerTOC::TocStorageReserved_t *m_pNewTOCEntry;
	struct OverwriteRequest_t
	{
		char m_chOverwriteContainerFile[VALVE_CONTAINER_8_3_LEN];
	};
	CUtlVector< OverwriteRequest_t > m_arrOverwriteRequests;
	OverwriteRequest_t m_owrDelete;

	CCellFsFileDescriptorAutoClose m_fd[VALVE_CONTAINER_FPARTS];
	int OpenAndStatFiles();
	int LoadAndCompressFileData();
	void PrepareToc();

protected: // Stat callback
	virtual void DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS );

protected: // Write the file and update TOC
	void DoDataFile_DeleteOldFile( SONY_SAVEUTIL_FILE_PARAMS );
	void DoDataFile_WriteFiles( SONY_SAVEUTIL_FILE_PARAMS );
	void DoDataFile_UpdateTOC( SONY_SAVEUTIL_FILE_PARAMS );
};

//////////////////////////////////////////////////////////////////////////

void SaveUtilV2_Write( CPS3SaveRestoreAsyncStatus *pAsync, const char *pSourcepath, const char *pScreenshotPath, const char *pComment )
{
	if ( !SaveUtilV2_CanStartJob() )
		return;

	// Start the job
	CSaveUtilV2Job_Save *pJob = new CSaveUtilV2Job_Save;
	V_strncpy( pJob->m_chComment, pComment ? pComment : "", sizeof( pJob->m_chComment ) );
	V_strncpy( pJob->m_chFile[0], pSourcepath ? pSourcepath : "", sizeof( pJob->m_chFile[0] ) );
	V_strncpy( pJob->m_chFile[1], pScreenshotPath ? pScreenshotPath : "", sizeof( pJob->m_chFile[1] ) );
	pJob->m_numAutoSavesCount = 0;
	pJob->m_numCloudFiles = 0;

	SaveUtilV2_EnqueueJob( pAsync, pJob );
}

void SaveUtilV2_WriteAutosave( CPS3SaveRestoreAsyncStatus *pAsync, 
							  const char *pSourcepath, // eg "/dev_hdd1/tempsave/autosave.ps3.sav"
							  const char *pComment, // the comment field for the new autosave.
							  const unsigned int nMaxNumAutosaves )
{
	if ( !SaveUtilV2_CanStartJob() )
		return;

	// Start the job
	CSaveUtilV2Job_Save *pJob = new CSaveUtilV2Job_Save;
	V_strncpy( pJob->m_chComment, pComment ? pComment : "", sizeof( pJob->m_chComment ) );
	V_strncpy( pJob->m_chFile[0], pSourcepath ? pSourcepath : "", sizeof( pJob->m_chFile[0] ) );
	V_strncpy( pJob->m_chFile[1], "", sizeof( pJob->m_chFile[1] ) );
	pJob->m_numAutoSavesCount = nMaxNumAutosaves;
	pJob->m_numCloudFiles = 0;

	SaveUtilV2_EnqueueJob( pAsync, pJob );
}

void SaveUtilV2_WriteCloudFile(  CPS3SaveRestoreAsyncStatus *pAsync, 
							   const char *pSourcepath, // eg "/dev_hdd1/tempsave/autosave.ps3.sav"
							   const unsigned int nMaxNumCloudFiles )
{
	if ( !SaveUtilV2_CanStartJob() )
		return;

	// Start the job
	CSaveUtilV2Job_Save *pJob = new CSaveUtilV2Job_Save;
	V_strncpy( pJob->m_chComment, "", sizeof( pJob->m_chComment ) );
	V_strncpy( pJob->m_chFile[0], pSourcepath ? pSourcepath : "", sizeof( pJob->m_chFile[0] ) );
	V_strncpy( pJob->m_chFile[1], "", sizeof( pJob->m_chFile[1] ) );
	pJob->m_numAutoSavesCount = 0;
	pJob->m_numCloudFiles = nMaxNumCloudFiles;

	SaveUtilV2_EnqueueJob( pAsync, pJob );
}

//////////////////////////////////////////////////////////////////////////

JobStatus_t CSaveUtilV2Job_Save::DoExecute()
{
	float flTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Save @%.3f\n", flTimeStamp );

	// Prepare new TOC and attempt to determine if the operation is write new or overwrite
	PrepareToc();

	// Fill out the rest of the TOC
	if ( OpenAndStatFiles() < 0 )
	{
		for ( int iPart = 0; iPart < VALVE_CONTAINER_FPARTS; ++ iPart )
			m_fd[iPart].Close();
		return SaveUtilV2_JobDone( CELL_SAVEDATA_ERROR_FAILURE );
	}

	// Allocate our buffer
	int nCapacityRequired = 0;
	for ( int iPart = 0; iPart < VALVE_CONTAINER_FPARTS; ++ iPart )
		nCapacityRequired += m_pNewTOCEntry->m_entry.m_numBytesFile[iPart];
	m_bufFiles.EnsureCapacity( CSaveUtilV2ContainerTOC::kStorageCapacity +
		( ( ps3_saveutil2_compress.GetBool() && ( m_numCloudFiles <= 0 ) ) ? 2 : 1 ) * nCapacityRequired );

	if ( ps3_saveutil2_compress.GetBool() && ( m_numCloudFiles <= 0 ) )
	{
		uint8 *pBaseRawData = ( ( uint8 * ) m_bufFiles.Base() ) + CSaveUtilV2ContainerTOC::kStorageCapacity;
		uint8 *pBaseOutData = pBaseRawData + nCapacityRequired;
		for ( int iPart = 0; iPart < VALVE_CONTAINER_FPARTS; ++ iPart )
		{
			if( m_fd[iPart].m_fd == -1 )
				continue;
			m_fd[iPart].BeginDeflate();
			uint64_t numBytesActuallyRead = 0, nUncompressedSize = m_pNewTOCEntry->m_entry.m_numBytesFile[iPart];
			// Read the file at the end of the buffer
			int ret = cellFsRead( m_fd[iPart].m_fd, pBaseRawData, nUncompressedSize, &numBytesActuallyRead );
			m_fd[iPart].Close();
			if ( ret < 0 || numBytesActuallyRead != nUncompressedSize )
				return SaveUtilV2_JobDone( CELL_SAVEDATA_ERROR_FAILURE );

			// Compress the file

			job_zlibdeflate::JobDescriptor_t * pJobDeflate = m_fd[iPart].m_pJobDeflate;
			job_zlibdeflate::JobParams_t * pJobParams = job_zlibdeflate::GetJobParams( pJobDeflate );
			pJobParams->m_eaInputUncompressedData  = pBaseRawData;
			pJobParams->m_eaOutputCompressedData   = pBaseOutData;
			pJobParams->m_nMaxCompressedOutputSize = nUncompressedSize;
			pJobParams->m_nUncompressedSize        = nUncompressedSize;

			pJobParams->m_nStatus = 0; // get ready to push the job
			int nError = g_saveUtilVjobInstance.m_pRoot->m_queuePortSound.pushJob( &pJobDeflate->header, sizeof( *pJobDeflate ), 0, CELL_SPURS_JOBQUEUE_FLAG_SYNC_JOB );
			if( nError != CELL_OK )
			{
				pJobParams->m_nStatus = 3; // done
				pJobParams->m_nError = 1; // error
				Warning( "Cannot push zlib job, ERROR 0x%X\n", nError );
			}

			pBaseRawData += nUncompressedSize;
			pBaseOutData += nUncompressedSize;
		}
		Assert( pBaseOutData <= ((uint8*)m_bufFiles.Base())+m_bufFiles.Size() );
	}

	// Call saveutil
	int retv = cellSaveDataAutoSave2(
		CELL_SAVEDATA_VERSION_CURRENT,
		g_pszSaveUtilContainerName,
		// autosaves report PS3 system dialog-errors
		(m_numAutoSavesCount||g_pSaveUtilAsyncStatus->m_bUseSystemDialogs) ? CELL_SAVEDATA_ERRDIALOG_ALWAYS : CELL_SAVEDATA_ERRDIALOG_NONE,
		&m_SaveDirInfo,	
		csDataStatCallback,
		csDataFileCallback,
		SYS_MEMORY_CONTAINER_ID_INVALID,
		this );

	for ( int iPart = 0; iPart < VALVE_CONTAINER_FPARTS; ++ iPart )
		m_fd[iPart].EndDeflate();

	float flEndTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Save: cellSaveDataAutoSave2 returned %x @%.3f ( total time = %.3f sec )\n", retv, flEndTimeStamp, flEndTimeStamp - flTimeStamp );

	// Close the file handles before returning so that we didn't hold the files locked
	// in case main thread resumes after job is done and tries to re-use the files
	for ( int iPart = 0; iPart < VALVE_CONTAINER_FPARTS; ++ iPart )
		m_fd[iPart].Close();
	++ g_SaveUtilV2TOCVersion;
	return SaveUtilV2_JobDone( retv );
}

void CSaveUtilV2Job_Save::DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS )
{
	Msg( "CSaveUtilV2Job_Save::DoDataStatCallback @%.3f\n", Plat_FloatTime() );

	// TODO: investigate how to move delete after the write and maintain
	// consistent transactional state of TOC
	SetDataFileCallback( &CSaveUtilV2Job_Save::DoDataFile_DeleteOldFile );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
}

void CSaveUtilV2Job_Save::DoDataFile_DeleteOldFile( SONY_SAVEUTIL_FILE_PARAMS )
{
	if ( !m_arrOverwriteRequests.Count() )
	{
		DoDataFile_WriteFiles( SONY_SAVEUTIL_PARAMS );
		return;
	}
	m_owrDelete = m_arrOverwriteRequests[m_arrOverwriteRequests.Count() - 1];
	m_arrOverwriteRequests.SetCountNonDestructively( m_arrOverwriteRequests.Count() - 1 );

	Msg( "CSaveUtilV2Job_Save::DoDataFile_DeleteOldFile @%.3f\n", Plat_FloatTime() );

	// Perform the delete
	set->fileOperation = CELL_SAVEDATA_FILEOP_DELETE;
	set->fileBuf = NULL;
	set->fileSize = set->fileBufSize = 0;
	set->fileName = m_owrDelete.m_chOverwriteContainerFile;
	set->fileOffset = 0;
	set->fileType = CELL_SAVEDATA_FILETYPE_SECUREFILE;
	memcpy( set->secureFileId, g_pszSaveUtilSecureFileId, CELL_SAVEDATA_SECUREFILEID_SIZE );
	set->reserved = NULL;

	SetDataFileCallback( &CSaveUtilV2Job_Save::DoDataFile_DeleteOldFile );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_Save::DoDataFile_DeleteOldFile will delete '%s'...\n", set->fileName );
}

void CSaveUtilV2Job_Save::DoDataFile_WriteFiles( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Save::DoDataFile_WriteFiles @%.3f\n", Plat_FloatTime() );

	// Obtain the files data required to be written
	if ( LoadAndCompressFileData() < 0 )
	{
		Msg( "ERROR: CSaveUtilV2Job_Save::DoDataFile_WriteFiles failed to load file!\n" );
		g_pSaveUtilAsyncStatus->m_nSonyRetValue = CELL_SAVEDATA_ERROR_FAILURE;
		cbResult->result = CELL_SAVEDATA_CBRESULT_ERR_FAILURE;
		return;
	}

	// Perform the write
	set->fileOperation = CELL_SAVEDATA_FILEOP_WRITE;
	set->fileBuf = m_bufFiles.Base();
	set->fileSize = set->fileBufSize = m_bufFiles.TellPut();
	set->fileName = m_pNewTOCEntry->m_entry.m_chContainerName;
	set->fileOffset = 0;
	set->fileType = CELL_SAVEDATA_FILETYPE_SECUREFILE;
	memcpy( set->secureFileId, g_pszSaveUtilSecureFileId, CELL_SAVEDATA_SECUREFILEID_SIZE );
	set->reserved = NULL;

	// update our TOC after the write succeeds
	SetDataFileCallback( &CSaveUtilV2Job_Save::DoDataFile_UpdateTOC );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_Save::DoDataFile_WriteFiles will write %d bytes...\n", set->fileSize );
}

void CSaveUtilV2Job_Save::DoDataFile_UpdateTOC( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Save::DoDataFile_UpdateTOC @%.3f\n", Plat_FloatTime() );

	// Update TOC since we successfully wrote the file
	{
		AUTO_LOCK( g_SaveUtilV2TOC.m_mtx );
		// Memory has been pre-reserved for the larger number of entries
		m_newTOC.CopyInto( &g_SaveUtilV2TOC );
	}

	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
}

int CSaveUtilV2Job_Save::OpenAndStatFiles()
{
	float flTimeStamp = Plat_FloatTime();

	if ( m_numCloudFiles > 0 )
	{
		// Cloud save
		int ret = cellFsOpen( m_chFile[0], CELL_FS_O_RDONLY, &m_fd[0].m_fd, NULL, 0 );
		if ( ret < 0 )
			return ret;
		CellFsStat cfs;
		ret = cellFsFstat( m_fd[0].m_fd, &cfs );
		if ( ret < 0 )
			return ret;
		if ( cfs.st_size <= sizeof( CSaveUtilV2ContainerTOC::TocStorageReserved_t ) )
			return -1;
		m_pNewTOCEntry->m_entry.m_numBytesFile[0] = cfs.st_size;
	}
	else
	{
		// Non-cloud save
		Q_strncpy( m_pNewTOCEntry->m_entry.m_chComment, m_chComment, sizeof( m_pNewTOCEntry->m_entry.m_chComment ) );
		m_pNewTOCEntry->m_entry.m_timeModification = time( NULL );
		for ( int iPart = 0; iPart < VALVE_CONTAINER_FPARTS; ++ iPart )
		{
			if ( !m_chFile[iPart][0] )
				continue;

			Q_strncpy( m_pNewTOCEntry->m_entry.m_chFile[iPart], V_GetFileName( m_chFile[iPart] ), VALVE_CONTAINER_FILENAME_LEN );
			int ret = cellFsOpen( m_chFile[iPart], CELL_FS_O_RDONLY, &m_fd[iPart].m_fd, NULL, 0 );
			if ( ret < 0 )
				return ret;
			CellFsStat cfs;
			ret = cellFsFstat( m_fd[iPart].m_fd, &cfs );
			if ( ret < 0 )
				return ret;
			m_pNewTOCEntry->m_entry.m_numBytesFile[iPart] = cfs.st_size;
		}
	}
	
	float flEndTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Save::OpenAndStatFiles took %.3f sec [ %u + %u bytes ]\n",
		flEndTimeStamp - flTimeStamp, m_pNewTOCEntry->m_entry.m_numBytesFile[0], m_pNewTOCEntry->m_entry.m_numBytesFile[1] );
	
	return 0;
}

int CSaveUtilV2Job_Save::LoadAndCompressFileData()
{
	// Leave room for TOC
	m_bufFiles.SeekPut( CUtlBuffer::SEEK_HEAD, CSaveUtilV2ContainerTOC::kStorageCapacity );

	float flTimeStamp = Plat_FloatTime();
	Assert( m_bufFiles.TellPut() == CSaveUtilV2ContainerTOC::kStorageCapacity ) ;
	uint8 *pBaseData = ( ( uint8 * ) m_bufFiles.Base() ) + CSaveUtilV2ContainerTOC::kStorageCapacity;

	if ( m_numCloudFiles <= 0 )
	{
		for ( int iPart = 0; iPart < VALVE_CONTAINER_FPARTS; ++ iPart )
		{
			if ( ps3_saveutil2_compress.GetBool() )
			{
				job_zlibdeflate::JobDescriptor_t * pJobDeflate = m_fd[iPart].m_pJobDeflate;
				if( !pJobDeflate )
					continue;

				job_zlibdeflate::JobParams_t * pJobParams = job_zlibdeflate::GetJobParams( pJobDeflate );
				uint nStallCount = 0;
				while( !pJobParams->IsDone() )
				{
					ThreadSleep( 1 );
					++nStallCount;
				}
				uint numCompressedBytes = pJobParams->m_nCompressedSizeOut & 0x7FFFFFFF;
				Msg( "job_zlibDeflate stalled ~%u ms : %u -> %u KiB\n", nStallCount, pJobParams->m_nUncompressedSize / 1024, ( numCompressedBytes & 0x7FFFFFFF ) / 1024 );

				if ( pJobParams->m_nError == 0 && ( pJobParams->m_nCompressedSizeOut & 0x80000000 ) && numCompressedBytes < m_pNewTOCEntry->m_entry.m_numBytesFile[iPart] ) // we actually have deflated data
				{
					// file compressed successfully
					// remove MSB
					m_pNewTOCEntry->m_entry.m_numBytesDecompressedFile[iPart] = m_pNewTOCEntry->m_entry.m_numBytesFile[iPart];
					m_pNewTOCEntry->m_entry.m_numBytesFile[iPart] = numCompressedBytes;
					V_memcpy( pBaseData, pJobParams->m_eaOutputCompressedData, numCompressedBytes );
					Msg( "CSaveUtilV2Job_Save::LoadAndCompressFileData compresses '%s' %d/%d bytes\n", m_pNewTOCEntry->m_entry.m_chFile[iPart], m_pNewTOCEntry->m_entry.m_numBytesFile[iPart], m_pNewTOCEntry->m_entry.m_numBytesDecompressedFile[iPart] );
				}
				else
				{
					// there was an error during compression; use uncompressed data
					m_pNewTOCEntry->m_entry.m_numBytesDecompressedFile[iPart] = 0;
					if( pBaseData != pJobParams->m_eaInputUncompressedData )
					{
						V_memmove( pBaseData, pJobParams->m_eaInputUncompressedData, m_pNewTOCEntry->m_entry.m_numBytesFile[iPart] );
					}
				}
				pBaseData += m_pNewTOCEntry->m_entry.m_numBytesFile[iPart];
			}
			else
			{
				if ( m_fd[iPart].m_fd == -1 )
					continue;

				uint64 numBytesActuallyRead;
				int ret = cellFsRead( m_fd[iPart].m_fd, ( ( uint8 * )m_bufFiles.Base() ) + m_bufFiles.TellPut(), m_bufFiles.Size() - m_bufFiles.TellPut(), &numBytesActuallyRead );
				m_fd[iPart].Close();
				if ( ret < 0 )
					return ret;

				if ( numBytesActuallyRead != m_pNewTOCEntry->m_entry.m_numBytesFile[iPart] )
					return -1;

			}
			m_bufFiles.SeekPut( CUtlBuffer::SEEK_HEAD, m_bufFiles.TellPut() + m_pNewTOCEntry->m_entry.m_numBytesFile[iPart] );
		}
	}
	else
	{
		if ( m_fd[0].m_fd == -1 )
			return -1;

		uint64 numBytesActuallyRead;
		m_bufFiles.SeekPut( CUtlBuffer::SEEK_HEAD, CSaveUtilV2ContainerTOC::kStorageCapacity - sizeof( CSaveUtilV2ContainerTOC::TocStorageReserved_t ) );
		char unsigned *pbIncomingFileBase = ( ( char unsigned * ) m_bufFiles.Base() ) + m_bufFiles.TellPut();
		int ret = cellFsRead( m_fd[0].m_fd, pbIncomingFileBase, m_bufFiles.Size() - m_bufFiles.TellPut(), &numBytesActuallyRead );
		m_fd[0].Close();
		if ( ret < 0 )
			return ret;

		if ( numBytesActuallyRead != m_pNewTOCEntry->m_entry.m_numBytesFile[0] )
			return -1;

		m_bufFiles.SeekPut( CUtlBuffer::SEEK_HEAD, m_bufFiles.TellPut() + m_pNewTOCEntry->m_entry.m_numBytesFile[0] );

		//
		// Signature
		//

		// Version of our save header
		CSaveUtilV2ContainerTOC::TocEntry_t *pSignature = (CSaveUtilV2ContainerTOC::TocEntry_t *) pbIncomingFileBase;
		if ( pSignature->m_chContainerName[0] != 'S' ||
			 pSignature->m_chContainerName[1] != 'A' ||
			 pSignature->m_chContainerName[2] != 'V' ||
			 pSignature->m_chContainerName[3] != '1' )
		{
			Msg( "ERROR: CSaveUtilV2Job_Save : header mismatch, expecting SAV1\n" );
			return -2; // header mismatch
		}

		// Fetch the current hash
		uint32 uiFileHashCurrent = 0, uiHash = 0;
		V_memcpy( &uiFileHashCurrent, ( (char*) pSignature ) + 8 + sizeof( g_uiSteamCloudCryptoKey ) - sizeof( uiHash ), sizeof( uiHash ) );

		// Temporarily put our cryptokey in place of hash
		V_memcpy( ( (char*) pSignature ) + 8, &g_uiSteamCloudCryptoKey, sizeof( g_uiSteamCloudCryptoKey ) );
		uiHash = SaveUtilV2_ComputeBufferHash( pSignature, m_pNewTOCEntry->m_entry.m_numBytesFile[0] );
		if ( uiHash != uiFileHashCurrent )
		{
			Msg( "ERROR: CSaveUtilV2Job_Save : signature hash mismatch\n" );
			return -3;
		}

		// We only need to preserve container name
		char chContainerName[sizeof( m_pNewTOCEntry->m_entry.m_chContainerName )];
		V_memcpy( chContainerName, m_pNewTOCEntry->m_entry.m_chContainerName, sizeof( m_pNewTOCEntry->m_entry.m_chContainerName ) );
		V_memcpy( m_pNewTOCEntry, pbIncomingFileBase, sizeof( CSaveUtilV2ContainerTOC::TocStorageReserved_t ) );
		V_memcpy( m_pNewTOCEntry->m_entry.m_chContainerName, chContainerName, sizeof( m_pNewTOCEntry->m_entry.m_chContainerName ) );
		V_memset( m_pNewTOCEntry->m_entry.m_chFile[0], 0, sizeof( m_pNewTOCEntry->m_entry.m_chFile[0] ) );
		V_snprintf( m_pNewTOCEntry->m_entry.m_chFile[0], sizeof( m_pNewTOCEntry->m_entry.m_chFile[0] ), "cloudsave%016llx.ps3.sav", m_pNewTOCEntry->m_entry.m_timeModification );
		if ( *m_pNewTOCEntry->m_entry.m_chFile[1] )
			V_snprintf( m_pNewTOCEntry->m_entry.m_chFile[1], sizeof( m_pNewTOCEntry->m_entry.m_chFile[1] ), "cloudsave%016llx.ps3.tga", m_pNewTOCEntry->m_entry.m_timeModification );
	}

	float flEndTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Save::LoadFileToBuffer finished loading%s @%.3f (%.3f sec)\n", ps3_saveutil2_compress.GetBool() ? " and compressing" : "",
		flEndTimeStamp, flEndTimeStamp - flTimeStamp );

	// New TOC is fully ready, serialize it for writing
	m_newTOC.SerializeIntoTocBuffer( m_bufFiles.Base() );
	return 0;
}

static int TocAutosavesSortFunc( CSaveUtilV2ContainerTOC::TocStorageReserved_t * const *a, CSaveUtilV2ContainerTOC::TocStorageReserved_t * const *b )
{
	if ( (*a)->m_entry.m_timeModification != (*b)->m_entry.m_timeModification )
		return ( (*a)->m_entry.m_timeModification < (*b)->m_entry.m_timeModification ) ? -1 : 1;
	else
		return V_stricmp( (*a)->m_entry.m_chContainerName, (*b)->m_entry.m_chContainerName );
}

void CSaveUtilV2Job_Save::PrepareToc()
{
	{
		AUTO_LOCK( g_SaveUtilV2TOC.m_mtx );
		m_newTOC.m_arrEntries.EnsureCapacity( g_SaveUtilV2TOC.m_arrEntries.Count() + 1 );
		g_SaveUtilV2TOC.CopyInto( &m_newTOC );

		// Assuming all goes well, make room in the global TOC (need to do this now because we MUST NOT allocate inside the callback!):
		g_SaveUtilV2TOC.m_arrEntries.EnsureCapacity( g_SaveUtilV2TOC.m_arrEntries.Count() + 1 );
	}

	// PrepareToc is running before callback so can use memory allocations
	int k = m_newTOC.FindByEmbeddedFileName( V_GetFileName( m_chFile[0] ), NULL );
	if ( ( k < 0 ) || ( k >= m_newTOC.m_arrEntries.Count() ) || ( m_numCloudFiles > 0 ) )
	{
		// AUTOSAVE NOTE: Even if this is an autosave, if there's no other autosave.sav file
		// in the container then we can freely write the the new file, since the total number
		// of autosaves will be below the threshold
		// CLOUD NOTE: All cloud file writes are controlled by cloud sync manager
		// when cloud sync manager requests a write it will always be a new file

		m_pNewTOCEntry = &m_newTOC.m_arrEntries[ m_newTOC.m_arrEntries.AddToTail() ];
	}
	else if ( m_numAutoSavesCount <= 0 )
	{
		// It's a regular save, not autosave, overwrite the file inside container
		m_pNewTOCEntry = &m_newTOC.m_arrEntries[ k ];
		OverwriteRequest_t owr;
		V_strncpy( owr.m_chOverwriteContainerFile, m_pNewTOCEntry->m_entry.m_chContainerName, sizeof( owr.m_chOverwriteContainerFile ) );
		m_arrOverwriteRequests.AddToTail( owr );
		Msg( "CSaveUtilV2Job_Save will overwrite existing file '%s'\n", m_pNewTOCEntry->m_entry.m_chContainerName );
	}
	else // AUTOSAVE CASE
	{
		// It's an autosave and should overwrite the oldest autosave in this case
		// other autosaves must be aged and renamed
		char const *szSaveFileStringSearch = "autosave";
		int numPreserve = MAX( m_numAutoSavesCount, m_numCloudFiles );
		CUtlVector< CSaveUtilV2ContainerTOC::TocStorageReserved_t * > arrAutoSaves;
		arrAutoSaves.EnsureCapacity( numPreserve + 3 );
		for ( int ii = 0; ii < m_newTOC.m_arrEntries.Count(); ++ ii )
		{
			if ( V_stristr( m_newTOC.m_arrEntries[ii].m_entry.m_chFile[0], szSaveFileStringSearch ) )
				arrAutoSaves.AddToTail( &m_newTOC.m_arrEntries[ii] );
		}
		arrAutoSaves.Sort( TocAutosavesSortFunc );

		// Now we have a sorted list of autosaves, first element is oldest, last element is newest
		// the list is guaranteed non-empty, otherwise we would be creating new file altogether
		// Walk the list backwards and rename the autosaves appropriately
		int nAutosaveNameIndex = 1;
		for ( int ii = arrAutoSaves.Count(); ii-- > 0; ++ nAutosaveNameIndex )
		{
			V_snprintf( arrAutoSaves[ii]->m_entry.m_chFile[0], sizeof( arrAutoSaves[ii]->m_entry.m_chFile[0] ),
				"%s%02d.ps3.sav", szSaveFileStringSearch, nAutosaveNameIndex );
			if ( *arrAutoSaves[ii]->m_entry.m_chFile[1] )
			{
				V_snprintf( arrAutoSaves[ii]->m_entry.m_chFile[1], sizeof( arrAutoSaves[ii]->m_entry.m_chFile[1] ),
					"%s%02d.ps3.tga", szSaveFileStringSearch, nAutosaveNameIndex );
			}
		}

		// Now if the list of autosaves hasn't yet reached the max number, then just create a new file now
		if ( arrAutoSaves.Count() <= numPreserve )
		{
			// Generate filename to make inside container
			m_pNewTOCEntry = &m_newTOC.m_arrEntries[ m_newTOC.m_arrEntries.AddToTail() ];
			Msg( "CSaveUtilV2Job_Save will create new %s, %d existing %ss renamed\n",
				szSaveFileStringSearch, arrAutoSaves.Count(), szSaveFileStringSearch );
		}
		else
		{
			// Overwrite the oldest autosave (the TOC entry is updated, and the old container file will be deleted)
			m_pNewTOCEntry = arrAutoSaves[0];
			OverwriteRequest_t owr;
			V_strncpy( owr.m_chOverwriteContainerFile, m_pNewTOCEntry->m_entry.m_chContainerName, sizeof( owr.m_chOverwriteContainerFile ) );
			m_arrOverwriteRequests.AddToTail( owr );
			Msg( "CSaveUtilV2Job_Save will overwrite oldest %s '%s', %d other existing %ss preserved and renamed\n",
				szSaveFileStringSearch, m_pNewTOCEntry->m_entry.m_chContainerName, arrAutoSaves.Count() - 1, szSaveFileStringSearch );
		}
	}

	// Prepare the new/updated TOC entry
	Q_memset( m_pNewTOCEntry, 0, sizeof( CSaveUtilV2ContainerTOC::TocStorageReserved_t ) );
	int idxContainerIndex = ++ m_newTOC.m_idxNewSaveName;
	V_snprintf( m_pNewTOCEntry->m_entry.m_chContainerName, sizeof( m_pNewTOCEntry->m_entry.m_chContainerName ), "%08X.SAV", idxContainerIndex );
	Msg( "CSaveUtilV2Job_Save will create new file '%s'\n", m_pNewTOCEntry->m_entry.m_chContainerName );
}



//===== Copyright © 1996-2011, Valve Corporation, All rights reserved. ======//

#include "ps3_saveutil_v2.h"
#include "memdbgon.h"

class CSaveUtilV2Job_Delete : public ISaveUtilV2Job
{
public:	// Job entry point
	virtual JobStatus_t DoExecute();

public: // Data resolved from the main thread
	CSaveUtilV2ContainerTOC::TocEntry_t *m_pTocEntry;
	int m_nTocEntryIndex;

protected: // Stat callback
	virtual void DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS );

protected: // Delete the file and update TOC
	void DoDataFile_Delete( SONY_SAVEUTIL_FILE_PARAMS );
	void DoDataFile_UpdateTOC( SONY_SAVEUTIL_FILE_PARAMS );
};

//////////////////////////////////////////////////////////////////////////

void SaveUtilV2_Delete( CPS3SaveRestoreAsyncStatus *pAsync, const char *pFilename )
{
	if ( !SaveUtilV2_CanStartJob() )
		return;

	// Find the file that the caller wants
	int k = g_SaveUtilV2TOC.FindByEmbeddedFileName( pFilename, NULL );
	if ( ( k < 0 ) || ( k >= g_SaveUtilV2TOC.m_arrEntries.Count() ) )
	{
		pAsync->m_nSonyRetValue = CELL_SAVEDATA_ERROR_FAILURE;
		pAsync->m_bDone = 1;
		Warning( "ERROR: SaveUtilV2_Delete: attempted to delete file '%s' which doesn't exist in container!\n", pFilename );
		return;
	}

	// Start the job
	CSaveUtilV2Job_Delete *pJob = new CSaveUtilV2Job_Delete;
	// It is safe to hold this pointer into the TOC for the duration of this job
	// Only jobs update TOC and this is the current job, main thread accesses TOC for read only
	pJob->m_pTocEntry = &g_SaveUtilV2TOC.m_arrEntries[k].m_entry;
	pJob->m_nTocEntryIndex = k;

	SaveUtilV2_EnqueueJob( pAsync, pJob );
}

//////////////////////////////////////////////////////////////////////////

JobStatus_t CSaveUtilV2Job_Delete::DoExecute()
{
	float flTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Delete @%.3f\n", flTimeStamp );

	// Call saveutil
	int retv = cellSaveDataAutoSave2(
		CELL_SAVEDATA_VERSION_CURRENT,
		g_pszSaveUtilContainerName,
		g_pSaveUtilAsyncStatus->m_bUseSystemDialogs ? CELL_SAVEDATA_ERRDIALOG_ALWAYS : CELL_SAVEDATA_ERRDIALOG_NONE,
		&m_SaveDirInfo,	
		csDataStatCallback,
		csDataFileCallback,
		SYS_MEMORY_CONTAINER_ID_INVALID,
		this );

	float flEndTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Delete: cellSaveDataAutoSave2 returned %x @%.3f ( total time = %.3f sec )\n", retv, flEndTimeStamp, flEndTimeStamp - flTimeStamp );

	++ g_SaveUtilV2TOCVersion;
	return SaveUtilV2_JobDone( retv );
}

void CSaveUtilV2Job_Delete::DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS )
{
	Msg( "CSaveUtilV2Job_Delete::DoDataStatCallback @%.3f\n", Plat_FloatTime() );

	SetDataFileCallback( &CSaveUtilV2Job_Delete::DoDataFile_Delete );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
}

void CSaveUtilV2Job_Delete::DoDataFile_Delete( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Delete::DoDataFile_Delete @%.3f\n", Plat_FloatTime() );

	// Perform the delete
	set->fileOperation = CELL_SAVEDATA_FILEOP_DELETE;
	set->fileBuf = NULL;
	set->fileSize = set->fileBufSize = 0;
	set->fileName = m_pTocEntry->m_chContainerName;
	set->fileOffset = 0;
	set->fileType = CELL_SAVEDATA_FILETYPE_SECUREFILE;
	memcpy( set->secureFileId, g_pszSaveUtilSecureFileId, CELL_SAVEDATA_SECUREFILEID_SIZE );
	set->reserved = NULL;

	// final write
	SetDataFileCallback( &CSaveUtilV2Job_Delete::DoDataFile_UpdateTOC );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_Delete::DoDataFile_Delete will delete '%s'...\n", m_pTocEntry->m_chContainerName );
}

void CSaveUtilV2Job_Delete::DoDataFile_UpdateTOC( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Delete::DoDataFile_UpdateTOC @%.3f\n", Plat_FloatTime() );

	// Update TOC since we successfully deleted the file
	{
		AUTO_LOCK( g_SaveUtilV2TOC.m_mtx );
		// Deleting an entry doesn't reallocate the memory
		g_SaveUtilV2TOC.m_arrEntries.Remove( m_nTocEntryIndex );
	}

	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
}


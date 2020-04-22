//===== Copyright © 1996-2011, Valve Corporation, All rights reserved. ======//

#include "ps3_saveutil_v2.h"
#include "memdbgon.h"

class CSaveUtilV2Job_WriteSteamInfo : public ISaveUtilV2Job
{
public:	// Job entry point
	virtual JobStatus_t DoExecute();

protected: // Stat callback
	virtual void DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS );

protected: // Write Steam info
	void DoDataFile_WriteSteamInfo( SONY_SAVEUTIL_FILE_PARAMS );
};

//////////////////////////////////////////////////////////////////////////

void SaveUtilV2_WriteSteamInfo( CPS3SaveRestoreAsyncStatus *pAsync )
{
	if ( !SaveUtilV2_CanStartJob() )
		return;

	// Make sure that Steam info is prepared on the main thread
	if ( !g_pSteamInfoProvider->PrepareSaveBufferForCommit() )
		return;

	// Start the job
	CSaveUtilV2Job_WriteSteamInfo *pJob = new CSaveUtilV2Job_WriteSteamInfo;

	SaveUtilV2_EnqueueJob( pAsync, pJob );
}

//////////////////////////////////////////////////////////////////////////

JobStatus_t CSaveUtilV2Job_WriteSteamInfo::DoExecute()
{
	float flTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_WriteSteamInfo @%.3f\n", flTimeStamp );

	// Call saveutil
	int retv = cellSaveDataAutoSave2(
		CELL_SAVEDATA_VERSION_CURRENT,
		g_pszSaveUtilContainerName,
		CELL_SAVEDATA_ERRDIALOG_ALWAYS,
		&m_SaveDirInfo,	
		csDataStatCallback,
		csDataFileCallback,
		SYS_MEMORY_CONTAINER_ID_INVALID,
		this );

	float flEndTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_WriteSteamInfo: cellSaveDataAutoSave2 returned %x @%.3f ( total time = %.3f sec )\n", retv, flEndTimeStamp, flEndTimeStamp - flTimeStamp );

	return SaveUtilV2_JobDone( retv );
}

void CSaveUtilV2Job_WriteSteamInfo::DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS )
{
	Msg( "CSaveUtilV2Job_WriteSteamInfo::DoDataStatCallback @%.3f\n", Plat_FloatTime() );

	SetDataFileCallback( &CSaveUtilV2Job_WriteSteamInfo::DoDataFile_WriteSteamInfo );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
}

void CSaveUtilV2Job_WriteSteamInfo::DoDataFile_WriteSteamInfo( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_WriteSteamInfo::DoDataFile_WriteSteamInfo @%.3f\n", Plat_FloatTime() );

	// Obtain steam buffer
	CUtlBuffer *pBuffer = g_pSteamInfoProvider->GetSaveBufferForCommit();
	if ( !pBuffer )
	{
		cbResult->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
		return;
	}

	// Perform the write
	set->fileOperation = CELL_SAVEDATA_FILEOP_WRITE;
	set->fileBuf = pBuffer->Base();
	set->fileSize = set->fileBufSize = pBuffer->TellPut();
	set->fileName = VALVE_CONTAINER_FILE_STEAM;
	set->fileOffset = 0;
	set->fileType = CELL_SAVEDATA_FILETYPE_SECUREFILE;
	memcpy( set->secureFileId, g_pszSaveUtilSecureFileId, CELL_SAVEDATA_SECUREFILEID_SIZE );
	set->reserved = NULL;

	// final write
	SetDataFileCallbackFinalize();
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_WriteSteamInfo::DoDataFile_WriteSteamInfo will write %d bytes...\n", pBuffer->TellPut() );
}


//===== Copyright © 1996-2011, Valve Corporation, All rights reserved. ======//

#include "ps3_saveutil_v2.h"
#include "fmtstr.h"
#include "checksum_crc.h"
#include "memdbgon.h"


CON_COMMAND( ps3_saveutil_showtoc, "" )
{
	AUTO_LOCK( g_SaveUtilV2TOC.m_mtx );

	int numTocEntries = g_SaveUtilV2TOC.m_arrEntries.Count();
	Msg( "--------- SAVEUTILTOC -----------\n" );
	for ( int k = 0; k < numTocEntries; ++ k )
	{
		CSaveUtilV2ContainerTOC::TocEntry_t &e = g_SaveUtilV2TOC.m_arrEntries[k].m_entry;
		Msg( "%02d : %016llx  %s\n"
			 "               '%s' %u/%u\n"
			 "               '%s' %u/%u\n"
			 "                %s\n",
			k + 1, e.m_timeModification, e.m_chContainerName,
			e.m_chFile[0], e.m_numBytesFile[0], e.m_numBytesDecompressedFile[0],
			e.m_chFile[1], e.m_numBytesFile[1], e.m_numBytesDecompressedFile[1],
			e.m_chComment
			);
	}
	Msg( "--------- %02d ENTRIES -----------\n", numTocEntries );
}

void SaveUtilV2_GetFileInfoSync( CUtlVector< IPS3SaveRestoreToUI::PS3SaveGameInfo_t > &saveGameInfos, bool bFindAll )
{
	// This can be called after starting a save op but before it completes, so this will return old data in that case.
	// The caller should be aware that if SaveUtil is busy then it can check the operation TAG and know what is in
	// progress and whether it can affect the TOC after it's finished.
	// Currently only UI queries the TOC and ensures that writes of savegames are completed before queries.
	AUTO_LOCK( g_SaveUtilV2TOC.m_mtx );

	if ( !g_SaveUtilV2TOC.m_arrEntries.Count() )
	{
		saveGameInfos.RemoveAll();
		return;
	}

	int numTocEntries = bFindAll ? g_SaveUtilV2TOC.m_arrEntries.Count() : 1;
	saveGameInfos.SetCount( numTocEntries );
	for ( int k = 0; k < numTocEntries; ++ k )
	{
		saveGameInfos[k].m_InternalName = CFmtStr( "!%s", g_SaveUtilV2TOC.m_arrEntries[k].m_entry.m_chContainerName );
		saveGameInfos[k].m_Comment = g_SaveUtilV2TOC.m_arrEntries[k].m_entry.m_chComment;
		saveGameInfos[k].m_Filename = g_SaveUtilV2TOC.m_arrEntries[k].m_entry.m_chFile[0];
		saveGameInfos[k].m_ScreenshotFilename = g_SaveUtilV2TOC.m_arrEntries[k].m_entry.m_chFile[1];
		saveGameInfos[k].m_nFileTime = g_SaveUtilV2TOC.m_arrEntries[k].m_entry.m_timeModification;
	}
}

//////////////////////////////////////////////////////////////////////////

bool SaveUtilV2_CanStartJob()
{
	bool bResult = ( g_pSaveUtilThreadPool && !g_pSaveUtilAsyncStatus );
	if ( !bResult )
	{
		Warning( "SaveUtilV2_CanStartJob : cannot start job now! Invalid usage!\n" );
		Assert( 0 );
	}
	return bResult;
}

void SaveUtilV2_EnqueueJob( CPS3SaveRestoreAsyncStatus *pAsync, ISaveUtilV2Job *pJob )
{
	if ( g_pSaveUtilAsyncStatus )
		Error( "SaveUtilV2_EnqueueJob while job already running ( %p running, %p attempted )!\n", g_pSaveUtilAsyncStatus, pAsync );

	g_pSaveUtilAsyncStatus = pAsync;

	// Prepare for saveutil operation
	const int numContainers = VALVE_CONTAINER_COUNT;
	pJob->m_bufSaveDirList.EnsureCapacity( numContainers * MAX( sizeof( CellSaveDataFileStat ), sizeof( CellSaveDataDirList ) ) );

	// Prepare save dir info
	memset( &pJob->m_SaveDirInfo, 0, sizeof(CellSaveDataSetBuf) );
	pJob->m_SaveDirInfo.dirListMax = numContainers;
	pJob->m_SaveDirInfo.fileListMax = numContainers;
	pJob->m_SaveDirInfo.bufSize = pJob->m_bufSaveDirList.Size();
	pJob->m_SaveDirInfo.buf = pJob->m_bufSaveDirList.Base();

	// Mark the job as pending
	g_pSaveUtilAsyncStatus->m_nSonyRetValue = CELL_SAVEDATA_ERROR_NOTSUPPORTED;
	g_pSaveUtilAsyncStatus->m_bDone = 0;

	// Let's notify the file system that a save is starting, that way the file system can try to reduce HDD accesses and use BluRay instead
	g_pFullFileSystem->OnSaveStateChanged( true );

	// Add the job to thread pool
	pJob->SetFlags( JF_SERIAL | JF_QUEUE );
	g_pSaveUtilThreadPool->AddJob( pJob );
	pJob->Release();
}

JobStatus_t SaveUtilV2_JobDone( int nErrorCode )
{
	// Let's notify the file system that a save is finished, that way the file system can restart using the HDD
	g_pFullFileSystem->OnSaveStateChanged( false );

	// Set the job error code and set that the job is done
	if ( nErrorCode != CELL_SAVEDATA_ERROR_CBRESULT )
		g_pSaveUtilAsyncStatus->m_nSonyRetValue = nErrorCode;
	else if ( g_pSaveUtilAsyncStatus->m_nSonyRetValue >= 0 )
		g_pSaveUtilAsyncStatus->m_nSonyRetValue = CELL_SAVEDATA_ERROR_FAILURE;
	
	CPS3SaveRestoreAsyncStatus *pAsync = g_pSaveUtilAsyncStatus;
	g_pSaveUtilAsyncStatus = NULL;
	pAsync->m_bDone = 1;
	return JOB_OK;
}

uint32 SaveUtilV2_ComputeBufferHash( void const *pvData, uint32 numBytes )
{
	return CRC32_ProcessSingleBuffer( pvData, numBytes );
}

//////////////////////////////////////////////////////////////////////////

void ISaveUtilV2Job::csDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS )
{
	ISaveUtilV2Job *pSelf = static_cast<ISaveUtilV2Job*>( cbResult->userdata );
	pSelf->DoDataStatCallback( SONY_SAVEUTIL_PARAMS );
}

void ISaveUtilV2Job::csDataFileCallback( SONY_SAVEUTIL_FILE_PARAMS )
{
	ISaveUtilV2Job *pSelf = static_cast<ISaveUtilV2Job*>( cbResult->userdata );
	if ( pSelf->m_pfnDoDataFileCallback )
	{
		(pSelf->*(pSelf->m_pfnDoDataFileCallback))( SONY_SAVEUTIL_PARAMS );
	}
	else
	{
		Msg( "ISaveUtilV2Job::csDataFileCallback finalizing save operation @%.3f\n", Plat_FloatTime() );
		cbResult->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
	}
}

//////////////////////////////////////////////////////////////////////////

void CSaveUtilV2ContainerTOC::SerializeIntoTocBuffer( void *pvBuffer )
{
	uint32 *pui32 = (uint32*) pvBuffer;
	*( pui32 ++ ) = m_idxNewSaveName;
	*( pui32 ++ ) = m_arrEntries.Count();
	V_memcpy( pui32, m_arrEntries.Base(), m_arrEntries.Count() * sizeof( TocStorageReserved_t ) );
}

void CSaveUtilV2ContainerTOC::SerializeFromTocBuffer( void *pvBuffer )
{
	uint32 *pui32 = (uint32*) pvBuffer;
	m_idxNewSaveName = *( pui32 ++ );
	uint32 uiEntriesCount = *( pui32 ++ );
	uiEntriesCount = MIN( uiEntriesCount, VALVE_CONTAINER_COUNT );
	m_arrEntries.AddMultipleToTail( uiEntriesCount, reinterpret_cast< TocStorageReserved_t * >( pui32 ) );
}

void CSaveUtilV2ContainerTOC::CopyInto( CSaveUtilV2ContainerTOC *pOther )
{
	pOther->m_idxNewSaveName = m_idxNewSaveName;
	pOther->m_arrEntries.RemoveAll();
	pOther->m_arrEntries.AddMultipleToTail( m_arrEntries.Count(), m_arrEntries.Base() );
}

int CSaveUtilV2ContainerTOC::FindByEmbeddedFileName( char const *szFilename, int *pnPartIndex )
{
	for ( int k = 0; k < m_arrEntries.Count(); ++ k )
	{
		if ( szFilename[0] == '!' )
		{
			if ( V_stricmp( m_arrEntries[k].m_entry.m_chContainerName, szFilename + 1 ) )
				continue;
			if ( pnPartIndex )
				*pnPartIndex = 0;
			return k;
		}
		for ( int iPart = 0; iPart < VALVE_CONTAINER_FPARTS; ++ iPart )
		{
			if ( !V_stricmp( m_arrEntries[k].m_entry.m_chFile[iPart], szFilename ) )
			{
				if ( pnPartIndex )
					*pnPartIndex = iPart;
				return k;
			}
		}
	}

	if ( pnPartIndex )
		*pnPartIndex = -1;
	return -1;
}


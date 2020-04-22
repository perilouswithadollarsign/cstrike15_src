//===== Copyright © 1996-2011, Valve Corporation, All rights reserved. ======//

#ifndef PS3_SAVEUTIL_V2_H
#define PS3_SAVEUTIL_V2_H

//////////////////////////////////////////////////////////////////////////

#include "tier1/utlbuffer.h"
#include "common.h"
#include "vstdlib/jobthread.h"

#include <sysutil/sysutil_userinfo.h>
#include <sysutil/sysutil_savedata.h>
#include <cell/sysmodule.h>
#include <sys/fs_external.h>
#include <ps3/saverestore_ps3_api_ui.h>
#include <vjobs_interface.h>

//////////////////////////////////////////////////////////////////////////

extern CPS3SaveRestoreAsyncStatus *g_pSaveUtilAsyncStatus;
extern IPS3SaveSteamInfoProvider *g_pSteamInfoProvider;
extern IThreadPool *g_pSaveUtilThreadPool;
extern char const *g_pszSaveUtilContainerName;
extern char const *g_pszSaveUtilSecureFileId;
extern uint64 g_uiSteamCloudCryptoKey;

#define VALVE_CONTAINER_FILE_STEAM "STEAMDAT.BIN"

#define VALVE_CONTAINER_STRLEN 128
#define VALVE_CONTAINER_FILENAME_LEN 64
#define VALVE_CONTAINER_8_3_LEN 16
#define VALVE_CONTAINER_COUNT 64
#define VALVE_CONTAINER_FPARTS 2

void SaveUtilV2_Initialize( CPS3SaveRestoreAsyncStatus *pAsync, IPS3SaveSteamInfoProvider *pSteamInfoProvider, int nKBRequired );

void SaveUtilV2_Shutdown();
bool SaveUtilV2_CanShutdown();

void SaveUtilV2_GetFileInfoSync( CUtlVector< IPS3SaveRestoreToUI::PS3SaveGameInfo_t > &saveGameInfos, bool bFindAll );

void SaveUtilV2_Write( CPS3SaveRestoreAsyncStatus *pAsync, const char *pSourcepath, const char *pScreenshotPath, const char *pComment );
void SaveUtilV2_WriteAutosave(  CPS3SaveRestoreAsyncStatus *pAsync, 
			  const char *pSourcePath, // eg "/dev_hdd1/tempsave/autosave.ps3.sav"
			  const char *pComment, // the comment field for the new autosave.
			  const unsigned int nMaxNumAutosaves );
void SaveUtilV2_WriteCloudFile(  CPS3SaveRestoreAsyncStatus *pAsync, 
							  const char *pSourcePath, // eg "/dev_hdd1/tempsave/autosave.ps3.sav"
							  const unsigned int nMaxNumCloudFiles );

void SaveUtilV2_WriteSteamInfo( CPS3SaveRestoreAsyncStatus *pAsync );

void SaveUtilV2_Load( CPS3SaveRestoreAsyncStatus *pAsync, const char *pFilename, const char *pDestFullPath );

void SaveUtilV2_Delete( CPS3SaveRestoreAsyncStatus *pAsync, const char *pFilename );


//////////////////////////////////////////////////////////////////////////
//
// Helper definitions and declarations
//

#define SONY_SAVEUTIL_STAT_PARAMS CellSaveDataCBResult *cbResult, CellSaveDataStatGet *get, CellSaveDataStatSet *set
#define SONY_SAVEUTIL_FILE_PARAMS CellSaveDataCBResult *cbResult, CellSaveDataFileGet *get, CellSaveDataFileSet *set
#define SONY_SAVEUTIL_PARAMS cbResult, get, set

class ISaveUtilV2Job : public CJob
{
public:
	ISaveUtilV2Job() { m_pfnDoDataFileCallback = 0; }

public:
	static void csDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS );
	static void csDataFileCallback( SONY_SAVEUTIL_FILE_PARAMS );

public:
	virtual void DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS ) = 0;
	void (ISaveUtilV2Job::*m_pfnDoDataFileCallback)( SONY_SAVEUTIL_FILE_PARAMS );
	template< typename T > inline void SetDataFileCallback( void (T::*pfnCallback)( SONY_SAVEUTIL_FILE_PARAMS ) ) { m_pfnDoDataFileCallback = reinterpret_cast< void (ISaveUtilV2Job::*)( SONY_SAVEUTIL_FILE_PARAMS ) >( pfnCallback ); }
	inline void SetDataFileCallbackFinalize() { m_pfnDoDataFileCallback = 0; }

public:
	CellSaveDataSetBuf m_SaveDirInfo;
	CUtlBuffer m_bufSaveDirList;
};

bool SaveUtilV2_CanStartJob();
void SaveUtilV2_EnqueueJob( CPS3SaveRestoreAsyncStatus *pAsync, ISaveUtilV2Job *pJob );
JobStatus_t SaveUtilV2_JobDone( int nErrorCode );

uint32 SaveUtilV2_ComputeBufferHash( void const *pvData, uint32 numBytes );

//////////////////////////////////////////////////////////////////////////
//
// Container TOC
//

class CSaveUtilV2ContainerTOC
{
public:
	CSaveUtilV2ContainerTOC() : m_idxNewSaveName( 0 ) {}

	struct TocEntry_t
	{
		char m_chContainerName[VALVE_CONTAINER_8_3_LEN];						// 8.3 name inside container: 0000001A.SAV
		char m_chComment[VALVE_CONTAINER_STRLEN];								// description of file without needing to open file
		char m_chFile[VALVE_CONTAINER_FPARTS][VALVE_CONTAINER_FILENAME_LEN];	// names of the contained parts [sav+tga]
		uint32 m_numBytesFile[VALVE_CONTAINER_FPARTS];							// sizes of the parts inside container file [sav+tga]
		uint32 m_numBytesDecompressedFile[VALVE_CONTAINER_FPARTS];				// sizes of original decompressed parts [sav+tga], zero if uncompressed
		time_t m_timeModification;												// timestamp of the container file
	};
	union TocStorageReserved_t
	{
		TocEntry_t m_entry;
		char m_chPadding[384];
	};
	CUtlVector< TocStorageReserved_t > m_arrEntries;
	CThreadFastMutex m_mtx;
	uint32 m_idxNewSaveName;

	enum Capacity_t
	{
		kStorageCapacity =
			sizeof(uint32) +	// new save name
			sizeof(uint32) +	// number of valid entries
			VALVE_CONTAINER_COUNT*sizeof(TocStorageReserved_t)	// actual TOC entries
	};
	
public:
	void SerializeIntoTocBuffer( void *pvBuffer );
	void SerializeFromTocBuffer( void *pvBuffer );
	void CopyInto( CSaveUtilV2ContainerTOC *pOther );
	int FindByEmbeddedFileName( char const *szFilename, int *pnPartIndex );
};
extern CSaveUtilV2ContainerTOC g_SaveUtilV2TOC;
extern uint32 g_SaveUtilV2TOCVersion;


class CSaveUtilVjobInstance : public VJobInstance
{
public:
	void Init();
	void Shutdown();
};

extern CSaveUtilVjobInstance g_saveUtilVjobInstance;

#endif

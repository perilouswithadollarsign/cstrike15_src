//===== Copyright © 1996-2011, Valve Corporation, All rights reserved. ======//

#include "ps3_saveutil_v2.h"
#include "vgui/ILocalize.h"
#include "vstdlib/vstrtools.h"
#include "memdbgon.h"

//////////////////////////////////////////////////////////////////////////

CPS3SaveRestoreAsyncStatus *g_pSaveUtilAsyncStatus = NULL;
IPS3SaveSteamInfoProvider *g_pSteamInfoProvider = NULL;
IThreadPool *g_pSaveUtilThreadPool = NULL;
CSaveUtilVjobInstance g_saveUtilVjobInstance;

static char g_chSaveUtilContainerName[64];
char const *g_pszSaveUtilContainerName = g_chSaveUtilContainerName;
CSaveUtilV2ContainerTOC g_SaveUtilV2TOC;
uint32 g_SaveUtilV2TOCVersion;
extern IVJobs * g_pVJobs;

// this is our "SECURE FILE ID"
static const char s_SaveUtilSecureFileId[CELL_SAVEDATA_SECUREFILEID_SIZE] = 
{ 0xDD, 0xE3, 0x80, 0x72, 0xC7, 0x9F, 0xAF, 0x2A, 0x2B, 0x68, 0xAF, 0xC9, 0x6D, 0x6A, 0xED, 0xC1 } ;
char const *g_pszSaveUtilSecureFileId = s_SaveUtilSecureFileId;
uint64 g_uiSteamCloudCryptoKey = 0ull;

ASSERT_INVARIANT( sizeof( CSaveUtilV2ContainerTOC::TocEntry_t ) < sizeof( CSaveUtilV2ContainerTOC::TocStorageReserved_t ) );


//////////////////////////////////////////////////////////////////////////


class CSaveUtilV2Job_Initialize : public ISaveUtilV2Job
{
public:	// Job entry point
	virtual JobStatus_t DoExecute();

public: // Caller passes data from main thread
	int m_nKBRequired;

protected: // Our buffer for interacting with filesystem
	CUtlBuffer m_bufScratch;
	CSaveUtilV2ContainerTOC m_newTOC;
	CUtlVector< CellSaveDataFileStat > m_arrFilesInContainer;

protected: // Stat callback
	virtual void DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS );
	bool DoDataStat_ValidateFreeSpace( SONY_SAVEUTIL_STAT_PARAMS );
	void DoDataStat_NewContainer( SONY_SAVEUTIL_STAT_PARAMS );

protected: // When Steam file exists we load it first
	CellSaveDataFileStat m_LoadSteamFileStat;
	void DoDataFile_LoadSteam( SONY_SAVEUTIL_FILE_PARAMS );

protected: // When container exists we load TOC
	CellSaveDataFileStat m_LoadTocFileStat;
	void DoDataFile_LoadToc( SONY_SAVEUTIL_FILE_PARAMS );
	void DoDataFile_DisplayToc( SONY_SAVEUTIL_FILE_PARAMS );

protected: // TOC post-processing
	bool PostProcessToc_DeletedFiles();
	bool PostProcessToc_Validate();

protected: // When container is newly created we write initial data
	void DoDataFile_WriteEmptySteamFile( SONY_SAVEUTIL_FILE_PARAMS );
	void DoDataFile_WriteIcon0png( SONY_SAVEUTIL_FILE_PARAMS );
	void DoDataFile_WriteIcon1pam( SONY_SAVEUTIL_FILE_PARAMS );
	void DoDataFile_WritePic1png( SONY_SAVEUTIL_FILE_PARAMS );
};

//////////////////////////////////////////////////////////////////////////

static void SaveUtilV2_InitAndClearSaveShadow()
{
	char const *pszSaveDir = g_pPS3PathInfo->SaveShadowPath();
	g_pFullFileSystem->CreateDirHierarchy( pszSaveDir );
	// delete any files that are in the save dir already
	FileFindHandle_t handle;
	const char *pfname;
	char searchpath[255];
	V_snprintf( searchpath, sizeof(searchpath), "%s*", pszSaveDir );
	char deletepath[255];
	const int dirnamelen = V_strlen( pszSaveDir );
	Assert(dirnamelen < 255);
	memcpy(deletepath, pszSaveDir, dirnamelen);
	for ( pfname = g_pFullFileSystem->FindFirst( searchpath, &handle ) ;
		pfname ;
		pfname = g_pFullFileSystem->FindNext( handle ) )
	{
		V_strncpy( deletepath + dirnamelen, pfname, sizeof(deletepath) - dirnamelen );
		Msg( "Removing %s\n", deletepath );
		g_pFullFileSystem->RemoveFile( deletepath, NULL );
	}	
}

void SaveUtilV2_Initialize( CPS3SaveRestoreAsyncStatus *pAsync, IPS3SaveSteamInfoProvider *pSteamInfoProvider, int nKBRequired )
{
	if ( g_pSaveUtilThreadPool )
		return;

	// Initialize SPU jobs instance
	g_saveUtilVjobInstance.Init();

	// Indicate that we are running V2
	V_snprintf( g_chSaveUtilContainerName, 32, "%s-%s", g_pPS3PathInfo->GetWWMASTER_TitleID(), "PORTAL2-AUTOSAVE3" );
	SaveUtilV2_InitAndClearSaveShadow();

	// First of all start our thread pool
	ThreadPoolStartParams_t params;
	params.nThreads = 1;
	params.nStackSize = 64*1024;
	params.fDistribute = TRS_FALSE;
	g_pSaveUtilThreadPool = CreateNewThreadPool();
	g_pSaveUtilThreadPool->Start( params, "SaveUtilV2" );

	// Remember launch parameters
	g_pSteamInfoProvider = pSteamInfoProvider;

	// Start the job
	CSaveUtilV2Job_Initialize *pJob = new CSaveUtilV2Job_Initialize;
	pJob->m_nKBRequired = nKBRequired;

	SaveUtilV2_EnqueueJob( pAsync, pJob );
}

void SaveUtilV2_Shutdown()
{
	if ( !g_pSaveUtilThreadPool )
		return;

	g_pSaveUtilThreadPool->Stop();
	DestroyThreadPool( g_pSaveUtilThreadPool );
	g_pSaveUtilThreadPool = NULL;

	// Shutdown jobs instance after thread pool has been
	// stopped which ensures that jobs were finished and released
	g_saveUtilVjobInstance.Shutdown();
}

bool SaveUtilV2_CanShutdown()
{
	if ( !g_pSaveUtilThreadPool )
		return true;

	if ( g_pSaveUtilAsyncStatus )
		// job in progress
		return false;

	return true;
}




//////////////////////////////////////////////////////////////////////////

JobStatus_t CSaveUtilV2Job_Initialize::DoExecute()
{
	float flTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Initialize( %d KB ) @%.3f\n", m_nKBRequired, flTimeStamp );

	// Prepare data for calling saveutil (we cannot allocate memory in the callbacks!)
	m_bufScratch.EnsureCapacity( 5 * 1024 * 1024 ); // we always have 5 MB on startup

	m_newTOC.m_arrEntries.EnsureCapacity( VALVE_CONTAINER_COUNT );
	m_arrFilesInContainer.EnsureCapacity( VALVE_CONTAINER_COUNT );

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

	// Set the global TOC as loaded
	{
		AUTO_LOCK( g_SaveUtilV2TOC.m_mtx );
		m_newTOC.CopyInto( &g_SaveUtilV2TOC );
	}

	// Job finished
	float flEndTimeStamp = Plat_FloatTime();
	Msg( "CSaveUtilV2Job_Initialize: cellSaveDataAutoSave2 returned %x @%.3f ( total time = %.3f sec )\n", retv, flEndTimeStamp, flEndTimeStamp - flTimeStamp );

	return SaveUtilV2_JobDone( retv );
}


void CSaveUtilV2Job_Initialize::DoDataStatCallback( SONY_SAVEUTIL_STAT_PARAMS )
{
	Msg( "CSaveUtilV2Job_Initialize::DoDataStatCallback @%.3f\n", Plat_FloatTime() );
	Msg("\tnKBRequired   %d\n", m_nKBRequired );
	Msg("\tisNewData     %d\n", get->isNewData );
	Msg("\thddFreeSizeKB %d\n", get->hddFreeSizeKB);
	Msg("\tsizeKB        %d\n", get->sizeKB);
	Msg("\tsysSizeKB     %d\n", get->sysSizeKB);
	Msg("\tbind          %d\n", get->bind);

	bool bCreateNew = false; // do we need to create a new container?
	if ( get->isNewData || get->bind != CELL_SAVEDATA_BINDSTAT_OK ) 
	{
		if ( get->isNewData )
		{	// if the data is just absent, we can always make a new container.
			bCreateNew = true;
		}
		else if ( ( get->bind & ( CELL_SAVEDATA_BINDSTAT_ERR_NOACCOUNTID | CELL_SAVEDATA_BINDSTAT_ERR_LOCALOWNER ) ) == get->bind )
		{
			// this  is actually owned by the current user; since the account
			// id will added on the next write, we can safely keep going.
			get->bind = 0;
		}
		else
		{
			// this is an ownership error
			set->reCreateMode = CELL_SAVEDATA_RECREATE_NO;
			g_pSaveUtilAsyncStatus->m_nSonyRetValue = CPS3SaveRestoreAsyncStatus::CELL_SAVEDATA_ERROR_WRONG_USER;
			cbResult->result = CELL_SAVEDATA_CBRESULT_ERR_INVALID;
			return;
		}
	}

	set->setParam = NULL; // don't edit the PARAM.SFO

	// Validate free space
	if ( !DoDataStat_ValidateFreeSpace( SONY_SAVEUTIL_PARAMS ) )
		return;

	// if there's a toc, read it. 
	if ( !get->isNewData )
	{
		V_memset( &m_LoadSteamFileStat, 0, sizeof( m_LoadSteamFileStat ) );
		V_memset( &m_LoadTocFileStat, 0, sizeof( m_LoadTocFileStat ) );

		for ( int i = 0 ; i < get->fileListNum ; ++i )
		{
			// Icons and video have special filetypes, we are interested only in our data
			// our data has file type "securefile"
			if ( get->fileList[i].fileType != CELL_SAVEDATA_FILETYPE_SECUREFILE )
				continue;

			// Steam configuration file
			if ( !V_strcmp( get->fileList[i].fileName, VALVE_CONTAINER_FILE_STEAM ) )
			{
				Msg( "CSaveUtilV2Job_Initialize       found Steam file: %u bytes\n", get->fileList[i].st_size );
				V_memcpy( &m_LoadSteamFileStat, &get->fileList[i], sizeof( m_LoadSteamFileStat ) );
				continue;
			}

			// Every file will have full TOC prepended, makes no sense
			// to have files in container that are smaller than required size
			if ( get->fileList[i].st_size < CSaveUtilV2ContainerTOC::kStorageCapacity )
				continue;

			// Remember the file as present in container
			m_arrFilesInContainer.AddToTail( get->fileList[i] );

			// Add this file for TOC discovery (the latest TOC will be at the start of the file with the alphanumeric-highest filename)
			Msg( "CSaveUtilV2Job_Initialize       found file '%s', %u bytes\n", get->fileList[i].fileName, get->fileList[i].st_size );
			if ( !m_LoadTocFileStat.fileName[0] || // TOC filename not set yet
				( V_strcmp( get->fileList[i].fileName, m_LoadTocFileStat.fileName ) > 0 ) ) // or bigger filename ( -> more recent file)
			{
				V_memcpy( &m_LoadTocFileStat, &get->fileList[i], sizeof( m_LoadTocFileStat ) );
			}
		}
		
		SetDataFileCallback( &CSaveUtilV2Job_Initialize::DoDataFile_LoadSteam );
		cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
		return;
	}
	else if ( bCreateNew )
	{
		// Create a new container
		DoDataStat_NewContainer( SONY_SAVEUTIL_PARAMS );

		SetDataFileCallback( &CSaveUtilV2Job_Initialize::DoDataFile_WriteEmptySteamFile );
		cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
		return;
	}

	// this means "okay,  I'm done"
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
}

bool CSaveUtilV2Job_Initialize::DoDataStat_ValidateFreeSpace( SONY_SAVEUTIL_STAT_PARAMS )
{
	// do we have enough space?
	//                   system overhead + caller max size  ?    HDD free space   + current container file
	int numKbRequired = ( get->sysSizeKB + m_nKBRequired ) - ( get->hddFreeSizeKB + get->sizeKB );
	if ( numKbRequired > 0 )
	{
		// inadequate space.
		cbResult->result = CELL_SAVEDATA_CBRESULT_ERR_NOSPACE;
		g_pSaveUtilAsyncStatus->m_nSonyRetValue = CELL_SAVEDATA_ERROR_NOSPACE;
		cbResult->errNeedSizeKB = numKbRequired;
		g_pSaveUtilAsyncStatus->m_uiAdditionalDetails = numKbRequired;
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CSaveUtilV2Job_Initialize::DoDataStat_NewContainer( SONY_SAVEUTIL_STAT_PARAMS )
{
	set->reCreateMode = CELL_SAVEDATA_RECREATE_YES_RESET_OWNER;
	get->bind = 0;

	// the convention for editing param.sfo is pointing at the struct in the get param, and then editing that. (jeez...)
	set->setParam = &get->getParam;  

	/* Set parameters of PARAM.SFO */
	V_strncpy( set->setParam->title, g_pPS3PathInfo->GetParamSFO_Title(), CELL_SAVEDATA_SYSP_TITLE_SIZE );

	// the save game title
	wchar_t *szLocalizedSaveDataTitle = g_pLocalize->Find( "#CSGOPS3_SaveData" );

	if ( szLocalizedSaveDataTitle )
	{
		V_UnicodeToUTF8( szLocalizedSaveDataTitle, set->setParam->subTitle, CELL_SAVEDATA_SYSP_SUBTITLE_SIZE );
	}
	else // failsafe
	{
		V_strncpy( set->setParam->subTitle, "Counter Strike: Global Offensive" , CELL_SAVEDATA_SYSP_SUBTITLE_SIZE );
	}

	// the save game caption -- if missing, we just use an empty string here.
	szLocalizedSaveDataTitle = g_pLocalize->Find( "#CSGOPS3_SaveDetail" );
	if ( szLocalizedSaveDataTitle )
	{
		V_UnicodeToUTF8( szLocalizedSaveDataTitle, set->setParam->detail, CELL_SAVEDATA_SYSP_DETAIL_SIZE );
	}
	else
	{
		memset( set->setParam->detail, 0, CELL_SAVEDATA_SYSP_DETAIL_SIZE );
	}

	set->setParam->attribute = CELL_SAVEDATA_ATTR_NORMAL;

	// listparam is available to the application inside stat callback
	Q_memset( set->setParam->listParam, 0, CELL_SAVEDATA_SYSP_LPARAM_SIZE );

	memset( set->setParam->reserved,  0x0, sizeof(set->setParam->reserved) );	/* The reserved member must be zero-filled */
	memset( set->setParam->reserved2, 0x0, sizeof(set->setParam->reserved2) );	/* The reserved member must be zero-filled */
}

void CSaveUtilV2Job_Initialize::DoDataFile_WriteEmptySteamFile( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_WriteEmptySteamFile @%.3f\n", Plat_FloatTime() );
	
	const static char szEmptyToc[] = "EMPTY";
	set->fileOperation = CELL_SAVEDATA_FILEOP_WRITE;
	set->fileBuf = const_cast<char *>(szEmptyToc);
	set->fileSize = set->fileBufSize = sizeof(szEmptyToc);
	set->fileName = VALVE_CONTAINER_FILE_STEAM;
	set->fileOffset = 0;
	set->fileType = CELL_SAVEDATA_FILETYPE_SECUREFILE;
	memcpy( set->secureFileId, g_pszSaveUtilSecureFileId, CELL_SAVEDATA_SECUREFILEID_SIZE );
	set->reserved = NULL;
	
	// call back again to write icon
	SetDataFileCallback( &CSaveUtilV2Job_Initialize::DoDataFile_WriteIcon0png );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_WriteEmptySteamFile will write %d bytes...\n", set->fileSize );
}

void CSaveUtilV2Job_Initialize::DoDataFile_WriteIcon0png( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_WriteIcon0png @%.3f\n", Plat_FloatTime() );

	// Load ICON0.PS3.PNG
	m_bufScratch.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	if ( !g_pFullFileSystem->ReadFile( "ps3/ICON0.PS3.PNG", "GAME", m_bufScratch ) )
	{
		DoDataFile_WriteIcon1pam( SONY_SAVEUTIL_PARAMS );
		return;
	}

	// Write it
	set->fileOperation = CELL_SAVEDATA_FILEOP_WRITE;
	set->fileBuf = m_bufScratch.Base();
	set->fileSize = set->fileBufSize = m_bufScratch.TellPut();
	set->fileOffset = 0;
	set->fileType = CELL_SAVEDATA_FILETYPE_CONTENT_ICON0;
	set->reserved = NULL;

	// call back again to write icon
	SetDataFileCallback( &CSaveUtilV2Job_Initialize::DoDataFile_WriteIcon1pam );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_WriteIcon0png will write %d bytes...\n", set->fileSize );
}

void CSaveUtilV2Job_Initialize::DoDataFile_WriteIcon1pam( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_WriteIcon1pam @%.3f\n", Plat_FloatTime() );

	// Load ICON1.PS3.PAM
	m_bufScratch.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	
#if defined( CSTRIKE15 )

	// TODO($msmith): Remove this and put the PAM back in for CS:GO once it has been updated.
	if ( !g_pFullFileSystem->ReadFile( "ps3/not_found.PAM", "GAME", m_bufScratch ) )

#else

	if ( !g_pFullFileSystem->ReadFile( "ps3/ICON1.PS3.PAM", "GAME", m_bufScratch ) )

#endif

	{
		DoDataFile_WritePic1png( SONY_SAVEUTIL_PARAMS );
		return;
	}

	// Write it
	set->fileOperation = CELL_SAVEDATA_FILEOP_WRITE;
	set->fileBuf = m_bufScratch.Base();
	set->fileSize = set->fileBufSize = m_bufScratch.TellPut();
	set->fileOffset = 0;
	set->fileType = CELL_SAVEDATA_FILETYPE_CONTENT_ICON1;
	set->reserved = NULL;

	// call back again to write pic1
	SetDataFileCallback( &CSaveUtilV2Job_Initialize::DoDataFile_WritePic1png );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_WriteIcon1pam will write %d bytes...\n", set->fileSize );
}

void CSaveUtilV2Job_Initialize::DoDataFile_WritePic1png( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_WritePic1png @%.3f\n", Plat_FloatTime() );

	// Load PIC1.PS3.PNG
	m_bufScratch.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	if ( !g_pFullFileSystem->ReadFile( "ps3/PIC1.PS3.PNG", "GAME", m_bufScratch ) )
	{
		cbResult->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
		return;
	}

	// Write it
	set->fileOperation = CELL_SAVEDATA_FILEOP_WRITE;
	set->fileBuf = m_bufScratch.Base();
	set->fileSize = set->fileBufSize = m_bufScratch.TellPut();
	set->fileOffset = 0;
	set->fileType = CELL_SAVEDATA_FILETYPE_CONTENT_PIC1;
	set->reserved = NULL;

	// final write
	SetDataFileCallbackFinalize();
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_WritePic1png will write %d bytes...\n", set->fileSize );
}

//////////////////////////////////////////////////////////////////////////

void CSaveUtilV2Job_Initialize::DoDataFile_LoadSteam( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_LoadSteam @%.3f\n", Plat_FloatTime() );

	// Check that Steam file is present
	if ( m_LoadSteamFileStat.st_size < 16 )
	{
		DoDataFile_LoadToc( SONY_SAVEUTIL_PARAMS );
		return;
	}

	// Read into Steam buffer
	CUtlBuffer *pBuffer = g_pSteamInfoProvider->GetInitialLoadBuffer();
	if ( !pBuffer || ( pBuffer->Size() < m_LoadSteamFileStat.st_size ) )
	{
		Warning( "ERROR: CSaveUtilV2Job_Initialize::DoDataFile_LoadSteam: cannot load Steam config (size %llu bytes)!\n", m_LoadSteamFileStat.st_size );
		DoDataFile_LoadToc( SONY_SAVEUTIL_PARAMS );
		return;
	}

	// Read Steam info file
	set->fileOperation = CELL_SAVEDATA_FILEOP_READ;
	set->fileBuf = pBuffer->Base();
	set->fileSize = m_LoadSteamFileStat.st_size;
	set->fileBufSize = pBuffer->Size();
	set->fileName = VALVE_CONTAINER_FILE_STEAM;
	set->fileOffset = 0;
	set->fileType = CELL_SAVEDATA_FILETYPE_SECUREFILE;
	memcpy( set->secureFileId, g_pszSaveUtilSecureFileId, CELL_SAVEDATA_SECUREFILEID_SIZE );
	set->reserved = NULL;

	// Set the buffer size
	pBuffer->SeekPut( CUtlBuffer::SEEK_HEAD, set->fileSize );

	// final read
	SetDataFileCallback( &CSaveUtilV2Job_Initialize::DoDataFile_LoadToc );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_LoadSteam will load %d bytes...\n", set->fileSize );
}

void CSaveUtilV2Job_Initialize::DoDataFile_LoadToc( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_LoadToc @%.3f\n", Plat_FloatTime() );

	if ( m_LoadTocFileStat.st_size < CSaveUtilV2ContainerTOC::kStorageCapacity )
	{
		Msg( "CSaveUtilV2Job_Initialize::DoDataFile_LoadToc -- no TOC available.\n" );
		cbResult->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
		return;
	}

	// Load the TOC entry
	set->fileOperation = CELL_SAVEDATA_FILEOP_READ;
	set->fileBuf = m_bufScratch.Base();
	set->fileBufSize = set->fileSize = CSaveUtilV2ContainerTOC::kStorageCapacity;
	set->fileName = m_LoadTocFileStat.fileName;
	set->fileOffset = 0;
	set->fileType = CELL_SAVEDATA_FILETYPE_SECUREFILE;
	memcpy( set->secureFileId, g_pszSaveUtilSecureFileId, CELL_SAVEDATA_SECUREFILEID_SIZE );
	set->reserved = NULL;
	
	m_bufScratch.SeekPut( CUtlBuffer::SEEK_HEAD, CSaveUtilV2ContainerTOC::kStorageCapacity );

	// keep reading
	SetDataFileCallback( &CSaveUtilV2Job_Initialize::DoDataFile_DisplayToc );
	cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_LoadToc will load %u bytes of TOC from '%s'...\n", set->fileSize, set->fileName );
}

void CSaveUtilV2Job_Initialize::DoDataFile_DisplayToc( SONY_SAVEUTIL_FILE_PARAMS )
{
	Msg( "CSaveUtilV2Job_Initialize::DoDataFile_DisplayToc @%.3f\n", Plat_FloatTime() );

	m_newTOC.SerializeFromTocBuffer( m_bufScratch.Base() );
	bool bTocValidationResult = PostProcessToc_DeletedFiles();

	for ( int k = 0; k < m_newTOC.m_arrEntries.Count(); ++ k )
	{
		Msg( "  %s %s %d/%d %s\n",
			m_newTOC.m_arrEntries[k].m_entry.m_chContainerName,
			m_newTOC.m_arrEntries[k].m_entry.m_chFile[0],
			m_newTOC.m_arrEntries[k].m_entry.m_numBytesFile[0],
			m_newTOC.m_arrEntries[k].m_entry.m_numBytesDecompressedFile[0],
			m_newTOC.m_arrEntries[k].m_entry.m_chComment
			);
	}
	Msg( "  new save game will have index %u\n", m_newTOC.m_idxNewSaveName + 1 );
	Msg( "  END OF TOC REPORT\n" );

	if ( !PostProcessToc_Validate() || !bTocValidationResult )
	{
		// TOC is not matching contents of the container!
		Warning( "  ERROR: TOC DOES NOT MATCH SAVE CONTAINER!\n" );
		g_pSaveUtilAsyncStatus->m_nSonyRetValue = CELL_SAVEDATA_CBRESULT_ERR_BROKEN;
		cbResult->result = CELL_SAVEDATA_CBRESULT_ERR_BROKEN;
	}
	else
	{
		cbResult->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
	}
}

bool CSaveUtilV2Job_Initialize::PostProcessToc_DeletedFiles()
{
	// Iterate over TOC and remove entries that were deleted from container previously
	bool bTocAndContainerValid = true;
	for ( int k = m_newTOC.m_arrEntries.Count(); k --> 0; )
	{
		bool bFoundInContainer = false;
		int jc = 0;
		for ( ; jc < m_arrFilesInContainer.Count(); ++ jc )
		{
			if ( !V_strcmp( m_newTOC.m_arrEntries[k].m_entry.m_chContainerName, m_arrFilesInContainer[jc].fileName ) )
			{
				bFoundInContainer = true;
				break;
			}
		}
		if ( !bFoundInContainer )
		{
			Msg( "CSaveUtilV2Job_Initialize::PostProcessToc_DeletedFiles discovered deleted file '%s'\n", m_newTOC.m_arrEntries[k].m_entry.m_chContainerName );
			m_newTOC.m_arrEntries.Remove( k );
		}
		else
		{
			// Make sure the TOC information matches container stat information
			int nExpectedSize = CSaveUtilV2ContainerTOC::kStorageCapacity;
			for ( int ipart = 0; ipart < VALVE_CONTAINER_FPARTS; ++ ipart )
				nExpectedSize += m_newTOC.m_arrEntries[k].m_entry.m_numBytesFile[ipart];
			if ( m_arrFilesInContainer[jc].st_size != nExpectedSize )
			{
				Msg( "CSaveUtilV2Job_Initialize::PostProcessToc_DeletedFiles discovered size inconsistency in '%s' (TOC size = %d; Container size = %llu)\n",
					m_newTOC.m_arrEntries[k].m_entry.m_chContainerName, nExpectedSize, m_arrFilesInContainer[jc].st_size );
				bTocAndContainerValid = false;
			}
		}
	}
	return bTocAndContainerValid;
}

bool CSaveUtilV2Job_Initialize::PostProcessToc_Validate()
{
	// Iterate over TOC and make sure every container file is in TOC
	for ( int jc = m_arrFilesInContainer.Count(); jc --> 0; )
	{
		bool bFoundInTOC = false;
		for ( int k = 0; k < m_newTOC.m_arrEntries.Count(); ++ k )
		{
			if ( !V_strcmp( m_newTOC.m_arrEntries[k].m_entry.m_chContainerName, m_arrFilesInContainer[jc].fileName ) )
			{
				bFoundInTOC = true;
				break;
			}
		}
		if ( bFoundInTOC )
		{
			m_arrFilesInContainer.Remove( jc );
		}
		else
		{
			Msg( "CSaveUtilV2Job_Initialize::PostProcessToc_Validate discovered extra file '%s'\n", m_arrFilesInContainer[jc].fileName );
		}
	}

	// We shouldn't have any extra unaccounted files in the container
	return ( !m_arrFilesInContainer.Count() );
}




void CSaveUtilVjobInstance::Init()
{
	g_pVJobs->Register( this );
}


void CSaveUtilVjobInstance::Shutdown()
{
	g_pVJobs->Unregister( this );
}


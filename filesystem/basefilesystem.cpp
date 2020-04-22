//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "basefilesystem.h"
#include "tier0/vprof.h"
#include "tier1/characterset.h"
#include "tier1/utlbuffer.h"
#include "tier1/convar.h"
#include "tier1/keyvalues.h"
#include "tier0/icommandline.h"
#include "tier0/stacktools.h"
#include "generichash.h"
#include "tier1/utllinkedlist.h"
#include "filesystem/IQueuedLoader.h"
#include "filesystem/IXboxInstaller.h"
#include "tier2/tier2.h"
#include "tier1/lzmaDecoder.h"
#include "vstdlib/vstrtools.h"
#include "zip_utils.h"
#include "fmtstr.h"
#ifdef _X360
#include "xbox/xbox_launch.h"
#include "xbox/xbox_console.h"
#elif defined( _PS3 )
#include <cell/sysmodule.h>
#endif

#ifndef DEDICATED
#include "keyvaluescompiler.h"
#endif
#include "ifilelist.h"

#ifdef IS_WINDOWS_PC
// Needed for getting file type string
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#endif

#if defined( _X360 )
#include "xbox\xbox_win32stubs.h"
#undef GetCurrentDirectory
#endif

#ifdef _PS3

#include "ps3/ps3_core.h"
#include "ps3_pathinfo.h"
#include "tls_ps3.h"
#include <cell/fios.h>

// extern bool g_bUseBdvdGameData;
#ifndef PLATFORM_EXT
#pragma message("PLATFORM_EXT define is missing, wtf?")
#define PLATFORM_EXT ".ps3"
#endif // ifndef PLATFORM_EXT

void getcwd(...) { AssertMsg(false, "getcwd does not exist on PS3\n"); }
bool SetupFios();
bool TeardownFios();

#endif // _PS3

#ifdef _X360
	#define FS_DVDDEV_REMAP_ROOT "d:"
	#define FS_DVDDEV_ROOT "d:\\dvddev"
	#define FS_EXCLUDE_PATHS_FILENAME "xbox_exclude_paths.txt"
#elif defined( _PS3 )
	#define FS_DVDDEV_REMAP_ROOT g_pPS3PathInfo->GameImagePath()
	#define FS_DVDDEV_ROOT "/app_home/dvddev"
	#define FS_EXCLUDE_PATHS_FILENAME "ps3_exclude_paths.txt"
#else
	#define FS_DVDDEV_REMAP_ROOT ""
	#define FS_DVDDEV_ROOT "dvddev???:::"
	#define FS_EXCLUDE_PATHS_FILENAME "allbad_exclude_paths.txt"
#endif

#ifdef _GAMECONSOLE
static bool IsDvdDevPathString( char const *szPath )
{
	if ( IsGameConsole() && StringAfterPrefix( szPath, FS_DVDDEV_ROOT ) &&
		szPath[ sizeof( FS_DVDDEV_ROOT ) - 1 ] == CORRECT_PATH_SEPARATOR )
	{
		return true;
	}
	else if ( IsX360() )
	{
		const char *pFirstDir = V_strstr( szPath, ":" );
		if ( pFirstDir )
		{
			// skip past colon/slash
			pFirstDir += 2;
			return ( V_strnicmp( pFirstDir, "dvddev", 6 ) == false );
		}
	}

	return false;
}
#else
#define IsDvdDevPathString( x ) false
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#pragma warning( disable : 4355 )  // warning C4355: 'this' : used in base member initializer list


ConVar fs_report_sync_opens( "fs_report_sync_opens", "0", FCVAR_RELEASE, "0:Off, 1:Always, 2:Not during map load" );
ConVar fs_report_sync_opens_callstack( "fs_report_sync_opens_callstack", "0", 0, "0 to not display the call-stack when we hit a fs_report_sync_opens warning. Set to 1 to display the call-stack." );
ConVar fs_report_long_reads( "fs_report_long_reads", "0", 0, "0:Off, 1:All (for tracking accumulated duplicate read times), >1:Microsecond threshold" );
ConVar fs_warning_mode( "fs_warning_mode", "0", 0, "0:Off, 1:Warn main thread, 2:Warn other threads"  );
ConVar fs_monitor_read_from_pack( "fs_monitor_read_from_pack", "0", 0, "0:Off, 1:Any, 2:Sync only" );

#if IsPlatformPS3()
ConVar fs_fios_spew_prefetches( "fs_fios_spew_prefetches", "0", 0, "Set this to 1 to output prefetch operations, otherwise set this to 0." );
ConVar fs_fios_enabled( "fs_fios_enabled", "0", 0, "Set this to 1 to enable FIOS, otherwise set this to 0." );
#endif

#define BSPOUTPUT	0	// bsp output flag -- determines type of fs_log output to generate


static void AddSeperatorAndFixPath( char *str );

// Case-insensitive symbol table for path IDs.
CUtlSymbolTableMT g_PathIDTable( 0, 32, true );

int g_iNextSearchPathID = 1;

#if defined (_PS3)
	
	// Copied from zip_utils.cpp (we don't want to add the file to the project (for now))
	BEGIN_BYTESWAP_DATADESC( ZIP_EndOfCentralDirRecord )
		DEFINE_FIELD( signature, FIELD_INTEGER ),
		DEFINE_FIELD( numberOfThisDisk, FIELD_SHORT ),
		DEFINE_FIELD( numberOfTheDiskWithStartOfCentralDirectory, FIELD_SHORT ),
		DEFINE_FIELD( nCentralDirectoryEntries_ThisDisk, FIELD_SHORT ),
		DEFINE_FIELD( nCentralDirectoryEntries_Total, FIELD_SHORT ),
		DEFINE_FIELD( centralDirectorySize, FIELD_INTEGER ),
		DEFINE_FIELD( startOfCentralDirOffset, FIELD_INTEGER ),
		DEFINE_FIELD( commentLength, FIELD_SHORT ),
	END_BYTESWAP_DATADESC()

	BEGIN_BYTESWAP_DATADESC( ZIP_FileHeader )
		DEFINE_FIELD( signature, FIELD_INTEGER ),
		DEFINE_FIELD( versionMadeBy, FIELD_SHORT ),
		DEFINE_FIELD( versionNeededToExtract, FIELD_SHORT ),
		DEFINE_FIELD( flags, FIELD_SHORT ),
		DEFINE_FIELD( compressionMethod, FIELD_SHORT ),
		DEFINE_FIELD( lastModifiedTime, FIELD_SHORT ),
		DEFINE_FIELD( lastModifiedDate, FIELD_SHORT ),
		DEFINE_FIELD( crc32, FIELD_INTEGER ),
		DEFINE_FIELD( compressedSize, FIELD_INTEGER ),
		DEFINE_FIELD( uncompressedSize, FIELD_INTEGER ),
		DEFINE_FIELD( fileNameLength, FIELD_SHORT ),
		DEFINE_FIELD( extraFieldLength, FIELD_SHORT ),
		DEFINE_FIELD( fileCommentLength, FIELD_SHORT ),
		DEFINE_FIELD( diskNumberStart, FIELD_SHORT ),
		DEFINE_FIELD( internalFileAttribs, FIELD_SHORT ),
		DEFINE_FIELD( externalFileAttribs, FIELD_INTEGER ),
		DEFINE_FIELD( relativeOffsetOfLocalHeader, FIELD_INTEGER ),
	END_BYTESWAP_DATADESC()
#endif

void FixUpPathCaseForPS3(const char* pFilePath)
{
    char* prev_ptr = NULL;
    char* last_ptr = NULL;
	// This is really bad but the EA code does it so let's give it a try for now
	char* pFilePathNonConst = const_cast< char * >( pFilePath );
	char* ptr = pFilePathNonConst;

	//Convert all "\" to forward "/" and reformat relative paths
    while ( *ptr )
    {
        if ( *ptr == '\\' || *ptr == '/' )
        {
            *ptr='/';
            while(ptr[1]=='\\' || ptr[1] == '/') //Get rid of multiple slashes
            {
                strcpy(ptr+1,ptr+2);
            }
             if(strncmp(ptr+1,"..",2)==0 && ptr[3]!=0 && ptr[4]!=0 && last_ptr) //Some relative paths are used at runtime in Team Fortress
             {
                 //printf("Changing relative path %s to ... ", pFilePathNonConst);
                 strcpy(last_ptr+1, ptr+4); //Remove relative path
                 if(strncmp(last_ptr+1,"..",2)==0 && last_ptr[3]!=0 && last_ptr[4]!=0 && prev_ptr) //Sometimes get /../../ strings 
                 {
                     strcpy(prev_ptr+1, last_ptr+4);
                     if(strncmp(prev_ptr+1,"..",2)==0)
                     {
                         printf("Error: Can't process PS3 filenames containing /../../../\n");
                         Assert(0);
                     }
                 }
                 //printf("%s\n", pFilePathNonConst);
             }
            prev_ptr = last_ptr;
            last_ptr = ptr;
        }
        ptr++;
    }

    // terrible, terrible cruft: savegames (*.HL?) are written with uppercase from a million 
	// different places. For now, I'm just going to leave them alone here, rather than try
	// to find every single possible place that has a savegame go through it (as an alias 
	// of a copy of an alias of a string that's impossible to track by grepping). Y-U-C-K.
	if ( V_strstr(pFilePath, ".HL") )
	{
		return;
	}
    

	//PS3 file system is case sensitive (though this isn't enforced for /app_home/)
    if(pFilePathNonConst[0]=='/')
    {
		// if we're in the USRDIR directory, don't mess with paths up to that point
		char *pAfterUsrDir = V_strstr(pFilePathNonConst, "USRDIR");
		if ( pAfterUsrDir )
		{
			strlwr( pAfterUsrDir + 6 );
		}
        else if ((strnicmp(pFilePathNonConst,"/app_home/",10)==0) || (strnicmp(pFilePathNonConst,"/dev_bdvd/",10)==0) || (strnicmp(pFilePathNonConst,"/host_root/",11)==0))
        {
            strlwr(pFilePathNonConst+10);
        }
	    else if (strnicmp(pFilePathNonConst,"/dev_hdd0/game/",15)==0)
	    {		
		    strlwr(pFilePathNonConst+15);
	    }
	    else
	    {
		    //Lowercase everything after second "/" 
		    ptr=strchr(pFilePathNonConst,'/');
		    if (ptr) ptr=strchr(ptr+1,'/');
		    if (ptr) strlwr(ptr);
	    }
    }
    else
    {
	    //Lowercase everything
	    strlwr(pFilePathNonConst);
    }

}

// Look for cases like materials\\blah.vmt.
bool V_CheckDoubleSlashes( const char *pStr )
{
	int len = V_strlen( pStr );

	for ( int i=1; i < len-1; i++ )
	{
		if ( (pStr[i] == '/' || pStr[i] == '\\') && (pStr[i+1] == '/' || pStr[i+1] == '\\') )
			return true;
	}
	return false;
}

//
// Format relative filename when used under a search path
// allows "symlinking" official workshop locations into
// official locations in shipping depots.
//
// Returns passed pFileName if no symlinking occurs,
// or pointer to temp symlink buffer containing the symlink target.
//
static char const * V_FormatFilenameForSymlinking( char (&tempSymlinkBuffer)[MAX_PATH], char const *pFileName )
{
	if ( !pFileName )
		return NULL;

	if ( !V_strnicmp( pFileName, "maps", 4 ) &&
		 ( ( pFileName[4] == CORRECT_PATH_SEPARATOR ) || ( pFileName[4] == INCORRECT_PATH_SEPARATOR ) ) &&
		 !V_strnicmp( pFileName + 5, "workshop", 8 ) &&
		 ( ( pFileName[13] == CORRECT_PATH_SEPARATOR ) || ( pFileName[13] == INCORRECT_PATH_SEPARATOR ) ) )
	{
		//    maps/workshop/
		if ( ( false
			/** Removed for partner depot **/
			) &&
			( ( pFileName[23] == CORRECT_PATH_SEPARATOR ) || ( pFileName[23] == INCORRECT_PATH_SEPARATOR ) ) )
		{
			Q_snprintf( tempSymlinkBuffer, ARRAYSIZE( tempSymlinkBuffer ), "maps%c%s", pFileName[4], pFileName + 24 );
			return tempSymlinkBuffer;
		}
	}

	static bool bLoadBannedWords = ( !!CommandLine()->FindParm( "-usebanlist" ) ) || (!!CommandLine()->FindParm( "-perfectworld" ) );
	if ( bLoadBannedWords )
	{
		if ( !V_strnicmp( pFileName, "maps", 4 ) &&
			( ( pFileName[ 4 ] == CORRECT_PATH_SEPARATOR ) || ( pFileName[ 4 ] == INCORRECT_PATH_SEPARATOR ) ) &&
			!V_strnicmp( pFileName + 5, "ar_monastery", 12 ) )
		{
			//	maps/ar_monastery -> maps/ar_shoots
			Q_snprintf( tempSymlinkBuffer, ARRAYSIZE( tempSymlinkBuffer ), "maps%car_shoots%s", pFileName[ 4 ], pFileName + 17 );
			return tempSymlinkBuffer;
		}
	}

	return pFileName; // nothing symlinked here
}


// This can be used to easily fix a filename on the stack.
#define CHECK_DOUBLE_SLASHES( x ) Assert( V_CheckDoubleSlashes(x) == false );


// Win32 dedicated.dll contains both filesystem_steam.cpp and filesystem_stdio.cpp, so it has two
// CBaseFileSystem objects.  We'll let it manage BaseFileSystem() itself.
#if !( defined(_WIN32) && defined(DEDICATED) ) || defined( _PS3 )
static CBaseFileSystem *g_pBaseFileSystem;
CBaseFileSystem *BaseFileSystem()
{
	return g_pBaseFileSystem;
}
#endif

ConVar filesystem_buffer_size( "filesystem_buffer_size", "0", 0, "Size of per file buffers. 0 for none" );


class CFileHandleTimer : public CFastTimer
{
public:
	FileHandle_t m_hFile;
	char m_szName[ MAX_PATH ];
};

struct FileOpenDuplicateTime_t 
{
	char m_szName[ MAX_PATH ];
	int m_nLoadCount;
	float m_flAccumulatedMicroSeconds;

	FileOpenDuplicateTime_t()
	{
		m_szName[ 0 ] = '\0';
		m_nLoadCount = 0;
		m_flAccumulatedMicroSeconds = 0.0f;
	}
};
CUtlVector< FileOpenDuplicateTime_t* > g_FileOpenDuplicateTimes;	// Used to debug approximate time spent reading files duplicate times
CThreadFastMutex g_FileOpenDuplicateTimesMutex;

#if defined( TRACK_BLOCKING_IO )

// If we hit more than 100 items in a frame, we're probably doing a level load...
#define MAX_ITEMS	100

class CBlockingFileItemList : public IBlockingFileItemList
{
public:
	CBlockingFileItemList( CBaseFileSystem *fs ) 
		: 
		m_pFS( fs ),
		m_bLocked( false )
	{
	}

	// You can't call any of the below calls without calling these methods!!!!
	virtual void	LockMutex()
	{
		Assert( !m_bLocked );
		if ( m_bLocked )
			return;
		m_bLocked = true;
		m_pFS->BlockingFileAccess_EnterCriticalSection();
	}

	virtual void	UnlockMutex()
	{
		Assert( m_bLocked );
		if ( !m_bLocked )
			return;

		m_pFS->BlockingFileAccess_LeaveCriticalSection();
		m_bLocked = false;
	}

	virtual int First() const
	{
		if ( !m_bLocked )
		{
			Error( "CBlockingFileItemList::First() w/o calling EnterCriticalSectionFirst!" );
		}
		return m_Items.Head();
	}

	virtual int Next( int i ) const
	{
		if ( !m_bLocked )
		{
			Error( "CBlockingFileItemList::Next() w/o calling EnterCriticalSectionFirst!" );
		}
		return m_Items.Next( i ); 
	}

	virtual int InvalidIndex() const
	{
		return m_Items.InvalidIndex();
	}

	virtual const FileBlockingItem& Get( int index ) const
	{
		if ( !m_bLocked )
		{
			Error( "CBlockingFileItemList::Get( %d ) w/o calling EnterCriticalSectionFirst!", index );
		}
		return m_Items[ index ];
	}

	virtual void Reset()
	{
		if ( !m_bLocked )
		{
			Error( "CBlockingFileItemList::Reset() w/o calling EnterCriticalSectionFirst!" );
		}
		m_Items.RemoveAll();
	}

	void Add( const FileBlockingItem& item )
	{
		// Ack, should use a linked list probably...
		while ( m_Items.Count() > MAX_ITEMS )
		{
			m_Items.Remove( m_Items.Head() );
		}
		m_Items.AddToTail( item );
	}


private:
	CUtlLinkedList< FileBlockingItem, unsigned short >	m_Items;
	CBaseFileSystem						*m_pFS;
	bool								m_bLocked;
};
#endif

CUtlSymbol	CBaseFileSystem::m_GamePathID;
CUtlSymbol	CBaseFileSystem::m_BSPPathID;
DVDMode_t	CBaseFileSystem::m_DVDMode;
bool		CBaseFileSystem::m_bFoundXboxImageInCache;
bool		CBaseFileSystem::m_bLaunchedFromXboxHDD;		
bool		CBaseFileSystem::m_bDVDHosted;	
bool		CBaseFileSystem::m_bAllowXboxInstall;			
bool		CBaseFileSystem::m_bSearchPathsPatchedAfterInstall;

CUtlVector< FileNameHandle_t > CBaseFileSystem::m_ExcludeFilePaths;

CUtlSortVector< DLCContent_t, CDLCLess > CBaseFileSystem::m_DLCContents;
CUtlVector< DLCCorrupt_t > CBaseFileSystem::m_CorruptDLC;

CUtlBuffer	g_UpdateZipBuffer;
CUtlBuffer	g_XLSPPatchZipBuffer;

class CStoreIDEntry
{
public:
	CStoreIDEntry() {}
	CStoreIDEntry( const char *pPathIDStr, int storeID )
	{
		m_PathIDString = pPathIDStr;
		m_StoreID = storeID;
	}

public:
	CUtlSymbol	m_PathIDString;
	int			m_StoreID;
};


//-----------------------------------------------------------------------------
// CSimpleFileList (used by CFileCRCTracker).
// Uses a dictionary to refer to the list of files.
//-----------------------------------------------------------------------------

class CFileSystemReloadFileList : public IFileList
{
public:
	CFileSystemReloadFileList( CBaseFileSystem *pFileSystem )
	{
		m_pFileSystem = pFileSystem;
	}
	
	virtual void Release()
	{
		delete this;
	}
	
	// The engine is calling this for any files it wants to be pure.
	// Return true if this file should be reloaded based on its current state and the whitelist that we have now.
	virtual bool IsFileInList( const char *pFilename )
	{
		bool bRet = m_pFileSystem->ShouldGameReloadFile( pFilename );
		return bRet;
	}
	
private:
	CBaseFileSystem *m_pFileSystem;
};


static CStoreIDEntry* FindPrevFileByStoreID( CUtlDict< CUtlVector<CStoreIDEntry>* ,int> &filesByStoreID, const char *pFilename, const char *pPathIDStr, int foundStoreID )
{
	int iEntry = filesByStoreID.Find( pFilename );
	if ( iEntry == filesByStoreID.InvalidIndex() )
	{
		CUtlVector<CStoreIDEntry> *pList = new CUtlVector<CStoreIDEntry>; 
		pList->AddToTail( CStoreIDEntry(pPathIDStr, foundStoreID) );
		filesByStoreID.Insert( pFilename, pList );
		return NULL;
	}
	else
	{
		// Now is there a previous entry with a different path ID string and the same store ID?
		CUtlVector<CStoreIDEntry> *pList = filesByStoreID[iEntry]; 
		for ( int i=0; i < pList->Count(); i++ )
		{
			CStoreIDEntry &entry = pList->Element( i );
			if ( entry.m_StoreID == foundStoreID && V_stricmp( entry.m_PathIDString.String(), pPathIDStr ) != 0 )
				return &entry;
		}
		return NULL;
	}
}

//-----------------------------------------------------------------------------
// IIOStats implementation
//-----------------------------------------------------------------------------

#ifndef _CERT
class CIoStats : public IIoStats
{
public:
	CIoStats();
	~CIoStats();

	virtual void OnFileSeek( int nTimeInMs );
	virtual void OnFileRead( int nTimeInMs, int nBytesRead );
	virtual void OnFileOpen( const char * pFileName );
	virtual int GetNumberOfFileSeeks();
	virtual int GetTimeInFileSeek();
	virtual int GetNumberOfFileReads();
	virtual int GetTimeInFileReads();
	virtual int GetFileReadTotalSize();
	virtual int GetNumberOfFileOpens();
	void Reset();

private:
	CInterlockedInt m_nNumberOfFileSeeks;
	CInterlockedInt m_nTimeInFileSeek;
	CInterlockedInt m_nNumberOfFileReads;
	CInterlockedInt m_nTimeInFileRead;
	CInterlockedInt m_nFileReadTotalSize;
	CInterlockedInt m_nNumberOfFileOpens;
};

static CIoStats s_IoStats;

CIoStats::CIoStats()
:
m_nNumberOfFileSeeks( 0 ),
m_nTimeInFileSeek( 0 ),
m_nNumberOfFileReads( 0 ),
m_nTimeInFileRead( 0 ),
m_nFileReadTotalSize( 0 ),
m_nNumberOfFileOpens( 0 )
{
	// Do nothing...
}

CIoStats::~CIoStats()
{
	// Do nothing...
}

void CIoStats::OnFileSeek( int nTimeInMs )
{
	++m_nNumberOfFileSeeks;
	m_nTimeInFileSeek += nTimeInMs;
}

void CIoStats::OnFileRead( int nTimeInMs, int nBytesRead )
{
	++m_nNumberOfFileReads;
	m_nTimeInFileRead += nTimeInMs;
	m_nFileReadTotalSize += nBytesRead;
}

void CIoStats::OnFileOpen( const char * pFileName )
{
	++m_nNumberOfFileOpens;
}

int CIoStats::GetNumberOfFileSeeks()
{
	return m_nNumberOfFileSeeks;
}

int CIoStats::GetTimeInFileSeek()
{
	return m_nTimeInFileSeek;
}

int CIoStats::GetNumberOfFileReads()
{
	return m_nNumberOfFileReads;
}

int CIoStats::GetTimeInFileReads()
{
	return m_nTimeInFileRead;
}

int CIoStats::GetFileReadTotalSize()
{
	return m_nFileReadTotalSize;
}

int CIoStats::GetNumberOfFileOpens()
{
	return m_nNumberOfFileOpens;
}

void CIoStats::Reset()
{
	m_nNumberOfFileSeeks = 0;
	m_nTimeInFileSeek = 0;
	m_nNumberOfFileReads = 0;
	m_nTimeInFileRead = 0;
	m_nFileReadTotalSize = 0;
	m_nNumberOfFileOpens = 0;
}
#endif

//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------

CBaseFileSystem::CBaseFileSystem()
	: m_FileTracker( this ), m_FileWhitelist( NULL ),m_FileTracker2( this )
{
#if !( defined(_WIN32) && defined(DEDICATED) )
	g_pBaseFileSystem = this;
#endif
	g_pFullFileSystem = this;			// Left in for non tier Apps, tools, etc...

	m_WhitelistFileTrackingEnabled = -1;

	// If this changes then FileNameHandleInternal_t/FileNameHandle_t needs to be fixed!!!
	Assert( sizeof( CUtlSymbol ) == sizeof( short ) );

	// Clear out statistics
	memset( &m_Stats, 0, sizeof(m_Stats) );

	m_fwLevel    = FILESYSTEM_WARNING_REPORTUNCLOSED;
	m_pfnWarning = NULL;
	m_pLogFile   = NULL;
	m_bOutputDebugString = false;
	m_WhitelistSpewFlags = 0;
	m_DirtyDiskReportFunc = NULL;

	m_pThreadPool = NULL;
#if defined( TRACK_BLOCKING_IO )
	m_pBlockingItems = new CBlockingFileItemList( this );
	m_bBlockingFileAccessReportingEnabled = false;
	m_bAllowSynchronousLogging = true;
#endif

	m_iMapLoad = 0;

	m_DVDMode = DVDMODE_STRICT;
	if ( IsGameConsole() )
	{
		if ( CommandLine()->FindParm( "-dvddev" ) )
		{
			// FIXME: Is there a way of determining the host is a vista machine without
			// needing to explicitly say so on the command line?
			bool bIsVista = ( CommandLine()->CheckParm( "-vista" ) != NULL );
			if ( bIsVista )
			{
				m_DVDMode = DVDMODE_DEV_VISTA;
			}
			else
			{
				m_DVDMode = DVDMODE_DEV;
			}
		}
	}
#ifdef SUPPORT_IODELAY_MONITORING
	m_pDelayThread = NULL;
	m_flDelayLimit = 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::~CBaseFileSystem()
{
	m_PathIDInfos.PurgeAndDeleteElements();
#if defined( TRACK_BLOCKING_IO )
	delete m_pBlockingItems;
#endif

	// Free the whitelist.
	RegisterFileWhitelist( NULL, NULL, NULL );
}


//-----------------------------------------------------------------------------
// Methods of IAppSystem
//-----------------------------------------------------------------------------
void *CBaseFileSystem::QueryInterface( const char *pInterfaceName )
{
	// We also implement the IMatSystemSurface interface
	if (!Q_strncmp(	pInterfaceName, BASEFILESYSTEM_INTERFACE_VERSION, Q_strlen(BASEFILESYSTEM_INTERFACE_VERSION) + 1))
		return (IBaseFileSystem*)this;

	return NULL;
}


#ifdef _PS3
// this is strictly a debug variable used to catch errors where we load and tear down more than one filesystem:
static int s_PS3_libfs_ref_count = 0;
#endif
InitReturnVal_t CBaseFileSystem::Init()
{
	m_FileTracker2.InitAsyncThread();

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

#ifdef _PS3
	// load the PS3's file system module to memory
	AssertMsg1( s_PS3_libfs_ref_count == 0, "%d CBaseFileSystems were instantiated!\n", s_PS3_libfs_ref_count+1 );
	if ( cellSysmoduleLoadModule(CELL_SYSMODULE_FS) != CELL_OK )
	{
		Error( "Could not load system libfs!\n" );
	}
	else
	{
		s_PS3_libfs_ref_count += 1;
	}
#endif

	// This is a special tag to allow iterating just the BSP file, it doesn't show up in the list per se, but gets converted to "GAME" in the filter function
	m_BSPPathID = g_PathIDTable.AddString( "BSP" );
	m_GamePathID = g_PathIDTable.AddString( "GAME" );

	if ( getenv( "fs_debug" ) )
	{
		m_bOutputDebugString = true;
	}

	const char *logFileName = CommandLine()->ParmValue( "-fs_log" );
	if ( logFileName )
	{
		m_pLogFile = fopen( logFileName, "w" ); // STEAM OK
		if ( !m_pLogFile )
			return INIT_FAILED;
		fprintf( m_pLogFile, "@echo off\n" );
		fprintf( m_pLogFile, "setlocal\n" );
		const char *fs_target = CommandLine()->ParmValue( "-fs_target" );
		if( fs_target )
		{
			fprintf( m_pLogFile, "set fs_target=\"%s\"\n", fs_target );
		}
		fprintf( m_pLogFile, "if \"%%fs_target%%\" == \"\" goto error\n" );
		fprintf( m_pLogFile, "@echo on\n" );
	}

	InitAsync();

	if ( IsGameConsole() )
	{
		BuildExcludeList();
	}

#if defined( _X360 )
	MEM_ALLOC_CREDIT();

#if defined( _DEMO )
	// under demo conditions cannot allow install or use existing install
	// slam to expected state, do not override
	m_bLaunchedFromXboxHDD = false;
	m_bFoundXboxImageInCache = false;
	m_bAllowXboxInstall = false;
	m_bDVDHosted = true;
#else
	// determine the type of system where we launched from
	// this allows other systems (like the installer) to conditionalize the install process
	// MS may very well auto-install for us at a later date
	DWORD dwDummyFlags;
	char szFileSystemName[MAX_PATH];
	DWORD dwResult = GetVolumeInformation(
		"D:\\",
		NULL,
		0,
		NULL,
		0,
		&dwDummyFlags,
		szFileSystemName,
		sizeof( szFileSystemName ) );
	if ( dwResult != 0 )
	{
		m_bLaunchedFromXboxHDD = ( V_stricmp( szFileSystemName, "FATX" ) == 0 );
	}

	if ( m_DVDMode == DVDMODE_STRICT )
	{
		// must be in a strict dvd environment and not explicitly disabled
		if ( !CommandLine()->FindParm( "-noinstall" ) )
		{
			// the install is allowed if we launched from anywhere but the HDD
			// or it can be tested from the HDD by forcing with command line options
			m_bAllowXboxInstall = ( m_bLaunchedFromXboxHDD == false ) ||
								( CommandLine()->FindParm( "-installer" ) != 0 ) ||
								( CommandLine()->FindParm( "-install" ) != 0 );
			if ( m_bAllowXboxInstall )
			{
				// install may have already occurred
				m_bFoundXboxImageInCache = IsAlreadyInstalledToXboxHDDCache();
				if ( m_bFoundXboxImageInCache )
				{
					// we are using the installed image
					// no further installer activity is ever allowed (as the targets will be opened)
					m_bAllowXboxInstall = false;
				}
			}
		}

		// The update zip is designed to be held resident to avoid MU yanking or other transient issues.
		// The zip is expected to be < 100K and is a special compressed format.
		const char *pszUpdatePath = "UPDATE:\\update\\update" PLATFORM_EXT ".zip";
		if ( !IsCert() && !FileExists( pszUpdatePath ) )
		{
			// allows us to fallback and test when it is in the image
			pszUpdatePath = "D:\\update\\update" PLATFORM_EXT ".zip";
		}
		ReadFile( pszUpdatePath, NULL, g_UpdateZipBuffer, 0, 0 );
	}

	// if we are in any way HDD based, we do not want the reduced DVD experience
	m_bDVDHosted = ( m_bAllowXboxInstall || CommandLine()->FindParm( "-dvdtest" ) || 
					( !m_bLaunchedFromXboxHDD && !m_bFoundXboxImageInCache )  );
#endif
#elif defined( _PS3 )
	m_bLaunchedFromXboxHDD = true;
	m_bFoundXboxImageInCache = false;
	m_bAllowXboxInstall = false;
	m_bDVDHosted = false;

	SetupFios();
#endif

	return INIT_OK;
}

void CBaseFileSystem::Shutdown()
{
	ShutdownAsync();
	m_FileTracker2.ShutdownAsync();

#if !defined( _X360 ) && !defined( _PS3 )
	if( m_pLogFile )
	{
		if( CommandLine()->FindParm( "-fs_logbins" ) >= 0 )
		{
			char cwd[MAX_FILEPATH];
			getcwd( cwd, MAX_FILEPATH-1 );
			fprintf( m_pLogFile, "set binsrc=\"%s\"\n", cwd );
			fprintf( m_pLogFile, "mkdir \"%%fs_target%%\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\hl2.exe\" \"%%fs_target%%\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\hl2.dat\" \"%%fs_target%%\"\n" );
			fprintf( m_pLogFile, "mkdir \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\*.asi\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\materialsystem.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\shaderapidx9.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\filesystem_stdio.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\soundemittersystem.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\stdshader*.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\shader_nv*.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\launcher.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\engine.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\mss32.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\tier0.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\vgui2.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\vguimatsurface.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\voice_miles.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\vphysics.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\vstdlib.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\studiorender.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\bin\\vaudio_miles.dll\" \"%%fs_target%%\\bin\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\hl2\\resource\\*.ttf\" \"%%fs_target%%\\hl2\\resource\"\n" );
			fprintf( m_pLogFile, "copy \"%%binsrc%%\\hl2\\bin\\gameui.dll\" \"%%fs_target%%\\hl2\\bin\"\n" );
		}
		fprintf( m_pLogFile, "goto done\n" );
		fprintf( m_pLogFile, ":error\n" );
		fprintf( m_pLogFile, "echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\"\n" );
		fprintf( m_pLogFile, "echo ERROR: must set fs_target=targetpath (ie. \"set fs_target=u:\\destdir\")!\n" );
		fprintf( m_pLogFile, "echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\"\n" );
		fprintf( m_pLogFile, ":done\n" );
		fclose( m_pLogFile ); // STEAM OK
	}
#endif

	RemoveAllSearchPaths();
	Trace_DumpUnclosedFiles();

#if defined( _PS3 )
	TeardownFios();

	if ( --s_PS3_libfs_ref_count == 0 )
	{
		cellSysmoduleUnloadModule(CELL_SYSMODULE_FS);
	}
	else
	{
		AssertMsg( false, "More than one CBaseFileSystem was instantiated! Failsafe triggered to refcount sysutil libfs.\n" );
	}
#endif

	BaseClass::Shutdown();
}

void CBaseFileSystem::BuildExcludeList()
{
	if ( !IsGameConsole() || m_DVDMode != DVDMODE_DEV )
	{
		// xbox only
		return;
	}

	// clear prior results
	m_ExcludeFilePaths.Purge();

	// exclude paths are valid only in dvddev mode
	// the optional exclude path file should exist in the dvddev cache
	// populate the exclusion list
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( ReadFile( FS_DVDDEV_ROOT "/" FS_EXCLUDE_PATHS_FILENAME, NULL, buf, 0, 0 ) )
	{
		characterset_t breakSet;
		CharacterSetBuild( &breakSet, "" );
		char szPath[MAX_PATH];
		char szToken[MAX_PATH];
		for ( ;; )
		{
			int nTokenSize = buf.ParseToken( &breakSet, szToken, sizeof( szToken ) );
			if ( nTokenSize <= 0 )
			{
				break;
			}

			char *pToken = szToken;

#ifndef _CERT
			// At some point, we should also test that the corresponding file exist in the normal path
			// It is a bit more complicated on consoles due to the zip files.
			V_ComposeFileName( FS_DVDDEV_ROOT, pToken, szPath, sizeof( szPath ) );
			bool bExist = FileExists( szPath );
			if ( bExist == false)
			{
				Warning( "File '%s' does not exist. Won't be added to dvddev.\n", szPath );
				continue;
			}
#endif

			V_ComposeFileName( FS_DVDDEV_REMAP_ROOT, pToken, szPath, sizeof( szPath ) );
			FileNameHandle_t hFileName = FindOrAddFileName( szPath );
			if ( m_ExcludeFilePaths.Find( hFileName ) == -1 )
			{
				m_ExcludeFilePaths.AddToTail( hFileName );
				DevMsg( "File '%s' added to dvddev.\n", szPath );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Computes a full write path
//-----------------------------------------------------------------------------
inline void CBaseFileSystem::ComputeFullWritePath( char* pDest, int maxlen, const char *pRelativePath, const char *pWritePathID )
{
	Q_strncpy( pDest, GetWritePath( pRelativePath, pWritePathID ), maxlen );
	Q_strncat( pDest, pRelativePath, maxlen, COPY_ALL_CHARACTERS );
	Q_FixSlashes( pDest );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src1 - 
//			src2 - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::OpenedFileLessFunc( COpenedFile const& src1, COpenedFile const& src2 )
{
	return src1.m_pFile < src2.m_pFile;
}


void CBaseFileSystem::InstallDirtyDiskReportFunc( FSDirtyDiskReportFunc_t func )
{
	m_DirtyDiskReportFunc = func;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fullpath - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::LogAccessToFile( char const *accesstype, char const *fullpath, char const *options )
{
	LOCAL_THREAD_LOCK();

	if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES )
	{
		FileSystemWarning( FILESYSTEM_WARNING_REPORTALLACCESSES, "---FS%s:  %s %s (%.3f)\n", ThreadInMainThread() ? "" : "[a]", accesstype, fullpath, Plat_FloatTime() );
	}

	int c = m_LogFuncs.Count();
	if ( !c )
		return;

	for ( int i = 0; i < c; ++i )
	{
		( m_LogFuncs[ i ] )( fullpath, options );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*options - 
// Output : FILE
//-----------------------------------------------------------------------------
FILE *CBaseFileSystem::Trace_FOpen( const char *filename, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo )
{
	if ( m_NonexistingFilesExtensions.GetNumStrings() )
	{
		if ( char const *pszExt = V_GetFileExtension( filename ) )
		{
			AUTO_LOCK( m_OpenedFilesMutex );
			UtlSymId_t symFound = m_NonexistingFilesExtensions.Find( pszExt );
			if ( ( symFound != UTL_INVAL_SYMBOL ) && m_NonexistingFilesExtensions[ symFound ] )
			{
				DevWarning( "Known VPK-only extension [%s], file {%s} declared missing. Run with -fullfsvalveds to search filesystem.\n", pszExt, filename );
				return NULL;
			}
		}
	}

#ifdef NONEXISTING_FILES_CACHE_SUPPORT
	bool bReadOnlyRequest = !strchr(options,'w') && !strchr(options,'a') && !strchr(options,'+');

	static bool s_bNeverCheckFS = !CommandLine()->FindParm( "-alwayscheckfs" );
	if ( s_bNeverCheckFS )
	{
		AUTO_LOCK( m_OpenedFilesMutex );

		UtlSymId_t symFound = m_NonexistingFilesCache.Find( filename );
		if ( symFound != UTL_INVAL_SYMBOL )
		{
			double &refCacheTime = m_NonexistingFilesCache[ symFound ];

			if ( bReadOnlyRequest )
			{
				if ( refCacheTime != 0.0 )
				{
					Warning( "Trace_FOpen: duplicate request for missing file: %s [was missing %.3f sec ago]\n", filename, Plat_FloatTime() - refCacheTime );
					return NULL;	// we looked for this file already, it doesn't exist
				}
				else
				{
					// This file was previously missing, but a write request was made and could have created the file, so this read call should fall through
				}
			}
			else
			{
				// This is possibly a write request, so remove cached ENOENT record
				Warning( "Trace_FOpen: possibly write request for missing file: %s [was missing %.3f sec ago]\n", filename, Plat_FloatTime() - refCacheTime );
				refCacheTime = 0.0f;
			}
		}
		else
		{
			// Nothing known about this file, fall through into syscall to fopen
		}
	}
#endif

	AUTOBLOCKREPORTER_FN( Trace_FOpen, this, true, filename, FILESYSTEM_BLOCKING_SYNCHRONOUS, FileBlockingItem::FB_ACCESS_OPEN );

	FILE *fp = FS_fopen( filename, options, flags, size, pInfo );

#ifdef NONEXISTING_FILES_CACHE_SUPPORT
	if ( s_bNeverCheckFS && !fp && bReadOnlyRequest )
	{
		double dblNow = Plat_FloatTime();

		AUTO_LOCK( m_OpenedFilesMutex );
		m_NonexistingFilesCache[ filename ] = dblNow;
		Warning( "Trace_FOpen: missing file: %s [will never check again]\n", filename );
	}
#endif

	if ( fp )
	{
		if ( options[0] == 'r' )
		{
			FS_setbufsize(fp, filesystem_buffer_size.GetInt() );
		}
		else
		{
			FS_setbufsize(fp, 32*1024 );
		}

		AUTO_LOCK( m_OpenedFilesMutex );
		COpenedFile file;

		file.SetName( filename );
		file.m_pFile = fp;

		m_OpenedFiles.AddToTail( file );

		LogAccessToFile( "open", filename, options );
	}

	return fp;
}

void CBaseFileSystem::GetFileNameForHandle( FileHandle_t handle, char *buf, size_t buflen )
{
	V_strncpy( buf, "Unknown", buflen );
	/*
	CFileHandle *fh = ( CFileHandle *)handle;
	if ( !fh )
	{
		buf[ 0 ] = 0;
		return;
	}

	// Pack file filehandles store the underlying name for convenience
	if ( fh->IsPack() )
	{
		Q_strncpy( buf, fh->Name(), buflen );
		return;
	}

	AUTO_LOCK( m_OpenedFilesMutex );

	COpenedFile file;
	file.m_pFile = fh->GetFileHandle();

	int result = m_OpenedFiles.Find( file );
	if ( result != -1 )
	{
		COpenedFile found = m_OpenedFiles[ result ];
		Q_strncpy( buf, found.GetName(), buflen );
	}
	else
	{
		buf[ 0 ] = 0;
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fp - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Trace_FClose( FILE *fp )
{
	if ( fp )
	{
		m_OpenedFilesMutex.Lock();

		COpenedFile file;
		file.m_pFile = fp;

		int result = m_OpenedFiles.Find( file );
		if ( result != -1 /*m_OpenedFiles.InvalidIdx()*/ )
		{
			COpenedFile found = m_OpenedFiles[ result ];
			if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES )
			{
				FileSystemWarning( FILESYSTEM_WARNING_REPORTALLACCESSES, "---FS%s:  close %s %p %i (%.3f)\n", ThreadInMainThread() ? "" : "[a]", found.GetName(), fp, m_OpenedFiles.Count(), Plat_FloatTime() );
			}
			m_OpenedFiles.Remove( result );
		}
		else
		{
			Assert( 0 );

			if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES )
			{
				FileSystemWarning( FILESYSTEM_WARNING_REPORTALLACCESSES, "Tried to close unknown file pointer %p\n", fp );
			}
		}

		m_OpenedFilesMutex.Unlock();

		FS_fclose( fp );
	}
}


void CBaseFileSystem::Trace_FRead( int size, FILE* fp )
{
	if ( !fp || m_fwLevel < FILESYSTEM_WARNING_REPORTALLACCESSES_READ )
		return;

	AUTO_LOCK( m_OpenedFilesMutex );

	COpenedFile file;
	file.m_pFile = fp;

	int result = m_OpenedFiles.Find( file );

	if( result != -1 )
	{
		COpenedFile found = m_OpenedFiles[ result ];
		
		FileSystemWarning( FILESYSTEM_WARNING_REPORTALLACCESSES_READ, "---FS%s:  read %s %i %p (%.3f)\n", ThreadInMainThread() ? "" : "[a]", found.GetName(), size, fp, Plat_FloatTime()  );
	} 
	else
	{
		FileSystemWarning( FILESYSTEM_WARNING_REPORTALLACCESSES_READ, "Tried to read %i bytes from unknown file pointer %p\n", size, fp );
		
	}
}

void CBaseFileSystem::Trace_FWrite( int size, FILE* fp )
{
	if ( !fp || m_fwLevel < FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE )
		return;

	COpenedFile file;
	file.m_pFile = fp;

	AUTO_LOCK( m_OpenedFilesMutex );

	int result = m_OpenedFiles.Find( file );

	if( result != -1 )
	{
		COpenedFile found = m_OpenedFiles[ result ];

		FileSystemWarning( FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE, "---FS%s:  write %s %i %p\n", ThreadInMainThread() ? "" : "[a]", found.GetName(), size, fp  );
	} 
	else
	{
		FileSystemWarning( FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE, "Tried to write %i bytes from unknown file pointer %p\n", size, fp );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Trace_DumpUnclosedFiles( void )
{
	AUTO_LOCK( m_OpenedFilesMutex );
	for ( int i = 0 ; i < m_OpenedFiles.Count(); i++ )
	{
		COpenedFile *found = &m_OpenedFiles[ i ];

		if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTUNCLOSED )
		{
			FileSystemWarning( FILESYSTEM_WARNING_REPORTUNCLOSED, "File %s was never closed\n", found->GetName() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::PrintOpenedFiles( void )
{
	FileWarningLevel_t saveLevel = m_fwLevel;
	m_fwLevel = FILESYSTEM_WARNING_REPORTUNCLOSED;
	Trace_DumpUnclosedFiles();
	m_fwLevel = saveLevel;
}

void CBaseFileSystem::AddVPKFile( char const *pBasename, SearchPathAdd_t addType )
{
	// Ensure that the passed in file name has a .vpk extension. Otherwise the check
	// for already having the .vpk file will always fail and the same file may get
	// added dozens of times, wasting hundreds of MB of memory.
	const char *pExtension = strrchr( pBasename, '.' );
	Assert( pExtension && V_strcmp( pExtension, ".vpk" ) == 0 );
	if ( !pExtension || V_strcmp( pExtension, ".vpk" ) )
	{
		Warning( "Extensionless VPK file '%s' specified. Ignoring.\n", pBasename );
		return;
	}

#ifdef SUPPORT_VPK
	char nameBuf[MAX_PATH];
	Q_MakeAbsolutePath( nameBuf, sizeof( nameBuf ), pBasename );
#ifdef _WIN32
	Q_strlower( nameBuf );
#endif
	Q_FixSlashes( nameBuf );
	// see if we already have this vpk file
	for( int i = 0; i < m_VPKFiles.Count(); i++ )
	{
		if ( ! V_strcmp( m_VPKFiles[i]->FullPathName(), nameBuf ) )
		{
			return;											// already have this one
		}
	}
	char pszFName[MAX_PATH];
	CPackedStore *pNew = new CPackedStore( nameBuf, pszFName, this ); 
	pNew->RegisterFileTracker( (IThreadedFileMD5Processor *)&m_FileTracker2 );
	if ( pNew->IsEmpty() )
	{
		delete pNew;
	}
	else
	{
		if ( PATH_ADD_TO_TAIL == addType )
		{
			m_VPKFiles.AddToTail( pNew );
		}
		else
		{
			m_VPKFiles.AddToHead( pNew );
		}
		char szRelativePathName[512];
		Assert ( V_IsAbsolutePath( pNew->FullPathName() ) );
		char szBasePath[MAX_PATH];
		V_strncpy( szBasePath, pNew->FullPathName(), sizeof(szBasePath) );
		V_StripFilename( szBasePath );
		V_StripLastDir( szBasePath, sizeof(szBasePath) );
		V_MakeRelativePath( pNew->FullPathName(), szBasePath, szRelativePathName, sizeof( szRelativePathName ) );
		pNew->m_PackFileID = m_FileTracker2.NotePackFileOpened( pszFName, szRelativePathName, "GAME", 0 );		
	}
#endif
}

void CBaseFileSystem::RemoveVPKFile( char const *pBasename )
{
#ifdef SUPPORT_VPK
	char nameBuf[MAX_PATH];
	Q_MakeAbsolutePath( nameBuf, sizeof( nameBuf ), pBasename );
	Q_strlower( nameBuf );
	Q_FixSlashes( nameBuf );
	// see if we already have this vpk file
	for( int i = 0; i < m_VPKFiles.Count(); i++ )
	{
		if ( ! V_strcmp( m_VPKFiles[i]->FullPathName(), nameBuf ) )
		{
			delete m_VPKFiles[i];
			m_VPKFiles.Remove( i );
			break;											// already have this one
		}
	}
#endif
}

void CBaseFileSystem::GetVPKFileNames( CUtlVector<CUtlString> &destVector )
{
#ifdef SUPPORT_VPK
	for( int i = 0; i < m_VPKFiles.Count(); i++ )
	{
		destVector.AddToTail( CUtlString( m_VPKFiles[i]->FullPathName() ) );
	}
#endif	
}

//-----------------------------------------------------------------------------
// Purpose: Adds the specified pack file to the list
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::AddPackFile( const char *pFileName, const char *pathID )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	AsyncFinishAll();
	return AddPackFileFromPath( "", pFileName, true, pathID );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a pack file from the specified path
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::AddPackFileFromPath( const char *pPath, const char *pakfile, bool bCheckForAppendedPack, const char *pathID )
{
	char fullpath[ MAX_PATH ];
	_snprintf( fullpath, sizeof(fullpath), "%s%s", pPath, pakfile );
	Q_FixSlashes( fullpath );

	struct	_stat buf;
	if ( FS_stat( fullpath, &buf ) == -1 )
		return false;

	CPackFile *pf = new CZipPackFile( this );
	pf->m_hPackFileHandleFS = Trace_FOpen( fullpath, "rb", 0, NULL );

	if ( pf->m_hPackFileHandleFS )
	{
		// Get the length of the pack file:
		FS_fseek( ( FILE * )pf->m_hPackFileHandleFS, 0, FILESYSTEM_SEEK_TAIL );
		int64 len = FS_ftell( ( FILE * )pf->m_hPackFileHandleFS );
		FS_fseek( ( FILE * )pf->m_hPackFileHandleFS, 0, FILESYSTEM_SEEK_HEAD );

		if ( !pf->Prepare( len ) )
		{
			// Failed for some reason, ignore it
			Trace_FClose( pf->m_hPackFileHandleFS );
			pf->m_hPackFileHandleFS = NULL;
			delete pf;

			return false;
		}
	}
#ifdef SUPPORT_VPK
	else
	{
		pf->m_hPackFileHandleVPK = FindFileInVPKs( fullpath );

		if ( pf->m_hPackFileHandleVPK )
		{
			// Get the length of the pack file:
			pf->m_hPackFileHandleVPK.Seek( 0, FILESYSTEM_SEEK_TAIL );
			int64 len = pf->m_hPackFileHandleVPK.Tell();
			pf->m_hPackFileHandleVPK.Seek( 0, FILESYSTEM_SEEK_HEAD );

			if ( !pf->Prepare( len ) )
			{
				// Failed for some reason, ignore it
				delete pf;

				return false;
			}
		}
	}
#endif

	// Add this pack file to the search path:
	CSearchPath *sp = &m_SearchPaths[ m_SearchPaths.AddToTail() ];
	pf->SetPath( sp->GetPath() );
	pf->m_lPackFileTime = GetFileTime( pakfile );

	sp->SetPath( pPath );
	sp->m_pPathIDInfo->SetPathID( pathID );
	sp->SetPackFile( pf );

	return true;
}

// Read a bit of the file from the pack file:
int CPackFileHandle::Read( void* pBuffer, int nDestSize, int nBytes )
{
	// Clamp nBytes to not go past the end of the file (async is still possible due to nDestSize)
	if ( nBytes + m_nFilePointer > m_nLength )
	{
		nBytes = m_nLength - m_nFilePointer;
	}

	// Seek to the given file pointer and read
	int nBytesRead = m_pOwner->ReadFromPack( m_nIndex, pBuffer, nDestSize, nBytes, m_nBase + m_nFilePointer );

	m_nFilePointer += nBytesRead;

	return nBytesRead;
}

// Seek around inside the pack:
int CPackFileHandle::Seek( int nOffset, int nWhence )
{
	if ( nWhence == SEEK_SET )
	{
		m_nFilePointer = nOffset;
	}
	else if ( nWhence == SEEK_CUR )
	{
		m_nFilePointer += nOffset;
	}
	else if ( nWhence == SEEK_END )
	{
		m_nFilePointer = m_nLength + nOffset;
	}

	// Clamp the file pointer to the actual bounds of the file:
	if ( m_nFilePointer > m_nLength )
	{
		m_nFilePointer = m_nLength;
	}

	return m_nFilePointer;
}

//-----------------------------------------------------------------------------
// Low Level I/O routine for reading from pack files.
// Offsets all reads by the base of the pack file as needed.
// Return bytes read.
//-----------------------------------------------------------------------------
int CPackFile::ReadFromPack( int nIndex, void* buffer, int nDestBytes, int nBytes, int64 nOffset )
{
	m_mutex.Lock();

	if ( fs_monitor_read_from_pack.GetInt() == 1 || ( fs_monitor_read_from_pack.GetInt() == 2 && ThreadInMainThread() ) )
	{
		// spew info about real i/o request
		char szName[MAX_PATH];
		IndexToFilename( nIndex, szName, sizeof( szName ) );
		Msg( "Read From Pack: Sync I/O: Requested:%7d, Offset:0x%16.16llx, %s\n", nBytes, m_nBaseOffset + nOffset, szName );
	}

	int nBytesRead = 0;
	// Seek to the start of the read area and perform the read: TODO: CHANGE THIS INTO A CFileHandle
	if ( m_hPackFileHandleFS )
	{
		m_fs->FS_fseek( m_hPackFileHandleFS, m_nBaseOffset + nOffset, SEEK_SET );
		nBytesRead = m_fs->FS_fread( buffer, nDestBytes, nBytes, m_hPackFileHandleFS );
	}
#ifdef SUPPORT_VPK
	else if ( m_hPackFileHandleVPK )
	{
		// We're a packfile embedded in a VPK
		m_hPackFileHandleVPK.Seek( m_nBaseOffset + nOffset, FILESYSTEM_SEEK_HEAD );
		nBytesRead = m_hPackFileHandleVPK.Read( buffer, nBytes );
	}
#endif
	else
	{
		Error("Failure in CPackFile::ReadFromPack(): m_hPackFileHandleFS and/or m_hPackFileHandleVPK are uninitialized - The file open call(s) likely failed\n");
	}
	m_mutex.Unlock();

	return nBytesRead;
}

//-----------------------------------------------------------------------------
// Open a file inside of a pack file.
//-----------------------------------------------------------------------------
CFileHandle *CPackFile::OpenFile( const char *pFileName, const char *pOptions )
{
	int nIndex, nLength;
	int64 nPosition;

	// find the file's location in the pack
	if ( FindFile( pFileName, nIndex, nPosition, nLength ) )
	{
		m_mutex.Lock();
#ifdef SUPPORT_VPK
		if ( m_nOpenFiles == 0 && m_hPackFileHandleFS == NULL && !m_hPackFileHandleVPK )
#else
		if ( m_nOpenFiles == 0 && m_hPackFileHandleFS == NULL )
#endif
		{
			// Try to open it as a regular file first
			m_hPackFileHandleFS = m_fs->Trace_FOpen( m_ZipName, "rb", 0, NULL );

#ifdef SUPPORT_VPK
			// Try opening from a VPK
			if ( !m_hPackFileHandleFS )
			{
				m_hPackFileHandleVPK = m_fs->FindFileInVPKs( pFileName );
			}
#endif
		}
		m_nOpenFiles++;
		m_mutex.Unlock();
		CPackFileHandle* ph = new CPackFileHandle( this, nPosition, nLength, nIndex );
		CFileHandle *fh = new CFileHandle( m_fs );
		fh->m_pPackFileHandle = ph;
		fh->m_nLength = nLength;

		// The default mode for fopen is text, so require 'b' for binary
		if ( strstr( pOptions, "b" ) == NULL )
		{
			fh->m_type = FT_PACK_TEXT;
		}
		else
		{
			fh->m_type = FT_PACK_BINARY;
		}

		fh->SetName( pFileName );
		return fh;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//	Get a directory entry from a pack's preload section
//-----------------------------------------------------------------------------
ZIP_PreloadDirectoryEntry* CZipPackFile::GetPreloadEntry( int nEntryIndex )  
{
	if ( !m_pPreloadHeader )
	{
		return NULL;
	}

	// If this entry doesn't have a corresponding preload entry, fail.
	if ( m_PackFiles[nEntryIndex].m_nPreloadIdx == INVALID_PRELOAD_ENTRY )
	{
		return NULL;
	}
	
	return &m_pPreloadDirectory[m_PackFiles[nEntryIndex].m_nPreloadIdx];
}

//-----------------------------------------------------------------------------
//	Read a file from the pack
//-----------------------------------------------------------------------------
int CZipPackFile::ReadFromPack( int nEntryIndex, void* pBuffer, int nDestBytes, int nBytes, int64 nOffset )
{
	if ( nEntryIndex >= 0 )
	{
		if ( nBytes <= 0 ) 
		{
			return 0;
		}

		// X360TBD: This is screwy, it works because m_nBaseOffset is 0 for preload capable zips
		// It comes into play for files out of the embedded bsp zip,
		// this hackery is a pre-bias expecting ReadFromPack() do a symmetric post bias, yuck.
		nOffset -= m_nBaseOffset;

		// Attempt to satisfy request from possible preload section, otherwise fall through
		// A preload entry may be compressed
		ZIP_PreloadDirectoryEntry *pPreloadEntry = GetPreloadEntry( nEntryIndex );
		if ( pPreloadEntry )
		{
			// convert the absolute pack file position to a local file position 
			int nLocalOffset = nOffset - m_PackFiles[nEntryIndex].m_nPosition;
			byte *pPreloadData = (byte*)m_pPreloadData + pPreloadEntry->DataOffset;

			CLZMA lzma;
			if ( lzma.IsCompressed( pPreloadData ) )
			{
				unsigned int actualSize = lzma.GetActualSize( pPreloadData );
				if ( nLocalOffset + nBytes <= (int)actualSize )
				{
					// satisfy from compressed preload
					if ( fs_monitor_read_from_pack.GetInt() == 1 )
					{
						char szName[MAX_PATH];
						IndexToFilename( nEntryIndex, szName, sizeof( szName ) );
						Msg( "Read From Pack: [Preload] Requested:%d, Compressed:%d, %s\n", nBytes, pPreloadEntry->Length, szName );
					}

					if ( nLocalOffset == 0 && nDestBytes >= (int)actualSize && nBytes == (int)actualSize )
					{
						// uncompress directly into caller's buffer
						lzma.Uncompress( (unsigned char *)pPreloadData, (unsigned char *)pBuffer );
						return nBytes;
					}
			
					// uncompress into temporary memory
					CUtlMemory< byte > tempMemory;
					tempMemory.EnsureCapacity( actualSize );
					lzma.Uncompress( pPreloadData, tempMemory.Base() );
					// copy only what caller expects
					V_memcpy( pBuffer, (byte*)tempMemory.Base() + nLocalOffset, nBytes );
					return nBytes;
				}
			}
			else if ( nLocalOffset + nBytes <= (int)pPreloadEntry->Length )
			{
				// satisfy from uncompressed preload
				if ( fs_monitor_read_from_pack.GetInt() == 1 )
				{
					char szName[MAX_PATH];
					IndexToFilename( nEntryIndex, szName, sizeof( szName ) );
					Msg( "Read From Pack: [Preload] Requested:%d, Total:%d, %s\n", nBytes, pPreloadEntry->Length, szName );
				}

				V_memcpy( pBuffer, pPreloadData + nLocalOffset, nBytes );
				return nBytes;
			}
		}
	}

	// fell through as a direct request from within the pack
	// intercept to possible embedded section
	if ( m_pSection )
	{
		// a section is a special update zip that has no files, only preload
		// it has to be in the section
		V_memcpy( pBuffer, (byte*)m_pSection + nOffset, nBytes );
		return nBytes;
	}

	return CPackFile::ReadFromPack( nEntryIndex, pBuffer, nDestBytes, nBytes, nOffset );	
}

//-----------------------------------------------------------------------------
//	Gets size, position, and index for a file in the pack.
//-----------------------------------------------------------------------------
bool CZipPackFile::GetOffsetAndLength( const char *pFileName, int &nBaseIndex, int64 &nFileOffset, int &nLength )
{
	CZipPackFile::CPackFileEntry lookup;
	lookup.m_HashName = HashStringCaselessConventional( pFileName );

	int idx = m_PackFiles.Find( lookup );
	if ( -1 != idx  )
	{
		nFileOffset = m_PackFiles[idx].m_nPosition;
		nLength = m_PackFiles[idx].m_nLength;
		nBaseIndex = idx;
		return true;
	}

	return false;
}

bool CZipPackFile::IndexToFilename( int nIndex, char *pBuffer, int nBufferSize )
{
	if ( nIndex >= 0 )
	{
		m_fs->String( m_PackFiles[nIndex].m_hDebugFilename, pBuffer, nBufferSize );
		return true;
	}

	Q_strncpy( pBuffer, "unknown", nBufferSize );

	return false;
}

//-----------------------------------------------------------------------------
//	Find a file in the pack.
//-----------------------------------------------------------------------------
bool CZipPackFile::FindFile( const char *pFilename, int &nIndex, int64 &nOffset, int &nLength )
{
	char szCleanName[MAX_FILEPATH];
	Q_strncpy( szCleanName, pFilename, sizeof( szCleanName ) );
#ifdef _WIN32
	Q_strlower( szCleanName );
#endif
	Q_FixSlashes( szCleanName );
 
	if ( !Q_RemoveDotSlashes( szCleanName ) )
	{
		return false;
	}
	
    bool bFound = GetOffsetAndLength( szCleanName, nIndex, nOffset, nLength );

	nOffset += m_nBaseOffset;
	return bFound;
}


//-----------------------------------------------------------------------------
//	Set up the preload section
//-----------------------------------------------------------------------------
void CZipPackFile::SetupPreloadData()
{
	if ( m_pPreloadHeader || !m_nPreloadSectionSize )
	{
		// already loaded or not availavble
		return;
	}

	MEM_ALLOC_CREDIT_( "xZip" );

	void *pPreload;
	if ( m_pSection )
	{
		pPreload = (byte*)m_pSection + m_nPreloadSectionOffset;
	}
	else
	{
		pPreload = malloc( m_nPreloadSectionSize );
		if ( !pPreload )
		{
			return;
		}

		if ( IsGameConsole() )
		{
			// 360 XZips are always dvd aligned
			Assert( ( m_nPreloadSectionSize % XBOX_DVD_SECTORSIZE ) == 0 );
			Assert( ( m_nPreloadSectionOffset % XBOX_DVD_SECTORSIZE ) == 0 );
		}

		// preload data is loaded as a single unbuffered i/o operation
		ReadFromPack( -1, pPreload, -1, m_nPreloadSectionSize, m_nPreloadSectionOffset );
	}

	// setup the header
	m_pPreloadHeader = (ZIP_PreloadHeader *)pPreload;

	// setup the preload directory
	m_pPreloadDirectory = (ZIP_PreloadDirectoryEntry *)((byte *)m_pPreloadHeader + sizeof( ZIP_PreloadHeader ) );

	// setup the remap table
	m_pPreloadRemapTable = (unsigned short *)((byte *)m_pPreloadDirectory + m_pPreloadHeader->PreloadDirectoryEntries * sizeof( ZIP_PreloadDirectoryEntry ) );

	// set the preload data base
	m_pPreloadData = (byte *)m_pPreloadRemapTable + m_pPreloadHeader->DirectoryEntries * sizeof( unsigned short );
}

void CZipPackFile::DiscardPreloadData()
{
	if ( !m_pPreloadHeader )
	{
		// already discarded
		return;
	}

	// a section is an alias, the header becomes an alias, not owned memory
	if ( !m_pSection )
	{
		free( m_pPreloadHeader );
	}
	m_pPreloadHeader = NULL;
}

//-----------------------------------------------------------------------------
//	Parse the zip file to build the file directory and preload section
//-----------------------------------------------------------------------------
bool CZipPackFile::Prepare( int64 fileLen, int64 nFileOfs )
{
	if ( !fileLen || fileLen < sizeof( ZIP_EndOfCentralDirRecord ) )
	{
		// nonsense zip
		return false;
	}

	// Pack files are always little-endian
	m_swap.ActivateByteSwapping( IsX360() || IsPS3());

	m_FileLength = fileLen;
	m_nBaseOffset = nFileOfs;

	ZIP_EndOfCentralDirRecord rec = { 0 };

	// Find and read the central header directory from its expected position at end of the file
	bool bCentralDirRecord = false;
	int64 offset = fileLen - sizeof( ZIP_EndOfCentralDirRecord );

	// 360 can have an incompatible format
	bool bCompatibleFormat = true;
	if ( IsGameConsole() )
	{
		// 360 has dependable exact zips, backup to handle possible xzip format
		if ( offset - XZIP_COMMENT_LENGTH >= 0 )
		{
			offset -= XZIP_COMMENT_LENGTH;
		}
	
		// single i/o operation, scanning forward
		unsigned int nBytes = (unsigned int)( fileLen - offset );
		char *pTemp = (char *)_alloca( nBytes );
		ReadFromPack( -1, pTemp, -1, nBytes, offset );
		while ( offset <= fileLen - sizeof( ZIP_EndOfCentralDirRecord ) )
		{
			memcpy( &rec, pTemp, sizeof( ZIP_EndOfCentralDirRecord ) );
			m_swap.SwapFieldsToTargetEndian( &rec );
			if ( rec.signature == PKID( 5, 6 ) )
			{
				bCentralDirRecord = true;
				if ( rec.commentLength >= 4 )
				{
					char *pComment = pTemp + sizeof( ZIP_EndOfCentralDirRecord );
					if ( !V_strnicmp( pComment, "XZP", 3 ) && pComment[3] != '1' )
					{
						bCompatibleFormat = false;
					}
				}
				break;
			}
			offset++;
			pTemp++;
		}
	}
	else
	{
		// scan entire file from expected location for central dir
		for ( ; offset >= 0; offset-- )
		{
			ReadFromPack( -1, (void*)&rec, -1, sizeof( rec ), offset );
			m_swap.SwapFieldsToTargetEndian( &rec );
			if ( rec.signature == PKID( 5, 6 ) )
			{
				bCentralDirRecord = true;
				break;
			}
		}
	}
	Assert( bCentralDirRecord );
	if ( !bCentralDirRecord )
	{
		// no zip directory, bad zip
		return false;
	}

	int numFilesInZip = rec.nCentralDirectoryEntries_Total;
	if ( numFilesInZip <= 0 )
	{
		// empty valid zip
		return true;
	}

	int firstFileIdx = 0;

	MEM_ALLOC_CREDIT();

	// read central directory into memory and parse
	CUtlBuffer zipDirBuff( 0, rec.centralDirectorySize, 0 );
	zipDirBuff.EnsureCapacity( rec.centralDirectorySize );
	zipDirBuff.ActivateByteSwapping( IsX360() || IsPS3() );
	ReadFromPack( -1, zipDirBuff.Base(), -1, rec.centralDirectorySize, rec.startOfCentralDirOffset );
	zipDirBuff.SeekPut( CUtlBuffer::SEEK_HEAD, rec.centralDirectorySize );

	ZIP_FileHeader zipFileHeader;
	char filename[MAX_PATH];

	// Check for a preload section, expected to be the first file in the zip
	zipDirBuff.GetObjects( &zipFileHeader );
	zipDirBuff.Get( filename, zipFileHeader.fileNameLength );
	filename[zipFileHeader.fileNameLength] = '\0';
	if ( !V_stricmp( filename, PRELOAD_SECTION_NAME ) )
	{
		m_nPreloadSectionSize = zipFileHeader.uncompressedSize;
		m_nPreloadSectionOffset = zipFileHeader.relativeOffsetOfLocalHeader + 
						  sizeof( ZIP_LocalFileHeader ) + 
						  zipFileHeader.fileNameLength + 
						  zipFileHeader.extraFieldLength;
		SetupPreloadData();

		// Set up to extract the remaining files
		int nextOffset = bCompatibleFormat ? zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength : 0;
		zipDirBuff.SeekGet( CUtlBuffer::SEEK_CURRENT, nextOffset );
		firstFileIdx = 1;
	}
	else
	{
		if ( IsGameConsole() )
		{
			// all 360 zip files are expected to have preload sections
			// only during development, maps are allowed to lack them, due to auto-conversion
			Warning( "Xbox Zip File '%s' missing preload section\n", m_ZipName.String() );
		}

		// No preload section, reset buffer pointer
		zipDirBuff.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	}

	// Parse out central directory and determine absolute file positions of data.
	// Supports uncompressed zip files, with or without preload sections
	bool bSuccess = true;
	char tmpString[MAX_PATH];		
	CZipPackFile::CPackFileEntry lookup;

	m_PackFiles.EnsureCapacity( numFilesInZip );

	for ( int i = firstFileIdx; i < numFilesInZip; ++i )
	{
		zipDirBuff.GetObjects( &zipFileHeader );
		if ( zipFileHeader.signature != PKID( 1, 2 ) || zipFileHeader.compressionMethod != 0 )
		{
			Msg( "Incompatible pack file detected! %s\n", ( zipFileHeader.compressionMethod != 0 ) ? " File is compressed" : "" );
			bSuccess = false;
			break;	
		}

		Assert( zipFileHeader.fileNameLength < sizeof( tmpString ) );
		zipDirBuff.Get( (void *)tmpString, zipFileHeader.fileNameLength );
		tmpString[zipFileHeader.fileNameLength] = '\0';
		Q_FixSlashes( tmpString );

		lookup.m_hDebugFilename = m_fs->FindOrAddFileName( tmpString );
		lookup.m_HashName = HashStringCaselessConventional( tmpString );
		lookup.m_nLength = zipFileHeader.uncompressedSize;
		lookup.m_nPosition = zipFileHeader.relativeOffsetOfLocalHeader + 
								sizeof( ZIP_LocalFileHeader ) + 
								zipFileHeader.fileNameLength + 
								zipFileHeader.extraFieldLength;

		// track the index to this file's possible preload directory entry
		if ( m_pPreloadRemapTable )
		{
			lookup.m_nPreloadIdx = m_pPreloadRemapTable[i];
		}
		else
		{
			lookup.m_nPreloadIdx = INVALID_PRELOAD_ENTRY;
		}
		m_PackFiles.InsertNoSort( lookup );

		int nextOffset = bCompatibleFormat ? zipFileHeader.extraFieldLength + zipFileHeader.fileCommentLength : 0;
		zipDirBuff.SeekGet( CUtlBuffer::SEEK_CURRENT, nextOffset );
	}

	m_PackFiles.RedoSort();

	if ( IsGameConsole() )
	{
		// mount optional kv pool
		int nIndex;
		int64 nOffset;
		int nLength;
		if ( FindFile( "kvpool" PLATFORM_EXT ".image", nIndex, nOffset, nLength ) )
		{
			CUtlBuffer kvPoolBuffer;
			int nOptimalSize = AlignValue( nLength, XBOX_DVD_SECTORSIZE );
			void *pBuffer = malloc( nOptimalSize );
			kvPoolBuffer.SetExternalBuffer( pBuffer, nOptimalSize, nLength, CUtlBuffer::READ_ONLY );

			ReadFromPack( nIndex, pBuffer, nOptimalSize, nLength, nOffset );
			
			m_KVPoolKey = kvPoolBuffer.GetUnsignedInt();
		
			bSuccess = m_KVStringPool.RestoreFromBuffer( kvPoolBuffer );
			if ( !bSuccess )
			{
				Msg( "Failed to unserialize compiled kvpool\n" );
				m_KVStringPool.RemoveAll();
				m_KVPoolKey = 0;
			}

			free( pBuffer );
		}
	}

	return bSuccess;
}

CRC32_t CZipPackFile::GetKVPoolKey()
{
	return m_KVPoolKey;
}

bool CZipPackFile::GetStringFromKVPool( unsigned int key, char *pOutBuff, int buflen )
{
	return m_KVStringPool.String( (FileNameHandle_t)key, pOutBuff, buflen );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CZipPackFile::CZipPackFile( CBaseFileSystem* fs, void *pSection )
 : m_PackFiles(), m_KVStringPool()
{
	m_fs = fs;	
	m_pPreloadDirectory = NULL;
	m_pPreloadData = NULL;
	m_pPreloadHeader = NULL;
	m_pPreloadRemapTable = NULL;
	m_nPreloadSectionOffset = 0;
	m_nPreloadSectionSize = 0;
	m_KVPoolKey = 0;

#if defined( _GAMECONSOLE )
	m_pSection = pSection;
#else
	m_pSection = NULL;
#endif
}

CZipPackFile::~CZipPackFile()
{
	DiscardPreloadData();
	m_KVStringPool.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src1 - 
//			src2 - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CZipPackFile::CPackFileLessFunc::Less( CZipPackFile::CPackFileEntry const& src1, CZipPackFile::CPackFileEntry const& src2, void *pCtx )
{
	return ( src1.m_HashName < src2.m_HashName );
}


//-----------------------------------------------------------------------------
// Purpose: Search pPath for pak?.pak files and add to search path if found
// Input  : *pPath - 
//-----------------------------------------------------------------------------
#if defined( _GAMECONSOLE )
#define PACK_NAME_FORMAT "zip%i" PLATFORM_EXT ".zip"
#define PACK_LOCALIZED_NAME_FORMAT "zip%i_%s" PLATFORM_EXT ".zip"
#else
#define PACK_NAME_FORMAT "zip%i.zip"
#endif

void CBaseFileSystem::AddPackFiles( const char *pPath, const CUtlSymbol &pathID, SearchPathAdd_t addType, int iForceInsertIndex )
{
	Assert( ThreadInMainThread() );
	DISK_INTENSIVE();

	// Xbox Update and DLC zips are purposely not using the ZipN decoration so as not to interfere with the
	// install process that wants to move zip0 to the cache partition. These zips also have other mounting
	// requirements that prevent the simpler ZipN discovery logic.

#if defined( _GAMECONSOLE )
	// hack prepend the update path during first time add only
	// only specific paths get the update override explicitly prepended
	const char *pPathIDString = g_PathIDTable.String( pathID );
	if ( addType == PATH_ADD_TO_TAIL && iForceInsertIndex == 0 && 
		( !V_stricmp( pPathIDString, "PLATFORM" ) || !V_stricmp( pPathIDString, "GAME" ) || !V_stricmp( pPathIDString, "MOD" ) ) )
	{
		// update search path gets added once per allowed pathID
		bool bFoundSearchPath = false;
		for ( int i = 0; i < m_SearchPaths.Count(); i++ )
		{
			CSearchPath *pSearchPath = &m_SearchPaths[i];
			if ( pSearchPath->GetPathID() == pathID && !V_stricmp( pSearchPath->GetPathString(), "u:\\update\\" ) )
			{
				bFoundSearchPath = true;
				break;
			}
		}

		if ( !bFoundSearchPath && g_UpdateZipBuffer.TellPut() )
		{
			// found update blob in executable
			int nIndex = m_SearchPaths.AddToTail();
			CSearchPath *sp = &m_SearchPaths[ nIndex ];

			// the path and filename are fake but reserved, they denote this binary resident blob
			// these names ensure they get ignored during post hdd install sp fixup
			const char *pFullpath = "u:\\update\\update" PLATFORM_EXT ".zip";
			sp->m_pPathIDInfo = FindOrAddPathIDInfo( pathID, -1 );
			sp->m_storeId = g_iNextSearchPathID++;
			sp->SetPath( g_PathIDTable.AddString( "u:\\update\\" ) );

			// find and alias existing reference
			CPackFile *pf = NULL;
			for ( int iPackFile = 0; iPackFile < m_ZipFiles.Count(); iPackFile++ )
			{
				if ( !Q_stricmp( m_ZipFiles[iPackFile]->m_ZipName.Get(), pFullpath ) )
				{
					// found
					pf = m_ZipFiles[iPackFile];
					sp->SetPackFile( pf );
					pf->AddRef();
					break;
				}
			}
			if ( !pf )
			{
				// there is no 'file', point to the embedded section instead
				pf = new CZipPackFile( this, g_UpdateZipBuffer.Base() );

				pf->SetPath( sp->GetPath() );
				pf->m_bIsExcluded = false;
				pf->m_ZipName = pFullpath;

				m_ZipFiles.AddToTail( pf );
				sp->SetPackFile( pf );

				pf->m_lPackFileTime = 0;
				pf->m_hPackFileHandleFS = NULL;
				pf->Prepare( g_UpdateZipBuffer.TellPut() );
			}
		}
	}
#endif

	CUtlVector< CUtlString > pakPaths;
	CUtlVector< CUtlString > pakNames;
	CUtlVector< int64 > pakSizes;

	// determine pak files, [zip0..zipN]
	for ( int i = 0; ; i++ )
	{
		char pakfile[MAX_PATH];
		char fullpath[MAX_PATH];
		V_snprintf( pakfile, sizeof( pakfile ), PACK_NAME_FORMAT, i );
		V_ComposeFileName( pPath, pakfile, fullpath, sizeof( fullpath ) );

		struct _stat buf;
		if ( FS_stat( fullpath, &buf ) == -1 )
			break;

		MEM_ALLOC_CREDIT();

		pakPaths.AddToTail( pPath );
		pakNames.AddToTail( pakfile );
		pakSizes.AddToTail( (int64)((unsigned int)buf.st_size) );
	}

#if defined( _GAMECONSOLE )
	// safety measure, ensure 360 dlc zip search path gets added ONLY once per allowed pathID
	// dlc paths have unique suffixes, _dlc1..._dlcN
	if ( m_DLCContents.Count() && V_stristr( pPath, "_dlc" ) )
	{
		// per pathID, dlc zip should only occur once, but might have already been added
		bool bFoundSearchPath = false;
		for ( int i = 0; i < m_SearchPaths.Count(); i++ )
		{
			CSearchPath *pSearchPath = &m_SearchPaths[i];
			if ( pSearchPath->GetPathID() == pathID && V_stristr( pSearchPath->GetPathString(), pPath ) )
			{
				bFoundSearchPath = true;
				break;
			}
		}

		if ( !bFoundSearchPath )
		{
			char szFullPath[MAX_PATH];
			V_ComposeFileName( pPath, "dlc" PLATFORM_EXT ".zip", szFullPath, sizeof( szFullPath ) );
			struct _stat buf;
			if ( FS_stat( szFullPath, &buf ) != -1 )
			{
				pakPaths.AddToTail( pPath );
				pakNames.AddToTail( "dlc" PLATFORM_EXT ".zip" );
				pakSizes.AddToTail( (__int64)((unsigned int)buf.st_size) );
			}
		}
	}
#endif

	// Add any zip files in the format zip1.zip ... zip0.zip
	// Add them backwards so zip(N) is higher priority than zip(N-1), etc.
	int pakcount = pakSizes.Count();

	int nCount = 0;
	if ( IsGameConsole() && addType == PATH_ADD_TO_TAIL_ATINDEX )
	{
		// we will be inserting before this
		nCount = iForceInsertIndex;
	}

	for ( int i = pakcount-1; i >= 0; i-- )
	{
		char fullpath[MAX_PATH];
		V_ComposeFileName( pakPaths[i].Get(), pakNames[i].Get(), fullpath, sizeof( fullpath ) );

		int nIndex;
		if ( addType == PATH_ADD_TO_TAIL )
		{
			nIndex = m_SearchPaths.AddToTail();
		}
		else
		{
			nIndex = m_SearchPaths.InsertBefore( nCount );
			++nCount;
		}

		CSearchPath *sp = &m_SearchPaths[ nIndex ];
		
		sp->m_pPathIDInfo = FindOrAddPathIDInfo( pathID, -1 );
		sp->m_storeId = g_iNextSearchPathID++;
		sp->SetPath( g_PathIDTable.AddString( pakPaths[i].Get() ) );

		CPackFile *pf = NULL;
		for ( int iPackFile = 0; iPackFile < m_ZipFiles.Count(); iPackFile++ )
		{
			if ( !Q_stricmp( m_ZipFiles[iPackFile]->m_ZipName.Get(), fullpath ) )
			{
				pf = m_ZipFiles[iPackFile];
				sp->SetPackFile( pf );
				pf->AddRef();
				break;
			}
		}

		if ( !pf )
		{
			MEM_ALLOC_CREDIT();

			pf = new CZipPackFile( this );

			pf->SetPath( sp->GetPath() );
			pf->m_ZipName = fullpath;

			m_ZipFiles.AddToTail( pf );
			sp->SetPackFile( pf );
			pf->m_lPackFileTime = GetFileTime( fullpath );

			pf->m_hPackFileHandleFS = Trace_FOpen( fullpath, "rb", 0, NULL );

			if ( pf->m_hPackFileHandleFS )
			{
				FS_setbufsize( pf->m_hPackFileHandleFS, 32*1024 );	// 32k buffer.

				if ( pf->Prepare( pakSizes[i] ) )
				{
					FS_setbufsize( pf->m_hPackFileHandleFS, filesystem_buffer_size.GetInt() );
				}
				else
				{
					// Failed for some reason, ignore it
					if ( pf->m_hPackFileHandleFS )
					{
						Trace_FClose( pf->m_hPackFileHandleFS );
						pf->m_hPackFileHandleFS = NULL;
					}
					m_SearchPaths.Remove( nIndex );
				}
			}
#ifdef SUPPORT_VPK
			else
			{
				pf->m_hPackFileHandleVPK = FindFileInVPKs( fullpath );

				if ( !pf->m_hPackFileHandleVPK || !pf->Prepare( pakSizes[i] ) )
				{
					m_SearchPaths.Remove( nIndex );
				} 
			}
#endif
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Wipe all map (.bsp) pak file search paths
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveAllMapSearchPaths( void )
{
	AsyncFinishAll();

	int c = m_SearchPaths.Count();
	for ( int i = c - 1; i >= 0; i-- )
	{
		if ( !( m_SearchPaths[i].GetPackFile() && m_SearchPaths[i].GetPackFile()->m_bIsMapPath ) )
		{
			continue;
		}
		
		m_SearchPaths.Remove( i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddMapPackFile( const char *pPath, const char *pPathID, SearchPathAdd_t addType )
{
	char tempPathID[MAX_PATH];
	ParsePathID( pPath, pPathID, tempPathID );

	char newPath[ MAX_FILEPATH ];
	// +2 for '\0' and potential slash added at end.
	Q_strncpy( newPath, pPath, sizeof( newPath ) );
#ifdef _WIN32 // don't do this on linux!
	Q_strlower( newPath );
#endif
	Q_FixSlashes( newPath );

	// Open the .bsp and find the map lump
	char fullpath[ MAX_FILEPATH ];
	if ( Q_IsAbsolutePath( newPath ) ) // If it's an absolute path, just use that.
	{
		Q_strncpy( fullpath, newPath, sizeof(fullpath) );
	}
	else
	{
		if ( !GetLocalPath( newPath, fullpath, sizeof(fullpath) ) )
		{
			// Couldn't find that .bsp file!!!
			return;
		}
	}

	int c = m_SearchPaths.Count();
	for ( int i = c - 1; i >= 0; i-- )
	{
		if ( !( m_SearchPaths[i].GetPackFile() && m_SearchPaths[i].GetPackFile()->m_bIsMapPath ) )
			continue;
		
		if ( Q_stricmp( m_SearchPaths[i].GetPackFile()->m_ZipName.Get(), fullpath ) == 0 )
		{
			// Already set as map path
			return;
		}
	}

	// Remove previous
	RemoveAllMapSearchPaths();

#ifdef SUPPORT_VPK
	CPackedStoreFileHandle psHandle = FindFileInVPKs( pPath );

	if ( psHandle )
	{
		// Get the .bsp file header
		BSPHeader_t header;
		memset( &header, 0, sizeof( header ) );
		m_Stats.nBytesRead += psHandle.Read( &header, sizeof( header ) );
		m_Stats.nReads++;

		if ( header.ident != IDBSPHEADER || header.m_nVersion < MINBSPVERSION || header.m_nVersion > BSPVERSION )
		{
			return;
		}

		// Find the LUMP_PAKFILE offset
		lump_t *packfile = &header.lumps[ LUMP_PAKFILE ];
		if ( packfile->filelen <= sizeof( lump_t ) )
		{
			// It's empty or only contains a file header ( so there are no entries ), so don't add to search paths
			return;
		}

		// Seek to correct position
		psHandle.Seek( packfile->fileofs, FILESYSTEM_SEEK_HEAD );

		CPackFile *pf = new CZipPackFile( this );

		pf->m_bIsMapPath = true;
		pf->m_hPackFileHandleFS = NULL;
		pf->m_hPackFileHandleVPK = psHandle;

		MEM_ALLOC_CREDIT();
		pf->m_ZipName = fullpath;

		if ( pf->Prepare( packfile->filelen, packfile->fileofs ) )
		{
			int nIndex;
			if ( addType == PATH_ADD_TO_TAIL )
			{
				nIndex = m_SearchPaths.AddToTail();	
			}
			else
			{
				nIndex = m_SearchPaths.AddToHead();	
			}

			CSearchPath *sp = &m_SearchPaths[ nIndex ];

			sp->SetPackFile( pf );
			sp->m_storeId = g_iNextSearchPathID++;
			sp->SetPath( g_PathIDTable.AddString( newPath ) );
			sp->m_pPathIDInfo = FindOrAddPathIDInfo( g_PathIDTable.AddString( pPathID ), -1 );

			if ( IsDvdDevPathString( newPath ) )
			{
				sp->m_bIsDvdDevPath = true;
			}

			pf->SetPath( sp->GetPath() );
			pf->m_lPackFileTime = GetFileTime( newPath );

			//pf->m_PackFileID = m_FileTracker2.NotePackFileOpened( pPath, pPathID, packfile->filelen );
			m_ZipFiles.AddToTail( pf );
		}
		else
		{
			delete pf;
		}
	}
	else
#endif
	{
		FILE *fp = Trace_FOpen( fullpath, "rb", 0, NULL );
		if ( !fp )
		{
			// Couldn't open it
			FileSystemWarning( FILESYSTEM_WARNING, "Couldn't open .bsp %s for embedded pack file check\n", fullpath );
			return;
		}

		// Get the .bsp file header
		BSPHeader_t header;
		memset( &header, 0, sizeof( header ) );
		m_Stats.nBytesRead += FS_fread( &header, sizeof( header ), fp );
		m_Stats.nReads++;

		if ( header.ident != IDBSPHEADER || header.m_nVersion < MINBSPVERSION || header.m_nVersion > BSPVERSION )
		{
			Trace_FClose( fp );
			return;
		}

		// Find the LUMP_PAKFILE offset
		lump_t *packfile = &header.lumps[ LUMP_PAKFILE ];
		if ( packfile->filelen <= sizeof( lump_t ) )
		{
			// It's empty or only contains a file header ( so there are no entries ), so don't add to search paths
			Trace_FClose( fp );
			return;
		}

		// Seek to correct position
		FS_fseek( fp, packfile->fileofs, FILESYSTEM_SEEK_HEAD );

		CPackFile *pf = new CZipPackFile( this );

		pf->m_bIsMapPath = true;
		pf->m_hPackFileHandleFS = fp;
#ifdef SUPPORT_VPK
		pf->m_hPackFileHandleVPK = psHandle;
#endif
		MEM_ALLOC_CREDIT();
		pf->m_ZipName = fullpath;

		if ( pf->Prepare( packfile->filelen, packfile->fileofs ) )
		{
			int nIndex;
			if ( addType == PATH_ADD_TO_TAIL )
			{
				nIndex = m_SearchPaths.AddToTail();	
			}
			else
			{
				nIndex = m_SearchPaths.AddToHead();	
			}

			CSearchPath *sp = &m_SearchPaths[ nIndex ];

			sp->SetPackFile( pf );
			sp->m_storeId = g_iNextSearchPathID++;
			sp->SetPath( g_PathIDTable.AddString( newPath ) );
			sp->m_pPathIDInfo = FindOrAddPathIDInfo( g_PathIDTable.AddString( pPathID ), -1 );

			if ( IsDvdDevPathString( newPath ) )
			{
				sp->m_bIsDvdDevPath = true;
			}

			pf->SetPath( sp->GetPath() );
			pf->m_lPackFileTime = GetFileTime( newPath );

			Trace_FClose( pf->m_hPackFileHandleFS );
			pf->m_hPackFileHandleFS = NULL;

			//pf->m_PackFileID = m_FileTracker2.NotePackFileOpened( pPath, pPathID, packfile->filelen );
			m_ZipFiles.AddToTail( pf );
		}
		else
		{
			delete pf;
		}
	}
}


void CBaseFileSystem::BeginMapAccess() 
{
#if IsPlatformPS3()
	// SONY Fios library does not play well with QueuedLoader, so suspend it.
	// Adding a delay in the prefetch actually creates a soft-lock.
	SuspendPrefetches( "IFileSystem::BeginMapAccess" );
#endif

	if ( m_iMapLoad++ == 0 )
	{
		int c = m_SearchPaths.Count();
		for( int i = 0; i < c; i++ )
		{
			CSearchPath *pSearchPath = &m_SearchPaths[i];
			CPackFile *pPackFile = pSearchPath->GetPackFile();

			if ( pPackFile && pPackFile->m_bIsMapPath )
			{
				pPackFile->AddRef();
				pPackFile->m_mutex.Lock();
#ifdef SUPPORT_VPK
				if ( pPackFile->m_nOpenFiles == 0 && pPackFile->m_hPackFileHandleFS == NULL && !pPackFile->m_hPackFileHandleVPK )
#else
				if ( pPackFile->m_nOpenFiles == 0 && pPackFile->m_hPackFileHandleFS == NULL )
#endif
				{
					// Try opening the file as a regular file 
					pPackFile->m_hPackFileHandleFS = Trace_FOpen( pPackFile->m_ZipName, "rb", 0, NULL );

#ifdef SUPPORT_VPK
					if ( !pPackFile->m_hPackFileHandleFS )
					{
						pPackFile->m_hPackFileHandleVPK = FindFileInVPKs( pPackFile->m_ZipName );
					}
#endif
				}
				pPackFile->m_nOpenFiles++;
				pPackFile->m_mutex.Unlock();
			}
		}
	}
}


void CBaseFileSystem::EndMapAccess() 
{
	if ( m_iMapLoad-- == 1 )
	{
		int c = m_SearchPaths.Count();
		for( int i = 0; i < c; i++ )
		{
			CSearchPath *pSearchPath = &m_SearchPaths[i];
			CPackFile *pPackFile = pSearchPath->GetPackFile();

			if ( pPackFile && pPackFile->m_bIsMapPath )
			{
				pPackFile->m_mutex.Lock();
				pPackFile->m_nOpenFiles--;
				if ( pPackFile->m_nOpenFiles == 0  )
				{
					if ( pPackFile->m_hPackFileHandleFS )
					{
						Trace_FClose( pPackFile->m_hPackFileHandleFS );
						pPackFile->m_hPackFileHandleFS = NULL;
					}
				}
				pPackFile->m_mutex.Unlock();
				pPackFile->Release();
			}
		}
	}

#if IsPlatformPS3()
	ResumePrefetches( "IFileSystem::EndMapAccess" );
#endif
}


void CBaseFileSystem::PrintSearchPaths( void )
{
	int i;
	Msg( "---------------\n" );
	Msg( "%-20s %s\n", "Path ID:", "File Path:" );
	int c = m_SearchPaths.Count();
	for( i = 0; i < c; i++ )
	{
		CSearchPath *pSearchPath = &m_SearchPaths[i];

		const char *pszPack = "";
		const char *pszType = "";
		if ( pSearchPath->GetPackFile() && pSearchPath->GetPackFile()->m_bIsMapPath )
		{
			pszType = "(map)";
		}
		else if ( pSearchPath->GetPackFile()  )
		{
			pszType = "(pack) ";
			pszPack = pSearchPath->GetPackFile()->m_ZipName;
		}

		Msg( "%-20s \"%s\" %s%s\n", (const char *)pSearchPath->GetPathIDString(), pSearchPath->GetPathString(), pszType, pszPack );
	}

	if ( IsGameConsole() && m_ExcludeFilePaths.Count() )
	{
		// dump current list
		Msg( "\nExclude Paths:\n" );
		char szPath[MAX_PATH];
		for ( int i = 0; i < m_ExcludeFilePaths.Count(); i++ )
		{
			if ( String( m_ExcludeFilePaths[i], szPath, sizeof( szPath ) ) )
			{
				Msg( "\"%s\"\n", szPath );
			}
		}
	}

	PrintDLCInfo();
}


//-----------------------------------------------------------------------------
// Purpose: This is where search paths are created.  map files are created at head of list (they occur after
//  file system paths have already been set ) so they get highest priority.  Otherwise, we add the disk (non-packfile)
//  path and then the paks if they exist for the path
// Input  : *pPath - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddSearchPathInternal( const char *pPath, const char *pathID, SearchPathAdd_t addType, bool bAddPackFiles, int iForceInsertIndex )
{
	AsyncFinishAll();

	Assert( ThreadInMainThread() );

	// Map pak files have their own handler
	if ( V_stristr( pPath, ".bsp" ) )
	{
		AddMapPackFile( pPath, pathID, addType );
		return;
	}

	// Clean up the name
	char newPath[ MAX_FILEPATH ];
	if ( pPath[0] == 0 )
	{
		newPath[0] = newPath[1] = 0;
	}
	else
	{
		if ( IsGameConsole() || Q_IsAbsolutePath( pPath ) )
		{
			Q_strncpy( newPath, pPath, sizeof( newPath ) );
		}
		else
		{
			Q_MakeAbsolutePath( newPath, sizeof(newPath), pPath );
		}
#ifdef _WIN32
		Q_strlower( newPath );
#endif
		AddSeperatorAndFixPath( newPath );
	}

	// Make sure that it doesn't already exist
	CUtlSymbol pathSym, pathIDSym;
	pathSym = g_PathIDTable.AddString( newPath );
	pathIDSym = g_PathIDTable.AddString( pathID );
	int i;
	int c = m_SearchPaths.Count();
	int id = 0;
	for ( i = 0; i < c; i++ )
	{
		CSearchPath *pSearchPath = &m_SearchPaths[i];
		if ( pSearchPath->GetPath() == pathSym && pSearchPath->GetPathID() == pathIDSym )
		{
			if ( ( addType == PATH_ADD_TO_HEAD && i == 0 ) || ( addType == PATH_ADD_TO_TAIL ) )
			{
				return; // this entry is already at the head
			}
			else
			{
				m_SearchPaths.Remove(i); // remove it from its current position so we can add it back to the head
				i--;
				c--;
			}
		}
		if ( !id && pSearchPath->GetPath() == pathSym )
		{
			// get first found - all reference the same store
			id = pSearchPath->m_storeId;
		}
	}

	if (!id)
	{
		id = g_iNextSearchPathID++;
	}

	if ( IsGameConsole() && bAddPackFiles && ( !Q_stricmp( pathID, "DEFAULT_WRITE_PATH" ) || !Q_stricmp( pathID, "LOGDIR" ) ) )
	{
		// xbox can be assured that no zips would ever be loaded on its write path
		// otherwise xbox reloads zips because of mirrored drive mappings
		bAddPackFiles = false;
	}

	// Add to list
	bool bAdded = false;
	int nIndex = m_SearchPaths.Count();

	if ( IsGameConsole() && addType == PATH_ADD_TO_TAIL_ATINDEX )
	{
		nIndex = iForceInsertIndex;
	}

	if ( bAddPackFiles )
	{
		// Add pack files for this path next
		int lastCount = m_SearchPaths.Count();
		AddPackFiles( newPath, pathIDSym, addType, iForceInsertIndex );
		bAdded = m_SearchPaths.Count() != lastCount;
	}

	if ( IsGameConsole() && addType == PATH_ADD_TO_TAIL_ATINDEX )
	{
		// isolated this specific pathadd hack behavior to not destablize existing state
		// this behavior is for install and dlc searchpath modifications
		if ( V_stristr( pPath, "_dlc" ) )
		{
			if ( !bAdded )
			{
				// dlc should have had a pack file, if it didn't mount shouldn't be adding in anything else
				return;
			}

			if ( !V_stristr( pPath, XBX_GetLanguageString() ) )
			{
				// only dlc that is non-localized (language dlc doesn't have maps) gets a fallthrough directory due to map paths
				// every dlc thus far has had maps
				m_SearchPaths.InsertAfter( nIndex );
				nIndex++;
			}
		}
	}
	else
	{
		if ( addType == PATH_ADD_TO_HEAD )
		{
			nIndex = m_SearchPaths.Count() - nIndex;
			Assert( nIndex >= 0 );
		}

		if ( IsPC() || !bAddPackFiles || !bAdded )
		{
			// Grab last entry and set the path
			m_SearchPaths.InsertBefore( nIndex );
		}
		else if ( IsGameConsole() && bAddPackFiles && bAdded )
		{
			bool bAddNonZipPath = true;
			if ( bAddNonZipPath )
			{
				// 360 needs to find files (for the preload hit) in the zip first for fast loading
				// 360 always adds the non-pack search path *after* the pack file but respects the overall list ordering
				// fixup the indexes
				if ( addType == PATH_ADD_TO_HEAD )
				{
					m_SearchPaths.InsertBefore( nIndex );
				}
				else
				{
					nIndex = m_SearchPaths.Count() - 1;
					m_SearchPaths.InsertAfter( nIndex );
					nIndex++;
				}
			}
		}
	}

	// setup the 'base' path
	CSearchPath *sp = &m_SearchPaths[ nIndex ];
	sp->SetPath( pathSym );
	sp->m_pPathIDInfo = FindOrAddPathIDInfo( pathIDSym, -1 );

	// all matching paths have a reference to the same store
	sp->m_storeId = id;

	// classify the dvddev path
	if ( IsDvdDevPathString( newPath ) )
	{
		sp->m_bIsDvdDevPath = true;
	}
}

//-----------------------------------------------------------------------------
// Create the search path.
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddSearchPath( const char *pPath, const char *pathID, SearchPathAdd_t addType )
{
	char tempSymlinkBuffer[MAX_PATH];
	pPath = V_FormatFilenameForSymlinking( tempSymlinkBuffer, pPath );

#if !defined( _X360 )
	// The PC has no concept of update/dlc discovery, it explicitly adds them now
	// This layout matches the Xbox's search path layout, when the Xbox does late bind the DLC
	// any platform, game, or mod search paths get subverted in order to prepend the DLC path
	const char *pGameName = "csgo";

	// CSGO compatibility VPKs
	if ( const char *pActualPathID = pathID ? StringAfterPrefix( pathID, "COMPAT:" ) : NULL )
	{
#ifdef SUPPORT_VPK
		// Check if we are running with VPKs?
		if ( m_VPKFiles.Count() > 0 )
		{
			char newVPK[ MAX_PATH ] = {};
			sprintf( newVPK, "%s/pakxv_%s.vpk", pGameName, pPath );
			AddVPKFile( newVPK, addType );
			return;
		}
#endif
		if ( addType != PATH_ADD_TO_HEAD )
			return;	// in non-vpk mode compatibility paths can only be added to head

		// Build compatibility syntax path and proceed with adding
		pathID = pActualPathID;
		V_sprintf_safe( tempSymlinkBuffer, "csgo/compatibility/%s/", pPath );
		pPath = tempSymlinkBuffer;
	}

	if ( V_stristr( pPath, pGameName ) &&
		( !Q_stricmp( pathID, "GAME" ) || !Q_stricmp( pathID, "MOD" ) || !Q_stricmp( pathID, "PLATFORM" ) ) )
	{
		char szPathHead[MAX_PATH];
		char szUpdatePath[MAX_PATH];
		V_strncpy( szPathHead, pPath, sizeof( szPathHead ) );
		V_StripLastDir( szPathHead, sizeof( szPathHead ) );

		// xlsppatch trumps all
		V_ComposeFileName( szPathHead, "xlsppatch", szUpdatePath, sizeof( szUpdatePath ) );
		struct _stat buf;
		if ( FS_stat( szUpdatePath, &buf ) != -1 )
		{
			// found
			AddSearchPathInternal( szUpdatePath, pathID, addType, true );
		}

		// followed by update
		V_ComposeFileName( szPathHead, "update", szUpdatePath, sizeof( szUpdatePath ) );
		if ( FS_stat( szUpdatePath, &buf ) != -1 )
		{
			// found
			AddSearchPathInternal( szUpdatePath, pathID, addType, true );
		}

		// followed by dlc
		if ( !Q_stricmp( pathID, "GAME" ) || !Q_stricmp( pathID, "MOD" ) )
		{
			// DS would have all DLC dirs
			// find highest DLC dir available
			int nHighestDLC = 1;
			for ( ;nHighestDLC <= 99; nHighestDLC++ )
			{
				V_ComposeFileName( szPathHead, CFmtStr( "%s_dlc%d", pGameName, nHighestDLC ), szUpdatePath, sizeof( szUpdatePath ) );
				if ( FS_stat( szUpdatePath, &buf ) == -1 )
				{
					// does not exist, highest dlc available is previous
					nHighestDLC--;
					break;
				}

				V_ComposeFileName( szPathHead, CFmtStr( "%s_dlc%d/dlc_disabled.txt", pGameName, nHighestDLC ), szUpdatePath, sizeof( szUpdatePath ) );
				if ( FS_stat( szUpdatePath, &buf ) != -1 )
				{
					// disabled, highest dlc available is previous
					nHighestDLC--;
					break;
				}
			}
		
			// mount in correct order
			for ( ;nHighestDLC >= 1; nHighestDLC-- )
			{
				V_ComposeFileName( szPathHead, CFmtStr( "%s_dlc%d", pGameName, nHighestDLC ), szUpdatePath, sizeof( szUpdatePath ) );
				AddSearchPathInternal( szUpdatePath, pathID, addType, true );

#ifdef SUPPORT_VPK
				// scan for vpk's
				for( int i = 1 ; i < 99; i++ )
				{
					char newVPK[MAX_PATH];
					sprintf( newVPK, "%s/pak%02d_dir.vpk", szUpdatePath, i );
					// we will fopen to bypass pathing, etc
					FILE *pstdiofile = fopen( newVPK, "rb" );
					if ( pstdiofile )
					{
						fclose( pstdiofile );
						sprintf( newVPK, "%s/pak%02d.vpk", szUpdatePath, i );
						AddVPKFile( newVPK );
					}
					else
					{
						break;
					}
				}
#endif
			}
		}
	}
#endif

	int currCount = m_SearchPaths.Count();

	AddSearchPathInternal( pPath, pathID, addType, true );

#ifdef SUPPORT_VPK
	// scan for vpk's
	for( int i = 1 ; i < 99; i++ )
	{
		char newVPK[MAX_PATH];
		sprintf( newVPK, "%s/pak%02d_dir.vpk", pPath, i );
		// we will fopen to bypass pathing, etc
		FILE *pstdiofile = fopen( newVPK, "rb" );
		if ( pstdiofile )
		{
			fclose( pstdiofile );
			sprintf( newVPK, "%s/pak%02d.vpk", pPath, i );
			AddVPKFile( newVPK );
		}
		else
		{
			break;
		}
	}
#endif

	if ( IsGameConsole() && 
		( m_DVDMode == DVDMODE_DEV ) &&
		( !V_stricmp( pathID, "MOD" ) || !V_stricmp( pathID, "GAME" ) || !V_stricmp( pathID, "PLATFORM" ) ) )
	{
		// dvd development mode clones a search path based on the remote path for fall through
		const char *pBasePath = CommandLine()->ParmValue( "-basedir" );
		if ( pBasePath && !V_stristr( pPath, ".bsp" ) )
		{
			// isolate the search sub path from the base path
			const char *pSearchSubPath = StringAfterPrefix( pPath, pBasePath );
			if ( pSearchSubPath )
			{
				if ( pSearchSubPath[0] == CORRECT_PATH_SEPARATOR )
				{
					pSearchSubPath++;
				}

				// substitue the dvddev path head
				char szDvdDevPath[MAX_PATH];
				V_ComposeFileName( FS_DVDDEV_ROOT, pSearchSubPath, szDvdDevPath, sizeof( szDvdDevPath ) );

				// no pack files are allowed on the fall through dvddev path
				AddSearchPathInternal( szDvdDevPath, pathID, addType, false );
			}
		}
	}

	if ( currCount != m_SearchPaths.Count() )
	{
#if !defined( DEDICATED ) && !defined( _CERT )
		if ( IsGameConsole() )
		{
			// spew updated search paths
			// PrintSearchPaths();
		}
#endif
	}
}

//-----------------------------------------------------------------------------
// Patches the search path after install has finished. This is a hack until
// a reboot occurs. This is really bad, but our system has no fundamental
// way of manipulating search paths, so had to bury this here. This is designed
// to be innvoked ONCE after the installer has completed to swap in the
// cache based paths. The reboot causes a normal search path build out using
// the proper paths.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FixupSearchPathsAfterInstall()
{
#if defined( _X360 )
	if ( m_bSearchPathsPatchedAfterInstall )
	{
		// do not want to ever call this twice
		return true;
	}

	AsyncFinishAll();

	// this is incredibly hardcoded and fragile
	// after shipping need to revisit and generalize for installs
	// this assumes exact knowledge of how zips are mounted and the install footprint
	for ( int i = 0; i < m_SearchPaths.Count(); i++ )
	{
		const char *pPathID = m_SearchPaths[i].GetPathIDString();
		if ( V_stricmp( pPathID, "GAME" ) && V_stricmp( pPathID, "MOD" ) )
		{
			// only consider these paths
			continue;
		}

		const char *pPath = m_SearchPaths[i].GetPathString();
		const char *pColon = strchr( pPath, ':' );
		if ( !pColon || 
			!V_stristr( pPath, "csgo" ) || 
			V_stristr( pPath, "_lv" ) || 
			V_stristr( pPath, "_dlc" ) ||
			V_stristr( pPath, "_tempcontent" ) )
		{
			// ignore relative paths, can't patch those
			// ignore any non csgo path
			// ignore lv, dlc, tempcontent path, not installing those zips
			continue;
		}
		if ( !m_SearchPaths[i].GetPackFile() || m_SearchPaths[i].GetPackFile()->m_bIsMapPath )
		{
			// ignore non pack based paths
			// ignore bsps
			continue;
		}
		if ( !V_stristr( m_SearchPaths[i].GetPackFile()->m_ZipName.String(), "zip0" PLATFORM_EXT ".zip" ) )
		{
			// only patching zip0
			continue;
		}

		// Not installing localized data
		if ( m_SearchPaths[i].m_bIsLocalizedPath )
		{
			continue;
		}

		char szNewPath[MAX_PATH];
		V_snprintf( szNewPath, sizeof( szNewPath ), "%s%s", CACHE_PATH_CSTIKRE15, pColon+1 );
		V_FixSlashes( szNewPath );

		int lastCount = m_SearchPaths.Count();
		AddSearchPathInternal( szNewPath, pPathID, PATH_ADD_TO_TAIL_ATINDEX, true, i );
		int numNewPaths = m_SearchPaths.Count() - lastCount;
		if ( numNewPaths )
		{
			// skip paths all the paths we just added
			i += numNewPaths;
			// this is really bad, skip past the zip we just considered, the next iteration will skip us to the next zip
			i++;
		}

		m_bSearchPathsPatchedAfterInstall = true;
	}

	if ( m_bSearchPathsPatchedAfterInstall )
	{
		// cache paths got added
		// shutdown non cache paths
		// must do multiple passes until no removal occurs
		bool bRemoved;
		while ( 1 )
		{
			bRemoved = false;

			for ( int i = 0; i < m_SearchPaths.Count(); i++ )
			{
				const char *pPathID = m_SearchPaths[i].GetPathIDString();
				if ( V_stricmp( pPathID, "GAME" ) && V_stricmp( pPathID, "MOD" ) )
				{
					// only consider these paths
					continue;
				}
			
				const char *pPath = m_SearchPaths[i].GetPathString();
				const char *pColon = strchr( pPath, ':' );
				if ( !pColon || 
					!V_stristr( pPath, "csgo" ) || 
					V_stristr( pPath, "_lv" ) || 
					V_stristr( pPath, "_dlc" ) ||
					V_stristr( pPath, "_tempcontent" ) )
				{
					// ignore relative paths, can't patch those
					// ignore any non csgo path
					// ignore lv, dlc, or tempcontent path, not installing those zips
					continue;
				}
				if ( !m_SearchPaths[i].GetPackFile() || m_SearchPaths[i].GetPackFile()->m_bIsMapPath )
				{
					// ignore non pack based paths
					// ignore bsps
					continue;
				}

				// Not installing localized data
				if ( m_SearchPaths[i].m_bIsLocalizedPath )
				{
					continue;
				}

				if ( V_stristr( pPath, "cache:" ) || !V_stristr( m_SearchPaths[i].GetPackFile()->m_ZipName.String(), "zip0" PLATFORM_EXT ".zip" ) )
				{
					// ignore any cache oriented paths
					// only want to remove non-cache paths of zip0.360.zip
					continue;
				}

				m_SearchPaths.Remove( i );
				bRemoved = true;
				break;
			}

			if ( !bRemoved )
			{
				break;
			}
		}
	}
#endif

	return m_bSearchPathsPatchedAfterInstall;
}

//-----------------------------------------------------------------------------
// ConCommands can just blindly call this to ensure user's request to flush
// data picks up any intended changes they may have done locally.
//-----------------------------------------------------------------------------
void CBaseFileSystem::SyncDvdDevCache()
{
	if ( !IsGameConsole() || m_DVDMode != DVDMODE_DEV )
	{
		// xbox dvddev only
		return;
	}

#if defined( _X360 )
	XBX_rSyncDvdDevCache();
#endif

	BuildExcludeList();
}

//-----------------------------------------------------------------------------
// Returns the search path, each path is separated by ;s. Returns the length of the string returned
// Pack search paths include the pack name, so that callers can still form absolute paths
// and that absolute path can be sent to the filesystem, and mounted as a file inside a pack.
//-----------------------------------------------------------------------------
int CBaseFileSystem::GetSearchPath( const char *pathID, bool bGetPackFiles, char *pPath, int nMaxLen )
{
	AUTO_LOCK( m_SearchPathsMutex );

	int nLen = 0;
	if ( nMaxLen )
	{
		pPath[0] = 0;
	}

	CSearchPathsIterator iter( this, pathID, bGetPackFiles ? FILTER_NONE : FILTER_CULLPACK );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		CPackFile *pPackFile = pSearchPath->GetPackFile();

		if ( nLen >= nMaxLen )
		{
			// Add 1 for the semicolon if our length is not 0
			nLen += (nLen > 0) ? 1 : 0;

			if ( !pPackFile )
			{
				nLen += Q_strlen( pSearchPath->GetPathString() );
			}
			else
			{
				// full path and slash
				nLen += Q_strlen( pPackFile->m_ZipName.String() ) + 1;
			}
			continue;
		}

		if ( nLen != 0 )
		{
			pPath[nLen++] = ';';
		}

		if ( !pPackFile )
		{
			Q_strncpy( &pPath[nLen], pSearchPath->GetPathString(), nMaxLen - nLen );
			nLen += Q_strlen( pSearchPath->GetPathString() );
		}
		else
		{
			// full path and slash
			Q_strncpy( &pPath[nLen], pPackFile->m_ZipName.String(), nMaxLen - nLen );
			V_AppendSlash( &pPath[nLen], nMaxLen - nLen );
			nLen += Q_strlen( pPackFile->m_ZipName.String() ) + 1;
		}
	}

	// Return 1 extra for the NULL terminator
	return nLen + 1;
}

//-----------------------------------------------------------------------------
// Returns the search path IDs, each path is separated by ;s. Returns the length of the string returned
//-----------------------------------------------------------------------------
int CBaseFileSystem::GetSearchPathID( char *pPath, int nMaxLen )
{
	AUTO_LOCK( m_SearchPathsMutex );

	if ( nMaxLen )
	{
		pPath[0] = 0;
	}

	// determine unique PathIDs
	CUtlVector< CUtlSymbol > list;
	for ( int i = 0 ; i < m_SearchPaths.Count(); i++ )
	{
		CUtlSymbol pathID = m_SearchPaths[i].GetPathID();
		if ( pathID != UTL_INVAL_SYMBOL && list.Find( pathID ) == -1 )
		{
			list.AddToTail( pathID );
			V_strncat( pPath, m_SearchPaths[i].GetPathIDString(), nMaxLen );
			V_strncat( pPath, ";", nMaxLen );
		}
	}
	
	return strlen( pPath ) + 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPath - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::RemoveSearchPath( const char *pPath, const char *pathID )
{
	char tempSymlinkBuffer[MAX_PATH];
	pPath = V_FormatFilenameForSymlinking( tempSymlinkBuffer, pPath );

	AsyncFinishAll();

	char newPath[ MAX_FILEPATH ];
	newPath[ 0 ] = 0;

	if ( const char *pActualPathID = pathID ? StringAfterPrefix( pathID, "COMPAT:" ) : NULL )
	{
#ifdef SUPPORT_VPK
		if ( m_VPKFiles.Count() > 0 )
		{
			char newVPK[ MAX_PATH ] = {};
			sprintf( newVPK, "pakxv_%s", pPath );
			while ( m_VPKFiles.Count() > 0 )
			{
				CPackedStore *pVPK = m_VPKFiles.Head();
				char szExistingVPKBase[ MAX_PATH ] = {};
				V_FileBase( pVPK->BaseName(), szExistingVPKBase, sizeof( szExistingVPKBase ) );
				char const *szAfterRemovedPathVPK = StringAfterPrefix( szExistingVPKBase, newVPK );
				if ( szAfterRemovedPathVPK && ( *szAfterRemovedPathVPK == 0 || *szAfterRemovedPathVPK == '_' ) )
				{
					m_VPKFiles.RemoveMultipleFromHead( 1 );
					delete pVPK;
				}
				else
					break;
			}
			return true;
		}
#endif
		int c = m_SearchPaths.Count();
		for ( int i = c - 1; i >= 0; i-- )
		{
			char newCompatPath[ MAX_PATH ] = {};
			sprintf( newCompatPath, "/csgo/compatibility/%s/", pPath );
			Q_FixSlashes( newCompatPath );
			if ( V_strstr( m_SearchPaths[ i ].GetPathString(), newCompatPath ) )
			{
				m_SearchPaths.Remove( i );
				return true;
			}
		}
	}

	if ( pPath )
	{
		// +2 for '\0' and potential slash added at end.
		Q_strncpy( newPath, pPath, sizeof( newPath ) );
#ifdef _WIN32 // don't do this on linux!
		Q_strlower( newPath );
#endif
		if ( V_stristr( newPath, ".bsp" ) )
		{
			Q_FixSlashes( newPath );
		}
		else
		{
			AddSeperatorAndFixPath( newPath );
		}
	}
	pPath = newPath;

	CUtlSymbol lookup = g_PathIDTable.AddString( pPath );
	CUtlSymbol id = g_PathIDTable.AddString( pathID );

	bool bret = false;

	// Count backward since we're possibly deleting one or more pack files, too
	int i;
	int c = m_SearchPaths.Count();
	for( i = c - 1; i >= 0; i-- )
	{
		if ( newPath && m_SearchPaths[i].GetPath() != lookup )
			continue;

		if ( FilterByPathID( &m_SearchPaths[i], id ) )
			continue;

		m_SearchPaths.Remove( i );
		bret = true;
	}
	return bret;
}


//-----------------------------------------------------------------------------
// Purpose: Removes all search paths for a given pathID, such as all "GAME" paths.
// Input  : pathID - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveSearchPaths( const char *pathID )
{
	AsyncFinishAll();

	int nCount = m_SearchPaths.Count();
	for (int i = nCount - 1; i >= 0; i--)
	{
		if (!Q_stricmp(m_SearchPaths.Element(i).GetPathIDString(), pathID))
		{
			m_SearchPaths.FastRemove(i);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath *CBaseFileSystem::FindWritePath( const char *pFilename, const char *pathID )
{
	CUtlSymbol lookup = g_PathIDTable.AddString( pathID );

	AUTO_LOCK( m_SearchPathsMutex );

	// a pathID has been specified, find the first match in the path list
	int c = m_SearchPaths.Count();
	for ( int i = 0; i < c; i++ )
	{
		// pak files are not allowed to be written to...
		CSearchPath *pSearchPath = &m_SearchPaths[i];
		if ( pSearchPath->GetPackFile() )
		{
			continue;
		}

		if ( pathID && ( pSearchPath->GetPathID() != lookup ) )
		{
			// not the right pathID
			continue;
		}

		if ( IsGameConsole() && ( m_DVDMode == DVDMODE_DEV ) && pFilename && !pSearchPath->m_bIsDvdDevPath )
		{
			bool bIgnorePath = false;
			char szExcludeFile[MAX_PATH];
			char szFilename[MAX_PATH];
			V_ComposeFileName( pSearchPath->GetPathString(), pFilename, szFilename, sizeof( szFilename ) );
			for ( int j = 0; j < m_ExcludeFilePaths.Count(); j++ )
			{
				if ( g_pFullFileSystem->String( m_ExcludeFilePaths[j], szExcludeFile, sizeof( szExcludeFile ) ) )
				{
					if ( !V_stricmp( szFilename, szExcludeFile ) )
					{
						bIgnorePath = true;
						break;
					}
				}
			}
			if ( bIgnorePath )
			{
				// filename matches exclusion path, skip it
				// favoring the next path which should be the path fall through hit
				continue;
			}
		}

		return pSearchPath;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Finds a search path that should be used for writing to, given a pathID.
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::GetWritePath( const char *pFilename, const char *pathID )
{
	CSearchPath *pSearchPath = NULL;
	if ( pathID && pathID[ 0 ] != '\0' )
	{
		pSearchPath = FindWritePath( pFilename, pathID );
		if ( pSearchPath )
		{
			return pSearchPath->GetPathString();
		}

		FileSystemWarning( FILESYSTEM_WARNING, "Requested non-existent write path %s!\n", pathID );
	}

	pSearchPath = FindWritePath( pFilename, "DEFAULT_WRITE_PATH" );
	if ( pSearchPath )
	{
		return pSearchPath->GetPathString();
	}

	pSearchPath = FindWritePath( pFilename, NULL ); // okay, just return the first search path added!
	if ( pSearchPath )
	{
		return pSearchPath->GetPathString();
	}

	// Hope this is reasonable!!
	return ".\\";
}

//-----------------------------------------------------------------------------
// Reads/writes files to utlbuffers.  Attempts alignment fixups for optimal read
//-----------------------------------------------------------------------------
#ifdef _PS3
#define g_pszReadFilename GetTLSGlobals()->pFileSystemReadFilename
#else
CTHREADLOCALPTR(char) g_pszReadFilename;
#endif

bool CBaseFileSystem::ReadToBuffer( FileHandle_t fp, CUtlBuffer &buf, int nMaxBytes, FSAllocFunc_t pfnAlloc )
{
	SetBufferSize( fp, 0 );  // TODO: what if it's a pack file? restore buffer size?

	int nBytesToRead = Size( fp );
	if ( nBytesToRead == 0 )
	{
		// no data in file
		return true;
	}

	if ( nMaxBytes > 0 )
	{
		// can't read more than file has
		nBytesToRead = MIN( nMaxBytes, nBytesToRead );
	}

	int nBytesRead = 0;
	int nBytesOffset = 0;

	int iStartPos = Tell( fp );

	if ( nBytesToRead != 0 )
	{
		int nBytesDestBuffer = nBytesToRead;
		unsigned nSizeAlign = 0, nBufferAlign = 0, nOffsetAlign = 0;

		bool bBinary = !( buf.IsText() && !buf.ContainsCRLF() );

		if ( bBinary && !IsPosix() && !buf.IsExternallyAllocated() && !pfnAlloc && 
			( buf.TellPut() == 0 ) && ( buf.TellGet() == 0 ) && ( iStartPos % 4 == 0 ) &&
			GetOptimalIOConstraints( fp, &nOffsetAlign, &nSizeAlign, &nBufferAlign ) )
		{
			// correct conditions to allow an optimal read
			if ( iStartPos % nOffsetAlign != 0 )
			{
				// move starting position back to nearest alignment
				nBytesOffset = ( iStartPos % nOffsetAlign );
				Assert ( ( iStartPos - nBytesOffset ) % nOffsetAlign == 0 );
				Seek( fp, -nBytesOffset, FILESYSTEM_SEEK_CURRENT );

				// going to read from aligned start, increase target buffer size by offset alignment
				nBytesDestBuffer += nBytesOffset;
			}

			// snap target buffer size to its size alignment
			// add additional alignment slop for target pointer adjustment
			nBytesDestBuffer = AlignValue( nBytesDestBuffer, nSizeAlign ) + nBufferAlign;
		}

		if ( !pfnAlloc )
		{
			buf.EnsureCapacity( nBytesDestBuffer + buf.TellPut() );
		}
		else
		{
			// caller provided allocator
			void *pMemory = (*pfnAlloc)( g_pszReadFilename, nBytesDestBuffer );
			buf.SetExternalBuffer( pMemory, nBytesDestBuffer, 0, buf.GetFlags() & ~CUtlBuffer::EXTERNAL_GROWABLE );
		}

		int seekGet = -1;
		if ( nBytesDestBuffer != nBytesToRead )
		{
			// doing optimal read, align target pointer
			int nAlignedBase = AlignValue( (byte *)buf.Base(), nBufferAlign ) - (byte *)buf.Base();
			buf.SeekPut( CUtlBuffer::SEEK_HEAD, nAlignedBase );
	
			// the buffer read position is slid forward to ignore the addtional
			// starting offset alignment
			seekGet = nAlignedBase + nBytesOffset;
		}

		nBytesRead = ReadEx( buf.PeekPut(), nBytesDestBuffer - nBufferAlign, nBytesToRead + nBytesOffset, fp );
		buf.SeekPut( CUtlBuffer::SEEK_CURRENT, nBytesRead );

		if ( seekGet != -1 )
		{
			// can only seek the get after data has been put, otherwise buffer sets overflow error
			buf.SeekGet( CUtlBuffer::SEEK_HEAD, seekGet );
		}

		Seek( fp, iStartPos + ( nBytesRead - nBytesOffset ), FILESYSTEM_SEEK_HEAD );
	}

	return (nBytesRead != 0);
}

//-----------------------------------------------------------------------------
// Reads/writes files to utlbuffers
// NOTE NOTE!! 
// If you change this implementation, copy it into CBaseVMPIFileSystem::ReadFile
// in vmpi_filesystem.cpp
//-----------------------------------------------------------------------------
bool CBaseFileSystem::ReadFile( const char *pFileName, const char *pPath, CUtlBuffer &buf, int nMaxBytes, int nStartingByte, FSAllocFunc_t pfnAlloc )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	bool bBinary = !( buf.IsText() && !buf.ContainsCRLF() );

	FileHandle_t fp = Open( pFileName, ( bBinary ) ? "rb" : "rt", pPath );
	if ( !fp )
		return false;

	if ( nStartingByte != 0 )
	{
		Seek( fp, nStartingByte, FILESYSTEM_SEEK_HEAD );
	}

	if ( pfnAlloc )
	{
		g_pszReadFilename = (char *)pFileName;
	}

	bool bSuccess = ReadToBuffer( fp, buf, nMaxBytes, pfnAlloc );

	Close( fp );

	return bSuccess;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CBaseFileSystem::ReadFileEx( const char *pFileName, const char *pPath, void **ppBuf, bool bNullTerminate, bool bOptimalAlloc, int nMaxBytes, int nStartingByte, FSAllocFunc_t pfnAlloc )
{
	FileHandle_t fp = Open( pFileName, "rb", pPath );
	if ( !fp )
	{
		return 0;
	}

	if ( IsGameConsole() )
	{
		// callers are sloppy, always want optimal
		bOptimalAlloc = true;
	}

	SetBufferSize( fp, 0 );  // TODO: what if it's a pack file? restore buffer size?

	int nBytesToRead = Size( fp );
	int nBytesRead = 0;
	if ( nMaxBytes > 0 )
	{
		nBytesToRead = MIN( nMaxBytes, nBytesToRead );
		if ( bNullTerminate )
		{
			nBytesToRead--;
		}
	}

	if ( nBytesToRead != 0 )
	{
		int nBytesBuf;
		if ( !*ppBuf )
		{
			nBytesBuf = nBytesToRead + ( ( bNullTerminate ) ? 1 : 0 );

			if ( !pfnAlloc && !bOptimalAlloc )
			{
				*ppBuf = new byte[nBytesBuf];
			}
			else if ( !pfnAlloc )
			{
				*ppBuf = AllocOptimalReadBuffer( fp, nBytesBuf, 0 );
				nBytesBuf = GetOptimalReadSize( fp, nBytesBuf );
			}
			else
			{
				*ppBuf = (*pfnAlloc)( pFileName, nBytesBuf );
			}
		}
		else
		{
			nBytesBuf = nMaxBytes;
		}

		if ( nStartingByte != 0 )
		{
			Seek( fp, nStartingByte, FILESYSTEM_SEEK_HEAD );
		}

		nBytesRead = ReadEx( *ppBuf, nBytesBuf, nBytesToRead, fp );

		if ( bNullTerminate )
		{
			((byte *)(*ppBuf))[nBytesToRead] = 0;
		}
	}

	Close( fp );
	return nBytesRead;
}


//-----------------------------------------------------------------------------
// NOTE NOTE!! 
// If you change this implementation, copy it into CBaseVMPIFileSystem::WriteFile
// in vmpi_filesystem.cpp
//-----------------------------------------------------------------------------
bool CBaseFileSystem::WriteFile( const char *pFileName, const char *pPath, CUtlBuffer &buf )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	const char *pWriteFlags = "wb";
	if ( buf.IsText() && !buf.ContainsCRLF() )
	{
		pWriteFlags = "wt";
	}

	FileHandle_t fp = Open( pFileName, pWriteFlags, pPath );
	if ( !fp )
		return false;

	int nBytesWritten = Write( buf.Base(), buf.TellMaxPut(), fp );

	Close( fp );
	return (nBytesWritten != 0);
}


bool CBaseFileSystem::UnzipFile( const char *pFileName, const char *pPath, const char *pDestination )
{
#ifdef POSIX
	Error( " need to hook up zip for linux" );
#else
	IZip *pZip = IZip::CreateZip( NULL, true );

	HANDLE hZipFile = pZip->ParseFromDisk( pFileName );
	if ( !hZipFile )
	{
		Msg( "Bad or missing zip file, failed to open '%s'\n", pFileName );
		return false;
	}

	int iZipIndex = -1;
	int iFileSize;
	char szFileName[MAX_PATH];

	// Create Directories
	CreateDirHierarchy( pDestination, pPath );

	while ( 1 )
	{
		// Get the next file in the zip
		szFileName[0] = '\0';
		iFileSize = 0;
		iZipIndex = pZip->GetNextFilename( iZipIndex, szFileName, sizeof( szFileName ), iFileSize );

		// If there aren't any more files then break out of this while
		if ( iZipIndex == -1 )
			break;

		int iFileNameLength = Q_strlen( szFileName );
		if ( szFileName[ iFileNameLength - 1 ] == '/' )
		{
			// Its a directory, so create it
			szFileName[ iFileNameLength - 1 ] = '\0';

			char szFinalName[ MAX_PATH ];
			Q_snprintf( szFinalName, sizeof( szFinalName ), "%s\\%s", pDestination, szFileName );
			CreateDirHierarchy( szFinalName, pPath );
		}
	}

	// Write Files
	while ( 1 )
	{
		szFileName[0] = '\0';
		iFileSize = 0;
		iZipIndex = pZip->GetNextFilename( iZipIndex, szFileName, sizeof( szFileName ), iFileSize );

		// If there aren't any more files then break out of this while
		if ( iZipIndex == -1 )
			break;

		int iFileNameLength = Q_strlen( szFileName );
		if ( szFileName[ iFileNameLength - 1 ] != '/' )
		{
			// It's not a directory, so write the file
			CUtlBuffer fileBuffer;
			fileBuffer.Purge();

			if ( pZip->ReadFileFromZip( hZipFile, szFileName, false, fileBuffer ) )
			{
				char szFinalName[ MAX_PATH ];
				Q_snprintf( szFinalName, sizeof( szFinalName ), "%s\\%s", pDestination, szFileName );

				WriteFile( szFinalName, pPath, fileBuffer );
			}
		}
	}

	::CloseHandle( hZipFile );

	IZip::ReleaseZip( pZip );
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveAllSearchPaths( void )
{
	AUTO_LOCK( m_SearchPathsMutex );
	// Sergiy: AaronS said it is a good idea to destroy these paths in reverse order
	while( m_SearchPaths.Count() )
	{
		m_SearchPaths.Remove( m_SearchPaths.Count() - 1 );
	}
	//m_PackFileHandles.Purge();
}


void CBaseFileSystem::LogFileAccess( const char *pFullFileName )
{
	if( !m_pLogFile )
	{
		return;
	}
	if ( IsPS3() )
	{
		AssertMsg( false, "LogFileAccess broken on PS3\n" );
		return;
	}
	char buf[1024];
#if BSPOUTPUT
	Q_snprintf( buf, sizeof( buf ), "%s\n%s\n", pShortFileName, pFullFileName);
	fprintf( m_pLogFile, "%s", buf ); // STEAM OK
#else
	char cwd[MAX_FILEPATH];
	getcwd( cwd, MAX_FILEPATH-1 );
	Q_strcat( cwd, "\\", sizeof( cwd ) );
	if( Q_strnicmp( cwd, pFullFileName, strlen( cwd ) ) == 0 )
	{
		const char *pFileNameWithoutExeDir = pFullFileName + strlen( cwd );
		char targetPath[ MAX_FILEPATH ];
		strcpy( targetPath, "%fs_target%\\" );
		strcat( targetPath, pFileNameWithoutExeDir );
		Q_snprintf( buf, sizeof( buf ), "copy \"%s\" \"%s\"\n", pFullFileName, targetPath );
		char tmp[ MAX_FILEPATH ];
		Q_strncpy( tmp, targetPath, sizeof( tmp ) );
		Q_StripFilename( tmp );
		fprintf( m_pLogFile, "mkdir \"%s\"\n", tmp ); // STEAM OK
		fprintf( m_pLogFile, "%s", buf ); // STEAM OK
	}
	else
	{
		Assert( 0 );
	}
#endif
}

class CFileOpenInfo
{
public:
	CFileOpenInfo( CBaseFileSystem *pFileSystem, const char *pFileName, const CBaseFileSystem::CSearchPath *path, const char *pOptions, int flags, char **ppszResolvedFilename, bool bTrackCRCs ) : 
		m_pFileSystem( pFileSystem ), m_pFileName( pFileName ), m_pPath( path ), m_pOptions( pOptions ), m_Flags( flags ), m_ppszResolvedFilename( ppszResolvedFilename ), m_bTrackCRCs( bTrackCRCs )
	{
		// Multiple threads can access the whitelist simultaneously. 
		// That's fine, but make sure it doesn't get freed by another thread.
		if ( IsPC() )
		{
			m_pWhitelist = m_pFileSystem->m_FileWhitelist.AddRef();
		}
		else
		{
			m_pWhitelist = NULL;
		}
		m_pFileHandle = NULL;
		m_bLoadedFromSteamCache = m_bSteamCacheOnly = false;
		
		if ( m_ppszResolvedFilename )
			*m_ppszResolvedFilename = NULL;
#ifdef SUPPORT_VPK
		m_pPackFile = NULL;
		m_pVPKFile = NULL;
#endif
	}
	
	~CFileOpenInfo()
	{
		if ( IsGameConsole() )
		{
			return;
		}

		m_pFileSystem->m_FileWhitelist.ReleaseRef( m_pWhitelist );
	}
	
	void SetAbsolutePath( const char *pFormat, ... )
	{
		va_list marker;
		va_start( marker, pFormat );
		V_vsnprintf( m_AbsolutePath, sizeof( m_AbsolutePath ), pFormat, marker );
		va_end( marker );

		V_FixSlashes( m_AbsolutePath );
	}
	
	void SetResolvedFilename( const char *pStr )
	{
		if ( m_ppszResolvedFilename )
		{
			Assert( !( *m_ppszResolvedFilename ) );
			*m_ppszResolvedFilename = strdup( pStr );
		}
	}

	// Handles telling CFileTracker about the file we just opened so it can remember
	// where the file came from, and possibly calculate a CRC if necessary.
	void HandleFileCRCTracking( const char *pRelativeFileName, bool bIsAbsolutePath )
	{
		if ( IsGameConsole() )
		{
			return;
		}

		if ( m_pFileSystem->m_WhitelistFileTrackingEnabled == 0 || !m_bTrackCRCs )
			return;
		
		if ( m_pFileHandle )
		{
			FILE *fp = m_pFileHandle->m_pFile;
			int64 nLength = m_pFileHandle->m_nLength;
#ifdef SUPPORT_VPK
			if ( m_pVPKFile )
			{
				m_pFileSystem->m_FileTracker2.NotePackFileAccess( pRelativeFileName, m_pPath->GetPathIDString(), m_pFileHandle->m_VPKHandle );
				return;
			}
#endif
			// we always record hashes of everything we load. we may filter later.
			m_pFileSystem->m_FileTracker2.NoteFileLoadedFromDisk( pRelativeFileName, m_pPath->GetPathIDString(), fp, nLength );
	}
		else if ( m_bSteamCacheOnly )
		{
			// Remember that the file failed to load. We only need to do this in the case where we forced it to ignore files
			// on disk (in which case we'll want it to check the disk next time if sv_pure changed).
			m_pFileSystem->m_FileTracker.NoteFileFailedToLoad( pRelativeFileName, m_pPath->GetPathIDString() );
		}
	}

	// Decides if the file must come from Steam or if it can be allowed to come off disk.
	void DetermineFileLoadInfoParameters( CFileLoadInfo &fileLoadInfo, bool bIsAbsolutePath )
	{
		if ( IsGameConsole() )
		{
			fileLoadInfo.m_bSteamCacheOnly = false;
#ifdef _PS3
            fileLoadInfo.m_ps3Filetype = PS3_FILETYPE_UNKNOWN;
#endif
			return;
		}

		if ( m_bTrackCRCs && m_pWhitelist && m_pWhitelist->m_pAllowFromDiskList && !bIsAbsolutePath )
		{
			Assert( !V_IsAbsolutePath( m_pFileName ) ); // (This is what bIsAbsolutePath is supposed to tell us..)
			// Ask the whitelist if this file must come from Steam.
			fileLoadInfo.m_bSteamCacheOnly = !m_pWhitelist->m_pAllowFromDiskList->IsFileInList( m_pFileName );
		}
		else
		{
			fileLoadInfo.m_bSteamCacheOnly = false;
		}	
	}

public:
	CBaseFileSystem *m_pFileSystem;
	CWhitelistSpecs *m_pWhitelist;

	// These are output parameters.
	CFileHandle *m_pFileHandle;
	char **m_ppszResolvedFilename;


#ifdef SUPPORT_VPK
	CPackFile *m_pPackFile;
	CPackedStore *m_pVPKFile;
#endif

	const char *m_pFileName;
	const CBaseFileSystem::CSearchPath *m_pPath;
	const char *m_pOptions;
	int m_Flags;
	bool m_bTrackCRCs;

	// Stats about how the file was opened and how we asked the stdio/steam filesystem to open it.
	// Used to decide whether or not we need to generate and store CRCs.
	bool m_bLoadedFromSteamCache;	// Did it get loaded out of the Steam cache?
	bool m_bSteamCacheOnly;			// Are we asking that this file only come from Steam?
	
	char m_AbsolutePath[MAX_FILEPATH];	// This is set 
};


bool CBaseFileSystem::HandleOpenFromZipFile( CFileOpenInfo &openInfo )
{
	// an absolute path can encode a zip pack file (i.e. caller wants to open the file from within the pack file)
	// format must strictly be ????.zip\????
	// assuming a reasonable restriction that the zip must be a pre-existing search path zip
	char *pZipExt = V_stristr( openInfo.m_AbsolutePath, ".zip" );
	if ( !pZipExt )
	{
		pZipExt = V_stristr( openInfo.m_AbsolutePath, ".bsp" );
	}
	
	if ( pZipExt && pZipExt[4] == CORRECT_PATH_SEPARATOR && pZipExt[5] )
	{
		// want full path to zip only, terminate at slash
		pZipExt[4] = '\0';
		// want relative portion only, everything after the slash
		char *pRelativeFileName = pZipExt + 5;

		// find the zip
		for ( int i=0; i< m_ZipFiles.Count(); i++ )
		{
			CPackFile *pPackFile = m_ZipFiles[i];
			
			if ( Q_stricmp( pPackFile->m_ZipName.Get(), openInfo.m_AbsolutePath ) == 0 )
			{
				openInfo.m_pFileHandle = pPackFile->OpenFile( pRelativeFileName, openInfo.m_pOptions );
#ifdef SUPPORT_VPK
				openInfo.m_pPackFile = pPackFile;
#endif
				openInfo.HandleFileCRCTracking( pRelativeFileName, false );
					
				break;
			}
		}

		if ( openInfo.m_pFileHandle )
			openInfo.SetResolvedFilename( openInfo.m_pFileName );
	
		return true;
	}
	else
	{
		return false;
	}
}

void CBaseFileSystem::HandleOpenFromPackFile( CPackFile *pPackFile, CFileOpenInfo &openInfo )
{
	openInfo.m_pFileHandle = pPackFile->OpenFile( openInfo.m_pFileName, openInfo.m_pOptions );
#ifdef SUPPORT_VPK
	openInfo.m_pPackFile = pPackFile;
#endif

	// HACK! The bsp pack's paths may be different now that we've moved it to the workshop subdir...
	// If we're still failing to find it, remove the workshop/id subdir
	if ( !openInfo.m_pFileHandle ) 
	{
		char szScratch[MAX_PATH];
		V_strcpy_safe( szScratch, openInfo.m_pFileName );
		CFmtStr workshopDir( "%cworkshop%c", CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR );
		char *pStart = V_stristr( szScratch, workshopDir.Access() );
		if ( pStart && pStart[0] && pStart[1] )
		{
			*pStart = 0; // null terminate the first section of the path we're keeping
			pStart++;
			size_t len = strlen( pStart );
			const char *pEnd = V_strnchr( pStart, CORRECT_PATH_SEPARATOR, len ); // skip past workshop path
			if ( pEnd && pEnd[0] && pEnd[1] )
			{
				len = strlen( pEnd );
				pEnd = V_strnchr( pEnd+1, CORRECT_PATH_SEPARATOR, len ); // skip past id path
				if ( pEnd )
				{
					CFmtStr pathNoWorkshop( "%s%s", szScratch, pEnd );
					openInfo.m_pFileHandle = pPackFile->OpenFile( pathNoWorkshop.Access(), openInfo.m_pOptions );
				}
			}
		}
	}

	if ( openInfo.m_pFileHandle )
	{
		char tempStr[MAX_PATH*2+2];
		V_snprintf( tempStr, sizeof( tempStr ), "%s%c%s", pPackFile->m_ZipName.String(), CORRECT_PATH_SEPARATOR, openInfo.m_pFileName );
		openInfo.SetResolvedFilename( tempStr );
	}

	// If it's a BSP file, then the BSP file got CRC'd elsewhere so no need to verify stuff in there.
	if ( !pPackFile->m_bIsMapPath )
		openInfo.HandleFileCRCTracking( openInfo.m_pFileName, false );
}

void CBaseFileSystem::HandleOpenRegularFile( CFileOpenInfo &openInfo, bool bIsAbsolutePath )
{
	// Setup the parameters for the call (like to tell Steam to force the file to come out of the Steam caches or not).
	CFileLoadInfo fileLoadInfo;
	openInfo.DetermineFileLoadInfoParameters( fileLoadInfo, bIsAbsolutePath );
	
	// xbox dvddev mode needs to convolve non-compliant fatx filenames
	// purposely placing this at this level, so only loose files pay the burden
	const char *pFilename = openInfo.m_AbsolutePath;
	bool bFixed = false;
	char fixedFATXFilename[MAX_PATH];
	if ( IsX360() )
	{
		bFixed = FixupFATXFilename( pFilename, fixedFATXFilename, sizeof( fixedFATXFilename ) );
	}

	int64 size;
	FILE *fp = Trace_FOpen( bFixed ? fixedFATXFilename : pFilename, openInfo.m_pOptions, openInfo.m_Flags, &size, &fileLoadInfo );
	if ( fp )
	{
		if ( m_pLogFile )
		{
			LogFileAccess( openInfo.m_AbsolutePath );
		}

		if ( m_bOutputDebugString )
		{
#ifdef _WIN32
			Plat_DebugString( "fs_debug: " );
			Plat_DebugString( openInfo.m_AbsolutePath );
			Plat_DebugString( "\n" );
#elif POSIX
			fprintf(stderr, "fs_debug: %s\n", openInfo.m_AbsolutePath );
#endif
		}

		openInfo.m_pFileHandle = new CFileHandle(this);
		openInfo.m_pFileHandle->m_pFile = fp;
		openInfo.m_pFileHandle->m_type = FT_NORMAL;
		openInfo.m_pFileHandle->m_nLength = size;

		openInfo.SetResolvedFilename( openInfo.m_AbsolutePath );
		
		// Remember what was returned by the Steam filesystem and track the CRC.
		openInfo.m_bLoadedFromSteamCache = fileLoadInfo.m_bLoadedFromSteamCache;
		openInfo.m_bSteamCacheOnly = fileLoadInfo.m_bSteamCacheOnly;
		openInfo.HandleFileCRCTracking( openInfo.m_pFileName, bIsAbsolutePath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: The base file search goes through here
// Input  : *path - 
//			*pFileName - 
//			*pOptions - 
//			packfile - 
//			*filetime - 
// Output : FileHandle_t
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::FindFile( 
	const CSearchPath *path, 
	const char *pFileName, 
	const char *pOptions, 
	unsigned flags, 
	char **ppszResolvedFilename, 
	bool bTrackCRCs )
{
	VPROF( "CBaseFileSystem::FindFile" );

	char tempSymlinkBuffer[MAX_PATH];
	pFileName = V_FormatFilenameForSymlinking( tempSymlinkBuffer, pFileName );
	
	CFileOpenInfo openInfo( this, pFileName, path, pOptions, flags, ppszResolvedFilename, bTrackCRCs );
	bool bIsAbsolutePath = V_IsAbsolutePath( pFileName );
	if ( bIsAbsolutePath )
	{
#ifdef SUPPORT_VPK
		if ( m_VPKFiles.Count()  && ( ! V_stristr( pFileName, ".vpk" ) ) )
		{
			// FileSystemWarning( FILESYSTEM_WARNING, "***VPK: FindFile Attempting to use full path with VPK file!\n\tFile: %s\n", pFileName );
		}
#endif
		openInfo.SetAbsolutePath( "%s", pFileName );

		// Check if it's of the form C:/a/b/c/blah.zip/materials/blah.vtf
		if ( HandleOpenFromZipFile( openInfo ) )
		{
			return (FileHandle_t)openInfo.m_pFileHandle;
		}
	}
	else
	{
		// check vpk file
#ifdef SUPPORT_VPK
		for( int i = 0 ; i < m_VPKFiles.Count(); i++ )
		{
			CPackedStoreFileHandle fHandle = m_VPKFiles[i]->OpenFile( pFileName );
			if ( fHandle )
			{
				openInfo.m_pFileHandle = new CFileHandle(this);
				openInfo.m_pFileHandle->m_VPKHandle = fHandle;
				openInfo.m_pFileHandle->m_type = FT_NORMAL;
				openInfo.m_pFileHandle->m_nLength = fHandle.m_nFileSize;
				openInfo.SetResolvedFilename( openInfo.m_AbsolutePath );
		
				// Remember what was returned by the Steam filesystem and track the CRC.
				openInfo.m_bLoadedFromSteamCache = false;
				openInfo.m_bSteamCacheOnly = false;
				openInfo.m_pVPKFile = m_VPKFiles[i];
				openInfo.HandleFileCRCTracking( openInfo.m_pFileName, false );
				return ( FileHandle_t ) openInfo.m_pFileHandle;
			}
		}
#endif
		// Caller provided a relative path
		if ( path->GetPackFile() )
		{
			HandleOpenFromPackFile( path->GetPackFile(), openInfo );
			return (FileHandle_t)openInfo.m_pFileHandle;
		}
		else
		{
			openInfo.SetAbsolutePath( "%s%s", path->GetPathString(), pFileName );
		}
	}

	// now have an absolute name
	HandleOpenRegularFile( openInfo, bIsAbsolutePath );
	return (FileHandle_t)openInfo.m_pFileHandle;
}


FileHandle_t CBaseFileSystem::FindFileInSearchPaths( 
	const char *pFileName, 
	const char *pOptions, 
	const char *pathID, 
	unsigned flags, 
	char **ppszResolvedFilename, 
	bool bTrackCRCs )
{
	// Run through all the search paths.
	PathTypeFilter_t pathFilter = FILTER_NONE;

#if defined( _GAMECONSOLE ) && defined( _DEBUG )
	// -pakfallbackfs will perform a filesystem search if the
	// requested file is not in pak zip (very expensive!)
	static
		enum PakFallback_t
		{
			PAK_FALLBACK_UNKNOWN, PAK_FALLBACK_ALLOW, PAK_FALLBACK_RETAIL
		}
		s_PakFallbackType = PAK_FALLBACK_UNKNOWN;
	if ( s_PakFallbackType == PAK_FALLBACK_UNKNOWN )
	{
		s_PakFallbackType = CommandLine()->FindParm( "-pakfallbackfs" ) ? PAK_FALLBACK_ALLOW : PAK_FALLBACK_RETAIL;
	}
#define IsPakStrictMode() ( s_PakFallbackType != PAK_FALLBACK_ALLOW )
#else
#define IsPakStrictMode() true
#endif
	
	if ( IsGameConsole() && IsPakStrictMode() )
	{
		if ( flags & FSOPEN_NEVERINPACK )
		{
			pathFilter = FILTER_CULLPACK;
		}
		else
		{
			// most all files on the dvd are expected to be in the pack
			// don't allow disk paths to be searched, which is very expensive on the dvd
			pathFilter = FILTER_CULLNONPACK;
		}
	}

	CSearchPathsIterator iter( this, &pFileName, pathID, pathFilter );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		FileHandle_t filehandle = FindFile( pSearchPath, pFileName, pOptions, flags, ppszResolvedFilename, bTrackCRCs );
		if ( filehandle )
			return filehandle;
	}

	return ( FileHandle_t )0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::OpenForRead( const char *pFileName, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename )
{
	VPROF( "CBaseFileSystem::OpenForRead" );
	return FindFileInSearchPaths( pFileName, pOptions, pathID, flags, ppszResolvedFilename, true );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::OpenForWrite( const char *pFileName, const char *pOptions, const char *pathID )
{
	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pathID, tempPathID );

	// Opening for write or append uses the write path
	// Unless an absolute path is specified...
	const char *pTmpFileName;
	char szScratchFileName[MAX_PATH];
	if ( Q_IsAbsolutePath( pFileName ) )
	{
		pTmpFileName = pFileName;
	}
	else
	{
		ComputeFullWritePath( szScratchFileName, sizeof( szScratchFileName ), pFileName, pathID );
		pTmpFileName = szScratchFileName; 
	}

	int64 size;
	FILE *fp = Trace_FOpen( pTmpFileName, pOptions, 0, &size );
	if ( !fp )
	{
		return ( FileHandle_t )0;
	}

	CFileHandle *fh = new CFileHandle( this );
	fh->m_nLength = size;
	fh->m_type = FT_NORMAL;
	fh->m_pFile = fp;

	return ( FileHandle_t )fh;
}


// This looks for UNC-type filename specifiers, which should be used instead of 
// passing in path ID. So if it finds //mod/cfg/config.cfg, it translates
// pFilename to "cfg/config.cfg" and pPathID to "mod" (mod is placed in tempPathID).
void CBaseFileSystem::ParsePathID( const char* &pFilename, const char* &pPathID, char tempPathID[MAX_PATH] )
{
	tempPathID[0] = 0;
	
	if ( !pFilename || pFilename[0] == 0 )
		return;

	// FIXME: Pain! Backslashes are used to denote network drives, forward to denote path ids
	// HOORAY! We call FixSlashes everywhere. That will definitely not work
	// I'm not changing it yet though because I don't know how painful the bugs would be that would be generated
	bool bIsForwardSlash = ( pFilename[0] == '/' && pFilename[1] == '/' );
//	bool bIsBackwardSlash = ( pFilename[0] == '\\' && pFilename[1] == '\\' );
	if ( !bIsForwardSlash ) //&& !bIsBackwardSlash ) 
		return;

	// Parse out the path ID.
	const char *pIn = &pFilename[2];
	char *pOut = tempPathID;
	while ( *pIn && !PATHSEPARATOR( *pIn ) && (pOut - tempPathID) < (MAX_PATH-1) )
	{
		*pOut++ = *pIn++;
	}

	*pOut = 0;

	// They're specifying two path IDs. Ignore the one passed-in.  
	// AND only warn if they are inconsistent
	if ( pPathID && Q_stricmp( pPathID, tempPathID ) )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS: Specified two path IDs (%s, %s).\n", pFilename, pPathID );
	}
	if ( tempPathID[0] == '*' )
	{
		// * means NULL.
		pPathID = NULL;
	}
	else
	{
		pPathID = tempPathID;
	}

	// Move pFilename up past the part with the path ID.
	if ( *pIn == 0 )
		pFilename = pIn;
	else
		pFilename = pIn + 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::Open( const char *pFileName, const char *pOptions, const char *pathID )
{
	return OpenEx( pFileName, pOptions, 0, pathID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::OpenEx( const char *pFileName, const char *pOptions, unsigned flags, const char *pathID, char **ppszResolvedFilename )
{
	VPROF_BUDGET( "CBaseFileSystem::Open", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	if ( !pFileName )
		return (FileHandle_t)0;

#ifndef _CERT
	s_IoStats.OnFileOpen( pFileName );
#endif

	NoteIO();
	CFileHandleTimer *pTimer = NULL;
	bool bReportLongLoads = ( fs_report_long_reads.GetInt() > 0 );

	if ( bReportLongLoads )
	{
		// When a file is opened we add it to the list and note the time
		pTimer = new CFileHandleTimer;
		if ( pTimer != NULL )
		{
			// Need the lock only when adding to the vector, not during construction
			AUTO_LOCK( m_FileHandleTimersMutex );
			m_FileHandleTimers.AddToTail( pTimer );
			pTimer->Start();
		}
	}

	CHECK_DOUBLE_SLASHES( pFileName );

	if ( fs_report_sync_opens.GetInt() > 0 && ThreadInMainThread() && 
		 !bReportLongLoads ) // If we're reporting timings we have to delay this spew till after the file has been closed
	{
		Warning( "File::Open( %s ) on main thread.\n", pFileName );

#if !defined(_CERT) && !IsPS3() && !IsX360()
		if ( fs_report_sync_opens_callstack.GetInt() > 0 )
		{
			// GetCallstack() does not work on PS3, it is using TLS which is not supported in cross-platform manner
			const int CALLSTACK_SIZE = 16;
			void * pAddresses[CALLSTACK_SIZE];
			const int CALLSTACK_SKIP = 1;
			int nCount = GetCallStack( pAddresses, CALLSTACK_SIZE, CALLSTACK_SKIP );
			if ( nCount != 0)
			{
				// Allocate dynamically instead of using the stack, this path is going to be very rarely used
				const int BUFFER_SIZE = 4096;
				char * pBuffer = new char[ BUFFER_SIZE ];
				TranslateStackInfo( pAddresses, CALLSTACK_SIZE, pBuffer, BUFFER_SIZE, "\n", TSISTYLEFLAG_DEFAULT );
				Msg( "%s\n", pBuffer );
				delete[] pBuffer;
			}
		}
#endif
	}

	// Allow for UNC-type syntax to specify the path ID.
	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pathID, tempPathID );
	char tempFileName[MAX_PATH];
	Q_strncpy( tempFileName, pFileName, sizeof(tempFileName) );
	Q_FixSlashes( tempFileName );
#ifdef _WIN32
	Q_strlower( tempFileName );
#endif

	FileHandle_t hFile;

	// Try each of the search paths in succession
	// FIXME: call createdirhierarchy upon opening for write.
	if ( strstr( pOptions, "r" ) && !strstr( pOptions, "+" ) )
	{
		hFile = OpenForRead( tempFileName, pOptions, flags, pathID, ppszResolvedFilename );
	}
	else
	{
		hFile = OpenForWrite( tempFileName, pOptions, pathID );
	}

	if ( bReportLongLoads )
	{
		// Save the file handle for ID when we close it
		if ( hFile && pTimer )
		{
			pTimer->m_hFile = hFile;
			Q_strncpy( pTimer->m_szName, pFileName, sizeof( pTimer->m_szName ) );

			// See if we've opened this file before so we can accumulate time spent rereading files
			FileOpenDuplicateTime_t *pFileOpenDuplicate = NULL;

			AUTO_LOCK( g_FileOpenDuplicateTimesMutex );
			for ( int nFileOpenDuplicate = g_FileOpenDuplicateTimes.Count() - 1; nFileOpenDuplicate >= 0; --nFileOpenDuplicate )
			{
				FileOpenDuplicateTime_t *pTempFileOpenDuplicate = g_FileOpenDuplicateTimes[ nFileOpenDuplicate ];
				if ( Q_stricmp( pFileName, pTempFileOpenDuplicate->m_szName ) == 0 )
				{
					// Found it!
					pFileOpenDuplicate = pTempFileOpenDuplicate;
					break;
				}
			}

			if ( pFileOpenDuplicate == NULL )
			{
				// We haven't opened this file before, so add it to the list
				pFileOpenDuplicate = new FileOpenDuplicateTime_t;
				if ( pFileOpenDuplicate != NULL )
				{
					g_FileOpenDuplicateTimes.AddToTail( pFileOpenDuplicate );
					Q_strncpy( pFileOpenDuplicate->m_szName, pFileName, sizeof( pTimer->m_szName ) );
				}
			}

			// Increment the number of times we've opened this file
			if ( pFileOpenDuplicate != NULL )
			{
				pFileOpenDuplicate->m_nLoadCount++;
			}
		}
		else
		{
			// File didn't open, pop it off the list
			if ( pTimer != NULL )
			{
				// We need the lock only when removing from the vector, deleting the timer does not need it
				AUTO_LOCK( m_FileHandleTimersMutex );
				for ( int nTimer = m_FileHandleTimers.Count() - 1; nTimer >= 0; --nTimer )
				{
					CFileHandleTimer *pLocalTimer = m_FileHandleTimers[ nTimer ];
					if ( pLocalTimer == pTimer )
					{
						m_FileHandleTimers.Remove( nTimer );
						break;
					}
				}
				delete pTimer;
			}
		}
	}

	return hFile;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Close( FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::Close", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	if ( !file )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to Close NULL file handle!\n" );
		return;
	}

	unsigned long ulLongLoadThreshold = fs_report_long_reads.GetInt();
	if ( ulLongLoadThreshold > 0 )
	{
		// Let's find the nTimer that matches the file (we assume that we only have to close once 
		CFileHandleTimer *pTimer = NULL;
		{
			AUTO_LOCK( m_FileHandleTimersMutex );
			// Still do from the end to the beginning for consistency with previous code (and make access to Count() only once).
			for ( int nTimer = m_FileHandleTimers.Count() - 1; nTimer >= 0; --nTimer )
			{
				CFileHandleTimer *pLocalTimer = m_FileHandleTimers[ nTimer ];
				if ( pLocalTimer && pLocalTimer->m_hFile == file )
				{
					pTimer = pLocalTimer;
					m_FileHandleTimers.Remove( nTimer );
					break;
				}
			}
		}

		// m_FileHandleTimers is not locked here (but we can still access pTimer)
		if ( pTimer != NULL )
		{
			// Found the file, report the time between opening and closing
			pTimer->End();

			unsigned long ulMicroseconds = pTimer->GetDuration().GetMicroseconds();

			if ( ulLongLoadThreshold <= ulMicroseconds )
			{
				Warning( "Open( %lu microsecs, %s )\n", ulMicroseconds, pTimer->m_szName );
			}

			// Accumulate time spent if this file has been opened at least twice
			{
				AUTO_LOCK( g_FileOpenDuplicateTimesMutex );
				for ( int nFileOpenDuplicate = 0; nFileOpenDuplicate < g_FileOpenDuplicateTimes.Count(); nFileOpenDuplicate++ )
				{
					FileOpenDuplicateTime_t *pFileOpenDuplicate = g_FileOpenDuplicateTimes[ nFileOpenDuplicate ];
					if ( Q_stricmp( pTimer->m_szName, pFileOpenDuplicate->m_szName ) == 0 )
					{
						if ( pFileOpenDuplicate->m_nLoadCount > 1 )
						{
							pFileOpenDuplicate->m_flAccumulatedMicroSeconds += pTimer->GetDuration().GetMicrosecondsF();
						}
						break;
					}
				}
			}

			delete pTimer;
		}
	}

	delete (CFileHandle*)file;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Seek( FileHandle_t file, int pos, FileSystemSeek_t whence )
{
	VPROF_BUDGET( "CBaseFileSystem::Seek", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	CFileHandle *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "Tried to Seek NULL file handle!\n" );
		return;
	}

#ifndef _CERT
	int nTimeStart = Plat_MSTime();
#endif
	fh->Seek( pos, whence );
#ifndef _CERT
	s_IoStats.OnFileSeek( Plat_MSTime() - nTimeStart );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Tell( FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::Tell", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	if ( !file )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to Tell NULL file handle!\n" );
		return 0;
	}


	// Pack files are relative
	return (( CFileHandle *)file)->Tell(); 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Size( FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::Size", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	if ( !file )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to Size NULL file handle!\n" );
		return 0;
	}

	return ((CFileHandle *)file)->Size();
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Size( const char* pFileName, const char *pPathID )
{
	VPROF_BUDGET( "CBaseFileSystem::Size", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	CHECK_DOUBLE_SLASHES( pFileName );
	
	// handle the case where no name passed...
	if ( !pFileName || !pFileName[0] )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to Size NULL filename!\n" );
		return 0;
	}

	if ( IsPC() )
	{
		// If we have a whitelist and it's forcing the file to load from Steam instead of from disk,
		// then do this the slow way, otherwise we'll get the wrong file size (i.e. the size of the file on disk).
		CWhitelistSpecs *pWhitelist = m_FileWhitelist.AddRef();
		if ( pWhitelist )
		{
			bool bAllowFromDisk = pWhitelist->m_pAllowFromDiskList->IsFileInList( pFileName );
			m_FileWhitelist.ReleaseRef( pWhitelist );
			
			if ( !bAllowFromDisk )
			{
				FileHandle_t fh = Open( pFileName, "rb", pPathID );
				if ( fh )
				{
					unsigned int ret = Size( fh );
					Close( fh );
					return ret;
				}
				else
				{
					return 0;
				}
			}
		}
	}
	
	// Ok, fall through to the fast path.
	int iSize = 0;

	CSearchPathsIterator iter( this, &pFileName, pPathID );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		iSize = FastFindFile( pSearchPath, pFileName );
		if ( iSize >= 0 )
		{
			break;
		}
	}
	return iSize;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			*pFileName - 
// Output : long
//-----------------------------------------------------------------------------
long CBaseFileSystem::FastFileTime( const CSearchPath *path, const char *pFileName )
{
	struct	_stat buf;
	
	char tempSymlinkBuffer[MAX_PATH];
	pFileName = V_FormatFilenameForSymlinking( tempSymlinkBuffer, pFileName );

	if ( path->GetPackFile() )
	{
		int		nIndex, nLength;
		int64	nPosition;

		// If we found the file:
		if ( path->GetPackFile()->FindFile( pFileName, nIndex, nPosition, nLength ) )
		{
			return (path->GetPackFile()->m_lPackFileTime);
		}
	}
	else
	{
		// Is it an absolute path?
		char tempFileName[ MAX_FILEPATH ]; 
		
		if ( Q_IsAbsolutePath( pFileName ) )
		{
			Q_strncpy( tempFileName, pFileName, sizeof( tempFileName ) );
#ifdef SUPPORT_VPK
			if ( m_VPKFiles.Count() )
			{
#ifdef _DEBUG
				FileSystemWarning( FILESYSTEM_WARNING, "***VPK: FastFileTime Attempting to use full path with VPK file!\n\tFile: %s\n", pFileName );
#endif
			}
#endif
		}
		else
		{
			bool bFileInVpk = false;
#ifdef SUPPORT_VPK
			if ( m_VPKFiles.Count() )
			{
				Q_strncpy( tempFileName, pFileName, sizeof( tempFileName ) );
				Q_FixSlashes( tempFileName );
				// check vpk file
				for ( int i = 0; i < m_VPKFiles.Count(); i++ )
				{
					CPackedStoreFileHandle fHandle = m_VPKFiles[i]->OpenFile( tempFileName );
					if ( fHandle )
					{
						// File found in VPK - return file time of the VPK
						m_VPKFiles[i]->GetPackFileName( fHandle, tempFileName, sizeof( tempFileName ) );
						bFileInVpk = true;
						break;
					}
				}
			}
#endif

			if ( !bFileInVpk )
			{
				Q_snprintf( tempFileName, sizeof( tempFileName ), "%s%s", path->GetPathString(), pFileName );
			}
		}
		Q_FixSlashes( tempFileName );

		bool bFixed = false;
		char fixedFATXFilename[MAX_PATH];
		if ( IsX360() )
		{
			bFixed = FixupFATXFilename( tempFileName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
		}

		if ( FS_stat( bFixed ? fixedFATXFilename : tempFileName, &buf ) != -1 )
		{
			return buf.st_mtime;
		}
#ifdef LINUX
		// Support Linux and its case sensitive file system
		char realName[MAX_PATH];
		const char *pRealName = findFileInDirCaseInsensitive( tempFileName, realName );
		if ( pRealName && FS_stat( pRealName, &buf ) != -1 )
		{
			return buf.st_mtime;
		}
#endif
	}

	return ( 0L );
}

int CBaseFileSystem::FastFindFile( const CSearchPath *path, const char *pFileName )
{
	struct	_stat buf;
	
	char tempSymlinkBuffer[MAX_PATH];
	pFileName = V_FormatFilenameForSymlinking( tempSymlinkBuffer, pFileName );

	if ( path->GetPackFile() )
	{
		int nIndexResult;
		int64 nOffsetResult;
		int nLengthResult;
		if ( path->GetPackFile()->FindFile( pFileName, nIndexResult, nOffsetResult, nLengthResult ) )
		{
			return nLengthResult;
		}
		else
		{
			return -1;
		}
	}

	char tempFileName[ MAX_FILEPATH ]; 

	// Is it an absolute path?	
	bool bRelativePath = !Q_IsAbsolutePath( pFileName );

#ifdef SUPPORT_VPK
	// NOTE: Pack files need relative paths
	if ( !bRelativePath && m_VPKFiles.Count() )
	{
#ifdef _DEBUG
		FileSystemWarning( FILESYSTEM_WARNING, "***VPK: FastFindFile Attempting to use full path with VPK file!\n\tFile: %s\n", pFileName );
#endif
	}
	if ( bRelativePath )
	{
		Q_strncpy( tempFileName, pFileName, sizeof( tempFileName ) );
		Q_FixSlashes( tempFileName );
		// check vpk file
		for( int i = 0 ; i < m_VPKFiles.Count(); i++ )
		{
			CPackedStoreFileHandle fHandle = m_VPKFiles[i]->OpenFile( tempFileName );
			if ( fHandle )
			{
				return fHandle.m_nFileSize;
			}
		}
	}
#endif

	if ( !bRelativePath )
	{
		Q_strncpy( tempFileName, pFileName, sizeof( tempFileName ) );
	}
	else
	{
		Q_snprintf( tempFileName, sizeof( tempFileName ), "%s%s", path->GetPathString(), pFileName );
	}
	Q_FixSlashes( tempFileName );

	bool bFixed = false;
	char fixedFATXFilename[MAX_PATH];
	if ( IsX360() )
	{
		bFixed = FixupFATXFilename( tempFileName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
	}

#if defined(_PS3)
	FixUpPathCaseForPS3(tempFileName);
#endif
	if ( FS_stat( bFixed ? fixedFATXFilename : tempFileName, &buf ) != -1 )
	{
		LogAccessToFile( "stat", tempFileName, "" );
		return buf.st_size;
	}

#ifdef LINUX
	// Support Linux and its case sensitive file system
	char realName[MAX_PATH];
	if ( findFileInDirCaseInsensitive( tempFileName, realName ) && FS_stat( realName, &buf ) != -1 )
	{
		return buf.st_size;
	}
#endif

	return ( -1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::EndOfFile( FileHandle_t file )
{
	if ( !file )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to EndOfFile NULL file handle!\n" );
		return true;
	}

	return ((CFileHandle *)file)->EndOfFile();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::Read( void *pOutput, int size, FileHandle_t file )
{
	return ReadEx( pOutput, size, size, file );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::ReadEx( void *pOutput, int destSize, int size, FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::Read", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	NoteIO();
	if ( !file )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to Read NULL file handle!\n" );
		return 0;
	}
	if ( size < 0 )
	{
		return 0;
	}

#ifndef _CERT
	int nTimeStart = Plat_MSTime();
#endif
	int nRet = ((CFileHandle*)file)->Read(pOutput, destSize, size );
#ifndef _CERT
	s_IoStats.OnFileRead( Plat_MSTime() - nTimeStart, size );
#endif

	return nRet;

}

//-----------------------------------------------------------------------------
// Purpose: Takes a passed in KeyValues& head and fills in the precompiled keyvalues data into it.
// Input  : head - 
//			type - 
//			*filename - 
//			*pPathID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::LoadKeyValues( KeyValues& head, KeyValuesPreloadType_t type, char const *filename, char const *pPathID /*= 0*/ )
{
	bool bret = true;

#ifndef DEDICATED
	char tempPathID[MAX_PATH];
	ParsePathID( filename, pPathID, tempPathID );
	bret = head.LoadFromFile( this, filename, pPathID );
#else
	bret = head.LoadFromFile( this, filename, pPathID );
#endif
	return bret;
}


//-----------------------------------------------------------------------------
// Purpose: If the "PreloadedData" hasn't been purged, then this'll try and instance the KeyValues using the fast path of 
/// compiled keyvalues loaded during startup.
// Otherwise, it'll just fall through to the regular KeyValues loading routines
// Input  : type - 
//			*filename - 
//			*pPathID - 
// Output : KeyValues
//-----------------------------------------------------------------------------
KeyValues *CBaseFileSystem::LoadKeyValues( KeyValuesPreloadType_t type, char const *filename, char const *pPathID /*= 0*/ )
{
	KeyValues *kv = new KeyValues( filename );
	if ( kv )
	{
		kv->LoadFromFile( this, filename, pPathID );
	}

	return kv;
}

//-----------------------------------------------------------------------------
// Purpose: This is the fallback method of reading the name of the first key in the file
// Input  : *filename - 
//			*pPathID - 
//			*rootName - 
//			bufsize - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::LookupKeyValuesRootKeyName( char const *filename, char const *pPathID, char *rootName, size_t bufsize )
{
	if ( FileExists( filename, pPathID ) )
	{
		// open file and get shader name
		FileHandle_t hFile = Open( filename, "r", pPathID );
		if ( hFile == FILESYSTEM_INVALID_HANDLE )
		{
			return false;
		}

		char buf[ 128 ];
		ReadLine( buf, sizeof( buf ), hFile );
		Close( hFile );

		// The name will possibly come in as "foo"\n

		// So we need to strip the starting " character
		char *pStart = buf;
		if ( *pStart == '\"' )
		{
			++pStart;
		}
		// Then copy the rest of the string
		Q_strncpy( rootName, pStart, bufsize );

		// And then strip off the \n and the " character at the end, in that order
		int len = Q_strlen( pStart );
		while ( len > 0 && rootName[ len - 1 ] == '\n' )
		{
			rootName[ len - 1 ] = 0;
			--len;
		}
		while ( len > 0 && rootName[ len - 1 ] == '\"' )
		{
			rootName[ len - 1 ] = 0;
			--len;
		}
	}
	else
	{
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetupPreloadData()
{
	int i;

	for ( i = 0; i < m_SearchPaths.Count(); i++ )
	{
		CPackFile* pPF = m_SearchPaths[i].GetPackFile();
		if ( pPF ) 
		{
			pPF->SetupPreloadData();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::DiscardPreloadData()
{
	int i;
	for( i = 0; i < m_SearchPaths.Count(); i++ )
	{
		CPackFile* pf = m_SearchPaths[i].GetPackFile();
		if ( pf )
		{
			pf->DiscardPreloadData();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::Write( void const* pInput, int size, FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::Write", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	AUTOBLOCKREPORTER_FH( Write, this, true, file, FILESYSTEM_BLOCKING_SYNCHRONOUS, FileBlockingItem::FB_ACCESS_WRITE );

	CFileHandle *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to Write NULL file handle!\n" );
		return 0;
	}
	return fh->Write( pInput, size );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::FPrintf( FileHandle_t file, const char *pFormat, ... )
{
	va_list args;
	va_start( args, pFormat );
	VPROF_BUDGET( "CBaseFileSystem::FPrintf", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	CFileHandle *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to FPrintf NULL file handle!\n" );
		return 0;
	}
/*
	if ( !fh->GetFileHandle() )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to FPrintf NULL file pointer inside valid file handle!\n" );
		return 0;
	}
	*/


	char buffer[65535];
	int len = vsnprintf( buffer, sizeof( buffer), pFormat, args );
	len = fh->Write( buffer, len );
	//int len = FS_vfprintf( fh->GetFileHandle() , pFormat, args );
	va_end( args );

	
	return len;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetBufferSize( FileHandle_t file, unsigned nBytes )
{
	CFileHandle *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to SetBufferSize NULL file handle!\n" );
		return;
	}
	fh->SetBufferSize( nBytes );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::IsOk( FileHandle_t file )
{
	CFileHandle *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to IsOk NULL file handle!\n" );
		return false;
	}

	return fh->IsOK();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Flush( FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::Flush", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	CFileHandle *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to Flush NULL file handle!\n" );
		return;
	}

	fh->Flush();

}

bool CBaseFileSystem::Precache( const char *pFileName, const char *pPathID)
{
	CHECK_DOUBLE_SLASHES( pFileName );

	// Allow for UNC-type syntax to specify the path ID.
	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pPathID, tempPathID );
	Assert( pPathID );

	// Really simple, just open, the file, read it all in and close it. 
	// We probably want to use file mapping to do this eventually.
	FileHandle_t f = Open( pFileName, "rb", pPathID );
	if ( !f )
		return false;

	// not for consoles, the read discard is a negative benefit, slow and clobbers small drive caches
	if ( IsPC() )
	{
		char buffer[16384];
		while( sizeof(buffer) == Read(buffer,sizeof(buffer),f) );
	}

	Close( f );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char *CBaseFileSystem::ReadLine( char *pOutput, int maxChars, FileHandle_t file )
{
	VPROF_BUDGET( "CBaseFileSystem::ReadLine", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	CFileHandle *fh = ( CFileHandle *)file;
	if ( !fh )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to ReadLine NULL file handle!\n" );
		return NULL;
	}
	m_Stats.nReads++;

	int nRead = 0;

	// Read up to maxchars:
	while( nRead < ( maxChars - 1 ) )
	{
		// Are we at the end of the file?
		if( 1 != fh->Read( pOutput + nRead, 1 ) )
			break;

		// Translate for text mode files:
		if( fh->m_type == FT_PACK_TEXT && pOutput[nRead] == '\r' )
		{
			// Ignore \r
			continue;
		}

		// We're done when we hit a '\n'
		if( pOutput[nRead] == '\n' )
		{
			nRead++;
			break;
		}

		// Get outta here if we find a NULL.
		if( pOutput[nRead] == '\0' )
		{
			pOutput[nRead] = '\n';
			nRead++;
			break;
		}

		nRead++;
	}

	if( nRead < maxChars )
		pOutput[nRead] = '\0';

	
	m_Stats.nBytesRead += nRead;
	return ( nRead ) ? pOutput : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
// Output : long
//-----------------------------------------------------------------------------
long CBaseFileSystem::GetFileTime( const char *pFileName, const char *pPathID )
{
	VPROF_BUDGET( "CBaseFileSystem::GetFileTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	CHECK_DOUBLE_SLASHES( pFileName );

	CSearchPathsIterator iter( this, &pFileName, pPathID );

	char tempFileName[MAX_PATH];
	Q_strncpy( tempFileName, pFileName, sizeof(tempFileName) );
	Q_FixSlashes( tempFileName );
#ifdef _WIN32
	Q_strlower( tempFileName );
#endif

	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		long ft = FastFileTime( pSearchPath, tempFileName );
		if ( ft != 0L )
		{
			if ( !pSearchPath->GetPackFile() && m_LogFuncs.Count() )
			{
				char pTmpFileName[ MAX_FILEPATH ]; 
				if ( strchr( tempFileName, ':' ) ) // 
				{
					Q_strncpy( pTmpFileName, tempFileName, sizeof( pTmpFileName ) );
				}
				else
				{
					Q_snprintf( pTmpFileName, sizeof( pTmpFileName ), "%s%s", pSearchPath->GetPathString(), tempFileName );
				}

				Q_FixSlashes( pTmpFileName );

				LogAccessToFile( "filetime", pTmpFileName, "" );
			}

			return ft;
		}
	}
	return 0L;
}

long CBaseFileSystem::GetPathTime( const char *pFileName, const char *pPathID )
{
	VPROF_BUDGET( "CBaseFileSystem::GetFileTime", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	CSearchPathsIterator iter( this, &pFileName, pPathID );

	char tempFileName[MAX_PATH];
	Q_strncpy( tempFileName, pFileName, sizeof(tempFileName) );
	Q_FixSlashes( tempFileName );
#ifdef _WIN32
	Q_strlower( tempFileName );
#endif

	long pathTime = 0L;
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		long ft = FastFileTime( pSearchPath, tempFileName );
		if ( ft > pathTime )
			pathTime = ft;
		if ( ft != 0L )
		{
			if ( !pSearchPath->GetPackFile() && m_LogFuncs.Count() )
			{
				char pTmpFileName[ MAX_FILEPATH ]; 
				if ( strchr( tempFileName, ':' ) )
				{
					Q_strncpy( pTmpFileName, tempFileName, sizeof( pTmpFileName ) );
				}
				else
				{
					Q_snprintf( pTmpFileName, sizeof( pTmpFileName ), "%s%s", pSearchPath->GetPathString(), tempFileName );
				}

				Q_FixSlashes( pTmpFileName );

				LogAccessToFile( "filetime", pTmpFileName, "" );
			}
		}
	}
	return pathTime;
}


bool CBaseFileSystem::ShouldGameReloadFile( const char *pFilename )
{
	if ( IsGameConsole() )
	{
		return false;
	}

	if ( V_IsAbsolutePath( pFilename ) )
	{
		if ( m_WhitelistSpewFlags & WHITELIST_SPEW_RELOAD_FILES )
		{
			Msg( "Whitelist -       reload (absolute path) %s\n", pFilename );
		}

		// They should be checking with relative filenames, but this is easy to remedy if we need to.
		// Easy enough to remedy if we want.. just strip off the path ID prefixes until we find the file.
		Assert( false );	
		return true;
	}

	CFileInfo *fileInfos[256];
	int nFileInfos = m_FileTracker.GetFileInfos( fileInfos, ARRAYSIZE( fileInfos ), pFilename );
	if ( nFileInfos == 0 )
	{
		// Ain't heard of this file. It probably came from a BSP or a pak file.
		if ( m_WhitelistSpewFlags & WHITELIST_SPEW_DONT_RELOAD_FILES )
		{
			Msg( "Whitelist - don't reload (unheard-of-file) %s\n", pFilename );
		}			
		return false;
	}

	// Note: This might be null, in which case all files are allowed to come off disk.
	bool bFileAllowedToComeFromDisk = true;
	CWhitelistSpecs *pWhitelist = m_FileWhitelist.GetInMainThread();
	if ( pWhitelist )
		bFileAllowedToComeFromDisk = pWhitelist->m_pAllowFromDiskList->IsFileInList( pFilename );
	
	// Since we don't require the game to specify which path ID it's interested in here, there's a small amount
	// of ambiguity here (because 2 files with the same name could have been loaded from different path IDs).
	// This case should be extremely rare, and we error on the side of simplicity (don't require the game to 
	// remember which path ID its files were opened from).
	//
	// In the case where there are multiple files, we error on the side of reloading the file - if any of the
	// files here would require a reload, then we tell the game to reload this file even if it might not
	// have strictly needed to reload it.
	bool bRet = false;
	for ( int i=0; i < nFileInfos; i++ )
	{
		CFileInfo *pFileInfo = fileInfos[i];
		
		// See comments above k_eFileFlagsFailedToLoadLastTime for info about this case.
		if ( bFileAllowedToComeFromDisk && (pFileInfo->m_Flags & k_eFileFlagsFailedToLoadLastTime) )
		{
			bRet = true;
			break;
		}
		
		if ( pFileInfo->m_Flags & k_eFileFlagsLoadedFromSteam )
		{
			if ( (pFileInfo->m_Flags & k_eFileFlagsForcedLoadFromSteam) && bFileAllowedToComeFromDisk )
			{
				// So.. the last time we loaded this file, we forced it to come from Steam, but the new whitelist says it's ok if this
				// file is loaded off disk. So reload it. 
				//
				// TODO: we could optimize this by checking if there even IS a file on disk that would override the Steam one,
				//       and in that case, don't tell the game to reload it.
				//
				//       We could also optimize it if we remembered whether we told Steam to allow loads off disk or not.
				//		 If we did allow loads and Steam still got the file from Steam, then we know that there isn't
				//		 a file on disk here, and therefore we wouldn't have to reload it now.
				bRet = true;
				break;
			}
			else
			{
				// So we loaded the file from Steam last time, and the new whitelist only allows it to come from Steam, so we're ok.
				//return false;
			}
		}
		else
		{
			if ( bFileAllowedToComeFromDisk )
			{
				// No need to reload. The new whitelist says this file can come off disk, and the last time we loaded it, it was off disk.
				// The client will still verify the CRC of the file with the server to make sure its file is legit..
				//return false;
			}
			else
			{
				// The file was loaded off disk but the server won't allow that now.
				bRet = true;
				break;
			}
		}
	}

	if ( (m_WhitelistSpewFlags & WHITELIST_SPEW_RELOAD_FILES) && bRet )
		Msg( "Whitelist -       reload %s\n", pFilename );

	if ( (m_WhitelistSpewFlags & WHITELIST_SPEW_DONT_RELOAD_FILES) && !bRet )
		Msg( "Whitelist - don't reload %s\n", pFilename );

	return bRet;
}


void CBaseFileSystem::MarkAllCRCsUnverified()
{
	if ( IsGameConsole() )
	{
		return;
	}

	m_FileTracker2.MarkAllCRCsUnverified();
}


void CBaseFileSystem::CacheFileCRCs( const char *pPathname, ECacheCRCType eType, IFileList *pFilter )
{
	if ( IsGameConsole() )
	{
		return;
	}

	// Get a list of the unique search path names (mod, game, platform, etc).
	CUtlDict<int,int> searchPathNames;
	m_SearchPathsMutex.Lock();
	for ( int i = 0; i <  m_SearchPaths.Count(); i++ )
	{
		CSearchPath *pSearchPath = &m_SearchPaths[i];
		if ( searchPathNames.Find( pSearchPath->GetPathIDString() ) == searchPathNames.InvalidIndex() )
			searchPathNames.Insert( pSearchPath->GetPathIDString() );
	}
	m_SearchPathsMutex.Unlock();

	CacheFileCRCs_R( pPathname, eType, pFilter, searchPathNames );
}


void CBaseFileSystem::CacheFileCRCs_R( const char *pPathname, ECacheCRCType eType, IFileList *pFilter, CUtlDict<int,int> &searchPathNames )
{
	if ( IsGameConsole() )
	{
		return;
	}

	char searchStr[MAX_PATH];
	bool bRecursive = false;

	if ( eType == k_eCacheCRCType_SingleFile )
	{
		V_snprintf( searchStr, sizeof( searchStr ), "%s", pPathname );
	}
	else if ( eType == k_eCacheCRCType_Directory )
	{
		V_ComposeFileName( pPathname, "*.*", searchStr, sizeof( searchStr ) );
	}
	else if ( eType == k_eCacheCRCType_Directory_Recursive )
	{
		V_ComposeFileName( pPathname, "*.*", searchStr, sizeof( searchStr ) );
		bRecursive = true;
	}
	
	// Get the path we're searching in.
	char pathDirectory[MAX_PATH];
	V_strncpy( pathDirectory, searchStr, sizeof( pathDirectory ) );
	V_StripLastDir( pathDirectory, sizeof( pathDirectory ) );

	/*
	Note:	This is tricky because the client could check different path IDs with the same filename and we'd either
			have the same file or a different file depending on these two cases:
			
			a) they have one file : hl2\blah.txt
			   (in this case, checking the GAME and MOD path IDs for blah.txt return the same file CRC)
			   
			b) they have two files: hl2\blah.txt AND hl2mp\blah.txt
			   (in this case, checking the GAME and MOD path IDs for blah.txt return different file CRCs)
	*/				

	
	CUtlDict< CUtlVector<CStoreIDEntry>* ,int> filesByStoreID;			// key=filename, value=list of store IDs this filename was found in
	for ( int i=searchPathNames.First(); i != searchPathNames.InvalidIndex(); i = searchPathNames.Next( i ) )
	{
		// Now find all the files..
		int foundStoreID;
		const char *pPathIDStr = searchPathNames.GetElementName( i );
		FileFindHandle_t findHandle;
		const char *pFilename = FindFirstHelper( searchStr, pPathIDStr, &findHandle, &foundStoreID );
		while ( pFilename )
		{
			if ( pFilename[0] != '.' )
			{
				char relativeName[MAX_PATH];
				V_ComposeFileName( pathDirectory, pFilename, relativeName, sizeof( relativeName ) );

				if ( FindIsDirectory( findHandle ) )
				{
					if ( bRecursive )
						CacheFileCRCs_R( relativeName, eType, pFilter, searchPathNames );
				}
				else
				{
					if ( pFilter->IsFileInList( relativeName ) )
					{
						CStoreIDEntry *pPrevRecord = FindPrevFileByStoreID( filesByStoreID, pFilename, pPathIDStr, foundStoreID );
						if ( pPrevRecord )
						{
							// Ok, we already found this file in an earlier search path with the same storeID (i.e. the exact same disk path)
							// so rather than recalculate the CRC redundantly, just copy the CRC from the previous one into a record with the new path ID string.
							// This saves a lot of redundant CRC calculations since logdir, default_write_path, game, and mod all share directories.
							m_FileTracker.CacheFileCRC_Copy( pPathIDStr, relativeName, pPrevRecord->m_PathIDString.String() );
						}
						else 
						{
							// Ok, we want the CRC for this file.
							m_FileTracker.CacheFileCRC( pPathIDStr, relativeName );
						}
					}
				}
			}

			FindData_t *pFindData = &m_FindData[findHandle];
			if ( !FindNextFileHelper( pFindData, &foundStoreID ) )
				break;
			
			pFilename = pFindData->findData.cFileName;
		}
		FindClose( findHandle );
	}
	filesByStoreID.PurgeAndDeleteElements();
}


EFileCRCStatus CBaseFileSystem::CheckCachedFileHash( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash )
{
	return m_FileTracker2.CheckCachedFileHash( pPathID, pRelativeFilename, nFileFraction, pFileHash );
}


void CBaseFileSystem::EnableWhitelistFileTracking( bool bEnable, bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes )
{
	if ( IsGameConsole() )
	{
		m_WhitelistFileTrackingEnabled = false;
		return;
	}

	if ( m_WhitelistFileTrackingEnabled != -1 )
	{
		Error( "CBaseFileSystem::EnableWhitelistFileTracking called more than once." );
	}
	
	m_WhitelistFileTrackingEnabled = bEnable;
	if ( m_WhitelistFileTrackingEnabled && bCacheAllVPKHashes )
	{
		CacheAllVPKFileHashes( bCacheAllVPKHashes, bRecalculateAndCheckHashes );
	}
}


void CBaseFileSystem::CacheAllVPKFileHashes( bool bCacheAllVPKHashes, bool bRecalculateAndCheckHashes )
{
#ifdef SUPPORT_VPK
	for( int i = 0 ; i < m_VPKFiles.Count(); i++ )
	{
		if ( !m_VPKFiles[i]->BTestDirectoryHash() )
		{
			Msg( "VPK dir file hash does not match. File corrupted or modified.\n" );
		}
		if ( !m_VPKFiles[i]->BTestMasterChunkHash() )
		{
			Msg( "VPK chunk hash hash does not match. File corrupted or modified.\n" );
		}

		CUtlVector<ChunkHashFraction_t> &vecChunkHash = m_VPKFiles[i]->AccessPackFileHashes();
		CPackedStoreFileHandle fhandle = m_VPKFiles[i]->GetHandleForHashingFiles();
		CUtlVector<ChunkHashFraction_t> vecChunkHashFractionCopy;
		if ( bRecalculateAndCheckHashes )
		{
			CUtlString sPackFileErrors;
			m_VPKFiles[i]->GetPackFileLoadErrorSummary( sPackFileErrors );

			if ( sPackFileErrors.Length() )
			{
				Msg( "Errors occured loading files.\n" );
				Msg( "%s", sPackFileErrors.String() );
				Msg( "Verify integrity of your game files, perform memory and disk diagnostics on your system.\n" );
			}
			else
				Msg( "No VPK Errors occured loading files.\n" );

			Msg( "Recomputing all VPK file hashes.\n" );
			vecChunkHashFractionCopy.Swap( vecChunkHash );
		}
		int cFailures = 0;
		if ( vecChunkHash.Count() == 0 )
		{
			if ( vecChunkHashFractionCopy.Count() == 0 )
				Msg( "File hash information not found: Hashing all VPK files for pure server operation.\n" );
			m_VPKFiles[i]->HashAllChunkFiles();
			if ( vecChunkHashFractionCopy.Count() != 0 )
			{
				if ( vecChunkHash.Count() != vecChunkHashFractionCopy.Count() )
				{
					Msg( "VPK hash count does not match. VPK content may be corrupt.\n" );
				}
				else if  ( Q_memcmp( vecChunkHash.Base(), vecChunkHashFractionCopy.Base(), vecChunkHash.Count()*sizeof(vecChunkHash[0])) != 0 )
				{
					Msg( "VPK hashes do not match. VPK content may be corrupt.\n" );
					// find the actual mismatch
					FOR_EACH_VEC( vecChunkHashFractionCopy, iHash )
					{
						if ( Q_memcmp( vecChunkHashFractionCopy[iHash].m_md5contents.bits, vecChunkHash[iHash].m_md5contents.bits, sizeof( vecChunkHashFractionCopy[iHash].m_md5contents.bits ) ) != 0 )
						{
							Msg( "VPK hash for file %d failure at offset %x.\n", vecChunkHashFractionCopy[iHash].m_nPackFileNumber, vecChunkHashFractionCopy[iHash].m_nFileFraction );
							cFailures++;
						}
					}
				}
			}
		}
		if ( bCacheAllVPKHashes )
		{
			Msg( "Loading VPK file hashes for pure server operation.\n" );
			FOR_EACH_VEC( vecChunkHash, i )
			{
				m_FileTracker2.AddFileHashForVPKFile( vecChunkHash[i].m_nPackFileNumber, vecChunkHash[i].m_nFileFraction, vecChunkHash[i].m_cbChunkLen, vecChunkHash[i].m_md5contents, fhandle );
			}
		}
		else
		{
			if ( cFailures == 0 && vecChunkHash.Count() == vecChunkHashFractionCopy.Count() )
				Msg( "File hashes checked. %d matches. no failures.\n", vecChunkHash.Count() );
			else
				Msg( "File hashes checked. %d matches. %d failures.\n", vecChunkHash.Count(), cFailures );
		}
	}
#endif
}


bool CBaseFileSystem::CheckVPKFileHash( int PackFileID, int nPackFileNumber, int nFileFraction, MD5Value_t &md5Value )
{
#ifdef SUPPORT_VPK
	for( int i = 0 ; i < m_VPKFiles.Count(); i++ )
	{
		if ( m_VPKFiles[i]->m_PackFileID == PackFileID )
		{
			ChunkHashFraction_t fileHashFraction;
			if ( m_VPKFiles[i]->FindFileHashFraction( nPackFileNumber, nFileFraction, fileHashFraction ) )
			{
				CPackedStoreFileHandle fhandle = m_VPKFiles[i]->GetHandleForHashingFiles();
				fhandle.m_nFileNumber = nPackFileNumber;
				char szFileName[MAX_PATH];

				m_VPKFiles[i]->GetPackFileName( fhandle, szFileName, sizeof(szFileName) );

				char hex[ 34 ];
				Q_memset( hex, 0, sizeof( hex ) );
				Q_binarytohex( (const byte *)md5Value.bits, sizeof( md5Value.bits ), hex, sizeof( hex ) );

				char hex2[ 34 ];
				Q_memset( hex2, 0, sizeof( hex2 ) );
				Q_binarytohex( (const byte *)fileHashFraction.m_md5contents.bits, sizeof( fileHashFraction.m_md5contents.bits ), hex2, sizeof( hex2 ) );

				if ( Q_memcmp( fileHashFraction.m_md5contents.bits, md5Value.bits, sizeof(md5Value.bits) ) != 0 )
				{
					Msg( "File %s offset %x hash %s does not match ( should be %s ) \n", szFileName, nFileFraction, hex, hex2 );
					return false;
				}
				else
				{
					return true;
				}
			}
		}
	}
	return false;
#endif
}


void CBaseFileSystem::RegisterFileWhitelist( IFileList *pWantCRCList, IFileList *pAllowFromDiskList, IFileList **pFilesToReload )
{
	if ( IsGameConsole() )
	{
		return;
	}

	CWhitelistSpecs *pOldList = m_FileWhitelist.GetInMainThread();
	if ( pOldList )
	{
		m_FileWhitelist.ReleaseRef( pOldList );					// Get rid of our reference to it so it can be freed.
		m_FileWhitelist.ResetWhenNoRemainingReferences( NULL );	// Wait for everyone else to stop hanging onto it, then free it.
	
		// Free the old ones (other threads shouldn't have access to these anymore because 
		pOldList->m_pAllowFromDiskList->Release();
		pOldList->m_pWantCRCList->Release();
	}
	
	if ( pAllowFromDiskList && pWantCRCList )
	{
		CWhitelistSpecs *pNewList = new CWhitelistSpecs;
		pNewList->m_pAllowFromDiskList = pAllowFromDiskList;
		pNewList->m_pWantCRCList = pWantCRCList;
		m_FileWhitelist.Init( pNewList );
	}

}


int CBaseFileSystem::GetUnverifiedFileHashes( CUnverifiedFileHash *pFiles, int nMaxFiles )
{
	return m_FileTracker2.GetUnverifiedFileHashes( pFiles, nMaxFiles );
}



int CBaseFileSystem::GetWhitelistSpewFlags()
{
	return m_WhitelistSpewFlags;
}


void CBaseFileSystem::SetWhitelistSpewFlags( int flags )
{
	m_WhitelistSpewFlags = flags;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pString - 
//			maxCharsIncludingTerminator - 
//			fileTime - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::FileTimeToString( char *pString, int maxCharsIncludingTerminator, long fileTime )
{
	if ( IsGameConsole() )
	{
		char szTemp[ 256 ];

		time_t time = fileTime;
		V_strncpy( szTemp, ctime( &time ), sizeof( szTemp ) );
		char *pFinalColon = Q_strrchr( szTemp, ':' );
		if ( pFinalColon )
			*pFinalColon = '\0';

		// Clip off the day of the week
		V_strncpy( pString, szTemp + 4, maxCharsIncludingTerminator );
	}
	else
	{
		time_t time = fileTime;
		V_strncpy( pString, ctime( &time ), maxCharsIncludingTerminator );

		// We see a linefeed at the end of these strings...if there is one, gobble it up
		int len = V_strlen( pString );
		if ( pString[ len - 1 ] == '\n' )
		{
			pString[ len - 1 ] = '\0';
		}

		pString[maxCharsIncludingTerminator-1] = '\0';
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FileExists( const char *pFileName, const char *pPathID )
{
	VPROF_BUDGET( "CBaseFileSystem::FileExists", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	NoteIO();

	CHECK_DOUBLE_SLASHES( pFileName );

	CSearchPathsIterator iter( this, &pFileName, pPathID );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		int size = FastFindFile( pSearchPath, pFileName );
		if ( size >= 0 )
		{
			return true;
		}
	}
	return false;
}

bool CBaseFileSystem::IsFileWritable( char const *pFileName, char const *pPathID /*=0*/ )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	struct	_stat buf;

	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pPathID, tempPathID );

	if ( Q_IsAbsolutePath( pFileName ) )
	{
		bool bFixed = false;
		char fixedFATXFilename[MAX_PATH];
		if ( IsX360() )
		{
			bFixed = FixupFATXFilename( pFileName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
		}

		if ( FS_stat( bFixed ? fixedFATXFilename : pFileName, &buf ) != -1 )
		{
#ifdef WIN32
			if ( buf.st_mode & _S_IWRITE )
#elif defined( _PS3 )
			if( buf.st_mode & S_IWUSR )
#elif POSIX
			if ( buf.st_mode & S_IWRITE )
#else
			if ( buf.st_mode & S_IWRITE )
#endif
			{
				return true;
			}
		}
		return false;
	}

	CSearchPathsIterator iter( this, &pFileName, pPathID, FILTER_CULLPACK );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		char tempFileName[ MAX_FILEPATH ];
		Q_snprintf( tempFileName, sizeof( tempFileName ), "%s%s", pSearchPath->GetPathString(), pFileName );
		Q_FixSlashes( tempFileName );

		bool bFixed = false;
		char fixedFATXFilename[MAX_PATH];
		if ( IsX360() )
		{
			bFixed = FixupFATXFilename( tempFileName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
		}

		if ( FS_stat( bFixed ? fixedFATXFilename : tempFileName, &buf ) != -1 )
		{
#ifdef WIN32
			if ( buf.st_mode & _S_IWRITE )
#elif defined( _PS3 )
			if( buf.st_mode & S_IWUSR )
#elif POSIX
			if ( buf.st_mode & S_IWRITE )
#else
			if ( buf.st_mode & S_IWRITE )
#endif
			{
				return true;
			}
		}
	}
	return false;
}


bool CBaseFileSystem::SetFileWritable( char const *pFileName, bool writable, const char *pPathID /*= 0*/ )
{
	CHECK_DOUBLE_SLASHES( pFileName );

#ifdef _WIN32
	int pmode = writable ? ( _S_IWRITE | _S_IREAD ) : ( _S_IREAD );
#elif defined( _PS3 )
	int pmode = writable ? ( S_IWUSR | S_IRUSR ) : ( S_IRUSR );
#else
	int pmode = writable ? ( S_IWRITE | S_IREAD ) : ( S_IREAD );
#endif

	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pPathID, tempPathID );

	if ( Q_IsAbsolutePath( pFileName ) )
	{
		bool bFixed = false;
		char fixedFATXFilename[MAX_PATH];
		if ( IsX360() )
		{
			bFixed = FixupFATXFilename( pFileName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
		}

		return ( FS_chmod( bFixed ? fixedFATXFilename : pFileName, pmode ) == 0 );
	}

	CSearchPathsIterator iter( this, &pFileName, pPathID, FILTER_CULLPACK );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		char tempFilename[ MAX_FILEPATH ];
		Q_snprintf( tempFilename, sizeof( tempFilename ), "%s%s", pSearchPath->GetPathString(), pFileName );
		Q_FixSlashes( tempFilename );

		bool bFixed = false;
		char fixedFATXFilename[MAX_PATH];
		if ( IsX360() )
		{
			bFixed = FixupFATXFilename( tempFilename, fixedFATXFilename, sizeof( fixedFATXFilename ) );
		}

		if ( FS_chmod( bFixed ? fixedFATXFilename : tempFilename, pmode ) == 0 )
		{
			return true;
		}
	}

	// Failure
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::IsDirectory( const char *pFileName, const char *pathID )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	// Allow for UNC-type syntax to specify the path ID.
	struct	_stat buf;

	char pTempBuf[MAX_PATH];
	Q_strncpy( pTempBuf, pFileName, sizeof(pTempBuf) );
	Q_StripTrailingSlash( pTempBuf );
	pFileName = pTempBuf;

	char tempPathID[MAX_PATH];
	ParsePathID( pFileName, pathID, tempPathID );
	if ( Q_IsAbsolutePath( pFileName ) )
	{
		bool bFixed = false;
		char fixedFATXFilename[MAX_PATH];
		if ( IsX360() )
		{
			bFixed = FixupFATXFilename( pFileName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
		}

		if ( FS_stat( bFixed ? fixedFATXFilename : pFileName, &buf ) != -1 )
		{
			if ( buf.st_mode & _S_IFDIR )
				return true;
		}
		return false;
	}

	CSearchPathsIterator iter( this, &pFileName, pathID, FILTER_CULLPACK );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		char tempFileName[ MAX_FILEPATH ];
		Q_snprintf( tempFileName, sizeof( tempFileName ), "%s%s", pSearchPath->GetPathString(), pFileName );
		Q_FixSlashes( tempFileName );

		bool bFixed = false;
		char fixedFATXFilename[MAX_PATH];
		if ( IsX360() )
		{
			bFixed = FixupFATXFilename( tempFileName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
		}

		if ( FS_stat( bFixed ? fixedFATXFilename : tempFileName, &buf ) != -1 )
		{
			if ( buf.st_mode & _S_IFDIR )
				return true;
		}
	}

#ifdef SUPPORT_VPK
	//
	// Let's see if the directory exists in the VPK file structure
	//
	if ( 0 == m_VPKDirectories.Count() )
	{
		// Populate the directory list
		CUtlStringList dirNames;
		CUtlStringList fileNames;

		for( int i = 0 ; i < m_VPKFiles.Count(); i++ )
		{
			m_VPKFiles[i]->GetFileAndDirLists( dirNames, fileNames, true );
		}

		FOR_EACH_VEC( dirNames, j )
		{
			m_VPKDirectories.Insert( dirNames[j], 0 );
		}
	}

	// If the dir isn't part of the VPK structure then game over
	char szPathWithCorrectSlashes[MAX_PATH];
	V_strncpy( szPathWithCorrectSlashes, pFileName, sizeof( szPathWithCorrectSlashes ) );
	V_FixSlashes( szPathWithCorrectSlashes, '/' );

	return ( -1 != m_VPKDirectories.Find( szPathWithCorrectSlashes ) );
#else
	return ( false );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::CreateDirHierarchy( const char *pRelativePath, const char *pathID )
{	
	CHECK_DOUBLE_SLASHES( pRelativePath );

	// Allow for UNC-type syntax to specify the path ID.
	char tempPathID[MAX_PATH];
 	ParsePathID( pRelativePath, pathID, tempPathID );

	char szScratchFileName[MAX_PATH];
	if ( !Q_IsAbsolutePath( pRelativePath ) )
	{
		Assert( pathID );
		ComputeFullWritePath( szScratchFileName, sizeof( szScratchFileName ), pRelativePath, pathID );
	}
	else
	{
		Q_strncpy( szScratchFileName, pRelativePath, sizeof(szScratchFileName) );
	}

	Q_FixSlashes( szScratchFileName );

	int len = strlen( szScratchFileName ) + 1;
	char *end = szScratchFileName + len;
	char *s = szScratchFileName;
	while( s < end )
    {
		if	(	PATHSEPARATOR( *s ) && 
				s != szScratchFileName && 
				( IsLinux() || IsPS3() || *( s - 1 ) != ':' ) 
			)
        {
			char save = *s;
			*s = '\0';
#if defined( _WIN32 )
			_mkdir( szScratchFileName );
#elif defined( _PS3 )
			CellFsStat status;
			//Only create is the path doesn't exist already - Jawad.
			if ( cellFsStat( szScratchFileName, &status ) != CELL_FS_SUCCEEDED )
				cellFsMkdir( szScratchFileName, CELL_FS_DEFAULT_CREATE_MODE_1 );
#elif defined( POSIX )
			mkdir( szScratchFileName, S_IRWXU |  S_IRGRP |  S_IROTH );// owner has rwx, rest have r
#endif
			*s = save;
        }
		s++;
    }

#if defined( _WIN32 )
	_mkdir( szScratchFileName );
#elif defined( _PS3 )
	CellFsStat status;
	if ( cellFsStat( szScratchFileName, &status ) != CELL_FS_SUCCEEDED )
		cellFsMkdir( szScratchFileName, CELL_FS_DEFAULT_CREATE_MODE_1 );
#elif defined( POSIX )
	mkdir( szScratchFileName, S_IRWXU |  S_IRGRP |  S_IROTH );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Given an absolute path, do a find first find next on it and build
// a list of files.  Physical file system only
//-----------------------------------------------------------------------------
void CBaseFileSystem::FindFileAbsoluteListHelper( CUtlVector< CUtlString > &outAbsolutePathNames, FindData_t &findData, const char *pAbsoluteFindName )
{
	// TODO: figure out what PS3 does without VPKs
#ifndef _PS3
	bool bFixed = false;
	char fixedFATXFilename[MAX_PATH];
	if ( IsX360() )
	{
		bFixed = FixupFATXFilename( pAbsoluteFindName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
	}

	char path[MAX_PATH];
	V_strncpy( path, pAbsoluteFindName, sizeof(path) );
	V_StripFilename( path );

	findData.findHandle = FS_FindFirstFile( bFixed ? fixedFATXFilename : pAbsoluteFindName, &findData.findData );

	while ( findData.findHandle != INVALID_HANDLE_VALUE )
	{
		char result[MAX_PATH];
		V_ComposeFileName( path, findData.findData.cFileName, result, sizeof(result) );

		outAbsolutePathNames.AddToTail( result );

		if ( !FS_FindNextFile( findData.findHandle, &findData.findData ) )
		{	
			FS_FindClose( findData.findHandle );
			findData.findHandle = INVALID_HANDLE_VALUE;
		}
	}
#else
	Error( "Not implemented!\n" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Searches for a file in all paths and results absolute path names 
// for the file, works in pack files (zip and vpk) too.  Lets you search for 
// something like sound/sound.cache and get a list of every sound cache
//-----------------------------------------------------------------------------
void CBaseFileSystem::FindFileAbsoluteList( CUtlVector< CUtlString > &outAbsolutePathNames, const char *pWildCard, const char *pPathID )
{
	// TODO: figure out what PS3 does without VPKs
#ifndef _PS3
	VPROF_BUDGET( "CBaseFileSystem::FindFileAbsoluteList", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	outAbsolutePathNames.Purge();

	FindData_t findData;
	if ( pPathID )
	{
		findData.m_FilterPathID = g_PathIDTable.AddString( pPathID );
	}
	int maxlen = strlen( pWildCard ) + 1;
	findData.wildCardString.AddMultipleToTail( maxlen );
	Q_strncpy( findData.wildCardString.Base(), pWildCard, maxlen );
	Q_FixSlashes( findData.wildCardString.Base() );
	findData.findHandle = INVALID_HANDLE_VALUE;

	if ( Q_IsAbsolutePath( pWildCard ) )
	{
		FindFileAbsoluteListHelper( outAbsolutePathNames, findData, pWildCard );
	}
	else
	{
		int c = m_SearchPaths.Count();
		for (	findData.currentSearchPathID = 0; 
			findData.currentSearchPathID < c; 
			findData.currentSearchPathID++ )
		{
			CSearchPath *pSearchPath = &m_SearchPaths[findData.currentSearchPathID];

			if ( pSearchPath->GetPackFile() ) // We're going to search pack files second
				continue;

			if ( FilterByPathID( pSearchPath, findData.m_FilterPathID ) )
				continue;

			// already visited this path
			if ( findData.m_VisitedSearchPaths.MarkVisit( *pSearchPath ) )
				continue;

			char tempFileName[ MAX_FILEPATH ];
			Q_snprintf( tempFileName, sizeof( tempFileName ), "%s%s", pSearchPath->GetPathString(), findData.wildCardString.Base() );
			Q_FixSlashes( tempFileName );

			FindFileAbsoluteListHelper( outAbsolutePathNames, findData, tempFileName );
		}
	}

	//TODO:  zips!

#if defined( SUPPORT_VPK )
	//
	// Now that we have searched the filesystem let's look in the VPK files
	//
	FOR_EACH_VEC( m_VPKFiles, i )
	{
		CUtlStringList dirMatchesFromVPK, fileMatchesFromVPK;
		m_VPKFiles[i]->GetFileAndDirLists( pWildCard, dirMatchesFromVPK, fileMatchesFromVPK, true );

		FOR_EACH_VEC( dirMatchesFromVPK, j )
		{
			char result[MAX_PATH];
			V_ComposeFileName( m_VPKFiles[i]->FullPathName(), dirMatchesFromVPK[j], result, sizeof(result) );
			outAbsolutePathNames.AddToTail( result );
		}

		FOR_EACH_VEC( fileMatchesFromVPK, j )
		{
			char result[MAX_PATH];
			V_ComposeFileName( m_VPKFiles[i]->FullPathName(), fileMatchesFromVPK[j], result, sizeof(result) );
			outAbsolutePathNames.AddToTail( result );
		}
	}
#endif
#else
Error( "Not implemented!\n" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pWildCard - 
//			*pHandle - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::FindFirstEx( const char *pWildCard, const char *pPathID, FileFindHandle_t *pHandle )
{
	CHECK_DOUBLE_SLASHES( pWildCard );

	return FindFirstHelper( pWildCard, pPathID, pHandle, NULL );
}


const char *CBaseFileSystem::FindFirstHelper( const char *pWildCard, const char *pPathID, FileFindHandle_t *pHandle, int *pFoundStoreID )
{
	VPROF_BUDGET( "CBaseFileSystem::FindFirst", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
 	Assert( pWildCard );
 	Assert( pHandle );

	FileFindHandle_t hTmpHandle = m_FindData.AddToTail();
	FindData_t *pFindData = &m_FindData[hTmpHandle];
	Assert( pFindData );
	if ( pPathID )
	{
		pFindData->m_FilterPathID = g_PathIDTable.AddString( pPathID );
	}
	int maxlen = strlen( pWildCard ) + 1;
	pFindData->wildCardString.AddMultipleToTail( maxlen );
	Q_strncpy( pFindData->wildCardString.Base(), pWildCard, maxlen );
	Q_FixSlashes( pFindData->wildCardString.Base() );
	pFindData->findHandle = INVALID_HANDLE_VALUE;

	if ( Q_IsAbsolutePath( pWildCard ) )
	{
		bool bFixed = false;
		char fixedFATXFilename[MAX_PATH];
		if ( IsX360() )
		{
			bFixed = FixupFATXFilename( pWildCard, fixedFATXFilename, sizeof( fixedFATXFilename ) );
		}

		pFindData->findHandle = FS_FindFirstFile( bFixed ? fixedFATXFilename : pWildCard, &pFindData->findData );
		pFindData->currentSearchPathID = -1;
	}
	else
	{
		int c = m_SearchPaths.Count();
		for (	pFindData->currentSearchPathID = 0; 
				pFindData->currentSearchPathID < c; 
				pFindData->currentSearchPathID++ )
		{
			CSearchPath *pSearchPath = &m_SearchPaths[pFindData->currentSearchPathID];

			// FIXME:  Should findfirst/next work with pak files?
			if ( pSearchPath->GetPackFile() )
				continue;

			if ( FilterByPathID( pSearchPath, pFindData->m_FilterPathID ) )
				continue;
			
			// already visited this path
			if ( pFindData->m_VisitedSearchPaths.MarkVisit( *pSearchPath ) )
				continue;

			char tempFileName[ MAX_FILEPATH ];
			Q_snprintf( tempFileName, sizeof( tempFileName ), "%s%s", pSearchPath->GetPathString(), pFindData->wildCardString.Base() );
			Q_FixSlashes( tempFileName );

			bool bFixed = false;
			char fixedFATXFilename[MAX_PATH];
			if ( IsX360() )
			{
				bFixed = FixupFATXFilename( tempFileName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
			}

			pFindData->findHandle = FS_FindFirstFile( bFixed ? fixedFATXFilename : tempFileName, &pFindData->findData );
			pFindData->m_CurrentStoreID = pSearchPath->m_storeId;

			if ( pFindData->findHandle != INVALID_HANDLE_VALUE )
				break;
		}
	}

#ifdef SUPPORT_VPK
	//
	// Now that we have searched the filesystem let's look in the VPK files
	//
	for( int i = 0 ; i < m_VPKFiles.Count(); i++ )
	{
		m_VPKFiles[i]->GetFileAndDirLists( pWildCard, pFindData->m_dirMatchesFromVPK, pFindData->m_fileMatchesFromVPK, true );
	}
#endif

	// We have a result from the filesystem 
	if( pFindData->findHandle != INVALID_HANDLE_VALUE )
	{
		// Remember that we visited this file already.
		pFindData->m_VisitedFiles.Insert( pFindData->findData.cFileName, 0 );

		if ( pFoundStoreID )
			*pFoundStoreID = pFindData->m_CurrentStoreID;

		*pHandle = hTmpHandle;
		return pFindData->findData.cFileName;
	}
#ifdef SUPPORT_VPK
	// We have a file result from the VPK file but not the filesystem
	else if ( pFindData->m_fileMatchesFromVPK.Count() > 0 )
	{
		// Remember that we visited this file already.
		pFindData->m_VisitedFiles.Insert( V_UnqualifiedFileName( pFindData->m_fileMatchesFromVPK[0] ), 0 );
		*pHandle = hTmpHandle;

		V_strncpy( pFindData->findData.cFileName, V_UnqualifiedFileName( pFindData->m_fileMatchesFromVPK[0] ), sizeof( pFindData->findData.cFileName ) );

		pFindData->findData.dwFileAttributes = 0;

		delete pFindData->m_fileMatchesFromVPK.Head();
		pFindData->m_fileMatchesFromVPK.RemoveMultipleFromHead( 1 );

		return pFindData->findData.cFileName;	
	}
	// We have a dir result from the VPK file but not the filesystem
	else if ( pFindData->m_dirMatchesFromVPK.Count() > 0 )
	{
		// Remember that we visited this file already.
		pFindData->m_VisitedFiles.Insert( V_UnqualifiedFileName( pFindData->m_dirMatchesFromVPK[0] ), 0 );
		*pHandle = hTmpHandle;

		V_strncpy( pFindData->findData.cFileName, V_UnqualifiedFileName( pFindData->m_dirMatchesFromVPK[0] ), sizeof( pFindData->findData.cFileName ) );

		pFindData->findData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

		delete pFindData->m_dirMatchesFromVPK.Head();
		pFindData->m_dirMatchesFromVPK.RemoveMultipleFromHead( 1 );

		return pFindData->findData.cFileName;	
	}
#endif
	// Handle failure here
	pFindData = 0;
	m_FindData.Remove(hTmpHandle);
	*pHandle = -1;

	return NULL;
}

const char *CBaseFileSystem::FindFirst( const char *pWildCard, FileFindHandle_t *pHandle )
{
	return FindFirstEx( pWildCard, NULL, pHandle );
}


// Get the next file, trucking through the path. . don't check for duplicates.
bool CBaseFileSystem::FindNextFileHelper( FindData_t *pFindData, int *pFoundStoreID )
{
	// PAK files???

	// Try the same search path that we were already searching on.
	if( FS_FindNextFile( pFindData->findHandle, &pFindData->findData ) )
	{
		if ( pFoundStoreID )
			*pFoundStoreID = pFindData->m_CurrentStoreID;

		return true;
	}

	// This happens when we searched a full path
	if ( pFindData->currentSearchPathID < 0 )
		return false;

	pFindData->currentSearchPathID++;

	if ( pFindData->findHandle != INVALID_HANDLE_VALUE )
	{
		FS_FindClose( pFindData->findHandle );
	}
	pFindData->findHandle = INVALID_HANDLE_VALUE;

	int c = m_SearchPaths.Count();
	for( ; pFindData->currentSearchPathID < c; ++pFindData->currentSearchPathID ) 
	{
		CSearchPath *pSearchPath = &m_SearchPaths[pFindData->currentSearchPathID];

		// FIXME: Should this work with PAK files?
		if ( pSearchPath->GetPackFile() )
			continue;

		if ( FilterByPathID( pSearchPath, pFindData->m_FilterPathID ) )
			continue;
		
		// already visited this path
		if ( pFindData->m_VisitedSearchPaths.MarkVisit( *pSearchPath ) )
			continue;

		char tempFileName[ MAX_FILEPATH ];
		Q_snprintf( tempFileName, sizeof( tempFileName ), "%s%s", pSearchPath->GetPathString(), pFindData->wildCardString.Base() );
		Q_FixSlashes( tempFileName );

		bool bFixed = false;
		char fixedFATXFilename[MAX_PATH];
		if ( IsX360() )
		{
			bFixed = FixupFATXFilename( tempFileName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
		}

		pFindData->findHandle = FS_FindFirstFile( bFixed ? fixedFATXFilename : tempFileName, &pFindData->findData );
		pFindData->m_CurrentStoreID = pSearchPath->m_storeId;
		if ( pFindData->findHandle != INVALID_HANDLE_VALUE )
		{
			if ( pFoundStoreID )
				*pFoundStoreID = pFindData->m_CurrentStoreID;

			return true;
		}
	}

#ifdef SUPPORT_VPK
	// Return the next one from the list of VPK matches if there is one
	if ( pFindData->m_fileMatchesFromVPK.Count() > 0 )
	{
		V_strncpy( pFindData->findData.cFileName, V_UnqualifiedFileName( pFindData->m_fileMatchesFromVPK[0] ), sizeof( pFindData->findData.cFileName ) );
		pFindData->findData.dwFileAttributes = 0;
		delete pFindData->m_fileMatchesFromVPK.Head();
		pFindData->m_fileMatchesFromVPK.RemoveMultipleFromHead( 1 );

		return true;
	}

	// Return the next one from the list of VPK matches if there is one
	if ( pFindData->m_dirMatchesFromVPK.Count() > 0 )
	{
		V_strncpy( pFindData->findData.cFileName, V_UnqualifiedFileName( pFindData->m_dirMatchesFromVPK[0] ), sizeof( pFindData->findData.cFileName ) );
		pFindData->findData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		delete pFindData->m_dirMatchesFromVPK.Head();
		pFindData->m_dirMatchesFromVPK.RemoveMultipleFromHead( 1 );

		return true;
	}
#endif

	return false;
}	

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::FindNext( FileFindHandle_t handle )
{
	VPROF_BUDGET( "CBaseFileSystem::FindNext", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );
	FindData_t *pFindData = &m_FindData[handle];

	while( 1 )
	{
		if( FindNextFileHelper( pFindData, NULL ) )
		{
			if ( pFindData->m_VisitedFiles.Find( pFindData->findData.cFileName ) == -1 )
			{
				pFindData->m_VisitedFiles.Insert( pFindData->findData.cFileName, 0 );
				return pFindData->findData.cFileName;
			}
		}
		else
		{
			return NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FindIsDirectory( FileFindHandle_t handle )
{
	FindData_t *pFindData = &m_FindData[handle];
	return !!( pFindData->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::FindClose( FileFindHandle_t handle )
{
	if ( ( handle < 0 ) || ( !m_FindData.IsInList( handle ) ) )
		return;

	FindData_t *pFindData = &m_FindData[handle];
	Assert(pFindData);

	if ( pFindData->findHandle != INVALID_HANDLE_VALUE)
	{
		FS_FindClose( pFindData->findHandle );
	}
	pFindData->findHandle = INVALID_HANDLE_VALUE;

	pFindData->wildCardString.Purge();
#ifdef SUPPORT_VPK
	pFindData->m_fileMatchesFromVPK.PurgeAndDeleteElements();
	pFindData->m_dirMatchesFromVPK.PurgeAndDeleteElements();
#endif
	m_FindData.Remove( handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::GetLocalCopy( const char *pFileName )
{
	// do nothing. . everything is local.
}

#ifdef SUPPORT_VPK
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
//-----------------------------------------------------------------------------
CPackedStoreFileHandle CBaseFileSystem::FindFileInVPKs( const char *pFileName )
{
	CPackedStoreFileHandle retVal;

	// Try to find the path in the mounted VPK files
	for( int i = 0 ; i < m_VPKFiles.Count(); i++ )
	{
		retVal = m_VPKFiles[i]->OpenFile( pFileName );
		if ( retVal )
		{
			break;
		}
	}

	return retVal;
}
#endif

//-----------------------------------------------------------------------------
// Converts a partial path into a full path
// Relative paths that are pack based are returned as an absolute path .../zip?.zip/foo
// A pack absolute path can be sent back in for opening, and the file will be properly
// detected as pack based and mounted inside the pack.
//-----------------------------------------------------------------------------
const char *CBaseFileSystem::RelativePathToFullPath( const char *pFileName, const char *pPathID, char *pFullPath, int fullPathBufferSize, PathTypeFilter_t pathFilter, PathTypeQuery_t *pPathType )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	struct	_stat buf;

	if ( pPathType )
	{
		*pPathType = PATH_IS_NORMAL;
	}

#ifdef _PS3
	// crush the filename to lowercase
	char lowercasedname[256];
	V_strncpy( lowercasedname, pFileName, 255 );
	V_strnlwr( lowercasedname, 255 );
	pFileName = lowercasedname;
#endif

	// Fill in the default in case it's not found...
	Q_strncpy( pFullPath, pFileName, fullPathBufferSize );

	if ( IsPC() && pathFilter == FILTER_NONE )
	{
		// X360TBD: PC legacy behavior never returned pack paths
		// do legacy behavior to ensure naive callers don't break
		pathFilter = FILTER_CULLPACK;
	}

	CSearchPathsIterator iter( this, &pFileName, pPathID, pathFilter );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		int		dummy;
		int64 dummy64;

		CPackFile *pPack = pSearchPath->GetPackFile();
		if ( pPack )
		{
			if ( pPack->FindFile( pFileName, dummy, dummy64, dummy ) )
			{
				if ( pPathType )
				{
					if ( pPack->m_bIsMapPath )
					{
						*pPathType |= PATH_IS_MAPPACKFILE;
					}
					else
					{
						*pPathType |= PATH_IS_PACKFILE;
					}
					if ( pSearchPath->m_bIsDvdDevPath )
					{
						*pPathType |= PATH_IS_DVDDEV;
					}
				}

				// form an encoded absolute path that can be decoded by our FS as pak based
				int len;
				V_strncpy( pFullPath, pPack->m_ZipName.String(), fullPathBufferSize );
				len = strlen( pFullPath );
				V_AppendSlash( pFullPath, fullPathBufferSize - len );
				len = strlen( pFullPath );				
				V_strncpy( pFullPath + len, pFileName, fullPathBufferSize - len ); 

				return pFullPath;
			}

			continue;
		}

		char tempFileName[ MAX_FILEPATH ];
		Q_snprintf( tempFileName, sizeof( tempFileName ), "%s%s", pSearchPath->GetPathString(), pFileName );
		Q_FixSlashes( tempFileName );

		bool bFixed = false;
		char fixedFATXFilename[MAX_PATH];
		if ( IsX360() )
		{
			// do not expose naming convolution to callers
			// callers don't expect the filename to change, only that the path get filled out
			bFixed = FixupFATXFilename( tempFileName, fixedFATXFilename, sizeof( fixedFATXFilename ) );
		}
		bool bFound = FS_stat( bFixed ? fixedFATXFilename : tempFileName, &buf ) != -1;
		if ( bFound )
		{
			Q_strncpy( pFullPath, tempFileName, fullPathBufferSize );
			if ( pPathType && pSearchPath->m_bIsDvdDevPath )
			{
				*pPathType |= PATH_IS_DVDDEV;
			}
			return pFullPath;
		}
	}

#ifdef SUPPORT_VPK
	// Try to find the path in the mounted VPK files
	if ( FindFileInVPKs( pFileName ) )
	{
		char pModPath[MAX_PATH];

		GetSearchPath( "MOD", false, pModPath, sizeof( pModPath ) );
		V_snprintf( pFullPath, fullPathBufferSize, "%s%s", pModPath, pFileName );

		return pFullPath;
	}
#endif

	// not found
	return NULL;
}

#if IsGameConsole()
bool CBaseFileSystem::GetPackFileInfoFromRelativePath( const char *pFileName, const char *pPathID, char *pPackPath, int nPackPathBufferSize, int64 &nPosition, int64 &nLength )
{
	CHECK_DOUBLE_SLASHES( pFileName );

#ifdef _PS3
	// crush the filename to lowercase
	char lowercasedname[256];
	V_strncpy( lowercasedname, pFileName, 255 );
	V_strnlwr( lowercasedname, 255 );
	pFileName = lowercasedname;
#endif

	CSearchPathsIterator iter( this, &pFileName, pPathID, FILTER_CULLNONPACK );
	for ( CSearchPath *pSearchPath = iter.GetFirst(); pSearchPath != NULL; pSearchPath = iter.GetNext() )
	{
		int	nIndex;
		int nLength32;

		CPackFile *pPack = pSearchPath->GetPackFile();
		if ( pPack )
		{
			if ( pPack->FindFile( pFileName, nIndex, nPosition, nLength32 ) )
			{
				// Support all types as long as it is in a pack file.
				// Note that if it is in dvddev, prefetching will not really help (and may not be initialized correctly anyway).
				V_strncpy( pPackPath, pPack->m_ZipName.String(), nPackPathBufferSize );
				nLength = nLength32;
				return true;
			}
		}
	}

	// not found
	pPackPath[0] = '\0';
	nPosition = -1;
	nLength = -1;
	return false;
}
#endif

const char *CBaseFileSystem::GetLocalPath( const char *pFileName, char *pLocalPath, int localPathBufferSize )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	return RelativePathToFullPath( pFileName, NULL, pLocalPath, localPathBufferSize );
}


//-----------------------------------------------------------------------------
// Returns true on success, otherwise false if it can't be resolved
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FullPathToRelativePathEx( const char *pFullPath, const char *pPathId, char *pRelative, int nMaxLen )
{
	CHECK_DOUBLE_SLASHES( pFullPath );

	int nInlen = Q_strlen( pFullPath );
	if ( nInlen <= 0 )
	{
		pRelative[ 0 ] = 0;
		return false;
	}

	Q_strncpy( pRelative, pFullPath, nMaxLen );

	char pInPath[ MAX_FILEPATH ];
	Q_strncpy( pInPath, pFullPath, sizeof( pInPath ) );
#ifdef _WIN32
	Q_strlower( pInPath );
#endif
	Q_FixSlashes( pInPath );

	CUtlSymbol lookup;
	if ( pPathId )
	{
		lookup = g_PathIDTable.AddString( pPathId );
	}

	int c = m_SearchPaths.Count();
	for( int i = 0; i < c; i++ )
	{
		// FIXME: Should this work with embedded pak files?
		if ( m_SearchPaths[i].GetPackFile() && m_SearchPaths[i].GetPackFile()->m_bIsMapPath )
			continue;

		// Skip paths that are not on the specified search path
		if ( FilterByPathID( &m_SearchPaths[i], lookup ) )
			continue;

		char pSearchBase[ MAX_FILEPATH ];
		Q_strncpy( pSearchBase, m_SearchPaths[i].GetPathString(), sizeof( pSearchBase ) );
#ifdef _WIN32
		Q_strlower( pSearchBase );
#endif
		Q_FixSlashes( pSearchBase );
		int nSearchLen = Q_strlen( pSearchBase );
		if ( Q_strnicmp( pSearchBase, pInPath, nSearchLen ) )
			continue;

		Q_strncpy( pRelative, &pInPath[ nSearchLen ], nMaxLen );
		return true;
	}

	return false;
}

	
//-----------------------------------------------------------------------------
// Obsolete version
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FullPathToRelativePath( const char *pFullPath, char *pRelative, int nMaxLen )
{
	return FullPathToRelativePathEx( pFullPath, NULL, pRelative, nMaxLen );
}


//-----------------------------------------------------------------------------
// Deletes a file
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveFile( char const* pRelativePath, const char *pathID )
{
	CHECK_DOUBLE_SLASHES( pRelativePath );

	// Allow for UNC-type syntax to specify the path ID.
	char tempPathID[MAX_PATH];
	ParsePathID( pRelativePath, pathID, tempPathID );

	Assert( pathID || !IsGameConsole() );

	// Opening for write or append uses Write Path
	char szScratchFileName[MAX_PATH];
	if ( Q_IsAbsolutePath( pRelativePath ) )
	{
		Q_strncpy( szScratchFileName, pRelativePath, sizeof( szScratchFileName ) );
	}
	else
	{
		ComputeFullWritePath( szScratchFileName, sizeof( szScratchFileName ), pRelativePath, pathID );
	}
	int fail = unlink( szScratchFileName );
	if ( fail != 0 )
	{
		FileSystemWarning( FILESYSTEM_WARNING, "Unable to remove %s! (errno %x)\n", szScratchFileName, errno );
	}
}


//-----------------------------------------------------------------------------
// Renames a file
//-----------------------------------------------------------------------------
bool CBaseFileSystem::RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID )
{
	Assert( pOldPath && pNewPath );

	CHECK_DOUBLE_SLASHES( pOldPath );
	CHECK_DOUBLE_SLASHES( pNewPath );

	// Allow for UNC-type syntax to specify the path ID.
	char pPathIdCopy[MAX_PATH];
	const char *pOldPathId = pathID;
	if ( pathID )
	{
		Q_strncpy( pPathIdCopy, pathID, sizeof( pPathIdCopy ) );
		pOldPathId = pPathIdCopy;
	}

	char tempOldPathID[MAX_PATH];
	ParsePathID( pOldPath, pOldPathId, tempOldPathID );
	Assert( pOldPathId );

	// Allow for UNC-type syntax to specify the path ID.
	char tempNewPathID[MAX_PATH];
	ParsePathID( pNewPath, pathID, tempNewPathID );
	Assert( pathID );

	char pNewFileName[ MAX_PATH ];
	char szScratchFileName[MAX_PATH];

	// The source file may be in a fallback directory, so just resolve the actual path, don't assume pathid...
	RelativePathToFullPath( pOldPath, pOldPathId, szScratchFileName, sizeof( szScratchFileName ) );

	// Figure out the dest path
	if ( !Q_IsAbsolutePath( pNewPath ) )
	{
		ComputeFullWritePath( pNewFileName, sizeof( pNewFileName ), pNewPath, pathID );
	}
	else
	{
		Q_strncpy( pNewFileName, pNewPath, sizeof(pNewFileName) );
	}

	// Make sure the directory exitsts, too
	char pPathOnly[ MAX_PATH ];
	Q_strncpy( pPathOnly, pNewFileName, sizeof( pPathOnly ) );
	Q_StripFilename( pPathOnly );
	CreateDirHierarchy( pPathOnly, pathID );

	// Now copy the file over
	int fail = rename( szScratchFileName, pNewFileName );
	if (fail != 0)
	{
		FileSystemWarning( FILESYSTEM_WARNING, "Unable to rename %s to %s!\n", szScratchFileName, pNewFileName );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **ppdir - 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::GetCurrentDirectory( char* pDirectory, int maxlen )
{
#if defined( _WIN32 ) && !defined( _X360 )
	if ( !::GetCurrentDirectoryA( maxlen, pDirectory ) )
#elif ( defined( POSIX ) && !defined( _PS3 ) ) || defined( _X360 )
	if ( !getcwd( pDirectory, maxlen ) )
#endif
		return false;

	Q_FixSlashes(pDirectory);

	// Strip the last slash
	int len = strlen(pDirectory);
	if ( pDirectory[ len-1 ] == CORRECT_PATH_SEPARATOR )
	{
		pDirectory[ len-1 ] = 0;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pfnWarning - warning function callback
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetWarningFunc( void (*pfnWarning)( const char *fmt, ... ) )
{
	m_pfnWarning = pfnWarning;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : level - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetWarningLevel( FileWarningLevel_t level )
{
	m_fwLevel = level;
}

const FileSystemStatistics *CBaseFileSystem::GetFilesystemStatistics()
{
	return &m_Stats;
}

//-----------------------------------------------------------------------------
// Purpose: Get VPK file IO stats for OGS reporting
//-----------------------------------------------------------------------------
void CBaseFileSystem::GetVPKFileStatisticsKV( KeyValues *pKV )
{
	if ( pKV && !V_strcmp( pKV->GetName(), "ForceVpkOnlyExtensions" ) )
	{
		AUTO_LOCK( m_OpenedFilesMutex );	// this code will force VPK only extensions
		FOR_EACH_SUBKEY( pKV, kvExt )
		{
			m_NonexistingFilesExtensions[ kvExt->GetName() ] = kvExt->GetBool();
		}
		return;
	}

	for( int i = 0; i < m_VPKFiles.Count(); i++ )
	{
		m_VPKFiles[i]->GetPackFileLoadErrorSummaryKV( pKV );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : level - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::FileSystemWarning( FileWarningLevel_t level, const char *fmt, ... )
{
#ifdef _CERT
	return;
#endif

	if ( level > m_fwLevel )
		return;

	if ( ( fs_warning_mode.GetInt() == 1 && !ThreadInMainThread() ) || ( fs_warning_mode.GetInt() == 2 && ThreadInMainThread() ) )
		return;

	va_list argptr; 
    char warningtext[ 4096 ];
    
    va_start( argptr, fmt );
    Q_vsnprintf( warningtext, sizeof( warningtext ), fmt, argptr );
    va_end( argptr );

	// Dump to stdio
	printf( "%s", warningtext );
	if ( m_pfnWarning )
	{
		(*m_pfnWarning)( warningtext );
	}
	else
	{
#ifdef _WIN32
		Plat_DebugString( warningtext );
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::COpenedFile( void )
{
	m_pFile = NULL;
	m_pName = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::~COpenedFile( void )
{
	delete[] m_pName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::COpenedFile( const COpenedFile& src )
{
	m_pFile = src.m_pFile;
	if ( src.m_pName )
	{
		int len = strlen( src.m_pName ) + 1;
		m_pName = new char[ len ];
		Q_strncpy( m_pName, src.m_pName, len );
	}
	else
	{
		m_pName = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::COpenedFile::operator==( const CBaseFileSystem::COpenedFile& src ) const
{
	return src.m_pFile == m_pFile;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::COpenedFile::SetName( char const *name )
{
	delete[] m_pName;
	int len = strlen( name ) + 1;
	m_pName = new char[ len ];
	Q_strncpy( m_pName, name, len );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char
//-----------------------------------------------------------------------------
char const *CBaseFileSystem::COpenedFile::GetName( void )
{
	return m_pName ? m_pName : "???";
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath::CSearchPath( void )
{
	m_Path = g_PathIDTable.AddString( "" );
	m_pDebugPath = "";

	m_storeId = 0;
	m_pPackFile = NULL;
	m_pPathIDInfo = NULL;
	m_bIsDvdDevPath = false;
	m_bIsLocalizedPath = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath::~CSearchPath( void )
{
	if ( m_pPackFile )
	{	
		m_pPackFile->Release();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath *CBaseFileSystem::CSearchPathsIterator::GetFirst()
{
	if ( m_SearchPaths.Count() )
	{
		m_visits.Reset();
		m_iCurrent = -1;
		m_bExcluded = false;
		return GetNext();
	}
	return &m_EmptySearchPath;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath *CBaseFileSystem::CSearchPathsIterator::GetNext()
{
	CSearchPath *pSearchPath = NULL;

	// PURPOSELY!!! split the 360 dvddev logic from the nominal (shipping) case
	// the logic is permuted slightly to do the right kind of filtering in dvddev or strict mode
	// 360 can optionally ignore and exclude a local search path in dvddev mode
	// excluding a local search path falls through to its cloned dvddev cache path
	// map paths are exempt from this exclusion logic
	if ( IsGameConsole() && ( m_DVDMode == DVDMODE_DEV || m_DVDMode == DVDMODE_DEV_VISTA ) && m_Filename[0] )
	{
		for ( m_iCurrent++; m_iCurrent < m_SearchPaths.Count(); m_iCurrent++ )
		{
			pSearchPath = &m_SearchPaths[m_iCurrent];

			if ( m_PathTypeFilter == FILTER_CULLPACK && pSearchPath->GetPackFile() )
				continue;

			if ( ( m_PathTypeFilter == FILTER_CULLLOCALIZED || m_PathTypeFilter == FILTER_CULLLOCALIZED_ANY ) && pSearchPath->m_bIsLocalizedPath )
			{
				continue;
			}

			if ( pSearchPath->m_bIsDvdDevPath && !m_bExcluded )
			{
				// the dvddev cache path is ignored until an exclusion has been matched
				continue;
			}
			else if ( !pSearchPath->m_bIsDvdDevPath && m_bExcluded )
			{
				// an excluded file falls all the way through and can only be in the dvddev cache
				continue;
			}

			if ( CBaseFileSystem::FilterByPathID( pSearchPath, m_pathID ) )
				continue;

			// prevent a duplicate costly check for exclusion
			if ( m_visits.MarkVisit( *pSearchPath ) )
				continue;

			// a path gets ignored/skipped if it matches a dvddev exclusion, thus falling through to the dvddev cache path
			bool bIgnorePath = false;
			if ( !pSearchPath->m_bIsDvdDevPath )
			{
				if ( !pSearchPath->GetPackFile() || !pSearchPath->GetPackFile()->m_bIsMapPath )
				{
					char szExcludeFile[MAX_PATH];
					char szFilename[MAX_PATH];
					V_ComposeFileName( pSearchPath->GetPathString(), m_Filename, szFilename, sizeof( szFilename ) );
					for ( int i = 0; i < m_ExcludeFilePaths.Count(); i++ )
					{
						if ( g_pFullFileSystem->String( m_ExcludeFilePaths[i], szExcludeFile, sizeof( szExcludeFile ) ) )
						{
							if ( !V_stricmp( szFilename, szExcludeFile ) )
							{
								// the file was excluded and should not be fetched from this search path
								// the fallthrough will hit the dvddev cache
								bIgnorePath = true;
								m_bExcluded = true;
								break;
							}
						}
					}

					// only an exclusion match is allowed to inhibit the filtering on non-zip paths, i.e. loose files
					// ignoring loose files is critical to preventing pointless slow presence i/o, i.e. 99% files are expected to be in a zip
					if ( !pSearchPath->GetPackFile() && m_PathTypeFilter == FILTER_CULLNONPACK && !bIgnorePath )
					{
						continue;
					}
				}
			}
			if ( !bIgnorePath )
			{
				break;
			}
		}
	}
	else
	{
		// nominal behavior
		for ( m_iCurrent++; m_iCurrent < m_SearchPaths.Count(); m_iCurrent++ )
		{
			pSearchPath = &m_SearchPaths[m_iCurrent];

			if ( m_PathTypeFilter == FILTER_CULLPACK && pSearchPath->GetPackFile() )
				continue;

			if ( ( m_PathTypeFilter == FILTER_CULLNONPACK || m_PathTypeFilter == FILTER_CULLLOCALIZED ) && !pSearchPath->GetPackFile() )
				continue;

			if ( ( m_PathTypeFilter == FILTER_CULLLOCALIZED || m_PathTypeFilter == FILTER_CULLLOCALIZED_ANY ) && pSearchPath->m_bIsLocalizedPath )
			{
				continue;
			}

			if ( CBaseFileSystem::FilterByPathID( pSearchPath, m_pathID ) )
				continue;

			if ( !m_visits.MarkVisit( *pSearchPath ) )
				break;
		}
	}

	if ( m_iCurrent < m_SearchPaths.Count() )
	{
		return pSearchPath;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Load/unload a DLL
//-----------------------------------------------------------------------------
CSysModule *CBaseFileSystem::LoadModule( const char *pFileName, const char *pPathID, bool bValidatedDllOnly )
{
	CHECK_DOUBLE_SLASHES( pFileName );

	bool bPathIsGameBin = false; bPathIsGameBin; // Touch the var for !Win64 build compiler warnings.
	LogFileAccess( pFileName );
	if ( !pPathID )
	{
		pPathID = "EXECUTABLE_PATH"; // default to the bin dir
	}
	else if ( IsPlatformWindowsPC64() )
	{
		bPathIsGameBin = V_strcmp( "GAMEBIN", pPathID ) == 0;
	}

#if defined(POSIX) && defined(PLATFORM_64BITS)
	bPathIsGameBin = V_strcmp( "GAMEBIN", pPathID ) == 0;
#endif

	char tempPathID[ MAX_PATH ];
	ParsePathID( pFileName, pPathID, tempPathID );
	
	CUtlSymbol lookup = g_PathIDTable.AddString( pPathID );

	// a pathID has been specified, find the first match in the path list
	int c = m_SearchPaths.Count();
	for ( int i = 0; i < c; i++ )
	{
		// pak files don't have modules
		if ( m_SearchPaths[i].GetPackFile() )
			continue;

		if ( FilterByPathID( &m_SearchPaths[i], lookup ) )
			continue;

		Q_snprintf( tempPathID, sizeof(tempPathID), "%s%s", m_SearchPaths[i].GetPathString(), pFileName ); // append the path to this dir.
		CSysModule *pModule = Sys_LoadModule( tempPathID );
		if ( pModule ) 
		{
			// we found the binary in one of our search paths
			return pModule;
		}
		else if ( IsPlatformWindowsPC64() && bPathIsGameBin )
		{
			Q_snprintf( tempPathID, sizeof( tempPathID ), "%s%s%s%s", m_SearchPaths[ i ].GetPathString(), "x64", CORRECT_PATH_SEPARATOR_S, pFileName ); // append the path to this dir.
			pModule = Sys_LoadModule( tempPathID );
			if ( pModule )
			{
				// we found the binary in a 64-bit location.
				return pModule;
			}
		}
#if defined(POSIX) && defined(PLATFORM_64BITS)
		else if ( bPathIsGameBin )
		{
#if defined(LINUX)
			const char* plat_dir = "linux64";
#else
			const char* plat_dir = "osx64";
#endif
			Q_snprintf( tempPathID, sizeof( tempPathID ), "%s%s%s%s", m_SearchPaths[ i ].GetPathString(), plat_dir, CORRECT_PATH_SEPARATOR_S, pFileName ); // append the path to this dir.
			pModule = Sys_LoadModule( tempPathID );
			if ( pModule )
			{
				// we found the binary in a 64-bit location.
				return pModule;
			}
		}
#endif
	}

	// couldn't load it from any of the search paths, let LoadLibrary try
	return Sys_LoadModule( pFileName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::UnloadModule( CSysModule *pModule )
{
	Sys_UnloadModule( pModule );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a filesystem logging function
//-----------------------------------------------------------------------------
void CBaseFileSystem::AddLoggingFunc( FileSystemLoggingFunc_t logFunc )
{
	Assert(!m_LogFuncs.IsValidIndex(m_LogFuncs.Find(logFunc)));
	m_LogFuncs.AddToTail(logFunc);
}

//-----------------------------------------------------------------------------
// Purpose: Removes a filesystem logging function
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveLoggingFunc( FileSystemLoggingFunc_t logFunc )
{
	m_LogFuncs.FindAndRemove(logFunc);
}

//-----------------------------------------------------------------------------
// Make sure that slashes are of the right kind and that there is a slash at the 
// end of the filename.
// WARNING!!: assumes that you have an extra byte allocated in the case that you need
// a slash at the end.
//-----------------------------------------------------------------------------
static void AddSeperatorAndFixPath( char *str )
{
	char *lastChar = &str[strlen( str ) - 1];
	if( *lastChar != CORRECT_PATH_SEPARATOR && *lastChar != INCORRECT_PATH_SEPARATOR )
	{
		lastChar[1] = CORRECT_PATH_SEPARATOR;
		lastChar[2] = '\0';
	}
	Q_FixSlashes( str );

	if ( IsGameConsole() )
	{
		// 360 FS won't resolve any path with ../
		V_RemoveDotSlashes( str );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
// Output : FileNameHandle_t
//-----------------------------------------------------------------------------
FileNameHandle_t CBaseFileSystem::FindOrAddFileName( char const *pFileName )
{
	return m_FileNames.FindOrAddFileName( pFileName );
}

FileNameHandle_t CBaseFileSystem::FindFileName( char const *pFileName )
{
	return m_FileNames.FindFileName( pFileName );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
// Output : char const
//-----------------------------------------------------------------------------
bool CBaseFileSystem::String( const FileNameHandle_t& handle, char *buf, int buflen )
{
	return m_FileNames.String( handle, buf, buflen );
}

int	CBaseFileSystem::GetPathIndex( const FileNameHandle_t &handle )
{
	return m_FileNames.PathIndex(handle);
}

CBaseFileSystem::CPathIDInfo* CBaseFileSystem::FindOrAddPathIDInfo( const CUtlSymbol &id, int bByRequestOnly )
{
	for ( int i=0; i < m_PathIDInfos.Count(); i++ )
	{
		CBaseFileSystem::CPathIDInfo *pInfo = m_PathIDInfos[i];
		if ( pInfo->GetPathID() == id )
		{
			if ( bByRequestOnly != -1 )
			{
				pInfo->m_bByRequestOnly = (bByRequestOnly != 0);
			}
			return pInfo;
		}
	}

	// Add a new one.
	CBaseFileSystem::CPathIDInfo *pInfo = new CBaseFileSystem::CPathIDInfo;
	m_PathIDInfos.AddToTail( pInfo );
	pInfo->SetPathID( id );
	pInfo->m_bByRequestOnly = (bByRequestOnly == 1);
	return pInfo;
}
		

void CBaseFileSystem::MarkPathIDByRequestOnly( const char *pPathID, bool bRequestOnly )
{
	FindOrAddPathIDInfo( g_PathIDTable.AddString( pPathID ), bRequestOnly );
}

bool CBaseFileSystem::IsFileInReadOnlySearchPath(const char *pPath, const char *pathID)
{
	//TODO: implementme!
	return false;
}

#if defined( TRACK_BLOCKING_IO )

void CBaseFileSystem::EnableBlockingFileAccessTracking( bool state )
{
	m_bBlockingFileAccessReportingEnabled = state;
}

bool CBaseFileSystem::IsBlockingFileAccessEnabled() const
{
	return m_bBlockingFileAccessReportingEnabled;
}

IBlockingFileItemList *CBaseFileSystem::RetrieveBlockingFileAccessInfo()
{
	Assert( m_pBlockingItems );
	return m_pBlockingItems;
}

void CBaseFileSystem::RecordBlockingFileAccess( bool synchronous, const FileBlockingItem& item )
{
	AUTO_LOCK( m_BlockingFileMutex );

	// Not tracking anything
	if ( !m_bBlockingFileAccessReportingEnabled )
		return;

	if ( synchronous && !m_bAllowSynchronousLogging && ( item.m_ItemType == FILESYSTEM_BLOCKING_SYNCHRONOUS ) )
		return;

	// Track it
	m_pBlockingItems->Add( item );
}

bool CBaseFileSystem::SetAllowSynchronousLogging( bool state )
{
	bool oldState = m_bAllowSynchronousLogging;
	m_bAllowSynchronousLogging = state;
	return oldState;
}

void CBaseFileSystem::BlockingFileAccess_EnterCriticalSection()
{
	m_BlockingFileMutex.Lock();
}

void CBaseFileSystem::BlockingFileAccess_LeaveCriticalSection()
{
	m_BlockingFileMutex.Unlock();
}

#endif // TRACK_BLOCKING_IO

bool CBaseFileSystem::GetFileTypeForFullPath( char const *pFullPath, wchar_t *buf, size_t bufSizeInBytes )
{
#if !defined( _X360 ) && !defined( POSIX )
	wchar_t wcharpath[512];
	::MultiByteToWideChar( CP_UTF8, 0, pFullPath, -1, wcharpath, sizeof( wcharpath ) / sizeof(wchar_t) );
	wcharpath[(sizeof( wcharpath ) / sizeof(wchar_t)) - 1] = L'\0';

	SHFILEINFOW info = { 0 };
	DWORD_PTR dwResult = SHGetFileInfoW( 
		wcharpath,
		0,
		&info,
		sizeof( info ),
		SHGFI_TYPENAME 
	);
	if ( dwResult )
	{
		wcsncpy( buf, info.szTypeName, ( bufSizeInBytes / sizeof( wchar_t  ) ) );
		buf[( bufSizeInBytes / sizeof( wchar_t ) ) - 1] = L'\0';
		return true;
	}
	else
#endif
	{
		char ext[32];
		Q_ExtractFileExtension( pFullPath, ext, sizeof( ext ) );
#ifdef POSIX		
		V_snwprintf( buf, ( bufSizeInBytes / sizeof( wchar_t ) ) - 1, L"%s File", V_strupr( ext ) ); // Matches what Windows does
#else
		V_snwprintf( buf, ( bufSizeInBytes / sizeof( wchar_t ) ) - 1, L".%S", ext );
#endif
		buf[( bufSizeInBytes / sizeof( wchar_t ) ) - 1] = L'\0';
	}
	return false;
}


bool CBaseFileSystem::GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign )
{
	if ( pOffsetAlign )
		*pOffsetAlign = 1;
	if ( pSizeAlign )
		*pSizeAlign = 1;
	if ( pBufferAlign )
		*pBufferAlign = 1;
	return false;
}

//-----------------------------------------------------------------------------
// This is a DVDDEV misery that needs to convolve loose filenames that exceed the 42 character limit.
// This is not for files inside zip files or in any other context.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FixupFATXFilename( const char *pFilename, char *pOutFilename, int nOutSize )
{
	if ( !IsX360() || m_DVDMode != DVDMODE_DEV )
	{
		// xbox dvdev mode only
		return false;
	}

	// back up to isolate the filename
	const char *pFilenameStart = V_UnqualifiedFileName( pFilename );
	int nFilenameLen = strlen( pFilenameStart );
	if ( nFilenameLen <= 42 )
	{
		// already compliant
		return false;
	}

	// only files within the dvddev cache or shadercache would be convolved (via vxconsole)
	if ( !V_stristr( pFilename, "dvddev" ) )
	{
		// not in any of the vxconsole caches, ignore
		return false;
	}

	// deterministically generate a unique suffix
	// this is the same operation that vxconsole does before pushing the files
	// good enough to ensure similar excessive filenames in the same dir won't collide
	char szFixedFilename[MAX_PATH];
	V_strncpy( szFixedFilename, pFilenameStart, sizeof( szFixedFilename ) );
	strlwr( szFixedFilename );
	CRC32_t	crc32 = CRC32_ProcessSingleBuffer( szFixedFilename, nFilenameLen );
	crc32 %= 10000;
	char szUnique[32];
	sprintf( szUnique, "~%u",  ( uint ) crc32 );
	szFixedFilename[42 - strlen( szUnique )] = '\0';
	V_strncat( szFixedFilename, szUnique, sizeof( szFixedFilename ) );

	if ( pFilenameStart != pFilename )
	{
		// get the initial path (and the slash)
		V_strncpy( pOutFilename, pFilename, MIN( nOutSize, pFilenameStart - pFilename + 1 ) );
	}
	// tack on the fixed filename
	V_strncat( pOutFilename, szFixedFilename, nOutSize );

	return true;
}

bool CBaseFileSystem::GetStringFromKVPool( CRC32_t poolKey, unsigned int key, char *pOutBuff, int buflen )
{
	// xbox only
	if ( !IsGameConsole() )
	{
		Assert( 0 );
		return false;
	}

	AUTO_LOCK( m_SearchPathsMutex );

	int c = m_SearchPaths.Count();
	for ( int i = 0; i < c; i++ )
	{
		CSearchPath *pSearchPath = &m_SearchPaths[i];

		CPackFile *pPackFile = pSearchPath->GetPackFile();
		if ( pPackFile && !pPackFile->m_bIsMapPath )
		{
			if ( poolKey == pPackFile->GetKVPoolKey() )
			{
				return pPackFile->GetStringFromKVPool( key, pOutBuff, buflen );
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructs a file handle
// Input  : base file system handle
// Output : 
//-----------------------------------------------------------------------------
CFileHandle::CFileHandle( CBaseFileSystem* fs )
{
	Init( fs );
}

CFileHandle::~CFileHandle()
{
	Assert( IsValid() );
	delete[] m_pszTrueFileName;

	if ( m_pPackFileHandle )
	{
		delete m_pPackFileHandle;
		m_pPackFileHandle = NULL;
	}

	if ( m_pFile )
	{
		m_fs->Trace_FClose( m_pFile );
		m_pFile = NULL;
	}

	m_nMagic = FREE_MAGIC;
}

void CFileHandle::Init( CBaseFileSystem *fs )
{
	m_nMagic = MAGIC;
	m_pFile = NULL;
	m_nLength = 0;
	m_type = FT_NORMAL;		
	m_pPackFileHandle = NULL;

	m_fs = fs;

	m_pszTrueFileName = 0;
}

bool CFileHandle::IsValid()
{
	return ( m_nMagic == MAGIC );
}

int CFileHandle::GetSectorSize()
{
	Assert( IsValid() );

	if ( m_pFile )
	{
		return m_fs->FS_GetSectorSize( m_pFile );
	}
	else if ( m_pPackFileHandle )
	{
		return m_pPackFileHandle->GetSectorSize();
	}
	else
	{
		return -1;
	}
}

bool CFileHandle::IsOK()
{
#ifdef SUPPORT_VPK
	if ( m_VPKHandle )
		return true;
#endif
	if ( m_pFile )
	{
		return ( IsValid() && m_fs->FS_ferror( m_pFile ) == 0 );
	}
	else if ( m_pPackFileHandle )
	{
		return IsValid();
	}

	m_fs->FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to IsOk NULL file pointer inside valid file handle!\n" );
	return false;
}

void CFileHandle::Flush()
{
	Assert( IsValid() );

	if ( m_pFile )
	{
		m_fs->FS_fflush( m_pFile );
	}
}

void CFileHandle::SetBufferSize( int nBytes )
{
	Assert( IsValid() );

	if ( m_pFile )
	{
		m_fs->FS_setbufsize( m_pFile, nBytes );
	}
	else if ( m_pPackFileHandle )
	{
		m_pPackFileHandle->SetBufferSize( nBytes );
	}
}

int CFileHandle::Read( void* pBuffer, int nLength )
{
	Assert( IsValid() );
	return Read( pBuffer, -1, nLength );
}

int CFileHandle::Read( void* pBuffer, int nDestSize, int nLength )
{
	Assert( IsValid() );

#ifdef SUPPORT_VPK
	if ( m_VPKHandle )
	{
		if ( nDestSize >= 0 )
			nLength = MIN( nLength, nDestSize );
		int nLengthRead = m_VPKHandle.Read( pBuffer, nLength );
		m_fs->m_FileTracker2.NotePackFileRead( m_VPKHandle, pBuffer, nLength );
		return nLengthRead;
	}
#endif

	// Is this a regular file or a pack file?  
	if ( m_pFile )
	{
		return m_fs->FS_fread( pBuffer, nDestSize, nLength, m_pFile );
	}
	else if ( m_pPackFileHandle )
	{
		// Pack file handle handles clamping all the reads:
		return m_pPackFileHandle->Read( pBuffer, nDestSize, nLength );
	}

	return 0;
}

int CFileHandle::Write( const void* pBuffer, int nLength )
{
	Assert( IsValid() );

	if ( !m_pFile )
	{
		m_fs->FileSystemWarning( FILESYSTEM_WARNING, "FS:  Tried to Write NULL file pointer inside valid file handle!\n" );
		return 0;
	}

	size_t nBytesWritten = m_fs->FS_fwrite( (void*)pBuffer, nLength, m_pFile  );

	m_fs->Trace_FWrite(nBytesWritten,m_pFile);

	return nBytesWritten;
}

int CFileHandle::Seek( int64 nOffset, int nWhence )
{
	Assert( IsValid() );

#ifdef SUPPORT_VPK
	if ( m_VPKHandle )
	{
		return m_VPKHandle.Seek( nOffset, nWhence );
	}
#endif
	if ( m_pFile )
	{
		m_fs->FS_fseek( m_pFile, nOffset, nWhence );
		// TODO - FS_fseek should return the resultant offset
		return 0;
	}
	else if ( m_pPackFileHandle )
	{
		return m_pPackFileHandle->Seek( (int)nOffset, nWhence );
	}

	return -1;
}

int CFileHandle::Tell()
{
	Assert( IsValid() );

#ifdef SUPPORT_VPK
	if ( m_VPKHandle )
	{
		return m_VPKHandle.Tell();
	}
#endif
	if ( m_pFile )
	{
		return m_fs->FS_ftell( m_pFile );
	}
	else if ( m_pPackFileHandle )
	{
		return m_pPackFileHandle->Tell();
	}

	return -1;
}

int CFileHandle::Size()
{
	Assert( IsValid() );

	int nReturnedSize = -1;

#ifdef SUPPORT_VPK
	if ( m_VPKHandle )
	{
		return m_VPKHandle.m_nFileSize;
	}
#endif
	if ( m_pFile  )
	{
		nReturnedSize = m_nLength; 
	}
	else if ( m_pPackFileHandle )
	{
		nReturnedSize = m_pPackFileHandle->Size();
	}

	return nReturnedSize;
}

int64 CFileHandle::AbsoluteBaseOffset()
{
	Assert( IsValid() );

	if ( m_pFile )
	{
		return 0;
	}
	else
	{
		return m_pPackFileHandle->AbsoluteBaseOffset();
	}
}

bool CFileHandle::EndOfFile()
{
	Assert( IsValid() );

	return ( Tell() >= Size() );
}

#ifdef _GAMECONSOLE
static int s_DLC_Numeric_Supported[] =
{
	0, 0, 0, 0, 0, // 5
	0, 0, 0, 0, 0, // 10
	0, 0, 0, 0, 0, // 15
	0, 0, 0, 0, 20, // 20
	0, 0, 0, 0, 0, // 25
	0, 0, 0, 0, 0, // 30
};
static bool IsDlcNumericSupported( int iDLC )
{
#if defined( CSTRIKE15 )
	return ( iDLC >= 1 && iDLC < 31 );
#else // CSTRIKE15
	for ( int k = 0; k < ARRAYSIZE( s_DLC_Numeric_Supported ); ++ k )
		if ( s_DLC_Numeric_Supported[k] == iDLC )
			return true;
	return false;
#endif // CSTRIKE15
}
#else
static bool IsDlcNumericSupported( int iDLC )
{
	return false;
}
#endif

bool CBaseFileSystem::DiscoverDLC( int iController )
{
#if !defined( _X360 )
	return false;
#else
	DevMsg( "Discovering DLC...\n" );

	// clear prior corrupt results
	m_CorruptDLC.Purge();

	CUtlSortVector< DLCContent_t, CDLCLess > dlcResults;

	// development path supports locally mounting the dlc when not using XLAST or XBL
	// this is development only, retail runtime would never have command line dictated DLC
	// command line trumps ANY discovery so we can have a desired exact DLC testing state
	bool bUsingCommandLineDLC = false;
	const char *pCmdLine = CommandLine()->GetCmdLine();
	while( pCmdLine )
	{
		pCmdLine = V_stristr( pCmdLine, "-dlc" );
		if ( !pCmdLine )
			break;

		bUsingCommandLineDLC = true;

		int nDLC = atoi( pCmdLine + 4 );
		if ( nDLC == 0 )
		{
			// malformed command line dev args
			DevWarning( "Bad argument: %s\n", pCmdLine );
			break;
		}
		
		// get the required dlcflags -dlc<n> 0x<flags>
		// user must supply as the lower word identifies control bits
		char const *szDlcNflags = CommandLine()->ParmValue( CFmtStr( "-dlc%d", nDLC ), "0x0" );
		unsigned int nDLCFlags = 0;
		if ( 1 != sscanf( szDlcNflags, "0x%x", &nDLCFlags ) )
		{
			DevWarning( "Bad DLC flags: -dlc%d %s\n", nDLC, szDlcNflags );
			break;
		}

		// form the license mask
		// identify development DLCN as N.0 in the upper MSW, the retail version is at least N.1
		// the LSW are the control flags
		unsigned int nLicenseMask = ( ( nDLC & 0xFF ) << 24 ) | ( nDLCFlags & 0xFFFF );

		DLCContent_t dlcContent;

		// should be part of test image
		// the real DLC encodes descriptive data we can only spoof
		V_strcpy( dlcContent.m_szVolume, "D" );
		V_strtowcs( CFmtStr( "DLC%d Dev Name (0x%8.8x)", nDLC, nLicenseMask ), -1, dlcContent.m_ContentData.szDisplayName, sizeof( dlcContent.m_ContentData.szDisplayName ) );

		dlcContent.m_nController = 0;
		dlcContent.m_LicenseMask = nLicenseMask;
		dlcContent.m_bMounted = ( nLicenseMask & DLCFLAGS_PRESENCE_ONLY ) != 0;

		dlcResults.Insert( dlcContent );

		// next arg
		pCmdLine += 4;
	}

	if ( bUsingCommandLineDLC )
	{
		for ( int i = 0; i < dlcResults.Count(); i++ )
		{
			// only care about new unique occurring DLC
			// skip over any DLC that we have already discovered
			bool bFound = false;
			for ( int j = 0; j < m_DLCContents.Count() && !bFound; j++ )
			{
				bFound = ( DLC_LICENSE_ID( m_DLCContents[j].m_LicenseMask ) == DLC_LICENSE_ID( dlcResults[i].m_LicenseMask ) );
			}
			if ( !bFound )
			{
				m_DLCContents.Insert( dlcResults[i] );
			}
		}
	}
	else
	{
		CUtlMemory< BYTE >	buffer;
		DWORD nNumItems = 0;
		BYTE *pBuffer = NULL;

		// find additional content
		// must have a signed in user, otherwise cannot find enhanced content
		DWORD nBufferSize;
		HANDLE hEnumerator;
		if ( XContentCreateEnumerator(	
				iController, 
				XCONTENTDEVICE_ANY,
				XCONTENTTYPE_MARKETPLACE, 
				0, 
				100, 
				&nBufferSize, 
				&hEnumerator ) == ERROR_SUCCESS )
		{
			if ( nBufferSize )
			{
				// get a buffer to capture enumeration results
				buffer.EnsureCapacity( nBufferSize );
				pBuffer = buffer.Base();
				if ( XEnumerate( hEnumerator, pBuffer, nBufferSize, &nNumItems, NULL ) != ERROR_SUCCESS )
				{
					nNumItems = 0;
				}
			}
		}

		::CloseHandle( hEnumerator );
		if ( !nNumItems )
		{
			return false;
		}

		char szFilename[XCONTENT_MAX_FILENAME_LENGTH+1];
		szFilename[XCONTENT_MAX_FILENAME_LENGTH] = 0;
		XCONTENT_DATA *pContentData;

		// determine all our dlc content
		for ( unsigned int i = 0; i < nNumItems; i++ )
		{
			// filenames are encoded encryptions, useless to anything but the system
			pContentData = (XCONTENT_DATA *)pBuffer + i;
			V_memcpy( szFilename, pContentData->szFileName, XCONTENT_MAX_FILENAME_LENGTH );

			// must mount to get license mask
			// license mask is ONLY available for content downloaded through XBL (not mounted locally)
			DWORD licenseMask = 0;
			DWORD dwStatus = XContentCreate( iController, "DLC", pContentData, XCONTENTFLAG_OPENEXISTING, NULL, &licenseMask, NULL );
			if ( dwStatus != ERROR_SUCCESS )
			{
				// assume corrupt
				DLCCorrupt_t dlcCorrupt;
				dlcCorrupt.m_ContentData = *pContentData;
				m_CorruptDLC.AddToTail( dlcCorrupt );
				continue;
			}

			// always unmount, highest version will get re-mounted
			// as we might rev the DLC without a TU
			XContentClose( "DLC", NULL );

			// DLC N
			int nDlcNumericId = DLC_LICENSE_ID( licenseMask );
			bool bDlcIsSupported = IsDlcNumericSupported( nDlcNumericId );

			// only consider DLC with a valid license mask
			// we DONT/CANT support install-test-locally DLC because DLC lacks license mask to decode
			if ( nDlcNumericId && bDlcIsSupported )
			{
				// insert into ascending sorted list, ensures dlc1..dlcN order
				DLCContent_t dlcContent;
				dlcContent.m_ContentData = *pContentData;
				dlcContent.m_LicenseMask = licenseMask;
				dlcContent.m_nController = iController;
				dlcContent.m_szVolume[0] = '\0';
				dlcContent.m_bMounted = false;
				dlcResults.Insert( dlcContent );
			}
			else
			{
				// assume corrupt
				DLCCorrupt_t dlcCorrupt;
				dlcCorrupt.m_ContentData = *pContentData;
				m_CorruptDLC.AddToTail( dlcCorrupt );
				continue;
			}
		}

		// mount the highest version of each type
		// sorted results order guarantees ascending type/version order
		for ( int i = 0; i < dlcResults.Count(); )
		{
			// iterate ascending list determine highest version of matching type
			DWORD dlcType = DLC_LICENSE_ID( dlcResults[i].m_LicenseMask );
			int nBest = i;
			for ( int j = i+1; j < dlcResults.Count(); j++ )
			{
				if ( dlcType != DLC_LICENSE_ID( dlcResults[j].m_LicenseMask ) )
				{
					// wrong one, due to sort order, no more of this type
					break;
				}
				nBest = j;
			}	

			// only care about unique DLC types, can't handle newly discovered sub versions of the same type
			// iterate for a match
			bool bFound = false;
			for ( int j = 0; j < m_DLCContents.Count() && !bFound; j++ )
			{
				bFound = ( DLC_LICENSE_ID( m_DLCContents[j].m_LicenseMask ) == DLC_LICENSE_ID( dlcResults[nBest].m_LicenseMask ) );
			}
			if ( !bFound )
			{
				// mount the highest version of each type only
				DLCContent_t dlcContent = dlcResults[nBest];
				V_strcpy( dlcContent.m_szVolume, CFmtStr( "DLC%d", DLC_LICENSE_ID( dlcContent.m_LicenseMask ) ) );

				DWORD dwResults;
				if ( dlcContent.m_LicenseMask & DLCFLAGS_PRESENCE_ONLY )
				{
					// we have it, that's all that is required
					// what's inside it is never acessed
					dwResults = ERROR_SUCCESS;
					dlcContent.m_bMounted = true;
				}
				else
				{
					dwResults = XContentCreate(
									iController,
									dlcContent.m_szVolume,
									&dlcContent.m_ContentData,
									XCONTENTFLAG_OPENEXISTING,
									NULL,
									NULL,
									NULL );
				}
				if ( dwResults == ERROR_SUCCESS )
				{
					// already handled corrupt errors, so expecting success
					m_DLCContents.Insert( dlcContent );
				}
			}

			// continue with next dlc type
			i = nBest + 1;
		}
	}

	PrintDLCInfo();

	return ( m_DLCContents.Count() != 0 );
#endif
}

// Returns the number of DLC components found
int CBaseFileSystem::IsAnyDLCPresent( bool *pbDLCSearchPathMounted )
{
	if ( !IsX360() )
	{
		return 0;
	}

	if ( pbDLCSearchPathMounted )
	{
		// discovered DLC may have added new DLC that have not mounted their search path
		*pbDLCSearchPathMounted = true;
		for ( int i = 0; i < m_DLCContents.Count(); i++ )
		{
			if ( !m_DLCContents[i].m_bMounted )
			{
				// set caller's query to false so they know to trigger the AddDLCSearchPaths()
				*pbDLCSearchPathMounted = false;
				break;
			}
		}
	}

	return m_DLCContents.Count();
}

bool CBaseFileSystem::GetAnyDLCInfo( int iDLC, unsigned int *pLicenseMask, wchar_t *pTitleBuff, int nOutTitleSize )
{
	if ( !IsX360() )
	{
		return false;
	}

	if ( !m_DLCContents.IsValidIndex( iDLC ) )
	{
		return false;
	}

	if ( pLicenseMask )
	{
		*pLicenseMask = m_DLCContents[iDLC].m_LicenseMask;
	}

	if ( pTitleBuff )
	{
		V_wcsncpy( pTitleBuff, m_DLCContents[iDLC].m_ContentData.szDisplayName, nOutTitleSize );
	}

	return true;
}

int CBaseFileSystem::IsAnyCorruptDLC()
{
	if ( !IsX360() )
	{
		return 0;
	}

	return m_CorruptDLC.Count();
}

bool CBaseFileSystem::GetAnyCorruptDLCInfo( int iCorruptDLC, wchar_t *pTitleBuff, int nOutTitleSize )
{
	if ( !IsX360() )
	{
		return false;
	}

	if ( !m_CorruptDLC.IsValidIndex( iCorruptDLC ) )
	{
		return false;
	}

	if ( pTitleBuff )
	{
		V_wcsncpy( pTitleBuff, m_CorruptDLC[iCorruptDLC].m_ContentData.szDisplayName, nOutTitleSize );
	}

	return true;
}

bool CBaseFileSystem::IsSpecificDLCPresent( unsigned int nDLCPackage )
{
	for( int i = 0; i < m_DLCContents.Count(); i++ )
	{
		if ( m_DLCContents[i].m_bMounted && ( DLC_LICENSE_ID( m_DLCContents[i].m_LicenseMask ) == nDLCPackage ) )
		{
			return true;
		}
	}

	return false;
}

bool CBaseFileSystem::AddDLCSearchPaths()
{
	if ( !IsX360() )
	{
		return false;
	}

	if ( !IsAnyDLCPresent() )
	{
		return false;
	}

	AsyncFinishAll();

	// have to add the DLC to achieve desired SP order DLCN..DLC1
	for ( int iDLC = 0; iDLC < m_DLCContents.Count(); iDLC++ )
	{
		if ( m_DLCContents[iDLC].m_bMounted )
		{
			// already procesed this on a prior add, skip now
			// only care about newly discovered unique DLC
			continue;
		}

		unsigned int nDLCType = DLC_LICENSE_ID( m_DLCContents[iDLC].m_LicenseMask );

		char szDLCPath[MAX_PATH];
		V_snprintf( szDLCPath, sizeof( szDLCPath ), "%s:\\csgo_dlc%d", m_DLCContents[iDLC].m_szVolume, nDLCType );

		char szDLCLanguagePath[MAX_PATH];
		const char *pLanguageString = NULL;
		if ( XBX_IsAudioLocalized() )
		{
			pLanguageString = XBX_GetLanguageString();
			// paranoid check
			if ( !V_stricmp( pLanguageString, "english" ) )
			{
				// bad, system is confused
				pLanguageString = NULL;
			}
		}
		if ( pLanguageString )
		{		
			V_strncpy( szDLCLanguagePath, CFmtStr( "%s_%s", szDLCPath, pLanguageString ), sizeof( szDLCLanguagePath ) );
		}

		const char *pathTargets[] = { "GAME", "MOD" };
		for ( int i = 0; i < ARRAYSIZE( pathTargets ); i++ )
		{
			// inject dlc path for each target path id, once at top
			for ( int j = 0; j < m_SearchPaths.Count(); j++ )
			{
				if ( m_SearchPaths[j].GetPackFile() && m_SearchPaths[j].GetPackFile()->m_bIsMapPath )
				{
					// skip over any map based search paths
					continue;
				}

				if ( V_stricmp( m_SearchPaths[j].GetPathIDString(), pathTargets[i] ) )
				{
					// skip over any path IDs that we are not interested in
					continue;
				}

				// always after update if present
				int iIndex = j;
				if ( V_stristr( m_SearchPaths[j].GetPathString(), "update" ) )
				{
					// add after update
					iIndex++;
				}

				// scan to find any existing DLC, need to be either before or after
				bool bNoAdd = false;
				for ( int k = iIndex; k < m_SearchPaths.Count(); k++ )
				{
					const char *pCurrentPathString = m_SearchPaths[k].GetPathString();
					const char *pDLCSuffix = V_stristr( pCurrentPathString, "_dlc" );
					if ( !pDLCSuffix )
					{
						break;
					}
					else
					{
						unsigned int nExistingDLC = atoi( pDLCSuffix + 4 );
						if ( !nExistingDLC )
						{
							// uh-oh, can't determine what DLC this is, can't add any other
							DevWarning( "ERROR! Skipping DLC Mount, malformed DLC search path found: %s\n", pCurrentPathString );
							bNoAdd = true;
							break;
						}
						if ( nDLCType > nExistingDLC )
						{
							// stop, we need to be prior to this one
							break;
						}
						else if ( nDLCType == nExistingDLC )
						{
							// uh-oh, we don't add DLCs of the same type
							bNoAdd = true;
							break;
						}
						else
						{
							// the DLC we want to add is smaller, it needs to at least go after this one
							iIndex = k + 1;
						}
					}
				}
			
				if ( bNoAdd )
				{
					// not adding this DLC
					break;
				}

				if ( pLanguageString )
				{
					// only add dlc language search path, if DLC has it
					struct _stat buf;
					if ( FS_stat( szDLCLanguagePath, &buf ) != -1 )
					{
						AddSearchPathInternal( szDLCLanguagePath, pathTargets[i], PATH_ADD_TO_TAIL_ATINDEX, true, iIndex );
						iIndex++;
					}
				}

				AddSearchPathInternal( szDLCPath, pathTargets[i], PATH_ADD_TO_TAIL_ATINDEX, true, iIndex );
				break;
			}
		}

		// never multiply mount or re-mount again
		m_DLCContents[iDLC].m_bMounted = true;
	}

	PrintSearchPaths();
	
	return true;
}

void CBaseFileSystem::PrintDLCInfo()
{
	if ( IsX360() )
	{
		if ( m_DLCContents.Count() )
		{
			Msg( "\nDLC:\n" );
			for ( int i = 0; i < m_DLCContents.Count(); i++ )
			{
				char szTitle[MAX_PATH];
				V_UnicodeToUTF8( m_DLCContents[i].m_ContentData.szDisplayName, szTitle, sizeof( szTitle ) );
				Msg( "DLC Found: '%s' License: 0x%8.8x\n", szTitle, m_DLCContents[i].m_LicenseMask );
			}
		}

		if ( m_CorruptDLC.Count() )
		{
			Msg( "\nCorrupt DLC:\n" );
			for ( int i = 0; i < m_CorruptDLC.Count(); i++ )
			{
				char szTitle[MAX_PATH];
				V_UnicodeToUTF8( m_CorruptDLC[i].m_ContentData.szDisplayName, szTitle, sizeof( szTitle ) );
				Msg( "Corrupt: '%s'\n", szTitle );
			}
		}
	}
}

bool CBaseFileSystem::AddXLSPUpdateSearchPath( const void *pData, int nSize )
{
	if ( IsPC() )
	{
		return false;
	}

	const char *targetPathIDs[] = { "PLATFORM", "GAME", "MOD" };
	for ( int i = 0; i <ARRAYSIZE( targetPathIDs ); i++ )
	{
		RemoveSearchPath( "u:\\xlsppatch", targetPathIDs[i] );
	}

	g_XLSPPatchZipBuffer.Purge();

	if ( !pData || !nSize )
	{
		return true;
	}

	g_XLSPPatchZipBuffer.Put( pData, nSize );

	bool bXLSPPatchValid = true;

	for ( int i = 0; i < ARRAYSIZE( targetPathIDs ); i++ )
	{
		// only specific paths get the xlsppatch override explicitly prepended
		const char *pPathIDString = targetPathIDs[i];

		// search path gets added once per allowed pathID
		bool bFoundSearchPath = false;
		for ( int j = 0; j < m_SearchPaths.Count(); j++ )
		{
			CSearchPath *pSearchPath = &m_SearchPaths[j];
			if ( !V_stricmp( pSearchPath->GetPathIDString(), pPathIDString ) && !V_stricmp( pSearchPath->GetPathString(), "u:\\xlsppatch\\" ) )
			{
				bFoundSearchPath = true;
				break;
			}
		}

		if ( !bFoundSearchPath && g_XLSPPatchZipBuffer.TellPut() )
		{
			// find first matching pathID, xlsppatch will be placed before it
			int nIndex = 0;
			for ( int j = 0; j < m_SearchPaths.Count(); j++ )
			{
				CSearchPath *pSearchPath = &m_SearchPaths[j];
				if ( !V_stricmp( pSearchPath->GetPathIDString(), pPathIDString ) )
				{
					nIndex = j;
					break;
				}
			}

			// have xlsppatch blob
			nIndex = m_SearchPaths.InsertBefore( nIndex );
			CSearchPath *sp = &m_SearchPaths[ nIndex ];

			// the path and filename are fake but reserved, they denote this binary resident blob
			// these names ensure they get ignored during post hdd install sp fixup
			const char *pFullpath = "u:\\xlsppatch\\xlsppatch" PLATFORM_EXT ".zip";
			sp->m_pPathIDInfo = FindOrAddPathIDInfo( g_PathIDTable.AddString( pPathIDString ), -1 );
			sp->m_storeId = g_iNextSearchPathID++;
			sp->SetPath( g_PathIDTable.AddString( "u:\\xlsppatch\\" ) );

			// find and alias existing reference
			CPackFile *pf = NULL;
			for ( int iPackFile = 0; iPackFile < m_ZipFiles.Count(); iPackFile++ )
			{
				if ( !Q_stricmp( m_ZipFiles[iPackFile]->m_ZipName.Get(), pFullpath ) )
				{
					// found
					pf = m_ZipFiles[iPackFile];
					sp->SetPackFile( pf );
					pf->AddRef();
					break;
				}
			}
			if ( !pf )
			{
				// there is no 'file', point to the embedded section instead
				pf = new CZipPackFile( this, g_XLSPPatchZipBuffer.Base() );

				pf->SetPath( sp->GetPath() );
				pf->m_bIsExcluded = false;
				pf->m_ZipName = pFullpath;

				m_ZipFiles.AddToTail( pf );
				sp->SetPackFile( pf );

				pf->m_lPackFileTime = 0;
				pf->m_hPackFileHandleFS = NULL;
				if ( !pf->Prepare( g_XLSPPatchZipBuffer.TellPut() ) )
				{
					bXLSPPatchValid = false;
					break;
				}
			}
		}
	}

	return bXLSPPatchValid;
}

void CBaseFileSystem::MarkLocalizedPath( CSearchPath *sp )
{
// game console only for now
#ifdef _GAMECONSOLE
	const char *pPath = g_PathIDTable.String( sp->GetPath() );
	if ( !pPath || !*pPath )
		return;

	if ( !XBX_IsAudioLocalized() )
	{
		return;
	}

	const char *pLanguage = XBX_GetLanguageString();
	if ( !pLanguage || !V_stricmp( pLanguage, "english" ) )
	{
		return;
	}

	int languagelen = V_strlen( pLanguage );
	int pathlen = V_strlen( pPath );
	// ignore trailing slash
	if ( pPath[ pathlen - 1 ] == '\\' || pPath[ pathlen - 1 ] == '/' )
	{
		--pathlen;
	}

	if ( pathlen > languagelen &&
		 V_strnicmp( pPath + pathlen - languagelen, pLanguage, languagelen ) == 0 )
	{
		sp->m_bIsLocalizedPath = true;
	}
#endif
}
#ifdef SUPPORT_IODELAY_MONITORING
class CIODelayAlarmThread : public CThread
{
public:
	CIODelayAlarmThread( CBaseFileSystem *pFileSystem );
	void WakeUp( void );
	CBaseFileSystem *m_pFileSystem;
	CThreadEvent m_hThreadEvent;

	volatile bool m_bThreadShouldExit;
	// CThread Overrides
	virtual int Run( void );
};


CIODelayAlarmThread::CIODelayAlarmThread( CBaseFileSystem *pFileSystem )
{
	m_pFileSystem = pFileSystem;
	m_bThreadShouldExit = false;

}

void CIODelayAlarmThread::WakeUp( void )
{
	m_hThreadEvent.Set();
}

int CIODelayAlarmThread::Run( void )
{
	while( ! m_bThreadShouldExit )
	{
		uint32 nWaitTime = 1000;
		float flCurTimeout = m_pFileSystem->m_flDelayLimit;
		if ( flCurTimeout > 0. )
		{
			nWaitTime = ( uint32 )( 1000.0 * flCurTimeout );
			m_hThreadEvent.Wait( nWaitTime );
		}
		// check for overflow 
		float flCurTime = Plat_FloatTime();
		if ( flCurTime - m_pFileSystem->m_flLastIOTime > m_pFileSystem->m_flDelayLimit )
		{
			Warning( " %f elapsed w/o i/o\n", flCurTime - m_pFileSystem->m_flLastIOTime );
			DebuggerBreakIfDebugging();
			m_pFileSystem->m_flLastIOTime = MAX( Plat_FloatTime(), m_pFileSystem->m_flLastIOTime );
		}
	}
	return 0;
}


#endif // SUPPORT_IODELAY_MONITORING

void CBaseFileSystem::SetIODelayAlarm( float flTime )
{
#ifdef SUPPORT_IODELAY_MONITORING
	m_flDelayLimit = flTime;
	if ( m_flDelayLimit > 0. )
	{
		m_flDelayLimit = flTime;
		if ( ! m_pDelayThread )
		{
			m_pDelayThread = new CIODelayAlarmThread( this );
			m_pDelayThread->Start();
		}
		m_pDelayThread->WakeUp();
	}
#endif
}

IIoStats *CBaseFileSystem::GetIoStats()
{
#ifndef _CERT
	return &s_IoStats;
#else
	return NULL;
#endif
}

CON_COMMAND( fs_dump_open_duplicate_times, "Set fs_report_long_reads 1 before loading to use this. Prints a list of files that were opened more than once and ~how long was spent reading from them." )
{
	float flTotalTime = 0.0f, flAccumulatedMilliseconds;

	AUTO_LOCK( g_FileOpenDuplicateTimesMutex );
	for ( int nFileOpenDuplicate = 0; nFileOpenDuplicate< g_FileOpenDuplicateTimes.Count(); nFileOpenDuplicate++ )
	{
		FileOpenDuplicateTime_t *pFileOpenDuplicate = g_FileOpenDuplicateTimes[ nFileOpenDuplicate ];
		if ( pFileOpenDuplicate )
		{
			if ( pFileOpenDuplicate->m_nLoadCount > 1 )
			{
				flTotalTime += pFileOpenDuplicate->m_flAccumulatedMicroSeconds;
				flAccumulatedMilliseconds = pFileOpenDuplicate->m_flAccumulatedMicroSeconds / 1000.0f;
				DevMsg( "Times Opened: %3i\t\tAccumulated Time: %10.5fms\t\tAverage Time per Open: %13.8fms\t\tFile: %s\n", 
						pFileOpenDuplicate->m_nLoadCount, flAccumulatedMilliseconds, ( flAccumulatedMilliseconds / pFileOpenDuplicate->m_nLoadCount ), 
						pFileOpenDuplicate->m_szName );
			}
		}
	}

	DevMsg( "Total Seconds: %.5f\n", flTotalTime / 1000000.0f );
}

CON_COMMAND( fs_clear_open_duplicate_times, "Clear the list of files that have been opened." )
{
	AUTO_LOCK( g_FileOpenDuplicateTimesMutex );
	for ( int nFileOpenDuplicate = 0; nFileOpenDuplicate< g_FileOpenDuplicateTimes.Count(); nFileOpenDuplicate++ )
	{
		FileOpenDuplicateTime_t *pFileOpenDuplicate = g_FileOpenDuplicateTimes[ nFileOpenDuplicate ];
		delete pFileOpenDuplicate;
		g_FileOpenDuplicateTimes[ nFileOpenDuplicate ] = NULL;
	}

	g_FileOpenDuplicateTimes.RemoveAll();
}

#if IsPlatformPS3()

class CFiosAllocator : public cell::fios::allocator
{
public:
	void* Allocate(uint32_t size, uint32_t flags, const char* pFile, int line)
	{
		(void) pFile;
		(void) line;
		return memalign(FIOS_ALIGNMENT_FROM_MEMFLAGS(flags), size);
	}

	void Deallocate(void* pMemory, uint32_t flags, const char* pFile, int line)
	{
		(void) flags;
		(void) pFile;
		(void) line;
		free(pMemory);
	}

	void* Reallocate(void* pMemory, uint32_t newSize, uint32_t flags, const char* pFile, int line)
	{
		(void) pMemory;
		(void) newSize;
		(void) flags;
		(void) pFile;
		(void) line;
		return NULL; /* fios does not use Reallocate */
	}
};

// Configure FIOS, allows prefetching of whole files of files within pack files.
class CFiosConfiguration
{
public:
	CFiosConfiguration();
	~CFiosConfiguration();

	bool Setup();
	bool Teardown();

	void FlushCache();

	// These methods are not thread safe.
	bool PrefetchFile( const char * pFileName, int nPriority, bool bPersist );
	bool PrefetchFile( const char * pFileName, int nPriority, bool bPersist, int64 nOffset, int64 nSize );

	void SuspendPrefetches( const char *pWhy );
	void ResumePrefetches( const char *pWhy );

	void PrintPrefetches();
	void PrintPrefetch( int nSlot );
	int ClearFinishedPrefetches();

	const char * GetPathCached() const;

	void CancelAllPrefetches();

	bool IsPrefetchingDone();

private:

	void CancelAndRespawnPrefetches( bool bPersistent );
	int ResumePrefetchesToZero();
	void SuspendPrefetchesToN( int nNumberOfSuspends );

	CFiosAllocator					m_Allocator;
	cell::fios::media *				m_SysCacheMedia;
	cell::fios::scheduler *			m_SchedulerForHDD;
	cell::fios::schedulercache *	m_SchedulerCache;
	cell::fios::media *				m_DevBdvdMedia;
	cell::fios::scheduler *			m_MainScheduler;

	class CPrefetchInfo
	{
	public:
		CPrefetchInfo( cell::fios::op * pOp, const char * pFileName, int64 nOffset, int64 nSize, const cell::fios::opattr_t & opAttributes, bool bPersistent )
			:
			m_OpAttributes( opAttributes ),
			m_nOffset( nOffset ),
			m_nSize( nSize ),
			m_pOp( pOp ),
			m_bPersitent( bPersistent )
		{
			Assert( pOp != NULL );
			V_strncpy( m_FileName, pFileName, sizeof( m_FileName ) );
		}

		// Clone but use a different cell::fios::op *.
		CPrefetchInfo( cell::fios::op * pOp, const CPrefetchInfo & other )
			:
			m_OpAttributes( other.m_OpAttributes ),
			m_nOffset( other.m_nOffset ),
			m_nSize( other.m_nSize ),
			m_pOp( pOp ),
			m_bPersitent( other.m_bPersitent )
		{
			Assert( pOp != NULL );
			V_strncpy( m_FileName, other.m_FileName, sizeof( m_FileName ) );
		}

		cell::fios::op * GetOp() const
		{
			return m_pOp;
		}

		const char * GetFileName() const
		{
			return m_FileName;
		}

		int64 GetOffset() const
		{
			return m_nOffset;
		}

		int64 GetSize() const
		{
			return m_nSize;
		}

		const cell::fios::opattr_t & GetOpAttributes() const
		{
			return m_OpAttributes;
		}

		bool IsPersistent() const
		{
			return m_bPersitent;
		}

	private:
		char					m_FileName[256];
		cell::fios::opattr_t	m_OpAttributes;
		int64					m_nOffset;
		int64					m_nSize;
		cell::fios::op *		m_pOp;
		bool					m_bPersitent;
	};

	CUtlVector< CPrefetchInfo * > m_Prefetches;
};

CFiosConfiguration g_FiosConfiguration;

bool SetupFios()
{
	if ( fs_fios_enabled.GetBool() )
	{
		return g_FiosConfiguration.Setup();
	}
	else
	{
		return false;
	}
}

bool TeardownFios()
{
	if ( fs_fios_enabled.GetBool() )
	{
		return g_FiosConfiguration.Teardown();
	}
	else
	{
		return false;
	}
}

const char * CFiosConfiguration::GetPathCached() const
{
	return g_pPS3PathInfo->GameImagePath();
}

CFiosConfiguration::CFiosConfiguration()
	:
	m_Allocator(),
	m_SysCacheMedia( NULL ),
	m_SchedulerForHDD( NULL ),
	m_SchedulerCache( NULL ),
	m_DevBdvdMedia( NULL ),
	m_MainScheduler( NULL ),
	m_Prefetches( 4, 32 )				// 32 allows us to prefetch 16 high priority prefetches at the same time as one low priority prefetch
										// With the low priority being canceled and restarted every time.
{
	// Do nothing...
}

CFiosConfiguration::~CFiosConfiguration()
{
	// Do nothing...
	Assert( m_Prefetches.Count() == 0 );
}

// SONY overrides new with the types ps3media and schedulercache.
#include "memdbgoff.h"

bool CFiosConfiguration::Setup()
{
	cell::fios::fios_parameters parameters = FIOS_PARAMETERS_INITIALIZER;

	parameters.pAllocator = &m_Allocator;
	parameters.pLargeMemcpy = 0; // Use memcpy
	parameters.pVprintf = 0;     // Use vprintf

	// Enable this only to see if the cache is working as expected
	//parameters.profiling = cell::fios::kProfileOps | cell::fios::kProfileCache;

	cell::fios::FIOSInit( &parameters );

	m_DevBdvdMedia = new cell::fios::ps3media( GetPathCached() );
	if ( m_DevBdvdMedia == NULL )
	{
		Warning( "[CFiosConfiguration::Setup] Can't allocate dev bdvd media.\n" );
		return false;
	}

	m_SysCacheMedia = new cell::fios::ps3media( g_pPS3PathInfo->SystemCachePath() );
	if ( m_SysCacheMedia == NULL )
	{
		Warning( "[CFiosConfiguration::Setup] Can't allocate system cache media.\n" );
		return false;
	}

	m_SchedulerForHDD = cell::fios::scheduler::createSchedulerForMedia( m_SysCacheMedia );
	if ( m_SchedulerForHDD == NULL )
	{
		Warning( "[CFiosConfiguration::Setup] Can't create scheduler for media.\n" );
		return false;
	}

	// Use SONY-like configuration:
	const int BLOCK_SIZE = 1024 * 1024;		// 1 Mb blocks.
	const int NUMBER_OF_BLOCKS = 1800;		// 1800 blocks mean that the cache is going to be 1.8 Gb big.
											// We leave 237 MB for the other usages.
											// In theory, without counting other systems, we can go up to 2037 blocks.

	// To remove one potential cause for a CERT issue, let's remove the usage of this flag (#if 0)
	// This is a speculative fix for crashes that would not exist for the end user (although SONY states that we should not ship with this setting turned on).
#if defined(_CERT) && 0
	const bool bCheckModification = false;	// No need to scan for modifications at release time (faster)
#else
	const bool bCheckModification = true;	// But we want to double-check during development to avoid stale data
#endif

	m_SchedulerCache = new cell::fios::schedulercache( m_DevBdvdMedia, m_SchedulerForHDD,
		"FILECACHE",					// Cache directory
		0x0123456789ABCDEFll,			// diskId. For one disk only title, we can hard-code it
		true,							// Use single-file as it is faster.
		bCheckModification, NUMBER_OF_BLOCKS, BLOCK_SIZE);
	if ( m_SchedulerCache == NULL )
	{
		Warning( "[CFiosConfiguration::Setup] Can't allocate m_SchedulerCache.\n" );
		return false;
	}

	m_MainScheduler = cell::fios::scheduler::createSchedulerForMedia( m_SchedulerCache );
	if (m_MainScheduler == NULL )
	{
		Warning( "[CFiosConfiguration::Setup] Can't allocate m_MainScheduler.\n" );
		return false;
	}

	// Starts the prefetches 0.5 second after no disk-usage.
	// This should make the IO faster for normal cases, prefetch will start when sounds / animations have not been needed IO for a while.
	// Except it creates huge IO issues. These functions are not reliable at all, created deadlocks, etc... Re-enable it on the PS4.
	// m_MainScheduler->setPrefetchDelay( cell::fios::FIOSMillisecondsToAbstime( 500 ) );
	// m_MainScheduler->setPrefetchThrottle( cell::fios::FIOSMillisecondsToAbstime( 100 ) );		// One prefetch command every 3 game frames
																									// This will make the prefetches two times slower.

	m_MainScheduler->setDefault();

	// Currently we are not creating a RAM cache (this could speed up some small reads). 
	return true;
}

bool CFiosConfiguration::Teardown()
{
	// Tearing down, we need to make sure suspend state is reset (FIOS limitation).
	ResumePrefetchesToZero();

	// Ops are taken cared of when we destroy the scheduler, but here we want to make sure PrefetchInfo are also deallocated correctly
	CancelAllPrefetches();

	// We cancel all outstanding operations
	m_MainScheduler->cancelAllOps();

	// Wait for the main scheduler to be idle, so all the operations are effectively canceled (otherwise FIOS may crash in shutdownAndCancelOps).
	while ( m_MainScheduler->isIdle() == false )
	{
		ThreadSleep( 1 );
	}

	// sometimes, files aren't closed by file system, and this will prevent the scheduler from shutting down
	m_MainScheduler->closeAllFiles();
	
	m_MainScheduler->shutdownAndCancelOps();

	cell::fios::scheduler::destroyScheduler( m_MainScheduler );
	m_MainScheduler = NULL;
	delete m_SchedulerCache;
	m_SchedulerCache = NULL;
	m_SchedulerForHDD->shutdownAndCancelOps();
	cell::fios::scheduler::destroyScheduler( m_SchedulerForHDD );
	m_SchedulerForHDD = NULL;
	delete m_SysCacheMedia;
	m_SysCacheMedia = NULL;
	delete m_DevBdvdMedia;
	m_DevBdvdMedia = NULL;

	cell::fios::FIOSTerminate();
	return true;
}

// Turn it back on
#include "memdbgon.h"

void CFiosConfiguration::FlushCache()
{
	m_SchedulerCache->flush();
}

bool CFiosConfiguration::PrefetchFile( const char * pFileName, int nPriority, bool bPersist )
{
#if 0
	// Because prefetching a big file is going to block all the other prefetches (and mess up the priorities if we need some prefetch done ASAP)
	// We are going to split a big prefetch in smaller pieces.

	// This is not necessary anymore:
	// If we cancel a prefetch in the middle and restart it, all the file portion that have been already prefetched before the cancellation
	// will be directly accounted for. I.e. there is not a lot of overhead to cancel and restart a prefetch.

	int64 nFileSize;
	cell::fios::err_t err = cell::fios::scheduler::getDefaultScheduler()->getFileSizeSync( NULL, pFileName, &nFileSize );
	if ( err < 0 )
	{
		Warning( "Can't retrieve size of file '%s'.\n", pFileName );
		return false;
	}

	const int MAX_PREFETCH_BLOCK_SIZE = 16 * 1024 * 1024;		// We prefetch 16 Mb at a time max
																// This is around 2 seconds of prefetching at the BluRay speed.
																// A 1.2 Gb file (like the sound zip file on Portal 2) will use 75 prefetch commands.
																// And will take a little bit more than 2 minutes to be prefetched.
	int64 nOffset = 0;
	int64 nRemainingSize = nFileSize;
	while ( nRemainingSize != 0 )
	{
		int64 nPrefetchSize = MIN( nRemainingSize, MAX_PREFETCH_BLOCK_SIZE );
		if ( PrefetchFile( pFileName, nPriority, bPersist, nOffset, nPrefetchSize ) == false)
		{
			return false;
		}
		nRemainingSize -= nPrefetchSize;
		nOffset += nPrefetchSize;
	}
	return true;
#else
	return PrefetchFile( pFileName, nPriority, bPersist, 0, FIOS_OFF_T_MAX );
#endif
}

bool CFiosConfiguration::PrefetchFile( const char * pFileName, int nPriority, bool bPersist, int64 nOffset, int64 nSize )
{
	// Before we prefetch more, let's clear the old prefetches
	ClearFinishedPrefetches();

	cell::fios::opattr_t opattr = FIOS_OPATTR_INITIALIZER;
	opattr.deadline = bPersist ? kDEADLINE_LATER : kDEADLINE_ASAP;	// If not persistent, assume that we need this sooner rather than later
																	// ASAP maybe a bit high (but still lower than NOW)
																	// This is to handle the case where non-persistent files (usually for a given map)
																	// are prefetched before persistent files (usually for the game in general).

																	// Note that FIOS doe not seem to care about the deadline for prefetches (or the priority). :(
	opattr.priority = nPriority;
	opattr.pCallback = NULL;
	opattr.opflags = bPersist ? cell::fios::kOPF_CACHEPERSIST : 0;
	opattr.pLicense = 0;
	cell::fios::op * pOp = cell::fios::scheduler::getDefaultScheduler()->prefetchFile( &opattr, pFileName, nOffset, nSize );
	if ( pOp == NULL )
	{
		Warning( "FIOS error: Can't prefetch the file '%s'.\n", pFileName );
		return false;
	}

	CPrefetchInfo * pPrefetchInfo = new CPrefetchInfo( pOp, pFileName, nOffset, nSize, opattr, bPersist );
	m_Prefetches.AddToTail( pPrefetchInfo );

	if ( bPersist == false )
	{
		// If the prefetch is not persistent, it is deemed higher priority than persistent prefetch
		// (as they are map specific prefetching, so need to happen sooner rather than later).
		// Due to incorrect priorities in the FIOS prefetching engine, we are going to cancel persistent prefetches
		// And recreate them (they will be at the end of the list and recreated just after, as FIOS seems to execute them in order :().
		// This is a workaround that we hope will work.

		// We can't suspend / resume individual op. We can suspend / resume all prefetches but that will not help us here.
		CancelAndRespawnPrefetches( true );
	}
	return true;
}

void CFiosConfiguration::SuspendPrefetches( const char *pWhy )
{
	if ( fs_fios_spew_prefetches.GetBool() )
	{
		Msg( "[Fios] Suspend prefetches. %s\n", pWhy );
	}
	cell::fios::scheduler::getDefaultScheduler()->suspendPrefetch();
}

void CFiosConfiguration::ResumePrefetches( const char *pWhy )
{
	if ( fs_fios_spew_prefetches.GetBool() )
	{
		Msg( "[Fios] Resume prefetches. %s\n", pWhy );
	}
	cell::fios::scheduler::getDefaultScheduler()->resumePrefetch();
}

void CFiosConfiguration::CancelAndRespawnPrefetches( bool bPersistent )
{
	// This code will add new prefetches, but we are only interested in the prefetch already there

	// Note that this code is pretty inefficient if called several times in a row
	// (like when we prefetch non-persistent file one after the other).
	// But it should not happen often enough to be a big performance issue (we are talking about microseconds, not milliseconds).
	int nSize = m_Prefetches.Count();
	for ( int i = 0 ; i < nSize ; ++i )
	{
		CPrefetchInfo * pPrefetchInfo = m_Prefetches[i];
		if ( pPrefetchInfo->IsPersistent() == false )
		{
			// We want to keep these.
			continue;
		}
		cell::fios::op * pOp = pPrefetchInfo->GetOp();
		if ( pOp->isDone() || pOp->isCancelled() )
		{
			// Already done or canceled, nothing to cancel...
			continue;
		}
		// We cancel it
		pOp->cancel();
		// And recreate it, it will be pretty much pushed at the end of the FIOS stack.
		// If the previous op happened to have finished earlier, re-prefetching it will not do another I/O operation.

		cell::fios::op * pNewOp = cell::fios::scheduler::getDefaultScheduler()->prefetchFile( &pPrefetchInfo->GetOpAttributes(), pPrefetchInfo->GetFileName(),
																		   pPrefetchInfo->GetOffset(), pPrefetchInfo->GetSize() );
		if ( pNewOp == NULL )
		{
			Warning( "FIOS error: Can't prefetch the file '%s'.\n", pPrefetchInfo->GetFileName() );
			continue;		// Not restarting the prefetch is not critical but could reduce the game experience
							// Continue with the other files
		}

		CPrefetchInfo * pNewPrefetchInfo = new CPrefetchInfo( pNewOp, *pPrefetchInfo );
		m_Prefetches.AddToTail( pNewPrefetchInfo );
	}

	// In case the canceled ops are done by now (this will also clear the old CPrefetchInfo).
	ClearFinishedPrefetches();
}

void CFiosConfiguration::PrintPrefetches()
{
	int nSize = m_Prefetches.Count();
	if ( nSize != 0 )
	{
		for (int i = 0 ; i < nSize ; ++i )
		{
			PrintPrefetch( i );
		}
	}
	else
	{
		Msg( "No prefetch in progress.\n" );
	}
}

void CFiosConfiguration::PrintPrefetch( int nSlot )
{
	CPrefetchInfo * pPrefetchInfo = m_Prefetches[nSlot];
	cell::fios::op * pOp = pPrefetchInfo->GetOp();

	int nPriority = pOp->getPriority();
	const float ONE_MEGABYTE = 1024.0f * 1024.0f;
	int64_t nRequestedSize = pOp->getRequestedSize();		// Note that if the file is 1.2 Gb (like the sound zip file in Portal2), the reported size will be incorrect.
	if ( nRequestedSize == FIOS_OFF_T_MAX )
	{
		// We only get the size when we print it (not in normal process).
		int64 nFileSize;
		cell::fios::err_t err = cell::fios::scheduler::getDefaultScheduler()->getFileSizeSync( NULL, pPrefetchInfo->GetFileName(), &nFileSize );
		nRequestedSize = nFileSize - pPrefetchInfo->GetOffset();
	}

	float fRequestedSize = ( nRequestedSize >= 0 ) ? ( ( float )( nRequestedSize ) / ONE_MEGABYTE ) : -1.f;	// Sometimes the size is unknown, use -1 in this case
	float fFullfilledSize = ( float )( pOp->getFulfilledSize() ) / ONE_MEGABYTE;

	if ( pOp->isDone() || pOp->isCancelled() )
	{
		cell::fios::err_t nError = pOp->getError();
		if ( nError < 0 )
		{
			Msg( "Slot[%d] File: '%s' - Priority: %d - Requested size: %0.1f Mb - Fulfilled size: %0.1f Mb - Error: 0x%08X.\n",
				nSlot, pPrefetchInfo->GetFileName(), nPriority, fRequestedSize, fFullfilledSize, nError );
		}
		else
		{
			Msg( "Slot[%d] File: '%s' - Priority: %d - Requested size: %0.1f Mb - Fulfilled size: %0.1f Mb - Done.\n",
				nSlot, pPrefetchInfo->GetFileName(), nPriority, fRequestedSize, fFullfilledSize );
		}
	}
	else
	{
		int millisecondsToCompletion = cell::fios::FIOSAbstimeToMilliseconds( pOp->getEstimatedCompletion()  );
		float fSecondsToCompletion = (float)millisecondsToCompletion / 1000.0f;

		Msg( "Slot[%d] File: '%s' - Priority: %d - Requested size: %0.1f Mb - Fulfilled size: %0.1f Mb - Estimated completion: %0.3f seconds.\n",
			nSlot, pPrefetchInfo->GetFileName(), nPriority, fRequestedSize, fFullfilledSize, fSecondsToCompletion );
	}
}

int CFiosConfiguration::ClearFinishedPrefetches()
{
	int nCleared = 0;
	int nSize = m_Prefetches.Count();
	// From end to beginning to indices are preserved during the scan
	for ( int i = nSize - 1 ; i >= 0 ; --i )
	{
		CPrefetchInfo * pPrefetchInfo = m_Prefetches[i];
		cell::fios::op * pOp = pPrefetchInfo->GetOp();
		if ( pOp->isDone() || pOp->isCancelled() )
		{
			// Remove it from the list
			delete pPrefetchInfo;
			++nCleared;
			cell::fios::scheduler::getDefaultScheduler()->deleteOp( pOp );

			m_Prefetches.Remove( i );
		}
	}
	return nCleared;
}

void CFiosConfiguration::CancelAllPrefetches()
{
	int nSize = m_Prefetches.Count();
	for ( int i = 0 ; i < nSize ; ++i )
	{
		CPrefetchInfo * pPrefetchInfo = m_Prefetches[i];
		cell::fios::op * pOp = pPrefetchInfo->GetOp();
		if ( pOp->isDone() || pOp->isCancelled() )
		{
			continue;
		}
		pOp->cancel();
	}

	// The prefetches may actually have been suspended (like during map loading).
	// cancel() may not do anything in this suspended state, so let's resume so they can complete.
	int nNumberOfSuspends = ResumePrefetchesToZero();

	// This is going to be a blocking call until all the ops are considered done (after cancellation).
	// Ops are canceled very quickly though.
	while ( m_Prefetches.Count() != 0 )
	{
		ClearFinishedPrefetches();
	}

	// Once it is over, we re-suspend the same number of times the prefetches (so the state is the same as when we entered the function)
	SuspendPrefetchesToN( nNumberOfSuspends );
}

int CFiosConfiguration::ResumePrefetchesToZero()
{
	uint32 nNumberOfSuspends = cell::fios::scheduler::getDefaultScheduler()->getPrefetchSuspendCount();

	for ( int i = 0 ; i < nNumberOfSuspends ; ++i )
	{
		cell::fios::scheduler::getDefaultScheduler()->resumePrefetch( "Resume so CancelAllPrefetches() can complete." );
	}
	Assert( cell::fios::scheduler::getDefaultScheduler()->getPrefetchSuspendCount() == 0 );
	return ( int )nNumberOfSuspends;
}

void CFiosConfiguration::SuspendPrefetchesToN( int nNumberOfSuspends )
{
	for ( int i = 0 ; i < nNumberOfSuspends ; ++i )
	{
		cell::fios::scheduler::getDefaultScheduler()->suspendPrefetch( "Suspends restored after CancelAllPrefetches() has completed." );
	}
	Assert( cell::fios::scheduler::getDefaultScheduler()->getPrefetchSuspendCount() == nNumberOfSuspends );
}

bool CFiosConfiguration::IsPrefetchingDone()
{
	int nSize = m_Prefetches.Count();
	for ( int i = 0 ; i < nSize ; ++i )
	{
		CPrefetchInfo * pPrefetchInfo = m_Prefetches[i];
		cell::fios::op * pOp = pPrefetchInfo->GetOp();
		if ( ( pOp->isDone() || pOp->isCancelled() ) == false )
		{
			return false;
		}
	}
	return true;			// All are done or canceled
}

bool CBaseFileSystem::PrefetchFile( const char *pFileName, int nPriority, bool bPersist )
{
	if ( fs_fios_enabled.GetBool() )
	{
		return g_FiosConfiguration.PrefetchFile( pFileName, nPriority, bPersist );
	}
	else
	{
		return false;
	}
}

bool CBaseFileSystem::PrefetchFile( const char *pFileName, int nPriority, bool bPersist, int64 nOffset, int64 nSize )
{
	if ( fs_fios_enabled.GetBool() )
	{
		return g_FiosConfiguration.PrefetchFile( pFileName, nPriority, bPersist, nOffset, nSize );
	}
	else
	{
		return false;
	}
}

void CBaseFileSystem::FlushCache()
{
	if ( fs_fios_enabled.GetBool() )
	{
		g_FiosConfiguration.FlushCache();
	}
}

void CBaseFileSystem::SuspendPrefetches( const char *pWhy )
{
	if ( fs_fios_enabled.GetBool() )
	{
		g_FiosConfiguration.SuspendPrefetches( pWhy );
	}
}

bool g_bUseFiosHddCache = true;

void CBaseFileSystem::OnSaveStateChanged( bool bSaving )
{
	static CInterlockedInt nPrefetchesDueToSaving = 0;

	if ( bSaving )
	{
		// If we are saving, we want to reduce the HDD access as much as possible
		// That way, normal IO and saving IO don't compete for the HDD usage.
		// Normal IO will not be slowed down by the saving (we will use the BluRay instead),
		// And hopefully the saving will be faster too.
		SuspendPrefetches( "Saving" );
		++nPrefetchesDueToSaving;
		g_bUseFiosHddCache = false;					// Let's stop using the HDD cache (read and write)
	}
	else
	{
		// In case, OnSaveStateChanged( false ) is not called as many times as OnSaveStateChanged( true ),
		// let's restore the prefetch state (until nPrefetchesDueToSaving == 0).
		// This is a paranoid code, in case we don't have expected parity.
		for ( ; ; )
		{
			int nResult = --nPrefetchesDueToSaving;
			if ( nResult < 0 )
			{
				// We decremented too far
				++nPrefetchesDueToSaving;			// Put back the value of 0
				break;								// We are done with the resume prefetches related to the save
			}
			ResumePrefetches( "Save finished" );
			// Let's continue until nPrefetchesDueToSaving == 0
		}
		g_bUseFiosHddCache = true;					// Safe to re-use the cache again
	}
}

bool CBaseFileSystem::IsPrefetchingDone()
{
	return g_FiosConfiguration.IsPrefetchingDone( );
}

void CBaseFileSystem::ResumePrefetches( const char *pWhy )
{
	if ( fs_fios_enabled.GetBool() )
	{
		g_FiosConfiguration.ResumePrefetches( pWhy );
	}
}

CON_COMMAND( fs_fios_flush_cache, "Flushes the FIOS HDD cache." )
{
	if ( fs_fios_enabled.GetBool() )
	{
		g_FiosConfiguration.FlushCache();
		Msg( "FIOS cache flushed.\n" );
	}
}

CON_COMMAND( fs_fios_print_prefetches, "Displays all the prefetches currently in progress." )
{
	if ( fs_fios_enabled.GetBool() )
	{
		g_FiosConfiguration.PrintPrefetches();

		// Clear it after displaying the list so the user has a chance to view the one finished
		int nCleared = g_FiosConfiguration.ClearFinishedPrefetches();
		if ( nCleared != 0 )
		{
			Msg( "%d prefetch(s) finished and removed from the list.\n", nCleared );
		}
	}
}

CON_COMMAND( fs_fios_prefetch_file, "Prefetches a file: </PS3_GAME/USRDIR/filename.bin>.\nThe preftech is medium priority and persistent." )
{
	if ( fs_fios_enabled.GetBool() )
	{
		if ( args.ArgC() == 2 )
		{
			// Minimum priority and persistent for full zip file
			// As the assumption is all the data in the zip files have been organized to stay prefetched (like for sounds).
			// We want it persistent so we know after a while it will always stay there, but it is a lower priority prefetch
			// as it may not be used before a while. (short term / non-persistent preftech have a more immediate use).
			bool bSucceeded = g_FiosConfiguration.PrefetchFile( args.Arg( 1 ), -128, true );
			if ( bSucceeded == false )
			{
				Warning( "Prefetch failed. Check if there are other prefetches going on with the command 'fs_fios_print_prefetches'.\n");
			}
		}
		else
		{
			Warning( "Incorrect parameter for the command 'fs_fios_prefetch_file'. Please use a file name (like \"/PS3_GAME/USRDIR/filename.bin\".\n" );
		}
	}
}

CON_COMMAND( fs_fios_prefetch_file_in_pack, "Prefetches a file in a pack: <portal2/models/container_ride/fineDebris_part5.ani>.\nThe preftech is medium priority and non-persistent.")
{
	if ( fs_fios_enabled.GetBool() )
	{
		if ( args.ArgC() == 2 )
		{
			// Higher priority and non-persistent for single file (we don't want the file to pollute the cache, it is just used for short term optimization, like in the next map).
			// But it has to be higher priority than the long term persistent prefetches as there is more chance the prefetch is going to help shortly.
			const char * pFileInPack = args.Arg( 1 );

			char packFileName[256];
			int64 nPosition = 0, nLength = 0;
			if ( g_pBaseFileSystem->GetPackFileInfoFromRelativePath( pFileInPack, "GAME", packFileName, sizeof(packFileName), nPosition, nLength ) )
			{
				const char * pPathCached = g_FiosConfiguration.GetPathCached();
				const int nPathCachedLength = strlen( pPathCached );
				if ( memcmp( packFileName, pPathCached, nPathCachedLength ) == 0 )
				{
					Msg( "Prefetching file: '%s' - Pack file: '%s' - Position: %lld - Size: %lld.\n", pFileInPack, packFileName, nPosition, nLength );
					// We have to skip the path cached as FIOS expects a path relative to that.
					// Priority is set higher than average, just to differentiate with persistent prefetches.
					bool bSucceeded = g_FiosConfiguration.PrefetchFile( packFileName + nPathCachedLength, -127, false, nPosition, nLength );
					if ( bSucceeded == false )
					{
						Warning( "Prefetch failed. Check if there are other prefetches going on with the command 'fs_fios_print_prefetches'.\n" );
					}
				}
				else
				{
					Warning( "Can't prefetch file '%s' in the pack file '%s' as the pack file is not a under the cached path '%s'.\n", pFileInPack, packFileName, pPathCached );
				}
			}
			else
			{
				Warning( "Can't find the corresponding pack file for '%s'.\n", pFileInPack );
			}
		}
		else
		{
			Warning( "Incorrect parameter for the command 'fs_fios_prefetch_file_in_pack'. Please use a file name (like \"portal2/models/container_ride/fineDebris_part5.ani\".\n" );
		}
	}
}

CON_COMMAND( fs_fios_cancel_prefetches, "Cancels all the prefetches in progress." )
{
	if ( fs_fios_enabled.GetBool() )
	{
		g_FiosConfiguration.CancelAllPrefetches();
	}
}

#else

// Fake commands for other platforms...
CON_COMMAND( fs_fios_flush_cache, "Flushes the FIOS HDD cache." )
{
}

CON_COMMAND( fs_fios_print_prefetches, "Displays all the prefetches currently in progress." )
{
}

CON_COMMAND( fs_fios_prefetch_file, "Prefetches a file: </PS3_GAME/USRDIR/filename.bin>.\nThe preftech is medium priority and persistent." )
{
}

CON_COMMAND( fs_fios_prefetch_file_in_pack, "Prefetches a file in a pack: <portal2/models/container_ride/fineDebris_part5.ani>.\nThe preftech is medium priority and non-persistent.")
{
}

CON_COMMAND( fs_fios_cancel_prefetches, "Cancels all the prefetches in progress." )
{
}

#endif




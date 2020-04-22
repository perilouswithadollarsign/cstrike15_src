//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose:  
//
// $NoKeywords: $
//=============================================================================//

#include "tier0/platform.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "basefilesystem.h"

#ifndef _PS3
#include "filesystemasync.h"
#endif

#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"

#ifdef _WIN32
#include "tier0/tslist.h"
#elif defined(POSIX)
#include <fcntl.h>
#ifdef LINUX
#include <sys/file.h>
#endif
#endif
#include "tier1/convar.h"
#include "tier0/vprof.h"
#include "tier1/fmtstr.h"
#include "tier1/utlrbtree.h"


#define GAMEINFO_FILENAME "GAMEINFO.TXT"

bool ShouldFailIo()
{
#if defined( _CERT ) || !defined( _PS3 )
	return false;
#else
	static float s_flFailIoAfter = CommandLine()->ParmValue( "-failioafter", 0.0f );
	return ( s_flFailIoAfter > 0 && Plat_FloatTime() > s_flFailIoAfter );
#endif
}


#if defined( _PS3 )
#include <cell/cell_fs.h>
#include <cell/sysmodule.h>
#include <tier0/memalloc.h>
#include <sys/process.h>
#include <sys/memory.h>
#include <sys/timer.h>
#include <sysutil/sysutil_gamecontent.h>
#include "ps3/ps3_console.h"
// #include "ps3/ps3_gamedata.h"
#include "tls_ps3.h"
#include "ps3_pathinfo.h"
#include <dirent.h>
#include <cell/fios.h>

#if 0 // defined( _PS3 )

#include "MemMgr/inc/MemMgr.h"
#include "FileGroup.h"
#include "const.h"
#include <sys/sys_time.h>
#include "memmgr\inc\PS3VirtualAlloc.h"

char gSrcGameDataPath[MAX_PATH];
bool g_bUseBdvdGameData = false;

extern uint g_ioThreadId;

CFileGroupSystem g_fileGroupSystem;
int g_levelLoadGroup = -1;

#endif //_PS3

#endif 


#ifdef _X360
#undef WaitForSingleObject
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ASSERT_INVARIANT( SEEK_CUR == FILESYSTEM_SEEK_CURRENT );
ASSERT_INVARIANT( SEEK_SET == FILESYSTEM_SEEK_HEAD );
ASSERT_INVARIANT( SEEK_END == FILESYSTEM_SEEK_TAIL );

#ifdef _PS3

/// A bunch of little subroutines to handle all the ickyness necessary
/// in emulating the FindFirstFile() function (use of which is a WTF
/// in itself). 
namespace   // unnamed namespaces are a convenient way to mark a whole bunch of stuff as "static" ie internal linkage
{
	int scandir(const char *dir, struct dirent ***namelist,
		int (*select)(const struct dirent *),
		int (*compar)(const struct dirent **, const struct dirent **))
	{
		DIR *d;
		struct dirent *entry;
		register int i=0;
		size_t entrysize;

		if ((d=opendir(dir)) == NULL)
			return(-1);

		*namelist=NULL;
		while ((entry=readdir(d)) != NULL)
		{
			if (select == NULL || (select != NULL && (*select)(entry)))
			{
				*namelist=(struct dirent **)realloc((void *)(*namelist),
					(size_t)((i+1)*sizeof(struct dirent *)));
				if (*namelist == NULL) return(-1);
				entrysize=sizeof(struct dirent)-sizeof(entry->d_name)+strlen(entry->d_name)+1;
				(*namelist)[i]=(struct dirent *)malloc(entrysize);
				if ((*namelist)[i] == NULL) return(-1);
				memcpy((*namelist)[i], entry, entrysize);
				i++;
			}
		}
		if (closedir(d)) return(-1);
		if (i == 0) return(-1);
		//	if (compar != NULL)
		//		qsort((void *)(*namelist), (size_t)i, sizeof(struct dirent *), compar);

		return(i);
	}

	int alphasort(const struct dirent **a, const struct dirent **b)
	{
		return(strcmp((*a)->d_name, (*b)->d_name));
	}



	char selectBuf[PATH_MAX];

	int FileSelect(const struct dirent *ent)
	{
		const char *mask=selectBuf;
		const char *name=ent->d_name;

		//DEBUG_PRINTF("Test:%s %s\n",mask,name);

		if(!strcmp(name,".") || !strcmp(name,"..") ) return 0;

		if(!strcmp(selectBuf,"*.*")) return 1;

		while( *mask && *name )
		{
			if(*mask=='*')
			{
				mask++; // move to the next char in the mask
				if(!*mask) // if this is the end of the mask its a match 
				{
					return 1;
				}
				while(*name && toupper(*name)!=toupper(*mask)) 
				{ // while the two don't meet up again
					name++;
				}
				if(!*name) 
				{ // end of the name
					break; 
				}
			}
			else if (*mask!='?')
			{
				if( toupper(*mask) != toupper(*name) )
				{	// mismatched!
					return 0;
				}
				else
				{	
					mask++;
					name++;
					if( !*mask && !*name) 
					{ // if its at the end of the buffer
						return 1;
					}

				}

			}
			else /* mask is "?", we don't care*/
			{
				mask++;
				name++;
			}
		}	

		return( !*mask && !*name ); // both of the strings are at the end
	}

	int FillDataStruct(FIND_DATA *dat)
	{
		struct stat fileStat;

		if(dat->numMatches<0)
			return -1;

		Q_strncpy(dat->cFileName,dat->namelist[dat->numMatches]->d_name, sizeof( dat->cFileName ) );

		if(!stat(dat->cFileName,&fileStat))
		{
			dat->dwFileAttributes=fileStat.st_mode;           
		}
		else
		{
			dat->dwFileAttributes=0;
		}	
		//DEBUG_PRINTF("%s\n", dat->namelist[dat->numMatches]->d_name);
		free(dat->namelist[dat->numMatches]);

		dat->numMatches--;
		return 1;
	}


	const char *GetSonyFSErrorString( int errorcode )
	{
		switch( errorcode )
		{
		case CELL_FS_SUCCEEDED:
				return "Normal termination"; 

		case 	CELL_FS_ENOTMOUNTED:
				return "File system corresponding to pathis not mounted";

		case CELL_FS_ENOENT:
				return "File specified by path does not exist";

		case CELL_FS_EIO:
				return "I/O error has occurred";

		case CELL_FS_ENOMEM:
				return "Memory is insufficient ";

		case CELL_FS_ENOTDIR:
				return "Components in path contain something other than a directory";

		case CELL_FS_ENAMETOOLONG:
				return "path or components in the path exceed the maximum length ";

		case CELL_FS_EFSSPECIFIC:
				return "File system specific internal error has occurred";

		case CELL_FS_EFAULT:
				return "pathor sb is NULL";

		case CELL_FS_EACCES:
				return "Search permission is denied for a component of path. ";

		default:
			return "Unknown error code";
		}
	}

}

HANDLE FindFirstFile(char *fileName, FIND_DATA *dat)
{
	char nameStore[PATH_MAX];
	char *dir=NULL;
	int n,iret=-1;

	Q_strncpy(nameStore,fileName, sizeof( nameStore ) );
	FixUpPathCaseForPS3(nameStore);

	if(strrchr(nameStore,'/') )
	{
		dir=nameStore;
		while(strrchr(dir,'/') )
		{
			struct stat dirChk;

			// zero this with the dir name
			dir=strrchr(nameStore,'/');
			*dir='\0';

			dir=nameStore;
			stat(dir,&dirChk);

			if( dirChk.st_mode & _S_IFDIR )
			{
				break;	
			}
		}
	}
	else
	{
		// couldn't find a dir seperator...
		return ( void * ) INVALID_HANDLE_VALUE;
	}

	if( strlen(dir)>0 )
	{
		Q_strncpy(selectBuf,fileName+strlen(dir)+1, sizeof( selectBuf ) );

		n = scandir(dir, &dat->namelist, FileSelect, alphasort);
		if (n < 0)
		{
			// silently return, nothing interesting
		}
		else 
		{
			dat->numMatches=n-1; // n is the number of matches
			iret=FillDataStruct(dat);
			if(iret<0)
			{
				free(dat->namelist);
			}

		}
	}

	return reinterpret_cast<void*>(iret);
}

bool FindNextFile(HANDLE handle, FIND_DATA *dat)
{
	AssertMsg( false, "WARNING: untested\n" );
	if(dat->numMatches<0)
	{	
		free(dat->namelist);
		return false; // no matches left
	}	

	FillDataStruct(dat);
	return true;
}

bool FindClose(HANDLE handle)
{
	AssertMsg( false, "WARNING: untested\n" );
	return true;
}

#endif

#if 0 // defined(_PS3)

#define DebugPrint(fmt, ...)	Msg( fmt, ## __VA_ARGS__ )

static bool ThreadInIoThread()
{
    return( ThreadGetCurrentId() == g_ioThreadId );
}



#endif //_PS3

#if __DARWIN_64_BIT_INO_T
#error badness
#endif

#if _DARWIN_FEATURE_64_BIT_INODE
#error additional badness
#endif
//-----------------------------------------------------------------------------

class CFileSystem_Stdio : public CBaseFileSystem
{
public:
	CFileSystem_Stdio();
	~CFileSystem_Stdio();

	// Used to get at older versions
	void *QueryInterface( const char *pInterfaceName );

	// Higher level filesystem methods requiring specific behavior
	virtual void GetLocalCopy( const char *pFileName );
	virtual int	HintResourceNeed( const char *hintlist, int forgetEverything );
	virtual bool IsFileImmediatelyAvailable(const char *pFileName);
	virtual WaitForResourcesHandle_t WaitForResources( const char *resourcelist );
	virtual bool GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float *progress /* out */ , bool *complete /* out */ );
	virtual void CancelWaitForResources( WaitForResourcesHandle_t handle );
	virtual bool IsSteam() const { return false; }
	virtual	FilesystemMountRetval_t MountSteamContent( int nExtraAppId = -1 ) { return FILESYSTEM_MOUNT_OK; }

	bool GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign );
	void *AllocOptimalReadBuffer( FileHandle_t hFile, unsigned nSize, unsigned nOffset );
	void FreeOptimalReadBuffer( void *p );

protected:
	// implementation of CBaseFileSystem virtual functions
	virtual FILE *FS_fopen( const char *filename, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo );
	virtual void FS_setbufsize( FILE *fp, unsigned nBytes );
	virtual void FS_fclose( FILE *fp );
	virtual void FS_fseek( FILE *fp, int64 pos, int seekType );
	virtual long FS_ftell( FILE *fp );
	virtual int FS_feof( FILE *fp );
	virtual size_t FS_fread( void *dest, size_t destSize, size_t size, FILE *fp );
	virtual size_t FS_fwrite( const void *src, size_t size, FILE *fp );
	virtual bool FS_setmode( FILE *fp, FileMode_t mode );
	virtual size_t FS_vfprintf( FILE *fp, const char *fmt, va_list list );
	virtual int FS_ferror( FILE *fp );
	virtual int FS_fflush( FILE *fp );
	virtual char *FS_fgets( char *dest, int destSize, FILE *fp );
	virtual int FS_stat( const char *path, struct _stat *buf );
	virtual int FS_chmod( const char *path, int pmode );
	virtual HANDLE FS_FindFirstFile(const char *findname, WIN32_FIND_DATA *dat);
	virtual bool FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat);
	virtual bool FS_FindClose(HANDLE handle);
	virtual int FS_GetSectorSize( FILE * );

private:
	bool CanAsync() const
	{
		return m_bCanAsync;
	}

	bool m_bMounted;
	bool m_bCanAsync;
};


//-----------------------------------------------------------------------------
// Per-file worker classes
//-----------------------------------------------------------------------------
abstract_class CStdFilesystemFile
{
public:
	virtual ~CStdFilesystemFile() {}
	virtual void FS_setbufsize( unsigned nBytes ) = 0;
	virtual void FS_fclose() = 0;
	virtual void FS_fseek( int64 pos, int seekType ) = 0;
	virtual long FS_ftell() = 0;
	virtual int FS_feof() = 0;
	virtual size_t FS_fread( void *dest, size_t destSize, size_t size ) = 0;
	virtual size_t FS_fwrite( const void *src, size_t size ) = 0;
	virtual bool FS_setmode( FileMode_t mode ) = 0;
	virtual size_t FS_vfprintf( const char *fmt, va_list list ) = 0;
	virtual int FS_ferror() = 0;
	virtual int FS_fflush() = 0;
	virtual char *FS_fgets( char *dest, int destSize ) = 0;
	virtual int FS_GetSectorSize() { return 1; }
};

//---------------------------------------------------------

class CStdioFile : public CStdFilesystemFile
{
public:
	static CStdioFile *FS_fopen( const char *filename, const char *options, int64 *size );

	virtual void FS_setbufsize( unsigned nBytes );
	virtual void FS_fclose();
	virtual void FS_fseek( int64 pos, int seekType );
	virtual long FS_ftell();
	virtual int FS_feof();
	virtual size_t FS_fread( void *dest, size_t destSize, size_t size);
	virtual size_t FS_fwrite( const void *src, size_t size );
	virtual bool FS_setmode( FileMode_t mode );
	virtual size_t FS_vfprintf( const char *fmt, va_list list );
	virtual int FS_ferror();
	virtual int FS_fflush();
	virtual char *FS_fgets( char *dest, int destSize );

#if defined( POSIX ) && !defined( _PS3 )
	static CUtlMap< int, CInterlockedInt > m_LockedFDMap;
#endif
private:
	CStdioFile( FILE *pFile, bool bWriteable )
		: m_pFile( pFile ), m_bWriteable( bWriteable )
	{
	}

	FILE *m_pFile;
	bool m_bWriteable;
};

#if defined( POSIX ) && !defined( _PS3 )
CUtlMap< int, CInterlockedInt > CStdioFile::m_LockedFDMap;
#endif

//-----------------------------------------------------------------------------

#ifdef _WIN32
class CWin32ReadOnlyFile : public CStdFilesystemFile
{
public:
	static bool CanOpen( const char *filename, const char *options );
	static CWin32ReadOnlyFile *FS_fopen( const char *filename, const char *options, int64 *size );

	virtual void FS_setbufsize( unsigned nBytes ) {}
	virtual void FS_fclose();
	virtual void FS_fseek( int64 pos, int seekType );
	virtual long FS_ftell();
	virtual int FS_feof();
	virtual size_t FS_fread( void *dest, size_t destSize, size_t size);
	virtual size_t FS_fwrite( const void *src, size_t size ) { return 0; }
	virtual bool FS_setmode( FileMode_t mode ) { Error( "Can't set mode, open a second file in right mode\n" ); return false; }
	virtual size_t FS_vfprintf( const char *fmt, va_list list ) { return 0; }
	virtual int FS_ferror() { return 0;	}
	virtual int FS_fflush() { return 0; }
	virtual char *FS_fgets( char *dest, int destSize );
	virtual int FS_GetSectorSize() { return m_SectorSize; }

private:
	CWin32ReadOnlyFile( HANDLE hFileUnbuffered, HANDLE hFileBuffered, int sectorSize, int64 fileSize, bool bOverlapped )
	 :	m_hFileUnbuffered( hFileUnbuffered ),
		m_hFileBuffered( hFileBuffered ),
		m_ReadPos( 0 ),
		m_Size( fileSize ),
		m_SectorSize( sectorSize ),
		m_bOverlapped( bOverlapped )
	{
	}

	int64				m_ReadPos;
	int64				m_Size;
	HANDLE				m_hFileUnbuffered;
	HANDLE				m_hFileBuffered;
	CThreadFastMutex	m_Mutex;
	int					m_SectorSize;
	bool				m_bOverlapped;
};

#endif

#if IsPlatformPS3()
class CFiosReadOnlyFile : public CStdFilesystemFile
{
public:
	static bool CanOpen( const char *filename, const char *options );
	static CFiosReadOnlyFile *FS_fopen( const char *filename, const char *options, int64 *size );

	virtual void FS_setbufsize( unsigned nBytes ) {}
	virtual void FS_fclose();
	virtual void FS_fseek( int64 pos, int seekType );
	virtual long FS_ftell();
	virtual int FS_feof();
	virtual size_t FS_fread( void *dest, size_t destSize, size_t size);
	virtual size_t FS_fwrite( const void *src, size_t size ) { return 0; }
	virtual bool FS_setmode( FileMode_t mode ) { Error( "Can't set mode, open a second file in right mode\n" ); return false; }
	virtual size_t FS_vfprintf( const char *fmt, va_list list ) { return 0; }
	virtual int FS_ferror() { return 0;	}
	virtual int FS_fflush() { return 0; }
	virtual char *FS_fgets( char *dest, int destSize );
	virtual int FS_GetSectorSize() { return 2048; }

private:
	CFiosReadOnlyFile( cell::fios::filehandle * pFileHandle, int64 nFileSize )
		:
		m_pHandle( pFileHandle ),
		m_nSize( nFileSize ),
		m_nReadPos( 0 )
	{
		// Do nothing...
	}

	cell::fios::filehandle * m_pHandle;
	int64					 m_nSize;
	int64					 m_nReadPos;
};

#endif


//-----------------------------------------------------------------------------
// singleton
//-----------------------------------------------------------------------------
CFileSystem_Stdio g_FileSystem_Stdio;

#ifndef _PS3
CAsyncFileSystem g_FileSystem_Async;
#endif

#if defined(_WIN32) && defined(DEDICATED)
CBaseFileSystem *BaseFileSystem_Stdio( void )
{
	return &g_FileSystem_Stdio;
}
#endif
 
#if defined( DEDICATED ) && defined( LAUNCHERONLY ) // "hack" to allow us to not export a stdio version of the FILESYSTEM_INTERFACE_VERSION anywhere

IFileSystem *g_pFileSystem = &g_FileSystem_Stdio;
IBaseFileSystem *g_pBaseFileSystem = &g_FileSystem_Stdio;


#else

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CFileSystem_Stdio, IFileSystem, FILESYSTEM_INTERFACE_VERSION, g_FileSystem_Stdio );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CFileSystem_Stdio, IBaseFileSystem, BASEFILESYSTEM_INTERFACE_VERSION, g_FileSystem_Stdio );

#endif

#ifndef _PS3
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CAsyncFileSystem, IAsyncFileSystem, ASYNCFILESYSTEM_INTERFACE_VERSION, g_FileSystem_Async );
#endif // _PS3


//-----------------------------------------------------------------------------

bool UseOptimalBufferAllocation()
{
	static bool bUseOptimalBufferAllocation = ( IsX360() || ( !IsPosix() && Q_stristr( Plat_GetCommandLine(), "-unbuffered_io" ) != NULL ) );
	return bUseOptimalBufferAllocation;
}
ConVar filesystem_unbuffered_io( "filesystem_unbuffered_io", "1", 0, "" );
#define UseUnbufferedIO() ( UseOptimalBufferAllocation() && filesystem_unbuffered_io.GetBool() )

ConVar filesystem_native( "filesystem_native", "1", 0, "Use native FS or STDIO" );
ConVar filesystem_max_stdio_read( "filesystem_max_stdio_read", IsX360() ? "64" : "16", 0, "" );
ConVar filesystem_report_buffered_io( "filesystem_report_buffered_io", "0" );

#if IsPlatformPS3()
extern bool g_bUseFiosHddCache;
ConVar fs_fios_enable_hdd_cache( "fs_fios_enable_hdd_cache", "0", 0, "Use fios HDD cache, disable this to have normal BluRay speed. 1 to enable it, 0 to disable it." );
#endif

//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CFileSystem_Stdio::CFileSystem_Stdio()
{
	m_bMounted = false;
	m_bCanAsync = true;
#if defined( POSIX ) && !defined( _PS3 )
	SetDefLessFunc( CStdioFile::m_LockedFDMap );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFileSystem_Stdio::~CFileSystem_Stdio()
{
	Assert(!m_bMounted);
}


//-----------------------------------------------------------------------------
// QueryInterface: 
//-----------------------------------------------------------------------------
void *CFileSystem_Stdio::QueryInterface( const char *pInterfaceName )
{
	// We also implement the IMatSystemSurface interface
	if (!Q_strncmp(	pInterfaceName, FILESYSTEM_INTERFACE_VERSION, Q_strlen(FILESYSTEM_INTERFACE_VERSION) + 1))
		return (IFileSystem*)this;

	return CBaseFileSystem::QueryInterface( pInterfaceName );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::GetOptimalIOConstraints( FileHandle_t hFile, unsigned *pOffsetAlign, unsigned *pSizeAlign, unsigned *pBufferAlign )
{
	unsigned sectorSize;
	
	CFileHandle *fh = ( CFileHandle *)hFile;
#ifdef SUPPORT_VPK
	if ( fh && fh->m_VPKHandle  )
	{
		return false;
	}
#endif
	if ( hFile && UseOptimalBufferAllocation() )
	{
		sectorSize = fh->GetSectorSize();
		
		if ( !sectorSize || ( fh->m_pPackFileHandle && ( fh->m_pPackFileHandle->AbsoluteBaseOffset() % sectorSize ) ) )
		{
			sectorSize = 1;
		}
	}
	else
	{
		sectorSize = 1;
	}

	if ( pOffsetAlign )
	{
		*pOffsetAlign = sectorSize;
	}

	if ( pSizeAlign )
	{
		*pSizeAlign = sectorSize;
	}

	if ( pBufferAlign )
	{
		if ( IsX360() )
		{
			*pBufferAlign = 4;
		}
		else
		{
			*pBufferAlign = sectorSize;
		}
	}

	return ( sectorSize > 1 );
}

// was from launcher.cpp in EA PS3 port, but can't do that in a PRX
const char* GetGameMode()
{
	return "portal2";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void *CFileSystem_Stdio::AllocOptimalReadBuffer( FileHandle_t hFile, unsigned nSize, unsigned nOffset )
{
	if ( !UseOptimalBufferAllocation() )
	{
		return CBaseFileSystem::AllocOptimalReadBuffer( hFile, nSize, nOffset );
	}

	unsigned sectorSize;
	if ( hFile != FILESYSTEM_INVALID_HANDLE )
	{
		CFileHandle *fh = ( CFileHandle *)hFile;
		sectorSize = fh->GetSectorSize();

		if ( !nSize )
		{
			nSize = fh->Size();
		}

		if ( fh->m_pPackFileHandle )
		{
			nOffset += fh->m_pPackFileHandle->AbsoluteBaseOffset();
		}
	}
	else
	{
		// an invalid handle gets a fake "optimal" but valid buffer
		// this path is for a caller that isn't doing i/o, 
		// but needs an "optimal" buffer that can end up passed to FreeOptimalReadBuffer()
		sectorSize = 4;
	}

	bool bOffsetIsAligned = ( nOffset % sectorSize == 0 );
	unsigned nAllocSize = ( bOffsetIsAligned ) ? AlignValue( nSize, sectorSize ) : nSize;

	if ( IsX360() )
	{
		return malloc( nAllocSize );
	}
	else
	{
		unsigned nAllocAlignment = ( bOffsetIsAligned ) ? sectorSize : 4;
		return _aligned_malloc( nAllocSize, nAllocAlignment );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FreeOptimalReadBuffer( void *p )
{
	if ( !UseOptimalBufferAllocation() )
	{
		CBaseFileSystem::FreeOptimalReadBuffer( p );
		return;
	}

	if ( p )
	{
		if ( IsX360() )
		{
			free( p );
		}
		else
		{
			 _aligned_free( p );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
FILE *CFileSystem_Stdio::FS_fopen( const char *filename, const char *options, unsigned flags, int64 *size, CFileLoadInfo *pInfo )
{
	if( ShouldFailIo() )
		return NULL;

	CStdFilesystemFile *pFile = NULL;

	if ( pInfo )
		pInfo->m_bLoadedFromSteamCache = false;


#ifdef _WIN32
	if ( CWin32ReadOnlyFile::CanOpen( filename, options ) )
	{
		pFile = CWin32ReadOnlyFile::FS_fopen( filename, options, size );
		return (FILE *)pFile;
	}
#endif

#if IsPlatformPS3()
	if ( CFiosReadOnlyFile::CanOpen( filename, options ) )
	{
		pFile = CFiosReadOnlyFile::FS_fopen( filename, options, size );
		return (FILE *)pFile;
	}
#endif

	pFile = CStdioFile::FS_fopen( filename, options, size );

	return (FILE *)pFile;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FS_setbufsize( FILE *fp, unsigned nBytes )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	pFile->FS_setbufsize( nBytes );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FS_fclose( FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);

	if ( m_WhitelistFileTrackingEnabled )
		m_FileTracker2.RecordFileClose( fp );

	pFile->FS_fclose();
	delete pFile;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FS_fseek( FILE *fp, int64 pos, int seekType )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);

	if ( m_WhitelistFileTrackingEnabled )
		m_FileTracker2.RecordFileSeek( fp, pos, seekType );

	pFile->FS_fseek( pos, seekType );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
long CFileSystem_Stdio::FS_ftell( FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_ftell();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_feof( FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_feof();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Stdio::FS_fread( void *dest, size_t destSize, size_t size, FILE *fp )
{
	if( ShouldFailIo() )
		return 0;

	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	size_t nBytesRead = pFile->FS_fread( dest, destSize, size);

	Trace_FRead( nBytesRead, fp );

	if ( m_WhitelistFileTrackingEnabled )
		m_FileTracker2.RecordFileRead( dest, nBytesRead, size, fp );

	return nBytesRead;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Stdio::FS_fwrite( const void *src, size_t size, FILE *fp )
{
	if( ShouldFailIo() )
		return 0;

	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);

	size_t nBytesWritten = pFile->FS_fwrite(src, size);

	return nBytesWritten;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::FS_setmode( FILE *fp, FileMode_t mode )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_setmode( mode );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Stdio::FS_vfprintf( FILE *fp, const char *fmt, va_list list )
{
	if( ShouldFailIo() )
		return 0;
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_vfprintf(fmt, list);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_ferror( FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_ferror();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_fflush( FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_fflush();
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char *CFileSystem_Stdio::FS_fgets( char *dest, int destSize, FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_fgets(dest, destSize);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			pmode - 
// Output : int
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_chmod( const char *path, int pmode )
{
	if ( !path )
		return -1;

	int rt = _chmod( path, pmode );
#if defined( LINUX )
	if (rt==-1)
	{
		char file[MAX_PATH];
		if ( findFileInDirCaseInsensitive(path, file) )
		{
			rt=_chmod(file,pmode);
		}
	}	
#endif
	return rt;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_stat( const char *path, struct _stat *buf )
{
	if ( !path )
	{
		return -1;
	}

	int rt;
#ifdef _PS3
    CellFsStat cellBuf;
    CellFsErrno retFs = cellFsStat(path, &cellBuf);
    if(retFs == CELL_FS_SUCCEEDED)
    {
        buf->st_atime = cellBuf.st_atime;
        buf->st_blksize = cellBuf.st_blksize;
        buf->st_ctime = cellBuf.st_ctime;
        buf->st_gid = cellBuf.st_gid;
        buf->st_mode = cellBuf.st_mode;
        buf->st_mtime = cellBuf.st_mtime;
        buf->st_size = cellBuf.st_size;
        buf->st_uid = cellBuf.st_uid;
        buf->st_dev = 0;
        buf->st_ino = 0;
        buf->st_nlink = 0;
        buf->st_rdev = 0;
        buf->st_blocks = 0;
        rt = 0;
    }
    else
    {
        rt = -1;
        //TBD: SET ERRNO
    }
#else
    rt = _stat( path, buf );
#endif
#if defined(LINUX)
	if ( rt == -1 )
	{
		char file[MAX_PATH];
		if ( findFileInDirCaseInsensitive( path, file ) )
		{
			rt = _stat( file, buf );
		}
	}	
#endif
	return rt;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
HANDLE CFileSystem_Stdio::FS_FindFirstFile(const char *findname, WIN32_FIND_DATA *dat)
{
	return ::FindFirstFile(const_cast<char *>(findname), dat);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::FS_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat)
{
	return (::FindNextFile(handle, dat) != 0);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::FS_FindClose(HANDLE handle)
{
	return (::FindClose(handle) != 0);
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_GetSectorSize( FILE *fp )
{
	CStdFilesystemFile *pFile = ((CStdFilesystemFile *)fp);
	return pFile->FS_GetSectorSize();
}

//-----------------------------------------------------------------------------
// Purpose: files are always immediately available on disk
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::IsFileImmediatelyAvailable(const char *pFileName)
{
	return true;
}

// enable this if you want the stdio filesystem to pretend it's steam, and make people wait for resources
//#define DEBUG_WAIT_FOR_RESOURCES_API

#if defined(DEBUG_WAIT_FOR_RESOURCES_API)
static float g_flDebugProgress = 0.0f;
#endif

//-----------------------------------------------------------------------------
// Purpose: steam call, unnecessary in stdio
//-----------------------------------------------------------------------------
WaitForResourcesHandle_t CFileSystem_Stdio::WaitForResources( const char *resourcelist )
{
#if defined(DEBUG_WAIT_FOR_RESOURCES_API)
	g_flDebugProgress = 0.0f;
#endif

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: steam call, unnecessary in stdio
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float *progress /* out */ , bool *complete /* out */ )
{
	VPROF_BUDGET( "GetWaitForResourcesProgress (stdio)", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

#if defined(DEBUG_WAIT_FOR_RESOURCES_API)
	g_flDebugProgress += 0.002f;
	if (g_flDebugProgress < 1.0f)
	{
		*progress = g_flDebugProgress;
		*complete = false;
		return true;
	}
#endif

	// always return that we're complete
	*progress = 0.0f;
	*complete = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: steam call, unnecessary in stdio
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::CancelWaitForResources( WaitForResourcesHandle_t handle )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::GetLocalCopy( const char *pFileName )
{
	// do nothing. . everything is local.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::HintResourceNeed( const char *hintlist, int forgetEverything )
{
	// do nothing. . everything is local.
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
CStdioFile *CStdioFile::FS_fopen( const char *filename, const char *options, int64 *size )
{
	MEM_ALLOC_CREDIT();

	FILE *pFile = NULL;

	// stop newline characters at end of filename
	Assert(!strchr(filename, '\n') && !strchr(filename, '\r'));
	
	pFile = fopen(filename, options);
	if (pFile && size)
	{
		// todo: replace with filelength()? 
		struct _stat buf;
		int rt = _stat( filename, &buf );
		if (rt == 0)
		{
			*size = buf.st_size;
		}
	}

#if defined( LINUX )
	if(!pFile && !strchr(options,'w') && !strchr(options,'+') ) // try opening the lower cased version
	{
		char file[MAX_PATH];
		if ( findFileInDirCaseInsensitive(filename, file ) )
		{	
			pFile = fopen( file, options );

			if (pFile && size)
			{
				// todo: replace with filelength()? 
				struct _stat buf;
				int rt = _stat( file, &buf );
				if (rt == 0)
				{
					*size = buf.st_size;
				}
			}
		}
	}
#endif

	if ( pFile )
	{
		bool bWriteable = false;
		if ( strchr(options,'w') || strchr(options,'a') )
			bWriteable = true;
		
#if defined( POSIX ) && !defined( _PS3 )
		if ( bWriteable )
		{
			// Win32 has an undocumented feature that is serialized ALL writes to a file across threads (i.e only 1 thread can open a file at a time)
			// so use flock here to mimic that behavior
			
			ThreadId_t curThread = ThreadGetCurrentId();
			
			int fd = fileno_unlocked( pFile );
			int iLockID = m_LockedFDMap.Find( fd );
			int ret = flock( fd, LOCK_EX | LOCK_NB );
			if ( ret < 0 )
			{
				if ( errno == EWOULDBLOCK  )
				{
					if ( iLockID != m_LockedFDMap.InvalidIndex() && 
						 m_LockedFDMap[iLockID] != -1 && 
						 curThread != m_LockedFDMap[iLockID] )
					{
						ret = flock( fd, LOCK_EX );
						if ( ret < 0 )
						{
							fclose( pFile );
							return NULL;
						}
					}
				}
				else 
				{
					fclose( pFile );
					return NULL;
				}
			}

			if ( iLockID != m_LockedFDMap.InvalidIndex() )
				m_LockedFDMap[iLockID] = curThread;
			else
				m_LockedFDMap.Insert( fd, curThread );
			
			rewind( pFile );
		}
#endif
		return new CStdioFile( pFile, bWriteable );
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CStdioFile::FS_setbufsize( unsigned nBytes )
{
#ifdef _PS3
	if ( nBytes )
	{
		setvbuf( m_pFile, NULL, _IOFBF,  nBytes );
	}
#elif defined _WIN32
	if ( nBytes )
	{
		setvbuf( m_pFile, NULL, _IOFBF,  32768 );
	}
	else
	{
		setvbuf( m_pFile, NULL, _IONBF,  0 );
		// hack to make microsoft stdio not always read one stray byte on odd sized files
		// hopefully this isn't needed on vs2015??
#if (defined(_MSC_VER) && (_MSC_VER < 1900))
		m_pFile->_bufsiz = 1;
#endif
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CStdioFile::FS_fclose()
{
#if defined( POSIX ) && !defined( _PS3 )
	if ( m_bWriteable )
	{
		fflush( m_pFile );
		int fd = fileno_unlocked( m_pFile );
		flock( fd, LOCK_UN );
		int iLockID = m_LockedFDMap.Find( fd );
		if ( iLockID != m_LockedFDMap.InvalidIndex() )
			m_LockedFDMap[ iLockID ] = -1;
	}
#endif
	fclose(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CStdioFile::FS_fseek( int64 pos, int seekType )
{
	int nNewSeekType = seekType;
	
	// Handle values outside the 32-bit signed range.
	// Only 0 or 1 of the while loops will execute inthe same call.

	while ( pos > INT_MAX )
	{
		fseek( m_pFile, INT_MAX, nNewSeekType );
		pos -= INT_MAX;
		nNewSeekType = SEEK_CUR; // Now all seeks need to be relative
	}

	while ( pos < INT_MIN )
	{
		fseek( m_pFile, INT_MIN, nNewSeekType );
		pos -= INT_MIN;
		nNewSeekType = SEEK_CUR; // Now all seeks need to be relative
	}

	fseek( m_pFile, pos, nNewSeekType );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
long CStdioFile::FS_ftell()
{
	return ftell(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CStdioFile::FS_feof()
{
	return feof(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CStdioFile::FS_fread( void *dest, size_t destSize, size_t size )
{
	// read (size) of bytes to ensure truncated reads returns bytes read and not 0
	return fread( dest, 1, size, m_pFile );
}


#define WRITE_CHUNK		(256 * 1024)

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//
// This routine breaks data into chunks if the amount to be written is beyond WRITE_CHUNK (256kb)
// Windows can fail on monolithic writes of ~12MB or more, so we work around that here
//-----------------------------------------------------------------------------
size_t CStdioFile::FS_fwrite( const void *src, size_t size )
{
	if ( size > WRITE_CHUNK )
	{
		size_t remaining = size;
		const byte* current = (const byte *) src;
		size_t total = 0;

		while ( remaining > 0 )
		{
			size_t bytesToCopy = MIN(remaining, WRITE_CHUNK);

			total += fwrite(current, 1, bytesToCopy, m_pFile);

			remaining -= bytesToCopy;
			current += bytesToCopy;
		}

		Assert( total == size );
		return total;
	}

	return fwrite(src, 1, size, m_pFile);// return number of bytes written (because we have size = 1, count = bytes, so it return bytes)
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
bool CStdioFile::FS_setmode( FileMode_t mode )
{
#ifdef _WIN32
	int fd = _fileno( m_pFile );
	int newMode = ( mode == FM_BINARY ) ? _O_BINARY : _O_TEXT;
	return ( _setmode( fd, newMode) != -1 );
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CStdioFile::FS_vfprintf( const char *fmt, va_list list )
{
	return vfprintf(m_pFile, fmt, list);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CStdioFile::FS_ferror()
{
	return ferror(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CStdioFile::FS_fflush()
{
	return fflush(m_pFile);
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char *CStdioFile::FS_fgets( char *dest, int destSize )
{
	return fgets(dest, destSize, m_pFile);
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
#ifdef _WIN32

ConVar filesystem_use_overlapped_io( "filesystem_use_overlapped_io", "1", 0, "" );
#define UseOverlappedIO() filesystem_use_overlapped_io.GetBool()

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int GetSectorSize( const char *pszFilename )
{
	if ( ( !pszFilename[0] || !pszFilename[1] ) ||
		 ( pszFilename[0] == '\\' && pszFilename[1] == '\\' ) ||
		 ( pszFilename[0] == '/' && pszFilename[1] == '/' ) )
	{
		// Cannot determine sector size with a UNC path (need volume identifier)
		return 0;
	}

	if ( IsX360() )
	{
		// purposely dvd centric, which is also the worst case
		return XBOX_DVD_SECTORSIZE;
	}

#if defined( _WIN32 ) && !defined( FILESYSTEM_STEAM ) && !defined( _X360 )
	char szAbsoluteFilename[MAX_FILEPATH];
	if ( pszFilename[1] != ':' )
	{
		Q_MakeAbsolutePath( szAbsoluteFilename, sizeof(szAbsoluteFilename), pszFilename );
		pszFilename = szAbsoluteFilename;
	}

	DWORD sectorSize = 1;

	struct DriveSectorSize_t
	{
		char volume;
		DWORD sectorSize;
	};

	static DriveSectorSize_t cachedSizes[4];

	char volume = tolower( *pszFilename );

	int i;
	for ( i = 0; i < ARRAYSIZE(cachedSizes) && cachedSizes[i].volume; i++ )
	{
		if ( cachedSizes[i].volume == volume )
		{
			sectorSize = cachedSizes[i].sectorSize;
			break;
		}
	}

	if ( sectorSize == 1 )
	{
		char root[4] = "X:\\";
		root[0] = *pszFilename;

		DWORD ignored;
		if ( !GetDiskFreeSpace( root, &ignored, &sectorSize, &ignored, &ignored ) )
		{
			sectorSize = 0;
		}

		if ( i < ARRAYSIZE(cachedSizes) )
		{
			cachedSizes[i].volume = volume;
			cachedSizes[i].sectorSize = sectorSize;
		}
	}

	return sectorSize;
#else
	return 0;
#endif
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

class CThreadIOEventPool
{
public:
	~CThreadIOEventPool()
	{
	}

	CThreadEvent *GetEvent()
	{
		return m_Events.GetObject();
	}

	void ReleaseEvent( CThreadEvent *pEvent )
	{
		m_Events.PutObject( pEvent );
	}

private:
	CTSPool<CThreadEvent> m_Events;
};


CThreadIOEventPool g_ThreadIOEvents;


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CWin32ReadOnlyFile::CanOpen( const char *filename, const char *options )
{
	return ( options[0] == 'r' && options[1] == 'b' && options[2] == 0 && filesystem_native.GetBool() );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

static HANDLE OpenWin32File( const char *filename, bool bOverlapped, bool bUnbuffered, int64 *pFileSize )
{
	HANDLE hFile;

	DWORD createFlags = FILE_ATTRIBUTE_NORMAL;
		
	if ( bOverlapped )
	{
		createFlags |= FILE_FLAG_OVERLAPPED;
	}

	if ( bUnbuffered )
	{
		createFlags |= FILE_FLAG_NO_BUFFERING;
	}

	hFile = ::CreateFile( filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, createFlags, NULL );
	if ( hFile != INVALID_HANDLE_VALUE && !*pFileSize )
	{
		LARGE_INTEGER fileSize;
		if ( !GetFileSizeEx( hFile, &fileSize ) )
		{
			CloseHandle( hFile );
			hFile = INVALID_HANDLE_VALUE;
		}
		*pFileSize = fileSize.QuadPart;
	}
	return hFile;
}

CWin32ReadOnlyFile *CWin32ReadOnlyFile::FS_fopen( const char *filename, const char *options, int64 *size )
{
	Assert( CanOpen( filename, options ) );

	int sectorSize = 0;
	bool bTryUnbuffered = ( UseUnbufferedIO() && ( sectorSize = GetSectorSize( filename ) ) != 0 );
	bool bOverlapped = UseOverlappedIO();

	HANDLE hFileUnbuffered = INVALID_HANDLE_VALUE;
	int64 fileSize = 0;

	if ( bTryUnbuffered )
	{
		hFileUnbuffered = OpenWin32File( filename, bOverlapped, true, &fileSize );
		if ( hFileUnbuffered == INVALID_HANDLE_VALUE )
		{
			return NULL;
		}
	}

	HANDLE hFileBuffered = OpenWin32File( filename, bOverlapped, false, &fileSize );
	if ( hFileBuffered == INVALID_HANDLE_VALUE )
	{
		if ( hFileUnbuffered != INVALID_HANDLE_VALUE )
		{
			CloseHandle( hFileUnbuffered );
		}
		return NULL;
	}

	if ( size )
	{
		*size = fileSize;
	}

	return new CWin32ReadOnlyFile( hFileUnbuffered, hFileBuffered, ( sectorSize ) ? sectorSize : 1, fileSize, bOverlapped );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CWin32ReadOnlyFile::FS_fclose()
{
	if ( m_hFileUnbuffered != INVALID_HANDLE_VALUE )
	{
		CloseHandle( m_hFileUnbuffered );
	}

	if ( m_hFileBuffered != INVALID_HANDLE_VALUE )
	{
		CloseHandle( m_hFileBuffered );
	}
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CWin32ReadOnlyFile::FS_fseek( int64 pos, int seekType )
{
	switch ( seekType )
	{
	case SEEK_SET:
		m_ReadPos = pos;
		break;

	case SEEK_CUR:
		m_ReadPos += pos;
		break;

	case SEEK_END:
		m_ReadPos = m_Size - pos;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
long CWin32ReadOnlyFile::FS_ftell()
{
	return m_ReadPos;	
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CWin32ReadOnlyFile::FS_feof()
{
	return ( m_ReadPos >= m_Size );	
}

// ends up on a thread's stack, don't blindly increase without awareness of that implication
// 360 threads have small stacks, using small buffer of the worst case quantum sector size
#if !defined( _X360 )
#define READ_TEMP_BUFFER	( 32*1024 )
#else
#define READ_TEMP_BUFFER	( 2*XBOX_DVD_SECTORSIZE )
#endif

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CWin32ReadOnlyFile::FS_fread( void *dest, size_t destSize, size_t size )
{
	VPROF_BUDGET( "CWin32ReadOnlyFile::FS_fread", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	if ( !size || ( m_hFileUnbuffered == INVALID_HANDLE_VALUE && m_hFileBuffered == INVALID_HANDLE_VALUE ) )
	{
		return 0;
	}

	CThreadEvent *pEvent = NULL;

	if ( destSize == (size_t)-1 )
	{
		destSize = size;
	}

	byte tempBuffer[READ_TEMP_BUFFER];
	HANDLE hReadFile = m_hFileBuffered;
	int nBytesToRead = size;
	byte *pDest = (byte *)dest;
	int64 offset = m_ReadPos;

	if ( m_hFileUnbuffered != INVALID_HANDLE_VALUE )
	{
		const int destBaseAlign = ( IsX360() ) ? 4 : m_SectorSize;
		bool bDestBaseIsAligned = ( (DWORD)dest % destBaseAlign == 0 );
		bool bCanReadUnbufferedDirect = ( bDestBaseIsAligned && ( destSize % m_SectorSize == 0 ) && ( m_ReadPos % m_SectorSize == 0 ) );

		if ( bCanReadUnbufferedDirect )
		{
			// fastest path, unbuffered
			nBytesToRead = AlignValue( size, m_SectorSize );
			hReadFile = m_hFileUnbuffered;
		}
		else
		{
			// not properly aligned, snap to alignments
			// attempt to perform single unbuffered operation using stack buffer
			int64 alignedOffset = AlignValue( ( m_ReadPos - m_SectorSize ) + 1, m_SectorSize );
			unsigned int alignedBytesToRead = AlignValue( ( m_ReadPos - alignedOffset ) + size, m_SectorSize );
			if ( alignedBytesToRead <= sizeof( tempBuffer ) - destBaseAlign )
			{
				// read operation can be performed as unbuffered follwed by a post fixup
				nBytesToRead = alignedBytesToRead;
				offset = alignedOffset;
				pDest = AlignValue( tempBuffer, destBaseAlign );
				hReadFile = m_hFileUnbuffered;
			}
		}
	}

	OVERLAPPED overlapped = { 0 };	
	if ( m_bOverlapped )
	{
		pEvent = g_ThreadIOEvents.GetEvent();
		overlapped.hEvent = *pEvent;
	}

#ifdef REPORT_BUFFERED_IO
	if ( hReadFile == m_hFileBuffered && filesystem_report_buffered_io.GetBool() )
	{
		Msg( "Buffered Operation :(\n" );
	}
#endif

	// some disk drivers will fail if read is too large
	static int MAX_READ = filesystem_max_stdio_read.GetInt()*1024*1024;
	const int MIN_READ = 64*1024;
	bool bReadOk = true;
	DWORD nBytesRead = 0;
	size_t result = 0;
	int64 currentOffset = offset;

	while ( bReadOk && nBytesToRead > 0 )
	{
		int nCurBytesToRead = min( nBytesToRead, MAX_READ );
		DWORD nCurBytesRead = 0;

		overlapped.Offset = currentOffset & 0xFFFFFFFF;
		overlapped.OffsetHigh = ( currentOffset >> 32 ) & 0xFFFFFFFF;

		bReadOk = ( ::ReadFile( hReadFile, pDest + nBytesRead, nCurBytesToRead, &nCurBytesRead, &overlapped ) != 0 );
		if ( !bReadOk )
		{
			if ( m_bOverlapped && GetLastError() == ERROR_IO_PENDING )
			{
				// Read is pending, we should block until the OS is finished. Otherwise this loop is just a evil spinloop.
				// (Why are we even using asynchronous I/O in this loop?)
				if ( GetOverlappedResult( hReadFile, &overlapped, &nCurBytesRead, TRUE ) )
				{
					bReadOk = true;
				}
			}
		}

		if ( bReadOk )
		{
			nBytesRead += nCurBytesRead;
			nBytesToRead -= nCurBytesToRead;
			currentOffset += nCurBytesRead;
		}
		 
		if ( !bReadOk )
		{
			DWORD dwError = GetLastError();

			if ( IsX360() )
			{
				if ( dwError == ERROR_DISK_CORRUPT || dwError == ERROR_FILE_CORRUPT )
				{
					FSDirtyDiskReportFunc_t func = g_FileSystem_Stdio.GetDirtyDiskReportFunc();
					if ( func )
					{
						func();
						result = 0;
					}
				}
			}

			if ( dwError == ERROR_NO_SYSTEM_RESOURCES && MAX_READ > MIN_READ )
			{
				MAX_READ /= 2;
				bReadOk = true;
				DevMsg( "ERROR_NO_SYSTEM_RESOURCES: Reducing max read to %d bytes\n", MAX_READ );
			}
			else
			{
				DevMsg( "Unknown read error %d\n", dwError );
			}
		}
	}

	if ( bReadOk )
	{
		if ( nBytesRead && hReadFile == m_hFileUnbuffered && pDest != dest )
		{
			int nBytesExtra = ( m_ReadPos - offset );
			nBytesRead -= nBytesExtra;
			if ( nBytesRead )
			{
				memcpy( dest, (byte *)pDest + nBytesExtra, size );
			}
		}

		result = min( nBytesRead, size );
	}

	if ( m_bOverlapped )
	{
		pEvent->Reset();
		g_ThreadIOEvents.ReleaseEvent( pEvent );
	}

	m_ReadPos += result;

	return result;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char *CWin32ReadOnlyFile::FS_fgets( char *dest, int destSize ) 
{  
	if ( FS_feof() )
	{
		return NULL;
	}
	int nStartPos = m_ReadPos;
	int nBytesRead = FS_fread( dest, destSize, destSize );
	if ( !nBytesRead )
	{
		return NULL;
	}

	dest[min( nBytesRead, destSize - 1)] = 0;
	char *pNewline = strchr( dest, '\n' );
	if ( pNewline )
	{
		// advance past, leave \n
		pNewline++;
		*pNewline = 0;
	}
	else
	{
		pNewline = &dest[min( nBytesRead, destSize - 1)];
	}
	m_ReadPos = nStartPos + ( pNewline - dest ) + 1;

	return dest; 
}


#endif

#if IsPlatformPS3()

const char * GetSupportedPrefix()
{
	return g_pPS3PathInfo->GameImagePath();
}

int GetSupportedPrefixLength()
{
	return strlen( GetSupportedPrefix() );
}


bool CFiosReadOnlyFile::CanOpen( const char *filename, const char *options )
{
	if( ShouldFailIo() )
		return false;

	extern ConVar fs_fios_enabled;
	if ( fs_fios_enabled.GetBool() )
	{
		bool bSupported = ( options[0] == 'r' && options[1] == 'b' && options[2] == 0 && filesystem_native.GetBool() );
		bSupported &= ( memcmp( filename, GetSupportedPrefix(), GetSupportedPrefixLength() ) == 0 );
		return bSupported;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

static cell::fios::filehandle * OpenFiosFile( const char *filename, int64 *pFileSize )
{
	if( ShouldFailIo() )
		return NULL;

	cell::fios::scheduler * pScheduler = cell::fios::scheduler::getDefaultScheduler();

	cell::fios::filehandle * pFileHandle;
	cell::fios::err_t err;

	Assert( memcmp( filename, GetSupportedPrefix(), GetSupportedPrefixLength() ) == 0);
	filename += GetSupportedPrefixLength();		// Skip the prefix, FIOS already takes it in account

	err = pScheduler->getFileSizeSync( NULL, filename, pFileSize );
	if ( err != cell::fios::CELL_FIOS_NOERROR )
	{
		Warning( "[FIOS] Failed to get size of file '%s'.\n", filename );
		return NULL;
	}

	err = pScheduler->openFileSync( NULL, filename, cell::fios::kO_RDONLY, &pFileHandle );
	if ( err != cell::fios::CELL_FIOS_NOERROR )
	{
		Warning( "[FIOS] Failed to open file '%s'.\n", filename );
		return NULL;
	}

	return pFileHandle;
}

CFiosReadOnlyFile * CFiosReadOnlyFile::FS_fopen( const char *filename, const char *options, int64 *size )
{
	if( ShouldFailIo() )
		return NULL;

	Assert( CanOpen( filename, options ) );

	int64 nFileSize;
	cell::fios::filehandle * pFileHandle = OpenFiosFile( filename, &nFileSize );
	if ( pFileHandle == NULL )
	{
		return NULL;
	}

	if ( size )
	{
		*size = nFileSize;
	}

	return new CFiosReadOnlyFile( pFileHandle, nFileSize );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFiosReadOnlyFile::FS_fclose()
{
	if ( m_pHandle != NULL )
	{
		cell::fios::err_t err = cell::fios::scheduler::getDefaultScheduler()->closeFileSync( NULL, m_pHandle );
		if ( err != cell::fios::CELL_FIOS_NOERROR )
		{
			Warning( "[FIOS] Failed to close file.\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFiosReadOnlyFile::FS_fseek( int64 pos, int seekType )
{
	switch ( seekType )
	{
	case SEEK_SET:
		m_nReadPos = pos;
		break;

	case SEEK_CUR:
		m_nReadPos += pos;
		break;

	case SEEK_END:
		m_nReadPos = m_nSize - pos;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
long CFiosReadOnlyFile::FS_ftell()
{
	return m_nReadPos;	
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFiosReadOnlyFile::FS_feof()
{
	return ( m_nReadPos >= m_nSize );	
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------

// Set the flag to true if any of the op is processing.
void IsAnyOpProcessing( void *pContext, cell::fios::op *pOp )
{
	bool * pBool = ( bool * )pContext;
	bool bFinished = pOp->isDone() || pOp->isCancelled();
	*pBool |= ( bFinished == false );		// Mark the ops that are still working
}

size_t CFiosReadOnlyFile::FS_fread( void *dest, size_t destSize, size_t size )
{
	VPROF_BUDGET( "CFiosReadOnlyFile::FS_fread", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	if( ShouldFailIo() )
		return 0;

	if ( size == 0 )
	{
		return 0;
	}

	cell::fios::opattr_t opattr = FIOS_OPATTR_INITIALIZER;
	// The user can disable usage of HDD cache with the ConVar fs_fios_enable_hdd_cache.
	// However in some case, the game will disable its usage temporarily too (with g_bUseFiosHddCache).
	// This can happen if the game is saving. We want to avoid the save system and FIOS to compete for the HDD usage.
	// In that case, IO accesses will be done on the BluRay. It is only temporary (few seconds), and most data should be in memory anyway.
	// We just want to avoid cases where a single data takes several seconds to load.
	if ( fs_fios_enable_hdd_cache.GetBool() && ( g_bUseFiosHddCache == false ) )
	{
		// Display a message so we can detect prolonged incorrect state.
		static uint32 nLastSpew = 0;
		const int SPEW_EVERY_N_MILLISECONDS = 5 * 1000;		// Don't need to spew too much. Every 5 seconds is enough for us to detect potential issue.

		uint32 nCurrentTime = Plat_MSTime();
		if ( nCurrentTime > nLastSpew + SPEW_EVERY_N_MILLISECONDS )
		{
			Msg( "Fios HDD accesses disabled as a save is occurring.\n" );
			nLastSpew = nCurrentTime;
		}
	}

	cell::fios::scheduler *pScheduler = cell::fios::scheduler::getDefaultScheduler();
	uint32_t opFlags = cell::fios::kOPF_DONTFILLDISKCACHE;			// By default, full cache usage

	// Again another FIOS function "pScheduler->isIdle()" does not work as expected. Implement another work around.
	bool bWorkingOps = false;
	pScheduler->iterateOps( &IsAnyOpProcessing, &bWorkingOps );
	if ( bWorkingOps )
	{
		// It is not idle, it is probably prefetching or doing something else. Let's reduce the HDD usage (read but don't write).
		// If the data is really important, it will be cached later when the scheduler is idle.
		opFlags = cell::fios::kOPF_DONTFILLCACHE;
	}
	opattr.opflags = ( fs_fios_enable_hdd_cache.GetBool() && g_bUseFiosHddCache ) ? opFlags : cell::fios::kOPF_NOCACHE;
	opattr.deadline = kDEADLINE_NOW;					// Consider using kDEADLINE_ASAP
														// By using ASAP, the hope is that FIOS will schedule the read in the best manner to reduce seeks
														// NOW could help serve this read better but could reduce overall performance.
														// We use NOW, as the non-persistent prefetches are ASAP
														// And persistent prefetches are LATER (the priority doesn't really apply between prefetches otherwise).
	opattr.priority = cell::fios::kPRIO_DEFAULT;
	opattr.pCallback = 0;
	opattr.opflags = 0;
	opattr.pLicense = 0;

	cell::fios::err_t err = pScheduler->readFileSync( &opattr, m_pHandle, dest, size, m_nReadPos );
	if ( err != cell::fios::CELL_FIOS_NOERROR )
	{
		return 0;
	}
	m_nReadPos += size;
	return size;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char *CFiosReadOnlyFile::FS_fgets( char *dest, int destSize ) 
{  
	if( ShouldFailIo() )
		return NULL;

	if ( FS_feof() )
	{
		return NULL;
	}
	int nStartPos = m_nReadPos;
	int nBytesRead = FS_fread( dest, destSize, destSize );
	if ( !nBytesRead )
	{
		return NULL;
	}

	dest[imin( nBytesRead, destSize - 1)] = 0;
	char *pNewline = strchr( dest, '\n' );
	if ( pNewline )
	{
		// advance past, leave \n
		pNewline++;
		*pNewline = 0;
	}
	else
	{
		pNewline = &dest[imin( nBytesRead, destSize - 1)];
	}
	m_nReadPos = nStartPos + ( pNewline - dest ) + 1;

	return dest; 
}


#endif


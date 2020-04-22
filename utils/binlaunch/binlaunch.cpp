//================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
//
//
//==================================================================================================

#ifdef _WIN32
#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <io.h>
#else
#include <stdarg.h>
#include <dlfcn.h>
#endif
#include <stdio.h>
#include "tier0/platform.h"
#include "tier0/basetypes.h"
#include "ilaunchabledll.h"


#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')

#ifdef PLATFORM_WINDOWS
	#pragma warning(disable : 4127)
	#define CORRECT_PATH_SEPARATOR_S "\\"
	#define CORRECT_PATH_SEPARATOR '\\'
	#define INCORRECT_PATH_SEPARATOR '/'
#else
	#define CORRECT_PATH_SEPARATOR '/'
	#define CORRECT_PATH_SEPARATOR_S "/"
	#define INCORRECT_PATH_SEPARATOR '\\'
#endif

#undef stricmp

#ifdef COMPILER_MSVC
	#define V_stricmp stricmp
#else
	#define V_stricmp strcasecmp
#endif

#define CREATEINTERFACE_PROCNAME	"CreateInterface"
typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);


static void V_strncpy( char *pDest, char const *pSrc, int maxLen )
{
	strncpy( pDest, pSrc, maxLen );
	if ( maxLen > 0 )
	{
		pDest[maxLen-1] = 0;
	}
}

static int V_strlen( const char *pStr )
{
	return (int)strlen( pStr );
}

static void V_strncat( char *pDest, const char *pSrc, int destSize )
{
	strncat( pDest, pSrc, destSize );
	pDest[destSize-1] = 0;
}

static void V_AppendSlash( char *pStr, int strSize )
{
	int len = V_strlen( pStr );
	if ( len > 0 && !PATHSEPARATOR(pStr[len-1]) )
	{
		if ( len+1 >= strSize )
		{
			fprintf( stderr, "V_AppendSlash: ran out of space on %s.", pStr );
			exit( 1 );
		}
		
		pStr[len] = CORRECT_PATH_SEPARATOR;
		pStr[len+1] = 0;
	}
}

static void V_FixSlashes( char *pStr )
{
	for ( ; *pStr; ++pStr )
	{
		if ( *pStr == INCORRECT_PATH_SEPARATOR )
			*pStr = CORRECT_PATH_SEPARATOR;
	}
}

static void V_ComposeFileName( const char *path, const char *filename, char *dest, int destSize )
{
	V_strncpy( dest, path, destSize );
	V_AppendSlash( dest, destSize );
	V_strncat( dest, filename, destSize );
	V_FixSlashes( dest );
}

static int V_snprintf( char *pDest, int maxLen, const char *pFormat, ... )
{
	va_list marker;

	va_start( marker, pFormat );
#ifdef _WIN32
	int len = _vsnprintf( pDest, maxLen, pFormat, marker );
#elif POSIX
	int len = vsnprintf( pDest, maxLen, pFormat, marker );
#else
	#error "define vsnprintf type."
#endif
	va_end( marker );
	
	// Len < 0 represents an overflow
	if( len < 0 )
	{
		len = maxLen;
		pDest[maxLen-1] = 0;
	}

	return len;
}

static bool V_StripLastDir( char *dirName, int maxlen )
{
	if( dirName[0] == 0 || !V_stricmp( dirName, "./" ) || !V_stricmp( dirName, ".\\" ) )
	{
		return false;
	}
	
	int len = V_strlen( dirName );

	// skip trailing slash
	if ( PATHSEPARATOR( dirName[len-1] ) )
	{
		len--;
	}

	while ( len > 0 )
	{
		if ( PATHSEPARATOR( dirName[len-1] ) )
		{
			dirName[len] = 0;
			V_FixSlashes( dirName );
			return true;
		}
		len--;
	}

	// Allow it to return an empty string and true. This can happen if something like "tf2/" is passed in.
	// The correct behavior is to strip off the last directory ("tf2") and return true.
	if( len == 0 )
	{
		V_snprintf( dirName, maxlen, ".%c", CORRECT_PATH_SEPARATOR );
		return true;
	}

	return true;
}

#ifdef _WIN32

typedef struct _REPARSE_DATA_BUFFER {
  ULONG  ReparseTag;
  USHORT  ReparseDataLength;
  USHORT  Reserved;
  union {
    struct {
      USHORT  SubstituteNameOffset;
      USHORT  SubstituteNameLength;
      USHORT  PrintNameOffset;
      USHORT  PrintNameLength;
      ULONG  Flags;
      WCHAR  PathBuffer[1];
      } SymbolicLinkReparseBuffer;
    struct {
      USHORT  SubstituteNameOffset;
      USHORT  SubstituteNameLength;
      USHORT  PrintNameOffset;
      USHORT  PrintNameLength;
      WCHAR  PathBuffer[1];
      } MountPointReparseBuffer;
    struct {
      UCHAR  DataBuffer[1];
    } GenericReparseBuffer;
  };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE  ( 16 * 1024 )
#define IO_REPARSE_TAG_SYMLINK 0xa0000003

void TranslateSymlink( const char *pInDir, char *pOutDir, int len )
{
	// This is the default. If it's a reparse point, it'll get replaced below.
	V_strncpy( pOutDir, pInDir, len );

	// The equivalent of symlinks in Win32 is "NTFS reparse points".
	DWORD nAttribs = GetFileAttributes( pInDir );
	if ( nAttribs & FILE_ATTRIBUTE_REPARSE_POINT )
	{
		HANDLE hDir = CreateFile( pInDir, FILE_READ_EA, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL );
		if ( hDir )
		{
			DWORD dwBufSize = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
			REPARSE_DATA_BUFFER *pReparseData = (REPARSE_DATA_BUFFER*)malloc( dwBufSize );
  
			DWORD nBytesReturned = 0;
			BOOL bSuccess = DeviceIoControl( hDir, FSCTL_GET_REPARSE_POINT, NULL, 0, pReparseData, dwBufSize, &nBytesReturned, NULL );
			CloseHandle( hDir );

			if ( bSuccess )
			{
				if ( IsReparseTagMicrosoft( pReparseData->ReparseTag ) )
				{
					if ( pReparseData->ReparseTag == IO_REPARSE_TAG_SYMLINK )
					{
						REPARSE_DATA_BUFFER *rdata = pReparseData;

						// Pull out the substitution name.
						char szSubName[MAX_PATH*2];
						wchar_t *pSrcString = &rdata->SymbolicLinkReparseBuffer.PathBuffer[rdata->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR)];
						size_t nConvertedChars;
						wcstombs_s( &nConvertedChars, szSubName, wcslen( pSrcString ) + 1, pSrcString, _TRUNCATE );

						// Look for the drive letter and start there.
						const char *pColon = strchr( szSubName, ':' );
						if ( pColon && pColon > szSubName )
						{
							const char *pRemappedName = ( pColon - 1 );
							V_strncpy( pOutDir, pRemappedName, len );
						}
					}
				}
			}

			free( pReparseData );
		}
		else
		{
			printf( "Warning: Found a reparse point (ntfs symlink) for %s but CreateFile failed\n", pInDir );
		}
	}
}

#endif

int main( int argc, char **argv )
{
	// Find the game\bin directory and setup the DLL path.
	char szModuleFilename[MAX_PATH], szModuleParts[MAX_PATH], szCurDir[MAX_PATH];
#ifdef WIN32
	GetModuleFileName( NULL, szModuleFilename, sizeof( szModuleFilename ) );
	V_FixSlashes( szModuleFilename );
#else
	V_strncpy( szModuleFilename, argv[0], sizeof(szModuleFilename) );
#endif
	
	V_strncpy( szModuleParts, szModuleFilename, sizeof( szModuleParts ) );
	char *pFilename = strrchr( szModuleParts, CORRECT_PATH_SEPARATOR );
	if ( !pFilename )
	{
		fprintf( stderr, "%s (binlaunch): Can't get filename from GetModuleFilename (%s).\n", argv[0], szModuleFilename );
		return 1;
	}

	*pFilename = 0;
	++pFilename;
	const char *pBaseDir = szModuleParts;


#ifdef WIN32
	TranslateSymlink( pBaseDir, szCurDir, sizeof( szCurDir ) );
#else
	V_strncpy( szCurDir, pBaseDir, sizeof(szCurDir) );
#endif
	
	char szGameBinDir[MAX_PATH];
	while ( 1 )
	{
		V_ComposeFileName( szCurDir, "game" CORRECT_PATH_SEPARATOR_S "bin", szGameBinDir, sizeof( szGameBinDir ) );

		// Look for stuff we know about in game\bin.
		char szTestFile1[MAX_PATH], szTestFile2[MAX_PATH];
		V_ComposeFileName( szGameBinDir, "tier0.dll", szTestFile1, sizeof( szTestFile1 ) );
		V_ComposeFileName( szGameBinDir, "vstdlib.dll", szTestFile2, sizeof( szTestFile2 ) );
		if ( _access( szTestFile1, 0 ) == 0 && _access( szTestFile2, 0 ) == 0 )
		{
			break;
		}

		// Backup a directory.
		if ( !V_StripLastDir( szCurDir, sizeof( szCurDir ) ) )
		{
			fprintf( stderr, "%s (binlaunch): Unable to find game\\bin directory anywhere up the tree from %s.\n", argv[0], pBaseDir );
			return 1;
		}
	}


	// Setup the path to include the specified directory.
	int nGameBinDirLen = V_strlen( szGameBinDir );
	char *pOldPath = getenv( "PATH" );
	int nNewLen = V_strlen( pOldPath ) + nGameBinDirLen + 5 + 1 + 1; // 5 for PATH=, 1 for the semicolon, and 1 for the null terminator.
	char *pNewPath = new char[nNewLen];
	V_snprintf( pNewPath, nNewLen, "PATH=%s;%s", szGameBinDir, pOldPath );
	_putenv( pNewPath );
	delete [] pNewPath;


	// Get rid of the file extension on our executable name.
#ifdef WIN32
	char *pDot = strchr( &szModuleFilename[pFilename-szModuleParts], '.' );
	if ( !pDot )
	{
		fprintf( stderr, "%s (binlaunch): No dot character in the filename.\n", argv[0] );
		return 1;
	}
	*pDot = 0;
#endif
	
	char szDLLName[MAX_PATH];
	V_snprintf( szDLLName, sizeof( szDLLName ), "%s%s", szModuleFilename, DLL_EXT_STRING );

	//
	// Now go load their DLL and launch it.
	//
#ifdef WIN32
	HMODULE hModule = LoadLibrary( szDLLName );
#else
	HMODULE hModule = dlopen( szDLLName, RTLD_NOW );	
#endif
	if ( !hModule )
	{
		fprintf( stderr, "%s (binlaunch): Unable to load module %s\n\n", argv[0], szDLLName );
		return 9998;
	}

	CreateInterfaceFn fn = (CreateInterfaceFn)GetProcAddress( hModule, CREATEINTERFACE_PROCNAME );
	ILaunchableDLL *pLaunchable;
	if ( !fn )
	{
		fprintf( stderr, "%s (binlaunch): Can't get function %s from %s\n\n", argv[0], CREATEINTERFACE_PROCNAME, szDLLName );
		return 9997;
	}

	pLaunchable = (ILaunchableDLL*)fn( LAUNCHABLE_DLL_INTERFACE_VERSION, NULL );
	if ( !pLaunchable )
	{
		fprintf( stderr, "%s (binlaunch): Can't get interface %s from from %s\n\n", argv[0], LAUNCHABLE_DLL_INTERFACE_VERSION, szDLLName );
		return 9996;
	}

	return pLaunchable->main( argc, argv );
}



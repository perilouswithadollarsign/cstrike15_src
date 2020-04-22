//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// -----------------------
// cmdlib.c
// -----------------------
#include "tier0/platform.h"
#ifdef IS_WINDOWS_PC
#include <windows.h>
#endif
#include "cmdlib.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "tier1/strtools.h"
#ifdef _WIN32
#include <conio.h>
#endif
#include "utlvector.h"
#include "filesystem_helpers.h"
#include "utllinkedlist.h"
#include "tier0/icommandline.h"
#include "keyvalues.h"
#include "filesystem_tools.h"

#if defined( MPI )

	#include "vmpi.h"
	#include "vmpi_tools_shared.h"

#endif


#if defined( _WIN32 ) || defined( WIN32 )
#include <direct.h>
#endif

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// set these before calling CheckParm
int myargc;
char **myargv;

int newdirs = 0;

char		com_token[ 1024 ];

qboolean	archive;
char		archivedir[ 1024 ];



CUtlLinkedList<CleanupFn, unsigned short> g_CleanupFunctions;

bool g_bStopOnExit = false;

#if defined( _WIN32 ) || defined( WIN32 )

void CmdLib_FPrintf( FileHandle_t hFile, const char *pFormat, ... )
{
	static CUtlVector<char> buf;
	if ( buf.Count() == 0 )
	{
		buf.SetCount( 1024 );
	}

	va_list marker;
	va_start( marker, pFormat );
	
	while ( 1 )
	{
		int ret = Q_vsnprintf( buf.Base(), buf.Count(), pFormat, marker );
		if ( ret >= 0 )
		{
			// Write the string.
			g_pFileSystem->Write( buf.Base(), ret, hFile );
			
			break;
		}
		else
		{
			// Make the buffer larger.
			int newSize = buf.Count() * 2;
			buf.SetCount( newSize );
			if ( buf.Count() != newSize )
			{
				Error( "CmdLib_FPrintf: can't allocate space for text." );
			}
		}
	}

	va_end( marker );
}

char* CmdLib_FGets( char *pOut, int outSize, FileHandle_t hFile )
{
	int iCur=0;
	for ( ; iCur < (outSize-1); iCur++ )
	{
		char c;
		if ( !g_pFileSystem->Read( &c, 1, hFile ) )
		{
			if ( iCur == 0 )
				return NULL;
			else
				break;
		}

		pOut[iCur] = c;
		if ( c == '\n' )
			break;

		if ( c == EOF )
		{
			if ( iCur == 0 )
				return NULL;
			else
				break;
		}
	}

	pOut[iCur] = 0;
	return pOut;
}

#if !defined( _X360 )
#include <wincon.h>
#endif

// This pauses before exiting if they use -StopOnExit. Useful for debugging.
class CExitStopper
{
public:
	~CExitStopper()
	{
		if ( g_bStopOnExit )
		{
			Warning( "\nPress any key to quit.\n" );
			getch();
		}
	}
} g_ExitStopper;


static unsigned short g_InitialColor = 0xFFFF;
static unsigned short g_LastColor = 0xFFFF;
static unsigned short g_BadColor = 0xFFFF;
static WORD g_BackgroundFlags = 0xFFFF;
static void GetInitialColors( )
{
#if !defined( _X360 )
	// Get the old background attributes.
	CONSOLE_SCREEN_BUFFER_INFO oldInfo;
	GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &oldInfo );
	g_InitialColor = g_LastColor = oldInfo.wAttributes & ( FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY );
	g_BackgroundFlags = oldInfo.wAttributes & ( BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE|BACKGROUND_INTENSITY );

	g_BadColor = 0;
	if (g_BackgroundFlags & BACKGROUND_RED)
	{
		g_BadColor |= FOREGROUND_RED;
	}
	if (g_BackgroundFlags & BACKGROUND_GREEN)
	{
		g_BadColor |= FOREGROUND_GREEN;
	}
	if (g_BackgroundFlags & BACKGROUND_BLUE)
	{
		g_BadColor |= FOREGROUND_BLUE;
	}
	if (g_BackgroundFlags & BACKGROUND_INTENSITY)
	{
		g_BadColor |= FOREGROUND_INTENSITY;
	}
#endif
}

WORD SetConsoleTextColor( int red, int green, int blue, int intensity )
{
	WORD ret = g_LastColor;
#if !defined( _X360 )
	
	g_LastColor = 0;
	if( red )
	{
		g_LastColor |= FOREGROUND_RED;
	}
	if( green )
	{
		g_LastColor |= FOREGROUND_GREEN;
	}
	if( blue )
	{
		g_LastColor |= FOREGROUND_BLUE;
	}
	if( intensity )
	{
		g_LastColor |= FOREGROUND_INTENSITY;
	}

	// Just use the initial color if there's a match...
	if (g_LastColor == g_BadColor)
	{
		g_LastColor = g_InitialColor;
	}

	SetConsoleTextAttribute( GetStdHandle( STD_OUTPUT_HANDLE ), g_LastColor | g_BackgroundFlags );
#endif
	return ret;
}

void RestoreConsoleTextColor( WORD color )
{
#if !defined( _X360 )
	SetConsoleTextAttribute( GetStdHandle( STD_OUTPUT_HANDLE ), color | g_BackgroundFlags );
	g_LastColor = color;
#endif
}


#if defined( CMDLIB_NODBGLIB )

// This can go away when everything is in bin.
void Error( char const *pMsg, ... )
{
	va_list marker;
	va_start( marker, pMsg );
	vprintf( pMsg, marker );
	va_end( marker );

	exit( -1 );
}

#else

bool g_bSuppressPrintfOutput = false;

void CCmdLibStandardLoggingListener::Log( const LoggingContext_t *pContext, const tchar *pMessage )
{
	if ( ( pContext->m_Flags & LCF_DO_NOT_ECHO ) != 0 )
	{
		return;
	}

	WORD oldColor;
	Color spewColor = pContext->m_Color;
	if ( spewColor == UNSPECIFIED_LOGGING_COLOR )
	{
		switch ( pContext->m_Severity )
		{
		case LS_MESSAGE:
			spewColor = Color( 255, 255, 255, 0 );
			break;

		case LS_WARNING:
			spewColor = Color( 255, 255, 0, 255	);
			break;

		case LS_ERROR:
		case LS_ASSERT:
			spewColor = Color( 255, 0, 0, 255	);
			break;
		}
	}
	oldColor = SetConsoleTextColor( spewColor.r(), spewColor.g(), spewColor.b(), spewColor.a() );
	
#ifdef MPI
	if ( pContext->m_Severity == LS_ASSERT )
	{
		// VMPI workers don't want to bring up dialogs and suchlike.
		// They need to have a special function installed to handle
		// the exceptions and write the minidumps.
		// Install the function after VMPI_Init with a call:
		// SetupToolsMinidumpHandler( VMPI_ExceptionFilter );
		if ( g_bUseMPI && !g_bMPIMaster && !Plat_IsInDebugSession() )
		{
			// Generating an exception and letting the
			// installed handler handle it
			::RaiseException
				(
				0,							// dwExceptionCode
				EXCEPTION_NONCONTINUABLE,	// dwExceptionFlags
				0,							// nNumberOfArguments,
				NULL						// const ULONG_PTR* lpArguments
				);

			// Never get here (non-continuable exception)

			VMPI_HandleCrash( pMessage, 0, NULL, true );
			exit( 0 );
		}
	}
#endif

	if ( !g_bSuppressPrintfOutput || pContext->m_Severity == LS_ERROR )
	{
		printf( "%s", pMessage );
	}

	OutputDebugString( pMessage );

	if ( pContext->m_Severity == LS_ERROR )
	{
		if ( !g_bSuppressPrintfOutput )
		{
			printf( "\n" );
		}
		OutputDebugString( "\n" );
	}

	RestoreConsoleTextColor( oldColor );
}

CCmdLibFileLoggingListener::CCmdLibFileLoggingListener() : m_pLogFile( FILESYSTEM_INVALID_HANDLE ) { }

void CCmdLibFileLoggingListener::Log( const LoggingContext_t *pContext, const tchar *pMessage )
{
	if( m_pLogFile != FILESYSTEM_INVALID_HANDLE && ( pContext->m_Flags & LCF_CONSOLE_ONLY ) == 0 )
	{
		CmdLib_FPrintf( m_pLogFile, "%s", pMessage );
		g_pFileSystem->Flush( m_pLogFile );
	}
}

void CCmdLibFileLoggingListener::Open( char const *pFilename )
{
	Assert( m_pLogFile == FILESYSTEM_INVALID_HANDLE );
	m_pLogFile = g_pFileSystem->Open( pFilename, "a" );

	Assert( m_pLogFile != FILESYSTEM_INVALID_HANDLE );
	if ( !m_pLogFile )
	{
		Error( "Can't create LogFile:\"%s\"\n", pFilename );
	}

	CmdLib_FPrintf( m_pLogFile, "\n\n\n" );
}


void CCmdLibFileLoggingListener::Close()
{
	if ( g_pFileSystem && m_pLogFile != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close( m_pLogFile );
		m_pLogFile = FILESYSTEM_INVALID_HANDLE;
	}
}

CCmdLibStandardLoggingListener g_CmdLibOutputLoggingListener;
CCmdLibFileLoggingListener g_CmdLibFileLoggingListener;
bool g_bInstalledSpewFunction = false;

void InstallSpewFunction()
{
	Assert( !g_bInstalledSpewFunction );
	if ( !g_bInstalledSpewFunction )
	{
		g_bInstalledSpewFunction = true;
		setvbuf( stdout, NULL, _IONBF, 0 );
		setvbuf( stderr, NULL, _IONBF, 0 );

		LoggingSystem_PushLoggingState();
		LoggingSystem_RegisterLoggingListener( &g_CmdLibOutputLoggingListener );
		LoggingSystem_RegisterLoggingListener( &g_CmdLibFileLoggingListener );
		GetInitialColors();
	}
}

void CmdLib_AllocError( unsigned long size )
{
	Error( "Error trying to allocate %d bytes.\n", size );
}


int CmdLib_NewHandler( size_t size )
{
	CmdLib_AllocError( size );
	return 0;
}

void InstallAllocationFunctions()
{
	_set_new_mode( 1 ); // so if malloc() fails, we exit.
	_set_new_handler( CmdLib_NewHandler );
}
#endif

void CmdLib_AtCleanup( CleanupFn pFn )
{
	g_CleanupFunctions.AddToTail( pFn );
}


void CmdLib_Cleanup()
{
	if ( g_bInstalledSpewFunction )
	{
		LoggingSystem_PopLoggingState();
	}

	g_CmdLibFileLoggingListener.Close();

	CmdLib_TermFileSystem();

	FOR_EACH_LL( g_CleanupFunctions, i )
	{
		g_CleanupFunctions[ i ]();
	}

#if defined( MPI )
	// Unfortunately, when you call exit(), even if you have things registered with atexit(),
	// threads go into a seemingly undefined state where GetExitCodeThread gives STILL_ACTIVE
	// and WaitForSingleObject will stall forever on the thread. Because of this, we must cleanup
	// everything that uses threads before exiting.
	VMPI_Finalize();
#endif
}



#endif




/*
===================
ExpandWildcards

Mimic unix command line expansion
===================
*/
#define	MAX_EX_ARGC	1024
int		ex_argc;
char	*ex_argv[ MAX_EX_ARGC ];
#if defined( _WIN32 ) && !defined( _X360 )
#include "io.h"
void ExpandWildcards( int *argc, char ***argv )
{
	struct _finddata_t fileinfo;
	int		handle;
	int		i;
	char	filename[ 1024 ];
	char	filebase[ 1024 ];
	char	*path;

	ex_argc = 0;
	for ( i = 0; i < *argc; i++ )
	{
		path = (*argv)[i];
		if ( path[0] == '-'
			|| ( !strstr( path, "*" ) && !strstr( path, "?" ) ) )
		{
			ex_argv[ ex_argc++ ] = path;
			continue;
		}

		handle = _findfirst( path, &fileinfo );
		if ( handle == -1 )
			return;

		Q_ExtractFilePath( path, filebase, sizeof( filebase ) );

		do
		{
			V_sprintf_safe( filename, "%s%s", filebase, fileinfo.name );
			ex_argv[ ex_argc++ ] = copystring( filename );
		} while ( _findnext( handle, &fileinfo ) != -1 );

		_findclose (handle);
	}

	*argc = ex_argc;
	*argv = ex_argv;
}
#else
void ExpandWildcards( int *argc, char ***argv )
{
}
#endif


// only printf if in verbose mode
qboolean verbose = false;
void qprintf( char *format, ... )
{
	if ( !verbose )
		return;

	va_list argptr;
	va_start( argptr, format );

	char str[ 2048 ];
	V_vsprintf_safe( str, format, argptr );

#if defined( CMDLIB_NODBGLIB )
	printf( "%s", str );
#else
	Msg( "%s", str );
#endif

	va_end( argptr );
}


// ---------------------------------------------------------------------------------------------------- //
// Helpers.
// ---------------------------------------------------------------------------------------------------- //

static void CmdLib_getwd( char *out, int outSize )
{
#if defined( _WIN32 ) || defined( WIN32 )
	_getcwd( out, outSize );
	Q_strncat( out, "\\", outSize, COPY_ALL_CHARACTERS );
#else
	getwd( out );
	strcat( out, "/" );
#endif
	Q_FixSlashes( out );
}

char *ExpandArg( char *path )
{
	static char full[ 1024 ];

	if ( path[ 0 ] != '/' && path[ 0 ] != '\\' && path[ 1 ] != ':' )
	{
		CmdLib_getwd ( full, sizeof( full ) );
		V_strcat_safe( full, path, COPY_ALL_CHARACTERS );
	}
	else
	{
		V_strcpy_safe( full, path );
	}
	return full;
}


char *ExpandPath (char *path)
{
	static char full[ 1024 ];
	if ( path[ 0 ] == '/' || path[ 0 ] == '\\' || path[ 1 ] == ':')
		return path;
	V_sprintf_safe( full, "%s%s", qdir, path );
	return full;
}



char *copystring(const char *s)
{
	char	*b;
	b = ( char * )malloc( strlen( s ) + 1 );
	V_strcpy( b, s );
	return b;
}


void GetHourMinuteSeconds( int nInputSeconds, int &nHours, int &nMinutes, int &nSeconds )
{
}


void GetHourMinuteSecondsString( int nInputSeconds, char *pOut, int outLen )
{
	int nMinutes = nInputSeconds / 60;
	int nSeconds = nInputSeconds - nMinutes * 60;
	int nHours = nMinutes / 60;
	nMinutes -= nHours * 60;

	char *extra[ 2 ] = { "", "s" };
	
	if ( nHours > 0 )
	{
		Q_snprintf( pOut, outLen, "%d hour%s, %d minute%s, %d second%s", nHours, extra[ nHours != 1 ], nMinutes, extra[ nMinutes != 1 ], nSeconds, extra[ nSeconds != 1 ] );
	}
	else if ( nMinutes > 0 )
	{
		Q_snprintf( pOut, outLen, "%d minute%s, %d second%s", nMinutes, extra[ nMinutes != 1 ], nSeconds, extra[ nSeconds != 1 ] );
	}
	else
	{
		Q_snprintf( pOut, outLen, "%d second%s", nSeconds, extra[ nSeconds != 1 ] );
	}
}


void Q_mkdir( char *path )
{
#if defined( _WIN32 ) || defined( WIN32 )
	if ( _mkdir( path ) != -1)
		return;
#else
	if ( mkdir( path, 0777 ) != -1)
		return;
#endif
//	if (errno != EEXIST)
	Error( "mkdir failed %s\n", path );
}

void CmdLib_InitFileSystem( const char *pFilename, int maxMemoryUsage )
{
	FileSystem_Init( pFilename, maxMemoryUsage );
	if ( !g_pFileSystem )
	{
		Error( "CmdLib_InitFileSystem failed." );
	}
}

void CmdLib_TermFileSystem()
{
	FileSystem_Term();
}

CreateInterfaceFn CmdLib_GetFileSystemFactory()
{
	return FileSystem_GetFactory();
}


/*
============
FileTime

returns -1 if not present
============
*/
int	FileTime( char *path )
{
	struct	stat	buf;
	
	if ( stat( path, &buf ) == -1 )
		return -1;
	
	return buf.st_mtime;
}



/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse( char *data )
{
	return ( char* )ParseFile( data, com_token, NULL );
}


/*
=============================================================================

						MISC FUNCTIONS

=============================================================================
*/


/*
=================
CheckParm

Checks for the given parameter in the program's command line arguments
Returns the argument number (1 to argc-1) or 0 if not present
=================
*/
int CheckParm( char *check )
{
	int i;

	for ( i = 1; i < myargc; i++ )
	{
		if ( !Q_strcasecmp( check, myargv[ i ] ) )
			return i;
	}

	return 0;
}



/*
================
Q_filelength
================
*/
int Q_filelength( FileHandle_t f )
{
	return g_pFileSystem->Size( f );
}


FileHandle_t SafeOpenWrite( const char *filename )
{
	FileHandle_t f = g_pFileSystem->Open( filename, "wb" );

	if ( !f )
	{
		//Error( "Error opening %s: %s", filename, strerror( errno ) );
		// BUGBUG: No way to get equivalent of errno from IFileSystem!
		Error( "Error opening %s! (Check for write enable)\n", filename );
	}

	return f;
}

#define MAX_CMDLIB_BASE_PATHS 20
static char g_pBasePaths[ MAX_CMDLIB_BASE_PATHS ][ MAX_PATH ];
static int g_NumBasePaths = 0;

void CmdLib_AddBasePath( const char *pPath )			 
{
	//printf( "CmdLib_AddBasePath( \"%s\" )\n", pPath );
	if( g_NumBasePaths < MAX_CMDLIB_BASE_PATHS )
	{
		V_strcpy_safe( g_pBasePaths[ g_NumBasePaths ], pPath );
		Q_FixSlashes( g_pBasePaths[ g_NumBasePaths ] );
		g_NumBasePaths++;
	}
	else
	{
		Assert( 0 );
	}
}


void CmdLib_AddNewSearchPath( const char *pPath )
{
	static int g_nAdditionalDirectoryCount = 0;
	static char s_addedRelativeDirs[ MAX_CMDLIB_BASE_PATHS ][ MAX_PATH ];
   	static int s_originalNumBasePaths = 0;
    static char s_originalBasePaths[ MAX_CMDLIB_BASE_PATHS ][ MAX_PATH ];

	if ( g_nAdditionalDirectoryCount == 0 ) // first call: 
	{
		// remember all original paths
	   s_originalNumBasePaths = g_NumBasePaths;
	   for ( int nOriginalBasePath = 0; nOriginalBasePath < s_originalNumBasePaths; nOriginalBasePath++ )
	   {
			V_strcpy_safe( s_originalBasePaths[nOriginalBasePath], g_pBasePaths[nOriginalBasePath] );
	   }
	}
    
	if( g_nAdditionalDirectoryCount < MAX_CMDLIB_BASE_PATHS )
	{
		V_strcpy_safe( s_addedRelativeDirs[g_nAdditionalDirectoryCount], pPath );
		Q_FixSlashes( s_addedRelativeDirs[g_nAdditionalDirectoryCount] );
		g_nAdditionalDirectoryCount++;
	}
	else
	{
		Assert( 0 );
	}
	
	//update the original base paths with the new number
	// we'll produce a cross-product of the set of original paths and the new relative paths
	g_NumBasePaths = s_originalNumBasePaths + ( s_originalNumBasePaths * g_nAdditionalDirectoryCount );
	if ( g_NumBasePaths > MAX_CMDLIB_BASE_PATHS )
	{
		Error( "You have too many search paths, let SteveK know about this\n");
	}

	//make an array of all the new search directories and copy it back into g_pBasePaths
	int nGeneratedPaths = 0;
	for ( int nOriginalBasePath = 0; nOriginalBasePath <  s_originalNumBasePaths; nOriginalBasePath++ )
	{
		for ( int nAdditionalDir = 0; nAdditionalDir < g_nAdditionalDirectoryCount; nAdditionalDir++ )
		{
			//add in the new search dirs here
			V_strcpy_safe( g_pBasePaths[ nGeneratedPaths ], s_originalBasePaths[ nOriginalBasePath ] ); 
			V_strcat_safe( g_pBasePaths[ nGeneratedPaths ], s_addedRelativeDirs[ nAdditionalDir ], sizeof( s_addedRelativeDirs[ nAdditionalDir ] ) );
			nGeneratedPaths++;
		}
		//we still need the original base path, but insert it after the newly added ones
		V_strcpy_safe( g_pBasePaths[ nGeneratedPaths ], s_originalBasePaths[ nOriginalBasePath ] );
		nGeneratedPaths++;
	}
}


bool CmdLib_HasBasePath( const char *pFileName_, int &pathLength )
{
	char *pFileName = ( char * )stackalloc( strlen( pFileName_ ) + 1 );
	V_strcpy( pFileName, pFileName_ );
	Q_FixSlashes( pFileName );
	pathLength = 0;
	for( int i = 0; i < g_NumBasePaths; i++ )
	{
		// see if we can rip the base off of the filename.
		if( Q_strncasecmp( g_pBasePaths[ i ], pFileName, Q_strlen( g_pBasePaths[ i ] ) ) == 0 )
		{
			pathLength = strlen( g_pBasePaths[ i ] );
			return true;
		}
	}
	return false;
}

int CmdLib_GetNumBasePaths( void )
{
	return g_NumBasePaths;
}

const char *CmdLib_GetBasePath( int i )
{
	Assert( i >= 0 && i < g_NumBasePaths );
	return g_pBasePaths[ i ];
}

FileHandle_t SafeOpenRead( const char *filename )
{
	int pathLength;
	FileHandle_t f = 0;
	if( CmdLib_HasBasePath( filename, pathLength ) )
	{
		filename = filename + pathLength;
		char tmp[ MAX_PATH ];
		for( int i = 0; i < g_NumBasePaths; i++ )
		{
			V_strcpy_safe( tmp, g_pBasePaths[ i ] );
			V_strcat_safe( tmp, filename );
			f = g_pFileSystem->Open( tmp, "rb" );
			if( f )
			{
				return f;
			}
		}
	
		Error( "Error opening %s\n",filename );
		return f;
	}
	else
	{
		f = g_pFileSystem->Open( filename, "rb" );
		if ( !f )
		{
			Error( "Error opening %s",filename );
		}

		return f;
	}
}

void SafeRead( FileHandle_t f, void *buffer, int count)
{
	if ( g_pFileSystem->Read( buffer, count, f ) != ( size_t )count )
	{
		Error( "File read failure" );
	}
}


void SafeWrite ( FileHandle_t f, void *buffer, int count)
{
	if ( g_pFileSystem->Write( buffer, count, f ) != ( size_t )count )
	{
		Error( "File write failure" );
	}
}


/*
==============
FileExists
==============
*/
qboolean	FileExists( const char *filename )
{
	FileHandle_t hFile = g_pFileSystem->Open( filename, "rb" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
	{
		return false;
	}
	else
	{
		g_pFileSystem->Close( hFile );
		return true;
	}
}

/*
==============
LoadFile
==============
*/
int    LoadFile( const char *filename, void **bufferptr )
{
	int    length = 0;
	void    *buffer;

	FileHandle_t f = SafeOpenRead( filename );
	if ( FILESYSTEM_INVALID_HANDLE != f )
	{
		length = Q_filelength( f );
		buffer = malloc( length + 1 );
		( ( char * )buffer )[ length ] = 0;
		SafeRead( f, buffer, length );
		g_pFileSystem->Close( f );
		*bufferptr = buffer;
	}
	else
	{
		*bufferptr = NULL;
	}
	return length;
}



/*
==============
SaveFile
==============
*/
void    SaveFile( const char *filename, void *buffer, int count )
{
	FileHandle_t f = SafeOpenWrite( filename );
	SafeWrite( f, buffer, count );
	g_pFileSystem->Close( f );
}

/*
====================
Extract file parts
====================
*/
// FIXME: should include the slash, otherwise
// backing to an empty path will be wrong when appending a slash



/*
==============
ParseNum / ParseHex
==============
*/
int ParseHex( char *hex )
{
	char    *str;
	int    num;

	num = 0;
	str = hex;

	while ( *str )
	{
		num <<= 4;
		if ( *str >= '0' && *str <= '9' )
		{
			num += *str - '0';
		}
		else if ( *str >= 'a' && *str <= 'f' )
		{
			num += 10 + *str - 'a';
		}
		else if ( *str >= 'A' && *str <= 'F' )
		{
			num += 10 + *str - 'A';
		}
		else
		{
			Error( "Bad hex number: %s", hex );
		}
		str++;
	}

	return num;
}


int ParseNum( char *str )
{
	if ( str[ 0 ] == '$' )
		return ParseHex( str + 1 );
	if ( str[ 0 ] == '0' && str[ 1 ] == 'x' )
		return ParseHex( str + 2 );
	return atol( str );
}

/*
============
CreatePath
============
*/
void CreatePath( char *path )
{
	char	*ofs, c;

	// strip the drive
	if ( path[ 1 ] == ':' )
		path += 2;

	for ( ofs = path + 1; *ofs; ofs++)
	{
		c = *ofs;
		if ( c == '/' || c == '\\' )
		{	// create the directory
			*ofs = 0;
			Q_mkdir( path );
			*ofs = c;
		}
	}
}

//-----------------------------------------------------------------------------
// Creates a path, path may already exist
//-----------------------------------------------------------------------------
#if defined( _WIN32 ) || defined( WIN32 )
void SafeCreatePath( char *path )
{
	char *ptr;

	// skip past the drive path, but don't strip
	if ( path[ 1 ] == ':' )
	{
		ptr = strchr( path, '\\' );
	}
	else
	{
		ptr = path;
	}
	while ( ptr )
	{		
		ptr = strchr( ptr + 1, '\\' );
		if ( ptr )
		{
			*ptr = '\0';
			_mkdir( path );
			*ptr = '\\';
		}
	}
}
#endif

/*
============
QCopyFile

  Used to archive source files
============
*/
void QCopyFile( char *from, char *to )
{
	void	*buffer;
	int		length;

	length = LoadFile( from, &buffer );
	CreatePath( to );
	SaveFile( to, buffer, length );
	free( buffer );
}




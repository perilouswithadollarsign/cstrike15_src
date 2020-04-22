//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "host.h"
#include <ctype.h>
#include "draw.h"
#include "zone.h"
#include "sys.h"
#include <edict.h>
#include <coordsize.h>
#include <characterset.h>
#include <bitbuf.h>
#include "common.h"
#include "traceinit.h"
#include <filesystem.h>
#include "filesystem_engine.h"
#include <convar.h>
#include "gl_matsysiface.h"
#include "filesystem_init.h"
#include <materialsystem/imaterialsystemhardwareconfig.h>
#include <tier0/icommandline.h>
#include <vstdlib/random.h>
#include "sys_dll.h"
#include "datacache/idatacache.h"
#include "tier1/keyvalues.h"
#ifndef DEDICATED
#include "vgui_baseui_interface.h"
#include "vgui/ISystem.h"
#include "vgui_controls/Controls.h"
#endif
#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"
#include "cl_steamauth.h"
#ifdef _X360
#include "xbox/xbox_launch.h"
#elif defined(_PS3)
#include "tls_ps3.h"
#include "ps3_pathinfo.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Things in other C files.
#define MAX_LOG_DIRECTORIES	10000

bool com_ignorecolons = false; 

// wordbreak parsing set
static characterset_t	g_BreakSet, g_BreakSetIncludingColons;

#define COM_TOKEN_MAX_LENGTH 1024
char	com_token[COM_TOKEN_MAX_LENGTH];
unsigned char    com_character;

/*
All of Quake's data access is through a hierarchical file system, but the contents of 
the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all
game directories.  The sys_* files pass this to host_init in engineparms->basedir.
This can be overridden with the "-basedir" command line parm to allow code
debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory
that all generated files (savegames, screenshots, demos, config files) will
be saved to.  This can be overridden with the "-game" command line parameter.
The game directory can never be changed while quake is executing.
This is a precacution against having a malicious server instruct clients
to write files over areas they shouldn't.

The "cache directory" is only used during development to save network bandwidth,
especially over ISDN / T1 lines.  If there is a cache directory
specified, when a file is found by the normal search path, it will be mirrored
into the cache directory, then opened there.

FIXME:
The file "parms.txt" will be read out of the game directory and appended to the
current command line arguments to allow different games to initialize startup
parms differently.  This could be used to add a "-sspeed 22050" for the high
quality sound edition.  Because they are added at the end, they will not override
an explicit setting on the original command line.
*/

/*
==============================
COM_ExplainDisconnection

==============================
*/
void COM_ExplainDisconnection( bool bPrint, const char *fmt, ... )
{
	va_list		argptr;
	char		szString[1024];

	va_start ( argptr, fmt );
	Q_vsnprintf( szString, sizeof( szString ), fmt,argptr );
	va_end ( argptr );

	if ( !IsX360() )
	{
		Q_strncpy( gszDisconnectReason, szString, 256 );
		gfExtendedError = true;
	}

	if ( bPrint )
	{
		ConMsg( "%s\n", gszDisconnectReason );
	}

	char const *szNotificationReason = szString;
	if ( char const *szActualDisconnectReason = StringAfterPrefix( szString, "Disconnect: " ) )
		szNotificationReason = szActualDisconnectReason;

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
		"OnEngineDisconnectReason", "reason", szNotificationReason ) );
}

/*
==============================
COM_ExtendedExplainDisconnection

==============================
*/
void COM_ExtendedExplainDisconnection( bool bPrint, const char *fmt, ... )
{
	if ( !IsX360() )
	{
		va_list		argptr;
		char		string[1024];
		
		va_start (argptr, fmt);
		Q_vsnprintf(string, sizeof( string ), fmt,argptr);
		va_end (argptr);

		Q_strncpy( gszExtendedDisconnectReason, string, 256 );
	}

	if ( bPrint )
	{
		ConMsg( "%s\n", gszExtendedDisconnectReason );
	}

	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
		"OnEngineDisconnectReason", "reason", "" ) );
}

/*
==============
COM_ReadChar

Reads a single code point, turning unsupported 
UTF-8 characters into question marks
==============
*/

const char *COM_ReadChar(const char *data)
{
	if ( !data || !*data )
		return NULL;

	com_character = *data++;
	return data;
}

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
const char *COM_Parse (const char *data)
{
	unsigned char    c;
	int             len;
	characterset_t	*breaks;
	
	breaks = &g_BreakSetIncludingColons;
	if ( com_ignorecolons )
		breaks = &g_BreakSet;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if ( IN_CHARACTERSET( *breaks, c ) )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if ( IN_CHARACTERSET( *breaks, c ) )
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}

/*
==============
COM_AddNoise

Changes n random bits in a data block
==============
*/
void COM_AddNoise( unsigned char *data, int length, int number )
{
	for ( int i = 0; i < number; i++ )
	{
		int randomByte = RandomInt( 0, length-1 );
		int randomBit = RandomInt( 0, 7 );

		// get original data
		unsigned char dataByte = data[randomByte];

		// flip bit
		if ( dataByte & randomBit )
		{
			dataByte &= ~randomBit;
		}
		else
		{
			dataByte |= randomBit;
		}

		// write back
		data[randomByte] = dataByte;
	}
}

/*
==============

COM_Parse_Line

Parse a line out of a string
==============
*/

#pragma warning( disable : 4706 )

const char *COM_ParseLine (const char *data)
{
	int len = 0;

	while( data = COM_ReadChar( data ) )
	{
		if ( com_character < ' ' && com_character != '\t' )
			break;

		com_token[len++] = com_character;

		if ( len == COM_TOKEN_MAX_LENGTH-1 )
			break;
	}

	com_token[len] = 0;

	if ( len == COM_TOKEN_MAX_LENGTH-1 )
	{
		// if we stopped the line because it was too long, then 
		// skip to the actual line end

		while( data = COM_ReadChar( data ) )
		{
			if ( com_character < ' ' && com_character != '\t' )
				break;
		}
	}

	// now eat control chars at the end of the line
	const char *nextChar;

	while( nextChar = COM_ReadChar( data ) )
	{
		if ( com_character >= ' ' )
			return data;
		else
			data = nextChar;
	}

	return NULL;
}

#pragma warning( default : 4706 )

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( const char *buffer )
{
	const char *p;

	p = buffer;
	while ( *p && *p!='\n')
	{
		if ( !V_isspace( *p ) || V_isalnum( *p ) )
			return 1;

		p++;
	}

	return 0;
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
============
*/
char *va( const char *format, ... )
{
	va_list		argptr;
	static char	string[8][512];
	static int	curstring = 0;
	
	curstring = ( curstring + 1 ) % 8;

	va_start (argptr, format);
	Q_vsnprintf( string[curstring], sizeof( string[curstring] ), format, argptr );
	va_end (argptr);

	return string[curstring];  
}

/*
============
vstr

prints a vector into a temporary string
bufffer.
============
*/
const char *vstr(Vector& v)
{
	static int idx = 0;
	static char string[16][1024];

	idx++;
	idx &= 15;

	Q_snprintf(string[idx], sizeof( string[idx] ), "%.2f %.2f %.2f", v[0], v[1], v[2]);
	return string[idx];  
}

char    com_basedir[MAX_OSPATH];
char    com_gamedir[MAX_OSPATH];

/*
==================
CL_CheckGameDirectory

Client side game directory change.
==================
*/
bool COM_CheckGameDirectory( const char *gamedir )
{
	// Switch game directories if needed, or abort if it's not good.
	char szGD[ MAX_OSPATH ];

	if ( !gamedir || !gamedir[0] )
	{
		ConMsg( "Server didn't specify a gamedir, assuming no change\n" );
		return true;
	}

	// Rip out the current gamedir.
	Q_FileBase( com_gamedir, szGD, sizeof( szGD ) );

	if ( Q_stricmp( szGD, gamedir ) )
	{
		// We know szGD and gamedir aren't the same, but as long as one is 'portal2' and the other is 'portal_sixense2', go ahead and connect
		bool sixense_vs_not_sixense = false;
		if( (!Q_stricmp( szGD, "portal2" ) || !Q_stricmp( szGD, "portal2_sixense" )) && (!Q_stricmp( gamedir, "portal2" ) || !Q_stricmp( gamedir, "portal2_sixense" )) )
		{
			sixense_vs_not_sixense = true;
		}

		if( !sixense_vs_not_sixense )
		{
			// Changing game directories without restarting is not permitted any more
			ConMsg( "COM_CheckGameDirectory: game directories don't match (%s / %s)\n", szGD, gamedir );
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the file in the search path.
// Input  : *filename - 
//			*file - 
// Output : int
//-----------------------------------------------------------------------------
int COM_FindFile( const char *filename, FileHandle_t *file )
{
	Assert( file );
	int filesize = -1;
	*file = g_pFileSystem->Open( filename, "rb" );
	if ( *file )
	{
		filesize = g_pFileSystem->Size( *file );
	}
	return filesize;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*file - 
// Output : int
//-----------------------------------------------------------------------------
int COM_OpenFile( const char *filename, FileHandle_t *file )
{
	return COM_FindFile( (char *)filename, file );
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile (const char *filename, void *data, int len)
{
	FileHandle_t  handle;
	
	int nameLen = strlen( filename ) + 2;
	char *pName = ( char * )stackalloc( nameLen );

	Q_snprintf( pName, nameLen, "%s", filename);

	Q_FixSlashes( pName );
	COM_CreatePath( pName );

	handle = g_pFileSystem->Open( pName, "wb" );
	if ( !handle )
	{
		Warning ("COM_WriteFile: failed on %s\n", pName);
		return;
	}
	
	g_pFileSystem->Write( data, len, handle );
	g_pFileSystem->Close( handle );
}

/*
============
COM_CreatePath

Only used for CopyFile
============
*/
void COM_CreatePath (const char *path)
{
	char temppath[512];
	Q_strncpy( temppath, path, sizeof(temppath) );
	
	for (char *ofs = temppath+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/' || *ofs == '\\')
		{       // create the directory
			char old = *ofs;
			*ofs = 0;
			Sys_mkdir (temppath);
			*ofs = old;
		}
	}
}


/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
bool COM_CopyFile ( const char *netpath, const char *cachepath)
{
	if ( IsX360() )
		return false;

	int             remaining, count;
	char			buf[4096];
	FileHandle_t in, out;

	in = g_pFileSystem->Open( netpath, "rb" );

	Assert( in );

	if ( in == FILESYSTEM_INVALID_HANDLE )
		return false;
		
	// create directories up to the cache file
	COM_CreatePath (cachepath);     

	out = g_pFileSystem->Open( cachepath, "wb", "MOD" );

	Assert( out );
	
	if ( out == FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close( in );
		return false;
	}
		
	remaining = g_pFileSystem->Size( in );
	while ( remaining > 0 )
	{
		if (remaining < sizeof(buf))
		{
			count = remaining;
		}
		else
		{
			count = sizeof(buf);
		}
		g_pFileSystem->Read( buf, count, in );
		g_pFileSystem->Write( buf, count, out );
		remaining -= count;
	}

	g_pFileSystem->Close( in );
	g_pFileSystem->Close( out );   
	
	return true;
}


/*
===========
COM_ExpandFilename

Finds the file in the search path, copies over the name with the full path name.
This doesn't search in the pak file.
===========
*/
int COM_ExpandFilename( char *filename, int maxlength )
{
	char expanded[MAX_OSPATH];

	if ( g_pFileSystem->GetLocalPath( filename, expanded, sizeof(expanded) ) != NULL )
	{
		Q_strncpy( filename, expanded, maxlength );
		return 1;
	}

	if ( filename && filename[0] != '*' )
	{
		Warning ("COM_ExpandFilename: can't find %s\n", filename);
	}
	
	return 0;
}

/*
===========
COM_FileSize

Returns the size of the file only.
===========
*/
int COM_FileSize (const char *filename)
{
	return g_pFileSystem->Size(filename);
}

//-----------------------------------------------------------------------------
// Purpose: Close file handle
// Input  : hFile - 
//-----------------------------------------------------------------------------
void COM_CloseFile( FileHandle_t hFile )
{
	g_pFileSystem->Close( hFile );
}
 

/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Allways appends a 0 byte.
============
*/
cache_user_t *loadcache;
byte    *loadbuf;
int             loadsize;
byte *COM_LoadFile (const char *path, int usehunk, int *pLength)
{
	FileHandle_t	hFile;
	byte			*buf = NULL;
	char			base[128];
	int             len;

	if (pLength)
	{
		*pLength = 0;
	}

// look for it in the filesystem or pack files
	len = COM_OpenFile( path, &hFile );
	if ( !hFile )
	{
		return NULL;
	}

	// Extract the filename base name for hunk tag
	Q_FileBase( path, base, sizeof( base ) );

	unsigned bufSize = len + 1;
	if ( IsX360() )
	{
		bufSize = g_pFileSystem->GetOptimalReadSize( hFile, bufSize ); // align to sector
	}

	switch ( usehunk )
	{
	case 1:
		buf = (byte *)Hunk_AllocName (bufSize, base);
		break;
	case 2:
		AssertMsg( 0, "Temp alloc no longer supported\n" );
		break;
	case 3:
		AssertMsg( 0, "Cache alloc no longer supported\n" );
		break;
	case 4:
		{
			if (len+1 > loadsize)
				buf = (byte *)malloc(bufSize);
			else
				buf = loadbuf;
		}
		break;
	case 5:
		buf = (byte *)malloc(bufSize);  // YWB:  FIXME, this is evil.
		break;
	default:
		Sys_Error ("COM_LoadFile: bad usehunk");
	}

	if ( !buf ) 
	{
		Sys_Error ("COM_LoadFile: not enough space for %s", path);
		COM_CloseFile(hFile);	// exit here to prevent fault on oom (kdb)
		return NULL;			
	}
		
	g_pFileSystem->ReadEx( buf, bufSize, len, hFile );
	COM_CloseFile( hFile );

	((byte *)buf)[ len ] = 0;

	if ( pLength )
	{
		*pLength = len;
	}
	return buf;
}

/*
===============
COM_CopyFileChunk

===============
*/
void COM_CopyFileChunk( FileHandle_t dst, FileHandle_t src, int nSize )
{
	int   copysize = nSize;
	char  copybuf[COM_COPY_CHUNK_SIZE];

	while (copysize > COM_COPY_CHUNK_SIZE)
	{
		g_pFileSystem->Read ( copybuf, COM_COPY_CHUNK_SIZE, src );
		g_pFileSystem->Write( copybuf, COM_COPY_CHUNK_SIZE, dst );
		copysize -= COM_COPY_CHUNK_SIZE;
	}

	g_pFileSystem->Read ( copybuf, copysize, src );
	g_pFileSystem->Write( copybuf, copysize, dst );

	g_pFileSystem->Flush ( src );
	g_pFileSystem->Flush ( dst );
}

// uses malloc if larger than bufsize
byte *COM_LoadStackFile (const char *path, void *buffer, int bufsize, int& filesize )
{
	byte    *buf;
	loadbuf = (byte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, 4, &filesize );
	return buf;
}

void COM_ShutdownFileSystem( void )
{
}

/*
================
COM_Shutdown

Remove the searchpaths
================
*/
void COM_Shutdown( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: allocates memory and copys source text
// Input  : *in - 
// Output : char *CopyString
//-----------------------------------------------------------------------------
char *COM_StringCopy(const char *in)
{
	int len = Q_strlen(in)+1;
	char *out = (char *)new char[ len ];
	Q_strncpy (out, in, len );
	return out;
}

void COM_StringFree(const char *in)
{
	delete [] in;
}


void COM_SetupLogDir( const char *mapname )
{
#if defined( _CERT )
	return;
#endif

	char gameDir[MAX_OSPATH];
	COM_GetGameDir( gameDir, sizeof( gameDir ) );

	// Blat out the all directories in the LOGDIR path
	g_pFileSystem->RemoveSearchPath( NULL, "LOGDIR" );

	// set the log directory
	if ( mapname && CommandLine()->FindParm("-uselogdir") )
	{
		int i;
		char sRelativeLogDir[MAX_PATH];
		for ( i = 0; i < MAX_LOG_DIRECTORIES; i++ )
		{
			Q_snprintf( sRelativeLogDir, sizeof( sRelativeLogDir ), "logs/%s/%04i", mapname, i );
			if ( !g_pFileSystem->IsDirectory( sRelativeLogDir, "GAME" ) )
				break;
		}

		// Loop at max
		if ( i == MAX_LOG_DIRECTORIES )
		{
			i = 0;	
			Q_snprintf( sRelativeLogDir, sizeof( sRelativeLogDir ), "logs/%s/%04i", mapname, i );
		}

		// Make sure the directories we need exist.
		g_pFileSystem->CreateDirHierarchy( sRelativeLogDir, "GAME" );	

		{
			static bool pathsetup = false;

			if ( !pathsetup )
			{
				pathsetup = true;

				// Set the search path
				char sLogDir[MAX_PATH];
				Q_snprintf( sLogDir, sizeof( sLogDir ), "%s/%s", gameDir, sRelativeLogDir );
				g_pFileSystem->AddSearchPath( sLogDir, "LOGDIR" );
			}
		}
	}
	else
	{
		// Default to the base game directory for logs.
		g_pFileSystem->AddSearchPath( gameDir, "LOGDIR" );
	}

	// no reason to have this as part of search path fallthrough
	// callers must explicitly request
	g_pFileSystem->MarkPathIDByRequestOnly( "LOGDIR", true );
}

/*
================
COM_GetModDirectory - return the final directory in the game dir (i.e "cstrike", "hl2", rather than c:\blah\cstrike )
================
*/
const char *COM_GetModDirectory()
{
	static char modDir[MAX_PATH];
	if ( Q_strlen( modDir ) == 0 )
	{
		const char *gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
		Q_strncpy( modDir, gamedir, sizeof(modDir) );
		if ( strchr( modDir, '/' ) || strchr( modDir, '\\' ) )
		{
			Q_StripLastDir( modDir, sizeof(modDir) );
			int dirlen = Q_strlen( modDir );
			Q_strncpy( modDir, gamedir + dirlen, sizeof(modDir) - dirlen );
		}
	}

	return modDir;
}

/*
================
COM_InitFilesystem
================
*/
void COM_InitFilesystem( const char *pFullModPath )
{
	CFSSearchPathsInit initInfo;

#ifndef DEDICATED	
	char language[128];

	// Fallback to English
	V_strncpy( language, "english", sizeof( language ) );

	if ( IsPC() )
	{
#if !defined( NO_STEAM )
		if ( CommandLine()->CheckParm( "-language" ) )
		{
			Q_strncpy( language, CommandLine()->ParmValue( "-language", "english"), sizeof( language ) );
		}
		else
		{
			// get Steam client language
			memset( language, 0, sizeof( language ) );
			if ( Steam3Client().SteamApps() )
			{
				// just follow the language steam wants you to be
				const char *lang = Steam3Client().SteamApps()->GetCurrentGameLanguage();
				if ( lang && Q_strlen(lang) )
				{
					Q_strncpy( language, lang, sizeof(language) );
				}
				else 
					Q_strncpy( language, "english", sizeof(language) );
			}
			else 
			{
				Q_strncpy( language, "english", sizeof(language) );
			}
		}
#endif // NO_STEAM
	}

#if defined( _GAMECONSOLE )
	if ( XBX_IsAudioLocalized() )
	{
		// allow non-english audio localization for gameconsole configured language
		V_strncpy( language, XBX_GetLanguageString(), sizeof( language ) );
	}
#endif

	if ( ( Q_strlen( language ) > 0 ) && ( Q_stricmp( language, "english" ) != 0 ) )
	{
		initInfo.m_pLanguage = language;
	}
#endif
	
	initInfo.m_pFileSystem = g_pFileSystem;
#if !defined(_PS3)
	initInfo.m_pDirectoryName = pFullModPath;
#else
	char ps3NeedsAbsoluteModPath[256];

#ifdef HDD_BOOT
	snprintf( ps3NeedsAbsoluteModPath, 256, "%s", pFullModPath );
#else
	snprintf( ps3NeedsAbsoluteModPath, 256, "%s/%s", g_pPS3PathInfo->GameImagePath(), pFullModPath );
#endif    

	initInfo.m_pDirectoryName = ps3NeedsAbsoluteModPath;
#endif
	if ( !initInfo.m_pDirectoryName )
	{
		initInfo.m_pDirectoryName = GetCurrentGame();
	}

	// Load gameinfo.txt and setup all the search paths, just like the tools do.
	FileSystem_LoadSearchPaths( initInfo );

	// The mod path becomes com_gamedir.
	Q_MakeAbsolutePath( com_gamedir, sizeof( com_gamedir ), initInfo.m_ModPath );
							  	
	// Set com_basedir.
	Q_strncpy ( com_basedir, GetBaseDirectory(), sizeof( com_basedir ) ); // the "root" directory where hl2.exe is
	Q_strlower( com_basedir );
	Q_FixSlashes( com_basedir );
	
#ifndef DEDICATED
	EngineVGui()->SetVGUIDirectories();
#endif

	// Set LOGDIR to be something reasonable
	COM_SetupLogDir( NULL );
}

const char *COM_DXLevelToString( int dxlevel )
{
	bool bHalfPrecision = false;

	const char *pShaderDLLName = g_pMaterialSystemHardwareConfig->GetShaderDLLName();
	if( pShaderDLLName && Q_stristr( pShaderDLLName, "nvfx" ) )
	{
		bHalfPrecision = true;
	}

	switch( dxlevel )
	{
	case 60:
		return "gamemode - 6.0";
	case 70:
		return "gamemode - 7.0";
	case 80:
		return "gamemode - 8.0";
	case 81:
		return "gamemode - 8.1";
	case 82:
		if( bHalfPrecision )
		{
			return "gamemode - 8.1 with some 9.0 (half-precision)";
		}
		else
		{
			return "gamemode - 8.1 with some 9.0 (full-precision)";
		}
	case 90:
		if( bHalfPrecision )
		{
			return "gamemode - 9.0 (half-precision)";
		}
		else
		{
			return "gamemode - 9.0 (full-precision)";
		}
	case 92:
		return "9.0 Shader Model 2.0b";
	case 95:
		return "9.0 Shader Model 3.0";
	case 98:
		return "XBox 360";
	case 100:
		return "10.0 Shader Model 4.0";
	default:
		return "gamemode";
	}
}

const char *COM_FormatSeconds( int seconds )
{
	static char string[64];

	int hours = 0;
	int minutes = seconds / 60;

	if ( minutes > 0 )
	{
		seconds -= (minutes * 60);
		hours = minutes / 60;

		if ( hours > 0 )
		{
			minutes -= (hours * 60);
		}
	}
	
	if ( hours > 0 )
	{
		Q_snprintf( string, sizeof(string), "%2i:%02i:%02i", hours, minutes, seconds );
	}
	else
	{
		Q_snprintf( string, sizeof(string), "%02i:%02i", minutes, seconds );
	}

	return string;
}

// Non-VarArgs version
void COM_LogString( char const *pchFile, char const *pchString )
{
	if ( !g_pFileSystem )
	{
		Assert( 0 );
		return;
	}

	FileHandle_t fp;
	const char *pfilename;

	if ( !pchFile )
	{
		pfilename = "hllog.txt";
	}
	else
	{
		pfilename = pchFile;
	}

	fp = g_pFileSystem->Open( pfilename, "a+t");
	if (fp)
	{
		g_pFileSystem->FPrintf(fp, "%s", pchString);
		g_pFileSystem->Close(fp);
	}
}

void COM_Log( const char *pszFile, const char *fmt, ...)
{
	if ( !g_pFileSystem )
	{
		Assert( 0 );
		return;
	}

	va_list		argptr;
	char		string[8192];

	va_start (argptr,fmt);
	Q_vsnprintf(string, sizeof( string ), fmt,argptr);
	va_end (argptr);

	COM_LogString( pszFile, string );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename1 - 
//			*filename2 - 
//			*iCompare - 
// Output : int
//-----------------------------------------------------------------------------
int COM_CompareFileTime(const char *filename1, const char *filename2, int *iCompare)
{
	int bRet = 0;
	if ( iCompare )
	{
		*iCompare = 0;
	}

	if (filename1 && filename2)
	{
		long ft1 = g_pFileSystem->GetFileTime( filename1 );
		long ft2 = g_pFileSystem->GetFileTime( filename2 );

		if ( iCompare )
		{
			*iCompare = Sys_CompareFileTime( ft1,  ft2 );
		}
		bRet = 1;
	}

	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szGameDir - 
//-----------------------------------------------------------------------------
void COM_GetGameDir(char *szGameDir, int maxlen)
{
	if (!szGameDir) return;
	Q_strncpy(szGameDir, com_gamedir, maxlen );
}

//-----------------------------------------------------------------------------
// Purpose: Parse a token from a file stream
// Input  : *data - 
//			*token - 
// Output : char
//-----------------------------------------------------------------------------
const char *COM_ParseFile(const char *data, char *token, int maxtoken )
{
	const char *return_data = COM_Parse(data);
	Q_strncpy(token, com_token, maxtoken);
	return return_data;	
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void COM_Init
//-----------------------------------------------------------------------------
void COM_Init ( void )
{
	CharacterSetBuild( &g_BreakSet, "{}()'" );
	CharacterSetBuild( &g_BreakSetIncludingColons, "{}()':" );
}

bool COM_IsValidPath( const char *pszFilename )
{
	if ( !pszFilename )
	{
		return false;
	}

	if ( Q_strlen( pszFilename ) <= 0    ||
		Q_strstr( pszFilename, "\\\\" ) ||	// to protect network paths
		Q_strstr( pszFilename, ":" )    || // to protect absolute paths
		Q_strstr( pszFilename, ".." ) ||   // to protect relative paths
		Q_strstr( pszFilename, "\n" ) ||   // CFileSystem_Stdio::FS_fopen doesn't allow this
		Q_strstr( pszFilename, "\r" ) )    // CFileSystem_Stdio::FS_fopen doesn't allow this
	{
		return false;
	}

	return true;
}

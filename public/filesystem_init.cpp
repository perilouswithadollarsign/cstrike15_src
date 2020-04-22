//====== Copyright 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#undef PROTECTED_THINGS_ENABLE
#undef PROTECT_FILEIO_FUNCTIONS
#ifndef POSIX
#undef fopen
#endif

#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#elif defined( POSIX )
#include <unistd.h>
#endif
#include <stdio.h>
#include <sys/stat.h>
#include "tier0/platform.h"
#include "tier1/strtools.h"
#include "filesystem_init.h"
#include "tier0/icommandline.h"
#include "tier0/stacktools.h"
#include "keyvalues.h"
#include "appframework/IAppSystemGroup.h"
#include "tier1/smartptr.h"
#if defined( _X360 )
#include "xbox\xbox_win32stubs.h"
#endif
#if defined( _PS3 )
#include "ps3/ps3_win32stubs.h"
#include "ps3/ps3_helpers.h"
#include "ps3_pathinfo.h"
#endif

#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#if !defined( _X360 )
#define GAMEINFO_FILENAME			"gameinfo.txt"
#else
// The .xtx file is a TCR requirement, as .txt files cannot live on the DVD.
// The .xtx file only exists outside the zips (same as .txt and is made during the image build) and is read to setup the search paths.
// So all other code should be able to safely expect gameinfo.txt after the zip is mounted as the .txt file exists inside the zips.
// The .xtx concept is private and should only have to occurr here. As a safety measure, if the .xtx file is not found
// a retry is made with the original .txt name
#define GAMEINFO_FILENAME			"gameinfo.xtx"
#endif
#define GAMEINFO_FILENAME_ALTERNATE	"gameinfo.txt"

static char g_FileSystemError[256];
static bool s_bUseVProjectBinDir = false;
static FSErrorMode_t g_FileSystemErrorMode = FS_ERRORMODE_VCONFIG;

// Call this to use a bin directory relative to VPROJECT
void FileSystem_UseVProjectBinDir( bool bEnable )
{
	s_bUseVProjectBinDir = bEnable;
}

// This class lets you modify environment variables, and it restores the original value
// when it goes out of scope.
class CTempEnvVar
{
public:
	CTempEnvVar( const char *pVarName )
	{
		m_bRestoreOriginalValue = true;
		m_pVarName = pVarName;

		const char *pValue = NULL;

#if defined( _WIN32 ) || defined( _GAMECONSOLE )
		// Use GetEnvironmentVariable instead of getenv because getenv doesn't pick up changes
		// to the process environment after the DLL was loaded.
		char szBuf[ 4096 ];
		if ( GetEnvironmentVariable( m_pVarName, szBuf, sizeof( szBuf ) ) != 0)
		{
			pValue = szBuf;
		}
#else
		// LINUX BUG: see above
		pValue = getenv( pVarName );
#endif 

		if ( pValue )
		{
			m_bExisted = true;
			m_OriginalValue.SetSize( strlen( pValue ) + 1 );
			memcpy( m_OriginalValue.Base(), pValue, m_OriginalValue.Count() );
		}
		else
		{
			m_bExisted = false;
		}
	}

	~CTempEnvVar()
	{
		if ( m_bRestoreOriginalValue )
		{
			// Restore the original value.
			if ( m_bExisted )
			{
				SetValue( "%s", m_OriginalValue.Base() );
			}
			else
			{
				ClearValue();
			}
		}
	}

	void SetRestoreOriginalValue( bool bRestore )
	{
		m_bRestoreOriginalValue = bRestore;
	}

	int GetValue(char *pszBuf, int nBufSize )
	{
		if ( !pszBuf || ( nBufSize <= 0 ) )
			return 0;
	
#if defined( _WIN32 ) || defined( _GAMECONSOLE )
		// Use GetEnvironmentVariable instead of getenv because getenv doesn't pick up changes
		// to the process environment after the DLL was loaded.
		return GetEnvironmentVariable( m_pVarName, pszBuf, nBufSize );
#else
		// LINUX BUG: see above
		const char *pszOut = getenv( m_pVarName );
		if ( !pszOut )
		{
			*pszBuf = '\0';
			return 0;
		}

		Q_strncpy( pszBuf, pszOut, nBufSize );		
		return Q_strlen( pszBuf );
#endif
	}

	void SetValue( const char *pValue, ... )
	{
		char valueString[4096];
		va_list marker;
		va_start( marker, pValue );
		Q_vsnprintf( valueString, sizeof( valueString ), pValue, marker );
		va_end( marker );

#if defined( WIN32 ) || defined( _GAMECONSOLE )
		char str[4096];
		Q_snprintf( str, sizeof( str ), "%s=%s", m_pVarName, valueString );
		_putenv( str );
#else
		setenv( m_pVarName, valueString, 1 );
#endif
	}

	void ClearValue()
	{
#if defined( WIN32 ) || defined( _GAMECONSOLE )
		char str[512];
		Q_snprintf( str, sizeof( str ), "%s=", m_pVarName );
		_putenv( str );
#else
		setenv( m_pVarName, "", 1 );
#endif
	}

private:
	bool m_bRestoreOriginalValue;
	const char *m_pVarName;
	bool m_bExisted;
	CUtlVector<char> m_OriginalValue;
};


class CSteamEnvVars
{
public:
	CSteamEnvVars() :
		m_SteamAppId( "SteamAppId" ),
		m_SteamUserPassphrase( "SteamUserPassphrase" ),
		m_SteamAppUser( "SteamAppUser" ),
		m_Path( "path" )
	{
	}
	
	void SetRestoreOriginalValue_ALL( bool bRestore )
	{
		m_SteamAppId.SetRestoreOriginalValue( bRestore );
		m_SteamUserPassphrase.SetRestoreOriginalValue( bRestore );
		m_SteamAppUser.SetRestoreOriginalValue( bRestore );
		m_Path.SetRestoreOriginalValue( bRestore );
	}

	CTempEnvVar m_SteamAppId;
	CTempEnvVar m_SteamUserPassphrase;
	CTempEnvVar m_SteamAppUser;
	CTempEnvVar m_Path;
};

// ---------------------------------------------------------------------------------------------------- //
// Helpers.
// ---------------------------------------------------------------------------------------------------- //
void Q_getwd( char *out, int outSize )
{
#if defined( _WIN32 ) || defined( WIN32 )
	_getcwd( out, outSize );
	Q_strncat( out, "\\", outSize, COPY_ALL_CHARACTERS );
#elif defined( _PS3 )
	Assert( 0 );
	if ( outSize > 0 )
		out[0] = '\0';
#else
	getcwd( out, outSize );
	strcat( out, "/" );
#endif
	Q_FixSlashes( out );
}

// ---------------------------------------------------------------------------------------------------- //
// Module interface.
// ---------------------------------------------------------------------------------------------------- //

CFSSearchPathsInit::CFSSearchPathsInit()
{
	m_pDirectoryName = NULL;
	m_pLanguage = NULL;
	m_ModPath[0] = 0;
}


CFSSteamSetupInfo::CFSSteamSetupInfo()
{
	m_pDirectoryName = NULL;
	m_bOnlyUseDirectoryName = false;
	m_bSteam = false;
	m_bToolsMode = true;
	m_bNoGameInfo = false;
}


CFSLoadModuleInfo::CFSLoadModuleInfo()
{
	m_pFileSystemDLLName = NULL;
	m_pFileSystem = NULL;
	m_pModule = NULL;
}


CFSMountContentInfo::CFSMountContentInfo()
{
	m_bToolsMode = true;
	m_pDirectoryName = NULL;
	m_pFileSystem = NULL;
}


const char *FileSystem_GetLastErrorString()
{
	return g_FileSystemError;
}


void AddLanguageGameDir( IFileSystem *pFileSystem, const char *pLocation, const char *pLanguage )
{
#if !defined( DEDICATED )
	char temp[MAX_PATH];
	Q_snprintf( temp, sizeof(temp), "%s_%s", pLocation, pLanguage );
	pFileSystem->AddSearchPath( temp, "GAME", PATH_ADD_TO_TAIL );

	if ( IsPC() )
	{
		// also look in "..\localization\<folder>" if that directory exists
		char baseDir[MAX_PATH];
		char *tempPtr = NULL, *gameDir = NULL;

		Q_strncpy( baseDir, pLocation, sizeof(baseDir) );
#ifdef WIN32
		tempPtr = Q_strstr( baseDir, "\\game\\" );
#else
		tempPtr = Q_strstr( baseDir, "/game/" );
#endif		
		if ( tempPtr )
		{
#ifdef WIN32
			gameDir = tempPtr + Q_strlen( "\\game\\" );
#else
			gameDir = tempPtr + Q_strlen( "/game/" );
#endif
			*tempPtr = 0;
			Q_snprintf( temp, sizeof(temp), "%s%clocalization%c%s_%s", baseDir, CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR, gameDir, pLanguage );
			
			if ( pFileSystem->IsDirectory( temp ) )
			{
				pFileSystem->AddSearchPath( temp, "GAME", PATH_ADD_TO_TAIL );
			}
		}
	}
#endif
}


void AddGameBinDir( IFileSystem *pFileSystem, const char *pLocation, bool bForceHead = false )
{
	char temp[MAX_PATH];
	Q_snprintf( temp, sizeof(temp), "%s%cbin", pLocation, CORRECT_PATH_SEPARATOR );
	pFileSystem->AddSearchPath( temp, "GAMEBIN", bForceHead ? PATH_ADD_TO_HEAD : PATH_ADD_TO_TAIL );
}

KeyValues* ReadKeyValuesFile( const char *pFilename )
{
	MEM_ALLOC_CREDIT();
	// Read in the gameinfo.txt file and null-terminate it.
	FILE *fp = fopen( pFilename, "rb" );
	if ( !fp )
		return NULL;
	CUtlVector<char> buf;
	fseek( fp, 0, SEEK_END );
	buf.SetSize( ftell( fp ) + 1 );
	fseek( fp, 0, SEEK_SET );
	fread( buf.Base(), 1, buf.Count()-1, fp );
	fclose( fp );
	buf[buf.Count()-1] = 0;

	KeyValues *kv = new KeyValues( "" );
	if ( !kv->LoadFromBuffer( pFilename, buf.Base() ) )
	{
		kv->deleteThis();
		return NULL;
	}
	
	return kv;
}

static bool Sys_GetExecutableName( char *out, int len )
{

#if defined( _WIN32 )

    if ( !::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), out, len ) )
    {
		return false;
    }
	// Fix up the path if Windows gave us a messy one.
	if ( !Q_RemoveDotSlashes( out ) )
	{
		Error( "V_MakeAbsolutePath: tried to \"..\" past the root." );
		return false;
	}
	Q_FixSlashes( out );

#elif defined( _PS3 )

	Q_snprintf( out, len, "%s/valve.self", g_pPS3PathInfo->SelfDirectory() );

#else

	if ( CommandLine()->GetParm(0) )
	{
		Q_MakeAbsolutePath( out, len, CommandLine()->GetParm(0) );
	}
	else
	{
		return false;
	}

#endif
	
	return true;
}

bool FileSystem_GetExecutableDir( char *exedir, int exeDirLen )
{
	exedir[0] = 0;

	if ( s_bUseVProjectBinDir )
	{
		const char *pProject = GetVProjectCmdLineValue();
		if ( !pProject )
		{
			// Check their registry.
			pProject = getenv( GAMEDIR_TOKEN );
		}
		if ( pProject )
		{
			Q_snprintf( exedir, exeDirLen, "%s%c..%cbin", pProject, CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR );
			return true;
		}
		return false;
	}

	if ( !Sys_GetExecutableName( exedir, exeDirLen ) )
		return false;
	Q_StripFilename( exedir );

	if ( IsX360() )
	{
		// The 360 can have its exe and dlls reside on different volumes
		// use the optional basedir as the exe dir
		if ( CommandLine()->FindParm( "-basedir" ) )
		{
			strcpy( exedir, CommandLine()->ParmValue( "-basedir", "" ) );
		}
	}

	Q_FixSlashes( exedir );

	// Return the bin directory as the executable dir if it's not in there
	// because that's really where we're running from...
	char ext[MAX_PATH];
	Q_StrRight( exedir, 4, ext, sizeof( ext ) );
	if ( ext[0] != CORRECT_PATH_SEPARATOR || Q_stricmp( ext+1, "bin" ) != 0 )
	{
		Q_strncat( exedir, CORRECT_PATH_SEPARATOR_S, exeDirLen, COPY_ALL_CHARACTERS );
		Q_strncat( exedir, "bin", exeDirLen, COPY_ALL_CHARACTERS );

		#ifdef PLATFORM_64BITS
			#ifdef _WIN64
				const char *pPlatPath = "x64";
			#elif OSX
				const char *pPlatPath = "osx64";
			#elif LINUX
				const char *pPlatPath = "linux64";
			#endif

			Q_strncat( exedir, CORRECT_PATH_SEPARATOR_S, exeDirLen, COPY_ALL_CHARACTERS );
			Q_strncat( exedir, pPlatPath, exeDirLen, COPY_ALL_CHARACTERS );
		#endif
		Q_FixSlashes( exedir );
	}
	
	return true;
}

static bool FileSystem_GetBaseDir( char *baseDir, int baseDirLen )
{
#ifdef _PS3
	V_strncpy( baseDir,  g_pPS3PathInfo->GameImagePath(), baseDirLen );
		
	return baseDir[0] != 0;
#else
	if ( FileSystem_GetExecutableDir( baseDir, baseDirLen ) )
	{
		Q_StripFilename( baseDir );
		return true;
	}

	return false;
#endif
}

void LaunchVConfig()
{
#if defined( _WIN32 ) && !defined( _X360 )
	char vconfigExe[MAX_PATH];
	FileSystem_GetExecutableDir( vconfigExe, sizeof( vconfigExe ) );
	Q_AppendSlash( vconfigExe, sizeof( vconfigExe ) );
	Q_strncat( vconfigExe, "vconfig.exe", sizeof( vconfigExe ), COPY_ALL_CHARACTERS );

	char *argv[] =
	{
		vconfigExe,
		"-allowdebug",
		NULL
	};

	_spawnv( _P_NOWAIT, vconfigExe, argv );
#elif defined( _X360 )
	Msg( "Launching vconfig.exe not supported\n" );
#endif
}

const char* GetVProjectCmdLineValue()
{
	return CommandLine()->ParmValue( "-vproject", CommandLine()->ParmValue( "-game" ) );
}

FSReturnCode_t SetupFileSystemError( bool bRunVConfig, FSReturnCode_t retVal, const char *pMsg, ... )
{
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( g_FileSystemError, sizeof( g_FileSystemError ), pMsg, marker );
	va_end( marker );

	Warning( "%s\n", g_FileSystemError );

	// Run vconfig?
	// Don't do it if they specifically asked for it not to, or if they manually specified a vconfig with -game or -vproject.
	if ( bRunVConfig && g_FileSystemErrorMode == FS_ERRORMODE_VCONFIG && !CommandLine()->FindParm( CMDLINEOPTION_NOVCONFIG ) && !GetVProjectCmdLineValue() )
	{
		LaunchVConfig();
	}

	if ( g_FileSystemErrorMode == FS_ERRORMODE_AUTO || g_FileSystemErrorMode == FS_ERRORMODE_VCONFIG )
	{
		// this may happen when we eject disk while loading the game. 
		if( IsPS3() )
		{
			return FS_UNABLE_TO_INIT;
		}
		else
		{
			Error( "%s\n", g_FileSystemError );
		}
	}
	
	return retVal;
}

FSReturnCode_t LoadGameInfoFile( 
	const char *pDirectoryName, 
	KeyValues *&pMainFile, 
	KeyValues *&pFileSystemInfo, 
	KeyValues *&pSearchPaths )
{
	// If GameInfo.txt exists under pBaseDir, then this is their game directory.
	// All the filesystem mappings will be in this file.
	char gameinfoFilename[MAX_PATH];
	Q_strncpy( gameinfoFilename, pDirectoryName, sizeof( gameinfoFilename ) );
	Q_AppendSlash( gameinfoFilename, sizeof( gameinfoFilename ) );
	Q_strncat( gameinfoFilename, GAMEINFO_FILENAME, sizeof( gameinfoFilename ), COPY_ALL_CHARACTERS );
	Q_FixSlashes( gameinfoFilename );
	pMainFile = ReadKeyValuesFile( gameinfoFilename );
#if defined( _PS3 )
	if ( IsPS3() && !pMainFile )
	{
		Q_strncpy( gameinfoFilename, g_pPS3PathInfo->GameImagePath(), sizeof( gameinfoFilename ) );
		Q_AppendSlash( gameinfoFilename, sizeof( gameinfoFilename ) );
		Q_strncat( gameinfoFilename, pDirectoryName, sizeof( gameinfoFilename ) );
		Q_AppendSlash( gameinfoFilename, sizeof( gameinfoFilename ) );
		Q_strncat( gameinfoFilename, GAMEINFO_FILENAME_ALTERNATE, sizeof( gameinfoFilename ), COPY_ALL_CHARACTERS );
		Q_FixSlashes( gameinfoFilename );
		Msg("Attempting %s\n", gameinfoFilename);
		pMainFile = ReadKeyValuesFile( gameinfoFilename );
	}
#endif
	if ( IsX360() && !pMainFile )
	{
		// try again
		Q_strncpy( gameinfoFilename, pDirectoryName, sizeof( gameinfoFilename ) );
		Q_AppendSlash( gameinfoFilename, sizeof( gameinfoFilename ) );
		Q_strncat( gameinfoFilename, GAMEINFO_FILENAME_ALTERNATE, sizeof( gameinfoFilename ), COPY_ALL_CHARACTERS );
		Q_FixSlashes( gameinfoFilename );
		pMainFile = ReadKeyValuesFile( gameinfoFilename );
	}
	if ( !pMainFile )
	{
		return SetupFileSystemError( true, FS_MISSING_GAMEINFO_FILE, "%s is missing.", gameinfoFilename );
	}

	pFileSystemInfo = pMainFile->FindKey( "FileSystem" );
	if ( !pFileSystemInfo )
	{
		pMainFile->deleteThis();
		return SetupFileSystemError( true, FS_INVALID_GAMEINFO_FILE, "%s is not a valid format.", gameinfoFilename );
	}

	// Now read in all the search paths.
	pSearchPaths = pFileSystemInfo->FindKey( "SearchPaths" );
	if ( !pSearchPaths )
	{
		pMainFile->deleteThis();
		return SetupFileSystemError( true, FS_INVALID_GAMEINFO_FILE, "%s is not a valid format.", gameinfoFilename );
	}
	return FS_OK;
}

// checks the registry for the low violence setting
// Check "HKEY_CURRENT_USER\Software\Valve\Source\Settings" and "User Token 2" or "User Token 3"
bool IsLowViolenceBuild( void )
{
#if defined( _LOWVIOLENCE )
	// a low violence build can not be-undone
	return true;
#endif

	// Users can opt into low violence mode on the command-line.
	if ( CommandLine()->FindParm( "-lv" ) != 0 )
		return true;

#if defined(_WIN32)
	HKEY hKey;
	char szValue[64];
	unsigned long len = sizeof(szValue) - 1;
	bool retVal = false;
	
	if ( IsPC() && RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Valve\\Source\\Settings", NULL, KEY_READ, &hKey) == ERROR_SUCCESS )
	{
		// User Token 2
		if ( RegQueryValueEx( hKey, "User Token 2", NULL, NULL, (unsigned char*)szValue, &len ) == ERROR_SUCCESS )
		{
			if ( Q_strlen( szValue ) > 0 )
			{
				retVal = true;
			}
		}

		if ( !retVal )
		{
			// reset "len" for the next check
			len = sizeof(szValue) - 1;

			// User Token 3
			if ( RegQueryValueEx( hKey, "User Token 3", NULL, NULL, (unsigned char*)szValue, &len ) == ERROR_SUCCESS )
			{
				if ( Q_strlen( szValue ) > 0 )
				{
					retVal = true;
				}
			}
		}

		RegCloseKey(hKey);
	}

	return retVal;
#elif defined( _GAMECONSOLE )
	// console builds must be compiled with _LOWVIOLENCE
	return false;
#elif POSIX
	return false;
#else
	#error "Fix me"
#endif
}

static void FileSystem_AddLoadedSearchPath( 
	CFSSearchPathsInit &initInfo, 
	const char *pPathID, 
	bool *bFirstGamePath, 
	const char *pBaseDir, 
	const char *pLocation,
	bool bLowViolence )
{
	char fullLocationPath[MAX_PATH];
	Q_MakeAbsolutePath( fullLocationPath, sizeof( fullLocationPath ), pLocation, pBaseDir );

	// Now resolve any ./'s.
	V_FixSlashes( fullLocationPath );
	if ( !V_RemoveDotSlashes( fullLocationPath ) )
		Error( "FileSystem_AddLoadedSearchPath - Can't resolve pathname for '%s'", fullLocationPath );

	// Add language, mod, and gamebin search paths automatically.
	if ( Q_stricmp( pPathID, "game" ) == 0 )
	{
		bool bDoAllPaths = true;
#if defined( _X360 ) && defined( LEFT4DEAD )
		// hl2 is a vestigal mistake due to shaders, xbox needs to prevent any search path bloat
		if ( V_stristr( fullLocationPath, "\\hl2" ) )
		{
			bDoAllPaths = false;
		}
#endif

		// add the language path, needs to be topmost, generally only contains audio
		// and the language localized movies (there are 2 version one normal, one LV)
		// this trumps the LV english movie as desired for the language
		if ( initInfo.m_pLanguage && bDoAllPaths )
		{
			AddLanguageGameDir( initInfo.m_pFileSystem, fullLocationPath, initInfo.m_pLanguage );
		}

		// next add the low violence path
		if ( bLowViolence && bDoAllPaths )
		{
			char szPath[MAX_PATH];
			Q_snprintf( szPath, sizeof(szPath), "%s_lv", fullLocationPath );
			initInfo.m_pFileSystem->AddSearchPath( szPath, pPathID, PATH_ADD_TO_TAIL );
		}
		
		if ( CommandLine()->FindParm( "-tempcontent" ) != 0 && bDoAllPaths )
		{
			char szPath[MAX_PATH];
			Q_snprintf( szPath, sizeof(szPath), "%s_tempcontent", fullLocationPath );
			initInfo.m_pFileSystem->AddSearchPath( szPath, pPathID, PATH_ADD_TO_TAIL );
		}

		// mark the first "game" dir as the "MOD" dir
		if ( *bFirstGamePath )
		{
			*bFirstGamePath = false;
			initInfo.m_pFileSystem->AddSearchPath( fullLocationPath, "MOD", PATH_ADD_TO_TAIL );
			Q_strncpy( initInfo.m_ModPath, fullLocationPath, sizeof( initInfo.m_ModPath ) );
		}
	
		if ( bDoAllPaths )
		{
			// add the game bin
			AddGameBinDir( initInfo.m_pFileSystem, fullLocationPath );
		}
	}

	initInfo.m_pFileSystem->AddSearchPath( fullLocationPath, pPathID, PATH_ADD_TO_TAIL );
}


bool FileSystem_IsHldsUpdateToolDedicatedServer()
{
	// To determine this, we see if the directory our executable was launched from is "orangebox".
	// We only are under "orangebox" if we're run from hldsupdatetool.
	char baseDir[MAX_PATH];
	if ( !FileSystem_GetBaseDir( baseDir, sizeof( baseDir ) ) )
		return false;

	V_FixSlashes( baseDir );
	V_StripTrailingSlash( baseDir );
	const char *pLastDir = V_UnqualifiedFileName( baseDir );
	return ( pLastDir && V_stricmp( pLastDir, "orangebox" ) == 0 );
}

#ifdef ENGINE_DLL
	extern void FileSystem_UpdateAddonSearchPaths( IFileSystem *pFileSystem );
#endif

FSReturnCode_t FileSystem_LoadSearchPaths( CFSSearchPathsInit &initInfo )
{
	if ( !initInfo.m_pFileSystem || !initInfo.m_pDirectoryName )
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_LoadSearchPaths: Invalid parameters specified." );

	KeyValues *pMainFile, *pFileSystemInfo, *pSearchPaths;
	FSReturnCode_t retVal = LoadGameInfoFile( initInfo.m_pDirectoryName, pMainFile, pFileSystemInfo, pSearchPaths );
	if ( retVal != FS_OK )
		return retVal;
	
	// All paths except those marked with |gameinfo_path| are relative to the base dir.
	char baseDir[MAX_PATH];
	if ( !FileSystem_GetBaseDir( baseDir, sizeof( baseDir ) ) )
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetBaseDir failed." );

	initInfo.m_ModPath[0] = 0;

	#define GAMEINFOPATH_TOKEN		"|gameinfo_path|"
	#define BASESOURCEPATHS_TOKEN	"|all_source_engine_paths|"

	bool bLowViolence = IsLowViolenceBuild();
	bool bFirstGamePath = true;
	
	for ( KeyValues *pCur=pSearchPaths->GetFirstValue(); pCur; pCur=pCur->GetNextValue() )
	{
		const char *pPathID = pCur->GetName();
		const char *pLocation = pCur->GetString();
		
		if ( Q_stristr( pLocation, GAMEINFOPATH_TOKEN ) == pLocation )
		{
			pLocation += strlen( GAMEINFOPATH_TOKEN );
			FileSystem_AddLoadedSearchPath( initInfo, pPathID, &bFirstGamePath, initInfo.m_pDirectoryName, pLocation, bLowViolence );
		}
		else if ( Q_stristr( pLocation, BASESOURCEPATHS_TOKEN ) == pLocation )
		{
			// This is a special identifier that tells it to add the specified path for all source engine versions equal to or prior to this version.
			// So in Orange Box, if they specified:
			//		|all_source_engine_paths|hl2
			// it would add the ep2\hl2 folder and the base (ep1-era) hl2 folder.
			//
			// We need a special identifier in the gameinfo.txt here because the base hl2 folder exists in different places.
			// In the case of a game or a Steam-launched dedicated server, all the necessary prior engine content is mapped in with the Steam depots,
			// so we can just use the path as-is.

			// In the case of an hldsupdatetool dedicated server, the base hl2 folder is "..\..\hl2" (since we're up in the 'orangebox' folder).
												   
			pLocation += strlen( BASESOURCEPATHS_TOKEN );

			// Add the Orange-box path (which also will include whatever the depots mapped in as well if we're 
			// running a Steam-launched app).
			FileSystem_AddLoadedSearchPath( initInfo, pPathID, &bFirstGamePath, baseDir, pLocation, bLowViolence );

			if ( FileSystem_IsHldsUpdateToolDedicatedServer() )
			{			
				// If we're using the hldsupdatetool dedicated server, then go up a directory to get the ep1-era files too.
				char ep1EraPath[MAX_PATH];
				V_snprintf( ep1EraPath, sizeof( ep1EraPath ), "..%c%s", CORRECT_PATH_SEPARATOR, pLocation );
				FileSystem_AddLoadedSearchPath( initInfo, pPathID, &bFirstGamePath, baseDir, ep1EraPath, bLowViolence );
			}
		}
		else
		{
			FileSystem_AddLoadedSearchPath( initInfo, pPathID, &bFirstGamePath, baseDir, pLocation, bLowViolence );
		}
	}

#ifdef _PS3
	// Always base GAMEBIN PRX location off of main PRX path (unless content and PRX paths are same)
	if ( Q_strncmp( g_pPS3PathInfo->GameImagePath(), g_pPS3PathInfo->PrxPath(), Q_strlen( g_pPS3PathInfo->GameImagePath() ) ) )
	{
		char gamebindir[CELL_GAME_PATH_MAX];
		// get the moddirname
		const char *pModName;
		for ( pModName = initInfo.m_ModPath + strlen(initInfo.m_ModPath) - 1 ;
			  pModName > initInfo.m_ModPath && *(pModName-1) != '/' ;
			  pModName--);

		V_snprintf( gamebindir, CELL_GAME_PATH_MAX, "%s/../%s", g_pPS3PathInfo->PrxPath(), pModName );
		AddGameBinDir( initInfo.m_pFileSystem, gamebindir, true );
	}
#endif

	pMainFile->deleteThis();

	//
	// Set up search paths for add-ons
	//
	if ( IsPC() )
	{
#ifdef ENGINE_DLL
		FileSystem_UpdateAddonSearchPaths( initInfo.m_pFileSystem );
#endif
	}

	// Add the "platform" directory as a game searchable path
	char pPlatformPath[MAX_PATH];
	V_ComposeFileName( baseDir, "platform", pPlatformPath, sizeof(pPlatformPath) );
	initInfo.m_pFileSystem->AddSearchPath( pPlatformPath, "GAME", PATH_ADD_TO_TAIL );

	// these specialized tool paths are not used on 360 and cause a costly constant perf tax, so inhibited
	if ( IsPC() )
	{
		// Create a content search path based on the game search path
		char szContentRoot[MAX_PATH];
		V_strncpy( szContentRoot, baseDir, sizeof(szContentRoot) );
		char *pRootEnd = V_strrchr( szContentRoot, '\\' );
		if ( pRootEnd )
		{
			*pRootEnd = '\0';
		}

		V_strncat( szContentRoot, "\\content", sizeof( szContentRoot ) );

		int nLen = initInfo.m_pFileSystem->GetSearchPath( "GAME", false, NULL, 0 );
		char *pSearchPath = (char*)stackalloc( nLen * sizeof(char) );
		initInfo.m_pFileSystem->GetSearchPath( "GAME", false, pSearchPath, nLen );
		char *pPath = pSearchPath;
		while( pPath )
		{
			char *pSemiColon = strchr( pPath, ';' );
			if ( pSemiColon )
			{
				*pSemiColon = 0;
			}

			Q_StripTrailingSlash( pPath );
			Q_FixSlashes( pPath );

			const char *pCurPath = pPath;
			pPath = pSemiColon ? pSemiColon + 1 : NULL;

			char pRelativePath[MAX_PATH];
			char pContentPath[MAX_PATH];
			if ( !Q_MakeRelativePath( pCurPath, baseDir, pRelativePath, sizeof(pRelativePath) ) )
				continue;

			Q_ComposeFileName( szContentRoot, pRelativePath, pContentPath, sizeof(pContentPath) );
			initInfo.m_pFileSystem->AddSearchPath( pContentPath, "CONTENT" );
		}

		// Also, mark specific path IDs as "by request only". That way, we won't waste time searching in them
		// when people forget to specify a search path.
		initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "content", true );
	}

	// Also, mark specific path IDs as "by request only". That way, we won't waste time searching in them
	// when people forget to specify a search path.
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "executable_path", true );
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "gamebin", true );
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "mod", true );

	// Add the write path last.
	if ( initInfo.m_ModPath[0] != 0 )
	{
		initInfo.m_pFileSystem->AddSearchPath( initInfo.m_ModPath, "DEFAULT_WRITE_PATH", PATH_ADD_TO_TAIL );
	}

#ifdef _DEBUG	
	initInfo.m_pFileSystem->PrintSearchPaths();
#endif

#if defined( ENABLE_RUNTIME_STACK_TRANSLATION ) && !defined( _GAMECONSOLE )
	//copy search paths to stack tools so it can grab pdb's from all over. But only on P4 or Steam Beta builds
	if( (CommandLine()->FindParm( "-steam" ) == 0) || //not steam
		(CommandLine()->FindParm( "-internalbuild" ) != 0) ) //steam beta is ok
	{
		char szSearchPaths[4096];
		//int CBaseFileSystem::GetSearchPath( const char *pathID, bool bGetPackFiles, char *pPath, int nMaxLen )
		int iLength1 = initInfo.m_pFileSystem->GetSearchPath( "EXECUTABLE_PATH", false, szSearchPaths, 4096 );
		if( iLength1 == 1 )
			iLength1 = 0;

		int iLength2 = initInfo.m_pFileSystem->GetSearchPath( "GAMEBIN", false, szSearchPaths + iLength1, 4096 - iLength1 );
		if( (iLength2 > 1) && (iLength1 > 1) )
		{
			szSearchPaths[iLength1 - 1] = ';'; //replace first null terminator
		}

		const char *szAdditionalPath = CommandLine()->ParmValue( "-AdditionalPDBSearchPath" );
		if( szAdditionalPath && szAdditionalPath[0] )
		{
			int iLength = iLength1;
			if( iLength2 > 1 )
				iLength += iLength2;

			if( iLength != 0 )
			{
				szSearchPaths[iLength - 1] = ';'; //replaces null terminator
			}
			V_strncpy( &szSearchPaths[iLength], szAdditionalPath, 4096 - iLength );			
		}

		//Append the perforce symbol server last. Documentation says that "srv*\\perforce\symbols" should work, but it doesn't.
		//"symsrv*symsrv.dll*\\perforce\symbols" which the docs say is the same statement, works.
		{
			V_strncat( szSearchPaths, ";symsrv*symsrv.dll*\\\\perforce\\symbols", 4096 );
		}

		SetStackTranslationSymbolSearchPath( szSearchPaths );
		//MessageBox( NULL, szSearchPaths, "Search Paths", 0 );
	}
#endif

	return FS_OK;
}

bool DoesFileExistIn( const char *pDirectoryName, const char *pFilename )
{
	char filename[MAX_PATH];

	Q_strncpy( filename, pDirectoryName, sizeof( filename ) );
	Q_AppendSlash( filename, sizeof( filename ) );
	Q_strncat( filename, pFilename, sizeof( filename ), COPY_ALL_CHARACTERS );
	Q_FixSlashes( filename );
#ifdef _PS3
	Assert( 0 );
	bool bExist = false;
#else  // !_PS3
	bool bExist = ( _access( filename, 0 ) == 0 );
#endif // _PS3

	return ( bExist );
}

namespace
{
	SuggestGameInfoDirFn_t & GetSuggestGameInfoDirFn( void )
	{
		static SuggestGameInfoDirFn_t s_pfnSuggestGameInfoDir = NULL;
		return s_pfnSuggestGameInfoDir;
	}
}; // `anonymous` namespace

SuggestGameInfoDirFn_t SetSuggestGameInfoDirFn( SuggestGameInfoDirFn_t pfnNewFn )
{
	SuggestGameInfoDirFn_t &rfn = GetSuggestGameInfoDirFn();
	SuggestGameInfoDirFn_t pfnOldFn = rfn;
	rfn = pfnNewFn;
	return pfnOldFn;
}

static FSReturnCode_t TryLocateGameInfoFile( char *pOutDir, int outDirLen, bool bBubbleDir )
{
	// Retain a copy of suggested path for further attempts
	CArrayAutoPtr < char > spchCopyNameBuffer( new char [ outDirLen ] );
	Q_strncpy( spchCopyNameBuffer.Get(), pOutDir, outDirLen );
	spchCopyNameBuffer[ outDirLen - 1 ] = 0;

	// Make appropriate slashes ('/' - Linux style)
	for ( char *pchFix = spchCopyNameBuffer.Get(),
		*pchEnd = pchFix + outDirLen;
		pchFix < pchEnd; ++ pchFix )
	{
		if ( '\\' == *pchFix )
		{
			*pchFix = '/';
		}
	}

	// Have a look in supplied path
	do
	{
		if ( DoesFileExistIn( pOutDir, GAMEINFO_FILENAME ) )
		{
			return FS_OK;
		}
		if ( IsX360() && DoesFileExistIn( pOutDir, GAMEINFO_FILENAME_ALTERNATE ) ) 
		{
			return FS_OK;
		}
	} 
	while ( bBubbleDir && Q_StripLastDir( pOutDir, outDirLen ) );

	// Make an attempt to resolve from "content -> game" directory
	Q_strncpy( pOutDir, spchCopyNameBuffer.Get(), outDirLen );
	pOutDir[ outDirLen - 1 ] = 0;
	if ( char *pchContentFix = Q_stristr( pOutDir, "/content/" ) )
	{
		V_strcpy( pchContentFix, "/game/" );
		memmove( pchContentFix + 6, pchContentFix + 9, pOutDir + outDirLen - (pchContentFix + 9) );

		// Try in the mapped "game" directory
		do
		{
			if ( DoesFileExistIn( pOutDir, GAMEINFO_FILENAME ) )
			{
				return FS_OK;
			}
			if ( IsX360() && DoesFileExistIn( pOutDir, GAMEINFO_FILENAME_ALTERNATE ) )
			{
				return FS_OK;
			}
		} 
		while ( bBubbleDir && Q_StripLastDir( pOutDir, outDirLen ) );
	}

	// Could not find it here
	return FS_MISSING_GAMEINFO_FILE;
}

FSReturnCode_t LocateGameInfoFile( const CFSSteamSetupInfo &fsInfo, char *pOutDir, int outDirLen )
{
	// Engine and Hammer don't want to search around for it.
	if ( fsInfo.m_bOnlyUseDirectoryName )
	{
		if ( !fsInfo.m_pDirectoryName )
			return SetupFileSystemError( false, FS_MISSING_GAMEINFO_FILE, "bOnlyUseDirectoryName=1 and pDirectoryName=NULL." );

		bool bExists = DoesFileExistIn( fsInfo.m_pDirectoryName, GAMEINFO_FILENAME );
		if ( IsX360() && !bExists )
		{
			bExists = DoesFileExistIn( fsInfo.m_pDirectoryName, GAMEINFO_FILENAME_ALTERNATE );
		}
		if ( !bExists )
		{
			if ( IsX360() && CommandLine()->FindParm( "-basedir" ) )
			{
				char basePath[MAX_PATH];
				strcpy( basePath, CommandLine()->ParmValue( "-basedir", "" ) );
				Q_AppendSlash( basePath, sizeof( basePath ) );
				Q_strncat( basePath, fsInfo.m_pDirectoryName, sizeof( basePath ), COPY_ALL_CHARACTERS );
				if ( DoesFileExistIn( basePath, GAMEINFO_FILENAME ) )
				{
					Q_strncpy( pOutDir, basePath, outDirLen );
					return FS_OK;
				}
				if ( IsX360() && DoesFileExistIn( basePath, GAMEINFO_FILENAME_ALTERNATE ) )
				{
					Q_strncpy( pOutDir, basePath, outDirLen );
					return FS_OK;
				}
			}

			return SetupFileSystemError( true, FS_MISSING_GAMEINFO_FILE, "Setup file '%s' doesn't exist in subdirectory '%s'.\nCheck your -game parameter or VCONFIG setting.", GAMEINFO_FILENAME, fsInfo.m_pDirectoryName );
		}

		Q_strncpy( pOutDir, fsInfo.m_pDirectoryName, outDirLen );
		return FS_OK;
	}

	// First, check for overrides on the command line or environment variables.
	const char *pProject = GetVProjectCmdLineValue();

	if ( pProject )
	{
		if ( DoesFileExistIn( pProject, GAMEINFO_FILENAME ) )
		{
			Q_MakeAbsolutePath( pOutDir, outDirLen, pProject );
			return FS_OK;
		}
		if ( IsX360() && DoesFileExistIn( pProject, GAMEINFO_FILENAME_ALTERNATE ) )	
		{
			Q_MakeAbsolutePath( pOutDir, outDirLen, pProject );
			return FS_OK;
		}

		if ( IsX360() && CommandLine()->FindParm( "-basedir" ) )
		{
			char basePath[MAX_PATH];
			strcpy( basePath, CommandLine()->ParmValue( "-basedir", "" ) );
			Q_AppendSlash( basePath, sizeof( basePath ) );
			Q_strncat( basePath, pProject, sizeof( basePath ), COPY_ALL_CHARACTERS );
			if ( DoesFileExistIn( basePath, GAMEINFO_FILENAME ) )
			{
				Q_strncpy( pOutDir, basePath, outDirLen );
				return FS_OK;
			}
			if ( DoesFileExistIn( basePath, GAMEINFO_FILENAME_ALTERNATE ) )
			{
				Q_strncpy( pOutDir, basePath, outDirLen );
				return FS_OK;
			}
		}
		
		if ( fsInfo.m_bNoGameInfo )
		{
			// fsInfo.m_bNoGameInfo is set by the Steam dedicated server, before it knows which mod to use.
			// Steam dedicated server doesn't need a gameinfo.txt, because we'll ask which mod to use, even if
			// -game is supplied on the command line.
			Q_strncpy( pOutDir, "", outDirLen );
			return FS_OK;
		}
		else
		{
			// They either specified vproject on the command line or it's in their registry. Either way,
			// we don't want to continue if they've specified it but it's not valid.
			goto ShowError;
		}
	}

	if ( fsInfo.m_bNoGameInfo )
	{
		Q_strncpy( pOutDir, "", outDirLen );
		return FS_OK;
	}

	// Ask the application if it can provide us with a game info directory
	{
		bool bBubbleDir = true;
		SuggestGameInfoDirFn_t pfnSuggestGameInfoDirFn = GetSuggestGameInfoDirFn();
		if ( pfnSuggestGameInfoDirFn &&
			( * pfnSuggestGameInfoDirFn )( &fsInfo, pOutDir, outDirLen, &bBubbleDir ) &&
			FS_OK == TryLocateGameInfoFile( pOutDir, outDirLen, bBubbleDir ) )
			return FS_OK;
	}

	// Try to use the environment variable / registry
	if ( ( pProject = getenv( GAMEDIR_TOKEN ) ) != NULL &&
		 ( Q_MakeAbsolutePath( pOutDir, outDirLen, pProject ), 1 ) &&
		 FS_OK == TryLocateGameInfoFile( pOutDir, outDirLen, false ) )
		return FS_OK;

	if ( IsPC() )
	{
		Warning( "Warning: falling back to auto detection of vproject directory.\n" );
		
		// Now look for it in the directory they passed in.
		if ( fsInfo.m_pDirectoryName )
			Q_MakeAbsolutePath( pOutDir, outDirLen, fsInfo.m_pDirectoryName );
		else
			Q_MakeAbsolutePath( pOutDir, outDirLen, "." );

		if ( FS_OK == TryLocateGameInfoFile( pOutDir, outDirLen, true ) )
			return FS_OK;

		// Use the CWD
		Q_getwd( pOutDir, outDirLen );
		if ( FS_OK == TryLocateGameInfoFile( pOutDir, outDirLen, true ) )
			return FS_OK;
	}

ShowError:
	return SetupFileSystemError( true, FS_MISSING_GAMEINFO_FILE, 
		"Unable to find %s. Solutions:\n\n"
		"1. Read http://www.valve-erc.com/srcsdk/faq.html#NoGameDir\n"
		"2. Run vconfig to specify which game you're working on.\n"
		"3. Add -game <path> on the command line where <path> is the directory that %s is in.\n",
		GAMEINFO_FILENAME, GAMEINFO_FILENAME );
}

bool DoesPathExistAlready( const char *pPathEnvVar, const char *pTestPath )
{
	// Fix the slashes in the input arguments.
	char correctedPathEnvVar[8192], correctedTestPath[MAX_PATH];
	Q_strncpy( correctedPathEnvVar, pPathEnvVar, sizeof( correctedPathEnvVar ) );
	Q_FixSlashes( correctedPathEnvVar );
	pPathEnvVar = correctedPathEnvVar;

	Q_strncpy( correctedTestPath, pTestPath, sizeof( correctedTestPath ) );
	Q_FixSlashes( correctedTestPath );
	if ( strlen( correctedTestPath ) > 0 && PATHSEPARATOR( correctedTestPath[strlen(correctedTestPath)-1] ) )
		correctedTestPath[ strlen(correctedTestPath) - 1 ] = 0;

	pTestPath = correctedTestPath;

	const char *pCurPos = pPathEnvVar;
	while ( 1 )
	{
		const char *pTestPos = Q_stristr( pCurPos, pTestPath );
		if ( !pTestPos )
			return false;

		// Ok, we found pTestPath in the path, but it's only valid if it's followed by an optional slash and a semicolon.
		pTestPos += strlen( pTestPath );
		if ( pTestPos[0] == 0 || pTestPos[0] == ';' || (PATHSEPARATOR( pTestPos[0] ) && pTestPos[1] == ';') )
			return true;
	
		// Advance our marker..
		pCurPos = pTestPos;
	}
}

FSReturnCode_t SetSteamInstallPath( char *steamInstallPath, int steamInstallPathLen, CSteamEnvVars &steamEnvVars, bool bErrorsAsWarnings )
{
	if ( IsGameConsole() )
	{
		// consoles don't use steam
		return FS_MISSING_STEAM_DLL;
	}

	if ( IsPosix() )
		return FS_OK; // under posix the content does not live with steam.dll up the path, rely on the environment already being set by steam
	
	// Start at our bin directory and move up until we find a directory with steam.dll in it.
	char executablePath[MAX_PATH];
	if ( !FileSystem_GetExecutableDir( executablePath, sizeof( executablePath ) )	)
	{
		if ( bErrorsAsWarnings )
		{
			Warning( "SetSteamInstallPath: FileSystem_GetExecutableDir failed.\n" );
			return FS_INVALID_PARAMETERS;
		}
		else
		{
			return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetExecutableDir failed." );
		}
	}

	Q_strncpy( steamInstallPath, executablePath, steamInstallPathLen );
#ifdef WIN32
	const char *pchSteamDLL = "steam" DLL_EXT_STRING;
#elif defined(OSX) || defined(LINUX)
	const char *pchSteamDLL = "libsteam" DLL_EXT_STRING;

	// under Linux & OSX the bin lives in the bin/ folder, so step back one

	Q_StripLastDir( steamInstallPath, steamInstallPathLen );
#elif defined( _PS3 )
	const char *pchSteamDLL = "steam_ps3.ps3";
#else
#error
#endif
	while ( 1 )
	{
		// Ignore steamapp.cfg here in case they're debugging. We still need to know the real steam path so we can find their username.
		// find 
		if ( DoesFileExistIn( steamInstallPath, pchSteamDLL ) && !DoesFileExistIn( steamInstallPath, "steamapp.cfg" ) )
			break;
	
		if ( !Q_StripLastDir( steamInstallPath, steamInstallPathLen ) )
		{
			if ( bErrorsAsWarnings )
			{
				Warning( "Can't find %s relative to executable path: %s.\n", pchSteamDLL, executablePath );
				return FS_MISSING_STEAM_DLL;
			}
			else
			{
				return SetupFileSystemError( false, FS_MISSING_STEAM_DLL, "Can't find %s relative to executable path: %s.", pchSteamDLL, executablePath );
			}
		}			
	}

	// Also, add the install path to their PATH environment variable, so filesystem_steam.dll can get to steam.dll.
	char szPath[ 8192 ];
	steamEnvVars.m_Path.GetValue( szPath, sizeof( szPath ) );
	if ( !DoesPathExistAlready( szPath, steamInstallPath ) )
	{
#ifdef WIN32
#define PATH_SEP ";"
#else
#define PATH_SEP ":"
#endif	
		steamEnvVars.m_Path.SetValue( "%s%s%s", szPath, PATH_SEP, steamInstallPath );
	}
	return FS_OK;
}

FSReturnCode_t GetSteamCfgPath( char *steamCfgPath, int steamCfgPathLen )
{
	steamCfgPath[0] = 0;
	char executablePath[MAX_PATH];
	if ( !FileSystem_GetExecutableDir( executablePath, sizeof( executablePath ) ) )
	{
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetExecutableDir failed." );
	}
	Q_strncpy( steamCfgPath, executablePath, steamCfgPathLen );
	while ( 1 )
	{
		if ( DoesFileExistIn( steamCfgPath, "steam.cfg" ) )
			break;
	
		if ( !Q_StripLastDir( steamCfgPath, steamCfgPathLen) )
		{
			// the file isnt found, thats ok, its not mandatory
			return FS_OK;
		}			
	}
	Q_AppendSlash( steamCfgPath, steamCfgPathLen );
	Q_strncat( steamCfgPath, "steam.cfg", steamCfgPathLen, COPY_ALL_CHARACTERS );

	return FS_OK;
}

void SetSteamAppUser( KeyValues *pSteamInfo, const char *steamInstallPath, CSteamEnvVars &steamEnvVars )
{
	// Always inherit the Steam user if it's already set, since it probably means we (or the
	// the app that launched us) were launched from Steam.
	char appUser[MAX_PATH];
	if ( steamEnvVars.m_SteamAppUser.GetValue( appUser, sizeof( appUser ) ) )
		return;

	const char *pTempAppUser = NULL;
	if ( pSteamInfo && (pTempAppUser = pSteamInfo->GetString( "SteamAppUser", NULL )) != NULL )
	{
		Q_strncpy( appUser, pTempAppUser, sizeof( appUser ) );
	}
	else
	{
		// They don't have SteamInfo.txt, or it's missing SteamAppUser. Try to figure out the user
		// by looking in <steam install path>\config\SteamAppData.vdf.
		char fullFilename[MAX_PATH];
		Q_strncpy( fullFilename, steamInstallPath, sizeof( fullFilename ) );
		Q_AppendSlash( fullFilename, sizeof( fullFilename ) );
		Q_strncat( fullFilename, "config\\SteamAppData.vdf", sizeof( fullFilename ), COPY_ALL_CHARACTERS );

		KeyValues *pSteamAppData = ReadKeyValuesFile( fullFilename );
		if ( !pSteamAppData || (pTempAppUser = pSteamAppData->GetString( "AutoLoginUser", NULL )) == NULL )
		{
			Error( "Can't find steam app user info." );
		}
		Q_strncpy( appUser, pTempAppUser, sizeof( appUser ) ); 
		
		pSteamAppData->deleteThis();
	}

	Q_strlower( appUser );
	steamEnvVars.m_SteamAppUser.SetValue( "%s", appUser );
}

void SetSteamUserPassphrase( KeyValues *pSteamInfo, CSteamEnvVars &steamEnvVars )
{
	// Always inherit the passphrase if it's already set, since it probably means we (or the
	// the app that launched us) were launched from Steam.
	char szPassPhrase[ MAX_PATH ];
	if ( steamEnvVars.m_SteamUserPassphrase.GetValue( szPassPhrase, sizeof( szPassPhrase ) ) )
		return;

	// SteamUserPassphrase.
	const char *pStr;
	if ( pSteamInfo && (pStr = pSteamInfo->GetString( "SteamUserPassphrase", NULL )) != NULL )
	{
		steamEnvVars.m_SteamUserPassphrase.SetValue( "%s", pStr );
	}
}

void SetSteamAppId( KeyValues *pFileSystemInfo, const char *pGameInfoDirectory, CSteamEnvVars &steamEnvVars )
{
	// SteamAppId is in gameinfo.txt->FileSystem->FileSystemInfo_Steam->SteamAppId.
	int iAppId = pFileSystemInfo->GetInt( "SteamAppId", -1 );
	if ( iAppId == -1 )
		Error( "Missing SteamAppId in %s\\%s.", pGameInfoDirectory, GAMEINFO_FILENAME );

	steamEnvVars.m_SteamAppId.SetValue( "%d", iAppId );
}

FSReturnCode_t SetupSteamStartupEnvironment( KeyValues *pFileSystemInfo, const char *pGameInfoDirectory, CSteamEnvVars &steamEnvVars )
{
	// Ok, we're going to run Steam. See if they have SteamInfo.txt. If not, we'll try to deduce what we can.
	char steamInfoFile[MAX_PATH];
	Q_strncpy( steamInfoFile, pGameInfoDirectory, sizeof( steamInfoFile ) );
	Q_AppendSlash( steamInfoFile, sizeof( steamInfoFile ) );
	Q_strncat( steamInfoFile, "steaminfo.txt", sizeof( steamInfoFile ), COPY_ALL_CHARACTERS );
	KeyValues *pSteamInfo = ReadKeyValuesFile( steamInfoFile );

	char steamInstallPath[MAX_PATH];
	FSReturnCode_t ret = SetSteamInstallPath( steamInstallPath, sizeof( steamInstallPath ), steamEnvVars, false );
	if ( ret != FS_OK )
		return ret;

	SetSteamAppUser( pSteamInfo, steamInstallPath, steamEnvVars );
	SetSteamUserPassphrase( pSteamInfo, steamEnvVars );
	SetSteamAppId( pFileSystemInfo, pGameInfoDirectory, steamEnvVars );

	if ( pSteamInfo )
		pSteamInfo->deleteThis();

	return FS_OK;
}

FSReturnCode_t GetSteamExtraAppId( const char *pDirectoryName, int *nExtraAppId )
{
	// Now, load gameinfo.txt (to make sure it's there)
	KeyValues *pMainFile = NULL, *pFileSystemInfo = NULL, *pSearchPaths = NULL;
	FSReturnCode_t ret = LoadGameInfoFile( pDirectoryName, pMainFile, pFileSystemInfo, pSearchPaths );
	if ( ret != FS_OK )
		return ret;

	*nExtraAppId = pFileSystemInfo->GetInt( "ToolsAppId", -1 );
	pMainFile->deleteThis();
	return FS_OK;
}

FSReturnCode_t FileSystem_SetBasePaths( IFileSystem *pFileSystem )
{
	pFileSystem->RemoveSearchPaths( "EXECUTABLE_PATH" );

#ifndef _PS3
	char executablePath[MAX_PATH];
	if ( !FileSystem_GetExecutableDir( executablePath, sizeof( executablePath ) )	)
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetExecutableDir failed." );

	pFileSystem->AddSearchPath( executablePath, "EXECUTABLE_PATH" );
#endif
	return FS_OK;
}

//-----------------------------------------------------------------------------
// Returns the name of the file system DLL to use
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_GetFileSystemDLLName( char *pFileSystemDLL, int nMaxLen, bool &bSteam )
{
	bSteam = false;

	// Inside of here, we don't have a filesystem yet, so we have to assume that the filesystem_stdio or filesystem_steam
	// is in this same directory with us.
	char executablePath[MAX_PATH];
	if ( !FileSystem_GetExecutableDir( executablePath, sizeof( executablePath ) )	)
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetExecutableDir failed." );
	
#if defined( _PS3 )
	Q_snprintf( pFileSystemDLL, nMaxLen, "%s%cfilesystem_stdio" DLL_EXT_STRING, g_pPS3PathInfo->PrxPath(), CORRECT_PATH_SEPARATOR );
#else
	Q_snprintf( pFileSystemDLL, nMaxLen, "%s%cfilesystem_stdio" DLL_EXT_STRING, executablePath, CORRECT_PATH_SEPARATOR );
#endif

	return FS_OK;
}

//-----------------------------------------------------------------------------
// Sets up the steam.dll install path in our PATH env var (so you can then just 
// LoadLibrary() on filesystem_steam.dll without having to copy steam.dll anywhere special )
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_SetupSteamInstallPath()
{
	CSteamEnvVars steamEnvVars;
	char steamInstallPath[MAX_PATH];
	FSReturnCode_t ret = SetSteamInstallPath( steamInstallPath, sizeof( steamInstallPath ), steamEnvVars, true );
	steamEnvVars.m_Path.SetRestoreOriginalValue( false ); // We want to keep the change to the path going forward.
	return ret;
}

//-----------------------------------------------------------------------------
// Sets up the steam environment + gets back the gameinfo.txt path
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_SetupSteamEnvironment( CFSSteamSetupInfo &fsInfo )
{
	// First, locate the directory with gameinfo.txt.
	FSReturnCode_t ret = LocateGameInfoFile( fsInfo, fsInfo.m_GameInfoPath, sizeof( fsInfo.m_GameInfoPath ) );
	if ( ret != FS_OK )
		return ret;

	// This is so that processes spawned by this application will have the same VPROJECT
#if defined( WIN32 ) || defined( _GAMECONSOLE )
	char pEnvBuf[MAX_PATH+32];
	Q_snprintf( pEnvBuf, sizeof(pEnvBuf), "%s=%s", GAMEDIR_TOKEN, fsInfo.m_GameInfoPath );
	_putenv( pEnvBuf );
#else
	setenv( GAMEDIR_TOKEN, fsInfo.m_GameInfoPath, 1 );
#endif
	
	CSteamEnvVars steamEnvVars;
	if ( fsInfo.m_bSteam )
	{
		if ( fsInfo.m_bToolsMode )
		{
			// Now, load gameinfo.txt (to make sure it's there)
			KeyValues *pMainFile, *pFileSystemInfo, *pSearchPaths;
			ret = LoadGameInfoFile( fsInfo.m_GameInfoPath, pMainFile, pFileSystemInfo, pSearchPaths );
			if ( ret != FS_OK )
				return ret;


			// Setup all the environment variables related to Steam so filesystem_steam.dll knows how to initialize Steam.
			ret = SetupSteamStartupEnvironment( pFileSystemInfo, fsInfo.m_GameInfoPath, steamEnvVars );
			if ( ret != FS_OK )
				return ret;

			steamEnvVars.m_SteamAppId.SetRestoreOriginalValue( false ); // We want to keep the change to the path going forward.

			// We're done with main file
			pMainFile->deleteThis();
		}
		else if ( fsInfo.m_bSetSteamDLLPath )
		{
			// This is used by the engine to automatically set the path to their steam.dll when running the engine,
			// so they can debug it without having to copy steam.dll up into their hl2.exe folder.
			char steamInstallPath[MAX_PATH];
			ret = SetSteamInstallPath( steamInstallPath, sizeof( steamInstallPath ), steamEnvVars, true );
			steamEnvVars.m_Path.SetRestoreOriginalValue( false ); // We want to keep the change to the path going forward.
		}
	}

	return FS_OK;
}


//-----------------------------------------------------------------------------
// Loads the file system module
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_LoadFileSystemModule( CFSLoadModuleInfo &fsInfo )
{
	// First, locate the directory with gameinfo.txt.
	FSReturnCode_t ret = FileSystem_SetupSteamEnvironment( fsInfo );
	if ( ret != FS_OK )
		return ret;

	// Now that the environment is setup, load the filesystem module.
	if ( !Sys_LoadInterface(
		fsInfo.m_pFileSystemDLLName,
		FILESYSTEM_INTERFACE_VERSION,
		&fsInfo.m_pModule,
		(void**)&fsInfo.m_pFileSystem ) )
	{
		return SetupFileSystemError( false, FS_UNABLE_TO_INIT, "Can't load %s.", fsInfo.m_pFileSystemDLLName );
	}

	if ( !fsInfo.m_pFileSystem->Connect( fsInfo.m_ConnectFactory ) )
		return SetupFileSystemError( false, FS_UNABLE_TO_INIT, "%s IFileSystem::Connect failed.", fsInfo.m_pFileSystemDLLName );

	if ( fsInfo.m_pFileSystem->Init() != INIT_OK )
		return SetupFileSystemError( false, FS_UNABLE_TO_INIT, "%s IFileSystem::Init failed.", fsInfo.m_pFileSystemDLLName );

	return FS_OK;
}


//-----------------------------------------------------------------------------
// Mounds a particular steam cache
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_MountContent( CFSMountContentInfo &mountContentInfo )
{
	// This part is Steam-only.
	if ( mountContentInfo.m_pFileSystem->IsSteam() )
	{
		// Find out the "extra app id". This is for tools, which want to mount a base app's filesystem
		// like HL2, then mount the SDK content (tools materials and models, etc) in addition.
		int nExtraAppId = -1;
		if ( mountContentInfo.m_bToolsMode )
		{
			FSReturnCode_t ret = GetSteamExtraAppId( mountContentInfo.m_pDirectoryName, &nExtraAppId );
			if ( ret != FS_OK )
				return ret;
		}

		// Set our working directory temporarily so Steam can remember it.
		// This is what Steam strips off absolute filenames like c:\program files\valve\steam\steamapps\username\sourcesdk
		// to get to the relative part of the path.
		char baseDir[MAX_PATH], oldWorkingDir[MAX_PATH];
		if ( !FileSystem_GetBaseDir( baseDir, sizeof( baseDir ) ) )
			return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetBaseDir failed." );

		Q_getwd( oldWorkingDir, sizeof( oldWorkingDir ) );
		_chdir( baseDir );

		// Filesystem_tools needs to add dependencies in here beforehand.
		FilesystemMountRetval_t retVal = mountContentInfo.m_pFileSystem->MountSteamContent( nExtraAppId );
		
		_chdir( oldWorkingDir );

		if ( retVal != FILESYSTEM_MOUNT_OK )
			return SetupFileSystemError( true, FS_UNABLE_TO_INIT, "Unable to mount Steam content in the file system" );
	}

	return FileSystem_SetBasePaths( mountContentInfo.m_pFileSystem );
}

void FileSystem_SetErrorMode( FSErrorMode_t errorMode )
{
	g_FileSystemErrorMode = errorMode;
}

void FileSystem_ClearSteamEnvVars()
{
	CSteamEnvVars envVars;

	// Change the values and don't restore the originals.
	envVars.m_SteamAppId.SetValue( "" );
	envVars.m_SteamUserPassphrase.SetValue( "" );
	envVars.m_SteamAppUser.SetValue( "" );
	
	envVars.SetRestoreOriginalValue_ALL( false );
}

//-----------------------------------------------------------------------------
// Adds the platform folder to the search path.
//-----------------------------------------------------------------------------
void FileSystem_AddSearchPath_Platform( IFileSystem *pFileSystem, const char *szGameInfoPath )
{
	char platform[MAX_PATH];
	if ( pFileSystem->IsSteam() )
	{
		// Steam doesn't support relative paths
		Q_strncpy( platform, "platform", MAX_PATH );
	}
	else
	{
		Q_strncpy( platform, szGameInfoPath, MAX_PATH );
		Q_StripTrailingSlash( platform );
#ifdef _PS3
		Q_strncat( platform, "/platform", MAX_PATH, MAX_PATH );
#else // _PS3
		Q_strncat( platform, "/../platform", MAX_PATH, MAX_PATH );
#endif // !_PS3
	}

	pFileSystem->AddSearchPath( platform, "PLATFORM" );
}

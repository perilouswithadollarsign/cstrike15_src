//===== Copyright (c) Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//==================================================================//

#if defined( _WIN32 ) && !defined( _X360 )
#undef PROTECTED_THINGS_ENABLE
#include <windows.h>
#endif
#include "quakedef.h" // for max_ospath
#include <stdlib.h>
#include <assert.h>
#include "filesystem.h"
#include "bitmap/tgawriter.h"
#include <tier2/tier2.h>
#include "filesystem_init.h"
#include "keyvalues.h"
#include "host.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ADDONLIST_FILENAME			"addonlist.txt"
#define ADDONS_DIRNAME				"addons"

IFileSystem *g_pFileSystem = NULL;

// This comes is in filesystem_init.cpp
extern KeyValues* ReadKeyValuesFile( const char *pFilename );

void fs_whitelist_spew_flags_changefn( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	if ( g_pFileSystem )
	{
		ConVarRef var( pConVar );
		g_pFileSystem->SetWhitelistSpewFlags( var.GetInt() );
	}
}

#if defined( _DEBUG )
ConVar fs_whitelist_spew_flags( "fs_whitelist_spew_flags", "0", 0,
	"Set whitelist spew flags to a combination of these values:\n"
	"   0x0001 - list files as they are added to the CRC tracker\n"
	"   0x0002 - show files the filesystem is telling the engine to reload\n"
	"   0x0004 - show files the filesystem is NOT telling the engine to reload",
	fs_whitelist_spew_flags_changefn );
#endif

CON_COMMAND( path, "Show the engine filesystem path." )
{
	if( g_pFileSystem )
	{
		g_pFileSystem->PrintSearchPaths();
	}
}

CON_COMMAND( fs_printopenfiles, "Show all files currently opened by the engine." )
{
	if( g_pFileSystem )
	{
		g_pFileSystem->PrintOpenedFiles();
	}
}

CON_COMMAND( fs_warning_level, "Set the filesystem warning level." )
{
	if( args.ArgC() != 2 )
	{
		Warning( "\"fs_warning_level n\" where n is one of:\n" );
		Warning( "\t0:\tFILESYSTEM_WARNING_QUIET\n" );
		Warning( "\t1:\tFILESYSTEM_WARNING_REPORTUNCLOSED\n" );
		Warning( "\t2:\tFILESYSTEM_WARNING_REPORTUSAGE\n" );
		Warning( "\t3:\tFILESYSTEM_WARNING_REPORTALLACCESSES\n" );
		Warning( "\t4:\tFILESYSTEM_WARNING_REPORTALLACCESSES_READ\n" );
		Warning( "\t5:\tFILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE\n" );
		Warning( "\t6:\tFILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC\n" );
		return;
	}

	int level = atoi( args[ 1 ] );
	switch( level )
	{
	case FILESYSTEM_WARNING_QUIET:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_QUIET\n" );
		break;
	case FILESYSTEM_WARNING_REPORTUNCLOSED:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTUNCLOSED\n" );
		break;
	case FILESYSTEM_WARNING_REPORTUSAGE:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTUSAGE\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES_READ:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES_READ\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC\n" );
		break;

	default:
		Warning( "fs_warning_level = UNKNOWN!!!!!!!\n" );
		return;
		break;
	}
	g_pFileSystem->SetWarningLevel( ( FileWarningLevel_t )level );
}


//-----------------------------------------------------------------------------
// Purpose: Wrap Sys_LoadModule() with a filesystem GetLocalCopy() call to
//			ensure have the file to load when running Steam.
//-----------------------------------------------------------------------------
CSysModule *FileSystem_LoadModule(const char *path)
{
	if ( g_pFileSystem )
		return g_pFileSystem->LoadModule( path );
	else
		return Sys_LoadModule(path);
}

//-----------------------------------------------------------------------------
// Purpose: Provided for symmetry sake with FileSystem_LoadModule()...
//-----------------------------------------------------------------------------
void FileSystem_UnloadModule(CSysModule *pModule)
{
	Sys_UnloadModule(pModule);
}


void FileSystem_SetWhitelistSpewFlags()
{
#if defined( _DEBUG )
	if ( !g_pFileSystem )
	{
		Assert( !"FileSystem_InitSpewFlags - no filesystem." );
		return;
	}
	
	g_pFileSystem->SetWhitelistSpewFlags( fs_whitelist_spew_flags.GetInt() );
#endif
}

CON_COMMAND( fs_syncdvddevcache, "Force the 360 to get updated files that are in your p4 changelist(s) from the host PC when running with -dvddev." )
{
	if( g_pFileSystem )
	{
		g_pFileSystem->SyncDvdDevCache();
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Loads the optional addonlist.txt file which lives in the same location as gameinfo.txt and defines additional search
// paths for content add-ons to mods.
//---------------------------------------------------------------------------------------------------------------------
bool LoadAddonListFile( const char *pDirectoryName, KeyValues *&pAddons )
{
	char addoninfoFilename[MAX_PATH];

	V_snprintf( addoninfoFilename, sizeof( addoninfoFilename), "%s%s", pDirectoryName, ADDONLIST_FILENAME );
	pAddons = ReadKeyValuesFile( addoninfoFilename );

	return ( pAddons != NULL );
}

//---------------------------------------------------------------------------------------------------------------------
// Copies any addons staged under <STEAMDIR>\steamapps\SourceMods\addons\<APPID> to the addons directory
//---------------------------------------------------------------------------------------------------------------------
void CopyStagedAddons( IFileSystem *pFileSystem, const char *pModPath )
{
#if (defined( PLATFORM_WINDOWS ) && !defined( _X360 )  ) || defined( PLATFORM_OSX )

#ifdef IS_WINDOWS_PC
	HKEY hKey;

	// Find the Steam installation path in the registry
	if ( ERROR_SUCCESS == RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) )
	{
		DWORD nReadLength = MAX_PATH;
		char szAddonInstallPath[MAX_PATH];

		if ( ERROR_SUCCESS == RegQueryValueEx( hKey, "SourceModInstallPath", NULL, NULL, (LPBYTE)szAddonInstallPath,  &nReadLength ) )
		{
#else
	{
		{
			char szAddonInstallPath[MAX_PATH];
			char *pszHomeDir = getenv("HOME");
			V_snprintf( szAddonInstallPath, sizeof(szAddonInstallPath), "%s/Library/Application Support/Steam/SteamApps/sourcemods", pszHomeDir );			
#endif
			char szAddonsWildcard[MAX_PATH];
			FileFindHandle_t findHandleDir;
			
			//
			// Loop through the .vpk files in the staged location 
			//
			CUtlVector< CUtlString > vecAddonVPKs;
			V_snprintf( szAddonsWildcard, sizeof( szAddonsWildcard ), "%s%c%s%c%i%c%s", szAddonInstallPath, CORRECT_PATH_SEPARATOR, ADDONS_DIRNAME, 
						CORRECT_PATH_SEPARATOR, GetSteamAppID(), CORRECT_PATH_SEPARATOR, "*.vpk" );
			const char *pFileName = pFileSystem->FindFirst( szAddonsWildcard, &findHandleDir );

			while ( pFileName )
			{	
				char szSrcVPKPath[MAX_PATH];

				V_snprintf( szSrcVPKPath, sizeof( szSrcVPKPath), "%s%c%s%c%i%c%s", szAddonInstallPath, CORRECT_PATH_SEPARATOR, ADDONS_DIRNAME, 
							CORRECT_PATH_SEPARATOR, GetSteamAppID(), CORRECT_PATH_SEPARATOR, pFileName);
				vecAddonVPKs.AddToTail( CUtlString( szSrcVPKPath ) );
				pFileName = pFileSystem->FindNext( findHandleDir );
			}

			pFileSystem->FindClose( findHandleDir );

			//
			// Copy each of the VPKs to the addons directory
			//
			FOR_EACH_VEC( vecAddonVPKs, i )
			{
				char szDestPath[MAX_PATH];

				V_snprintf( szDestPath, sizeof( szDestPath ),"%s%s%c%s", pModPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, V_UnqualifiedFileName( vecAddonVPKs[i] ) );
				pFileSystem->RemoveFile( szDestPath );
				pFileSystem->RenameFile( vecAddonVPKs[i], szDestPath );
			}
		}

#ifdef IS_WINDOWS_PC
		RegCloseKey( hKey );
#endif
	}
#endif
}

//---------------------------------------------------------------------------------------------------------------------
// Reconciles the contents of the addonlist.txt file with the addon folders located under <MODPATH>/addons. If the 
// contains directory names that no longer exist they are removed. If there are directories present that are not in
// the file they are added. Added directories default to the disabled state in the file.
//---------------------------------------------------------------------------------------------------------------------
void ReconcileAddonListFile( IFileSystem *pFileSystem, const char *pModPath )
{
	KeyValues *pAddonList;

	// Load the existing addonlist.txt file 
	LoadAddonListFile( pModPath, pAddonList );

	// If there is no addonlist.txt then create an empty KeyValues
	if ( !pAddonList )
	{
		pAddonList = new KeyValues( "AddonList" );
	}

	// Get the list of subdirectories of addons
	char addonsWildcard[MAX_PATH];

	FileFindHandle_t findHandleDir;

	//
	// Loop through the .vpk files
	//
	CUtlVector< CUtlString > vecAddonVPKs;
	V_snprintf( addonsWildcard, sizeof( addonsWildcard ), "%s%s%c%s", pModPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, "*.vpk" );
	const char *pFileName = pFileSystem->FindFirst( addonsWildcard, &findHandleDir );

	while ( pFileName )
	{	
		vecAddonVPKs.AddToTail( CUtlString( pFileName ) );
		pFileName = pFileSystem->FindNext( findHandleDir );
	}

	pFileSystem->FindClose( findHandleDir );

	//
	// Loop through the loose directories
	//
	CUtlVector< CUtlString > vecAddonDirs;
	V_snprintf( addonsWildcard, sizeof( addonsWildcard ), "%s%s%c%s", pModPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, "*.*" );
	pFileName = pFileSystem->FindFirst( addonsWildcard, &findHandleDir );

	while ( pFileName )
	{	
		// We only want directories that is not already represented by a .vpk and that contains a valid addoninfo.txt
		if ( pFileSystem->FindIsDirectory( findHandleDir ) && ( pFileName[0] != '.' ) )
		{
			char szVPKized[MAX_PATH];

			V_snprintf( szVPKized, sizeof( szVPKized ), "%s.vpk", pFileName );

			if ( !vecAddonVPKs.IsValidIndex( vecAddonVPKs.Find( CUtlString( szVPKized ) ) ) )
			{
				char addonsInfoFile[MAX_PATH];
				FileFindHandle_t findHandleConfig;

				V_snprintf( addonsInfoFile, sizeof( addonsInfoFile ), "%s%s%c%s%c%s", pModPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pFileName, CORRECT_PATH_SEPARATOR, "addoninfo.txt" );

				if ( pFileSystem->FindFirst( addonsInfoFile, &findHandleConfig ) )
				{
					vecAddonDirs.AddToTail( CUtlString( pFileName ) ); 
				}

				pFileSystem->FindClose( findHandleConfig );
			}
		}
		pFileName = pFileSystem->FindNext( findHandleDir );
	}

	pFileSystem->FindClose( findHandleDir );

	// Add missing, existing directories to the KeyValues
	FOR_EACH_VEC( vecAddonDirs, i )
	{
		// We found a directory that wasn't included in the file - add it
		if ( !pAddonList->FindKey( vecAddonDirs[i] ) )
		{
			pAddonList->SetInt( vecAddonDirs[i], 1 );
		}
	}

	// Add missing, existing VPKs to the KeyValues
	FOR_EACH_VEC( vecAddonVPKs, i )
	{
		// We found a VPK that wasn't included in the file - add it
		if ( !pAddonList->FindKey( vecAddonVPKs[i] ) )
		{
			pAddonList->SetInt( vecAddonVPKs[i], 1 );
		}
	}

	// Remove any non-existent directories from the KeyValues
	KeyValues* pIter = pAddonList->GetFirstSubKey();
	CUtlVector<KeyValues*> vecDoomedSubkeys;

	while( pIter )
	{
		if ( !vecAddonDirs.IsValidIndex( vecAddonDirs.Find( CUtlString( pIter->GetName() ) ) ) && 
			!vecAddonVPKs.IsValidIndex( vecAddonVPKs.Find( CUtlString( pIter->GetName() ) ) ) )
		{
			vecDoomedSubkeys.AddToTail( pIter );
		}

		pIter = pIter->GetNextKey();
	} 

	// Now actually delete the missing directories
	FOR_EACH_VEC( vecDoomedSubkeys, j )
	{
		pAddonList->RemoveSubKey( vecDoomedSubkeys[j] );
		vecDoomedSubkeys[j]->deleteThis();
	}

	// Persist and dispose
	char addoninfoFilename[MAX_PATH];

	V_snprintf( addoninfoFilename, sizeof( addoninfoFilename), "%s%s", pModPath, ADDONLIST_FILENAME );

	if ( pAddonList->GetFirstSubKey() )
	{
		pAddonList->SaveToFile( pFileSystem, addoninfoFilename );	
	}
	else
	{
		if ( pFileSystem->FileExists( addoninfoFilename ) )
		{
			pFileSystem->RemoveFile( addoninfoFilename );
		}
	}
	pAddonList->deleteThis();
}

//---------------------------------------------------------------------------------------------------------------------
// Adds enabled addons to the GAME search path after removing any existing addons from the GAME path.
//---------------------------------------------------------------------------------------------------------------------
void FileSystem_UpdateAddonSearchPaths( IFileSystem *pFileSystem )
{
	// Get the path to the mod dir
	char modPath[MAX_PATH];

	pFileSystem->GetSearchPath( "MOD", false, modPath, sizeof( modPath ) );

	//
	// Remove any existing addons from the search path
	//
	char gameSearchPath[10*MAX_PATH];
	char addonSearchString[MAX_PATH];
	CUtlStringList gameSearchPathList;

	// Construct the search string for determining whether the search path component is an add-on
	V_snprintf( addonSearchString, sizeof( addonSearchString ), "%s%s", modPath, ADDONS_DIRNAME );

	pFileSystem->GetSearchPath( "GAME", false, gameSearchPath, sizeof( gameSearchPath ) );
	V_SplitString(gameSearchPath, ";", gameSearchPathList );

	FOR_EACH_VEC( gameSearchPathList, i )
	{
		if ( V_stristr( gameSearchPathList[i], addonSearchString ) )
		{
			pFileSystem->RemoveSearchPath( gameSearchPathList[i], "GAME" );
		}
	}

	// Unmount any VPK addons
	CUtlVector<CUtlString> loadedVPKs;

	pFileSystem->GetVPKFileNames( loadedVPKs );

	FOR_EACH_VEC( loadedVPKs, i )
	{
		if ( V_stristr( loadedVPKs[i], addonSearchString ) )
		{
			pFileSystem->RemoveVPKFile( loadedVPKs[i] );
		}
	}

	//
	// Copy over any addons that were staged by the addon installer
	//
	CopyStagedAddons( pFileSystem, modPath );

	//
	// Reconcile the addons file and add any newly added ones to the list
	//
	ReconcileAddonListFile( pFileSystem, modPath );

	//
	// Add any enabled addons to the GAME search path
	//
	KeyValues *pAddonList;
	if ( LoadAddonListFile( modPath, pAddonList ) )
	{
		for ( KeyValues *pCur=pAddonList->GetFirstValue(); pCur; pCur=pCur->GetNextValue() )
		{
			const char *pszAddonName = pCur->GetName();
			const bool bAddonActivated = pCur->GetInt() != 0;

			if ( bAddonActivated )
			{
				char addOnPath[MAX_PATH];

				V_snprintf( addOnPath, sizeof( addOnPath ), "%s%s%c%s", modPath, ADDONS_DIRNAME, CORRECT_PATH_SEPARATOR, pszAddonName );

				if ( V_stristr( pszAddonName, ".vpk" ) )
				{
					pFileSystem->AddVPKFile( addOnPath, PATH_ADD_TO_TAIL );
				}
				else
				{
					pFileSystem->AddSearchPath( addOnPath, "GAME", PATH_ADD_TO_TAIL );
				}
			}
		}

		pAddonList->deleteThis();
	}

	modelloader->Studio_ReloadModels( IModelLoader::RELOAD_EVERYTHING );
	materials->UncacheAllMaterials();
}
CON_COMMAND( update_addon_paths, "Reloads the search paths for game addons." )
{
	if( g_pFileSystem )
	{
		FileSystem_UpdateAddonSearchPaths( g_pFileSystem );
	}
}

CON_COMMAND( unload_all_addons, "Reloads the search paths for game addons." )
{
	//
	// Unmount any VPK addons
	//
	if( g_pFileSystem )
	{
		char addonSearchString[MAX_PATH];
		char modPath[MAX_PATH];
		CUtlVector<CUtlString> loadedVPKs;

		// Get the path to the mod dir
		g_pFileSystem->GetSearchPath( "MOD", false, modPath, sizeof( modPath ) );

		// Construct the search string for determining whether the search path component is an add-on
		V_snprintf( addonSearchString, sizeof( addonSearchString ), "%s%s", modPath, ADDONS_DIRNAME );

		g_pFileSystem->GetVPKFileNames( loadedVPKs );

		FOR_EACH_VEC( loadedVPKs, i )
		{
			if ( V_stristr( loadedVPKs[i], addonSearchString ) )
			{
				g_pFileSystem->RemoveVPKFile( loadedVPKs[i] );
			}
		}
	}
}



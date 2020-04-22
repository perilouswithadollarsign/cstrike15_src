//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "mathlib/vector.h"
#include "DownloadListGenerator.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "sys.h"
#include "cmd.h"
#include "common.h"
#include "quakedef.h"
#include "vengineserver_impl.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "checksum_engine.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <tier0/dbg.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CDownloadListGenerator g_DownloadListGenerator;
CDownloadListGenerator &DownloadListGenerator()
{
	return g_DownloadListGenerator;
}

ConVar	sv_logdownloadlist( "sv_logdownloadlist", "0" );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDownloadListGenerator::CDownloadListGenerator()
	: m_AlreadyWrittenFileNames( 0, 0, true )
{
	m_hReslistFile = FILESYSTEM_INVALID_HANDLE;
	m_pStringTable = NULL;
	m_mapName[0] = 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDownloadListGenerator::SetStringTable( INetworkStringTable *pStringTable )
{
	if ( IsX360() )
	{
		// not supporting
		return;
	}

	m_pStringTable = pStringTable;

	// reset the duplication list
	m_AlreadyWrittenFileNames.RemoveAll();

	// add in the bsp file to the list, and its node graph and nav mesh
	char path[_MAX_PATH];
	Q_snprintf(path, sizeof(path), "maps\\%s.bsp", m_mapName);
	OnResourcePrecached(path);

	bool useNodeGraph = true;
	KeyValues *modinfo = new KeyValues("ModInfo");
	if ( modinfo->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
	{
		useNodeGraph = modinfo->GetInt( "nodegraph", 1 ) != 0;
	}
	modinfo->deleteThis();

	if ( useNodeGraph )
	{
		Q_snprintf(path, sizeof(path), "maps\\graphs\\%s.ain", m_mapName);
		OnResourcePrecached(path);
	}

	Q_snprintf(path, sizeof(path), "maps\\%s.nav", m_mapName);
	OnResourcePrecached(path);

	char resfilename[MAX_OSPATH];
	Q_snprintf( resfilename, sizeof( resfilename), "maps/%s.res", m_mapName );

	KeyValues::AutoDelete resfilekeys( "resourcefiles" );
	if ( resfilekeys->LoadFromFile( g_pFileSystem, resfilename, "GAME" ) )
	{
		for ( KeyValues *pKey = resfilekeys->GetFirstSubKey(); pKey != NULL; pKey = pKey->GetNextKey() )
		{
			OnResourcePrecached( pKey->GetName() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: call to mark level load/end
//-----------------------------------------------------------------------------
void CDownloadListGenerator::OnLevelLoadStart(const char *levelName)
{
	if ( IsX360() )
	{
		// not supporting
		return;
	}

	// close the previous level reslist, if any
	if (m_hReslistFile != FILESYSTEM_INVALID_HANDLE)
	{
		g_pFileSystem->Close(m_hReslistFile);
		m_hReslistFile = FILESYSTEM_INVALID_HANDLE;
	}

	// reset the duplication list
	m_AlreadyWrittenFileNames.RemoveAll();

	if ( sv_logdownloadlist.GetBool() )
	{
		// open the new level reslist
		char path[MAX_OSPATH];
		g_pFileSystem->CreateDirHierarchy( "DownloadLists", "MOD" );
		Q_snprintf(path, sizeof(path), "DownloadLists/%s.lst", levelName);
		m_hReslistFile = g_pFileSystem->Open(path, "wt", "GAME");
	}

	// add a slash to the end of com_gamedir, so we can only deal with files for this mod
	Q_snprintf( m_gameDir, sizeof(m_gameDir), "%s/", com_gamedir );
	V_FixSlashes( m_gameDir, '/' );

	// save off the map name
	V_strcpy_safe( m_mapName, levelName );
}

//-----------------------------------------------------------------------------
// Purpose: call to mark level load/end
//-----------------------------------------------------------------------------
void CDownloadListGenerator::OnLevelLoadEnd()
{
	if ( IsX360() )
	{
		// not supporting
		return;
	}

	if ( m_hReslistFile != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close(m_hReslistFile);
		m_hReslistFile = FILESYSTEM_INVALID_HANDLE;
	}
	m_pStringTable = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: logs and handles mdl files being precaches
//-----------------------------------------------------------------------------
void CDownloadListGenerator::OnModelPrecached(const char *relativePathFileName)
{
	if ( IsX360() )
	{
		// not supporting
		return;
	}

	if (Q_strstr(relativePathFileName, ".vmt"))
	{
		// it's a materials file, make sure that it starts in the materials directory, and we get the .vtf
		char file[_MAX_PATH];

		if ( StringHasPrefix( relativePathFileName, "materials" ) )
		{
			Q_strncpy(file, relativePathFileName, sizeof(file));
		}
		else
		{
			// prepend the materials directory
			Q_snprintf(file, sizeof(file), "materials\\%s", relativePathFileName);
		}

		OnResourcePrecached(file);

		// get the matching vtf file
		char *ext = Q_strstr(file, ".vmt");
		if (ext)
		{
			Q_strncpy(ext, ".vtf", 5);
			OnResourcePrecached(file);
		}
	}
	else
	{
		OnResourcePrecached(relativePathFileName);
	}
}

//-----------------------------------------------------------------------------
// Purpose: logs sound file access
//-----------------------------------------------------------------------------
void CDownloadListGenerator::OnSoundPrecached(const char *relativePathFileName)
{
	if ( IsX360() )
	{
		// not supporting
		return;
	}

	// skip any special characters
	if (!V_isalnum(relativePathFileName[0]))
	{
		++relativePathFileName;
	}

	// prepend the sound/ directory if necessary
	char file[_MAX_PATH];
	if ( StringHasPrefix( relativePathFileName, "sound" ) )
	{
		Q_strncpy(file, relativePathFileName, sizeof(file));
	}
	else
	{
		// prepend the materials directory
		Q_snprintf(file, sizeof(file), "sound\\%s", relativePathFileName);
	}

	OnResourcePrecached(file);
}

//-----------------------------------------------------------------------------
// Purpose: logs the precache as a file access
//-----------------------------------------------------------------------------
void CDownloadListGenerator::OnResourcePrecached( const char *pRelativePathFileName )
{
	// not supporting
	if ( IsX360() )
		return;

	// ignore empty string
	if ( pRelativePathFileName[0] == 0 )
		return;

	// ignore files that start with '*' since they signify special models
	if ( pRelativePathFileName[0] == '*' )
		return;

	if ( Q_IsAbsolutePath( pRelativePathFileName ) )
	{
		Warning( "*** CDownloadListGenerator::OnResourcePrecached: Encountered full path %s!\n", pRelativePathFileName );
		return;
	}

	char pRelativePath[MAX_PATH];
	Q_strncpy( pRelativePath, pRelativePathFileName, sizeof(pRelativePath) );
	Q_FixSlashes( pRelativePath, '/' );

	// make sure the filename hasn't already been written
	UtlSymId_t filename = m_AlreadyWrittenFileNames.Find( pRelativePath );
	if ( filename != UTL_INVAL_SYMBOL )
		return;

	// record in list, so we don't write it again
	m_AlreadyWrittenFileNames.AddString( pRelativePath );

	// don't allow files for download the server doesn't have
	if ( !g_pFileSystem->FileExists( pRelativePath, "GAME" ) )
		return;	

	// add extras for mdl's
	char *pExt = const_cast< char* >( Q_GetFileExtension( pRelativePath ) );
	if ( !Q_stricmp( pExt, "mdl" ) )
	{
		Q_strncpy(pExt, "vvd", 10);
		OnResourcePrecached(pRelativePath);

		Q_strncpy(pExt, "ani", 10);
		OnResourcePrecached(pRelativePath);

		Q_strncpy(pExt, "dx90.vtx", 10);
		OnResourcePrecached(pRelativePath);

		Q_strncpy(pExt, "phy", 10);
		OnResourcePrecached(pRelativePath);

		Q_strncpy(pExt, "jpg", 10);
		OnResourcePrecached(pRelativePath);
	}

	FileHandle_t handle = m_hReslistFile;
	if ( handle != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Write("\"", 1, handle);
		g_pFileSystem->Write( pRelativePath, Q_strlen(pRelativePath), handle );
		g_pFileSystem->Write("\"\n", 2, handle);
	}
	if ( m_pStringTable )
	{
		m_pStringTable->AddString( true, pRelativePath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: marks a precached file as needing a specific CRC on the client
//-----------------------------------------------------------------------------
void CDownloadListGenerator::ForceExactFile( const char *relativePathFileName, ConsistencyType consistency )
{
	if ( IsX360() )
	{
		// not supporting
		return;
	}

	if ( !m_pStringTable )
		return;

	if ( consistency != CONSISTENCY_EXACT && consistency != CONSISTENCY_SIMPLE_MATERIAL )
	{
		consistency = CONSISTENCY_EXACT;
	}

	CRC32_t crc;
	bool error = true;
	char file[_MAX_PATH];
	const char *filePtr = relativePathFileName;

	if (Q_strstr(relativePathFileName, ".vmt") || Q_strstr(relativePathFileName, ".vtf"))
	{
		// it's a materials file, make sure that it starts in the materials directory, and we get the .vtf
		if ( StringHasPrefix(relativePathFileName, "materials" ) )
		{
			Q_strncpy(file, relativePathFileName, sizeof(file));
		}
		else
		{
			// prepend the materials directory
			Q_snprintf(file, sizeof(file), "materials\\%s", relativePathFileName);
		}
		error = !CRC_File( &crc, file );
		filePtr = file;
	}
	else
	{
		error = !CRC_File( &crc, relativePathFileName );
	}

	if ( error )
	{
		DevWarning( "Failed to CRC %s\n", relativePathFileName );
	}
	else
	{
		char relativeFileName[_MAX_PATH];
		Q_strncpy( relativeFileName, filePtr, sizeof( relativeFileName ) );
		V_FixSlashes( relativeFileName, '/' );

		ExactFileUserData userData;
		userData.consistencyType = consistency;
		userData.crc = crc;

		int index = m_pStringTable->FindStringIndex( relativeFileName );
		if ( index != INVALID_STRING_INDEX )
		{
			m_pStringTable->SetStringUserData( index, sizeof( ExactFileUserData ), &userData );
		}
		else
		{
			m_pStringTable->AddString( true, relativeFileName, sizeof( ExactFileUserData ), &userData );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: marks a precached model as having a maximum size on the client
//-----------------------------------------------------------------------------
void CDownloadListGenerator::ForceModelBounds( const char *relativePathFileName, const Vector &mins, const Vector &maxs )
{
	if ( IsX360() )
	{
		// not supporting
		return;
	}

	if ( !m_pStringTable )
		return;

	if ( !relativePathFileName )
		relativePathFileName = "";

	if (!Q_stristr(relativePathFileName, ".mdl"))
	{
		DevWarning( "Warning - trying to enforce model bounds on %s\n", relativePathFileName );
		return;
	}

	char relativeFileName[_MAX_PATH];
	Q_strncpy( relativeFileName, relativePathFileName, sizeof( relativeFileName ) );
	V_FixSlashes( relativeFileName, '/' );

	ModelBoundsUserData userData;
	userData.consistencyType = CONSISTENCY_BOUNDS;
	userData.mins = mins;
	userData.maxs = maxs;

	int index = m_pStringTable->FindStringIndex( relativeFileName );
	if ( index != INVALID_STRING_INDEX )
	{
		m_pStringTable->SetStringUserData( index, sizeof( ModelBoundsUserData ), &userData );
	}
	else
	{
		m_pStringTable->AddString( true, relativeFileName, sizeof( ModelBoundsUserData ), &userData );
	}
}



	

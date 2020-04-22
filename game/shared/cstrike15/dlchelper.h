//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef _DLC_HELPER_H
#define _DLC_HELPER_H
#pragma once

#include "platform.h"
#include "keyvalues.h"
#include "filesystem.h"

class KeyValues;

// Helper methods for DLC
class DLCHelper
{
public:
	static inline uint64 GetInstalledDLCMask( void );

	// Loads the key values from all installed DLC files matching the file name plus _dlc[id].
	// For example, if fileName is "gamemodes.txt" we will look for "gamemodes_dlc1.txt", "gamemodes_dlc2.txt", etc...
	// The key values from the dlc files will then be merged in (with update) to the passed
	// in key values.
	static inline void AppendDLCKeyValues( KeyValues* pKeyValues, const char* fileName, const char* startDir = NULL );
};

uint64 DLCHelper::GetInstalledDLCMask( void )
{
//	Assert( g_pMatchFramework );
//	return g_pMatchFramework->GetMatchSystem()->GetDlcManager()->GetDataInfo()->GetUint64( "@info/installed" ); 

	//static ConVarRef mm_dlcs_mask_fake( "mm_dlcs_mask_fake" );
	//char const *szFakeDlcsString = mm_dlcs_mask_fake.GetString();
	//if ( *szFakeDlcsString )
	//	return atoi( szFakeDlcsString );

	//static ConVarRef mm_dlcs_mask_extras( "mm_dlcs_mask_extras" );
	//uint64 uiDLCmask = ( unsigned ) mm_dlcs_mask_extras.GetInt();

	uint64 uiDLCmask = 0;
	bool bSearchPath = false;
	int numDLCs = g_pFullFileSystem->IsAnyDLCPresent( &bSearchPath );

	// If we need to, trigger the mounting of DLC
	if ( !bSearchPath )
	{
		g_pFullFileSystem->AddDLCSearchPaths();
	}

	for ( int j = 0; j < numDLCs; ++ j )
	{
		unsigned int uiDlcHeader = 0;
		if ( !g_pFullFileSystem->GetAnyDLCInfo( j, &uiDlcHeader, NULL, 0 ) )
			continue;

		int idDLC = DLC_LICENSE_ID( uiDlcHeader );
		if ( idDLC < 1 || idDLC >= 31 )
			continue;	// unsupported DLC id

		uiDLCmask |= ( 1ull << idDLC );
	}

	return uiDLCmask;
}

void DLCHelper::AppendDLCKeyValues( KeyValues* pKeyValues, const char* fileName, const char* startDir )
{
	uint64 installedDlc = GetInstalledDLCMask();

	if ( installedDlc )
	{
		KeyValues* pDlcKeyValues = new KeyValues( "" );
		char dlcFileName[128] = "";

		// We need to insert _dlc[id] into the file name right before the extension
		// so filename.txt should become filename_dlc1.txt
		
		// scan backward for '.'
		int dotIndex = V_strlen( fileName ) - 1;
		while ( dotIndex > 0 && fileName[dotIndex] != '.' )
		{
			--dotIndex;
		}

		if ( dotIndex == 0 )
		{
			Warning( "Invalid file name passed to DLCHelper::AppendDLCKeyValues (%s)\n", fileName );
			return;
		}

		const char* extension = fileName + dotIndex + 1;
		V_strncpy( dlcFileName, fileName, 128 );

		// For each installed dlc check for an updated file and merge it in
		for ( uint64 i = 1; i < 64; i++ )
		{
			// Don't bother if this dlc isn't installed
			if ( installedDlc & ( 1ull << i ) )
			{
				// Get the filename for this dlc
				V_snprintf( dlcFileName + dotIndex, 128 - dotIndex, "_dlc%d.%s", ( int )i, extension );
				
				// Load and merge the keys from this dlc
				pDlcKeyValues->Clear();
				if ( pDlcKeyValues->LoadFromFile( g_pFullFileSystem, dlcFileName, startDir  ) )
				{
					pKeyValues->MergeFrom( pDlcKeyValues, KeyValues::MERGE_KV_UPDATE );
				}
				else
				{
					Warning( "Failed to load %s\n", dlcFileName );
				}
			}
		}

		if ( pDlcKeyValues )
		{
			pDlcKeyValues->deleteThis();
			pDlcKeyValues = NULL;
		}
	}
}


#endif // _DLC_HELPER_H

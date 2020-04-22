//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Functions to help with map instancing.
//
//===============================================================================

#ifndef INSTANCINGHELPER_H
#define INSTANCINGHELPER_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

class CInstancingHelper
{
public:
	//-----------------------------------------------------------------------------
	// Attempts to locate an instance by its filename, given the 
	// location of the root-level VMF file, the instance directory specified in 
	// the gameinfo.txt file, and the GAME directory.
	//
	//    pFileSystem - pointer to file system object
	//    pBaseFilename - path of the file including the instance
	//    pInstanceFilename - relative filename of the instance, as read from
	//        a func_instance entity
	//    pInstanceDirectory - instance directory, specified by gameinfo.txt
	//    pResolvedInstanceFilename - pointer to buffer to receive resolved
	//        filename (only valid if function returns true)
	//    nBufferSize - size of the buffer.  Should be at least MAX_PATH.
	//
	// Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	static bool ResolveInstancePath( IFileSystem *pFileSystem, const char *pBaseFilename, const char *pInstanceFilename, const char *pInstanceDirectory, char *pResolvedInstanceFilename, int nBufferSize )
	{
		Assert( nBufferSize >= MAX_PATH );

		char fixedInstanceFilename[MAX_PATH];
		Q_strncpy( fixedInstanceFilename, pInstanceFilename, MAX_PATH );
		Q_SetExtension( fixedInstanceFilename, ".vmf", MAX_PATH );
		Q_FixSlashes( fixedInstanceFilename );
		V_FixDoubleSlashes( fixedInstanceFilename );

		// Try to locate the instance file relative to the current file's path
		Q_strncpy( pResolvedInstanceFilename, pBaseFilename, nBufferSize );
		Q_StripFilename( pResolvedInstanceFilename );
		Q_strncat( pResolvedInstanceFilename, "\\", nBufferSize );
		Q_strncat( pResolvedInstanceFilename, fixedInstanceFilename, nBufferSize );
		Q_RemoveDotSlashes( pResolvedInstanceFilename );
		V_FixDoubleSlashes( pResolvedInstanceFilename );
		Q_FixSlashes( pResolvedInstanceFilename );

		if ( pFileSystem->FileExists( pResolvedInstanceFilename ) )
		{
			return true;
		}
#ifdef __IN_HAMMER
		if ( CMapInstance::IsMapInVersionControl( pResolvedInstanceFilename ) == true )
		{
			return true;
		}
#endif	// #ifdef __IN_HAMMER

		// Try to locate the instance file relative to the "maps\" directory
		const char *pMapPath = "\\maps\\";
		char *pMapPathPosition = Q_stristr( pResolvedInstanceFilename, pMapPath );
		if ( pMapPathPosition != NULL )
		{
			pMapPathPosition += Q_strlen( pMapPath );
		}
		else if ( pMapPathPosition == NULL && Q_strnicmp( pResolvedInstanceFilename, "maps\\", 5 ) == 0 )
		{
			pMapPathPosition = pResolvedInstanceFilename + 5;
		}

		// Assuming we found a maps\ directory of some kind
		if ( pMapPathPosition != NULL )
		{
			*pMapPathPosition = 0;
			Q_strncat( pResolvedInstanceFilename, fixedInstanceFilename, nBufferSize );

			if ( pFileSystem->FileExists( pResolvedInstanceFilename ) )
			{
				return true;
			}
#ifdef __IN_HAMMER
			if ( CMapInstance::IsMapInVersionControl( pResolvedInstanceFilename ) == true )
			{
				return true;
			}
#endif // #ifdef __IN_HAMMER
		}

		if ( pInstanceDirectory[0] != '\0' )
		{
			char instanceDirectoryRelativeFilename[MAX_PATH];
			Q_snprintf( instanceDirectoryRelativeFilename, nBufferSize, "%s/%s", pInstanceDirectory, fixedInstanceFilename );
			Q_SetExtension( instanceDirectoryRelativeFilename, ".vmf", MAX_PATH );
			Q_FixSlashes( instanceDirectoryRelativeFilename );
			Q_RemoveDotSlashes( instanceDirectoryRelativeFilename );
			V_FixDoubleSlashes( instanceDirectoryRelativeFilename );

			pFileSystem->RelativePathToFullPath( instanceDirectoryRelativeFilename, "CONTENT", pResolvedInstanceFilename, nBufferSize );

			if ( pFileSystem->FileExists( instanceDirectoryRelativeFilename, "CONTENT" ) )
			{
				return true;
			}
#ifdef __IN_HAMMER
			if ( CMapInstance::IsMapInVersionControl( pResolvedInstanceFilename ) == true )
			{
				return true;
			}
#endif // #ifdef __IN_HAMMER
		}

		int searchPathLen = pFileSystem->GetSearchPath( "CONTENT", true, NULL, 0 );
		char *searchPaths = (char *)stackalloc( searchPathLen + 1 );
		pFileSystem->GetSearchPath( "CONTENT", true, searchPaths, searchPathLen );

		for ( char *path = strtok( searchPaths, ";" ); path; path = strtok( NULL, ";" ) )
		{
			Q_strncpy( pResolvedInstanceFilename, path, nBufferSize );
			Q_strncat( pResolvedInstanceFilename, "maps\\", nBufferSize );
			Q_strncat( pResolvedInstanceFilename, fixedInstanceFilename, nBufferSize );

			if ( pFileSystem->FileExists( pResolvedInstanceFilename ) )
			{
				return true;
			}
		}

#ifdef __IN_HAMMER
		if ( CMapInstance::IsMapInVersionControl( pResolvedInstanceFilename ) == true )
		{
			return true;
		}
#endif // #ifdef __IN_HAMMER

		if ( nBufferSize > 0 )
		{
			pResolvedInstanceFilename[0] = '\0';
		}
		return false;
	}
};

#endif // INSTANCINGHELPER_H

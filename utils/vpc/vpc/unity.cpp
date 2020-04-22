//========= Copyright 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC (unity file support)
//
// Summary:
//  - to enable unity files for a project, change $Project to $UnityProject in the *LEAF* VPC file
//  - each $Folder replaced with $UnityFolder in a VPC file will generate one or more unity files
//   o this will include only files listed in that folder, *NOT* child folders
//   o each unity file includes about 20 CPP files (to balance serial compile speed with
//     parallelism) and a unity file including just one source file will not be generated
//   o files are grouped based on compiler settings, so for example files using different
//     PCHs will be segregated into different unity files
//  - source files are #included in unity files unless locally modified:
//   o read-only files are included in their unity files and excluded from the normal build
//   o writeable (locally-edited) files are excluded from unity files and built normally
//     (this minimizes rebuild time AND ensures that edited files will build independently
//     of their containing unity file, i.e so you don't forget to add new header includes!)
//  - schema/qt/protobuf generated files get special handling:
//   o the unity file includes the generated (*.gen_cpp/moc_*.cpp) file, not the source file
//   o the unity file may include different files for debug/release
//  - the ValveVSAddin (Visual Studio addin) provides supplementary functionality
//   o it detects changes to the readonly status of files at build-time and calls into
//     VPC (using '/unity_update') to update the unity files as appropriate, so you
//     needn't re-VPC your project whenever you check a file out
//   o it provides 'smart' compile commands which compile the appropriate 'output' file
//     for the current file; for example, running 'compile' on a schematized header will
//     compile the generated (debug/release) gen_cpp file, or its containing unity file
//   o VPC writes out a manifest file to provide the file mapping data to the addin
//     (see VPC_GeneratedFiles_OnParseProjectEnd)
//
//=====================================================================================//

#include "vpc.h"

// Define UNITY_SPEW as true to get detailed unity-related spew
#define UNITY_SPEW false

#define g_pUnityFileIncludePrefix "/// "
#define g_pUnityFileExcludePrefix "/*/ "
#define g_pUnityFileSourcePrefix "Source file, "
static const char  g_pUnityFileIncludeString[]	= g_pUnityFileIncludePrefix g_pUnityFileSourcePrefix "included in unity build   "; // NOTE: these strings are the same length
static const char  g_pUnityFileExcludeString[]	= g_pUnityFileExcludePrefix g_pUnityFileSourcePrefix "excluded from unity build ";


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool VPC_Unity_UpdateUnityFile( const CUtlString &unityFilePath )
{
	CUtlBuffer unityFile;
	if ( !Sys_LoadFileIntoBuffer( unityFilePath, unityFile, true ) )
	{
		g_pVPC->VPCWarning( "/unity_update: cannot access unity file '%s'", unityFilePath.Get() );
		return false;
	}

	char *pLine = V_strstr( (char *)unityFile.Base(), g_pUnityFileSourcePrefix );
	while ( pLine )
	{
		static const int REPLACEABLE_STRING_LENGTH = ARRAYSIZE( g_pUnityFileIncludeString ) - 1;
		static const int TOGGLE_STRING_LENGTH = ARRAYSIZE( g_pUnityFileIncludePrefix ) - 1;
		pLine -= TOGGLE_STRING_LENGTH;
		char *pLineEnd  = V_strstr( pLine, "\n" );
		char *pFilePath = pLine + REPLACEABLE_STRING_LENGTH;

		// Check the read-only status of each source file listed in the unity file:
		CUtlString filepath( pFilePath, pLineEnd-pFilePath );
		int64 nFileSize, nModifyTime;
		bool bIsReadOnly, bExcludeFromUnity;
		if ( !Sys_FileInfo( filepath, nFileSize, nModifyTime, bIsReadOnly ) )
		{
			g_pVPC->VPCWarning( "/unity_update: cannot access source file '%s'", filepath.Get() );
			return false;
		}

#ifdef DISALLOW_UNITY_FILE_EXCLUSION
		bExcludeFromUnity = false;
#else
		bExcludeFromUnity = !bIsReadOnly;
#endif

		// Update the unity file just by replacing strings (which requires the strings to be the same length):
		COMPILE_TIME_ASSERT( ARRAYSIZE( g_pUnityFileIncludeString ) == ARRAYSIZE( g_pUnityFileExcludeString ) );
		if ( bExcludeFromUnity )
			V_memcpy( pLine, g_pUnityFileExcludeString, REPLACEABLE_STRING_LENGTH );
		else
			V_memcpy( pLine, g_pUnityFileIncludeString, REPLACEABLE_STRING_LENGTH );

		pLine = V_strstr( pLineEnd, g_pUnityFileSourcePrefix );
	}

	// Now write back out the unity file, IF it has changed
	Sys_WriteFileIfChanged( unityFilePath.Get(), unityFile, true );
	
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool VPC_Unity_UpdateUnityFiles( const char **ppArgs, int nArgs )
{
	// Command should be of the form:  vpc /unity_update "<unityfilelistpath>"
	if ( nArgs != 2 )
		return false;

	// Load the list of unity files
    CUtlStringBuilder* pStrBuf = g_pVPC->GetTempStringBuffer1();
	V_MakeAbsolutePath( pStrBuf->Access(), pStrBuf->Capacity(), ppArgs[1], NULL, k_bVPCForceLowerCase );
	CUtlVector< CUtlString > absFilePaths;
	if ( !Sys_LoadFileAsLines( pStrBuf->Get(), absFilePaths ) )
	{
		g_pVPC->VPCWarning( "/unity_update: cannot find file %s", pStrBuf->Get() );
		return false;
	}

	for ( int i = 0; i < absFilePaths.Count(); i++ )
	{
		// In this path, the files should already exist (we're modifying files generated by VPC earlier, in VPC_Unity_OnParseProjectEnd)
		if ( !VPC_Unity_UpdateUnityFile( absFilePaths[i] ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

struct CSourceFileInfoLessFunc
{
	bool Less( const CSourceFileInfo * const & lhs, const CSourceFileInfo * const & rhs, void *pContext )
	{
		// Tie-break on the original folder index, to retain VPC file ordering as much as possible:
		if ( lhs->m_ConfigStringCRC == rhs->m_ConfigStringCRC )
			return ( lhs->m_iFolderIndex > rhs->m_iFolderIndex ); // Reverse; we consume the sorted vector from the tail (see: 'Perf OCD Disorder')
		return ( lhs->m_ConfigStringCRC < rhs->m_ConfigStringCRC );
	}
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Unity_CopyFileCompilerConfig( CProjectFile *dstFile, CProjectFile *srcFile, const char *pConfigName )
{
	CProjectConfiguration *pDstConfig, *pSrcConfig;
	srcFile->GetConfiguration( pConfigName, &pSrcConfig );
	if ( !pSrcConfig )
		return;

	// Add a destination file config if the source has one
	dstFile->AddConfiguration( pConfigName, &pDstConfig );
	Assert( pDstConfig );

	// Copy the 'compiler' properties:
	CPropertyStates &dst = pDstConfig->GetCompilerTool()->m_PropertyStates;
	CPropertyStates &src = pSrcConfig->GetCompilerTool()->m_PropertyStates;
	Assert( dst.m_Properties.IsEmpty() );
	for ( int i = 0; i < src.m_Properties.Count(); i++ )
	{
		dst.m_Properties.AddToTail( src.m_Properties[i] );
		dst.m_PropertiesInOutputOrder.Insert( dst.m_Properties.Count()-1 );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Unity_BuildUnityfiles( CProjectFolder *pFolder, const CUtlVector< CSourceFileInfo > &unsortedFileList,
								const CUtlVector< CProjectConfiguration *> &rootConfigs, CVCProjGenerator *pDataCollector )
{
	if ( unsortedFileList.IsEmpty() )
	{
		g_pVPC->VPCStatus( UNITY_SPEW, "\n$UnityFolder: skipping folder '%s' (contains no viable files)", pFolder->m_Name.Get() );
		return;
	}

	// Put unity files in "_VPC_\unity_<ProjectName>_<GameName>\<Platform>" to avoid filename conflicts and clutter
	CUtlPathStringHolder unityPath;
    g_pVPC->CreateGeneratedSubdirPath( &unityPath, "unity" );
	Sys_CreatePath( unityPath.Get() );

	// Sort the input files
	CUtlSortVector< const CSourceFileInfo *, CSourceFileInfoLessFunc > sortedFileList;
	for ( int i = 0; i < unsortedFileList.Count(); i++ ) sortedFileList.InsertNoSort( &( unsortedFileList[i] ) );
	sortedFileList.RedoSort();

	int cFilenameAttempt = 0;
	while( sortedFileList.Count() )
	{
		// Create a group of files with consistent configurations (up to a preset limit;
		// the build doesn't parallelize well if unity files are too big - 20 seems to be
		// the sweet spot for client/server builds)
		// [ TODO: split evenly; use "nGroups = ceil( numFiles / MAX_INCLUDES_PER_UNITY_FILE )" ]
		CUtlVector< const CSourceFileInfo * > fileGroup;
		const CSourceFileInfo &firstFile = *sortedFileList.Tail();
		static const int MAX_INCLUDES_PER_UNITY_FILE = 20;
		for ( int i = 0; i < MAX_INCLUDES_PER_UNITY_FILE; i++ )
		{
			if ( !sortedFileList.Count() )
				break;

			// Split files with different configurations into different unity files
			if ( sortedFileList.Tail()->m_ConfigStringCRC != firstFile.m_ConfigStringCRC )
				break;

			fileGroup.AddToTail( sortedFileList.Tail() );
			sortedFileList.RemoveMultipleFromTail( 1 );
		}

		// Don't bother emitting a unity file unless there are at least 2 files in the group!
		if ( fileGroup.Count() < 2 )
		{
			if ( sortedFileList.Count() || ( unsortedFileList.Count() < 2 ) )
				g_pVPC->VPCStatus( UNITY_SPEW, "\n$UnityFolder: skipping file '%s' (cannot group with any other files)", fileGroup[0]->m_pSourceFile->m_Name.Get() );
			continue;
		}


		// Generate a unique .cpp filename from the folderName
		CUtlPathStringHolder unityName;
		while( true )
		{
            CUtlStringBuilder* pStrBuf = g_pVPC->GetTempStringBuffer1();
            pStrBuf->Format( "%s_unity_%s_%02d.cpp", unityPath.Get(), pFolder->m_Name.Get(), ++cFilenameAttempt );
            pStrBuf->Replace( ' ', '_' );
			V_strlower( pStrBuf->Access() );
			if ( !g_pVPC->m_UnityFilesSeen.HasElement( pStrBuf->Get() ) )
            {
                unityName.Set( pStrBuf->Get() );
				break;
            }
		}
		g_pVPC->m_UnityFilesSeen.Insert( unityName.Get() );
		g_pVPC->VPCStatus( UNITY_SPEW, "\n$UnityFolder: emitting '%s' in project: '%s'", unityName.Get(), g_pVPC->GetProjectName() );

		// Add the unity file to the project:
		CProjectFile *pUnityFile = NULL;
		pDataCollector->AddFileToFolder( unityName.Get(), pFolder, true, VPC_FILE_FLAGS_DYNAMIC, &pUnityFile );

		// Make the unity file's Debug/Release compiler settings match those of the files that it includes:
		VPC_Unity_CopyFileCompilerConfig( pUnityFile, firstFile.m_pDebugCompiledFile,   "Debug"   );
		VPC_Unity_CopyFileCompilerConfig( pUnityFile, firstFile.m_pReleaseCompiledFile, "Release" );


		// Generate the contents of the unity file:
		{
			// Add the header
			CUtlBuffer unityFile( 0, 0, CUtlBuffer::TEXT_BUFFER );
			unityFile.Printf( "%s\n",	"// ----------------------------------------- //\n"
										"//   File generated by VPC ($UnityFolder)    //\n"
										"// ----------------------------------------- //\n" );
			if ( firstFile.m_PCHName.IsEmpty() )
			{
				unityFile.Printf( "// Not using a precompiled header\n\n" );
				g_pVPC->VPCStatus( UNITY_SPEW, " -  no PCH" );
			}
			else
			{
				unityFile.Printf( "// Precompiled header:\n#include \"%s\"\n\n", firstFile.m_PCHName.Get() );
				g_pVPC->VPCStatus( UNITY_SPEW, " -  PCH: %s", firstFile.m_PCHName.Get() );
			}

			// Add the file includes
			for ( int i = 0; i < fileGroup.Count(); i++ )
			{
				// Include this file, or its generated output file(s), in the current unity file
				const CSourceFileInfo &fileInfo = *fileGroup[i];
				CUtlString absSourceFilePath     = fileInfo.m_pSourceFile->m_Name.AbsPath( NULL, k_bVPCForceLowerCase );
				CUtlString absDebugIncludePath   = fileInfo.m_pDebugCompiledFile->m_Name.AbsPath( NULL, k_bVPCForceLowerCase );
				CUtlString absReleaseIncludePath = fileInfo.m_pReleaseCompiledFile->m_Name.AbsPath( NULL, k_bVPCForceLowerCase );

				// Determine whether the file is read-only
				int64 nFileSize, nModifyTime;
				bool bExcludeFromUnity = false;

#ifdef DISALLOW_UNITY_FILE_EXCLUSION
				if ( 0 ) 
#else
				if ( !g_pVPC->m_bUnityOnWritableFiles )
#endif
				{
					bool bIsReadOnly = true;
					if ( !Sys_FileInfo( absSourceFilePath, nFileSize, nModifyTime, bIsReadOnly ) )
						g_pVPC->VPCError( "VPC_Unity_BuildUnityfiles: cannot access source file '%s'", absSourceFilePath.Get() );
					bExcludeFromUnity = !bIsReadOnly;
				}

				// #include the appropriate (output) source files in the unity file:
				unityFile.Printf( "#include \"tier0/memdbgoff.h\"\n\n" );

				unityFile.Printf( "\n" );
				unityFile.Printf( "%s%s\n", bExcludeFromUnity ? g_pUnityFileExcludeString : g_pUnityFileIncludeString, absSourceFilePath.Get() );
				unityFile.Printf( "#if defined( _DEBUG )\n" );
				unityFile.Printf( "#include \"%s\"\n", absDebugIncludePath.Get()   );
				unityFile.Printf( "#else  // _DEBUG\n" );
				unityFile.Printf( "#include \"%s\"\n", absReleaseIncludePath.Get() );
				unityFile.Printf( "#endif // _DEBUG\n" );
				unityFile.Printf( "/**/\n" );

				// Exclude unity-included files from the regular build (unless they are locally modified)
				if ( bExcludeFromUnity )
				{
					g_pVPC->VPCStatus( UNITY_SPEW, " ->>EXCLUDING:FILE<<'%s'    >>> LOCALLY-EDITED FILE, WILL BUILD STANDALONE <<<", absSourceFilePath.Get() );
				}
				else
				{
					// NOTE: we exclude the output files, since those are the ones that actually get built
					VPC_SetProperty_ForFile( fileInfo.m_pDebugCompiledFile,   "Debug",   KEYWORD_GENERAL, g_pOption_ExcludedFromBuild, "Yes", pDataCollector );
					VPC_SetProperty_ForFile( fileInfo.m_pReleaseCompiledFile, "Release", KEYWORD_GENERAL, g_pOption_ExcludedFromBuild, "Yes", pDataCollector );
					g_pVPC->VPCStatus( UNITY_SPEW, " -  including file  '%s' (will build with unity)", absSourceFilePath.Get() );
				}

				// Record the relationship between the input/output files:
				g_pVPC->m_UnityOutputFileMap[ fileInfo.m_pSourceFile->m_Name ] = unityName;
			}

			// Finally, write the unity file to disk:
			Sys_WriteFileIfChanged( unityName.Get(), unityFile, true );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Unity_ProcessFolder( CProjectFolder *pFolder, CVCProjGenerator *pDataCollector, const CUtlVector< CProjectConfiguration * > &rootConfigs )
{
	if ( pFolder->m_iFlags & VPC_FOLDER_FLAGS_UNITY )
	{
		CUtlVector< CSourceFileInfo > fileList;
		fileList.EnsureCapacity( pFolder->m_Files.Count() ); // Avoid reallocs
		for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
		{
			CProjectFile *pFile = pFolder->m_Files[iIndex];
			if ( pFile->m_nBuildOrderModifier != 0 ) //modifying the build/"encounter" order means we can't lump together properly
				continue;

			CSourceFileInfo fileInfo( fileList.Count() );

			if ( g_pVPC->m_bUnitySchemaHeadersOnly )
			{
				// In this mode, we ONLY add schematized headers to unity files (preserves the old schema-specific unity file behaviour)
				// TODO: remove this once the unity feature is more widely tested
				if ( !IsHFileExtension( V_GetFileExtension( pFile->m_Name.Get() ) ) || !( pFile->m_iFlags & VPC_FILE_FLAGS_SCHEMA ) )
					continue;
			}

			// Determine if this is to be compiled directly, or if generated files will be compiled in its stead,
			// and generate a config string (so we can determine with which other files this can be grouped):
			if ( !VPC_GeneratedFiles_GetSourceFileInfo( fileInfo, pFile, true, pDataCollector, rootConfigs ) )
				continue;

			// Ignore files that create PCHs, they cannot be put into unity files
			if ( fileInfo.m_bCreatesPCH )
				continue;

			fileList.AddToTail( fileInfo );
		}
		VPC_Unity_BuildUnityfiles( pFolder, fileList, rootConfigs, pDataCollector );
	}

	// Recurse into child folders
	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		VPC_Unity_ProcessFolder( pFolder->m_Folders[iIndex], pDataCollector, rootConfigs );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Unity_OnParseProjectStart( void )
{
	// reset unity file tracking (it's per project)
	g_pVPC->m_UnityFilesSeen.RemoveAll();
	g_pVPC->m_UnityOutputFileMap.Clear();
	g_pVPC->m_bDoneOnParseProjectEnd = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Unity_OnParseProjectEnd( CVCProjGenerator *pDataCollector )
{
	// Skip unity processing during dependency-generation (let it see the original, unmodified contents of the VPC)
	if ( g_pVPC->m_bIsDependencyPass )
		return;

	// Treat $UnityFolder folders as ordinary $Folders, unless the Unity build feature is enabled for this project:
	if ( !g_pVPC->IsProjectUsingUnity() )
		return;


	// Paranoid check: we make the assumption that the PCH is the same for all configs - this call validates that assumption:
	CUtlString globalPCHName;
	CUtlVector< CProjectConfiguration * > rootConfigs;
	pDataCollector->GetAllRootConfigurations( rootConfigs );
	VPC_GetGlobalPropertyString( KEYWORD_COMPILER, pDataCollector, g_pOption_UsePCHThroughFile, &globalPCHName );

	// Paranoid check: the unity code also assumes we have exactly 2 configurations (Debug+Release)
	if ( ( rootConfigs.Count() != 2 ) || ( rootConfigs[0]->m_Name != "Debug" ) || ( rootConfigs[1]->m_Name != "Release" ) )
		g_pVPC->VPCError( "$UnityFolder: unexpected configurations (expected 'Debug' and 'Release')" );

	// Iterate the folders in the project and generate unity files for all $UnityFolder folders:
	VPC_Unity_ProcessFolder( pDataCollector->GetRootFolder(), pDataCollector, rootConfigs );

	g_pVPC->m_bDoneOnParseProjectEnd = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CProjectFile *VPC_Unity_GetContainingUnityFile( CProjectFile *pInputFile, const char * /*pConfigName*/, CVCProjGenerator *pDataCollector )
{
	// This data isn't generated until VPC_Unity_OnParseProjectEnd is run
	if ( !g_pVPC->m_bDoneOnParseProjectEnd )
		return NULL;

	// TODO: why not use a (CProjectFile*)->(CProjectFile*) map, rather than going through strings? These lookups are expensive (ditto for qt/schema).
	CUtlString &generatedFilename = g_pVPC->m_UnityOutputFileMap[ pInputFile->m_Name.Get() ];
	generatedFilename.FixSlashes();
	CProjectFile *pGeneratedFile = NULL;
	pDataCollector->FindFile( generatedFilename.Get(), &pGeneratedFile );
	// This may return NULL if pInputFile is not in a unity file
	return pGeneratedFile;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool IsUnitySupportedForThisTargetPlatform( void )
{
	// TODO: Only tested on WIN32/WIN64 so far... in theory should work for all platforms
	const char *pPlatform = g_pVPC->GetTargetPlatformName();
	return ( VPC_IsPlatformWindows( pPlatform ) ||
             VPC_IsPlatformOSX( pPlatform ) ||
             VPC_IsPlatformLinux( pPlatform ) ||
			 VPC_IsPlatformAndroid( pPlatform ) );
}

bool CVPC::IsUnityEnabled( void )
{
	if ( !m_bAllowUnity )
		return false;	// Unity feature not enabled
	if ( IsUnitySupportedForThisTargetPlatform() )
		return true;	// Feature enabled & supported!
	ExecuteOnce( VPCWarning( "$UnityProject feature disabled, not supported for %s yet", g_pVPC->GetTargetPlatformName() ) )
	return false;		// Platform not supported
}

bool CVPC::IsProjectUsingUnity( script_t *pProjectScript )
{
	// The token $UnityProject enables/disables $UnityFolder per-project (projects with
	// this token will generate unity files and also add the '_unity' suffix to the
	// project name and intermediate folders - this is so we can A/B the two variants
	// in the same solution with no conflicts)

	if ( pProjectScript )
	{
		// Compute this once per project (we do it in this hacky way because this
		// is called from CVPC::CreateOutputFilename, before script parsing begins)
		// TODO: find a way to compute this more cheaply/simply (as it stands, it is called multiple times per project)
		m_bProjectUsesUnity = false;

		CUtlBuffer file;
		CUtlString vpcFilename = pProjectScript->name.UnqualifiedFilename();
		if ( Sys_LoadFileIntoBuffer( vpcFilename.Get(), file, true ) )
		{
			const char *pFileStart = (const char *)file.Base();
			const char *pKeyword   = V_stristr( pFileStart, "$UnityProject" );
			if ( pKeyword && !Sys_IsSingleLineComment( pKeyword, pFileStart ))
			{
				m_bProjectUsesUnity = true;
			}
		}
		else { g_pVPC->VPCWarning( "Could not find VPC file '%s' !", vpcFilename.Get() ); }

		// Set the _UNITYSUBDIR macro here, before project parsing begins:
		SetSystemMacro( "_UNITYSUBDIR", ( m_bProjectUsesUnity && IsUnityEnabled() ) ? "\\unity" : "", false );
	}

	return ( m_bProjectUsesUnity && IsUnityEnabled() );
}

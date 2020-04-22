//======= Copyright 1996-2016, Valve Corporation, All rights reserved. ========
//
// Purpose: 
//
//=============================================================================

#include "vpc.h"
#include "dependencies.h"
#include "baseprojectdatacollector.h"
#include "tier0/fasttimer.h"
#include "tier1/utlvector.h"

#define VPC_CRC_CACHE_VERSION 8

// ------------------------------------------------------------------------------------------------------- //
// CDependency functions.
// ------------------------------------------------------------------------------------------------------- //

CDependency::CDependency( CProjectDependencyGraph *pDependencyGraph ) :
	m_pDependencyGraph( pDependencyGraph )
{
	// ensure this dependency marker is initialized unmarked by ensuring inequality
	m_iDependencyMark = m_pDependencyGraph->m_iDependencyMark - 1;

	m_bCheckedIncludes = false;
	m_nCacheModificationTime = m_nCacheFileSize = 0;
}

CDependency::~CDependency()
{
}

const char* CDependency::GetName() const
{
	return m_Filename.String();
}

bool CDependency::CompareAbsoluteFilename( const char *pAbsPath  ) const
{
	return ( V_stricmp_fast( m_Filename.String(), pAbsPath ) == 0 );
}

bool CDependency::DependsOn( CDependency *pTest, int flags )
{
	m_pDependencyGraph->ClearAllDependencyMarks();
	CUtlVector<CUtlBuffer> callTreeOutputStack;
	if ( FindDependency_Internal( callTreeOutputStack, pTest, flags, 1 ) )
	{
		if ( g_pVPC->IsShowDependencies() )
		{
			Log_Msg( LOG_VPC, "-------------------------------------------------------------------------------\n" );
			Log_Msg( LOG_VPC, "%s\n", GetName() );
			for( int i = callTreeOutputStack.Count() - 1; i >= 0; i-- )
			{
				Log_Msg( LOG_VPC, "%s", ( const char * )callTreeOutputStack[i].Base() );
			}
			Log_Msg( LOG_VPC, "-------------------------------------------------------------------------------\n" );
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool CDependency::FindDependency_Internal( CUtlVector<CUtlBuffer> &callTreeOutputStack, CDependency *pTest, int flags, int depth )
{
	if ( pTest == this )
		return true;
	
	// Don't revisit us.
	if ( HasBeenMarked() )
		return false;

	Mark();

	// Don't recurse further?
	if ( depth > 1 && !(flags & k_EDependsOnFlagRecurse) )
		return false;

	// Don't go into the children of libs if they don't want.
	if ( !(flags & k_EDependsOnFlagTraversePastLibs) && m_Type == k_eDependencyType_Library )
		return false;

	// Go through everything I depend on. If any of those things 
	for ( int iDepList=0; iDepList < 2; iDepList++ )
	{
		if ( iDepList == 1 && !(flags & k_EDependsOnFlagCheckAdditionalDependencies) )
			continue;

		CUtlVector<CDependency*> &depList = (iDepList == 0 ? m_Dependencies : m_AdditionalDependencies);

		for ( int i=0; i < depList.Count(); i++ )
		{
			CDependency *pChild = depList[i];
			if ( pChild->FindDependency_Internal( callTreeOutputStack, pTest, flags, depth+1 ) )
			{
				if ( g_pVPC->IsShowDependencies() )
				{
					CUtlString dependsString;
					dependsString.Format( "depends on %s\n", pChild->GetName() );

					int n = callTreeOutputStack.AddToTail();
					CUtlBuffer &b = callTreeOutputStack[n];

					b.EnsureCapacity( dependsString.Length() + 2 );
					b.PutString( dependsString.Get() );
					b.PutChar( 0 );
				}
				return true;
			}
		}
	}

	return false;
}

bool CDependency::GetDirectDependenciesByType( EDependencyType type, CUtlVector< CDependency * > &result ) const
{
	for ( int iDepList = 0; iDepList < 2; iDepList++ )
	{
		const CUtlVector<CDependency*> &depList = ( iDepList == 0 ? m_Dependencies : m_AdditionalDependencies );
		for ( int i = 0; i < depList.Count(); i++ )
		{
			CDependency *pChild = depList[i];
			if ( pChild->m_Type == type )
			{
				AssertDbg( !result.HasElement( pChild ) ); // Should be no dupes (slow check for large sets)
				result.AddToTail( pChild );
			}
		}
	}
	return !!result.Count();
}

void CDependency::Mark()
{
	m_iDependencyMark = m_pDependencyGraph->m_iDependencyMark;
}

bool CDependency::HasBeenMarked()
{
	return m_iDependencyMark == m_pDependencyGraph->m_iDependencyMark;
}


CDependency_Project::CDependency_Project( CProjectDependencyGraph *pDependencyGraph )
 :	CDependency( pDependencyGraph )
 , m_pProjectGenerator( nullptr )
{
}

const char *CDependency_Project::GetProjectFileName( void )
{
	if ( !m_pProjectGenerator )
	{
		g_pVPC->VPCError( "Could not determine project file name for \"%s\"", m_Filename.Get() );
	}

	return m_pProjectGenerator->GetOutputFileName();
}

const char *CDependency_Project::GetProjectGUIDString( void )
{
	if ( !m_pProjectGenerator )
	{
		g_pVPC->VPCError( "Could not determine project GUID for \"%s\"", m_Filename.Get() );
	}

	return m_pProjectGenerator->GetGUIDString();
}


// This is responsible for scanning a project file and pulling out:
// - a list of libraries it uses
// - the $AdditionalIncludeDirectories paths
// - a list of source files it uses
// - the name of the file it generates
class CSingleProjectScanner
{
public:
	CSingleProjectScanner()
	 :	m_pDependencyGraph( NULL ),
		m_pDependencyProject( NULL ),
		m_pDataCollector( NULL ),
		m_nDupeChecks( 0 )
	{
		Assert( !s_pSingleton );
		s_pSingleton = this;
	}

	~CSingleProjectScanner()
	{
		Assert( s_pSingleton == this );
		s_pSingleton = NULL;
	}
	
	void ScanProjectFile( CProjectDependencyGraph *pGraph, const char *szScriptName, CDependency_Project *pProject )
	{
		if ( !VPC_AreProjectDependenciesSupportedForThisTargetPlatform() ) // Should error-out further upstream than here...
			g_pVPC->VPCError( "Cannot build project dependencies, not supported for %s yet", g_pVPC->GetTargetPlatformName() );

		m_ScriptName		 = szScriptName;
		m_pDependencyGraph	 = pGraph;
		m_pDependencyProject = pProject;

		// This has VPC parse the script and CVCProjGenerator collects all the data into lists of the
		// stuff we care about like source files and include paths. It will call back into OnEndProject,
		// from CVCProjGenerator::EndProject(), via VPC_GenerateProjectDependencies()
		g_pVPC->ParseProjectScript( szScriptName, 0, true, false, pProject );
	}

	void OnEndProject( CBaseProjectDataCollector *pDataCollector )
	{
		m_pDataCollector = pDataCollector;
		m_pVCProjGenerator = dynamic_cast<CVCProjGenerator *>(pDataCollector);
		m_ProjectName    = pDataCollector->GetProjectName();
		g_pVPC->UndecorateProjectName( m_ProjectName );

		// Clear the dependency marks, which we will use to avoid multiply-processing files in SetupFilesList()
		m_pDependencyGraph->ClearAllDependencyMarks();

		if ( m_pVCProjGenerator )
		{
			CUtlVector<CProjectConfiguration *> rootConfigs;
			m_pVCProjGenerator->GetAllRootConfigurations( rootConfigs );
			for ( int i = 0; i < rootConfigs.Count(); i++ )
			{
				// TODO: SetupFilesList() will early-out the second time through, so any additional search paths in the Release
				//       config will be ignored! We should combine the two sets of search paths and call SetupFilesList() ONCE
				//       (it will warn if any ambiguities are found - which we should definitely avoid)
				SetupIncludeDirectories( rootConfigs[i] );
				SetupFilesList( m_pVCProjGenerator->GetRootFolder() );
				SetupAdditionalProjectDependencies( rootConfigs[i] );
				SetupBuildToolDependencies( rootConfigs[i] );
				SetupProjectOutputs( rootConfigs[i] );
			}
		}
		else
		{
			for ( int nFileIter = pDataCollector->m_Files.First(); pDataCollector->m_Files.IsValidIndex( nFileIter ); nFileIter = pDataCollector->m_Files.Next( nFileIter ) )
			{
				CFileConfig *pFile = pDataCollector->m_Files.Element( nFileIter );
				char sAbsolutePath[MAX_FIXED_PATH];
				V_MakeAbsolutePath( sAbsolutePath, sizeof( sAbsolutePath ), pFile->m_Filename.Get(), NULL, k_bVPCForceLowerCase );

				// Don't bother with source files if we're not building the full dependency set.
				bool bIsSourceFile = IsSourceFile( sAbsolutePath );
				if ( bIsSourceFile && !m_pDependencyGraph->m_bFullDependencySet )
					continue;

				// For source files, don't bother with files that don't exist. If we do create entries
				// for files that don't exist, then they'll have a "cache file size"
				if ( bIsSourceFile && !Sys_Exists( sAbsolutePath ) )
					continue;

				// Add an entry to the project for this file (but only once - not twice for debug+release!)
				CDependency *pDep = m_pDependencyGraph->FindOrCreateDependency( sAbsolutePath, (pFile->m_iFlags & (VPC_FILE_FLAGS_STATIC_LIB | VPC_FILE_FLAGS_IMPORT_LIB | VPC_FILE_FLAGS_SHARED_LIB)) ? k_eDependencyType_Library : (bIsSourceFile ? k_eDependencyType_SourceFile : k_eDependencyType_Unknown) );
				if ( !pDep->HasBeenMarked() && pDep != m_pDependencyProject )
				{
					AssertDbg( !m_pDependencyProject->m_Dependencies.HasElement( pDep ) ); // HasBeenMarked() should prevent this (slow check for large sets)
					m_pDependencyProject->m_Dependencies.AddToTail( pDep );
					pDep->Mark();
				}
			}
		}
	}

	void SetupFilesList( CProjectFolder *pFolder )
	{
		for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
		{
			// Don't bother with dynamic files; prefer keeping the dependency behaviour simple and predictable
			// (the code which generates the dynamic files can inject additional dependencies explicitly)
			CProjectFile *pFile = pFolder->m_Files[iIndex];
			if ( pFile->m_iFlags & VPC_FILE_FLAGS_DYNAMIC )
				continue;

			// If this file is excluded from all configs, skip it.
			// NOTE: Schema files are always excluded from the build (see VPC_Schema_TrackFile), 
			//       but we do want proper dependencies for them, so ignore exclusion for them.
			if ( pFile->m_Configs.Count() && !( pFile->m_iFlags & VPC_FILE_FLAGS_SCHEMA ) )
			{
				bool bExcluded, bIncluded = false;
				for ( int iConfig = 0; iConfig < pFile->m_Configs.Count(); iConfig++ )
				{
					if ( !VPC_GetPropertyBool( KEYWORD_GENERAL, NULL, pFile->m_Configs[ iConfig ], g_pOption_ExcludedFromBuild, &bExcluded ) || !bExcluded )
						bIncluded = true; // Marked as NOT excluded (or not marked at all) -> included
				}
				if ( !bIncluded )
					continue;
			}

			// Make this an absolute path.
			char sAbsolutePath[MAX_FIXED_PATH];
			V_MakeAbsolutePath( sAbsolutePath, sizeof( sAbsolutePath ), pFile->m_Name.Get(), NULL, k_bVPCForceLowerCase );

			if ( !V_stricmp_fast( V_GetFileExtensionSafe( sAbsolutePath ), "vpc" ) )
			{
				// The vpc script file chain is auto-added into the "VPC Scripts" the project as a nicety.
				// This can create an incorrect dependency, when the vpc chain just happens to include a vpc that maps to another project,
				// since the FindOrCreateDependency() is name based. This pattern occurs with VPC's that have an outer script that use a common
				// inner script, but then have another project that uses the inner script directly. Prefer to fix here than do script renames.
				continue;
			}

			// Don't bother with source files if we're not building the full dependency set.
			bool bIsSourceFile = IsSourceFile( sAbsolutePath );
			if ( bIsSourceFile && !m_pDependencyGraph->m_bFullDependencySet )
				continue;

			// For source files, don't bother with files that don't exist. If we do create entries
			// for files that don't exist, then they'll have a "cache file size"
			if ( bIsSourceFile && !Sys_Exists( sAbsolutePath ) )
				continue;

			// Add an entry to the project for this file (but only once - not twice for debug+release!)
			CDependency *pDep = m_pDependencyGraph->FindOrCreateDependency( sAbsolutePath, (pFile->m_iFlags & (VPC_FILE_FLAGS_STATIC_LIB | VPC_FILE_FLAGS_IMPORT_LIB)) ? k_eDependencyType_Library : (bIsSourceFile ? k_eDependencyType_SourceFile : k_eDependencyType_Unknown) );
			if ( !pDep->HasBeenMarked() && pDep != m_pDependencyProject )
			{
				AssertDbg( !m_pDependencyProject->m_Dependencies.HasElement( pDep ) ); // HasBeenMarked() should prevent this (slow check for large sets)
				m_pDependencyProject->m_Dependencies.AddToTail( pDep );
				pDep->Mark();
			}

			// Process the file's includes, recursively.
			if ( pDep->m_Type == k_eDependencyType_SourceFile )
			{
				AddIncludesForFile( pDep );
			}
		}

		// Recurse into child folders:
		for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
		{
			SetupFilesList( pFolder->m_Folders[iIndex] );
		}
	}

	void AddIncludesForFile( CDependency *pFile )
	{
		// Have we already parsed this file for its includes?
		if ( pFile->m_bCheckedIncludes )
			return;
		pFile->m_bCheckedIncludes = true;

		// Setup all the include paths we want to search.
		CUtlVector<CUtlString> includeDirs;
		CUtlPathStringHolder fileDir;
		if ( !fileDir.ExtractFilePath( pFile->GetName() ) )
			g_pVPC->VPCError( "AddIncludesForFile: V_ExtractFilePath( %s ) failed.", pFile->GetName() );

		// NOTE: for headers *in* a search path folder (e.g system headers), fileDir duplicates that folder in the search
		//       path list, so we search the same path twice... that duplication is handled in the loop below
		// TODO: the usage of fileDir here does not correctly emulate compiler search path behaviour (see http://msdn.microsoft.com/en-us/library/36k2cdd4.aspx)
		includeDirs.AddToTail( fileDir.Get() );
		includeDirs.AddMultipleToTail( m_IncludeDirectories.Count(), m_IncludeDirectories.Base() );

		// Get all the #include directives.
		CUtlVector<CUtlString> includes;
		GetIncludeFiles( pFile->GetName(), includes );
		++m_pDependencyGraph->m_nFilesParsedForIncludes;

		// Now see which of them we can open.
		for ( int iIncludeFile=0; iIncludeFile < includes.Count(); iIncludeFile++ )
		{
			CUtlVector< CDependency * > searchResults;

			for ( int iIncludeDir=0; iIncludeDir < includeDirs.Count(); iIncludeDir++ )
			{
				CUtlPathStringHolder fullName;
				fullName.ComposeFileName( includeDirs[iIncludeDir].String(), includes[iIncludeFile].String() );

				CDependency *pIncludeFile = m_pDependencyGraph->FindDependency( fullName );
				if ( !pIncludeFile )
				{
					if ( !Sys_Exists( fullName ) )
						continue;

					// Find or add the dependency (NOTE: we know it's a source file no matter what the extension, e.g <algorithm>)
					pIncludeFile = m_pDependencyGraph->FindOrCreateDependency( fullName, k_eDependencyType_SourceFile );
				}

				if ( !searchResults.HasElement( pIncludeFile ) )
					searchResults.AddToTail( pIncludeFile );

				if ( ( m_nDupeChecks += pFile->m_Dependencies.Count() ) > 1000000 ) // TODO: pFile->m_Dependencies.HasElement() is slow for large sets
					ExecuteOnce( g_pVPC->VPCWarning( "PERF WARNING in CSingleProjectScanner::AddIncludesForFile..." ) );

				// Don't process duplicate dependencies! (caused by: duplicate #includes, duplicate
				// search paths and different search paths resolving to the same absolute path)
				if ( pFile->m_Dependencies.HasElement( pIncludeFile ) )
					continue;
				pFile->m_Dependencies.AddToTail( pIncludeFile );

				// Recurse.
				AddIncludesForFile( pIncludeFile );
			}

			if ( g_pVPC->IsVerbose() && ( searchResults.Count() == 0 ) )
			{
				// Could not find this include (not too uncommon, since the include parser ignores #ifdefs)
				g_pVPC->VPCWarning( "Cannot find include file: %s (included in %s)", includes[iIncludeFile].String(), pFile->GetName() );
			}
			else if ( g_pVPC->IsVerbose() && ( searchResults.Count() > 1 ) )
			{
				// We found multiple (ambiguous) results for this include!
				g_pVPC->VPCWarning( "CSingleProjectScanner: Ambiguous include file %s (included in %s)", includes[iIncludeFile].String(), pFile->GetName() );
				for ( int i = 0; i < searchResults.Count(); i++ ) g_pVPC->VPCWarning( " - Ambiguous include file could be: %s", searchResults[i]->GetName() );
			}
		}
	}

	bool SeekToIncludeStart( const char* &pSearchPos )
	{
		while ( 1 )
		{
			++pSearchPos;
			if ( *pSearchPos == 0 || *pSearchPos == '\r' || *pSearchPos == '\n' )
				return false;

			if ( *pSearchPos == '\"' || *pSearchPos == '<' )
			{
				++pSearchPos;
				return true;
			}
		}
	}

	bool SeekToIncludeEnd( const char* &pSearchPos )
	{
		while ( 1 )
		{
			++pSearchPos;
			if ( *pSearchPos == 0 || *pSearchPos == '\r' || *pSearchPos == '\n' )
				return false;

			if ( *pSearchPos == '\"' || *pSearchPos == '>' )
				return true;
		}
	}

	void GetIncludeFiles( const char *pFilename, CUtlVector<CUtlString> &includes )
	{
		char *pFileData;
		int ret = Sys_LoadFile( pFilename, (void**)&pFileData, false );
		if ( ret == -1 )
		{
			if ( g_pVPC->IsVerbose() )
			{
				g_pVPC->VPCWarning( "GetIncludeFiles( %s ) - can't open file (included by project %s).", pFilename, m_ScriptName.String() );
			}
			return;
		}

		const char *pSearchPos = pFileData;
		while ( 1 )
		{
			const char *pLookFor = "#include";
			const char *pIncludeStatement = V_strstr( pSearchPos, pLookFor );
			if ( !pIncludeStatement )
				break;

			pSearchPos = pIncludeStatement + V_strlen( pLookFor );

			if ( Sys_IsSingleLineComment( pIncludeStatement, pFileData ) ) // Skip single-line comments
				continue;
			
			if ( !SeekToIncludeStart( pSearchPos ) )
				continue;
			const char *pFilenameStart = pSearchPos;

			if ( !SeekToIncludeEnd( pSearchPos ) )
				continue;
			const char *pFilenameEnd = pSearchPos;

			CUtlPathStringHolder includeFilename( pFilenameStart, (int)( pFilenameEnd - pFilenameStart ) );

			// Fixup double slashes.
            V_FixDoubleSlashes( includeFilename.GetForModify() );
            includeFilename.FixSlashes();

			includes.AddToTail( includeFilename.Get() );
		}
		
		delete [] pFileData;
	}

	void SetupIncludeDirectories( CProjectConfiguration *pRootConfig )
	{
		// Get includes from the config:
		CUtlVector<CUtlString> includeList;
		VPC_GetIncludeDirectories( NULL, pRootConfig, includeList );

		// Also add system include paths (last):
		// TODO: the VS compiler also searches "$(FrameworkSDKDir)\include", which is missing here...
		if ( m_pDependencyGraph->m_bIncludeSystemFiles )
		{
			CUtlVector< CUtlString > systemPaths;	
			if ( !GetSystemIncludePaths( systemPaths, g_pVPC->GetTargetPlatformName(), g_pVPC->GetTargetCompilerName() ) )
			{
				g_pVPC->VPCError( "CSingleProjectScanner::SetupIncludeDirectories failed to set up system include paths" );
			}
			includeList.AddVectorToTail( systemPaths );
		}

		for ( int i=0; i < includeList.Count(); i++ )
		{
			char sAbsolute[MAX_FIXED_PATH], sFixed[MAX_FIXED_PATH];
			V_MakeAbsolutePath( sAbsolute, sizeof( sAbsolute ), includeList[i].Get(), NULL, k_bVPCForceLowerCase );
			V_FixupPathName( sFixed, ARRAYSIZE(sFixed), sAbsolute );
			m_IncludeDirectories.AddToTail( sFixed );
		}
	}

	void SetupProjectOutputs( CProjectConfiguration *pRootConfig )
	{
		CUtlString tempString;
		CUtlVector<CUtlString> outputsForThisConfig; //collecting locally so we can resolve compiler macros and deduplicate before adding to the member list

		//read $AdditionalOutputFiles property
		if ( VPC_GetPropertyString( KEYWORD_GENERAL, pRootConfig, NULL, g_pOption_AdditionalOutputFiles, &tempString ) )
		{
			CSplitString outStrings( tempString.Get(), ";" );
			for ( int i=0; i < outStrings.Count(); i++ )
			{
				outputsForThisConfig.AddToTail( outStrings[i] );
			}
		}

		//linker outputs
		tempString.Clear();
		VPC_GetPropertyString( KEYWORD_LINKER, pRootConfig, NULL, g_pOption_ImportLibrary, &tempString );
		if ( !tempString.IsEmpty() )
		{
			outputsForThisConfig.AddToTail( tempString );
		}
		tempString.Clear();
		VPC_GetPropertyString( KEYWORD_LIBRARIAN, pRootConfig, NULL, g_pOption_OutputFile, &tempString );
		if ( !tempString.IsEmpty() )
		{
			outputsForThisConfig.AddToTail( tempString );
		}
		tempString.Clear();
		VPC_GetPropertyString( KEYWORD_LINKER, pRootConfig, NULL, g_pOption_OutputFile, &tempString );
		if ( !tempString.IsEmpty() )
		{
			outputsForThisConfig.AddToTail( tempString );
		}

		tempString.Clear();
		VPC_GetPropertyString( KEYWORD_GENERAL, pRootConfig, NULL, "$GameOutputFile", &tempString );
		if ( !tempString.IsEmpty() )
		{
			outputsForThisConfig.AddToTail( tempString );
		}
		
		for ( int nOutputIter = 0; nOutputIter < outputsForThisConfig.Count(); ++nOutputIter )
		{
			//Resolve compiler macros
			const char *szResolve = VPC_ResolveCompilerMacrosInString( outputsForThisConfig[nOutputIter], tempString, pRootConfig, nullptr );

			//add to the member list while deduplicating
			bool bDuplicate = false;
			for ( int nDupeIter = 0; nDupeIter < m_ProjectOutputs.Count(); ++nDupeIter )
			{
				if ( V_strcmp( szResolve, m_ProjectOutputs[nDupeIter] ) == 0 )
				{
					bDuplicate = true;
					break;
				}
			}
			if ( !bDuplicate )
			{
				m_ProjectOutputs.AddToTail( szResolve );
			}
		}
	}

	void SetupAdditionalProjectDependencies( CProjectConfiguration *pRootConfig )
	{
		CUtlString cfgString;
		if ( VPC_GetPropertyString( KEYWORD_GENERAL, pRootConfig, NULL, g_pOption_AdditionalProjectDependencies, &cfgString ) )
		{
			CSplitString outStrings ( cfgString.Get(), ";" );
			for ( int i=0; i < outStrings.Count(); i++ )
			{
				CUtlString projectName( outStrings[i] );
				if ( !m_pDependencyProject->m_AdditionalProjectDependencies.IsValidIndex( m_pDependencyProject->m_AdditionalProjectDependencies.Find( projectName ) ) )
				{
					m_pDependencyProject->m_AdditionalProjectDependencies.AddToTail( projectName );
				}
			}
		}
	}

	void SetupBuildToolDependencies( CProjectConfiguration *pRootConfig )
	{
		CUtlVector< CUtlString > splitStrings;
		auto addFileDependencies = 
			[&] ( const char *szSemiColonDelimitedList ) -> void
			{
				if ( !szSemiColonDelimitedList || !szSemiColonDelimitedList[0] )
					return;

				splitStrings.RemoveAll();
				V_SplitString( szSemiColonDelimitedList, ";", splitStrings );

				for ( int nSplitIter = 0; nSplitIter < splitStrings.Count(); ++nSplitIter )
				{
					if ( splitStrings[nSplitIter].IsEmpty() )
						continue;
				
					char szDependencyAbsolutePath[MAX_FIXED_PATH];
					V_MakeAbsolutePath( szDependencyAbsolutePath, sizeof( szDependencyAbsolutePath ), splitStrings[nSplitIter], g_pVPC->GetProjectPath(), k_bVPCForceLowerCase );

					//copy vpc file anti-dependency from SetupFilesList()
					if ( !V_stricmp_fast( V_GetFileExtensionSafe( szDependencyAbsolutePath ), "vpc" ) )
					{
						continue;
					}

					CDependency *pOODependency = m_pDependencyGraph->FindOrCreateDependency( szDependencyAbsolutePath, k_eDependencyType_Unknown );
					if ( !m_pDependencyProject->m_Dependencies.IsValidIndex( m_pDependencyProject->m_Dependencies.Find( pOODependency ) ) )
					{
						m_pDependencyProject->m_Dependencies.AddToTail( pOODependency );
					}
				}
			};

		//Note because it threw me off. This will not contain vpc generated files from the schema step (possibly Qt as well?) during the solution build order dependency generation phase
		CUtlVector< CProjectFile * > allFiles;
		m_pVCProjGenerator->GetAllProjectFiles( allFiles );
		for ( int nFileIter = 0; nFileIter < allFiles.Count(); ++nFileIter )
		{
			CProjectFile *pProjectFile = allFiles[nFileIter];

			//additional dependencies
			if ( m_pVCProjGenerator->HasFilePropertyValue( pProjectFile, pRootConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies ) )
			{
				addFileDependencies( m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pRootConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies ) );
			}

			//order only file dependencies
			if ( m_pVCProjGenerator->HasFilePropertyValue( pProjectFile, pRootConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_OrderOnlyFileDependencies ) )
			{
				addFileDependencies( m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pRootConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_OrderOnlyFileDependencies ) );
			}

			//order only project dependencies
			if ( m_pVCProjGenerator->HasFilePropertyValue( pProjectFile, pRootConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_OrderOnlyProjectDependencies ) )
			{
				const char *szProjectDependencies = m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pRootConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_OrderOnlyProjectDependencies );
				if ( szProjectDependencies && szProjectDependencies[0] )
				{
					splitStrings.RemoveAll();
					V_SplitString( szProjectDependencies, ";", splitStrings );

					for ( int nSplitIter = 0; nSplitIter < splitStrings.Count(); ++nSplitIter )
					{
						if ( splitStrings[nSplitIter].IsEmpty() )
							continue;

						if ( !m_pDependencyProject->m_AdditionalProjectDependencies.IsValidIndex( m_pDependencyProject->m_AdditionalProjectDependencies.AddToTail( splitStrings[nSplitIter] ) ) )
						{
							m_pDependencyProject->m_AdditionalProjectDependencies.AddToTail( splitStrings[nSplitIter] );
						}
					}
				}
			}
		}
	}

public:
	// Project include directories. These strings are deleted when the object goes away.
	CUtlVector<CUtlString> m_IncludeDirectories;
	CProjectDependencyGraph *m_pDependencyGraph;
	CDependency_Project *m_pDependencyProject;
	CBaseProjectDataCollector *m_pDataCollector;
	CVCProjGenerator *m_pVCProjGenerator;
	CUtlVector<CUtlString> m_ProjectOutputs;
	CUtlString m_ScriptName;
	CUtlString m_ProjectName;
	int m_nDupeChecks;

	static CSingleProjectScanner *s_pSingleton;
};

CSingleProjectScanner *CSingleProjectScanner::s_pSingleton = NULL;
void VPC_GenerateProjectDependencies( CBaseProjectDataCollector *pDataCollector )
{
	// This hooks into CVCProjGenerator::EndProject for dependency-extraction 
	// (instantiating CSingleProjectScanner sets up CSingleProjectScanner::s_pSingleton)
	if ( !CSingleProjectScanner::s_pSingleton )
		return;
	CSingleProjectScanner::s_pSingleton->OnEndProject( pDataCollector );
}

CProjectDependencyGraph::CProjectDependencyGraph()
{
	m_iDependencyMark = 1;
	m_bFullDependencySet = false;
	m_bIncludeSystemFiles = false;
	m_bHasGeneratedDependencies = false;
}

void CProjectDependencyGraph::BuildProjectDependencies( int nBuildProjectDepsFlags, CUtlVector< projectIndex_t > *pAllowedProjects, CUtlVector< projectIndex_t > *pOverrideProjects )
{
	g_pVPC->m_bIsDependencyPass = true;

	m_nFilesParsedForIncludes = 0;

	// Have it iterate ALL projects in the list, with the current platform conditional.
	CUtlVector< projectIndex_t > projectList;
	CUtlVector< CUtlString > priorSetGames;

	// Build the list of projects to iterate:
	if ( pOverrideProjects )
	{
		// iterate just the given projects
		projectList.AddMultipleToTail( pOverrideProjects->Count(), pOverrideProjects->Base() );
		Assert( !( nBuildProjectDepsFlags & BUILDPROJDEPS_CHECK_ALL_PROJECTS ) ); // It's one or the other, bozo.
	}
	else if ( nBuildProjectDepsFlags & BUILDPROJDEPS_CHECK_ALL_PROJECTS )
	{
		// So iterate all projects.
		projectList.SetCount( g_pVPC->m_Projects.Count() );
		for ( int i=0; i < g_pVPC->m_Projects.Count(); i++ )
		{
			projectList[i] = i;
		}

		CUtlVector< projectIndex_t > everythingProjectsIndices;
		if ( g_pVPC->RestrictProjectsToEverything() )
		{
			if ( g_pVPC->GetProjectsInGroup( everythingProjectsIndices, "everything" ) )
			{
				CUtlVector< projectIndex_t > doomedProjectsIndices;
				for ( int i = 0; i < projectList.Count() ; i++ )
				{
					int nTargetProjectIndex = projectList[i];
					if ( everythingProjectsIndices.Find( nTargetProjectIndex ) == everythingProjectsIndices.InvalidIndex() )
					{
						if ( pAllowedProjects && pAllowedProjects->Find( nTargetProjectIndex ) != pAllowedProjects->InvalidIndex() )
						{
							// this project's consideration was overridden, it gets a pardon
							continue;
						}

						// the target project is not in the everything group so it gets removed
						doomedProjectsIndices.AddToTail( nTargetProjectIndex );
					}
				}

				for ( int i = 0; i < doomedProjectsIndices.Count(); i++ )
				{
					projectList.FindAndRemove( doomedProjectsIndices[i] );
				}
			}	
		}
	}
	else
	{
		// Iterate just the projects specified on the command-line
		projectList.AddMultipleToTail( g_pVPC->m_TargetProjects.Count(), g_pVPC->m_TargetProjects.Base() );
	}

	if ( projectList.Count() )
	{
		m_bIncludeSystemFiles = ( ( nBuildProjectDepsFlags & BUILDPROJDEPS_INCLUDE_SYSTEM_FILES ) != 0 );

		m_bFullDependencySet = ( ( nBuildProjectDepsFlags & BUILDPROJDEPS_FULL_DEPENDENCY_SET ) != 0 );
		if ( m_bFullDependencySet )
		{
			g_pVPC->VPCStatus( true, "\nBuilding full dependency set (all sources and headers)..." );
		}
		else
		{
			g_pVPC->VPCStatus( true, "\nBuilding partial dependency set (libs only)..." );
		}

		if ( nBuildProjectDepsFlags & BUILDPROJDEPS_CHECK_ALL_PROJECTS )
		{
			// BUILDPROJDEPS_CHECK_ALL_PROJECTS forces all games which causes all the game based projects to be iterated
			// save current state of game defines
			for ( int j=0; j<g_pVPC->m_Conditionals.Count(); j++ )
			{
				if ( g_pVPC->m_Conditionals[j]->m_Type == CONDITIONAL_GAME && g_pVPC->m_Conditionals[j]->m_bDefined )
				{
					priorSetGames.AddToTail( g_pVPC->m_Conditionals[j]->m_Name.Get() );
				}
			}

			// force all games
			g_pVPC->SetupAllGames( true );
		}

		if ( m_bFullDependencySet && !LoadCache() )
		{
			// Load any prior results so we don't have to regenerate the whole cache (which can take a couple minutes).
			g_pVPC->VPCStatus( true, "Missing or stale dependency cache file: '%s'.\nThis will take a minute to generate dependency info from all the sources.\nNext time it will have a cache file and be faster.", GetCacheFileName() );
		}

		// iterate projects, determine dependencies
		CFastTimer timer;
		timer.Start();
		g_pVPC->ClearPacifier();
		g_pVPC->IterateTargetProjects( projectList, this );
		g_pVPC->BreakPacifier();
		timer.End();

		// add in explicit dependencies
		ResolveAdditionalProjectDependencies();

		if ( m_bFullDependencySet )
		{
			// Save the expensive work we did into a cache file so it can be used next time.
			SaveCache();
		}

		if ( nBuildProjectDepsFlags & BUILDPROJDEPS_CHECK_ALL_PROJECTS )
		{
			// Restore the old game defines state
			g_pVPC->SetupAllGames( false );
			for ( int j = 0; j < priorSetGames.Count(); j++ )
			{
				g_pVPC->SetConditional( priorSetGames[j].Get(), true, CONDITIONAL_GAME );
			}
		}		

		if ( m_nFilesParsedForIncludes > 0 )
		{
			g_pVPC->VPCStatus( true, "%d files parsed in %.2f seconds for #includes.", m_nFilesParsedForIncludes, timer.GetDuration().GetSeconds() );
		}
	}

	m_bHasGeneratedDependencies = true;

	g_pVPC->m_bIsDependencyPass = false;
}

void CProjectDependencyGraph::ResolveAdditionalProjectDependencies()
{
	// projects support an explicit list of dependencies that need to be accounted for
	for ( int iMainProject=0; iMainProject < m_Projects.Count(); iMainProject++ )
	{
		CDependency_Project *pMainProject = m_Projects[iMainProject];

		// get the target project's dependency list
		for ( int i=0; i < pMainProject->m_AdditionalProjectDependencies.Count(); i++ )
		{
			const char *pLookingFor = pMainProject->m_AdditionalProjectDependencies[i].String();

			// Look for this project name among all the projects.
			int j;
			for ( j=0; j < m_Projects.Count(); j++ )
			{
				// TODO: this is broken for schemacompiler - need to port schemacompiler to win64
				//       (requires: ripping out Incredibuild integration and tweaking Buildbot)
				if ( V_stricmp_fast( m_Projects[j]->m_ProjectName.String(), pLookingFor ) == 0 )
				{
					// found
					if ( pMainProject->m_AdditionalDependencies.Find( m_Projects[j] ) == pMainProject->m_AdditionalDependencies.InvalidIndex() )
					{
						pMainProject->m_AdditionalDependencies.AddToTail( m_Projects[j] );
					}
					break;
				}
			}

			if ( g_pVPC->IsVerbose() && ( j == m_Projects.Count() ) )
			{
				// not found
				g_pVPC->VPCWarning( "Project '%s' lists '%s' in its $AdditionalProjectDependencies, but there is no project by that name.", pMainProject->GetName(), pLookingFor );
			}
		}
	}
}

bool CProjectDependencyGraph::HasGeneratedDependencies() const
{
	return m_bHasGeneratedDependencies;
}

bool CProjectDependencyGraph::VisitProject( projectIndex_t iProject, const char *szProjectName )
{
	// Read in the project.
	if ( !Sys_Exists( szProjectName ) )
	{
		return false;
	}

	g_pVPC->OutputPacifier();

	// Add this project.
	CDependency_Project *pProject = new CDependency_Project( this );
	
	char szAbsolute[MAX_FIXED_PATH];
	V_MakeAbsolutePath( szAbsolute, sizeof( szAbsolute ), szProjectName, NULL, k_bVPCForceLowerCase );
	pProject->m_Filename = szAbsolute;

	pProject->m_Type = k_eDependencyType_Project;
	pProject->m_iProjectIndex = iProject;
	m_Projects.AddToTail( pProject );
	m_AllFiles.Insert( szAbsolute, pProject );

	// Scan the project file and get all its libs, cpp, and h files.
	CSingleProjectScanner scanner;
	scanner.ScanProjectFile( this, szAbsolute, pProject );
	pProject->m_ProjectName = scanner.m_ProjectName;

	// Now add a CDependency for each output file.
	if ( !g_pVPC->UsingShallowDependencies() )
	{
		for ( int i = 0; i < scanner.m_ProjectOutputs.Count(); ++i )
		{
			const char *pFilename = scanner.m_ProjectOutputs[i].Get();

			// fixup the path and add it
			char szOutputAbsPath[MAX_FIXED_PATH];
			V_MakeAbsolutePath( szOutputAbsPath, sizeof( szOutputAbsPath ), pFilename, g_pVPC->GetProjectPath(), k_bVPCForceLowerCase );

			CDependency *pOutputDependency = FindOrCreateDependency( szOutputAbsPath, k_eDependencyType_Unknown );
			pOutputDependency->m_Dependencies.AddToTail( pProject );
			//Msg( " - ADDING DEPENDENCY: %s (%s)\n", szOutputAbsPath, pProject->m_ProjectName.Get() );
		}
	}

	return true;
}


void CProjectDependencyGraph::GetProjectDependencyTree( projectIndex_t iProject, CUtlVector<projectIndex_t> &dependentProjects, bool bDownwards )
{
	// add self
	if ( dependentProjects.Find( iProject ) == dependentProjects.InvalidIndex() )
	{
		dependentProjects.AddToTail( iProject );
	}

	// add anything that depends on it
	for ( int i=0; i < m_Projects.Count(); i++)
	{
		CDependency_Project *pProject = m_Projects[i];
		if ( pProject->m_iProjectIndex != iProject )
			continue;

		// found target project, find anything that depends on it
		for ( int iOther=0; iOther < m_Projects.Count(); iOther++ )
		{
			CDependency_Project *pOther = m_Projects[iOther];
			if ( pOther->m_iProjectIndex == iProject )
				continue;

			bool bThereIsADependency;
			if ( bDownwards )
			{
				bThereIsADependency = pProject->DependsOn( pOther, k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagCheckAdditionalDependencies | k_EDependsOnFlagRecurse | k_EDependsOnFlagTraversePastLibs );
			}
			else
			{
				bThereIsADependency = pOther->DependsOn( pProject, k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagCheckAdditionalDependencies | k_EDependsOnFlagRecurse | k_EDependsOnFlagTraversePastLibs );
			}

			if ( bThereIsADependency )
			{
				if ( dependentProjects.Find( pOther->m_iProjectIndex ) == dependentProjects.InvalidIndex() )
				{
					dependentProjects.AddToTail( pOther->m_iProjectIndex );
				}
			}
		}
	}
}

CDependency* CProjectDependencyGraph::FindDependency( const char *pFilename, CUtlPathStringHolder *pFixedFilename )
{
	// Normalize paths on entry to m_AllFiles; fix slashes, case and stuff like blah/../blah
	// (optionally returning the result to the caller, to avoid redundant work)
    CUtlPathStringHolder tmpFixupBuf;
    if ( pFixedFilename == NULL )
    {
        pFixedFilename = &tmpFixupBuf;
    }
    pFixedFilename->Set( pFilename );
    pFixedFilename->FixupPathName();

	int i = m_AllFiles.Find( pFixedFilename->Get() );
	if ( i == m_AllFiles.InvalidIndex() )
		return NULL;
	else
		return m_AllFiles[i];
}


CDependency* CProjectDependencyGraph::FindOrCreateDependency( const char *pFilename, EDependencyType type )
{
    CUtlPathStringHolder fixedFilename;
	CDependency *pDependency = FindDependency( pFilename, &fixedFilename );
	if ( pDependency )
	{
		if ( (pDependency->m_Type != type) && (type != k_eDependencyType_Unknown) )
		{
			g_pVPC->VPCWarning( "Dependency for file \"%s\" is inconsistent. Was \"%s\", changing to \"%s\"\n", pFilename, k_DependencyTypeStrings[pDependency->m_Type], k_DependencyTypeStrings[type] );
		}
		return pDependency;
	}

	// Couldn't find it. Create one (using the fixed-up filename).
	pDependency = new CDependency( this );
	pDependency->m_Filename = fixedFilename;
	m_AllFiles.Insert( fixedFilename, pDependency );

	bool bIsReadOnly;
	Sys_FileInfo( fixedFilename, pDependency->m_nCacheFileSize, pDependency->m_nCacheModificationTime, bIsReadOnly );

	pDependency->m_Type = type;
	if ( type == k_eDependencyType_Unknown )
	{
		// If the caller didn't specify, figure out the type from the filename
		if ( IsSourceFile( fixedFilename ) )
			pDependency->m_Type = k_eDependencyType_SourceFile;
		else if ( IsLibraryFile( fixedFilename ) )
			pDependency->m_Type = k_eDependencyType_Library;
		else
			pDependency->m_Type = k_eDependencyType_Unknown;
	}

	return pDependency;
}


void CProjectDependencyGraph::ClearAllDependencyMarks()
{
	if ( m_iDependencyMark == 0xFFFFFFFF )
	{
		// overflow, not expected to happen, rollover and reset
		m_iDependencyMark = 1;
		for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=m_AllFiles.Next(i) )
		{
			m_AllFiles[i]->m_iDependencyMark = 0;
		}
	}
	else
	{
		// Advance the dependency marker, all other marks become unequal and therefore unmarked.
		++m_iDependencyMark;
	}
}

const char *CProjectDependencyGraph::GetCacheFileName( void )
{
	static char cacheFile[MAX_FIXED_PATH] = {0};
	if ( g_pVPC->RestrictProjectsToEverything() )
	{
		// narrower cache file that only has dependencies based on projects in the everything group
		V_ComposeFileName( g_pVPC->GetSourcePath(), "vpc2.cache", cacheFile, sizeof( cacheFile ) );
	}
	else
	{
		V_ComposeFileName( g_pVPC->GetSourcePath(), "vpc.cache", cacheFile, sizeof( cacheFile ) );
	}
	return cacheFile;
}

bool CProjectDependencyGraph::LoadCache( void )
{
	if ( g_pVPC->IsForceRebuildCache() )
		return false;

	const char *pFilename = GetCacheFileName();
	FILE *fp = fopen( pFilename, "rb" );
	if ( !fp )
		return false;

	int nVersion;
	if ( fread( &nVersion, sizeof( nVersion ), 1, fp ) != 1 )
    {
        goto ErrClose;
    }
    
	if ( nVersion != VPC_CRC_CACHE_VERSION )
	{
		// only spew if the version number is not within one older revision
		// this allows us to invalidate the cache (i.e. format, features, etc). which just quietly rebuilds without everybody complaining about spew
		if ( nVersion && nVersion != VPC_CRC_CACHE_VERSION - 1 )
		{
			g_pVPC->VPCWarning( "Invalid dependency cache file version (expected %d, found %d) in '%s'.", VPC_CRC_CACHE_VERSION, nVersion, pFilename );
		}
        goto ErrClose;
	}

	while ( 1 )
	{
		byte bMore;
		if ( fread( &bMore, 1, 1, fp ) != 1 || bMore == 0 )
			break;

		CUtlString filename = ReadString( fp );
		
		byte nType;
		if ( !Verify( fread( &nType, 1, 1, fp ) == 1 ) )
		{
			goto ErrClose;
		}
		
		CDependency *pDep = FindOrCreateDependency( filename.String(), (EDependencyType)nType );
		if ( pDep->m_Dependencies.Count() != 0 )
			g_pVPC->VPCError( "Cache loading dependency %s but it already exists!", filename.String() );

		if ( fread( &pDep->m_nCacheFileSize, sizeof( pDep->m_nCacheFileSize ), 1, fp ) != 1 )
        {
            goto ErrClose;
        }
		if ( fread( &pDep->m_nCacheModificationTime, sizeof( pDep->m_nCacheModificationTime ), 1, fp ) != 1 )
        {
            goto ErrClose;
        }

		int nDependencies;
		if ( fread( &nDependencies, sizeof( nDependencies ), 1, fp ) != 1 )
        {
            goto ErrClose;
        }
		pDep->m_Dependencies.SetCount( nDependencies );

		for ( int iDependency=0; iDependency < nDependencies; iDependency++ )
		{
			CUtlString childDepName = ReadString( fp );
			byte nChildType;
			if ( !Verify( fread( &nChildType, 1, 1, fp ) == 1 ) )
			{
				goto ErrClose;
			}

			CDependency *pChildDep = FindOrCreateDependency( childDepName.String(), (EDependencyType)nType );
			pDep->m_Dependencies[iDependency] = pChildDep;
		}
	}

	fclose( fp );

	int nOriginalEntries;
    nOriginalEntries = m_AllFiles.Count();

	CheckCacheEntries();
	RemoveDirtyCacheEntries();
	MarkAllCacheEntriesValid();

	g_pVPC->VPCStatus( true, "Loaded %d valid dependency cache entries (%d were out of date).", m_AllFiles.Count(), nOriginalEntries-m_AllFiles.Count() );
	return true;

ErrClose:
    fclose( fp );
    return false;
}

bool CProjectDependencyGraph::SaveCache( void )
{
	const char *pFilename = GetCacheFileName();
	FILE *fp = fopen( pFilename, "wb" );
	if ( !fp )
		return false;

	// Write the version.
	int version = VPC_CRC_CACHE_VERSION;
	fwrite( &version, sizeof( version ), 1, fp );

	// Write each file.
	for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=m_AllFiles.Next( i ) )
	{
		CDependency *pDep = m_AllFiles[i];
		
		// We only care about source files.
		if ( pDep->m_Type != k_eDependencyType_SourceFile )
			continue;

		// Write that there's a file here.
		byte bYesThereIsAFileHere = 1;
		fwrite( &bYesThereIsAFileHere, 1, 1, fp );

		WriteString( fp, pDep->m_Filename );
		byte nType = pDep->m_Type;
		fwrite( &nType, 1, 1, fp );
		fwrite( &pDep->m_nCacheFileSize, sizeof( pDep->m_nCacheFileSize ), 1, fp );
		fwrite( &pDep->m_nCacheModificationTime, sizeof( pDep->m_nCacheModificationTime ), 1, fp );

		int nDependencies = pDep->m_Dependencies.Count();
		fwrite( &nDependencies, sizeof( nDependencies ), 1, fp );

		for ( int iDependency=0; iDependency < pDep->m_Dependencies.Count(); iDependency++ )
		{
			WriteString( fp, pDep->m_Dependencies[iDependency]->m_Filename );
			byte nChildType = pDep->m_Dependencies[iDependency]->m_Type;
			fwrite( &nChildType, 1, 1, fp );
		}
	}

	// Write a terminator.
	byte bNoMore = 0;
	fwrite( &bNoMore, 1, 1, fp );

	fclose( fp );

	Sys_CopyToMirror( pFilename );

	return true;
}

void CProjectDependencyGraph::WriteString( FILE *fp, CUtlString &utlString )
{
	const char *pStr = utlString.String();
	int len = V_strlen( pStr );
	fwrite( &len, sizeof( len ), 1, fp );
	fwrite( pStr, len, 1, fp );
}

CUtlString CProjectDependencyGraph::ReadString( FILE *fp )
{
	int len;
	if ( fread( &len, sizeof( len ), 1, fp ) != 1 )
    {
        return CUtlString();
    }

	char *pTemp = new char[len+1];
	if ( fread( pTemp, len, 1, fp ) != 1 )
    {
        delete [] pTemp;
        return CUtlString();
    }
	pTemp[len] = 0;

	CUtlString ret = pTemp;
	delete [] pTemp;

	return ret;
}


void CProjectDependencyGraph::CheckCacheEntries()
{
	for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=m_AllFiles.Next( i ) )
	{
		CDependency *pDep = m_AllFiles[i];
		pDep->m_bCacheDirty = false;

		if ( pDep->m_Type != k_eDependencyType_SourceFile )
			continue;
	
		int64 fileSize, modTime;
		bool bIsReadOnly;
		if ( !Sys_FileInfo( pDep->m_Filename.String(), fileSize, modTime, bIsReadOnly ) ||
		     pDep->m_nCacheFileSize != fileSize ||
			 pDep->m_nCacheModificationTime != modTime )
		{
			pDep->m_bCacheDirty = true;
		}
	}
}

void CProjectDependencyGraph::RemoveDirtyCacheEntries()
{
	// NOTE: This could be waaaay more efficient by pointing files at their parents and removing all the way
	// up the chain rather than iterating over and over but this keeps the data structures simple.
	bool bAnyDirty = true;
	while ( bAnyDirty )
	{
		bAnyDirty = false;

		for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=m_AllFiles.Next( i ) )
		{
			CDependency *pDep = m_AllFiles[i];
			if ( pDep->m_bCacheDirty )
				continue;

			// If any of its children are dirty, then mark this guy as dirty and make sure to remove the child.
			for ( int iChild=0; iChild < pDep->m_Dependencies.Count(); iChild++ )
			{
				CDependency *pChild = pDep->m_Dependencies[iChild];
				if ( pChild->m_bCacheDirty )
				{
					pDep->m_bCacheDirty = true;
					bAnyDirty = true;
				}
			}
		}
	}
	
	// Now that any dirty children have flagged their parents as dirty, we can remove them.
	int iNext;
	for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=iNext )
	{
		iNext = m_AllFiles.Next( i );

		if ( m_AllFiles[i]->m_bCacheDirty )
		{
			delete m_AllFiles[ i ];
			m_AllFiles.RemoveAt( i );
		}
	}
}


void CProjectDependencyGraph::MarkAllCacheEntriesValid()
{
	for ( int i=m_AllFiles.First(); i != m_AllFiles.InvalidIndex(); i=m_AllFiles.Next( i ) )
	{
		CDependency *pDep = m_AllFiles[i];
		pDep->m_bCheckedIncludes = true;
	}
}


// This is called so we can translate from projectIndex_t to (CDependency_Project*)
class CProjectDependencyGraphProjectFilter : public IProjectIterator
{
public:
	virtual bool VisitProject( projectIndex_t iProject, const char *szProjectName )
	{
		char szAbsolute[MAX_FIXED_PATH];
		V_MakeAbsolutePath( szAbsolute, sizeof( szAbsolute ), szProjectName, NULL, k_bVPCForceLowerCase );
		
		// Ok, we've got an (absolute) project filename. Search the dependency graph for one with that name.
		bool bAdded = false;
		for ( int i=0; i < m_pAllProjectsList->Count(); i++ )
		{
			CDependency_Project *pProject = m_pAllProjectsList->Element( i );

			if ( pProject->CompareAbsoluteFilename( szAbsolute ) )
			{
				m_pOutProjectsList->AddToTail( pProject );
				bAdded = true;
				break;
			}
		}

		if ( !bAdded )
		{
			g_pVPC->VPCWarning( "Project Dependency Iteration: Project '%s' not recognized by dependency graph, skipping.\nProject is likely not part of \"Everything\" group.", szProjectName );
		}

		return true;
	}

public:
	const CUtlVector<CDependency_Project*> *m_pAllProjectsList;
	CUtlVector<CDependency_Project*> *m_pOutProjectsList;
};

void CProjectDependencyGraph::TranslateProjectIndicesToDependencyProjects( CUtlVector<projectIndex_t> &projectList, CUtlVector<CDependency_Project*> &out )
{
	CProjectDependencyGraphProjectFilter iterator;
	iterator.m_pAllProjectsList = &m_Projects;
	iterator.m_pOutProjectsList = &out;

	g_pVPC->IterateTargetProjects( projectList, &iterator );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool VPC_AreProjectDependenciesSupportedForThisTargetPlatform( void )
{
	// Only supported for platforms that use CVCProjGenerator 
	// [ CDependencyGraph was switched to use CVCProjGenerator, due to bugs in CBaseProjectDataCollector ]
	const char *pPlatformName = g_pVPC->GetTargetPlatformName();

	bool bSupported = !V_stricmp_fast( pPlatformName, "WIN32" ) || 
					  !V_stricmp_fast( pPlatformName, "WIN64" )||
					  !V_stricmp_fast( pPlatformName, "X360" ) ||
					  VPC_IsPlatformLinux( pPlatformName ) ||
					  VPC_IsPlatformAndroid( pPlatformName ) ||
					  !V_stricmp_fast( pPlatformName, "OSX32" ) ||
                      !V_stricmp_fast( pPlatformName, "OSX64" );
	return bSupported;
}

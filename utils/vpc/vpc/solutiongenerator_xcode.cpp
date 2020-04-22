//====== Copyright 1996-2016, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "vpc.h"
#include "dependencies.h"
#include "baseprojectdatacollector.h"
#include "tier1/utlsortvector.h"

#ifdef WIN32
#define snprintf _snprintf
typedef unsigned __int64 uint64_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef signed __int16 int16_t;
#else
#include <stdint.h>
#endif

#define MAX_FILE_TYPE 80

#define FOR_EACH_PROJECT_FILE( files, iter ) FOR_EACH_VEC( files, iter )

static const char *k_rgchConfigNames[] = { "Debug", "Release" };
static const char *k_rgchXCConfigFiles[] = { "debug.xcconfig", "release.xcconfig", "base.xcconfig" };

extern bool FormatRPaths( const char *pRawString, CUtlString &output, char cNewPathSeparator, const char *szOriginReplacement );

enum EOIDType
{
    EOIDTypeProject = 0x00000001,
    EOIDTypeGroup,

    EOIDTypeGroupConfigFirst,
    // These must match the config string arrays.
    EOIDTypeGroupDebug = EOIDTypeGroupConfigFirst,
    EOIDTypeGroupRelease,
    
    EOIDTypeFileReference,
    EOIDTypeBuildFile,
    EOIDTypeSourcesBuildPhase,
    EOIDTypeFrameworksBuildPhase,
    EOIDTypeCopyFilesBuildPhase,
    EOIDTypeHeadersBuildPhase,
    EOIDTypeShellScriptBuildPhase,
    EOIDTypePostBuildPhase,
    EOIDTypeNativeTarget,
    EOIDTypeAggregateTarget,
    EOIDTypeTargetDependency,
    EOIDTypeContainerItemProxy,
    EOIDTypeBuildConfiguration,
    EOIDTypeConfigurationList,
    EOIDTypeCustomBuildRule,
    EOIDTypeLegacyTarget,

    EOIDTypeMax
};

enum EOIDSpace
{
    EOIDSpaceFixed = 1,
    EOIDSpaceProjects,
    EOIDSpaceFiles,
    EOIDSpaceProjectFiles,
    EOIDSpaceConfigFiles,
    EOIDSpaceRootConfigs,
    EOIDSpaceProjectOutputFile,
    EOIDSpaceProjectGameOutputFile,
    EOIDSpaceProjectWithCustomOutputs,
    EOIDSpaceProjectConfigsFirst,
    EOIDSpaceProjectConfigDebug = EOIDSpaceProjectConfigsFirst,
    EOIDSpaceProjectConfigRelease,
    EOIDSpaceProjectLocalFrameworks,
    EOIDSpaceProjectSystemFrameworks,
    EOIDSpaceProjectSystemLibraries,

    EOIDSpaceMax
};

enum EOIDFixed
{
    EOIDFixedRoot = 1,
    EOIDFixedAllProjects,
};

static uint64_t makeoid( EOIDSpace space, uint64 nSpaceId, EOIDType type, int16_t ordinal = 0 )
{
    COMPILE_TIME_ASSERT(EOIDSpaceMax <= 256);
    COMPILE_TIME_ASSERT(EOIDTypeMax <= 256);
    return ( (uint64_t)space << 56 ) + ( (uint64_t)type << 48 ) + ( (uint64_t)(ordinal + 1) << 28 ) + nSpaceId;
}


static bool NeedsBuildFileEntry( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false; 
    else if ( IsCFileExtension( pchExtension ) || !V_stricmp_fast( pchExtension, "m" ) || !V_stricmp_fast( pchExtension, "mm" ) )
        return true;
    else if ( !V_stricmp_fast( pchExtension, "a" ) || !V_stricmp_fast( pchExtension, "dylib" ) )
        return true;
    return false;
}


static bool IsXCodeSourceFile( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false; 
    else if ( IsCFileExtension( pchExtension ) || !V_stricmp_fast( pchExtension, "m" ) || !V_stricmp_fast( pchExtension, "mm" ) ) 
        return true;
    return false;
}


static bool IsStaticLibrary( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false; 
    else if ( !V_stricmp_fast( pchExtension, "a" ) )
        return true;
    return false;
}

             
static bool IsDynamicLibrary( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false; 
    else if ( !V_stricmp_fast( pchExtension, "dylib" ) )
        return true;
    return false;
}

             
//
// We capture a bunch of project and configuration state
// in an internal data structure.  This is partly done
// to simplify code but also to give stable pointers
// for generating consistent OIDs.
//

class CProject_Xcode
{
public:

    CVCProjGenerator *m_pGenerator;
    CDependency_Project* m_pDependencyProj;
    const char *m_pConfigName;
    
    CUtlString m_projectDir;
    CUtlString m_projectOutputFile;
    
    CUtlString m_gameOutputFile;
    CUtlString m_outputFile;
    const char *m_pPrimaryOutputFile; // NULL if no output file.
    const char *m_pPrimaryOutputFileSafe; // Empty if no output file.
    EOIDSpace m_primaryOutputFileOidSpace;

    CSplitString m_systemFrameworks;
    CSplitString m_systemLibraries;
    CSplitString m_localFrameworks;

    CUtlVector<CProjectFile *> m_projectFiles;
    
    CUtlVector<CProject_Xcode *> m_additionalDependencies;

    // Indexed by configuration.
    CUtlVector<CUtlString> m_customBuildOutputFiles[2];
    CUtlVector<CUtlString> m_customBuildOutputFilesFromBuildRule[2];
    bool m_bHasCustomBuildOutputFiles;
    bool m_bCustomBuildNeedsLegacyTargetClean;
    int m_nCustomBuildRules;
    
    int m_nShellScriptPhases;

    uint64 m_nOid;

    uint64 GetProjectRelativeOid( EOIDSpace space, EOIDType type, uint16_t nOrdinal = 0 )
    {
        return makeoid( space, m_nOid, type, nOrdinal );
    }

    uint64 GetProjectConfigOid( EOIDType type, int nConfig )
    {
        return GetProjectRelativeOid((EOIDSpace)( EOIDSpaceProjectConfigsFirst + nConfig ), type );
    }

    uint64 GetLocalFrameworkOid( EOIDType type, int nIndex )
    {
        // We've checked that no index can be larger than 1,000 already.
        return GetProjectRelativeOid( EOIDSpaceProjectLocalFrameworks, type, (uint16_t)nIndex );
    }
    uint64 GetSystemFrameworkOid( EOIDType type, int nIndex )
    {
        // We've checked that no index can be larger than 1,000 already.
        return GetProjectRelativeOid( EOIDSpaceProjectSystemFrameworks, type, (uint16_t)nIndex );
    }
    uint64 GetSystemLibraryOid( EOIDType type, int nIndex )
    {
        // We've checked that no index can be larger than 1,000 already.
        return GetProjectRelativeOid( EOIDSpaceProjectSystemLibraries, type, (uint16_t)nIndex );
    }
    
    const char *GetName() const
    {
        return m_pGenerator->GetProjectName();
    }
    
    const char *GetStringProperty( configKeyword_e configKeyword, const char *pPropertyName )
    {
        return m_pGenerator->GetPropertyValueAsString( NULL, m_pConfigName, configKeyword, pPropertyName );
    }
    const char *GetConfigStringProperty( const char *pConfigName, configKeyword_e configKeyword, const char *pPropertyName )
    {
        return m_pGenerator->GetPropertyValueAsString( NULL, pConfigName, configKeyword, pPropertyName );
    }

    bool DependsOn( CProject_Xcode *pProj )
    {
        if ( pProj == this )
        {
            return false;
        }
        
        int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
        return m_additionalDependencies.HasElement( pProj ) ||
            m_pDependencyProj->DependsOn( pProj->m_pDependencyProj, dependsOnFlags );
    }

    void SetDependencyProj( CDependency_Project *pDependencyProj )
    {
        m_pDependencyProj = pDependencyProj;
        
        CUtlPathStringHolder path = pDependencyProj->m_Filename.Get();
        path.StripFilename();
        path.FixSlashesAndDotSlashes( '/' );
        m_projectDir = path;
    }

    bool IsCustomBuildOutputFileFromBuildRule( const char *pFileName ) const
    {
        CUtlStringBuilder *pVsStr = g_pVPC->GetTempStringBuffer1();
        CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pFileName, pVsStr, NULL );
		// We aren't given the usual OBJECT_FILE_DIR_normal and it would be specific
		// to our fake target anway so locate the true project's objdir.
		CUtlStringHolder<80> sObjDir( "$CONFIGURATION_TEMP_DIR/", GetName(), ".build/Objects-normal" );
        pVsStr->ReplaceFastCaseless( "$(OBJ_DIR)", sObjDir );

        CUtlPathStringHolder sFullFileName;
        GetAbsolutePath( pVsStr->Get(), &sFullFileName );

        for ( int i = 0; i < ARRAYSIZE( m_customBuildOutputFilesFromBuildRule ); i++ )
        {
            for ( int j = 0; j < m_customBuildOutputFilesFromBuildRule[i].Count(); j++ )
            {
                if ( V_stricmp_fast( sFullFileName, m_customBuildOutputFilesFromBuildRule[i][j] ) == 0 )
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool ProjectProducesBinary() const
    {
        return m_pPrimaryOutputFile != NULL;
    }

    const char *GetAbsolutePath( const char *pCheckFileName,
                                 CUtlPathStringHolder *pAbsPath,
                                 const char *pMacroCheck = NULL ) const
    {
        if ( V_IsAbsolutePath( pCheckFileName ) ||
             ( pMacroCheck && pMacroCheck[0] == '$' ) )
        {
            pAbsPath->Set( pCheckFileName );
        }
        else
        {
            pAbsPath->Set( m_projectDir, "/", pCheckFileName );
        }
        pAbsPath->FixSlashesAndDotSlashes( '/' );
        return pAbsPath->Get();
    }

    bool FileBuildsWithCustomBuildRule( CProjectFile *pProjectFile, const char *pConfigName ) const
    {
        return m_pGenerator->HasFilePropertyValue( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_CommandLine ) &&
            !m_pGenerator->HasFilePropertyValue( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies );
    }

    bool AppearsInSourcesBuildPhase( CProjectFile *pProjectFile, const char *pConfigName ) const
    {
        return IsXCodeSourceFile( pProjectFile->m_Name ) ||
            ( FileBuildsWithCustomBuildRule( pProjectFile, pConfigName ) && ProjectProducesBinary() );
    }
};

class CSolutionGenerator_Xcode : public IBaseSolutionGenerator
{
public:
    CSolutionGenerator_Xcode()
    {
        m_outBuf.SetBufferType( true, false );
    }

	virtual void GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects ) OVERRIDE;
    virtual void ProjectEnd( IBaseProjectGenerator *pCurGen ) OVERRIDE;
	virtual SolutionType_t GetSolutionType( void ) OVERRIDE { return ST_XCODE; }

private:
    const char *UsePOSIXSlashes( const char *pStr );
    bool IsFileCompletelyExcluded( CProjectFile *pProjectFile );
    void AddProject( CVCProjGenerator *pGen );
    void ProcessGenerators();
    void XcodeFileTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf );
    void XcodeProductTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf );
    void EmitBuildSettings( const char *pszProjectName, CProject_Xcode *pProj, const char *pConfigName, bool bIsDebug );
    void WriteGroup( uint64_t oid, const char *pName, const CUtlVector<CProjectFile*> &files );
    bool WriteFilesFolder( CProject_Xcode *pProj, int nFolderIndex, const char *pFolderName, const char *pExtensions, const CUtlVector<CProjectFile *> &files );
    bool EmitCustomBuildStep( CProject_Xcode *pProj, CProjectFile *pProjectFile, const char *pConfigName );
    void AddCustomBuildOutputFiles( CProject_Xcode *pProj, CProjectFile *pProjectFile, const char *pConfigName, int nConfig );
    
    void EmitBuildFileSection();
    void EmitBuildRuleSection();
    void EmitFileReferenceSection();
    void EmitGroupSection();
    void EmitSourcesBuildPhaseSection();
    void EmitFrameworksBuildPhaseSection();
    void EmitShellScriptBuildPhaseSection();
    void EmitNativeTargetSection();
    void EmitLegacyTargetSection();
    void EmitAggregateTargetSection();
    void EmitProjectSection();
    void EmitContainerItemProxySection();
    void EmitTargetDependencySection();
    void EmitBuildConfigurationSection();
    void EmitConfigurationListSection();

 	void Write( const char *pMsg, ... );

    CUtlBuffer m_outBuf;
	int m_nIndent;

    CUtlString m_solutionFilename;
    CUtlVector<CProject_Xcode> m_projects;
    
    CUtlStringBuilder m_TempFixedPath;
};

static const char* EscapeQuotes( const char *pStr, CUtlStringBuilder *pBuf )
{
    int len = V_strlen( pStr );
    pBuf->Clear();
    pBuf->EnsureCapacity( len + 30 );
    for ( int i = 0; i < len; i++ )
    {
        if ( pStr[i] == '"' )
        {
            pBuf->AppendChar( '\\' );
            pBuf->AppendChar( '\\' );
        }
        pBuf->AppendChar( pStr[i] );
    }

    return pBuf->String();
}

const char *CSolutionGenerator_Xcode::UsePOSIXSlashes( const char *pStr )
{
    m_TempFixedPath.Set( pStr );
    char *pBuf = m_TempFixedPath.Access();
    int len = (int)m_TempFixedPath.Length();
    for ( int i = 0; i < len; i++ )
    {
        if ( pBuf[i] == '\\' )
        {
            // allow escaping of bash special characters
            if ( i+1 < len && ( pBuf[i+1] != '"' && pBuf[i+1] != '$' &&
                                pBuf[i+1] != '\'' && pBuf[i+1] != '\\' ) )
                pBuf[i] = '/';
        }
        if ( pBuf[i] == '\0' )
            break;
    }
	return m_TempFixedPath;
}

bool CSolutionGenerator_Xcode::IsFileCompletelyExcluded( CProjectFile *pProjectFile )
{
    for( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
    {
        if ( g_pVPC->IsConfigAllowedBySourceFileConfigFilter( k_rgchConfigNames[iConfig] ) &&
             !pProjectFile->IsExcludedFrom( k_rgchConfigNames[iConfig] ) )
        {
            return false;
        }
    }

    return true;
}

void CSolutionGenerator_Xcode::AddProject( CVCProjGenerator *pGen )
{
	char sAbsolutePath[MAX_FIXED_PATH];

    // In many places we select properties from only one
    // configuration, the primary config.
    const char *pPrimaryConfig = "Release";

#if defined( DBGFLAG_ASSERT )
	for ( int i = 0; i < m_projects.Count(); ++i )
	{
		AssertMsg( m_projects[i].m_pGenerator != pGen, "XCode generators cannot be recycled" );
	}
#endif

    int nProjIndex = m_projects.AddToTail();
    CProject_Xcode *pProj = &m_projects[nProjIndex];

    pProj->m_nOid = nProjIndex;
    pProj->m_pGenerator = pGen;
    // Set later at generate time.
    pProj->m_pDependencyProj = NULL;
	V_MakeAbsolutePath( sAbsolutePath, sizeof( sAbsolutePath ), pGen->GetOutputFileName(), NULL, k_bVPCForceLowerCase );
    pProj->m_projectOutputFile = sAbsolutePath;
    pProj->m_pConfigName = pPrimaryConfig;
    pProj->m_nShellScriptPhases = 0;
    pProj->m_bHasCustomBuildOutputFiles = false;
    pProj->m_bCustomBuildNeedsLegacyTargetClean = false;
    pProj->m_nCustomBuildRules = 0;

    pProj->m_gameOutputFile = UsePOSIXSlashes( pProj->GetStringProperty( KEYWORD_GENERAL, g_pOption_GameOutputFile ) );
    pProj->m_outputFile = UsePOSIXSlashes( pProj->GetStringProperty( KEYWORD_LINKER, g_pOption_OutputFile ) );
    if ( pProj->m_outputFile.IsEmpty() )
    {
        pProj->m_outputFile = UsePOSIXSlashes( pProj->GetStringProperty( KEYWORD_LIBRARIAN, g_pOption_OutputFile ) );
    }
    pProj->m_primaryOutputFileOidSpace = EOIDSpaceProjectGameOutputFile;
    if ( !pProj->m_gameOutputFile.IsEmpty() )
    {
        pProj->m_pPrimaryOutputFile = pProj->m_gameOutputFile;
        pProj->m_pPrimaryOutputFileSafe = pProj->m_gameOutputFile;
    }
    else if ( !pProj->m_outputFile.IsEmpty() )
    {
        pProj->m_pPrimaryOutputFile = pProj->m_outputFile;
        pProj->m_pPrimaryOutputFileSafe = pProj->m_outputFile;
        pProj->m_primaryOutputFileOidSpace = EOIDSpaceProjectOutputFile;
    }
    else
    {
        pProj->m_pPrimaryOutputFile = NULL;
        pProj->m_pPrimaryOutputFileSafe = "";
    }

    const char *pSystemFrameworks = pProj->GetStringProperty( KEYWORD_LINKER, g_pOption_SystemFrameworks );
    pProj->m_systemFrameworks.Set( pSystemFrameworks, (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
    if ( pProj->m_systemFrameworks.Count() >= 1000 )
    {
        g_pVPC->VPCError( "Too many system frameworks in %s\n", pProj->GetName() );
    }
    
    const char *pSystemLibraries = pProj->GetStringProperty( KEYWORD_LINKER, g_pOption_SystemLibraries );
    pProj->m_systemLibraries.Set( pSystemLibraries, (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
    if ( pProj->m_systemLibraries.Count() >= 1000 )
    {
        g_pVPC->VPCError( "Too many system libraries in %s\n", pProj->GetName() );
    }
    
    const char *pLocalFrameworks = pProj->GetStringProperty( KEYWORD_LINKER, g_pOption_LocalFrameworks );
    pProj->m_localFrameworks.Set( pLocalFrameworks, (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
    if ( pProj->m_localFrameworks.Count() >= 1000 )
    {
        g_pVPC->VPCError( "Too many local frameworks in %s\n", pProj->GetName() );
    }

    // Project files and additional dependencies cannot
    // be determined until we start generation.
    // See ProcessGenerators.
}

void CSolutionGenerator_Xcode::ProcessGenerators()
{
    uint64 nFileOid = 1;
    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];

        pProj->m_pGenerator->GetAllProjectFiles( pProj->m_projectFiles );
        for ( int iFile = 0; iFile < pProj->m_projectFiles.Count(); iFile++ )
        {
            pProj->m_projectFiles[iFile]->m_nGeneratorId = nFileOid++;
        }

        for ( int i = 0; i < pProj->m_pDependencyProj->m_AdditionalProjectDependencies.Count(); i++ )
        {
            const char *pLookingFor = pProj->m_pDependencyProj->m_AdditionalProjectDependencies[i].String();
        
            int j;
            for ( j = 0; j < m_projects.Count(); j++ )
            {
                if ( V_stricmp_fast( m_projects[j].m_pDependencyProj->m_ProjectName.String(), pLookingFor ) == 0 )
                    break;
            }
        
            if ( j != m_projects.Count() )
			{
            	pProj->m_additionalDependencies.AddToTail( &m_projects[j] );
			}
        }

        for ( int iConfig = 0; iConfig < V_ARRAYSIZE( k_rgchConfigNames ); iConfig++ )
        {
            AddCustomBuildOutputFiles( pProj, NULL, k_rgchConfigNames[iConfig], iConfig );
        }
        for ( int iFile = 0; iFile < pProj->m_projectFiles.Count(); iFile++ )
        {
            for ( int iConfig = 0; iConfig < V_ARRAYSIZE( k_rgchConfigNames ); iConfig++ )
            {
                AddCustomBuildOutputFiles( pProj, pProj->m_projectFiles[iFile], k_rgchConfigNames[iConfig], iConfig );
            }
        }
    }
}

void CSolutionGenerator_Xcode::WriteGroup( uint64_t oid, const char *pName, const CUtlVector<CProjectFile*> &files )
{
	Write( "%024llX /* %s */ = {\n", oid, pName );
	++m_nIndent;
	Write( "isa = PBXGroup;\n" );
	Write( "children = (\n" );
	++m_nIndent;
    for ( int i = 0; i < files.Count(); i++ )
    {
        const char *pFileName = files[i]->m_Name;
        Write( "%024llX /* %s */,\n", makeoid( EOIDSpaceFiles, files[i]->m_nGeneratorId, EOIDTypeFileReference ), UsePOSIXSlashes( pFileName ) );
	}
    
	--m_nIndent;
	Write( ");\n" );
	Write( "name = \"%s\";\n", pName );
	Write( "sourceTree = \"<group>\";\n" );
	--m_nIndent;
	Write( "};\n" );
}


bool CSolutionGenerator_Xcode::WriteFilesFolder( CProject_Xcode *pProj, int nFolderIndex, const char *pFolderName, const char *pExtensions, const CUtlVector<CProjectFile *> &files )
{
	CUtlVector<char*> extensions;
	V_SplitString( pExtensions, ";", extensions );

    CUtlVector<CProjectFile*> perConfigFiles[V_ARRAYSIZE( k_rgchConfigNames )];
    CUtlVector<CProjectFile*> allConfigFiles;
    int nTotalFiles = 0;
    
    FOR_EACH_PROJECT_FILE( files, iFile )
	{
        CProjectFile *pProjectFile = files[iFile];
        
		// Make sure this file's extension is one of the extensions they're asking for.
		bool bInFolder = false;
		const char *pFileExtension = V_GetFileExtension( V_UnqualifiedFileName( pProjectFile->m_Name ) );
		if ( pFileExtension )
		{
			for ( int iExt = 0; iExt < extensions.Count(); iExt++ )
			{
				const char *pTestExt = extensions[iExt];
                
				if ( pTestExt[0] == '*' && pTestExt[1] == '.' && V_stricmp_fast( pTestExt + 2, pFileExtension ) == 0 )
				{
					bInFolder = true;
					break;
				}
			}
		}

        if ( bInFolder )
        {
            int nConfigMask = 0;
            for ( int iConfig = 0; iConfig < V_ARRAYSIZE( k_rgchConfigNames ); iConfig++ )
            {
                if ( g_pVPC->IsConfigAllowedBySourceFileConfigFilter( k_rgchConfigNames[iConfig] ) &&
                     !pProjectFile->IsExcludedFrom( k_rgchConfigNames[iConfig] ) )
                {
                    nConfigMask |= 1 << iConfig;
                }
            }

            // If a file is in only one configuration put it there,
            // otherwise put it in the general bucket.
            if ( nConfigMask )
            {
                nTotalFiles++;
            
                if ( IsPowerOfTwo( nConfigMask ) )
                {
                    for ( int iConfig = 0; iConfig < V_ARRAYSIZE( k_rgchConfigNames ); iConfig++ )
                    {
                        if ( nConfigMask == ( 1 << iConfig ) )
                        {
                            perConfigFiles[iConfig].AddToTail( pProjectFile );
                        }
                    }
                }
                else
                {
                    allConfigFiles.AddToTail( pProjectFile );
                }
            }
        }
    }

    if ( !nTotalFiles )
    {
        return false;
    }
    
    // Write any per-config subgroups we need.
    for ( int iConfig = 0; iConfig < V_ARRAYSIZE( k_rgchConfigNames ); iConfig++ )
    {
        if ( !perConfigFiles[iConfig].IsEmpty() )
        {
            WriteGroup( makeoid( EOIDSpaceProjectFiles, pProj->m_nOid, (EOIDType)( EOIDTypeGroupConfigFirst + iConfig ), nFolderIndex ), k_rgchConfigNames[iConfig], perConfigFiles[iConfig] );
        }
    }

	Write( "%024llX /* %s */ = {\n", makeoid( EOIDSpaceProjectFiles, pProj->m_nOid, EOIDTypeGroup, nFolderIndex ), pFolderName );
	++m_nIndent;
	Write( "isa = PBXGroup;\n" );
	Write( "children = (\n" );
	++m_nIndent;

    // Reference any subgroups we wrote.
    for ( int iConfig = 0; iConfig < V_ARRAYSIZE( k_rgchConfigNames ); iConfig++ )
    {
        if ( !perConfigFiles[iConfig].IsEmpty() )
        {
            Write( "%024llX /* %s files for %s */,\n", makeoid( EOIDSpaceProjectFiles, pProj->m_nOid, (EOIDType)( EOIDTypeGroupConfigFirst + iConfig ), nFolderIndex ), k_rgchConfigNames[iConfig], pProj->GetName() );
        }
    }

    for ( int i = 0; i < allConfigFiles.Count(); i++ )
    {
        const char *pFileName = allConfigFiles[i]->m_Name;
        Write( "%024llX /* %s */,\n", makeoid( EOIDSpaceFiles, allConfigFiles[i]->m_nGeneratorId, EOIDTypeFileReference ), UsePOSIXSlashes( pFileName ) );
	}

	--m_nIndent;
	Write( ");\n" );
	Write( "name = \"%s\";\n", pFolderName );
	Write( "sourceTree = \"<group>\";\n" );
	--m_nIndent;
	Write( "};\n" );

    return true;
}

             
bool CSolutionGenerator_Xcode::EmitCustomBuildStep( CProject_Xcode *pProj, CProjectFile *pProjectFile, const char *pConfigName )
{
    if ( !g_pVPC->IsConfigAllowedBySourceFileConfigFilter( pConfigName ) )
    {
        return false;
    }

    const char *pProjectDir = pProj->m_projectDir;
    const char *pFileName = NULL;
    CUtlPathStringHolder sFixedFileName; 

    if ( pProjectFile )
    {
        pFileName = pProjectFile->m_Name.String();
        if ( !pProj->m_pGenerator->HasFilePropertyValue( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_CommandLine ) ||
             !pProj->m_pGenerator->HasFilePropertyValue( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_Outputs ) )
        {
            return false;
        }

        // DoStandardVisualStudioReplacements needs to know where the file is, so make sure it's got a path on it
        pFileName = pProj->GetAbsolutePath( pFileName, &sFixedFileName );
    }

    const char *pCustomBuildCommandLine = pProj->m_pGenerator->GetPropertyValueAsString( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_CommandLine );
    if ( !pCustomBuildCommandLine[0] )
    {
        return false;
    }
    
    const char *pOutputFiles = pProj->m_pGenerator->GetPropertyValueAsString( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_Outputs );
    if ( !pOutputFiles[0] )
    {
        return false;
    }

    const char *pAdditionalDependencies = pProj->m_pGenerator->GetPropertyValueAsString( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies );
    if ( pProjectFile )
    {
        // if the project produces a binary, it's a native target and we'll handle this custom build step
        // as a build rule unless the custom build has additional dependencies
        if ( pProj->ProjectProducesBinary() && !pAdditionalDependencies[0] )
        {
            return false;
        }
    }
    else if ( !pAdditionalDependencies[0] )
    {
        pAdditionalDependencies = pProj->m_pGenerator->GetPropertyValueAsString( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies_Proj );
    }

    // Check whether this is schemacompiler.  We know that schemacompiler
    // always wants to run so we can just omit inputs.  We also omit
    // any specification of outputs since we don't want Xcode to think
    // that they might have changed when the rule runs.
    bool bIsAlwaysRun = false;
    CSplitString additionalDeps( pAdditionalDependencies, ";" );
    if ( !pProjectFile && V_stricmp_fast( additionalDeps[0], "__always_run__" ) == 0 )
    {
        bIsAlwaysRun = true;
    }

    Write( "\n" );
    Write( "%024llX /* ShellScript */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeShellScriptBuildPhase, pProj->m_nShellScriptPhases++ ) );
    ++m_nIndent;
    {
        Write( "isa = PBXShellScriptBuildPhase;\n" );
        Write( "buildActionMask = 2147483647;\n" );
        Write( "files = (\n" );
        Write( ");\n" );
        Write( "inputPaths = (\n" );
        ++m_nIndent;
        if ( !bIsAlwaysRun )
        {
            if ( pFileName )
            {
                Write( "\"%s\",\n", pFileName );
            }

            FOR_EACH_VEC( additionalDeps, i )
            {
                const char *pchOneFile = additionalDeps[i];
                if ( *pchOneFile != '\0' )
                {
                    CUtlPathStringHolder sDependency;
                    Write( "\"%s\",\n", pProj->GetAbsolutePath( pchOneFile, &sDependency ) );
                }
            }
        }
        --m_nIndent;
        Write( ");\n" );

        CUtlStringBuilder *pVsStr = g_pVPC->GetTempStringBuffer1();

        CUtlString sDescription;
        const char *pCustomDescription = pProj->m_pGenerator->GetPropertyValueAsString( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_Description );
        if ( pCustomDescription[0] )
        {
            CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pCustomDescription, pVsStr, pFileName );
            sDescription.Set( pVsStr->String() );
        }
        else if ( pFileName )
        {
            pVsStr->Format( "Custom Build Step for %s", pFileName );
            sDescription = pVsStr->Access();
        }
        else
        {
            pVsStr->Format( "Custom Build Step for project %s", pProj->GetName() );
            sDescription = pVsStr->Access();
        }

        Write( "name = \"%s\";\n", sDescription.String() );

        Write( "outputPaths = (\n" );
#define TELL_XCODE_ABOUT_OUTPUT_FILES 1
#ifdef TELL_XCODE_ABOUT_OUTPUT_FILES
        // telling xcode about the output files used to cause it's dependency evaluation to
        // assume that those files had changed anytime the script had run, even if the script
        // doesn't change them, which caused us to rebuild a bunch of stuff we didn't need to rebuild
        // but testing with Xcode 6 suggests they fixed that bug, and lying less to the build system is
        // generally good.
        ++m_nIndent;
        if ( !bIsAlwaysRun )
        {
            CSplitString outFiles( pOutputFiles, ";" );
            for ( int i = 0; i < outFiles.Count(); i++ )
            {
                CBaseProjectDataCollector::DoStandardVisualStudioReplacements( outFiles[i], pVsStr, pFileName );
                pVsStr->ReplaceFastCaseless( "$(OBJ_DIR)", "${OBJECT_FILE_DIR_normal}" );
                
                CUtlPathStringHolder sOutputPath;
                
                Write( "\"%s\",\n", pProj->GetAbsolutePath( pVsStr->Get(), &sOutputPath, outFiles[i] ) );
            }
        }
        --m_nIndent;
#endif
        Write( ");\n");
        Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
        Write( "shellPath = /bin/bash;\n" );

        // Temporarily suppress environment variable dump to see if
        // it affects build perf.
        Write( "showEnvVarsInLog = 0;\n" );

        CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pCustomBuildCommandLine, pVsStr, pFileName );
        pVsStr->ReplaceFastCaseless( "$(OBJ_DIR)", "\"${OBJECT_FILE_DIR_normal}\"" );
        pVsStr->Replace( ";", ";\\n" );
        pVsStr->Replace( "\"", "\\\"" );
        // pVsStr must remain valid until used after the output file block.

        CUtlStringBuilder *pVsStr2 = g_pVPC->GetTempStringBuffer2();

        CUtlString sConditionalBlock;

        // We don't associate shell scripts with configurations, they are just
        // build phases on targets.  In order to make sure the right one
        // runs for the current configuration we conditionalize here.
        pVsStr2->Format( "if [ \\\"%s\\\" != \\\"${CONFIGURATION}\\\" ]; then\\necho Rule for %s, ignoring\\nexit 0;\\nfi;\\n", pConfigName, pConfigName );
        sConditionalBlock = pVsStr2->Access();
        
        // this is something of a dirty ugly hack.  it seems that xcode wants to run your custom shell
        // scripts anytime the pbxproj has changed (which makes some sense - the script might be different)
        // since we generate one big project, any vpc change means we'll run all the custom
        // build steps again, which will generate code, and link code, and generally take time
        // so if this project was up-to-date (i.e. no vpc changes), add an early out that checks if
        // all the output files are newer than the input files and early out if that's the case

        // We only need to make this check if the custom rule is for a specific file.
        if ( pProjectFile )
        {
            sConditionalBlock += CFmtStrMax( "export CANARY_FILE=\\\"%s\\\";\\n", pProj->m_projectOutputFile.String() ).Access();
            // uncomment this line to debug the embedded shell script
            // sConditionalBlock += "set -x\\n";
            sConditionalBlock += "EARLY_OUT=1\\n"
                "let LI=$SCRIPT_INPUT_FILE_COUNT-1\\n"
                "let LO=$SCRIPT_OUTPUT_FILE_COUNT-1\\n"
                "for j in $(seq 0 $LO); do\\n"
                "    OUTPUT=SCRIPT_OUTPUT_FILE_$j\\n"
                "    if [ \\\"${CANARY_FILE}\\\" -nt \\\"${!OUTPUT}\\\" ]; then\\n"
                "        EARLY_OUT=0\\n"
                "        break\\n"
                "    fi\\n"
                "    for i in $(seq 0 $LI); do\\n"
                "        INPUT=SCRIPT_INPUT_FILE_$i\\n"
                "        if [ \\\"${!INPUT}\\\" -nt \\\"${!OUTPUT}\\\" ]; then\\n"
                "            EARLY_OUT=0\\n"
                "            break 2\\n"
                "        fi\\n"
                "    done\\n"
                "done\\n";
                                
            sConditionalBlock += "if [ $EARLY_OUT -eq 1 ]; then\\n"
                "    echo \\\"outputs are newer than input, skipping execution...\\\"\\n"
                "    exit 0\\n"
                "fi\\n";
        }
                    
        Write( "shellScript = \"cd %s;\\n"
               "%s"
               "%s\";\n", pProjectDir, sConditionalBlock.String(), pVsStr->String() );
    }
    --m_nIndent;
    Write( "};" );

    return true;
}


void CSolutionGenerator_Xcode::AddCustomBuildOutputFiles( CProject_Xcode *pProj, CProjectFile *pProjectFile, const char *pConfigName, int nConfig )
{
    if ( !g_pVPC->IsConfigAllowedBySourceFileConfigFilter( pConfigName ) )
    {
        return;
    }

    const char *pFileName = NULL;
    CUtlPathStringHolder sFixedFileName; 
    if ( pProjectFile )
    {
        if ( !pProj->m_pGenerator->HasFilePropertyValue( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_Outputs ) )
        {
            return;
        }

        // DoStandardVisualStudioReplacements needs to know where the file is, so make sure it's got a path on it
        pFileName = pProjectFile->m_Name.String();
        pFileName = pProj->GetAbsolutePath( pFileName, &sFixedFileName );
    }
    
    const char *pOutputFiles = pProj->m_pGenerator->GetPropertyValueAsString( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_Outputs );
    if ( !pOutputFiles[0] )
    {
        return;
    }

    CUtlStringBuilder *pVsStr2 = g_pVPC->GetTempStringBuffer2();
    CSplitString outFiles( pOutputFiles, ";" );
    for ( int i = 0; i < outFiles.Count(); i++ )
    {
        const char *pOutFile = outFiles[i];
        while ( *pOutFile == ' ' )
        {
            pOutFile++;
        }
        CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pOutFile, pVsStr2, pFileName );
		// We aren't given the usual OBJECT_FILE_DIR_normal and it would be specific
		// to our fake target anway so locate the true project's objdir.
		CUtlStringHolder<80> sObjDir( "$CONFIGURATION_TEMP_DIR/", pProj->GetName(), ".build/Objects-normal" );
        pVsStr2->ReplaceFastCaseless( "$(OBJ_DIR)", sObjDir );

        CUtlPathStringHolder sOutputPath;
                
        pProj->m_customBuildOutputFiles[nConfig].AddToTail( pProj->GetAbsolutePath( pVsStr2->Get(), &sOutputPath, outFiles[i] ) );
        // If this file's custom build is going to be expressed as a custom build rule,
        // as opposed to a shell script, we need to handle it specially so remember those
        // in their own place.
        if ( pProjectFile &&
             !pProj->m_pGenerator->GetPropertyValueAsString( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies )[0] )
        {
            pProj->m_customBuildOutputFilesFromBuildRule[nConfig].AddToTail( sOutputPath.Get() );
        }
        pProj->m_bHasCustomBuildOutputFiles = true;
        // We could avoid emitting a special clean target if we're sure that all outputs
        // are within the generated file directories as they'll be cleaned automatically,
        // but source2 puts generated files in the source tree so we pretty much always
        // need special cleaning.
        pProj->m_bCustomBuildNeedsLegacyTargetClean = true;
    }
}


void CSolutionGenerator_Xcode::XcodeFileTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        snprintf( pchOutBuf, cchOutBuf, "compiled.mach-o.executable" );  
	else if ( !V_stricmp_fast( pchExtension, "c" )  || !V_stricmp_fast( pchExtension, GENERATED_CPP_FILE_EXTENSION ) )
		snprintf( pchOutBuf, cchOutBuf, "sourcecode.cpp.cpp" );
    else if ( IsCFileExtension( pchExtension ) || IsHFileExtension( pchExtension ) )
        snprintf( pchOutBuf, cchOutBuf, "sourcecode.cpp.%s", pchExtension );
    else if ( !V_stricmp_fast( pchExtension, "m" ) || !V_stricmp_fast( pchExtension, "mm" ) )
        snprintf( pchOutBuf, cchOutBuf, "sourcecode.objc.%s", pchExtension );
    else if ( !V_stricmp_fast( pchExtension, "a" ) )
        snprintf( pchOutBuf, cchOutBuf, "archive.ar" );
    else if ( !V_stricmp_fast( pchExtension, "dylib" ) )
    {
        const char *pszLibName = V_UnqualifiedFileName( pszFileName );
        if ( pszLibName[0] == 'l' && pszLibName[1] == 'i' && pszLibName[2] == 'b' )
            snprintf( pchOutBuf, cchOutBuf, "compiled.mach-o.dylib" );
        else
            snprintf( pchOutBuf, cchOutBuf, "compiled.mach-o.bundle" );
    }
    else if ( !V_stricmp_fast( pchExtension, "pl" ) )
		snprintf( pchOutBuf, cchOutBuf, "text.script.perl" );
    else
        snprintf( pchOutBuf, cchOutBuf, "text.plain" );
}


void CSolutionGenerator_Xcode::XcodeProductTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.tool" );  
    else if ( !V_stricmp_fast( pchExtension, "a" ) )
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.library.static" );
    else if ( !V_stricmp_fast( pchExtension, "dylib" ) )
    {
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.library.dynamic" );
#if 0
        const char *pszLibName = V_UnqualifiedFileName( pszFileName );
        if ( pszLibName[0] != 'l' || pszLibName[1] != 'i' || pszLibName[2] != 'b' )
            snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.bundle" );     
#endif
    }
    else
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.unknown" );
}


void CSolutionGenerator_Xcode::EmitBuildSettings( const char *pszProjectName, CProject_Xcode *pProj, const char *pConfigName, bool bIsDebug )
{
    if ( !pProj )
    {
        Write( "PRODUCT_NAME = \"%s\";\n", pszProjectName );
        return;
    }

//	Write( "CC = \"$(SOURCE_ROOT)/devtools/bin/osx32/xcode_ccache_wrapper\";\n" );
//	Write( "LDPLUSPLUS = \"$(DT_TOOLCHAIN_DIR)/usr/bin/clang++\";\n" );
	
	Write( "ARCHS = (\n" );
	{
		++m_nIndent;

        if ( !V_stricmp_fast( g_pVPC->GetTargetPlatformName(), "OSX32" ) )
        {
            Write( "i386,\n" );
            if ( Sys_StringToBool( pProj->GetConfigStringProperty( pConfigName, KEYWORD_GENERAL, g_pOption_BuildMultiArch ) ) )
            {
                Write( "x86_64,\n" );
            }
        }
        else
        {
            Write( "x86_64,\n" );
        }
        
		--m_nIndent;
	}
	Write( ");\n" );

    auto pDictFiles = &pProj->m_projectFiles;
    
    if ( pProj->m_pPrimaryOutputFile )
    {
        CUtlPathStringHolder sFilename( pProj->m_pPrimaryOutputFile );
		sFilename.StripExtension();
        Write( "PRODUCT_NAME = \"%s\";\n", V_UnqualifiedFileName( sFilename.Get() ) );

        const char *pExtraLinkerFlags = pProj->GetConfigStringProperty( pConfigName, KEYWORD_LINKER, g_pOption_GCC_ExtraLinkerFlags );
        if ( V_strlen( pExtraLinkerFlags ) )
            Write( "OTHER_LDFLAGS = \"%s\";\n", pExtraLinkerFlags );

		CUtlString formattedRPaths;
		FormatRPaths( pProj->GetConfigStringProperty( pConfigName, KEYWORD_LINKER, g_pOption_POSIX_RPaths ), formattedRPaths, ' ', "@loader_path" );
		if ( formattedRPaths.Length() )
		{
			Write( "LD_RUNPATH_SEARCH_PATHS = \"%s\";\n", formattedRPaths.Get() );
		}

		CUtlStringBuilder sOtherCompilerCFlags = "OTHER_CFLAGS = \"$(OTHER_CFLAGS) ";
		CUtlStringBuilder sOtherCompilerCPlusFlags = "OTHER_CPLUSPLUSFLAGS = \"$(OTHER_CPLUSPLUSFLAGS) ";
		
        const char *pExtraCompilerFlags = pProj->GetConfigStringProperty( pConfigName, KEYWORD_COMPILER, g_pOption_GCC_ExtraCompilerFlags );
		if ( V_strlen( pExtraCompilerFlags ) )
        {
			sOtherCompilerCFlags += pExtraCompilerFlags;
			sOtherCompilerCPlusFlags += pExtraCompilerFlags;
        }

        const char *pExtraCxxCompilerFlags = pProj->GetConfigStringProperty( pConfigName, KEYWORD_COMPILER, g_pOption_GCC_ExtraCxxCompilerFlags );
		if ( V_strlen( pExtraCxxCompilerFlags ) )
        {
            sOtherCompilerCPlusFlags += " ";
			sOtherCompilerCPlusFlags += pExtraCxxCompilerFlags;
        }

        const char *pSymbolVisibility = pProj->GetConfigStringProperty( pConfigName, KEYWORD_COMPILER, g_pOption_SymbolVisibility );
        // base.xcconfig defaults visibility to hidden so only change it if we need to.
        if ( pSymbolVisibility[0] && V_stricmp_fast( pSymbolVisibility, "hidden" ) != 0 )
        {
            CUtlStringHolder<50> sVis( " -fvisibility=", pSymbolVisibility );
            sOtherCompilerCFlags += sVis.Get();
            sOtherCompilerCPlusFlags += sVis.Get();
        }

        const char *pForceInclude = pProj->GetConfigStringProperty( pConfigName, KEYWORD_COMPILER, g_pOption_ForceInclude );
        if ( V_strlen( pForceInclude ) )
        {
			CSplitString outStrings( pForceInclude, (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
			for ( int i = 0; i < outStrings.Count(); i++ )
			{
				if ( V_strlen( outStrings[i] ) > 2 )
				{	
					CUtlStringHolder<200> szIncludeLine( " -include ", UsePOSIXSlashes( outStrings[i] ) );
					sOtherCompilerCFlags += szIncludeLine.Get();
					sOtherCompilerCPlusFlags += szIncludeLine.Get();
				}
			}			
        }		
		
		sOtherCompilerCFlags += "\";\n" ;
		sOtherCompilerCPlusFlags += "\";\n" ;
		
		Write( sOtherCompilerCFlags.Get() );
		Write( sOtherCompilerCPlusFlags.Get() );
        
        if ( IsDynamicLibrary( pProj->m_pPrimaryOutputFileSafe ) )
        {
            Write( "MACH_O_TYPE = mh_dylib;\n" );
            Write( "LD_DYLIB_INSTALL_NAME = \"@rpath/%s.dylib\";\n", V_UnqualifiedFileName( sFilename.Get() ) );
        }

		if ( IsStaticLibrary( pProj->m_pPrimaryOutputFileSafe ) )
		{
			Write( "DEBUG_INFORMATION_FORMAT = dwarf;\n" );
		}
    }
    else
        Write( "PRODUCT_NAME = \"%s\";\n", pszProjectName );

    // add our header search paths
    const char *pAdditionalIncludeDirectories = pProj->GetConfigStringProperty( pConfigName, KEYWORD_COMPILER, g_pOption_AdditionalIncludeDirectories );
    CSplitString outStrings( pAdditionalIncludeDirectories, (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
    if ( outStrings.Count() )
    {
        // start the iquote list with the project directory
        CUtlPathStringHolder sIncludeDir( pProj->m_projectDir );
#ifdef STEAM
        V_StripPrecedingAndTrailingWhitespace( sIncludeDir.GetForModify() );
#endif

        Write( "USER_HEADER_SEARCH_PATHS = (\n" );
        ++m_nIndent;
#ifdef STEAM
        Write( "\"%s\",\n", sIncludeDir.Get() );
#endif
        for ( int i = 0; i < outStrings.Count(); i++ )
        {
            CUtlStringBuilder *pExpandedStr = g_pVPC->GetTempStringBuffer1();

			CBaseProjectDataCollector::DoStandardVisualStudioReplacements( outStrings[i], pExpandedStr, CFmtStrMax( "%s/dummy.txt", pProj->m_projectDir.String() ).Access() );
            pExpandedStr->ReplaceFastCaseless( "$(OBJ_DIR)", "${OBJECT_FILE_DIR_normal}" );

            pProj->GetAbsolutePath( pExpandedStr->Get(), &sIncludeDir, outStrings[i] );

#ifdef STEAM
            V_StripPrecedingAndTrailingWhitespace( sIncludeDir.GetForModify() );
#endif
            Write( "\"%s\",\n", sIncludeDir.Get() );
        }
        --m_nIndent;
        Write( ");\n" );
    }

    
    // add local frameworks we link against to the compiler framework search paths
    if ( pProj->m_localFrameworks.Count() )
    {
        Write( "FRAMEWORK_SEARCH_PATHS = (\n" );
        ++m_nIndent;
        {
            Write( "\"$(inherited)\",\n" );
            for ( int i = 0; i < pProj->m_localFrameworks.Count(); i++ )
            {
                CUtlPathStringHolder rgchFrameworkPath;

                pProj->GetAbsolutePath( pProj->m_localFrameworks[i], &rgchFrameworkPath );
                rgchFrameworkPath.StripFilename();
                
                Write( "\"%s\",\n", rgchFrameworkPath.Get() );
            }
        }
        --m_nIndent;
        Write( ");\n" );
    }

    // add our needed preprocessor definitions
    const char *pPreprocessorDefinitions = pProj->GetConfigStringProperty( pConfigName, KEYWORD_COMPILER, g_pOption_PreprocessorDefinitions );
    CSplitString preprocessorDefines( pPreprocessorDefinitions, (const char**)g_IncludeSeparators, V_ARRAYSIZE(g_IncludeSeparators) );
    if ( preprocessorDefines.Count() )
    {
        Write( "GCC_PREPROCESSOR_DEFINITIONS = (\n" );
        ++m_nIndent;
        {
            Write( "\"$(GCC_PREPROCESSOR_DEFINITIONS)\",\n" );
            for ( int i = 0; i < preprocessorDefines.Count(); i++ )
            {
                Write( "\"%s\",\n", preprocessorDefines[i] );
            }
        }
        --m_nIndent;
        Write( ");\n" );
    }

    if ( !IsStaticLibrary( pProj->m_pPrimaryOutputFileSafe ) )
    {
        bool bDisableDeadCodeElimination = false;
        const char *pDisableDeadCodeElimination = pProj->GetConfigStringProperty( pConfigName, KEYWORD_LINKER, g_pOption_DisableLinkerDeadCodeElimination );
        if ( pDisableDeadCodeElimination[0] )
        {
            bDisableDeadCodeElimination = Sys_StringToBool( pDisableDeadCodeElimination );
        }
        Write( "DEAD_CODE_STRIPPING = %s;\n", bDisableDeadCodeElimination ? "NO" : "YES" );
    }

    bool bTreatWarningsAsErrors = Sys_StringToBool( pProj->GetConfigStringProperty( pConfigName, KEYWORD_COMPILER, g_pOption_TreatWarningsAsErrors ) );
    Write( "GCC_TREAT_WARNINGS_AS_ERRORS = %s;\n", bTreatWarningsAsErrors ? "YES" : "NO" );

    if ( pDictFiles )
    {
        CUtlMap<const char *, bool> searchPaths;
        searchPaths.SetLessFunc( StringLessThan );
        // libraries we consume (specified in our files list)
        FOR_EACH_PROJECT_FILE( *pDictFiles, iFile )
        {
			CProjectFile *pProjectFile = (*pDictFiles)[iFile];
            const char *pFileName = pProjectFile->m_Name.String();
            if ( pProjectFile->m_iFlags & (VPC_FILE_FLAGS_STATIC_LIB | VPC_FILE_FLAGS_IMPORT_LIB) )
            {
                CUtlPathStringHolder rgchLibPath;

                pProj->GetAbsolutePath( pFileName, &rgchLibPath );
                rgchLibPath.StripFilename();
                int nIndex = searchPaths.Find( rgchLibPath );
                if ( nIndex == searchPaths.InvalidIndex() )
                {
                    char *pszLibPath = new char[V_strlen( rgchLibPath ) + 1];
                    V_strncpy( pszLibPath, rgchLibPath, V_strlen( rgchLibPath ) + 1 ); 
                    nIndex = searchPaths.Insert( pszLibPath );
                }
            }
        }
        
        // add the library path we know we need to reference
        Write( "LIBRARY_SEARCH_PATHS = (\n" );
        ++m_nIndent;
        {
            Write( "\"$(inherited)\",\n" );
            FOR_EACH_DICT_FAST( searchPaths, iSearchPath )
            {
                Write( "\"%s\",\n", searchPaths.Key(iSearchPath) );
                delete [] searchPaths.Key(iSearchPath);
            }
        }
        --m_nIndent;
        Write( ");\n" );
    }    
}

class CStringLess
{
public:
	bool Less( const char *lhs, const char *rhs, void *pCtx )
	{
		return ( V_strcmp( lhs, rhs ) < 0 ? true : false );
	}
};

class CUtlStringLess
{
public:
	bool Less( const CUtlString &lhs, const CUtlString &rhs, void *pCtx )
	{
		return ( V_strcmp( lhs.Get(), rhs.Get() ) < 0 ? true : false );
	}
};

void CSolutionGenerator_Xcode::EmitBuildFileSection()
{
    /** 
     **
     ** buildfiles - any file that's involved in, or the output of, a build phase
     **
     **/

    Write( "\n/* Begin PBXBuildFile section */" );
    ++m_nIndent;
    
    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];
        
        // the files this project references
        auto pDictFiles = &pProj->m_projectFiles;
        FOR_EACH_PROJECT_FILE( *pDictFiles, iFile )
        {        
            CProjectFile *pProjectFile = (*pDictFiles)[iFile];
            const char *pFileName = pProjectFile->m_Name.String();
            
            CUtlPathStringHolder rgchFilePath;

            pProj->GetAbsolutePath( pFileName, &rgchFilePath );
            
            if ( IsFileCompletelyExcluded( pProjectFile ) )
            {
                g_pVPC->VPCStatus( false, "xcode: excluding File %s\n", pFileName );
                continue;
            }
            
            // dynamic files - generated as part of the build - may be automatically added to the build set by xcode,
            // if we add them twice, bad things (duplicate symbols) happen.
            bool bIsDynamicFile = ( pProjectFile->m_iFlags & VPC_FILE_FLAGS_DYNAMIC ) != 0;
            const char *pConfigName = k_rgchConfigNames[1]; // Retail
            
            // if we have a custom build step, we need to include this file in the build set
            if ( ( !bIsDynamicFile || !pProj->IsCustomBuildOutputFileFromBuildRule( pFileName ) ) && pProj->AppearsInSourcesBuildPhase( pProjectFile, pConfigName ) )
            {
                Write( "\n" );
                Write( "%024llX /* %s in Sources */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; };", makeoid( EOIDSpaceFiles, pProjectFile->m_nGeneratorId, EOIDTypeBuildFile ), V_UnqualifiedFileName( pFileName ), makeoid( EOIDSpaceFiles, pProjectFile->m_nGeneratorId, EOIDTypeFileReference ), V_UnqualifiedFileName( pFileName ) );
            }
            
            if ( pProjectFile->m_iFlags & VPC_FILE_FLAGS_IMPORT_LIB )
            {
                Write( "\n" );
                Write( "%024llX /* %s in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; };", makeoid( EOIDSpaceFiles, pProjectFile->m_nGeneratorId, EOIDTypeBuildFile ), V_UnqualifiedFileName( pFileName ), makeoid( EOIDSpaceFiles, pProjectFile->m_nGeneratorId, EOIDTypeFileReference ), V_UnqualifiedFileName( pFileName ) );
            }
            
            if ( pProjectFile->m_iFlags & VPC_FILE_FLAGS_STATIC_LIB )
            {
                const char *pszLibPfx = "";
                Write( "\n" );
                Write( "%024llX /* %s%s in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s%s */; };", makeoid( EOIDSpaceFiles, pProjectFile->m_nGeneratorId, EOIDTypeBuildFile ), pszLibPfx, V_UnqualifiedFileName( pFileName ), makeoid( EOIDSpaceFiles, pProjectFile->m_nGeneratorId, EOIDTypeFileReference ), pszLibPfx, V_UnqualifiedFileName( pFileName ) );
            }
        }
        
        // system libraries we link against
        for ( int i = 0; i < pProj->m_systemLibraries.Count(); i++ )
        {
            Write( "\n" );
            Write( "%024llX /* lib%s.dylib in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* lib%s.dylib */; };", 
                   pProj->GetSystemLibraryOid( EOIDTypeBuildFile, i ), pProj->m_systemLibraries[i], 
                   pProj->GetSystemLibraryOid( EOIDTypeFileReference, i ), pProj->m_systemLibraries[i] );
        }
        
        // system frameworks we link against
        for ( int i = 0; i < pProj->m_systemFrameworks.Count(); i++ )
        {
            Write( "\n" );
            Write( "%024llX /* %s.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s.framework */; };", 
                   pProj->GetSystemFrameworkOid( EOIDTypeBuildFile, i ), pProj->m_systemFrameworks[i], 
                   pProj->GetSystemFrameworkOid( EOIDTypeFileReference, i ), pProj->m_systemFrameworks[i] );
        }
        
        // local frameworks we link against
        for ( int i = 0; i < pProj->m_localFrameworks.Count(); i++ )
        {
            CUtlStringHolder<80> rgchFrameworkName( V_UnqualifiedFileName( pProj->m_localFrameworks[i] ) );
            rgchFrameworkName.StripExtension();
            
            Write( "\n" );
            Write( "%024llX /* %s.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s.framework */; };", 
                   pProj->GetLocalFrameworkOid( EOIDTypeBuildFile, i ), rgchFrameworkName.Get(), 
                   pProj->GetLocalFrameworkOid( EOIDTypeFileReference, i ), rgchFrameworkName.Get() );
        }

        // look at everyone who depends on us, and emit a build file pointing at our output file for each of them
        const char *pszOutputFile = pProj->m_gameOutputFile;
        EOIDSpace outputFileOidSpace = EOIDSpaceProjectGameOutputFile;
        CUtlString sOutputFile = V_UnqualifiedFileName( pszOutputFile );
        if ( !sOutputFile.Length() || IsStaticLibrary( sOutputFile.Get() ) )
        {
            pszOutputFile = pProj->m_outputFile;
            outputFileOidSpace = EOIDSpaceProjectOutputFile;
            sOutputFile = V_UnqualifiedFileName( pszOutputFile );
        } 
        if ( sOutputFile.Length() && ( IsStaticLibrary( sOutputFile.Get() ) || IsDynamicLibrary( sOutputFile.Get() ) ) )
        {
            for ( int iTestProject = 0; iTestProject < m_projects.Count(); iTestProject++ )
            {
                if ( m_projects[iTestProject].DependsOn( pProj ) )
                {
                    Write( "\n" );
                    Write( "%024llX /* %s */ = {isa = PBXBuildFile; fileRef = %024llX /* %s - depended on by %s */; };", 
                           makeoid( outputFileOidSpace, pProj->m_nOid, EOIDTypeBuildFile, iTestProject ), 
                           sOutputFile.String(), 
                           makeoid( outputFileOidSpace, pProj->m_nOid, EOIDTypeFileReference ), 
                           sOutputFile.String(),
                           m_projects[iTestProject].GetName() );
                }
            }
        }
        
        // and our output file
        if ( sOutputFile.Length() ) 
        {
            Write( "\n" );
            Write( "%024llX /* %s in Products */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; };", 
                   makeoid( outputFileOidSpace, pProj->m_nOid, EOIDTypeBuildFile, -1 ), 
                   sOutputFile.String(), 
                   makeoid( outputFileOidSpace, pProj->m_nOid, EOIDTypeFileReference ), 
                   sOutputFile.String() );
        }
    }

    --m_nIndent;
    Write( "\n/* End PBXBuildFile section */\n" );
}

void CSolutionGenerator_Xcode::EmitBuildRuleSection()
{
    /** 
     **
     ** build rule - implement custom actions
     **
     **/
    Write( "\n/*Begin PBXBuildRule section */\n" );
    ++m_nIndent;

    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];
        if ( !pProj->ProjectProducesBinary() )
            continue;
          
        // we don't have an output file - wander the list of files, looking for custom build steps
        // if we find any, magic up shell scripts to run them
        // the files this project references
        auto pDictFiles = &pProj->m_projectFiles;
        FOR_EACH_PROJECT_FILE( *pDictFiles, iFile )
        {        
            CProjectFile *pProjectFile = (*pDictFiles)[iFile];
            const char *pFileName = pProjectFile->m_Name.String();
            const char *pConfigName = k_rgchConfigNames[1]; // Retail

            // Build rules are only used for files with custom build commands, not projects.
            if ( !pProj->m_pGenerator->HasFilePropertyValue( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_Outputs ) )
            {
                continue;
            }
            
            // custom build rules with additional dependencies don't map to pbxbuildrules, we handle them
            // as custom script phases
            if ( pProj->m_pGenerator->HasFilePropertyValue( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies ) )
            {
                continue;
            }

            const char *pCustomBuildCommandLine = pProj->m_pGenerator->GetPropertyValueAsString( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_CommandLine );
            if ( !pCustomBuildCommandLine[0] )
            {
                continue;
            }
    
            const char *pOutputFiles = pProj->m_pGenerator->GetPropertyValueAsString( pProjectFile, pConfigName, KEYWORD_CUSTOMBUILDSTEP, g_pOption_Outputs );
            if ( !pOutputFiles[0] )
            {
                continue;
            }

            Write( "\n" );
            Write( "%024llX /* PBXBuildRule */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeCustomBuildRule, pProj->m_nCustomBuildRules++ ) );
            ++m_nIndent;
            {
                Write( "isa = PBXBuildRule;\n" );
                Write( "compilerSpec = com.apple.compilers.proxy.script;\n" );

                CUtlPathStringHolder sInputFile;
                
                // DoStandardVisualStudioReplacements needs to know where the file is, so make sure it's got a path on it
                pProj->GetAbsolutePath( pFileName, &sInputFile );

                Write( "filePatterns = \"%s\";\n", sInputFile.Get() );
                Write( "fileType = pattern.proxy;\n" );
                Write( "isEditable = 1;\n" );
                
                CUtlStringBuilder *pVsStr = g_pVPC->GetTempStringBuffer1();

                CSplitString outFiles( pOutputFiles, ";" );

                Write( "outputFiles = (\n" );
                ++m_nIndent;
                {
                    for ( int i = 0; i < outFiles.Count(); i++ )
                    {
                        CBaseProjectDataCollector::DoStandardVisualStudioReplacements( outFiles[i], pVsStr, sInputFile );
                        pVsStr->ReplaceFastCaseless( "$(OBJ_DIR)", "${OBJECT_FILE_DIR_normal}" );
                        
                        CUtlPathStringHolder sOutputPath;

                        Write( "\"%s\",\n", pProj->GetAbsolutePath( pVsStr->Get(), &sOutputPath, outFiles[i] ) );
                    }
                }
                --m_nIndent;
                Write( ");\n");

                Write( "outputFilesCompilerFlags = (\n" );
                ++m_nIndent;
                {
                    for ( int i = 0; i < outFiles.Count(); i++ )
                    {
                        Write( "\"-x c++\",\n" );
                    }
                }
                --m_nIndent;
                Write( ");\n");

                CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pCustomBuildCommandLine, pVsStr, sInputFile );
                pVsStr->ReplaceFastCaseless( "$(OBJ_DIR)", "\"${OBJECT_FILE_DIR_normal}\"" );
                pVsStr->ReplaceFastCaseless( ";", ";\\n" );
                pVsStr->ReplaceFastCaseless( "\"", "\\\"" );
                        
                Write( "script = \"#!/bin/bash\\n"
                       "cd %s\\n"
                       "%s\\n"
                       "exit $?\";\n", pProj->m_projectDir.Get(), pVsStr->Get() );
            }
            --m_nIndent;
            Write( "};" );
        }
    }

    --m_nIndent;
    Write( "\n/*End PBXBuildRule section */\n" );
}

void CSolutionGenerator_Xcode::EmitFileReferenceSection()
{
    /** 
     **
     ** file references - any file that appears in the project browser
     **
     **/
    Write( "\n/* Begin PBXFileReference section */" );
    ++m_nIndent;
    
    // include the xcconfig files
    for ( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchXCConfigFiles); iConfig++ )
    {
        CUtlPathStringHolder rgchFilePath( m_solutionFilename, ".xcodeproj/../devtools/", k_rgchXCConfigFiles[iConfig] );
        rgchFilePath.FixSlashesAndDotSlashes( '/' );
        
        Write( "\n" );
        Write( "%024llX /* %s */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = text.xcconfig; name = \"%s\"; path = \"%s\"; sourceTree = \"<absolute>\"; };",
               makeoid( EOIDSpaceConfigFiles, iConfig, EOIDTypeFileReference ),
               k_rgchXCConfigFiles[iConfig], 
               k_rgchXCConfigFiles[iConfig],
               rgchFilePath.Get()
            );
    }
                
    FOR_EACH_VEC( m_projects, iProject )
    {                    
        CProject_Xcode *pProj = &m_projects[iProject];
        
        auto pDictFiles = &pProj->m_projectFiles;
        FOR_EACH_PROJECT_FILE( *pDictFiles, iFile )
        {        
            CProjectFile *pProjectFile = (*pDictFiles)[iFile];
            if ( IsFileCompletelyExcluded( pProjectFile ) )
            {
                continue;
            }
            
            const char *pFileName = pProjectFile->m_Name.String();
            const char *pUnqualFileName = V_UnqualifiedFileName( pFileName );
            
            CUtlPathStringHolder rgchFilePath;

            pProj->GetAbsolutePath( pFileName, &rgchFilePath );
            
            char rgchFileType[MAX_FILE_TYPE];
            XcodeFileTypeFromFileName( pUnqualFileName, rgchFileType, sizeof( rgchFileType ) );
            
            Write( "\n" );
            Write( "%024llX /* %s */ = {isa = PBXFileReference; fileEncoding = 4; explicitFileType = \"%s\"; name = \"%s\"; path = \"%s\"; sourceTree = \"<absolute>\"; };",
                   makeoid( EOIDSpaceFiles, pProjectFile->m_nGeneratorId, EOIDTypeFileReference ),
                   pUnqualFileName, 
                   rgchFileType,
                   pUnqualFileName,
                   rgchFilePath.Get() );
        }
                    
        // system libraries we link against
        for ( int i = 0; i < pProj->m_systemLibraries.Count(); i++ )
        {
            Write( "\n" );
            Write( "%024llX /* lib%s.dylib */ = {isa = PBXFileReference; lastKnownFileType = \"compiled.mach-o.dylib\"; name = \"lib%s.dylib\"; path = \"usr/lib/lib%s.dylib\"; sourceTree = SDKROOT; };",
                   pProj->GetSystemLibraryOid( EOIDTypeFileReference, i ),
                   pProj->m_systemLibraries[i],
                   pProj->m_systemLibraries[i],
                   pProj->m_systemLibraries[i] );
        }
        
        // system frameworks we link against
        for ( int i = 0; i < pProj->m_systemFrameworks.Count(); i++ )
        {
            Write( "\n" );
            Write( "%024llX /* %s.framework */ = {isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = %s.framework; path = System/Library/Frameworks/%s.framework; sourceTree = SDKROOT; };",
                   pProj->GetSystemFrameworkOid( EOIDTypeFileReference, i ),
                   pProj->m_systemFrameworks[i],
                   pProj->m_systemFrameworks[i],
                   pProj->m_systemFrameworks[i] );
        }

        // local frameworks we link against
        for ( int i = 0; i < pProj->m_localFrameworks.Count(); i++ )
        {
            CUtlStringHolder<80> rgchFrameworkName( V_UnqualifiedFileName( pProj->m_localFrameworks[i] ) );
            rgchFrameworkName.StripExtension();

            CUtlPathStringHolder rgchFrameworkPath;

            pProj->GetAbsolutePath( pProj->m_localFrameworks[i], &rgchFrameworkPath );

            Write( "\n" );
            Write( "%024llX /* %s.framework */ = {isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = %s.framework; path = \"%s\"; sourceTree = \"<absolute>\"; };",                        
                   pProj->GetLocalFrameworkOid( EOIDTypeFileReference, i ),
                   rgchFrameworkName.Get(),
                   rgchFrameworkName.Get(),
                   rgchFrameworkPath.Get() ); 
        }

        // include the output files (build products)                  
        const char *pszOutputFile = pProj->m_outputFile;
        CUtlString sOutputFile = V_UnqualifiedFileName( pszOutputFile );
        if ( sOutputFile.Length() )
        {
            char rgchFileType[MAX_FILE_TYPE];
            XcodeFileTypeFromFileName( sOutputFile.Get(), rgchFileType, sizeof( rgchFileType ) );
                        
            Write( "\n" );
            Write( "%024llX /* %s */ = {isa = PBXFileReference; explicitFileType = \"%s\"; includeInIndex = 0; path = \"%s\"; sourceTree = BUILT_PRODUCTS_DIR; };", 
                   makeoid( EOIDSpaceProjectOutputFile, pProj->m_nOid, EOIDTypeFileReference ), sOutputFile.String(), rgchFileType, sOutputFile.String() );
        }
                    
        // and the gameoutputfile
        CUtlString sGameOutputFile = pProj->m_gameOutputFile;
        if ( sGameOutputFile.Length() )
        {
            CUtlPathStringHolder rgchFilePath;

            pProj->GetAbsolutePath( sGameOutputFile.String(), &rgchFilePath );
                        
            char rgchFileType[MAX_FILE_TYPE];
            XcodeFileTypeFromFileName( sGameOutputFile.Get(), rgchFileType, sizeof( rgchFileType ) );
                        
            Write( "\n" );
            Write( "%024llX /* %s */ = {isa = PBXFileReference; explicitFileType = \"%s\"; includeInIndex = 0; path = \"%s\"; sourceTree = \"<absolute>\"; };", 
                   makeoid( EOIDSpaceProjectGameOutputFile, pProj->m_nOid, EOIDTypeFileReference ), V_UnqualifiedFileName( sGameOutputFile.Get() ), rgchFileType, rgchFilePath.Get() );
        }
    }

    --m_nIndent;
    Write( "\n/* End PBXFileReference section */\n" );
}

void CSolutionGenerator_Xcode::EmitGroupSection()
{
    /** 
     **
     ** groups - the file hierarchy displayed in the project
     **
     **/
    Write( "\n/* Begin PBXGroup section */\n" );
    ++m_nIndent;

    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];
        const char *pProjName = pProj->GetName();
        
        CUtlVector<char*> folderNames;
        V_SplitString( "Autogenerated Schema Files;Source Files;Header Files;Resources;VPC Files", ";", folderNames );
                    
        static const char* folderExtensions[] = 
        {
            "*.gen_cpp",
            "*.c;*.C;*.cc;*.cpp;*.cp;*.cxx;*.c++;*.prg;*.pas;*.dpr;*.asm;*.s;*.bas;*.java;*.cs;*.sc;*.e;*.cob;*.html;*.tcl;*.py;*.pl;*.m;*.mm",
            "*.h;*.H;*.hh;*.hpp;*.hxx;*.inc;*.sh;*.cpy;*.if",
            "*.plist;*.strings;*.xib;*.rc;*.proto;*.g",
            "*.vpc"
        };

        CUtlVector<const char *> foldersPresent;
        FOR_EACH_VEC( folderNames, iFolder )
        {
            if ( WriteFilesFolder( pProj, foldersPresent.Count() + 1, folderNames[iFolder], folderExtensions[iFolder], pProj->m_projectFiles ) )
            {
                foldersPresent.AddToTail( folderNames[iFolder] );
            }
        }
                    
        Write( "%024llX /* %s */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeGroup ), pProjName );
        ++m_nIndent;
        {
            Write( "isa = PBXGroup;\n" );
            Write( "children = (\n" );        
                        
            ++m_nIndent;
            {
                FOR_EACH_VEC( foldersPresent, iFolder )
                {
                    Write( "%024llX /* %s */,\n", makeoid( EOIDSpaceProjectFiles, pProj->m_nOid, EOIDTypeGroup, iFolder + 1 ), foldersPresent[iFolder] );
                }

                // system libraries we link against
                for ( int i = 0; i < pProj->m_systemLibraries.Count(); i++ )
                {
                    Write( "%024llX /* lib%s.dylib (system library) */,\n", pProj->GetSystemLibraryOid( EOIDTypeFileReference, i ), pProj->m_systemLibraries[i] );
                }
                            
                // system frameworks we link against
                for ( int i = 0; i < pProj->m_systemFrameworks.Count(); i++ )
                {
                    Write( "%024llX /* %s.framework (system framework) */,\n", pProj->GetSystemFrameworkOid( EOIDTypeFileReference, i ), pProj->m_systemFrameworks[i] );
                }
                            
                // local frameworks we link against
                for ( int i = 0; i < pProj->m_localFrameworks.Count(); i++ )
                {
                    CUtlPathStringHolder rgchFrameworkName( V_UnqualifiedFileName( pProj->m_localFrameworks[i] ) );
                    rgchFrameworkName.StripExtension();

                    Write( "%024llX /* %s.framework (local framework) */,\n", pProj->GetLocalFrameworkOid( EOIDTypeFileReference, i ), rgchFrameworkName.Get() ); 
                }

                // libraries we consume (specified in our files list)
                auto pDictFiles = &pProj->m_projectFiles;
                FOR_EACH_PROJECT_FILE( *pDictFiles, iFile )
                {
                    CProjectFile *pProjectFile = (*pDictFiles)[iFile];
                    const char *pszFileName = pProjectFile->m_Name.String();
                    CUtlString sFileName = V_UnqualifiedFileName( UsePOSIXSlashes( pszFileName ) );
                    bool bInclude = (pProjectFile->m_iFlags & VPC_FILE_FLAGS_IMPORT_LIB) != 0;
                    if ( pProjectFile->m_iFlags & VPC_FILE_FLAGS_STATIC_LIB )
                    {
                        bInclude = true;
                        FOR_EACH_VEC( m_projects, iProject2 )
                        {
                            // don't include static libs generated by other projects - we'll pull them out of the built products tree
                            CUtlString sOutputFile = V_UnqualifiedFileName( m_projects[iProject2].m_outputFile );
                            if ( !V_stricmp_fast( sFileName.Get(), sOutputFile.Get() ) )
                            {
                                bInclude = false;
                                break;
                            }
                        }
                    }
                    if ( bInclude )
                        Write( "%024llX /* %s in Frameworks (explicit) */,\n", makeoid( EOIDSpaceFiles, pProjectFile->m_nGeneratorId, EOIDTypeFileReference ), sFileName.String() );
                }
                
                CUtlString sOutputFile = V_UnqualifiedFileName( pProj->m_outputFile );
                if ( sOutputFile.Length() )
                    Write( "%024llX /* %s */,\n", makeoid( EOIDSpaceProjectOutputFile, pProj->m_nOid, EOIDTypeFileReference ), sOutputFile.String() );
                
            }
            --m_nIndent;
            
            Write( ");\n" );
            Write( "name = \"%s\";\n", pProjName );
            Write( "sourceTree = \"<group>\";\n" );
        }

        --m_nIndent;
        Write( "};\n" );
    }
                                
    // root group - the top of the displayed hierarchy
    Write( "%024llX = {\n", makeoid( EOIDSpaceFixed, EOIDFixedAllProjects, EOIDTypeGroup ) );
    ++m_nIndent;
    {
        Write( "isa = PBXGroup;\n" );
        Write( "children = (\n" );
					
        // sort the projects by name before we emit the list
        CUtlSortVector< CUtlString, CUtlStringLess > vecSortedProjectNames;
        FOR_EACH_VEC( m_projects, iProject )
        {
            vecSortedProjectNames.Insert( m_projects[iProject].GetName() );
        }

        ++m_nIndent;
        {
            FOR_EACH_VEC( vecSortedProjectNames, iProjectName ) 
            {
                // and each project's group (of groups)
                FOR_EACH_VEC( m_projects, iProject )
                {
                    if ( V_strcmp( m_projects[iProject].GetName(), vecSortedProjectNames[iProjectName].Get() ) )
                    {
                        // fprintf( stderr, "   skipping '%s' (%p) != '%s' (%p) (%d, %d)\n", m_projects[iProject].GetName(), m_projects[iProject].GetName(), vecSortedProjectNames[iProjectName].String(), vecSortedProjectNames[iProjectName].String(), iProject, iProjectName );
                        continue;
                    }
                    // fprintf( stderr, "emitting %s (%d, %d)\n", m_projects[iProject].GetName(), iGenerator, iProjectName );
                    
                    Write( "%024llX /* %s */,\n", makeoid( EOIDSpaceProjects, m_projects[iProject].m_nOid, EOIDTypeGroup ), m_projects[iProject].GetName() );
                    break;
                }
            }

            // add the build config (.xcconfig) files
            for( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchXCConfigFiles); iConfig++ )
            {
                Write( "%024llX /* %s */, \n", makeoid( EOIDSpaceConfigFiles, iConfig, EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
            }
        }
        --m_nIndent;
        Write( ");\n" );
        Write( "sourceTree = \"<group>\";\n" );
        // make the project follow our coding standards, and use tabs.
        Write( "usesTabs = 1;\n" );
    }
    --m_nIndent;
    Write( "};" );

    m_nIndent--;
    Write( "\n/* End PBXGroup section */\n" );
}

void CSolutionGenerator_Xcode::EmitSourcesBuildPhaseSection()
{
    /** 
     **
     ** the sources build phases - each target that compiles source references on of these, it in turn references the source files to be compiled
     **
     **/
    Write( "\n/* Begin PBXSourcesBuildPhase section */" );
    ++m_nIndent;
    
    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];

        Write( "\n" );
        Write( "%024llX /* Sources */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeSourcesBuildPhase ) );
        ++m_nIndent;
        {
            Write( "isa = PBXSourcesBuildPhase;\n" );
            Write( "buildActionMask = 2147483647;\n" );
            Write( "files = (\n" );
            ++m_nIndent;
            {
                auto pDictFiles = &pProj->m_projectFiles;
                FOR_EACH_PROJECT_FILE( *pDictFiles, iFile )
                {
                    CProjectFile *pProjectFile = (*pDictFiles)[iFile];
                    if ( IsFileCompletelyExcluded( pProjectFile ) )
                    {
                        continue;
                    }

                    const char *pFileName = pProjectFile->m_Name.String();
                    const char *pConfigName = k_rgchConfigNames[1]; // Retail
                    if ( pProj->AppearsInSourcesBuildPhase( pProjectFile, pConfigName ) )
                    {
                        Write( "%024llX /* %s in Sources */,\n", makeoid( EOIDSpaceFiles, pProjectFile->m_nGeneratorId, EOIDTypeBuildFile ), V_UnqualifiedFileName( UsePOSIXSlashes( pFileName ) ) );
                    }
                }
            }
            --m_nIndent;
            Write( ");\n");
            Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
        }
        --m_nIndent;
        Write( "};" );
    }
    
    --m_nIndent;
    Write( "\n/* End PBXSourcesBuildPhase section */\n" );
}

void CSolutionGenerator_Xcode::EmitFrameworksBuildPhaseSection()
{
    /** 
     **
     ** the frameworks build phases - each target that links libraries (static, dynamic, framework) has one of these, it references the linked thing
     **
     **/
    Write( "\n/* Begin PBXFrameworksBuildPhase section */" );
    ++m_nIndent;
    
    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];

        Write( "\n" );
        Write( "%024llX /* Frameworks */ = {\n", makeoid( EOIDSpaceProjects, m_projects[iProject].m_nOid, EOIDTypeFrameworksBuildPhase ) );
        ++m_nIndent;
        {
            Write( "isa = PBXFrameworksBuildPhase;\n" );
            Write( "buildActionMask = 2147483647;\n" );
            Write( "files = (\n" );
            ++m_nIndent;
            {
                // libraries we consume (specified in our files list)
                auto pDictFiles = &pProj->m_projectFiles;
                FOR_EACH_PROJECT_FILE( *pDictFiles, iFile )
                {
                    CProjectFile *pProjectFile = (*pDictFiles)[iFile];
                    const char *pFileName = pProjectFile->m_Name.String();
                    if ( pProjectFile->m_iFlags & (VPC_FILE_FLAGS_STATIC_LIB | VPC_FILE_FLAGS_IMPORT_LIB) )
                    {
                        CUtlString sFileName = V_UnqualifiedFileName( UsePOSIXSlashes( pFileName ) );
                        bool bInclude = true;
                        FOR_EACH_VEC( m_projects, iProject2 )
                        {
                            // don't include libs generated by other projects - we'll pull them out of the built products tree
                            CUtlString sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( m_projects[iProject2].m_outputFile ) );
                            if ( !V_stricmp_fast( sFileName.Get(), sOutputFile.Get() ) )
                            {
                                bInclude = false;
                                break;
                            }
                        }
                        if ( bInclude )
                            Write( "%024llX /* %s in Frameworks (explicit) */,\n", makeoid( EOIDSpaceFiles, pProjectFile->m_nGeneratorId, EOIDTypeBuildFile ), V_UnqualifiedFileName( UsePOSIXSlashes( pFileName ) ) );

                    }
                }

                // libraries from projects we depend on
                for ( int iTestProject = m_projects.Count() - 1; iTestProject >= 0; --iTestProject )
                {
                    if ( pProj->DependsOn( &m_projects[iTestProject] ) )
                    {
                        const char *pszOutputFile = m_projects[iTestProject].m_gameOutputFile;
                        EOIDSpace outputFileOidSpace = EOIDSpaceProjectGameOutputFile;
                        CUtlString sOutputFile = V_UnqualifiedFileName( pszOutputFile );
                        if ( !sOutputFile.Length() || IsStaticLibrary( sOutputFile.Get() ) )
                        { 
                            pszOutputFile = m_projects[iTestProject].m_outputFile;
                            outputFileOidSpace = EOIDSpaceProjectOutputFile;
                            sOutputFile = V_UnqualifiedFileName( pszOutputFile );
                        }
                        if ( sOutputFile.Length() && ( IsStaticLibrary( sOutputFile.Get() ) || IsDynamicLibrary( sOutputFile.Get() ) ) )
                        {
                            Write( "%024llX /* %s (dependency) */,\n", makeoid( outputFileOidSpace, m_projects[iTestProject].m_nOid, EOIDTypeBuildFile, iProject ), sOutputFile.String() );
                        }
                    }
                }

                // local frameworks we link against
                for ( int i = 0; i < pProj->m_localFrameworks.Count(); i++ )
                {
                    CUtlPathStringHolder rgchFrameworkName( V_UnqualifiedFileName( pProj->m_localFrameworks[i] ) );
                    rgchFrameworkName.StripExtension();
                            
                    Write( "%024llX /* %s in Frameworks (local framework) */,\n", pProj->GetLocalFrameworkOid( EOIDTypeBuildFile, i ), rgchFrameworkName.Get() ); 
                }
                       
                // system frameworks we link against
                for ( int i = 0; i < pProj->m_systemFrameworks.Count(); i++ )
                {
                    Write( "%024llX /* %s in Frameworks (system framework) */,\n", pProj->GetSystemFrameworkOid( EOIDTypeBuildFile, i ), pProj->m_systemFrameworks[i] );
                }
                        
                // system libraries we link against
                for ( int i = 0; i < pProj->m_systemLibraries.Count(); i++ )
                {
                    Write( "%024llX /* %s in Frameworks (system library) */,\n", pProj->GetSystemLibraryOid( EOIDTypeBuildFile, i ), pProj->m_systemLibraries[i] );
                }
            }
            --m_nIndent;
            Write( ");\n");
            Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
        }
        --m_nIndent;
        Write( "};" );
    }

    --m_nIndent;
    Write( "\n/* End PBXFrameworksBuildPhase section */\n" );
}

void CSolutionGenerator_Xcode::EmitShellScriptBuildPhaseSection()
{
    /** 
     **
     ** the shell script (pre/post build step) build phases - each target that generates a "gameoutputfile" has one of these, 
     ** to p4 edit the target and copy the build result there.  
     **
     **/
    Write( "\n/* Begin PBXShellScriptBuildPhase section */" );
    ++m_nIndent;

    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];
        const char *pProjectDir = pProj->m_projectDir;

        // Emit any project-wide (non-file) custom build step.
        for ( int iConfig = 0; iConfig < V_ARRAYSIZE( k_rgchConfigNames ); iConfig++ )
        {
            EmitCustomBuildStep( pProj, NULL, k_rgchConfigNames[iConfig] );
        }

        // wander the list of files, looking for custom build steps
        // if we find any, magic up a shell script to run them
        auto pDictFiles = &pProj->m_projectFiles;
        FOR_EACH_PROJECT_FILE( *pDictFiles, iFile )
        {
            CProjectFile *pProjectFile = (*pDictFiles)[iFile];
            for ( int iConfig = 0; iConfig < V_ARRAYSIZE( k_rgchConfigNames ); iConfig++ )
            {
                EmitCustomBuildStep( pProj, pProjectFile, k_rgchConfigNames[iConfig] );
            }
        }

        // emit a shell script to copy $GameOutputFiles into place
        // and run any post-build command
        CUtlString sDebugOutputFile = UsePOSIXSlashes( pProj->GetConfigStringProperty( "Debug", KEYWORD_GENERAL, g_pOption_GameOutputFile ) );
        CUtlString sReleaseOutputFile = UsePOSIXSlashes( pProj->GetConfigStringProperty( "Release", KEYWORD_GENERAL, g_pOption_GameOutputFile ) );
        if ( !sDebugOutputFile.Length() || !sReleaseOutputFile.Length() )
        {
            sDebugOutputFile = UsePOSIXSlashes( pProj->GetConfigStringProperty( "Debug", KEYWORD_LINKER, g_pOption_OutputFile ) );
            sReleaseOutputFile = UsePOSIXSlashes( pProj->GetConfigStringProperty( "Release", KEYWORD_LINKER, g_pOption_OutputFile ) );

            if ( !sDebugOutputFile.Length() || !sReleaseOutputFile.Length() )
            {
                sDebugOutputFile = UsePOSIXSlashes( pProj->GetConfigStringProperty( "Debug", KEYWORD_LIBRARIAN, g_pOption_OutputFile ) );
                sReleaseOutputFile = UsePOSIXSlashes( pProj->GetConfigStringProperty( "Release", KEYWORD_LIBRARIAN, g_pOption_OutputFile ) );
            }
        }
                    
        if ( sDebugOutputFile.Length() || sReleaseOutputFile.Length() )
        {                        
            CUtlPathStringHolder rgchDebugFilePath( pProjectDir, "/", sDebugOutputFile.String() );
            rgchDebugFilePath.FixSlashesAndDotSlashes( '/' );
            
            CUtlPathStringHolder rgchReleaseFilePath( pProjectDir, "/", sReleaseOutputFile.String() );
            rgchReleaseFilePath.FixSlashesAndDotSlashes( '/' );

            // We only handle one import library.
            // The debug and release import library targets should usually match
            // so pick the release version and only fall back to debug if
            // a release version isn't found.
            const char *pImportLibraryRaw = pProj->GetConfigStringProperty( "Release", KEYWORD_LINKER, g_pOption_ImportLibrary );
            if ( *pImportLibraryRaw == 0 )
            {
                pImportLibraryRaw = pProj->GetConfigStringProperty( "Debug", KEYWORD_LINKER, g_pOption_ImportLibrary );
            }
            CUtlString importLibrary;
            if ( *pImportLibraryRaw )
            {
                CUtlPathStringHolder absImportLibrary( pProjectDir, "/", pImportLibraryRaw );
                absImportLibrary.FixSlashesAndDotSlashes( '/' );
                CUtlStringBuilder *pVsStr = g_pVPC->GetTempStringBuffer1();
                CBaseProjectDataCollector::DoStandardVisualStudioReplacements( absImportLibrary.Get(), pVsStr, absImportLibrary.Get() );
                importLibrary = pVsStr->Get();
            }

            Write( "\n" );
            Write( "%024llX /* ShellScript */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypePostBuildPhase, 0 ) );
            ++m_nIndent;
            {
                Write( "isa = PBXShellScriptBuildPhase;\n" );
                Write( "buildActionMask = 2147483647;\n" );
                Write( "files = (\n" );
                Write( ");\n" );
                Write( "inputPaths = (\n" );
                ++m_nIndent;
                {
                    Write( "\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}\",\n" );
                }
                --m_nIndent;
                Write( ");\n" );
                Write( "name = \"Post-Build Step\";\n" );
                Write( "outputPaths = (\n" );
                ++m_nIndent;
                {
                    CUtlStringBuilder *pPathStr = g_pVPC->GetTempStringBuffer2();
                    pPathStr->Set( rgchDebugFilePath );
                    pPathStr->ReplaceFastCaseless( "/lib/osx32/debug/", "/lib/osx32/${CONFIGURATION}/" );
                    pPathStr->ReplaceFastCaseless( "/lib/osx64/debug/", "/lib/osx64/${CONFIGURATION}/" );
                    CUtlStringBuilder *pVsStr = g_pVPC->GetTempStringBuffer1();
                    CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pPathStr->Get(), pVsStr, pPathStr->Get() );
                    rgchDebugFilePath.Set( pVsStr->String() );
                    
                    pPathStr->Set( rgchReleaseFilePath );
                    pPathStr->ReplaceFastCaseless( "/lib/osx32/release/", "/lib/osx32/${CONFIGURATION}/" );
                    pPathStr->ReplaceFastCaseless( "/lib/osx64/release/", "/lib/osx64/${CONFIGURATION}/" );
                    CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pPathStr->Get(), pVsStr, pPathStr->Get() );
                    rgchReleaseFilePath.Set( pVsStr->String() );

                    Write( "\"%s\",\n", rgchDebugFilePath.Get() );
                    if ( V_strcmp( rgchDebugFilePath, rgchReleaseFilePath ) )
                        Write( "\"%s\",\n", rgchReleaseFilePath.Get() );
                }
                --m_nIndent;
                Write( ");\n");
                Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
                Write( "shellPath = /bin/bash;\n" );

                bool bIsExe = false;
                bool bIsDll = false;
                const char *pBinaryType = "binary";
                const char *pConfigType = pProj->GetStringProperty( KEYWORD_GENERAL, g_pOption_ConfigurationType );
                if ( V_stristr_fast( pConfigType, ".exe" ) )
                {
                    pBinaryType = "xbinary";
                    bIsExe = true;
                }
                else if ( V_stristr_fast( pConfigType, ".dll" ) )
                {
                    bIsDll = true;
                }

                CUtlStringBuilder *pVsStr2 = g_pVPC->GetTempStringBuffer2();
                CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pProj->GetStringProperty( KEYWORD_POSTBUILDEVENT, g_pOption_CommandLine ), pVsStr2, NULL );
                pVsStr2->ReplaceFastCaseless( "$(OBJ_DIR)", "${OBJECT_FILE_DIR_normal}" );
                CUtlString sCommandLine = UsePOSIXSlashes( pVsStr2->Get() );

                // If there's no custom post-build step we suppress the environment variable
                // dump.  This is a common case and avoids a bunch of spew for steps that
                // aren't that interesting.
                if ( sCommandLine.IsEmpty() )
                {
                    Write( "showEnvVarsInLog = 0;\n" );
                }
                
                CUtlStringBuilder *pScript = g_pVPC->GetTempStringBuffer2();
                CUtlStringBuilder *pVsStrCmd = g_pVPC->GetTempStringBuffer1();

                pScript->Set( "shellScript = \"cd " );
                pScript->Append( pProjectDir );
                pScript->Append( ";\\n" );

                // Make sure we have a configuration.
                pScript->Append(
                    "if [ -z \\\"$CONFIGURATION\\\" -a -n \\\"$BUILD_STYLE\\\" ]; then\\n"
                    "  CONFIGURATION=${BUILD_STYLE} \\n"
                    "fi\\n"
                    "if [ -z \\\"$CONFIGURATION\\\"  ]; then\\n"
                    "  echo \\\"Could not determine build configuration.\\\";\\n"
                    "  exit 1; \\n"
                    "fi\\n" );

                // Skip work if we didn't update the output file.
                pScript->Append( "OUTPUTFILE=\\\"" );
                pScript->Append( rgchReleaseFilePath.Get() );
                pScript->Append( "\\\"\\n" );
                pScript->Append(
                    "if [ \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}\\\" -ot \\\"$OUTPUTFILE\\\" ]; then\\n"
                    "  echo \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME} is older than $OUTPUTFILE, skipping publish...\\\";\\n"
                    "  exit 0\\n"
                    "fi\\n" );

                // Make output file writable.
                pScript->Append( "if [ -f \\\"$OUTPUTFILE\\\" -o -d \\\"$OUTPUTFILE.dSYM\\\" ]; then\\n" );
                pScript->Append( UsePOSIXSlashes( g_pVPC->GetSourcePath() ) );
                pScript->AppendFormat( "/vpc_scripts/valve_p4_edit.sh \\\"Xcode Auto Checkout\\\" -cname -ftype %s \\\"$OUTPUTFILE\\\"\\n", pBinaryType );

                // Make symbol directory writable.
                // We only check in .dSYM dirs for some builds.
                pScript->Append( "  if [ -d \\\"$OUTPUTFILE.dSYM\\\" ]; then\\n" );
                if ( g_pVPC->IsConditionalDefined( "SYMBOLS_IN_P4" ) )
                {
                    pScript->Append( UsePOSIXSlashes( g_pVPC->GetSourcePath() ) );
                    pScript->Append( "/vpc_scripts/valve_p4_edit.sh \\\"Xcode Auto Checkout\\\" -cname -ftype text \\\"$OUTPUTFILE.dSYM/Contents/Info.plist\\\"\\n" );
                    pScript->Append( UsePOSIXSlashes( g_pVPC->GetSourcePath() ) );
                    pScript->Append( "/vpc_scripts/valve_p4_edit.sh \\\"Xcode Auto Checkout\\\" -cname -ftype binary \\\"$OUTPUTFILE.dSYM/Contents/Resources/DWARF/" );
                    pScript->Append( V_UnqualifiedFileName( rgchReleaseFilePath.Get() ) );
                    pScript->Append( "\\\"\\n" );
                }
                else
                {
                    pScript->Append( "    chmod -R -f +w \\\"$OUTPUTFILE.dSYM\\\"\\n" );
                }
                pScript->Append( "  fi\\n" );
                pScript->Append( "fi\\n" );

                if ( !importLibrary.IsEmpty() )
                {
                    pScript->Append( "IMPORTLIBRARY=\\\"" );
                    pScript->Append( importLibrary.Get() );
                    pScript->Append( "\\\"\\n" );

                    pScript->Append( "if [ -f \\\"$IMPORTLIBRARY\\\" ]; then\\n" );
                    pScript->Append( UsePOSIXSlashes( g_pVPC->GetSourcePath() ) );
                    pScript->Append( "/vpc_scripts/valve_p4_edit.sh \\\"Xcode Auto Checkout\\\" -cname -ftype binary \\\"$IMPORTLIBRARY\\\"\\n" );
                    pScript->Append( "fi\\n" );
                }
                
                // Copy outputs into final locations.
                pScript->Append(
                    "if [ -d \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}.dSYM\\\" ]; then\\n"
                    "  echo \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}.dSYM -> ${OUTPUTFILE}.dSYM\\\"\\n"
                    "  rm -rf \\\"${OUTPUTFILE}.dSYM\\\"\\n"
                    "  cp -R \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}.dSYM\\\" \\\"${OUTPUTFILE}.dSYM\\\"\\n"
                    "fi\\n"
                    "cp -v \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}\\\" \\\"$OUTPUTFILE\\\"\\n"
                    );

                if ( bIsExe || bIsDll )
                {
                    pScript->Append( "if [ \\\"Release\\\" == \\\"${CONFIGURATION}\\\" ]; then\\necho Stripping symbols...\\nstrip -x -S \\\"$OUTPUTFILE\\\"\\nfi;\\n" );
                }

                if ( !importLibrary.IsEmpty() )
                {
                    pScript->Append( "cp -v \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}\\\" \\\"$IMPORTLIBRARY\\\"\\n" );
                }

                // Add post build command if necessary.
                if ( !sCommandLine.IsEmpty() )
                {
                    // We recheck the command in the script to see if it's all blanks.
                    pScript->Append( "POST_BUILD_COMMAND=\\\"" );
                    pScript->Append( EscapeQuotes( sCommandLine.Get(), pVsStrCmd ) );
                    pScript->Append( "\\\"\\n" );
                    pScript->Append( "if [ \\\"${POST_BUILD_COMMAND/ /}\\\" != \\\"\\\" ]; then\\n" );
                    pScript->Append( sCommandLine.Get() );
                    pScript->Append( "\\n" );
                    pScript->Append(
                        "  if [ $? != 0 ]; then\\n"
                        "    exit $?\\n"
                        "  fi\\n"
                        "fi" );
                }
                
                pScript->Append( "\";\n" );
                Write( pScript->Get() );
            }
            --m_nIndent;
            Write( "};" );
        }            
    }

    --m_nIndent;
    Write( "\n/* End PBXShellScriptBuildPhase section */\n" );
}

void CSolutionGenerator_Xcode::EmitNativeTargetSection()
{
    /** 
     **
     ** nativetargets section - build targets, which ultimately reference build phases
     **
     **/
    Write( "\n/* Begin PBXNativeTarget section */" );
    ++m_nIndent;
    
    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];
        const char *pProjName = pProj->GetName();

        if ( !pProj->m_pPrimaryOutputFile )
            continue;
        
        Write( "\n" );
        Write( "%024llX /* %s */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeNativeTarget ), pProjName );
        ++m_nIndent;
        {
            Write( "isa = PBXNativeTarget;\n" );
                    
            Write( "buildConfigurationList = %024llX /* Build configuration list for PBXNativeTarget \"%s\" */;\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeConfigurationList ), pProjName );
            Write( "buildPhases = (\n" );
            ++m_nIndent;
            {
                for ( int i = 0; i < pProj->m_nShellScriptPhases; i++ )
                    Write( "%024llX /* ShellScript */,\n",  makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeShellScriptBuildPhase, i ) );
                Write( "%024llX /* Sources */,\n",  makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeSourcesBuildPhase ) );
                Write( "%024llX /* Frameworks */,\n",  makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeFrameworksBuildPhase ) );                        
                Write( "%024llX /* ShellScript */,\n",  makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypePostBuildPhase, 0 ) );
            }
            --m_nIndent;
            Write( ");\n" );
            Write( "buildRules = (\n" );
            ++m_nIndent;
            {
                for ( int i = 0; i < pProj->m_nCustomBuildRules; i++ )
                    Write( "%024llX /* PBXBuildRule */,\n",  makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeCustomBuildRule, i ) );
            }
            --m_nIndent;
            Write( ");\n" );
            Write( "dependencies = (\n" );
            ++m_nIndent;
            {
                // these dependencies point to the dependency objects, which reference other projects through the container item proxy objects
                for ( int iTestProject = 0; iTestProject < m_projects.Count(); iTestProject++ )
                {
                    if ( pProj->DependsOn( &m_projects[iTestProject] ) )
                    {
                        Write( "%024llX /* %s */,\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeTargetDependency, (uint16_t)iTestProject ), UsePOSIXSlashes( m_projects[iTestProject].GetName() ) );
                    }
                }
            }
            --m_nIndent;
            Write( ");\n" );
            Write( "productName = \"%s\";\n", pProjName );
            Write( "name = \"%s\";\n", pProjName );
                    
            Write( "productReference = %024llX /* %s */;\n", makeoid( pProj->m_primaryOutputFileOidSpace, pProj->m_nOid, EOIDTypeFileReference ), V_UnqualifiedFileName( pProj->m_pPrimaryOutputFile ) );
                    
            char rgchProductType[MAX_FILE_TYPE];
            XcodeProductTypeFromFileName( V_UnqualifiedFileName( pProj->m_pPrimaryOutputFile ), rgchProductType, sizeof( rgchProductType ) );
            Write( "productType = \"%s\";\n", rgchProductType );                
        }
        --m_nIndent;
        Write( "};" );
    }

    --m_nIndent;
    Write( "\n/* End PBXNativeTarget section */\n" );
}

void CSolutionGenerator_Xcode::EmitLegacyTargetSection()
{
    /** 
     **
     ** legacytargets section - clean script pseudo-targets
     **
     **/
    Write( "\n/* Begin PBXLegacyTarget section */" );
    ++m_nIndent;
    
    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];
        const char *pProjName = pProj->GetName();

        if ( !pProj->m_bCustomBuildNeedsLegacyTargetClean )
            continue;
        
        Write( "\n" );
        Write( "%024llX /* cleaning %s */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeLegacyTarget ), pProjName );
        ++m_nIndent;
        {
            Write( "isa = PBXLegacyTarget;\n" );
                    
            Write( "buildConfigurationList = %024llX /* Build configuration list for PBXLegacyTarget \"%s\" */;\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeConfigurationList ), pProjName );
            Write( "buildPhases = (\n" );
            Write( ");\n" );
            Write( "dependencies = (\n" );
            Write( ");\n" );
            Write( "buildToolPath = /bin/bash;\n" );
            Write( "buildArgumentsString = \"-c \\\"%s\\\"",
				   "if [ \\\\\\\"$ACTION\\\\\\\" != \\\\\\\"clean\\\\\\\" ]; then "
				   "    echo For cleaning only, ignored ; "
				   "    exit 0 ; "
				   "fi ; "
				   "echo Removing $* ; "
				   "rm -f $* ; " );
            int nIndent = m_nIndent;
            m_nIndent = 0;
			// We currently just clean all configurations on any clean.  Xcode
			// does something similar so there's no point in making extra effort here.
            for ( int iConfig = 0; iConfig < V_ARRAYSIZE( k_rgchConfigNames ); iConfig++ )
            {
                for ( int i = 0; i < pProj->m_customBuildOutputFiles[iConfig].Count(); i++ )
                {
                    Write( " %s", pProj->m_customBuildOutputFiles[iConfig][i].Get() );
                }
            }
            Write( "\";\n" );
            m_nIndent = nIndent;
            // Do not have settings passed in the environment as we don't need them,
            // plus then the environment is spewed in the build log.
            Write( "passBuildSettingsInEnvironment = 0;\n" );
            Write( "productName = \"Clean Custom Build Outputs for %s\";\n", pProjName );
            Write( "name = \"Clean Custom Build Outputs for %s\";\n", pProjName );
        }
        --m_nIndent;
        Write( "};" );
    }

    --m_nIndent;
    Write( "\n/* End PBXLegacyTarget section */\n" );
}

void CSolutionGenerator_Xcode::EmitAggregateTargetSection()
{
    /** 
     **
     ** aggregate targets - for targets that have no output files (i.e. are scripts)
     ** and the "all" target
     **
     **/
    Write( "\n/* Begin PBXAggregateTarget section */\n" );
    ++m_nIndent;

    Write( "%024llX /* All */ = {\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeAggregateTarget ) );
    ++m_nIndent;
    {
        Write( "isa = PBXAggregateTarget;\n" );
        Write( "buildConfigurationList = %024llX /* Build configuration list for PBXAggregateTarget \"All\" */;\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeConfigurationList, 1 ) );
        Write( "buildPhases = (\n" );
        Write( ");\n" );
        Write( "dependencies = (\n" );
        ++m_nIndent;
        {
            FOR_EACH_VEC( m_projects, iProject )
            {
				CProject_Xcode *pProj = &m_projects[iProject];
				
				// note the sneaky -1 ordinal here, is we can later generate a dependency block for the target thats not tied to any other targets dependency.
                Write( "%024llX /* PBXTargetDependency for %s */,\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeTargetDependency, -1 ), pProj->GetName() );
				
				if ( pProj->m_bCustomBuildNeedsLegacyTargetClean )
				{
					Write( "%024llX /* cleaning %s */,\n", makeoid( EOIDSpaceProjectWithCustomOutputs, pProj->m_nOid, EOIDTypeTargetDependency ), pProj->GetName() );
				}
            }
        }
        --m_nIndent;
        Write( ");\n" );
        Write( "name = All;\n" );
        Write( "productName = All;\n" );
    }
    --m_nIndent;
    Write( "};\n" );
                
    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];
		const char *pProjName = pProj->GetName();

        if ( pProj->m_pPrimaryOutputFile )
            continue;
        
        // NOTE: the use of EOIDTypeNativeTarget here is intentional - a project will never appear as both, and this makes things link up without 
        // having to special case in dependencies and aggregate targets
        Write( "%024llX /* %s */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeNativeTarget ), pProjName );
        ++m_nIndent;
        {
            Write( "isa = PBXAggregateTarget;\n" );
                        
            Write( "buildConfigurationList = %024llX /* Build configuration list for PBXAggregateTarget \"%s\" */;\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeConfigurationList ), pProjName );
            Write( "buildPhases = (\n" );
            ++m_nIndent;
            {
                for ( int i = 0; i < pProj->m_nShellScriptPhases; i++ )
                    Write( "%024llX /* ShellScript */,\n",  makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeShellScriptBuildPhase, i ) );
            }
            --m_nIndent;
            Write( ");\n" );
            Write( "buildRules = (\n" );
            ++m_nIndent;
            {
                // Nothing yet.
            }
            --m_nIndent;
            Write( ");\n" );
            Write( "dependencies = (\n" );
            ++m_nIndent;
            {
                // these dependencies point to the dependency objects, which reference other projects through the container item proxy objects
                for ( int iTestProject = 0; iTestProject < m_projects.Count(); iTestProject++ )
                {
                    if ( pProj->DependsOn( &m_projects[iTestProject] ) )
                    {
                        Write( "%024llX /* %s */,\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeTargetDependency, (uint16_t)iTestProject ), UsePOSIXSlashes( m_projects[iTestProject].GetName() ) );
                    }
                }
            }
            --m_nIndent;
            Write( ");\n" );
            Write( "name = \"%s\";\n", pProjName );
            Write( "productName = \"%s\";\n", pProjName );
        }
        --m_nIndent;
        Write( "};\n" );
    }

    --m_nIndent;
    Write( "\n/* End PBXAggregateTarget section */\n" );
}

void CSolutionGenerator_Xcode::EmitProjectSection()
{
    /** 
     **
     ** project section - the top-level object that ties all the bits (targets, groups, ...) together
     **
     **/
    Write( "\n/* Begin PBXProject section */\n" );
    ++m_nIndent;

    Write( "%024llX /* project object */ = {\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeProject ) );
    ++m_nIndent;
    {
        Write( "isa = PBXProject;\n" );
        Write( "attributes = {\n" );
        ++m_nIndent;
        {
            Write( "BuildIndependentTargetsInParallel = YES;\n" );
        }
        --m_nIndent;
        Write( "};\n" );
        Write( "buildConfigurationList = %024llX /* Build configuration list for PBXProject \"%s\" */;\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeConfigurationList ), V_UnqualifiedFileName( m_solutionFilename ) ); 
        Write( "compatibilityVersion = \"Xcode 3.0\";\n" );
        Write( "hasScannedForEncodings = 0;\n" );
        Write( "mainGroup = %024llX;\n", makeoid( EOIDSpaceFixed, EOIDFixedAllProjects, EOIDTypeGroup ) );
        Write( "productRefGroup = %024llX /* Products */;\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeGroup ) );
        Write( "projectDirPath = \"\";\n" );
        Write( "projectRoot = \"\";\n" );
        Write( "targets = (\n" );
        ++m_nIndent;
        {
            Write( "%024llX /* All */,\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeAggregateTarget ) );
            // sort the projects by name before we emit the list
            CUtlSortVector< CUtlString, CUtlStringLess > vecSortedProjectNames;
            FOR_EACH_VEC( m_projects, iProject )
            {
                vecSortedProjectNames.Insert( m_projects[iProject].GetName() );
            }
            FOR_EACH_VEC( vecSortedProjectNames, iProjectName ) 
            {
                FOR_EACH_VEC( m_projects, iProject )
                {
        			CProject_Xcode *pProj = &m_projects[iProject];

                    if ( V_strcmp( pProj->GetName(), vecSortedProjectNames[iProjectName].Get() ) )
                    {
                        continue;
                    }
                    Write( "%024llX /* %s */,\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeNativeTarget ), pProj->GetName() );
				
					if ( pProj->m_bCustomBuildNeedsLegacyTargetClean )
					{
						Write( "%024llX /* cleaning %s */,\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeLegacyTarget ), pProj->GetName() );
					}
                }
            }
        }
        --m_nIndent;
        Write( ");\n" );
    }      

    --m_nIndent;
    Write( "};" );
    Write( "\n/* End PBXProject section */\n" );
}

void CSolutionGenerator_Xcode::EmitContainerItemProxySection()
{
    /** 
     **
     ** container item proxies (no clue, I just work here...) - they sit between projects when expressing dependencies
     **
     **/
    Write( "\n/* Begin PBXContainerItemProxy section */\n" );

    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];

        // for the aggregate target
        Write( "%024llX /* PBXContainerItemProxy for %s */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeContainerItemProxy, -1 ), pProj->GetName() );
        ++m_nIndent;
        {
            Write( "isa = PBXContainerItemProxy;\n" );
            // it looks like if you cross ref between xcodeprojs, this is the oid for the other xcode proj
            Write( "containerPortal = %024llX; /* Project object */\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeProject ) );
            Write( "proxyType = 1;\n" ); 
            Write( "remoteGlobalIDString = %024llX; /* PBXNativeTarget */\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeNativeTarget ) );
            Write( "remoteInfo = \"%s\";\n", pProj->GetName() );
        }
        --m_nIndent;
        Write( "};\n" );
		
		// clean target
		if ( pProj->m_bCustomBuildNeedsLegacyTargetClean )
		{
			Write( "%024llX /* PBXContainerItemProxy for cleaning %s */ = {\n", makeoid( EOIDSpaceProjectWithCustomOutputs, pProj->m_nOid, EOIDTypeContainerItemProxy ), pProj->GetName() );
			++m_nIndent;
			{
				Write( "isa = PBXContainerItemProxy;\n" );
				// it looks like if you cross ref between xcodeprojs, this is the oid for the other xcode proj
				Write( "containerPortal = %024llX; /* Project object */\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeProject ) );
				Write( "proxyType = 1;\n" );
				Write( "remoteGlobalIDString = %024llX; /* PBXLegacyTarget */\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeLegacyTarget ) );
				Write( "remoteInfo = \"clean for %s\";\n", pProj->GetName() );
			}
			--m_nIndent;
			Write( "};\n" );
		}

        // for each project, figure out what projects it depends on, and spit out a containeritemproxy for that dependency
        // of particular note is that there are many item proxies for a given project, so we make their oids with the ordinal 
        // of the project they depend on - this must be consistent within the generated solution
        for ( int iTestProject = 0; iTestProject < m_projects.Count(); iTestProject++ )
        {
	        CProject_Xcode *pTestProj = &m_projects[iTestProject];

            if ( pProj->DependsOn( pTestProj ) )
            {
                Write( "%024llX /* PBXContainerItemProxy for %s in %s */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeContainerItemProxy, (uint16_t)iTestProject ), pTestProj->GetName(), pProj->GetName() );
                ++m_nIndent;
                {
                    Write( "isa = PBXContainerItemProxy;\n" );
                    // it looks like if you cross ref between xcodeprojs, this is the oid for the other xcode proj
                    Write( "containerPortal = %024llX; /* Project object */\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeProject ) );
                    Write( "proxyType = 1;\n" );
                    Write( "remoteGlobalIDString = %024llX; /* PBXNativeTarget */\n", makeoid( EOIDSpaceProjects, pTestProj->m_nOid, EOIDTypeNativeTarget ) );
                    Write( "remoteInfo = \"%s\";\n", pTestProj->GetName() );
                }
                --m_nIndent;
                Write( "};\n" );          
            }
        }
    }                    

    Write( "\n/* End PBXContainerItemProxy section */\n" );
}

void CSolutionGenerator_Xcode::EmitTargetDependencySection()
{
    /** 
     **
     ** target dependencies - referenced by each project, in turn references the proxy container objects to express dependencies between targets
     **
     **/
    Write( "\n/* Begin PBXTargetDependency section */\n" );
    
    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];

        Write( "%024llX /* PBXTargetDependency for %s */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeTargetDependency, -1 ), pProj->GetName() );
        ++m_nIndent;
        {
            Write( "isa = PBXTargetDependency;\n" );
            Write( "target = %024llX /* %s */; /* PBXNativeTarget */\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeNativeTarget ), pProj->GetName() );
            Write( "targetProxy = %024llX /* PBXContainerItemProxy */;\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeContainerItemProxy, -1 ) );
		}
        --m_nIndent;
        Write( "};\n" );
		
		// clean target
		if ( pProj->m_bCustomBuildNeedsLegacyTargetClean )
		{
			Write( "%024llX /* PBXTargetDependency for cleaning %s */ = {\n", makeoid( EOIDSpaceProjectWithCustomOutputs, pProj->m_nOid, EOIDTypeTargetDependency ), pProj->GetName() );
			++m_nIndent;
			{
				Write( "isa = PBXTargetDependency;\n" );
				Write( "target = %024llX /* %s */; /* PBXLegacyTarget */\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeLegacyTarget ), pProj->GetName() );
            	Write( "targetProxy = %024llX /* PBXContainerItemProxy */;\n", makeoid( EOIDSpaceProjectWithCustomOutputs, pProj->m_nOid, EOIDTypeContainerItemProxy ) );
			}
			--m_nIndent;
			Write( "};\n" );
		}

        for ( int iTestProject = 0; iTestProject < m_projects.Count(); iTestProject++ )
        {
	        CProject_Xcode *pTestProj = &m_projects[iTestProject];

            if ( pProj->DependsOn( pTestProj ) )
            {
                Write( "%024llX /* PBXTargetDependency for %s in %s */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeTargetDependency, (uint16_t)iTestProject ), pTestProj->GetName(), pProj->GetName() );
                ++m_nIndent;
                {
                    Write( "isa = PBXTargetDependency;\n" );
                    Write( "target = %024llX /* %s */; /* PBXNativeTarget */\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeNativeTarget ), pProj->GetName() );
                    Write( "targetProxy = %024llX /* PBXContainerItemProxy */;\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeContainerItemProxy, (uint16_t)iTestProject ) );
                }
                --m_nIndent;
                Write( "};\n" );
            }
        }
    }

    --m_nIndent;
    Write( "\n/* End PBXTargetDependency section */\n" );
}
            
void CSolutionGenerator_Xcode::EmitBuildConfigurationSection()
{
    /** 
     **
     ** build configurations - each target (and the project) has a set of build configurations (one release, one debug), each with their own set of build settings 
     ** the "baseConfigurationReference" points back to the appropriate .xcconfig file that gets referenced by the project and has all the non-target specific settings
     **
     **/
    Write( "\n/* Begin XCBuildConfiguration section */" );	
    ++m_nIndent;

    // project and aggregate "all" target
    for( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
    {
        if ( !g_pVPC->IsConfigAllowedBySourceFileConfigFilter( k_rgchConfigNames[iConfig] ) )
        {
            continue;
        }

        bool bIsDebug = !V_stristr_fast( k_rgchConfigNames[iConfig], "release" );
                    
        Write( "\n" );
        Write( "%024llX /* %s */ = {\n", makeoid( EOIDSpaceRootConfigs, iConfig, EOIDTypeBuildConfiguration ), k_rgchConfigNames[iConfig] );
        ++m_nIndent;
        {
            Write( "isa = XCBuildConfiguration;\n" );
            Write( "baseConfigurationReference = %024llX /* %s */;\n", makeoid( EOIDSpaceConfigFiles, iConfig, EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
            Write( "buildSettings = {\n" );
            ++m_nIndent;
            {
                EmitBuildSettings( "All", NULL, NULL, bIsDebug );            
            }
            --m_nIndent;
            Write( "};\n" );
            Write( "name = \"%s\";\n", k_rgchConfigNames[iConfig] );            
        }
        --m_nIndent;
        Write( "};" );

        Write( "\n" );
        Write( "%024llX /* %s */ = {\n", makeoid( EOIDSpaceRootConfigs, iConfig, EOIDTypeBuildConfiguration, 1 ), k_rgchConfigNames[iConfig] );
        ++m_nIndent;
        {
            Write( "isa = XCBuildConfiguration;\n" );
            Write( "baseConfigurationReference = %024llX /* %s */;\n", makeoid( EOIDSpaceConfigFiles, iConfig, EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
            Write( "buildSettings = {\n" );
            ++m_nIndent;
            {
                EmitBuildSettings( "All", NULL, NULL, bIsDebug );            
            }
            --m_nIndent;
            Write( "};\n" );
            Write( "name = \"%s\";\n", k_rgchConfigNames[iConfig] );            
        }
        --m_nIndent;
        Write( "};" );
    }
                
    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];

        for( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
        {
            if ( !g_pVPC->IsConfigAllowedBySourceFileConfigFilter( k_rgchConfigNames[iConfig] ) )
            {
                continue;
            }

            bool bIsDebug = !V_stristr_fast( k_rgchConfigNames[iConfig], "release" );
                        
            Write( "\n" );
            Write( "%024llX /* %s for %s */ = {\n", pProj->GetProjectConfigOid( EOIDTypeBuildConfiguration, iConfig ), k_rgchConfigNames[iConfig], pProj->GetName() );
            ++m_nIndent;
            {
                Write( "isa = XCBuildConfiguration;\n" );
                Write( "baseConfigurationReference = %024llX /* %s */;\n", makeoid( EOIDSpaceConfigFiles, iConfig, EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
                Write( "buildSettings = {\n" );
                ++m_nIndent;
                {
                    EmitBuildSettings( pProj->GetName(), pProj, k_rgchConfigNames[iConfig], bIsDebug );
                }
                --m_nIndent;
                Write( "};\n" );
                Write( "name = \"%s\";\n", k_rgchConfigNames[iConfig] );        
            }
            --m_nIndent;
            Write( "};" );
        }
    }        

    --m_nIndent;
    Write( "\n/* End XCBuildConfiguration section */\n" );
}

void CSolutionGenerator_Xcode::EmitConfigurationListSection()
{
    /** 
     **
     ** configuration lists - aggregates the build configurations above into sets, which are referenced by the individual targets.
     **
     **/
    Write( "\n/* Begin XCConfigurationList section */\n" );
    ++m_nIndent;

    int nDefConfig = 0;
    for( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
    {
        if ( !g_pVPC->IsConfigAllowedBySourceFileConfigFilter( k_rgchConfigNames[iConfig] ) )
        {
            continue;
        }

        nDefConfig = iConfig;
        break;
    }

    Write( "%024llX /* Build configuration list for PBXProject \"%s\" */ = {\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeConfigurationList ), V_UnqualifiedFileName( m_solutionFilename ) );
    ++m_nIndent;
    {
        Write( "isa = XCConfigurationList;\n" );
        Write( "buildConfigurations = (\n" );
        ++m_nIndent;
        for( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
        {
            if ( !g_pVPC->IsConfigAllowedBySourceFileConfigFilter( k_rgchConfigNames[iConfig] ) )
            {
                continue;
            }

            Write( "%024llX /* %s */,\n", makeoid( EOIDSpaceRootConfigs, iConfig, EOIDTypeBuildConfiguration ), k_rgchConfigNames[iConfig] );
        }
        --m_nIndent;
        Write( ");\n" );
        Write( "defaultConfigurationIsVisible = 0;\n" );
        Write( "defaultConfigurationName = \"%s\";\n", k_rgchConfigNames[nDefConfig] );
    }
    --m_nIndent;
    Write( "};\n" );

    Write( "%024llX /* Build configuration list for PBXAggregateTarget \"All\" */ = {\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeConfigurationList, 1 ) );
    ++m_nIndent;
    {
        Write( "isa = XCConfigurationList;\n" );
        Write( "buildConfigurations = (\n" );
        ++m_nIndent;
        for( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
        {
            if ( !g_pVPC->IsConfigAllowedBySourceFileConfigFilter( k_rgchConfigNames[iConfig] ) )
            {
                continue;
            }

            Write( "%024llX /* %s */,\n", makeoid( EOIDSpaceRootConfigs, iConfig, EOIDTypeBuildConfiguration, 1 ), k_rgchConfigNames[iConfig] );
        }
        --m_nIndent;
        Write( ");\n" );
        Write( "defaultConfigurationIsVisible = 0;\n" );
        Write( "defaultConfigurationName = \"%s\";\n", k_rgchConfigNames[nDefConfig] );
    }
    --m_nIndent;
    Write( "};" );

    FOR_EACH_VEC( m_projects, iProject )
    {
        CProject_Xcode *pProj = &m_projects[iProject];

        Write( "\n" );
        Write( "%024llX /* Build configuration list for PBXNativeTarget \"%s\" */ = {\n", makeoid( EOIDSpaceProjects, pProj->m_nOid, EOIDTypeConfigurationList ), pProj->GetName() );
        ++m_nIndent;
        {
            Write( "isa = XCConfigurationList;\n" );
            Write( "buildConfigurations = (\n" );
            ++m_nIndent;
            for( int iConfig = 0; iConfig < V_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
            {
                if ( !g_pVPC->IsConfigAllowedBySourceFileConfigFilter( k_rgchConfigNames[iConfig] ) )
                {
                    continue;
                }

                Write( "%024llX /* %s */,\n", pProj->GetProjectConfigOid( EOIDTypeBuildConfiguration, iConfig ), k_rgchConfigNames[iConfig] );
            }
            --m_nIndent;
            Write( ");\n" );
            Write( "defaultConfigurationIsVisible = 0;\n" );
            Write( "defaultConfigurationName = \"%s\";\n", k_rgchConfigNames[nDefConfig] );
        }
        --m_nIndent;
        Write( "};" );
    }            

    --m_nIndent;
    Write( "\n/* End XCConfigurationList section */\n" );
}

void CSolutionGenerator_Xcode::GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projectsUnordered )
{
    if ( projectsUnordered.Count() != m_projects.Count() )
    {
        g_pVPC->VPCError( "Solution project mismatch (%u vs. %u), unable to write solution",
                          projectsUnordered.Count(), m_projects.Count() );
        return;
    }

    FOR_EACH_VEC( m_projects, iProject )
    {
        int iMatch = -1;
        FOR_EACH_VEC( projectsUnordered, iProjectUnordered )
        {
            if ( !V_strcmp( projectsUnordered[iProjectUnordered]->m_ProjectName.Get(), m_projects[iProject].GetName() ) )
            {
                iMatch = iProjectUnordered;
                break;
            }
        }
        if ( iMatch < 0 )
        {
            g_pVPC->VPCError( "Solution project mismatch ('%s' not found), unable to write solution",
                              m_projects[iProject].GetName() );
            return;
        }
        if ( m_projects[iProject].m_pDependencyProj )
        {
            g_pVPC->VPCError( "Solution project mismatch ('%s' found twice), unable to write solution",
                              m_projects[iProject].GetName() );
            return;
        }

        m_projects[iProject].SetDependencyProj( projectsUnordered[iMatch] );
    }

    m_solutionFilename = UsePOSIXSlashes( pSolutionFilename );
    
    //
    // m_projects must stay stable after this point as pointers
    // into it will be used as identities.
    //
    
    CUtlPathStringHolder sPbxProjFile( pSolutionFilename, ".xcodeproj/" );
	Sys_CreatePath( sPbxProjFile );
	sPbxProjFile.Set( pSolutionFilename, ".xcodeproj/project.pbxproj" );

    CUtlPathStringHolder sProjProjectListFile( sPbxProjFile, ".projects" );
    
	bool bUpToDate = !g_pVPC->IsForceGenerate() && Sys_Exists( sPbxProjFile );
    if ( bUpToDate )
    {
        int64 llSize = 0, llModTime = 0;
        bool bReadOnly = false;

        // the solution is up-to-date only if all the projects in it were up to date
		// and the solution was built after the mod time on all the project outputs
        int64 llLastModTime = 0;
        FOR_EACH_VEC( m_projects, iProject )
        {
            if ( bUpToDate && Sys_FileInfo( m_projects[iProject].m_projectOutputFile, llSize, llModTime, bReadOnly ) && llModTime > llLastModTime )
            {
                llLastModTime = llModTime;
            }
        }

        // regenerate pbxproj if it is older than the latest of the project output files
        if ( bUpToDate && ( !Sys_FileInfo( sPbxProjFile, llSize, llModTime, bReadOnly ) || llModTime < llLastModTime ) )
        {
            bUpToDate = false;
        }
    }

    // now go see if our project list agrees with the one on disk
    if ( bUpToDate )
    {
        FILE *fp = fopen( sProjProjectListFile, "rt" );
        if ( !fp )
            bUpToDate = false;

        char line[2048];
        char *pLine;

        if ( fp )
        {
            pLine = fgets( line, sizeof(line), fp );
            if ( !pLine )
            {
                bUpToDate = false;
            }
            else
            {
                if ( V_stricmp_fast( line, VPCCRCCHECK_FILE_VERSION_STRING "\n" ) )
                    bUpToDate = false;
            }
        }

        int cProjectsPreviously = 0;
        while ( bUpToDate )
        {
            pLine = fgets( line, sizeof(line), fp );
            if ( !pLine )
                break;

            ++cProjectsPreviously;

            int len = (int)strlen( line ) - 1;
            while ( line[len] == '\n' || line[len] == '\r' )
            {
                line[len] = '\0';
                len--;
            }

            // N^2 sucks, but N is small
            bool bProjectFound = false;
            FOR_EACH_VEC( m_projects, iProject )
            {
                CProject_Xcode *pProj = &m_projects[iProject];
                if ( V_stricmp_fast( pProj->GetName(), pLine ) == 0 )
                {
                    bProjectFound = true;
                    break;
                }
            }
            if ( !bProjectFound )
            {
                //fprintf( stderr, "%s has vanished from the project, regenerating...\n",  pLine );
                bUpToDate = false;
                break;
            }
        }
        if ( m_projects.Count() != cProjectsPreviously )
        {
            //fprintf( stderr, "Project count has changed (%d/%d), regenerating...\n",  cProjectsPreviously, m_projects.Count() );
            bUpToDate = false;
        }

        if ( fp )
        {
            fclose( fp );
        }
    }

	if ( bUpToDate )
	{
		g_pVPC->VPCStatus( true, "Xcode Project %s.xcodeproj looks up-to-date, not generating", pSolutionFilename );
		return;
	}

    ProcessGenerators();
    
    m_outBuf.Clear();

	m_nIndent = 0;
    
	Msg( "\nWriting master Xcode project %s.xcodeproj.\n\n", pSolutionFilename );

    /** header **/
	Write( "// !$*UTF8*$!\n{\n" );
    ++m_nIndent;
    {
        /** 
         **
         ** preamble
         **
         **/
        Write( "archiveVersion = 1;\n" );
        Write( "classes = {\n" );
        Write( "};\n" );
        Write( "objectVersion = 44;\n" );
        Write( "objects = {\n" );

        EmitBuildFileSection();
        EmitBuildRuleSection();
        EmitFileReferenceSection();
        EmitGroupSection();
        EmitSourcesBuildPhaseSection();
        EmitFrameworksBuildPhaseSection();
        EmitShellScriptBuildPhaseSection();
        EmitNativeTargetSection();
        EmitLegacyTargetSection();
        EmitAggregateTargetSection();
        EmitProjectSection();
        EmitContainerItemProxySection();
        EmitTargetDependencySection();
        EmitBuildConfigurationSection();
        EmitConfigurationListSection();

        Write( "};\n" ); // objects = { ...

        /** 
         **
         ** root object in the graph
         **
         **/
        Write( "rootObject = %024llX /* Project object */;\n", makeoid( EOIDSpaceFixed, EOIDFixedRoot, EOIDTypeProject ) );
    }
    --m_nIndent;
      
    Write( "}\n" );

    // and now write a .projects file inside the xcode project so we can detect the list of projects changing
    // (specifically a vpc project dissapearing from our target list)
    FILE *fp = fopen( sProjProjectListFile, "wt" );
    if ( !fp )
    {
        g_pVPC->VPCError( "Unable to open %s to write projects into.", sProjProjectListFile.Get() );
    }
    else
    {
        // we don't need to be quite as careful as project script, as we're only looking to catch cases where the rest of VPC thinks we're up-to-date
        fprintf( fp, "%s\n", VPCCRCCHECK_FILE_VERSION_STRING );
        FOR_EACH_VEC( m_projects, iProject )
        {
            CProject_Xcode *pProj = &m_projects[iProject];
            fprintf( fp, "%s\n", pProj->GetName() );
        }
        fclose( fp );
    }

    bool bFileUpToDate = !g_pVPC->IsForceGenerate() && !Sys_FileChanged( sPbxProjFile, m_outBuf, true );
    if ( bFileUpToDate )
    {
        // No need for this message right now.
#if 0
		g_pVPC->VPCStatus( true, "Xcode Project %s.xcodeproj did not change, not rewriting file", pSolutionFilename );
#endif
    }
    else
    {
        Sys_WriteFile( sPbxProjFile, m_outBuf, true );
        Sys_CopyToMirror( sPbxProjFile );
    }
}

void CSolutionGenerator_Xcode::Write( const char *pMsg, ... )
{
	for ( int i = 0; i < m_nIndent; i++ )
        m_outBuf.PutChar( '\t' );

	va_list marker;
	va_start( marker, pMsg );
	m_outBuf.VaPrintf( pMsg, marker );
	va_end( marker );
}

static CSolutionGenerator_Xcode g_SolutionGenerator_Xcode;
IBaseSolutionGenerator* GetXcodeSolutionGenerator()
{
	return &g_SolutionGenerator_Xcode;
}

class CProjectGenerator_Xcode : public IVCProjWriter
{
public:
	CProjectGenerator_Xcode();

	virtual const char *GetProjectFileExtension()
    {
        return "timestamp";
    }

	virtual bool Save( const char *pOutputFilename )
    {
		// we need the "project file" to exist for crc checking
		if ( !Sys_Exists( pOutputFilename ) )
        {
            g_pVPC->VPCStatus( true, "  creating project file" );
			Sys_Touch( pOutputFilename );
        }

		// remember if we needed rebuild according to vpc
		// and update the mod time on the file if we needed rebuild
        CUtlString statusStr;
		if ( !g_pVPC->IsProjectCurrent( g_pVPC->GetScript().GetName(), statusStr ) )
        {
            g_pVPC->VPCStatus( true, "  %s", statusStr.Get() );
			Sys_Touch( pOutputFilename );
        }
        
        return true;
    }
	
	virtual CVCProjGenerator *GetProjectGenerator() OVERRIDE { return m_pVCProjGenerator; }

protected:
	CVCProjGenerator *m_pVCProjGenerator;
};

CProjectGenerator_Xcode::CProjectGenerator_Xcode()
{
	// The makefile generator MUST use the 2010 vcproj generator for proper VPC parsing and feature support.
	// The output serialization is where the makefile distinction occurs.
	extern IVCProjWriter* GetWin32ProjectGenerator_VCProjWriter_2010();
	m_pVCProjGenerator = GetWin32ProjectGenerator_VCProjWriter_2010()->GetProjectGenerator();

    // The .vcxproj generator just added itself to the writers vector
    // but we don't want that since the usage of the 2010 generator
    // is an implementation detail here.
    m_pVCProjGenerator->RemoveLastProjectWriter();

	// write project output using our do-nothing serializer
	m_pVCProjGenerator->AddProjectWriter( this );
}

IBaseProjectGenerator* GetXcodeProjectGenerator()
{
	CProjectGenerator_Xcode *pNew = new CProjectGenerator_Xcode();
	return pNew->GetProjectGenerator();
}

void CSolutionGenerator_Xcode::ProjectEnd( IBaseProjectGenerator *pCurGen )
{
    // VPC will process projects twice, once in a regular parse and
    // once parse during dependency building.  This hack lets us
    // collect just one set of generators.
    if ( !g_pVPC->m_bIsDependencyPass )
    {
		AddProject( (CVCProjGenerator*)pCurGen );
    }
}

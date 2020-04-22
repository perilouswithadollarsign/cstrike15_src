//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//==================================================================================================
#include "vpc.h"
#include "bitvec.h"

void MakeFriendlyProjectName( char *pchProject );

#define SPACE_TAB "    " //4 spaces

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
IBaseProjectGenerator* GetMakefileProjectGenerator()
{
	CProjectGenerator_Makefile *pNew = new CProjectGenerator_Makefile();
	return pNew->GetProjectGenerator();
}

CProjectGenerator_Makefile::ScriptHelpers_t CProjectGenerator_Makefile::s_ScriptHelpers[ST_COUNT] = 
	{ 
		{ ".sh", "set -e\n\n", CBaseProjectDataCollector::DoShellScriptReplacements },
		{ ".bat", "", CBaseProjectDataCollector::DoBatchScriptReplacements }
	};

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CProjectGenerator_Makefile::CProjectGenerator_Makefile()
{
	// The makefile generator MUST use the 2010 vcproj generator for proper VPC parsing and feature support.
	// The output serialization is where the makefile distinction occurs.
	extern IVCProjWriter* GetWin32ProjectGenerator_VCProjWriter_2010();
	IVCProjWriter *pWin32Writer = GetWin32ProjectGenerator_VCProjWriter_2010();
	m_pVCProjGenerator = pWin32Writer->GetProjectGenerator();

	// The .vcxproj generator just added itself to the writers vector
	// but we don't want that since the usage of the 2010 generator
	// is an implementation detail here.
	m_pVCProjGenerator->RemoveLastProjectWriter();

	// write project output use a makefile serializer
	m_pVCProjGenerator->AddProjectWriter( this );

	if ( g_pVPC->IsConditionalDefined( "GENERATE_MAKEFILE_VCXPROJ" ) )
	{
		//we've been requested to write out the vcxproj as well. Re-add it instead of not-removing it because order seems important somewhere in the guts of the code
		m_pVCProjGenerator->AddProjectWriter( pWin32Writer );
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *CProjectGenerator_Makefile::UsePOSIXSlashes( const char *pStr )
{
    m_TempFixedPath.Set( pStr );
    V_FixSlashes( m_TempFixedPath.Access(), '/' );
	return m_TempFixedPath;
}

//--------------------------------------------------------------------------------------------------
// pExt should be the bare extension without the . in front. i.e. "h", "cpp", "lib".
//--------------------------------------------------------------------------------------------------
static inline bool CheckExtension( const char *pFilename, const char *pExt )
{
	Assert( pExt[0] != '.' );

	int nFilenameLen = V_strlen( pFilename );
	int nExtensionLen = V_strlen( pExt );

	return (nFilenameLen > nExtensionLen && pFilename[nFilenameLen-nExtensionLen-1] == '.' && V_stricmp_fast( &pFilename[nFilenameLen-nExtensionLen], pExt ) == 0 );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
static bool CheckExtensions( const char *pFilename, const char **ppExtensions )
{
	for ( int i=0; ppExtensions[i] != NULL; i++ )
	{
		if ( CheckExtension( pFilename, ppExtensions[i] ) )
			return true;
	}
	return false;
}

static void NeuterPathAscension( char *pPath )
{
	// Strip out any '../' and replace with 'up/'. Do this to avoid escaping
	// the $(OBJDIR) hierarchy.
	char *p = pPath;
	while( (p = strstr(p, "../")) != NULL ) {
		memcpy( p, "up", 2 );
		p += 3;
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
static void GetObjFilenameForFile( const char *pFilename, char *pOut, int maxLen )
{
	char sBasePath[MAX_FIXED_PATH];
	char sBaseFilename[MAX_BASE_FILENAME];
	char szRelPath[MAX_FIXED_PATH];

	// Make sure we always have a clean project-relative path.
	//  Subtle filename differences will confuse the hierarchy checks
	{
		V_MakeAbsolutePath( sBasePath, ARRAYSIZE( sBasePath ), pFilename, g_pVPC->GetProjectPath(), k_bVPCForceLowerCase );
		V_MakeRelativePath( sBasePath, g_pVPC->GetProjectPath(), szRelPath, sizeof( szRelPath ) );
		V_RemoveDotSlashes( szRelPath );
		V_FixSlashes( szRelPath, '/' );
		pFilename = szRelPath;
	}

	// Generate a path which is contained inside the $(OBJDIR) hierarchy.
	V_ExtractFilePath( pFilename, sBasePath, sizeof( sBasePath ) );
	V_AppendSlash( sBasePath, sizeof( sBasePath ) );
	NeuterPathAscension( sBasePath );

	V_FileBase( pFilename, sBaseFilename, sizeof( sBaseFilename ) );

	// We had .cxx files -> .oxx, but this macro:
	//   GENDEP_CXXFLAGS = -MD -MP -MF $(@:.o=.P)
	// was turning into -MF blah.oxx, which led to blah.oxx containing the file dependencies.
	// This obviously failed to link. Quick fix is to just have .cxx -> .o like everything else.
	//$ const char *pObjExtension = (CheckExtension( pFilename, "cxx" ) ? "oxx" : "o");
	const char *pObjExtension = "o";

	//V_snprintf( pOut, maxLen, "$(OBJ_DIR)/%s%s.%s", sBasePath, sBaseFilename, pObjExtension );
	V_snprintf( pOut, maxLen, "$(OBJ_DIR)/%s.%s", sBaseFilename, pObjExtension );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CProjectGenerator_Makefile::WriteSourceFilesList( CUtlBuffer &outBuf, const char *pListName, const char **pExtensions, const char *pConfigName )
{
	outBuf.Printf( "%s=", pListName );
	for ( int i=0; i < m_Files.Count(); i++ )
	{
		CProjectFile *pProjectFile = m_Files[i];
		if ( pProjectFile->IsExcludedFrom( pConfigName ) )
			continue;

		if ( CheckExtensions( m_Files[i]->m_Name.Get(), pExtensions ) )
		{
            outBuf.PutString( " \\\n" SPACE_TAB );

			const char *pFileName = UsePOSIXSlashes( m_Files[i]->m_Name.Get() );

			// Make sure we always have a clean project-relative path.
			//  Subtle filename differences will confuse the hierarchy checks
			char szRelPath[MAX_FIXED_PATH];
			{
				char szAbsPath[MAX_FIXED_PATH];
				V_MakeAbsolutePath( szAbsPath, ARRAYSIZE( szAbsPath ), pFileName, g_pVPC->GetProjectPath(), k_bVPCForceLowerCase );
				V_MakeRelativePath( szAbsPath, g_pVPC->GetProjectPath(), szRelPath, sizeof( szRelPath ) );
				V_RemoveDotSlashes( szRelPath );
				V_FixSlashes( szRelPath, '/' );
				pFileName = szRelPath;
			}
			
			outBuf.PutString( pFileName );
		}
	}
	outBuf.PutString( "\n\n" );	
	outBuf.Printf( "unexport %s\n", pListName );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CProjectGenerator_Makefile::WriteNonConfigSpecificStuff( CUtlBuffer &outBuf )
{
	outBuf.PutString( "#Generated by VPC\n\n" );

	// NAME
	char szName[256];
	V_strncpy( szName, m_pVCProjGenerator->GetProjectName(), sizeof(szName) );
	MakeFriendlyProjectName( szName );
	outBuf.Printf( "NAME=%s\n", szName );

	// TargetPlatformName
	const char *pTargetPlatformName = g_pVPC->GetTargetPlatformName();
	if ( !pTargetPlatformName )
		g_pVPC->VPCError( "GetTargetPlatformName failed." );
	outBuf.Printf( "TARGET_PLATFORM=%s\n", pTargetPlatformName );
	outBuf.Printf( "TARGET_PLATFORM_EXT=%s\n", g_pVPC->IsDedicatedBuild() ? "" : "_client" );

	// Record the host type that VPC was run on so that we can
	// block attempts to compile on a different host, with
	// possibly bizarre results.
	outBuf.Printf( "VPC_HOST=%s\n\n", g_pVPC->GetMacroValue( "HOST_PLATSUBDIR" ) + 1 );

	// SRCDIR
	CUtlStringBuilder *pStrBuf = g_pVPC->GetMacroReplaceBuffer();
	g_pVPC->ResolveMacrosInString( "$SRCDIR", pStrBuf );
	V_FixSlashes( pStrBuf->Access(), '/' );
	outBuf.PutString( "SRCROOT=" );
	outBuf.PutString( pStrBuf->Get() );
	outBuf.PutString( "\n" );

	if ( VPC_IsPlatformAndroid( pTargetPlatformName ) )
	{
		outBuf.PutString( "ANDROID_MAKEFILE=1\n" );

		g_pVPC->ResolveMacrosInString( "$ANDROID_NDK_DIR", pStrBuf );
		V_FixSlashes( pStrBuf->Access(), '/' );
		outBuf.Printf( "ANDROID_NDK_DIR=%s\n", pStrBuf->Get() );

		g_pVPC->ResolveMacrosInString( "$ANDROID_PLATFORM", pStrBuf );
		outBuf.Printf( "ANDROID_PLATFORM=%s\n", pStrBuf->Get() );
	}

	if ( VPC_IsPlatformLinux( pTargetPlatformName ) )
	{
		outBuf.PutString( "LINUX_MAKEFILE=1\n" );
	}

	outBuf.Printf( "USE_VALVE_BINDIR=%s\n", ( g_pVPC->UseValveBinDir() ? "1" : "0" ) );

	// Decide whether symbol files should be checked into Perforce or not.
	if ( g_pVPC->IsConditionalDefined( "SYMBOLS_IN_P4" ) )
	{
		outBuf.Printf( "P4_SYMS=1\n\n" );
	}
	
	// Select release config if no config is specified.
	outBuf.Printf( "# If no configuration is specified, \"release\" will be used.\nCFG ?= release\n" );
}

struct Filename_t
{
    CUtlPathStringHolder filename;
    CUtlStringHolder<50> basename;
};

class FileSortSortFunc
{
public:
	bool Less( const CProjectFile * const & src1, const CProjectFile * const & src2, void *pCtx )
	{
		return src1->m_nInsertOrder < src2->m_nInsertOrder;
	}
};

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CProjectGenerator_Makefile::CollectDependencies( const char *pDependencies, DependencyTable_t &dependenciesOut, const char *pFullFileNameForVisualStudioReplacements /*= nullptr*/ )
{
	if ( !pDependencies ||
		!pDependencies[0] )
	{
		return;
	}

	CUtlStringBuilder *pVsStr = g_pVPC->GetTempStringBuffer1();
	CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pDependencies, pVsStr, pFullFileNameForVisualStudioReplacements );

	const char *szProjectPath = g_pVPC->GetProjectPath();

	CSplitStringInPlace splitDeps( pVsStr->Access(), ';' );
	while ( splitDeps.HasNext() )
	{
		const char *szNext = splitDeps.GetNext();

		// Make sure we always have a clean project-relative path.
		//  Subtle filename differences will confuse the hierarchy checks
		char szRelative[MAX_PATH];
		char szAbsPath[MAX_FIXED_PATH];
		V_MakeAbsolutePath( szAbsPath, ARRAYSIZE( szAbsPath ), szNext, g_pVPC->GetProjectPath(), k_bVPCForceLowerCase );
		V_MakeRelativePath( szAbsPath, szProjectPath, szRelative, sizeof( szRelative ) );
		V_RemoveDotSlashes( szRelative );
		V_FixSlashes( szRelative, '/' );

		dependenciesOut.AddString( szRelative );
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CProjectGenerator_Makefile::WriteCustomDependencies( const char *pDependencies, CUtlBuffer &outBuf, DependencyTable_t *pDependenciesOut )
{
    if ( !pDependencies ||
         !pDependencies[0] )
    {
        return;
    }

    CUtlStringBuilder *pVsStr = g_pVPC->GetTempStringBuffer1();
    CBaseProjectDataCollector::DoStandardVisualStudioReplacements( pDependencies, pVsStr, NULL );

    CSplitStringInPlace splitDeps( pVsStr->Access(), ';' );
    while ( splitDeps.HasNext() )
    {
		const char *szNext = splitDeps.GetNext();
        outBuf.PutChar( ' ' );
        outBuf.PutString( szNext );
		if ( pDependenciesOut )
		{
			pDependenciesOut->AddString( szNext );
		}
    }
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CProjectGenerator_Makefile::WriteCustomBuildTool(	CProjectConfiguration *pConfig,
														CUtlBuffer &outBuf,
														ScriptType_t scriptType,
														CProjectFile *pProjectFile,
														const char *pFixedProjectFileName,
														const char *szVPCGeneratedScriptsBasePath,
														DependencyTable_t &dependenciesOut,
														DependencyTable_t &generatedScriptsOut,
														DependencyTable_t &outputsOut )
{
    // Property lookups naturally bubble out from more-specific
    // to less-specific, so if we're looking at a specific
    // file make sure that it has a local property so that
    // we don't bubble up to a global property and duplicate
    // it for the file.
    if ( pProjectFile )
    {
        if ( !m_pVCProjGenerator->HasFilePropertyValue( pProjectFile, pConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_Outputs ) )
        {
            return false;
        }
    }
    
    const char *outputsString = m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_Outputs );
    if ( !outputsString || !outputsString[0] )
    {
        return false;
    }
    
    const char *commandLineString = m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_CommandLine );
    if ( !commandLineString || !commandLineString[0] )
    {
        return false;
    }

	const char *descriptionString = m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_Description );
    if ( !descriptionString )
	{
		descriptionString = "";
	}

    char szAbsPath[MAX_FIXED_PATH];
	char szRelativePath[MAX_FIXED_PATH];
	const char *pRelativeFilePath = nullptr;

    if ( pFixedProjectFileName &&
         pFixedProjectFileName[0] )
    {
		V_MakeAbsolutePath( szAbsPath, sizeof( szAbsPath ), pFixedProjectFileName, NULL, k_bVPCForceLowerCase );
        V_FixSlashes( szAbsPath, '/' );

		V_strncpy( szRelativePath, pFixedProjectFileName, ARRAYSIZE( szRelativePath ) );
        V_FixSlashes( szRelativePath, '/' );
		V_RemoveDotSlashes( szRelativePath, '/' );

		pFixedProjectFileName = szAbsPath;
		pRelativeFilePath = szRelativePath;
    }

	if ( outBuf.TellPut() != 0 )
	{
		outBuf.PutString( "\n\n" );
	}

    //
    // We can't just emit a make rule with all of the outputs as make considers all of the
    // outputs independent and can schedule multiple make jobs to simultaneously
    // create the outputs.  We want to treat custom build steps as atomic units so
    // we have to create an intermediate fake file to be the sole output and then
    // make the real outputs depend on it.  We use an actual file so that
    // make's file timestamp checks still work for incremental building.
    //
    // We skip this dance if there's only one output anyway.
    //
    
    CUtlStringBuilder *pVsStr = g_pVPC->GetTempStringBuffer1();

	CUtlString buildToolName;

	CBaseProjectDataCollector::DoStandardVisualStudioReplacements( descriptionString, pVsStr, pFixedProjectFileName );
	if ( pRelativeFilePath )
	{
		outBuf.Printf(	"#\n"
						"# Custom Build Tool \"%s\" for file \"%s\"\n"
						"#\n", pVsStr->Get(), pRelativeFilePath );
		buildToolName = pRelativeFilePath;
	}
	else
	{
		outBuf.Printf(	"#\n"
						"# Global Custom Build Tool \"%s\"\n"
						"#\n", pVsStr->Get() );
		buildToolName = "global";
	}

	//sanitize the build tool name so it sits where we want it
	buildToolName = buildToolName.Replace( "../", "updir_" );
	buildToolName = buildToolName.Replace( "./", "samedir_" );
	buildToolName = buildToolName.Replace( ".", "_dot_" );
	buildToolName = buildToolName.Replace( "/", "_dir_" );

	//write out the shell script
	CUtlString shellScriptFileName;
	{
		shellScriptFileName.Format( "%s/custom_build_tools/%s", szVPCGeneratedScriptsBasePath, buildToolName.Get() );
		shellScriptFileName += s_ScriptHelpers[scriptType].szExtension;

		CBaseProjectDataCollector::DoStandardVisualStudioReplacements( commandLineString, pVsStr, pFixedProjectFileName );
		WriteScriptFile( shellScriptFileName.Get(), pVsStr, scriptType );
	}

	//collect all the dependencies into DEPENDENCIES_FOR_BUILD_TOOL_%s
	CFmtStr dependenciesVarName( "DEPENDENCIES_FOR_BUILD_TOOL_%s", buildToolName.Get() );
	DependencyTable_t dependenciesTable;
	{
		if ( pRelativeFilePath )
		{
			dependenciesTable.AddString( pRelativeFilePath );
		}

		CollectDependencies( m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies ), dependenciesTable, pFixedProjectFileName );
		// Don't emit project dependencies if this is a rule for a specific file.
		if ( !pProjectFile )
		{
			CollectDependencies( m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies_Proj ), dependenciesTable, pFixedProjectFileName );
		}

		//copy dependencies into the function output table
		for ( int i = 0; i < dependenciesTable.GetNumStrings(); i++ )
		{
			dependenciesOut.AddString( dependenciesTable.GetElementString( i ) );
		}

		WriteDependencies( dependenciesVarName.Get(), dependenciesTable, outBuf );		
	}

	//collect all the order-only dependencies into ORDER_ONLY_DEPENDENCIES_FOR_BUILD_TOOL_%s
	CFmtStr orderOnlyDependenciesVarName( "ORDER_ONLY_DEPENDENCIES_FOR_BUILD_TOOL_%s", buildToolName.Get() );
	DependencyTable_t orderOnlyDependenciesTable;
	{
		CollectDependencies( m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_OrderOnlyFileDependencies ), orderOnlyDependenciesTable, pFixedProjectFileName );

		outBuf.PutString( "\n" );
		WriteDependencies( orderOnlyDependenciesVarName.Get(), orderOnlyDependenciesTable, outBuf );
	}

	generatedScriptsOut.AddString( shellScriptFileName.Get() );

	//load the description into pVsStr
	CBaseProjectDataCollector::DoStandardVisualStudioReplacements( m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_Description ), pVsStr, pFixedProjectFileName );
	
	outBuf.Printf(	"\n"
													"CUSTOM_BUILD_TOOL_ACTION_%s = \\\n"
					SPACE_TAB						"{ \\\n"
					SPACE_TAB SPACE_TAB					"$(ECHO) \"%s\" && \\\n"
					SPACE_TAB SPACE_TAB					"{ $(SHELL) %s $(PROCESS_COMPILER_OUTPUT); } && \\\n"
					SPACE_TAB SPACE_TAB SPACE_TAB		"$(TOUCH) \"$(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/sentinels/%s\" || \\\n"
					SPACE_TAB SPACE_TAB SPACE_TAB		"{ $(ECHO) \"Error executing custom build tool \\\"%s\\\"\"; false }; }; \\\n"
					SPACE_TAB						"}\n\n"
													"unexport CUSTOM_BUILD_TOOL_ACTION_%s\n",
					buildToolName.Get(), pVsStr->String(), shellScriptFileName.Get(), buildToolName.Get(), shellScriptFileName.Get(), buildToolName.Get() );
	
	//collect all the outputs into OUTPUTS_FOR_BUILD_TOOL_%s
	CFmtStr outputsVarName( "OUTPUTS_FOR_BUILD_TOOL_%s", buildToolName.Get() );
	DependencyTable_t outputFilesDependencyTable;
	{
		CollectDependencies( outputsString, outputFilesDependencyTable, pFixedProjectFileName );

		//copy outputs into the function output table
		for ( int i = 0; i < outputFilesDependencyTable.GetNumStrings(); i++ )
		{
			outputsOut.AddString( outputFilesDependencyTable.GetElementString( i ) );
		}

		outBuf.PutString( "\n" );
		WriteDependencies( outputsVarName.Get(), outputFilesDependencyTable, outBuf );
	}

	//collect all the potential outputs into POTENTIAL_OUTPUTS_FOR_BUILD_TOOL_%s
	CFmtStr potentialOutputsVarName( "POTENTIAL_OUTPUTS_FOR_BUILD_TOOL_%s", buildToolName.Get() );
	DependencyTable_t potentialOutputFilesDependencyTable;
	{
		const char *potentialOutputsString = m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_PotentialOutputs );
		CollectDependencies( potentialOutputsString, potentialOutputFilesDependencyTable, pFixedProjectFileName );

		outBuf.PutString( "\n" );
		WriteDependencies( potentialOutputsVarName.Get(), potentialOutputFilesDependencyTable, outBuf );
	}

	//clear the mutex and "already run" sentinels on build start
	{
		outBuf.Printf(	"\n"
						"_prebuild_always::\n"
						"\t"	"$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) _prebuild_always for custom build tool %s start\n"
						"\t"	"$(QUIET_PREFIX) $(RM) -fr $(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/ranthisbuild/%s\n"
						"\t"	"$(QUIET_PREFIX) $(RM) -fr $(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/locks/%s\n",  buildToolName.Get(), buildToolName.Get(), buildToolName.Get() );
	}

	//create the rule to run the tool
	{
		outBuf.PutString( "\n" );
		outBuf.Printf( "$(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/sentinels/%s $(OUTPUTS_FOR_BUILD_TOOL_%s): %s $(%s) | _prebuild_steps $(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/_vpc_create_dirs $(%s)\n", buildToolName.Get(), buildToolName.Get(), shellScriptFileName.Get(), dependenciesVarName.Get(), orderOnlyDependenciesVarName.Get() );
		outBuf.Printf( "\t$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) custom build tool for %s start\n", buildToolName.Get() );
		outBuf.PutString( "\t$(QUIET_PREFIX) $(call SPEW_UPDATED_DEPENDENCIES)\n" );
		outBuf.PutString( "\t$(QUIET_PREFIX) $(call RUN_PREBUILD_EVENT_ONCE)\n" );
		outBuf.Printf( "\t$(QUIET_PREFIX) $(call RUN_RECIPE_ACTION_ONCE,\"$(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/ranthisbuild/%s\",\"$(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/locks/%s\",$(call CUSTOM_BUILD_TOOL_ACTION_%s))\n", buildToolName.Get(), buildToolName.Get(), buildToolName.Get() );
	}

	//mark all the potential outputs as invalidated the sentinel so their timestamps are evaluated after the tool has run
	if ( potentialOutputFilesDependencyTable.GetNumStrings() != 0 )
	{
		outBuf.Printf( "\n$(%s): $(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/sentinels/%s ;\n", potentialOutputsVarName.Get(), buildToolName.Get() );
	}

	m_CustomBuildTools.AddToTail( buildToolName );

    return true;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CProjectGenerator_Makefile::WriteDefines( CProjectConfiguration *pConfig,
                                               CUtlBuffer &outBuf,
                                               CProjectFile *pProjectFile )
{
    const char *preprocessorDefinitionsString = m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_PreprocessorDefinitions );
    if ( strchr( preprocessorDefinitionsString, ' ' ) != NULL ||
         strchr( preprocessorDefinitionsString, '\t' ) != NULL )
    {
        g_pVPC->VPCError( "$PreprocessorDefinitions contains whitespace: '%s'", preprocessorDefinitionsString );
    }
    if ( strchr( preprocessorDefinitionsString, ',' ) != NULL )
    {
        g_pVPC->VPCError( "$PreprocessorDefinitions contains commas, use semicolons as separators: '%s'", preprocessorDefinitionsString );
    }

    CUtlStringBuilder *pStrBuf = g_pVPC->GetTempStringBuffer1();
    pStrBuf->Set( preprocessorDefinitionsString );
    CSplitStringInPlace outStrings( pStrBuf->Access(), ';' );
    while ( outStrings.HasNext() )
    {
        outBuf.PutString( "-D" );
        outBuf.PutString( outStrings.GetNext() );
        outBuf.PutString( " " );
    }
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CProjectGenerator_Makefile::WriteIncludes( CProjectConfiguration *pConfig,
                                                CUtlBuffer &outBuf,
                                                CProjectFile *pProjectFile )
{
    const char *additionalIncludeDirectoriesString = m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_AdditionalIncludeDirectories );
    CSplitString outStrings( additionalIncludeDirectoriesString, (const char**)g_IncludeSeparators, V_ARRAYSIZE( g_IncludeSeparators ) );
    CUtlStringBuilder *pStrBuf = g_pVPC->GetTempStringBuffer1();
    for ( int i = 0; i < outStrings.Count(); i++ )
    {
        pStrBuf->Set( outStrings[i] );
        if ( !V_stricmp_fast( pStrBuf->Get(), "$(IntDir)" ) )
            pStrBuf->Set( "$(OBJ_DIR)" );
        V_FixSlashes( pStrBuf->Access(), '/' );

        outBuf.PutString( pStrBuf->Get() );
        outBuf.PutString( " " );
    }
}


//returns true if $ORIGIN was used
bool FormatRPaths( const char *pRawString, CUtlString &output, char cNewPathSeparator, const char *szOriginReplacement )
{
	bool bRetVal = false;

	output.Clear();
	if ( !pRawString || !pRawString[0] )
		return bRetVal;

	CUtlStringList inputList;
	inputList.SplitString( pRawString, ":" );

	CUtlStringList outputList;
	CUtlString compositionTemp;

	for ( int listIter = 0; listIter < inputList.Count(); ++listIter )
	{
		const char *pEntry = inputList[listIter];
		if ( !pEntry || !pEntry[0] )
			continue;

		compositionTemp.Clear();		

		const char *pParse = pEntry;
		const char *pEnd = pParse + V_strlen( pParse );
		while ( pParse < pEnd )
		{
			const char *pOrigin = szOriginReplacement ? V_stristr_fast( pParse, "$ORIGIN" ) : NULL;
			if ( pOrigin )
			{
				bRetVal = true;

				compositionTemp.Append( pParse, pOrigin - pParse );
				compositionTemp.Append( szOriginReplacement );
				
				pParse = pOrigin + V_strlen( "$ORIGIN" );
			}
			else
			{
				compositionTemp.Append( pParse, pEnd - pParse );
				pParse = pEnd;
			}
		}

		if ( compositionTemp.Length() > 0 )
		{
			//fix up paths
			V_FixSlashes( compositionTemp.Access(), '/' );

			//make sure we're not writing dupes
			bool bFound = false;
			for ( int dupeIter = 0; dupeIter < outputList.Count(); ++dupeIter )
			{
				const char *pDupeEntry = outputList[dupeIter];

				if ( V_strcmp( compositionTemp.Get(), pDupeEntry ) == 0 )
				{
					bFound = true;
					break;
				}
			}

			//write if not a dupe
			if ( !bFound )
			{
				outputList.CopyAndAddToTail( compositionTemp.Get() );
			}
		}
	}

	for ( int outputIter = 0; outputIter < outputList.Count(); ++outputIter )
	{
		if ( output.Length() != 0 )
		{
			output.Append( cNewPathSeparator );
		}

		output.Append( outputList[outputIter] );
	}

	return bRetVal;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CProjectGenerator_Makefile::WriteConfigSpecificStuff( CProjectConfiguration *pConfig, CUtlBuffer &outBuf, ScriptType_t scriptType )
{
    char szAbsPath[MAX_FIXED_PATH];
    char szRelPath[MAX_FIXED_PATH];

	CUtlString formattedRPaths;
	bool bRPathsUseOrigin = FormatRPaths( m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_LINKER, g_pOption_POSIX_RPaths ), formattedRPaths, ':', "$$ORIGIN" ); //double dollar signs because we're going through make
	
	outBuf.Printf(	"\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
					"################################################################################\n"
					"#\n"
					"# CFG=%s\n"
					"#\n"
					"################################################################################\n\n",
					pConfig->m_LowerCaseName.Get() );
    outBuf.Printf( "ifeq \"$(CFG)\" \"%s\"\n\n", pConfig->m_LowerCaseName.Get() );

	// GCC_ExtraCompilerFlags
	// Hopefully, they don't ever need to use backslashes because we're turning them into forward slashes here.
	// If that does become a problem, we can put some token around the pathnames we need to be fixed up and leave the rest alone.
	outBuf.Printf( "GCC_ExtraCompilerFlags=%s\n", UsePOSIXSlashes( m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_GCC_ExtraCompilerFlags ) ) );
	outBuf.Printf( "GCC_ExtraCxxCompilerFlags=%s\n", UsePOSIXSlashes( m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_GCC_ExtraCxxCompilerFlags ) ) );

	// GCC_ExtraLinkerFlags
	outBuf.Printf( "GCC_ExtraLinkerFlags=%s", m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_LINKER, g_pOption_GCC_ExtraLinkerFlags ) );
	if ( formattedRPaths.Length() )
	{
		if ( bRPathsUseOrigin )
		{
			outBuf.Printf( " -Wl,-z,origin" );
		}
		outBuf.Printf( " -Wl,-rpath,%s", formattedRPaths.Get() );
	}
	outBuf.Printf( "\n" );

    // DisableLinkerDeadCodeElimination
	outBuf.Printf( "DisableLinkerDeadCodeElimination=%s\n", m_pVCProjGenerator->GetPropertyValueAsBool( NULL, pConfig->m_Name.Get(), KEYWORD_LINKER, g_pOption_DisableLinkerDeadCodeElimination ) ? "1" : "" );

	// SymbolVisibility
	outBuf.Printf( "SymbolVisibility=%s\n", m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_SymbolVisibility, "hidden" ) );

    // TreatWarningsAsErrors
    outBuf.Printf( "TreatWarningsAsErrors=%s\n", m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_COMPILER, "$TreatWarningsAsErrors", "false" ) );

	// OptimizerLevel
	outBuf.Printf( "OptimizerLevel=%s\n", m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_OptimizerLevel, "$(SAFE_OPTFLAGS_GCC_422)" ) );
		
	// Precompiled header.
    const char *pchOption = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_PrecompiledHeader, "" );
    const char *pchInclude = "";
    const char *pchIncludeUnqual = "";
    CUtlString pchIncludeRelative;
#if defined( WIN32 ) && 1
    // The cross-compiling gcc has a bug with PCH handling so
    // we can't use PCH in that case, default to it being off.
    bool bDisableMakefilePch = g_pVPC->FindOrCreateConditional( "ENABLE_MAKEFILE_PCH", false, CONDITIONAL_NULL ) == NULL;
#else
    bool bDisableMakefilePch = true; // g_pVPC->FindOrCreateConditional( "DISABLE_MAKEFILE_PCH", false, CONDITIONAL_NULL ) != NULL;
#endif
    if ( !bDisableMakefilePch &&
         pchOption &&
         pchOption[0] &&
         V_stristr_fast( pchOption, "Not Using" ) == NULL )
    {
        pchInclude = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_UsePCHThroughFile, "" );
    }
	outBuf.Printf( "PrecompiledHeaderInclude=%s\n", pchInclude );
    if ( pchInclude && pchInclude[0] )
    {
        pchIncludeUnqual = V_UnqualifiedFileName( pchInclude );

		// The PCH header file needs to have a path relative to the project
		// path.
		const char *additionalIncludeDirectoriesString = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_AdditionalIncludeDirectories );
		CSplitString incStrings( additionalIncludeDirectoriesString, (const char**)g_IncludeSeparators, V_ARRAYSIZE( g_IncludeSeparators ) );
		for ( int i = 0; i < incStrings.Count(); i++ )
		{
			V_MakeAbsolutePath( szAbsPath, sizeof( szAbsPath ), incStrings[i], NULL, k_bVPCForceLowerCase );
			V_strncat( szAbsPath, "/", ARRAYSIZE( szAbsPath ) );
			V_strncat( szAbsPath, pchInclude, ARRAYSIZE( szAbsPath ) );
			V_FixSlashes( szAbsPath );
			V_FixDoubleSlashes( szAbsPath );
			if ( Sys_Exists( szAbsPath ) )
			{
				V_MakeRelativePath( szAbsPath, g_pVPC->GetProjectPath(), szRelPath, sizeof( szRelPath ) );
				V_RemoveDotSlashes( szRelPath );
				pchIncludeRelative = szRelPath;
				break;
			}
		}

		// If we couldn't find it on the include paths fall back
		// on assuming it's relative to the current directory.
        if ( pchIncludeRelative.IsEmpty() )
        {
            V_MakeAbsolutePath( szAbsPath, sizeof( szAbsPath ), pchInclude, NULL, k_bVPCForceLowerCase );
			V_FixDoubleSlashes( szAbsPath );
			V_MakeRelativePath( szAbsPath, g_pVPC->GetProjectPath(), szRelPath, sizeof( szRelPath ) );
			V_RemoveDotSlashes( szRelPath );
			V_FixSlashes( szRelPath, '/' );
            pchIncludeRelative = szRelPath;
        }
    }

	// system libraries
    {
        outBuf.Printf( "SystemLibraries=" );
        {
			const char *systemLibrariesString = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_LINKER, g_pOption_SystemLibraries );
            CSplitString libs( systemLibrariesString, (const char**)g_IncludeSeparators, V_ARRAYSIZE( g_IncludeSeparators ) );
            for ( int i=0; i < libs.Count(); i++ )
            {
                outBuf.Printf( "-l%s ", libs[i] );
            }
        }

        if ( VPC_IsPlatformOSX( g_pVPC->GetTargetPlatformName() ) )
        {
            CUtlString rgchFrameworkCompilerFlags;

			const char *systemFrameworksString = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_LINKER, g_pOption_SystemFrameworks );
            CSplitString systemFrameworks( systemFrameworksString, (const char**)g_IncludeSeparators, V_ARRAYSIZE( g_IncludeSeparators ) );
            for ( int i = 0; i < systemFrameworks.Count(); i++ )
            {
                outBuf.Printf( "-framework %s ", systemFrameworks[i] );
            }

			const char *localFrameworksString = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_LINKER, g_pOption_LocalFrameworks );
            CSplitString localFrameworks( localFrameworksString, (const char**)g_IncludeSeparators, V_ARRAYSIZE( g_IncludeSeparators ) );
            for ( int i = 0; i < localFrameworks.Count(); i++ )
            {
                char rgchFrameworkName[MAX_BASE_FILENAME];
                V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );
                V_StripFilename( localFrameworks[i] );
                outBuf.Printf( "-F%s ", localFrameworks[i] );
                outBuf.Printf( "-framework %s ", rgchFrameworkName );
                rgchFrameworkCompilerFlags.Append( "-F" );
                rgchFrameworkCompilerFlags.Append( localFrameworks[i] );
            }
            outBuf.Printf( "\n" );

            if ( !rgchFrameworkCompilerFlags.IsEmpty() )
			{
                // the colon here is important - and should probably get percolated to more places in our generated
                // makefiles - it means to perform the assignment once, rather than at evaluation time
                outBuf.Printf( "GCC_ExtraCompilerFlags:=$(GCC_ExtraCompilerFlags) %s\n", rgchFrameworkCompilerFlags.Get() );
			}
        }
        else
		{
            outBuf.Printf( "\n" );
		}
    }
		
	const char *pMacroValue = g_pVPC->GetMacroValue( "_DLL_EXT" );
	if ( pMacroValue[0] )
	{
		outBuf.Printf( "DLL_EXT=%s\n", pMacroValue );
	}

	pMacroValue = g_pVPC->GetMacroValue( "_SYM_EXT" );
	if ( pMacroValue[0] )
	{
		outBuf.Printf( "SYM_EXT=%s\n", pMacroValue );
	}

	pMacroValue = g_pVPC->GetMacroValue( "DEVTOOLS" );
	if ( pMacroValue[0] )
	{
		outBuf.Printf( "DEVTOOLS := %s\n", UsePOSIXSlashes( pMacroValue ) );
	}

	pMacroValue = g_pVPC->GetMacroValue( "VPC_SCRIPTS" );
	if ( pMacroValue[0] )
	{
		outBuf.Printf( "VPC_SCRIPTS := %s\n", UsePOSIXSlashes( pMacroValue ) );
	}

	if ( g_pVPC->IsConditionalDefined( "POSIX" ) )
	{
		outBuf.Printf( "MAKEFILE_BASE := $(DEVTOOLS)/makefile_base_posix.mak\n" );
		outBuf.Printf( "COMPILE_DEPENDANT_MAKEFILES := %s $(MAKEFILE_BASE)\n", GetProjectGenerator()->GetOutputFileName() );
	}
	else
	{
		outBuf.Printf( "COMPILE_DEPENDANT_MAKEFILES := %s\n", GetProjectGenerator()->GetOutputFileName() );
	}
		
	// ForceIncludes
	{
		const char *forceIncludesString = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_ForceInclude );
		CSplitString outStrings( forceIncludesString, (const char**)g_IncludeSeparators, V_ARRAYSIZE( g_IncludeSeparators ) );
		outBuf.Printf( "FORCEINCLUDES= " );
		for ( int i=0; i < outStrings.Count(); i++ )
		{
			if ( V_strlen( outStrings[i] ) > 2 )
			{
				outBuf.Printf( "-include %s ", UsePOSIXSlashes( outStrings[i] ) );
			}
		}
	}
	outBuf.Printf( "\n" );

	// CFG_DEFINES
    outBuf.Printf( "DEFINES += " );
    WriteDefines( pConfig, outBuf, NULL );
    outBuf.Printf( "\n" );

	// CFG_INCLUDEDIRS
    outBuf.Printf( "INCLUDEDIRS += " );
    WriteIncludes( pConfig, outBuf, NULL );
    outBuf.Printf( "\n" );

	// CONFTYPE
	bool bProjectIsLib = false;
	const char *configurationTypeString = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_GENERAL, g_pOption_ConfigurationType );
	if ( V_stristr_fast( configurationTypeString, ".dll" ) )
	{
		outBuf.Printf( "CONFTYPE=dll\n" );

		// Write ImportLibrary for dll (so) builds.
		const char *importLibraryString = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_LINKER, g_pOption_ImportLibrary );
		outBuf.Printf( "IMPORTLIBRARY=%s\n", UsePOSIXSlashes( importLibraryString ) );
	}
	else if ( V_stristr_fast( configurationTypeString, ".lib" ) )
	{
		bProjectIsLib = true;
		outBuf.Printf( "CONFTYPE=lib\n" );
	}
	else if ( V_stristr_fast( configurationTypeString, ".exe" ) )
	{
		outBuf.Printf( "CONFTYPE=exe\n" );
	}
	else
	{
		g_pVPC->VPCError( "Unknown configuration type %s.", configurationTypeString );
		outBuf.Printf( "CONFTYPE=***UNKNOWN***\n" );
	}

    CUtlStringBuilder *pVsStr = g_pVPC->GetTempStringBuffer1();

	// OutputFile is where it builds to.
	CUtlPathStringHolder outputFile;
   {
		const char *outputFileString = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), bProjectIsLib ? KEYWORD_LIBRARIAN : KEYWORD_LINKER, g_pOption_OutputFile );
		const char *fixedOutputFile = UsePOSIXSlashes( outputFileString );

		V_MakeAbsolutePath( szAbsPath, sizeof( szAbsPath ), fixedOutputFile, NULL, k_bVPCForceLowerCase );
		CBaseProjectDataCollector::DoStandardVisualStudioReplacements( fixedOutputFile, pVsStr, szAbsPath );

		outBuf.Printf( "OUTPUTFILE=%s\n", pVsStr->String() );

		outputFile.Set( outputFileString );
		outputFile.FixSlashesAndDotSlashes( '/' );
	}

	// GameOutputFile is where it copies OutputFile to.
	{
		const char *gameOutputFileString = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_GENERAL, g_pOption_GameOutputFile );
		const char *fixedGameOutputFile = UsePOSIXSlashes( gameOutputFileString );

		V_MakeAbsolutePath( szAbsPath, sizeof( szAbsPath ), fixedGameOutputFile, NULL, k_bVPCForceLowerCase );
		CBaseProjectDataCollector::DoStandardVisualStudioReplacements( fixedGameOutputFile, pVsStr, szAbsPath );

		outBuf.Printf( "GAMEOUTPUTFILE=%s\n", pVsStr->String() );
	}
        
    // Write all the filenames.
	const char *sSourceFileExtensions[] = { "gen_cpp", "cpp", "cxx", "cc", "c", "mm", NULL };
	outBuf.Printf( "\n" );
	WriteSourceFilesList( outBuf, "CPPFILES", (const char**)sSourceFileExtensions, pConfig->m_Name.Get() );

	// LIBFILES
	const char *importLibraryString = bProjectIsLib ? "" : m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_LINKER, g_pOption_ImportLibrary );
	CUtlPathStringHolder importLibraryFile( importLibraryString );
    importLibraryFile.FixSlashesAndDotSlashes( '/' );

	outBuf.PutString( "\n" );
	outBuf.Printf( "LIBFILES =" );

	// Get original order the link files were specified in the .vpc files. See:
	//  http://stackoverflow.com/questions/45135/linker-order-gcc
	// TL;DR. Gcc does a single pass through the list of libraries to resolve references.
	//  If library A depends on symbols in library B, library A should appear first so we
	//  need to restore the original order to allow users to control link order via
	//  their .vpc files.
	CUtlSortVector< CProjectFile *, FileSortSortFunc > OriginalSort;
	for ( int i = 0; i < m_Files.Count(); i++ )
	{
		CProjectFile *pProjectFile = m_Files[i];
		OriginalSort.InsertNoSort( pProjectFile );
	}
	OriginalSort.RedoSort();

	CUtlVector< Filename_t > ImportLib;
	CUtlVector< Filename_t > StaticLib;

	for ( int i = 0; i < OriginalSort.Count(); i++ )
	{
		CProjectFile *pProjectFile = OriginalSort[i];
		if ( pProjectFile->IsExcludedFrom( pConfig->m_Name.Get() ) )
			continue;

		Filename_t Filename;

        Filename.filename.Set( pProjectFile->m_Name.Get() );
        Filename.filename.FixSlashes( '/' );
		const char *pFilename = Filename.filename;
		if ( (pProjectFile->m_iFlags & (VPC_FILE_FLAGS_STATIC_LIB | VPC_FILE_FLAGS_IMPORT_LIB)) != 0 )
		{
			char *pchFileName = (char*)V_strrchr( pFilename, '/' ) + 1; 
			if (( importLibraryFile.IsEmpty() || V_stricmp_fast( importLibraryFile, pFilename )) && // only link this as a library if it isn't our own output!
				( outputFile.IsEmpty() || V_stricmp_fast( outputFile, pFilename )))
			{
				if ( pProjectFile->m_iFlags & VPC_FILE_FLAGS_IMPORT_LIB )
				{
					Assert( pchFileName[0] == 'l' && pchFileName[1] == 'i' && pchFileName[2] == 'b' );
					*(pchFileName-1) = 0;

                                        // Remove the ".so" or ".a" extension from the library
                                        char *pExt = (char*)V_strrchr( pchFileName, '.' );
                                        if ( pExt && V_strrchr( pchFileName, '/' ) < pExt && V_strrchr( pchFileName, '\\' ) < pExt )
                                                *pExt = 0;
						
					// +3 to dodge the lib prefix.
					Filename.basename.Set( pchFileName + 3 );
					ImportLib.AddToTail( Filename );
				}
				else
				{
					StaticLib.AddToTail( Filename );
				}
			}
		}
	}

	// Spew static libs out first, then import libraries. Otherwise things like bsppack
	//	will fail to link because libvstdlib.so came before tier1.a.
	for( int i = 0; i < StaticLib.Count(); i++ )
	{
		outBuf.Printf( " \\\n" SPACE_TAB "%s", StaticLib[ i ].filename.Get() );
	}

	for( int i = 0; i < ImportLib.Count(); i++ )
	{
		outBuf.Printf( " \\\n" SPACE_TAB "-L%s -l%s", ImportLib[ i ].filename.Get(), ImportLib[ i ].basename.Get() );
	}
	outBuf.Printf( "\n\n" );
	outBuf.Printf( "unexport LIBFILES\n\n" );
		
	outBuf.Printf( "LIBFILENAMES =" );
	for ( int i = 0; i < m_Files.Count(); i++ )
	{
		CProjectFile *pProjectFile = m_Files[i];
		if ( pProjectFile->IsExcludedFrom( pConfig->m_Name.Get() ) )
			continue;
			
		const char *pFilename = pProjectFile->m_Name.String();
		if ( (pProjectFile->m_iFlags & (VPC_FILE_FLAGS_STATIC_LIB | VPC_FILE_FLAGS_IMPORT_LIB)) != 0 )
		{
			if (( importLibraryFile.IsEmpty() || V_stricmp_fast( importLibraryFile, pFilename ) ) &&  // only link this as a library if it isn't our own output!
				( outputFile.IsEmpty() || V_stricmp_fast( outputFile, pFilename)))
			{
				outBuf.Printf( " \\\n" SPACE_TAB "%s", UsePOSIXSlashes( pFilename ) );
			}
		}
	}		
	outBuf.Printf( "\n\n" );
	outBuf.Printf( "unexport LIBFILENAMES\n\n" );

	CUtlVector< CUtlString > allDepGenFiles;
	CUtlBuffer createScriptSentinelDirectoriesBuf, depGenRulesBuf, compileRulesBuf, customBuildToolsBuf;
	createScriptSentinelDirectoriesBuf.SetBufferType( true, false );
    depGenRulesBuf.SetBufferType( true, false );
	compileRulesBuf.SetBufferType( true, false );
	customBuildToolsBuf.SetBufferType( true, false );

//#define ADD_DEPGEN_TARGETS

	// PCH generation and dependency tracking
    if ( pchInclude && pchInclude[0] )
    {
		CFmtStr depGenFile( "$(PCH_OBJ_DIR)/$(PCH_PRE)%s.P", pchIncludeUnqual );

#if defined( ADD_DEPGEN_TARGETS )
        depGenRulesBuf.Printf( "%s: %s $(COMPILE_DEPENDANT_MAKEFILES) | _predepgen_steps\n",
                       depGenFile.Get(),
                       pchIncludeRelative.Get() );
		depGenRulesBuf.Printf(		"\t$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) pch depgen for %s start\n", depGenFile.Get() );
		depGenRulesBuf.PutString(	"\t$(QUIET_PREFIX) $(call SPEW_UPDATED_DEPENDENCIES)\n" );
		depGenRulesBuf.Printf(		"\t$(QUIET_PREFIX) $(call RUN_PREBUILD_EVENT_ONCE)\n" );
        depGenRulesBuf.Printf(		"\t$(call DEPGEN_PCH_FILE)\n" );
#endif
		
		allDepGenFiles.AddToTail( depGenFile.Get() );

		compileRulesBuf.Printf( "$(PCH_OBJ_DIR)/$(PCH_PRE)%s.$(PCH_EXT): %s $(call DETECT_STRING_CHANGE_BETWEEN_BUILDS_TIMESTAMP_FILE,COMPILE_PCH_SETTINGS) | _precompile_steps",
                       pchIncludeUnqual,
                       pchIncludeRelative.Get() );
#if defined( ADD_DEPGEN_TARGETS )
		compileRulesBuf.Printf( " | %s", depGenFile.Get() );
#endif

        compileRulesBuf.Printf( "\n" );
		compileRulesBuf.Printf(		"\t$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) pch gen for %s start\n", pchIncludeUnqual );
		compileRulesBuf.PutString(	"\t$(QUIET_PREFIX) $(call SPEW_UPDATED_DEPENDENCIES)\n" );
		compileRulesBuf.Printf(		"\t$(QUIET_PREFIX) $(call RUN_PREBUILD_EVENT_ONCE)\n" );
        compileRulesBuf.Printf(		"\t$(call DO_PCH_GEN)\n" );
    }
	
	CUtlString vpcGeneratedScriptsBasePath;
	{
		CUtlPathStringHolder pathString;
		const char *szBasePath = g_pVPC->CreateGeneratedSubdirPath( &pathString, "generated_scripts" );
		
		vpcGeneratedScriptsBasePath.Format( "%s/%s", szBasePath, pConfig->m_Name.Get() );
		vpcGeneratedScriptsBasePath = vpcGeneratedScriptsBasePath.Replace( "\t", "_" );
		vpcGeneratedScriptsBasePath = vpcGeneratedScriptsBasePath.Replace( " ", "_" );

		V_FixDoubleSlashes( vpcGeneratedScriptsBasePath.Access() );
		V_FixSlashes( vpcGeneratedScriptsBasePath.Access(), '/' );
		
		createScriptSentinelDirectoriesBuf.Printf( "$(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/_vpc_create_dirs: %s | $(OBJ_DIR)/_create_dir\n", GetProjectGenerator()->GetOutputFileName() );
		createScriptSentinelDirectoriesBuf.PutString( "\t$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) create custom build tools dir start\n" );
		createScriptSentinelDirectoriesBuf.PutString( "\t$(QUIET_PREFIX) $(MKDIR) -p $(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/locks $(QUIET_ECHO_POSTFIX)\n" );
		createScriptSentinelDirectoriesBuf.PutString( "\t$(QUIET_PREFIX) $(MKDIR) -p $(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/ranthisbuild $(QUIET_ECHO_POSTFIX)\n" );
		createScriptSentinelDirectoriesBuf.PutString( "\t$(QUIET_PREFIX) $(MKDIR) -p $(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/sentinels $(QUIET_ECHO_POSTFIX)\n" );
		createScriptSentinelDirectoriesBuf.PutString( "\t$(QUIET_PREFIX) $(TOUCH) $(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/_vpc_create_dirs\n" );
	}

	m_CustomBuildTools.RemoveAll();
	DependencyTable_t customBuildToolDependencies, customBuildToolScripts, customBuildToolOutputs, depGenFiles;

	// Global custom build step
	{
		const char *szGlobalCustomBuildCommand = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_CUSTOMBUILDSTEP, g_pOption_CommandLine );
		if ( szGlobalCustomBuildCommand && szGlobalCustomBuildCommand[0] )
		{
			WriteCustomBuildTool( pConfig, customBuildToolsBuf, scriptType, NULL, "", vpcGeneratedScriptsBasePath.Get(), customBuildToolDependencies, customBuildToolScripts, customBuildToolOutputs );
		}
	}

	//individual file rules for custom build tools and generic compilation
	// outputs are left out of outBuf until we're done processing all the files
	for ( int i = 0; i < m_Files.Count(); i++ )
	{
		CProjectFile *pProjectFile = m_Files[i];
		if ( pProjectFile->IsExcludedFrom( pConfig->m_Name.Get() ) )
			continue;

		CUtlStringHolder<200> fixedFilename;
		// Make sure we always have a clean project-relative path.
		//  Subtle filename differences will confuse the hierarchy checks
		{
			char szAbsolutePath[MAX_PATH];
			char szRelativePath[MAX_PATH];
			V_MakeAbsolutePath( szAbsolutePath, ARRAYSIZE( szAbsolutePath ), pProjectFile->m_Name.String(), g_pVPC->GetProjectPath(), k_bVPCForceLowerCase );
			V_MakeRelativePath( szAbsolutePath, g_pVPC->GetProjectPath(), szRelativePath, sizeof( szRelativePath ) );
			V_RemoveDotSlashes( szRelativePath );
			V_FixSlashes( szRelativePath, '/' );
			fixedFilename.Set( szRelativePath );
		}

		if ( WriteCustomBuildTool( pConfig, customBuildToolsBuf, scriptType, pProjectFile, fixedFilename.Get(), vpcGeneratedScriptsBasePath.Get(), customBuildToolDependencies, customBuildToolScripts, customBuildToolOutputs ) )
		{
			//file wrote a custom build tool
			continue;
		}

		if ( CheckExtensions( fixedFilename.Get(), (const char**)sSourceFileExtensions ) )
        {
			CUtlStringBuilder additionalDependencies;
			CUtlStringBuilder additionalOrderOnlyDependencies;

			char sFullSourceFilename[MAX_BASE_FILENAME + 50];
			V_strncpy( sFullSourceFilename, fixedFilename.Get(), sizeof( sFullSourceFilename ) );

            char sObjFilename[MAX_BASE_FILENAME + 50];
            GetObjFilenameForFile( fixedFilename.Get(), sObjFilename, sizeof( sObjFilename ) );

            // Get the base obj filename for the .P file.
            char sPFile[MAX_BASE_FILENAME + 50];
            V_StripExtension( sObjFilename, sPFile, sizeof( sPFile ) );
			V_strncat( sPFile, ".P", sizeof( sPFile ) );

            bool bUsePch = pchInclude && pchInclude[0];
            // We assume that if the file has a different PCH setting from the
            // base PCH file that means it's disabling PCH for the file.
            if ( bUsePch &&
                 ( V_stristr_fast( m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_PrecompiledHeader, "" ), "Not Using" ) != NULL ||
                   V_stricmp_fast( pchInclude, m_pVCProjGenerator->GetPropertyValueAsString( pProjectFile, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_UsePCHThroughFile, "" ) ) != 0 ) )
            {
                bUsePch = false;
            }

			allDepGenFiles.AddToTail( sPFile );

#if defined( ADD_DEPGEN_TARGETS )
            // dependency generation rule
			{
				if ( compileRulesBuf.TellPut() != 0 )
				{
					depGenRulesBuf.PutString( "\n" );
				}

				additionalDependencies.Clear();
				additionalOrderOnlyDependencies.Clear();
				if ( bUsePch )
				{
					additionalDependencies.AppendFormat( " $(PCH_OBJ_DIR)/$(PCH_PRE)%s.$(PCH_EXT)", pchIncludeUnqual );
				}
				additionalDependencies.Append( " $(COMPILE_DEPENDANT_MAKEFILES)" );
				additionalOrderOnlyDependencies.Append( " _predepgen_steps" );
				WriteCompileRule( depGenRulesBuf, pProjectFile, pConfig, "DEPGEN_FILE",
								  sPFile, sFullSourceFilename,
								  additionalDependencies.Access(), additionalOrderOnlyDependencies.Access(),
								  bUsePch ? pchIncludeUnqual : nullptr );
			}
#endif
			depGenFiles.AddString( sPFile );

			// compilation rule
			{
				if ( compileRulesBuf.TellPut() != 0 )
				{
					compileRulesBuf.PutString( "\n" );
				}

				additionalDependencies.Clear();
				additionalOrderOnlyDependencies.Clear();
				if ( bUsePch )
				{
					additionalDependencies.AppendFormat( " $(PCH_OBJ_DIR)/$(PCH_PRE)%s.$(PCH_EXT)", pchIncludeUnqual ); 
				}
				additionalDependencies.Append( " $(call DETECT_STRING_CHANGE_BETWEEN_BUILDS_TIMESTAMP_FILE,COMPILE_O_GLOBAL_SETTINGS)" );
				//additionalDependencies.Append( " $(COMPILE_DEPENDANT_MAKEFILES)" );
#if defined( ADD_DEPGEN_TARGETS )
				additionalOrderOnlyDependencies.AppendFormat( " %s", sPFile );
#endif
				additionalOrderOnlyDependencies.Append( " _precompile_steps" );
				WriteCompileRule( compileRulesBuf, pProjectFile, pConfig, "COMPILE_FILE",
								  sObjFilename, sFullSourceFilename,
								  additionalDependencies.Access(), additionalOrderOnlyDependencies.Access(), 
								  bUsePch ? pchIncludeUnqual : nullptr );
			}
        }
	}


	{
		outBuf.PutString( "ALL_CUSTOM_BUILD_TOOLS =" );
		for ( int i = 0; i < m_CustomBuildTools.Count(); ++i )
		{
			outBuf.Printf( " \\\n" SPACE_TAB "$(OBJ_DIR)/_vpc_generated_scripts/custom_build_tools/sentinels/%s", m_CustomBuildTools[i].Get() );
		}
		outBuf.PutString( "\n\n" );
		outBuf.PutString( "unexport ALL_CUSTOM_BUILD_TOOLS\n" );
	}

	outBuf.PutString( "\n" );
	WriteDependencies( "ALL_CUSTOM_BUILD_TOOL_DEPENDENCIES", customBuildToolDependencies, outBuf );
	outBuf.PutString( "\n" );
	WriteDependencies( "ALL_CUSTOM_BUILD_TOOL_SCRIPTS", customBuildToolScripts, outBuf );
	outBuf.PutString( "\n" );
	WriteDependencies( "ALL_CUSTOM_BUILD_TOOL_OUTPUTS", customBuildToolOutputs, outBuf );
	outBuf.PutString( "\n" );
	WriteDependencies( "ALL_DEPGEN_FILES", depGenFiles, outBuf );	

	// Include the base makefile before the rules to build the .o files.
    // NOTE: This should only be included after setting all variables
    // that the base makefile will look at.
	outBuf.PutString(	"\n\n"
					"#\n"
					"# Include the base makefile now.\n"
					"#\n" );
	if ( g_pVPC->IsConditionalDefined( "POSIX" ) )
	{
		outBuf.Printf( "include $(MAKEFILE_BASE)\n" );
	}
	
	// Create script sentinel directories
	if ( createScriptSentinelDirectoriesBuf.TellPut() != 0 )
	{
		outBuf.PutString(	"\n\n"
							"#\n"
							"# Create sentinel directories\n"
							"#\n" );
		outBuf.Put( createScriptSentinelDirectoriesBuf.Base(), createScriptSentinelDirectoriesBuf.TellPut() );
	}

	// Pre-Build event
	{
		const char *szPreBuildEvent = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_PREBUILDEVENT, g_pOption_CommandLine );
		if ( szPreBuildEvent && szPreBuildEvent[0] )
		{
			CUtlString scriptFileName;
			scriptFileName.Format( "%s/build_events/pre_build_event%s", vpcGeneratedScriptsBasePath.Get(), s_ScriptHelpers[scriptType].szExtension );
	
			outBuf.PutString(	"\n\n"
								"#\n"
								"# Pre-Build event\n"
								"#  This is phrased as a macro because it's harder to run the step before anything compiles while also detecting that something needs to compile after it without introducing circular dependencies.\n"
								"#\n" );

			outBuf.Printf( "PREBUILD_EVENT_ACTION = { $(SHELL) %s $(PROCESS_COMPILER_OUTPUT); }\n", scriptFileName.Get() );
			outBuf.PutString( "unexport PREBUILD_EVENT_ACTION\n" );
						
			CBaseProjectDataCollector::DoStandardVisualStudioReplacements( szPreBuildEvent, pVsStr, "" );
			WriteScriptFile( scriptFileName, pVsStr, scriptType );
		}
	}

	// Pre-Link event
	{
		const char *szPreLinkEvent = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_PRELINKEVENT, g_pOption_CommandLine );
		if ( szPreLinkEvent && szPreLinkEvent[0] )
		{
			outBuf.PutString(	"\n\n"
								"#\n"
								"# Pre-Link event\n"
								"#\n" );

			CUtlString scriptFileName;
			scriptFileName.Format( "%s/build_events/pre_link_event%s", vpcGeneratedScriptsBasePath.Get(), s_ScriptHelpers[scriptType].szExtension );
			
			CBaseProjectDataCollector::DoStandardVisualStudioReplacements( szPreLinkEvent, pVsStr, "" );
			WriteScriptFile( scriptFileName, pVsStr, scriptType );

			outBuf.Printf( "PRELINK_EVENT_ACTION = { $(SHELL) %s $(PROCESS_COMPILER_OUTPUT); }\n", scriptFileName.Get() );
			outBuf.PutString( "unexport PRELINK_EVENT_ACTION\n" );
		}
	}


	// Post-Build event
	{
		const char *szPostBuildEvent = m_pVCProjGenerator->GetPropertyValueAsString( NULL, pConfig->m_Name.Get(), KEYWORD_POSTBUILDEVENT, g_pOption_CommandLine );
		if ( szPostBuildEvent && szPostBuildEvent[0] )
		{
			outBuf.PutString(	"\n\n"
								"#\n"
								"# Post-Build event\n"
								"#\n" );

			CFmtStr scriptFileName;
			scriptFileName.Format( "%s/build_events/post_build_event%s", vpcGeneratedScriptsBasePath.Get(), s_ScriptHelpers[scriptType].szExtension );
			
			CBaseProjectDataCollector::DoStandardVisualStudioReplacements( szPostBuildEvent, pVsStr, "" );
			WriteScriptFile( scriptFileName, pVsStr, scriptType );
			
			outBuf.Printf( "POSTBUILD_EVENT_ACTION = { $(SHELL) %s $(PROCESS_COMPILER_OUTPUT); }\n", scriptFileName.Get() );
			outBuf.PutString( "unexport POSTBUILD_EVENT_ACTION\n" );
		}
	}

	// Custom build tools
	{
		if ( customBuildToolsBuf.TellPut() != 0 )
		{
			outBuf.PutString( "\n\n" );
			outBuf.Put( customBuildToolsBuf.Base(), customBuildToolsBuf.TellPut() );
		}
	}

	if ( depGenRulesBuf.TellPut() != 0 )
	{
		outBuf.PutString(	"\n\n"
							"#\n"
							"# Depgen rules.\n"
							"#\n" );

		outBuf.Put( depGenRulesBuf.Base(), depGenRulesBuf.TellPut() );
	}

	// include all the depgen .P files which will include dependency information.
	if ( allDepGenFiles.Count() > 0 )
	{
		outBuf.PutString(	"\n\n"
							"#\n"
							"# include all the depgen .P files which will include dependency information.\n"
							"#\n" );

		for ( int i = 0; i < allDepGenFiles.Count(); ++i )
		{
			outBuf.Printf( "include $(wildcard %s)\n", allDepGenFiles[i].Get() );
		}
	}

	outBuf.PutString(	"\n\n"
						"#\n"
						"# Compare/cache a string of compile settings to see if we need to recompile .o's\n"
						"#\n"
						"$(eval $(call DETECT_STRING_CHANGE_BETWEEN_BUILDS,COMPILE_O_GLOBAL_SETTINGS,$$(call COMPILE_O_SETTINGS,$$(CFG_CPPFLAGS))))\n" );


	if ( pchInclude && pchInclude[0] )
	{
		outBuf.PutString(	"\n\n"
							"#\n"
							"# Compare/cache a string of compile settings to see if we need to recompile the PCH\n"
							"#\n"
							"$(eval $(call DETECT_STRING_CHANGE_BETWEEN_BUILDS,COMPILE_PCH_SETTINGS,$$(COMPILE_PCH_SETTINGS)))\n" );
	}

	if ( compileRulesBuf.TellPut() != 0 )
	{
		outBuf.PutString(	"\n\n"
							"#\n"
							"# Compile rules\n"
							"#\n" );

		outBuf.Put( compileRulesBuf.Base(), compileRulesBuf.TellPut() );
	}

    outBuf.Printf( "\nendif # (CFG=%s)\n", pConfig->m_LowerCaseName.Get() );
}

void CProjectGenerator_Makefile::WriteCompileRule( CUtlBuffer &outBuf,
		CProjectFile *pProjectFile, CProjectConfiguration *pConfig,
		const char *pCompileFunction, const char *pTarget, const char *pSource, const char *pAdditionalDeps, const char *pOrderOnlyDeps,
		const char *pPchInclude )
{
	outBuf.Printf( "%s: %s", pTarget, pSource );

	if ( pAdditionalDeps )
	{
		outBuf.Printf( "%s", pAdditionalDeps );
	}

	if ( pOrderOnlyDeps )
	{
		outBuf.Printf( " |%s", pOrderOnlyDeps );
	}

	outBuf.Printf( "\n" );

	outBuf.Printf( "\t$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) %s compile start for file %s\n", pCompileFunction, pTarget );
	outBuf.PutString( "\t$(QUIET_PREFIX) $(call SPEW_UPDATED_DEPENDENCIES)\n" );
	outBuf.Printf( "\t$(QUIET_PREFIX) $(call RUN_PREBUILD_EVENT_ONCE)\n" );
	outBuf.Printf( "\t$(PRE_%s)\n", pCompileFunction );
	outBuf.Printf( "\t$(QUIET_PREFIX) $(MKDIR) -p \"$(dir $@)\"\n" );
	outBuf.Printf( "\t$(call %s,", pCompileFunction );

	if ( pPchInclude )
	{
		outBuf.Printf( "$(call PCH_FLAGS,$(PCH_OBJ_DIR),$(PCH_PRE)%s)", pPchInclude );
	}

	// If this file has per-file includes or defines we will emit them directly,
	// otherwise we'll refer to the base configuration flags.
	if ( m_pVCProjGenerator->HasFilePropertyValue( pProjectFile, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_PreprocessorDefinitions ) ||
		 m_pVCProjGenerator->HasFilePropertyValue( pProjectFile, pConfig->m_Name.Get(), KEYWORD_COMPILER, g_pOption_AdditionalIncludeDirectories ) )
	{
		outBuf.PutString( " " );
		WriteDefines( pConfig, outBuf, pProjectFile );
		outBuf.PutString( " $(addprefix -I, $(abspath " );
		WriteIncludes( pConfig, outBuf, pProjectFile );
		outBuf.PutString( "))" );
	}
	else
	{
		outBuf.PutString( " $(CFG_CPPFLAGS)" );
	}
	
	outBuf.Printf( ") $(POST_%s)\n", pCompileFunction );	
	outBuf.Printf( "\t$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) %s compile end for file %s\n", pCompileFunction, pTarget );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CProjectGenerator_Makefile::WriteDependencies( const char *szVarName, DependencyTable_t &otherDependencies, CUtlBuffer &outBuf )
{
	outBuf.Printf( "%s =", szVarName );
	for ( int i = 0; i < otherDependencies.GetNumStrings(); i++ )
	{
		outBuf.Printf( " \\\n" SPACE_TAB "%s", UsePOSIXSlashes( otherDependencies.GetElementString( i ) ) );
	}
	outBuf.PutString( "\n\n" );
	outBuf.Printf( "unexport %s\n", szVarName );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CProjectGenerator_Makefile::WriteMakefile( const char *pFilename )
{
    CUtlBuffer outBuf;
    outBuf.SetBufferType( true, false );

	ScriptType_t scriptType = ST_SH;
	/*if ( g_pVPC->ResolveConditionalSymbol( "$HOST_WINDOWS" ) )
	{
		scriptType = ST_BAT;
	}*/

	// Write all the non-config-specific stuff.
	WriteNonConfigSpecificStuff( outBuf );

    // Write each config out.
    for ( int i = 0; i < m_RootConfigurations.Count(); i++ )
    {
        CProjectConfiguration *pConfig = m_RootConfigurations[i];
        WriteConfigSpecificStuff( pConfig, outBuf, scriptType );
	}

    bool bUpToDate = !g_pVPC->IsForceGenerate() && !Sys_FileChanged( pFilename, outBuf, true );

    if ( !bUpToDate )
    {
        Sys_WriteFile( pFilename, outBuf, true );
        Sys_CopyToMirror( pFilename );
    }

	return true;
}

bool CProjectGenerator_Makefile::WriteScriptFile( const char *pFileNameParam, CUtlStringBuilder *pContents, ScriptType_t scriptType )
{
	if ( s_ScriptHelpers[scriptType].pFN_ScriptConversion )
	{
		s_ScriptHelpers[scriptType].pFN_ScriptConversion( pContents );
	}
		
	CUtlBuffer scriptFile( 0, 0, CUtlBuffer::TEXT_BUFFER );
	scriptFile.PutString( s_ScriptHelpers[scriptType].szContentsPreamble );
	scriptFile.PutString( pContents->String() );

	CFmtStrMax scriptFileAbsolutePath;
	V_MakeAbsolutePath( scriptFileAbsolutePath.Access(), scriptFileAbsolutePath.GetMaxLength(), pFileNameParam, nullptr, k_bVPCForceLowerCase );

	//create the path
	{
		char *pFileName = (char *)V_UnqualifiedFileName( scriptFileAbsolutePath.Get() );
		if ( Verify( pFileName != scriptFileAbsolutePath.Get() ) )
		{
			char cBackup = pFileName[-1];
			pFileName[-1] = '\0';
			Sys_CreatePath( scriptFileAbsolutePath.Get() );
			pFileName[-1] = cBackup;
		}
	}
	
	return Sys_WriteFileIfChanged( scriptFileAbsolutePath.Get(), scriptFile, true );
}
	
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CProjectGenerator_Makefile::Save( const char *pOutputFilename )
{
	m_pVCProjGenerator->GetAllProjectFiles( m_Files );
	m_pVCProjGenerator->GetAllRootConfigurations( m_RootConfigurations );

	bool bValid = WriteMakefile( pOutputFilename );
	if ( !bValid )
	{
		g_pVPC->VPCWarning( "Error saving makefile: %s", pOutputFilename );
	}

	return bValid;
}

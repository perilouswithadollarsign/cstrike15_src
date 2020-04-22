//========= Copyright 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: Schema Script Generation
//
//=====================================================================================//

#include "vpc.h"
#include "tier1/utldict.h"
#include "tier1/keyvalues.h"
#include "baseprojectdatacollector.h"
#include "tier1/fmtstr.h"
#include "../../../public/schemasystem/schemaversionnumbers.h" // for SCHEMA_SCHPROJ_VERSION

//--------------------------------------------------------------------------------------------------
struct SchemaUnityBatch_t
{
	CUtlString m_BatchFileName;
	CUtlVector<CUtlString> m_BatchFiles;
};

struct SchemaContext_t
{
	CUtlString m_platformName;
	CUtlString m_compilerName;
	CUtlStringBuilder m_schemaCompilerExePath;
	CUtlStringBuilder m_schemaCompilerExeDependent;
	CUtlPathStringHolder m_uniqueProjectName;
	CUtlStringBuilder m_projectGenFolder;
	CUtlPathStringHolder m_schprojFilename;
	char m_pProjectCodeFolder[MAX_FIXED_PATH];
	char m_pSrcDirAbsPath[MAX_FIXED_PATH];
	CUtlStringBuilder m_globalConfigFolder;
	CUtlStringBuilder m_unityFilenameBase;
	char m_pUnityFilenameTail[50];
	bool m_bHeaderUnityBuild;
	int m_nUnityBatchSize;
};


struct SchemaFileInfo_t
{
	bool m_bIsCppFile;
	CUtlString m_inputFile;
    CUtlPathStringHolder m_noExt;
	char m_pBase[MAX_BASE_FILENAME];
	CUtlPathStringHolder m_path;
	char m_pExt[MAX_BASE_FILENAME];
	char m_pSymbolName[MAX_BASE_FILENAME];
	CUtlString m_pchName;
	CUtlString m_preIncludeFiles;
	bool m_bPerformSchemaCompile;
};

KeyValues *ConfigPreprocessorSettingsAsKV( CVCProjGenerator *pDataCollector, CProjectConfiguration *pRootConfig, CUtlVector<SchemaFileInfo_t> &schemaFileInfos );
void EmitSchprojFile( CVCProjGenerator *pDataCollector, const SchemaContext_t &ctx, CUtlVector<SchemaFileInfo_t> &schemaFileInfos );
void AddUnitySchemaFilesForConfig( const SchemaContext_t &ctx, CUtlVector<SchemaFileInfo_t> &schemaFileInfos, CVCProjGenerator *pDataCollector, const char *pConfigName, CUtlVector< SchemaUnityBatch_t > *pOutUnityBatches );
void BuildSchemaFileInfo( SchemaFileInfo_t *pOutInfo, const SchemaContext_t &ctx, const char *pFilename, CVCProjGenerator *pDataCollector, bool bPerformSchemaCompile );
void CollectAllSchemaRelatedfiles( CVCProjGenerator *pDataCollector, CUtlVector< CProjectFile * > &outputVector );
void CollectSchemaIncludeFiles( const CUtlVector< CProjectFile * > &allSchemaRelatedFiles, CUtlVector< CUtlString > &outputVector );

CUtlString SchemaGeneratedFilePath( SchemaFileInfo_t &file, const SchemaContext_t &ctx, const char *szConfigurationString );
CUtlString SchemaGeneratedFileFolder( const SchemaContext_t &ctx, const char *szConfigurationString );

#define SCHEMA_CMD_FILE_NAME "schemacompiler"
#define SCHEMA_PRE_INCLUDE_FILES_PROPERTY "$SchemaPreIncludeFiles"

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CSchemaVPC
{
public:
	CSchemaVPC() { }
	~CSchemaVPC() { m_Configs.PurgeAndDeleteElements(); }

	void Init( CVCProjGenerator *pDataCollector, const CUtlVector< CUtlStringCI > &fileList, const CUtlVector< CProjectConfiguration * > &configList, const char *pSchprojSuffix );

	void AddCustomBuildStep();
	void EmitSchproj( bool bTouchUnchangedOutputs );
	void AddFilesToProject();
	void AddFilesToVPCBuffer( CUtlBuffer *pOutVPCBuffer, const char *pFolderName, bool bSingleConfig, bool bUseSchprojBuildStep, bool bSchprojBuildStepShouldRegistersOutputs );

private:
	struct CConfig
	{
		CUtlString m_CasedConfigName; // ie. Debug / Release
		CUtlString m_LowerCaseConfigName; // ie. debug / release
		CProjectConfiguration *m_pRootConfig;
		CUtlVector<CUtlString> m_AllPreincludeFiles;
		CUtlVector<SchemaFileInfo_t> m_Files;
		CUtlVector< SchemaUnityBatch_t > m_UnityBatches;
		CUtlString m_AnchorPath;
	};

	void AddConfig( const CUtlVector< CUtlStringCI > &fileList, const CUtlVector< CProjectFile * > &relevantProjectFiles, CProjectConfiguration *pRootConfig );
	void AddUnityFiles();
	void BuildSchemaContext( CVCProjGenerator *pDataCollector, SchemaContext_t *pCtx );
	void CreateSchemaCustomBuild( CConfig *pCfg, CProjectConfiguration *pRootConfiguration, CVCProjGenerator *pVCProjGenerator, const SchemaContext_t &ctx, CUtlVector<SchemaFileInfo_t> &schemaFileInfos );
	void GetCustomBuildStrings( bool bIsGlobalBuildStep, CConfig *pCfg, const SchemaContext_t &ctx, CUtlVector<SchemaFileInfo_t> &schemaFileInfos, CUtlString &description, CUtlString &cmdLine, CUtlString &outputList, CUtlString &potentialOutputList, CUtlString &additionalDependencies, CUtlString &orderOnlyFileDependencies );
	void CollectSchemaPreincludeFilesForConfig( CConfig *pConfig, const CUtlVector< CProjectFile * > &relevantProjectFiles, CProjectConfiguration *pRootConfig );

	CVCProjGenerator *m_pDataCollector;
	CUtlVector<CConfig*> m_Configs;
	SchemaContext_t m_Context;
	CUtlString m_SchprojSuffix;
	CUtlVector<CUtlString> m_IncludeFiles;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CSchemaVPC::Init( CVCProjGenerator *pDataCollector, const CUtlVector< CUtlStringCI > &fileList, const CUtlVector< CProjectConfiguration * > &configList, const char *pSchprojSuffix )
{
	m_pDataCollector = pDataCollector;
	m_SchprojSuffix = pSchprojSuffix;

	//collect all files with flags VPC_FILE_FLAGS_SCHEMA_INCLUDE or VPC_FILE_FLAGS_SCHEMA
	CUtlVector< CProjectFile * > allRelevantFiles;
	CollectAllSchemaRelatedfiles( pDataCollector, allRelevantFiles );

	//Collect all the $SchemaIncludeFile files
	CollectSchemaIncludeFiles( allRelevantFiles, m_IncludeFiles );

	BuildSchemaContext( m_pDataCollector, &m_Context );

	for ( int i = 0; i < configList.Count(); ++i )
	{
		AddConfig( fileList, allRelevantFiles, configList[i] );
	}

	AddUnityFiles();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CSchemaVPC::AddConfig( const CUtlVector< CUtlStringCI > &fileList, const CUtlVector< CProjectFile * > &relevantProjectFiles, CProjectConfiguration *pRootConfig )
{
	CConfig *pConfig = new CConfig();
	m_Configs.AddToTail( pConfig );

	pConfig->m_pRootConfig = pRootConfig;
	pConfig->m_CasedConfigName = pConfig->m_pRootConfig->m_Name;
	pConfig->m_LowerCaseConfigName = pConfig->m_pRootConfig->m_LowerCaseName;

	pConfig->m_AnchorPath.Format( "%s/%s%s." GENERATED_CPP_FILE_EXTENSION, SchemaGeneratedFileFolder(m_Context,pConfig->m_LowerCaseConfigName.Get()).Get(), m_Context.m_uniqueProjectName.Get(), g_SchemaAnchorBase );

	//Collect all the $SchemaPreIncludeFiles
	CollectSchemaPreincludeFilesForConfig( pConfig, relevantProjectFiles, pRootConfig );

	int nNumFiles = fileList.Count();
	pConfig->m_Files.SetCount( nNumFiles );
	for ( int i = 0; i < nNumFiles; ++i )
	{
		BuildSchemaFileInfo( &pConfig->m_Files[i], m_Context, fileList[i].Get(), m_pDataCollector, true );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CSchemaVPC::CollectSchemaPreincludeFilesForConfig( CConfig *pConfig, const CUtlVector< CProjectFile * > &relevantProjectFiles, CProjectConfiguration *pRootConfig )
{
	//Collect all the $SchemaPreIncludeFiles
	//need to loop all schema files since any of them can technically modify their own version of $SchemaPreIncludeFiles
	for ( int i = 0; i < relevantProjectFiles.Count(); ++i )
	{
		CProjectFile *pFile = relevantProjectFiles[i];
		if ( (pFile->m_iFlags & (VPC_FILE_FLAGS_SCHEMA | VPC_FILE_FLAGS_SCHEMA_INCLUDE)) == 0 )
			continue;
			
		//the file is a schema file

		//get the $SchemaPreIncludeFiles property for this file
		CUtlString preIncludeFiles;
		CProjectConfiguration *pFileConfig = nullptr;
		pFile->GetConfiguration( pRootConfig->m_Name, &pFileConfig );
		if ( !VPC_GetPropertyString( KEYWORD_GENERAL, pRootConfig, pFileConfig, SCHEMA_PRE_INCLUDE_FILES_PROPERTY, &preIncludeFiles ) )
			continue; //couldn't find the $SchemaPreIncludeFiles general property
				
		//split the semicolon separated filenames
		CUtlStringList fileList;
		V_SplitString( preIncludeFiles.Get(), ";", fileList );
		
		for ( int j = 0; j < fileList.Count(); ++j )
		{
			char *szFileName = fileList[j];
			
			//do a leading/trailing whitespace cleanup pass
			V_StripLeadingWhitespace( szFileName );
			V_StripTrailingWhitespace( szFileName );

			if ( szFileName[0] == '\0' )
				continue; //nothing left

			CUtlString asUtlString = szFileName;

			//test for dupes
			if ( pConfig->m_AllPreincludeFiles.IsValidIndex( pConfig->m_AllPreincludeFiles.Find( asUtlString ) ) )
				continue; //duplicate, drop it

			//not a dupe, add it
			pConfig->m_AllPreincludeFiles.AddToTail( asUtlString );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CSchemaVPC::AddCustomBuildStep()
{
	// add the custom build step to the project
	for ( int iConfig = 0; iConfig < m_Configs.Count(); ++iConfig )
	{
		CConfig *pCfg = m_Configs[iConfig];
		CreateSchemaCustomBuild( pCfg, pCfg->m_pRootConfig, m_pDataCollector, m_Context, pCfg->m_Files );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CSchemaVPC::AddUnityFiles()
{
	// emit the schema unity files
	for ( int iConfig = 0; iConfig < m_Configs.Count(); ++iConfig )
	{
		CConfig *pCfg = m_Configs[iConfig];
		AddUnitySchemaFilesForConfig( m_Context, pCfg->m_Files, m_pDataCollector, pCfg->m_LowerCaseConfigName.Get(), &pCfg->m_UnityBatches );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void DoNormalSchema( CVCProjGenerator *pDataCollector, const CUtlVector< CUtlStringCI > &fileList, CUtlBuffer *pOutVPCBuffer )
{
	if ( fileList.Count() == 0 )
	{
		// no Schema
		return;
	}

	CUtlVector<CProjectConfiguration *> rootConfigs;
	pDataCollector->GetAllRootConfigurations( rootConfigs );

	bool bStrictDependencies = g_pVPC->IsConditionalDefined( "SchemaCompileUsesStrictDependencies" );

	//If we want to avoid building when all our output contents are up to date then we have 2 options:
	//	1. Touch every output any time any of the inputs changes. This will cause all the outputs to run through the C++ compiler. 
	//		Potentially very long for game projects with hundreds of outputs
	//	2. Avoid officially listing the C++ outputs and ensure that our C++ outputs are correct by the time the C++ compiler evaluates them
	//		This is possible in MSVC because custom build tools are guaranteed to run before the C++ compiler step
	//		For Xcode we emit all custom build tools as buildPhases that run before the "Sources" phase (containing general C++ files)
	//		For Makefiles we add an order-only dependency on all the custom build tools to each C++ file to ensure ordering
	const bool bStrictOutputs = false;

	CSchemaVPC cfg;
	cfg.Init( pDataCollector, fileList, rootConfigs, "" );
	if ( !bStrictDependencies )
	{
		//the global build step does not use strict dependencies
		cfg.AddCustomBuildStep();
	}
	cfg.EmitSchproj( bStrictOutputs );
	cfg.AddFilesToVPCBuffer( pOutVPCBuffer, "Generated Files", false, bStrictDependencies, bStrictOutputs );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void BeginVPCBuffer( CUtlBuffer *pOutVPCBuffer )
{
	// NOTE! ($Last) is a special keyword that tells VPC to place this folder last in the list. This
	// is done because there's a very strange link error that occurs when building the hammer project
	// (two _Dllmains are defined) when this folder is first in the list. I was not able to determine
	// why this was occurring, so this is the hack solution.
	pOutVPCBuffer->Printf( "$Folder \"($Last)%s\" \n", g_SchemaFolderName );
	pOutVPCBuffer->Printf( "{\n" );
	pOutVPCBuffer->PushTab();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void EndVPCBuffer( CUtlBuffer *pOutVPCBuffer )
{
	pOutVPCBuffer->PopTab();
	pOutVPCBuffer->Printf( "}\n" );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void InjectVPCBuffer( const CUtlBuffer &vpcBuffer )
{
	// save parser
	bool bIgnoreRedundancyWarning = g_pVPC->IsIgnoreRedundancyWarning();
	g_pVPC->SetIgnoreRedundancyWarning( true );
	g_pVPC->GetScript().PushScript( "Internal List [Schema]", (char*)vpcBuffer.Base(), 1, false, false );

	const char *pToken = g_pVPC->GetScript().GetToken( true );
	NOTE_UNUSED( pToken );
	Assert( pToken && pToken[0] && !V_stricmp_fast( pToken, "$folder" ) );
	VPC_Keyword_Folder( VPC_FOLDER_FLAGS_DYNAMIC );

	// restore parser
	g_pVPC->GetScript().PopScript();
	g_pVPC->SetIgnoreRedundancyWarning( bIgnoreRedundancyWarning );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// This is called when it's done parsing a project. If there were any $SchemaFile
// entries in the project, then we will make this project depend on schemacompiler and schemacompiler_parent
// (via $AdditionalProjectDependencies).
//
// This fixes full rebuild problems where it's building a project that uses schemacompiler
// at the same time as it's building schemacompiler. This usually screws up when 
// it tries to copy the new schemacompiler.exe to game\bin but it's in use.
void VPC_Schema_ForceAdditionalDependencies( const char *pProjectName )
{
	// NOTE: we do not early-out here during dependency-generation (that code depends upon these script modifications)
	if ( !g_pVPC->m_SchemaFiles.Count() )
		return;


	// Add "$BASE;schemacompiler;schemacompiler_parent" to $AdditionalProjectDependencies.
	bool bShouldSkip;
	CUtlVector< CUtlString > configurationNames;
	g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
	for ( int i=0; i < configurationNames.Count(); i++ )
	{
		g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), false );
		g_pVPC->GetProjectGenerator()->StartPropertySection( KEYWORD_GENERAL, &bShouldSkip );

		g_pVPC->GetProjectGenerator()->HandleProperty( g_pOption_AdditionalProjectDependencies, "$BASE;schemacompiler;schemacompiler_parent" );

		g_pVPC->GetProjectGenerator()->EndPropertySection( KEYWORD_GENERAL );
		g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Schema_TrackFile( const char *pName, bool bRemove, VpcFileFlags_t iFileFlags )
{
#ifdef STEAM
	return;
#else

	CUtlVector<CUtlStringCI> *pList = &g_pVPC->m_SchemaFiles;

	if ( !bRemove )
	{
		// Ignore files without the SCHEMA flag
		if ( !( iFileFlags & VPC_FILE_FLAGS_SCHEMA ) )
			return;

		// Ignore if this script opts out of the Schema feature
		if ( g_pVPC->IsConditionalDefined( "NOSCHEMACOMPILER" ) )
		{
			g_pVPC->VPCError( "ERROR: Schema file '%s' in project '%s' that specifies NOSCHEMACOMPILER!", pName, g_pVPC->GetProjectName() );
			return;
		}

		// Ignore if the Schema feature is disabled
		if ( !g_pVPC->IsSchemaEnabled() )
			return;

		// Add the file (if not already added
		if ( !pList->HasElement( pName ) )
			pList->AddToTail( pName );
	}
	else
	{
		// remove from the list
		pList->FindAndRemove( pName );
		return;	
	}


	// Early-out during dependency-generation (let that code see the original, unmodified contents of the VPC)
	if ( g_pVPC->m_bIsDependencyPass )
		return;

	// suppress building of schematized cpp files
	// (they get #included by an auto-generated cpp that is built instead)
	const char *pExt = V_GetFileExtension( pName );
	if ( IsCFileExtension( pExt ) )
	{
		CUtlVector< CUtlString > configurationNames;
 		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
		for ( int i=0; i < configurationNames.Count(); i++ )
		{
			g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), true );
			g_pVPC->GetProjectGenerator()->FileExcludedFromBuild( true );
			g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
		}
	}

#endif
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CProjectFile * VPC_Schema_GetGeneratedFile( CProjectFile *pInputFile, const char *pConfigName, CVCProjGenerator *pDataCollector )
{
	CProjectFile *pResult = NULL;
    CUtlStringBuilder* pStrBuf = g_pVPC->GetTempStringBuffer1();
    pStrBuf->Set( pInputFile->m_Name.Get() );
	V_FixSlashes( pStrBuf->Access(), '/' );
	UtlSymId_t index = g_pVPC->m_SchemaOutputFileMap.Find( pStrBuf->Get() );
	if ( index == UTL_INVAL_SYMBOL )
	{
		pStrBuf->AppendFormat( ":%s", pConfigName );
		index = g_pVPC->m_SchemaOutputFileMap.Find( pStrBuf->Get() );
	}
	if ( index != UTL_INVAL_SYMBOL )
	{
		CUtlString generatedFilename = g_pVPC->m_SchemaOutputFileMap[ index ];
		generatedFilename.FixSlashes();
		pDataCollector->FindFile( generatedFilename.Get(), &pResult );
	}
	//if ( !pResult )
	//	g_pVPC->VPCWarning( "VPC_Schema_GetGeneratedFile: could not find generated file for '%s' (%s)", pInputFile->m_Name.Get(), pConfigName );
	return pResult;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void VPC_Schema_OnParseProjectStart( void )
{
	g_pVPC->m_SchemaFiles.Purge();
	g_pVPC->m_SchemaOutputFileMap.Clear();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
// TODO: Currently schema only works for vcproj's
void VPC_Schema_OnParseProjectEnd( CVCProjGenerator *pDataCollector )
{
	if ( !g_pVPC->m_SchemaFiles.Count() )
		return;

	// Early-out during dependency-generation (let that code see the original, unmodified contents of the VPC)
	if ( g_pVPC->m_bIsDependencyPass )
		return;


	//--------------------------------------------------------------------------------------------------
	// start the VPC script
	CUtlBuffer vpcBuffer;
	vpcBuffer.SetBufferType( true, true );
	BeginVPCBuffer( &vpcBuffer );

	//--------------------------------------------------------------------------------------------------
	// Handle the schemas
	DoNormalSchema( pDataCollector, g_pVPC->m_SchemaFiles, &vpcBuffer );

	//--------------------------------------------------------------------------------------------------
	// inject the generated VPC script
	EndVPCBuffer( &vpcBuffer );
	InjectVPCBuffer( vpcBuffer );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void RemoveSubstring_CaseInsensitive( char *pString, const char *pSubString )
{
	char *pFound = V_stristr( pString, pSubString );
	if ( pFound )
	{
		// bury the substring under the remainder of the string.
		int nSubstringLength = V_strlen( pSubString );
		int nRemainingLength = V_strlen( pFound );
		V_memmove( pFound, pFound + nSubstringLength, (nRemainingLength - nSubstringLength) + 1 ); //+1 to copy the null terminator
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CSchemaVPC::BuildSchemaContext( CVCProjGenerator *pDataCollector, SchemaContext_t *pCtx )
{
	// eg. win32
	pCtx->m_platformName = g_pVPC->GetTargetPlatformName();
	pCtx->m_platformName.ToLower();

	pCtx->m_compilerName = g_pVPC->GetTargetCompilerName();
	pCtx->m_compilerName.ToLower();

	V_GetCurrentDirectory( pCtx->m_pProjectCodeFolder, sizeof( pCtx->m_pProjectCodeFolder ) );
	V_FixSlashes( pCtx->m_pProjectCodeFolder, '/' );


	// SRCDIR
    CUtlStringBuilder *pStrBuf = g_pVPC->GetMacroReplaceBuffer();
	g_pVPC->ResolveMacrosInString( "$SRCDIR", pStrBuf );
	V_MakeAbsolutePath( pCtx->m_pSrcDirAbsPath, sizeof(pCtx->m_pSrcDirAbsPath), pStrBuf->Get(), NULL, k_bVPCForceLowerCase );
	V_FixSlashes( pCtx->m_pSrcDirAbsPath, '/' );

	// this will be 'projectname' or 'server_gamename'
    pCtx->m_uniqueProjectName.Set( g_pVPC->GetProjectGenerator()->GetOutputFileName() );
	pCtx->m_uniqueProjectName.StripExtension();

	// TODO: This is ultra-fragile! Instead, we should use a 'ComposeProjectName' helper that adds just the desired suffices
	//       (it would take N bools/enums, such that adding a new suffix will force all existing callers to be updated)
	{
		//strip the platform out of the unique name, because we're turning it into a subdirectory difference
		RemoveSubstring_CaseInsensitive( pCtx->m_uniqueProjectName.GetForModify(), CFmtStr( "_%s", pCtx->m_platformName.Get() ) );
		// Also strip the '_unity' decorator, since individual gen_cpp files do not depend on that
		// [TODO: unify the schema-specific unity files with the new general-purpose VPC unity feature]
		RemoveSubstring_CaseInsensitive( pCtx->m_uniqueProjectName.GetForModify(), "_unity" );
	}


	// Is this a schema unity build?						
	const char *pVal = g_pVPC->GetMacroValue( "SCHEMA_UNITY_H_BUILD" );
	pCtx->m_bHeaderUnityBuild = pVal ? ( atoi( pVal ) != 0 ) : false;
	if ( g_pVPC->IsProjectUsingUnity() )
	{
		// TODO: The new unity feature supports schema files, so no need to do it here too. Once the new
		//       unity feature is more widely deployed/tested, the schema-specific unity code can be deleted.
		pCtx->m_bHeaderUnityBuild = false;
	}


	const char *pBatchSizeVal = g_pVPC->GetMacroValue( "SCHEMACOMPILER_UNITY_BATCH_SIZE" );
	pCtx->m_nUnityBatchSize = pBatchSizeVal ? atoi( pBatchSizeVal ) : -1;
	if ( pCtx->m_bHeaderUnityBuild && pCtx->m_nUnityBatchSize <= 0 )
	{
		g_pVPC->VPCError( "VPC could not determine schema unity batch size (missing SCHEMACOMPILER_UNITY_BATCH_SIZE macro?)\n" );
	}

	// path to schemacompiler 
	g_pVPC->ResolveMacrosInString( "$SCHEMACOMPILER_EXE_PATH", &pCtx->m_schemaCompilerExePath );
	V_FixSlashes( pCtx->m_schemaCompilerExePath.Access() );
	g_pVPC->ResolveMacrosInString( "$SCHEMACOMPILER_EXE_DEPENDENT", &pCtx->m_schemaCompilerExeDependent );
	V_FixSlashes( pCtx->m_schemaCompilerExeDependent.Access() );

	// schema unity filename base
	g_pVPC->ResolveMacrosInString( "$SCHEMACOMPILER_UNITY_FILENAME_BASE", &pCtx->m_unityFilenameBase );
	V_FixSlashes( pCtx->m_unityFilenameBase.Access() );

	// schema unity filename tail
	V_strcpy_safe( pCtx->m_pUnityFilenameTail, "." GENERATED_CPP_FILE_EXTENSION );
	
	// global root folder for all generated code
	g_pVPC->ResolveMacrosInString( "$SCHEMACOMPILER_GENERATED_CODE_PATH", &pCtx->m_projectGenFolder );

	// root folder for generated code for this project
    pCtx->m_projectGenFolder.AppendChar( '/' );
    pCtx->m_projectGenFolder.Append( pCtx->m_uniqueProjectName.Get() );
	V_FixSlashes( pCtx->m_projectGenFolder.Access(), '/' );
		
	// ensure the output folder exists:
	Sys_CreatePath( pCtx->m_projectGenFolder );

	// schproj script location (next to vcproj)
	pCtx->m_schprojFilename.Set( pDataCollector->GetOutputFileName() );
    pCtx->m_schprojFilename.StripExtension();
    pCtx->m_schprojFilename.Append( ".schproj" );
	pCtx->m_schprojFilename.FixSlashes( '/' );

	// folder that stores the schemacompiler config (eg. schema_atomic_types.cfg)
	g_pVPC->ResolveMacrosInString( "$SCHEMACOMPILER_CONFIG_PATH", &pCtx->m_globalConfigFolder );
	V_FixSlashes( pCtx->m_globalConfigFolder.Access(), '/' );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void BuildSchemaFileInfo( SchemaFileInfo_t *pOutInfo, const SchemaContext_t &ctx, const char *pFilename, CVCProjGenerator *pDataCollector, bool bPerformSchemaCompile )
{
	// TODO: build a SchemaFileInfo_t per config (for now, this assumes the data is the same across all configs...)

	pOutInfo->m_inputFile = pFilename;
	pOutInfo->m_inputFile.FixSlashes( '/' );

    pOutInfo->m_noExt.Set( pOutInfo->m_inputFile );
    pOutInfo->m_noExt.StripExtension();
	V_FileBase( pOutInfo->m_inputFile, pOutInfo->m_pBase, sizeof(pOutInfo->m_pBase) );
	pOutInfo->m_path.ExtractFilePath( pOutInfo->m_inputFile );
	V_ExtractFileExtension( pOutInfo->m_inputFile, pOutInfo->m_pExt, sizeof(pOutInfo->m_pExt) );

	if ( IsCFileExtension( pOutInfo->m_pExt ) )
	{
		pOutInfo->m_bIsCppFile = true;
		V_snprintf( pOutInfo->m_pSymbolName, sizeof(pOutInfo->m_pSymbolName), "%s_cpp", pOutInfo->m_pBase );
	}
	else
	{
		pOutInfo->m_bIsCppFile = false;
		V_snprintf( pOutInfo->m_pSymbolName, sizeof(pOutInfo->m_pSymbolName), "%s_h", pOutInfo->m_pBase );
	}
	
	pOutInfo->m_bPerformSchemaCompile = bPerformSchemaCompile;

	if ( !pDataCollector )
	{
		// no config = no PCH
		pOutInfo->m_pchName.Purge();
		pOutInfo->m_preIncludeFiles.Purge();
	}
	else
	{
		CProjectFile *pProjectFile = NULL;
		if ( !pDataCollector->FindFile( pFilename, &pProjectFile ) )
		{
			g_pVPC->VPCError( "Internal VPC error trying to look up file info for schema file '%s'\n", pFilename );
		}

		CUtlVector<CProjectConfiguration *> rootConfigs;
		pDataCollector->GetAllRootConfigurations( rootConfigs );
		for ( int i = 0; i < rootConfigs.Count(); i++ )
		{
			CProjectConfiguration *pRootConfig = rootConfigs[i];

			CProjectConfiguration *pFileConfig = NULL;
			if ( pProjectFile )
			{
				pProjectFile->GetConfiguration( pRootConfig->m_Name.Get(), &pFileConfig );
			}

			bool bCreatesPCH, bExcludesPCH;
			CUtlString configPCHName;
			VPC_GetPCHInclude( pProjectFile, pRootConfig, configPCHName, bCreatesPCH, bExcludesPCH );
			if ( !bCreatesPCH ) // We care about PCH-consumers, not creators
			{
				if ( ( i > 0 ) && ( configPCHName != pOutInfo->m_pchName ) )
				{
					// The PCH filename must be the same for all configurations (eg. Debug / Release)
					// TODO: generate per-config SchemaFileInfo_t instead of making this assumption!
					g_pVPC->VPCError( "Inconsistent PCH filenames for schema file '%s' ('%s' != '%s') - they must be the same for all configurations.\n", pFilename, configPCHName.Get(), pOutInfo->m_pchName.Get() );
				}
				pOutInfo->m_pchName = configPCHName;
			}

			CUtlString preIncludeFiles;
			if ( !VPC_GetPropertyString( KEYWORD_GENERAL, pRootConfig, pFileConfig, SCHEMA_PRE_INCLUDE_FILES_PROPERTY, &preIncludeFiles ) )
			{
				g_pVPC->VPCError( "Unexpected %s missing for schema file '%s' (config '%s') - should at least auto-inherit a default value from base vpc.\n", SCHEMA_PRE_INCLUDE_FILES_PROPERTY, pFilename, pRootConfig->m_Name.Get() );
			}
			pOutInfo->m_preIncludeFiles = preIncludeFiles.Get();
			pOutInfo->m_preIncludeFiles.FixSlashes( '/' );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void EmitSchemaGeneratedFileInfo_ForConfiguration( CUtlBuffer &vpcBuffer, const char *szName, const char *szActiveConfiguration, bool bSingleConfig, bool bRemovePCH = false )
{
	vpcBuffer.Printf( "$DynamicFile%s \"%s\"\n", bRemovePCH ? "_NoPCH" : "", szName );

	if ( !bSingleConfig )
	{
		vpcBuffer.Printf( "{\n" );
	
		//in general, the file is excluded
		vpcBuffer.Printf( "$Configuration\n" );
		vpcBuffer.Printf( "{\n" );
		{
			vpcBuffer.Printf( "%s	\"Yes\"\n", g_pOption_ExcludedFromBuild );
		}
		vpcBuffer.Printf( "}\n" );

		//except this exact configuration
		vpcBuffer.Printf( "$Configuration \"%s\"\n" , szActiveConfiguration );
		vpcBuffer.Printf( "{\n" );
		{
			vpcBuffer.Printf( "%s	\"No\"\n", g_pOption_ExcludedFromBuild );
		}
		vpcBuffer.Printf( "}\n" );

		vpcBuffer.Printf( "}\n" );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void BuildRequiredPCHListForSchemaFiles( CUtlVector< CUtlString > &pchNames, const CUtlVector<SchemaFileInfo_t> &schemaFileInfos )
{
	// TODO: generate per-config SchemaFileInfo_t data... for now we assume it's the same for a given file, across all configs
	pchNames.RemoveAll();
	for ( int i = 0; i < schemaFileInfos.Count(); i++ )
	{
		CUtlString pchName = schemaFileInfos[i].m_pchName;
		if ( !pchName.IsEmpty() && !pchNames.HasElement( pchName ) )
			pchNames.AddToTail( pchName );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void AddUnitySchemaFilesForConfig( const SchemaContext_t &ctx, CUtlVector<SchemaFileInfo_t> &schemaFileInfos, CVCProjGenerator *pDataCollector, const char *pConfigName, CUtlVector< SchemaUnityBatch_t > *pOutUnityBatches )
{
	if ( !ctx.m_bHeaderUnityBuild )
		return;

	int nHFileCount = 0;
	for ( int f = 0; f < schemaFileInfos.Count(); ++f )
	{
		SchemaFileInfo_t &info = schemaFileInfos[f];
		if ( !info.m_bIsCppFile )
		{
			++nHFileCount;
		}
	}
	if ( nHFileCount == 0 )
		return;

	// Add schema file info for the schema unity file
	int nUnityFileCount = ( nHFileCount + (ctx.m_nUnityBatchSize - 1 ) ) / ctx.m_nUnityBatchSize;
	pOutUnityBatches->SetCount( nUnityFileCount );

	for ( int nBatchNumber = 0; nBatchNumber < nUnityFileCount; ++nBatchNumber )
	{
		CUtlPathStringHolder unityFilename( g_pVPC->FormatTemp1( "%s%d%s", ctx.m_unityFilenameBase.Get(), nBatchNumber, ctx.m_pUnityFilenameTail ) );
		SchemaFileInfo_t *pNewUnityFileInfo = &schemaFileInfos[schemaFileInfos.AddToTail()];
		BuildSchemaFileInfo( pNewUnityFileInfo, ctx, unityFilename, NULL, false );

		SchemaUnityBatch_t &batchInfo = pOutUnityBatches->Element( nBatchNumber );
		batchInfo.m_BatchFileName = SchemaGeneratedFilePath( *pNewUnityFileInfo, ctx, pConfigName );
	}


	int nCurrFileIndex = 0;
	for ( int nBatchNumber = 0; nBatchNumber < nUnityFileCount; ++nBatchNumber )
	{
		SchemaUnityBatch_t &batchInfo = pOutUnityBatches->Element( nBatchNumber );

		for ( int nFileIndex = 0; ( nCurrFileIndex < schemaFileInfos.Count() && nFileIndex < ctx.m_nUnityBatchSize ); ++nCurrFileIndex )
		{
			SchemaFileInfo_t &info = schemaFileInfos[nCurrFileIndex];
			if ( info.m_bIsCppFile )
				continue;

			CUtlString generatedFile = SchemaGeneratedFilePath( schemaFileInfos[nCurrFileIndex], ctx, pConfigName );
			char pFilePathRelativeToBatch[MAX_FIXED_PATH];
			V_MakeRelativePath( generatedFile.Get(), batchInfo.m_BatchFileName.DirName().Get(), pFilePathRelativeToBatch, sizeof(pFilePathRelativeToBatch) );
			batchInfo.m_BatchFiles.AddToTail( pFilePathRelativeToBatch );
			++nFileIndex;
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CSchemaVPC::AddFilesToVPCBuffer( CUtlBuffer *pOutVPCBuffer, const char *pFolderName, bool bSingleConfig, bool bUseSchprojBuildStep, bool bSchprojBuildStepShouldRegistersOutputs )
{
	//--------------------------------------------------------------------------------------------------
	// Files generated by the schemacompiler
	pOutVPCBuffer->Printf( "$Folder %s \n", pFolderName );
	pOutVPCBuffer->Printf( "{\n" );

	for ( int iConfig = 0; iConfig < m_Configs.Count(); ++iConfig )
	{
		CConfig *pCfg = m_Configs[iConfig];
		const char *pCasedConfigName = pCfg->m_CasedConfigName.Get();
		const char *pLowerCaseConfigName = pCfg->m_LowerCaseConfigName.Get();
		SchemaContext_t &ctx = m_Context;

		if ( !bSingleConfig )
		{
			pOutVPCBuffer->Printf( "$Folder \"%s\" \n", pCasedConfigName );
			pOutVPCBuffer->Printf( "{\n" );
		}

		{
			// The anchor file doesn't use a PCH (since projects may contain multiple PCHs, and the anchor file is tiny anyway)
			bool bRemovePCH = true;
			EmitSchemaGeneratedFileInfo_ForConfiguration( *pOutVPCBuffer, pCfg->m_AnchorPath.Get(), pCasedConfigName, bSingleConfig, bRemovePCH );
			
			for ( int iFile = 0; iFile < pCfg->m_Files.Count(); ++iFile )
			{
				SchemaFileInfo_t &fileInfo = pCfg->m_Files[iFile];

				// The schema unity build cpp file includes all the _h files
				if ( ctx.m_bHeaderUnityBuild && !fileInfo.m_bIsCppFile )
					continue;

				CUtlString generatedFilePath = SchemaGeneratedFilePath( fileInfo, ctx, pLowerCaseConfigName );
				EmitSchemaGeneratedFileInfo_ForConfiguration( *pOutVPCBuffer, generatedFilePath.Get(), pCasedConfigName, bSingleConfig );

				// Record the relationship between the input & output files:
                CUtlStringBuilder* pStrBuf = g_pVPC->GetTempStringBuffer1();
                pStrBuf->Set( fileInfo.m_inputFile );
				if ( !bSingleConfig )
					pStrBuf->AppendFormat( ":%s", pLowerCaseConfigName );
				g_pVPC->m_SchemaOutputFileMap[ pStrBuf->Get() ] = generatedFilePath.Get();
			}
		}		
		if ( !bSingleConfig )
		{
			pOutVPCBuffer->Printf( "}\n" );
		}
	}
	pOutVPCBuffer->Printf( "}\n" );

	//--------------------------------------------------------------------------------------------------
	// The schema schproj	
	pOutVPCBuffer->Printf( "$DynamicFile \"%s\"\n", m_Context.m_schprojFilename.Get() );
	if ( bUseSchprojBuildStep )
	{
		//to use the file build tool, we attach it to the schproj file
		pOutVPCBuffer->Printf( "{\n" );
		// add the custom build step to the schproj file
		for ( int iConfig = 0; iConfig < m_Configs.Count(); ++iConfig )
		{
			CConfig *pCfg = m_Configs[iConfig];

			CUtlString description, cmdLine, outputList, potentialOutputList, additionalDependencies, orderOnlyFileDependencies;
			GetCustomBuildStrings( false, pCfg, m_Context, pCfg->m_Files, description, cmdLine, outputList, potentialOutputList, additionalDependencies, orderOnlyFileDependencies );

			pOutVPCBuffer->Printf(	"$Configuration \"%s\"\n"
									"{\n"
										"$CustomBuildStep\n"
										"{\n", pCfg->m_CasedConfigName.Get() );

			pOutVPCBuffer->Printf(				"$Description				\"%s\"\n", description.Get() );
			pOutVPCBuffer->Printf(				"$CommandLine				\"%s\"\n", cmdLine.Get() );
			//outputs and dependencies can be large enough to overflow CUtlBuffer::Printf()
				pOutVPCBuffer->PutString(		"$Outputs					\"" );
				pOutVPCBuffer->PutString(										outputList.Get() );
				pOutVPCBuffer->PutString(															"\"\n" );

				pOutVPCBuffer->PutString(		"$PotentialOutputs			\"" );
				pOutVPCBuffer->PutString(										potentialOutputList.Get() );
				pOutVPCBuffer->PutString(															"\"\n" );

				pOutVPCBuffer->PutString(		"$AdditionalDependencies	\"" );
				pOutVPCBuffer->PutString(										additionalDependencies.Get() );
				pOutVPCBuffer->PutString(																	"\"\n" );

				pOutVPCBuffer->PutString(		"$OrderOnlyFileDependencies		\"" );
				pOutVPCBuffer->PutString(										orderOnlyFileDependencies.Get() );
				pOutVPCBuffer->PutString(																	"\"\n" );

			pOutVPCBuffer->Printf(		"}\n"
									"}\n" );
		}
		pOutVPCBuffer->Printf( "}\n" );
	}
}
						 

//--------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void SetProjectCustomBuild( const char *pConfigName, CProjectConfiguration *pRootConfiguration, CVCProjGenerator *pVCProjGenerator, const char *pCmdLine, const char *pDesc, const char *pAdditionalDeps, const char *pOutputs )
{
	CGeneratorDefinition *pGenerator = pVCProjGenerator->GetGeneratorDefinition();
	ToolProperty_t *pCommandLineProp = pGenerator->GetProperty( KEYWORD_CUSTOMBUILDSTEP, "$CommandLine" );
	ToolProperty_t *pDescriptionProp = pGenerator->GetProperty( KEYWORD_CUSTOMBUILDSTEP, "$Description" );
	ToolProperty_t *pAdditionalDependenciesProp = pGenerator->GetProperty( KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies_Proj );
	ToolProperty_t *pOutputsProp = pGenerator->GetProperty( KEYWORD_CUSTOMBUILDSTEP, "$Outputs" );
	ToolProperty_t *pExecuteBeforeProp = pGenerator->GetProperty( KEYWORD_CUSTOMBUILDSTEP, "$ExecuteBefore" );

	if ( !pCommandLineProp || !pDescriptionProp || !pAdditionalDependenciesProp || !pOutputsProp || !pExecuteBeforeProp )
	{
		// if you hit this, you're probably missing $AdditionalDependencies_Proj for your target platform
		g_pVPC->VPCError( "Internal VPC error trying to set Schema custom build for project\n" );
		return;
	}

	const char *pExecuteBeforeValue = "ClCompile";
	
	CCustomBuildTool *pTool = pRootConfiguration->GetCustomBuildTool();
	Assert( pTool );

	g_pVPC->GetScript().PushScript( "ProjectCustomBuild( CommandLine )", pCmdLine, 1, false, false );
	pTool->SetProperty( pCommandLineProp );
	g_pVPC->GetScript().PopScript();

	g_pVPC->GetScript().PushScript( "ProjectCustomBuild( Description )", pDesc, 1, false, false );
	pTool->SetProperty( pDescriptionProp );
	g_pVPC->GetScript().PopScript();

	g_pVPC->GetScript().PushScript( "ProjectCustomBuild( AdditionalDependencies_Proj )", pAdditionalDeps, 1, false, false );
	pTool->SetProperty( pAdditionalDependenciesProp );
	g_pVPC->GetScript().PopScript();

	g_pVPC->GetScript().PushScript( "ProjectCustomBuild( Outputs )", pOutputs, 1, false, false );
	pTool->SetProperty( pOutputsProp );
	g_pVPC->GetScript().PopScript();

	g_pVPC->GetScript().PushScript( "ProjectCustomBuild( ExecuteBefore )", pExecuteBeforeValue, 1, false, false );
	pTool->SetProperty( pExecuteBeforeProp );
	g_pVPC->GetScript().PopScript();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void MakeSchemacompilerCommandLine( const SchemaContext_t &ctx, const char *pConfigName, const char *pSentinelName, CUtlString *pOutCmdLine )
{
	CUtlString sentinelArg;
	if ( pSentinelName != NULL )
	{
		sentinelArg.Format( " -sentinel %s", pSentinelName );
	}
	else
	{
		sentinelArg = " -no_sentinel";
	}

    const char *pCmdFmt = "%s -schproj %s -config %s%s";
	pOutCmdLine->Format( pCmdFmt, ctx.m_schemaCompilerExePath.Get(), ctx.m_schprojFilename.Get(), pConfigName, sentinelArg.Get() );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CSchemaVPC::CreateSchemaCustomBuild( CConfig *pCfg, CProjectConfiguration *pRootConfiguration, CVCProjGenerator *pVCProjGenerator, const SchemaContext_t &ctx, CUtlVector<SchemaFileInfo_t> &schemaFileInfos )
{
	CUtlString description, cmdLine, outputList, potentialOutputList, additionalDependencies, orderOnlyFileDependencies;
	GetCustomBuildStrings( true, pCfg, ctx, schemaFileInfos, description, cmdLine, outputList, potentialOutputList, additionalDependencies, orderOnlyFileDependencies );

	if ( !orderOnlyFileDependencies.IsEmpty() )
	{
		if ( !additionalDependencies.IsEmpty() )
		{
			additionalDependencies += ";";
		}
		additionalDependencies += orderOnlyFileDependencies;
	}
	
	if ( !potentialOutputList.IsEmpty() )
	{
		if ( !outputList.IsEmpty() )
		{
			outputList += ";";
		}
		outputList += potentialOutputList;
	}

	CUtlString quotedDescription;
	quotedDescription.Format( "\"%s\"", description.Get() );

	CUtlString cmdLineQuotedNewline;
	cmdLineQuotedNewline.Format( "\"%s\"\n", cmdLine.Get() );

	CUtlString quotedOutputList;
	quotedOutputList.Format( "\"%s\"", outputList.Get() );

	SetProjectCustomBuild( pCfg->m_LowerCaseConfigName.Get(), pRootConfiguration, pVCProjGenerator, cmdLineQuotedNewline.Get(), quotedDescription.Get(), additionalDependencies.Get(), quotedOutputList.Get() );
}

void CSchemaVPC::GetCustomBuildStrings( bool bIsGlobalBuildStep, CConfig *pCfg, const SchemaContext_t &ctx, CUtlVector<SchemaFileInfo_t> &schemaFileInfos, CUtlString &description, CUtlString &cmdLine, CUtlString &outputList, CUtlString &potentialOutputList, CUtlString &additionalDependencies, CUtlString &orderOnlyFileDependencies )
{
	additionalDependencies.Clear();
	bool bAlwaysRun = false;

	if ( bIsGlobalBuildStep )
	{
		// __always_run__ is there to force the schema compiler to always be invoked as
		// explicitly listing all of the proper dependencies would require the #include
		// tree to be known and expressed as dependencies.  Instead the schema compiler
		// is always run and only updates its outputs if necessary.
		bAlwaysRun = true;
	}
	else
	{
		//direct dependencies
		{
			//depend on the actual schema files
			for ( int i = 0; i < schemaFileInfos.Count(); ++i )
			{
				if ( !schemaFileInfos[i].m_bPerformSchemaCompile )
					continue;

				if ( !additionalDependencies.IsEmpty() )
				{
					additionalDependencies += ";";
				}
				additionalDependencies += schemaFileInfos[i].m_inputFile;
			}

			//depend on $SchemaIncludeFile files
			for ( int i = 0; i < m_IncludeFiles.Count(); ++i )
			{
				if ( !additionalDependencies.IsEmpty() )
				{
					additionalDependencies += ";";
				}
				additionalDependencies += m_IncludeFiles[i].Get();
			}

			//depend on $SchemaPreIncludeFiles
			for ( int i = 0; i < pCfg->m_AllPreincludeFiles.Count(); ++i )
			{
				if ( !additionalDependencies.IsEmpty() )
				{
					additionalDependencies += ";";
				}
				additionalDependencies += pCfg->m_AllPreincludeFiles[i].Get();
			}
		}

		//Indirectly depend on the output .mak/.vcxproj/.xcodeproj to plug holes in dependency checking
		{
			if ( !additionalDependencies.IsEmpty() )
			{
				additionalDependencies += ";";
			}
			additionalDependencies += "$VPC_OUTPUT_FILE";
		}

		if ( bAlwaysRun )
		{
			Log_Warning( LOG_VPC, Color( 255, 255, 0, 255 ), "Schema compile falling back to \"Always Run\"\n" );
		}
	}

	if ( bAlwaysRun )
	{
		additionalDependencies = "__always_run__";
	}

	// The schema compiler executables are also listed as dependencies so that
	// if the schema compiler is rebuilt along with schematized files in one
	// build the build knows that it should not re-run the schema compiler
	// until the rebuild of the schema compiler itself is done.
	orderOnlyFileDependencies = ctx.m_schemaCompilerExePath.Get();
	orderOnlyFileDependencies += ";";
	orderOnlyFileDependencies += ctx.m_schemaCompilerExeDependent.Get();

	CUtlString sentinelFile;
	sentinelFile.Format( "$(IntermediateOutputPath)/schema_sentinel.txt" );

	cmdLine.Clear();
	MakeSchemacompilerCommandLine( ctx, pCfg->m_LowerCaseConfigName.Get(), sentinelFile.Get(), &cmdLine );

	description.Format( "Schema Compiler: %s [%s]", ctx.m_uniqueProjectName.Get(), pCfg->m_CasedConfigName.Get() );

	// NOTE NOTE: The makefile generator also relies on the sentinel file being first.
	outputList.Clear();
	outputList += sentinelFile; // NOTE: This MUST be first due to a bug in VS2010 - utils/schemacompiler/schemacompiler_schproj.cpp - CSchprojFile::Load()
	
	//list all the cpp files schemacompiler generates as potential outputs
	{
		if ( !potentialOutputList.IsEmpty() )
		{
			potentialOutputList += ";";
		}

		potentialOutputList += pCfg->m_AnchorPath.Get();

		for ( int i = 0; i < schemaFileInfos.Count(); ++i )
		{
			potentialOutputList += ";";
			potentialOutputList += SchemaGeneratedFilePath( schemaFileInfos[i], ctx, pCfg->m_LowerCaseConfigName.Get() );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CSchemaVPC::EmitSchproj( bool bStrictOutputs )
{
	KeyValues *pOutKeyValues = new KeyValues("schproj");

	pOutKeyValues->SetInt( "schproj_version", SCHEMA_SCHPROJ_VERSION );

	pOutKeyValues->SetString( "project_name", g_pVPC->GetProjectName() ); // NOTE: This is intentionally different from pUniqueProjectName
	pOutKeyValues->SetString( "project_path", m_Context.m_pProjectCodeFolder );
	pOutKeyValues->SetString( "src_dir_abs_path", m_Context.m_pSrcDirAbsPath );
	pOutKeyValues->SetString( "platform_name", m_Context.m_platformName );
	pOutKeyValues->SetString( "compiler_name", m_Context.m_compilerName );
	pOutKeyValues->SetString( "global_config_folder", m_Context.m_globalConfigFolder );
	pOutKeyValues->SetBool( "touch_unchanged_outputs", bStrictOutputs ); //If we're using well-defined output dependencies, then we *must* update the timestamp on outputs to have any effect

	const char *pBinName = g_pVPC->GetMacroValue( "OUTBINNAME" );
	const char *pLibName = g_pVPC->GetMacroValue( "OUTLIBNAME" );
	if ( ( !pBinName[0] && !pLibName[0] ) || ( pBinName[0] && pLibName[0] ) )
	{
		g_pVPC->VPCError( "VPC could not determine whether this is a bin or a lib.\n" );
	}

	KeyValues *pOutAllConfigs = new KeyValues( "configs" );
	pOutKeyValues->AddSubKey( pOutAllConfigs );

	//--------------------------------------------------------------------------------------------------
	// All of the config data
	for ( int iConfig = 0; iConfig < m_Configs.Count(); ++iConfig )
	{
		CConfig *pCfg = m_Configs[iConfig];

		KeyValues *pConfigKV = ConfigPreprocessorSettingsAsKV( m_pDataCollector, pCfg->m_pRootConfig, pCfg->m_Files );
		pOutAllConfigs->AddSubKey( pConfigKV );

		pConfigKV->SetString( "anchor_path", pCfg->m_AnchorPath.Get() );

		char szNum[64];
		{
			KeyValues *pSchemaFiles = new KeyValues( "schema_files" );
			pConfigKV->AddSubKey( pSchemaFiles );

			int nInput = 0;			
			for ( int i = 0; i < pCfg->m_Files.Count(); i++ )
			{
				V_snprintf( szNum, sizeof(szNum), "%03d", nInput++ );

				SchemaFileInfo_t &fileInfo = pCfg->m_Files[i];

				if ( !fileInfo.m_bPerformSchemaCompile )
					continue;

				KeyValues *pOutFile = new KeyValues( szNum );
				pSchemaFiles->AddSubKey( pOutFile );

				pOutFile->SetString( "file", fileInfo.m_inputFile );
				pOutFile->SetString( "generated_file", SchemaGeneratedFilePath( fileInfo, m_Context, pCfg->m_pRootConfig->m_LowerCaseName.Get() ).Get() );
				pOutFile->SetString( "symbol_name", fileInfo.m_pSymbolName );
				pOutFile->SetString( "pch_filename", fileInfo.m_pchName );
				pOutFile->SetString( "pre_include_files", fileInfo.m_preIncludeFiles );

				pOutFile->SetBool( "is_cpp", fileInfo.m_bIsCppFile );
			}
		}

		//--------------------------------------------------------------------------------------------------
		// Unity batches
		if ( pCfg->m_UnityBatches.Count() > 0 )
		{
			KeyValues *pOutUnityBatchList = new KeyValues( "unity_batches" );
			pConfigKV->AddSubKey( pOutUnityBatchList );

			for ( int i = 0; i < pCfg->m_UnityBatches.Count(); ++i )
			{
				const SchemaUnityBatch_t &batch = pCfg->m_UnityBatches[i];
				KeyValues *pBatch = new KeyValues( batch.m_BatchFileName.Get() );
				pOutUnityBatchList->AddSubKey( pBatch );

				for ( int iFile = 0; iFile < batch.m_BatchFiles.Count(); ++iFile )
				{
					pBatch->CreateNewKey()->SetString( NULL, batch.m_BatchFiles[iFile].Get() );
				}
			}
		}
	}

	//--------------------------------------------------------------------------------------------------
	// Plumb $SchemaIncludeFile through to the schema compiler
	if ( m_IncludeFiles.Count() )
	{
		KeyValues *pIncludeFiles = new KeyValues( "include_files" );
		pOutKeyValues->AddSubKey( pIncludeFiles );

		char szNum[64];
		int nInput = 0;
		for ( int i = 0; i < m_IncludeFiles.Count(); ++i )
		{
			V_snprintf( szNum, sizeof(szNum), "%03d", nInput++ );

			pIncludeFiles->SetString( szNum, m_IncludeFiles[i].Get() );
		}
	}

	//--------------------------------------------------------------------------------------------------
	// Write the schproj
	CUtlBuffer tmpBuf;
	tmpBuf.SetBufferType( true, false );
	pOutKeyValues->RecursiveSaveToFile( tmpBuf, 0 );
	pOutKeyValues->deleteThis();

	if ( g_pVPC->IsForceGenerate() || Sys_FileChanged( m_Context.m_schprojFilename.Get(), tmpBuf, true ) )
	{
		if ( Sys_WriteFile( m_Context.m_schprojFilename.Get(), tmpBuf, true ) )
		{
			Sys_CopyToMirror( m_Context.m_schprojFilename.Get() );
		}
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues *ConfigPreprocessorSettingsAsKV( CVCProjGenerator *pDataCollector, CProjectConfiguration *pRootConfig, CUtlVector<SchemaFileInfo_t> &schemaFileInfos )
{
	// TODO: schproj files currently ignore per-file configuration overrides:
	CProjectFile *pFile = NULL;

	KeyValues *pOutConfig = new KeyValues( pRootConfig->m_Name.Get() );
	CFmtStr num;
	//////////////////////////////////////////////////////////////////////////
	// write defines 
	{
		KeyValues *pOutDefines = new KeyValues( "defines" );
		pOutConfig->AddSubKey( pOutDefines );

		CUtlVector<CUtlString> defines;
		VPC_GetPreprocessorDefines( pFile, pRootConfig, defines );
		Assert( defines.Count() ); // The key won't get written out if there are zero defines
		for ( int i=0; i < defines.Count(); i++ )
		{
			pOutDefines->SetString( num.sprintf( "%03d", i ), defines[i].Get() );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// write includes 
	{
		KeyValues *pOutIncludes = new KeyValues( "includes" );
		pOutConfig->AddSubKey( pOutIncludes );

		CUtlVector<CUtlString> includes;
 		VPC_GetIncludeDirectories( pFile, pRootConfig, includes );
		Assert( includes.Count() ); // The key won't get written out if there are zero defines
		for ( int i=0; i < includes.Count(); i++ )
		{
			pOutIncludes->SetString( num.sprintf( "%03d", i ), includes[i].Get() );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// pch info
	{
		// Build PCH data - find out which PCH files our schemafiles include, and the files which generate those PCHs
		CUtlVector< CUtlString > requiredPCHs;
		BuildRequiredPCHListForSchemaFiles( requiredPCHs, schemaFileInfos );
		CUtlVector< CUtlString > pchIncludeNames, pchCreatorNames;
		VPC_GeneratePCHInfo( pDataCollector, pRootConfig, pchIncludeNames, pchCreatorNames, &requiredPCHs );

		// TODO: refactor this and other code to use 'SaveStringListToKV' & 'ReadStringListFromKV' (put those in tier1)
		KeyValues *pOutPCHs = new KeyValues( "pchs" );
		pOutConfig->AddSubKey( pOutPCHs );
		for ( int i = 0; i < pchIncludeNames.Count(); i++ )
		{
			KeyValues *pOutPCH = new KeyValues( num.sprintf( "%03d", i ) );
			pOutPCHs->AddSubKey( pOutPCH );
			pOutPCH->SetString( "pch_include_filename", pchIncludeNames[i].Get() );
			pOutPCH->SetString( "pch_creator_filename", pchCreatorNames[i].Get() );
		}
		if ( !pchIncludeNames.Count() )
		{
			// Keyvalues WTF: empty keys (and keys holding empty strings!) aren't serialized!!
			pOutPCHs->SetStringValue( "none" );
		}
	}

	return pOutConfig;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CUtlString SchemaGeneratedFileFolder( const SchemaContext_t &ctx, const char *szConfigurationString )
{
	return g_pVPC->FormatTemp1( "%s/%s/%s", ctx.m_projectGenFolder.Get(), szConfigurationString, ctx.m_platformName.Get() );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CUtlString SchemaGeneratedFilePath( SchemaFileInfo_t &file, const SchemaContext_t &ctx, const char *szConfigurationString )
{
	CUtlString retVal;
	CUtlString folder = SchemaGeneratedFileFolder( ctx, szConfigurationString );
	
	if ( file.m_bIsCppFile )
	{
		retVal.Format( "%s/%s_cpp_schema." GENERATED_CPP_FILE_EXTENSION, folder.Get(), file.m_pBase );	
	}
	else
	{
		retVal.Format( "%s/%s_h_schema." GENERATED_CPP_FILE_EXTENSION, folder.Get(), file.m_pBase );
	}

	retVal.FixSlashes( '/' );
	
	return retVal;
}

void CollectAllSchemaRelatedfiles( CVCProjGenerator *pDataCollector, CUtlVector< CProjectFile * > &outputVector )
{
	//grab all the project files with flags VPC_FILE_FLAGS_SCHEMA_INCLUDE or VPC_FILE_FLAGS_SCHEMA
	pDataCollector->GetAllProjectFiles( outputVector );
	for ( int i = outputVector.Count() - 1; --i >= 0; )
	{
		CProjectFile *pFile = outputVector[i];
		if ( (pFile->m_iFlags & (VPC_FILE_FLAGS_SCHEMA_INCLUDE | VPC_FILE_FLAGS_SCHEMA)) == 0 )
		{
			//file is irrelevant, remove
			outputVector.Remove( i );
		}
	}	
}

void CollectSchemaIncludeFiles( const CUtlVector< CProjectFile * > &allSchemaRelatedFiles, CUtlVector< CUtlString > &outputVector )
{
	//whittle down the list to just VPC_FILE_FLAGS_SCHEMA_INCLUDE files with standardized '/' path separators
	for ( int i = 0; i < allSchemaRelatedFiles.Count(); ++i )
	{
		CProjectFile *pFile = allSchemaRelatedFiles[i];
		if ( pFile->m_iFlags & VPC_FILE_FLAGS_SCHEMA_INCLUDE )
		{
			//this is a $SchemaIncludeFile, fix the path separators
			CUtlString nameWithFixedSlashes = pFile->m_Name;
			nameWithFixedSlashes.FixSlashes( '/' );

			//and insert in the list if it's not a dupe
			if ( !outputVector.IsValidIndex( outputVector.Find( nameWithFixedSlashes ) ) )
			{
				outputVector.AddToTail( nameWithFixedSlashes );
			}
		}
	}
}

bool IsSchemaSupportedForThisTargetPlatform( void )
{
	const char *pPlatformName = g_pVPC->GetTargetPlatformName();
	bool bSupported = !V_stricmp_fast( pPlatformName, "WIN32" ) || 
						!V_stricmp_fast( pPlatformName, "WIN64" )||
						!V_stricmp_fast( pPlatformName, "X360" ) ||
						!V_stricmp_fast( pPlatformName, "OSX32" ) ||
						!V_stricmp_fast( pPlatformName, "OSX64" ) ||
						VPC_IsPlatformLinux( pPlatformName ) ||
						VPC_IsPlatformAndroid( pPlatformName );

	return bSupported;
}

bool CVPC::IsSchemaEnabled( void )
{
	if ( !m_bAllowSchema )
		return false;	// Schema not enabled
	if ( IsSchemaSupportedForThisTargetPlatform() )
		return true;	// Feature enabled & supported!
	ExecuteOnce( VPCWarning( "Schema feature disabled, not supported for %s yet", g_pVPC->GetTargetPlatformName() ) )
	return false;		// Platform not supported
}

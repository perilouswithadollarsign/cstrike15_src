//========= Copyright 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: Clang Script Generation
//
//=====================================================================================//

#include "vpc.h"

#include "baseprojectdatacollector.h"
#include "projectgenerator_vcproj.h"

#include "tier1/fmtstr.h"
#include "environment_utils.h"
#include "clang/clang_utils.h"



CUtlString JsonEscapeAndTrim( const CUtlString &str )
{
	int nDepth = 0; 
	CUtlString esc;
	for( int i = 0; i < str.Length(); ++i )
	{
		if( str.Get()[i] == '\\' || str.Get()[i] == '\"' )
		{
			if( str[i] == '\"' && nDepth == 1 )
			{
				nDepth = 0;
			}
			esc.Append( nDepth ? "\\\\\\" : "\\" );
			if( str[i] == '\"' && nDepth == 0 )
			{
				nDepth = 1;
			}
		}
		esc.Append( str[i] );
	}
	esc.TrimRight();
	return esc;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void VPC_GenerateClangProject_GenerateFileOutput( CProjectFile *pFile, CProjectConfiguration *pRootConfig, const CUtlString &pchName, bool bShouldBuildPCH, CUtlBuffer &buffer, CUtlBuffer &jsonBuffer )
{
	const char *pFilename = pFile->m_Name.Get();

	// TODO: optimize this (platform/compiler are constant for all files, and most files can use the define/include lists from pRootConfig, i.e if pFile has no file config)
	CUtlVector<CUtlString> defineList;
	VPC_GetPreprocessorDefines( pFile, pRootConfig, defineList );

	CUtlVector<CUtlString> includeList;
	VPC_GetIncludeDirectories( pFile, pRootConfig, includeList );

	CUtlString clangCmdLine;
	if ( !Clang_GenerateCommandLine( clangCmdLine, pFilename, g_pVPC->GetTargetPlatformName(), g_pVPC->GetTargetCompilerName(), defineList, includeList, pchName, bShouldBuildPCH ) )
		return;

	// Write out the command:
		buffer.Printf( "@echo. %%CLANG_OUTPUT%%\n" );
	buffer.Printf( "@echo -----START FILE (%s)----- %%CLANG_OUTPUT%%\n", pFilename );
	buffer.Printf( "@echo %%time%% %%CLANG_OUTPUT%%\n" );
	CFmtStrMax command( "%%CLANG_CMD%% %%CLANG_OPTIONS%% %s %%CLANG_OUTPUT%%\n", clangCmdLine.Get() );
	buffer.PutString( command );
	buffer.Printf( "@echo -----END FILE (%s)----- %%CLANG_OUTPUT%%\n", pFilename );
	buffer.Printf( "@echo %%time%% %%CLANG_OUTPUT%%\n" );
	buffer.Printf( "@echo. %%CLANG_OUTPUT%%\n\n" );

	jsonBuffer.Printf( "\n{\n\t\"directory\":\"%s\",\n\t\"command\":\"clang %s\",\n\t\"file\":\"%s\"\n},", JsonEscapeAndTrim( g_pVPC->GetProjectPath() ).Get(), JsonEscapeAndTrim( clangCmdLine ).Get(), JsonEscapeAndTrim( pFilename ).Get() );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void VPC_GenerateClangProject_ProcessFile(	CProjectFile *pFile, CProjectConfiguration *pRootConfig, CUtlSymbolTable *pSchemaFiles,
											CUtlVector< CUtlString > &requiredPCHs, CUtlBuffer &buffer, CUtlBuffer &jsonBuffer )
{
	// Parse all CPP files but only schematised headers (and skip everything else)
	const char *pFilename = pFile->m_Name.Get();
	bool bIsSource = Clang_IsSourceFile( pFilename ), bIsHeader = Clang_IsHeaderFile( pFilename );
	if ( !bIsSource && ( !bIsHeader || !pSchemaFiles->HasElement( pFilename ) ) )
	{
		return;
	}

	// If this file uses a PCH, make sure its in our list of required PCHs,
	// but skip actually building PCHs (those commands are added in a second pass):
	bool bFileCreatesPCH, bFileExcludesPCH;
	CUtlString pchName;
	VPC_GetPCHInclude( pFile, pRootConfig, pchName, bFileCreatesPCH, bFileExcludesPCH );
	if ( bFileCreatesPCH )
		return;
	if ( !pchName.IsEmpty() && !requiredPCHs.HasElement( pchName ) )
		requiredPCHs.AddToTail( pchName );

	VPC_GenerateClangProject_GenerateFileOutput( pFile, pRootConfig, pchName, false, buffer, jsonBuffer );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void VPC_GenerateClangProject_ProcessFolder(	CProjectFolder *pFolder, CProjectConfiguration *pRootConfig, CUtlSymbolTable *pSchemaFiles,
												CUtlVector< CUtlString > &requiredPCHs, CUtlBuffer &buffer, CUtlBuffer &jsonBuffer )
{
	for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
	{
		VPC_GenerateClangProject_ProcessFile( pFolder->m_Files[iIndex], pRootConfig, pSchemaFiles, requiredPCHs, buffer, jsonBuffer );
	}
	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		VPC_GenerateClangProject_ProcessFolder( pFolder->m_Folders[iIndex], pRootConfig, pSchemaFiles, requiredPCHs, buffer, jsonBuffer );
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void VPC_Clang_OnParseProjectEnd( CVCProjGenerator *pDataCollector )
{
	// Skip Clang processing during dependency-generation (or we'll end up doing the work multiple times!)
	if ( g_pVPC->m_bIsDependencyPass )
		return;

	if ( !g_pVPC->ShouldEmitClangProject() )
		return;

	// Build a symbol table of all the schema files, so we can skip non-schema headers:
	CUtlSymbolTable schemaFiles( 0, 16, true );
	int nNumSchemaFiles = g_pVPC->m_SchemaFiles.Count();
	for ( int i = 0; i < nNumSchemaFiles; i++ )
	{
		schemaFiles.AddString( g_pVPC->m_SchemaFiles[i].Get() );
	}

	CUtlVector<CProjectConfiguration *> rootConfigs;
	pDataCollector->GetAllRootConfigurations( rootConfigs );
	for ( int i = 0; i < rootConfigs.Count(); i++ )
	{
		CProjectConfiguration *pRootConfig = rootConfigs[i];

		// Output file is "<Project>_<GameName>_<Platform>_<Config>.bat"
		CUtlString gameName = g_pVPC->GetGameName();
		gameName = CUtlString( gameName.IsEmpty() ? "" : "_" ) + gameName;
		CUtlString baseName = CUtlString( g_pVPC->GetProjectName() ) + gameName + CUtlString( "_" ) + g_pVPC->GetTargetPlatformName() + "_" + pRootConfig->m_Name;

		// Write to the file via head/body/tail buffers:
		CUtlBuffer headBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
		CUtlBuffer bodyBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
		CUtlBuffer tailBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
		CUtlBuffer jsonPchBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
		CUtlBuffer jsonBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );

		// Prologue:
		// set 'CLANG_OPTIONS' at the top of the file, so its easy to add an option to all files in the project
		// TODO: translate these into command-line options: CXTranslationUnit_Incomplete, CXTranslationUnit_PrecompiledPreamble, CXTranslationUnit_CacheCompletionResults, CXTranslationUnit_SkipFunctionBodies
		headBuffer.Printf( "setlocal\n" );
		headBuffer.Printf( "set CLANG_CMD=clang.exe\n" );
		headBuffer.Printf( "set CLANG_OPTIONS=\n" );
		headBuffer.Printf( "REM set CLANG_OPTIONS=%%CLANG_OPTIONS%% -detailed-preprocessing-record \n" );
		// Spew into an output file, also done via an envvar (just comment CLANG_OUTPUT to spew to stdout instead)
		headBuffer.Printf( "set CLANG_OUTPUT_FILE=%s_clang_output.txt\n", baseName.Get() );
		headBuffer.Printf( "set CLANG_OUTPUT= ^>^> %%CLANG_OUTPUT_FILE%% 2^>^&1\n" );
		headBuffer.Printf( "@echo. > %%CLANG_OUTPUT_FILE%%\n" );
		headBuffer.Printf( "\n" );

		// Body:
		// Iterate over all the folders+files, generating compile commands (and building a list of included PCH files):
// TODO-CLANG: should we skip excluded files & build generated files, to match vcproj build behaviour? (i.e schema + qt files)
		CUtlVector< CUtlString > requiredPCHs;
		VPC_GenerateClangProject_ProcessFolder( pDataCollector->GetRootFolder(), pRootConfig, &schemaFiles, requiredPCHs, bodyBuffer, jsonBuffer );

		// Generate info for all the PCHs used by this project:
		CUtlVector< CUtlString > pchIncludeNames, pchCreatorNames;
		VPC_GeneratePCHInfo( pDataCollector, pRootConfig, pchIncludeNames, pchCreatorNames, &requiredPCHs );
		if ( pchIncludeNames.Count() )
		{
			// Add commands to build the PCH files at the head of the file (before the files which will use them)
			headBuffer.Printf( "\nREM ====PCH files first:====\n\n" );
			for( int j = 0; j < pchIncludeNames.Count(); j++ )
			{
				CProjectFile *pFile = NULL;
				bool bFixSlashes = true; // We need to write '/' to the schproj file, but FindFile expects '\'
				pDataCollector->FindFile( pchCreatorNames[j].Get(), &pFile, bFixSlashes );
				VPC_GenerateClangProject_GenerateFileOutput( pFile, pRootConfig, pchIncludeNames[j], true, headBuffer, jsonPchBuffer );
			}
			headBuffer.Printf( "\nREM ====Non-PCH files second:====\n\n" );

			// Add commands to delete the PCH files at the tail of the file
			tailBuffer.Printf( "\n@echo.\n" );
			tailBuffer.Printf( "@echo Press any key to delete PCH files, or exit now to leave them be.\n" );
			tailBuffer.Printf( "pause\n" );
			for( int j = 0; j < pchIncludeNames.Count(); j++ )
			{
				tailBuffer.Printf( "del %s.pch\n", pchIncludeNames[j].Get() );
			}
			tailBuffer.Printf( "\n" );
		}

		// Epilogue:
		// Misc cleanup...
		tailBuffer.Printf( "\n@echo.\n" );
		tailBuffer.Printf( "@echo DONE!\n" );
		tailBuffer.Printf( "pause\n\n" );

		// Output the batch file:
		CUtlPathStringHolder batchFilename;
        g_pVPC->CreateGeneratedRootFilePath( &batchFilename, baseName.Get(), "_clang.bat" );
		Sys_CreatePath( batchFilename.Get() );
		FILE *batFile = fopen( batchFilename.Get(), "wt" );
		if ( batFile )
		{
			fwrite( headBuffer.Base(), sizeof(char), headBuffer.TellMaxPut(), batFile );
			fwrite( bodyBuffer.Base(), sizeof(char), bodyBuffer.TellMaxPut(), batFile );
			fwrite( tailBuffer.Base(), sizeof(char), tailBuffer.TellMaxPut(), batFile );
			fclose( batFile );
		}
		else { g_pVPC->VPCWarning( "Error saving Clang batch file: %s", batchFilename.Get() ); }

		CUtlPathStringHolder jsonFilename;
        g_pVPC->CreateGeneratedRootFilePath( &jsonFilename, baseName.Get(), "_compile_commands.json" );
		Sys_CreatePath( jsonFilename.Get() );
		FILE *jsonFile = fopen( jsonFilename.Get(), "wt" );
		if( jsonFile )
		{
			fputs( "[", jsonFile );
			fwrite( jsonPchBuffer.Base(), sizeof( char ), jsonPchBuffer.TellMaxPut(), jsonFile );
			if( jsonBuffer.TellMaxPut() )
			{
				// don't put the last character, which is a comma..
				fwrite( jsonBuffer.Base(), sizeof( char ), jsonBuffer.TellMaxPut() - 1, jsonFile );
			}
			fputs( "\n]\n", jsonFile );
			fclose( jsonFile );
		}
		else { g_pVPC->VPCStatus( true, "Error saving Clang compile_database.json file: %s", jsonFilename.Get() ); }
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool IsClangSupportedForThisTargetPlatform( void )
{
	// TODO: Only implemented+tested on WIN32/WIN64 so far...
	//       [ VPC_GenerateClangProject() depends upon CVCProjGenerator due to bugs in the makefile code,
	//         and there's some porting work to do in clang_utils.cpp ]
	const char *pPlatform = g_pVPC->GetTargetPlatformName();
	return ( !V_stricmp_fast( pPlatform, "WIN32" ) || !V_stricmp_fast( pPlatform, "WIN64" ) );
}

bool CVPC::IsClangEnabled( void )
{
	if ( !m_bAllowClang )
		return false;	// Clang feature not enabled
	if ( IsClangSupportedForThisTargetPlatform() )	
		return true;	// Feature enabled & supported!
	ExecuteOnce( VPCWarning( "Clang feature disabled, not supported for %s yet", g_pVPC->GetTargetPlatformName() ) )
	return false;		// Platform not supported
}

bool CVPC::ShouldEmitClangProject( void )
{
	if ( !m_bEmitClangProject )
		return false;		// Feature not requested
	if ( IsClangEnabled() )
		return true;		// Feature requested & enabled & supported!
	ExecuteOnce( VPCWarning( "Ignoring '/clangall' option, Clang feature disabled" ) )
	return false;			// Clang not enabled
}

//========= Copyright (C), Valve Corporation, All rights reserved. =====================//
//
// Purpose: Helper code relating to build-generated files (schema/qt/protobuf/unity)
//
//=====================================================================================//

#include "vpc.h"


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CSourceFileInfo::CSourceFileInfo( int folderIndex )
 :	m_pSourceFile( NULL ),
	m_pDebugCompiledFile( NULL ),
	m_pReleaseCompiledFile( NULL ),
	m_pContainingUnityFile( NULL ),
	m_ConfigStringCRC( 0 ),
	m_bCreatesPCH( false ),
	m_iFolderIndex( folderIndex )
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool VPC_GeneratedFiles_CreateFileConfigString( CSourceFileInfo &fileInfo, const CUtlVector< CProjectConfiguration *> &rootConfigs )
{
	for ( int i = 0; i < rootConfigs.Count(); i++ )
	{
		CProjectFile *pCompiledFile = ( rootConfigs[i]->m_Name == "Debug" ) ? fileInfo.m_pDebugCompiledFile : fileInfo.m_pReleaseCompiledFile;

		// Start the config string off with the PCH name (for clarity; files with different PCHs must sort into different groups)
		fileInfo.m_ConfigString += fileInfo.m_PCHName;
		fileInfo.m_ConfigString += " ";

		// Look for configs with compiler properties
		CProjectConfiguration *pFileConfig = NULL;
		pCompiledFile->GetConfiguration( rootConfigs[i]->m_Name.Get(), &pFileConfig );
		CCompilerTool *pCompilerTool = pFileConfig ? pFileConfig->GetCompilerTool() : NULL;
		if ( !pCompilerTool || pCompilerTool->m_PropertyStates.m_Properties.IsEmpty() )
			continue;

		// Concat config name
		fileInfo.m_ConfigString += pFileConfig->m_Name;
		fileInfo.m_ConfigString += " ";

		// Concat compiler property names+values in consistent sorted order
		for ( int j = 0; j < pCompilerTool->m_PropertyStates.m_PropertiesInOutputOrder.Count(); j++ )
		{
			int sortedIndex = pCompilerTool->m_PropertyStates.m_PropertiesInOutputOrder[j];
			PropertyState_t &property = pCompilerTool->m_PropertyStates.m_Properties[sortedIndex];
			fileInfo.m_ConfigString += property.m_pToolProperty->m_ParseString;
			fileInfo.m_ConfigString += " ";
			fileInfo.m_ConfigString += property.m_StringValue;
			fileInfo.m_ConfigString += " ";
		}
	}

	fileInfo.m_ConfigString.ToLower(); // Ignore case differences
	fileInfo.m_ConfigStringCRC = CRC32_ProcessSingleBuffer( fileInfo.m_ConfigString.Get(), fileInfo.m_ConfigString.Length() );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool VPC_GeneratedFiles_GetPCHInfo( CSourceFileInfo &fileInfo, const CUtlVector< CProjectConfiguration *> &rootConfigs )
{
	for ( int i = 0; i < rootConfigs.Count(); i++ )
	{
		CProjectFile *pCompiledFile = ( rootConfigs[i]->m_Name == "Debug" ) ? fileInfo.m_pDebugCompiledFile : fileInfo.m_pReleaseCompiledFile;

		// Get the PCH used by this file [ NOTE: we expect the same PCH settings between Debug+Release ]
		bool bExcludesPCH;
		CUtlString pchName;
		VPC_GetPCHInclude( pCompiledFile, rootConfigs[i], pchName, fileInfo.m_bCreatesPCH, bExcludesPCH );
		if ( ( i > 0 ) && ( pchName != fileInfo.m_PCHName ) )
		{
			g_pVPC->VPCWarning( "VPC_GeneratedFiles_CreateFileConfigString: Unsupported PCH configuration for %s", pCompiledFile->m_Name.Get() );
			return false;
		}
		fileInfo.m_PCHName = pchName;
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// TODO: having to pass pDataCollector/rootConfigs is a pain, those should be (efficiently) accessible as globals
bool VPC_GeneratedFiles_GetSourceFileInfo(	CSourceFileInfo &fileInfo, CProjectFile *pFile, bool bGenerateConfigString,
											CVCProjGenerator *pDataCollector, const CUtlVector< CProjectConfiguration * > &rootConfigs )
{
	// Determine if this is to be compiled directly, or if generated files will be compiled in its stead
	fileInfo.m_pSourceFile = pFile;
	if ( pFile->m_iFlags & VPC_FILE_FLAGS_DYNAMIC )
	{
		// We don't return data for a dynamic file, but rather for the source file from which it is generated
		return false;
	}
	else if ( pFile->m_iFlags & VPC_FILE_FLAGS_SCHEMA )
	{
		// Get the schema-generated output files:
		fileInfo.m_pDebugCompiledFile   = VPC_Schema_GetGeneratedFile( pFile, "Debug",   pDataCollector );
		fileInfo.m_pReleaseCompiledFile = VPC_Schema_GetGeneratedFile( pFile, "Release", pDataCollector );
	}
	else if ( pFile->m_iFlags & VPC_FILE_FLAGS_QT )
	{
		// Get the Qt/MOC-generated output files (should both be the same):
		fileInfo.m_pDebugCompiledFile   = VPC_Qt_GetGeneratedFile( pFile, "Debug",   pDataCollector );
		fileInfo.m_pReleaseCompiledFile = VPC_Qt_GetGeneratedFile( pFile, "Release", pDataCollector );
	}
	else if ( IsCFileExtension( V_GetFileExtension( pFile->m_Name.Get() ) ) )
	{
		// Normal source file, outputs are the same as the input
		fileInfo.m_pDebugCompiledFile   = pFile;
		fileInfo.m_pReleaseCompiledFile = pFile;
	}
	else
	{
		// This file yields no compilable outputs (e.g a non-code file or a non-schema header)
		return false;
	}

	// Ignore missing input files (the output files may be missing though, if they're dynamic)
	if ( !Sys_Exists( pFile->m_Name.AbsPath( NULL /*, k_bVPCForceLowerCase*/ ).Get() ) )
		return false;

	if ( !fileInfo.m_pDebugCompiledFile || !fileInfo.m_pReleaseCompiledFile )
	{
		//g_pVPC->VPCWarning( "VPC_GeneratedFiles_GetSourceFileInfo: could not determine compileable output files for %s", pFile->m_Name.Get() );
		return false;
	}

	// Determine the containing unity file, if any
	fileInfo.m_pContainingUnityFile = VPC_Unity_GetContainingUnityFile( pFile, NULL, pDataCollector );

	// Determine which PCH this file uses, and whether it is responsible for building the PCH file
	if ( !VPC_GeneratedFiles_GetPCHInfo( fileInfo, rootConfigs ) )
		return false;

	if ( bGenerateConfigString )
	{
		// Generate additional information summarizing the file config settings for the output file(s)
		return VPC_GeneratedFiles_CreateFileConfigString( fileInfo, rootConfigs );
	}
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_GeneratedFiles_ProcessFolder( CProjectFolder *pFolder, CVCProjGenerator *pDataCollector, const CUtlVector< CProjectConfiguration * > &rootConfigs,
										CUtlBuffer &manifestFile )
{
	// Process files
	for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
	{
		CProjectFile *pFile = pFolder->m_Files[iIndex];
		CSourceFileInfo fileInfo;

		if ( !VPC_GeneratedFiles_GetSourceFileInfo( fileInfo, pFile, true, pDataCollector, rootConfigs ) )
			continue;

		CUtlString absSourceFilePath	= fileInfo.m_pSourceFile->m_Name.AbsPath( NULL /*, k_bVPCForceLowerCase*/ );
		CUtlString absDebugOutputPath	= fileInfo.m_pDebugCompiledFile->m_Name.AbsPath( NULL /*, k_bVPCForceLowerCase*/ );
		CUtlString absReleaseOutputPath	= fileInfo.m_pReleaseCompiledFile->m_Name.AbsPath( NULL /*, k_bVPCForceLowerCase*/ );
		CUtlString absUnityFilePath		= fileInfo.m_pContainingUnityFile ? fileInfo.m_pContainingUnityFile->m_Name.AbsPath( NULL /*, k_bVPCForceLowerCase*/ ) : "";

		manifestFile.Printf( "%-24s%s\n", "Source file:",			absSourceFilePath.Get() );
		manifestFile.Printf( "%-24s%s\n", "Debug output file:",		absDebugOutputPath.Get() );
		manifestFile.Printf( "%-24s%s\n", "Release output file:",	absReleaseOutputPath.Get() );
		manifestFile.Printf( "%-24s%s\n", "Containing unity file:",	absUnityFilePath.Get() );
		manifestFile.Printf( "%-24s%s\n", "PCH file:",				fileInfo.m_PCHName.Get() );
		if ( fileInfo.m_bCreatesPCH )
			manifestFile.Printf( "%-24s%s\n", "Builds PCH file:",	"yes" );
		manifestFile.Printf( "\n" );
	}

	// Recurse into child folders
	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		VPC_GeneratedFiles_ProcessFolder( pFolder->m_Folders[iIndex], pDataCollector, rootConfigs, manifestFile );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_GeneratedFiles_OnParseProjectEnd( CVCProjGenerator *pDataCollector )
{
	// This generates a build manifest file, which provides data for ValveVSAddin.

	// Skip building the manifest during dependency-generation (we need to see all
	// the schema/qt/unity-generated files, which are not created during this pass)
	if ( g_pVPC->m_bIsDependencyPass )
		return;

	// Write the manifest to "_vpc_\manifest_<ProjectName>_<GameName>\<Platform>\manifest.txt" to avoid filename conflicts and clutter
	CUtlPathStringHolder manifestPath;
    g_pVPC->CreateGeneratedSubdirPath( &manifestPath, "manifest" );
	Sys_CreatePath( manifestPath );

    CUtlPathStringHolder manifestName( manifestPath, "manifest.txt" );

	CUtlBuffer manifestFile( 0, 0, CUtlBuffer::TEXT_BUFFER );
	manifestFile.Printf( "%s\n",	"// ----------------------------------------- //\n"
									"//           File generated by VPC           //\n"
									"// ----------------------------------------- //\n" );

	// Iterate the folders in the project and generate data for all compiled files:
	CUtlVector< CProjectConfiguration * > rootConfigs;
	pDataCollector->GetAllRootConfigurations( rootConfigs );
	CProjectFolder *pRootFolder = pDataCollector->GetRootFolder();
	VPC_GeneratedFiles_ProcessFolder( pRootFolder, pDataCollector, rootConfigs, manifestFile );

	Sys_WriteFileIfChanged( manifestName.Get(), manifestFile, true );

	// Add the manifest file to the 'VPC Scripts' folder:
	CProjectFolder *pVPCFolder;
	if ( !pRootFolder->GetFolder( "VPC Scripts", &pVPCFolder ) )
		g_pVPC->VPCError( "VPC_GeneratedFiles_OnParseProjectEnd: cannot find 'VPC Scripts' folder!" );
	pDataCollector->AddFileToFolder( manifestName.Get(), pVPCFolder, true, VPC_FILE_FLAGS_DYNAMIC, NULL );
}

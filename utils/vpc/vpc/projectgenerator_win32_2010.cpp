//========= Copyright © 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

const char *GetVCProjTargetPlatformName( const char *szVPCTargetPlatformName )
{
	//makefile wrappers still have to be labeled as a valid windows platform to even try executing >_<
	if ( VPC_IsPlatform64Bits( szVPCTargetPlatformName ) )
	{
		return "x64";
	}
	else if ( VPC_IsPlatform32Bits( szVPCTargetPlatformName ) )
	{
		return "Win32";
	}
	
	Assert( false );
	
	return szVPCTargetPlatformName;
}

#undef PROPERTYNAME
#define PROPERTYNAME( X, Y ) { X##_##Y, #X, #Y },
static PropertyName_t s_Win32PropertyNames_2010[] =
{
	#include "projectgenerator_win32_2010.inc"
	{ -1, NULL, NULL }
};

IBaseProjectGenerator* GetWin32ProjectGenerator_2010()
{
	CProjectGenerator_Win32_2010 *pGen = new CProjectGenerator_Win32_2010();
    return (CVCProjGenerator*)pGen->GetProjectGenerator();
}

IVCProjWriter* GetWin32ProjectGenerator_VCProjWriter_2010()
{
    // Make sure we have an instance.
	return new CProjectGenerator_Win32_2010();
}

CProjectGenerator_Win32_2010::CProjectGenerator_Win32_2010()
{
	m_bGenerateMakefileVCXProj = false;
	m_bVisualGDB = false;

	m_pVCProjGenerator = new CVCProjGenerator();
	m_pVCProjGenerator->SetupGeneratorDefinition( "win32_2010.def", s_Win32PropertyNames_2010 );
	m_pVCProjGenerator->AddProjectWriter( this );
}

enum TypeKeyNames_e
{
	TKN_LIBRARY = 0,
	TKN_INCLUDE,
	TKN_COMPILE,
	TKN_RESOURCECOMPILE,
	TKN_CUSTOMBUILD,
	TKN_NONE,
	TKN_MAX_COUNT,
};

static const char *s_TypeKeyNames[] =
{
	"Library",
	"ClInclude",
	"ClCompile",
	"ResourceCompile",
	"CustomBuild",
	"None"
};

const char *CProjectGenerator_Win32_2010::GetKeyNameForFile( CProjectFile *pFile )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_TypeKeyNames ) == TKN_MAX_COUNT );

	const char *pExtension = V_GetFileExtension( pFile->m_Name.Get() );

	const char *pKeyName = s_TypeKeyNames[TKN_NONE];
	if ( pExtension )
	{
		if ( pFile->m_Configs.Count() && pFile->m_Configs[0]->GetCustomBuildTool() )
		{
			pKeyName = s_TypeKeyNames[TKN_CUSTOMBUILD];
		}
		else if ( IsCFileExtension( pExtension ) )
		{
			pKeyName = s_TypeKeyNames[TKN_COMPILE];
		}
		else if ( IsHFileExtension( pExtension ) )
		{
			pKeyName = s_TypeKeyNames[TKN_INCLUDE];
		}
		else if ( !V_stricmp_fast( pExtension, "lib" ) )
		{
			pKeyName = s_TypeKeyNames[TKN_LIBRARY];
		}
		else if ( !V_stricmp_fast( pExtension, "rc" ) )
		{
			pKeyName = s_TypeKeyNames[TKN_RESOURCECOMPILE];
		}
	}

	return pKeyName;
}

bool CProjectGenerator_Win32_2010::WritePropertyGroupTool( CProjectTool *pProjectTool, CProjectConfiguration *pConfiguration, const char *szPlatformName )
{
	if ( !pProjectTool )
		return true;

	for ( int i = 0; i < pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
	{
		int sortedIndex = pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder[i];
		if ( !pProjectTool->m_PropertyStates.m_Properties[sortedIndex].m_pToolProperty->m_bEmitAsGlobalProperty )
			continue;

		if ( !WriteProperty( &pProjectTool->m_PropertyStates.m_Properties[sortedIndex], szPlatformName, true, pConfiguration->m_Name.Get() ) )
			return false;
	}

	return true;
}

bool CProjectGenerator_Win32_2010::WriteFile( CProjectFile *pFile, const char *pFileTypeName, const char *szPlatformName )
{
	const char *pKeyName = GetKeyNameForFile( pFile );
	if ( V_stricmp_fast( pFileTypeName, pKeyName ) )
	{
		// skip it
		return true;
	}

	if ( !pFile->m_Configs.Count() )
	{
		m_XMLWriter.PushNode( pKeyName, CFmtStrMax( "Include=\"%s\"", pFile->m_Name.Get() ) );
		m_XMLWriter.PopNode();
	}
	else
	{
		m_XMLWriter.PushNode( pKeyName, CFmtStrMax( "Include=\"%s\"", pFile->m_Name.Get() ) );

		for ( int i = 0; i < pFile->m_Configs.Count(); i++ )
		{
			if ( !WriteConfiguration( pFile->m_Configs[i], szPlatformName ) )
				return false;
		}

		m_XMLWriter.PopNode();
	}
	
	return true;
}

bool CProjectGenerator_Win32_2010::WriteFolder( CProjectFolder *pFolder, const char *pFileTypeName, int nDepth, const char *szPlatformName )
{
	if ( !nDepth )
	{
		m_XMLWriter.PushNode( "ItemGroup" );
	}

	for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
	{
		if ( !WriteFile( pFolder->m_Files[iIndex], pFileTypeName, szPlatformName ) )
			return false;
	}

	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolder( pFolder->m_Folders[iIndex], pFileTypeName, nDepth+1, szPlatformName ) )
			return false;
	}

	if ( !nDepth )
	{
		m_XMLWriter.PopNode();
	}
	
	return true;
}

bool CProjectGenerator_Win32_2010::WriteConfiguration( CProjectConfiguration *pConfig, const char *szPlatformName )
{
	if ( !pConfig->m_bIsFileConfig )
	{
		m_XMLWriter.PushNode( "PropertyGroup", CFmtStr( "Condition=\"'$(Configuration)|$(Platform)'=='%s|%s'\" Label=\"Configuration\"", pConfig->m_Name.Get(), szPlatformName ) );

		for ( int i = 0; i < pConfig->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
		{
			int sortedIndex = pConfig->m_PropertyStates.m_PropertiesInOutputOrder[i];
			if ( pConfig->m_PropertyStates.m_Properties[sortedIndex].m_pToolProperty->m_bEmitAsGlobalProperty )
				continue;

			if ( !WriteProperty( &pConfig->m_PropertyStates.m_Properties[sortedIndex], szPlatformName ) )
				return false;
		}

		m_XMLWriter.PopNode();
	}
	else
	{
		for ( int i = 0; i < pConfig->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
		{
			int sortedIndex = pConfig->m_PropertyStates.m_PropertiesInOutputOrder[i];
			if ( !WriteProperty( &pConfig->m_PropertyStates.m_Properties[sortedIndex], szPlatformName, true, pConfig->m_Name.Get() ) )
				return false;
		}

        if ( !m_bGenerateMakefileVCXProj )
        {
            if ( !WriteTool( "ClCompile", pConfig->GetCompilerTool(), pConfig, szPlatformName ) )
                return false;

			if ( !WriteTool( "ResourceCompile", pConfig->GetResourcesTool(), pConfig, szPlatformName ) )
                return false;

            if ( !WriteTool( "CustomBuildStep", pConfig->GetCustomBuildTool(), pConfig, szPlatformName ) )
                return false;
        }
	}

	return true;
}

bool CProjectGenerator_Win32_2010::WriteTools( CProjectConfiguration *pConfig, const char *szPlatformName )
{
	m_XMLWriter.PushNode( "ItemDefinitionGroup", CFmtStr( "Condition=\"'$(Configuration)|$(Platform)'=='%s|%s'\"", pConfig->m_Name.Get(), szPlatformName ) );

    if ( !m_bGenerateMakefileVCXProj )
    {
        if ( !WriteTool( "PreBuildEvent", pConfig->GetPreBuildEventTool(), pConfig, szPlatformName ) )
            return false;
        
        if ( !WriteTool( "ClCompile", pConfig->GetCompilerTool(), pConfig, szPlatformName ) )
            return false;
        
        if ( !WriteTool( "ResourceCompile", pConfig->GetResourcesTool(), pConfig, szPlatformName ) )
            return false;
        
        if ( !WriteTool( "PreLinkEvent", pConfig->GetPreLinkEventTool(), pConfig, szPlatformName ) )
            return false;
        
        if ( !WriteTool( "Link", pConfig->GetLinkerTool(), pConfig, szPlatformName ) )
            return false;
        
        if ( !WriteTool( "Lib", pConfig->GetLibrarianTool(), pConfig, szPlatformName ) )
            return false;
        
        if ( !WriteTool( "Manifest", pConfig->GetManifestTool(), pConfig, szPlatformName ) )
            return false;
        
        if ( !WriteTool( "Xdcmake", pConfig->GetXMLDocGenTool(), pConfig, szPlatformName ) )
            return false;
        
        if ( !WriteTool( "Bscmake", pConfig->GetBrowseInfoTool(), pConfig, szPlatformName ) )
            return false;
        
        if ( !WriteTool( "PostBuildEvent", pConfig->GetPostBuildEventTool(), pConfig, szPlatformName ) )
            return false;
        
        if ( !WriteTool( "CustomBuildStep", pConfig->GetCustomBuildTool(), pConfig, szPlatformName ) )
            return false;
    }
	else
	{
		if ( !WriteTool( "Intellisense", pConfig->GetIntellisenseTool(), pConfig, szPlatformName ) )
            return false;
	}

	m_XMLWriter.PopNode();

	return true;
}

bool CProjectGenerator_Win32_2010::WritePrimaryXML( const char *pOutputFilename, const char *szPlatformName )
{
	if ( !m_XMLWriter.Open( pOutputFilename, true, g_pVPC->IsForceGenerate() ) )
		return false;
	
	m_XMLWriter.PushNode( "Project", "DefaultTargets=\"Build\" ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\"" );

	m_XMLWriter.PushNode( "ItemGroup", "Label=\"ProjectConfigurations\"" );
	CUtlVector< CUtlString > configurationNames;
	m_pVCProjGenerator->GetAllConfigurationNames( configurationNames ); 
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		m_XMLWriter.PushNode( "ProjectConfiguration" );
		{
			m_XMLWriter.AddNodeProperty( "Include", CFmtStr( "%s|%s", configurationNames[i].Get(), szPlatformName ) );

			m_XMLWriter.WriteLineNode( "Configuration", "", configurationNames[i].Get() );
			m_XMLWriter.WriteLineNode( "Platform", "", szPlatformName );
		}
		m_XMLWriter.PopNode();
	}
	m_XMLWriter.PopNode();

	m_XMLWriter.PushNode( "PropertyGroup", "Label=\"Globals\"" );
	m_XMLWriter.WriteLineNode( "ProjectName", "", m_pVCProjGenerator->GetProjectName() );
	m_XMLWriter.WriteLineNode( "ProjectGuid", "", CFmtStr( "{%s}", m_pVCProjGenerator->GetGUIDString() ) );
	// Suppress bogus warning MSB8027 in schemacompiler files in VS 2013 caused by this bug:
	// https://connect.microsoft.com/VisualStudio/feedback/details/797460/incorrect-warning-msb8027-reported-for-files-excluded-from-build
	m_XMLWriter.WriteLineNode( "IgnoreWarnCompileDuplicatedFilename", "", "true" ); 

	if ( g_pVPC->IsSourceControlEnabled() )
	{
		m_XMLWriter.WriteLineNode( "SccProjectName", "", "Perforce Project" );
		// it looks like 2k10 (at least) doesn't hook files in the project but not under
		// the project root into source control, so make all the projects local paths
		// the solution dir
		char szCurrentDirectory[MAX_FIXED_PATH];
		V_GetCurrentDirectory( szCurrentDirectory, V_ARRAYSIZE( szCurrentDirectory ) );
		char szRelativeFilename[MAX_FIXED_PATH];
		if ( !V_MakeRelativePath( g_pVPC->GetStartDirectory(), szCurrentDirectory, szRelativeFilename, sizeof( szRelativeFilename ) ) )
		{
			V_strncpy( szRelativeFilename, ".", V_ARRAYSIZE( szRelativeFilename ) );
		}
		m_XMLWriter.WriteLineNode( "SccLocalPath", "", szRelativeFilename );
		m_XMLWriter.WriteLineNode( "SccProvider", "", "MSSCCI:Perforce SCM" );
	}

	m_XMLWriter.PopNode();

	m_XMLWriter.PushNode( "Import", "Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\"" );
	m_XMLWriter.PopNode();

	// write the root configurations
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		CProjectConfiguration *pConfiguration = NULL;
		if ( m_pVCProjGenerator->GetRootConfiguration( configurationNames[i].Get(), &pConfiguration ) )
		{
			if ( !WriteConfiguration( pConfiguration, szPlatformName ) )
				return false;
		}
	}

	m_XMLWriter.PushNode( "Import", "Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\"" );
	m_XMLWriter.PopNode();

	m_XMLWriter.PushNode( "ImportGroup", "Label=\"ExtensionSettings\"" );
	m_XMLWriter.PopNode();

	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		m_XMLWriter.PushNode( "ImportGroup", CFmtStr( "Condition=\"'$(Configuration)|$(Platform)'=='%s|%s'\" Label=\"PropertySheets\"", configurationNames[i].Get(), szPlatformName ) );
		m_XMLWriter.PushNode( "Import" );
		m_XMLWriter.AddNodeProperty( "Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\"" );
		m_XMLWriter.AddNodeProperty( "Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\"" );
		m_XMLWriter.AddNodeProperty( "Label=\"LocalAppDataPlatform\"" );
		m_XMLWriter.PopNode();
		m_XMLWriter.PopNode();
	}
	
	m_XMLWriter.PushNode( "PropertyGroup", "Label=\"UserMacros\"" );
	m_XMLWriter.PopNode();

	m_XMLWriter.PushNode( "PropertyGroup" );
	m_XMLWriter.WriteLineNode( "_ProjectFileVersion", "", "10.0.30319.1" );
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		CProjectConfiguration *pConfiguration = NULL;
		if ( m_pVCProjGenerator->GetRootConfiguration( configurationNames[i].Get(), &pConfiguration ) )
		{
			for ( int j = 0; j < pConfiguration->m_PropertyStates.m_PropertiesInOutputOrder.Count(); j++ )
			{
				int sortedIndex = pConfiguration->m_PropertyStates.m_PropertiesInOutputOrder[j];
				if ( !pConfiguration->m_PropertyStates.m_Properties[sortedIndex].m_pToolProperty->m_bEmitAsGlobalProperty )
					continue;

				if ( !WriteProperty( &pConfiguration->m_PropertyStates.m_Properties[sortedIndex], szPlatformName, true, pConfiguration->m_Name.Get() ) )
					return false;
			}

			if ( !WritePropertyGroupTool( pConfiguration->GetPreBuildEventTool(), pConfiguration, szPlatformName ) )
				return false;

			if ( !WritePropertyGroupTool( pConfiguration->GetPreLinkEventTool(), pConfiguration, szPlatformName ) )
				return false;

			if ( !WritePropertyGroupTool( pConfiguration->GetLinkerTool(), pConfiguration, szPlatformName ) )
				return false;

			if ( !WritePropertyGroupTool( pConfiguration->GetLibrarianTool(), pConfiguration, szPlatformName ) )
				return false;

			if ( !WritePropertyGroupTool( pConfiguration->GetPostBuildEventTool(), pConfiguration, szPlatformName ) )
				return false;

			if ( !WritePropertyGroupTool( pConfiguration->GetCustomBuildTool(), pConfiguration, szPlatformName ) )
				return false;

			if ( !WritePropertyGroupTool( pConfiguration->GetIntellisenseTool(), pConfiguration, szPlatformName ) )
				return false;
		}
	}
	m_XMLWriter.PopNode();

	// write the tool configurations
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		CProjectConfiguration *pConfiguration = NULL;
		if ( m_pVCProjGenerator->GetRootConfiguration( configurationNames[i].Get(), &pConfiguration ) )
		{
			if ( !WriteTools( pConfiguration, szPlatformName ) )
				return false;
		}
	}

	// write root folders
	for ( int i = 0; i < TKN_MAX_COUNT; i++ )
	{
		if ( !WriteFolder( m_pVCProjGenerator->GetRootFolder(), s_TypeKeyNames[i], 0, szPlatformName ) )
			return false;
	}
	
	m_XMLWriter.PushNode( "Import", "Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\"" );
	m_XMLWriter.PopNode();

	m_XMLWriter.PushNode( "ImportGroup", "Label=\"ExtensionTargets\"" );
	m_XMLWriter.PopNode();

	m_XMLWriter.PopNode();

	m_XMLWriter.Close();

	return true;
}

bool CProjectGenerator_Win32_2010::WriteFolderToSecondaryXML( CProjectFolder *pFolder, const char *pParentPath )
{
	CUtlString parentPath = CFmtStrMax( "%s%s%s", pParentPath, pParentPath[0] ? "\\" : "", pFolder->m_Name.Get() ).Get();
	CUtlString lowerParentPath = parentPath;
	lowerParentPath.ToLower();

	MD5Context_t ctx;
	unsigned char digest[MD5_DIGEST_LENGTH];
	V_memset( &ctx, 0, sizeof( ctx ) );
	V_memset( digest, 0, sizeof( digest ) );
	MD5Init( &ctx );
	MD5Update( &ctx, (unsigned char *)lowerParentPath.Get(), V_strlen( lowerParentPath.Get() ) );
	MD5Final( digest, &ctx );

	char szMD5[64];
	V_binarytohex( digest, MD5_DIGEST_LENGTH, szMD5, sizeof( szMD5 ) );
	V_strupper( szMD5 );

	char szGUID[100];
	V_snprintf( szGUID, sizeof( szGUID ), "{%8.8s-%4.4s-%4.4s-%4.4s-%12.12s}", szMD5, &szMD5[8], &szMD5[12], &szMD5[16], &szMD5[20] );

	m_XMLFilterWriter.PushNode( "Filter", CFmtStrMax( "Include=\"%s\"", parentPath.Get() ) );
	m_XMLFilterWriter.WriteLineNode( "UniqueIdentifier", "", szGUID );
	m_XMLFilterWriter.PopNode();

	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolderToSecondaryXML( pFolder->m_Folders[iIndex], parentPath.Get() ) )
			return false;
	}

	return true;
}

bool CProjectGenerator_Win32_2010::WriteFileToSecondaryXML( CProjectFile *pFile, const char *pParentPath, const char *pFileTypeName )
{
	const char *pKeyName = GetKeyNameForFile( pFile );
	if ( V_stricmp_fast( pFileTypeName, pKeyName ) )
	{
		// skip it
		return true;
	}

	if ( pParentPath )
	{
		m_XMLFilterWriter.PushNode( pKeyName, CFmtStrMax( "Include=\"%s\"", pFile->m_Name.Get() ) );
		m_XMLFilterWriter.WriteLineNode( "Filter", "", pParentPath );
		m_XMLFilterWriter.PopNode();
	}
	else
	{
		m_XMLFilterWriter.PushNode( pKeyName, CFmtStrMax( "Include=\"%s\"", pFile->m_Name.Get() ) );
		m_XMLFilterWriter.PopNode();
	}
	
	return true;
}

bool CProjectGenerator_Win32_2010::WriteFolderContentsToSecondaryXML( CProjectFolder *pFolder, const char *pParentPath, const char *pFileTypeName, int nDepth )
{
	CUtlString parentPath;
	if ( pParentPath )
	{
		parentPath = CFmtStrMax( "%s%s%s", pParentPath, pParentPath[0] ? "\\" : "", pFolder->m_Name.Get() );
	}

	if ( !nDepth )
	{
		m_XMLFilterWriter.PushNode( "ItemGroup", NULL );
	}

	for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
	{
		if ( !WriteFileToSecondaryXML( pFolder->m_Files[iIndex], parentPath.Get(), pFileTypeName ) )
			return false;
	}

	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolderContentsToSecondaryXML( pFolder->m_Folders[iIndex], parentPath.Get(), pFileTypeName, nDepth+1 ) )
			return false;
	}

	if ( !nDepth )
	{
		m_XMLFilterWriter.PopNode();
	}

	return true;
}

bool CProjectGenerator_Win32_2010::WriteSecondaryXML( const char *pOutputFilename )
{
	if ( !m_XMLFilterWriter.Open( pOutputFilename, true, g_pVPC->IsForceGenerate() ) )
			return false;

	m_XMLFilterWriter.PushNode( "Project", "ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\"" );

	// write the root folders
	m_XMLFilterWriter.PushNode( "ItemGroup", NULL );
	CProjectFolder *pRootFolder = m_pVCProjGenerator->GetRootFolder();
	for ( int iIndex = pRootFolder->m_Folders.Head(); iIndex != pRootFolder->m_Folders.InvalidIndex(); iIndex = pRootFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolderToSecondaryXML( pRootFolder->m_Folders[iIndex], "" ) )
			return false;
	}
	m_XMLFilterWriter.PopNode();

	// write folder contents
	for ( int i = 0; i < TKN_MAX_COUNT; i++ )
	{
		if ( !WriteFolderContentsToSecondaryXML( pRootFolder, NULL, s_TypeKeyNames[i], 0 ) )
			return false;
	}

	m_XMLFilterWriter.PopNode();

	m_XMLFilterWriter.Close();

	return true;
}

bool CProjectGenerator_Win32_2010::WriteTool( const char *pToolName, const CProjectTool *pProjectTool, CProjectConfiguration *pConfig, const char *szPlatformName )
{
	if ( !pProjectTool )
	{
		// not an error, some tools n/a for a config
		return true;
	}

	if ( !pConfig->m_bIsFileConfig )
	{
		m_XMLWriter.PushNode( pToolName, NULL );
	}

	for ( int i = 0; i < pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
	{
		int sortedIndex = pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder[i];
		if ( !pConfig->m_bIsFileConfig )
		{
			if ( pProjectTool->m_PropertyStates.m_Properties[sortedIndex].m_pToolProperty->m_bEmitAsGlobalProperty )
				continue;

			if ( !WriteProperty( &pProjectTool->m_PropertyStates.m_Properties[sortedIndex], szPlatformName ) )
				return false;
		}
		else
		{
			if ( !WriteProperty( &pProjectTool->m_PropertyStates.m_Properties[sortedIndex], szPlatformName, true, pConfig->m_Name.Get() ) )
				return false;
		}
	}

	if ( !pConfig->m_bIsFileConfig )
	{
		m_XMLWriter.PopNode();
	}
	
	return true;
}	

bool CProjectGenerator_Win32_2010::WriteProperty( const PropertyState_t *pPropertyState, const char *szPlatformName, bool bEmitConfiguration, const char *pConfigName, const char *pOutputName, const char *pOutputValue )
{
	if ( !pPropertyState )
	{
		m_XMLWriter.WriteLineNode( pOutputName, "", pOutputValue );
		return true;
	}

	if ( !pOutputName )
	{
		pOutputName = pPropertyState->m_pToolProperty->m_OutputString.Get();
		if ( !pOutputName[0] )
		{
			pOutputName = pPropertyState->m_pToolProperty->m_ParseString.Get();
			if ( pOutputName[0] == '$' )
			{
				pOutputName++;
			}
		}
	}

	const char *pCondition = "";
	CUtlString conditionString;
	if ( bEmitConfiguration )
	{
		conditionString = CFmtStr( " Condition=\"'$(Configuration)|$(Platform)'=='%s|%s'\"", pConfigName, szPlatformName );
		pCondition = conditionString.Get();
	}

	if ( pPropertyState && !pPropertyState->m_pToolProperty->m_bIgnoreForOutput )
	{
		const char *pValueStr = pPropertyState->m_StringValue.Get();
		CUtlString generatedValueBuffer;
		if ( pPropertyState->m_pToolProperty->m_bGeneratedOnOutput && GenerateToolProperty( pOutputName, pValueStr, generatedValueBuffer, pPropertyState, szPlatformName, pConfigName ) )
		{
			pValueStr = generatedValueBuffer.Get();
		}

		switch ( pPropertyState->m_pToolProperty->m_nType )
		{
		case PT_BOOLEAN:
			{
				bool bEnabled = Sys_StringToBool( pValueStr );
				if ( pPropertyState->m_pToolProperty->m_bInvertOutput )
				{
					bEnabled ^= 1;
				}
				m_XMLWriter.WriteLineNode( pOutputName, pCondition, bEnabled ? "true" : "false" );
			}
			break;

		case PT_STRING:
			m_XMLWriter.WriteLineNode( pOutputName, pCondition, m_XMLWriter.FixupXMLString( pValueStr ).Get() );
			break;

		case PT_LIST:
		case PT_INTEGER:
            // When we're generating a makefile wrapper we have to
            // force the configuration type to Makefile.  We can't
            // do this in vpc scripts themselves as the makefile
            // output pass needs the correct configuration type.
            if ( m_bGenerateMakefileVCXProj && !V_stricmp_fast( pOutputName, "ConfigurationType" ) )
            {
                pValueStr = "Makefile";
            }
			m_XMLWriter.WriteLineNode( pOutputName, pCondition, pValueStr );
			break;

		case PT_IGNORE:
			break;

		default:
			g_pVPC->VPCError( "CProjectGenerator_Win32_2010: WriteProperty, %s - not implemented", pOutputName );
		}
	}

	return true;
}

bool CProjectGenerator_Win32_2010::WriteVisualGDBSettings( const char *pConfiguration )
{
	const char *pConfigType = m_pVCProjGenerator->
        GetPropertyValueAsString( NULL, pConfiguration, KEYWORD_GENERAL, g_pOption_ConfigurationType );
    if ( !V_stristr( pConfigType, ".exe" ) )
    {
        return true;
    }

    CUtlPathStringHolder finalSettingsFile( g_pVPC->FormatTemp1( "%s-%s.vgdbsettings", GetProjectGenerator()->GetOutputFileName(), pConfiguration ) );
    // Print just the filename first for status output.
    g_pVPC->VPCStatus( true, "Saving... Project: '%s' File: '%s'",
                       g_pVPC->GetProjectName(), finalSettingsFile.Get() );

    // Now create the full path.
    finalSettingsFile.Set( g_pVPC->FormatTemp1( "%s/%s-%s.vgdbsettings", g_pVPC->GetProjectPath(), GetProjectGenerator()->GetOutputFileName(), pConfiguration ) );
    finalSettingsFile.FixSlashes();

    CUtlPathStringHolder settingsPath( g_pVPC->GetSourcePath(), "/devtools/vpc.vgdbsettings" );
    settingsPath.FixSlashes();

    CUtlBuffer settingsBuf;
    if ( !Sys_LoadFileIntoBuffer( settingsPath, settingsBuf, true ) )
    {
        g_pVPC->VPCError( "Cannot open .vgdbsettings file '%s'", settingsPath.Get() );
        return false;
    }

    CUtlStringBuilder *pStrBuf = g_pVPC->GetTempStringBuffer1();
	const char *pTargetOutput = m_pVCProjGenerator->
        GetPropertyValueAsString( NULL, pConfiguration, KEYWORD_LINKER, g_pOption_OutputFile );
    V_MakeAbsolutePath( pStrBuf->Access(), pStrBuf->Capacity(), pTargetOutput, g_pVPC->GetProjectPath() );

    pStrBuf->ReplaceFastCaseless( g_pVPC->GetSourcePath(), "$(LinuxDebugRoot)/src" );
    V_FixSlashes( pStrBuf->Access(), '/' );

    CUtlStringBuilder strSettings( (char*)settingsBuf.Base() );
    strSettings.Replace( "$$CONFIGURATION$$", pConfiguration );
    strSettings.Replace( "$$TARGET$$", pStrBuf->Get() );

    const char *pEnv;

    pEnv = getenv( "LINUX_DEBUG_HOST_NAME" );
    if ( pEnv != NULL )
    {
        strSettings.Replace( "$(LinuxDebugHostName)", pEnv );
    }
    pEnv = getenv( "LINUX_DEBUG_USER_NAME" );
    if ( pEnv != NULL )
    {
        strSettings.Replace( "$(LinuxDebugUserName)", pEnv );
    }
    pEnv = getenv( "LINUX_DEBUG_ROOT" );
    if ( pEnv != NULL )
    {
        strSettings.Replace( "$(LinuxDebugRoot)", pEnv );
    }
        
	FILE *pFile = fopen( finalSettingsFile, "wt" );
	if ( !pFile )
	{
        g_pVPC->VPCError( "Cannot open '%s' for writing", finalSettingsFile.Get() );
        return false;
    }

    fwrite( strSettings, 1, V_strlen( strSettings ), pFile );
    fclose( pFile );

    return true;
}

bool CProjectGenerator_Win32_2010::Save( const char *pOutputFilename )
{
	bool bValid = true;

	m_bGenerateMakefileVCXProj = g_pVPC->IsConditionalDefined( "GENERATE_MAKEFILE_VCXPROJ" );
	m_bVisualGDB = g_pVPC->IsConditionalDefined( "VISUALGDB" );
	const char *szPlatformName = GetVCProjTargetPlatformName( g_pVPC->GetTargetPlatformName() );
    
    if ( m_bGenerateMakefileVCXProj && m_bVisualGDB )
    {
        CUtlVector< CUtlString > vConfigurationNames;
        m_pVCProjGenerator->GetAllConfigurationNames( vConfigurationNames ); 
        for ( int i = 0; i < vConfigurationNames.Count(); i++ )
        {
            if ( !WriteVisualGDBSettings( vConfigurationNames[i] ) )
            {
                bValid = false;
                break;
            }
        }
    }

    if ( bValid )
    {
        bValid = WritePrimaryXML( pOutputFilename, szPlatformName );
    }
    
	if ( bValid )
	{
		bValid = WriteSecondaryXML( CFmtStrMax( "%s.filters", pOutputFilename ) );
		if ( !bValid )
		{
			g_pVPC->VPCError( "Cannot save to the specified project '%s'", pOutputFilename );
		}
	}

	return bValid;
}

bool CProjectGenerator_Win32_2010::GenerateToolProperty( const char *pOutputName, const char *pScriptValue, CUtlString &outputWrite, const PropertyState_t *pPropertyState, const char *szPlatformName, const char *pConfigName )
{
	CProjectConfiguration *pRootConfig = nullptr;
	if ( pConfigName )
	{
		if ( !m_pVCProjGenerator->GetRootConfiguration( pConfigName, &pRootConfig ) || !pRootConfig )
		{
			g_pVPC->VPCError( "Could not get config \"%s\"", pConfigName );
			UNREACHABLE();
		}
	}
	else
	{
		CUtlVector<CProjectConfiguration *> rootConfigs;
		m_pVCProjGenerator->GetAllRootConfigurations( rootConfigs );
		if ( rootConfigs.Count() == 0 )
		{
			if ( !m_pVCProjGenerator->GetRootConfiguration( pConfigName, &pRootConfig ) || !pRootConfig )
			{
				g_pVPC->VPCError( "No configs found" );
				UNREACHABLE();
			}
		}
		pRootConfig = rootConfigs[0];
	}

	Assert( outputWrite.IsEmpty() );
	if ( !V_stricmp_fast( pOutputName, "NMakePreprocessorDefinitions" ) )
	{
		CUtlString cfgString;
		if ( VPC_GetPropertyString( KEYWORD_COMPILER, pRootConfig, nullptr, g_pOption_PreprocessorDefinitions, &cfgString ) && !cfgString.IsEmpty() )
		{
			outputWrite = pScriptValue;
			outputWrite += cfgString;
			return true;
		}
		return false;
	}
	else if ( !V_stricmp_fast( pOutputName, "NMakeIncludeSearchPath" ) )
	{
		CUtlString cfgString;
		if ( VPC_GetPropertyString( KEYWORD_COMPILER, pRootConfig, nullptr, g_pOption_AdditionalIncludeDirectories, &cfgString ) && !cfgString.IsEmpty() )
		{
			outputWrite = cfgString;
			if ( pScriptValue && pScriptValue[0] )
			{
				outputWrite += ";";
				outputWrite += pScriptValue;
			}
			outputWrite.FixSlashes();
			return true;
		}
		return false;
	}
	else
	{
		g_pVPC->VPCWarning( "No property generator defined for \"%s\"", pOutputName );
		return false;
	}
	UNREACHABLE();
}

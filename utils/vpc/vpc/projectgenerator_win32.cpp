//========= Copyright © 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

extern const char *GetVCProjTargetPlatformName( const char *szVPCTargetPlatformName );

#undef PROPERTYNAME
#define PROPERTYNAME( X, Y ) { X##_##Y, #X, #Y },
static PropertyName_t s_Win32PropertyNames[] =
{
	#include "projectgenerator_win32.inc"
	{ -1, NULL, NULL }
};

IBaseProjectGenerator* GetWin32ProjectGenerator()
{
	CProjectGenerator_Win32 *pNew = new CProjectGenerator_Win32;
	return pNew->GetProjectGenerator();
}

CProjectGenerator_Win32::CProjectGenerator_Win32()
{
	m_pVCProjGenerator = new CVCProjGenerator();
	m_pVCProjGenerator->SetupGeneratorDefinition( "win32_2005.def", s_Win32PropertyNames );
	m_pVCProjGenerator->AddProjectWriter( this );
}

bool CProjectGenerator_Win32::WriteFile( CProjectFile *pFile )
{
	m_XMLWriter.PushNode( "File" );
	m_XMLWriter.AddNodeProperty( CFmtStrMax( "RelativePath=\"%s\"", pFile->m_Name.Get() ) );

	for ( int i = 0; i < pFile->m_Configs.Count(); i++ )
	{
		if ( !WriteConfiguration( pFile->m_Configs[i] ) )
			return false;
	}

	m_XMLWriter.PopNode();
	
	return true;
}

bool CProjectGenerator_Win32::WriteFolder( CProjectFolder *pFolder )
{
	m_XMLWriter.PushNode( "Filter" );
	m_XMLWriter.AddNodeProperty( CFmtStrMax( "Name=\"%s\"", m_XMLWriter.FixupXMLString( pFolder->m_Name.Get() ).Get() ) );

	for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
	{
		if ( !WriteFile( pFolder->m_Files[iIndex] ) )
			return false;
	}

	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolder( pFolder->m_Folders[iIndex] ) )
			return false;
	}

	m_XMLWriter.PopNode();
	
	return true;
}

bool CProjectGenerator_Win32::WriteConfiguration( CProjectConfiguration *pConfig )
{
	const char *szVisualStudioPlatformName = GetVCProjTargetPlatformName( g_pVPC->GetTargetPlatformName() );

	if ( pConfig->m_bIsFileConfig )
	{
		m_XMLWriter.PushNode( "FileConfiguration" );
	}
	else
	{
		m_XMLWriter.PushNode( "Configuration" );
	}

	const char *pOutputName = "???";
	if ( !V_stricmp_fast( pConfig->m_Name.Get(), "debug" ) )
	{
		pOutputName = "Debug";
	}
	else if ( !V_stricmp_fast( pConfig->m_Name.Get(), "release" ) )
	{
		pOutputName = "Release";
	}
	else
	{
		return false;
	}

	m_XMLWriter.AddNodeProperty( CFmtStrMax( "Name=\"%s|%s\"", pOutputName, szVisualStudioPlatformName ) );

	// write configuration properties
	for ( int i = 0; i < pConfig->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
	{
		int sortedIndex = pConfig->m_PropertyStates.m_PropertiesInOutputOrder[i];
		WriteProperty( &pConfig->m_PropertyStates.m_Properties[sortedIndex] );
	}

	if ( !WriteTool( "VCPreBuildEventTool", pConfig->GetPreBuildEventTool() ) )
		return false;

	if ( !WriteTool( "VCCustomBuildTool", pConfig->GetCustomBuildTool() ) )
		return false;

	if ( !WriteNULLTool( "VCXMLDataGeneratorTool", pConfig ) )
		return false;

	if ( !WriteNULLTool( "VCWebServiceProxyGeneratorTool", pConfig ) )
		return false;

	if ( !WriteNULLTool( "VCMIDLTool", pConfig ) )
		return false;

	if ( !WriteTool( "VCCLCompilerTool", pConfig->GetCompilerTool() ) )
		return false;

	if ( !WriteNULLTool( "VCManagedResourceCompilerTool", pConfig ) )
		return false;

	if ( !WriteTool( "VCResourceCompilerTool", pConfig->GetResourcesTool() ) )
		return false;

	if ( !WriteTool( "VCPreLinkEventTool", pConfig->GetPreLinkEventTool() ) )
		return false;

	if ( !WriteTool( "VCLinkerTool", pConfig->GetLinkerTool() ) )
		return false;

	if ( !WriteTool( "VCLibrarianTool", pConfig->GetLibrarianTool() ) )
		return false;

	if ( !WriteNULLTool( "VCALinkTool", pConfig ) )
		return false;

	if ( !WriteTool( "VCManifestTool",  pConfig->GetManifestTool() ) )
		return false;

	if ( !WriteTool( "VCXDCMakeTool", pConfig->GetXMLDocGenTool() ) )
		return false;

	if ( !WriteTool( "VCBscMakeTool", pConfig->GetBrowseInfoTool() ) )
		return false;

	if ( !WriteNULLTool( "VCFxCopTool", pConfig ) )
		return false;

	if ( !pConfig->GetLibrarianTool() )
	{
		if ( !WriteNULLTool( "VCAppVerifierTool", pConfig ) )
			return false;

		if ( !WriteNULLTool( "VCWebDeploymentTool", pConfig ) )
			return false;
	}

	if ( !WriteTool( "VCPostBuildEventTool", pConfig->GetPostBuildEventTool() ) )
		return false;
	
	m_XMLWriter.PopNode();

	return true;
}

bool CProjectGenerator_Win32::WriteToXML()
{
	const char *szVisualStudioPlatformName = GetVCProjTargetPlatformName( g_pVPC->GetTargetPlatformName() );

	m_XMLWriter.PushNode( "VisualStudioProject" );
	m_XMLWriter.AddNodeProperty( "ProjectType=\"Visual C++\"" );
	m_XMLWriter.AddNodeProperty( "Version=\"8.00\"" );
	m_XMLWriter.AddNodeProperty( CFmtStrMax( "Name=\"%s\"", m_pVCProjGenerator->GetProjectName() ) );
	m_XMLWriter.AddNodeProperty( "ProjectGUID", CFmtStr( "{%s}", m_pVCProjGenerator->GetGUIDString() ) );

	if ( g_pVPC->IsSourceControlEnabled() )
	{
		m_XMLWriter.AddNodeProperty( "SccProjectName=\"Perforce Project\"" );
		m_XMLWriter.AddNodeProperty( "SccLocalPath=\"..\"" );
		m_XMLWriter.AddNodeProperty( "SccProvider=\"MSSCCI:Perforce SCM\"\n" );
	}

	m_XMLWriter.PushNode( "Platforms" );
	m_XMLWriter.AddNodeProperty( CFmtStr( "<Platform Name=\"%s\"/>", szVisualStudioPlatformName ).Get() );
	m_XMLWriter.PopNode();

	m_XMLWriter.PushNode( "ToolFiles" );
	m_XMLWriter.PopNode();

	CUtlVector< CUtlString > configurationNames;
	m_pVCProjGenerator->GetAllConfigurationNames( configurationNames ); 

	// write the root configurations
	m_XMLWriter.PushNode( "Configurations" );
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		CProjectConfiguration *pConfiguration = NULL;
		if ( m_pVCProjGenerator->GetRootConfiguration( configurationNames[i].Get(), &pConfiguration ) )
		{
			if ( !WriteConfiguration( pConfiguration ) )
				return false;
		}
	}
	m_XMLWriter.PopNode();

	m_XMLWriter.PushNode( "References" );
	m_XMLWriter.PopNode();

	m_XMLWriter.PushNode( "Files" );

	CProjectFolder *pRootFolder = m_pVCProjGenerator->GetRootFolder();

	for ( int iIndex = pRootFolder->m_Folders.Head(); iIndex != pRootFolder->m_Folders.InvalidIndex(); iIndex = pRootFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolder( pRootFolder->m_Folders[iIndex] ) )
			return false;
	}

	for ( int iIndex = pRootFolder->m_Files.Head(); iIndex != pRootFolder->m_Files.InvalidIndex(); iIndex = pRootFolder->m_Files.Next( iIndex ) )
	{
		if ( !WriteFile( pRootFolder->m_Files[iIndex] ) )
			return false;
	}

	m_XMLWriter.PopNode();

	m_XMLWriter.PopNode();

	return true;
}

bool CProjectGenerator_Win32::Save( const char *pOutputFilename )
{
	if ( !m_XMLWriter.Open( pOutputFilename, false, g_pVPC->IsForceGenerate() ) )
		return false;

	bool bValid = WriteToXML();

	m_XMLWriter.Close();

	return bValid;
}

bool CProjectGenerator_Win32::WriteNULLTool( const char *pToolName, const CProjectConfiguration *pConfig )
{
	if ( pConfig->m_bIsFileConfig )
		return true;

	m_XMLWriter.PushNode( "Tool", CFmtStr( "Name=\"%s\"", pToolName ).Get() );
	m_XMLWriter.PopNode();

	return true;
}

bool CProjectGenerator_Win32::WriteTool( const char *pToolName, const CProjectTool *pProjectTool )
{
	if ( !pProjectTool )
	{
		// not an error, some tools n/a for a config
		return true;
	}

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.AddNodeProperty( CFmtStr( "Name=\"%s\"", pToolName ) );

	for ( int i = 0; i < pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder.Count(); i++ )
	{
		int sortedIndex = pProjectTool->m_PropertyStates.m_PropertiesInOutputOrder[i];
		WriteProperty( &pProjectTool->m_PropertyStates.m_Properties[sortedIndex] );
	}

	m_XMLWriter.PopNode();
	
	return true;
}	

bool CProjectGenerator_Win32::WriteProperty( const PropertyState_t *pPropertyState, const char *pOutputName, const char *pOutputValue )
{
	if ( !pPropertyState )
	{
		m_XMLWriter.AddNodeProperty( CFmtStrMax( "%s=\"%s\"", pOutputName, pOutputValue ) );
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

	if ( pPropertyState && !pPropertyState->m_pToolProperty->m_bIgnoreForOutput )
	{
		switch ( pPropertyState->m_pToolProperty->m_nType )
		{
		case PT_BOOLEAN:
			{
				bool bEnabled = Sys_StringToBool( pPropertyState->m_StringValue.Get() );
				if ( pPropertyState->m_pToolProperty->m_bInvertOutput )
				{
					bEnabled ^= 1;
				}
				m_XMLWriter.AddNodeProperty( CFmtStrMax( "%s=\"%s\"", pOutputName, bEnabled ? "true" : "false" ) );
			}
			break;

		case PT_STRING:
			m_XMLWriter.AddNodeProperty( CFmtStrMax( "%s=\"%s\"", pOutputName, m_XMLWriter.FixupXMLString( pPropertyState->m_StringValue.Get() ).Get() ) );
			break;

		case PT_LIST:
		case PT_INTEGER:
			m_XMLWriter.AddNodeProperty( CFmtStrMax( "%s=\"%s\"", pOutputName, pPropertyState->m_StringValue.Get() ) );
			break;

		case PT_IGNORE:
			break;

		default:
			g_pVPC->VPCError( "CProjectGenerator_Win32: WriteProperty, %s - not implemented", pOutputName );
		}
	}

	return true;
}

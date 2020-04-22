//===== Copyright (c) 1996-2016, Valve Corporation, All rights reserved. ======
//
// Purpose: 
//
//=============================================================================

#include "vpc.h"
#include "baseprojectdatacollector.h"
#include "tier1/utlstack.h"
#include "p4lib/ip4.h"


// ------------------------------------------------------------------------------------------------ //
// CSpecificConfig implementation.
// ------------------------------------------------------------------------------------------------ //

CSpecificConfig::CSpecificConfig( CSpecificConfig *pParentConfig )
	: m_pParentConfig( pParentConfig )
{
	m_pKV = new KeyValues( "" );
	m_bFileExcluded = false;
}

CSpecificConfig::~CSpecificConfig()
{
	m_pKV->deleteThis();
}

const char* CSpecificConfig::GetConfigName()
{
	return m_pKV->GetName();
}

const char* CSpecificConfig::GetOption( const char *pOptionName, const char *pDefaultValue /*= nullptr*/ )
{
	const char *pRet = m_pKV->GetString( pOptionName, NULL );
	if ( pRet )
		return pRet;

	if ( m_pParentConfig )
		return m_pParentConfig->m_pKV->GetString( pOptionName, NULL );

	return pDefaultValue;
}


// ------------------------------------------------------------------------------------------------ //
// CFileConfig implementation.
// ------------------------------------------------------------------------------------------------ //

CFileConfig::~CFileConfig()
{
	Term();
}

void CFileConfig::Term()
{
	m_Configurations.PurgeAndDeleteElements();
	m_Filename.Clear();
}

const char* CFileConfig::GetName()
{
	return m_Filename.String();
}

CSpecificConfig* CFileConfig::GetConfig( const char *pConfigName )
{
	int i = m_Configurations.Find( pConfigName );
	if ( i == m_Configurations.InvalidIndex() )
		return NULL;
	else
		return m_Configurations[i];
}

CSpecificConfig* CFileConfig::GetOrCreateConfig( const char *pConfigName, CSpecificConfig *pParentConfig )
{
	int i = m_Configurations.Find( pConfigName );
	if ( i == m_Configurations.InvalidIndex() )
	{
		CSpecificConfig *pConfig = new CSpecificConfig( pParentConfig );
		pConfig->m_pKV->SetName( pConfigName );
		i = m_Configurations.Insert( pConfigName, pConfig );
	}

	return m_Configurations[i];
}

bool CFileConfig::IsExcludedFrom( const char *pConfigName )
{
	CSpecificConfig *pSpecificConfig = GetConfig( pConfigName );
	if ( pSpecificConfig )
		return pSpecificConfig->m_bFileExcluded;
	else
		return false;
}



// ------------------------------------------------------------------------------------------------ //
// CBaseProjectDataCollector implementation.
// ------------------------------------------------------------------------------------------------ //

CBaseProjectDataCollector::CBaseProjectDataCollector( CRelevantPropertyNames *pNames )
 :	m_BaseConfigData( "", VPC_FILE_FLAGS_NONE ),
	m_Files( k_eDictCompareTypeFilenames )
{
	m_RelevantPropertyNames.m_nNames = 0;
	m_RelevantPropertyNames.m_pNames = NULL;

	if ( pNames )
	{
		m_RelevantPropertyNames = *pNames;
	}
}

CBaseProjectDataCollector::~CBaseProjectDataCollector()
{
	Term();
}

void CBaseProjectDataCollector::StartProject()
{
	m_ProjectName = "UNNAMED";
	m_CurFileConfig.Push( &m_BaseConfigData );
	m_CurSpecificConfig.Push( NULL );

	// TODO: none of these support non-WIN32 platforms yet (this code emits the appropriate warnings)
	g_pVPC->ShouldEmitClangProject();
}

void CBaseProjectDataCollector::EndProject( bool bSaveData )
{
	if ( g_pVPC->GetMissingFilesCount() > 0 )
	{
		if ( g_pVPC->IsMissingFileAsErrorEnabled() )
		{
			g_pVPC->VPCError( "%d files missing.", g_pVPC->GetMissingFilesCount() );
		}
		else
		{
			g_pVPC->VPCWarning( "%d files missing.", g_pVPC->GetMissingFilesCount() );
		}
	}

	VPC_GenerateProjectDependencies( this );
}

void CBaseProjectDataCollector::Term()
{
	m_BaseConfigData.Term();
	m_Files.PurgeAndDeleteElements();
	m_CurFileConfig.Purge();
	m_CurSpecificConfig.Purge();
}

const char *CBaseProjectDataCollector::GetProjectName()
{
	return m_ProjectName;
}

void CBaseProjectDataCollector::SetProjectName( const char *pProjectName )
{
	CUtlPathStringHolder tmpBuf( pProjectName );
	V_strlower( tmpBuf.GetForModify() );
	m_ProjectName = tmpBuf;
}

// Get a list of all configurations.
void CBaseProjectDataCollector::GetAllConfigurationNames( CUtlVector< CUtlString > &configurationNames )
{
	configurationNames.Purge();
	for ( int i=m_BaseConfigData.m_Configurations.First(); i != m_BaseConfigData.m_Configurations.InvalidIndex(); i=m_BaseConfigData.m_Configurations.Next(i) )
	{
		configurationNames.AddToTail( m_BaseConfigData.m_Configurations.GetElementName(i) );
	}
}

void CBaseProjectDataCollector::StartConfigurationBlock( const char *pConfigName, bool bFileSpecific )
{
	CFileConfig *pFileConfig = m_CurFileConfig.Top();

	// Find or add a new config block.
	CUtlStringHolder<50> lowerCaseConfigName( pConfigName );
	V_strlower( lowerCaseConfigName.GetForModify() );

	int index = pFileConfig->m_Configurations.Find( lowerCaseConfigName );
	if ( index == -1 )
	{
		CSpecificConfig *pParent = ( pFileConfig==&m_BaseConfigData ? NULL : m_BaseConfigData.GetOrCreateConfig( lowerCaseConfigName, NULL ) );

		CSpecificConfig *pConfig = new CSpecificConfig( pParent );
		pConfig->m_bFileExcluded = false;
		pConfig->m_pKV->SetName( lowerCaseConfigName );
		index = pFileConfig->m_Configurations.Insert( lowerCaseConfigName, pConfig );
	}

	// Remember what the current config is.
	m_CurSpecificConfig.Push( pFileConfig->m_Configurations[index] );
}

void CBaseProjectDataCollector::EndConfigurationBlock()
{
	m_CurSpecificConfig.Pop();
}

const char *CBaseProjectDataCollector::GetCurrentConfigurationName()
{
    const char *configName = "";
    CSpecificConfig *pConfig = m_CurSpecificConfig.Top();
    if ( pConfig )
    {
        configName = pConfig->GetConfigName();
    }

    return configName;
}

bool CBaseProjectDataCollector::StartPropertySection( configKeyword_e keyword, bool *pbShouldSkip )
{
	return true;
}

void CBaseProjectDataCollector::HandleProperty( const char *pProperty, const char *pCustomScriptData )
{
	int i;
	for ( i=0; i < m_RelevantPropertyNames.m_nNames; i++ )
	{
		if ( V_stricmp_fast( m_RelevantPropertyNames.m_pNames[i], pProperty ) == 0 )
			break;
	}
	if ( i == m_RelevantPropertyNames.m_nNames )
	{
		// not found
		return;
	}

	if ( pCustomScriptData )
	{
		g_pVPC->GetScript().PushScript( "HandleProperty( custom script data )", pCustomScriptData, 1, false, false );
	}

	const char *pNextToken = g_pVPC->GetScript().PeekNextToken( false );
	if ( pNextToken && pNextToken[0] != 0 )
	{
		// Pass in the previous value so the $base substitution works.
		CSpecificConfig *pConfig = m_CurSpecificConfig.Top();
		const char *pBaseString = pConfig->m_pKV->GetString( pProperty );
        CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
        if ( g_pVPC->GetScript().ParsePropertyValue( pBaseString, pStrBuf ) )
		{
			pConfig->m_pKV->SetString( pProperty, pStrBuf->Get() );
		}
	}

	if ( pCustomScriptData )
	{
		// Restore prior script state
		g_pVPC->GetScript().PopScript();
	}
}

const char *CBaseProjectDataCollector::GetPropertyValue( const char *pProperty )
{
	return "";
}

void CBaseProjectDataCollector::EndPropertySection( configKeyword_e keyword )
{
}

void CBaseProjectDataCollector::StartFolder( const char *pFolderName, VpcFolderFlags_t iFlags )
{
}
void CBaseProjectDataCollector::EndFolder()
{
}

bool CBaseProjectDataCollector::StartFile( const char *pFilename, VpcFileFlags_t iFlags, bool bWarnIfAlreadyExists )
{
	CFileConfig *pFileConfig = new CFileConfig( pFilename, iFlags );

	if ( m_Files.Find( pFilename ) == m_Files.InvalidIndex() )
	{
		m_Files.Insert( pFilename, pFileConfig );
	}

	m_CurFileConfig.Push( pFileConfig );
	m_CurSpecificConfig.Push( NULL );

	char szFullPath[MAX_FIXED_PATH];

	V_GetCurrentDirectory( szFullPath, sizeof( szFullPath ) );
	V_AppendSlash( szFullPath, sizeof( szFullPath ) );
	V_strncat( szFullPath, pFilename, sizeof( szFullPath ) );
	V_RemoveDotSlashes( szFullPath );

#if 0 // FULLY DISABLED - DOES NOT WORK CORRECTLY.
	if ( g_pVPC->IsP4AutoAddEnabled() )
	{
		// Add file to Perforce if it isn't there already
		if ( Sys_Exists( szFullPath ) )
		{
			if ( g_pP4 && !g_pP4->IsFileInPerforce( szFullPath ) )
			{
				g_pP4->OpenFileForAdd( szFullPath );
				VPCStatus( "%s automatically opened for add in default changelist.", szFullPath );
			}
		}
		else 
		{
			g_pVPC->Warning( "%s not found on disk at location specified in project script.", szFullPath );
		}
	}
#endif

	return true;
}

void CBaseProjectDataCollector::EndFile()
{
	m_CurFileConfig.Pop();
	m_CurSpecificConfig.Pop();
}

// This is actually just per-file configuration data.
void CBaseProjectDataCollector::FileExcludedFromBuild( bool bExcluded )
{
	CSpecificConfig *pConfig = m_CurSpecificConfig.Top();
	pConfig->m_bFileExcluded = bExcluded;
}

bool CBaseProjectDataCollector::RemoveFile( const char *pFilename )
{
	bool bRet = false;
	int i = m_Files.Find( pFilename );
	if ( i != m_Files.InvalidIndex() )
	{
		delete m_Files[i];
		m_Files.RemoveAt( i );
		bRet = true;
	}
	return bRet;
}

void CBaseProjectDataCollector::DoStandardVisualStudioReplacements( const char *pInitStr, CUtlStringBuilder *pStr, const char *pFullInputFilename )
{	
	CUtlPathStringHolder inputDir;

	char sFileBase[MAX_BASE_FILENAME];
	sFileBase[0] = '\0';

    if ( pInitStr )
    {
        pStr->Set( pInitStr );
    }
    
	// Decompose the input filename.
	if ( pFullInputFilename && pFullInputFilename[0] )
	{
		if ( !inputDir.ExtractFilePath( pFullInputFilename ) )
		{
			g_pVPC->VPCError( "DoStandardVisualStudioReplacements:: V_ExtractFilePath failed on %s.", pFullInputFilename );
		}

		V_FileBase( pFullInputFilename, sFileBase, sizeof( sFileBase ) );
	}
	
	if ( pFullInputFilename && pFullInputFilename[0] )
	{
        pStr->Replace( "$(InputPath)", pFullInputFilename );
	}

	if ( !inputDir.IsEmpty() )
	{
		pStr->Replace( "$(InputDir)", inputDir );
	}

	if ( sFileBase[0] )
	{	
		pStr->Replace( "$(InputName)", sFileBase );
	}

    pStr->Replace( "$(IntermediateOutputPath)", "$(OBJ_DIR)" );
    pStr->Replace( "$(IntDir)", "$(OBJ_DIR)" );

	if ( pFullInputFilename && pFullInputFilename[0] )
	{
		pStr->Replace( "$(InputFileName)", pFullInputFilename + V_strlen( inputDir ) );
	}

    pStr->Replace( "$(ConfigurationName)", "${CONFIGURATION}" );
	
    V_FixSlashes( pStr->Access(), '/' );
}

//the input is expected to use makefile variable usage
void CBaseProjectDataCollector::DoShellScriptReplacements( CUtlStringBuilder *pStr )
{
	char *pParse = pStr->AccessNoAssert();
	if ( !pParse )
		return;

	pStr->Replace( "$(OBJ_DIR)", "${OBJ_DIR}" );
}

//the input is expected to use /bin/sh variable style
void CBaseProjectDataCollector::DoBatchScriptReplacements( CUtlStringBuilder *pStr )
{
	DoShellScriptReplacements( pStr );

	char *pParse = pStr->AccessNoAssert();
	if ( !pParse )
		return;

	char *pStringEnd = pParse + V_strlen( pParse );
	for ( ; pParse != pStringEnd; ++pParse )
	{
		if ( *pParse == '$' )
		{
			if ( pParse[1] == '{' )
			{
				char *pReplaceEnd = pParse + 2;
				while ( *pReplaceEnd != '\0' && *pReplaceEnd != '}' )
				{
					Assert( *pReplaceEnd != '$' ); //if we find something that looks like nesting, we're probably doing something wrong
					++pReplaceEnd;
				}

				if ( *pReplaceEnd == '}' )
				{
					//replace {} with %%
					pParse[1] = '%';
					*pReplaceEnd = '%';

					//the $ is a character we don't want, shift the string remainder left
					V_memmove( pParse, pParse + 1, pStringEnd - pParse );
					--pStringEnd;

					pParse = pReplaceEnd;
				}
			}
		}
	}
}

const char *CBaseProjectDataCollector::GetCurrentFileName()
{
	if ( m_CurFileConfig.Count() == 0 )
		return "";
	else
		return m_CurFileConfig.Top()->GetName();
}

//========= Copyright © 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

extern const char *GetVCProjTargetPlatformName( const char *szVPCTargetPlatformName );

void IBaseProjectGenerator::SetOutputFileName( void )
{
	const char *pProjectFileNamePrefix = g_pVPC->IsTestMode() ? "test" : g_pVPC->GetProjectName();

	m_OutputFileName = pProjectFileNamePrefix;

	// win32 projects are the most prevalent, so by popular demand they have no decoration
	// all other platforms use their platform name as a suffix
	const char *pPlatform = NULL;
	if ( !g_pVPC->IsPlatformDefined( "win32" ) )
	{
		pPlatform = g_pVPC->GetTargetPlatformName();
	}

	if ( pPlatform && pPlatform[0] )
	{
		// non-WIN32 platforms get decorated
		m_OutputFileName += "_";
		m_OutputFileName += pPlatform;
	}

	if ( g_pVPC->OutputName_ShouldAppendSrvToDedicated() )
	{
		// Add _srv to the temp lib path.
		m_OutputFileName += "_srv";
	}

	if ( g_pVPC->IsProjectUsingUnity() && g_pVPC->OutputName_ShouldAddUnitySuffix() )
	{
		// If requested via /unity_suffix, decorate projects with $UnityProject with a '_unity' suffix (useful for A/B testing)
		m_OutputFileName += "_unity";
	}

	const char *pGameName = g_pVPC->GetGameName();
	if ( pGameName && pGameName[0] )
	{
		// game projects get decorated
		m_OutputFileName += "_";
		m_OutputFileName += pGameName;
	}

	const CUtlString &projectSuffixString = g_pVPC->OutputName_ProjectSuffixString();
	if ( !projectSuffixString.IsEmpty() )
	{
		m_OutputFileName += "_";
		m_OutputFileName += projectSuffixString;
	}

	const char *pProjExtension = GetProjectFileExtension();
	if ( pProjExtension && pProjExtension[0] )
	{
		m_OutputFileName += ".";
		m_OutputFileName += pProjExtension;
	}
}

void IBaseProjectGenerator::SetGUID( void )
{
	char szBasename[MAX_BASE_FILENAME];
	V_FileBase( GetOutputFileName(), szBasename, sizeof( szBasename ) );
	V_strlower( szBasename );

	// set the GUID
	MD5Context_t ctx;
	unsigned char digest[MD5_DIGEST_LENGTH];
	V_memset( &ctx, 0, sizeof( ctx ) );
	V_memset( digest, 0, sizeof( digest ) );
	MD5Init( &ctx );
	MD5Update( &ctx, (unsigned char *)szBasename, V_strlen( szBasename ) );
	MD5Final( digest, &ctx );

	char szMD5[64];
	V_binarytohex( digest, MD5_DIGEST_LENGTH, szMD5, sizeof( szMD5 ) );
	V_strupper( szMD5 );

	char szGUID[100];
	V_snprintf( szGUID, sizeof( szGUID ), "%8.8s-%4.4s-%4.4s-%4.4s-%12.12s", szMD5, &szMD5[8], &szMD5[12], &szMD5[16], &szMD5[20] );
	m_GUIDString = szGUID;
}

CProjectFile::CProjectFile( CVCProjGenerator *pGenerator, const char *pFilename, VpcFileFlags_t iFlags )
 :	m_pGenerator( pGenerator ),
	m_Name( pFilename ),
	m_iFlags( iFlags ),
	m_nInsertOrder( 0 ),
	m_nBuildOrderModifier( 0 ),
    m_nGeneratorId( 0 )
{
}

CProjectFile::~CProjectFile()
{
	m_Configs.PurgeAndDeleteElements();
}

bool CProjectFile::GetConfiguration( const char *pConfigName, CProjectConfiguration **ppConfig )
{
	if ( !pConfigName || !pConfigName[0] )
	{
		g_pVPC->VPCError( "Empty or bad configuration name." );
	}

	if ( ppConfig )
	{
		// assume not found
		*ppConfig = NULL;
	}

	for ( int i = 0; i < m_Configs.Count(); i++ )
	{
		if ( !V_stricmp_fast( m_Configs[i]->m_Name.Get(), pConfigName ) )
		{
			// found
			if ( ppConfig )
			{
				*ppConfig = m_Configs[i];
			}
			return true;
		}
	}

	// not found
	return false;
}

bool CProjectFile::AddConfiguration( const char *pConfigName, CProjectConfiguration **ppConfig )
{
	if ( ppConfig )
	{
		// assume not found
		*ppConfig = NULL;
	}

	if ( GetConfiguration( pConfigName, NULL ) )
	{
		// found, cannot add duplicate
		return false;
	}

	// add in alphabetic order
	CProjectConfiguration *pNewConfig = new CProjectConfiguration( m_pGenerator, pConfigName, m_Name.Get() );

	int iIndex = 0;
	for ( iIndex = 0; iIndex < m_Configs.Count(); iIndex++ )
	{
		if ( V_stricmp_fast( pConfigName, m_Configs[iIndex]->m_Name.Get() ) < 0 )
		{
			m_Configs.InsertBefore( iIndex, pNewConfig );
			break;
		}
	}
	if ( iIndex == m_Configs.Count() )
	{
		m_Configs.AddToTail( pNewConfig );
	}

	if ( ppConfig )
	{
		*ppConfig = pNewConfig;
	}
	return true;
}

bool CProjectFile::RemoveConfiguration( CProjectConfiguration *pConfiguration )
{
	for ( int i = 0; i < m_Configs.Count(); i++ )
	{
		if ( m_Configs[i] == pConfiguration )
		{
			m_Configs.Remove( i );
			delete pConfiguration;
			return true;
		}
	}

	return false;
}

bool CProjectFile::IsExcludedFrom( const char *pConfigName )
{
	CProjectConfiguration *pConfiguration;
	GetConfiguration( pConfigName, &pConfiguration );
	if ( !pConfiguration )
		return false;

	PropertyState_t *pPropertyState = pConfiguration->m_PropertyStates.GetProperty( g_pOption_ExcludedFromBuild );
	if ( !pPropertyState )
		return false;

	return !V_stricmp_fast( pPropertyState->m_StringValue, "1" );
}

CProjectFolder::CProjectFolder( CVCProjGenerator *pGenerator, const char *pFolderName, VpcFolderFlags_t iFlags )
{
	m_pGenerator = pGenerator;
	m_Name = pFolderName;
	m_iFlags = iFlags;
}

CProjectFolder::~CProjectFolder()
{
	m_Folders.PurgeAndDeleteElements();
	m_Files.PurgeAndDeleteElements();
}

bool CProjectFolder::GetFolder( const char *pFolderName, CProjectFolder **pFolder )
{
	if ( pFolder )
	{
		// assume not found
		*pFolder = NULL;
	}

	if ( !pFolderName || !pFolderName[0] )
	{
		g_pVPC->VPCError( "Empty or bad folder name." );
	}

	for ( int iIndex = m_Folders.Head(); iIndex != m_Folders.InvalidIndex(); iIndex = m_Folders.Next( iIndex ) )
	{
		if ( !V_stricmp_fast( m_Folders[iIndex]->m_Name.Get(), pFolderName ) )
		{
			// found
			if ( pFolder )
			{
				*pFolder = m_Folders[iIndex];
			}
			return true;
		}
	}

	// not found
	return false;
}

bool CProjectFolder::AddFolder( const char *pFolderName, VpcFolderFlags_t iFlags, CProjectFolder **pFolder )
{
	if ( pFolder )
	{
		// assume not added
		*pFolder = NULL;
	}

	CProjectFolder *pExistingFolder;
	if ( GetFolder( pFolderName, &pExistingFolder ) )
	{
		// found, cannot add duplicate but check flags are consistent
		// NOTE: we allow child scripts to override a base script folder from normal to $UnityFolder
		pExistingFolder->m_iFlags = (VpcFolderFlags_t)( pExistingFolder->m_iFlags | ( iFlags & VPC_FOLDER_FLAGS_UNITY ) );
		if ( pExistingFolder->m_iFlags != iFlags )
			g_pVPC->VPCWarning( "Folder '%s' specified multiple times with different types!", pFolderName );
		return false;
	}

	bool bAddToTail = false;
	if ( V_stristr( pFolderName, "($Last)" ) == pFolderName )
	{
		pFolderName += 7;
		bAddToTail = true;
	}

	CProjectFolder *pNewFolder = new CProjectFolder( m_pGenerator, pFolderName, iFlags );

	// maintain sorted ascending alphabetic order
	int iIndex = bAddToTail ? m_Folders.InvalidIndex() : m_Folders.Head();
	for ( ; iIndex != m_Folders.InvalidIndex(); iIndex = m_Folders.Next( iIndex ) )
	{
		if ( V_stricmp_fast( pFolderName, m_Folders[iIndex]->m_Name.Get() ) < 0 )
		{
			m_Folders.InsertBefore( iIndex, pNewFolder );
			break;
		}
	}
	if ( iIndex == m_Folders.InvalidIndex() )
	{
		m_Folders.AddToTail( pNewFolder );
	}

	if ( pFolder )
	{
		*pFolder = pNewFolder;
	}
	return true;
}

void CProjectFolder::AddFile( const char *pFilename, VpcFileFlags_t iFlags, CProjectFile **ppFile )
{
	if ( !pFilename || !pFilename[0] )
	{
		g_pVPC->VPCError( "Empty or bad filename." );
	}

	CProjectFile *pNewFile = new CProjectFile( m_pGenerator, pFilename, iFlags );

	// maintain sorted ascending alphabetic order
	int iIndex;
	for ( iIndex = m_Files.Head(); iIndex != m_Files.InvalidIndex(); iIndex = m_Files.Next( iIndex ) )
	{
		if ( g_pVPC->IsPlatformDefined( "PS3" ) )
		{
			// temporary legacy behavior for diff ease until I can be sure project generation is equivalent
			iIndex = m_Files.InvalidIndex();
			break;
		}

		// the COM layer for WIN32 sorted by filename only, and NOT the entire path
		if ( V_stricmp_fast( V_UnqualifiedFileName( pFilename ), V_UnqualifiedFileName( m_Files[iIndex]->m_Name.Get() ) ) < 0 )
		{
			m_Files.InsertBefore( iIndex, pNewFile );
			break;
		}
	}
	if ( iIndex == m_Files.InvalidIndex() )
	{
		m_Files.AddToTail( pNewFile );
	}
	
	// track relative placement ordering in vpc (i.e. needed for explicit makefile lib ordering)
	static uint32 s_nScriptOrder;
	pNewFile->m_nInsertOrder = ++s_nScriptOrder;
	
	if ( ppFile )
	{
		*ppFile = pNewFile;
	}
}

bool CProjectFolder::FindFile( const char *pFilename )
{
	if ( !pFilename || !pFilename[0] )
	{
		g_pVPC->VPCError( "Empty or bad filename." );
	}

	for ( int iIndex = m_Files.Head(); iIndex != m_Files.InvalidIndex(); iIndex = m_Files.Next( iIndex ) )
	{
		if ( !V_stricmp_fast( m_Files[iIndex]->m_Name.Get(), pFilename ) )
		{
			// found
			return true;
		}
	}

	return false;
}

bool CProjectFolder::RemoveFile( const char *pFilename )
{
	if ( !pFilename || !pFilename[0] )
	{
		g_pVPC->VPCError( "Empty or bad filename." );
	}

	for ( int iIndex = m_Files.Head(); iIndex != m_Files.InvalidIndex(); iIndex = m_Files.Next( iIndex ) )
	{
		if ( !V_stricmp_fast( m_Files[iIndex]->m_Name.Get(), pFilename ) )
		{
			// found, remove
			delete m_Files[iIndex];
			m_Files.Unlink( iIndex );
			return true;
		}
	}

	return false;
}

bool CPropertyStateLessFunc::Less( const int& lhs, const int& rhs, void *pContext )
{
	int lhsPropertyId = (( CPropertyStates* )pContext)->m_Properties[lhs].m_pToolProperty->m_nPropertyId;
	int rhsPropertyId = (( CPropertyStates* )pContext)->m_Properties[rhs].m_pToolProperty->m_nPropertyId;

	return lhsPropertyId < rhsPropertyId;
}

CPropertyStates::CPropertyStates()
{
	m_PropertiesInOutputOrder.SetLessContext( this );
}

PropertyState_t *CPropertyStates::GetProperty( int nPropertyId )
{
	for ( int i = 0 ; i < m_Properties.Count(); i++ )
	{
		if ( m_Properties[i].m_pToolProperty->m_nPropertyId == nPropertyId )
		{
			return &m_Properties[i];
		}
	}

	return NULL;
}

PropertyState_t *CPropertyStates::GetProperty( const char *pPropertyName )
{
	if ( pPropertyName[0] == '$' )
	{
		pPropertyName++;
	}

	for ( int i = 0; i < m_Properties.Count(); i++ )
	{
		const char *pParseString = m_Properties[i].m_pToolProperty->m_ParseString.Get();
		if ( pParseString[0] == '$' )
		{
			pParseString++;
		}
		if ( !V_stricmp_fast( pPropertyName, pParseString ) )
		{
			return &m_Properties[i];
		}
	
		const char *pLegacyString = m_Properties[i].m_pToolProperty->m_LegacyString.Get();
		if ( pLegacyString[0] )
		{
			if ( pLegacyString[0] == '$' )
			{
				pLegacyString++;
			}
			if ( !V_stricmp_fast( pPropertyName, pLegacyString ) )
			{
				return &m_Properties[i];
			}
		}
	}

	return NULL;
}

bool CPropertyStates::SetStringProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	// find possible current value
	const char *pCurrentValue = NULL;
	for ( int i = 0; i < m_Properties.Count(); i++ )
	{
		if ( m_Properties[i].m_pToolProperty == pToolProperty )
		{
			pCurrentValue = m_Properties[i].m_StringValue.Get();
			break;
		}
	}

	if ( !pCurrentValue && pRootTool )
	{
		// fallback to root tool's config to find current value
		for ( int i = 0; i < pRootTool->m_PropertyStates.m_Properties.Count(); i++ )
		{
			if ( pRootTool->m_PropertyStates.m_Properties[i].m_pToolProperty == pToolProperty )
			{
				pCurrentValue = pRootTool->m_PropertyStates.m_Properties[i].m_StringValue.Get();
				break;
			}
		}
	}

	// feed in current value to resolve $BASE
	// possibly culled or tokenized new value
    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( pCurrentValue, pStrBuf ) )
		return true;

	if ( pToolProperty->m_bFixSlashes )
	{
		V_FixSlashes( pStrBuf->Access() );
	}

	if ( pToolProperty->m_bPreferSemicolonNoComma )
	{
        pStrBuf->Replace( ',', ';' );
	}

	if ( pToolProperty->m_bPreferSemicolonNoSpace )
	{
        pStrBuf->Replace( ' ', ';' );
	}

	if ( pToolProperty->m_bAppendSlash )
	{
		size_t len = pStrBuf->Length();
		if ( len >= 1 && pStrBuf->Get()[len-1] != '\\' )
		{
            pStrBuf->AppendChar( '\\' );
		}
	}

	if ( !V_stricmp( pToolProperty->m_ParseString.Get(), "$CommandLine" ) && !V_strnicmp( pStrBuf->Get(), "echo ", 5 ) )
	{
		// the COM layer auto appended a CR-LF for a command line with an echo
		size_t len = pStrBuf->Length();
		if ( ( len >= 1 && pStrBuf->Get()[len-1] != '\n' ) &&
             ( len >= 12 && V_stricmp( pStrBuf->Get() + len - 12, "&#x0D;&#x0A;" ) ) )
		{
            pStrBuf->AppendChar( '\n' );
		}
	}

	if ( pCurrentValue && !V_stricmp( pCurrentValue, pStrBuf->Get() ) )
	{
		g_pVPC->VPCWarning( "%s matches default setting, [%s line:%d]", pToolProperty->m_ParseString.Get(), g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
	}

	if ( pCurrentValue )
	{
		// update existing state
		// always replace or add strings due to case changes
		for ( int i = 0; i < m_Properties.Count(); i++ )
		{
			if ( m_Properties[i].m_pToolProperty == pToolProperty )
			{
				m_Properties[i].m_StringValue = pStrBuf->Get();
				return true;
			}
		}
	}

	// add
	int iIndex = m_Properties.AddToTail();
	m_Properties[iIndex].m_pToolProperty = pToolProperty;
 	m_Properties[iIndex].m_StringValue = pStrBuf->Get();

	m_PropertiesInOutputOrder.Insert( iIndex );

	return true;
}

bool CPropertyStates::SetListProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
		return true;

	// resolve the parsed token to an expected ordinal
	const char *pNewOrdinalValue = NULL;
	for ( int i = 0; i < pToolProperty->m_Ordinals.Count() ;i++ )
	{
		if ( !V_stricmp_fast( pToolProperty->m_Ordinals[i].m_ParseString.Get(), pStrBuf->Get() ) )
		{
			pNewOrdinalValue = pToolProperty->m_Ordinals[i].m_ValueString.Get();
			break;
		}
	}

	if ( !pNewOrdinalValue && !V_stricmp_fast( pStrBuf->Get(), "default" ) )
	{
		// allow "default" if not explicitly provided
		// same as empty, state stays unaffected
		return true;
	}

	if ( !pNewOrdinalValue )
	{
		g_pVPC->VPCSyntaxError( "Unknown Ordinal for %s", pToolProperty->m_ParseString.Get() );
	}

	// find possible current value
	const char *pCurrentOrdinalValue = NULL;
	for ( int i = 0; i < m_Properties.Count(); i++ )
	{
		if ( m_Properties[i].m_pToolProperty == pToolProperty )
		{
			pCurrentOrdinalValue = m_Properties[i].m_StringValue.Get();
			break;
		}
	}

	if ( !pCurrentOrdinalValue && pRootTool )
	{
		// fallback to root tool's config to find current value
		for ( int i = 0; i < pRootTool->m_PropertyStates.m_Properties.Count(); i++ )
		{
			if ( pRootTool->m_PropertyStates.m_Properties[i].m_pToolProperty == pToolProperty )
			{
				pCurrentOrdinalValue = pRootTool->m_PropertyStates.m_Properties[i].m_StringValue.Get();
				break;
			}
		}
	}

	if ( pCurrentOrdinalValue && !V_stricmp_fast( pCurrentOrdinalValue, pNewOrdinalValue ) )
	{
		g_pVPC->VPCWarning( "%s matches default setting, [%s line:%d]", pToolProperty->m_ParseString.Get(), g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
	}

	if ( pCurrentOrdinalValue )
	{
		// update existing state
		for ( int i = 0; i < m_Properties.Count(); i++ )
		{
			if ( m_Properties[i].m_pToolProperty == pToolProperty )
			{
				m_Properties[i].m_OrdinalString = pStrBuf->Get();
				m_Properties[i].m_StringValue = pNewOrdinalValue;
				return true;
			}
		}
	}

	// add
	int iIndex = m_Properties.AddToTail();
	m_Properties[iIndex].m_pToolProperty = pToolProperty;
 	m_Properties[iIndex].m_OrdinalString = pStrBuf->Get();
 	m_Properties[iIndex].m_StringValue = pNewOrdinalValue;

	m_PropertiesInOutputOrder.Insert( iIndex );

	return true;
}

bool CPropertyStates::SetBoolProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool, bool bEnabled )
{
	const char *pNewOrdinalValue = bEnabled ? "1" :"0";

	// find possible current value
	const char *pCurrentOrdinalValue = NULL;
	for ( int i = 0; i < m_Properties.Count(); i++ )
	{
		if ( m_Properties[i].m_pToolProperty == pToolProperty )
		{
			pCurrentOrdinalValue = m_Properties[i].m_StringValue.Get();
			break;
		}
	}

	if ( !pCurrentOrdinalValue && pRootTool )
	{
		// fallback to root tool's config to find current value
		for ( int i = 0; i < pRootTool->m_PropertyStates.m_Properties.Count(); i++ )
		{
			if ( pRootTool->m_PropertyStates.m_Properties[i].m_pToolProperty == pToolProperty )
			{
				pCurrentOrdinalValue = pRootTool->m_PropertyStates.m_Properties[i].m_StringValue.Get();
				break;
			}
		}
	}

	if ( pCurrentOrdinalValue && !V_stricmp_fast( pCurrentOrdinalValue, pNewOrdinalValue ) )
	{
		g_pVPC->VPCWarning( "%s matches default setting, [%s line:%d]", pToolProperty->m_ParseString.Get(), g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
	}

	if ( pCurrentOrdinalValue )
	{
		// update existing state
		for ( int i = 0; i < m_Properties.Count(); i++ )
		{
			if ( m_Properties[i].m_pToolProperty == pToolProperty )
			{
				m_Properties[i].m_StringValue = pNewOrdinalValue;
				return true;
			}
		}
	}

	// add
	int iIndex = m_Properties.AddToTail();
	m_Properties[iIndex].m_pToolProperty = pToolProperty;
 	m_Properties[iIndex].m_StringValue = pNewOrdinalValue;

	m_PropertiesInOutputOrder.Insert( iIndex );

	return true;
}

bool CPropertyStates::SetBoolProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
		return true;

	return SetBoolProperty( pToolProperty, pRootTool, Sys_StringToBool( pStrBuf->Get() ) );
}

bool CPropertyStates::SetBoolProperty( ToolProperty_t *pToolProperty, bool bEnabled )
{
	return SetBoolProperty( pToolProperty, NULL, bEnabled );
}

bool CPropertyStates::SetIntegerProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
		return true;

	// ensure the parsed token is a real integer and not just quietly mapped to 0
	int64 nParsedValue = V_atoi64( pStrBuf->Get() );
	{
		const char *pParse = pStrBuf->Get();
		bool bNegative = false;
		if ( pParse[0] == '-' )
		{
			bNegative = true;
			++pParse;
		}

		CFmtStr compareStr;
		if ( pParse[0] == '0' && (pParse[1] == 'x' || pParse[1] == 'X') )
		{
			compareStr.Format( "%s0x%llX", bNegative ? "-" : "", nParsedValue );
		}
		else
		{
			compareStr.Format( "%lld", nParsedValue );
		}

		if ( V_stricmp_fast( compareStr, pStrBuf->Get() ) )
		{
			g_pVPC->VPCSyntaxError( "Unrecognized integer value: %s", pStrBuf->Get() );
		}
	}

	// find possible current value
	const char *pCurrentOrdinalValue = NULL;
	for ( int i = 0; i < m_Properties.Count(); i++ )
	{
		if ( m_Properties[i].m_pToolProperty == pToolProperty )
		{
			pCurrentOrdinalValue = m_Properties[i].m_StringValue.Get();
			break;
		}
	}

	if ( !pCurrentOrdinalValue && pRootTool )
	{
		// fallback to root tool's config to find current value
		for ( int i = 0; i < pRootTool->m_PropertyStates.m_Properties.Count(); i++ )
		{
			if ( pRootTool->m_PropertyStates.m_Properties[i].m_pToolProperty == pToolProperty )
			{
				pCurrentOrdinalValue = pRootTool->m_PropertyStates.m_Properties[i].m_StringValue.Get();
				break;
			}
		}
	}

	if ( pCurrentOrdinalValue && ( V_atoi64( pCurrentOrdinalValue ) == nParsedValue ) )
	{
		g_pVPC->VPCWarning( "%s matches default setting, [%s line:%d]", pToolProperty->m_ParseString.Get(), g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
	}

	if ( pCurrentOrdinalValue )
	{
		// update existing state
		for ( int i = 0; i < m_Properties.Count(); i++ )
		{
			if ( m_Properties[i].m_pToolProperty == pToolProperty )
			{
				m_Properties[i].m_StringValue = pStrBuf->Get();
				return true;
			}
		}
	}

	// add
	int iIndex = m_Properties.AddToTail();
	m_Properties[iIndex].m_pToolProperty = pToolProperty;
 	m_Properties[iIndex].m_StringValue = pStrBuf->Get();

	m_PropertiesInOutputOrder.Insert( iIndex );

	return true;
}

bool CPropertyStates::SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	if ( pToolProperty->m_bBreakOnWrite )
	{
		DebuggerBreakIfDebugging();
	}

	bool bHandled = false;
	switch ( pToolProperty->m_nType )
	{
	case PT_BOOLEAN:
		bHandled = SetBoolProperty( pToolProperty, pRootTool );
		break;

	case PT_STRING:
		bHandled = SetStringProperty( pToolProperty, pRootTool );
		break;

	case PT_INTEGER:
		bHandled = SetIntegerProperty( pToolProperty, pRootTool );
		break;

	case PT_LIST:
		bHandled = SetListProperty( pToolProperty, pRootTool );
		break;

	case PT_IGNORE:
		bHandled = true;
		g_pVPC->GetScript().SkipRestOfLine();
		break;

	case PT_DEPRECATED:
		g_pVPC->VPCError( "SetProperty: Property %s has been deprecated and is no longer supported!", pToolProperty->m_ParseString.Get() );
		break;

	default:
		g_pVPC->VPCError( "SetProperty: Unknown type for %s - requires implementation", pToolProperty->m_ParseString.Get() );
	}

	return bHandled;
}

const char *CPropertyStates::GetPropertyValue( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	if ( pToolProperty->m_bBreakOnRead )
	{
		DebuggerBreakIfDebugging();
	}

	switch ( pToolProperty->m_nType )
	{
	case PT_BOOLEAN:
	case PT_STRING:
	case PT_INTEGER:
	case PT_LIST:
		break;

	case PT_IGNORE:
		g_pVPC->VPCError( "GetPropertyValue: Property %s is an 'ignored' key and has no real state associated with it and cannot be queried.", pToolProperty->m_ParseString.Get() );
		
	case PT_DEPRECATED:
		g_pVPC->VPCError( "GetPropertyValue: Property %s has been deprecated and is no longer supported!", pToolProperty->m_ParseString.Get() );

	default:
		g_pVPC->VPCError( "GetPropertyValue: Unknown type for %s - requires implementation", pToolProperty->m_ParseString.Get() );
	}

	// find possible current value
	const char *pCurrentOrdinalValue = NULL;
	for ( int i = 0; i < m_Properties.Count(); i++ ) // TODO: optimize
	{
		if ( m_Properties[i].m_pToolProperty == pToolProperty )
		{
			if ( pToolProperty->m_nType == PT_LIST )
			{
				// Lists need to return the VPC script facing UI ordinal string expected in the scripts, NOT the internal string that the 
				// ordinal resolves to that the vcxproj expects.
				pCurrentOrdinalValue = m_Properties[i].m_OrdinalString.Get();
			}
			else
			{
				pCurrentOrdinalValue = m_Properties[i].m_StringValue.Get();
			}
			break;
		}
	}

	if ( !pCurrentOrdinalValue && pRootTool )
	{
		// fallback to root tool's config to find current value
		for ( int i = 0; i < pRootTool->m_PropertyStates.m_Properties.Count(); i++ ) // TODO: optimize
		{
			if ( pRootTool->m_PropertyStates.m_Properties[i].m_pToolProperty == pToolProperty )
			{
				if ( pToolProperty->m_nType == PT_LIST )
				{
					// Lists need to return the VPC script facing UI ordinal string expected in the scripts, NOT the internal string that the 
					// ordinal resolves to that the vcxproj expects.
					pCurrentOrdinalValue = pRootTool->m_PropertyStates.m_Properties[i].m_OrdinalString.Get();
				}
				else
				{
					pCurrentOrdinalValue = pRootTool->m_PropertyStates.m_Properties[i].m_StringValue.Get();
				}
				break;
			}
		}
	}

	return pCurrentOrdinalValue ? pCurrentOrdinalValue : "";
}

static bool FilesSortLessFunc( CProjectFile* const &pLHS, CProjectFile* const &pRHS )
{
	if ( pLHS->m_nBuildOrderModifier != pRHS->m_nBuildOrderModifier )
		return pLHS->m_nBuildOrderModifier < pRHS->m_nBuildOrderModifier;

	return pLHS->m_nInsertOrder < pRHS->m_nInsertOrder;
}

CProjectConfiguration::CProjectConfiguration( CVCProjGenerator *pGenerator, const char *pConfigName, const char *pFilename )
{
	m_pGenerator = pGenerator;
	m_bIsFileConfig = ( pFilename != NULL );

	m_Name = pConfigName;
	m_LowerCaseName = pConfigName;
	m_LowerCaseName.ToLower();

	m_pDebuggingTool = NULL;
	m_pCompilerTool = NULL;
	m_pLibrarianTool = NULL;
	m_pLinkerTool = NULL; 
	m_pManifestTool = NULL; 
	m_pXMLDocGenTool = NULL; 
	m_pBrowseInfoTool = NULL; 
	m_pResourcesTool = NULL; 
	m_pPreBuildEventTool = NULL;
	m_pPreLinkEventTool = NULL;
	m_pPostBuildEventTool = NULL;
	m_pCustomBuildTool = NULL;
	m_pXboxImageTool = NULL;
	m_pXboxDeploymentTool = NULL;
	m_pIntellisenseTool = nullptr;

	if ( !m_bIsFileConfig )
	{
		m_pDebuggingTool = new CDebuggingTool( pGenerator );
		m_pCompilerTool = new CCompilerTool( pGenerator, pConfigName, false );
		m_pLibrarianTool = new CLibrarianTool( pGenerator );
		m_pLinkerTool = new CLinkerTool( pGenerator );
		m_pManifestTool = new CManifestTool( pGenerator );
		m_pXMLDocGenTool = new CXMLDocGenTool( pGenerator );
		m_pBrowseInfoTool = new CBrowseInfoTool( pGenerator );
		m_pResourcesTool = new CResourcesTool( pGenerator, pConfigName, false );
		m_pPreBuildEventTool = new CPreBuildEventTool( pGenerator );
		m_pPreLinkEventTool = new CPreLinkEventTool( pGenerator );
		m_pPostBuildEventTool = new CPostBuildEventTool( pGenerator );
		m_pCustomBuildTool = new CCustomBuildTool( pGenerator, pConfigName, false );
		m_pXboxImageTool = new CXboxImageTool( pGenerator );
		m_pXboxDeploymentTool = new CXboxDeploymentTool( pGenerator );
		m_pIntellisenseTool = new CProjectTool( pGenerator );
	}
	else
	{
		// a file's config can only be the compiler, librarian, resources, or custom build tool
		const char *pExtension = V_GetFileExtension( pFilename );
		bool bIsCPP = IsCFileExtension( pExtension );
		bool bIsLib = pExtension && !V_stricmp_fast( pExtension, "lib" );
		bool bIsRC = pExtension && !V_stricmp_fast( pExtension, "rc" );
		bool bIsNatVis = pExtension && !V_stricmp_fast( pExtension, "natvis" );
		if ( bIsCPP )
		{
			m_pCompilerTool = new CCompilerTool( pGenerator, pConfigName, true );
		}
		else if ( bIsLib )
		{
			m_pLibrarianTool = new CLibrarianTool( pGenerator );
		}
		else if ( bIsRC )
		{
			m_pResourcesTool = new CResourcesTool( pGenerator, pConfigName, true );
		}
		else if ( bIsNatVis )
		{
			// Visual Studio .natvis files have no associated tool. In order for them to serialize
			// correctly to the vcxproj, their tool component is empty. No tool property options are
			// thus available, except $ExcludedFromBuild.
		}
		else
		{
			// assuming any unrecognized extension would be a custom build
			m_pCustomBuildTool = new CCustomBuildTool( pGenerator, pConfigName, true );
		}
	}
}

CProjectConfiguration::~CProjectConfiguration()
{
	delete m_pDebuggingTool;
	delete m_pCompilerTool;
	delete m_pLibrarianTool;
	delete m_pLinkerTool; 
	delete m_pManifestTool; 
	delete m_pXMLDocGenTool; 
	delete m_pBrowseInfoTool; 
	delete m_pResourcesTool; 
	delete m_pPreBuildEventTool;
	delete m_pPreLinkEventTool;
	delete m_pPostBuildEventTool;
	delete m_pCustomBuildTool;
	delete m_pXboxImageTool;
	delete m_pXboxDeploymentTool;
	delete m_pIntellisenseTool;
}

bool CProjectConfiguration::IsEmpty()
{
	if ( m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pDebuggingTool && m_pDebuggingTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pCompilerTool && m_pCompilerTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pLibrarianTool && m_pLibrarianTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pLinkerTool && m_pLinkerTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pManifestTool && m_pManifestTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pXMLDocGenTool && m_pXMLDocGenTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pBrowseInfoTool && m_pBrowseInfoTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pResourcesTool && m_pResourcesTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pPreBuildEventTool && m_pPreBuildEventTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pPreLinkEventTool && m_pPreLinkEventTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pPostBuildEventTool && m_pPostBuildEventTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pCustomBuildTool && m_pCustomBuildTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pXboxImageTool && m_pXboxImageTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pXboxDeploymentTool && m_pXboxDeploymentTool->m_PropertyStates.m_Properties.Count() )
		return false;

	if ( m_pIntellisenseTool && m_pIntellisenseTool->m_PropertyStates.m_Properties.Count() )
		return false;

	return true;
}

bool CProjectConfiguration::SetProperty( ToolProperty_t *pToolProperty )
{
	bool bHandled = m_PropertyStates.SetProperty( pToolProperty );

	// have to mimic what the COM layer used to do which is to configure itself based on the type of application its building
	// VPC enforces a strict order, configuration blocks must come before any tool block to allow this to be rational
	if ( bHandled && !V_stricmp_fast( pToolProperty->m_ParseString.Get(), "$ConfigurationType" ) )
	{
		PropertyState_t *pPropertyState = m_PropertyStates.GetProperty( pToolProperty->m_nPropertyId );
		if ( pPropertyState && 
			( ( V_stristr_fast( pPropertyState->m_OrdinalString.Get(), "static library" ) || !V_stricmp_fast( pPropertyState->m_OrdinalString.Get(), "LIB" ) ) ) )
		{
			// static library does not get these tools
			delete m_pResourcesTool;
			m_pResourcesTool = NULL;

			delete m_pManifestTool;
			m_pManifestTool = NULL;

			delete m_pLinkerTool;
			m_pLinkerTool = NULL;

			delete m_pXboxImageTool;
			m_pXboxImageTool = NULL;

			delete m_pXboxDeploymentTool;
			m_pXboxDeploymentTool = NULL;
		}
		else if ( !pPropertyState ||
                  V_stricmp_fast( pPropertyState->m_OrdinalString, "Makefile" ) != 0 )
		{
			// exe/dlls do not get the librarian
			delete m_pLibrarianTool;
			m_pLibrarianTool = NULL;
		}
	}

	return bHandled;
}

const char *CProjectConfiguration::GetPropertyValue( ToolProperty_t *pToolProperty )
{
	return m_PropertyStates.GetPropertyValue( pToolProperty );
}

bool CProjectTool::SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	return m_PropertyStates.SetProperty( pToolProperty, pRootTool );
}

const char *CProjectTool::GetPropertyValue( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	return m_PropertyStates.GetPropertyValue( pToolProperty, pRootTool );
}

//-----------------------------------------------------------------------------

bool CCompilerTool::SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	if ( m_bIsFileConfig )
	{
		CProjectConfiguration *pConfig;
		if ( !GetGenerator()->GetRootConfiguration( m_ConfigName.Get(), &pConfig ) )
			return false;

		return CProjectTool::SetProperty( pToolProperty, pConfig->GetCompilerTool() );
	}
	return CProjectTool::SetProperty( pToolProperty );
}

const char *CCompilerTool::GetPropertyValue( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	if ( m_bIsFileConfig )
	{
		CProjectConfiguration *pConfig;
		if ( !GetGenerator()->GetRootConfiguration( m_ConfigName.Get(), &pConfig ) )
		{
			g_pVPC->VPCError( "CCompilerTool::GetPropertyValue: Failed to get configuration %s", m_ConfigName.Get() );
		}

		return CProjectTool::GetPropertyValue( pToolProperty, pConfig->GetCompilerTool() );
	}
	return CProjectTool::GetPropertyValue( pToolProperty );
}

//-----------------------------------------------------------------------------

bool CCustomBuildTool::SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	if ( m_bIsFileConfig )
	{
		CProjectConfiguration *pConfig;
		if ( !GetGenerator()->GetRootConfiguration( m_ConfigName.Get(), &pConfig ) )
			return false;

		return CProjectTool::SetProperty( pToolProperty, pConfig->GetCustomBuildTool() );
	}
	return CProjectTool::SetProperty( pToolProperty );
}

const char *CCustomBuildTool::GetPropertyValue( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	if ( m_bIsFileConfig )
	{
		CProjectConfiguration *pConfig;
		if ( !GetGenerator()->GetRootConfiguration( m_ConfigName.Get(), &pConfig ) )
		{
			g_pVPC->VPCError( "CCustomBuildTool::GetPropertyValue: Failed to get configuration %s", m_ConfigName.Get() );
		}

		return CProjectTool::GetPropertyValue( pToolProperty, pConfig->GetCustomBuildTool() );
	}
	return CProjectTool::GetPropertyValue( pToolProperty );
}

//-----------------------------------------------------------------------------

bool CResourcesTool::SetProperty( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	if ( m_bIsFileConfig )
	{
		CProjectConfiguration *pConfig;
		if ( !GetGenerator()->GetRootConfiguration( m_ConfigName.Get(), &pConfig ) )
			return false;

		return CProjectTool::SetProperty( pToolProperty, pConfig->GetCompilerTool() );
	}
	return CProjectTool::SetProperty( pToolProperty );
}

const char *CResourcesTool::GetPropertyValue( ToolProperty_t *pToolProperty, CProjectTool *pRootTool )
{
	if ( m_bIsFileConfig )
	{
		CProjectConfiguration *pConfig;
		if ( !GetGenerator()->GetRootConfiguration( m_ConfigName.Get(), &pConfig ) )
		{
			g_pVPC->VPCError( "CResourcesTool::GetPropertyValue: Failed to get configuration %s", m_ConfigName.Get() );
		}

		return CProjectTool::GetPropertyValue( pToolProperty, pConfig->GetCompilerTool() );
	}
	return CProjectTool::GetPropertyValue( pToolProperty );
}

// These are the only properties we care about for schema purposes.
static const char *g_pRelevantSchemaProperties[] =
{
	g_pOption_AdditionalIncludeDirectories,
	g_pOption_PreprocessorDefinitions,
	g_pOption_AdditionalProjectDependencies,
	g_pOption_PrecompiledHeader,
	g_pOption_UsePCHThroughFile,
	g_pOption_PrecompiledHeaderFile
};


CRelevantPropertyNames g_RelevantSchemaPropertyNames =
{
	g_pRelevantSchemaProperties,
	V_ARRAYSIZE( g_pRelevantSchemaProperties )
};


CVCProjGenerator::CVCProjGenerator() : 
	BaseClass( &g_RelevantSchemaPropertyNames )
{
	m_pGeneratorDefinition = NULL;
	m_pRootFolder = NULL;
	m_FileDictionary.SetLessFunc( FilesSortLessFunc );

	Clear();
}

void CVCProjGenerator::Clear()
{
	BaseClass::Term();

	m_nActivePropertySection = KEYWORD_UNKNOWN;

	m_pProjectFile = NULL;
	m_pConfig = NULL;
	m_pFileConfig = NULL;

	m_pDebuggingTool = NULL;
	m_pCompilerTool = NULL;
	m_pLibrarianTool = NULL;
	m_pLinkerTool = NULL; 
	m_pManifestTool = NULL; 
	m_pXMLDocGenTool = NULL; 
	m_pBrowseInfoTool = NULL; 
	m_pResourcesTool = NULL; 
	m_pPreBuildEventTool = NULL;
	m_pPreLinkEventTool = NULL;
	m_pPostBuildEventTool = NULL;
	m_pCustomBuildTool = NULL;
	m_pXboxImageTool = NULL;
	m_pXboxDeploymentTool = NULL;
	m_pIntellisenseTool = nullptr;

	m_spFolderStack.Purge();
	m_spCompilerStack.Purge();
	m_spCustomBuildToolStack.Purge();
	m_spResourcesToolStack.Purge();

	// undefined until set
	m_VSIType = PS3_VSI_TYPE_UNDEFINED;

	m_FileDictionary.Purge();

	// setup expected root folder
	delete m_pRootFolder;
	m_pRootFolder = new CProjectFolder( this, "???", VPC_FOLDER_FLAGS_NONE );

	// setup the root configurations
	// There are ONLY two root configurations. There should only EVER be two root configurations. "Debug" and "Release".
	// This was a specific design choice that had group consensus. Otherwise the configuration permutation space
	// explodes with no studio benefit. All other configurations are supported by minor property changes.
	m_RootConfigurations.PurgeAndDeleteElements();
	CProjectConfiguration *pDebugConfig = new CProjectConfiguration( this, "Debug", NULL );
	m_RootConfigurations.AddToTail( pDebugConfig );
	CProjectConfiguration *pReleaseConfig = new CProjectConfiguration( this, "Release", NULL );
	m_RootConfigurations.AddToTail( pReleaseConfig );
}

void CVCProjGenerator::SetupGeneratorDefinition( const char *pDefinitionName, PropertyName_t *pPropertyNames )
{
	delete m_pGeneratorDefinition;
	m_pGeneratorDefinition = new CGeneratorDefinition();
	m_pGeneratorDefinition->LoadDefinition( pDefinitionName, pPropertyNames );
}

void CVCProjGenerator::AddProjectWriter( IVCProjWriter *pVCProjWriter )
{
	m_VCProjWriters.AddToTail( pVCProjWriter );
}

const char* CVCProjGenerator::GetProjectFileExtension()
{
	const char *pProjectExtension = NULL;
	if ( !m_VCProjWriters.IsEmpty() )
	{
		// the primary writer can supply it
		pProjectExtension = m_VCProjWriters[0]->GetProjectFileExtension();
	}

	if ( !pProjectExtension )
	{
		// provide default
		pProjectExtension = g_pVPC->Is2005() ? "vcproj" : "vcxproj";
	}

	return pProjectExtension;
}

void CVCProjGenerator::StartProject()
{
	if ( !m_pGeneratorDefinition )
	{
		g_pVPC->VPCError( "Missing a properly configured generator definition" );
	}

	BaseClass::StartProject();

	// create the default project
	// must have a root project for most operations
	m_ProjectName = "UNNAMED";
}

void CVCProjGenerator::EndProject( bool bSaveData )
{
	// push generator definition scripts into CRC check
	CRC32_t scriptCRC = 0;
	const char *pScriptName = m_pGeneratorDefinition->GetScriptName( &scriptCRC );
    if ( !V_IsAbsolutePath( pScriptName ) )
    {
        CUtlPathStringHolder srcDirScriptName( "$SRCDIR\\" );
        srcDirScriptName.Append( pScriptName );
        CUtlStringBuilder *pStrBuf = g_pVPC->GetMacroReplaceBuffer();
        g_pVPC->ResolveMacrosInString( srcDirScriptName.Get(), pStrBuf );
        pScriptName = pStrBuf->Get();
    }
	g_pVPC->AddScriptToParsedList( pScriptName, true, scriptCRC );

	// done once, right before save
	ApplyInternalPreprocessorDefinitions();

	// Let features hook in and read/modify the project data:
	VPC_Schema_OnParseProjectEnd( this );
	VPC_Qt_OnParseProjectEnd( this );
		
	VPC_Unity_OnParseProjectEnd( this );
	VPC_Clang_OnParseProjectEnd( this );

	EvaluateHackMacro_HACK_DEPENDENCIES_ALLVPCSCRIPTS();
	AddIndirectCustomBuildDependencies();
	
	// NOTE: the following is deliberately *after* all other 'features', so it can see all generated dynamic files
	VPC_GeneratedFiles_OnParseProjectEnd( this );

#ifdef STEAM
#error( "NEEDS TO BE FIXED" )
	// add the perforce integration magic
	bstr = "Perforce Project";
	g_spProject->put_SccProjectName( bstr );
	bstr = "..";
	g_spProject->put_SccLocalPath( bstr );
	bstr = "MSSCCI:Perforce SCM";
	g_spProject->put_SccProvider( bstr );
#endif

	// Don't write a VCPROJ when we're being called by CDependencyGraph!
	if ( !g_pVPC->m_bIsDependencyPass )
	{
		if ( m_ProjectName.IsEmpty() )
		{
			g_pVPC->VPCError( "Invalid Empty Project Name" );
		}

		const char *pOutputFileName = GetOutputFileName();
		if ( !pOutputFileName || !pOutputFileName[0] )
		{
			g_pVPC->VPCError( "Invalid Empty Output Filename (project %s)", m_ProjectName.Get() );
		}
	
		const char *pGUID = GetGUIDString();
		if ( !pGUID || !pGUID[0] )
		{
			g_pVPC->VPCError( "Invalid Empty GUID String (project %s)", m_ProjectName.Get() );
		}

        if ( m_VCProjWriters.IsEmpty() )
        {
			g_pVPC->VPCError( "No writers to save project '%s' to '%s'", m_ProjectName.Get(), pOutputFileName );
        }

        LogOutputFiles( "Release" );

        for ( int i = 0; i < m_VCProjWriters.Count(); i++ )
        {
            CUtlString UseFilenameBuf;
            const char *pUseFilename;

            if ( i == 0 )
            {
                // Use the primary filename for the primary writer.
                pUseFilename = pOutputFileName;
            }
            else
            {
                // Subsidiary writers use the primary
                // filename plus their own extension.
                UseFilenameBuf = pOutputFileName;
				UseFilenameBuf += ".";
                UseFilenameBuf += m_VCProjWriters[i]->GetProjectFileExtension();
                pUseFilename = UseFilenameBuf.Get();
            }

            g_pVPC->VPCStatus( true, "Saving... Project: '%s' File: '%s'", m_ProjectName.Get(), pUseFilename );

            // Save the output file.
            if ( !m_VCProjWriters[i]->Save( pUseFilename ) )
            {
                g_pVPC->VPCError( "Cannot save the specified project '%s' to '%s'", m_ProjectName.Get(), pUseFilename );
            }
        }
	}

	// Expected to not be inside a property section.
	Assert( m_nActivePropertySection == KEYWORD_UNKNOWN ); 

	BaseClass::EndProject( bSaveData );

    if ( !bSaveData )
    {
        Clear();
    }
}

const char *CVCProjGenerator::GetProjectName()
{
	return m_ProjectName;
}

void CVCProjGenerator::SetProjectName( const char *pProjectName )
{
	m_ProjectName = pProjectName;
}

void CVCProjGenerator::StartFolder( const char *pFolderName, VpcFolderFlags_t iFlags )
{
	BaseClass::StartFolder( pFolderName, iFlags );

	bool bValid;
	CProjectFolder *pFolder = NULL;

	if ( m_spFolderStack.Count() == 0 )
	{
		// add to root
		bValid = AddFolder( pFolderName, NULL, iFlags, &pFolder );
	}
	else
	{
		// add as subfolder
		bValid = AddFolder( pFolderName, m_spFolderStack.Top(), iFlags, &pFolder );
	}

	if ( !bValid )
	{
		// resolve failure
		// folder already exists, not an error
		// find the matching object
		pFolder = NULL;
		if ( m_spFolderStack.Count() == 0 )
		{
			// at root
			GetFolder( pFolderName, NULL, &pFolder );
		}
		else
		{
			// at subfolder
			GetFolder( pFolderName, m_spFolderStack.Top(), &pFolder );
		}
		if ( !pFolder )
		{
			g_pVPC->VPCError( "Cannot find expected folder %s", pFolderName );
		}
	}

	m_spFolderStack.Push( pFolder );
}

void CVCProjGenerator::EndFolder()
{
	BaseClass::EndFolder();

	if ( m_spFolderStack.Count() == 0 )
	{
		g_pVPC->VPCError( "EndFolder called and no folder has been started." );
	}

	m_spFolderStack.Pop();
}

bool CVCProjGenerator::StartFile( const char *pFilename, VpcFileFlags_t iFlags, bool bWarnIfAlreadyExists )
{
	// normalize filename, filenames need to compare correctly
    CUtlPathStringHolder cleanFilename( pFilename );
    cleanFilename.FixSlashesAndDotSlashes();

	// some vpc scripts decided to unnecessarily double quote their filenames
	// remove any incoming surrounding quotes, this only causes string handling problems (i.e. extension comparison, etc)
	// all files get serialized to xml output with mandatory surrounding quotes
	if ( cleanFilename[0] == '\"' )
	{
		size_t len = strlen( cleanFilename );
		if ( len > 1 && cleanFilename[len-1] == '\"' )
		{
			memcpy( cleanFilename.GetForModify(), cleanFilename+1, len - 2 );
			cleanFilename.GetForModify()[len-2] = '\0';
		}
	}

	pFilename = cleanFilename;

	BaseClass::StartFile( pFilename, iFlags, bWarnIfAlreadyExists );

	CProjectFile *pFile = NULL;

	if ( m_spFolderStack.Count() == 0 )
	{
		// add at root
		AddFileToFolder( pFilename, NULL, bWarnIfAlreadyExists, iFlags, &pFile );
	}
	else
	{
		// add at subfolder
		AddFileToFolder( pFilename, m_spFolderStack.Top(), bWarnIfAlreadyExists, iFlags, &pFile );
	}

	m_pProjectFile = pFile;
	return ( pFile != NULL );
}

void CVCProjGenerator::EndFile()
{
	if ( m_pProjectFile->m_nBuildOrderModifier != 0 )
	{
		//file is making use of the build order modifier, reinsert
		int nOldIndex = FindFileInDictionary( m_pProjectFile->m_Name.Get() );
		if ( nOldIndex != m_FileDictionary.InvalidIndex() )
		{
			m_FileDictionary.Reinsert(  nOldIndex );
		}
	}
	m_pProjectFile = NULL;

	BaseClass::EndFile();
}

bool CVCProjGenerator::RemoveFile( const char *pFilename )
{
	// normalize filename, filenames need to compare correctly
    CUtlPathStringHolder cleanFilename( pFilename );
    cleanFilename.FixSlashesAndDotSlashes();
	pFilename = cleanFilename;

	BaseClass::RemoveFile( pFilename );

	bool bValid;
	if ( m_spFolderStack.Count() == 0 )
	{
		// remove from root
		bValid = RemoveFileFromFolder( pFilename, NULL );
	}
	else
	{
		// remove at subfolder
		bValid = RemoveFileFromFolder( pFilename, m_spFolderStack.Top() );
	}

	return bValid;
}

void CVCProjGenerator::EnumerateSupportedVPCTargetPlatforms( CUtlVector<CUtlString> &output )
{
	output.AddToTail( g_pVPC->GetTargetPlatformName() );
}

bool CVCProjGenerator::BuildsForTargetPlatform( const char *szVPCTargetPlatform )
{
	return true;
}

bool CVCProjGenerator::DeploysForVPCTargetPlatform( const char *szVPCTargetPlatform )
{
	return false;
}

CUtlString CVCProjGenerator::GetSolutionPlatformAlias( const char *szVPCTargetPlatform, IBaseSolutionGenerator *pSolutionGenerator )
{
	switch ( pSolutionGenerator->GetSolutionType() )
	{
	case ST_VISUALSTUDIO:
		return GetVCProjTargetPlatformName( szVPCTargetPlatform );

	case ST_MAKEFILE:
	case ST_XCODE:
		return szVPCTargetPlatform;

	NO_DEFAULT;
	};
}

bool CVCProjGenerator::Config_GetConfigurations( const char *pszConfigName )
{
	CProjectConfiguration *pConfig = NULL;
	bool bValid = GetRootConfiguration( pszConfigName, &pConfig );
	if ( !bValid )
	{
		g_pVPC->VPCError( "Could not get configuration '%s'", pszConfigName );
	}
	m_pConfig = pConfig;

	return true;
}

void CVCProjGenerator::StartConfigurationBlock( const char *pConfigName, bool bFileSpecific )
{
	BaseClass::StartConfigurationBlock( pConfigName, bFileSpecific );

	if ( bFileSpecific )
	{
		CProjectConfiguration *pFileConfig = NULL;
		bool bValid = m_pProjectFile->GetConfiguration( pConfigName, &pFileConfig );
		if ( !bValid )
		{
			// not found, must be valid config
			// must match predefined configurations, prevents misspellings
			if ( !IsConfigurationNameValid( pConfigName ) )
			{
				g_pVPC->VPCError( "File %s, Unknown configuration '%s'", m_pProjectFile->m_Name.Get(), pConfigName );
			}

			bValid = m_pProjectFile->AddConfiguration( pConfigName, &pFileConfig );
			if ( !bValid )
			{
				g_pVPC->VPCError( "File %s, Could not get file configuration '%s'", m_pProjectFile->m_Name.Get(), pConfigName );
			}
		}
		m_pFileConfig = pFileConfig;
	}
	else
	{
		Config_GetConfigurations( pConfigName );
	}
}

void CVCProjGenerator::EndConfigurationBlock()
{
	BaseClass::EndConfigurationBlock();

	if ( m_pFileConfig && m_pFileConfig->IsEmpty() )
	{
		// any file configuration (after parsing) that has no property state gets purged
		m_pProjectFile->RemoveConfiguration( m_pFileConfig );
	}

	m_pFileConfig = NULL;
}

void CVCProjGenerator::FileExcludedFromBuild( bool bExcluded )
{
	if ( !m_pFileConfig )
	{ 
		g_pVPC->VPCSyntaxError( "Cannot set %s unless in a $File configuration context", g_pOption_ExcludedFromBuild );
	}

	BaseClass::FileExcludedFromBuild( bExcluded );

	ToolProperty_t* pToolProperty = m_pGeneratorDefinition->GetProperty( KEYWORD_GENERAL, g_pOption_ExcludedFromBuild );
	if ( !pToolProperty )
	{
		g_pVPC->VPCError( "Missing proper declaration for %s", g_pOption_ExcludedFromBuild );
	}

	m_pFileConfig->m_PropertyStates.SetBoolProperty( pToolProperty, bExcluded );
}

bool CVCProjGenerator::StartPropertySection( configKeyword_e eKeyword, bool *pbShouldSkip )
{
	BaseClass::StartPropertySection( eKeyword );

	*pbShouldSkip = false;
	m_nActivePropertySection = KEYWORD_UNKNOWN;
	bool bHandled = false;

	switch ( eKeyword )
	{
	case KEYWORD_GENERAL:
		bHandled = true;
		break;

	case KEYWORD_DEBUGGING:
		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pDebuggingTool = m_pConfig->GetDebuggingTool();
		if ( !m_pDebuggingTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_COMPILER:
	case KEYWORD_PS3_SNCCOMPILER:
	case KEYWORD_PS3_GCCCOMPILER:
		eKeyword = SetPS3VisualStudioIntegrationType( eKeyword );
		if ( eKeyword == KEYWORD_UNKNOWN )
		{
			// skip this section
			break;
		}

		m_spCompilerStack.Push( m_pCompilerTool );
		if ( m_pFileConfig )
		{
			m_pCompilerTool = m_pFileConfig->GetCompilerTool();
		}
		else
		{
			m_pCompilerTool = m_pConfig->GetCompilerTool();
		}
		if ( !m_pCompilerTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_INTELLISENSE:
		m_pIntellisenseTool = m_pConfig->GetIntellisenseTool();
		if ( !m_pIntellisenseTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_LIBRARIAN:
		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pLibrarianTool = m_pConfig->GetLibrarianTool();
		if ( !m_pLibrarianTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_LINKER:
	case KEYWORD_PS3_SNCLINKER:
	case KEYWORD_PS3_GCCLINKER:
		eKeyword = SetPS3VisualStudioIntegrationType( eKeyword );
		if ( eKeyword == KEYWORD_UNKNOWN )
		{
			// skip this section
			break;
		}

		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pLinkerTool = m_pConfig->GetLinkerTool();
		if ( !m_pLinkerTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_MANIFEST:
		if ( !( g_pVPC->IsPlatformDefined( "WIN32" ) || g_pVPC->IsPlatformDefined( "WIN64" ) ) )
		{
			// windows specific
			break;
		}

		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pManifestTool = m_pConfig->GetManifestTool();
		if ( !m_pManifestTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_XMLDOCGEN:
		if ( !( g_pVPC->IsPlatformDefined( "WIN32" ) || g_pVPC->IsPlatformDefined( "WIN64" ) ) )
		{
			// windows specific
			break;
		}

		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pXMLDocGenTool = m_pConfig->GetXMLDocGenTool();
		if ( !m_pXMLDocGenTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_BROWSEINFO:
		if ( g_pVPC->IsPlatformDefined( "PS3" ) )
		{
			// not for ps3
			break;
		}

		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pBrowseInfoTool = m_pConfig->GetBrowseInfoTool();
		if ( !m_pBrowseInfoTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_RESOURCES:
		if ( !( g_pVPC->IsPlatformDefined( "WIN32" ) || g_pVPC->IsPlatformDefined( "WIN64" ) ) )
		{
			// windows specific
			break;
		}

		m_spResourcesToolStack.Push( m_pResourcesTool );
		if ( m_pFileConfig )
		{
			m_pResourcesTool = m_pFileConfig->GetResourcesTool();
		}
		else
		{
			m_pResourcesTool = m_pConfig->GetResourcesTool();
		}
		if ( !m_pResourcesTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_PREBUILDEVENT:
		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pPreBuildEventTool = m_pConfig->GetPreBuildEventTool();
		if ( !m_pPreBuildEventTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_PRELINKEVENT:
		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pPreLinkEventTool = m_pConfig->GetPreLinkEventTool();
		if ( !m_pPreLinkEventTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_POSTBUILDEVENT:
		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pPostBuildEventTool = m_pConfig->GetPostBuildEventTool();
		if ( !m_pPostBuildEventTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_CUSTOMBUILDSTEP:
		m_spCustomBuildToolStack.Push( m_pCustomBuildTool );
		if ( m_pFileConfig )
		{
			m_pCustomBuildTool = m_pFileConfig->GetCustomBuildTool();
		}
		else
		{
			m_pCustomBuildTool = m_pConfig->GetCustomBuildTool();
		}
		if ( !m_pCustomBuildTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_XBOXIMAGE:
		if ( !g_pVPC->IsPlatformDefined( "X360" ) )
		{
			// xbox generator specific
			break;
		}

		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pXboxImageTool = m_pConfig->GetXboxImageTool();
		if ( !m_pXboxImageTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	case KEYWORD_XBOXDEPLOYMENT:
		if ( !g_pVPC->IsPlatformDefined( "X360" ) )
		{
			// xbox generator specific
			break;
		}

		if ( m_pFileConfig )
		{
			g_pVPC->VPCSyntaxError( "%s tool interface for file configuration not implemented.", g_pVPC->KeywordToName( eKeyword ) );
		}

		m_pXboxDeploymentTool = m_pConfig->GetXboxDeploymentTool();
		if ( !m_pXboxDeploymentTool )
		{
			g_pVPC->VPCError( "Could not get %s tool interface from configuration", g_pVPC->KeywordToName( eKeyword ) );
		}
		bHandled = true;
		break;

	default:
		// unknown
		return false;
	}

	if ( bHandled )
	{
		// handled
		m_nActivePropertySection = eKeyword;
	}
	else
	{
		// allow other platform specifc sections to just be quietly ignored
		*pbShouldSkip = true;
	}

	return true;
}
				
void CVCProjGenerator::EndPropertySection( configKeyword_e eKeyword )
{
	BaseClass::EndPropertySection( eKeyword );

	switch( eKeyword )
	{
	case KEYWORD_CUSTOMBUILDSTEP:
		m_spCustomBuildToolStack.Pop( m_pCustomBuildTool );
		break;

	case KEYWORD_COMPILER:
	case KEYWORD_PS3_SNCCOMPILER:
	case KEYWORD_PS3_GCCCOMPILER:
		eKeyword = SetPS3VisualStudioIntegrationType( eKeyword );
		m_spCompilerStack.Pop( m_pCompilerTool );
		break;

	case KEYWORD_RESOURCES:
		m_spResourcesToolStack.Pop( m_pResourcesTool );
		break;
	}

	m_nActivePropertySection = KEYWORD_UNKNOWN;
}

void CVCProjGenerator::HandleProperty( const char *pPropertyName, const char *pCustomScriptData )
{
	// don't allow the baseclass to alter the script state
	g_pVPC->GetScript().PushCurrentScript();
	BaseClass::HandleProperty( pPropertyName, pCustomScriptData );
	g_pVPC->GetScript().PopScript();

	if ( m_nActivePropertySection == KEYWORD_UNKNOWN )
	{
		if ( !V_stricmp_fast( pPropertyName, "$BuildOrderModifier" ) )
		{
			if ( m_pProjectFile )
			{
				m_pProjectFile->m_nBuildOrderModifier = V_atoi64( pCustomScriptData );
				return;
			}
		}
	}

	if ( pCustomScriptData )
	{
		g_pVPC->GetScript().PushScript( "HandleProperty custom data", pCustomScriptData, 1, false, false );
	}

	ToolProperty_t *pToolProperty = m_pGeneratorDefinition->GetProperty( m_nActivePropertySection, pPropertyName );
	if ( !pToolProperty )
	{
		// unknown property
		g_pVPC->VPCSyntaxError( "Unknown property %s", pPropertyName );
	}

	const char *pToken = g_pVPC->GetScript().PeekNextToken( false );
	if ( !pToken || !pToken[0] )
	{
		// quietly ignoring any property without a value (not an error)
		// TODO: this is a problem - the user cannot deliberately clear a property to the emtpy string!
		if ( pCustomScriptData )
		{
			g_pVPC->GetScript().PopScript();
		}
		return;
	}

	CProjectConfiguration *pConfig = NULL;
	CProjectTool *pTool = NULL;
	switch ( m_nActivePropertySection )
	{
	case KEYWORD_GENERAL:
		pConfig = m_pConfig;
		break;

	case KEYWORD_DEBUGGING:
		pTool = m_pDebuggingTool;
		break;

	case KEYWORD_COMPILER:
	case KEYWORD_PS3_SNCCOMPILER:
	case KEYWORD_PS3_GCCCOMPILER:
		pTool = m_pCompilerTool;
		break;

	case KEYWORD_INTELLISENSE:
		pTool = m_pIntellisenseTool;
		break;
	
	case KEYWORD_LIBRARIAN:
		pTool = m_pLibrarianTool;
		break;

	case KEYWORD_LINKER:
	case KEYWORD_PS3_SNCLINKER:
	case KEYWORD_PS3_GCCLINKER:
		pTool = m_pLinkerTool;
		break;

	case KEYWORD_MANIFEST:
		pTool = m_pManifestTool;
		break;

	case KEYWORD_XMLDOCGEN:
		pTool = m_pXMLDocGenTool;
		break;

	case KEYWORD_BROWSEINFO:
		pTool = m_pBrowseInfoTool;
		break;

	case KEYWORD_RESOURCES:
		pTool = m_pResourcesTool;
		break;

	case KEYWORD_PREBUILDEVENT:
		pTool = m_pPreBuildEventTool;
		break;

	case KEYWORD_PRELINKEVENT:
		pTool = m_pPreLinkEventTool;
		break;

	case KEYWORD_POSTBUILDEVENT:
		pTool = m_pPostBuildEventTool;
		break;

	case KEYWORD_CUSTOMBUILDSTEP:
		pTool = m_pCustomBuildTool;
		break;

	case KEYWORD_XBOXIMAGE:
		pTool = m_pXboxImageTool;
		break;

	case KEYWORD_XBOXDEPLOYMENT:
		pTool = m_pXboxDeploymentTool;
		break;

	default:
		g_pVPC->VPCError( "HandleProperty: No support for Tool:%s Property:%s - requires implementation", g_pVPC->KeywordToName( m_nActivePropertySection ), pPropertyName );
	}

	bool bHandled = false;
	if ( pTool )
	{
		bHandled = pTool->SetProperty( pToolProperty );
	}
	else if ( pConfig )
	{
		bHandled = pConfig->SetProperty( pToolProperty );
	}

	if ( !bHandled )
	{
		g_pVPC->VPCError( "HandleProperty: Failed to set %s", pPropertyName );
	}

	if ( pCustomScriptData )
	{
		g_pVPC->GetScript().PopScript();
	}
}

const char *CVCProjGenerator::GetPropertyValue( const char *pPropertyName )
{
	ToolProperty_t *pToolProperty = m_pGeneratorDefinition->GetProperty( m_nActivePropertySection, pPropertyName );
	if ( !pToolProperty )
	{
		// unknown property
		g_pVPC->VPCSyntaxError( "Unknown property %s", pPropertyName );
	}

	CProjectConfiguration *pConfig = NULL;
	CProjectTool *pTool = NULL;
	switch ( m_nActivePropertySection )
	{
	case KEYWORD_GENERAL:
		pConfig = m_pConfig;
		break;

	case KEYWORD_DEBUGGING:
		pTool = m_pDebuggingTool;
		break;

	case KEYWORD_COMPILER:
	case KEYWORD_PS3_SNCCOMPILER:
	case KEYWORD_PS3_GCCCOMPILER:
		pTool = m_pCompilerTool;
		break;

	case KEYWORD_INTELLISENSE:
		pTool = m_pIntellisenseTool;
		break;

	case KEYWORD_LIBRARIAN:
		pTool = m_pLibrarianTool;
		break;

	case KEYWORD_LINKER:
	case KEYWORD_PS3_SNCLINKER:
	case KEYWORD_PS3_GCCLINKER:
		pTool = m_pLinkerTool;
		break;

	case KEYWORD_MANIFEST:
		pTool = m_pManifestTool;
		break;

	case KEYWORD_XMLDOCGEN:
		pTool = m_pXMLDocGenTool;
		break;

	case KEYWORD_BROWSEINFO:
		pTool = m_pBrowseInfoTool;
		break;

	case KEYWORD_RESOURCES:
		pTool = m_pResourcesTool;
		break;

	case KEYWORD_PREBUILDEVENT:
		pTool = m_pPreBuildEventTool;
		break;

	case KEYWORD_PRELINKEVENT:
		pTool = m_pPreLinkEventTool;
		break;

	case KEYWORD_POSTBUILDEVENT:
		pTool = m_pPostBuildEventTool;
		break;

	case KEYWORD_CUSTOMBUILDSTEP:
		pTool = m_pCustomBuildTool;
		break;

	case KEYWORD_XBOXIMAGE:
		pTool = m_pXboxImageTool;
		break;

	case KEYWORD_XBOXDEPLOYMENT:
		pTool = m_pXboxDeploymentTool;
		break;

	default:
		g_pVPC->VPCError( "GetPropertyValue: No support for Tool:%s Property:%s - requires implementation", g_pVPC->KeywordToName( m_nActivePropertySection ), pPropertyName );
	}

	const char *propertyValue = "";
	if ( pTool )
	{
		propertyValue = pTool->GetPropertyValue( pToolProperty );
	}
	else if ( pConfig )
	{
		propertyValue = pConfig->GetPropertyValue( pToolProperty );
	}
	else
	{
		g_pVPC->VPCError( "GetPropertyValue: Failed to get %s", pPropertyName );
	}

	return propertyValue;
}

bool CVCProjGenerator::GetFolder( const char *pFolderName, CProjectFolder *pParentFolder, CProjectFolder **ppOutFolder )
{
	bool bValid;
	if ( !pParentFolder )
	{
		bValid = m_pRootFolder->GetFolder( pFolderName, ppOutFolder );
	}
	else
	{
		bValid = pParentFolder->GetFolder( pFolderName, ppOutFolder );
	}
	return bValid;
}

bool CVCProjGenerator::AddFolder( const char *pFolderName, CProjectFolder *pParentFolder, VpcFolderFlags_t iFlags, CProjectFolder **ppOutFolder )
{
	bool bValid;
	if ( !pParentFolder )
	{
		bValid = m_pRootFolder->AddFolder( pFolderName, iFlags, ppOutFolder );
	}
	else
	{
		bValid = pParentFolder->AddFolder( pFolderName, iFlags, ppOutFolder );
	}
	return bValid;
}

bool CVCProjGenerator::FindFile( const char *pFilename, CProjectFile **ppFile, bool bFixSlashes )
{
	CUtlString fixedFilename;
	if ( bFixSlashes )
	{
		// TODO: account for destination platform (see CORRECT_PATH_SEPARATOR)
		fixedFilename = pFilename;
		fixedFilename.FixSlashes( '\\' );
		pFilename = fixedFilename.Get();
	}

	int iIndex = FindFileInDictionary( pFilename );
	if ( iIndex != m_FileDictionary.InvalidIndex() )
	{
		// found
		if ( ppFile )
		{
			*ppFile = m_FileDictionary[iIndex];
		}
		return true;
	}
	
	// not found
	if ( ppFile )
	{
		*ppFile = NULL;
	}

	return false;
}

void CVCProjGenerator::AddFileToFolder( const char *pFilename, CProjectFolder *pFolder, bool bWarnIfAlreadyExists, VpcFileFlags_t iFlags, CProjectFile **ppFile )
{
	if ( FindFile( pFilename, ppFile ) )
	{
		// already present
		if ( bWarnIfAlreadyExists )
			g_pVPC->VPCWarning( "File '%s' already exists in project", pFilename );
		// check the file flags (type) is specified consistently
		if ( (*ppFile)->m_iFlags != iFlags )
			g_pVPC->VPCWarning( "File '%s' specified multiple times with conflicting types!", pFilename );
		return;
	}

	CProjectFile *pFile;
	if ( !pFolder )
	{
		// add at root
		m_pRootFolder->AddFile( pFilename, iFlags, &pFile );
	}
	else
	{
		// add at folder
		pFolder->AddFile( pFilename, iFlags, &pFile );
	}

	// add to dictionary
	m_FileDictionary.Insert( pFile );
	if ( ppFile )
	{
		*ppFile = pFile;
	}
}

bool CVCProjGenerator::RemoveFileFromFolder( const char *pFilename, CProjectFolder *pFolder )
{
	bool bFound = false;
	
	int iIndex = FindFileInDictionary( pFilename );
	if ( iIndex != m_FileDictionary.InvalidIndex() )
	{
		bFound = true;
		m_FileDictionary.RemoveAt( iIndex );
	}

	if ( !bFound )
		return false;

	if ( !pFolder )
	{
		m_pRootFolder->RemoveFile( pFilename );
	}
	else
	{
		pFolder->RemoveFile( pFilename );
	}

	return bFound;
}

void CVCProjGenerator::GetAllConfigurationNames( CUtlVector< CUtlString > &configurationNames )
{
	configurationNames.RemoveAll();
	for ( int i = 0; i < m_RootConfigurations.Count(); i++ )
	{
		configurationNames.AddToTail( m_RootConfigurations[i]->m_Name.Get() );
	}
}

void CVCProjGenerator::GetAllRootConfigurations( CUtlVector< CProjectConfiguration * > &rootConfigurations )
{
	rootConfigurations.RemoveAll();
	rootConfigurations.AddVectorToTail( m_RootConfigurations );
}

bool CVCProjGenerator::GetRootConfiguration( const char *pConfigName, CProjectConfiguration **ppConfig )
{
	if ( !pConfigName || !pConfigName[0] )
	{
		g_pVPC->VPCError( "Empty or bad configuration name." );
	}

	if ( ppConfig )
	{
		// assume not found
		*ppConfig = NULL;
	}

	for ( int i = 0; i < m_RootConfigurations.Count(); i++ )
	{
		if ( !V_stricmp_fast( m_RootConfigurations[i]->m_Name.Get(), pConfigName ) )
		{
			// found
			if ( ppConfig )
			{
				*ppConfig = m_RootConfigurations[i];
			}
			return true;
		}
	}

	return false;
}

bool CVCProjGenerator::IsConfigurationNameValid( const char *pConfigName )
{
	return GetRootConfiguration( pConfigName, NULL );
}

configKeyword_e CVCProjGenerator::SetPS3VisualStudioIntegrationType( configKeyword_e eKeyword )
{
	PS3VSIType_e vsiType = PS3_VSI_TYPE_UNDEFINED;

	switch ( eKeyword )
	{
	case KEYWORD_COMPILER:
	case KEYWORD_LINKER:
		if ( !g_pVPC->IsPlatformDefined( "PS3" ) )
		{
			return eKeyword;
		}

		if ( m_VSIType == PS3_VSI_TYPE_UNDEFINED )
		{
			// PS3 defaults to SNC, unless explictly specified
			vsiType = PS3_VSI_TYPE_SNC;
		}
		else
		{
			// already set
			vsiType = m_VSIType;
		}
		break;

	case KEYWORD_PS3_SNCCOMPILER:
	case KEYWORD_PS3_SNCLINKER:
		if ( !g_pVPC->IsPlatformDefined( "PS3" ) )
		{
			// ps3 generator specific
			// not available for other platforms
			return KEYWORD_UNKNOWN;
		}
		vsiType = PS3_VSI_TYPE_SNC;
		break;

	case KEYWORD_PS3_GCCCOMPILER:
	case KEYWORD_PS3_GCCLINKER:
		if ( !g_pVPC->IsPlatformDefined( "PS3" ) )
		{
			// ps3 generator specific
			// not available for other platforms
			return KEYWORD_UNKNOWN;
		}
		vsiType = PS3_VSI_TYPE_GCC;
		break;

	default:
		g_pVPC->VPCError( "Unknown PS3 compiler/linker type" );
		break;
	}

	if ( m_VSIType == PS3_VSI_TYPE_UNDEFINED )
	{
		// once set, compiler/linker choice (snc or gcc) cannot be changed
		m_VSIType = vsiType;
	}
	else if ( m_VSIType != vsiType )
	{
		// cannot intermix tool properties, they must be exclusive
		g_pVPC->VPCSyntaxError( "PS3 compiler/linker (GCC or SNC) already set, cannot be changed" );
	}

	// remap ambiguous compiler/linker tool to explicit SNC/GCC tool flavor
	if ( eKeyword == KEYWORD_COMPILER )
	{
		eKeyword = ( m_VSIType == PS3_VSI_TYPE_SNC ) ? KEYWORD_PS3_SNCCOMPILER : KEYWORD_PS3_GCCCOMPILER;
	}
	else if ( eKeyword == KEYWORD_LINKER )
	{
		eKeyword = ( m_VSIType == PS3_VSI_TYPE_SNC ) ? KEYWORD_PS3_SNCLINKER : KEYWORD_PS3_GCCLINKER;
	}

	return eKeyword;
}

void CVCProjGenerator::ApplyInternalPreprocessorDefinitions()
{
	// prep to add in vpc generated compiler defines
	CUtlVector< CMacro* > macroDefines;
	g_pVPC->GetMacrosMarkedForCompilerDefines( macroDefines );

	if ( !macroDefines.Count() )
	{
		// nothing to fixup
		return;
	}

	// get all the vpc macros that have been marked for auto adding as compiler define
	CUtlString extraDefineString;
	for ( int i = 0; i < macroDefines.Count(); i++ )
	{
		CMacro *pMacro = macroDefines[i];

		CUtlString tempString;
		tempString.Format( ";%s=%s", pMacro->GetName(), pMacro->GetValue() );
		extraDefineString += tempString;
	}

	// fixup root configurations
	for ( int i = 0; i < m_RootConfigurations.Count(); i++ )
	{
		CCompilerTool *pCompilerTool = m_RootConfigurations[i]->GetCompilerTool();
		if ( pCompilerTool )
		{
			PropertyState_t *pPropertyState = pCompilerTool->m_PropertyStates.GetProperty( "$PreprocessorDefinitions" );
			if ( pPropertyState )
			{
				pPropertyState->m_StringValue += extraDefineString;

				//remove empty defines from the beginning
				{
					char *pValidString = pPropertyState->m_StringValue.Access();
					while ( *pValidString == ';' )
					{
						++pValidString;
					}
					
					//I can't find a good way to shift a CUtlString in place, so copy out the valid substring and put it back.
					CUtlString temp = pValidString;
					pPropertyState->m_StringValue = temp;
				}
			}
		}
	}

	// fixup any file configuration overrides
	for ( int iIndex = m_FileDictionary.FirstInorder(); iIndex != m_FileDictionary.InvalidIndex(); iIndex = m_FileDictionary.NextInorder( iIndex ) )
	{
		CProjectFile *pProjectFile = m_FileDictionary[iIndex];
		for ( int i = 0; i < pProjectFile->m_Configs.Count(); i++ )
		{
			CCompilerTool *pCompilerTool = pProjectFile->m_Configs[i]->GetCompilerTool();
			if ( pCompilerTool )
			{
				PropertyState_t *pPropertyState = pCompilerTool->m_PropertyStates.GetProperty( "$PreprocessorDefinitions" );
				if ( pPropertyState )
				{
					pPropertyState->m_StringValue += extraDefineString;

					//remove empty defines from the beginning
					{
						char *pValidString = pPropertyState->m_StringValue.Access();
						while ( *pValidString == ';' )
						{
							++pValidString;
						}
					
						//I can't find a good way to shift a CUtlString in place, so copy out the valid substring and put it back.
						CUtlString temp = pValidString;
						pPropertyState->m_StringValue = temp;
					}
				}
			}
		}
	}
}

void CVCProjGenerator::LogOutputFiles( const char *pConfigName )
{
    const char *configurationTypeString = GetPropertyValueAsString( NULL, pConfigName, KEYWORD_GENERAL, g_pOption_ConfigurationType );
    const char *importLibraryString = "";
    const char *outputFileString = "";
    const char *gameOutputFileString = "";
    
    if ( V_stristr( configurationTypeString, ".dll" ) )
    {
        importLibraryString = GetPropertyValueAsString( NULL, pConfigName, KEYWORD_LINKER, g_pOption_ImportLibrary );
        outputFileString = GetPropertyValueAsString( NULL, pConfigName, KEYWORD_LINKER, g_pOption_OutputFile );
        gameOutputFileString = GetPropertyValueAsString( NULL, pConfigName, KEYWORD_GENERAL, g_pOption_GameOutputFile );
    }
    else if ( V_stristr( configurationTypeString, ".lib" ) )
    {
        outputFileString = GetPropertyValueAsString( NULL, pConfigName, KEYWORD_LIBRARIAN, g_pOption_OutputFile );
    }
    else if ( V_stristr( configurationTypeString, ".exe" ) )
    {
        outputFileString = GetPropertyValueAsString( NULL, pConfigName, KEYWORD_LINKER, g_pOption_OutputFile );
        gameOutputFileString = GetPropertyValueAsString( NULL, pConfigName, KEYWORD_GENERAL, g_pOption_GameOutputFile );
    }

    const char *outBinDirString = g_pVPC->GetMacroValue( "OUTBINDIR" );
    char pathString[MAX_FIXED_PATH];

    CUtlStringBuilder *pVsStr = g_pVPC->GetTempStringBuffer1();
    if ( importLibraryString[0] )
    {
        CBaseProjectDataCollector::DoStandardVisualStudioReplacements( importLibraryString, pVsStr, NULL );
        pVsStr->Replace( "$(OutDir)", outBinDirString );
        V_MakeAbsolutePath( pathString, ARRAYSIZE( pathString ), pVsStr->Get(), NULL, k_bVPCForceLowerCase );
        g_pVPC->VPCStatus( false, "$ImportLibrary: %s", pathString );
    }
    if ( outputFileString[0] )
    {
        CBaseProjectDataCollector::DoStandardVisualStudioReplacements( outputFileString, pVsStr, NULL );
        pVsStr->Replace( "$(OutDir)", outBinDirString );
        V_MakeAbsolutePath( pathString, ARRAYSIZE( pathString ), pVsStr->Get(), NULL, k_bVPCForceLowerCase );
        g_pVPC->VPCStatus( false, "$OutputFile: %s", pathString );
    }
    if ( gameOutputFileString[0] )
    {
        CBaseProjectDataCollector::DoStandardVisualStudioReplacements( gameOutputFileString, pVsStr, NULL );
        pVsStr->Replace( "$(OutDir)", outBinDirString );
        V_MakeAbsolutePath( pathString, ARRAYSIZE( pathString ), pVsStr->Get(), NULL, k_bVPCForceLowerCase );
        g_pVPC->VPCStatus( false, "$GameOutputFile: %s", pathString );
    }
}

int CVCProjGenerator::FindFileInDictionary( const char *pFilename )
{
	for ( int iIndex = m_FileDictionary.FirstInorder(); iIndex != m_FileDictionary.InvalidIndex(); iIndex = m_FileDictionary.NextInorder( iIndex ) )
	{
		CProjectFile *pProjectFile = m_FileDictionary[iIndex];

		if ( V_stricmp_fast( pProjectFile->m_Name.Get(), pFilename ) == 0 )
		{
			return iIndex;
		}
	}

	return m_FileDictionary.InvalidIndex();
}

void CVCProjGenerator::GetAllProjectFiles( CUtlVector< CProjectFile * > &projectFiles )
{
	projectFiles.RemoveAll();
	for ( int iIndex = m_FileDictionary.FirstInorder(); iIndex != m_FileDictionary.InvalidIndex(); iIndex = m_FileDictionary.NextInorder( iIndex ) )
	{
		CProjectFile *pProjectFile = m_FileDictionary[iIndex];
		projectFiles.AddToTail( pProjectFile );
	}
}

bool CVCProjGenerator::HasFilePropertyValue( CProjectFile *pProjectFile, const char *pConfigrationName, configKeyword_e configKeyword, const char *pPropertyName )
{
	// find the specific tool property
	ToolProperty_t *pToolProperty = GetGeneratorDefinition()->GetProperty( configKeyword, pPropertyName );
	if ( !pToolProperty )
	{
		g_pVPC->VPCError( "Unknown property %s expected for section %s", pPropertyName, g_pVPC->KeywordToName( configKeyword ) );
        return false;
	}

	CProjectConfiguration *pFileConfiguration = NULL;
    pProjectFile->GetConfiguration( pConfigrationName, &pFileConfiguration );
    if ( !pFileConfiguration )
    {
        return false;
    }

	CProjectTool *pFileTool = NULL;
	switch( configKeyword )
	{
	case KEYWORD_COMPILER:
		pFileTool = pFileConfiguration->GetCompilerTool();
		break;

	case KEYWORD_LIBRARIAN:	
		pFileTool = pFileConfiguration->GetLibrarianTool();
		break;

	case KEYWORD_LINKER:	
		pFileTool = pFileConfiguration->GetLinkerTool();
		break;

	case KEYWORD_POSTBUILDEVENT:
		pFileTool = pFileConfiguration->GetPostBuildEventTool();
		break;

	case KEYWORD_CUSTOMBUILDSTEP:
		pFileTool = pFileConfiguration->GetCustomBuildTool();
		break;

	default:
		// add support as needed
		g_pVPC->VPCError( "Unsupported tool expected for section %s", g_pVPC->KeywordToName( configKeyword ) );
        return false;
	}

    if ( !pFileTool )
    {
        return false;
    }
    
    const char *pProp = pFileTool->m_PropertyStates.GetPropertyValue( pToolProperty, NULL );
    return pProp && pProp[0];
}

//--------------------------------------------------------------------------------------------------
// Utility support for querying property, respecting proper file-to-global state cascade.
//--------------------------------------------------------------------------------------------------
const char *CVCProjGenerator::GetPropertyValueAsString( CProjectFile *pProjectFile, const char *pConfigrationName, configKeyword_e configKeyword, const char *pPropertyName, const char *pDefaultValue )
{
	// find the specific tool property
	ToolProperty_t *pToolProperty = GetGeneratorDefinition()->GetProperty( configKeyword, pPropertyName );
	if ( !pToolProperty )
	{
		g_pVPC->VPCError( "Unknown property %s expected for section %s", pPropertyName, g_pVPC->KeywordToName( configKeyword ) );
	}

	CProjectConfiguration *pRootConfiguration = NULL; 
	GetRootConfiguration( pConfigrationName, &pRootConfiguration );

	CProjectConfiguration *pFileConfiguration = NULL; 
	if ( pProjectFile )
	{
		// get the file's specified configuration
		pProjectFile->GetConfiguration( pConfigrationName, &pFileConfiguration );
	}
	
	CProjectTool *pRootTool = NULL;
	CProjectTool *pFileTool = NULL;
	switch( configKeyword )
	{
	case KEYWORD_GENERAL:
		// there are no tools associated, there are only configs
		break;

	case KEYWORD_COMPILER:
		pRootTool = pRootConfiguration ? pRootConfiguration->GetCompilerTool() : NULL;
		pFileTool = pFileConfiguration ? pFileConfiguration->GetCompilerTool() : NULL;
		break;

	case KEYWORD_INTELLISENSE:
		pRootTool = pRootConfiguration ? pRootConfiguration->GetIntellisenseTool() : nullptr;
		pFileTool = pFileConfiguration ? pFileConfiguration->GetIntellisenseTool() : nullptr;
		break;

	case KEYWORD_LIBRARIAN:	
		pRootTool = pRootConfiguration ? pRootConfiguration->GetLibrarianTool() : NULL;
		pFileTool = pFileConfiguration ? pFileConfiguration->GetLibrarianTool() : NULL;
		break;

	case KEYWORD_LINKER:	
		pRootTool = pRootConfiguration ? pRootConfiguration->GetLinkerTool() : NULL;
		pFileTool = pFileConfiguration ? pFileConfiguration->GetLinkerTool() : NULL;
		break;

	case KEYWORD_PREBUILDEVENT:
		pRootTool = pRootConfiguration ? pRootConfiguration->GetPreBuildEventTool() : NULL;
		pFileTool = pFileConfiguration ? pFileConfiguration->GetPreBuildEventTool() : NULL;
		break;

	case KEYWORD_PRELINKEVENT:
		pRootTool = pRootConfiguration ? pRootConfiguration->GetPreLinkEventTool() : NULL;
		pFileTool = pFileConfiguration ? pFileConfiguration->GetPreLinkEventTool() : NULL;
		break;

	case KEYWORD_POSTBUILDEVENT:
		pRootTool = pRootConfiguration ? pRootConfiguration->GetPostBuildEventTool() : NULL;
		pFileTool = pFileConfiguration ? pFileConfiguration->GetPostBuildEventTool() : NULL;
		break;

	case KEYWORD_CUSTOMBUILDSTEP:
		pRootTool = pRootConfiguration ? pRootConfiguration->GetCustomBuildTool() : NULL;
		pFileTool = pFileConfiguration ? pFileConfiguration->GetCustomBuildTool() : NULL;
		break;

	default:
		// add support as needed
		g_pVPC->VPCError( "Unsupported tool expected for section %s", g_pVPC->KeywordToName( configKeyword ) );
	}

	const char *propertyValue = "";
	if ( pFileTool )
	{
		// properly cascades into root tool config as needed
		propertyValue = pFileTool->GetPropertyValue( pToolProperty );
	}
	else if ( pRootTool )
	{
		propertyValue = pRootTool->GetPropertyValue( pToolProperty );
	}
	else if ( pFileConfiguration )
	{
		// properly cascades into root config as needed
		propertyValue = pFileConfiguration->GetPropertyValue( pToolProperty );
	}
	else if ( pRootConfiguration )
	{
		propertyValue = pRootConfiguration->GetPropertyValue( pToolProperty );
	}

	if ( ( !propertyValue || !propertyValue[0] ) && pDefaultValue && pDefaultValue[0] )
	{
		// use the supplied default value
		propertyValue = pDefaultValue;
	}

	return propertyValue;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CVCProjGenerator::GetPropertyValueAsBool( CProjectFile *pProjectFile, const char *pConfigrationName, configKeyword_e configKeyword, const char *pPropertyName, bool bDefaultValue )
{
	const char *valueString = GetPropertyValueAsString( pProjectFile, pConfigrationName, configKeyword, pPropertyName, bDefaultValue ? "1" : "0" );
	return Sys_StringToBool( valueString );
}

static void addFilesToDependenciesProperty( const char *szSemicolonSeparatedFileList, ToolProperty_t *pToolProperty_Dependencies, CCustomBuildTool *pCustomBuildTool, ToolProperty_t *pToolProperty_CommandLine )
{
	CPropertyStates &propertyStates = pCustomBuildTool->m_PropertyStates;

	//search the properties for $CommandLine and bail if it doesn't exist so we don't add dependencies to bogus custom build tools
	PropertyState_t *pCommandLineProperty = propertyStates.GetProperty( pToolProperty_CommandLine->m_nPropertyId );

	//no command line, no need for a dependency
	if ( !pCommandLineProperty || pCommandLineProperty->m_StringValue.IsEmpty() )
		return;

	//search the properties for $AdditionalDependencies
	PropertyState_t *pAdditionalDependenciesProperty = propertyStates.GetProperty( pToolProperty_Dependencies->m_nPropertyId );

	if ( !pAdditionalDependenciesProperty )
	{
		//no additional properties already defined, add it
		int iIndex = pCustomBuildTool->m_PropertyStates.m_Properties.AddToTail();
		pAdditionalDependenciesProperty = &pCustomBuildTool->m_PropertyStates.m_Properties[iIndex];
		pAdditionalDependenciesProperty->m_pToolProperty = pToolProperty_Dependencies;

		pCustomBuildTool->m_PropertyStates.m_PropertiesInOutputOrder.Insert( iIndex );
	}

	//append dependencies
	if ( !pAdditionalDependenciesProperty->m_StringValue.IsEmpty() )
	{
		pAdditionalDependenciesProperty->m_StringValue += ";";
	}
	pAdditionalDependenciesProperty->m_StringValue += szSemicolonSeparatedFileList;
}

static void ReplaceHackMacro_HACK_DEPENDENCIES_ALLVPCSCRIPTS( PropertyState_t *pProperty, CUtlString &scriptDependencies )
{
	if ( pProperty->m_StringValue.IsEmpty() )
		return;

	char *szExistingString = pProperty->m_StringValue.GetForModify();

	char *pFound = V_strstr( szExistingString, "$HACK_DEPENDENCIES_ALLVPCSCRIPTS" );
	if ( pFound )
	{
		uint32 nFoundIndex = pFound - szExistingString;
		CUtlString replacementString = pProperty->m_StringValue.Slice( 0, nFoundIndex );
		replacementString += scriptDependencies;
		replacementString += pProperty->m_StringValue.Slice( nFoundIndex + V_strlen( "$HACK_DEPENDENCIES_ALLVPCSCRIPTS" ), pProperty->m_StringValue.Length() );
		pProperty->m_StringValue = replacementString;
	}
}

void CVCProjGenerator::EvaluateHackMacro_HACK_DEPENDENCIES_ALLVPCSCRIPTS( void )
{
	//Search all the custom build tools looking for $DEPENDENCIES_ALLSCRIPTS and replace it with every script file referenced
	// Need to evaluate this late instead of up-front, which is pretty hacky

	//ensure that the macro does not exist
	Assert( !g_pVPC->FindMacro( "HACK_DEPENDENCIES_ALLVPCSCRIPTS" ) );

	ToolProperty_t *pToolProperty_Global = GetGeneratorDefinition()->GetProperty( KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies_Proj );
	ToolProperty_t *pToolProperty_File = GetGeneratorDefinition()->GetProperty( KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies );
	ToolProperty_t *pToolProperty_CommandLine = GetGeneratorDefinition()->GetProperty( KEYWORD_CUSTOMBUILDSTEP, g_pOption_CommandLine );

	if ( !Verify( pToolProperty_Global && pToolProperty_File && pToolProperty_CommandLine ) )
	{
		g_pVPC->VPCError( "Unknown property %s && %s && %s expected for section %s", g_pOption_AdditionalDependencies_Proj, g_pOption_AdditionalDependencies, g_pOption_CommandLine, g_pVPC->KeywordToName( KEYWORD_CUSTOMBUILDSTEP ) );
		return;
	}

	//generate the list of vpc scripts in semicolon separated list
	CUtlString scriptDependencies;
	for ( int i = 0; i < g_pVPC->m_ScriptList.Count(); ++i )
	{
		scriptList_t *pScript = &g_pVPC->m_ScriptList[i];

		if ( pScript->m_bCRCCheck && !pScript->m_scriptName.IsEmpty() )
		{
			if ( !scriptDependencies.IsEmpty() )
			{
				scriptDependencies += ";";
			}

			char szRelativeScriptPath[MAX_PATH];
			if ( V_IsAbsolutePath( pScript->m_scriptName ) )
			{
				V_MakeRelativePath( pScript->m_scriptName, g_pVPC->GetProjectPath(), szRelativeScriptPath, ARRAYSIZE( szRelativeScriptPath ) );
			}
			else
			{
				V_strncpy( szRelativeScriptPath, pScript->m_scriptName, ARRAYSIZE( szRelativeScriptPath ) );
			}
			V_RemoveDotSlashes( szRelativeScriptPath );

			scriptDependencies += szRelativeScriptPath;
		}
	}

	if ( scriptDependencies.IsEmpty() )
		return;

	CProjectConfiguration **ppRootConfigurations = m_RootConfigurations.Base();
	int nRootConfigurations = m_RootConfigurations.Count();

	//loop global configurations looking for custom build tools
	for ( int nConfigIter = 0; nConfigIter < nRootConfigurations; ++nConfigIter )
	{
		CProjectConfiguration *pRootConfig = ppRootConfigurations[nConfigIter];
		
		//grab the custom build tool if it's been specified
		CCustomBuildTool *pCustomBuildTool = pRootConfig->GetCustomBuildTool();
		if ( !pCustomBuildTool )
			continue; //no custom build tool, nothing to update

		PropertyState_t *pDependencies = pCustomBuildTool->m_PropertyStates.GetProperty( pToolProperty_Global->m_nPropertyId );
		if ( !pDependencies )
				continue;
		
		ReplaceHackMacro_HACK_DEPENDENCIES_ALLVPCSCRIPTS( pDependencies, scriptDependencies );
	}
	
	//iterate all files looking for custom build tools
	CUtlVector< CProjectFile * > projectFiles;
	GetAllProjectFiles( projectFiles );
	for ( int nFileIter = 0; nFileIter < projectFiles.Count(); ++nFileIter )
	{
		CProjectFile *pProjectFile = projectFiles[nFileIter];

		//custom build tools are defined per config, so loop all configurations
		for ( int nConfigIter = 0; nConfigIter < nRootConfigurations; ++nConfigIter )
		{
			CProjectConfiguration *pRootConfig = ppRootConfigurations[nConfigIter];

			//grab the file configuration if there is one
			CProjectConfiguration *pFileConfiguration = NULL;
			if ( !pProjectFile->GetConfiguration( pRootConfig->m_Name.Get(), &pFileConfiguration ) )
				continue; //no file config, nothing to update

			//grab the custom build tool if it's been specified
			CCustomBuildTool *pCustomBuildTool = pFileConfiguration->GetCustomBuildTool();
			if ( !pCustomBuildTool )
				continue; //no custom build tool, nothing to update

			PropertyState_t *pDependencies = pCustomBuildTool->m_PropertyStates.GetProperty( pToolProperty_File->m_nPropertyId );
			if ( !pDependencies )
				continue;

			ReplaceHackMacro_HACK_DEPENDENCIES_ALLVPCSCRIPTS( pDependencies, scriptDependencies );
		}
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//Add extra files as a timestamp dependency for all custom build tools
// At least .mak and .vcxproj files have some holes where dependencies may remain falsely valid when a vpc script change
//  or mode change intended to invalidate it. So let's add extra triggers to invalidate all custom build steps easily
void CVCProjGenerator::AddIndirectCustomBuildDependencies( void )
{
	// MSVC has a crappy problem where updates to the "$AdditionalDependencies" field does not invalidate existing results on it's own
	// So new dependencies are not tracked until an existing dependency invalidates the results

	// MAK files have a somewhat crappy problem where updates to a recipe's commands do not invalidate existing timestamp based results
	
	ToolProperty_t *pToolProperty_Global = GetGeneratorDefinition()->GetProperty( KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies_Proj );
	ToolProperty_t *pToolProperty_File = GetGeneratorDefinition()->GetProperty( KEYWORD_CUSTOMBUILDSTEP, g_pOption_AdditionalDependencies );
	ToolProperty_t *pToolProperty_CommandLine = GetGeneratorDefinition()->GetProperty( KEYWORD_CUSTOMBUILDSTEP, g_pOption_CommandLine );

	if ( !Verify( pToolProperty_Global && pToolProperty_File && pToolProperty_CommandLine ) )
	{
		g_pVPC->VPCError( "Unknown property %s && %s && %s expected for section %s", g_pOption_AdditionalDependencies_Proj, g_pOption_AdditionalDependencies, g_pOption_CommandLine, g_pVPC->KeywordToName( KEYWORD_CUSTOMBUILDSTEP ) );
		return;
	}

	CProjectConfiguration **ppRootConfigurations = m_RootConfigurations.Base();
	int nRootConfigurations = m_RootConfigurations.Count();

	CUtlVector< CProjectFile * > projectFiles;
	GetAllProjectFiles( projectFiles );

	bool bWarnedTrivial = false;

	//depend on $SRCDIR/vpc_scripts/vpc_trivial_dependency.txt
	for ( int nConfigIter = 0; nConfigIter < nRootConfigurations; ++nConfigIter )
	{
		CProjectConfiguration *pRootConfig = ppRootConfigurations[nConfigIter];
		CUtlString addDependencies;

		//depending on the output .mak/.vcxproj/.xcodeproj file would be the easiest setup, but may cause overbuilding
		//addDependencies = g_pVPC->GetOutputFilename();
				
		//grab the VPC_TRIVIAL_DEPENDENCY_PATH and add it
		CMacro *pTrivialDependencyPath;
		{
			pTrivialDependencyPath = g_pVPC->FindMacro( "VPC_TRIVIAL_DEPENDENCY_PATH", pRootConfig->m_Name );
			if ( !pTrivialDependencyPath )
			{
				pTrivialDependencyPath = g_pVPC->FindMacro( "VPC_TRIVIAL_DEPENDENCY_PATH", nullptr );
			}

			CUtlStringBuilder trivialDependencyPath;
			trivialDependencyPath.EnsureCapacity( 2048 );
			if ( pTrivialDependencyPath )
			{
				g_pVPC->ResolveMacrosInString( "$VPC_TRIVIAL_DEPENDENCY_PATH", &trivialDependencyPath );
			}
		
			if ( !trivialDependencyPath.IsEmpty() )
			{
				CUtlPathStringHolder cleanFilename( trivialDependencyPath );
				cleanFilename.FixSlashesAndDotSlashes();
				if ( !addDependencies.IsEmpty() )
				{
					addDependencies += ";";
				}
				addDependencies += cleanFilename.Get();
			}
		}

		//try to add dependencies to the global custom build tool
		{
			CCustomBuildTool *pCustomBuildTool = pRootConfig->GetCustomBuildTool();
			if ( !pCustomBuildTool )
				continue; //no custom build tool, nothing to update

			if ( !pTrivialDependencyPath && !bWarnedTrivial )
			{
				g_pVPC->VPCWarning( "$VPC_TRIVIAL_DEPENDENCY_PATH should be defined somewhere in the script if you have custom build tools.\n"
									"\tOtherwise pain and suffering are more likely at a later date\n" );

				bWarnedTrivial = true;
			}
		
			addFilesToDependenciesProperty( addDependencies.Get(), pToolProperty_Global, pCustomBuildTool, pToolProperty_CommandLine );
		}

		//iterate all files looking for custom build tools to add dependencies to	
		for ( int nFileIter = 0; nFileIter < projectFiles.Count(); ++nFileIter )
		{
			CProjectFile *pProjectFile = projectFiles[nFileIter];			

			//grab the file configuration if there is one
			CProjectConfiguration *pFileConfiguration = NULL;
			if ( !pProjectFile->GetConfiguration( pRootConfig->m_Name.Get(), &pFileConfiguration ) )
				continue; //no file config, nothing to update

			//grab the custom build tool if it's been specified
			CCustomBuildTool *pCustomBuildTool = pFileConfiguration->GetCustomBuildTool();
			if ( !pCustomBuildTool )
				continue; //no custom build tool, nothing to update

			if ( !pTrivialDependencyPath && !bWarnedTrivial )
			{
				g_pVPC->VPCWarning( "$VPC_TRIVIAL_DEPENDENCY_PATH should be defined somewhere in the script if you have custom build tools.\n"
									"\tTo provide a guaranteed way of resolving compilers falsely believing the tool results are up to date\n" );

				bWarnedTrivial = true;
			}
			
			addFilesToDependenciesProperty( addDependencies.Get(), pToolProperty_File, pCustomBuildTool, pToolProperty_CommandLine );
		}
	}
}

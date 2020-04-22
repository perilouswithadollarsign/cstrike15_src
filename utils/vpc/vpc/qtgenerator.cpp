//========= Copyright 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: Qt MOC integration
//
//=====================================================================================//

#include "vpc.h"
#include "baseprojectdatacollector.h"
#include "projectgenerator_vcproj.h"


//-----------------------------------------------------------------------------

#define QT_COMPILER_CONFIG_START		"\n"																				\
										"{\n"   																			\
										"	$Configuration\n"   															\
										"	{\n"																			\
										"		$Compiler\n"																\
										"		{\n"																		\
										"			$DisableSpecificWarnings		\"$BASE;$QT_MOC_DISABLED_WARNINGS\"\n"



#define QT_COMPILER_CONFIG_END			"		}\n"																		\
										"	}\n"																			\
										"}\n"


#define QT_FAKEMOC_BLOCK				"\n"																				\
										"$DynamicFile \"%s\"\n"																\
										"$DynamicFile \"%s\"\n"																\
										"{\n"   																			\
										"	$Configuration\n"   															\
										"	{\n"																			\
										"		$CustomBuildStep\n"															\
										"		{\n"																		\
										"			$AdditionalDependencies \"%s\"\n"										\
										"			$CommandLine			\"$QT_MOC_COMMAND_PREFIX %s -o %s\"\n"			\
										"			$Description			\"$QT_DESCRIPTION_CPP\"\n"						\
										"			$Outputs				\"$QT_OUTPUT_CPP\"\n"							\
										"		}\n"																		\
										"	}\n"																			\
										"}\n"



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_VerifyQtMacrosPresent()
{
	if ( 0 == V_strlen( g_pVPC->GetMacroValue( "QT_MACROS_DEFINED" ) ) )
	{
		g_pVPC->VPCError( "Cannot have Qt files without defining the Qt macros (QT_MACROS_DEFINED) - ensure you include the core Qt script (in project %s).", g_pVPC->GetProjectName() );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Qt_TrackFile( const char *pName, bool bRemove, VpcFileFlags_t iFileFlags )
{
#ifdef STEAM
	return;
#else

	if ( !bRemove )
	{
		// Ignore files without the QT flag
		if ( !( iFileFlags & VPC_FILE_FLAGS_QT ) )
			return;

		// Skip Qt processing during dependency-generation (let that code see the original, unmodified contents of the VPC)
		if ( g_pVPC->m_bIsDependencyPass )
			return;

		// Ignore if this script opts out of the Qt feature
		if ( g_pVPC->IsConditionalDefined( "NOQTFOLDER" ) )
		{
			g_pVPC->VPCWarning( "Ignoring Qt file '%s', project '%s' specifies NOQTFOLDER!", pName, g_pVPC->GetProjectName() );
			return;
		}

		// Ignore if the Qt feature is disabled
		if ( !g_pVPC->IsQtEnabled() )
			return;

		// Add the file (if not already added)
		if ( !g_pVPC->m_QtFiles.HasElement( pName ) )
			g_pVPC->m_QtFiles.AddToTail( pName );
	}
	else
	{
		// remove the file
		g_pVPC->m_QtFiles.FindAndRemove( pName );
		return;
	}

	VPC_VerifyQtMacrosPresent();

	// Qt header files get a custom build step
	const char *pExt = V_GetFileExtension( pName );
	if ( IsHFileExtension( pExt ) )
	{
		CUtlVector< CUtlString > configurationNames;
		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );

		bool bShouldSkip;
		for ( int i=0; i < configurationNames.Count(); i++ )
		{
			g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), true );
			g_pVPC->GetProjectGenerator()->StartPropertySection( KEYWORD_CUSTOMBUILDSTEP, &bShouldSkip );
				g_pVPC->GetProjectGenerator()->HandleProperty( "$CommandLine", "\"$QT_CUSTOM_BUILD_H\"" );
				g_pVPC->GetProjectGenerator()->HandleProperty( "$Description", "\"$QT_DESCRIPTION_H\"" );
				g_pVPC->GetProjectGenerator()->HandleProperty( "$Outputs", "\"$QT_OUTPUT_H\"" );
			g_pVPC->GetProjectGenerator()->EndPropertySection( KEYWORD_CUSTOMBUILDSTEP );
			g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
		}
	}
	else if ( !V_stricmp_fast( pExt, "ui" ) )
	{
		CUtlVector< CUtlString > configurationNames;
		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );

		bool bShouldSkip;
		for ( int i=0; i < configurationNames.Count(); i++ )
		{
			g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), true );
			g_pVPC->GetProjectGenerator()->StartPropertySection( KEYWORD_CUSTOMBUILDSTEP, &bShouldSkip );
			g_pVPC->GetProjectGenerator()->HandleProperty( "$CommandLine", "\"$QT_CUSTOM_BUILD_UI\"" );
			g_pVPC->GetProjectGenerator()->HandleProperty( "$Description", "\"$QT_DESCRIPTION_UI\"" );
			g_pVPC->GetProjectGenerator()->HandleProperty( "$Outputs", "\"$QT_OUTPUT_UI\"" );
			g_pVPC->GetProjectGenerator()->EndPropertySection( KEYWORD_CUSTOMBUILDSTEP );
			g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
		}
	}
#endif
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Qt_OnParseProjectStart( void )
{
	g_pVPC->m_QtFiles.Purge();
	g_pVPC->m_QtOutputFileMap.Clear();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Qt_GetPCHFile( CVCProjGenerator *pDataCollector, const char *pQtHeaderFile, CUtlString *pOutPCHFile )
{
	CUtlVector<CProjectConfiguration *> rootConfigs;
	pDataCollector->GetAllRootConfigurations( rootConfigs );

	CSourceFileInfo fileInfo;
		
	char pFileNoExt[ MAX_BASE_FILENAME ];
	V_StripExtension( pQtHeaderFile, pFileNoExt, sizeof( pFileNoExt ) );

	char cppFileName[ MAX_BASE_FILENAME ];
	const int nNumCFileExtensions = GetNumCFileExtensions();

	for ( int i = 0; i < nNumCFileExtensions; ++i )
	{
		V_strcpy_safe( cppFileName, pFileNoExt );
		V_strcat_safe( cppFileName, "." );
		V_strcat_safe( cppFileName, GetCFileExtension( i ) );
				
		CProjectFile *pProjectFile = NULL;
		if ( pDataCollector->FindFile( cppFileName, &pProjectFile ) )
		{					
			VPC_GeneratedFiles_GetSourceFileInfo( fileInfo, pProjectFile, true, pDataCollector, rootConfigs );
			*pOutPCHFile = fileInfo.m_PCHName;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Qt_OnParseProjectEnd( CVCProjGenerator *pDataCollector )
{
	if ( g_pVPC->m_QtFiles.Count() == 0 )
		return;


	CUtlBuffer vpcBuffer;
	vpcBuffer.SetBufferType( true, true );
	vpcBuffer.Printf( "$Folder \"%s\"\n{\n", g_QtFolderName );

	CUtlStringBuilder qtTargetSubdir;
	g_pVPC->ResolveMacrosInString( "$QT_TARGET_SUBDIR", &qtTargetSubdir );
	CUtlStringBuilder qtUISubdir;
	g_pVPC->ResolveMacrosInString( "$QT_UI_SUBDIR", &qtUISubdir );

	for ( int i = 0; i < g_pVPC->m_QtFiles.Count(); ++i )
	{
		const char *pFilename = g_pVPC->m_QtFiles[i].Get();

		char pFileBase[MAX_BASE_FILENAME];
		V_FileBase( pFilename, pFileBase, sizeof( pFileBase ) );

		char pFileNoExt[MAX_BASE_FILENAME];
		V_StripExtension( pFilename, pFileNoExt, sizeof(pFileNoExt) );

        const char *pFileSuff = "";
		const char *pExt = V_GetFileExtension( pFilename );
		if ( IsHFileExtension( pExt ) )
		{
			CUtlString pchConfig;			
			CUtlString pchFile;
			VPC_Qt_GetPCHFile( pDataCollector, pFilename, &pchFile );

			if ( pchFile.IsEmpty() )
			{
                pFileSuff = "_NoPCH";
			}
			else
			{
				pchConfig.Format(	"			$Create/UsePCHThroughFile 	\"%s\"\n"	\
									"			$ForceIncludes				\"%s\"\n", pchFile.Get(), pchFile.Get() );
			}

			CUtlString configBlock = QT_COMPILER_CONFIG_START;
			configBlock += pchConfig;
			configBlock += QT_COMPILER_CONFIG_END;

			const char *pOutputFilename = g_pVPC->FormatTemp1( "%s/moc_%s.cpp", qtTargetSubdir.Get(), pFileBase );
			vpcBuffer.Printf( "$DynamicFile%s \"%s\" %s\n\n", pFileSuff, pOutputFilename, configBlock.Get() );
			// Record the relationship between the input/output files:
			g_pVPC->m_QtOutputFileMap[ pFilename ] = pOutputFilename;
		}
		else if ( IsCFileExtension( pExt ) )
		{
			CUtlString mocFilePath;
			mocFilePath.Format( "%s/%s.moc", qtTargetSubdir.Get(), pFileNoExt );
			CUtlString fakemocFilePath;
			fakemocFilePath.Format( "%s/%s.fakemoc", qtTargetSubdir.Get(), pFileNoExt );

			// Inject the fakemoc block
			vpcBuffer.Printf( QT_FAKEMOC_BLOCK, mocFilePath.Get(), fakemocFilePath.Get(), pFilename, pFilename, mocFilePath.Get() );

			// Resolve the paths to the moc and fakemoc files
			// Ensure the directory structure is there because MOC and Visual Studio will be unhappy if it's not
            CUtlStringBuilder *pStrBuf = g_pVPC->GetTempStringBuffer1();
			g_pVPC->ResolveMacrosInString( mocFilePath.Get(), pStrBuf );
			Sys_CreatePath( pStrBuf->Get() );
            
			g_pVPC->ResolveMacrosInString( fakemocFilePath.Get(), pStrBuf );
			Sys_CreatePath( pStrBuf->Get() );

			// Write an empty fakemoc so IncrediBuild in VS2010 doesn't keep trying to build projects that include this file
			if ( !Sys_Exists( pStrBuf->Get() ) ) // only write the file if it's not already there
			{
				FILE *fp = fopen( pStrBuf->Get(), "wt" );
				if ( fp )
				{
					fclose( fp );
				}
			}

			// Record the relationship between the input/output files (in this special case, they are the same;
			// the source CPP file is still compiled, but must explicitly #include the generated MOC file):
			g_pVPC->m_QtOutputFileMap[ pFilename ] = pFilename;
		}
		else if ( !V_stricmp_fast( pExt, "ui" ) )
		{
			vpcBuffer.Printf( "$DynamicFile \"%s/ui_%s.h\"\n", qtUISubdir.Get(), pFileBase );
		}
		else
		{
			g_pVPC->VPCSyntaxError( "Can only use $QtFile for cpp, header, or ui files. (%s)", pFilename );
			continue;
		}
	}

	vpcBuffer.Printf( "}\n" );

	// save parser
	bool bIgnoreRedundancyWarning = g_pVPC->IsIgnoreRedundancyWarning();
	g_pVPC->SetIgnoreRedundancyWarning( true );
	g_pVPC->GetScript().PushScript( "Internal List [Qt]", (char*)vpcBuffer.Base(), 1, false, false );

	const char *pToken = g_pVPC->GetScript().GetToken( true );
	if ( pToken && pToken[0] && !V_stricmp_fast( pToken, "$folder" ) )
	{
		VPC_Keyword_Folder( VPC_FOLDER_FLAGS_DYNAMIC );
	}

	// restore parser
	g_pVPC->GetScript().PopScript();
	g_pVPC->SetIgnoreRedundancyWarning( bIgnoreRedundancyWarning );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CProjectFile *VPC_Qt_GetGeneratedFile( CProjectFile *pInputFile, const char * /*pConfigName*/, CVCProjGenerator *pDataCollector )
{
	CUtlString &generatedFilename = g_pVPC->m_QtOutputFileMap[ pInputFile->m_Name.Get() ];
	generatedFilename.FixSlashes();
	CProjectFile *pGeneratedFile = NULL;
	pDataCollector->FindFile( generatedFilename.Get(), &pGeneratedFile );
	if ( !pGeneratedFile )
		g_pVPC->VPCWarning( "VPC_Qt_GetGeneratedFile: could not find generated file for '%s'", pInputFile->m_Name.Get() );
	return pGeneratedFile;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool IsQtSupportedForThisTargetPlatform( void )
{
	// TODO: Only implemented+tested on WIN32/WIN64 so far...
	//       [ uses CBaseProjectDataCollector, so may work on other platforms, but untested ]
	const char *pPlatform = g_pVPC->GetTargetPlatformName();
	return ( !V_stricmp_fast( pPlatform, "WIN32" ) || !V_stricmp_fast( pPlatform, "WIN64" ) );
}

bool CVPC::IsQtEnabled( void )
{
	if ( !m_bAllowQt )
		return false;	// Schema not enabled
	if ( IsQtSupportedForThisTargetPlatform() )
		return true;	// Feature enabled & supported!
	ExecuteOnce( VPCWarning( "Qt feature disabled, not supported for %s yet", g_pVPC->GetTargetPlatformName() ) )
	return false;		// Platform not supported
}

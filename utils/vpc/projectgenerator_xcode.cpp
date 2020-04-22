//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "vpc.h"
#include "projectgenerator_xcode.h"

#ifdef WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#define getcwd _getcwd
#endif

static const char *k_pchSource = "Source Files";
static const char *k_pchHeaders = "Header Files";
static const char *k_pchResources = "Resources";
static const char *k_pchVPCFiles = "VPC Files";
static const char *k_pchFrameworksAndLibs = "VPC Files";

void CProjectGenerator_XCode::GenerateXCodeProject( CBaseProjectDataCollector *pCollector, const char *pOutFilename, const char *pMakefileFilename )
{
	char sPbxProjFile[MAX_PATH];
	sprintf( sPbxProjFile, "%s.xcodeproj", pOutFilename );
	mkdir( sPbxProjFile, 0777 );

	g_pVPC->VPCStatus( true, "Saving XCode project for: '%s' File: '%s'", pCollector->GetProjectName().String(), sPbxProjFile );

	sprintf( sPbxProjFile, "%s.xcodeproj/project.pbxproj", pOutFilename );
	m_fp = fopen( sPbxProjFile, "wt" );
	
	m_nIndent = 0;
	m_pCollector = pCollector;
	m_pMakefileFilename = pMakefileFilename;

	WriteHeader();
	WriteFileReferences();
	WriteFiles();
	WriteProject( pMakefileFilename );
	//WriteBuildFiles();
	WriteBuildConfigurations();
	// Write the files list.

	WriteTrailer();

	fclose( m_fp );
	m_fp = NULL;
}


void CProjectGenerator_XCode::Write( const char *pMsg, ... )
{
	char sOut[8192];

	va_list marker;
	va_start( marker, pMsg );
	V_vsnprintf( sOut, sizeof( sOut ), pMsg, marker );
	va_end( marker );

	for ( int i=0; i < m_nIndent; i++ )
		fprintf( m_fp, "\t" );

	fprintf( m_fp, "%s", sOut );
}

void CProjectGenerator_XCode::WriteHeader()
{
	// preamble
	Write( "// !$*UTF8*$!\n{\n" );
	++m_nIndent;
	Write( "archiveVersion = 1;\n" );
	Write( "classes = {\n" );
    Write( "};\n" );
	Write( "objectVersion = 42;\n" );
	Write( "objects = {\n" );
}


void CProjectGenerator_XCode::WriteProject( const char *pMakefileFilename )
{
	++m_nIndent;
	Write( "%024X = {\n", m_pCollector );
	++m_nIndent;
	Write( "isa = PBXGroup;\n" );
	Write( "children = (\n" );
	++m_nIndent;
	Write( "%024X /* %s */,\n", k_pchSource, k_pchSource );
	Write( "%024X /* %s */,\n", k_pchHeaders, k_pchHeaders );
	Write( "%024X /* %s */,\n", k_pchResources, k_pchResources );
	Write( "%024X /* %s */,\n", k_pchVPCFiles, k_pchVPCFiles );
	--m_nIndent;
	Write( ");\n" );
	Write( "sourceTree = \"<group>\";\n" );
	--m_nIndent;
	Write( "};" );
	--m_nIndent;
	Write( "\n/* End PBXGroup section */\n" );

	WriteLegacyTargets( pMakefileFilename );

	Write( "\n/* Begin PBXProject section */\n" );
	Write( "%024X /* Project object */ = {\n", this );
	++m_nIndent;
	Write( "isa = PBXProject;\n" );
	Write( "buildConfigurationList = %024X /* Build configuration list for PBXProject \"%s\" */;\n", &(m_pCollector->m_BaseConfigData), m_pCollector->GetProjectName().String() ); 
	Write( "compatibilityVersion = \"Xcode 2.4\";\n" );
	Write( "hasScannedForEncodings = 0;\n" );
	Write( "mainGroup = %024X;\n", m_pCollector );
	Write( "projectDirPath = \"\";\n" );
	
	char sSrcRootRelative[MAX_PATH];
	g_pVPC->ResolveMacrosInString( "$SRCDIR", sSrcRootRelative, sizeof( sSrcRootRelative ) );
	V_FixSlashes( sSrcRootRelative, '/' );

	Write( "projectRoot = \"%s\";\n", sSrcRootRelative );
	Write( "targets = (\n" );
	++m_nIndent;
	Write( "%024X /* build with make */, \n", &CProjectGenerator_XCode::WriteLegacyTargets );
	--m_nIndent;
	Write( ");\n" );
	--m_nIndent;
	Write( "};" );
	Write( "\n/* End PBXProject section */\n" );
}

void CProjectGenerator_XCode::WriteBuildFiles()
{
	Write( "\n/* Begin PBXBuildFile section */\n" );
	for ( int i=m_pCollector->m_Files.First(); i != m_pCollector->m_Files.InvalidIndex(); i=m_pCollector->m_Files.Next( i ) )
	{
		const char *pFilename = m_pCollector->m_Files[i]->GetName();
		Write( "%024X /* %s */ = {isa = PBXBuildFile; fileRef = %024X /* %s */;};\n", 
			   m_pCollector->m_Files[i], 
			   pFilename, m_pCollector->m_Files[i]->GetName(),
			   pFilename, m_pCollector->m_Files[i]->GetName() );
	}
	Write( "\n/* End PBXBuildFile section */\n" );
}

void CProjectGenerator_XCode::WriteBuildConfigurations()
{
	++m_nIndent;
	CUtlVector< CUtlString > configNames;
	m_pCollector->GetAllConfigurationNames( configNames );

	Write( "\n/* Begin XCBuildConfiguration section */\n" );	
	FOR_EACH_VEC( configNames, iConfig )
	{
		Write( "%024X /* %s */ = {\n", &(configNames[iConfig]), configNames[iConfig].String() );
		++m_nIndent;
		Write( "isa = XCBuildConfiguration;\n" );
		Write( "buildSettings = {\n" );
		++m_nIndent;
		Write( "COPY_PHASE_STRIP = %s;\n", ( V_strstr( configNames[iConfig].String(), "release" ) ? "YES" : "NO" ) );
		--m_nIndent;
		Write( "};\n" );
		Write( "name = %s;\n", configNames[iConfig].String() );
		--m_nIndent;
		Write( "};\n" );
	}

	FOR_EACH_VEC( configNames, iConfig )
	{
		Write( "%024X /* %s */ = {\n", (byte*)&(configNames[iConfig])+1, configNames[iConfig].String() );
		++m_nIndent;
		Write( "isa = XCBuildConfiguration;\n" );
		Write( "buildSettings = {\n" );
		++m_nIndent;
		Write( "PRODUCT_NAME = \"build with make\";\n" );
		Write( "CFG = %s;\n", configNames[iConfig].String() );
		--m_nIndent;
		Write( "};\n" );
		Write( "name = %s;\n", configNames[iConfig].String() );
		--m_nIndent;
		Write( "};\n" );
	}

	Write( "\n/* End XCBuildConfiguration section */\n" );	

	Write( "\n/* Begin XCBuildConfigurationList section */\n" );	
	Write( "%024X /* Build configuration list for PBXProject \"%s\" */ = {\n", &(m_pCollector->m_BaseConfigData), m_pCollector->GetProjectName().String() );
	++m_nIndent;
	Write( "isa = XCConfigurationList;\n" );
	Write( "buildConfigurations = (\n" );
	++m_nIndent;
	FOR_EACH_VEC( configNames, iConfig )
	{
		Write( "%024X /* %s */,\n", &(configNames[iConfig]), configNames[iConfig].String() );
	}
	--m_nIndent;
	Write( ");\n" );
	Write( "defaultConfigurationIsVisible = 0;\n" );
	Write( "defaultConfigurationName = %s;\n", configNames[0].String() );
	--m_nIndent;
	Write( "};\n" );

	Write( "%024X /* Build configuration list for PBXLegacyTarget \"build with make\" */ = {\n", (byte*)&(m_pCollector->m_BaseConfigData)+1 );
	++m_nIndent;
	Write( "isa = XCConfigurationList;\n" );
	Write( "buildConfigurations = (\n" );
	++m_nIndent;
	FOR_EACH_VEC( configNames, iConfig )
	{
		Write( "%024X /* %s */,\n", (byte*)&(configNames[iConfig])+1, configNames[iConfig].String() );
	}
	--m_nIndent;
	Write( ");\n" );
	Write( "defaultConfigurationIsVisible = 0;\n" );
	Write( "defaultConfigurationName = %s;\n", configNames[0].String() );
	--m_nIndent;
	Write( "};" );

	Write( "\n/* End XCBuildConfigurationList section */\n" );	

	--m_nIndent;

}

void CProjectGenerator_XCode::WriteLegacyTargets( const char *pchMakefileName )
{
	Write( "\n/* Begin PBXLegacyTarget section */\n" );	
	++m_nIndent;
	Write( "%024X /* build with make */ = {\n", &CProjectGenerator_XCode::WriteLegacyTargets );
	++m_nIndent;
	Write( "isa = PBXLegacyTarget;\n");
	Write( "buildArgumentsString = \"-f %s $(ACTION) MAKE_VERBOSE=1\";\n", pchMakefileName );
	Write( "buildConfigurationList = %024X /* Build configuration list for PBXLegacyTarget \"build with make\" */;\n",  (byte*)&(m_pCollector->m_BaseConfigData)+1 );
	Write( "buildPhases = (\n" );
	Write( ");\n" );
	Write( "buildToolPath = /usr/bin/make;\n" );
	char rgchCurDir[MAX_PATH];
	getcwd( rgchCurDir, MAX_PATH );
	Write( "buildWorkingDirectory = %s;\n", rgchCurDir );
	Write( "dependencies = (\n" );
	Write( ");\n");
	Write( "name = \"build with make\";\n" );
	Write( "passBuildSettingsInEnvironment = 1;\n" );
	Write( "productName = \"build with make\";\n" );
	--m_nIndent;
	Write( "};\n" );
	--m_nIndent;
	Write( "\n/* End PBXLegacyTarget section */\n" );	
	
}

void CProjectGenerator_XCode::WriteFileReferences()
{
	Write( "\n/* Begin PBXFileReference section */\n" );
	++m_nIndent;
	for ( int i=m_pCollector->m_Files.First(); i != m_pCollector->m_Files.InvalidIndex(); i=m_pCollector->m_Files.Next( i ) )
	{
		const char *pchExtension = V_GetFileExtension( m_pCollector->m_Files[i]->GetName() );
		char rgchFileType[MAX_PATH];
		if ( pchExtension && ( !Q_stricmp( pchExtension, "cpp" ) || !Q_stricmp( pchExtension, "h" ) || !Q_stricmp( pchExtension, "c" ) || !Q_stricmp( pchExtension, "cc" ) ) )
			sprintf( rgchFileType, "sourcecode.cpp.%s", pchExtension );
		else
			sprintf( rgchFileType, "text.plain" );

		char rgchFilePath[MAX_PATH];
		Q_strncpy( rgchFilePath, m_pCollector->m_Files[i]->GetName(), sizeof( rgchFilePath ) );
		Q_RemoveDotSlashes( rgchFilePath );

		Write( "%024X /* %s */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = %s; path = \"%s\"; sourceTree = \"<group>\"; };\n",
			   m_pCollector->m_Files[i], 
			   rgchFilePath,
			   rgchFileType,
			   rgchFilePath
			);
	}   
	--m_nIndent;
	Write( "\n/* End PBXFileReference section */\n" );
}

void CProjectGenerator_XCode::WriteTrailer()
{
	Write( "};\n" );
	Write( "rootObject = %024X /* project object */;\n", this );
	--m_nIndent;
	Write( "}\n" );
}

void CProjectGenerator_XCode::WriteConfig( CSpecificConfig *pConfig )
{
}

void CProjectGenerator_XCode::WriteTarget_Build( CSpecificConfig *pConfig )
{
}

void CProjectGenerator_XCode::WriteTarget_Compile( CSpecificConfig *pConfig )
{
}

void CProjectGenerator_XCode::WriteTarget_Rebuild( CSpecificConfig *pConfig )
{
}

void CProjectGenerator_XCode::WriteTarget_Link( CSpecificConfig *pConfig )
{
}

void CProjectGenerator_XCode::WriteTarget_Debug( CSpecificConfig *pConfig )
{
}

void CProjectGenerator_XCode::WriteIncludes( CSpecificConfig *pConfig )
{
}

void CProjectGenerator_XCode::WriteFilesFolder( const char *pFolderName, const char *pExtensions )
{
	CUtlVector<char*> extensions;
	V_SplitString( pExtensions, ";", extensions );

	Write( "%024X /* %s */ = {\n", pFolderName, pFolderName );
	++m_nIndent;
	Write( "isa = PBXGroup;\n" );
	Write( "children = (\n" );
	++m_nIndent;
	for ( int i=m_pCollector->m_Files.First(); i != m_pCollector->m_Files.InvalidIndex(); i=m_pCollector->m_Files.Next( i ) )
	{
		const char *pFilename = m_pCollector->m_Files[i]->GetName();

		// Make sure this file's extension is one of the extensions they're asking for.
		bool bValidExt = false;
		const char *pFileExtension = V_GetFileExtension( pFilename );
		if ( pFileExtension )
		{
			for ( int iExt=0; iExt < extensions.Count(); iExt++ )
			{
				const char *pTestExt = extensions[iExt];

				if ( pTestExt[0] == '*' && pTestExt[1] == '.' && V_stricmp( pTestExt+2, pFileExtension ) == 0 )
				{
					bValidExt = true;
					break;
				}
			}
		}

		if ( bValidExt )
		{
			char sFixedSlashes[MAX_PATH];
			V_strncpy( sFixedSlashes, pFilename, sizeof( sFixedSlashes ) );
			Write( "%024X /* %s */,\n", m_pCollector->m_Files[i], sFixedSlashes );
		}
	}

	--m_nIndent;
	Write( ");\n" );
	Write( "name = \"%s\";\n", pFolderName );
	Write( "sourceTree = \"<group>\";\n" );
	--m_nIndent;
	Write( "};\n" );
}

void CProjectGenerator_XCode::WriteFiles()
{
	Write( "\n/* Begin PBXGroup section */\n" );
	++m_nIndent;
	WriteFilesFolder( k_pchSource, "*.c;*.C;*.cc;*.cpp;*.cp;*.cxx;*.c++;*.prg;*.pas;*.dpr;*.asm;*.s;*.bas;*.java;*.cs;*.sc;*.e;*.cob;*.html;*.rc;*.tcl;*.py;*.pl;*.m;*.mm" );
	WriteFilesFolder( k_pchHeaders, "*.h;*.H;*.hh;*.hpp;*.hxx;*.inc;*.sh;*.cpy;*.if" );
	WriteFilesFolder( k_pchResources, "*.plist;*.strings;*.xib" );
	WriteFilesFolder( k_pchVPCFiles, "*.vpc" );
	--m_nIndent;
}

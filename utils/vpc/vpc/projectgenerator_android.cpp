//================= Copyright Valve Corporation, All rights reserved. =================//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"
#include "projectgenerator_android.h"

#undef PROPERTYNAME
#define PROPERTYNAME( X, Y ) { X##_##Y, #X, #Y },
static PropertyName_t s_AndroidPropertyNames[] =
{
	#include "projectgenerator_android.inc"
	{ -1, NULL, NULL }
};

//all of the defined properties are relevant
#undef PROPERTYNAME
#define PROPERTYNAME( X, Y ) "$" #Y,
static const char *sg_RelevantPropertyStrings[] =
{
	#include "projectgenerator_android.inc"
};

static CRelevantPropertyNames s_RelevantPropertyNames = { sg_RelevantPropertyStrings, ARRAYSIZE( sg_RelevantPropertyStrings ) };

IBaseProjectGenerator* GetAndroidProjectGenerator()
{
	return new CProjectGenerator_Android();
}

CProjectGenerator_Android::CProjectGenerator_Android()
	: CBaseProjectDataCollector( &s_RelevantPropertyNames )
{
	m_BaseConfigData.GetOrCreateConfig( "Debug", nullptr );
	m_BaseConfigData.GetOrCreateConfig( "Release", nullptr );

	m_GeneratorDefinition.LoadDefinition( "android.def", s_AndroidPropertyNames );
}

void CProjectGenerator_Android::EndProject( bool bSaveData )
{
	BaseClass::EndProject( bSaveData );

	if ( !bSaveData || g_pVPC->m_bIsDependencyPass )
		return;

	if ( m_BaseConfigData.m_Configurations.Count() == 0 )
		return;

	CSpecificConfig *pBaseConfig = m_BaseConfigData.m_Configurations[m_BaseConfigData.m_Configurations.Find( "Release" )];
	CUtlVectorFixedGrowableCompat<CSpecificConfig *,2> generalConfigurations;

	for ( int nConfigIter = m_BaseConfigData.m_Configurations.First(); m_BaseConfigData.m_Configurations.IsValidIndex( nConfigIter ); nConfigIter = m_BaseConfigData.m_Configurations.Next( nConfigIter ) )
	{
		generalConfigurations.AddToTail( m_BaseConfigData.m_Configurations.Element( nConfigIter ) );
	}

	if ( !WriteAndroidProj( pBaseConfig, generalConfigurations ) )
	{
		g_pVPC->VPCError( "Unable to write \"%s\"", GetOutputFileName() );
		UNREACHABLE();
	}

	if ( !WriteBuildXML( pBaseConfig, generalConfigurations ) )
	{
		g_pVPC->VPCError( "Unable to write \"%s\" build.xml", GetProjectName() );
		UNREACHABLE();
	}

	if ( !WriteAndroidManifestXML( pBaseConfig, generalConfigurations ) )
	{
		g_pVPC->VPCError( "Unable to write \"%s\" androidmanifest.xml", GetProjectName() );
		UNREACHABLE();
	}

	if ( !WriteProjectProperties( pBaseConfig, generalConfigurations ) )
	{
		g_pVPC->VPCError( "Unable to write \"%s\" project.properties", GetProjectName() );
		UNREACHABLE();
	}

	//success
}

const char *CProjectGenerator_Android::GetTargetAndroidPlatformName( const char *szVPCPlatformName )
{
	if ( !V_stricmp_fast( szVPCPlatformName, "androidx8632" ) )
	{
		return "x86";
	}
	else if ( !V_stricmp_fast( szVPCPlatformName, "androidx8664" ) )
	{
		return "x64";
	}
	else if ( !V_stricmp_fast( szVPCPlatformName, "androidarm32" ) )
	{
		return "ARM";
	}
	else if ( !V_stricmp_fast( szVPCPlatformName, "androidarm64" ) )
	{
		return "ARM64";
	}
	else
	{
		g_pVPC->VPCError( "AndroidProj does not have a Visual Studio platform mapping for VPC platform \"%s\"", szVPCPlatformName );
		UNREACHABLE();
		return nullptr;
	}
}

bool CProjectGenerator_Android::WriteAndroidProj( CSpecificConfig *pBaseConfig, const CUtlVector<CSpecificConfig *> &generalConfigurations )
{
	CXMLWriter xmlWriter;

	if ( !xmlWriter.Open( GetOutputFileName(), true, g_pVPC->IsForceGenerate() ) )
		return false;

	const char *szVPCPlatformName = g_pVPC->GetTargetPlatformName();
	const char *szVisualStudioPlatformName = GetTargetAndroidPlatformName( szVPCPlatformName );
	

	xmlWriter.PushNode( "Project" );
	{
		xmlWriter.AddNodeProperty( "DefaultTargets", "Build" );
		xmlWriter.AddNodeProperty( "ToolsVersion", "14.0" );
		xmlWriter.AddNodeProperty( "xmlns", "http://schemas.microsoft.com/developer/msbuild/2003" );

		xmlWriter.PushNode( "ItemGroup" );
		{
			xmlWriter.AddNodeProperty( "Label", "ProjectConfigurations" );

			for ( int i = 0; i < generalConfigurations.Count(); i++ )
			{
				xmlWriter.PushNode( "ProjectConfiguration" );
				{
					xmlWriter.AddNodeProperty( "Include", CFmtStr( "%s|%s", generalConfigurations[i]->GetConfigName(), szVisualStudioPlatformName ) );

					xmlWriter.WriteLineNode( "Configuration", "", generalConfigurations[i]->GetConfigName() );
					xmlWriter.WriteLineNode( "Platform", "", CFmtStr( "%s", szVisualStudioPlatformName ) );
				}
				xmlWriter.PopNode();
			}
			
		}
		xmlWriter.PopNode();

		xmlWriter.PushNode( "PropertyGroup" );
		{
			xmlWriter.AddNodeProperty( "Label", "Globals" );

			xmlWriter.WriteLineNode( "RootNamespace", "", GetProjectName() );
			xmlWriter.WriteLineNode( "MinimumVisualStudioVersion", "", "14.0" ); //VS2015
			xmlWriter.WriteLineNode( "ProjectVersion", "", "1.0" );
			xmlWriter.WriteLineNode( "ProjectGuid", "", CFmtStr( "{%s}", GetGUIDString() ) );
			xmlWriter.WriteLineNode( "_PackagingProjectWithoutNativeComponent", "", "true" ); //failing to set this will cause us to fail to deploy (blank ABI name) if we don't reference a native component directly. And I'm not sure what that reference actually buys us
			//<LaunchActivity Condition="'$(LaunchActivity)' == ''">com.example.hellojni.HelloJni</LaunchActivity>
			xmlWriter.WriteLineNode( "JavaSourceRoots", "", "src" );
		}
		xmlWriter.PopNode();

		xmlWriter.PushNode( "Import" );
		{
			xmlWriter.AddNodeProperty( "Project", "$(AndroidTargetsPath)\\Android.Default.props" );
		}
		xmlWriter.PopNode();
				
		//general config part 1, not sure why, but this group has "Label=\"Configuration\"" and part 2 doesn't
		for ( int i = 0; i < generalConfigurations.Count(); i++ )
		{
			xmlWriter.PushNode( "PropertyGroup", XMLOIE_OMIT_ON_EMPTY_CONTENTS );
			{
				xmlWriter.AddNodeProperty( "Condition", CFmtStr( "'$(Configuration)|$(Platform)'=='%s|%s'", generalConfigurations[i]->GetConfigName(), szVisualStudioPlatformName ) );
				xmlWriter.AddNodeProperty( "Label", "Configuration" );

				xmlWriter.WriteLineNode( "UseDebugLibraries", nullptr, (V_stricmp_fast( generalConfigurations[i]->GetConfigName(), "debug" ) == 0) ? "true" : "false", XMLOIE_OMIT_ON_EMPTY_EVERYTHING ); //HACKY mapping of debug config to debug libraries
				xmlWriter.WriteLineNode( "TargetName", nullptr, generalConfigurations[i]->GetOption( "$TargetName" ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
				xmlWriter.WriteLineNode( "ConfigurationType", nullptr, generalConfigurations[i]->GetOption( "$ConfigurationType" ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
				xmlWriter.WriteLineNode( "AndroidAPILevel", nullptr, generalConfigurations[i]->GetOption( "$AndroidAPILevel", "android-21" ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
			}
			xmlWriter.PopNode();
		}

		xmlWriter.PushNode( "Import" );
		{
			xmlWriter.AddNodeProperty( "Project", "$(AndroidTargetsPath)\\Android.props" );
		}
		xmlWriter.PopNode();

		xmlWriter.PushNode( "ImportGroup" );
		{
			xmlWriter.AddNodeProperty( "Label", "ExtensionSettings" );
		}
		xmlWriter.PopNode();

		xmlWriter.PushNode( "PropertyGroup" );
		{
			xmlWriter.AddNodeProperty( "Label", "UserMacros" );
		}
		xmlWriter.PopNode();

		//general config part 2
		for ( int i = 0; i < generalConfigurations.Count(); i++ )
		{
			xmlWriter.PushNode( "PropertyGroup", XMLOIE_OMIT_ON_EMPTY_CONTENTS );
			{
				xmlWriter.AddNodeProperty( "Condition", CFmtStr( "'$(Configuration)|$(Platform)'=='%s|%s'", generalConfigurations[i]->GetConfigName(), szVisualStudioPlatformName ).Get() );

				xmlWriter.WriteLineNode( "OutDir", nullptr, generalConfigurations[i]->GetOption( "$OutputDirectory", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
				xmlWriter.WriteLineNode( "IntDir", nullptr, generalConfigurations[i]->GetOption( "$IntermediateDirectory", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
				//xmlWriter.WriteLineNode( "TargetExt", nullptr, "vpctest_targetextension", XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
			}
			xmlWriter.PopNode();
		}

		//debugger properties
		for ( int i = 0; i < generalConfigurations.Count(); i++ )
		{
			xmlWriter.PushNode( "PropertyGroup", XMLOIE_OMIT_ON_EMPTY_CONTENTS );
			{
				xmlWriter.AddNodeProperty( "Condition", CFmtStr( "'$(Configuration)|$(Platform)'=='%s|%s'", generalConfigurations[i]->GetConfigName(), szVisualStudioPlatformName ).Get() );

				//xmlWriter.WriteLineNode( "AndroidDeviceID", nullptr, "vpctest_debugtarget", XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
				xmlWriter.WriteLineNode( "PackagePath", nullptr, generalConfigurations[i]->GetOption( "$PackagePath", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
				xmlWriter.WriteLineNode( "LaunchActivity", nullptr, CFmtStr( "%s.%s", generalConfigurations[i]->GetOption( "$PackageName", nullptr ), generalConfigurations[i]->GetOption( "$LauncherActivity", "LaunchActivity" ) ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
				xmlWriter.WriteLineNode( "AdditionalSymbolSearchPaths", nullptr, generalConfigurations[i]->GetOption( "$AdditionalSymbolSearchPaths", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
				xmlWriter.WriteLineNode( "AdditionalSourceSearchPaths", nullptr, generalConfigurations[i]->GetOption( "$AdditionalSourceSearchPaths", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
				xmlWriter.WriteLineNode( "DebuggerFlavor", nullptr, generalConfigurations[i]->GetOption( "$DebuggerFlavor", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
			}
			xmlWriter.PopNode();
		}

		//ant properties
		for ( int i = 0; i < generalConfigurations.Count(); i++ )
		{
			xmlWriter.PushNode( "ItemDefinitionGroup", XMLOIE_OMIT_ON_EMPTY_CONTENTS );
			{
				xmlWriter.AddNodeProperty( "Condition", CFmtStr( "'$(Configuration)|$(Platform)'=='%s|%s'", generalConfigurations[i]->GetConfigName(), szVisualStudioPlatformName ).Get() );

				xmlWriter.PushNode( "AntPackage", XMLOIE_OMIT_ON_EMPTY_CONTENTS );
				{
					xmlWriter.WriteLineNode( "AndroidAppLibName", nullptr, generalConfigurations[i]->GetOption( "$AndroidAppLibName", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
					xmlWriter.WriteLineNode( "ApplicationName", nullptr, generalConfigurations[i]->GetOption( "$ApplicationName", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
					xmlWriter.WriteLineNode( "WorkingDirectory", nullptr, generalConfigurations[i]->GetOption( "$WorkingDirectory", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
					//xmlWriter.WriteLineNode( "AntTarget", nullptr, generalConfigurations[i]->GetOption( "", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING ); //vpctest_antbuildtarget
					xmlWriter.WriteLineNode( "AdditionalOptions", nullptr, generalConfigurations[i]->GetOption( "$AdditionalOptions", nullptr ), XMLOIE_OMIT_ON_EMPTY_EVERYTHING );
				}
				xmlWriter.PopNode();
			}
			xmlWriter.PopNode();
		}

		//files
		xmlWriter.PushNode( "ItemGroup", XMLOIE_OMIT_ON_EMPTY_CONTENTS );
		{
			/*xmlWriter.PushNode( "Content" );
			{
				xmlWriter.AddNodeProperty( "Include", "res\\values\\strings.xml" );

				xmlWriter.WriteLineNode( "SubType", "", "Designer" );
			}
			xmlWriter.PopNode();*/

			xmlWriter.PushNode( "AntBuildXml" );
			{
				xmlWriter.AddNodeProperty( "Include", "build.xml" );

				xmlWriter.WriteLineNode( "SubType", "", "Designer" );
			}
			xmlWriter.PopNode();

			xmlWriter.PushNode( "AndroidManifest" );
			{
				xmlWriter.AddNodeProperty( "Include", "AndroidManifest.xml" );

				xmlWriter.WriteLineNode( "SubType", "", "Designer" );
			}
			xmlWriter.PopNode();

			xmlWriter.PushNode( "AntProjectPropertiesFile" );
			{
				xmlWriter.AddNodeProperty( "Include", "project.properties" );
			}
			xmlWriter.PopNode();

			for ( unsigned int i = 0; i < m_Files.Count(); ++i )
			{
				CFileConfig *pCurFile = m_Files[i];
				if ( (pCurFile->m_iFlags & (VPC_FILE_FLAGS_STATIC_LIB | VPC_FILE_FLAGS_IMPORT_LIB)) != 0 )
				{
					continue;
					//g_pVPC->VPCError( "Android projects aren't set up to do anything with static or import libs \"%s\"", pCurFile->GetName() );
					//UNREACHABLE();
				}

				if ( (pCurFile->m_iFlags & (VPC_FILE_FLAGS_QT | VPC_FILE_FLAGS_SCHEMA | VPC_FILE_FLAGS_SCHEMA_INCLUDE)) != 0 )
				{
					g_pVPC->VPCError( "Android projects aren't set up to do anything with Schema or Qt files \"%s\"", pCurFile->GetName() );
					UNREACHABLE();
				}

				if ( (pCurFile->m_iFlags & (VPC_FILE_FLAGS_SHARED_LIB)) != 0 )
				{
					//reference the shared lib so it's listed in the solution?
				}
				
				//regular file
				const char *pExtension = V_GetFileExtension( pCurFile->GetName() );
				if ( !pExtension )
				{
					g_pVPC->VPCWarning( "Don't know what to do with file \"%s\"", pCurFile->GetName() );
					continue;
				}

				const char *szNodeTypeForFile = nullptr;

				if ( IsLibraryFile( pCurFile->GetName() ) || (V_stricmp( pExtension, "vpc" ) == 0) )
				{
					szNodeTypeForFile = "None";
				}
				else if ( V_stricmp( pExtension, "java" ) == 0 )
				{
					szNodeTypeForFile = "JavaCompile";
				}
				else
				{
					szNodeTypeForFile = "Content";
				}

				Assert( szNodeTypeForFile != nullptr );

				xmlWriter.PushNode( szNodeTypeForFile );
				{
					xmlWriter.AddNodeProperty( "Include", xmlWriter.FixupXMLString( pCurFile->GetName() ) );
				}
				xmlWriter.PopNode();
			}
		}
		xmlWriter.PopNode();

		xmlWriter.PushNode( "Import" );
		{
			xmlWriter.AddNodeProperty( "Project", "$(AndroidTargetsPath)\\Android.targets" );
		}
		xmlWriter.PopNode();

		xmlWriter.PushNode( "ImportGroup" );
		{
			xmlWriter.AddNodeProperty( "Label", "ExtensionTargets" );
		}
		xmlWriter.PopNode();
	}
	xmlWriter.PopNode();

	xmlWriter.Close();

	return true;
}

bool CProjectGenerator_Android::WriteBuildXML( CSpecificConfig *pBaseConfig, const CUtlVector<CSpecificConfig *> &generalConfigurations )
{
	CXMLWriter xmlWriter;

	if ( !xmlWriter.Open( "build.xml", true, g_pVPC->IsForceGenerate() ) )
		return false;

	const char *szVPCPlatformName = g_pVPC->GetTargetPlatformName();
	const char *szVisualStudioPlatformName = GetTargetAndroidPlatformName( szVPCPlatformName );

	xmlWriter.PushNode( "project" );
	{
		xmlWriter.AddNodeProperty( "name", pBaseConfig->GetOption( "$ApplicationName", GetProjectName() ) );
		xmlWriter.AddNodeProperty( "default", "help" );

		xmlWriter.PushNode( "property", "file=\"ant.properties\"" );
		xmlWriter.PopNode();

		xmlWriter.PushNode( "property", "environment=\"env\"" );
		xmlWriter.PopNode();

		xmlWriter.PushNode( "condition" );
		{
			xmlWriter.AddNodeProperty( "property", "sdk.dir" );
			xmlWriter.AddNodeProperty( "value", "${env.ANDROID_HOME}" );

			xmlWriter.PushNode( "isset", "property=\"env.ANDROID_HOME\"" );
			xmlWriter.PopNode();
		}
		xmlWriter.PopNode();

		xmlWriter.PushNode( "loadproperties", "srcfile=\"project.properties\"" );
		xmlWriter.PopNode();

		xmlWriter.PushNode( "fail" );
		{
			xmlWriter.AddNodeProperty( "unless=\"sdk.dir\"" );
			xmlWriter.AddNodeProperty( "message=\"sdk.dir is missing. Make sure ANDROID_HOME environment variable is correctly set.\"" );
		}
		xmlWriter.PopNode();

		xmlWriter.PushNode( "target", "name=\"-pre-build\"" );
		{
			//copy shared libs to the packaging dir
			for ( unsigned int i = 0; i < m_Files.Count(); ++i )
			{
				CFileConfig *pCurFile = m_Files[i];
				if ( (pCurFile->m_iFlags & VPC_FILE_FLAGS_SHARED_LIB) == 0 )
					continue;

				//switch to absolute path because the build script can move from the VPC write location before executing. Might want to make this a more robust relative path in the future for smoother cross compilation
				char szAbsolutePath[MAX_PATH];
				V_MakeAbsolutePath( szAbsolutePath, ARRAYSIZE( szAbsolutePath ), pCurFile->GetName(), nullptr, false );

				xmlWriter.PushNode( "copy" );
				{
					xmlWriter.AddNodeProperty( "file", szAbsolutePath ); //This will probably need to account for the script being moved
					xmlWriter.AddNodeProperty( "tofile", CFmtStr( "libs/%s/%s", szVisualStudioPlatformName, V_UnqualifiedFileName( szAbsolutePath ) ) );
				}
				xmlWriter.PopNode();
			}

			//copy shared libs from the packaging directory to the ".gdb" directory so the visual studio debugger can find them
			xmlWriter.PushNode( "copy", "todir=\"../.gdb\"" );
			{
				xmlWriter.PushNode( "fileset" );
				{
					xmlWriter.AddNodeProperty( "dir", CFmtStr( "libs/%s", szVisualStudioPlatformName ) );

					xmlWriter.PushNode( "include", "name=\"*.so\"" );
					xmlWriter.PopNode();
				}
				xmlWriter.PopNode();
			}
			xmlWriter.PopNode();
		}
		xmlWriter.PopNode();

		xmlWriter.PushNode( "import", "file=\"${sdk.dir}/tools/ant/build.xml\"" );
		xmlWriter.PopNode();
	}
	xmlWriter.PopNode();

	xmlWriter.Close();

	return true;
}

bool CProjectGenerator_Android::WriteAndroidManifestXML( CSpecificConfig *pBaseConfig, const CUtlVector<CSpecificConfig *> &generalConfigurations )
{
	CXMLWriter xmlWriter;

	if ( !xmlWriter.Open( "AndroidManifest.xml", true, g_pVPC->IsForceGenerate() ) )
		return false;

	const char *szPackageName = pBaseConfig->GetOption( "$PackageName" );

	xmlWriter.PushNode( "manifest" );
	{
		xmlWriter.AddNodeProperty( "xmlns:android", "http://schemas.android.com/apk/res/android" );
		xmlWriter.AddNodeProperty( "package", szPackageName );
		xmlWriter.AddNodeProperty( "android:versionCode", "1" );
		xmlWriter.AddNodeProperty( "android:versionName", "1.0" );

		xmlWriter.PushNode( "uses-sdk" );
		{
			const char *pMinSDK = pBaseConfig->GetOption( "$MinAndroidAPILevel", "android-19" );
			if ( V_strnicmp( pMinSDK, "android-", 8 ) != 0 )
			{
				g_pVPC->VPCError( "Expecting $MinAndroidAPILevel to being with \"android-\", was actually \"%s\"", pMinSDK );
				UNREACHABLE();
			}

			const char *pTargetSDK = pBaseConfig->GetOption( "$AndroidAPILevel", "android-19" );
			if ( V_strnicmp( pTargetSDK, "android-", 8 ) != 0 )
			{
				g_pVPC->VPCError( "Expecting $AndroidAPILevel to being with \"android-\", was actually \"%s\"", pMinSDK );
				UNREACHABLE();
			}

			xmlWriter.AddNodeProperty( "android:minSdkVersion", pMinSDK + 8 );
			xmlWriter.AddNodeProperty( "android:targetSdkVersion", pTargetSDK + 8 );
		}
		xmlWriter.PopNode();

		xmlWriter.PushNode( "application" );
		{
			xmlWriter.AddNodeProperty( "android:label", pBaseConfig->GetOption( "$ApplicationName", GetProjectName() ) );
			xmlWriter.AddNodeProperty( "android:hasCode", "true" );
			xmlWriter.AddNodeProperty( "android:debuggable", "true" );
			xmlWriter.AddNodeProperty( "android:name", ".application" );

			xmlWriter.PushNode( "activity" );
			{
				xmlWriter.AddNodeProperty( "android:name", CFmtStr( ".%s", pBaseConfig->GetOption( "$LauncherActivity", "LaunchActivity" ) ) );
				xmlWriter.AddNodeProperty( "android:label", pBaseConfig->GetOption( "$ApplicationName", GetProjectName() ) );

				xmlWriter.PushNode( "intent-filter" );
				{
					xmlWriter.PushNode( "action", "android:name=\"android.intent.action.MAIN\"" );
					xmlWriter.PopNode();

					xmlWriter.PushNode( "category", "android:name=\"android.intent.category.LAUNCHER\"" );
					xmlWriter.PopNode();
				}
				xmlWriter.PopNode();
			}
			xmlWriter.PopNode();
		}
		xmlWriter.PopNode();

		CSplitString splitPermissions( pBaseConfig->GetOption( "$Permissions" ), ";" );
		for ( int i = 0; i < splitPermissions.Count(); ++i )
		{
			const char *szPermission = splitPermissions[i];
			if ( szPermission[0] )
			{
				xmlWriter.PushNode( "uses-permission" );
				{
					xmlWriter.AddNodeProperty( "android:name", szPermission );
				}
				xmlWriter.PopNode();
			}
		}		
	}
	xmlWriter.PopNode();

	xmlWriter.Close();

	return true;
}

bool CProjectGenerator_Android::WriteProjectProperties( CSpecificConfig *pBaseConfig, const CUtlVector<CSpecificConfig *> &generalConfigurations )
{
	CUtlBuffer projectProperties;
	projectProperties.SetBufferType( true, false );

	projectProperties.PutString( "# Project target.\n" );
	projectProperties.Printf( "target=%s\n", pBaseConfig->GetOption( "$AndroidAPILevel", "android-21" ) );

	Sys_WriteFileIfChanged( "project.properties", projectProperties, true );

	return true;
}

void CProjectGenerator_Android::EnumerateSupportedVPCTargetPlatforms( CUtlVector<CUtlString> &output )
{
	output.AddToTail( g_pVPC->GetTargetPlatformName() );
}

bool CProjectGenerator_Android::BuildsForTargetPlatform( const char *szVPCTargetPlatform )
{
	return VPC_IsPlatformAndroid( szVPCTargetPlatform );
}

bool CProjectGenerator_Android::DeploysForVPCTargetPlatform( const char *szVPCTargetPlatform )
{
	return VPC_IsPlatformAndroid( szVPCTargetPlatform );
}

CUtlString CProjectGenerator_Android::GetSolutionPlatformAlias( const char *szVPCTargetPlatform, IBaseSolutionGenerator *pSolutionGenerator )
{
	switch ( pSolutionGenerator->GetSolutionType() )
	{
	case ST_VISUALSTUDIO:
		return GetTargetAndroidPlatformName( szVPCTargetPlatform );

	case ST_MAKEFILE:
	case ST_XCODE:
		return szVPCTargetPlatform;

	NO_DEFAULT;
	};
}


//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// Purpose: 
//
//==================================================================================================

#include "vpc.h"
#include "dependencies.h"
#include "tier1/checksum_md5.h"

struct SolutionFolderData_t
{
	CUtlString strAbsPath;
	CUtlString strFolderName;
	CUtlString strGUID;
	CUtlString strParentGUID;
	CUtlString strSearchPattern;
	CUtlVector< CUtlString > files;
};

class CSolutionGenerator_Win32 : public IBaseSolutionGenerator
{
public:
	void GetVCPROJSolutionGUID( const char *szProjectExtension, char (&szSolutionGUID)[256] )
	{
#if defined( PLATFORM_WINDOWS )
		HKEY hKey;
		int firstVer = 8;
		const int lastVer = 14; // Handle up to VS 14, AKA VS 2015

		if ( g_pVPC->Is2010() )
		{
			firstVer = 10;
		}

		// Handle both VisualStudio and VCExpress (used by some SourceSDK customers)
		const char* productName[] =
		{
			"VisualStudio",
			"VCExpress",
		};

		for ( int nLocationIter = 0; nLocationIter < 2; ++nLocationIter ) //for some reason I don't care to investigate there are more keys available at HKEY_CURRENT_USER\\Software\\Microsoft\\%s\\%d.0_Config\\Projects (androidproj support)
		{
			for ( int vsVer = firstVer; vsVer <= lastVer; ++vsVer )
			{
				for ( int productNumber = 0; productNumber < ARRAYSIZE(productName); ++productNumber )
				{
					LONG ret;
					if ( nLocationIter == 0 )
					{
#if defined( _WIN64 )
						#define WOW6432NODESTR	"Software\\Wow6432Node"
#else
						#define WOW6432NODESTR	"Software"
#endif
						ret = RegOpenKeyEx( HKEY_LOCAL_MACHINE, CFmtStrN<1024>( WOW6432NODESTR "\\Microsoft\\%s\\%d.0\\Projects", productName[ productNumber ], vsVer ).Get(), 0, KEY_READ, &hKey );
					}
					else if ( nLocationIter == 1 )
					{
						ret = RegOpenKeyEx( HKEY_CURRENT_USER, CFmtStrN<1024>( "Software\\Microsoft\\%s\\%d.0_Config\\Projects", productName[ productNumber ], vsVer ).Get(), 0, KEY_READ, &hKey );
					}
					else
					{
						UNREACHABLE();
					}

					if ( ret != ERROR_SUCCESS )
						continue;

					int nEnumKey = 0;
					do
					{
						char szKeyName[MAX_FIXED_PATH];	
						DWORD dwKeyNameSize = sizeof( szKeyName );
						ret = RegEnumKeyEx( hKey, nEnumKey++, szKeyName, &dwKeyNameSize, NULL, NULL, NULL, NULL );
						if ( ret == ERROR_NO_MORE_ITEMS )
							break;

						HKEY hSubKey;
						ret = RegOpenKeyEx( hKey, szKeyName, 0, KEY_READ, &hSubKey );
						if ( ret == ERROR_SUCCESS )
						{						
							DWORD dwType;
							char ext[MAX_BASE_FILENAME];
							DWORD dwExtLen = sizeof( ext );
							ret = RegQueryValueEx( hSubKey, "DefaultProjectExtension", NULL, &dwType, (BYTE*)ext, &dwExtLen );
							RegCloseKey( hSubKey );

							// VS 2012 and beyond has the DefaultProjectExtension as vcxproj instead of vcproj
							if ( (ret == ERROR_SUCCESS) && (dwType == REG_SZ) && V_stricmp_fast( ext, szProjectExtension ) == 0 )
							{
								V_strncpy( szSolutionGUID, szKeyName, ARRAYSIZE(szSolutionGUID) );
								RegCloseKey( hKey );
								return;
							}
						}
					}
					while( true );

					RegCloseKey( hKey );
				}
			}
		}
#endif
		g_pVPC->VPCError( "Unable to find RegKey for .%s files in solutions.", szProjectExtension );
	}

	const char *UpdateProjectFilename( const char *pProjectFilename, CUtlPathStringHolder *pUpdateBuffer )
	{
		const char *pExt = V_GetFileExtension( pProjectFilename );

		// We may be generating a makefile wrapper solution,
		// in which case we need to look at the wrapper
		// project instead of the base project.
		const char *pProjectExt = "vcproj";
		if ( g_pVPC->Is2010() )
		{
			pProjectExt = "vcxproj";
		}
		if ( pExt == NULL ||
			V_stricmp_fast( pExt, "mak" ) == 0 )
		{
			pUpdateBuffer->Set( pProjectFilename, ".", pProjectExt );
			return pUpdateBuffer->Get();
		}

		return pProjectFilename;
	}

	virtual void GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects )
	{
		// Default extension.
		CUtlPathStringHolder tmpSolutionFilename;
		if ( !V_GetFileExtension( pSolutionFilename ) )
		{
			tmpSolutionFilename.Set( pSolutionFilename, ".sln" );
			pSolutionFilename = tmpSolutionFilename.Get();
		}

		CUtlVector<CUtlString> allProjectPlatforms;
		{
			CUtlVector<CUtlString> platformCollect;
			for ( int i = 0; i < projects.Count(); i++ )
			{
				//collect all the platforms supported by this project
				platformCollect.RemoveAll();
				projects[i]->m_pProjectGenerator->EnumerateSupportedVPCTargetPlatforms( platformCollect );
				
				//add each supported platform to the final list if it's not already in there
				for ( int j = 0; j < platformCollect.Count(); ++j )
				{
					if ( !allProjectPlatforms.IsValidIndex( allProjectPlatforms.Find( platformCollect[j] ) ) )
					{
						allProjectPlatforms.AddToTail( platformCollect[j] );
					}
				}
			}
		}

		g_pVPC->VPCStatus( true, "\nWriting solution file %s.", pSolutionFilename );

		// Write the file.
		FILE *fp = fopen( pSolutionFilename, "wt" );
		if ( !fp )
			g_pVPC->VPCError( "Can't open %s for writing.", pSolutionFilename );

		if ( g_pVPC->Is2015() )
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 14.00\n" );
			fprintf( fp, "# Visual Studio 2015\n" );
		}
		else if ( g_pVPC->Is2013() )
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 12.00\n" ); // Format didn't change from VS 2012 to VS 2013
			fprintf( fp, "# Visual Studio 2013\n" );
		}
		else if ( g_pVPC->Is2012() )
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 12.00\n" );
			fprintf( fp, "# Visual Studio 2012\n" );
		}
		else if ( g_pVPC->Is2010() )
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 11.00\n" );
			fprintf( fp, "# Visual Studio 2010\n" );
		}
		else
		{
			fprintf( fp, "\xef\xbb\xbf\nMicrosoft Visual Studio Solution File, Format Version 9.00\n" );
			fprintf( fp, "# Visual Studio 2005\n" );
		}
		fprintf( fp, "#\n" );
		fprintf( fp, "# Automatically generated solution:\n" );
		fprintf( fp, "# devtools\\bin\\vpc " );
#if defined( PLATFORM_WINDOWS )
		for ( int k = 1; k < __argc; ++ k )
			fprintf( fp, "%s ", __argv[k] );
#endif
		fprintf( fp, "\n" );
		fprintf( fp, "#\n" );
		fprintf( fp, "#\n" );

		if ( !g_pVPC->Is2010() )
		{
			// if /slnItems <filename> is passed on the command line, build a Solution Items project
			const char *pSolutionItemsFilename = g_pVPC->GetSolutionItemsFilename();
			if ( pSolutionItemsFilename[0] != '\0' )
			{
				fprintf( fp, "Project(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"Solution Items\", \"Solution Items\", \"{AAAAAAAA-8B4A-11D0-8D11-90A07D6D6F7D}\"\n" );
				fprintf( fp, "\tProjectSection(SolutionItems) = preProject\n" );
				WriteSolutionItems( fp );
				fprintf( fp, "\tEndProjectSection\n" );
				fprintf( fp, "EndProject\n" );
			}
		}

		//Write the data for all the solution folders
		CUtlVector< SolutionFolderData_t > solutionFolderData;
		WriteSolutionFolders( fp, solutionFolderData );

		for ( int i=0; i < projects.Count(); i++ )
		{
			CDependency_Project *pCurProject = projects[i];

			char szBasePath[MAX_PATH];
			V_strncpy( szBasePath, pCurProject->m_Filename, ARRAYSIZE( szBasePath ) );
			V_StripFilename( szBasePath );

			char szOutputFilePath[MAX_PATH];
			V_ComposeFileName( szBasePath, pCurProject->GetProjectFileName(), szOutputFilePath, ARRAYSIZE( szOutputFilePath ) );

			// Get a relative filename for the vcproj file.
			CUtlPathStringHolder updatedFilename;
			const char *pFullProjectFilename = UpdateProjectFilename( szOutputFilePath,
                                                                      &updatedFilename );
			char szRelativeFilename[MAX_FIXED_PATH];
			if ( !V_MakeRelativePath( pFullProjectFilename, g_pVPC->GetSourcePath(), szRelativeFilename, sizeof( szRelativeFilename ) ) )
				g_pVPC->VPCError( "Can't make a relative path (to the base source directory) for %s.", pFullProjectFilename );

			char szSolutionGUID[256];
			GetVCPROJSolutionGUID( V_GetFileExtension( szRelativeFilename ), szSolutionGUID );

			if ( g_pVPC->Is2010() )
			{
				char *pLastDot;
				char pProjectName[MAX_BASE_FILENAME];

				// It looks like Incredibuild 3.6 looks to build projects using the full project name
				//	with _x360 or _win64 attached to the end. Basically, the full project filename with
				//	the path and .vcxproj extension removed.
				Sys_StripPath( pFullProjectFilename, pProjectName, sizeof( pProjectName ) );
				pLastDot = V_strrchr( pProjectName, '.' );
				if (pLastDot)
				{
					*pLastDot = 0;
				}

				fprintf( fp, "Project(\"%s\") = \"%s\", \"%s\", \"{%s}\"\n", szSolutionGUID, pProjectName, szRelativeFilename, pCurProject->GetProjectGUIDString() );
			}
			else
			{
				fprintf( fp, "Project(\"%s\") = \"%s\", \"%s\", \"{%s}\"\n", szSolutionGUID, pCurProject->GetName(), szRelativeFilename, pCurProject->GetProjectGUIDString() );
			}
			bool bHasDependencies = false;

			for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
			{
				if ( i == iTestProject )
					continue;

				CDependency_Project *pTestProject = projects[iTestProject];
				if ( pCurProject->DependsOn( pTestProject, k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagRecurse ) ||
					pCurProject->DependsOn( pTestProject, k_EDependsOnFlagCheckAdditionalDependencies | k_EDependsOnFlagTraversePastLibs ) )
				{
					if ( !bHasDependencies )
					{
						fprintf( fp, "\tProjectSection(ProjectDependencies) = postProject\n" );
						bHasDependencies = true;
					}
					fprintf( fp, "\t\t{%s} = {%s}\n", projects[iTestProject]->GetProjectGUIDString(), projects[iTestProject]->GetProjectGUIDString() );
				}
			}
			if ( bHasDependencies )
				fprintf( fp, "\tEndProjectSection\n" );

			fprintf( fp, "EndProject\n" );
		}

		if ( g_pVPC->Is2010() )
		{
			fprintf( fp, "Global\n" );
			fprintf( fp, "	GlobalSection(SolutionConfigurationPlatforms) = preSolution\n" );
			for ( int nPlatformIter = 0; nPlatformIter < allProjectPlatforms.Count(); ++nPlatformIter )
			{
				const char *szVPCPlatformName = allProjectPlatforms[nPlatformIter].Get();
				fprintf( fp, "		Debug|%s = Debug|%s\n", szVPCPlatformName, szVPCPlatformName );
				fprintf( fp, "		Release|%s = Release|%s\n", szVPCPlatformName, szVPCPlatformName );
			}
			fprintf( fp, "	EndGlobalSection\n" );
			fprintf( fp, "	GlobalSection(ProjectConfigurationPlatforms) = postSolution\n" );

			for ( int nPlatformIter = 0; nPlatformIter < allProjectPlatforms.Count(); ++nPlatformIter )
			{
				const char *szVPCPlatformName = allProjectPlatforms[nPlatformIter].Get();

				for ( int i=0; i < projects.Count(); i++ )
				{
					const char *ProjectGUID = projects[i]->GetProjectGUIDString();

					IBaseProjectGenerator *pProjectGenerator = projects[i]->m_pProjectGenerator;

					bool bBuilds = pProjectGenerator->BuildsForTargetPlatform( szVPCPlatformName );
					bool bDeploys = pProjectGenerator->DeploysForVPCTargetPlatform( szVPCPlatformName );
					if ( bBuilds || bDeploys )
					{
						CUtlString sPlatformAlias = pProjectGenerator->GetSolutionPlatformAlias( szVPCPlatformName, this );
						const char *szVisualStudioPlatformName = sPlatformAlias.Get();

						fprintf( fp, "		{%s}.Debug|%s.ActiveCfg = Debug|%s\n", ProjectGUID, szVPCPlatformName, szVisualStudioPlatformName );
						if ( bBuilds )
						{
							fprintf( fp, "		{%s}.Debug|%s.Build.0 = Debug|%s\n", ProjectGUID, szVPCPlatformName, szVisualStudioPlatformName );
						}
						if ( bDeploys )
						{
							fprintf( fp, "		{%s}.Debug|%s.Deploy.0 = Debug|%s\n", ProjectGUID, szVPCPlatformName, szVisualStudioPlatformName );
						}

						fprintf( fp, "		{%s}.Release|%s.ActiveCfg = Release|%s\n", ProjectGUID, szVPCPlatformName, szVisualStudioPlatformName );
						if ( bBuilds )
						{
							fprintf( fp, "		{%s}.Release|%s.Build.0 = Release|%s\n", ProjectGUID, szVPCPlatformName, szVisualStudioPlatformName );
						}
						if ( bDeploys )
						{
							fprintf( fp, "		{%s}.Release|%s.Deploy.0 = Release|%s\n", ProjectGUID, szVPCPlatformName, szVisualStudioPlatformName );
						}
					}
				}
			}

			fprintf( fp, "	EndGlobalSection\n" );
			fprintf( fp, "	GlobalSection(SolutionProperties) = preSolution\n" );
			fprintf( fp, "		HideSolutionNode = FALSE\n" );
			fprintf( fp, "	EndGlobalSection\n" );

			if ( solutionFolderData.Count() > 0 )
			{
				//Add the nested solution folders
				fprintf( fp, "	GlobalSection(NestedProjects) = preSolution\n" );
				FOR_EACH_VEC( solutionFolderData, i )
				{
					if ( !solutionFolderData[i].strParentGUID.IsEmpty() && ShouldWriteSolutionFolder( solutionFolderData[i], solutionFolderData ) )
					{
						fprintf( fp, "\t\t%s = %s\n", solutionFolderData[i].strGUID.Get(), solutionFolderData[i].strParentGUID.Get() );
					}
				}
				fprintf( fp, "	EndGlobalSection\n" );
			}

			fprintf( fp, "EndGlobal\n" );
		}

		fclose( fp );
		Sys_CopyToMirror( pSolutionFilename );
	}

    virtual const char *GetSolutionFileExtension() { return "sln"; }	
	
	virtual SolutionType_t GetSolutionType( void ) OVERRIDE { return ST_VISUALSTUDIO; }
	
	const char* FindInFile( const char *pFilename, const char *pFileData, const char *pSearchFor )
	{
		const char *pPos = V_stristr( pFileData, pSearchFor );
		if ( !pPos )
		{
			g_pVPC->VPCError( "Can't find %s in %s.", pSearchFor, pFilename );
		}

		return pPos + V_strlen( pSearchFor );
	}

	// Parse g_SolutionItemsFilename, reading in filenames (including wildcards),
	// and add them to the Solution Items project we're already writing.
	void WriteSolutionItems( FILE *fp )
	{
#if defined( PLATFORM_WINDOWS )
		CUtlPathStringHolder fullSolutionItemsPath;
		if ( V_IsAbsolutePath( g_pVPC->GetSolutionItemsFilename() ) )
			fullSolutionItemsPath.Set( g_pVPC->GetSolutionItemsFilename() );
		else
			fullSolutionItemsPath.ComposeFileName( g_pVPC->GetStartDirectory(), g_pVPC->GetSolutionItemsFilename() );
		
		g_pVPC->GetScript().PushScript( fullSolutionItemsPath );

		int numSolutionItems = 0;
		while ( g_pVPC->GetScript().GetData() )
		{
			// read a line
			const char *pToken = g_pVPC->GetScript().GetToken( false );

			// strip out \r\n chars
			char *end = V_strstr( pToken, "\n" );
			if ( end )
			{
				*end = '\0';
			}
			end = V_strstr( pToken, "\r" );
			if ( end )
			{
				*end = '\0';
			}

			// bail on strings too small to be paths
			if ( V_strlen( pToken ) < 3 )
				continue;

			// compose an absolute path w/o any ../
			CUtlPathStringHolder fullPath;
			if ( V_IsAbsolutePath( pToken ) )
                fullPath.Set( pToken );
			else
				fullPath.ComposeFileName( g_pVPC->GetStartDirectory(), pToken );

			fullPath.FixSlashesAndDotSlashes();

			if ( V_strstr( fullPath, "*" ) != NULL )
			{
				// wildcard!
				CUtlPathStringHolder wildcardPath( fullPath );
				wildcardPath.StripFilename();

				struct _finddata32_t data;
				intptr_t handle = _findfirst32( fullPath, &data );
				if ( handle != -1L )
				{
					do 
					{
						if ( ( data.attrib & _A_SUBDIR ) == 0 )
						{
							// not a dir, just a filename - add it
							fullPath.ComposeFileName( wildcardPath, data.name );
                            fullPath.FixSlashesAndDotSlashes();
                            fprintf( fp, "\t\t%s = %s\n", fullPath.Get(), fullPath.Get() );
                            ++numSolutionItems;
						}
					} while ( _findnext32( handle, &data ) == 0 );

					_findclose( handle );
				}
			}
			else
			{
				// just a file - add it
				fprintf( fp, "\t\t%s = %s\n", fullPath.Get(), fullPath.Get() );
				++numSolutionItems;
			}
		}

		g_pVPC->GetScript().PopScript();

		Msg( "Found %d solution files in %s\n", numSolutionItems, g_pVPC->GetSolutionItemsFilename() );
#endif
	}

	void AddSolutionFolder( CUtlString strAbsPath, CUtlString strSearchPattern, CUtlVector< SolutionFolderData_t > &solutionFolders )
	{
		solutionFolders.AddToTail();
		SolutionFolderData_t &folder = solutionFolders.Tail();
		folder.strAbsPath = strAbsPath;
		folder.strAbsPath.StripTrailingSlash();
		folder.strSearchPattern = strSearchPattern;

		//Get the name of the folder that will be added to the solution
		int nPathLength = folder.strAbsPath.Length();
		while ( nPathLength > 0 )
		{
			//Find the last path separator in the path
			if ( PATHSEPARATOR( folder.strAbsPath[nPathLength-1] ) )
			{
				break;
			}
			nPathLength--;
		}
		folder.strFolderName = folder.strAbsPath.Slice( nPathLength );
		folder.strFolderName.ToLower();

		//Get the GUID of the folder
		MD5Context_t ctx;
		unsigned char digest[MD5_DIGEST_LENGTH];
		V_memset( &ctx, 0, sizeof( ctx ) );
		V_memset( digest, 0, sizeof( digest ) );
		MD5Init( &ctx );
		MD5Update( &ctx, (unsigned char *)folder.strAbsPath.Get(), folder.strAbsPath.Length() );
		MD5Final( digest, &ctx );

		char szMD5[64];
		V_binarytohex( digest, MD5_DIGEST_LENGTH, szMD5, sizeof( szMD5 ) );
		V_strupper_fast( szMD5 );

		char szGUID[100];
		V_snprintf( szGUID, sizeof( szGUID ), "{%8.8s-%4.4s-%4.4s-%4.4s-%12.12s}", szMD5, &szMD5[8], &szMD5[12], &szMD5[16], &szMD5[20] );
		folder.strGUID = szGUID;
	}

	void AddFilesToSolutionFolder( CUtlVector< SolutionFolderData_t > &folders, int nIndex )
	{
#if defined( PLATFORM_WINDOWS )
		CUtlString strSearchPattern = folders[nIndex].strSearchPattern;
		bool bAllFiles = strSearchPattern == "*.*";
		const char *pszSearchExtension = V_GetFileExtensionSafe( strSearchPattern );

		CUtlString strSearchPath = CUtlString::PathJoin( folders[nIndex].strAbsPath, "*.*" );

		WIN32_FIND_DATA findFileData;
		HANDLE hFind = FindFirstFile( strSearchPath, &findFileData );
		if ( hFind != INVALID_HANDLE_VALUE )
		{
			do
			{
				//FindFirstFile and FindNextFile find "." and ".." as files when searched using "*.*"
				//we don't want these to be added to our lists
				if ( findFileData.cFileName[0] != '.' )
				{
					//If the found file is actually a directory
					if ( findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
					{
						//Found a sub-dir, add it to the list of folders and add all the files in the sub-dir
						CUtlString strSubDirPath = CUtlString::PathJoin( folders[nIndex].strAbsPath, findFileData.cFileName );
						AddSolutionFolder( strSubDirPath, folders[nIndex].strSearchPattern, folders );

						//Set the parent GUID for the sub-directory
						int nSubDirIndex = folders.Count() - 1;
						folders[nSubDirIndex].strParentGUID = folders[nIndex].strGUID;

						//Recursively add the files from the sub-dir
						AddFilesToSolutionFolder( folders, nSubDirIndex );
					}
					else
					{
						//Add this file to the list if we are adding all files or if this files extension matches the search pattern
						const char *pszExtension = V_GetFileExtensionSafe( findFileData.cFileName );
						if ( bAllFiles || !V_stricmp_fast( pszExtension, pszSearchExtension ) )
						{
							folders[nIndex].files.AddToTail( CUtlString::PathJoin( folders[nIndex].strAbsPath, findFileData.cFileName ) );
						}
					}
				}

			} while ( FindNextFile( hFind, &findFileData ) != 0 );

			FindClose( hFind );
		}
#endif
	}

	bool ShouldWriteSolutionFolder( SolutionFolderData_t &folder, CUtlVector< SolutionFolderData_t > &solutionFolderData )
	{
		if ( folder.files.Count() > 0 )
		{
			//Write the folder if it has files
			return true;
		}
		else
		{
			//Only write empty folders if they are the parent of another folder that has files or children that have files
			FOR_EACH_VEC( solutionFolderData, i )
			{
				if ( folder.strGUID == solutionFolderData[i].strParentGUID && ShouldWriteSolutionFolder( solutionFolderData[i], solutionFolderData ) )
				{
					return true;
				}
			}
		}

		return false;
	}

	void WriteSolutionFolders( FILE *fp, CUtlVector< SolutionFolderData_t > &solutionFolderData )
	{
		const CUtlVector< CUtlString > &solutionFolderNames = g_pVPC->GetSolutionFolderNames();

		char szOldPath[MAX_FIXED_PATH];
		V_GetCurrentDirectory( szOldPath, ARRAYSIZE( szOldPath ) );
		V_SetCurrentDirectory( g_pVPC->GetSourcePath() );

		FOR_EACH_VEC( solutionFolderNames, x )
		{
			//Get the path and search pattern for the folder
			CUtlString strAbsPath, strSearchPattern;
			if ( solutionFolderNames[x].GetExtension().IsEmpty() )
			{
				//No search pattern provided, assume "*.*" (all files)
				strAbsPath = solutionFolderNames[x].AbsPath( NULL, k_bVPCForceLowerCase );
				strSearchPattern = "*.*";
			}
			else
			{
				//Separate the path and search pattern
				strAbsPath = solutionFolderNames[x].StripFilename().AbsPath( NULL, k_bVPCForceLowerCase );
				strSearchPattern = solutionFolderNames[x].UnqualifiedFilename();
			}

			AddSolutionFolder( strAbsPath, strSearchPattern, solutionFolderData );

			AddFilesToSolutionFolder( solutionFolderData, solutionFolderData.Count() - 1 );
		}

		V_SetCurrentDirectory( szOldPath );

		//Write out each solution folder
		FOR_EACH_VEC( solutionFolderData, i )
		{
			if ( ShouldWriteSolutionFolder( solutionFolderData[i], solutionFolderData ) )
			{
				fprintf( fp, "Project(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = \"%s\", \"%s\", \"%s\"\n", solutionFolderData[i].strFolderName.Get(), solutionFolderData[i].strFolderName.Get(), solutionFolderData[i].strGUID.Get() );

				if ( solutionFolderData[i].files.Count() > 0 )
				{
					fprintf( fp, "\tProjectSection(SolutionItems) = preProject\n" );

					FOR_EACH_VEC( solutionFolderData[i].files, j )
					{
						fprintf( fp, "\t\t%s = %s\n", solutionFolderData[i].files[j].Get(), solutionFolderData[i].files[j].Get() );
					}

					fprintf( fp, "\tEndProjectSection\n" );
				}
				fprintf( fp, "EndProject\n" );
			}
		}
	}
};


static CSolutionGenerator_Win32 g_SolutionGenerator_Win32;
IBaseSolutionGenerator* GetSolutionGenerator_Win32()
{
	return &g_SolutionGenerator_Win32;
}


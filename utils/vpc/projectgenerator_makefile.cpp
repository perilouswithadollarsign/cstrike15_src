//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "vpc.h"
#include "baseprojectdatacollector.h"
#include "tier1/utlstack.h"
#include "projectgenerator_xcode.h"

extern bool g_bForceGenerate;

static const char *g_pOption_OptimizerLevel = "$OptimizerLevel";
static const char *g_pOption_GameOutputFile = "$GameOutputFile";
static const char *g_pOption_SymbolVisibility = "$SymbolVisibility";
static const char *g_pOption_ConfigurationType = "$ConfigurationType";
static const char *g_pOption_PrecompiledHeader = "$Create/UsePrecompiledHeader";
static const char *g_pOption_UsePCHThroughFile = "$Create/UsePCHThroughFile";
static const char *g_pOption_PrecompiledHeaderFile = "$PrecompiledHeaderFile";
static const char *g_pOption_SystemLibraries = "$SystemLibraries";
static const char *g_pOption_SystemFrameworks = "$SystemFrameworks";
static const char *g_pOption_LocalFrameworks = "$LocalFrameworks";
static const char *g_pOption_ExtraCompilerFlags = "$GCC_ExtraCompilerFlags";
static const char *g_pOption_ExtraLinkerFlags = "$GCC_ExtraLinkerFlags";
static const char *g_pOption_ForceInclude = "$ForceIncludes";
static const char *g_pOption_LowerCaseFileNames = "$LowerCaseFileNames";

static const char *g_pOption_PostBuildEvent = "$PostBuildEvent";
static const char *g_pOption_CommandLine = "$CommandLine";
static const char *g_pOption_Description = "$Description";
static const char *g_pOption_Outputs = "$Outputs";

static const char *k_pszBase_Makefile = "$(SRCROOT)/devtools/makefile_base_posix.mak";

// These are the only properties we care about for makefiles.
static const char *g_pRelevantProperties[] =
{
	g_pOption_AdditionalIncludeDirectories,
	g_pOption_OptimizerLevel,
	g_pOption_OutputFile,
	g_pOption_GameOutputFile,
	g_pOption_SymbolVisibility,
	g_pOption_PreprocessorDefinitions,
	g_pOption_ConfigurationType,
	g_pOption_ImportLibrary,
	g_pOption_PrecompiledHeader,
	g_pOption_UsePCHThroughFile,
	g_pOption_PrecompiledHeaderFile,
	g_pOption_CommandLine,
	g_pOption_Outputs,
	g_pOption_Description,
	g_pOption_SystemLibraries,
    g_pOption_SystemFrameworks,
    g_pOption_LocalFrameworks,
	g_pOption_ExtraCompilerFlags,
	g_pOption_ExtraLinkerFlags,
	g_pOption_ForceInclude,
};


CRelevantPropertyNames g_RelevantPropertyNames =
{
	g_pRelevantProperties,
	Q_ARRAYSIZE( g_pRelevantProperties )
};

void MakeFriendlyProjectName( char *pchProject );

void V_MakeAbsoluteCygwinPath( char *pOut, int outLen, const char *pRelativePath )
{
	// While generating makefiles under Win32, we must translate drive letters like c:\
	// to Cygwin-style paths like /cygdrive/c/
#ifdef _WIN32
	char tmp[MAX_PATH];
	V_MakeAbsolutePath( tmp, sizeof( tmp ), pRelativePath );
	V_FixSlashes( tmp, '/' );

	if ( tmp[0] != 0 && tmp[1] == ':' && tmp[2] == '/' )
	{
		// Ok, this is an absolute path
		V_snprintf( pOut, outLen, "/cygdrive/%c/", tmp[0] );
		V_strncat( pOut, &tmp[3], outLen );
	}
	else
#endif // _WIN32
	{
		V_MakeAbsolutePath( pOut, outLen, pRelativePath );
		V_RemoveDotSlashes( pOut );
	}
}

static const char* UsePOSIXSlashes( const char *pStr )
{
	static char str[2048];
	V_strncpy( str, pStr, sizeof( str ) );
	V_FixSlashes( str, '/' );
	return str;
}


// pExt should be the bare extension without the . in front. i.e. "h", "cpp", "lib".
static inline bool CheckExtension( const char *pFilename, const char *pExt )
{
	Assert( pExt[0] != '.' );

	int nFilenameLen = V_strlen( pFilename );
	int nExtensionLen = V_strlen( pExt );

	return (nFilenameLen > nExtensionLen && pFilename[nFilenameLen-nExtensionLen-1] == '.' && V_stricmp( &pFilename[nFilenameLen-nExtensionLen], pExt ) == 0 );
}


static bool CheckExtensions( const char *pFilename, const char **ppExtensions )
{
	for ( int i=0; ppExtensions[i] != NULL; i++ )
	{
		if ( CheckExtension( pFilename, ppExtensions[i] ) )
			return true;
	}
	return false;
}


static void GetObjFilenameForFile( const char *pConfigName, const char *pFilename, char *pOut, int maxLen )
{
	char sBaseFilename[MAX_PATH];
	V_FileBase( pFilename, sBaseFilename, sizeof( sBaseFilename ) );
	//V_strlower( sBaseFilename );

	const char *pObjExtension = (CheckExtension( pFilename, "cxx" ) ? "oxx" : "o");

	V_snprintf( pOut, maxLen, "$(OBJ_DIR)/%s.%s", sBaseFilename, pObjExtension );
}


// This class drastically accelerates looking up which file creates which precompiled header.
class CPrecompiledHeaderAccel
{
public:
	void Setup( CUtlDict<CFileConfig*,int> &files, CFileConfig *pBaseConfig )
	{
		for ( int i=files.First(); i != files.InvalidIndex(); i=files.Next( i ) )
		{
			CFileConfig *pFile = files[i];

			for ( int iSpecific=pFile->m_Configurations.First(); iSpecific != pFile->m_Configurations.InvalidIndex(); iSpecific=pFile->m_Configurations.Next( iSpecific ) )
			{
				CSpecificConfig *pSpecific = pFile->m_Configurations[iSpecific];
				if ( pSpecific->m_bFileExcluded )
					continue;

				// Does this file create a precompiled header?
				const char *pPrecompiledHeaderOption = pSpecific->GetOption( g_pOption_PrecompiledHeader );
				if ( pPrecompiledHeaderOption && V_stristr( pPrecompiledHeaderOption, "Create" ) )
				{   
					// Ok, which header do we scan through?
					const char *pUsePCHThroughFile = pSpecific->GetOption( g_pOption_UsePCHThroughFile );
					if ( !pUsePCHThroughFile )
					{
						g_pVPC->VPCError( "File %s creates a precompiled header in config %s but no UsePCHThroughFile option specified.", pFile->m_Filename.String(), pSpecific->GetConfigName() );
					}

					char sLookup[1024];
					V_snprintf( sLookup, sizeof( sLookup ), "%s__%s", pSpecific->GetConfigName(), pUsePCHThroughFile );

					if ( m_Lookup.Find( sLookup ) != m_Lookup.InvalidIndex() )
					{
						g_pVPC->VPCError( "File %s has UsePCHThroughFile of %s but another file already does.", pFile->m_Filename.String(), pUsePCHThroughFile );
					}

					m_Lookup.Insert( sLookup, pFile );
				}
			}
		}
	}

	CFileConfig* FindFileThatCreatesPrecompiledHeader( const char *pConfigName, const char *pUsePCHThroughFile )
	{
		char sLookup[1024];
		V_snprintf( sLookup, sizeof( sLookup ), "%s__%s", pConfigName, pUsePCHThroughFile );

		int i = m_Lookup.Find( sLookup );
		if ( i == m_Lookup.InvalidIndex() )
			return NULL;
		else
			return m_Lookup[i];
	}

private:
	// This indexes whatever file creates a certain precompiled header for a certain config.
	// These are indexed as <config name>_<pchthroughfile>.
	// So an entry might look like release_cbase.h
	CUtlDict<CFileConfig*,int> m_Lookup;
};



class CProjectGenerator_Makefile : public CBaseProjectDataCollector
{
public:

	typedef CBaseProjectDataCollector BaseClass;

	CProjectGenerator_Makefile() : BaseClass( &g_RelevantPropertyNames )
	{
	}

	virtual void Setup()
	{
	}

	virtual const char* GetProjectFileExtension()
	{
		return "mak";
	}
	
	virtual void EndProject()
	{
		const char *pMakefileFilename = g_pVPC->GetOutputFilename();
		
		CUtlString strProjectName = GetProjectName();
        bool bProjectIsCurrent = g_pVPC->IsProjectCurrent( pMakefileFilename ); 
        if ( g_pVPC->IsForceGenerate() || !bProjectIsCurrent )
        {
			g_pVPC->VPCStatus( true, "Saving makefile project for: '%s' File: '%s'", strProjectName.String(), g_pVPC->GetOutputFilename() );
            WriteMakefile( pMakefileFilename );
        }
        
		const char *pTargetPlatformName = g_pVPC->GetTargetPlatformName();
		if ( !pTargetPlatformName )
			g_pVPC->VPCError( "GetTargetPlatformName failed." );
		
		if ( !V_stricmp( pTargetPlatformName, "OSX32" )  || !V_stricmp( pTargetPlatformName, "OSX64" ) )
		{
            if ( g_pVPC->IsForceGenerate() || !bProjectIsCurrent )
            {
                // Write a XCode project as well.
                char sFilename[MAX_PATH];
                V_StripExtension( g_pVPC->GetOutputFilename(), sFilename, sizeof( sFilename ) );
                CProjectGenerator_XCode xcodeGenerator;
                xcodeGenerator.GenerateXCodeProject( this, sFilename, pMakefileFilename );
            }
            
            extern CUtlVector<CBaseProjectDataCollector*> g_vecPGenerators;
            g_vecPGenerators.AddToTail( this );
            
            extern IBaseProjectGenerator *g_pGenerator;
            g_pVPC->SetProjectGenerator( new CProjectGenerator_Makefile() );
		}
        else
        {
            Term();
        }
	}

	void WriteSourceFilesList( FILE *fp, const char *pListName, const char **pExtensions, const char *pConfigName )
	{
		fprintf( fp, "%s= \\\n", pListName );
		for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next(i) )
		{
			CFileConfig *pFileConfig = m_Files[i];
			if ( pFileConfig->IsExcludedFrom( pConfigName ) )
				continue;

			const char *pFilename = m_Files[i]->m_Filename.String();
			if ( CheckExtensions( pFilename, pExtensions ) )
			{
				fprintf( fp, "    %s \\\n", UsePOSIXSlashes( pFilename ) );
			}
		}
		fprintf( fp, "\n\n" );
	}

	void WriteNonConfigSpecificStuff( FILE *fp )
	{
		// NAME
		char szName[256];
		V_strncpy( szName, m_ProjectName.String(), sizeof(szName) );
		MakeFriendlyProjectName( szName );
		fprintf( fp, "NAME=%s\n", szName );

		// SRCDIR
		char sSrcRootRelative[MAX_PATH];
		g_pVPC->ResolveMacrosInString( "$SRCDIR", sSrcRootRelative, sizeof( sSrcRootRelative ) );

		fprintf( fp, "SRCROOT=%s\n", UsePOSIXSlashes( sSrcRootRelative ) );

		// TargetPlatformName
		const char *pTargetPlatformName = g_pVPC->GetTargetPlatformName();
		if ( !pTargetPlatformName )
			g_pVPC->VPCError( "GetTargetPlatformName failed." );
		fprintf( fp, "TARGET_PLATFORM=%s\n", pTargetPlatformName );
		fprintf( fp, "PWD:=$(shell pwd)\n" );


		// Select debug config if no config is specified.
		fprintf( fp, "# If no configuration is specified, \"release\" will be used.\n" );
		fprintf( fp, "ifeq \"$(CFG)\" \"\"\n" );
		fprintf( fp, "CFG=release\n" );
		fprintf( fp, "endif\n\n" );
	}

	void WriteConfigSpecificStuff( CSpecificConfig *pConfig, FILE *fp, CPrecompiledHeaderAccel *pAccel )
	{
		KeyValues *pKV = pConfig->m_pKV;

		fprintf( fp, "#\n#\n# CFG=%s\n#\n#\n\n", pConfig->GetConfigName() );
		fprintf( fp, "ifeq \"$(CFG)\" \"%s\"\n\n", pConfig->GetConfigName() );

		// GCC_ExtraCompilerFlags
		// Hopefully, they don't ever need to use backslashes because we're turning them into forward slashes here.
		// If that does become a problem, we can put some token around the pathnames we need to be fixed up and leave the rest alone.
		fprintf( fp, "GCC_ExtraCompilerFlags=%s\n", UsePOSIXSlashes( pKV->GetString( g_pOption_ExtraCompilerFlags, "" ) ) );

		// GCC_ExtraLinkerFlags
		fprintf( fp, "GCC_ExtraLinkerFlags=%s\n", pKV->GetString( g_pOption_ExtraLinkerFlags, "" ) );

		// SymbolVisibility
		fprintf( fp, "SymbolVisibility=%s\n", pKV->GetString( g_pOption_SymbolVisibility, "hidden" ) );
		
		// OptimizerLevel
		fprintf( fp, "OptimizerLevel=%s\n", pKV->GetString( g_pOption_OptimizerLevel, "$(SAFE_OPTFLAGS_GCC_422)" ) );
		
		// system libraries
        {
            fprintf( fp, "SystemLibraries=" );
            {
                CSplitString libs( pKV->GetString( g_pOption_SystemLibraries ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                for ( int i=0; i < libs.Count(); i++ )
                {
                    fprintf( fp, "-l%s ", libs[i] );
                }
            }
            if ( !V_stricmp( g_pVPC->GetTargetPlatformName(), "OSX32" )  || !V_stricmp( g_pVPC->GetTargetPlatformName(), "OSX64" ) )
            {
                char rgchFrameworkCompilerFlags[1024]; rgchFrameworkCompilerFlags[0] = '\0';
                CSplitString systemFrameworks( pKV->GetString( g_pOption_SystemFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                for ( int i=0; i < systemFrameworks.Count(); i++ )
                {
                    fprintf( fp, "-framework %s ", systemFrameworks[i] );
                }
                CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                for ( int i=0; i < localFrameworks.Count(); i++ )
                {
                    char rgchFrameworkName[MAX_PATH];
                    V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );
                    V_StripFilename( localFrameworks[i] );
                    fprintf( fp, "-F%s ", localFrameworks[i] );
                    fprintf( fp, "-framework %s ", rgchFrameworkName );
                    strcat( rgchFrameworkCompilerFlags, "-F" );
                    strcat( rgchFrameworkCompilerFlags, localFrameworks[i] );
                }
                fprintf( fp, "\n" );
                if ( rgchFrameworkCompilerFlags[0] )
                    // the colon here is important - and should probably get percolated to more places in our generated
                    // makefiles - it means to perform the assignment once, rather than at evaluation time
                    fprintf( fp, "GCC_ExtraCompilerFlags:=$(GCC_ExtraCompilerFlags) %s\n", rgchFrameworkCompilerFlags );
            }
            else
                fprintf( fp, "\n" );
        }
		
		macro_t *pMacro = g_pVPC->FindOrCreateMacro( "_DLL_EXT", false, NULL );
		if ( pMacro )
			fprintf( fp, "DLL_EXT=%s\n", pMacro->value.String() );

		pMacro = g_pVPC->FindOrCreateMacro( "_SYM_EXT", false, NULL );
		if ( pMacro )
			fprintf( fp, "SYM_EXT=%s\n", pMacro->value.String() );
		
		// ForceIncludes
		{
			CSplitString outStrings( pKV->GetString( g_pOption_ForceInclude ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
			fprintf( fp, "FORCEINCLUDES= " );
			for ( int i=0; i < outStrings.Count(); i++ )
			{
				if ( V_strlen( outStrings[i] ) > 2 )
					fprintf( fp, "-include %s ", UsePOSIXSlashes( outStrings[i] ) );
			}
		}
			fprintf( fp, "\n" );

		// DEFINES
		{
			CSplitString outStrings( pKV->GetString( g_pOption_PreprocessorDefinitions ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
			fprintf( fp, "DEFINES= " );
			for ( int i=0; i < outStrings.Count(); i++ )
			{
				fprintf( fp, "-D%s ", outStrings[i] );
			}
		}
		
		// Add VPC macros marked to become defines.
		CUtlVector< macro_t* > macroDefines;
		g_pVPC->GetMacrosMarkedForCompilerDefines( macroDefines );
		for ( int i=0; i < macroDefines.Count(); i++ )
		{
			macro_t *pMacro = macroDefines[i];
			fprintf( fp, "-D%s=%s ", pMacro->name.String(), pMacro->value.String() );
		}

		fprintf( fp, "\n" );
		// INCLUDEDIRS
		{
			CSplitString outStrings( pKV->GetString( g_pOption_AdditionalIncludeDirectories ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
			fprintf( fp, "INCLUDEDIRS= " );
			for ( int i=0; i < outStrings.Count(); i++ )
			{
				char sDir[MAX_PATH];
				V_strncpy( sDir, outStrings[i], sizeof( sDir ) );
				if ( !V_stricmp( sDir, "$(IntDir)" ) )
					V_strncpy( sDir, "$(OBJ_DIR)", sizeof( sDir ) );

				V_FixSlashes( sDir, '/' );
				fprintf( fp, "%s ", sDir );
			}
			fprintf( fp, "\n" );
		}
		// CONFTYPE
		if ( V_stristr( pKV->GetString( g_pOption_ConfigurationType ), "dll" ) )
		{
			fprintf( fp, "CONFTYPE=dll\n" );

			// Write ImportLibrary for dll (so) builds.
			const char *pRelative = pKV->GetString( g_pOption_ImportLibrary, "" );
			fprintf( fp, "IMPORTLIBRARY=%s\n", UsePOSIXSlashes( pRelative ) );

			// GameOutputFile is where it copies OutputFile to.
			fprintf( fp, "GAMEOUTPUTFILE=%s\n", UsePOSIXSlashes( pKV->GetString( g_pOption_GameOutputFile, "" ) ) );
		}
		else if ( V_stristr( pKV->GetString( g_pOption_ConfigurationType ), "lib" ) )
		{
			fprintf( fp, "CONFTYPE=lib\n" );
		}
		else if ( V_stristr( pKV->GetString( g_pOption_ConfigurationType ), "exe" ) )
		{
			fprintf( fp, "CONFTYPE=exe\n" );
		}
		else
		{
			fprintf( fp, "CONFTYPE=***UNKNOWN***\n" );
		}

		// OutputFile is where it builds to.
		char szFixedOutputFile[MAX_PATH];
		V_strncpy( szFixedOutputFile, pKV->GetString( g_pOption_OutputFile ), sizeof( szFixedOutputFile ) );
		V_FixSlashes( szFixedOutputFile, '/' );
		fprintf( fp, "OUTPUTFILE=%s\n", szFixedOutputFile );

        fprintf( fp, "\n\n" );
        
        // post build event
        char rgchPostBuildCommand[2048]; rgchPostBuildCommand[0] = '\0';
        if ( pKV->GetString( g_pOption_CommandLine, NULL ) )
        {
            V_strncpy( rgchPostBuildCommand, pKV->GetString( g_pOption_CommandLine, NULL ), sizeof( rgchPostBuildCommand ) );
            // V_StripPrecedingAndTrailingWhitespace( rgchPostBuildCommand );
        }
        if ( Q_strlen( rgchPostBuildCommand ) )
            fprintf( fp, "POSTBUILDCOMMAND=%s\n", rgchPostBuildCommand );
        else
            fprintf( fp, "POSTBUILDCOMMAND=true\n" );
        
		fprintf( fp, "\n\n" );


		// Write all the filenames.
		const char *sSourceFileExtensions[] = {"cpp","cxx","cc","c","mm","cc",NULL};
		fprintf( fp, "\n" );
		WriteSourceFilesList( fp, "CPPFILES", (const char**)sSourceFileExtensions, pConfig->GetConfigName() );

		// LIBFILES
		char sImportLibraryFile[MAX_PATH];
		const char *pRelative = pKV->GetString( g_pOption_ImportLibrary, "" );
		V_strncpy( sImportLibraryFile, UsePOSIXSlashes( pRelative ), sizeof( sImportLibraryFile ) );
		V_RemoveDotSlashes( sImportLibraryFile );

		fprintf( fp, "LIBFILES = \\\n" );
		for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next(i) )
		{
			CFileConfig *pFileConfig = m_Files[i];
			if ( pFileConfig->IsExcludedFrom( pConfig->GetConfigName() ) )
				continue;

			char szFilename[MAX_PATH];
			V_strncpy( szFilename, UsePOSIXSlashes( pFileConfig->m_Filename.String() ), sizeof( szFilename ) );
			const char *pFilename = szFilename;
			if ( IsLibraryFile( pFilename ) )
			{
				char *pchFileName = (char*)Q_strrchr( pFilename, '/' ) + 1; 
				if ( !sImportLibraryFile[0] || Q_stricmp( sImportLibraryFile, pFilename ) ) // only link this as a library if it isn't our own output!
				{
					char szExt[32];
					Q_ExtractFileExtension( pFilename, szExt, sizeof(szExt) );
					if ( IsLibraryFile( pFilename ) && (pchFileName-1) && pchFileName[0] == 'l' && pchFileName[1] == 'i' && pchFileName[2] == 'b' 
													&& szExt[0] != 'a' ) // its a lib ext but not an archive file, link like a library
					{
						*(pchFileName-1) = 0;

						// Cygwin import libraries use ".dll.a", so get rid of any file extensions here.
						char *pExt;
						while ( 1 )
						{
							pExt = (char*)Q_strrchr( pchFileName, '.' );
							if ( !pExt || Q_strrchr( pchFileName, '/' ) > pExt || Q_strrchr( pchFileName, '\\' ) > pExt )
								break;

							*pExt = 0;
						}
						
						fprintf( fp, "    -L%s -l%s \\\n", pFilename, pchFileName + 3 ); // +3 to dodge the lib ext      
					}
					else
					{
						fprintf( fp, "    %s \\\n", pFilename );
					}
				}
			}
		}

		fprintf( fp, "\n\n" );
		
		fprintf( fp, "LIBFILENAMES = \\\n" );
		for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next(i) )
		{
			CFileConfig *pFileConfig = m_Files[i];
			if ( pFileConfig->IsExcludedFrom( pConfig->GetConfigName() ) )
				continue;
			
			const char *pFilename = pFileConfig->m_Filename.String();
			if ( IsLibraryFile( pFilename ) )
			{
				if ( !sImportLibraryFile[0] || Q_stricmp( sImportLibraryFile, pFilename ) ) // only link this as a library if it isn't our own output!
				{
					fprintf( fp, "    %s \\\n", UsePOSIXSlashes( pFilename ) );
				}
			}
		}
		
		fprintf( fp, "\n\n" );


		// Include the base makefile before the rules to build the .o files.
		fprintf( fp, "# Include the base makefile now.\n" );
		if ( g_pVPC->FindOrCreateConditional( "POSIX", false, CONDITIONAL_NULL ) )
		{
			fprintf( fp, "include %s\n\n\n", k_pszBase_Makefile );
		}


		CUtlVector< CUtlString > otherDependencies;
		
		// Scan the list of files for any generated dependencies so we can pull them up front
		for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next(i) )
		{
			CFileConfig *pFileConfig = m_Files[i];
			CSpecificConfig *pFileSpecificData = pFileConfig->GetOrCreateConfig( pConfig->GetConfigName(), pConfig );
			
			if ( pFileConfig->IsExcludedFrom( pConfig->GetConfigName() ) )
			{
				continue;
			}

			const char *pCustomBuildCommandLine = pFileSpecificData->GetOption( g_pOption_CommandLine );
			const char *pOutputFile = pFileSpecificData->GetOption( g_pOption_Outputs );
			if ( pOutputFile && pCustomBuildCommandLine && V_strlen( pCustomBuildCommandLine ) > 0 )
			{
				char szTempFilename[MAX_PATH];
				V_strncpy( szTempFilename, UsePOSIXSlashes( pFileConfig->m_Filename.String() ), sizeof( szTempFilename ) );
				const char *pFilename = szTempFilename;
								
				// This file uses a custom build step.
				char sFormattedOutputFile[MAX_PATH];
				char szAbsPath[MAX_PATH];
				V_MakeAbsolutePath( szAbsPath, sizeof(szAbsPath), pFilename );
				DoStandardVisualStudioReplacements( pOutputFile, szAbsPath, sFormattedOutputFile, sizeof( sFormattedOutputFile ) );
				
				CSplitString outFiles( sFormattedOutputFile, ";" );
				for ( int i = 0; i < outFiles.Count(); i ++ )
				{
					// Remember this as a dependency so the executable will depend on it.
					if ( otherDependencies.Find( outFiles[i] ) == otherDependencies.InvalidIndex() )
						otherDependencies.AddToTail( outFiles[i] );
				}
			}
		}
			
		WriteOtherDependencies( fp, otherDependencies );
		fprintf( fp, "\n\n" );
		
		// Now write the rules to build the .o files.
		// .o files go in [project dir]/obj/[config]/[base filename]
		for ( int i=m_Files.First(); i != m_Files.InvalidIndex(); i=m_Files.Next(i) )
		{
			CFileConfig *pFileConfig = m_Files[i];
			CSpecificConfig *pFileSpecificData = pFileConfig->GetOrCreateConfig( pConfig->GetConfigName(), pConfig );

			if ( pFileConfig->IsExcludedFrom( pConfig->GetConfigName() ) )
			{
				continue;
			}

			char szTempFilename[MAX_PATH];
			V_strncpy( szTempFilename, UsePOSIXSlashes( pFileConfig->m_Filename.String() ), sizeof( szTempFilename ) );
			const char *pFilename = szTempFilename;

			char sAbsFilename[MAX_PATH];
			V_strncpy( sAbsFilename, pFilename, sizeof( sAbsFilename ) );
							   
			// Custom build steps??
			const char *pCustomBuildCommandLine = pFileSpecificData->GetOption( g_pOption_CommandLine );
			const char *pOutputFile = pFileSpecificData->GetOption( g_pOption_Outputs );
			if ( pOutputFile && pCustomBuildCommandLine && V_strlen( pCustomBuildCommandLine ) > 0 )
			{
				// This file uses a custom build step.
				char sFormattedOutputFile[MAX_PATH];
				char sFormattedCommandLine[8192];
				char szAbsPath[MAX_PATH];
				V_MakeAbsolutePath( szAbsPath, sizeof(szAbsPath), sAbsFilename );
				DoStandardVisualStudioReplacements( pCustomBuildCommandLine, szAbsPath, sFormattedCommandLine, sizeof( sFormattedCommandLine ) );
				DoStandardVisualStudioReplacements( pOutputFile, szAbsPath, sFormattedOutputFile, sizeof( sFormattedOutputFile ) );

				CSplitString outFiles( sFormattedOutputFile, ";" );
				for ( int i = 0; i < outFiles.Count(); i ++ )
				{
					fprintf( fp, "%s ", outFiles[i] );
				}
				fprintf( fp, ": %s\n", UsePOSIXSlashes( szAbsPath ) );
				const char *pDescription = pFileSpecificData->GetOption( g_pOption_Description );
				DoStandardVisualStudioReplacements( pDescription, szAbsPath, sFormattedOutputFile, sizeof( sFormattedOutputFile ) );

				fprintf( fp, "\t @echo \"%s\";mkdir -p $(OBJ_DIR) 2> /dev/null;%s\n\n", sFormattedOutputFile, sFormattedCommandLine );
			}
			else if ( CheckExtensions( pFilename, (const char**)sSourceFileExtensions ) )
			{
				//V_strlower( sAbsFilename );
				char sObjFilename[MAX_PATH];
				GetObjFilenameForFile( pConfig->GetConfigName(), pFilename, sObjFilename, sizeof( sObjFilename ) );

				// Get the base obj filename for the .P file.
				char sPFileBase[MAX_PATH];
				V_StripExtension( sObjFilename, sPFileBase, sizeof( sPFileBase ) );

				// include the .P file which will include dependency information.
				fprintf( fp, "ifneq (clean, $(findstring clean, $(MAKECMDGOALS)))\n" );
				fprintf( fp, "\n%s.P: %s $(PWD)/%s %s $(OTHER_DEPENDENCIES)\n", sPFileBase, UsePOSIXSlashes( sAbsFilename ), g_pVPC->GetOutputFilename(), 
						 g_pVPC->FindOrCreateConditional( "POSIX", false, CONDITIONAL_NULL ) == NULL ? "" : k_pszBase_Makefile );			
				fprintf( fp, "\t$(GEN_DEP_FILE)\n");

				fprintf( fp, "\n-include %s.P\n", sPFileBase );
				fprintf( fp, "endif\n" );


				fprintf( fp, "\n%s : $(PWD)/%s $(PWD)/%s %s\n", sObjFilename, sAbsFilename, g_pVPC->GetOutputFilename(),
						 g_pVPC->FindOrCreateConditional( "POSIX", false, CONDITIONAL_NULL ) == NULL ? "" : k_pszBase_Makefile );
				fprintf( fp, "\t$(PRE_COMPILE_FILE)\n" );
				fprintf( fp, "\t$(COMPILE_FILE) $(POST_COMPILE_FILE)\n" );

			}
		}

		fprintf( fp, "\n\nendif # (CFG=%s)\n\n", pConfig->GetConfigName() );
		fprintf( fp, "\n\n" );
	}

	void WriteOtherDependencies( FILE *fp, CUtlVector< CUtlString > &otherDependencies )
	{
		fprintf( fp, "\nOTHER_DEPENDENCIES = \\\n" );
		for ( int i=0; i < otherDependencies.Count(); i++ )
		{
			fprintf( fp, "\t%s%s\n", otherDependencies[i].String(), 
				(i == otherDependencies.Count()-1) ? "" : " \\" );
		}
		fprintf( fp, "\n\n" );
	}

	void WriteMakefile( const char *pFilename )
	{
		FILE *fp = fopen( pFilename, "wt" );

		CPrecompiledHeaderAccel accel;
		accel.Setup( m_Files, &m_BaseConfigData );

		// Write all the non-config-specific stuff.
		WriteNonConfigSpecificStuff( fp );

		// Write each config out.
		for ( int i=m_BaseConfigData.m_Configurations.First(); i != m_BaseConfigData.m_Configurations.InvalidIndex(); i=m_BaseConfigData.m_Configurations.Next( i ) )
		{
			CSpecificConfig *pConfig = m_BaseConfigData.m_Configurations[i];
			WriteConfigSpecificStuff( pConfig, fp, &accel );
		}

		fclose( fp );
	}
	
	bool m_bForceLowerCaseFileName;
};

static CProjectGenerator_Makefile g_ProjectGenerator_Makefile;
IBaseProjectGenerator* GetMakefileProjectGenerator()
{
	return &g_ProjectGenerator_Makefile;
}


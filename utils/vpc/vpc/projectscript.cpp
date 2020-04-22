//========= Copyright 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"
#include "tier1/utldict.h"
#include "tier1/keyvalues.h"
#include "baseprojectdatacollector.h"
#include "tier1/fmtstr.h"

void VPC_Keyword_RemoveFile( void (*pFNNameTranslation)( CUtlStringBuilder *pStrBuf ) = nullptr );
static void VPC_AddLibraryDependencies( const char *pLibPath );

#ifndef STEAM
bool V_StrSubstInPlace( char *pchInOut, int cchInOut, const char *pMatch, const char *pReplaceWith, bool bCaseSensitive )
{
	char *pchT = (char *)stackalloc( cchInOut );
	if ( V_StrSubst( pchInOut, pMatch, pReplaceWith, pchT, cchInOut, bCaseSensitive ) )
	{
		V_strncpy( pchInOut, pchT, cchInOut );
		return true;
	}
	return false;
}
#endif

void VPC_Keyword_FileBuildOrderModifier()
{
	const char *pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] )
		return;

	if ( CharStrEq( pToken, '}' ) )
	{
		// end of section
		return;
	}

	g_pVPC->GetProjectGenerator()->HandleProperty( "$BuildOrderModifier", pToken );
}


void VPC_ParseFileSection()
{
	while ( 1 )
	{
		const char *pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
			break;

		if ( CharStrEq( pToken, '}' ) )
		{
			// end of section
			break;
		}

		if ( !V_stricmp_fast( pToken, "$Configuration" ) )
		{
			VPC_Keyword_FileConfiguration();
		}
		else if ( !V_stricmp_fast( pToken, "$BuildOrderModifier" ) )
		{
			VPC_Keyword_FileBuildOrderModifier();
		}
		else
		{
			g_pVPC->VPCSyntaxError( "Unrecognized token '%s' in file section. Possibly missing a $Configuration scope?", pToken );
		}
	}
}

static const char *ResolveCandidate( const char *pszFile, const char *pszPlatform, CUtlPathStringHolder *pRetBuf )
{
    CUtlStringBuilder* pStrBuf = g_pVPC->GetTempStringBuffer2();
	V_ComposeFileName( g_pVPC->GetProjectPath(), pszFile, pStrBuf->Access(), pStrBuf->Capacity() );
    pStrBuf->ReplaceFastCaseless( "$os", pszPlatform );
	V_FixSlashes( pStrBuf->Access() );
	V_RemoveDotSlashes( pStrBuf->Access() );
	V_FixDoubleSlashes( pStrBuf->Access() );

	if ( Sys_Exists( pStrBuf->Get() ) )
	{
        pStrBuf->Set( pszFile );
        pStrBuf->ReplaceFastCaseless( "$os", pszPlatform );
        pRetBuf->Set( pStrBuf->Get() );
		return pRetBuf->Get();
	}

	return NULL;
}

// No particular order, but must not change without changing the arrPlatformChains matrix
const char *g_szArrPlatforms[] = 
{
	// raw platforms...
	"win32",				//  0
	"win64",				//  1
	"osx32",				//  2
	"osx64",				//  3
	"linux64",				//  4
	"linuxserver64",		//  5
	"linuxsteamrtarm32hf",	//  6
	"linuxsteamrtarm64hf",	//  7
	"",						//  8
	"ps3",					//  9
	"x360",					// 10
	"androidarm32",			// 11
	"androidarm64",			// 12
	"androidmips32",		// 13
	"androidmips64",		// 14
	"androidx8632",			// 15
	"androidx8664",			// 16

	// fallbacks...
	"win",					// 17
	"osx",					// 18
	"linux",				// 19
	"posix",				// 20
	"any",					// 21
	NULL
};

//fallback indices to match the array above
enum FallBacks_t
{
	FB_WIN = 17,
	FB_OSX = 18,
	FB_LIN = 19,
	FB_POS = 20,
	FB_ANY = 21
};

//-----------------------------------------------------------------------------
//	ResolveFilename
//		Utility to expand $OS if present
//		Returns expanded filename to a file that exists on disk, or NULL otherwise
//		vecBonusFiles will contain a list of valid OS files that are on disk but did not
//		match the current target platform.
//-----------------------------------------------------------------------------
static const char *ResolveFilename( const char *pszFile, CUtlPathStringHolder *pRetBuf, CUtlVector<CUtlString> &vecBonusFiles )
{
	static const int k_lastRealPlatform = 16; // index, not count
	static const int k_AnyPlatform = 21; // index

	static const int arrPlatformChains[][k_lastRealPlatform + 1] =
	{
		{	0,		1,		2,		3,		4,		5,		6,		7,		8,		9,		10,		11,		12,		13,		14,		15,		16 }, // the raw platforms
		{ FB_WIN, FB_WIN, FB_OSX, FB_OSX, FB_LIN, FB_LIN, FB_LIN, FB_LIN, FB_ANY, FB_POS, FB_WIN, FB_LIN, FB_LIN, FB_LIN, FB_LIN, FB_LIN, FB_LIN }, // first fallback
		{ FB_ANY, FB_ANY, FB_POS, FB_POS, FB_POS, FB_POS, FB_POS, FB_POS, FB_ANY, FB_ANY, FB_ANY, FB_POS, FB_POS, FB_POS, FB_POS, FB_POS, FB_POS }, // 2nd fallback
		{ FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY, FB_ANY }, // 3rd fallback
	};

	// Don't want the actual backing store to be const since we aren't compile-time init'ing,
	// but it is single-instance initialized, so make a const ref to it that everyone except
	// the init codes uses to make sure it doesn't get mucked with.
	static bool bInited = false;
	static char _szPlatformStore[128];
	static const char (&szPlatform)[128] = _szPlatformStore;
	
	if ( !bInited )
	{
		bInited = true;
		const char *pszPlatform = g_pVPC->GetTargetPlatformName();
		V_strncpy( _szPlatformStore, pszPlatform, sizeof( szPlatform ) );
		V_strlower( _szPlatformStore );
	}

	vecBonusFiles.RemoveAll();

	int nPlatformColumn = -1;
	for ( int i = 0; i <= k_lastRealPlatform; i++ )
	{
		if ( g_szArrPlatforms[i][0] && V_strcmp( szPlatform, g_szArrPlatforms[i] ) == 0 )
		{
			nPlatformColumn = i;
			break;
		}
	}
	
	if ( nPlatformColumn < 0 )
	{
		g_pVPC->VPCWarning( "Internal Error: Target Platform: '%s' unrecognized while expanding $os!", szPlatform );
		return NULL;
	}

	// Now walk the chain of potential platform matches from strongest to weakest
	const char *pszResolved = NULL;
	int nPlatformToCheck = -1;
	int nCurrentPlatformRow = -1;
	do 
	{
		nCurrentPlatformRow++;
		nPlatformToCheck = arrPlatformChains[nCurrentPlatformRow][nPlatformColumn];
		pszResolved = ResolveCandidate( pszFile, g_szArrPlatforms[ nPlatformToCheck ], pRetBuf );
		if ( pszResolved )
			break;
	}
	while ( arrPlatformChains[nCurrentPlatformRow][nPlatformColumn] != k_AnyPlatform );

	// Now go pickup the any files that exist, but were non-matches
    CUtlPathStringHolder nonMatch;
	for ( int i = 0; g_szArrPlatforms[i] != NULL; i++ )
	{
		// Don't pick up the actual found platform
		if ( i != nPlatformToCheck )
		{
			if ( ResolveCandidate( pszFile, g_szArrPlatforms[ i ], &nonMatch ) )
				vecBonusFiles.AddToTail( nonMatch.Get() );
		}
	}
	return pszResolved;
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_AddFilesByPattern
//
//-----------------------------------------------------------------------------
void VPC_Keyword_AddFilesByPattern()
{
	CUtlVector<CUtlString>			files;

	if ( !g_pVPC->IsFilePatternEnabled() )
	{
		g_pVPC->VPCSyntaxError( "$FilePattern support not enabled" );
	}

	// THIS FEATURE IS NOT COMPLIANT TO VPC CONDITIONAL SYNTAX.
	while ( 1 )
	{
		const char *pToken = g_pVPC->GetScript().GetToken( false );
		if ( !pToken || !pToken[0] )
			break;

		// Is this a conditional expression?
		if ( pToken[0] == '[' )
		{
			const char *pNextToken = g_pVPC->GetScript().PeekNextToken( false );
			if ( pNextToken && pNextToken[0] == '[' )
			{
				g_pVPC->VPCSyntaxError( "Bad conditional syntax. Use C style boolean expression operators to express compound conditionals." );
			}

			if ( files.Count() == 0 )
			{
				g_pVPC->VPCSyntaxError( "Conditional specified on a $FilePattern without any pattern preceding it." );
			}

			if ( !g_pVPC->EvaluateConditionalExpression( pToken ) )
			{
				// we did all that work for no reason, time to bail out
				return;
			}
		}

        CUtlStringBuilder *pStrBuf = g_pVPC->GetMacroReplaceBuffer();
		g_pVPC->ResolveMacrosInString( pToken, pStrBuf );
		V_FixSlashes( pStrBuf->Access() );

		CUtlVector< CUtlString > vecResults;
		Sys_ExpandFilePattern( pStrBuf->Get(), vecResults );

		for ( int i=0; i < vecResults.Count(); i++ )
		{
			g_pVPC->VPCStatus( false, "glob: adding '%s' to project", vecResults[i].String() );
			g_pVPC->GetProjectGenerator()->StartFile( vecResults[i].String(), VPC_FILE_FLAGS_NONE, true ); 
			g_pVPC->GetProjectGenerator()->EndFile();
		}
	}
}

class CNoNameTranslation
{
public:
	void operator()( CUtlStringBuilder *pStrBuf ) {}
};

//-----------------------------------------------------------------------------
// Parse a list of filenames from the current token. Handles conditionals
// and line continuation (via '\' ). The presence of a line suffix conditional will
// cull ONLY that line's file.
//
// Valid syntax formats: (i.e. various combinations of conditionals and continuation)
//
// $PreQualifiedToken	"file_a" [$CONDITION]
//
// $PreQualifiedToken	"file_a" \
//						"file_b" [$CONDITION] \
//						"file_c" \
//						"file_d" [$CONDITION]
//
//-----------------------------------------------------------------------------
static void VPC_ParseFileList( CUtlVector< CUtlString > &files, void (*pFNNameTranslation)( CUtlStringBuilder *pStrBuf ) = nullptr )
{
	bool bFoundFilename = false;
	bool bAllowNextLine = false;
	while ( 1 )
	{
		const char *pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			g_pVPC->VPCSyntaxError();

		// enforcing per-line syntax (VPC's need to stay legible), and not some continuous-same-line cryptic compound sequence
		bAllowNextLine = false;
	
		if ( pToken && pToken[0] == '[' )
		{
			if ( !bFoundFilename )
			{
				g_pVPC->VPCSyntaxError( "Conditional '%s' specified on a file list without any file preceding it.", pToken );
			}

			bFoundFilename = false;

			// token is an optional conditional
			bool bResult = g_pVPC->EvaluateConditionalExpression( pToken );
			if ( !bResult )	
			{
				// conditional evaluated false
				// cull the just-added file from list
				files.Remove( files.Count() - 1 );
			}

			const char *pNextToken = g_pVPC->GetScript().PeekNextToken( false );
			if ( !pNextToken || !pNextToken[0] )
			{
				// end of tokens, finished
				break;
			}
			else if ( CharStrEq( pNextToken, '\\' ) )
			{
				// the only allowed token after the conditional is the continuation marker
				continue;
			}
			else
			{
				g_pVPC->VPCSyntaxError( "Unexpected token '%s' after the conditional '%s'", pNextToken, pToken );
			}
		}
	
		if ( CharStrEq( pToken, '\\' ) )
		{
			const char *pNextToken = g_pVPC->GetScript().PeekNextToken( false );
			if ( pNextToken && pNextToken[0] )
			{
				g_pVPC->VPCSyntaxError( "Unexpected token '%s' on same line after the file continuation marker", pNextToken );
			}

			// parse on next line
			bAllowNextLine = true;
			continue;
		}

        CUtlStringBuilder *pStrBuf = g_pVPC->GetMacroReplaceBuffer();
		g_pVPC->ResolveMacrosInString( pToken, pStrBuf );
		V_FixSlashes( pStrBuf->Access() );

		if ( pFNNameTranslation )
		{
			pFNNameTranslation( pStrBuf );
		}

		const char *pExtension = V_GetFileExtension( pStrBuf->Get() );
		if ( IsCFileExtension( pExtension ) || IsHFileExtension( pExtension ) )
		{
            char szFilename[MAX_FIXED_PATH];
			// Only need to fix c/h files as MSDEV is intolerant of these kinds of files with redundant pathing
			// fixup filename with redundant pathing
			Sys_ForceToMinimalRelativePath( g_pVPC->GetProjectPath(), pStrBuf->Get(), szFilename, sizeof( szFilename ) );
            pStrBuf->Set( szFilename );
		}

		bFoundFilename = true;
		files.AddToTail( pStrBuf->Get() );

		// check for another optional token
		const char *pNextToken = g_pVPC->GetScript().PeekNextToken( false );
		if ( !pNextToken || !pNextToken[0] )
			break;

		if ( pNextToken[0] == '[' || CharStrEq( pNextToken, '\\' ) )
			continue;

		g_pVPC->VPCSyntaxError( "Unexpected token '%s' after filename '%s'", pNextToken, pToken );
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_AddFile
//
//-----------------------------------------------------------------------------
void VPC_Keyword_AddFile( VpcFileFlags_t iFileFlags = VPC_FILE_FLAGS_NONE, void (*pFNNameTranslation)( CUtlStringBuilder *pStrBuf ) = nullptr )
{
	bool bAddedAsLibrary = (iFileFlags & (VPC_FILE_FLAGS_STATIC_LIB | VPC_FILE_FLAGS_IMPORT_LIB | VPC_FILE_FLAGS_SHARED_LIB)) != 0;

	CUtlVector< CUtlString > files;
	VPC_ParseFileList( files, pFNNameTranslation );

	if (!bAddedAsLibrary)
	{
		for (int i = 0; i < files.Count(); i++)
		{
			const char *pFilename = files[i].String();
			if (IsLibraryFile(pFilename))
			{
				iFileFlags = VPC_FILE_FLAGS_STATIC_LIB;
				bAddedAsLibrary = true;
				break;
			}
		}
	}

	// check for optional section
	bool bHasSection = false;
	const char *pToken = g_pVPC->GetScript().PeekNextToken( true );
	if ( pToken && pToken[0] && CharStrEq( pToken, '{' ) )
	{
		bHasSection = true;
	}

	//verify that libraries are tagged properly
	if ( bAddedAsLibrary )
	{
		bool bFailLibs = false;
		if ( g_pVPC->IsConditionalDefined( "COMPILING_LIB" ) && (iFileFlags & (VPC_FILE_FLAGS_STATIC_LIB | VPC_FILE_FLAGS_IMPORT_LIB)) )
		{
			bFailLibs = !g_pVPC->IsLibWithinLibEnabled();
		}

		for ( int i = 0; i < files.Count(); i++ )
		{
			const char *pFilename = files[i].String();

			// Linking one static library into another causes many problems. It leads to binary bloat (the
			// same code ends up checked in multiple times), it ends up causing duplicate symbol problems
			// that are hard to track down, it makes it hard to figure out which libraries a DLL uses,
			// and it is not supported on platforms other than Windows.
			if ( bFailLibs )
			{
				g_pVPC->VPCSyntaxError( "Using $Lib or $ImpLib in a static library project is prohibited (lib used '%s').\nYou can use $LibDependsOn[Imp]Lib if you need something similar, look at lib_depends_source.vpc.\n", pFilename );
			}

			VPC_AddLibraryDependencies( pFilename );
		}
	}
	else
	{
		for ( int i = 0; i < files.Count(); i++ )
		{
			const char *pFilename = files[i].String();
			if ( IsLibraryFile( pFilename ) )
			{
				g_pVPC->VPCError( "Cannot add library \"%s\" using $File tag. Use $Lib, $ImpLib, or $SharedLib to specify how the library is used\n", pFilename );
			}
		}
	}

	// Check files is a guard requested to provide an early failure point as programmers were getting tagged later by compilation failures,
	// or worse, run-time latent bugs, that could have been prevented at the moment of their sync/vpc.
	// Check file existence early to handle a possible rejected section (NOTE: ignore dynamic files)
	bool bDynamicFile = !!( iFileFlags & VPC_FILE_FLAGS_DYNAMIC );
	if ( g_pVPC->IsCheckFiles() && !bDynamicFile )
	{
		for ( int i = 0; i < files.Count(); i++ )
		{
			const char *pFilename = files[i].String();
			const char *pExtension = V_GetFileExtensionSafe( pFilename );

			// Need to isolate files that have build steps and ignore script or other no-op injected project files.
			bool bIsRelevantFile = IsSourceFile( pFilename ) || g_pVPC->m_CustomBuildSteps.HasElement( pExtension );
			if ( bIsRelevantFile && !Sys_Exists( pFilename ) && !V_stristr( pFilename, "$os" ) )
			{
				g_pVPC->VPCWarning( "File '%s' does not exist for project '%s'. (Use $DynamicFile if it is created during the build process.)", pFilename, g_pVPC->GetProjectName() );
				g_pVPC->IncrementFileMissing();

				// need script stack to assist in tracking down missing file
				g_pVPC->GetScript().SpewScriptStack( false );

				files.Remove( i );
			}
		}
	}

	if ( g_pVPC->IsShowCaseIssues() && !bDynamicFile )
	{
		for ( int i = 0; i < files.Count(); i++ )
		{
			const char *pFilename = files[i].String();
			char actualFilename[MAX_FIXED_PATH];
			if ( !Sys_IsFilenameCaseConsistent( pFilename, actualFilename, sizeof( actualFilename ) ) )
			{
				g_pVPC->VPCWarning( "Case Consistency Issue! File '%s' specified in '%s' is inconsistent with OS version '%s'.", pFilename, g_pVPC->GetProjectName(), actualFilename );

				// need script stack to assist in tracking down missing file
				g_pVPC->GetScript().SpewScriptStack( true );
			}
		}
	}
	
	if ( !files.Count() && bHasSection )
	{
		// optional section has been conditionally removed
		g_pVPC->GetScript().SkipBracedSection();
		return;
	}

	if ( bHasSection )
	{
		// found optional section, parse opening brace
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] || !CharStrEq( pToken, '{' ) )
			g_pVPC->VPCSyntaxError();
	}

	// Handle $OS expansion
	// save parser state
	CScriptSource startingScriptSource = g_pVPC->GetScript().GetCurrentScript();

	CUtlVector< CUtlString > customFiles;
	for ( int i = 0; i < files.Count(); i++ )
	{
		const char *pFilename = files[i].String();
        CUtlPathStringHolder macroFilename;

		if ( g_pVPC->IsOSMacroEnabled() )
		{
			if ( !g_pVPC->m_bIsDependencyPass && V_stristr( pFilename, "$os" ) )
			{
				CUtlVector<	CUtlString > vecExcludedFiles;
				const char *pResolvedFilename = ResolveFilename( pFilename, &macroFilename, vecExcludedFiles );

				char rgchRejectList[4096]; 
				rgchRejectList[0] = '\0';

				if ( vecExcludedFiles.Count() )
				{
					for ( int j = 0; j < files.Count(); j++ )
					{
						V_strncat( rgchRejectList, files[j].String(), V_ARRAYSIZE( rgchRejectList ) );
						V_strncat( rgchRejectList, ",", V_ARRAYSIZE( rgchRejectList ) );
					}
				}
				g_pVPC->VPCStatus( false, "$OS: Resolved %s -> %s, rejected %s", pFilename, pResolvedFilename ? pResolvedFilename : "<nothing>", rgchRejectList );

				if ( !pResolvedFilename )
				{
					g_pVPC->VPCWarning( "$File %s did not resolve to an existing file, skipping!", pFilename );
					continue;
				}

				for ( i = 0; i < vecExcludedFiles.Count(); i++ )
				{
					const char *pExcludedFilename = vecExcludedFiles[i].String();
					const char *pExcludedExtension = V_GetFileExtension( pExcludedFilename );
					if ( !pExcludedExtension )
					{
						pExcludedExtension = "";
					}
					if ( pExcludedExtension && !V_stricmp_fast( pExcludedExtension, "cpp" ) )
					{
						g_pVPC->VPCStatus( false, "Excluding '%s' from build", pExcludedFilename );
						g_pVPC->GetProjectGenerator()->StartFile( pExcludedFilename, iFileFlags, true ); 
						CUtlVector< CUtlString > configurationNames;
 						g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
						for ( int j = 0; j < configurationNames.Count(); j++ )
						{
							g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[j].String(), true );
							g_pVPC->GetProjectGenerator()->FileExcludedFromBuild( true );
							g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
						}
						g_pVPC->GetProjectGenerator()->EndFile();
					}
				}

				pFilename = pResolvedFilename;
			}
		}

		bool bAdded = g_pVPC->GetProjectGenerator()->StartFile( pFilename, iFileFlags, !bAddedAsLibrary );
		
		// Lookup extension for a custom build script
		const char *pExtension = V_GetFileExtensionSafe( pFilename );
	
		int nBuildStepIndex = g_pVPC->m_CustomBuildSteps.Find( pExtension );
		if ( g_pVPC->m_CustomBuildSteps.IsValidIndex( nBuildStepIndex ) )
		{
			CVPC::CustomBuildStepForExtension_t &buildStepsForExtension = g_pVPC->m_CustomBuildSteps[nBuildStepIndex];
			const char *pBuffer = buildStepsForExtension.m_BuildSteps.Get();
			
			// save parser state
			g_pVPC->GetScript().PushScript( buildStepsForExtension.m_DefinedInFile.Get(), pBuffer, buildStepsForExtension.m_nDefinitionStartLine, false, true );

			// parse injected buildstep
			VPC_ParseFileSection();

			// restore parser state
			g_pVPC->GetScript().PopScript();
			g_pVPC->GetScript().RestoreScript( startingScriptSource );
		}

		const char *pCleanFilename = g_pVPC->GetProjectGenerator()->GetCurrentFileName();

        // Apply PCH expansions.
        if ( ( iFileFlags & ( VPC_FILE_FLAGS_CREATE_PCH |
                              VPC_FILE_FLAGS_NO_PCH ) ) != 0 )
        {
            const char *pScriptName = ( iFileFlags & VPC_FILE_FLAGS_CREATE_PCH ) != 0 ? "$CreatePCH" : "$NoPCH";
			int nScriptIndex = g_pVPC->m_CustomAutoScripts.Find( pScriptName );
			if ( g_pVPC->m_CustomAutoScripts.IsValidIndex( nScriptIndex ) )
			{
				g_pVPC->SetSystemMacro( "AUTO_SCRIPT_FILENAME", pCleanFilename, false );
				g_pVPC->SetSystemMacro( "AUTO_SCRIPT_BASEFILENAME", V_UnqualifiedFileName( pCleanFilename ), false );

				CUtlString scriptName;
				scriptName.Format( "Custom Auto Script for '%s'", pScriptName );

				g_pVPC->GetScript().PushScript( scriptName.Get(),  g_pVPC->m_CustomAutoScripts[nScriptIndex].Get(), 1, false, false );
                // We're going to use a per-file compiler option so allow that.
                // This will get reverted on RestoreScript.
                g_pVPC->GetScript().EnterPrivilegedScript();

				// parse injected steps
				VPC_ParseFileSection();

				// restore parser state
				g_pVPC->GetScript().PopScript();
				g_pVPC->GetScript().RestoreScript( startingScriptSource );
			}
        }

		// apply optional section to each file
		if ( bHasSection && bAdded )
		{
			// restore parser state
			g_pVPC->GetScript().RestoreScript( startingScriptSource );
			VPC_ParseFileSection();
		}

		if ( bAdded )
		{
			int nAutoScriptIndex = g_pVPC->m_CustomAutoScripts.Find( pExtension );
			if ( g_pVPC->m_CustomAutoScripts.IsValidIndex( nAutoScriptIndex ) )
			{
				// track each custom file that has a custom auto script
				customFiles.AddToTail( pCleanFilename );
			}
		}

		VPC_Schema_TrackFile( pCleanFilename, false, iFileFlags );
		VPC_Qt_TrackFile( pCleanFilename, false, iFileFlags );

		if ( bAdded )
		{
			g_pVPC->GetProjectGenerator()->EndFile();
		}
	}

	if ( customFiles.Count() )
	{
		// save parser state
		startingScriptSource = g_pVPC->GetScript().GetCurrentScript();

		// emit the auto scripts
		for ( int i = 0 ; i < customFiles.Count(); i++ )
		{
			const char *pExtension = V_GetFileExtensionSafe( customFiles[i].Get() );
			int nAutoScriptIndex = g_pVPC->m_CustomAutoScripts.Find( pExtension );
			if ( g_pVPC->m_CustomAutoScripts.IsValidIndex( nAutoScriptIndex ) )
			{
				g_pVPC->SetSystemMacro( "AUTO_SCRIPT_FILENAME", customFiles[i].Get(), false );
				g_pVPC->SetSystemMacro( "AUTO_SCRIPT_BASEFILENAME", customFiles[i].GetBaseFilename().Get(), false );

				CUtlString scriptName;
				scriptName.Format( "Custom Auto Script for '%s'", pExtension );

				// save parser state
				g_pVPC->GetScript().PushScript( scriptName.Get(),  g_pVPC->m_CustomAutoScripts[nAutoScriptIndex].Get(), 1, false, false );

				// parse injected steps
				VPC_HandleProjectCommands( NULL, 1, false );

				// restore parser state
				g_pVPC->GetScript().PopScript();
				g_pVPC->GetScript().RestoreScript( startingScriptSource );
			}
		}

		g_pVPC->SetSystemMacro( "AUTO_SCRIPT_FILENAME", "", false );
		g_pVPC->SetSystemMacro( "AUTO_SCRIPT_BASEFILENAME", "", false );
	}
}

static const char *VPC_ExpandLibraryName( const char *pName, const char *pDefaultPath, char const *pFileNamePrefix, char const *pSuffix, CUtlPathStringHolder *pFullName )
{
	if ( !pFileNamePrefix )
	{
		pFileNamePrefix = "";
	}

    // do not add the path prefix if the filename contains path information already, or if null was passed.
    CUtlStringBuilder *pFixedName = g_pVPC->GetTempStringBuffer2();
    pFixedName->Set( pName );
    V_RemoveDotSlashes( pFixedName->Access() );
		
    CUtlStringBuilder *pFormatBuf = g_pVPC->GetTempStringBuffer1();
    char *pLastSlash = (char*)MAX( strrchr( pFixedName->Access(), '\\' ), strrchr( pFixedName->Access(), '/' ) );
    if ( pLastSlash )
    {
        *pLastSlash = 0;
        const char *pFilenamePart = pLastSlash + 1;
        pFormatBuf->Format( "%s/%s%s%s", pFixedName->Get(), pFileNamePrefix, pFilenamePart, pSuffix );
    }
    else
    {
        pFormatBuf->Format( "%s%s%s%s", pDefaultPath, pFileNamePrefix, pName, pSuffix );
    }

    CUtlStringBuilder *pStrBuf = g_pVPC->GetMacroReplaceBuffer();
    g_pVPC->ResolveMacrosInString( pFormatBuf->Get(), pStrBuf );

    pFullName->Set( pStrBuf->Get() );
    pFullName->FixSlashesAndDotSlashes();
    return pFullName->Get();
}

static void nameTransformImpLib( CUtlStringBuilder *pStrBuf)
{
	CUtlPathStringHolder szFilename1;
	VPC_ExpandLibraryName( pStrBuf->Get(), "$LIBPUBLIC\\", "$_IMPLIB_PREFIX", "$_IMPLIB_EXT", &szFilename1 );
	pStrBuf->Set( szFilename1.Get() );
}

static void nameTransformImpLibExternal( CUtlStringBuilder *pStrBuf)
{
	CUtlPathStringHolder szFilename1;
	VPC_ExpandLibraryName( pStrBuf->Get(), "$LIBPUBLIC\\", "$_IMPLIB_PREFIX", "$_EXTERNAL_IMPLIB_EXT", &szFilename1 );
	pStrBuf->Set( szFilename1.Get() );
}

static void VPC_Keyword_ImportLibrary( bool bRemove = false, bool bExternal = false )
{
	auto nameTransformFunction = bExternal ? nameTransformImpLibExternal : nameTransformImpLib;

	if ( !bRemove )
	{
		VPC_Keyword_AddFile( VPC_FILE_FLAGS_IMPORT_LIB, nameTransformFunction );
	}
	else
	{
		VPC_Keyword_RemoveFile( nameTransformFunction );
	}
}

static void nameTransformLinkLib( CUtlStringBuilder *pStrBuf )
{
	CUtlPathStringHolder szFilename1;
	VPC_ExpandLibraryName( pStrBuf->Get(), "$LIBPUBLIC\\", NULL, "$_STATICLIB_EXT", &szFilename1 );
	pStrBuf->Set( szFilename1.Get() );
}

static void nameTransformLinkLibExternal( CUtlStringBuilder *pStrBuf )
{
	CUtlPathStringHolder szFilename1;
	VPC_ExpandLibraryName( pStrBuf->Get(), "$LIBPUBLIC\\", NULL, "$_EXTERNAL_STATICLIB_EXT", &szFilename1 );
	pStrBuf->Set( szFilename1.Get() );
}

static void VPC_Keyword_LinkerLibrary( bool bRemove = false, bool bExternal = false )
{
	auto nameTransformFunction = bExternal ? nameTransformLinkLibExternal : nameTransformLinkLib;

	if ( !bRemove )
	{
		VPC_Keyword_AddFile( VPC_FILE_FLAGS_STATIC_LIB, nameTransformFunction );
	}
	else
	{
		VPC_Keyword_RemoveFile( nameTransformFunction );
	}
}

static void VPC_Keyword_SharedLibrary( bool bRemove = false )
{
	auto nameTransformFunction = []( CUtlStringBuilder *pStrBuf ) -> void
		{
			CUtlPathStringHolder szFilename1;
			VPC_ExpandLibraryName( pStrBuf->Get(), "$LIBPUBLIC\\", "$_DLL_PREFIX", "$_DLL_EXT", &szFilename1 );
			pStrBuf->Set( szFilename1.Get() );
		};

	if ( !bRemove )
	{
		VPC_Keyword_AddFile( VPC_FILE_FLAGS_SHARED_LIB, nameTransformFunction );
	}
	else
	{
		VPC_Keyword_RemoveFile( nameTransformFunction );
	}
}

static void VPC_AddLibraryDependencies( const char *pLibPath )
{
    int i = g_pVPC->m_LibraryDependencies.Find( pLibPath );
    if ( i == g_pVPC->m_LibraryDependencies.InvalidIndex() )
    {
        return;
    }

    CUtlVector< CUtlString > &dependencies = g_pVPC->m_LibraryDependencies[i];
    for ( i = 0; i < dependencies.Count(); i++ )
    {
        const char *pDependency = dependencies[i].Get();
        if ( !g_pVPC->GetProjectGenerator()->HasFile( pDependency ) )
        {
            if ( !g_pVPC->m_bIsDependencyPass )
            {
                g_pVPC->VPCStatus( false, "$LibDependency: '%s' added for '%s'",
                                   pDependency, pLibPath );
            }

            bool bAdded = g_pVPC->GetProjectGenerator()->StartFile( pDependency, VPC_FILE_FLAGS_STATIC_LIB, true );
            if ( !bAdded )
            {
                g_pVPC->VPCError( "couldn't add %s", pDependency );
            }
            g_pVPC->GetProjectGenerator()->EndFile();

            VPC_AddLibraryDependencies( pDependency );
        }
    }
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_$LibDepends
//-----------------------------------------------------------------------------

static void VPC_LibDepends( char const *pDefaultPath, char const *pFileNamePrefix, char const *pSuffix )
{
    const char *pToken = g_pVPC->GetScript().GetToken( false );
    if ( !pToken || !pToken[0] )
        g_pVPC->VPCSyntaxError();

    // The lib is always a static lib.
    CUtlPathStringHolder libName;
    VPC_ExpandLibraryName( pToken, "$LIBPUBLIC\\", NULL, "$_STATICLIB_EXT", &libName );
    
    int i = g_pVPC->m_LibraryDependencies.Find( libName );
    if ( i == g_pVPC->m_LibraryDependencies.InvalidIndex() )
    {
        i = g_pVPC->m_LibraryDependencies.Insert( libName );
    }
    
    CUtlVector< CUtlString > &dependencies = g_pVPC->m_LibraryDependencies[i];

	CUtlVector< CUtlString > dependents;
	VPC_ParseFileList( dependents );
    if ( !dependents.Count() )
    {
        return;
    }
    
    for ( i = 0; i < dependents.Count(); i++ )
    {
        CUtlPathStringHolder depName;
        VPC_ExpandLibraryName( dependents[i], pDefaultPath, pFileNamePrefix, pSuffix, &depName );

        int j;
        for ( j = 0; j < dependencies.Count(); j++ )
        {
            if ( !V_stricmp_fast( depName, dependencies[j] ) )
            {
                break;
            }
        }
        if ( j >= dependencies.Count() )
        {
            dependencies.AddToTail( depName.Get() );
        }
    }
}

void VPC_Keyword_LibDependsOnLib()
{
	VPC_LibDepends( "$LIBPUBLIC\\", NULL, "$_STATICLIB_EXT" );
}

void VPC_Keyword_LibDependsOnImpLib()
{
	VPC_LibDepends( "$LIBPUBLIC\\", "$_IMPLIB_PREFIX", "$_IMPLIB_EXT" );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_RemoveFile
//
//-----------------------------------------------------------------------------
void VPC_Keyword_RemoveFile( void (*pFNNameTranslation)( CUtlStringBuilder *pStrBuf ) /*= nullptr*/ )
{
	CUtlVector< CUtlString > filesToRemove;
	VPC_ParseFileList( filesToRemove, pFNNameTranslation );
	for ( int i = 0; i < filesToRemove.Count(); i++ )
	{
		bool bSucc = g_pVPC->GetProjectGenerator()->RemoveFile( filesToRemove[i].Get() );
		if ( !bSucc )
		{
			// VPC_HandleLibraryExpansion() remove ./s so if we initially failed at
			// killing this file, remove the ./ and retry.
			CUtlString modifiedString = filesToRemove[i].Get();
			V_RemoveDotSlashes( modifiedString.GetForModify() );
			bSucc = g_pVPC->GetProjectGenerator()->RemoveFile( modifiedString.Get() );
			if ( bSucc )
			{
				filesToRemove[i] = modifiedString;
			}
		}

		if ( !bSucc )
		{
			g_pVPC->VPCWarning( "Failed to remove file \"%s\" from project \"%s\".",
				filesToRemove[i].Get(), g_pVPC->GetProjectName() );
		}

		VPC_Schema_TrackFile( filesToRemove[i].Get(), true, VPC_FILE_FLAGS_NONE );
		VPC_Qt_TrackFile(     filesToRemove[i].Get(), true, VPC_FILE_FLAGS_NONE );
	}
}

//-----------------------------------------------------------------------------
//	VPC_Write_Shader_Folder
//
//-----------------------------------------------------------------------------
void VPC_Write_Shader_Folder( CUtlBuffer &vpcBuffer, const CUtlVector< CUtlString > &files, const char *pFolderName )
{
	if ( files.Count() )
	{
		vpcBuffer.Printf( "$Folder \"%s\" \n", pFolderName );
		vpcBuffer.Printf( "{\n" );
		for ( int i=0; i<files.Count(); i++ )
		{
			vpcBuffer.Printf( "$file \"%s\"\n", files[i].String() );
		}
		vpcBuffer.Printf( "}\n" );
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_Shaders
//
//-----------------------------------------------------------------------------
void VPC_Keyword_Shaders( int depth )
{
	// TODO: turn this into a 'feature' ala qt/schema

	const char						*pToken;
	CUtlBuffer					vpcBuffer;
	CUtlVector< CUtlString >	fxcList;
	CUtlVector< CUtlString >	vshList;
	CUtlVector< CUtlString >	pshList;
	CUtlVector< CUtlString >	vfxList;
	CUtlVector< CUtlString >	otherList;
	bool						bIgnoreRedundancyWarning;

    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
	{
		return;		
	}

    CUtlStringHolder<100> shadersName( pStrBuf->Get() );
	g_pVPC->VPCStatus( false, "Parsing: %s", shadersName.Get() );
	g_pVPC->GetScript().PushScript( shadersName, true );

	// parse the shader list file into types (fxc,vsh,psh)
	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			// end of file
			break;
		}

		// Make token relative to location of script file by prepending the relative path of the script to the token
		CUtlPathStringHolder relativePath( shadersName );
		relativePath.StripLastDir();
        relativePath.Append( pToken );

		if ( V_stristr( relativePath.Get(), ".fxc" ) )
		{
			fxcList.AddToTail( relativePath.Get() );
		}
		else if ( V_stristr( relativePath.Get(), ".vsh" ) )
		{
			vshList.AddToTail( relativePath.Get() );
		}
		else if ( V_stristr( relativePath.Get(), ".psh" ) )
		{
			pshList.AddToTail( relativePath.Get() );
		}
		else if ( V_stristr( relativePath.Get(), ".vfx" ) )
		{
			vfxList.AddToTail( relativePath.Get() );
		}
		else
		{
			otherList.AddToTail( relativePath.Get() );
		}
	}

	g_pVPC->GetScript().PopScript();

	if ( !fxcList.Count() && 
		!vshList.Count() && 
		!pshList.Count() &&
		!vfxList.Count() &&
		!otherList.Count() )
	{
		//g_pVPC->VPCWarning( "No shaders found in %s", shadersName );
		return;
	}

	// generate a vpc compatible file to generate the shader file hierarchy
	vpcBuffer.SetBufferType( true, true );
	vpcBuffer.Printf( "$Folder \"Shader Source\" \n" );
	vpcBuffer.Printf( "{\n" );
	{
		// make one subfolder for each type of file:
		VPC_Write_Shader_Folder( vpcBuffer, fxcList,   "fxc"   );
		VPC_Write_Shader_Folder( vpcBuffer, vshList,   "vsh"   );
		VPC_Write_Shader_Folder( vpcBuffer, pshList,   "psh"   );
		VPC_Write_Shader_Folder( vpcBuffer, vfxList,   "vfx"   );
		VPC_Write_Shader_Folder( vpcBuffer, otherList, "other" );

		// add the shaders file as a convienence
		vpcBuffer.Printf( "$file \"%s\"\n", shadersName.Get() );
	}
	vpcBuffer.Printf( "}\n" );

	// save parser
	bIgnoreRedundancyWarning = g_pVPC->IsIgnoreRedundancyWarning();
	g_pVPC->SetIgnoreRedundancyWarning( true );

	g_pVPC->GetScript().PushScript( "Internal List", (char*)vpcBuffer.Base(), 1, false, false );

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( pToken && pToken[0] && !V_stricmp_fast( pToken, "$folder" ) )
	{
		VPC_Keyword_Folder( VPC_FOLDER_FLAGS_DYNAMIC );
	}

	// restore parser
	g_pVPC->GetScript().PopScript();
	g_pVPC->SetIgnoreRedundancyWarning( bIgnoreRedundancyWarning );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_Folder
//
//-----------------------------------------------------------------------------
void VPC_Keyword_Folder( VpcFolderFlags_t iFolderFlags )
{
	const char *pToken;

    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
	{
		g_pVPC->GetScript().SkipBracedSection();
		return;		
	}

	g_pVPC->GetProjectGenerator()->StartFolder( pStrBuf->Get(), iFolderFlags );

	// Now parse all the files and subfolders..
	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || !CharStrEq( pToken, '{' ) )
		g_pVPC->VPCSyntaxError();

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
			break;

		if ( CharStrEq( pToken, '}' ) )
		{
			// pop
			break;
		}
		else if ( !V_stricmp_fast( pToken, "$file" ) )
		{
			// add file
			VPC_Keyword_AddFile();
		}
		else if ( !V_stricmp_fast( pToken, "$File_CreatePCH" ) )
		{
			// add file
			VPC_Keyword_AddFile( VPC_FILE_FLAGS_CREATE_PCH );
		}
		else if ( !V_stricmp_fast( pToken, "$File_NoPCH" ) )
		{
			// add file
			VPC_Keyword_AddFile( VPC_FILE_FLAGS_NO_PCH );
		}
		else if ( !V_stricmp_fast( pToken, "$DynamicFile" ) )
		{
			// add file
			VPC_Keyword_AddFile( VPC_FILE_FLAGS_DYNAMIC );
		}
		else if ( !V_stricmp_fast( pToken, "$DynamicFile_NoPCH" ) )
		{
			// add file
			VPC_Keyword_AddFile( (VpcFileFlags_t)( VPC_FILE_FLAGS_DYNAMIC | VPC_FILE_FLAGS_NO_PCH ) );
		}
		else if ( !V_stricmp_fast( pToken, "$FilePattern" ) )
		{
			// glob the given pattern, add all files
			VPC_Keyword_AddFilesByPattern();
		}
		else if ( !V_stricmp_fast( pToken, "$qtfile" ) )
		{
			// add file
			VPC_Keyword_AddFile( VPC_FILE_FLAGS_QT );
		}
		else if ( !V_stricmp_fast( pToken, "$qtschemafile" ) )
		{
			// add file
			VPC_Keyword_AddFile( (VpcFileFlags_t)( VPC_FILE_FLAGS_QT | VPC_FILE_FLAGS_SCHEMA ) );
		}
		else if ( !V_stricmp_fast( pToken, "$schemafile" ) )
		{
			// add file
			VPC_Keyword_AddFile( VPC_FILE_FLAGS_SCHEMA );
		}
		else if ( !V_stricmp_fast( pToken, "$SchemaIncludeFile" ) )
		{
			// add file
			VPC_Keyword_AddFile( VPC_FILE_FLAGS_SCHEMA_INCLUDE );
		}
		else if ( !V_stricmp_fast( pToken, "$implib" ) )
		{
			// add file
			VPC_Keyword_ImportLibrary();
		}
		else if (!V_stricmp_fast(pToken, "$implibexternal"))
		{
			// add file
			VPC_Keyword_ImportLibrary(false, true);
		}
		else if ( !V_stricmp_fast( pToken, "-$implib" ) )
		{
			// remove file
			VPC_Keyword_ImportLibrary( true );
		}
		else if ( !V_stricmp_fast( pToken, "$libexternal" ) )
		{
			// add file
			VPC_Keyword_LinkerLibrary(false, true);
		}
		else if ( !V_stricmp_fast( pToken, "$lib" ) )
		{
			// add file
			VPC_Keyword_LinkerLibrary();
		}
		else if ( !V_stricmp_fast( pToken, "-$lib" ) )
		{
			// remove file
			VPC_Keyword_LinkerLibrary( true );
		}
		else if ( !V_stricmp_fast( pToken, "$SharedLib" ) )
		{
			// add file
			VPC_Keyword_SharedLibrary();
		}
		else if ( !V_stricmp_fast( pToken, "-$SharedLib" ) )
		{
			// remove file
			VPC_Keyword_SharedLibrary( true );
		}
		else if ( !V_stricmp_fast( pToken, "-$file" ) )
		{
			// remove file
			VPC_Keyword_RemoveFile();
		}
		else if ( !V_stricmp_fast( pToken, "$Shaders" ) )
		{
			// add contained shaders folder
			VPC_Keyword_Shaders( 0 );
		}
		else if ( !V_stricmp_fast( pToken, "$folder" ) )
		{
			// descend into subdirectory
			VPC_Keyword_Folder();
		}
		else if ( !V_stricmp_fast( pToken, "$unityfolder" ) )
		{
			// descend into subdirectory (and build unity files)
			VPC_Keyword_Folder( VPC_FOLDER_FLAGS_UNITY );
		}
		else
		{
			g_pVPC->VPCSyntaxError("Unrecognized token: %s", pToken);
		}
	}

	g_pVPC->GetProjectGenerator()->EndFolder();
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_Macro
//
//-----------------------------------------------------------------------------
enum MacroType_t { VPC_MACRO_VALUE, VPC_MACRO_EMPTY_STRING };
void VPC_Keyword_Macro( MacroType_t eMacroType )
{
	const char	*pToken;

	pToken = g_pVPC->GetScript().GetToken( false );
	if ( !pToken || !pToken[0] )
		g_pVPC->VPCSyntaxError();

    CUtlStringHolder<MAX_MACRO_NAME> macroName( pToken );

    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
	{
		return;
	}

    CUtlStringHolder<100> value( pStrBuf->Get() );
    
	const char *pValue = Sys_EvaluateEnvironmentExpression( value, "" );
    if ( !pValue )
	{
        pValue = value;
	}

	g_pVPC->SetScriptMacro( macroName, ( eMacroType == VPC_MACRO_VALUE ) ? pValue : "" );
}

//-----------------------------------------------------------------------------
//	$MacroRequired <Macro> [DefaultValue] [Condition]
//	$MacroRequiredAllowEmpty <Macro> [DefaultValue] [Condition]
//
//	Forces a script to error if a macro that it depends on was not set.
//	The Default will be used if the macro was not defined, otherwise error.
//	This is to allow a required macro in a base script to have a concept
//	of a default initialization value.
//-----------------------------------------------------------------------------
enum MacroRequiredType_t { VPC_MACRO_REQUIRED_NOT_EMPTY, VPC_MACRO_REQUIRED_ALLOW_EMPTY };
void VPC_Keyword_MacroRequired( MacroRequiredType_t eMacroRequiredType )
{
	const char	*pToken;

    CUtlStringHolder<100> macroDefaultValue;
	
	pToken = g_pVPC->GetScript().GetToken( false );
	if ( !pToken || !pToken[0] )
	{
		g_pVPC->VPCSyntaxError();
	}
    CUtlStringHolder<MAX_MACRO_NAME> macroName( pToken );

	// optional default macro value or conditional
	pToken = g_pVPC->GetScript().PeekNextToken( false );
	if ( pToken && pToken[0] )
	{
		if ( pToken[0] == '[' )
		{
			pToken = g_pVPC->GetScript().GetToken( false );
			// evaluate argument as conditional
			if ( !g_pVPC->EvaluateConditionalExpression( pToken ) )
			{
				return;
			}
		}
		else
		{
			// argument is a default macro value
            CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
			{
				return;
			}

            macroDefaultValue.Set( pStrBuf->Get() );
		}
	}

	// find macro
	CMacro *pMacro = g_pVPC->FindMacro( macroName );
	if ( pMacro && pMacro->IsPropertyMacro() )
	{
		// property macros which are constrained to be within a configuration are not interchangeable with normal macros
		g_pVPC->VPCError( "Macro '%s' already defined as a property macro.", macroName.Get() );
	}

	if ( !pMacro || ( eMacroRequiredType == VPC_MACRO_REQUIRED_NOT_EMPTY && !pMacro->HasValue() ) )
	{
		if ( macroDefaultValue[0] || ( eMacroRequiredType == VPC_MACRO_REQUIRED_ALLOW_EMPTY ) )
		{
			g_pVPC->SetScriptMacro( macroName, macroDefaultValue );
		}
		else
		{
			// In case we're in mksln showing a pacifier of dots. Make sure to show the error on a new line.
			g_pVPC->VPCSyntaxError( "\n\nRequired Macro '%s', not defined or empty", macroName.Get() );
		}
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_LoadAddressMacro
//
//	$LoadAddressMacro	<MacroName>
//	{
//		<ProjectName>	<BaseAddress>
//	}
//
//	Specialized instruction to populate the load address macro based on a project
//	name.
//-----------------------------------------------------------------------------
void VPC_Keyword_LoadAddressMacro( void )
{
	const char	*pToken;

    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
	{
		g_pVPC->GetScript().SkipBracedSection();
		return;		
	}

    CUtlStringHolder<50> szMacroName( pStrBuf->Get() );
    
	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || !CharStrEq( pToken, '{' ) )
	{
		g_pVPC->VPCSyntaxError();
	}
	
    CUtlStringHolder<100> szProjectName;

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			break;
		}

		if ( CharStrEq( pToken, '}' ) )
		{
			break;
		}
		else
		{
            szProjectName.Set( pToken );

			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
			{
				continue;
			}
		
			if ( !V_stricmp_fast( szProjectName, g_pVPC->GetLoadAddressName() ) )
			{
				// set Macro
				g_pVPC->SetScriptMacro( szMacroName, pStrBuf->Get() );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_LoadAddressMacroAlias
//
//	$LoadAddressMacroAlias	<Alias>
//	{
//		<ProjectName>
//	}
//
//	When evaluating $LoadAddressMacro/$LoadAddressMacroAuto, substitute all listed <ProjectName> entries with <Alias>
//-----------------------------------------------------------------------------
void VPC_Keyword_LoadAddressMacroAlias( void )
{
	const char	*pToken;

    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
	{
		g_pVPC->GetScript().SkipBracedSection();
		return;		
	}

    CUtlStringHolder<50> szAlias( pStrBuf->Get() );
    
	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || !CharStrEq( pToken, '{' ) )
	{
		g_pVPC->VPCSyntaxError();
	}

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			break;
		}

		if ( CharStrEq( pToken, '}' ) )
		{
			break;
		}
		else
		{
			if ( !V_stricmp_fast( pToken, g_pVPC->GetProjectName() ) )
			{
				// set Macro and alias
				g_pVPC->SetScriptMacro( "LOADADDRESSNAME", szAlias );
				g_pVPC->SetLoadAddressName( szAlias );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	Internal_LoadAddressMacroAuto
//
//	bPad - Differentiate between $LoadAddressMacroAuto and $LoadAddressMacroAuto_Padded
//			implementations
//
//	Specialized instruction to populate the load address macro based on a project
//	name.
//-----------------------------------------------------------------------------
void Internal_LoadAddressMacroAuto( bool bPad )
{
	const char	*pToken;

	pToken = g_pVPC->GetScript().GetToken( false );
	if ( !pToken || !pToken[0] )
	{
		g_pVPC->VPCSyntaxError();
	}
    CUtlStringHolder<MAX_MACRO_NAME> szMacroName( pToken );

    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
	{
		g_pVPC->GetScript().SkipBracedSection();
		return;		
	}
	uint64 baseAddress = 0;
	sscanf( pStrBuf->Get(), "%llx", &baseAddress );
	uint64 iInitialBaseAddress = baseAddress;

	int iSetEntryNum = 0;
	uint64 iSetBaseAddress = 0;

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || !CharStrEq( pToken, '{' ) )
	{
		g_pVPC->VPCSyntaxError();
	}
	
	int iEntryNum = 0;
	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			break;
		}

		if ( !V_stricmp_fast( pToken, g_pVPC->GetLoadAddressName() ) )
		{
			// set Macro
			char szMacroValue[100];
			sprintf( szMacroValue, "0x%8.8llx", baseAddress );
			
			iSetEntryNum = iEntryNum;
			iSetBaseAddress = baseAddress;
			g_pVPC->SetScriptMacro( szMacroName, szMacroValue );
		}

		if ( CharStrEq( pToken, '}' ) )
		{
			break;
		}
		else
		{
			unsigned int dllLength = 0;
			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
			{
				continue;
			}
			if ( strstr( pStrBuf->Get(), "." ) )
			{
				// assume float format
				float fLength = 0;
				sscanf( pStrBuf->Get(), "%f", &fLength );
				dllLength = fLength * 1024.0f * 1024.0f;
			}
			else
			{
				sscanf( pStrBuf->Get(), "%d", &dllLength );
			}

			if ( !bPad )
			{
				dllLength = AlignValue( dllLength, 64 * 1024 ); //will align later when we actually set the darn thing
			}

			if ( dllLength == 0 )
			{
				g_pVPC->VPCSyntaxError( "$LoadAddressMacroAuto no longer supports 0 size dlls. Use $LoadAddressMacroAlias to have two orthogonal projects load in the same space" );
			}

			baseAddress += dllLength;
		}

		++iEntryNum;
	}

	if ( bPad )
	{
		unsigned int iEndAddress;
		if ( (iInitialBaseAddress >= 0x82000000) && (iInitialBaseAddress < 0x8C000000) )
		{
			iEndAddress = 0x8BFFFFFF;
		}
		else
		{
			iEndAddress = 0x9BFFFFFF;
		}

		// compute leftover unused address space
		unsigned int iRemainingSpace = iEndAddress - baseAddress;

		int iPadPerEntry = iRemainingSpace / iEntryNum;
		//iPadPerEntry = iPadPerEntry & ~(64 * 1024); //align DOWN to 64k
		if ( iPadPerEntry > 0 )
		{
			// set the base address again with padding added
			iSetBaseAddress += iPadPerEntry * iSetEntryNum;
			iSetBaseAddress = AlignValue( iSetBaseAddress, 64 * 1024 );

			char szMacroValue[100];
			sprintf( szMacroValue, "0x%8.8llx", iSetBaseAddress );
	
			g_pVPC->SetScriptMacro( szMacroName, szMacroValue );
		}
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_LoadAddressMacroAuto
//
//	$LoadAddressMacroAuto	<MacroName> <BaseAddress>
//	{
//		<ProjectName>	<Length>
//	}
//
//	Specialized instruction to populate the load address macro based on a project
//	name.
//-----------------------------------------------------------------------------
void VPC_Keyword_LoadAddressMacroAuto( void )
{
	Internal_LoadAddressMacroAuto( false );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_LoadAddressMacroAuto_Padded
//
//	$LoadAddressMacroAuto_Padded	<MacroName> <BaseAddress>
//	{
//		<ProjectName>	<Length>
//	}
//
//	Specialized instruction to populate the load address macro based on a project
//	name. Assumes the contained list is minimally packed and has free reign of 
//	space up to the limit. Finds unused space spreads it out evenly between
//	each project
//-----------------------------------------------------------------------------
void VPC_Keyword_LoadAddressMacroAuto_Padded( void )
{
	Internal_LoadAddressMacroAuto( true );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Keyword_Conditional( bool bOverrideReserved )
{
	const char *pToken = g_pVPC->GetScript().GetToken( false );
	if ( !pToken || !pToken[0] )
		g_pVPC->VPCSyntaxError();

	if ( pToken[0] == '$' )
	{
		// being nice to users, quietly remove the unwanted conditional prefix '$'
		pToken++;
	}
    CUtlStringHolder<50> name( pToken );

    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
	{
		return;
	}

    CUtlStringHolder<100> value( pStrBuf->Get() );
    
	conditional_t *pConditional = g_pVPC->FindOrCreateConditional( name, true, CONDITIONAL_SCRIPT );
	if ( !bOverrideReserved &&
		 ( pConditional->m_Type != CONDITIONAL_CUSTOM && pConditional->m_Type != CONDITIONAL_SCRIPT ) )
	{
		// vpc scripts can only affect custom or script conditionals
		// scripts cannot affect the more systemic global conditionals
		g_pVPC->VPCSyntaxError( "$Conditional cannot be used on the reserved '$%s'", pConditional->m_UpperCaseName.Get() );
	}

	const char *pValue = Sys_EvaluateEnvironmentExpression( value, "0" );
    if ( !pValue )
	{
        pValue = value;
	}

	// conditional has been pre-qualified, set accordingly
	g_pVPC->SetConditional( name, Sys_StringToBool( pValue ), pConditional->m_Type );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_IgnoreRedundancyWarning
//
//-----------------------------------------------------------------------------
void VPC_Keyword_IgnoreRedundancyWarning( void )
{
    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
	{
		return;
	}

	bool bVal = Sys_StringToBool( pStrBuf->Get() );
	g_pVPC->SetIgnoreRedundancyWarning( bVal );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_Linux
//
//-----------------------------------------------------------------------------
void VPC_Keyword_Linux( void )
{
	// always ignore everything in this block
	// parsed and processed by a different tool
	g_pVPC->GetScript().SkipBracedSection();
}

//-----------------------------------------------------------------------------
//	VPC_PrepareToReadScript
//
//-----------------------------------------------------------------------------
void VPC_PrepareToReadScript( const char *pInputScriptName, int depth, bool bQuiet, char* &pScriptBuffer, CUtlString *pFixedScriptName )
{
	pFixedScriptName->Set( pInputScriptName );
	pFixedScriptName->FixSlashes();

	// always spew the root script
	if ( !bQuiet )
	{
		bool bSpew = ( depth == 0 );
		g_pVPC->VPCStatus( bSpew, "Parsing: %s", pFixedScriptName->Get() );
	}

	// parse the text script
    g_pVPC->GetScript().PushScript( pFixedScriptName->Get(), true );
}

void VPC_HandleIncludeStatement( int depth, bool bQuiet, void (*CallbackFn)( const char *pScriptName, int depth, bool bQuiet ) )
{
    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
	{
		// recurse into and run
        CUtlString pScriptName;
		char *pScriptBuffer;
		VPC_PrepareToReadScript( pStrBuf->Get(), depth+1, bQuiet, pScriptBuffer, &pScriptName );

		g_pVPC->AddScriptToParsedList( pStrBuf->Get(), false );

		CallbackFn( pScriptName, depth + 1, bQuiet );
		
		// restore state
		g_pVPC->GetScript().PopScript();
	}
}

void VPC_HandleProjectCommands( const char *pUnusedScriptName, int depth, bool bQuiet )
{
	const char *pToken;

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
			break;

		if ( CharStrEq( pToken, '}' ) )
		{
			break;
		}
		else if ( !V_stricmp_fast( pToken, "$include" ) )
		{
			VPC_HandleIncludeStatement( depth, bQuiet, VPC_HandleProjectCommands );
		}
		else if ( !V_stricmp_fast( pToken, "$Folder" ) ||
                  !V_stricmp_fast( pToken, "$ProjectOrFolder" ) )
		{
			// root level folder
			VPC_Keyword_Folder();
		}
		else if ( !V_stricmp_fast( pToken, "$UnityFolder" ) )
		{
			// root level folder
			VPC_Keyword_Folder( VPC_FOLDER_FLAGS_UNITY );
		}
		else if ( !V_stricmp_fast( pToken, "$File" ) )
		{
			// add root level file
			VPC_Keyword_AddFile();
		}
		else if ( !V_stricmp_fast( pToken, "-$File" ) )
		{
			// remove root level file
			VPC_Keyword_RemoveFile();
		}
		else if ( !V_stricmp_fast( pToken, "$Shaders" ) )
		{
			// add root level shaders folder
			VPC_Keyword_Shaders( 0 );
		}
		else
		{
			g_pVPC->VPCSyntaxError();
		}
	}
}

const char *GetVPCExecuteableCRC( CRC32_t *pCRC )
{
	static char vpcExePath[MAX_FIXED_PATH] = { 0 };
	static CRC32_t nCRCFromFileContents = 0;
	static bool bCalculated = false;

	// only need to do this sill VPC exe CRC calc once, and re-use
	if ( !bCalculated )
	{
		bCalculated = true;
	
		if ( Sys_GetExecutablePath( vpcExePath, sizeof( vpcExePath ) ) )
		{
			char *pBuffer = NULL;
			int cbVPCExe = Sys_LoadFile( vpcExePath, (void**)&pBuffer );
			if ( cbVPCExe != - 1 )
			{
				// Calculate the CRC from the contents of the file.
				nCRCFromFileContents = CRC32_ProcessSingleBuffer( pBuffer, cbVPCExe );
			}
			delete [] pBuffer;
		}
	}

	*pCRC = nCRCFromFileContents;
	return vpcExePath;
}

void WriteCRCCheckFile( const char *pVCProjFilename )
{
    CUtlStringBuilder *pFormatBuf = g_pVPC->GetTempStringBuffer1();
    pFormatBuf->Format( "%s." VPCCRCCHECK_FILE_EXTENSION, pVCProjFilename );

	char szVCProjBasePath[MAX_PATH];
	{
		char szProjRelativeDir[MAX_PATH];
		V_strncpy( szProjRelativeDir, pVCProjFilename, ARRAYSIZE( szProjRelativeDir ) );
		V_StripFilename( szProjRelativeDir );
		V_MakeAbsolutePath( szVCProjBasePath, ARRAYSIZE( szVCProjBasePath ), szProjRelativeDir, nullptr, k_bVPCForceLowerCase );
	}

	FILE *fp = fopen( pFormatBuf->Get(), "wt" );
	if ( !fp )
	{
		g_pVPC->VPCError( "Unable to open %s to write CRCs into.", pFormatBuf->Get() );
	}

	fprintf( fp, "%s\n", VPCCRCCHECK_FILE_VERSION_STRING );

	// add the supplemental string crc
	fprintf( fp, "%s\n", g_pVPC->GetCRCString() );

	// add metadata for the VS add-in
	fprintf( fp, "%s\n", g_pVPC->GetVSAddInMetadataString() );

	// Embedding the exe's CRC is not desired in branches that do rebuilds
	if ( g_pVPC->AddExecuteableToCRCChecks() )
	{
		// add the executable crc
		CRC32_t nCRCFromFileContents;
		const char *pExePath = GetVPCExecuteableCRC( &nCRCFromFileContents );
		fprintf( fp, "BIN %8.8x %s\n", ( unsigned int ) nCRCFromFileContents, pExePath );
	}
	
	CUtlDict<int, int> filenameDict( k_eDictCompareTypeFilenames );
	for ( int i=0; i < g_pVPC->m_ScriptList.Count(); i++ )
	{
		scriptList_t *pScript = &g_pVPC->m_ScriptList[i];
		if ( !pScript->m_bCRCCheck )
			continue;

		char szRelativeScriptPath[MAX_PATH];
		if ( V_IsAbsolutePath( pScript->m_scriptName ) )
		{
			V_MakeRelativePath( pScript->m_scriptName, szVCProjBasePath, szRelativeScriptPath, ARRAYSIZE( szRelativeScriptPath ) );
		}
		else
		{
			V_strncpy( szRelativeScriptPath, pScript->m_scriptName, ARRAYSIZE( szRelativeScriptPath ) );
		}
		V_RemoveDotSlashes( szRelativeScriptPath );

		// Use the dictionary to prevent duplicate file CRCs being written in here.
		if ( filenameDict.Find( szRelativeScriptPath ) == filenameDict.InvalidIndex() )
		{
			filenameDict.Insert( szRelativeScriptPath, 1 );

			// [crc] [filename]
			fprintf( fp, "%8.8x %s\n", ( unsigned int ) pScript->m_crc, szRelativeScriptPath );
		}
	}

	fclose( fp );
}


//-----------------------------------------------------------------------------
//	VPC_Keyword_Project
//
//-----------------------------------------------------------------------------
void VPC_Keyword_Project( int depth, bool bQuiet )
{
	//determine project generator before any generator-dependent configuration is allowed
	g_pVPC->DetermineProjectGenerator();

	CUtlString projectName;

    g_pVPC->m_bInProjectSection = true;
    
	// check for optional project name
	const char *pToken = g_pVPC->GetScript().PeekNextToken( false );

	if ( pToken && pToken[0] && !CharStrEq( pToken, '{' ) )
	{
		// get optional project name
		pToken = g_pVPC->GetScript().GetToken( false );
		if ( !pToken || !pToken[0] )
		{
			g_pVPC->VPCSyntaxError();
		}

        CUtlStringBuilder *pStrBuf = g_pVPC->GetMacroReplaceBuffer();
		g_pVPC->ResolveMacrosInString( pToken, pStrBuf );
		projectName = pStrBuf->Get();

		g_pVPC->DecorateProjectName( projectName );
	}
	else
	{
		projectName = g_pVPC->GetProjectGenerator()->GetProjectName();
	}

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || !CharStrEq( pToken, '{' ) )
		g_pVPC->VPCSyntaxError();

	VPC_HandleProjectCommands( NULL, depth, bQuiet );
	
	// the unnamed project does not get written, once it is named it will be written on closing scope
	if ( V_stricmp_fast( projectName.Get(), "UNNAMED" ) )
	{
		VPC_Schema_ForceAdditionalDependencies( projectName.Get() );
		
        IBaseProjectGenerator *pProjGen = g_pVPC->GetProjectGenerator();

		//write out the "VPC Scripts" folder
		{
			pProjGen->StartFolder( "VPC Scripts", VPC_FOLDER_FLAGS_DYNAMIC );

			for ( int i = 0; i < g_pVPC->m_ScriptList.Count(); ++i )
			{
				if ( g_pVPC->m_ScriptList[i].m_bCRCCheck )
				{
					pProjGen->StartFile( g_pVPC->m_ScriptList[i].m_scriptName, VPC_FILE_FLAGS_NONE, false );
					pProjGen->EndFile();
				}
			}
			pProjGen->EndFolder();
		}

		// change name
		pProjGen->SetProjectName( projectName.Get() );
        
		pProjGen->EndProject( true );
        g_pVPC->GetSolutionGenerator()->ProjectEnd( pProjGen );

		g_pVPC->m_bGeneratedProject = true;
	}

    g_pVPC->m_bInProjectSection = false;
}

bool VPC_IsBuiltInFileType( const char *pExtension )
{
	return ( IsCFileExtension( pExtension ) || IsHFileExtension( pExtension ) );
}

void VPC_Keyword_CustomBuildStep( void )
{
	bool							bAllowNextLine = false;
	CUtlVector<CUtlString>			extensions;

	const char *pToken = NULL;
	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;

		// Is this a conditional expression?
		if ( pToken[0] == '[' )
		{
			const char *pNextToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
			if ( pNextToken && pNextToken[0] == '[' )
			{
				g_pVPC->VPCSyntaxError( "Bad conditional syntax. Use C style boolean expression operators to express compound conditionals." );
			}

			if ( extensions.Count() == 0 )
			{
				g_pVPC->VPCSyntaxError( "Conditional specified on a $CustomBuildStep without any extensions preceding it." );
			}

			if ( !g_pVPC->EvaluateConditionalExpression( pToken ) )
			{
				extensions.Remove( extensions.Count() - 1 );
			}

			continue;
		}

		if ( CharStrEq( pToken, '\\' ) )
		{
			bAllowNextLine = true;
			continue;
		}
		else
		{
			bAllowNextLine = false;
		}

		if ( VPC_IsBuiltInFileType( pToken ) )
		{
			g_pVPC->VPCSyntaxError( "Cannot define a $CustomBuildStep for built in file type: %s", pToken);
		}

		CUtlString string = pToken;
		extensions.AddToTail( string );

		// check for another token
		pToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;
	}

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_strcmp( pToken, "{" ) )
	{
		g_pVPC->VPCSyntaxError( "Missing section for $CustomBuildStep" );
	}
	else if ( extensions.Count() == 0 )
	{
		g_pVPC->GetScript().SkipBracedSection();
		return;
	}
	else
	{
		int nLineSave = g_pVPC->GetScript().GetLine();
		const char *pScriptSave = g_pVPC->GetScript().GetData();
		while ( 1 )
		{
			pToken = g_pVPC->GetScript().GetToken( true );
			if ( !pToken || !pToken[0] )
				break;

			if ( CharStrEq( pToken, '}' ) )
			{
				// end of section
				break;
			}			
		}

		CVPC::CustomBuildStepForExtension_t customBuildStep;

		if ( g_pVPC->GetScript().GetData() > pScriptSave )
		{
			CUtlString tempString;
			tempString.SetDirect( pScriptSave, int( g_pVPC->GetScript().GetData() - pScriptSave - 1 ) );
			customBuildStep.m_BuildSteps = "$Configuration\n{\n$CustomBuildStep\n{";
			customBuildStep.m_BuildSteps += tempString + "}\n}\n";
		}

		if ( !customBuildStep.m_BuildSteps.IsEmpty() )
		{
			FOR_EACH_VEC( extensions, i )
			{
				if ( g_pVPC->m_CustomBuildSteps.Find( extensions[i].Get() ) != g_pVPC->m_CustomBuildSteps.InvalidIndex() )
				{
					g_pVPC->VPCWarning( "Duplicate $CustomBuildStep For '%s' - Ignoring.", extensions[i].Get() );
				}
				else
				{
					customBuildStep.m_DefinedInFile = g_pVPC->GetScript().GetName();
					customBuildStep.m_nDefinitionStartLine = nLineSave - 3; //-3 because we prepend 3 lines into customBuildStep.m_BuildSteps above
					g_pVPC->m_CustomBuildSteps.Insert( extensions[i].Get(), customBuildStep );
				}
			}
		}
	}
}

void VPC_Keyword_CustomAutoScript()
{
	bool bAllowNextLine = false;
	CUtlVector<CUtlString> extensions;
	const char *pToken = NULL;
	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;

		// Is this a conditional expression?
		if ( pToken[0] == '[' )
		{
			const char *pNextToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
			if ( pNextToken && pNextToken[0] == '[' )
			{
				g_pVPC->VPCSyntaxError( "Bad conditional syntax. Use C style boolean expression operators to express compound conditionals." );
			}

			if ( extensions.Count() == 0 )
			{
				g_pVPC->VPCSyntaxError( "Conditional specified on a $CustomAutoScript without any extensions preceding it." );
			}

			if ( !g_pVPC->EvaluateConditionalExpression( pToken ) )
			{
				extensions.Remove( extensions.Count() - 1 );
			}

			continue;
		}

		bAllowNextLine = CharStrEq( pToken, '\\' ) ;
		if ( bAllowNextLine )
			continue;

		if ( VPC_IsBuiltInFileType( pToken ) )
		{
			g_pVPC->VPCSyntaxError( "Cannot define a $CustomAutoScript for built in file type: %s", pToken);
		}

		CUtlString string = pToken;
		extensions.AddToTail( string );

		// check for another token
		pToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;
	}

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_strcmp( pToken, "{" ) )
	{
		g_pVPC->VPCSyntaxError( "Missing section for $CustomAutoScript" );
	}
	else if ( extensions.Count() == 0 )
	{
		g_pVPC->GetScript().SkipBracedSection( 1 );
		return;
	}
	else
	{
		const char *pScriptSave = g_pVPC->GetScript().GetData();
		g_pVPC->GetScript().SkipBracedSection( 1 );

		CUtlString autoScriptString;
		if ( g_pVPC->GetScript().GetData() > pScriptSave )
		{
			CUtlString tempString;
			tempString.SetDirect( pScriptSave, int( g_pVPC->GetScript().GetData() - pScriptSave - 1 ) );
			autoScriptString = tempString;
		}

		if ( !autoScriptString.IsEmpty() )
		{
			FOR_EACH_VEC( extensions, i )
			{
				if ( g_pVPC->m_CustomAutoScripts.Find( extensions[i].Get() ) != g_pVPC->m_CustomAutoScripts.InvalidIndex() )
				{
					g_pVPC->VPCWarning( "Duplicate $CustomAutoScript For '%s' - Ignoring.", extensions[i].Get() );
				}
				else
				{
					g_pVPC->m_CustomAutoScripts.Insert( extensions[i].Get(), autoScriptString );
				}
			}
		}
	}
}

void VPC_ParseProjectScriptParameters( const char *szScriptName, int depth, bool bQuiet )
{
	while ( 1 )
	{
		const char *pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			// end of file
			break;
		}

		if ( !V_stricmp_fast( pToken, "$Include" ) )
		{
			VPC_HandleIncludeStatement( depth, bQuiet, VPC_ParseProjectScriptParameters );
		}
		else if ( !V_stricmp_fast( pToken, "$Configuration" ) )
		{
			VPC_Keyword_Configuration();
		}
		else if ( !V_stricmp_fast( pToken, "$Project" ) )
		{
			Assert( depth || !g_pVPC->m_bProjectUsesUnity ); // Should already have been set up by CVPC::IsProjectUsingUnity()
			VPC_Keyword_Project( depth, bQuiet );
		}
		else if ( !V_stricmp_fast( pToken, "$UnityProject" ) ||
                  !V_stricmp_fast( pToken, "$ProjectOrFolder" ) )
		{
			Assert( depth || g_pVPC->m_bProjectUsesUnity ); // Should already have been set up by CVPC::IsProjectUsingUnity()
			VPC_Keyword_Project( depth, bQuiet );
		}
		else if ( !V_stricmp_fast( pToken, "$Macro" ) )
		{
			VPC_Keyword_Macro( VPC_MACRO_VALUE );
		}
		else if (!V_stricmp_fast(pToken, "$MacroEmptyString"))
		{
			VPC_Keyword_Macro(VPC_MACRO_EMPTY_STRING);
		}
		else if ( !V_stricmp_fast( pToken, "$MacroRequired" ) )
		{
			VPC_Keyword_MacroRequired( VPC_MACRO_REQUIRED_NOT_EMPTY );
		}
		else if ( !V_stricmp_fast( pToken, "$MacroRequiredAllowEmpty" ) )
		{
			VPC_Keyword_MacroRequired( VPC_MACRO_REQUIRED_ALLOW_EMPTY );
		}
		else if ( !V_stricmp_fast( pToken, "$LoadAddressMacro" ) )
		{
			VPC_Keyword_LoadAddressMacro();
		}
		else if ( !V_stricmp_fast( pToken, "$LoadAddressMacroAlias" ) )
		{
			VPC_Keyword_LoadAddressMacroAlias();
		}
		else if ( !V_stricmp_fast( pToken, "$LoadAddressMacroAuto" ) )
		{
			VPC_Keyword_LoadAddressMacroAuto();
		}
		else if ( !V_stricmp_fast( pToken, "$LoadAddressMacroAuto_Padded" ) )
		{
			VPC_Keyword_LoadAddressMacroAuto_Padded();
		}
		else if ( !V_stricmp_fast( pToken, "$IgnoreRedundancyWarning" ) )
		{
			VPC_Keyword_IgnoreRedundancyWarning();
		}
		else if ( !V_stricmp_fast( pToken, "$Linux" ) )
		{
			VPC_Keyword_Linux();
		}
		else if ( !V_stricmp_fast( pToken, "$CustomBuildStep" ) )
		{
			VPC_Keyword_CustomBuildStep();
		}
		else if ( !V_stricmp_fast( pToken, "$CustomAutoScript" ) )
		{
			VPC_Keyword_CustomAutoScript();
		}
		else if ( !V_stricmp_fast( pToken, "$Conditional" ) )
		{
			VPC_Keyword_Conditional( false );
		}
		else if ( !V_stricmp_fast( pToken, "$ConditionalOverrideReserved" ) )
		{
			VPC_Keyword_Conditional( true );
		}
		else if ( !V_stricmp_fast( pToken, "$LibDependsOnLib" ) )
		{
			VPC_Keyword_LibDependsOnLib();
		}
		else if ( !V_stricmp_fast( pToken, "$LibDependsOnImpLib" ) )
		{
			VPC_Keyword_LibDependsOnImpLib();
		}
		else
		{
			g_pVPC->VPCSyntaxError();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CVPC::ParseProjectScript( const char *pScriptName, int depth, bool bQuiet, bool bWriteCRCCheckFile, CDependency_Project *pDependencyProject /*= nullptr*/ )
{
	CDependency_Project *pOldDependencyProject = m_pDependencyProject;
	if ( pDependencyProject )
	{
		m_pDependencyProject = pDependencyProject;
	}
	char	*pScriptBuffer;

	if ( !depth && !bQuiet )
	{
		// Emit a separator from the prior project in case of errors.
		g_pVPC->VPCStatus( true, "\n" );
	}

    CUtlString szScriptName;
	VPC_PrepareToReadScript( pScriptName, depth, bQuiet, pScriptBuffer, &szScriptName );

	if ( !depth )
	{
		// create reserved $ROOTSCRIPT - tracks the root script
		SetScriptMacro( "ROOTSCRIPT", szScriptName );

		// create reserved $PROJECTNAME - tracks the undecorated pure project name
		// $(ProjectName) can be auto-decorated, making it unuseable by scripts expecting a pure project name
		SetScriptMacro( "PROJECTNAME", g_pVPC->GetProjectName() );

		// An uppercase version of the project name for preprocessor macro standardization
		CUtlString projectNameUpper( g_pVPC->GetProjectName() );
		projectNameUpper.ToUpper();
		SetScriptMacro( "UPPERCASEPROJECTNAME", projectNameUpper.Get() );

		// create reserved $LOADADDRESSNAME - defaults to project name but can be aliased with $LoadAddressMacroAlias
		SetScriptMacro( "LOADADDRESSNAME", g_pVPC->GetLoadAddressName() );

//#ifdef STEAM
		// create reserved $PROJECTDIR
        CUtlStringBuilder *pStrBuf = g_pVPC->GetTempStringBuffer1();
		pStrBuf->Set( g_pVPC->GetProjectPath() );
		V_RemoveDotSlashes( pStrBuf->Access() );
		SetScriptMacro( "PROJECTDIR", pStrBuf->Get(), true );
//#endif
		const CPUInformation &cpuInfo = GetCPUInformation();
		SetScriptMacro( "PHYSICAL_PROCESSOR_COUNT", CFmtStr( "%d", cpuInfo.m_nPhysicalProcessors ).Get() );
		SetScriptMacro( "PHYSICAL_PROCESSOR_COUNT_MINUS_ONE", CFmtStr( "%d", Max<int>( cpuInfo.m_nPhysicalProcessors - 1, 1 ) ).Get() );
		SetScriptMacro( "LOGICAL_PROCESSOR_COUNT", CFmtStr( "%d", cpuInfo.m_nLogicalProcessors ).Get() );
		SetScriptMacro( "LOGICAL_PROCESSOR_COUNT_MINUS_ONE", CFmtStr( "%d", Max<int>( cpuInfo.m_nLogicalProcessors - 1, 1 ) ).Get() );

		g_pVPC->ResetMissingFilesCount();

		// reset
		g_pVPC->m_CustomBuildSteps.Purge();
		g_pVPC->m_CustomAutoScripts.Purge();

		VPC_Qt_OnParseProjectStart();
		VPC_Schema_OnParseProjectStart();
		VPC_Unity_OnParseProjectStart();

		// save the conditional state before scripts can modify
		g_pVPC->SaveConditionals();
	}

	VPC_ParseProjectScriptParameters( szScriptName, depth, bQuiet );

	// for safety, force callers to restore to proper state
	g_pVPC->GetScript().PopScript();

	if ( !depth )
	{
		// at end of all processing		
		if ( bWriteCRCCheckFile )
		{
			// Finally write out the file with all the CRCs in it. This is referenced by the $CRCCHECK macro in the prebuild steps.
			WriteCRCCheckFile( GetProjectGenerator()->GetOutputFileName() );
		}

		g_pVPC->UpdateCacheFile( szScriptName );

		g_pVPC->m_ScriptList.Purge();
		
		// Remove any macros that came from the script file.
		g_pVPC->RemoveScriptCreatedMacros(); 

		// Restore the state of conditionals (don't want script modifications to affect further processing)
		g_pVPC->RestoreConditionals();

		//done with this generator. We'll pick a new one the next time we parse a script's configuration or project section.
        g_pVPC->SetProjectGenerator( nullptr );
	}

	if ( pDependencyProject )
	{
		m_pDependencyProject = pOldDependencyProject;
	}

	return true;
}

void CVPC::AddScriptToParsedList( const char *pScriptName, bool bAddToCRCCheck, CRC32_t crc /*= 0*/ )
{
	for ( int i = 0; i < m_ScriptList.Count(); i++ )
	{
		if ( !V_stricmp_fast( m_ScriptList[i].m_scriptName.Get(), pScriptName ) )
		{
			if ( bAddToCRCCheck )
			{
				// update
				m_ScriptList[i].m_crc = crc;
				m_ScriptList[i].m_bCRCCheck = bAddToCRCCheck;
			}
			return;
		}
	}

	int index = m_ScriptList.AddToTail();
	m_ScriptList[index].m_scriptName = pScriptName;
	m_ScriptList[index].m_crc = crc;
	m_ScriptList[index].m_bCRCCheck = bAddToCRCCheck;
}

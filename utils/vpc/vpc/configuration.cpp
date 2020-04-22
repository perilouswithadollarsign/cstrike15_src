//========= Copyright © 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

static KeywordName_t s_KeywordNameTable[] =
{
	{"$General",				KEYWORD_GENERAL},
	{"$Debugging",				KEYWORD_DEBUGGING},
	{"$Compiler",				KEYWORD_COMPILER},
	{"$SNCCompiler", 			KEYWORD_PS3_SNCCOMPILER},
	{"$GCCCompiler",			KEYWORD_PS3_GCCCOMPILER},
	{"$Librarian",				KEYWORD_LIBRARIAN},
	{"$Linker",					KEYWORD_LINKER},
	{"$SNCLinker",				KEYWORD_PS3_SNCLINKER},
	{"$GCCLinker",				KEYWORD_PS3_GCCLINKER},
	{"$ManifestTool",			KEYWORD_MANIFEST},
	{"$XMLDocumentGenerator",	KEYWORD_XMLDOCGEN},
	{"$BrowseInformation",		KEYWORD_BROWSEINFO},
	{"$Resources",				KEYWORD_RESOURCES},
	{"$PreBuildEvent",			KEYWORD_PREBUILDEVENT},
	{"$PreLinkEvent",			KEYWORD_PRELINKEVENT},
	{"$PostBuildEvent",			KEYWORD_POSTBUILDEVENT},
	{"$CustomBuildStep",		KEYWORD_CUSTOMBUILDSTEP},
	{"$Xbox360ImageConversion",	KEYWORD_XBOXIMAGE},
	{"$ConsoleDeployment",		KEYWORD_XBOXDEPLOYMENT},
	{"$Ant",					KEYWORD_ANT},
	{"$Intellisense",			KEYWORD_INTELLISENSE},
};

const char *CVPC::KeywordToName( configKeyword_e keyword )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_KeywordNameTable ) == KEYWORD_MAX );

	if ( keyword == KEYWORD_UNKNOWN )
	{
		return "???";
	}

	return s_KeywordNameTable[keyword].m_pName;
}

configKeyword_e CVPC::NameToKeyword( const char *pKeywordName )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_KeywordNameTable ) == KEYWORD_MAX );

	for ( int i = 0; i < ARRAYSIZE( s_KeywordNameTable ); i++ )
	{
		if ( !V_stricmp_fast( pKeywordName, s_KeywordNameTable[i].m_pName ) )
		{
			return s_KeywordNameTable[i].m_Keyword;
		}
	}

	return KEYWORD_UNKNOWN;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void VPC_Config_Macro()
{
	// Allowing macros to be created/set inside of configurations in order to construct a macro that is vectored on the configuration
	const char *pToken = g_pVPC->GetScript().GetToken( false );
	if ( !pToken || !pToken[0] )
	{
		g_pVPC->VPCSyntaxError();
	}

    CUtlStringHolder<MAX_MACRO_NAME> macroName( pToken );

    CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
	if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
	{
		return;
	}

	g_pVPC->SetPropertyMacro( macroName, pStrBuf->Get(), g_pVPC->GetProjectGenerator()->GetCurrentConfigurationName() );
}

//-----------------------------------------------------------------------------
//	VPC_Config_Keyword
//
//-----------------------------------------------------------------------------
void VPC_Config_Keyword( configKeyword_e keyword, const char *pkeywordToken )
{
	const char		*pToken;

	if ( ( keyword == KEYWORD_LIBRARIAN ) && g_pVPC->IsProjectUsingUnity() )
	{
		g_pVPC->VPCWarning( "$UnityProject should not be used in .LIB projects! The unity build generates a few large OBJs, so due to per-OBJ linker dependency determination linking with a unity-built .LIB would thus incur many unnecessary link dependencies." );
	}

	bool bShouldSkip = false;
	if ( !g_pVPC->GetProjectGenerator()->StartPropertySection( keyword, &bShouldSkip ) )
	{
		g_pVPC->VPCSyntaxError( "Unsupported Keyword: %s for target platform", pkeywordToken );
	}

	if ( bShouldSkip )
	{
		pToken = g_pVPC->GetScript().PeekNextToken( true );
		if ( !pToken || !pToken[0] || !CharStrEq( pToken, '{' ) )
			g_pVPC->VPCSyntaxError();

		g_pVPC->GetScript().SkipBracedSection();
	}
	else
	{
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
				// end of section
				break;
			}

			// Copy off the token name so HandleProperty() doesn't have to (or else the parser will overwrite it on the next token).
            CUtlStringHolder<100> tempTokenName( pToken );

			if ( !V_stricmp_fast( tempTokenName, "$PropertyMacro" ) )
			{
				// Allowing macros to be created/set inside of configurations in order to save off a property state into a macro.
				// This provides a way for users to temp alter properties and then restore them.
				// Syntax: $Macro <MacroName> <PropertyName> [condition]
				pToken = g_pVPC->GetScript().GetToken( false );
				if ( !pToken || !pToken[0] )
					g_pVPC->VPCSyntaxError();

                CUtlStringHolder<MAX_MACRO_NAME> macroName( pToken );

				// resolve the token that should be a recognized property key
                CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
				if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
				{
					continue;
				}

				// get the specified property key's value and set it
				g_pVPC->SetPropertyMacro( macroName, g_pVPC->GetProjectGenerator()->GetPropertyValue( pStrBuf->Get() ), g_pVPC->GetProjectGenerator()->GetCurrentConfigurationName() );
			}
			else if ( !V_stricmp_fast( tempTokenName, "$Macro" ) )
			{
				VPC_Config_Macro();
			}
			else
			{
				g_pVPC->GetProjectGenerator()->HandleProperty( tempTokenName );
			}
		}
	}

	g_pVPC->GetProjectGenerator()->EndPropertySection( keyword );
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_Configuration
//
//-----------------------------------------------------------------------------
void VPC_Keyword_Configuration()
{
	//determine project generator before any generator-dependent configuration is allowed
	g_pVPC->DetermineProjectGenerator();

	const char				*pToken;
	CUtlStringHolder<50>	configName;
	bool					bAllowNextLine = false;
	CUtlVector<CUtlString>	configs;

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;

		if ( CharStrEq( pToken, '\\' ) )
		{
			bAllowNextLine = true;
			continue;
		}
		else
		{
			bAllowNextLine = false;
		}

		int index = configs.AddToTail();
		configs[index] = pToken;

		// check for another optional config
		pToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
		if ( !pToken || !pToken[0] || CharStrEq( pToken, '{' ) || CharStrEq( pToken, '}' ) || (pToken[0] == '$') )
			break;
	}

	// no configuration specified, use all known
	if ( !configs.Count() )
	{
		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configs );
		if ( !configs.Count() )
		{
			g_pVPC->VPCError( "Trying to parse a configuration block and no configs have been defined yet.\n[%s line:%d]", g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
		}
	}

	// save parser state
	CScriptSource scriptSource = g_pVPC->GetScript().GetCurrentScript();

	for ( int i = 0; i < configs.Count(); i++ )
	{
		// restore parser state
		g_pVPC->GetScript().RestoreScript( scriptSource );

        configName.Set( configs[i].String() );

		// get access objects
		g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configName, false );

		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] || !CharStrEq( pToken, '{' ) )
		{
			g_pVPC->VPCSyntaxError();
		}

		while ( 1 )
		{
			g_pVPC->GetScript().SkipToValidToken();

			pToken = g_pVPC->GetScript().PeekNextToken( true );
			if ( pToken && pToken[0] && !V_stricmp_fast( pToken, "$Macro" ) )
			{
				pToken = g_pVPC->GetScript().GetToken( true );
				if ( !pToken  || !pToken[0] )
					g_pVPC->VPCSyntaxError();

				VPC_Config_Macro();
				continue;
			}

            CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
			{
				g_pVPC->GetScript().SkipBracedSection();
				continue;
			}

			if ( CharStrEq( pStrBuf->Get(), '}' ) )
			{
				// end of section
				break;
			}

			configKeyword_e keyword = g_pVPC->NameToKeyword( pStrBuf->Get() );
			if ( keyword == KEYWORD_UNKNOWN )
			{
				g_pVPC->VPCSyntaxError();
			}
			else
			{
                CUtlStringHolder<50> keywordStr( pStrBuf->Get() );
				VPC_Config_Keyword( keyword, keywordStr );
			}
		}

		g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
	}
}

//-----------------------------------------------------------------------------
//	VPC_Keyword_FileConfiguration
//
//-----------------------------------------------------------------------------
void VPC_Keyword_FileConfiguration()
{
	const char	*pToken;
	bool		bAllowNextLine = false;
	CUtlVector< CUtlString > configurationNames;

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( bAllowNextLine );
		if ( !pToken || !pToken[0] )
			break;

		if ( CharStrEq( pToken, '\\' ) )
		{
			bAllowNextLine = true;
			continue;
		}
		else
		{
			bAllowNextLine = false;
		}

		configurationNames.AddToTail( pToken );

		// check for another optional config
		pToken = g_pVPC->GetScript().PeekNextToken( bAllowNextLine );
		if ( !pToken || !pToken[0] || CharStrEq( pToken, '{' ) || CharStrEq( pToken, '}' ) || (pToken[0] == '$') )
			break;
	}

	// no configuration specified, use all known
	if ( configurationNames.Count() == 0 )
	{
		g_pVPC->GetProjectGenerator()->GetAllConfigurationNames( configurationNames );
	}

	// save parser state
	CScriptSource scriptSource = g_pVPC->GetScript().GetCurrentScript();

    int nWarningLine = -1;

	for ( int i=0; i < configurationNames.Count(); i++ )
	{
		// restore parser state
		g_pVPC->GetScript().RestoreScript( scriptSource );

		// Tell the generator we're about to feed it configuration data for this file.
		g_pVPC->GetProjectGenerator()->StartConfigurationBlock( configurationNames[i].String(), true );

		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] || !CharStrEq( pToken, '{' ) )
		{
			g_pVPC->VPCSyntaxError();
		}

		while ( 1 )
		{
			g_pVPC->GetScript().SkipToValidToken();

			pToken = g_pVPC->GetScript().PeekNextToken( true );
			if ( pToken && pToken[0] && !V_stricmp_fast( pToken, g_pOption_ExcludedFromBuild ) )
			{
				pToken = g_pVPC->GetScript().GetToken( true );
				if ( !pToken || !pToken[0] )
					g_pVPC->VPCSyntaxError();

                CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
				if ( g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
				{
					g_pVPC->GetProjectGenerator()->FileExcludedFromBuild( Sys_StringToBool( pStrBuf->Get() ) );
				}

				continue;
			}
			else if ( pToken && pToken[0] && !V_stricmp_fast( pToken, "$Macro" ) )
			{
				pToken = g_pVPC->GetScript().GetToken( true );
				if ( !pToken || !pToken[0] )
					g_pVPC->VPCSyntaxError();

				VPC_Config_Macro();
				continue;
			}

            CUtlStringBuilder *pStrBuf = g_pVPC->GetPropertyValueBuffer();
			if ( !g_pVPC->GetScript().ParsePropertyValue( NULL, pStrBuf ) )
			{
				g_pVPC->GetScript().SkipBracedSection();
				continue;
			}

			if ( CharStrEq( pStrBuf->Get(), '}' ) )
			{
				// end of section
				break;
			}

			configKeyword_e keyword = g_pVPC->NameToKeyword( pStrBuf->Get() );
			switch ( keyword )
			{
			// these are the only tools wired to deal with file configuration overrides
			case KEYWORD_COMPILER:
			case KEYWORD_PS3_SNCCOMPILER:
			case KEYWORD_PS3_GCCCOMPILER:
                if ( !g_pVPC->IsPerFileCompileConfigEnabled() &&
                     !g_pVPC->GetScript().IsInPrivilegedScript() &&
                     g_pVPC->GetScript().GetLine() != nWarningLine )
                {
                    g_pVPC->VPCSyntaxError( "%s(%u): per-file compile configuration not allowed",
                                            g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetLine() );
                    nWarningLine = g_pVPC->GetScript().GetLine();
                }
                // Fall through
			case KEYWORD_RESOURCES:
			case KEYWORD_CUSTOMBUILDSTEP:
            {
                CUtlStringHolder<50> keywordStr( pStrBuf->Get() );
				VPC_Config_Keyword( keyword, keywordStr );
				break;
            }
			default:
				g_pVPC->VPCSyntaxError();
			}
		}
		
		g_pVPC->GetProjectGenerator()->EndConfigurationBlock();
	}
}


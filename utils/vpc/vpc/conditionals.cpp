//======== Copyright � 1996-2016, Valve Corporation, All rights reserved. ===========//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::SetupDefaultConditionals()
{
	//
	// PLATFORM Conditionals
	//
	{
		FindOrCreateConditional( "WIN32", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "WIN64", true, CONDITIONAL_PLATFORM );
		
		FindOrCreateConditional( "LINUX32", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "LINUX64", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "LINUXSERVER32", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "LINUXSERVER64", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "LINUXSTEAMRTARM32HF", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "LINUXSTEAMRTARM64HF", true, CONDITIONAL_PLATFORM );

		FindOrCreateConditional( "OSX32", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "OSX64", true, CONDITIONAL_PLATFORM );

		FindOrCreateConditional( "X360", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "PS3", true, CONDITIONAL_PLATFORM );

		FindOrCreateConditional( "IOS", true, CONDITIONAL_PLATFORM );

		FindOrCreateConditional( "ANDROIDARM32", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "ANDROIDARM64", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "ANDROIDMIPS32", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "ANDROIDMIPS64", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "ANDROIDX8632", true, CONDITIONAL_PLATFORM );
		FindOrCreateConditional( "ANDROIDX8664", true, CONDITIONAL_PLATFORM );
	}
	
	//
	// SYSTEM conditionals
	//
	{
		// setup default system conditionals
		FindOrCreateConditional( "PROFILE", true, CONDITIONAL_SYSTEM );		
		FindOrCreateConditional( "RETAIL", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "CALLCAP", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "FASTCAP", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "CERT", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "MEMTEST", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "NOFPO", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "POSIX", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "LV", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "DEMO", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "NO_SCALEFORM", false, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "NO_STEAM", false, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "DVDEMU", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "QTDEBUG", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "NO_CEG", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "UPLOAD_CEG", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "SOURCECONTROL", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "ALLOW_OS_MACRO", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "CRCCHECK_IN_PROJECT", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "MISSING_FILE_CHECK", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "MISSING_FILE_IS_ERROR", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "FILEPATTERN", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "ADD_EXE_TO_CRC_CHECK", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "P4_AUTO_ADD", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "ALLOW_QT", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "ALLOW_SCHEMA", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "ALLOW_UNITY", true, CONDITIONAL_SYSTEM );
		FindOrCreateConditional( "ALLOW_CLANG", true, CONDITIONAL_SYSTEM );
	}
}	

CUtlString CVPC::GetCRCStringFromConditionals()
{
	CUtlString CRCString;

	CUtlVectorFixedGrowable<const char *, 1024> sortRelevantConditionals;

	// Any enabled system conditional needs to make a CRC string that can be matched against for project staleness.
	// These used to be terse abbreviations when they were passed on the CL but now not a constraint with vpccrccheck and peer crc files.
	for ( int i = 0; i < m_Conditionals.Count(); i++ )
	{
		if ( m_Conditionals[i]->m_bDefined && 
			( m_Conditionals[i]->m_Type == CONDITIONAL_SYSTEM || m_Conditionals[i]->m_Type == CONDITIONAL_CUSTOM || m_Conditionals[i]->m_Type == CONDITIONAL_SCRIPT ) )
		{
			sortRelevantConditionals.AddToTail( m_Conditionals[i]->m_UpperCaseName.Get() );
		}
	}

	//sort the conditionals so the existing crc doesn't appear stale if they simply appear in a different order on the command line
	sortRelevantConditionals.SortPredicate( 
		[] ( const char *szLeft, const char *szRight ) -> bool
		{
			return V_stricmp_fast( szLeft, szRight ) < 0;
		} );

	for ( int i = 0; i < sortRelevantConditionals.Count(); ++i )
	{
		CRCString += CFmtStr( ".%s", sortRelevantConditionals[i] );
	}

	return CRCString;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CVPC::GetTargetPlatformName()
{
	for ( int i = 0; i < m_Conditionals.Count(); i++ )
	{
		conditional_t *pConditional = m_Conditionals[i];
		if ( pConditional->m_Type == CONDITIONAL_PLATFORM && pConditional->m_bDefined )
		{
			return pConditional->m_Name.String();
		}
	}

	// fatal - should have already been default set
	Assert( 0 );
	VPCError( "Unspecified platform." );

	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CVPC::GetTargetCompilerName()
{
	const char *pPlatformName = GetTargetPlatformName();
	if ( !V_stricmp_fast( pPlatformName, "WIN32" ) ||
		 !V_stricmp_fast( pPlatformName, "WIN64" ) ||
		 !V_stricmp_fast( pPlatformName, "X360" ) )
	{
		if ( IsConditionalDefined( "VS2005" ) )
			return "VS2005";

		if ( IsConditionalDefined( "VS2010" ) )
			return "VS2010";

		if ( IsConditionalDefined( "VS2012" ) )
			return "VS2012";

		if ( IsConditionalDefined( "VS2013" ) )
			return "VS2013";

		if ( IsConditionalDefined( "VS2015" ) )
			return "VS2015";
	}
    else if ( VPC_IsPlatformLinux( pPlatformName ) || VPC_IsPlatformAndroid( pPlatformName ) )
    {
        return "GCC";
    }
	else if ( !V_stricmp_fast( pPlatformName, "OSX32" ) ||
              !V_stricmp_fast( pPlatformName, "OSX64" ) )
	{
        return "Clang";
    }

	// TODO: support other platforms (needed by schemacompiler/clang)
	ExecuteOnce( g_pVPC->VPCWarning( "TODO: GetTargetCompilerName not yet implemented for platform %s!", pPlatformName ) );
	return "UNKNOWN";
}

//-----------------------------------------------------------------------------
//	Case Insensitive. Returns true if platform conditional has been marked
//	as defined.
//-----------------------------------------------------------------------------
bool CVPC::IsPlatformDefined( const char *pName )
{
	for ( int i = 0; i < m_Conditionals.Count(); i++ )
	{
		if ( m_Conditionals[i]->m_Type == CONDITIONAL_PLATFORM && !V_stricmp_fast( pName, m_Conditionals[i]->m_Name.String() ) )
		{
			return m_Conditionals[i]->m_bDefined;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//	Case Insensitive. Returns true if the given string is a platform name
//-----------------------------------------------------------------------------
bool CVPC::IsPlatformName( const char *pName )
{
	for ( int i=0; i<m_Conditionals.Count(); i++ )
	{
		if ( m_Conditionals[i]->m_Type == CONDITIONAL_PLATFORM && !V_stricmp_fast( pName, m_Conditionals[i]->m_Name.String() ) )
		{
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
//	Case Insensitive
//-----------------------------------------------------------------------------
conditional_t *CVPC::FindOrCreateConditional( const char *pName, bool bCreate, conditionalType_e type )
{
	for (int i=0; i<m_Conditionals.Count(); i++)
	{
		if ( !V_stricmp_fast( pName, m_Conditionals[i]->m_Name.String() ) )
		{
			// found
			return m_Conditionals[i];
		}
	}

	if ( !bCreate )
	{
		return NULL;
	}

	int index = m_Conditionals.AddToTail();
	m_Conditionals[index] = new conditional_t();

	char tempName[256];
	V_strncpy( tempName, pName, sizeof( tempName ) );
	
	// primary internal use as lower case, but spewed to user as upper for style consistency
	m_Conditionals[index]->m_Name = V_strlower( tempName );
	m_Conditionals[index]->m_UpperCaseName = V_strupper( tempName );
	m_Conditionals[index]->m_Type = type;

	return m_Conditionals[index];
}

void CVPC::SetConditional( const char *pString, bool bSet, conditionalType_e conditionalType )
{
	conditional_t *pConditional = FindOrCreateConditional( pString, true, conditionalType );
	if ( !pConditional )
	{
		VPCError( "Failed to find or create $%s conditional", pString );
	}

	VPCStatus( false, "Set Conditional: $%s = %s", pConditional->m_UpperCaseName.Get(), ( bSet ? "1" : "0" ) );

	if ( conditionalType != pConditional->m_Type )
	{
		VPCSyntaxError( "Cannot set reserved conditional '$%s'", pConditional->m_UpperCaseName.Get() );
	}

	pConditional->m_bDefined = bSet;

	if ( pConditional->m_Type == CONDITIONAL_SYSTEM )
	{
		// system conditionals are set at specific early execution points and not mutable by scripts
		// cache off the state for any possible inner loop repetitive state queries
		if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "SOURCECONTROL" ) )
		{
			m_bSourceControl = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "ALLOW_OS_MACRO" ) )
		{
			m_bAllowOSMacro = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "CRCCHECK_IN_PROJECT" ) )
		{
			m_bCRCCheckInProject = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "MISSING_FILE_CHECK" ) )
		{
			m_bCheckFiles = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "MISSING_FILE_IS_ERROR" ) )
		{
			m_bMissingFileIsError = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "FILEPATTERN" ) )
		{
			m_bAllowFilePattern = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "ADD_EXE_TO_CRC_CHECK" ) )
		{
			m_bAddExecuteableToCRC = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "P4_AUTO_ADD" ) )
		{
			m_bP4AutoAdd = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "PREFER_VS2010" ) )
		{
			m_bPreferVS2010 = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "PREFER_VS2012" ) )
		{
			m_bPreferVS2012 = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "PREFER_VS2013" ) )
		{
			m_bPreferVS2013 = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "PREFER_VS2015" ) )
		{
			m_bPreferVS2015 = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "ALLOW_QT" ) )
		{
			m_bAllowQt = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "ALLOW_SCHEMA" ) )
		{
			m_bAllowSchema = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "ALLOW_UNITY" ) )
		{
			m_bAllowUnity = bSet;
		}
		else if ( !V_strcmp( pConditional->m_UpperCaseName.Get(), "ALLOW_CLANG" ) )
		{
			m_bAllowClang = bSet;
		}
	}
}

//-----------------------------------------------------------------------------
//	Returns true if string has a conditional of the specified type
//-----------------------------------------------------------------------------
bool CVPC::ConditionHasDefinedType( const char* pCondition, conditionalType_e type )
{
	for ( int i=0; i<m_Conditionals.Count(); i++ )
	{
		if ( m_Conditionals[i]->m_Type != type )
			continue;

        const char *pScan = pCondition;
        while ( *pScan )
        {
            pScan = strchr( pScan, '$' );
            if ( !pScan )
            {
                break;
            }

            pScan++;
            if ( V_strnicmp( pScan, m_Conditionals[i]->m_Name, m_Conditionals[i]->m_Name.Length() ) == 0 )
            {
                // a define of expected type occurs in the conditional expression
                return true;
            }
        }
	}

	return false;
}

//-----------------------------------------------------------------------------
//	Callback for expression evaluator.
//-----------------------------------------------------------------------------
bool CVPC::ResolveConditionalSymbol( const char *pSymbol )
{
	int offset = 0;

	if ( ( pSymbol[0] == '$' && pSymbol[1] == '0' && pSymbol[2] == 0 ) ||
         CharStrEq( pSymbol, '0' ) )
	{
		return false;
	}
	else if ( ( pSymbol[0] == '$' && pSymbol[1] == '1' && pSymbol[2] == 0 ) ||
              CharStrEq( pSymbol, '1' ) )
	{
		return true;
	}

	if ( pSymbol[0] == '$' )
	{
		offset = 1;
	}

	conditional_t *pConditional = FindOrCreateConditional( (char*)pSymbol+offset, false, CONDITIONAL_NULL );
	if ( pConditional )
	{
		// game conditionals only resolve true when they are 'defined' and 'active'
		// only one game conditional is expected to be active at a time
		if ( pConditional->m_Type == CONDITIONAL_GAME )
		{
			if ( !pConditional->m_bDefined )
			{
				return false;
			}

			return pConditional->m_bGameConditionActive;
		}

		// all other type of conditions are gated by their 'defined' state
		return pConditional->m_bDefined;
	}
	else
	{
		// The conditional was not found.
		// MACROS ARE NOT ALLOWED IN CONDITIONALS because it became too commonplace to use the wrong symbol
		// causing quiet unintended results and since macros can be arbitrary strings, there placement in a
		// conditional expression is invalid. Restricting to conditionals ensures we are only ever evaluating
		// is valid boolean values.
		CMacro *pMacro = g_pVPC->FindMacro( (char*)pSymbol+offset );
		if ( pMacro )
		{
			// found a macro, and not allowed
			g_pVPC->VPCSyntaxError( "Macro '%s' detected in conditional expression and not allowed. Use \"$Conditional <name> <0/1>\"", pSymbol );
		}
	}

	// unknown conditional, defaults to false
	return false;
}

void CVPC::SaveConditionals()
{
	// only expecting a single save point
	AssertMsg( m_SavedConditionals.Count() == 0, "SaveConditionals: Unexpected processing state, conditionals already saved\n" );

	m_SavedConditionals.Purge();

	if ( !m_Conditionals.Count() )
		return;

	// clone
	m_SavedConditionals.SetCount( m_Conditionals.Count() );
	for ( int i = 0; i < m_Conditionals.Count(); i++ )
	{
		m_SavedConditionals[i] = new conditional_t( *m_Conditionals[i] );
	}
}

void CVPC::RestoreConditionals()
{
	if ( !m_SavedConditionals.Count() )
	{
		// already restored or nothing saved
		return;
	}

	// whatever state the conditionals were changed to is undesired
	// these get discarded
	m_Conditionals.PurgeAndDeleteElements();

	// restore the saved conditionals and purge the saved
	m_Conditionals.Swap( m_SavedConditionals );
	for ( int i = 0; i < m_Conditionals.Count(); i++ )
	{
		// Call SetConditional to update cached member bools:
		SetConditional( m_Conditionals[i]->m_Name.Get(), m_Conditionals[i]->m_bDefined, m_Conditionals[i]->m_Type );
	}
}

//-----------------------------------------------------------------------------
//	Callback for expression evaluator.
//-----------------------------------------------------------------------------
static bool ResolveSymbol( const char *pSymbol )
{
	return g_pVPC->ResolveConditionalSymbol( pSymbol );
}

//-----------------------------------------------------------------------------
//	Callback for expression evaluator.
//-----------------------------------------------------------------------------
static void SymbolSyntaxError( const char *pReason )
{
	// invoke internal syntax error hndling which spews script stack as well
	g_pVPC->VPCSyntaxError( "%s", pReason );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CVPC::EvaluateConditionalExpression( const char *pExpression )
{
	if ( !pExpression || !pExpression[0] )
	{
		// empty string, same as not having a conditional
		return true;
	}

	bool bResult = false;
	CExpressionEvaluator ExpressionHandler;
	bool bValid = ExpressionHandler.Evaluate( bResult, pExpression, ::ResolveSymbol, ::SymbolSyntaxError );
	if ( !bValid )
	{
		g_pVPC->VPCSyntaxError( "VPC Conditional Evaluation Error" );
	}

	return bResult;
}

bool CVPC::IsConditionalDefined( const char *pName )
{
	conditional_t *pConditional = FindOrCreateConditional( pName, false, CONDITIONAL_NULL );
	return pConditional && pConditional->m_bDefined;
}


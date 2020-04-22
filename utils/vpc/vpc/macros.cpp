//========= Copyright © 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC 
//
//=====================================================================================//

#include "vpc.h"

inline bool IsValidMacroNameChar( char ch )
{
    return ch == '_' || V_isalnum( ch );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMacro::CMacro( const char *pMacroName, const char *pMacroValue, const char *pConfigurationName, bool bSystemMacro, bool bSetupDefine )
{
	SetMacroName( pMacroName );
	m_Value = pMacroValue;

	if ( pConfigurationName )
	{
		// property macros (i.e. with configurations) are purposely narrow
		// they are not interchangeable with non-property macros that provide a different set of features
		// this is just to ensure that hacks don't come along with a misunderstanding
		Assert( bSystemMacro == false );
		Assert( bSetupDefine == false );
		
		if ( !pConfigurationName[0] )
		{
			// valid configuration name is mandatory
			g_pVPC->VPCError( "Missing expected configuration for property macro '%s'.", pMacroName );
		}

		m_ConfigurationName = pConfigurationName;

		m_bSetupDefineInProjectFile = false;
		m_bSystemMacro = false;
	}
	else
	{
		m_bSetupDefineInProjectFile = bSetupDefine;
		m_bSystemMacro = bSystemMacro;
	}

	m_pFNResolveDynamicMacro = nullptr;
}

CMacro::CMacro( const char *pMacroName, void (*pFNResolveValue)( CMacro * ) )
{
	SetMacroName( pMacroName );
	m_pFNResolveDynamicMacro = pFNResolveValue;
	m_bSetupDefineInProjectFile = false;
	m_bSystemMacro = true;
}

void CMacro::SetMacroName( const char *pMacroName )
{
	m_nBaseNameLength = V_strlen( pMacroName );
	if ( m_nBaseNameLength >= MAX_MACRO_NAME )
	{
		g_pVPC->VPCError( "Macro name '%s' too long.", pMacroName );
	}

	for ( int i = 0; i < m_nBaseNameLength; i++ )
	{
		if ( !IsValidMacroNameChar( pMacroName[i] ) )
		{
			g_pVPC->VPCError( "Macro name '%s' contains illegal character '%c'.",
								pMacroName, pMacroName[i] );
		}
	}
    
	// Internally adds another one for terminator.
	m_FullName.SetLength( m_nBaseNameLength + 1 );
	char *pFullName = m_FullName.Access();
	*pFullName = '$';
	V_strcpy( pFullName + 1, pMacroName );
}

//-----------------------------------------------------------------------------
// System macros are created by VPC and are expected to persist across projects.
/// They appear as Read Only to scripts.
//-----------------------------------------------------------------------------
CMacro *CVPC::SetSystemMacro( const char *pMacroName, const char *pMacroValue, bool bSetupDefineInProjectFile )
{
	VPCStatus( false, "Set System Macro: $%s = %s", pMacroName, pMacroValue );

	CMacro *pMacro = FindMacro( pMacroName );
	if ( pMacro )
	{
		// found existing macro
		if ( pMacro->IsPropertyMacro() )
		{
			// duplicate macro names not allowed
			g_pVPC->VPCError( "Macro '%s' already defined as a property macro.", pMacro->GetName() );
		}

		if ( !pMacro->IsSystemMacro() )
		{
			// internal macros cannot clash with script macros
			g_pVPC->VPCError( "$Macro '%s' already defined by script.", pMacro->GetName() );
		}

		// update value
		pMacro->SetValue( pMacroValue );
	}
	else
	{
		// create a system type macro
		pMacro = new CMacro( pMacroName, pMacroValue, NULL, true, bSetupDefineInProjectFile );
		m_Macros.InsertWithDupes( pMacroName, pMacro );
	}

	return pMacro;
}

CMacro *CVPC::SetDynamicMacro( const char *pMacroName, void (*pFNResolveValue)( CMacro *pThis ) )
{
	VPCStatus( false, "Set Dynamic Macro: $%s", pMacroName );

	CMacro *pMacro = FindMacro( pMacroName );
	if ( pMacro )
	{
		// found existing macro
		if ( pMacro->IsPropertyMacro() )
		{
			// duplicate macro names not allowed
			g_pVPC->VPCError( "Macro '%s' already defined as a property macro.", pMacro->GetName() );
		}

		if ( !pMacro->IsSystemMacro() )
		{
			// internal macros cannot clash with script macros
			g_pVPC->VPCError( "$Macro '%s' already defined by script.", pMacro->GetName() );
		}

		// update value
		pMacro->SetResolveFunc( pFNResolveValue );
	}
	else
	{
		// create a system type macro
		pMacro = new CMacro( pMacroName, pFNResolveValue );
		m_Macros.InsertWithDupes( pMacroName, pMacro );
	}

	return pMacro;
}

//-----------------------------------------------------------------------------
// Script macros are created by a project script based on THEIR state. They are removed at the conclusion of that project
// to avoid polluting the next project that gets processed.
//-----------------------------------------------------------------------------
CMacro *CVPC::SetScriptMacro( const char *pMacroName, const char *pMacroValue, bool bSetupDefineInProjectFile )
{
	VPCStatus( false, "Set Script Macro: $%s = %s", pMacroName, pMacroValue );

	CMacro *pMacro = FindMacro( pMacroName );
	if ( pMacro )
	{
		// found existing macro
		if ( pMacro->IsPropertyMacro() )
		{
			// duplicate macro names not allowed
			g_pVPC->VPCError( "Macro '%s' already defined as a property macro.", pMacro->GetName() );
		}
		/*
		if ( pMacro->IsSystemMacro() )
		{
			// scripts are not allowed to alter system macros
			g_pVPC->VPCError( "Script not allowed to alter system macro '%s'.", pMacro->GetName() );
		}*/

		// update value
		pMacro->SetValue( pMacroValue );
	}
	else
	{
		// create a script type macro
		pMacro = new CMacro( pMacroName, pMacroValue, NULL, false, bSetupDefineInProjectFile );
		m_Macros.InsertWithDupes( pMacroName, pMacro );
	}

	return pMacro;
}

//-----------------------------------------------------------------------------
// Property macros are a variant of script macros that are highly constrained and can only
// be used to capture the state of a property key within a configuration block. They can then
// only be resolved with a configuration block.
//-----------------------------------------------------------------------------
CMacro *CVPC::SetPropertyMacro( const char *pMacroName, const char *pMacroValue, const char *pConfigurationName )
{
	VPCStatus( false, "Set Property Macro (%s): $%s = %s", ( pConfigurationName && pConfigurationName[0] ? pConfigurationName : "???" ), pMacroName, pMacroValue );

	if ( !pConfigurationName || !pConfigurationName[0] )
	{
		// configuration is mandatory
		VPCError( "Missing expected configuration for property macro '%s'.", pMacroName );
	}

	CMacro *pMacro = FindMacro( pMacroName );
	if ( pMacro && !pMacro->IsPropertyMacro() )
	{
		// duplicate macro names are not allowed
		// found an existing non-property based macro with same name
		VPCError( "Cannot set pre-existing macro '%s' as a property macro.", pMacroName );
	}

	// resolve with expected configuration
	pMacro = FindMacro( pMacroName, pConfigurationName );
	if ( pMacro )
	{
		// update the macro
		pMacro->SetValue( pMacroValue );
	}
	else
	{
		// create property macro
		pMacro = new CMacro( pMacroName, pMacroValue, pConfigurationName, false, false );
		m_Macros.InsertWithDupes( pMacroName, pMacro );
	}

	return pMacro;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMacro *CVPC::FindMacro( const char *pMacroName, const char *pConfigurationName )
{
	if ( pConfigurationName && pConfigurationName[0] )
	{
		// iterate to find macro (duplicated due to configuration) with matching configuration
		for ( int nMacroIndex = m_Macros.FindFirst( pMacroName ); nMacroIndex != m_Macros.InvalidIndex(); nMacroIndex = m_Macros.NextInorderSameKey( nMacroIndex ) )
		{
			CMacro *pMacro = m_Macros[nMacroIndex];
			if ( pMacro->IsPropertyMacro() && !V_stricmp_fast( pConfigurationName, pMacro->GetConfigurationName() ) )
			{
				// found matching configuration based macro
				return pMacro;
			}
		}
	
		// not found
		return NULL;
	}
	
	// direct lookup
	int nMacroIndex = m_Macros.Find( pMacroName );
	if ( nMacroIndex != m_Macros.InvalidIndex() )
	{
		return m_Macros[nMacroIndex];
	}

	// not found
	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CVPC::GetMacrosMarkedForCompilerDefines( CUtlVector< CMacro* > &macroDefines )
{
	macroDefines.Purge();

	for ( int nMacroIndex = m_Macros.FirstInorder(); nMacroIndex != m_Macros.InvalidIndex(); nMacroIndex = m_Macros.NextInorder( nMacroIndex ) )
	{
		CMacro *pMacro = m_Macros[nMacroIndex];
		if ( pMacro->ShouldDefineInProjectFile() )
		{
			macroDefines.AddToTail( pMacro );
		}
	}

	return macroDefines.Count();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::ResolveMacrosInString( char const *pString, CUtlStringBuilder *pOutBuff, CUtlVector< CUtlString > *pMacrosReplaced )
{
	// iterate and resolve user macros until all macros resolved
    if ( pString )
    {
        pOutBuff->Set( pString );
    }

    int nScanIndex = 0;
    while ( (size_t)nScanIndex < pOutBuff->Length() )
	{
        const char *pStartOfMacroToken = strchr( pOutBuff->Get() + nScanIndex, '$' );
        if ( !pStartOfMacroToken )
        {
            break;
        }

        // Skip over $.
        pStartOfMacroToken++;
        
        // If we don't find a macro for this $token we start scanning
        // right after the $. If we do find a macro and replace some
        // text we'll start right where we did the replacement at the $.
        nScanIndex = (int)( pStartOfMacroToken - pOutBuff->Get() );

        if ( !IsValidMacroNameChar( *pStartOfMacroToken ) )
        {
            continue;
        }
        
        char macroToken[MAX_MACRO_NAME];
        int nTokenChars = 0;

        CMacro *pMacro = NULL;
		for ( int nMacroIndex = m_Macros.FirstInorder(); nMacroIndex != m_Macros.InvalidIndex(); nMacroIndex = m_Macros.NextInorder( nMacroIndex ) )
		{
			CMacro *pCheck = m_Macros[nMacroIndex];
            if ( ( nTokenChars <= 0 ||
                   pCheck->GetNameLength() >= nTokenChars ) &&
                 V_strnicmp( pStartOfMacroToken, pCheck->GetName(), pCheck->GetNameLength() ) == 0 )
            {
                //
                // resolve substring match as possible larger token for disqualifying an unintended replacement collision
                // i.e. $FOO cannot be replaced in a string that contains $FOOBAR, where $FOOBAR is a macro as well
                //

                // Collect token if we haven't already.
                if ( nTokenChars <= 0 )
                {
                    const char *pEndOfMacroToken = pStartOfMacroToken;
                    while ( *pEndOfMacroToken )
                    {
                        char ch = *pEndOfMacroToken;
                        if ( !IsValidMacroNameChar( ch ) )
                        {
                            break;
                        }
                        if ( nTokenChars < MAX_MACRO_NAME - 1 )
                        {
                            macroToken[nTokenChars] = ch;
                            nTokenChars++;
                        }
                        pEndOfMacroToken++;
                    }
                    macroToken[nTokenChars] = 0;

                    // We matched a macro name so there must
                    // be some legal token chars.
                    Assert( nTokenChars > 0 );
                }

                if ( pCheck->GetNameLength() < nTokenChars )
                {
                    if ( FindMacro( macroToken ) )
                    {
                        // cannot replace this macro since it is colliding with the name of a larger macro.
                        // the iterations will converge to the correct macro.
                        continue;
                    }
                }

                pMacro = pCheck;
                break;
            }
        }
        if ( !pMacro )
        {
            continue;
        }

        if ( pMacro->HasConfigurationName() )
        {
            // property macros store a unique value for multiple configurations
            const char *configurationName = GetProjectGenerator()->GetCurrentConfigurationName();
            if ( !configurationName || !configurationName[0] )
            {
                // no current configuration
                // trying to use a property macro outside a configuration block is nonsense
                // a property macro is paired to a configuration
                VPCError( "Cannot use property macro '%s' in an expression outside of a configuration block", pMacro->GetName() );
            }

            if ( V_stricmp_fast( pMacro->GetConfigurationName(), configurationName ) )
            {
                // correct macro, but wrong configuration, get correct macro
                CMacro *pCorrectMacro = FindMacro( pMacro->GetName(), configurationName );
                if ( !pCorrectMacro )
                {
                    // script expected macro to resolve
                    VPCError( "Property macro '%s' does not have an expected configuration '%s'.", pMacro->GetName(), configurationName );
                }
                else
                {
                    // this is the correct property macro with the expected configuration
                    pMacro = pCorrectMacro;
                }
            }
        }

        if ( pOutBuff->ReplaceAt( nScanIndex - 1, pMacro->GetFullNameLength(), pMacro->GetValue(), pMacro->GetValueLength() ) )
        {
            if ( pMacrosReplaced )
            {
                pMacrosReplaced->AddToTail( pMacro->GetFullName() );
            }
            
            // We replaced the text starting from the $ so restart
            // the scan there to pick up any new macro that came in.
            nScanIndex--;
        }
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CVPC::RemoveScriptCreatedMacros()
{
	// remove all the script created macros
	// this is to ensure the next project to be processed starts out with an unpolluted state
	for ( int nMacroIndex = m_Macros.FirstInorder(); nMacroIndex != m_Macros.InvalidIndex(); )
	{
		int nNextMacroIndex = m_Macros.NextInorder( nMacroIndex );

		CMacro *pMacro = m_Macros[nMacroIndex];
		if ( !pMacro->IsSystemMacro() )
		{			
			m_Macros.RemoveAt( nMacroIndex );
			delete pMacro;
		}

		nMacroIndex = nNextMacroIndex;
	}
}

const char *CVPC::GetMacroValue( const char *pMacroName, const char *pConfigurationName )
{
	CMacro *pMacro = FindMacro( pMacroName, pConfigurationName );
	if ( pMacro )
	{
		if ( pMacro->IsPropertyMacro() && ( !pConfigurationName || !pConfigurationName[0] ) )
		{
			VPCError( "Missing required configuration to access property macro '%s'.", pMacroName );
		}

		return pMacro->GetValue();
	}

	// not found
	return "";
}

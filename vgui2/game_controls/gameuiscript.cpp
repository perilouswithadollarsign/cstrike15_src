//===== Copyright ©            Valve Corporation, All rights reserved. ======//
//
// Purpose: Defines scripting system.
//
//===========================================================================//

#include "tier0/dbg.h"
#include "gameuiscript.h"
#include "gameuiscriptinterface.h"
#include "gameuidefinition.h"
#include "keyvalues.h"
#include "fmtstr.h"


static ConVar ui_script_error_path( "ui_script_error_path", "", FCVAR_DEVELOPMENTONLY );

//-------------------------------------------------------------
//
//-------------------------------------------------------------
static void ScriptOutputFunc( const char *pszText )
{
	Msg( "%s\n", pszText );
}


//-------------------------------------------------------------
//
//-------------------------------------------------------------
static bool ScriptErrorFunc( ScriptErrorLevel_t eLevel, const char *pszText )
{
	// Attempt to parse the error for Visual Studio presentation for double-click
	if ( char const *pParse1 = StringAfterPrefix( pszText, "[string \"" ) )
	{
		if ( char const *pQuote = strchr( pParse1, '\"' ) )
		{
			if ( char const *pParse2 = StringAfterPrefix( pQuote, "\"]:" ) )
			{
				if ( char const *pEndNum = strchr( pParse2, ':' ) )
				{
					char const *szType = (eLevel == SCRIPT_LEVEL_WARNING) ? "WARNING" : "ERROR";
					Warning( "%s%.*s(%.*s): %s: %s\n",
						ui_script_error_path.GetString(),
						pQuote - pParse1, pParse1, // file name
						pEndNum - pParse2, pParse2, // line number
						szType,
						pEndNum + 1
						);
					return true;
				}
			}
		}
	}

	switch( eLevel )
	{
		case SCRIPT_LEVEL_WARNING:
			Warning( "WARNING: %s\n", pszText );
			break;

		case SCRIPT_LEVEL_ERROR:
			Warning( "ERROR: %s\n", pszText );
			break;
	}

	return true;
}

//-------------------------------------------------------------
//	Constructor
//-------------------------------------------------------------
CGameUIScript::CGameUIScript( )
{
	m_pScriptVM = GameUIScriptSystemCreate();

	m_pScriptVM->SetOutputCallback( ScriptOutputFunc );
	m_pScriptVM->SetErrorCallback( ScriptErrorFunc );

	m_IsActive = false;
	m_pGameUIScriptInterface = NULL;
	m_Version = -1;
	m_Name = "unknown";
}

//-------------------------------------------------------------
//	Destructor
//-------------------------------------------------------------
CGameUIScript::~CGameUIScript( )
{
	Shutdown();
}

void CGameUIScript::Shutdown()
{
	if ( m_pGameUIScriptInterface )
	{
		delete m_pGameUIScriptInterface;
		m_pGameUIScriptInterface = NULL;
	}
}

//-------------------------------------------------------------
// Assign this class a script file and a menu to run it on
//-------------------------------------------------------------
bool CGameUIScript::SetScript( const char *pszFileName, CGameUIDefinition *pDef )
{
	ScriptVariant_t Value;

	m_ScriptFile = pszFileName;
	m_pGameUIScriptInterface = new CGameUIScriptInterface( m_pScriptVM, pDef );
	
	if ( GameUIScriptSystemRun( m_pScriptVM, m_ScriptFile, NULL, true ) == false )
	{
		return false;
	}

	if ( !m_pScriptVM->GetValue( pDef->GetName(), &Value ) )
	{
		return false;
	}

	m_Scope = Value.m_hScript;
	// we don't release Value as that would kill m_Scope.
	

	//GetScriptName();
	GetScriptVersion();

#ifndef _DEBUG
	//if ( m_Version != build_number() && 0 )
	//{
	//	Msg( "Script %s is out of date.  Got %d.  Expected %d.\n", pszFileName, m_Version, build_number() );
	//	return false;
	//}
#endif

	return true;
}


//-------------------------------------------------------------
//
//-------------------------------------------------------------
bool CGameUIScript::GetScriptName( )
{
	HSCRIPT ExecuteFuncProc = m_pScriptVM->LookupFunction( "Name", m_Scope );
	if ( ExecuteFuncProc == NULL )
	{
		return false;
	}

	ScriptVariant_t Return;

	m_pScriptVM->Call( ExecuteFuncProc, m_Scope, true, &Return );

	m_Name = Return.m_pszString;

	m_pScriptVM->ReleaseFunction( ExecuteFuncProc );
	m_pScriptVM->ReleaseValue( Return );

	return true;
}

//-------------------------------------------------------------
//
//-------------------------------------------------------------
bool CGameUIScript::GetScriptVersion( )
{
	HSCRIPT ExecuteFuncProc = m_pScriptVM->LookupFunction( "Version", m_Scope );
	if ( ExecuteFuncProc == NULL )
	{
		return false;
	}

	ScriptVariant_t Return;

	m_pScriptVM->Call( ExecuteFuncProc, m_Scope, true, &Return );

	if ( Return.m_type == FIELD_FLOAT )
	{
		m_Version = ( int )Return.m_float;
	}
	else
	{
		m_Version = Return.m_int;
	}

	m_pScriptVM->ReleaseFunction( ExecuteFuncProc );
	m_pScriptVM->ReleaseValue( Return );

	return true;
}

//-------------------------------------------------------------
// Call a scripting function that takes an array of strings as an arg.
//-------------------------------------------------------------
bool CGameUIScript::Execute( KeyValues *pData, KeyValues **ppResult )
{
	const char *eventName = pData->GetName();
	HSCRIPT ExecuteFuncProc = m_pScriptVM->LookupFunction( eventName, m_Scope );
	if ( ExecuteFuncProc == NULL )
	{
		return false;
	}

	// Transform the key values into script scope to execute
	HSCRIPT hParams = CGameUIScriptInterface::ScriptTableFromKeyValues( m_pScriptVM, pData );
	
	ScriptVariant_t varParams = hParams, varResult;
	ScriptStatus_t ret = m_pScriptVM->ExecuteFunction( ExecuteFuncProc, &varParams, 1, &varResult, m_Scope, true );

	if ( hParams )
	{
		m_pScriptVM->ReleaseValue( varParams );
	}
	if ( ret == SCRIPT_DONE && varResult.m_type == FIELD_HSCRIPT && ppResult )
	{
		Assert( !*ppResult );	// storing return value, might overwrite caller's keyvalues
		*ppResult = CGameUIScriptInterface::ScriptVmKeyValueFromVariant( m_pScriptVM, varResult );
	}
	m_pScriptVM->ReleaseValue( varResult );
	m_pScriptVM->ReleaseFunction( ExecuteFuncProc );

	return true;
}


//===== Copyright ©            Valve Corporation, All rights reserved. ======//
//
// Purpose: Defines scripting system.
//
//===========================================================================//

#include "gameuiscriptsystem.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"
#include "filesystem.h"
#include "gameuisystemmgr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IScriptVM *GameUIScriptSystemCreate( )
{
	IScriptVM	*pScriptVM = NULL;

	ScriptLanguage_t scriptLanguage = SL_LUA;

	if( scriptLanguage != SL_NONE )
	{
		pScriptVM = g_pScriptManager->CreateVM( scriptLanguage );

		if( pScriptVM )
		{
			DevMsg("VSCRIPT: Started VScript virtual machine using script language '%s'\n", pScriptVM->GetLanguageName() );

			char SZFullPath[ MAX_PATH ];
			g_pFullFileSystem->RelativePathToFullPath( "scripts/vguiedit/modules", "GAME", SZFullPath, sizeof( SZFullPath ) );
			pScriptVM->AddSearchPath( SZFullPath );
		}
	}

	return pScriptVM;
}

HSCRIPT GameUIScriptSystemCompile( IScriptVM *pScriptVM, const char *pszScriptName, bool bWarnMissing )
{
	if ( !pScriptVM )
	{
		return NULL;
	}

	static const char *pszExtensions[] =
	{
		"",		// SL_NONE
		".gm",	// SL_GAMEMONKEY
		".nut",	// SL_SQUIRREL
		".lua", // SL_LUA
		".py",  // SL_PYTHON
	};

	const char *pszVMExtension = pszExtensions[ pScriptVM->GetLanguage() ];
	const char *pszIncomingExtension = V_strrchr( pszScriptName , '.' );
	if ( pszIncomingExtension && V_stricmp( pszIncomingExtension, pszVMExtension ) != 0 )
	{
		Msg( "Script file type does not match VM type\n" );
		return NULL;
	}

	CFmtStr scriptPath;
	if ( pszIncomingExtension )
	{
		scriptPath.sprintf( "scripts/%s", pszScriptName );
	}
	else
	{	
		scriptPath.sprintf( "scripts/%s%s", pszScriptName,  pszVMExtension );
	}

	const char *pBase;
	CUtlBuffer bufferScript;

	if ( pScriptVM->GetLanguage() == SL_PYTHON )
	{
		// python auto-loads raw or precompiled modules - don't load data here
		pBase = NULL;
	}
	else
	{
		bool bResult = g_pFullFileSystem->ReadFile( scriptPath, "GAME", bufferScript );

		if( !bResult )
		{
			Warning( "Script not found (%s) \n", scriptPath.operator const char *() );
		}

		pBase = (const char *) bufferScript.Base();

		if ( !pBase || !*pBase )
		{
			return NULL;
		}
	}

	const char *pszFilename = V_strrchr( scriptPath, '/' );
	pszFilename++;
	HSCRIPT hScript = pScriptVM->CompileScript( pBase, pszFilename );
	if ( hScript == INVALID_HSCRIPT )
	{
		DevMsg( "FAILED to compile and execute script file named %s\n", scriptPath.operator const char *() );
	}
	return hScript;
}


bool GameUIScriptSystemRun( IScriptVM *pScriptVM, const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing )
{
	HSCRIPT	hScript = GameUIScriptSystemCompile( pScriptVM, pszScriptName, bWarnMissing );
	bool bSuccess = false;
	if ( hScript != INVALID_HSCRIPT )
	{
		//		if ( gpGlobals->maxClients == 1 )
		//{
		//CBaseEntity *pPlayer = UTIL_GetLocalPlayer();
		//if ( pPlayer )
		//{
		//g_pScriptVM->SetValue( "player", pPlayer->GetScriptInstance() );
		//}
		//}
		bSuccess = ( pScriptVM->Run( hScript, hScope ) != SCRIPT_ERROR );
		if ( !bSuccess )
		{
			DevMsg( "Error running script named %s\n", pszScriptName );
		}
		pScriptVM->ReleaseScript( hScript );
	}
	return bSuccess;
}

#include <tier0/memdbgoff.h>


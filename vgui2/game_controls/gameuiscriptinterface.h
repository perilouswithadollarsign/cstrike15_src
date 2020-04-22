//===== Copyright ©            Valve Corporation, All rights reserved. ======//
//
// Purpose: Defines scripting system.
//
//===========================================================================//

#ifndef GAMEUISCRIPTINTERFACE_H
#define GAMEUISCRIPTINTERFACE_H
#ifdef _WIN32
#pragma once
#endif

#include "game_controls/igameuisystemmgr.h"
#include "gameuiscriptsystem.h"
#include "tier1/utlntree.h"
#include "utlmap.h"


class CGameUIDefinition;
class CGameGraphic;

//-----------------------------------------------------------------------------
// These are functions that can be called from lua that do things in the gameui. 
//-----------------------------------------------------------------------------
class CGameUIScriptInterface
{
public:
	explicit CGameUIScriptInterface( IScriptVM *pScriptVM, CGameUIDefinition *pDef );
	~CGameUIScriptInterface();

	int GetGraphicID( CGameGraphic *pGraphic );

public:
	void Shutdown();

	// Load a new menu to show now.
	HSCRIPT LoadMenu( const char *szMenuName, HSCRIPT hParams );

	// Create a graphic by classname
	HSCRIPT CreateGraphic( const char *szGraphicClassName, HSCRIPT hParams );


	// Execute a script command from menus
	HSCRIPT CallScript( int32 iScriptHandle, const char *szCommandName, HSCRIPT hParams );

	// Call a function inside a graphic
	HSCRIPT CallGraphic( int32 iGraphicHandle, const char *szCommandName, HSCRIPT hParams );

	// Interface with nuggets
	HSCRIPT Nugget( const char *szCommandName, HSCRIPT hParams );

public:
	enum { KV_VARIANT_SCRATCH_BUF_SIZE = 128 };
	static bool ScriptVmKeyValueToVariant( IScriptVM *pVM, KeyValues *val, ScriptVariant_t &varValue, char chScratchBuffer[KV_VARIANT_SCRATCH_BUF_SIZE] );
	static bool ScriptVmStringFromVariant( ScriptVariant_t &varValue, char chScratchBuffer[KV_VARIANT_SCRATCH_BUF_SIZE] );
	static KeyValues * ScriptVmKeyValueFromVariant( IScriptVM *pVM, ScriptVariant_t &varValue );
	static KeyValues * ScriptTableToKeyValues( IScriptVM *pVM, char const *szName, HSCRIPT hTable );
	static HSCRIPT ScriptTableFromKeyValues( IScriptVM *pVM, KeyValues *kv );

private:
	HSCRIPT		m_Scope;
	IScriptVM	*m_pScriptVM;

	CGameUIDefinition *m_pMenu;

	CUtlMap< CUtlString, IGameUIScreenController * > m_Nuggets;
	CUtlMap< CUtlString, CGameGraphic * > m_GraphicScriptInstances;	// graphics created from scripting.
};


#endif // GAMEUISCRIPTINTERFACE_H

//===== Copyright ©            Valve Corporation, All rights reserved. ======//
//
// Purpose: Defines gameui scripting system.
//
//===========================================================================//

#ifndef GAMEUISCRIPT_H
#define GAMEUISCRIPT_H
#ifdef _WIN32
#pragma once
#endif

#include "gameuiscriptsystem.h"

class CGameUIScriptInterface;
class CGameUIDefinition;
class KeyValues;


class CGameUIScript
{
public:
	CGameUIScript( );
	~CGameUIScript( );

	void Shutdown();

	IScriptVM		*GetVM( ) { return m_pScriptVM; }
	CUtlString		&GetName( ) { return m_Name; }
	int				GetVersion( ) { return m_Version; }
	CUtlString		&GetScriptFile( ) { return m_ScriptFile; }
	bool			IsActive( ) { return m_IsActive; }

	bool	SetScript( const char *pszFileName, CGameUIDefinition *pDef );
	void	SetActive( bool IsActive ) { m_IsActive = IsActive; }

	bool   Execute( KeyValues *pData, KeyValues **ppResult );

	CGameUIScriptInterface	* GetScriptInterface() const { return m_pGameUIScriptInterface; }


private:
	bool	GetScriptName( );
	bool	GetScriptType( );
	bool	GetScriptVersion( );

	CGameUIScriptInterface	*m_pGameUIScriptInterface;

	CUtlString				m_Name;
	int						m_Version;
	bool					m_IsActive;
	CUtlString				m_ScriptFile;
	IScriptVM				*m_pScriptVM;
	HSCRIPT					m_Scope;
};

#endif // GAMEUISCRIPT_H

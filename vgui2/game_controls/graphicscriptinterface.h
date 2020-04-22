//===== Copyright ©            Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef GRAPHICSCRIPTINTERFACE_H
#define GRAPHICSCRIPTINTERFACE_H
#ifdef _WIN32
#pragma once
#endif

#include "gameuiscriptsystem.h"


class CGameGraphic;

//-----------------------------------------------------------------------------
// These are functions that can be called from lua that do things to graphic classes. 
//-----------------------------------------------------------------------------
class CGraphicScriptInterface
{
public:
	CGraphicScriptInterface( IScriptVM *pScriptVM );
	void InstallGraphic( CGameGraphic *pGraphic );

	HSCRIPT GetScope( ) { return m_Scope; }

private:
	// private functions to support scripting
	//CGameGraphic *FindGraphic( int nID );

public:
	// exposed functions to scripting
	void PlayAnim( const char *pAnimName );

private:
	void SetScope( HSCRIPT Scope ) { m_Scope = Scope; }

	CGameGraphic *m_pGraphic;
	HSCRIPT		m_Scope;
	IScriptVM	*m_pScriptVM;

};


#endif // GRAPHICSCRIPTINTERFACE_H

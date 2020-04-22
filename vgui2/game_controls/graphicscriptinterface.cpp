//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "graphicscriptinterface.h"
#include "gamegraphic.h"
#include "gameuisystemmgr.h"

BEGIN_SCRIPTDESC_ROOT_NAMED( CGraphicScriptInterface, "CGraphicScriptInterface", SCRIPT_SINGLETON "" )
DEFINE_SCRIPTFUNC( PlayAnim, "Play an animation by name" )
END_SCRIPTDESC()

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGraphicScriptInterface::CGraphicScriptInterface( IScriptVM *pScriptVM )
{
	m_pScriptVM = pScriptVM;
	m_pGraphic = NULL;

	HSCRIPT Scope = m_pScriptVM->RegisterInstance( this, "Graphic" );
	SetScope( Scope );
}

//-----------------------------------------------------------------------------
// Tell this script what graphic it belongs to.
//-----------------------------------------------------------------------------
void CGraphicScriptInterface::InstallGraphic( CGameGraphic *pGraphic )
{
	m_pGraphic = pGraphic;
}

//-----------------------------------------------------------------------------
// Play an animation on the graphic.
//-----------------------------------------------------------------------------
void CGraphicScriptInterface::PlayAnim( const char *pAnimName )
{
	Assert( m_pGraphic );
	if ( !m_pGraphic->HasState( pAnimName ) )
	{
		Warning( "Unable to find state %s for graphic %s\n", pAnimName, m_pGraphic->GetName() );
		return;
	}
	m_pGraphic->SetState( pAnimName );
}








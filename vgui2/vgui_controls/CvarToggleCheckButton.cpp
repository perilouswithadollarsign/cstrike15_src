//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "tier1/keyvalues.h"

#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/CvarToggleCheckButton.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

vgui::Panel *Create_CvarToggleCheckButton()
{
	return new CvarToggleCheckButton< ConVarRef >( NULL, NULL );
}

DECLARE_BUILD_FACTORY_CUSTOM_ALIAS( CvarToggleCheckButton<ConVarRef>, CvarToggleCheckButton, Create_CvarToggleCheckButton );


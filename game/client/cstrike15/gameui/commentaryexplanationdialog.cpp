//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "commentaryexplanationdialog.h"
#include "basepanel.h"
#include "convar.h"
#include "engineinterface.h"
#include "gameui_interface.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"

#include <stdio.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommentaryExplanationDialog::CCommentaryExplanationDialog(vgui::Panel *parent, char *pszFinishCommand) : BaseClass(parent, "CommentaryExplanationDialog")
{
	SetDeleteSelfOnClose(true);
	SetSizeable( false );

	input()->SetAppModalSurface(GetVPanel());

	LoadControlSettings("Resource/CommentaryExplanationDialog.res");

	MoveToCenterOfScreen();

	GameUI().PreventEngineHideGameUI();

	// Save off the finish command
	Q_snprintf( m_pszFinishCommand, sizeof( m_pszFinishCommand ), "%s", pszFinishCommand );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommentaryExplanationDialog::~CCommentaryExplanationDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommentaryExplanationDialog::OnKeyCodePressed(KeyCode code)
{
	if (code == KEY_ESCAPE)
	{
		Close();
	}
	else
	{
		BaseClass::OnKeyCodePressed(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: handles button commands
//-----------------------------------------------------------------------------
void CCommentaryExplanationDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "ok" ) )
	{
		Close();
		BasePanel()->FadeToBlackAndRunEngineCommand( m_pszFinishCommand );
	}
	else if ( !stricmp( command, "cancel" ) || !stricmp( command, "close" ) )
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommentaryExplanationDialog::OnClose( void )
{
	BaseClass::OnClose();
	GameUI().AllowEngineHideGameUI();
}

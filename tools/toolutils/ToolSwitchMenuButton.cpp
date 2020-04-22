//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "toolutils/toolswitchmenubutton.h"
#include "vgui_controls/panel.h"
#include "toolutils/toolmenubutton.h"
#include "toolutils/enginetools_int.h"
#include "tier1/keyvalues.h"
#include "vgui_controls/menu.h"
#include "toolframework/ienginetool.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ListPanel.h"
#include "filesystem.h"
#include "tier1/fmtstr.h"
#include "toolframework/itoolframework.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

class CLoadToolDialog : public Frame
{
	DECLARE_CLASS_SIMPLE( CLoadToolDialog, Frame );
public:

	CLoadToolDialog( Panel *panel );

protected:

	virtual void OnCommand( const char *pCommand );

	ListPanel *m_pModuleList;
	Button *m_pLoadButton;
	Button *m_pCancelButton;
};

CLoadToolDialog::CLoadToolDialog( Panel *panel ) : 
	BaseClass( panel, "LoadToolDialog" )
{
	m_pModuleList = new ListPanel( this, "ModuleList" );
	m_pModuleList->AddActionSignalTarget( this );
	m_pModuleList->AddColumnHeader( 0, "text", "Text", 250, ListPanel::COLUMN_RESIZEWITHWINDOW );
	m_pModuleList->SetMultiselectEnabled( true );

	m_pLoadButton = new Button( this, "OkButton", "#VGui_OK", this, "Ok" );
	m_pCancelButton = new Button( this, "Cancel", "#vgui_Cancel", this, "Cancel" );
	SetSize( 300, 400 );
	SetSizeable( true );
	SetMoveable( true );

	// Iterate loadable tools
	// Search the directory structure.
	char searchpath[ MAX_PATH ];
	Q_strncpy( searchpath, "tools/*.dll", sizeof(searchpath) );

	CUtlVector< CUtlString > toolmodules;

	FileFindHandle_t handle;
	char const *findfn = g_pFullFileSystem->FindFirstEx( searchpath, "EXECUTABLE_PATH", &handle );
	while ( findfn )
	{
		if ( !g_pFullFileSystem->FindIsDirectory( handle ) )
		{
			char sz[ MAX_PATH ] = { 0 };
			Q_FileBase( findfn, sz, sizeof( sz ) );

			toolmodules.AddToTail( CUtlString( sz ) );
		}
		findfn = g_pFullFileSystem->FindNext( handle );
	}

	g_pFullFileSystem->FindClose( handle );

	for ( int i = 0; i < toolmodules.Count(); ++i )
	{
		KeyValues *item = new KeyValues( "item" );

		item->SetString( "text", toolmodules[ i ].String() );

		m_pModuleList->AddItem( item, 0, false, false );
	}

	LoadControlSettings( "resource/loadtooldialog.res" );
}

void CLoadToolDialog::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		KeyValues *pActionKeys = new KeyValues( "OnLoadTools" );
		int nCount = m_pModuleList->GetSelectedItemsCount();
		pActionKeys->SetInt( "count", nCount );
		for ( int i = 0; i < nCount; ++i )
		{
			int itemID = m_pModuleList->GetSelectedItem( i );
			KeyValues *item = m_pModuleList->GetItem( itemID );
			pActionKeys->SetString( CFmtStr( "%i", i ), item->GetString( "text" ) );
		}

		CloseModal();
		PostActionSignal( pActionKeys );
		return;
	}

	if ( !Q_stricmp( pCommand, "Cancel" ) )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( pCommand );
}
//-----------------------------------------------------------------------------
// Menu to switch between tools
//-----------------------------------------------------------------------------
class CToolSwitchMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CToolSwitchMenuButton, CToolMenuButton );

public:
	CToolSwitchMenuButton( Panel *parent, const char *panelName, const char *text, Panel *pActionTarget );
	virtual void OnShowMenu(Menu *menu);

protected:

	MESSAGE_FUNC( OnShowLoadToolDialog, "OnShowLoadToolDialog" );
	MESSAGE_FUNC_PARAMS( OnLoadTools, "OnLoadTools", params );

	DHANDLE< CLoadToolDialog >	 m_hLoadToolDialog;
};

//-----------------------------------------------------------------------------
// Global function to create the switch menu
//-----------------------------------------------------------------------------
CToolMenuButton* CreateToolSwitchMenuButton( Panel *parent, const char *panelName, const char *text, Panel *pActionTarget )
{
	return new CToolSwitchMenuButton( parent, panelName, text, pActionTarget );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolSwitchMenuButton::CToolSwitchMenuButton( Panel *parent, const char *panelName, const char *text, Panel *pActionTarget ) :
	BaseClass( parent, panelName, text, pActionTarget )
{
	SetMenu(m_pMenu);
}


void CToolSwitchMenuButton::OnShowLoadToolDialog()
{
	m_hLoadToolDialog = new CLoadToolDialog( this );
	m_hLoadToolDialog->MoveToCenterOfScreen();
	m_hLoadToolDialog->AddActionSignalTarget( this );
	m_hLoadToolDialog->DoModal();
}

void CToolSwitchMenuButton::OnLoadTools( KeyValues *params )
{
	int nCount = params->GetInt( "count", 0 );
	for ( int i = 0; i < nCount; ++i )
	{
		char const *pToolName = params->GetString( CFmtStr( "%i", i ), "" );
		if ( pToolName && *pToolName )
		{
			// Load it
			enginetools->LoadToolModule( pToolName, false );
		}
	}
}

//-----------------------------------------------------------------------------
// Is called when the menu is made visible
//-----------------------------------------------------------------------------
void CToolSwitchMenuButton::OnShowMenu(Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	Reset();

	int c = enginetools->GetToolCount();
	for ( int i = 0 ; i < c; ++i )
	{
		char const *toolname = enginetools->GetToolName( i );

		char toolcmd[ 32 ];
		Q_snprintf( toolcmd, sizeof( toolcmd ), "OnTool%i", i );

		int id = AddCheckableMenuItem( toolname, toolname, new KeyValues ( "Command", "command", toolcmd ), m_pActionTarget );
		m_pMenu->SetItemEnabled( id, true );
		m_pMenu->SetMenuItemChecked( id, enginetools->IsTopmostTool( enginetools->GetToolSystem( i ) ) );
	}

	m_pMenu->AddSeparator();
	m_pMenu->AddMenuItem( "#ToolShowLoadToolDialog", new KeyValues( "OnShowLoadToolDialog" ), this );
}

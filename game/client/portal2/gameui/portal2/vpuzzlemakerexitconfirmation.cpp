//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"

#if defined( PORTAL2_PUZZLEMAKER )

#include "vpuzzlemakerexitconfirmation.h"
#include "VFooterPanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/imagepanel.h"
#include "vgui/ISurface.h"
#include "vpuzzlemakersavedialog.h"
#include "vgenericconfirmation.h"
#include "puzzlemaker/puzzlemaker.h"
#include "vpuzzlemakermychambers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

extern void ExitPuzzleMaker();

CPuzzleMakerExitConfirmation::CPuzzleMakerExitConfirmation( Panel *parent, const char *panelName )
							: BaseClass( parent, panelName, true, true, false ),
							  m_pLblMessage( NULL ),
							  m_eReason( EXIT_REASON_EXIT_FROM_VGUI )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );
	SetUseAlternateTiles( true );

	SetTitle( "", false );
	SetMoveable( false );

	m_hMessageFont = vgui::INVALID_FONT;

	m_nTextOffsetX = 0;
	m_nIconOffsetY = 0;

	SetFooterEnabled( true );

	m_bValid = false;
}


CPuzzleMakerExitConfirmation::~CPuzzleMakerExitConfirmation()
{
	delete m_pLblMessage;
}

void CPuzzleMakerExitConfirmation::SetReason( ExitConfirmationReason_t eReason )
{
	m_eReason = eReason;
}

void CPuzzleMakerExitConfirmation::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hMessageFont = pScheme->GetFont( pScheme->GetResourceString( "ConfirmationDialog.TextFont" ), true );

	m_nTextOffsetX = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "ConfirmationDialog.TextOffsetX" ) ) );
	m_nIconOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "ConfirmationDialog.IconOffsetY" ) ) );

	m_bValid = true;

	FixLayout();
	UpdateFooter();
	GameUI().PreventEngineHideGameUI();

}


void CPuzzleMakerExitConfirmation::OnCommand(const char *command)
{
	if ( Q_stricmp( command, "OK" ) == 0 )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if ( Q_stricmp( command, "cancel" ) == 0 )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if ( Q_stricmp( command, "discard" ) == 0 )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_X, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
}


void CPuzzleMakerExitConfirmation::OnKeyCodePressed( KeyCode keycode )
{
	int userId = GetJoystickForCode( keycode );
	vgui::KeyCode code = GetBaseButtonCode( keycode );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( userId );

	switch ( code )
	{
	case KEY_XBUTTON_A: //Save
		{
			//Save puzzle and return to test chamber menu
			PuzzleMakerExitSave();
		}
		break;

	case KEY_XBUTTON_B: //Cancel
		{
			//Go back to escape menu
			PuzzleMakerExitCancel();
		}
		break;

	case KEY_XBUTTON_X: //Discard
		{
			//Confirm loss of data
			PuzzleMakerExitDiscard();
		}
		break;

	default:
		BaseClass::OnKeyCodePressed(keycode);
		break;
	}
}


#ifndef _GAMECONSOLE
void CPuzzleMakerExitConfirmation::OnKeyCodeTyped( vgui::KeyCode code )
{
	// For PC, this maps space bar and enter to save and esc to cancel
	switch ( code )
	{
	case KEY_SPACE:
	case KEY_ENTER:
		return OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );

	case KEY_ESCAPE:
		return OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}

	BaseClass::OnKeyTyped( code );
}
#endif


void CPuzzleMakerExitConfirmation::OnOpen()
{
	BaseClass::OnOpen();

	UpdateFooter();

	m_bNeedsMoveToFront = true;

	g_pPuzzleMaker->TakeScreenshotAsync( NULL, false );
}


void CPuzzleMakerExitConfirmation::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int buttons = FB_ABUTTON|FB_XBUTTON|FB_BBUTTON;

		pFooter->SetButtons( buttons, FF_ABXYDL_ORDER, FOOTER_MENUS );
		pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PuzzleMaker_SavePuzzle", false, FOOTER_MENUS );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel", false, FOOTER_MENUS );
		pFooter->SetButtonText( FB_XBUTTON, "#PORTAL2_PuzzleMaker_DiscardChanges", false, FOOTER_MENUS );
	}
}

void CPuzzleMakerExitConfirmation::FixLayout()
{
	if ( !m_bValid )
	{
		//Want to delay this until ApplySchemeSettings() gets called
		return;
	}

	m_pLblMessage = new Label( this, "LblMessage", "#PORTAL2_PuzzleMaker_ExitPuzzleMakerConfMsg" );
	if ( m_pLblMessage )
	{
		m_pLblMessage->SetFont( m_hMessageFont );

		if ( UsesAlternateTiles() )
		{
			m_pLblMessage->SetFgColor( Color( 201, 211, 207, 255 ) );
		}
	}

	int screenWidth, screenHeight;
	CBaseModPanel::GetSingleton().GetSize( screenWidth, screenHeight );

	int nTileWidth, nTileHeight;
	GetDialogTileSize( nTileWidth, nTileHeight );

	int nDesiredDialogWidth = 7 * nTileWidth;
	int nDesiredDialogHeight = 2 * nTileHeight;

	int nMsgWide = 0;
	int nMsgTall = 0;
	if ( m_pLblMessage )
	{
		//Account for the size of the message
		m_pLblMessage->GetContentSize( nMsgWide, nMsgTall );
		if ( nMsgWide > nDesiredDialogWidth )
		{
			m_pLblMessage->SetWrap( true );
			m_pLblMessage->SetWide( nDesiredDialogWidth );
			m_pLblMessage->SetTextInset( 0, 0 );
			m_pLblMessage->GetContentSize( nMsgWide, nMsgTall );
		}
	}

	//Account for one column due to icon plus spacing
	int nRequiredDialogWidth = nMsgWide + nTileWidth + nTileWidth/2;
	int nRequiredDialogHeight = nMsgTall + nTileHeight/2;

	if ( nDesiredDialogWidth < nRequiredDialogWidth )
	{
		nDesiredDialogWidth = nRequiredDialogWidth;
	}

	if ( nDesiredDialogHeight < nRequiredDialogHeight )
	{
		nDesiredDialogHeight = nRequiredDialogHeight;
	}

	int nDialogTileWidth = ( nDesiredDialogWidth + nTileWidth - 1 )/nTileWidth;
	int nDialogTileHeight = ( nDesiredDialogHeight + nTileHeight - 1 )/nTileHeight;

	//Set size in tile units
	SetDialogTitle( "#PORTAL2_PuzzleMaker_UnsavedChanges", NULL, false, nDialogTileWidth, nDialogTileHeight );
	SetupAsDialogStyle();

	int x, y, nActualDialogWidth, nActualDialogHeight;
	GetBounds( x, y, nActualDialogWidth, nActualDialogHeight );

	if ( m_pLblMessage )
	{
		//Center the message
		int msgWide, msgTall;
		m_pLblMessage->GetContentSize( msgWide, msgTall );

		//Account for the possible dialog title
		y = ( ( nActualDialogHeight - nTileHeight ) - msgTall ) / 2;

		m_pLblMessage->SetBounds( nTileWidth + m_nTextOffsetX, y, msgWide, msgTall );

		vgui::ImagePanel *pIcon = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "InfoIcon" ) );
		if ( pIcon )
		{
			vgui::HFont hFont = m_pLblMessage->GetFont();
			int nFontTall = 0;
			if ( hFont != vgui::INVALID_FONT )
			{
				nFontTall = vgui::surface()->GetFontTall( hFont );
			}

			int nIconX, nIconY, nIconWide, nIconTall;
			pIcon->GetBounds( nIconX, nIconY, nIconWide, nIconTall );
			pIcon->SetPos( nIconX, y + ( nFontTall - nIconTall ) / 2 + m_nIconOffsetY );
		}
	}

	if ( !g_pPuzzleMaker->HasUnsavedChanges() )
	{
		PuzzleMakerExitDiscard();
	}
}

void CPuzzleMakerExitConfirmation::PaintBackground()
{
	BaseClass::PaintBackground();

	if ( UsesAlternateTiles() )
	{
		m_pLblMessage->SetFgColor( Color( 201, 211, 207, 255 ) );
	}

	if ( m_bNeedsMoveToFront )
	{
		vgui::ipanel()->MoveToFront( GetVPanel() );
		m_bNeedsMoveToFront = false;
	}
}


void CPuzzleMakerExitConfirmation::PuzzleMakerExitSave()
{
	GameUI().PreventEngineHideGameUI();

	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

	CPuzzleMakerSaveDialog *pSaveDialog = static_cast<CPuzzleMakerSaveDialog*>(CBaseModPanel::GetSingleton().OpenWindow( WT_PUZZLEMAKERSAVEDIALOG, this, true ));
	
	if( m_eReason == EXIT_REASON_OPEN )
	{
		pSaveDialog->SetReason( PUZZLEMAKER_SAVE_FROM_OPEN_CHAMBER );
	}
	else if( m_eReason == EXIT_REASON_NEW )
	{
		pSaveDialog->SetReason( PUZZLEMAKER_SAVE_FROM_NEW_CHAMBER );
	}
	else if( m_eReason == EXIT_REASON_QUIT_GAME )
	{
		pSaveDialog->SetReason( PUZZLEMAKER_SAVE_ON_QUIT_APP );
	}
	else if( m_eReason == EXIT_REASON_JOIN_COOP_GAME )
	{
		pSaveDialog->SetReason( PUZZLEMAKER_SAVE_JOIN_COOP_GAME );
	}
	else
	{
		pSaveDialog->SetReason( PUZZLEMAKER_SAVE_ONEXIT );
	}

}


void CPuzzleMakerExitConfirmation::PuzzleMakerExitDiscard()
{
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );


	if( m_eReason == EXIT_REASON_NEW )
	{
		g_pPuzzleMaker->NewPuzzle( false );
		GameUI().AllowEngineHideGameUI();
		engine->ExecuteClientCmd("gameui_hide");
		CBaseModPanel::GetSingleton().CloseAllWindows();
	}
	else if( m_eReason == EXIT_REASON_OPEN )
	{
		// Open our chambers dialog, making sure to note that we're inside the editor (handles autosaves differently)
		KeyValues *pSettings = new KeyValues( "settings" );
		KeyValues::AutoDelete autoDelete_settings( pSettings );
		pSettings->SetBool( "ineditor", true );

		BASEMODPANEL_SINGLETON.OpenWindow( BaseModUI::WT_EDITORCHAMBERLIST, this, true, pSettings );
	}
	else if ( m_eReason == EXIT_REASON_QUIT_GAME )
	{
		g_pPuzzleMaker->QuitGame();
	}
	else if ( m_eReason == EXIT_REASON_JOIN_COOP_GAME )
	{
		CUIGameData::Get()->Invite_Approved();
		ExitPuzzleMaker();
	}
	else
	{
		ExitPuzzleMaker();
	}
}


void CPuzzleMakerExitConfirmation::PuzzleMakerExitCancel()
{
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
	// Close all the way out if we're not coming from a VGUI command
	switch( m_eReason )
	{
	case EXIT_REASON_JOIN_COOP_GAME:
		CUIGameData::Get()->Invite_Declined();

	case EXIT_REASON_NEW:
	case EXIT_REASON_EXIT_FROM_PUZZLEMAKER_UI:
	case EXIT_REASON_OPEN:
	case EXIT_REASON_QUIT_GAME:
		GameUI().AllowEngineHideGameUI();
		engine->ExecuteClientCmd("gameui_hide");
		CBaseModPanel::GetSingleton().CloseAllWindows();
		break;
		
	default:
		if ( !NavigateBack() )
		{
			Close();
		}
		break;
	}
}


void cc_puzzlemaker_quit( const CCommand &args )
{
	Assert( args.ArgC() == 2 );
	if ( args.ArgC() <= 1 )
		return;

	PuzzleMakerQuitReason_t reason = PuzzleMakerQuitReason_t( atoi( args[1] ) );

	//If we have been asked to quit the app then close all other UI windows so we can show the confirmation dialog
	if ( PUZZLEMAKER_QUIT_APPLICATION )
	{
		//If the confirmation dialog is already up then don't do anything
		CPuzzleMakerExitConfirmation *pOldExitConfirmation = static_cast<CPuzzleMakerExitConfirmation*>( BASEMODPANEL_SINGLETON.GetWindow( WT_PUZZLEMAKEREXITCONRFIRMATION ) );
		if ( pOldExitConfirmation && pOldExitConfirmation->IsVisible() )
		{
			pOldExitConfirmation->SetReason( EXIT_REASON_QUIT_GAME );
			return;
		}

		engine->ExecuteClientCmd( "puzzlemaker_show 1" );

		//Cancel any compile
		if ( g_pPuzzleMaker->IsCompiling() )
		{
			g_pPuzzleMaker->CancelCompile();
		}
		//GameUI().AllowEngineHideGameUI();
		//engine->ExecuteClientCmd("gameui_hide");
		CBaseModPanel::GetSingleton().CloseAllWindows();
	}

	//Bring up the pause menu
	engine->ExecuteClientCmd("gameui_activate");
	BaseModUI::CBaseModFrame *pInGameMenu = BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_INGAMEMAINMENU, NULL, true );

	//Bring up the confirmation dialog
	CPuzzleMakerExitConfirmation* pExitConfirmation = static_cast<CPuzzleMakerExitConfirmation*>(BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_PUZZLEMAKEREXITCONRFIRMATION, pInGameMenu, true ));
	ExitConfirmationReason_t exitReason(EXIT_REASON_EXIT_FROM_PUZZLEMAKER_UI);
	switch( reason )
	{
	case PUZZLEMAKER_QUIT_TO_MAINMENU:
		exitReason = EXIT_REASON_EXIT_FROM_PUZZLEMAKER_UI;
		break;
	case PUZZLEMAKER_QUIT_APPLICATION:
		exitReason = EXIT_REASON_QUIT_GAME;
		break;
	case PUZZLEMAKER_QUIT_TO_ACCEPT_COOP_INVITE:
		exitReason = EXIT_REASON_JOIN_COOP_GAME;
		break;
	default:
			Assert( !"Unknown puzzlemaker exit reason.  Defaulting to exit puzzlemaker only." );
	}
	pExitConfirmation->SetReason( exitReason );
}
static ConCommand puzzlemaker_quit("puzzlemaker_quit", cc_puzzlemaker_quit );


void cc_puzzlemaker_new_chamber( const CCommand &args )
{
	// It's a fullscreen hud element, so we don't need to activate it for other slots
	if ( GET_ACTIVE_SPLITSCREEN_SLOT() != 0 )
		return;

	if ( g_pPuzzleMaker->HasUnsavedChanges() )
	{
		engine->ExecuteClientCmd("gameui_activate");
		BaseModUI::CBaseModFrame *pInGameMenu = BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_INGAMEMAINMENU, NULL, true );
		GameUI().PreventEngineHideGameUI();

		CPuzzleMakerExitConfirmation* pExitConfirmation = static_cast<CPuzzleMakerExitConfirmation*>(BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_PUZZLEMAKEREXITCONRFIRMATION, pInGameMenu, true ));
		pExitConfirmation->SetReason( EXIT_REASON_NEW );
	}
	else // No pending changes.  Just make a new puzzle
	{
		g_pPuzzleMaker->NewPuzzle( false );
	}
}
static ConCommand puzzlemaker_new_chamber("puzzlemaker_new_chamber", cc_puzzlemaker_new_chamber );



#endif //PORTAL2_PUZZLEMAKER

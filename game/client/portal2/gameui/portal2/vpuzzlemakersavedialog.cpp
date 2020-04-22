//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//
#if defined( PORTAL2_PUZZLEMAKER )

#include "cbase.h"

#include "vpuzzlemakersavedialog.h"
#include "vfooterpanel.h"
#include "vgui_controls/textentry.h"
#include "vgui/ilocalize.h"
#include "vgenericconfirmation.h"
#include "vgui/isurface.h"
#include "puzzlemaker/puzzlemaker.h"
#include "vgui_controls/label.h"
#include "gameui/portal2/vdialoglistbutton.h"
#include "vgui_controls/imagepanel.h"
#include "imageutils.h"
#include "vpuzzlemakerpublishprogress.h"
#include "vpuzzlemakermychambers.h"
#include "vingamemainmenu.h"
#include <vgui/IInput.h>
#include "vgui_controls/checkbutton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

const int MAX_TESTCHAMBER_NAME_LENGTH = 128;
const int MAX_TESTCHAMBER_DESCRIPTION_LENGTH = 1024;

void ExitPuzzleMaker()
{
	InGameMainMenu* pMainMenu = 
	static_cast< InGameMainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_INGAMEMAINMENU ) );

	if ( pMainMenu )
	{
		pMainMenu->Close();
	}

	// FIXME: This is here because the disconnect below this code will force a window up, which will confused the state of the background movie. This ensures that the intent is
	//		  available when that decision is made.
	CBaseModPanel::GetSingleton().MoveToEditorMainMenu();

	//GameUI().HideGameUI();
	engine->ExecuteClientCmd( "gameui_hide" );

	// All done!
	if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		// Closing an active session results in disconnecting from the game.
		g_pMatchFramework->CloseSession();
	}
	else
	{
		// On PC people can be playing via console bypassing matchmaking
		// and required session settings, so to leave game duplicate
		// session closure with an extra "disconnect" command.
		engine->ExecuteClientCmd( "disconnect" );
	}

	engine->ExecuteClientCmd( "puzzlemaker_show 0" );
	g_pPuzzleMaker->SetActive( false );

	//GameUI().ActivateGameUI();
	//GameUI().AllowEngineHideGameUI();
	engine->ExecuteClientCmd( "gameui_activate" );

	CBaseModPanel::GetSingleton().CloseAllWindows();
	CBaseModPanel::GetSingleton().MoveToEditorMainMenu();
	CBaseModPanel::GetSingleton().OpenFrontScreen();
}


CPuzzleMakerSaveDialog::CPuzzleMakerSaveDialog( Panel *pParent, const char *pPanelName )
					  : BaseClass( pParent, pPanelName ),
					    m_pChamberNameTextEntry( NULL ),
						m_pChamberDescriptionTextEntry( NULL ),
						m_pWorkshopAgreementCheckBox( NULL ),
						m_pWorkshopAgreementLabel( NULL ),
						m_eReason( PUZZLEMAKER_SAVE_UNSPECIFIED ),
						m_pwscDefaultChamberName( NULL ),
						m_pwszDefaultChamberDescription( NULL ),
						m_bFirstTime( true ),
						m_pPublishButtonLabel( NULL ),
						m_pPublishListButton( NULL ),
						m_pPublishDescriptionLabel( NULL ),
						m_pChamberImagePanel( NULL ),
						m_nScreenshotID( -1 ),
						m_eVisibility( k_ERemoteStoragePublishedFileVisibilityPublic )
{
	V_memset( m_szScreenshotName, 0, sizeof( m_szScreenshotName ) );
	GameUI().PreventEngineHideGameUI();

	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#PORTAL2_PuzzleMaker_SavePuzzle" );

	SetFooterEnabled( true );
	UpdateFooter();
}


CPuzzleMakerSaveDialog::~CPuzzleMakerSaveDialog()
{
	GameUI().AllowEngineHideGameUI();

	if ( surface() && m_nScreenshotID != -1 )
	{
		surface()->DestroyTextureID( m_nScreenshotID );
	}
}


void CPuzzleMakerSaveDialog::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_TextEntryBoxFocusFgColor = GetSchemeColor( "HybridButton.FocusColorAlt", pScheme );
	m_TextEntryBoxFocusBgColor = GetSchemeColor( "HybridButton.MouseOverCursorColorAlt", pScheme );
	m_TextEntryBoxNonFocusFgColor = GetSchemeColor( "HybridButton.TextColorAlt", pScheme );
	m_TextEntryBoxNonFocusBgColor = Color( 0, 0, 0, 64 );

	m_pwscDefaultChamberName = g_pVGuiLocalize->Find( "PORTAL2_PuzzleMaker_SaveChamberNameDefault" );
	m_pwszDefaultChamberDescription = g_pVGuiLocalize->Find( "PORTAL2_PuzzleMaker_SaveChamberDescriptionDefault" );

	m_pChamberNameTextEntry = static_cast<TextEntry*>( FindChildByName( "TextEntryChamberName" ) );
	if ( m_pChamberNameTextEntry )
	{
		m_pChamberNameTextEntry->SetFont( pScheme->GetFont( "FriendsListStatusLine", IsProportional() ) );
		m_pChamberNameTextEntry->SetMultiline( false );
		m_pChamberNameTextEntry->SetMaximumCharCount( MAX_TESTCHAMBER_NAME_LENGTH );
		m_pChamberNameTextEntry->SelectAllOnFirstFocus( true );
		m_pChamberNameTextEntry->SetSelectionTextColor( m_TextEntryBoxNonFocusFgColor );
		m_pChamberNameTextEntry->SetPinCorner( PIN_NO, 0, 0 );
	}

	m_pChamberDescriptionTextEntry = static_cast<TextEntry*>( FindChildByName( "TextEntryChamberDescription" ) );
	if ( m_pChamberDescriptionTextEntry )
	{
		m_pChamberDescriptionTextEntry->SetFont( pScheme->GetFont( "FriendsListStatusLine", IsProportional() ) );
		m_pChamberDescriptionTextEntry->SetMultiline( true );
		m_pChamberDescriptionTextEntry->SetCatchEnterKey( true );
		m_pChamberDescriptionTextEntry->SetMaximumCharCount( MAX_TESTCHAMBER_DESCRIPTION_LENGTH );
		m_pChamberDescriptionTextEntry->SelectAllOnFirstFocus( true );
		m_pChamberDescriptionTextEntry->SetSelectionTextColor( m_TextEntryBoxNonFocusFgColor );
		m_pChamberDescriptionTextEntry->SetPinCorner( PIN_NO, 0, 0 );
	}

	m_pPublishButtonLabel = static_cast<Label*>( FindChildByName( "LblPublishButtonTitle" ) );
	if ( m_pPublishButtonLabel )
	{
		m_pPublishButtonLabel->SetVisible( false );
	}

	m_pPublishListButton = static_cast<CDialogListButton*>( FindChildByName( "ListBtnPublishZone" ) );
	if ( m_pPublishListButton )
	{
		m_pPublishListButton->SetVisible( false );
		m_pPublishListButton->SetCurrentSelectionIndex( 0 );
		m_pPublishListButton->SetArrowsAlwaysVisible( IsPC() );
		m_pPublishListButton->SetCanWrap( true );
		m_pPublishListButton->SetDrawAsDualStateButton( false );
	}

	m_pPublishDescriptionLabel = static_cast<Label*>( FindChildByName( "LblPublishDescription" ) );
	if ( m_pPublishDescriptionLabel )
	{
		m_pPublishDescriptionLabel->SetVisible( false );
	}

	m_pChamberImagePanel = static_cast<ImagePanel*>( FindChildByName( "ImgChamberThumb" ) );
	if ( m_pChamberImagePanel )
	{
		m_pChamberImagePanel->SetVisible( false );
	}

	m_pWorkshopAgreementLabel = static_cast<Label*>( FindChildByName( "LblWorkshopAgreement" ) );
	if ( m_pWorkshopAgreementLabel )
	{
		m_pWorkshopAgreementLabel->SetVisible( false );
	}

	m_pWorkshopAgreementCheckBox = static_cast<CheckButton*>( FindChildByName( "ChckBxWorkshopAgreement" ) );
	if ( m_pWorkshopAgreementCheckBox )
	{
		m_pWorkshopAgreementCheckBox->SetVisible( false );
	}

	InitializeText();

	UpdateFooter();
}


void CPuzzleMakerSaveDialog::Activate()
{
	BaseClass::Activate();

	UpdateFooter();
}


void CPuzzleMakerSaveDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_eReason == PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE )
	{
		if ( m_pPublishButtonLabel )
		{
			m_pPublishButtonLabel->SetVisible( true );
		}

		if ( m_pPublishListButton )
		{
			m_pPublishListButton->SetVisible( true );
			m_pPublishListButton->SetCurrentSelectionIndex( 0 );
		}

		if ( m_pPublishDescriptionLabel )
		{
			m_pPublishDescriptionLabel->SetVisible( true );
			m_pPublishDescriptionLabel->SetText( "#PORTAL2_PuzzleMaker_PublishBetaDescription" );
		}

		if ( m_pWorkshopAgreementLabel )
		{
			m_pWorkshopAgreementLabel->SetVisible( true );
		}

		if ( m_pWorkshopAgreementCheckBox )
		{
			m_pWorkshopAgreementCheckBox->SetVisible( true );
			m_pWorkshopAgreementCheckBox->SetSelected( false );
		}
	}
}


void CPuzzleMakerSaveDialog::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_ABUTTON | FB_BBUTTON );

		if ( m_eReason == PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE )
		{
			pFooter->SetButtons( FB_ABUTTON | FB_BBUTTON | FB_XBUTTON );
			pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PuzzleMaker_PublishPuzzleSaveDialog" );
			pFooter->SetButtonText( FB_XBUTTON, "#PORTAL2_Workshop_Agreement_View" );
		}
		else if ( m_eReason == PUZZLEMAKER_SAVE_RENAME )
		{
			pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PuzzleMaker_RenamePuzzle" );
		}
		else
		{
			pFooter->SetButtonText( FB_ABUTTON, "#PORTAL2_PuzzleMaker_SavePuzzle" );
		}
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel" );
	}
}


void CPuzzleMakerSaveDialog::OnThink( void )
{
	BaseClass::OnThink();

	if ( m_pChamberNameTextEntry )
	{
		SetTextEntryBoxColors( m_pChamberNameTextEntry, m_pChamberNameTextEntry->HasFocus() );
	}

	if ( m_pChamberDescriptionTextEntry )
	{
		SetTextEntryBoxColors( m_pChamberDescriptionTextEntry, m_pChamberDescriptionTextEntry->HasFocus() );
	}
}


void CPuzzleMakerSaveDialog::SetTextEntryBoxColors( vgui::TextEntry *pTextEntryBox, bool bHasFocus )
{
	if ( bHasFocus )
	{
		pTextEntryBox->SetFgColor( m_TextEntryBoxFocusFgColor );
		pTextEntryBox->SetBgColor( m_TextEntryBoxFocusBgColor );
		pTextEntryBox->SetCursorColor( m_TextEntryBoxFocusFgColor );
	}
	else
	{
		pTextEntryBox->SetFgColor( m_TextEntryBoxNonFocusFgColor );
		pTextEntryBox->SetBgColor( m_TextEntryBoxNonFocusBgColor );
		pTextEntryBox->SetCursorColor( m_TextEntryBoxNonFocusBgColor );
	}
}


void CPuzzleMakerSaveDialog::InitializeText()
{
	const PuzzleFilesInfo_t& puzzleInfo = g_pPuzzleMaker->GetPuzzleInfo();

	if ( m_pChamberNameTextEntry )
	{
		if ( puzzleInfo.m_strPuzzleTitle.IsEmpty() )
		{
			m_pChamberNameTextEntry->SetText( m_pwscDefaultChamberName );
		}
		else
		{
			m_pChamberNameTextEntry->SetText( puzzleInfo.m_strPuzzleTitle );
		}
	}

	if ( m_pChamberDescriptionTextEntry )
	{
		if ( puzzleInfo.m_strDescription.IsEmpty() )
		{
			m_pChamberDescriptionTextEntry->SetText( m_pwszDefaultChamberDescription );
		}
		else
		{
			m_pChamberDescriptionTextEntry->SetText( puzzleInfo.m_strDescription );
		}
	}

	//Puzzle already has a title
	if ( !puzzleInfo.m_strPuzzleTitle.IsEmpty() )
	{
		//Don't bring up the dialog unless save as, rename or publish
		if ( m_eReason != PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE &&
			 m_eReason != PUZZLEMAKER_SAVE_RENAME && 
			 m_eReason != PUZZLEMAKER_SAVE_SHORTCUT )
		{
			SaveButtonPressed();
		}
	}
}


void PuzzleMakerScreenshotCallback( const char *pszScreenshotName )
{
	CPuzzleMakerSaveDialog *pSelf = static_cast<CPuzzleMakerSaveDialog*>( CBaseModPanel::GetSingleton().GetWindow( WT_PUZZLEMAKERSAVEDIALOG ) );
	if ( pSelf )
	{
		pSelf->SetScreenshotName( pszScreenshotName );
	}
}


void CPuzzleMakerSaveDialog::SetScreenshotName( const char *pszScreenshotName )
{
	V_strncpy( m_szScreenshotName, pszScreenshotName, V_strlen( pszScreenshotName ) + 1 );

	//Load the screenshot into the image panel
	if ( V_strlen( m_szScreenshotName ) > 0 )
	{
		int nWidth;
		int nHeight;
		CUtlBuffer jpgBuffer;
		if ( g_pFullFileSystem->ReadFile( m_szScreenshotName, NULL, jpgBuffer ) )
		{
			CUtlBuffer destBuffer;
			
			if ( ImgUtl_ReadJPEGAsRGBA( jpgBuffer, destBuffer, nWidth, nHeight ) == CE_SUCCESS )
			{
				//success
				if ( m_nScreenshotID == -1 )
				{
					// create a procedural texture id
					m_nScreenshotID = vgui::surface()->CreateNewTextureID( true );
				}

				surface()->DrawSetTextureRGBALinear( m_nScreenshotID, (unsigned char*)destBuffer.Base(), nWidth, nHeight );
			}
		}
	}
}


void CPuzzleMakerSaveDialog::PaintBackground()
{
	BaseClass::PaintBackground();

	//Draw the screenshot
	if( m_nScreenshotID != -1 )
	{
		int x, y, wide, tall;
		m_pChamberImagePanel->GetBounds( x, y, wide, tall );

		int nTextureWide, nTextureTall;
		surface()->DrawGetTextureSize( m_nScreenshotID, nTextureWide, nTextureTall );
		float flScale = MIN( (float)wide/nTextureWide, (float)tall/nTextureTall );
		int nFinalWidth = nTextureWide * flScale;
		int nFinalHeight = nTextureTall * flScale;
		int xPos = x + ( wide/2 ) - ( nFinalWidth/2 );
		int yPos = y + ( tall/2 ) - ( nFinalHeight/2 );
		surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		surface()->DrawSetTexture( m_nScreenshotID );
		surface()->DrawTexturedRect( xPos, yPos, xPos + nFinalWidth, yPos + nFinalHeight );
	}
}


void CPuzzleMakerSaveDialog::SetReason( PuzzleMakerSaveDialogReason_t eReason )
{
	m_eReason = eReason;

	if ( m_eReason == PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE )
	{
		SetDialogTitle( "#PORTAL2_PuzzleMaker_PublishPuzzleTitle" );
	}
	else if ( m_eReason == PUZZLEMAKER_SAVE_RENAME )
	{
		SetDialogTitle( "#PORTAL2_PuzzleMaker_RenamePuzzle" );
	}

	if ( m_eReason == PUZZLEMAKER_SAVE_FROMPAUSEMENU ||
		 m_eReason == PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE ||
		 m_eReason == PUZZLEMAKER_SAVE_SHORTCUT )
	{
		g_pPuzzleMaker->TakeScreenshotAsync( PuzzleMakerScreenshotCallback, false );
	}
	else
	{
		SetScreenshotName( g_pPuzzleMaker->GetPuzzleInfo().m_strScreenshotFileName );
	}
}


void CPuzzleMakerSaveDialog::OnCommand( char const *pszCommand )
{
	if ( !V_stricmp( pszCommand, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		return;
	}
	else if( !V_stricmp( pszCommand, "PuzzleMaker_SaveDialog_PublishFriends" ) )
	{
		if ( m_pPublishDescriptionLabel )
		{
			m_pPublishDescriptionLabel->SetText( "#PORTAL2_PuzzleMaker_PublishFriendsDescription" );
		}

		m_eVisibility = k_ERemoteStoragePublishedFileVisibilityFriendsOnly;
	}
	else if( !V_stricmp( pszCommand, "PuzzleMaker_SaveDialog_PublishPublic" ) )
	{
		if ( m_pPublishDescriptionLabel )
		{
			m_pPublishDescriptionLabel->SetText( "#PORTAL2_PuzzleMaker_PublishPublicDescription" );
		}

		m_eVisibility = k_ERemoteStoragePublishedFileVisibilityPublic;
	}
	else if( !V_stricmp( pszCommand, "PuzzleMaker_SaveDialog_PublishHidden" ) )
	{
		if ( m_pPublishDescriptionLabel )
		{
			m_pPublishDescriptionLabel->SetText( "#PORTAL2_PuzzleMaker_PublishHiddenDescription" );
		}

		m_eVisibility = k_ERemoteStoragePublishedFileVisibilityPrivate;
	}

	BaseClass::OnCommand( pszCommand );
}


void CPuzzleMakerSaveDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
		SaveButtonPressed();
		return;
	case KEY_XBUTTON_B:
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );

		// These all happen from INSIDE the editor, so we just close all the way out
		if( m_eReason == PUZZLEMAKER_SAVE_FROM_NEW_CHAMBER
			|| m_eReason == PUZZLEMAKER_SAVE_FROM_OPEN_CHAMBER
			|| m_eReason == PUZZLEMAKER_SAVE_SHORTCUT
			|| m_eReason == PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE)
		{
			CloseUI();
		}
		else if ( !NavigateBack() )
		{
			Close();
		}
		return;
	case KEY_XBUTTON_X:
		ViewButtonPressed();
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}


void CPuzzleMakerSaveDialog::SaveButtonPressed()
{
	//Verify they've filled everything out properly

	wchar_t wszChamberName[MAX_TESTCHAMBER_NAME_LENGTH];
	m_pChamberNameTextEntry->GetText( wszChamberName, sizeof( wszChamberName ) );

	wchar_t wszChamberDescription[MAX_TESTCHAMBER_DESCRIPTION_LENGTH];
	m_pChamberDescriptionTextEntry->GetText( wszChamberDescription, sizeof( wszChamberDescription ) );

	bool bSameNameAsDefault = !V_wcscmp( wszChamberName, m_pwscDefaultChamberName );
	bool bSameDescriptionAsDefault = !V_wcscmp( wszChamberDescription, m_pwszDefaultChamberDescription );

	bool bHasName = ( m_pChamberNameTextEntry->GetTextLength() > 0 );
	bool bHasDescription = ( m_pChamberDescriptionTextEntry->GetTextLength() > 0 );

	//Only care about description if publishing
	if ( m_eReason != PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE )
	{
		bSameDescriptionAsDefault = false;
		bHasDescription = true;
	}


	if ( !bHasName || !bHasDescription || bSameNameAsDefault || bSameDescriptionAsDefault )
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );

		GenericConfirmation *pConfirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		if ( m_eReason == PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE )
		{
			data.pWindowTitle = "#PORTAL2_PuzzleMaker_PublishError";
		}
		else
		{
			data.pWindowTitle = "#PORTAL2_PuzzleMaker_SaveError";
		}
		data.pMessageText = "#PORTAL2_PuzzleMaker_SaveErrorInvalidNameMsg";

		data.bCancelButtonEnabled = true;
		data.pCancelButtonText = "#L4D360UI_Back";

		pConfirmation->SetUsageData( data );
		return;
	}
	else if ( m_eReason == PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE && m_pWorkshopAgreementCheckBox && !m_pWorkshopAgreementCheckBox->IsSelected() )
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );

		GenericConfirmation *pConfirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#PORTAL2_PuzzleMaker_PublishError";
		data.pMessageText = "#PORTAL2_PuzzleMaker_PublishErrorWorkshopAgreement";

		data.bCancelButtonEnabled = true;
		data.pCancelButtonText = "#L4D360UI_Back";

		pConfirmation->SetUsageData( data );
		return;
	}
	else //Valid save data
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		//TODO: Converting back to non-wide strings since CUtlString and the steam back end don't support wide strings
		char szChamberName[MAX_TESTCHAMBER_NAME_LENGTH];
		char szChamberDescription[MAX_TESTCHAMBER_DESCRIPTION_LENGTH];
		V_wcstostr( wszChamberName, V_wcslen( wszChamberName ) + 1, szChamberName, sizeof( szChamberName ) );
		V_wcstostr( wszChamberDescription, V_wcslen( wszChamberDescription ) + 1, szChamberDescription, sizeof( szChamberDescription ) );

		PuzzleFilesInfo_t puzzleInfo = g_pPuzzleMaker->GetPuzzleInfo();
		puzzleInfo.m_strPuzzleTitle = szChamberName;
		puzzleInfo.m_strDescription = szChamberDescription;
		g_pPuzzleMaker->SetPuzzleInfo( puzzleInfo );

		switch ( m_eReason )
		{
		case PUZZLEMAKER_SAVE_RENAME:
			{
				g_pPuzzleMaker->SavePuzzle( false );

				// Tell the "my chambers" dialog that we need it to refresh its list
				CBaseModFrame *pMyChambers = BASEMODPANEL_SINGLETON.GetWindow( WT_EDITORCHAMBERLIST );
				if ( pMyChambers != NULL )
				{
					PostMessage( pMyChambers, new KeyValues("RefreshList") );
				}

				if ( !NavigateBack() )
				{
					Close();
				}
			}
			break;
		case PUZZLEMAKER_SAVE_UNSPECIFIED:
		case PUZZLEMAKER_SAVE_FROMPAUSEMENU:
		case PUZZLEMAKER_SAVE_SHORTCUT:
			{
				g_pPuzzleMaker->SavePuzzle( true );
				CloseUI();
			}
			break;
		case PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE:
			{
				g_pPuzzleMaker->SavePuzzle( false );
				PublishButtonPressed();
			}
			break;
		case PUZZLEMAKER_SAVE_JOIN_COOP_GAME:
			{
				CUIGameData::Get()->Invite_Approved();
			}
			// Intentionally fall through
		case PUZZLEMAKER_SAVE_ONEXIT:
			{
				g_pPuzzleMaker->SavePuzzle( false );
				ExitPuzzleMaker();
			}
			break;
		case PUZZLEMAKER_SAVE_ON_QUIT_APP:
			{
				g_pPuzzleMaker->SavePuzzle( false );
				g_pPuzzleMaker->QuitGame();
			}
		case PUZZLEMAKER_SAVE_FROM_NEW_CHAMBER:
			{
				g_pPuzzleMaker->SavePuzzle( false );
				g_pPuzzleMaker->NewPuzzle( false );
				CloseUI();
			}
			break;
		case PUZZLEMAKER_SAVE_FROM_OPEN_CHAMBER:
			{
				g_pPuzzleMaker->SavePuzzle( false );
				Close();
				CPuzzleMakerMyChambers* pMyChamber = static_cast<CPuzzleMakerMyChambers*>(BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_EDITORCHAMBERLIST, this, true ));
				pMyChamber->UseInEditorFooter();
			}
			break;
		}
	}
}


void CPuzzleMakerSaveDialog::CloseUI()
{
	GameUI().AllowEngineHideGameUI();
	engine->ExecuteClientCmd("gameui_hide");
	CBaseModPanel::GetSingleton().CloseAllWindows();
}


void CPuzzleMakerSaveDialog::DisplayPublishDialog()
{
	engine->ExecuteClientCmd("gameui_activate");
	BaseModUI::CBaseModFrame *pInGameMenu = BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_INGAMEMAINMENU, NULL, true );

	BaseModUI::CBaseModFrame *pCompileDialog = BaseModUI::CBaseModPanel::GetSingleton().GetWindow( BaseModUI::WT_PUZZLEMAKERCOMPILEDIALOG );
	if ( pCompileDialog )
	{
		pCompileDialog->Close();
	}

	BaseModUI::CPuzzleMakerSaveDialog *pSaveDialog =
		static_cast<BaseModUI::CPuzzleMakerSaveDialog*>( BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_PUZZLEMAKERSAVEDIALOG, pInGameMenu, true ) );
	if ( pSaveDialog )
	{
		pSaveDialog->SetReason( BaseModUI::PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE );
	}
}


void BaseModUI::CPuzzleMakerSaveDialog::PublishButtonPressed()
{
	CPuzzleMakerPublishProgress* pPublishProgress =
		static_cast<CPuzzleMakerPublishProgress*>( CBaseModPanel::GetSingleton().OpenWindow( WT_PUZZLEMAKERPUBLISHPROGRESS, this, true ) );

	if ( pPublishProgress )
	{
		pPublishProgress->BeginPublish( m_eVisibility );
	}
}


void BaseModUI::CPuzzleMakerSaveDialog::ViewButtonPressed()
{
	if ( steamapicontext && steamapicontext->SteamFriends() )
	{
		if( steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->IsOverlayEnabled() )
		{
			steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( "http://www.steamcommunity.com/workshop/workshoplegalagreement" );
		}
		else
		{
			BaseModUI::GenericConfirmation* pConfirmation =
				static_cast<BaseModUI::GenericConfirmation*>( BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_GENERICCONFIRMATION, this, true ) );

			BaseModUI::GenericConfirmation::Data_t data;
			data.pWindowTitle = "#L4D360UI_SteamOverlay_Title";
			data.pMessageText = "#L4D360UI_SteamOverlay_Text";

			data.pCancelButtonText = "#L4D360UI_Back";
			data.bCancelButtonEnabled = true;

			pConfirmation->SetUsageData( data );
		}
	}
}


void cc_puzzlemaker_publish( const CCommand &args )
{
	// It's a fullscreen hud element, so we don't need to activate it for other slots
	if ( GET_ACTIVE_SPLITSCREEN_SLOT() != 0 )
		return;

	BaseModUI::CBaseModFrame *pInGameMenu = BaseModUI::CBaseModPanel::GetSingleton().GetWindow( BaseModUI::WT_INGAMEMAINMENU );

	CPuzzleMakerSaveDialog *pSaveDialog = static_cast<CPuzzleMakerSaveDialog*>(CBaseModPanel::GetSingleton().OpenWindow( WT_PUZZLEMAKERSAVEDIALOG, pInGameMenu, true ));
	pSaveDialog->DisplayPublishDialog();
	GameUI().PreventEngineHideGameUI();
}
static ConCommand puzzlemaker_publish("puzzlemaker_request_publish", cc_puzzlemaker_publish );

#endif //PORTAL2_PUZZLEMAKER

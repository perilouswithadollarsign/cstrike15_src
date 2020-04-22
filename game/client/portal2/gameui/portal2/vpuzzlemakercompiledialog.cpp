//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//
#if defined( PORTAL2_PUZZLEMAKER )

#include "vpuzzlemakercompiledialog.h"
#include "vfooterpanel.h"
#include "puzzlemaker/puzzlemaker.h"
#include "vgui_controls/ImagePanel.h"
#include "vgenericconfirmation.h"
#include <vgui/IInput.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

extern void CloseUI();

const int MAX_PUZZLEMAKER_EDIT_HINTS = 42;
const int PUZZLEMAKER_LAST_BASIC_HINT_INDEX = 6;
#define PUZZLEMAKER_EDIT_HINT_BASE "#PORTAL2_PuzzleEditor_Hint_Edit_"

const int VBSP_ERROR_CODE_TOO_MANY_EDICTS = 2;

ConVar puzzlemaker_current_hint( "puzzlemaker_current_hint", "0", FCVAR_ARCHIVE );


CPuzzleMakerCompileDialog::CPuzzleMakerCompileDialog( Panel *pParent, const char *pPanelName )
						 : BaseClass( pParent, pPanelName, false, true, false ),
						   m_pSpinner( NULL ),
						   m_pHintMsgLabel( NULL ),
						   m_pVBSPProgress( NULL ),
						   m_pVVISProgress( NULL ),
						   m_pVRADProgress( NULL ),
						   m_bFromShortcut( false )
{
	engine->ExecuteClientCmd("hideconsole");

	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#PORTAL2_PuzzleMaker_CompileDialogTitle" );

	SetFooterEnabled( true );
	UpdateFooter();

	GameUI().PreventEngineHideGameUI();
}


CPuzzleMakerCompileDialog::~CPuzzleMakerCompileDialog()
{}


void CPuzzleMakerCompileDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pSpinner = dynamic_cast< ImagePanel* >( FindChildByName( "CompileSpinner" ) );

	m_pHintMsgLabel = dynamic_cast<Label*>( FindChildByName( "LblHintMsg" ) );

	m_pVBSPProgress = dynamic_cast<ContinuousProgressBar*>( FindChildByName( "VBSPProgress" ) );
	m_pVVISProgress = dynamic_cast<ContinuousProgressBar*>( FindChildByName( "VVISProgress" ) );
	m_pVRADProgress = dynamic_cast<ContinuousProgressBar*>( FindChildByName( "VRADProgress" ) );

	UpdateHintText();
	UpdateFooter();
}


void CPuzzleMakerCompileDialog::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_BBUTTON );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel" );
	}
}


void CPuzzleMakerCompileDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		CancelAndClose();
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}


#ifndef _GAMECONSOLE
void CPuzzleMakerCompileDialog::OnKeyCodeTyped( vgui::KeyCode code )
{
	switch ( code )
	{
	case KEY_ESCAPE:
		return OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}

	BaseClass::OnKeyTyped( code );
}
#endif

void CPuzzleMakerCompileDialog::CancelAndClose()
{
	g_pPuzzleMaker->CancelCompile();

	if ( m_bFromShortcut )
	{
		GameUI().AllowEngineHideGameUI();
		engine->ExecuteClientCmd("gameui_hide");
		CBaseModPanel::GetSingleton().CloseAllWindows();
	}
	else
	{
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
		if ( !NavigateBack() )
		{
			Close();
		}
	}
}


void BaseModUI::CPuzzleMakerCompileDialog::SetFromShortcut( bool bFromShortcut )
{
	m_bFromShortcut = bFromShortcut;
}


void CPuzzleMakerCompileDialog::OnThink()
{
	BaseClass::OnThink();

	//Update spinner
	if ( m_pSpinner )
	{
		const int nAnimFrame = ( uint64 )( Plat_FloatTime() * 10 );
		m_pSpinner->SetFrame( nAnimFrame );
	}

	int nFailedErrorCode;
	CUtlString strFailedProcess;
	PuzzleCompileSteps eCompileStep;
	float flScale = g_pPuzzleMaker->GetCurrentCompileProgress( &nFailedErrorCode, &strFailedProcess, &eCompileStep );
	
	if ( flScale == -1 )
	{
		CompileFailed( nFailedErrorCode, strFailedProcess );
	}
	else if ( m_pVBSPProgress && m_pVVISProgress && m_pVRADProgress )
	{
		switch ( eCompileStep )
		{
		case PUZZLE_COMPILE_VBSP:
			{
				m_pVBSPProgress->SetProgress( flScale );
			}
			break;
		case PUZZLE_COMPILE_VVIS:
			{
				m_pVBSPProgress->SetProgress( 1.0f );
				m_pVVISProgress->SetProgress( flScale );
			}
			break;
		case PUZZLE_COMPILE_VRAD:
			{
				m_pVBSPProgress->SetProgress( 1.0f );
				m_pVVISProgress->SetProgress( 1.0f );
				m_pVRADProgress->SetProgress( flScale );
			}
			break;
		}
	}
}


void CPuzzleMakerCompileDialog::UpdateHintText()
{
	if ( m_pHintMsgLabel )
	{
		//This will cycle through all the hints in order
		//The first few hints will only show up once
		int nHintIndex = puzzlemaker_current_hint.GetInt();
		if ( nHintIndex > MAX_PUZZLEMAKER_EDIT_HINTS )
		{
			nHintIndex = PUZZLEMAKER_LAST_BASIC_HINT_INDEX;
		}

		puzzlemaker_current_hint.SetValue( nHintIndex + 1 );

		char szHintLabel[MAX_PATH];

		V_snprintf( szHintLabel, ARRAYSIZE( szHintLabel ), "%s%02d", PUZZLEMAKER_EDIT_HINT_BASE, nHintIndex );

		m_pHintMsgLabel->SetText( szHintLabel );
	}
}


void CPuzzleMakerCompileDialog::CompileFailed( int nFailedErrorCode, const char *pszFailedProcess )
{
	GameUI().PreventEngineHideGameUI();
	if ( !V_stricmp( pszFailedProcess, "VBSP" ) && nFailedErrorCode == VBSP_ERROR_CODE_TOO_MANY_EDICTS )
	{
		CompileError( "#PORTAL2_PuzzleMaker_CompileError_TooManyEdicts", false );
	}
	else
	{
		CompileError( "#PORTAL2_PuzzleMaker_CompileError_generic", true );
	}
}


void CompileErrorHelpPressed()
{
	if ( steamapicontext && steamapicontext->SteamFriends() )
	{
		if( steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->IsOverlayEnabled() )
		{
			CloseUI();

			steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( "http://www.thinkwithportals.com/puzzlemaker/compile_error.php" );
		}
		else
		{
			engine->ExecuteClientCmd("gameui_activate");
			GameUI().PreventEngineHideGameUI();

			BaseModUI::GenericConfirmation* pConfirmation =
			static_cast<BaseModUI::GenericConfirmation*>( BaseModUI::CBaseModPanel::GetSingleton().OpenWindow( BaseModUI::WT_GENERICCONFIRMATION, NULL, true ) );

			BaseModUI::GenericConfirmation::Data_t data;
			data.pWindowTitle = "#L4D360UI_SteamOverlay_Title";
			data.pMessageText = "#L4D360UI_SteamOverlay_Text";

			data.pCancelButtonText = "#L4D360UI_Ok";
			data.bCancelButtonEnabled = true;
			data.pfnCancelCallback = CloseUI;

			pConfirmation->SetUsageData( data );
		}
	}
}



void CPuzzleMakerCompileDialog::CompileError( const char *pszError, bool bShowHelpButton )
{
	Close();

	BaseModUI::CBaseModFrame *pInGameMenu = BaseModUI::CBaseModPanel::GetSingleton().GetWindow( BaseModUI::WT_INGAMEMAINMENU );
	GenericConfirmation* pConfirmation =
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, pInGameMenu, true ) );

	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#PORTAL2_PuzzleMaker_CompilingError";
	data.pMessageText = pszError;

	if ( bShowHelpButton )
	{
		data.pOkButtonText = "#PORTAL2_PuzzleMaker_CompileError_help";
		data.bOkButtonEnabled = true;
		data.pfnOkCallback = CompileErrorHelpPressed;
	}

	data.pCancelButtonText = "#L4D360UI_Back";
	data.bCancelButtonEnabled = true;
	data.pfnCancelCallback = CloseUI;

	pConfirmation->SetUsageData( data );
}

void cc_puzzlemaker_switch_session( const CCommand &args )
{
	Assert( args.ArgC() == 3 );
	
	if( args.ArgC() == 3 )
	{
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

		KeyValues *pSettings = new KeyValues( "FadeOutStartGame" );
		KeyValues::AutoDelete autodelete_pSettings( pSettings );
		pSettings->SetString( "map", args[1] );
		if ( !V_stricmp( args[2], "mp" ) || !V_stricmp( args[2], "MP" ) )
		{
			pSettings->SetString( "reason", "coop_puzzlemaker_preview" );
		}
		else
		{
			pSettings->SetString( "reason", "sp_puzzlemaker_preview" );
		}

		BaseModUI::CBaseModFrame *pCompileDialog = BaseModUI::CBaseModPanel::GetSingleton().GetWindow( BaseModUI::WT_PUZZLEMAKERCOMPILEDIALOG );
		BASEMODPANEL_SINGLETON.OpenWindow( WT_FADEOUTSTARTGAME, pCompileDialog, true, pSettings );
	}
}
static ConCommand puzzlemaker_switch_session("puzzlemaker_switch_session", cc_puzzlemaker_switch_session );

#endif //PORTAL2_PUZZLEMAKER

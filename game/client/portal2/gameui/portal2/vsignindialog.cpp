//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VSignInDialog.h"
#include "VAttractScreen.h"
#include "VFooterPanel.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

#define PLAYER_DEBUG_NAME "WWWWWWWWWWWWWWW"

ConVar ui_signin_dialog_autoclose( "ui_signin_dialog_autoclose", "60" );

SignInDialog::SignInDialog( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, false )
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );

	SetDialogTitle( "#PORTAL2_SignIn_Title" );

	SetFooterEnabled( true );
	UpdateFooter();

	m_pBtnSignIn = NULL;

	m_flTimeAutoClose = Plat_FloatTime() + ui_signin_dialog_autoclose.GetFloat();
}

SignInDialog::~SignInDialog()
{
}

void SignInDialog::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pBtnSignIn = FindChildByName( "BtnSignin" );

	UpdateFooter();
}

void SignInDialog::OnCommand( const char *command )
{
	int iUser = BaseModUI::CBaseModPanel::GetSingleton().GetLastActiveUserId();

	if ( !V_stricmp( command, "Play" ) )
	{
		if ( iUser != (int) XBX_GetPrimaryUserId() )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}

		NavigateBack( 1 );
	}
	else if ( !V_stricmp( command, "PlaySplitscreen" ) )
	{
		NavigateBack( 2 );
	}
	else if ( !V_stricmp( command, "PlayAsGuest" ) )
	{
		if ( iUser != (int) XBX_GetPrimaryUserId() )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}

		if ( CAttractScreen *pAttractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
		{
			BaseClass::NavigateBack();
			Close();
			pAttractScreen->StartGameWithTemporaryProfile_Stage1();
		}
	}
	else if ( !V_stricmp( command, "CancelSignIn" ) )
	{
		NavigateBack( 0 );
	}
}

void SignInDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		NavigateBack( 0 );
		break;
	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void SignInDialog::LoadLayout()
{
	BaseClass::LoadLayout();

#ifdef _GAMECONSOLE
	if ( m_pBtnSignIn )
	{
		m_pBtnSignIn->NavigateTo();
	}
#endif
}

void SignInDialog::OnThink()
{
	// As soon as UI opens pretend like cancel was selected
	if ( IsX360() && CUIGameData::Get()->IsXUIOpen() )
	{
		NavigateBack( 0 );
		return;
	}

	if ( IsX360() && ( Plat_FloatTime() > m_flTimeAutoClose ) )
	{
		NavigateBack( 0 );
		return;
	}
	
	BaseClass::OnThink();
}

vgui::Panel *SignInDialog::NavigateBack( int numSlotsRequested )
{
	CAttractScreen *pAttractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) );
	if ( pAttractScreen )
	{
#if defined(_GAMECONSOLE)
		// Todo: add an option to play without signing in!

		// Attract screen preserved the state of who activated the
		// sign-in dialog, so we can reset user ids freely here.

		// When the attact screen opens, clear the primary userId and
		// reset all the controllers as enabled
		// This is here because CAttractScreen::OnOpen can be called when navigating away
		// from it as well
		XBX_SetPrimaryUserId( XBX_INVALID_USER_ID );
		XBX_ResetUserIdSlots();
#endif //defined(_GAMECONSOLE)

		switch ( numSlotsRequested )
		{
		default:
		case 0:
			pAttractScreen->ShowPressStart();
			break;
		case 1:
			PostMessage( pAttractScreen, new KeyValues( "StartWaitingForBlade1" ) );
			break;
		case 2:
			PostMessage( pAttractScreen, new KeyValues( "StartWaitingForBlade2" ) );
			break;
		}
	}

	vgui::Panel *pReturn = BaseClass::NavigateBack();

	Close();

	return pReturn;
}

void SignInDialog::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_ABUTTON | FB_BBUTTON );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_SignIn_CancelSignIn" );
	}
}
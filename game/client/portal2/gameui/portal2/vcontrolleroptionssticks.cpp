//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VControllerOptionsSticks.h"
#include "VFooterPanel.h"
#include "VSliderControl.h"
#include "VDropDownMenu.h"
#include "VFlyoutMenu.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/ImagePanel.h"
#include "VHybridButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

enum
{
	CONTROLLER_STICKS_NORMAL = 0,
	CONTROLLER_STICKS_SOUTHPAW,
	NUM_CONTROLLER_STICKS_SETTINGS
};

const char *pszStickSettingsButtonName[NUM_CONTROLLER_STICKS_SETTINGS] =
{
	"BtnDefault",
	"BtnSouthpaw"
};

const char *pszStickButtonNamesNormal[NUM_CONTROLLER_STICKS_SETTINGS] =
{
	"#L4D360UI_Controller_Sticks_Default",
	"#L4D360UI_Controller_Sticks_Southpaw"
};

const char *pszStickButtonNamesLegacy[NUM_CONTROLLER_STICKS_SETTINGS] =
{
	"#L4D360UI_Controller_Sticks_Legacy",
	"#L4D360UI_Controller_Sticks_Legacy_Southpaw"
};

const char **arrSticksLegacy[2] =
{
	pszStickButtonNamesNormal,
	pszStickButtonNamesLegacy
};

ControllerOptionsSticks::ControllerOptionsSticks(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	m_iActiveUserSlot = -1;

	SetDialogTitle( "#L4D360UI_Controller_Sticks_Title", NULL, true );

	SetFooterEnabled( true );
}

ControllerOptionsSticks::~ControllerOptionsSticks()
{
}

void ControllerOptionsSticks::SetDataSettings( KeyValues *pSettings )
{
	m_iActiveUserSlot = pSettings->GetInt( "slot", CBaseModPanel::GetSingleton().GetLastActiveUserId() );
	SetGameUIActiveSplitScreenPlayerSlot( m_iActiveUserSlot );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( m_iActiveUserSlot );
}

void ControllerOptionsSticks::SetImageState( int nSetNewSetting )
{
	int nStickState = CONTROLLER_STICKS_NORMAL;
	CGameUIConVarRef joy_movement_stick( "joy_movement_stick" );
	if ( joy_movement_stick.IsValid() )
	{
		if ( nSetNewSetting >= 0 )
		{
			joy_movement_stick.SetValue( nSetNewSetting );
		}

		nStickState = ( joy_movement_stick.GetInt() > 0 ) ? 1 : 0;
	}

	int nLegacy = 0;
	CGameUIConVarRef s_joy_legacy( "joy_legacy" );
	if ( s_joy_legacy.IsValid() )
	{
		nLegacy = ( s_joy_legacy.GetInt() > 0 ) ? 1 : 0;
	}

	bool bReversedStickLayout = ( nStickState == CONTROLLER_STICKS_SOUTHPAW ) ^ ( !!nLegacy );

	SetControlVisible( "Normal_Move", !bReversedStickLayout );
	SetControlVisible( "Normal_Look", !bReversedStickLayout );
	SetControlVisible( "Southpaw_Move", bReversedStickLayout );
	SetControlVisible( "Southpaw_Look", bReversedStickLayout );

	char const *arrImagePanels[] = { "Normal_Move", "Normal_Look", "Southpaw_Move", "Southpaw_Look" };
	int clrLegacy = nLegacy ? 10 : 255;
	for ( int k = 0; k < ARRAYSIZE( arrImagePanels ); ++ k )
	{
		vgui::ImagePanel *pImgPanel = dynamic_cast< vgui::ImagePanel * >( FindChildByName( arrImagePanels[k] ) );
		if ( pImgPanel )
		{
			pImgPanel->SetDrawColor( Color( 255, clrLegacy, clrLegacy, 255 ) );
		}
	}

	if ( vgui::Label *pLegacyExplanation = dynamic_cast< vgui::Label * >( FindChildByName( "LegacyExplanation" ) ) )
	{
		pLegacyExplanation->SetVisible( nLegacy > 0 );
	}
}

void ControllerOptionsSticks::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	CGameUIConVarRef joy_movement_stick( "joy_movement_stick" );
	if ( joy_movement_stick.IsValid() )
	{
		int iSetting = ( joy_movement_stick.GetInt() > 0 ) ? 1 : 0;

		// need to set these NOW, otherwise they flicker as the paint occurs before the nav
		SetImageState( iSetting );

		for ( int k = 0; k < NUM_CONTROLLER_STICKS_SETTINGS; ++ k )
		{
			if ( Panel *btn = FindChildByName( pszStickSettingsButtonName[k] ) )
			{
				btn->NavigateFrom();
			}
		}

		if ( Panel *btnActive = FindChildByName( pszStickSettingsButtonName[iSetting] ) )
		{
			btnActive->NavigateTo();
			m_ActiveControl = btnActive;
			if ( IsPC() )
			{
				OnCommand( m_ActiveControl->GetName() );
			}
		}
	}

	UpdateFooter();
	UpdateButtonNames();
}

void ControllerOptionsSticks::OnKeyCodePressed(KeyCode code)
{
	int lastUser = GetJoystickForCode( code );
	if ( m_iActiveUserSlot != lastUser )
		return;

	vgui::KeyCode basecode = GetBaseButtonCode( code );

	switch( basecode )
	{
	case KEY_XBUTTON_A:
		// Nav back when the select one of the options
		BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		break;

	case KEY_XBUTTON_Y:
		// Toggle Legacy control settings.
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			static CGameUIConVarRef s_joy_legacy( "joy_legacy" );
			if ( s_joy_legacy.IsValid() )
			{
				s_joy_legacy.SetValue( !s_joy_legacy.GetBool() );
			}

			SetImageState( -1 );
			UpdateFooter();
			UpdateButtonNames();
		}
		break;

#if !defined( GAMECONSOLE )
	case KEY_XBUTTON_DOWN:
	case KEY_XBUTTON_UP:
	case KEY_DOWN:
	case KEY_UP:
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK1_UP:
	case KEY_XSTICK2_DOWN:
	case KEY_XSTICK2_UP:
		// the PC wants the mouse to click to set the active item, but...that prevents the PC gamepad from being useable
		// this allows gamepad to be used to navigate this menu and make the focus item the active item.
		if ( m_ActiveControl )
		{
			OnCommand( m_ActiveControl->GetName() );
		}
		BaseClass::OnKeyCodePressed( code );
		break;
#endif

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void ControllerOptionsSticks::OnHybridButtonNavigatedTo( VPANEL defaultButton )
{
	if ( IsPC() )
		return;

	Panel *panel = ipanel()->GetPanel( defaultButton, GetModuleName() );
	if ( panel )
	{
		OnCommand( panel->GetName() );
	}
}

void ControllerOptionsSticks::OnCommand( const char *command )
{
	if ( IsPC() )
	{
		for ( int i = 0; i < NUM_CONTROLLER_STICKS_SETTINGS; i++ )
		{
			BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( FindChildByName( pszStickSettingsButtonName[i] ) );
			if ( pButton )
			{
				bool bActive = !V_stricmp( command, pszStickSettingsButtonName[i] );
				pButton->SetPostCheckMark( bActive );
			}
		}
	}

	if ( !Q_strcmp( command, pszStickSettingsButtonName[CONTROLLER_STICKS_NORMAL] ) )
	{
		SetImageState( CONTROLLER_STICKS_NORMAL );
	}
	else if ( !Q_strcmp( command, pszStickSettingsButtonName[CONTROLLER_STICKS_SOUTHPAW] ) )
	{
		SetImageState( CONTROLLER_STICKS_SOUTHPAW );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

Panel* ControllerOptionsSticks::NavigateBack()
{
	// parent dialog will write config
	return BaseClass::NavigateBack();
}

void ControllerOptionsSticks::UpdateFooter()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( !footer )
		return;

	static CGameUIConVarRef s_joy_legacy( "joy_legacy" );
	bool bSupportsLegacy = s_joy_legacy.IsValid();

	int visibleButtons = FB_BBUTTON;
	if ( IsGameConsole() )
	{
		visibleButtons |= FB_ABUTTON;
	}

	footer->SetButtons( visibleButtons | ( bSupportsLegacy ? FB_YBUTTON : FB_NONE ) );
	footer->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
	footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );

	if ( bSupportsLegacy )
	{
		footer->SetButtonText( FB_YBUTTON, s_joy_legacy.GetBool() ? "#L4D360UI_LegacyOn" : "#L4D360UI_LegacyOff" );
	}
}

void ControllerOptionsSticks::UpdateButtonNames()
{
	CGameUIConVarRef joy_legacy( "joy_legacy" );
	if ( joy_legacy.IsValid() )
	{
		int iLegacy = ( joy_legacy.GetInt() > 0 ) ? 1 : 0;

		const char **arrNames = arrSticksLegacy[iLegacy];
		for ( int k = 0; k < NUM_CONTROLLER_STICKS_SETTINGS; ++k )
		{
			if ( BaseModHybridButton *btn = dynamic_cast< BaseModHybridButton * >( FindChildByName( pszStickSettingsButtonName[k] ) ) )
			{
				btn->SetText( arrNames[k] );
			}
		}
	}
}


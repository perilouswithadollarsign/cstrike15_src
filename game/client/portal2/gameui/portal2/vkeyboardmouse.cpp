//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VKeyboardMouse.h"
#include "VFooterPanel.h"
#include "VDropDownMenu.h"
#include "VSliderControl.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "materialsystem/materialsystem_config.h"

#ifdef _X360
#include "xbox/xbox_launch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

KeyboardMouse::KeyboardMouse( Panel *parent, const char *panelName ):
BaseClass( parent, panelName )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#L4D360UI_KeyboardMouse" );

	m_btnEditBindings = NULL;
	m_drpMouseYInvert = NULL;
	m_sldMouseSensitivity = NULL;
	m_drpDeveloperConsole = NULL;
	m_drpRawMouse = NULL;
	m_drpMouseAcceleration = NULL;
	m_sldMouseAcceleration = NULL;

	m_bDirtyConfig = false;

	SetFooterEnabled( true );
	UpdateFooter( true );
}

KeyboardMouse::~KeyboardMouse()
{
}

void KeyboardMouse::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_btnEditBindings = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnEditBindings" ) );
	m_drpMouseYInvert = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpMouseYInvert" ) );
	m_sldMouseSensitivity = dynamic_cast< SliderControl* >( FindChildByName( "SldMouseSensitivity" ) );
	m_drpRawMouse = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpRawMouse" ) );
	m_drpMouseAcceleration = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpMouseAcceleration" ) );
	m_sldMouseAcceleration = dynamic_cast< SliderControl* >( FindChildByName( "SldMouseAcceleration" ) );
	m_drpDeveloperConsole = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpDeveloperConsole" ) );
	
	if ( m_drpMouseYInvert )
	{
		CGameUIConVarRef m_pitch( "m_pitch" );
		if ( m_pitch.GetFloat() > 0.0f )
		{
			m_drpMouseYInvert->SetCurrentSelection( "#L4D360UI_Disabled" );
		}
		else
		{
			m_drpMouseYInvert->SetCurrentSelection( "#L4D360UI_Enabled" );
		}
	}

	if ( m_sldMouseSensitivity )
	{
		m_sldMouseSensitivity->Reset();
	}

	if ( m_drpRawMouse )
	{
		CGameUIConVarRef m_rawinput( "m_rawinput" );
		if ( !m_rawinput.GetBool() || !IsPlatformWindowsPC() )
		{
			m_drpRawMouse->SetCurrentSelection( "#L4D360UI_Disabled" );
		}
		else
		{
			m_drpRawMouse->SetCurrentSelection( "#L4D360UI_Enabled" );
		}
	}

	if ( m_drpMouseAcceleration )
	{
		CGameUIConVarRef m_customaccel( "m_customaccel" );
		if ( m_customaccel.GetInt() > 0 )
		{
			m_drpMouseAcceleration->SetCurrentSelection( "#L4D360UI_Enabled" );
//			SetControlEnabled( "SldMouseAcceleration", true );
		}
		else
		{
			m_drpMouseAcceleration->SetCurrentSelection( "#L4D360UI_Disabled" );
//			SetControlEnabled( "SldMouseAcceleration", false );
		}
	}

	if ( m_sldMouseAcceleration )
	{
		m_sldMouseAcceleration->Reset();
	}

	if ( m_drpDeveloperConsole )
	{
		CGameUIConVarRef con_enable( "con_enable" );
		if ( !con_enable.GetBool() )
		{
			m_drpDeveloperConsole->SetCurrentSelection( "#L4D360UI_Disabled" );
		}
		else
		{
			m_drpDeveloperConsole->SetCurrentSelection( "#L4D360UI_Enabled" );
		}
	}

	if ( m_btnEditBindings )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		m_btnEditBindings->NavigateTo();
		m_ActiveControl = m_btnEditBindings;
	}

	UpdateFooter( true );
}

void KeyboardMouse::Activate()
{
	BaseClass::Activate();
	UpdateFooter( true );
}

void KeyboardMouse::UpdateFooter( bool bEnableCloud )
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_BBUTTON );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );

		pFooter->SetShowCloud( bEnableCloud );
	}
}

void KeyboardMouse::OnKeyCodePressed(KeyCode code)
{
	int joystick = GetJoystickForCode( code );
	int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		// Ready to write that data... go ahead and nav back
		BaseClass::OnKeyCodePressed( code );
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void KeyboardMouse::OnCommand( const char *pCommand )
{
	if ( !V_stricmp( "#L4D360UI_Controller_Edit_Keys_Buttons", pCommand ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_KEYBINDINGS, this, true );
	}
	else if ( !V_stricmp( "MouseYInvertEnabled", pCommand ) )
	{
		CGameUIConVarRef m_pitch( "m_pitch" );
		if ( m_pitch.GetFloat() > 0.0f )
		{
			m_pitch.SetValue( -1.0f * m_pitch.GetFloat() );
			m_bDirtyConfig = true;
		}
	}
	else if ( !V_stricmp( "MouseYInvertDisabled", pCommand ) )
	{
		CGameUIConVarRef m_pitch( "m_pitch" );
		if ( m_pitch.GetFloat() < 0.0f )
		{
			m_pitch.SetValue( -1.0f * m_pitch.GetFloat() );
			m_bDirtyConfig = true;
		}
	}
	else if ( !V_stricmp( "RawMouseEnabled", pCommand ) )
	{
		CGameUIConVarRef m_rawinput( "m_rawinput" );
		m_rawinput.SetValue( true );
		m_bDirtyConfig = true;
	}
	else if ( !V_stricmp( "RawMouseDisabled", pCommand ) )
	{
		CGameUIConVarRef m_rawinput( "m_rawinput" );
		m_rawinput.SetValue( false );
		m_bDirtyConfig = true;
	}
	else if ( !V_stricmp( "MouseAccelerationEnabled", pCommand ) )
	{
		CGameUIConVarRef m_customaccel( "m_customaccel" );
		m_customaccel.SetValue( 3 );
//		SetControlEnabled( "SldMouseAcceleration", true );
		m_bDirtyConfig = true;
	}
	else if ( !V_stricmp( "MouseAccelerationDisabled", pCommand ) )
	{
		CGameUIConVarRef m_customaccel( "m_customaccel" );
		m_customaccel.SetValue( 0 );
//		SetControlEnabled( "SldMouseAcceleration", false );
		m_bDirtyConfig = true;
	}
	else if ( !V_stricmp( "DeveloperConsoleEnabled", pCommand ) )
	{
		CGameUIConVarRef con_enable( "con_enable" );
		con_enable.SetValue( true );
		m_bDirtyConfig = true;
	}
	else if ( !V_stricmp( "DeveloperConsoleDisabled", pCommand ) )
	{
		CGameUIConVarRef con_enable( "con_enable" );
		con_enable.SetValue( false );
		m_bDirtyConfig = true;
	}
	else if ( !V_stricmp( "Back", pCommand ) )
	{
		OnKeyCodePressed( KEY_XBUTTON_B );
	}
	else
	{
		BaseClass::OnCommand( pCommand );
	}
}

Panel *KeyboardMouse::NavigateBack()
{
	if ( m_bDirtyConfig ||
		( m_sldMouseSensitivity && m_sldMouseSensitivity->IsDirty() ) ||
		( m_sldMouseAcceleration && m_sldMouseAcceleration->IsDirty() ) )
	{
		engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetPrimaryUserId() ) );
	}

	UpdateFooter( false );

	return BaseClass::NavigateBack();
}

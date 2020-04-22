//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VControllerOptions.h"
#include "VFooterPanel.h"
#include "VSliderControl.h"
#include "VDropDownMenu.h"
#include "VFlyoutMenu.h"
#include "VHybridButton.h"
#include "VGenericConfirmation.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ILocalize.h"
#include "VControllerOptionsButtons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

#define DUCK_MODE_HOLD		0
#define DUCK_MODE_TOGGLE	1

const char *pszDuckModes[2] =
{
	"#L4D360UI_Controller_Hold",
	"#L4D360UI_Controller_Toggle"
};

#define CONTROLLER_LOOK_TYPE_NORMAL		0
#define CONTROLLER_LOOK_TYPE_INVERTED	1

const char *pszLookTypes[2] =
{
	"#L4D360UI_Controller_Normal",
	"#L4D360UI_Controller_Inverted"
};

const char *pszButtonSettingsDisplayName[4] =
{
	"#L4D360UI_Controller_Buttons_Config1",
	"#L4D360UI_Controller_Buttons_Config2",
	"#L4D360UI_Controller_Buttons_Config3",
	"#L4D360UI_Controller_Buttons_Config4"
};

const char *pszStickSettingsDisplayName[4] = 
{
	"#L4D360UI_Controller_Sticks_Default",
	"#L4D360UI_Controller_Sticks_Southpaw",
	"#L4D360UI_Controller_Sticks_Legacy",
	"#L4D360UI_Controller_Sticks_Legacy_Southpaw"
};

const char *pszEnableTypes[2] =
{
	"#L4D360UI_Disabled",
	"#L4D360UI_Enabled",
};

static ControllerOptions *s_pControllerOptions = NULL;

ControllerOptions::ControllerOptions(Panel *parent, const char *panelName):
BaseClass( parent, panelName )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#L4D360UI_Controller_Title", NULL, true );

	m_pVerticalSensitivity = NULL;
	m_pHorizontalSensitivity = NULL;
	m_pHorizontalLookType = NULL;
	m_pVerticalLookType = NULL;
	m_pDuckMode = NULL;
	m_pEditButtons = NULL;
	m_pEditSticks = NULL;
	m_pVibration = NULL;
	m_pController = NULL;

	m_bDirty = false;
	m_nResetControlValuesTicks = -1;

	m_iActiveUserSlot = -1;

	SetFooterEnabled( true );
}

ControllerOptions::~ControllerOptions()
{
}

void ControllerOptions::SetDataSettings( KeyValues *pSettings )
{
	m_iActiveUserSlot = pSettings->GetInt( "slot", CBaseModPanel::GetSingleton().GetLastActiveUserId() );
	SetGameUIActiveSplitScreenPlayerSlot( m_iActiveUserSlot );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( m_iActiveUserSlot );
}

void ControllerOptions::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;
		if ( IsGameConsole() )
		{
			visibleButtons |= FB_ABUTTON;
		}
		else
		{
			visibleButtons |= FB_XBUTTON;
		}

		pFooter->SetButtons( visibleButtons );
		pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );
		if ( IsPC() )
		{
			pFooter->SetButtonText( FB_XBUTTON, "#GameUI_UseDefaults" );
		}
	}
}	

void ControllerOptions::Activate()
{
	BaseClass::Activate();

	ResetControlValues();
	UpdateFooter();
}

void ControllerOptions::ResetControlValues( void )
{
	if ( !IsGameConsole() && m_pController )
	{
		CGameUIConVarRef joystick( "joystick" );
		int iEnable = (int)joystick.GetBool();
		m_pController->SetCurrentSelection( pszEnableTypes[iEnable] );
	}

	// labels for button config and stick config
	if ( m_pEditButtons )
	{
		CGameUIConVarRef joy_cfg_preset( "joy_cfg_preset" );
		const char *pDisplayName;
		if ( joy_cfg_preset.GetInt() == CONTROLLER_BUTTONS_SPECCUSTOM )
		{
			pDisplayName = "#PORTAL2_Controller_Buttons_Custom";
		}
		else
		{
			int iButtonSetting = clamp( joy_cfg_preset.GetInt() - 1, 0, 3 );
			pDisplayName = pszButtonSettingsDisplayName[iButtonSetting];
		}
		m_pEditButtons->SetCurrentSelection( pDisplayName );
	}

	if ( m_pEditSticks )
	{
		CGameUIConVarRef joy_movement_stick("joy_movement_stick");
		int iStickSetting = ( joy_movement_stick.GetInt() > 0 ) ? 1 : 0;

		static CGameUIConVarRef s_joy_legacy( "joy_legacy" );
		if ( s_joy_legacy.IsValid() && s_joy_legacy.GetBool() )
		{
			iStickSetting += 2; // Go to the legacy version of the default/southpaw string.
		}

		m_pEditSticks->SetCurrentSelection( pszStickSettingsDisplayName[iStickSetting] );
	}

	if ( m_pVerticalSensitivity )
	{
		m_pVerticalSensitivity->Reset();
	}

	if ( m_pHorizontalSensitivity )
	{
		m_pHorizontalSensitivity->Reset();
	}

	if ( m_pHorizontalLookType )
	{
		CGameUIConVarRef joy_invertx( "joy_invertx" );
		int iInvert = ( joy_invertx.GetInt() > 0 ) ? CONTROLLER_LOOK_TYPE_INVERTED : CONTROLLER_LOOK_TYPE_NORMAL;
		m_pHorizontalLookType->SetCurrentSelection( pszLookTypes[iInvert] );
	}

	if ( m_pVerticalLookType )
	{
		CGameUIConVarRef joy_inverty( "joy_inverty" );
		int iInvert = ( joy_inverty.GetInt() > 0 ) ? CONTROLLER_LOOK_TYPE_INVERTED : CONTROLLER_LOOK_TYPE_NORMAL;
		m_pVerticalLookType->SetCurrentSelection( pszLookTypes[iInvert] );
	}

	if ( m_pDuckMode )
	{
		CGameUIConVarRef option_duck_method( "option_duck_method" );
		int iDuckMode = ( option_duck_method.GetInt() > 0 ) ? DUCK_MODE_TOGGLE : DUCK_MODE_HOLD;
		m_pDuckMode->SetCurrentSelection( pszDuckModes[iDuckMode] );
	}

	if ( m_pVibration )
	{
		CGameUIConVarRef joy_vibration( "joy_vibration" );
		int iEnable = (int)joy_vibration.GetBool();
		m_pVibration->SetCurrentSelection( pszEnableTypes[iEnable] );
	}
}

void ControllerOptions::OnThink()
{
	BaseClass::OnThink();

	if ( m_nResetControlValuesTicks >= 0 )
	{
		if ( m_nResetControlValuesTicks > 0 )
		{
			m_nResetControlValuesTicks--;
		}
		else
		{
			ResetControlValues();
			m_nResetControlValuesTicks = -1;
		}
	}
}

void ControllerOptionsResetDefaults_Confirm( void )
{
	s_pControllerOptions->ResetToDefaults();
}

void ControllerOptions::ResetToDefaults( void )
{
	int iOldSlot = engine->GetActiveSplitScreenPlayerSlot();
	engine->SetActiveSplitScreenPlayerSlot( m_iActiveUserSlot );

	if ( IsPC() )
	{
		engine->ClientCmd( "exec joy_pc_default.cfg" );
	}
	else
	{
		engine->ExecuteClientCmd( "exec config" PLATFORM_EXT ".cfg" );
	}

	engine->ExecuteClientCmd( "joy_cfg_preset 1" );
	engine->ExecuteClientCmd( "exec joy_preset_1" PLATFORM_EXT ".cfg" );

#ifdef _GAMECONSOLE
	int iCtrlr = XBX_GetUserId( m_iActiveUserSlot );
	UserProfileData upd;
	Q_memset( &upd, 0, sizeof( upd ) );
	if ( IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr ) )
	{
		upd = pPlayer->GetPlayerProfileData();
	}

	int nMovementControl = ( upd.action_movementcontrol == /* XPROFILE_ACTION_MOVEMENT_CONTROL_L_THUMBSTICK = */ 0 ) ? 0 : 1;
	engine->ExecuteClientCmd( VarArgs( "joy_movement_stick %d", nMovementControl ) );

	int nYinvert = upd.yaxis;
	engine->ExecuteClientCmd( VarArgs( "joy_inverty %d", nYinvert ) );

	engine->ExecuteClientCmd( "joy_invertx 0" );
#endif

	engine->SetActiveSplitScreenPlayerSlot( iOldSlot );

	m_nResetControlValuesTicks = 1; // used to delay polling the values until we've flushed the command buffer 

	m_bDirty = true;
}

void ControllerOptions::ConfirmUseDefaults()
{
	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().
		OpenWindow( WT_GENERICCONFIRMATION, this, false ) );

	if ( confirmation )
	{
		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#L4D360UI_Controller_Default";
		data.pMessageText =	"#L4D360UI_Controller_Default_Details";
		data.bOkButtonEnabled = true;
		data.bCancelButtonEnabled = true;
		data.pOkButtonText = "#PORTAL2_ButtonAction_Reset";

		s_pControllerOptions = this;

		data.pfnOkCallback = ControllerOptionsResetDefaults_Confirm;
		data.pfnCancelCallback = NULL;

		confirmation->SetUsageData(data);
	}
}

void ControllerOptions::OnKeyCodePressed(KeyCode code)
{
	int lastUser = GetJoystickForCode( code );
	if ( m_iActiveUserSlot != lastUser )
		return;

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
	case KEY_XBUTTON_LEFT:
	case KEY_XBUTTON_LEFT_SHOULDER:
	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_XBUTTON_RIGHT_SHOULDER:
	{
		// If they want to modify buttons or sticks with a direction, take them to the edit menu
		vgui::Panel *panel = FindChildByName( "BtnEditButtons" );
		if ( panel->HasFocus() )
		{
			CBaseModPanel::GetSingleton().OpenWindow( WT_CONTROLLER_BUTTONS, this, true,
				KeyValues::AutoDeleteInline( new KeyValues( "Settings", "slot", m_iActiveUserSlot ) ) );
			m_bDirty = true;
		}
		else
		{
			panel = FindChildByName( "BtnEditSticks" );
			if ( panel->HasFocus() )
			{
				CBaseModPanel::GetSingleton().OpenWindow( WT_CONTROLLER_STICKS, this, true,
					KeyValues::AutoDeleteInline( new KeyValues( "Settings", "slot", m_iActiveUserSlot ) ) );
				m_bDirty = true;
			}
			else
			{
				BaseClass::OnKeyCodePressed(code);
			}
		}
		break;
	}

	case KEY_XBUTTON_B:
		if ( m_bDirty || 
			 ( m_pVerticalSensitivity && m_pVerticalSensitivity->IsDirty() ) || 
			 ( m_pHorizontalSensitivity && m_pHorizontalSensitivity->IsDirty() ) )
		{
		}
		break;

#if !defined( _GAMECONSOLE )
	case KEY_XBUTTON_X:
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		ConfirmUseDefaults();
		break;
#endif

	}

	BaseClass::OnKeyCodePressed( code );
}

void ControllerOptions::OnCommand(const char *command)
{
	if ( !Q_strcmp( command, "EditButtons" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_CONTROLLER_BUTTONS, this, true,
			KeyValues::AutoDeleteInline( new KeyValues( "Settings", "slot", m_iActiveUserSlot ) ) );
		m_bDirty = true;
	}
	else if ( !Q_strcmp( command, "EditSticks" ) )
	{
		CBaseModPanel::GetSingleton().OpenWindow( WT_CONTROLLER_STICKS, this, true,
			KeyValues::AutoDeleteInline( new KeyValues( "Settings", "slot", m_iActiveUserSlot ) ) );
		m_bDirty = true;
	}
	else if ( !Q_strcmp( command, pszDuckModes[DUCK_MODE_HOLD] ) )
	{
		CGameUIConVarRef option_duck_method("option_duck_method");
		if ( option_duck_method.IsValid() )
		{
			option_duck_method.SetValue( DUCK_MODE_HOLD );
			m_bDirty = true;
		}
	}
	else if ( !Q_strcmp( command, pszDuckModes[DUCK_MODE_TOGGLE] ) )
	{
		CGameUIConVarRef option_duck_method("option_duck_method");
		if ( option_duck_method.IsValid() )
		{
			option_duck_method.SetValue( DUCK_MODE_TOGGLE );
			m_bDirty = true;
		}
	}	
	else if ( !V_stricmp( command, "HorizontalNormal" ) )
	{
		CGameUIConVarRef joy_invertx( "joy_invertx" );
		if ( joy_invertx.IsValid() )
		{
			joy_invertx.SetValue( CONTROLLER_LOOK_TYPE_NORMAL );
			m_bDirty = true;
		}
	}
	else if ( !V_stricmp( command, "HorizontalInverted" ) )
	{
		CGameUIConVarRef joy_invertx( "joy_invertx" );
		if ( joy_invertx.IsValid() )
		{
			joy_invertx.SetValue( CONTROLLER_LOOK_TYPE_INVERTED );
			m_bDirty = true;
		}
	}
	else if ( !Q_strcmp( command, "VerticalNormal" ) )
	{
		CGameUIConVarRef joy_inverty( "joy_inverty" );
		if ( joy_inverty.IsValid() )
		{
			joy_inverty.SetValue( CONTROLLER_LOOK_TYPE_NORMAL );
			m_bDirty = true;
		}
	}
	else if ( !Q_strcmp( command, "VerticalInverted" ) )
	{
		CGameUIConVarRef joy_inverty( "joy_inverty" );
		if ( joy_inverty.IsValid() )
		{
			joy_inverty.SetValue( CONTROLLER_LOOK_TYPE_INVERTED );
			m_bDirty = true;
		}
	}
	else if ( !V_stricmp( command, "VibrationDisabled" ) )
	{
		CGameUIConVarRef joy_vibration( "joy_vibration" );
		if ( joy_vibration.IsValid() )
		{
			joy_vibration.SetValue( false );
			m_bDirty = true;
		}
	}
	else if ( !V_stricmp( command, "VibrationEnabled" ) )
	{
		CGameUIConVarRef joy_vibration( "joy_vibration" );
		if ( joy_vibration.IsValid() )
		{
			joy_vibration.SetValue( true );
			m_bDirty = true;
		}
	}
	else if ( !V_stricmp( command, "ControllerDisabled" ) )
	{
		CGameUIConVarRef joystick( "joystick" );
		if ( joystick.IsValid() )
		{
			joystick.SetValue( false );
			m_bDirty = true;
		}
	}
	else if ( !V_stricmp( command, "ControllerEnabled" ) )
	{
		CGameUIConVarRef joystick( "joystick" );
		if ( joystick.IsValid() )
		{
			joystick.SetValue( true );
			m_bDirty = true;
		}
	}	else if ( !Q_strcmp( command, "Defaults" ) )
	{
		ConfirmUseDefaults();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void ControllerOptions::OnNotifyChildFocus( vgui::Panel* child )
{
}

void ControllerOptions::OnFlyoutMenuClose( vgui::Panel* flyTo )
{
	UpdateFooter();
}

void ControllerOptions::OnFlyoutMenuCancelled()
{
}

Panel *ControllerOptions::NavigateBack()
{
	if ( m_bDirty || 
		 ( m_pVerticalSensitivity && m_pVerticalSensitivity->IsDirty() ) || 
		 ( m_pHorizontalSensitivity && m_pHorizontalSensitivity->IsDirty() ) )
	{
		engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetUserId( m_iActiveUserSlot ) ) );
	}

	SetGameUIActiveSplitScreenPlayerSlot( 0 );

	return BaseClass::NavigateBack();
}

void ControllerOptions::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pVerticalSensitivity = dynamic_cast< SliderControl* >( FindChildByName( "SldVertSens" ) );
	m_pHorizontalSensitivity = dynamic_cast< SliderControl* >( FindChildByName( "SldHorizSens" ) );
	m_pHorizontalLookType = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpHorizontalLookType" ) );
	m_pVerticalLookType = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpVerticalLookType" ) );
	m_pDuckMode = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpDuckMode" ) );
	m_pEditButtons = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnEditButtons" ) );
	m_pEditSticks = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnEditSticks" ) );
	m_pVibration = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpVibration" ) );
	m_pController = dynamic_cast< BaseModHybridButton* >( FindChildByName( "DrpController" ) );

	ResetControlValues();

	if ( IsGameConsole() && m_pEditButtons )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		m_pEditButtons->NavigateTo();
		m_ActiveControl = m_pEditButtons;
	}
	else if ( !IsGameConsole() && m_pController )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}
		m_pController->NavigateTo();
		m_ActiveControl = m_pController;
	}

	UpdateFooter();
}

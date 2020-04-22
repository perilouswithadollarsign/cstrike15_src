//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VControllerOptionsButtons.h"
#include "VFooterPanel.h"
#include "VSliderControl.h"
#include "VDropDownMenu.h"
#include "VFlyoutMenu.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ILocalize.h"
#include "vhybridbutton.h"
#include "VControlsListPanel.h"
#include "inputsystem/iinputsystem.h"
#include "vgui/IInput.h"
#include "VGenericConfirmation.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

struct ControllerBindingMap
{
	vgui::KeyCode m_keyCode;
	const char *m_pszLabelName;
};

//this array represents the "configurable" buttons on the controller
static const ControllerBindingMap sControllerBindings[] =	
{
	{ KEY_XBUTTON_A,				"LblAButton" },
	{ KEY_XBUTTON_B,				"LblBButton" },
	{ KEY_XBUTTON_X,				"LblXButton" },
	{ KEY_XBUTTON_Y,				"LblYButton" },
	{ KEY_XBUTTON_LEFT_SHOULDER,	"LblLShoulder" },
	{ KEY_XBUTTON_RIGHT_SHOULDER,	"LblRShoulder" },
	{ KEY_XBUTTON_STICK1,			"LblLStick" },
	{ KEY_XBUTTON_STICK2,			"LblRStick" },
	{ KEY_XBUTTON_LTRIGGER,			"LblLTrigger" },
	{ KEY_XBUTTON_RTRIGGER,			"LblRTrigger" },
	{ IsPS3() ? KEY_XBUTTON_RIGHT : KEY_XBUTTON_LEFT,		"LblDPadPing" },
	{ IsPS3() ? KEY_XBUTTON_DOWN : KEY_XBUTTON_UP,		"LblDPadGest" },
};

// Map the important bindings to the string that should be displayed on the diagram
struct BindingDisplayMap
{
	const char *pszBinding;
	const char *pszDisplay;
};

static const BindingDisplayMap sBindingToDisplay[] = 
{
	{ "+mouse_menu_taunt",		"#P2Controller_Taunt" },
	{ "+mouse_menu",			"#P2Controller_Signal" },
	{ "+jump",					"#L4D360UI_Controller_Jump" },
	{ "+duck",					"#L4D360UI_Controller_Crouch" },
	{ "+use",					"#L4D360UI_Controller_Use" },
	{ "+remote_view",			"#P2Controller_RemoteView" },
	{ "+attack",				"#P2Controller_PortalBlue" },
	{ "+attack2",				"#P2Controller_PortalOrange" },
	{ "+quick_ping",			"#P2Controller_QuickPing" },
	{ "+zoom",					"#P2Controller_Zoom" },
};

static ButtonCode_t s_CustomBindingButtons[] =
{
	KEY_XBUTTON_A,
	KEY_XBUTTON_B,
	KEY_XBUTTON_X,
	KEY_XBUTTON_Y,
	KEY_XBUTTON_LEFT_SHOULDER,
	KEY_XBUTTON_RIGHT_SHOULDER,
	KEY_XBUTTON_LTRIGGER,
	KEY_XBUTTON_RTRIGGER,	
	KEY_XBUTTON_UP,
	KEY_XBUTTON_RIGHT,
	KEY_XBUTTON_DOWN,
	KEY_XBUTTON_LEFT,
	KEY_XBUTTON_STICK1,
	KEY_XBUTTON_STICK2,
};

// an encoding of joy_preset_1, a cheaper way to default/reset the custom bindings
struct DefaultCustomBinding_t
{
	ButtonCode_t	nButtonCode;
	const char		*pszBinding;
};
static DefaultCustomBinding_t s_DefaultCustomBinding[] =
{
	{ KEY_XBUTTON_A,					"+jump" },
	{ KEY_XBUTTON_B,					"+duck" },
	{ KEY_XBUTTON_X,					"+use" },
	{ KEY_XBUTTON_Y,					"+remote_view" },
	{ KEY_XBUTTON_LEFT_SHOULDER,		"+quick_ping" },
	{ KEY_XBUTTON_RIGHT_SHOULDER,		"+zoom" },
	{ KEY_XBUTTON_LTRIGGER,				"+attack2" },
	{ KEY_XBUTTON_RTRIGGER,				"+attack" },
	{ KEY_XBUTTON_UP,					"+mouse_menu_taunt" },
	{ KEY_XBUTTON_RIGHT,				"", },
	{ KEY_XBUTTON_DOWN,					"", },
	{ KEY_XBUTTON_LEFT,					"+mouse_menu" },
	{ KEY_XBUTTON_STICK1,				"" },
	{ KEY_XBUTTON_STICK2,				"" }
};

static const BindingDisplayMap s_CustomBindingToDisplay[] = 
{
	{ "",						"#P2Controller_NoAction" },
	{ "+jump",					"#L4D360UI_Controller_Jump" },
	{ "+duck",					"#L4D360UI_Controller_Crouch" },
	{ "+attack",				"#P2Controller_PortalBlue" },
	{ "+attack2",				"#P2Controller_PortalOrange" },
	{ "+use",					"#L4D360UI_Controller_Use" },
	{ "+zoom",					"#P2Controller_Zoom" },
	{ "+remote_view",			"#P2Controller_RemoteView" },
	{ "+mouse_menu_taunt",		"#P2Controller_Taunt" },
	{ "+mouse_menu",			"#P2Controller_Signal" },
	{ "+quick_ping",			"#P2Controller_QuickPing" },
};

enum
{
	CONTROLLER_BUTTONS_SPEC1 = 0,
	CONTROLLER_BUTTONS_SPEC2,
	CONTROLLER_BUTTONS_SPEC3,
	CONTROLLER_BUTTONS_SPEC4,
	NUM_CONTROLLER_BUTTONS_SETTINGS,
};

const char *pszButtonSettingsButtonName[NUM_CONTROLLER_BUTTONS_SETTINGS] =
{
	"BtnSpec1",
	"BtnSpec2",
	"BtnSpec3",
	"BtnSpec4"
};

const char *pszButtonSettingConfigs[NUM_CONTROLLER_BUTTONS_SETTINGS] = 
{
	"exec joy_preset_1" PLATFORM_EXT ".cfg",
	"exec joy_preset_2" PLATFORM_EXT ".cfg",
	"exec joy_preset_3" PLATFORM_EXT ".cfg",
	"exec joy_preset_4" PLATFORM_EXT ".cfg"
};

static void AcceptDefaultsOkCallback()
{
	ControllerOptionsButtons *pSelf = 
		static_cast< ControllerOptionsButtons* >( CBaseModPanel::GetSingleton().GetWindow( WT_CONTROLLER_BUTTONS ) );
	if ( pSelf )
	{
		pSelf->ResetCustomToDefault();
	}
}

void BaseModUI::ResetControllerConfig()
{
	CGameUIConVarRef joy_cfg_preset( "joy_cfg_preset" );

	// resetting the controller config needs to put back the binds after an "unbindall"
	if ( joy_cfg_preset.IsValid() )
	{
		// resetting the controller config IS NOT EXPECTED to blow away the user's custom settings
		if ( joy_cfg_preset.GetInt() == CONTROLLER_BUTTONS_SPECCUSTOM )
		{
			// rather it causes the binds to be reset TO the user's custom settings
			// the binds will be set to the user's custom configuration
			engine->ExecuteClientCmd( "joy_cfg_custom" );
		}
		else
		{
			// setup the binds according to the 1-of-n presets
			int iPreset = clamp( joy_cfg_preset.GetInt() - 1, 0, NUM_CONTROLLER_BUTTONS_SETTINGS-1 );
			engine->ExecuteClientCmd( pszButtonSettingConfigs[iPreset] );
		}
		return;
	}	

	// fallback and choose the first, which is the "default"
	engine->ExecuteClientCmd( pszButtonSettingConfigs[0] );
}

ControllerOptionsButtons::ControllerOptionsButtons( Panel *parent, const char *panelName ):
BaseClass(parent, panelName)
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	m_nRecalculateLabelsTicks = -1;
	m_iActiveUserSlot = -1;

	m_hKeyFont = vgui::INVALID_FONT;
	m_hHeaderFont = vgui::INVALID_FONT;

	m_nActionColumnWidth = 0;
	m_nButtonColumnWidth = 0;

	m_ActiveControl = NULL;

	m_bCustomizingButtons = false;
	m_bDirtyCustomConfig = false;

	SetDialogTitle( "#L4D360UI_Controller_Buttons_Title", NULL, true );

	m_pCustomBindList = new VControlsListPanel( this, "listpanel_custombindlist" );

	SetFooterEnabled( true );
}

ControllerOptionsButtons::~ControllerOptionsButtons()
{
}

void ControllerOptionsButtons::SetDataSettings( KeyValues *pSettings )
{
	m_iActiveUserSlot = pSettings->GetInt( "slot", CBaseModPanel::GetSingleton().GetLastActiveUserId() );
	SetGameUIActiveSplitScreenPlayerSlot( m_iActiveUserSlot );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( m_iActiveUserSlot );
}

void ControllerOptionsButtons::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_nActionColumnWidth = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "CustomButtonBindings.ActionColumnWidth" ) ) );
	m_nButtonColumnWidth = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "CustomButtonBindings.ButtonColumnWidth" ) ) );

	const char *pHeaderFontString = pScheme->GetResourceString( "CustomButtonBindings.HeaderFont" );
	if ( pHeaderFontString && pHeaderFontString[0] )
	{
		m_hHeaderFont = pScheme->GetFont( pHeaderFontString, true );
	}

	const char *pKeyFontString = pScheme->GetResourceString( "CustomButtonBindings.ButtonFont" );
	if ( pKeyFontString && pKeyFontString[0] )
	{
		m_hKeyFont = pScheme->GetFont( pKeyFontString, true );
	}

	PopulateCustomBindings();

	// Figure out which button and state should be default
	bool bCustomSettings = false;
	vgui::Panel *pFirstPanel = NULL;

	CGameUIConVarRef joy_cfg_preset( "joy_cfg_preset" ); 
	if ( joy_cfg_preset.IsValid() )
	{
		if ( joy_cfg_preset.GetInt() == CONTROLLER_BUTTONS_SPECCUSTOM )
		{
			pFirstPanel = FindChildByName( "BtnSpecCustom" );
			if ( pFirstPanel )
			{
				bCustomSettings = true;
			}
		}
		
		if ( !bCustomSettings )
		{
			int iPreset = clamp( joy_cfg_preset.GetInt() - 1, 0, NUM_CONTROLLER_BUTTONS_SETTINGS-1 );
			pFirstPanel = FindChildByName( pszButtonSettingsButtonName[iPreset] );
		}
	}

	SetupControlStates( bCustomSettings );

	if ( pFirstPanel )
	{
		if ( m_ActiveControl )
		{
			m_ActiveControl->NavigateFrom();
		}

		pFirstPanel->NavigateTo();
		m_ActiveControl = pFirstPanel;

		if ( IsPC() )
		{
			OnCommand( pFirstPanel->GetName() );
		}
	}

	UpdateFooter();
}

void ControllerOptionsButtons::SetupControlStates( bool bCustomSettings )
{
	EditablePanel *pContainer = dynamic_cast<EditablePanel *>( FindChildByName( "LabelContainer" ) );

	if ( pContainer )
	{
		pContainer->SetVisible( !bCustomSettings );
	}

	m_pCustomBindList->SetVisible( bCustomSettings );
	m_pCustomBindList->ClearSelection();

	// binding labels may expect to be delayed
	if ( !bCustomSettings && m_nRecalculateLabelsTicks == -1 )
	{
		RecalculateBindingLabels();
	}

	UpdateFooter();
}

void ControllerOptionsButtons::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int visibleButtons = FB_BBUTTON;
		if ( IsGameConsole() && m_pCustomBindList->IsVisible() && !m_bCustomizingButtons )
		{
			visibleButtons |= FB_ABUTTON;
		}

		if ( IsPC() && m_pCustomBindList->IsVisible() )
		{
			visibleButtons |= FB_LSHOULDER;

			pFooter->SetButtonText( FB_LSHOULDER, "#GameUI_UseDefaults" );
		}

		pFooter->SetButtons( visibleButtons );

		if ( visibleButtons & FB_ABUTTON )
		{
			pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Modify" );
		}
		else
		{
			pFooter->SetButtonText( FB_ABUTTON, "#L4D360UI_Select" );
		}

		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Controller_Done" );	
	}	
}

void ControllerOptionsButtons::OnMousePressed(vgui::MouseCode code)
{
	BaseClass::OnMousePressed( code );
}

void ControllerOptionsButtons::OnKeyCodePressed(KeyCode code)
{
	int lastUser = GetJoystickForCode( code );
	if ( m_iActiveUserSlot != lastUser )
		return;

	vgui::KeyCode basecode = GetBaseButtonCode( code );

	int nItemId = m_pCustomBindList->GetSelectedItem();

	switch( basecode )
	{
	case KEY_XBUTTON_A:
		if ( m_pCustomBindList->IsVisible() )
		{
			if ( !m_bCustomizingButtons )
			{
				m_bCustomizingButtons = true;
				Panel *pPanel = FindChildByName( "BtnSpecCustom" );
				if ( pPanel )
				{
					pPanel->NavigateFrom();
				}

				m_pCustomBindList->NavigateTo();
				m_pCustomBindList->ResetToTop();
				UpdateFooter();
			}
			else
			{
				// same as advancing to next binding selection
				OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_RIGHT, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
		}
		else
		{
			// Nav back when the select one of the options
			BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}
		return;

	case KEY_XBUTTON_B:
		if ( FinishCustomizingButtons() )
		{
			if ( IsGameConsole() )
			{
				Panel *pPanel = FindChildByName( "BtnSpecCustom" );
				if ( pPanel )
				{
					CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
					pPanel->NavigateTo();
					UpdateFooter();
					return;
				}
			}
		}
		break;

	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
	case KEY_XBUTTON_LEFT:
	case KEY_LEFT:
		if ( m_pCustomBindList->IsVisible() && m_bCustomizingButtons )
		{
			if ( SelectPreviousBinding( nItemId ) )
				return;
		}
		break;

	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case KEY_RIGHT:
		if ( m_pCustomBindList->IsVisible() && m_bCustomizingButtons )
		{
			if ( SelectNextBinding( nItemId ) )
				return;
		}
		break;

	case KEY_XBUTTON_LEFT_SHOULDER:
		if ( m_pCustomBindList->IsVisible() )
		{
			// use defaults
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			GenericConfirmation *pConfirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#PORTAL2_ButtonLayoutConf";
			data.pMessageText = "#PORTAL2_ButtonLayoutText";
			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptDefaultsOkCallback;
			data.pOkButtonText = "#PORTAL2_ButtonAction_Reset";
			data.bCancelButtonEnabled = true;
			pConfirmation->SetUsageData( data );
			return;
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
		if ( !m_bCustomizingButtons && m_ActiveControl )
		{
			OnCommand( m_ActiveControl->GetName() );
		}
		break;
#endif
	}

	BaseClass::OnKeyCodePressed( code );
}

bool ControllerOptionsButtons::SelectPreviousBinding( int nItemID )
{
	KeyValues *pItemData = m_pCustomBindList->GetItemData( m_pCustomBindList->GetItemIDFromRow( nItemID ) );
	if ( pItemData )
	{
		// select previous binding
		m_bDirtyCustomConfig = true;
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_CLICK );
		int nBindingId = BindingNameToIndex( pItemData->GetString( "Binding" ) );
		nBindingId = ( nBindingId - 1 + ARRAYSIZE( s_CustomBindingToDisplay ) ) % ARRAYSIZE( s_CustomBindingToDisplay );
		pItemData->SetString( "Binding", s_CustomBindingToDisplay[nBindingId].pszBinding );
		SetActionFromBinding( nItemID, true );
		return true;
	}

	return false;
}

bool ControllerOptionsButtons::SelectNextBinding( int nItemID )
{
	KeyValues *pItemData = m_pCustomBindList->GetItemData( m_pCustomBindList->GetItemIDFromRow( nItemID ) );
	if ( pItemData )
	{
		// select next binding
		m_bDirtyCustomConfig = true;
		CBaseModPanel::GetSingleton().PlayUISound( UISOUND_CLICK );
		int nBindingId = BindingNameToIndex( pItemData->GetString( "Binding" ) );
		nBindingId = ( nBindingId + 1 ) % ARRAYSIZE( s_CustomBindingToDisplay );
		pItemData->SetString( "Binding", s_CustomBindingToDisplay[nBindingId].pszBinding );
		SetActionFromBinding( nItemID, true );
		return true;
	}

	return false;
}

bool ControllerOptionsButtons::FinishCustomizingButtons()
{
	if ( !m_bCustomizingButtons )
		return false;

	m_bCustomizingButtons = false;
	if ( m_bDirtyCustomConfig )
	{
		SaveCustomBindings();
	}

	m_pCustomBindList->ResetToTop();
	m_pCustomBindList->ClearSelection();

	return true;
}

void ControllerOptionsButtons::RecalculateBindingLabels( void )
{
	// Populate the bindings labels with the currently bound keys
	EditablePanel *pContainer = dynamic_cast<EditablePanel *>( FindChildByName( "LabelContainer" ) );
	if ( !pContainer || !pContainer->IsVisible() )
		return;

	// whether sticks are bound in this layout
	bool bSticksBound = false;

	// for every button on the controller
	for ( int i = 0; i < ARRAYSIZE( sControllerBindings ); i++ )
	{
		// what is it bound to?
		vgui::KeyCode code = sControllerBindings[i].m_keyCode;
		Label *pLabel = dynamic_cast< Label * >( pContainer->FindChildByName( sControllerBindings[i].m_pszLabelName ) );
		if ( !pLabel )
			continue;
		pLabel->SetText( L"" );

		//int nJoystick = m_iActiveUserSlot;
		code = ButtonCodeToJoystickButtonCode( code, m_iActiveUserSlot );

		const char *pBinding = engine->Key_BindingForKey( code );
		if ( !pBinding )
			continue;

		// find the localized string for this binding and set the label text
		for( int j = 0; j < ARRAYSIZE( sBindingToDisplay ); j++ )
		{
			const BindingDisplayMap *entry = &( sBindingToDisplay[ j ] );

			if ( !Q_stricmp( pBinding, entry->pszBinding ) )
			{
				if ( Label *pLabel = dynamic_cast< Label * >( pContainer->FindChildByName( sControllerBindings[i].m_pszLabelName ) ) )
				{
					pLabel->SetText( entry->pszDisplay );

					switch ( sControllerBindings[i].m_keyCode )
					{
					case KEY_XBUTTON_STICK1:
					case KEY_XBUTTON_STICK2:
						bSticksBound = true;
					}
				}
			}
		}
	}

	// set callouts for when sticks are bound
	pContainer->SetControlVisible( "ControllerImageCallouts", bSticksBound );
}

void ControllerOptionsButtons::OnThink()
{
	BaseClass::OnThink();

	if ( m_nRecalculateLabelsTicks >= 0 )
	{
		if ( m_nRecalculateLabelsTicks > 0 )
		{
			m_nRecalculateLabelsTicks--;
		}
		else
		{
			RecalculateBindingLabels();
			m_nRecalculateLabelsTicks = -1;
		}
	}
}

void ControllerOptionsButtons::OnHybridButtonNavigatedTo( VPANEL defaultButton )
{
	if ( IsPC() )
		return;

	Panel *panel = ipanel()->GetPanel( defaultButton, GetModuleName() );
	if ( panel )
	{
		OnCommand( panel->GetName() );
	}
}

void ControllerOptionsButtons::OnCommand( const char *pCommand )
{
	CGameUIConVarRef joy_cfg_preset( "joy_cfg_preset" );

	if ( IsPC() )
	{
		FinishCustomizingButtons();

		for ( int i = 0; i < NUM_CONTROLLER_BUTTONS_SETTINGS; i++ )
		{
			BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( FindChildByName( pszButtonSettingsButtonName[i] ) );
			if ( pButton )
			{
				bool bActive = !V_stricmp( pCommand, pszButtonSettingsButtonName[i] );
				pButton->SetPostCheckMark( bActive );
			}
		}

		BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton* >( FindChildByName( "BtnSpecCustom" ) );
		if ( pButton )
		{
			bool bActive = !V_stricmp( pCommand, "BtnSpecCustom" );
			pButton->SetPostCheckMark( bActive );
		}
	}

	if ( !V_stricmp( pCommand, "BtnSpecCustom" ) )
	{
		if ( joy_cfg_preset.IsValid() )
		{
			joy_cfg_preset.SetValue( CONTROLLER_BUTTONS_SPECCUSTOM );
		}

		engine->ExecuteClientCmd( CFmtStr( "cmd%d joy_cfg_custom", m_iActiveUserSlot + 1 ) );

		SetupControlStates( true );
		return;
	}
	else
	{
		for ( int i=0; i<NUM_CONTROLLER_BUTTONS_SETTINGS; i++ )
		{
			if ( !Q_stricmp( pCommand, pszButtonSettingsButtonName[i] ) )
			{
				if ( joy_cfg_preset.IsValid() )
				{
					joy_cfg_preset.SetValue( i + 1 );
				}

				int iOldSlot = engine->GetActiveSplitScreenPlayerSlot();
				engine->SetActiveSplitScreenPlayerSlot( m_iActiveUserSlot );

				engine->ExecuteClientCmd( pszButtonSettingConfigs[i] );

				engine->SetActiveSplitScreenPlayerSlot( iOldSlot );

				m_nRecalculateLabelsTicks = 1; // used to delay polling the values until we've flushed the command buffer

				SetupControlStates( false );
				return;
			}
		}
	}

	BaseClass::OnCommand( pCommand );
}

Panel* ControllerOptionsButtons::NavigateBack()
{
	// parent dialog will write config
	return BaseClass::NavigateBack();
}

void ControllerOptionsButtons::PopulateCustomBindings( void )
{
	CGameUIConVarRef joy_cfg_custom_bindingsA( "joy_cfg_custom_bindingsA" );
	CGameUIConVarRef joy_cfg_custom_bindingsB( "joy_cfg_custom_bindingsB" );

	uint64 packedConfig = 0L;
	if ( joy_cfg_custom_bindingsA.IsValid() && joy_cfg_custom_bindingsB.IsValid() )
	{
		packedConfig = ( uint64 )atoi( joy_cfg_custom_bindingsB.GetString() );
		packedConfig <<= 32;
		packedConfig |= ( ( uint64 )atoi( joy_cfg_custom_bindingsA.GetString() ) ) & 0xFFFFFFFFL;
	}

	uint64 tempPackedConfig = packedConfig;
	bool bValid = ( tempPackedConfig != 0 );
	for ( int i = 0; bValid && i < ARRAYSIZE( s_CustomBindingButtons ); i++ )
	{
		int nBindingIndex = tempPackedConfig & 0x0FL;
		nBindingIndex--;
		tempPackedConfig >>= 4;

		if ( nBindingIndex < 0 || nBindingIndex >= ARRAYSIZE( s_CustomBindingToDisplay ) )
		{
			// invalid
			bValid = false;
		}
	}

	m_pCustomBindList->SetHeaderFont( m_hHeaderFont );

	m_pCustomBindList->AddSection( 1, "ROOT" );
	m_pCustomBindList->AddColumnToSection( 1, "Button", "#GameUI_KeyButton", SectionedListPanel::COLUMN_BRIGHT, m_nButtonColumnWidth );
	m_pCustomBindList->AddColumnToSection( 1, "Action", "#P2_Actions_Title", SectionedListPanel::COLUMN_BRIGHT, m_nActionColumnWidth );

	KeyValues *pItem = new KeyValues( "Item" );

	for ( int i = 0; i < ARRAYSIZE( s_CustomBindingButtons ); i++ )
	{
		int nBindingIndex = packedConfig & 0x0FL;
		nBindingIndex--;
		packedConfig >>= 4;

		if ( nBindingIndex < 0 || nBindingIndex >= ARRAYSIZE( s_CustomBindingToDisplay ) )
		{
			// should have already been prequalified above as invalid
			nBindingIndex = 0;
		}

		// fill in data
		pItem->SetWString( "DrawAsButton", ButtonCodeToString( s_CustomBindingButtons[i] ) );
		if ( !bValid )
		{
			// fill using default custom
			nBindingIndex = 0;
			for ( int j = 0; j < ARRAYSIZE( s_DefaultCustomBinding ); j++ )
			{
				if ( s_CustomBindingButtons[i] == s_DefaultCustomBinding[j].nButtonCode )
				{
					for ( int k = 0; k < ARRAYSIZE( s_CustomBindingToDisplay ); k++ )
					{
						if ( !V_stricmp( s_DefaultCustomBinding[j].pszBinding, s_CustomBindingToDisplay[k].pszBinding ) )
						{
							nBindingIndex = k;
							break;
						}
					}
					break;
				}
			}
		}
	
		if ( !nBindingIndex )
		{
			pItem->SetString( "Action", "" );
			pItem->SetString( "Binding", "" );
		}
		else
		{
			pItem->SetString( "Action", s_CustomBindingToDisplay[nBindingIndex].pszDisplay );
			pItem->SetString( "Binding", s_CustomBindingToDisplay[nBindingIndex].pszBinding );
		}

		pItem->SetBool( "Selected", false );
	
		// Add to list
		m_pCustomBindList->AddItem( 1, pItem );
	}
	pItem->deleteThis();

	for ( int i = 0; i < m_pCustomBindList->GetItemCount(); i++ )
	{
		int nItemId = m_pCustomBindList->GetItemIDFromRow( i );
		m_pCustomBindList->SetItemFont( nItemId, m_hKeyFont );
		m_pCustomBindList->SetInternalItemFont( m_hKeyFont );
	}
}

void ControllerOptionsButtons::ItemLeftClick( int nItemID )
{
	// item must be selected before L/R mouse click will have affect
	int nOldItemId = m_pCustomBindList->GetSelectedItem();
	if ( nOldItemId != nItemID )
		return;

	KeyValues *pItemData = m_pCustomBindList->GetItemData( nItemID );
	if ( pItemData && !pItemData->GetBool( "Selected" ) )
		return;

	if ( m_pCustomBindList->IsLeftArrowHighlighted() )
	{
		SelectPreviousBinding( nItemID );
	}
	else if ( m_pCustomBindList->IsRightArrowHighlighted() )
	{
		SelectNextBinding( nItemID );
	}
}

void ControllerOptionsButtons::ItemDoubleLeftClick( int nItemID )
{
	// item must be selected before L/R mouse click will have affect
	int nOldItemId = m_pCustomBindList->GetSelectedItem();
	if ( nOldItemId != nItemID )
		return;

	KeyValues *pItemData = m_pCustomBindList->GetItemData( nItemID );
	if ( pItemData && !pItemData->GetBool( "Selected" ) )
		return;

	if ( m_pCustomBindList->IsLeftArrowHighlighted() )
	{
		SelectPreviousBinding( nItemID );
	}
	else if ( m_pCustomBindList->IsRightArrowHighlighted() )
	{
		SelectNextBinding( nItemID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: User clicked on item: remember where last active row/column was
//-----------------------------------------------------------------------------
void ControllerOptionsButtons::ItemSelected( int nItemID )
{
	m_pCustomBindList->SetItemOfInterest( nItemID );

	for ( int i = 0; i < m_pCustomBindList->GetItemCount(); i++ )
	{
		// remove any other stale selection state
		KeyValues *pItemData = m_pCustomBindList->GetItemData( m_pCustomBindList->GetItemIDFromRow( i ) );
		if ( pItemData && pItemData->GetBool( "Selected" ) )
		{
			SetActionFromBinding( m_pCustomBindList->GetItemIDFromRow( i ), false );
		}
	}

	if ( IsPC() )
	{
		// PC can't supporting the same behavior as consoles (due to mouse clicking anywhere) which has a modal concept for modifying buttons
		// instead when the focus is on the list and there is a selection, that becomes the modifying state, otherwise not modifying.
		m_bCustomizingButtons = ( nItemID != -1 );
	}

	if ( nItemID == -1 )
	{
		return;
	}

	SetActionFromBinding( nItemID, true );
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
}

int ControllerOptionsButtons::BindingNameToIndex( const char *pName )
{
	for ( int i = 0; i < ARRAYSIZE( s_CustomBindingToDisplay ); i++ )
	{
		if ( !V_stricmp( pName, s_CustomBindingToDisplay[i].pszBinding ) )
		{
			return i;
		}
	}

	return 0;
}

bool ControllerOptionsButtons::SetActionFromBinding( int nItemID, bool bSelected )
{
	KeyValues *pItemData = m_pCustomBindList->GetItemData( nItemID );
	if ( !pItemData )
		return false;

	// lookup the current binding
	int nBindingIndex = BindingNameToIndex( pItemData->GetString( "Binding" ) );

	if ( bSelected )
	{
		// prevent the drawing in the selected state
		wchar_t *pLocalizedString = g_pVGuiLocalize->Find( s_CustomBindingToDisplay[nBindingIndex].pszDisplay );
		if ( !pLocalizedString )
		{
			pLocalizedString = L"???";
		}

		// hack to allow subclassed control to draw this column with L/R selection arrows
		pItemData->SetString( "Action", "" );
		pItemData->SetWString( "ActionText", pLocalizedString );
	}
	else
	{
		// restore the action to its unselected state
		if ( !nBindingIndex )
		{
			pItemData->SetString( "Action", "" );
		}
		else
		{
			pItemData->SetString( "Action", s_CustomBindingToDisplay[nBindingIndex].pszDisplay );
		}
	}

	// track the selected state
	pItemData->SetBool( "Selected", bSelected );

	m_pCustomBindList->InvalidateItem( nItemID );

	return true;
}

void ControllerOptionsButtons::SaveCustomBindings()
{
	// pack custom bindings into single 64 bit word
	uint64	packedConfig = 0;
	Assert( m_pCustomBindList->GetItemCount() == ARRAYSIZE( s_CustomBindingButtons ) );
	for ( int i = ARRAYSIZE( s_CustomBindingButtons ) - 1; i >= 0 ; i-- )
	{
		KeyValues *pItemData = m_pCustomBindList->GetItemData( m_pCustomBindList->GetItemIDFromRow( i ) );

		uint64 nBindingIndex = 0;
		if ( pItemData )
		{
			// lookup the current binding
			nBindingIndex = BindingNameToIndex( pItemData->GetString( "Binding" ) );
		}

		Assert( nBindingIndex >= 0 && nBindingIndex < ARRAYSIZE( s_CustomBindingToDisplay ) );

		packedConfig <<= 4;
		packedConfig |= ( nBindingIndex + 1 ) & 0x0FL;
	}

	// save low word
	CGameUIConVarRef joy_cfg_custom_bindingsA( "joy_cfg_custom_bindingsA" );
	joy_cfg_custom_bindingsA.SetValue( (int)( packedConfig & 0xFFFFFFFFL ) );

	// save high word
	CGameUIConVarRef joy_cfg_custom_bindingsB( "joy_cfg_custom_bindingsB" );
	joy_cfg_custom_bindingsB.SetValue( (int)( ( packedConfig >> 32 ) & 0xFFFFFFFFL ) );
}

const wchar_t *ControllerOptionsButtons::ButtonCodeToString( ButtonCode_t buttonCode )
{
	const char *pKeyString = NULL;
	switch ( buttonCode )
	{
	case KEY_XBUTTON_UP:
		pKeyString = "#GameUI_Icons_UP";
		break;
	case KEY_XBUTTON_RIGHT:
		pKeyString = "#GameUI_Icons_RIGHT";
		break;
	case KEY_XBUTTON_DOWN:
		pKeyString = "#GameUI_Icons_DOWN";
		break;
	case KEY_XBUTTON_LEFT:
		pKeyString = "#GameUI_Icons_LEFT";
		break;
	case KEY_XBUTTON_A:
		pKeyString = "#GameUI_Icons_A_BUTTON";
		break;
	case KEY_XBUTTON_B:
		pKeyString = "#GameUI_Icons_B_BUTTON";
		break;
	case KEY_XBUTTON_X:
		pKeyString = "#GameUI_Icons_X_BUTTON";
		break;
	case KEY_XBUTTON_Y:
		pKeyString = "#GameUI_Icons_Y_BUTTON";
		break;
	case KEY_XBUTTON_LEFT_SHOULDER:
		pKeyString = "#GameUI_Icons_L_SHOULDER";
		break;
	case KEY_XBUTTON_RIGHT_SHOULDER:
		pKeyString = "#GameUI_Icons_R_SHOULDER";
		break;
	case KEY_XBUTTON_LTRIGGER:
		pKeyString = "#GameUI_Icons_L_TRIGGER";
		break;
	case KEY_XBUTTON_RTRIGGER:
		pKeyString = "#GameUI_Icons_R_TRIGGER";
		break;
	case KEY_XBUTTON_STICK1:
		pKeyString = "#GameUI_Icons_LSTICK";
		break;
	case KEY_XBUTTON_STICK2:
		pKeyString = "#GameUI_Icons_RSTICK";
		break;
	}

	if ( pKeyString )
	{
		const wchar_t *pActualString = g_pVGuiLocalize->Find( pKeyString );
		if ( pActualString && pActualString[0] )
		{
			return pActualString;
		}
	}

	return L"";
}

void ControllerOptionsButtons::ResetCustomToDefault()
{
	// resets the custom button layout to its initial defaults
	for ( int i = 0; i < m_pCustomBindList->GetItemCount(); i++ )
	{
		// remove any other stale selection state
		KeyValues *pItemData = m_pCustomBindList->GetItemData( m_pCustomBindList->GetItemIDFromRow( i ) );
		if ( pItemData )
		{
			for ( int k = 0; k < ARRAYSIZE( s_DefaultCustomBinding ); k++ )
			{
				if ( s_CustomBindingButtons[i] == s_DefaultCustomBinding[k].nButtonCode )
				{
					pItemData->SetString( "Binding", s_DefaultCustomBinding[k].pszBinding );
					break;
				}
			}

			SetActionFromBinding( m_pCustomBindList->GetItemIDFromRow( i ), false );
		}
	}

	m_bDirtyCustomConfig = true;
	m_bCustomizingButtons = true;
	FinishCustomizingButtons();

	Panel *pPanel = FindChildByName( "BtnSpecCustom" );
	if ( pPanel )
	{
		pPanel->NavigateTo();
		UpdateFooter();
	}
}

CON_COMMAND( joy_cfg_custom, "" )
{
	CGameUIConVarRef joy_cfg_preset( "joy_cfg_preset" );
	CGameUIConVarRef joy_cfg_custom_bindingsA( "joy_cfg_custom_bindingsA" );
	CGameUIConVarRef joy_cfg_custom_bindingsB( "joy_cfg_custom_bindingsB" );

	CUtlString configString;
	bool bValid = false;

	if ( joy_cfg_preset.IsValid() && joy_cfg_custom_bindingsA.IsValid() && joy_cfg_custom_bindingsB.IsValid() )
	{
		uint64 packedConfig;
		packedConfig = ( uint64 )atoi( joy_cfg_custom_bindingsB.GetString() );
		packedConfig <<= 32;
		packedConfig |= ( ( uint64 )atoi( joy_cfg_custom_bindingsA.GetString() ) ) & 0xFFFFFFFFL;

		bValid = ( packedConfig != 0 );
		for ( int i = 0; bValid && i < ARRAYSIZE( s_CustomBindingButtons ); i++ )
		{
			int nBindingIndex = packedConfig & 0x0FL;
			nBindingIndex--;
			packedConfig >>= 4;

			if ( nBindingIndex < 0 || nBindingIndex >= ARRAYSIZE( s_CustomBindingToDisplay ) )
			{
				// invalid
				bValid = false;
				break;
			}

			if ( !nBindingIndex )
			{
				configString += CFmtStr( "unbind \"%s\"\n", g_pInputSystem->ButtonCodeToString( s_CustomBindingButtons[i] ) );
			}
			else
			{
				configString += CFmtStr( "bind \"%s\" \"%s\"\n", g_pInputSystem->ButtonCodeToString( s_CustomBindingButtons[i] ), s_CustomBindingToDisplay[nBindingIndex].pszBinding );
			}
		}
	}

	if ( !bValid )
	{
		if ( joy_cfg_custom_bindingsA.IsValid() && joy_cfg_custom_bindingsB.IsValid() )
		{
			// force invalid bindings to default state
			joy_cfg_custom_bindingsA.SetValue( 0 );
			joy_cfg_custom_bindingsB.SetValue( 0 );
		}

		configString.Clear();
		for ( int i = 0; i < ARRAYSIZE( s_DefaultCustomBinding ); i++ )
		{
			if ( !s_DefaultCustomBinding[i].pszBinding[0] )
			{
				configString += CFmtStr( "unbind \"%s\"\n", g_pInputSystem->ButtonCodeToString( s_DefaultCustomBinding[i].nButtonCode ) );
			}
			else
			{
				configString += CFmtStr( "bind \"%s\" \"%s\"\n", g_pInputSystem->ButtonCodeToString( s_DefaultCustomBinding[i].nButtonCode ), s_DefaultCustomBinding[i].pszBinding );
			}
		}
	}

	engine->ClientCmd( configString.Get() );
}

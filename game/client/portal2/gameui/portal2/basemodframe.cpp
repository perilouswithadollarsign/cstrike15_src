//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

// Client-tracing
#include "cbase.h"
#include "vgui/IInput.h"
#include "view_shared.h"
#include "cdll_client_int.h"
#ifdef XBX_GetPrimaryUserId
	#undef XBX_GetPrimaryUserId
#endif
#include "c_baseplayer.h"

#include <ctype.h>
#ifdef _PS3
#include <wctype.h>
#endif
#include "basemodframe.h"
#include "basemodpanel.h"
#include "transitionpanel.h"
#include "vhybridbutton.h"
#include "EngineInterface.h"

#include "VFooterPanel.h"
#include "VGenericConfirmation.h"
#include "VFlyoutMenu.h"
#include "IGameUIFuncs.h"

// vgui controls
#include <vgui/IVGui.h>
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "vgui_controls/Tooltip.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ilocalize.h"

#include "filesystem.h"
#include "fmtstr.h"
#include "gameconsole.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

//setup in GameUI_Interface.cpp
extern class IMatchSystem *matchsystem;
extern IGameUIFuncs *gameuifuncs;

ConVar ui_gameui_modal( "ui_gameui_modal", IsPC() ? "0" : "1", FCVAR_DEVELOPMENTONLY, "If set, the game UI pages will take modal input focus." );
ConVar ui_gameui_ctrlr_title( "ui_gameui_ctrlr_title", "0", FCVAR_DEVELOPMENTONLY, "" );

#define VIRTUAL_UI_COMMAND_PREFIX "virtual_ui_command_"

// arranged:
// a b c
// d e f
// g h i
const char *g_TileImageNames[MAX_MENU_TILES] =
{
	"vgui/menu_tiles/UI_tile_128_crnr_topL",
	"vgui/menu_tiles/UI_tile_128_top",
	"vgui/menu_tiles/UI_tile_128_crnr_topR",
	"vgui/menu_tiles/UI_tile_128_left",
	"vgui/menu_tiles/UI_tile_128_interior1",
	"vgui/menu_tiles/UI_tile_128_interior_alt",
	"vgui/menu_tiles/UI_tile_128_right",
	"vgui/menu_tiles/UI_tile_128_crnr_bottomL",
	"vgui/menu_tiles/UI_tile_128_bottom",
	"vgui/menu_tiles/UI_tile_128_crnr_bottomR",
};

const char *g_AltTileImageNames[MAX_MENU_TILES] =
{
	"vgui/menu_tiles/alt/UI_tile_128_crnr_topL",
	"vgui/menu_tiles/alt/UI_tile_128_top",
	"vgui/menu_tiles/alt/UI_tile_128_crnr_topR",
	"vgui/menu_tiles/alt/UI_tile_128_left",
	"vgui/menu_tiles/alt/UI_tile_128_interior1",
	"vgui/menu_tiles/alt/UI_tile_128_interior_alt",
	"vgui/menu_tiles/alt/UI_tile_128_right",
	"vgui/menu_tiles/alt/UI_tile_128_crnr_bottomL",
	"vgui/menu_tiles/alt/UI_tile_128_bottom",
	"vgui/menu_tiles/alt/UI_tile_128_crnr_bottomR",
};

const char *g_AltRandomTileImageNames[2] =
{
	"vgui/menu_tiles/alt/UI_tile_128_interior2",
	"vgui/menu_tiles/alt/UI_tile_128_interior3",
};

//=============================================================================
//
//=============================================================================
CUtlVector< IBaseModFrameListener * > CBaseModFrame::m_FrameListeners;

bool CBaseModFrame::m_DrawTitleSafeBorder = false;

//=============================================================================
CBaseModFrame::CBaseModFrame( vgui::Panel *parent, const char *panelName, bool okButtonEnabled, 
	bool cancelButtonEnabled, bool imgBloodSplatterEnabled, bool doButtonEnabled ):
		BaseClass(parent, panelName, true, false ),
		m_ActiveControl(0),
		m_FooterEnabled(false),
		m_OkButtonEnabled(okButtonEnabled),
		m_CancelButtonEnabled(cancelButtonEnabled),
		m_WindowType(WT_NONE),
		m_WindowPriority(WPRI_NORMAL),
		m_CanBeActiveWindowType(false),
		m_pVuiSettings( NULL )
{
	m_bIsFullScreen = false;
	m_bLayoutLoaded = false;
	m_bDelayPushModalInputFocus = false;

	SetConsoleStylePanel( true );

	// bodge to disable the frames title image and display our own 
	//(the frames title has an invalid zpos and does not draw over the garnish)
	Frame::SetTitle("", false);
	m_LblTitle = new Label(this, "LblTitle", "");

	Q_snprintf(m_ResourceName, sizeof( m_ResourceName ), "Resource/UI/BaseModUI/%s.res", panelName);
	m_pResourceLoadConditions = NULL;

#ifdef _GAMECONSOLE
	m_PassUnhandledInput = false;
#endif
	m_NavBack = NULL;
	m_bCanNavBack = false;

	SetScheme( CBaseModPanel::GetSingleton().GetScheme() );

	DisableFadeEffect();

	m_nDialogStyle = 0;

	m_hTitleFont = vgui::INVALID_FONT;
	m_hButtonFont = vgui::INVALID_FONT;
	m_hSubTitleFont = vgui::INVALID_FONT;

	m_nOriginalWide = 0;
	m_nOriginalTall = 0;

	m_nTilesWide = 0;
	m_nTilesTall = 0;
	
	m_nTitleOffsetX = 0;
	m_nTitleOffsetY = 0;

	m_nTitleWide = 0;
	m_nTitleTall = 0;
	m_nTitleTilesWide = 0;

	m_TitleColor = Color( 0, 0, 0, 255 );
	m_TitleColorAlt = Color( 154, 167, 164, 255 );

	m_MessageBoxTitleColor = Color( 0, 0, 0, 255 );
	m_MessageBoxTitleColorAlt = Color( 154, 167, 164, 255 );

	m_SubTitleColor = Color( 150, 150, 150, 255 );
	m_SubTitleColorAlt = Color( 201, 211, 207, 255 );

	m_TitleString[0] = 0;
	m_SubTitleString[0] = 0;

	m_nTileWidth = 0;	
	m_nTileHeight = 0;	

	m_nPinFromBottom = 0;
	m_nPinFromLeft = 0;
	m_nFooterOffsetY = 0;

	COMPILE_TIME_ASSERT( ARRAYSIZE( g_TileImageNames ) == ARRAYSIZE( m_nTileImageId ) );
	for ( int i = 0; i < ARRAYSIZE( m_nTileImageId ); i++ )
	{
		m_nTileImageId[i] = -1;
	}

	COMPILE_TIME_ASSERT( ARRAYSIZE( g_AltTileImageNames ) == ARRAYSIZE( g_TileImageNames ) );
	COMPILE_TIME_ASSERT( ARRAYSIZE( g_AltTileImageNames ) == ARRAYSIZE( m_nAltTileImageId ) );
	for ( int i = 0; i < ARRAYSIZE( m_nAltTileImageId ); i++ )
	{
		m_nAltTileImageId[i] = -1;
	}

	// Random tiles for flavor
	COMPILE_TIME_ASSERT( ARRAYSIZE( g_AltRandomTileImageNames ) == ARRAYSIZE( m_nAltRandomTileImageId ) );
	for ( int i = 0; i < ARRAYSIZE( m_nAltRandomTileImageId ); i++ )
	{
		m_nAltRandomTileImageId[i] = -1;
	}	

	m_bUseAlternateTiles = false;
	m_bLayoutFixed = false;
	m_bShowController = false;
}

//=============================================================================
CBaseModFrame::~CBaseModFrame()
{
	// Purposely not destroying our texture IDs, they are finds.
	// The naive create/destroy pattern causes excessive i/o for no benefit.
	// These images will always have to be there anyways.

	delete m_LblTitle;

	if ( m_pVuiSettings )
		m_pVuiSettings->deleteThis();
	m_pVuiSettings = NULL;

	m_arrVirtualUiControls.PurgeAndDeleteElements();
}

//=============================================================================
void CBaseModFrame::SetTitle( const char *title, bool surfaceTitle )
{
	m_LblTitle->SetText(title);

	int wide, tall;
	m_LblTitle->GetContentSize(wide, tall);
	m_LblTitle->SetSize(wide, tall);
}

//=============================================================================
void CBaseModFrame::SetTitle( const wchar_t *title, bool surfaceTitle )
{
	m_LblTitle->SetText(title);

	int wide, tall;
	m_LblTitle->GetContentSize(wide, tall);
	m_LblTitle->SetSize(wide, tall);
}

//=============================================================================
void CBaseModFrame::LoadLayout()
{
	int wide, tall;
	m_LblTitle->GetContentSize( wide, tall );
	m_LblTitle->SetSize( wide, tall );

	LoadControlSettings( m_ResourceName, NULL, NULL, GetResourceLoadConditions() );
	MakeReadyForUse();

	SetTitleBarVisible(true);
	SetMoveable(false);
	SetCloseButtonVisible(false);
	SetMinimizeButtonVisible(false);
	SetMaximizeButtonVisible(false);
	SetMenuButtonVisible(false);
	SetMinimizeToSysTrayButtonVisible(false);
	SetSizeable(false);

	// determine if we're full screen
	int screenwide,screentall;
	vgui::surface()->GetScreenSize( screenwide, screentall );
	if ( ( GetWide() == screenwide ) && ( GetTall() == screentall ) )
	{
		m_bIsFullScreen = true;
	}

	// APS: Very bad hack to prevent appmodel focus for the panel
	// When the console drops down, the ui can't maintain the appmodal focus
	// This needs to get fixed properly when PC gets closer to ship
	if ( IsPC() && GameConsole().IsConsoleVisible() && !m_bIsFullScreen )
	{
		m_bIsFullScreen = true;
	}

	m_bLayoutLoaded = true;

	// if we were supposed to take modal input focus, do it now
	if ( m_bDelayPushModalInputFocus )
	{
		if ( ui_gameui_modal.GetBool() )
			PushModalInputFocus();
		m_bDelayPushModalInputFocus = false;
	}
}

//=============================================================================

void CBaseModFrame::SetDataSettings( KeyValues *pSettings )
{
	return;
}

void CBaseModFrame::ReloadSettings()
{
	LoadControlSettings( m_ResourceName );
	InvalidateLayout( false, true );
	MakeReadyForUse();
}

//=============================================================================
void CBaseModFrame::OnKeyCodePressed(KeyCode keycode)
{
	int lastUser = GetJoystickForCode( keycode );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( lastUser );

	vgui::KeyCode code = GetBaseButtonCode( keycode );

	switch(code)
	{
	case KEY_XBUTTON_A:
		if(m_OkButtonEnabled)
		{
			BaseClass::OnKeyCodePressed(keycode);
		}
		break;
	case KEY_XBUTTON_B:
		if(m_CancelButtonEnabled)
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
			NavigateBack();
		}
		break;
	case KEY_XBUTTON_STICK1:
#ifdef BASEMOD360UI_DEBUG
		ToggleTitleSafeBorder();
#endif
		break;
	case KEY_XBUTTON_STICK2:
#ifdef BASEMOD360UI_DEBUG
		{
			//DEBUG: Remove this later
			CBaseModFooterPanel *footer = CBaseModPanel::GetSingleton().GetFooterPanel();
			if( footer )
			{
				footer->LoadLayout();
			}

			LoadLayout();
			//ENDDEBUG
			break;
		}
#endif
	default:
		BaseClass::OnKeyCodePressed(keycode);
		break;
	}

	// HACK: Allow F key bindings to operate even here
	if ( IsPC() && keycode >= KEY_F1 && keycode <= KEY_F12 )
	{
		// See if there is a binding for the FKey
		const char *binding = gameuifuncs->GetBindingForButtonCode( keycode );
		if ( binding && binding[0] )
		{
			// submit the entry as a console commmand
			char szCommand[256];
			Q_strncpy( szCommand, binding, sizeof( szCommand ) );
			engine->ClientCmd_Unrestricted( szCommand );
		}
	}
}

#ifndef _GAMECONSOLE
void CBaseModFrame::OnKeyCodeTyped( vgui::KeyCode code )
{
	WINDOW_TYPE currWindow = CBaseModPanel::GetSingleton().GetActiveWindowType();
	// For PC, this maps space bar to OK and esc to cancel
	switch ( code )
	{
	case KEY_SPACE:
		// don't special handle spacebar for portal2 leaderboard UI
		if (  currWindow != WT_PORTALLEADERBOARD && currWindow != WT_PORTALCOOPLEADERBOARD && currWindow != WT_PORTALLEADERBOARDHUD )
		{
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}
		break;

	case KEY_ESCAPE:
		// close active menu if there is one, else navigate back
		if ( FlyoutMenu::GetActiveMenu() )
		{
			FlyoutMenu::CloseActiveMenu( FlyoutMenu::GetActiveMenu()->GetNavFrom() );
		}
		else
		{
			OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		}
		break;
	}

	BaseClass::OnKeyTyped( code );
}
#endif


void CBaseModFrame::OnMousePressed( vgui::MouseCode code )
{
	if ( m_pVuiSettings )
	{
		char const *szHitTest = GetEntityOverMouseCursorInEngine();
		ExecuteCommandForEntity( szHitTest );
	}

	if( FlyoutMenu::GetActiveMenu() )
	{
		FlyoutMenu::CloseActiveMenu( FlyoutMenu::GetActiveMenu()->GetNavFrom() );
	}
	
	if ( CBaseModPanel::GetSingleton().GetActiveWindowType() == GetWindowType() )
	{
		RestoreFocusToActiveControl();
	}

	BaseClass::OnMousePressed( code );
}

void CBaseModFrame::ExecuteCommandForEntity( char const *szEntityName )
{
	if ( !szEntityName || !*szEntityName )
		return;

	// Check if the virtual UI defines such command
	for ( KeyValues *kvCmd = m_pVuiSettings->FindKey( "commands" )->GetFirstValue(); kvCmd; kvCmd = kvCmd->GetNextValue() )
	{
		char const *szCmdName = kvCmd->GetName();
		bool bMatch = false;
		switch ( szCmdName[0] )
		{
		case '^':
			bMatch = StringHasPrefix( szEntityName, szCmdName + 1 );
			break;
		default:
			bMatch = !Q_stricmp( szEntityName, szCmdName );
			break;
		}
		if ( bMatch )
		{
			OnCommand( kvCmd->GetString() );
			break;
		}
	}
}

void CBaseModFrame::GetEntityNameForControl( char const *szControlId, char chEntityName[256] )
{
	chEntityName[0] = 0;
	if ( KeyValues *kvCtrl = m_pVuiSettings->FindKey( "controls" )->FindKey( szControlId ) )
	{
		char const *szEntityName = kvCtrl->GetString( "entity" );
		ResolveEntityName( szEntityName, chEntityName );
	}
}

void CBaseModFrame::ResolveEntityName( char const *szEntityName, char chEntityName[256] )
{
	switch ( szEntityName[0] )
	{
	case '^':
		Q_snprintf( chEntityName, 256, "%s%s", szEntityName + 1, m_pVuiSettings->GetString( "entities/suffix" ) );
		break;
	default:
		Q_snprintf( chEntityName, 256, "%s", szEntityName );
		break;
	}
}

void CBaseModFrame::OnThink()
{
	if ( IsPC() && m_pVuiSettings )
	{
		// Mouse-over tracking
		char const *szHitName = "";
		if ( IsVisible() && !CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION ) )
		{
			szHitName = GetEntityOverMouseCursorInEngine();
		}
		g_BackgroundMapActiveControlManager.NavigateToEntity( szHitName );
	}

	BaseClass::OnThink();
}

void CBaseModFrame::Activate()
{
	BaseClass::Activate();

	// Navigate to default entity
	if ( char const *szDefaultEntity = m_pVuiSettings->GetString( "default/entity", NULL ) )
	{
		char chEntityName[256];
		ResolveEntityName( szDefaultEntity, chEntityName );
		g_BackgroundMapActiveControlManager.NavigateToEntity( chEntityName );
	}

	if ( char const *szDefaultControl = m_pVuiSettings->GetString( "default/control", NULL ) )
	{
		char const *szControlName = m_pVuiSettings->GetString( CFmtStr( "controls/%s/name", szDefaultControl ), szDefaultControl );
		if ( vgui::Panel *pDefault = FindChildByName( szControlName ) )
		{
			if ( m_ActiveControl )
				m_ActiveControl->NavigateTo();
			else
				pDefault->NavigateTo();
		}
	}

	// Setup a new seed
	if ( UsesAlternateTiles() )
	{
		m_nAltSeed = random->RandomInt( 0, 99999999 );
		m_RandomStream.SetSeed( m_nAltSeed );
	}
}

void CBaseModFrame::OnCommand( char const *szCommand )
{
	if ( char const *szControlId = StringAfterPrefix( szCommand, VIRTUAL_UI_COMMAND_PREFIX ) )
	{
		char chEntity[256] = {0};
		GetEntityNameForControl( szControlId, chEntity );
		ExecuteCommandForEntity( chEntity );
		return;
	}

	BaseClass::OnCommand( szCommand );
}

//=============================================================================
void CBaseModFrame::OnOpen()
{
	Panel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		if ( GetFooterEnabled() )
		{
			pFooter->SetVisible( true );
			pFooter->MoveToFront();
		}
		else
		{
			pFooter->SetVisible( false );
		}	
	}
	
	SetAlpha(255);//make sure we show up.
	Activate();

	if ( m_ActiveControl != 0 )
	{
		m_ActiveControl->NavigateTo();
	}

	// close active menu if there is one
	FlyoutMenu::CloseActiveMenu();

	PushModalInputFocus();
}

//======================================================================\=======
void CBaseModFrame::OnClose()
{
	FindAndSetActiveControl();

	if ( ui_gameui_modal.GetBool() )
		PopModalInputFocus();

	WINDOW_PRIORITY pri = GetWindowPriority();
	WINDOW_TYPE wt = GetWindowType();
	bool bLetBaseKnowOnFrameClosed = true;

	if ( wt == WT_GENERICWAITSCREEN )
	{
		CBaseModFrame *pFrame = CBaseModPanel::GetSingleton().GetWindow( wt );
		if ( pFrame && pFrame != this )
		{
			// Hack for generic waitscreen closing after another
			// waitscreen spawned
			bLetBaseKnowOnFrameClosed = false;
		}
	}

	BaseClass::OnClose();

	if ( bLetBaseKnowOnFrameClosed )
	{
		CBaseModPanel::GetSingleton().OnFrameClosed( pri, wt );
	}
}

//=============================================================================
Panel* CBaseModFrame::NavigateBack()
{
	// don't do anything if we have nowhere to navigate back to
	if ( !CanNavBack() )
		return NULL;

	CBaseModFrame* navBack = GetNavBack();
	CBaseModFrame* mainMenu = CBaseModPanel::GetSingleton().GetWindow(WT_MAINMENU);

	// fix to cause panels navigating back to the main menu to close without
	// fading so weird blends do not occur
	bool bDisableFade = (mainMenu != NULL && GetNavBack() == mainMenu);

	if ( bDisableFade )
	{
		SetFadeEffectDisableOverride(true);
	}

	Close();

	// If our nav back pointer is NULL, that means the thing we are supposed to nav back to has closed.  Just nav back to 
	// the topmost active window.
	if ( !navBack )
	{
		WINDOW_TYPE wt = CBaseModPanel::GetSingleton().GetActiveWindowType();
		if ( wt > WT_NONE )
		{
			navBack = CBaseModPanel::GetSingleton().GetWindow( wt );
		}
	}

	if ( navBack )
	{
		if ( ( GetWindowType() == WT_GENERICWAITSCREEN ) &&
			( CBaseModPanel::GetSingleton().GetActiveWindowPriority() > WPRI_WAITSCREEN ) )
		{
			// Don't actually activate parent to avoid setting it visible while message
			// box is already visible
		}
		else if ( ( GetWindowType() == WT_GENERICCONFIRMATION ) &&
			( CBaseModPanel::GetSingleton().GetActiveWindowPriority() >= WPRI_WAITSCREEN ) )
		{
			// Don't actually activate parent to avoid setting it visible while
			// waitscreen is already visible
		}
		else
		{
			if ( CBaseModPanel::GetSingleton().GetActiveWindowPriority() <= navBack->GetWindowPriority() )
			{
				navBack->SetVisible( true );
			}
			CBaseModPanel::GetSingleton().SetActiveWindow( navBack );
		}
		
		// Flip our background video
		if ( UsesAlternateTiles() != navBack->UsesAlternateTiles() )
		{
			CBaseModPanel::GetSingleton().OnTileSetChanged();
		}
	}	

	if ( bDisableFade )
	{
		SetFadeEffectDisableOverride(false);
	}

	CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->SetExpectedDirection( false, navBack ? navBack->GetWindowType() : WT_NONE );

	return navBack;
}

//=============================================================================
void CBaseModFrame::PostChildPaint()
{
	BaseClass::PostChildPaint();

	if ( m_DrawTitleSafeBorder )
	{
		int screenwide, screenTall;
		surface()->GetScreenSize(screenwide, screenTall);

		int titleSafeWide, titleSafeTall;

		int x, y, wide, tall;
		GetBounds(x, y, wide, tall);

		if ( IsX360() || !IsPS3() )
		{
			// Required Xbox360 Title safe is TCR documented at inner 90% (RED)
			titleSafeWide = screenwide * 0.05f;
			titleSafeTall = screenTall * 0.05f;

			if ( (titleSafeWide >= x) &&
				(titleSafeTall >= y) &&
				((screenwide - titleSafeWide) <= (wide - x)) &&
				((screenTall - titleSafeTall) <= (tall - y)) )
			{
				surface()->DrawSetColor(255, 0, 0, 255);
				surface()->DrawOutlinedRect(titleSafeWide - x, titleSafeTall - y, 
					(screenwide - titleSafeWide) - x, (screenTall - titleSafeTall) - y);
			}

			// Suggested Xbox360 Title Safe is TCR documented at inner 85% (YELLOW)
			titleSafeWide = screenwide * 0.075f;
			titleSafeTall = screenTall * 0.075f;

			if ( (titleSafeWide >= x) &&
				(titleSafeTall >= y) &&
				((screenwide - titleSafeWide) <= (wide - x)) &&
				((screenTall - titleSafeTall) <= (tall - y)) )
			{
				surface()->DrawSetColor(255, 255, 0, 255);
				surface()->DrawOutlinedRect(titleSafeWide - x, titleSafeTall - y, 
					(screenwide - titleSafeWide) - x, (screenTall - titleSafeTall) - y);
			}
		}
		else if ( IsPS3() )
		{
			// Required PS3 Title Safe is TCR documented at inner 85% (RED)
			titleSafeWide = screenwide * 0.075f;
			titleSafeTall = screenTall * 0.075f;

			if ( (titleSafeWide >= x) &&
				(titleSafeTall >= y) &&
				((screenwide - titleSafeWide) <= (wide - x)) &&
				((screenTall - titleSafeTall) <= (tall - y)) )
			{
				surface()->DrawSetColor( 255, 0, 0, 255 );
				surface()->DrawOutlinedRect(titleSafeWide - x, titleSafeTall - y, 
					(screenwide - titleSafeWide) - x, (screenTall - titleSafeTall) - y);
			}
		}
	}
}

//=============================================================================
void CBaseModFrame::PaintBackground()
{
	if ( m_nDialogStyle )
	{
		DrawDialogBackground();
	}
	else
	{
		BaseClass::PaintBackground();
	}
}

//=============================================================================
void CBaseModFrame::FindAndSetActiveControl()
{
}

void CBaseModFrame::RestoreFocusToActiveControl()
{
	RequestFocus();
	if ( m_ActiveControl )
	{
		m_ActiveControl->NavigateTo();
	}
	else
	{
	}
}


//=============================================================================
void CBaseModFrame::RunFrame()
{
}

//-----------------------------------------------------------------------------
void CBaseModFrame::OnNavigateTo( const char* panelName )
{
	for(int i = 0; i < GetChildCount(); ++i)
	{
		Panel* child = GetChild(i);
		if(child != NULL && (!Q_strcmp(panelName, child->GetName())))
		{						
			m_ActiveControl = child;

			if ( BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton * >( m_ActiveControl ) )
			{
				char const *szCommand = pButton->GetCommand()->GetString( "command" );
				if ( char const *szControlId = StringAfterPrefix( szCommand, VIRTUAL_UI_COMMAND_PREFIX ) )
				{
					char chEntity[256] = {0};
					GetEntityNameForControl( szControlId, chEntity );
					g_BackgroundMapActiveControlManager.NavigateToEntity( chEntity );
				}
			}
			break;
		}
	}
}

//=============================================================================
void CBaseModFrame::AddFrameListener( IBaseModFrameListener * frameListener )
{
	if(m_FrameListeners.Find(frameListener) == -1)
	{
		int index = m_FrameListeners.AddToTail();
		m_FrameListeners[index] = frameListener;
	}
}

//=============================================================================
void CBaseModFrame::RemoveFrameListener( IBaseModFrameListener * frameListener )
{
	m_FrameListeners.FindAndRemove(frameListener);
}

//=============================================================================
void CBaseModFrame::RunFrameOnListeners()
{
	for(int i = 0; i < m_FrameListeners.Count(); ++i)
	{
		m_FrameListeners[i]->RunFrame();
	}
}

//=============================================================================
bool CBaseModFrame::GetFooterEnabled()
{
	return m_FooterEnabled;
}

//=============================================================================
void CBaseModFrame::CloseWithoutFade()
{
	SetFadeEffectDisableOverride(true);

	FindAndSetActiveControl();

	Close();

	SetFadeEffectDisableOverride(false);
}

//=============================================================================
void CBaseModFrame::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	const char *pFont = NULL;
	pFont = pScheme->GetResourceString( "FrameTitleBar.Font" );
	m_LblTitle->SetFont( pScheme->GetFont( (pFont && *pFont) ? pFont : "Default", IsProportional() ) );
	
	m_hTitleFont = pScheme->GetFont( pScheme->GetResourceString( "Dialog.TitleFont" ), true );
	m_hButtonFont = pScheme->GetFont( pScheme->GetResourceString( "Dialog.ButtonFont" ), true );

	m_hSubTitleFont = pScheme->GetFont( "FriendsListSmall" /*"DialogButton"*/, true );

	m_TitleColor = GetSchemeColor( "Dialog.TitleColor", pScheme );
	m_TitleColorAlt = GetSchemeColor( "Dialog.TitleColorAlt", pScheme );
	m_MessageBoxTitleColor = GetSchemeColor( "Dialog.MessageBoxTitleColor", pScheme );
	m_MessageBoxTitleColorAlt = GetSchemeColor( "Dialog.MessageBoxTitleColorAlt", pScheme );

	m_nTitleOffsetX = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.TitleOffsetX" ) ) );
	m_nTitleOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.TitleOffsetY" ) ) );

	m_nTileWidth = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.TileWidth" ) ) );
	m_nTileHeight = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.TileHeight" ) ) );

	m_nPinFromBottom = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.PinFromBottom" ) ) );
	m_nPinFromLeft = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "Dialog.PinFromLeft" ) ) );

	m_nFooterOffsetY = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "FooterPanel.OffsetY" ) ) );

	for ( int i = 0; i < ARRAYSIZE( m_nTileImageId ); i++ )
	{
		m_nTileImageId[i] = CBaseModPanel::GetSingleton().GetImageId( g_TileImageNames[i] );
		m_nAltTileImageId[i] = CBaseModPanel::GetSingleton().GetImageId( g_AltTileImageNames[i] );
	}

	for ( int i = 0; i < ARRAYSIZE( m_nAltRandomTileImageId ); i++ )
	{
		m_nAltRandomTileImageId[i] = CBaseModPanel::GetSingleton().GetImageId( g_AltRandomTileImageNames[i] );
	}

	LoadLayout();
	DisableFadeEffect();

	if ( m_nDialogStyle )
	{
		// required for new style
		SetPaintBackgroundEnabled( true );
		SetupAsDialogStyle();

		// set the cursor to the first control
		for ( int i = 0; i < GetChildCount(); i++ )
		{
			Panel *pPanel = GetChild( i );
			BaseModHybridButton *pButton = dynamic_cast< BaseModHybridButton * >( pPanel );
			if ( pButton && pButton->IsEnabled() && pButton->IsVisible() )
			{
				if ( m_ActiveControl )
				{
					m_ActiveControl->NavigateFrom();
				}
				pButton->NavigateTo();
				break;
			}
		}
	}
}

void CBaseModFrame::PerformLayout()
{
	BaseClass::PerformLayout();

	// gets called multiple times, need to wait until dialog style has been applied
	if ( m_nDialogStyle && m_nTitleTall && !m_bLayoutFixed )
	{
		for ( int i = 0; i < GetChildCount(); i++ )
		{
			Panel *pChild = GetChild( i );
			int x, y, wide, tall;
			pChild->GetBounds( x, y, wide, tall );

			if ( m_nDialogStyle != DS_WAITSCREEN )
			{
				if ( m_nTitleWide )
				{
					// shift all the children down one tile to account for dialog frame heading
					y += m_nTileHeight;
				}
			}

			// set the widths for any controls that need to be auto-set
			if ( !wide )
			{
				wide = GetWide();
			}

			pChild->SetBounds( x, y, wide, tall );
		}

		m_bLayoutFixed = true;
	}
}

// Load the control settings 
void CBaseModFrame::LoadControlSettings( const char *dialogResourceName, const char *pathID, KeyValues *pPreloadedKeyValues, KeyValues *pConditions )
{
	// Active control can be stale after settings reload
	m_ActiveControl = NULL;

	// Use the keyvalues they passed in or load them using special hook for flyouts generation
	KeyValues *rDat = pPreloadedKeyValues;
	if ( !rDat )
	{
		// load the resource data from the file
		rDat  = new KeyValues(dialogResourceName);

		// check the skins directory first, if an explicit pathID hasn't been set
		bool bSuccess = false;
		if ( !IsGameConsole() && !pathID )
		{
			bSuccess = rDat->LoadFromFile( g_pFullFileSystem, dialogResourceName, "SKIN" );
		}
		if ( !bSuccess )
		{
			bSuccess = rDat->LoadFromFile( g_pFullFileSystem, dialogResourceName, pathID );
		}
		if ( bSuccess )
		{
			if ( pConditions && pConditions->GetFirstSubKey() )
			{
				GetBuildGroup()->ProcessConditionalKeys( rDat, pConditions );
			}
		}
	}

	// Find the auto-generated-chapter hook
	if ( KeyValues *pHook = rDat->FindKey( "FlmChapterXXautogenerated" ) )
	{
		const int numMaxAutogeneratedFlyouts = 20;
		for ( int k = 1; k <= numMaxAutogeneratedFlyouts; ++ k )
		{
			KeyValues *pFlyoutInfo = pHook->MakeCopy();
			
			CFmtStr strName( "FlmChapter%d", k );
			pFlyoutInfo->SetName( strName );
			pFlyoutInfo->SetString( "fieldName", strName );
			
			pFlyoutInfo->SetString( "ResourceFile", CFmtStr( "FlmChapterXXautogenerated_%d/%s", k, pHook->GetString( "ResourceFile" ) ) );

			rDat->AddSubKey( pFlyoutInfo );
		}

		rDat->RemoveSubKey( pHook );
		pHook->deleteThis();
	}

	BaseClass::LoadControlSettings( dialogResourceName, pathID, rDat, pConditions );
	if ( rDat != pPreloadedKeyValues )
	{
		rDat->deleteThis();
	}

	// NOT shippng VUI in Portal2, causes needless i/o
#if 0
	// Load Virtual UI settings
	if ( m_pVuiSettings )
	{
		m_pVuiSettings->deleteThis();
		m_pVuiSettings = NULL;
	}

	if ( dialogResourceName )
	{
		int nLen = Q_strlen( dialogResourceName );
		if ( nLen > 4 )
		{
			char *chBuf = ( char * ) stackalloc( nLen + 1 );
			Q_strncpy( chBuf, dialogResourceName, nLen + 1 );
			Q_snprintf( chBuf + nLen - 4, 5, ".vui" );
			m_pVuiSettings = new KeyValues( "" );
			if ( !m_pVuiSettings->LoadFromFile( g_pFullFileSystem, chBuf, pathID ) )
			{
				m_pVuiSettings->deleteThis();
				m_pVuiSettings = NULL;
			}

			CreateVirtualUiControls();
		}
	}
#endif
}

//=============================================================================
void CBaseModFrame::ApplySettings( KeyValues *pInResourceData )
{
	// store off the original value
	// dialogs encode these as tile units
	if ( !m_nTilesWide )
	{
		m_nTilesWide = pInResourceData->GetInt( "wide", 0 );
	}

	if ( !m_nTilesTall )
	{
		m_nTilesTall = pInResourceData->GetInt( "tall", 0 );
	}

	BaseClass::ApplySettings( pInResourceData );

	m_nDialogStyle = pInResourceData->GetInt( "dialogstyle", 0 );

	PostApplySettings();
	DisableFadeEffect();
}

//=============================================================================
void CBaseModFrame::PostApplySettings()
{
}

//=============================================================================
void CBaseModFrame::SetOkButtonEnabled(bool enabled)
{
	m_OkButtonEnabled = enabled;
	BaseModUI::CBaseModPanel::GetSingleton().SetOkButtonEnabled( enabled );
}

//=============================================================================
void CBaseModFrame::SetCancelButtonEnabled(bool enabled)
{
	m_CancelButtonEnabled = enabled;
	BaseModUI::CBaseModPanel::GetSingleton().SetCancelButtonEnabled( enabled );
}

void CBaseModFrame::SetUpperGarnishEnabled( bool bEnabled )
{
}

//=============================================================================
void CBaseModFrame::SetFooterEnabled(bool enabled)
{
	m_FooterEnabled = enabled;	
}

//=============================================================================
void CBaseModFrame::ToggleTitleSafeBorder()
{
	m_DrawTitleSafeBorder = !m_DrawTitleSafeBorder;
	SetPostChildPaintEnabled(true);
}


//=============================================================================
// Takes modal app input focus so all keyboard and mouse input only goes to 
// this panel and its children.  Prevents the problem of clicking outside a panel
// and activating some other panel that is underneath.  PC only since it's a mouse
// issue.  Maintains a stack of previous panels that had focus.
void CBaseModFrame::PushModalInputFocus()
{
return;
	if ( IsPC() )
	{
		if ( !m_bLayoutLoaded )
		{
			// if we haven't been loaded yet, we've just been created and will load our layout shortly,
			// delay this action until then
			m_bDelayPushModalInputFocus = true;
		}
		else if ( !m_bIsFullScreen )
		{
			// only need to do this if NOT full screen -- if we are full screen then you can't click outside
			// the topmost window.

			// maintain a stack of who had modal input focus
			HPanel handle = vgui::ivgui()->PanelToHandle( vgui::input()->GetAppModalSurface() );
			if ( handle )
			{
				m_vecModalInputFocusStack.AddToHead( handle );
			}

			// set ourselves modal so you can't mouse click outside this panel to a child panel
			vgui::input()->SetAppModalSurface( GetVPanel() );		
		}
	}
}


//=============================================================================
void CBaseModFrame::PopModalInputFocus()
{
return;
	// only need to do this if NOT full screen -- if we are full screen then you can't click outside
	// the topmost window.
	if ( IsPC() && !m_bIsFullScreen )
	{
		// In general we should have modal input focus since we should only pop modal input focus
		// if we are top most.  However, if all windows get closed at once we can get closed out of 
		// order so we may not actually have modal input focus.  Similarly, we should in general
		// NOT appear on the modal input focus stack but we may if we get closed out of order,
		// so remove ourselves from the stack if we're on it.
		bool bHaveModalInputFocus = ( vgui::input()->GetAppModalSurface() == GetVPanel() );

		HPanel handle = ivgui()->PanelToHandle( GetVPanel() );
		m_vecModalInputFocusStack.FindAndRemove( handle );

		if ( bHaveModalInputFocus )
		{
			// if we have modal input focus, figure out who is supposed to get it next

			// start from the top of the stack
			while ( m_vecModalInputFocusStack.Count() > 0 )
			{
				handle = m_vecModalInputFocusStack[0];

				// remove top stack entry no matter what
				m_vecModalInputFocusStack.Remove( 0 );

				VPANEL panel = ivgui()->HandleToPanel( handle );

				if ( !panel )
					continue;

				// if the panel on the modal input focus stack isn't visible any more, ignore it and go on
				if ( ipanel()->IsVisible( panel ) == false )
					continue;
				
				// found the top panel on the stack, give it the modal input focus
				vgui::input()->SetAppModalSurface( panel );
				break;
			}

			// there are no valid panels on the stack to receive modal input focus after us, so set it to NULL
			if ( m_vecModalInputFocusStack.Count() == 0 )
			{
				vgui::input()->SetAppModalSurface( NULL );
			}			
		}
	}
}

//=============================================================================
// If on PC and not logged into Steam, displays an error and returns true.  
// Returns false if everything is OK.
bool CBaseModFrame::CheckAndDisplayErrorIfNotLoggedIn()
{
	// only check if PC
	if ( !IsPC() )
		return false;

#ifndef NO_STEAM
#ifndef SWDS
	// if we have Steam interfaces and user is logged on, everything is OK
	if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamMatchmaking() )
	{
		/*
		// Try starting to log on
		if ( !steamapicontext->SteamUser()->BLoggedOn() )
		{
		steamapicontext->SteamUser()->LogOn();
		}
		*/

		return false;
	}
	
#endif
#endif

	// Steam is not running or user not logged on, display error

	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, CBaseModPanel::GetSingleton().GetWindow( WT_GAMELOBBY ), false ) );
	GenericConfirmation::Data_t data;
	data.pWindowTitle = "#L4D360UI_MsgBx_LoginRequired";
	data.pMessageText = "#L4D360UI_MsgBx_SteamRequired";
#if defined( _PS3 ) && !defined( NO_STEAM )
	if ( steamapicontext && steamapicontext->SteamUtils() && !steamapicontext->SteamUtils()->BIsPSNOnline() )
		data.pMessageText = "#L4D360UI_MsgBx_PSNRequired";
#endif
	if ( !CUIGameData::Get()->IsNetworkCableConnected() )
		data.pMessageText = "#L4D360UI_MsgBx_EthernetCableNotConnected";
	data.bOkButtonEnabled = true;
	confirmation->SetUsageData(data);

	return true;
}

//=============================================================================
void CBaseModFrame::SetWindowType(WINDOW_TYPE windowType)
{
	m_WindowType = windowType;
}

//=============================================================================
WINDOW_TYPE CBaseModFrame::GetWindowType()
{
	return m_WindowType;
}

//=============================================================================
void CBaseModFrame::SetWindowPriority( WINDOW_PRIORITY pri )
{
	m_WindowPriority = pri;
}

//=============================================================================
WINDOW_PRIORITY CBaseModFrame::GetWindowPriority()
{
	return m_WindowPriority;
}

//=============================================================================
void CBaseModFrame::SetCanBeActiveWindowType(bool allowed)
{
	m_CanBeActiveWindowType = allowed;
}

//=============================================================================
bool CBaseModFrame::GetCanBeActiveWindowType()
{
	return m_CanBeActiveWindowType;
}

//=============================================================================
CBaseModFrame* CBaseModFrame::SetNavBack( CBaseModFrame* navBack )
{
	CBaseModFrame* lastNav = m_NavBack.Get();
	m_NavBack = navBack;
	m_bCanNavBack = navBack != NULL;

	return lastNav;
}

void CBaseModFrame::DrawGenericBackground()
{
	int wide, tall;
	GetSize( wide, tall );
	DrawHollowBox( 0, 0, wide, tall, Color( 150, 150, 150, 255 ), 1.0f );
	DrawBox( 2, 2, wide-4, tall-4, Color( 48, 48, 48, 255 ), 1.0f );
}

void CBaseModFrame::DrawDialogBackground( const char *pMajor, const wchar_t *pMajorFormatted, const char *pMinor, const wchar_t *pMinorFormatted,
									  DialogMetrics_t *pMetrics /*= NULL*/, bool bAllCapsTitle /*= false*/, int iTitleXOffset /*= INT_MAX*/ )
{
	int wide;
	int tall;
	GetSize( wide, tall );

	int xpos, ypos;
	GetPos( xpos, ypos );

	// Setup our random seed to ensure proper drawing
	m_RandomStream.SetSeed( m_nAltSeed );

	surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
	for ( int y = 0; y < tall; y += m_nTileHeight )
	{
		for ( int x = 0; x < wide; x += m_nTileWidth )
		{
			if ( !y && m_nTitleWide && x >= m_nTitleWide )
				break;

			// select the tile texture
			int nTexture;
			if ( !y )
			{
				if ( !x )
					nTexture = MT_TOP_LEFT;
				else if ( x + m_nTileWidth < wide && !m_nTitleWide )
					nTexture = MT_TOP;
				else if ( x + m_nTileWidth < wide && x + m_nTileWidth < m_nTitleWide )
					nTexture = MT_TOP;
				else
					nTexture = MT_TOP_RIGHT;
			}
			else if ( y < tall - m_nTileHeight )
			{
				if ( !x )
					nTexture = MT_LEFT;
				else if ( x + m_nTileWidth < wide )
				{
					if ( y == m_nTileHeight && m_nTitleWide && x >= m_nTitleWide )
						nTexture = MT_TOP;
					else if ( y == m_nTileHeight && m_nTitleWide && x + m_nTileWidth >= m_nTitleWide )
						nTexture = MT_INTERIOR_ALT;
					else
						nTexture = MT_INTERIOR;
				}
				else
				{
					if ( y == m_nTileHeight && m_nTitleWide && x > m_nTitleWide )
						nTexture = MT_TOP_RIGHT;
					else
						nTexture = MT_RIGHT;
				}
			}
			else
			{
				if ( !x )
					nTexture = MT_BOTTOM_LEFT;
				else if ( x + m_nTileWidth < wide )
					nTexture = MT_BOTTOM;
				else
					nTexture = MT_BOTTOM_RIGHT;
			}

			CBaseModPanel::GetSingleton().GetTransitionEffectPanel()->MarkTile( xpos + x, ypos + y, m_WindowType );

			// Use an alternate background tile for certain community menu types
			if ( m_bUseAlternateTiles )
			{
				int nRandomAlternate = -1;

				// If this is an interior tile, we can randomly choose two other types
				if ( nTexture == MT_INTERIOR )
				{
					if ( m_RandomStream.RandomInt( 0, 100 ) < 10 )
					{
						nRandomAlternate = m_RandomStream.RandomInt( 0, 1 );
					}
				}

				if ( nRandomAlternate != -1 )
				{
					surface()->DrawSetTexture( m_nAltRandomTileImageId[nRandomAlternate] );
				}
				else
				{
					surface()->DrawSetTexture( m_nAltTileImageId[nTexture] );
				}
			}
			else
			{
				surface()->DrawSetTexture( m_nTileImageId[nTexture] );
			}

			surface()->DrawTexturedSubRect( x, y, x + m_nTileWidth, y + m_nTileHeight, 0, 0, 1, 1 );
		}
	}

	if ( m_TitleString[0] )
	{
		// draw major title in header band
		vgui::surface()->DrawSetTextFont( m_hTitleFont );
		int nTitleYOffset = m_nTitleOffsetY;
		// if drawing a subtitle, move the title up a bit to separate the two
		if ( m_SubTitleString[0] )
		{
			nTitleYOffset = m_nTitleOffsetY * 0.7f;
		}

		vgui::surface()->DrawSetTextPos( m_nTitleOffsetX, nTitleYOffset );

		Color titleColor = ( m_bUseAlternateTiles ) ? m_TitleColorAlt : m_TitleColor;
		
		if ( m_nDialogStyle == DS_CONFIRMATION )
		{
			titleColor = ( m_bUseAlternateTiles ) ? m_MessageBoxTitleColorAlt : m_MessageBoxTitleColor;
		}

		vgui::surface()->DrawSetTextColor( titleColor );

		vgui::surface()->DrawPrintText( m_TitleString, V_wcslen( m_TitleString ) );
	}

	if ( m_SubTitleString[0] )
	{
		// draw sub title in header band
		vgui::surface()->DrawSetTextFont( m_hSubTitleFont );
		vgui::surface()->DrawSetTextPos( m_nTitleOffsetX, (m_nTitleOffsetY * 0.25f) + m_nTitleTall );

		Color titleColor = ( m_bUseAlternateTiles ) ? m_SubTitleColorAlt : m_SubTitleColor;
		if ( m_nDialogStyle == DS_CONFIRMATION )
		{
			titleColor = ( m_bUseAlternateTiles ) ? m_MessageBoxTitleColorAlt : m_MessageBoxTitleColor;
		}
		vgui::surface()->DrawSetTextColor( titleColor );

		vgui::surface()->DrawPrintText( m_SubTitleString, V_wcslen( m_SubTitleString ) );
	}

	DrawControllerIndicator();
}

void CBaseModFrame::DrawControllerIndicator()
{
	if ( !m_bShowController )
		return;

	int iActiveUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( iActiveUserSlot < 0 )
		return;

	iActiveUserSlot = XBX_GetUserId( iActiveUserSlot );
	if ( iActiveUserSlot < 0 || iActiveUserSlot >= XUSER_MAX_COUNT )
		return;

	wchar_t *pButtonString = g_pVGuiLocalize->Find( "#GameUI_Icons_XboxButton" );
	wchar_t *pHighlightButtonString = IsPS3() ? pButtonString : g_pVGuiLocalize->Find( CFmtStr( "#GameUI_Icons_XboxButton%d", iActiveUserSlot + 1 ) );

	int nEntireTitleEndW = m_nTitleWide;
#ifdef _PS3
	{
		wchar_t chControllerIndex[2] = {0};
		chControllerIndex[0] = L'1' + iActiveUserSlot;
		chControllerIndex[1] = 0;
		int iCtrlrIdxW, iCtrlrIdxH;
		vgui::surface()->GetTextSize( m_hTitleFont, chControllerIndex, iCtrlrIdxW, iCtrlrIdxH );

		vgui::surface()->DrawSetTextPos( nEntireTitleEndW - iCtrlrIdxW, m_nTitleOffsetY );
		vgui::surface()->DrawPrintText( chControllerIndex, 1 );
		nEntireTitleEndW -= iCtrlrIdxW;
	}
#endif

	if ( pButtonString && pHighlightButtonString )
	{
		int buttonWide, buttonTall;
		vgui::surface()->GetTextSize( m_hButtonFont, pButtonString, buttonWide, buttonTall );

		vgui::surface()->DrawSetTextFont( m_hButtonFont );
		vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );

		vgui::surface()->DrawSetTextPos( nEntireTitleEndW - buttonWide, ( m_nTileHeight - buttonTall ) / 2 );
		vgui::surface()->DrawPrintText( pButtonString, V_wcslen( pButtonString ) );

		if ( !IsPS3() )
		{
			vgui::surface()->DrawSetTextPos( nEntireTitleEndW - buttonWide, ( m_nTileHeight - buttonTall ) / 2 );
			vgui::surface()->DrawPrintText( pHighlightButtonString, V_wcslen( pHighlightButtonString ) );
		}
	}
}

void CBaseModFrame::SetDialogTitle( const char *pMajor, const wchar_t *pMajorFormatted, bool bShowController, int nTilesWide, int nTilesTall, int nTitleTilesWide )
{
	// resolve the major title
	if ( pMajor )
	{
		wchar_t *pMajorString = g_pVGuiLocalize->Find( pMajor );
		if ( !pMajorString )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pMajor, m_TitleString, sizeof( m_TitleString ) );
		}
		else
		{
			Q_wcsncpy( m_TitleString, pMajorString, sizeof( m_TitleString ) );
		}

		int nTitleLen = Q_wcslen( m_TitleString );

		char uilanguage[64];
		engine->GetUILanguage( uilanguage, sizeof( uilanguage ) );
		bool bIsEnglish = ( uilanguage[0] == 0 ) || !V_stricmp( uilanguage, "english" );
		if ( bIsEnglish )
		{
			if ( m_nDialogStyle == DS_CONFIRMATION )
			{
				// confirmation dialogs have a camel case title for english only
				bool bCaps = true;
				for ( int i = 0; i < nTitleLen; i++ )
				{
					if ( iswspace( m_TitleString[ i ] ) )
					{
						bCaps = true;
					}
					else if ( iswalpha( m_TitleString[ i ] ) )
					{
						if ( bCaps )
						{
							m_TitleString[ i ] = towupper( m_TitleString[ i ] );
							bCaps = false;
						}
						else
						{
							m_TitleString[ i ] = towlower( m_TitleString[ i ] );
						}
					}
				}
			}
			else
			{
				for ( int i = 0; i < nTitleLen; i++ )
				{
					m_TitleString[ i ] = towupper( m_TitleString[ i ] );
				}
			}
		}
	}
	else if ( pMajorFormatted )
	{
		// already resolved
		Q_wcsncpy( m_TitleString, pMajorFormatted, sizeof( m_TitleString ) );
	}

	if ( IsGameConsole() && m_TitleString[0] && ( ( XBX_GetNumGameUsers() > 1 ) || ui_gameui_ctrlr_title.GetBool() ) )
	{
		m_bShowController = bShowController;
	}

	if ( nTilesWide )
	{
		m_nTilesWide = nTilesWide;
	}

	if ( nTilesTall )
	{
		m_nTilesTall = nTilesTall;
	}

	if ( nTitleTilesWide )
	{
		m_nTitleTilesWide = nTitleTilesWide;
	}
}

void CBaseModFrame::GetDialogTileSize( int &nTileWidth, int &nTileHeight )
{
	nTileWidth = m_nTileWidth;
	nTileHeight = m_nTileHeight;
}

void CBaseModFrame::SetupAsDialogStyle()
{
	m_bLayoutFixed = false;

	// it is critical that m_nTitleTall gets set, even for an empty string
	vgui::surface()->GetTextSize( m_hTitleFont, m_TitleString, m_nTitleWide, m_nTitleTall );
	vgui::surface()->GetTextSize( m_hSubTitleFont, m_SubTitleString, m_nSubTitleWide, m_nSubTitleTall );

	// the title can be trumped by a caller who needs a specific number of title tiles
	m_nTitleWide = MAX( m_nTitleWide, m_nSubTitleWide );
	m_nTitleWide = MAX( m_nTitleWide, m_nTitleTilesWide * m_nTileWidth );

	if ( m_nTitleWide && m_TitleString[0] )
	{
		// text title has gap on both sides
		m_nTitleWide += 2 * m_nTitleOffsetX;
	}

	if ( m_bShowController )
	{
		wchar_t *pButtonString = g_pVGuiLocalize->Find( "#GameUI_Icons_XboxButton" );
		if ( pButtonString )
		{
			int buttonWide, buttonTall;
			vgui::surface()->GetTextSize( m_hButtonFont, pButtonString, buttonWide, buttonTall );
			m_nTitleWide += buttonWide + m_nTitleOffsetX;
		}

#ifdef _PS3
		{
			int iActiveUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
			int iCtrlr = XBX_GetUserId( iActiveUserSlot );
			wchar_t chControllerIndex[2] = {0};
			chControllerIndex[0] = L'1' + iCtrlr;
			chControllerIndex[1] = 0;
			int iCtrlrIdxW, iCtrlrIdxH;
			vgui::surface()->GetTextSize( m_hTitleFont, chControllerIndex, iCtrlrIdxW, iCtrlrIdxH );
			m_nTitleWide += iCtrlrIdxW;
		}
#endif
	}

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	// bounds are in tile units
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	m_nOriginalWide = m_nTilesWide * m_nTileWidth;
	m_nOriginalTall = m_nTilesTall * m_nTileHeight;

	// want a notched look to the dialog
	// ideally, the title is shorter than the dialog
	// width is based on at least title + 1
	int nDialogWide = ( m_nTitleWide / m_nTileWidth ) * m_nTileWidth;
	if ( nDialogWide && nDialogWide < m_nTitleWide )
	{
		// add an extra tile to the title
		nDialogWide += m_nTileWidth;
	}

	if ( nDialogWide < m_nOriginalWide )
	{
		// the title is shorter, respect the original dimension
		nDialogWide = m_nOriginalWide;
	}
	else if ( m_nDialogStyle != DS_CUSTOMTITLE )
	{
		// longer dialog title than expected
		// the dialog needs to be 1 tile wider than the title
		nDialogWide += m_nTileWidth;
	}

	int nDialogTall = m_nOriginalTall;
	if ( m_nTitleWide )
	{
		// account for frame title
		nDialogTall += m_nTileHeight;
	}

	x = m_nPinFromLeft;
	y = screenTall - m_nPinFromBottom - nDialogTall;
	SetBounds( x, y, nDialogWide, nDialogTall );

	CBaseModFooterPanel *pFooter = CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		int fx = x;
		int	fy = y + nDialogTall + m_nFooterOffsetY;

		WINDOW_PRIORITY wp = GetWindowPriority();
		FooterType_t footerType = FOOTER_MENUS;
		if ( wp == WPRI_MESSAGE )
			footerType = FOOTER_GENERICCONFIRMATION;
		else if ( wp == WPRI_WAITSCREEN )
			footerType = FOOTER_GENERICWAITSCREEN;

		pFooter->SetPosition( fx, fy, footerType );
	}
}

abstract_class IControlNavLinker
{
public:
	virtual ~IControlNavLinker() {}
	virtual void LinkControls( BaseModHybridButton *b1, BaseModHybridButton *b2 ) = 0;
};

class NavLinkLeftRight : public IControlNavLinker
{
	virtual void LinkControls( BaseModHybridButton *b1, BaseModHybridButton *b2 )
	{
		b1->SetNavRight( b2 );
		b2->SetNavLeft( b1 );
	}
};

class NavLinkUpDown : public IControlNavLinker
{
	virtual void LinkControls( BaseModHybridButton *b1, BaseModHybridButton *b2 )
	{
		b1->SetNavDown( b2 );
		b2->SetNavUp( b1 );
	}
};

void CBaseModFrame::CreateVirtualUiControls()
{
	if ( !IsGameConsole() )
		return;

	// Null active control before deleting
	if ( m_arrVirtualUiControls.Find( ( BaseModHybridButton * ) m_ActiveControl ) != m_arrVirtualUiControls.InvalidIndex() )
		m_ActiveControl = NULL;

	// Delete old controls
	m_arrVirtualUiControls.PurgeAndDeleteElements();

	// Iterate over all controls and create virtual UI controls
	for ( KeyValues *kvCtrl = m_pVuiSettings->FindKey( "controls" )->GetFirstTrueSubKey(); kvCtrl; kvCtrl = kvCtrl->GetNextTrueSubKey() )
	{
		char const *szToken = kvCtrl->GetName();
		BaseModHybridButton *pHybridButton = new BaseModHybridButton( this, kvCtrl->GetString( "name", szToken ), szToken, this, CFmtStr( VIRTUAL_UI_COMMAND_PREFIX "%s", szToken ) );
		pHybridButton->SetPos( -1000, -1000 ); // offscreen
		m_arrVirtualUiControls.AddToTail( pHybridButton );
		kvCtrl->SetPtr( "btn", pHybridButton );
	}

	// Configure navigation
	for ( KeyValues *kvNav = m_pVuiSettings->FindKey( "navigation" )->GetFirstSubKey(); kvNav; kvNav = kvNav->GetNextKey() )
	{
		char const *szNavType = kvNav->GetName();
		IControlNavLinker *pNavLinker = NULL;
		if ( !Q_stricmp( "leftright", szNavType ) )
		{
			pNavLinker = new NavLinkLeftRight;
		}
		else if ( !Q_stricmp( "updown", szNavType ) )
		{
			pNavLinker = new NavLinkUpDown;
		}

		if ( pNavLinker )
		{
			KeyValues *kvPrev = NULL, *kvFirst = NULL;
			for ( KeyValues *kvBtn = kvNav->GetFirstSubKey(); kvBtn; kvBtn = kvBtn->GetNextKey() )
			{
				KeyValues *kvControl = m_pVuiSettings->FindKey( "controls" )->FindKey( kvBtn->GetName() );
				if ( !kvControl )
					continue;

				if ( kvPrev )
				{
					pNavLinker->LinkControls( ( ( BaseModHybridButton * )( kvPrev->GetPtr( "btn" ) ) ), ( ( BaseModHybridButton * )( kvControl->GetPtr( "btn" ) ) ) );
				}
				else
				{
					kvFirst = kvControl;
				}

				kvPrev = kvControl;
			}

			if ( kvNav->GetInt( "*loop" ) && kvFirst && kvPrev )
			{
				pNavLinker->LinkControls( ( ( BaseModHybridButton * )( kvPrev->GetPtr( "btn" ) ) ), ( ( BaseModHybridButton * )( kvFirst->GetPtr( "btn" ) ) ) );
			}

			delete pNavLinker;
		}
	}

	// Navigate to default entity
	if ( char const *szDefaultEntity = m_pVuiSettings->GetString( "default/entity", NULL ) )
	{
		char chEntityName[256];
		ResolveEntityName( szDefaultEntity, chEntityName );
		g_BackgroundMapActiveControlManager.NavigateToEntity( chEntityName );
	}

	// Set default control
	if ( char const *szDefaultControl = m_pVuiSettings->GetString( "default/control", NULL ) )
	{
		if ( BaseModHybridButton *btn = ( BaseModHybridButton * ) m_pVuiSettings->GetPtr( CFmtStr( "controls/%s/btn", szDefaultControl ) ) )
		{
			if ( m_ActiveControl )
				m_ActiveControl->NavigateFrom();
			btn->NavigateTo();
		}
	}
}

char const * CBaseModFrame::GetEntityOverMouseCursorInEngine()
{
	if( !engine->IsLocalPlayerResolvable() )
	{
		return "";
	}

	int curX, curY;
	vgui::input()->GetCursorPosition( curX, curY );

	int scrX, scrY;
	vgui::surface()->GetScreenSize( scrX, scrY );

	CViewSetup vs;
	clientdll->GetPlayerView( vs );

	// Half-width of the screen in world units:
	// ww_half = zNear * tan( fov / 2 )
	float flWwHalf = vs.zNear * tan( vs.fov * M_PI / 360.0f );
	float flScreenToWwCoeff = flWwHalf * 2.0f / MAX( scrX, 1 );
	float flWwX = (curX - scrX/2) * flScreenToWwCoeff;
	float flWwY = (scrY/2 - curY) * flScreenToWwCoeff;

	// Find the view basis
	C_BaseEntity *pHitEnt = NULL;
	if ( C_BasePlayer *pBasePlayer = C_BasePlayer::GetLocalPlayer( 0 ) )
	{
		C_BaseEntity *pEntOrg = pBasePlayer->GetViewEntity();
		if ( !pEntOrg )
			pEntOrg = pBasePlayer;

		Vector org = pEntOrg->GetAbsOrigin();
		Vector basis[3] = { -pEntOrg->Left(), pEntOrg->Up(), pEntOrg->Forward() };

		Vector vecTraceDirection = vs.zNear * basis[2] + flWwX * basis[0] + flWwY * basis[1];
		Vector ptOnNearPlane = org + vecTraceDirection;

		trace_t tr;
		UTIL_TraceLine( org + vecTraceDirection, org + 10000.f * vecTraceDirection,
			ALL_VISIBLE_CONTENTS | CONTENTS_TRANSLUCENT, NULL, &tr );
		if ( !tr.startsolid && tr.DidHit() && tr.m_pEnt )
		{
			pHitEnt = tr.m_pEnt;
		}
	}

	// Upload the hittest information to the server
	// static int s_iServerTick = -1;
	// if ( gpGlobals->tickcount != s_iServerTick )
	// {
	// 	s_iServerTick = gpGlobals->tickcount;
	// }

	static char s_chHitResult[128] = {0};
	char const *szHitName = pHitEnt ? pHitEnt->GetEntityName() : "";
	Q_strncpy( s_chHitResult, szHitName, sizeof( s_chHitResult ) );
	return s_chHitResult;
}


//
// Background map active control manager
//

CBackgroundMapActiveControlManager g_BackgroundMapActiveControlManager;

CBackgroundMapActiveControlManager::CBackgroundMapActiveControlManager()
{
	Reset();
}

void CBackgroundMapActiveControlManager::Reset()
{
	memset( m_chLastActiveEntity, 0, sizeof( m_chLastActiveEntity ) );
}

void CBackgroundMapActiveControlManager::Apply()
{
	char chReapplyEntity[ sizeof( m_chLastActiveEntity ) ];
	memcpy( chReapplyEntity, m_chLastActiveEntity, sizeof( m_chLastActiveEntity ) );
	m_chLastActiveEntity[0] = 0;
	NavigateToEntity( chReapplyEntity );
}

void CBackgroundMapActiveControlManager::NavigateToEntity( const char *szEntity )
{
	if ( Q_stricmp( szEntity, m_chLastActiveEntity ) )
	{
		if ( m_chLastActiveEntity[0] )
			engine->ClientCmd( CFmtStr( "ent_fire %s fireuser2", m_chLastActiveEntity ) );
		Q_strncpy( m_chLastActiveEntity, szEntity, sizeof( m_chLastActiveEntity ) );
		if ( m_chLastActiveEntity[0] )
			engine->ClientCmd( CFmtStr( "ent_fire %s fireuser1", m_chLastActiveEntity ) );
	}
}


void BaseModUI::CBaseModFrame::SetDialogSubTitle( const char *pMajor, const wchar_t *pMajorFormatted, bool bShowController, int nTilesWide, int nTilesTall, int nTitleTilesWide )
{
	// resolve the major title
	if ( pMajor )
	{
		wchar_t *pMajorString = g_pVGuiLocalize->Find( pMajor );
		if ( !pMajorString )
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pMajor, m_SubTitleString, sizeof( m_SubTitleString ) );
		}
		else
		{
			Q_wcsncpy( m_SubTitleString, pMajorString, sizeof( m_SubTitleString ) );
		}

		int nTitleLen = Q_wcslen( m_SubTitleString );

		char uilanguage[64];
		engine->GetUILanguage( uilanguage, sizeof( uilanguage ) );
		bool bIsEnglish = ( uilanguage[0] == 0 ) || !V_stricmp( uilanguage, "english" );
		if ( bIsEnglish )
		{
			if ( m_nDialogStyle == DS_CONFIRMATION )
			{
				// confirmation dialogs have a camel case title for english only
				bool bCaps = true;
				for ( int i = 0; i < nTitleLen; i++ )
				{
					if ( iswspace( m_SubTitleString[ i ] ) )
					{
						bCaps = true;
					}
					else if ( iswalpha( m_SubTitleString[ i ] ) )
					{
						if ( bCaps )
						{
							m_SubTitleString[ i ] = towupper( m_SubTitleString[ i ] );
							bCaps = false;
						}
						else
						{
							m_SubTitleString[ i ] = towlower( m_SubTitleString[ i ] );
						}
					}
				}
			}
			else
			{
				for ( int i = 0; i < nTitleLen; i++ )
				{
					m_SubTitleString[ i ] = towupper( m_SubTitleString[ i ] );
				}
			}
		}
	}
	else if ( pMajorFormatted )
	{
		// already resolved
		Q_wcsncpy( m_SubTitleString, pMajorFormatted, sizeof( m_SubTitleString ) );
	}

	if ( IsGameConsole() && m_SubTitleString[0] && ( ( XBX_GetNumGameUsers() > 1 ) || ui_gameui_ctrlr_title.GetBool() ) )
	{
		m_bShowController = bShowController;
	}

	if ( nTilesWide )
	{
		m_nTilesWide = nTilesWide;
	}

	if ( nTilesTall )
	{
		m_nTilesTall = nTilesTall;
	}

	if ( nTitleTilesWide )
	{
		m_nTitleTilesWide = nTitleTilesWide;
	}
}
